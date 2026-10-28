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
void func_003DFF30(f32 a, u32 x, u32 y, u32 size, u32 tile, u32 col);
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
/* NON_MATCHING: 204 lines: typed from the asm (T19 F02; the m2c draft in gotos-only form passed uninitialised
 * locals as the text, the window x and the entry count, dropped func_003DFA00 arguments and bit-cast the scroll
 * bar's float quotient); same structure and calls, other saved registers (s8 / s3 / s4 / s5) and stack slots for the spilled locals */
extern u32 D_00415718[];
extern u32 D_00415728[];
extern u8 D_00522B90[];
extern u8 D_005F6910[];
void Snd_PlaySe(s32 id, s32 side);
s32 func_003DB9A0(u8 *s);
void func_003DBAA8(s32 x0, s32 x1, s32 y0, s32 y1);
void func_003DC188(u32 x, u32 y, u32 w, u32 h, u32 z);

/* The practice command list window of `side` (pad `port`): title, up to four entries of two rows scrolled by the
 * pad (unk8 the first entry, unkC the scroll offset in pixels easing to 0), and a scroll bar past four entries. */
void func_003DC9C8(s32 side, s32 port) {
    Fighter *f;
    PracCmdText *t;
    TeamMember *m;
    u8 *text;
    u8 *s;
    u32 n;
    u32 trig;
    u32 held;
    s32 x;
    s32 y;
    s32 top;
    s32 few;
    s32 col;
    s32 d;
    s32 v;
    s32 off;
    s32 neg;
    s32 k;
    s32 e;
    s32 yy;
    s32 bar;
    s32 q;
    s32 cnt;
    s32 i;

    f = &gFighters[side];
    t = &gPracCmdText[f->unk22];
    y = 0x59;
    if (D_0043B7F8[0] == 7) {
        y = 0x3B;
    }
    text = t->text;
    top = y + 0x32;
    n = text[0];
    trig = D_00415728[port];
    held = D_00415718[port];
    x = 0x150;
    if (side == 0) {
        x = 0x14;
    }
    col = 0;
    if (n == 0) {
        return;
    }
    func_003DBCF0(x - 8, y - 8, 0x12C, 0x3A, 0xFFFE, 0x40405073);
    few = n < 5;
    func_003DFA00(D_00522B90, 0x60606080, 0x118, x, y + 0x20, 0);
    func_003DFA00(D_005F6910, x + 0x33, y, 0x60606080);
    text = func_003DFA00(text + 1);
    m = &D_00440940[side][f->unk0];
    D_005F6738 = 0x80808080;
    D_005F6730 = 0xFFFF0;
    func_003CBFB8(func_003CF230(m->unk0, m->unk4), x - 3, y - 4, 0);
    if (!few) {
        d = 0;
        if (trig == 0x4000) {
            Snd_PlaySe(0x103F, 0);
            d = 1;
        }
        if (trig == 0x1000) {
            Snd_PlaySe(0x103F, 0);
            d = -1;
        }
        if (!(held & 0x5000)) {
            if (trig == 0x2000) {
                Snd_PlaySe(0x103F, 0);
                d = 4;
            } else if (trig == 0x8000) {
                Snd_PlaySe(0x103F, 0);
                d = -4;
            }
        }
        t->unkC += d * 0x48;
        v = ((u32)t->unk8 < n) * t->unk8 + d;
        if (v < 0) {
            v += n;
        }
        if ((u32)v >= n) {
            v -= n;
        }
        t->unk8 = v;
    }
    off = t->unkC;
    neg = 0;
    if (off != 0) {
        if (off < 0) {
            neg = 1;
            off = -off;
        }
        off -= (off + 3) / 3;
        if (off <= 0) {
            off = 0;
        }
        if (neg) {
            off = -off;
        }
    }
    t->unkC = off;
    k = off / 72 + (off > 0);
    e = t->unk8 - k;
    yy = off + (top - k * 0x48);
    bar = top + 0x120;
    while (e < 0) {
        e += n;
    }
    while ((u32)e >= n) {
        e -= n;
    }
    s = text;
    for (i = 0; i < e * 2; i++) {
        if (s[0] == 0xFC && s[1] < 0xFE) {
            col = s[1] - 1;
        }
        s = Str_Next(s);
    }
    func_003DBAA8(0, 0x27F, top, bar);
    if (!few) {
        q = 0x480 / n;
        bar = (t->unk8 * 0x48 - t->unkC) * 4;
        if (bar != 0) {
            bar = (f32)bar / (f32)n;
        }
        func_003DBCF0(x + 0x11E, top, 6, 0x120, 0xFFFD, 0xFFFFFF80);
        bar += top;
        cnt = 5;
        func_003DBCF0(x + 0x11E, bar, 6, q, 0xFFFF, 0xB0B0B080);
        func_003DBCF0(x + 0x11E, bar - 0x120, 6, q, 0xFFFF, 0xB0B0B080);
    } else {
        cnt = n;
        func_003DBCF0(x - 8, n * 0x48 + yy, 0x12C, (4 - n) * 0x48, 0xFFFE, D_004564F0[0].c1);
    }
    for (i = 0; i < cnt; i++) {
        if (s[0] == 0xFC && s[1] < 0xFE) {
            col = s[1] - 1;
            s += 2;
        }
        if (func_003DB9A0(s) > 0) {
            func_003DBCF0(x - 8, yy, 0x12C, 0x12, 0xFFFE, D_004564F0[col].c0);
            func_003DBCF0(x - 8, yy + 0x12, 0x12C, 0x36, 0xFFFE, D_004564F0[col].c1);
        } else {
            func_003DBCF0(x - 8, yy, 0x12C, 0x48, 0xFFFE, D_004564F0[col].c1);
        }
        e++;
        func_003DFA00(D_005F6918, x, yy);
        s = func_003DFA00(s);
        func_003DFA00(D_005F6918, x + 2, yy + 0x18);
        s = func_003DFA00(s);
        if ((u32)e >= n) {
            s = text;
            e = 0;
            col = 0;
        }
        yy += 0x48;
    }
    func_003DBAA8(0, 0x27F, 0, 0x1BF);
    func_003DC188(x, y, 0x11C, 0x14A, 0xFFFF);
}
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
/* NON_MATCHING: 494 lines: typed draft written after the asm in T29 batch 5 (the m2c draft read the scratch matrix's
 * translation from a separate local `sp30` and wrote back only x of the scale vector: the bones 7-0x1C of the head
 * and the upper body got garbage translations). Stack layout and register allocation not compared. */
