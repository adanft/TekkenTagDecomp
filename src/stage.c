/* 0x38BF98-0x396D00 (module camera_stage, docs/game_overview.md section 4; docs/systems/stage.md): the fight camera,
 * its scripted paths and its per-stage settings. Tekken Tag has no walls, ring-outs or floor geometry in the
 * simulation: the floor is the plane y = 0 and the fight is kept near the origin by func_00372E98 (battle_flow). The
 * simulation depends on this module through four channels (docs/systems/stage.md section 5): the camera phase
 * (func_0038D518 is called with &gRoundPhase every simulated frame and advances it), the camera angles D_00415808
 * (Fighter.unk48 and the command quadrants), the random generators it advances (newlib rand(), the CPU's
 * func_003800D8 and an LCG on D_005F5CF4) and the camera state D_00433330 read by battle_flow.
 * .rodata 0x507EE0-0x509D00 is this file's subsegment; every table and jump table in it is included in place with
 * INCLUDE_RODATA (force_not_migration in data.txt), except the jump tables of functions that are C. Its .lit4 words
 * (0x5F5174-0x5F519C) stay in the generic .lit4 chunk while their four functions (func_0038DA28, func_00392F80,
 * func_003932F0, func_00393550) are assembly. The NON_MATCHING drafts marked "m2c draft" are raw m2c output kept
 * for reading; they do not compile yet. */
#include "common.h"
#include "fighter.h"

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
    /* 0x7C */ s32 unk7C;
    /* 0x80 */ s32 unk80;
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
void func_0038DA28(s32 dist);
u8 func_003923F0(s32 n, Unk8B92D0 *out, s32 a2);
s32 func_0039C0E0(s32 a0, s32 a1);
void func_0035BB58(s32 a0);
void func_0035BD20(s32 a0, s32 a1);
void func_0038FD90(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7);
void func_00391358(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6, s32 a7);
void func_003959C8(s32 a0, s32 a1, s32 a2);
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
void func_00392F80(s32 n, s32 frame);
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

extern const Unk5095C0 D_005095C0[];
extern Unk8BC690 D_008BC690;
extern const s16 D_00509C2C[];
extern s32 D_005F61A4;
extern s32 gRoundNumber;

s32 rand(void);
u32 Math_Isqrt(u32 x);
s32 func_0038BF40(s32 kind);
s32 func_0038BFE0(void);
void func_0038DC68(Unk8B90B8 *c, s32 *out);
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
extern s32 D_005F6170[];
extern s32 D_005F6190;
extern s32 D_005F6194;

void func_0038C0F0(void);
s32 func_0038C188(void);
void func_003967E0(void);
s32 func_0038E4F8(s32 i);
void func_0038EC08(Fighter *a, Fighter *b, s32 a2);
s32 func_00393A90(u32 kind, s32 side, s32 *idx);
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
void func_0035D238(s32 id, void *dst, s32 size);
void func_00393870(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f);
s32 func_003403B8(void);
s32 func_003912C8(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6);
s32 func_00391310(s32 a0, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6);
s32 func_003954E8(void);
void func_0038E6D0(Unk8B90B8 *a, Unk8B90B8 *b, s32 t);
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
s32 func_00395CD8(s32 a0, s32 a1, u32 a2);
s32 func_00395EE0(s32 a0, s32 a1, s32 a2);
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

/* NON_MATCHING: the stores to unkC0, unkC4 and D_005F6140 are scheduled earlier, and the two loop stores take each other's base registers (10 lines) */
#if 0
void func_0038C0F0(void) {
    s32 i;

    for (i = 0; i < 5; i++) {
        D_00433330.unk4[i] = 0;
        D_00433330.unk18[i] = 0;
    }
    D_00433330.unk4[0] = 0x1000;
    D_00433330.unkC8 = 4;
    D_00433330.unk0 = 1;
    D_00433330.unkBC = -1;
    D_005F6140 = 1;
    D_00433330.unk38 = -1;
    D_00433330.unk44 = -1;
    D_00433330.unk40 = -1;
    D_00433330.unk3C = -1;
    D_00433330.unk50 = -1;
    D_00433330.unk4C = -1;
    D_00433330.unk2C = 0;
    D_00433330.unkB8 = 0;
    D_00433330.unkC0 = -1;
    D_00433330.unk80 = 0;
    D_00433330.unk7C = 0;
    D_00433330.unkC4 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038C0F0);
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

/* NON_MATCHING: m2c draft, not cleaned up (886 instructions; stack locals, struct copies and some types still raw) */
#if 0
s32 func_0038C1E8(void *arg0, void *arg1) {
    ? sp20;
    ? sp40;
    ? sp60;
    ? sp80;
    ? spA0;
    ? spC0;
    ? spE0;
    ? *var_a1;
    ? *var_s0_2;
    ? *var_s1_2;
    s16 temp_a0_4;
    s32 *temp_a0;
    s32 *temp_v0_21;
    s32 *temp_v1_2;
    s32 *temp_v1_8;
    s32 temp_a0_10;
    s32 temp_a0_2;
    s32 temp_a0_3;
    s32 temp_a0_7;
    s32 temp_a0_8;
    s32 temp_a0_9;
    s32 temp_a1;
    s32 temp_a1_2;
    s32 temp_a1_3;
    s32 temp_a1_4;
    s32 temp_a1_5;
    s32 temp_a1_6;
    s32 temp_a1_7;
    s32 temp_a1_8;
    s32 temp_a1_9;
    s32 temp_a2_4;
    s32 temp_lo;
    s32 temp_lo_2;
    s32 temp_lo_3;
    s32 temp_lo_4;
    s32 temp_lo_5;
    s32 temp_lo_6;
    s32 temp_lo_7;
    s32 temp_lo_8;
    s32 temp_lo_9;
    s32 temp_s0_2;
    s32 temp_s0_3;
    s32 temp_s0_4;
    s32 temp_s0_5;
    s32 temp_s0_7;
    s32 temp_v0;
    s32 temp_v0_10;
    s32 temp_v0_11;
    s32 temp_v0_12;
    s32 temp_v0_13;
    s32 temp_v0_14;
    s32 temp_v0_15;
    s32 temp_v0_16;
    s32 temp_v0_18;
    s32 temp_v0_19;
    s32 temp_v0_20;
    s32 temp_v0_22;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v0_4;
    s32 temp_v0_5;
    s32 temp_v0_6;
    s32 temp_v0_7;
    s32 temp_v0_8;
    s32 temp_v0_9;
    s32 temp_v1;
    s32 temp_v1_5;
    s32 var_a0;
    s32 var_a0_2;
    s32 var_a1_2;
    s32 var_a1_3;
    s32 var_a1_4;
    s32 var_a2;
    s32 var_a2_2;
    s32 var_a2_3;
    s32 var_a3_2;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v0_4;
    s32 var_v0_5;
    s32 var_v0_6;
    s32 var_v0_7;
    s32 var_v1;
    u16 temp_a2_5;
    u32 temp_v1_3;
    u32 temp_v1_7;
    void *temp_a0_5;
    void *temp_a0_6;
    void *temp_a2;
    void *temp_a2_2;
    void *temp_a2_3;
    void *temp_a3;
    void *temp_s0;
    void *temp_s0_6;
    void *temp_s0_8;
    void *temp_s1;
    void *temp_s1_2;
    void *temp_v0_17;
    void *temp_v1_4;
    void *temp_v1_6;
    void *var_a3;
    void *var_s0;
    void *var_s1;
    void *var_v1_2;
    if (D_005F6140 != 0) {
        func_0038EC08(arg0, arg1, 0);
        temp_v0 = D_00433330.unkC4 - 1;
        D_00433330.unkC4 = temp_v0;
        if (temp_v0 <= 0) {
            D_00433330.unkC4 = 0;
        }
        temp_v0_2 = D_00433330.unkC8 - 1;
        D_00433330.unkC8 = temp_v0_2;
        if (temp_v0_2 <= 0) {
            D_00433330.unkC8 = 0;
        }
        if (((D_00433330.unk44 == 0) && (arg0->unkE0 == 0)) || ((D_00433330.unk44 == 1) && (arg1->unkE0 == 0))) {
            D_00433330.unk44 = -1;
        }
        D_00433330.unkC0 = func_0038C188();
        temp_a0 = (arg0->unk20 * 4) + (&D_00433330 + 0x7C);
        temp_v1 = *temp_a0;
        if ((temp_v1 != 0) && (temp_v1 != arg0->unkC0)) {
            *temp_a0 = 0;
        }
        temp_v1_2 = (arg1->unk20 * 4) + (&D_00433330 + 0x7C);
        temp_a0_2 = *temp_v1_2;
        if ((temp_a0_2 != 0) && (temp_a0_2 != arg1->unkC0)) {
            *temp_v1_2 = 0;
        }
        if (D_00433330.unk0 != 1) {
            D_00433330.unkC4 = 0x1E;
        }
        temp_v1_3 = D_00433330.unk0 - 1;
        if (temp_v1_3 < 0xFU) {
            temp_a0_3 = *((temp_v1_3 * 4) + &jtbl_00507EE0);
            switch (D_00433330.unk0) {              /* switch 1 */
            case 1:                                 /* switch 1 */
                D_00433330.unkBC = -1;
                D_00433330.unk54 = -1;
                D_00433330.unk3C = -1;
                D_00433330.unk50 = -1;
                var_a1 = &D_00433330;
                if ((func_0038DEA0(arg0, arg1) == 0) && (D_00433330.unk44 < 0) && ((temp_a0_4 = arg0->unkE0, (temp_a0_4 > 0)) || (arg1->unkE0 > 0))) {
                    var_v0 = temp_a0_4 < 1;
                    if ((u32) (((u32) arg0->unkC0->unk24 >> 0x17) - 0x11) < 0x58U) {
                        if ((u32) (((u32) arg1->unkC0->unk24 >> 0x17) - 0x11) >= 0x58U) {
                            var_v0 = temp_a0_4 < 1;
                            goto block_27;
                        }
                    } else {
block_27:
                        var_a0 = -1;
                        D_00433330.unk44 = var_v0;
                        D_00433330.unk30 = var_v0;
                        if (var_v0 == 0) {
                            var_v0_2 = D_005F6150 < D_005F6154;
                            if (D_005F6150 == D_005F6154) {
                                var_v0_3 = D_005F615C;
                                var_v1 = D_005F6158;
                                goto block_32;
                            }
                            goto block_33;
                        }
                        var_v0_2 = D_005F6154 < D_005F6150;
                        if (D_005F6154 == D_005F6150) {
                            var_v1 = D_005F615C;
                            var_v0_3 = D_005F6158;
block_32:
                            var_v0_2 = var_v1 < var_v0_3;
                            if (var_v1 != var_v0_3) {
                                goto block_33;
                            }
                        } else {
block_33:
                            var_a0 = var_v0_2 ^ 1;
                        }
                        D_00433330.unk34 = var_a0;
                        D_00433330.unk50 = -1;
                        temp_v0_3 = func_00393E08(arg0, arg1, D_00433330.unk30, var_a0);
                        if (temp_v0_3 >= 0) {
                            if (D_00433330.unk58 < 0) {
                                D_00433330.unk3C = temp_v0_3;
                                D_00433330.unk0 = 6;
                                D_00433330.unk38 = -1;
                                D_00433330.unk40 = temp_v0_3;
                            } else {
                                func_003910D0(arg0, arg1);
                                D_00433330.unk0 = 3;
                                D_00433330.unk3C = temp_v0_3;
                                D_00433330.unk38 = 0x10;
                                D_00433330.unk40 = temp_v0_3;
                            }
                        } else {
                            D_00433330.unk54 = -1;
                        }
                    }
                    goto block_131;
                }
                goto block_132;
            case 2:                                 /* switch 1 */
                temp_s0 = &D_00433330 + 0x3330;
                temp_s0->unk8 = 0x1000;
                temp_s0->unkC = 0;
                temp_s0->unk4 = 0;
                if (func_0038DEA0(arg0, arg1) != 0) {
                    temp_s0->unk14 = 0x1000;
                    temp_s0->unk54 = -1;
                    temp_s0->unk4 = 0;
                    temp_s0->unk8 = 0;
                    temp_s0->unkC = 0;
                    goto block_131;
                }
                if (temp_s0->unk30 == 0) {
                    if (func_00393CB8(arg0, arg1) == 0) {
                        goto block_46;
                    }
                    goto block_48;
                }
block_46:
                var_a1 = &D_00433330 + 0x3330;
                if (temp_s0->unk30 == 1) {
                    var_a1 = &D_00433330 + 0x3330;
                    if (func_00393CB8(arg1, arg0) != 0) {
block_48:
                        temp_v0_4 = func_003940B0();
                        temp_v1_4 = &D_00433330 + 0x3330;
                        temp_v1_4->unk3C = temp_v0_4;
                        D_00433330.unk3330 = 4;
                        temp_v1_4->unk40 = temp_v0_4;
                        goto block_131;
                    }
                }
                goto block_132;
            case 8:                                 /* switch 1 */
                temp_s1 = &D_00433330 + 0x3330;
                temp_s1->unk8 = 0x1000;
                temp_s0_2 = temp_s1->unk4C - 1;
                temp_s1->unkC = 0;
                temp_s1->unk4 = 0;
                temp_s1->unk4C = temp_s0_2;
                var_a1 = &D_00433330 + 0x3330;
                if (func_00394628(temp_a0_3) >= temp_s0_2) {
                    temp_s0_3 = func_00394628();
                    func_003953B0();
                    temp_s1->unk3C = temp_s0_3;
                    D_00433330.unk3330 = 9;
                    temp_s1->unk40 = temp_s0_3;
                    goto block_131;
                }
                goto block_132;
            case 14:                                /* switch 1 */
                temp_a3 = &D_00433330 + 0x3330;
                temp_v0_5 = temp_a3->unk38;
                temp_v0_6 = temp_v0_5 - 1;
                if (temp_v0_5 >= 0) {
                    temp_a3->unk38 = temp_v0_6;
                    if (temp_v0_6 == 0) {
                        func_0038EAA0(3);
                        var_a1 = &D_00433330 + 0x3330;
                    } else {
                        goto block_131;
                    }
                } else {
                    temp_a1 = temp_a3->unk3C;
                    temp_lo = (s32) (temp_a1 << 0xC) / (s32) temp_a3->unk40;
                    temp_a1_2 = temp_a1 - 1;
                    temp_a3->unk3C = temp_a1_2;
                    temp_a3->unk8 = 0;
                    temp_a3->unk4C = (s32) (temp_a3->unk4C - 1);
                    temp_a3->unkC = 0;
                    temp_a3->unk4 = temp_lo;
                    temp_a3->unk14 = (s32) (0x1000 - temp_lo);
                    if (temp_a1_2 <= 0) {
                        D_00433330.unk3330 = 0xF;
                    }
                    goto block_131;
                }
                goto block_132;
            case 15:                                /* switch 1 */
                temp_s1_2 = &D_00433330 + 0x3330;
                temp_s1_2->unk14 = 0x1000;
                temp_s0_4 = temp_s1_2->unk4C - 1;
                temp_s1_2->unk8 = 0;
                temp_s1_2->unk4 = 0;
                temp_s1_2->unkC = 0;
                temp_s1_2->unk4C = temp_s0_4;
                var_a1 = &D_00433330 + 0x3330;
                if (func_00394628(temp_a0_3) >= temp_s0_4) {
                    temp_s0_5 = func_00394628();
                    D_00433330.unk3330 = 5;
                    temp_s1_2->unk3C = temp_s0_5;
                    temp_s1_2->unk40 = temp_s0_5;
                    goto block_131;
                }
                goto block_132;
            case 12:                                /* switch 1 */
                var_a3 = &D_00433330 + 0x3330;
                temp_v0_7 = var_a3->unk38;
                var_v0_4 = temp_v0_7 - 1;
                if (temp_v0_7 < 0) {
block_70:
                    temp_a1_3 = var_a3->unk3C;
                    temp_lo_2 = (s32) (temp_a1_3 << 0xC) / (s32) var_a3->unk40;
                    var_a1_2 = temp_a1_3 - 1;
                    var_a3->unk3C = var_a1_2;
                    var_a3->unk4 = 0;
                    var_a3->unk4C = (s32) (var_a3->unk4C - 1);
                    var_a3->unkC = 0;
                    var_a2 = 0x1000 - temp_lo_2;
                    var_a3->unk14 = temp_lo_2;
block_71:
                    var_a3->unk8 = var_a2;
                    if (var_a1_2 <= 0) {
                        D_00433330.unk3330 = 8;
                    }
                } else {
block_61:
                    var_a3->unk38 = var_v0_4;
                }
                goto block_131;
            case 7:                                 /* switch 1 */
                var_a3 = &D_00433330 + 0x3330;
                temp_v0_8 = var_a3->unk38;
                var_v0_4 = temp_v0_8 - 1;
                if (temp_v0_8 >= 0) {
                    goto block_61;
                }
                temp_a1_4 = var_a3->unk3C;
                temp_lo_3 = (s32) (temp_a1_4 << 0xC) / (s32) var_a3->unk40;
                var_a1_2 = temp_a1_4 - 1;
                var_a3->unk3C = var_a1_2;
                var_a3->unk14 = 0;
                var_a3->unk4C = (s32) (var_a3->unk4C - 1);
                var_a3->unkC = 0;
                var_a2 = 0x1000 - temp_lo_3;
                var_a3->unk4 = temp_lo_3;
                goto block_71;
                goto block_131;
            case 10:                                /* switch 1 */
                var_a3 = &D_00433330 + 0x3330;
                temp_v0_9 = var_a3->unk38;
                var_v0_5 = temp_v0_9 - 1;
                if (temp_v0_9 < 0) {
                    temp_a1_5 = var_a3->unk3C;
                    temp_lo_4 = (s32) (temp_a1_5 << 0xC) / (s32) var_a3->unk40;
                    var_a1_2 = temp_a1_5 - 1;
                    var_a3->unk3C = var_a1_2;
                    var_a3->unk14 = 0;
                    var_a3->unk4C = (s32) (var_a3->unk4C - 1);
                    var_a3->unkC = 0;
                    var_a2 = 0x1000 - temp_lo_4;
                    var_a3->unk4 = temp_lo_4;
                    goto block_71;
                }
block_66:
                var_a3->unk38 = var_v0_5;
                if (((u32) arg0->unkC0->unk24 >> 0x17) != 3) {
                    var_a1 = &D_00433330 + 0x3330;
                    if (((u32) arg1->unkC0->unk24 >> 0x17) == 3) {
                        goto block_69;
                    }
                } else {
block_69:
                    var_a3->unk38 = -1;
                    D_00433330.unk3330 = 1;
                    var_a3->unk54 = -1;
                    goto block_131;
                }
                goto block_132;
            case 13:                                /* switch 1 */
                var_a3 = &D_00433330 + 0x3330;
                temp_v0_10 = var_a3->unk38;
                var_v0_5 = temp_v0_10 - 1;
                if (temp_v0_10 >= 0) {
                    goto block_66;
                }
                goto block_70;
                goto block_132;
            case 3:                                 /* switch 1 */
                temp_s0_6 = &D_00433330 + 0x3330;
                temp_v0_11 = temp_s0_6->unk38;
                if (temp_v0_11 >= 0) {
                    temp_s0_6->unk38 = (s32) (temp_v0_11 - 1);
                    if (((u32) arg0->unkC0->unk24 >> 0x17) != 3) {
                        var_a1 = &D_00433330 + 0x3330;
                        if (((u32) arg1->unkC0->unk24 >> 0x17) == 3) {
                            goto block_77;
                        }
                    } else {
block_77:
                        temp_s0_6->unk38 = -1;
                        D_00433330.unk3330 = 1;
                        temp_s0_6->unk54 = -1;
                        goto block_131;
                    }
                } else {
                    temp_lo_5 = (s32) (temp_s0_6->unk3C << 0xC) / (s32) temp_s0_6->unk40;
                    temp_s0_6->unk4 = temp_lo_5;
                    temp_s0_6->unk8 = (s32) (0x1000 - temp_lo_5);
                    if (temp_s0_6->unk30 == 0) {
                        if (func_00393CB8(arg0, arg1) == 0) {
                            goto block_81;
                        }
                        goto block_83;
                    }
block_81:
                    var_v1_2 = &D_00433330 + 0x3330;
                    if ((temp_s0_6->unk30 == 1) && (var_v1_2 = &D_00433330 + 0x3330, (func_00393CB8(arg1, arg0) != 0))) {
block_83:
                        temp_v0_12 = func_003940B0();
                        var_a1 = &D_00433330 + 0x3330;
                        var_a1->unk3C = temp_v0_12;
                        var_a1->unk40 = temp_v0_12;
                        if (var_a1->unk8 != 0) {
                            if (var_a1->unk4 == 0) {
                                D_00433330.unk3330 = 4;
                            } else {
                                D_00433330.unk3330 = 5;
                                var_a1->unk14 = 0x1000;
                                var_a1->unk4 = 0;
                                var_a1->unk8 = 0;
                                func_0038EAA0(3, var_a1);
                                var_a1 = &D_00433330 + 0x3330;
                            }
                        } else {
block_129:
                            D_00433330.unk3330 = 1;
                            goto block_131;
                        }
                    } else {
                        temp_v0_13 = var_v1_2->unk3C - 1;
                        var_v1_2->unk3C = temp_v0_13;
                        if (temp_v0_13 < 0) {
                            D_00433330.unk3330 = 2;
                        }
                        goto block_131;
                    }
                }
                goto block_132;
            case 6:                                 /* switch 1 */
                temp_a0_5 = &D_00433330 + 0x3330;
                var_a2_2 = 0;
                if (temp_a0_5->unkB8 != 0) {
                    var_v0_6 = -3;
                    temp_a0_5->unkB8 = 0;
                    goto block_93;
                }
                var_s0 = &D_00433330 + 0x3330;
                if (temp_a0_5->unk38 == -1) {
                    var_v0_6 = -2;
                    var_a2_2 = 1;
block_93:
                    temp_a0_5->unk38 = var_v0_6;
                    var_s0 = &D_00433330 + 0x3330;
                }
                temp_v0_14 = var_s0->unkA4 * 0x76C;
                temp_v0_15 = func_00391170(arg0, arg1, var_a2_2, (s32) ((temp_v0_14 > -1) ? temp_v0_14 : (temp_v0_14 + 0xFF)) >> 8);
                if (temp_v0_15 >= 0) {
                    temp_v0_16 = temp_v0_15 << 5;
                    var_s0->unk4 = temp_v0_16;
                    var_s0->unkC = (s32) (0x1000 - temp_v0_16);
                    var_s0->unk8 = 0;
                    if (((u32) arg0->unkC0->unk24 >> 0x17) != 3) {
                        if (((u32) arg1->unkC0->unk24 >> 0x17) != 3) {
                            temp_v1_5 = var_s0->unk30;
                            if ((temp_v1_5 != 0) || (arg0->unkE0 >= 0)) {
                                temp_a0_6 = &D_00433330 + 0x3330;
                                if (temp_v1_5 == 1) {
                                    if (arg1->unkE0 < 0) {
                                        goto block_107;
                                    }
                                    goto block_109;
                                }
block_109:
                                if (temp_a0_6->unk38 != -2) {
                                    var_s1 = &D_00433330 + 0x3330;
                                    goto block_116;
                                }
                                if (temp_v0_15 != 0) {
                                    var_s1 = &D_00433330 + 0x3330;
block_116:
                                    if ((var_s1->unk38 == -3) && (temp_v0_15 == 0x80)) {
                                        temp_s0_7 = func_003940B0(arg0);
                                        D_00433330.unk3330 = 1;
                                        var_s1->unk38 = -1;
                                        var_s1->unk3C = temp_s0_7;
                                        var_s1->unk40 = temp_s0_7;
                                        func_003927D8();
                                    }
                                } else {
                                    temp_a0_6->unk38 = -3;
                                }
                                temp_v0_17 = &D_00433330 + 0x3330;
                                if (func_0038DEA0(arg0, arg1) != 0) {
                                    temp_v0_17->unk54 = -1;
                                    temp_v0_17->unk14 = 0x1000;
                                    temp_v0_17->unk4 = 0;
                                    temp_v0_17->unk8 = 0;
                                    temp_v0_17->unkC = 0;
                                }
                            } else {
block_107:
                                temp_s0_8 = &D_00433330 + 0x3330;
                                temp_s0_8->unk3C = 0x23;
                                temp_s0_8->unk14 = 0x1000;
                                D_00433330.unk3330 = 5;
                                temp_s0_8->unk40 = 0x23;
                                temp_s0_8->unkC = 0;
                                temp_s0_8->unk4 = 0;
                                func_0038EAA0(3, (? *)0x23);
                                temp_s0_8->unk54 = -1;
                                temp_s0_8->unk38 = -1;
                            }
                        } else {
                            goto block_99;
                        }
                    } else {
block_99:
                        func_003940B0((void *)3);
                        var_s0->unk3C = 8;
                        var_s0->unk40 = 8;
                        if (temp_v0_15 == 0x80) {
                            D_00433330.unk3330 = 1;
                        } else {
                            var_s0->unk14 = 0x1000;
                            D_00433330.unk3330 = 5;
                            var_s0->unk4 = 0;
                            var_s0->unkC = 0;
                            func_0038EAA0(3);
                        }
                        temp_v1_6 = &D_00433330 + 0x3330;
                        temp_v1_6->unk54 = -1;
                        temp_v1_6->unk38 = -1;
                    }
                    goto block_131;
                }
                break;
            case 4:                                 /* switch 1 */
                temp_a2 = &D_00433330 + 0x3330;
                temp_a0_7 = temp_a2->unk3C;
                temp_lo_6 = (s32) (temp_a0_7 << 0xC) / (s32) temp_a2->unk40;
                temp_a0_8 = temp_a0_7 - 1;
                temp_a2->unk3C = temp_a0_8;
                temp_a2->unk8 = temp_lo_6;
                temp_a2->unk4 = (s32) (0x1000 - temp_lo_6);
                if (temp_a0_8 < 0) {
                    temp_a2->unk3C = -1;
                    D_00433330.unk3330 = 1;
                    temp_a2->unk40 = -1;
                }
                goto block_131;
            case 9:                                 /* switch 1 */
                temp_a2_2 = &D_00433330 + 0x3330;
                temp_a1_6 = temp_a2_2->unk3C;
                temp_lo_7 = (s32) (temp_a1_6 << 0xC) / (s32) temp_a2_2->unk40;
                temp_a1_7 = temp_a1_6 - 1;
                temp_a2_2->unk14 = 0;
                temp_a2_2->unkC = 0;
                temp_a2_2->unk4C = (s32) (temp_a2_2->unk4C - 1);
                temp_a2_2->unk3C = temp_a1_7;
                temp_a2_2->unk8 = temp_lo_7;
                temp_a2_2->unk4 = (s32) (0x1000 - temp_lo_7);
                if (temp_a1_7 <= 0) {
                    D_00433330.unk3330 = 1;
                    temp_a2_2->unk50 = -1;
                    temp_a2_2->unk4 = 0x1000;
                    temp_a2_2->unk8 = 0;
                    temp_a2_2->unk40 = -1;
                    temp_a2_2->unk3C = -1;
                    temp_a2_2->unkC = 0;
                }
                goto block_131;
            case 11:                                /* switch 1 */
                temp_a2_3 = &D_00433330 + 0x3330;
                temp_a1_8 = temp_a2_3->unk3C;
                temp_lo_8 = (s32) (temp_a1_8 << 0xC) / (s32) temp_a2_3->unk40;
                temp_a1_9 = temp_a1_8 - 1;
                temp_a2_3->unk4 = 0;
                temp_a2_3->unkC = 0;
                temp_a2_3->unk4C = (s32) (temp_a2_3->unk4C - 1);
                temp_a2_3->unk3C = temp_a1_9;
                temp_a2_3->unk8 = temp_lo_8;
                temp_a2_3->unk14 = (s32) (0x1000 - temp_lo_8);
                if (temp_a1_9 <= 0) {
                    D_00433330.unk3330 = 5;
                    temp_a2_3->unk50 = -1;
                    temp_a2_3->unk14 = 0x1000;
                    temp_a2_3->unk4 = 0;
                    temp_a2_3->unk40 = 5;
                    temp_a2_3->unk3C = 5;
                    temp_a2_3->unkC = 0;
                    temp_a2_3->unk8 = 0;
                }
                goto block_131;
            case 5:                                 /* switch 1 */
                var_a1 = &D_00433330 + 0x3330;
                temp_a0_9 = var_a1->unk3C;
                temp_lo_9 = (s32) (temp_a0_9 << 0xC) / (s32) var_a1->unk40;
                temp_a0_10 = temp_a0_9 - 1;
                var_a1->unk3C = temp_a0_10;
                var_a1->unk14 = temp_lo_9;
                var_a1->unk4 = (s32) (0x1000 - temp_lo_9);
                if (temp_a0_10 < 0) {
                    goto block_129;
                }
                goto block_132;
            }
        } else {
            func_0038C0F0(&D_00433330);
block_131:
            var_a1 = &D_00433330;
block_132:
            if (var_a1->unk8 > 0) {
                temp_a2_4 = var_a1->unk1C < 1;
                if (var_a1->unk50 < 0) {
                    temp_v0_18 = var_a1->unkA4 * 0x76C;
                    func_00391170(arg0, arg1, temp_a2_4, (s32) ((temp_v0_18 > -1) ? temp_v0_18 : (temp_v0_18 + 0xFF)) >> 8);
                } else {
                    temp_v0_19 = var_a1->unkA4 * 0x76C;
                    func_00394648(arg0, arg1, temp_a2_4, (s32) ((temp_v0_19 > -1) ? temp_v0_19 : (temp_v0_19 + 0xFF)) >> 8);
                }
            }
            if (D_00433330.unk4 > 0) {
                temp_v0_20 = D_00433330.unkA4 * 0x76C;
                func_00390FA8(arg0, arg1, D_00433330.unk18 < 1, (s32) ((temp_v0_20 > -1) ? temp_v0_20 : (temp_v0_20 + 0xFF)) >> 8);
            }
            var_a3_2 = 1;
            var_a2_3 = 0;
            var_a1_3 = 0;
            do {
                var_a0_2 = var_a2_3 | var_a3_2;
                temp_v0_21 = (var_a1_3 * 4) + (&D_00433330 + 4);
                var_a1_3 += 1;
                var_a3_2 *= 2;
                var_a2_3 = (*temp_v0_21 >= 1) ? var_a0_2 : var_a2_3;
            } while (var_a1_3 < 5);
            temp_v1_7 = var_a2_3 - 1;
            if (temp_v1_7 < 0x12U) {
                var_a0_2 = *((temp_v1_7 * 4) + &jtbl_00507F20);
                switch (var_a2_3) {                 /* switch 2 */
                case 1:                             /* switch 2 */
                    func_0038E4F8(0, var_a1_3, var_a2_3, var_a3_2);
                    break;
                case 2:                             /* switch 2 */
                    func_0038E4F8(1, var_a1_3, var_a2_3, var_a3_2);
                    break;
                case 4:                             /* switch 2 */
                    func_0038E4F8(2, var_a1_3, var_a2_3, var_a3_2);
                    break;
                case 3:                             /* switch 2 */
                    func_0038E408(sp, 0, var_a2_3, var_a3_2);
                    func_0038E408(&sp20, 1);
                    func_0038E6D0(sp, &sp20, (&D_00433330 + 0x3330)->unk4);
                    break;
                case 5:                             /* switch 2 */
                    func_0038E408(&sp40, 0, var_a2_3, var_a3_2);
                    func_0038E408(&sp60, 2);
                    func_0038E6D0(&sp40, &sp60, (&D_00433330 + 0x3330)->unk4);
                    break;
                case 16:                            /* switch 2 */
                    func_0038E4F8(4, var_a1_3, var_a2_3, var_a3_2);
                    break;
                case 17:                            /* switch 2 */
                    var_s1_2 = &sp80;
                    func_0038E408(var_s1_2, 0, var_a2_3, var_a3_2);
                    var_s0_2 = &spA0;
                    func_0038E408(var_s0_2, 4);
                    var_v0_7 = (&D_00433330 + 0x3330)->unk4;
block_151:
                    temp_a2_5 = *(((((var_v0_7 / 2) - 0x400) & 0xFFF) * 2) + &gSinTable);
                    func_0038E6D0(var_s1_2, var_s0_2, ((s32) ((s16) temp_a2_5 + ((u32) (temp_a2_5 << 0x10) >> 0x1F)) >> 1) + 0x800);
                    break;
                case 18:                            /* switch 2 */
                    var_s1_2 = &spC0;
                    func_0038E408(var_s1_2, 1, var_a2_3, var_a3_2);
                    var_s0_2 = &spE0;
                    func_0038E408(var_s0_2, 4);
                    var_v0_7 = (&D_00433330 + 0x3330)->unk8;
                    goto block_151;
                }
            } else {
            case 6:                                 /* switch 2 */
            case 7:                                 /* switch 2 */
            case 8:                                 /* switch 2 */
            case 9:                                 /* switch 2 */
            case 10:                                /* switch 2 */
            case 11:                                /* switch 2 */
            case 12:                                /* switch 2 */
            case 13:                                /* switch 2 */
            case 14:                                /* switch 2 */
            case 15:                                /* switch 2 */
                func_0038C0F0((? *) var_a0_2, var_a1_3, var_a2_3, var_a3_2);
            }
            var_a1_4 = 0;
            do {
                temp_v0_22 = var_a1_4 * 4;
                temp_v1_8 = temp_v0_22 + (&D_00433348 - 0x14);
                var_a1_4 += 1;
                *(temp_v0_22 + &D_00433348) = *temp_v1_8;
            } while (var_a1_4 < 5);
        }
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
        func_0038E4F8(0);
        break;
    case 4:
        func_0038E4F8(2);
        break;
    case 5:
        func_0038E408(&c0, 0);
        func_0038E408(&c2, 2);
        func_0038E6D0(&c0, &c2, D_00433330.unk4[0]);
        break;
    default:
        func_0038C0F0();
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

/* NON_MATCHING: the order of the stores around the calls differs */
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
        D_00433330.unkA0 = 0;
        D_00433330.unk94 = f->unk524;
        D_00433330.unk84 = 0;
        func_00392B38(-D_00433330.unk58);
        D_00433330.unk38 = -1;
        D_00433330.unk0 = 6;
        D_00433330.unk40 = r;
        D_00433330.unk3C = r;
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
    D_00433330.unk4[1] = 0;
    D_00433330.unk4[0] = 0;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038D290);
#endif

/* NON_MATCHING: the order of the stores around the calls differs */
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
        D_00433330.unk84 = 0;
        D_00433330.unk94 = f->unk524;
        func_00392B38(-D_00433330.unk58);
        D_00433330.unk38 = -1;
        D_00433330.unk0 = 6;
        D_00433330.unk40 = r;
        D_00433330.unk3C = r;
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
    D_00433330.unk4[0] = 0;
    D_00433330.unk4[2] = 0;
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
s32 func_0038D518(Fighter *a, Fighter *b, s32 *phase) {
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
        func_0038C0F0();
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
        func_0038C0F0();
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
        func_0038C0F0();
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
        func_0038E4F8(0);
        n = 6;
        goto next;
    case 7:
    start_b:
        func_00396CF8();
        func_0038C0F0();
        func_003963C0(D_005F6138 & 3, a, b);
        func_00390E08(a, b);
        func_003927D8();
        func_00396CF8();
        D_005F6134 = 1;
        D_005F6138++;
        func_00395EE0((s32)a, (s32)b, -1);
        D_005F6144 = 0;
    case 8:
        func_0038EC08(a, b, 0);
        func_00395EE0((s32)a, (s32)b, 0);
        func_0038E4F8(0);
        n = 8;
        goto next;
    case 9:
    start_c:
        func_00396CF8();
        func_0038C0F0();
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
        func_0038E4F8(0);
        n = 0xA;
        goto next;
    case 11:
    start_d:
        func_00396CF8();
        func_0038C0F0();
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
        func_0038E4F8(0);
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

/* NON_MATCHING: m2c draft, not cleaned up (92 instructions; stack locals, struct copies and some types still raw) (float code: its .lit4 constants would need a .lit4 subsegment for this file) */
#if 0
s32 func_0038DA28(s32 arg0) {
    f32 temp_f25;
    s32 var_v1;
    var_v1 = D_00433330.unkA8;
    if (arg0 >= D_00433330.unkA8) {
        var_v1 = (D_00433330.unkAC >= arg0) ? arg0 : D_00433330.unkAC;
    }
    temp_f25 = (f32) var_v1;
    D_005F5CD0 = var_v1;
    func_00398540(var_v1);
    sceVu0ViewScreenMatrix(temp_f25, 0x3F800000, &D_00433440);
    sceVu0ViewScreenMatrix(temp_f25, 0x3F800000, &D_00433480);
    D_00433440.unk0 = (f32) (D_00433440.unk0 * 1.25f);
    D_00433440.unk14 = (f32) (D_00433440.unk14 * 1.25f);
    D_00433480.unk0 = (f32) (D_00433480.unk0 * 1.25f);
    D_00433480.unk14 = (f32) (D_00433480.unk14 * 1.25f);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038DA28);
#endif

INCLUDE_RODATA("asm/nonmatchings/stage", D_00507FE0);

s32 func_0038DB98(void) {
    u32 n = D_005F5CB6;

    if (n >= 0x19) {
        n = 0;
    }
    return D_00507FE0[n].unk0[0];
}

/* Loads the stage's camera values (D_00507FE0, 5 per stage, stage 0 above 0x18): default, minimum and maximum distance,
 * script scaling flag and camera style, then sets the projection with the default (func_0038DA28) [V code; I meanings]. */
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
    func_0038DA28(D_00507FE0[n].unk0[0]);
    return 0;
}

void func_0038DC40(void) {
    D_00433330.unkAC = 0x5DC;
    D_00433330.unkA8 = 0;
}

void func_0038DC58(s32 dist) {
    D_00433330.unkA4 = dist;
    func_0038DA28(dist);
}

/* NON_MATCHING: m2c draft, not cleaned up (136 instructions; stack locals, struct copies and some types still raw) */
#if 0
u32 func_0038DC68(void *arg0, void *arg1) {
    void *sp10;
    s32 sp18;
    s16 *temp_a2;
    s16 *temp_a3;
    s16 temp_s2;
    s16 temp_s3;
    s16 temp_s6;
    s16 temp_s7;
    s32 temp_a0;
    s32 temp_a0_2;
    s32 temp_a2_2;
    s32 temp_s0_2;
    s32 temp_s0_3;
    s32 temp_s1;
    s32 temp_s1_2;
    s32 temp_s2_2;
    s32 temp_s3_2;
    s32 temp_t0;
    s32 temp_t1;
    s32 temp_t2;
    s32 temp_t3;
    s32 temp_v0;
    s32 temp_v1;
    u32 temp_s0;
    void *temp_t6;
    M2C_ERROR(/* unknown instruction: ldl $v0, 0x7($a0) */);
    M2C_ERROR(/* unknown instruction: ldr $v0, ($a0) */);
    M2C_ERROR(/* unknown instruction: ldl $v1, 0xf($a0) */);
    M2C_ERROR(/* unknown instruction: ldr $v1, 0x8($a0) */);
    M2C_ERROR(/* unknown instruction: ldl $a2, 0x17($a0) */);
    M2C_ERROR(/* unknown instruction: ldr $a2, 0x10($a0) */);
    M2C_ERROR(/* unknown instruction: sdl $v0, 0x7($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v0, ($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $v1, 0xf($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v1, 0x8($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a2, 0x17($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a2, 0x10($sp) */);
    sp18 = arg0->unk18;
    temp_t6 = sp10 + &D_1003F;
    temp_a0 = D_00433520.unk0 - subroutine_arg0;
    temp_s1 = D_00433520.unk10 - subroutine_arg0;
    temp_t1 = D_00433520.unk14 - subroutine_arg1;
    temp_t3 = D_00433520.unk4 - subroutine_arg1;
    temp_t2 = D_00433520.unk18 - subroutine_arg2;
    temp_t0 = D_00433520.unk8 - subroutine_arg2;
    temp_v1 = ((s32) ((subroutine_arg3 > -1) ? subroutine_arg3 : (subroutine_arg3 + 0x3F)) >> 6) & 0xFFF;
    temp_v0 = ((s32) (((s32) temp_t6 > -1) ? temp_t6 : temp_t6) >> 6) & 0xFFF;
    temp_a3 = ((temp_v0 + 0x400) * 2) + &gSinTable;
    temp_a2 = ((temp_v1 + 0x400) * 2) + &gSinTable;
    temp_s7 = *temp_a2;
    temp_s1_2 = (temp_s1 * temp_s1) + (temp_t1 * temp_t1) + (temp_t2 * temp_t2);
    temp_s6 = *((temp_v1 * 2) + &gSinTable);
    temp_s3 = *((temp_v0 * 2) + &gSinTable);
    temp_s2 = *temp_a3;
    sp10 = temp_t6;
    temp_s0_2 = Math_Isqrt((temp_a0 * temp_a0) + (temp_t3 * temp_t3) + (temp_t0 * temp_t0), temp_a2, temp_a3);
    temp_s0 = (u32) (temp_s0_2 + Math_Isqrt(temp_s1_2)) >> 1;
    temp_s3_2 = temp_s0 * temp_s3;
    temp_s2_2 = temp_s0 * temp_s2;
    temp_s0_3 = temp_s0 * temp_s6;
    temp_a0_2 = ((s32) ((temp_s3_2 > -1) ? temp_s3_2 : (temp_s3_2 + 0xFFF)) >> 0xC) * temp_s7;
    temp_a2_2 = ((s32) ((temp_s2_2 > -1) ? temp_s2_2 : (temp_s2_2 + 0xFFF)) >> 0xC) * temp_s7;
    arg1->unk0 = (s32) (subroutine_arg0 + ((s32) ((temp_a2_2 > -1) ? temp_a2_2 : (temp_a2_2 + 0xFFF)) >> 0xC));
    arg1->unk8 = (s32) (subroutine_arg2 + ((s32) ((temp_a0_2 > -1) ? temp_a0_2 : (temp_a0_2 + 0xFFF)) >> 0xC));
    arg1->unk4 = (s32) (subroutine_arg1 + ((s32) ((temp_s0_3 > -1) ? temp_s0_3 : (temp_s0_3 + 0xFFF)) >> 0xC));
    return temp_s0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038DC68);
#endif

s32 func_0038DE88(void) {
    return D_00433330.unk0 == 1;
}

INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_005080E0);

