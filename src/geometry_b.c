/* 0x347760-0x34A7A8 (module geometry, docs/game_overview.md section 4): the fighter skeleton's float matrices
 * (func_003478F0 and its secondary joints func_00347E28), matrix blends towards a direction (func_00348988 family),
 * fixed / float matrix conversions, and hand-written VU0 rotation helpers (func_00349CF8 .. func_0034A184). The .lit4
 * pool (0x5F573C-0x5F57EC) has func_0034A570's words (0x5F57C0) in front of func_00349D70's although its code is
 * later: the words of the VU0 routines after 0x5F57C8 are LIT4_WORD_AT(1, ...), after func_0034A570's C literals. */
#include "common.h"
#include "gte.h"

typedef f32 Vu0Matrix[4][4];
typedef f32 FVec4[4] __attribute__((aligned(16)));

/* A 16-byte .rodata vector copied with ld / sd. */
typedef struct {
    f32 v[4];
} __attribute__((aligned(8))) F4;

/* Float joint of a fighter (0xDB0 + k * 0xC0): world, view (D_004334C0 * world), the three rows of screen * world,
 * the parent's world matrix (as in src/scene3d_d.c). */
typedef struct {
    /* 0x00 */ Vu0Matrix m0;
    /* 0x40 */ Vu0Matrix m1;
    /* 0x80 */ f32 m2[3][4];
    /* 0xB0 */ Vu0Matrix *parent;
    /* 0xB4 */ s32 padB4[3];
} FJoint; /* size 0xC0 */

typedef struct {
    /* 0x0 */ u8 pad0[4];
    /* 0x4 */ s32 unk4;
} McModelHdr;

extern Vu0Matrix D_004334C0;
extern Vu0Matrix D_00433400;
extern s32 D_00437E70[];
extern u8 D_00416A18[];
extern u128 D_0046AFD0[];
extern f32 D_01FC49E0[];
extern const F4 D_00522DE0;
extern const F4 D_00522F80;

f32 atan2f(f32 y, f32 x);
f32 asinf(f32 x);
f32 sqrtf(f32 x);
f32 fabsf(f32 x);
f32 fmodf(f32 x, f32 y);
f32 sinf(f32 x);
f32 cosf(f32 x);
void Mtx_Unit(Vu0Matrix m);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0InversMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0RotMatrixX(Vu0Matrix m0, Vu0Matrix m1, f32 rx);
void sceVu0RotMatrixY(Vu0Matrix m0, Vu0Matrix m1, f32 ry);
void sceVu0RotMatrixZ(Vu0Matrix m0, Vu0Matrix m1, f32 rz);
void sceVu0ApplyMatrix(f32 *v0, Vu0Matrix m, f32 *v1);
void sceVu0OuterProduct(f32 *v0, f32 *v1, f32 *v2);
f32 sceVu0InnerProduct(f32 *v0, f32 *v1);
void sceVu0ScaleVector(f32 *v0, f32 *v1, f32 s);
void sceVu0AddVector(f32 *v0, f32 *v1, f32 *v2);
void sceVu0SubVector(f32 *v0, f32 *v1, f32 *v2);
void func_0034A868(f32 *out, f32 *in);
McModelHdr *func_003B87B0(s32 id);
f32 *func_003B87F8(s16 id);
void *func_0034FC88(s32 chara, s32 k);
void func_003DD918(void *f);
void func_003DE0D8(u8 *f);
void Light_GetFighterMatrices(s32 id, u128 *m);
void *Fighter_GetOpponent(void *f);
void func_00351E98(void *f, f32 *v, Vu0Matrix scr);
void func_0034D258(void *joints, void *mats, s32 chara, s32 parts, Vu0Matrix scr);
void func_003DE4E0(u8 *f, u8 *bones, u8 *mats, s32 side, void *view);
void func_00351AA0(void *f, void *mdl, void *p, void *mats, Vu0Matrix scr);

void func_00347E28(s32 chara, void *tbl, Vu0Matrix *L);
void func_00348988(Vu0Matrix d, Vu0Matrix s);
void func_00348C68(Vu0Matrix d, Vu0Matrix s);
void func_00348F20(Vu0Matrix d, Vu0Matrix s);
void func_003491D8(Vu0Matrix d, Vu0Matrix s);
void func_00349490(Vu0Matrix d, Vu0Matrix s);
void func_00349748(Vu0Matrix d, Vu0Matrix s);
void func_003499D0(Vu0Matrix d, Vu0Matrix s);
void func_00349D70(Vu0Matrix m, f32 *ang);
void func_00349EE0(Vu0Matrix m, f32 *ang);
void func_0034A050(Vu0Matrix m, f32 a);
void func_0034A0D0(Vu0Matrix m, f32 a);
void func_0034A2B8(Vu0Matrix d, GteMatrix *g);
void func_0034A3B8(Vu0Matrix d, GteMatrix *g);
f32 func_0034A570(f32 a);
void func_0034A710(Vu0Matrix d, Vu0Matrix s);

/* Byte-offset views of the local matrix array of func_00347E28. */
#define LM(o) (*(Vu0Matrix *)((u8 *)L + (o)))
#define LV(o) ((f32 *)((u8 *)L + (o)))

/* NON_MATCHING: 31 lines: the u128 copy from f + 0x8B80 shares the 0x8000 base with the later float loads (original: its own ori 0x8B80 / addu, loads scheduled after it) */
#if 0
/* Builds the local joint matrices of fighter f from its angle table (func_003B87F8): three runs of joints, with
 * the scale word at 0xB090 set between them; stores the fighter's reference vector and a height. */
void func_00347760(u8 *f) {
    f32 *ang = func_003B87F8(*(s16 *)(f + 0x20));
    s32 i;
    f32 h;

    for (i = 0x40; i < 0x4F; i++) {
        func_00349D70(*(Vu0Matrix *)(f + ((i << 6) + 0x87D0)), ang + i * 3);
    }
    for (i = 0x27; i < 0x36; i++) {
        func_00349D70(*(Vu0Matrix *)(f + ((i << 6) + 0x87D0)), ang + i * 3);
    }
    *(f32 *)(f + 0xB090) = 1.0f;
    for (i = 7; i < 0x1D; i++) {
        func_00349D70(*(Vu0Matrix *)(f + ((i << 6) + 0x87D0)), ang + i * 3);
    }
    D_0046AFD0[*(s16 *)(f + 0x20)] = *(u128 *)(f + 0x8B80);
    h = *(s32 *)(f + 0x500) * 1150.0f;
    h = h * (1.0f / 4096.0f);
    D_01FC49E0[*(s16 *)(f + 0x20)] = h - *(f32 *)(f + 0x9BC0) - *(f32 *)(f + 0x9C40) - *(f32 *)(f + 0x9D00)
        - *(f32 *)(f + 0x9DC0) + *(f32 *)(f + 0x9E04);
}
#else
LIT4_WORD(D_005F573C, 0x448FC000);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00347760);
#endif

