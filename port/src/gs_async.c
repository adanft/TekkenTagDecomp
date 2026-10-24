/*
 * The GS path on a thread of its own (T30 batch 3, TTT_GS_ASYNC): VIF1 (UNPACK, MPG, the VU1 programs), the GIF and
 * the GS front end run beside the game instead of inside the poll that carries the DMA transfer out.
 *
 * The DMA channels (plat_hw.c) still carry transfers out at the same polls, in the same order, and finish them at once
 * as before; only the data they hand VIF1 and the GIF (PATH3) is copied into a queue, together with each VBlank's
 * display registers, and the GS thread carries the queue out in order: the same VIF codes, VU1 programs, GIF packets,
 * primitives and pictures as when the game's thread ran them, so GS memory, the dumps and the window's pictures are the
 * same. What the game sees does not change either, because nothing it reads comes from that work:
 *   - VU1 memory (0x1100C000) and micro memory are only written by the game in Gfx_Init, right after sceGsResetGraph,
 *     which waits for the queue to empty (Gs_Reset / Gs_Vif1Reset drain it first);
 *   - the VIF1 registers are not modelled (the VIF1 status the game would read is the DMA channel's, plat_hw.c);
 *   - GS CSR: the game reads its FIELD bit only (gfx.c, movie.c); the SIGNAL / FINISH / LABEL a GIF packet sets are
 *     kept here and written into CSR / SIGLBLID by the game's thread at the next VBlank and at every drain (the GS
 *     thread never writes the registers the game's VBlank code changes);
 *   - the queue holds copies: the game may reuse its packet buffers and the MFIFO ring as soon as the DMA finished.
 * The end-of-run report, the GS reset and the self-tests wait for the queue to empty (Gs_AsyncDrain).
 *
 * On by default when the GS has raster threads or the GL back end; TTT_GS_ASYNC=0 turns it off, the GS traces and the
 * self-tests do too (they print from the GS path in order with the game's output).
 *
 * Two GS threads (T30 batch 4, TTT_GS_ASYNC=2, the default with the GL back end): the GS thread runs VIF1 and the VU1
 * programs and hands the GIF data of PATH1 (XGKICK, written in place), PATH2 (DIRECT) and PATH3 and the VBlanks, in
 * order, to the GIF thread through a second queue; the GIF thread runs the GIF and the GS front end. Nothing in VIF1 or
 * VU1 reads the GIF's or the GS's state, so the order of everything the GS sees is unchanged. A drain waits for both
 * queues (the GS thread queues a record's GIF data before it moves its own queue's tail).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "port_host.h"
#include "port_addr.h"
#include "gs.h"
#include "gs_int.h"

#define QSIZE (32u << 20) /* bytes per queue; a power of two */
enum { REC_PAD, REC_VIF1, REC_GIF3, REC_VBLANK, REC_GIF };

typedef struct {
    uint32_t kind, n; /* n: words (VIF1), quadwords (GIF3, GIF), the VBlank's number */
    uint32_t bytes;   /* the record's size, a multiple of 16 */
    uint32_t path;    /* REC_GIF: the GIF path */
} Rec;

typedef struct {
    uint32_t v;
    uint8_t pad[60];
} __attribute__((aligned(64))) Counter;

/* A queue of records from one thread to another: bytes written (head) and carried out (tail) on cache lines of their
   own; `reader` / `writer`: the sSleeping bits of the threads that wait on it. */
typedef struct {
    uint8_t *q;
    Counter head, tail;
    uint32_t need; /* the writer's: the room it waits for */
    uint32_t reader, writer;
} Queue;

static int sOn = -1;      /* -1: not decided yet */
static int sSplit;        /* 1: VIF1 / VU1 and the GIF / GS front end on two threads */
static Queue sQ1 = {NULL, {0, {0}}, {0, {0}}, 0, 1, 2}; /* the game's thread -> the GS thread (VIF1, VU1) */
static Queue sQ2 = {NULL, {0, {0}}, {0, {0}}, 0, 4, 8}; /* the GS thread -> the GIF thread (GIF, GS front end) */
/* the threads waiting: 1 the GS thread for records, 2 the game's thread for room or the end, 4 the GIF thread for
   records, 8 the GS thread for room */
