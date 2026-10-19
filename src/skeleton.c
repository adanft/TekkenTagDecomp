/* 0x34E748-0x34FF10 (module skeleton, docs/game_overview.md section 4; docs/systems/animation.md): the fighter skeleton.
 * Each frame Skel_Update samples the motion of the fighter's moves, builds the root matrix (whose translation is the
 * position unk51C..unk524 the simulation uses) and the joints' world matrices; Skel_PlaceHitShapes and
 * Skel_SetVolumeStarts put the body spheres, the push spheres and the attack volumes on joints; Skel_ProjectJoints
 * projects four joints with the camera. The range is a cut inside the scene3d range (no evidence for the object edges).
 * Its .lit4 words (0x5F5114-0x5F5154, twice 2 pi / 0x10000 per float angle conversion) are a subsegment: LIT4_WORD
 * emits them in place while Skel_SetRootMatrix is assembly. Every NON_MATCHING draft (#if 0) compiles when enabled. */
#include "common.h"
#include "fighter.h"
#include "gte.h"

/* A joint of a fighter's skeleton (0x50 bytes): its transform relative to the parent, built each frame from the motion
 * pose, and its world transform (parent world * local, Skel_ComputeWorld). The root is Fighter + 0x504 (its local
 * translation is the fighter's position unk51C..unk524); the joints start at Fighter + 0x630. */
typedef struct SkelJoint {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ GteMatrix unk4;                /* local: rotation of the pose, translation = the bone's offset */
    /* 0x24 */ GteMatrix unk24;               /* world */
    /* 0x44 */ s32 unk44;
    /* 0x48 */ struct SkelJoint *unk48;       /* parent */
    /* 0x4C */ s32 unk4C;
} SkelJoint;

/* The skeleton part of a fighter slot. */
typedef struct {
    /* 0x000 */ u8 pad0[0x504];
    /* 0x504 */ SkelJoint unk504;             /* root */
    /* 0x554 */ u8 pad554[0xDC];
    /* 0x630 */ SkelJoint unk630[24];         /* 18 joints, then the character's extra ones (unkB050) */
    /* 0xDB0 */ u8 padDB0[0xAAF0];
} FighterSkel; /* size 0xB8A0, the size of Fighter */

#define SKEL(f) ((FighterSkel *)(f))

typedef struct {
    s32 v[5];
} S32x5;

typedef f32 FMatrix[4][4];

extern Fighter gFighters[4];
extern s16 gMode;
extern s32 D_005F5EF8;
extern s32 D_005F5F10;
extern s16 D_005F5E70[];
extern s16 D_00416A18[];
extern s16 D_00416A1E[][3];
extern const s16 gSinTable[];
extern const S32x5 D_004FC668;
extern GteMatrix D_008791A0[2][5];
extern s32 D_0042F060[14];
extern s32 D_0042EFB8[8];
extern const s8 D_0051DE50[18];
extern GteMatrix D_00436530;
extern s16 D_005F5E50[];
extern s16 D_005F5E58[];
extern s16 D_005F5E60[];
extern s16 D_005F5E68[];

