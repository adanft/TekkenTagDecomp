/* 0x3DC9C8-0x3E1C68 (module demo, docs/game_overview.md section 4; docs/systems/modes.md). */
#include "common.h"

#include "fighter.h"

/* A side's practice command text (D_01FC4930, 16 bytes per command-list slot, Fighter.unk22). */
typedef struct {
    /* 0x0 */ u8 *text;
    /* 0x4 */ struct DemoImg *unk4;
    /* 0x8 */ s32 unk8;
    /* 0xC */ s32 unkC;
} PracCmdText;

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
    /* 0x24 */ s32 unk24;
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;
    /* 0x30 */ f32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ f32 unk38;
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ f32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ f32 unk4C;
    /* 0x50 */ f32 unk50;
    /* 0x54 */ f32 unk54;
    /* 0x58 */ f32 unk58;
    /* 0x5C */ f32 unk5C;
    /* 0x60 */ f32 unk60;
    /* 0x64 */ s32 unk64;
    /* 0x68 */ f32 unk68;
    /* 0x6C */ f32 unk6C;
    /* 0x70 */ f32 unk70;
    /* 0x74 */ s32 unk74;
    /* 0x78 */ f32 unk78;
} Demo46AF50;

/* The flying-object state of the menu background (D_01FC4A40). */
typedef struct {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ s16 unk6;
    /* 0x08 */ u8 pad8[0x1C];
    /* 0x24 */ f32 unk24;
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;
    /* 0x30 */ f32 unk30;
    /* 0x34 */ s16 unk34;
    /* 0x36 */ u8 pad36[0xA];
    /* 0x40 */ f32 unk40[3];
} Demo1FC4A40;

/* An image upload packet: the GS registers at +0x10 / +0x20, the image size in qwords at +0x50. */
typedef struct DemoImg {
    /* 0x00 */ u8 pad0[0x10];
    /* 0x10 */ u64 bitbltbuf;
    /* 0x18 */ u64 pad18;
    /* 0x20 */ u64 trxpos;
    /* 0x28 */ u8 pad28[0x28];
    /* 0x50 */ u32 unk50;
} DemoImg;

typedef struct {
    /* 0x0 */ DemoImg *img;
    /* 0x4 */ s32 size;
} DemoImgOut;

/* Two image upload packets per slot (D_01FC4970, 16 bytes; D_01FC49C0 is slot 5). */
typedef struct {
    /* 0x0 */ DemoImgOut a;
    /* 0x8 */ DemoImgOut b;
} DemoTexPair;

typedef f32 Vu0Matrix[4][4] __attribute__((aligned(16)));

/* Six joint indices per fighter slot (D_0046B010, 12 bytes per slot). */
typedef struct {
    /* 0x0 */ s16 j[6];
} DemoJoints;

/* VIF1 packet of the SDK (sceVif1Packet). */
typedef struct {
    /* 0x0 */ u32 *pCurrent;
    /* 0x4 */ u32 *pBase;
    /* 0x8 */ u32 *pDmaTag;
    /* 0xC */ u32 *pVifCode;
    /* 0x10 */ u32 pad10[4];
} Vif1Packet;

/* A GS register value and two (x, y) points (D_00456540, 24 bytes). */
typedef struct {
    /* 0x00 */ u64 reg;
    /* 0x08 */ f32 x0;
    /* 0x0C */ f32 y0;
    /* 0x10 */ f32 x1;
    /* 0x14 */ f32 y1;
} DemoGsEntry;

typedef struct {
    /* 0x00 */ u64 unk0;
    /* 0x08 */ u64 unk8;
    /* 0x10 */ u64 unk10;
    /* 0x18 */ u64 unk18;
} DemoGsPacket;

/* A glyph position and its colours (PracFont_DrawGlyph). */
typedef struct {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ u8 c[4];
} DemoGlyph;

/* The text position of func_003E06A0: the glyph state, then the line start. */
typedef struct {
    /* 0x0 */ DemoGlyph g;
    /* 0x8 */ s16 x0;
    /* 0xA */ s16 y0;
} DemoTextPos;

typedef struct {
    /* 0x0 */ u32 c0;
    /* 0x4 */ u32 c1;
} DemoColors;

/* Arrow tiles (D_00522D00, 2 bytes per direction). */
typedef struct {
    /* 0x0 */ u8 tile;
    /* 0x1 */ u8 flip;
} DemoArrow;

extern Fighter gFighters[];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern DemoJoints D_0046B010[];
extern f32 D_004334C0[4][4];
extern const DemoArrow D_00522D00[];
extern TeamMember D_00440940[2][2];
extern char **D_00415840[];
extern u8 D_0043B7F8[];
extern s32 D_005F6730;
extern u64 D_005F6738;
extern u16 gFrameCount;
extern DemoColors D_004564F0[];
extern const char D_00522BB0[];
extern const char D_00522BC8[];
extern const char D_00522BE0[];
extern const char D_00522BF0[];
extern const char D_00522C00[];
extern const char D_00522C20[];
extern const char D_00522C30[];
extern const char D_00522C40[];
extern DemoTexPair D_01FC4970[];
extern DemoTexPair D_01FC4990[];
extern DemoTexPair D_01FC49C0[];
extern u8 D_004567A0[];
extern DemoGsEntry D_00456540[];
extern DemoGsPacket D_00456780;
extern f32 D_005F6944;
extern f32 D_005F6948;
extern s32 D_01FC4A30[];
extern PracCmdText gPracCmdText[] __asm__("D_01FC4930");
extern u16 D_005F5CB0[2];
extern u16 D_005F6932;
extern u16 D_005F6930;
extern s16 D_005F7060[2];
extern s16 D_005F7064[2];
extern u32 D_005F7068;
extern u8 D_005F6918[];
extern u8 D_005F6920[];
extern u8 D_005F6928[];
extern u16 D_005F6934;
extern s16 D_005F6936;
extern Demo1FC4A40 D_01FC4A40;
extern f32 D_0046AF50[];
extern struct DemoImg *D_0046AF40[];
extern u8 D_00457380[];
extern s8 D_004572E0[];

s32 rand(void);
u32 Spr_Setup(u32 count, s32 reserved);
void func_00343128(u32 spr, void *src, u32 qwc);
u32 Dma_Wait(volatile u32 *chan);
void func_00343178(u32 *addr, s32 qwc);
void sceVif1PkInit(Vif1Packet *pk, u32 base);
void sceVif1PkReset(Vif1Packet *pk);
void sceVif1PkCnt(Vif1Packet *pk, u32 flag);
void sceVif1PkOpenDirectCode(Vif1Packet *pk, s32 i);
void sceVif1PkReserve(Vif1Packet *pk, u32 n);
void sceVif1PkCloseDirectCode(Vif1Packet *pk);
void sceVif1PkTerminate(Vif1Packet *pk);
s32 sceVif1PkSize(Vif1Packet *pk);
s32 Text_Printf(const char *fmt, ...);
void func_003DBCF0(s32 x, s32 y, s32 w, s32 h, s32 z, u32 color);
void func_003BF180(void *g);
s32 func_003CF230(s32 chr, s32 costume);
void func_003CBFB8(s32 kind, s32 x, s32 y, s32 sz);
s32 func_003AFFB8(s32 side, s16 chara);
void func_003DC9C8(s32 side, s32 v);
void func_003DE850(s32 side, struct DemoImg *img);
void func_003DEC20(void);
void func_00349D70(void *m, f32 *v);
void sceVu0ScaleVector(void *d, void *s, f32 k);
#ifdef PORT
/* The PS2 routine takes four arguments, d = a * k + b * (1 - k) (src/geometry_b.c func_0034A184); the matched callers
 * below pass the third ($a2) without naming it (docs/port/README.md, "Leftover argument registers"). */
void func_0034A180(void *d, void *a, void *b, f32 k);
#else
void func_0034A180(void *m, void *r, f32 k);
#endif
void func_0034A710(void *a, void *b);
struct DemoImg *func_00364528(s32 i, u8 *p);
u64 *func_00399520(void);
void func_00399650(u64 *p);
void func_003DD098(void);
#ifdef PORT
void func_003DD8E8(void *m, void *r, void *b, f32 k);
#else
void func_003DD8E8(void *m, void *r, f32 k);
#endif
void func_003DE0D8(u8 *f);
s16 *func_00364390(void *f, s32 ch);
void func_00365718(void *f);
s32 func_003A9698(void);
void func_003DE4E0(u8 *f, u8 *bones, u8 *mats, s32 side, void *view);
s16 *func_00367270(s32 id);
void sceVu0MulMatrix(void *m0, void *m1, void *m2);
void func_003DD788(void *m, s16 *a);
void func_003DE428(void *m, s16 *a);
void func_003DD800(u8 *m, u16 *s, s32 unused);
void Prac_DrawDemoPrompt(s32 side, s32 k);
void PracFont_DrawGlyph(DemoGlyph *g, u32 c, s32 i);
void func_003DEF98(s32 x, s32 y, s32 k);
void func_003DF150(s32 x, s32 y, s32 k);
void func_003DF2F0(s32 x, s32 y, u32 k);
void func_003DF500(s32 x, s32 y, u32 k);
u8 *Str_Next(u8 *s);
u8 *func_003DFA00(u8 *fmt, ...);
char *func_003E06A0(char *s, ...);
void func_003E0460(u32 x, u32 y, u32 w, u32 h, u32 k, u32 col);
void func_003DFF30(s32 a0, s32 a1, s32 a2, s32 a3, u32 col);
void func_003DD918(void *f);
s32 func_003E08D8(f32 *p, f32 *v);
void func_003DECE0(void);
void func_003DED90(void);
void func_00399820(void *p, s32 qwc);
void Sprite_TexFlush(s32 a);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_SetBlend(s32 a, s32 b);
f32 func_0034A570(f32 x);
s32 Text_Get(s32 i);
s32 PracFont_GlyphWidth(s32 c);
s32 PracFont_StrWidth(u8 *s);
struct DemoImg *Gfx_BuildImageUpload(DemoImgOut *out, struct DemoImg *p, u32 spsm, u32 dpsm, u32 dbp, u32 dsax, u32 dsay, u32 w);
s32 func_003E08D8(f32 *p, f32 *v);

INCLUDE_RODATA("asm/nonmatchings/demo", D_00522B90);
#if 0
/* NON_MATCHING: 311 lines: m2c draft in gotos-only form (irreducible flow graph) */
#define M2C_UNK u8
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(s32)(expr))
extern u8 D_01FC4930[];
void Snd_PlaySe(s32, s32);
s32 func_003DB9A0(u8 *);
void func_003DBAA8(s32, s32, s32, s32);
void func_003DC188(s32, s32, s32, s32);
extern u8 D_00415718[];
extern u8 D_00415728[];
extern u8 D_00522B90;
extern u8 D_005F6910;

