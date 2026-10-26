/* 0x3E8AD8-0x3F3320 (module tekken_bowl, docs/game_overview.md section 4; docs/systems/modes.md): Tekken Bowl, game
 * type 8. .rodata 0x523570-0x523FA0 (subsegment; the tables read by several functions are included in front of the
 * first jump table, as in the original object), .lit4 0x5F596C-0x5F5BCC. The object is built without -ffast-math
 * (NO_FAST_MATH: x - 0.0f kept in func_003EF3A8 / Bowl_PinHomePos).
 *
 * Bowling state: a byte-stream writer / reader at 0x5F7098 (0xFF stuffing, JPEG-like), the ball D_01FC51E0, the ten
 * pins D_01FC5260 (BowlBody, 0x180 bytes), the two players' games D_01FC61E0 (BowlPlayer, 0x5E bytes: ball index,
 * pins per ball, cumulative frame scores), the high-score table D_0046BC70 (top 5 + bonus flags). */
#include "common.h"
#include "fighter.h"

extern u8 *D_005F7098;
extern u8 *D_005F709C;
extern u8 *D_005F70A0;
extern s16 D_005F70A4;
extern s16 D_005F70A6;

void Bs_Init(u8 *buf, s32 size);
s32 func_003E8AF8(void);
void func_003E8B48(void);
void Bs_PutBits(u8 v, s32 n);
void func_003E8C40(void);
s32 Bs_GetBits(s32 n);
void func_003E8CE0(void);
s32 Bs_GetBitsN(s32 n);

typedef f32 FVec4[4] __attribute__((aligned(16)));

extern s16 gMode;
extern u8 D_0043B7F8[];
extern u16 D_005F5CB6;
extern u16 D_005F5C76;
extern s32 D_005F6730;
extern s32 D_005F69B8;

extern s16 D_005F70A8;
extern s16 D_005F70AE;
extern s16 D_005F70AA;
extern u16 D_005F70AC;
extern f32 D_005F70B0;
extern s16 D_005F70B6;
extern u16 D_005F70C2;
extern s16 D_005F70D4;
extern s16 D_005F70D6;
extern s16 D_005F70D8;
extern s16 D_005F70DA;
extern s16 D_005F70F8;
extern s16 D_005F70FA;
extern s16 D_005F70FC;
extern s16 D_005F70FE;
extern s16 D_005F7100;
extern s16 D_005F7102;
extern s16 D_005F7104;
extern u8 D_005F7106;
extern u8 D_005F7107;

/* 0x1FC51B0 */
typedef struct {
    s32 unk0;
    f32 unk4;
    f32 unk8;
    s32 unkC;
    f32 unk10;
    s32 unk14;
    s32 unk18;
    s32 unk1C;
    f32 unk20;
    s32 unk24;
} BowlCam;
extern BowlCam D_01FC51B0;

/* The ball, 0x1FC51E0 */
typedef struct {
    f32 unk0;
    f32 unk4;
    f32 unk8;
    f32 unkC;
    f32 unk10;
    f32 unk14;
    f32 unk18;
    f32 unk1C;
    f32 unk20;
    f32 unk24;
    f32 unk28;
    f32 unk2C;
    u8 pad30[0x20];
    f32 unk50;
    f32 unk54;
    f32 unk58;
    f32 unk5C;
    f32 unk60;
    f32 unk64;
    f32 unk68;
    f32 unk6C;
    s32 unk70;
    s32 unk74;
    s32 unk78;
} __attribute__((aligned(8))) BowlBall;
extern BowlBall D_01FC51E0;
extern BowlBall D_01FC62C0;


/* Per-player state, 0x1FC61E0 (0x5E bytes each) */
typedef struct {
    /* 0x00 */ s8 unk0;
    /* 0x01 */ s8 unk1[12];
    /* 0x0D */ s8 unkD[26];
    /* 0x27 */ u8 unk27[26];
    /* 0x41 */ u8 pad41;
    /* 0x42 */ s16 unk42[11];
    /* 0x58 */ s8 unk58[3];
    /* 0x5B */ u8 unk5B[3];
} BowlPlayer;
extern BowlPlayer D_01FC61E0[];

/* 0x1FC62A0 */
typedef struct {
    s16 unk0;
    u8 pad2[0x3E];
} BowlLane;
extern BowlLane D_01FC62A0;

/* Score sheet record (0x66 bytes): rank, character / costume, name, then a copy of the player's game. */
typedef struct {
    u8 unk0;
    s8 unk1;
    u8 unk2;
    u8 unk3;
    u8 name[4];
    BowlPlayer game;
} BowlScore;
typedef struct {
    BowlScore rec[5];
    s16 unk1FE;
    s16 unk200;
    s16 unk202;
    s16 unk204;
} BowlScores;
extern BowlScores D_0046BC70;

extern TeamMember D_00440940[2][2];

typedef struct {
    u8 unk0[4];
    u8 unk4;
} BowlChar;

extern s32 D_00415808[];
extern u8 D_00436530[];
extern const s16 D_005237D8[];
extern const char D_00523990[];

s32 Text_Printf(const char *fmt, ...);
void func_00361968(void);
void func_0035C2F8(s16 i);
void func_003BB218(s32 i, s32 port);
void func_0035B4B0(s32 *c, void *m);
void func_003671D0(f32 a);
void func_00341218(void);
void func_003D1040(void);
void Wind_Update(void);
BowlChar *func_0035C738(s32 character, s32 costume);
void sceVu0SubVector(f32 *v0, f32 *v1, f32 *v2);
void sceVu0ScaleVector(f32 *v0, f32 *v1, f32 s);
f32 func_0034A570(f32 a);

void func_003E92C0(f32 *d, f32 *a, f32 *b, f32 t);
void func_003EC388(f32 *a, f32 *b);
void Bowl_DrawTop5(void);
s32 func_003F1ED8(void);
s32 func_003F1EE8(void);
s32 func_003F1EF8(void);
extern u64 D_005F6738;
extern u64 D_005F6740;
extern s32 D_005F5F20;
extern s32 D_005F5EE0;
extern u16 D_005F70C0;
extern s16 D_005F70F0[2];
extern const char D_00523F40[];

/* A rigid body of the bowling scene (ball / pin): position, velocity, rotation, matrices. */
typedef union {
    u128 q;
    f32 v[4];
} BVec;
typedef struct {
    BVec m[4];
} BMtx;
typedef struct {
    /* 0x000 */ BVec unk0;
    /* 0x010 */ BVec unk10;
    /* 0x020 */ BVec unk20;
    /* 0x030 */ BVec unk30;
    /* 0x040 */ BVec unk40;
    /* 0x050 */ BVec unk50;
    /* 0x060 */ BVec unk60;
    /* 0x070 */ BVec unk70;
    /* 0x080 */ BMtx unk80;
    /* 0x0C0 */ BMtx unkC0;
    /* 0x100 */ BVec unk100[4];
    /* 0x140 */ BVec unk140;
    /* 0x150 */ BVec unk150;
    /* 0x160 */ BVec unk160;
    /* 0x170 */ s32 unk170;
    /* 0x174 */ s32 unk174;
    /* 0x178 */ s32 unk178;
    /* 0x17C */ s16 unk17C;
    /* 0x17E */ s16 unk17E;
} BowlBody;
extern BowlBody D_01FC5260[];
extern const f32 D_00523570[];
extern s32 D_005F5F08;
extern s32 D_005F5EE4;
extern s32 D_005F5CD0;
typedef f32 BMat[4][4] __attribute__((aligned(16)));
extern BMat D_01FC61A0;
extern BMat D_01FC6160;
void func_003C5790(void);
void Mtx_Unit(BMat m);
void sceVu0CopyMatrix(BMat m0, BMat m1);
void func_003B8ED8(void);
void func_003A9688(s32 a);
void func_00393708(s32 a);
s16 func_003A9680(void);
void Motion_LoadBanks(s32 src);
void func_003E1BC8(void);
void Fx_StartStageLayers(void);
void func_003E9AE8(void);
void func_003F1838(void);
void func_003E9A90(void);
void func_003E9C10(void);
void func_003E9530(s16 v);
void func_0035C8D0(Fighter *f, s32 character, s32 costume);
void func_003ADB98(Fighter *f);
void func_00368168(Fighter *f);
void Tag_SetCounter(s32 side, s32 state);
s32 func_00367310(s16 slot);
void func_003AF540(Fighter *f, s32 a, s32 b);
void func_003AFDA8(s32 side, s32 slot, s32 b);
void Fx_LoadFighterTex(Fighter *f);
void func_003ECA80(BowlBody *b, f32 t);

void Sprite_SetBlend(s32 a, s32 b);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_Draw(s32 a, s32 b, s32 c, s32 x, s32 y);
void func_003CE5D8(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, s32 tex);
void sceVu0AddVector(f32 *v0, f32 *v1, f32 *v2);
void func_003532A0(void);
void func_0034E8B0(Fighter *f);
void func_003ADFB0(void);
void func_003F2AC0(BowlScore *s, s32 y);
s32 Bowl_SetFighterMove(Fighter *f, s32 c);
u8 func_003F0CC8(s32 side, s32 member);
void func_003F25D8(BowlPlayer *p, u8 c0, u8 c1, s32 d);
void func_003F0E28(s32 x, s32 y, s32 v);
extern s16 D_005F70B4;
extern s16 D_005F70B8;
extern s16 D_005F70BA;
extern u16 D_005F70BC;
extern s16 D_005F70C4;
extern u16 D_005F70C8;
extern s16 D_01FC6340[2][3];
typedef struct {
    u8 str[4];
    u16 len;
    u8 unk6;
    u8 unk7;
    u8 pad8[8];
    u8 unk10;
    u8 unk11;
} BowlName;
extern BowlName D_01FC6368[2];
void NameEntry_Init(BowlName *p, s32 a, s32 b, s32 c);
s32 func_003AFFB8(s32 i, s32 a);
void Bowl_ResetPins(void);
void func_003E92E0(s16 *p, s32 n);
extern s16 D_005F70F4;
extern s16 D_005F70F6;
extern const u8 D_00523590[][4];
extern const u8 D_005237B0[];
s32 Bowl_NameEntry(s32 a, s32 i, s32 st);
void func_003F2E58(void);
void func_003F1F30(s32 a, s32 b);
u16 func_00364088(s32 i);
void func_00364080(s32 i);
void Fighter_RunMoveEvents(Fighter *f);
typedef struct {
    u16 x;
    u16 y;
    u32 z;
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    u32 q;
} BowlVtx;
typedef struct {
    u16 alpha;
    u16 pad2[3];
    BowlVtx v[4];
} __attribute__((aligned(8))) BowlQuad;
extern BowlQuad D_0043FD10[];
void func_003BB680(BowlQuad *q, s32 z, s32 unused);
extern const f32 D_00523580[];
void func_00362E90(Fighter *f, s32 a, s32 b, u8 c);

/* Bare sqrt.s as in model_anim_b.c (keeps the loads after it in place). */
static inline f32 SqrtS(f32 x) {
    f32 r;

    EE_SQRT_S(r, x);
    return r;
}
extern u16 gFrameCount;
extern u16 D_005F5CB0[2];
extern const char D_00523870[];
extern const char D_00523880[];
extern const char D_00523890[];
extern const char D_005238A0[];
extern const char D_005238B0[];
extern const char D_005238C0[];
void func_003C59D8(s32 x, s32 y, s32 mode, s32 first, s32 last, s32 extra, BowlPlayer *p0, BowlPlayer *p1, s32 cur);
typedef struct {
    f32 v[4];
} __attribute__((aligned(8))) BVec8;
extern const BVec8 D_00523950;
/* 0x1FC6350: the ball in the hand (three bone matrices) */
typedef struct {
    f32 *m[3];
    u8 unkC;
    u8 unkD;
    u8 unkE;
    u8 unkF;
    u8 unk10;
    u8 unk11;
    u8 unk12;
} BowlHold;
extern BowlHold D_01FC6350;
void sceVu0ApplyMatrix(f32 *v0, f32 *m, f32 *v1);
extern s16 D_005F70CA;
extern s16 D_005F70EA;
typedef struct {
    char s[0x2B];
} BowlStr2B;
extern const BowlStr2B D_00523A80;
typedef struct {
    const char *s[5];
} BowlStr5;
extern const BowlStr5 D_00523AB0;
extern const char D_00523AC8[];
extern const char D_00523AD8[];
extern const char D_00523AF0[];
void *memset(void *p, s32 c, s32 n);
s32 func_003DB8A8(s32 i);
void func_003DB808(s32 i, f32 *v);
void func_003DB860(s32 i);
/* Declared with a return value (none is returned or used): with `void`, func_003EFDB8's i < 13 test lands in v0
 * instead of v1. */
s32 func_003EA4C8(BowlPlayer *p);
void func_003E97C8(s32 i, s32 port);
s32 func_003F23F8(s32 i);
extern s16 D_005F70D0;
extern s16 D_005F70D2;
void Snd_PlaySe(s32 id, s32 player);
extern Fighter gFighters[4];
extern const char D_00523858[];
extern const char D_00523A30[];
extern const char D_00523A40[];
s32 Pad_IsMultitap(void);
s32 Pad_GetPlayerMode(s32 player);
void func_003DEB80(s32 a);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 b, s32 color);
void Text_DrawId(s32 x, s32 y, s32 id, s32 color);
float asinf(float);
s32 Bowl_DrawMissFoul(void);
void sceVu0RotMatrixY(BMat m0, BMat m1, f32 ry);
void sceVu0RotMatrixX(BMat m0, BMat m1, f32 rx);
void sceVu0MulMatrix(BMat m0, BMat m1, BMat m2);
void func_003D10D0(s32 i, BMat m, f32 scale);
extern s16 D_005F70E8;
extern s16 D_005F70EC;
s32 Bowl_InsertHighScore(BowlPlayer *g, s32 c0, s32 c1, s32 from);
void func_003F2398(void);
void func_003F2D98(s32 (*names)[2]);
BowlPlayer *func_003E9438(s32 i);
extern s16 D_005F70BE;
extern const u8 D_00523618[][4];
void Bowl_PinHomePos(f32 *p, u32 i);
extern const BVec8 D_00523820;
void func_0034A868(f32 *out, f32 *v);
void func_003A2298(f32 *v);
void func_003EC768(BowlBall *b);
void func_003EDC90(BowlBall *b, BowlBody *p);
void func_003EE2F8(BowlBall *b);
void func_003EEBF8(Fighter *f, BowlBody *p);
void func_003EDF88(BowlBody *p, BowlBody *q);
void func_003ED338(BowlBody *p);
void func_003EE598(BowlBody *p);
s32 func_003EC200(void);
void func_003EBB80(void);
void func_003EC9A0(BowlBody *b);
typedef struct {
    s32 x;
    s32 y;
    s32 z;
    s16 r;
    u8 padE[6];
} BowlSph;
void func_003ECDE8(BowlBody *b, f32 *n, f32 *pt);
void func_003ECAD8(BowlBody *b);
typedef struct {
    s32 v[2];
} BowlW2;
typedef struct {
    const char *s[2];
} BowlS2;
extern s32 D_005F69F0[];
extern const char *D_005F6A00[];
extern s32 D_005F6A08[];
extern u16 D_005F70C6;
extern s16 D_005F70CE;
extern const char D_005239A8[];
void Bgm_SetVolumeFade(s32 vol, s32 frames);
extern const u8 D_00523C00[][2];
extern const BowlPlayer D_00523C10[];
void func_003D01C0(u8 *dst, u8 i);
extern const char *D_005F69D8[];
extern s32 D_005F69E0[];
extern s32 D_005F69E8[];
extern f32 D_005F69BC;
extern f32 D_005F69C0;
extern const char D_00523F68[];
extern const char D_00523F88[];
s32 func_003B1468(void);
void func_003F2A38(s32 y);
typedef struct {
    s8 unk0;
    u8 pad1[7];
} BowlBgm;
extern BowlBgm D_00455D80[];
void Bgm_Stop(void);
void Bgm_Play(s32 id);
void func_003F1968(s32 a);
void func_003F1F18(s16 a, s16 b);
extern const char D_00523EC8[];
extern const char D_00523ED8[];
extern const char D_00523EE8[];
extern const char D_00523EF8[];
extern const char D_00523F08[];
extern const char D_00523F18[];
extern const char D_00523F30[];
void func_003CC078(s32 x, s32 y, s32 z);
void func_003CBFB8(s32 spr, s32 x, s32 y, s32 z);
extern const char D_00523F58[];
extern const char D_005F6AD0[];
s32 NameEntry_Step(BowlName *e, s32 a);
void Str_Copy(u8 *dst, u8 *src);
void func_003EC990(BowlBody *b);
s32 rand(void);
extern BVec D_0046BC20[4];
extern BVec D_0046BC60;
void func_003E9120(BMat m, f32 *rot, f32 *pos);
void func_003EC320(f32 *out, f32 *in);
void func_003E8FA0(f32 a);

/* data read by the drafts */
/* Byte blocks for the drafts' stack copies of .rodata / .sdata tables (copied with ldl / ldr). */
typedef struct { u8 b[8]; } BowlB8;
typedef struct { u8 b[12]; } BowlB12;
typedef struct { u8 b[13]; } BowlB13;
typedef struct { u8 b[16]; } BowlB16;
typedef struct { u8 b[20]; } BowlB20;
typedef struct { u8 b[24]; } BowlB24;
#define BOWL_SP(o, T) (*(T *)((u8 *)sp + (o)))
extern s32 D_005F6998[];
extern s32 D_005F69A0[];
extern s32 D_005F6A60[];
extern s32 D_005F6A68[];
extern s32 D_005F6AC8[];
extern const u8 D_005239C8[];
extern const u8 D_00523A18[];
extern const u8 D_00523B00[];
extern const u8 D_00523E08[];
extern const u8 D_00523E18[];
extern const u8 D_00523E48[];
extern const char D_005238E0[];
extern const char D_005238F0[];
extern const char D_00523908[];
extern const char D_00523B18[];
extern const char D_00523B30[];
extern const char D_00523B40[];
extern const char D_00523B78[];
extern const char D_00523B98[];
extern const char D_00523BB8[];
extern const char D_00523BD0[];
extern const char D_00523BF0[];
extern const char D_00523E60[];
extern const char D_00523E80[];
extern const char D_00523EA0[];
extern const char D_00523EB8[];
extern u16 D_005F5CBE;
extern u16 D_005F5CB2;
f32 cosf(f32);
f32 sinf(f32);
f32 atan2f(f32, f32);
f32 acosf(f32);
void func_003F3108(void);
extern f32 D_01FC51C0[];
extern s32 D_00415718[];
extern s32 D_00415728[];
extern BVec D_0046BBB0[7];
extern s32 D_005F69B0;
extern BVec D_0046BB70;
extern s16 D_005F6988;
extern s16 D_005F698A;
extern s16 D_005F6A10;
extern s16 D_005F6A12;
extern s16 D_005F70CC;
extern s16 D_005F70F2;
extern s32 D_005F69A8;
extern s32 D_005F70DC;
extern s32 D_005F70E0;
extern s32 D_01FC5254[];
extern s8 D_0043B7FA[];
extern u8 D_01FC635C[];
extern u8 D_01FC6360[];
extern const u8 D_00523960[];
extern const u8 D_00523E30[];
extern const u8 D_005236A0[][8];

/* drafts (NON_MATCHING) */
void func_003E9C58(Fighter *arg0, void *arg1);
void func_003EA978(void);
void func_003EAF78(Fighter *arg0, s16 *arg1, s16 arg2, s16 arg3);
void func_003EBC98(void *arg0);
void func_003EEF00(void);
void func_003EF8B0(s8 *arg0);
s32 func_003EFB00(s32 arg0);
s32 func_003F0008(void);
void func_003F0FF0(s32 arg0, s32 arg1, s32 arg2);
s32 func_003F1570(void);
void func_003E9808(void);
void func_003E9FD8(void);
s32 func_003EAE08(Fighter *f, s16 *out);
void func_003ECF70(BowlBody *b);
s32 Bowl_DrawSpare(void);
s32 func_003EFDB8(void);
s32 Pause_BowlOptions(u16 held, s16 pressed);
s32 func_003F2418(u32 i);
s32 Pause_BowlTop5(s16 pad);

/* .rodata tables read by several functions (in front of the first jump table, as in the original object). */
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523570);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523580);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523590);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523618);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005236A0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005237B0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005237D8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523820);

void Bs_Init(u8 *buf, s32 size) {
    D_005F7098 = buf; 
    D_005F709C = buf + size; 
    D_005F70A4 = 8; 
    D_005F70A0 = buf; 
    D_005F70A6 = 0; 
}

s32 func_003E8AF8(void) {
    return D_005F70A0 - D_005F7098;
}

s32 func_003E8B08(void) {
    s32 size = D_005F709C - D_005F7098;
    return size * D_005F70A6 + func_003E8AF8();
}

s32 func_003E8B40(void) {
    return D_005F70A6;
}

void func_003E8B48(void) {
    s16 n = D_005F70A6;

    if (++D_005F70A0 >= D_005F709C) {
        Bs_Init(D_005F7098, D_005F709C - D_005F7098);
        D_005F70A6 = n + 1;
    }
}

void Bs_PutBits(u8 v, s32 n) {
    *D_005F70A0 &= -(1 << D_005F70A4);
    D_005F70A4 -= n;
    *D_005F70A0 |= (v & ((1 << n) - 1)) << D_005F70A4;
    if (D_005F70A4 == 0) {
        if (*D_005F70A0 == 0xFF) {
            func_003E8B48();
            *D_005F70A0 = 0;
        }
        func_003E8B48();
        D_005F70A4 = 8;
    }
}

void func_003E8C40(void) {
    if (D_005F70A4 != 8) {
        Bs_PutBits(0xFF, D_005F70A4);
    }
}

s32 Bs_GetBits(s32 n) {
    u8 c = *D_005F70A0;
    s32 v;

    D_005F70A4 -= n;
    v = (c >> D_005F70A4) & ((1 << n) - 1);
    if (D_005F70A4 == 0) {
        if (*D_005F70A0 == 0xFF) {
            func_003E8B48();
        }
        func_003E8B48();
        D_005F70A4 = 8;
    }
    return v;
}

void func_003E8CE0(void) {
    if (D_005F70A4 != 8) {
        func_003E8B48();
        D_005F70A4 = 8;
    }
}

void func_003E8D18(s32 v, s32 n) {
    s32 b = D_005F70A4;

    if (n <= b) {
        Bs_PutBits(v, n);
        return;
    }
    n -= b;
    Bs_PutBits(v >> n, b);
    func_003E8D18(v, n);
}

void func_003E8D80(u8 v) {
    func_003E8C40();
    *D_005F70A0 = v;
    func_003E8B48();
}

void func_003E8DB0(u32 v) {
    func_003E8C40();
    *D_005F70A0 = v >> 8;
    func_003E8B48();
    *D_005F70A0 = v;
    func_003E8B48();
}

void func_003E8DF0(u8 *p, s32 n) {
    func_003E8C40();
    while (n != 0) {
        n--;
        *D_005F70A0 = *p++;
        func_003E8B48();
    }
}

s32 Bs_GetBitsN(s32 n) {
    s32 b = D_005F70A4;
    s32 v;

    if (n <= b) {
        return Bs_GetBits(n);
    }
    n -= b;
    v = Bs_GetBits(b);
    return (v << n) | Bs_GetBitsN(n);
}

s32 func_003E8EB0(void) {
    u8 c = *D_005F70A0;
    s32 v;

    D_005F70A4--;
    v = (c >> D_005F70A4) & 1;
    if (D_005F70A4 == 0) {
        if (*D_005F70A0 == 0xFF) {
            func_003E8B48();
        }
        func_003E8B48();
        D_005F70A4 = 8;
    }
    return v;
}

PORT_RET(u8) func_003E8F20(void) {
    u8 v;

    func_003E8CE0();
    v = *D_005F70A0;
    func_003E8B48();
    return v;
}

void func_003E8F58(s32 n) {
    func_003E8CE0();
    while (n != 0) {
        n--;
        func_003E8B48();
    }
}

void func_003E8F98(void) {
}

/* VU0 sincos of x (cos in vf8x, sin in vf8y), the libvu0 _sceVu0ecossin series [I]. */
#ifndef PORT /* hand-written EE / VU0 assembly: the PC build has its own (port/src/vu0_pc.c) */
void func_003E8FA0(f32 x) {
    f32 h = 1.5707964f;
    s32 neg;

    if (x < 0.0f) {
        neg = 1;
        x += h;
    } else {
        x = h - x;
        neg = 0;
    }
    __asm__ volatile("mfc1 $8, %0\n"
                     "qmtc2.ni $8, $vf10\n"
                     "lui $8, %%hi(D_0046BB60)\n"
                     "addiu $8, $8, %%lo(D_0046BB60)\n"
                     "lqc2 $vf9, 0x0($8)\n"
                     "vmr32.w $vf10, $vf10\n"
                     "vaddx.y $vf8, $vf0, $vf10x\n"
                     "vmul.x $vf10, $vf10, $vf10\n"
                     "vmulw.xyzw $vf9, $vf9, $vf10w\n"
                     "vmulx.xyzw $vf9, $vf9, $vf10x\n"
                     "vmulx.xyz $vf9, $vf9, $vf10x\n"
                     "vaddw.y $vf8, $vf8, $vf9w\n"
                     "vmulx.xy $vf9, $vf9, $vf10x\n"
                     "vaddz.y $vf8, $vf8, $vf9z\n"
                     "vmulx.x $vf9, $vf9, $vf10x\n"
                     "vaddy.y $vf8, $vf8, $vf9y\n"
                     "vaddx.y $vf8, $vf8, $vf9x\n"
                     "vaddaw.y $ACC, $vf0, $vf0w\n"
                     "vmsuby.y $vf9, $vf8, $vf8y\n"
                     ".word 0x4A8903BD\n" /* vsqrt $Q, $vf9y */
                     "vwaitq\n"
                     ".word 0x4848B000\n" /* cfc2.ni $8, $vi22, as data: no gas hazard nop */
                     "qmtc2.ni $8, $vf9\n"
                     :
                     : "f"(x)
                     : "$8");
    if (neg) {
        __asm__ volatile("vsubx.x $vf8, $vf0, $vf9x\n");
    } else {
        __asm__ volatile("vaddx.x $vf8, $vf0, $vf9x\n");
    }
}
#endif