f32 sinf(f32 x);
f32 cosf(f32 x);
void func_0034E018(void);
void func_003ADFB0(void);
void func_00365300(Fighter *f);
s32 func_00346378(s32 side);
void func_00353280(s32 side);
void Motion_SampleRoot(u8 *m, s16 *out, s32 frame, Fighter *f);
void Motion_SamplePose(u8 *m, s16 *out, s32 frame);
void func_0034FA60(Fighter *f);
void func_00373E38(Fighter *f);
void func_00373428(Fighter *f);
void func_00365248(Fighter *f);
void Skel_SetRootMatrix(Fighter *f);
void func_003632E0(Fighter *f);
void func_00362C28(Fighter *f);
void func_00350728(Fighter *f);
s32 func_003D3228(s32 a, s32 b);
void Skel_ComputeWorld(Fighter *f);
void func_003478F0(Fighter *f, s32 n);
void func_00353878(s32 side, void *p);
void func_0034FB48(Fighter *f);
void Skel_Update(Fighter *f);
void func_00365108(Fighter *f, GteSVector *r);
u8 func_003A9520(void);
void func_00349EE0(FMatrix m, f32 *r);
u16 func_003A0340(Fighter *f, s32 y);
void func_00364F38(Fighter *f);
void func_003986D8(GteSVector *r, GteMatrix *m);
s32 func_003AE6C0(s32 character);
void func_0039BFB0(GteMatrix *m, GteVector *v);
void Skel_GetPoseAngles(s32 j, GteSVector *r);
void Skel_RotMatrixFromAngles(GteSVector *r, GteMatrix *m);
void func_003A02C0(s32 j, Fighter *f, GteMatrix *m, s16 *d, GteMatrix *out);
GteMatrix *func_0034FC68(s32 side, s32 k);
s32 func_003626B8(Fighter *f, s32 j, GteMatrix **m);
void func_0034C508(Fighter *f, s32 j);
void func_003D3210(s32 a, s32 b);
void Skel_ProjectJoints(Fighter *f);
void func_0034FD10(GteMatrix *cam, GteMatrix *m);
s32 func_00398600(s32 x, s32 y, s32 z, s16 *out);
GteVector *Gte_ApplyMatrix(GteMatrix *m, GteSVector *v, GteVector *out);
GteMatrix *Gte_CompMatrix(GteMatrix *m0, GteMatrix *m1, GteMatrix *m2);
GteMatrix *Gte_RotMatrixXYZ(GteSVector *r, GteMatrix *m);
void Gte_SetRotMatrix(GteMatrix *m);
void Gte_SetTransMatrix(GteMatrix *m);
GteMatrix *Gte_MulRotMatrix(GteMatrix *m);
GteVector *Gte_ApplyRotMatrixLV(GteVector *v, GteVector *out);

/* The position of joint `jj` into (x, y, z); joint 0x13 is joint 6 for character 0xE; joints past 23 are ignored. */
#define SKEL_VOLUME(f, j, jj, x, y, z)                                                                          \
    j = jj;                                                                                                       \
    if (j == 0x13 && (f)->unk2A == 0xE) {                                                                         \
        j = 6;                                                                                                    \
    }                                                                                                             \
    if (j < 0x18) {                                                                                               \
        x = ((FighterJoints *)(f))->unk668[j].unk0.unk0[0];                                                       \
        y = ((FighterJoints *)(f))->unk668[j].unk0.unk0[1];                                                       \
        z = ((FighterJoints *)(f))->unk668[j].unk0.unk0[2];                                                       \
    }

/* FighterMove.unk24 seen as bit fields: the low 19 bits are masked before a flag test (func_0034FB48). */
typedef struct {
    u32 lo : 19;
    u32 hi : 13;
} MoveFlags24;

/* One 16-bit angle unit in radians (2 pi / 0x10000). */
#define ANGLE_RAD (6.2831855f / 65536.0f)
#define ANGLE_TO_RAD(a) (((a) & 0xFFFF) <= 0x7FFF ? ((a) & 0xFFFF) * ANGLE_RAD : (((a) & 0xFFFF) - 0x10000) * ANGLE_RAD)
#define ANGLE_TO_RAD2(a) (((a) & 0xFFFF) <= 0x7FFF ? ANGLE_RAD * ((a) & 0xFFFF) : (((a) & 0xFFFF) - 0x10000) * ANGLE_RAD)

typedef struct {
    u8 pad0[0xB390];
    GteMatrix unkB390[18];
} FighterBlend;

/* The per-frame skeleton update of a fighter (via func_0034E8B0 from func_00371B18): root channels of the base motion
 * (unkB8) into D_005F5E70 and unk30..unk34, the pose of the current move (unkC0) at frame unkC4 into D_00416A18, the
 * motion blend, the root matrix (Skel_SetRootMatrix) and, unless the slot's countdown func_003D3228 runs, the world
 * matrices (Skel_ComputeWorld) and the joint projections (func_0034FB48). */
