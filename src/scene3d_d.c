/* 0x351E98-0x3543F0 (module scene3d; docs/systems/render.md section 4): the fighter look-at (neck, head, eyes), the
 * light sets (stage, per fighter, flashes) and their VU0 light matrices, and the GS image upload over the GIF channel.
 * The start is a cut [I], the end the theater module. Its .lit4 (0x5F5668-0x5F5700) is a subsegment (LIT4_WORD for the
 * functions still in assembly). Every NON_MATCHING draft (#if 0) compiles when enabled. */
#include "common.h"
#include "fighter.h"

typedef f32 Vu0Matrix[4][4];
typedef f32 FVec4[4] __attribute__((aligned(16)));

typedef struct {
    f32 x, y, z;
} V3;

typedef struct {
    f32 v[4];
} __attribute__((aligned(16))) V4;

typedef struct {
    f32 v[4];
} __attribute__((aligned(8))) Vec4A8;

/* One light set in the sceVu0 light-matrix layout (0x80 bytes): three directions, three colours, the ambient. */
typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 pad4[3];
    /* 0x10 */ FVec4 dir[3];
    /* 0x40 */ FVec4 col[3];
    /* 0x70 */ FVec4 amb;
} LightSet; /* size 0x80 */

/* Per-fighter lights: the current set, the stage's base set, the flash colour. */
typedef struct {
    /* 0x000 */ LightSet cur;
    /* 0x080 */ LightSet base;
    /* 0x100 */ u128 unk100;
    /* 0x110 */ u128 flash;
} LightSlot; /* size 0x120 */

/* The whole light block at 0x1FC4210: the global set, the two fighter slots, the extra set. */
typedef struct {
    /* 0x000 */ LightSet global;
    /* 0x080 */ LightSlot slot[2];
    /* 0x2C0 */ LightSet extra;
} Lights; /* size 0x340 */

/* A light set as stored in the stage data: count, ambient, three (direction, colour) pairs. */
typedef struct {
    /* 0x0 */ f32 dir[3];
    /* 0xC */ f32 col[3];
} LightDefPair; /* size 0x18 */

typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ f32 amb[3];
    /* 0x10 */ LightDefPair l[3];
} LightDef; /* size 0x58 */

/* Fade distances and switch of the fighter light (.data). */
typedef struct {
    /* 0x0 */ f32 far;
    /* 0x4 */ f32 near;
    /* 0x8 */ s32 on;
} Unk4550B0;

/* A GS image upload record (0x60-byte header + image data): BITBLTBUF data at 0x10, quadword count and last flag at
 * 0x50. */
typedef struct {
    /* 0x00 */ u64 unk0[2];
    /* 0x10 */ u64 bitbltbuf;
    /* 0x18 */ u8 pad18[0x38];
    /* 0x50 */ s32 qwc;
    /* 0x54 */ u8 pad54[0xC];
} GsImageRec; /* size 0x60, followed by qwc quadwords */

/* Float joint of a fighter (0x1170 + k * 0xC0), filled by func_003478F0: world, view (D_004334C0 * world), the
 * three rows of screen * world, the parent's world matrix. */
typedef struct {
    /* 0x00 */ Vu0Matrix m0;
    /* 0x40 */ Vu0Matrix m1;
    /* 0x80 */ f32 m2[3][4];
    /* 0xB0 */ Vu0Matrix *parent;
    /* 0xB4 */ s32 padB4[3];
} FJoint; /* size 0xC0 */

/* Fighter slot view: the float joints and their local matrices. */
typedef struct {
    /* 0x0000 */ u8 pad0[0x1E];
    /* 0x001E */ s16 unk1E;
    /* 0x0020 */ u8 pad20[0xA0];
    /* 0x00C0 */ struct FighterMove *unkC0;
    /* 0x00C4 */ s16 unkC4;
    /* 0x00C6 */ u8 padC6[0x10AA];
    /* 0x1170 */ FJoint joint[18];
    /* 0x1EF0 */ u8 pad1EF0[0x6A20];
    /* 0x8910 */ Vu0Matrix local[18];
    /* 0x8D90 */ u8 pad8D90[0x23A2];
    /* 0xB132 */ s16 unkB132;
    /* 0xB134 */ u8 padB134[0x76C];
} FighterLook; /* size 0xB8A0 */

/* A working copy of one joint: parent world matrix, world, view, screen rows, local. */
typedef struct {
    /* 0x000 */ Vu0Matrix *parent;
    /* 0x004 */ s32 pad4[3];
    /* 0x010 */ Vu0Matrix m0;
    /* 0x050 */ Vu0Matrix m1;
    /* 0x090 */ Vu0Matrix m2;
    /* 0x0D0 */ Vu0Matrix local;
} LookJoint; /* size 0x110 */

extern s16 gMode;
extern Fighter gFighters[4];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern u16 D_005F5CB6;
extern V3 *D_005F5E08;
extern V3 *D_005F5E0C;
extern V3 *D_005F5E10;
extern s32 D_005F6FE8[2];
extern f32 D_005F6FF0[2];
extern f32 D_005F6FF8[2];
extern s32 D_005F7000[2];
extern f32 D_005F7008[2];
extern f32 D_005F7010[2];
extern s32 D_005F7018[2];
extern f32 D_005F7020;
extern f32 D_005F7024;
extern f32 D_005F7028;
extern f32 D_005F702C;
extern s8 D_005F7030[8];
extern s8 D_005F7038[2];
extern s16 D_005F703A;
extern s16 D_005F7040[2];
extern s32 D_00415808[];
extern Vu0Matrix D_004334C0;
extern FVec4 D_00455090;
extern f32 D_004550A0[];
extern Unk4550B0 D_004550B0;
extern s32 D_004550B8[];
extern f32 D_004550C0[];
extern s8 D_00455148[];
extern LightSet D_00455170[];
extern f32 D_00455CF0[];
extern u128 D_00455E30[];
extern const Vec4A8 D_00521FE0;
extern Lights D_01FC4210;
extern FVec4 D_01FC4550[2];
extern FVec4 D_01FC4570;

