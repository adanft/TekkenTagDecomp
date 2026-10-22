/* 0x38BF98-0x396D00 (module camera_stage, docs/game_overview.md section 4; docs/systems/stage.md): the fight camera,
 * its scripted paths and its per-stage settings. Tekken Tag has no walls, ring-outs or floor geometry in the
 * simulation: the floor is the plane y = 0 and the fight is kept near the origin by func_00372E98 (battle_flow). The
 * simulation depends on this module through four channels (docs/systems/stage.md section 5): the camera phase
 * (Cam_Update is called with &gRoundPhase every simulated frame and advances it), the camera angles D_00415808
 * (Fighter.unk48 and the command quadrants), the random generators it advances (newlib rand(), the CPU's
 * func_003800D8 and an LCG on D_005F5CF4) and the camera state D_00433330 read by battle_flow.
 * .rodata 0x507EE0-0x509D00 is this file's subsegment; every table and jump table in it is included in place with
 * INCLUDE_RODATA (force_not_migration in data.txt), except the jump tables of functions that are C. Its .lit4 words
 * (0x5F5174-0x5F519C, ten float constants of Cam_SetDistance, func_00392F80, func_003932F0 and func_00393550) are a
 * subsegment too: while those functions are assembly, LIT4_WORD in front of their INCLUDE_ASM emits each word in
 * place under the label the .s file uses; a function that becomes C drops its LIT4_WORD lines and the compiler emits
 * the same words from its float literals. Every NON_MATCHING draft (#if 0) compiles when enabled. */
#include "common.h"
#include "fighter.h"
#include "gte.h"

typedef f32 Vu0Matrix[4][4];

/* The camera state (0x433330, .data) [V layout of the fields used; I meanings]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4[5];
    /* 0x18 */ s32 unk18[5];
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s32 unk44;
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ s32 unk58;
    /* 0x5C */ s32 unk5C;
    /* 0x60 */ struct FighterMove *unk60;
    /* 0x64 */ s32 unk64;
    /* 0x68 */ s32 unk68;
    /* 0x6C */ s32 unk6C;
    /* 0x70 */ s32 unk70;
    /* 0x74 */ s32 unk74;
    /* 0x78 */ s32 unk78;
    /* 0x7C */ s32 unk7C[2]; /* the move that last started a hit camera, per fighter slot (Fighter.unk20) */
    /* 0x84 */ s32 unk84;
    /* 0x88 */ s32 unk88;
    /* 0x8C */ s32 unk8C;
    /* 0x90 */ s32 unk90;
    /* 0x94 */ s32 unk94;
    /* 0x98 */ s32 unk98;
    /* 0x9C */ s32 unk9C;
    /* 0xA0 */ s32 unkA0;
    /* 0xA4 */ s32 unkA4;
    /* 0xA8 */ s32 unkA8;
    /* 0xAC */ s32 unkAC;
    /* 0xB0 */ s32 unkB0;
    /* 0xB4 */ s32 unkB4;
    /* 0xB8 */ s32 unkB8;
    /* 0xBC */ s32 unkBC;
    /* 0xC0 */ s32 unkC0;
    /* 0xC4 */ s32 unkC4;
    /* 0xC8 */ s32 unkC8;
} Unk433330;

/* 0x50-byte records (.data 0x433010, 10 of them): 10 steps {value, frames} [I: idle camera sequences]. */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
} Unk433010Step;

typedef struct {
    /* 0x00 */ Unk433010Step unk0[10];
} Unk433010;

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
} Unk8BC6B8;

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ f32 unk20;
} Unk8BC5F8;

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u8 pad14[0xC];
    /* 0x20 */ Vu0Matrix unk20;
} Unk8BC620;

typedef struct {
    /* 0x00 */ s32 unk0[7];
} Unk8B90B8; /* 0x1C bytes */

typedef struct {
    /* 0x0 */ s16 unk0[5];
} Unk507FE0; /* 0xA bytes, one per stage */

/* A position as copied whole (four words, 4-byte aligned). */
typedef struct {
    /* 0x0 */ s32 unk0[4];
} Vec4i;

extern Unk433330 D_00433330;
extern Unk433010 D_00433010[];
extern s32 D_00433520[2][4];
extern u16 D_00433560[];
extern Unk8BC6B8 D_008BC6B8;
extern Unk8BC5F8 D_008BC5F8;
extern Unk8BC620 D_008BC620;
extern Unk8B90B8 D_008B90B8[];
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8[2][4];
} Unk8BC710;

extern Unk8BC710 D_008BC710;
/* The output of the camera script decoder (.bss 0x8B92D0): eye and target as s16 offsets, a flag and a distance. */
typedef struct {
    /* 0x00 */ s16 unk0[4];
    /* 0x08 */ s16 unk8[4];
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
} Unk8B92D0;

extern Unk8B92D0 D_008B92D0;
extern const Unk507FE0 D_00507FE0[];
extern const s16 D_00508B60[];
extern const u16 D_00509C10[];
extern s32 D_008BC578[];
extern u16 D_005F6C50;
extern u16 D_005F6C52;
extern s32 D_005F5CD0;
extern s32 D_005F6144;
extern Fighter gFighters[4];
extern Fighter D_00884EE0;
extern s16 *D_005F6C28;
extern s32 D_005F61CC;
extern s32 D_005F6134;
extern s32 D_005F6C20;
extern s32 D_005F6140;
extern s32 D_005F6C0C;
extern s32 D_005F6C10;
extern s32 D_005F6C14;
extern u16 D_005F5CB6;
extern u16 D_005F5CBC;

s32 func_003800D8(s32 n);
void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);
s32 func_003ABCC0(void);
void func_00366EB8(Vu0Matrix m);
s32 func_003934D0(s32 n, s32 a1, Vu0Matrix m);
s32 func_00390E08(Fighter *a, Fighter *b);
s32 func_0038DEA0(Fighter *a, Fighter *b);
s32 func_00366108(s32 dx, s32 dz);
s32 Cam_SetDistance(s32 dist);
u8 CamScript_Step(s32 n, Unk8B92D0 *out, s32 a2);
s32 func_0039C0E0(s32 a0, s32 a1);
void func_0035BB58(s32 a0);
void func_0035BD20(s32 a0, s32 a1);
s32 func_0038FD90(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7);
s32 func_00391358(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7);
void func_003959C8(Fighter *a, Fighter *b, s32 a2);
s32 func_00390FA8(s32 a0, s32 a1, s32 a2, s32 a3);


/* 0x14-byte records (.rodata 0x508908) [I: camera presets by number]. */
typedef struct {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4[4];
    /* 0x0C */ s16 unkC[4];
} Unk508908;

extern const Unk508908 D_00508908[];

/* The preset of a camera number (func_00393C78, inlined by its neighbours): numbers below 0x1E through the table
 * D_00508B60, 0x1E-0x231 give -1, -2 ... (the number counted from 0x1E), others 0 [V code; I meaning]. */
static inline s32 Cam_PresetIndex(u32 n) {
    if (n < 0x1E) {
        return D_00508B60[n];
    }
    if (n < 0x232) {
        return -(n - 0x1E + 1);
    }
    return 0;
}

/* A joint position of a fighter (the skeleton, Fighter + 0x668, 0x50 bytes per joint). */
#define CAM_JOINT(f, j) (((FighterJoints *)(f))->unk668[j].unk0.unk0)

/* 0x3C-byte records (.bss 0x8B9148, 5 of them) [I: camera paths]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
} Unk8B9148;

typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u16 unk2;
} Unk508120;

/* 8-byte entries reached through the records of D_00434C78. */
typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ s16 unk4[2];
} Unk434C78Entry;

typedef struct {
    /* 0x0 */ Unk434C78Entry *unk0;
    /* 0x4 */ s16 unk4; /* entry count */
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s16 unk8;
    /* 0xA */ s16 unkA;
} Unk434C78;

extern Unk8B9148 D_008B9148[];
extern const Unk508120 D_00508120[];
extern Unk434C78 D_00434C78[];
extern s16 D_005F6C18;
extern u16 D_005F6C1A;
extern s32 D_005F6188;
extern s32 D_005F6198;
extern s32 D_005F619C;
extern s32 D_005F6C54;
extern s32 D_005F6C58;

s32 func_00393550(void);
void func_0036DDD0(Fighter *a, Fighter *b);
s32 func_0039BDA8(s16 *a, s16 *b, s16 *c);
void func_0035B858(s32 *v);
s32 func_00394110(s32 n, s32 *a, s32 *b);
s32 func_0038F7F8(s32 a0, s32 a1, s32 *out, s32 a3, s32 a4);
s32 func_00392F80(s32 n, s32 frame);
s32 func_0038E308(Unk8B90B8 *out, Vec4i *from, Vec4i *to, s32 roll, s32 dist);
s32 func_0038E460(Unk8B90B8 *src, s32 i);
s32 func_00391058(Fighter *a, Fighter *b);

/* The entries of D_005095C0's lists: a chance threshold and four pointers to camera scripts. */
typedef struct {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 *unk4[4];
} Unk5095C0Entry;

typedef struct {
    /* 0x0 */ Unk5095C0Entry *unk0;
    /* 0x4 */ s32 unk4;
} Unk5095C0;

typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ Unk8B90B8 unk8;
} Unk8BC690;

typedef struct {
    /* 0x0 */ u32 unk0;
    /* 0x4 */ u32 unk4;
} U32Pair;

/* Two words copied whole (8 bytes, ldl / sdl): a value per fighter, built as an initialised local pair. */
typedef struct {
    /* 0x0 */ s32 v[2];
} CamPair;

/* The same pair as plain arrays: an initialised local array (`S32x2 kind = { x, y };`) is built in a temporary and
 * copied whole, as the original does. */
typedef s32 S32x2[2];
typedef Fighter *FighterPtrs2[2];

/* The sine and cosine of an 18-bit angle (0x40000 per turn; gSinTable has 0x1000 entries per turn). */
#define CAM_SIN(a) gSinTable[((a) / 64) & 0xFFF]
#define CAM_COS(a) gSinTable[(((a) / 64) & 0xFFF) + 0x400]

extern const Unk5095C0 D_005095C0[];
extern Unk8BC690 D_008BC690;
extern const s16 D_00509C2C[];
extern s32 D_005F61A4;
extern s32 gRoundNumber;

s32 rand(void);
void *memset(void *p, s32 c, s32 n);
s32 abs(s32 x);
f32 atan2f(f32 y, f32 x);
f32 sqrtf(f32 x);
s32 func_003BB4D8();
u32 Math_Isqrt(u32 x);
s32 func_0038BF40(s32 kind);
s32 func_0038BFE0(void);
s32 func_0038DC68(Unk8B90B8 *c, s32 *out);
s32 func_00393F60(Fighter *f, Fighter *o, s32 side);
s32 func_003927D8(void);
s32 func_0038E408(Unk8B90B8 *dst, s32 i);
extern Unk8B90B8 D_008BC698;
extern s32 D_00415808[];
extern s32 D_005F61A0;

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
} Unk8BC6E8;

extern Unk8BC6E8 D_008BC6E8;
extern s32 D_005F6170[2];
extern s32 D_005F6190;
extern s32 D_005F6194;

void Cam_Reset(void);
s32 func_0038C188(void);
void func_003967E0(void);
s32 Cam_SetKey(s32 i);
void func_0038EC08(Fighter *a, Fighter *b, s32 a2);
s32 Cam_PickReaction(u32 kind, s32 side, s32 *idx);
s32 func_003928B8(s32 n, Fighter *f, Fighter *o);
s32 func_00390F48(Fighter *f);
s32 func_0038D3D0(s32 side, Fighter *f, Fighter *o);

extern s32 D_005F6C24;
extern s32 D_005F6184;
extern s32 D_005F618C;
extern s32 *D_008BC4F8[];
extern s32 D_005F61A8;
extern s32 D_005F61AC;
extern s32 D_005F61B4;
extern s32 D_005F61BC;
extern s32 D_005F61C0;
extern s32 D_005F61C8;
extern s32 D_00436230[];
extern s32 D_00436250[];
extern s32 D_00436258[];
extern s32 D_00436278[];

s32 *func_003B8790(void);
void exit(s32 status);
s32 func_0035D238(s32 id, void *dst, s32 size);
void func_00393870(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f);
s32 func_003403B8(void);
s32 func_003912C8(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6);
s32 func_00391310(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6);
s32 func_003954E8(void);
s32 func_0038E6D0(Unk8B90B8 *a, Unk8B90B8 *b, s32 t);
extern s32 D_005F61B0;
extern s32 D_005F61C4;

/* 0x20-byte camera record of the special ending (.bss 0x8BC6C8) [I]. */
typedef struct {
    /* 0x00 */ s32 unk0[4];
    /* 0x10 */ s32 unk10[4];
} Unk8BC6C8;

extern Unk8BC6C8 D_008BC6C8;
extern const s16 D_00509C20[][7];
extern s16 gSinTable[];
extern s32 D_005F5CF4;
extern u16 D_005F5CF6;
extern s32 D_005F61B8;

s32 func_003D7DA8(s32 *pos);
s32 func_003985D8(s32 x);
void func_0035B690(Unk8BC6C8 *c, s32 *out);
s32 func_00395CD8(s32 a0, s32 a1, s32 a2);
s32 func_00395EE0(Fighter *a, Fighter *b, s32 kind);
s32 func_00395D20(s32 a0, s32 a1, s32 a2);
s32 func_00396320(Fighter *a, Fighter *b, s32 start);
s32 func_00396620(s32 a0, s32 a1, s32 a2);
s32 func_003963C0(u32 n, Fighter *a, Fighter *b);
s32 func_00396930(void);
void func_00396970(Fighter *a, Fighter *b);
void func_003967C8(void);

/* The camera script reader (.bss 0x8B92E8): a 0x3200-byte buffer filled 0x40 bytes at a time. */
typedef struct {
    /* 0x0000 */ u8 unk0[0x3200];
    /* 0x3200 */ u8 *unk3200; /* read pointer */
    /* 0x3204 */ u8 *unk3204; /* write pointer */
    /* 0x3208 */ s32 unk3208; /* bytes buffered */
} Unk8B92E8;

extern Unk8B92E8 D_008B92E8;
extern s32 D_008B9280[8];
extern s32 D_008B92A0[8];
extern s16 D_008B92C0[8];
extern const s32 D_00508130[];
extern s32 D_005F6C1C;
extern s32 D_008BC680[3];
extern s32 D_005F6C2C;
extern const s32 D_00509700[][3];
extern const s32 D_00509820[][3];

u8 *func_003B8778(void);
void func_0035C698(void *dst, s32 src, s32 size);
s32 func_00394300(s32 n, s32 k);
s32 func_003927F0(Fighter *a, Fighter *b);
s32 func_00393BF0(Fighter *f);
s32 func_00393C78(u32 n);
s32 func_00392B68(Fighter *f, Fighter *o);
s32 func_00392888(s32 a, s32 b);
s32 func_0038D1B8(s32 side, Fighter *a1, Fighter *a2);
s32 func_0038D290(s32 side, Fighter *a1, Fighter *f);
extern s32 D_005F6138;
extern s32 D_005F613C;

s32 func_0038C1E8(Fighter *a, Fighter *b);
s32 func_0038CFC0(Fighter *a, Fighter *b);
s32 func_00395400(s32 again, Fighter *a, Fighter *b);
void func_00396CF8(void);
void func_00391048(void);
void func_003927B8(void);
void func_0035C2B0(void);
s32 func_00395718(s32 a0, s32 a1, s32 a2);
s32 func_00395750(void);
s32 func_00395C90(s32 a0, s32 a1, s32 a2);
void func_003D0A38(Fighter *a, Fighter *b);
void func_003D2818(Fighter *a, Fighter *b);
s32 func_0038BF98(void);
s32 func_0038C048(s32 a0);
s32 func_00393788(Fighter *a, Fighter *b);

s32 func_0038BF98(void) {
    s32 n = func_003800D8(10);

    D_005F6C0C = n;
    D_005F6C10 = 0;
    D_005F6C14 = D_00433010[n].unk0[0].unk4;
    return D_00433010[n].unk0[0].unk0;
}

s32 func_0038BFE0(void) {
    s32 busy = 0;
    s32 t;

    if (gFighters[0].unkE0 != 0 || gFighters[1].unkE0 != 0) {
        busy = 1;
    }
    t = func_0038DEA0(&gFighters[0], &D_00884EE0);
    return busy == 0 && t == 0;
}

s32 func_0038C048(s32 a0) {
    s32 n;

    if (--D_005F6C14 < 0 && func_0038BFE0() != 0) {
        n = D_005F6C10;
        n++;
        D_005F6C10 = n;
        if (n >= 10) {
            D_005F6C10 = 0;
            n = 0;
        }
        D_005F6C14 = D_00433010[D_005F6C0C].unk0[n].unk4;
        return func_0038BF40(D_00433010[D_005F6C0C].unk0[n].unk0);
    }
    if (D_005F6C14 < 0xF0) {
        D_005F6144 = 0;
    }
    return a0;
}

/* NON_MATCHING: 4 lines: the D_005F6140 / unkC0 / unkC4 stores are scheduled earlier than in the original (D_00433334 is the unk4 array's own symbol) */
#if 0
extern s32 D_00433334[]; /* D_00433330.unk4, read by its own symbol */

void Cam_Reset(void) {
    s32 i;
    s32 *a = D_00433334;
    s32 *b = a + 5;

    for (i = 0; i < 5; i++) {
        a[i] = b[i] = 0;
    }
    D_00433330.unk4[0] = 0x1000;
    D_00433330.unkC8 = 4;
    D_00433330.unk0 = 1;
    D_00433330.unk38 = -1;
    D_00433330.unk44 = -1;
    D_00433330.unk40 = -1;
    D_00433330.unk3C = -1;
    D_00433330.unk50 = -1;
    D_00433330.unk4C = -1;
    D_00433330.unkC0 = -1;
    D_00433330.unkBC = -1;
    D_005F6140 = 1;
    D_00433330.unk2C = 0;
    D_00433330.unkB8 = 0;
    D_00433330.unk7C[1] = 0;
    D_00433330.unkC4 = 0;
    D_00433330.unk7C[0] = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", Cam_Reset);
#endif

s32 func_0038C188(void) {
    struct FighterMove *m = gFighters[0].unkC0;

    if (m == NULL || gFighters[1].unkC0 == NULL) {
        return -1;
    }
    if ((m->unk24 >> 23) == 0xAE) {
        return 0;
    }
    return (gFighters[1].unkC0->unk24 >> 23) == 0xAE ? 1 : -1;
}

INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_00507EE0);
INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_00507F20);

/* NON_MATCHING: same size (2 lines): in case 11's end block the store of 5 to unk3C is scheduled ahead of the mode store (unk0 = 5) instead of after unk40, as in func_0038D290 / func_0038D3D0 */
#if 0
extern s32 D_005F6150[2];
extern s32 D_005F6158[2];
s32 func_00393E08(Fighter *a, Fighter *b, s32 which, s32 side);
s32 func_003910D0(Fighter *a, Fighter *b);
s32 func_00393CB8(Fighter *a, Fighter *b);
s32 func_003940B0(void);
s32 func_00394628(void);
s32 func_003953B0(void);
void func_0038EAA0(s32 i);
s32 func_00391170(Fighter *a, Fighter *b, s32 a2, s32 a3, s32 swap);
s32 func_00394648(Fighter *a, Fighter *b, s32 a2, s32 a3, s32 swap);

#define CAM_KIND(f) ((f)->unkC0->unk24 >> 23)

/* [V] The fight camera: runs camera mode D_00433330.unk0 (1 default orbit, which starts the
 * reaction cameras; 2-0xF preset / script / blend modes moving the five key weights unk4[]), then builds keys 0 / 1 from
 * the weights, picks the current key by the set of weights above 0 (one key, or two keys interpolated) and saves the
 * weights as the previous frame's (unk18[]). */