INCLUDE_RODATA("asm/nonmatchings/geometry_b", D_00522DE0);
/* NON_MATCHING: about 230 lines: m2c-based draft, frame layout and register allocation differ (two inlined joint loops, 98-way switch) */
#if 0
/* NON_MATCHING: draft (m2c-based, not compared yet): the per-frame skeleton of fighter f. */
void func_003478F0(u8 *f) {
    Vu0Matrix rot;
    Vu0Matrix scr;
    Vu0Matrix tmp;
    Vu0Matrix tmp2;
    Vu0Matrix *local;
    Vu0Matrix *last;
    s16 chara;
    McModelHdr *mdl;
    s32 parts;
    f32 *ang;
    s32 i;
    s32 k;
    void *g;
    F4 c;
    f32 h;
    f32 t;
    FJoint *j;

    chara = *(s16 *)(f + 0x20);
    mdl = func_003B87B0(*(s16 *)(f + 0x22));
    parts = mdl->unk4;
    func_0034A2B8(*(Vu0Matrix *)(f + 0x560), (GteMatrix *)(f + 0x508));
    local = (Vu0Matrix *)(f + 0x87D0);
    ang = (f32 *)func_003B87F8(chara);
    last = local + 98;
    func_00349D70(local[2], ang + 6);
    for (i = 0; i < 98; i++) {
        switch (i) {
        case 0x00:
        case 0x01:
        case 0x1F:
        case 0x38:
        case 0x3B:
        case 0x3F:
        case 0x4F:
        case 0x51:
        case 0x54:
        case 0x57:
        case 0x5A:
        case 0x5D:
        case 0x60:
            k = D_00437E70[i];
            func_0034A2B8(local[i], (GteMatrix *)(f + 0x630 + k * 0x50 + 4));
            break;
        case 0x22:
        case 0x26:
            k = D_00437E70[i];
            g = func_0034FC88(chara, k);
            if (g == NULL) {
                g = f + 0x630 + k * 0x50 + 4;
            }
            func_0034A2B8(local[i], (GteMatrix *)g);
            break;
        case 0x05:
            k = D_00437E70[i];
            func_0034A3B8(local[i], (GteMatrix *)(f + 0x630 + k * 0x50 + 4));
            break;
        case 0x1D:
        case 0x1E:
        case 0x36:
        case 0x37:
            k = D_00437E70[i];
            func_0034A2B8(local[i], (GteMatrix *)(f + 0x630 + k * 0x50 + 4));
            local[i][3][0] -= local[2][3][0];
            local[i][3][1] -= local[2][3][1];
            local[i][3][2] -= local[2][3][2];
            break;
        case 0x03:
        case 0x04:
        case 0x06:
        case 0x25:
        case 0x3E:
        case 0x50:
        case 0x53:
        case 0x56:
        case 0x58:
        case 0x59:
        case 0x5C:
        case 0x5F:
        case 0x61:
            func_00349D70(local[i], ang + i * 3);
            break;
        }
    }
    func_003DD918(f);
    func_003DE0D8(f);
    func_0034A050(rot, 3.141593f);
    sceVu0MulMatrix(*(Vu0Matrix *)(f + 0x560), rot, *(Vu0Matrix *)(f + 0x560));
    func_00347E28(chara, D_00416A18, local);
    Light_GetFighterMatrices(*(s16 *)(f + 0x1E), (u128 *)scr);
    j = (FJoint *)(f + 0xDB0);
    for (i = 98; i > 0; i--, j++) {
        sceVu0MulMatrix(j->m0, *j->parent, local[98 - i]);
        sceVu0MulMatrix(j->m1, D_004334C0, j->m0);
        sceVu0MulMatrix(tmp, scr, j->m0);
        *(u128 *)j->m2[0] = *(u128 *)tmp[0];
        *(u128 *)j->m2[1] = *(u128 *)tmp[1];
        *(u128 *)j->m2[2] = *(u128 *)tmp[2];
    }
    *(u128 *)tmp[0] = *(u128 *)((u8 *)Fighter_GetOpponent(f) + 0x11A0);
    func_00351E98(f, tmp[0], scr);
    func_0034D258(f + 0xDB0, last, chara, parts, scr);
    func_003DE4E0(f, f + 0xDB0, (u8 *)last, chara, scr);
    func_00351AA0(f, mdl, f + 0x5730, last, scr);
    c = D_00522DE0;
    h = D_01FC49E0[*(s16 *)(f + 0x20)];
    sceVu0ApplyMatrix(tmp[1], *(Vu0Matrix *)(f + 0x4FB0), c.v);
    if (tmp[1][1] < h) {
        t = (h - tmp[1][1]) / c.v[0];
        if (t > 1.0f) {
            t = 1.0f;
        }
        func_0034A0D0(*(Vu0Matrix *)(f + 0x9DD0), 3.1415927f - asinf(-*(f32 *)(f + 0x4EF4)) * t);
    }
    sceVu0ApplyMatrix(tmp[1], *(Vu0Matrix *)(f + 0x5670), c.v);
    if (tmp[1][1] < h) {
        t = (h - tmp[1][1]) / c.v[0];
        if (t > 1.0f) {
            t = 1.0f;
        }
        func_0034A0D0(*(Vu0Matrix *)(f + 0xA010), 3.1415927f - asinf(-*(f32 *)(f + 0x55B4)) * t);
    }
    j = (FJoint *)(f + 0xDB0);
    local = (Vu0Matrix *)(f + 0x87D0);
    for (i = 98; i > 0; i--, j++, local++) {
        sceVu0MulMatrix(j->m0, *j->parent, *local);
        sceVu0MulMatrix(j->m1, D_004334C0, j->m0);
        sceVu0MulMatrix(tmp2, scr, j->m0);
        *(u128 *)j->m2[0] = *(u128 *)tmp2[0];
        *(u128 *)j->m2[1] = *(u128 *)tmp2[1];
        *(u128 *)j->m2[2] = *(u128 *)tmp2[2];
    }
    sceVu0ApplyMatrix(tmp[3], D_00433400, (f32 *)(f + 0x590));
    *(f32 *)(f + 0x620) = tmp[3][2];
}
#else
INCLUDE_RODATA("asm/nonmatchings/geometry_b", jtbl_00522DF0);
LIT4_WORD(D_005F5740, 0x40490FDC);
LIT4_WORD(D_005F5744, 0x40490FDB);
LIT4_WORD(D_005F5748, 0x40490FDB);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_003478F0);
#endif

