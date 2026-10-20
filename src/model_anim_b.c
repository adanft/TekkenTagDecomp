/* 0x3D3630-0x3DB758, the second object of module model_anim (docs/game_overview.md section 4; docs/systems/render.md
 * section 11, "Models"): the stage wind (Lorenz-attractor generators, gusts and their replay ring), the animated stage
 * scenes built on the model hierarchy of src/model_anim.c (flocks, lanes, orbiting and curve-driven parts, debris,
 * sound panning), the GS image packets of model files and the stage objects (two lists of up to 36 animated, skinned
 * objects drawn through VU1 with planar shadows). Its .rodata (0x521B80-0x521CE0) and .lit4 (0x5F539C-0x5F5614) are
 * subsegments, included in place (force_not_migration in data.txt); LIT4_WORD emits the pool words of functions still
 * in assembly. Built without -ffast-math (configure.py NO_FAST_MATH; four functions match only that way); its bare
 * sqrt.s comes from an inline-asm square root. StageObjSetup / StageObjSetupList (set-up code) and StageObj / StageObjList
 * (update and draw code) are two views of the same 0x90-byte objects and 0x1680-byte lists. Every NON_MATCHING draft
 * (#if 0) compiles when enabled. */
#include "common.h"
#include "fighter.h"

typedef f32 FVec4[4] __attribute__((aligned(16)));

typedef f32 Vu0Matrix[4][4];

/* GS image packets and stage object set-up (0x3D8158-0x3D9588). */
/* An animation bank's part records (0x20 bytes) [I]. */
typedef struct {
    /* 0x00 */ u8 pad0[0x10];
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u8 pad14[0x8];
    /* 0x1C */ s32 unk1C;
} Unk3D8498Rec;

/* State returned by func_003B87C8 [I]. */
typedef struct {
    /* 0x00 */ s32 unk0;              /* set up */
    /* 0x04 */ s32 unk4;              /* count */
    /* 0x08 */ void *unk8;            /* source data */
    /* 0x0C */ Unk3D8498Rec *unkC[0x15]; /* 0x12 + unk4 records of unk8 (overlapping unk60 past 3) */
    /* 0x60 */ s32 unk60[1];          /* per record: matching index in func_003B87B0's table */
} Unk3B87C8;

/* Table returned by func_003B87B0 [I]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ Unk3D8498Rec *unkC[1];
} Unk3B87B0;

typedef struct {
    /* 0x00 */ u8 pad0[0x8];
    /* 0x08 */ s32 unk8;
} Unk3D8498Src;

typedef struct {
    /* 0x00 */ u8 pad0[0x22];
    /* 0x22 */ s16 unk22;
} Unk3D8498Arg;

/* A model file with its GS image records (func_003D8158) [I]. */
typedef struct {
    /* 0x00 */ u8 pad0[0xC];
    /* 0x0C */ s32 unkC;          /* offset of the records (16-aligned) */
    /* 0x10 */ s32 unk10;         /* pair count */
    /* 0x14 */ s32 unk14[22];     /* per block: size / 16 and a skip [I] */
    /* 0x6C */ s32 unk6C;         /* record count - 1 */
} Unk3D8158;

/* A 64-byte block copied with lq / sq. */
typedef struct {
    /* 0x00 */ u128 q[4];
} Q4;

/* A libpkt packet header (16 bytes). */
typedef struct {
    /* 0x0 */ u32 w[4];
} GifPk;

/* Small list of byte pairs: up to three entries, count - 1 at 6. */
typedef struct {
    /* 0x0 */ s8 unk0[3];
    /* 0x3 */ s8 unk3[3];
    /* 0x6 */ s8 unk6;
} Unk3D8C98;

/* A stage object's definition (0x20 bytes) [I]. */
typedef struct {
    /* 0x00 */ s8 unk0;   /* range of unk78 / 60 */
    /* 0x01 */ s8 unk1;
    /* 0x02 */ u16 unk2;  /* range of unk7C */
    /* 0x04 */ u16 unk4;
    /* 0x06 */ u16 unk6;  /* flags */
    /* 0x08 */ f32 unk8;  /* position */
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14; /* facing, degrees */
    /* 0x18 */ u8 unk18;
    /* 0x19 */ u8 unk19;  /* model index */
    /* 0x1A */ s8 unk1A;
    /* 0x1B */ s8 unk1B;  /* repeat count without a variant list */
    /* 0x1C */ struct StageObjVar *unk1C;
} StageObjDef;

/* A random variant list of a stage object: n bytes at unk0 [I]. */
typedef struct StageObjVar {
    /* 0x0 */ u8 *unk0;
    /* 0x4 */ s8 unk4;
    /* 0x5 */ u8 unk5;  /* added to the model index */
    /* 0x6 */ s8 unk6;  /* repeat count (< 0: special) */
    /* 0x7 */ s8 unk7;  /* steps back (func_003D8E30) */
    /* 0x8 */ s16 unk8;  /* fixed unk78 when >= 0 */
    /* 0xA */ s16 unkA;  /* fixed unk7C when >= 0 */
    /* 0xC */ s16 unkC;  /* fixed unk7A when > 0 */
    /* 0xE */ u8 padE[0x2];
} StageObjVar; /* 0x10 bytes, arrays end with unk0 == NULL */

/* A model of the stage (D_01FC2380 entries) [I]. */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ f32 unk4;
} StageModel;

/* A stage object (0x90 bytes, lists of 0x25 at D_01FBF680 / D_01FC0D00) [I]. */
typedef struct {
    /* 0x00 */ void *unk0;
    /* 0x04 */ u8 pad4[0x4C];
    /* 0x50 */ StageObjVar *unk50;
    /* 0x54 */ StageObjDef *unk54;
    /* 0x58 */ u8 pad58[0x4];
    /* 0x5C */ f32 unk5C; /* position */
    /* 0x60 */ f32 unk60;
    /* 0x64 */ f32 unk64;
    /* 0x68 */ f32 unk68; /* facing, radians */
    /* 0x6C */ f32 unk6C;
    /* 0x70 */ f32 unk70; /* facing, degrees */
    /* 0x74 */ f32 unk74; /* scale */
    /* 0x78 */ s16 unk78;
    /* 0x7A */ s16 unk7A;
    /* 0x7C */ u16 unk7C;
    /* 0x7E */ u16 unk7E; /* kind */
    /* 0x80 */ s16 unk80;
    /* 0x82 */ s8 unk82;
    /* 0x83 */ u8 pad83[0x2];
    /* 0x85 */ u8 unk85;
    /* 0x86 */ u8 unk86;
    /* 0x87 */ u8 unk87;
    /* 0x88 */ Unk3D8C98 unk88;
} StageObjSetup;

/* A stage object list (0x1680 bytes: D_01FBF680, D_01FC0D00) [I]. */
typedef struct {
    /* 0x0000 */ StageObjSetup unk0[36];
    /* 0x1440 */ u128 unk1440[4][8];
    /* 0x1640 */ u128 unk1640;
    /* 0x1650 */ u128 unk1650;
    /* 0x1660 */ u8 unk1660[0x12];
    /* 0x1672 */ u8 pad1672[0x2];
    /* 0x1674 */ f32 **unk1674; /* per kind: offset x, z and turn of the next object [I] */
    /* 0x1678 */ s16 unk1678;   /* object count */
} StageObjSetupList;

typedef struct {
    /* 0x00 */ u8 pad0[0x1A];
    /* 0x1A */ s8 unk1A;
    /* 0x1B */ u8 pad1B;
    /* 0x1C */ s32 unk1C;
} Unk3D91B0;

extern s16 gMode;
extern s32 D_005F6F84;
extern s32 D_005F5EE0;
extern u16 D_005F5CB6;
extern u16 D_0046B280[];
extern s32 D_005F5EE4;
extern u8 D_0043B800[];
extern s32 D_01FBF620[];
extern TeamMember D_00440940[2][2];
extern Fighter gFighters[4];
extern u8 D_00879666[]; /* the low byte of gFighters[0].unk26 (character * 8 + costume) */
extern f32 D_005F6FC8;
extern s32 D_005F6F88;
extern s32 D_005F6FC0;
extern u128 D_00454340;
extern u128 D_00454350;
extern u8 D_00454360[];
extern u128 D_00454140[4][8];
extern f32 D_005F6FCC;

s32 func_003800D8(s32 n);
void *memset(void *p, s32 c, s32 n);
void func_00366EA8(void *m);
s32 func_003A9698(void);
s32 Tag_StepCounters(s32 side, s32 a);
s32 rand(void);
Unk3B87C8 *func_003B87C8(s32 id);
Unk3B87B0 *func_003B87B0(s32 id);
f32 sinf(f32 x);
f32 cosf(f32 x);
f32 func_0034A570(f32 a);
s32 func_003ABCD0(void);
s16 func_003A9680(void);
s32 func_003BB4D8(void);
void sceGifPkInit(void *pk, void *base);
void sceGifPkEnd(void *pk, u32 a, u32 b, u32 c);
void sceGifPkTerminate(void *pk);
void FlushCache(s32 op);
void func_0035E6B8(void *tag);
u32 func_00354290(void *pk, void *rec, u32 base);
void func_003D8158(u8 *p, s32 off, u32 mode);

/* The f32 at Fighter + 0x620 (inside a pad of include/fighter.h): a camera distance of the fighter [I]. */
#define FIGHTER_F620(f) (*(f32 *)((u8 *)(f) + 0x620))

/* Stage model nodes (0x3D6208-0x3D8158). */
/* H5 group: the animated stage objects of model_anim_b (0x3D6208-0x3D8158) and the module's small float helpers. */


/* Integer state of a stage object (StgObj.unk28). */
typedef struct {
    /* 0x00 */ s32 unk0;  /* matrix / model index */
    /* 0x04 */ s32 unk4;  /* flag */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;  /* phase */
    /* 0x10 */ s32 unk10; /* frame counter */
    /* 0x14 */ s32 unk14;
} StgObjI;

/* Float state of a stage object (StgObj.unk30). */
typedef struct {
    /* 0x00 */ f32 unk0;
    /* 0x04 */ f32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ f32 unk20;
    /* 0x24 */ f32 unk24;
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;
    /* 0x30 */ f32 unk30;
    /* 0x34 */ f32 unk34;
    /* 0x38 */ f32 unk38;
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ f32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ f32 unk4C;
    /* 0x50 */ f32 unk50;
} StgObjF;

/* A model node of the stage hierarchy (0x70 bytes, pool D_00445690, func_003D1A80; the ModelNode of
 * src/model_anim.c seen from the stage objects). */
typedef struct StgObj {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8; /* active */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ struct StgObj *parent;
    /* 0x18 */ struct StgObj **unk18; /* child list [I] */
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ StgObjI *unk28; /* unk0: matrix slot index in D_00444CD0 */
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ StgObjF *unk30; /* per-object work values */
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ FVec4 rot;
    /* 0x50 */ FVec4 pos;
    /* 0x60 */ FVec4 unk60;
} StgObj; /* size 0x70 */

typedef StgObj *StgObjPtrs10[10];

/* A Hermite curve in Newton form (0x444D78): doubled knots and the divided-difference table. */
typedef struct {
    /* 0x000 */ f32 knot[20];
    /* 0x050 */ f32 dd[20][20]; /* dd[j][0] are the Newton coefficients */
} Hermite;

/* The stage path block at 0x444CD0: a centre, 40 node pointers and the curve (also read as D_00444D78). */
typedef struct {
    /* 0x000 */ f32 x;
    /* 0x004 */ f32 z;
    /* 0x008 */ StgObj *obj[40];
    /* 0x0A8 */ Hermite curve;
} StgPath;

typedef struct {
    f32 v[4];
} __attribute__((aligned(8))) V4;


int rand(void);
f32 atan2f(f32 y, f32 x);
f32 sqrtf(f32);
void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0RotMatrix(Vu0Matrix m0, Vu0Matrix m1, f32 *r);
void sceVu0RotMatrixX(Vu0Matrix m0, Vu0Matrix m1, f32 r);
void sceVu0RotMatrixY(Vu0Matrix m0, Vu0Matrix m1, f32 r);
void sceVu0RotMatrixZ(Vu0Matrix m0, Vu0Matrix m1, f32 r);
void sceVu0TransMatrix(Vu0Matrix m0, Vu0Matrix m1, f32 *v);
void sceVu0ApplyMatrix(f32 *out, Vu0Matrix m, f32 *v);
void sceVu0ScaleVectorXYZ(f32 *out, f32 *v, f32 s);
void func_00353C88(s32 i, f32 *out);
f32 fabsf(f32);
f32 sinf(f32);
f32 cosf(f32);
f32 sceVu0InnerProduct(f32 *a, f32 *b);
void sceVu0DivVectorXYZ(f32 *out, f32 *v, f32 q);
s32 func_003800D8(s32 n);
extern u16 D_005F5CBC;
extern Vu0Matrix D_004334C0;
extern Vu0Matrix D_00433400;
void sceVu0AddVector(f32 *out, f32 *a, f32 *b);
void sceVu0SubVector(f32 *out, f32 *a, f32 *b);
void sceVu0ScaleVector(f32 *out, f32 *v, f32 s);
void func_0034A868(f32 *out, f32 *v);
s32 func_003D7A40(s32 i, f32 x, f32 y, f32 z);
void func_00356710(Vu0Matrix m);
void Model_GetMatrix(Vu0Matrix m);
void func_003D1468(StgObj *o);
void Model_ClearMatrixStack(void);
void Model_LoadIdentity(void);
extern s16 gMode;
extern Fighter gFighters[4];
void Wind_Sample(f32 x, f32 y, f32 z, f32 *out);
void func_003E13C8(f32 x);
f32 func_003D80E8(f32 a);
f32 func_003D8088(f32 a);
f32 func_003D8128(void);

extern Hermite D_00444D78;
extern f32 D_00445408[20];
extern f32 D_0044C180[4];
extern f32 D_0044C190[4];
extern const V4 D_00521BF0;
extern const V4 D_00521C00;
extern const V4 D_00521C10;
extern const V4 D_00521C20;
StgObj *func_003D1A80(void);
void func_003D1B68(StgObj *dst, StgObj *src);
extern Vu0Matrix D_00445460[];
extern Vu0Matrix D_004455A0[2];
extern f32 D_00445620[];
extern Vu0Matrix D_0046AF50[2];
extern f32 D_0044C170[4];

/* Stage animations of 0x3D47A8-0x3D6208 (flock, lanes, orbits, sound panning). */
typedef struct {
    f32 v[4];
} __attribute__((aligned(16))) V4q;


/* One flying thing of the flock stage object (0xC0 bytes; ten of them at D_00444D20) [I]. */
typedef struct {
    /* 0x00 */ f32 pos[4];
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 speed; /* along Z [I] */
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ struct Unk44B1C0 *range;
    /* 0x24 */ s32 unk24[3];
    /* 0x30 */ struct Unk44B1C0 {
        f32 v[8]; /* lower bounds 0-3, upper bounds 4-7 [I] */
    } __attribute__((aligned(8))) lim;
    /* 0x50 */ struct Unk444D20Sub {
        /* 0x00 */ f32 unk0;
        /* 0x04 */ f32 unk4;
        /* 0x08 */ f32 unk8;
        /* 0x0C */ f32 unkC;
        /* 0x10 */ f32 unk10;
        /* 0x14 */ f32 unk14;
        /* 0x18 */ struct StgObj *part;
    } sub[4];
} Unk444D20; /* size 0xC0 */

/* The stage animation work area at 0x444CD0 (.data): matrices, counters, object pointers by stage [I]. */
typedef union {
    Vu0Matrix m[32];
    s32 i[512];
    f32 f[512];
    struct StgObj *p[512];
    struct {
        /* 0x000 */ s32 timer;
        /* 0x004 */ u8 pad4[0x3C];
        /* 0x040 */ struct StgObj *target;
        /* 0x044 */ u8 pad44[0xC];
        /* 0x050 */ Unk444D20 bird[10];
        /* 0x7D0 */ f32 unk7D0;
        /* 0x7D4 */ s32 unk7D4;
    } fl;
    StgPath path;
    struct Unk444CD0Rw {
        /* 0x000 */ struct Unk444CD0Row {
            /* 0x00 */ s16 count;
            /* 0x04 */ f32 unk4;
            /* 0x08 */ f32 speed;
            /* 0x0C */ struct StgObj *item[20];
        } row[10];
        /* 0x398 */ struct StgObj *list[40];
    } rw;
} Unk444CD0;
extern Unk444CD0 D_00444CD0;
extern StgObj *D_00444D10[];
extern Unk444D20 D_00444D20[10]; /* D_00444CD0.fl.bird */
extern struct Unk44B1C0 D_0044B1C0[]; /* the flock's position ranges (.data) */ /* inside the work area (D_00444CD0 + 0x40) */

/* One sound / light source entry of the scene (0x150 bytes) [I]. */
typedef struct {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ s32 unk140;
    /* 0x144 */ s32 unk144;
    /* 0x148 */ s32 unk148;
    /* 0x14C */ s32 unk14C;
} __attribute__((aligned(8))) Unk5F6780Ent;

typedef struct {
    /* 0x000 */ f32 dist;
    /* 0x004 */ s32 index;
} Unk5F6780Ref;

/* The scene work area D_005F6780 points to: the wind grid (src: func_003D43A8) and the queued scene entries. */
typedef struct {
    /* 0x00000 */ u8 pad0[0x6];
    /* 0x00006 */ s16 unk6;                   /* wind grid: last column */
    /* 0x00008 */ s16 unk8;                   /* wind grid: last row */
    /* 0x0000A */ u8 padA[0xA];
    /* 0x00014 */ f32 unk14;                  /* wind grid: column spacing */
    /* 0x00018 */ f32 unk18;                  /* wind grid: row spacing */
    /* 0x0001C */ u8 pad1C[0x2D4];
    /* 0x002F0 */ FVec4 (*unk2F0)[17];        /* wind grid rows */
    /* 0x002F4 */ s32 count;
    /* 0x002F8 */ u8 pad2F8[8];
    /* 0x00300 */ Unk5F6780Ent ent[304];
    /* 0x19200 */ u8 pad19200[0x100];
    /* 0x19300 */ Unk5F6780Ref ref[1];
} Unk5F6780;
extern Unk5F6780 *D_005F6780;

extern s32 D_0041580C[]; /* [0]: a frame counter [I] */
extern const V4q D_00521B80;
extern const V4q D_00521B90;
extern const V4q D_00521BA0;
extern const V4q D_00521BB0;
extern const V4q D_00521BC0;
extern f32 D_005F67B4[];
extern f32 D_005F6798[];
extern Vu0Matrix D_00433400; /* the camera (world to view) matrix [I] */

f32 func_003D80E8(f32 a);   /* wraps an angle into (-pi, pi] [I] */
f32 func_003D8128(void);    /* rand() * 2^-31: uniform in [0, 1) */
f32 func_003D8088(f32 a);
f32 Fx_RandFloat(void);
s32 func_003800D8(s32 n);
f32 func_00366EC8(void);
void func_00356710(Vu0Matrix m); /* unit matrix */
void func_003D1B48(StgObj *o, f32 *pos);
void func_003D10D0(s32 i, Vu0Matrix m, f32 s);
void Model_QueueDraw(Unk5F6780Ent *e, s32 a, s32 n, s32 b, f32 s, f32 d);
void Model_GetMatrix(Vu0Matrix m); /* copies the model's root matrix (D_01FAD6D0) */
void func_0034A868(f32 *v0, f32 *v1); /* normalises a vector [I] */
s32 func_00375930(u16 left, u16 right);
s32 func_003758F0(u16 left, u16 right);
s32 func_0035A5A8(void);
s32 func_003D2110(void);
void func_00366EB8(Vu0Matrix m); /* copies the camera matrix [I] */
s32 func_003ABCD0(void);
StgObj *func_003D1A80(void); /* allocates a model part [I] */
void func_003D1B68(StgObj *o, StgObj *tmpl); /* nonzero while the game is paused or in a replay [I] */
void func_003D1160(StgObj *o, Vu0Matrix m, f32 s);
void sceVu0AddVector(f32 *out, f32 *a, f32 *b);
f32 sqrtf(f32 x);

/* A bare sqrt.s with the two nops the compiler puts in front of it: this object is built without -ffast-math, where
 * sqrtf() would add the errno check and call (fake: the original's own sqrt helper is unknown). */
static inline f32 SqrtS(f32 x) {
    f32 r;

    __asm__ volatile("nop\n\tnop\n\tsqrt.s %0, %1" : "=f"(r) : "f"(x));
    return r;
}
f32 fabsf(f32 x);

