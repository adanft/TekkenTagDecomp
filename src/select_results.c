/* 0x3BDF60-0x3C1A78 (module select_results up to the hud cut, docs/game_overview.md section 4; docs/systems/modes.md):
 * the unlock state, the character select helpers, the team members' life at round start (Battle_InitTeamLife) and the
 * per-frame life recovery of the partner (Battle_RecoverLife). .rodata 0x51F150-0x51F678 is its own (subsegment, every
 * string and table included in place); the start is an object edge [V: its .rodata follows modes_b's], the end is the
 * cut before src/hud.c [V: hud's strings follow func_003C1350's jump table, one object]. */
#include "common.h"
#include "fighter.h"

extern Fighter gFighters[4];
extern TeamMember D_00440940[2][2];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 D_005F6668[2];

s32 func_003AE420(s32 side, s32 member);
void func_003AE400(s32 side, s32 member, s32 n);
s32 func_003C1350(s32 character, s32 side);
void func_003C1620(void);
void func_003C18B8(void);
s32 func_0036E158(void);
s32 func_003A9698(void);
s32 func_003AE3C0(Fighter *f);

extern u16 gFrameCount;
extern s32 D_005F6E54;
extern s16 D_005F6660;
extern s16 D_005F665E;
extern u16 D_0043B84C[];
extern u16 D_005F5CB6;
/* The options / unlock save block (0x68 bytes, docs/systems/modes.md section 4). */
typedef struct {
    u8 unk0[8];
    u64 unk8;
    u64 unk10;
    u32 unk18;
    u8 pad1C[4];
    u16 unk20;
    u8 unk22;
    u8 unk23;
    u8 unk24;
    u8 pad25[0x12];
    u8 unk37;
    u8 pad38[0x30];
} SaveBlk;
extern SaveBlk D_00439C60;
/* An unlock table entry (D_00440818, ended by a threshold of 0xFFFF). */
typedef struct {
    u16 thr;
    u16 kind;
    s32 arg;
} UnlockEnt;
extern UnlockEnt D_00440818[];
typedef struct {
    s32 unk0;
    u16 unk4;
    u16 pad6;
} SelEnt;
extern u8 D_0043B7F8[];
extern s32 D_01FAC7C0[][23];
void func_0035CAF8(void);
void Unlock_ApplyAll(void);
void Unlock_Step(void);
u8 func_003BFEE8(void);
s32 func_003BFEA0(s32 m);
s32 func_003BFA68(void);
extern const s32 D_0051F2F0[][23];
typedef struct {
    u8 unk0[4];
    u8 unk4;
} ArcEnt;
extern ArcEnt D_00440A00[];
s32 func_003800D8(s32 n);
typedef struct {
    u16 x;
    u16 y;
    u32 z;
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    u32 q;
} SelVtx;
/* Menu window quad: alpha limit, then four vertices (XYZ2, RGBAQ), as modes_b.c's MbQuad. */
typedef struct {
    u16 alpha;
    u16 pad2[3];
    SelVtx v[4];
} __attribute__((aligned(8))) SelQuad;
typedef struct {
    u16 x;
    u16 pad2;
    u16 y;
    u16 pad6;
    u16 w;
    u16 padA;
    u16 h;
    u16 padE;
} SelRect;
extern SelQuad D_0043FD10[];
extern SelRect D_00440688[];
extern s32 D_005F6730;
extern u16 D_005F5C5C;
extern u16 D_005F5C5E;
extern const s32 D_0051F5A0[];
extern const s32 D_0051F5B0[];
extern const s32 D_0051F5C0[];
void func_003BB680(SelQuad *q, s32 z, s32 unused);
void func_003BB4E8(SelQuad *q, u16 *alpha, s32 unused);
extern u16 D_005F62AC;
typedef struct {
    u8 r;
    u8 g;
    u8 b;
} SelRgb;
extern SelRgb D_00440748[];
extern s16 gMode;
extern s32 D_00440708[][2];
extern s32 D_00440728[][2];
extern u8 D_0043B968[];
int rand(void);
s32 func_003ADA08(u8 *set, s32 c);
s32 func_003AE608(s32 a, s32 b);
s32 func_003C0210(s32 c, u32 kind);
s32 func_003BFAA8(void);
typedef struct {
    const char *name;
    u8 unk4;
} SelChar;
extern SelChar *D_00415840[];
typedef struct {
    s32 unk0;
    u8 pad4[0xC];
    s32 unk10;
    s32 unk14;
    u8 pad18[4];
} SelSide;
extern SelSide D_00440D08[];
extern s16 D_005F62A8[2];
extern const char D_005F65E0[];
extern const char D_0051F1A8[];
extern const char D_0051F1B8[];
extern const char D_0051F1C8[];
s32 Text_Printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
s32 func_003AF2E0(s32 side, s32 n);
s32 func_003BDF60(void);
/* Drafts (NON_MATCHING) */
void Arcade_BuildOpponents(s32 side);
void func_003BE118(s32 side, u8 *r, s32 unused, s32 *mode);
void func_003BE3E8(u8 *r, s32 b);
void func_003BECC0(void);
void func_003BEAB0(s32 side, s32 m, s32 c, s32 cos);
void func_003BF080(s32 x, s32 y, s32 c);
void func_003BFFB0(void);
extern s16 D_004FF690[];
extern s8 D_0043B9B8[];
extern const s32 D_0051F570[6][2];
extern const s32 D_0051F2D0[];
extern u64 D_005F6738;
extern const u8 D_0051E8F0[];
extern u8 D_0043B9E0[];
s32 func_003ADDD8(s32 side);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_SetBlend(s32 a, s32 b);
void Sprite_DrawScaled(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);
void CharSet_Copy(const void *a, void *b);
void func_0035CA00(s32 a);
void func_0035C998(s32 a);
void func_0035CA68(s32 a);
void func_0035CAE0(void);
void func_003BFBA0(void);
void func_003BFB80(void);
void func_003BFD98(void);
void func_003BFA78(s32 keep);
s32 func_003BFB18(void);
s32 func_003CBEC8(void);
void func_003CBEB8(void);
void Snd_PlaySe(s32 id, s32 player);
s32 func_003BF7D0(void);
void func_003C0F48(s32 k, s32 skip);
s32 func_003C0FF0(s32 *a, s32 *out, s32 n);
s32 func_003C1030(s32 k, s32 n);
void func_003C1078(s32 k, s32 i);
void func_003C1160(s32 k);
s32 func_003C11B0(s32 k);
void func_003C1238(s32 a);
extern s32 D_00439C78[];
extern u8 D_00439C82[];
extern u8 D_00439C83[];
extern u8 D_00439C84[];
extern u8 D_00439C97[];
extern u64 D_00439C70[];
extern SelEnt D_00440770[];