INCLUDE_RODATA("asm/nonmatchings/geometry_b", D_00522F80);
/* NON_MATCHING: about 115 lines: m2c-based draft, stack slots and register allocation differ */
#if 0
/* NON_MATCHING-draft body: the secondary joints of the skeleton (look-at frames, blends, twist halves). */
void func_00347E28(s32 chara, void *tbl, Vu0Matrix *L) {
    FVec4 v;
    Vu0Matrix m;
    FVec4 ang;
    F4 c;
    FVec4 keep;
    Vu0Matrix r;
    Vu0Matrix w;
    Vu0Matrix n;
    f32 a;
    f32 b;
    f32 x;
    f32 y;
    f32 z;
    f32 len;
    f32 inv;
    f32 a0;
    f32 a1;
    f32 b0;
    f32 b1;
    f32 c0;
    f32 c1;
    f32 e0;
    f32 e1;

    Mtx_Unit(m);
    a = atan2f(LV(0x744)[0], -LV(0x748)[0]);
    if (a < -0.17453295f) {
        sceVu0RotMatrixX(m, LM(0x740), -0.17453295f - a);
    } else {
        sceVu0CopyMatrix(m, LM(0x740));
    }
    sceVu0MulMatrix(m, m, LM(0x7C0));
    v[0] = m[3][0] - LV(0xF0)[0];
    v[1] = m[3][1] - LV(0xF0)[1];
    v[2] = m[3][2] - LV(0xF0)[2];
    func_0034A868(LV(0xC0), v);
    v[0] = 0.0f;
    v[1] = 1.0f;
    v[2] = 0.0f;
    sceVu0OuterProduct(LV(0xD0), v, LV(0xC0));
    sceVu0OuterProduct(LV(0xE0), LV(0xC0), LV(0xD0));
    a = atan2f(LV(0xD84)[0], LV(0xD88)[0]);
    if (a < -0.17453295f) {
        sceVu0RotMatrixX(m, LM(0xD80), -(-0.17453295f - a));
    } else {
        sceVu0CopyMatrix(m, LM(0xD80));
    }
    sceVu0MulMatrix(m, m, LM(0xE00));
    v[0] = m[3][0] - LV(0x130)[0];
    v[1] = m[3][1] - LV(0x130)[1];
    v[2] = m[3][2] - LV(0x130)[2];
    func_0034A868(LV(0x100), v);
    v[0] = 0.0f;
    v[1] = 1.0f;
    v[2] = 0.0f;
    sceVu0OuterProduct(LV(0x110), v, LV(0x100));
    sceVu0OuterProduct(LV(0x120), LV(0x100), LV(0x110));
    sceVu0CopyMatrix(m, LM(0x13C0));
    func_00349748(m, m);
    sceVu0InversMatrix(m, m);
    sceVu0MulMatrix(m, m, LM(0x13C0));
    a = atan2f(m[1][2], m[1][1]);
    func_003499D0(m, LM(0x40));
    sceVu0InversMatrix(m, m);
    sceVu0MulMatrix(m, m, LM(0x40));
    a = func_0034A570(a + atan2f(m[1][2], m[1][1]));
    func_0034A050(m, -a * 0.5f);
    sceVu0MulMatrix(LM(0x40), LM(0x40), m);
    *(u128 *)v = *(u128 *)LV(0xB0);
    func_0034A050(LM(0x80), a * 0.5f);
    *(u128 *)LV(0xB0) = *(u128 *)v;
    c = D_00522F80;
    sceVu0CopyMatrix(r, LM(0x140));
    func_00349D70(w, c.v);
    sceVu0InversMatrix(m, w);
    *(u128 *)keep = *(u128 *)LV(0x170);
    sceVu0MulMatrix(m, m, LM(0x140));
    z = -m[0][2];
    v[2] = 0.0f;
    v[1] = 0.0f;
    v[0] = 0.0f;
    if (fabsf(1.0f - z) < 1e-05f) {
        x = m[1][0];
        y = m[2][0];
        v[1] = atan2f(z, 0.0f);
        v[2] = func_0034A570(v[0] - atan2f(x, y));
    } else if (fabsf(-1.0f - z) < 1e-05f) {
        x = -m[1][0];
        v[1] = atan2f(z, 0.0f);
        y = -m[2][0];
        v[2] = func_0034A570(v[0] - atan2f(x, y));
    } else {
        len = sqrtf(m[0][0] * m[0][0] + m[0][1] * m[0][1]);
        inv = 1.0f / len;
        a0 = atan2f(m[1][2] * inv, m[2][2] * inv);
        a1 = atan2f(-m[1][2] * inv, -m[2][2] * inv);
        b0 = atan2f(z, len);
        b1 = atan2f(z, -len);
        c0 = atan2f(m[0][1] * inv, m[0][0] * inv);
        c1 = atan2f(-m[0][1] * inv, -m[0][0] * inv);
        e0 = fabsf(func_0034A570(v[0] - a0)) * 8.0f + fabsf(func_0034A570(v[1] - b0))
            + fabsf(func_0034A570(v[2] - c0));
        e1 = fabsf(func_0034A570(v[0] - a1)) * 8.0f + fabsf(func_0034A570(v[1] - b1))
            + fabsf(func_0034A570(v[2] - c1));
        if (e0 < e1) {
            v[0] = a0;
            v[1] = b0;
            v[2] = c0;
        } else {
            v[0] = a1;
            v[1] = b1;
            v[2] = c1;
        }
    }
    ang[0] = func_0034A570(v[0]) * 0.2f;
    ang[1] = func_0034A570(v[1]) * 0.6f;
    ang[2] = func_0034A570(v[2]) * 0.4f;
    func_00349D70(LM(0x140), ang);
    sceVu0MulMatrix(LM(0x140), w, LM(0x140));
    *(u128 *)LV(0x170) = *(u128 *)keep;
    *(u128 *)keep = *(u128 *)LV(0x1B0);
    sceVu0InversMatrix(m, LM(0x140));
    sceVu0MulMatrix(LM(0x180), m, r);
    *(u128 *)LV(0x1B0) = *(u128 *)keep;
    func_00348C68(LM(0x780), LM(0x740));
    sceVu0InversMatrix(m, LM(0x740));
    sceVu0MulMatrix(m, m, LM(0x780));
    sceVu0MulMatrix(m, LM(0x7C0), m);
    func_00348988(LM(0x800), m);
    sceVu0AddVector(m[0], LM(0x7C0)[0], LM(0x800)[0]);
    sceVu0AddVector(m[1], LV(0x7D0), LV(0x810));
    sceVu0AddVector(m[2], LV(0x7E0), LV(0x820));
    *(u128 *)m[3] = *(u128 *)LV(0x7F0);
    func_0034A710(LM(0x840), m);
    func_00348F20(LM(0xDC0), LM(0xD80));
    sceVu0InversMatrix(m, LM(0xD80));
    sceVu0MulMatrix(m, m, LM(0xDC0));
    sceVu0MulMatrix(m, LM(0xE00), m);
    func_00348988(LM(0xE40), m);
    sceVu0AddVector(m[0], LM(0xE00)[0], LM(0xE40)[0]);
    sceVu0AddVector(m[1], LV(0xE10), LV(0xE50));
    sceVu0AddVector(m[2], LV(0xE20), LV(0xE60));
    *(u128 *)m[3] = *(u128 *)LV(0xE30);
    func_0034A710(LM(0xE80), m);
    func_0034A0D0(LM(0x940), atan2f(LV(0x884)[0], LV(0x880)[0]) * -0.5f);
    func_0034A0D0(LM(0xF80), atan2f(LV(0xEC4)[0], LV(0xEC0)[0]) * -0.5f);
    func_00349490(m, LM(0x980));
    sceVu0InversMatrix(m, m);
    sceVu0MulMatrix(m, m, LM(0x980));
    m[3][2] = 0.0f;
    m[3][1] = 0.0f;
    m[3][0] = 0.0f;
    sceVu0MulMatrix(LM(0x900), LM(0x880), m);
    sceVu0AddVector(m[0], LM(0x880)[0], LM(0x900)[0]);
    sceVu0AddVector(m[1], LV(0x890), LV(0x910));
    sceVu0AddVector(m[2], LV(0x8A0), LV(0x920));
    *(u128 *)m[3] = *(u128 *)LV(0x8B0);
    func_0034A710(LM(0x8C0), m);
    func_00349490(m, LM(0xFC0));
    sceVu0InversMatrix(m, m);
    sceVu0MulMatrix(m, m, LM(0xFC0));
    m[3][2] = 0.0f;
    m[3][1] = 0.0f;
    m[3][0] = 0.0f;
    sceVu0MulMatrix(LM(0xF40), LM(0xEC0), m);
    sceVu0AddVector(m[0], LM(0xEC0)[0], LM(0xF40)[0]);
    sceVu0AddVector(m[1], LV(0xED0), LV(0xF50));
    sceVu0AddVector(m[2], LV(0xEE0), LV(0xF60));
    *(u128 *)m[3] = *(u128 *)LV(0xEF0);
    func_0034A710(LM(0xF00), m);
    func_003491D8(LM(0x1480), LM(0x1440));
    sceVu0AddVector(m[0], LM(0x1440)[0], LM(0x1480)[0]);
    sceVu0AddVector(m[1], LV(0x1450), LV(0x1490));
    sceVu0AddVector(m[2], LV(0x1460), LV(0x14A0));
    *(u128 *)m[3] = *(u128 *)LV(0x1470);
    func_0034A710(LM(0x14C0), m);
    func_003491D8(LM(0x16C0), LM(0x1680));
    sceVu0AddVector(m[0], LM(0x1680)[0], LM(0x16C0)[0]);
    sceVu0AddVector(m[1], LV(0x1690), LV(0x16D0));
    sceVu0AddVector(m[2], LV(0x16A0), LV(0x16E0));
    *(u128 *)m[3] = *(u128 *)LV(0x16B0);
    func_0034A710(LM(0x1700), m);
    sceVu0ScaleVector(n[0], LV(0x15D0), -1.0f);
    *(u128 *)n[1] = *(u128 *)LV(0x15C0);
    *(u128 *)n[2] = *(u128 *)LV(0x15E0);
    *(u128 *)n[3] = *(u128 *)LV(0x15F0);
    func_003491D8(m, n);
    sceVu0InversMatrix(m, m);
    sceVu0MulMatrix(m, m, n);
    m[3][2] = 0.0f;
    m[3][1] = 0.0f;
    m[3][0] = 0.0f;
    sceVu0MulMatrix(LM(0x1540), LM(0x1500), m);
    sceVu0ScaleVector(n[0], LV(0x1810), -1.0f);
    *(u128 *)n[1] = *(u128 *)LV(0x1800);
    *(u128 *)n[2] = *(u128 *)LV(0x1820);
    *(u128 *)n[3] = *(u128 *)LV(0x1830);
    func_003491D8(m, n);
    sceVu0InversMatrix(m, m);
    sceVu0MulMatrix(m, m, n);
    m[3][2] = 0.0f;
    m[3][1] = 0.0f;
    m[3][0] = 0.0f;
    sceVu0MulMatrix(LM(0x1780), LM(0x1740), m);
    func_0034A0D0(LM(0x1580), atan2f(LV(0x1504)[0], LV(0x1500)[0]) * -0.5f);
    func_0034A0D0(LM(0x17C0), atan2f(LV(0x1744)[0], LV(0x1740)[0]) * -0.5f);
    b = atan2f(-LV(0x1480)[0], LV(0x1484)[0]);
    if (b > 0.0f) {
        b -= 6.283186f;
    }
    func_0034A0D0(LM(0x1400), b * -0.5f);
    b = atan2f(-LV(0x16C0)[0], LV(0x16C4)[0]);
    if (b > 0.0f) {
        b -= 6.283186f;
    }
    func_0034A0D0(LM(0x1640), b * -0.5f);
}
#else
LIT4_WORD(D_005F574C, 0xBE32B8C4);
LIT4_WORD(D_005F5750, 0xBE32B8C4);
LIT4_WORD(D_005F5754, 0x3727C5AC);
LIT4_WORD(D_005F5758, 0x3E4CCCCD);
LIT4_WORD(D_005F575C, 0x3F19999A);
LIT4_WORD(D_005F5760, 0x3ECCCCCD);
LIT4_WORD(D_005F5764, 0x40C90FDC);
LIT4_WORD(D_005F5768, 0x40C90FDC);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00347E28);
#endif

