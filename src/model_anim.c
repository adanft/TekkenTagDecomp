/* 0x3D0660-0x3D3630, the first object of module model_anim (docs/game_overview.md section 4; docs/systems/render.md
 * section 11, "Models"): the round camera shots (0x3D0660-0x3D0C68, 0x3D22E0-0x3D29E8), the model hierarchy of the
 * stage scenes (nodes, a VU0 matrix stack, depth-sorted draw lists flushed by Model_FlushDrawLists), the per-member
 * move log and the round-end bonus, the tag counters and the tag swap. Its .rodata (0x520C40-0x521700) and .lit4
 * (0x5F5370-0x5F5378) are subsegments; the data after them (0x521700, 0x5F5378, the .sbss 0x5F6F1C-0x5F6F28) belongs
 * to the object at 0x340000, so the module is two objects with the edge at 0x3D3630 (src/model_anim_b.c). Every table
 * is included in place (force_not_migration in data.txt); LIT4_WORD emits the pool words of functions still in
 * assembly. Built without -ffast-math (configure.py NO_FAST_MATH; func_003D1468 matches only that way). Every NON_MATCHING
 * draft (#if 0) compiles when enabled. */
#include "common.h"
#include "fighter.h"
#include "gte.h"

extern Fighter gFighters[4];
extern TeamMember D_00440940[2][2];
extern u8 D_0043B7F8[];

void func_003C1A78(s32 side);
void Tag_SetCounter(s32 side, s32 state);
void func_0035C8D0(Fighter *f, s32 character, s32 costume);
void func_0035C850(Fighter *f, s32 n);
void func_00368168(Fighter *f);
void func_00377F58(Fighter *f);
void func_003465F0(void);
void func_00344318(void);
void Fx_LoadFighterTex(Fighter *f);
void func_00370E20(Fighter *f, s32 n);
void func_0038ADF8(s32 side);
void func_0037A970(Fighter *f);
void func_0037EA08(void);

/* The round camera (0x3D0660-0x3D0C68, 0x3D22E0-0x3D29E8). */
/* A position copied whole (four words, 4-byte aligned). */
typedef struct {
    /* 0x0 */ s32 unk0[4];
} Vec4i;

/* A camera key (eye, target, two words) as func_0035B690 reads it. */
typedef struct {
    /* 0x00 */ s32 unk0[3];
    /* 0x0C */ s32 unkC[3];
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
} CamKey;

/* One shot of the round-start camera (.rodata 0x520C40, 0x14 bytes each): where the camera looks from, the base
 * rotation and its random spread, the distance and its spread [V offsets; I meanings]. */
typedef struct {
    /* 0x00 */ s16 unk0;  /* 0: the other fighter, 1: the chosen fighter, other: the last hit position */
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;  /* rotation x */
    /* 0x06 */ s16 unk6;  /* rotation y, added to the fighter's facing / 16 */
    /* 0x08 */ s16 unk8;  /* rotation z */
    /* 0x0A */ s16 unkA;  /* spread of unk4 */
    /* 0x0C */ s16 unkC;  /* spread of unk6 */
    /* 0x0E */ s16 unkE;  /* spread of unk8 */
    /* 0x10 */ s16 unk10; /* distance (scaled by the camera distance / 600) */
    /* 0x12 */ s16 unk12; /* spread of unk10 */
} Unk520C40;

/* Start and end of a scripted camera move (0x1FB6EB8): target and eye as s16 points [I]. */
typedef struct {
    /* 0x00 */ s16 unk0[3]; /* target, start */
    /* 0x06 */ s16 unk6[3]; /* target, end */
    /* 0x0C */ s16 unkC[3]; /* eye, start */
    /* 0x12 */ s16 unk12[3]; /* eye, end */
} CamMove;

/* A scripted camera move of a round intro / win pose (.rodata 0x521140, 0x24 bytes each): two rotations and four
 * offsets (target and eye, at the start and at the end), scaled by the camera distance / 600 [I]. */
typedef struct {
    /* 0x00 */ s16 unk0[3];  /* rotation of the start */
    /* 0x06 */ s16 unk6[3];  /* rotation of the end */
    /* 0x0C */ s16 unkC[3];  /* target offset, start */
    /* 0x12 */ s16 unk12[3]; /* target offset, end */
    /* 0x18 */ s16 unk18[3]; /* eye offset, start */
    /* 0x1E */ s16 unk1E[3]; /* eye offset, end */
} Unk521140;

/* The camera state (0x433330, src/stage.c), up to the camera distance. */
typedef struct {
    /* 0x00 */ u8 pad0[0xA4];
    /* 0xA4 */ s32 unkA4;
} Unk433330Head;

extern const Unk520C40 D_00520C40[];
extern const s8 D_00520F88[][4];
extern s32 D_004333D4[]; /* D_00433330.unkA4 (src/stage.c): the camera distance */
extern s32 D_005F6ED0;
extern s32 D_005F6ED4;
extern s32 D_005F6ED8;
extern s32 D_005F6EDC;
extern s32 D_005F6F14;
extern s32 D_005F6F18;
extern CamKey D_01FAD3D0;
extern CamKey D_01FB6E98;
extern const Unk521140 D_00521140[];
extern Unk433330Head D_00433330;
extern CamMove D_01FB6EB8;
extern s32 D_005F64DC;
extern const s16 gSinTable[];
extern Vec4i D_01FAD3F0;
extern s32 D_01FAD400[];

s32 func_003800D8(s32 n);
void *memset(void *p, s32 c, s32 n);
GteMatrix *Gte_RotMatrixYXZ(GteSVector *r, GteMatrix *m);
GteVector *Gte_ApplyMatrixLV(GteMatrix *m, GteVector *v, GteVector *out);
void func_0035B690(CamKey *c, s32 *out);
void func_0035B858(s32 *v);
s32 func_00356D78(Fighter *a, Fighter *b, s32 *p);
void func_0038EC08(Fighter *a, Fighter *b, s32 a2);

/* ---- model_anim: move log, round-end bonus and tag counters ---- */

/* Fighter fields 0x5C-0x8B seen as arrays per team member (Fighter.unk0 / unk2 index them): two counters per member
 * and the ids of its last 20 moves (fighter.h has unk5C..unk62 and pad64[0x50] there) [V offsets; I names]. */
typedef struct {
    /* 0x00 */ u8 pad0[0x5C];
    /* 0x5C */ s16 unk5C[2];                  /* per member: +1 for a move with flag 0x4000, +2 with flag 0x8000 */
    /* 0x60 */ s16 unk60[2];                  /* per member: moves repeated 3+ times in the last 20 */
    /* 0x64 */ s16 unk64[2][20];              /* per member: ids of the last moves, newest first (0x1305: none) */
} FighterMoveLog;

#define MOVELOG(f) ((FighterMoveLog *)(f))

/* An entry of a character's start-frame list (D_00447480). */
typedef struct {
    /* 0x0 */ s16 unk0;                       /* move id */
    /* 0x2 */ s16 unk2;                       /* frame */
} StartFrame;

/* D_00447480: per Fighter.unk28, a count and a list of StartFrame entries. */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ StartFrame *unk4;
} StartFrameList;

/* 0x20 bytes of the fighter at 0x508 (it holds unk51C..unk524, the position) kept across func_003AFDA8. */
typedef struct {
    s32 w[8];
} FighterPos20;

extern Fighter gFighters[4];
extern TeamMember D_00440940[2][2];
extern u8 D_0043B7F8[];                     /* game options; [11] the bonus table column */
extern StartFrameList D_00447480[];
extern s32 D_00447590[];                    /* factor per round number, 0-9 */
extern const s32 D_00521190[][2][5];        /* per character, two thresholds per option column */
extern s16 D_005F5CC8[2];
extern s32 D_005F5EF4;
extern s32 gRoundTimer;
extern s32 D_01FB6ED0[4];                   /* tag counter per side */
extern s32 D_01FB6EE0[4];                   /* second tag counter per side */