/* NON_MATCHING: m2c draft, not cleaned up (268 instructions; stack locals, struct copies and some types still raw) */
#if 0
s32 func_0038DEA0(void *arg0, void *arg1) {
    ? *temp_a1;
    ? *var_v1_2;
    s16 var_a0_2;
    s16 var_a0_3;
    s16 var_a0_4;
    s32 temp_a0;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v0_4;
    s32 temp_v0_5;
    s32 temp_v0_6;
    s32 temp_v0_7;
    s32 temp_v0_8;
    s32 temp_v0_9;
    s32 var_a0;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v0_4;
    s32 var_v0_5;
    s32 var_v0_6;
    s32 var_v1;
    u32 temp_v1;
    void *temp_a0_2;
    void *temp_a0_3;
    void *temp_v1_2;
    void *temp_v1_3;
    void *temp_v1_4;
    void *var_a1;
    void *var_s0;
    void *var_v1_3;
    void *var_v1_4;
    void *var_v1_5;
    if (D_00433330.unk7C == 0) {
        var_v0 = 0;
        if (D_00433330.unk80 == 0) {
            temp_v0 = func_003943A8(arg0, arg1);
            if (temp_v0 >= 0) {
                D_00433330.unk30 = temp_v0;
                var_a0 = -1;
                if (temp_v0 == 0) {
                    var_v0_2 = D_005F6150 < D_005F6154;
                    if (D_005F6150 == D_005F6154) {
                        var_v0_3 = D_005F615C;
                        var_v1 = D_005F6158;
                        goto block_11;
                    }
                    goto block_12;
                }
                var_v0_2 = D_005F6154 < D_005F6150;
                if (D_005F6154 == D_005F6150) {
                    var_v1 = D_005F615C;
                    var_v0_3 = D_005F6158;
block_11:
                    var_v0_2 = var_v1 < var_v0_3;
                    if (var_v1 != var_v0_3) {
                        goto block_12;
                    }
                } else {
block_12:
                    var_a0 = var_v0_2 ^ 1;
                }
                D_00433330.unk34 = var_a0;
                temp_v0_2 = func_00394610(arg0, arg1, temp_v0, var_a0);
                var_v1_2 = &D_00433330;
                if (temp_v0_2 >= 0) {
                    var_v0 = 1;
                    if (D_00433330.unk58 >= 0) {
                        temp_v0_3 = func_003953B0();
                        temp_v1 = temp_v0_3 - 3;
                        if (temp_v1 < 0xFU) {
                            temp_a0 = *((temp_v1 * 4) + &jtbl_005080E0);
                            switch (temp_v0_3) {
                            case 3:
                            case 6:
                                var_s0 = &D_00433330 + 0x3330;
                                var_s0->unk38 = 0x10;
                                D_00433330.unk3330 = 0xA;
                                var_s0->unk40 = temp_v0_2;
                                var_s0->unk3C = temp_v0_2;
                                temp_v0_4 = func_00394638(temp_a0);
                                var_s0->unk4C = temp_v0_4;
                                var_s0->unk50 = temp_v0_4;
                                if (var_s0->unk30 != 0) {
                                    var_a0_2 = arg1->unk3C;
                                } else {
block_19:
                                    var_a0_2 = arg0->unk3C;
                                }
block_20:
                                var_s0->unk74 = (s32) ((s16) ((var_a0_2 > -1) ? var_a0_2 : (var_a0_2 + 0xF)) >> 4);
block_51:
                                func_0038EAA0(3);
                                var_v1_2 = &D_00433330 + 0x3330;
                                break;
                            case 11:
                            case 14:
                                var_s0 = &D_00433330 + 0x3330;
                                var_s0->unk38 = 0x10;
                                D_00433330.unk3330 = 0xD;
                                var_s0->unk40 = temp_v0_2;
                                var_s0->unk3C = temp_v0_2;
                                temp_v0_5 = func_00394638(temp_a0);
                                var_s0->unk4C = temp_v0_5;
                                var_s0->unk50 = temp_v0_5;
                                if (var_s0->unk30 != 0) {
                                    var_a0_2 = arg1->unk3C;
                                } else {
                                    goto block_19;
                                }
                                goto block_20;
                            case 10:
                                var_v1_3 = &D_00433330 + 0x3330;
                                if (func_003953F0(temp_a0) != 0) {
                                    var_v0_4 = func_003953F0();
                                    var_v1_3 = &D_00433330 + 0x3330;
                                } else {
                                    var_v0_4 = -1;
                                }
                                var_v1_3->unk38 = var_v0_4;
                                if ((temp_v0_2 == 1) && (temp_a1 = &D_00433330 + 0x3330, (temp_a1->unk38 == -1))) {
                                    temp_a1->unk3C = 0;
                                    D_00433330.unk3330 = 0xF;
                                    temp_a1->unk40 = 0;
                                    func_0038EAA0(3, temp_a1);
                                } else {
                                    temp_v1_2 = &D_00433330 + 0x3330;
                                    D_00433330.unk3330 = 0xE;
                                    temp_v1_2->unk3C = temp_v0_2;
                                    temp_v1_2->unk40 = temp_v0_2;
                                }
                                temp_v0_6 = func_00394638();
                                var_v1_2 = &D_00433330 + 0x3330;
                                var_v1_2->unk4C = temp_v0_6;
                                var_v1_2->unk50 = temp_v0_6;
                                break;
                            case 12:
                            case 13:
                            case 15:
                            case 16:
                            case 17:
                                var_v1_4 = &D_00433330 + 0x3330;
                                if (func_003953F0(temp_a0) != 0) {
                                    var_v0_5 = func_003953F0();
                                    var_v1_4 = &D_00433330 + 0x3330;
                                } else {
                                    var_v0_5 = -1;
                                }
                                var_v1_4->unk38 = var_v0_5;
                                if ((temp_v0_2 == 1) && (temp_a0_2 = &D_00433330 + 0x3330, (temp_a0_2->unk38 == -1))) {
                                    temp_a0_2->unk3C = 0;
                                    D_00433330.unk3330 = 8;
                                    temp_a0_2->unk40 = 0;
                                } else {
                                    temp_v1_3 = &D_00433330 + 0x3330;
                                    D_00433330.unk3330 = 0xC;
                                    temp_v1_3->unk3C = temp_v0_2;
                                    temp_v1_3->unk40 = temp_v0_2;
                                }
                                temp_v0_7 = func_00394638();
                                var_a1 = &D_00433330 + 0x3330;
                                var_a1->unk4C = temp_v0_7;
                                var_a1->unk50 = temp_v0_7;
                                if (var_a1->unk30 != 0) {
                                    var_a0_3 = arg1->unk3C;
                                } else {
block_49:
                                    var_a0_3 = arg0->unk3C;
                                }
block_50:
                                var_a1->unk74 = (s32) ((s16) ((var_a0_3 > -1) ? var_a0_3 : (var_a0_3 + 0xF)) >> 4);
                                goto block_51;
                            case 4:
                            case 5:
                            case 7:
                            case 8:
                            case 9:
                                var_v1_5 = &D_00433330 + 0x3330;
                                if (func_003953F0(temp_a0) != 0) {
                                    var_v0_6 = func_003953F0();
                                    var_v1_5 = &D_00433330 + 0x3330;
                                } else {
                                    var_v0_6 = -1;
                                }
                                var_v1_5->unk38 = var_v0_6;
                                if ((temp_v0_2 == 1) && (temp_a0_3 = &D_00433330 + 0x3330, (temp_a0_3->unk38 == -1))) {
                                    temp_a0_3->unk3C = 0;
                                    D_00433330.unk3330 = 8;
                                    temp_a0_3->unk40 = 0;
                                } else {
                                    temp_v1_4 = &D_00433330 + 0x3330;
                                    D_00433330.unk3330 = 7;
                                    temp_v1_4->unk3C = temp_v0_2;
                                    temp_v1_4->unk40 = temp_v0_2;
                                }
                                temp_v0_8 = func_00394638();
                                var_a1 = &D_00433330 + 0x3330;
                                var_a1->unk4C = temp_v0_8;
                                var_a1->unk50 = temp_v0_8;
                                if (var_a1->unk30 != 0) {
                                    var_a0_3 = arg1->unk3C;
                                } else {
                                    goto block_49;
                                }
                                goto block_50;
                            }
                        } else {
                            D_00433330.unk38 = -1;
                            if (temp_v0_2 == 1) {
                                D_00433330.unk3C = 0;
                                D_00433330.unk0 = 8;
                                D_00433330.unk40 = 0;
                            } else {
                                D_00433330.unk3C = temp_v0_2;
                                D_00433330.unk0 = 7;
                                D_00433330.unk40 = temp_v0_2;
                            }
                            temp_v0_9 = func_00394638();
                            D_00433330.unk4C = temp_v0_9;
                            D_00433330.unk50 = temp_v0_9;
                            if (D_00433330.unk30 != 0) {
                                var_a0_4 = arg1->unk3C;
                            } else {
                                var_a0_4 = arg0->unk3C;
                            }
                            D_00433330.unk74 = (s32) ((s16) ((var_a0_4 > -1) ? var_a0_4 : (var_a0_4 + 0xF)) >> 4);
                            func_0038EAA0(3, &D_00433330);
                            var_v1_2 = &D_00433330;
                        }
                        goto block_59;
                    }
                } else {
block_59:
                    var_v0 = 1;
                }
                var_v1_2->unk3C = temp_v0_2;
                var_v1_2->unk40 = temp_v0_2;
                /* Duplicate return node #61. Try simplifying control flow for better match */
                return var_v0;
            }
            goto block_5;
        }
        return var_v0;
    }
block_5:
    return 0;
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

/* NON_MATCHING: the two positions are copied whole, then read back and subtracted in another order (one instruction short) */
#if 0
s32 func_0038E308(Unk8B90B8 *out, Vec4i *from, Vec4i *to, s32 roll, s32 dist) {
    Vec4i a = *from;
    Vec4i b = *to;
    s32 d[3];

    out->unk0[0] = a.unk0[0];
    out->unk0[1] = a.unk0[1];
    out->unk0[2] = a.unk0[2];
    d[0] = b.unk0[0] - a.unk0[0];
    d[1] = b.unk0[1] - a.unk0[1];
    d[2] = b.unk0[2] - a.unk0[2];
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

/* NON_MATCHING */
#if 0
/* Makes camera key i (D_008B90B8[i]) current: copies it to key 3 and writes the camera angles D_00415808 (pitch and yaw
 * in 1/4096 turns, roll 0, position) that the simulation reads (Battle_UpdateFacing uses D_00415808[1]) [V code]. */
s32 func_0038E4F8(s32 i) {
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
    func_0038DA28(D_008B90B8[i].unk0[6]);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038E4F8);
#endif

/* NON_MATCHING: the loads and stores of the copied fields are paired differently (11 lines) */
#if 0
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
        out[7] = 0;
        out[6] = D_008B90B8[i].unk0[6];
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038E610);
#endif

/* NON_MATCHING: m2c draft, not cleaned up (244 instructions; stack locals, struct copies and some types still raw) */
#if 0
s32 func_0038E6D0(void *arg0, void *arg1, s32 arg2) {
    ? sp20;
    s32 sp38;
    s32 sp40;
    s32 sp44;
    s32 sp48;
    s32 sp50;
    s32 sp54;
    s32 sp58;
    s32 sp60;
    s32 sp70;
    s32 sp80;
    s32 sp84;
    s32 sp88;
    ? sp90;
    s16 temp_v0_6;
    s32 temp_a0;
    s32 temp_a0_2;
    s32 temp_a1;
    s32 temp_a1_2;
    s32 temp_a1_3;
    s32 temp_a1_4;
    s32 temp_a2;
    s32 temp_a3;
    s32 temp_a3_2;
    s32 temp_a3_3;
    s32 temp_s0;
    s32 temp_s0_2;
    s32 temp_s0_3;
    s32 temp_s0_4;
    s32 temp_s2;
    s32 temp_t0;
    s32 temp_t1;
    s32 temp_t4;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v0_4;
    s32 temp_v0_5;
    s32 temp_v0_7;
    s32 temp_v1;
    s32 temp_v1_2;
    s32 temp_v1_3;
    s32 temp_v1_4;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;
    M2C_ERROR(/* unknown instruction: ldl $v0, 0x7($a0) */);
    M2C_ERROR(/* unknown instruction: ldr $v0, ($a0) */);
    M2C_ERROR(/* unknown instruction: ldl $v1, 0xf($a0) */);
    M2C_ERROR(/* unknown instruction: ldr $v1, 0x8($a0) */);
    M2C_ERROR(/* unknown instruction: ldl $a3, 0x17($a0) */);
    M2C_ERROR(/* unknown instruction: ldr $a3, 0x10($a0) */);
    M2C_ERROR(/* unknown instruction: sdl $v0, 0x7($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v0, ($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $v1, 0xf($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v1, 0x8($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a3, 0x17($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a3, 0x10($sp) */);
    M2C_ERROR(/* unknown instruction: ldl $v1, 0x7($a1) */);
    M2C_ERROR(/* unknown instruction: ldr $v1, ($a1) */);
    M2C_ERROR(/* unknown instruction: ldl $a0, 0xf($a1) */);
    M2C_ERROR(/* unknown instruction: ldr $a0, 0x8($a1) */);
    M2C_ERROR(/* unknown instruction: ldl $a2, 0x17($a1) */);
    M2C_ERROR(/* unknown instruction: ldr $a2, 0x10($a1) */);
    temp_a3 = arg1->unk18;
    M2C_ERROR(/* unknown instruction: sdl $v1, 0x27($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v1, 0x20($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a0, 0x2f($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a0, 0x28($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a2, 0x37($sp) */);
    sp38 = temp_a3;
    temp_s2 = 0x1000 - arg2;
    temp_s0 = func_0038DC68(sp, &sp60, M2C_ERROR(/* unknown instruction: sdr $a2, 0x30($sp) */), temp_a3, arg0->unk18);
    temp_s0_2 = (temp_s0 * arg2) + (func_0038DC68(&sp20, &sp70) * temp_s2);
    temp_a0 = (sp60 * arg2) + (sp70 * temp_s2);
    temp_a1 = (sp64 * arg2) + (sp74 * temp_s2);
    temp_v1 = (sp68 * arg2) + (sp78 * temp_s2);
    temp_v0 = temp_s0_2 > -1;
    temp_v1_2 = temp_s0_2 + 0xFFF;
    sp80 = (s32) ((temp_a0 > -1) ? temp_a0 : (temp_a0 + 0xFFF)) >> 0xC;
    sp84 = (s32) ((temp_a1 > -1) ? temp_a1 : (temp_a1 + 0xFFF)) >> 0xC;
    sp88 = (s32) ((temp_v1 > -1) ? temp_v1 : (temp_v1 + 0xFFF)) >> 0xC;
    temp_t4 = (s32) ((temp_v0 != 0) ? temp_s0_2 : temp_v1_2) >> 0xC;
    temp_v1_3 = sp20.unkC - subroutine_arg3;
    if (temp_v1_3 & 0x20000) {
        var_v0 = temp_v1_3 | 0xFFFC0000;
    } else {
        var_v0 = temp_v1_3 & 0x3FFFF;
    }
    temp_v0_2 = var_v0 * temp_s2;
    sp40 = subroutine_arg3 + ((s32) ((temp_v0_2 > -1) ? temp_v0_2 : (temp_v0_2 + 0xFFF)) >> 0xC);
    temp_a2 = sp->unk10;
    temp_a1_2 = sp20.unk10 - temp_a2;
    if (temp_a1_2 & 0x20000) {
        var_v0_2 = temp_a1_2 | 0xFFFC0000;
    } else {
        var_v0_2 = temp_a1_2 & 0x3FFFF;
    }
    temp_v0_3 = var_v0_2 * temp_s2;
    sp44 = temp_a2 + ((s32) ((temp_v0_3 > -1) ? temp_v0_3 : (temp_v0_3 + 0xFFF)) >> 0xC);
    temp_a3_2 = sp->unk14;
    temp_a1_3 = sp20.unk14 - temp_a3_2;
    if (temp_a1_3 & 0x20000) {
        var_v0_3 = temp_a1_3 | 0xFFFC0000;
    } else {
        var_v0_3 = temp_a1_3 & 0x3FFFF;
    }
    temp_v0_4 = var_v0_3 * temp_s2;
    temp_s0_3 = temp_a3_2 + ((s32) ((temp_v0_4 > -1) ? temp_v0_4 : (temp_v0_4 + 0xFFF)) >> 0xC);
    temp_v0_5 = (((s32) ((sp44 > -1) ? sp44 : (sp44 + 0x3F)) >> 6) + 0x400) & 0xFFF;
    temp_a0_2 = ((s32) ((sp40 > -1) ? sp40 : (sp40 + 0x3F)) >> 6) & 0xFFF;
    temp_t0 = temp_t4 * *((temp_v0_5 * 2) + &gSinTable);
    temp_a3_3 = temp_t4 * *(((temp_v0_5 + 0x400) * 2) + &gSinTable);
    temp_v0_6 = *(((temp_a0_2 + 0x400) * 2) + &gSinTable);
    temp_a1_4 = ((s32) ((temp_t0 > -1) ? temp_t0 : (temp_t0 + 0xFFF)) >> 0xC) * temp_v0_6;
    temp_v1_4 = ((s32) ((temp_a3_3 > -1) ? temp_a3_3 : (temp_a3_3 + 0xFFF)) >> 0xC) * temp_v0_6;
    temp_t1 = temp_t4 * *((temp_a0_2 * 2) + &gSinTable);
    sp48 = temp_s0_3;
    sp50 = sp80 - ((s32) ((temp_v1_4 > -1) ? temp_v1_4 : (temp_v1_4 + 0xFFF)) >> 0xC);
    sp54 = sp84 - ((s32) ((temp_t1 > -1) ? temp_t1 : (temp_t1 + 0xFFF)) >> 0xC);
    sp58 = sp88 - ((s32) ((temp_a1_4 > -1) ? temp_a1_4 : (temp_a1_4 + 0xFFF)) >> 0xC);
    temp_v0_7 = (sp->unk18 * arg2) + (sp20.unk18 * temp_s2);
    temp_s0_4 = (s32) ((temp_v0_7 > -1) ? temp_v0_7 : (temp_v0_7 + 0xFFF)) >> 0xC;
    func_0038E308(&sp90, &sp50, &sp80, temp_s0_3);
    func_0038E460(&sp90, 3);
    func_0038E4F8(3);
    func_0038DA28(temp_s0_4);
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

/* NON_MATCHING: m2c draft, not cleaned up (682 instructions; stack locals, struct copies and some types still raw) */
#if 0
void *func_0038EC08(void *arg0, void *arg1, s32 arg2) {
    s32 sp10;
    u32 sp20;
    u32 sp24;
    s32 sp30;
    s32 sp34;
    s32 sp40;
    s32 sp44;
    s32 sp50;
    s32 sp54;
    s32 sp60;
    s32 sp64;
    void *sp70;
    s32 sp74;
    s32 sp78;
    s32 *sp7C;
    u32 *sp80;
    s32 *sp84;
    s16 temp_a1_2;
    s16 temp_v1;
    s16 temp_v1_2;
    s16 var_a0;
    s16 var_a2;
    s32 **var_v0_8;
    s32 *temp_a0_6;
    s32 *temp_a1_6;
    s32 *temp_a2;
    s32 *temp_a2_5;
    s32 *temp_a3;
    s32 *temp_s3;
    s32 *temp_t0;
    s32 *temp_t0_2;
    s32 *temp_t0_3;
    s32 *temp_t1;
    s32 *temp_t2;
    s32 *temp_t2_2;
    s32 *temp_t4;
    s32 *temp_t8;
    s32 *var_a2_2;
    s32 *var_v0_3;
    s32 *var_v0_4;
    s32 *var_v0_5;
    s32 *var_v0_6;
    s32 *var_v0_7;
    s32 temp_a0_10;
    s32 temp_a0_11;
    s32 temp_a0_3;
    s32 temp_a0_5;
    s32 temp_a0_7;
    s32 temp_a0_8;
    s32 temp_a0_9;
    s32 temp_a1_10;
    s32 temp_a1_3;
    s32 temp_a1_4;
    s32 temp_a1_5;
    s32 temp_a1_8;
    s32 temp_a1_9;
    s32 temp_a2_2;
    s32 temp_a2_3;
    s32 temp_a2_4;
    s32 temp_a2_6;
    s32 temp_a2_7;
    s32 temp_a3_2;
    s32 temp_a3_3;
    s32 temp_a3_4;
    s32 temp_lo;
    s32 temp_t0_4;
    s32 temp_t1_2;
    s32 temp_t3;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v0_5;
    s32 temp_v0_6;
    s32 temp_v0_7;
    s32 temp_v0_8;
    s32 temp_v1_3;
    s32 var_a1;
    s32 var_a3;
    s32 var_s0;
    s32 var_s0_2;
    s32 var_s0_3;
    s32 var_s0_4;
    s32 var_s0_5;
    s32 var_t5;
    s32 var_t6;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v1;
    s32 var_v1_2;
    void *temp_a0;
    void *temp_a0_2;
    void *temp_a0_4;
    void *temp_a1;
    void *temp_a1_7;
    void *temp_fp;
    void *temp_s5;
    void *temp_v0_4;
    sp70 = arg0;
    temp_a1 = arg0->unkC0;
    sp74 = arg2;
    temp_a0 = arg1->unkC0;
    sp20 = (u32) temp_a1->unk24 >> 0x17;
    sp24 = (u32) temp_a0->unk24 >> 0x17;
    M2C_ERROR(/* unknown instruction: ldl $v1, 0x27($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $v1, 0x20($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $v1, 0x17($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v1, 0x10($sp) */);
    temp_v0 = (u32) (sp10 - 0x17) < 4U;
    sp30 = temp_v0;
    sp34 = (u32) (sp14 - 0x17) < 4U;
    M2C_ERROR(/* unknown instruction: ldl $t3, 0x37($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $t3, 0x30($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $t3, 0x27($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $t3, 0x20($sp) */);
    sp40 = ((u32) temp_a1->unk24 >> 0x14) & 3;
    sp44 = ((u32) temp_a0->unk24 >> 0x14) & 3;
    M2C_ERROR(/* unknown instruction: ldl $t1, 0x47($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $t1, 0x40($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $t1, 0x37($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $t1, 0x30($sp) */);
    var_a3 = 0;
    if (temp_v0 != 2) {
        var_a3 = sp10 != 0x19;
    }
    var_v1 = 0;
    sp50 = var_a3;
    if (sp34 != 2) {
        var_v1 = sp14 != 0x19;
    }
    sp54 = var_v1;
    temp_a1_2 = sp70->unkE0;
    M2C_ERROR(/* unknown instruction: ldl $a0, 0x57($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $a0, 0x50($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a0, 0x47($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a0, 0x40($sp) */);
    sp64 = (s32) arg1->unkE0;
    sp60 = (s32) temp_a1_2;
    M2C_ERROR(/* unknown instruction: ldl $t3, 0x67($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $t3, 0x60($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $t3, 0x57($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $t3, 0x50($sp) */);
    var_a2 = 0;
    if (((sp70->unkD4 == 8) || (temp_a1_2 != 0)) && (sp20 == 0)) {
        if (sp24 == 0) {
            var_a2 = 1;
        }
    }
    sp70->unkE0 = var_a2;
    var_a0 = 0;
    if (((arg1->unkD4 == 8) || (arg1->unkE0 != 0)) && (sp20 == 0) && (sp24 == 0)) {
        var_a0 = 1;
    }
    arg1->unkE0 = var_a0;
    if (func_0038EAF0(sp70, arg1, var_a2, var_a3) != 0) {
        sp30 = 1;
        sp34 = 1;
    }
    sp84 = &sp40;
    sp7C = &sp10;
    var_s0 = 0;
    sp80 = &sp20;
    var_v0_2 = 1 - 0;
    do {
        temp_a1_3 = var_s0 * 4;
        if ((&sp10)[var_v0_2] != 0x19) {
            temp_a2 = &sp84[var_s0];
            if (*temp_a2 == 0) {
                var_v0_3 = temp_a1_3 + &D_005F6168;
                if (sp7C[var_s0] == 0x19) {
                    temp_a0_2 = *(sp + temp_a1_3);
                    temp_v1 = temp_a0_2->unk628;
                    if (temp_v1 >= 0x101) {
                        var_v0_3 = temp_a1_3 + &D_005F6168;
                        if (temp_v1 < 0x1EC) {
                            var_v0_3 = temp_a1_3 + &D_005F6168;
                            if (temp_a0_2->unk62A < 0x1EC) {
                                var_v0_3 = temp_a1_3 + &D_005F6168;
                                if (temp_a0_2->unk62C < 0x1EC) {
                                    *temp_a2 = 1;
                                }
                            }
                        }
                    } else {
                        var_v0_3 = temp_a1_3 + &D_005F6168;
                        if (temp_v1 >= 0x15) {
                            var_v0_3 = temp_a1_3 + &D_005F6168;
                            if (temp_a0_2->unk62A >= 0x15) {
                                if (temp_a0_2->unk62C >= 0x15) {
                                    *temp_a2 = 1;
                                }
                                goto block_31;
                            }
                        }
                    }
                }
            } else {
block_31:
                var_v0_3 = temp_a1_3 + &D_005F6168;
            }
            temp_a2_2 = *var_v0_3;
            if (temp_a2_2 == 0x19) {
                temp_a0_3 = *(temp_a1_3 + &D_005F6160);
                if ((temp_a0_3 == 1) && ((&sp10)[var_s0] == temp_a2_2)) {
                    sp84[var_s0] = temp_a0_3;
                }
            }
        }
        var_s0 += 1;
        var_v0_2 = 1 - var_s0;
    } while (var_s0 < 2);
    if ((((u16) sp70->unkE0 << 0x10) != 0) || (arg1->unkE0 != 0)) {
        D_005F6174 = 0;
        sp74 = 1;
        D_005F6170 = 0;
    }
    var_s0_2 = 0;
    do {
        temp_a1_4 = var_s0_2 * 4;
        if (*(temp_a1_4 + &D_005F6160) == 0) {
            var_v0_4 = sp84 + temp_a1_4;
            if (*(sp84 + temp_a1_4) == 1) {
                temp_a0_4 = *(sp + temp_a1_4);
                temp_v1_2 = temp_a0_4->unk628;
                if (temp_v1_2 >= 0x101) {
                    var_v0_5 = &(&D_005F6170)[var_s0_2];
                    if (temp_v1_2 < 0x201) {
                        var_v0_5 = &(&D_005F6170)[var_s0_2];
                        if (temp_a0_4->unk62A < 0x201) {
                            var_v0_6 = &(&D_005F6170)[var_s0_2];
                            if (temp_a0_4->unk62C < 0x201) {
                                goto block_53;
                            }
                            goto block_51;
                        }
                    }
                    goto block_52;
                }
                var_v0_5 = &(&D_005F6170)[var_s0_2];
                if ((temp_v1_2 >= 0) && (var_v0_5 = &(&D_005F6170)[var_s0_2], (temp_a0_4->unk62A >= 0))) {
                    var_v0_6 = &(&D_005F6170)[var_s0_2];
                    if (temp_a0_4->unk62C < 0) {
block_51:
                        var_v0_5 = &(&D_005F6170)[var_s0_2];
                        goto block_52;
                    }
block_53:
                    *var_v0_6 = 4;
                } else {
block_52:
                    *var_v0_5 = 2;
                }
                *(temp_a1_4 + &D_005F6178) = 0;
                goto block_55;
            }
        } else {
block_55:
            var_v0_4 = sp84 + temp_a1_4;
        }
        temp_a2_3 = sp7C[var_s0_2];
        var_s0_2 += 1;
        *(temp_a1_4 + &D_005F6160) = *var_v0_4;
        *(temp_a1_4 + &D_005F6168) = temp_a2_3;
    } while (var_s0_2 < 2);
    sp78 = D_004333F0;
    temp_t8 = &D_00433520 + 8;
    temp_s5 = &D_00433540 + 8;
    var_s0_3 = 0;
    var_t6 = 0 * 4;
    do {
        var_a2_2 = &D_005F6150;
        *(var_t6 + &D_005F6158) = *(var_t6 + &D_005F6150);
        if ((sp78 != var_s0_3) || ((*(sp + var_t6))->unkC4 & 1)) {
            temp_a2_4 = *(&sp30 + var_t6);
            if (temp_a2_4 != 1) {
                var_v0_7 = &sp30 + var_t6;
                if (((*(sp + var_t6))->unkE0 == 0) && ((temp_a2_4 != 0) || (var_v0_7 = &sp30 + var_t6, (*(sp80 + var_t6) != 0)))) {
                    temp_v0_2 = 1 - var_s0_3;
                    if (*(sp84 + var_t6) != 0) {
                        var_v0_7 = &sp30 + var_t6;
                        goto block_66;
                    }
                    if ((&sp30)[temp_v0_2] == 2) {
                        if (sp80[temp_v0_2] != 0) {
                            var_t5 = var_s0_3 * 0x10;
                            if (D_005F6180 == 0) {
                                goto block_79;
                            }
                            goto block_97;
                        }
                        var_t5 = var_s0_3 * 0x10;
block_97:
                        *(var_t5 + &D_00433520) = *(var_t5 + &D_00433540);
                        *(var_t5 + temp_t8) = *(var_t5 + temp_s5);
                    } else {
block_79:
                        var_a1 = 1 - var_s0_3;
                        if (var_s0_3 == 0) {
                            if (sp34 != 1) {
                                if (subroutine_arg0->unkE0 == 0) {
                                    var_t5 = var_s0_3 * 0x10;
                                    if ((sp34 == 0) && (sp24 == 0)) {
                                        goto block_85;
                                    }
                                } else {
block_85:
                                    if (sp74 == 0) {
                                        D_00433520.unk10 = (s32) subroutine_arg1->unk4;
                                        var_v1_2 = subroutine_arg1->unkC;
                                    } else {
                                        goto block_88;
                                    }
                                    goto block_90;
                                }
                            } else {
block_88:
                                D_00433520.unk10 = (s32) subroutine_arg1->unk51C;
                                var_v1_2 = subroutine_arg1->unk524;
block_90:
                                D_00433520.unk18 = var_v1_2;
                                if (D_005F6174 != 0) {
                                    var_a1 = 1 - var_s0_3;
                                    if (sp20 == 0) {
                                        temp_v0_3 = D_00433520.unk10 - D_00433540.unk10;
                                        temp_a0_5 = D_00433520.unk18 - D_00433540.unk18;
                                        D_00433520.unk10 = (s32) (D_00433540.unk10 + ((s32) ((temp_v0_3 > -1) ? temp_v0_3 : (temp_v0_3 + 3)) >> 2));
                                        D_00433520.unk18 = (s32) (D_00433540.unk18 + ((s32) ((temp_a0_5 > -1) ? temp_a0_5 : (temp_a0_5 + 3)) >> 2));
                                        D_005F6174 -= 1;
                                        goto block_93;
                                    }
                                } else {
block_93:
                                    var_a1 = 1 - var_s0_3;
                                }
                                goto block_94;
                            }
                        } else {
block_94:
                            var_t5 = var_s0_3 * 0x10;
                        }
                        temp_a1_5 = var_a1 * 0x10;
                        temp_a2_5 = var_t5 + &D_00433520;
                        temp_t2 = temp_a1_5 + &D_00433520;
                        temp_t3 = *(temp_a1_5 + &D_00433540);
                        temp_t4 = temp_a1_5 + temp_s5;
                        temp_a1_6 = temp_a1_5 + temp_t8;
                        temp_a0_6 = var_t5 + temp_t8;
                        temp_t0 = var_t5 + &D_00433540;
                        *temp_a2_5 += *temp_t2 - temp_t3;
                        temp_t1 = var_t5 + temp_s5;
                        *temp_a0_6 += *temp_a1_6 - *temp_t4;
                        *temp_t0 += *temp_t2 - temp_t3;
                        *temp_t1 += *temp_a1_6 - *temp_t4;
                    }
                    goto block_98;
                }
block_66:
                temp_a0_7 = *var_v0_7;
                if ((temp_a0_7 != 1) && (temp_v0_4 = *(sp + var_t6), (temp_v0_4->unkE0 == 0))) {
                    if (temp_a0_7 != 2) {
                        var_t5 = var_s0_3 * 0x10;
                        *(var_t5 + &D_00433520) = temp_v0_4->unk4;
                        *(var_t5 + temp_t8) = temp_v0_4->unkC;
                    } else {
                        goto block_71;
                    }
                } else {
                    goto block_71;
                }
                goto block_73;
            }
block_71:
            var_t5 = var_s0_3 * 0x10;
            temp_a1_7 = *(sp + var_t6);
            *(var_t5 + &D_00433520) = temp_a1_7->unk51C;
            *(var_t5 + temp_t8) = temp_a1_7->unk524;
block_73:
            temp_t2_2 = var_t6 + &D_005F6170;
            temp_t1_2 = *temp_t2_2;
            if (temp_t1_2 != 0) {
                var_v0_8 = sp + var_t6;
                if (sp80[1 - var_s0_3] == 0) {
                    temp_a3 = var_t5 + &D_00433520;
                    temp_a1_8 = *(var_t5 + &D_00433540);
                    temp_a2_6 = *(var_t5 + temp_s5);
                    temp_t0_2 = var_t5 + temp_t8;
                    temp_a0_8 = *temp_a3 - temp_a1_8;
                    *temp_t2_2 = temp_t1_2 - 1;
                    *temp_a3 = temp_a1_8 + ((s32) ((temp_a0_8 > -1) ? temp_a0_8 : (temp_a0_8 + 3)) >> 2);
                    temp_v0_5 = *temp_t0_2 - temp_a2_6;
                    *temp_t0_2 = temp_a2_6 + ((s32) ((temp_v0_5 > -1) ? temp_v0_5 : (temp_v0_5 + 3)) >> 2);
                    goto block_98;
                }
            } else {
block_98:
                var_v0_8 = sp + var_t6;
            }
            var_a2_2 = *var_v0_8;
            *(var_t6 + &D_005F6150) = (s32) var_a2_2->unk624;
            if (*(&sp30 + var_t6) != 2) {
                *(var_t5 + &D_00433524) = var_a2_2->unk70C - 0x122;
            } else {
                var_a2_2 = &D_00433524;
                *(var_t5 + &D_00433524) = *(var_t5 + &D_00433544);
            }
        }
        var_s0_3 += 1;
        var_t6 = var_s0_3 * 4;
    } while (var_s0_3 < 2);
    temp_s3 = &D_00433520 + 8;
    temp_fp = &D_00433540 + 8;
    var_s0_4 = 0;
    do {
        temp_a3_2 = 1 - var_s0_4;
        if ((sp80[var_s0_4] != 0) && (sp80[temp_a3_2] == 0) && (sp74 == 0)) {
            temp_a0_9 = sp7C[var_s0_4];
            switch (temp_a0_9) {                    /* irregular */
            case 25:
                break;
            default:
                var_a2_2 = (s32 *) (var_s0_4 * 0x10);
                if (temp_a0_9 != 0x17) {
                } else {
                    temp_a3_3 = temp_a3_2 * 0x10;
                    temp_a0_10 = *(var_a2_2 + &D_00433540);
                    temp_t0_3 = var_a2_2 + &D_00433520;
                    temp_a1_9 = *(var_a2_2 + temp_fp);
                    temp_v1_3 = (*(temp_a3_3 + &D_00433520) - temp_a0_10) * 6;
                    var_a2_2 += temp_s3;
                    *temp_t0_3 = temp_a0_10 + (temp_v1_3 >> 7);
                    *var_a2_2 = temp_a1_9 + ((s32) ((*(temp_a3_3 + temp_s3) - temp_a1_9) * 6) >> 7);
                }
                break;
            case 24:
                /* fallthrough */
            case 26:
                temp_v0_6 = D_00433520.unk0 - D_00433520.unk10;
                temp_a0_11 = D_00433520.unk8 - D_00433520.unk18;
                temp_v0_7 = func_00398548((temp_v0_6 * temp_v0_6) + (temp_a0_11 * temp_a0_11), D_00433520.unk18, var_a2_2, temp_a3_2);
                temp_a3_4 = (1 - var_s0_4) * 0x10;
                temp_t0_4 = var_s0_4 * 0x10;
                temp_lo = (s32) (0x2198 - ((s32) ((temp_v0_7 > -1) ? temp_v0_7 : (temp_v0_7 + 0x3F)) >> 6)) / 1250;
                temp_v0_8 = *(temp_t0_4 + &D_00433540);
                temp_a2_7 = *(temp_t0_4 + temp_fp);
                *(temp_t0_4 + &D_00433520) = temp_v0_8 + ((s32) ((*(temp_a3_4 + &D_00433520) - temp_v0_8) * temp_lo) >> 7);
                var_a2_2 = temp_a2_7 + ((s32) ((*(temp_a3_4 + temp_s3) - temp_a2_7) * temp_lo) >> 7);
                *(temp_t0_4 + temp_s3) = var_a2_2;
                break;
            }
        }
        var_s0_4 += 1;
    } while (var_s0_4 < 2);
    if ((D_00433520.unk0 == D_00433520.unk10) && (D_00433520.unk8 == D_00433520.unk18)) {
        temp_a1_10 = D_00433520.unk0 + 1;
        D_00433520.unk0 -= 1;
        D_00433520.unk18 = (s32) (D_00433520.unk8 + 1);
        D_00433520.unk8 = (s32) (D_00433520.unk8 - 1);
        D_00433520.unk10 = temp_a1_10;
    }
    var_s0_5 = 0;
    var_v0 = 0 * 0x10;
loop_122:
    var_s0_5 += 1;
    M2C_ERROR(/* unknown instruction: ldl $t1, 0x7($a0) */);
    M2C_ERROR(/* unknown instruction: ldr $t1, ($a0) */);
    M2C_ERROR(/* unknown instruction: ldl $t2, 0xf($a0) */);
    M2C_ERROR(/* unknown instruction: ldr $t2, 0x8($a0) */);
    M2C_ERROR(/* unknown instruction: sdl $t1, 0x7($v0) */);
    M2C_ERROR(/* unknown instruction: sdr $t1, ($v0) */);
    M2C_ERROR(/* unknown instruction: sdl $t2, 0xf($v0) */);
    M2C_ERROR(/* unknown instruction: sdr $t2, 0x8($v0) */);
    if (var_s0_5 < 2) {
        var_v0 = var_s0_5 * 0x10;
        goto loop_122;
    }
    sp70->unkE0 = (s16) (u16) sp50;
    arg1->unkE0 = (s16) (u16) sp54;
    return var_v0 + &D_00433540;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038EC08);
#endif

/* NON_MATCHING: the two move kinds are stored to the stack in another order and 0x13 is kept in a3 instead of a0 */
#if 0
s32 func_0038F6B0(void) {
    U32Pair k;
    U32Pair t;
    s32 r;

    t.unk0 = gFighters[0].unkC0->unk24 >> 23;
    t.unk4 = gFighters[1].unkC0->unk24 >> 23;
    k = t;
    r = -1;
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
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038F6B0);
#endif

/* NON_MATCHING: the loop-invariant field addresses are hoisted into other temporaries (4 lines) */
#if 0
void func_0038F768(void) {
    s32 i;

    for (i = 0; i < 5; i++) {
        D_008B9148[i].unk0 = 0;
        D_008B9148[i].unk4 = 0;
        D_008B9148[i].unk8 = 0;
        D_008B9148[i].unk18 = 0;
        D_008B9148[i].unkC = 0;
        D_008B9148[i].unk10 = 0;
        D_008B9148[i].unk14 = 0;
        D_008B9148[i].unk1C = 0;
        D_008B9148[i].unk20 = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038F768);
#endif

/* NON_MATCHING: m2c draft, not cleaned up (358 instructions; stack locals, struct copies and some types still raw) */
#if 0
s32 func_0038F7F8(void *arg0, void *arg1, s32 *arg2, s32 arg3) {
    ? *sp30;
    ? *sp34;
    ? *var_v0;
    ? *var_v0_2;
    s32 *temp_a3_2;
    s32 *temp_s0_4;
    s32 *temp_s1;
    s32 *temp_v1_2;
    s32 *temp_v1_4;
    s32 temp_a0;
    s32 temp_a0_2;
    s32 temp_a0_3;
    s32 temp_a0_4;
    s32 temp_a1_2;
    s32 temp_a1_3;
    s32 temp_a1_4;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_a3;
    s32 temp_lo;
    s32 temp_s0;
    s32 temp_s0_2;
    s32 temp_s0_3;
    s32 temp_s3;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 temp_v1;
    s32 temp_v1_3;
    s32 var_a0;
    s32 var_a0_2;
    s32 var_fp;
    s32 var_s2;
    s32 var_s5;
    s32 var_v0_4;
    s32 var_v0_5;
    s32 var_v1_2;
    s32 var_v1_3;
    void *temp_a1;
    void *temp_a1_5;
    void *var_v0_3;
    void *var_v1;
    var_fp = 0;
    var_s5 = 0;
    temp_s3 = M2C_ERROR(/* Read from unset register $t0 */);
    if ((((u32) arg0->unkC0->unk24 >> 0x14) & 3) == 2) {
        var_v0 = arg0 + 0x51C;
    } else {
        var_v0 = &D_00433520;
    }
    sp30 = var_v0;
    if ((((u32) arg1->unkC0->unk24 >> 0x14) & 3) == 2) {
        var_v0_2 = arg1 + 0x51C;
    } else {
        var_v0_2 = &D_00433530;
    }
    sp34 = var_v0_2;
    M2C_ERROR(/* unknown instruction: ldl $v0, 0x37($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $v0, 0x30($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $v0, 0x27($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v0, 0x20($sp) */);
    func_0038E408(sp, temp_s3);
    func_0038E408(sp, 3);
    var_v0_3 = sp24;
    if (arg0->unk624 < arg1->unk624) {
        var_v1 = sp20;
    } else {
        var_v0_3 = sp20;
        var_v1 = sp24;
    }
    temp_a3 = var_v0_3->unk8;
    temp_a2 = var_v0_3->unk0;
    temp_s0 = func_00366108(temp_a2 - var_v1->unk0, temp_a3 - var_v1->unk8, temp_a2, temp_a3);
    temp_s0_2 = (func_0037D898() != 0) ? 0 : temp_s0;
    func_0037D890();
    temp_s0_3 = temp_s0_2 << 6;
    temp_v1 = temp_s0_3 - sp10;
    if (!(temp_v1 & 0x20000)) {
        var_s2 = temp_v1 & 0x3FFFF;
    } else {
        var_s2 = temp_v1 | 0xFFFC0000;
    }
    if (var_s2 > 0) {
        var_s5 = 1;
    } else if (var_s2 < 0) {
        var_s2 = -var_s2;
        var_s5 = -1;
    }
    if (arg3 != 0) {
        temp_a1 = &D_008B9148 + (temp_s3 * 0x3C);
        temp_a1->unk4 = 0;
        temp_a1->unk8 = 0;
        temp_a1->unk34 = 0;
        temp_a1->unk38 = 0;
        temp_a1->unk10 = 0;
        temp_a1->unkC = 0;
        temp_a1->unk14 = 0;
        *arg2 = temp_s0_3;
        return arg3;
    }
    if ((temp_s3 == 0) && (var_s2 <= 0x1AAAA)) {
        if ((u32) D_005F5C28 < 0xC8U) {
            goto block_22;
        }
        switch (D_008B9148.unk0) {                  /* irregular */
        case 0:
            if (var_s2 < 0xE38) {
                D_008B9148.unk20 = 0;
            } else {
                D_008B9148.unk20 = (s32) (D_008B9148.unk20 + 1);
            }
            temp_a1_2 = temp_s3 * 0x3C;
            temp_a3_2 = temp_a1_2 + (&D_008B9148 + 0x20);
            if (*temp_a3_2 > 0) {
                *(temp_a1_2 + &D_008B9148) = 1;
                temp_v1_2 = temp_a1_2 + (&D_008B9148 + 8);
                *temp_v1_2 += var_s5 * 0x600;
                *temp_a3_2 = 0;
            }
            break;
        case 1:
            if (var_s2 < 0x38E4) {
                temp_a1_3 = (D_008B9148.unk8 - ((s32) D_008B9148.unk8 >> 1)) + ((s32) (var_s5 * 0x600) >> 1);
                D_008B9148.unk4 = (s32) (D_008B9148.unk4 - ((s32) (D_008B9148.unk4 * 0xE) >> 7));
                D_008B9148.unk8 = temp_a1_3;
                if (var_s2 < 0xE39) {
                    temp_v1_3 = D_008B9148.unk20 + 1;
                    D_008B9148.unk8 = (s32) (temp_a1_3 - ((s32) (temp_a1_3 + ((u32) temp_a1_3 >> 0x1F)) >> 1));
                    D_008B9148.unk20 = temp_v1_3;
                    if (temp_v1_3 >= 0x20) {
                        D_008B9148.unk20 = 0;
                        D_008B9148.unk4 = 0;
                        D_008B9148.unk0 = 0;
                    }
                } else {
                    D_008B9148.unk20 = 0;
                }
            } else {
                if (var_s5 > 0) {
                    var_v0_4 = D_008B9148.unk8 + 0x600;
                    goto block_41;
                }
                if (var_s5 < 0) {
                    var_v0_4 = D_008B9148.unk8 - 0x600;
block_41:
                    D_008B9148.unk8 = var_v0_4;
                }
            }
            temp_a0 = temp_s3 * 0x3C;
            temp_s1 = temp_a0 + (&D_008B9148 + 4);
            temp_s0_4 = temp_a0 + (&D_008B9148 + 8);
            *temp_s1 += *temp_s0_4;
            temp_v0 = func_0038F6B0(temp_a0);
            if (temp_v0 > 0) {
                temp_a2_2 = *temp_s1;
                temp_a1_4 = (s32) (temp_v0 * 0xDDD0) >> 8;
                if (temp_a1_4 < temp_a2_2) {
                    *temp_s0_4 -= 0x600;
                    *temp_s1 = temp_a1_4;
                } else {
                    temp_a0_2 = (s32) (temp_v0 * 0xFFFF2230) >> 8;
                    if (temp_a2_2 < temp_a0_2) {
                        *temp_s0_4 += 0x600;
                        *temp_s1 = temp_a0_2;
                    }
                }
            } else {
                temp_a0_3 = *temp_s1;
                var_v1_2 = 0xDDD0;
                if (temp_a0_3 > 0xDDD0) {
                    var_v0_5 = *temp_s0_4 - 0x600;
                    goto block_52;
                }
                var_v1_2 = -0xDDD0;
                if (temp_a0_3 < 0xFFFF2230) {
                    var_v0_5 = *temp_s0_4 + 0x600;
block_52:
                    *temp_s0_4 = var_v0_5;
                    *temp_s1 = var_v1_2;
                }
            }
            var_fp = (s32) (&D_008B9148 + (temp_s3 * 0x3C))->unk4 >> 6;
            break;
        }
        temp_a0_4 = temp_s3 * 0x3C;
        (&D_008B9148 + temp_a0_4)->unk18 = var_s2;
        if (var_s2 > 0xE38D) {
            temp_v1_4 = temp_a0_4 + (&D_008B9148 + 0x1C);
            *temp_v1_4 += 1;
        } else {
            (&D_008B9148 + temp_a0_4)->unk1C = 0;
        }
        var_v1_3 = sp10 + var_fp;
        if (temp_s3 == 0) {
            if (var_s2 >= 0x38E3) {
                temp_lo = (s32) ((var_s2 - 0x38E3) << 0xC) / 50973;
                D_008B9148.unk10 = (s32) ((s32) (*((((s32) ((temp_lo > -1) ? temp_lo : (temp_lo + 0xFF)) >> 7) & 0x1FFE) + &gSinTable) * 0x44440) >> 0xC);
            } else {
                D_008B9148.unk10 = 0;
            }
            if (D_008B9148.unk24 == 0) {
                D_008B9148.unk14 = 0;
            } else {
                temp_v0_2 = D_008B9148.unk10 - D_008B9148.unkC;
                var_a0 = temp_v0_2 >> 4;
                if (temp_v0_2 >= 0) {
                    var_a0 = (temp_v0_2 >= 1) ? var_a0 : 0;
                }
                if (var_a0 >= 0x101) {
                    var_a0_2 = 0x100;
                } else {
                    var_a0_2 = (var_a0 <= -0x101) ? -0x100 : var_a0;
                }
                D_008B9148.unk14 = var_a0_2;
                D_008B9148.unkC = (s32) (D_008B9148.unkC + var_a0_2);
            }
            var_v1_3 = sp10 + var_fp;
        }
        *arg2 = var_v1_3;
        return arg3;
    }
block_22:
    temp_a1_5 = &D_008B9148 + (temp_s3 * 0x3C);
    temp_a1_5->unk34 = 0;
    temp_a1_5->unk38 = 0;
    *arg2 = sp10;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038F7F8);
#endif

/* NON_MATCHING: m2c draft, not cleaned up (1054 instructions; stack locals, struct copies and some types still raw) */
#if 0
s32 func_0038FD90(void *arg0, void *arg1, s32 arg2, s32 arg3) {
    s32 sp10;
    s32 sp14;
    s32 sp18;
    s32 sp20;
    s32 sp24;
    s32 sp28;
    s32 sp30;
    s32 sp38;
    s32 sp40;
    s32 sp44;
    s32 sp48;
    s32 sp4C;
    s32 sp50;
    s32 sp54;
    s32 sp78;
    u32 sp90;
    u32 sp94;
    s32 spA0;
    s32 spA4;
    s32 spB0;
    s32 spB4;
    s32 spC4;
    s32 spC8;
    s32 spCC;
    void *spE0;
    void *spE4;
    s32 spE8;
    s32 spEC;
    s32 spF0;
    s32 spF4;
    s32 spF8;
    s32 *spFC;
    s32 sp100;
    s32 sp104;
    s16 temp_a1_2;
    s16 temp_a2_9;
    s16 temp_a3_8;
    s16 temp_s2;
    s16 temp_s3;
    s16 temp_s6;
    s16 temp_t0_3;
    s16 temp_t1_5;
    s16 temp_t7;
    s16 temp_v1_4;
    s32 *temp_a0_6;
    s32 *temp_a3_7;
    s32 temp_a0;
    s32 temp_a0_10;
    s32 temp_a0_11;
    s32 temp_a0_12;
    s32 temp_a0_13;
    s32 temp_a0_2;
    s32 temp_a0_3;
    s32 temp_a0_4;
    s32 temp_a0_5;
    s32 temp_a0_7;
    s32 temp_a0_8;
    s32 temp_a0_9;
    s32 temp_a1;
    s32 temp_a1_10;
    s32 temp_a1_12;
    s32 temp_a1_3;
    s32 temp_a1_4;
    s32 temp_a1_5;
    s32 temp_a1_6;
    s32 temp_a1_7;
    s32 temp_a1_8;
    s32 temp_a1_9;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_a2_3;
    s32 temp_a2_4;
    s32 temp_a2_5;
    s32 temp_a2_6;
    s32 temp_a2_7;
    s32 temp_a2_8;
    s32 temp_a3;
    s32 temp_a3_2;
    s32 temp_a3_3;
    s32 temp_a3_4;
    s32 temp_a3_5;
    s32 temp_a3_6;
    s32 temp_a3_9;
    s32 temp_fp;
    s32 temp_fp_2;
    s32 temp_s0;
    s32 temp_s1;
    s32 temp_s4;
    s32 temp_t0;
    s32 temp_t1;
    s32 temp_t1_2;
    s32 temp_t1_3;
    s32 temp_t1_4;
    s32 temp_t1_6;
    s32 temp_t2;
    s32 temp_t2_2;
    s32 temp_t4;
    s32 temp_t7_2;
    s32 temp_v0;
    s32 temp_v0_10;
    s32 temp_v0_11;
    s32 temp_v0_12;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v0_4;
    s32 temp_v0_5;
    s32 temp_v0_6;
    s32 temp_v0_7;
    s32 temp_v0_8;
    s32 temp_v0_9;
    s32 temp_v1;
    s32 temp_v1_10;
    s32 temp_v1_11;
    s32 temp_v1_12;
    s32 temp_v1_3;
    s32 temp_v1_5;
    s32 temp_v1_6;
    s32 temp_v1_7;
    s32 temp_v1_8;
    s32 temp_v1_9;
    s32 var_a0;
    s32 var_a0_2;
    s32 var_a0_3;
    s32 var_a0_4;
    s32 var_a1;
    s32 var_a1_2;
    s32 var_a1_3;
    s32 var_a1_4;
    s32 var_a1_5;
    s32 var_a2;
    s32 var_a2_2;
    s32 var_a3;
    s32 var_a3_2;
    s32 var_a3_3;
    s32 var_a3_4;
    s32 var_s3;
    s32 var_t0;
    s32 var_t1;
    s32 var_t1_2;
    s32 var_t2;
    s32 var_t3;
    s32 var_t4;
    s32 var_t5;
    s32 var_t5_2;
    s32 var_t7;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v1;
    s32 var_v1_2;
    u32 *temp_t0_2;
    void *temp_a1_11;
    void *temp_v1_2;
    spE0 = arg0;
    spE4 = arg1;
    temp_fp = M2C_ERROR(/* Read from unset register $t2 */);
    spE8 = arg2;
    temp_s1 = M2C_ERROR(/* Read from unset register $t3 */);
    spEC = arg3;
    spF0 = M2C_ERROR(/* Read from unset register $t0 */);
    spF4 = M2C_ERROR(/* Read from unset register $t1 */);
    memset(sp, 0, 0x10);
    spF8 = 0;
    temp_s0 = D_00433330.unkA4;
    sp90 = (u32) spE0->unkC0->unk24 >> 0x17;
    sp94 = (u32) spE4->unkC0->unk24 >> 0x17;
    M2C_ERROR(/* unknown instruction: ldl $a1, 0x97($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $a1, 0x90($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a1, 0x87($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a1, 0x80($sp) */);
    if ((sp80 == 0x17) || (sp84 == 0x17) || (sp80 == 0x18) || (sp84 == 0x18) || (sp80 == 0x19) || (sp84 == 0x19) || (sp80 == 0x1A) || (sp84 == 0x1A)) {
        spF8 = 1;
    }
    spFC = &sp40;
    temp_fp_2 = (temp_fp <= 0x320) ? 0x320 : temp_fp;
    func_0038E408(&sp40, temp_s1, 0x17);
    temp_a1 = -spE8;
    temp_a0 = -spEC;
    temp_v0 = ((s32) ((temp_a1 > -1) ? temp_a1 : (temp_a1 + 0x3F)) >> 6) & 0xFFF;
    temp_v1 = ((s32) ((temp_a0 > -1) ? temp_a0 : (temp_a0 + 0x3F)) >> 6) & 0xFFF;
    temp_t7 = *((temp_v0 * 2) + &gSinTable);
    temp_s6 = *(((temp_v0 + 0x400) * 2) + &gSinTable);
    temp_s3 = *((temp_v1 * 2) + &gSinTable);
    M2C_ERROR(/* unknown instruction: ldl $v0, 0x47($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $v0, 0x40($sp) */);
    M2C_ERROR(/* unknown instruction: ldl $v1, 0x4f($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $v1, 0x48($sp) */);
    M2C_ERROR(/* unknown instruction: ldl $a0, 0x57($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $a0, 0x50($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $v0, 0x67($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v0, 0x60($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $v1, 0x6f($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v1, 0x68($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a0, 0x77($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a0, 0x70($sp) */);
    sp78 = sp58;
    temp_t1 = (s32) (D_00433520.unk8 + D_00433520.unk18) >> 1;
    temp_t0 = (s32) (D_00433520.unk0 + D_00433520.unk10) >> 1;
    temp_s2 = *(((temp_v1 + 0x400) * 2) + &gSinTable);
    if (spF4 != 0) {
        temp_v1_2 = spEC + &D_3003F;
        sp14 = sp44;
        temp_v0_2 = ((s32) (((s32) temp_v1_2 > -1) ? temp_v1_2 : temp_v1_2) >> 6) & 0xFFF;
        sp18 = temp_t1 + ((s32) (temp_fp_2 * *((temp_v0_2 * 2) + &gSinTable)) >> 0xC);
        sp10 = temp_t0 + ((s32) (temp_fp_2 * *(((temp_v0_2 + 0x400) * 2) + &gSinTable)) >> 0xC);
    } else {
        temp_v1_3 = spEC - sp50;
        temp_a3 = sp40 - temp_t0;
        temp_a2 = sp48 - temp_t1;
        var_a1 = temp_v1_3 & 0x3FFFF;
        if (temp_v1_3 & 0x20000) {
            var_a1 |= (s32) &D_FFFC003F;
        }
        temp_v0_3 = ((s32) ((var_a1 > -1) ? var_a1 : var_a1) >> 6) & 0xFFF;
        sp14 = sp44;
        temp_a1_2 = *((temp_v0_3 * 2) + &gSinTable);
        temp_v1_4 = *(((temp_v0_3 + 0x400) * 2) + &gSinTable);
        sp18 = temp_t1 + ((s32) ((temp_a1_2 * temp_a3) + (temp_v1_4 * temp_a2)) >> 0xC);
        sp10 = temp_t0 + ((s32) ((temp_v1_4 * temp_a3) - (temp_a1_2 * temp_a2)) >> 0xC);
    }
    sp100 = spE8 + 0x3F;
    sp104 = spEC + 0x3F;
    var_t5 = 0;
    do {
        temp_a3_2 = var_t5 * 0x10;
        temp_a0_2 = *(temp_a3_2 + &D_00433520) - sp10;
        temp_v0_4 = *(temp_a3_2 + &D_00433528) - sp18;
        temp_a2_2 = *(temp_a3_2 + &D_00433524) - sp14;
        temp_a1_3 = (s32) ((temp_s3 * temp_a0_2) + (temp_s2 * temp_v0_4)) >> 0xC;
        (sp + temp_a3_2)->unk20 = (s32) ((s32) ((temp_s2 * temp_a0_2) - (temp_s3 * temp_v0_4)) >> 0xC);
        temp_t0_2 = &(&sp90)[var_t5];
        var_t5 += 1;
        (sp + temp_a3_2)->unk28 = (s32) ((s32) ((temp_s6 * temp_a1_3) - (temp_t7 * temp_a2_2)) >> 0xC);
        *(&sp24 + temp_a3_2) = (s32) ((temp_t7 * temp_a1_3) + (temp_s6 * temp_a2_2)) >> 0xC;
        *temp_t0_2 = (u32) temp_a1_3;
    } while (var_t5 < 2);
    temp_t1_2 = (s32) (sp90 + sp94) >> 1;
    temp_a3_3 = -0x1C2 - sp14;
    temp_a0_3 = D_00433330.unkA4 * -0xCE4;
    temp_v1_5 = (s32) ((temp_a0_3 > -1) ? temp_a0_3 : (temp_a0_3 + 0xFF)) >> 8;
    temp_t7_2 = (s32) ((temp_s6 * temp_t1_2) - (temp_t7 * temp_a3_3)) >> 0xC;
    temp_v0_5 = temp_fp_2 - temp_t7_2;
    temp_v0_6 = (temp_v0_5 < temp_v1_5) ? temp_v1_5 : temp_v0_5;
    temp_s4 = (s32) ((temp_t7 * temp_t1_2) + (temp_s6 * temp_a3_3)) >> 0xC;
    temp_t1_3 = sp28 + temp_v0_6;
    sp38 += temp_v0_6;
    var_t7 = temp_t7_2 + temp_v0_6;
    sp28 = temp_t1_3;
    if (temp_t1_3 == 0) {
        sp28 = 1;
    }
    var_t4 = sp38;
    if (var_t4 == 0) {
        var_t4 = 1;
        sp38 = 1;
    }
    temp_a0_4 = sp20 - 0x96;
    if (sp20 < sp30) {
        temp_a1_4 = sp30 + 0x96;
        var_t3 = sp28;
        var_a3 = 0;
        sp20 = temp_a0_4;
        var_s3 = 1;
        sp30 = temp_a1_4;
        var_v1 = (s32) (temp_a1_4 * temp_s0) / var_t4;
        var_t2 = (s32) (temp_a0_4 * temp_s0) / var_t3;
    } else {
        temp_a0_5 = sp30 - 0x96;
        var_a3 = 1;
        temp_a1_5 = sp20 + 0x96;
        sp30 = temp_a0_5;
        sp20 = temp_a1_5;
        var_s3 = 0;
        var_t3 = sp28;
        var_t2 = (s32) (temp_a0_5 * temp_s0) / var_t4;
        var_v1 = (s32) (temp_a1_5 * temp_s0) / var_t3;
    }
    temp_v1_6 = var_v1 - var_t2;
    if (temp_v1_6 < 0x189) {
        D_005F6180 = 0;
    } else {
        D_005F6180 = 1;
    }
    if (temp_v1_6 >= 0x189) {
        temp_v1_7 = var_s3 * 0x10;
        temp_a3_4 = var_a3 * 0x10;
        temp_t1_4 = *(&sp28 + temp_a3_4);
        var_a2 = temp_t1_4 - ((s32) ((temp_s0 * (*(&sp20 + temp_v1_7) - *(&sp20 + temp_a3_4))) - ((*(&sp28 + temp_v1_7) - temp_t1_4) * 0xC4)) / 392);
        if (spF4 != 0) {
            (&D_008B9148 + (temp_s1 * 0x3C))->unk30 = var_a2;
        } else {
            temp_a0_6 = (temp_s1 * 0x3C) + (&D_008B9148 + 0x30);
            temp_a3_5 = *temp_a0_6;
            if (temp_a3_5 < var_a2) {
                var_a2 = temp_a3_5 + ((s32) ((var_a2 - temp_a3_5) * 4) >> 7);
            } else {
                var_a2 = temp_a3_5 - ((s32) ((temp_a3_5 - var_a2) * 0x10) >> 7);
            }
            *temp_a0_6 = var_a2;
        }
        temp_a1_6 = var_t3 - var_a2;
        temp_a0_7 = var_t4 - var_a2;
        var_t7 -= var_a2;
        var_t3 = temp_a1_6;
        var_t4 = temp_a0_7;
        sp28 = temp_a1_6;
        sp38 = temp_a0_7;
    } else {
        if (spF4 != 0) {
            goto block_41;
        }
        temp_a2_3 = var_s3 * 0x10;
        temp_a3_6 = var_a3 * 0x10;
        if ((((s32) (*(&sp28 + temp_a2_3) * 0x44) / temp_s0) - (*(&sp20 + temp_a2_3) - *(&sp20 + temp_a3_6))) < ((s32) (*(&sp28 + temp_a3_6) * -0x44) / temp_s0)) {
block_41:
            if (spF4 == 0) {
                temp_a2_4 = (&D_008B9148 + (temp_s1 * 0x3C))->unk30;
                var_t1 = 0;
                if (temp_a2_4 < 0) {
                    temp_a0_8 = -temp_a2_4;
                    var_t1 = temp_a2_4 + ((s32) ((temp_a0_8 > -1) ? temp_a0_8 : (temp_a0_8 + 0x1F)) >> 5);
                } else if (spF4 < temp_a2_4) {
                    var_t1 = temp_a2_4 - ((s32) ((temp_a2_4 > -1) ? temp_a2_4 : (temp_a2_4 + 7)) >> 3);
                }
                goto block_56;
            }
            (&D_008B9148 + (temp_s1 * 0x3C))->unk30 = 0;
        } else {
            if (var_t2 < -0x44) {
                var_t1 = 0;
                var_a3_2 = (&D_008B9148 + (temp_s1 * 0x3C))->unk30;
                if (var_a3_2 >= 0) {
                    var_v0 = spF4 < var_a3_2;
                    goto block_54;
                }
                goto block_52;
                goto block_56;
            }
            if (var_v1 >= 0x45) {
                var_t1 = 0;
                var_a3_2 = (&D_008B9148 + (temp_s1 * 0x3C))->unk30;
                if (var_a3_2 < 0) {
block_52:
                    temp_a0_9 = -var_a3_2;
                    var_t1 = var_a3_2 + ((s32) ((temp_a0_9 > -1) ? temp_a0_9 : (temp_a0_9 + 0x1F)) >> 5);
                } else {
                    var_v0 = spF4 < var_a3_2;
block_54:
                    if (var_v0 != 0) {
                        var_t1 = var_a3_2 - ((s32) ((var_a3_2 > -1) ? var_a3_2 : (var_a3_2 + 7)) >> 3);
                    }
                }
block_56:
                temp_a1_7 = var_t3 - var_t1;
                temp_a2_5 = var_t4 - var_t1;
                var_t7 -= var_t1;
                var_t3 = temp_a1_7;
                var_t4 = temp_a2_5;
                (&D_008B9148 + (temp_s1 * 0x3C))->unk30 = var_t1;
                sp28 = temp_a1_7;
                sp38 = temp_a2_5;
            } else {
                temp_a2_6 = (&D_008B9148 + (temp_s1 * 0x3C))->unk30;
                var_t1_2 = 0;
                if (temp_a2_6 < 0) {
                    temp_a0_10 = -temp_a2_6;
                    var_t1_2 = temp_a2_6 + ((s32) ((temp_a0_10 > -1) ? temp_a0_10 : (temp_a0_10 + 0x1F)) >> 5);
                } else if (spF4 < temp_a2_6) {
                    var_t1_2 = temp_a2_6 - ((s32) ((temp_a2_6 > -1) ? temp_a2_6 : (temp_a2_6 + 7)) >> 3);
                }
                temp_a1_8 = var_t3 - var_t1_2;
                temp_a2_7 = var_t4 - var_t1_2;
                sp28 = temp_a1_8;
                var_t7 -= var_t1_2;
                sp38 = temp_a2_7;
                (&D_008B9148 + (temp_s1 * 0x3C))->unk30 = var_t1_2;
                var_t3 = temp_a1_8;
                var_t4 = temp_a2_7;
            }
        }
    }
    var_t0 = 0x8C;
    if (var_t3 == 0) {
        var_t3 = 1;
        sp28 = 1;
    }
    if (var_t4 == 0) {
        var_t4 = 1;
        sp38 = 1;
    }
    var_a1_2 = sp34;
    if (((s32) (sp24 * temp_s0) / var_t3) < ((s32) (var_a1_2 * temp_s0) / var_t4)) {
        var_a1_2 = sp24;
        var_a3_3 = var_t3;
        var_v0_2 = D_00433520.unk4;
    } else {
        var_a3_3 = var_t4;
        var_v0_2 = D_00433520.unk14;
    }
    temp_a0_11 = (s32) (var_v0_2 * temp_s6) >> 0xC;
    if (temp_s1 == 1) {
        var_t0 = (spE8 <= -1) ? 0xB4 : 0x8C;
    }
    var_t5_2 = 0;
    if ((temp_s1 == 1) && (spE8 > 0)) {
        if (temp_a0_11 >= -0x3E8) {
            var_t5_2 = (s32) ((temp_a0_11 + 0x3E8) << 9) / 640;
        }
        if (spF4 == 0) {
            var_v0_3 = var_t5_2 - D_005F6148;
            if (D_005F6148 < var_t5_2) {
                var_v1_2 = D_005F6148 + 0x1C;
                goto block_80;
            }
            var_v0_3 = D_005F6148 - var_t5_2;
            if (var_t5_2 < D_005F6148) {
                var_v1_2 = D_005F6148 - 0x1C;
block_80:
                var_t5_2 = (var_v0_3 >= 0x1D) ? var_v1_2 : var_t5_2;
            }
        }
        D_005F6148 = var_t5_2;
    }
    temp_t2 = (s32) (temp_s6 * (var_t0 - ((s32) (temp_s0 * 0x1C2) / temp_fp_2))) >> 0xC;
    temp_v0_7 = temp_t2 + 0x9C;
    temp_a1_9 = ((s32) (((var_a3_3 - var_t7) * -0x9C) - (temp_s0 * (var_a1_2 - temp_s4))) / (s32) ((temp_v0_7 == 0) ? 1 : temp_v0_7)) - var_t7;
    var_a1_3 = (temp_a1_9 <= -1) ? 0 : temp_a1_9;
    if (spF4 != 0) {
        var_a1_3 = 0;
        (&D_008B9148 + (temp_s1 * 0x3C))->unk28 = 0;
    } else {
        temp_a2_8 = temp_s1 * 0x3C;
        temp_a3_7 = temp_a2_8 + (&D_008B9148 + 0x2C);
        temp_v1_8 = *temp_a3_7;
        if (var_a1_3 < temp_v1_8) {
            temp_v1_9 = temp_v1_8 - var_a1_3;
            temp_v1_10 = (temp_v1_9 >= 0x4B1) ? 0x4B0 : temp_v1_9;
            temp_v0_8 = ((s32) (temp_v1_10 * 0x78) >> 7) - temp_v1_10;
            (&D_008B9148 + temp_a2_8)->unk28 = temp_v0_8;
            var_a1_3 = *temp_a3_7 + temp_v0_8;
        } else if (temp_v1_8 < var_a1_3) {
            temp_a1_10 = var_a1_3 - temp_v1_8;
            if ((sp80 == 0x19) || (sp84 == 0x19)) {
                var_a0 = 4;
            } else {
                var_a0 = (spF8 != 0) ? 0xA : 0x10;
            }
            temp_a0_12 = (s32) (((temp_a1_10 >= 0x321) ? 0x320 : temp_a1_10) * var_a0) >> 7;
            temp_a1_11 = &D_008B9148 + (temp_s1 * 0x3C);
            temp_a1_11->unk28 = temp_a0_12;
            var_a1_3 = temp_a1_11->unk2C + temp_a0_12;
        }
    }
    (&D_008B9148 + (temp_s1 * 0x3C))->unk2C = var_a1_3;
    if (var_a1_3 > 0) {
        sp28 = var_t3 + var_a1_3;
        sp38 = var_t4 + var_a1_3;
        var_t7 += var_a1_3;
    }
    var_a1_4 = 0;
    if (((u32) (sp80 - 0x17) < 2U) || (sp80 == 0x1A)) {
        var_a1_4 = 1;
    }
    var_a0_2 = 0;
    spA0 = var_a1_4;
    if (((u32) (sp84 - 0x17) < 2U) || (sp84 == 0x1A)) {
        var_a0_2 = 1;
    }
    spA4 = var_a0_2;
    M2C_ERROR(/* unknown instruction: ldl $a0, 0xa7($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $a0, 0xa0($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a0, 0x97($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a0, 0x90($sp) */);
    var_a0_3 = temp_t2 * var_t7;
    if (subroutine_arg2 < 0) {
        if (((sp90 != 0) || (sp94 != 0)) && (((spE0->unkF3 != 0) && ((spE0->unkC0->unk4 & 0x400) || (spE0->unk135 != 0))) || ((spE4->unkF3 != 0) && ((spE4->unkC0->unk4 & 0x400) || (spE4->unk135 != 0))))) {
            if (var_s3 != 0) {
                if (sp90 == 0) {
                }
            } else if (sp90 != 0) {
            }
        }
        var_a0_3 = temp_t2 * var_t7;
    }
    temp_v0_9 = ((s32) ((spE8 > -1) ? spE8 : sp100) >> 6) & 0xFFF;
    temp_a3_8 = *((temp_v0_9 * 2) + &gSinTable);
    temp_t0_3 = *(((temp_v0_9 + 0x400) * 2) + &gSinTable);
    temp_v0_10 = (spEC > -1) ? spEC : sp104;
    sp104 = temp_v0_10;
    temp_v0_11 = (temp_v0_10 >> 6) & 0xFFF;
    temp_t1_5 = *(((temp_v0_11 + 0x400) * 2) + &gSinTable);
    temp_a0_13 = (temp_s4 - (var_a0_3 / temp_s0)) + var_t5_2;
    temp_a2_9 = *((temp_v0_11 * 2) + &gSinTable);
    temp_t4 = subroutine_arg0 * temp_a2_9;
    temp_a1_12 = (s32) ((subroutine_arg2 * temp_t0_3) - (temp_a0_13 * temp_a3_8)) >> 0xC;
    temp_v1_11 = subroutine_arg0 * temp_t1_5;
    temp_t1_6 = temp_a1_12 * temp_t1_5;
    temp_t2_2 = (s32) ((subroutine_arg2 * temp_a3_8) + (temp_a0_13 * temp_t0_3)) >> 0xC;
    temp_v0_12 = temp_a1_12 * -temp_a2_9;
    if (spF4 != 0) {
        sp44 = sp14 + temp_t2_2;
        var_a2_2 = spEC;
        var_a3_4 = spF0;
        sp40 = sp10 + ((s32) (temp_v1_11 + temp_v0_12) >> 0xC);
        sp48 = sp18 + ((s32) (temp_t4 + temp_t1_6) >> 0xC);
        sp4C = spE8;
        sp50 = var_a2_2;
        sp54 = var_a3_4;
    } else {
        spC4 = (sp10 + ((s32) (temp_v1_11 + temp_v0_12) >> 0xC)) - sp40;
        temp_a3_9 = spE8 - sp4C;
        spC8 = (sp14 + temp_t2_2) - sp44;
        spCC = (sp18 + ((s32) (temp_t4 + temp_t1_6) >> 0xC)) - sp48;
        if (!(temp_a3_9 & 0x20000)) {
            var_a1_5 = temp_a3_9 & 0x3FFFF;
        } else {
            var_a1_5 = temp_a3_9 | 0xFFFC0000;
        }
        var_a3_4 = sp50;
        temp_v1_12 = spEC - var_a3_4;
        spB0 = var_a1_5;
        if (!(temp_v1_12 & 0x20000)) {
            var_a0_4 = temp_v1_12 & 0x3FFFF;
        } else {
            var_a0_4 = temp_v1_12 | 0xFFFC0000;
        }
        spB4 = var_a0_4;
        if (temp_s1 == 0) {
            if (((spC4 * spC4) + (spCC * spCC)) >= 0x3E9) {
                D_008B9148.unk24 = 1;
            } else {
                D_008B9148.unk24 = 0;
            }
        }
        var_a2_2 = sp48 + spCC;
        sp44 += spC8;
        sp40 += spC4;
        sp48 = var_a2_2;
        sp4C += spB0;
        sp50 = var_a3_4 + spB4;
        sp54 = spF0;
    }
    func_0038E460(spFC, temp_s1, var_a2_2, var_a3_4);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_0038FD90);
#endif

/* NON_MATCHING: draft, 31 lines of fdiff differ (79 instructions in the original, 78 built) */
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
        D_008B9148[i].unk30 = 0;
        D_008B9148[i].unk2C = 0;
        D_008B9148[i].unk24 = 0;
    }
    c.unk0[2] = -0x1F40;
    c.unk0[1] = -0x4B0;
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

/* NON_MATCHING: the argument copies around the first call are scheduled differently (one instruction short) */
#if 0
s32 func_00390FA8(s32 a0, s32 a1, s32 a2, s32 a3) {
    s32 v;
    s32 r = func_0038F7F8(a0, a1, &v, a2, 0);

    D_005F6C1A += D_00508120[D_005F6C18].unk2;
    func_0038FD90(a0, a1, D_008B9148[0].unkC, v, 0, r, a3, 0);
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

/* NON_MATCHING: the test of unk34 uses v0 where the original uses v1 */
#if 0
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
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003910D0);
#endif

/* NON_MATCHING: draft, 53 lines of fdiff differ (86 instructions in the original, 84 built) */
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
        func_0038C0F0();
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

/* NON_MATCHING: m2c draft, not cleaned up (1062 instructions; stack locals, struct copies and some types still raw) */
#if 0
s32 func_00391358(void *arg0, void *arg1, s32 arg2, s32 arg3) {
    s32 sp10;
    s32 sp14;
    s32 sp18;
    s32 sp20;
    s32 sp24;
    s32 sp28;
    s32 sp30;
    s32 sp38;
    s32 sp40;
    s32 sp44;
    s32 sp48;
    s32 sp4C;
    s32 sp50;
    s32 sp54;
    s32 sp78;
    u32 sp90;
    u32 sp94;
    s32 spA0;
    s32 spA4;
    s32 spB0;
    s32 spB4;
    s32 spC4;
    s32 spC8;
    s32 spCC;
    void *spE0;
    void *spE4;
    s32 spE8;
    s32 spEC;
    s32 spF0;
    s32 spF4;
    s32 spF8;
    s32 *spFC;
    s32 sp100;
    s32 sp104;
    s16 temp_a1_2;
    s16 temp_a2_9;
    s16 temp_a3_8;
    s16 temp_s1;
    s16 temp_s3;
    s16 temp_s6;
    s16 temp_t0_3;
    s16 temp_t1_5;
    s16 temp_t7;
    s16 temp_v1_4;
    s32 *temp_a0_6;
    s32 *temp_a3_7;
    s32 temp_a0;
    s32 temp_a0_10;
    s32 temp_a0_11;
    s32 temp_a0_12;
    s32 temp_a0_2;
    s32 temp_a0_3;
    s32 temp_a0_4;
    s32 temp_a0_5;
    s32 temp_a0_7;
    s32 temp_a0_8;
    s32 temp_a0_9;
    s32 temp_a1;
    s32 temp_a1_10;
    s32 temp_a1_12;
    s32 temp_a1_3;
    s32 temp_a1_4;
    s32 temp_a1_5;
    s32 temp_a1_6;
    s32 temp_a1_7;
    s32 temp_a1_8;
    s32 temp_a1_9;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_a2_3;
    s32 temp_a2_4;
    s32 temp_a2_5;
    s32 temp_a2_6;
    s32 temp_a2_7;
    s32 temp_a2_8;
    s32 temp_a3;
    s32 temp_a3_2;
    s32 temp_a3_3;
    s32 temp_a3_4;
    s32 temp_a3_5;
    s32 temp_a3_6;
    s32 temp_a3_9;
    s32 temp_fp;
    s32 temp_fp_2;
    s32 temp_s0;
    s32 temp_s2;
    s32 temp_s4;
    s32 temp_t0;
    s32 temp_t1;
    s32 temp_t1_2;
    s32 temp_t1_3;
    s32 temp_t1_4;
    s32 temp_t1_6;
    s32 temp_t2;
    s32 temp_t2_2;
    s32 temp_t4;
    s32 temp_t7_2;
    s32 temp_v0;
    s32 temp_v0_10;
    s32 temp_v0_11;
    s32 temp_v0_12;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v0_4;
    s32 temp_v0_5;
    s32 temp_v0_6;
    s32 temp_v0_7;
    s32 temp_v0_8;
    s32 temp_v0_9;
    s32 temp_v1;
    s32 temp_v1_10;
    s32 temp_v1_11;
    s32 temp_v1_12;
    s32 temp_v1_3;
    s32 temp_v1_5;
    s32 temp_v1_6;
    s32 temp_v1_7;
    s32 temp_v1_8;
    s32 temp_v1_9;
    s32 var_a0;
    s32 var_a0_2;
    s32 var_a0_3;
    s32 var_a0_4;
    s32 var_a1;
    s32 var_a1_2;
    s32 var_a1_3;
    s32 var_a1_4;
    s32 var_a1_5;
    s32 var_a2;
    s32 var_a2_2;
    s32 var_a2_3;
    s32 var_a3;
    s32 var_a3_2;
    s32 var_a3_3;
    s32 var_s3;
    s32 var_t0;
    s32 var_t1;
    s32 var_t1_2;
    s32 var_t2;
    s32 var_t3;
    s32 var_t4;
    s32 var_t5;
    s32 var_t5_2;
    s32 var_t7;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v1;
    s32 var_v1_2;
    u32 *temp_t0_2;
    void *temp_a1_11;
    void *temp_v1_2;
    spE0 = arg0;
    spE4 = arg1;
    temp_fp = M2C_ERROR(/* Read from unset register $t2 */);
    spE8 = arg2;
    temp_s2 = M2C_ERROR(/* Read from unset register $t3 */);
    spEC = arg3;
    spF0 = M2C_ERROR(/* Read from unset register $t0 */);
    spF4 = M2C_ERROR(/* Read from unset register $t1 */);
    memset(sp, 0, 0x10);
    spF8 = 0;
    temp_s0 = D_00433330.unkA4;
    sp90 = (u32) spE0->unkC0->unk24 >> 0x17;
    sp94 = (u32) spE4->unkC0->unk24 >> 0x17;
    M2C_ERROR(/* unknown instruction: ldl $a1, 0x97($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $a1, 0x90($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a1, 0x87($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a1, 0x80($sp) */);
    if ((sp80 != 0x17) && (sp84 != 0x17) && (sp80 != 0x18) && (sp84 != 0x18) && (sp80 != 0x19) && (sp84 != 0x19)) {
        if (sp80 != 0x1A) {
            if (sp84 == 0x1A) {
                goto block_8;
            }
        } else {
            goto block_9;
        }
    } else {
block_8:
block_9:
        spF8 = 1;
    }
    spFC = &sp40;
    temp_fp_2 = (temp_fp <= 0x320) ? 0x320 : temp_fp;
    func_0038E408(&sp40, temp_s2, 0x17);
    temp_a1 = -spE8;
    temp_a0 = -spEC;
    temp_v0 = ((s32) ((temp_a1 > -1) ? temp_a1 : (temp_a1 + 0x3F)) >> 6) & 0xFFF;
    temp_v1 = ((s32) ((temp_a0 > -1) ? temp_a0 : (temp_a0 + 0x3F)) >> 6) & 0xFFF;
    temp_t7 = *((temp_v0 * 2) + &gSinTable);
    temp_s3 = *((temp_v1 * 2) + &gSinTable);
    temp_s1 = *(((temp_v1 + 0x400) * 2) + &gSinTable);
    temp_t0 = (s32) (D_00433520.unk0 + D_00433520.unk10) >> 1;
    M2C_ERROR(/* unknown instruction: ldl $a3, 0x47($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $a3, 0x40($sp) */);
    M2C_ERROR(/* unknown instruction: ldl $v0, 0x4f($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $v0, 0x48($sp) */);
    M2C_ERROR(/* unknown instruction: ldl $v1, 0x57($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $v1, 0x50($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a3, 0x67($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a3, 0x60($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $v0, 0x6f($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v0, 0x68($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $v1, 0x77($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $v1, 0x70($sp) */);
    sp78 = sp58;
    temp_s6 = *(((temp_v0 + 0x400) * 2) + &gSinTable);
    temp_t1 = (s32) (D_00433520.unk8 + D_00433520.unk18) >> 1;
    if (spF4 != 0) {
        temp_v1_2 = spEC + &D_3003F;
        sp14 = sp44;
        temp_v0_2 = ((s32) (((s32) temp_v1_2 > -1) ? temp_v1_2 : temp_v1_2) >> 6) & 0xFFF;
        sp18 = temp_t1 + ((s32) (temp_fp_2 * *((temp_v0_2 * 2) + &gSinTable)) >> 0xC);
        sp10 = temp_t0 + ((s32) (temp_fp_2 * *(((temp_v0_2 + 0x400) * 2) + &gSinTable)) >> 0xC);
    } else {
        temp_v1_3 = spEC - sp50;
        temp_a3 = sp40 - temp_t0;
        temp_a2 = sp48 - temp_t1;
        var_a1 = temp_v1_3 & 0x3FFFF;
        if (temp_v1_3 & 0x20000) {
            var_a1 |= (s32) &D_FFFC003F;
        }
        temp_v0_3 = ((s32) ((var_a1 > -1) ? var_a1 : var_a1) >> 6) & 0xFFF;
        sp14 = sp44;
        temp_a1_2 = *((temp_v0_3 * 2) + &gSinTable);
        temp_v1_4 = *(((temp_v0_3 + 0x400) * 2) + &gSinTable);
        sp18 = temp_t1 + ((s32) ((temp_a1_2 * temp_a3) + (temp_v1_4 * temp_a2)) >> 0xC);
        sp10 = temp_t0 + ((s32) ((temp_v1_4 * temp_a3) - (temp_a1_2 * temp_a2)) >> 0xC);
    }
    sp100 = spE8 + 0x3F;
    var_t5 = 0;
    sp104 = spEC + 0x3F;
    do {
        temp_a3_2 = var_t5 * 0x10;
        temp_a0_2 = *(temp_a3_2 + &D_00433520) - sp10;
        temp_v0_4 = *(temp_a3_2 + &D_00433528) - sp18;
        temp_a2_2 = *(temp_a3_2 + &D_00433524) - sp14;
        temp_a1_3 = (s32) ((temp_s3 * temp_a0_2) + (temp_s1 * temp_v0_4)) >> 0xC;
        temp_t0_2 = &(&sp90)[var_t5];
        (sp + temp_a3_2)->unk20 = (s32) ((s32) ((temp_s1 * temp_a0_2) - (temp_s3 * temp_v0_4)) >> 0xC);
        var_t5 += 1;
        (sp + temp_a3_2)->unk28 = (s32) ((s32) ((temp_s6 * temp_a1_3) - (temp_t7 * temp_a2_2)) >> 0xC);
        *(&sp24 + temp_a3_2) = (s32) ((temp_t7 * temp_a1_3) + (temp_s6 * temp_a2_2)) >> 0xC;
        *temp_t0_2 = (u32) temp_a1_3;
    } while (var_t5 < 2);
    temp_a3_3 = -0x1C2 - sp14;
    temp_t1_2 = (s32) (sp90 + sp94) >> 1;
    temp_a0_3 = D_00433330.unkA4 * -0xCE4;
    temp_v1_5 = (s32) ((temp_a0_3 > -1) ? temp_a0_3 : (temp_a0_3 + 0xFF)) >> 8;
    temp_t7_2 = (s32) ((temp_s6 * temp_t1_2) - (temp_t7 * temp_a3_3)) >> 0xC;
    temp_v0_5 = temp_fp_2 - temp_t7_2;
    temp_v0_6 = (temp_v0_5 < temp_v1_5) ? temp_v1_5 : temp_v0_5;
    temp_s4 = (s32) ((temp_t7 * temp_t1_2) + (temp_s6 * temp_a3_3)) >> 0xC;
    temp_t1_3 = sp28 + temp_v0_6;
    sp38 += temp_v0_6;
    var_t7 = temp_t7_2 + temp_v0_6;
    sp28 = temp_t1_3;
    if (temp_t1_3 == 0) {
        sp28 = 1;
    }
    var_t3 = sp38;
    if (var_t3 == 0) {
        var_t3 = 1;
        sp38 = 1;
    }
    temp_a0_4 = sp20 - 0x96;
    if (sp20 < sp30) {
        temp_a1_4 = sp30 + 0x96;
        var_t4 = sp28;
        var_a3 = 0;
        sp20 = temp_a0_4;
        var_s3 = 1;
        sp30 = temp_a1_4;
        var_v1 = (s32) (temp_a1_4 * temp_s0) / var_t3;
        var_t2 = (s32) (temp_a0_4 * temp_s0) / var_t4;
    } else {
        temp_a0_5 = sp30 - 0x96;
        var_a3 = 1;
        temp_a1_5 = sp20 + 0x96;
        sp30 = temp_a0_5;
        sp20 = temp_a1_5;
        var_s3 = 0;
        var_t4 = sp28;
        var_t2 = (s32) (temp_a0_5 * temp_s0) / var_t3;
        var_v1 = (s32) (temp_a1_5 * temp_s0) / var_t4;
    }
    temp_v1_6 = var_v1 - var_t2;
    if (temp_v1_6 < 0x189) {
        D_005F6180 = 0;
    } else {
        D_005F6180 = 1;
    }
    if (temp_v1_6 >= 0x189) {
        temp_v1_7 = var_s3 * 0x10;
        temp_a3_4 = var_a3 * 0x10;
        temp_t1_4 = *(&sp28 + temp_a3_4);
        var_a2 = temp_t1_4 - ((s32) ((temp_s0 * (*(&sp20 + temp_v1_7) - *(&sp20 + temp_a3_4))) - ((*(&sp28 + temp_v1_7) - temp_t1_4) * 0xC4)) / 392);
        if (spF4 != 0) {
            (&D_008B9148 + (temp_s2 * 0x3C))->unk30 = var_a2;
        } else {
            temp_a0_6 = (temp_s2 * 0x3C) + (&D_008B9148 + 0x30);
            temp_a3_5 = *temp_a0_6;
            if (temp_a3_5 < var_a2) {
                var_a2 = temp_a3_5 + ((s32) ((var_a2 - temp_a3_5) * 4) >> 7);
            } else {
                var_a2 = temp_a3_5 - ((s32) ((temp_a3_5 - var_a2) * 0x10) >> 7);
            }
            *temp_a0_6 = var_a2;
        }
        temp_a1_6 = var_t4 - var_a2;
        sp28 = temp_a1_6;
        sp38 = var_t3 - var_a2;
        if (temp_a1_6 == 0) {
            sp28 = 1;
        }
        var_t3 = sp38;
        var_t7 -= var_a2;
        if (var_t3 == 0) {
            var_t3 = 1;
            sp38 = 1;
        }
        var_t4 = sp28;
    } else {
        if (spF4 != 0) {
            goto block_46;
        }
        temp_a2_3 = var_s3 * 0x10;
        temp_a3_6 = var_a3 * 0x10;
        if ((((s32) (*(&sp28 + temp_a2_3) * 0x44) / temp_s0) - (*(&sp20 + temp_a2_3) - *(&sp20 + temp_a3_6))) < ((s32) (*(&sp28 + temp_a3_6) * -0x44) / temp_s0)) {
block_46:
            if (spF4 == 0) {
                temp_a2_4 = (&D_008B9148 + (temp_s2 * 0x3C))->unk30;
                var_t1 = 0;
                if (temp_a2_4 < 0) {
                    temp_a0_7 = -temp_a2_4;
                    var_t1 = temp_a2_4 + ((s32) ((temp_a0_7 > -1) ? temp_a0_7 : (temp_a0_7 + 0x1F)) >> 5);
                } else if (spF4 < temp_a2_4) {
                    var_t1 = temp_a2_4 - ((s32) ((temp_a2_4 > -1) ? temp_a2_4 : (temp_a2_4 + 7)) >> 3);
                }
                goto block_61;
            }
            (&D_008B9148 + (temp_s2 * 0x3C))->unk30 = 0;
        } else {
            if (var_t2 < -0x44) {
                var_t1 = 0;
                var_a3_2 = (&D_008B9148 + (temp_s2 * 0x3C))->unk30;
                if (var_a3_2 >= 0) {
                    var_v0 = spF4 < var_a3_2;
                    goto block_59;
                }
                goto block_57;
                goto block_61;
            }
            if (var_v1 >= 0x45) {
                var_t1 = 0;
                var_a3_2 = (&D_008B9148 + (temp_s2 * 0x3C))->unk30;
                if (var_a3_2 < 0) {
block_57:
                    temp_a0_8 = -var_a3_2;
                    var_t1 = var_a3_2 + ((s32) ((temp_a0_8 > -1) ? temp_a0_8 : (temp_a0_8 + 0x1F)) >> 5);
                } else {
                    var_v0 = spF4 < var_a3_2;
block_59:
                    if (var_v0 != 0) {
                        var_t1 = var_a3_2 - ((s32) ((var_a3_2 > -1) ? var_a3_2 : (var_a3_2 + 7)) >> 3);
                    }
                }
block_61:
                temp_a1_7 = var_t4 - var_t1;
                temp_a2_5 = var_t3 - var_t1;
                var_t7 -= var_t1;
                var_t4 = temp_a1_7;
                var_t3 = temp_a2_5;
                (&D_008B9148 + (temp_s2 * 0x3C))->unk30 = var_t1;
                sp28 = temp_a1_7;
                sp38 = temp_a2_5;
            } else {
                temp_a2_6 = (&D_008B9148 + (temp_s2 * 0x3C))->unk30;
                var_t1_2 = 0;
                if (temp_a2_6 < 0) {
                    temp_a0_9 = -temp_a2_6;
                    var_t1_2 = temp_a2_6 + ((s32) ((temp_a0_9 > -1) ? temp_a0_9 : (temp_a0_9 + 0x1F)) >> 5);
                } else if (spF4 < temp_a2_6) {
                    var_t1_2 = temp_a2_6 - ((s32) ((temp_a2_6 > -1) ? temp_a2_6 : (temp_a2_6 + 7)) >> 3);
                }
                temp_a1_8 = var_t4 - var_t1_2;
                temp_a2_7 = var_t3 - var_t1_2;
                sp28 = temp_a1_8;
                var_t7 -= var_t1_2;
                sp38 = temp_a2_7;
                (&D_008B9148 + (temp_s2 * 0x3C))->unk30 = var_t1_2;
                var_t4 = temp_a1_8;
                var_t3 = temp_a2_7;
            }
        }
    }
    var_t0 = 0x8C;
    var_t5_2 = -0x12C;
    if (var_t4 == 0) {
        var_t4 = 1;
        sp28 = 1;
    }
    if (var_t3 == 0) {
        var_t3 = 1;
        sp38 = 1;
    }
    var_a1_2 = sp34;
    if (((s32) (sp24 * temp_s0) / var_t4) < ((s32) (var_a1_2 * temp_s0) / var_t3)) {
        var_a1_2 = sp24;
        var_a2_2 = var_t4;
        var_v0_2 = D_00433520.unk4;
    } else {
        var_a2_2 = var_t3;
        var_v0_2 = D_00433520.unk14;
    }
    temp_a0_10 = (s32) (var_v0_2 * temp_s6) >> 0xC;
    if (temp_s2 == 1) {
        var_t0 = (spE8 <= -1) ? 0xB4 : 0x8C;
        if (spE8 > 0) {
            if (temp_a0_10 >= -0x3E8) {
                var_t5_2 = (s32) ((temp_a0_10 + 0x3E8) << 9) / 640;
            }
            if (spF4 == 0) {
                var_v0_3 = var_t5_2 - D_005F614C;
                if (D_005F614C < var_t5_2) {
                    var_v1_2 = D_005F614C + 0x1C;
                    goto block_83;
                }
                var_v0_3 = D_005F614C - var_t5_2;
                if (var_t5_2 < D_005F614C) {
                    var_v1_2 = D_005F614C - 0x1C;
block_83:
                    var_t5_2 = (var_v0_3 >= 0x1D) ? var_v1_2 : var_t5_2;
                }
            }
            D_005F614C = var_t5_2;
        }
    }
    temp_t2 = (s32) (temp_s6 * (var_t0 - ((s32) (temp_s0 * 0x1C2) / temp_fp_2))) >> 0xC;
    temp_v0_7 = temp_t2 + 0x9C;
    temp_a1_9 = ((s32) (((var_a2_2 - var_t7) * -0x9C) - (temp_s0 * (var_a1_2 - temp_s4))) / (s32) ((temp_v0_7 == 0) ? 1 : temp_v0_7)) - var_t7;
    var_a1_3 = (temp_a1_9 <= -1) ? 0 : temp_a1_9;
    if (spF4 != 0) {
        var_a1_3 = 0;
        (&D_008B9148 + (temp_s2 * 0x3C))->unk28 = 0;
    } else {
        temp_a2_8 = temp_s2 * 0x3C;
        temp_a3_7 = temp_a2_8 + (&D_008B9148 + 0x2C);
        temp_v1_8 = *temp_a3_7;
        if (var_a1_3 < temp_v1_8) {
            temp_v1_9 = temp_v1_8 - var_a1_3;
            temp_v1_10 = (temp_v1_9 >= 0x4B1) ? 0x4B0 : temp_v1_9;
            temp_v0_8 = ((s32) (temp_v1_10 * 0x78) >> 7) - temp_v1_10;
            (&D_008B9148 + temp_a2_8)->unk28 = temp_v0_8;
            var_a1_3 = *temp_a3_7 + temp_v0_8;
        } else if (temp_v1_8 < var_a1_3) {
            temp_a1_10 = var_a1_3 - temp_v1_8;
            if ((sp80 == 0x19) || (sp84 == 0x19)) {
                var_a0 = 4;
            } else {
                var_a0 = (spF8 != 0) ? 0xA : 0x10;
            }
            temp_a0_11 = (s32) (((temp_a1_10 >= 0x321) ? 0x320 : temp_a1_10) * var_a0) >> 7;
            temp_a1_11 = &D_008B9148 + (temp_s2 * 0x3C);
            temp_a1_11->unk28 = temp_a0_11;
            var_a1_3 = temp_a1_11->unk2C + temp_a0_11;
        }
    }
    (&D_008B9148 + (temp_s2 * 0x3C))->unk2C = var_a1_3;
    if (var_a1_3 > 0) {
        sp28 = var_t4 + var_a1_3;
        sp38 = var_t3 + var_a1_3;
        var_t7 += var_a1_3;
    }
    var_a1_4 = 0;
    if (((u32) (sp80 - 0x17) < 2U) || (sp80 == 0x1A)) {
        var_a1_4 = 1;
    }
    var_a0_2 = 0;
    spA0 = var_a1_4;
    if (((u32) (sp84 - 0x17) < 2U) || (sp84 == 0x1A)) {
        var_a0_2 = 1;
    }
    spA4 = var_a0_2;
    M2C_ERROR(/* unknown instruction: ldl $a0, 0xa7($sp) */);
    M2C_ERROR(/* unknown instruction: ldr $a0, 0xa0($sp) */);
    M2C_ERROR(/* unknown instruction: sdl $a0, 0x97($sp) */);
    M2C_ERROR(/* unknown instruction: sdr $a0, 0x90($sp) */);
    var_a0_3 = temp_t2 * var_t7;
    if (subroutine_arg2 < 0) {
        if (((sp90 != 0) || (sp94 != 0)) && (((spE0->unkF3 != 0) && ((spE0->unkC0->unk4 & 0x400) || (spE0->unk135 != 0))) || ((spE4->unkF3 != 0) && ((spE4->unkC0->unk4 & 0x400) || (spE4->unk135 != 0))))) {
            if (var_s3 != 0) {
                if (sp90 == 0) {
                }
            } else if (sp90 != 0) {
            }
        }
        var_a0_3 = temp_t2 * var_t7;
    }
    temp_v0_9 = ((s32) ((spE8 > -1) ? spE8 : sp100) >> 6) & 0xFFF;
    temp_a3_8 = *((temp_v0_9 * 2) + &gSinTable);
    temp_t0_3 = *(((temp_v0_9 + 0x400) * 2) + &gSinTable);
    temp_v0_10 = (spEC > -1) ? spEC : sp104;
    sp104 = temp_v0_10;
    temp_v0_11 = (temp_v0_10 >> 6) & 0xFFF;
    temp_t1_5 = *(((temp_v0_11 + 0x400) * 2) + &gSinTable);
    temp_a0_12 = (temp_s4 - (var_a0_3 / temp_s0)) + var_t5_2;
    temp_a2_9 = *((temp_v0_11 * 2) + &gSinTable);
    temp_t4 = subroutine_arg0 * temp_a2_9;
    temp_a1_12 = (s32) ((subroutine_arg2 * temp_t0_3) - (temp_a0_12 * temp_a3_8)) >> 0xC;
    temp_v1_11 = subroutine_arg0 * temp_t1_5;
    temp_t1_6 = temp_a1_12 * temp_t1_5;
    temp_t2_2 = (s32) ((subroutine_arg2 * temp_a3_8) + (temp_a0_12 * temp_t0_3)) >> 0xC;
    temp_v0_12 = temp_a1_12 * -temp_a2_9;
    if (spF4 != 0) {
        sp44 = sp14 + temp_t2_2;
        var_a2_3 = spEC;
        var_a3_3 = spF0;
        sp40 = sp10 + ((s32) (temp_v1_11 + temp_v0_12) >> 0xC);
        sp48 = sp18 + ((s32) (temp_t4 + temp_t1_6) >> 0xC);
        sp4C = spE8;
        sp50 = var_a2_3;
        sp54 = var_a3_3;
    } else {
        spC4 = (sp10 + ((s32) (temp_v1_11 + temp_v0_12) >> 0xC)) - sp40;
        temp_a3_9 = spE8 - sp4C;
        spC8 = (sp14 + temp_t2_2) - sp44;
        spCC = (sp18 + ((s32) (temp_t4 + temp_t1_6) >> 0xC)) - sp48;
        if (!(temp_a3_9 & 0x20000)) {
            var_a1_5 = temp_a3_9 & 0x3FFFF;
        } else {
            var_a1_5 = temp_a3_9 | 0xFFFC0000;
        }
        var_a3_3 = sp50;
        temp_v1_12 = spEC - var_a3_3;
        spB0 = var_a1_5;
        if (!(temp_v1_12 & 0x20000)) {
            var_a0_4 = temp_v1_12 & 0x3FFFF;
        } else {
            var_a0_4 = temp_v1_12 | 0xFFFC0000;
        }
        spB4 = var_a0_4;
        if (temp_s2 == 0) {
            if (((spC4 * spC4) + (spCC * spCC)) >= 0x3E9) {
                D_008B9148.unk24 = 1;
            } else {
                D_008B9148.unk24 = 0;
            }
        }
        var_a2_3 = sp48 + spCC;
        sp44 += spC8;
        sp40 += spC4;
        sp48 = var_a2_3;
        sp4C += spB0;
        sp50 = var_a3_3 + spB4;
        sp54 = spF0;
    }
    func_0038E460(spFC, temp_s2, var_a2_3, var_a3_3);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00391358);
#endif

INCLUDE_RODATA("asm/nonmatchings/stage", D_00508130);
INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_005088D0);

/* NON_MATCHING: same size, the register allocation and the order of the set-up differ (95 lines) */
#if 0
u8 func_003923F0(s32 n, Unk8B92D0 *out, s32 a2) {
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
INCLUDE_ASM("asm/nonmatchings/stage", func_003923F0);
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

/* NON_MATCHING: the loads of the two positions and the result test (`bgtz` / `slti` in the original) are scheduled differently */
#if 0
s32 func_003927F0(Fighter *a, Fighter *b) {
    s16 w[2];
    s16 u[2];
    s16 v[2];
    s32 r;

    w[0] = D_008B92D0.unk0[0];
    w[1] = D_008B92D0.unk0[2];
    u[0] = a->unk51C;
    u[1] = b->unk51C;
    v[0] = (a->unk51C + b->unk51C) / 2;
    v[1] = (a->unk524 + b->unk524) / 2;
    r = func_0039BDA8(u, v, w);
    if (r > 0) {
        return 0;
    }
    return r < 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003927F0);
#endif

/* NON_MATCHING: the result is built in v1 and copied to v0 at the end; the original keeps it in v0 and 0x9E in v1 */
#if 0
s32 func_00392888(s32 a, s32 b) {
    s32 r;

    if (a < 0x68) {
        return 1;
    }
    r = 0;
    if (b == 0x9E) {
        r = 2;
        if (a == 0x9F) {
            r = 0;
        }
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00392888);
#endif

INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_005088F0);

/* NON_MATCHING: draft, 95 lines of fdiff differ (159 instructions in the original, 151 built) */
#if 0
s32 func_003928B8(s32 n, Fighter *f, Fighter *o) {
    s32 r = 0;
    s32 a;

    if (n != 0) {
        D_00433330.unk84 = 0;
        a = 0;
        switch (func_003923F0(n, NULL, 0)) {
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
        func_003923F0(0, &D_008B92D0, 0);
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
                func_003923F0(-D_00433330.unk58, NULL, 0);
                func_003923F0(0, &D_008B92D0, 0);
                D_00433330.unk84 = 0;
                break;
            }
        }
    }
    if (D_00433330.unk58 < 0) {
        r = func_00392B68(f, o);
    } else {
        func_0038C0F0();
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003928B8);
#endif

void func_00392B38(s32 a0) {
    func_003923F0(a0, NULL, 0);
    func_003923F0(0, &D_008B92D0, 0);
}

/* NON_MATCHING: draft, 155 lines of fdiff differ (165 instructions in the original, 154 built) */
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
    func_003923F0(0, &D_008B92D0, 0);
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00392B68);
#endif

/* NON_MATCHING: draft, 36 lines of fdiff differ (95 instructions in the original, 94 built) */
#if 0
void func_00392E00(s32 id) {
    s32 *p = func_003B8790();
    s32 i;
    s32 *q;

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
            q = (s32 *)((u8 *)func_003B8790() + *p);
            D_008BC4F8[i] = q + 1;
            D_008BC578[i] = *q;
            p++;
        }
    } else {
        q = func_003B8790();
        D_008BC4F8[0] = q + 1;
        D_008BC578[0] = *q;
    }
    D_005F6184 = id;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00392E00);
