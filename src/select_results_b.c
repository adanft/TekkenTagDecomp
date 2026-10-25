/* 0x3C3150-0x3CD298 (module select_results after the hud cut, docs/game_overview.md section 4;
 * docs/systems/modes.md). .rodata 0x51F6C0-0x520648 is its own (subsegment, every string and table included in place);
 * the start is a cut after src/hud.c (whose strings 0x51F678-0x51F6C0 stay in the generic chunk), the end is the
 * start of sprite2d [I]. */
#include "common.h"

#include "fighter.h"

/* A team's entry list in the team modes (D_0043B7F8 + 0x42 + side * 0x1E) [V offsets; I names]. */
typedef struct {
    /* 0x00 */ u8 id[8];      /* characters */
    /* 0x08 */ u8 val[8];     /* costumes */
    /* 0x10 */ s8 unk10[8];   /* 1 once the member fought, 2 when out (func_0035A1E8) */
    /* 0x18 */ u8 pad18[2];
    /* 0x1A */ u8 unk1A;
    /* 0x1B */ u8 n;          /* number of members */
    /* 0x1C */ u8 unk1C;
    /* 0x1D */ u8 unk1D;      /* won rounds */
} SelTeam;

/* A record of one team-mode match (the characters and costumes of both teams, id = chr * 8 + costume). */
typedef struct {
    /* 0x0 */ s16 a[2][2];
    /* 0x8 */ s16 b;
    /* 0xA */ s16 c;
    /* 0xC */ s16 d;
    /* 0xE */ s16 e;
} SelSlot;

/* A side's win counter in the versus modes. */
typedef struct {
    /* 0x0 */ u16 wins;
    /* 0x2 */ u16 pad2;
    /* 0x4 */ s16 unk4;
} VsRec;

/* The game setup (D_0043B7F8): +0 game type, the human / CPU sides, the match counters; the tail is used differently
 * per game type [V offsets; I names]. */
typedef struct {
    /* 0x00 */ u8 unk0;       /* game type */
    /* 0x01 */ u8 pad1;
    /* 0x02 */ u8 unk2;
    /* 0x03 */ u8 unk3;
    /* 0x04 */ u8 pad4[4];
    /* 0x08 */ u8 unk8;
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 unkA;       /* character select cursor option (modes.md) */
    /* 0x0B */ u8 unkB;       /* round time setting (5: no limit) */
    /* 0x0C */ u8 padC[3];
    /* 0x0F */ u8 unkF;       /* set once the mode was set up */
    /* 0x10 */ u8 unk10;
    /* 0x11 */ u8 unk11;
    /* 0x12 */ u8 unk12;
    /* 0x13 */ u8 unk13;
    /* 0x14 */ u8 pad14[6];
    /* 0x1A */ u8 unk1A;      /* number of human players */
    /* 0x1B */ u8 unk1B;      /* human sides, bit per side */
    /* 0x1C */ u8 unk1C;      /* the human side */
    /* 0x1D */ u8 unk1D;      /* the CPU side */
    /* 0x1E */ u8 unk1E;
    /* 0x1F */ u8 unk1F;      /* match result kind (1, 2: a side won; 3: draw) [I] */
    /* 0x20 */ u8 unk20;      /* winner side */
    /* 0x21 */ u8 unk21;      /* loser side */
    /* 0x22 */ u8 pad22[2];
    /* 0x24 */ s32 unk24;     /* matches won in a row / stage number */
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ u8 pad38[8];
    /* 0x40 */ union {
        struct {
            u8 unk40;          /* current match record */
            u8 unk41;
            SelTeam team[2];
            SelSlot slot[15];
        } t;
        struct {
            s32 unk40;
            s32 unk44;
        } a;
        struct {
            VsRec rec[2];
            u16 unk4C;          /* draws */
            u8 pad4E[2];
            s32 unk50;
        } v;
    } u;
} GameSetup;

extern GameSetup D_0043B7F8;

/* One side of the select state (0x1C bytes) [I]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
} SelSide;

/* The state block passed to the select / result steps [I]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14[2];
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ SelSide side[2];
} SelState;

extern s16 gMode;
extern s16 gSubMode;

/* Records block D_00440D50 (same layout as src/sprite2d.c's S2aRecords). */
typedef struct {
    /* 0x0 */ s32 score;
    /* 0x4 */ u8 c0;
    /* 0x5 */ u8 c1;
    /* 0x6 */ u8 name[4];
    /* 0xA */ u8 padA[2];
} ScoreEnt;

typedef struct {
    /* 0x0 */ u8 c0;
    /* 0x1 */ u8 c1;
    /* 0x2 */ u16 time;
    /* 0x4 */ u8 name[4];
} TimeEnt;

typedef struct {
    /* 0x0 */ u32 score;
    /* 0x4 */ u16 side;
    /* 0x6 */ u8 c0;
    /* 0x7 */ u8 c1;
} RecSum;

typedef struct {
    /* 0x000 */ ScoreEnt *lastScore;
    /* 0x004 */ TimeEnt *lastTime;
    /* 0x008 */ ScoreEnt score[10];
    /* 0x080 */ TimeEnt time[10];
    /* 0x0D0 */ RecSum sum[8];
    /* 0x110 */ s16 unk110;
    /* 0x112 */ u8 unk112;
    /* 0x113 */ u8 unk113;
    /* 0x114 */ u8 unk114;
    /* 0x115 */ u8 unk115;
    /* 0x116 */ u8 unk116;
    /* 0x117 */ u8 pad117[3];
    /* 0x11A */ s16 unk11A;
    /* 0x11C */ u8 pad11C[4];
    /* 0x120 */ s16 unk120;
    /* 0x122 */ u16 unk122;
    /* 0x124 */ u16 unk124;
    /* 0x126 */ u16 unk126[10];
    /* 0x13A */ u8 pad13A[0x36];
    /* 0x170 */ s32 unk170;
    /* 0x174 */ u16 unk174[34];
    /* 0x1B8 */ u16 unk1B8;
    /* 0x1BA */ u16 unk1BA;
    /* 0x1BC */ u16 unk1BC;
} Records;

extern Records D_00440D50;

void Bgm_Play(s32 track);
void Bgm_SetVolumeFade(s32 vol, s32 frames);
void Bgm_Stop(void);
void func_00343F88(void);
void func_00344BE0(void);
void func_00346600(void);
s32 func_00358A98(s8 *p);
s32 func_0035CD48(s32 character);
void func_0035D078(void);
void func_0035D090(void);
s32 func_0035D0C8(s32 n, u32 dest);
s32 func_0035D120(s32 abort);
s32 func_0035D1C8(s32 n);
void func_00361968(void);
s32 func_00361978(void);
void func_00361AA8(void);
void func_003637D0(void);
void func_003638F0(void);
u64 * func_003639E0(void);
s32 func_003800D8(s32 n);
void func_003992C8(s32 value);
s32 func_0039C778(void);
void func_003A6BD0(void);
void CharSet_Copy(void *a, void *b);
s32 func_003ADA08(u8 *set, s32 i);
void func_003ADA18(s32 chr, void *buf);
s32 CharSet_Count(u32 *set);
void func_003ADB70(s32 side, s32 member, s32 n);
s32 func_003ADDD8(s32 side);
void func_003ADFA8(void);
void func_003ADFB0(void);
s32 func_003AE608(s32 a, s32 b);
s32 func_003AE670(void);
s32 func_003AE698(void);
void func_003AE738(s32 a, s32 b);
s32 func_003AFFB8(s32 i, s32 a);
s32 func_003B0078(s32 a, s32 b);
s32 func_003B01B0(s32 port);
s32 func_003B0340(void);
void func_003B0380(s32 side);
s32 func_003B0458(void);
s32 func_003B0818(void);
s32 func_003B1198(void);
s32 func_003B1378(void);
void Time_FramesToClock(s32 t, s32 *a, s32 *b, s32 *c);
void func_003B7A68(void);
s32 func_003BA450(s32 side);
void func_003BAA90(void);
s32 func_003BB8B8(s32 d, s32 lim);
void func_003BB900(s32 x);
s32 func_003BB938(void);
void func_003BBC40(void);
void func_003BBD88(s32 right, s32 x, s32 y); /* defined with one parameter in src/modes_b.c; this caller passes three */
s32 func_003BBFD0(s32 i);
void func_003BC828(s32 side, s32 x, s32 y, s32 c);
s32 func_003C0098(void);
s32 func_003C0210(s32 chr, s32 c);
void func_003C4868(void);
void func_003C6738(s32 a, s32 n, s32 x, s32 y);
void func_003C9358(s32 a);
void func_003CD728(void);
void func_003CD7E8(s32 buf, s32 i);
void func_003CDC10(s32 a);
void func_003CE418(s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 du, s32 dv, s32 tw, s32 th, s32 c0, s32 c1, s32 c2, s32 c3);
void func_003CE508(s32 a, s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 du, s32 dv, s32 tw, s32 th);
void func_003CE5D8(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, s32 tex);
s64 func_003CEA80(s32 n, u64 *dest, s32 size);
void func_003CEB68(s32 img);
void func_003CEC98(s32 img);
void func_003CF1C0(s32 a, s32 b, s32 x, s32 y);
void func_003CF1E8(s32 i, s32 x, s32 y);
s32 func_003CF230(s32 chr, s32 costume);
s32 func_003CF3D8(u32 v);
void func_003CF408(f32 *v, f32 angle);
void func_003CF608(s32 a);
void func_003CFE38(s32 x, s32 y, s32 w, s32 h, u8 *c);
void func_003D01C0(u8 *dst, u32 i);
void func_003DB690(void);
void func_003DBAA8(u32 x0, u32 x1, u32 y0, u32 y1);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 b, s32 color);
void func_003DBE98(u32 x, u32 y, u32 w, u32 h, u32 z, u32 c0, u32 c1, u32 c2, u32 c3);
void Fx_StartStageLayers(void);
u32 Math_Permille(u32 a, u32 b);
void Mode_Change(s32 mode);
u32 Num_ToBcd(u32 v);
s32 Pad_IsMultitap(void);
void Snd_PlaySe(s32 id, s32 player);
void Sprite_Draw(s32 a, s32 b, s32 c, s32 x, s32 y);
void Sprite_DrawBlended(s32 a, s32 b, s32 c, s32 x, s32 y);
void Sprite_DrawCurrentGouraud(s32 a);
void Sprite_DrawCurrentRotated(s32 a);
void Sprite_DrawScaled(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);
void Sprite_DrawScaledBlended(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);
s32 Sprite_LoadBank(s32 bank, u64 *dest, s32 size);
void Sprite_SetBlend(s32 a, s32 b);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_SetTexture(s32 tex, s32 clut);
void Sprite_TexFlush(s32 a);
void Sprite_UploadBank(s32 bank, u64 *data, s32 size, s32 r);
s32 Text_Printf(const char *fmt, ...);
u8 *Str_Copy(u8 *dst, const u8 *src);
s32 Text_Printf(const char *fmt, ...);

/* this file */
void func_003B7888(void);
extern s16 D_005F6EB4;
extern s16 D_00440E60[];
extern u8 D_00440E62[];
s32 func_003C80E8(Records *r);
s32 func_003C83A8(Records *r);

extern s16 D_005F66FA;
/* D_00440CE8: a header then 0x1C-byte entries [V offsets]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 pad8[0x14];
} Unk440D04;

typedef struct {
    /* 0x00 */ u8 pad0[0x10];
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u8 pad14[8];
    /* 0x1C */ Unk440D04 item[4];
} Unk440CE8;

extern Unk440CE8 D_00440CE8;
extern s32 D_005F6EB0;
extern s32 D_005F6E68;
extern s32 D_005F6E6C;
extern s32 D_005F6E70;
extern const s32 D_0051FE98[];
extern const s32 D_0051FE30[];
extern const s32 D_00520508[];
void Records_InitDefault(void);
extern s32 D_00440A58[];
extern s8 D_005F6EAA;
extern s8 D_005F6EAB;
extern s8 D_005F6EAC;
extern s8 D_005F6EAD;
extern s16 D_005F6EA4;
/* D_00440A68: 0xC-byte entries. */
typedef struct {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 unk1;
    /* 0x2 */ u8 unk2;
    /* 0x3 */ u8 unk3;
    /* 0x4 */ u8 unk4;
    /* 0x5 */ u8 unk5;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s16 unk8;      /* x */
    /* 0xA */ s16 unkA;      /* y */
} Unk440A68;
extern Unk440A68 D_00440A68[];
extern const u8 D_00520460[];
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ u8 unk6;
    /* 0x7 */ u8 unk7;
} Unk440E20;
extern Unk440E20 D_00440E20[8];
extern s32 D_005F6700[2];
void func_003CF608(s32 a);
extern const u8 D_00520488[];
typedef struct {
    /* 0x0 */ s8 unk0;
    /* 0x1 */ s8 unk1;
    /* 0x2 */ u8 pad2[6];
} Unk4157F8;
extern Unk4157F8 D_004157F8[];
void func_003C81D8(Records *r);
void func_003C8428(Records *r);
void func_003C8658(Records *r);
extern u16 D_00440E6E[];
extern const char D_00520368[];
extern s32 D_005F6730;
extern u64 D_005F6738;
extern const u8 D_00520140[];
/* An 8-byte pair of words (the size tables copied to the stack). */
typedef struct {
    s32 a;
    s32 b;
} WordPair;
typedef struct {
    WordPair e[2];
} WordPair2;
extern const WordPair2 D_005205B8;
s32 func_003AFF08(s32 port);
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
} Unk1FAC9B0;
extern Unk1FAC9B0 D_01FAC9B0;
extern const char D_00520308[];
extern const char D_00520320[];
s32 func_003C8270(TimeEnt *t, TimeEnt *last);
extern const char D_005202E8[];
extern const char D_00520338[];
extern const char D_00520350[];
extern s32 D_005F6E7C;
extern s32 D_005F6E80;
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s16 unk8;
    /* 0x0A */ s16 unkA;
    /* 0x0C */ s16 unkC;
    /* 0x0E */ s16 unkE;
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s16 unk14;
    /* 0x16 */ s16 unk16;
} Unk1FAC9C8;
extern Unk1FAC9C8 D_01FAC9C8[];
void func_003CE508(s32 a, s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 du, s32 dv, s32 tw, s32 th);
void func_003C7AB0(s32 i, TimeEnt *e, s32 a, s32 y, s32 b);
void func_003C75E8(s32 i, s32 idx, ScoreEnt *e, s32 a, s32 y, s32 b);
extern const s32 D_00520638[];
extern const char D_0051FA70[];
extern const char D_0051FA88[];
extern u64 D_005F6740;
u64 *func_003B8830(void);
typedef struct {
    s32 x[2];
    s32 tex;
} Unk5205D8;
extern const Unk5205D8 D_005205D8[];
extern TimeEnt D_00440DD0[];
extern s32 D_005F6E8C;
void func_003C7F60(s32 i, s32 idx, s32 v, s32 w, s32 a, s32 b, s32 y, s32 c);
typedef struct {
    WordPair e[3];
} WordPair3;
extern const WordPair3 D_00520448;
extern const WordPair2 D_005205A8;
extern const s32 D_00415718[];
extern const s32 D_00415748[];
extern s32 D_005F6ECC;
void func_003C6738(s32 a, s32 n, s32 x, s32 y);
extern u64 D_005F6748;
/* D_00440F20: the current sprite (src/sprite2d.c S2aSprite). */
typedef struct {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ s32 x;
    /* 0x0C */ s32 y;
    /* 0x10 */ s32 w;
    /* 0x14 */ s32 h;
    /* 0x18 */ s32 u;
    /* 0x1C */ s32 v;
    /* 0x20 */ s32 tw;
    /* 0x24 */ s32 th;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ f32 rot;
    /* 0x3C */ s32 unk3C;
} CurSprite;
extern CurSprite D_00440F20;
extern s16 D_005F6EA8;
extern s32 D_005F6708[2];
extern u8 D_005F6EC8[2];
extern const SelState D_0051FFA0[];
typedef struct {
    s32 idx;
    s32 val;
} Pair;
s32 Sort_PairsByValue(Pair *a, s32 n, s32 desc);
void func_003C68A8(SelState *s, s32 kind, s32 i);
extern s32 D_005F6E98[2];
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u8 pad4[0x5C];
} Unk440C28;
extern Unk440C28 D_00440C28[];
extern u8 D_0043B968[];
extern u8 D_0043B990[];
typedef struct {
    /* 0x00 */ s32 unk0;      /* load step */
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;      /* frame counter */
    /* 0x0C */ u8 padC[0x40];
    /* 0x4C */ s32 kind[4];
    /* 0x5C */ s32 tex[4];
    /* 0x6C */ s32 clut[4];
    /* 0x7C */ u64 *dest[5];
    /* 0x90 */ s32 size[5];
} Unk3C44B0;
typedef struct {
    s32 blend;
    s32 c;
    s32 a;
} Unk51FD30;
extern const Unk51FD30 D_0051FD30[][3];
typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ f32 unk4;
    /* 0x8 */ f32 unk8;
    /* 0xC */ f32 unkC;
} Unk3C65D0;
s32 func_003E9450(void);
void func_003C6280(void);
void func_003C6410(f32 a);
void func_003C64E8(f32 a);
typedef struct {
    WordPair e[10];
} WordPair10;
extern const WordPair10 D_0051FCE0;
s32 func_003E9478(s32 i);
extern s8 D_00440D40[2][8];
extern s16 D_005F66F8;
void func_003BF7F0(void);
void func_003CA470(SelState *s, s32 side);
extern ScoreEnt D_00440D58[];
extern const char D_00520210[];
extern const char D_00520220[];
extern const char D_00520230[];
extern const char D_00520240[];
extern const char D_00520250[];
void func_003B77B8(void);
void func_003BEAB0(s32 side, s32 member, s32 chr, s32 cos);
extern SelSide D_00440D08[];
s32 func_003BFF48(u8 *a, u8 *b);
typedef struct {
    u8 c[10][2];
} CharPair10;
extern const CharPair10 D_00520388;
extern const CharPair10 D_005203A0;
extern const u8 D_0051E850[];
extern TeamMember D_00440940[2][2];
typedef struct {
    s32 unk0;
    s32 unk4;
    s32 unk8;
} Unk51F760;
extern const Unk51F760 D_0051F760[];
/* The character slots of the select screen (four portraits) [I]. */
typedef struct {
    /* 0x00 */ u8 pad0[0xC];
    /* 0x0C */ s32 a[4];
    /* 0x1C */ s32 b[4];
    /* 0x2C */ s32 c[4];
    /* 0x3C */ s32 d[4];
    /* 0x4C */ s32 e[4];
    /* 0x5C */ s32 f[4];
    /* 0x6C */ s32 g[4];
} CharSlots;
void func_003C3150(CharSlots *p);
extern s32 D_005F6720;
void func_003BBC40(void);
void func_003BE8C0(void);
void func_003BF4B8(GameSetup *g, SelState *s);
void func_003B6408(void);
void func_003BE3E8(GameSetup *g, SelState *s);
void func_003CC6C8(SelState *s);
void func_003CC3E0(SelState *s);
void func_003CC9E0(SelState *s);

