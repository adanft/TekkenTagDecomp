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

#define QSIZE (32u << 20) /* bytes; a power of two */
enum { REC_PAD, REC_VIF1, REC_GIF3, REC_VBLANK };

typedef struct {
    uint32_t kind, n; /* n: words (VIF1), quadwords (GIF3), the VBlank's number */
    uint32_t bytes;   /* the record's size, a multiple of 16 */
    uint32_t pad;
} Rec;

typedef struct {
    uint32_t v;
    uint8_t pad[60];
} __attribute__((aligned(64))) Counter;

static int sOn = -1;      /* -1: not decided yet */
static uint8_t *sQ;
static Counter sHeadC, sTailC; /* bytes written by the game's thread / carried out by the GS thread */
static uint32_t sSleeping;     /* 1: the GS thread waits for records, 2: the game's thread waits for room or the end */
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sCond = PTHREAD_COND_INITIALIZER;
/* GS CSR / SIGLBLID effects of the GIF packets, written into the registers by the game's thread */
static pthread_mutex_t sRegLock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t sCsrBits, sSigLblId;
static int sSigLblIdSet;
static uint64_t sWaitNs;
static uint32_t sGsCpuMs; /* the GS thread's CPU time in ms as of its last wait (TTT_GS_STATS); 32 bits: 64-bit atomics
                             would call libatomic through new PLT entries (gotcha 20) */

static uint32_t ld(const uint32_t *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }

static void wake_all(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (__atomic_load_n(&sSleeping, __ATOMIC_SEQ_CST) != 0) {
        pthread_mutex_lock(&sLock);
        pthread_cond_broadcast(&sCond);
        pthread_mutex_unlock(&sLock);
    }
}

/* Waits until cond() holds: spins a little, then sleeps (who: 1 the GS thread, 2 the game's thread). */
static void wait_for(int (*cond)(void), uint32_t who) {
    int spin;
    for (spin = 0; spin < 200; spin++) {
        if (cond()) {
            return;
        }
        __builtin_ia32_pause();
    }
    pthread_mutex_lock(&sLock);
    __atomic_or_fetch(&sSleeping, who, __ATOMIC_SEQ_CST);
    while (!cond()) {
        pthread_cond_wait(&sCond, &sLock);
    }
    __atomic_and_fetch(&sSleeping, ~who, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&sLock);
}

static int has_rec(void) { return ld(&sHeadC.v) != ld(&sTailC.v); }
static int empty(void) { return ld(&sHeadC.v) == ld(&sTailC.v); }
static uint32_t sNeed;
static int has_room(void) { return QSIZE - (sHeadC.v - ld(&sTailC.v)) >= sNeed; }

static void *gs_main(void *arg) {
    (void)arg;
    for (;;) {
        uint32_t tail = sTailC.v;
        const Rec *r;
        const uint8_t *data;
        if (ld(&sHeadC.v) == tail) {
            struct timespec ts;
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
            __atomic_store_n(&sGsCpuMs, (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000), __ATOMIC_RELAXED);
            wake_all(); /* the game's thread may wait for the end */
            wait_for(has_rec, 1);
        }
        r = (const Rec *)&sQ[tail & (QSIZE - 1)];
        data = (const uint8_t *)(r + 1);
        switch (r->kind) {
        case REC_VIF1: Gs_Vif1(data == NULL ? NULL : (const uint32_t *)data, r->n); break;
        case REC_GIF3: Gs_Gif(3, data, r->n); break;
        case REC_VBLANK: {
            DispRegs regs;
            memcpy(&regs, data, sizeof regs);
            Gs_VBlankRun(r->n, &regs);
            break;
        }
        default: break;
        }
        __atomic_store_n(&sTailC.v, tail + r->bytes, __ATOMIC_RELEASE);
        if (__atomic_load_n(&sSleeping, __ATOMIC_SEQ_CST) & 2) {
            wake_all();
        }
    }
    return NULL;
}