void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0RotMatrix(Vu0Matrix m0, Vu0Matrix m1, f32 *r);
void sceVu0RotMatrixX(Vu0Matrix m0, Vu0Matrix m1, f32 rx);
void sceVu0RotMatrixY(Vu0Matrix m0, Vu0Matrix m1, f32 ry);
void sceVu0RotMatrixZ(Vu0Matrix m0, Vu0Matrix m1, f32 rz);
void sceVu0TransMatrix(Vu0Matrix m0, Vu0Matrix m1, f32 *v);
void sceVu0ApplyMatrix(f32 *out, Vu0Matrix m, f32 *v);
f32 sceVu0InnerProduct(f32 *a, f32 *b);
f32 sinf(f32 x);
f32 cosf(f32 x);
f32 atan2f(f32 y, f32 x);

/* Stage objects (0x3D9588-0x3DB758). */

/* Description of a stage object (shared by its instances). */
typedef struct {
    /* 0x00 */ u8 unk0[6];
    /* 0x06 */ u16 unk6; /* 1 / 2: matrix from func_003D7D90; 0x10: shadow; 0x40: extra rotation */
    /* 0x08 */ u8 unk8[0x10];
    /* 0x18 */ s8 unk18;
} StageObjDesc;

/* An object of a stage object list (0x90 bytes). */
typedef struct StageObj {
    /* 0x00 */ struct StageObj *self; /* the object in its list (the list is worked on in a scratchpad copy) */
    /* 0x04 */ u8 unk4[0xC];
    /* 0x10 */ Vu0Matrix mtx;
    /* 0x50 */ u8 unk50[4];
    /* 0x54 */ StageObjDesc *desc;
    /* 0x58 */ s32 unk58; /* animation data of unk7E */
    /* 0x5C */ f32 pos[3];
    /* 0x68 */ f32 rotY;
    /* 0x6C */ f32 unk6C; /* extra z rotation of bones 14 and 17 */
    /* 0x70 */ u8 unk70[4];
    /* 0x74 */ f32 scale;
    /* 0x78 */ s16 unk78; /* wait frames */
    /* 0x7A */ s16 unk7A;
    /* 0x7C */ u16 unk7C; /* frame */
    /* 0x7E */ u16 unk7E; /* animation */
    /* 0x80 */ s16 unk80; /* bounding box index */
    /* 0x82 */ u8 unk82;
    /* 0x83 */ u8 unk83; /* visible */
    /* 0x84 */ u8 unk84; /* shadow visible */
    /* 0x85 */ u8 unk85; /* flags */
    /* 0x86 */ u8 unk86;
    /* 0x87 */ u8 unk87[9];
} StageObj;

/* D_01FBF680 / D_01FC0D00: the two stage object lists (0x1680 bytes each). */
typedef struct {
    /* 0x0000 */ StageObj obj[36];
    /* 0x1440 */ FVec4 bbox[4][8];
    /* 0x1640 */ FVec4 minv; /* clip volume; w set to the frame's distance */
    /* 0x1650 */ FVec4 maxv;
    /* 0x1660 */ s8 parent[0x14]; /* parent bone of each bone (drawing hooks) */
    /* 0x1674 */ s32 unk1674;
    /* 0x1678 */ s16 unk1678; /* object count */
    /* 0x167A */ u8 unk167A[6];
} StageObjList;

/* Model bank of the stage objects (D_01FC2380[k]). */
typedef struct {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ s32 dataOffset; /* VU1 data, 16-aligned after this offset */
    /* 0x10 */ s32 count;      /* number of DMA references */
    /* 0x14 */ s32 refs[1][2]; /* quadword count of each reference */
} StageBank;

/* VIF1 packet of the SDK (sceVif1Packet). */
typedef struct {
    /* 0x0 */ u32 *pCurrent;
    /* 0x4 */ u32 *pBase;
    /* 0x8 */ u32 *pDmaTag;
    /* 0xC */ u32 *pVifCode;
} Vif1Packet;

/* Depth-sort node of the update pass (scratchpad). */
typedef struct SoNode {
    /* 0x0 */ struct SoNode *next;
    /* 0x4 */ StageObj *obj;
} SoNode;

/* Scratchpad work area of the update pass (Spr_Setup(1, 0x34C0)). */
typedef struct {
    /* 0x0000 */ u8 unk0[0x1200];
    /* 0x1200 */ StageObjList list;
    /* 0x2880 */ Vu0Matrix clip;
    /* 0x28C0 */ Vu0Matrix lightClip;
    /* 0x2900 */ Vu0Matrix anim;
    /* 0x2940 */ Vu0Matrix view;
    /* 0x2980 */ Vu0Matrix shadow;
    /* 0x29C0 */ Vu0Matrix light; /* func_00353E18 */
    /* 0x2A00 */ FVec4 lightc[4];
    /* 0x2A40 */ u8 unk2A40[0x440];
    /* 0x2E80 */ Vu0Matrix bones[18];
    /* 0x3300 */ SoNode head;
    /* 0x3308 */ SoNode node[36];
    /* 0x3428 */ s16 pose[19][3]; /* root (angle / y / radius in the update hooks), then one rotation per bone */
} StageSpr;

/* D_00453FC8: per-stage entry (12 bytes): data of list 1, data of list 2, shared resource. */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 unk8;
} Unk453FC8;

extern s32 D_005F67B8; /* stage object flags */
extern s32 D_005F6F80; /* index of the stage in D_00453FC8 */
extern s32 D_005F6F84; /* stage id the lists were set up for */
extern s32 D_005F6F88; /* number of extra animation banks (<= 16) */
extern s32 D_005F6FC0;
extern s32 D_005F6FC4; /* frame counter (0..189) */
extern f32 D_005F6FC8;
extern f32 D_005F6FCC;
extern void (*D_005F6FD0)(StageObjList *list, f32 t, s32 n);
extern void (*D_005F6FD4)(StageObjList *list);
extern u16 D_005F5CB6;
extern Unk453FC8 D_00453FC8[];
extern StageObjList D_01FBF680;
extern StageObjList D_01FC0D00;
extern void *D_01FC2380[];
extern u32 D_004FAF80[]; /* VU1 packet template of the stage objects (to D_004FB800) */
extern u32 D_004FAFB0[];
extern u32 D_004FB800[];
extern f32 D_005F6944;
extern f32 D_005F6948;

void FlushCache(s32 op);
void *Spr_Setup(s32 count, s32 reserved);
void func_00343128(void *dst, void *src, s32 qwc);
void Dma_Wait(u32 channel);
void func_00353C88(s32 i, f32 *out);
s32 func_00367CD0(u16 anim);
s32 func_00364168(u16 anim);
s16 *func_00364190(u16 anim, u16 frame);
s32 func_00367D00(u16 anim);
void func_00367FE0(s32 data, s16 *out, u16 frame);
void func_003D56D8(s32 i, Vu0Matrix m);
void func_003D7D90(s32 i, Vu0Matrix m);
void func_003D8748(Vu0Matrix m);
void func_003D8E30(StageObjSetup *o, StageObjSetup *o2, StageObjSetupList *l);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0RotMatrixY(Vu0Matrix m0, Vu0Matrix m1, f32 r);
s32 sceVu0ClipAll(f32 *minv, f32 *maxv, Vu0Matrix ms, FVec4 *vm, s32 n);
f32 sinf(f32 x);
f32 cosf(f32 x);
void func_00353E18(Vu0Matrix m);
void func_00367FE8(s32 data, s16 *out, u16 frame);
void sceVu0RotMatrixZ(Vu0Matrix m0, Vu0Matrix m1, f32 r);
void sceVif1PkInit(Vif1Packet *pk, void *base);
void sceVif1PkCnt(Vif1Packet *pk, u32 flag);
void sceVif1PkAlign(Vif1Packet *pk, u32 bit, u32 pos);
u32 *sceVif1PkReserve(Vif1Packet *pk, u32 count);
void sceVif1PkRef(Vif1Packet *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceVif1PkTerminate(Vif1Packet *pk);
u32 sceVif1PkSize(Vif1Packet *pk);
void func_00343178(void *spr, u32 qwc);
void *func_00364528(s32 i, void *bank);
s32 func_00364550(void *bank);
s32 func_00364558(s32 i, void *bank);
void func_00364130(void *p);
s32 func_00364160(void);
void func_00367CA0(void *p);
s32 func_00367CC8(void);
s32 func_0035A648(void);
void Snd_PlaySe(s32 id, s32 player);
void func_003D8478(void *p, s32 a);
void func_003D8490(void *p, s32 a);
s32 func_003D85D8(s32 stage);
s32 func_003D86D8(void);
s32 func_003D8700(void);
s32 func_003D9280(StageObjSetupList *l, StageObjDef *d);
s32 func_003D9410(void);
f32 func_003D9468(void);

void func_003D9588(StageObjList *list, f32 t, s32 n);
void func_003D9BE8(StageObjList *list, f32 t, s32 n);
void func_003DA138(StageObjList *list);
void func_003DAA80(StageObjList *list);
void func_003DB698(s32 n);

static inline void UnitMtx(Vu0Matrix m) {
    u128 z = 0;

    ((u128 *)m)[0] = z;
    ((u128 *)m)[1] = z;
    ((u128 *)m)[2] = z;
    ((u128 *)m)[3] = z;
    /* fake: 1.0f stored as an integer word; the float form gives lui / mtc1 / swc1 where the original has lui / sw */
    ((s32 *)m)[0] = 0x3F800000;
    ((s32 *)m)[5] = 0x3F800000;
    ((s32 *)m)[10] = 0x3F800000;
    ((s32 *)m)[15] = 0x3F800000;
}

static inline void CopyMtx(Vu0Matrix d, Vu0Matrix s) {
    ((u128 *)d)[0] = ((u128 *)s)[0];
    ((u128 *)d)[1] = ((u128 *)s)[1];
    ((u128 *)d)[2] = ((u128 *)s)[2];
    ((u128 *)d)[3] = ((u128 *)s)[3];
}

/* ---- model_anim_b: the stage wind ---- */

/* A wind generator (D_00447FE0, 3 x 0x1060): a Lorenz attractor sampled into a ring of 0x400 values, blowing along
 * unk10 with waves travelling across the line through unk20 [I]. */
typedef struct {
    /* 0x0000 */ s32 unk0;                    /* active */
    /* 0x0004 */ s32 unk4;                    /* frames */
    /* 0x0008 */ f32 unk8;                    /* wave length */
    /* 0x000C */ u8 padC[0x4];
    /* 0x0010 */ FVec4 unk10;                 /* direction, unit in x/z */
    /* 0x0020 */ FVec4 unk20;                 /* origin of the wave front */
    /* 0x0030 */ f32 unk30;                   /* strength */
    /* 0x0034 */ f32 unk34;                   /* slope z / x of the direction */
    /* 0x0038 */ f32 unk38;                   /* slope of the wave front, -1 / unk34 */
    /* 0x003C */ f32 unk3C;                   /* 1 / unk8 */
    /* 0x0040 */ s32 unk40;                   /* > 0: steps per frame; < 0: frames per step */
    /* 0x0044 */ s32 unk44;                   /* next sample */
    /* 0x0048 */ f32 unk48;                   /* attractor x */
    /* 0x004C */ f32 unk4C;                   /* attractor y */
    /* 0x0050 */ f32 unk50;                   /* attractor z */
    /* 0x0054 */ f32 unk54;                   /* last step */
    /* 0x0058 */ f32 unk58;                   /* step before */
    /* 0x005C */ f32 unk5C[0x400];            /* samples */
} WindGen;

/* A gust (D_0044B100, 4 x 0x30): a radial push from unk20 that fades with time [I]. */
typedef struct {
    /* 0x00 */ s32 unk0;                      /* active */
    /* 0x04 */ s32 unk4;                      /* frames */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;                     /* strength */
    /* 0x14 */ u8 pad14[0xC];
    /* 0x20 */ FVec4 unk20;                   /* centre */
} WindGust;

/* A wind generator of a stage's list (0x20 bytes). */
typedef struct {
    /* 0x00 */ s32 unk0;                      /* WindGen.unk40 */
    /* 0x04 */ f32 unk4;                      /* wave length */
    /* 0x08 */ f32 unk8;                      /* strength */
    /* 0x0C */ u8 padC[0x4];
    /* 0x10 */ FVec4 unk10;                   /* direction */
} StageWindGen;

/* The wind of a stage (D_00447EC0, 0xC bytes per stage). */
typedef struct {
    /* 0x0 */ s32 unk0;                       /* generator count */
    /* 0x4 */ StageWindGen *unk4;
    /* 0x8 */ s32 unk8;                       /* gusts allowed (D_005F6794) */
} StageWind;

/* The grid D_005F6780 points to. */
typedef Unk5F6780 WindGrid;

extern StageWind D_00447EC0[];
extern WindGen D_00447FE0[3];
extern WindGust D_0044B100[4];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 D_005F6790;
extern s32 D_005F6794;
extern s32 D_005F6F2C;
extern s32 D_005F6F30;
extern s32 D_005F6F34;
extern s32 D_005F6F38;
extern s32 D_005F6F3C;
extern s32 D_005F6F40;
extern FVec4 D_01FB6EF0[9][0x78][2];
extern FVec4 D_01FBE6F0[0x78][2];
extern s32 D_01FBF5F0[8];

void *memset(void *p, s32 c, s32 n);
f32 sqrtf(f32 x);

/* The square root the module uses: an inline sqrt.s the scheduler cannot move code across (the builtin sqrtf
 * schedules differently and folds x / sqrtf(y) into rsqrt.s) [I]. */
static inline f32 Sqrt(f32 x) {
    f32 r;

    __asm__ volatile("nop\n\tnop\n\tsqrt.s %0, %1" : "=f"(r) : "f"(x));
    return r;
}
f32 fabsf(f32 x);
f32 powf(f32 x, f32 y);
void sceVu0ScaleVector(f32 *out, f32 *in, f32 s);
void sceVu0ScaleVectorXYZ(f32 *out, f32 *in, f32 s);
void sceVu0AddVector(f32 *out, f32 *a, f32 *b);
void sceVu0SubVector(f32 *out, f32 *a, f32 *b);
void sceVu0DivVectorXYZ(f32 *out, f32 *in, f32 q);
void func_0034A868(f32 *out, f32 *in);
s32 func_00365D00(void);
s32 func_003A9698(void);
s32 func_003AA558(void);
s32 func_003ABCD0(void);
void func_003D3798(WindGen *g);
void func_003D37C8(WindGust *g);
s32 func_003D3988(f32 *dir, s32 rate, f32 len, f32 amp);
s32 func_003D3B80(f32 *pos, f32 amp);
void func_003D3E00(f32 x, f32 y, f32 z, f32 *out);
void func_003D3F48(WindGen *g, f32 *out, f32 *pos);
void func_003D3FF8(WindGust *g, f32 *out, f32 *pos);
s32 func_003D4178(WindGen *g);
void func_003D42A8(WindGust *g);
f32 Wind_StepLorenz(WindGen *g);

/* Sets up the wind of a stage (battle_flow, round init): clears the generators and gusts, starts the stage's
 * generators (D_00447EC0[stage]; strength x0.3 on stages 9 / 0x12 in one mode or when func_003AA558) and the
 * gust recorder [I]. */
void Wind_Init(s32 stage) {
    StageWind *s = &D_00447EC0[stage];
    StageWindGen *e = s->unk4;
    f32 k = 1.0f;
    s32 i;

    for (i = 0; i < 3; i++) {
        func_003D3798(&D_00447FE0[i]);
    }
    for (i = 0; i < 4; i++) {
        func_003D37C8(&D_0044B100[i]);
    }
    D_005F6790 = 1;
    D_005F6794 = s->unk8;
    if ((func_003ABCD0() != 0 && (stage == 9 || stage == 0x12)) || func_003AA558() != 0) {
        k = 0.3f;
    }
    for (i = 0; i < s->unk0; i++) {
        func_003D3988(e[i].unk10, e[i].unk0, e[i].unk4, e[i].unk8 * k);
    }
    D_005F6F2C = 0;
    D_005F6F30 = 1;
    D_005F6F34 = 0;
    D_005F6F38 = 0;
}

/* Resets a wind generator. */
void func_003D3798(WindGen *g) {
    g->unk0 = 0;
    g->unk4 = 0;
    g->unk40 = 1;
    g->unk44 = 0;
    g->unk30 = 1.0f;
    g->unk54 = 1.0f;
    g->unk58 = 1.0f;
}

/* Resets a gust. */
void func_003D37C8(WindGust *g) {
    g->unk0 = 0;
    g->unk4 = 0;
    g->unk10 = 0.0f;
    g->unkC = 0.05f;
}

/* Per frame: steps the active generators and gusts (unless paused), records the gust ring (D_005F6F30) or
 * replays it (D_005F6F2C, 120-frame ring at D_01FB6EF0 + 0x7800) [I]. */
void Wind_Update(void) {
    s32 i;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        for (i = 0; i < 3; i++) {
            if (D_00447FE0[i].unk0 != 0) {
                func_003D4178(&D_00447FE0[i]);
            }
        }
        for (i = 0; i < 4; i++) {
            if (D_0044B100[i].unk0 != 0) {
                func_003D42A8(&D_0044B100[i]);
            }
        }
    }
    if (D_005F6F30 != 0) {
        D_005F6F38++;
        D_005F6F34 = D_005F6F38 % 120;
        D_005F6F3C = 0;
        D_01FB6EF0[8][D_005F6F34][0][3] = 0.0f;
        D_01FB6EF0[8][D_005F6F34][1][3] = 0.0f;
    }
    if (D_005F6F2C != 0) {
        if (func_00365D00() != 0) {
            D_005F6F34++;
        }
        D_005F6F34 %= 120;
        for (i = 0; i < 2; i++) {
            if (D_01FB6EF0[8][D_005F6F34][i][3] != 0.0f) {
                func_003D3B80(D_01FB6EF0[8][D_005F6F34][i], D_01FB6EF0[8][D_005F6F34][i][3]);
            }
        }
    }
}

/* Starts a wind generator in a free slot: direction, wave length, strength, attractor rate; pre-fills its 0x400
 * samples; 0 when no slot is free or the direction has a zero x or z [I]. */
s32 func_003D3988(f32 *dir, s32 rate, f32 len, f32 amp) {
    WindGen *g;
    s32 slot = 0;
    s32 i;
    f32 *o;
    f32 x, z;
    f32 t;

    for (i = 0; i < 3; i++) {
        if (D_00447FE0[i].unk0 == 0) {
            slot = i;
            break;
        }
    }
    if (i == 3) {
        return 0;
    }
    if (dir[0] == 0.0f) {
        return 0;
    }
    if (dir[2] == 0.0f) {
        return 0;
    }
    g = &D_00447FE0[slot];
    g->unk0 = 1;
    g->unk40 = rate;
    g->unk48 = g->unk4C = g->unk50 = 1.0f;
    x = dir[0];
    z = dir[2];
    t = Sqrt(100000000.0f / (x * x / (z * z) + 1.0f));
    o = g->unk20;
    o[0] = t;
    o[1] = 0.0f;
    o[3] = 1.0f;
    o[2] = z / x * t;
    *(u128 *)g->unk10 = *(u128 *)dir;
    sceVu0DivVectorXYZ(g->unk10, g->unk10, Sqrt(g->unk10[0] * g->unk10[0] + g->unk10[2] * g->unk10[2]));
    g->unk30 = amp;
    g->unk8 = len;
    g->unk3C = 1.0f / len;
    g->unk34 = g->unk10[2] / g->unk10[0];
    g->unk38 = -1.0f / g->unk34;
    for (i = 0; i < 0x400; i++) {
        while (func_003D4178(g) != 0) {
        }
    }
}

#if 0
/* NON_MATCHING: 25 the original reloads D_005F6F34 / D_005F6F3C after the 16-byte ring store (alias), the build reuses them */
/* Starts a gust at pos (also written to the record ring while recording); 0 and gusts blocked when all 4 are
 * busy [I]. */