/* this file */
void func_003C3150(CharSlots *p);
void func_003C42D8(CharSlots *p);
void func_003C44B0(Unk3C44B0 *p);
void func_003C4828(void);
void func_003C4868(void);
void func_003C4930(void);
void func_003C4990(s32 r, s32 g, s32 b, s32 a);
void func_003C4A28(s32 k);
void func_003C4AE0(void);
s32 func_003C4AF8(s32 t);
void func_003C4BA0(u32 t, u32 *out);
void func_003C4BD8(s32 t, u32 *out);
void func_003C4C00(u32 t, u32 *min, u32 *sec);
void func_003C52E8(void);
void func_003C5790(void);
void func_003C57C8(void);
void func_003C5848(s32 x, s32 y, u8 *p, s32 k, s32 j, s32 flag);
void func_003C3370(s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 bw, s32 bh);
void func_003C3648(s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 bw, s32 bh);
void func_003C3DC0(void);
s32 func_003C45B0(void);
void func_003C9518(SelState *s, SelSide *d, s32 pl);
void func_003CB610(GameSetup *g, SelState *s);
void func_003C6280(void);
void func_003C6410(f32 a);
void func_003C64E8(f32 a);
void func_003C65D0(Unk3C65D0 *s);
void func_003C6738(s32 a, s32 n, s32 x, s32 y);
void func_003C6888(void);
void func_003C68A8(SelState *s, s32 kind, s32 i);
void func_003C68F0(SelState *s, s32 kind);
void func_003C6BD0(void);
s32 func_003C71F0(s32 i);
void func_003C7208(Records *r, u8 *name);
void Records_DrawRank(s32 rank, s32 y, s32 c);
void func_003C73A0(s32 t, s32 x, s32 y, s32 c, s32 w);
void func_003C7428(s32 v, s32 y, s32 c);
void func_003C74A8(s32 v, s32 y);
void func_003C7530(s32 i, s32 v, s32 x, s32 y);
void func_003C7598(s32 x, s32 y, s32 c);
void func_003C7F60(s32 rank, s32 k, s32 a, s32 b, s32 c, s32 x, s32 y, s32 unused);
s32 func_003C80E8(Records *r);
void func_003C81D8(Records *r);
s32 func_003C8270(TimeEnt *t, TimeEnt *last);
s32 func_003C83A8(Records *r);
void func_003C8428(Records *r);
void func_003C84B8(Records *r);
void func_003C8658(Records *r);
void func_003C8710(s32 i);
void func_003C8760(void);
void func_003C8768(Records *r);
void func_003C8808(Records *r);
void Records_InitDefault(void);
void func_003C8A38(void);
void func_003C8A50(void);
void func_003C9358(s32 v);
void func_003C9368(void);
void func_003C9378(void);
void func_003C9388(void);
void func_003C9398(void);
void func_003C93A0(s32 i);
void func_003C9458(s32 y);
void func_003C94C0(s32 i);
void func_003C94F0(void);
s32 func_003C9510(void);
void func_003C9778(SelState *s);
void func_003C9888(SelState *s);
s32 func_003C98A8(SelState *unused, SelState *s);
s32 func_003C98C8(SelState *s);
void func_003C98E8(SelState *a, SelState *s, s32 side);
void func_003C99C8(s32 side, s32 x);
void func_003C9E70(SelState *s);
void func_003C9E98(s32 unused, s32 side);
void func_003CA418(void);
s32 func_003CA428(s32 b);
void func_003CB508(s32 unused, SelState *s);
void func_003CB8E0(s32 i, s32 v);
void func_003CBE08(void);
s32 func_003CBE18(s32 port);
s32 func_003CBE88(SelState *s);
s32 func_003CBEA8(void);
void func_003CBEB8(void);
s32 func_003CBEC8(void);
s32 func_003CBED8(s32 port);
s32 func_003CBFA0(s32 i);
void func_003CBFB8(s32 kind, s32 x, s32 y, s32 sz);
void func_003CC078(s32 x, s32 y, s32 i);
void func_003CC0E0(void);
void func_003CC190(SelState *s);
void func_003CC260(SelState *s, s32 side, s32 member, s32 chr, s32 cos);
void func_003CC930(s32 side, s32 x, s32 y);
void func_003CCCB8(SelState *s, s32 show);
void func_003CCEA8(s32 x, s32 y, s32 i);
void func_003CCF40(s32 x0, s32 x1, s32 y, s32 i);
void func_003CCFF8(void);
void func_003CD030(s32 i, s32 chr0, s32 cos0, s32 chr1, s32 cos1, s16 h, s32 w);
s32 func_003CD0D0(void);
s32 func_003CD0E0(RecSum *out);
s32 func_003CD1E8(s32 t, s32 c0, s32 c1);
void func_003CD288(void);

/* NON_MATCHING: 42 lines, register allocation (the six array bases and loop counters) */
#if 0
void func_003C3150(CharSlots *p) {
    s32 i, j, s, t, u, v;

    for (i = 0; i < 4; i++) {
        p->e[i] = p->b[i];
        p->f[i] = p->c[i];
        p->g[i] = p->d[i];
    }
    for (s = 0; s < 2; s++) {
        if (func_003ADDD8(s) != 0 && D_00440940[s][0].unk8 == 1 && D_00440940[s][1].unk8 == 1) {
            t = p->c[s * 2];
            u = p->d[s * 2];
            p->c[s * 2] = p->c[s * 2 + 1];
            p->d[s * 2] = p->d[s * 2 + 1];
            p->c[s * 2 + 1] = t;
            p->d[s * 2 + 1] = u;
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = i + 1; j < 4; j++) {
            if (p->e[j] < p->e[i]) {
                t = p->e[i];
                u = p->f[i];
                v = p->g[i];
                p->e[i] = p->e[j];
                p->f[i] = p->f[j];
                p->g[i] = p->g[j];
                p->e[j] = t;
                p->f[j] = u;
                p->g[j] = v;
            } else if (p->e[i] == p->e[j]) {
                p->e[j] = 0x7B;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C3150);
#endif

void func_003C3370(s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 bw, s32 bh) {
    s32 c = 0x80;
    s32 xm;
    s32 ym;
    s32 hr = h - bh;
    s32 hm = hr - bh;
    s32 wr = w - bw;
    s32 wm = wr - bw;
    s32 xr;
    s32 yb;

    xm = x + bw;
    ym = y + bh;
    func_003CE418(tex, clut, xm, ym, wm, hm, bw, bh, wm, hm, c, c, c, c);
    func_003CE418(tex, clut, xm, y, wm, bh, bw, 0, wm, bh, 0, 0, c, c);
    xr = x + w - bw;
    func_003CE418(tex, clut, xr, y, bw, bh, wr, 0, bw, bh, 0, 0, c, 0);
    func_003CE418(tex, clut, xr, ym, bw, hm, wr, bh, bw, hm, c, 0, c, 0);
    yb = y + h - bh;
    func_003CE418(tex, clut, xr, yb, bw, bh, wr, hr, bw, bh, c, 0, 0, 0);
    func_003CE418(tex, clut, xm, yb, wm, bh, bw, hr, wm, bh, c, c, 0, 0);
    func_003CE418(tex, clut, x, yb, bw, bh, 0, hr, bw, bh, 0, c, 0, 0);
    func_003CE418(tex, clut, x, ym, bw, hm, 0, bh, bw, hm, 0, c, 0, c);
    func_003CE418(tex, clut, x, y, bw, bh, 0, 0, bw, bh, 0, 0, 0, c);
}

void func_003C3648(s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 bw, s32 bh) {
    s32 c = 0x80;
    s32 xb;
    s32 wr;
    s32 wm;
    s32 xc;
    s32 ym;
    s32 nw;
    s32 hr;
    s32 hm;
    s32 nb;
    s32 yb;

    wr = w - bw;
    xb = x + bw;
    wm = wr - bw;
    hr = h - bh;
    hm = hr - bh;
    xc = xb + wm;
    ym = y + bh;
    nw = bw - wr;
    nb = -bw;
    func_003CE418(tex, clut, xc, ym, nw, hm, bw, bh, wm, hm, c, c, c, c);
    x += w;
    func_003CE418(tex, clut, xc, y, nw, bh, bw, 0, wm, bh, 0, 0, c, c);
    func_003CE418(tex, clut, x, y, nb, bh, 0, 0, bw, bh, 0, 0, 0, c);
    func_003CE418(tex, clut, x, ym, nb, hm, 0, bh, bw, hm, 0, c, 0, c);
    yb = y + h - bh;
    func_003CE418(tex, clut, x, yb, nb, bh, 0, hr, bw, bh, 0, c, 0, 0);
    func_003CE418(tex, clut, xc, yb, nw, bh, bw, hr, wm, bh, c, c, 0, 0);
    func_003CE418(tex, clut, xb, yb, nb, bh, wr, hr, bw, bh, c, 0, 0, 0);
    func_003CE418(tex, clut, xb, ym, nb, hm, wr, bh, bw, hm, c, 0, c, 0);
    func_003CE418(tex, clut, xb, y, nb, bh, wr, 0, bw, bh, 0, 0, c, 0);
}

/* NON_MATCHING: 75 lines, the slot positions fold differently ((dy + 216) - n * 3 + off becomes dy - (n * 3 - 216)) and delay-slot fills */
#if 0
void func_003C3948(CharSlots *p, s32 n) {
    s32 off[2];
    s32 tag;
    s32 dx;
    s32 dy;
    s32 x;
    s32 y;

    if (D_0043B7F8.unk0 == 2) {
        tag = 1;
        off[0] = off[1] = -90;
    } else {
        off[0] = 0;
        tag = 0;
        off[1] = 0;
    }
    dx = 0;
    dy = 0;
    if (p->b[0] != 0x7B) {
        if (p->b[0] == 0x32) {
            if (!tag) {
                func_003CD7E8(1, p->c[0]);
                func_003CD7E8(1, p->d[0]);
                Sprite_TexFlush(1);
                Sprite_SetBlend(1, 0);
                func_003C3370(p->c[0], p->d[0], n * 38 - 0x169, n * 3 + 20, 0x124, 0x187, 20, 20);
                func_003CD7E8(1, 0x193);
                func_003CD7E8(1, 0x194);
                Sprite_TexFlush(1);
                Sprite_SetBlend(1, 2);
                Sprite_DrawScaledBlended(1, 0x193, 0x194, n * 38 - 0x169, n * 3 + 20, 0x124, 0x187);
            }
        } else {
            func_003CD7E8(1, p->c[0]);
            func_003CD7E8(1, p->d[0]);
            Sprite_TexFlush(1);
            Sprite_SetBlend(1, 0);
            y = off[0] / 4 + 20;
            func_003C3370(p->c[0], p->d[0], n * 38 - 0x101, n * 3 + y, 0xBC, 0x100, 20, 20);
        
        }
    } else {
        off[0] = 0;
        dx = 10;
        dy = -60;
    }
    if (p->b[1] != 0x7B) {
        func_003CD7E8(1, p->c[1]);
        func_003CD7E8(1, p->d[1]);
        Sprite_TexFlush(1);
        Sprite_SetBlend(1, 0);
        x = dx - 0x168;
        y = dy + 0xD8;
        func_003C3370(p->c[1], p->d[1], n * 38 + x, y - n * 3 + off[0], 0xBC, 0x100, 20, 20);
    }
    dx = 0;
    dy = 0;
    if (p->b[2] != 0x7B) {
        if (p->b[2] == 0x32) {
            if (!tag) {
                func_003CD7E8(1, p->c[2]);
                func_003CD7E8(1, p->d[2]);
                Sprite_TexFlush(1);
                Sprite_SetBlend(1, 0);
                func_003C3648(p->c[2], p->d[2], 0x2C4 - n * 38, n * 3 + 20, 0x124, 0x187, 20, 20);
                D_005F6738 = 0x80808080;
                func_003CD7E8(1, 0x193);
                func_003CD7E8(1, 0x194);
                Sprite_TexFlush(1);
                Sprite_SetBlend(1, 2);
                Sprite_DrawScaledBlended(1, 0x193, 0x194, 1000 - n * 38, n * 3 + 20, -0x124, 0x187);
            }
        } else {
            func_003CD7E8(1, p->c[2]);
            func_003CD7E8(1, p->d[2]);
            Sprite_TexFlush(1);
            Sprite_SetBlend(1, 0);
            func_003C3648(p->c[2], p->d[2], 0x2C4 - n * 38, n * 3 + (off[1] / 4 + 20), 0xBC, 0x100, 20, 20);
        
        }
    } else {
        off[1] = 0;
        dx = 10;
        dy = -60;
    }
    if (p->b[3] != 0x7B) {
        Sprite_SetBlend(1, 0);
        func_003CD7E8(1, p->c[3]);
        func_003CD7E8(1, p->d[3]);
        Sprite_TexFlush(1);
        func_003C3648(p->c[3], p->d[3], 0x32A - dx - n * 38, dy + 0xD8 - n * 3 + off[1], 0xBC, 0x100, 20, 20);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C3948);
#endif

/* NON_MATCHING: 62 lines, the constant 40 is hoisted into a saved register here (the original reloads it per iteration) */
#if 0
void func_003C3DC0(void) {
    s32 side;
    s32 i;
    s32 x;
    s32 y;
    GameSetup *g = &D_0043B7F8;

    for (side = 0; side < 2; side++) {
        for (i = 0; i < D_0043B7F8.u.t.team[side].n; i++) {
            if (side == 0) {
                x = 150 - i * 40;
                y = 0x15E;
                if (i >= 4) {
                    x += 160;
                    y = 0x186;
                }
            } else {
                y = 0x15E;
                if (i < 4) {
                    x = i * 40 + 0x1C2;
                } else {
                    x = i * 40 + 0x122;
                    y = 0x186;
                }
            }
            D_005F6738 = 0x80808080;
            func_003CC078(x, y, 1);
            if (i >= g->u.t.team[side].unk1A && (u8)g->u.t.team[side].unk10[i] == 0) {
                func_003CBFB8(0x37, x, y, 1);
            } else {
                if ((u8)g->u.t.team[side].unk10[i] == 2) {
                    D_005F6738 = 0x80303030;
                } else {
                    D_005F6738 = 0x80808080;
                }
                func_003CBFB8(func_003CF230(g->u.t.team[side].id[i], g->u.t.team[side].val[i]), x, y,
                              1);
            }
        }
    }
    D_005F6738 = 0x80808080;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C3DC0);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F6C0);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F6D0);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F6E0);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F6F0);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F700);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F710);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F720);
extern s32 D_005F6E64;
extern u16 D_005F5CB0[2];
extern u16 D_005F5CB2;
extern s32 D_00442E4C[];
extern const char D_0051F6C0[];
extern const char D_0051F6D0[];
extern const char D_0051F6E0[];
extern const char D_0051F6F0[];
extern const char D_0051F700[];
extern const char D_0051F710[];
extern const char D_0051F720[];
void func_003C3948(CharSlots *p, s32 n);

void func_003C3FB8(Unk3C44B0 *p, s32 st, s32 n) {
    s32 stage;
    s32 w;

    if (n >= 11) {
        n = 10;
    }
    switch (st) {
    case 0:
    case 1:
        return;
    case 2:
    case 3:
        break;
    default:
        return;
    }
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    D_005F6738 = 0x80808080;
    if (D_005F6E64 == 0) {
        func_003CD7E8(1, 0xF5);
        func_003CD7E8(1, 0xF6);
        Sprite_TexFlush(1);
        Sprite_Draw(1, 0xF5, 0xF6, 0, 0);
        func_003C3948((CharSlots *)p, n);
        if (st != 2) {
            Sprite_SetBlend(1, 0);
            D_005F6738 = 0x80808080;
            if (D_0043B7F8.unk0 == 2) {
                func_003C3DC0();
            }
            if (D_005F5CB0[0] != 0 && *(volatile u16 *)&D_005F5CB2 != 0) /* fake */ {
                func_003CD7E8(1, 0xF7);
                func_003CD7E8(1, 0xF8);
                Sprite_TexFlush(1);
                Sprite_DrawBlended(1, 0xF7, 0xF8, 320 - D_00442E4C[0] / 2, 0x154);
            } else {
                stage = D_0043B7F8.unk24 + 1;
                switch (D_0043B7F8.unk0) {
                case 0:
                case 3:
                case 5:
                    if (stage != 8) {
                        Text_Printf(D_0051F6C0, 0xF8, 0x15E, 10, 7, 0xE);
                        w = Text_Printf(D_0051F6D0, 10, 10, stage);
                        Text_Printf(D_0051F6E0, 320 - w / 2, 0x181, 10, 10, 0xE, stage);
                    } else {
                        Text_Printf(D_0051F6F0, 0xEB, 0x15E, 10, 8, 0xE);
                        Text_Printf(D_0051F6C0, 0xF0, 0x181, 10, 8, 0xE);
                    }
                    break;
                case 2:
                case 4:
                    Text_Printf(D_0051F700, 0xFB, 0x15E, 10, 7, 0xE);
                    w = Text_Printf(D_0051F6D0, 10, 10, stage);
                    Text_Printf(D_0051F6E0, 320 - w / 2, 0x181, 10, 10, 0xE, stage);
                    break;
                case 7:
                    w = Text_Printf(D_0051F710, 10, 7);
                    Text_Printf(D_0051F720, 320 - w / 2, 0x181, 10, 7, 0xE);
                    break;
                }
            }
        }
    } else {
        func_003CD7E8(1, 0xF9);
        func_003CD7E8(1, 0xFA);
        Sprite_TexFlush(1);
        Sprite_Draw(1, 0xF9, 0xFA, 0, 0);
    }
    Sprite_SetFilter(1, 1);
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F760);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F764);

/* NON_MATCHING: 56 lines, the team base and 0x7B are kept in t3 / t4 and spilled with sq around the call in the original; register allocation */
#if 0
void func_003C42D8(CharSlots *p) {
    TeamMember *m;
    s32 i, j;

    for (i = 0; i < 4; i++) {
        m = &D_00440940[i >> 1][i & 1];
        if (m->unk8 == 0) {
            if (i & 1) {
                j = i & 2;
                p->b[i] = p->b[j];
                p->c[i] = p->c[j];
                p->d[i] = p->d[j];
                if (p->a[j] == 0x21) {
                    p->b[j] = 0x32;
                    p->c[j] = 0x191;
                    p->d[j] = 0x192;
                } else {
                    p->b[j] = 0x7B;
                }
            } else {
                p->b[i & 2] = 0x7B;
            }
        } else {
            p->a[i] = func_003CF230(m->unk0, m->unk4);
            p->b[i] = D_0051F760[p->a[i]].unk8;
            p->c[i] = D_0051F760[p->a[i]].unk0;
            p->d[i] = D_0051F760[p->a[i]].unk4;
        }
    }
    func_003C3150(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C42D8);
#endif

void func_003C44B0(Unk3C44B0 *p) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (p->kind[i] != 0x7B) {
            func_003CEA80(p->kind[i], p->dest[i + 1], p->size[i + 1]);
            func_003CEB68(p->tex[i]);
            func_003CEC98(p->clut[i]);
            if (p->kind[i] == 0x32) {
                func_003CEB68(0x193);
                func_003CEC98(0x194);
            }
        }
    }
}

extern Unk3C44B0 D_01FAC908;
extern s32 D_005F6E60;
extern s32 D_005F6E64;
extern u16 D_005F5CB0[2];
void func_003BFFB0(void);
void func_003C3FB8(Unk3C44B0 *p, s32 st, s32 n);

s32 func_003C45B0(void) {
    Unk3C44B0 *p = &D_01FAC908;
    s32 done = 0;
    s32 st = 0;
    s32 i;
    s32 n;
    u8 *a;

    switch (p->unk0) {
    case 0:
        func_003BFFB0();
        Bgm_Stop();
    case 1:
        st = 1;
        p->unk0++;
        break;
    case 2:
        func_003CF608(0);
        func_003637D0();
        p->dest[0] = func_003639E0();
        if (D_005F6E64 == 0) {
            func_003C42D8((CharSlots *)p);
            a = (u8 *)p->dest[0];
            p->size[0] = func_0035D1C8(3);
            a += p->size[0];
            for (i = 0; i < 4; i++) {
                if (p->kind[i] != 0x7B) {
                    p->dest[i + 1] = (u64 *)a;
                    n = func_0035D1C8(p->kind[i]);
                    a += n;
                    p->size[i + 1] = n;
                }
            }
            func_003CEA80(3, p->dest[0], p->size[0]);
            func_003CEB68(0xF5);
            func_003CEC98(0xF6);
            if (D_005F5CB0[0] != 0 && D_005F5CB0[1] != 0) {
                func_003CEB68(0xF7);
                func_003CEC98(0xF8);
            }
            func_003C44B0(p);
        } else {
            p->size[0] = func_0035D1C8(4);
            func_003CEA80(4, p->dest[0], p->size[0]);
            func_003CEB68(0xF9);
            func_003CEC98(0xFA);
        }
        p->unk8 = 0;
        p->unk0 = 3;
        st = 1;
        break;
    case 3:
        st = 2;
        func_003639E0();
        if (++p->unk8 >= 10) {
            p->unk0 = 4;
            st = 3;
        }
        break;
    case 4:
        st = 3;
        func_003639E0();
        D_005F6E60 = 0;
        p->unk0 = 5;
        break;
    case 5:
        st = 0;
        func_003992C8(1);
        if (++D_005F6E60 >= 4) {
            p->unk0 = 6;
        }
        break;
    case 6:
        st = 0;
        func_003638F0();
        done = 1;
        break;
    }
    func_003C3FB8(p, st, p->unk8);
    return done;
}

