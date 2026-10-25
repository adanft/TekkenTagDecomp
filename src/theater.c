/* 0x3543F0-0x355D30 (module theater, docs/game_overview.md section 4; docs/systems/modes.md). */
#include "common.h"

/* A movie entry of the theater list (D_004562F0, 41 x 12 bytes). */
typedef struct {
    /* 0x0 */ u8 pad0[5];
    /* 0x5 */ u8 unk5;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s16 x;
    /* 0xA */ s16 y;
} ThEntry;

typedef struct {
    u8 id[40];
} ThIds;

/* A frame rectangle with four corner colours (0x24 bytes). */
typedef struct {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 w;
    /* 0x0C */ s32 h;
    /* 0x10 */ s32 z;
    /* 0x14 */ u32 c0;
    /* 0x18 */ u32 c1;
    /* 0x1C */ u32 c2;
    /* 0x20 */ u32 c3;
} ThRect;

/* The layout of one theater tile (func_00354730). */
typedef struct {
    /* 0x00 */ ThRect f;
    /* 0x24 */ s32 x;
    /* 0x28 */ s32 y;
    /* 0x2C */ s32 w;
    /* 0x30 */ s32 h;
    /* 0x34 */ s32 z;
    /* 0x38 */ u32 c[4];
    /* 0x48 */ ThRect s;
    /* 0x6C */ s32 ix;
    /* 0x70 */ s32 iy;
    /* 0x74 */ u8 pad74[0x1C];
    /* 0x90 */ s32 icon;
    /* 0x94 */ s32 iconKind;
} ThTile;

typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 unk8;
    /* 0xC */ s32 unkC;
    /* 0x10 */ s32 unk10; /* the row of the cursor's entry (Mode_Theater) */
    /* 0x14 */ s32 unk14;
} Th1FC4910;

extern s16 gMode;
extern ThEntry D_004562F0[];
extern const ThIds D_00522AD0;
extern Th1FC4910 D_01FC4910;
extern u8 D_0043B800[];
extern u8 D_0043B968[];
extern s32 D_005F68E8;
extern s16 D_004564C0[];
extern s32 D_004564E0[];
extern u8 D_005F7055;
extern u8 D_005F7056;
extern u8 D_005F7057;
extern u8 D_005F7058;
extern u8 D_005F7059;
extern u8 D_005F705A;
extern u8 D_005F705C;
extern s16 gSubMode;
extern u8 D_005F705B;
extern s32 D_005F704C;
extern s32 D_005F6730;
extern s32 D_00455E50[];
extern s8 D_0051B678[];
extern const s8 D_005227F8[];

s32 func_003BFDC8(void);
void Bgm_Stop(void);
void Unlock_ApplyAll(void);
void func_003B4B18(void);
s32 func_003ADA08(u8 *set, s32 id);
void func_003992C8(s32 a, s32 b, s32 c);
void Mode_EnterTheater(void);
s32 func_003BFDD8(s32 id);
s32 func_003CBFA0(s32 i);
void Sprite_DrawScaled(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);
s32 func_003543F8(s32 i);
s32 func_003544C8(s32 a, s32 b);
u32 func_00354600(u32 c, f32 k);
void func_00354730(ThTile *t, ThEntry *e, u32 a);
void Mode_Theater(void);

/* The original calls __fixunssfsi for a float to u32 conversion; this compiler emits fptoui (decomp_guide.md, T17 b4). */
#ifndef PORT
__asm__(".set fptoui, __fixunssfsi"); /* fake */
#endif