s32 func_003D3B80(f32 *pos, f32 amp) {
    WindGust *g;
    s32 i;
    u128 v;

    for (i = 0; i < 4; i++) {
        if (D_0044B100[i].unk0 == 0) {
            break;
        }
    }
    if (i == 4) {
        D_005F6790 = 0;
        return 0;
    }
    g = &D_0044B100[i];
    g->unk0 = 1;
    g->unk4 = 0;
    g->unk10 = amp;
    *(u128 *)g->unk20 = v = *(u128 *)pos;
    if (D_005F6F30 != 0) {
        u128 *r = (u128 *)D_01FBE6F0[D_005F6F34][D_005F6F3C];
        *r = v;
        D_01FBE6F0[D_005F6F34][D_005F6F3C][3] = amp;
        D_005F6F3C = (D_005F6F3C + 1) % 2;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D3B80);
#endif

/* A gust from a fighter's landing / hit (battle_flow): strength (w / 300 clamped to 0-1) squared x 30 [I]. */
void func_003D3C68(f32 x, f32 y, f32 z, f32 w) {
    FVec4 v;

    if (D_005F6794 != 0 && D_005F6790 != 0) {
        f32 max = 300.0f;

        w = w / max;
        if (w > 1.0f) {
            w = 1.0f;
        } else if (w < 0.0f) {
            w = 0.0f;
        }
        w = w * w;
        v[0] = x;
        v[1] = y;
        v[2] = z;
        v[3] = 1.0f;
        func_003D3B80(v, w * 30.0f);
    }
}

/* The wind of the generators at a point (fighters' cloth, effects). */
void Wind_Sample(f32 x, f32 y, f32 z, f32 *out) {
    FVec4 acc;
    FVec4 tmp;
    FVec4 pos;
    f32 *p;
    s32 i;

    memset(acc, 0, 0x10);
    p = pos;
    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
    acc[3] = 1.0f;
    p[3] = 1.0f;
    for (i = 0; i < 3; i++) {
        if (D_00447FE0[i].unk0 != 0) {
            func_003D3F48(&D_00447FE0[i], tmp, p);
            sceVu0AddVector(acc, acc, tmp);
        }
    }
    *(u128 *)out = *(u128 *)acc;
}

/* The wind of the generators and gusts at a point. */
void func_003D3E00(f32 x, f32 y, f32 z, f32 *out) {
    FVec4 acc;
    FVec4 tmp;
    FVec4 pos;
    f32 *p;
    s32 i;

    memset(acc, 0, 0x10);
    p = pos;
    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
    acc[3] = 1.0f;
    p[3] = 1.0f;
    for (i = 0; i < 3; i++) {
        if (D_00447FE0[i].unk0 != 0) {
            func_003D3F48(&D_00447FE0[i], tmp, p);
            sceVu0AddVector(acc, acc, tmp);
        }
    }
    for (i = 0; i < 4; i++) {
        if (D_0044B100[i].unk0 != 0) {
            func_003D3FF8(&D_0044B100[i], tmp, p);
            sceVu0AddVector(acc, acc, tmp);
        }
    }
    *(u128 *)out = *(u128 *)acc;
}

/* The wind of one generator at a point: its direction scaled by the sample at the point's distance to the wave
 * front line [I]. */
void func_003D3F48(WindGen *g, f32 *out, f32 *pos) {
    f32 c;
    f32 len;
    f32 d;
    s32 n;

    c = -g->unk38 * pos[0] + pos[2];
    len = Sqrt(g->unk38 * g->unk38 + 1.0f);
    d = fabsf(g->unk38 * g->unk20[0] - g->unk20[2] + c) / len * g->unk3C;
    n = (s32)d + g->unk44;
    n %= 0x400;
    sceVu0ScaleVectorXYZ(out, g->unk10, g->unk30 * g->unk5C[n]);
}

#if 0
/* NON_MATCHING: 10 order of the 1500.0f / unk4 / -0.1f loads around the inline sqrt.s (scheduling) */
/* The push of one gust at a point: away from its centre, fading with distance (1500) and age (2^(-0.1 t)). */
void func_003D3FF8(WindGust *g, f32 *out, f32 *pos) {
    FVec4 v;
    f32 range;
    f32 e;
    f32 t;

    memset(v, 0, 0x10);
    v[3] = 1.0f;
    if (g->unk4 > 0) {
        if (Sqrt((pos[0] - g->unk20[0]) * (pos[0] - g->unk20[0])
                  + (pos[2] - g->unk20[2]) * (pos[2] - g->unk20[2])) < 1500.0f) {
            sceVu0SubVector(v, pos, g->unk20);
            v[3] = 1.0f;
            if (v[0] == 0.0f && v[2] == 0.0f) {
                v[0] = v[2] = 0.1f;
            }
            func_0034A868(v, v);
            range = 1500.0f;
            e = g->unk4;
            t = 1.0f - Sqrt((pos[0] - g->unk20[0]) * (pos[0] - g->unk20[0])
                             + (pos[2] - g->unk20[2]) * (pos[2] - g->unk20[2])) / range;
            sceVu0ScaleVector(v, v, g->unk10 * powf(2.0f, e * -0.1f) * t);
        }
    }
    *(u128 *)out = *(u128 *)v;
}
#else
LIT4_WORD(D_005F53A8, 0x44BB8000);
LIT4_WORD(D_005F53AC, 0x3DCCCCCD);
LIT4_WORD(D_005F53B0, 0x44BB8000);
LIT4_WORD(D_005F53B4, 0xBDCCCCCD);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D3FF8);
#endif

/* Steps a generator once: one or more attractor steps, or an extrapolation between steps; stores the sample and
 * returns the next sample index (0 after a wrap). */
s32 func_003D4178(WindGen *g) {
    f32 v = 0.0f;
    s32 i;
    s32 r;
    s32 idx;

    g->unk4++;
    idx = g->unk44;
    if (g->unk40 >= 0) {
        for (i = 0; i < g->unk40; i++) {
            v = Wind_StepLorenz(g);
        }
    } else {
        i = -g->unk40;
        r = g->unk4 % i;
        if (r == 0) {
            g->unk58 = g->unk54;
            v = Wind_StepLorenz(g);
            g->unk54 = v;
        } else {
            v = g->unk54 + (g->unk54 - g->unk58) * ((f32)r / (f32)i);
        }
    }
    g->unk5C[idx] = v;
    g->unk44++;
    if (g->unk44 >= 0x400) {
        g->unk44 = 0;
    }
    return g->unk44;
}

/* Ages a gust; it ends after 300 frames and lets new gusts start. */
void func_003D42A8(WindGust *g) {
    g->unk4++;
    g->unkC -= 0.004f;
    if (g->unkC < 0.001f) {
        g->unkC = 0.001f;
    }
    if (g->unk4 >= 301) {
        g->unk0 = 0;
        D_005F6790 = 1;
    }
}

/* One Euler step of the Lorenz attractor (sigma 10, rho 28, beta 8/3, dt 0.01); returns (x + 40) / 80. */
f32 Wind_StepLorenz(WindGen *g) {
    f32 y = g->unk4C;
    f32 x = g->unk48;
    f32 z = g->unk50;
    f32 nx;
    f32 scale = 80.0f;
    f32 dt = 0.01f;
    f32 dx;

    dx = (y - x) * 10.0f;
    nx = x + dx * dt;
    g->unk48 = nx;
    g->unk50 = z + (x * y - z * 2.6666667f) * dt;
    g->unk4C = y + (x * (28.0f - z) - y) * dt;
    return (nx + 40.0f) / scale;
}

/* Resets a gust-like record and a counter [I]. */
void func_003D4388(WindGust *g, s32 *p) {
    g->unk0 = 0;
    g->unk4 = 0;
    g->unk8 = 0;
    g->unkC = 1.0f;
    *p = 0;
}

#if 0
/* NON_MATCHING: 8 f23/f24 swapped for the row/column spacings, one store order (register allocation) */
/* Fills the wind grid of D_005F6780 (rows of 17 vectors) around pos: wind x scale, y = -|xz| - off [I]. */
void func_003D43A8(f32 *pos, f32 scale, f32 off) {
    s16 nx = D_005F6780->unk6;
    s16 nz = D_005F6780->unk8;
    f32 dz = D_005F6780->unk18;
    f32 dx = D_005F6780->unk14;
    f32 z0 = -dz * (nz * 0.5f);
    f32 x0 = -dx * (nx * 0.5f);
    FVec4 w;
    FVec4 p;
    FVec4 *row;
    s32 i, j;

    memset(p, 0, 0x10);
    p[3] = 1.0f;
    z0 -= dz * 0.5f;
    x0 -= dx * 0.5f;
    for (j = 0; j <= nz; j++) {
        row = D_005F6780->unk2F0[j];
        for (i = 0; i <= nx; i++) {
            p[2] = z0 + dz * j;
            p[0] = x0 + dx * i;
            sceVu0AddVector(w, pos, p);
            func_003D3E00(w[0], w[1], w[2], row[i]);
            sceVu0ScaleVectorXYZ(row[i], row[i], scale);
            row[i][1] = -Sqrt(row[i][2] * row[i][2] + row[i][0] * row[i][0]) - off;
            row[i][3] = 0.0f;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D43A8);
#endif

/* Starts replaying the live gust ring from n frames back (slot 8). */
void func_003D45A8(s32 n) {
    s32 i;

    for (i = 0; i < 4; i++) {
        func_003D37C8(&D_0044B100[i]);
    }
    D_005F6F2C = 1;
    D_005F6F30 = 0;
    D_005F6F34 = D_005F6F38 - n;
    if (D_005F6F34 < 0) {
        D_005F6F34 += 120;
    }
    D_005F6F40 = 8;
}

/* Starts replaying a saved gust ring slot from n frames back. */
void func_003D4638(s32 slot, s32 n) {
    s32 i;

    for (i = 0; i < 4; i++) {
        func_003D37C8(&D_0044B100[i]);
    }
    D_005F6F2C = 1;
    D_005F6F30 = 0;
    D_005F6F40 = slot;
    D_005F6F38 = D_01FBF5F0[slot];
    D_005F6F34 = D_005F6F38 - n;
    if (D_005F6F34 < 0) {
        D_005F6F34 += 120;
    }
}

/* Restarts gust recording from frame 0. */
void func_003D46F0(void) {
    D_005F6F2C = 0;
    D_005F6F30 = 1;
    D_005F6F38 = 0;
}

/* Stops a replay and resumes recording (battle_flow). */
void func_003D4708(void) {
    D_005F6F2C = 0;
    D_005F6F30 = 1;
}

/* Stops both recording and replay (battle_flow). */
void func_003D4718(void) {
    D_005F6F2C = 0;
    D_005F6F30 = 0;
}

#if 0
/* NON_MATCHING: 10 the build hoists the D_005F6F38 load above the copy loop; the original loads it after (alias of the u128 stores) */
/* Saves the live gust ring and its frame count into a slot (replays). */
void func_003D4728(s32 slot) {
    s32 i, j;

    for (i = 0; i < 0x78; i++) {
        for (j = 0; j < 2; j++) {
            *(u128 *)D_01FB6EF0[slot][i][j] = *(u128 *)D_01FB6EF0[8][i][j];
        }
    }
    D_01FBF5F0[slot] = D_005F6F38;
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D4728);
#endif

/* Uniform random value in [lo, hi). */
f32 func_003D47A8(f32 lo, f32 hi) {
    return lo + func_003D8128() * (hi - lo);
}

/* Uniform random value in (c - r, c + r]. */
f32 func_003D47E8(f32 c, f32 r) {
    return c + (1.0f - func_003D8128() * 2.0f) * r;
}

/* Sets up the flock: ten birds with random ranges, each made of four distinct random parts among parts 1-10, and the
 * flock leader (part 11) [I]. */
void func_003D4830(StgObj *o) {
    s32 used[10];
    StgObjPtrs10 parts = { &o[1], &o[2], &o[3], &o[4], &o[5], &o[6], &o[7], &o[8], &o[9], &o[10] };
    Unk444D20 *b;
    struct Unk44B1C0 *t;
    f32 *r;
    struct Unk444D20Sub *sub;
    struct Unk444D20Sub *e;
    s32 i;
    s32 j;
    s32 k;
    f32 a;
    Unk444D20 *birds = D_00444D20;

    o->unk1C = 4;
    o->unkC = 0;
    for (i = 0; i < 10; i++) {
        t = &D_0044B1C0[func_003800D8(2)];
        b = &birds[i];
        r = b->lim.v;
        sub = b->sub;
        b->range = t;
        b->lim = *t;
        r[1] = r[5] = func_003D47A8(r[1], r[5]);
        b->lim.v[0] = r[4] = func_003D47A8(b->lim.v[0], r[4]);
        b->pos[2] = func_003D47A8(r[2], r[6]);
        b->unk10 = 0.0f;
        b->unk14 = 0.0f;
        b->speed = func_003D47A8(20.0f, 40.0f);
        b->pos[0] = b->lim.v[0];
        b->unk1C = 1.0f;
        b->pos[1] = r[1];
        b->pos[3] = 1.0f;
        for (j = 0; j < 10; j++) {
            used[j] = 0;
        }
        for (j = 0; j < 4; j++) {
            k = func_003800D8(10);
            if (used[k] != 0) {
                j--;
            } else {
                used[k] = 1;
                e = &sub[j];
                e->part = parts[k];
                e->unk0 = func_003D80E8(Fx_RandFloat() * 6.28318548f);
                e->unk4 = func_003D80E8(Fx_RandFloat() * 6.28318548f);
                e->unk10 = Fx_RandFloat() * 5000.0f;
                e->unk14 = Fx_RandFloat() * 5000.0f;
                e->unk8 = Fx_RandFloat() * -0.00174532901f + 0.00523598772f;
                e->unkC = Fx_RandFloat() * -0.00174532901f + 0.00523598772f;
            }
        }
    }
    a = func_003D8128();
    D_00444CD0.fl.target = &o[11];
    D_00444CD0.fl.unk7D0 = 3.14159274f - a * 6.28318548f;
    D_00444CD0.fl.unk7D4 = func_003ABCD0();
    o[11].unkC = 0;
}

/* Moves the flock: each bird flies along Z at its speed and restarts from a new random place past its range, turns
 * its four parts at random and draws them (faded in and out near the ends of the range); then the leader (part 11)
 * circles the origin and is drawn [I]. */
void func_003D4B18(StgObj *o) {
    Vu0Matrix m;
    Vu0Matrix ml;
    f32 *t;
    s32 n = D_0041580C[0] % 4096;
    f32 clock = n;
    StgObj *l = &o[11];
    Unk444D20 *b;
    f32 *r;
    struct Unk44B1C0 *rg;
    struct Unk444D20Sub *sub;
    struct Unk444D20Sub *e;
    StgObj *p;
    f32 one;
    f32 fade;
    s32 i;
    s32 j;
    Unk444D20 *birds = D_00444D20;

    func_00356710(m);
    for (i = 0; i < 10; i++) {
        b = &birds[i];
        r = b->lim.v;
        if (func_003D2110() == 0) {
            b->pos[2] += b->speed;
        }
        one = 1.0f;
        b->pos[3] = one;
        if (r[6] < b->pos[2]) {
            rg = b->range;
            r[1] = r[5] = func_003D47A8(rg->v[1], rg->v[5]);
            b->pos[0] = b->lim.v[0] = r[4] = func_003D47A8(rg->v[0], rg->v[4]);
            b->pos[1] = r[1];
            b->pos[2] = r[2];
            b->pos[3] = one;
        }
        *(u128 *)o->pos = *(u128 *)b->pos;
        fade = one;
        o->rot[1] = func_003D80E8(clock * (1.0f / 2048.0f) * 3.14159274f - func_00366EC8());
        if (fabsf(b->pos[2] - r[6]) < 10000.0f) {
            fade = fabsf(b->pos[2] - r[6]) / 10000.0f;
        }
        if (fabsf(b->pos[2] - r[2]) < 10000.0f) {
            fade = fabsf(b->pos[2] - r[2]) / 10000.0f;
        }
        sub = b->sub;
        for (j = 0; j < 4; j++) {
            e = &sub[j];
            p = e->part;
            if (func_003D2110() == 0) {
                e->unk0 += e->unk8 * Fx_RandFloat();
                e->unk4 += e->unkC * Fx_RandFloat();
            }
            e->unk0 = func_003D80E8(e->unk0);
            e->unk4 = func_003D80E8(e->unk4);
            p->pos[0] = e->unk10 * sinf(e->unk0);
            p->pos[1] = e->unk14 * sinf(e->unk4);
            p->pos[2] = 0.0f;
            p->pos[3] = one;
            o->unk18[j] = p;
        }
        func_003D1160(o, m, fade);
    }
    l->rot[1] = -atan2f(l->pos[2], l->pos[0]) + 1.57079637f;
    l->rot[1] = func_003D80E8(l->rot[1]);
    if (func_003D2110() == 0) {
        D_00444CD0.fl.unk7D0 += 0.00174532935f;
        D_00444CD0.fl.unk7D0 = func_003D80E8(D_00444CD0.fl.unk7D0);
        l->pos[0] = cosf(D_00444CD0.fl.unk7D0) * 100000.0f;
        l->pos[2] = sinf(D_00444CD0.fl.unk7D0) * 80000.0f;
    }
    l->rot[1] = -atan2f(l->pos[2], l->pos[0]);
    l->rot[1] = func_003D80E8(l->rot[1]);
    func_00356710(ml);
    t = ml[3];
    t[0] = -50000.0f;
    t[1] = D_005F6798[0];
    t[2] = 65000.0f;
    t[3] = 1.0f;
    if (D_00444CD0.fl.unk7D4 == 0) {
        func_003D1160(l, ml, 1.0f);
    } else {
        func_00375930(0, 0);
    }
}

/* Turns the object about Y by a constant step [I]. */
void func_003D4ED8(StgObj *o) {
    o->rot[1] += 1.02974427f;
    o->rot[1] = func_003D80E8(o->rot[1]);
}

/* Turns the object about X by a constant step [I]. */
void func_003D4F18(StgObj *o) {
    o->rot[0] += 1.02974427f;
    o->rot[0] = func_003D80E8(o->rot[0]);
}

INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521B80);
/* Sets the stereo volume of the object's sound from its distance to the origin and its direction in view space [I]. */
void func_003D4F58(StgObj *o) {
    Vu0Matrix root;
    Vu0Matrix m;
    V4q ear = D_00521B80;
    FVec4 v;
    f32 ip;
    f32 d;
    f32 t;
    f32 vol = 2048.0f;
    f32 c;
    f32 l;
    f32 r;

    Model_GetMatrix(root);
    func_00356710(m);
    sceVu0RotMatrix(m, m, o->rot);
    sceVu0TransMatrix(m, m, o->pos);
    sceVu0MulMatrix(m, root, m);
    sceVu0ApplyMatrix(v, D_00433400, m[3]);
    func_0034A868(v, v);
    ip = sceVu0InnerProduct(ear.v, v);
    d = SqrtS(m[3][0] * m[3][0] + m[3][2] * m[3][2]); /* fake: SqrtS */
    t = 1.0f - d / 150000.0f;
    if (t < 0.2f) {
        t = 0.2f;
    } else if (1.0f < t) {
        t = 1.0f;
    }
    vol *= t;
    t = (d - 10000.0f) / 150000.0f;
    if (t < 0.0f) {
        t = 0.0f;
    } else if (1.0f < t) {
        t = 1.0f;
    }
    c = ip + 1.0f;
    c += (1.0f - t) * 1.2f + 0.8f;
    if (2.0f < c) {
        c = 2.0f;
    }
    vol *= c * 0.5f;
    if (v[0] < 0.0f) {
        ip = 1.0f - fabsf(ip);
        l = ip * t * 0.6f + 1.0f;
        r = 2.0f - l;
    } else {
        ip = 1.0f - fabsf(ip);
        r = ip * t * 0.6f + 1.0f;
        l = 2.0f - r;
    }
    func_00375930((s32)(vol * l), (s32)(vol * r));
}

/* Turns the object about Y to face the work area's target object (D_00444D10), relative to that object's own turn [I]. */
void func_003D5190(StgObj *o) {
    Vu0Matrix m;
    Vu0Matrix mt;
    StgObj *t = D_00444D10[0];

    func_00356710(mt);
    sceVu0TransMatrix(mt, mt, t->pos);
    func_00356710(m);
    sceVu0TransMatrix(m, m, o->pos);
    sceVu0MulMatrix(m, mt, m);
    o->rot[1] = -atan2f(m[3][2], m[3][0]);
    o->rot[1] -= t->rot[1];
    o->rot[1] = func_003D80E8(o->rot[1]);
}

/* Puts the eight parts after the first into their matrix slots, in the order 1 5 2 6 3 7 4 8 [I]. */
void func_003D5238(StgObj *o) {
    StgObj *p[8];
    s32 i;

    p[0] = &o[1];
    p[1] = &o[5];
    p[2] = &o[2];
    p[3] = &o[6];
    p[4] = &o[3];
    p[5] = &o[7];
    p[6] = &o[4];
    p[7] = &o[8];
    for (i = 0; i < 8; i++) {
        p[i]->unk28->unk0 = i;
    }
}

#if 0
/* NON_MATCHING: 18 1.0f kept in two FPU registers (f20 for the calls, a copy in f21 for the loop clamp) */
/* Resets parts 0, 13 and 14 to the unit matrix, then hands every scene entry nearer than 10000 that names one of
 * the parts 15-18 (slots 5, 6, 9, 10) to Model_QueueDraw with a strength that falls off with the distance [I]. */
void func_003D52B0(StgObj *o) {
    Vu0Matrix m;
    Unk5F6780Ent e;
    StgObj *list[16];
    f32 one = 1.0f;
    f32 max = one;
    f32 f;
    f32 k;
    s32 i;
    s32 n;

    func_00356710(m);
    func_003D10D0(0, m, one);
    func_003D10D0(13, m, one);
    func_003D10D0(14, m, one);
    memset(list, 0, sizeof(list));
    list[5] = &o[15];
    list[6] = &o[16];
    list[9] = &o[17];
    list[10] = &o[18];
    for (i = 0; i < D_005F6780->count; i++) {
        e = D_005F6780->ent[D_005F6780->ref[i].index];
        if (D_005F6780->ref[i].dist < 10000.0f) {
            n = e.unk144;
            if (list[n] != NULL) {
                e.unk144 = 0;
                e.unk148 = 0;
                e.unk140 = list[n]->unk34;
                k = 1000.0f;
                f = ((9000.0f - D_005F6780->ref[i].dist) + k) / k;
                f = f * f;
                if (max < f) {
                    f = max;
                } else if (f < 0.0f) {
                    f = 0.0f;
                } else {
                    f *= 0.5f;
                }
                Model_QueueDraw(&e, list[n]->unk38, 4, list[n]->unk10, f, D_005F6780->ref[i].dist);
            }
        }
    }
}
#else
LIT4_WORD(D_005F5428, 0x461C4000);
LIT4_WORD(D_005F542C, 0x460CA000);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D52B0);
#endif

