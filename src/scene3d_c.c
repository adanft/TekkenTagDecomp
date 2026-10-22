/* 0x3504C0-0x351E98 (module scene3d; docs/systems/render.md section 3): the blade effect of the sword fighters (a
 * 75-entry trail of quads that re-sample the frame buffer, plus three glow fans) with its per-side history matrices and
 * spline (cubic Lagrange weights), and the two-part swing of character 20. The start is an object edge (see
 * src/scene3d_b.c); the end is a cut [I]. Its .lit4 (0x5F5614-0x5F5668) is a subsegment: in the original pool the words
 * of func_00351AA0 come before those of func_00350C40, so both functions' LIT4_WORD lines sit in front of
 * func_00350C40; func_003518B8's word (0x5F570C) lies in another part of the pool and stays in the generic chunk (the C
 * reads it with LIT4_REF). Every NON_MATCHING draft (#if 0) compiles when enabled. */
#include "common.h"
#include "fighter.h"


typedef f32 Vu0Matrix[4][4];
typedef f32 Vu0Vector[4];
typedef f32 sceVu0FVECTOR[4] __attribute__((aligned(16)));

typedef struct {
    u64 lo;
    u64 hi;
} U64Pair;

typedef union {
    U64Pair p;
    u128 q;
} QWord;

/* One projected trail entry on the scratchpad (0x60 bytes): three screen positions (12.4 fixed point, z) and the
 * texture coordinates taken from them [V layout; I meaning]. */
typedef struct {
    /* 0x00 */ s32 xyz[3][4];
    /* 0x30 */ f32 st[3][4];
} TrailVtx;

/* Per-side state of the two-part swing animation of character 20 (func_003519E0, func_00351A60, func_00351AA0). */
typedef struct {
    /* 0x00 */ f32 unk0;    /* angle of part 0 */
    /* 0x04 */ f32 unk4;    /* angle of part 1 */
    /* 0x08 */ s8 unk8;     /* direction: 0 opening, 1 closing */
    /* 0x09 */ s8 unk9;     /* phase: 0 idle sway, 1 / 2 start a fast swing, 3 fast swing */
    /* 0x0A */ s8 unkA[2];  /* the two model parts (kinds 21 and 24) */
    /* 0x0C */ f32 unkC;    /* speed of part 0 */
    /* 0x10 */ f32 unk10;   /* speed of part 1 */
    /* 0x14 */ f32 unk14;   /* 1 / duration (func_00351A60) */
} Unk1FC23C0; /* 0x18 */

typedef struct {
    /* 0x00 */ u8 pad0[0x1C];
    /* 0x1C */ s32 unk1C;
} Unk3519E0Part; /* 0x20 */

typedef struct {
    /* 0x000 */ u8 pad0[0x4];
    /* 0x004 */ s32 unk4;
    /* 0x008 */ u8 pad8[0x18C];
    /* 0x194 */ Unk3519E0Part *unk194;
} Unk3519E0Model;

/* A joint record of the skeleton passed to func_00351AA0 (0xC0 bytes) [I]. */
typedef struct {
    /* 0x00 */ Vu0Matrix unk0;
    /* 0x40 */ Vu0Matrix unk40;
    /* 0x80 */ u128 unk80[3];
    /* 0xB0 */ f32 (*unkB0)[4];
    /* 0xB4 */ u8 padB4[0xC];
} SkelJoint; /* 0xC0 */

extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern u8 D_005F6FD8[8];
extern u8 D_005F6FE0[8];
extern s32 D_005F6FE8[2];
extern s32 D_005F6FF0[2];
extern s32 D_005F6FF8[2];
extern s32 D_005F7000[2];
extern s32 D_005F7008[2];
extern s32 D_005F7010[2];
extern s32 D_005F7018[2];
extern u128 D_004543B0[];
extern u128 D_004543D0[];
extern u128 D_004543F0[];
extern u128 D_00454410[];
extern u128 D_00454430[];
extern u128 D_00454450[];
extern Vu0Vector D_00454470[];
extern Vu0Vector D_004544A0[];
extern s32 D_004544D0[][6][4];
extern f32 D_00454710[][2];
extern f32 D_00454768[][2];
extern u128 D_004547C0[][10];
extern u128 D_00454B80[][11];
extern s32 D_00454FA0[][3];
extern s32 D_00455000[][3];
extern Vu0Matrix D_00433400;
extern Vu0Matrix D_004334C0;
extern const U64Pair D_00521E20;
extern const U64Pair D_00521E30;
extern Unk1FC23C0 D_01FC23C0[];
extern u128 D_01FC2420[][12];
extern u128 D_01FC25A0[][75][3];
extern Vu0Vector D_01FC41C0[];
extern Fighter gFighters[4];