f32 sinf(f32 x);
f32 cosf(f32 x);
f32 sqrtf(f32 x);
f32 fabsf(f32 x);
f32 atan2f(f32 y, f32 x);
void *memset(void *p, s32 c, s32 n);
void *memcpy(void *d, const void *s, u32 n);
void sceVu0ScaleVector(f32 *out, f32 *in, f32 s);
void sceVu0ScaleVectorXYZ(f32 *out, f32 *in, f32 s);
void sceVu0AddVector(f32 *out, f32 *a, f32 *b);
void sceVu0SubVector(f32 *out, f32 *a, f32 *b);
f32 sceVu0InnerProduct(f32 *a, f32 *b);
void sceVu0ApplyMatrix(f32 *out, Vu0Matrix m, f32 *v);
void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0InversMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0TransposeMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0RotMatrixX(Vu0Matrix m0, Vu0Matrix m1, f32 rx);
void sceVu0RotMatrixY(Vu0Matrix m0, Vu0Matrix m1, f32 ry);
void sceVu0RotMatrixZ(Vu0Matrix m0, Vu0Matrix m1, f32 rz);
void sceGifPkInit(void *pk, void *base);
void sceGifPkRef(void *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceGifPkTerminate(void *pk);
u32 Dma_Wait(volatile u32 *chan);
void FlushCache(s32 op);
void func_0035E6B8(void *tag);
void func_00349D70(Vu0Matrix m, f32 *r);
void func_00349EE0(Vu0Matrix m, f32 *r);
void func_0034A868(f32 *out, f32 *in);
void Mtx_Unit(Vu0Matrix m);
void Mtx_Zero(Vu0Matrix m);
void func_00356760(f32 *dst, f32 *src);
void func_00356780(f32 *dst, f32 *src, f32 s);
s32 func_0036E160(void);
f32 func_00366EC8(void);
s32 func_003A9698(void);
s32 func_003BB4D8(void);
f32 func_00352F50(f32 x, f32 near, f32 far);
void func_00352ED8(f32 *out, f32 *a, f32 *b, f32 t);
void func_00352FA8(LightSet *out, LightSet *in, f32 s, f32 amb);
void func_00353070(LightSet *out, LightSet *in, f32 s, f32 amb);
void func_00353178(void);
void func_00353448(s32 i, s32 chr, f32 (*m)[4]);
u32 func_00354120(void *pk, GsImageRec *rec, u32 base);

/* Copies the xyz of a vector (12 bytes; both 16-byte aligned). */
static inline void CopyV3(FVec4 *d, FVec4 *s) {
    memcpy(d, s, 12);
}

/* Copies a whole 16-byte vector. */
static inline void CopyV4(f32 *d, f32 *s) {
    *(u128 *)d = *(u128 *)s;
}

/* NON_MATCHING: about 960 lines: typed draft with the original control flow (stack layout, registers, block order) */
/* Fighter look-at (func_003478F0, before the fighter draw): works on copies of four float joints (0, 1 = neck and
 * head, 16 and 17 = the eyes) and turns them toward `target` (or straight ahead, D_00521FE0, outside the move's
 * tracking window); yaw / pitch per fighter are clamped, eased in over D_005F6FE8[side] frames and kept in
 * D_005F6FF0-D_005F7018; the view (D_004334C0 * world) and screen (ws * world) matrices of each joint are rebuilt
 * and written back to the fighter only while tracking. [V] */
#if 0
void func_00351E98(FighterLook *f, f32 *target, Vu0Matrix ws) {
    LookJoint lj[4];
    Vu0Matrix inv;
    FVec4 eye;
    Vec4A8 fwd;
    Vu0Matrix base;
    FVec4 v;
    FVec4 ang;
    FVec4 save;
    FVec4 d;
    Vu0Matrix t;
    f32 off;
    f32 k;
    f32 yaw;
    f32 pitch;
    f32 prev;
    s32 track;
    s32 frozen;
    s32 side;
    s32 st;
    s32 i;

    track = 0;
    side = f->unk1E;
    lj[0].parent = f->joint[0].parent;
    sceVu0CopyMatrix(lj[0].m0, f->joint[0].m0);
    sceVu0CopyMatrix(lj[0].m2, (f32 (*)[4])f->joint[0].m2);
    sceVu0CopyMatrix(lj[0].m1, f->joint[0].m1);
    sceVu0CopyMatrix(lj[0].local, f->local[0]);
    lj[1].parent = &lj[0].m0;
    sceVu0CopyMatrix(lj[1].m0, f->joint[1].m0);
    sceVu0CopyMatrix(lj[1].m2, (f32 (*)[4])f->joint[1].m2);
    sceVu0CopyMatrix(lj[1].m1, f->joint[1].m1);
    sceVu0CopyMatrix(lj[1].local, f->local[1]);
    lj[2].parent = &lj[1].m0;
    sceVu0CopyMatrix(lj[2].m0, f->joint[16].m0);
    sceVu0CopyMatrix(lj[2].m2, (f32 (*)[4])f->joint[16].m2);
    sceVu0CopyMatrix(lj[2].m1, f->joint[16].m1);
    sceVu0CopyMatrix(lj[2].local, f->local[16]);
    lj[3].parent = &lj[1].m0;
    sceVu0CopyMatrix(lj[3].m0, f->joint[17].m0);
    sceVu0CopyMatrix(lj[3].m2, (f32 (*)[4])f->joint[17].m2);
    sceVu0CopyMatrix(lj[3].m1, f->joint[17].m1);
    sceVu0CopyMatrix(lj[3].local, f->local[17]);

    /* The first eye joint's position in the neck's space: its depth offsets the aim point. */
    sceVu0InversMatrix(inv, lj[0].m0);
    sceVu0ApplyMatrix(eye, inv, lj[2].m0[3]);
    off = eye[2];

    if (f->unkB132 != 0 && func_0036E160() != 0 && ((f->unkC0->unk24 >> 22) & 1)) {
        if (f->unkC0->unk2C == 0) {
            track = 1;
        } else if (f->unkC4 >= f->unkC0->unk2D) {
            track = f->unkC4 >= f->unkC0->unk18 - 0x11;
        }
    }
    frozen = 0;
    if (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
        frozen = 1;
    }
    if (track == 0) {
        fwd = D_00521FE0;
        sceVu0ApplyMatrix(target, lj[1].m0, fwd.v);
        D_005F6FE8[side] = 2;
    } else if (D_005F6FE8[side] >= 2) {
        D_005F6FE8[side]--;
    }

    /* Neck. */
    sceVu0CopyMatrix(base, lj[0].local);
    base[3][0] += off;
    sceVu0MulMatrix(base, *lj[0].parent, base);
    side = f->unk1E;
    if (track != 0) {
        k = (1.0f / D_005F6FE8[side]) * 0.125f;
    } else {
        k = 1.0f;
    }
    if (frozen == 0) {
        sceVu0SubVector(d, target, base[3]);
        t[3][3] = 1.0f;
        sceVu0TransposeMatrix(t, base);
        sceVu0ApplyMatrix(v, t, d);
        yaw = atan2f(v[1], v[0]);
        st = 4;
        if (!(yaw < -2.7925267f)) {
            st = 0;
            if (yaw > 2.7925267f) {
                st = 3;
            }
        }
        if ((st == 3 && D_005F7000[side] == 4) || (st == 4 && D_005F7000[side] == 3)) {
            yaw = D_005F6FF0[side];
            pitch = D_005F6FF8[side];
        } else {
            pitch = atan2f(-v[2], sqrtf(v[0] * v[0] + v[1] * v[1]));
        }
        if ((u32)(st - 3) < 2) {
            pitch = 0.0f;
        }
        if (st == 3 && D_005F7000[side] == st) {
            yaw = 0.6981317f;
        } else if (st == 4 && D_005F7000[side] == st) {
            yaw = -0.6981317f;
        }
        if (yaw < -0.6981317f) {
            yaw = -0.6981317f;
        } else if (yaw > 0.6981317f) {
            yaw = 0.6981317f;
        }
        prev = D_005F6FF0[side];
        yaw = prev + (yaw - prev) * k;
        if (track == 1 && (fabsf(yaw - prev) > 0.08726646f || fabsf(pitch - D_005F6FF8[side]) < 0.08726646f)) {
            pitch = D_005F6FF8[side];
        } else {
            if (pitch < -0.5672320f) {
                pitch = -0.5672320f;
            } else if (pitch > 0.2617994f) {
                pitch = 0.2617994f;
            }
            prev = D_005F6FF8[side];
            pitch = prev + (pitch - prev) * k;
        }
        ang[0] = 1.5707964f;
        ang[1] = 1.5707964f;
        ang[2] = 0.0f;
        ang[3] = 0.0f;
        func_00349EE0(lj[0].local, ang);
        sceVu0MulMatrix(lj[0].m0, *lj[0].parent, lj[0].local);
        CopyV3(&save, (FVec4 *)lj[0].local[3]);
        ang[0] = yaw;
        ang[1] = 0.0f;
        ang[2] = pitch;
        ang[3] = 0.0f;
        sceVu0RotMatrixZ(lj[0].local, lj[0].local, ang[2]);
        sceVu0RotMatrixY(lj[0].local, lj[0].local, ang[1]);
        sceVu0RotMatrixX(lj[0].local, lj[0].local, ang[0]);
        CopyV3((FVec4 *)lj[0].local[3], &save);
        sceVu0MulMatrix(lj[0].m0, *lj[0].parent, lj[0].local);
        D_005F6FF0[side] = yaw;
        D_005F6FF8[side] = pitch;
        if (st == 0) {
            D_005F7000[side] = 0;
        } else if (D_005F7000[side] == 0) {
            D_005F7000[side] = st;
        }
    } else {
        ang[0] = 1.5707964f;
        ang[1] = 1.5707964f;
        ang[2] = 0.0f;
        ang[3] = 0.0f;
        func_00349EE0(lj[0].local, ang);
        sceVu0MulMatrix(lj[0].m0, *lj[0].parent, lj[0].local);
        CopyV3(&save, (FVec4 *)lj[0].local[3]);
        ang[0] = D_005F6FF0[side];
        ang[1] = 0.0f;
        ang[2] = D_005F6FF8[side];
        ang[3] = 0.0f;
        sceVu0RotMatrixZ(lj[0].local, lj[0].local, ang[2]);
        sceVu0RotMatrixY(lj[0].local, lj[0].local, ang[1]);
        sceVu0RotMatrixX(lj[0].local, lj[0].local, ang[0]);
        CopyV3((FVec4 *)lj[0].local[3], &save);
        sceVu0MulMatrix(lj[0].m0, *lj[0].parent, lj[0].local);
    }
    sceVu0MulMatrix(lj[0].m1, D_004334C0, lj[0].m0);
    sceVu0MulMatrix(t, ws, lj[0].m0);
    for (i = 0; i < 3; i++) {
        CopyV4(lj[0].m2[i], t[i]);
    }

    /* Head. */
    sceVu0MulMatrix(lj[1].m0, *lj[1].parent, lj[1].local);
    sceVu0CopyMatrix(base, lj[0].local);
    base[3][0] += off;
    sceVu0MulMatrix(base, *lj[0].parent, base);
    side = f->unk1E;
    if (track != 0) {
        k = (1.0f / D_005F6FE8[side]) * 0.125f;
    } else {
        k = 1.0f;
    }
    if (frozen == 0) {
        sceVu0SubVector(d, target, base[3]);
        t[3][3] = 1.0f;
        sceVu0TransposeMatrix(t, base);
        sceVu0ApplyMatrix(v, t, d);
        yaw = atan2f(v[1], v[0]);
        st = 4;
        if (!(yaw < -2.7925267f)) {
            st = 0;
            if (yaw > 2.7925267f) {
                st = 3;
            }
        }
        if ((st == 3 && D_005F7018[side] == 4) || (st == 4 && D_005F7018[side] == 3)) {
            yaw = D_005F7008[side];
            pitch = D_005F7010[side];
        } else {
            pitch = atan2f(-v[2], sqrtf(v[0] * v[0] + v[1] * v[1]));
        }
        if ((u32)(st - 3) < 2) {
            pitch = 0.0f;
        }
        if (st == 3) {
            yaw = 0.6981317f;
        } else if (st == 4) {
            yaw = -0.6981317f;
        }
        if (yaw < -0.6981317f) {
            yaw = -0.6981317f;
        } else if (yaw > 0.6981317f) {
            yaw = 0.6981317f;
        }
        prev = D_005F7008[side];
        yaw = prev + (yaw - prev) * k;
        if (track == 1 && (fabsf(yaw - prev) > 0.08726646f || fabsf(pitch - D_005F7010[side]) < 0.08726646f)) {
            pitch = D_005F7010[side];
        } else {
            if (pitch < -0.5672320f) {
                pitch = -0.5672320f;
            } else if (pitch > 0.2617994f) {
                pitch = 0.2617994f;
            }
            prev = D_005F7010[side];
            pitch = prev + (pitch - prev) * k;
        }
        ang[0] = 0.0f;
        ang[1] = pitch;
        ang[2] = yaw;
        ang[3] = 0.0f;
        func_00349D70(lj[1].local, ang);
        sceVu0MulMatrix(lj[1].m0, *lj[1].parent, lj[1].local);
        D_005F7008[side] = yaw;
        D_005F7010[side] = pitch;
        if (st == 0) {
            D_005F7018[side] = 0;
        } else if (D_005F7018[side] == 0) {
            D_005F7018[side] = st;
        }
    } else {
        ang[1] = D_005F7010[side];
        ang[2] = D_005F7008[side];
        ang[0] = 0.0f;
        ang[3] = 0.0f;
        func_00349D70(lj[1].local, ang);
        sceVu0MulMatrix(lj[1].m0, *lj[1].parent, lj[1].local);
    }
    sceVu0MulMatrix(lj[1].m1, D_004334C0, lj[1].m0);
    sceVu0MulMatrix(t, ws, lj[1].m0);
    for (i = 0; i < 3; i++) {
        CopyV4(lj[1].m2[i], t[i]);
    }

    /* Eyes. */
    sceVu0MulMatrix(lj[2].m0, *lj[2].parent, lj[2].local);
    sceVu0MulMatrix(lj[3].m0, *lj[3].parent, lj[3].local);
    sceVu0SubVector(d, target, lj[2].m0[3]);
    t[3][3] = 1.0f;
    sceVu0TransposeMatrix(t, lj[2].m0);
    sceVu0ApplyMatrix(v, t, d);
    yaw = -atan2f(v[1], v[0]);
    pitch = atan2f(-v[2], sqrtf(v[0] * v[0] + v[1] * v[1]));
    if (track != 0) {
        if (fabsf(pitch) < 0.08726646f && v[0] < 1000.0f) {
            v[0] = 1000.0f;
        }
        if (pitch < -0.61086524f) {
            pitch = -0.61086524f;
        } else if (pitch > 0.61086524f) {
            pitch = 0.61086524f;
        }
        if (yaw < -0.12217305f) {
            yaw = -0.12217305f;
        } else if (yaw > 0.15707964f) {
            yaw = 0.15707964f;
        }
        ang[0] = 1.5707964f;
        ang[1] = 0.0f;
        ang[2] = 0.0f;
        ang[3] = 0.0f;
        func_00349D70(lj[2].local, ang);
        sceVu0MulMatrix(lj[2].m0, *lj[2].parent, lj[2].local);
        CopyV3(&save, (FVec4 *)lj[2].local[3]);
        ang[0] = 0.0f;
        ang[1] = yaw;
        ang[2] = pitch;
        ang[3] = 0.0f;
        sceVu0RotMatrixX(lj[2].local, lj[2].local, ang[0]);
        sceVu0RotMatrixY(lj[2].local, lj[2].local, ang[1]);
        sceVu0RotMatrixZ(lj[2].local, lj[2].local, ang[2]);
        CopyV3((FVec4 *)lj[2].local[3], &save);
        sceVu0MulMatrix(lj[2].m0, *lj[2].parent, lj[2].local);
    }
    sceVu0SubVector(d, target, lj[3].m0[3]);
    t[3][3] = 1.0f;
    sceVu0TransposeMatrix(t, lj[3].m0);
    sceVu0ApplyMatrix(v, t, d);
    yaw = -atan2f(v[1], v[0]);
    pitch = atan2f(-v[2], sqrtf(v[0] * v[0] + v[1] * v[1]));
    if (track != 0) {
        if (fabsf(pitch) < 0.08726646f && v[0] < 1000.0f) {
            v[0] = 1000.0f;
        }
        if (pitch < -0.61086524f) {
            pitch = -0.61086524f;
        } else if (pitch > 0.61086524f) {
            pitch = 0.61086524f;
        }
        if (yaw < -0.12217305f) {
            yaw = -0.12217305f;
        } else if (yaw > 0.15707964f) {
            yaw = 0.15707964f;
        }
        ang[0] = 1.5707964f;
        ang[1] = 0.0f;
        ang[2] = 0.0f;
        ang[3] = 0.0f;
        func_00349D70(lj[3].local, ang);
        sceVu0MulMatrix(lj[3].m0, *lj[3].parent, lj[3].local);
        CopyV3(&save, (FVec4 *)lj[3].local[3]);
        ang[0] = 0.0f;
        ang[1] = yaw;
        ang[2] = pitch;
        ang[3] = 0.0f;
        sceVu0RotMatrixX(lj[3].local, lj[3].local, ang[0]);
        sceVu0RotMatrixY(lj[3].local, lj[3].local, ang[1]);
        sceVu0RotMatrixZ(lj[3].local, lj[3].local, ang[2]);
        CopyV3((FVec4 *)lj[3].local[3], &save);
        sceVu0MulMatrix(lj[3].m0, *lj[3].parent, lj[3].local);
    }
    sceVu0MulMatrix(lj[2].m1, D_004334C0, lj[2].m0);
    sceVu0MulMatrix(t, ws, lj[2].m0);
    for (i = 0; i < 3; i++) {
        CopyV4(lj[2].m2[i], t[i]);
    }
    sceVu0MulMatrix(lj[3].m1, D_004334C0, lj[3].m0);
    sceVu0MulMatrix(t, ws, lj[3].m0);
    for (i = 0; i < 3; i++) {
        CopyV4(lj[3].m2[i], t[i]);
    }

    if (track != 0) {
        sceVu0CopyMatrix(f->joint[0].m0, lj[0].m0);
        sceVu0CopyMatrix((f32 (*)[4])f->joint[0].m2, lj[0].m2);
        sceVu0CopyMatrix(f->joint[0].m1, lj[0].m1);
        sceVu0CopyMatrix(f->local[0], lj[0].local);
        sceVu0CopyMatrix(*f->joint[1].parent, *lj[1].parent);
        sceVu0CopyMatrix(f->joint[1].m0, lj[1].m0);
        sceVu0CopyMatrix((f32 (*)[4])f->joint[1].m2, lj[1].m2);
        sceVu0CopyMatrix(f->joint[1].m1, lj[1].m1);
        sceVu0CopyMatrix(f->local[1], lj[1].local);
        sceVu0CopyMatrix(*f->joint[16].parent, *lj[2].parent);
        sceVu0CopyMatrix(f->joint[16].m0, lj[2].m0);
        sceVu0CopyMatrix((f32 (*)[4])f->joint[16].m2, lj[2].m2);
        sceVu0CopyMatrix(f->joint[16].m1, lj[2].m1);
        sceVu0CopyMatrix(f->local[16], lj[2].local);
        sceVu0CopyMatrix(*f->joint[17].parent, *lj[3].parent);
        sceVu0CopyMatrix(f->joint[17].m0, lj[3].m0);
        sceVu0CopyMatrix((f32 (*)[4])f->joint[17].m2, lj[3].m2);
        sceVu0CopyMatrix(f->joint[17].m1, lj[3].m1);
        sceVu0CopyMatrix(f->local[17], lj[3].local);
    }
}
#else
LIT4_WORD(D_005F5668, 0xC032B8C2);
LIT4_WORD(D_005F566C, 0x4032B8C2);
LIT4_WORD(D_005F5670, 0x3F32B8C2);
LIT4_WORD(D_005F5674, 0xBF32B8C2);
LIT4_WORD(D_005F5678, 0xBF32B8C2);
LIT4_WORD(D_005F567C, 0x3F32B8C2);
LIT4_WORD(D_005F5680, 0x3DB2B8C2);
LIT4_WORD(D_005F5684, 0xBF11361E);
LIT4_WORD(D_005F5688, 0x3E860A92);
LIT4_WORD(D_005F568C, 0x3FC90FDB);
LIT4_WORD(D_005F5690, 0x3FC90FDB);
LIT4_WORD(D_005F5694, 0xC032B8C2);
LIT4_WORD(D_005F5698, 0x4032B8C2);
LIT4_WORD(D_005F569C, 0x3F32B8C2);
LIT4_WORD(D_005F56A0, 0xBF32B8C2);
LIT4_WORD(D_005F56A4, 0xBF32B8C2);
LIT4_WORD(D_005F56A8, 0x3F32B8C2);
LIT4_WORD(D_005F56AC, 0x3DB2B8C2);
LIT4_WORD(D_005F56B0, 0xBF11361E);
LIT4_WORD(D_005F56B4, 0x3E860A92);
LIT4_WORD(D_005F56B8, 0x3DB2B8C2);
LIT4_WORD(D_005F56BC, 0xBF1C61AA);
LIT4_WORD(D_005F56C0, 0x3F1C61AA);
LIT4_WORD(D_005F56C4, 0xBDFA35DD);
LIT4_WORD(D_005F56C8, 0x3E20D97C);
LIT4_WORD(D_005F56CC, 0x3FC90FDB);
LIT4_WORD(D_005F56D0, 0x3DB2B8C2);
LIT4_WORD(D_005F56D4, 0xBF1C61AA);
LIT4_WORD(D_005F56D8, 0x3F1C61AA);
LIT4_WORD(D_005F56DC, 0xBDFA35DD);
LIT4_WORD(D_005F56E0, 0x3E20D97C);
LIT4_WORD(D_005F56E4, 0x3FC90FDB);
INCLUDE_ASM("asm/nonmatchings/scene3d_d", func_00351E98);
#endif

/* Blend: out = a * t + b * (1 - t), w = 0. [V] */
void func_00352ED8(f32 *out, f32 *a, f32 *b, f32 t) {
    FVec4 va;
    FVec4 vb;

    sceVu0ScaleVector(va, a, t);
    sceVu0ScaleVector(vb, b, 1.0f - t);
    sceVu0AddVector(out, va, vb);
    out[3] = 0;
}

/* Distance fade: 1 below `near`, 0 beyond `far`, linear between. [V] */
f32 func_00352F50(f32 x, f32 near, f32 far) {
    if (x < near) {
        return 1.0f;
    }
    if (x - far < 0.0f) {
        return (far - x) / (far - near);
    }
    return 0.0f;
}

/* Builds the VU0 light matrices from a light set: the three directions into `out` (transposed, the normal light
 * matrix), the colours scaled by `s`, the ambient scaled by `amb` (the colour light matrix). [V] */
void func_00352FA8(LightSet *out, LightSet *in, f32 s, f32 amb) {
    s32 i;

    Mtx_Zero((f32 (*)[4])out);
    Mtx_Zero(out->col);
    for (i = 0; i < 3; i++) {
        *(u128 *)((f32 (*)[4])out)[i] = *(u128 *)in->dir[i];
        sceVu0ScaleVector(out->col[i], in->col[i], s);
    }
    sceVu0TransposeMatrix((f32 (*)[4])out, (f32 (*)[4])out);
    sceVu0ScaleVector(out->amb, in->amb, amb);
}

/* Same with the directions mirrored in x and z and every colour set to D_004550A0 (scaled by `s`): the mirrored
 * lighting (D_005F7038). [V] */
void func_00353070(LightSet *out, LightSet *in, f32 s, f32 amb) {
    s32 i;
    f32 *x = (f32 *)out;
    f32 *y = (f32 *)out + 1;
    f32 *z = (f32 *)out + 2;
    s32 o;

    Mtx_Zero((f32 (*)[4])out);
    Mtx_Zero(out->col);
    for (i = 0; i < 3; i++) {
        o = i * 16;
        *(f32 *)((u8 *)x + o) = -in->dir[i][0];
        *(f32 *)((u8 *)y + o) = in->dir[i][1];
        *(f32 *)((u8 *)z + o) = -in->dir[i][2];
        sceVu0ScaleVector(out->col[i], D_004550A0, s);
    }
    sceVu0TransposeMatrix((f32 (*)[4])out, (f32 (*)[4])out);
    sceVu0ScaleVector(out->amb, in->amb, amb);
}

/* Resets the light state: the dim flags D_005F7040 and the flash timer. The quadword clears of the slots' unk100 /
 * flash and of the flash position are stored over the stack copies of their addresses, not the vectors (a bug of
 * the original, reproduced as compiled). [V] */
void func_00353178(void) {
    s32 i;
    u128 *p[3];
    LightSlot *s;

    for (i = 0; i < 2; i++) {
        s = &D_01FC4210.slot[i];
        D_005F7040[i] = 0;
        p[0] = &s->unk100;
        *(u128 *)&p[0] = 0;
        p[1] = &s->flash;
        *(u128 *)&p[1] = 0;
    }
    D_005F703A = 0;
    p[2] = (u128 *)&D_01FC4570;
    *(u128 *)&p[2] = 0;
}

/* Starts a light flash at `pos` with colour `rgb` (0-255 per channel) for 54 frames (effects). [V] */
void Light_StartFlash(f32 *pos, u8 *rgb) {
    s32 i;
    FVec4 c;

    CopyV4(D_01FC4570, pos);
    c[0] = rgb[0];
    c[1] = rgb[1];
    c[2] = rgb[2];
#ifdef PORT
    c[3] = 0; /* (the original copies a stale stack word: host-dependent here, docs/port/determinism.md) */
#endif
    for (i = 0; i < 2; i++) {
        CopyV4((f32 *)&D_01FC4210.slot[i].flash, c);
    }
    D_005F703A = 0x36;
}

/* Stops the flash. [V] */
void func_00353278(void) {
    D_005F703A = 0;
}

/* Sets the dim flag of fighter `i` (out of range: 0). [V] */
void func_00353280(u32 i) {
    if (i >= 2) {
        i = 0;
    }
    D_005F7040[i] = 1;
}

/* Counts the flash timer down while the game runs (Battle_UpdateDistance). [V] */
void func_003532A0(void) {
    if (D_005F703A > 0 && D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        D_005F703A--;
    }
}

/* Blends the flash into fighter `i`'s third light: direction from the flash to `pos`, flash colour, weight
 * timer / 54 times the distance fade (1000-5000). [V] */
void func_003532F0(s32 i, f32 *pos) {
    FVec4 d;
    f32 t;
    f32 r;

    if (D_005F703A > 0) {
        t = (f32)D_005F703A * (1.0f / 54.0f);
        sceVu0SubVector(d, D_01FC4570, pos);
        r = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        do {
        } while (0); /* fake: keeps the constants after the sqrt */
        t *= func_00352F50(r, 1000.0f, 5000.0f);
        func_0034A868(d, d);
        func_00352ED8(D_01FC4210.slot[i].cur.dir[2], d, D_01FC4210.slot[i].cur.dir[2], t);
        func_00352ED8(D_01FC4210.slot[i].cur.col[2], (f32 *)&D_01FC4210.slot[i].flash, D_01FC4210.slot[i].cur.col[2], t);
    }
}

void func_003533E8(s32 n) {
    D_004550B8[0] = n;
}

/* Selects the entry of D_00455148 for id `id` (< 37) into D_004550B8. [V] */
void func_003533F8(s32 id) {
    if (id < 0x25) {
        func_003533E8(D_00455148[id]);
    }
}

/* out = grey (D_004550C0[i], same in r, g, b): the fighter light colour of character `i`. [V/I] */
void func_00353420(f32 *out, s32 i) {
    f32 v = D_004550C0[i];

    out[2] = v;
    out[1] = v;
    out[0] = v;
}

/* NON_MATCHING: 1 line: our compiler 8-aligns the label after the `b` (an extra nop), the original does not */
/* Fighter `i`'s third light: a direction turned from the fighter's matrix `m` toward the stage light position
 * (D_00415808[5..7]), faded by the distance (D_004550B0) and by the angle to the first light; colour by character. [V] */
#if 0
void func_00353448(s32 i, s32 chr, f32 (*m)[4]) {
    FVec4 v;
    Vu0Matrix rot;
    FVec4 c;
    FVec4 lp;
    LightSlot *s;
    Unk4550B0 *p = &D_004550B0;
    f32 t;
    f32 d;
    f32 k;
    f32 r;

    memset(v, 0, 0x10);
    v[0] = 1.0f;
    s = &D_01FC4210.slot[i];
    if (p->on != 0) {
        Mtx_Unit(rot);
        sceVu0RotMatrixY(rot, rot, -0.03f);
        sceVu0ApplyMatrix(v, rot, v);
        sceVu0ApplyMatrix(v, m, v);
        func_0034A868(v, v);
        lp[3] = 1.0f;
        lp[0] = D_00415808[5];
        lp[1] = -D_00415808[6];
        lp[2] = -D_00415808[7];
        *(V4 *)c = *(V4 *)lp;
        {
            f32 *q = m[3];
            t = sqrtf((c[0] - q[0]) * (c[0] - q[0]) + (c[1] - q[1]) * (c[1] - q[1]) + (c[2] - q[2]) * (c[2] - q[2]));
        }
        do {
        } while (0); /* fake: keeps the fade distances after the sqrt */
        t = func_00352F50(t, p->near, p->far);
        d = sceVu0InnerProduct(v, s->cur.dir[0]);
        if (d < 0.7853982f) {
            d = 1.0f;
        } else {
            k = 0.21460181f;
            d = 1.0f - (d - 0.7853982f) / k;
        }
        r = d * t;
    } else {
        r = 0.0f;
    }
    func_00352ED8(s->cur.dir[2], v, s->base.dir[2], r);
    func_00353420(c, chr);
    func_00352ED8(s->cur.col[2], c, s->base.col[2], r);
}
#else
LIT4_WORD(D_005F56F0, 0xBCF5C28F);
LIT4_WORD(D_005F56F4, 0x3F490FDB);
LIT4_WORD(D_005F56F8, 0x3E5BC094);
INCLUDE_ASM("asm/nonmatchings/scene3d_d", func_00353448);
#endif

/* Resets the light scale and the light state. [V] */
void func_00353658(void) {
    D_005F7028 = 1.0f;
    func_00353178();
}

void func_00353668(s32 i) {
    D_005F7030[i] = 0;
}

void func_00353678(s32 i) {
    D_005F7038[i] = 0;
}

/* Lights 0 and 1 of a fighter from its base set: mirrored (x, z negated, colour D_00455090) once when
 * D_005F7038[i] is set, else copied. [V] */
void func_00353688(LightSet *out, LightSet *in, s32 i) {
    s32 j;

    if (D_005F7038[i] != 0) {
        for (j = 0; j < 2; j++) {
            out->dir[j][0] = -in->dir[j][0];
            out->dir[j][2] = -in->dir[j][2];
            CopyV3(&out->col[j], &D_00455090);
        }
        D_005F7038[i] = 0;
    } else {
        for (j = 0; j < 2; j++) {
            CopyV3(&out->dir[j], &in->dir[j]);
            CopyV3(&out->col[j], &in->col[j]);
        }
    }
}

/* Same from separate direction / colour arrays (not in mode 5). [V] */
void func_00353758(LightSet *out, V3 *dir, V3 *col, s32 i) {
    s32 j;

    if (gMode == 5) {
        return;
    }
    if (D_005F7038[i] != 0) {
        for (j = 0; j < 2; j++) {
            out->dir[j][0] = -dir[j].x;
            out->dir[j][2] = -dir[j].z;
            CopyV3(&out->col[j], &D_00455090);
        }
        D_005F7038[i] = 0;
    } else {
        for (j = 0; j < 2; j++) {
            func_00356760(out->dir[j], (f32 *)&dir[j]);
            func_00356760(out->col[j], (f32 *)&col[j]);
        }
    }
}

/* Per-fighter light update (Skel_Update): in mode 5 or when func_003BB4D8() is set the lights come from the tables
 * at D_005F5E08 / D_005F5E0C / D_005F5E10 scaled by D_005F7028; otherwise the third light follows the fighter
 * (func_00353448), the ambient takes the override D_01FC4550[i] once (D_005F7030[i]) or the base one, lights 0-1
 * from the base set; then the flash. [V] */
void Light_UpdateFighter(s32 i, f32 *pos) {
    LightSlot *s = &D_01FC4210.slot[i];
    s32 j;

    if (func_003BB4D8() != 0 || gMode == 5) {
        if (gMode == 5 || (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0)) {
            for (j = 0; j < 3; j++) {
                func_00356760(s->cur.dir[j], (f32 *)&D_005F5E08[j]);
                func_00356780(s->cur.col[j], (f32 *)&D_005F5E0C[j], D_005F7028);
            }
            func_00356780(s->cur.amb, (f32 *)D_005F5E10, D_005F7028);
            func_00353758(&s->cur, D_005F5E08, D_005F5E0C, i);
        }
    } else {
        func_00353448(i, gFighters[i].unk2A, (f32 (*)[4])((u8 *)&gFighters[i] + 0x1230));
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            if (D_005F7030[i] != 0) {
                D_005F7030[i] = 0;
                CopyV3(&s->cur.amb, &D_01FC4550[i]);
            } else {
                CopyV3(&s->cur.amb, &s->base.amb);
            }
            func_00353688(&s->cur, &s->base, i);
        }
    }
    func_003532F0(i, pos);
}