s32 func_00367310(s16 slot);
void func_003AF540(Fighter *f, s32 a, s32 b);
void func_003AFDA8(s32 side, s32 slot, s32 b);
void func_00363068(Fighter *f, s32 n);
void func_00362B38(Fighter *f, s32 a, s32 b, s32 c);
void func_003BB8E8(s32 side);
void func_003809C0(s32 side);
void Battle_RecoverLife(s32 side);
void Snd_PlaySe(s32 id, s32 player);
void Tag_SetCounter(s32 side, s32 n);
s32 Tag_StepCounters(s32 side, s32 n);
void func_003D33F0(s32 side);
void func_003D33F8(s32 side);

/* The model hierarchy (0x3D0C68-0x3D22E0). */
typedef f32 Vu0Matrix[4][4];
typedef f32 FVec4[4] __attribute__((aligned(16)));

/* A model resource set (texture / part bank) at 0x1FB6010: the first word points at its entry count. [I] */
typedef struct {
    /* 0x00 */ u32 *unk0;
    /* 0x04 */ u8 pad4[0x6C];
} ModelRes; /* size 0x70 */

typedef struct ModelNode ModelNode;

/* One node of a model hierarchy (pool D_00445690[64], 0x70 bytes). */
struct ModelNode {
    /* 0x00 */ s32 unk0;               /* camera matrix slot (D_01FB6080[unk0]) */
    /* 0x04 */ s32 unk4;               /* 0: the node's matrix starts from func_00366EB8 (view) */
    /* 0x08 */ s32 unk8;               /* rotation as X, Y, Z matrices instead of sceVu0RotMatrix */
    /* 0x0C */ s32 unkC;               /* drawn */
    /* 0x10 */ s32 unk10;              /* draw list (D_01FB6100 / D_01FB6110) */
    /* 0x14 */ ModelNode *parent;
    /* 0x18 */ ModelNode **children;   /* from the D_01FAD710 pool */
    /* 0x1C */ s32 numChildren;
    /* 0x20 */ s32 maxChildren;
    /* 0x24 */ s32 numWords;
    /* 0x28 */ s32 *words;             /* from the D_01FAD810 pool */
    /* 0x2C */ s32 numFloats;
    /* 0x30 */ f32 *floats;            /* from the D_01FAD810 pool */
    /* 0x34 */ s32 unk34;              /* part index (D_00444C60 kind) */
    /* 0x38 */ ModelRes *res;
    /* 0x3C */ f32 scale;
    /* 0x40 */ FVec4 rot;
    /* 0x50 */ FVec4 trans;
    /* 0x60 */ void (*update)(ModelNode *n);
    /* 0x64 */ u8 pad64[0xC];
}; /* size 0x70 */

/* One node of a model definition (0x40 bytes). */
typedef struct {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ s16 unk6;
    /* 0x08 */ s16 unk8;
    /* 0x0A */ s16 unkA;
    /* 0x0C */ s16 unkC;
    /* 0x0E */ s16 padE;
    /* 0x10 */ FVec4 rot;
    /* 0x20 */ FVec4 trans;
    /* 0x30 */ void (*update)(ModelNode *n);
    /* 0x34 */ void (*init)(ModelNode *n);
    /* 0x38 */ u8 pad38[8];
} ModelNodeDef; /* size 0x40 */

/* A model definition: parent / child links, nodes, per-part kinds and floats. */
typedef struct {
    /* 0x00 */ s32 numLinks;
    /* 0x04 */ s16 (*links)[2];
    /* 0x08 */ s32 numNodes;
    /* 0x0C */ ModelNodeDef *nodes;
    /* 0x10 */ s32 numKinds;
    /* 0x14 */ s32 *kinds;
    /* 0x18 */ s32 numParams;
    /* 0x1C */ f32 *params;
} ModelDef;

/* One entry of the scene model table D_00520FC0 (.rodata, 0x10 bytes). */
typedef struct {
    /* 0x00 */ ModelDef *def;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ void (*draw)(ModelNode *nodes);
    /* 0x0C */ void (*init)(ModelNode *nodes);
} ModelScene;

/* What a drawn node hands to the renderer (0x150 bytes, copied into a DrawObj). */
typedef struct {
    /* 0x000 */ Vu0Matrix m0;          /* camera * world */
    /* 0x040 */ Vu0Matrix m1;
    /* 0x080 */ Vu0Matrix m2;          /* world */
    /* 0x0C0 */ Vu0Matrix m3;
    /* 0x100 */ Vu0Matrix m4;
    /* 0x140 */ s32 unk140;            /* part index */
    /* 0x144 */ s32 unk144;
    /* 0x148 */ s32 unk148;
    /* 0x14C */ s32 pad14C;
} __attribute__((aligned(8))) DrawCtx;

/* A queued draw (pool D_01FADC10[96], 0x160 bytes). */
typedef struct {
    /* 0x000 */ DrawCtx ctx;
    /* 0x150 */ ModelRes *res;
    /* 0x154 */ s32 kind;
    /* 0x158 */ f32 scale;
    /* 0x15C */ s32 pad15C;
} DrawObj;

/* A draw list entry: sort key (depth) and object. */
typedef struct {
    /* 0x0 */ f32 key;
    /* 0x4 */ DrawObj *obj;
} DrawEntry;

void *memcpy(void *d, const void *s, u32 n);
float sinf(float);
float cosf(float);
float asinf(float);
void sceVu0ApplyMatrix(f32 *out, Vu0Matrix m, f32 *v);
void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0RotMatrix(Vu0Matrix m0, Vu0Matrix m1, f32 *r);
void sceVu0TransMatrix(Vu0Matrix m0, Vu0Matrix m1, f32 *t);
void sceVu0RotMatrixX(Vu0Matrix m0, Vu0Matrix m1, f32 rx);
void sceVu0RotMatrixY(Vu0Matrix m0, Vu0Matrix m1, f32 ry);
void sceVu0RotMatrixZ(Vu0Matrix m0, Vu0Matrix m1, f32 rz);
void sceVu0DivVectorXYZ(f32 *out, f32 *in, f32 q);

void Model_Draw(ModelRes *res, DrawObj *obj, void *lights, s32 kind);
void func_00342F98(f32 v);
f32 func_00342FA0(void);
void func_00353D68(Vu0Matrix *out);
void Mtx_Unit(Vu0Matrix m);
void func_00356840(void);
void func_00357230(s32 a, ModelRes *res, s32 b, s32 c, s32 *kinds);
void func_00357258(f32 *values, s32 count);
void func_003621F8(s32 a);
void func_00366EB8(Vu0Matrix m);
s32 func_0039C768(void);
s32 func_003A9698(void);
f32 func_003D80E8(f32 a);

extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s16 gMode;
extern s32 D_00415808[];
extern Vu0Matrix D_00433400;
extern Vu0Matrix D_00433440;
extern Vu0Matrix D_00433480;
extern s32 D_00444C60[];
extern Vu0Matrix D_00444CD0;
extern Vu0Matrix D_004455A0[2];
extern f32 D_00445620[];
extern ModelNode D_00445690[64];
extern const ModelScene D_00520FC0[];
extern s32 D_005F677C;
extern s32 D_005F6EE0;
extern s32 D_005F6EE4;
extern s32 D_005F6EE8;
extern s32 D_005F6EEC;
extern s32 D_005F6EF0;
extern s32 D_005F6EF4;
extern s32 D_005F6EF8;
extern s32 D_005F6EFC;
extern s32 D_005F6F00;
extern s32 D_005F6F04;
extern ModelNode *D_005F6F08;
extern s32 D_005F6F0C;
extern s32 D_005F6F10;
extern Vu0Matrix D_01FAD450[10];
extern Vu0Matrix D_01FAD6D0;
extern ModelNode *D_01FAD710[64];
extern s32 D_01FAD810[256];
extern DrawObj D_01FADC10[96];
extern ModelRes D_01FB6010;
extern Vu0Matrix D_01FB6080[2];
extern s32 D_01FB6100[4];
extern DrawEntry D_01FB6110[4][96];
extern DrawEntry D_01FB6D10[96];

