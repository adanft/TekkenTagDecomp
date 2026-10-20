/* 0x3DB758-0x3DC9C8 (module model_draw, docs/game_overview.md section 4; docs/systems/render.md). */
#include "common.h"
#include "fighter.h"

/* model_draw (0x3DB758-0x3DC9C8): despite the module name, no model or VU1 code here. It holds
 * - the draw hooks of the two stage object lists (D_01FBF680 / D_01FC0D00) and three accessors of the second list,
 * - small text helpers (string skip, column count / copy),
 * - 2D GS primitives written straight into the GIF packet buffers of src/gfx.c (D_008BD310): scissor, flat
 *   rectangles, a Gouraud quad, and the menu window (9-slice frame from a texture sheet plus a gradient background),
 * - two per-slot character resource helpers (D_01FC4930).
 * The packet buffers are opened with func_003994C0 (buffer A) / func_00399520 (buffer B), which return the current
 * write pointer (NULL after an overflow), and closed with func_00399580 / func_00399650 (end pointer). */

/* An object of a stage object list (0x90 bytes) [V layout of the fields used here]. */
typedef struct {
    /* 0x00 */ u8 unk0[0x5C];
    /* 0x5C */ f32 unk5C; /* position x, y, z */
    /* 0x60 */ f32 unk60;
    /* 0x64 */ f32 unk64;
    /* 0x68 */ u8 unk68[0x1D];
    /* 0x85 */ u8 unk85; /* flags; bit 1 set by func_003DB860 */
    /* 0x86 */ u8 unk86[0xA];
} StageObj;

/* D_01FBF680 / D_01FC0D00: the two stage object lists (0x1680 bytes each), filled by func_003DB578 from the stage's
 * entry of D_00453FC8 and run through the hooks D_005F6FD0 (update) / D_005F6FD4 (draw) [V]. */
typedef struct {
    /* 0x0000 */ StageObj obj[39];
    /* 0x15F0 */ u8 unk15F0[0x84];
    /* 0x1674 */ s32 unk1674;
    /* 0x1678 */ s16 unk1678; /* object count */
    /* 0x167A */ u8 unk167A[6];
} StageObjList;

extern s32 D_005F67B8; /* stage object flags: 8 / 0x10 = list 1 / 2 updated, draw pending */
extern void (*D_005F6FD4)(StageObjList *list);
extern StageObjList D_01FBF680;
extern StageObjList D_01FC0D00;

void func_003621F8(s32 a);
u64 *func_003994C0(void);
u64 *func_00399520(void);
void func_00399580(u64 *p);
void func_00399650(u64 *p);

/* Stage draw pass, before the stage: draws stage object list 1 once after each update [V]. */
void func_003DB758(void) {
    if ((D_005F67B8 & 8) && D_005F6FD4 != NULL) {
        func_003621F8(4);
        D_005F67B8 &= ~8;
        D_005F6FD4(&D_01FBF680);
    }
}

/* Stage draw pass, after the stage: the same for stage object list 2 [V]. */
void func_003DB7B0(void) {
    if ((D_005F67B8 & 0x10) && D_005F6FD4 != NULL) {
        func_003621F8(4);
        D_005F67B8 &= ~0x10;
        D_005F6FD4(&D_01FC0D00);
    }
}

/* Position of object i of stage list 2 as (x, y, z, 1); 0 if i is out of range [V]. */
s32 func_003DB808(s32 i, f32 *out) {
    StageObjList *l = &D_01FC0D00;
    StageObj *o;

    if (i < 0 || i >= l->unk1678) {
        return 0;
    }
    o = &l->obj[i];
    out[0] = o->unk5C;
    out[1] = o->unk60;
    out[2] = o->unk64;
    out[3] = 1.0f;
    return 1;
}

/* Sets flag 2 of object i of stage list 2 [V]. */
s32 func_003DB860(s32 i) {
    StageObjList *l = &D_01FC0D00;

    if (i < 0 || i >= l->unk1678) {
        return 0;
    }
    l->obj[i].unk85 |= 2;
    return 1;
}

