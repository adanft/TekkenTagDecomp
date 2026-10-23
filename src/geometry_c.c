/* 0x34B4E8-0x34C508 (module geometry, docs/game_overview.md section 4): keeps the camera eye out of a stage volume
 * (an elliptic cylinder, func_0034B4E8, or an ellipsoid, func_0034BDD8) by moving it along the eye-to-target line,
 * and the one-joint matrix update func_0034C4A0. .lit4 0x5F5160-0x5F5168 (1.05f, one per collision function). */
#include "common.h"

typedef f32 Vu0Matrix[4][4];
typedef f32 FVec4[4] __attribute__((aligned(16)));

/* The camera collision state: eye (unk0) and target (unk10) in the world, the volume's matrix (unk30) and
 * radii (unk20), the work vectors in the volume's space from 0x180. */
typedef struct {
    /* 0x000 */ FVec4 unk0;       /* eye */
    /* 0x010 */ FVec4 unk10;      /* target */
    /* 0x020 */ f32 *unk20;       /* volume radii */
    /* 0x024 */ u8 pad24[0xC];
    /* 0x030 */ Vu0Matrix unk30;  /* volume to world */
    /* 0x070 */ s32 unk70;        /* volume shape: 1 cylinder, 0 ellipsoid */
    /* 0x074 */ s32 unk74;
    /* 0x078 */ f32 unk78;        /* eye distance */
    /* 0x07C */ u8 pad7C[0x44];
    /* 0x0C0 */ Vu0Matrix unkC0;  /* world to volume */
    /* 0x100 */ u8 pad100[0x80];
    /* 0x180 */ FVec4 unk180;
    /* 0x190 */ FVec4 unk190;     /* eye in the volume */
    /* 0x1A0 */ FVec4 unk1A0;     /* target in the volume */
    /* 0x1B0 */ FVec4 unk1B0;     /* radii */
    /* 0x1C0 */ FVec4 unk1C0;
    /* 0x1D0 */ FVec4 unk1D0;
    /* 0x1E0 */ FVec4 unk1E0;     /* squared radii */
    /* 0x1F0 */ FVec4 unk1F0;
    /* 0x200 */ FVec4 unk200;     /* the two crossings */
    /* 0x210 */ FVec4 unk210;
    /* 0x220 */ FVec4 unk220;
    /* 0x230 */ FVec4 unk230;
    /* 0x240 */ FVec4 unk240;     /* 1 / radii */
    /* 0x250 */ u8 pad250[0x20];
    /* 0x270 */ f32 unk270;
    /* 0x274 */ f32 unk274;
    /* 0x278 */ f32 unk278;
    /* 0x27C */ f32 unk27C;
    /* 0x280 */ f32 unk280;
    /* 0x284 */ f32 unk284;
    /* 0x288 */ f32 unk288;
} CamVol;

/* Float joint (as in src/geometry_b.c). */
typedef struct {
    /* 0x00 */ Vu0Matrix m0;
    /* 0x40 */ Vu0Matrix m1;
    /* 0x80 */ f32 m2[3][4];
    /* 0xB0 */ Vu0Matrix *parent;
    /* 0xB4 */ s32 padB4[3];
} FJoint; /* size 0xC0 */

extern Vu0Matrix D_004334C0;

f32 sqrtf(f32 x);
f32 fabsf(f32 x);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0InversMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0ApplyMatrix(f32 *v0, Vu0Matrix m, f32 *v1);
f32 sceVu0InnerProduct(f32 *v0, f32 *v1);
void sceVu0ScaleVector(f32 *v0, f32 *v1, f32 s);
void sceVu0ScaleVectorXYZ(f32 *v0, f32 *v1, f32 s);
void sceVu0AddVector(f32 *v0, f32 *v1, f32 *v2);
void sceVu0SubVector(f32 *v0, f32 *v1, f32 *v2);
void sceVu0MulVector(f32 *v0, f32 *v1, f32 *v2);
void func_0034A868(f32 *out, f32 *in);
s32 func_0034B4E8(CamVol *c);
s32 func_0034BDD8(CamVol *c);