void func_00361968(void);
void func_003CF500(void);
void func_003CF608(s32 a);
void func_003C6888(void);

s32 func_003BDF60(void) {
    return D_005F6E54;
}

void func_003BDF68(s32 *p) {
    if (p[7] == 0) {
        D_005F6E54 = gFrameCount;
    }
}

void func_003BDF80(s32 right) {
    SelQuad q = D_0043FD10[15];
    s32 x = right ? 0x166 : 0x26;
    q.v[0].x += x << 4;
    q.v[1].x += x << 4;
    q.v[2].x += x << 4;
    q.v[3].x += x << 4;
    func_003BB680(&q, 0xFFEE0, 0);
}

void func_003BE038(s32 x, s32 y) {
    SelQuad q = D_0043FD10[11];
    q.v[0].y += y << 4;
    q.v[1].y += y << 4;
    q.v[2].y += y << 4;
    q.v[3].y += y << 4;
    q.v[0].x += x << 4;
    q.v[1].x += x << 4;
    q.v[2].x += x << 4;
    q.v[3].x += x << 4;
    func_003BB680(&q, D_005F6730, 0);
}

extern s16 gSinTable[];
extern s32 D_005F6700[2];
extern u16 D_005F5CB0[2];
typedef struct {
    s32 x;
    s32 y;
} SelPos;
extern SelPos D_004406C8[];
s32 func_003AF308(s32 a, s32 b);
s32 func_003CF230(s32 chr, s32 costume);
void func_003CBFB8(s32 spr, s32 x, s32 y, s32 z);