void Mtx_Unit(void *);
void *func_00364298(void *);
s32 func_003B87B0(s16);
s32 func_003B87F8(s16);
void sceVu0InterVector(void *d, void *a, void *b, f32 t);
s32 sceVu0MulVector(void *, void *, f32 *);
extern u8 D_0046AFD0[];

/* Bones 7-0x1C of fighter f (local matrices at f + 0x87D0 + i * 0x40, from 0x8990): without an animation record
 * (func_00364298) they are set from the model's bone table (offsets, and the angles written into the angle table);
 * else each bone is blended (weight f + 0xB090) towards the record's rotation and / or translation; bones 13 / 14
 * also take a scale (the blended scale vectors at f + 0xB0D0 / 0xB110, the matrices kept at 0xB0A0 / 0xB0E0). */
void func_003DD918(void *fv) {
    u8 *f = fv;
    Vu0Matrix m;
    f32 s[4] __attribute__((aligned(16)));
    f32 *ang;
    u8 *p;
    u8 *d;
    s32 *bones;
    f32 *src;
    f32 k;
    f32 fk;
    s32 i;

    ang = (f32 *)func_003B87F8(*(s16 *)(f + 0x20));
    if (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
        return;
    }
    Mtx_Unit(m);
    k = *(f32 *)(f + 0xB090);
    p = func_00364298(f);
    if (p == NULL) {
        bones = (s32 *)(func_003B87B0(*(s16 *)(f + 0x22)) + 0xC);
        d = f + 0x8990;
        for (i = 7; i < 0x1D; i++, d += 0x40) {
            src = (f32 *)bones[i];
            *(f32 *)(f + (i << 6) + 0x8800) = src[1];
            *(f32 *)(f + (i << 6) + 0x8804) = src[2];
            *(f32 *)(f + (i << 6) + 0x8808) = src[3];
            ang[i * 3 + 0] = src[4];
            ang[i * 3 + 1] = src[5];
            ang[i * 3 + 2] = src[6];
            func_00349D70(d, ang + i * 3);
        }
        return;
    }
    d = f + 0x8990;
    for (i = 7; i < 0x1D; i++, d += 0x40) {
        switch (i - 7) {
        case 0:  /* rotation only */
        case 14:
        case 15:
        case 18:
        case 19:
            func_003DD788(m, (s16 *)(p + (i == 7 ? 0 : i == 21 ? 0x2A : i == 22 ? 0x30 : i == 25 ? 0x36 : 0x3C)));
            func_003DD8E8(d, m, d, k);
            break;
        case 12: /* translation only */
        case 13:
        case 16:
        case 17:
        case 20:
        case 21:
            func_003DD890((u8 *)m, (s16 *)(p + (i == 19 ? 0x4E : i == 20 ? 0x54 : i == 23 ? 0x42 : i == 24 ? 0x48
                                                 : i == 27 ? 0x5A : 0x60)), *(s32 *)(f + 0x500));
            sceVu0InterVector(d + 0x30, m[3], d + 0x30, k);
            break;
        case 10: /* rotation and translation from the record */
        case 11:
            func_003DD788(m, (s16 *)(p + (i == 17 ? 0x72 : 0x78)));
            func_003DD890((u8 *)m, (s16 *)(p + (i == 17 ? 0x66 : 0x6C)), *(s32 *)(f + 0x500));
            func_003DD8E8(d, m, d, k);
            sceVu0InterVector(d + 0x30, m[3], d + 0x30, k);
            break;
        case 1: /* the bone's own angles, turned by the record, and a translation */
        case 2:
        case 3:
        case 4:
        case 5:
        case 8:
        case 9: {
            s32 r = i == 8 ? 0xAE : i == 9 ? 0xB4 : i == 10 ? 0x06 : i == 11 ? 0x96 : i == 12 ? 0x9C : i == 15 ? 0x7E
                                                                                                             : 0x84;
            func_00349D70(m, ang + i * 3);
            func_003DD800((u8 *)m, (u16 *)(p + r), *(s32 *)(f + 0x500));
            func_003DD890((u8 *)m, (s16 *)(p + (i == 10 ? 0x0C : r + 0xC)), *(s32 *)(f + 0x500));
            func_0034A180(d, m, d, k);
            sceVu0InterVector(d + 0x30, m[3], d + 0x30, k);
            break;
        }
        case 6: /* bone 13: rotation and the blended scale (0xB0D0) */
            func_003DD788(m, (s16 *)(p + 0x1E));
            func_003DD8E8(d, m, f + 0xB0A0, k);
            *(u128 *)(f + 0xB0A0) = *(u128 *)(d + 0x00);
            *(u128 *)(f + 0xB0B0) = *(u128 *)(d + 0x10);
            *(u128 *)(f + 0xB0C0) = *(u128 *)(d + 0x20);
            fk = (f32)*(s32 *)(f + 0x500);
            s[0] = (f32)(s32)*(u16 *)(p + 0x12) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            s[1] = (f32)(s32)*(u16 *)(p + 0x14) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            s[3] = 1.0f;
            s[2] = (f32)(s32)*(u16 *)(p + 0x16) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            sceVu0InterVector(s, s, f + 0xB0D0, k);
            *(u128 *)(f + 0xB0D0) = *(u128 *)s;
            sceVu0MulVector(d, d, s);
            sceVu0MulVector(d + 0x10, d + 0x10, s);
            sceVu0MulVector(d + 0x20, d + 0x20, s);
            break;
        case 7: /* bone 14: rotation, its own scale (0xB110), and the fighter's reference vector scaled by 0xB0D0's */
            func_003DD788(m, (s16 *)(p + 0x24));
            func_003DD8E8(d, m, f + 0xB0E0, k);
            *(u128 *)(f + 0xB0E0) = *(u128 *)(d + 0x00);
            *(u128 *)(f + 0xB0F0) = *(u128 *)(d + 0x10);
            *(u128 *)(f + 0xB100) = *(u128 *)(d + 0x20);
            fk = (f32)*(s32 *)(f + 0x500);
            s[0] = (f32)(s32)*(u16 *)(p + 0x18) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            s[1] = (f32)(s32)*(u16 *)(p + 0x1A) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            s[3] = 1.0f;
            s[2] = (f32)(s32)*(u16 *)(p + 0x1C) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            sceVu0InterVector(s, s, f + 0xB110, k);
            *(u128 *)(f + 0xB110) = *(u128 *)s;
            sceVu0MulVector(d, d, s);
            sceVu0MulVector(d + 0x10, d + 0x10, s);
            sceVu0MulVector(d + 0x20, d + 0x20, s);
            fk = (f32)*(s32 *)(f + 0x500);
            s[0] = (f32)(s32)*(u16 *)(p + 0x12) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            s[1] = (f32)(s32)*(u16 *)(p + 0x14) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            s[2] = (f32)(s32)*(u16 *)(p + 0x16) * (1.0f / 4096.0f) * fk * (1.0f / 4096.0f);
            sceVu0InterVector(s, s, f + 0xB0D0, k);
            sceVu0MulVector(d + 0x30, D_0046AFD0 + *(s16 *)(f + 0x20) * 16, s);
            *(f32 *)(d + 0x3C) = 1.0f;
            break;
        }
    }
}
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
/* NON_MATCHING: 328 lines: typed from the asm (T33 b1; was an m2c draft that never read its variadic arguments);
 * the original keeps the state in s1-s7 / fp and its switch is the jump table jtbl_00522D20 */