void Skel_Update(Fighter *f) {
    func_0034E018();
    func_003ADFB0();
    func_00365300(f);
    if (func_00346378(f->unk1E) != 0) {
        func_00353280(f->unk1E);
    }
    Motion_SampleRoot((u8 *)f->unkB8->unk0, D_005F5E70, f->unkBC - 1, f);
    Motion_SamplePose((u8 *)f->unkC0->unk0, D_00416A18, f->unkC4 - 1);
    f->unk30 = D_005F5E70[0];
    f->unk32 = D_005F5E70[1];
    f->unk34 = D_005F5E70[2] & 0x7FFF;
    func_0034FA60(f);
    if (gMode == 5) {
        func_00373E38(f);
    } else if (D_005F5F10 != 0) {
        func_00373428(f);
    }
    func_00365248(f);
    Skel_SetRootMatrix(f);
    func_003632E0(f);
    func_00362C28(f);
    func_00350728(f);
    if (func_003D3228((s16)f->unk20, 1) == 0) {
        Skel_ComputeWorld(f);
        func_003478F0(f, 0);
        func_00353878(f->unk1E, (u8 *)f + 0x590);
        func_0034FB48(f);
    }
}

/* Tail call of Skel_Update (func_00371B18 calls this one). */
void func_0034E8B0(Fighter *f) {
    Skel_Update(f);
}

/* Samples the root channels of the base motion and rebuilds the root matrix only. */
void func_0034E8B8(Fighter *f) {
    Motion_SampleRoot((u8 *)f->unkB8->unk0, D_005F5E70, f->unkBC - 1, f);
    Skel_SetRootMatrix(f);
}

/* The rotation angles of joint `j` in the decoded pose D_00416A18 (after its 3 root channels): -x, -y, z. */
void Skel_GetPoseAngles(s32 j, GteSVector *r) {
    s16 *p = D_00416A1E[j];

    r->vx = -p[0];
    r->vy = -p[1];
    r->vz = p[2];
}

/* NON_MATCHING: about 38 lines: register allocation of the float root-motion path (the original keeps two copies of
 * the angle and of the float distance) and the order of the integer path's loads; `- 0x8001 + 1` keeps the original's
 * addiu / negu (fake). */
/* The root matrix: rotation from (-unk18, unk3C, -(unk1C - 0x8000)) after func_00365108, translation = unk4..unkC minus
 * the root motion (height; distance along unk1A + root channel 0), which makes the position unk51C..unk524. Integer
 * (Gte_RotMatrixXYZ, gSinTable) unless D_005F6254 is set (floats, sinf / cosf). */
