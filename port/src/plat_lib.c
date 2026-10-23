/*
 * Sony SDK libraries of the game on PC: the GS set-up (libgraph), DMA / VU resets (libdma, libvu0), and those that
 * only need an answer for now: pads (libpad, libmtap) and memory cards (libmc), which report "nothing connected".
 * A function that is only a placeholder records itself (Port_Hit, plat_stub.c), so a headless run lists what still
 * waits for its real implementation: pads (T3x), memory cards, movies.
 */
#include <stdint.h>
#include <string.h>
#include "port_addr.h"
#include "gs.h"

void Port_Hit(const char *name);
int Port_WaitVBlank(void); /* plat_hw.c */
#define PLACEHOLDER() Port_Hit(__func__)

/* ---- libgraph ---- */
/* Waits for the next VBlank start; returns the field (0 even, 1 odd). */
int sceGsSyncV(int mode) { (void)mode; return Port_WaitVBlank(); }
/* PATH1-3 idle: the DMA is carried out when started (plat_hw.c). */
int sceGsSyncPath(int mode, uint16_t timeout) { (void)mode; (void)timeout; return 0; }
void sceGsResetPath(void) {}
void sceDmaReset(int mode) { (void)mode; }
void sceVpu0Reset(void) {}

/* The GS set-up of libgraph, after the SDK's documented behaviour; T29 b4 checked it against the library code
   (asm/B4320.s) for the values that reach the GS: PMODE 0x66 and the clear's RGBAQ Q = 1.0 now follow it. Still
   different, with the same GS result for the game's calls: SetDefDBuff gives the second buffer's FBP to draw1 here
   (the library: draw0 and disp[1]) and SwapDBuff pairs the ids the other way round; the page count ignores the
   16-bit / frame-mode rules; ResetGraph also resets VIF1 (the library: sceGsResetPath). The environments are built in the game's structures and written to the GS privileged registers (PMODE,
   SMODE2, DISPFB2, DISPLAY2, BGCOLOR); the drawing environment and its clear go to the GS as a GIF packet. The
   game writes DISPFB1 / DISPLAY1 itself (Gfx_ShowBuffer). */
static int sInter, sOmode, sFfmode;
void sceGsSetDefDispEnv(void *p, int16_t psm, int16_t w, int16_t h, int16_t dx, int16_t dy);

#define PRIV(off) (*(volatile uint64_t *)Port_Ptr(0x12000000u + (off)))

int sceGsResetGraph(int mode, int inter, int omode, int ffmode) {
    (void)mode;
    sInter = inter & 1;
    sOmode = omode;
    sFfmode = ffmode & 1;
    Gs_Reset();
    Gs_Vif1Reset();
    PRIV(GS_PMODE) = 0;
    PRIV(GS_SMODE2) = (uint64_t)sInter | (uint64_t)sFfmode << 1;
    PRIV(GS_CSR) &= 0x2000u; /* (FIELD stays: the VBlank keeps it) */
    return 0;
}

typedef struct { uint64_t v, addr; } AdReg; /* one A+D register write */
typedef struct { AdReg frame, zbuf, xyoffset, scissor, prmodecont, colclamp, dthe, test; } DrawEnv;
typedef struct { AdReg testa, prim, rgbaq, xyz2a, xyz2b, testb; } Clear;
typedef struct {
    uint64_t disp[2][5];
    uint64_t giftag0[2];
    DrawEnv draw0;
    Clear clear0;
    uint64_t giftag1[2];
    DrawEnv draw1;
    Clear clear1;
} DBuff; /* sceGsDBuff (include/gfx.h GsDBuff) */

static void ad(AdReg *r, uint64_t v, uint64_t addr) {
    r->v = v;
    r->addr = addr;
}