/* VU0 light matrices of fighter `i` for the model draw (render_fighter / dma_gif): directions turned about y by
 * func_00366EC8() in mode 4, transposed into `out`; colours and ambient scaled by D_005F7028; dimmed to 0.4 with
 * the ambient cleared once when D_005F7040[i] is set. [V] */
void Light_GetFighterMatrices(s32 i, LightSet *out) {
    LightSlot *s = &D_01FC4210.slot[i];
    FVec4 *d = s->cur.dir;
    f32 *x;
    f32 *y;
    f32 *z;
    f32 a;
    f32 sn;
    f32 cs;
    f32 k;
    s32 j;
    s32 o;

    Mtx_Zero((f32 (*)[4])out);
    Mtx_Zero(out->col);
    if (gMode == 4) {
        a = func_00366EC8();
    } else {
        a = 0.0f;
    }
    j = 0;
    sn = sinf(a);
    cs = cosf(a);
    x = ((f32 (*)[4])out)[0];
    y = ((f32 (*)[4])out)[1];
    z = ((f32 (*)[4])out)[2];
    for (; j < 3; j++) {
        sceVu0ScaleVector(out->col[j], s->cur.col[j], D_005F7028);
        o = j * 4;
        *(f32 *)((u8 *)x + o) = d[j][0] * cs + d[j][2] * sn;
        *(f32 *)((u8 *)y + o) = d[j][1];
        *(f32 *)((u8 *)z + o) = -d[j][0] * sn + d[j][2] * cs;
    }
    sceVu0ScaleVector(out->amb, s->cur.amb, D_005F7028);
    if (D_005F7040[i] != 0) {
        k = 0.4f;
        *(u64 *)&s->cur.amb[0] = 0;
        *(s32 *)&s->cur.amb[3] = 0;
        sceVu0ScaleVector(out->col[0], out->col[0], k);
        sceVu0ScaleVector(out->col[1], out->col[1], k);
        sceVu0ScaleVector(out->col[2], out->col[2], k);
        D_005F7040[i] = 0;
    }
}

