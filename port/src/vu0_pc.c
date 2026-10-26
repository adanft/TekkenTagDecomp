/*
 * PC versions of the game routines written in EE / VU0 assembly (guarded by `#ifndef PORT` in src/, or drafts the PC
 * build leaves off: port/tools/portsrc.py REPLACED), written by hand on the VU0 register file of plat_ee.c (list:
 * docs/port/README.md, "Hand replacements"); the FPU part of the Tekken Bowl drafts is in tekken_bowl_pc.c.
 */
#include <stdint.h>
#include <string.h>
#include "softfloat_ps2_inl.h"
#include "port_addr.h"

extern float gPortVu0Vf[32][4];             /* plat_ee.c */

/* src/theater_c.c: m = identity / m = 0 (4 x 4 floats). */
void Mtx_Unit(float *m) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void Mtx_Zero(float *m) { memset(m, 0, 64); }

/* src/geometry_b.c func_0034A180 (0x34A180, one instruction before func_0034A184, its first instruction `mfc1 $t0,
 * $f12` being the delay slot of func_0034A148's return): d = a * k + b * (1 - k) for the three rotation rows, VU0
 * macro code:
 *   qmtc2 $t0, $vf3; vsubx.w $vf3, $vf0, $vf3x          vf3 = (k, ., ., 1 - k)
 *   vmulax.xyzw ACC, a[i], $vf3x; vmaddw.xyzw d[i], b[i], $vf3w     (i = 0..2, all six rows loaded first)
 * The third argument ($a2) is the blend partner; src/demo.c's callers name it under PORT (the PS2 code leaves it
 * in $a2 from the caller: the destination itself, or the previous frame's matrix). Vector-unit arithmetic
 * (this file's float operations are renamed to Port_Vu*: port/tools/toolchain.py VU_FILES). Leaves vf3..vf9 as the
 * PS2 does. */
void func_0034A180(float d[4][4], float a[4][4], float b[4][4], float k) {
    float w = 1.0f - k;
    float r[3][4];
    int i, j;

    for (i = 0; i < 3; i++) {
        memcpy(gPortVu0Vf[4 + i], a[i], 16);
        memcpy(gPortVu0Vf[7 + i], b[i], 16);
    }
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            r[i][j] = gPortVu0Vf[4 + i][j] * k + gPortVu0Vf[7 + i][j] * w;
        }
    }
    gPortVu0Vf[3][0] = k;
    gPortVu0Vf[3][3] = w;
    for (i = 0; i < 3; i++) {
        memcpy(gPortVu0Vf[4 + i], r[i], 16);
        memcpy(d[i], r[i], 16);
    }
}

/* src/dma_gif.c func_003435C0 / func_003436B8 / func_003437B8 / func_003438B8: the EE side of the skinning loop. Per
 * vertex record (0x40 bytes, plus 0x20 for each further bone: words 4..7 are the bone matrix addresses in VU0 memory,
 * 0 ending the list) it loads the vertex into vf7 / vf2 / vf26 and each further bone's pair into vf3 / vf27,
 * vf4 / vf28, vf5 / vf29, the addresses into vi2..vi5, starts the VU0 program (vcallms: skin_diffuse 0x000,
 * skin_specular 0x250, skin_gloss 0x750, skin_envmap 0xC30, src/vu0/) and writes three quadwords to *dst: the
 * record's first quadword (envmap: vf24), vf20, vf22. A bone address of 0 leaves the higher vi registers as they
 * were (the program stops at the first zero). Returns the record after the last; *dst advances. The two lighting
 * variants take a 64-bit value `a` into vf6 (x = low word, y = high word; the 128-bit register's upper half is not
 * an argument and is taken as zero) and vi6. */
void Port_Vu0CallMs(uint32_t addr);   /* vu0_micro.c */
extern uint16_t gPortVu0Vi[16];       /* vu0_micro.c */
/* vu_fast.c: the loop below (with the C programs' float mode set around it, 16-byte register copies) */
uint32_t *Vu0_Skin(uint32_t (*vf)[4], uint16_t *vi, uint32_t **dst, uint32_t *src, int32_t n, uint32_t prog, int envmap,
                   void (*run)(uint32_t addr));
#define VFW(r) ((uint32_t *)gPortVu0Vf[(r)])

static uint32_t *skin(uint32_t **dst, uint32_t *src, int32_t n, uint32_t prog, int envmap) {
    return Vu0_Skin((uint32_t (*)[4])gPortVu0Vf, gPortVu0Vi, dst, src, n, prog, envmap, Port_Vu0CallMs);
}

static void set_vf6(int64_t a) {
    uint32_t *vf6 = VFW(6);

    vf6[0] = (uint32_t)a;
    vf6[1] = (uint32_t)((uint64_t)a >> 32);
    vf6[2] = vf6[3] = 0;
    gPortVu0Vi[6] = (uint16_t)a;
}