void func_003D0CC8(s32 scene);
void func_003D10D0(s32 i, Vu0Matrix m, f32 scale);
void func_003D1160(ModelNode *n, Vu0Matrix m, f32 scale);
void func_003D1468(ModelNode *n);
void Model_QueueDraw(DrawCtx *ctx, ModelRes *res, s32 kind, f32 scale, s32 list, f32 depth);
void func_003D1920(void);
void func_003D1980(ModelNode *n);
void func_003D19E8(void);
void func_003D1A48(DrawObj *o);
ModelNode *func_003D1A80(void);
ModelNode *func_003D1AB8(s32 n);
void func_003D1AF0(ModelNode *n, ModelNode *parent);
void func_003D1AF8(ModelNode *n, ModelNode *child);
void func_003D1B30(ModelNode *n, FVec4 *rot);
void func_003D1B48(ModelNode *n, FVec4 *trans);
void func_003D1B60(ModelNode *n, void (*update)(ModelNode *));
void func_003D1B68(ModelNode *dst, ModelNode *src);
void func_003D1D90(void);
void func_003D1D98(ModelNode *n, s32 max);
void func_003D1DD8(void);
s32 *func_003D1DE0(u32 size);
void Model_ClearMatrixStack(void);
void Model_PushMatrix(void);
void Model_PopMatrix(void);
void Model_LoadMatrix(Vu0Matrix m);
void Model_MulMatrix(Vu0Matrix m);
void Model_GetMatrix(Vu0Matrix m);
void Model_LoadIdentity(void);
void func_003D1F00(DrawEntry *a, DrawEntry *tmp, s32 lo, s32 hi);
s32 func_003D2110(void);

/* Round-start camera set-up: restarts the shot sequence (D_005F6ED4 = -1), picks one of 14 sequences at random,
 * saves the last hit position of the chosen fighter and sets D_005F6ED8 when either fighter is in a special state
 * (unkE0, move flags 8, damage kind 1-3) [I]. */
void func_003D0660(Fighter *a, Fighter *b) {
    Fighter *f;

    D_005F6ED4 = -1;
    f = a->unkB4 < 0 ? a : b;
    D_005F6ED0 = func_003800D8(14);
    D_01FAD3F0 = *(Vec4i *)&f->unk39C;
    if (a->unkE0 != 0 || b->unkE0 != 0 || a->unkD4 == 8 || b->unkD4 == 8 || (a->unk13D >= 1 && a->unk13D <= 3) ||
        (b->unk13D >= 1 && b->unk13D <= 3)) {
        D_005F6ED8 = 1;
    } else {
        D_005F6ED8 = 0;
    }
}

INCLUDE_RODATA("asm/nonmatchings/model_anim", D_00520C40);
INCLUDE_RODATA("asm/nonmatchings/model_anim", D_00520F88);
/* Round-start camera: picks the next shot of the sequence D_005F6ED0 (D_00520F88 row, D_005F6ED4 step), places the
 * eye at a random rotation and distance around a fighter or the last hit, clamps heights and makes it current. */
void func_003D0748(Fighter *a, Fighter *b) {
    const Unk520C40 *r;
    Fighter *fa;
    Fighter *fb;
    s32 *pos;
    s32 n;
    s32 key[12];
    GteVector v;
    GteSVector rot;
    GteMatrix m;

    D_005F6ED4++;
    r = &D_00520C40[D_00520F88[D_005F6ED0][D_005F6ED4]];
    memset(&v, 0, 16);
    fb = a->unkB4 < 0 ? b : a;
    fa = a->unkB4 < 0 ? a : b;
    if (D_005F6ED8 != 0) {
        pos = &fa->unk51C;
    } else {
        switch (r->unk0) {
        case 0:
            pos = &fb->unk51C;
            break;
        case 2:
        default:
            pos = D_01FAD3F0.unk0;
            break;
        case 1:
            pos = &fa->unk51C;
            break;
        }
    }
    if (gMode == 8 && D_005F6ED4 == 3) {
        pos = &fb->unk51C;
    }
    rot.vx = r->unk4 + r->unkA - func_003800D8(r->unkA * 2);
    if (D_005F6ED8 != 0 && !(gMode == 8 && D_005F6ED4 == 3)) {
        n = func_003800D8(r->unkC * 2);
        rot.vy = r->unk6 + fa->unk3C / 16 + r->unkC - n;
    } else {
        n = func_003800D8(r->unkC * 2);
        rot.vy = r->unk6 + fb->unk3C / 16 + r->unkC - n;
    }
    rot.vz = r->unk8 + r->unkE - func_003800D8(r->unkE * 2);
    n = func_003800D8(r->unk12 * 2);
    v.vz = (r->unk10 + r->unk12 - n) * D_004333D4[0] / 600;
    if (D_005F6ED4 != 0) {
        D_005F6EDC = fa->unk3C;
    }
    Gte_RotMatrixYXZ(&rot, &m);
    Gte_ApplyMatrixLV(&m, &v, &v);
    D_01FAD3D0.unk0[0] = pos[0] + v.vx;
    D_01FAD3D0.unk0[1] = pos[1] + v.vy;
    D_01FAD3D0.unk0[2] = pos[2] + v.vz;
    if (D_01FAD3D0.unk0[1] >= -0x5DB) {
        D_01FAD3D0.unk0[1] = -0x5DC;
    }
    D_01FAD3D0.unkC[0] = pos[0];
    D_01FAD3D0.unkC[1] = pos[1];
    D_01FAD3D0.unkC[2] = pos[2];
    if (D_01FAD3D0.unkC[1] < -0x708) {
        D_01FAD3D0.unkC[1] = -0x708;
    }
    D_01FAD3D0.unk18 = 0;
    D_01FAD3D0.unk1C = 0;
    func_0035B690(&D_01FAD3D0, key);
    key[0] %= 0x1000;
    if (key[0] > 0x238) {
        key[0] = 0x238;
    }
    func_0035B858(key);
}

/* Round-start camera, per frame: eases the target 6/128 a frame towards the shot's target fighter (or none), keeps
 * it at most 0x708 high and makes the key current with its pitch capped [I]. */
void func_003D0A38(Fighter *a, Fighter *b) {
    const Unk520C40 *r;
    Fighter *fa;
    Fighter *fb;
    s32 *pos;
    s32 key[12];

    r = &D_00520C40[D_00520F88[D_005F6ED0][D_005F6ED4]];
    fb = a->unkB4 < 0 ? b : a;
    fa = a->unkB4 < 0 ? a : b;
    if (D_005F6ED8 != 0) {
        pos = &fa->unk51C;
    } else {
        switch (r->unk2) {
        case 2:
        default:
            pos = NULL;
            break;
        case 0:
            pos = &fb->unk51C;
            break;
        case 1:
            pos = &fa->unk51C;
            break;
        }
    }
    if (gMode == 8 && D_005F6ED4 == 3) {
        pos = &fb->unk51C;
    }
    if (pos != NULL) {
        D_01FAD3D0.unkC[0] += (pos[0] - D_01FAD3D0.unkC[0]) * 6 / 128;
        D_01FAD3D0.unkC[1] += (pos[1] - D_01FAD3D0.unkC[1]) * 6 / 128;
        D_01FAD3D0.unkC[2] += (pos[2] - D_01FAD3D0.unkC[2]) * 6 / 128;
    }
    if (D_01FAD3D0.unkC[1] < -0x708) {
        D_01FAD3D0.unkC[1] = -0x708;
    }
    func_0035B690(&D_01FAD3D0, key);
    key[0] %= 0x1000;
    if (key[0] > 0x238) {
        key[0] = 0x238;
    }
    func_0035B858(key);
}