/* Rotates matrix m about z, x then y by rot into d (VU0 macro code around the sincos helper func_003E8FA0). */
#ifndef PORT /* hand-written EE / VU0 assembly: the PC build has its own (port/src/vu0_pc.c) */
void func_003E9038(BMat d, BMat m, f32 *rot) {
    __asm__ volatile("lqc2 $vf4, 0x0(%0)\n"
                     "lqc2 $vf5, 0x10(%0)\n"
                     "lqc2 $vf6, 0x20(%0)\n"
                     "lqc2 $vf7, 0x30(%0)\n"
                     :
                     : "r"(m));
    func_003E8FA0(rot[2]);
    __asm__ volatile("vaddx.y $vf9, $vf0, $vf8x\n"
                     "vaddy.x $vf9, $vf0, $vf8y\n"
                     "vsubx.x $vf8, $vf0, $vf8x\n"
                     "vmulax.xy $ACC, $vf9, $vf4x\n"
                     "vmaddy.xy $vf4, $vf8, $vf4y\n"
                     "vmulax.xy $ACC, $vf9, $vf5x\n"
                     "vmaddy.xy $vf5, $vf8, $vf5y\n"
                     "vmulax.xy $ACC, $vf9, $vf6x\n"
                     "vmaddy.xy $vf6, $vf8, $vf6y\n"
                     "vmulax.xy $ACC, $vf9, $vf7x\n"
                     "vmaddy.xy $vf7, $vf8, $vf7y\n");
    func_003E8FA0(rot[0]);
    __asm__ volatile("vaddx.z $vf8, $vf0, $vf8x\n"
                     "vsubx.y $vf9, $vf0, $vf8x\n"
                     "vaddy.z $vf9, $vf0, $vf8y\n"
                     "vmulay.yz $ACC, $vf8, $vf4y\n"
                     "vmaddz.yz $vf4, $vf9, $vf4z\n"
                     "vmulay.yz $ACC, $vf8, $vf5y\n"
                     "vmaddz.yz $vf5, $vf9, $vf5z\n"
                     "vmulay.yz $ACC, $vf8, $vf6y\n"
                     "vmaddz.yz $vf6, $vf9, $vf6z\n"
                     "vmulay.yz $ACC, $vf8, $vf7y\n"
                     "vmaddz.yz $vf7, $vf9, $vf7z\n");
    func_003E8FA0(rot[1]);
    __asm__ volatile("vaddy.z $vf8, $vf0, $vf8y\n"
                     "vaddy.x $vf9, $vf0, $vf8y\n"
                     "vsubx.z $vf9, $vf0, $vf8x\n"
                     "vmulaz.xz $ACC, $vf8, $vf4z\n"
                     "vmaddx.xz $vf4, $vf9, $vf4x\n"
                     "vmulaz.xz $ACC, $vf8, $vf5z\n"
                     "vmaddx.xz $vf5, $vf9, $vf5x\n"
                     "vmulaz.xz $ACC, $vf8, $vf6z\n"
                     "vmaddx.xz $vf6, $vf9, $vf6x\n"
                     "vmulaz.xz $ACC, $vf8, $vf7z\n"
                     "vmaddx.xz $vf7, $vf9, $vf7x\n");
    __asm__ volatile("sqc2 $vf4, 0x0(%0)\n"
                     "sqc2 $vf5, 0x10(%0)\n"
                     "sqc2 $vf6, 0x20(%0)\n"
                     "sqc2 $vf7, 0x30(%0)\n"
                     :
                     : "r"(d)
                     : "memory");
}
#endif

/* Loads m into VU0 vf4-vf7, rotates it about x then y by rot and adds pos to the translation row; the result stays
 * in vf4-vf7 for the caller's inline code (func_003ECAD8). */
#ifndef PORT /* hand-written EE / VU0 assembly: the PC build has its own (port/src/vu0_pc.c) */
void func_003E9120(BMat m, f32 *rot, f32 *pos) {
    __asm__ volatile("lqc2 $vf4, 0x0(%0)\n"
                     "lqc2 $vf5, 0x10(%0)\n"
                     "lqc2 $vf6, 0x20(%0)\n"
                     "lqc2 $vf7, 0x30(%0)\n"
                     :
                     : "r"(m));
    func_003E8FA0(rot[0]);
    __asm__ volatile("vaddx.z $vf8, $vf0, $vf8x\n"
                     "vsubx.y $vf9, $vf0, $vf8x\n"
                     "vaddy.z $vf9, $vf0, $vf8y\n"
                     "vmulay.yz $ACC, $vf8, $vf4y\n"
                     "vmaddz.yz $vf4, $vf9, $vf4z\n"
                     "vmulay.yz $ACC, $vf8, $vf5y\n"
                     "vmaddz.yz $vf5, $vf9, $vf5z\n"
                     "vmulay.yz $ACC, $vf8, $vf6y\n"
                     "vmaddz.yz $vf6, $vf9, $vf6z\n"
                     "vmulay.yz $ACC, $vf8, $vf7y\n"
                     "vmaddz.yz $vf7, $vf9, $vf7z\n");
    func_003E8FA0(rot[1]);
    __asm__ volatile("vaddy.z $vf8, $vf0, $vf8y\n"
                     "vaddy.x $vf9, $vf0, $vf8y\n"
                     "vsubx.z $vf9, $vf0, $vf8x\n"
                     "vmulaz.xz $ACC, $vf8, $vf4z\n"
                     "vmaddx.xz $vf4, $vf9, $vf4x\n"
                     "vmulaz.xz $ACC, $vf8, $vf5z\n"
                     "vmaddx.xz $vf5, $vf9, $vf5x\n"
                     "vmulaz.xz $ACC, $vf8, $vf6z\n"
                     "vmaddx.xz $vf6, $vf9, $vf6x\n"
                     "vmulaz.xz $ACC, $vf8, $vf7z\n"
                     "vmaddx.xz $vf7, $vf9, $vf7x\n");
    __asm__ volatile("lqc2 $vf12, 0x0(%0)\n"
                     "vadd.xyz $vf7, $vf7, $vf12\n"
                     :
                     : "r"(pos));
}
#endif

/* Rotates m about y by -rot[3], x by rot[0] and y by -rot[1] into d. */
#ifndef PORT /* hand-written EE / VU0 assembly: the PC build has its own (port/src/vu0_pc.c) */
void func_003E91D0(BMat d, BMat m, f32 *rot) {
    __asm__ volatile("lqc2 $vf4, 0x0(%0)\n"
                     "lqc2 $vf5, 0x10(%0)\n"
                     "lqc2 $vf6, 0x20(%0)\n"
                     "lqc2 $vf7, 0x30(%0)\n"
                     :
                     : "r"(m));
    func_003E8FA0(-rot[3]);
    __asm__ volatile("vaddy.z $vf8, $vf0, $vf8y\n"
                     "vaddy.x $vf9, $vf0, $vf8y\n"
                     "vsubx.z $vf9, $vf0, $vf8x\n"
                     "vmulaz.xz $ACC, $vf8, $vf4z\n"
                     "vmaddx.xz $vf4, $vf9, $vf4x\n"
                     "vmulaz.xz $ACC, $vf8, $vf5z\n"
                     "vmaddx.xz $vf5, $vf9, $vf5x\n"
                     "vmulaz.xz $ACC, $vf8, $vf6z\n"
                     "vmaddx.xz $vf6, $vf9, $vf6x\n"
                     "vmulaz.xz $ACC, $vf8, $vf7z\n"
                     "vmaddx.xz $vf7, $vf9, $vf7x\n");
    func_003E8FA0(rot[0]);
    __asm__ volatile("vaddx.z $vf8, $vf0, $vf8x\n"
                     "vsubx.y $vf9, $vf0, $vf8x\n"
                     "vaddy.z $vf9, $vf0, $vf8y\n"
                     "vmulay.yz $ACC, $vf8, $vf4y\n"
                     "vmaddz.yz $vf4, $vf9, $vf4z\n"
                     "vmulay.yz $ACC, $vf8, $vf5y\n"
                     "vmaddz.yz $vf5, $vf9, $vf5z\n"
                     "vmulay.yz $ACC, $vf8, $vf6y\n"
                     "vmaddz.yz $vf6, $vf9, $vf6z\n"
                     "vmulay.yz $ACC, $vf8, $vf7y\n"
                     "vmaddz.yz $vf7, $vf9, $vf7z\n");
    func_003E8FA0(-rot[1]);
    __asm__ volatile("vaddy.z $vf8, $vf0, $vf8y\n"
                     "vaddy.x $vf9, $vf0, $vf8y\n"
                     "vsubx.z $vf9, $vf0, $vf8x\n"
                     "vmulaz.xz $ACC, $vf8, $vf4z\n"
                     "vmaddx.xz $vf4, $vf9, $vf4x\n"
                     "vmulaz.xz $ACC, $vf8, $vf5z\n"
                     "vmaddx.xz $vf5, $vf9, $vf5x\n"
                     "vmulaz.xz $ACC, $vf8, $vf6z\n"
                     "vmaddx.xz $vf6, $vf9, $vf6x\n"
                     "vmulaz.xz $ACC, $vf8, $vf7z\n"
                     "vmaddx.xz $vf7, $vf9, $vf7x\n");
    __asm__ volatile("sqc2 $vf4, 0x0(%0)\n"
                     "sqc2 $vf5, 0x10(%0)\n"
                     "sqc2 $vf6, 0x20(%0)\n"
                     "sqc2 $vf7, 0x30(%0)\n"
                     :
                     : "r"(d)
                     : "memory");
}
#endif

/* d = a + b * t (VU0 macro mode, inline asm as in the original's libvu0-style helpers [I]) */
#ifndef PORT /* hand-written EE / VU0 assembly: the PC build has its own (port/src/vu0_pc.c) */
void func_003E92C0(f32 *d, f32 *a, f32 *b, f32 t) {
    __asm__ volatile("lqc2 $vf12, 0x0(%1)\n"
                     "mfc1 $8, %3\n"
                     "qmtc2.ni $8, $vf16\n"
                     "lqc2 $vf14, 0x0(%2)\n"
                     "vaddax.xyzw $ACC, $vf12, $vf0x\n"
                     "vmaddx.xyzw $vf15, $vf14, $vf16x\n"
                     "sqc2 $vf15, 0x0(%0)\n"
                     :
                     : "r"(d), "r"(a), "r"(b), "f"(t)
                     : "$8", "memory");
}
#endif

void func_003E92E0(s16 *p, s32 n) {
    s32 i;
    s32 j;
    s32 k;

    for (i = 0; i < n; i++) {
        p[i] = -1;
    }
    for (i = 0; i < n; i++) {
        k = (f32)(rand() & 0xFFFFFF) * 5.9604645e-08f * (f32)(n - i);
        for (j = 0; j <= k; j++) {
            if (p[j] >= 0) {
                k++;
            }
        }
        p[k] = i;
    }
}

s32 func_003E93F8(void) {
    return D_005F70AC;
}

s32 func_003E9400(void) {
    if (D_005F70A8 == 1 || D_005F70A8 == 9) {
        return 1;
    }
    return 0;
}

BowlLane *func_003E9428(void) {
    return &D_01FC62A0;
}

BowlPlayer *func_003E9438(s32 i) {
    return &D_01FC61E0[i];
}

s32 func_003E9450(void) {
    return D_01FC61E0[D_005F70AC].unk0 & 1;
}

s32 func_003E9478(s32 i) {
    if (D_01FC62A0.unk0 >= 4 || D_005F70AA >= 0x12D || (D_005F70AA & 0x30) != 0) {
        return D_01FC5260[i].unk170;
    }
    return 0;
}

s32 func_003E94D0(void) {
    return gMode != 4 || D_0043B7F8[0] != 8;
}

s32 func_003E9500(void) {
    if (gMode != 4 || D_0043B7F8[0] != 8) {
        return 1;
    }
    return D_005F70D6;
}

void func_003E9530(s16 v) {
    D_005F70D4 = v;
}

s32 func_003E9538(s32 i) {
    return D_005237D8[i];
}

s32 Bowl_SetFighterMove(Fighter *f, s32 c) {
    s32 mv;
    s32 len;

    if (f->unk2A == 15) {
        mv = D_00523590[D_005237B0[f->unk28]][c];
    } else {
        mv = D_00523590[f->unk2A][c];
    }
    len = func_00364088(mv);
    func_00364080(mv);
    if (c == 0) {
        f->unkC4 = f->unkBC = D_005F70D4 + len;
    } else {
        f->unkC4 = f->unkBC = D_005F70D4;
    }
    Fighter_RunMoveEvents(f);
    f->unkC6 = f->unkC4;
    return f->unkC4 >= len;
}

s32 func_003E9628(Fighter *a, Fighter *b, s32 c) {
    s32 r = 0;

    D_005F5F20 = 2;
    func_003532A0();
    if (a->unk127 != 0) {
        r = Bowl_SetFighterMove(a, c);
        func_0034E8B0(a);
    }
    if (b->unk127 != 0) {
        r |= Bowl_SetFighterMove(b, c);
        func_0034E8B0(b);
    }
    if (D_005F5EE0 == 0) {
        D_005F70D4++;
    }
    return r;
}

s32 func_003E96D8(Fighter *f, s32 mv) {
    s16 t = D_005F70D4;

    D_005F5F20 = 2;
    if (func_00364088(mv) < t) {
        D_005F70D4 = 1;
    }
    f->unkC4 = f->unkBC = D_005F70D4;
    if (mv < 8) {
        f->unkB074[0] = f->unkB074[1] = mv + 0x32;
        f->unkB06C[0] = f->unkB06C[1] = f->unkC4;
        D_005F70D6 = 1;
    } else if (f->unkC4 == 1) {
        func_00362E90(f, 3, 0, 1);
        D_005F70D6 = 0;
    }
    func_00364080(mv);
    func_0034E8B0(f);
    D_005F70D6 = 0;
    D_005F70D4++;
    return func_00364088(mv) < D_005F70D4;
}

void func_003E97C8(s32 i, s32 port) {
    func_0035C2F8(i);
    func_003BB218(i, port);
}

#if 0
/* NON_MATCHING: 17 lines: the .rodata vector copy and the camera vector are scheduled differently */
void func_003E9808(void) {
    BVec8 d = D_00523820;
    BVec8 e;
    BVec8 t;
    s32 i;

    t.v[0] = D_00415808[5];
    t.v[1] = -(f32)D_00415808[6];
    t.v[2] = -(f32)D_00415808[7] + 8000.0f;
    t.v[3] = 0.0f;
    e = t;
    if (D_005F70AA == 20 && D_005F70C0 == 0) {
        Snd_PlaySe(0x1057, 0);
    }
    if (D_005F70C8 == 0) {
        for (i = 0; i < 10; i++) {
            if (D_01FC5260[i].unk178 == 0) {
                break;
            }
        }
        if (i < 10) {
            D_005F70C8 = 1;
            if (D_005F70C0 != 0) {
                Snd_PlaySe(0x1057, 0);
            }
            if (func_003F1EE8()) {
                func_0034A868(e.v, e.v);
                func_003E92C0(d.v, d.v, e.v, 300.0f);
                Snd_PlaySe(0x10A8, 0);
                func_003A2298(d.v);
                func_003E97C8(1, D_005F70AC);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003E9808);
#endif

void func_003E9980(void) {
    func_0035B4B0(D_00415808, &D_00436530);
    func_003671D0(D_005F70B0);
    func_00341218();
    func_003D1040();
    Wind_Update();
}

void func_003E99C0(Fighter *f, s32 side, s32 member, s32 port) {
    s32 m;

    func_0035C8D0(f, D_00440940[side][member].unk0, D_00440940[side][member].unk4);
    f->unk22 = member * 2 + side;
    func_003ADB98(f);
    func_00368168(f);
    Tag_SetCounter(port, 2);
    m = func_00367310(f->unk22);
    func_003AF540(f, 0, m);
    func_003AFDA8(port, f->unk22, m);
    Fx_LoadFighterTex(f);
}

void func_003E9A90(void) {
    switch (D_005F5CB6) {
    case 0x11:
        D_005F70B0 = 0.0f;
        break;
    case 0x14:
        D_005F70B0 = 3.1415927f;
        break;
    case 8:
        D_005F70B0 = -1.5707964f;
        break;
    default:
        D_005F70B0 = 0.0f;
        break;
    }
}

void func_003E9AE8(void) {
    BowlPlayer *q; /* fake (decomp-permuter): the second pointer gives the original row registers */
    BowlPlayer *p = D_01FC61E0;
    s32 i;
    s32 j;

    for (i = 2; i > 0; i--) {
        p->unk0 = 2;
        for (j = 0; j < 12; j++) {
            p->unk1[j] = 0;
        }
        for (j = 0; j < 26; j++) {
            p->unk27[j] = 0;
        }
        p->unkD[1] = 0;
        p->unkD[0] = 0;
        q = p;
        for (j = 2; j < 26; j++) {
            q->unkD[j] = -1;
        }
        q->unk42[0] = 0;
        for (j = 1; j < 11; j++) {
            q->unk42[j] = -1;
        }
        for (j = 0; j < 3; j++) {
            q->unk58[j] = -1;
            q->unk5B[j] = 0;
        }
        p++;
    }
}

void func_003E9C10(void) {
    D_01FC51B0.unk0 = 0;
    D_01FC51B0.unk4 = -2300.0f;
    D_01FC51B0.unk8 = -12000.0f;
    D_01FC51B0.unk10 = -0.2f;
    D_01FC51B0.unk20 = 2000.0f;
    D_01FC51B0.unk24 = 0;
    D_01FC51B0.unkC = 0;
    D_01FC51B0.unk14 = 0;
    do { /* fake (decomp-permuter): the loop notes keep the last zero stores in source order */
        D_01FC51B0.unk18 = 0;
        D_01FC51B0.unk1C = 0;
    } while (0);
}

#if 0
/* NON_MATCHING: 198 lines (224 instructions, draft 210): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003E9C58(Fighter *arg0, void *arg1) {
    f32 temp_f0;
    f32 var_f0;
    f32 var_f20;
    s16 temp_a0;
    s16 var_s3;
    u8 *temp_a0_2;
    u8 var_v0;
    u8 var_v0_2;
    u8 var_v0_3;
    u8 var_v1;
    u8 var_v1_2;

    var_s3 = arg0->unk2A;
    if ((var_s3 == 0xF) || (var_s3 == 0x21)) {
        func_0035C850(arg0, 1);
        func_00368168(arg0);
        var_s3 = (s16) D_005237B0[arg0->unk28];
    }
    temp_f0 = ((f32) (rand() & 0xFFFFFF) * 5.9604645e-8f) - 0.5f;
    arg0->unk524 = -0x2AF8;
    var_f20 = (f32) (*(s16 *)((u8 *)(D_00523618[var_s3]) + 0));
    arg0->unk8 = 0;
    arg0->unkC = -0x2AF8;
    arg0->unk18 = 0;
    arg0->unk3C = 0;
    arg0->unk1A = 0;
    arg0->unk1C = 0;
    arg0->unk520 = 0;
    temp_f0 = temp_f0 * 8.0f * 16.0f; /* T41 b1: the draft stored the bits of 16.0f, the asm the integer */
    arg0->unk51C = (s32)temp_f0;
    arg0->unk4 = (s32)temp_f0;
    if (var_f20 > 400.0f) {
        var_f20 = ((var_f20 - 400.0f) * 0.3f) + 400.0f;
    }
    (*(s16 *)((u8 *)(arg1) + 0)) = 0;
    var_f0 = ((f32) (rand() & 0xFFFFFF) * 5.9604645e-8f) - 0.5f;
    (*(f32 *)((u8 *)(arg1) + 4)) = var_f0 + var_f0;
    temp_a0 = arg0->unk28;
    if ((temp_a0 == 0x10) || (temp_a0 == 0x13)) {
        var_f0 = 0.048f;
    } else {
        var_f0 = 0.06f;
    }
    (*(f32 *)((u8 *)(arg1) + 8)) = (f32) (((var_f20 * var_f0) / 200.0f) + 0.03f);
    if ((((f32) (rand() & 0xFFFFFF) * 5.9604645e-8f) - 0.5f) > 0.0f) {
        (*(f32 *)((u8 *)(arg1) + 8)) = (f32) -(*(f32 *)((u8 *)(arg1) + 8));
    }
    temp_a0_2 = D_00523618[var_s3];
    (*(f32 *)((u8 *)(arg1) + 0xC)) = 0.0f;
    (*(f32 *)((u8 *)(arg1) + 0x14)) = (f32) (*(s16 *)((u8 *)(temp_a0_2) + 0));
    var_f0 = var_f20 + 180.0f;
    (*(f32 *)((u8 *)(arg1) + 0x10)) = (var_f0 + var_f0) / 340.0f;
    if (arg0->unk2A != 0x21) {
        var_v0 = temp_a0_2[2];
    } else {
        var_v0 = var_s3 != 4;
    }
    if (var_v0 != 0) {
        D_01FC6350.unkD = 5;
        D_01FC6350.unkC = 4;
        D_01FC6350.unkE = 6;
        D_01FC6350.unkF = 7;
        var_v1 = 0x6D;
        if (D_00523618[arg0->unk2A][3] != 0) {
            var_v0_2 = 0x71;
        } else {
            var_v1 = 0x6B;
            var_v0_2 = 0x6F;
        }
        D_01FC6350.unk11 = var_v0_2;
        D_01FC6350.unk10 = var_v1;
        D_01FC6350.m[0] = (f32 *) &arg0->pad1C30[0x2240];
        D_01FC6350.m[2] = (f32 *) &arg0->pad1C30[0x29C0];
        D_01FC6350.unk12 = 1;
        D_01FC6350.m[1] = (f32 *) &arg0->pad1C30[0x2780];
    } else {
        D_01FC6350.unkD = 1;
        D_01FC6350.unkE = 2;
        D_01FC6350.unkF = 3;
        D_01FC6350.unkC = 0;
        var_v1_2 = 0x6C;
        if (D_00523618[arg0->unk2A][3] != 0) {
            var_v0_3 = 0x70;
        } else {
            var_v1_2 = 0x6A;
            var_v0_3 = 0x6E;
        }
        D_01FC6350.unk11 = var_v0_3;
        D_01FC6350.unk10 = var_v1_2;
        D_01FC6350.m[0] = (f32 *) &arg0->pad1C30[0xF80];
        D_01FC6350.m[2] = (f32 *) &arg0->pad1C30[0x1700];
        D_01FC6350.m[1] = (f32 *) &arg0->pad1C30[0x14C0];
        D_01FC6350.unk12 = 0;
    }
    D_005F70BE = 0;
    D_005F70D4 = 1;
    D_005F70C0 = 0;
    func_003E1BC8();
}
#else
LIT4_WORD(D_005F5984, 0x3E99999A);
LIT4_WORD(D_005F5988, 0x3D449BA6);
LIT4_WORD(D_005F598C, 0x3D75C28F);
LIT4_WORD(D_005F5990, 0x3CF5C28F);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003E9C58);
#endif

void func_003E9FD8(void) {
    D_01FC51E0.unk78 = 0;
    D_01FC51E0.unk4 = D_01FC51E0.unkC = 0.0f;
    D_01FC51E0.unk10 = D_01FC51E0.unk14 = D_01FC51E0.unk18 = D_01FC51E0.unk1C = 0.0f;
    D_01FC51E0.unk20 = D_01FC51E0.unk24 = D_01FC51E0.unk28 = D_01FC51E0.unk2C = 0.0f;
    D_01FC51E0.unk50 = D_01FC51E0.unk54 = D_01FC51E0.unk58 = D_01FC51E0.unk5C = 0.0f;
    D_01FC51E0.unk64 = D_01FC51E0.unk68 = D_01FC51E0.unk6C = 0.0f;
    D_01FC51E0.unk60 = D_01FC51E0.unk0 = 0.0f;
    D_01FC51E0.unk74 = 1;
    D_01FC51E0.unk70 = 0;
    D_01FC51E0.unk8 = -11000.0f;
}

/* The 9th pool load (case 6) stays in front of its `b`: LIT4_KEEP. */
LIT4_KEEP(9);
void Bowl_PinHomePos(f32 *p, u32 i) {
    p[1] = -250.6579f;
    p[3] = 1.0f;
    switch (i) {
    case 0:
        p[0] = 0.0f;
        p[2] = -788.08313f;
        break;
    case 1:
        p[0] = -227.5f;
        p[2] = -394.04156f;
        break;
    case 2:
        p[0] = 227.5f;
        p[2] = -394.04156f;
        break;
    case 3:
        p[0] = -455.0f;
        p[2] = 0.0f;
        break;
    case 4:
    default:
        p[0] = p[2] = 0.0f;
        break;
    case 5:
        p[0] = 455.0f;
        p[2] = 0.0f;
        break;
    case 6:
        p[0] = -682.5f;
        p[2] = 394.04156f;
        break;
    case 7:
        p[0] = -227.5f;
        p[2] = 394.04156f;
        break;
    case 8:
        p[0] = 227.5f;
        p[2] = 394.04156f;
        break;
    case 9:
        p[0] = 682.5f;
        p[2] = 394.04156f;
        break;
    }
    p[0] += 0.0f;
    p[2] += 8000.0f;
}
LIT4_KEEP(0);

#if 0
/* NON_MATCHING: 21 lines: store order of the vector resets and register choice of the zero loops */
void Bowl_ResetPins(void) {
    BowlBody *p = D_01FC5260;
    s32 i;
    s32 j;
    s32 k;

    for (i = 0; i < 10; i++) {
        Bowl_PinHomePos(p->unk0.v, i);
        p->unk40.v[3] = p->unk40.v[0] = p->unk40.v[2] = 0.0f; 
        p->unk40.v[1] = 3.1415927f; 
        p->unk10.v[3] = p->unk10.v[0] = p->unk10.v[1] = p->unk10.v[2] = 0.0f; 
        p->unk50.v[3] = p->unk50.v[0] = p->unk50.v[1] = p->unk50.v[2] = 0.0f; 
        p->unk20.q = p->unk0.q;
        p->unk60.q = p->unk40.q;
        func_003ECAD8(p);
        p->unkC0.m[0].q = p->unk80.m[0].q;
        p->unkC0.m[1].q = p->unk80.m[1].q;
        p->unkC0.m[2].q = p->unk80.m[2].q;
        p->unk150.q = p->unk140.q;
        for (j = 0; j < 4; j++) {
            for (k = 0; k < 4; k++) {
                p->unk100[j].v[k] = 0.0f;
            }
        }
        for (k = 0; k < 4; k++) {
            p->unk160.v[k] = 0.0f;
        }
        p->unk17C = 0;
        p->unk170 = p->unk174 = p->unk178 = 1;
        p->unk17E = 0;
        p++;
    }
    D_005F70D2 = 0;
    D_005F70B4 = 10;
    D_005F70D0 = 0;
}
#else
LIT4_WORD(D_005F59CC, 0x40490FDB);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", Bowl_ResetPins);
#endif

void Bowl_NextBall(BowlPlayer *p) {
    s32 r = 2;
    s32 n = p->unk0;

    if (n >= 20) {
        if (p->unk42[10] >= 0) {
            r = 0;
        } else if (D_005F70B4 == 0) {
            r = 1;
        }
    } else if ((n & 1) || D_005F70B4 == 0) {
        r = 0;
    }
    if (r == 2 && p->unk27[n] == 5) {
        r = 1;
    }
    if ((n & 1) || D_005F70B4 != 0) {
        p->unk0++;
    } else {
        p->unk0 += 2;
    }
    switch (r) {
    case 0:
        D_005F70AC ^= 1;
    case 1:
        Bowl_ResetPins();
        break;
    case 2:
        break;
    }
}

void func_003EA378(u16 *p, s32 i, s32 v) {
    if (i < 11) {
        p[i] = p[i - 1] + v;
    }
}

s32 Bowl_IsSplit(void) {
    f32 lim = 500.0f;
    BowlBody *p = D_01FC5260;
    BowlBody *q;
    s32 i;
    s32 j;
    s32 hr;
    s32 hl;
    f32 r;
    f32 l;
    f32 dx;

    if (p->unk170 != 0) {
        return 0;
    }
    p++;
    for (i = 1; i < 10; i++, p++) {
        if (p->unk170 == 0) {
            continue;
        }
        q = &D_01FC5260[1];
        j = 1;
        r = 1820.0f;
        l = -r;
        hr = 0;
        hl = 0;
        for (; j < 10; j++, q++) {
            if (q == p || q->unk170 == 0) {
                continue;
            }
            dx = q->unk0.v[0] - p->unk0.v[0];
            if (dx > 0.0f && dx < r) {
                r = dx;
                hr = 1;
            } else if (dx < 0.0f && l < dx) {
                l = dx;
                hl = 1;
            }
        }
        if ((hr && r > lim) || (hl && -l > lim)) {
            return 1;
        }
    }
    return 0;
}

#if 0
/* NON_MATCHING: written from the asm (T41 b1; the m2c draft stepped its BowlBody pointer by 0x180 elements, not
   bytes, and returned nothing) */
/* After a ball: the pins that fell are set back (and counted into D_005F70B4), the ball's result goes into the score
 * sheet p (+0x0D pins, +0x27 mark: 1 / 2 gutter, 3 / 4 strike / spare, 5 foul, 6 split), the past balls shift, and the
 * frame totals of a strike / spare waiting for this ball are completed (func_003EA378). Returns 1 when this was the
 * first ball of a frame and all ten pins fell. */
s32 func_003EA4C8(BowlPlayer *p) {
    BowlBody *b = D_01FC5260;
    s32 n = p->unk0;
    s32 early = n < 0x14;
    s32 left;
    s32 odd;
    s32 i;
    s32 k;
    s32 m;
    s32 m2;
    s32 cur;
    s32 a;
    s32 c;
    s16 cnt;

    if (D_005F70C0 != 0) {
        p->unk27[n] = 5;
        left = 0;
    } else {
        left = D_005F70B4;
        cnt = 0;
        D_005F70B4 = 0;
        odd = n & 1;
        for (i = 0; i < 10; i++, b++) {
            if (b->unk170 != 0) {
                Bowl_PinHomePos(b->unk0.v, i);
                *(s32 *)&b->unk40.v[0] = 0;
                b->unk10.v[3] = 0.0f;
                b->unk10.v[2] = 0.0f;
                b->unk10.v[1] = 0.0f;
                b->unk10.v[0] = 0.0f;
                b->unk50.v[3] = 0.0f;
                b->unk50.v[2] = 0.0f;
                b->unk50.v[1] = 0.0f;
                b->unk50.v[0] = 0.0f;
                b->unk20.q = b->unk0.q;
                b->unk60.q = b->unk40.q;
                func_003ECAD8(b);
                b->unkC0.m[0].q = b->unk80.m[0].q;
                b->unkC0.m[1].q = b->unk80.m[1].q;
                b->unkC0.m[2].q = b->unk80.m[2].q;
                b->unk150.q = b->unk140.q;
                cnt = D_005F70B4;
                for (k = 0; k < 4; k++) {
                    for (m = 0; m < 4; m++) {
                        *(s32 *)&b->unk100[k].v[m] = 0;
                    }
                }
                for (m = 0; m < 4; m++) {
                    *(s32 *)&b->unk160.v[m] = 0;
                }
                b->unk178 = 1;
                cnt = cnt + 1;
                D_005F70B4 = cnt;
            } else {
                b->unk174 = 0;
            }
        }
        left -= cnt;
        if (cnt == 0) {
            p->unk27[n] = odd ? 2 : 1;
        } else if (left == 0) {
            p->unk27[n] = odd ? 4 : 3;
        } else if (!odd && Bowl_IsSplit() != 0) {
            p->unk27[n] = 6;
        }
    }
    p->unkD[n] = left;
    if (!early) {
        k = 0;
        if (p->unk58[0] >= 0) {
            do {
                k++;
            } while (p->unk58[k] >= 0);
        }
        p->unk58[k] = p->unkD[n];
        p->unk5B[k] = p->unk27[n];
    }
    for (k = 11; k > 0; k--) {
        p->unk1[k] = p->unk1[k - 1];
    }
    p->unk1[0] = p->unk0;
    m2 = p->unk1[1];
    m = p->unk1[2];
    cur = p->unk0;
    a = p->unkD[m2];
    c = p->unkD[m];
    if (!(m & 1) && c == 10) {
        func_003EA378((u16 *)p->unk42, m >> 1, a + (left + 10));
    }
    if ((m2 & 1) && c + a == 10) {
        func_003EA378((u16 *)p->unk42, m2 >> 1, left + 10);
    }
    if ((cur & 1) && a + left < 10) {
        func_003EA378((u16 *)p->unk42, cur >> 1, a + left);
    }
    if (cur & 1) {
        return 0;
    }
    return left == 10;
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EA4C8);
#endif

void func_003EA868(s32 x, s32 y, s32 full) {
    BowlPlayer *p = &D_01FC61E0[D_005F70AC];
    s32 n = p->unk0;
    s32 cur = (gFrameCount & 0x30) ? n : -50;
    s32 first = (n >> 1) - 3;
    s32 last;

    if (full) {
        first = 1;
        last = 10;
    } else {
        if (first <= 0) {
            first = 1;
        } else if (first >= 8) {
            first = 7;
        }
        last = first + 3;
    }
    if (cur >= 22) {
        if (p->unk58[1] >= 0) {
            cur = 22;
        } else {
            cur = 21;
        }
    }
    cur -= first * 2;
    if (D_005F5CB0[D_005F70AC ^ 1] != 0) {
        func_003C59D8(x, y, 2, first, last, 0, &D_01FC61E0[0], &D_01FC61E0[1], D_005F70AC * 21 + cur);
    } else {
        func_003C59D8(x, y, 1, first, last, 0, p, NULL, cur);
    }
}

#if 0
/* NON_MATCHING: 86 lines (116 instructions, draft 117): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003EA978(void) {
    f32 sp[64]; /* m2c stack */
    u128 sp0; /* m2c stack */
    u128 sp10; /* m2c stack */
    u128 sp20; /* m2c stack */
    s32 temp_t2;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 var_a0;
    s32 var_a0_2;
    s32 var_a2;
    s32 var_t0;
    s32 var_v0;
    s64 temp_a1;
    s64 var_a1;
    void *temp_a0;
    void *temp_a3;
    void *temp_t3;
    void *temp_v1;

    temp_t3 = func_003994C0();
    var_t0 = 0;
    __asm__ volatile("lqc2 $vf4, ($v0)\n" "lqc2 $vf5, 0x10($v0)\n" "lqc2 $vf6, 0x20($v0)\n" "lqc2 $vf7, 0x30($v0)\n");
    var_a0 = 0;
    var_v0 = 0 * 0x10;
    do {
        temp_t2 = var_a0 + 1;
        temp_a3 = var_v0 + &D_0046BBB0;
        var_a2 = 0;
        var_a1 = 0;
loop_2:
        temp_v1 = (var_a1 * 0x10) + sp;
        __asm__ volatile("lqc2 $vf12, ($a3)\n" "lqc2 $vf14, ($v0)\n" "vadd.xyzw $vf12, $vf12, $vf14\n" "vmulax.xyzw $ACC, $vf4, $vf12x\n" "vmadday.xyzw $ACC, $vf5, $vf12y\n" "vmaddaz.xyzw $ACC, $vf6, $vf12z\n" "vmaddw.xyzw $vf13, $vf7, $vf12w\n" "vdiv $Q, $vf0w, $vf13w\n" "vwaitq\n" "vmulq.xyz $vf18, $vf13, $Q\n" "vaddx.w $vf18, $vf0, $vf0x\n" "vftoi4.xyzw $vf20, $vf18\n" "sqc2 $vf20, ($v1)\n");
        var_a0_2 = 0;
        if (((u32) (*(u32 *)((u8 *)(temp_v1) + 0)) > 0xFFFFU) || (temp_v0 = (*(s32 *)((u8 *)(temp_v1) + 4)), (temp_v0 < 0)) || (temp_v0 > 0xFFFF) || (temp_v0_2 = (*(s32 *)((u8 *)(temp_v1) + 8)), (temp_v0_2 < 0)) || (temp_v0_2 > 0xFFFFF)) {
            var_a0_2 = 1;
        }
        var_a1 += 1;
        var_a2 |= var_a0_2;
        if (var_a1 < 3) {
            goto loop_2;
        }
        var_a0 = temp_t2;
        if (var_a2 == 0) {
            temp_a0 = (var_t0 << 6) + temp_t3;
            var_t0 += 1;
            (*(u128 *)((u8 *)(temp_a0) + 0x40)) = D_0046BB70.q;
            (*(u128 *)((u8 *)(temp_a0) + 0x50)) = sp0;
            (*(u128 *)((u8 *)(temp_a0) + 0x60)) = sp10;
            (*(u128 *)((u8 *)(temp_a0) + 0x70)) = sp20;
            var_a0 = temp_t2;
        }
        var_v0 = var_a0 * 0x10;
    } while (var_a0 < 7);
    if (var_t0 == 0) {
        func_00399580(temp_t3, var_a1, var_a2, temp_a3);
        return;
    }
    temp_a1 = (0x10000000 << 0x20) | 2;
    (*(s64 *)((u8 *)(temp_t3) + 0)) = temp_a1;
    (*(s64 *)((u8 *)(temp_t3) + 8)) = 0xE;
    (*(s64 *)((u8 *)(temp_t3) + 0x10)) = 0x50000;
    (*(s64 *)((u8 *)(temp_t3) + 0x18)) = 0x48;
    (*(s64 *)((u8 *)(temp_t3) + 0x20)) = 0x44;
    (*(s64 *)((u8 *)(temp_t3) + 0x28)) = 0x43;
    (*(s64 *)((u8 *)(temp_t3) + 0x30)) = (s64) (var_t0 | 0x8000 | (0x4141C000 << 0x20));
    (*(s64 *)((u8 *)(temp_t3) + 0x38)) = 0x4441;
    func_00399580(temp_t3 + ((var_t0 << 6) + 0x40), temp_a1, 0x50000, (void *)0x48);
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EA978);
#endif

