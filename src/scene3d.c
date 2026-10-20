/* 0x34C508-0x34E748 (module scene3d, docs/game_overview.md section 4; docs/systems/render.md section 4): the
 * fighters' extra parts. func_0034C508 swings the per-character extra joints 18+ (fixed point, a lagged pendulum pushed
 * by the wind of the fighter's side); func_0034D258 places the slot's accessory parts each frame (special handlers for
 * characters 0x21 / 0xF, otherwise a float spring: lag, gravity, wind, collision shapes, floor clamp) and
 * func_0034E148 sets their records and state up. The float code of func_0034CC38 and func_0034E148 keeps x + 0.0f,
 * x * C / C and x / 12.0f * s, which -ffast-math folds; they come within a few lines only without -ffast-math
 * (docs/decomp_guide.md, "Lessons from T17 batch 1"). Its .rodata (0x51EA60-0x51EB00: the jump table of func_0034CA70
 * and D_0051EAF0) and .lit4 (0x5F5350-0x5F535C) are subsegments. Every NON_MATCHING draft (#if 0) compiles when
 * enabled. */
#include "common.h"
#include "fighter.h"
#include "gte.h"

typedef f32 Vu0Matrix[4][4];
typedef f32 FVec4[4] __attribute__((aligned(16)));

typedef struct {
    f32 v[4];
} __attribute__((aligned(16))) Vec4f;

/* Three float angles (x, y, z), 12 bytes. */
typedef struct {
    f32 v[3];
} Ang3;

/* A joint of a fighter's skeleton (as in src/skeleton.c). */
typedef struct SkelJoint {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ GteMatrix unk4;                /* local */
    /* 0x24 */ GteMatrix unk24;               /* world */
    /* 0x44 */ s32 unk44;
    /* 0x48 */ struct SkelJoint *unk48;       /* parent */
    /* 0x4C */ s32 unk4C;
} SkelJoint;

typedef struct {
    /* 0x000 */ u8 pad0[0x504];
    /* 0x504 */ SkelJoint unk504;
    /* 0x554 */ u8 pad554[0xDC];
    /* 0x630 */ SkelJoint unk630[24];
    /* 0xDB0 */ u8 padDB0[0xAAF0];
} FighterSkel; /* size 0xB8A0 */

#define SKEL(f) ((FighterSkel *)(f))

/* Float matrices of a fighter slot used by the part handlers. */
typedef struct {
    /* 0x0000 */ u8 pad0[0x560];
    /* 0x0560 */ Vu0Matrix unk560;
    /* 0x05A0 */ u8 pad5A0[0x8230];
    /* 0x87D0 */ Vu0Matrix unk87D0[20];       /* by func_0034CA70's index */
    /* 0x8CD0 */ u8 pad8CD0[0x2450];
    /* 0xB120 */ u16 unkB120;                 /* phase of the floating bob (func_0034CC38) */
    /* 0xB122 */ u8 padB122[0x77E];
} FighterGfx; /* size 0xB8A0 */

#define GFX(f) ((FighterGfx *)(f))

/* A drawn object: its local-to-world matrix and the parent matrix it is placed under (0xC0 bytes each). */
typedef struct {
    /* 0x00 */ Vu0Matrix unk0;
    /* 0x40 */ u8 pad40[0x70];
    /* 0xB0 */ f32 (*unkB0)[4];
    /* 0xB4 */ u8 padB4[0xC];
} Obj3D;

/* Swing parameters of an extra joint (D_0043BC30 / D_0043BC40). */
typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;                       /* smoothing frames */
    /* 0x4 */ s16 unk4;                       /* weight of the swing direction (/ 256) */
    /* 0x6 */ s16 unk6;                       /* weight of the joint's own down axis (/ 256) */
    /* 0x8 */ s16 unk8;                       /* length */
    /* 0xA */ s16 unkA;                       /* wind factor (/ 256) */
} DangleParam;

/* A part record (0x20 bytes) of a slot's list (func_00367390); unk0 = unk1 = unkE = -1 ends the list. */
typedef struct {
    /* 0x00 */ s8 unk0;                       /* lag weight (/ 100) */
    /* 0x01 */ s8 unk1;                       /* gravity weight (/ 100) */
    /* 0x02 */ s16 unk2;                      /* smoothing frames */
    /* 0x04 */ s16 unk4;                      /* length */
    /* 0x06 */ s16 unk6;                      /* angle limits in degrees */
    /* 0x08 */ s16 unk8;
    /* 0x0A */ s16 unkA;
    /* 0x0C */ s16 unkC;
    /* 0x0E */ s16 unkE;                      /* part id */
    /* 0x10 */ u8 unk10;                      /* bit 0: hidden when still, 1: stretched, 2: other angle order */
    /* 0x11 */ s8 unk11;                      /* wind (/ 100) */
    /* 0x12 */ s8 unk12;                      /* stretch above (%) */
    /* 0x13 */ s8 unk13;                      /* stretch below (%) */
    /* 0x14 */ u8 pad14[0xC];
} PartRec;

/* Per slot and part state (0x40 bytes, D_0043BD10). */
typedef struct {
    /* 0x00 */ f32 unk0;                      /* angle limits in radians */
    /* 0x04 */ f32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ u16 *unk18;                    /* collision shape indices */
    /* 0x1C */ s32 unk1C;                     /* their count */
    /* 0x20 */ f32 unk20;                     /* wind factor */
    /* 0x24 */ u8 pad24[0xC];
    /* 0x30 */ f32 unk30;
    /* 0x34 */ f32 unk34;
    /* 0x38 */ f32 unk38;
    /* 0x3C */ f32 unk3C;
} PartState;

typedef struct {
    u8 pad0[0x1C];
    s32 unk1C;
} Unk34E148Sub;