void func_003DC9C8(s32 arg0, s32 arg1) {
    u8 sp[0x200];
    s32 subroutine_arg0, subroutine_arg1, subroutine_arg2, subroutine_arg3, subroutine_arg4, subroutine_arg5;
    s32 sp10;
    s32 sp14;
    s32 sp18;
    s32 temp_a0;
    s32 temp_a0_2;
    s32 temp_a1;
    s32 temp_a1_2;
    s32 temp_a2;
    s32 temp_fp;
    s32 temp_s0;
    s32 temp_s0_2;
    s32 temp_s0_3;
    s32 temp_s1;
    s32 temp_s3;
    s32 temp_s3_2;
    s32 temp_s3_3;
    s32 temp_s5;
    s32 temp_v0;
    s32 temp_v1_2;
    s32 temp_v1_3;
    s32 var_a0;
    s32 var_a3;
    s32 var_s1;
    s32 var_s1_2;
    s32 var_s3;
    s32 var_s3_2;
    s32 var_s4;
    s32 var_s5;
    s32 var_s5_2;
    s32 var_s5_3;
    s32 var_v0;
    s32 var_v0_2;
    s64 temp_a3;
    u32 temp_lo;
    u32 temp_v1;
    u8 *temp_s2_2;
    u8 *var_s2;
    u8 temp_s6;
    u8 temp_v0_2;
    u8 var_v0_3;
    void *temp_s2;
    void *temp_s7;

    /* Flowgraph is not reducible, falling back to gotos-only mode. */
    temp_s2 = (arg0 * 0xB8A0) + (u8 *)gFighters;
    temp_a1 = arg1 * 4;
    temp_s7 = (M2C_FIELD(temp_s2, s16 *, 0x22) * 0x10) + ((u8 *)&D_01FC4930);
    sp10 = 0x59;
    temp_a0 = ((D_0043B7F8[0] ^ 7) == 0) ? 0x3B : sp10;
    sp10 = temp_a0;
    temp_fp = temp_a0 + 0x32;
    temp_s6 = *M2C_FIELD(temp_s7, u8 **, 0);
    temp_s0 = *(temp_a1 + ((u8 *)&D_00415728));
    temp_s3 = *(temp_a1 + ((u8 *)&D_00415718));
    if (temp_s6 == 0) {
        goto block_58;
    }
    temp_v0 = ((arg0 == 0) ? 0x14 : 0x150) - 8;
    sp14 = temp_v0;
    func_003DBCF0(temp_v0, sp10 - 8, 0x12C, 0x3AU, 0, 0);
    temp_a0_2 = temp_s6 < 5U;
    sp18 = temp_a0_2;
    func_003DFA00(((u8 *)&D_00522B90), 0x60606080, 0x118, subroutine_arg1);
    func_003DFA00(((u8 *)&D_005F6910), subroutine_arg1 + 0x33, sp10, 0x60606080);
    func_003DFA00(subroutine_arg0 + 1);
    temp_a2 = arg0 * 0x60;
    temp_a3 = (0x8080 << 0x10) | 0x8080;
    D_005F6738 = temp_a3;
    temp_a1_2 = (M2C_FIELD(temp_s2, s16 *, 0) * 0x30) + temp_a2;
    D_005F6730 = 0xFFFF0;
    func_003CBFB8(func_003CF230(*(temp_a1_2 + ((u8 *)&D_00440940)), M2C_FIELD((((u8 *)&D_00440940) + temp_a1_2), s32 *, 4)), subroutine_arg1 - 3, sp10 - 4, 0);
    if (temp_a0_2 != 0) {
        goto block_12;
    }
    var_a3 = 0;
    if (temp_s0 != 0x4000) {
        goto block_4;
    }
    Snd_PlaySe(0x103F, 0);
    var_a3 = 1;
block_4:
    var_v0 = temp_s3 & 0x5000;
    if (temp_s0 != 0x1000) {
        goto block_6;
    }
    Snd_PlaySe(0x103F, 0);
    var_a3 = -1;
    var_v0 = temp_s3 & 0x5000;
block_6:
    if (var_v0 != 0) {
        goto block_11;
    }
    if (temp_s0 != 0x2000) {
        goto block_9;
    }
    Snd_PlaySe(0x103F, 0);
    var_a3 = 4;
    goto block_11;
block_9:
    if (temp_s0 != 0x8000) {
        goto block_11;
    }
    Snd_PlaySe(0x103F, 0);
    var_a3 = -4;
block_11:
    temp_v1 = M2C_FIELD(temp_s7, u32 *, 8);
    M2C_FIELD(temp_s7, s32 *, 0xC) = (s32) (M2C_FIELD(temp_s7, s32 *, 0xC) + (var_a3 * 0x48));
    temp_v1_2 = ((temp_v1 < temp_s6) * temp_v1) + var_a3;
    temp_v1_3 = (temp_v1_2 <= -1) ? (temp_v1_2 + temp_s6) : temp_v1_2;
    M2C_FIELD(temp_s7, u32 *, 8) = (u32) (((u32) temp_v1_3 >= temp_s6) ? (temp_v1_3 - temp_s6) : temp_v1_3);
block_12:
    var_s3 = M2C_FIELD(temp_s7, s32 *, 0xC);
    var_s5 = 0;
    if (var_s3 == 0) {
        goto block_18;
    }
    var_a0 = var_s3 + 3;
    if (var_s3 >= 0) {
        goto block_15;
    }
    var_s5 = 1;
    var_s3 = -var_s3;
    var_a0 = var_s3 + 3;
block_15:
    if (3 != 0) {
        goto block_17;
    }
    /* break 7 */;
block_17:
    temp_s3_2 = var_s3 - (var_a0 / 3);
    temp_s3_3 = (temp_s3_2 <= 0) ? 0 : temp_s3_2;
    var_s3 = (var_s5 != 0) ? -temp_s3_3 : temp_s3_3;
block_18:
    M2C_FIELD(temp_s7, s32 *, 0xC) = var_s3;
    temp_s5 = (var_s3 / 72) + (var_s3 > 0);
    var_s4 = M2C_FIELD(temp_s7, u32 *, 8) - temp_s5;
    var_s3_2 = var_s3 + (temp_fp - (temp_s5 * 0x48));
    if (var_s4 >= 0) {
        goto block_22;
    }
    var_s1 = temp_fp + 0x120;
loop_20:
    var_s4 += temp_s6;
    if (var_s4 < 0) {
        goto loop_20;
    }
    var_v0_2 = (u32) var_s4 < temp_s6;
    goto block_24;
block_22:
    var_s1 = temp_fp + 0x120;
block_23:
    var_v0_2 = (u32) var_s4 < temp_s6;
block_24:
    if (var_v0_2 != 0) {
        goto block_26;
    }
    var_s4 -= temp_s6;
    goto block_23;
block_26:
    var_s2 = subroutine_arg0;
    temp_s0_2 = var_s4 * 2;
    var_s5_2 = 0;
    if (temp_s0_2 <= 0) {
        goto block_35;
    }
    var_v0_3 = *var_s2;
loop_29:
    if (var_v0_3 != 0xFC) {
        goto block_32;
    }
    if ((u8) M2C_FIELD(var_s2, u8 *, 1) >= 0xFEU) {
        goto block_32;
    }
block_32:
    var_s5_2 += 1;
    var_s2 = Str_Next(var_s2);
    if (var_s5_2 >= temp_s0_2) {
        goto block_34;
    }
    var_v0_3 = *var_s2;
    goto loop_29;
block_34:
block_35:
    func_003DBAA8(0, 0x27F, temp_fp, var_s1);
    if (sp18 == 0) {
        goto block_37;
    }
    goto block_44;
block_37:
    temp_lo = 0x480U / temp_s6;
    var_s1_2 = ((M2C_FIELD(temp_s7, u32 *, 8) * 0x48) - M2C_FIELD(temp_s7, s32 *, 0xC)) * 4;
    if (var_s1_2 == 0) {
        goto block_43;
    }
    if ((s32) temp_s6 < 0) {
        goto block_41;
    }
    goto block_42;
block_41:
block_42:
    var_s1_2 = M2C_BITWISE(s32, (f32) var_s1_2);
block_43:
    temp_s0_3 = subroutine_arg1 + 0x11E;
    temp_s1 = temp_fp + var_s1_2;
    func_003DBCF0(temp_s0_3, temp_fp, 6, 0x120U, 0, 0);
    func_003DBCF0(temp_s0_3, temp_s1, 6, temp_lo, 0, 0);
    func_003DBCF0(temp_s0_3, temp_s1 - 0x120, 6, temp_lo, 0, 0);
    goto block_45;
block_44:
    func_003DBCF0(sp14, (temp_s6 * 0x48) + var_s3_2, 0x12C, (4 - temp_s6) * 0x48, 0, 0);
block_45:
    var_s5_3 = 0;
    if (subroutine_arg2 == 0) {
        goto block_56;
    }
loop_47:
    if (M2C_FIELD(var_s2, u8 *, 0) != 0xFC) {
        goto block_50;
    }
    if ((u8) M2C_FIELD(var_s2, u8 *, 1) >= 0xFEU) {
        goto block_50;
    }
    temp_v0_2 = M2C_FIELD(var_s2, u8 *, 1);
    var_s2 += 2;
block_50:
    if (func_003DB9A0(var_s2) <= 0) {
        goto block_52;
    }
    func_003DBCF0(sp14, var_s3_2, 0x12C, 0x12U, 0, 0);
    func_003DBCF0(sp14, var_s3_2 + 0x12, 0x12C, 0x36U, 0, 0);
    goto block_53;
block_52:
    func_003DBCF0(sp14, var_s3_2, 0x12C, 0x48U, 0, 0);
block_53:
    var_s4 += 1;
    func_003DFA00(((u8 *)&D_005F6918), subroutine_arg1, var_s3_2);
    temp_s2_2 = func_003DFA00(var_s2);
    func_003DFA00(((u8 *)&D_005F6918), subroutine_arg1 + 2, var_s3_2 + 0x18);
    var_s2 = func_003DFA00(temp_s2_2);
    if ((u32) var_s4 < temp_s6) {
        goto block_55;
    }
    var_s2 = subroutine_arg0;
    var_s4 = 0;
block_55:
    var_s5_3 += 1;
    var_s3_2 += 0x48;
    if (var_s5_3 < subroutine_arg2) {
        goto loop_47;
    }
block_56:
    func_003DBAA8(0, 0x27F, 0, 0x1BF);
    func_003DC188(subroutine_arg1, sp10, 0x11C, 0x14A);
    return;
block_58:
    return;
}
#undef M2C_UNK
#undef M2C_FIELD
#undef M2C_BITWISE
#else
INCLUDE_ASM("asm/nonmatchings/demo", func_003DC9C8);
#endif

/* Re-sends both sides' command lists after a load (func_003DE850, func_003DC9C8) [I]. */
void func_003DD098(void) {
    s32 i;

    i = 0;
    func_003DEC20();
    do {
        if (D_005F5CB0[i] != 0) {
            Fighter *f = &gFighters[i];

            func_003DE850(i, gPracCmdText[f->unk22].unk4);
            func_003DC9C8(i, func_003AFFB8(i, f->unk0));
        }
        i++;
    } while (i < 2);
}

void func_003DD158(s32 side) {
    func_003DE850(side, gPracCmdText[gFighters[side].unk22].unk4);
}

/* Draws entry n of side `side`'s command list in a two-row window at x, y (colour from a 0xFC n byte of the
 * text, D_004564F0) and prints its name and command with func_003DFA00; `e` is the second row's argument. */
void func_003DD188(s32 x, s32 y, s32 side, s32 n, s32 e) {
    u8 *t;
    u8 *s;
    s32 col;
    s32 i;

    col = 0;
    t = gPracCmdText[gFighters[side].unk22].text;
    if (t[0] < n) {
        return;
    }
    s = t + 1;
    for (i = 0; i < n * 2 + 1; i++) {
        if (s[0] == 0xFC && s[1] < 0xFE) {
            col = s[1] - 1;
        }
        s = Str_Next(s);
    }
    if (s[0] == 0xFC && s[1] < 0xFE) {
        col = s[1] - 1;
        s += 2;
    }
    func_003DBCF0(x, y, 0x12C, 0x12, 0xFFFE, (D_004564F0[col].c0 & 0xFFFFFF00) | 0x60);
    func_003DBCF0(x, y + 0x12, 0x12C, 0x2A, 0xFFFE, (D_004564F0[col].c1 & 0xFFFFFF00) | 0x60);
    func_003DFA00(D_005F6920, 0x73737380, 0x118);
    func_003DFA00(D_005F6918, x + 8, y + 1);
    s = func_003DFA00(s);
    func_003DFA00(D_005F6928, x + 8, y + 0x15, e);
    func_003DFA00(s);
}

INCLUDE_RODATA("asm/nonmatchings/demo", D_00522BB0);
INCLUDE_RODATA("asm/nonmatchings/demo", D_00522BC8);
INCLUDE_RODATA("asm/nonmatchings/demo", D_00522BE0);
INCLUDE_RODATA("asm/nonmatchings/demo", D_00522BF0);
/* The practice command-demo prompt of side `side`: "x=DEMO o=EXIT" bar (k < 4), the "DEMO" label (even k >= 4),
 * and the two flashing frames of the command window unless bit 1 of k is set [V strings]. */
void Prac_DrawDemoPrompt(s32 side, s32 k) {
    s32 x;
    s32 a;
    s32 t;

    x = 0x150;
    if (side == 0) {
        x = 0x14;
    }
    if (k < 4) {
        func_003DBCF0(0, 0x19A, 0x280, 0xA, 0xFFFF, 0x18181860);
        if (k == 0) {
            Text_Printf(D_00522BB0, x + 0x1E, 0x1AF, 0xB, 8, 6, 8, 6);
        }
        Text_Printf(D_00522BC8, x + 0x96, 0x1AF, 0xB, 8, 6, 8, 6);
    } else if (!(k & 1)) {
        Text_Printf(D_00522BE0, x + 0x3F, 0x6E, 0xB, 8);
        Text_Printf(D_00522BF0, x + 0x57, 0x6E, 9, 6);
    }
    if (!(k & 2)) {
        t = gFrameCount * 2;
        a = t & 0x3F;
        if (t & 0x20) {
            a = 0x3F - a;
        }
        k = (k & 1) + 7;
        a += 0x60;
        func_003DBCF0(x - 5, 0x6D, 0x129, 0x12, 0xFFFF, D_004564F0[k].c0 | (a & 0xFF));
        func_003DBCF0(x - 5, 0x7F, 0x129, 0x36, 0xFFFF, D_004564F0[k].c1 | (a & 0xFF));
    }
}

s32 func_003DD580(s32 slot) {
    return gPracCmdText[slot].unk8;
}

u8 func_003DD598(s32 slot) {
    return gPracCmdText[slot].text[0];
}

INCLUDE_RODATA("asm/nonmatchings/demo", D_00522C00);
INCLUDE_RODATA("asm/nonmatchings/demo", D_00522C20);
INCLUDE_RODATA("asm/nonmatchings/demo", D_00522C30);
INCLUDE_RODATA("asm/nonmatchings/demo", D_00522C40);
/* "CONGRATULATIONS!" screen of a cleared practice command: portrait, the character's name centred, the time
 * t in frames as minutes'seconds"hundredths (at most 100 minutes) [V strings]. */
void Prac_DrawCongratulations(Fighter *f, s32 t) {
    s32 s;
    s32 side;
    s32 cos;
    s32 n;

    side = f->unk1E;
    cos = f->unk26;
    D_005F6730 = 0xFFF00;
    func_003BF180(D_0043B7F8);
    D_005F6730 = 0xFFFF0;
    D_005F6738 = 0x80808080;
    func_003CBFB8(func_003CF230(D_00440940[side][gFighters[side].unk0].unk0, D_00440940[side][gFighters[side].unk0].unk4), 0x126, 0xAE, 0);
    Text_Printf(D_00522C00, 0x32, 0x64, 0xA, 8, 0xB);
    n = 0x280 - Text_Printf(D_00522C20, 0xA, 6, *D_00415840[cos]);
    Text_Printf(D_00522C30, n / 2, 0xFA, 0xA, 6, 0x13, *D_00415840[cos]);
    if (t > 359999) {
        t = 360000;
    }
    s = t / 60;
    Text_Printf(D_00522C40, 0xDC, 0x140, 0xA, 6, 0x17, s / 60, s % 60, t % 60 * 100 / 60);
}

/* Builds a rotation from three s16 angles (x and y negated) and hands it to func_00349D70. */
void func_003DD788(void *m, s16 *a) {
    f32 v[4];

    v[0] = (f32)(s16)-a[0] * (3.141593f / 32768.0f);
    v[1] = (f32)(s16)-a[1] * (3.141593f / 32768.0f);
    v[2] = (f32)a[2] * (3.141593f / 32768.0f);
    func_00349D70(m, v);
}

/* Scales the three rows of m by the 4.12 fixed-point factors s[0..2]. */
void func_003DD800(u8 *m, u16 *s, s32 unused) {
    sceVu0ScaleVector(m, m, s[0] * (1.0f / 4096.0f));
    sceVu0ScaleVector(m + 0x10, m + 0x10, s[1] * (1.0f / 4096.0f));
    sceVu0ScaleVector(m + 0x20, m + 0x20, s[2] * (1.0f / 4096.0f));
}

