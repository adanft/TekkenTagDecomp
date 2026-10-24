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
 *    their data; VIF0 feeds the VU0 microcode loader (vu0_micro.c), VIF1 and GIF the GS path (gs_vif1.c,
 *    gs_core.c); IPU / SIF only walk their chains.
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
#include "gs.h"

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
/* TTT_TIME_FROM=<vblank> (measurements): the GS path is drained at that VBlank and the host time from there to the end of
   the run (the GS path drained again) is reported. */
static uint32_t sTimeFrom;
static uint64_t sTimeT0;
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
        sTimeFrom = getenv("TTT_TIME_FROM") != NULL ? (uint32_t)strtoul(getenv("TTT_TIME_FROM"), NULL, 0) : 0;
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
    if (ch == 1 && qwc != 0) { /* VIF1: VU1 and PATH2 (gs_vif1.c) */
        Gs_DmaVif1((const uint32_t *)Port_DmaPtr(addr), qwc * 4);
    }
    if (ch == 2 && qwc != 0) { /* GIF: PATH3 (gs_core.c) */
        Gs_DmaGif3((const uint8_t *)Port_DmaPtr(addr), qwc);
    }
    /* IPU / SIF: dropped here. */
}

/* A block that follows its tag inside the MFIFO ring: it may wrap around the ring's end. */
static void dma_block_in(int ch, uint32_t addr, uint32_t qwc, int ring) {
    while (ring && qwc != 0) {
        uint32_t a = ring_wrap(addr), end = REG(D_RBOR) + REG(D_RBSR) + 16, n = (end - a) / 16;
        if (n == 0 || n > qwc) {
            n = qwc;
        }
        dma_block(ch, a, n);
        addr = a + n * 16;
        qwc -= n;
    }
    if (!ring) {
        dma_block(ch, addr, qwc);
    }
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
        if (ch == 1 && (REG(base + CHCR) & 0x40u)) { /* the same for VIF1 */
            Gs_DmaVif1(&tag[2], 2);
        }
        if (ch == 9 && (REG(base + CHCR) & 0x40u)) { /* toSPR with TTE: the whole tag quadword goes in front of its data */
            uint32_t sadr = REG(base + SADR);
            memcpy(spr(sadr), tag, 16);
            REG(base + SADR) = (sadr + 16) & 0x3FF0u;
        }
        switch (id) {
        case 0: /* refe */
            dma_block(ch, addr, qwc);
            tadr += 16;
            done = 1;
            break;
        case 1: /* cnt: data follows the tag */
            dma_block_in(ch, tadr + 16, qwc, ring);
            tadr += 16 + qwc * 16;
            break;
        case 2: /* next */
            dma_block_in(ch, tadr + 16, qwc, ring);
            tadr = addr;
            break;
        case 3: /* ref */
        case 4: /* refs */
            dma_block(ch, addr, qwc);
            tadr += 16;
            break;
        case 5: /* call */
            dma_block_in(ch, tadr + 16, qwc, ring);
            if (sp >= 2) {
                done = 1;
                break;
            }
            REG(base + 0x40 + 0x10 * (uint32_t)sp) = tadr + 16 + qwc * 16; /* ASR0 / ASR1 */
            sp++;
            tadr = addr;
            break;
        case 6: /* ret */
            dma_block_in(ch, tadr + 16, qwc, ring);
            if (sp == 0) {
                tadr += 16 + qwc * 16;
                done = 1;
                break;
            }
            sp--;
            tadr = REG(base + 0x40 + 0x10 * (uint32_t)sp);
            break;
        case 7: /* end */
            dma_block_in(ch, tadr + 16, qwc, ring);
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

/* The GIF channel's transfer carried out now, if one was started (the movie player, T33): on the PS2 sceGsSyncPath
   waits for it, and a second kick right after (func_0039D320 then func_0039D9F0) must not replace the first. The
   time does not advance; the channel's interrupt is delivered at the next poll, as before. */
void Port_GifSync(void) {
    if (!sInPoll && (REG(kDmaBase[2] + CHCR) & 0x100u)) {
        sInPoll = 1;
        dma_run(2);
        sInPoll = 0;
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
void Port_PadVBlank(uint32_t n); /* plat_pad.c */
void Port_PadReport(void);
void Port_SndVBlank(uint32_t n); /* plat_snd.c */
void Port_SndEnd(void);

static void vblank(void) {
    sVBlanks++;
    Port_PadVBlank((uint32_t)sVBlanks); /* the pads' state for this field: latched once, before the handler */
    Gs_VBlank((uint32_t)sVBlanks); /* the frame dump: the picture shown during the field that ends here */
    Port_SndVBlank((uint32_t)sVBlanks); /* the sound of that field */
    if (sTimeFrom != 0 && sVBlanks == sTimeFrom) {
        Gs_Finish();
        sTimeT0 = host_ns();
    }
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
        if (to > vblank_cycle(sVBlanks + 2)) {
            /* more than a VBlank behind (the host stalled): the lost time is dropped, at most one VBlank per poll.
               A burst of VBlanks in one poll would run the game's VBlank handler again before the DMA it waits for
               could finish (polls inside a handler return at once). */
            uint64_t keep = next * 1000ull / (BUSCLK / 1000000ull);
            sHostStart += now - keep;
            to = next;
        }
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

/* TTT_PEEK=<addr>[:<words>],...: EE RAM words at the end of a TTT_FRAMES run, for tests and debugging. */
static void peek_report(void) {
    const char *p = getenv("TTT_PEEK");

    while (p != NULL && *p != 0) {
        char *end;
        uint32_t a = (uint32_t)strtoul(p, &end, 16) & ~3u, n = 1, k;
        if (end == p) {
            break;
        }
        if (*end == ':') {
            n = (uint32_t)strtoul(end + 1, &end, 0);
        }
        printf("ttt: peek %08X:", (unsigned)a);
        for (k = 0; k < n && k < 64; k++) {
            uint32_t w = a + k * 4;
            printf(" %08X", w >= 0x00010000u && w < 0x02000000u ? (unsigned)*(volatile uint32_t *)Port_Ptr(w) : 0u);
        }
        printf("\n");
        p = *end == ',' ? end + 1 : end;
    }
}

static void report_and_exit(void) {
    uint32_t h = fnv((const uint8_t *)Port_Ptr(0x005F6B00u), 0x1000, 2166136261u);
    printf("ttt: %s: %u vblanks, %u frames\n", sHeadless ? "headless" : "real time", (unsigned)sVBlanks, (unsigned)gFrameCount);
    printf("ttt: modes: %s\n", sModes);
    printf("ttt: last mode %d sub %d\n", gMode, gSubMode);
    printf("ttt: state: seed 0x%x spin 0x%08x sbss-hash 0x%08x cycles %llu\n", (unsigned)sSeed,
           (unsigned)gRngSpin, h, (unsigned long long)sCycles);
    Gs_Report();
    if (sTimeT0 != 0) {
        uint64_t ms = (host_ns() - sTimeT0) / 1000000;
        printf("ttt: time: vblanks %u-%u in %llu ms\n", (unsigned)sTimeFrom, (unsigned)sVBlanks, (unsigned long long)ms);
    }
    Port_PadReport();
    Port_SndEnd();
    Port_StubReport();
    peek_report();
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

/* libpkt (plat_pkt.c) against the library's code (asm/B4320.s 0x404500-0x404AA0): the packet struct is pCurrent 0x0,
   pBase 0x4, pDmaTag 0x8, pVifCode 0xC, pGifTag 0x14; tags and codes get their counts ADDED (preset counts stay). */
void sceVif1PkInit(uint32_t *pk, void *base);
void sceVif1PkCnt(uint32_t *pk, uint32_t flag);
void sceVif1PkRef(uint32_t *pk, void *addr, uint32_t size, uint32_t opt1, uint32_t opt2, uint32_t flag);
uint32_t *sceVif1PkReserve(uint32_t *pk, uint32_t count);
void sceVif1PkOpenDirectCode(uint32_t *pk, int irq);
void sceVif1PkCloseDirectCode(uint32_t *pk);
void sceVif1PkOpenGifTag(uint32_t *pk, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3);
void sceVif1PkCloseGifTag(uint32_t *pk);
void sceVif1PkAddGsAD(uint32_t *pk, uint32_t addr, uint32_t lo, uint32_t hi);
uint32_t *sceVif1PkTerminate(uint32_t *pk);
uint32_t sceVif1PkSize(uint32_t *pk);
void sceGifPkInit(uint32_t *pk, void *base);
void sceGifPkEnd(uint32_t *pk, uint32_t opt1, uint32_t opt2, uint32_t flag);

static void pkt_self_test(void) {
    static uint32_t buf[64] __attribute__((aligned(16)));
    uint32_t pk[8], *g, *t;
    int i;

    for (i = 0; i < 8; i++) {
        pk[i] = 0xAAAAAAAAu;
    }
    sceVif1PkInit(pk, buf);
    expect(pk[0] == Port_Addr(buf) && pk[1] == Port_Addr(buf) && pk[2] == 0, "PkInit: pCurrent, pBase, pDmaTag = 0");
    expect(pk[3] == 0xAAAAAAAAu && pk[4] == 0xAAAAAAAAu && pk[5] == 0xAAAAAAAAu, "PkInit writes nothing past 0x8");
    /* Cnt: Terminate, then tag word 0 = flag | 0x10000000 (flag as given), word 1 = 0; pVifCode cleared */
    buf[2] = buf[3] = 0x55u;
    sceVif1PkCnt(pk, 0x80000000u);
    expect(buf[0] == 0x90000000u && buf[1] == 0 && pk[2] == Port_Addr(buf) && pk[3] == 0, "PkCnt tag and pointers");
    expect(pk[0] == Port_Addr(buf + 2) && buf[2] == 0x55u, "PkCnt leaves the tag's upper half to the VIF codes");
    /* OpenDirectCode: any non-zero irq sets bit 31; CloseDirectCode adds the whole quadwords after the code */
    sceVif1PkOpenDirectCode(pk, 2);
    expect(buf[2] == 0 && buf[3] == 0xD0000000u && pk[3] == Port_Addr(buf + 3), "PkOpenDirectCode at word 3, irq 2");
    /* OpenGifTag (pGifTag at 0x14); an IMAGE tag with NLOOP preset and closed at once keeps NLOOP */
    sceVif1PkOpenGifTag(pk, 0x00000005u, 0x08000000u, 0, 0);
    g = buf + 4;
    expect(pk[5] == Port_Addr(g) && pk[4] == 0xAAAAAAAAu, "PkOpenGifTag keeps the tag pointer at 0x14");
    sceVif1PkCloseGifTag(pk);
    expect(g[0] == 5u && pk[5] == 0, "PkCloseGifTag adds to a preset NLOOP (IMAGE, no data: + 0)");
    /* PACKED, NREG 1, NLOOP 0 + two A+D quadwords */
    sceVif1PkOpenGifTag(pk, 0, 0x10000000u, 0x0E, 0);
    g = (uint32_t *)Port_Ptr(pk[0]) - 4;
    sceVif1PkAddGsAD(pk, 0x3F, 1, 0);
    sceVif1PkAddGsAD(pk, 0x3F, 2, 0);
    sceVif1PkCloseGifTag(pk);
    expect(g[0] == 2u, "PkCloseGifTag PACKED NLOOP = quadwords / NREG");
    sceVif1PkReserve(pk, 1); /* one word: the code's count is rounded down */
    sceVif1PkCloseDirectCode(pk);
    expect(buf[3] == 0xD0000004u && pk[3] == 0, "PkCloseDirectCode adds whole quadwords (4, the odd word dropped)");
    /* Terminate pads with zero words, adds (end - tag) / 16 - 1 to the tag, clears pDmaTag, returns pCurrent */
    t = sceVif1PkTerminate(pk);
    expect(buf[0] == 0x90000005u && pk[2] == 0 && t == (uint32_t *)Port_Ptr(pk[0]) && t == buf + 24,
           "PkTerminate: QWC 5 added, pDmaTag cleared");
    /* Ref: tag words (flag | size | 0x30000000, addr & 0x9FFFFFFF, opt1, opt2), no open tag after it */
    sceVif1PkRef(pk, Port_Ptr(0x20123450u), 3, 0x11, 0x22, 0);
    expect(buf[24] == 0x30000003u && buf[25] == 0x00123450u && buf[26] == 0x11 && buf[27] == 0x22 && pk[2] == 0,
           "PkRef tag");
    sceVif1PkReserve(pk, 2);
    expect(sceVif1PkSize(pk) == 7, "PkSize = (pCurrent - pBase) / 16, rounded down");
    /* GIF packet End: (flag | 0x70000000, 0, opt1, opt2), pDmaTag = the tag */
    sceGifPkInit(pk, buf);
    sceGifPkEnd(pk, 0x33, 0x44, 0);
    expect(buf[0] == 0x70000000u && buf[1] == 0 && buf[2] == 0x33 && buf[3] == 0x44 && pk[2] == Port_Addr(buf) &&
               pk[0] == Port_Addr(buf + 4),
           "GifPkEnd tag");
}

/* libgraph (plat_lib.c): the values the library code writes that reach the GS. */
void sceGsSetDefDispEnv(void *p, int16_t psm, int16_t w, int16_t h, int16_t dx, int16_t dy);
int sceGsSetDefDBuff(void *p, int psm, int w, int h, int ztest, int zpsm, int clear);

static void graph_self_test(void) {
    static uint64_t db[0x300 / 8];

    sceGsSetDefDispEnv(db, 0, 640, 224, 0, 0);
    expect(db[0] == 0x66, "sceGsSetDefDispEnv PMODE 0x66 (0x3F6A84)");
    sceGsSetDefDBuff(db, 1, 640, 224, 3, 0x3A, 1);
    /* clear0 after disp[2][5] (0x50), giftag0 (0x10), draw0 (8 A+D): its rgbaq is the third A+D */
    expect(db[(0x50 + 0x10 + 0x80 + 0x20) / 8] == 0x3F80000000000000ull, "sceGsSetDefDBuff clear RGBAQ Q = 1.0");
}

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
        /* with TTE each tag quadword lands in the scratchpad in front of its data (func_00357640 reads the QWC of
           the fighter packet's tag back from there) */
        memset(Port_Ptr(0x70000400u), 0, 64);
        REG(0x1000D400u + TADR) = 0x00100200u;
        REG(0x1000D400u + SADR) = 0x400;
        REG(0x1000D400u) = 0x145;
        Port_HwPoll();
        expect(*(uint32_t *)Port_Ptr(0x70000400u) == 0x10000001u && *(uint32_t *)Port_Ptr(0x70000410u) == 0xAAAA5555u &&
                   *(uint32_t *)Port_Ptr(0x70000420u) == 0x70000001u && *(uint32_t *)Port_Ptr(0x70000430u) == 0x12345678u,
               "toSPR chain with TTE copies each tag before its data");
        expect(REG(0x1000D400u + SADR) == 0x440, "toSPR chain with TTE advances SADR over the tags");
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
    pkt_self_test();
    graph_self_test();
    if (sFails == 0) {
        printf("ttt: hw self-test OK\n");
    }
    exit(sFails != 0);
}

void Port_RpcSelfTest(void);   /* plat_sys.c */
void Port_GsSelfTest(void);    /* gs_core.c */
void Port_FloatSelfTest(void); /* plat_floattest.c */
int Port_Vu1SelfTest(void);    /* gs_vu1.c */

/* TTT_SELFTEST=hw | rpc | float | gs | gl | vu1 | ee | spu: run after the memory map (plat_mem.c, constructor 101), print and exit. */
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
    if (t != NULL && strcmp(t, "gs") == 0) {
        sHeadless = 1;
        Port_GsSelfTest();
    }
    if (t != NULL && strcmp(t, "ee") == 0) {
        int Port_EeSelfTest(void);
        exit(Port_EeSelfTest() != 0);
    }
    if (t != NULL && strcmp(t, "gl") == 0) {
        int Gl_SelfTest(void); /* gs_gl.c */
        exit(Gl_SelfTest() != 0);
    }
    if (t != NULL && strcmp(t, "spu") == 0) {
        int Spu2_SelfTest(void); /* spu2.c */
        exit(Spu2_SelfTest() != 0);
    }
    if (t != NULL && strcmp(t, "vu1") == 0) {
        int fails;
        sHeadless = 1;
        fails = Port_Vu1SelfTest();
        printf(fails ? "ttt: vu1 self-test FAILED (%d)\n" : "ttt: vu1 self-test OK\n", fails);
        exit(fails != 0);
    }
}
