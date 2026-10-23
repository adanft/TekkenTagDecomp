/*
 * The hardware that runs beside the game on the PS2, driven from the game's wait loops.
 *
 * The registers are plain memory (plat_mem.c). Nothing changes them behind the game's back: every loop that waits
 * on the hardware calls Port_HwPoll (EE_HW_POLL, include/ee_asm.h), and so do the platform functions that wait
 * (sceGsSyncV). One poll advances a clock of bus cycles (BUSCLK, 147.456 MHz) and brings the hardware up to it:
 *
 *  - Timers 0-3 (0x10000000 + 0x800 n): COUNT runs while MODE.CUE is set, at BUSCLK, /16, /256 (H-blank counts as
 *    BUSCLK/9370). COUNT is read back from memory and advanced, so the game's own writes (Gfx_WaitVSync zeroes
 *    Timer 0) take effect as on the console.
 *  - VBlank: one VBLANK_START every 1/59.94 s (NTSC fields). It sets GS CSR.VSINT, flips CSR.FIELD and runs the
 *    INTC handlers of cause 2 (Gfx_VSyncHandler), held while interrupts are off (Port_Di) until Port_Ei.
 *  - DMA: a channel whose CHCR.STR is set is carried out at once and STR is cleared, D_STAT.CIS set and the
 *    channel's DMAC handlers run (when EnableDmac allowed them). The scratchpad channels (fromSPR 8, toSPR 9) copy
 *    their data; VIF0 / VIF1 / GIF / IPU / SIF go to a sink that only walks their chains (the GS path is T28).
 *    MFIFO (D_CTRL.MFD): fromSPR fills the ring RBOR / RBSR, the VIF1 or GIF drain walks its chain from the ring
 *    and stays busy while the ring is empty (the game streams its display lists through the scratchpad).
 *  - D_ENABLEW (0x1000F590) is mirrored into D_ENABLER (0x1000F520).
 *
 * The clock: headless (TTT_HEADLESS=1, or TTT_FRAMES=n) each poll is one Timer 0 tick (256 bus cycles) and
 * nothing sleeps, so a run is the same every time: the number of polls per frame, the spin generator stepped in
 * Gfx_WaitVSync and the boot seed taken from Timer 0 all repeat. Otherwise the clock follows the host's monotonic
 * time, as the console's follows its own, and the game's seed and spin generator vary from run to run as on the
 * PS2 (docs/known_bugs.md, section 8); polls far from the next event give the host CPU back.
 *
 * TTT_FRAMES=n stops the program after n VBlanks with a short report (modes reached, state, placeholders hit),
 * exit code 0; with TTT_HEADLESS=0 it does so on the real-time clock.
 * TTT_SEED=n pins the boot seed (Rng_Init) to n instead of Timer 0.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "port_addr.h"

#define REG(a) (*(volatile uint32_t *)Port_Ptr(a))

#define BUSCLK 147456000ull
#define TICK_CYCLES 256ull /* one Timer 0 tick at BUSCLK/256: the headless clock's step */

void Port_Di(void);
void Port_Ei(void);
uint32_t Port_Mfc0Status(void);
void Port_SetStatus(uint32_t s); /* plat_ee.c */

/* ---- state ---- */
static int sHeadless = -1;     /* -1: not decided yet */
static uint64_t sCycles;       /* bus cycles since the start */
static uint64_t sHostStart;    /* real-time clock origin (ns) */
static uint64_t sTimerFrac[4]; /* bus cycles not yet counted by each timer */
static uint64_t sTimerLast[4];
static uint64_t sVBlanks;      /* VBLANK_STARTs so far */
static uint32_t sFrameLimit;   /* TTT_FRAMES */
static int sInPoll, sInHandler;

#define MAX_HANDLERS 8
typedef int32_t (*Handler)(int32_t);
static struct { Handler fn; int used; } sIntc[16][MAX_HANDLERS], sDmac[16][MAX_HANDLERS];
static uint32_t sIntcMask, sDmacMask;   /* EnableIntc / EnableDmac */
static uint32_t sIntcPend, sDmacPend;   /* raised while interrupts were off */

/* ---- time ---- */
static uint64_t host_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Bus cycle of the n-th VBLANK_START: n fields of 1001/60000 s. */
static uint64_t vblank_cycle(uint64_t n) { return n * BUSCLK * 1001ull / 60000ull; }