static void draw_env(uint64_t *tag, DrawEnv *d, Clear *c, uint32_t fbp, uint32_t zbp, int psm, int w, int h, int ztest, int zpsm, int clear) {
    uint64_t test = ztest ? 1ull << 16 | (uint64_t)(ztest & 3) << 17 : 0;
    tag[0] = (uint64_t)(clear ? 14 : 8) | 1ull << 15 | 1ull << 60; /* NLOOP, EOP, PACKED, one register: A+D */
    tag[1] = 0xE;
    ad(&d->frame, fbp | (uint64_t)((w + 63) / 64) << 16 | (uint64_t)psm << 24, 0x4C);
    ad(&d->zbuf, zbp | (uint64_t)(zpsm & 15) << 24 | (uint64_t)(ztest == 0) << 32, 0x4E);
    ad(&d->xyoffset, (uint64_t)((2048 - w / 2) << 4) | (uint64_t)((2048 - h / 2) << 4) << 32, 0x18);
    ad(&d->scissor, (uint64_t)(w - 1) << 16 | (uint64_t)(h - 1) << 48, 0x40);
    ad(&d->prmodecont, 1, 0x1A);
    ad(&d->colclamp, 1, 0x46);
    ad(&d->dthe, (psm & 2) ? 1 : 0, 0x45);
    ad(&d->test, test, 0x47);
    ad(&c->testa, 1ull << 16 | 1ull << 17, 0x47); /* depth test ALWAYS: the clear writes every pixel's depth */
    ad(&c->prim, 6, 0x00);
    ad(&c->rgbaq, 0x3F80000000000000ull, 0x01); /* Q = 1.0 (ori 0xFE00; dsll32 14) */
    ad(&c->xyz2a, (uint64_t)((2048 - w / 2) << 4) | (uint64_t)((2048 - h / 2) << 4) << 16, 0x05);
    ad(&c->xyz2b, (uint64_t)((2048 + w / 2) << 4) | (uint64_t)((2048 + h / 2) << 4) << 16, 0x05);
    ad(&c->testb, test, 0x47);
}

int sceGsSetDefDBuff(void *p, int psm, int w, int h, int ztest, int zpsm, int clear) {
    DBuff *db = p;
    uint32_t pages = (uint32_t)((w + 63) / 64) * (uint32_t)((h + 31) / 32); /* one buffer, in 8 KB pages */
    memset(db, 0, sizeof *db);
    draw_env(db->giftag0, &db->draw0, &db->clear0, 0, pages * 2, psm, w, h, ztest, zpsm, clear);
    draw_env(db->giftag1, &db->draw1, &db->clear1, pages, pages * 2, psm, w, h, ztest, zpsm, clear);
    sceGsSetDefDispEnv(&db->disp[0], (int16_t)psm, (int16_t)w, (int16_t)h, 0, 0);
    sceGsSetDefDispEnv(&db->disp[1], (int16_t)psm, (int16_t)w, (int16_t)h, 0, 0);
    db->disp[1][2] |= pages; /* DISPFB.FBP of the second buffer */
    return 0;
}

/* PMODE: circuit 2 (0x66); SMODE2 of the reset; DISPFB 0, width, psm; DISPLAY centred on an NTSC / PAL screen
   with MAGH filling 2560 video clocks; interlaced frame mode shows h buffer lines on 2h raster lines. */
void sceGsSetDefDispEnv(void *p, int16_t psm, int16_t w, int16_t h, int16_t dx, int16_t dy) {
    uint64_t *d = p;
    uint32_t magh = w > 0 ? (uint32_t)(2560 / w) : 4, lines = (uint32_t)h * (sInter && sFfmode ? 2u : 1u);
    uint32_t x0 = sOmode == 3 ? 656 : 636, y0 = sOmode == 3 ? 72 : 50;
    if (magh == 0) {
        magh = 1;
    }
    d[0] = 0x66; /* EN2, CRTMD, MMOD, AMOD, ALP 0 (0x3F6A84) */
    d[1] = (uint64_t)sInter | (uint64_t)sFfmode << 1;
    d[2] = (uint64_t)((w + 63) / 64) << 9 | (uint64_t)(psm & 0x1F) << 15;
    d[3] = (uint64_t)(x0 + (uint32_t)dx * magh) | (uint64_t)(y0 + (uint32_t)dy) << 12 | (uint64_t)(magh - 1) << 23 |
           (uint64_t)((uint32_t)w * magh - 1) << 32 | (uint64_t)(lines - 1) << 44;
    d[4] = 0;
}

void sceGsPutDispEnv(void *p) {
    const uint64_t *d = p;
    PRIV(GS_PMODE) = d[0];
    PRIV(GS_SMODE2) = d[1];
    PRIV(GS_DISPFB2) = d[2];
    PRIV(GS_DISPLAY2) = d[3];
    PRIV(GS_BGCOLOR) = d[4];
}

int sceGsPutDrawEnv(uint64_t *giftag) {
    Gs_Gif(3, (const uint8_t *)giftag, 1 + (uint32_t)(giftag[0] & 0x7FFF));
    return 0;
}

/* Shows buffer `id` and draws into the other one [I: the pairing, after the SDK's description]. */
int sceGsSwapDBuff(void *p, int id) {
    DBuff *db = p;
    sceGsPutDispEnv(db->disp[id & 1]);
    return sceGsPutDrawEnv((id & 1) ? db->giftag0 : db->giftag1);
}