/* Blends the rotation of d towards a frame built from s's first row (see the note above the family). */
void func_00348988(Vu0Matrix d, Vu0Matrix s) {
    Vu0Matrix a;
    Vu0Matrix b;
    FVec4 t;
    f32 k;
    f32 w;

    a[0][0] = s[0][0];
    a[0][1] = s[0][1];
    a[0][2] = s[0][2];
    func_0034A868(a[0], a[0]);
    if (fabsf(a[0][2]) < 0.9999f) {
        if (d[0][0] >= 0.0f) {
            a[2][2] = 0.0f;
            a[2][1] = a[0][0];
            a[2][0] = -a[0][1];
        } else {
            a[2][2] = 0.0f;
            a[2][0] = -a[0][1];
            a[2][1] = -a[0][0];
        }
    } else {
        a[2][0] = 0.0f;
        a[2][1] = a[0][2] * a[0][2];
        a[2][2] = -a[0][1] * a[0][2];
    }
    func_0034A868(a[2], a[2]);
    sceVu0OuterProduct(a[1], a[2], a[0]);
    b[0][0] = s[0][0];
    b[0][1] = s[0][1];
    b[0][2] = s[0][2];
    func_0034A868(b[0], b[0]);
    if (fabsf(b[0][1]) < 0.9999f) {
        b[1][1] = 0.0f;
        b[1][0] = b[0][2];
        b[1][2] = -b[0][0];
    } else {
        b[1][0] = 0.0f;
        b[1][1] = b[0][2] * b[0][1];
        b[1][2] = -b[0][1] * b[0][1];
    }
    func_0034A868(b[1], b[1]);
    sceVu0OuterProduct(b[2], b[0], b[1]);
    if (fabsf(s[0][1]) < 0.0001f && fabsf(s[0][2]) < 0.0001f) {
        k = 1.0f;
        w = 0.0f;
    } else {
        w = s[0][2] * s[0][2];
        w = w * w;
        k = 1.0f - w;
    }
    sceVu0ScaleVector(d[0], a[0], k);
    sceVu0ScaleVector(d[1], a[1], k);
    sceVu0ScaleVector(d[2], a[2], k);
    sceVu0ScaleVector(t, b[0], w);
    sceVu0AddVector(d[0], d[0], t);
    sceVu0ScaleVector(t, b[1], w);
    sceVu0AddVector(d[1], d[1], t);
    sceVu0ScaleVector(t, b[2], w);
    sceVu0AddVector(d[2], d[2], t);
    *(u128 *)d[3] = *(u128 *)s[3];
    func_0034A710(d, d);
}