/* Sets the ambient override of fighter `i`. [V] */
void func_00353C40(s32 i, FVec4 *v) {
    CopyV3(&D_01FC4550[i], v);
}

void func_00353C68(s32 i, s32 on) {
    D_005F7030[i] = on;
}

void func_00353C78(s32 i, s32 on) {
    D_005F7038[i] = on;
}

/* Fighter `i`'s first light direction turned like Light_GetFighterMatrices, w = 1 (render_fighter, model_anim). [V] */
void func_00353C88(s32 i, f32 *out) {
    f32 a;
    f32 sn;
    f32 cs;
    f32 *v;

    if (gMode == 4) {
        a = func_00366EC8();
    } else {
        a = 0.0f;
    }
    sn = sinf(a);
    cs = cosf(a);
    v = D_01FC4210.slot[i].cur.dir[0];
    out[0] = v[0] * cs + v[2] * sn;
    out[1] = v[1];
    out[2] = -v[0] * sn + v[2] * cs;
    out[3] = 1.0f;
}

void func_00353D58(void) {
    D_005F7020 = 1.0f;
}

/* Light matrices of the global set (scale D_005F7020, colours / 128). [V] */
void func_00353D68(LightSet *out) {
    if (D_005F7038[0] != 0) {
        func_00353070(out, &D_01FC4210.global, D_005F7020 * (1.0f / 128.0f), D_005F7020);
        return;
    }
    func_00352FA8(out, &D_01FC4210.global, D_005F7020 * (1.0f / 128.0f), D_005F7020);
}