uint32_t *func_003435C0(uint32_t **dst, uint32_t *src, int32_t n, void *hdr) {
    (void)hdr;
    return skin(dst, src, n, 0x000, 0);
}

uint32_t *func_003436B8(uint32_t **dst, uint32_t *src, int32_t n, int64_t a) {
    set_vf6(a);
    return skin(dst, src, n, 0x250, 0);
}

uint32_t *func_003437B8(uint32_t **dst, uint32_t *src, int32_t n, int64_t a) {
    set_vf6(a);
    return skin(dst, src, n, 0x750, 0);
}

uint32_t *func_003438B8(uint32_t **dst, uint32_t *src, int32_t n, void *hdr) {
    (void)hdr;
    return skin(dst, src, n, 0xC30, 1);
}

/* ---- VU0 macro mode on the shared register file (bit patterns; the vector unit's arithmetic) ---- */
extern uint32_t gPortVu0Acc[4]; /* vu0_micro.c */
extern uint32_t gPortVu0Q;
enum { MX = 8, MY = 4, MZ = 2, MW = 1 }; /* field masks, x first as in the instruction */
#define ON(m, c) (((m) >> (3 - (c))) & 1)
#define ALL (MX | MY | MZ | MW)

/* fd.m = fs.m + ft.bc / - ft.bc / * ft.bc;  (sub = 1, 2: mul) */
static void v_bc(int kind, int fd, int fs, int ft, int bc, unsigned m) {
    uint32_t r[4], b = VFW(ft)[bc];
    int c;

    for (c = 0; c < 4; c++) {
        uint32_t a = VFW(fs)[c];
        r[c] = kind == 2 ? sf_mul_core(a, b, 0) : sf_add_core(a, kind ? b ^ SF_SIGN : b, 0);
    }
    for (c = 0; c < 4; c++) {
        if (ON(m, c) && fd != 0) {
            VFW(fd)[c] = r[c];
        }
    }
}
/* ACC.m = fs.m * ft.bc (vmulabc) or fs.m + ft.bc (vaddabc) */
static void v_acc_bc(int add, int fs, int ft, int bc, unsigned m) {
    uint32_t b = VFW(ft)[bc];
    int c;

    for (c = 0; c < 4; c++) {
        if (ON(m, c)) {
            gPortVu0Acc[c] = add ? sf_add_core(VFW(fs)[c], b, 0) : sf_mul_core(VFW(fs)[c], b, 0);
        }
    }
}
/* fd.m = ACC.m +/- fs.m * ft.bc (vmaddbc / vmsubbc) */
static void v_madd_bc(int sub, int fd, int fs, int ft, int bc, unsigned m) {
    uint32_t r[4], b = VFW(ft)[bc];
    int c;

    for (c = 0; c < 4; c++) {
        uint32_t p = sf_mul_core(VFW(fs)[c], b, 0);
        r[c] = sf_add_core(gPortVu0Acc[c], sub ? p ^ SF_SIGN : p, 0);
    }
    for (c = 0; c < 4; c++) {
        if (ON(m, c) && fd != 0) {
            VFW(fd)[c] = r[c];
        }
    }
}
/* qmtc2 of a word moved with mfc1 / cfc2: x = the word, y = its sign extension; the register's upper half is
   not known here and is taken as zero. */
static void v_qmtc2_word(int fd, uint32_t w) {
    VFW(fd)[0] = w;
    VFW(fd)[1] = (w & SF_SIGN) ? 0xFFFFFFFFu : 0;
    VFW(fd)[2] = VFW(fd)[3] = 0;
}

/* src/tekken_bowl.c func_003E8FA0 (0x3E8FA0): VU0 sincos of x, the libvu0 _sceVu0ecossin series on the table
 * D_0046BB60: vf8.y = the series of x' (x' = pi/2 - x, or x + pi/2 for x < 0), vf8.x = +/-sqrt(1 - vf8.y^2)
 * (negative for x < 0); vf9 / vf10 / ACC / Q are left as the PS2 leaves them. The two adjustments of x are FPU
 * code (add.s / sub.s), the rest VU0 macro code. */
extern uint32_t D_0046BB60[4];