int Port_Headless(void) {
    if (sHeadless < 0) {
        const char *h = getenv("TTT_HEADLESS"), *f = getenv("TTT_FRAMES");
        sFrameLimit = f != NULL ? (uint32_t)strtoul(f, NULL, 0) : 0;
        sHeadless = h != NULL ? atoi(h) != 0 : sFrameLimit != 0;
        sHostStart = host_ns();
    }
    return sHeadless;
}

uint64_t Port_HwCycles(void) { return sCycles; }
uint32_t Port_VBlankCount(void) { return (uint32_t)sVBlanks; }

/* ---- interrupt controllers (the kernel calls of plat_sys.c land here) ---- */
static int add_handler(void *tab, int cause, Handler fn) {
    typeof(sIntc[0][0]) (*t)[MAX_HANDLERS] = tab;
    int i;
    if (cause < 0 || cause >= 16) {
        return -1;
    }
    for (i = 0; i < MAX_HANDLERS; i++) {
        if (!t[cause][i].used) {
            t[cause][i].used = 1;
            t[cause][i].fn = fn;
            return i + 1;
        }
    }
    return -1;
}

static int remove_handler(void *tab, int cause, int id) {
    typeof(sIntc[0][0]) (*t)[MAX_HANDLERS] = tab;
    if (cause < 0 || cause >= 16 || id < 1 || id > MAX_HANDLERS || !t[cause][id - 1].used) {
        return -1;
    }
    t[cause][id - 1].used = 0;
    return 0;
}

int AddIntcHandler(int cause, Handler fn, int next) { (void)next; return add_handler(sIntc, cause, fn); }
int AddIntcHandler2(int cause, Handler fn, int next, void *arg) { (void)next; (void)arg; return add_handler(sIntc, cause, fn); }
int RemoveIntcHandler(int cause, int id) { return remove_handler(sIntc, cause, id); }
int AddDmacHandler(int ch, Handler fn, int next) { (void)next; return add_handler(sDmac, ch, fn); }
int RemoveDmacHandler(int ch, int id) { return remove_handler(sDmac, ch, id); }

static int set_mask(uint32_t *mask, int bit, int on) {
    int was = (*mask >> bit) & 1;
    if (bit < 0 || bit >= 16) {
        return -1;
    }
    *mask = on ? *mask | (1u << bit) : *mask & ~(1u << bit);
    return was;
}
int EnableIntc(int cause) { return set_mask(&sIntcMask, cause, 1); }
int DisableIntc(int cause) { return set_mask(&sIntcMask, cause, 0); }
int iEnableIntc(int cause) { return set_mask(&sIntcMask, cause, 1); }
int iDisableIntc(int cause) { return set_mask(&sIntcMask, cause, 0); }
int EnableDmac(int ch) { return set_mask(&sDmacMask, ch, 1); }
int DisableDmac(int ch) { return set_mask(&sDmacMask, ch, 0); }
int iEnableDmac(int ch) { return set_mask(&sDmacMask, ch, 1); }
int iDisableDmac(int ch) { return set_mask(&sDmacMask, ch, 0); }

/* Runs the handlers of every pending cause, as the kernel's interrupt dispatcher: interrupts off meanwhile. */
static void run_handlers(void *tab, uint32_t *pend, uint32_t mask) {
    typeof(sIntc[0][0]) (*t)[MAX_HANDLERS] = tab;
    int c, i;
    for (c = 0; c < 16; c++) {
        if (!(*pend & (1u << c))) {
            continue;
        }
        *pend &= ~(1u << c);
        if (!(mask & (1u << c))) {
            continue;
        }
        for (i = 0; i < MAX_HANDLERS; i++) {
            if (t[c][i].used) {
                t[c][i].fn(c);
            }
        }
    }
}

/* Delivers what is pending if interrupts are on (and not already inside a handler). Port_Ei calls it. */
void Port_HwDeliver(void) {
    uint32_t status;
    if (sInHandler || !(Port_Mfc0Status() & 0x10000u) || !(sIntcPend | sDmacPend)) {
        return;
    }
    sInHandler = 1;
    status = Port_Mfc0Status();
    Port_SetStatus(status & ~0x10000u);
    while (sIntcPend | sDmacPend) {
        run_handlers(sDmac, &sDmacPend, sDmacMask);
        run_handlers(sIntc, &sIntcPend, sIntcMask);
    }
    Port_SetStatus(status);
    sInHandler = 0;
}

