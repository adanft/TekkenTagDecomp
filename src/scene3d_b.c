/* 0x34FF10-0x3504C0 (module scene3d, after the skeleton cut; docs/systems/render.md section 3): the six packet senders of
 * the blade effect (func_00350C40 in src/scene3d_c.c). Each copies a 2-quadword DMA/GIF header from .data to the
 * scratchpad with a toSPR transfer, fills the vertex quadwords and sends the buffer on with func_00343178. The end is an
 * object edge [V: this compiler keeps a callee-saved register's 128-bit save mode for the rest of a file once a function
 * held a u128 in it; func_00350578 and func_00351978 match only without these senders in the same file]; the start is
 * the skeleton cut [I]. */
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

/* Sends the 2-quadword packet D_004543B0 through the scratchpad (toSPR, then func_00343178) [V]. Called at the end of
 * func_00350C40 [I: restores the GS state the effect changed]. */
void func_0034FF10(void) {
    u128 *p = func_00342FB0();

    func_00343128(p, D_004543B0, 2);
    Dma_Wait((volatile u32 *)0x1000D400);
    func_00343178(p, 2);
}

/* Sends the 2-quadword header D_004543D0 plus the quadword v (3 quadwords) [V]. */
void func_0034FF60(u128 v) {
    u128 *p = func_00342FB0();

    func_00343128(p, D_004543D0, 2);
    p[2] = v;
    Dma_Wait((volatile u32 *)0x1000D400);
    func_00343178(p, 3);
}

/* As func_0034FF60 with the header D_004543F0 [V]. */
void func_0034FFC0(u128 v) {
    u128 *p = func_00342FB0();

    func_00343128(p, D_004543F0, 2);
    p[2] = v;
    Dma_Wait((volatile u32 *)0x1000D400);
    func_00343178(p, 3);
}

/* As func_0034FF60 with the header D_00454410 [V]; func_00350C40 passes the TEX0 value of the effect texture [I]. */
void func_00350020(u128 v) {
    u128 *p = func_00342FB0();

    func_00343128(p, D_00454410, 2);
    p[2] = v;
    Dma_Wait((volatile u32 *)0x1000D400);
    func_00343178(p, 3);
}

/* 1 when a projected vertex (x, y in 12.4 fixed point, z) lies outside 0..0x10000 or beyond z 0xFFF00 [V]. */
static inline s32 Scene_IsOffScreen(s32 *v) {
    s32 r = 0;

    if ((u32)v[2] > 0xFFF00 || v[0] < 0 || v[0] > 0x10000 || v[1] < 0 || v[1] > 0x10000) {
        r = 1;
    }
    return r;
}

/* Sends one Gouraud triangle: the header D_00454430 then RGBAQ / XYZ2 pairs (8 quadwords); when a vertex is off screen
 * the last XYZ2 gets the ADC bit (0x8000 in its top word) so the GS does not draw it [V]. */
void func_00350080(u128 *xyz0, u128 *xyz1, u128 *xyz2, u128 *c0, u128 *c1, u128 *c2) {
    u128 *p = func_00342FB0();

    func_00343128(p, D_00454430, 2);
    p[3] = *xyz0;
    p[5] = *xyz1;
    p[7] = *xyz2;
    if ((Scene_IsOffScreen((s32 *)xyz0) | Scene_IsOffScreen((s32 *)xyz1) | Scene_IsOffScreen((s32 *)xyz2)) << 1) {
        ((s32 *)p)[0x1F] = 0x8000;
    }
    p[2] = *c0;
    p[4] = *c1;
    p[6] = *c2;
    Dma_Wait((volatile u32 *)0x1000D400);
    func_00343178(p, 8);
}

/* Sends one textured quad as a 4-vertex strip: the header D_00454450 then ST / RGBAQ / XYZ2 triples (14 quadwords);
 * the ADC bit is set on vertex 2 when one of vertices 0-2 is off screen, on vertex 3 when one of all four is [V]. */
void func_00350238(u128 *xyz0, u128 *xyz1, u128 *xyz2, u128 *xyz3, u128 *c0, u128 *c1, u128 *c2, u128 *c3,
                   u128 *st0, u128 *st1, u128 *st2, u128 *st3) {
    u128 *p = func_00342FB0();
    s32 clip;

    func_00343128(p, D_00454450, 2);
    p[4] = *xyz0;
    p[7] = *xyz1;
    p[10] = *xyz2;
    p[13] = *xyz3;
    clip = (Scene_IsOffScreen((s32 *)xyz0) | Scene_IsOffScreen((s32 *)xyz1) | Scene_IsOffScreen((s32 *)xyz2)) << 1;
    if (clip) {
        ((s32 *)p)[0x2B] = 0x8000;
    }
    clip |= Scene_IsOffScreen((s32 *)xyz3);
    if (clip) {
        ((s32 *)p)[0x37] = 0x8000;
    }
    p[3] = *c0;
    p[6] = *c1;
    p[9] = *c2;
    p[12] = *c3;
    p[2] = *st0;
    p[5] = *st1;
    p[8] = *st2;
    p[11] = *st3;
    Dma_Wait((volatile u32 *)0x1000D400);
    func_00343178(p, 14);
}