/* m's translation row = v * k / 40960. */
void func_003DD890(u8 *m, s16 *v, s32 k) {
    f32 fk = k;

    *(f32 *)(m + 0x30) = v[0] * fk * (1.0f / 40960.0f);
    *(f32 *)(m + 0x34) = v[1] * fk * (1.0f / 40960.0f);
    *(f32 *)(m + 0x38) = v[2] * fk * (1.0f / 40960.0f);
}

#ifdef PORT
/* $a2 (b) comes from the caller untouched: the blend partner of m (m itself, or the previous frame's matrix). */
void func_003DD8E8(void *m, void *r, void *b, f32 k) {
    func_0034A180(m, r, b, k);
    func_0034A710(m, m);
}
#else
void func_003DD8E8(void *m, void *r, f32 k) {
    func_0034A180(m, r, k);
    func_0034A710(m, m);
}
#endif

#if 0
/* NON_MATCHING: 494 lines: m2c draft (float first arguments reordered by hand) */
#define M2C_UNK u8
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(s32)(expr))
#define s128 u128
void Mtx_Unit(void *);
void *func_00364298(void *);
s32 func_003B87B0(s16);
s32 func_003B87F8(s16);
void sceVu0InterVector(void *d, void *a, void *b, f32 t);
s32 sceVu0MulVector(void *, void *, f32 *);
extern u8 D_0046AFD0[];

void func_003DD918(void *arg0) {
    u8 sp[0x200];
    s32 subroutine_arg0, subroutine_arg1, subroutine_arg2, subroutine_arg3, subroutine_arg4, subroutine_arg5;
    f32 sp30;
    f32 sp40;
    f32 sp44;
    f32 sp48;
    f32 sp4C;
    f32 *temp_a0_3;
    f32 *temp_a0_4;
    f32 temp_f1;
    f32 temp_f1_2;
    f32 temp_f1_3;
    f32 temp_f22;
    s32 temp_a0;
    s32 temp_fp;
    s32 temp_s0;
    s32 var_s7;
    s32 var_s7_2;
    s32 var_v0;
    u32 var_v1;
    void *temp_a0_2;
    void *temp_a2;
    void *temp_s2;
    void *temp_s2_2;
    void *temp_s3;
    void *temp_s3_2;
    void *temp_v0;
    void *temp_v0_2;
    void *temp_v1;
    void *var_a1;
    void *var_a1_2;
    void *var_s4;
    void *var_s4_2;

    temp_fp = func_003B87F8(M2C_FIELD(arg0, s16 *, 0x20));
    if ((D_005F5EE0 == 0) && (D_005F5EE4 == 0) && (func_003A9698() == 0)) {
        Mtx_Unit(sp);
        temp_f22 = M2C_FIELD(arg0, f32 *, 0xB090);
        temp_v0 = func_00364298(arg0);
        if (temp_v0 == NULL) {
            var_s7 = 7;
            temp_s0 = func_003B87B0(M2C_FIELD(arg0, s16 *, 0x22)) + 0xC;
            var_s4 = arg0 + 0x8990;
            var_v0 = 7 * 4;
            do {
                temp_a2 = *(s32 *)(temp_s0 + var_v0);
                temp_v1 = arg0 + (var_s7 << 6) + 0x8000;
                temp_a0 = var_s7 * 0xC;
                var_s7 += 1;
                M2C_FIELD(temp_v1, f32 *, 0x800) = (f32) M2C_FIELD(temp_a2, f32 *, 4);
                temp_v0_2 = temp_a0 + temp_fp;
                temp_a0_2 = var_s4;
                var_s4 += 0x40;
                M2C_FIELD(temp_v1, f32 *, 0x804) = (f32) M2C_FIELD(temp_a2, f32 *, 8);
                M2C_FIELD(temp_v1, f32 *, 0x808) = (f32) M2C_FIELD(temp_a2, f32 *, 0xC);
                M2C_FIELD(temp_v0_2, f32 *, 0) = (f32) M2C_FIELD(temp_a2, f32 *, 0x10);
                M2C_FIELD(temp_v0_2, f32 *, 4) = (f32) M2C_FIELD(temp_a2, f32 *, 0x14);
                M2C_FIELD(temp_v0_2, f32 *, 8) = (f32) M2C_FIELD(temp_a2, f32 *, 0x18);
                func_00349D70(temp_a0_2, temp_v0_2);
                var_v0 = var_s7 * 4;
            } while (var_s7 < 0x1D);
        } else {
            var_s7_2 = 7;
            var_s4_2 = arg0 + 0x8990;
            var_v1 = 0;
            do {
                switch (var_v1) {
                case 0:
                    var_a1 = temp_v0;
block_18:
                    var_s7_2 += 1;
                    func_003DD788(sp, var_a1);
                    func_003DD8E8(var_s4_2, sp, var_s4_2, temp_f22);
                    break;
                case 3:
                    func_00349D70(sp, (var_s7_2 * 0xC) + temp_fp);
                    func_003DD800(sp, temp_v0 + 6, M2C_FIELD(arg0, s32 *, 0x500));
                    var_a1_2 = temp_v0 + 0xC;
block_32:
                    func_003DD890(sp, var_a1_2, M2C_FIELD(arg0, s32 *, 0x500));
                    func_0034A180(var_s4_2, sp, var_s4_2, temp_f22);
block_33:
                    temp_a0_3 = var_s4_2 + 0x30;
                    sceVu0InterVector(temp_a0_3, &sp30, temp_a0_3, temp_f22);
                default:
block_35:
                    var_s7_2 += 1;
                    break;
                case 6:
                    func_003DD788(sp, temp_v0 + 0x1E);
                    temp_s3 = var_s4_2 + 0x10;
                    func_003DD8E8(var_s4_2, sp, arg0 + 0xB0A0, temp_f22);
                    temp_s2 = var_s4_2 + 0x20;
                    M2C_FIELD(arg0, s128 *, 0xB0A0) = (s128) M2C_FIELD(var_s4_2, s128 *, 0);
                    M2C_FIELD(arg0, s128 *, 0xB0B0) = (s128) M2C_FIELD(var_s4_2, s128 *, 0x10);
                    M2C_FIELD(arg0, s128 *, 0xB0C0) = (s128) M2C_FIELD(var_s4_2, s128 *, 0x20);
                    temp_f1 = (f32) M2C_FIELD(arg0, s32 *, 0x500);
                    sp40 = (f32) M2C_FIELD(temp_v0, u16 *, 0x12) * 0.00024414062f * temp_f1 * 0.00024414062f;
                    sp44 = (f32) M2C_FIELD(temp_v0, u16 *, 0x14) * 0.00024414062f * temp_f1 * 0.00024414062f;
                    sp4C = 1.0f;
                    sp48 = (f32) M2C_FIELD(temp_v0, u16 *, 0x16) * 0.00024414062f * temp_f1 * 0.00024414062f;
                    sceVu0InterVector(&sp40, &sp40, arg0 + 0xB0D0, temp_f22);
                    M2C_FIELD(arg0, f32 *, 0xB0D0) = sp40;
                    sceVu0MulVector(var_s4_2, var_s4_2, &sp40);
                    sceVu0MulVector(temp_s3, temp_s3, &sp40);
                    sceVu0MulVector(temp_s2, temp_s2, &sp40);
                    goto block_35;
                case 7:
                    func_003DD788(sp, temp_v0 + 0x24);
                    func_003DD8E8(var_s4_2, sp, arg0 + 0xB0E0, temp_f22);
                    M2C_FIELD(arg0, s128 *, 0xB0E0) = (s128) M2C_FIELD(var_s4_2, s128 *, 0);
                    M2C_FIELD(arg0, s128 *, 0xB0F0) = (s128) M2C_FIELD(var_s4_2, s128 *, 0x10);
                    temp_s2_2 = var_s4_2 + 0x10;
                    temp_s3_2 = var_s4_2 + 0x20;
                    M2C_FIELD(arg0, s128 *, 0xB100) = (s128) M2C_FIELD(var_s4_2, s128 *, 0x20);
                    temp_f1_2 = (f32) M2C_FIELD(arg0, s32 *, 0x500);
                    sp40 = (f32) M2C_FIELD(temp_v0, u16 *, 0x18) * 0.00024414062f * temp_f1_2 * 0.00024414062f;
                    sp44 = (f32) M2C_FIELD(temp_v0, u16 *, 0x1A) * 0.00024414062f * temp_f1_2 * 0.00024414062f;
                    sp4C = 1.0f;
                    sp48 = (f32) M2C_FIELD(temp_v0, u16 *, 0x1C) * 0.00024414062f * temp_f1_2 * 0.00024414062f;
                    sceVu0InterVector(&sp40, &sp40, arg0 + 0xB110, temp_f22);
                    M2C_FIELD(arg0, f32 *, 0xB110) = sp40;
                    sceVu0MulVector(var_s4_2, var_s4_2, &sp40);
                    sceVu0MulVector(temp_s2_2, temp_s2_2, &sp40);
                    sceVu0MulVector(temp_s3_2, temp_s3_2, &sp40);
                    temp_f1_3 = (f32) M2C_FIELD(arg0, s32 *, 0x500);
                    sp40 = (f32) M2C_FIELD(temp_v0, u16 *, 0x12) * 0.00024414062f * temp_f1_3 * 0.00024414062f;
                    sp44 = (f32) M2C_FIELD(temp_v0, u16 *, 0x14) * 0.00024414062f * temp_f1_3 * 0.00024414062f;
                    sp48 = (f32) M2C_FIELD(temp_v0, u16 *, 0x16) * 0.00024414062f * temp_f1_3 * 0.00024414062f;
                    sceVu0InterVector(&sp40, &sp40, arg0 + 0xB0D0, temp_f22);
                    sceVu0MulVector(var_s4_2 + 0x30, (M2C_FIELD(arg0, s16 *, 0x20) * 0x10) + ((u8 *)&D_0046AFD0), &sp40);
                    M2C_FIELD(var_s4_2, f32 *, 0x3C) = 1.0f;
                    goto block_35;
                case 14:
                    var_a1 = temp_v0 + 0x2A;
                    goto block_18;
                case 15:
                    var_a1 = temp_v0 + 0x30;
                    goto block_18;
                case 18:
                    var_a1 = temp_v0 + 0x36;
                    goto block_18;
                case 19:
                    var_a1 = temp_v0 + 0x3C;
                    goto block_18;
                case 16:
                    func_003DD890(sp, temp_v0 + 0x42, M2C_FIELD(arg0, s32 *, 0x500));
                    goto block_33;
                case 17:
                    func_003DD890(sp, temp_v0 + 0x48, M2C_FIELD(arg0, s32 *, 0x500));
                    goto block_33;
                case 12:
                    func_003DD890(sp, temp_v0 + 0x4E, M2C_FIELD(arg0, s32 *, 0x500));
                    goto block_33;
                case 13:
                    func_003DD890(sp, temp_v0 + 0x54, M2C_FIELD(arg0, s32 *, 0x500));
                    goto block_33;
                case 20:
                    func_003DD890(sp, temp_v0 + 0x5A, M2C_FIELD(arg0, s32 *, 0x500));
                    goto block_33;
                case 21:
                    func_003DD890(sp, temp_v0 + 0x60, M2C_FIELD(arg0, s32 *, 0x500));
                    goto block_33;
                case 10:
                    func_003DD788(sp, temp_v0 + 0x72);
                    func_003DD890(sp, temp_v0 + 0x66, M2C_FIELD(arg0, s32 *, 0x500));
                    func_003DD8E8(var_s4_2, sp, var_s4_2, temp_f22);
                    goto block_33;
                case 11:
                    func_003DD788(sp, temp_v0 + 0x78);
                    func_003DD890(sp, temp_v0 + 0x6C, M2C_FIELD(arg0, s32 *, 0x500));
                    func_003DD8E8(var_s4_2, sp, var_s4_2, temp_f22);
                    goto block_33;
                case 8:
                    func_00349D70(sp, (var_s7_2 * 0xC) + temp_fp);
                    func_003DD800(sp, temp_v0 + 0x7E, M2C_FIELD(arg0, s32 *, 0x500));
                    var_a1_2 = temp_v0 + 0x8A;
                    goto block_32;
                case 9:
                    func_00349D70(sp, (var_s7_2 * 0xC) + temp_fp);
                    func_003DD800(sp, temp_v0 + 0x84, M2C_FIELD(arg0, s32 *, 0x500));
                    var_a1_2 = temp_v0 + 0x90;
                    goto block_32;
                case 4:
                    func_00349D70(sp, (var_s7_2 * 0xC) + temp_fp);
                    func_003DD800(sp, temp_v0 + 0x96, M2C_FIELD(arg0, s32 *, 0x500));
                    var_a1_2 = temp_v0 + 0xA2;
                    goto block_32;
                case 5:
                    func_00349D70(sp, (var_s7_2 * 0xC) + temp_fp);
                    func_003DD800(sp, temp_v0 + 0x9C, M2C_FIELD(arg0, s32 *, 0x500));
                    var_a1_2 = temp_v0 + 0xA8;
                    goto block_32;
                case 1:
                    func_00349D70(sp, (var_s7_2 * 0xC) + temp_fp);
                    func_003DD800(sp, temp_v0 + 0xAE, M2C_FIELD(arg0, s32 *, 0x500));
                    var_a1_2 = temp_v0 + 0xBA;
                    goto block_32;
                case 2:
                    func_00349D70(sp, (var_s7_2 * 0xC) + temp_fp);
                    func_003DD800(sp, temp_v0 + 0xB4, M2C_FIELD(arg0, s32 *, 0x500));
                    func_003DD890(sp, temp_v0 + 0xC0, M2C_FIELD(arg0, s32 *, 0x500));
                    func_0034A180(var_s4_2, sp, var_s4_2, temp_f22);
                    temp_a0_4 = var_s4_2 + 0x30;
                    sceVu0InterVector(temp_a0_4, &sp30, temp_a0_4, temp_f22);
                    goto block_35;
                }
                var_s4_2 += 0x40;
                var_v1 = var_s7_2 - 7;
            } while (var_s7_2 < 0x1D);
        }
    }
}
#undef M2C_UNK
#undef M2C_FIELD
#undef M2C_BITWISE
#undef s128
#else
INCLUDE_RODATA("asm/nonmatchings/demo", jtbl_00522C60);
INCLUDE_ASM("asm/nonmatchings/demo", func_003DD918);
#endif