s32 func_0038C1E8(Fighter *a, Fighter *b) {
    Unk8B90B8 k0;
    Unk8B90B8 k1;
    Unk8B90B8 k2;
    Unk8B90B8 k3;
    Unk8B90B8 k4;
    Unk8B90B8 k5;
    Unk8B90B8 k6;
    Unk8B90B8 k7;
    s32 side;
    s32 lead;
    s32 n;
    s32 t14;
    s32 w14;
    s32 t12;
    s32 w12;
    s32 t7;
    s32 w7;
    s32 t10;
    s32 w10;
    s32 t13;
    s32 w13;
    s32 w3;
    s32 t4;
    s32 w4;
    s32 t9;
    s32 w9;
    s32 t11;
    s32 w11;
    s32 t5;
    s32 w5;
    s32 c;
    s32 r;
    s32 flag;
    s32 i;
    s32 bit;
    s32 mask;

    if (D_005F6140 == 0) {
        return 0;
    }
    func_0038EC08(a, b, 0);
    if (--D_00433330.unkC4 <= 0) {
        D_00433330.unkC4 = 0;
    }
    if (--D_00433330.unkC8 <= 0) {
        D_00433330.unkC8 = 0;
    }
    if ((D_00433330.unk44 == 0 && a->unkE0 == 0) || (D_00433330.unk44 == 1 && b->unkE0 == 0)) {
        D_00433330.unk44 = -1;
    }
    D_00433330.unkC0 = func_0038C188();
    if (D_00433330.unk7C[(s16)a->unk20] != 0 && D_00433330.unk7C[(s16)a->unk20] != (s32)a->unkC0) {
        D_00433330.unk7C[(s16)a->unk20] = 0;
    }
    if (D_00433330.unk7C[(s16)b->unk20] != 0 && D_00433330.unk7C[(s16)b->unk20] != (s32)b->unkC0) {
        D_00433330.unk7C[(s16)b->unk20] = 0;
    }
    if (D_00433330.unk0 != 1) {
        D_00433330.unkC4 = 0x1E;
    }

    switch (D_00433330.unk0) {
    case 1:
        D_00433330.unk54 = -1;
        D_00433330.unk3C = -1;
        D_00433330.unk50 = -1;
        D_00433330.unkBC = -1;
        if (func_0038DEA0(a, b) != 0) {
            break;
        }
        if (D_00433330.unk44 >= 0) {
            break;
        }
        if (a->unkE0 > 0 || b->unkE0 > 0) {
            if ((u32)(CAM_KIND(a) - 0x11) < 0x58 && (u32)(CAM_KIND(b) - 0x11) < 0x58) {
                break;
            }
            side = a->unkE0 < 1;
            lead = -1;
            D_00433330.unk30 = side;
            D_00433330.unk44 = side;
            if (side == 0) {
                if (D_005F6150[0] != D_005F6150[1]) {
                    lead = !(D_005F6150[0] < D_005F6150[1]);
                } else if (D_005F6158[0] != D_005F6158[1]) {
                    lead = !(D_005F6158[0] < D_005F6158[1]);
                }
            } else {
                if (D_005F6150[1] != D_005F6150[0]) {
                    lead = !(D_005F6150[1] < D_005F6150[0]);
                } else if (D_005F6158[1] != D_005F6158[0]) {
                    lead = !(D_005F6158[1] < D_005F6158[0]);
                }
            }
            D_00433330.unk34 = lead;
            D_00433330.unk50 = -1;
            n = func_00393E08(a, b, D_00433330.unk30, lead);
            if (n < 0) {
                D_00433330.unk54 = -1;
            } else if (D_00433330.unk58 < 0) {
                D_00433330.unk40 = n;
                D_00433330.unk0 = 6;
                D_00433330.unk38 = -1;
                D_00433330.unk3C = n;
            } else {
                func_003910D0(a, b);
                D_00433330.unk40 = n;
                D_00433330.unk38 = 0x10;
                D_00433330.unk0 = 3;
                D_00433330.unk3C = n;
            }
        }
        break;
    case 2:
        D_00433330.unk4[1] = 0x1000;
        D_00433330.unk4[2] = 0;
        D_00433330.unk4[0] = 0;
        if (func_0038DEA0(a, b) != 0) {
            D_00433330.unk4[4] = 0x1000;
            D_00433330.unk54 = -1;
            D_00433330.unk4[1] = 0;
            D_00433330.unk4[2] = 0;
            D_00433330.unk4[0] = 0;
            break;
        }
        if ((D_00433330.unk30 == 0 && func_00393CB8(a, b) != 0) ||
            (D_00433330.unk30 == 1 && func_00393CB8(b, a) != 0)) {
            n = func_003940B0();
            D_00433330.unk40 = n;
            D_00433330.unk0 = 4;
            D_00433330.unk3C = n;
        }
        break;
    case 8:
        D_00433330.unk4[1] = 0x1000;
        c = D_00433330.unk4C;
        c--;
        D_00433330.unk4[2] = 0;
        D_00433330.unk4[0] = 0;
        D_00433330.unk4C = c;
        if (func_00394628() >= c) {
            n = func_00394628();
            func_003953B0();
            D_00433330.unk40 = n;
            D_00433330.unk0 = 9;
            D_00433330.unk3C = n;
        }
        break;
    case 14:
        if (D_00433330.unk38 >= 0) {
            if (--D_00433330.unk38 == 0) {
                func_0038EAA0(3);
            }
        } else {
            t14 = D_00433330.unk3C;
            w14 = (t14 << 12) / D_00433330.unk40;
            D_00433330.unk3C = --t14;
            D_00433330.unk4[1] = 0;
            D_00433330.unk4C--;
            D_00433330.unk4[2] = 0;
            D_00433330.unk4[0] = w14;
            D_00433330.unk4[4] = 0x1000 - w14;
            if (t14 <= 0) {
                D_00433330.unk0 = 0xF;
            }
        }
        break;
    case 15:
        D_00433330.unk4[4] = 0x1000;
        c = D_00433330.unk4C;
        c--;
        D_00433330.unk4[1] = 0;
        D_00433330.unk4[0] = 0;
        D_00433330.unk4[2] = 0;
        D_00433330.unk4C = c;
        if (func_00394628() >= c) {
            n = func_00394628();
            D_00433330.unk0 = 5;
            D_00433330.unk40 = n;
            D_00433330.unk3C = n;
        }
        break;
    case 12:
        if (D_00433330.unk38 >= 0) {
            D_00433330.unk38--;
        } else {
            t12 = D_00433330.unk3C;
            w12 = (t12 << 12) / D_00433330.unk40;
            D_00433330.unk3C = --t12;
            D_00433330.unk4[0] = 0;
            D_00433330.unk4C--;
            D_00433330.unk4[2] = 0;
            D_00433330.unk4[4] = w12;
            D_00433330.unk4[1] = 0x1000 - w12;
            if (t12 <= 0) {
                D_00433330.unk0 = 8;
            }
        }
        break;
    case 7:
        if (D_00433330.unk38 >= 0) {
            D_00433330.unk38--;
        } else {
            t7 = D_00433330.unk3C;
            w7 = (t7 << 12) / D_00433330.unk40;
            D_00433330.unk3C = --t7;
            D_00433330.unk4[4] = 0;
            D_00433330.unk4C--;
            D_00433330.unk4[2] = 0;
            D_00433330.unk4[0] = w7;
            D_00433330.unk4[1] = 0x1000 - w7;
            if (t7 <= 0) {
                D_00433330.unk0 = 8;
            }
        }
        break;
    case 10:
        if (D_00433330.unk38 < 0) {
            t10 = D_00433330.unk3C;
            w10 = (t10 << 12) / D_00433330.unk40;
            D_00433330.unk3C = --t10;
            D_00433330.unk4[4] = 0;
            D_00433330.unk4C--;
            D_00433330.unk4[2] = 0;
            D_00433330.unk4[0] = w10;
            D_00433330.unk4[1] = 0x1000 - w10;
            if (t10 <= 0) {
                D_00433330.unk0 = 8;
            }
        } else {
            D_00433330.unk38--;
            if (CAM_KIND(a) == 3 || CAM_KIND(b) == 3) {
                D_00433330.unk54 = -1;
                D_00433330.unk38 = -1;
                D_00433330.unk0 = 1;
            }
        }
        break;
    case 13:
        if (D_00433330.unk38 >= 0) {
            D_00433330.unk38--;
            if (CAM_KIND(a) == 3 || CAM_KIND(b) == 3) {
                D_00433330.unk54 = -1;
                D_00433330.unk38 = -1;
                D_00433330.unk0 = 1;
            }
        } else {
            t13 = D_00433330.unk3C;
            w13 = (t13 << 12) / D_00433330.unk40;
            D_00433330.unk3C = --t13;
            D_00433330.unk4[0] = 0;
            D_00433330.unk4C--;
            D_00433330.unk4[2] = 0;
            D_00433330.unk4[4] = w13;
            D_00433330.unk4[1] = 0x1000 - w13;
            if (t13 <= 0) {
                D_00433330.unk0 = 8;
            }
        }
        break;
    case 3:
        if (D_00433330.unk38 >= 0) {
            D_00433330.unk38--;
            if (CAM_KIND(a) == 3 || CAM_KIND(b) == 3) {
                D_00433330.unk54 = -1;
                D_00433330.unk38 = -1;
                D_00433330.unk0 = 1;
            }
        } else {
            w3 = (D_00433330.unk3C << 12) / D_00433330.unk40;
            D_00433330.unk4[0] = w3;
            D_00433330.unk4[1] = 0x1000 - w3;
            if ((D_00433330.unk30 == 0 && func_00393CB8(a, b) != 0) ||
                (D_00433330.unk30 == 1 && func_00393CB8(b, a) != 0)) {
                n = func_003940B0();
                D_00433330.unk40 = n;
                D_00433330.unk3C = n;
                if (D_00433330.unk4[1] == 0) {
                    D_00433330.unk0 = 1;
                } else if (D_00433330.unk4[0] == 0) {
                    D_00433330.unk0 = 4;
                } else {
                    D_00433330.unk0 = 5;
                    D_00433330.unk4[4] = 0x1000;
                    D_00433330.unk4[1] = 0;
                    D_00433330.unk4[0] = 0;
                    func_0038EAA0(3);
                }
            } else if (--D_00433330.unk3C < 0) {
                D_00433330.unk0 = 2;
            }
        }
        break;
    case 6:
        flag = 0;
        if (D_00433330.unkB8 != 0) {
            D_00433330.unkB8 = 0;
            D_00433330.unk38 = -3;
        } else if (D_00433330.unk38 == -1) {
            D_00433330.unk38 = -2;
            flag = 1;
        }
        r = func_00391170(a, b, flag, D_00433330.unkA4 * 0x76C / 256, D_00433330.unk30);
        if (r < 0) {
            return 0;
        }
        D_00433330.unk4[0] = r << 5;
        D_00433330.unk4[2] = 0x1000 - (r << 5);
        D_00433330.unk4[1] = 0;
        if (CAM_KIND(a) == 3 || CAM_KIND(b) == 3) {
            func_003940B0();
            D_00433330.unk40 = 8;
            D_00433330.unk3C = 8;
            if (r == 0x80) {
                D_00433330.unk0 = 1;
            } else {
                D_00433330.unk4[4] = 0x1000;
                D_00433330.unk0 = 5;
                D_00433330.unk4[2] = 0;
                D_00433330.unk4[0] = 0;
                func_0038EAA0(3);
            }
            D_00433330.unk38 = -1;
            D_00433330.unk54 = -1;
        } else if ((D_00433330.unk30 == 0 && a->unkE0 < 0) || (D_00433330.unk30 == 1 && b->unkE0 < 0)) {
            D_00433330.unk40 = 0x23;
            D_00433330.unk3C = 0x23;
            D_00433330.unk4[4] = 0x1000;
            D_00433330.unk0 = 5;
            D_00433330.unk4[2] = 0;
            D_00433330.unk4[0] = 0;
            func_0038EAA0(3);
            D_00433330.unk38 = -1;
            D_00433330.unk54 = -1;
        } else {
            if (D_00433330.unk38 == -2 && r == 0) {
                D_00433330.unk38 = -3;
            } else if (D_00433330.unk38 == -3 && r == 0x80) {
                n = func_003940B0();
                D_00433330.unk0 = 1;
                D_00433330.unk40 = n;
                D_00433330.unk3C = n;
                D_00433330.unk38 = -1;
                func_003927D8();
            }
            if (func_0038DEA0(a, b) != 0) {
                D_00433330.unk54 = -1;
                D_00433330.unk4[4] = 0x1000;
                D_00433330.unk4[1] = 0;
                D_00433330.unk4[2] = 0;
                D_00433330.unk4[0] = 0;
            }
        }
        break;
    case 4:
        t4 = D_00433330.unk3C;
        w4 = (t4 << 12) / D_00433330.unk40;
        D_00433330.unk3C = --t4;
        D_00433330.unk4[1] = w4;
        D_00433330.unk4[0] = 0x1000 - w4;
        if (t4 < 0) {
            D_00433330.unk40 = -1;
            D_00433330.unk3C = -1;
            D_00433330.unk0 = 1;
        }
        break;
    case 9:
        t9 = D_00433330.unk3C;
        w9 = (t9 << 12) / D_00433330.unk40;
        t9--;
        D_00433330.unk4[4] = 0;
        D_00433330.unk4[2] = 0;
        D_00433330.unk4C--;
        D_00433330.unk3C = t9;
        D_00433330.unk4[1] = w9;
        D_00433330.unk4[0] = 0x1000 - w9;
        if (t9 <= 0) {
            D_00433330.unk0 = 1;
            D_00433330.unk4[0] = 0x1000;
            D_00433330.unk4[1] = 0;
            D_00433330.unk40 = -1;
            D_00433330.unk3C = -1;
            D_00433330.unk4[2] = 0;
            D_00433330.unk50 = -1;
        }
        break;
    case 11:
        t11 = D_00433330.unk3C;
        w11 = (t11 << 12) / D_00433330.unk40;
        t11--;
        D_00433330.unk4[0] = 0;
        D_00433330.unk4[2] = 0;
        D_00433330.unk4C--;
        D_00433330.unk3C = t11;
        D_00433330.unk4[1] = w11;
        D_00433330.unk4[4] = 0x1000 - w11;
        if (t11 <= 0) {
            D_00433330.unk0 = 5;
            D_00433330.unk50 = -1;
            D_00433330.unk4[4] = 0x1000;
            D_00433330.unk4[0] = 0;
            D_00433330.unk40 = 5;
            D_00433330.unk4[2] = 0;
            D_00433330.unk4[1] = 0;
            D_00433330.unk3C = 5;
        }
        break;
    case 5:
        t5 = D_00433330.unk3C;
        w5 = (t5 << 12) / D_00433330.unk40;
        D_00433330.unk3C = --t5;
        D_00433330.unk4[4] = w5;
        D_00433330.unk4[0] = 0x1000 - w5;
        if (t5 < 0) {
            D_00433330.unk0 = 1;
        }
        break;
    default:
        Cam_Reset();
        break;
    }

    if (D_00433330.unk4[1] > 0) {
        flag = D_00433330.unk18[1] < 1;
        if (D_00433330.unk50 < 0) {
            func_00391170(a, b, flag, D_00433330.unkA4 * 0x76C / 256, D_00433330.unk30);
        } else {
            func_00394648(a, b, flag, D_00433330.unkA4 * 0x76C / 256, D_00433330.unk30);
        }
    }
    if (D_00433330.unk4[0] > 0) {
        func_00390FA8((s32)a, (s32)b, D_00433330.unk18[0] < 1, D_00433330.unkA4 * 0x76C / 256);
    }
    bit = 1;
    mask = 0;
    for (i = 0; i < 5; i++, bit <<= 1) {
        if (D_00433330.unk4[i] > 0) {
            mask |= bit;
        }
    }
    switch (mask) {
    case 1:
        Cam_SetKey(0);
        break;
    case 2:
        Cam_SetKey(1);
        break;
    case 4:
        Cam_SetKey(2);
        break;
    case 3:
        func_0038E408(&k0, 0);
        func_0038E408(&k1, 1);
        func_0038E6D0(&k0, &k1, D_00433330.unk4[0]);
        break;
    case 5:
        func_0038E408(&k2, 0);
        func_0038E408(&k3, 2);
        func_0038E6D0(&k2, &k3, D_00433330.unk4[0]);
        break;
    case 16:
        Cam_SetKey(4);
        break;
    case 17:
        func_0038E408(&k4, 0);
        func_0038E408(&k5, 4);
        func_0038E6D0(&k4, &k5, gSinTable[(D_00433330.unk4[0] / 2 - 0x400) & 0xFFF] / 2 + 0x800);
        break;
    case 18:
        func_0038E408(&k6, 1);
        func_0038E408(&k7, 4);
        func_0038E6D0(&k6, &k7, gSinTable[(D_00433330.unk4[1] / 2 - 0x400) & 0xFFF] / 2 + 0x800);
        break;
    default:
        Cam_Reset();
        break;
    }
    for (i = 0; i < 5; i++) {
        D_00433330.unk18[i] = D_00433330.unk4[i];
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038C1E8);
#endif

s32 func_0038CFC0(Fighter *a, Fighter *b) {
    Unk8B90B8 c0;
    Unk8B90B8 c2;
    s32 i;
    s32 bit;
    s32 mask;

    if (D_005F6140 == 0) {
        return 0;
    }
    func_0038EC08(a, b, 0);
    D_00433330.unk54 = -1;
    func_003927B8();
    D_00433330.unk4[2] = func_003954E8();
    D_00433330.unk4[0] = 0x1000 - D_00433330.unk4[2];
    if (D_00433330.unk4[0] > 0) {
        func_00390FA8((s32)a, (s32)b, D_00433330.unk18[0] < 1, D_00433330.unkA4 * 0x76C / 256);
    }
    bit = 1;
    mask = 0;
    for (i = 0; i < 5; i++, bit <<= 1) {
        if (D_00433330.unk4[i] > 0) {
            mask |= bit;
        }
    }
    switch (mask) {
    case 1:
        Cam_SetKey(0);
        break;
    case 4:
        Cam_SetKey(2);
        break;
    case 5:
        func_0038E408(&c0, 0);
        func_0038E408(&c2, 2);
        func_0038E6D0(&c0, &c2, D_00433330.unk4[0]);
        break;
    default:
        Cam_Reset();
        break;
    }
    for (i = 0; i < 5; i++) {
        D_00433330.unk18[i] = D_00433330.unk4[i];
    }
    if (D_00433330.unk4[2] <= 0) {
        D_00433330.unk0 = 1;
        D_00433330.unk4[0] = 0x1000;
        D_00433330.unk4[2] = 0;
        return 1;
    }
    return 0;
}

s32 func_0038D1B8(s32 side, Fighter *a1, Fighter *a2) {
    s32 r;

    D_00433330.unk34 = side;
    D_00433330.unk44 = D_00433330.unk30;
    D_00433330.unk50 = -1;
    r = func_00393F60(a1, a2, side);
    if (r < 0) {
        D_00433330.unk54 = -1;
    } else if (D_00433330.unk58 < 0) {
        D_00433330.unk38 = -1;
        D_00433330.unk0 = 6;
        D_00433330.unk40 = r;
        D_00433330.unk3C = r;
    } else {
        D_00433330.unk38 = 0x10;
        D_00433330.unk0 = 2;
        D_00433330.unk40 = 1;
        D_00433330.unk3C = 1;
        func_003927D8();
    }
    D_00433330.unk48 = 1;
    D_00433330.unk38 = -1;
    D_00433330.unk4[1] = 0x1000;
    D_00433330.unk4[2] = 0;
    D_00433330.unk4[0] = 0;
    return 0;
}

/* NON_MATCHING: 2 lines: the store of r to unk3C is scheduled after the mode store (unk0 = 6) instead of before the unkB8 store (as func_0038D3D0); every other store matches */
#if 0
s32 func_0038D290(s32 side, Fighter *a1, Fighter *f) {
    s32 r;

    D_00433330.unk34 = side;
    D_00433330.unk44 = D_00433330.unk30;
    D_00433330.unk50 = -1;
    r = func_00393F60(a1, f, side);
    if (r < 0) {
        D_00433330.unk54 = -1;
    } else if (D_00433330.unk58 < 0) {
        D_00433330.unk68 = -((s16)(f->unk1A / 16) + 0xA00) & 0xFFF;
        D_00433330.unk8C = f->unk51C;
        D_00433330.unk90 = f->unk8;
        D_00433330.unk94 = f->unk524;
        D_00433330.unkA0 = 0;
        D_00433330.unk84 = 0;
        func_00392B38(-D_00433330.unk58);
        D_00433330.unk38 = -1;
        D_00433330.unk0 = 6;
        D_00433330.unk3C = D_00433330.unk40 = r;
        D_00433330.unkB8 = 1;
    } else {
        D_00433330.unk38 = 0x10;
        D_00433330.unk0 = 2;
        D_00433330.unk40 = 1;
        D_00433330.unk3C = 1;
        func_003927D8();
    }
    D_00433330.unk48 = 1;
    D_00433330.unk38 = -1;
    D_00433330.unk4[2] = 0x1000;
    D_00433330.unk4[0] = 0;
    D_00433330.unk4[1] = 0;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038D290);
#endif

/* NON_MATCHING: 2 lines: the store of r to unk3C is scheduled after the mode store (unk0 = 6) instead of before the unkB8 store; every other store matches */
#if 0
s32 func_0038D3D0(s32 side, Fighter *a1, Fighter *f) {
    s32 r;
    s16 a;
    s32 t;

    D_00433330.unk34 = side;
    D_00433330.unk44 = D_00433330.unk30;
    D_00433330.unk50 = -1;
    r = func_00393F60(a1, f, side);
    if (r < 0) {
        D_00433330.unk54 = -1;
    } else if (D_00433330.unk58 < 0) {
        a = f->unk1A / 16;
        t = (0x400 - a) & 0xFFF;
        D_00433330.unk68 = t;
        D_00433330.unk8C = f->unk51C;
        D_00433330.unk90 = f->unk8;
        D_00433330.unk94 = f->unk524;
        D_00433330.unk84 = 0;
        func_00392B38(-D_00433330.unk58);
        D_00433330.unk38 = -1;
        D_00433330.unk0 = 6;
        D_00433330.unk3C = D_00433330.unk40 = r;
        D_00433330.unkB8 = 1;
    } else {
        D_00433330.unk38 = 0x10;
        D_00433330.unk0 = 2;
        D_00433330.unk40 = 1;
        D_00433330.unk3C = 1;
        func_003927D8();
    }
    D_00433330.unk48 = 1;
    D_00433330.unk38 = -1;
    D_00433330.unk4[1] = 0x1000;
    D_00433330.unk4[2] = 0;
    D_00433330.unk4[0] = 0;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038D3D0);
#endif

s32 func_0038D508(void) {
    D_005F6134 = 0;
    return 0;
}


/* The camera's step of the frame, called by func_0036DDD0 from Battle_UpdateFrame with &gRoundPhase. The phase selects
 * the camera: 0 picks the intro camera (a random preset of D_00433010 in the demo, else the default blend; phase 1
 * runs the blend and goes to 0xD when it ends), 4 and 0xD the fight camera func_0038C1E8, 5-0xC the four demo camera
 * kinds (set-up then step, the next phase from func_0038C048), 0xE / 0xF the win camera (then 0x10), 0x11 / 0x12 the
 * K.O. camera of the side that lost, 2 / 3 two cameras of model_anim. It writes the phase back [V code; I names]. */
s32 Cam_Update(Fighter *a, Fighter *b, s32 *phase) {
    s32 n;

    D_00433330.unk2C = 0;
    if (D_005F5CBC != 0 && func_003ABCC0() == 0 && func_00393788(a, b) != 0) {
        return 0;
    }
    D_00433330.unkC0 = func_0038C188();
    switch (*phase) {
    case 0:
        if (D_005F5CBC != 0 && func_003ABCC0() == 0) {
            switch (func_0038BF98()) {
            case 6:
                goto start_a;
            case 8:
                goto start_b;
            case 10:
                goto start_c;
            case 12:
                goto start_d;
            case 13:
                goto start_fight;
            }
        }
        Cam_Reset();
        func_00390E08(a, b);
        func_00395400(1, a, b);
        func_003927D8();
        func_00396CF8();
        func_00391048();
        func_003927B8();
        *phase = 1;
    case 1:
        if (func_0038CFC0(a, b) != 0) {
            *phase = 0xD;
        }
        break;
    case 4:
    start_fight:
        Cam_Reset();
        func_00390E08(a, b);
        func_00395400(1, a, b);
        func_003927D8();
        func_00396CF8();
        func_00391048();
        func_003927B8();
        D_005F6144 = 1;
    case 13:
        func_0038C1E8(a, b);
        if (D_005F5CBC == 0 || func_003ABCC0() != 0) {
            break;
        }
        n = 0xD;
        goto next;
    case 5:
    start_a:
        func_00396CF8();
        Cam_Reset();
        func_003963C0(D_005F6138 & 3, a, b);
        func_00390E08(a, b);
        func_003927D8();
        func_00396CF8();
        D_005F6134 = 1;
        D_005F6138++;
        func_00395D20((s32)a, (s32)b, -1);
        D_005F6144 = 0;
    case 6:
        func_0038EC08(a, b, 0);
        func_00395D20((s32)a, (s32)b, 0);
        Cam_SetKey(0);
        n = 6;
        goto next;
    case 7:
    start_b:
        func_00396CF8();
        Cam_Reset();
        func_003963C0(D_005F6138 & 3, a, b);
        func_00390E08(a, b);
        func_003927D8();
        func_00396CF8();
        D_005F6134 = 1;
        D_005F6138++;
        func_00395EE0(a, b, -1);
        D_005F6144 = 0;
    case 8:
        func_0038EC08(a, b, 0);
        func_00395EE0(a, b, 0);
        Cam_SetKey(0);
        n = 8;
        goto next;
    case 9:
    start_c:
        func_00396CF8();
        Cam_Reset();
        func_003963C0(D_005F6138 & 3, a, b);
        func_00390E08(a, b);
        func_003927D8();
        func_00396CF8();
        D_005F6134 = 1;
        D_005F6138++;
        func_00396320(a, b, -1);
        D_005F6144 = 0;
    case 10:
        func_0038EC08(a, b, 0);
        func_00396320(a, b, 0);
        Cam_SetKey(0);
        n = 0xA;
        goto next;
    case 11:
    start_d:
        func_00396CF8();
        Cam_Reset();
        func_003963C0(D_005F6138 & 3, a, b);
        func_00390E08(a, b);
        func_003927D8();
        func_00396CF8();
        D_005F6134 = 1;
        D_005F6138++;
        func_00396620((s32)a, (s32)b, -1);
        D_005F6144 = 0;
    case 12:
        func_0038EC08(a, b, 0);
        func_00396620((s32)a, (s32)b, 0);
        Cam_SetKey(0);
        n = 0xC;
    next:
        *phase = func_0038C048(n);
        break;
    case 14:
    case 15:
        func_0035C2B0();
        func_00395718(*phase != 0xE ? (s32)b : (s32)a, *phase != 0xE ? (s32)a : (s32)b, D_005F613C & 1);
        *phase = 0x10;
    case 16:
        func_0038EC08(a, b, 0);
        func_00395750();
        break;
    case 17:
    case 18: {
        Fighter *f;

        func_0035C2B0();
        f = *phase == 0x11 ? a : b;
        func_0038EC08(a, b, 0);
        func_00395C90((s32)f, *phase != 0x11 ? (s32)a : (s32)b, f->unk3F4 == 0);
        break;
    }
    case 3:
        func_003D0A38(a, b);
        break;
    case 2:
        func_003D2818(a, b);
        break;
    case 19:
        break;
    }
    return 0;
}

void sceVu0ViewScreenMatrix(Vu0Matrix m, f32 scrz, f32 ax, f32 ay, f32 cx, f32 cy, f32 zmin, f32 zmax, f32 nearz,
                            f32 farz);
extern Vu0Matrix D_00433440;
extern Vu0Matrix D_00433480;

/* Sets the projection distance, clamped to the stage's minimum and maximum (D_00433330.unkA8 / unkAC), into the two
 * view-screen matrices (the second with a nearer far clip), and widens both by 1.25 [V code; I meaning]. */
s32 Cam_SetDistance(s32 dist) {
    dist = dist < D_00433330.unkA8 ? D_00433330.unkA8 : (dist > D_00433330.unkAC ? D_00433330.unkAC : dist);
    D_005F5CD0 = dist;
    func_00398540(dist);
    sceVu0ViewScreenMatrix(D_00433440, dist, 1.0f, 1.0f, 2048.0f, 2048.0f, 0.0f, 262140.0f, 64.0f, 196608.0f);
    sceVu0ViewScreenMatrix(D_00433480, dist, 1.0f, 1.0f, 2048.0f, 2048.0f, 0.0f, 196604.0f, 64.0f, 196608.0f);
    D_00433440[0][0] *= 1.25f;
    D_00433440[1][1] *= 1.25f;
    D_00433480[0][0] *= 1.25f;
    D_00433480[1][1] *= 1.25f;
    return 0;
}

INCLUDE_RODATA("asm/nonmatchings/stage", D_00507FE0);

s32 func_0038DB98(void) {
    u32 n = D_005F5CB6;

    if (n >= 0x19) {
        n = 0;
    }
    return D_00507FE0[n].unk0[0];
}

/* Loads the stage's camera values (D_00507FE0, 5 per stage, stage 0 above 0x18): default, minimum and maximum distance,
 * script scaling flag and camera style, then sets the projection with the default (Cam_SetDistance) [V code; I meanings]. */
s32 func_0038DBC0(void) {
    u32 n = D_005F5CB6;

    if (n >= 0x19) {
        n = 0;
    }
    D_00433330.unkA4 = D_00507FE0[n].unk0[0];
    D_00433330.unkA8 = D_00507FE0[n].unk0[1];
    D_00433330.unkAC = D_00507FE0[n].unk0[2];
    D_00433330.unkB0 = D_00507FE0[n].unk0[3];
    D_00433330.unkB4 = D_00507FE0[n].unk0[4];
    Cam_SetDistance(D_00507FE0[n].unk0[0]);
    return 0;
}

void func_0038DC40(void) {
    D_00433330.unkAC = 0x5DC;
    D_00433330.unkA8 = 0;
}

void func_0038DC58(s32 dist) {
    D_00433330.unkA4 = dist;
    Cam_SetDistance(dist);
}

/* NON_MATCHING: 25 lines: same size; the loads of the key copy and of D_00433520 and the products are scheduled in another order */
#if 0
/* The point camera key c looks at: its position moved along its pitch and its yaw turned by a quarter (0x10000 of
 * 0x40000) by the mean distance from the key to the fighters' tracking points D_00433520; returns that distance
 * [V code; I meaning]. */
s32 func_0038DC68(Unk8B90B8 *c, s32 *out) {
    Unk8B90B8 k = *c;
    s32 dx0, dy0, dz0;
    s32 dx1, dy1, dz1;
    s32 pitch;
    s32 yaw;
    s32 cp;
    s32 sp;
    s32 sy;
    s32 cy;
    s32 d1;
    s32 r;

    k.unk0[4] += 0x10000;
    dx1 = D_00433520[1][0] - k.unk0[0];
    dy0 = D_00433520[0][1] - k.unk0[1];
    dy1 = D_00433520[1][1] - k.unk0[1];
    dz0 = D_00433520[0][2] - k.unk0[2];
    dz1 = D_00433520[1][2] - k.unk0[2];
    pitch = (k.unk0[3] / 64) & 0xFFF;
    yaw = (k.unk0[4] / 64) & 0xFFF;
    dx0 = D_00433520[0][0] - k.unk0[0];
    d1 = dx1 * dx1 + dy1 * dy1 + dz1 * dz1;
    sp = gSinTable[pitch];
    cp = gSinTable[pitch + 0x400];
    sy = gSinTable[yaw];
    cy = gSinTable[yaw + 0x400];
    r = (Math_Isqrt(dx0 * dx0 + dy0 * dy0 + dz0 * dz0) + Math_Isqrt(d1)) >> 1;
    out[0] = k.unk0[0] + (r * cy / 4096) * cp / 4096;
    out[1] = k.unk0[1] + r * sp / 4096;
    out[2] = k.unk0[2] + (r * sy / 4096) * cp / 4096;
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038DC68);
#endif

s32 func_0038DE88(void) {
    return D_00433330.unk0 == 1;
}

INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_005080E0);