/* Copies the saved hit position D_01FAD3F0 out. */
void func_003D0BD0(Vec4i *out) {
    *out = D_01FAD3F0;
}

/* Getter of D_005F6ED8 (the special-intro flag). */
s32 func_003D0C00(void) {
    return D_005F6ED8;
}

/* Setter of D_005F6ED8. */
void func_003D0C08(s32 n) {
    D_005F6ED8 = n;
}

/* Facing angle of the fighter that is chosen by unkB4 (a if unkB4 < 0, else b). */
s32 func_003D0C10(Fighter *a, Fighter *b) {
    return (a->unkB4 < 0 ? a : b)->unk3C;
}

/* Sets the facing angle of the fighter chosen by unkB4. */
void func_003D0C28(Fighter *a, Fighter *b, s32 v) {
    if (a->unkB4 < 0) {
        a->unk3C = v;
    } else {
        b->unk3C = v;
    }
}

/* func_00356D78 on D_01FAD400 [I]. */
s32 func_003D0C48(Fighter *a, Fighter *b) {
    return func_00356D78(a, b, D_01FAD400);
}

/* Selects the resource set of a scene: stores b, loads the 27 part parameters and the bank (func_00357230). [I] */
void func_003D0C68(s32 a, s32 b) {
    D_005F6F00 = b;
    func_00357258(D_00445620, 27);
    func_00357230(2, &D_01FB6010, a, b, D_00444C60);
}

INCLUDE_RODATA("asm/nonmatchings/model_anim", D_00520FC0);
/* Starts scene `scene` of D_00520FC0: clears the draw lists and every pool, copies the scene's part kinds and
 * parameters into D_00444C60 / D_00445620. */
void func_003D0CC8(s32 scene) {
    s32 i;

    D_005F6F0C = scene;
    D_005F6F10 = 0;
    for (i = 0; i < 4; i++) {
        D_01FB6100[i] = 0;
    }
    func_003D1920();
    func_003D19E8();
    func_003D1D90();
    func_003D1DD8();
    for (i = 0; i < D_00520FC0[D_005F6F0C].def->numKinds; i++) {
        D_00444C60[i] = D_00520FC0[D_005F6F0C].def->kinds[i];
    }
    for (i = 0; i < D_00520FC0[D_005F6F0C].def->numParams; i++) {
        D_00445620[i] = D_00520FC0[D_005F6F0C].def->params[i];
    }
}

#if 0
/* NON_MATCHING: 5 register swap v0/v1 for the D_005F6F10++ counter in the node loop */
/* Builds the node tree of the current scene from its definition: allocates the nodes, fills each from its def, links
 * parents and children, runs each node's init then the scene's init. */