/* Applies the face / hand animation channels 2 (bones 0x27-0x35) and 1 (bones 0x40-0x4E) of fighter f to its
 * bone matrices (0x40 bytes from +0x8000 + 0x1190), with the blend weights at unkB080 / unkB07C [I]. */
#if 0
/* NON_MATCHING: about 60 lines: the frame pointer is computed inside each case (i * 6 - 0x180) and s1/s2 swapped; jump table and .lit4 words not emitted from C yet */
void func_003DE0D8(u8 *f) {
    Vu0Matrix m;
    f32 v[4];
    s16 *a;
    s16 *p;
    u8 *dst;
    s32 i;

    if (D_005F5EE0 != 0) {
        return;
    }
    if (D_005F5EE4 != 0) {
        return;
    }
    if (func_003A9698() != 0) {
        return;
    }
    func_00365718(f);
    a = func_00364390(f, 2);
    if ((u32)a > 0x10000 && !((u32)a & 1) && (u32)a < 0x8000000) {
        dst = f + 0x9190;
        for (i = 0x27; i < 0x36; i++) {
            if ((u32)(i - 0x27) < 15) {
                func_003DD788(m, (s16 *)((u8 *)a + (i - 0x27) * 6));
                func_003DD8E8(dst, m, dst, *(f32 *)(f + 0xB080));
            }
            dst += 0x40;
        }
    }
    a = func_00364390(f, 1);
    if ((u32)a > 0x10000 && !((u32)a & 1) && (u32)a < 0x8000000) {
        dst = f + 0x97D0;
        for (i = 0x40; i < 0x4F; i++) {
            p = (s16 *)((u8 *)a + i * 6 - 0x180);
            switch (i) {
            case 0x40:
                v[0] = p[0] * (3.141593f / 32768.0f) - 3.1415927f;
                v[1] = (s16)-p[1] * (3.141593f / 32768.0f);
                v[2] = (s16)-p[2] * (3.141593f / 32768.0f);
                func_00349D70(m, v);
                func_003DD8E8(dst, m, dst, *(f32 *)(f + 0xB07C));
                break;
            case 0x41:
            case 0x42:
                func_003DD788(m, (s16 *)((u8 *)a + (i - 0x40) * 6));
                func_003DD8E8(dst, m, dst, *(f32 *)(f + 0xB07C));
                break;
            case 0x43:
            case 0x44:
            case 0x46:
            case 0x47:
            case 0x49:
            case 0x4A:
            case 0x4C:
            case 0x4D:
                v[0] = p[0] * (3.141593f / 32768.0f);
                v[1] = (s16)-p[1] * (3.141593f / 32768.0f);
                v[2] = (s16)-p[2] * (3.141593f / 32768.0f);
                func_00349D70(m, v);
                func_003DD8E8(dst, m, dst, *(f32 *)(f + 0xB07C));
                break;
            case 0x45:
            case 0x48:
            case 0x4B:
            case 0x4E:
                v[0] = (s16)-p[0] * (3.141593f / 32768.0f);
                v[1] = (s16)-p[1] * (3.141593f / 32768.0f);
                v[2] = (s16)-p[2] * (3.141593f / 32768.0f);
                func_00349D70(m, v);
                func_003DD8E8(dst, m, dst, *(f32 *)(f + 0xB07C));
                break;
            }
            dst += 0x40;
        }
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/demo", jtbl_00522CC0);
LIT4_WORD(D_005F5718, 0x38C90FDC);
LIT4_WORD(D_005F571C, 0x40490FDB);
LIT4_WORD(D_005F5720, 0x38C90FDC);
LIT4_WORD(D_005F5724, 0x38C90FDC);
INCLUDE_ASM("asm/nonmatchings/demo", func_003DE0D8);
#endif

void func_003DE428(void *m, s16 *a) {
    f32 v[4];
    s16 x;
    s16 y;
    s16 z;

    x = a[0] + 3.141593f;
    y = a[1] + 3.141593f;
    z = a[2] + 3.141593f;
    v[0] = x * (3.141593f / 32768.0f);
    v[1] = y * (3.141593f / 32768.0f);
    v[2] = z * (3.141593f / 32768.0f);
    func_00349D70(m, v);
}

/* Poses the six command-demo joints (D_0046B010[side]) of fighter `side` from the frame unkB058 of its
 * per-slot animation (func_00367270): three rotations of 0.12 angles, three of half turns, scale and translation,
 * then rebuilds the joint matrices in `bones` (0xC0 per joint, from +0x4980) with `view` [I]. */
#if 0
/* NON_MATCHING: 66 lines: register allocation (the original keeps the fighter pointer in s1 and a copy in s7, the frame data pointer in s4) */
void func_003DE4E0(u8 *f, u8 *bones, u8 *mats, s32 side, void *view) {
    Vu0Matrix m;
    Fighter *g;
    u8 *anim;
    s32 i;
    s32 r;
    u8 *dst;
    u8 *b;

    g = &gFighters[side];
    if (!(*(u16 *)((u8 *)g + 0xB130) & 2)) {
        return;
    }
    anim = (u8 *)func_00367270(*(s16 *)(f + 0x22));
    if (anim == NULL) {
        return;
    }
    i = *(s16 *)(f + 0xB058);
    if (i >= ((s16 *)anim)[0]) {
        i = ((s16 *)anim)[0] - 1;
    }
    anim += i * ((s16 *)anim)[1] * 6 + 4;
    for (i = 0; i < 6; i++) {
        s32 j = D_0046B010[side].j[i];

        r = i % 3;
        dst = mats + j * 64;
        b = bones + j * 0xC0 + 0x4980;
        if (i < 3) {
            func_003DD788(dst, (s16 *)(anim + r * 12 + 6));
        } else {
            func_003DE428(dst, (s16 *)(anim + r * 12 + 6));
        }
        func_003DD800(dst, (u16 *)(anim + r * 12), *(s32 *)((u8 *)g + 0x500));
        sceVu0MulMatrix(b, *(void **)(b + 0xB0), dst);
        sceVu0MulMatrix(b + 0x40, D_004334C0, b);
        sceVu0MulMatrix(m, view, b);
        ((u128 *)(b + 0x80))[0] = ((u128 *)m)[0];
        ((u128 *)(b + 0x80))[1] = ((u128 *)m)[1];
        ((u128 *)(b + 0x80))[2] = ((u128 *)m)[2];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/demo", func_003DE4E0);
#endif

/* Sends the GS register pair of entry i (D_00456540, 24 bytes) through VIF1 DIRECT and sets the linear map
 * D_005F6944 / D_005F6948 from its two (x, y) points [I: depth to fog]. */
void func_003DE6C0(s32 i) {
    Vif1Packet pk;
    DemoGsEntry *e;
    u32 spr;

    e = &D_00456540[i];
    D_00456780.unk10 = e->reg;
    Spr_Setup(1, 0);
    spr = *(u32 *)0x70000000;
    func_00343128(spr + 0x10, &D_00456780, 2);
    Dma_Wait((volatile u32 *)0x1000D400);
    sceVif1PkInit(&pk, spr);
    sceVif1PkReset(&pk);
    sceVif1PkCnt(&pk, 0);
    sceVif1PkOpenDirectCode(&pk, 0);
    pk.pCurrent[-2] = 0x11000000;
    sceVif1PkReserve(&pk, 8);
    sceVif1PkCloseDirectCode(&pk);
    sceVif1PkTerminate(&pk);
    func_00343178(pk.pBase, sceVif1PkSize(&pk));
    D_005F6944 = (e->x1 - e->x0) / (e->y1 - e->y0);
    D_005F6948 = e->x0 - e->y0 * (e->x1 - e->x0) / (e->y1 - e->y0);
}

/* Fills the BITBLTBUF (+0x10) and TRXPOS (+0x20) words of an image upload packet p and links it in `out`;
 * returns the end of the packet (0x60 header bytes plus the image qwords, p->unk50 & 0x7FFF). */
DemoImg *Gfx_BuildImageUpload(DemoImgOut *out, DemoImg *p, u32 spsm, u32 dpsm, u32 dbp, u32 dsax, u32 dsay, u32 w) {
    u64 bw;
    s32 n;

    out->img = p;
    bw = w >> 6;
    n = ((p->unk50 & 0x7FFF) << 4) + 0x60;
    out->size = n;
    p->bitbltbuf = (bw << 16) | ((u64)spsm << 24) | ((u64)dbp << 32) | (bw << 48) | ((u64)dpsm << 56);
    p->trxpos = ((u64)dsax << 32) | ((u64)dsay << 48);
    return (DemoImg *)((u8 *)p + n);
}

/* Builds the two upload packets of side `side`'s command text image (D_01FC4970[side]) and sends them. */
void func_003DE850(s32 side, DemoImg *img) {
    DemoImg *next;

    next = Gfx_BuildImageUpload(&D_01FC4970[side].a, img, 0x14, 0x14, 0x3B40, 0, 4, 0x100);
    Gfx_BuildImageUpload(&D_01FC4970[side].b, next, 0, 0, 0x3B40, 0, 0, 0x100);
    func_00399820(D_01FC4970[side].a.img, D_01FC4970[side].a.size / 16);
    func_00399820(D_01FC4970[side].b.img, D_01FC4970[side].b.size / 16);
    Sprite_TexFlush(1);
}

/* Loads the practice menu images (D_004567A0 entries 0-2, D_00457380 entries 0-2) and builds their upload
 * packets in D_01FC4990 / D_01FC49C0. */
void func_003DE930(void) {
    DemoImg *p;

    p = func_00364528(0, D_004567A0);
    p = Gfx_BuildImageUpload(&D_01FC4990[0].a, p, 0x14, 0x14, 0x3B40, 0, 0xC8, 0x100);
    Gfx_BuildImageUpload(&D_01FC4990[0].b, p, 0xA, 0xA, 0x3B40, 0, 0xEE, 0x100);
    p = func_00364528(1, D_004567A0);
    p = Gfx_BuildImageUpload(&D_01FC4990[1].a, p, 0x14, 0x14, 0x3B40, 0x80, 0xC8, 0x100);
    Gfx_BuildImageUpload(&D_01FC4990[1].b, p, 0xA, 0xA, 0x3B40, 0x80, 0xEE, 0x100);
    p = func_00364528(2, D_004567A0);
    p = Gfx_BuildImageUpload(&D_01FC4990[2].a, p, 0x14, 0x14, 0x3B40, 0x80, 0xC8, 0x100);
    Gfx_BuildImageUpload(&D_01FC4990[2].b, p, 0xA, 0xA, 0x3B40, 0xC0, 0xEE, 0x100);
    D_0046AF40[0] = func_00364528(2, D_00457380);
    D_0046AF40[1] = p = func_00364528(0, D_00457380);
    p = Gfx_BuildImageUpload(&D_01FC49C0[0].a, p, 0x14, 0x14, 0x3B40, 0, 4, 0x100);
    Gfx_BuildImageUpload(&D_01FC49C0[0].b, p, 0, 0, 0x3B40, 0, 0, 0x100);
    D_0046AF40[2] = func_00364528(2, D_00457380);
    D_0046AF40[3] = p = func_00364528(1, D_00457380);
    p = Gfx_BuildImageUpload(&D_01FC49C0[1].a, p, 0x14, 0x14, 0x3B40, 0, 4, 0x100);
    Gfx_BuildImageUpload(&D_01FC49C0[1].b, p, 0xA, 0xA, 0x3B40, 0, 0, 0x100);
}

void func_003DEB50(s32 i) {
    D_0046AF40[0] = func_00364528(i + 2, D_00457380);
}

/* Uploads the two images of slot i (D_01FC49C0) and sets the sprite state. */
void func_003DEB80(s32 i) {
    func_00399820(D_01FC49C0[i].a.img, D_01FC49C0[i].a.size / 16);
    func_00399820(D_01FC49C0[i].b.img, D_01FC49C0[i].b.size / 16);
    Sprite_TexFlush(1);
    Sprite_SetFilter(1, 0);
    Sprite_SetBlend(1, 1);
}

void func_003DEC20(void) {
    func_00399820(D_01FC4970[2].a.img, D_01FC4970[2].a.size / 16);
    func_00399820(D_01FC4970[2].b.img, D_01FC4970[2].b.size / 16);
    func_00399820(D_01FC4970[3].a.img, D_01FC4970[3].a.size / 16);
    func_00399820(D_01FC4970[3].b.img, D_01FC4970[3].b.size / 16);
    func_00399820(D_01FC4970[4].b.img, D_01FC4970[4].b.size / 16);
    Sprite_TexFlush(1);
}

/* A GS register packet (A+D, 4 registers): TEST, ALPHA, ... [I]. */
void func_003DECE0(void) {
    u64 *p;

    p = func_00399520();
    if (p != NULL) {
        *p++ = 0x118B000000000004UL;
        *p++ = 0xEEEE;
        *p++ = 0x44;
        *p++ = 0x43;
        *p++ = 0;
        *p++ = 9;
        *p++ = 1;
        *p++ = 0x15;
        *p++ = 0x5000D;
        *p++ = 0x48;
        func_00399650(p);
    }
}

void func_003DED90(void) {
    u64 *p;

    p = func_00399520();
    if (p != NULL) {
        p[0] = 0x18B000000008000UL;
        p++;
        p[0] = 0;
        func_00399650(p + 1);
    }
}

/* Draws glyph c (from 0x20) of the 12 x 18 command font at g->x, g->y with the two colours of g. */
#if 0
/* NON_MATCHING: packet pointer in t0 (original t3) and the TEX0 constant built later: register pressure of the original's packet macros [I] */
void PracFont_DrawGlyph(DemoGlyph *g, u32 c, s32 i) {
    u64 *p;
    s32 x;
    s32 y;
    s32 u;
    s32 v;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    if (c == 0x20) {
        return;
    }
    x = g->x;
    y = g->y;
    *p++ = 0x71AB400000000001UL;
    *p++ = 0x43431EE;
    *p++ = 4;
    *p++ = 0x1C;
    *p++ = 0x2007680621413B40UL;
    *p++ = 7;
    c -= 0x20;
    v = ((c >> 4) * 18 + 4) << 4;
    u = ((c & 0xF) * 12) << 4;
    *p++ = ((u64)g->c[2] << 32) | g->c[3];
    *p++ = ((u64)g->c[0] << 32) | g->c[1];
    *p++ = (u + 8) | ((u64)(v + 8) << 32);
    *p++ = 0;
    x = (x + 0x6C0) << 4;
    y = (y + 0x720) << 4;
    *p++ = x | ((u64)y << 32);
    *p++ = 0x1000FFFF0UL;
    *p++ = (u + 0xC8) | ((u64)(v + 0x128) << 32);
    *p++ = 0;
    *p++ = (x + 0xC0) | ((u64)(y + 0x120) << 32);
    *p++ = 0x1000FFFF0UL;
    func_00399650(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/demo", PracFont_DrawGlyph);
#endif

/* Draws button icon k (0-15, bit 4 the second row) at x, y [I]. */
#if 0
/* NON_MATCHING: frame 0x40 (original 0x50, an extra stack slot) and the 0xC0 / 0x80 choice as branches, not movz */
void func_003DEF98(s32 x, s32 y, s32 k) {
    u64 *p;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    *p++ = 0x818B400000008001UL;
    *p++ = 0x43431EEE;
    *p++ = ((u64)((k & 0x10) ? 0xC0 : 0x80) << 2) | ((u64)((k & 0xF) + 0xEE) << 12) | 4;
    *p++ = 0x1C;
    *p++ = 0x20D7680621413B40UL;
    *p++ = 7;
    *p++ = 0;
    *p++ = 9;
    *p++ = 0x8000000080UL;
    *p++ = 0x8000000080UL;
    *p++ = 0xC8800000808UL;
    *p++ = 0;
    x = (x + 0x6C0) << 4;
    y = (y + 0x720) << 4;
    *p++ = x | ((u64)y << 32);
    *p++ = 0x1000FFFF0UL;
    *p++ = 0xDB800000938UL;
    *p++ = 0;
    *p++ = (x + 0x131) | ((u64)(y + 0x131) << 32);
    *p++ = 0x1000FFFF0UL;
    func_00399650(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/demo", func_003DEF98);
#endif

/* Draws button icon k (0-15, bit 4 the second row) at x, y, one row higher than func_003DEF98 [I]. */
#if 0
/* NON_MATCHING: frame 0x40 (original 0x50, an extra stack slot) and the 0xC0 / 0x80 choice scheduled differently, as func_003DEF98 */
void func_003DF150(s32 x, s32 y, s32 k) {
    u64 *p;
    s32 t;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    *p++ = 0x718B400000000001UL;
    t = 0xC0;
    if (!(k & 0x10)) {
        t = 0x80;
    }
    *p++ = 0x43431EE;
    *p++ = ((u64)t << 2) | (((u64)((k & 0xF) + 0xEE) << 12) | 4);
    *p++ = 0x1C;
    *p++ = 0x20D7680621413B40UL;
    *p++ = 7;
    *p++ = 0x8000000080UL;
    *p++ = 0x8000000080UL;
    *p++ = 0xC8800000808UL;
    *p++ = 0;
    x = (x + 0x6C0) << 4;
    y = (y + 0x71E) << 4;
    *p++ = x | ((u64)y << 32);
    *p++ = 0x1000FFFF0UL;
    *p++ = 0xDB800000938UL;
    *p++ = 0;
    *p++ = (x + 0x131) | ((u64)(y + 0x131) << 32);
    *p++ = 0x1000FFFF0UL;
    func_00399650(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/demo", func_003DF150);
#endif

INCLUDE_RODATA("asm/nonmatchings/demo", D_00522D00);
/* Draws arrow icon k (0-7 a direction of D_00522D00: tile, flip bits; bit 3 / 4 select the colour) at x, y [I]. */
#if 0
/* NON_MATCHING: 75 lines: packet stores scheduled differently (same family as func_003DEF98) */
void func_003DF2F0(s32 x, s32 y, u32 k) {
    u64 *p;
    s32 i;
    s32 u0;
    s32 u1;
    s32 v0;
    s32 v1;
    s32 sx;
    s32 sy;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    i = k & 7;
    sx = (x + 0x6C0) << 4;
    sy = (y + 0x720) << 4;
    if (D_00522D00[i].flip & 1) {
        u0 = (D_00522D00[i].tile << 8) + 0xF8;
        u1 = (D_00522D00[i].tile << 8) + 8;
    } else {
        u1 = (D_00522D00[i].tile << 8) + 0xE8;
        u0 = (D_00522D00[i].tile << 8) + 8;
    }
    if (D_00522D00[i].flip & 2) {
        v1 = 0xC88;
        v0 = 0xD68;
    } else {
        v0 = 0xC88;
        v1 = 0xD68;
    }
    *p++ = 0x818B400000000001UL;
    *p++ = 0x43431EEE;
    *p++ = ((u64)((((k & 0x10) >> 3) + 0xEE) | (~(k >> 3) & 1)) << 12) | 4;
    *p++ = 0x1C;
    *p++ = 0x20D7680621413B40UL;
    *p++ = 7;
    *p++ = 0;
    *p++ = 9;
    *p++ = 0x8000000080UL;
    *p++ = 0x8000000080UL;
    *p++ = u0 | ((u64)v0 << 32);
    *p++ = 0;
    *p++ = sx | ((u64)sy << 32);
    *p++ = 0x1FFFFFFFFUL;
    *p++ = u1 | ((u64)v1 << 32);
    *p++ = 0;
    *p++ = (sx + 0xF1) | ((u64)(sy + 0xF1) << 32);
    *p++ = 0x1FFFFFFFFUL;
    func_00399650(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/demo", func_003DF2F0);
#endif

/* As func_003DF2F0 with one row less (no 0 / 9 registers); k == 0x20 draws direction 8 [I]. */
#if 0
/* NON_MATCHING: not compared (same packet family as func_003DF2F0, which is 75 lines off) */
void func_003DF500(s32 x, s32 y, u32 k) {
    u64 *p;
    s32 i;
    s32 u0;
    s32 u1;
    s32 v0;
    s32 v1;
    s32 sx;
    s32 sy;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    i = k != 0x20 ? k & 7 : 8;
    sx = (x + 0x6C0) << 4;
    sy = (y + 0x720) << 4;
    if (D_00522D00[i].flip & 1) {
        u0 = (D_00522D00[i].tile << 8) + 0xF8;
        u1 = (D_00522D00[i].tile << 8) + 8;
    } else {
        u1 = (D_00522D00[i].tile << 8) + 0xE8;
        u0 = (D_00522D00[i].tile << 8) + 8;
    }
    if (D_00522D00[i].flip & 2) {
        v1 = 0xC88;
        v0 = 0xD68;
    } else {
        v0 = 0xC88;
        v1 = 0xD68;
    }
    *p++ = 0x718B400000000001UL;
    *p++ = 0x43431EE;
    *p++ = ((u64)((((k & 0x10) >> 3) + 0xEE) | (~(k >> 3) & 1)) << 12) | 4;
    *p++ = 0x1C;
    *p++ = 0x20D7680621413B40UL;
    *p++ = 7;
    *p++ = 0x8000000080UL;
    *p++ = 0x8000000080UL;
    *p++ = u0 | ((u64)v0 << 32);
    *p++ = 0;
    *p++ = sx | ((u64)sy << 32);
    *p++ = 0x1FFFFFFFFUL;
    *p++ = u1 | ((u64)v1 << 32);
    *p++ = 0;
    *p++ = (sx + 0xF1) | ((u64)(sy + 0xF1) << 32);
    *p++ = 0x1FFFFFFFFUL;
    func_00399650(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/demo", func_003DF500);
#endif

/* Width of a glyph of the command font: 0xEC-0xFB 0x14, 0xDC-0xEB 0x10, 0xDB 0xE, space 4, else D_004572C0[c]. */
s32 PracFont_GlyphWidth(s32 c) {
    if ((u32)(c - 0xEC) < 0x10) {
        return 0x14;
    }
    if ((u32)(c - 0xDC) < 0x10) {
        return 0x10;
    }
    if (c == 0xDB) {
        return 0xE;
    }
    if (c == 0x20) {
        return 4;
    }
    return D_004572E0[c - 0x20];
}

/* Width of a command string: 0xFC skips a byte, 0xFF 'n' ends the line. */
s32 PracFont_StrWidth(u8 *s) {
    s32 w = 0;
    u8 c;

    c = *s++;
    if (c != 0) {
        do {
            if (c != 0xFC) {
                if (c == 0xFF) {
                    if (*s++ == 'n') {
                        break;
                    }
                } else {
                    w += PracFont_GlyphWidth(c);
                }
            } else {
                s++;
            }
            c = *s++;
        } while (c != 0);
    }
    return w;
}

/* Measures a command string for justification within `width`: counts its spaces in *n and returns the space
 * width (at least 4). "\S" steps the variant counter (only variant D_005F6936 is measured), "\n" ends the line
 * (returns 4), 0xFC skips a byte, 0xFF 'n' ends the string [I]. */
s32 func_003DF808(s32 *n, char *s, s32 width) {
    s32 esc;
    s32 sel;
    s32 cnt;
    s32 last;
    s32 x;
    u8 c;

    last = 0;
    cnt = 0;
    sel = -1;
    esc = 0;
    x = 0;
    *n = 0;
    c = *s++;
    while (c != 0) {
        if (c == '\\') {
            if (esc == 0) {
                esc = 1;
                goto next;
            }
            esc = 0;
        }
        if (esc != 0) {
            if (c == 'S') {
                sel++;
            } else if (c == 'n') {
                (*n)++;
                return 4;
            }
            esc = 0;
        } else {
            switch (c) {
            case 0xFF:
                if (*s++ == 'n') {
                    goto end;
                }
                break;
            case 0xFC:
                s++;
                break;
            case ' ':
                if (x < width) {
                    last = x;
                    x += 4;
                    cnt = ++*n;
                    break;
                }
                x = (width - (last - cnt * 4)) / (cnt - 1);
                return x > 3 ? x : 4;
            default:
                if (sel < 0 || sel == D_005F6936) {
                    x += PracFont_GlyphWidth(c);
                }
                break;
            }
        }
    next:
        c = *s++;
    }
end:
    if (x >= width) {
        x = (width - (last - cnt * 4)) / (cnt - 1);
        return x > 3 ? x : 4;
    }
    *n = 0;
    return 4;
}

#if 0
/* NON_MATCHING: 328 lines: m2c draft (variadic command printer; its switch emits a C jump table when enabled) */
#define M2C_UNK u8
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(s32)(expr))
s32 exit(s32);

u8 *func_003DFA00(u8 *arg0, ...) {
    u8 sp[0x200];
    s32 subroutine_arg0, subroutine_arg1, subroutine_arg2, subroutine_arg3, subroutine_arg4, subroutine_arg5;
    s32 sp10;
    s32 sp14;
    s32 sp18;
    s32 *sp1C;
    s64 sp108;
    s64 sp110;
    s64 sp118;
    s64 sp120;
    s64 sp128;
    s64 sp130;
    s64 sp138;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 var_fp;
    s32 var_s5;
    s32 var_s6;
    s32 var_s7;
    s64 *var_s4;
    u16 temp_v0;
    u16 temp_v0_2;
    u16 temp_v1;
    u16 var_a1;
    u16 var_s1_4;
    u32 temp_v1_2;
    u32 var_s1_3;
    u8 *temp_s0;
    u8 *var_a0;
    u8 *var_s0;
    u8 *var_s2;
    u8 *var_s3;
    u8 var_s1;
    u8 var_s1_2;

    var_s4 = &sp108;
    var_fp = 0;
    var_s7 = 0;
    var_s6 = 0;
    var_s5 = 0;
    sp18 = 0;
    D_005F6934 = 0;
    func_003DECE0();
    sp1C = &sp10;
    var_s1 = *arg0;
    sp14 = func_003DF808(&sp10, arg0, D_005F6930);
    var_s3 = arg0 + 1;
    if (var_s1 != 0) {
        do {
            switch (var_s1) {                       /* switch 1; irregular */
            case 0xFC:                              /* switch 1 */
                var_s3 += 1;
                if (*var_s3 == 0xFF) {
                    var_a0 = var_s3;
                    sp18 = 1;
block_14:
                    var_s2 = var_s3 + 1;
                    PracFont_StrWidth(var_a0);
                } else {
block_61:
                    var_s2 = var_s3 + 1;
                }
                break;
            case 0xFF:                              /* switch 1 */
                var_s6 = 1;
                if (var_s6 == 1) {
                    var_s6 = 0;
                default:                            /* switch 1 */
                    if (var_s6 != 0) {
                        var_s2 = var_s3 + 1;
                        if (var_s6 == 1) {
                            var_s6 = 0;
                            switch (var_s1) {       /* switch 2 */
                            case 0x6E:              /* switch 2 */
                                var_a0 = var_s3;
                                if (sp18 != 0) {
                                    goto block_14;
                                }
                                var_s2 = var_s3 + 1;
                                break;
                            case 0x73:              /* switch 2 */
                                var_s4 += 8;
                                temp_s0 = M2C_FIELD(var_s4, u8 **, -8);
                                var_s1_2 = *temp_s0;
                                var_s0 = temp_s0 + 1;
                                if (var_s1_2 != 0) {
                                    var_s2 = var_s3 + 1;
                                    do {
                                        PracFont_DrawGlyph(sp, var_s1_2, var_s5);
                                        var_s5 += 1;
                                        var_s1_2 = *var_s0;
                                        var_s0 += 1;
                                    } while (var_s1_2 != 0);
                                } else {
                                    goto block_61;
                                }
                                break;
                            case 0x48:              /* switch 2 */
                                var_s4 += 8;
                                var_s2 = var_s3 + 1;
                                break;
                            case 0x56:              /* switch 2 */
                                var_s4 += 8;
                                var_s2 = var_s3 + 1;
                                break;
                            case 0x68:              /* switch 2 */
                                var_s4 += 8;
                                var_s2 = var_s3 + 1;
                                break;
                            case 0x76:              /* switch 2 */
                                var_s4 += 8;
                                var_s2 = var_s3 + 1;
                                break;
                            case 0x77:              /* switch 2 */
                                var_s4 += 8;
                                var_s2 = var_s3 + 1;
                                D_005F6930 = M2C_FIELD(var_s4, u16 *, -8);
                                break;
                            case 0x69:              /* switch 2 */
                                var_s4 += 8;
                                var_s5 = 0;
                                var_s2 = var_s3 + 1;
                                break;
                            case 0x4C:              /* switch 2 */
                                var_s4 += 8;
                                var_s2 = var_s3 + 1;
                                D_005F6932 = M2C_FIELD(var_s4, u16 *, -8);
                                break;
                            default:                /* switch 2 */
                                exit(0);
                                goto block_30;
                            }
                        }
                    } else {
block_30:
                        temp_v1 = var_s1 - 0xDC;
                        if (temp_v1 >= 0x10U) {
                            temp_v1_2 = var_s1 - 0xEC;
                            if (temp_v1_2 < 0x10U) {
                                var_s1_3 = temp_v1_2;
                                var_s5 = 0;
                                if (var_s7 == 0) {
                                    temp_v0 = D_005F6932 - 1;
                                    if (D_005F6932 != 0) {
                                        D_005F6932 = temp_v0;
                                        if (!(temp_v0 & 0xFFFF)) {
                                            D_005F6934 = var_s1_3 | 0x20;
                                        }
                                        var_s1_3 |= 0x10;
                                    }
                                }
                                var_s2 = var_s3 + 1;
                                func_003DF150(subroutine_arg0, (s16) subroutine_arg0 + 2, var_s1_3);
                            } else {
                                switch (var_s1) {   /* switch 3; irregular */
                                case 0xDB:          /* switch 3 */
                                    var_s2 = var_s3 + 1;
                                    func_003DF500(subroutine_arg0, (s16) subroutine_arg0 + 2, 0x20U);
                                    break;
                                case 0x20:          /* switch 3 */
                                    var_fp += 1;
                                    if (var_fp == sp10) {
                                        var_fp = 0;
                                        var_s2 = var_s3 + 1;
                                        sp14 = func_003DF808(sp1C, var_s3, D_005F6930);
                                    } else {
                                        var_s2 = var_s3 + 1;
                                    }
                                    break;
                                case 0x28:          /* switch 3 */
                                    temp_a2 = var_s5;
                                    var_s5 += 1;
                                    PracFont_DrawGlyph(sp, 0x28U, temp_a2);
                                    var_s7 = 1;
                                    var_s2 = var_s3 + 1;
                                    break;
                                case 0x29:          /* switch 3 */
                                    temp_a2_2 = var_s5;
                                    var_s5 += 1;
                                    PracFont_DrawGlyph(sp, 0x29U, temp_a2_2);
                                    var_s7 = 0;
                                    var_s2 = var_s3 + 1;
                                    break;
                                default:            /* switch 3 */
                                    var_s5 += 1;
                                    PracFont_DrawGlyph(sp, var_s1, var_s5);
                                    var_s2 = var_s3 + 1;
                                    break;
                                }
                            }
                        } else {
                            var_s1_4 = temp_v1;
                            var_s5 = 0;
                            if (var_s7 == 0) {
                                temp_v0_2 = D_005F6932 - 1;
                                if (D_005F6932 != 0) {
                                    D_005F6932 = temp_v0_2;
                                    if (!(temp_v0_2 & 0xFFFF)) {
                                        var_a1 = var_s1_4 | 0x10;
                                        D_005F6934 = var_a1;
                                    } else {
                                        var_a1 = var_s1_4 | 0x10;
                                    }
                                    var_s1_4 = var_a1;
                                }
                            }
                            var_s2 = var_s3 + 1;
                            func_003DF500(subroutine_arg0, (s16) subroutine_arg0 + 2, var_s1_4);
                        }
                    }
                } else {
                    goto block_61;
                }
                break;
            }
            var_s1 = *var_s3;
            var_s3 = var_s2;
        } while (var_s1 != 0);
    }
    M2C_FIELD(((u8 *)&D_005F7060), u16 *, 0) = (u16) subroutine_arg2;
    M2C_FIELD(((u8 *)&D_005F7060), u16 *, 2) = (u16) subroutine_arg2;
    M2C_FIELD(((u8 *)&D_005F7064), u16 *, 0) = (u16) subroutine_arg0;
    M2C_FIELD(((u8 *)&D_005F7064), u16 *, 2) = (u16) subroutine_arg0;
    D_005F7068 = subroutine_arg1;
    func_003DED90();
    return var_s3;
}
#undef M2C_UNK
#undef M2C_FIELD
#undef M2C_BITWISE
#else
INCLUDE_RODATA("asm/nonmatchings/demo", jtbl_00522D20);
INCLUDE_ASM("asm/nonmatchings/demo", func_003DFA00);
#endif

u16 func_003DFF20(void) {
    return D_005F6934;
}

u16 func_003DFF28(void) {
    return D_005F6932;
}

#if 0
/* NON_MATCHING: 344 lines: m2c draft (the float to int conversions of the rotated corners are guesses) */
#define M2C_UNK u8
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(s32)(expr))
f32 cosf(f32);
f32 sinf(f32, u32);

void func_003DFF30(s32 arg0, s32 arg1, s32 arg2, s32 arg3, u32 arg4) {
    u8 sp[0x200];
    s32 subroutine_arg0, subroutine_arg1, subroutine_arg2, subroutine_arg3, subroutine_arg4, subroutine_arg5;
    f32 temp_f21;
    f32 temp_f4;
    f32 temp_f5;
    f32 var_f1;
    f32 var_f1_2;
    f32 var_f20;
    s32 temp_a0;
    s32 temp_a1;
    s32 temp_a2;
    s32 temp_a2_2;
    s64 temp_a1_2;
    s64 temp_a2_3;
    s64 temp_a3;
    s64 temp_a3_2;
    u32 temp_lo;
    u32 temp_s0;
    u32 temp_s3;
    u32 temp_s4;
    u64 temp_t0;
    void *temp_s0_10;
    void *temp_s0_11;
    void *temp_s0_12;
    void *temp_s0_13;
    void *temp_s0_14;
    void *temp_s0_15;
    void *temp_s0_16;
    void *temp_s0_17;
    void *temp_s0_18;
    void *temp_s0_19;
    void *temp_s0_20;
    void *temp_s0_21;
    void *temp_s0_22;
    void *temp_s0_23;
    void *temp_s0_24;
    void *temp_s0_25;
    void *temp_s0_26;
    void *temp_s0_2;
    void *temp_s0_3;
    void *temp_s0_4;
    void *temp_s0_5;
    void *temp_s0_6;
    void *temp_s0_7;
    void *temp_s0_8;
    void *temp_s0_9;

    temp_s4 = arg4;
    temp_s0 = arg2 << 8;
    temp_lo = temp_s0 / 200U;
    temp_s0_2 = func_00399520();
    if ((s32) temp_lo >= 0) {
        var_f20 = (f32) temp_lo;
    } else {
        var_f20 = 2.0f * (f32) ((temp_lo & 1) | (temp_s0 / 400));
    }
    if (temp_s0_2 != NULL) {
        temp_s3 = arg3 - 1;
        temp_f21 = sinf(arg0, temp_lo);
        temp_f5 = cosf(arg0);
        if (arg0 >= 0) {

        }
        temp_f4 = -var_f20;
        if (arg1 >= 0) {

        }
        if (arg0 >= 0) {
            var_f1 = (f32) arg0;
        } else {
            var_f1 = 2.0f * (f32) ((arg0 & 1) | ((u32) arg0 >> 1));
        }
        if (arg1 >= 0) {

        }
        if (arg0 >= 0) {

        }
        if (arg1 >= 0) {

        }
        if (arg0 >= 0) {
            var_f1_2 = (f32) arg0;
        } else {
            var_f1_2 = 2.0f * (f32) ((arg0 & 1) | ((u32) arg0 >> 1));
        }
        if (arg1 >= 0) {

        }
        M2C_FIELD(temp_s0_2, s64 *, 0) = (s64) ((0xC1AA4000ULL << 0x20) | 0x8001);
        temp_s0_3 = temp_s0_2 + 8;
        M2C_FIELD(temp_s0_2, s64 *, 8) = (s64) ((((0x4343ULL << 0x10) | 0x4343) << 0x10) | 0x1EEE);
        temp_s0_4 = temp_s0_3 + 8;
        M2C_FIELD(temp_s0_3, s64 *, 8) = (s64) (((temp_s3 & 0x30) * 4) | 4);
        temp_s0_5 = temp_s0_4 + 8;
        M2C_FIELD(temp_s0_4, s64 *, 8) = 0x1C;
        temp_s0_6 = temp_s0_5 + 8;
        M2C_FIELD(temp_s0_5, s64 *, 8) = (s64) ((((0x20D76806ULL << 0x10) | 0x2141) << 0x10) | 0x3B40);
        temp_s0_7 = temp_s0_6 + 8;
        M2C_FIELD(temp_s0_6, s64 *, 8) = 7;
        temp_s0_8 = temp_s0_7 + 8;
        M2C_FIELD(temp_s0_7, s64 *, 8) = 0x44;
        temp_s0_9 = temp_s0_8 + 8;
        M2C_FIELD(temp_s0_8, s64 *, 8) = 0x43;
        temp_s0_10 = temp_s0_9 + 8;
        M2C_FIELD(temp_s0_9, s64 *, 8) = (s64) (((temp_s4 >> 0x18) & 0xFF) | ((u64)((temp_s4 >> 0x10) & 0xFF) << 0x20));
        temp_a2 = (temp_s3 & 0xF) << 8;
        temp_a1 = (((temp_s3 >> 6) * 0xF) + 4) * 0x10;
        temp_s0_11 = temp_s0_10 + 8;
        temp_a0 = temp_a2 + 8;
        M2C_FIELD(temp_s0_10, s64 *, 8) = (s64) (((temp_s4 >> 8) & 0xFF) | ((u64)(temp_s4 & 0xFF) << 0x20));
        temp_a3 = (s64)(temp_a1 + 8) << 0x20;
        temp_s0_12 = temp_s0_11 + 8;
        M2C_FIELD(temp_s0_11, s64 *, 8) = (s64) (temp_a0 | temp_a3);
        temp_s0_13 = temp_s0_12 + 8;
        temp_t0 = -1U >> 0x1F;
        M2C_FIELD(temp_s0_12, s64 *, 8) = 0;
        temp_s0_14 = temp_s0_13 + 8;
        temp_a1_2 = (s64)(temp_a1 + 0xE8) << 0x20;
        M2C_FIELD(temp_s0_13, s64 *, 8) = (s64) (M2C_BITWISE(s32, (temp_f21 * temp_f4)) | (M2C_BITWISE(s64, (temp_f5 * temp_f4)) << 0x20));
        temp_s0_15 = temp_s0_14 + 8;
        M2C_FIELD(temp_s0_14, u64 *, 8) = temp_t0;
        temp_s0_16 = temp_s0_15 + 8;
        M2C_FIELD(temp_s0_15, s64 *, 8) = (s64) (temp_a0 | temp_a1_2);
        temp_s0_17 = temp_s0_16 + 8;
        M2C_FIELD(temp_s0_16, s64 *, 8) = 0;
        temp_a2_2 = temp_a2 + 0xF8;
        temp_s0_18 = temp_s0_17 + 8;
        M2C_FIELD(temp_s0_17, s64 *, 8) = (s64) (M2C_BITWISE(s32, var_f1) | (0x43F00000LL << 0x20));
        temp_s0_19 = temp_s0_18 + 8;
        temp_a3_2 = temp_a2_2 | temp_a3;
        M2C_FIELD(temp_s0_18, u64 *, 8) = temp_t0;
        temp_s0_20 = temp_s0_19 + 8;
        M2C_FIELD(temp_s0_19, s64 *, 8) = temp_a3_2;
        temp_a2_3 = temp_a2_2 | temp_a1_2;
        temp_s0_21 = temp_s0_20 + 8;
        M2C_FIELD(temp_s0_20, s64 *, 8) = 0;
        temp_s0_22 = temp_s0_21 + 8;
        M2C_FIELD(temp_s0_21, s64 *, 8) = (s64) (0x43F00000 | (0x43F00000LL << 0x20));
        temp_s0_23 = temp_s0_22 + 8;
        M2C_FIELD(temp_s0_22, u64 *, 8) = temp_t0;
        temp_s0_24 = temp_s0_23 + 8;
        M2C_FIELD(temp_s0_23, s64 *, 8) = temp_a2_3;
        temp_s0_25 = temp_s0_24 + 8;
        M2C_FIELD(temp_s0_24, s64 *, 8) = 0;
        temp_s0_26 = temp_s0_25 + 8;
        M2C_FIELD(temp_s0_25, s64 *, 8) = (s64) (M2C_BITWISE(s32, var_f1_2) | (0x43F00000LL << 0x20));
        M2C_FIELD(temp_s0_26, u64 *, 8) = temp_t0;
        func_00399650((u64 *)(temp_s0_26 + 8 + 8));
    }
}
#undef M2C_UNK
#undef M2C_FIELD
#undef M2C_BITWISE
#else
INCLUDE_ASM("asm/nonmatchings/demo", func_003DFF30);
#endif

/* Draws icon k (1-based, 16 per row of the icon page) centred on x, y, scaled to w and h percent [I]. */
#if 0
/* NON_MATCHING: frame 0x70 (original 0x80, an extra stack slot as func_003DEF98) and the two divisions scheduled apart (divu / divu1 pair in the original) */
void func_003E0460(u32 x, u32 y, u32 w, u32 h, u32 k, u32 col) {
    u64 *p;
    u32 dy;
    u32 dx;
    u32 u;
    u32 v;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    dy = h * 15 / 100;
    dx = w * 16 / 100;
    k--;
    *p++ = 0x81AB400000008001UL;
    *p++ = 0x43431EEE;
    *p++ = ((u64)(k & 0x30) << 2) | 4;
    *p++ = 0x1C;
    *p++ = 0x20D7680621413B40UL;
    *p++ = 7;
    *p++ = 0x44;
    *p++ = 0x43;
    *p++ = ((col >> 24) & 0xFF) | ((u64)((col >> 16) & 0xFF) << 32);
    *p++ = ((col >> 8) & 0xFF) | ((u64)(col & 0xFF) << 32);
    v = ((k >> 6) * 15 + 4) << 4;
    u = (k & 0xF) << 8;
    *p++ = (u + 8) | ((u64)(v + 8) << 32);
    *p++ = 0;
    dy <<= 3;
    dx <<= 3;
    *p++ = (x - dx) | ((u64)(y - dy) << 32);
    *p++ = 0x1FFFFFFFFUL;
    *p++ = (u + 0xF8) | ((u64)(v + 0xE8) << 32);
    *p++ = 0;
    *p++ = (x + dx + 1) | ((u64)(y + dy + 1) << 32);
    *p++ = 0x1FFFFFFFFUL;
    func_00399650(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/demo", func_003E0460);
#endif

/* Prints a justified command string at the text position D_005F7064 (line start D_005F7060, colours D_005F7068)
 * and keeps the position; "\S" variant blocks, "\n" new line (+20). Returns the end of the string [I]. */
char *func_003E06A0(char *s, ...) {
    DemoTextPos t;
    s32 n;
    s32 esc;
    s32 sel;
    s32 cnt;
    s32 k;
    s32 w;
    u8 c;

    t.x0 = D_005F7060[0];
    t.y0 = D_005F7060[1];
    t.g.x = D_005F7064[0];
    t.g.y = D_005F7064[1];
    *(u32 *)t.g.c = D_005F7068;
    k = 0;
    cnt = 0;
    sel = -1;
    esc = 0;
    n = 0;
    func_003DECE0();
    w = func_003DF808(&n, s, D_005F6930);
    c = *s++;
    while (c != 0) {
        if (c == '\\') {
            if (esc != 1) {
                esc = 1;
                goto next;
            }
            esc = 0;
        }
        if (esc != 0) {
            if (c != 'S') {
                if (c == 'n') {
                    t.g.x = t.x0;
                    cnt = 0;
                    t.g.y = t.y0 += 20;
                    w = func_003DF808(&n, s, D_005F6930);
                }
            } else {
                sel++;
            }
            esc = 0;
        } else if (sel < 0 || sel == D_005F6936) {
            if (c == ' ') {
                if (++cnt == n) {
                    t.g.x = t.x0;
                    cnt = 0;
                    t.g.y = t.y0 += 20;
                    w = func_003DF808(&n, s, D_005F6930);
                } else {
                    t.g.x += w;
                }
            } else {
                PracFont_DrawGlyph(&t.g, c, k++);
                {
                    u8 *w = (u8 *)D_004572E0 + c;

                    t.g.x += (s8)w[-0x20];
                }
            }
        }
    next:
        c = *s++;
    }
    D_005F7060[0] = t.x0;
    D_005F7060[1] = t.y0;
    D_005F7064[0] = t.g.x;
    D_005F7064[1] = t.g.y;
    D_005F7068 = *(u32 *)t.g.c;
    func_003DED90();
    return s;
}

void func_003E08D0(s16 v) {
    D_005F6936 = v;
}

#if 0
/* NON_MATCHING: 1074 lines: m2c draft (stack temporaries, float scheduling) */
#define M2C_UNK u8
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(s32)(expr))
f32 fabsf(f32 x);


s32 func_003E08D8(f32 *arg0, f32 *arg1) {
    f32 sp0;
    f32 sp4;
    f32 sp8;
    f32 spC;
    f32 sp10;
    f32 sp14;
    f32 sp18;
    f32 sp1C;
    f32 sp20;
    f32 sp24;
    f32 sp28;
    f32 sp2C;
    f32 sp30;
    f32 sp34;
    f32 sp38;
    f32 sp3C;
    f32 sp40;
    f32 sp44;
    f32 sp48;
    f32 sp4C;
    f32 sp50;
    f32 sp54;
    f32 sp58;
    f32 sp5C;
    f32 sp60;
    f32 sp64;
    f32 sp68;
    f32 sp6C;
    f32 sp70;
    f32 sp74;
    f32 sp78;
    f32 sp7C;
    f32 sp80;
    f32 sp84;
    f32 sp88;
    f32 sp8C;
    f32 sp90;
    f32 sp94;
    f32 sp98;
    f32 sp9C;
    f32 spA0;
    f32 spA4;
    f32 spA8;
    f32 spAC;
    f32 spB0;
    f32 spB4;
    f32 spB8;
    f32 spBC;
    f32 spC0;
    f32 spC4;
    f32 spC8;
    f32 spCC;
    f32 spD0;
    f32 spD4;
    f32 spD8;
    f32 spDC;
    f32 spE0;
    f32 spE4;
    f32 spE8;
    f32 spEC;
    f32 spF0;
    f32 spF4;
    f32 spF8;
    f32 spFC;
    f32 sp100;
    f32 sp104;
    f32 sp108;
    f32 sp10C;
    f32 sp110;
    f32 sp114;
    f32 sp118;
    f32 sp11C;
    f32 sp120;
    f32 sp124;
    f32 sp128;
    f32 sp12C;
    f32 sp130;
    f32 sp134;
    f32 sp138;
    f32 sp13C;
    f32 sp140;
    f32 sp144;
    f32 sp148;
    f32 sp14C;
    f32 sp150;
    f32 sp154;
    f32 sp158;
    f32 sp15C;
    f32 sp160;
    f32 sp164;
    f32 sp168;
    f32 sp16C;
    f32 sp170;
    f32 sp174;
    f32 sp178;
    f32 sp17C;
    f32 sp180;
    f32 sp184;
    f32 sp188;
    f32 sp194;
    f32 temp_f0;
    f32 temp_f0_2;
    f32 temp_f10;
    f32 temp_f10_2;
    f32 temp_f11;
    f32 temp_f11_2;
    f32 temp_f11_3;
    f32 temp_f12;
    f32 temp_f12_2;
    f32 temp_f12_3;
    f32 temp_f13;
    f32 temp_f14;
    f32 temp_f14_2;
    f32 temp_f14_3;
    f32 temp_f15;
    f32 temp_f15_2;
    f32 temp_f15_3;
    f32 temp_f15_4;
    f32 temp_f15_5;
    f32 temp_f15_6;
    f32 temp_f15_7;
    f32 temp_f15_8;
    f32 temp_f16;
    f32 temp_f16_10;
    f32 temp_f16_11;
    f32 temp_f16_2;
    f32 temp_f16_3;
    f32 temp_f16_4;
    f32 temp_f16_5;
    f32 temp_f16_6;
    f32 temp_f16_7;
    f32 temp_f16_8;
    f32 temp_f16_9;
    f32 temp_f17;
    f32 temp_f19;
    f32 temp_f1;
    f32 temp_f1_2;
    f32 temp_f1_3;
    f32 temp_f1_4;
    f32 temp_f1_5;
    f32 temp_f1_6;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f21;
    f32 temp_f23;
    f32 temp_f24;
    f32 temp_f25;
    f32 temp_f26;
    f32 temp_f26_2;
    f32 temp_f28;
    f32 temp_f28_2;
    f32 temp_f28_3;
    f32 temp_f2;
    f32 temp_f2_2;
    f32 temp_f30;
    f32 temp_f30_2;
    f32 temp_f31;
    f32 temp_f3;
    f32 temp_f3_2;
    f32 temp_f4;
    f32 temp_f4_2;
    f32 temp_f4_3;
    f32 temp_f5;
    f32 temp_f5_2;
    f32 temp_f6;
    f32 temp_f6_2;
    f32 temp_f6_3;
    f32 temp_f6_4;
    f32 temp_f6_5;
    f32 temp_f7;
    f32 temp_f7_2;
    f32 temp_f7_3;
    f32 temp_f8;
    f32 temp_f8_2;
    f32 temp_f8_3;
    f32 temp_f9;
    f32 temp_f9_2;
    s32 var_a2;
    s32 var_v0;

    sp4 = 0.0f;
    var_a2 = 0;
    sp0 = sp4;
    sp8 = sp4;
loop_1:
    temp_f7 = -M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 4);
    temp_f20 = sp0 - 1.0f;
    temp_f1 = sp0 + 1.0f;
    temp_f2 = sp4 + 1.0f;
    temp_f9 = temp_f7 * temp_f1;
    temp_f26 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x14) * temp_f20;
    temp_f11 = -M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 8);
    temp_f10 = -M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0);
    sp20 = temp_f1;
    temp_f0 = sp8 - 1.0f;
    temp_f3 = sp4 - 1.0f;
    temp_f12 = temp_f9 * temp_f2;
    temp_f15 = temp_f26 * temp_f2;
    temp_f16 = temp_f10 * temp_f1;
    temp_f6 = temp_f11 * temp_f1;
    sp58 = temp_f12;
    temp_f5 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x18) * temp_f20;
    sp5C = temp_f15;
    temp_f1_2 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x10) * temp_f20;
    temp_f28 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x24) * temp_f20;
    sp24 = temp_f16;
    temp_f12_2 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x34) * sp20;
    sp2C = temp_f1_2;
    temp_f1_3 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x28) * temp_f20;
    sp150 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x18) * temp_f2;
    sp64 = temp_f12_2;
    sp8C = temp_f1_3;
    sp88 = temp_f5 * temp_f2;
    temp_f16_2 = temp_f16 * temp_f2;
    sp60 = temp_f28 * temp_f3;
    sp28 = temp_f16_2;
    temp_f8 = temp_f6 * temp_f2;
    sp30 = temp_f1_2 * temp_f2;
    sp34 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x20) * temp_f20;
    temp_f15_2 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x44) * sp20;
    sp94 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x38) * sp20;
    sp6C = temp_f15_2;
    sp68 = temp_f12_2 * temp_f3;
    sp90 = temp_f1_3 * temp_f3;
    sp154 = sp150 * temp_f0;
    sp168 = temp_f6 * temp_f0;
    sp16C = temp_f5 * temp_f0;
    temp_f6_2 = sp34 * temp_f3;
    sp3C = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x30) * sp20;
    temp_f1_4 = sp8 + 1.0f;
    sp38 = temp_f6_2;
    sp110 = temp_f9 * temp_f0;
    sp74 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x54) * temp_f20;
    sp98 = sp94 * temp_f3;
    sp70 = sp6C * temp_f2;
    sp9C = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x48) * sp20;
    sp114 = sp110 + (temp_f26 * temp_f0);
    sp13C = sp58 + sp5C;
    spF0 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x34) * temp_f3;
    sp40 = sp3C * temp_f3;
    spAC = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x10) * temp_f2;
    sp44 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x40) * sp20;
    temp_f26_2 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x64) * temp_f20;
    temp_f23 = (sp168 + sp16C) - (sp8C * temp_f0);
    temp_f5_2 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x58) * temp_f20;
    spA0 = sp9C * temp_f2;
    sp78 = sp74 * temp_f2;
    spA4 = temp_f5_2;
    temp_f16_3 = spF0 * temp_f0;
    spF4 = temp_f16_3;
    temp_f15_3 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x44) * temp_f2;
    sp118 = sp114 - (temp_f28 * temp_f0);
    temp_f16_4 = sp64 * temp_f0;
    spF8 = temp_f15_3;
    sp11C = temp_f16_4;
    temp_f24 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x68) * temp_f20;
    temp_f4 = sp13C - sp60;
    sp174 = sp94 * temp_f0;
    sp140 = temp_f4;
    sp158 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x48) * temp_f2;
    temp_f16_5 = sp44 * temp_f2;
    spBC = sp24 * temp_f0;
    spB0 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x20) * temp_f3;
    sp48 = temp_f16_5;
    sp7C = temp_f26_2 * temp_f3;
    temp_f25 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x50) * temp_f20;
    temp_f28_2 = spA4 * temp_f2;
    sp80 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x74) * sp20;
    sp170 = temp_f23;
    sp120 = sp118 + temp_f16_4;
    temp_f16_6 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x54) * temp_f2;
    temp_f15_4 = temp_f15_3 * temp_f1_4;
    spFC = temp_f15_4;
    sp100 = temp_f16_6;
    temp_f15_5 = temp_f15_2 * temp_f1_4;
    sp124 = temp_f15_5;
    spC0 = spBC + (sp2C * temp_f0);
    sp144 = temp_f4 + sp68;
    sp178 = temp_f23 + sp174;
    temp_f14 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x78) * sp20;
    temp_f15_6 = sp80 * temp_f3;
    sp84 = temp_f15_6;
    sp15C = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x58) * temp_f2;
    spA8 = temp_f24 * temp_f3;
    sp4C = temp_f25 * temp_f2;
    temp_f6_3 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x30) * temp_f3;
    sp128 = sp120 + temp_f15_5;
    temp_f15_7 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x64) * temp_f3;
    spB4 = temp_f6_3;
    temp_f16_7 = temp_f16_6 * temp_f1_4;
    temp_f20_2 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x60) * temp_f20;
    sp104 = temp_f16_7;
    sp108 = temp_f15_7;
    temp_f16_8 = sp74 * temp_f1_4;
    sp12C = temp_f16_8;
    temp_f21 = sp178 + (sp9C * temp_f1_4);
    sp148 = sp144 + sp70;
    spDC = sp28 + sp30;
    spB8 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x40) * temp_f2;
    temp_f19 = temp_f21 - (temp_f5_2 * temp_f1_4);
    sp160 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x68) * temp_f3;
    spC4 = spC0 - (sp34 * temp_f0);
    sp50 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x70) * sp20;
    sp17C = temp_f21;
    temp_f6_4 = temp_f14 * temp_f3;
    sp130 = sp128 - temp_f16_8;
    temp_f30 = temp_f20_2 * temp_f3;
    sp10C = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x74) * temp_f3;
    temp_f31 = spC4 + (sp3C * temp_f0);
    sp134 = sp130 + (temp_f26_2 * temp_f1_4);
    sp184 = temp_f19 + (temp_f24 * temp_f1_4);
    sp164 = M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x78) * temp_f3;
    temp_f16_9 = sp80 * temp_f1_4;
    spC8 = temp_f31;
    sp180 = temp_f19;
    sp54 = sp50 * temp_f3;
    sp138 = temp_f16_9;
    temp_f28_3 = temp_f31 + (sp44 * temp_f1_4);
    spCC = temp_f28_3;
    temp_f17 = (spDC - sp38) + sp40;
    sp14C = (sp148 - sp78) + sp7C;
    spE0 = temp_f17;
    temp_f8_2 = (((((temp_f8 + sp88) - sp90) + sp98 + spA0) - temp_f28_2) + spA8) - temp_f6_4;
    temp_f11_2 = ((((((temp_f11 * temp_f2 * temp_f0) + sp154) - (M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x28) * temp_f3 * temp_f0)) + (M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x38) * temp_f3 * temp_f0) + (sp158 * temp_f1_4)) - (sp15C * temp_f1_4)) + (sp160 * temp_f1_4)) - (sp164 * temp_f1_4);
    temp_f13 = (((((((temp_f12 * temp_f0) + (temp_f15 * temp_f0)) - (sp60 * temp_f0)) + (sp68 * temp_f0) + (sp70 * temp_f1_4)) - (sp78 * temp_f1_4)) + (sp7C * temp_f1_4)) - (sp84 * temp_f1_4)) - M2C_FIELD(arg1, f32 *, 4);
    temp_f7_2 = ((((((temp_f7 * temp_f2 * temp_f0) + (M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x14) * temp_f2 * temp_f0)) - (M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x24) * temp_f3 * temp_f0)) + temp_f16_3 + temp_f15_4) - temp_f16_7) + (temp_f15_7 * temp_f1_4)) - (sp10C * temp_f1_4);
    sp14 = sp134 - temp_f16_9;
    spD0 = temp_f28_3 - (temp_f25 * temp_f1_4);
    temp_f4_2 = -temp_f13;
    temp_f16_10 = temp_f17 + temp_f16_5;
    sp1C = sp184 - (temp_f14 * temp_f1_4);
    spE4 = temp_f16_10;
    sp18 = sp14C - temp_f15_6;
    spD4 = spD0 + (temp_f20_2 * temp_f1_4);
    temp_f9_2 = (((((((temp_f16_2 * temp_f0) + (sp30 * temp_f0)) - (temp_f6_2 * temp_f0)) + (sp40 * temp_f0) + (sp48 * temp_f1_4)) - (sp4C * temp_f1_4)) + (temp_f30 * temp_f1_4)) - (sp54 * temp_f1_4)) - M2C_FIELD(arg1, f32 *, 0);
    temp_f12_3 = (((((((temp_f8 * temp_f0) + (sp88 * temp_f0)) - (sp90 * temp_f0)) + (sp98 * temp_f0) + (spA0 * temp_f1_4)) - (temp_f28_2 * temp_f1_4)) + (spA8 * temp_f1_4)) - (temp_f6_4 * temp_f1_4)) - M2C_FIELD(arg1, f32 *, 8);
    spE8 = temp_f16_10 - sp4C;
    temp_f10_2 = ((((((temp_f10 * temp_f2 * temp_f0) + (spAC * temp_f0)) - (spB0 * temp_f0)) + (temp_f6_3 * temp_f0) + (spB8 * temp_f1_4)) - (M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x50) * temp_f2 * temp_f1_4)) + (M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x60) * temp_f3 * temp_f1_4)) - (M2C_FIELD(((u8 *)&D_0046AF50), f32 *, 0x70) * temp_f3 * temp_f1_4);
    spD8 = sp50 * temp_f1_4;
    temp_f6_5 = (temp_f4_2 * temp_f8_2) + (sp18 * temp_f12_3);
    sp188 = sp18 * temp_f11_2;
    temp_f1_5 = (sp14 * temp_f8_2) - (sp18 * sp1C);
    temp_f30_2 = spE8 + temp_f30;
    temp_f14_2 = (-temp_f7_2 * temp_f12_3) + (temp_f13 * temp_f11_2);
    temp_f8_3 = (temp_f7_2 * temp_f8_2) - sp188;
    spEC = temp_f30_2;
    spC = spD4 - spD8;
    sp10 = temp_f30_2 - sp54;
    temp_f7_3 = (temp_f7_2 * sp1C) - (sp14 * temp_f11_2);
    temp_f11_3 = spC * temp_f14_2;
    sp194 = temp_f11_3;
    temp_f1_6 = ((temp_f10_2 * temp_f1_5) - (spC * temp_f8_3)) + (sp10 * temp_f7_3);
    temp_f0_2 = (((-temp_f9_2 * temp_f1_5) - (spC * temp_f6_5)) + (sp10 * ((temp_f4_2 * sp1C) + (sp14 * temp_f12_3)))) / temp_f1_6;
    temp_f4_3 = ((temp_f10_2 * temp_f6_5) + (temp_f9_2 * temp_f8_3) + (sp10 * temp_f14_2)) / temp_f1_6;
    temp_f3_2 = (((temp_f10_2 * ((-sp14 * temp_f12_3) + (temp_f13 * sp1C))) - temp_f11_3) - (temp_f9_2 * temp_f7_3)) / temp_f1_6;
    temp_f14_3 = sp0 + temp_f0_2;
    temp_f2_2 = fabsf(temp_f0_2);
    temp_f15_8 = sp4 + temp_f4_3;
    temp_f16_11 = sp8 + temp_f3_2;
    sp0 = temp_f14_3;
    M2C_FIELD(arg0, f32 *, 0) = temp_f14_3;
    sp4 = temp_f15_8;
    sp8 = temp_f16_11;
    M2C_FIELD(arg0, f32 *, 4) = temp_f15_8;
    M2C_FIELD(arg0, f32 *, 8) = temp_f16_11;
    if (!(temp_f2_2 < 1e-06f) || !(fabsf(temp_f4_3) < 1e-06f) || !(fabsf(temp_f3_2) < 1e-06f) || !(fabsf(temp_f14_3) <= 1.000001f) || !(fabsf(temp_f15_8) <= 1.000001f) || (var_v0 = 0, !(fabsf(temp_f16_11) <= 1.000001f))) {
        var_a2 += 1;
        if (var_a2 < 5) {
            goto loop_1;
        }
        var_v0 = -1;
    }
    return var_v0;
}
#undef M2C_UNK
#undef M2C_FIELD
#undef M2C_BITWISE
#else
LIT4_WORD(D_005F5730, 0x358637BD);
LIT4_WORD(D_005F5734, 0x358637BD);
LIT4_WORD(D_005F5738, 0x3F800008);
INCLUDE_ASM("asm/nonmatchings/demo", func_003E08D8);
#endif