void func_003EAB48(void) {
    s32 a;
    s32 b;
    s32 x;
    s32 c;
    s32 y;
    s32 port;

    if (D_01FC62A0.unk0 < 4 && D_005F70AA < 0x12D && !(gFrameCount & 0x10)) {
        if (Pad_IsMultitap()) {
            a = 0x31;
            b = 0x2D;
            c = 0x41;
            x = 0x56;
        } else {
            a = 0x20;
            b = 0x50;
            c = 0x31;
            x = 0x4C;
        }
        y = 0x74;
        if (D_005F5CB0[D_005F70AC ^ 1] != 0) {
            y = 0xA2;
        }
        port = func_003AFFB8(D_005F70AC, gFighters[D_005F70AC].unk0);
        Text_Printf(D_00523858, x, y, 10, 6, Pad_GetPlayerMode(port) >= 0 ? 0xB : 0x16, a, x + 0x10, b, c + port);
    }
}

void func_003EAC78(void) {
    D_005F70DA = 0;
    D_005F70D8 = 0;
}

void Bowl_UpdateSpeed(void) {
    f32 v = D_01FC51E0.unk18;

    if (D_005F70C0 == 0) {
        v *= 0.216f;
        if (v > 20.0f) {
            v -= 6.0f;
        } else {
            v *= 0.7f;
        }
        v *= 0.625f;
        D_005F70D8 = v;
        D_005F70DA = (v - D_005F70D8) * 100.0f;
    }
}

void Bowl_DrawSpeed(void) {
    Text_Printf(D_00523870, 0x1D0, 0x18C, 10, 4, 5);
    if (D_005F70C0 != 0) {
        Text_Printf(D_00523880, 0x1C0, 0x19C, 10, 5, 5, 4);
        Text_Printf(D_00523890, 0x200, 0x1A1, 10, 4, 5);
    } else {
        Text_Printf(D_005238A0, 0x1C0, 0x19C, 10, 5, 5, D_005F70D8);
        Text_Printf(D_005238B0, 0x200, 0x1A1, 10, 4, 5, D_005F70DA);
    }
    Text_Printf(D_005238C0, 0x234, 0x1A6, 10, 3, 5);
}

#if 0
/* NON_MATCHING: 49 lines: register allocation (t1/t2) and byte store order of the hand set-up */
s32 func_003EAE08(Fighter *f, s16 *out) {
    BowlHold *h;
    s32 c;

    if (f->unk2A == 0x21) {
        return 0;
    }
    c = f->unk28;
    if (c == 6 || c == 0x1E || c == 0x16 || c == 0xB || c == 2) {
        h = &D_01FC6350;
        if (h->unk12 == 1) {
            h->unkD = 1;
            h->unkF = 3;
            h->unkE = 2;
            h->unkC = 0;
            if (D_00523618[f->unk2A][3]) {
                h->unk11 = 0x70;
                h->unk10 = 0x6C;
            } else {
                h->unk11 = 0x6E;
                h->unk10 = 0x6A;
            }
            h->m[0] = (f32 *)((u8 *)f + 0x2BB0);
            h->m[2] = (f32 *)((u8 *)f + 0x3330);
            h->m[1] = (f32 *)((u8 *)f + 0x30F0);
            h->unk12 = 0;
        } else {
            h->unkD = 5;
            h->unkC = 4;
            h->unkE = 6;
            h->unkF = 7;
            if (D_00523618[f->unk2A][3]) {
                h->unk11 = 0x71;
                h->unk10 = 0x6D;
            } else {
                h->unk11 = 0x6F;
                h->unk10 = 0x6B;
            }
            h->m[0] = (f32 *)((u8 *)f + 0x3E70);
            h->m[2] = (f32 *)((u8 *)f + 0x45F0);
            h->unk12 = 1;
            h->m[1] = (f32 *)((u8 *)f + 0x43B0);
        }
        *out = 0;
        D_005F70BE = 0x78;
        return 1;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EAE08);
#endif

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523858);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523870);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523880);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523890);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005238A0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005238B0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005238C0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005238E0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005238F0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523908);
#if 0
/* NON_MATCHING: 707 lines (769 instructions, draft 743): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
struct _struct_D_005236A0_0x8 {
    /* 0x0 */ char pad0[4];
    /* 0x4 */ u16 unk4;                             /* inferred */
    /* 0x6 */ char pad6[2];
};                                                  /* size = 0x8 */

void func_003EAF78(Fighter *arg0, s16 *arg1, s16 arg2, s16 arg3) {
    f32 sp[64]; /* m2c stack */
    s8 sp20;
    s8 sp21;
    f32 temp_f1;
    f32 temp_f1_2;
    f32 temp_f1_3;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f22;
    f32 temp_f2;
    f32 temp_f6;
    f32 var_f0;
    f32 var_f0_2;
    f32 var_f0_3;
    f32 var_f0_4;
    f32 var_f1;
    f32 var_f1_2;
    f32 var_f1_3;
    s16 temp_s0;
    s16 temp_s0_2;
    s16 temp_s0_3;
    s16 temp_s0_4;
    s16 temp_s0_5;
    s16 temp_s0_7;
    s16 temp_s0_8;
    s16 temp_s2;
    s16 temp_v0_3;
    s16 temp_v0_4;
    s16 temp_v1;
    s16 temp_v1_2;
    s16 temp_v1_3;
    s32 temp_a1;
    s32 temp_a3;
    s32 temp_s0_6;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 temp_v0_5;
    s32 temp_v0_6;
    s32 temp_v1_4;
    s32 temp_v1_5;
    s32 var_a0;
    s32 var_a2;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v1;
    u16 temp_t0;
    u16 temp_v1_6;
    u16 temp_v1_7;
    u16 temp_v1_8;
    u8 var_t0;

    temp_a3 = arg3 << 0x10;
    BOWL_SP(0x0, BowlB8) = *(BowlB8 *)D_005F6998;
    var_a2 = D_005F69A8;
    BOWL_SP(0x10, BowlB8) = *(BowlB8 *)D_005F69A0;
    if (var_a2 > 0) {
        var_a2 -= 1;
        D_005F69A8 = var_a2;
    }
    temp_v1 = (*(s16 *)((u8 *)(arg1) + 0));
    switch (temp_v1) {
    case 0:
        D_005F70D4 = 1;
        D_005F6988 = (s16) D_01FC635C[0];
        var_a2 = 2;
        (*(s16 *)((u8 *)(arg1) + 2)) = 1;
        (*(s16 *)((u8 *)(arg1) + 0)) = (u16) (*(s16 *)((u8 *)(arg1) + 0)) + 1;
        D_005F69A8 = 2;
        /* fallthrough */
    case 1:
        D_01FC51B0.unk24 = 0;
        if (var_a2 <= 0) {
            if (arg3 & 0x800) {
                func_003F3108();
                D_0043B7FA[0] = 0;
                D_005F5F08 = 1 << D_005F70AC;
                func_003B7AC8(D_005F70AC);
                D_005F70A8 = 9;
                goto block_10;
            }
            if (arg3 & 0x10) {
                D_005F70AE = 1;
                D_005F70A8 = 9;
block_10:
                Snd_PlaySe(4, 0);
                Bgm_SetVolumeFade(0x40, 0x78);
            } else {
                goto block_11;
            }
        } else {
block_11:
            if (!(arg3 & 0x100) || (func_003EAE08(arg0, arg1) == 0)) {
                temp_a1 = arg2 & 0x2000;
                var_a0 = arg2 & 0x8000;
                if (temp_a1 != 0) {
                    temp_v0 = arg0->unk4 + 8;
                    arg0->unk4 = temp_v0;
                    if (temp_v0 >= 0x3E9) {
                        arg0->unk4 = 0x3E8;
                        var_a0 = arg2 & 0x8000;
                    }
                }
                if (var_a0 != 0) {
                    temp_v0_2 = arg0->unk4 - 8;
                    arg0->unk4 = temp_v0_2;
                    if (temp_v0_2 < -0x3E8) {
                        arg0->unk4 = -0x3E8;
                    }
                }
                if (temp_a1 != 0) {
                    if (D_005F6988 != D_01FC6350.unkC) {
                        if (D_005F6988 == D_01FC6350.unkD) {
                            temp_s0 = D_005F70D4;
                            if (func_00364088((s32) D_005F6988) < temp_s0) {
                                goto block_23;
                            }
                        }
                    } else {
block_23:
                        var_t0 = D_01FC6350.unkE;
                        D_005F70D4 = 1;
                        goto block_35;
                    }
                } else if (var_a0 != 0) {
                    if (D_005F6988 != D_01FC6350.unkC) {
                        if (D_005F6988 == D_01FC6350.unkE) {
                            temp_s0_2 = D_005F70D4;
                            if (func_00364088((s32) D_005F6988) < temp_s0_2) {
                                goto block_28;
                            }
                        }
                    } else {
block_28:
                        var_t0 = D_01FC6350.unkD;
                        D_005F70D4 = 1;
                        goto block_35;
                    }
                } else {
                    temp_t0 = (u16) D_005F6988;
                    if (D_005F6988 == D_01FC6350.unkE) {
                        temp_s0_3 = D_005F70D4;
                        if (func_00364088((s32) D_01FC6350.unkD) >= temp_s0_3) {
                            goto block_32;
                        }
                        goto block_34;
                    }
block_32:
                    if ((s16) temp_t0 == D_01FC6350.unkD) {
                        temp_s0_4 = D_005F70D4;
                        if (func_00364088((s32) (s16) temp_t0) < temp_s0_4) {
block_34:
                            var_t0 = D_01FC6350.unkC;
                            D_005F70D4 = 1;
block_35:
                            D_005F6988 = (s16) var_t0;
                        }
                    }
                }
                func_003EA868(0x14, 0x14, 0);
                if (arg3 & 0x40) {
                    (*(s16 *)((u8 *)(arg1) + 2)) = 2;
                    (*(s16 *)((u8 *)(arg1) + 0)) = (u16) (*(s16 *)((u8 *)(arg1) + 0)) + 1;
                    func_003E1B40();
                }
            default:
block_110:
                if ((arg3 != 0) && (D_005F70AA >= 0x259)) {
                    D_005F70AA = 0x258;
                }
                temp_v1_2 = (*(s16 *)((u8 *)(arg1) + 0));
                if (temp_v1_2 < 4) {
                    if (D_005F70AA >= 0x4651) {
                        if (temp_v1_2 == 2) {
                            func_003E1BB0();
                        }
                        D_005F70CE = 0x3C;
                        D_005F698A = 1;
                        (*(s16 *)((u8 *)(arg1) + 0)) = 5;
                    }
                }
                func_003E96D8(arg0, (s32) D_005F6988);
                if (D_005F70BE > 0) {
                    D_005F70BE = (u16) D_005F70BE - 1;
                    sp20 = (*(s8 *)((u8 *)(&D_005F69B0) + 0));
                    sp21 = (*(s8 *)((u8 *)(&D_005F69B0) + 1));
                    Text_Printf(D_005238F0, 0xEF, 0x16C, 0xA);
                    Text_Printf(D_00523908, 0xAE, 0x180, 0xA);
                }
                D_005F70AA = (u16) D_005F70AA + 1;
            }
        }
        break;
    case 2:
        D_01FC51B0.unk24 = 1;
        D_005F6988 = (s16) D_01FC6350.unkF;
        D_005F70D4 = 0x6A;
        if (arg2 & 0x2000) {
            temp_v0_3 = (u16) arg0->unk1A + 0x10;
            arg0->unk1A = temp_v0_3;
            if (temp_v0_3 >= 0x801) {
                arg0->unk1A = 0x800;
            }
            arg0->unk3C = (s16) (u16) arg0->unk1A;
        }
        if (arg2 & 0x8000) {
            temp_v0_4 = (u16) arg0->unk1A - 0x10;
            arg0->unk1A = temp_v0_4;
            if (temp_v0_4 < -0x800) {
                arg0->unk1A = -0x800;
            }
            arg0->unk3C = (s16) (u16) arg0->unk1A;
        }
        temp_f2 = (*(f32 *)((u8 *)(arg1) + 8));
        temp_f1 = (*(f32 *)((u8 *)(arg1) + 4)) + temp_f2;
        (*(f32 *)((u8 *)(arg1) + 4)) = temp_f1;
        temp_v1_3 = arg0->unk28;
        if ((temp_v1_3 == 0x10) || (temp_v1_3 == 0x13)) {
            if (!(1.2f < temp_f1)) {
                var_f0 = -1.2f;
                goto block_56;
            }
            goto block_57;
        }
        if (D_01FC6350.unk12 != 0) {
            if (!(temp_f1 > 2.0f)) {
                var_f0 = -1.0f;
                goto block_56;
            }
            goto block_57;
        }
        if (!(temp_f1 > 1.0f)) {
            var_f0 = -2.0f;
block_56:
            var_v0 = arg3 & 0x20;
            if (temp_f1 < var_f0) {
                goto block_57;
            }
        } else {
block_57:
            (*(f32 *)((u8 *)(arg1) + 8)) = (f32) -temp_f2;
            var_v0 = arg3 & 0x20;
        }
        if (var_v0 != 0) {
            D_005F6988 = (s16) D_01FC6350.unkC;
            (*(f32 *)((u8 *)(arg1) + 4)) = 0.0f;
            (*(s16 *)((u8 *)(arg1) + 2)) = 1;
            (*(s16 *)((u8 *)(arg1) + 0)) = (u16) (*(s16 *)((u8 *)(arg1) + 0)) - 1;
            func_003E1BB0((s32 *)1, 0x6A, &D_01FC6350, temp_a3);
        } else if (arg3 & 0x40) {
            (*(s16 *)((u8 *)(arg1) + 2)) = 3;
            (*(s16 *)((u8 *)(arg1) + 0)) = (u16) (*(s16 *)((u8 *)(arg1) + 0)) + 1;
            func_003E1BB0(&D_01FC51B0.unk24, 0x6A, &D_01FC6350, temp_a3);
        }
        goto block_110;
    case 3:
        D_01FC51B0.unk24 = 7;
        temp_f1_2 = (*(f32 *)((u8 *)(arg1) + 0xC));
        D_005F6988 = (s16) D_01FC6350.unkF;
        if (temp_f1_2 < 100.0f) {
            (*(f32 *)((u8 *)(arg1) + 0xC)) = (f32) (temp_f1_2 + (*(f32 *)((u8 *)(arg1) + 0x10)));
        } else {
            (*(f32 *)((u8 *)(arg1) + 0xC)) = 0.0f;
        }
        if (arg3 & 0x40) {
            if (((*(f32 *)((u8 *)(arg1) + 0xC)) > 0.0f) || (rand(), ((*(f32 *)((u8 *)(arg1) + 0x14)) < (f32) 0x44FA0000))) {
                (*(s16 *)((u8 *)(arg1) + 0)) = 4;
                D_005F70D4 = 1;
                D_005F6988 = (s16) D_01FC6350.unkF;
            } else {
                D_005F698A = 0;
                D_005F70CE = 0x3C;
                (*(s16 *)((u8 *)(arg1) + 0)) = 5;
            }
        }
        goto block_110;
    case 4:
        D_01FC51B0.unk24 = 7;
        if (arg2 & 0x2000) {
            temp_v1_4 = arg0->unk4 + (0x500 / (s32) 0 /* m2c: unknown value */);
            arg0->unk4 = temp_v1_4;
            if (temp_v1_4 >= 0x3E9) {
                var_v0_2 = 0x3E8;
                goto block_76;
            }
        } else if (arg2 & 0x8000) {
            temp_v1_5 = arg0->unk4 - (0x500 / (s32) 0 /* m2c: unknown value */);
            arg0->unk4 = temp_v1_5;
            if (temp_v1_5 < -0x3E8) {
                var_v0_2 = -0x3E8;
block_76:
                arg0->unk4 = var_v0_2;
            }
        }
        if (D_005F70D4 == 0x6A) {
            temp_f22 = (*(f32 *)((u8 *)(arg1) + 4)) + ((*(f32 *)((u8 *)(arg1) + 8)) * ((2.0f * ((f32) (rand() & 0xFFFFFF) * 5.9604645e-8f)) - 1.0f));
            temp_v1_6 = (u16) arg0->unk1A;
            temp_f20 = ((*(f32 *)((u8 *)(arg1) + 0x14)) * ((*(f32 *)((u8 *)(arg1) + 0xC)) + ((*(f32 *)((u8 *)(arg1) + 0x10)) * ((2.0f * ((f32) (rand() & 0xFFFFFF) * 5.9604645e-8f)) - 1.0f)) + 100.0f)) / 200.0f;
            if ((s32) temp_v1_6 <= 0x7FFF) {
                var_f1 = (f32) temp_v1_6;
                var_f0_2 = 9.58738e-05f;
            } else {
                var_f1 = 9.58738e-05f;
                var_f0_2 = (f32) (temp_v1_6 + 0xFFFF0000);
            }
            temp_f6 = cosf(var_f0_2 * var_f1);
            if (arg0->unk1A >= 0) {
                __asm__ volatile(".word 0x460000C4\n");
            } else {
                __asm__ volatile(".word 0x46000004\n");
            }
            D_01FC51E0.unk10 = temp_f20 * 0 /* m2c: unknown value */;
            D_01FC51E0.unk18 = temp_f20 * temp_f6;
            D_01FC51E0.unk28 = -temp_f22 * 0 /* m2c: unknown value */;
            D_01FC51E0.unk20 = temp_f22 * temp_f6;
            D_01FC51E0.unk70 = 1;
            D_01FC51E0.unk60 = (temp_f20 * -0.25f) / 200.0f;
            D_01FC51E0.unk68 = temp_f22 * 0.5f;
            D_01FC51E0.unk54 = (f32) arg0->unk1A * 3.1415927f * 0.000030517578f;
            temp_s0_5 = arg0->unk1E;
            temp_s2 = arg0->unk2C;
            Snd_PlayVoice(temp_s0_5, temp_s2, *(((rand() >> 0xF) & 2) + (arg0->unk2A * 8) + D_005236A0));
            D_01FC51B0.unk24 = 2;
            (*(s16 *)((u8 *)(arg1) + 2)) = 4;
            (*(s16 *)((u8 *)(arg1) + 0)) = 7;
            if (D_005F70BC == 0) {
                func_003BB228((u8) D_005F70AC, 5);
            }
        }
        goto block_110;
    case 5:
        if (D_005F70CE > 0) {
            D_005F6988 = (s16) D_01FC635C[0];
            D_005F70CE = (u16) D_005F70CE - 1;
            Text_Printf(D_005238E0, (*(s32 *)((u8 *)((sp + (D_005F698A * 4))) + 0x10)), 0xF0, 0xA);
        } else {
            D_005F70DC = arg0->unk4;
            D_005F70E0 = arg0->unkC;
            D_005F6988 = (s16) D_01FC6360[0];
            D_005F70D4 = 1;
            D_005F70C0 = 1;
            (*(s16 *)((u8 *)(arg1) + 0)) = 6;
            D_01FC51B0.unk24 = 2;
        }
        goto block_110;
    case 6:
        temp_s0_6 = D_005F70D4 - 0x19;
        D_005F6988 = (s16) D_01FC6350.unk10;
        if (temp_s0_6 == 0) {
            Snd_PlayVoice(arg0->unk1E, arg0->unk2C, *(u16 *)&D_005236A0[arg0->unk2A][4], temp_a3);
        }
        if (temp_s0_6 > 0) {
            temp_v1_7 = (u16) arg0->unk1A;
            if ((s32) temp_v1_7 <= 0x7FFF) {
                var_f1_2 = (f32) temp_v1_7;
                var_f0_3 = 9.58738e-05f;
            } else {
                var_f1_2 = 9.58738e-05f;
                var_f0_3 = (f32) (temp_v1_7 + 0xFFFF0000);
            }
            temp_v1_8 = (u16) arg0->unk1A;
            temp_f20_2 = sinf(var_f0_3 * var_f1_2);
            if ((s32) temp_v1_8 <= 0x7FFF) {
                var_f1_3 = (f32) temp_v1_8;
                var_f0_4 = 9.58738e-05f;
            } else {
                var_f1_3 = 9.58738e-05f;
                var_f0_4 = (f32) (temp_v1_8 + 0xFFFF0000);
            }
            temp_f1_3 = temp_f20_2 / cosf(var_f0_4 * var_f1_3);
            var_v1 = temp_s0_6 << 8;
            if (temp_s0_6 >= 0x19) {
                temp_v0_5 = temp_s0_6 - 0x19;
                if (temp_s0_6 < 0x32) {
                    var_v1 = (temp_s0_6 << 8) - (temp_v0_5 * temp_v0_5);
                } else {
                    temp_v0_6 = temp_s0_6 - 0x32;
                    if (temp_s0_6 < 0x4B) {
                        var_v1 = (temp_v0_6 * 0xCE) - ((temp_v0_6 * (temp_v0_6 * 4)) - 0x2F8F);
                    } else {
                        var_v1 = 0x39E9;
                    }
                }
            }
            arg0->unkC = D_005F70E0 + var_v1;
            arg0->unk4 = D_005F70DC + (*(s32 *)&(temp_f1_3));
        }
        temp_s0_7 = D_005F70D4;
        if (func_00364088((s32) D_005F6988) < temp_s0_7) {
            D_005F70D4 = 1;
            D_005F6988 = (s16) D_01FC6350.unk11;
            (*(s16 *)((u8 *)(arg1) + 0)) = 7;
        }
        goto block_110;
    case 7:
        if (D_005F6988 == D_01FC6350.unkF) {
            temp_s0_8 = D_005F70D4;
            if (func_00364088((s32) D_005F6988) < temp_s0_8) {
                D_005F70D4 = 1;
                D_005F6988 = (s16) D_01FC6350.unkC;
                (*(s16 *)((u8 *)(arg1) + 0)) = 7;
            }
        }
        goto block_110;
    }
}
#else
LIT4_WORD(D_005F59DC, 0x3F99999A);
LIT4_WORD(D_005F59E0, 0xBF99999A);
LIT4_WORD(D_005F59E4, 0x38C90FDB);
LIT4_WORD(D_005F59E8, 0x38C90FDB);
LIT4_WORD(D_005F59EC, 0x40490FDB);
LIT4_WORD(D_005F59F0, 0x38C90FDB);
LIT4_WORD(D_005F59F4, 0x38C90FDB);
LIT4_WORD(D_005F59F8, 0x38C90FDB);
LIT4_WORD(D_005F59FC, 0x38C90FDB);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", jtbl_00523930);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EAF78);
#endif