/* NON_MATCHING: 4 instructions long (210 lines, mostly relocations): the original cross-jumps the facing tail of cases 11/14 into cases 3/6 and stores unk38 once after the func_003953F0 test */
#if 0
extern s32 D_005F6150[2];
extern s32 D_005F6158[2];
s32 func_003953B0(void);
void func_0038EAA0(s32 i);
s32 func_00394610();
s32 func_00394638(void);
s32 func_003953F0(void);
s32 func_003943A8(Fighter *a, Fighter *b);

/* Starts a reaction camera when a move hits or throws (unless one is held in unk7C[0] / unk7C[1]): the side of the attack
 * (func_003943A8), the fighter in the lead by the words D_005F6150 / D_005F6158 (-1 on a tie), then by the kind of the stage's
 * reaction (func_003953B0, 3-17) the camera mode (0xA, 0xD, 0xE / 0xF, 0xC / 8, 7 / 8), its counters and facing;
 * returns 1 when a camera was set [V code; I meaning]. */
s32 func_0038DEA0(Fighter *a, Fighter *b) {
    s32 side;
    s32 lead;
    s32 n;

    if (D_00433330.unk7C[0] != 0) {
        return 0;
    }
    if (D_00433330.unk7C[1] != 0) {
        return 0;
    }
    side = func_003943A8(a, b);
    if (side < 0) {
        return 0;
    }
    D_00433330.unk30 = side;
    lead = -1;
    if (side == 0) {
        if (D_005F6150[0] != D_005F6150[1]) {
            lead = !(D_005F6150[0] < D_005F6150[1]);
        } else if (D_005F6158[0] != D_005F6158[1]) {
            lead = !(D_005F6158[0] < D_005F6158[1]);
        }
    } else {
        if (D_005F6150[1] != D_005F6150[0]) {
            lead = !(D_005F6150[1] < D_005F6150[0]);
        } else if (D_005F6158[1] != D_005F6158[0]) {
            lead = !(D_005F6158[1] < D_005F6158[0]);
        }
    }
    D_00433330.unk34 = lead;
    n = func_00394610(a, b, side, lead);
    if (n < 0) {
        goto end;
    }
    if (D_00433330.unk58 < 0) {
        goto end;
    }
    switch (func_003953B0()) {
    case 3:
    case 6:
        D_00433330.unk38 = 0x10;
        D_00433330.unk0 = 0xA;
        D_00433330.unk40 = n;
        D_00433330.unk3C = n;
        D_00433330.unk4C = D_00433330.unk50 = func_00394638();
        D_00433330.unk74 = (D_00433330.unk30 != 0 ? b->unk3C : a->unk3C) / 16;
        func_0038EAA0(3);
        break;
    case 11:
    case 14:
        D_00433330.unk38 = 0x10;
        D_00433330.unk0 = 0xD;
        D_00433330.unk40 = n;
        D_00433330.unk3C = n;
        D_00433330.unk4C = D_00433330.unk50 = func_00394638();
        D_00433330.unk74 = (D_00433330.unk30 != 0 ? b->unk3C : a->unk3C) / 16;
        func_0038EAA0(3);
        break;
    case 10:
        D_00433330.unk38 = func_003953F0() != 0 ? func_003953F0() : -1;
        if (n == 1 && D_00433330.unk38 == -1) {
            D_00433330.unk3C = 0;
            D_00433330.unk0 = 0xF;
            D_00433330.unk40 = 0;
            func_0038EAA0(3);
        } else {
            D_00433330.unk0 = 0xE;
            D_00433330.unk3C = n;
            D_00433330.unk40 = n;
        }
        D_00433330.unk4C = D_00433330.unk50 = func_00394638();
        break;
    case 12:
    case 13:
    case 15:
    case 16:
    case 17:
        D_00433330.unk38 = func_003953F0() != 0 ? func_003953F0() : -1;
        if (n == 1 && D_00433330.unk38 == -1) {
            D_00433330.unk3C = 0;
            D_00433330.unk0 = 8;
            D_00433330.unk40 = 0;
        } else {
            D_00433330.unk0 = 0xC;
            D_00433330.unk3C = n;
            D_00433330.unk40 = n;
        }
        D_00433330.unk4C = D_00433330.unk50 = func_00394638();
        D_00433330.unk74 = (D_00433330.unk30 != 0 ? b->unk3C : a->unk3C) / 16;
        func_0038EAA0(3);
        break;
    case 4:
    case 5:
    case 7:
    case 8:
    case 9:
        D_00433330.unk38 = func_003953F0() != 0 ? func_003953F0() : -1;
        if (n == 1 && D_00433330.unk38 == -1) {
            D_00433330.unk3C = 0;
            D_00433330.unk0 = 8;
            D_00433330.unk40 = 0;
        } else {
            D_00433330.unk0 = 7;
            D_00433330.unk3C = n;
            D_00433330.unk40 = n;
        }
        D_00433330.unk4C = D_00433330.unk50 = func_00394638();
        D_00433330.unk74 = (D_00433330.unk30 != 0 ? b->unk3C : a->unk3C) / 16;
        func_0038EAA0(3);
        break;
    default:
        D_00433330.unk38 = -1;
        if (n == 1) {
            D_00433330.unk3C = 0;
            D_00433330.unk0 = 8;
            D_00433330.unk40 = 0;
        } else {
            D_00433330.unk3C = n;
            D_00433330.unk0 = 7;
            D_00433330.unk40 = n;
        }
        D_00433330.unk4C = D_00433330.unk50 = func_00394638();
        D_00433330.unk74 = (D_00433330.unk30 != 0 ? b->unk3C : a->unk3C) / 16;
        func_0038EAA0(3);
        break;
    }
end:
    D_00433330.unk3C = n;
    D_00433330.unk40 = n;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038DEA0);
#endif

s32 func_0038E2D0(Fighter *a, Fighter *b) {
    s32 r = 0;

    if (D_00433330.unkC4 == 0 && a->unkC0->unk2C == 0) {
        r = b->unkC0->unk2C == 0;
    }
    return r;
}

/* NON_MATCHING: 16 lines: the two positions are copied whole, then read back and subtracted in another order (one instruction short) */
#if 0
s32 func_0038E308(Unk8B90B8 *out, Vec4i *from, Vec4i *to, s32 roll, s32 dist) {
    Vec4i a = *from;
    Vec4i b = *to;
    s32 d[3];

    out->unk0[1] = a.unk0[1];
    out->unk0[0] = a.unk0[0];
    out->unk0[2] = a.unk0[2];
    d[1] = b.unk0[1] - a.unk0[1];
    d[2] = b.unk0[2] - a.unk0[2];
    d[0] = b.unk0[0] - a.unk0[0];
    out->unk0[3] = func_00366108(Math_Isqrt(d[0] * d[0] + d[2] * d[2]), d[1]) << 6;
    out->unk0[4] = (func_00366108(d[0], d[2]) - 0x400) << 6;
    out->unk0[5] = roll;
    out->unk0[6] = dist;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038E308);
#endif

s32 func_0038E408(Unk8B90B8 *dst, s32 i) {
    *dst = D_008B90B8[i];
    return 0;
}

s32 func_0038E460(Unk8B90B8 *src, s32 i) {
    Unk8B90B8 t = *src;

    D_008B90B8[i] = t;
    return 0;
}

/* NON_MATCHING: one instruction long, frame 0x80 for 0x90 (57 lines): the original copies the key through a larger stack block and other saved registers */
#if 0
/* Makes camera key i (D_008B90B8[i]) current: copies it to key 3 and writes the camera angles D_00415808 (pitch and yaw
 * in 1/4096 turns, roll 0, position) that the simulation reads (Battle_UpdateFacing uses D_00415808[1]) [V code]. */