/* Turns the object about Y by a constant step [I]. */
void func_003D54F8(StgObj *o) {
    o->rot[1] += 0.00436332356f;
    o->rot[1] = func_003D80E8(o->rot[1]);
}

/* Bobs the object up and down (sine of its phase, 300 units) and builds its matrix under its parent [I]. */
void func_003D5538(StgObj *o) {
    Vu0Matrix m;
    Vu0Matrix ml;
    f32 *w = (f32 *)o->unk30;
    StgObj *p;
    s32 slot;

    w[0] += 0.03f;
    w[0] = func_003D80E8(w[0]);
    o->pos[1] = sinf(w[0]) * 300.0f;
    p = o->parent;
    slot = o->unk28->unk0;
    func_00356710(m);
    sceVu0RotMatrix(m, m, p->rot);
    sceVu0TransMatrix(m, m, p->pos);
    func_00356710(ml);
    sceVu0RotMatrix(ml, ml, o->rot);
    sceVu0TransMatrix(ml, ml, o->pos);
    sceVu0MulMatrix(D_00444CD0.m[slot], m, ml);
}

/* Clears the object's first work value. */
void func_003D5620(StgObj *o) {
    o->unk30->unk0 = 0.0f;
}

/* Sets the object's first work value [I]. */
void func_003D5630(StgObj *o) {
    o->unk30->unk0 = -3.14159274f;
}

/* Turns the object about Y with the game clock (one turn every 4096 frames), against the parent and the camera [I]. */
void func_003D5640(StgObj *o) {
    s32 t = D_0041580C[0];
    StgObj *p;
    f32 c;
    f32 a;

    t %= 4096;
    p = o->parent;
    c = p->rot[1] + func_00366EC8();
    a = (f32)t * (1.0f / 2048.0f);
    o->rot[1] = a * 3.14159274f - c;
    o->rot[1] = func_003D80E8(o->rot[1]);
}

/* Copies matrix slot i of the work area into m. */
void func_003D56D8(s32 i, Vu0Matrix m) {
    sceVu0CopyMatrix(m, D_00444CD0.m[i]);
}

INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521B90);
/* Places the object at a random direction about Y (the .rodata offset D_00521B90 turned by 5-10 degrees, Z mirrored)
 * and sets the work area's timer to 1800-2399 frames [I]. */
void func_003D56F0(StgObj *o) {
    V4q v = D_00521B90;
    Vu0Matrix m;

    func_00356710(m);
    sceVu0RotMatrixY(m, m, Fx_RandFloat() * 0.0872664675f + 0.0872664675f);
    sceVu0ApplyMatrix(v.v, m, v.v);
    v.v[2] = -v.v[2];
    func_003D1B48(o, v.v);
    D_00444CD0.i[0] = func_003800D8(600) + 1800;
}

INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521BA0);
/* Moves the object round the origin (a step along its heading, turned by D_00521BA0), faces it along its orbit, draws
 * it, and every 1800-2399 frames puts it back at a random direction (as func_003D56F0) [I]. */
void func_003D5798(StgObj *o) {
    V4q v = D_00521BA0;
    Vu0Matrix m;
    V4q r;
    Vu0Matrix mr;
    f32 one;
    f32 *t;

    o->rot[1] = -atan2f(o->pos[2], o->pos[0]) + 1.57079637f;
    o->rot[1] = func_003D80E8(o->rot[1]);
    if (func_003D2110() == 0) {
        func_00356710(m);
        m[0][0] = cosf(o->rot[1]);
        m[0][2] = sinf(o->rot[1] + 3.14159274f);
        sceVu0ApplyMatrix(v.v, m, v.v);
        sceVu0AddVector(o->pos, o->pos, v.v);
    }
    one = 1.0f;
    o->rot[1] = -atan2f(o->pos[2], o->pos[0]);
    o->rot[1] = func_003D80E8(o->rot[1]);
    func_00356710(m);
    t = m[3];
    t[0] = 140000.0f;
    t[1] = D_005F67B4[0];
    t[2] = 106000.0f;
    t[3] = one;
    if (func_0035A5A8() == 0) {
        func_003D1160(o, m, one);
    } else {
        func_003758F0(0, 0);
    }
    if (func_003D2110() == 0) {
        if (--D_00444CD0.i[0] <= 0) {
            r = D_00521B90;
            func_00356710(mr);
            sceVu0RotMatrixY(mr, mr, Fx_RandFloat() * 0.0872664675f + 0.0872664675f);
            sceVu0ApplyMatrix(r.v, mr, r.v);
            r.v[2] = -r.v[2];
            func_003D1B48(o, r.v);
            D_00444CD0.i[0] = func_003800D8(600) + 1800;
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521BB0);
/* Sets the stereo volume of the object's second sound from its direction and its distance in view space [I]. */
void func_003D5998(StgObj *o) {
    Vu0Matrix root;
    Vu0Matrix m;
    V4q ear = D_00521BB0;
    FVec4 v;
    f32 ip;
    f32 vol = 2048.0f;
    f32 c;
    f32 l;
    f32 r;
    f32 d;

    Model_GetMatrix(root);
    func_00356710(m);
    sceVu0RotMatrix(m, m, o->rot);
    sceVu0TransMatrix(m, m, o->pos);
    sceVu0MulMatrix(m, root, m);
    sceVu0ApplyMatrix(v, D_00433400, m[3]);
    func_0034A868(v, v);
    ip = sceVu0InnerProduct(ear.v, v);
    c = ip + 1.0f;
    c += 0.8f;
    if (2.0f < c) {
        c = 2.0f;
    }
    vol = c * 0.5f * vol;
    d = SqrtS(m[3][0] * m[3][0] + m[3][2] * m[3][2]); /* fake: SqrtS */
    vol *= 1.0f - (d - 20000.0f) / 100000.0f;
    if (vol < 0.0f) {
        vol = 0.0f;
    }
    if (v[0] < 0.0f) {
        ip = 1.0f - fabsf(ip);
        l = ip * 0.6f + 1.0f;
        r = 2.0f - l;
    } else {
        ip = 1.0f - fabsf(ip);
        r = ip * 0.6f + 1.0f;
        l = 2.0f - r;
    }
    func_003758F0((s32)(vol * l), (s32)(vol * r));
}

INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521BC0);
INCLUDE_RODATA("asm/nonmatchings/model_anim_b", jtbl_00521BD0);
#if 0
/* NON_MATCHING: 69 loop 3 addresses: compiler hoists &row[0].item (wk + 12) as an invariant, original recomputes row + 12 */
/* Sets up the lanes stage: 40 parts copied from templates 0, 8 and 9, ten lanes with a position and a random speed
 * (either way), and each part put at a random free place of a random lane (4000 apart) [I]. */
void func_003D5B58(StgObj *o) {
    V4q v = D_00521BC0;
    struct Unk444CD0Rw *wk = &D_00444CD0.rw;
    StgObj **list = wk->list;
    struct Unk444CD0Row *r;
    StgObj *n;
    s32 i;
    s32 j;
    s32 k;
    s32 ok;
    f32 z;

    for (i = 0; i < 40; i++) {
        n = func_003D1A80();
        switch (i & 7) {
            case 7:
                func_003D1B68(n, &o[8]);
                break;
            case 0:
            case 1:
            case 2:
            case 3:
                func_003D1B68(n, o);
                break;
            case 4:
            case 5:
            case 6:
            default:
                func_003D1B68(n, &o[9]);
                break;
        }
        func_003D1B48(n, v.v);
        list[i] = n;
    }
    for (i = 0; i < 10; i++) {
        r = &wk->row[i];
        r->unk4 = (f32)i * 4000.0f + -20000.0f + 2500.0f;
        r->speed = (func_003D8128() + 1.0f) * 15.0f;
        if (func_003800D8(255) & 1) {
            r->speed = -r->speed;
        }
        r->count = 0;
    }
    for (i = 0; i < 40; i++) {
        ok = 1;
        k = func_003800D8(10);
        if (wk->row[k].count >= 20) {
            i--;
        } else {
            list[i]->pos[2] = 20000.0f - func_003D8128() * 5000.0f;
            for (j = 0; j < wk->row[k].count; j++) {
                z = list[i]->pos[2];
                if (fabsf(wk->row[k].item[j]->pos[2] - z) < 4000.0f) {
                    ok = 0;
                    break;
                }
                if (fabsf(20000.0f - wk->row[k].item[j]->pos[2]) < 4000.0f && fabsf(20000.0f - z) < 4000.0f) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                wk->row[k].item[wk->row[k].count++] = list[i];
                if (wk->row[k].speed < 0.0f) {
                    list[i]->rot[1] = 3.14159274f;
                }
            } else {
                i--;
            }
        }
    }
}
#else
LIT4_WORD(D_005F546C, 0xC69C4000);
LIT4_WORD(D_005F5470, 0x451C4000);
LIT4_WORD(D_005F5474, 0x471C4000);
LIT4_WORD(D_005F5478, 0x469C4000);
LIT4_WORD(D_005F547C, 0x469C4000);
LIT4_WORD(D_005F5480, 0x40490FDB);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D5B58);
#endif

#if 0
/* NON_MATCHING: 81 same hoisting of wk + 4 / wk + 12 out of the lane loops */
/* Moves the lanes stage: each lane and part wraps round the camera within 20000 along X / Z, the parts move along Z at
 * their lane's speed and are drawn; then the scene entries nearer than 11000 that name parts 4-7 are handed to
 * Model_QueueDraw [I]. */
void func_003D5E80(StgObj *o) {
    Vu0Matrix m;
    Unk5F6780Ent e;
    StgObj *list[4];
    StgObj *p4 = &o[4];
    StgObj *p5 = &o[5];
    StgObj *p6 = &o[6];
    StgObj *p7 = &o[7];
    struct Unk444CD0Rw *wk = &D_00444CD0.rw;
    StgObj *p;
    f32 cx;
    f32 cz;
    f32 f;
    s32 i;
    s32 j;
    s32 n;

    func_00366EB8(m);
    cx = m[3][0];
    cz = m[3][2];
    for (i = 0; i < 10; i++) {
        if (20000.0f < wk->row[i].unk4 - cx) {
            wk->row[i].unk4 -= 40000.0f;
        } else if (wk->row[i].unk4 - cx < -20000.0f) {
            wk->row[i].unk4 += 40000.0f;
        }
        for (j = 0; j < wk->row[i].count; j++) {
            p = wk->row[i].item[j];
            if (func_003D2110() == 0) {
                p->pos[2] += wk->row[i].speed;
            }
            if (20000.0f < p->pos[2] - cz) {
                p->pos[2] -= 40000.0f;
            } else if (p->pos[2] - cz < -20000.0f) {
                p->pos[2] += 40000.0f;
            }
            p->pos[0] = wk->row[i].unk4;
            func_00356710(m);
            func_003D1160(p, m, 1.0f);
        }
    }
    func_00356710(m);
    func_003D10D0(1, m, 1.0f);
    list[0] = p4;
    list[1] = p5;
    list[2] = p6;
    list[3] = p7;
    for (i = 0; i < D_005F6780->count; i++) {
        e = D_005F6780->ent[D_005F6780->ref[i].index];
        if (D_005F6780->ref[i].dist < 11000.0f) {
            n = e.unk144;
            e.unk144 = 0;
            e.unk148 = 0;
            e.unk140 = list[n]->unk34;
            f = ((7000.0f - D_005F6780->ref[i].dist) + 3500.0f) / 3500.0f;
            f = f * f;
            if (1.0f < f) {
                f = 1.0f;
            } else if (f < 0.0f) {
                f = 0.0f;
            }
            Model_QueueDraw(&e, list[n]->unk38, 4, list[n]->unk10, f, D_005F6780->ref[i].dist);
        }
    }
}
#else
LIT4_WORD(D_005F5484, 0x469C4000);
LIT4_WORD(D_005F5488, 0x471C4000);
LIT4_WORD(D_005F548C, 0xC69C4000);
LIT4_WORD(D_005F5490, 0x462BE000);
LIT4_WORD(D_005F5494, 0x45DAC000);
LIT4_WORD(D_005F5498, 0x455AC000);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D5E80);
#endif

/* Initialises the bobbing object: its float state from the node, a random heading around pi [I]. */
void func_003D6208(StgObj *o) {
    StgObjF *f = o->unk30;
    StgObjI *s = o->unk28;

    f->unk0 = 2.0f;
    f->unk4 = 0.00048828125f;
    f->unk8 = 0.00048828125f;
    s->unk0 = 0;
    s->unk4 = 0;
    s->unk8 = 0;
    s->unkC = 0;
    f->unk1C = 0.0f;
    f->unk28 = o->pos[1];
    f->unk3C = o->rot[0];
    f->unk44 = o->rot[2];
    o->rot[1] = 0.5235988f - func_003D8128() * 1.0471976f + 3.1415927f;
    o->unk8 = 1;
}

INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521BF0);
#if 0
/* NON_MATCHING: 159 draft: case layout (0,2,3,1) and visibility flag shape found; register allocation, the divisions by /450 /330 /60 /120 /20000 and the dead off-screen test remain (fdiff lines with the file built without -ffast-math) */
/* Floating object: bobs while on screen, rises and turns away (phase 2), sinks back (phase 3); its sound follows the
 * light-relative facing and height (func_00375930 volume pair) [I]. */
void func_003D62A0(StgObj *o) {
    Vu0Matrix w;
    Vu0Matrix m;
    FVec4 p;
    Vu0Matrix w2;
    Vu0Matrix m2;
    V4 l;
    FVec4 q;
    StgObjI *s = o->unk28;
    StgObjF *f = o->unk30;
    s32 off = 0;
    s32 x;
    s32 y;
    f32 t;
    f32 a;
    f32 b;
    f32 ip;
    f32 vol;
    f32 k;

    Model_GetMatrix(w);
    func_00356710(m);
    sceVu0RotMatrix(m, m, o->rot);
    sceVu0TransMatrix(m, m, o->pos);
    sceVu0MulMatrix(m, w, m);
    sceVu0ApplyMatrix(p, D_004334C0, m[3]);
    sceVu0DivVectorXYZ(p, p, p[3]);
    if (!(p[2] < 0.0f)) {
        x = p[0];
        y = p[1];
        off = x < 0x6C0 || x > 0x940 || y < 0x720 || y > 0x8E0;
        off = 1; /* [I] the original sets the flag whatever the test gives */
    }
    switch (s->unkC) {
        case 0:
            if (off != 0) {
                s->unk10++;
            }
            f->unkC += 0.031634092f;
            f->unk10 += 0.031634092f;
            f->unkC = func_003D80E8(f->unkC);
            f->unk10 = func_003D80E8(f->unk10);
            o->pos[1] = f->unk28 + sinf(f->unkC) * 100.0f;
            if (s->unk10 >= 600 && ((gMode == 4 && D_005F5CBC == 0) || gMode == 0x11)) {
                s->unk10 = 0;
                s->unkC = 2;
                f->unk1C = 0.0f;
                f->unk28 = o->pos[1];
                f->unk48 = 0.2617994f;
                f->unk3C = o->rot[0];
                f->unk4C = o->rot[1] + (0.5235988f - func_003D8128() * 1.0471976f);
                if (func_003800D8(0xFF) & 1) {
                    f->unk50 = 1.0f;
                } else {
                    f->unk50 = -1.0f;
                }
            }
            break;
        case 2:
            s->unk10++;
            t = (f32)s->unk10 / 450.0f;
            f->unk1C += t * 20.0f;
            o->pos[1] = f->unk28 + f->unk1C;
            o->rot[1] += f->unk50 * 0.0034906587f * t;
            if (s->unk10 < 330) {
                a = f->unk48 - f->unk3C;
                o->rot[0] = f->unk3C + a * ((cosf(s->unk10 * 3.1415927f / 330.0f + 3.1415927f) + 1.0f) * 0.5f);
            }
            if (o->pos[1] > 50000.0f) {
                k = 3.1415927f;
                s->unkC = 3;
                f->unk1C = 0.0f;
                f->unk28 = o->pos[1];
                f->unk48 = 0.0f;
                f->unk3C = o->rot[0];
                o->rot[1] = 0.5235988f - func_003D8128() * 1.0471976f + k;
                o->rot[1] = 0.87266463f - func_003D8128() * 1.7453293f + k;
                o->pos[0] = 2000.0f - func_003D8128() * 4000.0f;
                o->pos[2] = func_003D8128() * 5000.0f + 14000.0f;
                if (func_003800D8(0xFF) & 1) {
                    f->unk50 = 1.0f;
                } else {
                    f->unk50 = -1.0f;
                }
            }
            break;
        case 3:
            s->unk10--;
            t = (f32)s->unk10 / 450.0f;
            f->unk1C -= t * 20.0f;
            o->pos[1] = f->unk28 + f->unk1C;
            if (s->unk10 < 330) {
                k = 330.0f;
                a = f->unk48 - f->unk3C;
                o->rot[0] = f->unk3C + a * (1.0f - (cosf(s->unk10 * 3.1415927f / k + 3.1415927f) + 1.0f) * 0.5f);
                o->rot[1] += f->unk50 * 0.0005235988f * (s->unk10 / k);
            }
            if (s->unk10 < 0) {
                f->unkC = 0.0f;
                f->unk10 = 0.0f;
                f->unk1C = 0.0f;
                f->unk28 = o->pos[1];
                f->unk3C = o->rot[0];
                f->unk44 = o->rot[2];
                s->unkC = 1;
                s->unk14 = 0;
                s->unk10 = 0;
            }
            break;
        case 1:
            if (s->unk14 == 0) {
                o->pos[1] -= 2.0f;
                if (o->pos[1] < 0.0f) {
                    o->pos[1] = 0.0f;
                    s->unk14 = 1;
                }
                break;
            }
            if (s->unk10 < 120) {
                k = 3.1415927f;
                t = func_003D80E8(s->unk10++ * k / 60.0f + k);
                b = (f32)s->unk10 / 120.0f;
                a = sinf(t);
                b *= b;
                if (t > 6.2831855f) {
                    a *= 1.0f - (t - 6.2831855f) / k;
                }
                o->pos[1] = b * 30.0f * a;
            } else if (off != 0) {
                s->unk10++;
            }
            if (s->unk10 >= 600 && ((gMode == 4 && D_005F5CBC == 0) || gMode == 0x11)) {
                s->unk10 = 0;
                s->unkC = 2;
                f->unk1C = 0.0f;
                f->unk28 = o->pos[1];
                f->unk48 = 0.2617994f;
                f->unk3C = o->rot[0];
                f->unk4C = o->rot[1] + (0.5235988f - func_003D8128() * 1.0471976f);
                if (func_003800D8(0xFF) & 1) {
                    f->unk50 = 1.0f;
                } else {
                    f->unk50 = -1.0f;
                }
            }
            break;
    }
    vol = 2048.0f;
    o->rot[0] = func_003D80E8(o->rot[0]);
    o->rot[1] = func_003D80E8(o->rot[1]);
    o->rot[2] = func_003D80E8(o->rot[2]);
    l = D_00521BF0;
    Model_GetMatrix(w2);
    func_00356710(m2);
    sceVu0RotMatrix(m2, m2, o->rot);
    sceVu0TransMatrix(m2, m2, o->pos);
    sceVu0MulMatrix(m2, w2, m2);
    sceVu0ApplyMatrix(q, D_00433400, m2[3]);
    func_0034A868(q, q);
    ip = sceVu0InnerProduct(l.v, q);
    a = ip + 1.0f + 0.8f;
    if (a > 2.0f) {
        a = 2.0f;
    }
    vol = a * 0.5f * vol * (1.0f - (o->pos[1] - 2000.0f) / 20000.0f);
    if (vol < 0.0f) {
        vol = 0.0f;
    }
    if (q[0] < 0.0f) {
        a = (1.0f - fabsf(ip)) * 0.6f + 1.0f;
        b = 2.0f - a;
    } else {
        b = (1.0f - fabsf(ip)) * 0.6f + 1.0f;
        a = 2.0f - b;
    }
    func_00375930(vol * a, vol * b);
}
#else
LIT4_WORD(D_005F54A8, 0x3D0192C0);
LIT4_WORD(D_005F54AC, 0x3E860A92);
LIT4_WORD(D_005F54B0, 0x3F860A92);
LIT4_WORD(D_005F54B4, 0x3F060A92);
LIT4_WORD(D_005F54B8, 0x3B64C389);
LIT4_WORD(D_005F54BC, 0x40490FDB);
LIT4_WORD(D_005F54C0, 0x47435000);
LIT4_WORD(D_005F54C4, 0x40490FDB);
LIT4_WORD(D_005F54C8, 0x3F860A92);
LIT4_WORD(D_005F54CC, 0x3F060A92);
LIT4_WORD(D_005F54D0, 0x3FDF66F3);
LIT4_WORD(D_005F54D4, 0x3F5F66F3);
LIT4_WORD(D_005F54D8, 0x459C4000);
LIT4_WORD(D_005F54DC, 0x465AC000);
LIT4_WORD(D_005F54E0, 0x40490FDB);
LIT4_WORD(D_005F54E4, 0x3A09421F);
LIT4_WORD(D_005F54E8, 0x40490FDB);
LIT4_WORD(D_005F54EC, 0x40C90FDB);
LIT4_WORD(D_005F54F0, 0x3E860A92);
LIT4_WORD(D_005F54F4, 0x3F860A92);
LIT4_WORD(D_005F54F8, 0x3F060A92);
LIT4_WORD(D_005F54FC, 0x3F4CCCCD);
LIT4_WORD(D_005F5500, 0x469C4000);
LIT4_WORD(D_005F5504, 0x3F19999A);
LIT4_WORD(D_005F5508, 0x3F19999A);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D62A0);
#endif