void func_003EBB80(void) {
    BVec8 d = D_00523950;
    FVec4 a;
    FVec4 c;

    if (D_01FC6350.unk12 != 0) {
        d.v[1] = -d.v[1];
    }
    D_01FC51E0.unk50 = D_01FC51E0.unk54 = D_01FC51E0.unk58 = D_01FC51E0.unk5C = 0.0f;
    sceVu0ApplyMatrix(a, D_01FC6350.m[0], d.v);
    sceVu0ApplyMatrix(c, D_01FC6350.m[1], d.v);
    sceVu0AddVector(a, a, c);
    sceVu0ApplyMatrix(c, D_01FC6350.m[2], d.v);
    sceVu0AddVector(a, a, c);
    sceVu0ScaleVector(a, a, 0.33333334f);
    D_01FC51E0.unk0 = a[0];
    D_01FC51E0.unk4 = -a[1];
    D_01FC51E0.unk8 = -a[2];
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523950);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523960);
#if 0
/* NON_MATCHING: 370 lines (346 instructions, draft 326): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003EBC98(void *arg0) {
    f32 sp[64]; /* m2c stack */
    f32 sp0;
    f32 sp4;
    f32 sp8;
    f32 spC;
    s64 sp10;
    s64 sp18;
    f32 *temp_s0;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f2;
    f32 var_f0;
    f32 var_f1;
    f32 var_f1_2;
    f32 var_f2;
    f32 var_f3;
    f32 var_f3_2;
    s16 temp_v1_2;
    s32 var_v1;
    void *temp_s2;
    void *temp_v0;
    void *temp_v1;
    void *var_v0;

    switch (D_01FC51B0.unk24) {                     /* switch 1 */
    case 0:                                         /* switch 1 */
        D_01FC51B0.unk24 = 7;
        D_01FC51B0.unk0 = 0;
        D_01FC51B0.unk4 = -2300.0f;
        D_01FC51B0.unk8 = -12000.0f;
        D_01FC51B0.unk20 = 2000.0f;
        D_01FC51B0.unk14 = 0;
        break;
    case 1:                                         /* switch 1 */
        temp_v1 = &D_01FC51B0 + 0x51B0;
        (*(f32 *)((u8 *)(temp_v1) + 4)) = (f32) -2350.0f;
        (*(f32 *)((u8 *)(&D_01FC51B0) + 0x51B0)) = (f32) (*(s32 *)((u8 *)(arg0) + 4));
        (*(f32 *)((u8 *)(temp_v1) + 8)) = (f32) (*(s32 *)((u8 *)(arg0) + 0xC));
        (*(s32 *)((u8 *)(temp_v1) + 0x20)) = 0;
        (*(f32 *)((u8 *)(temp_v1) + 0x14)) = (f32) ((f32) (*(s16 *)((u8 *)(arg0) + 0x1A)) * 3.1415927f * 0.000030517578f);
        break;
    case 2:                                         /* switch 1 */
        temp_s0 = &D_01FC51B0 + 0x51B0;
        temp_f20 = (D_01FC51E0.unk8 - 4000.0f) - (*(f32 *)((u8 *)(temp_s0) + 8));
        sp10 = (*(s64 *)((u8 *)(&D_00523960) + 0));
        sp18 = (*(s64 *)((u8 *)(&D_00523960) + 8));
        if (temp_f20 > 0.0f) {
            sceVu0SubVector(sp, (f32 *) &sp10, temp_s0);
            temp_f20_2 = temp_f20 / sp8;
            if (temp_f20_2 < 1.0f) {
                func_003E92C0(temp_s0, temp_s0, sp, temp_f20_2);
                temp_f2 = 1.0f - temp_f20_2;
                (*(f32 *)((u8 *)(temp_s0) + 0x14)) = (f32) ((*(f32 *)((u8 *)(temp_s0) + 0x14)) * temp_f2);
                (*(f32 *)((u8 *)(temp_s0) + 0x20)) = (f32) ((*(f32 *)((u8 *)(temp_s0) + 0x20)) * temp_f2);
            } else {
                (*(s32 *)((u8 *)(temp_s0) + 0x24)) = 7;
                *(u128 *)&D_01FC51B0 = *(u128 *)&sp10;
                (*(f32 *)((u8 *)(temp_s0) + 0x20)) = 0.0f;
                (*(f32 *)((u8 *)(temp_s0) + 0x14)) = 0.0f;
            }
        }
        break;
    case 3:                                         /* switch 1 */
        temp_s2 = &D_01FC51B0 + 0x51B0;
        (*(f32 *)((u8 *)(&D_01FC51B0) + 0x51B0)) = 0.0f;
        (*(f32 *)((u8 *)(temp_s2) + 4)) = -290.0f;
        (*(f32 *)((u8 *)(temp_s2) + 8)) = 8000.0f;
        (*(f32 *)((u8 *)(temp_s2) + 0x20)) = 4000.0f;
        (*(s32 *)((u8 *)(temp_s2) + 0x24)) = 7;
        (*(f32 *)((u8 *)(temp_s2) + 0x10)) = (f32) ((((f32) (*D_01FC6340)[(s16) D_005F70B6 % 3] + ((f32) (rand() & 0xFFFFFF) * 5.9604645e-8f)) * 0.2617994f) + -0.87964594f);
        if (D_005F70B6 < 3) {
            var_f3 = 0.5235988f;
            var_f2 = -0.7853982f;
            var_f0 = (f32) (*(s16 *)((u8 *)(&(*D_01FC6340)[D_005F70B6]) + 6));
            var_f1 = (f32) (rand() & 0xFFFFFF) * 5.9604645e-8f;
        } else {
            if (D_005F70B6 < 6) {
                var_f1_2 = (f32) (rand() & 0xFFFFFF);
                var_v1 = D_005F70B6 + 1;
            } else {
                var_f1_2 = (f32) (rand() & 0xFFFFFF);
                var_v1 = D_005F70B6 + 2;
            }
            var_f3 = 0.5235988f;
            var_f1 = var_f1_2 * 5.9604645e-8f;
            var_f2 = -0.7853982f;
            var_f0 = (f32) (*(s16 *)((u8 *)(&(*D_01FC6340)[var_v1 % 3]) + 6));
        }
        (*(f32 *)((u8 *)(temp_s2) + 0x14)) = (f32) (((var_f0 + var_f1) * var_f3) + var_f2);
        break;
    case 4:                                         /* switch 1 */
        temp_v0 = &D_01FC51B0 + 0x51B0;
        (*(f32 *)((u8 *)(&D_01FC51B0) + 0x51B0)) = (f32) (*(f32 *)((u8 *)(arg0) + 0x590));
        (*(f32 *)((u8 *)(temp_v0) + 4)) = (f32) (-(*(f32 *)((u8 *)(arg0) + 0x594)) - 500.0f);
        (*(s32 *)((u8 *)(temp_v0) + 0x24)) = (s32) ((*(s32 *)((u8 *)(temp_v0) + 0x24)) + 1);
        (*(f32 *)((u8 *)(temp_v0) + 0x14)) = (f32) 3.1415927f;
        (*(f32 *)((u8 *)(temp_v0) + 0x20)) = (f32) 3500.0f;
        (*(s32 *)((u8 *)(temp_v0) + 0x10)) = 0;
        (*(f32 *)((u8 *)(temp_v0) + 8)) = (f32) -(*(f32 *)((u8 *)(arg0) + 0x598));
        break;
    case 5:                                         /* switch 1 */
        temp_v1_2 = (*(s16 *)((u8 *)(arg0) + 0x2A));
        switch (temp_v1_2) {                        /* switch 2; irregular */
        case 15:                                    /* switch 2 */
            if (D_005237B0[(*(s16 *)((u8 *)(arg0) + 0x28))] != 6) {
            default:                                /* switch 2 */
                var_v0 = &D_01FC51B0 + 0x51B0;
                (*(f32 *)((u8 *)(&D_01FC51B0) + 0x51B0)) = (f32) (((*(f32 *)((u8 *)(&D_01FC51B0) + 0x51B0)) + (*(f32 *)((u8 *)(arg0) + 0x590))) * 0.5f);
                (*(f32 *)((u8 *)(var_v0) + 4)) = (f32) ((((*(f32 *)((u8 *)(var_v0) + 4)) - (*(f32 *)((u8 *)(arg0) + 0x594))) - 500.0f) * 0.5f);
                var_f3_2 = ((*(f32 *)((u8 *)(var_v0) + 8)) - (*(f32 *)((u8 *)(arg0) + 0x598))) * 0.5f;
            } else {
            case 6:                                 /* switch 2 */
                var_v0 = &D_01FC51B0 + 0x51B0;
                (*(f32 *)((u8 *)(&D_01FC51B0) + 0x51B0)) = (f32) (((*(f32 *)((u8 *)(&D_01FC51B0) + 0x51B0)) * 0.97f) + ((*(f32 *)((u8 *)(arg0) + 0x590)) * 0.03f));
                (*(f32 *)((u8 *)(var_v0) + 4)) = (f32) (((*(f32 *)((u8 *)(var_v0) + 4)) * 0.97f) - (((*(f32 *)((u8 *)(arg0) + 0x594)) + 500.0f) * 0.03f));
                var_f3_2 = ((*(f32 *)((u8 *)(var_v0) + 8)) * 0.97f) - ((*(f32 *)((u8 *)(arg0) + 0x598)) * 0.03f);
            }
            break;
        }
        (*(f32 *)((u8 *)(var_v0) + 8)) = var_f3_2;
        break;
    }
    sp0 = 0.0f;
    spC = 0.0f;
    sp8 = 0.0f;
    sp4 = 0.0f;
    func_003E9120(D_01FC61A0, D_01FC51C0, sp);
    sp8 = -(*(f32 *)((u8 *)((D_01FC51C0 - 0x10)) + 0x20));
    __asm__ volatile("lqc2 $vf12, ($sp)\n" "vmulax.xyzw $ACC, $vf4, $vf12x\n" "vmadday.xyzw $ACC, $vf5, $vf12y\n" "vmaddaz.xyzw $ACC, $vf6, $vf12z\n" "vmaddw.xyzw $vf13, $vf7, $vf12w\n" "sqc2 $vf13, ($a0)\n" "lqc2 $vf14, ($v1)\n" "vadd.xyzw $vf15, $vf13, $vf14\n" "sqc2 $vf15, ($v0)\n");
    (*(f32 *)((u8 *)(D_00415808) + 0x14)) = (f32) 0 /* m2c: unknown value */;
    (*(f32 *)((u8 *)(D_00415808) + 0x18)) = sp0;
    (*(f32 *)((u8 *)(D_00415808) + 0x1C)) = sp0;
    sceVu0ScaleVector(sp, D_01FC51C0, 651.8986f);
    (*(s32 *)((u8 *)(D_00415808) + 0)) = -0 /* m2c: unknown value */;
    (*(s32 *)((u8 *)(D_00415808) + 4)) = (s32) -0;
    (*(s32 *)((u8 *)(D_00415808) + 8)) = (s32) -0;
}
#else
LIT4_WORD(D_005F5A04, 0xC50FC000);
LIT4_WORD(D_005F5A08, 0xC63B8000);
LIT4_WORD(D_005F5A0C, 0xC512E000);
LIT4_WORD(D_005F5A10, 0x40490FDB);
LIT4_WORD(D_005F5A14, 0x3E860A92);
LIT4_WORD(D_005F5A18, 0xBF61307A);
LIT4_WORD(D_005F5A1C, 0x3F060A92);
LIT4_WORD(D_005F5A20, 0xBF490FDB);
LIT4_WORD(D_005F5A24, 0x3F060A92);
LIT4_WORD(D_005F5A28, 0xBF490FDB);
LIT4_WORD(D_005F5A2C, 0x40490FDB);
LIT4_WORD(D_005F5A30, 0x455AC000);
LIT4_WORD(D_005F5A34, 0x3F7851EC);
LIT4_WORD(D_005F5A38, 0x3CF5C28F);
LIT4_WORD(D_005F5A3C, 0x4422F983);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", jtbl_00523970);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EBC98);
#endif

#if 0
/* NON_MATCHING: 28 lines: one shared SE call site here (the original duplicates the last one) and the timer is sign-extended at the decrement where the original keeps it unextended in a1 */
s32 func_003EC200(void) {
    BowlBody *p = D_01FC5260;
    s32 hit = 0;
    s32 sum = 0;
    s32 i;
    s16 t = D_005F70D0;
    s32 id;
    s32 n;

    if (t > 0) {
        t--;
        D_005F70D0 = t;
    }
    if (D_005F70D2 > 0) {
        D_005F70D2--;
    }
    for (i = 0; i < 10; i++) {
        if (p->unk17C != 0) {
            if (p->unk17E == 0) {
                hit = 1;
            }
            p->unk17E = 5;
        }
        sum += p->unk17E;
        if (p->unk17E > 0) {
            p->unk17E--;
        }
        p->unk17C = 0;
        p++;
    }
    if (hit && sum >= 5) {
        if (sum < 15) {
            if (t > 0) {
                return;
            }
            id = 0x10AE;
            n = 8;
        } else if (sum < 30) {
            if (t >= 7) {
                return;
            }
            id = 0x10AD;
            n = 14;
        } else {
            if (t >= 13) {
                return;
            }
            id = 0x10AC;
            n = 30;
        }
        D_005F70D0 = n;
        Snd_PlaySe(id, 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EC200);
#endif

void func_003EC320(f32 *out, f32 *in) {
    s32 i;

    for (i = 0; i < 4; i++) {
        out[i] = func_0034A570(in[i]);
    }
}

#if 0
/* NON_MATCHING: 118 lines (248 instructions, draft 213): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003EC388(f32 *a, f32 *b) {
    f32 saved_reg_f21; /* m2c */
    f32 sp[64]; /* m2c stack */
    f32 sp0;
    f32 sp4;
    f32 sp8;
    f32 spC;
    f32 temp_f1;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f21;
    f32 temp_f22;
    f32 temp_f22_2;
    f32 temp_f23;
    f32 temp_f25;
    f32 var_f1;
    f32 var_f20;
    f32 var_f20_2;
    f32 var_f20_3;
    f32 var_f21;

    var_f21 = saved_reg_f21;
    func_003EC320(sp, a);
    (*(f32 *)((u8 *)(a) + 4)) = (f32) ((*(f32 *)((u8 *)(a) + 4)) + (*(f32 *)((u8 *)(b) + 4)));
    temp_f22 = cosf(sp4);
    __asm__ volatile(".word 0x46000544\n");
    if (sp4 < 0.0f) {
        var_f21 = -saved_reg_f21;
    }
    temp_f20 = sinf((*(f32 *)((u8 *)(b) + 0))) * temp_f22;
    temp_f25 = temp_f20 - (sinf((*(f32 *)((u8 *)(b) + 8))) * var_f21);
    temp_f20_2 = sinf((*(f32 *)((u8 *)(b) + 8))) * temp_f22;
    temp_f23 = temp_f20_2 + (sinf((*(f32 *)((u8 *)(b) + 0))) * var_f21);
    temp_f1 = (*(f32 *)((u8 *)(a) + 0)) + asinf(temp_f25);
    (*(f32 *)((u8 *)(a) + 0)) = temp_f1;
    __asm__ volatile("lqc2 $vf4, ($v0)\n" "lqc2 $vf5, 0x10($v0)\n" "lqc2 $vf6, 0x20($v0)\n" "lqc2 $vf7, 0x30($v0)\n");
    var_f20 = temp_f1;
    if (3.1415927f < var_f20) {
        var_f20 -= 6.2831855f;
    } else if (var_f20 < -3.1415927f) {
        var_f20 += 6.2831855f;
    }
    func_003E8FA0(var_f20);
    __asm__ volatile("vaddx.z $vf8, $vf0, $vf8x\n" "vsubx.y $vf9, $vf0, $vf8x\n" "vaddy.z $vf9, $vf0, $vf8y\n" "vmulay.yz $ACC, $vf8, $vf4y\n" "vmaddz.yz $vf4, $vf9, $vf4z\n" "vmulay.yz $ACC, $vf8, $vf5y\n" "vmaddz.yz $vf5, $vf9, $vf5z\n" "vmulay.yz $ACC, $vf8, $vf6y\n" "vmaddz.yz $vf6, $vf9, $vf6z\n" "vmulay.yz $ACC, $vf8, $vf7y\n" "vmaddz.yz $vf7, $vf9, $vf7z\n");
    func_003E8FA0(asinf(temp_f23));
    __asm__ volatile("vaddx.y $vf9, $vf0, $vf8x\n" "vaddy.x $vf9, $vf0, $vf8y\n" "vsubx.x $vf8, $vf0, $vf8x\n" "vmulax.xy $ACC, $vf9, $vf4x\n" "vmaddy.xy $vf4, $vf8, $vf4y\n" "vmulax.xy $ACC, $vf9, $vf5x\n" "vmaddy.xy $vf5, $vf8, $vf5y\n" "vmulax.xy $ACC, $vf9, $vf6x\n" "vmaddy.xy $vf6, $vf8, $vf6y\n" "vmulax.xy $ACC, $vf9, $vf7x\n" "vmaddy.xy $vf7, $vf8, $vf7y\n");
    temp_f21 = cosf(sp0);
    __asm__ volatile(".word 0x46000304\n");
    if (sp0 < 0.0f) {

    }
    temp_f22_2 = temp_f21 * temp_f23;
    var_f20_2 = sp0 + asinf(temp_f25);
    if (3.1415927f < var_f20_2) {
        var_f20_2 -= 6.2831855f;
    } else if (var_f20_2 < -3.1415927f) {
        var_f20_2 += 6.2831855f;
    }
    (*(f32 *)((u8 *)(a) + 0)) += asinf(0 /* m2c: unknown value */ * temp_f23 * temp_f22_2);
    if ((temp_f21 != 0.0f) && (temp_f23 != 0.0f)) {
        if (var_f20_2 >= 0.0f) {
            var_f1 = -atan2f(temp_f22_2, sinf(var_f20_2));
        } else {
            var_f1 = atan2f(temp_f22_2, -sinf(var_f20_2));
        }
        (*(f32 *)((u8 *)(a) + 4)) = (f32) ((*(f32 *)((u8 *)(a) + 4)) + var_f1);
        func_003E8FA0(-var_f1);
        __asm__ volatile("vaddy.z $vf8, $vf0, $vf8y\n" "vaddy.x $vf9, $vf0, $vf8y\n" "vsubx.z $vf9, $vf0, $vf8x\n" "vmulaz.xz $ACC, $vf8, $vf4z\n" "vmaddx.xz $vf4, $vf9, $vf4x\n" "vmulaz.xz $ACC, $vf8, $vf5z\n" "vmaddx.xz $vf5, $vf9, $vf5x\n" "vmulaz.xz $ACC, $vf8, $vf6z\n" "vmaddx.xz $vf6, $vf9, $vf6x\n" "vmulaz.xz $ACC, $vf8, $vf7z\n" "vmaddx.xz $vf7, $vf9, $vf7x\n");
    }
    var_f20_3 = (*(f32 *)((u8 *)(a) + 0));
    if (3.1415927f < var_f20_3) {
        var_f20_3 -= 6.2831855f;
    } else if (var_f20_3 < -3.1415927f) {
        var_f20_3 += 6.2831855f;
    }
    func_003E8FA0(-var_f20_3);
    __asm__ volatile("vaddx.z $vf8, $vf0, $vf8x\n" "vsubx.y $vf9, $vf0, $vf8x\n" "vaddy.z $vf9, $vf0, $vf8y\n" "vmulay.yz $ACC, $vf8, $vf4y\n" "vmaddz.yz $vf4, $vf9, $vf4z\n" "vmulay.yz $ACC, $vf8, $vf5y\n" "vmaddz.yz $vf5, $vf9, $vf5z\n" "vmulay.yz $ACC, $vf8, $vf6y\n" "vmaddz.yz $vf6, $vf9, $vf6z\n" "vmulay.yz $ACC, $vf8, $vf7y\n" "vmaddz.yz $vf7, $vf9, $vf7z\n");
    spC = 0.0f;
    sp8 = 1.0f;
    sp4 = 0.0f;
    sp0 = 0.0f;
    __asm__ volatile("lqc2 $vf12, ($sp)\n" "vmulax.xyzw $ACC, $vf4, $vf12x\n" "vmadday.xyzw $ACC, $vf5, $vf12y\n" "vmaddaz.xyzw $ACC, $vf6, $vf12z\n" "vmaddw.xyzw $vf13, $vf7, $vf12w\n" "sqc2 $vf13, ($v0)\n");
    if ((sp0 != 0.0f) || (1.0f != 0.0f)) {
        (*(f32 *)((u8 *)(a) + 0xC)) = (f32) ((*(f32 *)((u8 *)(a) + 0xC)) + atan2f(sp0, sp8));
    }
}
#else
LIT4_WORD(D_005F5A40, 0x40490FDB);
LIT4_WORD(D_005F5A44, 0x40C90FDB);
LIT4_WORD(D_005F5A48, 0xC0490FDB);
LIT4_WORD(D_005F5A4C, 0x40C90FDB);
LIT4_WORD(D_005F5A50, 0x40490FDB);
LIT4_WORD(D_005F5A54, 0x40C90FDB);
LIT4_WORD(D_005F5A58, 0xC0490FDB);
LIT4_WORD(D_005F5A5C, 0x40C90FDB);
LIT4_WORD(D_005F5A60, 0x40490FDB);
LIT4_WORD(D_005F5A64, 0x40C90FDB);
LIT4_WORD(D_005F5A68, 0xC0490FDB);
LIT4_WORD(D_005F5A6C, 0x40C90FDB);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EC388);
#endif

void func_003EC768(BowlBall *b) {
    f32 *p = &b->unk0;
    FVec4 v;
    f32 s;

    *(u128 *)(p + 12) = *(u128 *)p;
    sceVu0AddVector(p, p, p + 4);
    *(u128 *)(p + 16) = *(u128 *)(p + 4);
    func_003EC388(p + 20, p + 24);
    if (b->unk4 < -150.0f) {
        b->unk4 += 2.45f;
        b->unk14 += 4.9f;
    } else if (b->unk78 == 0) {
        s = SqrtS(b->unk10 * b->unk10 + b->unk18 * b->unk18);
        b->unk78 = 1;
        if (s < 270.0f) {
            Snd_PlaySe(0x10AA, 0);
        } else {
            Snd_PlaySe(0x10AB, 0);
        }
    }
    if (-150.0f < b->unk4) {
        b->unk4 = -150.0f;
        if (b->unk14 < 4.9f) {
            b->unk14 = 0.0f;
        } else {
            b->unk14 *= -0.6f;
        }
    }
    *(u128 *)v = *(u128 *)(p + 4);
    func_003E92C0(p + 4, p + 4, p + 8, (b->unk8 - -11000.0f) / 15000.0f * ((b->unk8 - -11000.0f) / 15000.0f));
    if (b->unk18 < 20.0f) {
        *(u128 *)(p + 4) = *(u128 *)v;
    } else {
        sceVu0ScaleVector(p + 4, p + 4,
                          SqrtS((v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) /
                                (b->unk10 * b->unk10 + b->unk14 * b->unk14 + b->unk18 * b->unk18)));
    }
}

void func_003EC970(BowlBody *b, f32 t) {
    func_003E92C0(b->unk0.v, b->unk30.v, b->unk40.v, t);
}

void func_003EC980(BowlBody *b, f32 t) {
    func_003E92C0(b->unk0.v, b->unk0.v, b->unk10.v, t);
}