u128 *func_00342FB0(void);
void func_00343128(void *spr, void *src, u32 qwc);
u32 Dma_Wait(volatile u32 *chan);
void func_00343178(void *spr, u32 qwc);
u32 Spr_Setup(u32 count, s32 reserved);
u32 func_003988E0(void);
s32 func_003A9698(void);
s32 func_003ABCC0(void);
s32 func_003AA460(void);
s32 func_003AA478(void);
u8 *func_003B87F8(s32 id);
void func_00356760(void *out, void *in);
void func_00349D70(Vu0Matrix m, void *in);
f32 atan2f(f32 y, f32 x);
f32 sqrtf(f32 x);
void sceVu0ApplyMatrix(Vu0Vector out, Vu0Matrix m, Vu0Vector v);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0RotMatrixZ(Vu0Matrix m0, Vu0Matrix m1, f32 rz);
void sceVu0RotTransPersN(s32 (*out)[4], Vu0Matrix m, Vu0Vector *v, s32 n, s32 mode);
void sceVu0ITOF4Vector(Vu0Vector out, s32 *v);
void sceVu0FTOI4Vector(s32 *out, Vu0Vector v);
void sceVu0SubVector(Vu0Vector out, Vu0Vector a, Vu0Vector b);

void func_00350578(Fighter *f, s32 side);
void func_003517F0(u128 *h, Vu0Matrix m, Vu0Vector *v);
void func_00351850(s32 (*out)[4], Vu0Matrix m, Vu0Vector *v);
void func_00351860(Vu0Vector *out, Vu0Matrix m, Vu0Vector *v);
void func_003518B8(Vu0Vector *w, s32 n);
void func_00351978(void *out, Vu0Vector *in, s32 n, void *m);

/* Copies one quadword. */
static inline void Vec_Copy(void *d, void *s) {
    *(u128 *)d = *(u128 *)s;
}

/* For a fighter that carries the blade (character 4, or 15 / 33 with costume 4): rebuilds the spline weights
 * D_01FC41C0 (5 + 1 points) and clears its trail flags D_005F6FD8 / D_005F6FE0 [V code; I meaning]. */
void func_003504C0(Fighter *f) {
    if (f->unk2A == 4 || ((f->unk2A == 15 || f->unk2A == 33) && f->unk28 == 4)) {
        func_003518B8(D_01FC41C0, 5);
        D_005F6FD8[f->unk1E] = 0;
        D_005F6FE0[f->unk1E] = 0;
    }
}

/* The three blade points (local space) for a weapon kind: D_004544A0 for 0x41-0x45 and 0x78, else D_00454470 [V]. */
Vu0Vector *func_00350540(s32 c) {
    switch (c) {
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x78:
        return D_004544A0;
    }
    return D_00454470;
}

/* Restarts the trail of side `side` at the blade's current place (outside the pause): the three blade points through
 * the joint matrix at Fighter+0x3CF0 become the last row of the three history matrices D_01FC2420[side], the spline
 * gives the 5 newest entries 70-74 of the ring D_01FC25A0[side], and every older entry and history row is set to them
 * [V code; I meaning]. */
void func_00350578(Fighter *f, s32 side) {
    sceVu0FVECTOR a[5];
    sceVu0FVECTOR b[5];
    sceVu0FVECTOR c[5];
    u128 *h;
    u128 (*t)[3];
    s32 skip;
    s32 i;
    s32 j;
    s32 k;

    skip = 0;
    if (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
        skip = 1;
    }
    if (skip == 0) {
        h = D_01FC2420[side];
        t = D_01FC25A0[side];
        func_003517F0(h, (void *)((u8 *)f + 0x3CF0), func_00350540(f->unk2E));
        func_00351978(a, D_01FC41C0, 5, &h[0]);
        func_00351978(b, D_01FC41C0, 5, &h[4]);
        func_00351978(c, D_01FC41C0, 5, &h[8]);
        for (i = 0; i < 5; i++) {
            Vec_Copy(&t[70 + i][0], a[i]);
            Vec_Copy(&t[70 + i][1], b[i]);
            Vec_Copy(&t[70 + i][2], c[i]);
        }
        for (j = 0, i = 5; i < 75; i++, j++) {
            Vec_Copy(&t[j][0], &t[70][0]);
            Vec_Copy(&t[j][1], &t[70][1]);
            Vec_Copy(&t[j][2], &t[70][2]);
        }
        for (k = 0; k < 3; k++) {
            Vec_Copy(&h[k], &h[3]);
            Vec_Copy(&h[4 + k], &h[7]);
            Vec_Copy(&h[8 + k], &h[11]);
        }
    }
}