void func_003E8FA0(uint32_t x) {
    const uint32_t h = 0x3FC90FDBu; /* 1.5707964f */
    int neg;

    if (sf_key(x) < 0) {
        neg = 1;
        x = sf_fpu_add(x, h);
    } else {
        x = sf_fpu_add(h, x ^ SF_SIGN);
        neg = 0;
    }
    v_qmtc2_word(10, x);
    memcpy(VFW(9), D_0046BB60, 16);
    VFW(10)[3] = VFW(10)[0];                    /* vmr32.w vf10, vf10 */
    v_bc(0, 8, 0, 10, 0, MY);                   /* vaddx.y vf8, vf0, vf10x */
    VFW(10)[0] = sf_mul_core(VFW(10)[0], VFW(10)[0], 0); /* vmul.x vf10, vf10, vf10 */
    v_bc(2, 9, 9, 10, 3, MX | MY | MZ | MW);    /* vmulw.xyzw vf9, vf9, vf10w */
    v_bc(2, 9, 9, 10, 0, MX | MY | MZ | MW);    /* vmulx.xyzw vf9, vf9, vf10x */
    v_bc(2, 9, 9, 10, 0, MX | MY | MZ);         /* vmulx.xyz */
    v_bc(0, 8, 8, 9, 3, MY);                    /* vaddw.y vf8, vf8, vf9w */
    v_bc(2, 9, 9, 10, 0, MX | MY);              /* vmulx.xy */
    v_bc(0, 8, 8, 9, 2, MY);                    /* vaddz.y vf8, vf8, vf9z */
    v_bc(2, 9, 9, 10, 0, MX);                   /* vmulx.x */
    v_bc(0, 8, 8, 9, 1, MY);                    /* vaddy.y vf8, vf8, vf9y */
    v_bc(0, 8, 8, 9, 0, MY);                    /* vaddx.y vf8, vf8, vf9x */
    v_acc_bc(1, 0, 0, 3, MY);                   /* vaddaw.y ACC, vf0, vf0w */
    v_madd_bc(1, 9, 8, 8, 1, MY);               /* vmsuby.y vf9, vf8, vf8y */
    gPortVu0Q = sf_sqrt_core(VFW(9)[1], 0);     /* vsqrt Q, vf9y; vwaitq */
    v_qmtc2_word(9, gPortVu0Q);                 /* cfc2 $t0, $vi22 (Q); qmtc2 $t0, vf9 */
    v_bc(neg, 8, 0, 9, 0, MX);                  /* vsubx.x / vaddx.x vf8, vf0, vf9x */
}

static void load4(int first, const uint32_t *m) { memcpy(VFW(first), m, 64); }
static void store4(uint32_t *d, int first) { memcpy(d, VFW(first), 64); }

/* The three rotation steps of the bowl code on vf4..vf7 after func_003E8FA0 (vf8 = (c, s) as it leaves them). */
static void rot_z(void) { /* xy */
    int r;
    v_bc(0, 9, 0, 8, 0, MY);  /* vaddx.y vf9, vf0, vf8x */
    v_bc(0, 9, 0, 8, 1, MX);  /* vaddy.x vf9, vf0, vf8y */
    v_bc(1, 8, 0, 8, 0, MX);  /* vsubx.x vf8, vf0, vf8x */
    for (r = 4; r < 8; r++) {
        v_acc_bc(0, 9, r, 0, MX | MY);    /* vmulax.xy ACC, vf9, vfRx */
        v_madd_bc(0, r, 8, r, 1, MX | MY); /* vmaddy.xy vfR, vf8, vfRy */
    }
}
static void rot_x(void) { /* yz */
    int r;
    v_bc(0, 8, 0, 8, 0, MZ);  /* vaddx.z vf8, vf0, vf8x */
    v_bc(1, 9, 0, 8, 0, MY);  /* vsubx.y vf9, vf0, vf8x */
    v_bc(0, 9, 0, 8, 1, MZ);  /* vaddy.z vf9, vf0, vf8y */
    for (r = 4; r < 8; r++) {
        v_acc_bc(0, 8, r, 1, MY | MZ);    /* vmulay.yz ACC, vf8, vfRy */
        v_madd_bc(0, r, 9, r, 2, MY | MZ); /* vmaddz.yz vfR, vf9, vfRz */
    }
}
static void rot_y(void) { /* xz */
    int r;
    v_bc(0, 8, 0, 8, 1, MZ);  /* vaddy.z vf8, vf0, vf8y */
    v_bc(0, 9, 0, 8, 1, MX);  /* vaddy.x vf9, vf0, vf8y */
    v_bc(1, 9, 0, 8, 0, MZ);  /* vsubx.z vf9, vf0, vf8x */
    for (r = 4; r < 8; r++) {
        v_acc_bc(0, 8, r, 2, MX | MZ);    /* vmulaz.xz ACC, vf8, vfRz */
        v_madd_bc(0, r, 9, r, 0, MX | MZ); /* vmaddx.xz vfR, vf9, vfRx */
    }
}

/* func_003E9038: d = m rotated about z, x then y by rot. */
void func_003E9038(uint32_t *d, const uint32_t *m, const uint32_t *rot) {
    load4(4, m);
    func_003E8FA0(rot[2]);
    rot_z();
    func_003E8FA0(rot[0]);
    rot_x();
    func_003E8FA0(rot[1]);
    rot_y();
    store4(d, 4);
}

/* func_003E9120: m rotated about x then y by rot, pos added to the translation; the result stays in vf4..vf7 (the
 * caller's inline code reads it). */