void func_003EC990(BowlBody *b) {
    sceVu0SubVector(b->unk40.v, b->unk0.v, b->unk30.v);
}

void func_003EC9A0(BowlBody *b) {
    BVec *s = b->unk80.m;
    BVec *d = b->unkC0.m;
    s32 i;

    for (i = 4; i > 0; i--) {
        d->q = s->q;
        s++;
        d++;
    }
    b->unk150.q = b->unk140.q;
    b->unk20.q = b->unk0.q;
    sceVu0AddVector(b->unk0.v, b->unk0.v, b->unk10.v);
    b->unk60.q = b->unk40.q;
    func_003EC388(b->unk40.v, b->unk50.v);
    func_003ECAD8(b);
}

void func_003ECA38(BowlBody *b, f32 t) {
    func_003E92C0(b->unk0.v, b->unk20.v, b->unk30.v, t);
    func_003E92C0(b->unk40.v, b->unk60.v, b->unk70.v, t);
}

void func_003ECA80(BowlBody *b, f32 t) {
    FVec4 tmp;

    func_003E92C0(b->unk0.v, b->unk0.v, b->unk10.v, t);
    sceVu0ScaleVector(tmp, b->unk50.v, t);
    func_003EC388(b->unk40.v, tmp);
}

#if 0
/* NON_MATCHING: 15 lines: temporaries of the last inline-asm block (the original hoists the +0x140/0x150/0x160 pointers) and the loop delay slot */
/* Rebuilds the body's corner points: VU0 vf4-vf7 hold the matrix func_003E9120 leaves there. */
void func_003ECAD8(BowlBody *b) {
    FVec4 r;
    BVec *src = D_0046BC20;
    BVec *m = b->unk80.m;
    BVec *d = b->unk100;
    BVec *o = b->unkC0.m;
    s32 i;

    sceVu0SubVector(b->unk30.v, b->unk0.v, b->unk20.v);
    sceVu0SubVector(b->unk70.v, b->unk40.v, b->unk60.v);
    func_003EC320(r, b->unk40.v);
    func_003E9120(D_01FC61A0, r, b->unk0.v);
    i = 4;
    while (i > 0) {
        __asm__ volatile("lqc2 $vf12, 0x0(%0)\n"
                         "vmulax.xyzw $ACC, $vf4, $vf12x\n"
                         "vmadday.xyzw $ACC, $vf5, $vf12y\n"
                         "vmaddaz.xyzw $ACC, $vf6, $vf12z\n"
                         "vmaddw.xyzw $vf13, $vf7, $vf12w\n"
                         "sqc2 $vf13, 0x0(%1)\n"
                         "lqc2 $vf14, 0x0(%2)\n"
                         "vsub.xyzw $vf15, $vf13, $vf14\n"
                         "sqc2 $vf15, 0x0(%3)\n"
                         :
                         : "r"(src), "r"(m), "r"(o), "r"(d)
                         : "memory");
        i--;
        src++;
        m++;
        d++;
        o++;
    }
    __asm__ volatile("lqc2 $vf12, 0x0(%0)\n"
                     "vmulax.xyzw $ACC, $vf4, $vf12x\n"
                     "vmadday.xyzw $ACC, $vf5, $vf12y\n"
                     "vmaddaz.xyzw $ACC, $vf6, $vf12z\n"
                     "vmaddw.xyzw $vf13, $vf7, $vf12w\n"
                     "sqc2 $vf13, 0x0(%1)\n"
                     "lqc2 $vf14, 0x0(%2)\n"
                     "vsub.xyzw $vf15, $vf13, $vf14\n"
                     "sqc2 $vf15, 0x0(%3)\n"
                     :
                     : "r"(&D_0046BC60), "r"(&b->unk140), "r"(&b->unk150), "r"(&b->unk160)
                     : "memory");
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003ECAD8);
#endif

void func_003ECBF0(f32 *v, BowlBody *b, s32 k, f32 t) {
    FVec4 a;
    f32 e;

    sceVu0SubVector(v, b->unkC0.m[0].v, b->unk150.v);
    sceVu0SubVector(a, b->unk80.m[0].v, b->unk140.v);
    sceVu0SubVector(a, a, v);
    func_003E92C0(v, v, a, t);
    e = D_00523580[k];
    func_003E92C0(v, b->unk100[k].v, v,
                  (v[0] * b->unk30.v[0] + v[1] * b->unk30.v[1] + v[2] * b->unk30.v[2]) * (3.8f - e) /
                      ((v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) * e));
}

f32 func_003ECD00(BowlBody *b, s32 k, f32 t) {
    FVec4 a;
    FVec4 c;
    f32 e;
    f32 d;
    f32 s;

    sceVu0SubVector(c, b->unkC0.m[0].v, b->unk150.v);
    sceVu0SubVector(a, b->unk80.m[0].v, b->unk140.v);
    sceVu0SubVector(a, a, c);
    func_003E92C0(c, c, a, t);
    s = c[1] * c[1];
    e = D_00523580[k];
    d = 3.8f - e;
    s = s / (c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
    return d * SqrtS(s) + e;
}

void func_003ECDE8(BowlBody *b, f32 *n, f32 *pt) {
    FVec4 l;
    FVec4 r;
    f32 nn = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
    f32 rr;
    f32 d;
    f32 cx;
    f32 cy;
    f32 cz;

    if (nn <= 0.0f) {
        return;
    }
    sceVu0SubVector(r, pt, b->unk0.v);
    rr = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
    if (rr == 0.0f) {
        *(u128 *)l = *(u128 *)n;
    } else {
        d = n[0] * r[0] + n[1] * r[1] + n[2] * r[2];
        sceVu0ScaleVector(l, n, d * d / (nn * rr));
    }
    func_003E92C0(b->unk10.v, b->unk30.v, l, 0.2631579f);
    sceVu0SubVector(l, n, l);
    sceVu0ScaleVector(l, l, 6.258214e-06f);
    cx = r[1] * l[2] - r[2] * l[1];
    cy = r[2] * l[0] - r[0] * l[2];
    cz = r[0] * l[1] - r[1] * l[0];
    b->unk50.v[0] += cx;
    b->unk50.v[1] += cy;
    b->unk50.v[2] += cz;
}

void func_003ECF70(BowlBody *b) {
    FVec4 v;
    f32 low = 0.0f;
    f32 y;
    s32 k;

    for (k = 0; k < 4; k++) {
        y = b->unk80.m[k].v[1] + D_00523570[k];
        if (low < y) {
            if (k == 3) {
                *(u128 *)v = b->unk80.m[3].q;
                v[1] += D_00523570[3];
                sceVu0SubVector(v, v, b->unk140.v);
                if (v[0] * v[0] + v[1] * v[1] + v[2] * v[2] < 5000.0f) {
                    sceVu0SubVector(v, b->unk80.m[0].v, b->unk140.v);
                    y = b->unk140.v[1] +
                        SqrtS((v[0] * v[0] + v[2] * v[2]) * 5000.0f / (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
                    if (!(low < y)) {
                        continue;
                    }
                }
            }
            low = y;
        }
    }
    do { /* fake: the loop notes swap f21 / f22 into the original order */
        if (0.0f < low) {
            b->unk10.v[1] = 0.0f;
            b->unk0.v[1] -= low;
            func_003ECAD8(b);
        }
    } while (0);
}

s32 func_003ED118(BowlBody *b, s32 k, f32 t) {
    BowlBody tmp = *b;
    s32 r;

    func_003ECA80(&tmp, t);
    if (k == 2) {
        r = 0;
        if (asinf(0.057142857f) + 1.5707964f - 0.005f < tmp.unk40.v[0]) {
            r = 1;
        } else if (tmp.unk40.v[0] < -(asinf(0.057142857f) + 1.5707964f) + 0.005f) {
            r = 1;
        }
        return r;
    }
    if (k == 0) {
        r = 0;
        if (tmp.unk40.v[0] < asinf(0.057142857f) + 1.5707964f + 0.005f) {
            if (-(asinf(0.057142857f) + 1.5707964f) - 0.005f < tmp.unk40.v[0]) {
                r = 1;
            }
        }
        return r;
    }
    return 0;
}

f32 func_003ED268(BowlBody *b, s32 k, f32 t) {
    BowlBody tmp = *b;

    func_003ECA80(&tmp, t);
    tmp.unk0.v[1] += 2.45f * t * t;
    func_003ECAD8(&tmp);
    if (k == 4) {
        k = 2;
    }
    return tmp.unk80.m[k].v[1] + D_00523570[k];
}

#if 0
/* NON_MATCHING: 518 lines (535 instructions, draft 468): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003ED338(BowlBody *p) {
    f32 saved_reg_f21; /* m2c */
    f32 sp[64]; /* m2c stack */
    f32 sp24; /* m2c stack */
    f32 sp28; /* m2c stack */
    f32 sp4;
    FVec4 sp10;
    f32 sp14;
    f32 sp18;
    f32 sp20;
    BVec *temp_a0;
    BVec *temp_a0_2;
    BVec *temp_s1;
    BVec *temp_s3;
    f32 temp_f0;
    f32 temp_f0_2;
    f32 temp_f0_3;
    f32 temp_f12;
    f32 temp_f12_2;
    f32 temp_f12_3;
    f32 temp_f12_4;
    f32 temp_f1;
    f32 temp_f1_2;
    f32 temp_f1_3;
    f32 temp_f1_4;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f20_3;
    f32 temp_f22;
    f32 temp_f23;
    f32 temp_f24;
    f32 temp_f2;
    f32 temp_f2_2;
    f32 temp_f3;
    f32 temp_f3_2;
    f32 temp_f3_3;
    f32 temp_f3_4;
    f32 temp_f3_5;
    f32 temp_f4;
    f32 temp_f4_2;
    f32 temp_f6;
    f32 var_f0;
    f32 var_f1;
    f32 var_f21;
    f32 var_f23;
    s32 temp_s5;
    s32 var_s1;
    s32 var_s4;
    s32 var_s6;
    s32 var_v0;
    u128 var_v0_2;
    void *temp_v0;

    var_f21 = saved_reg_f21;
    var_s4 = -1;
    var_s1 = 0;
    var_f23 = 1.0f;
    temp_f24 = 9.8f;
    var_v0 = 0 * 0x10;
    do {
        temp_v0 = p + var_v0;
        temp_f1 = (*(f32 *)((u8 *)(temp_v0) + 0xC4)) + D_00523570[var_s1];
        temp_f2 = (*(f32 *)((u8 *)(temp_v0) + 0x104));
        if (temp_f1 >= 0.0f) {
            var_f1 = 0.0f;
        } else {
            __asm__ volatile(".word 0x46010044\n");
            var_f1 = (((temp_f2 * temp_f2) - (temp_f1 * temp_f24)) - temp_f2) / 4.9f;
        }
        if ((var_f1 < 1.0f) && (var_f1 <= var_f23) && ((var_f23 = var_f1, (var_s1 != 2)) || ((var_f21 = 1.5707964f, temp_f22 = 0.005f, temp_f20 = p->unk60.v[0] + (p->unk70.v[0] * var_f23), var_s4 = 4, !(((asinf(0.057142857f) + var_f21) - temp_f22) < temp_f20)) && (temp_f20_2 = p->unk60.v[0] + (p->unk70.v[0] * var_f23), var_s4 = 4, !(temp_f20_2 < (-(asinf(0.057142857f) + var_f21) + temp_f22)))))) {
            var_s4 = var_s1;
        }
        var_s1 += 1;
        var_v0 = var_s1 * 0x10;
    } while (var_s1 < 4);
    if (var_s4 >= 0) {
        var_s6 = 0;
        if (var_s4 == 4) {
            func_003E92C0(sp, p->unk20.v, p->unk30.v, var_f23);
            var_v0_2 = p->unk30.q;
            goto block_26;
        }
        temp_s5 = var_s4 * 0x10;
        func_003E92C0(sp, p + (temp_s5 + 0xC0), p + (temp_s5 + 0x100), var_f23);
        sp4 += D_00523570[var_s4];
        if (var_s4 == 3) {
            temp_s1 = &p->unk150;
            func_003E92C0(&sp20, temp_s1->v, p->unk160.v, var_f23);
            temp_f20_3 = 5000.0f;
            sceVu0SubVector(&sp20, sp, &sp20);
            if (((sp20 * sp20) + (sp24 * sp24) + (sp28 * sp28)) < temp_f20_3) {
                var_s6 = 1;
                sceVu0SubVector(&sp20, p->unkC0.m[0].v, temp_s1->v);
                __asm__ volatile(".word 0x46000544\n");
                temp_f3 = (sp20 * sp20) + (sp28 * sp28);
                if (temp_f3 <= 0.0f) {

                } else {
                    __asm__ volatile(".word 0x460300C4\n");
                    sp14 = temp_f3;
                    sp10[0] = (-sp20 * sp24) / temp_f3;
                    sp18 = (-sp28 * sp24) / temp_f3;
                    func_003E92C0(sp, temp_s1->v, &sp10, var_f21);
                }
                temp_s3 = &p->unk140;
                if (sp4 <= 0.0f) {
                    sceVu0SubVector(&sp20, p->unk80.m[0].v, temp_s3->v);
                    temp_f3_2 = (sp20 * sp20) + (sp28 * sp28);
                    if (temp_f3_2 <= 0.0f) {
                        *(u128 *)sp10 = p->unk140.q;
                    } else {
                        __asm__ volatile(".word 0x460300C4\n");
                        sp14 = temp_f3_2;
                        sp10[0] = (-sp20 * sp24) / temp_f3_2;
                        sp18 = (-sp28 * sp24) / temp_f3_2;
                        func_003E92C0((f32 *) &sp10, temp_s3->v, (f32 *) &sp10, var_f21);
                    }
                    sceVu0SubVector(&sp10, &sp10, sp);
                    __asm__ volatile(".word 0x46000004\n");
                    var_f23 = (((sp14 * sp14) - (sp4 * 9.8f)) - sp14) / 4.9f;
                    if (!(var_f23 >= 1.0f)) {
                        func_003E92C0(sp, sp, &sp10, var_f23);
                        goto block_25;
                    }
                    goto block_68;
                }
                goto block_25;
            }
        }
block_25:
        var_v0_2 = (*(u128 *)((u8 *)((temp_s5 + p)) + 0x100));
block_26:
        *(u128 *)sp10 = var_v0_2;
        func_003ECA38(p, var_f23);
        temp_f3_3 = var_f23 * 4.9f;
        temp_f0 = var_f23 * 2.45f * var_f23;
        temp_f6 = sp14 + temp_f3_3;
        sp14 = temp_f6;
        sp4 += temp_f0;
        p->unk0.v[1] += temp_f0;
        p->unk10.v[1] += temp_f3_3;
        if (temp_f6 > 0.0f) {
            if (var_s4 == 4) {
                sp14 = temp_f6 * -6.8399997f;
            } else {
                sp14 *= -func_003ECD00(p, var_s4, var_f23) * 1.8f;
            }
            sp10[0] = 0.0f;
            sp18 = 0.0f;
            func_003ECDE8(p, (f32 *) &sp10, sp);
        }
        temp_f23 = 1.0f - var_f23;
        if (func_003ED268(p, var_s4, temp_f23) > 0.0f) {
            if (var_s4 != 4) {
                var_f0 = -func_003ECD00(p, var_s4, 1.0f - temp_f23) * 4.9f * temp_f23 * 0.8f;
            } else {
                var_f0 = temp_f23 * -18.62f * 0.8f;
            }
            sp14 = var_f0;
            sp18 = 0.0f;
            sp10[0] = 0.0f;
            func_003ECDE8(p, &sp10, sp);
            p->unk10.v[1] = 0.0f;
            if ((var_s4 == 4) || (func_003ED118(p, var_s4, temp_f23) != 0)) {
                temp_f4 = p->unk10.v[0];
                temp_f3_4 = p->unk10.v[2];
                __asm__ volatile(".word 0x46010044\n");
                temp_f12 = 1.0f - (temp_f23 * (0.25f / ((temp_f4 * temp_f4) + (temp_f3_4 * temp_f3_4))));
                if (!(temp_f12 <= 0.0f)) {
                    p->unk10.v[2] = temp_f3_4 * temp_f12;
                    p->unk10.v[0] = temp_f4 * temp_f12;
                } else {
                    p->unk10.v[0] = 0.0f;
                    p->unk10.v[2] = 0.0f;
                }
                temp_f1_2 = p->unk50.v[0];
                temp_f0_2 = p->unk50.v[2];
                __asm__ volatile(".word 0x46010044\n");
                temp_f12_2 = 1.0f - (temp_f23 * (0.0008f / ((temp_f1_2 * temp_f1_2) + (temp_f0_2 * temp_f0_2))));
                if (!(temp_f12_2 <= 0.0f)) {
                    temp_a0 = &p->unk50;
                    sceVu0ScaleVector(temp_a0->v, temp_a0->v, temp_f12_2);
                } else {
                    p->unk50.v[0] = 0.0f;
                    goto block_61;
                }
            } else if (var_s6 != 0) {
                temp_f1_3 = p->unk40.v[0];
                if ((temp_f1_3 < 0.005f) && (-0.005f < temp_f1_3)) {
                    temp_f4_2 = p->unk10.v[0];
                    temp_f3_5 = p->unk10.v[2];
                    __asm__ volatile(".word 0x46010044\n");
                    temp_f12_3 = 1.0f - (temp_f23 * (0.75f / ((temp_f4_2 * temp_f4_2) + (temp_f3_5 * temp_f3_5))));
                    if (!(temp_f12_3 <= 0.0f)) {
                        p->unk10.v[2] = temp_f3_5 * temp_f12_3;
                        p->unk10.v[0] = temp_f4_2 * temp_f12_3;
                    } else {
                        p->unk10.v[0] = 0.0f;
                        p->unk10.v[2] = 0.0f;
                    }
                    temp_f1_4 = p->unk50.v[0];
                    temp_f2_2 = p->unk50.v[1];
                    temp_f0_3 = p->unk50.v[2];
                    __asm__ volatile(".word 0x46010044\n");
                    temp_f12_4 = 1.0f - (temp_f23 * (0.0008f / ((temp_f1_4 * temp_f1_4) + (temp_f2_2 * temp_f2_2) + (temp_f0_3 * temp_f0_3))));
                    if (!(temp_f12_4 <= 0.0f)) {
                        temp_a0_2 = &p->unk50;
                        sceVu0ScaleVector(temp_a0_2->v, temp_a0_2->v, temp_f12_4);
                    } else {
                        p->unk50.v[0] = 0.0f;
                        p->unk40.v[0] = 0.0f;
block_61:
                        p->unk50.v[2] = 0.0f;
                        p->unk50.v[1] = 0.0f;
                    }
                }
            }
        }
        func_003ECA80(p, temp_f23);
        p->unk10.v[1] += temp_f23 * 4.9f;
        p->unk0.v[1] += temp_f23 * 2.45f * temp_f23;
        func_003ECAD8(p);
        func_003ECF70(p);
        if (var_s6 == 0) {
            p->unk170 = 0;
            return;
        }
        return;
    }
block_68:
    p->unk0.v[1] += 2.45f;
    p->unk10.v[1] += 4.9f;
    func_003ECAD8(p);
}
#else
LIT4_WORD(D_005F5AB8, 0x411CCCCD);
LIT4_WORD(D_005F5ABC, 0x409CCCCD);
LIT4_WORD(D_005F5AC0, 0x3D6A0EA1);
LIT4_WORD(D_005F5AC4, 0x3FC90FDB);
LIT4_WORD(D_005F5AC8, 0x3BA3D70A);
LIT4_WORD(D_005F5ACC, 0x3D6A0EA1);
LIT4_WORD(D_005F5AD0, 0x459C4000);
LIT4_WORD(D_005F5AD4, 0x411CCCCD);
LIT4_WORD(D_005F5AD8, 0x409CCCCD);
LIT4_WORD(D_005F5ADC, 0x401CCCCD);
LIT4_WORD(D_005F5AE0, 0x409CCCCD);
LIT4_WORD(D_005F5AE4, 0xC0DAE147);
LIT4_WORD(D_005F5AE8, 0x3FE66666);
LIT4_WORD(D_005F5AEC, 0xC194F5C3);
LIT4_WORD(D_005F5AF0, 0x3F4CCCCD);
LIT4_WORD(D_005F5AF4, 0x409CCCCD);
LIT4_WORD(D_005F5AF8, 0x3F4CCCCD);
LIT4_WORD(D_005F5AFC, 0x3A51B717);
LIT4_WORD(D_005F5B00, 0x3BA3D70A);
LIT4_WORD(D_005F5B04, 0xBBA3D70A);
LIT4_WORD(D_005F5B08, 0x3A51B717);
LIT4_WORD(D_005F5B0C, 0x401CCCCD);
LIT4_WORD(D_005F5B10, 0x409CCCCD);
LIT4_WORD(D_005F5B14, 0x401CCCCD);
LIT4_WORD(D_005F5B18, 0x409CCCCD);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003ED338);
#endif

f32 func_003EDB98(f32 *u, f32 *v, f32 r, f32 def) {
    f32 s = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    f32 b;
    f32 d;

    if (s == 0.0f) {
        return def;
    }
    s -= r * r;
    if (s <= 0.0f) {
        return 0.0f;
    }
    b = u[0] * u[0] + u[1] * u[1] + u[2] * u[2];
    if (b == 0.0f) {
        return def;
    }
    d = u[0] * v[0] + u[1] * v[1] + u[2] * v[2];
    if (d >= 0.0f) {
        return def;
    }
    s = d * d - b * s;
    if (s < 0.0f) {
        return def;
    }
    s = (-d - SqrtS(s)) / b;
    if (s >= def) {
        return def;
    }
    return s;
}

#if 0
/* NON_MATCHING: 121 lines (190 instructions, draft 205): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003EDC90(BowlBall *b, BowlBody *p) {
    f32 sp[64]; /* m2c stack */
    /* the m2c draft's vector locals (each 16 bytes on the EE stack) as vectors: as separate floats the vector
       routines' 16-byte stores ran over the neighbouring locals, wherever the compiler put them */
    f32 sp10[4];
    f32 sp20[4];
    f32 sp30[4];
    f32 sp40[4];
    f32 temp_f0;
    f32 temp_f0_2;
    f32 temp_f20;
    f32 temp_f21;
    f32 temp_f21_2;
    f32 temp_f3;
    f32 var_f22;
    s32 temp_a1;
    s32 var_s0;
    s32 var_s1;
    s32 var_s5;
    u8 *temp_s6;

    temp_s6 = &b->pad30[0x10];
    var_s5 = -1;
    var_s1 = 0;
    var_f22 = 1.0f;
    var_s0 = 0 * 0x10;
    do {
        sceVu0SubVector(sp10, (f32 *) b->pad30, p + (var_s0 + 0xC0));
        sceVu0SubVector(sp, (f32 *) temp_s6, p + (var_s0 + 0x100));
        temp_f0 = func_003EDB98(sp, sp10, D_00523570[var_s1] + 150.0f, var_f22);
        if (temp_f0 < var_f22) {
            var_f22 = temp_f0;
            var_s5 = var_s1;
        }
        var_s1 += 1;
        var_s0 = var_s1 * 0x10;
    } while (var_s1 < 4);
    if (var_s5 >= 0) {
        func_003EC970((BowlBody *) b, var_f22);
        func_003ECA38(p, var_f22);
        temp_a1 = var_s5 * 0x10;
        func_003E92C0(sp20, p + (temp_a1 + 0xC0), p + (temp_a1 + 0x100), var_f22);
        sceVu0SubVector(sp40, &b->unk0, sp20);
        temp_f0_2 = D_00523570[var_s5];
        func_003E92C0(sp20, sp20, sp40, temp_f0_2 / (temp_f0_2 + 150.0f));
        func_003ECBF0(sp30, p, var_s5, var_f22);
        sceVu0SubVector(sp30, sp30, (f32 *) temp_s6);
        temp_f21 = (sp30[0] * sp40[0]) + (sp30[1] * sp40[1]) + (sp30[2] * sp40[2]);
        if (temp_f21 > 0.0f) {
            temp_f3 = D_00523580[var_s5];
            p->unk178 = 0;
            p->unk17C = 1;
            temp_f21_2 = (2.0f * temp_f21 * temp_f3) / ((temp_f3 + 30.0f) * ((sp40[0] * sp40[0]) + (sp40[1] * sp40[1]) + (sp40[2] * sp40[2])));
            func_003E92C0(&b->unk10, (f32 *) temp_s6, sp40, temp_f21_2);
            b->unk14 = 0.0f;
            sceVu0ScaleVector(sp30, sp40, -temp_f21_2 * 30.0f);
            func_003ECDE8(p, sp30, sp20);
        }
        temp_f20 = 1.0f - var_f22;
        func_003EC980((BowlBody *) b, temp_f20);
        func_003ECA80(p, temp_f20);
        func_003EC990((BowlBody *) b);
        func_003ECAD8(p);
        if (D_005F70BC == 0) {
            func_003BB228(D_005F70AC, 5);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EDC90);
#endif

#if 0
/* NON_MATCHING: 265 lines (220 instructions, draft 238): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003EDF88(BowlBody *p, BowlBody *q) {
    f32 sp[64]; /* m2c stack */
    /* the m2c draft's vector locals (each 16 bytes on the EE stack) as vectors: as separate floats the vector
       routines' 16-byte stores ran over the neighbouring locals, wherever the compiler put them */
    f32 sp10[4];
    f32 sp20[4];
    f32 sp30[4];
    f32 sp40[4];
    f32 sp50[4];
    s32 sp60;
    s32 sp64;
    s32 sp68;
    f32 temp_f0;
    f32 temp_f0_2;
    f32 temp_f0_3;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f20_3;
    f32 temp_f3;
    f32 var_f21;
    s32 temp_a1;
    s32 temp_a1_2;
    s32 temp_v1;
    s32 var_s0;
    s32 var_s1;
    s32 var_s2;

    var_s2 = 0;
    var_f21 = 1.0f;
    sp60 = -1;
    sp64 = -1;
    do {
        temp_v1 = var_s2 * 0x10;
        sp68 = var_s2 + 1;
        var_s1 = 0;
        var_s0 = 0 * 0x10;
loop_2:
        sceVu0SubVector(sp10, p + (temp_v1 + 0xC0), q + (var_s0 + 0xC0));
        sceVu0SubVector(sp, p + (temp_v1 + 0x100), q + (var_s0 + 0x100));
        temp_f0 = func_003EDB98(sp, sp10, D_00523570[var_s2] + D_00523570[var_s1], var_f21);
        if (temp_f0 < var_f21) {
            sp60 = var_s2;
            var_f21 = temp_f0;
            sp64 = var_s1;
        }
        var_s1 += 1;
        var_s0 = var_s1 * 0x10;
        if (var_s1 < 4) {
            goto loop_2;
        }
        var_s2 = sp68;
    } while (var_s2 < 4);
    if (sp64 >= 0) {
        if (sp60 >= 0) {
            func_003ECA38(p, var_f21);
            func_003ECA38(q, var_f21);
            temp_a1 = sp60 * 0x10;
            func_003E92C0(sp50, p + (temp_a1 + 0xC0), p + (temp_a1 + 0x100), var_f21);
            temp_a1_2 = sp64 * 0x10;
            func_003E92C0(sp20, q + (temp_a1_2 + 0xC0), q + (temp_a1_2 + 0x100), var_f21);
            sceVu0SubVector(sp40, sp50, sp20);
            temp_f0_2 = D_00523570[sp64];
            func_003E92C0(sp20, sp20, sp40, temp_f0_2 / (temp_f0_2 + D_00523570[sp60]));
            func_003ECBF0(sp50, p, sp60, var_f21);
            func_003ECBF0(sp30, q, sp64, var_f21);
            sceVu0SubVector(sp30, sp30, sp50);
            temp_f20 = (sp30[0] * sp40[0]) + (sp30[1] * sp40[1]) + (sp30[2] * sp40[2]);
            if (temp_f20 > 0.0f) {
                temp_f0_3 = D_00523580[sp60];
                temp_f3 = D_00523580[sp64];
                q->unk17C = 1;
                p->unk17C = 1;
                q->unk178 = 0;
                p->unk178 = 0;
                temp_f20_2 = (2.0f * temp_f20 * temp_f0_3 * temp_f3) / ((temp_f0_3 + temp_f3) * ((sp40[0] * sp40[0]) + (sp40[1] * sp40[1]) + (sp40[2] * sp40[2])));
                sceVu0ScaleVector(sp30, sp40, temp_f20_2);
                func_003ECDE8(p, sp30, sp20);
                sceVu0ScaleVector(sp30, sp40, -temp_f20_2);
                func_003ECDE8(q, sp30, sp20);
            }
            temp_f20_3 = 1.0f - var_f21;
            func_003ECA80(p, temp_f20_3);
            func_003ECA80(q, temp_f20_3);
            func_003ECAD8(p);
            func_003ECAD8(q);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EDF88);
#endif

#if 0
/* NON_MATCHING: 47 lines: the original keeps 0.0f in f24 (a variable used by the left test and the right y bound) */
void func_003EE2F8(BowlBall *ball) {
    BowlBody *b = (BowlBody *)ball;
    FVec4 v;
    f32 d;
    f32 e;
    f32 push;
    f32 r;
    f32 x;

    if (b->unk0.v[2] < 9435.0f) {
        return;
    }
    e = b->unk0.v[0] - -2550.0f;
    push = 1.0f;
    d = -2550.0f - b->unk30.v[0];
    if (e <= 0.0f && d < 0.0f) {
        sceVu0SubVector(v, b->unk0.v, b->unk30.v);
        r = 106.066017f;
        func_003E92C0(v, b->unk30.v, v, d / (d + e));
        if (-1282.066f < v[1] && v[1] < r && 9478.934f < v[2] && v[2] < 12691.066f && e < push) {
            push = e;
        }
    }
    x = b->unk0.v[0];
    if (push <= 0.0f) {
        b->unk0.v[0] = x - push;
        if (b->unk10.v[0] < 0.0f) {
            b->unk10.v[0] = -b->unk10.v[0] * 0.4f;
        }
        func_003EC990(b);
        return;
    }
    e = x - 2550.0f;
    push = -1.0f;
    d = 2550.0f - b->unk30.v[0];
    if (0.0f <= e && 0.0f < d) {
        sceVu0SubVector(v, b->unk0.v, b->unk30.v);
        r = 106.066017f;
        func_003E92C0(v, b->unk30.v, v, d / (d + e));
        if (-1176.0f - r < v[1] && v[1] < 0.0f + r && 9585.0f - r < v[2] && v[2] < 12585.0f + r && push < e) {
            push = e;
        }
    }
    if (0.0f <= push) {
        b->unk0.v[0] -= push;
        if (0.0f < b->unk10.v[0]) {
            b->unk10.v[0] = -b->unk10.v[0] * 0.4f;
        }
        func_003EC990(b);
    }
}
#else
LIT4_WORD(D_005F5B1C, 0x46136C00);
LIT4_WORD(D_005F5B20, 0xC51F6000);
LIT4_WORD(D_005F5B24, 0x42D421CD);
LIT4_WORD(D_005F5B28, 0xC4A0421D);
LIT4_WORD(D_005F5B2C, 0x46141BBC);
LIT4_WORD(D_005F5B30, 0x46464C44);
LIT4_WORD(D_005F5B34, 0x3ECCCCCD);
LIT4_WORD(D_005F5B38, 0x451F6000);
LIT4_WORD(D_005F5B3C, 0x42D421CD);
LIT4_WORD(D_005F5B40, 0x4615C400);
LIT4_WORD(D_005F5B44, 0x4644A400);
LIT4_WORD(D_005F5B48, 0x3ECCCCCD);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EE2F8);
#endif

#if 0
/* NON_MATCHING: 408 lines (408 instructions, draft 431): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003EE598(BowlBody *p) {
    f32 sp[64]; /* m2c stack */
    f32 sp0; /* m2c stack */
    f32 sp4; /* m2c stack */
    f32 sp8; /* m2c stack */
    f32 *temp_s0;
    f32 *temp_s0_2;
    f32 *temp_s0_3;
    f32 *temp_s0_4;
    f32 temp_f1;
    f32 temp_f1_2;
    f32 temp_f1_3;
    f32 temp_f1_4;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f20_3;
    f32 temp_f20_4;
    f32 temp_f21;
    f32 temp_f21_2;
    f32 temp_f21_3;
    f32 temp_f21_4;
    f32 temp_f22;
    f32 temp_f22_2;
    f32 temp_f22_3;
    f32 temp_f22_4;
    f32 temp_f23;
    f32 temp_f23_2;
    f32 temp_f23_3;
    f32 temp_f25;
    f32 temp_f25_2;
    f32 temp_f25_3;
    f32 temp_f25_4;
    f32 temp_f26;
    f32 temp_f28;
    f32 temp_f29;
    f32 temp_f29_2;
    f32 temp_f29_3;
    f32 temp_f29_4;
    f32 temp_f2;
    f32 temp_f2_2;
    f32 temp_f2_3;
    f32 temp_f2_4;
    f32 temp_f30;
    f32 temp_f30_2;
    f32 temp_f30_3;
    f32 temp_f30_4;
    f32 temp_f31;
    f32 temp_f31_2;
    f32 temp_f31_3;
    f32 var_f24;
    f32 var_f24_2;
    f32 var_f24_3;
    f32 var_f24_4;
    s32 temp_a1;
    s32 temp_a1_2;
    s32 temp_a1_3;
    s32 temp_a1_4;
    s32 var_s2;
    s32 var_s2_2;
    s32 var_s2_3;
    s32 var_s2_4;
    s32 var_v0;
    s32 var_v0_2;
    void *temp_v1;
    void *temp_v1_2;

    if (!(p->unk0.v[2] < 9255.658f)) {
        var_f24 = -1.0f;
        temp_f30 = -2700.0f;
        temp_f25 = 2700.0f;
        var_s2 = 0;
        temp_f28 = -3528.0f;
        temp_f31 = 9585.0f;
        temp_f29 = 0.70710677f;
        var_v0 = 0 * 4;
loop_2:
        temp_a1 = var_s2 * 0x10;
        temp_f2 = *(var_v0 + D_00523570);
        temp_f23 = temp_f31 - temp_f2;
        temp_f22 = p->unk80.m[var_s2].v[2] - temp_f23;
        temp_f20 = temp_f23 - p->unkC0.m[var_s2].v[2];
        if ((temp_f22 >= 0.0f) && (temp_f20 > 0.0f)) {
            temp_f21 = temp_f2 * temp_f29;
            temp_s0 = p + (temp_a1 + 0xC0);
            sceVu0SubVector(sp, p + (temp_a1 + 0x80), temp_s0);
            func_003E92C0(sp, temp_s0, sp, temp_f20 / (temp_f20 + temp_f22));
            if (((temp_f30 - temp_f21) < sp0) && (sp0 < (temp_f25 + temp_f21)) && ((temp_f28 - temp_f21) < sp4) && (sp4 < (-1176.0f + temp_f21)) && (var_f24 < temp_f22)) {
                var_f24 = temp_f22;
            }
        }
        var_s2 += 1;
        if (var_s2 < 4) {
            var_v0 = var_s2 * 4;
            goto loop_2;
        }
        if (var_f24 >= 0.0f) {
            temp_f1 = p->unk10.v[2];
            p->unk0.v[2] -= var_f24;
            if (temp_f1 > 0.0f) {
                p->unk10.v[2] = -temp_f1 * 0.4f;
            }
            goto block_45;
        }
        var_f24_2 = 1.0f;
        temp_f30_2 = -2700.0f;
        temp_f25_2 = 2700.0f;
        var_s2_2 = 0;
        temp_f31_2 = 9585.0f;
        temp_f23_2 = 12585.0f;
        temp_f29_2 = 0.70710677f;
        do {
            temp_a1_2 = var_s2_2 * 0x10;
            temp_f2_2 = D_00523570[var_s2_2];
            temp_f26 = -1176.0f + temp_f2_2;
            temp_f22_2 = p->unk80.m[var_s2_2].v[1] - temp_f26;
            temp_f20_2 = temp_f26 - p->unkC0.m[var_s2_2].v[1];
            if ((temp_f22_2 <= 0.0f) && (temp_f20_2 < 0.0f)) {
                temp_f21_2 = temp_f2_2 * temp_f29_2;
                temp_s0_2 = p + (temp_a1_2 + 0xC0);
                sceVu0SubVector(sp, p + (temp_a1_2 + 0x80), temp_s0_2);
                func_003E92C0(sp, temp_s0_2, sp, temp_f20_2 / (temp_f20_2 + temp_f22_2));
                if (((temp_f30_2 - temp_f21_2) < sp0) && (sp0 < (temp_f25_2 + temp_f21_2)) && ((temp_f31_2 - temp_f21_2) < sp8) && (sp8 < (temp_f23_2 + temp_f21_2)) && (temp_f22_2 < var_f24_2)) {
                    var_f24_2 = temp_f22_2;
                }
            }
            var_s2_2 += 1;
        } while (var_s2_2 < 4);
        if (var_f24_2 <= 0.0f) {
            temp_f1_2 = p->unk10.v[1];
            p->unk0.v[1] -= var_f24_2;
            if (temp_f1_2 < 0.0f) {
                p->unk10.v[1] = -temp_f1_2 * 0.4f;
            }
            func_003ECAD8(p);
        }
        var_f24_3 = 1.0f;
        temp_f30_3 = -2700.0f;
        var_s2_3 = 0;
        temp_f31_3 = 9585.0f;
        temp_f23_3 = 12585.0f;
        temp_f29_3 = 0.70710677f;
        var_v0_2 = 0 * 4;
loop_31:
        temp_a1_3 = var_s2_3 * 0x10;
        temp_v1 = p + temp_a1_3;
        temp_f2_3 = *(var_v0_2 + D_00523570);
        temp_f25_3 = temp_f30_3 + temp_f2_3;
        temp_f22_3 = (*(f32 *)((u8 *)(temp_v1) + 0x80)) - temp_f25_3;
        temp_f20_3 = temp_f25_3 - (*(f32 *)((u8 *)(temp_v1) + 0xC0));
        if ((temp_f22_3 <= 0.0f) && (temp_f20_3 < 0.0f)) {
            temp_f21_3 = temp_f2_3 * temp_f29_3;
            temp_s0_3 = p + (temp_a1_3 + 0xC0);
            sceVu0SubVector(sp, p + (temp_a1_3 + 0x80), temp_s0_3);
            func_003E92C0(sp, temp_s0_3, sp, temp_f20_3 / (temp_f20_3 + temp_f22_3));
            if (((-1176.0f - temp_f21_3) < sp4) && (sp4 < (0.0f + temp_f21_3)) && ((temp_f31_3 - temp_f21_3) < sp8) && (sp8 < (temp_f23_3 + temp_f21_3)) && (temp_f22_3 < var_f24_3)) {
                var_f24_3 = temp_f22_3;
            }
        }
        var_s2_3 += 1;
        if (var_s2_3 < 4) {
            var_v0_2 = var_s2_3 * 4;
            goto loop_31;
        }
        if (var_f24_3 <= 0.0f) {
            temp_f1_3 = p->unk10.v[0];
            p->unk0.v[0] -= var_f24_3;
            if (temp_f1_3 < 0.0f) {
                p->unk10.v[0] = -temp_f1_3 * 0.4f;
            }
block_45:
            func_003ECAD8(p);
        } else {
            var_f24_4 = -1.0f;
            temp_f30_4 = 2700.0f;
            temp_f29_4 = 0.70710677f;
            var_s2_4 = 0;
            do {
                temp_a1_4 = var_s2_4 * 0x10;
                temp_v1_2 = p + temp_a1_4;
                temp_f2_4 = D_00523570[var_s2_4];
                temp_f25_4 = temp_f30_4 - temp_f2_4;
                temp_f22_4 = (*(f32 *)((u8 *)(temp_v1_2) + 0x80)) - temp_f25_4;
                temp_f20_4 = temp_f25_4 - (*(f32 *)((u8 *)(temp_v1_2) + 0xC0));
                if ((temp_f22_4 >= 0.0f) && (temp_f20_4 > 0.0f)) {
                    temp_f21_4 = temp_f2_4 * temp_f29_4;
                    temp_s0_4 = p + (temp_a1_4 + 0xC0);
                    sceVu0SubVector(sp, p + (temp_a1_4 + 0x80), temp_s0_4);
                    func_003E92C0(sp, temp_s0_4, sp, temp_f20_4 / (temp_f20_4 + temp_f22_4));
                    if (((-1176.0f - temp_f21_4) < sp4) && (sp4 < (0.0f + temp_f21_4)) && ((temp_f31_3 - temp_f21_4) < sp8) && (sp8 < (temp_f23_3 + temp_f21_4)) && (var_f24_4 < temp_f22_4)) {
                        var_f24_4 = temp_f22_4;
                    }
                }
                var_s2_4 += 1;
            } while (var_s2_4 < 4);
            if (var_f24_4 >= 0.0f) {
                temp_f1_4 = p->unk10.v[0];
                p->unk0.v[0] -= var_f24_4;
                if (temp_f1_4 > 0.0f) {
                    p->unk10.v[0] = -temp_f1_4 * 0.4f;
                }
                func_003ECAD8(p);
            }
        }
    }
}
#else
LIT4_WORD(D_005F5B4C, 0x46109EA2);
LIT4_WORD(D_005F5B50, 0xC528C000);
LIT4_WORD(D_005F5B54, 0x4528C000);
LIT4_WORD(D_005F5B58, 0xC55C8000);
LIT4_WORD(D_005F5B5C, 0x4615C400);
LIT4_WORD(D_005F5B60, 0x3F3504F3);
LIT4_WORD(D_005F5B64, 0x3ECCCCCD);
LIT4_WORD(D_005F5B68, 0xC528C000);
LIT4_WORD(D_005F5B6C, 0x4528C000);
LIT4_WORD(D_005F5B70, 0x4615C400);
LIT4_WORD(D_005F5B74, 0x4644A400);
LIT4_WORD(D_005F5B78, 0x3F3504F3);
LIT4_WORD(D_005F5B7C, 0x3ECCCCCD);
LIT4_WORD(D_005F5B80, 0xC528C000);
LIT4_WORD(D_005F5B84, 0x4615C400);
LIT4_WORD(D_005F5B88, 0x4644A400);
LIT4_WORD(D_005F5B8C, 0x3F3504F3);
LIT4_WORD(D_005F5B90, 0x3ECCCCCD);
LIT4_WORD(D_005F5B94, 0x4528C000);
LIT4_WORD(D_005F5B98, 0x3F3504F3);
LIT4_WORD(D_005F5B9C, 0x3ECCCCCD);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EE598);
#endif

