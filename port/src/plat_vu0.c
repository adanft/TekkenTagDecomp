/*
 * libvu0: the SDK's VU0 macro-mode vector and matrix helpers (0x34A7A8-0x34B4E8, asm/B7A8.s), in C. Matrices are
 * column-major (m[column][row]), as the library keeps them: m[3] is the translation. This file is a VU file
 * (port/tools/toolchain.py VU_FILES): its + - * / and sqrtf are the VU's arithmetic. The library's few EE FPU
 * instructions (ViewScreenMatrix, the angle fold of the Rot* functions) call Port_Fpu* explicitly.
 *
 * Each function was checked against the library's code instruction by instruction (T29 b4): fields written or
 * kept (w of Normalize / OuterProduct / InversMatrix / *XYZ), the order of the ACC sums, operands loaded before any
 * store (TransMatrix), the library's own cos / sin series (Rot*), the flag test of ClipAll. The self-test is
 * TTT_SELFTEST=float, group libvu0 (plat_floattest.c).
 */
#include <stdint.h>
#include <string.h>
#include "port_addr.h"

typedef float FVec[4];
typedef float FMat[4][4];
typedef int32_t IVec[4];

float sqrtf(float);
float Port_FpuAdd(float, float); /* softfloat_ps2.c: add.s / sub.s / mul.s / div.s */
float Port_FpuSub(float, float);
float Port_FpuMul(float, float);
float Port_FpuDiv(float, float);

static uint32_t bits_of(float f) {
    union { float f; uint32_t u; } b;
    b.f = f;
    return b.u;
}
static float float_of(uint32_t u) {
    union { float f; uint32_t u; } b;
    b.u = u;
    return b.f;
}

void sceVu0CopyVector(FVec v0, const FVec v1) { memcpy(v0, v1, 16); }
void sceVu0CopyVectorXYZ(FVec v0, const FVec v1) { v0[0] = v1[0]; v0[1] = v1[1]; v0[2] = v1[2]; }
void sceVu0AddVector(FVec v0, const FVec v1, const FVec v2) {
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = v1[i] + v2[i];
    }
}
void sceVu0SubVector(FVec v0, const FVec v1, const FVec v2) {
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = v1[i] - v2[i];
    }
}
void sceVu0MulVector(FVec v0, const FVec v1, const FVec v2) {
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = v1[i] * v2[i];
    }
}
void sceVu0ScaleVector(FVec v0, const FVec v1, float s) {
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = v1[i] * s;
    }
}
void sceVu0ScaleVectorXYZ(FVec v0, const FVec v1, float s) {
    v0[0] = v1[0] * s;
    v0[1] = v1[1] * s;
    v0[2] = v1[2] * s;
    v0[3] = v1[3];
}
void sceVu0DivVector(FVec v0, const FVec v1, float q) {
    float r = 1.0f / q;
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = v1[i] * r;
    }
}
void sceVu0DivVectorXYZ(FVec v0, const FVec v1, float q) {
    float r = 1.0f / q;
    v0[0] = v1[0] * r;
    v0[1] = v1[1] * r;
    v0[2] = v1[2] * r;
    v0[3] = v1[3];
}
void sceVu0InterVector(FVec v0, const FVec v1, const FVec v2, float r) {
    float s = 1.0f - r;
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = v1[i] * r + v2[i] * s;
    }
}
float sceVu0InnerProduct(const FVec v0, const FVec v1) { return v0[0] * v1[0] + v0[1] * v1[1] + v0[2] * v1[2]; }
void sceVu0OuterProduct(FVec v0, const FVec v1, const FVec v2) {
    float x = v1[1] * v2[2] - v1[2] * v2[1];
    float y = v1[2] * v2[0] - v1[0] * v2[2];
    float z = v1[0] * v2[1] - v1[1] * v2[0];
    v0[0] = x;
    v0[1] = y;
    v0[2] = z;
    v0[3] = 0.0f;
}
/* 0x34A868, after sceVu0InnerProduct in the library: sceVu0Normalize, xyz / |xyz| and w = 0 (the result register
 * is cleared with vsub.xyzw vf6, vf0, vf0 before vmulq.xyz). Callers normalise vectors whose w they never set (stack
 * matrices of func_00348988 / func_00348C68): keeping that w put garbage into the fighters' head and hair joints. */