#if 0
void Skel_SetRootMatrix(Fighter *f) {
    SkelJoint *root = &SKEL(f)->unk504;
    GteSVector r;
    f32 a[3];
    FMatrix fm;
    s32 i;
    s32 k;
    u16 dy;
    u16 dist;
    s32 ang;
    u16 *mv;

    r.vx = -f->unk18;
    r.vy = f->unk3C;
    r.vz = -(f->unk1C - 0x8001 + 1); /* fake */
    func_00365108(f, &r);
    if (D_005F5EF8 != 0) {
        f->unk3C = r.vy;
    }
    if (func_003A9520()) {
        a[0] = ANGLE_TO_RAD2(r.vx);
        a[1] = ANGLE_TO_RAD2(r.vy);
        a[2] = ANGLE_TO_RAD2(r.vz);
        func_00349EE0(fm, a);
        for (i = 0; i < 3; i++) {
            for (k = 0; k < 3; k++) {
                root->unk4.m[i][k] = fm[i][k] * 4096.0f;
            }
        }
    } else {
        r.vx = r.vx >> 4;
        r.vy = r.vy >> 4;
        r.vz = r.vz >> 4;
        Gte_RotMatrixXYZ(&r, &root->unk4);
    }
    if (f->unkB5DC != 0 && f->unkB878 != 0) {
        f->unkB884 = (f->unkB880 << 12) / f->unkB878;
        dy = f->unkB5D2 = func_003A0340(f, D_005F5E70[1]);
    } else {
        f->unkB884 = 0x1000;
        dy = f->unkB5D2 = D_005F5E70[1];
    }
    mv = (u16 *)&D_005F5E70[2];
    dist = *mv;
    root->unk4.t[0] = f->unk4;
    root->unk4.t[1] = f->unk8;
    root->unk4.t[2] = f->unkC;
    if (f->unk11D == 0) {
        if (func_003A9520()) {
            ang = (f->unk1A + (s16)mv[-2]) & 0xFFFF;
            root->unk4.t[0] -= (s32)((f32)dist * sinf(ANGLE_TO_RAD2(ang)));
            root->unk4.t[1] -= dy;
            root->unk4.t[2] -= (s32)((f32)dist * cosf(ANGLE_TO_RAD2(ang)));
        } else {
            i = ((f->unk1A + (s16)mv[-2]) >> 4) & 0xFFF;
            root->unk4.t[1] -= dy;
            root->unk4.t[0] -= dist * gSinTable[i] / 4096;
            root->unk4.t[2] -= dist * gSinTable[i + 0x400] / 4096;
        }
    } else if (f->unk11D == 2) {
        root->unk4.t[1] -= dy;
    }
    func_00364F38(f);
}
#else
LIT4_WORD(D_005F5114, 0x38C90FDB);
LIT4_WORD(D_005F5118, 0x38C90FDB);
LIT4_WORD(D_005F511C, 0x38C90FDB);
LIT4_WORD(D_005F5120, 0x38C90FDB);
LIT4_WORD(D_005F5124, 0x38C90FDB);
LIT4_WORD(D_005F5128, 0x38C90FDB);
LIT4_WORD(D_005F512C, 0x38C90FDB);
LIT4_WORD(D_005F5130, 0x38C90FDB);
LIT4_WORD(D_005F5134, 0x38C90FDB);
LIT4_WORD(D_005F5138, 0x38C90FDB);
INCLUDE_ASM("asm/nonmatchings/skeleton", Skel_SetRootMatrix);
#endif

/* Rotation matrix of three 16-bit angles: func_003986D8 (fixed point), or while D_005F6254 is set the float Euler
 * matrix func_00349EE0 of the negated angles in radians, scaled to 4.12. */
void Skel_RotMatrixFromAngles(GteSVector *r, GteMatrix *m) {
    f32 a[3];
    FMatrix fm;
    s32 i;
    s32 k;

    if (func_003A9520()) {
        a[0] = -ANGLE_TO_RAD(r->vx);
        a[1] = -ANGLE_TO_RAD(r->vy);
        a[2] = -ANGLE_TO_RAD(r->vz);
        func_00349EE0(fm, a);
        for (i = 0; i < 3; i++) {
            for (k = 0; k < 3; k++) {
                m->m[i][k] = fm[i][k] * 4096.0f;
            }
        }
    } else {
        func_003986D8(r, m);
    }
}

/* World matrices: root world = root local; for joints 0-17 the local rotation from the pose (blended with unkB630 while
 * unkB5DC is set, kept in unkB390), then world = parent world * local; joints 5 / 6 may be replaced (unkB130 bit 0,
 * func_003626B8); joints 18..unkB050-1 after func_0034C508. Game mode 5 scales the root first. */