#endif

/* NON_MATCHING: m2c draft, not cleaned up (156 instructions; stack locals, struct copies and some types still raw) (float code: its .lit4 constants would need a .lit4 subsegment for this file) */
#if 0
s32 func_00392F80(s32 arg0, s32 arg1) {
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f21;
    f32 temp_f22;
    f32 temp_f25;
    f32 temp_f26;
    f32 temp_f3;
    f32 var_f22;
    f32 var_f23;
    f32 var_f24;
    s32 temp_a0;
    s32 temp_a1;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_v1;
    s32 var_a3;
    s32 var_s0;
    void *temp_v0;
    void *temp_v0_2;
    void *temp_v0_3;
    void *temp_v0_4;
    void *temp_v0_5;
    var_a3 = arg1;
    var_s0 = 0;
    if (var_a3 < 0) {
        return *((arg0 * 4) + &D_008BC578);
    }
    temp_a2 = (arg0 <= -1) ? 0 : arg0;
    temp_a2_2 = (temp_a2 >= D_005F6C24) ? (D_005F6C24 - 1) : temp_a2;
    temp_a0 = temp_a2_2 * 4;
    temp_v1 = *(temp_a0 + &D_008BC578);
    if (var_a3 >= temp_v1) {
        var_a3 = temp_v1 - 1;
        var_s0 = 1;
    }
    temp_a1 = var_a3 * 0x1C;
    temp_v0 = *(temp_a0 + &D_008BC4F8) + temp_a1;
    temp_v0_2 = temp_v0 + 4;
    temp_v0_3 = temp_v0_2 + 4;
    temp_v0_4 = temp_v0_3 + 4;
    temp_v0_5 = temp_v0_4 + 4;
    temp_f26 = (temp_v0_5 + 4)->unk4;
    temp_f25 = temp_v0->unk0 + D_008BC5F8.unkC + D_008BC5F8.unkC;
    var_f24 = temp_v0->unk4 + D_008BC5F8.unk10 + D_008BC5F8.unk10;
    var_f23 = temp_v0_2->unk4 + D_008BC5F8.unk14 + D_008BC5F8.unk14;
    temp_f21 = temp_v0_3->unk4 + D_008BC5F8.unk18 + D_008BC5F8.unk18;
    var_f22 = temp_v0_4->unk4 + D_008BC5F8.unk1C + D_008BC5F8.unk1C;
    temp_f20 = temp_v0_5->unk4 + D_008BC5F8.unk20 + D_008BC5F8.unk20;
    if (func_003BB4D8(&D_008BC5F8, temp_a1, temp_a2_2, var_a3) != 0) {
        temp_f3 = var_f24 - var_f22;
        var_f22 -= 8.0f;
        var_f23 = temp_f20 + ((var_f23 - temp_f20) * D_005F517C);
        var_f24 = var_f22 + (temp_f3 * D_005F517C);
    }
    M2C_ERROR(/* unknown instruction: c1 0xd0344 */);
    temp_f22 = atan2f(var_f22 - var_f24);
    temp_f20_2 = atan2f(temp_f20 - var_f23) - D_005F5184;
    D_00433330.unkAC = 0x2710;
    D_00433330.unkA8 = 1;
    func_0038DA28(M2C_ERROR(/* Read from unset register $f2 */), 1);
    D_00415808.unk8 = 0;
    D_00415808.unk1C = 4096.0f;
    D_00415808.unk14 = 4096.0f;
    D_00415808.unk18 = 4096.0f;
    D_00415808.unk4 = 4096.0f;
    D_00415808.unk0 = 4096.0f;
    return var_s0;
}
#else
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