#ifdef PORT
#if defined(__x86_64__) /* x86-64 (TTT_CC=clang64): the SysV convention passes them in registers first: a real va_list */
#define DFA_AP(fmt) __builtin_va_list ap; __builtin_va_start(ap, fmt)
#define DFA_ARG() __builtin_va_arg(ap, s32)
#else
#define DFA_AP(fmt) u8 *ap = DFA_START(fmt)
#endif
/* i386 cdecl: the variadic arguments are 4-byte stack slots right after fmt. */
#ifndef DFA_ARG
#define DFA_ARG() (ap += 4, *(s32 *)(ap - 4))
#endif
#ifdef __clang__ /* no __builtin_next_arg; an i386 va_list is the address of the first variadic slot */
#define DFA_START(fmt) ({ __builtin_va_list v_; __builtin_va_start(v_, fmt); (u8 *)v_; })
#else
#define DFA_START(fmt) ((u8 *)__builtin_next_arg(fmt))
#endif
#else
#define DFA_ARG() (ap += 8, *(s32 *)(ap - 8))
#define DFA_START(fmt) ((u8 *)__builtin_next_arg(fmt) - 7 * 8) /* fake: the EABI register save area */
#define DFA_AP(fmt) u8 *ap = DFA_START(fmt)
#endif
s32 exit(s32);