void func_003C4828(void) {
    func_003CF608(0);
    func_003637D0();
    Sprite_LoadBank(1, func_003639E0(), 0x249F0);
    func_003638F0();
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_0051F960);

/* NON_MATCHING: 13 lines, one u8 mask instead of two and the colour word scheduled after the call arguments */
#if 0
void func_003C4868(void) {
    s32 d, lim;
    s32 v;

    Sprite_SetFilter(1, 0);
    Sprite_SetBlend(1, 1);
    switch (gMode) {
    case 5:
    case 9:
    case 13:
    case 15:
    case 16:
        d = -4;
        lim = 0x40;
        break;
    default:
        d = 4;
        lim = 0x80;
        break;
    }
    v = (u8)func_003BB8B8(d, lim);
    D_005F6738 = (u64)(u8)v | (u64)(u8)v << 8 | (u64)(u8)v << 16 | (u64)0x80 << 24;
    Sprite_Draw(1, 0xC0, 0xC1, 0, 0);
    Sprite_SetFilter(1, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C4868);
#endif

void func_003C4930(void) {
    Sprite_SetFilter(1, 0);
    Sprite_SetBlend(1, 1);
    D_005F6738 = 0x80808080;
    Sprite_Draw(1, 0xBA, 0xBB, 0x101, 0x15E);
    Sprite_SetFilter(1, 1);
}

void func_003C4990(s32 r, s32 g, s32 b, s32 a) {
    Sprite_SetFilter(1, 0);
    Sprite_SetBlend(1, 1);
    D_005F6738 = (u64)r | (u64)g << 8 | (u64)b << 16 | (u64)a << 24;
    Sprite_Draw(1, 0xBC, 0xBD, 0xD0, 0xC8);
    Sprite_SetFilter(1, 1);
}

void func_003C4A28(s32 k) {
    Sprite_SetFilter(1, 0);
    Sprite_SetBlend(1, 0);
    switch (k) {
    case 0:
        break;
    case 1:
        k = 0x20;
        break;
    case 2:
        k = 0x80;
        break;
    }
    D_005F6738 = (u64)k | (u64)k << 8 | (u64)k << 16 | (u64)0x80 << 24;
    Sprite_DrawScaledBlended(1, 0xC2, 0xC3, 0x72, 0x11D, 0x19C, 0x19);
    Sprite_SetFilter(1, 1);
}

void func_003C4AE0(void) {
    D_005F6E68 = 0;
    D_005F6E6C = -1;
    D_005F6E70 = 0;
    Bgm_Play(2);
}

s32 func_003C4AF8(s32 t) {
    s32 a = t * 128 / 60;
    s32 save = D_005F6730;
    s32 r;

    D_005F6730 = 0xFFFF0;
    D_005F6740 = 0x44;
    D_005F6738 = (u64)a << 24;
    func_003CE5D8(0, 0, 0x280, 0, 0, 0x1C0, 0x280, 0x1C0, 1);
    D_005F6730 = save;
    r = 0;
    if (t == 60) r = 1;
    return r;
}

void func_003C4BA0(u32 t, u32 *out) {
    if (t > 0x57E3F) t = 0x57E3F;
    *out = t * 100 / 60;
}

void func_003C4BD8(s32 t, u32 *out) {
    *out = (u32)(t * 100) / 60;
}

void func_003C4C00(u32 t, u32 *min, u32 *sec) {
    *sec = t / 100;
    *min = *sec / 60;
    *sec = *sec % 60;
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F990);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F9A8);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F9C8);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051F9E8);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FA08);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FA20);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FA40);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FA58);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FA70);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FA88);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FAA0);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FAB8);

extern u8 *D_00415840[];
extern const char D_0051F990[];
extern const char D_0051F9A8[];
extern const char D_0051F9C8[];
extern const char D_0051F9E8[];
extern const char D_0051FA08[];
extern const char D_0051FA20[];
extern const char D_0051FA40[];
extern const char D_0051FA58[];
extern const char D_0051FAA0[];
extern const char D_0051FAB8[];
/* The per-stage records of the time attack / 1 on 1 runs (D_0043B7F8 + 0x48, 6 bytes each) [V offsets; I names]. */
typedef struct {
    /* 0x0 */ u8 c0;
    /* 0x1 */ u8 k0;
    /* 0x2 */ u8 c1;
    /* 0x3 */ u8 k1;
    /* 0x4 */ u8 pad4[2];
} StageRec;
typedef struct {
    /* 0x00 */ u8 pad0[0x48];
    /* 0x48 */ StageRec st[8];
} GameStages;
s32 func_003B6BD8(void);

s32 Results_DrawStageTimes(void);

/* NON_MATCHING: 52 lines, the constant 100 of the stage-time loop is hoisted into s1 here, which spills i + 1 to the stack (the original reloads 100 per use) */
#if 0
s32 Results_DrawStageTimes(void) {
    s32 t[8];
    s32 total;
    u32 min;
    u32 sec;
    s32 ret;
    s32 sum;
    s32 i;
    s32 x;
    s32 y;
    s32 a;
    s32 b;
    s32 tx;
    s32 w;
    s32 c;
    StageRec *e;

    sum = 0;
    D_005F6730 = 0;
    func_003CD728();
    ret = 0;
    if (D_0043B7F8.unk0 == 5) {
        w = Text_Printf(D_0051F990, 10, 5);
        Text_Printf(D_0051F9A8, 320 - w / 2, 0x28, 10, 5, 0xE);
        func_003C9358(2);
    } else {
        w = Text_Printf(D_0051F9C8, 10, 5);
        Text_Printf(D_0051F9E8, 320 - w / 2, 0x28, 10, 5, 0xE);
        func_003C9358(0);
    }
    for (i = 0; i < 8; i++) {
        func_003C4BA0(D_00440E20[i].unk0, (u32 *)&t[i]);
        sum += t[i];
    }
    x = 0x14;
    y = 0x32;
    func_003C4BD8(D_0043B7F8.unk34, (u32 *)&total);
    t[7] += total - sum;
    D_005F6E68++;
    if (D_0043B7F8.unk0 == 5) {
        x = -10;
    }
    for (i = 0; i < 8; i++) {
        e = &((GameStages *)&D_0043B7F8)->st[i];
        if (D_005F6E68 < (i + 1) * 30) {
            return 0;
        }
        if ((i + 1) * 30 == D_005F6E68) {
            Snd_PlaySe(4, 0);
        }
        func_003C4C00(t[i], &min, &sec);
        a = e->c0 * 8 + e->k0;
        b = e->c1 * 8 + e->k1;
        if (D_0043B7F8.unk0 == 5) {
            func_003DBE98(x + 0x32, y - 6, 0x118, 0x3E, 0xFFFF, 0xFFFFFF80, 0xFFFFFF80, 0xFFFFFF00, 0xFFFFFF00);
            Sprite_SetFilter(1, 1);
            Sprite_SetBlend(1, 1);
            D_005F6738 = 0x80808080;
            D_005F6730 = 0xFFFF0;
            func_003CC078(x + 0x37, y, 0);
            func_003CBFB8(D_00415840[a][4], x + 0x37, y, 0);
        } else {
            func_003DBE98(x - 5, y - 6, 0x118, 0x3E, 0xFFFF, 0xFFFFFF80, 0xFFFFFF80, 0xFFFFFF00, 0xFFFFFF00);
            Sprite_SetFilter(1, 1);
            Sprite_SetBlend(1, 1);
            D_005F6738 = 0x80808080;
            D_005F6730 = 0xFFFF0;
            func_003CC078(x, y, 0);
            func_003CBFB8(D_00415840[a][4], x, y, 0);
            if (i != 7) {
                func_003CC078(x + 0x3C, y, 0);
                func_003CBFB8(D_00415840[b][4], x + 0x3C, y, 0);
            }
        }
        tx = x + 0x78;
        Text_Printf(D_0051FA08, 10, 5, 0xD, tx, y + 0x14, i + 1);
        Text_Printf(D_0051FA20, 10, 5, 0x17, tx, y + 0x32, min, sec, t[i] % 100);
        y += 0x42;
        if (i == 3) {
            y = 0x46;
            x = D_0043B7F8.unk0 == 5 ? 300 : 0x14A;
        }
    }
    if (D_005F6E68 < 0x10E) {
        return 0;
    }
    if (D_005F6E6C == -1) {
        D_005F6E6C = 0;
        Snd_PlaySe(5, 0);
    }
    D_005F6E6C++;
    func_003C4C00(total, &min, &sec);
    if (func_003CD0D0() != 0) {
        c = (D_005F6E6C % 28) / 14 ? 0x18 : 2;
        w = Text_Printf(D_0051FA40, 10, 5);
        Text_Printf(D_0051FA58, 10, 5, c, 320 - w / 2, 0x168);
    }
    if (min < 100) {
        w = Text_Printf(D_0051FA70, 10, min, sec, total % 100);
        Text_Printf(D_0051FA88, 10, 2, 320 - w / 2, 0x186, min, sec, total % 100);
    } else {
        w = Text_Printf(D_0051FAA0, 10, min, sec, total % 100);
        Text_Printf(D_0051FAB8, 10, 2, 320 - w / 2, 0x186, min, sec, total % 100);
    }
    if (D_005F6E6C >= 0xE10) {
        ret = func_003C4AF8(D_005F6E6C - 0xE10);
    } else if (func_003B6BD8() != 0) {
        D_005F6E6C = 0xE10;
    }
    if (ret != 0) {
        Bgm_Stop();
    }
    return ret;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", Results_DrawStageTimes);
#endif

void func_003C52E8(void) {
    s32 h, m, s;
    s32 w;

    D_005F6738 = 0x80808080;
    Time_FramesToClock(D_0043B7F8.unk34 + D_0043B7F8.unk2C, &h, &m, &s);
    w = Text_Printf(D_0051FA70, 10, h, m, s);
    Text_Printf(D_0051FA88, 10, 10, 0x140 - w / 2, 0x12C, h, m, s);
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FAD0);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FAE8);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FB08);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FB18);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FB28);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FB40);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FB58);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FB68);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FB80);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FB98);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FBB0);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FBC8);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FBE0);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FBF8);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FC10);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FC28);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FC40);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FC58);

extern s32 D_005F5CC0;
extern s32 D_005F5CC4;
extern char *D_00440A28[];
extern const char D_0051FAD0[];
extern const char D_0051FAE8[];
extern const char D_0051FB08[];
extern const char D_0051FB18[];
extern const char D_0051FB28[];
extern const char D_0051FB40[];
extern const char D_0051FB58[];
extern const char D_0051FB68[];
extern const char D_0051FB80[];
extern const char D_0051FB98[];
extern const char D_0051FBB0[];
extern const char D_0051FBC8[];
extern const char D_0051FBE0[];
extern const char D_0051FBF8[];
extern const char D_0051FC10[];
extern const char D_0051FC28[];
extern const char D_0051FC40[];
extern const char D_0051FC58[];
void func_003BF180(GameSetup *g);
s32 func_003B6BD8(void);

s32 Results_DrawSurvival(void) {
    s32 ret = 0;
    s32 n;
    s32 w;
    s32 c;
    char *title;
    s32 cx;

    if (D_005F5CC0 >= 10000) {
        D_005F5CC0 = 9999;
    }
    D_005F6730 = 0xFFF00;
    func_003BF180(&D_0043B7F8);
    w = Text_Printf(D_0051FAD0, 10, 5);
    Text_Printf(D_0051FAE8, 10, 5, 0x13, 320 - w / 2, 0x28);
    func_003C9358(1);
    D_005F6E68++;
    if (D_005F5CC0 > 40) {
        n = D_005F6E68 * D_005F5CC0 / 600;
    } else {
        n = D_005F6E68 / 15;
    }
    if (D_005F5CC0 < n) {
        n = D_005F5CC0;
    }
    if (D_005F6E70 != n) {
        D_005F6E70 = n;
        Snd_PlaySe(3, 0);
    }
    cx = 320;
    w = Text_Printf(D_0051FB08, 10, n) / 2;
    Text_Printf(D_0051FB18, 10, 0, cx - w, 0x96, n);
    if (D_005F5CC0 != n) {
        return 0;
    }
    if (D_005F6E6C == -1) {
        D_005F6E6C = 0;
        Snd_PlaySe(5, 0);
    }
    D_005F6E6C++;
    if (D_005F5CC4 == 0) {
        c = 1;
    } else {
        c = 0;
        if (D_005F5CC4 < 4) {
            c = 2;
        }
    }
    if (D_005F5CC4 != 0) {
        title = D_00440A28[D_005F5CC4];
        w = Text_Printf(D_0051FB28, 10, 10) / 2;
        Text_Printf(D_0051FB40, 10, 10, c, cx - w, 0xFA);
        w = Text_Printf(D_0051FB58, 10, 10, title) / 2;
        Text_Printf(D_0051FB68, 10, 10, c, cx - w, (D_005F6E6C % 30) / 15 ? D_00440A28[0] : title);
        w = Text_Printf(D_0051FB80, 10, 10) / 2;
        Text_Printf(D_0051FB98, 10, 10, c, cx - w);
        w = Text_Printf(D_0051FBB0, 10, 10) / 2;
        Text_Printf(D_0051FBC8, 10, 10, c, cx - w);
    } else {
        w = Text_Printf(D_0051FBE0, 10, 10) / 2;
        Text_Printf(D_0051FBF8, 10, 10, c, cx - w, 0x122);
        w = Text_Printf(D_0051FC10, 10, 10) / 2;
        Text_Printf(D_0051FC28, 10, 10, c, cx - w);
        w = Text_Printf(D_0051FC40, 10, 10) / 2;
        Text_Printf(D_0051FC58, 10, 10, c, cx - w);
    }
    D_005F6730 = 0xFFFE0;
    Sprite_SetBlend(1, 0);
    if (D_005F6E6C >= 0xE10) {
        ret = func_003C4AF8(D_005F6E6C - 0xE10);
    } else if (func_003B6BD8() != 0) {
        D_005F6E6C = 0xE10;
    }
    return ret;
}

void func_003C5790(void) {
    func_003637D0();
    Sprite_LoadBank(7, func_003639E0(), 0x249F0);
    func_003638F0();
}

void func_003C57C8(void) {
    func_003DBE98(D_01FAC9B0.unk4, D_01FAC9B0.unk8, D_01FAC9B0.unkC, D_01FAC9B0.unk10, 0x1000, 0x20009050, 0x20009050,
                  0x20009050, 0x20009050);
    Sprite_Draw(1, D_01FAC9B0.unk0, 0x109, D_01FAC9B0.unk4, D_01FAC9B0.unk8);
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FC70);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FC80);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FC90);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FCA0);
extern const char D_0051FC70[];
extern const char D_0051FC80[];
extern const char D_0051FC90[];
extern const char D_0051FCA0[];

/* NON_MATCHING: 71 lines, the flag argument moves to a0 and v takes t1 in the original; case 0 is cross-jumped into case 3 here */
#if 0
void func_003C5848(s32 x, s32 y, u8 *p, s32 k, s32 j, s32 flag) {
    u8 *q;
    s8 v;
    u8 t;

    if (j == 0) {
        x -= 10;
    } else if (j == 1) {
        x += 12;
    } else {
        x += 35;
    }
    y += 0x16;
    if (k == 10) {
        v = ((s8 *)p)[j + 0x58];
        t = p[j + 0x5B];
    } else {
        v = ((s8 *)p)[k * 2 + j + 0xD];
        t = p[k * 2 + j + 0x27];
    }
    if (flag) {
        Sprite_DrawBlended(1, 0x11D, 0x11E, x - 5, y - 0x15);
        return;
    }
    switch (t) {
    case 0:
        if (v < 0) {
            break;
        }
        Text_Printf(D_0051FC70, x, y, 0xB, 6, v);
        break;
    case 1:
        Sprite_DrawBlended(1, 0x11C, 0x109, x - 5, y - 0x15);
        break;
    case 2:
        Sprite_DrawBlended(1, 0x11B, 0x109, x - 3, y - 0x15);
        break;
    case 3:
        Text_Printf(D_0051FC80, x, y, 0xB, 6);
        break;
    case 4:
        Text_Printf(D_0051FC90, x, y, 0xB, 6);
        break;
    case 5:
        Text_Printf(D_0051FCA0, x, y, 0xB, 1);
        break;
    case 6:
        Text_Printf(D_0051FC70, x, y, 0xB, 5, v);
        break;
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_0051FCB0);

INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C5848);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FCD0);

extern const char D_0051FCD0[];
extern char D_005F66E8[];
/* The per-row values drawn under the panel cells (+0x42: one s16 per row, -1: none; +0x56 the total) [V offsets]. */
typedef struct {
    /* 0x00 */ u8 pad0[0x42];
    /* 0x42 */ s16 v[10];
    /* 0x56 */ s16 total;
} PanelVals;
void func_003C59D8(s32 x, s32 y, s32 mode, s32 first, s32 last, s32 extra, PanelVals *p0, PanelVals *p1, s32 cur);