s32 Cam_SetKey(s32 i) {
    s32 s[8];

    D_00415808[2] = 0;
    s[0] = D_008B90B8[i].unk0[3];
    s[1] = D_008B90B8[i].unk0[4];
    s[2] = D_008B90B8[i].unk0[5];
    s[5] = D_008B90B8[i].unk0[0];
    s[6] = D_008B90B8[i].unk0[1];
    s[7] = D_008B90B8[i].unk0[2];
    D_008B90B8[3] = D_008B90B8[i];
    D_00415808[0] = s[0] / 64;
    D_00415808[1] = s[1] / 64;
    D_00415808[5] = s[5];
    D_00415808[6] = s[6];
    D_00415808[7] = s[7];
    Cam_SetDistance(D_008B90B8[i].unk0[6]);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", Cam_SetKey);
#endif

s32 func_0038E610(s32 *out, s32 i) {
    s32 v[3];

    if (i >= 0 && (i < 3 || i == 4)) {
        func_0038DC68(&D_008B90B8[i], v);
        out[0] = D_008B90B8[i].unk0[0];
        out[3] = v[0];
        out[1] = D_008B90B8[i].unk0[1];
        out[4] = v[1];
        out[2] = D_008B90B8[i].unk0[2];
        out[5] = v[2];
        out[6] = D_008B90B8[i].unk0[6];
        out[7] = 0;
    }
    return 0;
}

/* NON_MATCHING: one instruction short; the roll blend is scheduled after the eye computation (the original finishes it first and stores it to the stack), and the bnezl delay slots differ (158 lines) */
#if 0
/* Blends camera keys a and b (t = a's weight, 0x1000 = all a): the looked-at points (func_0038DC68) and the look
 * distances are mixed linearly, the angles along the shorter way (18-bit differences sign-extended); the eye is put
 * back at the mixed distance from the mixed point, and the result becomes key 3 and the current camera
 * [V code; I meaning]. */
s32 func_0038E6D0(Unk8B90B8 *a, Unk8B90B8 *b, s32 t) {
    Unk8B90B8 ka = *a;
    Unk8B90B8 kb = *b;
    Unk8B90B8 *p = &ka;
    Unk8B90B8 *q = &kb;
    s32 rot[4];
    Vec4i eye;
    s32 pa[4];
    s32 pb[4];
    Vec4i tgt;
    Unk8B90B8 out;
    s32 u = 0x1000 - t;
    s32 ra;
    s32 rb;
    s32 r;
    s32 d;
    s32 py;
    s32 pp;
    s32 dist;
    s32 roll;

    ra = func_0038DC68(p, pa);
    rb = func_0038DC68(q, pb);
    r = (rb * u + ra * t) / 4096;
    tgt.unk0[0] = (pa[0] * t + pb[0] * u) / 4096;
    tgt.unk0[1] = (pa[1] * t + pb[1] * u) / 4096;
    tgt.unk0[2] = (pa[2] * t + pb[2] * u) / 4096;
    d = q->unk0[3] - p->unk0[3];
    if (!(d & 0x20000)) {
        d &= 0x3FFFF;
    } else {
        d |= 0xFFFC0000;
    }
    rot[0] = p->unk0[3] + d * u / 4096;
    d = q->unk0[4] - p->unk0[4];
    if (!(d & 0x20000)) {
        d &= 0x3FFFF;
    } else {
        d |= 0xFFFC0000;
    }
    rot[1] = p->unk0[4] + d * u / 4096;
    d = q->unk0[5] - p->unk0[5];
    if (!(d & 0x20000)) {
        d &= 0x3FFFF;
    } else {
        d |= 0xFFFC0000;
    }
    roll = p->unk0[5] + d * u / 4096;
    rot[2] = roll;
    py = (rot[1] / 64 + 0x400) & 0xFFF;
    pp = (rot[0] / 64) & 0xFFF;
    eye.unk0[0] = tgt.unk0[0] - (r * gSinTable[py + 0x400] / 4096) * gSinTable[pp + 0x400] / 4096;
    eye.unk0[1] = tgt.unk0[1] - r * gSinTable[pp] / 4096;
    eye.unk0[2] = tgt.unk0[2] - (r * gSinTable[py] / 4096) * gSinTable[pp + 0x400] / 4096;
    dist = (p->unk0[6] * t + q->unk0[6] * u) / 4096;
    func_0038E308(&out, &eye, &tgt, roll, dist);
    func_0038E460(&out, 3);
    Cam_SetKey(3);
    Cam_SetDistance(dist);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038E6D0);
#endif

void func_0038EAA0(s32 i) {
    D_008B90B8[4] = D_008B90B8[i];
}

s32 func_0038EAF0(Fighter *a, Fighter *b) {
    s32 r = 0;

    if (a->unk176 == 0x1589 || b->unk176 == 0x1589 ||
        a->unk176 == 0x158A || b->unk176 == 0x158A ||
        a->unk176 == 0x158B || b->unk176 == 0x158B ||
        a->unk176 == 0x158C || b->unk176 == 0x158C ||
        a->unk176 == 0x158D || b->unk176 == 0x158D ||
        a->unk176 == 0x158E || b->unk176 == 0x158E ||
        a->unk176 == 0x158F || b->unk176 == 0x158F ||
        a->unk176 == 0x1590 || b->unk176 == 0x1590 ||
        a->unk176 == 0x1591 || b->unk176 == 0x1591 ||
        a->unk176 == 0x1592 || b->unk176 == 0x1592 ||
        a->unk176 == 0x1593 || b->unk176 == 0x1593 ||
        a->unk176 == 0x1594 || b->unk176 == 0x1594 ||
        a->unk176 == 0x1595 || b->unk176 == 0x1595 ||
        a->unk176 == 0x1596 || b->unk176 == 0x1596 ||
        a->unk176 == 0x1597 || b->unk176 == 0x1597 ||
        a->unk176 == 0x1598 || b->unk176 == 0x1598) {
        r = 1;
    }
    return r;
}

/* NON_MATCHING: same size (53 lines): the loop-3 base pointers take other registers (D_00433540 in t8 for s3) and the 0x18 / 0x1A throw pull loads its operands in another order */
#if 0
extern s32 D_00433540[2][4];
extern s32 D_004333F0[]; /* D_00433330.unkC0 */
extern s32 D_005F6150[2];
extern s32 D_005F6158[2];
extern s32 D_005F6160[2];
extern s32 D_005F6168[2];
extern s32 D_005F6174;
extern s32 D_005F6178[2];
extern s32 D_005F6180;
s32 func_00398548(s32 x);

/* Updates the two fighters' camera tracking points D_00433520[i] (x, y, z) from this frame's moves [V code; I roles]:
 * per fighter the move kind (FighterMove.unk24 >> 23; 0x17-0x1A are throw kinds), the tracking mode ((unk24 >> 20) & 3:
 * 1 or 2 follow the root Fighter.unk51C / unk524, else the body unk4 / unkC) and whether it is visible on its own.
 * A fighter that is not tracked on its own is moved with the other one (its point and D_00433540 follow the other's
 * motion); D_005F6170 / D_005F6174 ease a point in over a few frames (a quarter of the gap per frame); throws pull the
 * thrown fighter's point toward the thrower (kind 0x17 by 6/128, 0x18 / 0x1A by a factor from their distance). The y
 * is the head height unk70C - 0x122. D_00433540 keeps the result as the previous frame's points. Fighter.unkE0 is set
 * for the call (both fighters held) and restored at the end. */
void func_0038EC08(Fighter *a, Fighter *b, s32 a2) {
    s32 side;
    s32 i;
    s32 t;
    s32 dx;
    s32 dz;
    s32 x;
    s32 z;
    FighterPtrs2 f = { a, b };
    S32x2 kind = { a->unkC0->unk24 >> 23, b->unkC0->unk24 >> 23 };
    S32x2 thr = { kind[0] >= 0x17 && kind[0] <= 0x1A, kind[1] >= 0x17 && kind[1] <= 0x1A };
    S32x2 mode = { (a->unkC0->unk24 >> 20) & 3, (b->unkC0->unk24 >> 20) & 3 };
    S32x2 vis = { mode[0] != 2 && kind[0] != 0x19, mode[1] != 2 && kind[1] != 0x19 };
    S32x2 e0 = { a->unkE0, b->unkE0 };

    a->unkE0 = (a->unkD4 == 8 || a->unkE0 != 0) && thr[0] == 0 && thr[1] == 0;
    b->unkE0 = (b->unkD4 == 8 || b->unkE0 != 0) && thr[0] == 0 && thr[1] == 0;
    if (func_0038EAF0(a, b) != 0) {
        mode[1] = 1;
        mode[0] = 1;
    }

    for (i = 0; i < 2; i++) {
        if (kind[1 - i] != 0x19) {
            if (vis[i] == 0 && kind[i] == 0x19) {
                if (f[i]->unk628 > 0x100) {
                    if (f[i]->unk628 < 0x1EC && f[i]->unk62A < 0x1EC && f[i]->unk62C < 0x1EC) {
                        vis[i] = 1;
                    }
                } else if (f[i]->unk628 >= 0x15 && f[i]->unk62A >= 0x15 && f[i]->unk62C >= 0x15) {
                    vis[i] = 1;
                }
            }
            if (D_005F6168[i] == 0x19 && D_005F6160[i] == 1 && kind[i] == D_005F6168[i]) {
                vis[i] = D_005F6160[i];
            }
        }
    }
    if (a->unkE0 != 0 || b->unkE0 != 0) {
        D_005F6174 = 0;
        a2 = 1;
        D_005F6170[0] = 0;
    }

    for (i = 0; i < 2; i++) {
        if (D_005F6160[i] == 0 && vis[i] == 1) {
            if (f[i]->unk628 > 0x100) {
                if (f[i]->unk628 > 0x200 || f[i]->unk62A > 0x200 || f[i]->unk62C > 0x200) {
                    D_005F6170[i] = 2;
                } else {
                    D_005F6170[i] = 4;
                }
            } else if (f[i]->unk628 < 0 || f[i]->unk62A < 0 || f[i]->unk62C < 0) {
                D_005F6170[i] = 2;
            } else {
                D_005F6170[i] = 4;
            }
            D_005F6178[i] = 0;
        }
        D_005F6160[i] = vis[i];
        D_005F6168[i] = kind[i];
    }

    side = D_004333F0[0];
    for (i = 0; i < 2; i++) {
        D_005F6158[i] = D_005F6150[i];
        if (side == i && !(f[i]->unkC4 & 1)) {
            continue;
        }
        if (mode[i] == 1 || f[i]->unkE0 != 0 || (mode[i] == 0 && thr[i] == 0) || vis[i] != 0) {
            if (mode[i] == 1 || f[i]->unkE0 != 0 || mode[i] == 2) {
                D_00433520[i][0] = f[i]->unk51C;
                D_00433520[i][2] = f[i]->unk524;
            } else {
                D_00433520[i][0] = f[i]->unk4;
                D_00433520[i][2] = f[i]->unkC;
            }
            if (D_005F6170[i] != 0 && thr[1 - i] == 0) {
                D_005F6170[i]--;
                D_00433520[i][0] = D_00433540[i][0] + (D_00433520[i][0] - D_00433540[i][0]) / 4;
                D_00433520[i][2] = D_00433540[i][2] + (D_00433520[i][2] - D_00433540[i][2]) / 4;
            }
        } else {
            if (mode[1 - i] != 2 || (thr[1 - i] != 0 && D_005F6180 == 0)) {
                if (i == 0 && (mode[1] == 1 || f[0]->unkE0 != 0 || (mode[1] == 0 && thr[1] == 0))) {
                    if (mode[1] == 1 || a2 != 0) {
                        D_00433520[1][0] = f[1]->unk51C;
                        D_00433520[1][2] = f[1]->unk524;
                    } else {
                        D_00433520[1][0] = f[1]->unk4;
                        D_00433520[1][2] = f[1]->unkC;
                    }
                    if (D_005F6174 != 0 && thr[0] == 0) {
                        D_00433520[1][0] = D_00433540[1][0] + (D_00433520[1][0] - D_00433540[1][0]) / 4;
                        D_00433520[1][2] = D_00433540[1][2] + (D_00433520[1][2] - D_00433540[1][2]) / 4;
                        D_005F6174--;
                    }
                }
                D_00433520[i][0] += D_00433520[1 - i][0] - D_00433540[1 - i][0];
                D_00433520[i][2] += D_00433520[1 - i][2] - D_00433540[1 - i][2];
                D_00433540[i][0] += D_00433520[1 - i][0] - D_00433540[1 - i][0];
                D_00433540[i][2] += D_00433520[1 - i][2] - D_00433540[1 - i][2];
            } else {
                D_00433520[i][0] = D_00433540[i][0];
                D_00433520[i][2] = D_00433540[i][2];
            }
        }
        D_005F6150[i] = f[i]->unk624;
        if (mode[i] != 2) {
            D_00433520[i][1] = f[i]->unk70C - 0x122;
        } else {
            D_00433520[i][1] = D_00433540[i][1];
        }
    }

    for (i = 0; i < 2; i++) {
        if (thr[i] != 0 && thr[1 - i] == 0 && a2 == 0) {
            switch (kind[i]) {
            case 0x17:
                D_00433520[i][0] = D_00433540[i][0] + (((D_00433520[1 - i][0] - D_00433540[i][0]) * 6) >> 7);
                D_00433520[i][2] = D_00433540[i][2] + (((D_00433520[1 - i][2] - D_00433540[i][2]) * 6) >> 7);
                break;
            case 0x18:
            case 0x1A:
                dx = D_00433520[0][0] - D_00433520[1][0];
                dz = D_00433520[0][2] - D_00433520[1][2];
                t = (0x2198 - func_00398548(dx * dx + dz * dz) / 64) / 1250;
                D_00433520[i][0] = D_00433540[i][0] + (((D_00433520[1 - i][0] - D_00433540[i][0]) * t) >> 7);
                D_00433520[i][2] = D_00433540[i][2] + (((D_00433520[1 - i][2] - D_00433540[i][2]) * t) >> 7);
                break;
            case 0x19:
                break;
            }
        }
    }

    if (D_00433520[0][0] == D_00433520[1][0] && D_00433520[0][2] == D_00433520[1][2]) {
        x = D_00433520[0][0];
        z = D_00433520[0][2];
        D_00433520[0][0] = x - 1;
        D_00433520[0][2] = z - 1;
        D_00433520[1][0] = x + 1;
        D_00433520[1][2] = z + 1;
    }
    for (i = 0; i < 2; i++) {
        *(Vec4i *)D_00433540[i] = *(Vec4i *)D_00433520[i];
    }
    a->unkE0 = e0[0];
    b->unkE0 = e0[1];
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038EC08);
#endif

s32 func_0038F6B0(void) {
    U32Pair k = { gFighters[0].unkC0->unk24 >> 23, gFighters[1].unkC0->unk24 >> 23 };
    s32 r = -1;

    if (k.unk0 == 0x13 || k.unk4 == 0x13) {
        r = 0x200;
    } else if (k.unk0 == 0x14 || k.unk4 == 0x14) {
        r = 0x180;
    } else if (k.unk0 == 0x15 || k.unk4 == 0x15) {
        r = 0x80;
    } else if (k.unk0 == 0x16 || k.unk4 == 0x16) {
        r = 0x40;
    }
    return r;
}

void func_0038F768(void) {
    s32 i;

    for (i = 0; i < 5; i++) {
        D_008B9148[i].unk0 = D_008B9148[i].unk4 = D_008B9148[i].unk8 = 0;
        D_008B9148[i].unk18 = 0;
        D_008B9148[i].unkC = D_008B9148[i].unk10 = 0;
        D_008B9148[i].unk14 = 0;
        D_008B9148[i].unk1C = 0;
        D_008B9148[i].unk20 = 0;
    }
}

/* NON_MATCHING: same size (63 lines): a and b take each other's saved registers (s0 / s1), the pair loads before func_00366108 and the registers of the swing clamp and sine step differ */
#if 0
extern u32 D_005F5C28;
s32 func_0037D898(void);
void func_0037D890(void);
s32 func_0038F6B0(void);

typedef struct {
    /* 0x0 */ s32 *unk0;
    /* 0x4 */ s32 *unk4;
} Cam7F8Pair;

/* The yaw of the fight camera for key `slot` [V code; I roles]: the yaw from the fighter behind (unk624) to the one in
 * front (tracking points Fighter.unk51C while their move kind is 2, else D_00433520), turned into a 16.6 angle, and its
 * difference d to the current key's yaw wrapped to 18 bits. With `reset` the path record D_008B9148[slot] is cleared and
 * the fighters' yaw is returned in *out. Otherwise, for slot 0 within 0x1AAAA of the camera and at a distance
 * D_005F5C28 of 0xC8 or more, the record swings the camera (state 0 counts frames off-axis, state 1 adds a swing
 * velocity unk8 to unk4, clamped by func_0038F6B0's limit) and *out = key yaw + unk4 >> 6; else *out = key yaw. */
s32 func_0038F7F8(s32 a0, s32 a1, s32 *out, s32 reset, s32 slot) {
    Unk8B90B8 key;
    Cam7F8Pair k;
    Cam7F8Pair t;
    Fighter *a = (Fighter *)a0;
    Fighter *b = (Fighter *)a1;
    s32 *p;
    s32 *q;
    s32 yaw;
    s32 d;
    s32 v;
    s32 lim;
    s32 sign;
    s32 swing;
    s32 *p4;
    s32 *p8;
    s32 lim2;

    swing = 0;
    sign = 0;
    t.unk4 = (((b->unkC0->unk24 >> 20) & 3) == 2) ? &b->unk51C : D_00433520[1];
    t.unk0 = (((a->unkC0->unk24 >> 20) & 3) == 2) ? &a->unk51C : D_00433520[0];
    k = t;
    func_0038E408(&key, slot);
    func_0038E408(&key, 3);
    if (a->unk624 < b->unk624) {
        p = k.unk4;
        q = k.unk0;
    } else {
        p = k.unk0;
        q = k.unk4;
    }
    lim = p[0];
    lim2 = p[2];
    yaw = func_00366108(lim - q[0], lim2 - q[2]);
    if (func_0037D898() != 0) {
        yaw = 0;
    }
    func_0037D890();
    yaw <<= 6;
    v = yaw - key.unk0[4];
    if (!(v & 0x20000)) {
        d = v & 0x3FFFF;
    } else {
        d = v | 0xFFFC0000;
    }
    if (d > 0) {
        sign = 1;
    } else if (d < 0) {
        d = -d;
        sign = -1;
    }
    if (reset != 0) {
        D_008B9148[slot].unk4 = 0;
        D_008B9148[slot].unk8 = 0;
        D_008B9148[slot].unk34 = 0;
        D_008B9148[slot].unk38 = 0;
        D_008B9148[slot].unk10 = 0;
        D_008B9148[slot].unkC = 0;
        D_008B9148[slot].unk14 = 0;
        *out = yaw;
        return reset;
    }
    if (slot != 0 || d > 0x1AAAA || D_005F5C28 < 0xC8) {
        D_008B9148[slot].unk34 = 0;
        D_008B9148[slot].unk38 = 0;
        *out = key.unk0[4];
        return 0;
    }
    switch (D_008B9148[0].unk0) {
    case 0:
        if (d >= 0xE38) {
            D_008B9148[0].unk20++;
        } else {
            D_008B9148[0].unk20 = 0;
        }
        if (D_008B9148[slot].unk20 > 0) {
            D_008B9148[slot].unk0 = 1;
            D_008B9148[slot].unk8 += sign * 0x600;
            D_008B9148[slot].unk20 = 0;
        }
        break;
    case 1:
        if (d < 0x38E4) {
            D_008B9148[0].unk4 -= (D_008B9148[0].unk4 * 0xE) >> 7;
            D_008B9148[0].unk8 = (D_008B9148[0].unk8 - (D_008B9148[0].unk8 >> 1)) + ((sign * 0x600) >> 1);
            if (d < 0xE39) {
                D_008B9148[0].unk8 -= D_008B9148[0].unk8 / 2;
                lim = D_008B9148[0].unk20 + 1;
                D_008B9148[0].unk20 = lim;
                if (lim >= 0x20) {
                    D_008B9148[0].unk4 = 0;
                    D_008B9148[0].unk20 = 0;
                    D_008B9148[0].unk0 = 0;
                }
            } else {
                D_008B9148[0].unk20 = 0;
            }
        } else if (sign > 0) {
            D_008B9148[0].unk8 += 0x600;
        } else if (sign < 0) {
            D_008B9148[0].unk8 -= 0x600;
        }
        p4 = &D_008B9148[slot].unk4;
        p8 = &D_008B9148[slot].unk8;
        *p4 += *p8;
        v = func_0038F6B0();
        if (v > 0) {
            lim = (v * 0xDDD0) >> 8;
            if (lim < *p4) {
                *p8 -= 0x600;
                *p4 = lim;
            } else {
                lim2 = (v * -0xDDD0) >> 8;
                if (*p4 < lim2) {
                    *p8 += 0x600;
                    *p4 = lim2;
                }
            }
        } else if (*p4 > 0xDDD0) {
            *p8 -= 0x600;
            *p4 = 0xDDD0;
        } else if (*p4 < -0xDDD0) {
            *p8 += 0x600;
            *p4 = -0xDDD0;
        }
        swing = D_008B9148[slot].unk4 >> 6;
        break;
    }
    D_008B9148[slot].unk18 = d;
    if (d > 0xE38D) {
        D_008B9148[slot].unk1C++;
    } else {
        D_008B9148[slot].unk1C = 0;
    }
    if (slot == 0) {
        if (d >= 0x38E3) {
            v = ((d - 0x38E3) << 12) / 0xC71D;
            D_008B9148[0].unk10 = (gSinTable[(v / 256) & 0xFFF] * 0x44440) >> 12;
        } else {
            D_008B9148[0].unk10 = 0;
        }
        if (D_008B9148[0].unk24 != 0) {
            lim = D_008B9148[0].unk10 - D_008B9148[0].unkC;
            if (lim < 0) {
                v = lim >> 4;
            } else if (lim > 0) {
                v = lim >> 4;
            } else {
                v = 0;
            }
            if (v > 0x100) {
                v = 0x100;
            } else if (v < -0x100) {
                v = -0x100;
            }
            D_008B9148[0].unk14 = v;
            D_008B9148[0].unkC += v;
        } else {
            D_008B9148[0].unk14 = 0;
        }
    }
    *out = key.unk0[4] + swing;
    return reset;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038F7F8);
#endif

/* NON_MATCHING: typed draft, 32 instructions long (794 lines): same control flow and frame; the original keeps &D_008B9148 in s7 and dist in fp, shares the unk30 easing differently and schedules otherwise */
#if 0
extern s32 D_005F6180;
extern s32 D_005F6148;

/* [V] The orbit key: key idx looks at the fighters' midpoint from (pitch, yaw); the distance is pushed out until both
 * fighters fit on screen (D_005F6180 = they are too far apart), the per-key offsets D_008B9148[idx].unk28-unk30 are
 * eased each frame unless reset, and the result is written with func_0038E460. Returns 0. */
s32 func_0038FD90(s32 a0, s32 a1, s32 pitch, s32 yaw, s32 roll, s32 reset, s32 dist, s32 idx) {
    Vec4i ofs;     /* camera offset from the centre, in the camera frame */
    Vec4i center;  /* the point looked at */
    Vec4i pos[2];  /* the two fighters in the camera frame */
    Unk8B90B8 key;
    Unk8B90B8 old;
    CamPair kind;
    CamPair tmp;
    CamPair flag;
    CamPair dang;  /* pitch and yaw steps */
    Vec4i dpos;    /* position step in [1..3] [I layout] */
    Vec4i unused;  /* never read or written [I] */
    Fighter *fa = (Fighter *)a0;
    Fighter *fb = (Fighter *)a1;
    s32 heavy;
    s32 s;
    s32 i;
    s32 sinP;
    s32 cosP;
    s32 sinY;
    s32 cosY;
    s32 cx;
    s32 cz;
    s32 dx;
    s32 dy;
    s32 dz;
    s32 h;
    s32 y;
    s32 lim;
    s32 depth;
    s32 cy;
    s32 d;
    s32 l;
    s32 r;
    s32 sl;
    s32 sr;
    s32 scale;
    s32 q;
    s32 *p;
    s32 v;
    s32 n;
    s32 ang;
    s32 lift;
    s32 hy;
    s32 ty;
    s32 tz;
    s32 k;
    s32 den;
    s32 pull;
    s32 m;
    s32 f;
    s32 z;
    s32 rz;
    s32 ry;
    s32 nx;
    s32 nz;

    memset(&ofs, 0, sizeof(ofs));
    heavy = 0;
    s = D_00433330.unkA4;
    tmp.v[0] = fa->unkC0->unk24 >> 23;
    tmp.v[1] = fb->unkC0->unk24 >> 23;
    kind = tmp;
    if (kind.v[0] == 0x17 || kind.v[1] == 0x17 || kind.v[0] == 0x18 || kind.v[1] == 0x18 || kind.v[0] == 0x19 ||
        kind.v[1] == 0x19 || kind.v[0] == 0x1A || kind.v[1] == 0x1A) {
        heavy = 1;
    }
    if (dist <= 0x320) {
        dist = 0x320;
    }
    func_0038E408(&key, idx);
    sinP = CAM_SIN(-pitch);
    cosP = CAM_COS(-pitch);
    sinY = CAM_SIN(-yaw);
    old = key;
    cz = (D_00433520[0][2] + D_00433520[1][2]) >> 1;
    cx = (D_00433520[0][0] + D_00433520[1][0]) >> 1;
    cosY = CAM_COS(-yaw);
    if (reset != 0) {
        center.unk0[1] = key.unk0[1];
        center.unk0[2] = cz + ((dist * CAM_SIN(yaw + 0x30000)) >> 12);
        center.unk0[0] = cx + ((dist * CAM_COS(yaw + 0x30000)) >> 12);
    } else {
        d = yaw - key.unk0[4];
        dx = key.unk0[0] - cx;
        dz = key.unk0[2] - cz;
        v = d & 0x3FFFF;
        if (d & 0x20000) {
            v |= 0xFFFC0000;
        }
        center.unk0[1] = key.unk0[1];
        center.unk0[2] = cz + ((CAM_SIN(v) * dx + CAM_COS(v) * dz) >> 12);
        center.unk0[0] = cx + ((CAM_COS(v) * dx - CAM_SIN(v) * dz) >> 12);
    }
    for (i = 0; i < 2; i++) {
        dx = D_00433520[i][0] - center.unk0[0];
        dz = D_00433520[i][2] - center.unk0[2];
        dy = D_00433520[i][1] - center.unk0[1];
        h = (sinY * dx + cosY * dz) >> 12;
        pos[i].unk0[0] = (cosY * dx - sinY * dz) >> 12;
        pos[i].unk0[2] = (cosP * h - sinP * dy) >> 12;
        pos[i].unk0[1] = (sinP * h + cosP * dy) >> 12;
        tmp.v[i] = h;
    }
    h = (tmp.v[0] + tmp.v[1]) >> 1;
    y = -0x1C2 - center.unk0[1];
    lim = D_00433330.unkA4 * -0xCE4 / 256;
    depth = (cosP * h - sinP * y) >> 12;
    cy = (sinP * h + cosP * y) >> 12;
    d = dist - depth;
    if (d < lim) {
        d = lim;
    }
    ofs.unk0[2] -= d;
    pos[0].unk0[2] += d;
    pos[1].unk0[2] += d;
    depth += d;
    if (pos[0].unk0[2] == 0) {
        pos[0].unk0[2] = 1;
    }
    if (pos[1].unk0[2] == 0) {
        pos[1].unk0[2] = 1;
    }
    if (pos[0].unk0[0] < pos[1].unk0[0]) {
        pos[0].unk0[0] -= 0x96;
        pos[1].unk0[0] += 0x96;
        l = 0;
        r = 1;
        sl = pos[0].unk0[0] * s / pos[0].unk0[2];
        sr = pos[1].unk0[0] * s / pos[1].unk0[2];
    } else {
        l = 1;
        pos[1].unk0[0] -= 0x96;
        pos[0].unk0[0] += 0x96;
        r = 0;
        sl = pos[1].unk0[0] * s / pos[1].unk0[2];
        sr = pos[0].unk0[0] * s / pos[0].unk0[2];
    }
    if (sr - sl >= 0x189) {
        D_005F6180 = 1;
    } else {
        D_005F6180 = 0;
    }
    scale = heavy ? 8 : 0x10;
    if (sr - sl >= 0x189) {
        q = (s * (pos[r].unk0[0] - pos[l].unk0[0]) - (pos[r].unk0[2] - pos[l].unk0[2]) * 0xC4) / 0x188;
        v = pos[l].unk0[2] - q;
        if (reset != 0) {
            ofs.unk0[2] += v;
            D_008B9148[idx].unk30 = v;
        } else {
            p = &D_008B9148[idx].unk30;
            n = *p;
            if (n < v) {
                v = n + (((v - n) * 4) >> 7);
            } else {
                v = n - (((n - v) * 16) >> 7);
            }
            ofs.unk0[2] += v;
            *p = v;
        }
        pos[0].unk0[2] -= v;
        pos[1].unk0[2] -= v;
        depth -= v;
        ofs.unk0[0] = ((pos[l].unk0[0] - q * -0xC4 / s) * scale) >> 7;
    } else if (reset != 0 ||
               pos[r].unk0[2] * 0x44 / s - (pos[r].unk0[0] - pos[l].unk0[0]) < pos[l].unk0[2] * -0x44 / s) {
        ofs.unk0[0] = ((pos[l].unk0[0] + (pos[r].unk0[0] - pos[l].unk0[0]) * pos[l].unk0[2] /
                                             (pos[l].unk0[2] + pos[r].unk0[2])) * scale) >> 7;
        if (reset == 0) {
            v = D_008B9148[idx].unk30;
            n = 0;
            if (v < 0) {
                n = v + -v / 32;
            } else if (v > reset) {
                n = v - v / 8;
            }
            ofs.unk0[2] += n;
            pos[0].unk0[2] -= n;
            pos[1].unk0[2] -= n;
            depth -= n;
            D_008B9148[idx].unk30 = n;
        } else {
            D_008B9148[idx].unk30 = 0;
        }
    } else if (sl < -0x44) {
        ofs.unk0[0] += ((pos[l].unk0[0] - pos[l].unk0[2] * -0x44 / s) * scale) >> 7;
        v = D_008B9148[idx].unk30;
        n = 0;
        if (v < 0) {
            n = v + -v / 32;
        } else if (v > reset) {
            n = v - v / 8;
        }
        ofs.unk0[2] += n;
        pos[0].unk0[2] -= n;
        pos[1].unk0[2] -= n;
        depth -= n;
        D_008B9148[idx].unk30 = n;
    } else if (sr > 0x44) {
        ofs.unk0[0] += ((pos[r].unk0[0] - pos[r].unk0[2] * 0x44 / s) * scale) >> 7;
        v = D_008B9148[idx].unk30;
        n = 0;
        if (v < 0) {
            n = v + -v / 32;
        } else if (v > reset) {
            n = v - v / 8;
        }
        ofs.unk0[2] += n;
        pos[0].unk0[2] -= n;
        pos[1].unk0[2] -= n;
        depth -= n;
        D_008B9148[idx].unk30 = n;
    } else {
        v = D_008B9148[idx].unk30;
        n = 0;
        if (v < 0) {
            n = v + -v / 32;
        } else if (v > reset) {
            n = v - v / 8;
        }
        ofs.unk0[2] += n;
        pos[0].unk0[2] -= n;
        pos[1].unk0[2] -= n;
        depth -= n;
        D_008B9148[idx].unk30 = n;
    }
    ang = 0x8C;
    if (pos[0].unk0[2] == 0) {
        pos[0].unk0[2] = 1;
    }
    if (pos[1].unk0[2] == 0) {
        pos[1].unk0[2] = 1;
    }
    if (pos[0].unk0[1] * s / pos[0].unk0[2] < pos[1].unk0[1] * s / pos[1].unk0[2]) {
        ty = pos[0].unk0[1];
        tz = pos[0].unk0[2];
        hy = D_00433520[0][1];
    } else {
        ty = pos[1].unk0[1];
        tz = pos[1].unk0[2];
        hy = D_00433520[1][1];
    }
    hy = (hy * cosP) >> 12;
    if (idx == 1) {
        ang = (pitch < 0) ? 0xB4 : 0x8C;
    }
    lift = 0;
    if (idx == 1 && pitch > 0) {
        if (hy >= -0x3E8) {
            lift = ((hy + 0x3E8) << 9) / 640;
        }
        if (reset == 0) {
            if (D_005F6148 < lift) {
                if (lift - D_005F6148 > 0x1C) {
                    lift = D_005F6148 + 0x1C;
                }
            } else if (lift < D_005F6148) {
                if (D_005F6148 - lift > 0x1C) {
                    lift = D_005F6148 - 0x1C;
                }
            }
        }
        D_005F6148 = lift;
    }
    k = (cosP * (ang - s * 0x1C2 / dist)) >> 12;
    den = k + 0x9C;
    pull = ((tz - depth) * -0x9C - s * (ty - cy)) / (den == 0 ? 1 : den) - depth;
    if (pull < 0) {
        pull = 0;
    }
    if (reset != 0) {
        pull = 0;
        D_008B9148[idx].unk28 = 0;
    } else if (pull < D_008B9148[idx].unk2C) {
        d = D_008B9148[idx].unk2C - pull;
        if (d > 0x4B0) {
            d = 0x4B0;
        }
        d = ((d * 0x78) >> 7) - d;
        D_008B9148[idx].unk28 = d;
        pull = D_008B9148[idx].unk2C + d;
    } else if (D_008B9148[idx].unk2C < pull) {
        d = pull - D_008B9148[idx].unk2C;
        if (d > 0x320) {
            d = 0x320;
        }
        if (kind.v[0] == 0x19 || kind.v[1] == 0x19) {
            m = 4;
        } else {
            m = heavy ? 0xA : 0x10;
        }
        d = (d * m) >> 7;
        D_008B9148[idx].unk28 = d;
        pull = D_008B9148[idx].unk2C + d;
    }
    D_008B9148[idx].unk2C = pull;
    if (pull > 0) {
        pos[0].unk0[2] += pull;
        pos[1].unk0[2] += pull;
        ofs.unk0[2] -= pull;
        depth += pull;
    }
    f = 0;
    if (kind.v[0] == 0x17 || kind.v[0] == 0x18 || kind.v[0] == 0x1A) {
        f = 1;
    }
    flag.v[0] = f;
    f = 0;
    if (kind.v[1] == 0x17 || kind.v[1] == 0x18 || kind.v[1] == 0x1A) {
        f = 1;
    }
    flag.v[1] = f;
    tmp = flag;
    z = ofs.unk0[2];
    if (z < 0 && (tmp.v[0] != 0 || tmp.v[1] != 0) &&
        ((fa->unkF3 != 0 && ((fa->unkC0->unk4 & 0x400) || fa->unk135 != 0)) ||
         (fb->unkF3 != 0 && ((fb->unkC0->unk4 & 0x400) || fb->unk135 != 0)))) {
        if (r != 0) {
            if (tmp.v[0] == 0) {
                ofs.unk0[0] = z * 0x17FA00 / D_005F5CD0 / 4096;
            } else {
                ofs.unk0[0] = -z * 0x17FA00 / D_005F5CD0 / 4096;
            }
        } else {
            if (tmp.v[0] != 0) {
                ofs.unk0[0] = z * 0x17FA00 / D_005F5CD0 / 4096;
            } else {
                ofs.unk0[0] = -z * 0x17FA00 / D_005F5CD0 / 4096;
            }
        }
    }
    ofs.unk0[1] = cy - k * depth / s + lift;
    sinP = CAM_SIN(pitch);
    cosP = CAM_COS(pitch);
    cosY = CAM_COS(yaw);
    sinY = CAM_SIN(yaw);
    rz = (ofs.unk0[2] * cosP - ofs.unk0[1] * sinP) >> 12;
    ry = (ofs.unk0[2] * sinP + ofs.unk0[1] * cosP) >> 12;
    nx = (ofs.unk0[0] * cosY + rz * -sinY) >> 12;
    nz = (ofs.unk0[0] * sinY + rz * cosY) >> 12;
    if (reset != 0) {
        key.unk0[1] = center.unk0[1] + ry;
        key.unk0[2] = center.unk0[2] + nz;
        key.unk0[0] = center.unk0[0] + nx;
        key.unk0[3] = pitch;
        key.unk0[4] = yaw;
        key.unk0[5] = roll;
    } else {
        dpos.unk0[1] = center.unk0[0] + nx - key.unk0[0];
        dpos.unk0[2] = center.unk0[1] + ry - key.unk0[1];
        dpos.unk0[3] = center.unk0[2] + nz - key.unk0[2];
        d = pitch - key.unk0[3];
        if (!(d & 0x20000)) {
            dang.v[0] = d & 0x3FFFF;
        } else {
            dang.v[0] = d | 0xFFFC0000;
        }
        d = yaw - key.unk0[4];
        if (!(d & 0x20000)) {
            dang.v[1] = d & 0x3FFFF;
        } else {
            dang.v[1] = d | 0xFFFC0000;
        }
        if (idx == 0) {
            if (dpos.unk0[1] * dpos.unk0[1] + dpos.unk0[3] * dpos.unk0[3] > 0x3E8) {
                D_008B9148[0].unk24 = 1;
            } else {
                D_008B9148[0].unk24 = 0;
            }
        }
        key.unk0[1] += dpos.unk0[2];
        key.unk0[0] += dpos.unk0[1];
        key.unk0[2] += dpos.unk0[3];
        key.unk0[3] += dang.v[0];
        key.unk0[4] += dang.v[1];
        key.unk0[5] = roll;
    }
    func_0038E460(&key, idx);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038FD90);
#endif

/* NON_MATCHING: one store short (24 lines): the original frame has 0x30 bytes of other locals below the key c (it clears a word at sp+0x20 between the dist and x stores); the loop's two field addresses are swapped */
#if 0
s32 func_00390E08(Fighter *a, Fighter *b) {
    Unk8B90B8 c;
    s32 i;

    func_0038DBC0();
    func_0038EC08(a, b, 0);
    func_0038EC08(a, b, 0);
    for (i = 0; i < 2; i++) {
        D_005F6170[i] = 0;
    }
    for (i = 1; i < 5; i++) {
        D_008B9148[i].unk34 = 0;
        D_008B9148[i].unk38 = 0;
        D_008B9148[i].unk28 = 0;
        D_008B9148[i].unk2C = 0;
        D_008B9148[i].unk30 = 0;
        D_008B9148[i].unk24 = 0;
    }
    c.unk0[1] = -0x4B0;
    c.unk0[2] = -0x1F40;
    c.unk0[6] = D_00433330.unkA4;
    D_005F6C18 = 0;
    D_005F6C1A = 0;
    c.unk0[0] = 0;
    c.unk0[3] = 0;
    c.unk0[4] = 0;
    c.unk0[5] = 0;
    func_0038E460(&c, 0);
    func_0038E460(&c, 1);
    func_0038F768();
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00390E08);
#endif

s32 func_00390F48(Fighter *f) {
    u32 k = f->unkC0->unk24 >> 23;

    if (k == 0x71 || k == 0xA2 || k == 0xA1 || k == 0xA3) {
        if (D_00433330.unkB4 != 2) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_RODATA("asm/nonmatchings/stage", D_00508120);

/* NON_MATCHING: 20 lines: argument shuffle through t1 and the D_005F6C1A sum's registers (D_008B9154 is read by its own symbol) */
#if 0
extern s32 D_008B9154[]; /* D_008B9154[0] by its own symbol */

s32 func_00390FA8(s32 a0, s32 a1, s32 a2, s32 a3) {
    s32 v;
    s32 r = func_0038F7F8(a0, a1, &v, a2, 0);

    D_005F6C1A += D_00508120[D_005F6C18].unk2;
    func_0038FD90(a0, a1, D_008B9154[0], v, 0, r, a3, 0);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00390FA8);
#endif

void func_00391048(void) {
    D_00433330.unk54 = -1;
}

s32 func_00391058(Fighter *a, Fighter *b) {
    s32 r;

    if (a->unk624 < b->unk624) {
        r = func_00366108(D_00433520[1][0] - D_00433520[0][0], D_00433520[1][2] - D_00433520[0][2]);
    } else {
        r = func_00366108(D_00433520[0][0] - D_00433520[1][0], D_00433520[0][2] - D_00433520[1][2]);
    }
    return r << 6;
}

s32 func_003910D0(Fighter *a, Fighter *b) {
    s32 v[2];
    s32 r;

    func_003800D8(0xE3);
    r = func_00391058(a, b);
    func_00394110(D_00433330.unk58 - 1, &v[0], &v[1]);
    if (D_00433330.unk34 == 0) {
        r += v[1];
    } else {
        r -= v[1];
    }
    D_00433330.unk64 = v[0];
    D_00433330.unk68 = r;
    D_00433330.unk6C = 0;
    return 0;
}

/* NON_MATCHING: 2 instructions short (50 lines): the prologue saves and the argument copies are scheduled differently, the registers follow */
#if 0
s32 func_00391170(Fighter *a, Fighter *b, s32 a2, s32 a3, s32 swap) {
    Fighter *f;
    Fighter *o;

    if (swap == 0) {
        f = a;
        o = b;
    } else {
        f = b;
        o = a;
    }
    if (D_00433330.unk58 < 0) {
        if (a2 != 0) {
            return func_003928B8(-D_00433330.unk58, f, o);
        }
        return func_003928B8(0, f, o);
    }
    if (D_00433330.unk58 > 0) {
        func_0038FD90((s32)a, (s32)b, D_00433330.unk64, D_00433330.unk68, D_00433330.unk6C, a2, a3 * 0x72 / 128, 1);
        if (a2 == 0 && f->unkC0 != D_00433330.unk60 && f->unkE0 > 0 && (f->unkC0->unk24 >> 23) != 0x11) {
            D_00433330.unk60 = f->unkC0;
            switch (func_00390F48(f)) {
            case 0:
                break;
            case 1:
                func_0038D3D0(D_00433330.unk34, f, o);
                break;
            }
        }
    } else {
        Cam_Reset();
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00391170);
#endif

s32 func_003912C8(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6) {
    D_00433330.unk54 = -1;
    func_0038FD90(a0, a1, a4, a5, a6, 1, a3, 0);
    return 0;
}

s32 func_00391310(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6) {
    D_00433330.unk54 = -1;
    func_00391358(a0, a1, a4, a5, a6, 1, a3, 0);
    return 0;
}

/* NON_MATCHING: typed draft, 26 instructions long (866 lines): the twin of func_0038FD90, same differences (register allocation of &D_008B9148 and dist, shared easing code, scheduling) */
#if 0
extern s32 D_005F6180;
extern s32 D_005F614C;

/* [V] Twin of func_0038FD90 (same orbit key): its lift memory is D_005F614C, the lift defaults to -0x12C and is
 * stored for key 1 whatever the pitch, and the wide case re-clamps the depths to 1. Returns 0. */
s32 func_00391358(s32 a0, s32 a1, s32 pitch, s32 yaw, s32 roll, s32 reset, s32 dist, s32 idx) {
    Vec4i ofs;     /* camera offset from the centre, in the camera frame */
    Vec4i center;  /* the point looked at */
    Vec4i pos[2];  /* the two fighters in the camera frame */
    Unk8B90B8 key;
    Unk8B90B8 old;
    CamPair kind;
    CamPair tmp;
    CamPair flag;
    CamPair dang;  /* pitch and yaw steps */
    Vec4i dpos;    /* position step in [1..3] [I layout] */
    Vec4i unused;  /* never read or written [I] */
    Fighter *fa = (Fighter *)a0;
    Fighter *fb = (Fighter *)a1;
    s32 heavy;
    s32 s;
    s32 i;
    s32 sinP;
    s32 cosP;
    s32 sinY;
    s32 cosY;
    s32 cx;
    s32 cz;
    s32 dx;
    s32 dy;
    s32 dz;
    s32 h;
    s32 y;
    s32 lim;
    s32 depth;
    s32 cy;
    s32 d;
    s32 l;
    s32 r;
    s32 sl;
    s32 sr;
    s32 scale;
    s32 q;
    s32 *p;
    s32 v;
    s32 n;
    s32 ang;
    s32 lift;
    s32 hy;
    s32 ty;
    s32 tz;
    s32 k;
    s32 den;
    s32 pull;
    s32 m;
    s32 f;
    s32 z;
    s32 rz;
    s32 ry;
    s32 nx;
    s32 nz;

    memset(&ofs, 0, sizeof(ofs));
    heavy = 0;
    s = D_00433330.unkA4;
    tmp.v[0] = fa->unkC0->unk24 >> 23;
    tmp.v[1] = fb->unkC0->unk24 >> 23;
    kind = tmp;
    if (kind.v[0] == 0x17 || kind.v[1] == 0x17 || kind.v[0] == 0x18 || kind.v[1] == 0x18 || kind.v[0] == 0x19 ||
        kind.v[1] == 0x19 || kind.v[0] == 0x1A || kind.v[1] == 0x1A) {
        heavy = 1;
    }
    if (dist <= 0x320) {
        dist = 0x320;
    }
    func_0038E408(&key, idx);
    sinP = CAM_SIN(-pitch);
    cosP = CAM_COS(-pitch);
    sinY = CAM_SIN(-yaw);
    old = key;
    cz = (D_00433520[0][2] + D_00433520[1][2]) >> 1;
    cx = (D_00433520[0][0] + D_00433520[1][0]) >> 1;
    cosY = CAM_COS(-yaw);
    if (reset != 0) {
        center.unk0[1] = key.unk0[1];
        center.unk0[2] = cz + ((dist * CAM_SIN(yaw + 0x30000)) >> 12);
        center.unk0[0] = cx + ((dist * CAM_COS(yaw + 0x30000)) >> 12);
    } else {
        d = yaw - key.unk0[4];
        dx = key.unk0[0] - cx;
        dz = key.unk0[2] - cz;
        v = d & 0x3FFFF;
        if (d & 0x20000) {
            v |= 0xFFFC0000;
        }
        center.unk0[1] = key.unk0[1];
        center.unk0[2] = cz + ((CAM_SIN(v) * dx + CAM_COS(v) * dz) >> 12);
        center.unk0[0] = cx + ((CAM_COS(v) * dx - CAM_SIN(v) * dz) >> 12);
    }
    for (i = 0; i < 2; i++) {
        dx = D_00433520[i][0] - center.unk0[0];
        dz = D_00433520[i][2] - center.unk0[2];
        dy = D_00433520[i][1] - center.unk0[1];
        h = (sinY * dx + cosY * dz) >> 12;
        pos[i].unk0[0] = (cosY * dx - sinY * dz) >> 12;
        pos[i].unk0[2] = (cosP * h - sinP * dy) >> 12;
        pos[i].unk0[1] = (sinP * h + cosP * dy) >> 12;
        tmp.v[i] = h;
    }
    h = (tmp.v[0] + tmp.v[1]) >> 1;
    y = -0x1C2 - center.unk0[1];
    lim = D_00433330.unkA4 * -0xCE4 / 256;
    depth = (cosP * h - sinP * y) >> 12;
    cy = (sinP * h + cosP * y) >> 12;
    d = dist - depth;
    if (d < lim) {
        d = lim;
    }
    ofs.unk0[2] -= d;
    pos[0].unk0[2] += d;
    pos[1].unk0[2] += d;
    depth += d;
    if (pos[0].unk0[2] == 0) {
        pos[0].unk0[2] = 1;
    }
    if (pos[1].unk0[2] == 0) {
        pos[1].unk0[2] = 1;
    }
    if (pos[0].unk0[0] < pos[1].unk0[0]) {
        pos[0].unk0[0] -= 0x96;
        pos[1].unk0[0] += 0x96;
        l = 0;
        r = 1;
        sl = pos[0].unk0[0] * s / pos[0].unk0[2];
        sr = pos[1].unk0[0] * s / pos[1].unk0[2];
    } else {
        l = 1;
        pos[1].unk0[0] -= 0x96;
        pos[0].unk0[0] += 0x96;
        r = 0;
        sl = pos[1].unk0[0] * s / pos[1].unk0[2];
        sr = pos[0].unk0[0] * s / pos[0].unk0[2];
    }
    if (sr - sl >= 0x189) {
        D_005F6180 = 1;
    } else {
        D_005F6180 = 0;
    }
    scale = heavy ? 8 : 0x10;
    if (sr - sl >= 0x189) {
        q = (s * (pos[r].unk0[0] - pos[l].unk0[0]) - (pos[r].unk0[2] - pos[l].unk0[2]) * 0xC4) / 0x188;
        v = pos[l].unk0[2] - q;
        if (reset != 0) {
            ofs.unk0[2] += v;
            D_008B9148[idx].unk30 = v;
        } else {
            p = &D_008B9148[idx].unk30;
            n = *p;
            if (n < v) {
                v = n + (((v - n) * 4) >> 7);
            } else {
                v = n - (((n - v) * 16) >> 7);
            }
            ofs.unk0[2] += v;
            *p = v;
        }
        pos[0].unk0[2] -= v;
        pos[1].unk0[2] -= v;
        ofs.unk0[0] = ((pos[l].unk0[0] - q * -0xC4 / s) * scale) >> 7;
        if (pos[0].unk0[2] == 0) {
            pos[0].unk0[2] = 1;
        }
        if (pos[1].unk0[2] == 0) {
            pos[1].unk0[2] = 1;
        }
        depth -= v;
    } else if (reset != 0 ||
               pos[r].unk0[2] * 0x44 / s - (pos[r].unk0[0] - pos[l].unk0[0]) < pos[l].unk0[2] * -0x44 / s) {
        ofs.unk0[0] = ((pos[l].unk0[0] + (pos[r].unk0[0] - pos[l].unk0[0]) * pos[l].unk0[2] /
                                             (pos[l].unk0[2] + pos[r].unk0[2])) * scale) >> 7;
        if (reset == 0) {
            v = D_008B9148[idx].unk30;
            n = 0;
            if (v < 0) {
                n = v + -v / 32;
            } else if (v > reset) {
                n = v - v / 8;
            }
            ofs.unk0[2] += n;
            pos[0].unk0[2] -= n;
            pos[1].unk0[2] -= n;
            depth -= n;
            D_008B9148[idx].unk30 = n;
        } else {
            D_008B9148[idx].unk30 = 0;
        }
    } else if (sl < -0x44) {
        ofs.unk0[0] += ((pos[l].unk0[0] - pos[l].unk0[2] * -0x44 / s) * scale) >> 7;
        v = D_008B9148[idx].unk30;
        n = 0;
        if (v < 0) {
            n = v + -v / 32;
        } else if (v > reset) {
            n = v - v / 8;
        }
        ofs.unk0[2] += n;
        pos[0].unk0[2] -= n;
        pos[1].unk0[2] -= n;
        depth -= n;
        D_008B9148[idx].unk30 = n;
    } else if (sr > 0x44) {
        ofs.unk0[0] += ((pos[r].unk0[0] - pos[r].unk0[2] * 0x44 / s) * scale) >> 7;
        v = D_008B9148[idx].unk30;
        n = 0;
        if (v < 0) {
            n = v + -v / 32;
        } else if (v > reset) {
            n = v - v / 8;
        }
        ofs.unk0[2] += n;
        pos[0].unk0[2] -= n;
        pos[1].unk0[2] -= n;
        depth -= n;
        D_008B9148[idx].unk30 = n;
    } else {
        v = D_008B9148[idx].unk30;
        n = 0;
        if (v < 0) {
            n = v + -v / 32;
        } else if (v > reset) {
            n = v - v / 8;
        }
        ofs.unk0[2] += n;
        pos[0].unk0[2] -= n;
        pos[1].unk0[2] -= n;
        depth -= n;
        D_008B9148[idx].unk30 = n;
    }
    ang = 0x8C;
    if (pos[0].unk0[2] == 0) {
        pos[0].unk0[2] = 1;
    }
    if (pos[1].unk0[2] == 0) {
        pos[1].unk0[2] = 1;
    }
    if (pos[0].unk0[1] * s / pos[0].unk0[2] < pos[1].unk0[1] * s / pos[1].unk0[2]) {
        ty = pos[0].unk0[1];
        tz = pos[0].unk0[2];
        hy = D_00433520[0][1];
    } else {
        ty = pos[1].unk0[1];
        tz = pos[1].unk0[2];
        hy = D_00433520[1][1];
    }
    hy = (hy * cosP) >> 12;
    lift = -0x12C;
    if (idx == 1) {
        if (pitch < 0) {
            ang = 0xB4;
        }
        if (pitch > 0) {
            if (hy >= -0x3E8) {
                lift = ((hy + 0x3E8) << 9) / 640;
            }
            if (reset == 0) {
                if (D_005F614C < lift) {
                    if (lift - D_005F614C > 0x1C) {
                        lift = D_005F614C + 0x1C;
                    }
                } else if (lift < D_005F614C) {
                    if (D_005F614C - lift > 0x1C) {
                        lift = D_005F614C - 0x1C;
                    }
                }
            }
        }
        D_005F614C = lift;
    }
    k = (cosP * (ang - s * 0x1C2 / dist)) >> 12;
    den = k + 0x9C;
    pull = ((tz - depth) * -0x9C - s * (ty - cy)) / (den == 0 ? 1 : den) - depth;
    if (pull < 0) {
        pull = 0;
    }
    if (reset != 0) {
        pull = 0;
        D_008B9148[idx].unk28 = 0;
    } else if (pull < D_008B9148[idx].unk2C) {
        d = D_008B9148[idx].unk2C - pull;
        if (d > 0x4B0) {
            d = 0x4B0;
        }
        d = ((d * 0x78) >> 7) - d;
        D_008B9148[idx].unk28 = d;
        pull = D_008B9148[idx].unk2C + d;
    } else if (D_008B9148[idx].unk2C < pull) {
        d = pull - D_008B9148[idx].unk2C;
        if (d > 0x320) {
            d = 0x320;
        }
        if (kind.v[0] == 0x19 || kind.v[1] == 0x19) {
            m = 4;
        } else {
            m = heavy ? 0xA : 0x10;
        }
        d = (d * m) >> 7;
        D_008B9148[idx].unk28 = d;
        pull = D_008B9148[idx].unk2C + d;
    }
    D_008B9148[idx].unk2C = pull;
    if (pull > 0) {
        pos[0].unk0[2] += pull;
        pos[1].unk0[2] += pull;
        ofs.unk0[2] -= pull;
        depth += pull;
    }
    f = 0;
    if (kind.v[0] == 0x17 || kind.v[0] == 0x18 || kind.v[0] == 0x1A) {
        f = 1;
    }
    flag.v[0] = f;
    f = 0;
    if (kind.v[1] == 0x17 || kind.v[1] == 0x18 || kind.v[1] == 0x1A) {
        f = 1;
    }
    flag.v[1] = f;
    tmp = flag;
    z = ofs.unk0[2];
    if (z < 0 && (tmp.v[0] != 0 || tmp.v[1] != 0) &&
        ((fa->unkF3 != 0 && ((fa->unkC0->unk4 & 0x400) || fa->unk135 != 0)) ||
         (fb->unkF3 != 0 && ((fb->unkC0->unk4 & 0x400) || fb->unk135 != 0)))) {
        if (r != 0) {
            if (tmp.v[0] == 0) {
                ofs.unk0[0] = z * 0x17FA00 / D_005F5CD0 / 4096;
            } else {
                ofs.unk0[0] = -z * 0x17FA00 / D_005F5CD0 / 4096;
            }
        } else {
            if (tmp.v[0] != 0) {
                ofs.unk0[0] = z * 0x17FA00 / D_005F5CD0 / 4096;
            } else {
                ofs.unk0[0] = -z * 0x17FA00 / D_005F5CD0 / 4096;
            }
        }
    }
    ofs.unk0[1] = cy - k * depth / s + lift;
    sinP = CAM_SIN(pitch);
    cosP = CAM_COS(pitch);
    cosY = CAM_COS(yaw);
    sinY = CAM_SIN(yaw);
    rz = (ofs.unk0[2] * cosP - ofs.unk0[1] * sinP) >> 12;
    ry = (ofs.unk0[2] * sinP + ofs.unk0[1] * cosP) >> 12;
    nx = (ofs.unk0[0] * cosY + rz * -sinY) >> 12;
    nz = (ofs.unk0[0] * sinY + rz * cosY) >> 12;
    if (reset != 0) {
        key.unk0[1] = center.unk0[1] + ry;
        key.unk0[2] = center.unk0[2] + nz;
        key.unk0[0] = center.unk0[0] + nx;
        key.unk0[3] = pitch;
        key.unk0[4] = yaw;
        key.unk0[5] = roll;
    } else {
        dpos.unk0[1] = center.unk0[0] + nx - key.unk0[0];
        dpos.unk0[2] = center.unk0[1] + ry - key.unk0[1];
        dpos.unk0[3] = center.unk0[2] + nz - key.unk0[2];
        d = pitch - key.unk0[3];
        if (!(d & 0x20000)) {
            dang.v[0] = d & 0x3FFFF;
        } else {
            dang.v[0] = d | 0xFFFC0000;
        }
        d = yaw - key.unk0[4];
        if (!(d & 0x20000)) {
            dang.v[1] = d & 0x3FFFF;
        } else {
            dang.v[1] = d | 0xFFFC0000;
        }
        if (idx == 0) {
            if (dpos.unk0[1] * dpos.unk0[1] + dpos.unk0[3] * dpos.unk0[3] > 0x3E8) {
                D_008B9148[0].unk24 = 1;
            } else {
                D_008B9148[0].unk24 = 0;
            }
        }
        key.unk0[1] += dpos.unk0[2];
        key.unk0[0] += dpos.unk0[1];
        key.unk0[2] += dpos.unk0[3];
        key.unk0[3] += dang.v[0];
        key.unk0[4] += dang.v[1];
        key.unk0[5] = roll;
    }
    func_0038E460(&key, idx);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00391358);
#endif

INCLUDE_RODATA("asm/nonmatchings/stage", D_00508130);
INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_005088D0);

/* NON_MATCHING: same size, the register allocation and the order of the set-up differ (95 lines) */
#if 0
u8 CamScript_Step(s32 n, Unk8B92D0 *out, s32 a2) {
    u8 r;
    s32 i;
    s32 off;
    s32 used;
    u32 mask;
    s32 code;
    s16 d;

    if (n != 0) {
        r = -1;
        for (i = 0; i < 8; i++) {
            D_008B9280[i] = 0;
            D_008B92A0[i] = 0;
            D_008B92C0[i] = 0;
        }
        D_008B92E8.unk3204 = D_008B92E8.unk0;
        D_008B92E8.unk3200 = D_008B92E8.unk0;
        D_005F6C1C = (s32)func_003B8778() + D_00508130[n - 1];
        off = D_005F6C1C & 3;
        D_005F6C1C -= off;
        func_0035C698(D_008B92E8.unk3204, D_005F6C1C, 0x40);
        D_008B92E8.unk3204 += 0x40;
        D_005F6C1C += 0x40;
        D_008B92E8.unk3200 += off;
        D_008B92E8.unk3208 = 0x40 - off;
        if (a2 == 0) {
            r = *D_008B92E8.unk3200++;
            D_008B92E8.unk3208--;
        }
        return r;
    }
    if (D_008B92E8.unk3208 < 0x21) {
        func_0035C698(D_008B92E8.unk3204, D_005F6C1C, 0x40);
        D_008B92E8.unk3204 += 0x40;
        D_008B92E8.unk3208 += 0x40;
        D_005F6C1C += 0x40;
    }
    d = 0;
    used = 2;
    mask = *D_008B92E8.unk3200++;
    mask |= *D_008B92E8.unk3200++ << 8;
    for (i = 0; i < 8; i++) {
        code = mask & 3;
        mask >>= 2;
        switch (code) {
        case 0:
            d = 0;
            break;
        case 1:
            d = D_008B92C0[i];
            break;
        case 2:
            used += 1;
            d = (s8)*D_008B92E8.unk3200++;
            break;
        case 3:
            used += 2;
            d = *D_008B92E8.unk3200++;
            d |= *D_008B92E8.unk3200++ << 8;
            break;
        }
        D_008B92C0[i] = d;
        switch (i) {
        case 0:
            out->unk0[1] = D_008B92A0[0] = (s16)(d + D_008B92A0[0]);
            break;
        case 1:
            out->unk8[1] = D_008B92A0[1] = (s16)(d + D_008B92A0[1]);
            break;
        case 2:
            out->unk0[0] = D_008B92A0[2] = (s16)(d + D_008B92A0[2]);
            break;
        case 3:
            out->unk0[2] = D_008B92A0[3] = (s16)(d + D_008B92A0[3]);
            break;
        case 4:
            out->unk8[0] = D_008B92A0[4] = (s16)(d + D_008B92A0[4]);
            break;
        case 5:
            out->unk8[2] = D_008B92A0[5] = (s16)(d + D_008B92A0[5]);
            break;
        case 6:
            out->unk14 = D_008B92A0[6] = d + D_008B92A0[6];
            break;
        case 7:
            out->unk10 = D_008B92A0[7] = d + D_008B92A0[7];
            break;
        }
    }
    if (a2 != 0) {
        out->unk10 = 0x80;
    }
    D_008B92E8.unk3208 -= used;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", CamScript_Step);
#endif

void func_003927B8(void) {
    D_00433330.unk84 = -1;
    D_005F6C20 = -1;
    D_00433330.unk9C = 0;
}

s32 func_003927D8(void) {
    D_00433330.unk9C = 0;
    D_005F6C20 = -1;
    return 0;
}

/* The side test func_0039BDA8 of the camera script's eye D_008B92D0 (x, z) against the fighters' midpoint and the pair
 * (a.x, b.x) as s16: 1 when it is negative, else 0 [V code]. */
s32 func_003927F0(Fighter *a, Fighter *b) {
    s16 w[2];
    s16 u[2];
    s16 v[2];
    s32 r;

    u[0] = a->unk51C;
    u[1] = b->unk51C;
    v[0] = (a->unk51C + b->unk51C) / 2;
    v[1] = (a->unk524 + b->unk524) / 2;
    w[0] = D_008B92D0.unk0[0];
    w[1] = D_008B92D0.unk0[2];
    r = func_0039BDA8(u, v, w);
    if (r > 0) {
        return 0;
    }
    if (r < 0) {
        return 1;
    }
    return 0;
}

s32 func_00392888(s32 a, s32 b) {
    if (a < 0x68) {
        return 1;
    }
    if (b == 0x9E && a != 0x9F) {
        return 2;
    }
    return 0;
}

INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_005088F0);

/* NON_MATCHING: 8 instructions short (91 lines): the original keeps more values in saved registers (s5 = 0 at the start) and orders the blocks otherwise */
#if 0
s32 func_003928B8(s32 n, Fighter *f, Fighter *o) {
    s32 r = 0;
    s32 a;

    if (n != 0) {
        D_00433330.unk84 = 0;
        a = 0;
        switch (CamScript_Step(n, NULL, 0)) {
        case 0:
        case 4:
            a = (0x400 - (s16)(o->unk1A / 16)) & 0xFFF;
            break;
        case 1:
            a = (0x800 - (s16)(o->unk1A / 16)) & 0xFFF;
            break;
        case 2:
            a = -(s16)(o->unk1A / 16) & 0xFFF;
            break;
        case 3:
        case 5:
            a = -((s16)(o->unk1A / 16) + 0x400) & 0xFFF;
            break;
        }
        CamScript_Step(0, &D_008B92D0, 0);
        D_00433330.unk68 = a;
        D_00433330.unk8C = o->unk4;
        D_00433330.unk90 = o->unk8;
        D_00433330.unk94 = o->unkC;
    } else if (f->unkC0 != D_00433330.unk60 && f->unkE0 > 0) {
        if ((f->unkC0->unk24 >> 23) != 0x11) {
            u32 prev = D_00433330.unk60->unk24 >> 23;

            D_00433330.unk60 = f->unkC0;
            switch (func_00392888(f->unkC0->unk24 >> 23, prev)) {
            case 1:
                func_0038D1B8(func_003927F0(f, o), f, o);
                return -1;
            case 2:
                func_0038D290(0, f, o);
            default:
                D_00433330.unk54 = func_00393BF0(f);
                D_00433330.unk58 = func_00393C78(D_00433330.unk54);
                CamScript_Step(-D_00433330.unk58, NULL, 0);
                CamScript_Step(0, &D_008B92D0, 0);
                D_00433330.unk84 = 0;
                break;
            }
        }
    }
    if (D_00433330.unk58 < 0) {
        r = func_00392B68(f, o);
    } else {
        Cam_Reset();
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003928B8);
#endif

void func_00392B38(s32 a0) {
    CamScript_Step(a0, NULL, 0);
    CamScript_Step(0, &D_008B92D0, 0);
}

/* NON_MATCHING: 11 instructions short (155 lines): the original reaches D_008B92D0 and the script origin through more saved registers; the rotation is scheduled otherwise */
#if 0
s32 func_00392B68(Fighter *f, Fighter *o) {
    Unk8B90B8 c;
    Vec4i eye;
    Vec4i tgt;
    s16 a[4];
    s16 b[4];
    s32 dist;
    s32 s;
    s32 k;
    s32 r;

    *(u64 *)a = *(u64 *)D_008B92D0.unk0;
    dist = D_008B92D0.unk14;
    *(u64 *)b = *(u64 *)D_008B92D0.unk8;
    if (D_00433330.unkA0 != 0) {
        a[2] = -a[2];
        b[2] = -b[2];
    }
    s = gSinTable[D_00433330.unk68 & 0xFFF];
    k = gSinTable[(D_00433330.unk68 & 0xFFF) + 0x400];
    if (D_00433330.unkB0 != 0) {
        a[0] = b[0] + a[0] * D_00433330.unkA4 / dist;
        a[1] = b[1] + a[1] * D_00433330.unkA4 / dist;
        a[2] = b[2] + a[2] * D_00433330.unkA4 / dist;
        dist = D_00433330.unkA4;
    } else {
        a[0] = b[0] + a[0];
        a[1] = b[1] + a[1];
        a[2] = b[2] + a[2];
    }
    eye.unk0[0] = D_00433330.unk8C + (k * a[0] - s * a[2]) / 4096;
    eye.unk0[1] = D_00433330.unk90 + a[1];
    eye.unk0[2] = D_00433330.unk94 + (s * a[0] + k * a[2]) / 4096;
    tgt.unk0[0] = D_00433330.unk8C + (k * b[0] - s * b[2]) / 4096;
    tgt.unk0[1] = D_00433330.unk90 + b[1];
    tgt.unk0[2] = D_00433330.unk94 + (s * b[0] + k * b[2]) / 4096;
    func_0038E308(&c, &eye, &tgt, 0, dist);
    func_0038E460(&c, 2);
    r = D_008B92D0.unk10;
    D_00433330.unk84++;
    CamScript_Step(0, &D_008B92D0, 0);
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00392B68);
#endif

void CamScript_Load(s32 id) {
    s32 *p = func_003B8790();
    s32 i;

    if (p == NULL) {
        exit(1);
    }
    if (id == 0) {
        func_0035D238(0, p, 0x14000);
        D_005F6C24 = *p++;
    } else if ((u32)(id - 2) < 0x28 || (u32)(id - 0x40) < 0x68) {
        func_0035D238(id, p, 0x14000);
        D_005F6C24 = 1;
    } else {
        exit(1);
    }
    func_00393870(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    if (id == 0) {
        for (i = 0; i < D_005F6C24; i++) {
            D_008BC4F8[i] = (s32 *)((u8 *)func_003B8790() + *p);
            D_008BC578[i] = *D_008BC4F8[i]++;
            p++;
        }
    } else {
        D_008BC4F8[0] = func_003B8790();
        D_008BC578[0] = *D_008BC4F8[0]++;
    }
    D_005F6184 = id;
}

/* NON_MATCHING: same size (14 lines): the original has gcc's unfilled `jal atan2f` with the sqrt.s in its delay slot
 * (configure.py fix_compiler_asm moves it there since T17 b4), but here gcc fills the slot with the `ty - ey`
 * subtraction itself; the two angle products keep their temporaries */
#if 0
/* Plays frame `frame` of loaded camera script n (CamScript_Load): eye, target and projection distance from the
 * script's 7-float frame plus the origin D_008BC5F8 (added twice), pulled back by 1.2 and lowered by 8 when
 * func_003BB4D8 says so; sets the projection and the camera angles and position D_00415808. A negative frame returns
 * the script's length; returns 1 once past the last frame [V code; I meaning]. */
s32 func_00392F80(s32 n, s32 frame) {
    s32 end = 0;
    f32 *p;
    f32 ex, ey, ez;
    f32 tx, ty, tz;
    f32 dist;
    f32 dx, dy, dz;
    f32 pitch;
    f32 yaw;
    f32 turn;

    if (frame < 0) {
        return D_008BC578[n];
    }
    if (n < 0) {
        n = 0;
    }
    if (n >= D_005F6C24) {
        n = D_005F6C24 - 1;
    }
    if (frame >= D_008BC578[n]) {
        frame = D_008BC578[n] - 1;
        end = 1;
    }
    p = (f32 *)((u8 *)D_008BC4F8[n] + frame * 0x1C);
    ex = *p++;
    ey = *p++;
    ez = *p++;
    tx = *p++;
    ty = *p++;
    tz = *p++;
    dist = *p;
    ex += D_008BC5F8.unkC;
    ey += D_008BC5F8.unk10;
    ez += D_008BC5F8.unk14;
    tx += D_008BC5F8.unk18;
    ty += D_008BC5F8.unk1C;
    tz += D_008BC5F8.unk20;
    ex += D_008BC5F8.unkC;
    ey += D_008BC5F8.unk10;
    ez += D_008BC5F8.unk14;
    tx += D_008BC5F8.unk18;
    ty += D_008BC5F8.unk1C;
    tz += D_008BC5F8.unk20;
    if (func_003BB4D8() != 0) {
        f32 k = 1.2f;
        dx = ex - tx;
        dy = ey - ty;
        dz = ez - tz;
        ty -= 8.0f;
        ex = tx + dx * k;
        ey = ty + dy * k;
        ez = tz + dz * k;
        dist *= 0.8f;
    }
    tx -= ex;
    tz -= ez;
    ty = atan2f(ty - ey, sqrtf(tx * tx + tz * tz));
    tz = atan2f(tz, tx) - 1.570796f;
#define pitch ty
#define yaw tz
    D_00433330.unkA8 = 1;
    D_00433330.unkAC = 10000;
    Cam_SetDistance(dist);
    D_00415808[2] = 0;
    D_00415808[7] = ez;
    D_00415808[5] = ex;
    D_00415808[6] = ey;
    turn = 6.283184f;
    pitch = pitch * 4096.0f / turn;
    yaw = yaw * 4096.0f / turn;
    D_00415808[1] = yaw;
    D_00415808[0] = pitch;
    return end;
}
#undef pitch
#undef yaw
#else
LIT4_WORD(D_005F517C, 0x3F99999A);
LIT4_WORD(D_005F5180, 0x3F4CCCCD);
LIT4_WORD(D_005F5184, 0x3FC90FD8);
LIT4_WORD(D_005F5188, 0x40C90FD8);
INCLUDE_ASM("asm/nonmatchings/stage", func_00392F80);
#endif

s32 func_003931F0(s32 n) {
    if (n < 0) {
        if (D_008BC5F8.unk0 < 0) {
            return 0;
        }
        func_00392F80(D_008BC5F8.unk0, D_008BC5F8.unk8++);
        if (D_008BC5F8.unk8 < D_008BC5F8.unk4) {
            return 0;
        }
        D_008BC5F8.unk0 = -1;
    } else {
        D_008BC5F8.unk0 = n;
        D_008BC5F8.unk4 = D_008BC578[n];
        D_008BC5F8.unk8 = 0;
    }
    return 0;
}

s32 func_00393290(s32 n) {
    if (n == -1) {
        return D_008BC5F8.unk0;
    }
    if (n == -2) {
        D_008BC5F8.unk0 = -1;
        return 0;
    }
    D_008BC5F8.unk0 = n;
    D_008BC5F8.unk4 = D_008BC578[n];
    D_008BC5F8.unk8 = 0;
    return 0;
}

/* NON_MATCHING: one instruction short (39 lines): gcc fills the first `jal atan2f` delay slot itself (a neg.s), where
 * the original left it to the assembler, which put the sqrt.s there (T17 b4); the scheduling and FPU registers around
 * the two atan2f calls differ */
#if 0
/* Steps the camera script playing in D_008BC5F8 (script unk0, length unk4, frame unk8) for an outside caller: the
 * frame's eye (y and z negated) as floats, its pitch and yaw in 16 bits per turn (negated), roll 0 and the distance;
 * ends the script (unk0 = -1) after its last frame [V code; I meaning]. */
s32 func_003932F0(f32 *ex, f32 *ey, f32 *ez, s32 *pitch, s32 *yaw, s32 *roll, s32 *dist) {
    f32 *p;
    f32 x, y, z;
    f32 dx, dy, dz;
    f32 d;
    f32 a;
    f32 b;
    f32 turn;

    if (D_008BC5F8.unk0 >= 0) {
        p = (f32 *)((u8 *)D_008BC4F8[D_008BC5F8.unk0] + D_008BC5F8.unk8 * 0x1C);
        x = *p++;
        y = *p++;
        z = *p++;
        dx = *p++;
        dy = *p++;
        dz = *p++;
        d = *p;
        dx -= x;
        dy -= y;
        dz -= z;
        a = atan2f(dy, sqrtf(dx * dx + dz * dz));
        b = atan2f(dz, dx);
        turn = 6.283184f;
        a = -a * 65536.0f / turn;
        b = -(b - 1.570796f) * 65536.0f / turn;
        y = -y;
        z = -z;
        *pitch = a;
        *yaw = b;
        *roll = 0;
        *ex = x;
        *ey = y;
        *ez = z;
        *dist = d;
        if (++D_008BC5F8.unk8 >= D_008BC5F8.unk4) {
            D_008BC5F8.unk0 = -1;
        }
    }
    return 0;
}
#else
LIT4_WORD(D_005F518C, 0x3FC90FD8);
LIT4_WORD(D_005F5190, 0x40C90FD8);
INCLUDE_ASM("asm/nonmatchings/stage", func_003932F0);
#endif

s32 func_003934D0(s32 n, s32 a1, Vu0Matrix m) {
    if (n == -1) {
        return D_008BC620.unk0;
    }
    if (n == -2) {
        D_008BC620.unk0 = -1;
        return 0;
    }
    if (n < 0) {
        D_008BC620.unkC = D_005F5CD0;
    }
    D_008BC620.unk0 = 0;
    D_008BC620.unk4 = D_008BC578[0];
    D_008BC620.unk8 = 0;
    sceVu0CopyMatrix(D_008BC620.unk20, m);
    D_005F6144 = 0;
    return 0;
}

/* NON_MATCHING: same size (26 lines): gcc fills the first `jal atan2f` delay slot itself with the load of `dist`, which
 * the original loads before the sqrt (whose sqrt.s then fills the slot, T17 b4); the FPU registers differ from there */
#if 0
/* Steps the demo's scripted camera D_008BC620 (script unk0, length unk4, frame unk8): eye and target from the
 * script frame, projection distance, camera angles and position D_00415808, as func_00392F80 without the origin;
 * after the last frame stops it, restores the stage's camera values (func_0038DBC0) and returns 1
 * [V code; I meaning]. */
s32 func_00393550(void) {
    f32 *p;
    f32 ex, ey, ez;
    f32 tx, ty, tz;
    f32 dist;
    f32 pitch;
    f32 yaw;
    f32 turn;

    if (D_008BC620.unk0 < 0) {
        return 0;
    }
    p = (f32 *)((u8 *)D_008BC4F8[D_008BC620.unk0] + D_008BC620.unk8 * 0x1C);
    ex = *p++;
    ey = *p++;
    ez = *p++;
    tx = *p++;
    ty = *p++;
    tz = *p++;
    dist = *p;
    tx -= ex;
    ty -= ey;
    tz -= ez;
    pitch = atan2f(ty, sqrtf(tx * tx + tz * tz));
    yaw = atan2f(tz, tx) - 1.570796f;
    D_00433330.unkA8 = 1;
    D_00433330.unkAC = 10000;
    Cam_SetDistance(dist);
    turn = 6.283184f;
    pitch = pitch * 4096.0f / turn;
    yaw = yaw * 4096.0f / turn;
    D_00415808[2] = 0;
    D_00415808[7] = ez;
    D_00415808[5] = ex;
    D_00415808[6] = ey;
    D_00415808[1] = yaw;
    D_00415808[0] = pitch;
    if (++D_008BC620.unk8 >= D_008BC620.unk4) {
        D_008BC620.unk0 = -1;
        func_0038DBC0();
        return 1;
    }
    return 0;
}
#else
LIT4_WORD(D_005F5194, 0x3FC90FD8);
LIT4_WORD(D_005F5198, 0x40C90FD8);
INCLUDE_ASM("asm/nonmatchings/stage", func_00393550);
#endif

void func_00393708(void) {
    D_008BC620.unk0 = -1;
}

void func_00393718(s32 unused, s32 n) {
    Vu0Matrix m;

    if (D_005F5CBC != 0 && func_003ABCC0() == 0) {
        D_008BC620.unk10 = n == 4;
        D_008BC620.unkC = D_005F5CD0;
        func_00366EB8(m);
        func_003934D0(n, 0, m);
    }
}

s32 func_00393788(Fighter *a, Fighter *b) {
    if (D_008BC620.unk0 >= 0) {
        if (func_00393550() != 0) {
            D_008BC620.unk0 = -1;
            D_005F6188 = -1;
            Cam_SetDistance(D_008BC620.unkC);
            func_0036DDD0(a, b);
        }
        return 1;
    }
    return 0;
}

s32 func_00393810(void) {
    s32 r = 0;

    if (D_005F5CBC != 0) {
        r = D_008BC620.unk0 >= r; /* fake: compared with r (0) for the original's slti / xori */
    }
    return r;
}

/* The camera script bank of a stage for the demo: stage * 4 for stages 0-0x19 (func_0036D010 loads file 0x40 + it
 * with CamScript_Load), -1 for the others; it also steps the stage's counter D_00433560 through 0-3 [V code]. */
s32 Stage_GetCamScriptBank(u32 stage) {
    if (stage >= 0x1A) {
        return -1;
    }
    D_00433560[stage] = (D_00433560[stage] + 1) & 3;
    return stage * 4;
}

void func_00393870(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f) {
    D_008BC5F8.unkC = a;
    D_008BC5F8.unk10 = b;
    D_008BC5F8.unk14 = c;
    D_008BC5F8.unk18 = d;
    D_008BC5F8.unk1C = e;
    D_008BC5F8.unk20 = f;
}

/* NON_MATCHING: 10 instructions short (71 lines): the original saves two more registers (s2, s3) and keeps constants such as -17 in them */
#if 0
s32 func_00393898(void) {
    u32 i;
    s32 j;
    s32 sum;
    s16 acc;
    s16 first;
    s16 start;
    u16 fl;
    Unk434C78Entry *e;

    if (D_005F618C != 0) {
        return 0;
    }
    for (i = 0; i < 0x134; i++) {
        acc = 0;
        first = -1;
        start = -1;
        j = 0;
        sum = 0;
        e = D_00434C78[i].unk0;
        for (; j < D_00434C78[i].unk4; j++) {
            fl = e->unk0;
            if (fl & 3) {
                first = first <= -1 ? acc : first;
            }
            if (fl & 0x10) {
                e->unk0 = fl = fl & ~0x10;
                if (start < 0) {
                    start = acc <= 0 ? -1 : acc;
                }
            }
            if ((fl & 3) == 1 && e->unk4[0] != e->unk4[1]) {
                e->unk0 = (fl & ~3) | 2;
            }
            {
                s16 w = e->unk2;

                e->unk2 = acc = w + acc;
                sum += w;
            }
            e++;
        }
        e = D_00434C78[i].unk0;
        for (j = 0; j < D_00434C78[i].unk4; j++) {
            e->unk2 = (e->unk2 << 12) / sum;
            e++;
        }
        if (start < 0) {
            D_00434C78[i].unk8 = 0x1000;
        } else {
            D_00434C78[i].unk8 = (start << 12) / sum;
        }
        D_00434C78[i].unkA = first;
        D_00434C78[i].unk6 = 0x1000;
    }
    D_005F618C = 1;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00393898);
#endif

/* NON_MATCHING: 1 line: the side == 0 case of the unkA0 flag is movz v1, s1, s1 here, movz v1, zero, s1 in the original */
#if 0
/* Picks the camera reaction of a move kind (D_00434C78[kind], weighted list): random by rand() & 0xFFF, or by
 * func_003800D8 (the CPU's generator) with the list's own range on stages of camera style 1 / 2 (D_00433330.unkB4,
 * from D_00507FE0) [V code]. */
s32 Cam_PickReaction(u32 kind, s32 side, s32 *idx) {
    Unk434C78Entry *e;
    s32 r = 0;
    s32 i;
    s32 fl;
    s16 v;
    s32 on;

    if (kind >= 0x134) {
        kind = 0;
    }
    switch (D_00433330.unkB4) {
    case 0:
        r = rand() & 0xFFF;
        break;
    case 1:
        r = func_003800D8(D_00434C78[kind].unk8);
        break;
    case 2:
        r = func_003800D8(D_00434C78[kind].unkA);
        break;
    }
    i = 0;
    e = D_00434C78[kind].unk0;
    while ((s16)e->unk2 < r) {
        e++;
        i++;
    }
    *idx = i;
    v = e->unk4[side != 0];
    fl = e->unk0 & 3;
    on = side;
    if (on != 0) {
        on = fl;
    }
    if (fl != 1) {
        on = 0;
    }
    D_005F6194 = kind;
    D_005F6190 = v;
    D_00433330.unkA0 = on;
    return v;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", Cam_PickReaction);
#endif

/* NON_MATCHING: 8 lines: &D_00433330 and unk34 take t1 / t0 the other way round, and the unk34 == 0 select is movz v1, t0, t0 (zero in the original, as Cam_PickReaction) */
#if 0
s32 func_00393BF0(Fighter *f) {
    u32 k = f->unkC0->unk24 >> 23;
    Unk434C78Entry *e;
    s32 flags;
    s32 r;
    s32 on;

    if (k >= 0x134) {
        k = 0;
    }
    e = &D_00434C78[k].unk0[D_00433330.unk5C];
    flags = e->unk0 & 3;
    r = e->unk4[D_00433330.unk34 != 0];
    on = D_00433330.unk34;
    if (on != 0) {
        on = flags;
    }
    if (flags != 1) {
        on = 0;
    }
    D_005F619C = k;
    D_00433330.unkA0 = on;
    D_005F6198 = r;
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00393BF0);
#endif

INCLUDE_RODATA("asm/nonmatchings/stage", D_00508908);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508B60);

s32 func_00393C78(u32 n) {
    return Cam_PresetIndex(n);
}

/* NON_MATCHING: the hoisted unkC4 loads and the shared return-1 block are placed differently (52 lines) */
#if 0
s32 func_00393CB8(Fighter *a, Fighter *b) {
    struct FighterMove *ma;
    struct FighterMove *mb;
    u16 *pa;
    u16 *pb;
    s32 ta;
    s32 tb;
    s32 r;

    if (a->unkE0 > 0 || b->unkE0 < 0) {
        ma = a->unkC0;
        if ((ma->unk24 >> 23) != 3) {
            if (D_00433330.unk48 == 1) {
                return 0;
            }
            mb = b->unkC0;
            pa = (u16 *)ma->unk20;
            ta = -1;
            tb = -1;
            pb = (u16 *)mb->unk20;
            if (!(mb->unk4 & 0x400000)) {
                if (pa != NULL) {
                    for (; pa[0] != 0; pa += 2) {
                        if ((pa[1] & 0xFF00) == 0xC00) {
                            ta = pa[0] - a->unkC4;
                        }
                    }
                }
                if (pb != NULL) {
                    for (; pb[0] != 0; pb += 2) {
                        if ((pb[1] & 0xFF00) == 0xD00) {
                            tb = pb[0] - b->unkC4;
                        }
                    }
                }
                if (ta < 0 && tb < 0) {
                    return 0;
                }
                if (tb < ta) {
                    r = 0;
                    if (ta == 0 && a->unk17A != 0x20) {
                        return 1;
                    }
                    return r;
                }
                r = 0;
                if (tb == 0) {
                    r = 1;
                    if (a->unk17A == 0x20) {
                        r = 0;
                    }
                }
                return r;
            }
        }
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00393CB8);
#endif

/* NON_MATCHING: one instruction short (37 lines): the original keeps &D_00433330 in s3 (one more saved register) and selects the fighter with movn where this has movz */
#if 0
s32 func_00393E08(Fighter *a, Fighter *b, s32 which, s32 side) {
    s32 idx;
    s16 n = 0;
    Fighter *f = which != 0 ? b : a;
    s32 k;

    if (D_00433330.unk54 >= 0) {
        Cam_Reset();
        return -1;
    }
    k = Cam_PickReaction(f->unkC0->unk24 >> 23, side, &idx);
    D_00433330.unk54 = k;
    D_00433330.unk5C = idx;
    if (k < 0) {
        return -1;
    }
    D_00433330.unk58 = Cam_PresetIndex(k);
    if (D_00433330.unk58 < 0) {
        D_00433330.unk48 = 0;
        D_00433330.unk60 = f->unkC0;
        return n;
    }
    if (D_00433330.unk58 > 0) {
        n = D_00508908[Cam_PresetIndex(D_00433330.unk54) - 1].unk0;
        D_00433330.unk60 = f->unkC0;
        D_00433330.unk48 = n < 2;
        return n;
    }
    return -1;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00393E08);
#endif

/* NON_MATCHING: same size (15 lines, mostly branch offsets): the k < 0 exit is a bgez over a jump to the shared return -1 where the original branches straight to the epilogue with -1 in the delay slot */
#if 0
s32 func_00393F60(Fighter *f, Fighter *o, s32 side) {
    s32 idx;
    s16 n = 0;
    s32 k;

    if (D_00433330.unk54 < 0) {
        Cam_Reset();
    fail:
        return -1;
    }
    k = Cam_PickReaction(f->unkC0->unk24 >> 23, side, &idx);
    D_00433330.unk54 = k;
    D_00433330.unk5C = idx;
    if (k < 0) {
        return -1;
    }
    D_00433330.unk58 = Cam_PresetIndex(k);
    if (D_00433330.unk58 < 0) {
        D_00433330.unk60 = f->unkC0;
        D_00433330.unk48 = 0;
        goto end;
    }
    if (D_00433330.unk58 <= 0) {
        goto fail;
    }
    n = D_00508908[Cam_PresetIndex(D_00433330.unk54) - 1].unk0;
    D_00433330.unk48 = n < 2;
end:
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00393F60);
#endif

s32 func_003940B0(void) {
    return D_00508908[Cam_PresetIndex(D_00433330.unk54) - 1].unk2;
}

s32 func_00394110(s32 n, s32 *a, s32 *b) {
    *a = D_00508908[n].unk4[rand() & 3] << 5;
    *b = D_00508908[n].unkC[rand() & 3] << 6;
    if (*a < 0 && D_00433330.unkB4 == 2) {
        *a = 0;
    }
    if (D_00433330.unkB4 != 0) {
        *a /= 2;
    }
    return 0;
}

INCLUDE_RODATA("asm/nonmatchings/stage", D_00508BA0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508BC0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508BE0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508C00);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508C40);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508C60);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508C80);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508CA0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508CC0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508CE0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508D00);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508D40);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508D60);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508D80);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508DA0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508DC0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508DE0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508E00);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508E20);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508E40);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508E60);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508E80);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508EA0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508EC0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508EE0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508F00);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508F20);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508F40);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508F60);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508F80);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508FA0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508FC0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00508FE0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509000);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509020);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509040);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509060);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509080);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005090A0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005090C0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005090E0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509100);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509120);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509140);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509160);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509180);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005091A0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005091C0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005091E0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509200);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509220);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509240);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509260);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509280);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005092A0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005092C0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005092E0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509300);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509320);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509340);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509360);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509380);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005093A0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005093C0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005093E0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509400);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509420);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509440);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509460);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509480);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005094A0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005094C0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005094E0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509500);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509520);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509540);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509560);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509580);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005095A0);
INCLUDE_RODATA("asm/nonmatchings/stage", D_005095C0);