/* ---- DMA ---- */
static const uint32_t kDmaBase[10] = { 0x10008000u, 0x10009000u, 0x1000A000u, 0x1000B000u, 0x1000B400u,
                                       0x1000C000u, 0x1000C400u, 0x1000C800u, 0x1000D000u, 0x1000D400u };
#define CHCR 0x00
#define MADR 0x10
#define QWC 0x20
#define TADR 0x30
#define SADR 0x80
#define D_STAT 0x1000E010u

uint32_t gPortDmaKicks[10]; /* transfers carried out per channel (the self-test and the report read them) */
uint32_t gPortDmaQwords[10];

/* Scratchpad address of SADR: 16 KB, quadword aligned. */
static uint8_t *spr(uint32_t sadr) { return (uint8_t *)Port_Ptr(PORT_SPR_ADDR + (sadr & 0x3FF0u)); }

/* MFIFO (D_CTRL.MFD = 2 VIF1, 3 GIF): fromSPR writes into the ring RBOR / RBSR, the drain channel reads its chain
   from there and stalls while the ring is empty. */
#define D_CTRL 0x1000E000u
#define D_RBSR 0x1000E040u
#define D_RBOR 0x1000E050u
static int mfifo_mode(void) { return (int)((REG(D_CTRL) >> 2) & 3); }
static int mfifo_drain(int ch) { return (mfifo_mode() == 2 && ch == 1) || (mfifo_mode() == 3 && ch == 2); }
static uint32_t ring_wrap(uint32_t a) { return REG(D_RBOR) | (a & REG(D_RBSR)); }

void Port_Vif0Feed(const uint32_t *w, uint32_t n); /* vu0_micro.c */

/* One block of a transfer: `qwc` quadwords at DMA address `addr`. */
static void dma_block(int ch, uint32_t addr, uint32_t qwc) {
    uint32_t base = kDmaBase[ch], i;
    gPortDmaQwords[ch] += qwc;
    if (ch == 9 && qwc != 0) { /* toSPR: memory -> scratchpad */
        uint32_t sadr = REG(base + SADR);
        for (i = 0; i < qwc; i++) {
            memcpy(spr(sadr + i * 16), (uint8_t *)Port_DmaPtr(addr + i * 16), 16);
        }
        REG(base + SADR) = (sadr + qwc * 16) & 0x3FF0u;
    } else if (ch == 8 && qwc != 0) { /* fromSPR: scratchpad -> memory (or the MFIFO ring) */
        uint32_t sadr = REG(base + SADR);
        int ring = mfifo_mode() >= 2;
        for (i = 0; i < qwc; i++) {
            uint32_t a = ring ? ring_wrap(addr + i * 16) : addr + i * 16;
            memcpy((uint8_t *)Port_DmaPtr(a), spr(sadr + i * 16), 16);
        }
        REG(base + SADR) = (sadr + qwc * 16) & 0x3FF0u;
    }
    if (ch == 0 && qwc != 0) { /* VIF0: microcode uploads (vu0_micro.c) */
        Port_Vif0Feed((const uint32_t *)Port_DmaPtr(addr), qwc * 4);
    }
    /* VIF1 / GIF / IPU / SIF: dropped here; the GS path (T28) takes the blocks over. */
}

/* Source chain (CHCR.MOD = 1) from TADR: refe / cnt / next / ref / refs / call / ret / end. Returns 0 when an MFIFO
   drain stalls on an empty ring (the channel stays busy and goes on at a later poll), 1 when the chain ended. */