/* NON_MATCHING: 549 lines, same blocks but 33 instructions shorter: register allocation of the spilled arguments and counters, the k0 / k1 increments are scheduled differently */
#if 0
void func_003C59D8(s32 x, s32 y, s32 mode, s32 first, s32 last, s32 extra, PanelVals *p0, PanelVals *p1, s32 cur) {
    s32 save;
    s32 k0;
    s32 k1;
    s32 i;
    s32 bx;
    s32 by;
    s32 w;
    s32 next;

    save = D_005F6730;
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    D_005F6738 = 0x80808080;
    D_01FAC9B0.unk4 = x;
    D_005F6730 = 0xFFF00;
    D_01FAC9B0.unk0 = 0x10A;
    D_01FAC9B0.unk10 = 0x44;
    D_01FAC9B0.unkC = 4;
    D_01FAC9B0.unk8 = y;
    func_003C57C8();
    if (mode == 2) {
        D_01FAC9B0.unk0 = 0x10B;
        D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
        D_01FAC9B0.unk10 = 0x2E;
        func_003C57C8();
    }
    D_01FAC9B0.unk0 = 0x10C;
    k0 = 0;
    D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
    D_01FAC9B0.unk10 = 4;
    k1 = 0x15;
    func_003C57C8();
    D_01FAC9B0.unk8 = y;
    D_01FAC9B0.unk4 += D_01FAC9B0.unkC;
    for (i = first; i <= last; i++) {
        if (i < 10) {
            D_01FAC9B0.unk0 = 0x10D;
            D_01FAC9B0.unkC = 0x2F;
            D_01FAC9B0.unk10 = 0x44;
            func_003C57C8();
            bx = D_01FAC9B0.unk4;
            by = D_01FAC9B0.unk8;
            D_005F6730 = 0xFFFF0;
            Text_Printf(D_0051FC70, bx + 0xF, by + 0x19, 0xB, 6, i);
            func_003C5848(bx + 0xF, by + 0x19, (u8 *)p0, i, 0, k0 == cur);
            k0++;
            func_003C5848(bx + 0xF, by + 0x19, (u8 *)p0, i, 1, k0 == cur);
            k0++;
            if (p0->v[i] != -1) {
                w = Text_Printf(D_005F66E8, 0xB, p0->v[i]);
                Text_Printf(D_0051FC70, bx + 0x18 - w / 2, by + 0x45, 0xB, 6, p0->v[i]);
            }
            if (mode == 2) {
                D_005F6730 = 0xFFF00;
                D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
                D_01FAC9B0.unk0 = 0x10E;
                D_01FAC9B0.unk10 = 0x2E;
                func_003C57C8();
                by = D_01FAC9B0.unk8;
                bx = D_01FAC9B0.unk4;
                D_005F6730 = 0xFFFF0;
                func_003C5848(bx + 0xF, by + 4, (u8 *)p1, i, 0, k1 == cur);
                k1++;
                func_003C5848(bx + 0xF, by + 4, (u8 *)p1, i, 1, k1 == cur);
                k1++;
                if (p1->v[i] != -1) {
                    w = Text_Printf(D_005F66E8, 0xB, p1->v[i]);
                    Text_Printf(D_0051FC70, bx + 0x18 - w / 2, by + 0x30, 0xB, 6, p1->v[i]);
                }
            }
            next = 0x10F;
        } else {
            D_01FAC9B0.unk0 = 0x110;
            D_01FAC9B0.unkC = 0x46;
            D_01FAC9B0.unk10 = 0x44;
            D_005F6730 = 0xFFF00;
            func_003C57C8();
            bx = D_01FAC9B0.unk4;
            by = D_01FAC9B0.unk8;
            D_005F6730 = 0xFFFF0;
            Text_Printf(D_0051FCD0, bx + 0xF, by + 0x19, 0xB, 6);
            func_003C5848(bx + 0xF, by + 0x19, (u8 *)p0, i, 0, k0 == cur);
            k0++;
            func_003C5848(bx + 0xF, by + 0x19, (u8 *)p0, i, 1, k0 == cur);
            k0++;
            func_003C5848(bx + 0xF, by + 0x19, (u8 *)p0, i, 2, k0 == cur);
            if (p0->v[i] != -1) {
                w = Text_Printf(D_005F66E8, 0xB, p0->v[i]);
                Text_Printf(D_0051FC70, bx + 0x22 - w / 2, by + 0x45, 0xB, 6, p0->v[i]);
            }
            if (mode == 2) {
                D_005F6730 = 0xFFF00;
                D_01FAC9B0.unk0 = 0x111;
                D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
                D_01FAC9B0.unk10 = 0x2E;
                func_003C57C8();
                by = D_01FAC9B0.unk8;
                bx = D_01FAC9B0.unk4;
                D_005F6730 = 0xFFFF0;
                func_003C5848(bx + 0xF, by + 4, (u8 *)p1, i, 0, k1 == cur);
                k1++;
                func_003C5848(bx + 0xF, by + 4, (u8 *)p1, i, 1, k1 == cur);
                k1++;
                func_003C5848(bx + 0xF, by + 4, (u8 *)p1, i, 2, k1 == cur);
                if (p1->v[i] != -1) {
                    w = Text_Printf(D_005F66E8, 0xB, p1->v[i]);
                    Text_Printf(D_0051FC70, bx + 0x22 - w / 2, by + 0x30, 0xB, 6, p1->v[i]);
                }
            }
            next = 0x112;
        }
        D_01FAC9B0.unk0 = next;
        D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
        D_01FAC9B0.unk10 = 4;
        func_003C57C8();
        D_01FAC9B0.unk8 = y;
        D_01FAC9B0.unk4 += D_01FAC9B0.unkC;
    }
    if (extra != 0) {
        D_01FAC9B0.unk0 = 0x113;
        D_01FAC9B0.unkC = 0x36;
        D_01FAC9B0.unk10 = 0x44;
        func_003C57C8();
        bx = D_01FAC9B0.unk4;
        by = D_01FAC9B0.unk8;
        w = Text_Printf(D_005F66E8, 0xB, p0->total);
        Text_Printf(D_0051FC70, bx + 0x1B - w / 2, by + 0x38, 0xB, 6, p0->total);
        if (mode == 2) {
            D_01FAC9B0.unk0 = 0x114;
            D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
            D_01FAC9B0.unk10 = 0x2E;
            func_003C57C8();
            bx = D_01FAC9B0.unk4;
            by = D_01FAC9B0.unk8;
            w = Text_Printf(D_005F66E8, 0xB, p1->total);
            Text_Printf(D_0051FC70, bx + 0x18 - w / 2, by + 0x24, 0xB, 6, p1->total);
        }
        D_01FAC9B0.unk0 = 0x115;
        D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
        D_01FAC9B0.unk10 = 4;
        func_003C57C8();
        D_01FAC9B0.unk8 = y;
        D_01FAC9B0.unk4 += D_01FAC9B0.unkC;
    }
    D_01FAC9B0.unk0 = 0x116;
    D_01FAC9B0.unk10 = 0x44;
    D_01FAC9B0.unkC = 4;
    func_003C57C8();
    if (mode == 2) {
        D_01FAC9B0.unk0 = 0x117;
        D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
        D_01FAC9B0.unk10 = 0x2E;
        func_003C57C8();
    }
    D_01FAC9B0.unk0 = 0x118;
    D_01FAC9B0.unk8 += D_01FAC9B0.unk10;
    D_01FAC9B0.unk10 = 4;
    func_003C57C8();
    D_005F6730 = save;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C59D8);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FCE0);

void func_003C6280(void) {
    WordPair10 t = D_0051FCE0;
    s32 i;

    Sprite_DrawBlended(1, 0x119, 0x11A, 0x20C, 0x14);
    for (i = 0; i < 10; i++) {
        s32 x = t.e[i].a + 0x20C;
        s32 y = t.e[i].b + 0x14;

        if (func_003E9478(i) != 0) Sprite_DrawBlended(1, 0x129, 0x12A, x, y);
        else Sprite_DrawBlended(1, 0x127, 0x128, x, y);
    }
}

void func_003C6410(f32 a) {
    f32 v[2];
    f32 b = a;
    f32 d;

    Sprite_DrawBlended(1, 0x11F, 0x120, 0x32, 0x118);
    v[0] = 0.0f;
    v[1] = -94.0f;
    a *= 0.25f;
    func_003CF408(v, a - 0.5f);
    D_005F6748 = 0x40;
    Sprite_SetTexture(0x125, 0x126);
    d = 1.3f;
    D_00440F20.rot = b / d;
    D_00440F20.x = v[0] + 138.0f;
    D_00440F20.y = v[1] + 369.0f;
    Sprite_DrawCurrentRotated(1);
}

/* NON_MATCHING: 16 lines, n and 0xF6 - n swap s1 / s2 and the second store run is ordered differently */
#if 0
void func_003C64E8(f32 a) {
    f32 d;
    s32 n, h;

    D_005F6748 = 0x40;
    d = 100.0f;
    n = (s32)(a * 236.0f / d) + 4;
    h = 0xF6 - n;
    Sprite_SetTexture(0x121, 0x122);
    D_00440F20.x = 0x226;
    D_00440F20.w = 0x40;
    D_00440F20.h = h;
    D_00440F20.th = h;
    D_00440F20.y = 0x8C;
    func_003CDC10(1);
    Sprite_SetTexture(0x123, 0x124);
    D_00440F20.th = n;
    D_00440F20.x = 0x226;
    D_00440F20.w = 0x40;
    D_00440F20.h = n;
    D_00440F20.y = h + 0x8C;
    D_00440F20.v += h;
    func_003CDC10(1);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C64E8);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FD30);

void func_003C65D0(Unk3C65D0 *s) {
    const Unk51FD30 *e;
    s32 save;
    s32 i;

    Sprite_SetFilter(1, 1);
    save = D_005F6730;
    D_005F6730 = 0xFFF00;
    for (i = 0; i < 3; i++) {
        e = &D_0051FD30[s->unk2][i];
        if (e->a != 0) {
            Sprite_SetBlend(1, e->blend);
            D_005F6738 = (u64)e->c | (u64)e->c << 8 | (u64)e->c << 16 | (u64)e->a << 24;
            switch (i) {
            case 0:
                if (func_003E9450() != 0) func_003C6280();
                break;
            case 1:
                func_003C6410(s->unk4);
                break;
            case 2:
                func_003C64E8(s->unkC);
                break;
            }
        }
    }
    D_005F6730 = save;
}

void func_003C6738(s32 a, s32 n, s32 x, s32 y) {
    s32 u = n % 10;
    s32 t = n / 10;

    Sprite_SetFilter(a, 0);
    Sprite_SetBlend(a, 1);
    D_005F6738 = 0x80808080;
    if (D_0043B7F8.unkB == 5 && gMode != 5) {
        Sprite_DrawBlended(a, 7, 1, 0x11D, y);
    } else {
        func_003CE508(a, 0, 1, x - 0x27, y, 0x28, 0x2F, t * 0x28, 0, 0x28, 0x2F);
        func_003CE508(a, 0, 1, x, y, 0x28, 0x2F, u * 0x28, 0, 0x28, 0x2F);
    }
    Sprite_SetFilter(a, 1);
}

void func_003C6888(void) {
    D_00440A58[0] = 0;
    D_00440A58[1] = 0;
    D_00440A58[2] = 0;
    D_00440A58[3] = 0;
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FE30);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FE98);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FEC8);

void func_003C68A8(SelState *s, s32 kind, s32 i) {
    s32 v = 0;

    switch (kind) {
    case 10:
        v = D_0051FE30[i];
        break;
    case 11:
        v = D_0051FE98[i];
        break;
    }
    s->unk1C = v;
}

/* NON_MATCHING: 2 lines, the table lui is scheduled one slot early */
#if 0
void func_003C68F0(SelState *s, s32 kind) {
    /* the table's entries are 0x20 bytes (a font), not sizeof(SelState) */
    const SelState *t = (const SelState *)((const u8 *)D_0051FFA0 + 0x20);

    switch (kind) {
    case 10:
    case 11:
        t = (const SelState *)((const u8 *)D_0051FFA0 + (kind - 10) * 0x20);
        break;
    }
    s->unk0 = t->unk0;
    s->unk4 = t->unk4;
    s->unk8 = t->unk8;
    s->unkC = t->unkC;
    s->unk10 = t->unk10;
    s->unk14[0] = t->unk14[0];
    s->unk14[1] = t->unk14[1];
    s->unk1C = t->unk1C;
    func_003C68A8(s, kind, s->unk1C);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C68F0);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FF08);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FF48);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_0051FFA0);

/* A font: kind 10 (proportional, widths in D_0051FF08) or 11 (fixed), a grid of cols x rows glyphs [I]. */
typedef struct {
    /* 0x00 */ s32 kind;
    /* 0x04 */ s32 cols;
    /* 0x08 */ s32 rows;
    /* 0x0C */ s32 w;
    /* 0x10 */ s32 h;
    /* 0x14 */ s8 *map;
    /* 0x18 */ s32 tex;
    /* 0x1C */ s32 clut;
} Font;

extern const s8 D_0051FF08[];

s32 Text_PutGlyph(Font *f, s8 c, s32 scale, s32 skip) {
    s32 idx;
    s32 adv;
    s32 i;
    s8 *p;

    adv = 0;
    p = f->map;
    idx = 0;
    for (i = 0; i < f->cols * f->rows; i++, p++) {
        if (*p == c) {
            idx = i;
            break;
        }
    }
    if (skip == 0 && c != 0x20) {
        s32 col = idx % f->cols;
        s32 row = idx / f->cols;
        s32 u = col * f->w;
        s32 v = row * f->h;

        if (f->kind == 10 && c == 0x41) {
            D_00440A58[0] -= scale * 6 / 10;
        }
        if (D_005F6E7C < 100) {
            Unk1FAC9C8 *e = &D_01FAC9C8[D_005F6E7C];

            e->unk0 = f->tex;
            e->unk4 = f->clut;
            e->unk8 = D_00440A58[0];
            e->unkA = D_00440A58[1];
            e->unkC = f->w * scale / 10;
            e->unkE = f->h * scale / 10;
            e->unk10 = u;
            e->unk12 = v;
            e->unk14 = f->w;
            e->unk16 = f->h;
            D_005F6E7C++;
        }
    }
    switch (f->kind) {
    case 10:
        adv = D_0051FF08[idx] * scale / 10;
        break;
    case 11:
        adv = f->w * scale / 10;
        break;
    }
    D_00440A58[0] += adv;
    return adv;
}

void func_003C6BD0(void) {
    s32 i;

    for (i = D_005F6E7C - 1; i >= 0; i--) {
        Unk1FAC9C8 *e = &D_01FAC9C8[i];

        func_003CE508(D_005F6E80, e->unk0, e->unk4, e->unk8, e->unkA, e->unkC, e->unkE, e->unk10, e->unk12, e->unk14,
                      e->unk16);
    }
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520000);
/* The hex digits "0123456789ABCDEF" copied to the stack [V]. */
typedef struct {
    char c[16];
} Digits16;
extern const Digits16 D_00520000;
extern s32 D_005F6E74;
extern s32 D_005F6E78;

/* NON_MATCHING: 257 lines, register allocation and block order of the format loop; as C the jump table is the whole
 * 0x49-entry table jtbl_00520010 (one symbol since T19 b2: `dont_allow_addend` keeps two .data words 0x005200DA from
 * splitting it; the C table needs RODATA_ALIGN16() after it); va_start
 * is spelled with __builtin_next_arg [I] */
#if 0
#ifdef PORT
/* i386 cdecl: the variadic arguments are 4-byte stack slots right after fmt. */
#define TP_ARG() (ap += 4, *(s32 *)(ap - 4))
#define TP_START(fmt) ((char *)__builtin_next_arg(fmt))
#else
#define TP_ARG() (ap += 8, *(s32 *)(ap - 8))
#define TP_START(fmt) ((char *)__builtin_next_arg(fmt) - 7 * 8) /* fake: the EABI register save area */
#endif

s32 Text_Printf(const char *fmt, ...) {
    Digits16 digits;
    Font font;
    char *ap = TP_START(fmt);
    s32 state;
    s32 zero;
    s32 width;
    s32 neg;
    s32 blank;
    s32 save;
    s32 measure;
    s32 total;
    s32 v;
    s32 n;
    s32 k;
    s8 c;
    const char *s;

    save = D_005F6730;
    D_005F6730 = 0xFFFF0;
    zero = 0;
    width = 0;
    state = 0;
    measure = 0;
    D_005F6738 = 0x80808080;
    D_005F6E74 = 0xB;
    D_005F6E78 = 10;
    digits = D_00520000;
    total = 0;
    func_003C68F0((SelState *)&font, 0xB);
    D_005F6E7C = 0;
    D_005F6E80 = 1;
    while ((c = *fmt++) != 0) {
        if (c == '\n') {
            D_00440A58[0] = D_00440A58[2];
            D_00440A58[1] = D_00440A58[3] = D_00440A58[3] + font.h * D_005F6E78 / 10;
            continue;
        }
        if (c == '%') {
            if (state != 1) {
                zero = 0;
                state = 1;
                width = 0;
                continue;
            }
            state = 0;
        }
        if (state != 1) {
            total += Text_PutGlyph(&font, c, D_005F6E78, measure);
            continue;
        }
        state = 0;
        blank = 0;
        switch (c) {
        case '0':
            zero = 1;
            state = 1;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
            width = c - '0';
            state = 1;
            break;
        case 'D':
            neg = 0;
            v = Num_ToBcd(TP_ARG());
            goto number;
        case 'd':
            neg = 0;
            v = TP_ARG();
            if (v < 0) {
                neg = 1;
                v = -v;
            }
            v = Num_ToBcd(v);
            goto number;
        case 'b':
            blank = 1;
        case 'x':
            neg = 0;
            v = TP_ARG();
        number:
            n = func_003CF3D8(v);
            if (n == 0) {
                n = 1;
            }
            if (width == 0) {
                width = n;
            } else {
                width -= neg;
                if (width < n || zero == 1) {
                    n = width;
                }
            }
            if (n < width) {
                k = width - n;
                if (blank) {
                    k *= 4;
                }
                for (; k > 0; k--) {
                    total += Text_PutGlyph(&font, ' ', D_005F6E78, measure);
                }
            }
            if (neg == 1) {
                total += Text_PutGlyph(&font, '-', D_005F6E78, measure);
            }
            if (!blank) {
                while (n > 0) {
                    n--;
                    total += Text_PutGlyph(&font, digits.c[(v >> (n * 4)) & 0xF], D_005F6E78, measure);
                }
            }
            break;
        case 's':
            for (s = (const char *)TP_ARG(); *s != 0; s++) {
                total += Text_PutGlyph(&font, *s, D_005F6E78, measure);
            }
            break;
        case 'C':
            total += Text_PutGlyph(&font, (s8)TP_ARG(), D_005F6E78, measure);
            break;
        case 'H':
            D_00440A58[0] = D_00440A58[2] = TP_ARG();
            break;
        case 'V':
            D_00440A58[1] = D_00440A58[3] = TP_ARG() - 0x14;
            break;
        case 'h':
            D_00440A58[0] = D_00440A58[2] = TP_ARG() * font.w;
            break;
        case 'v':
            D_00440A58[1] = D_00440A58[3] = TP_ARG() * font.h - 0x14;
            break;
        case 'F':
            D_005F6E74 = TP_ARG();
            func_003C68F0((SelState *)&font, D_005F6E74);
            break;
        case 'S':
            D_005F6E78 = TP_ARG();
            break;
        case 'c':
            func_003C68A8((SelState *)&font, D_005F6E74, TP_ARG());
            break;
        case 'f':
        case 'p':
            TP_ARG();
            break;
        case 'T':
            measure = 1;
            break;
        case 'Z':
            D_005F6730 = TP_ARG();
            break;
        case 'P':
            D_005F6730 = func_003C0098() << 4;
            D_005F6E80 = 0;
            break;
        default:
            return 0;
        }
    }
    Sprite_SetFilter(D_005F6E80, 1);
    Sprite_SetBlend(D_005F6E80, 5);
    ((void (*)(s32))func_003C6BD0)(D_005F6E80); /* the caller passes the blend flag */
    Sprite_SetFilter(D_005F6E80, 1);
    Sprite_SetBlend(D_005F6E80, 1);
    D_005F6730 = save;
    return total;
}
#else
INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_00520010);

INCLUDE_ASM("asm/nonmatchings/select_results_b", Text_Printf);
#endif

s32 func_003C71F0(s32 i) {
    return D_0051FE98[i];
}

void func_003C7208(Records *r, u8 *name) {
    if (r->unk112 & 1) Str_Copy(r->lastScore->name, name);
    if (r->unk112 & 2) Str_Copy(r->lastTime->name, name);
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520140);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520210);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520220);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520230);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520240);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520250);

void Records_DrawRank(s32 rank, s32 y, s32 c) {
    s32 r;

    rank++;
    r = rank;
    y -= 0x1B;
    Text_Printf(D_00520210, 0x10, y, 10, c, r);
    switch (r) {
    case 1:
    case 21:
    case 31:
        Text_Printf(D_00520220, y + 0x14, 10, 5, c);
        break;
    case 2:
    case 22:
    case 32:
        Text_Printf(D_00520230, y + 0x14, 10, 5, c);
        break;
    case 3:
    case 23:
    case 33:
        Text_Printf(D_00520240, y + 0x14, 10, 5, c);
        break;
    default:
        Text_Printf(D_00520250, y + 0x14, 10, 5, c);
        break;
    }
}

void func_003C73A0(s32 t, s32 x, s32 y, s32 c, s32 w) {
    s32 h, m, s;

    Time_FramesToClock(t, &h, &m, &s);
    Text_Printf(D_005202E8, x, y, 10, w, c, h, m, s);
}

void func_003C7428(s32 v, s32 y, s32 c) {
    s32 y1;

    y -= 7;
    y1 = y - 0x13;
    if (v >= 100) Text_Printf(D_00520308, 0x113, y, 10, 5, 11, y1, 10, v);
    else Text_Printf(D_00520320, 0x113, y, 10, 5, c, y1, 10, v);
}