/* NON_MATCHING: 88 lines, registers: the original spills the blink levels and the side offset to the stack */
#if 0
void func_003BE118(s32 side, u8 *r, s32 unused, s32 *mode) {
    s32 i, st, c, x;
    s32 a, b, save;

    a = gSinTable[(D_005F6E54 * 32) & 0xFE0];
    a = (a < 0 ? -a : a) >> 4;
    if (a >= 0x100) {
        a = 0xFF;
    }
    b = 0x80;
    save = D_005F6730;
    D_005F6730 = 0xFFEF0;
    x = 0x16C;
    if (side == 0) {
        x = 0x2C;
    }
    if (*mode == 4) {
        b = a;
    }
    D_005F6738 = 0x80808080;
    for (i = 0; i < D_005F6700[side]; i++) {
        st = 0;
        c = b;
        if (*mode == 1 || *mode == 6) {
            if (D_005F62A8[side] == i) {
                c = a;
                st = 1;
            } else if (i >= r[side * 30 + 0x5C]) {
                st = 2;
            } else if (i < D_005F62A8[side]) {
                func_003CBFB8(func_003CF230(func_003AF2E0(side, i), func_003AF308(side, i)), x + D_004406C8[i].x,
                              D_004406C8[i].y + 0x132, 0);
            } else {
                st = 1;
            }
        }
        if (D_005F5CB0[side] == 0 || st == 2) {
            func_003CBFB8(0x37, x + D_004406C8[i].x, D_004406C8[i].y + 0x132, 0);
        } else if (st) {
            func_003CBFB8(0x36, x + D_004406C8[i].x, D_004406C8[i].y + 0x132, 0);
        }
        if (*mode == 4) {
            c /= 2;
        }
        func_003BF080(x + D_004406C8[i].x, D_004406C8[i].y + 0x132, c);
    }
    D_005F6730 = save;
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results", func_003BE118);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F150);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F178);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F190);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F1A8);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F1B8);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F1C8);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F1D8);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F1F0);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F208);

extern const char D_0051F150[];
extern const char D_0051F178[];
extern const char D_0051F190[];
extern const char D_0051F1D8[];
extern const char D_0051F1F0[];
extern const char D_0051F208[];
s32 func_003AD900(s32 n, s32 k, s32 col, s32 d);
void func_003BE118(s32 side, u8 *r, s32 unused, s32 *mode);

/* NON_MATCHING: stack slots, 10 lines: the spilled base + 0x44/0x4B/0x55/0x6C and n ^ 1 get slots in GCSE hash order, set by the pseudo numbers of base and n (base needs about 20 more pseudos before it) */
#if 0
void func_003BE3E8(u8 *r, s32 b) {
    char buf[32];
    SelQuad q;
    s32 side, col, n, base, k, x, w, dy;
    s32 c1, c2, c3, c4, c5, c6, c7, c8;
    SelSide *e;

    for (side = 0; side < 2; side++) {
        e = &D_00440D08[side];
        col = 2;
        func_003BDF80(side);
        func_003BE118(side, r, b, &e->unk0);
        if (func_003BDF60() & 4) {
            col = 0x13;
        }
        switch (e->unk0) {
        case 2:
            break;
        case 4:
            n = r[side * 30 + 0x5D];
            base = side * 320;
            dy = (base + (n - 1) * 21 + 0x4B) * 16;
            q = D_0043FD10[22];
            q.v[0].x += dy;
            q.v[1].x += dy;
            q.v[2].x += dy;
            q.v[3].x += dy;
            func_003BB680(&q, 0xFFFF0, 0);
            c1 = func_003AD900(n, 1, col, 10);
            c2 = func_003AD900(n, 2, col, 10);
            c3 = func_003AD900(n, 3, col, 10);
            c4 = func_003AD900(n, 4, col, 10);
            c5 = func_003AD900(n, 5, col, 10);
            c6 = func_003AD900(n, 6, col, 10);
            c7 = func_003AD900(n, 7, col, 10);
            c8 = func_003AD900(n, 8, col, 10);
            Text_Printf(D_0051F150, base + 0x4B, 0x154, 10, 5, c1, c2, c3, c4, c5, c6, c7, c8);
            Text_Printf(D_0051F178, base + 0x44, 0x182, 10, 5, 7, n, base + 0x6C, n != 1 ? 0x53 : 0x20);
            Text_Printf(D_0051F190, base + 0x55, 0x1A0, 10, 5, 7);
            break;
        case 1:
            sprintf(buf, D_005F65E0, D_00415840[e->unk10 * 8 + e->unk14]->name);
            x = 320;
            if (side == 0) {
                x = 0;
            }
            for (k = 0; k < D_005F6700[side]; k++) {
                if (e->unk10 == func_003AF2E0(side, k)) {
                    sprintf(buf, D_0051F1A8);
                    col = 10;
                }
            }
            w = Text_Printf(D_0051F1B8, 10, 4, buf);
            Text_Printf(D_0051F1C8, x + (320 - w) / 2, 0x12A, 10, 4, col, buf);
            break;
        case 6:
            w = Text_Printf(D_0051F1D8, 10, 4);
            x = 320;
            if (side == 0) {
                x = 0;
            }
            Text_Printf(D_0051F1F0, x + (320 - w) / 2, 0x12A, 10, 4, 0x13);
            break;
        }
    }
    Text_Printf(D_0051F208, 0x120, 0x172, 10, 7, 2);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results", func_003BE3E8);
#endif

void func_003BE8C0(void) {
    SelQuad q;
    s32 n = 4, dy = 0, i;
    s32 (*t)[2] = D_00440708;

    switch (D_0043B7F8[0]) {
    case 2:
        t = D_00440728;
        break;
    case 5:
    case 6:
        dy = 10;
        n = 2;
        break;
    default:
        t = D_00440708;
        break;
    }
    for (i = 0; i < n; i++) {
        s32 x = t[i][0] << 4;
        s32 y = (t[i][1] - dy) << 4;
        if ((i & 1) == 0) {
            q = D_0043FD10[13];
        } else {
            q = D_0043FD10[12];
        }
        q.v[0].x += x;
        q.v[1].x += x;
        q.v[2].x += x;
        q.v[3].x += x;
        q.v[0].y += y;
        q.v[1].y += y;
        q.v[2].y += y;
        q.v[3].y += y;
        func_003BB680(&q, 0xFFEE0, 0);
    }
}

/* NON_MATCHING: 139 lines, one extra saved register; branch layout of the partner name */
#if 0
void func_003BEAB0(s32 side, s32 m, s32 c, s32 cos) {
    char buf[32];
    s32 dy = 0;
    s32 i = side + m * 2;
    SelSide *r = &D_00440D08[side];
    s32 x, y, k, col;

    if (D_0043B7F8[0] == 2) {
        return;
    }
    if ((u32)(D_0043B7F8[0] - 5) < 2) {
        if (m != 0) {
            return;
        }
        dy = 10;
    }
    x = D_00440708[i][0];
    y = D_00440708[i][1] - dy;
    k = c * 8 + cos;
    col = 0x13;
    if (!(func_003BDF60() & 4) && D_005F62A8[side] == m) {
        col = 2;
    }
    if (!(i & 1)) {
        x -= Text_Printf(D_0051F1B8, 10, 4, D_00415840[k]->name);
    }
    if (m != 0 && r->unk10 == func_003AF2E0(side, 0) && D_005F62A8[side] >= 2) {
        sprintf(buf, D_0051F1A8);
        col = 10;
        if (!(i & 1)) {
            x = D_00440708[i][0] - Text_Printf(D_0051F1B8, 10, 4, D_0051F1A8);
            col = 10;
        }
    } else {
        sprintf(buf, D_005F65E0, D_00415840[k]->name);
    }
    Text_Printf(D_0051F1C8, x, y, 10, 4, col, buf);
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results", func_003BEAB0);
#endif

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F218);

typedef struct {
    s32 x[2][2];
} SelSlotTbl;
extern const SelSlotTbl D_0051F218;
s32 func_003CBE88(SelSide *s);
s32 func_003AF330(s32 a, s32 b);

void func_003BECC0(void) {
    SelSlotTbl t = D_0051F218;
    s32 save = D_005F6730;
    s32 a, side, spr, c, k;
    SelSide *e;
    s32 *pos;

    {
        s32 v = gSinTable[(D_005F6E54 * 32) & 0xFE0];
        a = (v < 0 ? -v : v) >> 4;
    }
    if (a >= 0x100) {
        a = 0xFF;
    }
    D_005F6730 = 0xFFEF0;
    if (D_0043B7F8[0] == 2) {
        return;
    }
    for (side = 0; side < 2; side++) {
        e = &D_00440D08[side];
        pos = t.x[side];
        if (e->unk0 == 0 || e->unk0 == 2) {
            continue;
        }
        if (D_005F62A8[side] >= 2) {
            D_005F6738 = 0x80808080;
            func_003CBFB8(func_003CF230(func_003AF2E0(side, 0), func_003AF308(side, 0)), pos[0], 0x144, 0);
            func_003BF080(pos[0], 0x144, 0x40);
            func_003CBFB8(func_003CF230(func_003AF2E0(side, 1), func_003AF308(side, 1)), pos[1], 0x144, 0);
            func_003BF080(pos[1], 0x144, 0x40);
        } else if (D_005F62A8[side] > 0) {
            spr = func_003CF230(func_003AF2E0(side, 0), func_003AF308(side, 0));
            D_005F6738 = 0x80808080;
            func_003CBFB8(spr, pos[0], 0x144, 0);
            func_003BF080(pos[0], 0x144, 0x40);
            if ((u32)(D_0043B7F8[0] - 5) < 2) {
                continue;
            }
            c = 0x80;
            if (func_003CBE88(e) != 0) {
                spr = 0x37;
            } else {
                k = func_003CF230(func_003AF2E0(side, 0), 0);
                spr = func_003CF230(e->unk10, 0);
                if (k == spr) {
                    c = 0x40;
                }
            }
            {
                u64 cc = c;
                D_005F6738 = cc | (cc << 8) | (cc << 16) | 0x80000000;
            }
            func_003CBFB8(spr, pos[1], 0x144, 0);
            func_003BF080(pos[1], 0x144, a);
        } else {
            if (func_003CBE88(e) != 0) {
                spr = 0x37;
            } else {
                if (func_003AF330(side, 0) < 0 || gFighters[side].unk26 != func_003AF330(side, 0)) {
                    spr = func_003CF230(e->unk10, e->unk14);
                } else {
                    spr = func_003CF230(func_003AF330(side, 0) >> 3, func_003AF330(side, 0) & 7);
                }
            }
            D_005F6738 = 0x80808080;
            func_003CBFB8(spr, pos[0], 0x144, 0);
            func_003BF080(pos[0], 0x144, a);
            if ((u32)(D_0043B7F8[0] - 5) < 2) {
                continue;
            }
            func_003CBFB8(0x37, pos[1], 0x144, 0);
        }
    }
    D_005F6730 = save;
}

/* NON_MATCHING: 53 lines, colour store order and the u16 adds of the four quads */
#if 0
void func_003BF080(s32 x, s32 y, s32 c) {
    SelQuad q;
    s32 i;
    q.v[0].r = c;
    q.v[1].r = c;
    q.v[2].r = c;
    q.v[3].r = c;
    q.v[0].g = c;
    q.v[1].g = c;
    q.v[2].g = c;
    q.v[3].g = c;
    q.v[0].b = c;
    q.v[1].b = c;
    q.v[2].b = c;
    q.v[3].b = c;
    q.v[0].a = 0x80;
    q.v[1].a = 0x80;
    q.v[2].a = 0x80;
    q.v[3].a = 0x80;
    for (i = 0; i < 4; i++) {
        s32 x0 = D_00440688[i].x + (x << 4);
        s32 y0 = D_00440688[i].y + (y << 4);
        q.v[0].x = x0;
        q.v[0].y = y0;
        q.v[1].x = x0 + D_00440688[i].w;
        q.v[1].y = y0;
        q.v[2].x = x0;
        q.v[2].y = y0 + D_00440688[i].h;
        q.v[3].x = x0 + D_00440688[i].w;
        q.v[3].y = y0 + D_00440688[i].h;
        func_003BB680(&q, 0xFFEE0, 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results", func_003BF080);
#endif

void func_003BF180(u8 *p) {
    SelQuad q;
    u8 k = *p;

    D_005F62AC += 3;
    q = D_0043FD10[0];
    q.v[3].r = q.v[2].r = D_00440748[k].r;
    q.v[3].g = q.v[2].g = D_00440748[k].g;
    q.v[3].b = q.v[2].b = D_00440748[k].b;
    func_003BB4E8(&q, &D_005F62AC, 0);
    q = D_0043FD10[1];
    q.v[1].r = q.v[0].r = D_00440748[k].r;
    q.v[1].g = q.v[0].g = D_00440748[k].g;
    q.v[1].b = q.v[0].b = D_00440748[k].b;
    func_003BB4E8(&q, &D_005F62AC, 0);
}

void func_003BF2F8(u8 *p, u16 *alpha) {
    SelQuad q;
    s32 k = *p;

    if (gMode == 13 || gMode == 15) {
        k = 9;
    } else if (gMode == 16) {
        k = 10;
    } else if (gMode == 9 || gMode == 0) {
        k = 11;
    }
    q = D_0043FD10[2];
    q.v[1].r = q.v[3].r = D_00440748[k].r;
    q.v[1].g = q.v[3].g = D_00440748[k].g;
    q.v[1].b = q.v[3].b = D_00440748[k].b;
    func_003BB4E8(&q, alpha, 0);
    q = D_0043FD10[3];
    q.v[0].r = q.v[2].r = D_00440748[k].r;
    q.v[0].g = q.v[2].g = D_00440748[k].g;
    q.v[0].b = q.v[2].b = D_00440748[k].b;
    func_003BB4E8(&q, alpha, 0);
}

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F228);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F238);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F248);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F258);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F268);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F278);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F288);