/* NON_MATCHING: 12 lines: the weight block keeps |x| from the test in a register; the original recomputes abs.s in the else */
#if 0
void func_00348C68(Vu0Matrix d, Vu0Matrix s) {
    Vu0Matrix a;
    Vu0Matrix b;
    FVec4 t;
    f32 k;
    f32 w;
    f32 x;
    f32 y;
    f32 sum;

    a[0][0] = s[0][0];
    a[0][1] = s[0][1];
    a[0][2] = s[0][2];
    func_0034A868(a[0], a[0]);
    if (fabsf(a[0][0]) < 0.9999f) {
        a[2][0] = 0.0f;
        a[2][2] = a[0][1];
        a[2][1] = -a[0][2];
    } else {
        a[2][2] = 0.0f;
        a[2][1] = a[0][0] * a[0][0];
        a[2][0] = -a[0][1] * a[0][0];
    }
    func_0034A868(a[2], a[2]);
    sceVu0OuterProduct(a[1], a[2], a[0]);
    b[0][0] = s[0][0];
    b[0][1] = s[0][1];
    b[0][2] = s[0][2];
    func_0034A868(b[0], b[0]);
    if (fabsf(b[0][1]) < 0.9999f) {
        b[1][1] = 0.0f;
        b[1][0] = b[0][2];
        b[1][2] = -b[0][0];
    } else {
        b[1][2] = 0.0f;
        b[1][0] = b[0][1] * b[0][1];
        b[1][1] = -b[0][0] * b[0][1];
    }
    func_0034A868(b[1], b[1]);
    sceVu0OuterProduct(b[2], b[0], b[1]);
    x = s[0][1];
    y = s[0][0];
    if (fabsf(x) < 0.0001f && fabsf(y) < 0.0001f) {
        k = 1.0f;
        w = 0.0f;
    } else {
        sum = fabsf(x) + fabsf(y);
        k = fabsf(x) / sum;
        w = fabsf(y) / sum;
    }
    sceVu0ScaleVector(d[0], a[0], k);
    sceVu0ScaleVector(d[1], a[1], k);
    sceVu0ScaleVector(d[2], a[2], k);
    sceVu0ScaleVector(t, b[0], w);
    sceVu0AddVector(d[0], d[0], t);
    sceVu0ScaleVector(t, b[1], w);
    sceVu0AddVector(d[1], d[1], t);
    sceVu0ScaleVector(t, b[2], w);
    sceVu0AddVector(d[2], d[2], t);
    *(u128 *)d[3] = *(u128 *)s[3];
    func_0034A710(d, d);
}
#else
LIT4_WORD(D_005F5778, 0x3F7FF972);
LIT4_WORD(D_005F577C, 0x3F7FF972);
LIT4_WORD(D_005F5780, 0x38D1B717);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00348C68);
#endif

/* NON_MATCHING: 12 lines: as func_00348C68 (|x| reused from the test where the original recomputes it) */
#if 0
void func_00348F20(Vu0Matrix d, Vu0Matrix s) {
    Vu0Matrix a;
    Vu0Matrix b;
    FVec4 t;
    f32 k;
    f32 w;
    f32 x;
    f32 y;
    f32 sum;

    a[0][0] = s[0][0];
    a[0][1] = s[0][1];
    a[0][2] = s[0][2];
    func_0034A868(a[0], a[0]);
    if (fabsf(a[0][0]) < 0.9999f) {
        a[2][0] = 0.0f;
        a[2][1] = a[0][2];
        a[2][2] = -a[0][1];
    } else {
        a[2][2] = 0.0f;
        a[2][1] = a[0][0] * a[0][0];
        a[2][0] = -a[0][1] * a[0][0];
    }
    func_0034A868(a[2], a[2]);
    sceVu0OuterProduct(a[1], a[2], a[0]);
    b[0][0] = s[0][0];
    b[0][1] = s[0][1];
    b[0][2] = s[0][2];
    func_0034A868(b[0], b[0]);
    if (fabsf(b[0][1]) < 0.9999f) {
        b[1][1] = 0.0f;
        b[1][0] = b[0][2];
        b[1][2] = -b[0][0];
    } else {
        b[1][2] = 0.0f;
        b[1][0] = b[0][1] * b[0][1];
        b[1][1] = -b[0][0] * b[0][1];
    }
    func_0034A868(b[1], b[1]);
    sceVu0OuterProduct(b[2], b[0], b[1]);
    x = s[0][1];
    y = s[0][0];
    if (fabsf(x) < 0.0001f && fabsf(y) < 0.0001f) {
        k = 1.0f;
        w = 0.0f;
    } else {
        sum = fabsf(x) + fabsf(y);
        k = fabsf(x) / sum;
        w = fabsf(y) / sum;
    }
    sceVu0ScaleVector(d[0], a[0], k);
    sceVu0ScaleVector(d[1], a[1], k);
    sceVu0ScaleVector(d[2], a[2], k);
    sceVu0ScaleVector(t, b[0], w);
    sceVu0AddVector(d[0], d[0], t);
    sceVu0ScaleVector(t, b[1], w);
    sceVu0AddVector(d[1], d[1], t);
    sceVu0ScaleVector(t, b[2], w);
    sceVu0AddVector(d[2], d[2], t);
    *(u128 *)d[3] = *(u128 *)s[3];
    func_0034A710(d, d);
}
#else
LIT4_WORD(D_005F5784, 0x3F7FF972);
LIT4_WORD(D_005F5788, 0x3F7FF972);
LIT4_WORD(D_005F578C, 0x38D1B717);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00348F20);
#endif