void func_003E9120(const uint32_t *m, const uint32_t *rot, const uint32_t *pos) {
    load4(4, m);
    func_003E8FA0(rot[0]);
    rot_x();
    func_003E8FA0(rot[1]);
    rot_y();
    memcpy(VFW(12), pos, 16);
    {
        int c;
        for (c = 0; c < 3; c++) {
            VFW(7)[c] = sf_add_core(VFW(7)[c], VFW(12)[c], 0); /* vadd.xyz vf7, vf7, vf12 */
        }
    }
}

/* func_003E91D0: d = m rotated about y by -rot[3], x by rot[0], y by -rot[1] (the negations are FPU neg.s). */
void func_003E91D0(uint32_t *d, const uint32_t *m, const uint32_t *rot) {
    load4(4, m);
    func_003E8FA0(rot[3] ^ SF_SIGN);
    rot_y();
    func_003E8FA0(rot[0]);
    rot_x();
    func_003E8FA0(rot[1] ^ SF_SIGN);
    rot_y();
    store4(d, 4);
}

/* func_003E92C0: d = a + b * t (vaddax ACC = a + 0; vmaddx vf15 = ACC + b * t). */
void func_003E92C0(uint32_t *d, const uint32_t *a, const uint32_t *b, uint32_t t) {
    memcpy(VFW(12), a, 16);
    v_qmtc2_word(16, t);
    memcpy(VFW(14), b, 16);
    v_acc_bc(1, 12, 0, 0, MX | MY | MZ | MW);
    v_madd_bc(0, 15, 14, 16, 0, MX | MY | MZ | MW);
    memcpy(d, VFW(15), 16);
}

/* ---- src/tekken_bowl.c: the drafts around that VU0 code (portsrc.REPLACED) ----
 * The two that are VU0 code throughout are here; the other five are mostly FPU code (port/src/tekken_bowl_pc.c, built
 * with the FPU's arithmetic) and reach the vector unit through the Port_Vu0Bowl* steps below. */

/* ACC.m += fs.m * ft.bc (vmaddabc) */
static void v_madda_bc(int fs, int ft, int bc, unsigned m) {
    uint32_t b = VFW(ft)[bc];
    int c;

    for (c = 0; c < 4; c++) {
        if (ON(m, c)) {
            gPortVu0Acc[c] = sf_add_core(gPortVu0Acc[c], sf_mul_core(VFW(fs)[c], b, 0), 0);
        }
    }
}
/* vf13 = vf4..vf7 * vf12 (vmulax / vmadday / vmaddaz ACC; vmaddw vf13) */
static void xform13(void) {
    v_acc_bc(0, 4, 12, 0, ALL);
    v_madda_bc(5, 12, 1, ALL);
    v_madda_bc(6, 12, 2, ALL);
    v_madd_bc(0, 13, 7, 12, 3, ALL);
}
/* fd = fs + ft / fs - ft (vadd.xyzw / vsub.xyzw) */
static void v_add3(int sub, int fd, int fs, int ft) {
    uint32_t r[4];
    int c;

    for (c = 0; c < 4; c++) {
        r[c] = sf_add_core(VFW(fs)[c], sub ? VFW(ft)[c] ^ SF_SIGN : VFW(ft)[c], 0);
    }
    if (fd != 0) {
        memcpy(VFW(fd), r, 16);
    }
}
/* vftoiN of one field: cut toward zero after scaling by 2^frac; out of range saturates. */
static uint32_t v_ftoi(uint32_t a, int frac) {
    uint32_t m = SF_MANT(a), v;
    int x = (int)SF_EXP(a) - 127 + frac;

    if (SF_EXP(a) == 0 || x < 0) {
        return 0;
    }
    if (x >= 31) {
        return (a & SF_SIGN) ? 0x80000000u : 0x7FFFFFFFu;
    }
    v = x >= 23 ? m << (x - 23) : m >> (23 - x);
    return (a & SF_SIGN) ? 0u - v : v;
}

/* The steps tekken_bowl_pc.c takes on the VU0 registers, each a run of the original's instructions. */
void Port_Vu0BowlLoadMtx(const uint32_t *m) { load4(4, m); } /* lqc2 vf4..vf7, m */
void Port_Vu0BowlRotX(void) { rot_x(); }
void Port_Vu0BowlRotY(void) { rot_y(); }
void Port_Vu0BowlRotZ(void) { rot_z(); }
/* lqc2 vf12, v; vf13 = vf4..vf7 * vf12; sqc2 vf13, out */
void Port_Vu0BowlXform(uint32_t *out, const uint32_t *v) {
    memcpy(VFW(12), v, 16);
    xform13();
    memcpy(out, VFW(13), 16);
}
/* lqc2 vf14, b; vadd.xyzw vf15, vf13, vf14; sqc2 vf15, out */
void Port_Vu0BowlAdd13(uint32_t *out, const uint32_t *b) {
    memcpy(VFW(14), b, 16);
    v_add3(0, 15, 13, 14);
    memcpy(out, VFW(15), 16);
}