/* 1 while flag 2 of object i of stage list 2 is clear; 0 if set or out of range [V]. */
s32 func_003DB8A8(s32 i) {
    StageObjList *l = &D_01FC0D00;

    if (i < 0 || i >= l->unk1678) {
        return 0;
    }
    return (l->obj[i].unk85 & 2) == 0;
}

/* Returns the byte after the next NUL [V]. */
u8 *Str_Next(u8 *s) {
    while (*s++ != 0) {
    }
    return s;
}

/* Skips n NUL-terminated strings [V]. */
u8 *Str_SkipN(u8 *s, s32 n) {
    while (n-- > 0) {
        s = Str_Next(s);
    }
    return s;
}

/* Copies characters until n columns are used (codes 1-5 and 0xFE take one, every other byte two), NUL-terminates dst
 * and returns where src stopped [V]. */
u8 *func_003DB960(u8 *dst, u8 *src, s32 n) {
    while (n > 0) {
        u8 c = *src;
        if (c == 0xFE || (u32)(c - 1) < 5) {
            n--;
        } else {
            n -= 2;
        }
        *dst++ = c;
        src++;
    }
    *dst = 0;
    return src;
}

/* Column count of a string, with the rule of func_003DB960 [V]. */
s32 func_003DB9A0(u8 *s) {
    s32 n = 0;
    u8 c;

    while ((c = *s++) != 0) {
        if (c == 0xFE || (u32)(c - 1) < 5) {
            n++;
        } else {
            n += 2;
        }
    }
    return n;
}

/* SCISSOR_2 (x0, x1, y0, y1) in packet buffer A: GIF tag NLOOP 1 EOP, one A+D register [V]. */
void func_003DB9E8(u32 x0, u32 x1, u32 y0, u32 y1) {
    u64 *p = func_003994C0();

    if (p != NULL) {
        *p++ = 0x1000000000008001;
        *p++ = 0xE;
        *p++ = (u64)x0 | ((u64)x1 << 16) | ((u64)y0 << 32) | ((u64)y1 << 48);
        *p++ = 0x41;
        func_00399580(p);
    }
}

/* The same in packet buffer B [V]. */
void func_003DBAA8(u32 x0, u32 x1, u32 y0, u32 y1) {
    u64 *p = func_00399520();

    if (p != NULL) {
        *p++ = 0x1000000000008001;
        *p++ = 0xE;
        *p++ = (u64)x0 | ((u64)x1 << 16) | ((u64)y0 << 32) | ((u64)y1 << 48);
        *p++ = 0x41;
        func_00399650(p);
    }
}

/* Flat rectangle (x, y, w, h) at depth z, colour 0xRRGGBBAA, in buffer B: PACKED tag with PRIM SPRITE (context 2),
 * TEST_2 0x7000D (alpha > 0, z GREATER), RGBAQ and two XYZF2; screen pixels offset by (0x6C0, 0x720) [V]. */
void func_003DBB68(u32 x, u32 y, u32 w, u32 h, u32 z, u32 color) {
    u64 *p = func_00399520();
    u64 zf;

    if (p != NULL) {
        *p++ = 0x4103400000008001;
        *p++ = 0x441E;
        *p++ = 0x7000D;
        *p++ = 0x48;
        *p++ = ((color >> 24) & 0xFF) | ((u64)((color >> 16) & 0xFF) << 32);
        *p++ = ((color >> 8) & 0xFF) | ((u64)(color & 0xFF) << 32);
        *p++ = (((u64)x + 0x6C0) << 4) | (((u64)y + 0x720) << 36);
        *p++ = zf = ((u64)z << 4) | ((u64)1 << 32);
        *p++ = (((u64)x + (u64)w + 0x6C0) << 4) | (((u64)y + (u64)h + 0x720) << 36);
        *p++ = zf;
        func_00399650(p);
    }
}