/* NON_MATCHING: 12 lines: as func_00348C68 (|x| reused from the test where the original recomputes it) */
#if 0
void func_003491D8(Vu0Matrix d, Vu0Matrix s) {
    Vu0Matrix a;
    Vu0Matrix b;
    FVec4 t;
    f32 k;
    f32 w;
    f32 x;
    f32 y;
    f32 sum;

    a[0][0] = s[0][0];
    a[0][1] = s[0][1];
    a[0][2] = s[0][2];
    func_0034A868(a[0], a[0]);
    if (fabsf(a[0][1]) < 0.9999f) {
        a[2][1] = 0.0f;
        a[2][2] = a[0][0];
        a[2][0] = -a[0][2];
    } else {
        a[2][0] = 0.0f;
        a[2][2] = a[0][1] * a[0][1];
        a[2][1] = -a[0][2] * a[0][1];
    }
    func_0034A868(a[2], a[2]);
    sceVu0OuterProduct(a[1], a[2], a[0]);
    b[0][0] = s[0][0];
    b[0][1] = s[0][1];
    b[0][2] = s[0][2];
    func_0034A868(b[0], b[0]);
    if (fabsf(b[0][2]) < 0.9999f) {
        b[1][2] = 0.0f;
        b[1][1] = b[0][0];
        b[1][0] = -b[0][1];
    } else {
        b[1][0] = 0.0f;
        b[1][2] = b[0][1] * b[0][2];
        b[1][1] = -b[0][2] * b[0][2];
    }
    func_0034A868(b[1], b[1]);
    sceVu0OuterProduct(b[2], b[0], b[1]);
    x = s[0][2];
    y = s[0][1];
    if (fabsf(x) < 0.0001f && fabsf(y) < 0.0001f) {
        k = 1.0f;
        w = 0.0f;
    } else {
        sum = fabsf(x) + fabsf(y);
        k = fabsf(x) / sum;
        w = fabsf(y) / sum;
    }
    sceVu0ScaleVector(d[0], a[0], k);
    sceVu0ScaleVector(d[1], a[1], k);
    sceVu0ScaleVector(d[2], a[2], k);
    sceVu0ScaleVector(t, b[0], w);
    sceVu0AddVector(d[0], d[0], t);
    sceVu0ScaleVector(t, b[1], w);
    sceVu0AddVector(d[1], d[1], t);
    sceVu0ScaleVector(t, b[2], w);
    sceVu0AddVector(d[2], d[2], t);
    *(u128 *)d[3] = *(u128 *)s[3];
    func_0034A710(d, d);
}
#else
LIT4_WORD(D_005F5790, 0x3F7FF972);
LIT4_WORD(D_005F5794, 0x3F7FF972);
LIT4_WORD(D_005F5798, 0x38D1B717);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_003491D8);
#endif

/* NON_MATCHING: 12 lines: as func_00348C68 (|x| reused from the test where the original recomputes it) */
#if 0
void func_00349490(Vu0Matrix d, Vu0Matrix s) {
    Vu0Matrix a;
    Vu0Matrix b;
    FVec4 t;
    f32 k;
    f32 w;
    f32 x;
    f32 y;
    f32 sum;

    a[0][0] = s[0][0];
    a[0][1] = s[0][1];
    a[0][2] = s[0][2];
    func_0034A868(a[0], a[0]);
    if (fabsf(a[0][2]) < 0.9999f) {
        a[2][2] = 0.0f;
        a[2][0] = a[0][1];
        a[2][1] = -a[0][0];
    } else {
        a[2][0] = 0.0f;
        a[2][2] = a[0][1] * a[0][2];
        a[2][1] = -a[0][2] * a[0][2];
    }
    func_0034A868(a[2], a[2]);
    sceVu0OuterProduct(a[1], a[2], a[0]);
    b[0][0] = s[0][0];
    b[0][1] = s[0][1];
    b[0][2] = s[0][2];
    func_0034A868(b[0], b[0]);
    if (fabsf(b[0][1]) < 0.9999f) {
        b[1][1] = 0.0f;
        b[1][2] = b[0][0];
        b[1][0] = -b[0][2];
    } else {
        b[1][0] = 0.0f;
        b[1][2] = b[0][1] * b[0][1];
        b[1][1] = -b[0][2] * b[0][1];
    }
    func_0034A868(b[1], b[1]);
    sceVu0OuterProduct(b[2], b[0], b[1]);
    x = s[0][1];
    y = s[0][2];
    if (fabsf(x) < 0.0001f && fabsf(y) < 0.0001f) {
        k = 1.0f;
        w = 0.0f;
    } else {
        sum = fabsf(x) + fabsf(y);
        k = fabsf(x) / sum;
        w = fabsf(y) / sum;
    }
    sceVu0ScaleVector(d[0], a[0], k);
    sceVu0ScaleVector(d[1], a[1], k);
    sceVu0ScaleVector(d[2], a[2], k);
    sceVu0ScaleVector(t, b[0], w);
    sceVu0AddVector(d[0], d[0], t);
    sceVu0ScaleVector(t, b[1], w);
    sceVu0AddVector(d[1], d[1], t);
    sceVu0ScaleVector(t, b[2], w);
    sceVu0AddVector(d[2], d[2], t);
    *(u128 *)d[3] = *(u128 *)s[3];
    func_0034A710(d, d);
}
#else
LIT4_WORD(D_005F579C, 0x3F7FF972);
LIT4_WORD(D_005F57A0, 0x3F7FF972);
LIT4_WORD(D_005F57A4, 0x38D1B717);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00349490);
#endif

/* NON_MATCHING: 60 lines: the weights are constant (k 0, w 1) in both arms; the original keeps the dead compare and f20 / f21, this compiles the test away */
#if 0
void func_00349748(Vu0Matrix d, Vu0Matrix s) {
    Vu0Matrix a;
    Vu0Matrix b;
    FVec4 t;
    f32 k;
    f32 w;

    a[0][0] = s[0][0];
    a[0][1] = s[0][1];
    a[0][2] = s[0][2];
    func_0034A868(a[0], a[0]);
    if (fabsf(a[0][2]) < 0.9999f) {
        a[2][2] = 0.0f;
        a[2][1] = a[0][0];
        a[2][0] = -a[0][1];
    } else {
        a[2][1] = 0.0f;
        a[2][0] = a[0][2] * a[0][2];
        a[2][2] = -a[0][0] * a[0][2];
    }
    func_0034A868(a[2], a[2]);
    sceVu0OuterProduct(a[1], a[2], a[0]);
    b[0][0] = s[0][0];
    b[0][1] = s[0][1];
    b[0][2] = s[0][2];
    func_0034A868(b[0], b[0]);
    if (fabsf(b[0][0]) < 0.9999f) {
        b[1][0] = 0.0f;
        b[1][2] = b[0][1];
        b[1][1] = -b[0][2];
    } else {
        b[1][1] = 0.0f;
        b[1][0] = b[0][2] * b[0][2];
        b[1][2] = -b[0][0] * b[0][2];
    }
    func_0034A868(b[1], b[1]);
    sceVu0OuterProduct(b[2], b[0], b[1]);
    if (fabsf(s[0][0]) < 0.0001f && fabsf(s[0][2]) < 0.0001f) {
        k = 0.0f;
        w = 1.0f;
    } else {
        w = 1.0f;
        k = 0.0f;
    }
    sceVu0ScaleVector(d[0], a[0], k);
    sceVu0ScaleVector(d[1], a[1], k);
    sceVu0ScaleVector(d[2], a[2], k);
    sceVu0ScaleVector(t, b[0], w);
    sceVu0AddVector(d[0], d[0], t);
    sceVu0ScaleVector(t, b[1], w);
    sceVu0AddVector(d[1], d[1], t);
    sceVu0ScaleVector(t, b[2], w);
    sceVu0AddVector(d[2], d[2], t);
    *(u128 *)d[3] = *(u128 *)s[3];
    func_0034A710(d, d);
}
#else
LIT4_WORD(D_005F57A8, 0x3F7FF972);
LIT4_WORD(D_005F57AC, 0x3F7FF972);
LIT4_WORD(D_005F57B0, 0x38D1B717);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00349748);
#endif