void func_003EEBF8(Fighter *f, BowlBody *b) {
    BowlSph *s = (BowlSph *)((u8 *)f + 0x1F0);
    FVec4 d;
    s32 i;
    s32 k;
    f32 dist;
    f32 lim;

    for (i = 0; i < 14; i++) {
        for (k = 0; k < 4; k++) {
            d[0] = b->unk80.m[k].v[0] - (f32)s->x;
            d[1] = b->unk80.m[k].v[1] - (f32)s->y;
            d[2] = b->unk80.m[k].v[2] - (f32)s->z;
            d[3] = 0.0f;
            dist = SqrtS(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            lim = D_00523570[k] + (f32)s->r;
            if (dist < lim) {
                b->unk178 = 0;
                func_003E92C0(b->unk0.v, b->unk0.v, d, (lim - dist) / dist);
                sceVu0ScaleVector(d, d, 150.0f / dist);
                func_003ECDE8(b, d, b->unk80.m[k].v);
                func_003ECAD8(b);
            }
        }
        s++;
    }
}

void func_003EED88(Fighter *a) {
    BowlBall *ball = &D_01FC51E0;
    BowlBody *p = D_01FC5260;
    BowlBody *q;
    s32 i;
    s32 j;

    if (D_005F70C0 != 0) {
        func_003EBB80();
        for (i = 10; i > 0; i--) {
            if (p->unk174 != 0) {
                func_003EC9A0(p);
                func_003EEBF8(a, p);
            }
            p++;
        }
    } else {
        func_003EC768(ball);
        for (i = 10; i > 0; i--) {
            if (p->unk174 != 0) {
                func_003EC9A0(p);
                func_003EDC90(ball, p);
            }
            p++;
        }
        func_003EE2F8(ball);
    }
    for (i = 10, p = D_01FC5260; i > 0; i--, p++) {
        if (p->unk174 != 0) {
            for (j = i - 1, q = p + 1; j > 0; j--, q++) {
                if (q->unk174 != 0) {
                    func_003EDF88(p, q);
                }
            }
            func_003ED338(p);
            func_003EE598(p);
        }
    }
    if (D_005F70C2 == 0 || D_005F70BC == 0) {
        func_003EC200();
    }
}

#if 0
/* NON_MATCHING: 131 lines (179 instructions, draft 125): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003EEF00(void) {
    f32 sp[64]; /* m2c stack */
    BMat sp40; /* m2c stack */
    f32 sp84; /* m2c stack */
    f32 sp88; /* m2c stack */
    f32 sp30;
    f32 sp34;
    f32 sp38;
    f32 sp50;
    s32 sp54;
    f32 sp58;
    f32 sp80;
    f32 sp90;
    f32 sp94;
    f32 sp98;
    s32 sp9C;
    BVec *temp_s0;
    BowlBody *var_s2;
    f32 temp_f12;
    f32 temp_f1;
    f32 temp_f20;
    s32 var_s1;

    var_s2 = D_01FC5260;
    sceVu0CopyMatrix((f32 (*)[4]) &sp40[0], D_01FC61A0);
    func_00353C88(0, &sp80);
    sp54 = 0;
    sp50 = -sp80 / sp84;
    sp58 = -sp88 / sp84;
    if ((D_01FC51E0.unk74 != 0) && (D_01FC51E0.unk8 < 12585.0f)) {
        func_003EC320(&D_01FC51E0.unk50, &D_01FC51E0.unk50);
        func_003E91D0(sp, D_01FC61A0, &D_01FC51E0.unk50);
        sp34 -= D_01FC51E0.unk4;
        sp38 -= D_01FC51E0.unk8;
        sp30 += D_01FC51E0.unk0;
        func_003D10D0(0, sp, 1.0f);
        temp_f12 = sp80 * sp80;
        temp_f1 = sp88 * sp88;
        temp_f20 = temp_f12 + temp_f1;
        __asm__ volatile(".word 0x46140504\n" ".word 0x460C0304\n");
        sp90 = -acosf(temp_f20 / (temp_f12 + (sp84 * sp84) + temp_f1));
        sp94 = acosf(sp88 / temp_f20);
        sp9C = 0;
        sp98 = -D_01FC51E0.unk50;
        func_003E9038(sp, D_01FC61A0, &sp90);
        sp34 -= D_01FC51E0.unk4;
        sp38 -= D_01FC51E0.unk8;
        sp30 += D_01FC51E0.unk0;
        sceVu0MulMatrix(sp, (f32 (*)[4]) &sp40[0], sp);
        func_003D10D0(2, sp, 1.0f);
    }
    var_s1 = 0xA;
    do {
        temp_s0 = &var_s2->unk40;
        var_s1 -= 1;
        if (var_s2->unk174 != 0) {
            func_003EC320(temp_s0->v, temp_s0->v);
            func_003E91D0(sp, D_01FC6160, temp_s0->v);
            sp30 += var_s2->unk0.v[0];
            sp34 -= var_s2->unk0.v[1];
            sp38 -= var_s2->unk0.v[2];
            func_003D10D0(1, sp, 1.0f);
            sceVu0MulMatrix(sp, (f32 (*)[4]) &sp40[0], sp);
            func_003D10D0(3, sp, 1.0f);
        }
        var_s2 += 0x180;
    } while (var_s1 > 0);
}
#else
LIT4_WORD(D_005F5BA0, 0x4644A400);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EEF00);
#endif

void func_003EF1D0(void) {
    s32 i;

    D_005F70BA = 0;
    D_005F70AA = 0;
    D_005F70C2 = 0;
    D_005F70BC = 0;
    D_01FC62C0 = D_01FC51E0;
    D_005F70B8 = D_005F70D4;
    for (i = 0; i < 2; i++) {
        func_003E92E0(D_01FC6340[i], 3);
    }
}

void func_003EF290(void) {
    D_005F70B6--;
    D_01FC51B0.unk24 = 3;
    D_005F70AA = 0;
    D_005F70C4 = 0;
    D_01FC51E0 = D_01FC62C0;
    D_005F70D4 = D_005F70B8 + 10;
    if (D_005F70C0 != 0) {
        D_01FC62A0.unk0 = 6;
    }
    Bowl_ResetPins();
    D_005F70C8 = 0;
    D_005F70BC = 1;
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523990);
s32 func_003EF348(void) {
    if (D_005F70C2 != 0 && D_005F70B6 == 0) {
        if (D_005F69B8++ & 0x30) {
            return Text_Printf(D_00523990, 0x30, 0x19C, 10, 5, 14);
        }
    }
}

s32 func_003EF3A8(BowlBody *b) {
    f32 dx = b->unk0.v[0] - 0.0f;
    f32 dz = b->unk0.v[2] - 8000.0f;

    if (b->unk10.v[0] == 0.0f && b->unk10.v[1] == 0.0f && b->unk10.v[2] == 0.0f && b->unk50.v[0] == 0.0f &&
        b->unk50.v[1] == 0.0f && b->unk50.v[2] == 0.0f) {
        return 0;
    }
    if (dx * dx + dz * dz > 9000000.0f) {
        if (b->unk170 == 0) {
            return 0;
        }
        b->unk170 = 0;
    }
    return 1;
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005239A8);
s32 Bowl_DrawMissFoul(void) {
    BowlS2 s = *(BowlS2 *)D_005F69D8;
    BowlW2 x = *(BowlW2 *)D_005F69E0;
    BowlBody *p = D_01FC5260;
    BowlW2 c = *(BowlW2 *)D_005F69E8;
    s32 n = 0;
    s32 i;

    if (D_005F70C0 == 0) {
        for (i = 0; i < 10; i++) {
            n += p->unk170;
            p++;
        }
        if (n < D_005F70B4) {
            return 0;
        }
    }
    if (D_005F70CE == 0) {
        Snd_PlaySe(0x10B1, 0);
        Snd_PlaySe(0x10B3, 0);
        Bgm_SetVolumeFade(0x20, 0x14);
        D_005F69BC = -20.0f;
        D_005F69C0 = 5.0f;
    } else {
        D_005F69BC += D_005F69C0;
        D_005F69C0 += 0.5f;
        if ((s32)D_005F69BC >= 0x169) {
            D_005F69BC = 360.0f;
            D_005F69C0 = -D_005F69C0 * 0.5f;
        }
    }
    Text_Printf(D_005239A8, x.v[D_005F70C0], (s32)D_005F69BC, 10, c.v[D_005F70C0], s.s[D_005F70C0]);
    if (D_005F70C0 != 0) {
        func_003DEB80(0);
        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
        Text_DrawId(0x1E, 0x186, 0x69, 0x60606080);
    }
    if (++D_005F70CE < 150) {
        return 1;
    }
    Bgm_SetVolumeFade(0x80, 0x14);
    return 0;
}

#if 0
/* NON_MATCHING: 30 lines: register allocation (t2 / a1) of the pin flag and the text x */
s32 Bowl_DrawSpare(void) {
    BowlW2 w = *(BowlW2 *)D_005F69F0;
    BowlS2 s = *(BowlS2 *)D_005F6A00;
    BowlW2 c = *(BowlW2 *)D_005F6A08;
    s32 i;
    s32 x = 0;
    s32 half;
    u16 k;
    BowlBody *p = D_01FC5260;

    if ((D_01FC61E0[D_005F70AC].unk0 & 1) == 0) {
        return 0;
    }
    if (D_005F70C0 != 0) {
        return 0;
    }
    for (i = 0; i < 10; i++) {
        x |= p->unk170;
        p++;
    }
    if (x != 0) {
        return 0;
    }
    k = D_005F70C6;
    half = (0x280 - w.v[k]) >> 1;
    if (D_005F70AC != 0) {
        x = 0x280 - D_005F70CE * 32;
        if (x < half) {
            x = half;
        }
    } else {
        x = D_005F70CE * 32 - w.v[k];
        if (half < x) {
            x = half;
        }
    }
    Text_Printf(D_005239A8, x, 0x168, 10, c.v[k], s.s[k]);
    if (D_005F70CE == 0) {
        if (D_005F70C6 != 0) {
            Snd_PlaySe(0x10AF, 0);
        } else {
            Snd_PlaySe(0x10B0, 0);
        }
        Snd_PlaySe(0x10B4, 0);
        Bgm_SetVolumeFade(0x20, 0x14);
    }
    if (++D_005F70CE < 150) {
        return 1;
    }
    Bgm_SetVolumeFade(0x80, 0x14);
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", Bowl_DrawSpare);
#endif

void func_003EF818(s32 n) {
    s32 save = D_005F6730;
    s32 i;
    s32 x;

    if (n >= 3) {
        D_005F6730 = 0xFFFF0;
        x = (0x284 - n * 0x24) >> 1;
        for (i = 0; i < n; i++) {
            Sprite_Draw(1, 0x47, 0x48, x, 0x128);
            x += 0x24;
        }
        D_005F6730 = save;
    }
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005239C8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005239D8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005239E8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_005239F8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523A08);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523A18);
#if 0
/* NON_MATCHING: 126 lines (147 instructions, draft 132): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void func_003EF8B0(s8 *arg0) {
    f32 sp[64]; /* m2c stack */
    s16 temp_v1;
    s16 temp_v1_2;
    s16 var_a1;
    s16 var_v0_2;
    s32 temp_v0_2;
    s32 temp_v0_3;
    s32 var_a0;
    s32 var_v0;
    s32 var_v0_3;
    u8 temp_v0;

    /* The colours (built at sp+0x20, copied to sp+0), the x of each message (D_005239C8), the messages (D_00523A18). */
    ((s32 *)sp)[0] = 2;
    ((s32 *)sp)[1] = 9;
    ((s32 *)sp)[2] = 0x12;
    ((s32 *)sp)[3] = 0xB;
    ((s32 *)sp)[4] = 0x12;
    ((s32 *)sp)[5] = (gFrameCount & 2) ? 0xB : 2;
    BOWL_SP(0x20, BowlB12) = *(BowlB12 *)D_005239C8; BOWL_SP(0x40, BowlB24) = *(BowlB24 *)D_00523A18;
    temp_v1 = D_005F6A10;
    if (D_005F70CE == 0) {
        D_005F6A12 = 0;
        var_a1 = 0;
loop_2:
        temp_v0 = *(arg0 + 1 + var_a1);
        if (!(temp_v0 & 1)) {
            var_v0 = var_a1 << 0x10;
            if (*(arg0 + 0xD + (s8) temp_v0) == 0xA) {
                temp_v1_2 = var_a1 + 1;
                temp_v0_2 = temp_v1_2 << 0x10;
                var_a1 = temp_v1_2;
                D_005F6A12 = temp_v1_2;
                if ((temp_v0_2 >> 0x10) >= 0xC) {
                    goto block_5;
                }
                goto loop_2;
            }
        } else {
block_5:
            var_v0 = var_a1 << 0x10;
        }
        temp_v0_3 = var_v0 >> 0x10;
        if (temp_v0_3 != 0) {
            if (temp_v0_3 == 0xC) {
                var_v0_2 = 5;
            } else {
                var_v0_3 = var_a1 << 0x10;
                if ((temp_v0_3 >= 3) && (var_v0_3 = var_a1 << 0x10, (((s32) ((*(u8 *)((u8 *)(arg0) + 1)) << 0x18) >> 0x19) == 0xC))) {
                    var_v0_2 = 4;
                } else {
                    var_v0_2 = var_a1 - 1;
                    if ((var_v0_3 >> 0x10) >= 4) {
                        var_v0_2 = 3;
                    }
                }
            }
            D_005F6A10 = var_v0_2;
            switch (var_a1) {                       /* irregular */
            case 1:
                var_a0 = 0x10B2;
block_22:
                Snd_PlaySe(var_a0, 0);
                break;
            case 3:
                var_a0 = 0x1046;
                goto block_22;
            case 12:
                var_a0 = 0x1058;
                goto block_22;
            default:
                Snd_PlaySe(0x1057, 0);
                break;
            }
            Snd_PlaySe(0x10B5, 0);
            goto block_25;
        }
    } else {
block_25:
        Text_Printf(D_005239A8, BOWL_SP(0x20 + temp_v1 * 2, s16), 0x168, 0xA, BOWL_SP(temp_v1 * 4, s32),
                    BOWL_SP(0x40 + temp_v1 * 4, s32));
        func_003EF818((s32) D_005F6A12);
        D_005F70CE = (u16) D_005F70CE + 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EF8B0);
