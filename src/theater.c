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
__asm__(".set fptoui, __fixunssfsi"); /* fake */

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
u8 func_003543F0(void) {
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
/* NON_MATCHING: 1057 lines: m2c draft (switch trees, many stack locals) */
#define M2C_UNK u8
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(expr))
s32 Input_SkipPressed();
void Mode_Change(s32);
void Mode_EnterEnding(s32, s32, s32);
void Mode_EnterLoading(s32);
void Snd_PlaySe(s32, s32, ...);
void Sprite_SetBlend(s32, s32);
void Sprite_SetFilter(s32, s32);
s32 Text_Printf(void *, s32, s32, s32);
void Ui_DrawWindow(s32, s32, s32, s32);
s32 func_00366460(s32);
void func_0039C7D8(u8);
void func_003A9678(s32);
s32 func_003B1378();
void func_003B8A50();
void func_003BF2F8(void *, u16 *);
void func_003C4868();
void func_003DBE98(s32, s32, s32, s32);
extern u8 D_0043B7F8[];
extern s32 D_0043B838[];
extern u8 D_00522908[];
extern u8 D_00522A18[];
extern u8 D_00522A28[];
extern u8 D_00522A48[];
extern u8 D_00522A58[];
extern u8 D_00522A68[];
extern u8 D_00522A78[];
extern u8 D_00522A88[];
extern u8 D_00522AA0[];
extern u8 D_00522AB0[];
extern u8 D_00522AC0[];
extern u8 D_00522AF8[];
extern u16 D_005F5C76;
extern s32 D_005F5CDC;
extern s32 D_005F5CE0;
extern u16 D_005F62AC;
extern s64 D_005F6738;
extern u8 D_005F6906;
extern u8 D_005F6908;
extern u8 D_005F705D;