/* func_003EA978 (0x3EA978): the bowl's 7 x 3 marker points (D_0046BBB0[i] + D_0046BB80[j]) through the screen matrix
 * D_004334C0, divided by w (vdiv / vmulq) and turned into 12.4 fixed point (vftoi4); a marker whose three points all
 * fall inside (x <= 0xFFFF unsigned, 0 <= y <= 0xFFFF, 0 <= z <= 0xFFFFF) is added to the packet as D_0046BB70 and the
 * three points. With any marker the packet gets its GIF tag and register words in front. */
extern uint32_t D_004334C0[16];
extern uint32_t D_0046BB70[4];
extern uint32_t D_0046BB80[12];
extern uint32_t D_0046BBB0[28];
uint32_t func_003994C0(void); /* src/zsort.c */
void func_00399580(uint32_t p);

void func_003EA978(void) {
    uint32_t t3 = func_003994C0();
    uint32_t sp[3][4];
    int t0 = 0, i, j, any, c;

    load4(4, D_004334C0);
    for (i = 0; i < 7; i++) {
        any = 0;
        for (j = 0; j < 3; j++) {
            int32_t y, z;

            memcpy(VFW(12), D_0046BBB0 + i * 4, 16);
            memcpy(VFW(14), D_0046BB80 + j * 4, 16);
            v_add3(0, 12, 12, 14);                               /* vadd.xyzw vf12, vf12, vf14 */
            xform13();
            gPortVu0Q = sf_div_core(VFW(0)[3], VFW(13)[3], 0);   /* vdiv Q, vf0w, vf13w; vwaitq */
            for (c = 0; c < 3; c++) {                            /* vmulq.xyz vf18, vf13, Q */
                VFW(18)[c] = sf_mul_core(VFW(13)[c], gPortVu0Q, 0);
            }
            v_bc(0, 18, 0, 0, 0, MW);                            /* vaddx.w vf18, vf0, vf0x */
            for (c = 0; c < 4; c++) {                            /* vftoi4.xyzw vf20, vf18 */
                VFW(20)[c] = v_ftoi(VFW(18)[c], 4);
            }
            memcpy(sp[j], VFW(20), 16);
            y = (int32_t)sp[j][1];
            z = (int32_t)sp[j][2];
            any |= sp[j][0] > 0xFFFFu || y < 0 || y > 0xFFFF || z < 0 || z > 0xFFFFF;
        }
        if (!any) {
            uint8_t *p = (uint8_t *)(uintptr_t)t3 + t0 * 64;

            t0++;
            memcpy(p + 0x40, D_0046BB70, 16);
            memcpy(p + 0x50, sp, 48);
        }
    }
    if (t0 == 0) {
        func_00399580(t3);
        return;
    }
    {
        uint64_t *q = (uint64_t *)(uintptr_t)t3;

        q[0] = 0x1000000000000002ull;
        q[1] = 0xE;
        q[2] = 0x50000;
        q[3] = 0x48;
        q[4] = 0x44;
        q[5] = 0x43;
        q[6] = (0x4141C000ull << 32) | (uint32_t)(t0 | 0x8000);
        q[7] = 0x4441;
    }
    func_00399580(t3 + t0 * 64 + 0x40);
}

/* func_003ECAD8 (0x3ECAD8): the body's velocities (+0x30 = +0x00 - +0x20, +0x70 = +0x40 - +0x60), its matrix
 * (func_003E9120 on D_01FC61A0, the folded angles of +0x40 and the position), then the four corners D_0046BC20 and
 * the centre D_0046BC60 through it: +0x80.. / +0x140 the points, +0x100.. / +0x160 their moves from +0xC0.. / +0x150
 * (the previous frame's). */
extern uint32_t D_0046BC20[16];
extern uint32_t D_0046BC60[4];
extern uint32_t D_01FC61A0[16];
void sceVu0SubVector(void *d, const void *a, const void *b); /* plat_vu0.c */
void func_003EC320(void *out, const void *in);              /* src/tekken_bowl.c */