void Skel_ComputeWorld(Fighter *f) {
    GteSVector r;
    GteMatrix t;
    GteVector v;
    GteMatrix *m;
    SkelJoint *j;
    SkelJoint *jj;
    GteMatrix *alt;
    s32 i;
    s32 a;
    s32 b;
    s32 mode = gMode;

    if ((mode ^ 5) == 0) {
        v.vx = v.vy = v.vz = func_003AE6C0(f->unk2A);
        func_0039BFB0(&SKEL(f)->unk504.unk4, &v);
    }
    j = SKEL(f)->unk630;
    SKEL(f)->unk504.unk24 = SKEL(f)->unk504.unk4;
    jj = j;
    for (i = 0; i < 18; i++, j++) {
        Skel_GetPoseAngles(i, &r);
        if (i != 0 && f->unkB5DC != 0) {
            Skel_RotMatrixFromAngles(&r, &t);
            func_003A02C0(i, f, &t, f->unkB630[i], &j->unk4);
        } else {
            Skel_RotMatrixFromAngles(&r, &j->unk4);
        }
        ((FighterBlend *)f)->unkB390[i] = j->unk4;
    }
    j = jj;
    alt = func_0034FC68((s16)f->unk20, 0);
    for (i = 0; i < 18; i++, j++) {
        if (f->unkB130 & 1) {
            if (func_003626B8(f, i, &m)) {
                for (a = 0; a < 3; a++) {
                    for (b = 0; b < 3; b++) {
                        alt->m[a][b] = m->m[a][b];
                    }
                }
                alt++;
            }
        } else {
            m = &j->unk4;
        }
        Gte_CompMatrix(&j->unk48->unk24, m, &j->unk24);
    }
    if (f->unkB050 != 18) {
        for (i = 18; i < f->unkB050; i++, j++) {
            func_0034C508(f, i);
            if (f->unkB130 & 1) {
                func_003626B8(f, i, &m);
                for (a = 0; a < 3; a++) {
                    for (b = 0; b < 3; b++) {
                        alt->m[a][b] = m->m[a][b];
                    }
                }
                alt++;
            } else {
                m = &j->unk4;
            }
            Gte_CompMatrix(&j->unk48->unk24, m, &j->unk24);
        }
    }
}

/* Start points of the attack volumes (run first in a simulated frame, so they are last frame's joint positions): volume
 * 0 on joint unkC0->unk28 when unk29 is 0, and for unk118 == 2 volume 1 on unk2A when unk2B is 0. */
void Skel_SetVolumeStarts(Fighter *f) {
    s32 j;

    switch (f->unk118) {
    case 0:
        break;
    case 1:
        if (f->unkC0->unk29 == 0) {
            SKEL_VOLUME(f, j, f->unkC0->unk28, f->unk190, f->unk194, f->unk198);
        }
        break;
    case 2:
        if (f->unkC0->unk29 == 0) {
            SKEL_VOLUME(f, j, f->unkC0->unk28, f->unk190, f->unk194, f->unk198);
        }
        if (f->unkC0->unk2B == 0) {
            SKEL_VOLUME(f, j, f->unkC0->unk2A, f->unk1A8, f->unk1AC, f->unk1B0);
        }
        break;
    }
}

/* Hit shapes on joints: virtual joints 22 / 23 for some states (offsets D_005F5E50..D_005F5E68 from joints 10, 2 and
 * 6), the 14 body spheres on the joints of D_0042F060 (sphere 8 raised by 120, sphere 11 by 60), the 8 push spheres on
 * D_0042EFB8, unk388 = sphere 0, and the attack volume end points (and second-joint start points) of the current move. */