/* NON_MATCHING: about 420 lines: m2c-based draft of the cylinder test (block order and register allocation differ) */
#if 0
s32 func_0034B4E8(CamVol *c) {
    f32 lim;
    f32 h;
    f32 x;
    f32 z;
    f32 r;
    f32 a;
    f32 b;
    f32 *t;
    s32 k;

    sceVu0ScaleVectorXYZ(c->unk1B0, c->unk20, 10.0f);
    c->unk1B0[1] *= 2.5f;
    sceVu0MulVector(c->unk1E0, c->unk1B0, c->unk1B0);
    c->unk240[0] = 1.0f / c->unk1B0[0];
    c->unk240[1] = 1.0f / c->unk1B0[1];
    c->unk240[2] = 1.0f / c->unk1B0[2];
    sceVu0MulVector(c->unk1F0, c->unk190, c->unk240);
    c->unk1F0[1] = 0.0f;
    c->unk288 = sceVu0InnerProduct(c->unk1F0, c->unk1F0);
    h = c->unk1B0[1];
    lim = 1.05f;
    if (-h <= c->unk190[1] && c->unk190[1] <= h && c->unk288 <= lim) {
        sceVu0MulVector(c->unk1F0, c->unk1A0, c->unk240);
        c->unk1F0[1] = 0.0f;
        c->unk288 = sceVu0InnerProduct(c->unk1F0, c->unk1F0);
        c->unk74 = 1;
        if (!(-c->unk1B0[1] <= c->unk190[1]) || !(c->unk190[1] <= c->unk1B0[1]) || !(c->unk288 <= lim)) {
            return 0;
        }
        goto push;
    }
    sceVu0SubVector(c->unk1F0, c->unk1A0, c->unk190);
    c->unk1F0[1] = 0.0f;
    c->unk274 = sceVu0InnerProduct(c->unk1F0, c->unk1F0);
    c->unk270 = sqrtf(c->unk274); /* unk274 keeps the squared length: compared with squared lengths below (asm 0x34BC88) */
    if (c->unk1A0[0] == c->unk190[0]) {
        if (c->unk1A0[2] == c->unk190[2]) {
push:
            sceVu0MulVector(c->unk1F0, c->unk1A0, c->unk240);
            c->unk1F0[1] = 0.0f;
            func_0034A868(c->unk1F0, c->unk1F0);
            sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk1B0);
            sceVu0SubVector(c->unk1F0, c->unk1F0, c->unk190);
            c->unk1F0[1] = 0.0f;
            func_0034A868(c->unk1F0, c->unk1F0);
            sceVu0ScaleVector(c->unk1F0, c->unk1F0, c->unk270);
            sceVu0AddVector(c->unk1F0, c->unk1F0, c->unk190);
            sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk240);
            c->unk1F0[1] = 0.0f;
            func_0034A868(c->unk1F0, c->unk1F0);
            t = c->unk1A0;
            goto done;
        }
        x = c->unk1A0[0];
        if (x <= -c->unk1B0[0] || c->unk1B0[0] <= x) {
            return 0;
        }
        c->unk200[1] = c->unk1A0[1];
        c->unk210[0] = x;
        c->unk200[0] = x;
        c->unk210[1] = c->unk1A0[1];
        r = c->unk1B0[2] * sqrtf(1.0f - x * x / c->unk1E0[0]);
        c->unk200[2] = r;
        c->unk210[2] = -r;
    } else if (c->unk1A0[2] != c->unk190[2]) {
        c->unk220[1] = c->unk1B0[0] * c->unk190[2];
        c->unk220[3] = c->unk1B0[2] * c->unk190[0];
        c->unk220[0] = c->unk1B0[0] * c->unk1D0[2];
        c->unk220[2] = c->unk1B0[2] * c->unk1D0[0];
        sceVu0MulVector(c->unk230, c->unk220, c->unk220);
        c->unk27C = c->unk220[2] * c->unk220[3] + c->unk220[0] * c->unk220[1];
        c->unk278 = c->unk230[2] + c->unk230[0];
        c->unk280 = c->unk230[3] + c->unk230[1] - c->unk1E0[0] * c->unk1E0[2];
        c->unk284 = c->unk27C * c->unk27C - c->unk278 * c->unk280;
        if (c->unk284 <= 0.0f) {
            return 0;
        }
        h = sqrtf(c->unk284); /* in a register: unk284 keeps the discriminant (asm 0x34B960 / 0x34B9B0) */
        c->unk288 = (-c->unk27C + h) / c->unk278;
        sceVu0ScaleVector(c->unk200, c->unk1D0, c->unk288);
        sceVu0AddVector(c->unk200, c->unk200, c->unk190);
        c->unk288 = (-c->unk27C - h) / c->unk278;
        sceVu0ScaleVector(c->unk210, c->unk1D0, c->unk288);
        sceVu0AddVector(c->unk210, c->unk210, c->unk190);
    } else {
        z = c->unk1A0[2];
        if (z <= -c->unk1B0[2] || c->unk1B0[2] <= z) {
            return 0;
        }
        r = c->unk1B0[0] * sqrtf(1.0f - z * z / c->unk1E0[2]);
        c->unk200[2] = z;
        c->unk200[1] = c->unk1A0[1];
        c->unk210[1] = c->unk1A0[1];
        c->unk200[0] = r;
        c->unk210[2] = z;
        c->unk210[0] = -r;
    }
    k = 0;
    if (fabsf(c->unk1C0[0]) < fabsf(c->unk1C0[2])) {
        k = 2;
    }
    a = c->unk190[k];
    b = c->unk1A0[k];
    if (b < a) {
        if (a <= c->unk200[k] && a <= c->unk210[k]) {
            return 0;
        }
        if (c->unk200[k] <= b && c->unk210[k] <= b) {
            return 0;
        }
    } else {
        if (b <= c->unk200[k] && b <= c->unk210[k]) {
            return 0;
        }
        if (c->unk200[k] <= a && c->unk210[k] <= a) {
            return 0;
        }
    }
    h = c->unk1B0[1];
    if (c->unk200[1] < -h && c->unk210[1] < -h) {
        return 0;
    }
    if (h < c->unk200[1] && h < c->unk210[1]) {
        return 0;
    }
    if (c->unk200[1] < -h || h < c->unk200[1]) {
        c->unk200[1] = c->unk200[1] < 0.0f ? -h : h;
        r = c->unk200[1] - c->unk190[1];
        c->unk200[2] = c->unk190[2] + r * c->unk1D0[2] / c->unk1D0[1];
        c->unk200[0] = c->unk190[0] + r * c->unk1D0[0] / c->unk1D0[1];
    }
    if (c->unk210[1] < -h || h < c->unk210[1]) {
        c->unk210[1] = c->unk210[1] < 0.0f ? -h : h;
        r = c->unk210[1] - c->unk190[1];
        c->unk210[2] = c->unk190[2] + r * c->unk1D0[2] / c->unk1D0[1];
        c->unk210[0] = c->unk190[0] + r * c->unk1D0[0] / c->unk1D0[1];
    }
    sceVu0SubVector(c->unk1F0, c->unk200, c->unk190);
    c->unk1F0[1] = 0.0f;
    if (c->unk274 < sceVu0InnerProduct(c->unk1F0, c->unk1F0)) {
        *(u128 *)c->unk200 = *(u128 *)c->unk1A0;
    }
    sceVu0SubVector(c->unk1F0, c->unk210, c->unk190);
    c->unk1F0[1] = 0.0f;
    if (c->unk274 < sceVu0InnerProduct(c->unk1F0, c->unk1F0)) {
        *(u128 *)c->unk210 = *(u128 *)c->unk1A0;
    }
    sceVu0AddVector(c->unk1F0, c->unk200, c->unk210);
    c->unk1F0[1] = 0.0f;
    sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk240);
    func_0034A868(c->unk1F0, c->unk1F0);
    sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk1B0);
    sceVu0SubVector(c->unk1F0, c->unk1F0, c->unk190);
    c->unk1F0[1] = 0.0f;
    func_0034A868(c->unk1F0, c->unk1F0);
    sceVu0ScaleVector(c->unk1F0, c->unk1F0, c->unk270);
    sceVu0AddVector(c->unk1F0, c->unk1F0, c->unk190);
    c->unk1F0[1] = 0.0f;
    sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk240);
    func_0034A868(c->unk1F0, c->unk1F0);
    t = c->unk1A0;
