/*
 * libvu0: the SDK's VU0 macro-mode vector and matrix helpers, in C. Matrices are column-major (m[column][row]),
 * as the library keeps them: m[3] is the translation. The float operations go through the build's float
 * helpers (temporary host IEEE in plat_float_tmp.c, the EE model from T23); sin / cos are libm's for now, not the
 * library's own polynomials [T23: match the EE results].
 */
#include <stdint.h>
#include <string.h>

typedef float FVec[4];
typedef float FMat[4][4];
typedef int32_t IVec[4];

float sinf(float);
float cosf(float);
float sqrtf(float);

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
/* 0x34A868, after sceVu0InnerProduct in the library: sceVu0Normalize (xyz / |xyz|, w kept). */
void func_0034A868(FVec v0, const FVec v1) {
    float r = 1.0f / sqrtf(sceVu0InnerProduct(v1, v1));
    v0[0] = v1[0] * r;
    v0[1] = v1[1] * r;
    v0[2] = v1[2] * r;
    v0[3] = v1[3];
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
void sceVu0TransMatrix(FMat m0, const FMat m1, const FVec tv) {
    if (m0 != m1) {
        memcpy(m0, m1, 64);
    }
    m0[3][0] += tv[0];
    m0[3][1] += tv[1];
    m0[3][2] += tv[2];
}
/* Inverse of a rotation + translation: the transposed rotation, and the translation brought back through it. */
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
        r[3][j] = -(r[0][j] * m1[3][0] + r[1][j] * m1[3][1] + r[2][j] * m1[3][2]);
    }
    r[3][3] = 1.0f;
    memcpy(m0, r, 64);
}
static void rot(FMat m0, const FMat m1, int a, int b, float t) {
    float r[4][4];
    float c = cosf(t), s = sinf(t);
    sceVu0UnitMatrix(r);
    r[a][a] = c;
    r[a][b] = s;
    r[b][a] = -s;
    r[b][b] = c;
    sceVu0MulMatrix(m0, r, m1);
}
void sceVu0RotMatrixX(FMat m0, const FMat m1, float rx) { rot(m0, m1, 1, 2, rx); }
void sceVu0RotMatrixY(FMat m0, const FMat m1, float ry) { rot(m0, m1, 2, 0, ry); }
void sceVu0RotMatrixZ(FMat m0, const FMat m1, float rz) { rot(m0, m1, 0, 1, rz); }
void sceVu0RotMatrix(FMat m0, const FMat m1, const FVec r) {
    sceVu0RotMatrixZ(m0, m1, r[2]);
    sceVu0RotMatrixY(m0, m0, r[1]);
    sceVu0RotMatrixX(m0, m0, r[0]);
}
void sceVu0ViewScreenMatrix(FMat m, float scrz, float ax, float ay, float cx, float cy, float zmin, float zmax,
                            float nearz, float farz) {
    float mt[4][4];
    float cz = (-zmax * nearz + zmin * farz) / (-nearz + farz);
    float az = farz * nearz * (-zmin + zmax) / (-nearz + farz);
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
/* Whether all n vertices lie outside the same clip plane: placeholder, never (everything is drawn). */
void Port_Hit(const char *name);
int sceVu0ClipAll(const FVec minv, const FVec maxv, const FMat ms, const FVec *vm, int n) {
    (void)minv; (void)maxv; (void)ms; (void)vm; (void)n;
    Port_Hit(__func__);
    return 0;
}