static int dma_source_chain(int ch) {
    uint32_t base = kDmaBase[ch];
    uint32_t tadr = REG(base + TADR);
    int ring = mfifo_drain(ch), n, done = 0;
    int sp = (int)((REG(base + CHCR) >> 4) & 3); /* ASP */
    for (n = 0; n < 100000 && !done; n++) {
        const uint32_t *tag;
        uint32_t qwc, id, addr, irq;
        if (ring) {
            tadr = ring_wrap(tadr);
            if (Port_EePhys(tadr) == Port_EePhys(REG(0x1000D000u + MADR))) {
                REG(base + TADR) = tadr;
                return 0; /* nothing more in the ring yet */
            }
        }
        tag = (const uint32_t *)Port_DmaPtr(tadr);
        qwc = tag[0] & 0xFFFF;
        id = (tag[0] >> 28) & 7;
        addr = tag[1];
        irq = tag[0] >> 31;
        if (ch == 0 && (REG(base + CHCR) & 0x40u)) { /* TTE: the tag's upper half goes to VIF0 as two codes */
            Port_Vif0Feed(&tag[2], 2);
        }
        switch (id) {
        case 0: /* refe */
            dma_block(ch, addr, qwc);
            tadr += 16;
            done = 1;
            break;
        case 1: /* cnt: data follows the tag */
            dma_block(ch, tadr + 16, qwc);
            tadr += 16 + qwc * 16;
            break;
        case 2: /* next */
            dma_block(ch, tadr + 16, qwc);
            tadr = addr;
            break;
        case 3: /* ref */
        case 4: /* refs */
            dma_block(ch, addr, qwc);
            tadr += 16;
            break;
        case 5: /* call */
            dma_block(ch, tadr + 16, qwc);
            if (sp >= 2) {
                done = 1;
                break;
            }
            REG(base + 0x40 + 0x10 * (uint32_t)sp) = tadr + 16 + qwc * 16; /* ASR0 / ASR1 */
            sp++;
            tadr = addr;
            break;
        case 6: /* ret */
            dma_block(ch, tadr + 16, qwc);
            if (sp == 0) {
                tadr += 16 + qwc * 16;
                done = 1;
                break;
            }
            sp--;
            tadr = REG(base + 0x40 + 0x10 * (uint32_t)sp);
            break;
        case 7: /* end */
            dma_block(ch, tadr + 16, qwc);
            tadr += 16 + qwc * 16;
            done = 1;
            break;
        }
        if (!done && irq && (REG(base + CHCR) & 0x80u)) { /* TIE: stop after a tag with IRQ */
            done = 1;
        }
    }
    REG(base + CHCR) = (REG(base + CHCR) & ~0x30u) | ((uint32_t)sp << 4);
    REG(base + TADR) = ring ? ring_wrap(tadr) : tadr;
    return 1;
}

/* Destination chain (fromSPR with MOD = 1): the tags are in the scratchpad. */
static void dma_dest_chain(int ch) {
    uint32_t base = kDmaBase[ch];
    int n;
    for (n = 0; n < 100000; n++) {
        uint32_t sadr = REG(base + SADR);
        const uint32_t *tag = (const uint32_t *)spr(sadr);
        uint32_t qwc = tag[0] & 0xFFFF, id = (tag[0] >> 28) & 7, addr = tag[1];
        REG(base + SADR) = (sadr + 16) & 0x3FF0u;
        dma_block(ch, addr, qwc);
        if (id == 7) { /* end (cnts 0 and cnt 1 go on) */
            break;
        }
        if ((tag[0] >> 31) && (REG(base + CHCR) & 0x80u)) {
            break;
        }
    }
}

/* Carries out a started transfer; 0 when it has to wait (an MFIFO drain on an empty ring). */
static int dma_run(int ch) {
    uint32_t base = kDmaBase[ch];
    uint32_t chcr = REG(base + CHCR), mode = (chcr >> 2) & 3;
    if (mode == 1 && ch == 8) {
        dma_dest_chain(ch);
    } else if (mode == 1) {
        if (!dma_source_chain(ch)) {
            return 0;
        }
    } else {
        uint32_t madr = REG(base + MADR), qwc = REG(base + QWC) & 0xFFFF;
        dma_block(ch, madr, qwc);
        madr += qwc * 16;
        REG(base + MADR) = ch == 8 && mfifo_mode() >= 2 ? ring_wrap(madr) : madr;
    }
    REG(base + QWC) = 0;
    REG(base + CHCR) = REG(base + CHCR) & ~0x100u;
    gPortDmaKicks[ch]++;
    REG(D_STAT) |= 1u << ch; /* CIS */
    sDmacPend |= 1u << ch;
    return 1;
}

static void dma_poll(void) {
    int ch, again = 1, rounds = 0;
    while (again && rounds++ < 64) { /* a handler may start the next transfer, a fill may unblock a drain */
        again = 0;
        for (ch = 0; ch < 10; ch++) {
            if ((REG(kDmaBase[ch] + CHCR) & 0x100u) && dma_run(ch)) {
                again = 1;
            }
        }
        Port_HwDeliver();
    }
}