void func_0034A868(FVec v0, const FVec v1) {
    float r = 1.0f / sqrtf(sceVu0InnerProduct(v1, v1));
    v0[0] = v1[0] * r;
    v0[1] = v1[1] * r;
    v0[2] = v1[2] * r;
    v0[3] = 0.0f;
}
void sceVu0FTOI4Vector(IVec v0, const FVec v1) {
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = (int32_t)(v1[i] * 16.0f);
    }
}
void sceVu0FTOI0Vector(IVec v0, const FVec v1) {
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = (int32_t)v1[i];
    }
}
void sceVu0ITOF4Vector(FVec v0, const IVec v1) {
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = (float)v1[i] * 0.0625f;
    }
}
void sceVu0ITOF0Vector(FVec v0, const IVec v1) {
    int i;
    for (i = 0; i < 4; i++) {
        v0[i] = (float)v1[i];
    }
}

void sceVu0UnitMatrix(FMat m) {
    memset(m, 0, 64);
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}
void sceVu0CopyMatrix(FMat m0, const FMat m1) { memcpy(m0, m1, 64); }
void sceVu0ApplyMatrix(FVec v0, const FMat m, const FVec v1) {
    float r[4];
    int j;
    for (j = 0; j < 4; j++) {
        r[j] = m[0][j] * v1[0] + m[1][j] * v1[1] + m[2][j] * v1[2] + m[3][j] * v1[3];
    }
    memcpy(v0, r, 16);
}
/* m0 = m1 * m2 */
void sceVu0MulMatrix(FMat m0, const FMat m1, const FMat m2) {
    float r[4][4];
    int i;
    for (i = 0; i < 4; i++) {
        sceVu0ApplyMatrix(r[i], m1, m2[i]);
    }
    memcpy(m0, r, 64);
}
void sceVu0TransposeMatrix(FMat m0, const FMat m1) {
    float r[4][4];
    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            r[i][j] = m1[j][i];
        }
    }
    memcpy(m0, r, 64);
}
/* 0x34AA20: tv and m1 are loaded before anything is stored, so tv may be a row of m0 (vadd.xyz keeps m1's w). */
void sceVu0TransMatrix(FMat m0, const FMat m1, const FVec tv) {
    float t[4], r3[4];

    memcpy(t, tv, 16);
    memcpy(r3, m1[3], 16);
    r3[0] = r3[0] + t[0];
    r3[1] = r3[1] + t[1];
    r3[2] = r3[2] + t[2];
    if (m0 != m1) {
        memmove(m0, m1, 48);
    }
    memcpy(m0[3], r3, 16);
}
/* Inverse of a rotation + translation (0x34A8E8): the transposed rotation (rows 0-2 get w = 0 from the zeroed
 * translation row they are transposed with), and row 3 = 0 - R^T t (vsub from a register that keeps m1's row 3 w). */
void sceVu0InversMatrix(FMat m0, const FMat m1) {
    float r[4][4];
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r[i][j] = m1[j][i];
        }
        r[i][3] = 0.0f;
    }
    for (j = 0; j < 3; j++) {
        r[3][j] = 0.0f - (r[0][j] * m1[3][0] + r[1][j] * m1[3][1] + r[2][j] * m1[3][2]);
    }
    r[3][3] = m1[3][3];
    memcpy(m0, r, 64);
}
/* The library's cos / sin (0x34AAF0, reached from RotMatrixX / Y / Z), not libm's. The caller folds the angle with
 * the FPU: a = pi/2 - t (t >= 0) or pi/2 + t (t < 0, c.lt.s). The VU0 then sums the odd series of sin(a) = cos(t)
 * with the four coefficients at 0x4F5700 (x, y, z, w = 1/9!, -1/7!, 1/5!, -1/3!, read from the game's data), the
 * a^3 term first, and takes sin(t) = +/-sqrt(1 - cos^2) (vsqrt), negative when t < 0. Every step below is one VU
 * instruction of that routine, in its order. */