void func_003ECAD8(uint8_t *b) {
    uint32_t r[4] __attribute__((aligned(16)));
    int i;

    sceVu0SubVector(b + 0x30, b, b + 0x20);
    sceVu0SubVector(b + 0x70, b + 0x40, b + 0x60);
    func_003EC320(r, b + 0x40);
    func_003E9120(D_01FC61A0, r, (const uint32_t *)b);
    for (i = 0; i < 4; i++) {
        Port_Vu0BowlXform((uint32_t *)(b + 0x80 + i * 16), D_0046BC20 + i * 4);
        memcpy(VFW(14), b + 0xC0 + i * 16, 16);
        v_add3(1, 15, 13, 14);                                   /* vsub.xyzw vf15, vf13, vf14 */
        memcpy(b + 0x100 + i * 16, VFW(15), 16);
    }
    Port_Vu0BowlXform((uint32_t *)(b + 0x140), D_0046BC60);
    memcpy(VFW(14), b + 0x150, 16);
    v_add3(1, 15, 13, 14);
    memcpy(b + 0x160, VFW(15), 16);
}
/* src/model_anim_b.c: the inlined VU0 rotation of the draft RotMtxXYZ: m rotated about x, y then z by r[0..2]. */
void Port_Vu0RotMtxXYZ(uint32_t *m, const uint32_t *r) {
    load4(4, m);
    func_003E8FA0(r[0]);
    rot_x();
    func_003E8FA0(r[1]);
    rot_y();
    func_003E8FA0(r[2]);
    rot_z();
    store4(m, 4);
}

/* ---- src/geometry_b.c: the hand-written VU0 rotation builders (0x349CF8-0x34A148), on the VU0 registers ----
 * Their drafts built the matrix from a unit matrix with libvu0 calls, which cleared row 3: the originals store rows
 * 0-2 only (func_00349D70 also m[3][3] = 1.0, func_0034A050 row 3 = vf0), and func_00347760 relies on it: the
 * local matrices of joints 7-0x1C keep their bone offsets in row 3. With the offsets cleared the fighters' heads
 * collapsed onto their necks (T29 b4). Each step below is one instruction of the original, in its order. */
/* The FPU's fold of the angle (c.lt.s; add.s / sub.s with pi/2 from .lit4) into vf3.x; returns a3 (t < 0). */
static int fold_angle(uint32_t t) {
    const uint32_t h = 0x3FC90FDBu; /* D_005F57CC ... D_005F57E8 */
    int neg = sf_key(t) < 0;

    v_qmtc2_word(3, neg ? sf_fpu_add(h, t) : sf_fpu_add(h, t ^ SF_SIGN));
    return neg;
}
/* func_00349CF8: vf4.x = +/-sqrt(1 - c^2) (sin, negative when a3), vf4.y = c = the series of vf3.x (cos), with the
 * coefficients D_0046B040 and vf19 = 0. */
static void vu0_cos_sin(int neg) {
    memcpy(VFW(1), Port_Ptr(0x0046B040u), 16);          /* lqc2 vf1, D_0046B040 */
    VFW(3)[3] = VFW(3)[0];                               /* vmr32.w vf3, vf3 */
    v_bc(0, 4, 0, 3, 0, MX);                             /* vaddx.x vf4, vf0, vf3x */
    VFW(3)[0] = sf_mul_core(VFW(3)[0], VFW(3)[0], 0);    /* vmul.x vf3, vf3, vf3 */
    v_bc(2, 4, 4, 0, 0, MY | MZ | MW);                   /* vmulx.yzw vf4, vf4, vf0x */
    v_bc(2, 2, 1, 3, 3, ALL);                            /* vmulw.xyzw vf2, vf1, vf3w */
    v_bc(2, 2, 2, 3, 0, ALL);                            /* vmulx.xyzw vf2, vf2, vf3x */
    v_bc(2, 2, 2, 3, 0, MX | MY | MZ);                   /* vmulx.xyz */
    v_bc(0, 4, 4, 2, 3, MX);                             /* vaddw.x vf4, vf4, vf2w */
    v_bc(2, 2, 2, 3, 0, MX | MY);                        /* vmulx.xy */
    v_bc(0, 4, 4, 2, 2, MX);                             /* vaddz.x */
    v_bc(2, 2, 2, 3, 0, MX);                             /* vmulx.x */
    v_bc(0, 4, 4, 2, 1, MX);                             /* vaddy.x */
    v_bc(0, 4, 4, 2, 0, MX);                             /* vaddx.x */
    v_bc(0, 4, 19, 4, 0, MX | MY);                       /* vaddx.xy vf4, vf19, vf4x */
    VFW(5)[0] = sf_mul_core(VFW(4)[0], VFW(4)[0], 0);    /* vmul.x vf5, vf4, vf4 */
    v_bc(1, 5, 0, 5, 0, MW);                             /* vsubx.w vf5, vf0, vf5x */
    gPortVu0Q = sf_sqrt_core(VFW(5)[3], 0);              /* vsqrt Q, vf5w; vwaitq */
    v_qmtc2_word(5, gPortVu0Q);                          /* cfc2 $t0, $vi22; qmtc2 $t0, vf5 */
    v_bc(neg, 4, 19, 5, 0, MX);                          /* vaddx.x / vsubx.x vf4, vf19, vf5x */
}
/* vsub.xyzw fd, vf0, vf0 (field by field: all four are +0) */
static void v_zero(int fd) {
    int c;
    for (c = 0; c < 4; c++) {
        VFW(fd)[c] = sf_add_core(VFW(0)[c], VFW(0)[c] ^ SF_SIGN, 0);
    }
}
static void v_move(int fd, int fs) { memcpy(VFW(fd), VFW(fs), 16); }
/* ACC.c += fs.c * vf0.w (vmaddaw.c) */
static void v_madda_w(int fs, int c) {
    gPortVu0Acc[c] = sf_add_core(gPortVu0Acc[c], sf_mul_core(VFW(fs)[c], VFW(0)[3], 0), 0);
}