void func_003E1330(f32 *p, s32 n) {
    f32 v[4];
    s32 i;
    f32 y;

    p += 3;
    for (i = 0; i < n; i++) {
        v[0] = p[0];
        if (0.0f < p[1]) {
            y = p[1];
        } else {
            y = 0.0f;
        }
        v[1] = y;
        v[2] = p[2];
        func_003E08D8(p, v);
        p += 6;
    }
}

void func_003E13C8(f32 r) {
    f32 n = -r;

    D_0046AF50[0] = r;
    D_0046AF50[1] = r;
    D_0046AF50[2] = n;
    D_0046AF50[4] = n;
    D_0046AF50[5] = r;
    D_0046AF50[6] = n;
    D_0046AF50[8] = n;
    ((s32 *)D_0046AF50)[9] = 0;
    D_0046AF50[10] = n;
    D_0046AF50[12] = r;
    ((s32 *)D_0046AF50)[13] = 0;
    D_0046AF50[14] = n;
    D_0046AF50[16] = r;
    D_0046AF50[17] = r;
    D_0046AF50[18] = r;
    D_0046AF50[20] = n;
    D_0046AF50[21] = r;
    D_0046AF50[22] = r;
    D_0046AF50[24] = n;
    ((s32 *)D_0046AF50)[25] = 0;
    D_0046AF50[26] = r;
    D_0046AF50[28] = r;
    ((s32 *)D_0046AF50)[29] = 0;
    D_0046AF50[30] = r;
}