static void lib_cos_sin(float t, float *cos_out, float *sin_out) {
    const float *k = (const float *)Port_Ptr(0x4F5700u);
    int neg = t < 0.0f;
    float a = neg ? Port_FpuAdd(float_of(0x3FC90FDBu), t) : Port_FpuSub(float_of(0x3FC90FDBu), t);
    float x = 0.0f + a; /* vaddx.x vf4, vf0, vf6x */
    float a2 = a * a;   /* vmul.x vf6, vf6, vf6 */
    float p[4], q;
    int i;

    for (i = 0; i < 4; i++) {
        p[i] = k[i] * a; /* vmulw.xyzw vf8, vf5, vf6w */
    }
    for (i = 0; i < 4; i++) {
        p[i] = p[i] * a2; /* vmulx.xyzw */
    }
    for (i = 0; i < 3; i++) {
        p[i] = p[i] * a2; /* vmulx.xyz */
    }
    x = x + p[3];
    p[0] = p[0] * a2; /* vmulx.xy */
    p[1] = p[1] * a2;
    x = x + p[2];
    p[0] = p[0] * a2; /* vmulx.x */
    x = x + p[1];
    x = x + p[0];
    *cos_out = 0.0f + x; /* vaddx.xy vf4, vf5, vf4x (vf5 = 0): y = cos */
    x = 0.0f + x;
    q = sqrtf(1.0f - x * x); /* vmul.x vf7, vf4, vf4; vsubx.w vf7, vf0, vf7x; vsqrt Q, vf7w */
    q = 0.0f + q;            /* vaddq.x vf7, vf0, Q */
    *sin_out = neg ? 0.0f - q : 0.0f + q;
}
/* The rotation's columns are built from vf5 = 0 with vadd / vsub (0 + c, 0 - s ...), then m0 = R * m1 column by
 * column (the ACC sum of sceVu0MulMatrix). */