/* 0x349D70: m rows 0-2 = Rz(ang[2]) Ry(ang[1]) Rx(ang[0]), m[3][3] = 1.0; row 3 xyz kept. */
void func_00349D70(uint32_t *m, const uint32_t *ang) {
    int neg, r;

    m[15] = 0x3F800000u;
    neg = fold_angle(ang[0]);
    v_zero(19);
    vu0_cos_sin(neg);
    v_move(7, 19);
    v_move(8, 19);
    v_bc(0, 7, 19, 4, 0, MZ); /* vaddx.z vf7, vf19, vf4x */
    v_bc(0, 7, 19, 4, 1, MY); /* vaddy.y vf7, vf19, vf4y */
    v_bc(1, 8, 19, 4, 0, MY); /* vsubx.y vf8, vf19, vf4x */
    v_bc(0, 8, 19, 4, 1, MZ); /* vaddy.z vf8, vf19, vf4y */
    neg = fold_angle(ang[1]);
    vu0_cos_sin(neg);
    v_move(6, 19);
    v_move(11, 19);
    v_bc(1, 6, 19, 4, 0, MZ);  /* vsubx.z vf6, vf19, vf4x */
    v_bc(0, 6, 19, 4, 1, MX);  /* vaddy.x vf6, vf19, vf4y */
    v_bc(0, 11, 19, 4, 0, MX); /* vaddx.x vf11, vf19, vf4x */
    v_bc(0, 11, 19, 4, 1, MZ); /* vaddy.z vf11, vf19, vf4y */
    for (r = 7; r <= 8; r++) {
        v_acc_bc(0, 9, r, 0, ALL);       /* vmulax.xyzw ACC, vf9, vf(r)x (vf9 as the PS2 leaves it; x is 0) */
        v_madda_w(r, 1);                 /* vmaddaw.y ACC, vf(r), vf0w */
        v_madd_bc(0, r, 11, r, 2, ALL);  /* vmaddz.xyzw vf(r), vf11, vf(r)z */
    }
    neg = fold_angle(ang[2]);
    vu0_cos_sin(neg);
    v_move(9, 19);
    v_move(10, 19);
    v_bc(0, 9, 19, 4, 0, MY);  /* vaddx.y vf9, vf19, vf4x */
    v_bc(0, 9, 19, 4, 1, MX);  /* vaddy.x vf9, vf19, vf4y */
    v_bc(1, 10, 19, 4, 0, MX); /* vsubx.x vf10, vf19, vf4x */
    v_bc(0, 10, 19, 4, 1, MY); /* vaddy.y vf10, vf19, vf4y */
    for (r = 6; r <= 8; r++) {
        v_acc_bc(0, 9, r, 0, ALL);       /* vmulax.xyzw ACC, vf9, vf(r)x */
        v_madda_w(r, 2);                 /* vmaddaw.z ACC, vf(r), vf0w */
        v_madd_bc(0, r, 10, r, 1, ALL);  /* vmaddy.xyzw vf(r), vf10, vf(r)y */
    }
    memcpy(m, VFW(6), 16);
    memcpy(m + 4, VFW(7), 16);
    memcpy(m + 8, VFW(8), 16);
}