/* The same rectangle alpha-blended: PRIM SPRITE with ABE, and ALPHA_2 0x44 ((Cs - Cd) * As + Cd) [V]. */
void func_003DBCF0(u32 x, u32 y, u32 w, u32 h, u32 z, u32 color) {
    u64 *p = func_00399520();
    u64 zf;

    if (p != NULL) {
        *p++ = 0x5123400000008001;
        *p++ = 0x441EE;
        *p++ = 0x7000D;
        *p++ = 0x48;
        *p++ = 0x44;
        *p++ = 0x43;
        *p++ = ((color >> 24) & 0xFF) | ((u64)((color >> 16) & 0xFF) << 32);
        *p++ = ((color >> 8) & 0xFF) | ((u64)(color & 0xFF) << 32);
        *p++ = (((u64)x + 0x6C0) << 4) | (((u64)y + 0x720) << 36);
        *p++ = zf = ((u64)z << 4) | ((u64)1 << 32);
        *p++ = (((u64)x + (u64)w + 0x6C0) << 4) | (((u64)y + (u64)h + 0x720) << 36);
        *p++ = zf;
        func_00399650(p);
    }
}

/* Alpha-blended Gouraud quad in buffer B: A+D TEST_2 / ALPHA_2, then a REGLIST tag with PRIM 0x34C (triangle strip,
 * Gouraud, blended, context 2) and four RGBAQ / XYZF2 pairs: c0 top-left, c1 bottom-left, c2 top-right, c3
 * bottom-right [V]. */
void func_003DBE98(u32 x, u32 y, u32 w, u32 h, u32 z, u32 c0, u32 c1, u32 c2, u32 c3) {
    u64 *p = func_00399520();

    if (p != NULL) {
        *p++ = 0x2126000000000001;
        *p++ = 0xEE;
        *p++ = 0x7000D;
        *p++ = 0x48;
        *p++ = 0x44;
        *p++ = 0x43;
        *p++ = 0xA400000000008001;
        *p++ = 0xE414141410;
        *p++ = 0x34C;
        *p++ = ((c0 >> 24) & 0xFF) | ((u64)((c0 >> 16) & 0xFF) << 8) | ((u64)((c0 >> 8) & 0xFF) << 16)
             | ((u64)(c0 & 0xFF) << 24) | ((u64)1 << 32);
        *p++ = (u64)((x + 0x6C0) << 4) | ((u64)((y + 0x720) << 4) << 16) | ((u64)z << 32) | ((u64)1 << 56);
        *p++ = ((c1 >> 24) & 0xFF) | ((u64)((c1 >> 16) & 0xFF) << 8) | ((u64)((c1 >> 8) & 0xFF) << 16)
             | ((u64)(c1 & 0xFF) << 24) | ((u64)1 << 32);
        *p++ = (u64)((x + 0x6C0) << 4) | ((u64)((y + h + 0x720) << 4) << 16) | ((u64)z << 32) | ((u64)1 << 56);
        *p++ = ((c2 >> 24) & 0xFF) | ((u64)((c2 >> 16) & 0xFF) << 8) | ((u64)((c2 >> 8) & 0xFF) << 16)
             | ((u64)(c2 & 0xFF) << 24) | ((u64)1 << 32);
        *p++ = (u64)((x + w + 0x6C0) << 4) | ((u64)((y + 0x720) << 4) << 16) | ((u64)z << 32) | ((u64)1 << 56);
        *p++ = ((c3 >> 24) & 0xFF) | ((u64)((c3 >> 16) & 0xFF) << 8) | ((u64)((c3 >> 8) & 0xFF) << 16)
             | ((u64)(c3 & 0xFF) << 24) | ((u64)1 << 32);
        *p++ = (u64)((x + w + 0x6C0) << 4) | ((u64)((y + h + 0x720) << 4) << 16) | ((u64)z << 32) | ((u64)1 << 56);
        *p++ = 0;
        func_00399650(p);
    }
}

/* Texture of the window frame (TEX0 TBP0 / TBW / PSM) [V use]. */
typedef struct {
    /* 0x0 */ u32 tbp;
    /* 0x4 */ u32 tbw;
    /* 0x8 */ u32 psm;
} WinTex;