/* ---- libmtap / libpad: no multitap, no pad connected (the real input is a later task) ---- */
int sceMtapInit(void) { return 1; }
int sceMtapPortOpen(int port) { (void)port; return 1; }
int sceMtapPortClose(int port) { (void)port; return 1; }
int sceMtapGetConnection(int port) { (void)port; return 0; }
int scePadInit(int mode) { (void)mode; return 1; }
int scePadEnd(void) { return 1; }
int scePadPortOpen(int port, int slot, void *addr) { (void)port; (void)slot; (void)addr; return 1; }
int scePadPortClose(int port, int slot) { (void)port; (void)slot; return 1; }
int scePadGetState(int port, int slot) { (void)port; (void)slot; PLACEHOLDER(); return 0; } /* disconnected */
int scePadGetReqState(int port, int slot) { (void)port; (void)slot; return 0; } /* complete */
int scePadRead(int port, int slot, uint8_t *rdata) { (void)port; (void)slot; (void)rdata; return 0; }
int scePadInfoMode(int port, int slot, int term, int offs) { (void)port; (void)slot; (void)term; (void)offs; return 0; }
int scePadInfoAct(int port, int slot, int actno, int term) { (void)port; (void)slot; (void)actno; (void)term; return 0; }
int scePadInfoPressMode(int port, int slot) { (void)port; (void)slot; return 0; }
int scePadEnterPressMode(int port, int slot) { (void)port; (void)slot; return 0; }
int scePadSetMainMode(int port, int slot, int offs, int lock) { (void)port; (void)slot; (void)offs; (void)lock; return 0; }
int scePadSetActAlign(int port, int slot, const uint8_t *data) { (void)port; (void)slot; (void)data; return 0; }
int scePadSetActDirect(int port, int slot, const uint8_t *data) { (void)port; (void)slot; (void)data; return 0; }

/* ---- libmc: no memory card in either slot. Each call is accepted and finishes at the next sceMcSync with a
   "no card" result. ---- */
#define MC_NO_CARD (-10)
static int sMcCmd = -1, sMcBusy;
static int mc_start(int cmd) {
    sMcCmd = cmd;
    sMcBusy = 1;
    return 0;
}
int sceMcInit(void) { return 0; }
int sceMcSync(int mode, int *cmd, int *result) {
    (void)mode;
    if (!sMcBusy) {
        return -1; /* nothing in progress */
    }
    sMcBusy = 0;
    if (cmd != NULL) {
        *cmd = sMcCmd;
    }
    if (result != NULL) {
        *result = MC_NO_CARD;
    }
    Port_Hit("libmc (no card)");
    return 1;
}
/* libmc's numbers of the calls: getinfo 1, open 2, close 3, seek 4, read 5, write 6, mkdir 0xB, chdir 0xC,
   getdir 0xD, format 0x10, delete 0xF, setfileinfo 0xE, unformat 0x11 (the cmd sceMcSync hands back). */
int func_00406D08(int port, int slot, int *type, int *free, int *format) { /* sceMcGetInfo */
    (void)port; (void)slot;
    if (type != NULL) { *type = 0; }
    if (free != NULL) { *free = 0; }
    if (format != NULL) { *format = 0; }
    return mc_start(1);
}
int sceMcOpen(int port, int slot, int name, int mode) { (void)port; (void)slot; (void)name; (void)mode; return mc_start(2); }
int sceMcClose(int fd) { (void)fd; return mc_start(3); }
int func_004069B0(int fd, int buf, int size) { (void)fd; (void)buf; (void)size; return mc_start(5); } /* read */
int func_00406A88(int fd, int buf, int size) { (void)fd; (void)buf; (void)size; return mc_start(6); } /* write */
int sceMcMkdir(int port, int slot, int name) { (void)port; (void)slot; (void)name; return mc_start(0xB); }
int func_00407138(int port, int slot, int name) { (void)port; (void)slot; (void)name; return mc_start(0xF); } /* delete */
int func_00406E50(int port, int slot, int a, int b, int c, int d) { (void)port; (void)slot; (void)a; (void)b; (void)c; (void)d; return mc_start(0xD); } /* getdir */
int func_00406FC0(int port, int slot, int name, int x) { (void)port; (void)slot; (void)name; (void)x; return mc_start(0xE); } /* setfileinfo */
int func_004070A8(int port, int slot) { (void)port; (void)slot; return mc_start(0x10); } /* format */
int func_004074F8(int port, int slot) { (void)port; (void)slot; return mc_start(0x11); } /* unformat */

/* ---- movies: not played yet (libmpeg, the IPU, the decoder threads come with the movie task). The game's
   movie function (movie.c func_0039C878) asks first and skips the movie as when the stream does not go idle. */
int Port_MoviePlayable(void) {
    Port_Hit("movie (skipped)");
    return 0;
}

/* ---- libkernel ---- */
/* func_00401F58(buf, size): a data cache write-back / invalidate of a range (file.c, before a SIF transfer).
   There is no cache to keep coherent here. */
void func_00401F58(void *buf, int size) { (void)buf; (void)size; }