void Skel_PlaceHitShapes(Fighter *f) {
    GteSVector o0 = *(GteSVector *)D_005F5E50;
    GteSVector o1 = *(GteSVector *)D_005F5E58;
    GteSVector o2 = *(GteSVector *)D_005F5E60;
    GteSVector o3 = *(GteSVector *)D_005F5E68;
    SkelJoint *J = SKEL(f)->unk630;
    s32 i;
    s32 k;
    s32 j;

    if (f->unk28 == 4) {
        Gte_ApplyMatrix(&SKEL(f)->unk630[10].unk24, &o0, (GteVector *)SKEL(f)->unk630[22].unk24.t);
        for (i = 0; i < 3; i++) {
            SKEL(f)->unk630[22].unk24.t[i] += SKEL(f)->unk630[10].unk24.t[i];
        }
        Gte_ApplyMatrix(&SKEL(f)->unk630[10].unk24, &o1, (GteVector *)SKEL(f)->unk630[23].unk24.t);
        for (i = 0; i < 3; i++) {
            SKEL(f)->unk630[23].unk24.t[i] += SKEL(f)->unk630[10].unk24.t[i];
        }
    }
    if (f->unk28 == 0xE && (f->unk2A == 0x14 || f->unk2A == 0xF)) {
        Gte_ApplyMatrix(&SKEL(f)->unk630[2].unk24, &o2, (GteVector *)SKEL(f)->unk630[23].unk24.t);
        for (i = 0; i < 3; i++) {
            SKEL(f)->unk630[23].unk24.t[i] += SKEL(f)->unk630[2].unk24.t[i];
        }
    }
    if (f->unk28 == 0x1A) {
        Gte_ApplyMatrix(&SKEL(f)->unk630[6].unk24, &o3, (GteVector *)SKEL(f)->unk630[22].unk24.t);
        for (i = 0; i < 3; i++) {
            SKEL(f)->unk630[22].unk24.t[i] += SKEL(f)->unk630[6].unk24.t[i];
        }
    }
    if (f->unk28 == 0x20) {
        Gte_ApplyMatrix(&SKEL(f)->unk630[6].unk24, &o3, (GteVector *)SKEL(f)->unk630[22].unk24.t);
        for (i = 0; i < 3; i++) {
            SKEL(f)->unk630[22].unk24.t[i] += SKEL(f)->unk630[6].unk24.t[i];
        }
    }
    for (i = 0; i < 14; i++) {
        f->unk1F0[i].unk0 = SKEL(f)->unk630[D_0042F060[i]].unk24.t[0];
        f->unk1F0[i].unk4 = SKEL(f)->unk630[D_0042F060[i]].unk24.t[1];
        f->unk1F0[i].unk8 = SKEL(f)->unk630[D_0042F060[i]].unk24.t[2];
        if (i == 8) {
            f->unk1F0[8].unk4 -= 0x78;
        }
        if (i == 11) {
            f->unk1F0[11].unk4 -= 0x3C;
        }
    }
    for (i = 0; i < 8; i++) {
        f->unk308[i].unk0 = SKEL(f)->unk630[D_0042EFB8[i]].unk24.t[0];
        f->unk308[i].unk4 = SKEL(f)->unk630[D_0042EFB8[i]].unk24.t[1];
        f->unk308[i].unk8 = SKEL(f)->unk630[D_0042EFB8[i]].unk24.t[2];
    }
    f->unk388 = f->unk1F0[0].unk0;
    f->unk38C = f->unk1F0[0].unk4;
    f->unk390 = f->unk1F0[0].unk8;
    if (f->unk118 != 0) {
        if (f->unkC0->unk29 == 0) {
            SKEL_VOLUME(f, j, f->unkC0->unk28, f->unk19C, f->unk1A0, f->unk1A4);
        } else {
            SKEL_VOLUME(f, j, f->unkC0->unk28, f->unk19C, f->unk1A0, f->unk1A4);
            SKEL_VOLUME(f, j, f->unkC0->unk29, f->unk190, f->unk194, f->unk198);
        }
        if (f->unkC0->unk2B == 0) {
            SKEL_VOLUME(f, j, f->unkC0->unk2A, f->unk1B4, f->unk1B8, f->unk1BC);
        } else {
            SKEL_VOLUME(f, j, f->unkC0->unk2A, f->unk1B4, f->unk1B8, f->unk1BC);
            SKEL_VOLUME(f, j, f->unkC0->unk2B, f->unk1A8, f->unk1AC, f->unk1B0);
        }
    }
}

/* When unk126 is set: moves the base position unk4 / unkC so that the root stays in place, from the root channels at
 * frame unkBC - unkDA - 1. */
void func_0034FA60(Fighter *f) {
    s16 root[4];
    s32 frame;
    s32 a;

    if (f->unk126 != 0) {
        f->unk126 = 0;
        frame = f->unkBC - f->unkDA - 1;
        Motion_SampleRoot((u8 *)f->unkB8->unk0, root, frame > -1 ? frame : 0, f);
        a = ((f->unk1A + root[0]) >> 4) & 0xFFF;
        f->unk4 = f->unk51C + (u16)root[2] * gSinTable[a] / 4096;
        f->unkC = f->unk524 + (u16)root[2] * gSinTable[a + 0x400] / 4096;
    }
}