typedef struct {
    u16 val;
    s16 off;
    u16 pad4;
} SelGauge;
typedef struct {
    u8 pad0[0x42];
    SelGauge g[2];
} SelGaugeSet;
typedef struct {
    s32 state;
    u8 pad4[0x18];
} SelPlayer;
typedef struct {
    u8 pad0[0x20];
    SelPlayer p[2];
} SelPlayers;
extern const char D_0051F228[];
void func_003BF4B8(SelGaugeSet *r, SelPlayers *s);

/* NON_MATCHING: 87 lines, registers and the gauge segment clamps (original branch + movn order) */
#if 0
void func_003BF4B8(SelGaugeSet *r, SelPlayers *s) {
    s32 side, val, col, dir, x, a, c, w;
    s32 v0, v1, v2, v3;
    u64 cc;
    SelPlayer *pl;

    for (side = 0; side < 2; side++) {
        val = r->g[side].val;
        pl = &s->p[side];
        col = 6;
        if (func_003BDF60() & 2) {
            col = 6;
            if (pl->state == 3) {
                col = 2;
            }
        }
        dir = 1;
        if (side == 0) {
            dir = -1;
        }
        x = dir * 40 - 160;
        x = side * 320 - x + dir * r->g[side].off;
        Text_Printf(D_0051F228, x - dir * 20 - side * 65, 0x199, col, r->g[side].val);
        Sprite_SetFilter(1, 0);
        Sprite_SetBlend(1, 1);
        if (pl->state == 3) {
            a = gSinTable[(D_005F6E54 * 32) & 0xFE0];
            a = (a < 0 ? -a : a) >> 4;
            if (a >= 0x100) {
                a = 0xFF;
            }
            c = a / 2 + 0x60;
        } else {
            c = 0x80;
        }
        cc = c;
        D_005F6738 = cc | (cc << 8) | (cc << 16) | 0x80000000;
        w = dir * 9;
        Sprite_DrawScaled(1, 0x33, 0x34, x - w, 0x180, w, 0x1C);
        Sprite_DrawScaled(1, 0x35, 0x36, dir * 150 + x, 0x180, w, 0x1C);
        v3 = 50;
        if (val >= 101) {
            v3 = 150 - val;
        }
        v0 = val < 101 ? val : 100;
        v1 = 100 - val;
        v2 = val - 100;
        if (v1 < 0) {
            v1 = 0;
        }
        if (v2 < 0) {
            v2 = 0;
        }
        if (v3 < 0) {
            v3 = 0;
        }
        v0 *= dir;
        v1 *= dir;
        v2 *= dir;
        v3 *= dir;
        if (v0) {
            Sprite_DrawScaled(1, 0x2D, 0x2E, x, 0x180, v0, 0x1C);
        }
        if (v1) {
            Sprite_DrawScaled(1, 0x37, 0x38, x + v0, 0x180, v1, 0x1C);
        }
        if (v2) {
            Sprite_DrawScaled(1, 0x2F, 0x30, dir * 100 + x, 0x180, v2, 0x1C);
        }
        if (v3) {
            Sprite_DrawScaled(1, 0x31, 0x32, dir * 100 + x + v2, 0x180, v3, 0x1C);
        }
        D_005F6738 = 0x80808080;
        Sprite_SetFilter(1, 1);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results", func_003BF4B8);
#endif

s32 func_003BF7D0(void) {
    return 0x15;
}

s32 func_003BF7D8(s32 i) {
    return D_00440770[i].unk0;
}

void func_003BF7F0(void) {
    D_005F6660 = 0;
}

void func_003BF7F8(void) {
    D_005F6660 = 1;
}

s32 func_003BF808(void) {
    return D_005F6660;
}

void func_003BF810(void) {
    D_005F665E = 0x14;
}

void func_003BF820(s32 d, s32 b) {
    if (func_003CBEC8() == 0) {
        if (b) {
            Snd_PlaySe(5, 0);
            func_003CBEB8();
            __asm__ volatile(""); /* fake */
        } else {
            if (d) {
                Snd_PlaySe(3, 0);
            }
            D_005F665E += d;
            if (!(D_005F665E < func_003BF7D0())) {
                D_005F665E = 0;
            } else if (D_005F665E < 0) {
                D_005F665E = 0x14;
            }
        }
    }
}

void func_003BF8C8(void) {
    if (D_005F665E != 0x14) {
        D_005F5CB6 = D_00440770[D_005F665E].unk4;
    }
}

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F298);

void func_003BB890(s32 a, s32 x, s32 y, s32 b);
extern const char D_0051F298[];

void func_003BF8F8(void) {
    s32 col = 0x13;
    s32 name = func_003BF7D8(D_005F665E);
    s32 w;

    if (func_003CBEC8() == 0) {
        if (!(func_003BDF60() & 4)) {
            col = 2;
        }
        if ((func_003BDF60() / 24) & 1) {
            if (func_003BF808()) {
                func_003BB890(0, 0x101, 0x140, 5);
                func_003BB890(1, 0x173, 0x140, 5);
            }
        }
    }
    if (func_003BF808()) {
        w = Text_Printf(D_0051F1B8, 10, 4, name);
        Text_Printf(D_0051F298, 0x115, 0x140, 10, 4, 4);
        Text_Printf(D_0051F1C8, (640 - w) / 2, 0x154, 10, 4, col, name);
    }
}

s32 func_003BFA40(void) {
    return 0;
}

void func_003BFA48(void) {
    D_0043B84C[0] = 0xFFFF;
}

void func_003BFA58(void) {
    D_0043B84C[0] = D_005F5CB6;
}

s32 func_003BFA68(void) {
    return D_00439C78[0];
}

void func_003BFA78(s32 keep) {
    if (keep == 0) {
        func_0035CAF8();
    }
    D_00439C60.unk23 = 0;
    D_00439C60.unk37 = 0;
}

s32 func_003BFAA8(void) {
    s32 r = -1;
    u32 c = func_003BFA68();
    UnlockEnt *e = D_00440818;
    while (1) {
        if (e->thr == 0xFFFF) {
            r = -1;
            break;
        }
        if (c < e->thr) {
            break;
        }
        if (e->kind == 1) {
            r = e->arg;
        }
        e++;
    }
    return r;
}

s32 func_003BFB18(void) {
    s32 r = -1;
    u32 c = func_003BFA68();
    UnlockEnt *e = D_00440818;
    while (1) {
        if (e->thr == 0xFFFF) {
            r = -1;
            break;
        }
        if (c < e->thr) {
            if (e->kind == 1) {
                r = e->arg;
            }
            break;
        }
        e++;
    }
    return r;
}

void func_003BFB80(void) {
    if (D_00439C60.unk22 == 0) {
        D_00439C60.unk22 = 1;
    }
}

void func_003BFBA0(void) {
    D_00439C83[0] = 1;
}


void Unlock_ApplyAll(void) {
    u32 c = func_003BFA68();
    UnlockEnt *e;

    func_003BFA78(0);
    CharSet_Copy(D_0051E8F0, D_0043B9E0);
    for (e = D_00440818; e->thr != 0xFFFF && c >= e->thr; e++) {
        switch (e->kind) {
        case 0:
            func_0035CA00(e->arg);
            break;
        case 1:
            func_0035C998(e->arg);
            break;
        case 2:
            func_0035CA68(e->arg);
            break;
        case 3:
            func_0035CAE0();
            break;
        case 4:
            func_003BFBA0();
            break;
        case 5:
            func_003BFB80();
            break;
        case 6:
            func_003BFD98();
            break;
        case 7:
            break;
        }
    }
    func_003BFB18();
}

void Unlock_Step(void) {
    D_00439C60.unk18++;
    Unlock_ApplyAll();
    D_00439C60.unk20 = 0;
    func_00361968();
}

void func_003BFCF8(void) {
    if (++D_00439C60.unk22 < 4) {
        func_00361968();
    } else {
        D_00439C60.unk22 = 3;
    }
}

void func_003BFD30(s32 n) {
    if (n != -1) {
        D_00439C60.unk8 |= (u64)1 << n;
        func_00361968();
    }
}

void Unlock_SetClearBit(s32 n) {
    u64 old = D_00439C60.unk10;
    u64 m = old | ((u64)1 << n);
    if (m != old) {
        D_00439C60.unk10 = m;
        Unlock_Step();
    }
}

void func_003BFD98(void) {
    if (++D_00439C60.unk37 >= 2) {
        D_00439C60.unk37 = 1;
    }
}

u8 func_003BFDC8(void) {
    return D_00439C97[0];
}

s32 func_003BFDD8(s32 n) {
    return (D_00439C70[0] >> n) & 1;
}

void func_003BFDF8(s32 n) {
    if ((u32)(D_0043B7F8[0] - 8) >= 2) {
        D_00439C60.unk20 += n;
        func_00361968();
        if (D_00439C60.unk20 >= 50) {
            Unlock_Step();
            __asm__ volatile(""); /* fake */
        }
    }
}

s32 func_003BFE58(void) {
    return D_00439C70[0] & 0x40000;
}

void func_003BFE78(s32 m) {
    if ((D_00439C60.unk24 & m) == 0) {
        D_00439C60.unk24 |= m;
        func_00361968();
    }
}

s32 func_003BFEA0(s32 m) {
    return D_00439C84[0] & m;
}

u8 func_003BFEB0(void) {
    return D_00439C82[0];
}

s32 func_003BFEC0(void) {
    return D_00439C82[0] == 2;
}

s32 func_003BFED8(void) {
    return D_00439C70[0] != 0;
}

u8 func_003BFEE8(void) {
    return D_00439C83[0];
}

s32 func_003BFEF8(void) {
    s32 r = 0;
    if (func_003BFEE8()) {
        r = func_003BFEA0(1) != 0;
    }
    return r;
}

void func_003BFF38(void) {
}

s32 func_003BFF40(void) {
    return 0;
}

s32 func_003BFF48(s8 *a, s8 *b) {
    s32 i;
    for (i = 0; i < 0x22; i++) {
        if (a[i] != 0 && b[i] == 0) {
            return i;
        }
    }
    return -1;
}

void func_003BFF88(void) {
    func_003CF500();
    func_003CF608(0);
    func_003C6888();
}

void func_003BFFB0(void) {
    s32 i;
    for (i = 0; i < 4; i++) {
        s32 side = i >> 1;

        if (gFighters[side].unk402 == 1 && (i & 1) != func_003ADDD8(side)) {
            D_00440940[side][i & 1].unk8 = 0;
        } else {
            D_00440940[side][i & 1].unk8 = 1;
        }
    }
}

extern f32 D_00433440[];
/* The original calls __fixunssfsi for a float to u32 conversion; this compiler emits fptoui (decomp_guide.md, T17 b4). */
#ifndef PORT
__asm__(".set fptoui, __fixunssfsi"); /* fake */
#endif

u32 func_003C0098(void) {
    f32 d = gFighters[0].unk620;
    if (d < gFighters[1].unk620) {
        d = gFighters[1].unk620;
    }
    d += 1000.0f;
    return D_00433440[10] + D_00433440[14] / d;
}

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F2D0);

