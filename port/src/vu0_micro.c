/*
 * VU0 micro mode: the VIF0 side that loads the game's VU0 microcode (MPG) and the program start the EE makes with
 * vcallms (port/src/vu0_pc.c). The programs run on the micro-mode interpreter shared with VU1 (vu_micro.c); the
 * game's four skinning programs (src/vu0/skin_*.vsm) run unchanged on it.
 *
 * VU0 memory is the EE's view of it (plat_mem.c): micro memory at 0x11000000 (4 KB), data memory at 0x11004000
 * (4 KB). The registers are shared with macro mode: gPortVu0Vf (plat_ee.c), gPortVu0Vi, gPortVu0Acc, gPortVu0Q.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define memcpy __builtin_memcpy /* -fno-builtin: inline the small copies of the hot paths */
#include "vu_micro.h"

extern float gPortVu0Vf[32][4]; /* plat_ee.c */
uint16_t gPortVu0Vi[16];         /* vi0..vi15 (ctc2 / cfc2 in macro mode) */
uint32_t gPortVu0Acc[4];         /* ACC, shared with macro mode (vu0_pc.c) */
uint32_t gPortVu0Q = 0x3F800000u; /* Q */

#define MICRO ((uint8_t *)0x11000000u)
#define DATA ((uint8_t *)0x11004000u)

/* ---------------------------------------------------------------------------------------------- VIF0 */

static struct {
    uint32_t pending;  /* data words still owed to the current code */
    uint32_t kind;     /* 0 none, 1 MPG, 2 skipped data */
    uint32_t mpgAddr;  /* byte address in micro memory of the next MPG word */
    uint32_t mask;
} vif;

/* The VIF0 codes and data of a DMA block, in order (plat_hw.c): STCYCL / STMOD / STMASK / MARK / flushes are
   accepted, MPG loads micro memory; anything with data the port does not use yet (STROW, STCOL, UNPACK) has its
   data skipped and is reported once. */
void Port_Vif0Feed(const uint32_t *w, uint32_t n) {
    static int reported;
    uint32_t k;

    for (k = 0; k < n; k++) {
        uint32_t code, cmd, num, imm;

        if (vif.pending) {
            if (vif.kind == 1) {
                memcpy(MICRO + (vif.mpgAddr & 0xFFC), &w[k], 4);
                vif.mpgAddr += 4;
            }
            vif.pending--;
            continue;
        }
        code = w[k];
        cmd = (code >> 24) & 0x7F;
        num = (code >> 16) & 0xFF;
        imm = code & 0xFFFF;
        if (cmd == 0x4A) { /* MPG: num instructions (0 = 256) at imm * 8 */
            vif.kind = 1;
            vif.mpgAddr = (imm * 8) & 0xFFF;
            vif.pending = (num ? num : 256) * 2;
        } else if (cmd == 0x20) { /* STMASK: one word */
            vif.kind = 2;
            vif.pending = 1;
        } else if (cmd == 0x30 || cmd == 0x31) { /* STROW / STCOL: four words */
            vif.kind = 2;
            vif.pending = 4;
        } else if (cmd >= 0x60) { /* UNPACK: not used by the game on VIF0 so far */
            uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, cnt = num ? num : 256;
            uint32_t bits = (32u >> vl) * (vn + 1);
            vif.kind = 2;
            vif.pending = (cnt * bits + 31) / 32;
            if (!reported++) {
                fprintf(stderr, "ttt: vif0: UNPACK 0x%08x skipped (not implemented)\n", code);
            }
        } else if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) { /* MSCAL / MSCALF / MSCNT */
            if (!reported++) {
                fprintf(stderr, "ttt: vif0: program start 0x%08x ignored (not implemented)\n", code);
            }
        }
        /* NOP, STCYCL, OFFSET, BASE, ITOP, STMOD, MSKPATH3, MARK, FLUSH*: nothing to do here */
    }
}

/* ---------------------------------------------------------------------------------------------- program start */

/* Runs the program at micro-memory byte address `addr` until its end (E bit and the instruction after it). Results
   still in flight at the end stay in flight into the next call (the EE starts the next vertex at once). */
void Port_Vu0CallMs(uint32_t addr) {
    static VuCore vu;
    static int reported;

    if (vu.vf == NULL) {
        vu.vf = (uint32_t (*)[4])gPortVu0Vf;
        vu.vi = gPortVu0Vi;
        vu.acc = gPortVu0Acc;
        vu.q = &gPortVu0Q;
        vu.micro = MICRO;
        vu.data = DATA;
        vu.microMask = 0x1FF;
        vu.dataMask = 0xFF;
    }
    if (Vu_Run(&vu, addr / 8) != 0 && reported++ < 2) {
        fprintf(stderr, "ttt: vu0: program 0x%03X did not end\n", addr);
    }
    if (Vu_Unknown(&vu) != 0 && reported++ == 0) {
        fprintf(stderr, "ttt: vu0: %u instructions not implemented (program 0x%03X)\n", Vu_Unknown(&vu), addr);
    }
}