/* Turns the node about y by 59 degrees per frame, wrapped to (-pi, pi]. */
void func_003D6B50(StgObj *o) {
    o->rot[1] += 1.0297443f;
    if (o->rot[1] > 3.1415927f) {
        o->rot[1] -= 6.2831855f;
    }
}

/* Turns the node about x by 59 degrees per frame, wrapped to (-pi, pi]. */
void func_003D6B88(StgObj *o) {
    o->rot[0] += 1.0297443f;
    if (o->rot[0] > 3.1415927f) {
        o->rot[0] -= 6.2831855f;
    }
}

INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521C00);
#if 0
/* NON_MATCHING: 113 node loop matches; prologue spill order of the five node pointers and the divided-difference loop nest (hoisted i-1 / i+1, n=3 then n=7 in one register) differ */
/* Builds the 40 ring nodes from four templates at random places, the Hermite pitch curve, and starts nodes 14-18 [I]. */
void func_003D6BC0(StgObj *tpl) {
    Vu0Matrix m;
    V4 v;
    StgObj *t[5];
    StgObj **obj = D_00444CD0.path.obj;
    f32 *knot = D_00444CD0.path.curve.knot;
    f32 (*dd)[20] = D_00444CD0.path.curve.dd;
    StgObj *o;
    StgObj *src;
    StgObjI *s;
    f32 r;
    s32 n;
    s32 i;
    s32 j;

    t[0] = &tpl[14];
    t[1] = &tpl[15];
    t[2] = &tpl[16];
    t[3] = &tpl[17];
    t[4] = &tpl[18];
    for (i = 0; i < 40; i++) {
        o = func_003D1A80();
        switch (i % 4) {
            case 0:
                src = tpl;
                break;
            case 1:
                src = &tpl[1];
                break;
            case 2:
                src = &tpl[2];
                break;
            case 3:
                src = &tpl[3];
                break;
            default:
                src = tpl;
                break;
        }
        func_003D1B68(o, src);
        v = D_00521C00;
        v.v[0] *= func_003D8128();
        r = 3.1415927f - func_003D8128() * 6.2831855f;
        func_00356710(m);
        sceVu0RotMatrixY(m, m, r);
        sceVu0ApplyMatrix(o->pos, m, v.v);
        o->rot[1] = 3.1415927f - func_003D8128() * 6.2831855f;
        obj[i] = o;
    }
    n = 3;
    for (i = 0; i <= n; i++) {
        knot[i] = D_0044C170[i];
        dd[0][i] = D_0044C180[i];
        dd[1][i * 2] = D_0044C190[i];
    }
    for (i = 0; i < 20; i++) {
        D_00445408[i] = knot[i];
    }
    for (i = 0; i < 3; i++) {
        dd[1][i * 2 + 1] = (dd[0][i + 1] - dd[0][i]) / (knot[i + 1] - knot[i]);
    }
    for (i = 0; i <= n; i++) {
        knot[(n - i) * 2] = knot[n - i];
        knot[(n - i) * 2 + 1] = knot[n - i];
    }
    n = 7;
    for (i = 1; i < n; i++) {
        for (j = i; j < n; j++) {
            dd[i + 1][j - i] = (dd[i][j - (i - 1)] - dd[i][j - i]) / (knot[j + 1] - knot[j - i]);
        }
    }
    o = t[0];
    o->unk30->unk0 = 0.0f;
    s = o->unk28;
    s->unk4 = 1;
    s->unk0 = 0;
    o->unk8 = 1;
    o = t[1];
    o->unk30->unk0 = 0.0f;
    s = o->unk28;
    s->unk0 = 1;
    s->unk4 = 1;
    o->unk8 = 1;
    o = t[2];
    o->unk30->unk0 = 0.0f;
    s = o->unk28;
    s->unk0 = 2;
    s->unk4 = 0;
    o->unk8 = 1;
    o = t[3];
    o->unk30->unk0 = 0.0f;
    s = o->unk28;
    s->unk0 = 3;
    s->unk4 = 1;
    o->unk8 = 1;
    o = t[4];
    o->unk30->unk0 = 0.0f;
    s = o->unk28;
    s->unk0 = 4;
    s->unk4 = 1;
    o->unk8 = 1;
}
#else
LIT4_WORD(D_005F5524, 0x40C90FDB);
LIT4_WORD(D_005F5528, 0x40490FDB);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D6BC0);
#endif

/* Draws the 40 nodes around the fighters' centre, their planar shadows along the light, then fixed parts 4-18 [I]. */
void func_003D6FD8(void) {
    Vu0Matrix m;
    FVec4 l;
    Vu0Matrix n;
    Vu0Matrix sh;
    StgObj **obj = D_00444CD0.path.obj;
    f32 s0;
    f32 s1;
    f32 s2;
    f32 s3;
    f32 one;
    s32 i;

    if (gMode == 0x11) {
        D_00444CD0.path.x = gFighters[0].unk51C;
        D_00444CD0.path.z = gFighters[0].unk524;
    } else {
        D_00444CD0.path.x = (gFighters[0].unk51C + gFighters[1].unk51C) * 0.5f;
        D_00444CD0.path.z = (gFighters[0].unk524 + gFighters[1].unk524) * 0.5f;
    }
    D_00444CD0.path.z = -D_00444CD0.path.z;
    func_00356710(m);
    for (i = 0; i < 40; i++) {
        Model_ClearMatrixStack();
        Model_LoadIdentity();
        obj[i]->unk3C = 1.0f;
        func_003D1468(obj[i]);
    }
    func_00356710(sh);
    func_00353C88(0, l);
    sh[1][0] = -l[0] / l[1];
    sh[1][1] = 0.0f;
    sh[1][2] = -l[2] / l[1];
    for (i = 0; i < 40; i++) {
        func_00356710(n);
        sceVu0RotMatrix(n, n, obj[i]->rot);
        sceVu0TransMatrix(n, n, obj[i]->pos);
        sceVu0MulMatrix(n, sh, n);
        func_003D10D0(0x13, n, 1.0f);
    }
    one = 1.0f;
    func_00356710(m);
    s0 = 0.89f;
    m[2][2] = s0;
    m[0][0] = s0;
    m[1][1] = 1.1f;
    func_003D10D0(4, m, one);
    s1 = 0.91f;
    m[2][2] = s1;
    m[0][0] = s1;
    func_003D10D0(5, m, one);
    s2 = 0.77f;
    m[2][2] = s2;
    m[0][0] = s2;
    func_003D10D0(6, m, one);
    s3 = 0.96f;
    m[2][2] = s3;
    m[0][0] = s3;
    func_003D10D0(7, m, one);
    m[2][2] = s3;
    m[0][0] = s3;
    func_003D10D0(8, m, one);
    m[2][2] = s0;
    m[0][0] = s0;
    func_003D10D0(9, m, one);
    m[2][2] = s1;
    m[0][0] = s1;
    func_003D10D0(10, m, one);
    m[2][2] = s2;
    m[0][0] = s2;
    func_003D10D0(11, m, one);
    m[2][2] = s3;
    m[0][0] = s3;
    func_003D10D0(12, m, one);
    m[2][2] = s3;
    m[0][0] = s3;
    func_003D10D0(13, m, one);
    func_00356710(m);
    func_003D10D0(14, m, one);
    func_003D10D0(15, m, one);
    func_003D10D0(16, m, one);
    func_003D10D0(17, m, one);
    func_003D10D0(18, m, one);
}

/* Clears the first word of the object's state block [I]. */
void func_003D7368(StgObj *o) {
    o->unk28->unk0 = 0;
}

INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521C10);
INCLUDE_RODATA("asm/nonmatchings/model_anim_b", D_00521C20);
#if 0
/* NON_MATCHING: 105 draft: structure and stack layout right; the kept div.s by 30 / 4000 / 800 (literal li in the loop) and case-0 branch shapes differ (the file is built without -ffast-math; both square roots are SqrtS) */
/* Debris node: blown off by the stage's blast points, tumbles and falls back, respawns near the centre when too far [I]. */
void func_003D7378(StgObj *o) {
    f32 *pos = o->pos;
    f32 *rot = o->rot;
    f32 cx = D_00444CD0.path.x;
    StgObjI *s = o->unk28;
    f32 cz = D_00444CD0.path.z;
    StgObjF *f = o->unk30;
    FVec4 v;
    Vu0Matrix m2;
    FVec4 d;
    V4 w;
    WindGust *e;
    f32 fps;
    f32 range;
    f32 top;
    f32 dx;
    f32 dz;
    f32 t;
    f32 a;
    f32 r;
    s32 k;
    s32 i;

    for (i = 0; i < 4; i++) {
        e = &D_0044B100[i];
        if (e->unk0 != 0) {
            k = func_003D7A40(i, pos[0], pos[1], pos[2]);
            top = 800.0f;
            if (k == 1 && pos[1] < top) {
                dx = pos[0] - e->unk20[0];
                dz = pos[2] - e->unk20[2];
                fps = 30.0f;
                t = e->unk10 / fps;
                range = 4000.0f;
                a = (range - SqrtS(dx * dx + dz * dz)) / range * ((top - pos[1]) / top);
                a = a * a;
                if (a >= 0.8f) {
                    a = 0.8f;
                }
                s->unk4 = 0;
                s->unk0 = k;
                s->unk8 = t * 60.0f * a;
                sceVu0SubVector(d, pos, e->unk20);
                d[1] = 0.0f;
                d[3] = 1.0f;
                func_0034A868(d, d);
                sceVu0ScaleVectorXYZ(d, d, t * 100.0f * a);
                f->unk0 = d[0];
                f->unk4 = d[2];
            }
        }
    }
    switch (s->unk0) {
        case 0:
            *(V4 *)d = D_00521C10;
            if (rot[0] < 0.0f ? rot[0] < -1.5707964f : rot[0] <= 1.5707964f) {
                d[0] = -d[0];
            }
            if (rot[2] < 0.0f ? rot[2] < -1.5707964f : rot[2] <= 1.5707964f) {
                d[2] = -d[2];
            }
            sceVu0AddVector(rot, rot, d);
            if (pos[1] > 0.0f) {
                pos[1] -= 2.5f;
            } else if (pos[1] < 0.0f) {
                pos[1] = 0.0f;
            }
            rot[0] = func_003D80E8(rot[0]);
            rot[2] = func_003D80E8(rot[2]);
            break;
        case 1:
            Wind_Sample(pos[0], pos[1], pos[2], v);
            s->unk4++;
            if (s->unk4 < s->unk8) {
                r = ((f32)s->unk8 - (f32)s->unk4) / (f32)s->unk8;
                pos[0] += f->unk0 * r;
                pos[1] += r * 50.0f;
                pos[2] += f->unk4 * r;
                sceVu0ScaleVector(v, v, 20.0f);
                sceVu0AddVector(pos, pos, v);
                rot[0] += func_003D8128() * 0.15707964f;
                rot[1] += func_003D8128() * 0.20943952f;
                rot[2] += func_003D8128() * 0.2617994f;
                rot[0] = func_003D80E8(rot[0]);
                rot[1] = func_003D80E8(rot[1]);
                rot[2] = func_003D80E8(rot[2]);
            } else {
                Vu0Matrix m;
                f32 ax;
                f32 az;
                f32 fx;
                f32 fz;
                f32 g;
                f32 q;

                w = D_00521C20;
                rot[0] += func_003D8128() * 0.05235988f;
                rot[1] += func_003D8128() * 0.05235988f;
                rot[2] += func_003D8128() * 0.05235988f;
                rot[0] = func_003D80E8(rot[0]);
                rot[1] = func_003D80E8(rot[1]);
                rot[2] = func_003D80E8(rot[2]);
                ax = fabsf(rot[0]);
                if (ax > 1.5707964f) {
                    ax = 3.1415927f - ax;
                }
                q = 2.5132742f;
                fx = ax / q;
                fx *= fx;
                az = fabsf(rot[2]);
                if (az > 1.5707964f) {
                    az = 3.1415927f - az;
                }
                fz = az / q;
                fz *= fz;
                g = fx * fz;
                if (g < 0.008f) {
                    g = 0.008f;
                } else if (g > 0.05f) {
                    g = 0.05f;
                    fz += 0.01f;
                    fx += 0.01f;
                }
                w.v[0] *= fx;
                w.v[2] *= fz;
                w.v[1] = g * -500.0f;
                func_00356710(m);
                sceVu0RotMatrixY(m, m, rot[1]);
                sceVu0ApplyMatrix(w.v, m, w.v);
                sceVu0AddVector(pos, pos, w.v);
                sceVu0ScaleVector(v, v, 20.0f);
                sceVu0AddVector(pos, pos, v);
                if (pos[1] < 50.0f) {
                    s->unk0 = 0;
                }
            }
            break;
    }
    dx = cx - pos[0];
    dz = cz - pos[2];
    a = SqrtS(dx * dx + dz * dz); /* fake: SqrtS */
    if (a > 8000.0f) {
        FVec4 c;

        w = D_00521C00;
        c[0] = cx;
        c[2] = cz;
        c[3] = 1.0f;
        c[1] = 0.0f;
        r = 3.1415927f - func_003D8128() * 6.2831855f;
        func_00356710(m2);
        sceVu0RotMatrixY(m2, m2, r);
        sceVu0ApplyMatrix(pos, m2, w.v);
        sceVu0AddVector(pos, pos, c);
        s->unk0 = 0;
    }
}
#else
LIT4_WORD(D_005F5540, 0x3F4CCCCD);
LIT4_WORD(D_005F5544, 0xBFC90FDB);
LIT4_WORD(D_005F5548, 0x3FC90FDB);
LIT4_WORD(D_005F554C, 0xBFC90FDB);
LIT4_WORD(D_005F5550, 0x3FC90FDB);
LIT4_WORD(D_005F5554, 0x3E20D97C);
LIT4_WORD(D_005F5558, 0x3E567750);
LIT4_WORD(D_005F555C, 0x3E860A92);
LIT4_WORD(D_005F5560, 0x3D567750);
LIT4_WORD(D_005F5564, 0x3FC90FDB);
LIT4_WORD(D_005F5568, 0x40490FDB);
LIT4_WORD(D_005F556C, 0x4020D97C);
LIT4_WORD(D_005F5570, 0x40490FDB);
LIT4_WORD(D_005F5574, 0x3C03126F);
LIT4_WORD(D_005F5578, 0x3D4CCCCD);
LIT4_WORD(D_005F557C, 0x3C23D70A);
LIT4_WORD(D_005F5580, 0x40C90FDB);
LIT4_WORD(D_005F5584, 0x40490FDB);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D7378);
#endif

/* Returns entry i's value when (x, z) is within 4000 units of it, else 0 [I]. */
s32 func_003D7A40(s32 i, f32 x, f32 y, f32 z) {
    WindGust *e = &D_0044B100[i];
    f32 dx;
    f32 dz;
    f32 d;

    if (e->unk0 == 0) {
        return 0;
    }
    dx = x - e->unk20[0];
    dz = z - e->unk20[2];
    d = SqrtS(dx * dx + dz * dz); /* fake: SqrtS */
    if (d >= 4000.0f) {
        return 0;
    }
    return e->unk4;
}

/* Camera node: sets the lens of entry unk34, copies the base camera and moves it by the node's offset [I]. */
void func_003D7AB0(StgObj *o) {
    Vu0Matrix r;
    Vu0Matrix w;
    Vu0Matrix m;
    FVec4 v;

    func_003E13C8(D_00445620[o->unk34]);
    sceVu0CopyMatrix(D_004455A0[0], D_0046AF50[0]);
    sceVu0CopyMatrix(D_004455A0[1], D_0046AF50[1]);
    Model_GetMatrix(w);
    func_00356710(m);
    sceVu0RotMatrix(m, m, o->rot);
    sceVu0TransMatrix(m, m, o->pos);
    sceVu0MulMatrix(w, w, m);
    Wind_Sample(w[3][0], w[3][1], -w[3][2], v);
    sceVu0ScaleVectorXYZ(v, v, 5000.0f);
    func_00356710(r);
    sceVu0RotMatrixY(r, r, o->rot[1]);
    sceVu0ApplyMatrix(v, r, v);
    D_004455A0[0][0][0] += v[0];
    D_004455A0[0][0][2] += v[2];
    D_004455A0[0][1][0] += v[0];
    D_004455A0[0][1][2] += v[2];
    D_004455A0[1][0][0] += v[0];
    D_004455A0[1][0][2] += v[2];
    D_004455A0[1][1][0] += v[0];
    D_004455A0[1][1][2] += v[2];
}