/* ---- timers and VBlank ---- */
static void timers(void) {
    int t;
    for (t = 0; t < 4; t++) {
        uint32_t base = 0x10000000u + 0x800u * (uint32_t)t;
        uint32_t mode = REG(base + 0x10);
        uint64_t div, delta = sCycles - sTimerLast[t];
        sTimerLast[t] = sCycles;
        if (!(mode & 0x80u)) {
            sTimerFrac[t] = 0;
            continue;
        }
        div = (mode & 3) == 0 ? 1 : (mode & 3) == 1 ? 16 : (mode & 3) == 2 ? 256 : 9370;
        sTimerFrac[t] += delta;
        if (sTimerFrac[t] >= div) {
            uint64_t ticks = sTimerFrac[t] / div;
            sTimerFrac[t] -= ticks * div;
            REG(base) = (uint32_t)((REG(base) + ticks) & 0xFFFF);
        }
    }
}

extern uint16_t gFrameCount; /* src/main.c */
extern int16_t gMode, gSubMode;
static void report_and_exit(void);
static void track_mode(void);

static void vblank(void) {
    sVBlanks++;
    REG(0x12001000u) = (REG(0x12001000u) | 0x8u) ^ 0x2000u; /* CSR: VSINT, FIELD flips */
    sIntcPend |= 1u << 2;                                   /* INTC VBLANK_START */
    track_mode();
    if (sFrameLimit != 0 && sVBlanks >= sFrameLimit) {
        report_and_exit();
    }
}

/* Advances the clock to `to` (bus cycles), raising every VBlank on the way. */
static void advance(uint64_t to) {
    while (vblank_cycle(sVBlanks + 1) <= to) {
        sCycles = vblank_cycle(sVBlanks + 1);
        timers();
        vblank();
        Port_HwDeliver();
    }
    if (to > sCycles) {
        sCycles = to;
    }
    timers();
}

void Port_HwPoll(void) {
    uint64_t to;
    if (sInPoll) {
        return;
    }
    sInPoll = 1;
    if (Port_Headless()) {
        to = sCycles + TICK_CYCLES;
    } else {
        uint64_t now = host_ns() - sHostStart, next = vblank_cycle(sVBlanks + 1);
        to = now * (BUSCLK / 1000000ull) / 1000ull;
        if (next > to && next - to > BUSCLK / 1000ull) { /* more than 1 ms to the next VBlank: give the CPU back */
            struct timespec ts = { 0, 200000 };
            nanosleep(&ts, NULL);
        }
        if (to < sCycles) {
            to = sCycles;
        }
    }
    REG(0x1000F520u) = REG(0x1000F590u); /* D_ENABLER follows D_ENABLEW */
    advance(to);
    dma_poll();
    Port_HwDeliver();
    sInPoll = 0;
}

/* Waits for the next VBLANK_START (sceGsSyncV); returns the field now shown (0 even, 1 odd). */
int Port_WaitVBlank(void) {
    uint64_t n = sVBlanks;
    if (sInPoll) {
        return (int)((REG(0x12001000u) >> 13) & 1);
    }
    while (sVBlanks == n) {
        if (Port_Headless()) {
            /* nothing else can happen before it: jump there */
            sInPoll = 1;
            REG(0x1000F520u) = REG(0x1000F590u);
            advance(vblank_cycle(sVBlanks + 1));
            dma_poll();
            Port_HwDeliver();
            sInPoll = 0;
        } else {
            Port_HwPoll();
        }
    }
    return (int)((REG(0x12001000u) >> 13) & 1);
}

/* Rng_Init's seed (src/rng_misc.c): Timer 0 brought up to date, or TTT_SEED. */
static int sSeed;
int Port_BootSeed(int timer0) {
    const char *s = getenv("TTT_SEED");
    (void)timer0;
    if (s != NULL) {
        sSeed = (int)strtoul(s, NULL, 0);
    } else {
        Port_HwPoll();
        sSeed = (int)REG(0x10000000u);
    }
    return sSeed;
}

/* ---- headless report ---- */
static int16_t sLastMode = -1;
static char sModes[2048];
static void track_mode(void) {
    if (gMode != sLastMode) {
        size_t n = strlen(sModes);
        if (n + 24 < sizeof sModes) {
            snprintf(sModes + n, sizeof sModes - n, "%s%d@%u", n ? " " : "", gMode, (unsigned)sVBlanks);
        }
        sLastMode = gMode;
    }
}