typedef struct {
    u8 pad0[0xC];
    Unk34E148Sub *unkC[1];
} Unk34E148Data;

/* An entry of the table func_00367370 returns (0x60 bytes): a collision shape on a joint; the first one's unk0 is
 * the count, and a u16 list of part ids follows the last one. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;                      /* kind, 2: sphere */
    /* 0x08 */ u32 unk8;
    /* 0x0C */ s32 unkC;                      /* joint (matrices at Fighter + 0xDB0, 0xC0 apart) */
    /* 0x10 */ FVec4 unk10;                   /* z: radius / 100 */
    /* 0x20 */ Vu0Matrix unk20;
} Unk367370;

typedef struct {
    /* 0x00 */ FVec4 unk0;
    /* 0x10 */ FVec4 unk10;
    /* 0x20 */ f32 *unk20;
    /* 0x24 */ u8 pad24[0xC];
    /* 0x30 */ Vu0Matrix unk30;
    /* 0x70 */ s32 unk70;
    /* 0x74 */ s32 unk74;
    /* 0x78 */ f32 unk78;
    /* 0x7C */ u8 pad7C[4];
} PartSpring;

/* Scratchpad work area of func_0034D258 (Spr_Setup(1, 0x440)). */
typedef struct {
    /* 0x000 */ Vu0Matrix unk0;
    /* 0x040 */ Vu0Matrix unk40;
    /* 0x080 */ Vu0Matrix unk80;
    /* 0x0C0 */ Vu0Matrix unkC0;
    /* 0x100 */ FVec4 unk100;
    /* 0x110 */ FVec4 unk110;
    /* 0x120 */ FVec4 unk120;
    /* 0x130 */ FVec4 unk130;
    /* 0x140 */ FVec4 unk140;
    /* 0x150 */ FVec4 unk150;
    /* 0x160 */ FVec4 unk160;
    /* 0x170 */ FVec4 unk170;
    /* 0x180 */ FVec4 unk180;
    /* 0x190 */ f32 unk190;
    /* 0x194 */ f32 unk194;
    /* 0x198 */ u8 pad198[8];
    /* 0x1A0 */ FVec4 unk1A0;
    /* 0x1B0 */ PartSpring unk1B0;
    /* 0x230 */ Vu0Matrix unk230;
    /* 0x270 */ u8 pad270[0xC0];
    /* 0x330 */ FVec4 unk330;
    /* 0x340 */ u8 pad340[0x50];
    /* 0x390 */ FVec4 unk390;
    /* 0x3A0 */ u8 pad3A0[0x88];
    /* 0x428 */ f32 unk428;
    /* 0x42C */ f32 unk42C;
    /* 0x430 */ u8 pad430[0x10];
} PartWork;

extern Fighter gFighters[4];
extern s16 gMode;
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 D_005F6514;
extern s16 D_005F6E18[4];
extern f32 D_005F6E20[2];
extern s16 D_005F6E28[4];
extern u8 D_0043B7F8[];
extern DangleParam D_0043BC30;
extern DangleParam D_0043BC40;
extern s16 D_0043BC50[][4];
extern s16 D_0043BC68[][4];
extern FVec4 D_0043BC80;
extern FVec4 D_0043BC90;
extern u8 D_0043BCA0[];
extern PartState D_0043BD10[][64];
extern const Vec4f D_0051EAF0;
extern const s16 gSinTable[];
extern PartRec *D_01FAB1E0[][64];
extern s16 *D_01FAB5E0[][3];
extern GteVector D_01FAB610[][3];
extern GteVector D_01FAB6D0[];
extern DangleParam *D_01FAB710[];
extern s16 D_01FAB720[][3];
extern FVec4 D_01FAB740[];
extern FVec4 D_01FAB760[][64];
extern Vec4f D_01FAC760[][3];

f32 atan2f(f32 y, f32 x);
f32 sqrtf(f32 x);
s32 rand(void);
void *memcpy(void *, const void *, u32);
void *memset(void *p, s32 c, s32 n);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0TransposeMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0InversMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0ApplyMatrix(f32 *v0, Vu0Matrix m, f32 *v1);
void sceVu0ScaleVector(f32 *v0, f32 *v1, f32 s);
void sceVu0ScaleVectorXYZ(f32 *v0, f32 *v1, f32 s);
void sceVu0AddVector(f32 *v0, f32 *v1, f32 *v2);
void sceVu0SubVector(f32 *v0, f32 *v1, f32 *v2);
f32 sceVu0InnerProduct(f32 *v0, f32 *v1);
void *Spr_Setup(s32 a, s32 size);
GteMatrix *Gte_MulMatrix0(GteMatrix *m0, GteMatrix *m1, GteMatrix *m2);
GteMatrix *Gte_TransposeMatrix(GteMatrix *m0, GteMatrix *m1);
GteVector *Gte_ApplyMatrixLV(GteMatrix *m, GteVector *v, GteVector *out);
GteVector *Gte_ApplyRotMatrix(GteSVector *v, GteVector *out);
GteVector *Gte_ApplyRotMatrixLV(GteVector *v, GteVector *out);
void Gte_SetRotMatrix(GteMatrix *m);
void func_00349D70(Vu0Matrix m, Ang3 *a);
void func_00349EE0(Vu0Matrix m, f32 *a);
f32 func_0034A148(f32 x, s32 n);
void func_0034A2B8(Vu0Matrix m, GteMatrix *g);
void func_0034A868(f32 *v0, f32 *v1);
void func_0034C300(PartSpring *s, f32 *v, PartRec *p);
void func_0034C4A0(Obj3D *o, s32 arg);
s32 func_0034CA70(s32 id, s32 *sub);
void func_0034CB38(s32 slot);
void func_0034CBA8(Fighter *f, s32 j, Vu0Matrix m);
s32 func_0034CC38(s32 id, Vu0Matrix m, Fighter *f, Obj3D *o, s32 arg4);
s32 func_0034D038(s32 id, Vu0Matrix m, Fighter *f, Obj3D *o, s32 arg4);
s32 func_0034D108(s32 id, Vu0Matrix m, Fighter *f, Obj3D *o, Ang3 *a, s32 arg5);
s32 func_0034E5A8(void);
GteMatrix *func_0034FC68(s32 side, s32 k);
void func_00356710(Vu0Matrix m);
Unk367370 *func_00367370(s32 i);
PartRec *func_00367390(s32 i);
void func_00398690(s16 x, s16 y, s16 z, GteMatrix *m);
s32 func_0039BD68(s32 x);
void func_0039BDF0(GteVector *v, GteVector *out);
s32 func_0039C0E0(s32 y, s32 x);
s32 func_003A9698(void);
s32 func_003ABD38(void);
u8 *func_003B87F8(s32 slot);
void func_003D3D08(f32 x, f32 y, f32 z, f32 *out);