void func_00353DB0(void) {
    D_005F7024 = 1.0f;
}

/* Same with the scale D_005F7024. [V] */
void func_00353DC0(LightSet *out) {
    if (D_005F7038[0] != 0) {
        func_00353070(out, &D_01FC4210.global, D_005F7024 * (1.0f / 128.0f), D_005F7024);
        return;
    }
    func_00352FA8(out, &D_01FC4210.global, D_005F7024 * (1.0f / 128.0f), D_005F7024);
}

void func_00353E08(void) {
    D_005F702C = 1.0f;
}

/* Light matrices of the extra set (scale D_005F702C). [V] */
void func_00353E18(LightSet *out) {
    if (D_005F7038[0] != 0) {
        func_00353070(out, &D_01FC4210.extra, D_005F702C * (1.0f / 128.0f), D_005F702C);
        return;
    }
    func_00352FA8(out, &D_01FC4210.extra, D_005F702C * (1.0f / 128.0f), D_005F702C);
}

/* Stage light set-up (func_00356A78): the global set from `src` (ambient halved), the fighter slots and their base
 * copies from the stage's table D_00455170[stage], the extra set likewise with its colours scaled by
 * D_00455CF0[n]. [V] */
void Light_SetupStage(s32 n, LightDef *src) {
    s32 i;
    s32 j;
    s32 o;
    LightDefPair *l = src->l;

    D_01FC4210.global.count = src->count;
    func_00356780(D_01FC4210.global.amb, src->amb, 0.5f);
    D_01FC4210.global.amb[3] = 1.0f;
    for (i = 0; i < 3; i++) {
        o = i * 0x18;
        func_00356760(D_01FC4210.global.dir[i], (f32 *)((u8 *)l + o));
        o += 0xC;
        func_00356760(D_01FC4210.global.col[i], (f32 *)((u8 *)l + o));
        D_01FC4210.global.dir[i][3] = 0.0f;
        D_01FC4210.global.col[i][3] = 0.0f;
    }
    {
        LightSet *p = &D_01FC4210.slot[0].cur;
        *p = D_00455170[D_005F5CB6];
    }
    for (i = 0; i < 2; i++) {
        D_01FC4210.slot[i].cur = D_01FC4210.slot[0].cur;
        for (j = 0; j < 3; j++) {
            CopyV4(D_01FC4210.slot[i].base.dir[j], D_01FC4210.slot[i].cur.dir[j]);
            CopyV4(D_01FC4210.slot[i].base.col[j], D_01FC4210.slot[i].cur.col[j]);
        }
        CopyV4(D_01FC4210.slot[i].base.amb, D_01FC4210.slot[i].cur.amb);
    }
    D_01FC4210.extra = D_01FC4210.slot[0].cur;
    for (i = 0; i < D_01FC4210.extra.count; i++) {
        sceVu0ScaleVectorXYZ(D_01FC4210.extra.col[i], D_01FC4210.extra.col[i], D_00455CF0[n]);
    }
}