/* NON_MATCHING: 3 instructions short (71 lines): the normalisation loops take other registers and the original keeps an extra copy (move t8, t6) */
#if 0
s32 func_003941F0(void) {
    u32 i;
    s32 j;
    s32 sum;
    s16 acc;
    Unk5095C0Entry *e;

    if (D_005F61A0 != 0) {
        return 0;
    }
    for (i = 0; i < 0x4C; i++) {
        sum = 0;
        e = D_005095C0[i].unk0;
        acc = 0;
        for (j = 0; j < D_005095C0[i].unk4; j++) {
            s16 w = e->unk2;

            e->unk2 = acc = w + acc;
            sum += w;
            e++;
        }
        e = D_005095C0[i].unk0;
        for (j = 0; j < D_005095C0[i].unk4; j++) {
            e->unk2 = (e->unk2 << 12) / sum;
            e++;
        }
    }
    D_005F61A0 = 1;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003941F0);
#endif

s32 func_00394300(s32 n, s32 k) {
    s32 r = rand() & 0xFFF;
    Unk5095C0Entry *e;
    s16 *p;

    n -= 0x12C;
    e = D_005095C0[n].unk0;
    while (e->unk2 < r) {
        e++;
        if (e->unk2 > 0x1000) {
            return 0;
        }
    }
    p = e->unk4[k];
    if (p[0] == -1) {
        return 0;
    }
    D_005F6C28 = p;
    return 1;
}