s32 func_003C0108(s32 x, s32 y, s32 hide, s32 idx, s32 w) {
    D_005F6738 = 0x80808080;
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    if (hide == 0) {
        x -= 0x18;
        Sprite_DrawScaled(1, 0x12F, D_0051F2D0[idx], x, y, 0x18, 0x15);
        x -= w;
        Sprite_DrawScaled(1, 0x12E, D_0051F2D0[idx], x, y, w, 0x15);
        x -= 8;
        Sprite_DrawScaled(1, 0x12D, D_0051F2D0[idx], x, y, 8, 0x15);
        x += 8;
    }
    return x;
}

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F2F0);

s32 func_003C0210(s32 c, u32 kind) {
    s32 r = 0;
    switch (kind) {
    case 0:
        r = 2;
        if (D_004FF690[c * 8 + 2] == -1 || D_0043B9B8[c] == 0) {
            r = 1;
        }
        break;
    case 1:
        r = 3;
        if (D_004FF690[c * 8 + 3] == -1 || D_0043B9B8[c] == 0) {
            r = 0;
        }
        break;
    case 3:
        r = 1;
        break;
    case 2: case 4: case 5:
        r = 0;
        break;
    }
    return r;
}

typedef struct {
    s32 v[34][3];
} ArcPrefTbl;
typedef struct {
    s32 v[4];
} ArcFallback;
typedef struct {
    s32 a;
    s32 b;
} ArcPair;
extern const ArcPrefTbl D_0051F3C8;
extern const ArcFallback D_0051F560;
extern u8 D_0043B990[];
extern const s8 D_005207F0[];
s32 CharSet_Count(void *set);