/* NON_MATCHING: m2c draft, not cleaned up (120 instructions; stack locals, struct copies and some types still raw) (float code: its .lit4 constants would need a .lit4 subsegment for this file) */
#if 0
s32 func_003932F0(f32 *arg0, f32 *arg1, f32 *arg2, f32 *arg3) {
    f32 temp_f0;
    f32 temp_f20;
    f32 temp_f21;
    f32 temp_f22;
    f32 temp_f23;
    f32 temp_f24;
    f32 temp_f25;
    s32 temp_a1;
    s32 temp_v0_5;
    void *temp_v0;
    void *temp_v0_2;
    void *temp_v0_3;
    void *temp_v0_4;
    if (D_008BC5F8.unk0 >= 0) {
        temp_a1 = D_008BC5F8.unk8 * 0x1C;
        temp_v0 = *((D_008BC5F8.unk0 * 4) + &D_008BC4F8) + temp_a1;
        temp_f25 = temp_v0->unk0;
        temp_v0_2 = temp_v0 + 4;
        temp_f24 = temp_v0->unk4;
        temp_v0_3 = temp_v0_2 + 4;
        temp_f23 = temp_v0_2->unk4;
        temp_v0_4 = temp_v0_3 + 4;
        temp_f22 = temp_v0_3->unk4 - temp_f25;
        temp_f21 = (temp_v0_4 + 4)->unk4 - temp_f23;
        M2C_ERROR(/* unknown instruction: c1 0xd0344 */);
        temp_f20 = atan2f(temp_v0_4->unk4 - temp_f24, &D_008BC4F8, temp_a1);
        temp_f0 = (-(atan2f(temp_f21) - D_005F518C) * 65536.0f) / D_005F5190;
        *arg3 = D_005F518C;
        *M2C_ERROR(/* Read from unset register $t0 */) = D_005F518C;
        *M2C_ERROR(/* Read from unset register $t1 */) = 0;
        *arg0 = temp_f25;
        *arg1 = -temp_f24;
        *arg2 = -temp_f23;
        *M2C_ERROR(/* Read from unset register $t2 */) = temp_f0;
        temp_v0_5 = D_008BC5F8.unk8 + 1;
        D_008BC5F8.unk8 = temp_v0_5;
        if (temp_v0_5 >= D_008BC5F8.unk4) {
            D_008BC5F8.unk0 = -1;
        }
    }
    return 0;
}
#else
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