INCLUDE_RODATA("asm/nonmatchings/stage", D_00509700);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509820);

/* NON_MATCHING: one instruction long (61 lines): register allocation and the order of the prologue and argument copies */
#if 0
s32 func_003943A8(Fighter *a, Fighter *b) {
    Fighter *f = a;
    Fighter *o = b;
    const s32 *r = NULL;
    s32 kind = 0;
    s32 side;
    s32 sel;
    u32 k;
    u32 i;
    s16 s;

    if (a->unk131 != 0 && b->unk131 != 0) {
        return -1;
    }
    for (side = 0; side < 2; side++, f = b, o = a) {
        k = o->unkC0->unk24 >> 23;
        if (f->unk131 == 0 && k - 0x23 >= 0x44) {
            continue;
        }
        sel = 0;
        if (k - 0x1C < 5) {
            kind = 1;
        } else if (k - 0x23 < 0x44) {
            kind = 2;
        } else {
            sel = 2;
            if (f->unk13A == 0) {
                sel = f->unk13B != 0 ? 1 : 0;
            }
        }
        switch (kind) {
        case 0:
            r = D_00509820[0];
            break;
        case 1:
            r = D_00509700[k];
            break;
        case 2:
            r = D_00509700[k - 2];
            break;
        }
        for (i = 0; i < 0x4D; i++, r += 3) {
            if (sel == 3) {
                continue;
            }
            if (!(o->unk28 == r[0] && o->unk10C == r[1] && kind == 0) && kind == 0) {
                continue;
            }
            if (func_00394300(r[2], sel) == 0) {
                return -1;
            }
            s = *D_005F6C28;
            if (kind == 2 && s != 9 && s != 0x11) {
                if (s == 4 && f->unk131 == 0) {
                    return -1;
                }
                D_008BC680[0] = f->unk51C;
                D_008BC680[1] = f->unk520;
                D_008BC680[2] = f->unk524;
            } else {
                D_008BC680[0] = ((s32 *)&f->unk39C)[0];
                D_008BC680[1] = ((s32 *)&f->unk39C)[1];
                D_008BC680[2] = ((s32 *)&f->unk39C)[2];
            }
            D_005F6C2C = 0;
            D_00433330.unk7C[o->unk20] = (s32)o->unkC0;
            return side;
        }
    }
    D_005F6C28 = NULL;
    return -1;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003943A8);