done:
    c->unk288 = c->unk1A0[1];
    sceVu0MulVector(t, c->unk1F0, c->unk1B0);
    c->unk1A0[1] = c->unk288;
    return 1;
}
#else
LIT4_WORD(D_005F5160, 0x3F866666);
INCLUDE_ASM("asm/nonmatchings/geometry_c", func_0034B4E8);
#endif

/* NON_MATCHING: about 330 lines: the original keeps the work-vector addresses in saved registers and two on the stack; this draft reaches them through c */
#if 0
s32 func_0034BDD8(CamVol *c) {
    f32 lim = 1.05f;
    f32 *t;
    f32 a;
    f32 b;
    s32 k;

    sceVu0ScaleVectorXYZ(c->unk1B0, c->unk20, 50.0f);
    c->unk240[3] = 1.0f;
    c->unk240[0] = 1.0f / c->unk1B0[0];
    c->unk240[1] = 1.0f / c->unk1B0[1];
    c->unk240[2] = 1.0f / c->unk1B0[2];
    sceVu0MulVector(c->unk1F0, c->unk190, c->unk240);
    c->unk284 = sceVu0InnerProduct(c->unk1F0, c->unk1F0);
    if (c->unk284 <= lim) {
        sceVu0MulVector(c->unk1F0, c->unk1A0, c->unk240);
        c->unk284 = sceVu0InnerProduct(c->unk1F0, c->unk1F0);
        c->unk74 = 1;
        if (!(c->unk284 <= lim)) {
            return 0;
        }
        sceVu0MulVector(c->unk1F0, c->unk1A0, c->unk240);
        func_0034A868(c->unk1F0, c->unk1F0);
        sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk1B0);
        sceVu0SubVector(c->unk1F0, c->unk1F0, c->unk190);
        func_0034A868(c->unk1F0, c->unk1F0);
        sceVu0ScaleVector(c->unk1F0, c->unk1F0, c->unk78);
        sceVu0AddVector(c->unk1F0, c->unk1F0, c->unk190);
        sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk240);
        func_0034A868(c->unk1F0, c->unk1F0);
        t = c->unk1A0;
        goto done;
    }
    sceVu0MulVector(c->unk200, c->unk1D0, c->unk240);
    sceVu0MulVector(c->unk210, c->unk190, c->unk240);
    c->unk274 = sceVu0InnerProduct(c->unk200, c->unk200);
    c->unk278 = sceVu0InnerProduct(c->unk200, c->unk210);
    c->unk27C = sceVu0InnerProduct(c->unk210, c->unk210) - 1.0f;
    c->unk280 = c->unk278 * c->unk278 - c->unk274 * c->unk27C;
    if (c->unk280 <= 0.0f) {
        return 0;
    }
    a = sqrtf(c->unk280); /* kept in a register: unk280 stays the discriminant (asm 0x34C00C) */
    sceVu0ScaleVectorXYZ(c->unk200, c->unk1D0, (-c->unk278 - a) / c->unk274);
    sceVu0ScaleVectorXYZ(c->unk210, c->unk1D0, (-c->unk278 + a) / c->unk274);
    sceVu0AddVector(c->unk200, c->unk190, c->unk200);
    sceVu0AddVector(c->unk210, c->unk190, c->unk210);
    sceVu0SubVector(c->unk1F0, c->unk190, c->unk1A0);
    k = 0;
    if (fabsf(c->unk1F0[0]) < fabsf(c->unk1F0[1])) {
        k = 1;
    }
    if (fabsf(c->unk1F0[k]) < fabsf(c->unk1F0[2])) {
        k = 2;
    }
    a = c->unk190[k];
    b = c->unk1A0[k];
    if (b < a) {
        if (a <= c->unk200[k] && a <= c->unk210[k]) {
            return 0;
        }
        if (c->unk200[k] <= b && c->unk210[k] <= b) {
            return 0;
        }
    } else {
        if (b <= c->unk200[k] && b <= c->unk210[k]) {
            return 0;
        }
        if (c->unk200[k] <= a && c->unk210[k] <= a) {
            return 0;
        }
    }
    c->unk270 = c->unk78 * c->unk78;
    sceVu0SubVector(c->unk1F0, c->unk200, c->unk190);
    if (c->unk270 < sceVu0InnerProduct(c->unk1F0, c->unk1F0)) {
        *(u128 *)c->unk200 = *(u128 *)c->unk1A0;
    }
    sceVu0SubVector(c->unk1F0, c->unk210, c->unk190);
    if (c->unk270 < sceVu0InnerProduct(c->unk1F0, c->unk1F0)) {
        *(u128 *)c->unk210 = *(u128 *)c->unk1A0;
    }
    sceVu0AddVector(c->unk1F0, c->unk200, c->unk210);
    sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk240);
    func_0034A868(c->unk1F0, c->unk1F0);
    sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk1B0);
    sceVu0SubVector(c->unk1F0, c->unk1F0, c->unk190);
    func_0034A868(c->unk1F0, c->unk1F0);
    sceVu0ScaleVector(c->unk1F0, c->unk1F0, c->unk78);
    sceVu0AddVector(c->unk1F0, c->unk1F0, c->unk190);
    sceVu0MulVector(c->unk1F0, c->unk1F0, c->unk240);
    func_0034A868(c->unk1F0, c->unk1F0);
    t = c->unk1A0;