INCLUDE_RODATA("asm/nonmatchings/theater", D_005221E0);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005221F8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522208);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522218);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522228);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522238);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522248);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522258);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522268);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522278);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522288);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522298);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005222A8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005222B8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005222C8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005222D8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005222E8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005222F8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522308);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522318);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522328);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522338);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522348);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522358);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522368);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522378);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522388);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522398);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005223A8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005223B8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005223C8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005223D8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005223E8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005223F8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522408);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522418);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522428);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522438);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522448);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522458);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522468);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522478);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522488);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522498);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005224A8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005224B8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005224C8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005224D8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005224E8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005224F8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522508);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522518);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522528);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522538);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522548);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522558);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522568);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522578);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522588);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522598);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005225A8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005225B8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005225C8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005225D8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005225E8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005225F8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522608);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522618);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522628);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522638);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522648);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522658);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522668);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522678);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522688);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522698);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005226A8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005226B8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005226C8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005226D8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005226E8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005226F8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522708);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522718);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522728);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522738);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522748);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522758);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522768);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522778);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522788);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522798);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005227A8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005227B8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005227C8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005227D8);
INCLUDE_RODATA("asm/nonmatchings/theater", D_005227E8);
PORT_RET(u8) func_003543F0(void) {
    return D_005F705B;
}

/* Movie id -> word of D_00455E50; the ids 0x1C0-0x1D7 map to three blocks of 8 entries. */
s32 func_003543F8(s32 i) {
    if ((u32)(i - 0x1C0) < 8) {
        return D_00455E50[i % 8 + 0x110];
    }
    if ((u32)(i - 0x1C8) < 8) {
        return D_00455E50[i % 8 + 0x118];
    }
    if ((u32)(i - 0x1D0) < 8) {
        return D_00455E50[i % 8 + 0x120];
    }
    return D_00455E50[i];
}

INCLUDE_RODATA("asm/nonmatchings/theater", D_005227F8);
/* State of movie `b` (0-5) of row `a`: 0 locked, 1 available, 2 available with another variant [I]. */
s32 func_003544C8(s32 a, s32 b) {
    s32 r;
    s32 i;
    s8 x;
    s8 p;
    s8 q;

    r = a * 8;
    switch (a) {
    case 0x38:
        if (b != 3) {
            return 1;
        }
        return func_003BFDC8() != 0;
    case 0x39:
    case 0x3A:
        return 1;
    }
    i = r + b;
    if (func_003543F8(i) != 0) {
        x = D_0051B678[i];
        p = D_005227F8[r];
        q = D_005227F8[i];
        if (x != -1 && func_003BFDD8(x) != 0) {
            if (p == -1 || q == p) {
                return 1;
            }
            return 2;
        }
    }
    return 0;
}

/* 1 when any of the six entries of row `a` is available. */
s32 func_003545A8(s32 a) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (func_003544C8(a, i) != 0) {
            return 1;
        }
    }
    return 0;
}

/* Scales the three high bytes of an RGBA word by k, clamped to 255; the low byte is kept. */
u32 func_00354600(u32 c, f32 k) {
    u32 r;
    u32 g;
    u32 b;

    r = (f32)((c >> 24) & 0xFF) * k;
    g = (f32)((c >> 16) & 0xFF) * k;
    b = (f32)((c >> 8) & 0xFF) * k;
    if (b > 255) {
        b = 255;
    }
    if (g > 255) {
        g = 255;
    }
    if (r > 255) {
        r = 255;
    }
    return (r << 24) | ((g & 0xFF) << 16) | ((b & 0xFF) << 8) | (c & 0xFF);
}

/* Lays out the tile of movie entry e (D_004562F0) in t: frame, shadow and the four corner colours scaled by
 * a / 128 (a = 0x80 full); the special entries 0x38-0x3A (movies / music player) get their own sizes and icon
 * (D_004564E0 + ...) [I]. */