/* Command printer of the command font: 0xFF starts a command (H / V: x / y, h / v: x * 16 / y * 20, i: colour,
 * w: line width, L: highlighted icon count, s: a string argument, n: new line), 0xFC 0xFF right-aligns the rest,
 * 0xDC-0xEB / 0xEC-0xFB / 0xDB are button icons; the position, line start and colour persist in D_005F7060-68
 * for func_003E06A0. Returns the end of fmt (past its terminator). */
u8 *func_003DFA00(u8 *fmt, ...) {
    DemoTextPos t;
    DFA_AP(fmt);
    u8 *s;
    u8 *next;
    u8 *str;
    s32 n;      /* sp10: the spaces of the line */
    s32 sw;     /* sp14: the space width */
    s32 right;  /* sp18: right-aligned */
    s32 spaces; /* fp */
    s32 paren;  /* s7 */
    s32 esc;    /* s6 */
    s32 k;      /* s5: glyph index */
    u32 c;      /* s1 */
    u16 cnt;

    t.x0 = D_005F7060[0];
    t.y0 = D_005F7060[1];
    t.g.x = D_005F7064[0];
    t.g.y = D_005F7064[1];
    *(u32 *)t.g.c = D_005F7068;
    spaces = 0;
    paren = 0;
    esc = 0;
    k = 0;
    right = 0;
    D_005F6934 = 0;
    func_003DECE0();
    sw = func_003DF808(&n, (char *)fmt, D_005F6930);
    c = *fmt;
    s = fmt + 1;
    while (c != 0) {
        next = s + 1;
        if (c == 0xFC) {
            c = *s++;
            next = s + 1;
            if (c == 0xFF) {
                right = 1;
                t.g.x = t.x0 + D_005F6930 - PracFont_StrWidth(s);
            }
            goto tail;
        }
        if (c == 0xFF) {
            if (esc != 1) {
                esc = 1;
                goto tail;
            }
            esc = 0;
        }
        if (esc != 0) {
            esc = 0;
            switch (c) {
            case 'n':
                t.g.y = t.y0 += 20;
                if (right != 0) {
                    t.g.x = t.x0 + D_005F6930 - PracFont_StrWidth(s);
                } else {
                    t.g.x = t.x0;
                }
                break;
            case 's':
                str = (u8 *)DFA_ARG();
                c = *str++;
                while (c != 0) {
                    PracFont_DrawGlyph(&t.g, c, k);
                    k++;
                    c = *str++;
                }
                break;
            case 'H':
                t.g.x = t.x0 = DFA_ARG();
                break;
            case 'V':
                t.g.y = t.y0 = DFA_ARG();
                break;
            case 'h':
                t.g.x = t.x0 = DFA_ARG() << 4;
                break;
            case 'v':
                t.g.y = t.y0 = DFA_ARG() * 20;
                break;
            case 'w':
                D_005F6930 = DFA_ARG();
                break;
            case 'i':
                k = 0;
                *(u32 *)t.g.c = DFA_ARG();
                break;
            case 'L':
                D_005F6932 = DFA_ARG();
                break;
            default:
                exit(0);
                break;
            }
            goto tail;
        }
        if (c - 0xDC < 0x10) {
            c -= 0xDC;
            k = 0;
            if (paren == 0 && D_005F6932 != 0) {
                cnt = --D_005F6932;
                if (cnt == 0) {
                    D_005F6934 = c | 0x10;
                }
                c |= 0x10;
            }
            func_003DF500(t.g.x, t.g.y + 2, c);
            t.g.x += 0x10;
        } else if (c - 0xEC < 0x10) {
            c -= 0xEC;
            k = 0;
            if (paren == 0 && D_005F6932 != 0) {
                cnt = --D_005F6932;
                if (cnt == 0) {
                    D_005F6934 = c | 0x20;
                }
                c |= 0x10;
            }
            func_003DF150(t.g.x, t.g.y + 2, c);
            t.g.x += 0x14;
        } else if (c == 0xDB) {
            func_003DF500(t.g.x, t.g.y + 2, 0x20);
            t.g.x += 0xE;
        } else if (c == ' ') {
            if (++spaces == n) {
                t.g.x = t.x0;
                t.g.y = t.y0 += 20;
                spaces = 0;
                sw = func_003DF808(&n, (char *)s, D_005F6930);
            } else {
                t.g.x += sw;
            }
        } else {
            PracFont_DrawGlyph(&t.g, c, k);
            k++;
            if (c == '(') {
                paren = 1;
            } else if (c == ')') {
                paren = 0;
            }
            t.g.x += D_004572E0[c - 0x20];
        }
    tail:
        c = *s;
        s = next;
    }
    D_005F7060[0] = t.x0;
    D_005F7060[1] = t.y0;
    D_005F7064[0] = t.g.x;
    D_005F7064[1] = t.g.y;
    D_005F7068 = *(u32 *)t.g.c;
    func_003DED90();
    return s;
}
#undef DFA_ARG
#undef DFA_START
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
/* NON_MATCHING: 344 lines: rewritten from the asm (T41 b0; the m2c draft took the angle ($f12) as no argument, five
 * ints for six and lost the corner arithmetic), register allocation and scheduling not tried. */