/* Whether the GS path runs on its own thread (decided once, at the first transfer). */
int Gs_AsyncOn(void) {
    if (sOn < 0) {
        const char *e = getenv("TTT_GS_ASYNC");
        pthread_t t;
        sOn = e != NULL ? atoi(e) != 0 : Gs_ThreadsWanted();
        if (getenv("TTT_SELFTEST") != NULL || getenv("TTT_GS_TRACE") != NULL || getenv("TTT_GS_TRACE_PRIMS") != NULL) {
            sOn = 0;
        }
        if (sOn) {
            sQ = malloc(QSIZE);
            if (sQ == NULL || pthread_create(&t, NULL, gs_main, NULL) != 0) {
                fprintf(stderr, "ttt: gs: cannot start the GS thread: running it on the game's thread\n");
                sOn = 0;
            } else {
                pthread_detach(t);
            }
        }
    }
    return sOn;
}

/* Room for a record of `bytes` payload bytes; returns where its payload goes. */
static uint8_t *rec_begin(uint32_t kind, uint32_t n, uint32_t bytes, uint32_t *total) {
    uint32_t size = (uint32_t)sizeof(Rec) + ((bytes + 15) & ~15u), head = sHeadC.v, at = head & (QSIZE - 1);
    Rec *r;

    if (QSIZE - at < size) { /* no room before the end: a pad record to the start */
        sNeed = QSIZE - at;
        if (!has_room()) {
            wake_all();
            wait_for(has_room, 2);
        }
        r = (Rec *)&sQ[at];
        r->kind = REC_PAD;
        r->bytes = QSIZE - at;
        head += QSIZE - at;
        __atomic_store_n(&sHeadC.v, head, __ATOMIC_RELEASE);
        at = 0;
    }
    sNeed = size;
    if (!has_room()) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
        wake_all();
        wait_for(has_room, 2);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
        sWaitNs += (uint64_t)((t1.tv_sec - t0.tv_sec) * 1000000000 + (t1.tv_nsec - t0.tv_nsec));
    }
    r = (Rec *)&sQ[at];
    r->kind = kind;
    r->n = n;
    r->bytes = size;
    *total = size;
    return (uint8_t *)(r + 1);
}

static void rec_end(uint32_t total) {
    __atomic_store_n(&sHeadC.v, sHeadC.v + total, __ATOMIC_RELEASE);
    if (__atomic_load_n(&sSleeping, __ATOMIC_SEQ_CST) & 1) {
        wake_all();
    }
}

/* Waits until the GS thread carried out everything queued, then brings its CSR / SIGLBLID effects into the
   registers. The game's thread only. */
void Gs_AsyncDrain(void) {
    if (sOn > 0 && !empty()) {
        wake_all();
        wait_for(empty, 2);
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
    uint32_t total;
    uint8_t *p;

    if (!Gs_AsyncOn() || count == 0) {
        Gs_Vif1(w, count);
        return;
    }
    p = rec_begin(REC_VIF1, count, count * 4, &total);
    memcpy(p, w, (size_t)count * 4);
    rec_end(total);
}

void Gs_DmaGif3(const uint8_t *data, uint32_t qwc) {
    uint32_t total;
    uint8_t *p;

    if (!Gs_AsyncOn() || qwc == 0) {
        Gs_Gif(3, data, qwc);
        return;
    }
    p = rec_begin(REC_GIF3, qwc, qwc * 16, &total);
    memcpy(p, data, (size_t)qwc * 16);
    rec_end(total);
}

/* A VBlank (Gs_VBlank, the game's thread): its display registers go into the queue, in order with the drawing. */
int Gs_AsyncVBlank(uint32_t vblank, const DispRegs *regs) {
    uint32_t total;
    uint8_t *p;

    if (!Gs_AsyncOn()) {
        return 0;
    }
    Gs_AsyncRegs();
    p = rec_begin(REC_VBLANK, vblank, (uint32_t)sizeof *regs, &total);
    memcpy(p, regs, sizeof *regs);
    rec_end(total);
    return 1;
}

/* TTT_GS_STATS: the game's thread's CPU time spent waiting for room in the queue. */
uint64_t Gs_AsyncWaitNs(void) { return sWaitNs; }
uint32_t Gs_AsyncCpuMs(void) { return __atomic_load_n(&sGsCpuMs, __ATOMIC_RELAXED); }