/* fake (decomp-permuter): an inline helper keeps m + 6 apart from the qword count */
static inline u32 Gs_RefEnd(u32 m, s32 hdr, u32 c) {
    return m + hdr + (c & 0x7FFF);
}

/* Adds a chain of GS image records to the packet `pk`: each record's BITBLTBUF base pointer is moved to `base` +
 * its offset from the first one, and the records are sent by reference (VIF DIRECTHL, at most 0xFFFF quadwords per
 * reference). Returns the size of the uploaded area in 256-byte blocks. [V] */
u32 func_00354120(void *pk, GsImageRec *rec, u32 base) {
    s32 last;
    s64 delta;
    u32 n;
    u32 m;
    GsImageRec *start;
    GsImageRec *next;
    s64 dbp;
    s64 sh;
    u32 c;
    s32 q;
    u64 w;
    u32 u;

    start = rec;
    delta = (u64)base - ((rec->bitbltbuf >> 32) & 0x3FFF);
    n = 0;
    next = rec;
    do {
        rec = next;
        w = rec->bitbltbuf;
        c = rec->qwc;
        m = n;
        dbp = ((w & 0x3FFF00000000) >> 32) + delta;
        n = Gs_RefEnd(m, 6, c);
        q = (c & 0x7FFF) << 4;
        next = (GsImageRec *)((u8 *)rec + (q + 0x60));
        rec->bitbltbuf = (w & ~0x3FFF00000000) | ((dbp << 32) & 0x3FFF00000000);
        last = (c & 0x8000) >> 15;
        if (n > 0xFFFF) {
            sceGifPkRef(pk, start, m, 0x10000000, m | 0x51000000, 0);
            n -= m;
            start = rec;
        }
    } while (last == 0);
    sceGifPkRef(pk, start, n, 0x10000000, n | 0x51000000, 0);
    sh = dbp << 8;
    u = sh + q;
    if ((u & 0xFF) == 0) {
        return u >> 8;
    }
    u += 0x100;
    return u >> 8;
}