#endif

s32 func_00394610(void) {
    D_00433330.unk58 = 1;
    return D_005F6C28[2];
}

s32 func_00394628(void) {
    return D_005F6C28[3];
}

s32 func_00394638(void) {
    return D_005F6C28[1];
}

INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_00509BC0);

/* NON_MATCHING: typed draft, 7 instructions long in cases 6 and 9 (698 lines): same frame layout; first / dist / f take other saved registers, the original spills %hi(D_00433330) across func_00391058 (frame 0x170 for 0x160), and case 3's facing fix-up is a movz instead of a bgez branch */
#if 0
/* The throw camera definition D_005F6C28 points to (s16 words read by func_003953B0 / func_00394610 ...) [V offsets;
 * I meanings]. */
typedef struct {
    /* 0x00 */ s16 unk0;     /* kind: the case of func_00394648 */
    /* 0x02 */ s16 unk2[3];
    /* 0x08 */ s32 unk8;     /* camera distance, interpolated towards unkC over the throw */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s16 unk10[4]; /* pitch choices (unk10 start, unk12 end in the interpolated kinds) */
    /* 0x18 */ s16 unk18[4]; /* yaw choices (unk18 start, unk1A end in the interpolated kinds) */
} ThrowCamDef;

#define THROW_CAM ((ThrowCamDef *)D_005F6C28)

GteMatrix *Gte_RotMatrixYXZ(GteSVector *r, GteMatrix *m);
GteVector *Gte_ApplyMatrixLV(GteMatrix *m, GteVector *v, GteVector *out);

/* A random pitch and yaw offset from the definition's four choices; no pitch while D_00433330.unkB4 is 1 or 2 [V]. */
static inline void ThrowCam_PickAngles(s32 *pitch, s32 *yaw) {
    s32 p = THROW_CAM->unk10[rand() & 3] << 6;
    s32 y = THROW_CAM->unk18[rand() & 3] << 6;

    if (D_00433330.unkB4 != 0) {
        if (D_00433330.unkB4 >= 0) {
            if (D_00433330.unkB4 <= 2) {
                p = 0;
            }
        }
    }
    *pitch = p;
    *yaw = y;
}

/* Keeps a camera height (y, negative is up) between -0x578 and -0x384 [V]. */
#define THROW_CAM_CLAMP_Y(y)       \
    if ((y) > -0x384) {            \
        (y) = -0x384;              \
    } else if ((y) < -0x578) {     \
        (y) = -0x578;              \
    }

/* The throw cameras: by the kind of the definition D_005F6C28, an orbit key (0), a key orbiting the target at an
 * interpolated distance (1), a key behind the thrower's facing (3-5, 11-13) or along an interpolated yaw/pitch (6-8,
 * 14-16) with the target easing halfway to the fighter each frame, the same around a fixed target (9, 17); other
 * kinds reset the camera. first is set on the first frame (new random angles), dist the stage distance, side selects
 * the fighter followed [V code; I roles]. */
s32 func_00394648(Fighter *a, Fighter *b, s32 first, s32 dist, s32 side) {
    ThrowCamDef *def = THROW_CAM;
    Fighter *f = side == 0 ? a : b;
    s32 pitch;
    s32 yaw;
    s32 roll;
    s32 t;
    s32 d;
    s32 yawA;
    s32 pitchA;
    s32 cy;
    s32 sy;
    s32 cp;
    s32 sp;
    s32 *pos;
    s32 add;

    switch (def->unk0) {
    case 0: {
        s32 p0;
        s32 y0;

        dist = dist * 0x72 / 128;
        if (first != 0) {
            func_003800D8(0xE3);
            yaw = func_00391058(a, b);
            ThrowCam_PickAngles(&p0, &y0);
            if (D_00433330.unk34 == 0) {
                yaw += y0;
            } else {
                yaw -= y0;
            }
            pitch = p0;
            roll = 0;
            D_00433330.unk68 = yaw;
            D_00433330.unk64 = pitch;
            D_00433330.unk6C = roll;
        } else {
            yaw = D_00433330.unk68;
            pitch = D_00433330.unk64;
            roll = D_00433330.unk6C;
        }
        func_0038FD90((s32)a, (s32)b, pitch, yaw, roll, first, dist, 1);
        break;
    }
    case 1: {
        s32 p1;
        s32 y1;
        Unk8B90B8 key;
        Vec4i from;

        if (first != 0) {
            func_003800D8(0xE3);
            yaw = func_00391058(a, b);
            ThrowCam_PickAngles(&p1, &y1);
            if (D_00433330.unk34 == 0) {
                yaw += y1;
            } else {
                yaw -= y1;
            }
            pitch = p1;
            D_00433330.unk68 = yaw;
            D_00433330.unk64 = pitch;
            D_00433330.unk6C = 0;
        } else {
            pitch = D_00433330.unk64;
            yaw = D_00433330.unk68;
        }
        t = (D_00433330.unk4C << 12) / D_00433330.unk50 - 0x400;
        if (t <= 0) {
            t = 0;
        }
        yawA = ((yaw / 64) + 0x400) & 0xFFF;
        pitchA = (pitch / 64) & 0xFFF;
        cy = gSinTable[yawA + 0x400];
        sy = gSinTable[yawA];
        cp = gSinTable[pitchA + 0x400];
        sp = gSinTable[pitchA];
        d = (THROW_CAM->unkC + (THROW_CAM->unk8 - THROW_CAM->unkC) * t / 4096) * D_00433330.unkA4 / 955;
        sy = d * sy;
        cy = d * cy;
        sp = d * sp;
        sy = sy / 4096 * cp;
        cy = cy / 4096 * cp;
        from.unk0[0] = D_008BC680[0] - cy / 4096;
        from.unk0[1] = D_008BC680[1] - sp / 4096;
        from.unk0[2] = D_008BC680[2] - sy / 4096;
        func_0038E308(&key, &from, (Vec4i *)D_008BC680, 0, D_00433330.unkA4);
        func_0038E460(&key, 1);
        break;
    }
    case 2:
        Cam_Reset();
        break;
    case 3:
    case 4:
    case 5:
    case 11:
    case 12:
    case 13: {
        GteVector v;
        GteSVector r;
        GteMatrix m;
        Vec4i from;
        Unk8B90B8 key;
        s16 p0;
        s16 p1;
        s16 y0;
        s16 y1;
        s32 facing;

        memset(&v, 0, sizeof(v));
        pos = &f->unk51C;
        p0 = THROW_CAM->unk10[0];
        p1 = THROW_CAM->unk10[1];
        y0 = THROW_CAM->unk18[0];
        y1 = THROW_CAM->unk18[1];
        if (D_00433330.unk34 != 0) {
            y1 = 0x1000 - y1;
            y0 = 0x1000 - y0;
        }
        t = ((D_00433330.unk50 - D_00433330.unk4C) << 12) / D_00433330.unk50;
        facing = f->unk3C / 16;
        facing = facing % 4096;
        if (t <= 0) {
            t = 0;
        }
        p0 = p0 + (p1 - p0) * t / 4096;
        y0 = y0 + (y1 - y0) * t / 4096;
        if (facing < 0) {
            facing += 0x1000;
        }
        if (abs(D_00433330.unk74 - facing) <= 0x400) {
            D_00433330.unk74 = facing;
        }
        r.vx = p0;
        r.vz = 0;
        r.vy = y0 + (u16)D_00433330.unk74;
        v.vz = (THROW_CAM->unk8 + (THROW_CAM->unkC - THROW_CAM->unk8) * t / 4096) * D_00433330.unkA4 / 955;
        Gte_RotMatrixYXZ(&r, &m);
        Gte_ApplyMatrixLV(&m, &v, &v);
        if (((f->unkC0->unk24 >> 20) & 3) == 2) {
            D_005F6C2C = 1;
        }
        if (D_005F6C2C == 0) {
            add = THROW_CAM->unk10[2];
            D_008BC680[0] += (pos[0] - D_008BC680[0]) / 2;
            from.unk0[0] = D_008BC680[0] + v.vx;
            D_008BC680[1] += (pos[1] - D_008BC680[1]) / 2;
            from.unk0[1] = D_008BC680[1] + v.vy;
            D_008BC680[2] += (pos[2] - D_008BC680[2]) / 2;
            from.unk0[2] = D_008BC680[2] + v.vz;
            THROW_CAM_CLAMP_Y(D_008BC680[1]);
            THROW_CAM_CLAMP_Y(from.unk0[1]);
            func_0038E308(&key, &from, (Vec4i *)D_008BC680, 0, D_00433330.unkA4);
            key.unk0[1] += add;
            func_0038E460(&key, 1);
        } else {
            func_0038E408(&key, 3);
            func_0038E460(&key, 1);
        }
        break;
    }
    case 6:
    case 7:
    case 8:
    case 14:
    case 15:
    case 16: {
        Unk8B90B8 key;
        Vec4i from;
        s16 y1 = def->unk18[1];
        s16 p0 = def->unk10[0];
        s16 p1 = def->unk10[1];
        s16 y0 = def->unk18[0];

        if (D_00433330.unk4C == D_00433330.unk50 - 2 || D_00433330.unk4C == D_00433330.unk50 - 1) {
            yaw = func_00391058(a, b);
            D_00433330.unk74 = yaw;
        } else {
            yaw = D_00433330.unk74;
        }
        t = ((D_00433330.unk50 - D_00433330.unk4C) << 12) / D_00433330.unk50;
        if (t <= 0) {
            t = 0;
        }
        pitch = (p0 + (p1 - p0) * t / 4096) << 6;
        add = (y0 + (y1 - y0) * t / 4096) << 6;
        if (D_00433330.unk34 == 0) {
            yaw += add;
        } else {
            yaw -= add;
        }
        D_00433330.unk68 = yaw;
        D_00433330.unk64 = pitch;
        D_00433330.unk6C = 0;
        t = ((D_00433330.unk50 - D_00433330.unk4C) << 12) / D_00433330.unk50;
        pitchA = (pitch / 64) & 0xFFF;
        yawA = ((yaw / 64) + 0x400) & 0xFFF;
        pos = &f->unk51C;
        cp = gSinTable[pitchA + 0x400];
        if (t <= 0) {
            t = 0;
        }
        d = (THROW_CAM->unk8 + (THROW_CAM->unkC - THROW_CAM->unk8) * t / 4096) * D_00433330.unkA4 / 955;
        sp = gSinTable[pitchA];
        sy = gSinTable[yawA];
        cy = gSinTable[yawA + 0x400];
        if (((f->unkC0->unk24 >> 20) & 3) == 2) {
            D_005F6C2C = 1;
        }
        if (D_005F6C2C == 0) {
            sy = d * sy;
            cy = d * cy;
            add = THROW_CAM->unk10[2];
            D_008BC680[0] += (pos[0] - D_008BC680[0]) / 2;
            sp = d * sp;
            cy = cy / 4096 * cp;
            D_008BC680[1] += (pos[1] - D_008BC680[1]) / 2;
            sy = sy / 4096 * cp;
            D_008BC680[2] += (pos[2] - D_008BC680[2]) / 2;
            from.unk0[0] = D_008BC680[0] - cy / 4096;
            from.unk0[1] = D_008BC680[1] - sp / 4096;
            from.unk0[2] = D_008BC680[2] - sy / 4096;
            THROW_CAM_CLAMP_Y(D_008BC680[1]);
            THROW_CAM_CLAMP_Y(from.unk0[1]);
            func_0038E308(&key, &from, (Vec4i *)D_008BC680, 0, D_00433330.unkA4);
            key.unk0[1] += add;
            func_0038E460(&key, 1);
        } else {
            func_0038E408(&key, 3);
            func_0038E460(&key, 1);
        }
        break;
    }
    case 9:
    case 17: {
        Unk8B90B8 key;
        Vec4i from;
        s16 y1 = def->unk18[1];
        s16 p0 = def->unk10[0];
        s16 p1 = def->unk10[1];
        s16 y0 = def->unk18[0];

        if (D_00433330.unk4C == D_00433330.unk50 - 2 || D_00433330.unk4C == D_00433330.unk50 - 1) {
            yaw = func_00391058(a, b);
            D_00433330.unk74 = yaw;
        } else {
            yaw = D_00433330.unk74;
        }
        t = ((D_00433330.unk50 - D_00433330.unk4C) << 12) / D_00433330.unk50;
        if (t <= 0) {
            t = 0;
        }
        pitch = (p0 + (p1 - p0) * t / 4096) << 6;
        add = (y0 + (y1 - y0) * t / 4096) << 6;
        if (D_00433330.unk34 == 0) {
            yaw += add;
        } else {
            yaw -= add;
        }
        D_00433330.unk68 = yaw;
        D_00433330.unk64 = pitch;
        D_00433330.unk6C = 0;
        t = ((D_00433330.unk50 - D_00433330.unk4C) << 12) / D_00433330.unk50;
        if (t <= 0) {
            t = 0;
        }
        yawA = ((yaw / 64) + 0x400) & 0xFFF;
        pitchA = (pitch / 64) & 0xFFF;
        cy = gSinTable[yawA + 0x400];
        sy = gSinTable[yawA];
        cp = gSinTable[pitchA + 0x400];
        sp = gSinTable[pitchA];
        d = (THROW_CAM->unk8 + (THROW_CAM->unkC - THROW_CAM->unk8) * t / 4096) * D_00433330.unkA4 / 955;
        sy = d * sy;
        cy = d * cy;
        sp = d * sp;
        sy = sy / 4096 * cp;
        cy = cy / 4096 * cp;
        from.unk0[0] = D_008BC680[0] - cy / 4096;
        from.unk0[1] = D_008BC680[1] - sp / 4096;
        from.unk0[2] = D_008BC680[2] - sy / 4096;
        THROW_CAM_CLAMP_Y(D_008BC680[1]);
        THROW_CAM_CLAMP_Y(from.unk0[1]);
        func_0038E308(&key, &from, (Vec4i *)D_008BC680, 0, D_00433330.unkA4);
        func_0038E460(&key, 1);
        break;
    }
    default:
        Cam_Reset();
        break;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00394648);
#endif

s32 func_003953B0(void) {
    return D_005F6C28[0];
}

s32 func_003953C0(Fighter *a, Fighter *b) {
    return func_0039C0E0(a->unk524 - b->unk524, a->unk51C - b->unk51C);
}

s32 func_003953F0(void) {
    return D_005F6C28[14];
}

INCLUDE_RODATA("asm/nonmatchings/stage", D_00509C10);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509C20);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509C2C);