/* NON_MATCHING: m2c draft, not cleaned up (110 instructions; stack locals, struct copies and some types still raw) (float code: its .lit4 constants would need a .lit4 subsegment for this file) */
#if 0
s32 func_00393550(void) {
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f21;
    f32 temp_f22;
    s32 temp_a0;
    s32 temp_a1;
    s32 temp_v0_5;
    void *temp_v0;
    void *temp_v0_2;
    void *temp_v0_3;
    void *temp_v0_4;
    if (D_008BC620.unk0 >= 0) {
        temp_a1 = D_008BC620.unk8 * 0x1C;
        temp_v0 = *((D_008BC620.unk0 * 4) + &D_008BC4F8) + temp_a1;
        temp_v0_2 = temp_v0 + 4;
        temp_v0_3 = temp_v0_2 + 4;
        temp_v0_4 = temp_v0_3 + 4;
        temp_f21 = temp_v0_3->unk4 - temp_v0->unk0;
        temp_f20 = (temp_v0_4 + 4)->unk4 - temp_v0_2->unk4;
        M2C_ERROR(/* unknown instruction: c1 0xd0344 */);
        temp_f22 = atan2f(temp_v0_4->unk4 - temp_v0->unk4, &D_008BC4F8, temp_a1);
        temp_f20_2 = atan2f(temp_f20) - D_005F5194;
        D_00433330.unkAC = 0x2710;
        D_00433330.unkA8 = 1;
        func_0038DA28(M2C_ERROR(/* Read from unset register $f2 */), 1);
        temp_v0_5 = D_008BC620.unk8 + 1;
        D_00415808.unk8 = 0;
        temp_a0 = temp_v0_5 < D_008BC620.unk4;
        D_008BC620.unk8 = temp_v0_5;
        D_00415808.unk1C = 4096.0f;
        D_00415808.unk14 = 4096.0f;
        D_00415808.unk18 = 4096.0f;
        D_00415808.unk4 = 4096.0f;
        D_00415808.unk0 = 4096.0f;
        if (temp_a0 == 0) {
            D_008BC620.unk0 = -1;
            func_0038DBC0(temp_a0, &D_00415808);
            return 1;
        }
    }
    return 0;
}
#else
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
            func_0038DA28(D_008BC620.unkC);
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
 * with func_00392E00), -1 for the others; it also steps the stage's counter D_00433560 through 0-3 [V code]. */