done:
    sceVu0MulVector(t, c->unk1F0, c->unk1B0);
    return 1;
}
#else
LIT4_WORD(D_005F5164, 0x3F866666);
INCLUDE_ASM("asm/nonmatchings/geometry_c", func_0034BDD8);
#endif

/* Keeps the camera eye out of the stage volume: works in the volume's space (inverse of unk30), moves the eye
 * along the eye-to-target line by mode (func_0034B4E8 / func_0034BDD8) and, when moved, writes the new eye back. */
s32 func_0034C300(CamVol *c) {
    s32 r;

    c->unk180[3] = 1.0f;
    c->unk180[0] = c->unk180[1] = c->unk180[2] = 0.0f;
    sceVu0InversMatrix(c->unkC0, c->unk30);
    sceVu0ApplyMatrix(c->unk190, c->unkC0, c->unk0);
    sceVu0ApplyMatrix(c->unk1A0, c->unkC0, c->unk10);
    c->unk1A0[3] = 1.0f;
    c->unk190[3] = 1.0f;
    sceVu0SubVector(c->unk1C0, c->unk1A0, c->unk190);
    func_0034A868(c->unk1D0, c->unk1C0);
    sceVu0ScaleVectorXYZ(c->unk1A0, c->unk1D0, c->unk78);
    sceVu0AddVector(c->unk1A0, c->unk1A0, c->unk190);
    if (c->unk70 == 1) {
        r = func_0034B4E8(c);
    } else {
        r = 0;
        if (c->unk70 == 0) {
            r = func_0034BDD8(c);
        }
    }
    if (r != 0) {
        sceVu0SubVector(c->unk10, c->unk1A0, c->unk190);
        func_0034A868(c->unk10, c->unk10);
        sceVu0ScaleVectorXYZ(c->unk10, c->unk10, c->unk78);
        sceVu0AddVector(c->unk10, c->unk10, c->unk190);
        c->unk10[3] = 1.0f;
        sceVu0ApplyMatrix(c->unk10, c->unk30, c->unk10);
    }
    return r;
}

/* One joint: view = D_004334C0 * world, the screen rows = screen * world. */
void func_0034C4A0(FJoint *j, Vu0Matrix screen) {
    Vu0Matrix m;

    sceVu0MulMatrix(j->m1, D_004334C0, j->m0);
    sceVu0MulMatrix(m, screen, j->m0);
    *(u128 *)j->m2[0] = *(u128 *)m[0];
    *(u128 *)j->m2[1] = *(u128 *)m[1];
    *(u128 *)j->m2[2] = *(u128 *)m[2];
}