#if 0
/* NON_MATCHING: 244 lines: same blocks, store order and registers differ (t0 / t7 for e, the constants) */
void func_00354730(ThTile *t, ThEntry *e, u32 a) {
    f32 k;
    s32 x;
    s32 y;

    if (a == 0x80) {
        k = 1.0f;
    } else {
        k = (f32)a * (1.0f / 64.0f);
    }
    switch (e->unk6) {
    case 0x38:
        t->x = e->x - 0x35;
        t->w = 0x68;
        t->h = 0x34;
        t->y = e->y + 0x34;
        t->z = 0xFFFA;
        t->c[0] = func_00354600(0x60208080, k);
        t->c[1] = func_00354600(0x200A2A80, k);
        t->c[2] = func_00354600(0x30104080, k);
        t->c[3] = func_00354600(0x18082080, k);
        x = t->x;
        y = t->y;
        t->f.w = t->w + 4;
        t->f.z = 0xFFF9;
        t->f.c2 = 0x80808080;
        t->f.c3 = 0xD0D0D080;
        t->s.x = x - 2;
        t->s.y = y - 2;
        t->s.w = t->w + 6;
        t->s.h = t->h + 4;
        t->ix = x + 0xA;
        t->iy = y + 0x26;
        t->f.x = x - 2;
        t->f.y = y - 2;
        t->f.h = t->h + 4;
        t->f.c0 = 0xD0D0D080;
        t->f.c1 = 0x80808080;
        t->icon = D_004564E0[0];
        t->iconKind = 4;
        return;
    case 0x39:
        t->x = e->x;
        t->w = 0x68;
        t->h = 0x34;
        t->y = e->y + 0x34;
        t->z = 0xFFFA;
        t->c[0] = func_00354600(0x30104080, k);
        t->c[1] = func_00354600(0x18082080, k);
        t->c[2] = func_00354600(0x60208080, k);
        t->c[3] = func_00354600(0x200A2A80, k);
        x = t->x;
        y = t->y;
        t->f.w = t->w + 3;
        t->f.z = 0xFFF9;
        t->f.c2 = 0x80808080;
        t->f.c3 = 0xD0D0D080;
        t->s.x = x - 2;
        t->s.y = y - 2;
        t->s.w = t->w + 7;
        t->s.h = t->h + 4;
        t->ix = x + 0xA;
        t->iy = y + 0x26;
        t->f.x = x - 2;
        t->f.y = y - 2;
        t->f.h = t->h + 4;
        t->f.c0 = 0xD0D0D080;
        t->f.c1 = 0x80808080;
        t->icon = D_004564E0[1];
        t->iconKind = 4;
        return;
    case 0x3A:
        t->x = e->x;
        t->w = 0x33;
        t->h = 0x34;
        t->y = e->y + 0x34;
        t->z = 0xFFFA;
        if (e->x != D_004564C0[0]) {
            t->c[0] = func_00354600(0x30104080, k);
            t->c[1] = func_00354600(0x60208080, k);
            t->c[2] = func_00354600(0x18082080, k);
            t->c[3] = func_00354600(0x200A2A80, k);
        } else {
            t->c[0] = func_00354600(0x18082080, k);
            t->c[1] = func_00354600(0x200A2A80, k);
            t->c[2] = func_00354600(0x30104080, k);
            t->c[3] = func_00354600(0x60208080, k);
        }
        x = t->x;
        y = t->y;
        t->f.w = t->w + 4;
        t->f.z = 0xFFF9;
        t->f.c2 = 0x80808080;
        t->f.c3 = 0xD0D0D080;
        t->s.x = x - 2;
        t->s.y = y - 2;
        t->s.w = t->w + 6;
        t->s.h = t->h + 5;
        t->ix = x + 2;
        t->iy = y + 0x26;
        t->f.x = x - 2;
        t->f.y = y - 2;
        t->f.h = t->h + 5;
        t->f.c0 = 0xD0D0D080;
        t->f.c1 = 0x80808080;
        t->iconKind = 3;
        t->icon = D_004564E0[2];
        return;
    default:
        t->x = e->x;
        x = e->x - 2;
        y = e->y + 0x32;
        t->s.h = 0x39;
        t->y = e->y + 0x34;
        t->w = 0x35;
        t->h = 0x36;
        t->f.z = 0xFFF9;
        t->f.c2 = 0x80808080;
        t->f.c3 = 0xD0D0D080;
        t->s.x = x;
        t->s.y = y;
        t->s.w = 0x37;
        t->f.x = x;
        t->f.y = y;
        t->f.w = 0x37;
        t->f.h = 0x39;
        t->f.c0 = 0xD0D0D080;
        t->f.c1 = 0x80808080;
        return;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/theater", func_00354730);
#endif

/* Mode 12: set-up of the theater, then mode 13. Fills the 41 movie entries of D_004562F0 from D_00522AD0 (the
 * entry index is read for i = 40 too, one byte past the 40-byte copy, as in the original). */
#if 0
/* NON_MATCHING: 4 lines: the original keeps 1 and -1 in a2 / a1 for the stores and the call's arguments */
void Mode_EnterTheater(void) {
    ThIds t;
    ThEntry *list;
    ThEntry *e;
    s32 i;
    u8 v;

    i = 0;
    Bgm_Stop();
    D_0043B800[0] = 0;
    D_005F7056 = 1;
    D_005F7055 = 0;
    D_005F7057 = 0;
    D_005F7058 = 0;
    D_005F7059 = 0;
    D_005F705A = 0;
    D_005F705B = 0;
    gSubMode = 0;
    D_005F705C = 0;
    func_003B4B18();
    gMode = 13;
    Unlock_ApplyAll();
    list = D_004562F0;
    t = D_00522AD0;
    e = list;
    do {
        e->unk5 = 1;
        v = t.id[i];
        switch (v) {
        case 0x38:
        case 0x39:
        case 0x3A:
            e->unk6 = v;
            break;
        default:
            if (func_003ADA08(D_0043B968, v) != 0) {
                e->unk6 = v;
            } else {
                e->unk6 = 0x36;
            }
            break;
        }
        i++;
        e++;
    } while (i < 41);
    D_01FC4910.unkC = 1;
    list[40].unk5 = 0;
    D_01FC4910.unk4 = 0x24;
    D_005F68E8 = -1;
    list[7].unk5 = 0;
    list[0].unk5 = 0;
    D_01FC4910.unk8 = 1;
    func_003992C8(0, -1, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/theater", Mode_EnterTheater);
#endif

INCLUDE_RODATA("asm/nonmatchings/theater", D_00522908);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522A18);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522A28);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522A48);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522A58);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522A68);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522A78);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522A88);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522AA0);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522AB0);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522AC0);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522AD0);
INCLUDE_RODATA("asm/nonmatchings/theater", D_00522AF8);
#if 0
/* NON_MATCHING: about 1000 lines: rewritten from the asm (T41 b0), register allocation and block order not tried. The
 * button table at sp+0x10 shares its stack with the tile in the original. */