void Mode_Theater(void) {
    s32 sp34, sp38, sp3C, sp40, sp7C, sp80, sp60, sp58;
    u8 sp[0x200];
    s32 subroutine_arg0, subroutine_arg1, subroutine_arg2, subroutine_arg3, subroutine_arg4, subroutine_arg5;
    s32 sp10;
    s32 sp14;
    s32 sp18;
    s32 sp1C;
    s32 sp20;
    s32 spB0;
    s32 spB4;
    s32 spB8;
    s32 spBC;
    s32 spC0;
    s32 spC4;
    s32 spC8;
    M2C_UNK *temp_a1;
    M2C_UNK *var_a0_2;
    M2C_UNK *var_s2;
    s16 temp_s0;
    s16 temp_s0_2;
    s16 temp_v1;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_a2_3;
    s32 temp_s3;
    s32 temp_s4;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 var_a1_2;
    s32 var_a3;
    s32 var_fp;
    s32 var_s0;
    s32 var_s0_2;
    s32 var_s0_3;
    s32 var_s0_4;
    s32 var_s1_2;
    s32 var_s1_3;
    s32 var_s1_4;
    s32 var_s2_2;
    s32 var_s2_3;
    s32 var_s2_4;
    s32 var_s3;
    s32 var_s4;
    s32 var_s6;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v0_4;
    s32 var_v1_2;
    s64 var_s1;
    s8 var_a0;
    s8 var_v1;
    u8 temp_v1_3;
    u8 var_a1;
    void *temp_v1_2;

    var_s0 = 0;
    spB0 = 0;
    if (D_005F7056 == 0) {
        if (func_003B1378() != 0) {
            Mode_Change(3);
        } else {
            goto block_4;
        }
    } else {
        D_005F7056 = 0;
        func_003B8A50();
block_4:
        if (D_005F7055 == 0) {
            func_003992C8(0, 0, 0);
            D_005F62AC += 4;
            if (D_005F7057 == 0) {
                var_v1 = D_005F7058;
                if ((D_005F7058 == 0) && (D_005F705C == 0)) {
                    var_s0 = D_005F5CE0;
                    spB0 = D_005F5CDC;
                } else {
                    goto block_10;
                }
            } else {
                var_v1 = (s8) (u8) D_005F7058;
block_10:
                D_005F705C = 0;
            }
            if (D_005F7059 < 0) {
                D_005F7059 = (u8) D_005F7059 + 1;
            }
            var_v0 = var_v1 << 0x18;
            if (D_005F7057 & 0x80) {
                D_005F7057 += 1;
                var_v0 = var_v1 << 0x18;
            }
            if (var_v0 < 0) {
                D_005F7058 = var_v1 + 1;
            }
        } else {
            D_005F62AC -= 4;
        }
        if (spB0 & 0x100) {
            spB0 = 0x100;
            var_s0 = 0;
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
            func_003BF2F8(((u8 *)&D_0043B7F8), ((u8 *)&D_005F62AC));
        }
        Input_SkipPressed();
        if (D_005F7055 == 0) {
            Text_Printf(((u8 *)&D_00522AF8), 0xF0, 0x28, 0xA);
            temp_a2 = var_s0 & 0xA000;
            var_a3 = M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 4);
            temp_a2_2 = (temp_a2 == 0) ? (spB0 & 0x5000) : temp_a2;
            switch (temp_a2_2) {                    /* switch 1; irregular */
            case 0x8000:                            /* switch 1 */
                var_a1 = M2C_FIELD(((var_a3 * 0xC) + ((u8 *)&D_004562F0)), u8 *, 3);
block_36:
                D_005F68E8 = -1;
                if (var_a1 != 0x28) {
                    if (M2C_FIELD(((var_a1 * 0xC) + ((u8 *)&D_004562F0)), s16 *, 6) == 0x36) {
                        spC8 = spB0 & 0x8F8;
loop_39:
                        switch (temp_a2_2) {        /* switch 2; irregular */
                        case 0x8000:                /* switch 2 */
                            var_a1 = M2C_FIELD(((var_a1 * 0xC) + ((u8 *)&D_004562F0)), u8 *, 3);
                            break;
                        case 0x2000:                /* switch 2 */
                            var_a1 = M2C_FIELD(((var_a1 * 0xC) + ((u8 *)&D_004562F0)), u8 *, 4);
                            break;
                        case 0x1000:                /* switch 2 */
                            var_a1 = M2C_FIELD(((var_a1 * 0xC) + ((u8 *)&D_004562F0)), u8 *, 1);
                            break;
                        case 0x4000:                /* switch 2 */
                            var_a1 = M2C_FIELD(((var_a1 * 0xC) + ((u8 *)&D_004562F0)), u8 *, 2);
                            break;
                        }
                        if (var_a1 != 0x28) {
                            if (M2C_FIELD(((var_a1 * 0xC) + ((u8 *)&D_004562F0)), s16 *, 6) != 0x36) {
                                goto block_54;
                            }
                            goto loop_39;
                        }
                    } else {
                        spC8 = spB0 & 0x8F8;
block_54:
                        if ((var_a1 != 0x28) && (var_a1 != var_a3)) {
                            M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 4) = (s32) var_a1;
                            M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 0x14) = 0;
                            Snd_PlaySe(3, 0, temp_a2_2, var_a3);
                            var_a3 = M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 4);
                        }
                    }
                } else {
                    spC8 = spB0 & 0x8F8;
                }
                var_v1_2 = var_a3 * 0xC;
                break;
            case 0x2000:                            /* switch 1 */
                var_a1 = M2C_FIELD(((var_a3 * 0xC) + ((u8 *)&D_004562F0)), u8 *, 4);
                goto block_36;
            case 0x1000:                            /* switch 1 */
                var_a1 = M2C_FIELD(((var_a3 * 0xC) + ((u8 *)&D_004562F0)), u8 *, 1);
                goto block_36;
            case 0x4000:                            /* switch 1 */
                var_a1 = M2C_FIELD(((var_a3 * 0xC) + ((u8 *)&D_004562F0)), u8 *, 2);
                goto block_36;
            default:                                /* switch 1 */
                var_v1_2 = var_a3 * 0xC;
                spC8 = spB0 & 0x8F8;
                break;
            }
            M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 0x10) = (s32) M2C_FIELD((var_v1_2 + ((u8 *)&D_004562F0)), s16 *, 6);
            Sprite_SetFilter(1, 1);
            var_s2 = ((u8 *)&D_004562F0);
            Sprite_SetBlend(1, 1);
            var_s3 = 0;
            do {
                if (M2C_FIELD(var_s2, u8 *, 5) != 0) {
                    temp_s0 = M2C_FIELD(var_s2, s16 *, 6);
                    temp_s0_2 = ((temp_s0 ^ 0xF) == 0) ? 0x24 : temp_s0;
                    if ((temp_s0_2 == 0x36) || (func_003545A8(temp_s0) != 0)) {
                        var_s1 = 0x80;
                        if (M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 4) == var_s3) {
                            var_s1 = ((M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 8) ^ 1) == 0) ? 0xA0 : 0x80;
                        }
                    } else {
                        var_s1 = 0x30;
                    }
                    func_00354730(&sp10, var_s2, var_s1);
                    func_003DBE98(sp10, sp14, sp18, sp1C);
                    temp_v1 = M2C_FIELD(var_s2, s16 *, 6);
                    switch (temp_v1) {              /* switch 3; irregular */
                    case 0x3A:                      /* switch 3 */
                    case 0x38:                      /* switch 3 */
                        func_003DBE98(sp34, sp38, sp3C, sp40);
block_76:
                        Text_Printf(((u8 *)&D_00522AC0), sp7C, sp80, 0xA);
                        break;
                    case 0x39:                      /* switch 3 */
                        if (D_005F5C76 != 0) {
                            func_003DBE98(sp34, sp38, sp3C, sp40);
                        } else {
                            func_003DBE98(sp34, sp38, sp3C, sp40);
                        }
                        goto block_76;
                    default:                        /* switch 3 */
                        D_005F6738 = var_s1 | (0x8000 << 0x10) | ((var_s1 << 0x10) | (var_s1 << 8));
                        func_00355C88(temp_s0_2, sp34, sp38);
                        break;
                    }
                }
                var_s3 += 1;
                var_s2 += 0xC;
            } while (var_s3 < 0x29);
            var_fp = -1;
            var_s2_2 = 0;
            temp_s3 = M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 0x10);
            spB4 = temp_s3 * 8;
            spB8 = func_00366460(temp_s3);
            Ui_DrawWindow(0x40, 0x122, 0x200, 0x78);
            if (temp_s3 != 0x38) {
                if ((temp_s3 >= 0x38) && (temp_s3 < 0x3B)) {

                } else {
                    var_s1_2 = 0;
                    var_s0_2 = 0;
                    if (spB8 > 0) {
loop_86:
                        if (func_003543F8(spB4 + var_s1_2) != 0) {
                            var_s0_2 += 1;
                            var_s2_2 = (func_003544C8(temp_s3, var_s1_2) != 0) ? (var_s2_2 + 1) : var_s2_2;
                        }
                        var_s1_2 += 1;
                        if ((var_s0_2 < spB8) && (var_s1_2 < 6)) {
                            goto loop_86;
                        }
                    }
                }
            } else {
                var_s2_2 = (func_003BFDC8() != 0) ? 4 : 3;
            }
            if (var_s2_2 != 0) {
                Text_Printf(((u8 *)&D_00522A18), 0xFFFE0, 0x208, 0x19A);
            }
            if (func_003545A8((s16) temp_s3) != 0) {
                switch (temp_s3) {                  /* switch 4; irregular */
                case 0x39:                          /* switch 4 */
                    if (D_005F5C76 != 0) {
                    case 0x3A:                      /* switch 4 */
                    case 0x38:                      /* switch 4 */
                        var_v0_2 = 1;
                    } else {
                        Text_Printf(((u8 *)&D_00522A28), 0xFFFE0, 0xAC, 0x136);
                        var_v0_2 = 0;
                    }
                    break;
                default:                            /* switch 4 */
                    var_v0_2 = func_003ADA08(((u8 *)&D_0043B968), temp_s3);
                    break;
                }
                if (var_v0_2 != 0) {
                    var_v0_3 = 1;
                    if (!(spB0 & 0x80)) {
                        if (spB0 & 0x40) {
                            spBC = 2;
                        } else if (spB0 & 0x10) {
                            spBC = 4;
                        } else if (spB0 & 0x20) {
                            spBC = 8;
                        } else if (spB0 & 0x800) {
                            spBC = 0x10;
                        } else {
                            var_v0_3 = (spB0 * 4) & 0x20;
                            goto block_114;
                        }
                    } else {
block_114:
                        spBC = var_v0_3;
                    }
                    var_s1_3 = 0;
                    var_s0_3 = 0;
                    if (spB8 > 0) {
loop_117:
                        temp_s4 = spB4 + var_s1_3;
                        spC0 = func_003543F8(temp_s4);
                        sp14 = 0;
                        sp18 = 0;
                        sp10 = 1 << var_s1_3;
                        sp1C = 0;
                        sp20 = 0;
                        if (temp_s4 >= 0x110) {
                            var_a0 = 0;
                        } else {
                            var_a0 = *(temp_s4 + ((u8 *)&D_00522908));
                        }
                        var_s2_3 = 1;
                        var_a1_2 = 1;
                        if (var_a0 != 0) {
                            var_s6 = var_s1_3 + 1;
                            do {
                                if (var_a0 & 1) {
                                    temp_v1_2 = sp + (var_a1_2 * 4);
                                    var_a1_2 += 1;
                                    M2C_FIELD(temp_v1_2, s32 *, 0x10) = var_s2_3;
                                }
                                var_a0 = (s8) (var_a0 >> 1);
                                var_s2_3 *= 2;
                            } while (var_a0 != 0);
                        } else {
                            var_s6 = var_s1_3 + 1;
                        }
                        if (spC0 != 0) {
                            temp_v0 = func_003544C8(temp_s3, var_s1_3);
                            switch (temp_v0) {      /* switch 5; irregular */
                            case 1:                 /* switch 5 */
                                break;
                            case 2:                 /* switch 5 */
                                break;
                            }
                            if (temp_s3 != 0x38) {
                                if (temp_s3 >= 0x38) {
                                    if (temp_s3 < 0x3B) {
                                        if (spB0 & 0x8FC) {
                                            D_005F68E8 = spB4;
                                            var_fp = spB4;
                                            Snd_PlaySe(5, 0);
                                        }
                                    } else {
                                        goto block_139;
                                    }
                                } else {
                                    goto block_140;
                                }
                            } else {
block_139:
block_140:
                                if (spBC & (sp10 | sp20 | (sp18 | sp14) | sp1C)) {
                                    if (func_003544C8(temp_s3, var_s1_3) != 0) {
                                        D_005F68E8 = temp_s4;
                                        var_fp = temp_s4;
                                        Snd_PlaySe(5, 0);
                                    } else {
                                        var_fp = -1;
                                    }
                                }
                            }
                            if (temp_s3 == 0x38) {

                            }
                            if (temp_s3 == 0x38) {
                                var_s4 = var_s0_3 + 1;
                                if (var_s1_3 == 3) {
                                    if (func_003BFDC8() != 0) {
                                        goto block_149;
                                    }
                                } else {
                                    goto block_150;
                                }
                            } else {
block_149:
                                var_s4 = var_s0_3 + 1;
block_150:
                                spC4 = var_s4;
                                if ((temp_s3 >= 0x3B) || (temp_s3 < 0x39)) {
                                    var_s1_4 = (var_s4 * 0x14) + 0x122;
                                    var_s0_4 = 0x48;
                                    var_s2_4 = 0;
                                    var_v0_4 = 0 * 4;
                                    do {
                                        temp_v0_2 = *(&sp10 + var_v0_4);
                                        switch (temp_v0_2) { /* switch 6; irregular */
                                        case 1:     /* switch 6 */
                                            var_a0_2 = ((u8 *)&D_00522A48);
block_160:
                                            Text_Printf(var_a0_2, 0xFFFE0, var_s0_4, var_s1_4);
                                            var_s0_4 += 0x10;
                                            break;
                                        case 2:     /* switch 6 */
                                            var_a0_2 = ((u8 *)&D_00522A58);
                                            goto block_160;
                                        case 4:     /* switch 6 */
                                            var_a0_2 = ((u8 *)&D_00522A68);
                                            goto block_160;
                                        case 8:     /* switch 6 */
                                            var_a0_2 = ((u8 *)&D_00522A78);
                                            goto block_160;
                                        case 16:    /* switch 6 */
                                            Text_Printf(((u8 *)&D_00522A88), 0xFFFE0, var_s0_4 + (var_s2_4 * 0x10), var_s1_4);
                                            var_s0_4 = 0x48;
                                            var_s1_4 += 0x14;
                                            break;
                                        case 32:    /* switch 6 */
                                            temp_a2_3 = var_s0_4 + (var_s2_4 * 0x10);
                                            var_s0_4 += 0x20;
                                            Text_Printf(((u8 *)&D_00522AA0), 0xFFFE0, temp_a2_3, var_s1_4);
                                            break;
                                        }
                                        var_s2_4 += 1;
                                        var_v0_4 = var_s2_4 * 4;
                                    } while (var_s2_4 < 5);
                                }
                                Text_Printf(((u8 *)&D_00522AB0), 0xFFFE0, 0xA8, (spC4 * 0x14) + 0x122);
                            }
                            var_s0_3 = var_s4;
                        }
                        var_s1_3 = var_s6;
                        if ((var_s0_3 < spB8) && (var_s6 < 6)) {
                            goto loop_117;
                        }
                    }
                }
            }
            if (spC8 != 0) {
                if (var_fp == -1) {
                    Snd_PlaySe(3, 0);
                }
            }
            temp_v1_3 = D_005F6906 + D_005F6908;
            D_005F6906 = temp_v1_3;
            temp_a1 = ((u8 *)&D_004562F0) + (M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 4) * 0xC);
            if ((u32) ((temp_v1_3 - 0x60) & 0xFF) >= 0x91U) {
                D_005F6908 = (s16) -(s16) (u16) D_005F6908;
            }
            if (M2C_FIELD(temp_a1, u8 *, 5) != 0) {
                if (M2C_FIELD(((u8 *)&D_01FC4910), s32 *, 8) == 1) {
                    func_00354730(&sp10, temp_a1, 0x80);
                    D_005F6730 = 0xFFFD0;
                    Sprite_SetFilter(1, 1);
                    Sprite_SetBlend(1, 0);
                    D_005F6738 = D_005F6906 | (0x8000 << 0x10) | (((s64) D_005F6906 << 0x10) | ((s64) D_005F6906 << 8));
                    Sprite_DrawScaled(1, (sp60 < 0x3B) ? 0x73 : 0x76, 0x75, sp58, 0, 0, 0);
                }
            }
            if (var_fp != -1) {
                if (var_fp != 0x1C1) {
                    if (var_fp < 0x1C2) {
                        if (var_fp >= 0x108) {
                            if (var_fp >= 0x10A) {
                                if (var_fp != 0x1C0) {
                                    goto block_199;
                                }
                                D_005F68E8 = -1;
                                D_005F705B = 1;
                                D_005F7059 = 1;
                                D_005F705D = 0;
                                goto block_195;
                            }
                            D_005F705D = 2;
                            D_005F68E8 = -1;
                            D_005F705B = 1;
                            D_005F7059 = 1;
                            D_005F705C = 1;
                            D_005F7056 = 1;
                        } else {
block_199:
                            D_005F705B = 1;
                            D_0043B838[0] = var_fp;
                            Mode_EnterEnding(0xD, var_fp, var_fp);
                            D_005F7056 = 1;
                            D_005F68E8 = -1;
                        }
                    } else if (var_fp != 0x1C3) {
                        if (var_fp >= 0x1C3) {
                            switch (var_fp) {       /* switch 7; irregular */
                            case 0x1D0:             /* switch 7 */
                                D_005F68E8 = -1;
                                D_005F7055 = 1;
                                break;
                            case 0x1C8:             /* switch 7 */
                                Theater_OpenMusicPlayer(0xD);
                                D_005F68E8 = -1;
                                break;
                            }
                        } else {
                            D_005F705B = 1;
                            D_005F7057 = 1;
                            D_005F705A = 1;
                        }
                    } else {
                        D_005F705B = 1;
                        D_005F7058 = 1;
                        D_005F705A = 1;
                    }
                } else {
                    D_005F68E8 = -1;
                    D_005F705B = 1;
                    D_005F7059 = 1;
                    D_005F705D = 1;
block_195:
                    D_005F705C = 1;
                    D_005F7056 = 1;
                }
            }
        } else if ((s16) D_005F62AC <= 0) {
            func_003992C8(1, 0, 0);
            Mode_Change(3);
        }
        if (D_005F7059 > 0) {
            if (D_005F7059 >= 4) {
                if (D_005F7059 != 4) {

                } else {
                    D_005F7059 = -2;
                    D_005F705C = 1;
                    D_005F705A = 0;
                    func_0039C7D8(D_005F705D);
                    D_005F7056 = 1;
                    D_005F68E8 = -1;
                }
            } else {
                D_005F7059 = (u8) D_005F7059 + 1;
            }
        }
        switch ((s8) D_005F7057) {                  /* switch 8; irregular */
        case 1:                                     /* switch 8 */
            D_005F7057 += 1;
            break;
        case 2:                                     /* switch 8 */
            D_005F705A = 0;
            D_005F7057 = -2U;
            func_003992C8(1, 0, 0);
            func_003A9678(0);
            Mode_EnterLoading(0xD);
            D_005F7056 = 1;
            D_005F68E8 = -1;
            break;
        }
        switch (D_005F7058) {                       /* switch 9; irregular */
        case 1:                                     /* switch 9 */
            D_005F7058 = (u8) D_005F7058 + 1;
            return;
        case 2:                                     /* switch 9 */
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
}
#undef M2C_UNK
#undef M2C_FIELD
#undef M2C_BITWISE
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