/* Draws a line from a to b (GS fixed-point x, y, z words) in colour col (0xRRGGBBAA). */
void Gfx_DrawLine(s32 *a, s32 *b, u32 col) {
    u64 *p;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    *p++ = 0x5160C00000008001UL;
    *p++ = 0x441EE;
    *p++ = 0x7000D;
    *p++ = 0x48;
    *p++ = 0x44;
    *p++ = 0x43;
    *p++ = ((col >> 24) & 0xFF) | ((u64)((col >> 16) & 0xFF) << 32);
    *p++ = ((col >> 8) & 0xFF) | ((u64)(col & 0xFF) << 32);
    *p++ = a[0] | ((u64)a[1] << 32);
    *p++ = a[2] | 0x100000000UL;
    *p++ = b[0] | ((u64)b[1] << 32);
    *p++ = b[2] | 0x100000000UL;
    func_00399650(p);
}

/* Draws a line (x0, y0)-(x1, y1) at depth z with an extra A+D register value `reg` (0x48 = PRMODE? [I]). */
void func_003E1580(u32 x0, u32 y0, u32 x1, u32 y1, u32 z, u32 col, s32 reg) {
    u64 *p;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    *p++ = 0x5123400000008001UL;
    *p++ = 0x441EE;
    *p++ = 0x31001;
    *p++ = 0x48;
    *p++ = reg;
    *p++ = 0x43;
    *p++ = ((col >> 24) & 0xFF) | ((u64)((col >> 16) & 0xFF) << 32);
    *p++ = ((col >> 8) & 0xFF) | ((u64)(col & 0xFF) << 32);
    *p++ = x0 | ((u64)y0 << 32);
    *p++ = z | 0x100000000UL;
    *p++ = x1 | ((u64)y1 << 32);
    *p++ = z | 0x100000000UL;
    func_00399650(p);
}