/* The simulation does not advance this frame [I] (the pause tests of func_00373428 in battle_flow). */
#define SIM_FROZEN() (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0)

#define V(v) (&(v).vx)

/* NON_MATCHING: 65 lines: the original keeps &p in two registers (a copy in s2 and a spilled one); frame 0x10 smaller */
/* Local matrix of extra joint i (18+) [V]: rest angles D_01FAB5E0[slot][i - 18] the first frame (and joints 18 / 19
 * at rest while unkB130 bit 0); otherwise a pendulum: the joint's tip (length unk8 along x) is smoothed over unk2
 * frames in D_01FAB610, kept above the joint (y <= 0), turned into the parent frame and normalised; the direction is
 * blended with the joint's down axis (unk4 / unk6 / 256), then with the wind vector D_01FAB6D0[side] by
 * D_005F6E18[side] * unkA / 256, and becomes the yaw / pitch of the joint. Frozen while SIM_FROZEN(). */
#if 0
void func_0034C508(Fighter *f, s32 i) {
    GteMatrix m;
    GteSVector up;
    GteVector a;
    GteVector b;
    GteVector p;
    GteVector n;
    GteMatrix r;
    GteMatrix t;
    GteVector w;
    s32 *pa = V(a);
    s32 *pb = V(b);
    s32 *pp = V(p);
    s32 *pn = V(n);
    s16 slot = f->unk20;
    s32 k = i - 18;
    DangleParam *prm = D_01FAB710[slot];
    s16 *ang = D_01FAB5E0[slot][k];
    s16 *done;
    SkelJoint *j;
    s32 *st;
    s32 c;
    s32 cnt;
    s32 g;
    s32 h;
    s32 yaw;
    s32 pitch;

    if (prm == NULL) {
        return;
    }
    if ((f->unkB130 & 1) && k < 2) {
        func_00398690(0, 0, 0, &SKEL(f)->unk630[i].unk4);
        return;
    }
    if (SIM_FROZEN()) {
        return;
    }
    done = &D_01FAB720[slot][k];
    if (*done == 0) {
        *done = 1;
        func_00398690(ang[0], ang[1], ang[2], &SKEL(f)->unk630[i].unk4);
        return;
    }
    func_00398690(ang[0], ang[1], ang[2], &r);
    Gte_MulMatrix0(&SKEL(f)->unk630[i].unk48->unk24, &r, &t);
    Gte_TransposeMatrix(&t, &m);
    pa[2] = 0;
    pa[1] = 0;
    pa[0] = prm->unk8;
    j = &SKEL(f)->unk630[i];
    Gte_ApplyMatrixLV(&j->unk24, (GteVector *)pa, (GteVector *)pp);
    for (c = 2; c >= 0; c--) {
        pp[c] += j->unk24.t[c];
    }
    st = V(D_01FAB610[slot][k]);
    cnt = prm->unk2;
    for (c = 2; c >= 0; c--) {
        st[c] = pp[c] = (pp[c] + st[c] * (cnt - 1)) / cnt;
    }
    Gte_ApplyMatrixLV(&SKEL(f)->unk630[i].unk48->unk24, (GteVector *)SKEL(f)->unk630[i].unk4.t, (GteVector *)pb);
    for (c = 2; c >= 0; c--) {
        pb[c] += SKEL(f)->unk630[i].unk48->unk24.t[c];
    }
    if (pp[1] >= 0) {
        pp[1] = 0;
    }
    for (c = 2; c >= 0; c--) {
        pp[c] -= pb[c];
    }
    Gte_SetRotMatrix(&m);
    Gte_ApplyRotMatrixLV((GteVector *)pp, (GteVector *)pp);
    func_0039BDF0((GteVector *)pp, (GteVector *)pn);
    up.vy = 0x1000;
    up.vx = 0;
    up.vz = 0;
    Gte_ApplyRotMatrix(&up, &p);
    for (c = 2; c >= 0; c--) {
        pa[c] = pn[c] * prm->unk4;
        pa[c] = (V(p)[c] * prm->unk6 + pa[c]) / 256;
    }
    pa[0] += (0x100 - (prm->unk4 + prm->unk6)) * 16;
    h = D_005F6E18[f->unk1E] * prm->unkA / 256;
    g = 0x1000 - h;
    Gte_SetRotMatrix(&m);
    Gte_ApplyRotMatrixLV(&D_01FAB6D0[f->unk1E], &w);
    pa[0] = pa[0] * g / 4096 + w.vx * h / 4096;
    pa[1] = pa[1] * g / 4096 + w.vy * h / 4096;
    pa[2] = pa[2] * g / 4096 + w.vz * h / 4096;
    yaw = func_0039C0E0(-pa[2], pa[0]);
    pitch = func_0039C0E0(pa[1], func_0039BD68(pa[0] * pa[0] + pa[2] * pa[2]));
    func_00398690(0, yaw << 4, pitch << 4, &t);
    Gte_MulMatrix0(&r, &t, &SKEL(f)->unk630[i].unk4);
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d", func_0034C508);
#endif

/* Part ids 0x39..0x59 to the index of a float matrix at Fighter + 0x87D0 (func_0034CC38) [V]; 0x39 / 0x58 / 0x59 also
 * set *sub (0, 1, 2), -1 for other ids. */
s32 func_0034CA70(s32 arg0, s32 *arg1) {
    s32 var_v1;

    switch (arg0) {
    case 0x39:
        *arg1 = 0;
        var_v1 = 0;
        break;
    case 0x3C:
        var_v1 = 1;
        break;
    case 0x3F:
        var_v1 = 2;
        break;
    case 0x42:
        var_v1 = 5;
        break;
    case 0x45:
        var_v1 = 6;
        break;
    case 0x49:
        var_v1 = 0x1D;
        break;
    case 0x4A:
        var_v1 = 0x36;
        break;
    case 0x4C:
        var_v1 = 0x1F;
        break;
    case 0x4D:
        var_v1 = 0x38;
        break;
    case 0x4F:
        var_v1 = 0x22;
        break;
    case 0x50:
        var_v1 = 0x3B;
        break;
    case 0x52:
        var_v1 = 0x26;
        break;
    case 0x53:
        var_v1 = 0x3F;
        break;
    case 0x54:
        var_v1 = 0x4F;
        break;
    case 0x58:
        var_v1 = 0x52;
        *arg1 = 1;
        break;
    case 0x59:
        var_v1 = 0x5B;
        *arg1 = 2;
        break;
    default:
        var_v1 = -1;
        break;
    }
    return var_v1;
}

/* Clears the parts-ready flag of a slot [V] (func_0034D258 sets it after its first frame). */
void func_0034CB38(s32 slot) {
    D_005F6E28[slot] = 0;
}

/* Clears the parts-ready flags of the four slots [V]. */
void func_0034CB50(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        func_0034CB38(i);
    }
}