/* NON_MATCHING: 4 lines + register names: s2 / s3 swapped for side / move record; the original tests 0xEEE on the
 * sign-extended copy of the lhu'd move id (sll / sra) where this reloads the field with lh. */
/* Once per frame (Skel_Update) for a blade fighter: decides from the current move id (Fighter.unk10C) whether its
 * blade trail is drawn (D_005F6FD8[side] = 1): a list of moves draws it, another never does, any other move draws it
 * when the move record has kind 0x16, 0x17 or 0xA in unk28-unk2B; moves 0x12A2-0x12A5 from frame 6, or while
 * D_005F6FE0[side] and unkE0 > 0, draw it, move 0xEEE never. When it is off the trail is restarted (func_00350578).
 * With func_003ABCC0 set (a replay / demo mode [I]) mode 7 draws it from frame 0x37A on [V code; I meaning]. */
#if 0
void func_00350728(Fighter *f) {
    struct FighterMove *m;
    s32 side = f->unk1E;
    s32 on;
    u16 id;

    if (f->unk2A == 4 || ((f->unk2A == 15 || f->unk2A == 33) && f->unk28 == 4)) {
        m = f->unkC0;
        on = 0;
        if (func_003ABCC0() != 0) {
            if (func_003AA460() == 7) {
                on = func_003AA478() >= 0x37A;
            }
        } else {
            id = f->unk10C;
            switch (f->unk10C) {
            case 0x15A:
            case 0x15F:
            case 0x16F:
            case 0x176:
            case 0x181:
            case 0x189:
            case 0x18F:
            case 0x197:
            case 0x1AD:
            case 0x1B5:
            case 0x617:
            case 0xE5C:
            case 0x1225:
            case 0x1226:
            case 0x1227:
            case 0x1228:
            case 0x1229:
            case 0x122B:
            case 0x122C:
            case 0x122D:
            case 0x1230:
                on = 0;
                break;
            default:
                on = 0;
                if (!(m->unk28 == 0x16 || m->unk29 == 0x16 || m->unk2A == 0x16 || m->unk2B == 0x16 ||
                      m->unk28 == 0x17 || m->unk29 == 0x17 || m->unk2A == 0x17 || m->unk2B == 0x17 ||
                      m->unk28 == 0xA || m->unk29 == 0xA || m->unk2A == 0xA || m->unk2B == 0xA)) {
                    break;
                }
            case 0x43:
            case 0x74:
            case 0x77:
            case 0x7A:
            case 0x7B:
            case 0x7C:
            case 0x7E:
            case 0x80:
            case 0x82:
            case 0x84:
            case 0x86:
            case 0x1A8:
            case 0x4CC:
            case 0x5A5:
            case 0x5AA:
            case 0x5AB:
            case 0x5B8:
            case 0x5BD:
            case 0x5C0:
            case 0x5C3:
            case 0x5CA:
            case 0x5CB:
            case 0x5CC:
            case 0x5DA:
            case 0x5DB:
            case 0x602:
            case 0x607:
            case 0x608:
            case 0x60C:
            case 0x60E:
            case 0x60F:
            case 0x618:
            case 0x622:
            case 0x633:
            case 0xEE4:
            case 0xEE8:
            case 0xEED:
            case 0xEEF:
            case 0xF22:
            case 0xF60:
            case 0xF61:
            case 0x1266:
                on = 1;
                break;
            }
            if ((D_005F6FE0[side] != 0 && f->unkE0 > 0) || (id >= 0x12A2 && id <= 0x12A5 && f->unkC4 >= 6)) {
                on = 1;
            }
            if (f->unk10C == 0xEEE) {
                on = 0;
            }
        }
        if (on == 1) {
            D_005F6FD8[side] = on;
        } else {
            func_00350578(f, side);
            D_005F6FD8[side] = 0;
            D_005F6FE0[side] = 0;
        }
        if (f->unk118 != 0 && (u8)f->unk11B == 0) {
            D_005F6FE0[side] = 1;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d_c", func_00350728);
#endif

/* NON_MATCHING: typed draft with the original control flow (about 830 differing lines of 842): register allocation and
 * stack layout of the glow part, loop-invariant hoisting, and the TEX0 value, which the original moves into a TImode
 * register (por) but this gcc refuses as a cast ("unsupported wide integer operation"), so it goes through memory. */
/* The blade effect of one fighter, drawn after its model (func_00357DD8): picks a colour style from the weapon kind
 * (Fighter.unk2E), then (1) when the trail is on: advances the trail ring by 5 spline points (outside the pause),
 * projects the 75 entries x 3 points to the scratchpad with screen-derived texture coordinates, sets the GS state
 * (func_0034FF60 / func_0034FFC0 / func_00350020 with a TEX0 on the texture func_003988E0 gives) and draws 73 x 2
 * textured quads fading out along the trail; (2) unless a replay mode is in frames 0x1B0-0x1CF: projects the three
 * blade points and draws three fans of 12 / 8 / 8 Gouraud triangles (index tables D_00455000 / D_00454FA0 into the
 * shapes D_00454710 / D_00454768 and colours D_00454B80 / D_004547C0) rotated along the blade and scaled by its
 * screen length; finally func_0034FF10 [V code; I meaning]. */
#if 0
void func_00350C40(Fighter *f) {
    QWord cmd;
    sceVu0FVECTOR a[5];
    sceVu0FVECTOR b[5];
    sceVu0FVECTOR c5[5];
    sceVu0FVECTOR loc[3];
    Vu0Matrix mtx;
    QWord cmd2;
    QWord tex;
    Vu0Matrix unit;
    Vu0Matrix rot;
    sceVu0FVECTOR p0;
    sceVu0FVECTOR p1;
    sceVu0FVECTOR p2;
    sceVu0FVECTOR d;
    s32 tri[6][4];
    sceVu0FVECTOR v;
    Vu0Vector *shape;
    s32 style;
    s32 n;
    s32 z;
    s32 over;
    u128 *colors;
    s32 (*c)[4];
    s32 (*spr)[4];
    u128 *h;
    u128 (*t)[3];
    TrailVtx *p;
    TrailVtx *q;
    f32 len;
    f32 scale;
    f32 k;
    f32 zf;
    s32 side = f->unk1E;
    s32 paused;
    s32 alpha;
    s32 i;
    s32 j;
    s32 *idx;

    paused = 0;
    shape = func_00350540(f->unk2E);
    if (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
        paused = 1;
    }
    if (f->unk2A == 4 || ((f->unk2A == 15 || f->unk2A == 33) && f->unk28 == 4)) {
        switch ((s16)(f->unk2E - 0x11)) {
        case 0x13 - 0x11: /* [I] two values that keep the original's jump table (gcc needs 11 cases for its range) */
        case 0x15 - 0x11:
        default:
            style = 2;
            break;
        case 0x14 - 0x11:
            style = 3;
            break;
        case 0x11 - 0x11:
            style = 0;
            break;
        case 0x12 - 0x11:
            style = 1;
            break;
        case 0x41 - 0x11:
        case 0x42 - 0x11:
        case 0x44 - 0x11:
        case 0x78 - 0x11:
            style = 4;
            break;
        case 0x74 - 0x11:
        case 0x75 - 0x11:
            style = 5;
            break;
        }
        spr = (s32 (*)[4])Spr_Setup(2, 0x3D20);
        if (D_005F6FD8[side] != 0) {
            /* the blade trail: 75 entries of three points, drawn as quads that sample the frame buffer */
            cmd.p = D_00521E30;
            func_0034FF60(cmd.q);
            h = D_01FC2420[side];
            t = D_01FC25A0[side];
            if (paused == 0) {
                s32 i;
                s32 j;
                s32 k;

                func_003517F0(h, (void *)((u8 *)f + 0x3CF0), shape);
                for (j = 0, i = 5; i < 75; i++, j++) {
                    Vec_Copy(&t[j][0], &t[i][0]);
                    Vec_Copy(&t[j][1], &t[i][1]);
                    Vec_Copy(&t[j][2], &t[i][2]);
                }
                func_00351978(a, D_01FC41C0, 5, &h[0]);
                func_00351978(b, D_01FC41C0, 5, &h[4]);
                func_00351978(c5, D_01FC41C0, 5, &h[8]);
                for (i = 0; i < 5; i++) {
                    Vec_Copy(&t[70 + i][0], a[i]);
                    Vec_Copy(&t[70 + i][1], b[i]);
                    Vec_Copy(&t[70 + i][2], c5[i]);
                }
                for (j = 0, k = 1; k < 4; k++, j++) {
                    Vec_Copy(&h[j], &h[k]);
                    Vec_Copy(&h[4 + j], &h[4 + k]);
                    Vec_Copy(&h[8 + j], &h[8 + k]);
                }
            }
            p = (TrailVtx *)spr;
            for (i = 0; i < 75; i++) {
                sceVu0RotTransPersN(p->xyz, D_004334C0, (Vu0Vector *)t[i], 3, 0);
                p->xyz[0][3] = 0;
                p->xyz[1][3] = 0;
                p->xyz[2][3] = 0;
                p->st[0][2] = 1.0f;
                p->st[1][2] = 1.0f;
                p->st[2][2] = 1.0f;
                p->st[0][0] = p->xyz[0][0] * (1.0f / 16384.0f) - 1.6875f;
                p->st[0][1] = p->xyz[0][1] * (1.0f / 16384.0f) - 1.78125f;
                p->st[1][0] = p->xyz[1][0] * (1.0f / 16384.0f) - 1.6875f;
                p->st[1][1] = p->xyz[1][1] * (1.0f / 16384.0f) - 1.78125f;
                p->st[2][0] = p->xyz[2][0] * (1.0f / 16384.0f) - 1.6875f;
                p->st[2][1] = p->xyz[2][1] * (1.0f / 16384.0f) - 1.78125f;
                p++;
            }
            n = 75;
            q = (TrailVtx *)spr + 72;
            alpha = 75;
            c = D_004544D0[style];
            func_0034FFC0(0x70000A0000A);
            tex.p.lo = (u32)(func_003988E0() | 0x28128000) | ((u64)0xA000 << 18);
            func_00350020(tex.q);
            do {
                c[0][3] = alpha;
                c[1][3] = alpha;
                c[2][3] = alpha;
                alpha--;
                c[3][3] = alpha;
                c[4][3] = alpha;
                c[5][3] = alpha;
                n--;
                func_00350238((u128 *)q[2].xyz[0], (u128 *)q[2].xyz[1], (u128 *)q[1].xyz[0], (u128 *)q[1].xyz[1],
                              (u128 *)c[0], (u128 *)c[1], (u128 *)c[3], (u128 *)c[4], (u128 *)q[1].st[0],
                              (u128 *)q[1].st[1], (u128 *)q[0].st[0], (u128 *)q[0].st[1]);
                func_00350238((u128 *)q[2].xyz[1], (u128 *)q[2].xyz[2], (u128 *)q[1].xyz[1], (u128 *)q[1].xyz[2],
                              (u128 *)c[1], (u128 *)c[2], (u128 *)c[4], (u128 *)c[5], (u128 *)q[1].st[1],
                              (u128 *)q[1].st[2], (u128 *)q[0].st[1], (u128 *)q[0].st[2]);
                q--;
            } while (n >= 3);
        }
        if (!(func_003ABCC0() != 0 && func_003AA460() == 7 && func_003AA478() >= 0x1B0 && func_003AA478() < 0x1D0)) {
            /* the glow around the blade: three fans of flat triangles at the projected blade points */
            func_00351850(spr, (void *)((u8 *)f + 0x3D30), shape);
            spr[0][3] = 0;
            spr[1][3] = 0;
            spr[2][3] = 0;
            sceVu0MulMatrix(mtx, D_00433400, (void *)((u8 *)f + 0x3CF0));
            func_00351860(loc, mtx, shape);
            cmd2.p = D_00521E20;
            func_0034FF60(cmd2.q);
            *(u128 *)unit[0] = 0;
            *(u128 *)unit[1] = 0;
            *(u128 *)unit[2] = 0;
            *(u128 *)unit[3] = 0;
            unit[0][0] = 1.0f;
            unit[3][3] = 1.0f;
            unit[1][1] = 1.0f;
            unit[2][2] = 1.0f;
            zf = 262144000.0f / (loc[1][2] - 350.0f);
            sceVu0ITOF4Vector(p0, spr[0]);
            sceVu0ITOF4Vector(p1, spr[1]);
            sceVu0ITOF4Vector(p2, spr[2]);

            sceVu0SubVector(d, p0, p1);
            len = sqrtf(d[0] * d[0] + d[1] * d[1]);
            z = zf;
            k = 0.083333336f;
            over = 0xFFFF0 < z;
            sceVu0RotMatrixZ(rot, unit, atan2f(d[0], -d[1]));
            scale = len * 0.15f;
            colors = D_00454B80[style];
            for (i = 0; i < 12; i++) {
                for (j = 0; j < 3; j++) {
                    idx = &D_00455000[i][j];
                    v[2] = 0.0f;
                    v[3] = 1.0f;
                    v[1] = D_00454710[*idx][1] * scale;
                    v[0] = D_00454710[*idx][0];
                    sceVu0ApplyMatrix(v, rot, v);
                    v[0] *= k;
                    v[1] *= k;
                    sceVu0FTOI4Vector(tri[j], v);
                    tri[j][0] += spr[0][0];
                    tri[j][2] = z;
                    tri[j][1] += spr[0][1];
                    if (over) {
                        tri[j][2] = 0xFFFF0;
                    }
                    tri[j][3] = 0;
                    *(u128 *)tri[j + 3] = colors[*idx];
                }
                func_00350080((u128 *)tri[0], (u128 *)tri[1], (u128 *)tri[2], (u128 *)tri[3], (u128 *)tri[4],
                              (u128 *)tri[5]);
            }

            scale = len * 0.15f;
            k = 0.083333336f;
            z = zf;
            over = 0xFFFF0 < z;
            colors = D_004547C0[style];
            for (i = 0; i < 8; i++) {
                for (j = 0; j < 3; j++) {
                    idx = &D_00454FA0[i][j];
                    v[2] = 0.0f;
                    v[1] = D_00454768[*idx][1] * scale;
                    v[0] = D_00454768[*idx][0];
                    v[3] = 1.0f;
                    sceVu0ApplyMatrix(v, rot, v);
                    v[0] *= k;
                    v[1] *= k;
                    sceVu0FTOI4Vector(tri[j], v);
                    tri[j][0] += spr[1][0];
                    tri[j][1] += spr[1][1];
                    tri[j][2] = z;
                    if (over) {
                        tri[j][2] = 0xFFFF0;
                    }
                    tri[j][3] = 0;
                    *(u128 *)tri[j + 3] = colors[*idx];
                }
                func_00350080((u128 *)tri[0], (u128 *)tri[1], (u128 *)tri[2], (u128 *)tri[3], (u128 *)tri[4],
                              (u128 *)tri[5]);
            }

            sceVu0SubVector(d, p1, p2);
            len = sqrtf(d[0] * d[0] + d[1] * d[1]);
            z = zf;
            k = 0.083333336f;
            over = 0xFFFF0 < z;
            sceVu0RotMatrixZ(rot, unit, atan2f(d[0], -d[1]));
            scale = len * 0.29714283f;
            colors = D_004547C0[style];
            for (i = 0; i < 8; i++) {
                for (j = 0; j < 3; j++) {
                    idx = &D_00454FA0[i][j];
                    v[2] = 0.0f;
                    v[1] = D_00454768[*idx][1] * scale;
                    v[0] = D_00454768[*idx][0];
                    v[3] = 1.0f;
                    sceVu0ApplyMatrix(v, rot, v);
                    v[0] *= k;
                    v[1] *= k;
                    sceVu0FTOI4Vector(tri[j], v);
                    tri[j][0] += spr[2][0];
                    tri[j][1] += spr[2][1];
                    tri[j][2] = z;
                    if (over) {
                        tri[j][2] = 0xFFFF0;
                    }
                    tri[j][3] = 0;
                    *(u128 *)tri[j + 3] = colors[*idx];
                }
                func_00350080((u128 *)tri[0], (u128 *)tri[1], (u128 *)tri[2], (u128 *)tri[3], (u128 *)tri[4],
                              (u128 *)tri[5]);
            }
        }
        func_0034FF10();
    }
}
#else
LIT4_WORD(D_005F5614, 0x3E8EFA35);
LIT4_WORD(D_005F5618, 0x3EB2B8C3);
LIT4_WORD(D_005F561C, 0x3F4D87AD);
LIT4_WORD(D_005F5620, 0x3F4D87AD);
LIT4_WORD(D_005F5624, 0x3F9C61AA);
LIT4_WORD(D_005F5628, 0x3C64C388);
LIT4_WORD(D_005F562C, 0x3E8EFA35);
LIT4_WORD(D_005F5630, 0x3C8EFA36);
LIT4_WORD(D_005F5634, 0x3EB2B8C3);
LIT4_WORD(D_005F5638, 0x3C64C388);
LIT4_WORD(D_005F563C, 0x3C8EFA36);
LIT4_WORD(D_005F5640, 0x3F4D87AD);
LIT4_WORD(D_005F5644, 0x3F9C61AA);
LIT4_WORD(D_005F5648, 0x3C8EFA35);
LIT4_WORD(D_005F564C, 0xBF060A92);
LIT4_WORD(D_005F5650, 0x3DAAAAAB);
LIT4_WORD(D_005F5654, 0x3E19999A);
LIT4_WORD(D_005F5658, 0x3E19999A);
LIT4_WORD(D_005F565C, 0x3DAAAAAB);
LIT4_WORD(D_005F5660, 0x3DAAAAAB);
LIT4_WORD(D_005F5664, 0x3E98231B);
INCLUDE_ASM("asm/nonmatchings/scene3d_c", func_00350C40);
#endif

/* NON_MATCHING: 2 lines: the original saves s2 with sq / lq. This compiler keeps the save mode of a callee-saved register
 * from earlier functions of the file (s2 holds a TImode value in func_00350C40), so it matches once func_00350C40 is C
 * with s2 in TImode. T19 b2: a stub giving s2 a TImode use (ASM_STUB) matches this function and func_00351860, but the
 * save mode then sticks to func_00351978, which the original saves with sd: not that mechanism alone. */
/* The three blade points v[0..2] through matrix m become the last rows of the three history matrices h[2], h[1],
 * h[0] (in that order) [V]. */
#if 0
void func_003517F0(u128 *h, Vu0Matrix m, Vu0Vector *v) {
    sceVu0ApplyMatrix((f32 *)&h[11], m, v[0]);
    sceVu0ApplyMatrix((f32 *)&h[7], m, v[1]);
    sceVu0ApplyMatrix((f32 *)&h[3], m, v[2]);
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d_c", func_003517F0);
#endif

/* Projects three points (sceVu0RotTransPersN, mode 0) [V]. */
void func_00351850(s32 (*out)[4], Vu0Matrix m, Vu0Vector *v) {
    sceVu0RotTransPersN(out, m, v, 3, 0);
}

/* NON_MATCHING: 2 lines: the sq / lq save of s2, as func_003517F0. */
/* Transforms three points by m [V]. */
#if 0
void func_00351860(Vu0Vector *out, Vu0Matrix m, Vu0Vector *v) {
    sceVu0ApplyMatrix(out[0], m, v[0]);
    sceVu0ApplyMatrix(out[1], m, v[1]);
    sceVu0ApplyMatrix(out[2], m, v[2]);
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d_c", func_00351860);
#endif

/* Cubic Lagrange weights through four points at t = 1 .. 1 + n / (n - 1) (n + 1 entries): w[i] holds -t(t-1)(t-2)/6,
 * (t+1)(t-1)(t-2)/2, -(t+1)t(t-2)/2, (t+1)t(t-1)/6 [V]. */
/* Its .lit4 word (1 / 6, D_005F570C) lies after render_fighter_b's words in the generic part of the pool, outside this
 * object's (0x5F5614-0x5F5668): LIT4_REF. */
LIT4_REF(D_005F570C);
void func_003518B8(Vu0Vector *w, s32 n) {
    s32 i;
    f32 t;
    f32 step;

    step = 1.0f / (n - 1);
    t = 1.0f;
    for (i = 0; i <= n; i++) {
        f32 a = t + 1.0f;
        f32 b = t - 1.0f;
        f32 c = t - 2.0f;

        w[i][0] = -t * b * c * (1.0f / 6.0f);
        w[i][1] = a * b * c * 0.5f;
        w[i][2] = -a * t * c * 0.5f;
        w[i][3] = a * t * b * (1.0f / 6.0f);
        t += step;
    }
}

/* out[i] = m (one history matrix: four control points) applied to the weights in[i], i = n-1 .. 0 [V]. */
void func_00351978(void *out, Vu0Vector *in, s32 n, void *m) {
    while (n > 0) {
        n--;
        sceVu0ApplyMatrix(((Vu0Vector *)out)[n], m, in[n]);
    }
}

/* For character 20: resets the side's swing state and finds the two model parts of kind 21 and 24 [V code; I]. */
void func_003519E0(Fighter *f, Unk3519E0Model *m) {
    Unk1FC23C0 *e;
    Unk3519E0Part *p;
    s32 i;
    s32 n;
    s32 side = f->unk1E;

    if (f->unk2A == 20) {
        e = &D_01FC23C0[side];
        e->unk8 = 0;
        e->unk0 = 0;
        e->unk4 = 0;
        n = m->unk4;
        p = m->unk194;
        for (i = 0; i < n; i++, p++) {
            switch (p->unk1C) {
            case 21:
                e->unkA[0] = i;
                break;
            case 24:
                e->unkA[1] = i;
                break;
            }
        }
    }
}

/* Starts a swing of side `side`: phase `phase`, speed scale 1 / n [V]. */
void func_00351A60(s32 side, s32 phase, s32 n) {
    Unk1FC23C0 *e = &D_01FC23C0[side];

    e->unk9 = phase;
    e->unk14 = 1.0f / n;
}

/* NON_MATCHING: 31 lines: the loop keeps &out (sp+0x20) in a saved register (loop-invariant motion the original does
 * not do), so the parts pointer is spilled and the registers of data / mats shift. */
/* For character 20 (from func_003478F0): steps the swing of its two parts (sway between 0 and 16 / 20 degrees, or a
 * fast swing up to 46 / 70 degrees started by func_00351A60), then rotates each part matrix about Z by its angle
 * (part 0 by 1 degree - a, part 1 by b - 30 degrees) and rebuilds the joint world, view and screen matrices
 * [V code; I meaning]. */
#if 0
void func_00351AA0(Fighter *f, s32 unused, SkelJoint *joints, Vu0Matrix *mats, Vu0Matrix view) {
    sceVu0FVECTOR tmp;
    sceVu0FVECTOR save;
    u128 out[4];
    Unk1FC23C0 *e;
    SkelJoint *j;
    f32 (*m)[4];
    f32 (*pm)[4];
    f32 *p;
    u8 *data;
    f32 a;
    f32 b;
    f32 sa;
    f32 sb;
    f32 k;
    f32 rz;
    s32 side = f->unk1E;
    s32 part;
    s32 dir;
    s32 phase;
    s32 i;

    if (f->unk2A == 20) {
        e = &D_01FC23C0[side];
        a = e->unk0;
        b = e->unk4;
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            phase = e->unk9;
            dir = e->unk8;
            sa = e->unkC;
            sb = e->unk10;
            k = e->unk14;
            switch (phase) {
            case 3:
                sa = k * 0.27925268f;
                sb = k * 0.34906587f;
                break;
            case 1:
                dir = 0;
                phase = 3;
                sa = k * 0.8028515f;
                sb = k * 1.2217305f;
                break;
            case 2:
                dir = 1;
                phase = 3;
                sa = k * 0.8028515f;
                sb = k * 1.2217305f;
                break;
            }
            switch (phase) {
            case 0:
                switch (dir) {
                case 0:
                    a += 0.013962634f;
                    b += 0.017453294f;
                    if (a > 0.27925268f) {
                        a = 0.27925268f;
                        dir = 1;
                    }
                    if (b > 0.34906587f) {
                        b = 0.34906587f;
                    }
                    break;
                case 1:
                    a -= 0.013962634f;
                    b -= 0.017453294f;
                    if (a < 0.0f) {
                        a = 0.0f;
                        dir = 0;
                    }
                    if (b < 0.0f) {
                        b = 0.0f;
                    }
                    break;
                }
                break;
            case 3:
                switch (dir) {
                case 0:
                    a += sa;
                    if (a > 0.8028515f) {
                        a = 0.8028515f;
                    }
                    b += sb;
                    if (b > 1.2217305f) {
                        b = 1.2217305f;
                    }
                    break;
                case 1:
                    a -= sa;
                    if (a < 0.0f) {
                        a = 0.0f;
                    }
                    b -= sb;
                    if (b < 0.0f) {
                        b = 0.0f;
                    }
                    break;
                }
                break;
            }
            e->unk8 = dir;
            e->unk9 = phase;
            e->unk0 = a;
            e->unk4 = b;
            e->unkC = sa;
            e->unk10 = sb;
        }
        data = func_003B87F8((s16)f->unk20);
        for (i = 0; i < 2; i++) {
            part = e->unkA[i];
            j = &joints[part];
            func_00356760(tmp, data + part * 0xC + 0x498);
            func_00349D70(mats[part], tmp);
            if (i == 0) {
                rz = 0.017453292f - e->unk0;
            } else {
                rz = e->unk4 + -0.5235988f;
            }
            m = mats[part];
            pm = j->unkB0;
            p = m[3];
            *(u128 *)save = *(u128 *)p;
            sceVu0RotMatrixZ(m, m, rz);
            *(u64 *)p = *(u64 *)save;
            ((s32 *)p)[2] = ((s32 *)save)[2];
            sceVu0MulMatrix(j->unk0, pm, m);
            sceVu0MulMatrix(j->unk40, D_004334C0, j->unk0);
            sceVu0MulMatrix((void *)out, view, j->unk0);
            j->unk80[0] = out[0];
            j->unk80[1] = out[1];
            j->unk80[2] = out[2];
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/scene3d_c", func_00351AA0);
#endif

/* Resets the seven per-side words D_005F6FE8 (to 1) .. D_005F7018 of fighter f [V]. */
void func_00351E10(Fighter *f) {
    s32 i = f->unk1E;

    D_005F6FE8[i] = 1;
    D_005F6FF0[i] = 0;
    D_005F6FF8[i] = 0;
    D_005F7000[i] = 0;
    D_005F7008[i] = 0;
    D_005F7010[i] = 0;
    D_005F7018[i] = 0;
}

/* The opponent's slot: gFighters[1] for side 0, else gFighters[0] [V]. */
Fighter *Fighter_GetOpponent(Fighter *f) {
    Fighter *g = gFighters;

    if (f->unk1E == 0) {
        g++;
    }
    return g;
}