/* Draws a four-point strip (v: four GS fixed-point vertices of 16 bytes) in colour col. */
void func_003E1700(s32 *v, u32 col, u32 reg) {
    u64 *p;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    *p++ = 0x7122400000008001UL;
    *p++ = 0x44441EE;
    *p++ = 0x31001;
    *p++ = 0x48;
    *p++ = reg;
    *p++ = 0x43;
    *p++ = ((col >> 24) & 0xFF) | ((u64)((col >> 16) & 0xFF) << 32);
    *p++ = ((col >> 8) & 0xFF) | ((u64)(col & 0xFF) << 32);
    *p++ = v[0] | ((u64)v[1] << 32);
    *p++ = 0x1000FFFF0UL;
    *p++ = v[4] | ((u64)v[5] << 32);
    *p++ = 0x1000FFFF0UL;
    *p++ = v[8] | ((u64)v[9] << 32);
    *p++ = 0x1000FFFF0UL;
    *p++ = v[12] | ((u64)v[13] << 32);
    *p++ = 0x1000FFFF0UL;
    func_00399650(p);
}

/* Draws a filled rectangle (x0, y0)-(x1, y1) in GS fixed-point coordinates, colour col (0xRRGGBBAA). */
void Gfx_DrawRect(u32 x0, u32 y0, u32 x1, u32 y1, u32 col) {
    u64 *p;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    *p++ = 0x5120C00000008001UL;
    *p++ = 0x441EE;
    *p++ = 0x71001;
    *p++ = 0x48;
    *p++ = 0x44;
    *p++ = 0x43;
    *p++ = ((col >> 24) & 0xFF) | ((u64)((col >> 16) & 0xFF) << 32);
    *p++ = ((col >> 8) & 0xFF) | ((u64)(col & 0xFF) << 32);
    *p++ = x0 | ((u64)y0 << 32);
    *p++ = 0xFFFF0;
    *p++ = x1 | ((u64)y1 << 32);
    *p++ = 0xFFFF0;
    func_00399650(p);
}