/* Attach point j of side i (D_01FAC760, three float vectors per side) [V]. */
Vec4f *func_0034CB88(s32 i, s32 j) {
    return &D_01FAC760[i][j];
}

RODATA_ALIGN16();
INCLUDE_RODATA("asm/nonmatchings/scene3d", D_0051EAF0);

/* Stores attach point j of the fighter's side from matrix m [V]: point 0 is its translation, the others the point
 * D_0051EAF0 (300, 0, 0) transformed by m. */
void func_0034CBA8(Fighter *f, s32 j, Vu0Matrix m) {
    Vec4f v;

    if (j == 0) {
        memcpy((FVec4 *)&D_01FAC760[f->unk1E][0], (FVec4 *)m[3], 12);
    } else {
        v = D_0051EAF0;
        sceVu0ApplyMatrix(D_01FAC760[f->unk1E][j].v, m, v.v);
    }
}

/* NON_MATCHING: 61 lines (9 without -ffast-math: one delay slot): x + 0.0f and x / 12.0f * s are folded */
/* Parts of character 0x21 [V]: id -> matrix index (func_0034CA70); hidden (scaled to 0) when func_003ABD38() or
 * option D_0043B7F8[0] == 8; index k > 0 copies the fighter's matrix k keeping m's translation; index 0 is a body
 * floating at (0, 150, -800) from matrix 0: lagged (x / 12, y and z / 16 per frame) towards the fighter's 0x560
 * matrix and bobbing on gSinTable with the phase unkB120 (+0x40 a frame) plus a little rand(); still while
 * SIM_FROZEN() and placed directly while the slot's parts are not set up. Returns 1 when it handled the part. */