static uint32_t sSleeping;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sCond = PTHREAD_COND_INITIALIZER;
/* GS CSR / SIGLBLID effects of the GIF packets, written into the registers by the game's thread */
static pthread_mutex_t sRegLock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t sCsrBits, sSigLblId;
static int sSigLblIdSet;
static uint64_t sWaitNs;
static uint32_t sCpuMs[2]; /* the GS / GIF threads' CPU time in ms as of their last wait (TTT_GS_STATS); 32 bits: 64-bit
                              atomics would call libatomic through new PLT entries (gotcha 20) */

static uint32_t ld(const uint32_t *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }

static void wake_all(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (__atomic_load_n(&sSleeping, __ATOMIC_SEQ_CST) != 0) {
        pthread_mutex_lock(&sLock);
        pthread_cond_broadcast(&sCond);
        pthread_mutex_unlock(&sLock);
    }
}

/* Waits until cond(q) holds: spins a little, then sleeps (who: the waiter's sSleeping bit). */
static void wait_for(int (*cond)(Queue *), Queue *q, uint32_t who) {
    int spin;
    for (spin = 0; spin < 200; spin++) {
        if (cond(q)) {
            return;
        }
        __builtin_ia32_pause();
    }
    pthread_mutex_lock(&sLock);
    __atomic_or_fetch(&sSleeping, who, __ATOMIC_SEQ_CST);
    while (!cond(q)) {
        pthread_cond_wait(&sCond, &sLock);
    }
    __atomic_and_fetch(&sSleeping, ~who, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&sLock);
}

static int has_rec(Queue *q) { return ld(&q->head.v) != ld(&q->tail.v); }
static int has_room(Queue *q) { return QSIZE - (q->head.v - ld(&q->tail.v)) >= q->need; }
/* Everything carried out: the GS thread puts a record's GIF data into sQ2 before it moves sQ1's tail. */
static int all_done(Queue *q) {
    (void)q;
    return ld(&sQ1.head.v) == ld(&sQ1.tail.v) && ld(&sQ2.head.v) == ld(&sQ2.tail.v);
}

/* Room in q for a record of `bytes` payload bytes; returns where its payload goes. */
static uint8_t *rec_begin(Queue *q, uint32_t kind, uint32_t n, uint32_t bytes, uint32_t *total) {
    uint32_t size = (uint32_t)sizeof(Rec) + ((bytes + 15) & ~15u), head = q->head.v, at = head & (QSIZE - 1);
    Rec *r;

    if (QSIZE - at < size) { /* no room before the end: a pad record to the start */
        q->need = QSIZE - at;
        if (!has_room(q)) {
            wake_all();
            wait_for(has_room, q, q->writer);
        }
        r = (Rec *)&q->q[at];
        r->kind = REC_PAD;
        r->bytes = QSIZE - at;
        head += QSIZE - at;
        __atomic_store_n(&q->head.v, head, __ATOMIC_RELEASE);
        at = 0;
    }
    q->need = size;
    if (!has_room(q)) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
        q->need = size + QSIZE / 4; /* a quarter more: the reader then wakes this thread once, not at every record */
        wake_all();
        wait_for(has_room, q, q->writer);
        q->need = size;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
        if (q == &sQ1) {
            sWaitNs += (uint64_t)((t1.tv_sec - t0.tv_sec) * 1000000000 + (t1.tv_nsec - t0.tv_nsec));
        }
    }
    r = (Rec *)&q->q[at];
    r->kind = kind;
    r->n = n;
    r->bytes = size;
    *total = size;
    return (uint8_t *)(r + 1);
}

/* A sleeping reader is woken once WAKE_BYTES wait for it, at a VBlank's record, or when the writer waits itself (rec_begin,
   reader): not at every record (each wake costs the writer a futex call; the GS thread writes one per XGKICK). */
#define WAKE_BYTES (256u << 10)

static void rec_end(Queue *q, uint32_t total) {
    uint32_t head = q->head.v + total;
    __atomic_store_n(&q->head.v, head, __ATOMIC_RELEASE);
    if ((__atomic_load_n(&sSleeping, __ATOMIC_SEQ_CST) & q->reader) &&
        (head - ld(&q->tail.v) >= WAKE_BYTES || ((const Rec *)&q->q[(head - total) & (QSIZE - 1)])->kind == REC_VBLANK)) {
        wake_all();
    }
}

/* A copy of `bytes` bytes as one record of q. */
static void rec_put(Queue *q, uint32_t kind, uint32_t n, uint32_t path, const void *data, uint32_t bytes) {
    uint32_t total;
    uint8_t *p = rec_begin(q, kind, n, bytes, &total);
    ((Rec *)p - 1)->path = path;
    memcpy(p, data, bytes);
    rec_end(q, total);
}