extern int32_t gRngSpin; /* src/rng_misc.c */
static uint32_t fnv(const uint8_t *p, uint32_t n, uint32_t h) {
    while (n--) {
        h = (h ^ *p++) * 16777619u;
    }
    return h;
}

void Port_StubReport(void); /* plat_stub.c: library functions still answered by placeholders */

/* Where the run is (Port_Missing prints it before stopping on a placeholder). */
void Port_HwSummary(void) {
    printf("ttt: at vblank %u, frame %u; modes: %s; mode %d sub %d\n", (unsigned)sVBlanks, (unsigned)gFrameCount,
           sModes, gMode, gSubMode);
}

static void report_and_exit(void) {
    uint32_t h = fnv((const uint8_t *)Port_Ptr(0x005F6B00u), 0x1000, 2166136261u);
    printf("ttt: %s: %u vblanks, %u frames\n", sHeadless ? "headless" : "real time", (unsigned)sVBlanks, (unsigned)gFrameCount);
    printf("ttt: modes: %s\n", sModes);
    printf("ttt: last mode %d sub %d\n", gMode, gSubMode);
    printf("ttt: state: seed 0x%x spin 0x%08x sbss-hash 0x%08x cycles %llu\n", (unsigned)sSeed,
           (unsigned)gRngSpin, h, (unsigned long long)sCycles);
    Port_StubReport();
    fflush(stdout);
    exit(0);
}

/* ---- self-test (TTT_SELFTEST=hw) ---- */
static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: hw self-test FAILED: %s\n", what);
        sFails++;
    }
}
static int sHandled[16];
static int32_t count_handler(int32_t c) { sHandled[c & 15]++; return 0; }