s32 Input_SkipPressed(void);
void Mode_Change(s32);
void Mode_EnterEnding(s32 ret, s32 a, s32 b);
void Mode_EnterLoading(s16 next);
void Snd_PlaySe(s32 id, s32 player);
void Sprite_SetBlend(s32 a, s32 b);
void Sprite_SetFilter(s32 a, s32 b);
s32 Text_Printf(const char *fmt, ...);
void Ui_DrawWindow(s32 x, s32 y, s32 w, s32 h, s32 z, u32 c0, u32 c1);
s32 func_00366460(s32 n);
void func_0039C7D8(u32 movie);
void func_003A9678(s32);
s32 func_003B1378(void);
void func_003B8A50(void);
void func_003BF2F8(u8 *p, u16 *alpha);
void func_003C4868(void);
void func_003DBE98(u32 x, u32 y, u32 w, u32 h, u32 z, u32 c0, u32 c1, u32 c2, u32 c3);
void func_00355C88(s32 id, s32 x, s32 y);
void Theater_OpenMusicPlayer(s32 arg);
s32 func_003545A8(s32 a);
extern u8 D_0043B7F8[];
extern s32 D_0043B838[];
extern const s8 D_00522908[];
extern const char D_00522A18[];
extern const char D_00522A28[];
extern const char D_00522A48[];
extern const char D_00522A58[];
extern const char D_00522A68[];
extern const char D_00522A78[];
extern const char D_00522A88[];
extern const char D_00522AA0[];
extern const char D_00522AB0[];
extern const char D_00522AC0[];
extern const char D_00522AF8[];
extern u16 D_005F5C76;
extern s32 D_005F5CDC;
extern s32 D_005F5CE0;
extern u16 D_005F62AC;
extern u64 D_005F6738;
extern u8 D_005F6906;
extern s16 D_005F6908; /* read as a byte, negated as a halfword */
extern u8 D_005F705D;