/* Draws a triangle (v: three GS fixed-point vertices x, y, z of 16 bytes) in colour col. */
void Gfx_DrawTriangle(s32 *v, u32 col) {
    u64 *p;

    p = func_00399520();
    if (p == NULL) {
        return;
    }
    *p++ = 0x6121C00000008001UL;
    *p++ = 0x4441EE;
    *p++ = 0x31001;
    *p++ = 0x48;
    *p++ = 0x44;
    *p++ = 0x43;
    *p++ = ((col >> 24) & 0xFF) | ((u64)((col >> 16) & 0xFF) << 32);
    *p++ = ((col >> 8) & 0xFF) | ((u64)(col & 0xFF) << 32);
    *p++ = v[0] | ((u64)v[1] << 32);
    *p++ = v[2] | 0x100000000UL;
    *p++ = v[4] | ((u64)v[5] << 32);
    *p++ = v[6] | 0x100000000UL;
    *p++ = v[8] | ((u64)v[9] << 32);
    *p++ = v[10] | 0x100000000UL;
    func_00399650(p);
}

void func_003E1B40(void) {
    s32 t;

    D_01FC4A40.unk0 = 0;
    D_01FC4A40.unk2 = 0;
    D_01FC4A40.unk6 = 0;
    D_01FC4A40.unk24 = (rand() & 0xFF) + 0xC0;
    t = (rand() & 0xFF) + 0x60;
    D_01FC4A40.unk34 = 0;
    D_01FC4A40.unk2C = D_01FC4A40.unk30 = 0.0f;
    D_01FC4A40.unk28 = t;
}

void func_003E1BB0(void) {
    D_01FC4A40.unk0 = 2;
    D_01FC4A40.unk2 = 0;
}

void func_003E1BC8(void) {
    s32 i;

    D_01FC4A40.unk0 = 3;
    D_01FC4A40.unk6 = 0;
    for (i = 0; i < 3; i++) {
        D_01FC4A40.unk40[i] = func_0034A570(rand());
    }
    for (i = 0; i < 4; i++) {
        D_01FC4A30[i] = Text_Get(i + 0xCC);
    }
}