/* Node on the Hermite curve: pitch from the curve at time t, places matrix unk0, advances t (looping) [I]. */
void func_003D7C38(StgObj *o) {
    Vu0Matrix m;
    f32 *knot = D_00444D78.knot;
    f32 (*dd)[20] = D_00444D78.dd;
    s32 n = 7;
    f32 *t = &o->unk30->unk0;
    StgObjI *s = o->unk28;
    f32 x = *t;
    s32 idx = s->unk0;
    s32 i;
    s32 j;
    f32 f;

    if (s->unk4 != 0) {
        f = dd[n][0];
        i = 0;
        while (i < n) {
            j = i + 1;
            f = f * (x - knot[n - j]) + dd[n - j][0];
            i = j;
        }
        o->rot[0] = f;
    }
    func_00356710(m);
    sceVu0RotMatrixX(m, m, o->rot[0]);
    sceVu0RotMatrixY(m, m, o->rot[1]);
    sceVu0RotMatrixZ(m, m, o->rot[2]);
    sceVu0TransMatrix(D_00445460[idx], m, o->pos);
    *t += 1.0f;
    if (*t > D_0044C170[3]) {
        *t = D_0044C170[0];
    }
}

/* Copies matrix i of the table at 0x445460 to m. */
void func_003D7D90(s32 i, Vu0Matrix m) {
    sceVu0CopyMatrix(m, D_00445460[i]);
}

/* Heading of the stage's first light as a 16-bit angle (0..65535) [I]; pos is not read. */
s32 func_003D7DA8(s32 *pos) {
    FVec4 l;
    f32 a;
    f32 tau;

    func_00353C88(0, l);
    a = func_003D8088(func_003D80E8(-atan2f(-l[0], -l[2])));
    tau = 6.2831855f;
    return a * 65535.0f / tau;
}

#if 0
/* NON_MATCHING: 9 second loop: original reloads f[i] into v1 and copies it to a0 (move) for unk3C = unk1A; no source shape tried reproduces it */
/* Turns both fighters' positions and facings about the origin so that they line up with the light [I]. */
void func_003D7E10(Fighter *a, Fighter *b, s32 n) {
    Vu0Matrix m;
    FVec4 v;
    FVec4 l;
    Fighter *f[2];
    f32 ang;
    f32 c;
    f32 tau;
    s32 dir;
    s32 i;

    f[0] = a;
    f[1] = b;
    func_00353C88(0, l);
    ang = func_003D80E8(-atan2f(-l[0], -l[2]));
    if (n != 0) {
        dir = f[0]->unk1A;
    } else {
        dir = (f[0]->unk1A + f[1]->unk1A) / 2;
    }
    c = 65535.0f;
    ang = func_003D80E8(ang - func_003D80E8(dir / c * 2.0f * 3.1415927f));
    func_00356710(m);
    sceVu0RotMatrixY(m, m, ang);
    for (i = 0; i < 2; i++) {
        v[0] = f[i]->unk4;
        v[1] = 0.0f;
        v[2] = f[i]->unkC;
        v[3] = 1.0f;
        sceVu0ApplyMatrix(v, m, v);
        f[i]->unk4 = v[0];
        f[i]->unkC = v[2];
        f[i]->unk51C = f[i]->unk4;
        f[i]->unk524 = f[i]->unkC;
        f[i]->unk708 = f[i]->unk4;
        f[i]->unk710 = f[i]->unkC;
        f[i]->unk6B8 = f[i]->unk4;
        f[i]->unk6C0 = f[i]->unkC;
    }
    ang = func_003D8088(ang);
    tau = 6.2831855f;
    dir = ang * 65535.0f / tau;
    for (i = 0; i < 2; i++) {
        f[i]->unk1A += dir;
        f[i]->unk3C = f[i]->unk1A;
    }
}
#else
LIT4_WORD(D_005F5594, 0x477FFF00);
LIT4_WORD(D_005F5598, 0x40490FDB);
LIT4_WORD(D_005F559C, 0x477FFF00);
LIT4_WORD(D_005F55A0, 0x40C90FDB);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D7E10);
#endif

/* Reduces an angle into [-pi, pi] by whole multiples of pi [I]. */
f32 func_003D8038(f32 a) {
    f32 p = 3.1415927f;

    if (a > p || a < -3.1415927f) {
        a -= (s32)(a / p) * p;
    }
    return a;
}

/* Wraps an angle once into [-pi, pi], then into [0, 2pi). */
f32 func_003D8088(f32 a) {
    f32 r = a;

    if (a > 3.1415927f) {
        r = a - 6.2831855f;
    } else if (a < -3.1415927f) {
        r = a + 6.2831855f;
    }
    a = r;
    if (a < 0.0f) {
        a += 6.2831855f;
    }
    return a;
}

/* Wraps an angle once into [-pi, pi]. */
f32 func_003D80E8(f32 a) {
    if (a > 3.1415927f) {
        a -= 6.2831855f;
    } else if (a < -3.1415927f) {
        a += 6.2831855f;
    }
    return a;
}

/* Random float in [0, 1): rand() / 2^31. */
f32 func_003D8128(void) {
    return rand() * 4.656613e-10f;
}

#if 0
/* NON_MATCHING: 59 same structure and instruction count +2; register allocation of mode/n/base differs and gcc hoists base+0x20 into the first loop preheader */
/* Relocates the GS image records of a model file by `off` (TBP / CBP fields) and patches them for a draw mode: 1-3
 * set the register word appended after each last record (0x320 / 0x2BC / 0x258, mode 3 then acts as 2), mode 2 also
 * sets fixed alpha values, mode 0 the 0x100000 flag (except stage type 3) [I]. */