/* func_003D3210(slot, 1) for a move with flag 0x10000 (the low 19 bits of unk24 seen as a bit field), then
 * Skel_ProjectJoints. */
void func_0034FB48(Fighter *f) {
    if (((MoveFlags24 *)&f->unkC0->unk24)->lo & 0x10000) {
        func_003D3210((s16)f->unk20, 1);
    }
    Skel_ProjectJoints(f);
}

/* NON_MATCHING: about 20 lines: the original multiplies the joint index by 0x50 three times (copies of the index), this
 * C once. */
/* Copies the bone offsets of joints 5, 6, 18, 19, 20 (D_004FC668) into the replacement matrices D_008791A0[side]. */
#if 0
void func_0034FBA0(s32 side) {
    S32x5 idx = D_004FC668;
    GteMatrix *m = D_008791A0[side];
    SkelJoint *j = SKEL(&gFighters[side])->unk630;
    s32 i;

    for (i = 0; i < 5; i++, m++) {
        m->t[0] = j[idx.v[i]].unk4.t[0];
        m->t[1] = j[idx.v[i]].unk4.t[1];
        m->t[2] = j[idx.v[i]].unk4.t[2];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/skeleton", func_0034FBA0);
#endif

/* Replacement matrix k of a side. */
GteMatrix *func_0034FC68(s32 side, s32 k) {
    return &D_008791A0[side][k];
}

/* The replacement matrix of joint 5 or 6 while unkB130 bit 0 is set, else NULL. */
GteMatrix *func_0034FC88(s32 side, s32 j) {
    GteMatrix *m = NULL;

    if (gFighters[side].unkB130 & 1) {
        switch (j) {
        case 5:
            m = func_0034FC68(side, 0);
            break;
        case 6:
            m = func_0034FC68(side, 1);
            break;
        }
    }
    return m;
}

/* GTE rotation = cam * m, translation = cam * m.t + cam.t: sets up the projection of m's origin. */
void func_0034FD10(GteMatrix *cam, GteMatrix *m) {
    GteMatrix t;

    Gte_SetRotMatrix(cam);
    Gte_ApplyRotMatrixLV((GteVector *)m->t, (GteVector *)t.t);
    t.t[0] += cam->t[0];
    t.t[1] += cam->t[1];
    t.t[2] += cam->t[2];
    Gte_SetTransMatrix(&t);
    Gte_MulRotMatrix(m);
}

/* NON_MATCHING: 1 line: the original 8-aligns the loop head with a nop, as for a for loop, yet keeps only the table's
 * lui out of the loop; written as a for loop the whole table address is hoisted, this goto loop is not aligned. */
/* Projects joints 11, 2, 14 and 17 with the camera matrix D_00436530 (when the joint order D_0051DE50 lists them):
 * joint 11 into unk624 (and the return value into unk626), the others' x into unk628 / unk62A / unk62C, which
 * Cmd_CheckSituation (kind 0x5C) reads. */
#if 0
void Skel_ProjectJoints(Fighter *f) {
    s16 xy[2];
    s32 i;

    i = 0;
loop:
    {
        switch (D_0051DE50[i]) {
        case 11:
            func_0034FD10(&D_00436530, &SKEL(f)->unk630[11].unk24);
            f->unk626 = func_00398600(0, 0, 0, &f->unk624);
            break;
        case 2:
            func_0034FD10(&D_00436530, &SKEL(f)->unk630[2].unk24);
            func_00398600(0, 0, 0, xy);
            f->unk628 = xy[0];
            break;
        case 14:
            func_0034FD10(&D_00436530, &SKEL(f)->unk630[14].unk24);
            func_00398600(0, 0, 0, xy);
            f->unk62A = xy[0];
            break;
        case 17:
            func_0034FD10(&D_00436530, &SKEL(f)->unk630[17].unk24);
            func_00398600(0, 0, 0, xy);
            f->unk62C = xy[0];
            break;
        }
    }
    if (++i < 18) {
        goto loop;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/skeleton", Skel_ProjectJoints);
#endif