#if 0
s32 func_0034CC38(s32 id, Vu0Matrix m, Fighter *f, Obj3D *o, s32 arg4) {
    FVec4 save;
    FVec4 old;
    s32 sub = -1;
    s32 r = 0;
    s32 k;
    s32 on;
    f32 fs;

    if (f->unk2A == 0x21) {
        k = func_0034CA70(id, &sub);
        if (k >= 0) {
            if (func_003ABD38() != 0 || D_0043B7F8[0] == 8) {
                sceVu0ScaleVector(m[0], m[0], 0.0f);
                sceVu0ScaleVector(m[1], m[1], 0.0f);
                sceVu0ScaleVector(m[2], m[2], 0.0f);
                sceVu0MulMatrix(o->unk0, o->unkB0, m);
                func_0034C4A0(o, arg4);
            } else if (k == 0) {
                on = 1;
                sceVu0CopyMatrix(m, GFX(f)->unk87D0[0]);
                m[3][0] += 0.0f;
                m[3][1] += 150.0f;
                m[3][2] += -800.0f;
                if (D_005F6E28[(s16)f->unk20] == 0) {
                    sceVu0MulMatrix(o->unk0, o->unkB0, m);
                    return 1;
                }
                if (SIM_FROZEN()) {
                    on = 0;
                } else {
                    GFX(f)->unkB120 += 0x40;
                }
                memcpy((FVec4 *)old, (FVec4 *)o->unk0[3], 12);
                fs = on;
                sceVu0MulMatrix(o->unk0, GFX(f)->unk560, m);
                o->unk0[3][0] = old[0] + (o->unk0[3][0] - old[0]) / 12.0f * fs;
                o->unk0[3][1] = old[1] + (o->unk0[3][1] - old[1]) * 0.0625f * fs;
                o->unk0[3][2] = old[2] + (o->unk0[3][2] - old[2]) * 0.0625f * fs;
                o->unk0[3][0] += (f32)(gSinTable[((((s16)GFX(f)->unkB120 + ((rand() * on) & 0x3F)) / 2) & 0xFFF) + 0x400] / 192) * fs;
                o->unk0[3][1] += (f32)(gSinTable[((s16)GFX(f)->unkB120 + ((rand() * on) & 0x3F)) & 0xFFF] / 192) * fs;
                o->unk0[3][2] += (f32)(gSinTable[(((s16)GFX(f)->unkB120 + ((rand() * on) & 0x3F)) / 2) & 0xFFF] / 192) * fs;
                func_0034CBA8(f, sub, o->unk0);
            } else {
                FVec4 *t = (FVec4 *)m[3];

                memcpy((FVec4 *)save, (FVec4 *)t, 12);
                sceVu0CopyMatrix(m, GFX(f)->unk87D0[k]);
                memcpy((FVec4 *)t, (FVec4 *)save, 12);
                sceVu0MulMatrix(o->unk0, o->unkB0, m);
                if (sub > 0) {
                    func_0034CBA8(f, sub, o->unk0);
                }
            }
            func_0034C4A0(o, arg4);
            r = 1;
        }
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d", func_0034CC38);
#endif

/* Parts id 3..11 while unkB130 bit 13 is set [V]: id / 3 - 1 < 3 takes the GTE matrix of extra joint 18 + that
 * (or func_0034FC68's replacement while unkB130 bit 0) as the part's matrix. Returns 1 when it handled the part. */
s32 func_0034D038(s32 id, Vu0Matrix m, Fighter *f, Obj3D *o, s32 arg4) {
    s32 n = id / 3;
    s32 j = n - 1;
    s32 r = 0;
    SkelJoint *jt;

    if (j < 3) {
        jt = SKEL(f)->unk630;
        if (f->unkB130 & 1) {
            func_0034A2B8(m, func_0034FC68((s16)f->unk20, n + 1));
        } else {
            func_0034A2B8(m, &jt[j + 18].unk4);
        }
        sceVu0MulMatrix(o->unk0, o->unkB0, m);
        func_0034C4A0(o, arg4);
        r = 1;
    }
    return r;
}

/* NON_MATCHING: 15 lines: r and o in swapped saved registers, r = 1 not in the compare's delay slot */
/* Parts 0x78 / 0x7B and 0x7E / 0x81 of characters 0xF and 0x21 [V]: shown (scale 1) only while the fighter's unk28 is
 * 0x1A resp. 4, else the rotation is scaled to 0. Returns 1 when it handled the part. */
#if 0
s32 func_0034D108(s32 id, Vu0Matrix m, Fighter *f, Obj3D *o, Ang3 *a, s32 arg5) {
    s32 r = 0;
    f32 t = 0.0f;
    f32 s = t;

    if (f->unk2A == 0xF || f->unk2A == 0x21) {
        switch (id) {
        case 0x81:
            if (f->unk28 == 4) {
                s = 1.0f;
            }
            r = 1;
            break;
        case 0x7E:
            if (f->unk28 == 4) {
                s = 1.0f;
            }
            r = 1;
            break;
        case 0x78:
        case 0x7B:
            if (f->unk28 == 0x1A) {
                s = 1.0f;
            }
            r = 1;
            break;
        default:
            return 0;
        }
        if (s > 0.0f) {
            t = 1.0f;
        }
        func_00349D70(m, a);
        sceVu0ScaleVector(m[0], m[0], s);
        sceVu0ScaleVector(m[1], m[1], t);
        sceVu0ScaleVector(m[2], m[2], t);
        sceVu0MulMatrix(o->unk0, o->unkB0, m);
        func_0034C4A0(o, arg5);
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d", func_0034D108);
#endif

/* NON_MATCHING: about 610 lines: typed draft with the original control flow (880 instructions) */
/* Places the n accessory parts of a slot [V] (matrices mtx[i], objects at base + 0x4980, 0xC0 apart): the handlers
 * func_0034CC38 / func_0034D038 / func_0034D108 first; the first frame (D_005F6E28[slot] clear) puts each part at rest
 * and starts its smoothed tip D_01FAB760; otherwise a spring: the tip is averaged over unk2 frames, its direction in
 * the part's frame is blended with gravity D_0043BC80 (unk0 / unk1 %), then with the side's wind (unless gMode 5),
 * pushed out of the fighter's collision shapes (func_00367370, spheres inline, others func_0034C300), turned into two
 * angles (atan2f) clamped to the state's limits, optionally stretched by height, and kept above the floor. */
#if 0
void func_0034D258(u8 *base, Vu0Matrix *mtx, s32 slot, s32 n, s32 arg4) {
    FVec4 v;
    FVec4 w;
    Obj3D *o = (Obj3D *)(base + 0x4980);
    Ang3 *ang = (Ang3 *)(func_003B87F8(slot) + 0x498);
    Fighter *f = &gFighters[slot];
    s32 still = gMode == 5;
    FVec4 *tip = D_01FAB760[slot];
    PartState *ps = D_0043BD10[slot];
    s32 noCollide = 0;
    s32 stopped = func_0034E5A8();
    PartWork *ws;
    PartRec *p;
    PartSpring *sp;
    Unk367370 *tbl;
    Unk367370 *e;
    s32 i;
    s32 k;
    s32 alt;
    f32 kDir;
    f32 kGrav;
    f32 cnt;
    f32 inv;
    f32 kOwn;
    f32 len;
    f32 d;
    f32 sc;

    if (n == 0) {
        return;
    }
    if (still && (f->unk2A == 0xB || f->unk2A == 0x1C)) {
        noCollide = 1;
    }
    ws = Spr_Setup(1, 0x440);
    i = 0;
    func_00356710(ws->unkC0);
    ws->unk110[0] = 0.0f;
    alt = (f->unkB130 & 0x2000) != 0;
    for (; i < n; i++, o++, mtx++, ps++, ang++, tip++) {
        p = D_01FAB1E0[(s16)f->unk20][i];
        if (still && (p->unk10 & 1)) {
            continue;
        }
        if (func_0034CC38(p->unkE, *mtx, f, o, arg4) != 0) {
            continue;
        }
        if (alt && func_0034D038(p->unkE, *mtx, f, o, arg4) != 0) {
            continue;
        }
        if (func_0034D108(p->unkE, *mtx, f, o, ang, arg4) != 0) {
            continue;
        }
        if (D_005F6E28[(s16)f->unk20] == 0) {
            func_00349D70(*mtx, ang);
            sceVu0MulMatrix(o->unk0, o->unkB0, *mtx);
            D_0043BC90[0] = p->unk4;
            sceVu0ApplyMatrix(*tip, o->unk0, D_0043BC90);
            func_0034C4A0(o, arg4);
            continue;
        }
        if (((f->unkB130 & 2) && (u16)(p->unkE - 0x1F) < 8) || stopped) {
            sceVu0MulMatrix(o->unk0, o->unkB0, *mtx);
            func_0034C4A0(o, arg4);
            continue;
        }
        kDir = p->unk0 / 100.0f;
        kGrav = p->unk1 / 100.0f;
        cnt = p->unk2;
        len = p->unk4;
        *(u128 *)ws->unk140 = *(u128 *)(*mtx)[3];
        inv = 1.0f / cnt;
        kOwn = 1.0f - (kDir + kGrav);
        sceVu0CopyMatrix(ws->unk0, ws->unkC0);
        func_00349D70(ws->unk0, ang);
        sceVu0MulMatrix(ws->unk80, o->unkB0, ws->unk0);
        sceVu0TransposeMatrix(ws->unk40, ws->unk80);
        D_0043BC90[0] = len;
        sceVu0ApplyMatrix(ws->unk170, o->unk0, D_0043BC90);
        sceVu0ScaleVectorXYZ(ws->unk150, *tip, cnt - 1.0f);
        sceVu0AddVector(ws->unk150, ws->unk150, ws->unk170);
        sceVu0ScaleVectorXYZ(*tip, ws->unk150, inv);
        *(u128 *)ws->unk170 = *(u128 *)*tip;
        sceVu0ApplyMatrix(ws->unk180, o->unkB0, ws->unk140);
        sceVu0SubVector(ws->unk150, ws->unk170, ws->unk180);
        sceVu0ApplyMatrix(ws->unk160, ws->unk40, ws->unk150);
        func_0034A868(ws->unk120, ws->unk160);
        sceVu0ApplyMatrix(ws->unk130, ws->unk40, D_0043BC80);
        sceVu0ScaleVectorXYZ(ws->unk100, ws->unk120, kDir);
        sceVu0ScaleVectorXYZ(ws->unk130, ws->unk130, kGrav);
        sceVu0AddVector(ws->unk100, ws->unk100, ws->unk130);
        ws->unk100[0] += kOwn;
        if (!still) {
            ws->unk194 = D_005F6E20[f->unk1E] * ps->unk20;
            ws->unk190 = 1.0f - ws->unk194;
            sceVu0ApplyMatrix(ws->unk1A0, ws->unk40, D_01FAB740[f->unk1E]);
            sceVu0ScaleVectorXYZ(ws->unk1A0, ws->unk1A0, ws->unk194);
            sceVu0ScaleVectorXYZ(ws->unk100, ws->unk100, ws->unk190);
            sceVu0AddVector(ws->unk100, ws->unk100, ws->unk1A0);
        }
        if (ps->unk18 != NULL && !noCollide) {
            sp = &ws->unk1B0;
            tbl = func_00367370(f->unk22);
            k = 0;
            sceVu0CopyMatrix(ws->unk230, ws->unk0);
            *(u128 *)ws->unk230[3] = *(u128 *)ws->unk140;
            sceVu0MulMatrix(ws->unk230, o->unkB0, ws->unk230);
            func_0034A868(ws->unk330, ws->unk100);
            sceVu0ScaleVector(ws->unk330, ws->unk330, p->unk4);
            ws->unk330[3] = 1.0f;
            sceVu0ApplyMatrix(sp->unk10, ws->unk230, ws->unk330);
            *(u128 *)sp->unk0 = *(u128 *)ws->unk180;
            sp->unk74 = 0;
            for (; k < ps->unk1C; k++) {
                e = &tbl[ps->unk18[k]];
                sceVu0MulMatrix(sp->unk30, *(Vu0Matrix *)((u8 *)f + e->unkC * 0xC0 + 0xDB0), e->unk20);
                sp->unk20 = e->unk10;
                sp->unk70 = e->unk4;
                sp->unk78 = p->unk4;
                if (e->unk4 != 2) {
                    func_0034C300(sp, e->unk10, p);
                } else {
                    ws->unk428 = e->unk10[2] * 100.0f;
                    sceVu0SubVector(sp->unk10, sp->unk10, sp->unk30[3]);
                    d = sqrtf(sceVu0InnerProduct(sp->unk10, sp->unk10));
                    ws->unk42C = d;
                    if (d < ws->unk428) {
                        ws->unk428 = d * 0.8f + ws->unk428 * 0.2f;
                    }
                    func_0034A868(ws->unk390, sp->unk10);
                    sceVu0ScaleVectorXYZ(sp->unk10, ws->unk390, ws->unk428);
                    sceVu0AddVector(sp->unk10, sp->unk10, sp->unk30[3]);
                    sceVu0SubVector(sp->unk10, sp->unk10, sp->unk0);
                    func_0034A868(sp->unk10, sp->unk10);
                    sceVu0ScaleVectorXYZ(sp->unk10, sp->unk10, sp->unk78);
                    sceVu0AddVector(sp->unk10, sp->unk10, sp->unk0);
                }
            }
            sceVu0InversMatrix(ws->unk230, ws->unk230);
            sceVu0ApplyMatrix(ws->unk100, ws->unk230, sp->unk10);
        }
        if (p->unk10 & 4) {
            ws->unk110[1] = atan2f(-ws->unk100[2], ws->unk100[0]);
            ws->unk110[2] = atan2f(ws->unk100[1], sqrtf(ws->unk100[0] * ws->unk100[0] + ws->unk100[2] * ws->unk100[2]));
        } else {
            ws->unk110[1] = atan2f(-ws->unk100[2], sqrtf(ws->unk100[0] * ws->unk100[0] + ws->unk100[1] * ws->unk100[1]));
            ws->unk110[2] = atan2f(ws->unk100[1], ws->unk100[0]);
        }
        if (ps->unk0 != ps->unk4) {
            if (ps->unk0 < ws->unk110[1]) {
                ws->unk110[1] = ps->unk0;
            }
            if (ws->unk110[1] < ps->unk4) {
                ws->unk110[1] = ps->unk4;
            }
        }
        if (ps->unk8 != ps->unkC) {
            if (ps->unk8 < ws->unk110[2]) {
                ws->unk110[2] = ps->unk8;
            }
            if (ws->unk110[2] < ps->unkC) {
                ws->unk110[2] = ps->unkC;
            }
        }
        if (p->unk10 & 4) {
            func_00349EE0(ws->unk80, ws->unk110);
        } else {
            func_00349D70(ws->unk80, (Ang3 *)ws->unk110);
        }
        sceVu0MulMatrix(*mtx, ws->unk0, ws->unk80);
        *(u128 *)(*mtx)[3] = *(u128 *)ws->unk140;
        sceVu0MulMatrix(o->unk0, o->unkB0, *mtx);
        if (p->unk10 & 2) {
            memset(v, 0, 0x10);
            v[0] = 1.0f;
            sceVu0ApplyMatrix(w, o->unk0, v);
            if (w[1] > 0.0f) {
                sc = 1.0f - w[1] * ps->unk10;
            } else {
                sc = w[1] * ps->unk14 + 1.0f;
            }
            sceVu0ScaleVectorXYZ((*mtx)[0], (*mtx)[0], sc);
            sceVu0MulMatrix(o->unk0, o->unkB0, *mtx);
        }
        sceVu0ApplyMatrix(ws->unk170, o->unk0, D_0043BC90);
        if (ws->unk170[1] < 0.0f) {
            sceVu0SubVector(ws->unk150, ws->unk170, ws->unk180);
            ws->unk150[1] = 0.0f;
            func_0034A868(ws->unk150, ws->unk150);
            sceVu0ScaleVectorXYZ(ws->unk150, ws->unk150, sqrtf(func_0034A148(len, 2) - func_0034A148(ws->unk180[1], 2)));
            ws->unk150[1] = -ws->unk180[1];
            sceVu0ApplyMatrix(ws->unk100, ws->unk40, ws->unk150);
            if (p->unk10 & 4) {
                ws->unk110[1] = atan2f(-ws->unk100[2], ws->unk100[0]);
                ws->unk110[2] = atan2f(ws->unk100[1], sqrtf(ws->unk100[0] * ws->unk100[0] + ws->unk100[2] * ws->unk100[2]));
            } else {
                ws->unk110[1] = atan2f(-ws->unk100[2], sqrtf(ws->unk100[0] * ws->unk100[0] + ws->unk100[1] * ws->unk100[1]));
                ws->unk110[2] = atan2f(ws->unk100[1], ws->unk100[0]);
            }
            sceVu0CopyMatrix(ws->unk80, ws->unkC0);
            func_00349EE0(ws->unk80, ws->unk110);
            sceVu0MulMatrix(*mtx, ws->unk0, ws->unk80);
            *(u128 *)(*mtx)[3] = *(u128 *)ws->unk140;
            sceVu0MulMatrix(o->unk0, o->unkB0, *mtx);
        }
        func_0034C4A0(o, arg4);
    }
    if (D_005F6E28[(s16)f->unk20] == 0) {
        D_005F6E28[(s16)f->unk20] = 1;
    }
}
#else
LIT4_WORD(D_005F5350, 0x3F4CCCCD);
LIT4_WORD(D_005F5354, 0x3E4CCCCD);
INCLUDE_ASM("asm/nonmatchings/scene3d", func_0034D258);
#endif

/* NON_MATCHING: 19 lines: address computations and store order after the sqrt; the empty loop is a fake */
/* Wind of the fighter's side, once per frame (start of Skel_Update) unless gMode 5 [V]: func_003D3D08 turns the
 * fighter's unk4 / unk8 / unkC into the float vector D_01FAB740[side]; its length goes to D_005F6E20[side] (float) and
 * D_005F6E18[side] (x 4096), the vector x 4096 with y and z negated to D_01FAB6D0[side] (GTE space). */
#if 0
void func_0034E018(Fighter *f) {
    s16 side = f->unk1E;
    f32 *v;
    f32 len;

    if (gMode != 5) {
        v = D_01FAB740[side];
        func_003D3D08(f->unk4, f->unk8, f->unkC, v);
        len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        do {
        } while (0); /* fake */
        D_005F6E18[side] = len * 4096.0f;
        D_01FAB6D0[side].vx = v[0] * 4096.0f;
        D_01FAB6D0[side].vy = -(s32)(v[1] * 4096.0f);
        D_01FAB6D0[side].vz = -(s32)(v[2] * 4096.0f);
        D_005F6E20[side] = len;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d", func_0034E018);
#endif

/* Marks a part record as the end of the list [V]. */
void func_0034E130(PartRec *p) {
    p->unk0 = p->unk1 = -1;
    p->unkE = -1;
}

/* NON_MATCHING: 77 lines (23 without -ffast-math): x * 3.141593f / 180.0f folded; the record search loop is laid out
 * differently, three float zero stores go through a register in the original */
/* Sets up the n accessory parts of a fighter [V]: clears its ready flag; for each part finds (or appends, after the
 * terminator func_0034E130 writes) the record with the model's part id (d->unkC[i + 0x62]->unk1C) in the slot's
 * list func_00367390, keeps it in D_01FAB1E0[slot][i], and fills the state D_0043BD10[slot][i]: angle limits in
 * radians, wind, stretch factors, the stretch flag. Then numbers the collision shapes of func_00367370 (id < 0x62
 * by D_0043BCA0, else by part) and gives each part its list of shape indices. */
#if 0
void func_0034E148(Fighter *f, s32 n, Unk34E148Data *d) {
    PartState *s;
    PartRec *p;
    PartState *ps;
    PartRec *pr;
    Unk367370 *t;
    u16 *q;
    u16 *r;
    s32 i;
    s32 j;
    s32 found;
    s32 id;
    s32 cnt;
    u16 nb;

    s = D_0043BD10[(s16)f->unk20];
    func_0034CB38((s16)f->unk20);
    for (i = 0; i < n; i++, s++) {
        found = 0;
        p = func_00367390(f->unk22);
        id = d->unkC[i + 0x62]->unk1C;
        for (j = 0; j < 0x40; j++, p++) {
            if ((p->unk0 == -1 && p->unk1 == -1) || p->unkE == -1) {
                p->unkE = id;
                found = 1;
                p[1].unk0 = -1;
                p[1].unkE = -1;
                p[1].unk1 = -1;
                break;
            } else if (id == p->unkE) {
                break;
            }
        }
        D_01FAB1E0[(s16)f->unk20][i] = p;
        if (found) {
            p->unk4 = 0x78;
            p->unk2 = 2;
            p->unk1 = 0;
            p->unk0 = 0;
            p->unk10 = 0;
            p->unk6 = 0;
            p->unk8 = 0;
            p->unkA = 0;
            p->unkC = 0;
            p->unk11 = 0;
            p->unk12 = 0;
            p->unk13 = 0;
        }
        s->unk0 = p->unk6 * 3.141593f / 180.0f;
        s->unk4 = p->unk8 * 3.141593f / 180.0f;
        s->unk8 = p->unkA * 3.141593f / 180.0f;
        s->unkC = p->unkC * 3.141593f / 180.0f;
        s->unk20 = p->unk11 / 100.0f;
        s->unk10 = 1.0f - (p->unk12 + 100.0f) / 100.0f;
        s->unk14 = 1.0f - (p->unk13 + 100.0f) / 100.0f;
        if (p->unk12 != 0 || p->unk13 != 0) {
            p->unk10 |= 2;
        } else {
            p->unk10 &= ~2;
        }
        s->unk30 = 0.0f;
        s->unk3C = 1.0f;
        s->unk18 = 0;
        s->unk1C = 0;
        s->unk34 = s->unk38 = 0.0f;
    }
    t = func_00367370(f->unk22);
    if (t != NULL) {
        cnt = t->unk0;
        q = (u16 *)&t[cnt];
        nb = *q++;
        for (i = 0; i < cnt; i++) {
            t[i].unkC = 0;
            if (t[i].unk8 >= 0x62) {
                for (j = 0; j < n; j++) {
                    if (t[i].unk8 == D_01FAB1E0[(s16)f->unk20][j]->unkE + 0x62) {
                        t[i].unkC = j + 0x62;
                    }
                }
            } else {
                t[i].unkC = D_0043BCA0[t[i].unk8];
            }
        }
        ps = D_0043BD10[(s16)f->unk20];
        for (i = 0; i < n; i++, ps++) {
            r = q;
            pr = D_01FAB1E0[(s16)f->unk20][i];
            for (j = 0; j < nb; j++) {
                if (pr->unkE == r[0]) {
                    ps->unk18 = r + 2;
                    ps->unk1C = r[1];
                }
                r += r[1] + 2;
            }
        }
    }
}
#else
LIT4_WORD(D_005F5358, 0x40490FDC);
INCLUDE_ASM("asm/nonmatchings/scene3d", func_0034E148);
#endif

/* 1 while the simulation stays frozen, 0 on the frame it freezes and while it runs [V] (D_005F6514 keeps the last
 * state). */
s32 func_0034E5A8(void) {
    s32 now = 0;
    s32 r;

    if (SIM_FROZEN()) {
        now = 1;
    }
    r = 0;
    if (!((D_005F6514 ^ now) & now)) {
        if (SIM_FROZEN()) {
            r = 1;
        }
    }
    D_005F6514 = now;
    return r;
}

/* NON_MATCHING: 14 lines: the registers of the shared reset stores (a1 / a2 / a3 rotated) */
/* Sets extra joint j (18+) of characters 0x11 and 0x14 up [V]: rest angles D_0043BC50 / D_0043BC68 and swing
 * parameters D_0043BC40 / D_0043BC30 for the slot, then clears the joint's smoothed tip and first-frame flag. */
#if 0
void func_0034E640(s32 j, Fighter *f) {
    s32 k = j - 18;
    s16 slot = f->unk20;

    if (f->unk2A == 0x11) {
        D_01FAB710[slot] = &D_0043BC40;
        D_01FAB5E0[slot][k] = D_0043BC50[k];
    } else if (f->unk2A == 0x14) {
        D_01FAB710[slot] = &D_0043BC30;
        D_01FAB5E0[slot][k] = D_0043BC68[k];
    }
    D_01FAB610[slot][k].vz = 0;
    D_01FAB610[slot][k].vy = 0;
    D_01FAB720[slot][k] = 0;
    D_01FAB610[slot][k].vx = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d", func_0034E640);
#endif