void Port_HwSelfTest(void) {
    uint8_t *src = (uint8_t *)Port_Ptr(0x00100000u);
    uint32_t i, v0;
    int id;

    sHeadless = 1;
    /* DMA: VIF1 in normal mode completes at the next poll and its handler runs once interrupts allow it */
    id = AddDmacHandler(1, count_handler, 0);
    EnableDmac(1);
    REG(0x10009000u + MADR) = 0x00100000u;
    REG(0x10009000u + QWC) = 4;
    REG(0x10009000u) = 0x101;
    Port_Di();
    Port_HwPoll();
    expect(!(REG(0x10009000u) & 0x100u), "VIF1 STR cleared");
    expect(REG(0x10009000u + QWC) == 0 && REG(0x10009000u + MADR) == 0x00100040u, "VIF1 MADR / QWC advanced");
    expect(sHandled[1] == 0, "DMAC handler held while interrupts are off");
    Port_Ei();
    expect(sHandled[1] == 1, "DMAC handler run at ei");
    RemoveDmacHandler(1, id);
    DisableDmac(1);
    /* toSPR normal, then fromSPR back */
    for (i = 0; i < 64; i++) {
        src[i] = (uint8_t)(i * 7 + 1);
    }
    REG(0x1000D400u + MADR) = 0x00100000u;
    REG(0x1000D400u + QWC) = 4;
    REG(0x1000D400u + SADR) = 0x100;
    REG(0x1000D400u) = 0x101;
    Port_HwPoll();
    expect(memcmp(Port_Ptr(0x70000100u), src, 64) == 0, "toSPR copied to the scratchpad");
    REG(0x1000D000u + MADR) = 0x20100100u; /* uncached view */
    REG(0x1000D000u + QWC) = 4;
    REG(0x1000D000u + SADR) = 0x100;
    REG(0x1000D000u) = 0x100;
    Port_HwPoll();
    expect(memcmp(src + 0x100, src, 64) == 0, "fromSPR copied back to EE RAM");
    /* toSPR source chain: cnt 1 qword then end 1 qword */
    {
        uint32_t *t = (uint32_t *)Port_Ptr(0x00100200u);
        memset(t, 0, 64);
        t[0] = 0x10000001u; t[4] = 0xAAAA5555u; t[8] = 0x70000001u; t[12] = 0x12345678u;
        REG(0x1000D400u + TADR) = 0x00100200u;
        REG(0x1000D400u + SADR) = 0x200;
        REG(0x1000D400u) = 0x105;
        Port_HwPoll();
        expect(*(uint32_t *)Port_Ptr(0x70000200u) == 0xAAAA5555u &&
               *(uint32_t *)Port_Ptr(0x70000210u) == 0x12345678u, "toSPR chain copied cnt and end data");
        expect(!(REG(0x1000D400u) & 0x100u), "toSPR chain STR cleared");
    }
    /* MFIFO: a VIF1 drain on an empty ring waits; a fromSPR fill of an end tag lets it finish */
    {
        uint32_t *s = (uint32_t *)Port_Ptr(0x70000300u);
        REG(0x1000E050u) = 0x00110000u; /* RBOR */
        REG(0x1000E040u) = 0x0FF0u;     /* RBSR */
        REG(0x1000E000u) = (REG(0x1000E000u) & ~0xCu) | 8u;
        REG(0x1000D000u + MADR) = 0x00110000u;
        REG(0x10009000u + TADR) = 0x00110000u;
        REG(0x10009000u) = 0x105;
        Port_HwPoll();
        expect((REG(0x10009000u) & 0x100u) != 0, "MFIFO drain waits on an empty ring");
        memset(s, 0, 16);
        s[0] = 0x70000000u; /* end, qwc 0 */
        REG(0x1000D000u + QWC) = 1;
        REG(0x1000D000u + SADR) = 0x300;
        REG(0x1000D000u) = 0x100;
        Port_HwPoll();
        expect(!(REG(0x10009000u) & 0x100u), "MFIFO drain ends on the end tag the fill brought");
        expect(REG(0x1000D000u + MADR) == 0x00110010u, "fromSPR MADR advanced in the ring");
        REG(0x1000E000u) &= ~0xCu;
    }
    /* Timer 0 at BUSCLK/256: one tick per headless poll */
    REG(0x10000010u) = 0x82;
    REG(0x10000000u) = 0;
    Port_HwPoll();
    v0 = REG(0x10000000u);
    for (i = 0; i < 100; i++) {
        Port_HwPoll();
    }
    expect(REG(0x10000000u) - v0 == 100, "Timer 0 counts one tick per headless poll");
    /* VBlank: the counter, the handler, the period */
    {
        uint64_t n = sVBlanks, c0;
        id = AddIntcHandler(2, count_handler, 0);
        EnableIntc(2);
        Port_WaitVBlank();
        c0 = sCycles;
        expect(sVBlanks == n + 1 && sHandled[2] == 1, "VBlank counted and handler run");
        for (i = 0; i < 59; i++) {
            Port_WaitVBlank();
        }
        expect(sVBlanks == n + 60 && sHandled[2] == 60, "60 VBlanks, 60 handler calls");
        expect(sCycles - c0 >= 59 * 2460057ull && sCycles - c0 <= 59 * 2460058ull, "VBlank period 1/59.94 s");
        Port_Di();
        Port_WaitVBlank();
        expect(sHandled[2] == 60, "VBlank handler held while interrupts are off");
        Port_Ei();
        expect(sHandled[2] == 61, "held VBlank delivered at ei");
        DisableIntc(2);
        Port_WaitVBlank();
        expect(sHandled[2] == 61, "disabled cause not delivered");
        RemoveIntcHandler(2, id);
    }
    /* D_ENABLEW mirrored into D_ENABLER */
    REG(0x1000F590u) = 0x10000;
    Port_HwPoll();
    expect(REG(0x1000F520u) == 0x10000, "D_ENABLER follows D_ENABLEW");
    REG(0x1000F590u) = 0;
    Port_HwPoll();
    if (sFails == 0) {
        printf("ttt: hw self-test OK\n");
    }
    exit(sFails != 0);
}

void Port_RpcSelfTest(void);   /* plat_sys.c */
void Port_FloatSelfTest(void); /* plat_floattest.c */

/* TTT_SELFTEST=hw | rpc | float: run after the memory map (plat_mem.c, constructor 101), print and exit. */
__attribute__((constructor(102))) static void Port_HwSelfTestEntry(void) {
    const char *t = getenv("TTT_SELFTEST");
    if (t != NULL && strcmp(t, "hw") == 0) {
        Port_HwSelfTest();
    }
    if (t != NULL && strcmp(t, "rpc") == 0) {
        Port_RpcSelfTest();
    }
    if (t != NULL && strcmp(t, "float") == 0) {
        Port_FloatSelfTest();
    }
}