f32 cosf(f32);
f32 sinf(f32);
/* One textured sprite of the menu font sheet rotated by `a` about (x, y) (12.4 fixed point): `size` scales the half
 * width (size * 256 / 200), tile - 1 picks the 16 x 15 cell (u = (t & 15) * 256, v = ((t >> 6) * 15 + 4) * 16), col
 * is RGBA. The y terms are scaled by 480 and back, as the original does. */
void func_003DFF30(f32 a, u32 x, u32 y, u32 size, u32 tile, u32 col) {
    u64 *p;
    f32 r;
    f32 n;
    f32 s;
    f32 c;
    f32 fx;
    f32 fy;
    s32 x0, y0, x1, y1, x2, y2, x3, y3;
    u32 u;
    u32 v;

    r = (f32)((size << 8) / 200U);
    p = func_00399520();
    if (p == NULL) {
        return;
    }
    tile--;
    s = sinf(a);
    c = cosf(a);
    n = -r;
    fx = (f32)x;
    fy = (f32)y;
    x0 = fx + (c * n - s * n);
    y0 = fy + (s * n + c * n) * 480.0f / 480.0f;
    x1 = fx + (c * n - s * r);
    y1 = fy + (s * n + c * r) * 480.0f / 480.0f;
    x2 = fx + (c * r - s * n);
    y2 = fy + (s * r + c * n) * 480.0f / 480.0f;
    x3 = fx + (c * r - s * r);
    y3 = fy + (s * r + c * r) * 480.0f / 480.0f;
    u = (tile & 0xF) << 8;
    v = ((tile >> 6) * 15 + 4) << 4;
    *p++ = (0xC1AA4000ULL << 32) | 0x8001;
    *p++ = (((0x4343ULL << 16) | 0x4343) << 16) | 0x1EEE;
    *p++ = ((tile & 0x30) << 2) | 4;
    *p++ = 0x1C;
    *p++ = (((0x20D76806ULL << 16) | 0x2141) << 16) | 0x3B40;
    *p++ = 7;
    *p++ = 0x44;
    *p++ = 0x43;
    *p++ = ((col >> 24) & 0xFF) | ((u64)((col >> 16) & 0xFF) << 32);
    *p++ = ((col >> 8) & 0xFF) | ((u64)(col & 0xFF) << 32);
    *p++ = (u + 8) | ((u64)(v + 8) << 32);
    *p++ = 0;
    *p++ = (s64)x0 | ((u64)(u32)y0 << 32);
    *p++ = 0x1FFFFFFFFULL;
    *p++ = (u + 8) | ((u64)(v + 0xE8) << 32);
    *p++ = 0;
    *p++ = (s64)x1 | ((u64)(u32)y1 << 32);
    *p++ = 0x1FFFFFFFFULL;
    *p++ = (u + 0xF8) | ((u64)(v + 8) << 32);
    *p++ = 0;
    *p++ = (s64)x2 | ((u64)(u32)y2 << 32);
    *p++ = 0x1FFFFFFFFULL;
    *p++ = (u + 0xF8) | ((u64)(v + 0xE8) << 32);
    *p++ = 0;
    *p++ = (s64)x3 | ((u64)(u32)y3 << 32);
    *p++ = 0x1FFFFFFFFULL;
    func_00399650(p);
}
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