/* Its CLUT (TEX0 CBP / CPSM) [V use]. */
typedef struct {
    /* 0x0 */ u32 cbp;
    /* 0x4 */ u32 unk4;
    /* 0x8 */ u32 cpsm;
} WinClut;

typedef struct {
    /* 0x0 */ WinClut *clut;
    /* 0x4 */ s32 cbp; /* added to clut->cbp */
} WinClutRef;

/* D_00443780: the window skin, a texture and the texel origin (u, v) of a strip of eight 16x16 tiles [V use]. */
typedef struct {
    /* 0x00 */ WinTex *tex;
    /* 0x04 */ u32 u;
    /* 0x08 */ u32 v;
    /* 0x0C */ u8 unkC[0x14];
    /* 0x20 */ WinClutRef unk20;
} WinSkin;

extern WinSkin D_00443780;

void func_003CD918(s32 list, s32 on);

#define WIN_UV(u, v) ((u64)(((u) << 4) + 8) | ((u64)(((v) << 4) + 8) << 16))
#define WIN_XYZF(x, y) ((u64)(((x) + 0x6C0) << 4) | ((u64)(((y) + 0x720) << 4) << 16) | ((u64)z << 32) | ((u64)1 << 56))

/* Window frame around (x, y, w, h): TEX1_2 bilinear (func_003CD918), TEST_2 / ALPHA_2 / CLAMP_2, RGBAQ 0x80808080
 * and TEX0_2 of D_00443780 (TW = TH = 10, TCC, CLD 1), then eight textured sprites (PRIM 0x356) 16 pixels wide from
 * the eight tiles of the skin: corners and edges, counter-clockwise from the top-left [V]. */
void func_003DC188(u32 x, u32 y, u32 w, u32 h, u32 z) {
    WinTex *tex;
    u32 u;
    u32 v;
    WinClut *clut;
    WinClutRef *ref;
    u64 *p;

    func_003CD918(1, 1);
    tex = D_00443780.tex;
    ref = &D_00443780.unk20;
    clut = ref->clut;
    u = D_00443780.u;
    v = D_00443780.v;
    p = func_00399520();
    if (p != NULL) {
        *p++ = 0x3000000000000001;
        *p++ = 0xEEE;
        *p++ = 0x5180B;
        *p++ = 0x48;
        *p++ = 0x44;
        *p++ = 0x43;
        *p++ = 0;
        *p++ = 9;
        *p++ = 0x2400000000000001;
        *p++ = 0x71;
        *p++ = 0x80808080;
        *p++ = (u64)tex->tbp | ((u64)tex->tbw << 14) | ((u64)tex->psm << 20) | ((u64)10 << 26) | ((u64)10 << 30)
             | ((u64)1 << 34) | ((u64)(clut->cbp + ref->cbp) << 37) | ((u64)clut->cpsm << 51)
             | ((u64)1 << 61);
        *p++ = 0x5400000000008008;
        *p++ = 0x43430;
        /* top-left corner */
        *p++ = 0x356;
        *p++ = WIN_UV(u, v);
        *p++ = WIN_XYZF(x - 16, y - 16);
        *p++ = WIN_UV(u + 15, v + 15);
        *p++ = WIN_XYZF(x, y);
        /* left edge */
        *p++ = 0x356;
        *p++ = WIN_UV(u + 16, v);
        *p++ = WIN_XYZF(x - 16, y);
        *p++ = WIN_UV(u + 31, v + 15);
        *p++ = WIN_XYZF(x, y + h);
        /* bottom-left corner */
        *p++ = 0x356;
        *p++ = WIN_UV(u + 32, v);
        *p++ = WIN_XYZF(x - 16, y + h);
        *p++ = WIN_UV(u + 47, v + 15);
        *p++ = WIN_XYZF(x, y + h + 16);
        /* bottom edge */
        *p++ = 0x356;
        *p++ = WIN_UV(u + 48, v);
        *p++ = WIN_XYZF(x, y + h);
        *p++ = WIN_UV(u + 63, v + 15);
        *p++ = WIN_XYZF(x + w, y + h + 16);
        /* bottom-right corner */
        *p++ = 0x356;
        *p++ = WIN_UV(u + 64, v);
        *p++ = WIN_XYZF(x + w, y + h);
        *p++ = WIN_UV(u + 79, v + 15);
        *p++ = WIN_XYZF(x + w + 16, y + h + 16);
        /* right edge */
        *p++ = 0x356;
        *p++ = WIN_UV(u + 80, v);
        *p++ = WIN_XYZF(x + w, y);
        *p++ = WIN_UV(u + 95, v + 15);
        *p++ = WIN_XYZF(x + w + 16, y + h);
        /* top-right corner */
        *p++ = 0x356;
        *p++ = WIN_UV(u + 96, v);
        *p++ = WIN_XYZF(x + w, y - 16);
        *p++ = WIN_UV(u + 111, v + 15);
        *p++ = WIN_XYZF(x + w + 16, y);
        /* top edge */
        *p++ = 0x356;
        *p++ = WIN_UV(u + 112, v);
        *p++ = WIN_XYZF(x, y - 16);
        *p++ = WIN_UV(u + 127, v + 15);
        *p++ = WIN_XYZF(x + w, y);
        func_00399650(p);
    }
}