void func_003D8158(u8 *p, s32 off, u32 mode) {
    s32 k = 0;
    s32 acc = 0;
    u8 *base = (u8 *)(((s32)p + ((Unk3D8158 *)p)->unkC + 15) & -16);
    s32 reg;
    s32 n;
    s32 i;
    u32 w0;
    u32 w1;
    u32 w2;
    u32 w3;
    s32 q;
    s32 last;
    s32 cnt;
    s32 size[12];
    s32 skip[12];

    switch (mode) {
    case 1:
        reg = 0x320;
        break;
    case 2:
        reg = 0x2BC;
        break;
    case 3:
        reg = 0x258;
        mode = 2;
        break;
    default:
        reg = 0x564;
        break;
    }
    n = ((Unk3D8158 *)p)->unk10;
    for (i = 0; i < n; i++) {
        size[i] = ((Unk3D8158 *)p)->unk14[i * 2] << 4;
        skip[i] = ((Unk3D8158 *)p)->unk14[i * 2 + 1];
    }
    cnt = ((Unk3D8158 *)p)->unk6C;
    for (p = base + 0x20; cnt >= 0; cnt--) {
        if (p - base == acc + size[k] - skip[k]) {
            acc += size[k];
            p += skip[k];
            k++;
        }
        w0 = *(u32 *)(p + 0x3C);
        w1 = *(u32 *)(p + 0x40);
        *(u32 *)(p + 0x3C) = (w0 & ~0x3FFF) | (((w0 & 0x3FFF) + off) & 0x3FFF);
        *(u32 *)(p + 0x40) = (w1 & 0xFFF8001F) | ((((w1 >> 5) & 0x3FFF) + off) & 0x3FFF) << 5;
        if (mode == 2) {
            *(u32 *)(p + 0x20) = D_0046B280[D_005F5CB6] >> 1;
            *(u32 *)(p + 0x1C) = 0x66;
            *(u32 *)(p + 0x24) = 0x42;
            *(u32 *)(p + 0x28) = 0x42;
        }
        w0 = *(u32 *)(p + 0x50);
        w1 = *(u32 *)(p + 0x54);
        q = w0 & 0x7FFF;
        last = (w0 & 0x8000) >> 15;
        w2 = *(u32 *)(p + 0x58);
        w3 = *(u32 *)(p + 0x5C);
        if (mode == 0) {
            if (D_005F5CB6 != 3) {
                w1 |= 0x100000;
            }
        } else {
            w1 &= ~0x80000;
        }
        if (mode == 2) {
            w1 |= 0x200000;
        }
        *(u32 *)(p + 0x50) = w0;
        *(u32 *)(p + 0x54) = w1;
        *(u32 *)(p + 0x58) = w2;
        *(u32 *)(p + 0x5C) = w3;
        p += 0x60;
        p += q * 0x50;
        if (last) {
            *(u32 *)p = reg | 0x15000000;
            p += 4;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D8158);
#endif

/* Uploads a chain of GS image records now (packet in buf); returns its size in 256-byte blocks. */
u32 func_003D83A8(void *rec, u32 base, void *buf) {
    GifPk pk;
    u32 r;

    sceGifPkInit(&pk, buf);
    r = func_00354290(&pk, rec, base);
    sceGifPkEnd(&pk, 0, 0, 0);
    sceGifPkTerminate(&pk);
    FlushCache(0);
    func_0035E6B8(buf);
    return r;
}

/* func_003D83A8 with the packet buffer D_01FBF620. */
u32 func_003D8430(void *rec, u32 base) {
    return func_003D83A8(rec, base, D_01FBF620);
}

/* func_003D8158 in mode 0 [I]. */
void func_003D8450(void *rec, s32 a) {
    func_003D8158(rec, a, 0);
}

/* Same as func_003D8430. */
u32 func_003D8458(void *rec, u32 base) {
    return func_003D8430(rec, base);
}

/* Same as func_003D8450. */
void func_003D8478(void *rec, s32 a) {
    func_003D8450(rec, a);
}

/* func_003D8158 with offset 0 and mode b [I]. */
void func_003D8480(void *rec, s32 b) {
    func_003D8158(rec, 0, b);
}

/* Same as func_003D8480. */
void func_003D8490(void *rec, s32 b) {
    func_003D8480(rec, b);
}

/* Binds the data p to the state of slot a->unk22 once: lists its 0x12 + n records and, for the last n, the index
 * (0x62 + j) of the record of func_003B87B0's table with the same id [I]. */
void func_003D8498(Unk3D8498Arg *a, Unk3D8498Src *p) {
    s32 id = a->unk22;
    Unk3B87C8 *s = func_003B87C8(id);
    Unk3B87B0 *t = func_003B87B0(id);
    Unk3D8498Rec *r;
    s32 i;
    s32 j;

    s->unk8 = p;
    if (s->unk0 != 0) {
        return;
    }
    s->unk0 = 1;
    s->unk4 = p->unk8;
    r = (Unk3D8498Rec *)((u8 *)p + 0x70);
    for (i = 0; i < s->unk4 + 0x12; i++) {
        s->unkC[i] = r;
        r++;
    }
    for (i = 0; i < s->unk4; i++) {
        s->unk60[i] = 0;
        for (j = 0; j < t->unk4; j++) {
            if (t->unkC[j + 0x62]->unk1C == s->unkC[i + 0x12]->unk10) {
                s->unk60[i] = j + 0x62;
            }
        }
    }
}

/* The stage model to load for a stage [I]: in the mode of func_003ABCD0 mapped from func_003A9680's selection, with
 * func_003BB4D8 only stage 13 (as 0x1E), otherwise the stage itself except 0x15 / 0x16 (-1). */
s32 func_003D85D8(s32 stage) {
    if (func_003ABCD0() != 0) {
        switch (func_003A9680()) {
        case 4:
            return 0x17;
        case 6:
            return 0x1B;
        case 9:
            return 0x16;
        case 12:
            return 0x19;
        case 14:
            return 0xD;
        case 25:
            return 0x1C;
        case 32:
            return 0x1A;
        case 35:
            return 0x18;
        case 38:
            return 0x15;
        case 11:
        case 18:
        case 26:
            return 0x1D;
        case 17:
            return 0x1E;
        default:
            return -1;
        }
    }
    if (func_003BB4D8() != 0) {
        return stage != 13 ? -1 : 0x1E;
    }
    switch (stage) {
    case 0x15:
    case 0x16:
        return -1;
    }
    return stage;
}

/* 1 for stage 7 or 0x15 (D_005F6F84) [I]. */
s32 func_003D86D8(void) {
    if (D_005F6F84 == 7 || D_005F6F84 == 0x15) {
        return 1;
    }
    return 0;
}

/* 1 for stage 1, 10 or 13 [I]. */
s32 func_003D8700(void) {
    switch (D_005F6F84) {
    case 1:
    case 10:
    case 13:
        return 1;
    }
    return 0;
}

/* The stage's view matrix: D_004334C0 for stages 0x15-0x16, otherwise func_00366EA8 [I]. */
void func_003D8748(Vu0Matrix m) {
    switch (D_005F6F84) {
    case 0x15:
    case 0x16:
        ((Q4 *)m)->q[0] = ((Q4 *)D_004334C0)->q[0];
        ((Q4 *)m)->q[1] = ((Q4 *)D_004334C0)->q[1];
        ((Q4 *)m)->q[2] = ((Q4 *)D_004334C0)->q[2];
        ((Q4 *)m)->q[3] = ((Q4 *)D_004334C0)->q[3];
        break;
    default:
        func_00366EA8(m);
        break;
    }
}

/* Sets the object's model index (definition + variant, + 1 on stages 7 / 0x15) and scale (model unk4 / 1000), and
 * copies both to its twin [I]. */
void func_003D8798(StageObjSetup *o, StageObjSetup *o2) {
    s32 n;
    u8 v;
    f32 d;

    v = o->unk54->unk19;
    o->unk82 = v;
    if (o->unk50 != NULL) {
        o->unk82 = v + o->unk50->unk5;
    }
    n = o->unk82;
    n += func_003D86D8() == 1;
    o->unk82 = n;
    d = 1000.0f;
    o->unk74 = ((StageModel *)D_01FC2380[(s8)n])->unk4 / d;
    o2->unk82 = n;
    o2->unk74 = o->unk74;
}

/* Picks the object's kind unk7E: the definition's, or a random entry of its variant list; copies it to the twin and
 * sets the model (func_003D8798) [I]. */
void func_003D8848(StageObjSetup *o, StageObjSetup *o2) {
    if (o->unk50 == NULL) {
        o->unk7E = o->unk54->unk1A;
    } else {
        s8 n = o->unk50->unk4;
        u8 *p = o->unk50->unk0;

        if (n >= 2) {
            p += rand() / (0x7FFFFFFF / n + 1);
        }
        o->unk7E = *p;
    }
    o2->unk7E = o->unk7E;
    func_003D8798(o, o2);
}

/* Places an object from its definition: position, facing, the random ranges unk78 (x 60) and unk7C, the variant's
 * fixed values; copies it all to the twin, then picks the kind and model (func_003D8848) [I]. */
void func_003D8908(StageObjSetup *o, StageObjSetup *o2) {
    StageObjDef *d = o->unk54;
    s32 a = d->unk0 * 60;
    s32 b = d->unk1 * 60;
    u16 c0 = d->unk2;
    u16 c1 = d->unk4;
    f32 deg;
    f32 k;
    StageObjVar *v;

    o->unk50 = d->unk1C;
    o->unk87 = 0;
    o->unk5C = d->unk8;
    o->unk60 = d->unkC;
    o->unk64 = d->unk10;
    deg = d->unk14;
    k = 360.0f;
    o->unk70 = deg;
    o->unk68 = func_0034A570(deg / k * 2.0f * 3.1415927f);
    o->unk78 = a;
    if (a != b) {
        o->unk78 += (s16)(rand() / (0x7FFFFFFF / (b < a ? a - b : b - a) + 1));
    }
    o->unk7C = c0;
    if (c0 != c1) {
        o->unk7C = rand() / (0x7FFFFFFF / (c1 < c0 ? c0 - c1 : c1 - c0) + 1) + o->unk7C;
    }
    o->unk7A = -1;
    o->unk85 = 0;
    o->unk86 = 0;
    v = o->unk50;
    if (v != NULL) {
        if (v->unk8 >= 0) {
            o->unk78 = v->unk8;
        }
        if (v->unkA >= 0) {
            o->unk7C = v->unkA;
        }
        if (v->unkC > 0) {
            o->unk7A = v->unkC;
        }
    }
    o2->unk5C = o->unk5C;
    o2->unk60 = o->unk60;
    o2->unk64 = o->unk64;
    o2->unk70 = o->unk70;
    o2->unk68 = o->unk68;
    o2->unk78 = o->unk78;
    o2->unk7A = o->unk7A;
    o2->unk85 = o->unk85;
    o2->unk86 = o->unk86;
    func_003D8848(o, o2);
}

/* Places the next object of a chain relative to the previous one: moves it by its kind's offset (scaled, rotated by
 * its facing) and turns it; resets the ranges to the variant's values; copies to the twin; picks kind and model [I]. */
void func_003D8B20(StageObjSetup *o, StageObjSetup *o2, StageObjSetupList *l) {
    f32 *m = l->unk1674[o->unk7E];
    f32 s = sinf(o->unk68);
    f32 c = cosf(o->unk68);
    f32 x = m[0] * o->unk74;
    f32 z = m[1] * o->unk74;
    f32 k;
    StageObjVar *v;

    o->unk5C += x * c + z * s;
    o->unk64 += z * c - x * s;
    o->unk70 += m[2];
    k = 360.0f;
    o->unk68 = func_0034A570(o->unk70 / k * 2.0f * 3.1415927f);
    o->unk7A = -1;
    o->unk78 = 0;
    o->unk7C = 0;
    o->unk85 = 0;
    o->unk86 = 0;
    v = o->unk50;
    if (v != NULL) {
        if (v->unk8 >= 0) {
            o->unk78 = v->unk8;
        }
        if (v->unkA >= 0) {
            o->unk7C = v->unkA;
        }
        if (v->unkC > 0) {
            o->unk7A = v->unkC;
        }
    }
    o2->unk5C = o->unk5C;
    o2->unk64 = o->unk64;
    o2->unk70 = o->unk70;
    o2->unk68 = o->unk68;
    o2->unk78 = o->unk78;
    o2->unk7A = o->unk7A;
    o2->unk85 = o->unk85;
    o2->unk86 = o->unk86;
    func_003D8848(o, o2);
}

/* Clears a list of three byte pairs. */
void func_003D8C98(Unk3D8C98 *p) {
    s32 i;

    for (i = 0; i < 3; i++) {
        p->unk0[i] = -1;
        p->unk3[i] = -1;
    }
    p->unk6 = -1;
}

/* Appends (a - 1, b) to the list; returns 5 / 1 (by c) when added, 6 / 0 when full [I]. */
s32 func_003D8CD8(Unk3D8C98 *p, s8 a, s8 b, s32 c) {
    s32 n = p->unk6 + 1;

    if (n < 3) {
        p->unk0[n] = a - 1;
        p->unk3[n] = b;
        p->unk6++;
        return c ? 1 : 5;
    }
    return c != 0 ? 0 : 6;
}

#if 0
/* NON_MATCHING: 9 only t0/t1 swapped between &o->unk88 and its count byte; no declaration order tried changes it */
/* Next step of an object chain [I]: from the repeat count of the current variant (or the definition), returns 0 / 6
 * (end), 1 / 5 (repeat), 2 / 4 or 1 / 3 (special counts) - the second value when there is no variant list - keeping
 * the counters in the object's list unk88 (func_003D8CD8). */
s32 func_003D8D38(StageObjSetup *o) {
    Unk3D8C98 *l = &o->unk88;
    StageObjVar *v = o->unk50;
    s8 k = o->unk87;
    s8 n = l->unk6;
    s8 rep;
    s8 back;
    s8 cnt;

    if (v == NULL) {
        cnt = 1;
        back = 0;
        rep = o->unk54->unk1B;
    } else {
        rep = v->unk6;
        back = v->unk7;
        cnt = v->unk4;
    }
    if (rep < 0 && back == 0 && cnt == 1) {
        return v != NULL ? 2 : 4;
    }
    if (rep < 0) {
        return back == 0 ? 3 : 1;
    }
    if (rep == 0) {
        return 0;
    }
    if (n < 0 || l->unk3[n] != k) {
        return func_003D8CD8(l, rep, k, (s32)v);
    }
    if (--l->unk0[n] > 0) {
        return v != NULL ? 1 : 5;
    }
    l->unk0[n] = -1;
    l->unk3[n] = -1;
    l->unk6--;
    return v != NULL ? 0 : 6;
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D8D38);
#endif

/* Advances an object of a chain by the step func_003D8D38 returns (next variant, step back, repeat, restart from the
 * definition) and copies the chain state to the twin [I]. */
void func_003D8E30(StageObjSetup *o, StageObjSetup *o2, StageObjSetupList *l) {
    StageObjVar *v;

    switch (func_003D8D38(o)) {
    case 2:
    case 4:
        o->unk7C = 0;
        return;
    case 3:
        func_003D8B20(o, o2, l);
        return;
    case 0:
        v = o->unk50 + 1;
        o->unk50 = v;
        o->unk87++;
        if (v->unk0 != NULL) {
            func_003D8B20(o, o2, l);
            break;
        }
        if (v->unk7 != 0) {
            o->unk85 |= 1;
            o2->unk85 = 1;
            return;
        }
        func_003D8908(o, o2);
        break;
    case 1:
        o->unk87 -= o->unk50->unk7;
        o->unk50 -= o->unk50->unk7;
        if ((s8)o->unk87 >= 0) {
            func_003D8B20(o, o2, l);
            break;
        }
        func_003D8908(o, o2);
        break;
    case 5:
        func_003D8B20(o, o2, l);
        break;
    case 6:
        func_003D8908(o, o2);
        break;
    default:
        break;
    }
    o2->unk50 = o->unk50;
    o2->unk87 = o->unk87;
    o2->unk88 = o->unk88;
}

/* 1 if slot i (< n) of D_01FC2380 is set. */
s32 func_003D8FA0(s32 i, s32 n) {
    if (i >= n) {
        return 0;
    }
    return D_01FC2380[i] != NULL;
}

/* a < b. */
s32 func_003D8FD0(s32 a, s32 b) {
    return a < b;
}

/* 0 / 1 visibility of an object whose flags 0x84 tie it to character 0xB in costume 1 or 3 being in a team: bit 2
 * selects shown-with or shown-without [I]. */
s32 func_003D8FD8(StageObjDef *d) {
    s32 k;

    if ((d->unk6 & 0x84) == 0) {
        return 1;
    }
    k = ((d->unk6 >> 2) ^ 1) & 1;
    if (D_00440940[0][0].unk0 == 0xB && (D_00440940[0][0].unk4 == 1 || D_00440940[0][0].unk4 == 3)) {
        return k;
    }
    if (D_00440940[0][1].unk0 == 0xB && (D_00440940[0][1].unk4 == 1 || D_00440940[0][1].unk4 == 3)) {
        return k;
    }
    if (D_00440940[1][0].unk0 == 0xB && (D_00440940[1][0].unk4 == 1 || D_00440940[1][0].unk4 == 3)) {
        return k;
    }
    if (D_00440940[1][1].unk0 == 0xB && (D_00440940[1][1].unk4 == 1 || D_00440940[1][1].unk4 == 3)) {
        return k;
    }
    return k ^ 1;
}

/* 1 if every model and kind the definition can use is loaded (models < n, kinds < lim) and it is shown with the
 * current teams (func_003D8FD8) [I]. */
s32 func_003D90A8(StageObjDef *d, s32 n, s32 lim) {
    StageObjVar *v = d->unk1C;
    s8 m = d->unk19;
    u8 *p;
    s32 i;

    if (v == NULL) {
        if (func_003D8FA0(m, n) == 0) {
            return 0;
        }
        if (func_003D8FD0(d->unk1A, lim) == 0) {
            return 0;
        }
        return func_003D8FD8(d);
    } else {
        for (; v->unk0 != NULL; v++) {
            p = v->unk0;
            if (func_003D8FA0(m + (s8)v->unk5, n) == 0) {
                return 0;
            }
            for (i = 0; i < v->unk4; i++) {
                if (func_003D8FD0((s8)*p, lim) == 0) {
                    return 0;
                }
                p++;
            }
        }
    }
    return func_003D8FD8(d);
}

/* 1 if the object is in use (unk1A != -1 or unk1C != 0) [I]. */
s32 func_003D91B0(Unk3D91B0 *p) {
    s32 r = 0;

    if (p->unk1A != -1 || p->unk1C != 0) {
        r = 1;
    }
    return r;
}

/* Sets the object's facing (unk6C, radians) and turn mode unk80 from its definition flags 1 / 2 / 8 / 0x20 [I]. */
void func_003D91D8(StageObjSetup *o) {
    StageObjDef *d = o->unk54;
    u16 f = d->unk6;

    if (f & 1) {
        o->unk80 = 1;
        o->unk6C = -1.5707964f;
    } else if (f & 2) {
        o->unk80 = 2;
        o->unk6C = -1.5707964f;
    } else if (f & 8) {
        o->unk80 = 3;
        o->unk6C = -2.617994f;
    } else if (f & 0x20) {
        o->unk85 |= 4;
        o->unk86 = d->unk18;
        o->unk6C = -2.617994f;
        o->unk80 = 0;
        o->unk82 = D_00879666[0] & 3;
    } else {
        o->unk80 = 0;
        o->unk6C = -2.617994f;
    }
}

#if 0
/* NON_MATCHING: 21 everything matches except the 4x8 qword copy loop: the original keeps three register copies of the destination address (move t1/a3/a0,a1) with constant displacements; tried index, byte-offset and per-row pointer forms */
/* Builds a stage object list from a definition array (ends at the first unused definition): every definition whose
 * models are loaded becomes an object (at most 0x24); then copies the list's GS / colour set-up. Returns 0 if no
 * object was made [I]. */
s32 func_003D9280(StageObjSetupList *l, StageObjDef *d) {
    s32 i = 0;
    StageObjSetup *o;

    while (func_003D91B0(d) != 0) {
        if (func_003D90A8(d, D_005F6F88, D_005F6FC0) == 1) {
            o = &l->unk0[i++];
            o->unk0 = o;
            o->unk54 = d;
            func_003D8908(o, o);
            func_003D8C98(&o->unk88);
            func_003D91D8(o);
            if (i >= 0x25) {
                i--;
                break;
            }
        }
        d++;
    }
    if (i == 0) {
        return 0;
    }
    l->unk1678 = i;
    l->unk1640 = D_00454340;
    l->unk1650 = D_00454350;
    for (i = 0; i < 0x12; i++) {
        l->unk1660[i] = D_00454360[i];
    }
    for (i = 0; i < 8; i++) {
        s32 k = i * 16;
        *(u128 *)((u8 *)l->unk1440[0] + k) = *(u128 *)((u8 *)D_00454140[0] + k);
        *(u128 *)((u8 *)l->unk1440[1] + k) = *(u128 *)((u8 *)D_00454140[1] + k);
        *(u128 *)((u8 *)l->unk1440[2] + k) = *(u128 *)((u8 *)D_00454140[2] + k);
        *(u128 *)((u8 *)l->unk1440[3] + k) = *(u128 *)((u8 *)D_00454140[3] + k);
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D9280);
#endif

/* 1 while the game is paused (as the effects test it) or, with D_0043B800 set, in mode 5 [I]. */
s32 func_003D9410(void) {
    if (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
        return 1;
    }
    if (D_0043B800[0] != 0 && gMode == 5) {
        return 1;
    }
    return 0;
}

#if 0
/* NON_MATCHING: 18 first half matches; the second fighter clamp and the min keep b in f0 then copy it to f1 in the original, and the gFighters+0x8000 base stays in s0 across the call (36 clamp/min source forms tried) */
/* Camera far distance [I]: 3000 in mode 7, 2000 in mode 8, otherwise the smaller of the two fighters' distances
 * (unk + 0x620, kept in D_005F6FC8 / D_005F6FCC while a fighter is hidden), at least 3000. */
f32 func_003D9468(void) {
    f32 a;
    f32 b;
    f32 x;
    f32 r;

    switch (gMode) {
    case 8:
        return 2000.0f;
    case 7:
        return 3000.0f;
    }
    if (gFighters[0].unk127 != 0 && Tag_StepCounters(0, 0) == 0) {
        x = FIGHTER_F620(&gFighters[0]);
    } else {
        x = D_005F6FC8;
    }
    if (x < 3000.0f) {
        a = 3000.0f;
    } else {
        a = x;
    }
    if (gFighters[1].unk127 != 0 && Tag_StepCounters(1, 0) == 0) {
        x = FIGHTER_F620(&gFighters[1]);
    } else {
        x = D_005F6FCC;
    }
    if (x < 3000.0f) {
        b = 3000.0f;
    } else {
        b = x;
    }
    D_005F6FC8 = a;
    D_005F6FCC = b;
    if (a < b) {
        r = a;
    } else {
        r = b;
    }
    if (r < 3000.0f) {
        r = 3000.0f;
    }
    return r;
}
#else
LIT4_WORD(D_005F55EC, 0x453B8000);
LIT4_WORD(D_005F55F0, 0x453B8000);
LIT4_WORD(D_005F55F4, 0x453B8000);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D9468);
#endif

#if 0
/* NON_MATCHING: 91 register allocation (list/objs in s7/s8 and i/dst in s3/s2 swapped vs original) and the wait-timer if/else laid out as "b test; advance; test: blez; decrement" in the original (no C form found) */
/* Update hook of the stage object lists (first variant): steps the objects' animations in a scratchpad copy of the
 * list, sorts the animated ones by (animation, frame), then builds their matrices and clip flags (object and
 * projected shadow) [I]. */
void func_003D9588(StageObjList *list, f32 t, s32 n) {
    f32 light[4];
    Vu0Matrix sm;
    Vu0Matrix rm;
    s32 lastAnim;
    s32 lastFrame;
    Vu0Matrix *view;
    Vu0Matrix *shadow;
    Vu0Matrix *anim;
    StageSpr *spr;
    StageObj *objs;
    StageObj *o;
    StageObj *dst;
    StageObj *self;
    StageObjDesc *d;
    SoNode *node;
    SoNode *prev;
    SoNode *p;
    f32 *m;
    FVec4 *bb;
    f32 a;
    f32 r;
    f32 y;
    s32 i;

    dst = list->obj;
    i = 0;
    FlushCache(0);
    spr = Spr_Setup(1, 0x34C0);
    lastAnim = 0xFFFF;
    shadow = &spr->shadow;
    view = &spr->view;
    objs = spr->list.obj;
    anim = &spr->anim;
    func_00343128(&spr->list, list, 0x168);
    o = objs;
    lastFrame = 0xFFFF;
    func_003D8748(*view);
    func_00353C88(0, light);
    UnitMtx(sm);
    sm[3][1] = 100.0f;
    sm[1][1] = 0.0f;
    sm[1][0] = -light[0] / light[1];
    sm[1][2] = -light[2] / light[1];
    CopyMtx(*shadow, *view);
    sceVu0MulMatrix(*shadow, *shadow, sm);
    Dma_Wait(0x1000D400);
    spr->head.next = NULL;
    spr->head.obj = NULL;
    for (; i < list->unk1678; i++, o++, dst++) {
        if (n != 0 && func_003D9410() == 0 && !(o->unk85 & 1)) {
            if (o->unk78 <= 0) {
                if (o->unk7C + 1 >= func_00367CD0(o->unk7E) || o->unk7A == 0) {
                    func_003D8E30((StageObjSetup *)o, (StageObjSetup *)dst, (StageObjSetupList *)objs);
                } else {
                    dst->unk7C = ++o->unk7C;
                    if (o->unk7A > 0) {
                        dst->unk7A = --o->unk7A;
                    }
                    if (o->unk85 & 4) {
                        o->unk85 ^= 2;
                        if ((s8)--dst->unk86 < 0) {
                            o->unk85 &= ~6;
                        }
                        dst->unk85 = o->unk85;
                    }
                }
                dst->unk7C = o->unk7C;
            } else {
                dst->unk78 = --o->unk78;
            }
        }
        if (o->unk78 > 0 || (o->unk85 & 1) || (o->unk85 & 2)) {
            dst->unk83 = 1;
            dst->unk84 = 1;
        } else {
            dst->unk58 = o->unk58 = func_00367D00(o->unk7E);
            node = &spr->node[i];
            prev = &spr->head;
            p = prev->next;
            node->obj = o;
            while (p != NULL && !(node->obj->unk7E < p->obj->unk7E)
                   && (p->obj->unk7E < node->obj->unk7E || node->obj->unk7C > p->obj->unk7C)) {
                prev = p;
                p = p->next;
            }
            node->next = prev->next;
            prev->next = node;
        }
    }
    node = spr->head.next;
    spr->list.minv[3] = t;
    for (; node != NULL; node = node->next) {
        o = node->obj;
        self = o->self;
        m = o->mtx[0];
        d = o->desc;
        if (!(d->unk6 & 1) && !(o->desc->unk6 & 2)) { /* fake: o->desc again, else gcc folds the tests into & 3 */
            UnitMtx(o->mtx);
            sceVu0RotMatrixY(o->mtx, o->mtx, o->rotY);
            o->mtx[3][0] = o->pos[0];
            o->mtx[3][1] = o->pos[1];
            o->mtx[3][2] = o->pos[2];
            if (o->desc->unk6 & 0x40) {
                func_003D56D8(o->desc->unk18, rm);
                sceVu0MulMatrix(o->mtx, rm, o->mtx);
            }
        } else {
            func_003D7D90(d->unk18, o->mtx);
        }
        UnitMtx(*anim);
        if (lastAnim != o->unk7E || lastFrame != o->unk7C) {
            func_00367FE0(o->unk58, spr->pose[0], o->unk7C);
            lastFrame = o->unk7C;
            lastAnim = o->unk7E;
        }
        a = spr->pose[0][0] * 9.5873809e-05f;
        r = spr->pose[0][2] * o->scale;
        y = spr->pose[0][1] * o->scale;
        (*anim)[3][0] = sinf(a) * r;
        (*anim)[3][1] = y;
        (*anim)[3][2] = -cosf(a) * r;
        sceVu0MulMatrix(o->mtx, o->mtx, *anim);
        CopyMtx(self->mtx, o->mtx);
        sceVu0MulMatrix(spr->clip, *view, o->mtx);
        bb = spr->list.bbox[self->unk80];
        self->unk83 = sceVu0ClipAll(spr->list.minv, spr->list.maxv, spr->clip, bb, 8);
        if (o->desc->unk6 & 0x10) {
            sceVu0MulMatrix(spr->clip, *shadow, o->mtx);
            self->unk84 = sceVu0ClipAll(spr->list.minv, spr->list.maxv, spr->clip, bb, 8);
        } else {
            self->unk84 = 1;
        }
    }
}
#else
LIT4_WORD(D_005F55F8, 0x38C90FDC);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D9588);
#endif

#if 0
/* NON_MATCHING: 110 same two issues as func_003D9588 (shares its code shape) */
/* Update hook of the stage object lists (second variant, func_003D8700() != 0): steps the objects' animations in a scratchpad copy of the
 * list, sorts the animated ones by (animation, frame), then builds their matrices and clip flags (object and
 * projected shadow) [I]. */
void func_003D9BE8(StageObjList *list, f32 t, s32 n) {
    f32 light[4];
    Vu0Matrix sm;
    Vu0Matrix rm;
    Vu0Matrix *view;
    Vu0Matrix *shadow;
    Vu0Matrix *anim;
    StageSpr *spr;
    StageObj *objs;
    StageObj *o;
    StageObj *dst;
    StageObj *self;
    StageObjDesc *d;
    SoNode *node;
    SoNode *prev;
    SoNode *p;
    f32 *m;
    FVec4 *bb;
    s16 *rot;
    f32 a;
    f32 r;
    f32 y;
    s32 i;

    dst = list->obj;
    i = 0;
    FlushCache(0);
    spr = Spr_Setup(1, 0x34C0);
    shadow = &spr->shadow;
    view = &spr->view;
    objs = spr->list.obj;
    anim = &spr->anim;
    func_00343128(&spr->list, list, 0x168);
    o = objs;
    func_003D8748(*view);
    func_00353C88(0, light);
    UnitMtx(sm);
    sm[3][1] = 100.0f;
    sm[1][1] = 0.0f;
    sm[1][0] = -light[0] / light[1];
    sm[1][2] = -light[2] / light[1];
    CopyMtx(*shadow, *view);
    sceVu0MulMatrix(*shadow, *shadow, sm);
    Dma_Wait(0x1000D400);
    spr->head.next = NULL;
    spr->head.obj = NULL;
    for (; i < list->unk1678; i++, o++, dst++) {
        if (n != 0 && func_003D9410() == 0 && !(o->unk85 & 1)) {
            if (o->unk78 <= 0) {
                if (o->unk7C + 1 >= func_00364168(o->unk7E) || o->unk7A == 0) {
                    func_003D8E30((StageObjSetup *)o, (StageObjSetup *)dst, (StageObjSetupList *)objs);
                } else {
                    dst->unk7C = ++o->unk7C;
                    if (o->unk7A > 0) {
                        dst->unk7A = --o->unk7A;
                    }
                    if (o->unk85 & 4) {
                        o->unk85 ^= 2;
                        if ((s8)--dst->unk86 < 0) {
                            o->unk85 &= ~6;
                        }
                        dst->unk85 = o->unk85;
                    }
                }
                dst->unk7C = o->unk7C;
            } else {
                dst->unk78 = --o->unk78;
            }
        }
        if (o->unk78 > 0 || (o->unk85 & 1) || (o->unk85 & 2)) {
            dst->unk83 = 1;
            dst->unk84 = 1;
        } else {
            node = &spr->node[i];
            node->obj = o;
            node->next = spr->head.next;
            spr->head.next = node;
        }
    }
    node = spr->head.next;
    spr->list.minv[3] = t;
    for (; node != NULL; node = node->next) {
        o = node->obj;
        self = o->self;
        m = o->mtx[0];
        d = o->desc;
        if (!(d->unk6 & 1) && !(o->desc->unk6 & 2)) { /* fake: o->desc again, else gcc folds the tests into & 3 */
            UnitMtx(o->mtx);
            sceVu0RotMatrixY(o->mtx, o->mtx, o->rotY);
            o->mtx[3][0] = o->pos[0];
            o->mtx[3][1] = o->pos[1];
            o->mtx[3][2] = o->pos[2];
            if (o->desc->unk6 & 0x40) {
                func_003D56D8(o->desc->unk18, rm);
                sceVu0MulMatrix(o->mtx, rm, o->mtx);
            }
        } else {
            func_003D7D90(d->unk18, o->mtx);
        }
        UnitMtx(*anim);
        rot = func_00364190(o->unk7E, o->unk7C);
        o->unk58 = (s32)rot;
        a = rot[0] * 9.5873809e-05f;
        r = rot[2] * o->scale;
        y = rot[1] * o->scale;
        (*anim)[3][0] = sinf(a) * r;
        (*anim)[3][1] = y;
        (*anim)[3][2] = -cosf(a) * r;
        sceVu0MulMatrix(o->mtx, o->mtx, *anim);
        CopyMtx(self->mtx, o->mtx);
        sceVu0MulMatrix(spr->clip, *view, o->mtx);
        bb = spr->list.bbox[self->unk80];
        self->unk83 = sceVu0ClipAll(spr->list.minv, spr->list.maxv, spr->clip, bb, 8);
        if (o->desc->unk6 & 0x10) {
            sceVu0MulMatrix(spr->clip, *shadow, o->mtx);
            self->unk84 = sceVu0ClipAll(spr->list.minv, spr->list.maxv, spr->clip, bb, 8);
        } else {
            self->unk84 = 1;
        }
    }
}
#else
LIT4_WORD(D_005F55FC, 0x38C90FDC);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003D9BE8);
#endif

#if 0
/* NON_MATCHING: 337 draft; VU0 rotation as one inline __asm__ block (lqc2 / jal func_003E8FA0 / vmula / sqc2), stack layout and spills differ */
/* Rotates the matrix m by the angles r[0] (x), r[1] (y), r[2] (z) on VU0 (the inlined body of the library rotations,
 * func_003E8FA0 being its sin / cos helper that leaves both in vf8). */