/* NON_MATCHING: 480 lines, same stack layout and length, register allocation differs throughout (pair / list pointer spills) */
#if 0
void Arcade_BuildOpponents(s32 side) {
    s8 av[0x30];
    s8 av2[0x30];
    s8 av3[0x30];
    s32 list[34];
    s32 used[6];
    ArcPair pairs[0x4C];
    s32 pick[2];
    ArcPrefTbl pref = D_0051F3C8;
    ArcFallback fb = D_0051F560;
    s32 slot[1];
    s32 i, j, k, n, np, r, m, m1, a, b, c, oc;

    for (i = 0; i < 7; i++) {
        for (j = 0; j < 2; j++) {
            D_00440A00[i].unk0[j + 2] = 5;
        }
    }
    CharSet_Copy(D_0043B990, av);
    m = D_00440940[side][0].unk0;
    m1 = D_00440940[side][1].unk0;
    av[m] = 0;
    av[m1] = 0;
    av[0x21] = 0;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 4; j++) {
            if (j == 3) {
                c = 0x22;
            } else {
                c = pref.v[m][j];
            }
            if (c == 0x22) {
                c = fb.v[0];
                if (!av[c]) {
                    for (k = 1; k < 4; k++) {
                        c = fb.v[k];
                        if (av[c]) {
                            break;
                        }
                    }
                }
                pick[i] = c;
                av[c] = 0;
                break;
            }
            if (av[c]) {
                pick[i] = c;
                av[c] = 0;
                break;
            }
        }
        m = m1;
    }
    CharSet_Copy(av, av3);
    av[0xE] = 0;
    av[0x14] = 0;
    av[0x1D] = 0;
    av[0xF] = 0;
    n = 0;
    for (i = 0; i < 0x4C; i++) {
        a = D_005207F0[i * 5];
        b = D_005207F0[i * 5 + 2];
        if (a != 0x22 && b != 0x22 && av[a] && av[b] && a != b) {
            pairs[n].a = a;
            pairs[n].b = b;
            n++;
        }
    }
    np = n;
    for (i = 0; i < 6; i++) {
        used[i] = 0;
    }
    for (i = 0; i < 6; i++) {
        list[i] = i;
    }
    k = 6;
    for (i = 0; i < 1; i++) {
        r = func_003800D8(k);
        k--;
        slot[i] = list[r];
        used[list[r]] = 1;
        for (; r < k; r++) {
            list[r] = list[r + 1];
        }
    }
    for (i = 0; i < 1; i++) {
        r = func_003800D8(np);
        a = pairs[r].a;
        b = pairs[r].b;
        av[a] = 0;
        av[b] = 0;
        D_00440A00[slot[i]].unk0[0] = a;
        D_00440A00[slot[i]].unk0[1] = b;
    restart:
        for (j = 0; j < np; j++) {
            if (pairs[j].a == a || pairs[j].b == a || pairs[j].a == b || pairs[j].b == b) {
                np--;
                for (k = j; k < np; k++) {
                    pairs[k] = pairs[k + 1];
                }
                goto restart;
            }
        }
    }
    for (i = 0; i < 6; i++) {
        if (used[i] == 1) {
            continue;
        }
        if (i == 3) {
            if (av3[0xE]) {
                av[0xE] = 1;
            }
            if (av3[0x14]) {
                av[0x14] = 1;
            }
            if (av3[0x1D]) {
                av[0x1D] = 1;
            }
            if (av3[0xF]) {
                av[0xF] = 1;
            }
        }
        n = 0;
        for (c = 0; c < 0x22; c++) {
            if (av[c]) {
                list[n++] = c;
            }
        }
        a = list[func_003800D8(n)];
        D_00440A00[i].unk0[0] = a;
        av[a] = 0;
        CharSet_Copy(av, av2);
        for (j = 0; j < np; j++) {
            if (CharSet_Count(av2) >= 2) {
                for (k = 0; k < 2; k++) {
                    if ((&pairs[j].a)[k] == a) {
                        av2[(&pairs[j].a)[1 - k]] = 0;
                    }
                }
            }
        }
        n = 0;
        for (c = 0; c < 0x22; c++) {
            if (av2[c]) {
                list[n++] = c;
            }
        }
        b = list[func_003800D8(n)];
        D_00440A00[i].unk0[1] = b;
        av[b] = 0;
    }
    D_00440A00[6].unk0[0] = pick[0];
    D_00440A00[6].unk0[1] = pick[1];
    D_00440A00[7].unk0[1] = 0x21;
    D_00440A00[7].unk0[0] = 0x21;
    m = D_00440940[side][0].unk0;
    m1 = D_00440940[side][1].unk0;
    for (i = 0; i < 7; i++) {
        for (j = 0; j < 2; j++) {
            r = func_003800D8(5);
            c = (s8)D_00440A00[i].unk0[j];
            oc = (s8)D_00440A00[i].unk0[1 - j];
            if (c == 0x1D) {
                r = 0;
            } else if (c == 0xB) {
                if (m == 0 || m1 == 0) {
                    r = 2;
                } else if (oc == 7) {
                    r = 1;
                } else if (oc == 0xD || oc == 0) {
                    r = 2;
                }
            }
            if ((u32)(r - 2) < 2) {
                if (D_004FF690[c * 8 + r] == -1 || D_0043B9B8[c] == 0) {
                    r = func_003C0210(c, r);
                }
            } else if (r >= 4) {
                if (D_004FF690[c * 8 + r] == -1 || func_003AE608(c, r) == 0) {
                    r = func_003C0210(c, r);
                }
            }
            D_00440A00[i].unk0[j + 2] = r;
        }
    }
    if (D_0043B990[0x1D]) {
        D_00440A00[7].unk0[2] = func_003800D8(2);
        D_00440A00[7].unk0[3] = 0;
    } else {
        D_00440A00[7].unk0[3] = 0;
        D_00440A00[7].unk0[2] = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results", Arcade_BuildOpponents);
#endif

void Team_BuildRandom(void) {
    s32 list[34];
    s32 forced = -1;
    s32 t = 0;
    s32 n;
    s32 i, side, m, c, cos;

    if (rand() & 1) {
        forced = func_003BFAA8();
        if (forced >= 0) {
            t = rand() & 1;
        }
    }
    n = 0;
    for (i = 0; i < 34; i++) {
        if (func_003ADA08(D_0043B968, i) && forced != i) {
            list[n++] = i;
        }
    }
    for (side = 0; side < 2; side++) {
        for (m = 0; m < 2; m++) {
            if (forced != -1 && t == 0) {
                c = forced;
            } else {
                s32 j = func_003800D8(n);
                c = list[j];
                n--;
                for (i = j; i < n; i++) {
                    list[i] = list[i + 1];
                }
            }
            t--;
            cos = func_003800D8(5);
            if ((u32)(cos - 2) < 2) {
                if (D_004FF690[c * 8 + cos] == -1 || D_0043B9B8[c] == 0) {
                    cos = func_003C0210(c, cos);
                }
            } else if (cos >= 4) {
                if (D_004FF690[c * 8 + cos] == -1 || func_003AE608(c, cos) == 0) {
                    cos = func_003C0210(c, cos);
                }
            }
            D_00440940[side][m].unk0 = c;
            D_00440940[side][m].unk4 = cos;
        }
    }
}


void func_003C0F48(s32 k, s32 skip) {
    s32 i;
    for (i = 0; i < 23; i++) {
        if (skip == 23 || D_01FAC7C0[k][i] < 2) {
            D_01FAC7C0[k][i] = D_0051F2F0[k][i];
        }
    }
    if (skip != 23) {
        if ((u32)skip < 23) {
            D_01FAC7C0[k][skip] = 0;
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F3C8);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F560);

s32 func_003C0FF0(s32 *a, s32 *out, s32 n) {
    s32 i, cnt = 0;
    for (i = 0; i < n; i++, a++) {
        if (*a == 1) {
            *out++ = i;
            cnt++;
        }
    }
    return cnt;
}

s32 func_003C1030(s32 k, s32 n) {
    s32 *p = D_01FAC7C0[k];
    s32 i, r = 0;
    for (i = 0; i < n; i++, p++) {
        if (*p == 1) {
            r++;
        }
    }
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F570);

void func_003C1078(s32 k, s32 v) {
    s32 i, j, o;
    s32 p;
    s32 r = -1;
    for (i = 0; i < 6; i++) {
        for (j = 0, o = 1; j < 2; j++, o--) {
            if (D_0051F570[i][j] == v) {
                p = D_0051F570[i][o];
                if (D_01FAC7C0[k][p] == 1) {
                    r = p;
                }
            }
        }
    }
    if (r >= 0) {
        if (k == 0) {
            D_01FAC7C0[0][r] = 0;
        } else {
            D_01FAC7C0[k][r] = 3;
        }
    }
}

void func_003C1160(s32 k) {
    s32 i;
    for (i = 0; i < 23; i++) {
        if (D_01FAC7C0[k][i] >= 2) {
            D_01FAC7C0[k][i]--;
        }
    }
}

s32 func_003C11B0(s32 k) {
    s32 buf[36];
    s32 r;
    r = buf[func_003800D8(func_003C0FF0(D_01FAC7C0[k], buf, 23))];
    D_01FAC7C0[k][r] = 0;
    func_003C1078(k, r);
    return r;
}

void func_003C1238(s32 a) {
}

void func_003C1240(void) {
    s32 i;
    func_003C0F48(0, 23);
    for (i = 0; i < 6; i++) {
        s32 r = func_003C11B0(0);
        func_003C1160(0);
        D_00440A00[i].unk4 = r;
    }
    D_00440A00[6].unk4 = 6;
    D_00440A00[7].unk4 = 0xE;
}

s32 func_003C12D0(s32 a) {
    s32 r;
    if (a == 1) {
        func_003C0F48(1, 23);
        return 0;
    }
    r = func_003C11B0(1);
    func_003C1238(1);
    func_003C1160(1);
    if (func_003C1030(1, 23) == 0) {
        func_003C0F48(1, r);
        func_003C1238(1);
    }
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F5A0);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F5B0);
INCLUDE_RODATA("asm/nonmatchings/select_results", D_0051F5C0);

s32 func_003C1350(s32 character, s32 side) {
    u8 *s = D_0043B7F8;
    s32 v = 0;
    switch (s[0]) {
    case 0: case 2: case 3: case 4: case 5: case 7: case 8:
        if (s[0x1A] == 2) {
            v = D_0051F5A0[D_005F5C5E];
        } else {
            v = D_0051F5A0[D_005F5C5C];
        }
        break;
    case 1: case 6:
        v = (*(u16 *)&s[side * 6 + 0x42] * 0x8C / 100) << 12;
        break;
    case 9:
        v = D_0051F5B0[0];
        break;
    }
    v = v * D_0051F5C0[character] / 100;
    if (character == 0x21 && gFighters[side].unk401 == 0) {
        v = v * 15 / 10;
    }
    return v;
}


/* Round start: a member that func_003AE420 marks (1) gets its maximum life from func_003C1350 (by character) and
 * full life; each member's statistic unk28 restarts at 100; the fighter of the side takes the life, red life and
 * maximum of its active member (unk0) and is standing again (unk40C = 1). */
void Battle_InitTeamLife(void) {
    s32 side;
    s32 m;
    Fighter *f;
    s32 life;

    for (side = 0; side < 2; side++) {
        f = &gFighters[side];
        for (m = 0; m < 2; m++) {
            if (func_003AE420(side, m) == 1) {
                func_003AE400(side, m, 0);
                life = func_003C1350(D_00440940[side][m].unk0, side);
                D_00440940[side][m].unk14 = life;
                D_00440940[side][m].unk10 = life;
                D_00440940[side][m].unkC = D_00440940[side][m].unk14;
                D_00440940[side][m].unk1C = 0;
                D_00440940[side][m].unk18 = 0;
            }
            f->unk40C = 1;
            D_00440940[side][m].unk28 = 100;
            f->unk3F4 = D_00440940[side][f->unk0].unkC;
            f->unk3F8 = D_00440940[side][f->unk0].unk10;
            f->unk3FC = D_00440940[side][f->unk0].unk14;
        }
    }
}

/* Partner recovery between the matches of team battle (type 2) and survival (type 4): the side that won the last
 * match (setup +0x20, not when +0x1F is 3) gets 15% of each member's maximum back on life and red life (the old
 * values go to the HUD refill fields); survival only while D_005F6668[side] is 0, and the side in +0x21 is marked
 * with func_003AE400; every other game type marks every member. Life and red life are then clamped to the maximum. */
/* NON_MATCHING: 115 lines (2 instructions longer), the original does not hoist the unk18 field base (rebuilt with lui / addiu per use) and saves t1 alone around the call */
#if 0
#define TEAM_REFILL(t)                               \
    do {                                             \
        (t).unk1C = (t).unk10;                       \
        (t).unk10 += (t).unk14 * 15 / 100;           \
        (t).unk18 = (t).unkC;                        \
        (t).unkC += (t).unk14 * 15 / 100;            \
    } while (0)

void func_003C1620(void) {
    u8 *s = D_0043B7F8;
    s32 side;
    s32 m;

    for (side = 0; side < 2; side++) {
        for (m = 0; m < 2; m++) {
            switch (D_0043B7F8[0]) {
            case 2:
                if (side == s[0x20] && s[0x1F] != 3) {
                    TEAM_REFILL(D_00440940[side][m]);
                }
                break;
            case 4:
                if (side == s[0x20] && D_005F6668[side] == 0) {
                    TEAM_REFILL(D_00440940[side][m]);
                }
                if (side == s[0x21]) {
                    func_003AE400(side, m, 1);
                }
                break;
            default:
                func_003AE400(side, m, 1);
                break;
            }
            if (D_00440940[side][m].unk14 < D_00440940[side][m].unk10) {
                D_00440940[side][m].unk10 = D_00440940[side][m].unk14;
            }
            if (D_00440940[side][m].unk14 < D_00440940[side][m].unkC) {
                D_00440940[side][m].unkC = D_00440940[side][m].unk14;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/select_results", func_003C1620);
#endif

/* Marks both members of both sides with func_003AE400(side, member, 1). */
void func_003C18B8(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        func_003AE400(i >> 1, i & 1, 1);
    }
}

/* Once per simulated frame while the round runs (func_0036E158() == 1, not paused, func_003A9698 clear): the active
 * fighter regains maximum / 6000 per frame up to its red life only when func_003AE3C0 is clear and unkD2 is 0; its
 * life and red life are copied to its member record, and the partner's record regains maximum / 6000 per frame up to
 * its red life. */
void Battle_RecoverLife(s32 side) {
    Fighter *f;
    Fighter *g;
    s16 m0;
    s16 m1;

    if (func_0036E158() != 1) {
        return;
    }
    if (D_005F5EE0 != 0) {
        return;
    }
    if (D_005F5EE4 != 0) {
        return;
    }
    if (func_003A9698() != 0) {
        return;
    }
    f = &gFighters[side];
    m0 = f->unk0;
    m1 = f->unk2;
    if (func_003AE3C0(f) == 0 && f->unkD2 == 0) {
        f->unk3F4 += f->unk3FC / 6000;
        if (f->unk3F8 < f->unk3F4) {
            f->unk3F4 = f->unk3F8;
        }
    }
    g = &gFighters[side];
    D_00440940[side][m0].unkC = g->unk3F4;
    D_00440940[side][m0].unk10 = g->unk3F8;
    D_00440940[side][m1].unkC += D_00440940[side][m1].unk14 / 6000;
    if (D_00440940[side][m1].unk10 < D_00440940[side][m1].unkC) {
        D_00440940[side][m1].unkC = D_00440940[side][m1].unk10;
    }
}