/* Menu window: the frame, then a diagonal gradient behind it, 8 pixels larger on each side (c0 top-left, c1
 * bottom-right, their per-channel mean on the other two corners) [V]. */
void Ui_DrawWindow(u32 x, u32 y, u32 w, u32 h, u32 z, u32 c0, u32 c1) {
    u32 mid = ((((c0 & 0xFF) + (c1 & 0xFF)) >> 1) | ((((c0 >> 16) & 0xFF) + ((c1 >> 16) & 0xFF)) >> 1 << 16))
            | ((((c0 >> 24) & 0xFF) + ((c1 >> 24) & 0xFF)) >> 1 << 24
               | (((c0 >> 8) & 0xFF) + ((c1 >> 8) & 0xFF)) >> 1 << 8);

    func_003DC188(x, y, w, h, z);
    func_003DBE98(x - 8, y - 8, w + 16, h + 16, z, c0, mid, mid, c1);
}

/* D_01FC4930: per character slot, entries 1 and 0 of the slot's loaded bank (func_00367290) [V]. */
typedef struct {
    /* 0x0 */ void *unk0;
    /* 0x4 */ void *unk4;
    /* 0x8 */ s32 unk8;
    /* 0xC */ s32 unkC;
} Unk1FC4930;

extern Unk1FC4930 D_01FC4930[];

void *func_00364528(s32 i, void *bank);
void *func_00367290(s32 i);

/* Caches entries 1 and 0 of slot i's bank, or NULL without a bank [V]. */
void func_003DC870(s32 i) {
    void *bank = func_00367290(i);

    D_01FC4930[i].unk8 = 0;
    if (bank != NULL) {
        D_01FC4930[i].unk0 = func_00364528(1, func_00367290(i));
        D_01FC4930[i].unk4 = func_00364528(0, func_00367290(i));
    } else {
        D_01FC4930[i].unk0 = NULL;
        D_01FC4930[i].unk4 = NULL;
    }
}

extern Fighter gFighters[4];
extern u16 D_005F5CB0[2];

void func_003DE850(s32 side, void *data);

/* For each side flagged in D_005F5CB0, hands entry 0 of its fighter's slot (gFighters[side].unk22) to
 * func_003DE850 (demo module, builds a 0x3B40-byte transfer for that side) [V]. scmp shows 3 lines: the original
 * references the field address as the symbol D_01FC493C, the same address as D_01FC4930 + 0xC here. */
void func_003DC908(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        if (D_005F5CB0[i] != 0) {
            s32 k = gFighters[i].unk22;

            D_01FC4930[k].unkC = 0;
            if (D_01FC4930[k].unk4 != NULL) {
                func_003DE850(i, D_01FC4930[k].unk4);
            }
        }
    }
}