/* NON_MATCHING: the loop and the 0x5A / 0x78 choice are laid out differently (36 lines) */
#if 0
s32 func_00395400(s32 again, Fighter *a, Fighter *b) {
    s16 t;

    if (again != 0) {
        do {
            D_008BC690.unk4 = 0;
            D_005F61A4++;
        } while (D_00433330.unkB4 != 0 && D_00509C2C[0] != 0);
    } else {
        D_008BC690.unk4 = 0;
    }
    if (gRoundNumber == 1) {
        t = 0x78;
    } else {
        t = 0x5A;
    }
    D_008BC690.unk0 = D_008BC690.unk2 = t;
    func_00390FA8((s32)a, (s32)b, 1, D_00433330.unkA4 * 0x76C / 256);
    func_0038E408(&D_008BC698, 0);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00395400);
#endif

/* NON_MATCHING: 5 instructions short (74 lines): an s16 load and a compare are hoisted, and two branch-likely delay slots are filled otherwise */
#if 0
s32 func_003954E8(void) {
    Vec4i from;
    Vec4i to;
    Unk8B90B8 c;
    s32 z;
    s32 t;
    s32 s;
    const s16 *r;
    s16 n;

    func_0038E408(&c, 0);
    t = D_008BC690.unk0 - D_008BC690.unk2 + 0x5A;
    if (t <= 0) {
        t = 1;
    }
    r = D_00509C20[D_008BC690.unk4];
    z = D_008BC690.unk8.unk0[2];
    s = gSinTable[((t << 10) / 90 + 0xC00) & 0xFFF] + 0x1000;
    from.unk0[0] = D_008BC690.unk8.unk0[0] - r[0] * s / 4096;
    from.unk0[1] = D_008BC690.unk8.unk0[1] - r[1] * s / 4096;
    from.unk0[2] = z - r[2] * s / 4096;
    to.unk0[0] = D_008BC690.unk8.unk0[0] + r[3] * s / 4096;
    to.unk0[1] = D_008BC690.unk8.unk0[1] + r[4] * s / 4096;
    to.unk0[2] = r[5] * s / 4096;
    func_0038E308(&c, &from, &to, 0, c.unk0[6]);
    func_0038E460(&c, 2);
    n = --D_008BC690.unk0;
    if (n >= 14) {
        return 0x1000;
    }
    return (n << 12) / 13;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003954E8);
#endif

/* NON_MATCHING: `sd ra` is scheduled after the store to unk8 instead of before it */
#if 0
s32 func_00395718(s32 a0, s32 a1, s32 a2) {
    Unk8BC6B8 *p = &D_008BC6B8;

    p->unk0 = 0;
    D_005F6140 = 0;
    p->unk4 = a0;
    p->unk8 = a1;
    func_0035BB58(a0);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00395718);
#endif

s32 func_00395750(void) {
    Unk8BC6B8 *p = &D_008BC6B8;

    D_005F6140 = 0;
    if (p->unk0 == 0) {
        func_0035BD20(p->unk4, p->unk8);
    } else {
        func_0035BD20(p->unk4, p->unk8);
    }
    return 0;
}

/* NON_MATCHING: 3 instructions long (149 lines): the LCG product and the distance terms are scheduled differently from the start */
#if 0
/* The special-ending camera: advances the LCG D_005F5CF4 = (D_005F5CF4 + 1) * 0x10DCD and uses its high half for a
 * random swing of the angle [V code]. */
void func_003957A0(Fighter *a, Fighter *b) {
    s32 *pa = &a->unk51C;
    s32 *pb;
    s32 *ja = CAM_JOINT(a, 2);
    s32 *jb;
    s32 d;
    s32 ang;
    s32 dist;
    s32 c;
    s32 s;

    D_005F5CF4 = (u64)(u32)(D_005F5CF4 + 1) * 0x10DCD;
    pb = b != NULL ? &b->unk51C : pa;
    jb = b == NULL ? ja : CAM_JOINT(b, 2);
    D_00433330.unk4[0] = 0x1000;
    D_00433330.unk4[1] = 0;
    D_00433330.unk4[2] = 0;
    d = func_003D7DA8(pb);
    ang = (0x400 - d / 16 - 0xE3 + (s32)(((u64)D_005F5CF6 * 0x1C7) >> 16)) & 0xFFF;
    c = gSinTable[ang + 0x400];
    s = gSinTable[ang];
    D_008BC6C8.unk0[1] = -0x4B0;
    dist = D_00433330.unkA4 * 0x1B58 / 0x3BB + D_00433330.unkA4 * 0x258 / 0x3BB;
    D_008BC6C8.unk0[0] = (a->unk51C + pb[0]) / 2 + dist * c / 4096;
    D_008BC6C8.unk0[2] = (pa[2] + pb[2]) / 2 + dist * s / 4096;
    D_008BC6C8.unk0[3] = (a->unk51C + pb[0]) / 2;
    D_008BC6C8.unk10[0] = (ja[1] + jb[1]) / 2;
    if (D_008BC6C8.unk10[0] >= -0x590) {
        D_008BC6C8.unk10[0] = -0x591;
    }
    D_008BC6C8.unk10[3] = 0;
    D_008BC6C8.unk10[2] = 0;
    D_008BC6C8.unk10[1] = (pa[2] + pb[2]) / 2;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003957A0);
#endif

/* NON_MATCHING: one instruction short (39 lines): the divide-by-zero trap of 955 and the 191 constant are placed differently, registers follow */
#if 0
/* Steps the camera record D_008BC6C8 of the special ending: the eye closes in on the target while it is farther than
 * the stage distance scaled by 7000/955 (by unkA4 / 191 a frame), the target eases towards the fighters' midpoint
 * (6/128 a frame, its height capped at -0x190 and kept 0x190 below the eye on stages of camera style 2); the key is
 * then made current with its pitch kept at or below 0 (func_0035B690 / func_0035B858) [V code; I meaning]. */
void func_003959C8(Fighter *a, Fighter *b, s32 a2) {
    Unk8BC6C8 *c = &D_008BC6C8;
    s32 dx = c->unk0[3] - c->unk0[0];
    s32 dz = c->unk10[1] - c->unk0[2];
    s32 d = func_003985D8(dx * dx + dz * dz);
    s32 *ja = CAM_JOINT(a, 2);
    s32 *jb = b == NULL ? ja : CAM_JOINT(b, 2);
    s32 *pa = &a->unk51C;
    s32 *pb = b != NULL ? &b->unk51C : pa;
    s32 y = (ja[1] + jb[1]) >> 1;
    s32 n;
    s32 k;
    s32 v[12];

    k = d;
    if (k == 0) {
        k = 1;
    }
    if (d - D_00433330.unkA4 * 7000 / 955 > 0) {
        n = D_00433330.unkA4 / 191;
        c->unk0[0] += n * dx / k;
        c->unk0[2] += n * dz / k;
    }
    if (y >= -0x18F) {
        y = -0x190;
    }
    c->unk0[3] += (((a->unk51C + pb[0]) >> 1) - c->unk0[3]) * 6 / 128;
    c->unk10[0] += (y - c->unk10[0]) * 6 / 128;
    c->unk10[1] += (((pa[2] + pb[2]) >> 1) - c->unk10[1]) * 6 / 128;
    if (D_00433330.unkB4 == 2 && c->unk0[1] - c->unk10[0] < -0x190) {
        c->unk10[0] = c->unk0[1] + 0x190;
    }
    func_0035B690(&D_008BC6C8, v);
    n = v[0] & 0xFFF;
    if (v[0] & 0x800) {
        n -= 0x1000;
    }
    if (n > 0) {
        n = 0;
    }
    v[0] = n;
    func_0035B858(v);
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003959C8);
#endif

extern s32 D_004333D4[]; /* D_004333D4[0] read by its own symbol */

void func_00395BF0(Fighter *f) {
    s32 v[12];
    s32 off = D_005F6C58 - 0x400;

    v[0] = 0x400;
    v[1] = 0xC00 - f->unk1A / 16;
    v[1] += off;
    v[2] = 0;
    v[5] = f->unk51C;
    v[6] = -(D_004333D4[0] * 8000 / 0x3BB) - D_005F6C54;
    v[7] = f->unk524;
    func_0035B858(v);
}

s32 func_00395C90(s32 a0, s32 a1, s32 a2) {
    a2 &= 1;
    D_005F6140 = 0;
    switch (a2) {
    case 0:
    case 1:
        func_003959C8((Fighter *)a0, (Fighter *)a1, a2);
        break;
    default:
        func_003959C8((Fighter *)a0, NULL, a2);
        break;
    }
    return 0;
}

s32 func_00395CD8(s32 a0, s32 a1, s32 a2) {
    func_00390FA8(a0, a1, (u32)a2 >> 31, D_00433330.unkA4 * 0x76C / 256);
    return 0;
}

s32 func_00395D20(s32 a0, s32 a1, s32 a2) {
    if (a2 < 0) {
        switch (D_00433330.unkB4) {
        case 0:
            D_005F61A8 = D_00436230[rand() & 7] << 6;
            break;
        case 1:
            D_005F61A8 = D_00436230[rand() & 3] << 5;
            break;
        case 2:
            D_005F61A8 = D_00436230[rand() & 1] << 4;
            break;
        }
        if (func_003403B8() == 0xA) {
            D_005F61A8 = -0x3C;
        }
        D_005F61AC = (rand() & 0xFFF) << 6;
        D_005F61B4 = D_00436250[rand() & 1] << 6;
    }
    D_005F61AC = (D_005F61AC + D_005F61B4) & 0x3FFFF;
    func_00391310(a0, a1, 1, D_00433330.unkA4 * 0x76C / 256 * 8 / 10, D_005F61A8, D_005F61AC, D_005F61B0);
    return 0;
}

/* NON_MATCHING: same size and control flow (81 lines): register allocation from the angle on (the distance r in t2 for t1, the eye in t5/t4 for t4/t3) */
#if 0
extern u16 gFrameCount;
extern s32 D_005F6C30;
extern s32 D_005F6C34;
extern s32 D_005F6C38;
extern s32 D_005F6C3C;
extern s32 D_005F6C40;
extern s32 D_005F6C44;
extern s32 D_005F6C48;

/* Demo camera kind 2 (the sweep): a negative kind restarts it (kind ~kind, frame 0, the angle from a->unk1A); the
 * fighters' positions are sampled on odd frames only. The eye sits behind fighter a's facing at a distance from the
 * stage distance (unkA4 * 0x76C / 256 * 1.3 * 0.75), sideways by a sweep of 8000 / 720 a frame and at a height that
 * bobs with the frame (kinds 1 and 2), and looks at the fighters' midpoint (key 0) [V code; I meaning]. */
s32 func_00395EE0(Fighter *a, Fighter *b, s32 kind) {
    s32 r = D_00433330.unkA4 * 0x76C / 256 * 13 / 10 * 0x60 / 128;
    s32 side = 0;
    s32 y = 0;
    Unk8B90B8 k;
    s32 ang;
    s32 s;
    s32 c;

    if (D_00433330.unkC0 < 0 || !(gFrameCount & 1) || kind < 0) {
        D_005F6C3C = a->unk51C;
        D_005F6C40 = a->unk524;
        D_005F6C44 = b->unk51C;
        D_005F6C48 = b->unk524;
        if (kind < 0) {
            D_005F6C30 = ~kind;
            D_005F6C38 = a->unk1A;
            D_005F6C34 = 0;
        }
    }
    switch (D_005F6C30) {
    case 0:
        y = D_00433330.unkB4 == 0 ? -0x898 : -0x530;
        side = 0;
        break;
    case 1:
        if (D_00433330.unkB4 == 0) {
            y = gSinTable[(D_005F6C34 * 0x800 / 720) & 0xFFF] * 500 / 4096 - 0x5DC;
        } else {
            y = gSinTable[(D_005F6C34 * 0x800 / 720) & 0xFFF] * 100 / 4096 - 0x4B0;
        }
        side = D_005F6C34 * 8000 / 720 - 4000;
        break;
    case 2:
        if (D_00433330.unkB4 == 0) {
            y = -0x5DC - gSinTable[(D_005F6C34 * 0x800 / 720) & 0xFFF] * 500 / 4096;
        } else {
            y = gSinTable[(D_005F6C34 * 0x800 / 720) & 0xFFF] * 100 / 4096 - 0x4B0;
        }
        side = D_005F6C34 * 8000 / 720 - 4000;
        break;
    }
    D_005F6C34++;
    if (a->unkF2 == 0) {
        ang = D_005F6C38 = 0x400 - a->unk1A / 16;
    } else {
        ang = D_005F6C38;
    }
    ang = (ang / 64) & 0xFFF;
    s = gSinTable[ang];
    c = gSinTable[ang + 0x400];
    k.unk0[1] = y;
    k.unk0[0] = D_005F6C3C - r * c / 4096 + side * s / 4096;
    k.unk0[2] = D_005F6C40 - r * s / 4096 - side * c / 4096;
    k.unk0[4] = (func_00366108((D_005F6C44 + D_005F6C3C) / 2 - k.unk0[0], (D_005F6C48 + D_005F6C40) / 2 - k.unk0[2])
                 - 0x400) << 6;
    ang = (-k.unk0[4] / 64) & 0xFFF;
    k.unk0[3] = (func_00366108((gSinTable[ang] * (D_005F6C3C - k.unk0[0])
                                + gSinTable[ang + 0x400] * (D_005F6C40 - k.unk0[2])) / 4096, -k.unk0[1]) << 6)
                - 0x2400;
    k.unk0[5] = 0;
    k.unk0[6] = func_0038DB98();
    func_0038E460(&k, 0);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00395EE0);
#endif

s32 func_00396320(Fighter *a, Fighter *b, s32 start) {
    Unk8B90B8 c;
    Vec4i p;
    Vec4i q;

    p.unk0[0] = (CAM_JOINT(a, 2)[0] + CAM_JOINT(b, 2)[0]) / 2;
    p.unk0[1] = CAM_JOINT(a, 11)[1] - 500;
    p.unk0[2] = (CAM_JOINT(a, 2)[2] + CAM_JOINT(b, 2)[2]) / 2;
    q.unk0[0] = CAM_JOINT(b, 2)[0];
    q.unk0[1] = CAM_JOINT(b, 2)[1];
    q.unk0[2] = CAM_JOINT(b, 2)[2];
    func_0038E308(&c, &p, &q, 0, D_00433330.unkA4);
    func_0038E460(&c, 0);
    return 0;
}

s32 func_003963C0(u32 n, Fighter *a, Fighter *b) {
    u32 k = n % 5;

    D_005F6C50 = k;
    *(volatile u16 *)&D_005F6C52 = D_00509C10[k]; /* fake: keeps the store out of the call's delay slot */
    func_00390E08(a, b);
    return 0;
}

INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_00509C30);

/* NON_MATCHING: 5 lines: the D_005F6C52 increment after `start` loads into v0 where the original uses v1 (and keeps v0 for 720) */
#if 0
s32 func_00396418(s32 a0, s32 a1) {
    func_0038EC08((Fighter *)a0, (Fighter *)a1, 0);
    switch ((s16)(D_005F6C52 - 0xA)) {
    case 0x0:
        func_00395CD8(a0, a1, -1);
        goto start;
    case 0x1:
        func_00395CD8(a0, a1, 0);
        D_005F61B8--;
        goto test;
    case 0xA:
        func_00395EE0(a0, a1, -1);
        goto start;
    case 0xB:
        func_00395EE0(a0, a1, 0);
        D_005F61B8--;
        goto test;
    case 0x14:
        func_00395D20(a0, a1, -1);
        goto start;
    case 0x15:
        func_00395D20(a0, a1, 0);
        D_005F61B8--;
        goto test;
    case 0x1E:
        func_00395EE0(a0, a1, -2);
        goto start;
    case 0x1F:
        func_00395EE0(a0, a1, 0);
        D_005F61B8--;
        goto test;
    case 0x29:
        func_00396320((Fighter *)a0, (Fighter *)a1, 0);
        D_005F61B8--;
        goto test;
    case 0x28:
    case 0x32:
        func_00396320((Fighter *)a0, (Fighter *)a1, -1);
    start:
        D_005F61B8 = 0x2D0;
        *(volatile u16 *)&D_005F6C52 += 1; /* fake */
        break;
    case 0x33:
        func_00396620(a0, a1, 0);
        D_005F61B8 -= 2;
        goto test;
    step:
        D_005F61B8--;
    test:
        if (D_005F61B8 < 0) {
            D_005F6C50++;
            *(volatile u16 *)&D_005F6C52 = D_00509C10[(u32)(s16)D_005F6C50 % 5]; /* fake */
        }
        break;
    default:
        func_003963C0(0, (Fighter *)a0, (Fighter *)a1);
        break;
    }
    Cam_SetKey(0);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00396418);
#endif

s32 func_00396620(s32 a0, s32 a1, s32 a2) {
    if (a2 < 0) {
        switch (D_00433330.unkB4) {
        case 0:
            D_005F61BC = D_00436258[rand() & 7] << 6;
            break;
        case 1:
            D_005F61BC = D_00436258[rand() & 3] << 5;
            break;
        case 2:
            D_005F61BC = D_00436258[rand() & 1] << 4;
            break;
        }
        if (func_003403B8() == 0xA) {
            D_005F61BC = -0x3C;
        }
        D_005F61C0 = (rand() & 0xFFF) << 6;
        D_005F61C8 = D_00436278[rand() & 1] << 6;
    }
    D_005F61C0 = (D_005F61C0 + D_005F61C8) & 0x3FFFF;
    func_003912C8(a0, a1, 1, D_00433330.unkA4 * 0x76C / 256, D_005F61BC, D_005F61C0, D_005F61C4);
    return 0;
}

void func_003967C8(void) {
    D_008BC710.unk0 = -1;
    D_008BC710.unk4 = -1;
}

/* NON_MATCHING: 5 instructions short (51 lines): the original saves s7 and keeps the bases of D_00433520 and of a .bss record in saved registers */
#if 0
void func_003967E0(void) {
    s32 i;
    s32 y;
    s32 k = func_0038C188();

    D_008BC710.unk0 = k;
    if (k < 0) {
        D_008BC710.unk4 = 0;
        D_008BC710.unk0 = -1;
    } else {
        D_008BC710.unk4++;
    }
    for (i = 0; i < 2; i++) {
        if (D_008BC710.unk0 < 0 || !(D_008BC710.unk4 & 3)) {
            y = CAM_JOINT(&gFighters[i], 2)[1];
            if (gFighters[i].unk520 < y) {
                y = gFighters[i].unk520;
            }
            D_008BC710.unk8[i][0] = gFighters[i].unk51C;
            D_008BC710.unk8[i][2] = gFighters[i].unk524;
            D_008BC710.unk8[i][1] = y;
            D_00433520[i][0] = D_008BC710.unk8[i][0];
            D_00433520[i][1] = y;
            D_00433520[i][2] = D_008BC710.unk8[i][2];
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003967E0);
#endif

s32 func_00396930(void) {
    return func_00366108(D_00433520[1][0] - D_00433520[0][0], D_00433520[1][2] - D_00433520[0][2]) << 6;
}

#if 0
/* NON_MATCHING: 3 lines: the unk14 store is scheduled right after its sum, the original stores it after the unk1C loads */
void func_00396970(Fighter *a, Fighter *b) {
    s32 d;

    if (D_005F61CC != 0) {
        func_003967E0();
        if (D_008BC6E8.unk0 < 2) {
        if (D_008BC6E8.unk0 >= 0) {
            func_0038FD90((s32)a, (s32)b, D_008BC6E8.unkC, D_008BC6E8.unk14 + D_008BC6E8.unk20, 0, D_008BC6E8.unk4,
                          D_008BC6E8.unk1C, 1);
            d = D_008BC6E8.unk18 - D_008BC6E8.unk14;
            D_008BC6E8.unk4 = 0;
            if (!(d & 0x20000)) {
                D_008BC6E8.unk14 += (d & 0x3FFFF) / 128;
            } else {
                D_008BC6E8.unk14 += (s32)(d | 0xFFFC0000) / 128;
            }
            D_008BC6E8.unk1C += (D_004333D4[0] * 0x76C / 256 - D_008BC6E8.unk1C) * 6 / 256;
        }
        }
        Cam_SetKey(1);
        D_005F6140 = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00396970);
#endif

void func_00396AB0(Fighter *a, Fighter *b) {
    s32 n;

    Cam_Reset();
    func_00390E08(a, b);
    func_003967C8();
    n = D_008BC6E8.unk0 + 1;
    D_00433330.unk4[1] = 0x1000;
    D_005F61CC = 1;
    D_008BC6E8.unk4 = 1;
    D_00433330.unk4[0] = 0;
    D_00433330.unk4[2] = 0;
    D_008BC6E8.unk0 = n % 2;
    if (D_008BC6E8.unk0 == 0) {
        D_008BC6E8.unkC = 0;
        D_008BC6E8.unk20 = func_00396930();
        D_008BC6E8.unk1C = D_00433330.unkA4 * 0x76C / 256 * 2;
        if (rand() & 1) {
            D_008BC6E8.unk20 += 0x20000;
        }
        D_008BC6E8.unk20 += func_003800D8(0xE38E) - 0x71C7;
        if (rand() & 1) {
            D_008BC6E8.unk14 = ((func_003800D8(0x1E) + 0x19) << 18) / 360;
            D_008BC6E8.unk18 = -(((func_003800D8(0x1E) + 0x19) << 18) / 360);
        } else {
            D_008BC6E8.unk14 = -(((func_003800D8(0x1E) + 0x19) << 18) / 360);
            D_008BC6E8.unk18 = ((func_003800D8(0x1E) + 0x19) << 18) / 360;
        }
    } else {
        D_008BC6E8.unkC = 0;
        D_008BC6E8.unk20 = func_00396930();
        D_008BC6E8.unk1C = D_00433330.unkA4 * 0x76C / 256 * 2;
        if (rand() & 1) {
            D_008BC6E8.unk20 += 0x20000;
        }
        n = func_003800D8(0x15555);
        D_008BC6E8.unk18 = 0;
        D_008BC6E8.unk14 = 0;
        D_008BC6E8.unk20 += n - 0xAAAA;
    }
    func_00396970(a, b);
}

void func_00396CF8(void) {
    D_005F61CC = 0;
}