/* Room for up to maxQwc quadwords of GIF data in the GIF thread's queue, written in place (XGKICK: no copy in between);
   NULL when the GIF runs on this thread. Gs_AsyncGifCommit then queues the qwc written. */
static Rec *sReserved;

uint8_t *Gs_AsyncGifReserve(uint32_t maxQwc) {
    uint32_t total;
    uint8_t *p;
    if (!sSplit) {
        return NULL;
    }
    p = rec_begin(&sQ2, REC_GIF, 0, maxQwc * 16, &total);
    sReserved = (Rec *)p - 1;
    return p;
}

void Gs_AsyncGifCommit(int path, uint32_t qwc) {
    Rec *r = sReserved;
    sReserved = NULL;
    if (r == NULL || qwc == 0) {
        return;
    }
    r->n = qwc;
    r->path = (uint32_t)path;
    r->bytes = (uint32_t)sizeof(Rec) + qwc * 16;
    rec_end(&sQ2, r->bytes);
}

/* GIF data from VIF1 (PATH2) and VU1 (PATH1): to the GIF thread, or carried out at once. */
void Gs_AsyncGif(int path, const uint8_t *p, uint32_t qwc) {
    if (sSplit && qwc != 0) {
        rec_put(&sQ2, REC_GIF, qwc, (uint32_t)path, p, qwc * 16);
        return;
    }
    Gs_Gif(path, p, qwc);
}

static void run_vif(const Rec *r, const uint8_t *data) {
    switch (r->kind) {
    case REC_VIF1: Gs_Vif1((const uint32_t *)data, r->n); break;
    case REC_GIF3: Gs_AsyncGif(3, data, r->n); break;
    case REC_VBLANK:
        if (sSplit) {
            rec_put(&sQ2, REC_VBLANK, r->n, 0, data, (uint32_t)sizeof(DispRegs));
        } else {
            DispRegs regs;
            memcpy(&regs, data, sizeof regs);
            Gs_VBlankRun(r->n, &regs);
        }
        break;
    default: break;
    }
}

static void run_gif(const Rec *r, const uint8_t *data) {
    switch (r->kind) {
    case REC_GIF: Gs_Gif((int)r->path, data, r->n); break;
    case REC_VBLANK: {
        DispRegs regs;
        memcpy(&regs, data, sizeof regs);
        Gs_VBlankRun(r->n, &regs);
        break;
    }
    default: break;
    }
}

/* A queue's reader: carries its records out in order, forever. */
static void reader(Queue *q, void (*run)(const Rec *, const uint8_t *), uint32_t *cpuMs) {
    for (;;) {
        uint32_t tail = q->tail.v;
        const Rec *r;
        if (ld(&q->head.v) == tail) {
            struct timespec ts;
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
            __atomic_store_n(cpuMs, (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000), __ATOMIC_RELAXED);
            wake_all(); /* the game's thread may wait for the end */
            wait_for(has_rec, q, q->reader);
        }
        r = (const Rec *)&q->q[tail & (QSIZE - 1)];
        run(r, (const uint8_t *)(r + 1));
        __atomic_store_n(&q->tail.v, tail + r->bytes, __ATOMIC_RELEASE);
        {
            /* the writer waiting for room (it waits for some more than a record: see rec_begin), or the game's
               thread for the end */
            uint32_t s = __atomic_load_n(&sSleeping, __ATOMIC_SEQ_CST);
            if (((s & q->writer) && has_room(q)) || ((s & 2) && all_done(NULL))) {
                wake_all();
            }
        }
    }
}

static void *gs_main(void *arg) {
    (void)arg;
    reader(&sQ1, run_vif, &sCpuMs[0]);
    return NULL;
}

static void *gif_main(void *arg) {
    (void)arg;
    reader(&sQ2, run_gif, &sCpuMs[1]);
    return NULL;
}

/* Whether the GS path runs on its own thread (decided once, at the first transfer). TTT_GS_ASYNC: 0 off, 1 one GS
   thread, 2 two: VIF1 / VU1, and the GIF / GS front end. The default: 2 with the GL back end, 1 with raster threads
   (the software rasteriser's eight workers are its limit and want the processors: a third GS thread slowed it). */
