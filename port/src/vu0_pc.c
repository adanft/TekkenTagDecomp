/*
 * PC versions of the game routines written in EE / VU0 assembly (guarded by `#ifndef PORT` in src/, or drafts the PC
 * build leaves off: port/tools/portsrc.py REPLACED). Two are plain C already; the others report themselves until
 * they are written by hand on the VU0 register file of plat_ee.c (list: docs/port/README.md, "Hand replacements").
 */
#include <stdint.h>
#include <string.h>

extern void Port_Missing(const char *name); /* plat_stub.c */
extern float gPortVu0Vf[32][4];             /* plat_ee.c */

/* src/theater_c.c: m = identity / m = 0 (4 x 4 floats). */
void Mtx_Unit(float *m) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void Mtx_Zero(float *m) { memset(m, 0, 64); }

#define TODO(name) \
    void name(void) { Port_Missing(#name); }

/* src/geometry_b.c: entry one instruction before func_0034A184 (its first instruction, `mfc1 $t0, $f12`, is the delay
 * slot of func_0034A148's return). The callers in src/demo.c pass three arguments; the routine reads a fourth ($a2)
 * that they leave from earlier code: the PC version must find out which value that is. */
TODO(func_0034A180)
/* src/dma_gif.c: VU0 macro-mode skinning loops (vcallms into the VU0 microcode, jumps into their own bodies). */
TODO(func_003435C0)
TODO(func_003436B8)
TODO(func_003437B8)
TODO(func_003438B8)
/* src/tekken_bowl.c: VU0 sin / cos and the matrix rotations around it (results left in vf4-vf8 for the caller). */
TODO(func_003E8FA0)
TODO(func_003E9038)
TODO(func_003E9120)
TODO(func_003E91D0)
TODO(func_003E92C0)
/* src/tekken_bowl.c: m2c drafts around that VU0 code (portsrc.REPLACED). */
TODO(func_003EA978)
TODO(func_003EAF78)
TODO(func_003EBC98)
TODO(func_003EC388)
TODO(func_003ECAD8)
TODO(func_003ED338)
TODO(func_003EEF00)
/* src/model_anim_b.c: the inlined VU0 rotation of the draft RotMtxXYZ (x, y, z through func_003E8FA0). */
TODO(Port_Vu0RotMtxXYZ)