/* Mode 13: the theater menu. A grid of 41 movie tiles (D_004562F0), the selected row's movies with their unlock
 * buttons below, and the THEATER title; a chosen movie plays as an ending, 0x1C0-0x1D7 are the special rows. */
void Mode_Theater(void) {
    ThTile t;
    ThEntry *e;
    s32 btn[5];
    s32 rep;
    s32 pad;
    s32 dir;
    s32 cur;
    s32 next;
    s32 cancel;
    s32 i;
    s32 j;
    s32 n;
    s32 row;
    s32 base;
    s32 count;
    s32 avail;
    s32 shown;
    s32 chosen;
    s32 sel;
    s32 idx;
    s32 mask;
    s32 name;
    s32 bits;
    s32 bit;
    s32 k;
    s32 col;
    s32 x;
    s32 y;
    s32 id;
    u32 a;
    u8 v;

    rep = 0;
    pad = 0;
    if (D_005F7056 == 0) {
        if (func_003B1378() != 0) {
            Mode_Change(3);
            return;
        }
    } else {
        D_005F7056 = 0;
        func_003B8A50();
    }
    if (D_005F7055 == 0) {
        func_003992C8(0, 0, 0);
        D_005F62AC += 4;
        if (D_005F7057 == 0 && D_005F7058 == 0) {
            if (D_005F705C == 0) {
                pad = D_005F5CDC;
                rep = D_005F5CE0;
            } else {
                D_005F705C = 0;
            }
        } else {
            D_005F705C = 0;
        }
        if ((s8)D_005F7059 < 0) {
            D_005F7059++;
        }
        if ((s8)D_005F7057 < 0) {
            D_005F7057++;
        }
        if ((s8)D_005F7058 < 0) {
            D_005F7058++;
        }
    } else {
        D_005F62AC -= 4;
    }
    if (pad & 0x100) {
        pad = 0x100;
        rep = 0;
        D_005F7055 = 1;
        Snd_PlaySe(5, 0);
    }
    if (D_005F705A == 0) {
        if (D_005F705B == 0) {
            D_005F6730 = 0xFFF70;
            func_003C4868();
        }
        D_005F6730 = 0xFFF80;
        Sprite_SetBlend(1, 0);
        func_003BF2F8(D_0043B7F8, &D_005F62AC);
    }
    Input_SkipPressed();
    if (D_005F7055 == 0) {
        Text_Printf(D_00522AF8, 0xF0, 0x28, 0xA, 5, 0xD);
        /* Cursor: the d-pad (repeat, else pressed) moves along the entry's links, skipping locked tiles (0x36). */
        dir = rep & 0xA000;
        cur = D_01FC4910.unk4;
        if (dir == 0) {
            dir = pad & 0x5000;
        }
        switch (dir) {
        case 0x8000:
            next = ((u8 *)&D_004562F0[cur])[3];
            break;
        case 0x2000:
            next = ((u8 *)&D_004562F0[cur])[4];
            break;
        case 0x1000:
            next = ((u8 *)&D_004562F0[cur])[1];
            break;
        case 0x4000:
            next = ((u8 *)&D_004562F0[cur])[2];
            break;
        default:
            next = -1;
            break;
        }
        cancel = pad & 0x8F8;
        if (next >= 0) {
            D_005F68E8 = -1;
            while (next != 0x28 && D_004562F0[next].unk6 == 0x36) {
                switch (dir) {
                case 0x8000:
                    next = ((u8 *)&D_004562F0[next])[3];
                    break;
                case 0x2000:
                    next = ((u8 *)&D_004562F0[next])[4];
                    break;
                case 0x1000:
                    next = ((u8 *)&D_004562F0[next])[1];
                    break;
                case 0x4000:
                    next = ((u8 *)&D_004562F0[next])[2];
                    break;
                }
            }
            if (next != 0x28 && next != cur) {
                D_01FC4910.unk4 = next;
                D_01FC4910.unk14 = 0;
                Snd_PlaySe(3, 0);
                cur = D_01FC4910.unk4;
            }
        }
        D_01FC4910.unk10 = D_004562F0[cur].unk6;

        /* The tiles. */
        Sprite_SetFilter(1, 1);
        Sprite_SetBlend(1, 1);
        e = D_004562F0;
        for (i = 0; i < 0x29; i++, e++) {
            if (e->unk5 == 0) {
                continue;
            }
            id = e->unk6;
            if (id == 0xF) {
                id = 0x24;
            }
            if (id == 0x36 || func_003545A8(e->unk6) != 0) {
                a = 0x80;
                if (D_01FC4910.unk4 == i && D_01FC4910.unk8 == 1) {
                    a = 0xA0;
                }
            } else {
                a = 0x30;
            }
            func_00354730(&t, e, a);
            func_003DBE98(t.f.x, t.f.y, t.f.w, t.f.h, t.f.z, t.f.c0, t.f.c1, t.f.c2, t.f.c3);
            switch (e->unk6) {
            case 0x38:
            case 0x3A:
                func_003DBE98(t.x, t.y, t.w, t.h, t.z, t.c[0], t.c[1], t.c[2], t.c[3]);
                Text_Printf(D_00522AC0, t.ix, t.iy, 0xA, t.iconKind, 0xD, t.icon);
                break;
            case 0x39:
                if (D_005F5C76 != 0) {
                    func_003DBE98(t.x, t.y, t.w, t.h, t.z, t.c[0], t.c[1], t.c[2], t.c[3]);
                    Text_Printf(D_00522AC0, t.ix, t.iy, 0xA, t.iconKind, 0xD, t.icon);
                } else {
                    func_003DBE98(t.x, t.y, t.w, t.h, t.z, 0x20202080, 0x20202080, 0x20202080, 0x20202080);
                    Text_Printf(D_00522AC0, t.ix, t.iy, 0xA, t.iconKind, 0x16, t.icon);
                }
                break;
            default:
                D_005F6738 = (u64)a | 0x80000000ULL | ((u64)a << 16) | ((u64)a << 8);
                func_00355C88(id, t.x, t.y);
                break;
            }
        }

        /* The selected row: how many of its movies are open, then the list. */
        chosen = -1;
        avail = 0;
        shown = 0;
        sel = D_01FC4910.unk10;
        base = sel * 8;
        count = func_00366460(sel);
        Ui_DrawWindow(0x40, 0x122, 0x200, 0x78, 0xFFFA, 0x8080, 0x1080);
        if (sel == 0x38) {
            avail = (func_003BFDC8() != 0) ? 4 : 3;
            shown = 4;
        } else if (sel < 0x38 || sel >= 0x3B) {
            for (i = 0, n = 0; n < count && i < 6; i++) {
                if (func_003543F8(base + i) != 0) {
                    shown++;
                    n++;
                    if (func_003544C8(sel, i) != 0) {
                        avail++;
                    }
                }
            }
        }
        if (avail != 0) {
            Text_Printf(D_00522A18, 0xFFFE0, 0x208, 0x19A, 6, avail, shown);
        }
        if (func_003545A8(sel) != 0) {
            switch (sel) {
            case 0x39:
                if (D_005F5C76 != 0) {
                    v = 1;
                } else {
                    Text_Printf(D_00522A28, 0xFFFE0, 0xAC, 0x136, 0xA, 4, 0x13, 0xC);
                    v = 0;
                }
                break;
            case 0x38:
            case 0x3A:
                v = 1;
                break;
            default:
                v = func_003ADA08(D_0043B968, sel);
                break;
            }
            if (v != 0) {
                if (pad & 0x80) {
                    mask = 1;
                } else if (pad & 0x40) {
                    mask = 2;
                } else if (pad & 0x10) {
                    mask = 4;
                } else if (pad & 0x20) {
                    mask = 8;
                } else if (pad & 0x800) {
                    mask = 0x10;
                } else {
                    mask = (pad << 2) & 0x20;
                }
                for (i = 0, n = 0; n < count && i < 6; i = next) {
                    idx = base + i;
                    name = func_003543F8(idx);
                    btn[0] = 1 << i;
                    btn[1] = 0;
                    btn[2] = 0;
                    btn[3] = 0;
                    btn[4] = 0;
                    bits = (idx < 0x110) ? D_00522908[idx] : 0;
                    bit = 1;
                    k = 1;
                    while (bits != 0) {
                        if (bits & 1) {
                            btn[k++] = bit;
                        }
                        bits >>= 1;
                        bit <<= 1;
                    }
                    next = i + 1;
                    if (name == 0) {
                        continue;
                    }
                    switch (func_003544C8(sel, i)) {
                    case 1:
                        col = 0x13;
                        break;
                    case 2:
                        col = 0xB;
                        break;
                    default:
                        col = 0x16;
                        break;
                    }
                    if (sel >= 0x39 && sel < 0x3B) {
                        if (pad & 0x8FC) {
                            D_005F68E8 = base;
                            chosen = base;
                            Snd_PlaySe(5, 0);
                        }
                    } else if (mask & (btn[0] | btn[4] | (btn[2] | btn[1]) | btn[3])) {
                        if (func_003544C8(sel, i) != 0) {
                            D_005F68E8 = idx;
                            chosen = idx;
                            Snd_PlaySe(5, 0);
                            col = 0x10;
                        } else {
                            chosen = -1;
                        }
                    }
                    if (sel == 0x38 && i == 3) {
                        col = 0xB;
                    }
                    if (D_005F68E8 == idx) {
                        col = 0x10;
                    }
                    row = n + 1;
                    if (sel == 0x38 && i == 3 && func_003BFDC8() == 0) {
                        n = row;
                        continue;
                    }
                    if (sel >= 0x3B || sel < 0x39) {
                        y = row * 0x14 + 0x122;
                        x = 0x48;
                        for (j = 0; j < 5; j++) {
                            switch (btn[j]) {
                            case 1:
                                Text_Printf(D_00522A48, 0xFFFE0, x, y, 0xB, 8);
                                x += 0x10;
                                break;
                            case 2:
                                Text_Printf(D_00522A58, 0xFFFE0, x, y, 0xB, 8);
                                x += 0x10;
                                break;
                            case 4:
                                Text_Printf(D_00522A68, 0xFFFE0, x, y, 0xB, 8);
                                x += 0x10;
                                break;
                            case 8:
                                Text_Printf(D_00522A78, 0xFFFE0, x, y, 0xB, 8);
                                x += 0x10;
                                break;
                            case 16:
                                Text_Printf(D_00522A88, 0xFFFE0, x + j * 0x10, y, 0xA, 3, 5);
                                x = 0x48;
                                y += 0x14;
                                break;
                            case 32:
                                Text_Printf(D_00522AA0, 0xFFFE0, x + j * 0x10, y, 0xB, 9);
                                x += 0x20;
                                break;
                            }
                        }
                    }
                    Text_Printf(D_00522AB0, 0xFFFE0, 0xA8, row * 0x14 + 0x122, 0xA, 4, col, name);
                    n = row;
                }
            }
        }
        if (cancel != 0 && chosen == -1) {
            Snd_PlaySe(3, 0);
        }

        /* The pulsing highlight over the cursor's tile. */
        v = D_005F6906 + (u8)D_005F6908;
        D_005F6906 = v;
        e = &D_004562F0[D_01FC4910.unk4];
        if ((u8)(v - 0x60) >= 0x91) {
            D_005F6908 = -(u16)D_005F6908;
        }
        if (e->unk5 != 0 && D_01FC4910.unk8 == 1) {
            func_00354730(&t, e, 0x80);
            D_005F6730 = 0xFFFD0;
            Sprite_SetFilter(1, 1);
            Sprite_SetBlend(1, 0);
            D_005F6738 = (u64)D_005F6906 | 0x80000000ULL | ((u64)D_005F6906 << 16) | ((u64)D_005F6906 << 8);
            Sprite_DrawScaled(1, (t.s.w < 0x3B) ? 0x73 : 0x76, 0x75, t.s.x, t.s.y, t.s.w, t.s.h);
        }

        /* The chosen movie. */
        if (chosen != -1) {
            switch (chosen) {
            case 0x108:
            case 0x109:
                D_005F705D = 2;
                D_005F68E8 = -1;
                D_005F705B = 1;
                D_005F7059 = 1;
                D_005F705C = 1;
                D_005F7056 = 1;
                break;
            case 0x1C0:
                D_005F68E8 = -1;
                D_005F705B = 1;
                D_005F7059 = 1;
                D_005F705D = 0;
                D_005F705C = 1;
                D_005F7056 = 1;
                break;
            case 0x1C1:
                D_005F68E8 = -1;
                D_005F705B = 1;
                D_005F7059 = 1;
                D_005F705D = 1;
                D_005F705C = 1;
                D_005F7056 = 1;
                break;
            case 0x1C2:
                D_005F705B = 1;
                D_005F7057 = 1;
                D_005F705A = 1;
                break;
            case 0x1C3:
                D_005F705B = 1;
                D_005F7058 = 1;
                D_005F705A = 1;
                break;
            case 0x1C8:
                Theater_OpenMusicPlayer(0xD);
                D_005F68E8 = -1;
                break;
            case 0x1D0:
                D_005F68E8 = -1;
                D_005F7055 = 1;
                break;
            default:
                D_005F705B = 1;
                D_0043B838[0] = chosen;
                Mode_EnterEnding(0xD, chosen, chosen);
                D_005F7056 = 1;
                D_005F68E8 = -1;
                break;
            }
        }
    } else if ((s16)D_005F62AC <= 0) {
        func_003992C8(1, 0, 0);
        Mode_Change(3);
    }
    if ((s8)D_005F7059 > 0) {
        if ((s8)D_005F7059 < 4) {
            D_005F7059++;
        } else if ((s8)D_005F7059 == 4) {
            D_005F7059 = -2;
            D_005F705C = 1;
            D_005F705A = 0;
            func_0039C7D8(D_005F705D);
            D_005F7056 = 1;
            D_005F68E8 = -1;
        }
    }
    switch ((s8)D_005F7057) {
    case 1:
        D_005F7057++;
        break;
    case 2:
        D_005F705A = 0;
        D_005F7057 = -2;
        func_003992C8(1, 0, 0);
        func_003A9678(0);
        Mode_EnterLoading(0xD);
        D_005F7056 = 1;
        D_005F68E8 = -1;
        break;
    }
    switch ((s8)D_005F7058) {
    case 1:
        D_005F7058++;
        break;
    case 2:
        D_005F705A = 0;
        D_005F7058 = -2;
        func_003992C8(1, 0, 0);
        func_003A9678(1);
        Mode_EnterLoading(0xD);
        D_005F7056 = 1;
        D_005F68E8 = -1;
        break;
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/theater", jtbl_00522B10);
INCLUDE_ASM("asm/nonmatchings/theater", Mode_Theater);
#endif

void func_00355C88(s32 id, s32 x, s32 y) {
    D_005F6730 = 0xFFFA0;
    if (id == 0x36) {
        Sprite_DrawScaled(1, 0xA5, 0xA3, x, y, 0x33, 0x34);
        return;
    }
    Sprite_DrawScaled(1, func_003CBFA0(id), 0x84, x, y, 0x33, 0x34);
}

/* Opens the music player (mode 14) with `arg`. */
void Theater_OpenMusicPlayer(s32 arg) {
    D_005F704C = arg;
    gMode = 14;
}