#endif

#if 0
/* NON_MATCHING: written from the asm (T41 b1; the m2c draft stepped its BowlBody pointers by 0x180 elements, not
   bytes, and crashed on the first throw) */
/* The end of a throw: 2 when the next ball is due, 3 while the pins still move or a message is shown, 5 / 8 when
 * the frame (the pair of balls) is over (func_003EA4C8: 8 at the end of the game). */
s32 func_003EFB00(s32 arg0) {
    u8 *p;
    s32 i;
    s32 any;
    s16 old;
    s16 t;

    t = (u16)D_005F70AA + 1;
    D_005F70AA = t;
    if (arg0 != 0 && D_005F70BC != 0 && D_005F70C0 == 0) {
        for (i = 0, p = (u8 *)D_01FC5260; i < 10; i++, p += 0x180) {
            *(s32 *)(p + 0x170) = 0;
            *(s32 *)(p + 0x178) = 0;
        }
        goto done;
    }
    if (D_005F70B6 != 0 && D_005F70C2 == 0) {
        if (D_005F70BC != 0) {
            if ((s16)D_005F70BA < t) {
                return 2;
            }
        } else {
            old = D_005F70BA;
            any = 0;
            if ((u16)old == 0) {
                for (i = 10, p = (u8 *)D_01FC5260; i > 0; i--, p += 0x180) {
                    any |= *(s32 *)(p + 0x178);
                }
                D_005F70BA = t;
                if (any == 0) {
                    return 3;
                }
            } else {
                for (i = 10, p = (u8 *)D_01FC5260; i > 0; i--, p += 0x180) {
                    any |= *(s32 *)(p + 0x170);
                }
                if (any == 0) {
                    if (D_005F70C0 != 0) {
                        D_005F70B6 = 9;
                        D_005F70BA = 0x3C;
                        return 2;
                    }
                    D_005F70BA = (old + t) >> 1;
                    if (t < 0x5B) {
                        return 2;
                    }
                    D_005F70C2 = 1;
                    D_005F70B6 = 1;
                }
            }
        }
    }
    if (Bowl_DrawSpare() != 0) {
        return 3;
    }
    if (D_005F70AA < 0x3E8 && D_005F70CE == 0) {
        if (D_01FC51E0.unk8 < (D_005F70C0 != 0 ? 8000.0f : 12000.0f)) {
            return 3;
        }
        for (i = 10, p = (u8 *)D_01FC5260; i > 0; i--, p += 0x180) {
            if (func_003EF3A8((BowlBody *)p) != 0) {
                return 3;
            }
        }
    }
    if (D_005F70C2 != 0 && D_005F70B6 != 0) {
        return 2;
    }
    if (Bowl_DrawMissFoul() != 0) {
        return 3;
    }
    if (D_005F70AA < 0x96) {
        return 3;
    }
    if (D_005F70BC != 0 && D_005F70C0 == 0) {
        for (i = 0, p = (u8 *)D_01FC5260; i < 10; i++, p += 0x180) {
            *(s32 *)(p + 0x170) = 0;
            *(s32 *)(p + 0x178) = 0;
        }
    }
done:
    return func_003EA4C8(&D_01FC61E0[(u16)D_005F70AC]) == 0 ? 8 : 5;
}
#else
LIT4_WORD(D_005F5BA8, 0x463B8000);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003EFB00);
#endif

s32 func_003EFDB8(void) {
    FVec4 v;
    s32 i;

    for (i = 0; i < 25; i++) {
        if (func_003DB8A8(i) != 0) {
            func_003DB808(i, v);
            v[0] = -v[0];
            sceVu0SubVector(v, v, &D_01FC51E0.unk0);
            if (v[0] * v[0] + v[2] * v[2] < 90000.0f) {
                D_005F70AA = 0;
                func_003E97C8(2, D_005F70AC);
                func_003DB860(i);
                func_003EA4C8(&D_01FC61E0[D_005F70AC]);
                D_005F70CA = i < 13 ? -1 : 1;
                return 1;
            }
        }
    }
    return 0;
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523A30);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523A40);
s32 func_003EFED0(void) {
    if (D_005F70AA == 90) {
        Snd_PlaySe(0x1059, 0);
    }
    if (D_005F70AA >= 90) {
        func_003DEB80(0);
        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
        Text_DrawId(0x1E, 0x186, 0x69, 0x60606080);
        if (gFrameCount & 0x30) {
            Text_Printf(D_00523A30, D_005F70CA * 0xE0 + 0x10E, 0xC8, 10, 5, 12);
            Text_Printf(D_00523A40, D_005F70CA * 0x112 + 0x130, 0xCC, 2, D_005F70CA > 0 ? 0x7D : 0x7B);
        }
    }
    if (D_005F70AA >= 180) {
        if (Bowl_DrawMissFoul() != 0) {
            return 0;
        }
    }
    return D_005F70AA++ > 240;
}

#if 0
void func_003F1408(s32 n, s32 side);

/* NON_MATCHING: 465 lines (766 instructions, draft 734): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
void Bowl_DrawHud(void *f, void *o, void *pos, s16 pad, s16 arg4); /* menu_bg.c */
s32 func_003F0008(void) {
    BowlPlayer *temp_v0_2;
    Fighter *temp_s2;
    Fighter *temp_s3;
    s16 temp_s4;
    s16 temp_s5;
    s16 temp_v0_3;
    s32 temp_a0;
    s32 temp_a1;
    s32 temp_s0_2;
    s32 var_a2_2;
    s32 var_fp;
    s32 var_s0;
    s32 var_s0_2;
    s32 var_v0;
    s8 temp_s0;
    struct FighterMove *temp_v0_4;
    struct FighterMove *temp_v0_5;
    u16 temp_a0_2;
    u16 temp_v0;
    u16 var_a2;
    u8 *var_v0_2;

    var_fp = 0;
    D_005F5F20 = 5;
    if (D_005F5EE0 != 0) {
        D_005F70A8 = 9;
    }
    var_a2 = D_005F70AC;
    if (D_005F5CB0[var_a2] == 0) {
        temp_v0 = var_a2 ^ 1;
        D_005F70AC = temp_v0;
        var_a2 = temp_v0 & 0xFFFF;
    }
    temp_s2 = &gFighters[var_a2];
    temp_s3 = &gFighters[var_a2 ^ 1];
    temp_s4 = *(s16 *)((u8 *)D_00415718 + func_003AFFB8((s32) var_a2, (s32) temp_s2->unk0) * 4); /* lh */
    temp_s5 = *(s16 *)((u8 *)D_00415728 + func_003AFFB8((s32) D_005F70AC, (s32) temp_s2->unk0) * 4);
    if ((u32) D_005F70A8 < 0xBU) {
        switch (D_005F70A8) {                       /* switch 1 */
        case 0:                                     /* switch 1 */
            func_003E9C58(temp_s2, &D_01FC62A0);
            temp_s3->unk8 = 0;
            temp_s3->unkC = -0x3A98;
            temp_s3->unk4 = 0;
            func_003E9C10();
            func_0035C2B0();
            func_003E9FD8();
            temp_s0 = D_01FC61E0[D_005F70AC].unk0;
            if (temp_s0 & 1) {
                D_005F70B6 = 0;
                D_005F70C6 = D_01FC61E0->unk27[temp_s0 + ((D_005F70AC * 0x5E) - 1)] == 6;
            } else {
                D_005F70C6 = 0;
                D_005F70B6 = 3;
            }
            D_005F70AA = 0;
            D_005F70CC = 0x180;
            D_005F70CE = 0;
            D_005F70C8 = 0;
            D_005F70CA = 0;
            func_003EAC78();
            D_005F70A8 = (u16) D_005F70A8 + 1;
block_11:
            func_003ADFB0();
            if ((D_01FC62A0.unk0 == 0) || (D_01FC62A0.unk0 == 2) || (D_01FC62A0.unk0 == 3)) {
                temp_s2->unk127 = 0;
                D_01FC51E0.unk74 = 0;
            } else {
                D_01FC51E0.unk74 = 1;
                temp_s2->unk127 = 1;
            }
            temp_s3->unk127 = 0;
            func_003EAF78(temp_s2, &D_01FC62A0, temp_s4, temp_s5);
            func_003EBC98(temp_s2);
            func_003E9980();
            func_003DB690();
            if (D_01FC51E0.unk70 == 0) {
                if (Tag_StepCounters(D_005F70AC, 0) != 0) {
                    func_003E9FD8();
                } else {
                    func_003EBB80();
                }
            } else {
                func_003EC768(&D_01FC51E0);
            }
            func_003EEF00();
            func_003EA978();
            temp_a0_2 = (u16) D_005F70CC;
            if (D_005F70CC > 0) {
                if (D_01FC51E0.unk70 == 0) {
                    D_005F70CC = temp_a0_2 - 1;
                    func_003BC540(temp_a0_2);
                }
            }
            Bowl_DrawHud(temp_s2, D_01FC5260, &D_01FC51E0, temp_s4, D_01FC61E0[D_005F70AC].unk0 & 1);
            if (func_003F1ED8() != 0) {
                temp_v0_2 = &D_01FC61E0[D_005F70AC]; /* s7 holds %hi(D_01FC61E0) only (T41 b1) */
                if (temp_v0_2->unk58[0] >= 0) {
                    var_s0 = (temp_v0_2->unk58[1] <= -1) ? 1 : 2;
                } else {
                    var_s0 = (u8) temp_v0_2->unk0 & 1;
                }
                func_003DEB80(0);
                switch (D_01FC62A0.unk0) {          /* switch 2; irregular */
                case 1:                             /* switch 2 */
                    func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
                    var_a2_2 = (var_s0 * 4) + 0x5D;
block_43:
                    Text_DrawId(0x1E, 0x186, var_a2_2, 0x60606080);
                    break;
                case 2:                             /* switch 2 */
                    func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
                    var_a2_2 = (var_s0 * 4) + 0x5E;
                    goto block_43;
                case 3:                             /* switch 2 */
                    func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
                    var_a2_2 = (var_s0 * 4) + 0x5F;
                    goto block_43;
                case 4:                             /* switch 2 */
                    func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
                    Text_DrawId(0x1E, 0x186, (var_s0 * 4) | 0x60, 0x60606080);
                    break;
                }
            }
            if (func_003EFDB8() != 0) {
                Bowl_UpdateSpeed();
block_64:
                D_005F70A8 = 4;
            } else if (D_01FC51E0.unk8 > 4000.0f) {
                Bowl_UpdateSpeed();
                func_003EF1D0();
                (*(s16 *)((u8 *)(&D_01FC62A0) + 2)) = 5;
                D_005F70A8 = 3;
                D_005F70CE = 0;
            }
            break;
        case 1:                                     /* switch 1 */
            goto block_11;
        case 2:                                     /* switch 1 */
            func_003465E0(temp_a0);
            func_003EF290();
            D_005F70A8 = (u16) D_005F70A8 + 1;
            /* fallthrough */
        case 3:                                     /* switch 1 */
            var_s0_2 = 0;
            func_003ADFB0();
            if ((D_005F70C2 == 0) || (D_005F70B6 != 0) || (temp_v0_3 = (u16) D_005F70C4 ^ 1, D_005F70C4 = temp_v0_3, (temp_v0_3 != 0))) {
                var_s0_2 = 1;
            }
            temp_s3->unk127 = 0;
            if (D_005F70C0 != 0) {
                temp_s2->unk127 = 1;
                if (var_s0_2 != 0) {
                    func_003EAF78(temp_s2, &D_01FC62A0, temp_s4, temp_s5);
                    Skel_PlaceHitShapes(temp_s2);
                }
            } else {
                temp_s2->unk127 = 0;
            }
            func_003EBC98(temp_s2);
            func_0035C320();
            func_003E9980();
            func_003DB690();
            if (var_s0_2 != 0) {
                func_003EED88(temp_s2);
            }
            func_003EEF00();
            if ((D_005F70BC != 0) && (D_005F70C2 == 0)) {
                func_003E9808();
            }
            func_003EF348();
            Bowl_DrawSpeed();
            if (func_003EFDB8() != 0) {
                goto block_64;
            }
            D_005F70A8 = func_003EFB00((u32) ((temp_s5 & 0x800) << 0x10) >> 0x10);
            break;
        case 4:                                     /* switch 1 */
            func_003ADFB0();
            temp_s3->unk127 = 0;
            if (D_005F70C0 != 0) {
                temp_s2->unk127 = 1;
                func_0034E8B0(temp_s2);
            } else {
                temp_s2->unk127 = 0;
            }
            D_01FC51B0.unk24 = 7;
            func_003EBC98(temp_s2);
            func_0035C320();
            func_003E9980();
            func_003DB690();
            func_003EEF00();
            func_003EA978();
            Bowl_DrawSpeed();
            if (func_003EFED0() != 0) {
                D_005F70A8 = 8;
            }
            break;
        case 5:                                     /* switch 1 */
            func_003E99C0(temp_s3, (s32) D_005F70AC, 1, D_005F70AC ^ 1);
            if (temp_s3->unk2A == 0xF) {
                temp_s3->unk28 = (s16) (u16) temp_s2->unk28;
                func_00368168(temp_s3);
            }
            temp_v0_4 = func_00368218(temp_s2, func_003E9538((s32) temp_s2->unk28));
            temp_s2->unk188 = temp_v0_4;
            temp_s2->unkC0 = temp_v0_4;
            temp_s2->unkB8 = temp_v0_4;
            temp_v0_5 = func_00368218(temp_s3, func_003E9538((s32) temp_s3->unk28));
            temp_s3->unk188 = temp_v0_5;
            temp_s3->unkC0 = temp_v0_5;
            temp_s3->unkB8 = temp_v0_5;
            temp_s2->unkC = -0x1964;
            temp_s3->unkC = -0x251C;
            D_01FC51B0.unk24 = 4;
            temp_s3->unk4 = temp_s2->unk4 + (((D_005F70AC * 2) - 1) * 0x3E8);
            temp_s3->unk3C = 0;
            temp_s3->unk1A = 0;
            func_003E9530(1);
            func_00363150(temp_s2);
            func_00363150(temp_s3);
            Bgm_SetVolumeFade(0x20, 0x14);
            D_005F70A8 = (u16) D_005F70A8 + 1;
            /* fallthrough */
        case 6:                                     /* switch 1 */
            func_003ADFB0();
            temp_s2->unk127 = 1;
            temp_s3->unk127 = 1;
            temp_s0_2 = func_003E9628(temp_s2, temp_s3, 1);
            Snd_StartMoveVoices(temp_s2);
            if (temp_s2->unkDC != -1) {
                func_00376220(temp_s2, temp_s2);
            }
            Snd_StartMoveVoices(temp_s3);
            if (temp_s2->unkDC != -1) {
                func_00376220(temp_s3, temp_s3);
            }
            func_003EBC98(temp_s2);
            func_003E9980();
            func_003DB690();
            func_003EA978();
            func_003EF8B0(&D_01FC61E0[D_005F70AC]);
            if (temp_s0_2 == 0) {
                if ((temp_s5 | *(s32 *)((u8 *)D_00415728 + func_003AFFB8((s32) D_005F70AC, (s32) temp_s3->unk0) * 4)) & 0x800) {
                    goto block_82;
                }
            } else {
block_82:
                D_005F70A8 = (u16) D_005F70A8 + 1;
            }
            break;
        case 7:                                     /* switch 1 */
            func_003ADFA8(temp_a0);
            temp_s2->unk127 = 0;
            temp_a1 = D_005F70AC ^ 1;
            temp_s3->unk127 = 0;
            func_003E99C0(temp_s3, temp_a1, 0, temp_a1);
            func_00375428();
            Snd_StopVoiceSeq(temp_s2);
            Snd_StopVoiceSeq(temp_s3);
            Bgm_SetVolumeFade(0x80, 0x14);
            goto block_82;
        case 8:                                     /* switch 1 */
            func_003465E0(temp_a0);
            func_003ADFA8();
            temp_s2->unk127 = 0;
            temp_s3->unk127 = 0;
            var_v0 = D_005F70AC * 0x5E;
            if (D_01FC61E0->unk27[D_01FC61E0[D_005F70AC].unk0 + (D_005F70AC * 0x5E)] != 1) {
                Fighter_TagSwap(temp_s2->unk1E, 1, D_005F70AC);
                func_003D33F0(temp_s2->unk1E);
                func_003D33F8(temp_s2->unk1E);
                Fx_LoadFighterTex(temp_s2);
                var_v0 = D_005F70AC * 0x5E;
            }
            Bowl_NextBall(var_v0 + D_01FC61E0);
            if (*D_005F5CB0 != 0) {
                if (D_01FC61E0->unk42[0xA] < 0) {
                    D_005F70A8 = 0;
                } else {
                    goto block_89;
                }
            } else {
block_89:
                if ((D_005F5CB2 != 0) && ((*(s16 *)((u8 *)(D_01FC61E0) + 0xB4)) < 0)) {
                    D_005F70A8 = 0;
                } else {
                    D_005F70A8 = 0xA;
                }
            }
            break;
        case 9:                                     /* switch 1 */
            if (func_003B6220(temp_a0) == 0) {
                func_003ADFB0();
                if (D_005F5EE0 == 0) {
                    if (D_005F5F08 != 0) {
                        if (func_003B7A48(D_005F70AC) == 0) {
                            goto block_98;
                        }
                        goto block_100;
                    }
block_98:
                    temp_s2->unk127 = 1;
                    D_01FC5254[0] = 1;
                } else if (Tag_StepCounters(D_005F70AC, 0) != 0) {
block_100:
                    temp_s2->unk127 = 0;
                    D_01FC5254[0] = 0;
                }
                D_005F5EE4 = 1;
                if (temp_s3->unk127 != 0) {
                    func_003E9628(temp_s2, temp_s3, 1);
                } else {
                    func_0034E8B0(temp_s2);
                }
                func_003EBC98(temp_s2);
                func_003E9980();
                func_003DB690();
                func_003EEF00();
                func_003EA978();
                (*(s16 *)((u8 *)(&D_01FC62A0) + 2)) = 5;
                if (D_005F5EE0 == 0) {
                    if ((u16) D_005F70AE != 0) {
                        func_003EA868(0x3F, 0x80, 1);
                        if (temp_s5 & 0xFC) {
                            D_005F70AE = 0;
                            (*(s16 *)((u8 *)(&D_01FC62A0) + 2)) = 1;
                            D_005F70A8 = 1;
                            D_005F5EE4 = 0;
                            Bgm_SetVolumeFade(0x80, 0x78);
                        }
                    } else if (D_005F5F08 == 0) {
                        D_005F5EE4 = 0;
                        (*(s16 *)((u8 *)(&D_01FC62A0) + 2)) = 1;
                        D_005F70A8 = 1;
                        Bgm_SetVolumeFade(0x80, 0x78);
                    } else if ((func_003B7B38(0x3F) == 0) && (temp_s5 & 0x800)) {
                        func_003B7B40();
                        (*(s16 *)((u8 *)(&D_01FC62A0) + 2)) = 1;
                        D_005F70A8 = 1;
                        Bgm_SetVolumeFade(0x80, 0x78);
                    }
                }
            }
            break;
        case 10:                                    /* switch 1 */
            func_003ADFA8(temp_a0);
            if ((*(s16 *)((u8 *)(D_01FC61E0) + 0xB4)) < D_01FC61E0->unk42[0xA]) {
                var_v0_2 = D_0043B7F8;
                (*(s8 *)((u8 *)(D_0043B7F8) + 0x21)) = 1;
                (*(s8 *)((u8 *)(D_0043B7F8) + 0x1F)) = 1;
                (*(u8 *)((u8 *)(D_0043B7F8) + 0x20)) = 0U;
            } else {
                if (D_01FC61E0->unk42[0xA] < (*(s16 *)((u8 *)(D_01FC61E0) + 0xB4))) {
                    (*(s8 *)((u8 *)(D_0043B7F8) + 0x1F)) = 2;
                    (*(u8 *)((u8 *)(D_0043B7F8) + 0x20)) = 1U;
                    (*(s8 *)((u8 *)(D_0043B7F8) + 0x21)) = 0;
                } else {
                    (*(s8 *)((u8 *)(D_0043B7F8) + 0x1F)) = 3;
                    (*(s8 *)((u8 *)(D_0043B7F8) + 0x21)) = (s8) (D_005F5CBE ^ 1);
                    (*(u8 *)((u8 *)(D_0043B7F8) + 0x20)) = (u8) D_005F5CBE;
                }
                var_v0_2 = D_0043B7F8;
            }
            var_fp = 5;
            func_003F1408((s32) (*(u8 *)((u8 *)(var_v0_2) + 0x1A)), (s32) (*(u8 *)((u8 *)(var_v0_2) + 0x20)));
            func_003E9C10();
            func_003EBC98(temp_s2);
            break;
        }
    }
    Snd_UpdateVoices();
    if (func_003BB938() != 0) {
        func_003BB900(0xB4);
    } else if (D_005F70A8 == 1) {
        func_003EAB48();
    }
    func_003C65D0(&D_01FC62A0);
    return var_fp;
}
#else
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", jtbl_00523A50);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003F0008);
#endif

void func_003F0C00(void) {
    func_003C5790();
    D_005F5EE0 = 0;
    D_005F5F08 = 0;
    D_005F5EE4 = 0;
    Mtx_Unit(D_01FC61A0);
    sceVu0CopyMatrix(D_01FC6160, D_01FC61A0);
    D_01FC6160[3][1] = -250.6579f;
    func_003B8ED8();
    func_003A9688(0x2A);
    func_00393708(D_005F5CD0);
    Motion_LoadBanks(func_003A9680());
    func_003E9530(1);
    D_005F70D6 = 0;
    D_005F70A8 = 0;
    func_003E9A90();
    func_003671D0(D_005F70B0);
    D_005F70AC = 0;
    func_003E9C10();
    Bowl_ResetPins();
    func_003E9AE8();
    D_005F70AE = 0;
    func_003F1838();
    func_003E1BC8();
    Fx_StartStageLayers();
}

u8 func_003F0CC8(s32 side, s32 member) {
    return func_0035C738(D_00440940[side][member].unk0, D_00440940[side][member].unk4)->unk4;
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523A80);
void func_003F0D10(void) {
    struct {
        BowlStr2B s;
        char pad[7];
    } buf;
    s32 x = 0x280 - D_005F70EA * 2;
    s32 x2 = x + 0x280;

    buf.s = D_00523A80;
    memset(buf.pad, 0, 7);
    Text_Printf(buf.s.s, x, 0x1AC, 10, 4, 0x13);
    Text_Printf(buf.s.s, x2, 0x1AC, 10, 4, 0x13);
    D_005F70EA++;
    if (D_005F70EA >= 0x280) {
        D_005F70EA -= 0x140;
    }
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523AB0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523AC8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523AD8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523AF0);
void func_003F0E28(s32 x, s32 y, s32 rank) {
    BowlStr5 t = D_00523AB0;
    s32 c;

    if (rank < 5 && (gFrameCount & 0x30)) {
        rank = func_003F23F8(rank) - 1;
        c = 2;
        if (gFrameCount & 2) {
            c = 11;
        }
        if (gFrameCount & 0x40) {
            Text_Printf(D_00523AC8, x - 0x1A, y - 6, 10, 4, c);
            Text_Printf(D_00523AD8, x - 0x3C, y + 0xC, 10, 4, c);
        } else {
            Text_Printf(D_00523AF0, x - 0x20, y, 10, 5, c, t.s[rank]);
        }
    }
}

void func_003F0F40(s32 i, BowlPlayer *players) {
    u8 c0 = func_003F0CC8(i, 0);
    u8 c1 = func_003F0CC8(i, 1);
    BowlPlayer *p = &players[i];

    func_003ADFB0();
    func_0035B4B0(D_00415808, &D_00436530);
    func_003D1040();
    func_003F25D8(p, c0, c1, 0);
    func_003F0E28(0x8C, 0x15E, D_005F70F0[i]);
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523B00);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523B18);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523B30);
#if 0
/* NON_MATCHING: 162 lines (171 instructions): rewritten from the asm (T41 b0; the m2c draft dropped the Text_Printf
 * arguments and the panel call's), register allocation not tried. The original copies the result words
 * D_00523B00 and the format D_00523AF0 to its stack and prints from the copies. */
/* The result screen of a two-team game: per team the two members' faces, PLAYER n, DRAW / LOSERS / WINNERS (the
 * winner blinking), the total and the frame panel of `scores` (0x5E bytes a team). */