void func_003C74A8(s32 v, s32 y) {
    y -= 0x18;
    if (v == 1000) Text_Printf(D_00520338, 0x1D6, y, 10, 14, 5, y + 0x14);
    else Text_Printf(D_00520350, 0x1D6, y, 10, 14, v / 10, 5, y + 0x14, v % 10);
}


void func_003C7530(s32 i, s32 v, s32 x, s32 y) {
    s16 w;

    if (v > 1000) v = 1000;
    w = v * 0xD4 / 1000;
    if (w != 0) func_003CFE38(x + 0x1B8, y - 0x26, w, 0x24, (u8 *)&D_00520140[i * 6]);
}

RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_005202E8);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520308);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520320);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520338);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520350);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520368);

void func_003C7598(s32 x, s32 y, s32 c) {
    s32 t = D_00440E6E[0];

    if (t != 0) t = (t + 59) / 60;
    Text_Printf(D_00520368, x, y, 10, 5, c, t);
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520378);

/* The name-entry state (D_01FAD328): +4 the number of letters entered [I]. */
typedef struct {
    /* 0x0 */ u8 pad0[4];
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u8 pad6[0xA];
} Unk1FAD328;
extern Unk1FAD328 D_01FAD328;
extern char D_005F66F0[];
extern const char D_00520378[];
/* The records block seen with the highlight pulse at +0x1C4 [V offsets]. */
typedef struct {
    /* 0x000 */ u8 pad0[0x1C4];
    /* 0x1C4 */ s32 unk1C4;
    /* 0x1C8 */ s32 unk1C8;
} RecordsPulse;
/* A gradient bar on the stack [I]. */
typedef struct {
    /* 0x00 */ s16 x;
    /* 0x02 */ s16 y;
    /* 0x04 */ s16 w;
    /* 0x06 */ s16 h;
    /* 0x08 */ u8 pad8[8];
    /* 0x10 */ u8 r;
    /* 0x11 */ u8 g;
    /* 0x12 */ u8 b;
} Bar;

/* NON_MATCHING: 38 lines, the counter / width registers (s3 / s4) and the y - 8 / y - 0x4A registers are swapped, bar stores and colour ORs scheduled differently */
#if 0
void func_003C75E8(s32 i, s32 idx, ScoreEnt *e, s32 a, s32 y, s32 b) {
    RecordsPulse *rp;
    Bar bar;
    s32 c0;
    s32 c1;
    s32 save;
    s32 k;
    s32 w;
    s32 p;

    func_003DBAA8(0, 0x27F, 0x14, 0x1BE);
    if ((u32)(y - 0x3D) < 0x1E7) {
        c0 = e->c0;
        c1 = e->c1;
        if (b == 1 && D_01FAD328.unk4 < 3) {
            save = D_005F6730;
            D_005F6730 = 0xFFF00;
            Sprite_SetFilter(1, 1);
            k = 0;
            Sprite_SetBlend(1, 0);
            w = 0;
            D_005F6738 = 0x40808080;
            D_005F6740 = 0x44;
            func_003CE5D8(0, y - 0x5D, 0x280, y - 0x5D, 0, y, 0x280, y, b);
            D_005F6730 = save;
            for (k = 0; k < D_01FAD328.unk4; k++) {
                w += Text_Printf(D_005F66F0, 10, (s8)e->name[k]);
            }
            rp = (RecordsPulse *)&D_00440D50;
            p = rp->unk1C4;
            bar.x = a + w + 0x1E8;
            bar.w = 0x30;
            bar.h = 4;
            bar.r = 0xFF;
            bar.b = p * 2 + 1;
            bar.y = y - 8;
            bar.g = p * 2 + 1;
            if ((u32)(p - 5) >= 0x6C) {
                rp->unk1C8 = ~rp->unk1C8;
            }
            rp->unk1C4 = p + rp->unk1C8 * 5;
            D_005F6738 = (u64)bar.r | 0x80000000 | (((u64)bar.b << 16) | ((u64)bar.g << 8));
            func_003CE5D8(bar.x, bar.y, bar.x + bar.w, bar.y, bar.x, bar.y + bar.h, bar.x + bar.w, bar.y + bar.h, 1);
            D_005F6738 = 0x80808080;
            Records_DrawRank(i, y, 0xB);
            func_003C73A0(e->score, a + 0x12C, y - 8, 0xB, 5);
            Text_Printf(D_00520378, a + 0x1E8, y - 0x1C, 10, 0xB, e->name);
            func_003C7598(a + 0x1E0, y - 0x36, 0);
        } else {
            Records_DrawRank(i, y, 0xD);
            func_003C73A0(e->score, a + 0x12C, y - 8, 10, 5);
            Text_Printf(D_00520378, a + 0x1E8, y - 0x1C, 10, 0xD, e->name);
        }
        Sprite_SetFilter(1, 1);
        Sprite_SetBlend(1, 1);
        D_005F6738 = 0x80808080;
        func_003CC078(a + 0xD2, y - 0x39, 0);
        func_003CBFB8(c1, a + 0xD2, y - 0x39, 0);
        func_003CC078(a + 0xA0, y - 0x4A, 0);
        func_003CBFB8(c0, a + 0xA0, y - 0x4A, 0);
        func_003CF1E8(c0, a + 0xDC, y - 0x4E);
        func_003CF1E8(c1, a + 0x10E, y - 0x3C);
        func_003C9458(y);
    }
    func_003DBAA8(0, 0x27F, 0, 0x1BF);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C75E8);
#endif

/* NON_MATCHING: 12 lines, bar.x is a + w + 0x1E8 here (original a + (w + 0x1E8), which as C changes the hoisted coordinates) and s0 / s3 are swapped */
#if 0
void func_003C7AB0(s32 i, TimeEnt *e, s32 a, s32 y, s32 b) {
    RecordsPulse *rp;
    Bar bar;
    s32 c0;
    s32 c1;
    s32 t;
    s32 save;
    s32 k;
    s32 w;
    s32 p;

    func_003DBAA8(0, 0x27F, 0x14, 0x1BE);
    if ((u32)(y - 0x3D) < 0x1E7) {
        t = e->time;
        c0 = e->c0;
        c1 = e->c1;
        if (t >= 1000) {
            t = 999;
        }
        if (b == 1 && D_01FAD328.unk4 < 3) {
            save = D_005F6730;
            D_005F6730 = 0xFFF00;
            Sprite_SetFilter(1, 1);
            k = 0;
            Sprite_SetBlend(1, 0);
            w = 0;
            D_005F6738 = 0x40808080;
            D_005F6740 = 0x44;
            func_003CE5D8(0, y - 0x5D, 0x280, y - 0x5D, 0, y, 0x280, y, b);
            D_005F6730 = save;
            for (; k < D_01FAD328.unk4; k++) {
                w += Text_Printf(D_005F66F0, 10, (s8)e->name[k]);
            }
            rp = (RecordsPulse *)&D_00440D50;
            p = rp->unk1C4;
            bar.x = a + w + 0x1E8;
            bar.y = y - 8;
            bar.w = 0x30;
            bar.h = 4;
            bar.r = 0xFF;
            bar.g = p * 2 + 1;
            bar.b = p * 2 + 1;
            if ((u32)(p - 5) >= 0x6C) {
                rp->unk1C8 = ~rp->unk1C8;
            }
            rp->unk1C4 = p + rp->unk1C8 * 5;
            D_005F6738 = (u64)bar.r | ((u64)bar.g << 8) | ((u64)bar.b << 16) | 0x80000000;
            func_003CE5D8(bar.x, bar.y, bar.x + bar.w, bar.y, bar.x, bar.y + bar.h, bar.x + bar.w, bar.y + bar.h, 1);
            D_005F6738 = 0x80808080;
            Records_DrawRank(i, y, 0xB);
            func_003C7428(t, y, 0xB);
            Text_Printf(D_00520378, a + 0x1E8, y - 0x1C, 10, 0xB, e->name);
            func_003C7598(a + 0x1E0, y - 0x36, 0);
        } else {
            Records_DrawRank(i, y, 0xD);
            func_003C7428(t, y, 0xC);
            Text_Printf(D_00520378, a + 0x1E8, y - 0x1C, 10, 0xD, e->name);
        }
        Sprite_SetFilter(1, 1);
        Sprite_SetBlend(1, 1);
        D_005F6738 = 0x80808080;
        func_003CC078(a + 0xD2, y - 0x39, 0);
        func_003CBFB8(c1, a + 0xD2, y - 0x39, 0);
        func_003CC078(a + 0xA0, y - 0x4A, 0);
        func_003CBFB8(c0, a + 0xA0, y - 0x4A, 0);
        func_003CF1E8(c0, a + 0xDC, y - 0x4E);
        func_003CF1E8(c1, a + 0x10E, y - 0x3C);
        func_003C9458(y);
    }
    func_003DBAA8(0, 0x27F, 0, 0x1BF);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C7AB0);
#endif

void func_003C7F60(s32 rank, s32 k, s32 a, s32 b, s32 c, s32 x, s32 y, s32 unused) {
    s32 r, p, kk, x2, y2;

    func_003DBAA8(0, 0x27F, 0x14, 0x1BF);
    if ((u32)(y - 0x3D) < 0x1E7) {
        if (b == 0) {
            p = 0;
            r = 0;
        } else {
            r = a * c >> 6;
            p = a * c * 1000 / b >> 6;
        }
        kk = k != 0xF ? k : 0x24;
        x2 = x + 0xA0;
        Sprite_SetFilter(1, 1);
        y2 = y - 0x39;
        Sprite_SetBlend(1, 1);
        Records_DrawRank(rank, y, 0xE);
        D_005F6738 = 0x80808080;
        func_003CC078(x2, y2, 0);
        func_003CBFB8(kk, x2, y2, 0);
        func_003CF1E8(kk, x + 0xDC, y - 0x36);
        func_003C74A8(r, y);
        func_003C7530(k, p, x, y);
        func_003C9458(y);
    }
    func_003DBAA8(0, 0x27F, 0, 0x1BF);
}

/* NON_MATCHING: 24 lines, &r->unk126 is kept in s1 across the sort call (original recomputes it per loop) */
#if 0
s32 func_003C80E8(Records *r) {
    Pair p[10];
    Pair *q;
    s32 i;
    u32 j;

    r->unk120 = 0x96;
    r->unk124 = 10;
    for (i = 0, q = p; i < 10; i++, q++) {
        q->idx = i;
        q->val = r->score[i].score;
    }
    Sort_PairsByValue(p, 10, 1);
    for (j = 0; j < 10; j++) r->unk126[j] = p[j].idx;
    for (j = 0; j < 10; j++) {
        if (&r->score[r->unk126[j]] == r->lastScore) return j;
    }
    return -1;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C80E8);
#endif

void func_003C81D8(Records *r) {
    s32 i;

    for (i = 0; i < r->unk124; i++) func_003C75E8(i, r->unk126[i], &r->score[r->unk126[i]], 0, r->unk11A + i * 0x5D, 0);
}

/* NON_MATCHING: 51 lines, register allocation of the copy / sort loops (structure matches) */
#if 0
s32 func_003C8270(TimeEnt *t, TimeEnt *last) {
    Pair p[10];
    TimeEnt tmp[10];
    Pair *q;
    TimeEnt *e;
    s32 r = -1;
    s32 i;
    u16 v;

    for (i = 0, q = p; i < 10; i++, q++) {
        e = &t[i];
        v = e->time;
        tmp[i].c0 = e->c0;
        tmp[i].c1 = e->c1;
        tmp[i].time = v;
        Str_Copy(tmp[i].name, e->name);
        q->idx = i;
        if (e == last) r = i;
        q->val = v;
    }
    Sort_PairsByValue(p, 10, 0);
    for (i = 0, q = p; i < 10; i++, q++) {
        e = &t[i];
        e->c0 = tmp[q->idx].c0;
        e->c1 = tmp[q->idx].c1;
        e->time = tmp[q->idx].time;
        Str_Copy(e->name, tmp[q->idx].name);
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C8270);
#endif

s32 func_003C83A8(Records *r) {
    s32 n = func_003C8270(r->time, r->lastTime);
    s32 i;

    if (n != -1) r->lastTime = &r->time[n];
    r->unk124 = 10;
    for (i = 0; i < 10; i++) r->unk126[i] = i;
    r->unk120 = 0x96;
    return n;
}

void func_003C8428(Records *r) {
    s32 i;

    for (i = 0; i < r->unk124; i++) func_003C7AB0(i, &r->time[r->unk126[i]], 0, r->unk11A + i * 0x5D, 0);
}

void func_003C84B8(Records *r) {
    u8 set[0x30];
    Pair p[0x22];
    Pair *q;
    s32 k, i, total;
    u32 n;

    CharSet_Copy(D_0043B968, set);
    k = func_003BFF48(D_0043B990, D_0043B968);
    if (k != -1 && func_0035CD48(k) != 0) set[k] = 1;
    total = 0;
    for (i = 0; i < 0x22; i++) total += func_0035CD48(i);
    n = CharSet_Count((u32 *)set);
    r->unk124 = n;
    q = p;
    for (i = 0; i < 0x22; i++) {
        if (func_003ADA08(set, i) != 0) {
            s32 v;

            q->idx = i;
            v = func_0035CD48(i);
            q->val = v;
            q++;
            r->unk174[i] = Math_Permille(v, total);
        }
    }
    Sort_PairsByValue(p, n, 0);
    for (i = 0; i < n; i++) r->unk126[i] = p[i].idx;
    r->unk120 = 0x82;
    r->unk1B8 = r->unk174[p[0].idx];
}

void func_003C8658(Records *r) {
    s32 i;
    u16 v;
    s32 c;

    for (i = 0; i < r->unk124; i++) {
        v = r->unk174[r->unk126[i]];
        c = i < r->unk170 ? 0 : r->unk122;
        func_003C7F60(i, r->unk126[i], v, r->unk1B8, c, 0, r->unk11A + D_005F6E8C * i, 0);
    }
}

void func_003C8710(s32 i) {
    switch (i) {
    case 0:
        func_003C81D8(&D_00440D50);
        break;
    case 1:
        func_003C8428(&D_00440D50);
        break;
    case 2:
        func_003C8658(&D_00440D50);
        break;
    }
}

void func_003C8760(void) {
    func_003B7888();
}

void func_003C8768(Records *r) {
    s32 i;

    for (i = 0; i < r->unk124; i++)
        func_003C75E8(i, r->unk126[i], &r->score[r->unk126[i]], 0, r->unk11A + i * 0x5D, r->unk1BC == i);
}

void func_003C8808(Records *r) {
    s32 i;

    for (i = 0; i < r->unk124; i++) func_003C7AB0(i, &r->time[r->unk126[i]], 0, r->unk11A + i * 0x5D, r->unk1BC == i);
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520388);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_005203A0);

void Records_InitDefault(void) {
    CharPair10 a = D_00520388;
    CharPair10 b = D_005203A0;
    u8 set[0x30];
    Records *r = &D_00440D50;
    ScoreEnt *e;
    TimeEnt *t;
    s32 i;

    CharSet_Copy((void *)D_0051E850, set);
    for (i = 0, e = r->score; i < 10; i++, e++) {
        e->score = 0x57030;
        e->c0 = a.c[i][0];
        e->c1 = a.c[i][1];
        func_003D01C0(e->name, e->c0);
    }
    for (t = r->time, i = 0; i < 10; i++, t++) {
        t->c0 = b.c[i][0];
        t->c1 = b.c[i][1];
        t->time = ((9 - i) >> 1) + 1;
        func_003D01C0(t->name, t->c0);
    }
    func_003C8270(r->time, 0);
}

void func_003C8A38(void) {
    D_00440D50.unk110 = 0;
    D_00440D50.unk112 = 0;
    Records_InitDefault();
}

void func_003C8A50(void) {
    func_0035D078();
    gSubMode = 0;
    gMode = 7;
    func_003B77B8();
    func_00344BE0();
    Fx_StartStageLayers();
}

/* The records block as the name-entry flow sees it (D_00440D50) [V offsets; I names]. */
typedef struct {
    /* 0x000 */ u8 pad0[0x110];
    /* 0x110 */ u16 kind;     /* 0: score, 1: time, 2: ranking */
    /* 0x112 */ u8 flags;     /* a new record to enter (1: score, 2: time) */
    /* 0x113 */ u8 unk113;
    /* 0x114 */ u8 unk114;
    /* 0x115 */ u8 unk115;
    /* 0x116 */ u8 unk116;
    /* 0x117 */ u8 pad117;
    /* 0x118 */ u16 draw;     /* what the list draws */
    /* 0x11A */ s16 y;
    /* 0x11C */ s16 yTo;
    /* 0x11E */ u16 timer;
    /* 0x120 */ u16 unk120;
    /* 0x122 */ u16 unk122;
    /* 0x124 */ u16 count;
    /* 0x126 */ u8 pad126[0x46];
    /* 0x16C */ s32 first;
    /* 0x170 */ s32 unk170;
    /* 0x174 */ u8 pad174[0x46];
    /* 0x1BA */ u16 unk1BA;
    /* 0x1BC */ u16 unk1BC;
    /* 0x1BE */ u8 pad1BE[6];
    /* 0x1C4 */ s32 unk1C4;
    /* 0x1C8 */ s32 unk1C8;
} RecordsNE;
extern u32 D_0041571C[];
extern s32 D_005F5EE0;
extern s32 D_005F6510;
extern s32 D_005F6E84;
extern s32 D_005F6E88;
extern s32 D_005F6E90;
extern s32 D_005F6E94;
s32 func_003B1198(void);
s32 func_003639D8(void);
void NameEntry_Init(void *p, u16 a, u16 b, u16 c);
s32 NameEntry_Step(void *p, s32 mode);
void Mode_NameEntry(void);

/* NON_MATCHING: 257 lines (measured without the jtbl_00520430 include, which the C jump table replaces), state machine
 * complete; register allocation, case order and the D_01FAD328 / D_005F6510 accesses differ */