/* 0x349EE0: m rows 0-2 = Rx(ang[0]) Ry(ang[1]) Rz(ang[2]); row 3 untouched. */
void func_00349EE0(uint32_t *m, const uint32_t *ang) {
    int neg, r;

    neg = fold_angle(ang[2]);
    v_zero(19);
    vu0_cos_sin(neg);
    v_move(6, 19);
    v_move(7, 19);
    v_bc(0, 6, 19, 4, 0, MY); /* vaddx.y vf6, vf19, vf4x */
    v_bc(0, 6, 19, 4, 1, MX); /* vaddy.x vf6, vf19, vf4y */
    v_bc(1, 7, 19, 4, 0, MX); /* vsubx.x vf7, vf19, vf4x */
    v_bc(0, 7, 19, 4, 1, MY); /* vaddy.y vf7, vf19, vf4y */
    neg = fold_angle(ang[1]);
    vu0_cos_sin(neg);
    v_move(9, 19);
    v_move(10, 19);
    v_move(8, 19);
    v_bc(0, 10, 19, 0, 3, MY); /* vaddw.y vf10, vf19, vf0w */
    v_bc(1, 9, 19, 4, 0, MZ);  /* vsubx.z vf9, vf19, vf4x */
    v_bc(0, 9, 19, 4, 1, MX);  /* vaddy.x vf9, vf19, vf4y */
    v_bc(0, 8, 19, 4, 0, MX);  /* vaddx.x vf8, vf19, vf4x */
    v_bc(0, 8, 19, 4, 1, MZ);  /* vaddy.z vf8, vf19, vf4y */
    for (r = 6; r <= 7; r++) {
        v_acc_bc(0, 9, r, 0, ALL);   /* vmulax.xyzw ACC, vf9, vf(r)x */
        {
            uint32_t b = VFW(r)[1];
            int c;
            for (c = 0; c < 4; c++) { /* vmadday.xyzw ACC, vf10, vf(r)y */
                gPortVu0Acc[c] = sf_add_core(gPortVu0Acc[c], sf_mul_core(VFW(10)[c], b, 0), 0);
            }
        }
        v_madd_bc(0, r, 8, r, 2, ALL); /* vmaddz.xyzw vf(r), vf8, vf(r)z */
    }
    neg = fold_angle(ang[0]);
    vu0_cos_sin(neg);
    v_move(10, 19);
    v_move(11, 19);
    v_bc(0, 10, 19, 4, 0, MZ); /* vaddx.z vf10, vf19, vf4x */
    v_bc(0, 10, 19, 4, 1, MY); /* vaddy.y vf10, vf19, vf4y */
    v_bc(1, 11, 19, 4, 0, MY); /* vsubx.y vf11, vf19, vf4x */
    v_bc(0, 11, 19, 4, 1, MZ); /* vaddy.z vf11, vf19, vf4y */
    for (r = 6; r <= 8; r++) {
        v_acc_bc(0, 10, r, 1, ALL);      /* vmulay.xyzw ACC, vf10, vf(r)y */
        v_madda_w(r, 0);                 /* vmaddaw.x ACC, vf(r), vf0w */
        v_madd_bc(0, r, 11, r, 2, ALL);  /* vmaddz.xyzw vf(r), vf11, vf(r)z */
    }
    memcpy(m, VFW(6), 16);
    memcpy(m + 4, VFW(7), 16);
    memcpy(m + 8, VFW(8), 16);
}

/* 0x34A050: the rotation about X by a, row 3 = vf0. */
void func_0034A050(uint32_t *m, uint32_t a) {
    int neg = fold_angle(a);

    v_zero(19);
    vu0_cos_sin(neg);
    v_zero(5);
    v_move(6, 5);
    v_move(7, 5);
    v_move(8, 5);
    v_bc(0, 6, 5, 0, 3, MX); /* vaddw.x vf6, vf5, vf0w */
    v_bc(0, 7, 5, 4, 0, MZ); /* vaddx.z vf7, vf5, vf4x */
    v_bc(0, 7, 5, 4, 1, MY); /* vaddy.y vf7, vf5, vf4y */
    v_bc(1, 8, 5, 4, 0, MY); /* vsubx.y vf8, vf5, vf4x */
    v_bc(0, 8, 5, 4, 1, MZ); /* vaddy.z vf8, vf5, vf4y */
    memcpy(m, VFW(6), 16);
    memcpy(m + 4, VFW(7), 16);
    memcpy(m + 8, VFW(8), 16);
    memcpy(m + 12, VFW(0), 16);
}

/* 0x34A0D0: the rotation about Z by a in rows 0-2 (row 2 = vf0 rotated: (0, 0, 1, 0)); row 3 untouched. */
void func_0034A0D0(uint32_t *m, uint32_t a) {
    int neg = fold_angle(a);

    v_zero(19);
    vu0_cos_sin(neg);
    v_zero(5);
    v_move(6, 5);
    v_move(7, 5);
    VFW(8)[0] = VFW(0)[1];   /* vmr32.xyzw vf8, vf0 */
    VFW(8)[1] = VFW(0)[2];
    VFW(8)[2] = VFW(0)[3];
    VFW(8)[3] = VFW(0)[0];
    v_bc(0, 6, 5, 0, 3, MX); /* vaddw.x vf6, vf5, vf0w */
    v_bc(0, 6, 5, 4, 0, MY); /* vaddx.y vf6, vf5, vf4x */
    v_bc(0, 6, 5, 4, 1, MX); /* vaddy.x vf6, vf5, vf4y */
    v_bc(1, 7, 5, 4, 0, MX); /* vsubx.x vf7, vf5, vf4x */
    v_bc(0, 7, 5, 4, 1, MY); /* vaddy.y vf7, vf5, vf4y */
    memcpy(m, VFW(6), 16);
    memcpy(m + 4, VFW(7), 16);
    memcpy(m + 8, VFW(8), 16);
}
