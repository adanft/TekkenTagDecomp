/*
 * Sony SDK libraries of the game on PC that only need an answer for now: the GS set-up (libgraph), DMA / VU resets
 * (libdma, libvu0), pads (libpad, libmtap) and memory cards (libmc). Each reports "done / idle / nothing
 * connected". A function that is only a placeholder records itself (Port_Hit, plat_stub.c), so a headless run
 * lists what still waits for its real implementation: the GS path (T28), pads (T3x), memory cards, movies.
 */
#include <stdint.h>
#include <string.h>
#include "port_addr.h"

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

/* Placeholders for the GS set-up: the game's environment structures keep what the game itself wrote. */
int sceGsResetGraph(int mode, int inter, int omode, int ffmode) {
    (void)mode; (void)inter; (void)omode; (void)ffmode;
    PLACEHOLDER();
    return 0;
}
int sceGsSetDefDBuff(void *db, int psm, int w, int h, int ztest, int zpsm, int clear) {
    (void)db; (void)psm; (void)w; (void)h; (void)ztest; (void)zpsm; (void)clear;
    PLACEHOLDER();
    return 0;
}
void sceGsSetDefDispEnv(void *disp, int psm, int w, int h, int dx, int dy) {
    (void)disp; (void)psm; (void)w; (void)h; (void)dx; (void)dy;
    PLACEHOLDER();
}
int sceGsSwapDBuff(void *db, int id) { (void)db; (void)id; PLACEHOLDER(); return 0; }
void sceGsPutDispEnv(void *disp) { (void)disp; PLACEHOLDER(); }

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
