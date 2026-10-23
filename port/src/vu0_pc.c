/*
 * PC versions of the game routines written in EE / VU0 assembly (guarded by `#ifndef PORT` in src/, or drafts the PC
 * build leaves off: port/tools/portsrc.py REPLACED). Two are plain C already; the others report themselves until
 * they are written by hand on the VU0 register file of plat_ee.c (list: docs/port/README.md, "Hand replacements").
 */
#include <stdint.h>
#include <string.h>
#include "softfloat_ps2_inl.h"

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
#define VFW(r) ((uint32_t *)gPortVu0Vf[(r)])

static uint32_t *skin(uint32_t **dst, uint32_t *src, int32_t n, uint32_t prog, int envmap) {
    uint32_t *out = *dst;
    uint32_t *p = src;

    do {
        uint32_t t0 = p[4], t1 = p[5], t2 = p[6], t3 = p[7], first[4];
        uint32_t *next = p + 16;

        if (t1 != 0) {
            if (t2 != 0) {
                if (t3 != 0) {
                    memcpy(VFW(5), p + 32, 16);
                    memcpy(VFW(29), p + 36, 16);
                    next += 8;
                }
                gPortVu0Vi[5] = (uint16_t)t3;
                memcpy(VFW(4), p + 24, 16);
                memcpy(VFW(28), p + 28, 16);
                next += 8;
            }
            gPortVu0Vi[4] = (uint16_t)t2;
            memcpy(VFW(3), p + 16, 16);
            memcpy(VFW(27), p + 20, 16);
            next += 8;
        }
        gPortVu0Vi[3] = (uint16_t)t1;
        gPortVu0Vi[2] = (uint16_t)t0;
        memcpy(VFW(7), p, 16);
        memcpy(VFW(2), p + 8, 16);
        memcpy(VFW(26), p + 12, 16);
        Port_Vu0CallMs(prog);
        memcpy(first, p, 16);
        p = next;
        out += 12;
        memcpy(out - 12, envmap ? VFW(24) : first, 16);
        memcpy(out - 8, VFW(20), 16);
        memcpy(out - 4, VFW(22), 16);
    } while (--n != 0);
    *dst = out;
    return p;
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

/* src/tekken_bowl.c: m2c drafts around that VU0 code (portsrc.REPLACED). */
TODO(func_003EA978)
TODO(func_003EAF78)
TODO(func_003EBC98)
TODO(func_003EC388)
TODO(func_003ECAD8)
TODO(func_003ED338)
TODO(func_003EEF00)
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