/* NON_MATCHING: 60 lines: as func_00349748 (dead weight compare and f20 / f21 kept by the original) */
#if 0
void func_003499D0(Vu0Matrix d, Vu0Matrix s) {
    Vu0Matrix a;
    Vu0Matrix b;
    FVec4 t;
    f32 k;
    f32 w;

    a[0][0] = s[0][0];
    a[0][1] = s[0][1];
    a[0][2] = s[0][2];
    func_0034A868(a[0], a[0]);
    if (fabsf(a[0][2]) < 0.9999f) {
        a[2][2] = 0.0f;
        a[2][0] = a[0][1];
        a[2][1] = -a[0][0];
    } else {
        a[2][1] = 0.0f;
        a[2][0] = a[0][2] * a[0][2];
        a[2][2] = -a[0][0] * a[0][2];
    }
    func_0034A868(a[2], a[2]);
    sceVu0OuterProduct(a[1], a[2], a[0]);
    b[0][0] = s[0][0];
    b[0][1] = s[0][1];
    b[0][2] = s[0][2];
    func_0034A868(b[0], b[0]);
    if (fabsf(b[0][0]) < 0.9999f) {
        b[1][0] = 0.0f;
        b[1][2] = b[0][1];
        b[1][1] = -b[0][2];
    } else {
        b[1][1] = 0.0f;
        b[1][0] = b[0][2] * b[0][0];
        b[1][2] = -b[0][0] * b[0][0];
    }
    func_0034A868(b[1], b[1]);
    sceVu0OuterProduct(b[2], b[0], b[1]);
    if (fabsf(s[0][0]) < 0.0001f && fabsf(s[0][2]) < 0.0001f) {
        k = 0.0f;
        w = 1.0f;
    } else {
        w = 1.0f;
        k = 0.0f;
    }
    sceVu0ScaleVector(d[0], a[0], k);
    sceVu0ScaleVector(d[1], a[1], k);
    sceVu0ScaleVector(d[2], a[2], k);
    sceVu0ScaleVector(t, b[0], w);
    sceVu0AddVector(d[0], d[0], t);
    sceVu0ScaleVector(t, b[1], w);
    sceVu0AddVector(d[1], d[1], t);
    sceVu0ScaleVector(t, b[2], w);
    sceVu0AddVector(d[2], d[2], t);
    *(u128 *)d[3] = *(u128 *)s[3];
    func_0034A710(d, d);
}
#else
LIT4_WORD(D_005F57B4, 0x3F7FF972);
LIT4_WORD(D_005F57B8, 0x3F7FF972);
LIT4_WORD(D_005F57BC, 0x38D1B717);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_003499D0);
#endif

/* For n joints from j: world = parent * local, view = D_004334C0 * world, the screen rows = screen * world. */
void func_00349C50(FJoint *j, Vu0Matrix *local, Vu0Matrix screen) {
    Vu0Matrix m;
    s32 n = 98;

    do {
        n--;
        sceVu0MulMatrix(j->m0, *j->parent, *local);
        local++;
        sceVu0MulMatrix(j->m1, D_004334C0, j->m0);
        sceVu0MulMatrix(m, screen, j->m0);
        *(u128 *)j->m2[0] = *(u128 *)m[0];
        *(u128 *)j->m2[1] = *(u128 *)m[1];
        *(u128 *)j->m2[2] = *(u128 *)m[2];
        j++;
    } while (n > 0);
}

/* NON_MATCHING: hand-written VU0 assembly: sine / cosine kernel with a register interface (angle in vf3x, base in
 * vf19, sign flag in a3; result in vf4xy, the series coefficients at D_0046B040), not callable from C. The C below
 * only states what it computes. */
#if 0
void func_00349CF8(f32 a, s32 neg, f32 *out) {
    f32 s = sinf(a);
    f32 c = sqrtf(1.0f - s * s);

    out[0] = s;
    out[1] = neg ? -c : c;
}
#else
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00349CF8);
#endif

/* NON_MATCHING: hand-written VU0 assembly (jumps into its own labels, calls func_00349CF8 with ra kept in a2, the
 * three angles turned into rotations about X, Y and Z multiplied in the VU0); the C states what it computes. */
#if 0
void func_00349D70(Vu0Matrix m, f32 *ang) {
    Vu0Matrix r;

    /* rows 0-2 and m[3][3] only: the translation in row 3 is kept (port: port/src/vu0_pc.c) */
    Mtx_Unit(r);
    sceVu0RotMatrixX(r, r, ang[0]);
    sceVu0RotMatrixY(r, r, ang[1]);
    sceVu0RotMatrixZ(r, r, ang[2]);
    *(u128 *)m[0] = *(u128 *)r[0];
    *(u128 *)m[1] = *(u128 *)r[1];
    *(u128 *)m[2] = *(u128 *)r[2];
    m[3][3] = 1.0f;
}
#else
LIT4_WORD_AT(1, D_005F57CC, 0x3FC90FDB);
LIT4_WORD_AT(1, D_005F57D0, 0x3FC90FDB);
LIT4_WORD_AT(1, D_005F57D4, 0x3FC90FDB);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00349D70);
#endif

/* NON_MATCHING: hand-written VU0 assembly, as func_00349D70 with the rotations in another order. */
#if 0
void func_00349EE0(Vu0Matrix m, f32 *ang) {
    Vu0Matrix r;

    /* rows 0-2 only: row 3 is kept (port: port/src/vu0_pc.c) */
    Mtx_Unit(r);
    sceVu0RotMatrixZ(r, r, ang[2]);
    sceVu0RotMatrixY(r, r, ang[1]);
    sceVu0RotMatrixX(r, r, ang[0]);
    *(u128 *)m[0] = *(u128 *)r[0];
    *(u128 *)m[1] = *(u128 *)r[1];
    *(u128 *)m[2] = *(u128 *)r[2];
}
#else
LIT4_WORD_AT(1, D_005F57D8, 0x3FC90FDB);
LIT4_WORD_AT(1, D_005F57DC, 0x3FC90FDB);
LIT4_WORD_AT(1, D_005F57E0, 0x3FC90FDB);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_00349EE0);
#endif

/* NON_MATCHING: hand-written VU0 assembly: the rotation matrix about X by a (cosine through func_00349CF8 at
 * pi/2 - |a|). */
#if 0
void func_0034A050(Vu0Matrix m, f32 a) {
    Mtx_Unit(m);
    m[1][1] = cosf(a);
    m[1][2] = sinf(a);
    m[2][1] = -sinf(a);
    m[2][2] = cosf(a);
}
#else
LIT4_WORD_AT(1, D_005F57E4, 0x3FC90FDB);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_0034A050);
#endif

/* NON_MATCHING: hand-written VU0 assembly: the rotation matrix about Z by a (three rows, the fourth untouched). */
#if 0
void func_0034A0D0(Vu0Matrix m, f32 a) {
    m[0][0] = cosf(a);
    m[0][1] = sinf(a);
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;
    m[1][0] = -sinf(a);
    m[1][1] = cosf(a);
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = 0.0f;
}
#else
LIT4_WORD_AT(1, D_005F57E8, 0x3FC90FDB);
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_0034A0D0);
#endif