void sceVu0RotMatrixZ(FMat m0, const FMat m1, float rz) {
    float r[4][4], c, s;

    lib_cos_sin(rz, &c, &s);
    memset(r, 0, sizeof r);
    r[0][0] = 0.0f + c;
    r[0][1] = 0.0f + s;
    r[1][0] = 0.0f - s;
    r[1][1] = 0.0f + c;
    r[2][2] = 1.0f;
    r[3][3] = 1.0f;
    sceVu0MulMatrix(m0, r, m1);
}
void sceVu0RotMatrixX(FMat m0, const FMat m1, float rx) {
    float r[4][4], c, s;

    lib_cos_sin(rx, &c, &s);
    memset(r, 0, sizeof r);
    r[0][0] = 1.0f;
    r[1][1] = 0.0f + c;
    r[1][2] = 0.0f + s;
    r[2][1] = 0.0f - s;
    r[2][2] = 0.0f + c;
    r[3][3] = 1.0f;
    sceVu0MulMatrix(m0, r, m1);
}
void sceVu0RotMatrixY(FMat m0, const FMat m1, float ry) {
    float r[4][4], c, s;

    lib_cos_sin(ry, &c, &s);
    memset(r, 0, sizeof r);
    r[0][0] = 0.0f + c;
    r[0][2] = 0.0f - s;
    r[1][1] = 1.0f;
    r[2][0] = 0.0f + s;
    r[2][2] = 0.0f + c;
    r[3][3] = 1.0f;
    sceVu0MulMatrix(m0, r, m1);
}
void sceVu0RotMatrix(FMat m0, const FMat m1, const FVec r) {
    sceVu0RotMatrixZ(m0, m1, r[2]);
    sceVu0RotMatrixY(m0, m0, r[1]);
    sceVu0RotMatrixX(m0, m0, r[0]);
}
void sceVu0ViewScreenMatrix(FMat m, float scrz, float ax, float ay, float cx, float cy, float zmin, float zmax,
                            float nearz, float farz) {
    float mt[4][4];
    /* EE FPU code in the library (0x34AFB0): add.s / mul.s / div.s, in its order */
    float f0 = Port_FpuAdd(-zmin, zmax);
    float f21 = Port_FpuMul(Port_FpuMul(farz, nearz), f0);
    float f20 = Port_FpuAdd(Port_FpuMul(-zmax, nearz), Port_FpuMul(zmin, farz));
    float f19 = Port_FpuAdd(-nearz, farz);
    float az = Port_FpuDiv(f21, f19);
    float cz = Port_FpuDiv(f20, f19);
    sceVu0UnitMatrix(m);
    m[0][0] = scrz;
    m[1][1] = scrz;
    m[2][2] = 0.0f;
    m[3][3] = 0.0f;
    m[3][2] = 1.0f;
    m[2][3] = 1.0f;
    sceVu0UnitMatrix(mt);
    mt[0][0] = ax;
    mt[1][1] = ay;
    mt[2][2] = az;
    mt[3][0] = cx;
    mt[3][1] = cy;
    mt[3][2] = cz;
    sceVu0MulMatrix(m, mt, m);
}
/* Transform, perspective divide, 12.4 fixed point (z and w as integers when mode is set). */
void sceVu0RotTransPers(IVec v0, const FMat m, const FVec v1, int mode) {
    float v[4], q;
    sceVu0ApplyMatrix(v, m, v1);
    q = 1.0f / v[3];
    v[0] *= q;
    v[1] *= q;
    v[2] *= q;
    v0[0] = (int32_t)(v[0] * 16.0f);
    v0[1] = (int32_t)(v[1] * 16.0f);
    if (mode) {
        v0[2] = (int32_t)v[2];
        v0[3] = (int32_t)v[3];
    } else {
        v0[2] = (int32_t)(v[2] * 16.0f);
        v0[3] = (int32_t)(v[3] * 16.0f);
    }
}
void sceVu0RotTransPersN(IVec *v0, const FMat m, const FVec *v1, int n, int mode) {
    int i;
    for (i = 0; i < n; i++) {
        sceVu0RotTransPers(v0[i], m, v1[i], mode);
    }
}
/* Whether a VU result sets the zero or the sign flag. */
static int zero_or_sign(float f) {
    uint32_t u = bits_of(f);
    return (u & 0x7F800000u) == 0 || (u & 0x80000000u) != 0;
}
/* 0x34B3F8: 0 as soon as one vertex p = ms * v has x - min.x w, y - min.y w, w - min.w, max.x w - x, max.y w - y
 * and max.w - w all > 0 (ctc2 clears the sticky flags, the two vsub.xyw set them, cfc2 & 0xC0 = ZS | SS); 1 when no
 * vertex has (the object is culled: model_anim_b.c unk83 / unk84). z is not tested. */
int sceVu0ClipAll(const FVec minv, const FVec maxv, const FMat ms, const FVec *vm, int n) {
    do {
        float p[4];
        float lo0, lo1, hi0, hi1;

        sceVu0ApplyMatrix(p, ms, vm[0]);
        vm++;
        lo0 = minv[0] * p[3]; /* vmulw.xyz vf11, vf9, vf8w */
        lo1 = minv[1] * p[3];
        hi0 = maxv[0] * p[3]; /* vmulw.xyz vf12, vf10, vf8w */
        hi1 = maxv[1] * p[3];
        if (!(zero_or_sign(p[0] - lo0) | zero_or_sign(p[1] - lo1) | zero_or_sign(p[3] - minv[3]) |
              zero_or_sign(hi0 - p[0]) | zero_or_sign(hi1 - p[1]) | zero_or_sign(maxv[3] - p[3]))) {
            return 0;
        }
    } while (--n != 0);
    return 1;
}