void func_003F0FF0(s32 winner, s32 draw, s32 scores) {
    char words[3][8];
    char fmt[13];
    BowlPlayer *p;
    s32 i;
    s32 k;
    s32 y;
    s32 w;
    s32 c;
    s32 kind;

    D_005F6738 = (0x8080 << 0x10) | 0x8080;
    for (i = 0; i < 2; i++) {
        y = i * 0xB0;
        *(BowlB24 *)words = *(BowlB24 *)D_00523B00;
        *(BowlB13 *)fmt = *(BowlB13 *)D_00523AF0;
        for (k = 0; k < 2; k++) {
            w = func_003F0CC8(i, k);
            func_003CC078(k * 0x40 + 0xD8, y + 0x40, 0);
            func_003CBFB8(w, k * 0x40 + 0xD8, y + 0x40, 0);
        }
        Text_Printf(D_00523B18, 0x30, y + 0x74, 0xA, 5, 0xE, i + 1);
        if (draw != 0) {
            w = 0x46;
            c = 0xA;
            kind = 0;
        } else if (i != winner) {
            w = 0x65;
            c = 0xC;
            kind = 1;
        } else {
            w = 0x73;
            c = (gFrameCount & 2) ? 2 : 0xB;
            kind = 2;
        }
        Text_Printf(fmt, 0x1D8 - w, y + 0x4E, 0xA, 8, c, words[kind]);
        p = (BowlPlayer *)((u8 *)scores + i * 0x5E);
        Text_Printf(D_00523B30, 0x19F, y + 0x6E, 0xA, 8, c, *(s16 *)((u8 *)p + 0x56));
        func_003C59D8(0x24, y + 0x80, 1, 1, 0xA, 1, p, NULL, -1);
        Sprite_SetBlend(1, 1);
        func_003F0E28(0x7B, y + 0x54, D_005F70F0[i]);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003F0FF0);
#endif

s32 func_003F12A0(s32 a) {
    s32 r = 1;
    s32 any = 0;
    s32 i;
    s16 *p;

    for (i = 0; i < 2; i++) {
        p = &D_005F70F0[i];
        if (*p < 5) {
            any = 1;
            if (Bowl_NameEntry(a, i, *p) != 0) {
                *p = 5;
            } else {
                r = 0;
            }
        }
    }
    func_003F2E58();
    if (any && r) {
        r = 0;
        func_003F1F30(D_005F70F4, D_005F70F6);
    }
    return r;
}

s32 func_003F1370(s32 t) {
    s32 save = D_005F6730;

    D_005F6730 = 0xFFFF0;
    D_005F6740 = 0x44;
    D_005F6738 = (u64)((t << 7) / 60) << 24;
    func_003CE5D8(0, 0, 0x280, 0, 0, 0x1C0, 0x280, 0x1C0, 1);
    D_005F6730 = save;
    return t == 60;
}

void func_003F1408(s32 n, s32 side) {
    s32 c[4];
    BowlPlayer *p;
    s16 *r;
    s32 o;

    memset(c, 0, 0x10);
    r = &D_005F70F0[side];
    D_005F70E8 = 0; 
    D_005F70EA = 0; 
    D_005F70EC = 0; 
    D_005F70F0[1] = 5; 
    D_005F70F0[0] = 5; 
    p = func_003E9438(side);
    c[side * 2] = func_003F0CC8(side, 0);
    c[side * 2 + 1] = func_003F0CC8(side, 1);
    *r = Bowl_InsertHighScore(p, c[side * 2], c[side * 2 + 1], 0);
    if (n >= 2) {
        o = side ^ 1;
        p = func_003E9438(o);
        c[o * 2] = func_003F0CC8(o, 0);
        c[o * 2 + 1] = func_003F0CC8(o, 1);
        D_005F70F0[o] = Bowl_InsertHighScore(p, c[o * 2], c[o * 2 + 1], *r + 1);
    }
    func_003F2398();
    func_003F2D98((s32(*)[2])c);
    D_005F70F4 = 0;
    D_005F70F6 = 0;
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523B40);
#if 0
/* NON_MATCHING: 118 lines (178 instructions, draft 171): m2c draft, only made to compile (stack copies as struct copies, VU0 and unknown instructions as inline asm) */
s32 func_003F1570(void) {
    BowlPlayer *temp_s0;
    s16 temp_a0;
    s16 temp_a0_2;
    s16 var_v0;
    s32 temp_s3;
    u16 temp_a1;
    u8 temp_s2;

    temp_s2 = (*(u8 *)((u8 *)(D_0043B7F8) + 0x20));
    temp_s0 = func_003E9438(0);
    temp_s3 = func_003712C0() & 0x8F4;
    func_003CD728();
    Text_Printf(D_00523B40, 0x70, 0x28, 0xA, 5, 0xF);
    temp_a1 = (u16) D_005F70EC;
    switch (D_005F70EC) {                           /* switch 1 */
    case 0:                                         /* switch 1 */
        if (temp_s0->unk42[0xA] != 0x12C) {
            if ((*(s16 *)((u8 *)(temp_s0) + 0xB4)) == 0x12C) {
                goto block_5;
            }
        } else {
block_5:
            if ((*D_005F70F0 >= 5) || (func_003F2418((u32) *D_005F70F0) == 0) || (temp_s0->unk42[0xA] != 0x12C)) {
                if ((D_005F70F2 < 5) && (func_003F2418((u32) D_005F70F2) != 0)) {
                    if ((*(s16 *)((u8 *)(temp_s0) + 0xB4)) == 0x12C) {
                        goto block_12;
                    }
                }
            } else {
block_12:
                D_005F70F6 = 1;
            }
            Snd_PlaySe(0x1058, 0);
        }
        if ((*D_005F70F0 >= 5) || (temp_s0->unk42[0xA] < 0xC8)) {
            var_v0 = temp_a1 + 1;
            if (D_005F70F2 < 5) {
                var_v0 = temp_a1 + 1;
                if ((*(s16 *)((u8 *)(temp_s0) + 0xB4)) >= 0xC8) {
                    goto block_20;
                }
            }
        } else {
block_20:
            D_005F70F4 = 1;
            var_v0 = temp_a1 + 1;
        }
        D_005F70EC = var_v0;
        /* fallthrough */
    case 1:                                         /* switch 1 */
        switch ((*(u8 *)((u8 *)(D_0043B7F8) + 0x1A))) { /* switch 2; irregular */
        case 2:                                     /* switch 2 */
            func_003F0FF0(temp_s2, (*(u8 *)((u8 *)(D_0043B7F8) + 0x1F)) == 3, temp_s0);
            break;
        case 1:                                     /* switch 2 */
            func_003F0F40((s32) temp_s2, temp_s0);
            break;
        }
        func_003F0D10();
        if (temp_s3 != 0) {
            D_005F70E8 = 0xBB8;
            if ((*D_005F70F0 < 5) || (D_005F70F2 < 5)) {
                Bgm_Play(5);
            } else {
                Bgm_SetVolumeFade(0, 0xF0);
            }
block_39:
block_40:
            D_005F70EC = (u16) D_005F70EC + 1;
        }
block_43:
        return 0;
    case 2:                                         /* switch 1 */
        temp_a0_2 = (s16) (u16) D_005F70E8;
        D_005F70E8 = (u16) D_005F70E8 - 1;
        if (func_003F12A0((s32) temp_a0_2) != 0) {
            D_005F70E8 = 0x12C;
            goto block_40;
        }
        goto block_43;
    case 3:                                         /* switch 1 */
        func_003F2E58();
        D_005F70E8 = (u16) D_005F70E8 - 1;
        if (((u16) D_005F70E8 & 0x8000) || (temp_s3 != 0)) {
            D_005F70E8 = 0;
            Bgm_SetVolumeFade(0, 0x3C);
            goto block_39;
        }
        /* Duplicate return node #44. Try simplifying control flow for better match */
        return 0;
    case 4:                                         /* switch 1 */
        func_003F2E58();
        temp_a0 = (s16) (u16) D_005F70E8;
        D_005F70E8 = (u16) D_005F70E8 + 1;
        return func_003F1370((s32) temp_a0);
    default:                                        /* switch 1 */
        return 1;
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", jtbl_00523B60);
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003F1570);
#endif

void func_003F1838(void) {
    D_005F70F8 = 0;
    D_005F70FA = func_003F1ED8();
    D_005F70FC = func_003F1EE8();
    D_005F70FE = func_003F1EF8();
    D_005F7100 = 0x15;
}

void func_003F1878(s32 v) {
    if (D_005F5C76 == 0) {
        D_005F7100 = 0x15;
        return;
    }
    D_005F7100 = v - 2;
}

void func_003F1898(s32 a, s32 b) {
    BowlQuad q = D_0043FD10[0x5E8 / sizeof(BowlQuad)];

    q.v[0].y += b << 4;
    q.v[1].y += b << 4;
    q.v[2].y += b << 4;
    q.v[3].y += b << 4;
    q.v[0].x = 0x75B0; 
    q.v[2].x = 0x75B0; 
    q.v[1].x = (a + 0xB9) * 16 + 0x6C00; 
    q.v[3].x = (a + 0xB9) * 16 + 0x6C00; 
    func_003BB680(&q, 0xFFEF0, 1);
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523B78);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523B98);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523BB8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523BD0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523BF0);
#if 0
/* NON_MATCHING: 135 lines (211 instructions): rewritten from the asm (T41 b0; the m2c draft dropped the arguments of
 * Ui_DrawWindow, the Text_Printf calls and func_003DBCF0), register allocation not tried. The original copies the
 * two word pairs to its stack. */
void Ui_DrawWindow(s32 x, s32 y, s32 w, s32 h, s32 z, u32 c0, u32 c1);
void func_003BB890(s32 which, s32 x, s32 y, s32 c);
void func_003E08D0(s32 a);
/* The bowling options window: HELP MESSAGES, SPECIAL EFFECTS, JUKEBOX (the track's title, or OFF) and EXIT, the
 * cursor row highlighted with its arrows, and the help line of rows 0-2. */
void func_003F1968(s32 a) {
    s32 str[2];
    s32 col[2];
    s32 c;
    s32 y;
    s32 jc;
    s32 js;

    str[0] = D_005F6A60[0];
    str[1] = D_005F6A60[1];
    col[0] = D_005F6A68[0];
    col[1] = D_005F6A68[1];
    Ui_DrawWindow(0x87, 0xAC, 0x172, 0x58, 0xFFEE, 0x4040, 0x4040);
    c = 5;
    if (D_005F70F8 == 0) {
        func_003BB890(1, 0x1C2, 0xC8, 5);
        c = 0xB;
        func_003F1898(0x123, 0xC8);
    }
    Text_Printf(D_00523B78, 0x9B, 0xC8, 0xA, 4, c, col[D_005F70FA], str[D_005F70FA]);
    c = 5;
    if (D_005F70F8 == 1) {
        func_003BB890(1, 0x1E3, 0xDC, 5);
        c = 0xB;
        func_003F1898(0x142, 0xDC);
    }
    Text_Printf(D_00523B98, 0x9B, 0xDC, 0xA, 4, c, col[D_005F70FC], str[D_005F70FC]);
    y = 0xF0;
    if (D_005F70FE != 0) {
        if (D_005F7100 != 0x15 && D_005F5C76 != 0) {
            jc = col[1];
            js = *(s32 *)((u8 *)&D_00455D80[D_005F7100] + 4);
        } else {
            js = str[0];
            jc = col[0];
        }
        if (D_005F5C76 == 0) {
            c = 0x16;
        } else if (D_005F70F8 != 2) {
            c = 0x11;
        } else {
            func_003BB890(0, 0x128, y, 5);
            c = 0xB;
            func_003BB890(1, 0x1DB, y, 5);
            func_003F1898(0x12A, y);
        }
        Text_Printf(D_00523BB8, 0x9B, y, 0xA, 4, c, jc, js);
    } else {
        Text_Printf(D_00523BD0, 0x9B, 0xF0, 0xA, 4, 0x11);
    }
    y += 0x14;
    if (D_005F70F8 == 3) {
        c = 0xB;
        func_003F1898(0x46, y);
    } else {
        c = 5;
    }
    Text_Printf(D_00523BF0, 0x9B, y, 0xA, 4, c);
    if (func_003B1468() != 0 && D_005F70F8 < 3) {
        func_003DEB80(0);
        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
        if (D_005F70F8 == 0) {
            func_003E08D0(D_005F70FA);
        }
        if (D_005F70F8 == 1) {
            func_003E08D0(D_005F70FC);
        }
        Text_DrawId(0x1E, 0x186, D_005F70F8 + 0x6A, 0x60606080);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003F1968);
#endif

#if 0
/* NON_MATCHING: 48 lines: the pad word is widened with sll/srl and the bit tests are scheduled after the row update in the original */
s32 Pause_BowlOptions(u16 held, s16 pressed) {
    s32 dy = ((held >> 14) & 1) - ((held >> 12) & 1);
    s32 dx = ((held >> 13) & 1) - ((held >> 15) & 1);
    s32 r = 0;

    D_005F70F8 = (D_005F70F8 + dy + 4) % 4;
    if (D_005F70F8 == 2 && !(D_005F70FE != 0 && D_005F5C76 != 0)) {
        if (dy != 0) {
            D_005F70F8 += dy;
        } else {
            D_005F70F8++;
        }
    }
    switch (D_005F70F8) {
    case 0:
        if (dx != 0) {
            D_005F70FA ^= 1;
        }
        break;
    case 1:
        if (dx != 0) {
            D_005F70FC ^= 1;
        }
        break;
    case 2:
        if (D_005F70FE != 0 && D_005F5C76 != 0) {
            D_005F7100 += dx;
            if (D_005F7100 < 7) {
                D_005F7100 = 0x15;
            } else if (D_005F7100 >= 0x16) {
                D_005F7100 = 7;
            }
            if (dx != 0 || (pressed & 0xFC)) {
                Bgm_Stop();
                if (D_005F7100 != 0x15) {
                    Bgm_Play(D_00455D80[D_005F7100].unk0);
                }
            }
        }
        break;
    default:
        r = (pressed & 0xFC) != 0;
        break;
    }
    if (dy != 0 || (D_005F70F8 != 3 && dx != 0)) {
        Snd_PlaySe(0x103F, 0);
    }
    func_003F1968(D_005F70FA);
    if ((pressed & 0x800) && dx == 0 && dy == 0) {
        r = 1;
    }
    if (r) {
        Snd_PlaySe(3, 0);
        func_003F1F18(D_005F70FA, D_005F70FC);
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", Pause_BowlOptions);
#endif

s32 func_003F1ED8(void) {
    return D_0046BC70.unk1FE;
}

s32 func_003F1EE8(void) {
    return D_0046BC70.unk200;
}

s32 func_003F1EF8(void) {
    return D_0046BC70.unk202;
}

s32 func_003F1F08(void) {
    return D_0046BC70.unk204;
}

void func_003F1F18(s16 a, s16 b) {
    D_0046BC70.unk1FE = a;
    D_0046BC70.unk200 = b;
    func_00361968();
}

void func_003F1F30(s32 a, s32 b) {
    D_0046BC70.unk202 |= a;
    D_0046BC70.unk204 |= b;
    func_00361968();
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523C00);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523C10);
void Bowl_InitHighScores(void) {
    BowlScore *s = D_0046BC70.rec;
    s32 i;

    for (i = 0; i < 5; i++, s++) {
        s->unk0 = 0;
        s->unk2 = D_00523C00[i][0];
        s->unk3 = D_00523C00[i][1];
        func_003D01C0(s->name, s->unk2);
        s->game = D_00523C10[i];
    }
    func_003F2398();
    D_0046BC70.unk204 = 0; 
    D_0046BC70.unk1FE = 1; 
    D_0046BC70.unk202 = 0; 
    D_0046BC70.unk200 = 1; 
}

s32 Bowl_InsertHighScore(BowlPlayer *g, s32 c0, s32 c1, s32 from) {
    BowlScore *s = &D_0046BC70.rec[4];
    s32 i;
    s32 j;

    for (i = 5; from < i && g->unk42[10] >= s->game.unk42[10]; i--, s--) {
        if (i < 5) {
            s[1] = s[0];
            s[1].unk1++;
        }
    }
    if (i < 5) {
        s++;
        s->unk2 = c0;
        s->unk3 = c1;
        s->unk0 = 1;
        s->name[0] = 'A';
        for (j = 1; j < 4; j++) {
            s->name[j] = 0;
        }
        s->game = *g;
    }
    return i;
}

void func_003F2398(void) {
    BowlScore *base = D_0046BC70.rec;
    BowlScore *p;
    BowlScore *q;
    s32 i;

    base[0].unk1 = 1;
    for (i = 1; i < 5; i++) {
        p = &base[i];
        q = p - 1;
        if (p->game.unk42[10] == q->game.unk42[10]) {
            p->unk1 = q->unk1;
        } else {
            p->unk1 = i + 1;
        }
    }
}

s32 func_003F23F8(s32 i) {
    return D_0046BC70.rec[i].unk1;
}

#if 0
/* NON_MATCHING: registers: the original copies the index to a1 first */
s32 func_003F2418(u32 i) {
    u8 c;

    if (i >= 5) {
        return 0;
    }
    c = D_0046BC70.rec[i].unk2;
    switch (c) {
    case 0x10:
    case 0x15:
    case 0x16:
    case 0xC:
        return 0;
    }
    return c != 4;
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003F2418);
#endif

void func_003F2480(u32 k) {
    BMat m;
    BMat r;
    FVec4 pos;
    s32 id;
    f32 s;

    switch (k) {
    case 0:
    case 1:
        id = 1;
        break;
    case 2:
        id = 4;
        break;
    case 3:
        id = 5;
        break;
    case 4:
    default:
        id = 6;
        break;
    }
    s = 1.0f;
    Mtx_Unit(m);
    sceVu0RotMatrixY(m, m, ((u8)gFrameCount - 0x80) * 3.1415927f * 0.0078125f);
    m[3][0] = -800.0f;
    m[3][1] = -730.0f;
    m[3][2] = -1478.0f;
    sceVu0RotMatrixX(m, m, -0.2f);
    m[3][1] -= -2300.0f;
    m[3][2] -= -12000.0f;
    func_003D10D0(id, m, s);
    *(u128 *)pos = *(u128 *)m[3];
    Mtx_Unit(r);
    r[1][0] = -1.0f;
    r[1][2] = -1.5f;
    r[1][1] = 0.0f;
    sceVu0MulMatrix(m, r, m);
    *(u128 *)m[3] = *(u128 *)pos;
    func_003D10D0(3, m, s);
}

RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523E08);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523E18);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523E30);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523E48);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523E60);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523E70);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523E80);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523EA0);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523EB8);
#if 0
/* NON_MATCHING: 227 lines (280 instructions): rewritten from the asm (T41 b0; the m2c draft dropped the Text_Printf
 * arguments and the panel call's, and counted into an uninitialised table), register allocation not tried. The
 * original copies the five tables and the format D_00523E60 to its stack. */
void func_003CF1C0(s32 a, s32 b, s32 x, s32 y);
extern const char D_00523E70[];
/* The score sheet of a one-player game: SCORE SHEET, the score (or RANK n with it) coloured by rank, the frame panel,
 * the two members' faces and names, and the count of each kind of throw (strike, spare...) over frames 2-25. */
void func_003F25D8(BowlPlayer *p, u8 c0, u8 c1, s32 d) {
    s32 rcol[4];
    s32 name[6];
    s32 rname[5];
    s32 col[6];
    char fmt[13];
    s32 count[7];
    s32 score;
    s32 rank;
    s32 c;
    s32 i;
    s32 y;
    s32 n;

    *(BowlB16 *)rcol = *(BowlB16 *)D_00523E08;
    *(BowlB24 *)name = *(BowlB24 *)D_00523E18;
    *(BowlB20 *)rname = *(BowlB20 *)D_00523E30;
    score = p->unk42[0xA];
    *(BowlB24 *)col = *(BowlB24 *)D_00523E48;
    *(BowlB13 *)fmt = *(BowlB13 *)D_00523E60;
    if (score == 0x12C) {
        rank = 0;
    } else if (score >= 0xE6) {
        rank = 1;
    } else if (score >= 0xAA) {
        rank = 2;
    } else {
        rank = (score >= 0x64) ? 3 : 4;
    }
    Text_Printf(fmt, 0xCB, 0x50, 0xA, 5, 0xA, D_00523E70);
    if (rank == 0) {
        c = rcol[(gFrameCount >> 1) & 1]; /* a perfect game blinks */
    } else {
        c = rcol[rank - 1];
    }
    if (d == 0) {
        Text_Printf(D_00523E80, 0x4B, 0xD4, 0xA, 8, c, score);
    } else {
        Text_Printf(D_00523EA0, 0x6A, 0xD4, 0xA, 8, c, rname[d - 1], score);
    }
    func_003C59D8(0x24, 0x64, 1, 1, 0xA, 1, p, NULL, -1);
    Sprite_SetBlend(1, 1);
    D_005F6738 = (0x8080 << 0x10) | 0x8080;
    func_003CC078(0xF0, 0xF0, 0);
    func_003CBFB8(c0, 0xF0, 0xF0, 0);
    func_003CF1C0(1, c0, 0xCC, 0x128);
    func_003CC078(0xF0, 0x140, 0);
    func_003CBFB8(c1, 0xF0, 0x140, 0);
    func_003CF1C0(1, c1, 0xCC, 0x178);
    Text_Printf(fmt, 0x16D, 0x103, 0xA, 5, 0x13, D_005F6AC8);
    for (i = 0; i < 7; i++) {
        count[i] = 0;
    }
    for (i = 2; i < 0x1A; i++) {
        count[p->unk27[i]]++;
    }
    for (i = 1; i < 7; i++) {
        y = (i < 5) ? i * 0x1A + 0x103 : i * 0x1A + 0xE9;
        Text_Printf(fmt, 0x150, y, 0xA, 5, col[i - 1], name[i - 1]);
        n = (i == 3) ? count[3] + count[4] : count[i];
        Text_Printf(D_00523EB8, 0x1DC, y, 0xA, 5, col[i - 1], n);
        if (i == 3) {
            i++;
        }
    }
    func_003F2480(rank);
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003F25D8);
#endif

void func_003F2A38(s32 y) {
    s32 save = D_005F6730;

    D_005F6730 = 0xFFF00;
    Sprite_SetBlend(1, 0);
    D_005F6738 = 0x40808080;
    D_005F6740 = 0x44;
    func_003CE5D8(0, y - 0x3F, 0x280, y - 0x3F, 0, y, 0x280, y, 1);
    D_005F6730 = save;
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523EC8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523ED8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523EE8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523EF8);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523F08);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523F18);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523F30);
#if 0
/* NON_MATCHING: 55 lines: saved-register assignment, the char/costume pointer and the score x selection */
void func_003F2AC0(BowlScore *s, s32 x) {
    s32 x1 = x - 0x1B;
    s32 x2 = x - 7;
    s32 hl = s->unk0;
    s32 c;
    s32 i;
    s32 score;
    s32 sx;

    if (hl) {
        func_003F2A38(x);
    }
    c = hl ? 11 : 15;
    Text_Printf(D_00523EC8, 0x10, x1, 10, c, s->unk1);
    switch (s->unk1) {
    case 1:
        Text_Printf(D_00523ED8, x2, 10, 5, c);
        break;
    case 2:
        Text_Printf(D_00523EE8, x2, 10, 5, c);
        break;
    case 3:
        Text_Printf(D_00523EF8, x2, 10, 5, c);
        break;
    default:
        Text_Printf(D_00523F08, x2, 10, 5, c);
        break;
    }
    D_005F6738 = 0x80808080;
    for (i = 0; i < 2; i++) {
        func_003CC078(i * 0x34 + 0xB0, x - 0x39, 0);
        func_003CBFB8((&s->unk2)[i], i * 0x34 + 0xB0, x - 0x39, 0);
    }
    score = s->game.unk42[10];
    c = hl ? 11 : 0x15;
    if (score < 10) {
        sx = 0x16C;
    } else {
        sx = score > 99 ? 0x120 : 0x146;
    }
    Text_Printf(D_00523F18, sx, x1 + 4, 10, 9, c, score, 0x198, x2 + 4, 4);
    Text_Printf(D_00523F30, 0x1D4, x1, 10, hl ? 11 : 15, s->name);
}
#else
INCLUDE_ASM("asm/nonmatchings/tekken_bowl", func_003F2AC0);
#endif

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523F40);
void Bowl_DrawTop5(void) {
    BowlScore *p = D_0046BC70.rec;
    s32 i;

    Text_Printf(D_00523F40, 0xB7, 0x40, 10, 5, 10);
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 1);
    for (i = 0; i < 5; i++) {
        func_003F2AC0(p, i * 0x47 + 0x87);
        p++;
    }
}

void func_003F2D98(s32 (*names)[2]) {
    BowlName *e;
    s32 i;

    D_005F7104 = 0;
    D_005F7102 = 0;
    for (i = 0; i < 2; i++) {
        e = &D_01FC6368[i];
        NameEntry_Init(e, i, names[i][0], names[i][1]);
        e->unk10 = func_003AFFB8(i, 0);
        e->unk11 = func_003AFFB8(i, 1);
    }
    D_005F7106 = 1;
    D_005F7107 = 10;
}

void func_003F2E58(void) {
    s32 save = D_005F6730;

    D_005F6730 = 0xFFFF0;
    Bowl_DrawTop5();
    D_005F6730 = save;
    D_005F7106 += D_005F7107;
    if (D_005F7106 < 11 || D_005F7106 > 0xE7) {
        D_005F7107 = -D_005F7107;
    }
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523F58);
s32 Bowl_NameEntry(s32 t, s32 side, s32 rank) {
    BowlScore *s;
    BowlName *e;
    s32 save;
    s32 y;
    s32 x;
    s32 done;
    s32 i;
    u64 c;

    s = &D_0046BC70.rec[rank]; 
    e = &D_01FC6368[side]; 
    y = rank * 0x47 + 0x80; 
    x = 0x1D4; 
    save = D_005F6730; 

    if (t < 0) {
        done = NameEntry_Step(e, 1);
        Snd_PlaySe(4, 0);
    } else {
        done = NameEntry_Step(e, 0);
        if (e->unk6) {
            Snd_PlaySe(4, 0);
        }
        if (e->unk7) {
            Snd_PlaySe(3, 0);
        }
    }
    Str_Copy(s->name, e->str);
    if (done) {
        s->unk0 = 0;
        return 1;
    }
    Text_Printf(D_00523F58, 0x1B0, y - 0x1E, 10, 5, 0, (t + 0x3B) / 60);
    for (i = 0; i < D_01FC6368[side].len; i++) {
        x += Text_Printf(D_005F6AD0, 10, s->name[i]);
    }
    c = D_005F7106;
    D_005F6730 = 0xFFFF0;
    D_005F6738 = (c << 8) | 0xFF | (c << 16) | 0x80000000;
    D_005F6740 = 0x44;
    func_003CE5D8(x, y, x + 0x30, y, x, y + 4, x + 0x30, y + 4, 1);
    D_005F6738 = 0x80808080;
    D_005F6730 = save;
    return 0;
}

void func_003F3108(void) {
    D_005F7104 = 0;
    D_005F7102 = 0;
}

INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523F68);
INCLUDE_RODATA("asm/nonmatchings/tekken_bowl", D_00523F88);
s32 Pause_BowlTop5(s16 pad) {
    s32 save = D_005F6730;
    BowlScore *s;
    s32 d;
    s32 y;

    D_005F6730 = 0xFFFF0;
    Text_Printf(D_00523F68, 0x70, 0x28, 10, 5, 15);
    if (D_005F7102 != 0) {
        s = &D_0046BC70.rec[D_005F7104];
        func_003F25D8(&s->game, s->unk2, s->unk3, s->unk1);
        if (pad & 0xFC) {
            Snd_PlaySe(3, 0);
            D_005F7102 = 0;
        }
    } else {
        y = D_005F7104 * 0x47 + 0x87;
        d = (((u16)pad >> 14) & 1) - (((u16)pad >> 12) & 1);
        if (d != 0) {
            Snd_PlaySe(3, 0);
            D_005F7104 = (D_005F7104 + d + 5) % 5;
        }
        func_003F2A38(y);
        Bowl_DrawTop5();
        if (pad & 0xFC) {
            if (D_0046BC70.rec[D_005F7104].game.unk42[10] == 300) {
                Snd_PlaySe(0x1058, 0);
            } else {
                Snd_PlaySe(4, 0);
            }
            D_005F7102 = 1;
        }
    }
    D_005F6730 = save;
    if (func_003B1468()) {
        func_003DEB80(0);
        func_003DBCF0(0, 0x1A3, 0x280, 0x30, 0xFFFE, 0x18181860);
        Text_DrawId(0x1E, 0x1A4, 0x6D, 0x60606080);
    } else {
        Text_Printf(D_00523F88, 0x48, 0x1B8, 10, 4, 5);
    }
    if (pad & 0x800) {
        D_005F7102 = 0;
        return 1;
    }
    return 0;
}