/* The records, then a GS TEXFLUSH packet (D_00455E30). [V] */
u32 func_00354290(void *pk, GsImageRec *rec, u32 base) {
    u32 r = func_00354120(pk, rec, base);

    sceGifPkRef(pk, D_00455E30, 2, 0x10000000, 0x50000002, 0);
    return r;
}

/* Uploads a chain of GS image records now: builds the packet in `buf`, waits for the GIF channel, flushes the cache
 * and starts the GIF DMA chain. Returns the size in 256-byte blocks. [V] */
u32 func_003542E8(void *buf, GsImageRec *rec, u32 base) {
    u128 pk;
    u32 r;

    sceGifPkInit(&pk, buf);
    r = func_00354290(&pk, rec, base);
    sceGifPkTerminate(&pk);
    Dma_Wait((volatile u32 *)0x1000A000);
    FlushCache(0);
    func_0035E6B8(buf);
    return r;
}

/* Size of a chain of GS image records in 256-byte blocks (0 for none). [V] */
u32 func_00354368(GsImageRec *rec) {
    GsImageRec *p;
    s32 base;
    s32 dbp;
    s32 q;
    s32 c;
    s32 v;

    if (rec == NULL) {
        return 0;
    }
    p = rec;
    base = -(s32)((p->bitbltbuf >> 32) & 0x3FFF);
    do {
        c = p->qwc;
        dbp = (p->bitbltbuf >> 32) & 0x3FFF;
        q = (c & 0x7FFF) << 4;
        p = (GsImageRec *)((u8 *)p + (q + 0x60));
    } while (!(c & 0x8000));
    v = ((dbp + base) << 8) + q;
    if ((v & 0xFF) == 0) {
        return (u32)v >> 8;
    }
    v += 0x100;
    return (u32)v >> 8;
}