#if 0
void Mode_NameEntry(void) {
    RecordsNE *r = (RecordsNE *)&D_00440D50;
    u32 pad;
    s32 k;
    s32 n;
    s32 t;
    s32 d;

    if (gSubMode >= 3 && r->flags == 0 && func_003B1198() != 0 && gSubMode != 0x10) {
        gSubMode = 0x10;
        D_005F5EE0 = 1;
        D_005F6510 = 0;
    }
    switch (gSubMode) {
    case 0:
        D_005F6E84 = 0;
        D_005F6E88 = 0;
        if (r->flags == 0) {
            pad = D_0041571C[0];
            if (pad & 0x80) {
                k = 0;
            } else if (pad & 0x10) {
                k = 1;
            } else {
                k = r->kind;
                if (pad & 4) {
                    k = 2;
                }
            }
            r->kind = k;
            switch (k) {
            case 0:
                func_003C80E8((Records *)r);
                D_005F6E8C = 0x5D;
                D_005F6E90 = 4;
                D_005F6E94 = 0x99;
                break;
            case 1:
                func_003C83A8((Records *)r);
                D_005F6E8C = 0x5D;
                D_005F6E90 = 4;
                D_005F6E94 = 0x99;
                break;
            case 2:
                func_003C84B8((Records *)r);
                D_005F6E8C = 0x4B;
                D_005F6E90 = 5;
                D_005F6E94 = 0x87;
                break;
            }
            r->unk1BC = 0xFFFF;
            func_003BAA90();
            r->timer = 0;
        } else {
            r->unk1C4 = 0x40;
            D_005F6E8C = 0x5D;
            D_005F6E90 = 4;
            D_005F6E94 = 0x99;
            r->unk1C8 = 1;
            if (r->flags & 1) {
                r->kind = 0;
                r->unk1BC = func_003C80E8((Records *)r);
            } else if (r->flags & 2) {
                r->kind = 1;
                r->unk1BC = func_003C83A8((Records *)r);
            } else {
                r->flags = 0;
                r->kind = 2;
                break;
            }
            ((u8 *)&D_01FAD328)[0x10] = r->unk115;
            ((u8 *)&D_01FAD328)[0x11] = r->unk116;
            NameEntry_Init(&D_01FAD328, r->unk113, r->unk114, r->unk114);
            r->timer = 0;
        }
        gSubMode = 1;
        r->draw = 0;
        break;
    case 1:
        if (r->flags != 0) {
            Bgm_Play(5);
        }
        gSubMode = 2;
        break;
    case 2:
        switch (D_005F6E84) {
        case 0:
            func_003CF608(3);
            func_0035D0C8(5, func_003639D8());
            D_005F6E84 = 1;
            break;
        case 1:
            if (func_0035D120(0) != 0) {
                k = func_003639D8();
                Sprite_UploadBank(5, (u64 *)k, 0x3A98, func_0035D1C8(5));
                if (r->flags == 0) {
                    D_005F6E84 = 2;
                    func_00361AA8();
                } else {
                    D_005F6E84 = 4;
                    gSubMode = 3;
                }
            }
            D_005F6510 = 0;
            break;
        case 2:
            k = func_00361978();
            if (k == 0) {
                D_005F6E84 = 4;
                gSubMode = 3;
            } else if (k == 2) {
                D_005F6E84 = 3;
            }
            D_005F6510 = 0;
            break;
        case 3:
            k = func_00361978();
            func_003DBCF0(0, 0, 0x280, 0x1C0, 0xFFFE, D_005F6510 <= 16 ? D_005F6510 * 6 : 0x60);
            if (k == 1) {
                D_005F6E84 = 2;
            } else if (k == 2) {
                D_005F6510++;
            } else {
                D_005F6E84 = 4;
                D_005F6510 *= 3;
                if (D_005F6510 > 0x30) {
                    D_005F6510 = 0x30;
                }
            }
            break;
        case 4:
            func_003DBCF0(0, 0, 0x280, 0x1C0, 0xFFFE, D_005F6510 * 2);
            if (--D_005F6510 <= 0) {
                gSubMode = 3;
            }
            break;
        }
        break;
    case 3:
        r->draw = 1;
        gSubMode = 4;
        D_005F6510 = 0;
        break;
    case 4:
        D_005F6E84 = 5;
        D_005F6510 += 8;
        if (D_005F6510 >= 0x100) {
            if (r->flags != 0) {
                gSubMode = 0xB;
            } else {
                gSubMode = 5;
            }
        }
        break;
    case 5:
        n = r->count;
        r->unk170 = n;
        r->draw = 2;
        gSubMode = 6;
        k = n % D_005F6E90;
        if (k == 0) {
            k = D_005F6E90;
        }
        r->first = n - k;
        r->y = D_005F6E94 - n * D_005F6E8C;
        break;
    case 6:
        t = D_005F6E94 - r->first * D_005F6E8C;
        r->y += 0xC;
        r->yTo = t;
        if (r->y >= (s16)t) {
            r->y = t;
            gSubMode = 7;
            r->unk170 = r->first;
            r->timer = 0;
            r->unk122 = 0;
        }
        break;
    case 7:
        r->unk122 = r->timer < 0x40 ? r->timer : 0x40;
        if (++r->timer >= r->unk120) {
            if (r->first > 0) {
                gSubMode = 6;
                r->first -= D_005F6E90;
            } else {
                gSubMode = 8;
                D_005F6510 = 0;
            }
        }
        break;
    case 8:
        if (++D_005F6510 >= 0x3C) {
            gSubMode = 9;
        }
        break;
    case 9:
        D_005F6510 = 0x100;
        gSubMode = 0xA;
    case 10:
        if ((D_005F6510 -= 8) <= 0) {
            D_005F5EE0 = 1;
            gSubMode = 0xF;
            D_005F6510 = 0;
        }
        break;
    case 11:
        n = r->count;
        k = r->unk1BC;
        if (k < 2) {
            r->unk1BA = k;
            r->first = 0;
        } else if (n - 3 < k) {
            r->first = n - D_005F6E90;
            r->unk1BA = D_005F6E90 - (n - k);
        } else {
            r->first = k - 2;
            r->unk1BA = 2;
        }
        r->draw = 2;
        r->unk122 = 0x40;
        gSubMode = 0xC;
        r->unk170 = 0;
        r->y = D_005F6E94 - n * D_005F6E8C;
        break;
    case 12:
        r->yTo = D_005F6E94 - r->first * D_005F6E8C;
        d = (r->yTo - r->y + 0xF) >> 4;
        if (d == 0) {
            d = 1;
        } else if (d > 16) {
            d = 16;
        }
        r->y += d;
        if (r->y >= r->yTo) {
            r->y = r->yTo;
            r->timer = 0xBB8;
            r->draw = 3;
            gSubMode = 0xD;
        }
        break;
    case 13:
        k = NameEntry_Step(&D_01FAD328, --r->timer == 0);
        func_003C7208((Records *)r, (u8 *)&D_01FAD328);
        if (((u8 *)&D_01FAD328)[6] != 0) {
            Snd_PlaySe(4, 0);
        }
        if (((u8 *)&D_01FAD328)[7] != 0) {
            Snd_PlaySe(3, 0);
        }
        if (k != 0) {
            r->flags = 0;
            func_00361968();
            r->draw = 4;
            gSubMode = 0xE;
            D_005F6510 = 0;
        }
        break;
    case 14:
        if (++D_005F6510 >= 0x78) {
            gSubMode = 9;
        }
        break;
    case 15:
        if (D_005F6510++ >= 3) {
            func_003992C8(1);
            D_005F5EE0 = 0;
            Mode_Change(2);
        }
        break;
    case 16:
        if (D_005F6510++ >= 3) {
            func_0035D090();
            func_003992C8(1);
            D_005F5EE0 = 0;
            Mode_Change(3);
        }
        break;
    }
    if (D_005F6E84 == 5) {
        switch (r->draw) {
        case 0:
            break;
        case 2:
            func_003C8710(r->kind);
        case 1:
            func_003C93A0(r->kind);
            break;
        case 3:
        case 4:
            switch (r->kind) {
            case 0:
                func_003C8768(&D_00440D50);
                break;
            case 1:
                func_003C8808(&D_00440D50);
                break;
            }
            func_003C93A0(r->kind);
            break;
        }
    }
    if (gMode < 8 && gMode >= 6) {
        func_003C8760();
    }
    if ((u16)(gMode - 6) >= 2 && func_0035D120(0) == 0) {
        func_0035D120(1);
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_005203C0);

INCLUDE_ASM("asm/nonmatchings/select_results_b", Mode_NameEntry);

INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_00520410);
#endif

void func_003C9358(s32 v) {
    D_00440E60[0] = v;
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_00520430);

void func_003C9368(void) {
    func_003C80E8(&D_00440D50);
}

void func_003C9378(void) {
    func_003C83A8(&D_00440D50);
}

void func_003C9388(void) {
    func_003C84B8(&D_00440D50);
}

void func_003C9398(void) {
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520448);

void func_003C93A0(s32 i) {
    WordPair3 t = D_00520448;

    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    D_005F6738 = 0x80808080;
    D_005F6730 = 0xFFFF0;
    Sprite_Draw(1, t.e[i].a, t.e[i].b, 0, 0x14);
}

void func_003C9458(s32 y) {
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    D_005F6738 = 0x80808080;
    D_005F6730 = 0xFFFF0;
    Sprite_DrawBlended(1, 0xFB, 0xFC, 0, y);
}

void func_003C94C0(s32 i) {
    if (D_0043B7F8.unkA != 0) D_004157F8[i].unk1 = -1;
}

void func_003C94F0(void) {
    D_005F6EAD = 0;
    D_005F6EAA = 0;
    D_005F6EAB = -1;
    D_005F6EAC = -1;
    D_005F6EB0 = 0;
}

s32 func_003C9510(void) {
    return D_005F6EB4;
}

/* D_00440C28 seen as eight 0xC-byte member slots per side [V offsets]. */
typedef struct {
    s32 a;
    s32 b;
    s32 c;
} SelSlotM;
typedef struct {
    SelSlotM m[8];
} SelSlotsM;
extern s16 D_005F62A8[2];
extern s32 D_005F6EC0[2];
extern u16 D_005F5CB0[2];
void func_003B0120(s32 i);
void func_003AEDA0(s32 i);
void func_003AEDD8(s32 i);

/* NON_MATCHING: 30 lines, the slot address adds pl * 0x60 and n * 0xC in the other order and D_005F6EAA is loaded early */
#if 0
void func_003C9518(SelState *s, SelSide *d, s32 pl) {
    s32 se = 0;
    s16 *cnt;
    s32 n;

    if (s->unk1C != 0) {
        return;
    }
    cnt = &D_005F62A8[pl];
    ((SelSlotsM *)&D_00440C28[pl])->m[*cnt].a = -1;
    ((SelSlotsM *)&D_00440C28[pl])->m[*cnt].b = -1;
    if (*cnt == 0) {
        if (D_0043B7F8.unk0 == 2) {
            d->unk0 = 4;
            d->unk10 = func_003C98A8(s, (SelState *)d);
            se = 1;
            if (d->unk10 == 0x37) {
                D_005F6EC0[pl] = 1;
                d->unk10 = D_005F6708[pl];
            }
        }
        if (D_005F6EAA) {
            D_005F5CB0[D_005F6EAB] = 0;
            func_003C9518(s, d, D_005F6EAC);
            func_003AEDA0(D_005F6EAC + 2);
            func_003AEDD8(D_005F6EAB);
            func_003CB8E0(D_005F6EAC, 5);
            func_003CB8E0(D_005F6EAB, 2);
            func_003C94F0();
        }
    } else {
        func_003B0120(pl);
        if (d->unk0 == 3) {
            d->unk0 = 1;
        }
        se = 1;
        if (*cnt == 1) {
            func_003C94C0(pl);
        }
        n = --*cnt;
        if (n == 0) {
            func_003AEDD8(pl + 2);
        }
        d->unk4 = D_00440D40[pl][n];
        d->unk10 = func_003C98A8(s, (SelState *)d);
        d->unk14 = 0;
        if (d->unk10 == 0x37) {
            D_005F6EC0[pl] = 1;
            d->unk10 = D_005F6708[pl];
            if (n == 0) {
                func_003AEDD8(pl);
            }
        }
        ((SelSlotsM *)&D_00440C28[pl])->m[n].a = -1;
        ((SelSlotsM *)&D_00440C28[pl])->m[n].b = -1;
        se = 1;
    }
    if (se) {
        Snd_PlaySe(3, 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C9518);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520460);

void func_003C9778(SelState *unused) {
    Unk440A68 *e = D_00440A68;
    s32 i;

    for (i = 0; i < 0x24; i++, e++) {
        u8 c;

        e->unk5 = 1;
        c = D_00520460[i];
        if (func_003ADA08(D_0043B968, c) != 0 || c == 0x37) e->unk6 = c;
        else e->unk6 = 0x36;
    }
    if (D_0043B7F8.unk8 != 0) {
        D_00440A68[19].unk2 = 0x22;
        D_00440A68[26].unk2 = 0x23;
        D_00440A68[34].unk1 = 0x13;
        D_00440A68[35].unk1 = 0x1A;
    } else {
        D_00440A68[35].unk5 = 0;
        D_00440A68[19].unk2 = 0x24;
        D_00440A68[26].unk2 = 0x24;
        D_00440A68[34].unk1 = 0x24;
        D_00440A68[34].unk5 = 0;
        D_00440A68[35].unk1 = 0x24;
    }
}

void func_003C9888(SelState *s) {
    s->unkC = 0x3C;
    s->unk8 = 0x960;
    D_005F6EA4 = 0x5A;
}

s32 func_003C98A8(SelState *unused, SelState *s) {
    s32 i = s->unk4;

    return D_00440A68[i].unk6;
}

s32 func_003C98C8(SelState *s) {
    return D_00520460[s->unk4] == 0x37;
}

/* NON_MATCHING: 10 lines, the switch tree on the func_003BA450 result drops case 1 (original tests 1, < 2, 2, 3) */
#if 0
void func_003C98E8(SelState *a, SelState *s, s32 side) {
    switch (func_003BA450(side)) {
    case 1:
        break;
    case 2:
        s->unk4 = D_004157F8[side].unk0;
        break;
    case 3:
        s->unk4 = D_004157F8[side].unk0;
        break;
    }
    if (D_0043B7F8.unk0 == 7 && D_005F6EAA != 0) s->unk4 = D_005F6EA8;
    s->unk10 = func_003C98A8(a, s);
    if (s->unk10 == 0x37) s->unk10 = D_005F6708[side];
    D_005F6EC8[side] = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C98E8);
#endif

/* NON_MATCHING: 44 lines, register allocation of the retry loop (original keeps &D_005F6708[side], &D_00440C28[side] and both set addresses in s0-s4) */
#if 0
void func_003C99C8(s32 side, s32 x) {
    if (D_00440CE8.item[0].unk0 != 0) return;
    if (++D_005F6E98[side] < 3) return;
    D_005F6E98[side] = 0;
    do {
        if (++D_005F6708[side] == 0x22) D_005F6708[side] = 0;
    } while (func_003ADA08(x < 0xB4 ? D_0043B968 : D_0043B990, D_005F6708[side]) == 0 ||
             D_005F6708[side] == D_00440C28[side].unk0);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C99C8);
#endif

void func_003AEEF0(s32 i);
s32 func_003C9AC0(SelState *s, SelSide *d, s32 btn, s32 prev, s32 *moved, s32 side, s32 x);

/* NON_MATCHING: 16 lines, the original spills prev and ret to sp+0 / sp+4 (here ret is volatile, its slot and stores differ) */
#if 0
s32 func_003C9AC0(SelState *s, SelSide *d, s32 btn, s32 prev, s32 *moved, s32 side, s32 x) {
    s32 start;
    s32 cur;
    volatile s32 ret; /* fake: the original spills it to the stack */

    ret = 0;
    func_003C99C8(side, x);
    start = d->unk4;
    if (x == 0) {
        if (D_00440A68[start].unk6 != 0x37 && btn == 0xFFFF) {
            cur = 0;
        } else {
            cur = start;
        }
        goto draw;
    } else {
        if (btn & 0xA000) {
            btn &= 0xA000;
        }
        ret = (btn & 0xF000) != 0;
        switch (btn) {
        case 0x8000:
            cur = D_00440A68[start].unk3;
            func_003C94C0(side);
            break;
        case 0x2000:
            cur = D_00440A68[start].unk4;
            func_003C94C0(side);
            break;
        case 0x1000:
            cur = D_00440A68[start].unk1;
            func_003C94C0(side);
            break;
        case 0x4000:
            cur = D_00440A68[start].unk2;
            func_003C94C0(side);
            break;
        default:
            d->unk10 = func_003C98A8(s, (SelState *)d);
            if (d->unk10 == 0x37) {
                D_005F6EC0[side] = 1;
                d->unk10 = D_005F6708[side];
                if (((s8 *)D_005F6EC8)[side] != 0) {
                    *moved = 0;
                } else {
                    *moved = 1;
                }
            } else {
                D_005F6EC0[side] = 0;
                *moved = 0;
            }
            D_005F6EC8[side] = 1;
            goto out;
        }
    }
top:
    if (D_00440A68[cur].unk6 == 0x37 && prev != btn) {
        cur = 0x24;
    }
draw:
    func_003C99C8(side, x);
    *moved = 0;
    if (D_00440A68[cur].unk6 == 0x36) {
        switch (btn) {
        case 0x8000:
            cur = D_00440A68[cur].unk3;
            break;
        case 0x2000:
            cur = D_00440A68[cur].unk4;
            break;
        case 0x1000:
            cur = D_00440A68[cur].unk1;
            break;
        case 0x4000:
            cur = D_00440A68[cur].unk2;
            break;
        }
        goto top;
    }
    if (cur != 0x24 && cur != start) {
        Snd_PlaySe(3, 0);
        d->unk4 = cur;
        d->unk14 = 0;
        *moved = 1;
    }
    d->unk10 = func_003C98A8(s, (SelState *)d);
    D_005F6EC0[side] = 0;
    if (d->unk10 == 0x37) {
        D_005F6EC0[side] = 1;
        d->unk10 = D_005F6708[side];
        *moved = 1;
    }
    if (*moved) {
        func_003AEEF0(side + D_005F62A8[side] * 2);
    }
out:
    return ret;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C9AC0);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520488);

void func_003C9E70(SelState *s) {
    const u8 *p = &D_00520488[s->unk0 * 2];

    s->unk8 = p[0];
    s->unkC = p[1];
}

void func_003C9E98(s32 unused, s32 side) {
    D_0043B7F8.u.t.team[side].unk1C = 0;
    D_0043B7F8.u.t.team[side].unk1A = 8;
    D_005F6700[side] = 4;
}

/* 16-byte per-character entries; [2] / [3] / [4] the alternate costumes (-1: none) [I]. */
extern s16 D_004FF690[];
extern s8 D_0043B9B8[];
extern s32 D_005F6668;
extern s32 D_005F666C;
s32 func_003ADBE8(s32 side, s32 chr, s32 *cos);
#define SLOT(side, i) (*(SelSlotM *)((u8 *)D_00440C28 + (i) * 12 + (side) * 0x60))

s32 func_003C9ED8(SelState *s, SelSide *d, s32 side, s32 btn);

/* NON_MATCHING: 76 lines, the 0x80 case keeps its own d->unk14 = 0 block (original: bnezl into the shared one) and the second slot loop reloads `other` in another order; most lines are the one-instruction shift */
#if 0
s32 func_003C9ED8(SelState *s, SelSide *d, s32 side, s32 btn) {
    u8 set[0x30];
    s32 other = side ^ 1;
    s32 i;

    if (s->unk8 != 0) {
        if (!(btn & 0x8F4)) {
            return 1;
        }
    } else {
        btn = 0x80;
    }
    if (D_0043B7F8.unk0 == 2) {
        if ((btn & 0x800) && D_0043B7F8.u.t.team[side].unk1C == 0) {
            D_0043B7F8.u.t.team[side].unk1C = 1;
            D_0043B7F8.u.t.team[side].unk1A = D_005F62A8[side];
        }
        if (D_0043B7F8.u.t.team[side].unk1C != 0) {
            CharSet_Copy(D_0043B968, set);
            for (i = 0; i < D_005F6700[side]; i++) {
                if (SLOT(side, i).a >= 0) {
                    func_003ADA18(SLOT(side, i).a, set);
                }
            }
            d->unk10 = func_00358A98((s8 *)set);
        }
    }
    if (func_003C98C8((SelState *)d) != 0 ||
        (D_0043B7F8.u.t.team[side].unk1C != 0 && D_0043B7F8.unk0 == 2)) {
        switch (func_003800D8(5)) {
        case 0:
            btn = 0x80;
            break;
        case 1:
            btn = 0x10;
            break;
        case 2:
            btn = 4;
            break;
        case 3:
            btn = 0x40;
            break;
        case 4:
            btn = 0x20;
            break;
        }
    }
    for (i = 0; i < D_005F6700[side]; i++) {
        if (SLOT(side, i).a == d->unk10) {
            return 2;
        }
    }
    if (btn & 0x80) {
        d->unk14 = 0;
    } else if (btn & 0x40) {
        d->unk14 = 1;
    } else if (btn & 0x10) {
        if (D_004FF690[d->unk10 * 8 + 2] != -1 && D_0043B9B8[d->unk10] != 0) {
            d->unk14 = 2;
        } else {
            d->unk14 = 0;
        }
    } else if (btn & 0x20) {
        if (D_004FF690[d->unk10 * 8 + 3] != -1 && D_0043B9B8[d->unk10] != 0) {
            d->unk14 = 3;
        } else {
            d->unk14 = 1;
        }
    } else if (btn & 4) {
        if (D_004FF690[d->unk10 * 8 + 4] != -1 && func_003AE608(d->unk10, 4) != 0) {
            d->unk14 = 4;
        } else {
            d->unk14 = 0;
        }
    } else if (btn & 0x800) {
        if (D_004FF690[d->unk10 * 8 + 4] == -1 || D_0043B7F8.unk0 == 2 || func_003AE608(d->unk10, 4) == 0) {
            return 1;
        }
        d->unk14 = 4;
    }

    if (D_005F6EC0[side] == 1 && d->unk10 == 7 && func_003800D8(6) == 0) {
        d->unk14 = 5;
    }
    if (D_005F6668 == 0 && D_005F666C == 0) {
        for (i = 0; i < D_005F6700[other]; i++) {
            if (SLOT(other, i).a == d->unk10 &&
                SLOT(other, i).b == d->unk14) {
                d->unk14 = func_003C0210(SLOT(other, i).a,
                                         SLOT(other, i).b);
            }
        }
    }
    func_003ADBE8(side, d->unk10, &d->unk14);
    if (D_005F62A8[side] < D_005F6700[side] - 1) {
        if (D_0043B7F8.unk0 != 2 || D_0043B7F8.u.t.team[side].unk1C != 1) {
            Snd_PlaySe(4, side);
        }
    } else {
        Snd_PlaySe(5, side);
    }
    return 0;
}
#else
INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_005204A0);

INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003C9ED8);
#endif

void func_003CA418(void) {
    D_005F66FA = 0;
    D_00440CE8.item[0].unk0 = 0;
}

/* NON_MATCHING: 8 lines, the 0x5000 test becomes a branch and the 0xA000 test a movn (original: the reverse) */
#if 0
s32 func_003CA428(s32 b) {
    if (b & 0x1040F) b |= 4;
    b = ((b & 0x5000) != 0x5000) ? b & 0xF9F4 : b & 0xA9F4;
    if ((b & 0xA000) == 0xA000) return b & 0x59F4;
    return b;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003CA428);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_005204B8);
INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_005204D0);

#if 0
/* NON_MATCHING: 1292 lines, written from the asm for the PC port (T34 b2: the m2c draft before it read the pad words, the
   member counts and the game type as bytes / words of the wrong size, so the cursor never moved); not compared for
   matching yet. The C emits its own jump table, so jtbl_005204D0 must go when it is enabled. */
extern u16 D_005F5C80[2];
extern s32 D_005F6EB8;
extern u32 D_00415728[];
extern u32 D_00415738[];
extern u32 D_00415758[];
extern u32 D_00415768[];
extern const char D_005204B8[];
extern Fighter gFighters[4];
void func_003AEC58(s32 i, s32 a, s32 b, s32 c);
void func_003AECA8(s32 side, s32 ch, s32 costume, s32 c);
void func_003AE9D8(s32 i);
void func_003AFF40(s32 i);
void func_003B0660(s16 i);
void func_003BF7F8(void);
void func_003BF820(s32 d, s32 b);
void func_003AEF88(s32 side, s32 b, s32 c, s32 hold);
/* D_00440C28 as the picked members: 8 per side (0x60 bytes) [V offsets] */
typedef struct {
    /* 0x0 */ s32 chr;
    /* 0x4 */ s32 cos;
    /* 0x8 */ s16 id; /* chr * 8 + costume */
    /* 0xA */ s16 padA;
} SelMember;
#define MEMBER(side, i) (((SelMember *)D_00440C28)[(side) * 8 + (i)])

/* Both directions of an axis held cancel each other. */
static s32 sel_axes(s32 b) {
    if ((b & 0x5000) == 0x5000) {
        b &= ~0x5000;
    }
    if ((b & 0xA000) == 0xA000) {
        b &= ~0xA000;
    }
    return b;
}

/* The pad bits of every port that plays this side, ORed. */
static void sel_side_pads(s32 side, s32 *held, s32 *trig, s32 *last) {
    s32 n = func_003B01B0(side) + 1;
    s32 i;

    for (i = 0; i < n; i++) {
        s32 p = func_003AFFB8(side, i);
        *last = p;
        *held |= D_00415768[p];
        *trig |= D_00415758[p];
    }
}

/* One side's step of the character select: cursor, picks, the VS handicap and member count screens. */
void func_003CA470(SelState *s, s32 side) {
    SelSide *d = &s->side[side];
    GameSetup *g = &D_0043B7F8;
    s32 *timer = &D_005F6EB8 + side;
    s32 sp0, sp4, moved, spC;
    s32 btn, trig, rep, ret, r, n, i, p, c, dx, v;

    if (d->unk18 > 0) {
        d->unk18--;
    }
    p = 0;
    switch (d->unk0) {
    case 0:
        if (s->unk8 != 0) {
            func_003C9888(s);
        }
        d->unk18 = 0;
        if (g->unk0 == 2) {
            func_003C9E98((s32)d, side);
        }
        if (D_005F5C80[side] != 0) {
            D_005F62A8[side] = D_005F6700[side];
            d->unk18 = 0x5A;
        } else if (D_005F5CB0[side] == 0) {
            D_005F62A8[side] = D_005F6700[side];
            (&D_005F6668)[side] = 1;
        } else {
            (&D_005F6668)[side] = 0;
            D_005F62A8[side] = 0;
            for (i = 0; i < 8; i++) {
                MEMBER(side, i).chr = -1;
                MEMBER(side, i).cos = -1;
                MEMBER(side, i).id = -1;
            }
        }
        func_003C98E8(s, (SelState *)d, side);
        r = func_003BA450(side);
        if (r == 2) {
            sp0 = 0;
            c = func_003C98A8(s, (SelState *)d);
            if (c == 0x37) {
                c = D_005F6708[side];
            }
            if (g->unk8 == 0) {
                func_003ADBE8(side, c, &sp0);
                func_003AEC58(side, c, sp0, 0);
            }
            d->unk0 = g->unk0 != r ? 1 : 4;
            *timer = 0x3C;
        } else if (r < 3) {
            if (r >= 0) {
                d->unk0 = 2;
            }
        } else if (r == 3) {
            func_003ADBE8(side, MEMBER(side, 0).chr, &MEMBER(side, 0).cos);
            func_003AECA8(side, MEMBER(side, 0).chr, MEMBER(side, 0).cos, 1);
            func_003AE9D8(side);
            if ((u32)(g->unk0 - 5) >= 2) {
                func_003ADBE8(side, MEMBER(side, 1).chr, &MEMBER(side, 1).cos);
                func_003AECA8(side + 2, MEMBER(side, 1).chr, MEMBER(side, 1).cos, 1);
                func_003AE9D8(side + 2);
            }
            d->unk0 = 6;
        }
        return;
    case 1:
        trig = 0;
        btn = 0;
        if (D_005F6EAA == 0) {
            p = func_003AFF08(side);
            if (p != -1) {
                trig = func_003CA428(D_00415728[p]);
                btn = (D_00415738[p] & 0xFFFFA300) | (trig & 0x58F4);
            }
        } else {
            n = func_003B01B0(D_005F6EAC) + 1;
            for (i = 0; i < n; i++) {
                p = func_003AFFB8(D_005F6EAC, i);
                if (p != -1) {
                    trig |= func_003CA428(D_00415728[p]);
                    btn |= (trig & 0x58F4) | (D_00415738[p] & 0xFFFFA300);
                } else {
                    trig = 0;
                    btn = 0;
                }
            }
        }
        btn = sel_axes(btn);
        trig = sel_axes(trig);
        if (D_005F66FA != 0) {
            trig = 0;
            btn = 0;
        }
        if (trig & 0x100) {
            func_003C9518(s, d, side);
            break;
        }
        /* the held repeat for the costume buttons, every other frame once the timer runs out */
        if (*timer < 2) {
            rep = btn;
        } else {
            rep = 0;
            *timer -= 2;
        }
        for (;;) {
            ret = func_003C9AC0(s, d, btn, trig, &moved, side, s->unk8);
            if (D_005F62A8[side] == 1 && d->unk10 == MEMBER(side, 0).chr) {
                s->unk14[side] = 0;
            } else if (func_003C98C8((SelState *)d) != 0) {
                s->unk14[side] = 0;
            } else {
                s->unk14[side] = 1;
            }
            if (moved != 0) {
                spC = 0;
                func_003ADBE8(side, d->unk10, &spC);
                if (g->unk8 == 0) {
                    func_003AEC58(side + D_005F62A8[side] * 2, d->unk10, spC, 0);
                } else if (D_005F62A8[side] == 0) {
                    gFighters[side].unk26 = (u16)spC + (u16)d->unk10 * 8;
                }
            }
            if (ret != 0) {
                break;
            }
            if (func_003C9ED8(s, d, side, rep) != 0) {
                if (s->unk8 != 0) {
                    break;
                }
                btn = 0xFFFF;
                continue;
            }
            d->unk18 = 0x5A;
            *timer = 0x14;
            n = D_005F62A8[side];
            MEMBER(side, n).chr = d->unk10;
            MEMBER(side, n).cos = d->unk14;
            MEMBER(side, n).id = (u16)d->unk10 * 8 + ((u16)d->unk14 & 7);
            if (g->unk8 != 0 || gFighters[side + n * 2].unk26 != d->unk10 * 8 + d->unk14 ||
                func_003C98C8((SelState *)d) != 0) {
                n = D_005F62A8[side];
                if (func_003ADBE8(side, MEMBER(side, n).chr, &MEMBER(side, n).cos) == 0) {
                    n = D_005F62A8[side];
                    func_003AEC58(side + n * 2, MEMBER(side, n).chr, MEMBER(side, n).cos, 1);
                }
            }
            func_003AE9D8(side + D_005F62A8[side] * 2);
            if (g->unkA != 0) {
                ((s8 *)&D_004157F8[side])[D_005F62A8[side]] = d->unk4;
            }
            D_00440D40[side][D_005F62A8[side]] = d->unk4;
            func_003AFF40(side);
            if (++D_005F62A8[side] == 1) {
                func_003B0660((s16)side);
            }
            c = D_004157F8[side].unk1;
            if (c != -1 && D_005F62A8[side] == 1) {
                d->unk4 = c;
                sp4 = 0;
                d->unk10 = func_003C98A8(s, (SelState *)d);
                if (d->unk10 == 0x37) {
                    d->unk10 = 0;
                } else {
                    func_003ADBE8(side + 2, d->unk10, &sp4);
                    func_003AEC58(side + 2, d->unk10, sp4, 0);
                }
            }
            d->unk14 = 0;
            if (g->unk0 == 2 && D_005F62A8[side] < D_005F6700[side] && g->u.t.team[side].unk1C != 0) {
                continue; /* team battle, random members: the next pick at once */
            }
            if (D_005F62A8[side] != D_005F6700[side]) {
                if (s->unk8 != 0) {
                    break;
                }
                btn = 0xFFFF;
                continue;
            }
            /* the side is complete */
            if (g->unk0 == 7) {
                s32 o = side ^ 1;

                if (D_005F5CB0[o] == 0) {
                    func_003AE738(0, o);
                    D_005F6EAC = side;
                    D_005F6EAD = 1;
                    D_005F6EAA = 1;
                    D_005F6EAB = o;
                    D_005F6EA8 = d->unk4;
                    func_003CB8E0((s8)o, 0);
                    func_003B0380(D_005F6EAB);
                    func_003C94C0(D_005F6EAB);
                } else {
                    D_005F6EAA = 0;
                    (&D_005F6668)[D_005F6EAB] = 1;
                }
            }
            s->unk14[side] = 1;
            if (g->unk0 == 1 || g->unk0 == 6) {
                d->unk0 = 3;
            } else {
                d->unk0 = 6;
                D_005F6EB0 = 7;
            }
            break;
        }
        if (g->unk0 == 1 || g->unk0 == 6) {
            if (s->unk1C == 0) {
                g->u.v.rec[side].unk4 += 0x14;
            }
            if (g->u.v.rec[side].unk4 > 0x180) {
                g->u.v.rec[side].unk4 = 0x180;
            }
        }
        break;
    case 3: /* VS: the handicap */
        btn = 0;
        trig = 0;
        sel_side_pads(side, &btn, &trig, &p);
        btn = sel_axes(btn);
        trig = sel_axes(trig);
        if (trig & 0x400) {
            trig |= 4;
        }
        if (D_00415728[p] & 0x100) {
            func_003C9518(s, d, side);
            break;
        }
        if (s->unk1C == 0) {
            g->u.v.rec[side].unk4 -= 0x14;
        }
        if (g->u.v.rec[side].unk4 < 0) {
            g->u.v.rec[side].unk4 = 0;
        }
        dx = ((btn >> 13) & 1) - ((btn >> 15) & 1);
        if (side == 0) {
            dx = -dx;
        }
        v = g->u.v.rec[side].pad2;
        if (dx == 0) {
            s32 u = (btn >> 12) & 1;
            if (btn & 0x4000) {
                u--;
            }
            dx = u * 10;
        }
        v += dx;
        if (v <= 0) {
            v = 1;
        } else if (v >= 0x97) {
            v = 0x96;
        } else if (dx != 0) {
            Snd_PlaySe(3, 0);
        }
        g->u.v.rec[side].pad2 = v;
        if (trig & 0x8FC) {
            Snd_PlaySe(5, side);
            D_005F6EB0 |= 1 << side;
            d->unk0 = 6;
            func_003BF7F8();
        }
        break;
    case 4: /* team battle: the member count */
        n = D_005F6700[side];
        p = func_003AFF08(side);
        btn = (D_00415768[p] & 0xFFFFA7FF) | (D_00415758[p] & 0x5800);
        dx = ((btn >> 13) & 1) - ((btn >> 15) & 1);
        n += dx;
        if (n <= 0) {
            n = 1;
        } else if (n >= 9) {
            n = 8;
        } else if (dx != 0) {
            Snd_PlaySe(3, 0);
        }
        if (btn & 0x8FC) {
            Snd_PlaySe(5, side);
            d->unk0 = 1;
        }
        g->u.t.team[side].n = n;
        D_005F6700[side] = n;
        if ((&D_005F6668)[side ^ 1] != 0) {
            g->u.t.team[side ^ 1].n = n;
            D_005F6700[side ^ 1] = n;
            D_005F62A8[side ^ 1] = D_005F6700[side ^ 1];
        }
        break;
    case 5:
        func_003CB8E0(side, 1);
        break;
    case 6:
        if (g->unk0 == 1 || g->unk0 == 6) {
            if (s->unk1C == 0) {
                g->u.v.rec[side].unk4 -= 0x14;
            }
            if (g->u.v.rec[side].unk4 < 0) {
                g->u.v.rec[side].unk4 = 0;
            }
            btn = 0;
            n = func_003B01B0(side) + 1;
            for (i = 0; i < n; i++) {
                btn |= D_00415768[func_003AFFB8(side, i)];
            }
            btn = sel_axes(btn);
            func_003BF820(((btn >> 13) & 1) - ((btn >> 15) & 1), btn & 0x8FC);
        }
        break;
    }
    /* the preview fighters */
    if ((func_003C98C8((SelState *)d) == 0 && D_005F62A8[side] == 0) || D_005F62A8[side] > 0) {
        func_003AEF88(side, 0, 0, s->unk1C);
    } else {
        gFighters[side].unk127 = 0;
    }
    if ((u32)(g->unk0 - 5) >= 2) {
        if (s->unk14[side] != 0) {
            func_003AEF88(side + 2, 0, 0, s->unk1C);
        } else {
            gFighters[side + 2].unk127 = 0;
            if (d->unk18 == 0 && D_005F62A8[side] > 0 && g->unk8 == 0) {
                Text_Printf(D_005204B8, side == 0 ? 0x3E : 0x1E0, 0x15E, 10, 5, 0x172);
            }
        }
    }
    func_003C9E70((SelState *)d);
}
#undef MEMBER
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003CA470);
#endif

void func_003CB508(s32 unused, SelState *s) {
    SelSide *p = s->side;
    s32 k = 2;
    s32 side, j;

    func_003BF7F0();
    if ((u8)(D_0043B7F8.unk0 - 5) < 2) k = 1;
    func_003C9888(s);
    for (side = 0; side < 2; side++, p++) {
        for (j = 0; j < 8; j++) D_00440D40[side][j] = -1;
        p->unk0 = 0;
        p->unk14 = 0;
        D_005F6700[side] = k;
        func_003CA470(s, side);
    }
    D_005F66F8 = 0;
    D_005F6EB4 = 0;
}

extern u16 D_005F5C80[2];
extern s8 D_005F5C98[2][2];
extern s8 D_005F5CA0[2][2];
extern u16 D_005F5CA8[2];
extern u8 D_00439C60[];