static inline void RotMtxXYZ(Vu0Matrix m, f32 *r) {
    __asm__ volatile(
        ".set noreorder\n"
        "lqc2 $vf4, 0x0(%0)\n"
        "lqc2 $vf5, 0x10(%0)\n"
        "lqc2 $vf6, 0x20(%0)\n"
        "lqc2 $vf7, 0x30(%0)\n"
        "jal func_003E8FA0\n"
        "lwc1 $f12, 0x0(%1)\n"
        "vaddx.z $vf8, $vf0, $vf8x\n"
        "vsubx.y $vf9, $vf0, $vf8x\n"
        "vaddy.z $vf9, $vf0, $vf8y\n"
        "vmulay.yz $ACC, $vf8, $vf4y\n"
        "vmaddz.yz $vf4, $vf9, $vf4z\n"
        "vmulay.yz $ACC, $vf8, $vf5y\n"
        "vmaddz.yz $vf5, $vf9, $vf5z\n"
        "vmulay.yz $ACC, $vf8, $vf6y\n"
        "vmaddz.yz $vf6, $vf9, $vf6z\n"
        "vmulay.yz $ACC, $vf8, $vf7y\n"
        "vmaddz.yz $vf7, $vf9, $vf7z\n"
        "jal func_003E8FA0\n"
        "lwc1 $f12, 0x4(%1)\n"
        "vaddy.z $vf8, $vf0, $vf8y\n"
        "vaddy.x $vf9, $vf0, $vf8y\n"
        "vsubx.z $vf9, $vf0, $vf8x\n"
        "vmulaz.xz $ACC, $vf8, $vf4z\n"
        "vmaddx.xz $vf4, $vf9, $vf4x\n"
        "vmulaz.xz $ACC, $vf8, $vf5z\n"
        "vmaddx.xz $vf5, $vf9, $vf5x\n"
        "vmulaz.xz $ACC, $vf8, $vf6z\n"
        "vmaddx.xz $vf6, $vf9, $vf6x\n"
        "vmulaz.xz $ACC, $vf8, $vf7z\n"
        "vmaddx.xz $vf7, $vf9, $vf7x\n"
        "jal func_003E8FA0\n"
        "lwc1 $f12, 0x8(%1)\n"
        "vaddx.y $vf9, $vf0, $vf8x\n"
        "vaddy.x $vf9, $vf0, $vf8y\n"
        "vsubx.x $vf8, $vf0, $vf8x\n"
        "vmulax.xy $ACC, $vf9, $vf4x\n"
        "vmaddy.xy $vf4, $vf8, $vf4y\n"
        "vmulax.xy $ACC, $vf9, $vf5x\n"
        "vmaddy.xy $vf5, $vf8, $vf5y\n"
        "vmulax.xy $ACC, $vf9, $vf6x\n"
        "vmaddy.xy $vf6, $vf8, $vf6y\n"
        "vmulax.xy $ACC, $vf9, $vf7x\n"
        "vmaddy.xy $vf7, $vf8, $vf7y\n"
        "sqc2 $vf4, 0x0(%0)\n"
        "sqc2 $vf5, 0x10(%0)\n"
        "sqc2 $vf6, 0x20(%0)\n"
        "sqc2 $vf7, 0x30(%0)\n"
        ".set reorder\n"
        :
        : "r"(m), "r"(r)
        : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "$31",
          "$f0", "$f1", "$f2", "$f3", "$f12", "memory");
}

/* Draw hook of the stage object lists (first variant): sorts the objects whose model or shadow is not clipped by
 * (animation, frame), then for each one (and once more with the shadow matrix when its shadow is visible) builds the
 * 18 bone matrices from the pose of func_00367FE8 and sends a VIF1 packet (copy of the template D_004FAF80 with the
 * bone matrices, the light block and DMA references to the model bank's data) from a double-buffered scratchpad
 * area [I]. */
void func_003DA138(StageObjList *list) {
    f32 r[4];
    f32 light[4];
    Vu0Matrix sm;
    Vif1Packet pk;
    StageSpr *spr;
    StageSpr *buf;
    StageSpr *other;
    StageObj *o;
    SoNode *node;
    SoNode *prev;
    SoNode *p;
    Vu0Matrix *vm;
    Vu0Matrix *view;
    Vu0Matrix *shadow;
    Vu0Matrix *bones;
    Vu0Matrix *tmp;
    Vu0Matrix *m;
    StageBank *bank;
    u8 *ofs;
    s16 *pose;
    u8 *dst;
    u8 *data;
    u32 qwc;
    s32 lastAnim;
    s32 lastFrame;
    s32 pass;
    s32 k;
    s32 i;
    s32 j;
    s32 n;

    FlushCache(0);
    spr = Spr_Setup(1, 0x34C0);
    lastAnim = 0xFFFF;
    shadow = &spr->shadow;
    bones = spr->bones;
    view = &spr->view;
    o = spr->list.obj;
    func_00343128(&spr->list, list, 0x168);
    tmp = &spr->anim;
    buf = spr;
    func_003D8748(*view);
    lastFrame = 0xFFFF;
    func_00353C88(0, light);
    other = (StageSpr *)((u8 *)buf + 0x900);
    UnitMtx(sm);
    sm[3][1] = 100.0f;
    sm[1][1] = 0.0f;
    sm[1][0] = -light[0] / light[1];
    sm[1][2] = -light[2] / light[1];
    CopyMtx(*shadow, *view);
    sceVu0MulMatrix(*shadow, *shadow, sm);
    func_00353E18(spr->light);
    spr->lightc[0][3] = D_005F6944;
    spr->lightc[1][3] = D_005F6948;
    Dma_Wait(0x1000D400);
    qwc = (u32)(D_004FB800 - D_004FAF80) / 4;
    func_00343128((u8 *)spr + 0x10, D_004FAF80, qwc);
    Dma_Wait(0x1000D400);
    func_00343128((u8 *)spr + 0x910, D_004FAF80, qwc);
    Dma_Wait(0x1000D400);
    buf->head.next = NULL;
    buf->head.obj = NULL;
    for (i = 0; i < list->unk1678; i++, o++) {
        if (o->unk83 == 0 || o->unk84 == 0) {
            node = &spr->node[i];
            prev = &spr->head;
            p = prev->next;
            node->obj = o;
            while (p != NULL && !(node->obj->unk7E < p->obj->unk7E)
                   && (p->obj->unk7E < node->obj->unk7E || node->obj->unk7C > p->obj->unk7C)) {
                prev = p;
                p = p->next;
            }
            node->next = p;
            prev->next = node;
        }
    }
    node = spr->head.next;
    vm = view;
    pass = 0;
    while (node != NULL) {
        o = node->obj;
        k = (pass == 0) ? o->unk82 : 0;
        bank = D_01FC2380[k];
        ofs = (u8 *)bank + 0x70;
        sceVu0MulMatrix(spr->clip, *vm, o->mtx);
        sceVu0MulMatrix(spr->lightClip, spr->light, o->mtx);
        if (lastAnim != o->unk7E || lastFrame != o->unk7C) {
            func_00367FE8(o->unk58, spr->pose[0], o->unk7C);
            lastAnim = o->unk7E;
            lastFrame = o->unk7C;
        }
        pose = spr->pose[1];
        r[0] = (s16)-pose[0] * 9.5873809e-05f;
        r[1] = (s16)-pose[1] * 9.5873809e-05f;
        r[2] = pose[2] * 9.5873809e-05f;
        UnitMtx(*tmp);
        RotMtxXYZ(*tmp, r);
        (*tmp)[3][0] = ((f32 *)ofs)[1];
        (*tmp)[3][1] = ((f32 *)ofs)[2];
        (*tmp)[3][2] = ((f32 *)ofs)[3];
        ofs += 0x20;
        CopyMtx(bones[0], *tmp);
        pose = spr->pose[2];
        for (j = 1; j < 18; j++, pose += 3) {
            r[0] = (s16)-pose[0] * 9.5873809e-05f;
            r[1] = (s16)-pose[1] * 9.5873809e-05f;
            r[2] = pose[2] * 9.5873809e-05f;
            UnitMtx(*tmp);
            if (j == 14 || j == 17) {
                sceVu0RotMatrixZ(*tmp, *tmp, o->unk6C);
            }
            RotMtxXYZ(*tmp, r);
            (*tmp)[3][0] = ((f32 *)ofs)[1];
            (*tmp)[3][1] = ((f32 *)ofs)[2];
            (*tmp)[3][2] = ((f32 *)ofs)[3];
            ofs += 0x20;
            sceVu0MulMatrix(bones[j], bones[spr->list.parent[j]], *tmp);
        }
        if (pass == 1 || o->unk83 == 0) {
            sceVif1PkInit(&pk, buf);
            sceVif1PkCnt(&pk, 0);
            sceVif1PkAlign(&pk, 2, 0);
            pk.pBase[3] = 0x11000000;
            sceVif1PkReserve(&pk, D_004FB800 - D_004FAF80);
            dst = (u8 *)buf + ((u8 *)D_004FAFB0 - (u8 *)D_004FAF80);
            m = &bones[1];
            for (n = 0; n < 17; n++, m++, dst += 0x70) {
                sceVu0MulMatrix((f32 (*)[4])dst, spr->clip, *m);
                sceVu0MulMatrix((f32 (*)[4])(dst + 0x40), spr->lightClip, *m);
            }
            dst += 0x70;
            CopyMtx((f32 (*)[4])dst, (f32 (*)[4])spr->lightc);
            bank = D_01FC2380[k];
            data = (u8 *)(((u32)bank + bank->dataOffset + 15) & ~15);
            n = bank->count;
            for (j = 0; j < n; j++) {
                qwc = bank->refs[j][0];
                sceVif1PkRef(&pk, data, qwc, 0, 0, 0);
                data += qwc * 16;
            }
            sceVif1PkTerminate(&pk);
            func_00343178(buf, sceVif1PkSize(&pk));
            {
                StageSpr *t = other;

                other = buf;
                buf = t;
            }
        }
        if (pass == 0 && o->unk84 == 0) {
            vm = shadow;
            pass = 1;
        } else {
            vm = view;
            pass = 0;
            node = node->next;
        }
    }
}
#else
LIT4_WORD(D_005F5600, 0x38C90FDC);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003DA138);
#endif

#if 0
/* NON_MATCHING: 267 draft; same structure as func_003DA138 (pose from func_00364190, push-front list) */
/* Draw hook of the stage object lists (second variant, func_003D8700() != 0): as func_003DA138 with the objects in
 * reverse list order and the pose taken from the animation bank (func_00364190); for each object whose model or
 * shadow is not clipped it for each one (and once more with the shadow matrix when its shadow is visible) builds the 18 bone matrices from the pose of func_00367FE8 and sends a VIF1 packet (copy of the template D_004FAF80 with the
 * bone matrices, the light block and DMA references to the model bank's data) from a double-buffered scratchpad
 * area [I]. */
void func_003DAA80(StageObjList *list) {
    f32 r[4];
    f32 light[4];
    Vu0Matrix sm;
    Vif1Packet pk;
    StageSpr *spr;
    StageSpr *buf;
    StageSpr *other;
    StageObj *o;
    SoNode *node;
    Vu0Matrix *vm;
    Vu0Matrix *view;
    Vu0Matrix *shadow;
    Vu0Matrix *bones;
    Vu0Matrix *tmp;
    Vu0Matrix *m;
    StageBank *bank;
    u8 *ofs;
    s16 *pose;
    u8 *dst;
    u8 *data;
    u32 qwc;
    s32 pass;
    s32 k;
    s32 i;
    s32 j;
    s32 n;

    FlushCache(0);
    spr = Spr_Setup(1, 0x34C0);
    shadow = &spr->shadow;
    bones = spr->bones;
    view = &spr->view;
    o = spr->list.obj;
    func_00343128(&spr->list, list, 0x168);
    tmp = &spr->anim;
    buf = spr;
    func_003D8748(*view);
    func_00353C88(0, light);
    other = (StageSpr *)((u8 *)buf + 0x900);
    UnitMtx(sm);
    sm[3][1] = 100.0f;
    sm[1][1] = 0.0f;
    sm[1][0] = -light[0] / light[1];
    sm[1][2] = -light[2] / light[1];
    CopyMtx(*shadow, *view);
    sceVu0MulMatrix(*shadow, *shadow, sm);
    func_00353E18(spr->light);
    spr->lightc[0][3] = D_005F6944;
    spr->lightc[1][3] = D_005F6948;
    Dma_Wait(0x1000D400);
    qwc = (u32)(D_004FB800 - D_004FAF80) / 4;
    func_00343128((u8 *)spr + 0x10, D_004FAF80, qwc);
    Dma_Wait(0x1000D400);
    func_00343128((u8 *)spr + 0x910, D_004FAF80, qwc);
    Dma_Wait(0x1000D400);
    buf->head.next = NULL;
    buf->head.obj = NULL;
    for (i = 0; i < list->unk1678; i++, o++) {
        if (o->unk83 == 0 || o->unk84 == 0) {
            node = &spr->node[i];
            node->obj = o;
            node->next = spr->head.next;
            spr->head.next = node;
        }
    }
    node = spr->head.next;
    vm = view;
    pass = 0;
    while (node != NULL) {
        o = node->obj;
        k = (pass == 0) ? o->unk82 : 0;
        bank = D_01FC2380[k];
        ofs = (u8 *)bank + 0x70;
        sceVu0MulMatrix(spr->clip, *vm, o->mtx);
        sceVu0MulMatrix(spr->lightClip, spr->light, o->mtx);
        pose = func_00364190(o->unk7E, o->unk7C) + 3;
        r[0] = (s16)-pose[0] * 9.5873809e-05f;
        r[1] = (s16)-pose[1] * 9.5873809e-05f;
        r[2] = pose[2] * 9.5873809e-05f;
        UnitMtx(*tmp);
        RotMtxXYZ(*tmp, r);
        (*tmp)[3][0] = ((f32 *)ofs)[1];
        (*tmp)[3][1] = ((f32 *)ofs)[2];
        (*tmp)[3][2] = ((f32 *)ofs)[3];
        ofs += 0x20;
        CopyMtx(bones[0], *tmp);
        pose += 3;
        for (j = 1; j < 18; j++, pose += 3) {
            r[0] = (s16)-pose[0] * 9.5873809e-05f;
            r[1] = (s16)-pose[1] * 9.5873809e-05f;
            r[2] = pose[2] * 9.5873809e-05f;
            UnitMtx(*tmp);
            if (j == 14 || j == 17) {
                sceVu0RotMatrixZ(*tmp, *tmp, o->unk6C);
            }
            RotMtxXYZ(*tmp, r);
            (*tmp)[3][0] = ((f32 *)ofs)[1];
            (*tmp)[3][1] = ((f32 *)ofs)[2];
            (*tmp)[3][2] = ((f32 *)ofs)[3];
            ofs += 0x20;
            sceVu0MulMatrix(bones[j], bones[spr->list.parent[j]], *tmp);
        }
        if (pass == 1 || o->unk83 == 0) {
            sceVif1PkInit(&pk, buf);
            sceVif1PkCnt(&pk, 0);
            sceVif1PkAlign(&pk, 2, 0);
            pk.pBase[3] = 0x11000000;
            sceVif1PkReserve(&pk, D_004FB800 - D_004FAF80);
            dst = (u8 *)buf + ((u8 *)D_004FAFB0 - (u8 *)D_004FAF80);
            m = &bones[1];
            for (n = 0; n < 17; n++, m++, dst += 0x70) {
                sceVu0MulMatrix((f32 (*)[4])dst, spr->clip, *m);
                sceVu0MulMatrix((f32 (*)[4])(dst + 0x40), spr->lightClip, *m);
            }
            dst += 0x70;
            CopyMtx((f32 (*)[4])dst, (f32 (*)[4])spr->lightc);
            bank = D_01FC2380[k];
            data = (u8 *)(((u32)bank + bank->dataOffset + 15) & ~15);
            n = bank->count;
            for (j = 0; j < n; j++) {
                qwc = bank->refs[j][0];
                sceVif1PkRef(&pk, data, qwc, 0, 0, 0);
                data += qwc * 16;
            }
            sceVif1PkTerminate(&pk);
            func_00343178(buf, sceVif1PkSize(&pk));
            {
                StageSpr *t = other;

                other = buf;
                buf = t;
            }
        }
        if (pass == 0 && o->unk84 == 0) {
            vm = shadow;
            pass = 1;
        } else {
            vm = view;
            pass = 0;
            node = node->next;
        }
    }
}
#else
LIT4_WORD(D_005F5604, 0x38C90FDC);
INCLUDE_ASM("asm/nonmatchings/model_anim_b", func_003DAA80);
#endif

/* Stage 15 ambient sound: plays SE 0x1326 at frames 32, 69, 87, 135 and 185 of a 190-frame loop [I]. */
void func_003DB2A8(void) {
    if (D_005F6F80 == 15 && func_0035A648() != 0) {
        switch (D_005F6FC4) {
        case 0x20:
        case 0x45:
        case 0x57:
        case 0x87:
        case 0xB9:
            Snd_PlaySe(0x1326, 0);
            break;
        }
        D_005F6FC4++;
        if (D_005F6FC4 >= 0xBE) {
            D_005F6FC4 = 0;
        }
    }
}

/* Resets the stage object state (no lists active). */
void func_003DB340(void) {
    D_005F67B8 = 0;
    D_005F6F88 = 0;
    D_005F6FC0 = 0;
    D_005F6FC8 = 3000.0f;
    D_005F6FCC = 3000.0f;
    D_005F6FD0 = NULL;
    D_005F6FD4 = NULL;
}

/* Sets up the stage objects of `stage` from its animation bank: frame count, up to 16 extra banks, and the
 * update / draw hooks (two variants chosen by func_003D8700). */
void func_003DB368(s32 stage, void *bank, s32 *arg) {
    s32 i;
    s32 n;

    D_005F6F84 = stage;
    D_005F67B8 = 0;
    D_005F6F88 = 0;
    D_005F6FC0 = 0;
    D_005F6FC8 = 3000.0f;
    D_005F6FCC = 3000.0f;
    if (func_00364550(bank) < 2) {
        return;
    }
    if (func_003D8700() != 0) {
        func_00364130(func_00364528(0, bank));
        D_005F6FC0 = func_00364160();
    } else {
        func_00367CA0(func_00364528(0, bank));
        D_005F6FC0 = func_00367CC8();
    }
    if (D_005F6FC0 <= 0) {
        return;
    }
    n = func_00364550(bank) - 1;
    D_005F6F88 = n;
    if (n <= 0) {
        return;
    }
    if (n > 16) {
        D_005F6F88 = 16;
    }
    if (func_003D86D8() != 0) {
        if (func_00364558(1, bank) != 0) {
            D_01FC2380[0] = func_00364528(1, bank);
            func_003D8490(D_01FC2380[0], 3);
        } else {
            D_01FC2380[0] = NULL;
        }
        i = 1;
    } else {
        i = 0;
    }
    for (; i < D_005F6F88; i++) {
        if (func_00364558(i + 1, bank) != 0) {
            D_01FC2380[i] = func_00364528(i + 1, bank);
            func_003D8478(D_01FC2380[i], arg[i]);
        } else {
            D_01FC2380[i] = NULL;
        }
    }
    if (func_003D8700() != 0) {
        D_005F6FD0 = func_003D9BE8;
        D_005F6FD4 = func_003DAA80;
    } else {
        D_005F6FD0 = func_003D9588;
        D_005F6FD4 = func_003DA138;
    }
    D_005F67B8 |= 1;
}

/* Fills the two stage object lists from the stage's entry of D_00453FC8 (flags 2 / 4: list 1 / 2 active). */
void func_003DB578(void) {
    s32 idx;
    s32 p;
    Unk453FC8 *e;

    D_005F67B8 &= 1;
    if (D_005F67B8 == 0) {
        return;
    }
    if (D_005F5CB6 != D_005F6F84) {
        return;
    }
    idx = func_003D85D8(D_005F5CB6);
    D_005F6F80 = idx;
    if (idx < 0) {
        return;
    }
    if (D_00453FC8[idx].unk0 == 0 && D_00453FC8[idx].unk4 == 0) {
        return;
    }
    p = D_00453FC8[idx].unk8;
    if (p == 0) {
        return;
    }
    D_01FBF680.unk1674 = p;
    if (D_00453FC8[idx].unk0 != 0) {
        D_005F67B8 |= 0x20;
        if (func_003D9280((StageObjSetupList *)&D_01FBF680, (StageObjDef *)D_00453FC8[idx].unk0) != 0) {
            D_005F67B8 |= 2;
        }
    }
    D_01FC0D00.unk1674 = p;
    if (D_00453FC8[D_005F6F80].unk4 != 0) {
        D_005F67B8 |= 0x40;
        if (func_003D9280((StageObjSetupList *)&D_01FC0D00, (StageObjDef *)D_00453FC8[D_005F6F80].unk4) != 0) {
            D_005F67B8 |= 4;
        }
    }
    D_005F6FC4 = 0;
    D_005F6FC8 = 3000.0f;
    D_005F6FCC = 3000.0f;
}

/* Steps the stage objects one frame. */
void func_003DB690(void) {
    func_003DB698(1);
}

/* Per-frame update of the two stage object lists (n frames stepped); flags 8 / 0x10 mark them for drawing. */
void func_003DB698(s32 n) {
    f32 t;

    if (D_005F67B8 & 6) {
        if (D_005F6FD0 != NULL) {
            if (func_003D9410() == 0) {
                func_003DB2A8();
            }
            t = func_003D9468();
            if (D_005F67B8 & 2) {
                D_005F6FD0(&D_01FBF680, t, n);
                D_005F67B8 |= 8;
            }
            if (D_005F67B8 & 4) {
                D_005F6FD0(&D_01FC0D00, t, n);
                D_005F67B8 |= 0x10;
            }
        }
    }
}

