/* 0x39A2B8-0x39BBF0: fixed-point matrix and vector math in the style of the PlayStation GTE library, on the EE
 * CPU (1.0 = 0x1000). The same code continues past 0x39BBF0 (geometry offset and screen, perspective
 * transforms); see docs/game_overview.md section 4. */
#include "common.h"
#include "gte.h"

/* The current rotation and translation (the GTE's RT and TR registers). */
extern GteMatrix gGteRt;
/* Scratch result of the matrix products and of Gte_ApplyMatrixLV / Gte_ApplyMatrixXyz. */
extern GteMatrix gGteTmpMatrix;
extern GteVector gGteTmpVector;
/* sin(a) for a = 0-0x13FF (0x1000 = 360 degrees), cos(a) = sin(a + 0x400). Read from the original executable. */
extern const u16 gSinTable[];

/* m0 = m0 * m1. */
GteMatrix *Gte_MulMatrix(GteMatrix *m0, GteMatrix *m1) {
    gGteTmpMatrix.m[0][0] = m0->m[0][0] * m1->m[0][0] / 4096 + m0->m[0][1] * m1->m[1][0] / 4096 +
        m0->m[0][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[0][1] = m0->m[0][0] * m1->m[0][1] / 4096 + m0->m[0][1] * m1->m[1][1] / 4096 +
        m0->m[0][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[0][2] = m0->m[0][0] * m1->m[0][2] / 4096 + m0->m[0][1] * m1->m[1][2] / 4096 +
        m0->m[0][2] * m1->m[2][2] / 4096;
    gGteTmpMatrix.m[1][0] = m0->m[1][0] * m1->m[0][0] / 4096 + m0->m[1][1] * m1->m[1][0] / 4096 +
        m0->m[1][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[1][1] = m0->m[1][0] * m1->m[0][1] / 4096 + m0->m[1][1] * m1->m[1][1] / 4096 +
        m0->m[1][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[1][2] = m0->m[1][0] * m1->m[0][2] / 4096 + m0->m[1][1] * m1->m[1][2] / 4096 +
        m0->m[1][2] * m1->m[2][2] / 4096;
    gGteTmpMatrix.m[2][0] = m0->m[2][0] * m1->m[0][0] / 4096 + m0->m[2][1] * m1->m[1][0] / 4096 +
        m0->m[2][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[2][1] = m0->m[2][0] * m1->m[0][1] / 4096 + m0->m[2][1] * m1->m[1][1] / 4096 +
        m0->m[2][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[2][2] = m0->m[2][0] * m1->m[0][2] / 4096 + m0->m[2][1] * m1->m[1][2] / 4096 +
        m0->m[2][2] * m1->m[2][2] / 4096;
    m0->m[0][0] = gGteTmpMatrix.m[0][0];
    m0->m[0][1] = gGteTmpMatrix.m[0][1];
    m0->m[0][2] = gGteTmpMatrix.m[0][2];
    m0->m[1][0] = gGteTmpMatrix.m[1][0];
    m0->m[1][1] = gGteTmpMatrix.m[1][1];
    m0->m[1][2] = gGteTmpMatrix.m[1][2];
    m0->m[2][0] = gGteTmpMatrix.m[2][0];
    m0->m[2][1] = gGteTmpMatrix.m[2][1];
    m0->m[2][2] = gGteTmpMatrix.m[2][2];
    return m0;
}

/* m2 = m0 * m1. */
GteMatrix *Gte_MulMatrix0(GteMatrix *m0, GteMatrix *m1, GteMatrix *m2) {
    gGteTmpMatrix.m[0][0] = m0->m[0][0] * m1->m[0][0] / 4096 + m0->m[0][1] * m1->m[1][0] / 4096 +
        m0->m[0][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[0][1] = m0->m[0][0] * m1->m[0][1] / 4096 + m0->m[0][1] * m1->m[1][1] / 4096 +
        m0->m[0][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[0][2] = m0->m[0][0] * m1->m[0][2] / 4096 + m0->m[0][1] * m1->m[1][2] / 4096 +
        m0->m[0][2] * m1->m[2][2] / 4096;
    gGteTmpMatrix.m[1][0] = m0->m[1][0] * m1->m[0][0] / 4096 + m0->m[1][1] * m1->m[1][0] / 4096 +
        m0->m[1][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[1][1] = m0->m[1][0] * m1->m[0][1] / 4096 + m0->m[1][1] * m1->m[1][1] / 4096 +
        m0->m[1][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[1][2] = m0->m[1][0] * m1->m[0][2] / 4096 + m0->m[1][1] * m1->m[1][2] / 4096 +
        m0->m[1][2] * m1->m[2][2] / 4096;
    gGteTmpMatrix.m[2][0] = m0->m[2][0] * m1->m[0][0] / 4096 + m0->m[2][1] * m1->m[1][0] / 4096 +
        m0->m[2][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[2][1] = m0->m[2][0] * m1->m[0][1] / 4096 + m0->m[2][1] * m1->m[1][1] / 4096 +
        m0->m[2][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[2][2] = m0->m[2][0] * m1->m[0][2] / 4096 + m0->m[2][1] * m1->m[1][2] / 4096 +
        m0->m[2][2] * m1->m[2][2] / 4096;
    m2->m[0][0] = gGteTmpMatrix.m[0][0];
    m2->m[0][1] = gGteTmpMatrix.m[0][1];
    m2->m[0][2] = gGteTmpMatrix.m[0][2];
    m2->m[1][0] = gGteTmpMatrix.m[1][0];
    m2->m[1][1] = gGteTmpMatrix.m[1][1];
    m2->m[1][2] = gGteTmpMatrix.m[1][2];
    m2->m[2][0] = gGteTmpMatrix.m[2][0];
    m2->m[2][1] = gGteTmpMatrix.m[2][1];
    m2->m[2][2] = gGteTmpMatrix.m[2][2];
    return m2;
}

/* m1 = m0 * m1. */
GteMatrix *Gte_MulMatrix2(GteMatrix *m0, GteMatrix *m1) {
    gGteTmpMatrix.m[0][0] = m0->m[0][0] * m1->m[0][0] / 4096 + m0->m[0][1] * m1->m[1][0] / 4096 +
        m0->m[0][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[0][1] = m0->m[0][0] * m1->m[0][1] / 4096 + m0->m[0][1] * m1->m[1][1] / 4096 +
        m0->m[0][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[0][2] = m0->m[0][0] * m1->m[0][2] / 4096 + m0->m[0][1] * m1->m[1][2] / 4096 +
        m0->m[0][2] * m1->m[2][2] / 4096;
    gGteTmpMatrix.m[1][0] = m0->m[1][0] * m1->m[0][0] / 4096 + m0->m[1][1] * m1->m[1][0] / 4096 +
        m0->m[1][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[1][1] = m0->m[1][0] * m1->m[0][1] / 4096 + m0->m[1][1] * m1->m[1][1] / 4096 +
        m0->m[1][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[1][2] = m0->m[1][0] * m1->m[0][2] / 4096 + m0->m[1][1] * m1->m[1][2] / 4096 +
        m0->m[1][2] * m1->m[2][2] / 4096;
    gGteTmpMatrix.m[2][0] = m0->m[2][0] * m1->m[0][0] / 4096 + m0->m[2][1] * m1->m[1][0] / 4096 +
        m0->m[2][2] * m1->m[2][0] / 4096;
    gGteTmpMatrix.m[2][1] = m0->m[2][0] * m1->m[0][1] / 4096 + m0->m[2][1] * m1->m[1][1] / 4096 +
        m0->m[2][2] * m1->m[2][1] / 4096;
    gGteTmpMatrix.m[2][2] = m0->m[2][0] * m1->m[0][2] / 4096 + m0->m[2][1] * m1->m[1][2] / 4096 +
        m0->m[2][2] * m1->m[2][2] / 4096;
    m1->m[0][0] = gGteTmpMatrix.m[0][0];
    m1->m[0][1] = gGteTmpMatrix.m[0][1];
    m1->m[0][2] = gGteTmpMatrix.m[0][2];
    m1->m[1][0] = gGteTmpMatrix.m[1][0];
    m1->m[1][1] = gGteTmpMatrix.m[1][1];
    m1->m[1][2] = gGteTmpMatrix.m[1][2];
    m1->m[2][0] = gGteTmpMatrix.m[2][0];
    m1->m[2][1] = gGteTmpMatrix.m[2][1];
    m1->m[2][2] = gGteTmpMatrix.m[2][2];
    return m1;
}

/* RT = RT * m. */
GteMatrix *Gte_MulRotMatrix(GteMatrix *m) {
    s16 r[3][3];

    r[0][0] = gGteRt.m[0][0];
    r[0][1] = gGteRt.m[0][1];
    r[0][2] = gGteRt.m[0][2];
    r[1][0] = gGteRt.m[1][0];
    r[1][1] = gGteRt.m[1][1];
    r[1][2] = gGteRt.m[1][2];
    r[2][0] = gGteRt.m[2][0];
    r[2][1] = gGteRt.m[2][1];
    r[2][2] = gGteRt.m[2][2];
    gGteRt.m[0][0] = r[0][0] * m->m[0][0] / 4096 + r[0][1] * m->m[1][0] / 4096 + r[0][2] * m->m[2][0] / 4096;
    gGteRt.m[0][1] = r[0][0] * m->m[0][1] / 4096 + r[0][1] * m->m[1][1] / 4096 + r[0][2] * m->m[2][1] / 4096;
    gGteRt.m[0][2] = r[0][0] * m->m[0][2] / 4096 + r[0][1] * m->m[1][2] / 4096 + r[0][2] * m->m[2][2] / 4096;
    gGteRt.m[1][0] = r[1][0] * m->m[0][0] / 4096 + r[1][1] * m->m[1][0] / 4096 + r[1][2] * m->m[2][0] / 4096;
    gGteRt.m[1][1] = r[1][0] * m->m[0][1] / 4096 + r[1][1] * m->m[1][1] / 4096 + r[1][2] * m->m[2][1] / 4096;
    gGteRt.m[1][2] = r[1][0] * m->m[0][2] / 4096 + r[1][1] * m->m[1][2] / 4096 + r[1][2] * m->m[2][2] / 4096;
    gGteRt.m[2][0] = r[2][0] * m->m[0][0] / 4096 + r[2][1] * m->m[1][0] / 4096 + r[2][2] * m->m[2][0] / 4096;
    gGteRt.m[2][1] = r[2][0] * m->m[0][1] / 4096 + r[2][1] * m->m[1][1] / 4096 + r[2][2] * m->m[2][1] / 4096;
    gGteRt.m[2][2] = r[2][0] * m->m[0][2] / 4096 + r[2][1] * m->m[1][2] / 4096 + r[2][2] * m->m[2][2] / 4096;
    return m;
}

/* out = m * v (16-bit vector in, 32-bit out). */
GteVector *Gte_ApplyMatrix(GteMatrix *m, GteSVector *v, GteVector *out) {
    out->vx = v->vx * m->m[0][0] / 4096 + v->vy * m->m[0][1] / 4096 + v->vz * m->m[0][2] / 4096;
    out->vy = v->vx * m->m[1][0] / 4096 + v->vy * m->m[1][1] / 4096 + v->vz * m->m[1][2] / 4096;
    out->vz = v->vx * m->m[2][0] / 4096 + v->vy * m->m[2][1] / 4096 + v->vz * m->m[2][2] / 4096;
    return out;
}

/* out = m * v with 64-bit products (32-bit vector in and out). */
GteVector *Gte_ApplyMatrixLV(GteMatrix *m, GteVector *v, GteVector *out) {
    gGteTmpVector.vx = (s64)v->vx * m->m[0][0] / 4096 + (s64)v->vy * m->m[0][1] / 4096 + (s64)v->vz * m->m[0][2] / 4096;
    gGteTmpVector.vy = (s64)v->vx * m->m[1][0] / 4096 + (s64)v->vy * m->m[1][1] / 4096 + (s64)v->vz * m->m[1][2] / 4096;
    gGteTmpVector.vz = (s64)v->vx * m->m[2][0] / 4096 + (s64)v->vy * m->m[2][1] / 4096 + (s64)v->vz * m->m[2][2] / 4096;
    out->vx = gGteTmpVector.vx;
    out->vy = gGteTmpVector.vy;
    out->vz = gGteTmpVector.vz;
    return out;
}

/* out = RT * v. */
GteVector *Gte_ApplyRotMatrix(GteSVector *v, GteVector *out) {
    out->vx = v->vx * gGteRt.m[0][0] / 4096 + v->vy * gGteRt.m[0][1] / 4096 + v->vz * gGteRt.m[0][2] / 4096;
    out->vy = v->vx * gGteRt.m[1][0] / 4096 + v->vy * gGteRt.m[1][1] / 4096 + v->vz * gGteRt.m[1][2] / 4096;
    out->vz = v->vx * gGteRt.m[2][0] / 4096 + v->vy * gGteRt.m[2][1] / 4096 + v->vz * gGteRt.m[2][2] / 4096;
    return out;
}

/* out = RT * v with 64-bit products. */
GteVector *Gte_ApplyRotMatrixLV(GteVector *v, GteVector *out) {
    return Gte_ApplyMatrixLV(&gGteRt, v, out);
}

/* (*ox, *oy, *oz) = m * (x, y, z). */
void Gte_ApplyMatrixXyz(GteMatrix *m, s32 x, s32 y, s32 z, s32 *ox, s32 *oy, s32 *oz) {
    gGteTmpVector.vx = x * m->m[0][0] / 4096 + y * m->m[0][1] / 4096 + z * m->m[0][2] / 4096;
    gGteTmpVector.vy = x * m->m[1][0] / 4096 + y * m->m[1][1] / 4096 + z * m->m[1][2] / 4096;
    gGteTmpVector.vz = x * m->m[2][0] / 4096 + y * m->m[2][1] / 4096 + z * m->m[2][2] / 4096;
    *ox = gGteTmpVector.vx;
    *oy = gGteTmpVector.vy;
    *oz = gGteTmpVector.vz;
}

/* m2 = m0 * m1 including the translations. */
GteMatrix *Gte_CompMatrix(GteMatrix *m0, GteMatrix *m1, GteMatrix *m2) {
    GteVector v;
    GteVector out;

    Gte_MulMatrix0(m0, m1, m2);
    v.vx = m1->t[0];
    v.vy = m1->t[1];
    v.vz = m1->t[2];
    Gte_ApplyMatrixLV(m0, &v, &out);
    m2->t[0] = out.vx + m0->t[0];
    m2->t[1] = out.vy + m0->t[1];
    m2->t[2] = out.vz + m0->t[2];
    return m2;
}

/* m1 = transpose(m0); the translation is copied. */
GteMatrix *Gte_TransposeMatrix(GteMatrix *m0, GteMatrix *m1) {
    gGteTmpMatrix.m[0][0] = m0->m[0][0];
    gGteTmpMatrix.m[0][1] = m0->m[1][0];
    gGteTmpMatrix.m[0][2] = m0->m[2][0];
    gGteTmpMatrix.m[1][0] = m0->m[0][1];
    gGteTmpMatrix.m[1][1] = m0->m[1][1];
    gGteTmpMatrix.m[1][2] = m0->m[2][1];
    gGteTmpMatrix.m[2][0] = m0->m[0][2];
    gGteTmpMatrix.m[2][1] = m0->m[1][2];
    gGteTmpMatrix.m[2][2] = m0->m[2][2];
    m1->m[0][0] = gGteTmpMatrix.m[0][0];
    m1->m[0][1] = gGteTmpMatrix.m[0][1];
    m1->m[0][2] = gGteTmpMatrix.m[0][2];
    m1->m[1][0] = gGteTmpMatrix.m[1][0];
    m1->m[1][1] = gGteTmpMatrix.m[1][1];
    m1->m[1][2] = gGteTmpMatrix.m[1][2];
    m1->m[2][0] = gGteTmpMatrix.m[2][0];
    m1->m[2][1] = gGteTmpMatrix.m[2][1];
    m1->m[2][2] = gGteTmpMatrix.m[2][2];
    m1->t[0] = m0->t[0];
    m1->t[1] = m0->t[1];
    m1->t[2] = m0->t[2];
    return m1;
}

/* Rotation by `angle` about X. */
GteMatrix *Gte_RotMatrixX(s32 angle, GteMatrix *m) {
    u16 s, c;

    angle &= 0xFFF;
    c = gSinTable[angle + 0x400];
    s = gSinTable[angle];
    m->m[0][0] = 0x1000;
    m->m[0][1] = 0;
    m->m[0][2] = 0;
    m->m[1][0] = 0;
    m->m[1][1] = c;
    m->m[1][2] = -s;
    m->m[2][0] = 0;
    m->m[2][1] = s;
    m->m[2][2] = c;
    return m;
}

/* Rotation by `angle` about Y. */
GteMatrix *Gte_RotMatrixY(s32 angle, GteMatrix *m) {
    u16 s, c;

    angle &= 0xFFF;
    c = gSinTable[angle + 0x400];
    s = gSinTable[angle];
    m->m[0][0] = c;
    m->m[0][1] = 0;
    m->m[0][2] = s;
    m->m[1][0] = 0;
    m->m[1][1] = 0x1000;
    m->m[1][2] = 0;
    m->m[2][0] = -s;
    m->m[2][1] = 0;
    m->m[2][2] = c;
    return m;
}

/* Rotation by `angle` about Z. */
GteMatrix *Gte_RotMatrixZ(s32 angle, GteMatrix *m) {
    u16 s, c;

    angle &= 0xFFF;
    c = gSinTable[angle + 0x400];
    s = gSinTable[angle];
    m->m[0][0] = c;
    m->m[0][1] = -s;
    m->m[0][2] = 0;
    m->m[1][0] = s;
    m->m[1][1] = c;
    m->m[1][2] = 0;
    m->m[2][0] = 0;
    m->m[2][1] = 0;
    m->m[2][2] = 0x1000;
    return m;
}

/* m = X(r->vx) * Y(r->vy) * Z(r->vz). */
GteMatrix *Gte_RotMatrixXYZ(GteSVector *r, GteMatrix *m) {
    GteMatrix mx;
    GteMatrix my;
    GteMatrix mz;

    Gte_RotMatrixX(r->vx, &mx);
    Gte_RotMatrixY(r->vy, &my);
    Gte_RotMatrixZ(r->vz, &mz);
    Gte_MulMatrix0(&mx, &my, m);
    Gte_MulMatrix(m, &mz);
    return m;
}

/* m = Y(r->vy) * X(r->vx) * Z(r->vz). */
GteMatrix *Gte_RotMatrixYXZ(GteSVector *r, GteMatrix *m) {
    GteMatrix mx;
    GteMatrix my;
    GteMatrix mz;

    Gte_RotMatrixX(r->vx, &mx);
    Gte_RotMatrixY(r->vy, &my);
    Gte_RotMatrixZ(r->vz, &mz);
    Gte_MulMatrix0(&my, &mx, m);
    Gte_MulMatrix(m, &mz);
    return m;
}

/* m = Z(r->vz) * Y(r->vy) * X(r->vx). */
GteMatrix *Gte_RotMatrixZYX(GteSVector *r, GteMatrix *m) {
    GteMatrix mx;
    GteMatrix my;
    GteMatrix mz;

    Gte_RotMatrixX(r->vx, &mx);
    Gte_RotMatrixY(r->vy, &my);
    Gte_RotMatrixZ(r->vz, &mz);
    Gte_MulMatrix0(&mz, &my, m);
    Gte_MulMatrix(m, &mx);
    return m;
}

/* RT = m (rotation part). */
void Gte_SetRotMatrix(GteMatrix *m) {
    gGteRt.m[0][0] = m->m[0][0];
    gGteRt.m[0][1] = m->m[0][1];
    gGteRt.m[0][2] = m->m[0][2];
    gGteRt.m[1][0] = m->m[1][0];
    gGteRt.m[1][1] = m->m[1][1];
    gGteRt.m[1][2] = m->m[1][2];
    gGteRt.m[2][0] = m->m[2][0];
    gGteRt.m[2][1] = m->m[2][1];
    gGteRt.m[2][2] = m->m[2][2];
}

/* TR = m's translation. */
void Gte_SetTransMatrix(GteMatrix *m) {
    gGteRt.t[0] = m->t[0];
    gGteRt.t[1] = m->t[1];
    gGteRt.t[2] = m->t[2];
}