void func_003D0E18(void) {
    ModelDef *def = D_00520FC0[D_005F6F0C].def;
    s32 numNodes = def->numNodes;
    ModelNodeDef *nd = def->nodes;
    s32 numLinks = def->numLinks;
    s16 (*link)[2] = def->links;
    ModelNode *nodes;
    ModelNode *n;
    ModelNodeDef *d;
    ModelNode *p;
    ModelNode *c;
    s32 i;
    s32 k;

    D_005F6F04 = D_005F6F00;
    nodes = func_003D1AB8(numNodes);
    D_005F6F08 = nodes;
    for (i = 0; i < numNodes; i++) {
        if ((u32)nd[i].unk0 < *D_01FB6010.unk0) {
            nodes[i].unk34 = nd[i].unk0;
        }
        d = &nd[i];
        n = &nodes[i];
        n->unk0 = d->unk2;
        n->unk4 = d->unk4;
        n->unk10 = d->unk6;
        k = d->unkA;
        n->numWords = k;
        n->words = func_003D1DE0(k * 4);
        k = d->unkC;
        n->numFloats = k;
        n->floats = (f32 *)func_003D1DE0(k * 4);
        func_003D1B30(n, &d->rot);
        func_003D1B48(n, &d->trans);
        func_003D1B60(n, d->update);
        func_003D1D98(n, d->unk8);
        n->res = &D_01FB6010;
        D_005F6F10++;
    }
    for (i = 0; i < numLinks; i++) {
        p = &nodes[link[0][0]];
        c = &nodes[link[0][1]];
        link++;
        func_003D1AF0(c, p);
        func_003D1AF8(p, c);
    }
    for (i = 0; i < numNodes; i++) {
        if (nd[i].init != NULL) {
            nd[i].init(&nodes[i]);
        }
    }
    if (D_00520FC0[D_005F6F0C].init != NULL) {
        D_00520FC0[D_005F6F0C].init(D_005F6F08);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim", func_003D0E18);
#endif

/* Per frame: the two camera matrices (D_00433480 / D_00433440 times D_00433400), then the scene's draw callback. */
void func_003D1040(void) {
    const ModelScene *s = &D_00520FC0[D_005F6F0C];

    sceVu0MulMatrix(D_01FB6080[0], D_00433480, D_00433400);
    sceVu0MulMatrix(D_01FB6080[1], D_00433440, D_00433400);
    if (s->draw != NULL) {
        s->draw(D_005F6F08);
    }
}

/* Draws node `i` of the scene and its subtree under matrix m with the given scale. */
void func_003D10D0(s32 i, Vu0Matrix m, f32 scale) {
    ModelNode *n;
    Vu0Matrix view;

    if (i < D_005F6F10) {
        n = &D_005F6F08[i];
        Model_ClearMatrixStack();
        Model_LoadIdentity();
        if (n->unk4 == 0) {
            func_00366EB8(view);
            Model_LoadMatrix(view);
        }
        Model_MulMatrix(m);
        n->scale = scale;
        func_003D1468(n);
    }
}

/* Same for any node. */
void func_003D1160(ModelNode *n, Vu0Matrix m, f32 scale) {
    Vu0Matrix view;

    Model_ClearMatrixStack();
    Model_LoadIdentity();
    if (n->unk4 == 0) {
        func_00366EB8(view);
        Model_LoadMatrix(view);
    }
    Model_MulMatrix(m);
    n->scale = scale;
    func_003D1468(n);
}

/* Per frame (main loop, opaque lists 0-1 with a = 0, then lists 2-3 with a = 1): sorts lists 1 and 3 by depth, sends
 * every queued draw to the renderer (Model_Draw) and empties the lists. */
void Model_FlushDrawLists(s32 a) {
    Vu0Matrix lights[2];
    s32 start;
    s32 end;
    s32 i;
    s32 j;
    f32 save;
    DrawObj *o;
    s32 kind;

    func_00353D68(lights);
    lights[0][0][3] = sinf(0.0f);
    lights[0][1][3] = cosf(0.0f);
    if (a != 0) {
        start = 2;
        end = 4;
    } else {
        start = 0;
        end = 2;
    }
    j = 0; /* the draw count shares the register of the inner counter j */
    for (i = start; i < end; i++) {
        j += D_01FB6100[i];
    }
    if (j != 0) {
        func_003621F8(3);
    }
    func_00356840();
    save = func_00342FA0();
    for (i = start; i < end; i++) {
        switch (i) {
            case 0:
            case 2:
                break;
            case 1:
            case 3:
                func_003D1F00(D_01FB6110[i], D_01FB6D10, 0, D_01FB6100[i] - 1);
                break;
        }
        for (j = 0; j < D_01FB6100[i]; j++) {
            o = D_01FB6110[i][j].obj;
            func_00342F98(o->scale);
            kind = D_00444C60[o->ctx.unk140];
            if (kind == 3) {
                Model_Draw(o->res, o, o->ctx.m3, 3);
            } else {
                Model_Draw(o->res, o, lights, kind);
            }
        }
    }
    func_00342F98(save);
    if (a != 0) {
        D_005F6EFC = 0;
    }
    for (i = start; i < end; i++) {
        D_01FB6100[i] = 0;
    }
}

/* Walks a node and its subtree: runs the update callback (only kind-3 parts while paused), pushes the matrix stack,
 * applies the node's rotation / translation and, when drawn, queues it on its draw list with its depth. */
void func_003D1468(ModelNode *n) {
    Vu0Matrix m;
    FVec4 v;
    FVec4 org;
    DrawCtx ctx;
    s32 i;

    memset(org, 0, 16);
    org[3] = 1.0f;
    Model_PushMatrix();
    if (n->update != NULL) {
        if (func_003D2110() == 0 || (n->update != NULL && D_00444C60[n->unk34] == 3)) {
            n->update(n);
        }
    }
    Mtx_Unit(m);
    if (n->unk8 != 0) {
        sceVu0RotMatrixX(m, m, n->rot[0]);
        sceVu0RotMatrixY(m, m, n->rot[1]);
        sceVu0RotMatrixZ(m, m, n->rot[2]);
    } else {
        sceVu0RotMatrix(m, m, n->rot);
    }
    sceVu0TransMatrix(m, m, n->trans);
    Model_MulMatrix(m);
    if (n->unkC != 0) {
        Model_GetMatrix(m);
        sceVu0MulMatrix(ctx.m0, D_01FB6080[n->unk0], m);
        if (ctx.m0[3][3] > 0.0f) {
            sceVu0CopyMatrix(ctx.m2, m);
            if (D_00444C60[n->unk34] == 3) {
                sceVu0CopyMatrix(ctx.m4, D_004455A0[0]);
                sceVu0CopyMatrix(ctx.m1, D_004455A0[1]);
            } else {
                sceVu0MulMatrix(ctx.m1, D_00433400, m);
                Mtx_Unit(m);
                sceVu0RotMatrixY(m, m, asinf(ctx.m2[2][0]));
                ctx.m1[3][3] = 1.0f;
                ctx.m1[3][0] = -m[3][0] + (f32)D_00415808[5];
                ctx.m1[3][1] = -m[3][1] - (f32)D_00415808[6];
                ctx.m1[3][2] = -m[3][2] - (f32)D_00415808[7];
                sceVu0ApplyMatrix(ctx.m1[3], m, ctx.m1[3]);
            }
            switch (n->unk10) {
                case 0:
                case 2:
                    break;
                case 1:
                case 3:
                    sceVu0ApplyMatrix(v, ctx.m0, org);
                    sceVu0DivVectorXYZ(v, v, v[3]);
                    break;
            }
            ctx.unk140 = n->unk34;
            ctx.unk144 = 0;
            ctx.unk148 = 0;
            Model_QueueDraw(&ctx, n->res, D_00444C60[n->unk34], n->scale, n->unk10, v[3]);
        }
    }
    for (i = 0; i < n->numChildren; i++) {
        n->children[i]->scale = n->scale;
        func_003D1468(n->children[i]);
    }
    Model_PopMatrix();
}

/* Queues a draw: copies ctx into the next DrawObj and adds it with its depth to draw list `list`. */
void Model_QueueDraw(DrawCtx *ctx, ModelRes *res, s32 kind, f32 scale, s32 list, f32 depth) {
    s32 *count = &D_01FB6100[list];
    s32 k = *count;
    s32 slot = D_005F6EFC;
    DrawObj *o;

    if (func_0039C768() != 0) {
        return;
    }
    D_005F6EFC++;
    if (D_005F6EFC >= 96) {
        D_005F6EFC = 95;
        return;
    }
    (*count)++;
    if (*count >= 96) {
        *count = 95;
        return;
    }
    D_01FADC10[slot].ctx = *ctx;
    o = &D_01FADC10[slot];
    o->res = res;
    o->kind = kind;
    o->scale = scale;
    D_01FB6110[list][k].key = depth;
    D_01FB6110[list][k].obj = o;
}

/* Resets the node pool. */
void func_003D1920(void) {
    s32 i;

    D_005F677C = 0;
    for (i = 0; i < 64; i++) {
        func_003D1980(&D_00445690[i]);
    }
}

#if 0
/* NON_MATCHING: 2 store order: the scheduler hoists sw zero,0x2C(numFloats) where the original keeps it last */
/* Resets one node. */
void func_003D1980(ModelNode *n) {
    n->unkC = 1;
    n->scale = 1.0f;
    n->unk10 = 2;
    n->parent = NULL;
    n->children = NULL;
    n->maxChildren = 0;
    n->numChildren = 0;
    n->unk0 = 0;
    n->unk4 = 0;
    n->unk8 = 0;
    n->rot[0] = n->rot[1] = n->rot[2] = 0.0f;
    n->trans[0] = n->trans[1] = n->trans[2] = 0.0f;
    n->update = NULL;
    n->numFloats = n->numWords = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim", func_003D1980);
#endif

/* Resets the draw object pool. */
void func_003D19E8(void) {
    s32 i;

    D_005F6EFC = 0;
    for (i = 0; i < 96; i++) {
        func_003D1A48(&D_01FADC10[i]);
    }
}

/* Resets one draw object. */
void func_003D1A48(DrawObj *o) {
    o->ctx.unk140 = 3;
    o->res = &D_01FB6010;
    o->kind = 1;
    o->ctx.unk144 = 0;
    o->ctx.unk148 = 0;
    o->scale = 1.0f;
}

/* Allocates one node. */
ModelNode *func_003D1A80(void) {
    s32 i = D_005F677C;

    D_005F677C++;
    if (D_005F677C >= 64) {
        D_005F677C = 64;
    }
    return &D_00445690[i];
}

/* Allocates n consecutive nodes. */
ModelNode *func_003D1AB8(s32 n) {
    s32 i = D_005F677C;

    D_005F677C += n;
    if (D_005F677C >= 64) {
        D_005F677C = 64;
    }
    return &D_00445690[i];
}

/* Sets a node's parent. */
void func_003D1AF0(ModelNode *n, ModelNode *parent) {
    n->parent = parent;
}

/* Adds a child to a node (up to its capacity). */
void func_003D1AF8(ModelNode *n, ModelNode *child) {
    s32 k = n->numChildren;

    if (k < n->maxChildren) {
        n->children[k] = child;
        n->numChildren = k + 1;
    }
}

/* Sets a node's rotation (xyz). */
void func_003D1B30(ModelNode *n, FVec4 *rot) {
    memcpy(&n->rot, rot, 12);
}

/* Sets a node's translation (xyz). */
void func_003D1B48(ModelNode *n, FVec4 *trans) {
    memcpy(&n->trans, trans, 12);
}

/* Sets a node's update callback. */
void func_003D1B60(ModelNode *n, void (*update)(ModelNode *)) {
    n->update = update;
}

/* Copies a node tree: dst gets src's fields, new child nodes (copied recursively), new word / float arrays. */
static inline void CopyV3(FVec4 *d, FVec4 *s) {
    memcpy(d, s, 12);
}

void func_003D1B68(ModelNode *dst, ModelNode *src) {
    ModelNode *c;
    s32 i;
    s32 k;

    dst->unk0 = src->unk0;
    dst->unk4 = src->unk4;
    dst->unk8 = src->unk8;
    func_003D1D98(dst, src->maxChildren);
    c = func_003D1AB8(src->numChildren);
    for (i = 0; i < src->numChildren; i++) {
        func_003D1AF0(&c[i], dst);
        func_003D1AF8(dst, &c[i]);
    }
    for (i = 0; i < src->numChildren; i++) {
        func_003D1B68(dst->children[i], src->children[i]);
    }
    k = src->numWords;
    dst->numWords = k;
    dst->words = func_003D1DE0(k * 4);
    for (i = 0; i < dst->numWords; i++) {
        dst->words[i] = src->words[i];
    }
    k = src->numFloats;
    dst->numFloats = k;
    dst->floats = (f32 *)func_003D1DE0(k * 4);
    for (i = 0; i < dst->numFloats; i++) {
        dst->floats[i] = src->floats[i];
    }
    dst->unk34 = src->unk34;
    dst->res = src->res;
    dst->unk10 = src->unk10;
    CopyV3(&dst->rot, &src->rot);
    CopyV3(&dst->trans, &src->trans);
    dst->update = src->update;
}

/* Resets the child pointer pool. */
void func_003D1D90(void) {
    D_005F6EF4 = 0;
}

/* Gives a node room for max children from the child pointer pool. */
void func_003D1D98(ModelNode *n, s32 max) {
    s32 i = D_005F6EF4;

    D_005F6EF4 += max;
    if (D_005F6EF4 > 64) {
        max = 0;
        D_005F6EF4 = 63;
    }
    n->maxChildren = max;
    n->children = &D_01FAD710[i];
}

/* Resets the word pool. */
void func_003D1DD8(void) {
    D_005F6EF8 = 0;
}

/* Allocates size bytes (rounded up to words) from the word pool. */
s32 *func_003D1DE0(u32 size) {
    s32 i = D_005F6EF8;

    D_005F6EF8 += size / 4;
    if (size % 4) {
        D_005F6EF8++;
    }
    if (D_005F6EF8 >= 256) {
        D_005F6EF8 = 255;
    }
    return &D_01FAD810[i];
}

/* Empties the matrix stack. */
void Model_ClearMatrixStack(void) {
    D_005F6EF0 = 0;
}

/* Pushes the current matrix. */
void Model_PushMatrix(void) {
    sceVu0CopyMatrix(D_01FAD450[D_005F6EF0], D_01FAD6D0);
    D_005F6EF0++;
    if (D_005F6EF0 >= 10) {
        D_005F6EF0 = 9;
    }
}

/* Pops the current matrix. */
void Model_PopMatrix(void) {
    D_005F6EF0--;
    if (D_005F6EF0 < 0) {
        D_005F6EF0 = 0;
    }
    sceVu0CopyMatrix(D_01FAD6D0, D_01FAD450[D_005F6EF0]);
}

/* Loads the current matrix. */
void Model_LoadMatrix(Vu0Matrix m) {
    sceVu0CopyMatrix(D_01FAD6D0, m);
}

/* Multiplies the current matrix by m. */
void Model_MulMatrix(Vu0Matrix m) {
    sceVu0MulMatrix(D_01FAD6D0, D_01FAD6D0, m);
}

/* Reads the current matrix. */
void Model_GetMatrix(Vu0Matrix m) {
    sceVu0CopyMatrix(m, D_01FAD6D0);
}

/* Sets the current matrix to identity. */
void Model_LoadIdentity(void) {
    Mtx_Unit(D_01FAD6D0);
}

/* Merge sort of a draw list by decreasing key (tmp holds the left half; indices in globals). */
void func_003D1F00(DrawEntry *a, DrawEntry *tmp, s32 lo, s32 hi) {
    s32 mid;

    if (lo < hi) {
        mid = (lo + hi) / 2;
        func_003D1F00(a, tmp, lo, mid);
        func_003D1F00(a, tmp, mid + 1, hi);
        D_005F6EEC = 0;
        for (D_005F6EE0 = lo; D_005F6EE0 <= mid; D_005F6EE0++) {
            tmp[D_005F6EEC++] = a[D_005F6EE0];
        }
        D_005F6EE8 = lo;
        D_005F6EE0 = mid + 1;
        D_005F6EE4 = 0;
        while (D_005F6EE0 <= hi && D_005F6EE4 < D_005F6EEC) {
            if (tmp[D_005F6EE4].key >= a[D_005F6EE0].key) {
                a[D_005F6EE8++] = tmp[D_005F6EE4++];
            } else {
                a[D_005F6EE8++] = a[D_005F6EE0++];
            }
        }
        while (D_005F6EE4 < D_005F6EEC) {
            a[D_005F6EE8++] = tmp[D_005F6EE4++];
        }
    }
}

/* True while the game is paused (the usual three-flag test). */
s32 func_003D2110(void) {
    s32 r = 0;

    if (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
        r = 1;
    }
    return r;
}

/* Empty scene draw callback. */
void func_003D2160(ModelNode *nodes) {
}

/* Scene draw callback: nodes 0 and 1 under the identity. */
void func_003D2168(ModelNode *nodes) {
    Vu0Matrix m;

    Mtx_Unit(m);
    func_003D10D0(0, m, 1.0f);
    func_003D10D0(1, m, 1.0f);
}

/* Scene draw callback: node 10 under the identity except in game mode 10. */
void func_003D21B8(ModelNode *nodes) {
    Vu0Matrix m;

    Mtx_Unit(m);
    if (gMode != 10) {
        func_003D10D0(10, m, 1.0f);
    }
}

/* Node update callback: hides the node and stores current * its local transform in D_00444CD0. [I] */
void func_003D21F8(ModelNode *n) {
    Vu0Matrix m;
    Vu0Matrix cur;

    n->unkC = 0;
    Model_GetMatrix(cur);
    Mtx_Unit(m);
    sceVu0RotMatrix(m, m, n->rot);
    sceVu0TransMatrix(m, m, n->trans);
    sceVu0MulMatrix(D_00444CD0, cur, m);
}

/* Reads the matrix saved by func_003D21F8. */
void func_003D2270(Vu0Matrix m) {
    sceVu0CopyMatrix(m, D_00444CD0);
}

/* Node update callback: advances the z rotation by floats[0] and wraps it (func_003D80E8). */
void func_003D2280(ModelNode *n) {
    n->rot[2] += n->floats[0];
    n->rot[2] = func_003D80E8(n->rot[2]);
}

/* Scene init callback: the spin speeds (first float) of nodes 8 and 9: -0.5 and +0.25 degree per frame. */
void func_003D22C0(ModelNode *nodes) {
    f32 *p;

    p = nodes[8].floats;
    p[0] = -0.008726646f;
    p = nodes[9].floats;
    p[0] = 0.004363323f;
}

/* Starts a camera script: D_005F6F18 = n, D_005F6F14 = -1, then two steps of func_0038EC08 [I]. */
void func_003D22E0(Fighter *a, Fighter *b, s32 n) {
    D_005F6F18 = n;
    D_005F6F14 = -1;
    func_0038EC08(a, b, 0);
    func_0038EC08(a, b, 0);
}

INCLUDE_RODATA("asm/nonmatchings/model_anim", D_00521140);
/* Sets up the scripted camera move `idx` (D_00521140) around fighter b, or with `centre` around the midpoint of both
 * fighters (eye heights x 1.2): start and end target and eye in D_01FB6EB8, the start made current [I]. */
void func_003D2330(Fighter *a, Fighter *b, s32 idx, s32 centre) {
    const Unk521140 *r = &D_00521140[idx];
    s32 key[12];
    GteVector v = { r->unkC[0], r->unkC[1], r->unkC[2], 0 };
    GteSVector rot;
    GteMatrix m;
    GteVector base = { (b->unk708 + a->unk708) / 2, (b->unk70C + a->unk70C) / 2, (b->unk710 + a->unk710) / 2 };

    if (centre == 0) {
        base.vx = b->unk708;
        base.vy = b->unk70C;
        base.vz = b->unk710;
    }
    rot.vx = r->unk0[0];
    rot.vy = r->unk0[1];
    rot.vz = r->unk0[2];
    Gte_RotMatrixYXZ(&rot, &m);
    v.vx = r->unkC[0] * D_00433330.unkA4 / 600;
    v.vy = r->unkC[1] * D_00433330.unkA4 / 600;
    v.vz = r->unkC[2] * D_00433330.unkA4 / 600;
    if (centre != 0) {
        v.vy = v.vy * 0x1333 / 4096;
    }
    Gte_ApplyMatrixLV(&m, &v, &v);
    D_01FB6EB8.unk0[0] = base.vx + v.vx;
    D_01FB6EB8.unk0[1] = base.vy + v.vy;
    D_01FB6EB8.unk0[2] = base.vz + v.vz;
    v.vx = r->unk18[0] * D_00433330.unkA4 / 600;
    v.vy = r->unk18[1] * D_00433330.unkA4 / 600;
    v.vz = r->unk18[2] * D_00433330.unkA4 / 600;
    if (centre != 0) {
        v.vz = v.vz * 0x1333 / 4096;
    }
    Gte_ApplyMatrixLV(&m, &v, &v);
    D_01FB6EB8.unkC[0] = base.vx + v.vx;
    D_01FB6EB8.unkC[1] = base.vy + v.vy;
    D_01FB6EB8.unkC[2] = base.vz + v.vz;
    if (centre == 0) {
        base.vx = a->unk708;
        base.vy = a->unk70C;
        base.vz = a->unk710;
    }
    rot.vx = r->unk6[0];
    rot.vy = r->unk6[1];
    rot.vz = r->unk6[2];
    Gte_RotMatrixYXZ(&rot, &m);
    v.vx = r->unk12[0] * D_00433330.unkA4 / 600;
    v.vy = r->unk12[1] * D_00433330.unkA4 / 600;
    v.vz = r->unk12[2] * D_00433330.unkA4 / 600;
    Gte_ApplyMatrixLV(&m, &v, &v);
    D_01FB6EB8.unk6[0] = base.vx + v.vx;
    D_01FB6EB8.unk6[1] = base.vy + v.vy;
    D_01FB6EB8.unk6[2] = base.vz + v.vz;
    v.vx = r->unk1E[0] * D_00433330.unkA4 / 600;
    v.vy = r->unk1E[1] * D_00433330.unkA4 / 600;
    v.vz = r->unk1E[2] * D_00433330.unkA4 / 600;
    if (centre != 0) {
        v.vz = v.vz * 0x1333 / 4096;
    }
    Gte_ApplyMatrixLV(&m, &v, &v);
    D_01FB6EB8.unk12[0] = base.vx + v.vx;
    D_01FB6EB8.unk12[1] = base.vy + v.vy;
    D_01FB6EB8.unk12[2] = base.vz + v.vz;
    D_01FB6E98.unkC[0] = D_01FB6EB8.unk0[0];
    D_01FB6E98.unkC[1] = D_01FB6EB8.unk0[1];
    D_01FB6E98.unkC[2] = D_01FB6EB8.unk0[2];
    D_01FB6E98.unk0[0] = D_01FB6EB8.unkC[0];
    D_01FB6E98.unk0[1] = D_01FB6EB8.unkC[1];
    D_01FB6E98.unk0[2] = D_01FB6EB8.unkC[2];
    D_01FB6E98.unk18 = 0;
    D_01FB6E98.unk1C = 0;
    func_0035B690(&D_01FB6E98, key);
    func_0035B858(key);
}

/* Scripted camera move, per frame: eases eye and target from start to end along a quarter sine wave over D_005F6F18
 * frames (D_005F64DC left) and makes the key current [I]. */
void func_003D2818(Fighter *a, Fighter *b) {
    CamMove *m = &D_01FB6EB8;
    CamKey *c = &D_01FB6E98;
    s32 s;
    s32 key[12];

    s = gSinTable[(((D_005F6F18 - D_005F64DC) << 10) / D_005F6F18) & 0xFFF];
    c->unk18 = 0;
    c->unk1C = 0;
    c->unkC[0] = m->unk0[0] + (m->unk6[0] - m->unk0[0]) * s / 4096;
    c->unkC[1] = m->unk0[1] + (m->unk6[1] - m->unk0[1]) * s / 4096;
    c->unkC[2] = m->unk0[2] + (m->unk6[2] - m->unk0[2]) * s / 4096;
    c->unk0[0] = m->unkC[0] + (m->unk12[0] - m->unkC[0]) * s / 4096;
    c->unk0[1] = m->unkC[1] + (m->unk12[1] - m->unkC[1]) * s / 4096;
    c->unk0[2] = m->unkC[2] + (m->unk12[2] - m->unkC[2]) * s / 4096;
    func_0035B690(c, key);
    func_0035B858(key);
}

/* The start frame of a move in the fighter's start-frame list (D_00447480[f->unk28]), 1 when it has none [I]. */
s32 func_003D29E8(Fighter *f, s32 move) {
    s32 i;
    StartFrame *e = D_00447480[f->unk28].unk4;

    for (i = 0; i < D_00447480[f->unk28].unk0; i++, e++) {
        if (e->unk0 == move) {
            return e->unk2;
        }
    }
    return 1;
}

/* Logs the move that starts (from Fighter_StepMove's caller): counts flagged moves per member and the moves repeated
 * in the member's last 20 (the round-end bonus func_003D2B88 reads both) [I]. */
void Fighter_LogMove(Fighter *f, Fighter *o) {
    s16 n = f->unk0;
    s16 *log = MOVELOG(f)->unk64[n];
    s32 i;
    s32 cnt;
    u16 id;

    if (D_005F5EF4 == 0) {
        if (f->unk188->unk24 & 0x4000) {
            MOVELOG(f)->unk5C[n] += 1;
        } else if (f->unk188->unk24 & 0x8000) {
            MOVELOG(f)->unk5C[n] += 2;
        }
        if (f->unk188->unk2C != 0) {
            for (i = 19; i > 0; i--) {
                log[i] = log[i - 1];
            }
            id = f->unk184;
            log[0] = id;
            for (i = 1, cnt = 0; i < 20; i++) {
                if ((s16)id == log[i]) {
                    cnt++;
                }
            }
            if (cnt >= 2) {
                MOVELOG(f)->unk60[n]++;
            }
        }
    }
}

/* Clears both members' move logs (0x1305: no move) at the start of a fight. */
void Fighter_ClearMoveLog(Fighter *f) {
    s16 *p = MOVELOG(f)->unk64[0];
    s32 i, j;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 20; j++) {
            p[j] = 0x1305;
        }
        p = MOVELOG(f)->unk64[1];
    }
}

INCLUDE_RODATA("asm/nonmatchings/model_anim", D_00521190);
INCLUDE_RODATA("asm/nonmatchings/model_anim", jtbl_005216E0);
#if 0
/* NON_MATCHING: 203 structure and jump table right (flag |= 1 tests, time = 0 before the switch); register allocation differs (original keeps hi(D_0043B7F8), side*2, the partner's character and &D_005F5CC8 in s0-s3, the leader in t5) */
/* At round end (battle_flow): when the round leader's team beat the per-character thresholds of D_00521190 (scaled
 * by the round factor, and by the time left once already earned) it earns the bonus flag D_005F5CC8[side] with sound
 * 0x1046 [I]. */
void func_003D2B88(Fighter *a, Fighter *b) {
    Fighter *f;
    s32 side, m, p, c0, c1, opt, n, k, time, flag;

    f = (a->unk5A > b->unk5A) ? a : b;
    n = a->unk5A + b->unk5A;
    side = (s16)f->unk20;
    m = f->unk0;
    p = f->unk2;
    c0 = D_00440940[side][m].unk0;
    c1 = D_00440940[side][p].unk0;
    opt = D_0043B7F8[11];
    if (n < 0) {
        n = 0;
    } else if (n >= 10) {
        n = 9;
    }
    k = D_00447590[n];
    if (a->unk5A == b->unk5A) {
        return;
    }
    if (f->unk418 != 0) {
        return;
    }
    if (D_0043B7F8[0x1A] != 2 || D_0043B7F8[0xE] == 0) {
        return;
    }
    if (opt >= 5) {
        opt = 4;
    }
    if (D_005F5CC8[side] == 0) {
        flag = 0;
        if (MOVELOG(f)->unk5C[m] >= D_00521190[c0][0][opt] * n * k / 4096) {
            flag |= 1;
        }
        if (MOVELOG(f)->unk5C[p] >= D_00521190[c1][0][opt] * n * k / 4096) {
            flag |= 1;
        }
        if (MOVELOG(f)->unk60[m] >= D_00521190[c0][1][opt] * n * k / 4096) {
            flag |= 1;
        }
        if (MOVELOG(f)->unk60[p] >= D_00521190[c1][1][opt] * n * k / 4096) {
            flag |= 1;
        }
        if (flag) {
            D_005F5CC8[side] = 1;
            Snd_PlaySe(0x1046, 0);
        } else {
            D_005F5CC8[side] = 0;
        }
    } else {
        time = 0;
        switch (opt) {
        case 0:
            time = 0x870;
            break;
        case 1:
            time = 0xB04;
            break;
        case 2:
            time = 0x10A4;
            break;
        case 3:
            time = 0x1608;
            break;
        case 4:
            time = 0x1B30;
            break;
        }
        flag = 0;
        if (MOVELOG(f)->unk5C[m] > D_00521190[c0][0][opt] * n * 6 / 10 * (time - gRoundTimer) / time) {
            flag |= 1;
        }
        if (MOVELOG(f)->unk5C[p] > D_00521190[c1][0][opt] * n * 6 / 10 * (time - gRoundTimer) / time) {
            flag |= 1;
        }
        if (MOVELOG(f)->unk60[m] > D_00521190[c0][1][opt] * n * 6 / 10 * (time - gRoundTimer) / time) {
            flag |= 1;
        }
        if (MOVELOG(f)->unk60[p] > D_00521190[c1][1][opt] * n * 6 / 10 * (time - gRoundTimer) / time) {
            flag |= 1;
        }
        if (flag) {
            D_005F5CC8[side] = 1;
            Snd_PlaySe(0x1046, 0);
        } else {
            D_005F5CC8[side] = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/model_anim", func_003D2B88);
#endif

/* Threshold 0 of the fighter's character for the option column D_0043B7F8[11]. */
s32 func_003D2F70(Fighter *f) {
    return D_00521190[f->unk2A][0][D_0043B7F8[11]];
}

/* Threshold 0 of the partner's character. */
s32 func_003D2FA0(Fighter *f) {
    return D_00521190[D_00440940[(s16)f->unk20][f->unk2].unk0][0][D_0043B7F8[11]];
}

/* Threshold 1 of the fighter's character. */
s32 func_003D2FF8(Fighter *f) {
    return D_00521190[f->unk2A][1][D_0043B7F8[11]];
}

/* Threshold 1 of the partner's character. */
s32 func_003D3028(Fighter *f) {
    return D_00521190[D_00440940[(s16)f->unk20][f->unk2].unk0][1][D_0043B7F8[11]];
}

/* Threshold 1 of the fighter's character scaled by the round number (rounds won by both + 1, 0-9) [I]. */
s32 func_003D3080(Fighter *f, Fighter *o) {
    s32 n = f->unk5A + o->unk5A + 1;

    if (n < 0) {
        n = 0;
    } else if (n >= 10) {
        n = 9;
    }
    return D_00521190[f->unk2A][1][D_0043B7F8[11]] * n * D_00447590[n] / 4096;
}

/* The same for the partner's character [I]. */
s32 func_003D3108(Fighter *f, Fighter *o) {
    s32 c = D_00440940[(s16)f->unk20][f->unk2].unk0;
    s32 n = f->unk5A + o->unk5A + 1;

    if (n < 0) {
        n = 0;
    } else if (n >= 10) {
        n = 9;
    }
    return D_00521190[c][1][D_0043B7F8[11]] * n * D_00447590[n] / 4096;
}

/* Clears the tag counters of both sides (round init). */
void Tag_ResetCounters(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_01FB6ED0[i] = 0;
        D_01FB6EE0[i] = 0;
    }
}

/* Sets the tag counter of a side (Fighter_TagSwap sets 2). */
void Tag_SetCounter(s32 side, s32 n) {
    D_01FB6ED0[side] = n;
}

/* Sets the second tag counter of a side. */
void Tag_SetCounter2(s32 side, s32 n) {
    D_01FB6EE0[side] = n;
}

/* Counts both tag counters of a side down by n; returns the first while it runs, else 100 while the second runs,
 * else 0 (0: no tag in progress). */
s32 Tag_StepCounters(s32 side, s32 n) {
    s32 a = D_01FB6ED0[side];

    if (a > 0) {
        a = D_01FB6ED0[side] -= n;
    }
    if (D_01FB6EE0[side] > 0) {
        D_01FB6EE0[side] -= n;
    }
    if (a > 0) {
        return a;
    }
    if (D_01FB6EE0[side] > 0) {
        return 100;
    }
    return 0;
}

/* The tag swap of a side (from Fighter_StepMove when a tag move starts, and from func_003F0008): the fighter's active
 * and partner member indices unk0 / unk2 flip, the life and red life come from the incoming member's record, then
 * the character and costume of that member are loaded (func_0035C8D0, func_0035C850) and its body set up
 * (func_00368168, func_00377F58 body spheres, ...); practice mode (game type 7) also resets its state. */
void Fighter_TagSwap(s32 side) {
    Fighter *f = &gFighters[side];
    s16 n = f->unk2;
    s16 m;

    m = f->unk0 ^= 1;
    f->unk2 ^= 1;
    f->unk3F4 = D_00440940[side][n].unkC;
    f->unk3F8 = D_00440940[side][n].unk10;
    func_003C1A78(side);
    Tag_SetCounter(side, 2);
    func_0035C8D0(f, D_00440940[side][m].unk0, D_00440940[side][m].unk4);
    func_0035C850(f, 1);
    func_00368168(f);
    func_00377F58(f);
    func_003465F0();
    func_00344318();
    Fx_LoadFighterTex(f);
    func_00370E20(f, 1);
    func_0038ADF8(side);
    if (D_0043B7F8[0] == 7) {
        func_0037A970(f);
        func_0037EA08();
    }
}

/* Empty hook called before func_003D33F8 (a stripped debug call) [I]. */
void func_003D33F0(s32 side) {
}

/* Finishes a tag of a side: reloads the incoming member's body and motion (func_00367310 / func_003AF540 /
 * func_003AFDA8) keeping the fighter's position, restarts its motion and clears unkB134 [I]. */
void func_003D33F8(s32 side) {
    Fighter *f = &gFighters[side];
    s32 m = func_00367310(f->unk22);
    FighterPos20 pos;

    func_003AF540(f, 0, m);
    pos = *(FighterPos20 *)((u8 *)f + 0x508);
    func_003AFDA8(side, f->unk22, m);
    *(FighterPos20 *)((u8 *)f + 0x508) = pos;
    func_00363068(f, 1);
    func_00362B38(f, 0, 1, 0);
    f->unkB134 = 0;
    func_003BB8E8(side);
    func_003809C0(side);
}

/* Per frame (battle_flow): a side whose tag counter is idle recovers life (Battle_RecoverLife) unless the round is
 * over; a side whose counter reaches 1 finishes its tag (func_003D33F8). */
void func_003D3528(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        switch (Tag_StepCounters(i, 0)) {
        case 0:
            if (D_005F5EF4 == 0) {
                Battle_RecoverLife(i);
            }
            break;
        case 1:
            func_003D33F0(i);
            func_003D33F8(i);
            break;
        }
    }
}

/* At round end: finishes a pending tag of each side and clears the tag counters. */
void func_003D35B8(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        switch (Tag_StepCounters(i, 0)) {
        case 0:
            break;
        case 1:
            func_003D33F0(i);
            func_003D33F8(i);
            break;
        }
        Tag_SetCounter(i, 0);
    }
}