/* NON_MATCHING: 123 lines, register allocation: the D_0043B7F8 high half is kept in t9 here (the original reloads it per test) and the loop registers shift */
#if 0
void func_003CB610(GameSetup *g, SelState *unused) {
    s32 side;
    s32 j;

    for (side = 0; side < 2; side++) {
        if (D_0043B7F8.unk0 == 7 && side == D_005F6EAB) {
            D_005F5CB0[side] = 0;
            D_005F5C80[side] = 0;
        }
        if (D_005F5CB0[side] == 0) {
            if (D_0043B7F8.unk0 != 7) {
                continue;
            }
        } else if (g->unkA) {
            D_00439C60[side * 2 + 0x38] = D_004157F8[side].unk0;
            if (D_0043B7F8.unk0 - 5 >= 2U) {
                D_00439C60[side * 2 + 0x39] = D_004157F8[side].unk1;
            }
        }
        for (j = 0; j < 2; j++) {
            func_003ADB70(side, j, 1);
            D_00440940[side][j].unk0 = ((SelSlotsM *)&D_00440C28[side])->m[j].a;
            D_00440940[side][j].unk4 = ((SelSlotsM *)&D_00440C28[side])->m[j].b;
        }
        D_005F5C98[side][0] = D_00440940[side][0].unk0;
        D_005F5CA0[side][0] = D_00440940[side][0].unk4;
        D_005F5C98[side][1] = D_00440940[side][1].unk0;
        D_005F5CA0[side][1] = D_00440940[side][1].unk4;
        if (D_0043B7F8.unk0 == 2) {
            for (j = 0; j < 8; j++) {
                g->u.t.team[side].id[j] = ((SelSlotsM *)&D_00440C28[side])->m[j].a;
                g->u.t.team[side].val[j] = ((SelSlotsM *)&D_00440C28[side])->m[j].b;
            }
        }
        D_005F5CA8[side] = 0;
        D_005F5C80[side] = 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003CB610);
#endif

void func_003CB8E0(s32 i, s32 v) {
    D_00440CE8.item[i].unk4 = v;
}

void func_003BDF68(SelState *s);
s32 func_003AF448(void);
void func_003AF380(void);
void func_003AE7C8(void);
void func_003AEA30(SelState *s);
void func_003B0820(SelState *s);
s32 func_003B04A0(void);
void func_003BECC0(SelState *s);
void func_003BF8F8(void);
s32 func_003BDF60(void);
void func_003B0D50(s32 port, s32 side, s32 blink);
extern s32 D_005F5EE0;
extern s32 D_005F6EA0;
extern s32 D_005F6678;
extern s32 D_005F6EB8;
extern s32 D_005F6EBC;
extern s16 D_005F62AA;
extern s32 D_005F6704;
extern s32 D_005F5F20;
extern u16 D_005F5CB4;

s32 Mode_CharSelect(void);

/* NON_MATCHING: 80 lines, the state pointer and the counter / show flag take s1 / s2 the other way round (the rest is the same code) */
#if 0
s32 Mode_CharSelect(void) {
    SelState *s = (SelState *)&D_00440CE8;
    GameSetup *g = &D_0043B7F8;
    s32 show = 1;
    s32 i;
    s32 p;

    func_003BDF68(s);
    if (gSubMode >= 3 && s->unk4 >= 3) {
        if (func_003AF448() != 0 && D_005F66FA != 0 && ++D_005F66FA >= 4) {
            func_003B7A68();
            func_003992C8(1);
            return 1;
        }
        if (D_005F66FA == 0 && func_003B1378() != 0) {
            func_003AF380();
            D_005F5EE0 = 1;
            s->unk1C = 1;
            D_005F66FA = 1;
        }
    }
    switch (gSubMode) {
    case 0:
        D_005F6EA0 = 0;
        D_005F66FA = 0;
        for (show = 0; show < 2; show++) {
            if ((u32)D_005F6708[show] >= 0x22) {
                D_005F6708[show] = 0;
            }
        }
        func_003A6BD0();
        func_00346600();
        func_00343F88();
        gSubMode = 1;
    case 1:
        gSubMode = 2;
    case 2:
        D_005F6678 = 0;
        s->unk0 = g->unk0;
        D_005F6EB8 = 1;
        D_005F6EBC = 1;
        D_005F5CB4 = 1;
        D_005F62A8[0] = 0;
        D_005F62AA = 0;
        func_003C9778(s);
        s->unk4 = 0;
        s->unk10 = 0x12;
        s->unk1C = 0;
        func_003AE7C8();
        func_003C94F0();
        if (D_0043B7F8.unk8 == 0) {
            func_003CC0E0();
        }
        func_003CB508((s32)g, s);
        show = 0;
        s->unk14[1] = 1;
        gSubMode = 3;
        D_005F5CB4 = 0;
        s->unk14[0] = 1;
        break;
    case 3:
        D_005F5CB4 = 0;
        gSubMode = 4;
        show = 0;
        func_003992C8(0);
        break;
    case 4:
        if (D_0043B7F8.unk8 == 0) {
            if (s->unk1C == 0 && s->unk8 > 0) {
                s->unk8--;
            }
            func_003AEA30(s);
        }
        PORT_DIFF(0x3CA470, func_003CA470, "ii", s, 0); /* TTT_DIFF: the step against the original code */
        PORT_DIFF(0x3CA470, func_003CA470, "ii", s, 1);
        if (D_005F6EAD == 0 && s->unk1C == 0) {
            func_003B0820(s);
        }
        if (D_005F62A8[0] >= D_005F6700[0] && D_005F62AA >= D_005F6704) {
            p = func_003B0458();
            if (p == func_003B04A0() && D_005F6EAA == 0 && D_005F6EB0 == 7) {
                D_005F6EB8 = 1;
                D_005F6EBC = 1;
                D_005F6EB4 = 1;
                if (D_005F6EA4 > 0) {
                    if (D_005F6EA4 < 5) {
                        D_005F5CB4 = 1;
                        if (D_005F6EA4 == 4) {
                            Bgm_SetVolumeFade(0, 5);
                        }
                        D_005F5EE0 = 1;
                        D_005F5F20 = 0x14;
                        s->unk1C = 1;
                    }
                    if (func_003AF448() != 0) {
                        D_005F6EA4--;
                    }
                } else {
                    D_005F66F8 = 1;
                    func_003992C8(1);
                    func_003ADFA8();
                    gSubMode = 5;
                }
            }
        }
        break;
    case 5:
        func_003CB610(g, s);
        gSubMode = g->unk13;
        gMode = g->unk12;
        return 1;
    }
    if (s->unk10 != 0 && --s->unk10 == 0) {
        Bgm_Play(0);
    }
    s->unk4++;
    if (func_0039C778() == 0 && D_005F66F8 == 0) {
        if (D_0043B7F8.unk8 != 0) {
            if (func_003AE670() != 0) {
                func_003ADFB0();
                func_003DB690();
            } else if (func_003AE698() != 0) {
                D_005F6730 = 0;
                func_003C4868();
            }
            D_005F6730 = 0xFFB00;
            Sprite_SetBlend(1, 0);
            func_003BF180(g);
            func_003BECC0(s);
        }
        func_003CCCB8(s, show);
        func_003BF8F8();
    }
    if (D_005F66F8 != 0) {
        func_003ADFA8();
    }
    if (func_003B0340() != 0 && gSubMode >= 4 && D_005F66FA == 0) {
        D_005F6738 = 0x80808080;
        for (i = 0; i < 4; i++) {
            p = func_003B0078(0, i);
            func_003B0D50(i, p, func_003BDF60());
        }
    }
    if (func_003BB938() != 0) {
        func_003BB900(0xB4);
    }
    return 0;
}
#else
INCLUDE_RODATA("asm/nonmatchings/select_results_b", jtbl_005204F0);

INCLUDE_ASM("asm/nonmatchings/select_results_b", Mode_CharSelect);
#endif

void func_003CBE08(void) {
    D_00440CE8.unk10 = 0x3C;
}

s32 func_003CBE18(s32 port) {
    s32 r;

    if (D_005F6EAA != 0) return 6;
    if (func_003B0818() != 0 || Pad_IsMultitap() != 0) {
        r = func_003AFF08(port);
        if (r < 0) return 7;
        return r + 2;
    }
    return func_003AFF08(port);
}

s32 func_003CBE88(SelState *s) {
    return func_003C98C8(s);
}

s32 func_003CBEA8(void) {
    return D_005F6EB0 == 7;
}

void func_003CBEB8(void) {
    D_005F6EB0 |= 4;
}

s32 func_003CBEC8(void) {
    return D_005F6EB0 & 4;
}

/* NON_MATCHING: 8 lines, the loop's first D_005F6EAC load is moved into the back-branch delay slot (original: nop) */
#if 0
s32 func_003CBED8(s32 port) {
    s32 r = 0;
    s32 i, n, k;

    if (D_005F6EAA == 0) {
        r = D_00415718[func_003AFF08(port & 1)];
    } else {
        n = func_003B01B0(D_005F6EAC) + 1;
        for (i = 0; i < n; i++) {
            k = func_003AFFB8(D_005F6EAC, i);
            if (k != -1) r |= D_00415748[k];
            else r = 0;
        }
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003CBED8);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520508);

s32 func_003CBFA0(s32 i) {
    return D_00520508[i];
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_005205A8);

void func_003CBFB8(s32 kind, s32 x, s32 y, s32 sz) {
    WordPair2 t = D_005205A8;
    s32 w = t.e[sz].a;

    if (kind == 0x36) Sprite_DrawScaled(1, 0xA5, 0xA3, x, y, w, w);
    else if (kind == 0x37) Sprite_DrawScaled(1, 0xA2, 0xA3, x, y, w, w);
    else Sprite_DrawScaled(1, D_00520508[kind], 0x84, x, y, w, w);
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_005205B8);

void func_003CC078(s32 x, s32 y, s32 i) {
    WordPair2 t = D_005205B8;
    s32 w = t.e[i].a;

    Sprite_DrawScaled(1, 0xAE, 0x84, x - 2, y - 2, w, w);
}

void func_003CC0E0(void) {
    s32 i;

    func_003CF608(1);
    func_003CEA80(2, func_003B8830(), 0x249F0);
    if (D_0043B7F8.unk8 == 0) {
        func_003CEB68(0xF2);
        func_003CEC98(0xF3);
        func_003CD7E8(1, 0xF2);
        func_003CD7E8(1, 0xF3);
        for (i = 0xC5; i < 0xF2; i++) func_003CEB68(i);
        func_003CEC98(0xC4);
        func_003CD7E8(1, 0xC4);
    }
    Sprite_TexFlush(1);
}

void func_003CC190(SelState *s) {
    s32 n = (s->unk8 + s->unkC - 1) / s->unkC - 1;
    s32 save;

    if (n < 0) n = 0;
    if (n >= 7) D_005F6ECC = 0;
    if (n < 6 && (s->unk8 + 1) % 30 == 0) D_005F6ECC = 8;
    if (D_005F6ECC == 0 || s->unk1C != 0) {
        save = D_005F6730;
        D_005F6730 = 0xFFFF0;
        func_003C6738(1, n, 0x140, 0x104);
        D_005F6730 = save;
    } else {
        D_005F6ECC--;
    }
}

void func_003CC260(SelState *s, s32 side, s32 member, s32 chr, s32 cos) {
    s32 x, y, k, save;

    if (D_0043B7F8.unk8 != 0) {
        func_003BEAB0(side, member, chr, cos);
        return;
    }
    if ((u32)(D_0043B7F8.unk0 - 5) < 2 && member != 0) return;
    if (member == 0) {
        if (side == 0) {
            x = 0x8A;
            y = 0x108;
        } else {
            x = 0x178;
            y = 0x108;
        }
    } else {
        if (side == 0) {
            x = 0x29;
            y = 0xEE;
        } else {
            x = 0x1DB;
            y = 0xEE;
        }
    }
    k = func_003CF230(chr, cos);
    if (s->unk14[side] != 0 || member != 1 || func_003CBE88((SelState *)&D_00440D08[side]) != 0) {
        save = D_005F6730;
        Sprite_SetFilter(1, 0);
        Sprite_SetBlend(1, 1);
        D_005F6730 = 0xFFFF0;
        func_003CF1C0(1, k, x, y);
        Sprite_SetFilter(1, 1);
        D_005F6730 = save;
    }
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_005205C8);

extern const char D_005205C8[];
extern u16 D_005F5CB4;
extern Fighter gFighters[4];
s32 func_003AF2E0(s32 side, s32 n);
s32 func_003AF308(s32 side, s32 n);
s32 func_003AF330(s32 side, s32 n);

void func_003CC3E0(SelState *s) {
    s32 i;
    s32 x;
    s32 a;
    SelSide *d;

    D_005F6738 = 0x80808080;
    for (i = 0, d = &s->side[0]; i < 2; i++, d++) {
        x = i * 320 + 160;
        switch (d->unkC) {
        case 0:
            break;
        case 2:
            if (D_005F5CB4 != 0) {
                break;
            }
            if (func_003BBFD0(i)) {
                if (s->unk1C != 0) {
                    break;
                }
                if (D_0043B7F8.unk2) {
                    func_003BC828(i, x, 0x190, 3);
                }
            }
            if (s->unk1C == 0 && !D_0043B7F8.unk2) {
                func_003BBD88(i, i * 320 | 0x3C, 0x168);
            }
            break;
        case 1:
            if (D_005F62A8[i] >= 2) {
                a = func_003AF2E0(i, 1);
                func_003CC260(s, i, 1, a, func_003AF308(i, 1));
            } else {
                if (D_005F62A8[i] <= 0) {
                    goto none;
                }
                if (d->unk10 != func_003AF2E0(i, 0) || D_0043B7F8.unk8) {
                    func_003CC260(s, i, 1, d->unk10, d->unk14);
                }
            }
            a = func_003AF2E0(i, 0);
            func_003CC260(s, i, 0, a, func_003AF308(i, 0));
            break;
        none:
            if (func_003CBE88((SelState *)d) != 0 || func_003AF330(i, 0) < 0 ||
                gFighters[i].unk26 != func_003AF330(i, 0)) {
                func_003CC260(s, i, 0, d->unk10, d->unk14);
            } else {
                a = func_003AF330(i, 0) >> 3;
                func_003CC260(s, i, 0, a, func_003AF330(i, 0) & 7);
            }
            if (D_0043B7F8.unk0 != 5 && !D_0043B7F8.unk8) {
                Text_Printf(D_005205C8, i == 0 ? 0x3E : 0x1E0, 0x15E, 10, 5, 0x172);
            }
            break;
        }
    }
}

void func_003BE038(s32 x, s32 y);

/* NON_MATCHING: 116 lines, structure only: the loop invariants spilled to the stack and the colour register differ */
#if 0
void func_003CC6C8(SelState *s) {
    s32 i;
    s32 j;
    s32 k;
    s32 sel;
    s32 c;
    s32 id;
    s16 x;
    s16 y;
    s32 yo;
    s32 f;
    Unk440A68 *e;

    f = D_0043B7F8.unk8;
    yo = f ? 0x34 : 0;
    sel = 0;
    for (i = 0, e = D_00440A68; i < 36; i++, e++) {
        if (e->unk5 == 0) {
            continue;
        }
        if (f) {
            D_005F6738 = 0x80808080;
            func_003BE038(e->unk8, e->unkA + yo);
        }
        id = e->unk6;
        x = e->unk8;
        y = e->unkA;
        c = 0x80;
        for (j = 0; j < 2; j++) {
            if ((D_0043B7F8.unk0 == 1 || D_0043B7F8.unk0 == 6) && s->side[j].unk0 == 3) {
                continue;
            }
            if (i == s->side[j].unk4 && s->side[j].unk8 == 1) {
                sel = j;
                c = 0xA0;
                for (k = 0; k < 8; k++) {
                    if (((SelSlotsM *)&D_00440C28[sel])->m[k].a == id) {
                        c = 0x30;
                    }
                }
            }
        }
        if (c != 0x80 && id == 0x37) {
            id = D_005F6708[sel];
        }
        D_005F6738 = c | 0x80000000 | ((c << 16) | (c << 8));
        func_003CBFB8(id == 15 ? 0x24 : id, x, y + yo, 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003CC6C8);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_005205D8);

void func_003CC930(s32 side, s32 x, s32 y) {
    s32 c = side == 0 ? 0x78 : 0x7A;
    s32 k = func_003CBE18(side);

    if (k != 7) Sprite_Draw(1, D_005205D8[k].tex, c, x + D_005205D8[k].x[side], y + 0x28);
}

extern s32 D_005F6710[2];
extern s32 D_005F6718[2];

/* NON_MATCHING: 142 lines, the original keeps &D_005F6710[side] in s2 from the loop top and 1 in fp; here 6 is hoisted and the blink update is laid out differently */
#if 0
void func_003CC9E0(SelState *s) {
    s32 side;
    s32 yo;
    s32 x;
    s32 y;
    u64 c;
    SelSide *d;
    Unk440A68 *e;

    yo = D_0043B7F8.unk8 ? 0x34 : 0;
    for (side = 1, d = &s->side[1]; side >= 0; side--, d--) {
        if ((D_0043B7F8.unk0 == 1 || D_0043B7F8.unk0 == 6) && d->unk0 == 3) {
            continue;
        }
        if (s->unk1C == 0) {
            D_005F6710[side] += D_005F6718[side];
            if (D_005F6710[side] > 0xF0) {
                D_005F6718[side] = -6;
            }
            if (D_005F6710[side] < 0x60) {
                D_005F6718[side] = 6;
            }
        }
        e = &D_00440A68[d->unk4];
        if (e->unk5 == 0) {
            continue;
        }
        switch (d->unk8) {
        case 0:
        case 2:
            continue;
        }
        y = D_00440A68[d->unk4].unkA - 1;
        x = D_00440A68[d->unk4].unk8 - 1;
        Sprite_SetFilter(1, 0);
        y += yo;
        Sprite_SetBlend(1, 0);
        c = D_005F6710[side];
        D_005F6738 = c | 0x80000000 | ((c << 16) | (c << 8));
        if (side == 0) {
            if (s->side[0].unk4 == s->side[1].unk4 && s->side[1].unk8 == 1) {
                Sprite_SetTexture(0x73, 0x74);
                D_00440F20.w = 0x37;
                D_00440F20.h = 0x35;
                D_00440F20.unk30 = D_005F6710[side];
                D_00440F20.x = x;
                D_00440F20.y = y;
                D_00440F20.unk28 = D_005F6710[side];
                D_00440F20.unk2C = 0;
                D_00440F20.unk34 = 0;
                Sprite_DrawCurrentGouraud(1);
                c = D_005F6710[side];
                D_005F6738 = c | 0x80000000 | ((c << 16) | (c << 8));
            } else {
                Sprite_DrawScaled(1, 0x73, 0x74, x, y, 0x37, 0x35);
            }
            func_003CC930(0, x, y);
        } else {
            Sprite_DrawScaled(1, 0x73, 0x75, x, y, 0x37, 0x35);
            func_003CC930(1, x, y);
        }
        Sprite_SetFilter(1, 1);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003CC9E0);
#endif

void func_003CCCB8(SelState *s, s32 show) {
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 1);
    if (show == 0) return;
    if (D_0043B7F8.unk8 == 0) {
        func_003CD7E8(0, D_005F6720);
        Sprite_TexFlush(0);
        Sprite_SetBlend(0, 6);
        D_005F6738 = 0x80707070;
        Sprite_DrawScaled(0, D_005F6720, 0xC4, 0, 0xB4, 0x280, 0x12C);
        if (D_005F6720 > 0xF0) D_005F6720 = 0xC5;
        else if (s->unk1C == 0) D_005F6720++;
        Sprite_SetBlend(0, 0);
        D_005F6738 = 0x80808080;
        Sprite_DrawScaledBlended(0, 0xF2, 0xF3, 0, 0, 0x140, 0x135);
        Sprite_DrawScaledBlended(0, 0xF2, 0xF3, 0x27F, 0, -0x140, 0x135);
    } else {
        func_003BBC40();
        func_003BE8C0();
    }
    func_003CC6C8(s);
    func_003CC3E0(s);
    if (D_0043B7F8.unk8 == 0) func_003CC190(s);
    func_003CC9E0(s);
    if (D_0043B7F8.unk0 == 1 || D_0043B7F8.unk0 == 6) {
        func_003BF4B8(&D_0043B7F8, s);
        func_003B6408();
    }
    if (D_0043B7F8.unk0 == 2) func_003BE3E8(&D_0043B7F8, s);
    Sprite_SetFilter(1, 1);
}

INCLUDE_RODATA("asm/nonmatchings/select_results_b", D_00520638);

void func_003CCEA8(s32 x, s32 y, s32 i) {
    s32 save = D_005F6730;

    D_005F6730 = 0xFFF00;
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    Sprite_Draw(1, 0x80, D_00520638[i], x - 0xC, y);
    D_005F6730 = save;
}

void func_003CCF40(s32 x0, s32 x1, s32 y, s32 i) {
    s32 save = D_005F6730;

    D_005F6730 = 0xFFFF0;
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    Sprite_Draw(1, 0x81, D_00520638[i], x0, y);
    Sprite_Draw(1, 0x82, D_00520638[i], x1, y);
    D_005F6730 = save;
}

void func_003CCFF8(void) {
    Unk440E20 *p = D_00440E20;
    s32 i;

    for (i = 0; i < 8; i++, p++) {
        p->unk0 = 0;
        p->unk4 = 0;
        p->unk6 = 0;
        p->unk7 = 0;
    }
}

void func_003CD030(s32 i, s32 chr0, s32 cos0, s32 chr1, s32 cos1, s16 h, s32 w) {
    Unk440E20 *e = &D_00440E20[i];
    s32 j;

    e->unk0 = w;
    e->unk4 = h;
    e->unk6 = func_003CF230(chr0, cos0);
    e->unk7 = func_003CF230(chr1, cos1);
    for (j = 0; j <= i; j++) {
    }
}

s32 func_003CD0D0(void) {
    return D_00440E62[0] & 1;
}

/* NON_MATCHING: 21 lines, block layout of the first check loop and the second search registers */
#if 0
s32 func_003CD0E0(RecSum *out) {
    u32 sum = 0;
    s32 i, j;
    u16 h = D_00440D50.sum[7].side;
    u8 c0 = D_00440D50.sum[7].c0;
    u8 c1 = D_00440D50.sum[7].c1;
    RecSum *e = D_00440D50.sum;
    ScoreEnt *s;

    for (i = 0; i < 8; i++, e++) {
        if (h != e->side || c0 != e->c0 || c1 != e->c1) return 0x7B;
        sum += e->score;
    }
    out->side = h;
    out->c1 = c1;
    out->score = sum;
    out->c0 = c0;
    for (i = 0; i < 10; i++) {
        if (sum < D_00440D58[i].score) {
            for (j = 0; j < i; j++) {
                s = &D_00440D58[j];
                if ((c0 == s->c0 && c1 == s->c1) || (c1 == s->c0 && c0 == s->c1)) return 0x7B;
            }
            return i;
        }
    }
    return 0x7B;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results_b", func_003CD0E0);
#endif

s32 func_003CD1E8(s32 t, s32 c0, s32 c1) {
    TimeEnt *e;
    s32 i, j;

    for (i = 0; i < 10; i++) {
        e = &D_00440DD0[i];
        if (e->time < t) {
            for (j = 0; j < i; j++) {
                e = &D_00440DD0[j];
                if ((c0 == e->c0 && c1 == e->c1) || (c1 == e->c0 && c0 == e->c1)) return 0;
            }
            return i + 1;
        }
    }
    return 0;
}

void func_003CD288(void) {
    D_00440E60[0] = 2;
}