int Gs_AsyncOn(void) {
    if (sOn < 0) {
        const char *e = getenv("TTT_GS_ASYNC");
        int want = e != NULL ? atoi(e) : Gs_ThreadsWanted() ? (Port_WindowGl() ? 2 : 1) : 0;
        pthread_t t;
        if (getenv("TTT_SELFTEST") != NULL || getenv("TTT_GS_TRACE") != NULL || getenv("TTT_GS_TRACE_PRIMS") != NULL) {
            want = 0;
        }
        sOn = want > 0;
        if (want >= 2) {
            sQ2.q = malloc(QSIZE);
            if (sQ2.q != NULL && pthread_create(&t, NULL, gif_main, NULL) == 0) {
                pthread_detach(t);
                sSplit = 1;
            } else {
                fprintf(stderr, "ttt: gs: cannot start the GIF thread: the GS thread runs the GIF\n");
            }
        }
        if (sOn) {
            sQ1.q = malloc(QSIZE);
            if (sQ1.q == NULL || pthread_create(&t, NULL, gs_main, NULL) != 0) {
                fprintf(stderr, "ttt: gs: cannot start the GS thread: running it on the game's thread\n");
                sOn = 0;
                sSplit = 0; /* the GIF thread (if started) then never gets records */
            } else {
                pthread_detach(t);
            }
        }
    }
    return sOn;
}

/* Waits until the GS (and GIF) thread carried out everything queued, then brings its CSR / SIGLBLID effects into the
   registers. The game's thread only. */
void Gs_AsyncDrain(void) {
    if (sOn > 0 && !all_done(NULL)) {
        wake_all();
        wait_for(all_done, NULL, 2);
    }
    Gs_AsyncRegs();
}

/* The GS's CSR / SIGLBLID effects so far into the registers (the game's thread: at VBlank and at drains). */
void Gs_AsyncRegs(void) {
    if (sOn <= 0) {
        return;
    }
    pthread_mutex_lock(&sRegLock);
    if (sCsrBits != 0) {
        GS_PRIV(GS_CSR) |= sCsrBits;
        sCsrBits = 0;
    }
    if (sSigLblIdSet) {
        GS_PRIV(GS_SIGLBLID) = sSigLblId;
    }
    pthread_mutex_unlock(&sRegLock);
}

/* gs_core.c's SIGNAL / FINISH / LABEL: CSR bits and SIGLBLID under a mask, kept here when the GS has its own thread. */
int Gs_AsyncRegWrite(uint64_t csrBits, uint64_t sigMask, uint64_t sigValue) {
    if (sOn <= 0) {
        return 0;
    }
    pthread_mutex_lock(&sRegLock);
    if (!sSigLblIdSet) {
        sSigLblId = GS_PRIV(GS_SIGLBLID); /* nothing else writes it: the game's thread copies it back */
        sSigLblIdSet = 1;
    }
    sCsrBits |= csrBits;
    sSigLblId = (sSigLblId & ~sigMask) | (sigValue & sigMask);
    pthread_mutex_unlock(&sRegLock);
    return 1;
}

/* The DMA channels' data (plat_hw.c): VIF1 words, GIF (PATH3) quadwords, queued or carried out at once. */
void Gs_DmaVif1(const uint32_t *w, uint32_t count) {
    if (!Gs_AsyncOn() || count == 0) {
        Gs_Vif1(w, count);
        return;
    }
    rec_put(&sQ1, REC_VIF1, count, 0, w, count * 4);
}

void Gs_DmaGif3(const uint8_t *data, uint32_t qwc) {
    if (!Gs_AsyncOn() || qwc == 0) {
        Gs_Gif(3, data, qwc);
        return;
    }
    rec_put(&sQ1, REC_GIF3, qwc, 3, data, qwc * 16);
}

/* A VBlank (Gs_VBlank, the game's thread): its display registers go into the queue, in order with the drawing. */
int Gs_AsyncVBlank(uint32_t vblank, const DispRegs *regs) {
    if (!Gs_AsyncOn()) {
        return 0;
    }
    Gs_AsyncRegs();
    rec_put(&sQ1, REC_VBLANK, vblank, 0, regs, (uint32_t)sizeof *regs);
    return 1;
}

/* TTT_GS_STATS: the game's thread's CPU time spent waiting for room in the queue. */
uint64_t Gs_AsyncWaitNs(void) { return sWaitNs; }
uint32_t Gs_AsyncCpuMs(int k) { return __atomic_load_n(&sCpuMs[k & 1], __ATOMIC_RELAXED); }