s32 func_00393838(u32 stage) {
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

/* NON_MATCHING: draft, 71 lines of fdiff differ (125 instructions in the original, 115 built) */
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

/* NON_MATCHING: draft, 32 lines of fdiff differ (88 instructions in the original, 90 built) */
#if 0
/* Picks the camera reaction of a move kind (D_00434C78[kind], weighted list): random by rand() & 0xFFF, or by
 * func_003800D8 (the CPU's generator) with the list's own range on stages of camera style 1 / 2 (D_00433330.unkB4,
 * from D_00507FE0) [V code]. */
s32 func_00393A90(u32 kind, s32 side, s32 *idx) {
    Unk434C78Entry *e;
    s32 r = 0;
    s32 i;
    s32 fl;
    s16 v;

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
    while (e->unk2 < r) {
        e++;
        i++;
    }
    *idx = i;
    fl = e->unk0 & 3;
    v = e->unk4[side != 0];
    D_005F6194 = kind;
    D_005F6190 = v;
    D_00433330.unkA0 = fl != 1 ? 0 : (side != 0 ? fl : 0);
    return v;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00393A90);
#endif

/* NON_MATCHING: the selects of `on` and the loads of the entry come out in another order (21 lines) */
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
    on = flags;
    if (D_00433330.unk34 == 0) {
        on = 0;
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

/* NON_MATCHING: draft, 43 lines of fdiff differ (86 instructions in the original, 85 built) */
#if 0
s32 func_00393E08(Fighter *a, Fighter *b, s32 which, s32 side) {
    s32 idx;
    s16 n = 0;
    Fighter *f = which != 0 ? b : a;
    s32 k;

    if (D_00433330.unk54 >= 0) {
        func_0038C0F0();
        return -1;
    }
    k = func_00393A90(f->unkC0->unk24 >> 23, side, &idx);
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

/* NON_MATCHING: draft, 45 lines of fdiff differ (83 instructions in the original, 82 built) */
#if 0
s32 func_00393F60(Fighter *f, Fighter *o, s32 side) {
    s32 idx;
    s16 n = 0;
    s32 k;

    if (D_00433330.unk54 < 0) {
        func_0038C0F0();
        return -1;
    }
    k = func_00393A90(f->unkC0->unk24 >> 23, side, &idx);
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
        D_00433330.unk48 = n < 2;
        return n;
    }
    return -1;
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

/* NON_MATCHING */
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

/* NON_MATCHING: the `& 0xFFF` of the random value is scheduled one instruction earlier */
#if 0
s32 func_00394300(s32 n, s32 k) {
    s32 r = rand() & 0xFFF;
    Unk5095C0Entry *e = D_005095C0[n - 0x12C].unk0;
    s16 *p;

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
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00394300);
#endif

INCLUDE_RODATA("asm/nonmatchings/stage", D_00509700);
INCLUDE_RODATA("asm/nonmatchings/stage", D_00509820);

/* NON_MATCHING: draft, 97 lines of fdiff differ (154 instructions in the original, 155 built) */
#if 0
s32 func_003943A8(Fighter *a, Fighter *b) {
    Fighter *f = a;
    Fighter *o = b;
    s32 kind = 0;
    const s32 *r = NULL;
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
            (&D_00433330.unk7C)[o->unk20] = (s32)o->unkC0;
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

/* NON_MATCHING: m2c draft, not cleaned up (858 instructions; stack locals, struct copies and some types still raw) */
#if 0
s32 func_00394648(void *arg0, void *arg1, s32 arg2, s32 arg3) {
    s32 sp20;
    s32 sp24;
    s32 sp28;
    s32 sp30;
    s32 sp38;
    s16 sp40;
    s16 sp42;
    s16 sp44;
    ? sp50;
    s32 sp70;
    s32 sp74;
    s32 sp78;
    s32 sp80;
    s32 sp84;
    s32 sp90;
    s32 sp94;
    s32 sp98;
    s32 spA0;
    s32 spA4;
    s32 spA8;
    s32 spB0;
    s32 spB4;
    s32 spB8;
    s32 spBC;
    s128 spC0;
    s16 temp_a0_2;
    s16 temp_a3_3;
    s16 temp_fp;
    s16 temp_s0_3;
    s16 temp_s1;
    s16 temp_s3;
    s16 temp_s4_2;
    s16 temp_s4_3;
    s16 temp_s5;
    s16 temp_s5_2;
    s16 temp_s6_2;
    s16 temp_s6_3;
    s16 temp_s7;
    s16 temp_s7_2;
    s16 temp_t7;
    s16 temp_t7_2;
    s16 temp_v1;
    s16 var_t3;
    s16 var_v1;
    s32 *var_a0_2;
    s32 *var_s0_3;
    s32 temp_a0;
    s32 temp_a0_3;
    s32 temp_a0_4;
    s32 temp_a0_5;
    s32 temp_a0_6;
    s32 temp_a0_7;
    s32 temp_a1_2;
    s32 temp_a1_3;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_a2_3;
    s32 temp_a2_4;
    s32 temp_a2_5;
    s32 temp_a2_6;
    s32 temp_a2_7;
    s32 temp_a2_8;
    s32 temp_a2_9;
    s32 temp_a3;
    s32 temp_a3_2;
    s32 temp_a3_4;
    s32 temp_lo1;
    s32 temp_lo1_2;
    s32 temp_lo1_3;
    s32 temp_lo1_4;
    s32 temp_lo;
    s32 temp_lo_2;
    s32 temp_lo_3;
    s32 temp_lo_4;
    s32 temp_lo_5;
    s32 temp_s0;
    s32 temp_s0_2;
    s32 temp_s6;
    s32 temp_t0;
    s32 temp_t0_2;
    s32 temp_t0_3;
    s32 temp_t0_4;
    s32 temp_t1;
    s32 temp_t1_2;
    s32 temp_t1_3;
    s32 temp_t1_4;
    s32 temp_t1_5;
    s32 temp_t1_6;
    s32 temp_t2;
    s32 temp_t2_2;
    s32 temp_t2_3;
    s32 temp_t2_4;
    s32 temp_t3;
    s32 temp_t4;
    s32 temp_t5;
    s32 temp_t6_2;
    s32 temp_t6_3;
    s32 temp_v0;
    s32 temp_v0_10;
    s32 temp_v0_11;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v0_5;
    s32 temp_v0_6;
    s32 temp_v1_2;
    s32 temp_v1_3;
    s32 temp_v1_4;
    s32 temp_v1_5;
    s32 temp_v1_6;
    s32 temp_v1_7;
    s32 temp_v1_8;
    s32 temp_v1_9;
    s32 var_s0;
    s32 var_s0_2;
    s32 var_s0_4;
    s32 var_s0_5;
    s32 var_s0_6;
    s32 var_s0_7;
    s32 var_s1;
    s32 var_s1_2;
    s32 var_t1;
    s32 var_t7;
    s32 var_t7_2;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v0_4;
    s32 var_v0_5;
    s32 var_v0_6;
    s32 var_v0_7;
    u32 temp_v0_4;
    u32 temp_v0_7;
    u32 temp_v0_8;
    u32 temp_v0_9;
    void *temp_a1;
    void *temp_s2;
    void *temp_s2_2;
    void *temp_s3_2;
    void *temp_s4;
    void *temp_t6;
    void *var_a0;
    var_a0 = arg0;
    temp_t6 = D_005F6C28;
    temp_s2 = var_a0;
    temp_v1 = temp_t6->unk0;
    temp_s4 = (M2C_ERROR(/* Read from unset register $t0 */) != 0) ? arg1 : temp_s2;
    if ((u32) temp_v1 < 0x12U) {
        var_a0 = *((temp_v1 * 4) + &jtbl_00509BC0);
        switch (temp_v1) {
        case 0:
            if (arg2 != 0) {
                func_003800D8(0xE3);
                temp_s0 = func_00391058(temp_s2, arg1);
                var_s1 = (D_005F6C28 + ((rand() & 3) * 2))->unk10 << 6;
                temp_a0 = (D_005F6C28 + ((rand() & 3) * 2))->unk18 << 6;
                if ((D_00433330.unkB4 != 0) && (D_00433330.unkB4 >= 0)) {
                    var_s1 = (D_00433330.unkB4 <= 2) ? 0 : var_s1;
                }
                spB0 = var_s1;
                spB4 = temp_a0;
                if (D_00433330.unk34 == 0) {
                    var_s0 = temp_s0 + spB4;
                } else {
                    var_s0 = temp_s0 - spB4;
                }
                var_t7 = spB0;
                D_00433330.unk6C = 0;
                D_00433330.unk68 = var_s0;
                D_00433330.unk64 = var_t7;
            } else {
                var_t7 = D_00433330.unk64;
                var_s0 = D_00433330.unk68;
            }
            func_0038FD90(temp_s2, arg1, var_t7, var_s0);
            break;
        case 1:
            if (arg2 != 0) {
                func_003800D8(0xE3);
                temp_s0_2 = func_00391058(temp_s2, arg1);
                var_s1_2 = (D_005F6C28 + ((rand() & 3) * 2))->unk10 << 6;
                temp_a3 = (D_005F6C28 + ((rand() & 3) * 2))->unk18 << 6;
                if ((D_00433330.unkB4 != 0) && (D_00433330.unkB4 >= 0)) {
                    var_s1_2 = (D_00433330.unkB4 <= 2) ? 0 : var_s1_2;
                }
                spB8 = var_s1_2;
                spBC = temp_a3;
                if (D_00433330.unk34 == 0) {
                    var_s0_2 = temp_s0_2 + spBC;
                } else {
                    var_s0_2 = temp_s0_2 - spBC;
                }
                var_t7_2 = spB8;
                D_00433330.unk6C = 0;
                D_00433330.unk68 = var_s0_2;
                D_00433330.unk64 = var_t7_2;
            } else {
                var_s0_2 = D_00433330.unk68;
                var_t7_2 = D_00433330.unk64;
            }
            if (D_00433330.unk50 == 0) {
                M2C_BREAK(0);
            }
            temp_v1_2 = (((s32) ((var_s0_2 > -1) ? var_s0_2 : (var_s0_2 + 0x3F)) >> 6) + 0x400) & 0xFFF;
            temp_s6 = temp_t6->unkC;
            temp_v0 = ((s32) ((var_t7_2 > -1) ? var_t7_2 : (var_t7_2 + 0x3F)) >> 6) & 0xFFF;
            temp_s3 = *(((temp_v0 + 0x400) * 2) + &gSinTable);
            temp_t1 = ((s32) (D_00433330.unk4C << 0xC) / (s32) D_00433330.unk50) - 0x400;
            temp_t2 = (temp_t6->unk8 - temp_s6) * ((temp_t1 <= 0) ? 0 : temp_t1);
            temp_lo1 = (s32) ((((s32) ((temp_t2 > -1) ? temp_t2 : (temp_t2 + 0xFFF)) >> 0xC) + temp_s6) * D_00433330.unkA4) / 955;
            temp_t6_2 = temp_lo1 * *((temp_v1_2 * 2) + &gSinTable);
            temp_t5 = temp_lo1 * *(((temp_v1_2 + 0x400) * 2) + &gSinTable);
            temp_v0_2 = temp_lo1 * *((temp_v0 * 2) + &gSinTable);
            temp_t1_2 = ((s32) ((temp_t6_2 > -1) ? temp_t6_2 : (temp_t6_2 + 0xFFF)) >> 0xC) * temp_s3;
            temp_t2_2 = ((s32) ((temp_t5 > -1) ? temp_t5 : (temp_t5 + 0xFFF)) >> 0xC) * temp_s3;
            sp20 = D_008BC680.unk0 - ((s32) ((temp_t2_2 > -1) ? temp_t2_2 : (temp_t2_2 + 0xFFF)) >> 0xC);
            sp24 = D_008BC680.unk4 - ((s32) ((temp_v0_2 > -1) ? temp_v0_2 : (temp_v0_2 + 0xFFF)) >> 0xC);
            sp28 = D_008BC680.unk8 - ((s32) ((temp_t1_2 > -1) ? temp_t1_2 : (temp_t1_2 + 0xFFF)) >> 0xC);
            func_0038E308(sp, &sp20, &D_008BC680, 0);
            var_a0_2 = sp;
block_93:
            func_0038E460(var_a0_2, 1);
            break;
        case 2:
            func_0038C0F0(var_a0);
            break;
        case 3:
        case 4:
        case 5:
        case 11:
        case 12:
        case 13:
            memset(&sp30, 0, 0x10);
            temp_s2_2 = temp_s4 + 0x51C;
            temp_t7 = D_005F6C28->unk10;
            var_t3 = D_005F6C28->unk18;
            var_v1 = D_005F6C28->unk1A;
            if (D_00433330.unk34 != 0) {
                var_v1 = 0x1000 - var_v1;
                var_t3 = 0x1000 - var_t3;
            }
            if (D_00433330.unk50 == 0) {
                M2C_BREAK(0);
            }
            temp_a0_2 = temp_s4->unk3C;
            temp_lo = (s32) ((D_00433330.unk50 - D_00433330.unk4C) << 0xC) / (s32) D_00433330.unk50;
            temp_v1_3 = (s16) ((temp_a0_2 > -1) ? temp_a0_2 : (temp_a0_2 + 0xF)) >> 4;
            var_t1 = temp_v1_3 - (((s32) ((temp_v1_3 > -1) ? temp_v1_3 : (temp_v1_3 + 0xFFF)) >> 0xC) << 0xC);
            temp_t2_3 = (temp_lo <= 0) ? 0 : temp_lo;
            temp_t0 = (D_005F6C28->unk12 - temp_t7) * temp_t2_3;
            temp_a2 = (var_v1 - var_t3) * temp_t2_3;
            if (var_t1 < 0) {
                var_t1 += 0x1000;
            }
            var_v0 = D_00433330.unk74 - var_t1;
            if (var_v0 < 0) {
                var_v0 = -var_v0;
            }
            if (var_v0 < 0x401) {
                D_00433330.unk74 = var_t1;
            }
            sp40 = temp_t7 + ((s32) ((temp_t0 > -1) ? temp_t0 : (temp_t0 + 0xFFF)) >> 0xC);
            sp44 = 0;
            sp42 = (s16) (var_t3 + ((s32) ((temp_a2 > -1) ? temp_a2 : (temp_a2 + 0xFFF)) >> 0xC)) + (u16) D_00433330.unk74;
            temp_a2_2 = D_005F6C28->unk8;
            temp_v0_3 = (D_005F6C28->unkC - temp_a2_2) * temp_t2_3;
            temp_a3_2 = temp_v0_3 > -1;
            temp_lo1_2 = (s32) ((temp_a2_2 + ((s32) ((temp_a3_2 != 0) ? temp_v0_3 : (temp_v0_3 + 0xFFF)) >> 0xC)) * D_00433330.unkA4) / 955;
            sp38 = temp_lo1_2;
            Gte_RotMatrixYXZ(&sp40, &sp50, temp_lo1_2, temp_a3_2);
            Gte_ApplyMatrixLV(&sp50, &sp30, &sp30);
            if ((((u32) temp_s4->unkC0->unk24 >> 0x14) & 3) == 2) {
                D_005F6C2C = 1;
            }
            var_s0_3 = &sp80;
            if (D_005F6C2C == 0) {
                temp_v0_4 = temp_s4->unk51C - D_008BC680.unk0;
                temp_s1 = D_005F6C28->unk14;
                temp_a0_3 = D_008BC680.unk0 + ((s32) (temp_v0_4 + (temp_v0_4 >> 0x1F)) >> 1);
                D_008BC680.unk0 = temp_a0_3;
                temp_t1_3 = D_008BC680.unk4 + ((s32) (temp_s2_2->unk4 - D_008BC680.unk4) / 2);
                D_008BC680.unk4 = temp_t1_3;
                sp70 = temp_a0_3 + sp30;
                sp74 = temp_t1_3 + sp34;
                temp_a2_3 = D_008BC680.unk8 + ((s32) (temp_s2_2->unk8 - D_008BC680.unk8) / 2);
                D_008BC680.unk8 = temp_a2_3;
                sp78 = temp_a2_3 + sp38;
                if (temp_t1_3 >= -0x383) {
                    var_v0_2 = -0x384;
                    goto block_41;
                }
                var_v0_2 = -0x578;
                if (temp_t1_3 < -0x578) {
block_41:
                    D_008BC680.unk4 = var_v0_2;
                }
                if (sp74 >= -0x383) {
                    var_v0_3 = -0x384;
                    goto block_45;
                }
                var_v0_3 = -0x578;
                if (sp74 < -0x578) {
block_45:
                    sp74 = var_v0_3;
                }
                func_0038E308(&sp80, &sp70, &D_008BC680, 0);
                sp84 += temp_s1;
                func_0038E460(&sp80, 1);
            } else {
                func_0038E408(var_s0_3, 3);
block_92:
                var_a0_2 = var_s0_3;
                goto block_93;
            }
            break;
        case 6:
        case 7:
        case 8:
        case 14:
        case 15:
        case 16:
            temp_s7 = temp_t6->unk1A;
            temp_fp = temp_t6->unk10;
            temp_s5 = temp_t6->unk12;
            temp_s6_2 = temp_t6->unk18;
            if (D_00433330.unk4C != (D_00433330.unk50 - 2)) {
                if (D_00433330.unk4C != (D_00433330.unk50 - 1)) {
                    var_s0_4 = D_00433330.unk74;
                } else {
                    goto block_52;
                }
            } else {
block_52:
                spC0 = (s128) &D_00433330;
                var_s0_4 = func_00391058(temp_s2, arg1);
                D_00433330.unk74 = var_s0_4;
            }
            if (D_00433330.unk50 == 0) {
                M2C_BREAK(0);
            }
            temp_lo_2 = (s32) ((D_00433330.unk50 - D_00433330.unk4C) << 0xC) / (s32) D_00433330.unk50;
            temp_v0_5 = (temp_lo_2 <= 0) ? 0 : temp_lo_2;
            temp_a2_4 = (temp_s7 - temp_s6_2) * temp_v0_5;
            temp_t0_2 = (temp_s5 - temp_fp) * temp_v0_5;
            temp_a2_5 = (temp_fp + ((s32) ((temp_t0_2 > -1) ? temp_t0_2 : (temp_t0_2 + 0xFFF)) >> 0xC)) << 6;
            temp_v1_4 = (temp_s6_2 + ((s32) ((temp_a2_4 > -1) ? temp_a2_4 : (temp_a2_4 + 0xFFF)) >> 0xC)) << 6;
            if (D_00433330.unk34 == 0) {
                var_s0_5 = var_s0_4 + temp_v1_4;
            } else {
                var_s0_5 = var_s0_4 - temp_v1_4;
            }
            temp_a1 = &D_00433330 + 0x3330;
            temp_v0_6 = temp_a1->unk50;
            temp_a1->unk68 = var_s0_5;
            temp_a1->unk64 = temp_a2_5;
            temp_a1->unk6C = 0;
            temp_lo_3 = (s32) ((temp_v0_6 - temp_a1->unk4C) << 0xC) / temp_v0_6;
            temp_v1_5 = ((s32) ((temp_a2_5 > -1) ? temp_a2_5 : (temp_a2_5 + 0x3F)) >> 6) & 0xFFF;
            temp_a0_4 = (((s32) ((var_s0_5 > -1) ? var_s0_5 : (var_s0_5 + 0x3F)) >> 6) + 0x400) & 0xFFF;
            temp_t1_4 = temp_t6->unk8;
            temp_s3_2 = temp_s4 + 0x51C;
            temp_s0_3 = *(((temp_v1_5 + 0x400) * 2) + &gSinTable);
            temp_a2_6 = (temp_lo_3 <= 0) ? 0 : temp_lo_3;
            temp_t0_3 = (temp_t6->unkC - temp_t1_4) * temp_a2_6;
            temp_lo1_3 = (s32) ((temp_t1_4 + ((s32) ((temp_t0_3 > -1) ? temp_t0_3 : (temp_t0_3 + 0xFFF)) >> 0xC)) * temp_a1->unkA4) / 955;
            temp_a3_3 = *(((temp_a0_4 + 0x400) * 2) + &gSinTable);
            if ((((u32) temp_s4->unkC0->unk24 >> 0x14) & 3) == 2) {
                D_005F6C2C = 1;
            }
            if (D_005F6C2C != 0) {
                var_s0_3 = &sp70;
                func_0038E408(var_s0_3, 3, temp_a2_6, temp_a3_3);
                goto block_92;
            }
            temp_a2_7 = temp_lo1_3 * *((temp_a0_4 * 2) + &gSinTable);
            temp_a0_5 = temp_lo1_3 * temp_a3_3;
            temp_s4_2 = temp_t6->unk14;
            temp_v0_7 = temp_s4->unk51C - D_008BC680.unk0;
            temp_t4 = temp_lo1_3 * *((temp_v1_5 * 2) + &gSinTable);
            temp_t1_5 = D_008BC680.unk0 + ((s32) (temp_v0_7 + (temp_v0_7 >> 0x1F)) >> 1);
            D_008BC680.unk0 = temp_t1_5;
            temp_v1_6 = ((s32) ((temp_a0_5 > -1) ? temp_a0_5 : (temp_a0_5 + 0xFFF)) >> 0xC) * temp_s0_3;
            temp_v0_8 = temp_s3_2->unk4 - D_008BC680.unk4;
            temp_a1_2 = ((s32) ((temp_a2_7 > -1) ? temp_a2_7 : (temp_a2_7 + 0xFFF)) >> 0xC) * temp_s0_3;
            temp_t6_3 = D_008BC680.unk4 + ((s32) (temp_v0_8 + (temp_v0_8 >> 0x1F)) >> 1);
            D_008BC680.unk4 = temp_t6_3;
            temp_v0_9 = temp_s3_2->unk8 - D_008BC680.unk8;
            temp_t3 = D_008BC680.unk8 + ((s32) (temp_v0_9 + (temp_v0_9 >> 0x1F)) >> 1);
            spA0 = temp_t1_5 - ((s32) ((temp_v1_6 > -1) ? temp_v1_6 : (temp_v1_6 + 0xFFF)) >> 0xC);
            spA4 = temp_t6_3 - ((s32) ((temp_t4 > -1) ? temp_t4 : (temp_t4 + 0xFFF)) >> 0xC);
            spA8 = temp_t3 - ((s32) ((temp_a1_2 > -1) ? temp_a1_2 : (temp_a1_2 + 0xFFF)) >> 0xC);
            D_008BC680.unk8 = temp_t3;
            if (temp_t6_3 >= -0x383) {
                var_v0_4 = -0x384;
                goto block_66;
            }
            var_v0_4 = -0x578;
            if (temp_t6_3 < -0x578) {
block_66:
                D_008BC680.unk4 = var_v0_4;
            }
            if (spA4 >= -0x383) {
                var_v0_5 = -0x384;
                goto block_70;
            }
            var_v0_5 = -0x578;
            if (spA4 < -0x578) {
block_70:
                spA4 = var_v0_5;
            }
            func_0038E308(&sp70, &spA0, &D_008BC680, 0);
            sp74 += temp_s4_2;
            func_0038E460(&sp70, 1);
            break;
        case 9:
        case 17:
            temp_s5_2 = temp_t6->unk1A;
            temp_s7_2 = temp_t6->unk10;
            temp_s4_3 = temp_t6->unk12;
            temp_s6_3 = temp_t6->unk18;
            if (D_00433330.unk4C != (D_00433330.unk50 - 2)) {
                if (D_00433330.unk4C != (D_00433330.unk50 - 1)) {
                    var_s0_6 = D_00433330.unk74;
                } else {
                    goto block_77;
                }
            } else {
block_77:
                var_s0_6 = func_00391058(temp_s2, arg1);
                D_00433330.unk74 = var_s0_6;
            }
            if (D_00433330.unk50 == 0) {
                M2C_BREAK(0);
            }
            temp_lo_4 = (s32) ((D_00433330.unk50 - D_00433330.unk4C) << 0xC) / (s32) D_00433330.unk50;
            temp_v0_10 = (temp_lo_4 <= 0) ? 0 : temp_lo_4;
            temp_a2_8 = (temp_s5_2 - temp_s6_3) * temp_v0_10;
            temp_t0_4 = (temp_s4_3 - temp_s7_2) * temp_v0_10;
            temp_a2_9 = (temp_s7_2 + ((s32) ((temp_t0_4 > -1) ? temp_t0_4 : (temp_t0_4 + 0xFFF)) >> 0xC)) << 6;
            temp_v1_7 = (temp_s6_3 + ((s32) ((temp_a2_8 > -1) ? temp_a2_8 : (temp_a2_8 + 0xFFF)) >> 0xC)) << 6;
            if (D_00433330.unk34 == 0) {
                var_s0_7 = var_s0_6 + temp_v1_7;
            } else {
                var_s0_7 = var_s0_6 - temp_v1_7;
            }
            D_00433330.unk68 = var_s0_7;
            D_00433330.unk64 = temp_a2_9;
            D_00433330.unk6C = 0;
            temp_lo_5 = (s32) ((D_00433330.unk50 - D_00433330.unk4C) << 0xC) / (s32) D_00433330.unk50;
            temp_a0_6 = temp_t6->unk8;
            temp_v1_8 = (((s32) ((var_s0_7 > -1) ? var_s0_7 : (var_s0_7 + 0x3F)) >> 6) + 0x400) & 0xFFF;
            temp_v0_11 = ((s32) ((temp_a2_9 > -1) ? temp_a2_9 : (temp_a2_9 + 0x3F)) >> 6) & 0xFFF;
            temp_t7_2 = *(((temp_v0_11 + 0x400) * 2) + &gSinTable);
            temp_a3_4 = (temp_t6->unkC - temp_a0_6) * ((temp_lo_5 <= 0) ? 0 : temp_lo_5);
            temp_lo1_4 = (s32) ((temp_a0_6 + ((s32) ((temp_a3_4 > -1) ? temp_a3_4 : (temp_a3_4 + 0xFFF)) >> 0xC)) * D_00433330.unkA4) / 955;
            temp_t2_4 = temp_lo1_4 * *((temp_v1_8 * 2) + &gSinTable);
            temp_t1_6 = temp_lo1_4 * *(((temp_v1_8 + 0x400) * 2) + &gSinTable);
            temp_a0_7 = temp_lo1_4 * *((temp_v0_11 * 2) + &gSinTable);
            temp_v1_9 = ((s32) ((temp_t2_4 > -1) ? temp_t2_4 : (temp_t2_4 + 0xFFF)) >> 0xC) * temp_t7_2;
            temp_a1_3 = ((s32) ((temp_t1_6 > -1) ? temp_t1_6 : (temp_t1_6 + 0xFFF)) >> 0xC) * temp_t7_2;
            sp90 = D_008BC680.unk0 - ((s32) ((temp_a1_3 > -1) ? temp_a1_3 : (temp_a1_3 + 0xFFF)) >> 0xC);
            sp94 = D_008BC680.unk4 - ((s32) ((temp_a0_7 > -1) ? temp_a0_7 : (temp_a0_7 + 0xFFF)) >> 0xC);
            sp98 = D_008BC680.unk8 - ((s32) ((temp_v1_9 > -1) ? temp_v1_9 : (temp_v1_9 + 0xFFF)) >> 0xC);
            if (D_008BC680.unk4 >= -0x383) {
                var_v0_6 = -0x384;
                goto block_86;
            }
            var_v0_6 = -0x578;
            if (D_008BC680.unk4 < -0x578) {
block_86:
                D_008BC680.unk4 = var_v0_6;
            }
            if (sp94 >= -0x383) {
                var_v0_7 = -0x384;
                goto block_90;
            }
            var_v0_7 = -0x578;
            if (sp94 < -0x578) {
block_90:
                sp94 = var_v0_7;
            }
            var_s0_3 = &sp70;
            func_0038E308(var_s0_3, &sp90, &D_008BC680, 0);
            goto block_92;
        }
    } else {
    case 10:
        func_0038C0F0(var_a0);
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

/* NON_MATCHING: draft, 77 lines of fdiff differ (139 instructions in the original, 134 built) */
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

/* NON_MATCHING: draft, 150 lines of fdiff differ (138 instructions in the original, 141 built) */
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

/* NON_MATCHING: m2c draft, not cleaned up (138 instructions; stack locals, struct copies and some types still raw) */
#if 0
void func_003959C8(void *arg0, s32 arg1) {
    s32 temp_a1;
    s32 temp_a1_2;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_a3_2;
    s32 temp_lo;
    s32 temp_s1;
    s32 temp_s2;
    s32 temp_t0;
    s32 temp_t4;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v1;
    void *temp_a3;
    void *temp_t7;
    void *temp_t7_2;
    temp_s2 = D_008BC6C8.unkC - D_008BC6C8.unk0;
    temp_s1 = D_008BC6C8.unk14 - D_008BC6C8.unk8;
    temp_v0 = func_003985D8((temp_s2 * temp_s2) + (temp_s1 * temp_s1), D_008BC6C8.unk14);
    temp_a3 = arg0 + 0x708;
    temp_t7 = arg0 + 0x51C;
    temp_t7_2 = (arg1 != 0) ? (arg1 + 0x51C) : temp_t7;
    temp_a2 = (s32) (temp_a3->unk4 + ((arg1 == 0) ? temp_a3 : (arg1 + 0x708))->unk4) >> 1;
    temp_t0 = (temp_v0 == 0) ? 1 : temp_v0;
    if ((temp_v0 - ((s32) (D_00433330.unkA4 * 0x1B58) / 955)) > 0) {
        temp_lo = (s32) D_00433330.unkA4 / 191;
        D_008BC6C8.unk0 = (s32) (D_008BC6C8.unk0 + ((s32) (temp_lo * temp_s2) / temp_t0));
        D_008BC6C8.unk8 = (s32) (D_008BC6C8.unk8 + ((s32) (temp_lo * temp_s1) / temp_t0));
    }
    temp_v1 = (((s32) (arg0->unk51C + temp_t7_2->unk0) >> 1) - D_008BC6C8.unkC) * 6;
    temp_a1 = (((temp_a2 >= -0x18F) ? -0x190 : temp_a2) - D_008BC6C8.unk10) * 6;
    temp_a3_2 = temp_v1 > -1;
    temp_a2_2 = temp_a1 > -1;
    temp_t4 = D_008BC6C8.unk10 + ((s32) ((temp_a2_2 != 0) ? temp_a1 : (temp_a1 + 0x7F)) >> 7);
    D_008BC6C8.unkC = (s32) (D_008BC6C8.unkC + ((s32) ((temp_a3_2 != 0) ? temp_v1 : (temp_v1 + 0x7F)) >> 7));
    D_008BC6C8.unk10 = temp_t4;
    temp_v0_2 = (((s32) (temp_t7->unk8 + temp_t7_2->unk8) >> 1) - D_008BC6C8.unk14) * 6;
    D_008BC6C8.unk14 = (s32) (D_008BC6C8.unk14 + ((s32) ((temp_v0_2 > -1) ? temp_v0_2 : (temp_v0_2 + 0x7F)) >> 7));
    if ((D_00433330.unkB4 == 2) && ((D_008BC6C8.unk4 - temp_t4) < -0x190)) {
        D_008BC6C8.unk10 = (s32) (D_008BC6C8.unk4 + 0x190);
    }
    func_0035B690(&D_008BC6C8, sp, temp_a2_2, temp_a3_2);
    temp_v0_3 = subroutine_arg0 & 0xFFF;
    temp_a1_2 = temp_v0_3 - 0x1000;
    func_0035B858(sp, temp_a1_2, ((subroutine_arg0 & 0x800) ? temp_a1_2 : temp_v0_3) < 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003959C8);
#endif

/* NON_MATCHING: the constant 0xC00 - unk1A / 16 + (D_005F6C58 - 0x400) is folded into D_005F6C58 + 0x800 (two instructions short) */
#if 0
void func_00395BF0(Fighter *f) {
    s32 v[12];

    v[0] = 0x400;
    v[1] = 0xC00 - f->unk1A / 16;
    v[1] += D_005F6C58 - 0x400;
    v[2] = 0;
    v[5] = f->unk51C;
    v[6] = -(D_00433330.unkA4 * 8000 / 0x3BB) - D_005F6C54;
    v[7] = f->unk524;
    func_0035B858(v);
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00395BF0);
#endif

s32 func_00395C90(s32 a0, s32 a1, s32 a2) {
    a2 &= 1;
    D_005F6140 = 0;
    switch (a2) {
    case 0:
    case 1:
        func_003959C8(a0, a1, a2);
        break;
    default:
        func_003959C8(a0, 0, a2);
        break;
    }
    return 0;
}

s32 func_00395CD8(s32 a0, s32 a1, u32 a2) {
    func_00390FA8(a0, a1, a2 >> 31, D_00433330.unkA4 * 0x76C / 256);
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

/* NON_MATCHING: m2c draft, not cleaned up (272 instructions; stack locals, struct copies and some types still raw) */
#if 0
s32 func_00395EE0(void *arg0, void *arg1, s32 arg2) {
    s16 temp_a0;
    s16 temp_t0;
    s16 temp_t2;
    s32 temp_a0_3;
    s32 temp_a1;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_a2_3;
    s32 temp_a3;
    s32 temp_a3_2;
    s32 temp_a3_3;
    s32 temp_t1;
    s32 temp_t2_2;
    s32 temp_t5;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 temp_v1;
    s32 temp_v1_2;
    s32 var_a0;
    s32 var_t8;
    u32 temp_a0_2;
    u32 temp_a1_2;
    temp_a3 = D_00433330.unkA4 * 0x76C;
    var_t8 = 0;
    temp_v0 = ((s32) (((s32) ((temp_a3 > -1) ? temp_a3 : (temp_a3 + 0xFF)) >> 8) * 0xD) / 10) * 0x60;
    temp_t1 = (s32) ((temp_v0 > -1) ? temp_v0 : (temp_v0 + 0x7F)) >> 7;
    if ((D_00433330.unkC0 < 0) || !(gFrameCount & 1) || (arg2 < 0)) {
        D_005F6C3C = arg0->unk51C;
        D_005F6C40 = arg0->unk524;
        D_005F6C44 = arg1->unk51C;
        D_005F6C48 = arg1->unk524;
        if (arg2 < 0) {
            D_005F6C30 = ~arg2;
            D_005F6C38 = (s32) arg0->unk1A;
            D_005F6C34 = 0;
        }
    }
    switch (D_005F6C30) {                           /* irregular */
    case 0:
        var_t8 = 0;
        break;
    case 1:
        if (D_00433330.unkB4 == 0) {
        }
block_18:
        var_t8 = ((s32) (D_005F6C34 * 0x1F40) / 720) - 0xFA0;
        break;
    case 2:
        if (D_00433330.unkB4 == 0) {
        }
        goto block_18;
    }
    D_005F6C34 += 1;
    if (arg0->unkF2 != 0) {
        var_a0 = D_005F6C38;
    } else {
        temp_a0 = arg0->unk1A;
        temp_a1 = 0x400 - ((s16) ((temp_a0 > -1) ? temp_a0 : (temp_a0 + 0xF)) >> 4);
        var_a0 = temp_a1;
        D_005F6C38 = temp_a1;
    }
    temp_v0_2 = ((s32) ((var_a0 > -1) ? var_a0 : (var_a0 + 0x3F)) >> 6) & 0xFFF;
    temp_a0_2 = D_005F6C44 + D_005F6C3C;
    temp_t0 = *((temp_v0_2 * 2) + &gSinTable);
    temp_t2 = *(((temp_v0_2 + 0x400) * 2) + &gSinTable);
    temp_v1 = temp_t1 * temp_t0;
    temp_a2 = temp_t1 * temp_t2;
    temp_t2_2 = var_t8 * temp_t2;
    temp_t5 = var_t8 * temp_t0;
    temp_a1_2 = D_005F6C48 + D_005F6C40;
    temp_a3_2 = (s32) ((temp_v1 > -1) ? temp_v1 : (temp_v1 + 0xFFF)) >> 0xC;
    temp_a2_2 = (s32) ((temp_t2_2 > -1) ? temp_t2_2 : (temp_t2_2 + 0xFFF)) >> 0xC;
    temp_v0_3 = (func_00366108(((s32) (temp_a0_2 + (temp_a0_2 >> 0x1F)) >> 1) - ((D_005F6C3C - ((s32) ((temp_a2 > -1) ? temp_a2 : (temp_a2 + 0xFFF)) >> 0xC)) + ((s32) ((temp_t5 > -1) ? temp_t5 : (temp_t5 + 0xFFF)) >> 0xC)), ((s32) (temp_a1_2 + (temp_a1_2 >> 0x1F)) >> 1) - ((D_005F6C40 - temp_a3_2) - temp_a2_2), temp_a2_2, temp_a3_2) - 0x400) << 6;
    temp_a0_3 = -temp_v0_3;
    temp_v1_2 = ((s32) ((temp_a0_3 > -1) ? temp_a0_3 : (temp_a0_3 + 0x3F)) >> 6) & 0xFFF;
    temp_a3_3 = *(((temp_v1_2 + 0x400) * 2) + &gSinTable) * (D_005F6C40 - subroutine_arg2);
    temp_a2_3 = (*((temp_v1_2 * 2) + &gSinTable) * (D_005F6C3C - subroutine_arg0)) + temp_a3_3;
    func_00366108((s32) ((temp_a2_3 > -1) ? temp_a2_3 : (temp_a2_3 + 0xFFF)) >> 0xC, -subroutine_arg1, temp_a2_3, temp_a3_3, temp_v0_3);
    func_0038E460(sp, 0, func_0038DB98(0));
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

/* NON_MATCHING: the store to D_005F6C52 lands in the delay slot of the call, where the original has `move a1, a2` */
#if 0
s32 func_003963C0(u32 n, Fighter *a, Fighter *b) {
    u32 k = n % 5;

    D_005F6C50 = k;
    D_005F6C52 = D_00509C10[k];
    func_00390E08(a, b);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_003963C0);
#endif

INCLUDE_RODATA("asm/nonmatchings/stage", jtbl_00509C30);

/* NON_MATCHING: draft, 91 lines of fdiff differ (129 instructions in the original, 117 built) */
#if 0
s32 func_00396418(s32 a0, s32 a1) {
    func_0038EC08((Fighter *)a0, (Fighter *)a1, 0);
    switch ((s16)(D_005F6C52 - 0xA)) {
    case 0x0:
        func_00395CD8(a0, a1, -1);
        goto start;
    case 0x1:
        func_00395CD8(a0, a1, 0);
        goto step;
    case 0xA:
        func_00395EE0(a0, a1, -1);
        goto start;
    case 0xB:
        func_00395EE0(a0, a1, 0);
        goto step;
    case 0x14:
        func_00395D20(a0, a1, -1);
        goto start;
    case 0x15:
        func_00395D20(a0, a1, 0);
        goto step;
    case 0x1E:
        func_00395EE0(a0, a1, -2);
        goto start;
    case 0x1F:
        func_00395EE0(a0, a1, 0);
        goto step;
    case 0x29:
        func_00396320((Fighter *)a0, (Fighter *)a1, 0);
        goto step;
    case 0x28:
    case 0x32:
        func_00396320((Fighter *)a0, (Fighter *)a1, -1);
    start:
        D_005F61B8 = 0x2D0;
        D_005F6C52++;
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
            D_005F6C52 = D_00509C10[(u32)(s16)D_005F6C50 % 5];
        }
        break;
    default:
        func_003963C0(0, (Fighter *)a0, (Fighter *)a1);
        break;
    }
    func_0038E4F8(0);
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

/* NON_MATCHING: draft, 54 lines of fdiff differ (83 instructions in the original, 78 built) */
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

/* NON_MATCHING: draft, 57 lines of fdiff differ (79 instructions in the original, 77 built) */
#if 0
void func_00396970(Fighter *a, Fighter *b) {
    s32 d;

    if (D_005F61CC != 0) {
        func_003967E0();
        if (D_008BC6E8.unk0 < 2 && D_008BC6E8.unk0 >= 0) {
            func_0038FD90((s32)a, (s32)b, D_008BC6E8.unkC, D_008BC6E8.unk14 + D_008BC6E8.unk20, 0, D_008BC6E8.unk4,
                          D_008BC6E8.unk1C, 1);
            d = D_008BC6E8.unk18 - D_008BC6E8.unk14;
            D_008BC6E8.unk4 = 0;
            if (d & 0x20000) {
                d |= 0xFFFC0000;
            } else {
                d &= 0x3FFFF;
            }
            D_008BC6E8.unk14 += d / 128;
            D_008BC6E8.unk1C += (D_00433330.unkA4 * 0x76C / 256 - D_008BC6E8.unk1C) * 6 / 256;
        }
        func_0038E4F8(1);
        D_005F6140 = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00396970);
#endif

/* NON_MATCHING: the order of the stores around the calls differs */
#if 0
void func_00396AB0(Fighter *a, Fighter *b) {
    s32 n;

    func_0038C0F0();
    func_00390E08(a, b);
    func_003967C8();
    n = D_008BC6E8.unk0 + 1;
    D_00433330.unk4[1] = 0x1000;
    D_008BC6E8.unk4 = 1;
    D_005F61CC = 1;
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
        D_008BC6E8.unk20 = D_008BC6E8.unk20 - 0x71C7 + func_003800D8(0xE38E);
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
        D_008BC6E8.unk20 = D_008BC6E8.unk20 - 0xAAAA + n;
    }
    func_00396970(a, b);
}
#else
INCLUDE_ASM("asm/nonmatchings/stage", func_00396AB0);
#endif

void func_00396CF8(void) {
    D_005F61CC = 0;
}