/* NON_MATCHING: hand-written VU0 assembly: x squared once per bit of n above the lowest (x to the power n for a
 * power of two n); its `jr ra` delay slot is the first instruction of func_0034A180. */
#if 0
f32 func_0034A148(f32 x, s32 n) {
    u32 k = (u32)n >> 1;

    do {
        k >>= 1;
        x *= x;
    } while (k != 0);
    return x;
}
#else
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_0034A148);
#endif

/* NON_MATCHING: hand-written VU0 assembly, the body of func_0034A180 (whose first instruction sits in the delay slot
 * of func_0034A148): d = a * k + b * (1 - k) for the three rotation rows. */
#if 0
void func_0034A184(Vu0Matrix d, Vu0Matrix a, Vu0Matrix b, f32 k) {
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            d[i][j] = a[i][j] * k + b[i][j] * (1.0f - k);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/geometry_b", func_0034A184);
#endif

/* 4x3 (rows of three) to a 4x4 matrix: transposed rotation, translation, w = 1. */
void func_0034A1D0(Vu0Matrix d, f32 s[4][3]) {
    d[0][0] = s[0][0];
    d[0][1] = s[1][0];
    d[0][2] = s[2][0];
    d[0][3] = 0.0f;
    d[1][0] = s[0][1];
    d[1][1] = s[1][1];
    d[1][2] = s[2][1];
    d[1][3] = 0.0f;
    d[2][0] = s[0][2];
    d[2][1] = s[1][2];
    d[2][2] = s[2][2];
    d[2][3] = 0.0f;
    d[3][0] = s[3][0];
    d[3][1] = s[3][1];
    d[3][2] = s[3][2];
    d[3][3] = 1.0f;
}

/* 4x4 to 4x3: the transposed rotation and the translation. */
void func_0034A250(f32 d[4][3], Vu0Matrix s) {
    d[0][0] = s[0][0];
    d[0][1] = s[1][0];
    d[0][2] = s[2][0];
    d[1][0] = s[0][1];
    d[1][1] = s[1][1];
    d[1][2] = s[2][1];
    d[2][0] = s[0][2];
    d[2][1] = s[1][2];
    d[2][2] = s[2][2];
    d[3][0] = s[3][0];
    d[3][1] = s[3][1];
    d[3][2] = s[3][2];
}

/* GteMatrix (1.0 = 0x1000) to a float matrix (rotation transposed, translation, w = 1). */
void func_0034A2B8(Vu0Matrix d, GteMatrix *g) {
    f32 k = 1.0f / 4096.0f;

    d[0][0] = g->m[0][0] * k;
    d[0][1] = g->m[1][0] * k;
    d[0][2] = g->m[2][0] * k;
    d[0][3] = 0.0f;
    d[1][0] = g->m[0][1] * k;
    d[1][1] = g->m[1][1] * k;
    d[1][2] = g->m[2][1] * k;
    d[1][3] = 0.0f;
    d[2][0] = g->m[0][2] * k;
    d[2][1] = g->m[1][2] * k;
    d[2][2] = g->m[2][2] * k;
    d[2][3] = 0.0f;
    d[3][0] = g->t[0];
    d[3][1] = g->t[1];
    d[3][2] = g->t[2];
    d[3][3] = 1.0f;
}

/* The rotation part of func_0034A2B8. */
void func_0034A3B8(Vu0Matrix d, GteMatrix *g) {
    f32 k = 1.0f / 4096.0f;

    d[0][0] = g->m[0][0] * k;
    d[0][1] = g->m[1][0] * k;
    d[0][2] = g->m[2][0] * k;
    d[0][3] = 0.0f;
    d[1][0] = g->m[0][1] * k;
    d[1][1] = g->m[1][1] * k;
    d[1][2] = g->m[2][1] * k;
    d[1][3] = 0.0f;
    d[2][0] = g->m[0][2] * k;
    d[2][1] = g->m[1][2] * k;
    d[2][2] = g->m[2][2] * k;
    d[2][3] = 0.0f;
}

/* Float matrix to GteMatrix (the inverse of func_0034A2B8). */
void func_0034A488(GteMatrix *g, Vu0Matrix s) {
    f32 k = 4096.0f;

    g->m[0][0] = s[0][0] * k;
    g->m[1][0] = s[0][1] * k;
    g->m[2][0] = s[0][2] * k;
    g->m[0][1] = s[1][0] * k;
    g->m[1][1] = s[1][1] * k;
    g->m[2][1] = s[1][2] * k;
    g->m[0][2] = s[2][0] * k;
    g->m[1][2] = s[2][1] * k;
    g->m[2][2] = s[2][2] * k;
    g->t[0] = s[3][0];
    g->t[1] = s[3][1];
    g->t[2] = s[3][2];
}

/* Wraps an angle into [-pi, pi]. */
f32 func_0034A570(f32 a) {
    f32 turn = 6.283186f;

    a = fmodf(a, turn);
    if (a > 3.141593f) {
        return a - turn;
    }
    if (a < -3.141593f) {
        return a + turn;
    }
    return a;
}

/* A rotation matrix whose third column is the direction v (normalised); the first column is horizontal when it
 * can be. */
void func_0034A5D0(Vu0Matrix m, f32 *v) {
    FVec4 x;
    FVec4 y;
    FVec4 z;

    z[0] = v[0];
    z[1] = v[1];
    z[2] = v[2];
    func_0034A868(z, z);
    if (z[0] != 0.0f || z[2] != 0.0f) {
        x[0] = z[2];
        x[1] = 0.0f;
        x[2] = -z[0];
    } else {
        x[0] = 0.0f;
        x[1] = -z[1] * z[2];
        x[2] = -z[1] * z[1];
    }
    func_0034A868(x, x);
    sceVu0OuterProduct(y, z, x);
    m[0][0] = x[0];
    m[1][0] = x[1];
    m[2][0] = x[2];
    m[0][1] = y[0];
    m[1][1] = y[1];
    m[2][1] = y[2];
    m[0][2] = z[0];
    m[1][2] = z[1];
    m[2][2] = z[2];
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[0][3] = 0.0f;
    m[1][3] = 0.0f;
    m[2][3] = 0.0f;
    m[3][3] = 1.0f;
}

/* Orthonormalises the rotation rows of s into d (Gram-Schmidt: row 0 normalised, row 1 made orthogonal and
 * normalised, row 2 their cross product), copying the translation row. */
void func_0034A710(Vu0Matrix d, Vu0Matrix s) {
    FVec4 t;

    func_0034A868(d[0], s[0]);
    sceVu0ScaleVector(t, d[0], sceVu0InnerProduct(s[1], d[0]));
    sceVu0SubVector(t, s[1], t);
    func_0034A868(d[1], t);
    sceVu0OuterProduct(d[2], d[0], d[1]);
    *(u128 *)d[3] = *(u128 *)s[3];
}

INCLUDE_RODATA("asm/nonmatchings/geometry_b", D_00522F90);
