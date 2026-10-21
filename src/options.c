/* 0x3B1478-0x3B4D50 (module options, docs/game_overview.md section 4; docs/systems/modes.md section 4). .rodata
 * 0x51DEF0-0x51E580 is its own (subsegment, every string and table included in place; the input tables at 0x51DEF0 are
 * read by main.c); both edges are [I]. */
#include "common.h"

extern s16 gMode;
extern s16 gSubMode;
extern u16 gFrameCount;
extern u8 D_00439C60[];

/* The options / unlock save block D_00439C60 (0x68 bytes, docs/systems/modes.md section 4) seen as a struct. */
typedef struct {
    u8 unk0[0x3C];
    u8 unk3C[4];
    u8 unk40[0x28];
} SaveBlk;
extern s8 D_00439C8C[];
extern s32 D_00439C7C[];
extern s32 D_005F6DB0;
extern s32 D_005F6DB4;
extern s8 D_005F6D88[3];
extern u16 D_005F62F0[3];
extern s16 D_005F6D98[4];
extern s16 D_005F6DA0;
extern s16 D_005F6DA2;
extern s16 D_005F6DA4;
extern s16 D_005F6DA6;
extern s16 D_005F6DA8;
extern u16 D_005F62AC;
extern s32 D_008D7E98[4];

extern s16 D_005F5C50;
extern u16 D_005F5C52;
extern s16 D_005F5C54;
extern s16 D_005F5C58;
extern s16 D_005F5C5A;
extern s16 D_005F5C5C;
extern s16 D_005F5C5E;
extern s16 D_005F5C60;
extern s16 D_005F5C62;
extern s16 D_005F5C64;
extern s16 D_005F5C66;
extern s16 D_005F5C68;
extern s16 D_005F5C6A;
extern s16 D_005F5C6C;
extern s16 D_005F5C6E;
extern s16 D_005F5C76;
extern s16 D_005F5C78;
extern s16 D_005F5C7A;

extern const char D_0051E0F0[];

typedef struct {
    u8 *value;
    u8 cur;
    u8 pad5[0x3B];
} OptItem;
extern OptItem D_00439CE0[];

/* Menu window quad: alpha limit, then four vertices (XYZ2 word, RGBAQ word), as modes_b.c's MbQuad. */
typedef struct {
    u16 x;
    u16 y;
    u32 z;
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    u32 q;
} OptVtx;
typedef struct {
    u16 alpha;
    u16 pad2[3];
    OptVtx v[4];
} __attribute__((aligned(8))) OptQuad;
extern OptQuad D_0043FD10[];
extern u8 D_0043A148[];
extern u8 D_0043A168[];
extern const char D_0051E0B8[];
typedef struct {
    s32 v[3][4];
} OptPosTbl;
extern const OptPosTbl D_0051E198;
extern const char D_005F64A8[];
extern const char D_005F64B0[];
void func_003BB680(OptQuad *q, s32 z, s32 unused);

s32 Text_Printf(const char *fmt, ...);
void Snd_PlaySe(s32 id, s32 player);
void Snd_SetStereo(s32 on);
void Gfx_GetDisplayOffset(s32 *dx, s32 *dy);
void Gfx_SetDisplayOffset(s32 dx, s32 dy);
void Input_BuildMapping(s32 player);
void Unlock_ApplyAll(void);
void func_003637D0(void);
void func_003638F0(void);
u64 *func_003639E0(void);
s32 Sprite_LoadBank(s32 bank, u64 *dest, s32 size);
void func_003CF608(s32 a);
void func_003BA818(s32 a, s32 b);
void func_003BB890(s32 a, s32 x, s32 y, s32 b);
s32 func_003B79E0(void);

void Options_Apply(void);
void func_003B15F8(s32 a, s32 b, s32 c);
void func_003B18E0(s32 i);
void func_003B18F8(s32 pad, s32 mask);
void func_003B1920(s32 a);
void func_003B4938(s32 pad, s32 trig);
extern s32 D_005F6730;
extern s32 D_005F5CD8;
extern s32 D_005F5CDC;
extern s32 D_005F5CE0;
extern s16 D_005F6D8C;
extern u64 D_005F6738;
extern u8 D_0043B7F8[];
extern char D_005F6320[];
extern char D_005F6478[];
extern const char D_0051E548[];
extern const char D_005F64C8[];
extern const char D_005F64D0[];
void func_003C4868(void);
void Sprite_SetBlend(s32 a, s32 b);
void func_003BF2F8(u8 *p, u16 *alpha);
s32 func_003616C8(void);
s32 func_003B1378(void);
void func_003992C8(s32 value);
void func_003B19C0(s32 a, s32 b);
void func_003B40C0(s32 a, s32 b, s32 c);
s32 Options_ControllerSetup(void);
void func_003B2978(s32 a, s32 b);
void func_003B1E78(s32 a, s32 pad);
s32 func_00360F40(s32 a, s32 b);
void func_003BD240(void);
void Mode_Change(s32 mode);
void func_003B1980(void);
void func_003B1860(void);
void func_003B1890(void);
void func_003B4718(void);
/* Drafts (NON_MATCHING) */
void func_003B2090(s32 pad);
void func_003B2358(s32 pad);
void func_003B2670(s32 pad);
void func_003B2B58(s32 col, s32 row);
void func_003B2C28(s32 col, s32 row, s32 color, s32 on);

INCLUDE_RODATA("asm/nonmatchings/options", D_0051DEF0);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DF28);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DF48);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DF58);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DF70);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DF80);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DF98);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DFA8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DFB8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DFC8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DFD8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DFE8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051DFF8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E008);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E020);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E030);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E050);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E060);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E070);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E080);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E090);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E0A0);

void func_003B1478(void) {
    Gfx_GetDisplayOffset(&D_005F6DB0, &D_005F6DB4);
}

/* NON_MATCHING: 56 lines, load order of the option bytes; the 0x29 copy is scheduled late */
#if 0
void Options_Apply(void) {
    s32 i;
    u8 *b = D_00439C60;

    D_005F5C58 = (s8)b[0x26];
    D_005F5C5A = (s8)b[0x27];
    D_005F5C66 = (s8)b[0x28];
    D_005F5C60 = (s8)b[0x2A];
    D_005F5C64 = (s8)b[0x2B];
    D_005F5C6E = (s8)b[0x2C];
    D_005F5C52 = b[0x2D] & 1;
    D_005F5C7A = (s8)b[0x30];
    D_005F5C5E = 3;
    D_005F5C6A = (s8)b[0x29];
    D_005F5C54 = 1;
    D_005F5C68 = (s8)b[0x29];
    D_005F5C5C = 3;
    D_005F5C62 = 1;
    D_005F5C50 = 1;
    D_005F5C6C = 0;
    for (i = 0; i < 4; i++) {
        Input_BuildMapping(i);
    }
    D_005F5C76 = (s8)b[0x2E];
    D_005F5C78 = (s8)b[0x2F];
    Gfx_SetDisplayOffset(((s8 *)D_00439C60)[0x34] + D_005F6DB0, ((s8 *)D_00439C60)[0x35] + D_005F6DB4);
    Snd_SetStereo(D_005F5C52);
    Unlock_ApplyAll();
}
#else
INCLUDE_ASM("asm/nonmatchings/options", Options_Apply);
#endif

u8 func_003B15D0(s32 i) {
    return ((SaveBlk *)D_00439C60)->unk3C[i];
}

s32 func_003B15E8(void) {
    return D_00439C8C[0];
}

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E0B8);

/* NON_MATCHING: 26 lines, colour store order of the second quad copy */
#if 0
void func_003B15F8(s32 s, s32 row, s32 color) {
    OptQuad q;
    u8 *c = &D_0043A148[color * 3];
    s32 y = row << 4;
    s32 x = (Text_Printf(D_0051E0B8, 10, 4, s) + 0xA0) << 4;
    u8 r = c[0], g = c[1], b = c[2];

    q = D_0043FD10[21];
    q.v[0].y += y;
    q.v[1].y += y;
    q.v[2].y += y;
    q.v[3].y += y;
    q.v[1].x += x;
    q.v[3].x += x;
    q.v[0].r = q.v[1].r = q.v[2].r = q.v[3].r = r;
    q.v[0].g = q.v[1].g = q.v[2].g = q.v[3].g = g;
    q.v[0].b = q.v[1].b = q.v[2].b = q.v[3].b = b;
    func_003BB680(&q, 0xFFEF0, 0);

    r = c[0];
    g = c[1];
    b = c[2];
    q = D_0043FD10[21];
    q.v[0].y += y;
    q.v[1].y += y;
    q.v[2].y += y;
    q.v[3].y += y;
    q.v[0].r = q.v[1].r = q.v[2].r = q.v[3].r = r;
    q.v[0].g = q.v[1].g = q.v[2].g = q.v[3].g = g;
    q.v[0].b = q.v[1].b = q.v[2].b = q.v[3].b = b;
    q.v[1].x = q.v[3].x = 0x6C00;
    func_003BB680(&q, 0xFFEF0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B15F8);
#endif

void func_003B1820(void) {
    u32 i;
    for (i = 0; i < 14; i++) {
        if (D_00439CE0[i].value != 0) {
            *D_00439CE0[i].value = D_00439CE0[i].cur;
        }
    }
    Options_Apply();
}

void func_003B1860(void) {
    s32 i;
    for (i = 0; i < 3; i++) {
        D_005F6D88[i] = 0;
    }
}

void func_003B1890(void) {
    s32 i;
    for (i = 0; i < 3; i++) {
        if (D_005F6D88[i] == 0) continue;
        Snd_PlaySe(D_005F62F0[i], 0);
        break;
    }
}

void func_003B18E0(s32 i) {
    D_005F6D88[i] = 1;
}

void func_003B18F8(s32 pad, s32 mask) {
    if (pad & (mask | 0x100)) {
        gSubMode = 1;
        func_003B18E0(1);
    }
}

void func_003B1920(s32 a) {
    s32 i;
    s16 *b = &D_005F6DA0;
    for (i = 0; i < 4; i++) {
        func_003BA818(i, a);
        b[i] = 0;
    }
}

void func_003B1980(void) {
    func_003CF608(5);
    func_003637D0();
    Sprite_LoadBank(8, func_003639E0(), 0x249F0);
    func_003638F0();
}

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E0C8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E0E0);

extern const char D_0051E0C8[];
extern const char D_0051E0E0[];
extern const char *D_00439CC8[];
void func_003B4930(void);
extern s16 D_005F6DAC;
extern s16 D_005F6D90;
s32 func_0039C778(void);
void func_003B6218(void);
void func_003B2D78(void);
void func_00361678(void);
void func_00361968(void);
s32 func_003B1468(void);
void func_003DEB80(s32 a);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 z, u32 color);
void func_003E5268(s32 x, s32 y, s32 id, u32 color);
extern s16 D_005F6D8E;

/* NON_MATCHING: 49 lines, register allocation (trig, col and the loop temporaries in other saved registers) */
#if 0
void func_003B19C0(s32 pad, s32 trig) {
    OptQuad q;
    s32 d, u, i, y, col, back, ok, dy;

    if (func_0039C778() == 0) {
        Text_Printf(D_0051E0C8, 0xC1, 0x28, 10, 5, 0x13);
    }
    u = (pad >> 14) & 1;
    d = pad >> 12;
    d &= 1; /* fake: the mask as its own statement */
    d = u - d;
    D_005F6D8C += d;
    if (D_005F6D8C < 0) {
        D_005F6D8C = 5;
    } else if (D_005F6D8C >= 6) {
        D_005F6D8C = 0;
    }
    if (d) {
        func_003B18E0(2);
    }
    ok = trig & 0x8FC;
    back = trig & 0x100;
    for (i = 0, y = 0x5A; i < 6; i++, y += 0x1E) {
        dy = (y - 4) * 16;
        if (D_005F6D8C == i) {
            q = D_0043FD10[14];
            q.v[0].y += dy;
            q.v[1].y += dy;
            q.v[2].y += dy;
            q.v[3].y += dy;
            func_003BB680(&q, 0xFF0, 0);
            q = D_0043FD10[14];
            q.v[0].y += dy;
            q.v[1].y += dy;
            q.v[2].y += dy;
            q.v[3].y += dy;
            q.v[3].x = q.v[1].x = 0x6C00;
            func_003BB680(&q, 0xFF0, 0);
            q = D_0043FD10[20];
            q.v[0].r = q.v[1].r = q.v[2].r = q.v[3].r = 0xFF;
            q.v[0].g = q.v[1].g = q.v[2].g = q.v[3].g = 0xFF;
            q.v[0].b = q.v[1].b = q.v[2].b = q.v[3].b = 0xFF;
            col = 0xB;
        } else {
            q = D_0043FD10[20];
            col = 5;
        }
        dy = (y + 12) * 16;
        q.v[0].y += dy;
        q.v[1].y += dy;
        q.v[2].y += dy;
        q.v[3].y += dy;
        func_003BB680(&q, 0xFF0, 0);
        if (func_0039C778() == 0) {
            Text_Printf(D_0051E0E0, 0x28, y, 10, 4, col, D_00439CC8[i]);
        }
    }
    if (func_003B1468()) {
        func_003DEB80(0);
        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
        func_003E5268(0x1E, 0x186, D_005F6D8C, 0x60606080);
    }
    if (ok) {
        func_003B18E0(0);
        D_005F6DAC = -1;
        D_005F6D8E = 0;
        gSubMode = D_005F6D8C + 2;
        D_005F6D90 = 0;
        func_003B4930();
        if (gSubMode == 3) {
            func_003B6218();
            func_003B1920(1);
            func_003B2D78();
        } else if (gSubMode == 6) {
            func_00361678();
        }
    }
    if (back) {
        func_003B18E0(1);
        gSubMode = 7;
        func_00361968();
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B19C0);
#endif

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E0F0);

void func_003B1E78(s32 a, s32 pad) {
    func_003B4938(a, pad);
    Text_Printf(D_0051E0F0, 0xB2, 0x28, 10, 5, 0x13);
    func_003B18F8(pad, 0x8EC);
}


s32 func_003B1EC8(s32 c) {
    switch (c) {
    case 0: case 20: case 30:
        return 0;
    case 1: case 21: case 31:
        return 1;
    case 2: case 22: case 32:
        return 2;
    default:
        return 3;
    }
}

void func_003B1F10(void) {
    if ((gFrameCount / 24) & 1) {
        func_003BB890(0, 0x99, 0x46, 5);
        func_003BB890(1, 0x1E3, 0x46, 5);
    }
}

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E198);

void func_003B1F78(s32 i, s32 flags) {
    OptPosTbl t = D_0051E198;

    if ((gFrameCount / 24) & 1) {
        if (flags & 1) {
            Text_Printf(D_005F64A8, t.v[i][0], t.v[i][1], 5);
        }
        if (flags & 2) {
            Text_Printf(D_005F64B0, t.v[i][2], t.v[i][3], 5);
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E1C8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E1E8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E208);

typedef struct {
    s32 unk0;
    u8 spr[2];
    char name[6];
} OptRecEntry;
typedef struct {
    u8 spr[2];
    u16 cnt;
    char name[4];
} OptRecEntry8;
typedef struct {
    u8 pad0[8];
    OptRecEntry e[10];
    OptRecEntry8 f[20];
    u8 pad120[4];
    u16 n;
    u16 list[0x27];
    u16 cnt[1];
} OptRecords;
extern OptRecords D_00440D50;
typedef struct {
    const char *name;
    u8 spr;
} OptChar;
extern OptChar *D_00415840[];
extern const char *D_0043A158[];
extern const char D_0051E1C8[];
extern const char D_0051E1E8[];
extern const char D_0051E208[];
extern s16 D_005F6DAA;
s32 func_003CBFA0(s32 a);
void Sprite_DrawScaled(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);

/* NON_MATCHING: 25 lines, the arrow flags test becomes movz, registers of the scroll clamp */
#if 0
void func_003B2090(s32 pad) {
    OptRecords *rec = &D_00440D50;
    s32 n;
    s32 save;
    s32 u, d, col, i, j, k, idx, cnt;
    s32 y = 0x82;

    save = D_005F6730;
    n = 6;
    col = 3;
    if (rec->n < 7) {
        n = rec->n;
    }
    u = (pad >> 14) & 1;
    d = pad >> 12;
    d &= 1; /* fake: the mask as its own statement */
    d = u - d;
    D_005F6DAA += d;
    if (D_005F6DAA < 0) {
        D_005F6DAA = 0;
    } else if (D_005F6DAA > rec->n - 6) {
        D_005F6DAA = rec->n - 6;
    } else if (d) {
        func_003B18E0(2);
    }
    if (D_005F6DAA == 0) {
        col = 2;
    } else if (D_005F6DAA == rec->n - 6) {
        col = 1;
    }
    Text_Printf(D_0051E1C8, 0xC0, 0x46, 10, 4, 0xE);
    func_003B1F10();
    func_003B1F78(0, col);
    D_005F6730 = 0xFFFF0;
    i = D_005F6DAA;
    for (j = 0; j < n; j++) {
        func_003B15F8((s32)D_0051E1E8, y + 2, 1);
        idx = func_003B1EC8(i);
        k = rec->list[i];
        cnt = rec->cnt[k];
        Text_Printf(D_0051E208, 0x46, y - 4, 10, 5, 0xE, i + 1, 4, y, D_0043A158[idx], 0xB, 0xD2,
                    D_00415840[k * 8]->name, 0x19A, y - 6, 10, 5, 0xE, cnt / 10, cnt % 10);
        i++;
        Sprite_DrawScaled(1, func_003CBFA0(D_00415840[k * 8]->spr), 0x84, 0xA0, y - 0x2E, 0x28, 0x2A);
        y += 0x32;
    }
    D_005F6730 = save;
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B2090);
#endif

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E238);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E258);

extern const char D_0051E238[];
extern const char D_0051E258[];

/* NON_MATCHING: 29 lines, the arrow flags test becomes xor/movz (original bnel/beql), registers of the scroll clamp */
#if 0
void func_003B2358(s32 pad) {
    OptRecords *rec = &D_00440D50;
    s32 n;
    s32 save;
    s32 u, d, col, i, j, k, idx, cnt;
    s32 y = 0x82;
    OptRecEntry8 *e;

    save = D_005F6730;
    D_005F6730 = 0xFFFF0;
    n = 6;
    col = 3;
    if (rec->n < 7) {
        n = rec->n;
    }
    u = (pad >> 14) & 1;
    d = pad >> 12;
    d &= 1; /* fake: the mask as its own statement */
    d = u - d;
    D_005F6DAA += d;
    if (D_005F6DAA < 0) {
        D_005F6DAA = 0;
    } else if (D_005F6DAA > rec->n - 6) {
        D_005F6DAA = rec->n - 6;
    } else if (d) {
        func_003B18E0(2);
    }
    if (D_005F6DAA == 0) {
        col = 2;
    } else if (D_005F6DAA == rec->n - 6) {
        col = 1;
    }
    Text_Printf(D_0051E238, 0xAD, 0x46, 10, 4, 0xF);
    func_003B1F10();
    func_003B1F78(1, col);
    i = D_005F6DAA;
    for (j = 0; j < n; j++) {
        func_003B15F8((s32)D_0051E1E8, y + 2, 2);
        idx = func_003B1EC8(i);
        k = rec->list[i];
        e = &rec->f[k];
        cnt = e->cnt;
        if (cnt >= 1000) {
            cnt = 999;
        }
        Text_Printf(D_0051E258, 0x46, y - 4, 10, 5, 0xF, i + 1, 4, y, D_0043A158[idx], 0xB, 5, 0x104, y - 4, cnt, 4,
                    0x152, y, cnt >= 2 ? 0x53 : 0x20, 0x1CC, y - 6, 10, 5, 0xF, e->name);
        i++;
        Sprite_DrawScaled(1, func_003CBFA0(e->spr[0]), 0x84, 0xA0, y - 0x2E, 0x28, 0x2A);
        Sprite_DrawScaled(1, func_003CBFA0(e->spr[1]), 0x84, 0xD2, y - 0x2E, 0x28, 0x2A);
        y += 0x32;
    }
    D_005F6730 = save;
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B2358);
#endif

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E290);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E2A8);

extern const char D_0051E290[];
extern const char D_0051E2A8[];
void Time_FramesToClock(s32 v, s32 *a, s32 *b, s32 *c);

/* NON_MATCHING: 9 lines, the arrow flags test becomes xor/movz (original beql), same as func_003B2090 */
#if 0
void func_003B2670(s32 pad) {
    OptRecords *rec = &D_00440D50;
    s32 n;
    s32 save;
    s32 u, d, col, i, j, k, idx;
    s32 a, b, c;
    s32 y = 0x82;
    OptRecEntry *e;

    save = D_005F6730;
    D_005F6730 = 0xFFFF0;
    n = 6;
    col = 3;
    if (rec->n < 7) {
        n = rec->n;
    }
    u = (pad >> 14) & 1;
    d = pad >> 12;
    d &= 1; /* fake: the mask as its own statement */
    d = u - d;
    D_005F6DAA += d;
    if (D_005F6DAA < 0) {
        D_005F6DAA = 0;
    } else if (D_005F6DAA > rec->n - 6) {
        D_005F6DAA = rec->n - 6;
    } else if (d) {
        func_003B18E0(2);
    }
    Text_Printf(D_0051E290, 0xE6, 0x46, 10, 4, 6);
    if (D_005F6DAA == 0) {
        col = 2;
    } else if (D_005F6DAA == rec->n - 6) {
        col = 1;
    }
    func_003B1F10();
    func_003B1F78(2, col);
    i = D_005F6DAA;
    for (j = 0; j < n; j++) {
        func_003B15F8((s32)D_0051E1E8, y + 2, 3);
        k = rec->list[i];
        e = &rec->e[k];
        idx = func_003B1EC8(i);
        Time_FramesToClock(e->unk0, &a, &b, &c);
        Text_Printf(D_0051E2A8, 0x46, y - 4, 10, 5, 6, i + 1, 4, y, D_0043A158[idx], 0xB, 0x104, a, b, c, 0x1CC,
                    y - 6, 10, 5, 6, e->name);
        i++;
        Sprite_DrawScaled(1, func_003CBFA0(e->spr[0]), 0x84, 0xA0, y - 0x2E, 0x28, 0x2A);
        Sprite_DrawScaled(1, func_003CBFA0(e->spr[1]), 0x84, 0xD2, y - 0x2E, 0x28, 0x2A);
        y += 0x32;
    }
    D_005F6730 = save;
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B2670);
#endif

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E2E0);

extern s16 D_005F6D8E;
extern s16 D_005F6DAA;
extern const char D_0051E2E0[];
void func_003B2090(s32 a);
void func_003B2358(s32 a);
void func_003B2670(s32 a);
void func_003C9388(void);
void func_003C9378(void);
void func_003C9368(void);
s32 func_003B1468(void);
void func_003DEB80(s32 a);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 z, u32 color);
void func_003E5268(s32 x, s32 y, s32 id, u32 color);

void func_003B2978(s32 a, s32 pad) {
    s32 u = (pad >> 13) & 1;
    s32 d = pad >> 15;

    d &= 1; /* fake: the mask as its own statement */
    d = u - d;

    if (d < 0) {
        d *= 3;
    }
    D_005F6D8E += d;
    if (D_005F6D8E >= 6) {
        D_005F6D8E = 0;
    } else if (D_005F6D8E < 0) {
        D_005F6D8E = 4;
    }
    if (d) {
        func_003B18E0(2);
    }
    switch (D_005F6D8E) {
    case 0:
        D_005F6DAA = 0;
        func_003C9388();
        D_005F6D8E = 1;
    case 1:
        func_003B2090(a);
        break;
    case 2:
        func_003C9378();
        D_005F6DAA = 0;
        D_005F6D8E = 3;
    case 3:
        func_003B2358(a);
        break;
    case 4:
        func_003C9368();
        D_005F6DAA = 0;
        D_005F6D8E = 5;
    case 5:
        func_003B2670(a);
        break;
    }
    Text_Printf(D_0051E2E0, 0xF3, 0x28, 10, 5, 0x13);
    if (func_003B1468()) {
        func_003DEB80(0);
        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
        func_003E5268(0x1E, 0x186, D_005F6D8E / 2 + 0x18, 0x60606080);
    }
    func_003B18F8(pad, 0x6FF);
}

s32 func_003B2B30(s32 i, s32 v) {
    return D_005F6D98[i] == v ? 11 : 5;
}

/* NON_MATCHING: 27 lines, order of the x / y stores and the quad copy registers */
#if 0
void func_003B2B58(s32 col, s32 row) {
    OptQuad q;
    s32 c = col << 4;
    s32 x1 = c + 0x7900;
    s32 y = row << 4;
    s32 x0 = c + 0x6C00;
    q = D_0043FD10[21];
    q.v[0].y += y;
    q.v[1].y += y;
    q.v[2].y += y;
    q.v[3].y += y;
    q.v[0].x = q.v[2].x = x0;
    q.v[1].x = q.v[3].x = x1;
    func_003BB680(&q, 0xFFEF0, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B2B58);
#endif

/* NON_MATCHING: 31 lines, order of the x / y adds and the alpha register */
#if 0
void func_003B2C28(s32 col, s32 row, s32 color, s32 on) {
    OptQuad q;
    u8 *c = &D_0043A168[color * 3];
    u8 r = c[0], g = c[1], b = c[2];
    s32 x = col << 4;
    s32 y = row << 4;
    u8 a;

    q = D_0043FD10[32];
    a = on ? 0x80 : 0x38;
    q.v[0].x += x;
    q.v[1].x += x;
    q.v[2].x += x;
    q.v[3].x += x;
    q.v[0].y += y;
    q.v[1].y += y;
    q.v[2].y += y;
    q.v[3].y += y;
    q.v[0].r = q.v[1].r = q.v[2].r = q.v[3].r = r;
    q.v[0].g = q.v[1].g = q.v[2].g = q.v[3].g = g;
    q.v[0].b = q.v[1].b = q.v[2].b = q.v[3].b = b;
    q.v[0].a = q.v[1].a = q.v[2].a = q.v[3].a = a;
    func_003BB680(&q, 0xFFEE0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B2C28);
#endif

void func_003B2D78(void) {
    s32 i;
    for (i = 0; i < 4; i++) {
        D_005F6D98[i] = 0;
        (&D_005F6DA0)[i] = 0;
        D_008D7E98[i] = 0;
    }
}

void func_003B2DC8(void) {
    if (func_003B79E0() == 0) {
        func_003B1920(0);
    }
    D_005F6DA0 = 0;
    D_005F6DA2 = 0;
    D_005F6DA4 = 0;
    D_005F6DA6 = 0;
    func_003B18E0(1);
}

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E318);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E338);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E358);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E368);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E3A8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E3B8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E3C8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E3E0);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E400);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E418);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E428);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E438);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E448);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E468);

typedef struct {
    s32 v[7];
} OptStrOff;
typedef struct {
    s32 x;
    s32 y;
} OptXY;
typedef struct {
    OptXY v[4];
} OptXY4;
typedef struct {
    OptXY v[2];
} OptXY2;
typedef struct {
    OptXY v[8];
} OptXY8;
typedef struct {
    u32 mask;
    const char *label;
    s8 kind;
    s8 font;
    s8 dx;
    u8 padB;
} OptBtn;
extern const OptStrOff D_0051E318;
extern const OptXY4 D_0051E338;
extern const OptXY2 D_0051E358;
extern const OptXY8 D_0051E368;
extern const OptXY4 D_0051E428;
extern const char D_0051E3A8[];
extern const char D_0051E3B8[];
extern const char D_0051E3C8[];
extern const char D_0051E3E0[];
extern const char D_0051E400[];
extern const char D_0051E418[];
extern const char D_0051E438[];
extern const char D_0051E448[];
extern const char D_0051E468[];
extern u16 D_005F5CB0[4];
extern u32 D_00415718[];
extern u32 D_00415728[];
extern u8 D_00439C9C[];
extern u8 D_00439CA0[];
extern OptBtn D_0043A0A0[];
extern const char *D_0043A068[];
extern const char *D_0043A060[];
extern s32 D_0043A100[];
extern const char *D_0043A120[];
extern s32 D_005F6730;
typedef struct {
    s16 unk0;
    u8 pad2[0xB89E];
} OptFighter;
extern OptFighter gFighters[];
s32 Pad_CountReady(void);
s32 func_003B0078(s32 a, s32 port);
s32 func_003AFFB8(s32 i, s32 a);
s32 func_003E93F8(void);
s32 Pad_GetPlayerMode(s32 player);
void func_003BA770(s32 port, s32 *slot, s32 *portOut);
s32 scePadInfoAct(s32 port, s32 slot, s32 actno, s32 term);
void Input_ResetMapping(s32 player);
u8 func_003B15D0(s32 i);
s32 func_003BA888(s32 i, s32 a, s32 b);
void Sprite_Draw(s32 a, s32 b, s32 c, s32 x, s32 y);
s32 func_003B1418(s32 i);
u32 strlen(const char *s);
s32 func_003B00F0(s32 port);
void Ui_DrawWindow(s32 x, s32 y, s32 w, s32 h, s32 z, u32 c0, u32 c1);
s32 func_003F1F08(void);
void func_003BAA90(void);

/* NON_MATCHING: 837 lines, structure and length close, stack slots of the spilled locals and register allocation differ throughout */
#if 0
s32 Options_ControllerSetup(void) {
    OptStrOff off = D_0051E318;
    OptXY4 pos4;
    OptXY2 pos2;
    OptXY8 bpos;
    OptXY4 wpos;
    s32 slot, port;
    s32 i, p, k, id, sel, state, kind, flag, ret, all, drawn, nready, yoff;
    s32 trig, rep, d, dd, skip, v, col, x, y, x2, y2, n;
    OptXY *pp;
    OptXY *bp;
    u8 *m;
    const char *name;

    ret = 0;
    all = 0;
    drawn = 0;
    if (gMode == 9) {
        func_003CF608(5);
    } else {
        func_003B1860();
    }
    nready = Pad_CountReady();
    for (i = 0; i < 4; i++) {
        if (func_003B79E0()) {
            p = func_003B0078(1, i);
            if (!D_005F5CB0[p]) {
                continue;
            }
            if (func_003AFFB8(p, gFighters[p].unk0) != i) {
                continue;
            }
            if (D_0043B7F8[0] == 8 && func_003E93F8() != p) {
                continue;
            }
        }
        trig = D_00415728[i];
        v = D_008D7E98[i];
        sel = -1;
        all |= trig;
        flag = 0;
        rep = v & D_00415718[i];
        D_008D7E98[i] = v | trig;
        switch (Pad_GetPlayerMode(i)) {
        case 5:
            kind = 1;
            break;
        case 2:
            kind = 4;
            break;
        case 4:
            kind = 1;
            break;
        case 7:
            kind = 2;
            func_003BA770(i, &slot, &port);
            if (scePadInfoAct(port, slot, -1, 0) <= 1) {
                kind = 1;
            }
            break;
        case 0x300:
            kind = 1;
            break;
        default:
            kind = 0;
            break;
        }
        d = ((trig >> 14) & 1) - ((trig >> 12) & 1);
        if (rep & 0xFF) {
            d = 0;
        }
        D_005F6D98[i] += d;
        if (gMode == 4 && D_0043B7F8[0] == 8) {
            skip = 0;
            if (D_005F6D98[i] >= 4) {
                D_005F6D98[i] = 0;
            }
            while (D_005F6D98[i] == 0 || D_005F6D98[i] == 2 || (kind != 2 && D_005F6D98[i] == 1)) {
                if (d) {
                    D_005F6D98[i] += d;
                } else {
                    skip = 1;
                    D_005F6D98[i] += 1;
                }
                if (D_005F6D98[i] < 0) {
                    D_005F6D98[i] = 3;
                } else if (D_005F6D98[i] >= 4) {
                    D_005F6D98[i] = 0;
                }
            }
            if (skip) {
                trig &= ~0xFC;
            }
        }
        if (kind != 2) {
            if (D_005F6D98[i] == 1) {
                if (!d) {
                    d = 1;
                }
                D_005F6D98[i] += d;
            }
            flag = 1;
        }
        if (D_005F6D98[i] < 0) {
            D_005F6D98[i] = 3;
        } else if (D_005F6D98[i] >= 4) {
            D_005F6D98[i] = 0;
        }
        if (d) {
            func_003B18E0(2);
        }
        state = D_005F6D98[i];
        switch (state) {
        case 0:
            for (k = 0; k < 8; k++) {
                if ((D_0043A0A0[k].kind & kind) && (D_0043A0A0[k].mask & rep)) {
                    sel = k;
                    break;
                }
            }
            if (sel != -1) {
                m = &D_00439CA0[i * 10 + sel];
                dd = ((trig >> 13) & 1) - ((trig >> 15) & 1);
                *m += dd;
                if ((s8)*m < 0) {
                    *m = 0xD;
                } else if ((s8)*m >= 0xE) {
                    *m = 0;
                }
                if (dd) {
                    func_003B18E0(2);
                    func_00361968();
                    Input_BuildMapping(i);
                }
            }
            break;
        case 1:
            if (trig & 0xA000) {
                D_00439C9C[i] ^= 1;
                func_003B18E0(2);
                func_003BA818(i, func_003B15D0(i));
                func_00361968();
                if (D_00439C9C[i]) {
                    (&D_005F6DA0)[i] = 0x3C;
                }
            }
            break;
        case 2:
            if (trig & 0xFC) {
                func_003B18E0(0);
                Input_ResetMapping(i);
                func_003BA818(i, func_003B15D0(i));
                func_00361968();
            }
            break;
        case 3:
            if (trig & 0xFC) {
                func_003B2DC8();
                ret |= 1;
            }
            break;
        }
        v = (&D_005F6DA0)[i];
        if (v > 0) {
            (&D_005F6DA0)[i] = --v;
        }
        func_003BA888(i, (s16)v != 0, 0);
        if (gMode == 9) {
            pos4 = D_0051E338;
            pos2 = D_0051E358;
            bpos = D_0051E368;
            if (Pad_GetPlayerMode(i) < 0) {
                continue;
            }
            if (nready >= 3) {
                yoff = 0;
                pp = &pos4.v[i];
            } else if (nready == 2) {
                yoff = 0x14;
                pp = &pos2.v[drawn];
            } else {
                yoff = 0x14;
                pp = &pos2.v[i & 1];
            }
            x = pp->x;
            y = pp->y;
            D_005F6730 = 0xFFB00;
            drawn++;
            Sprite_Draw(1, 0x12B, 0x12C, x, y);
            x2 = x + 0x3C;
            for (k = 0; k < 8; k++) {
                id = D_0043A100[k];
                bp = &bpos.v[k];
                m = &D_00439CA0[i * 10 + id];
                if (!(D_0043A0A0[id].kind & kind)) {
                    continue;
                }
                col = 6;
                if (state == 0 && id == sel) {
                    col = 5;
                }
                n = strlen(D_0043A068[(s8)*m]);
                D_005F6730 = 0xFFB10;
                Text_Printf(D_0051E3A8, col, x + bp->x + off.v[n % 7], y + bp->y, D_0043A068[(s8)*m]);
                func_003B2C28(x + bp->x, y + bp->y, k, id == sel);
                if (id == sel) {
                    func_003BB890(0, x + bp->x - 0x14, y + bp->y, 5);
                    func_003BB890(1, x + bp->x + 0x64, y + bp->y, 5);
                }
            }
            y2 = y + 0x8C + yoff;
            Text_Printf(D_0051E3B8, 10, 5, 0xB, x - 8, y2 - 5, func_003B1418(i));
            col = func_003B2B30(i, 0);
            Text_Printf(D_0051E3C8, 10, 4, col, x2, y2);
            if (col == 0xB) {
                func_003B2B58(x2, y2);
            }
            y2 += 0x12;
            col = func_003B2B30(i, 1);
            if (col == 0xB) {
                func_003B2B58(x2, y2);
                func_003BB890(1, pp->x + 0x119, y2, 5);
            }
            if (flag) {
                v = 0;
                col = 0x16;
            } else {
                v = D_00439C9C[i];
            }
            Text_Printf(D_0051E3E0, 10, 4, col, x2, y2, v ? 0xC : 0x10, D_0043A060[v]);
            y2 += 0x12;
            col = func_003B2B30(i, 2);
            Text_Printf(D_0051E400, 10, 4, col, x2, y2);
            if (col == 0xB) {
                func_003B2B58(x2, y2);
            }
            y2 += 0x12;
            col = func_003B2B30(i, 3);
            Text_Printf(D_0051E418, 10, 4, col, x2, y2);
            if (col == 0xB) {
                func_003B2B58(x2, y2);
            }
        } else {
            wpos = D_0051E428;
            if (!func_003B00F0(i)) {
                continue;
            }
            p = func_003B0078(1, i);
            if (func_003AFFB8(p, gFighters[p].unk0) != i) {
                continue;
            }
            y = wpos.v[p].y;
            x = wpos.v[p].x;
            Ui_DrawWindow(x, y - 0x14, 0x118, 0xF0, 0xFFEE, 0x4040, 0x4040);
            col = Pad_GetPlayerMode(i) >= 0 ? 4 : 0x16;
            Text_Printf(D_0051E3B8, 10, 5, col, x + 8, y + 0x3C, func_003B1418(i));
            x2 = x + 0x10;
            y += 4;
            for (k = 0; k < 8; k++, y += 0x14) {
                id = D_0043A100[k];
                m = &D_00439CA0[i * 10 + id];
                if (!(D_0043A0A0[id].kind & kind)) {
                    continue;
                }
                col = 6;
                if (state == 0 && id == sel) {
                    col = 5;
                }
                if (D_0043B7F8[0] == 8) {
                    if (k == 5 && func_003F1F08()) {
                        name = D_0043A120[8];
                    } else {
                        name = D_0043A120[k];
                    }
                } else {
                    name = D_0043A068[(s8)*m];
                }
                n = strlen(name);
                Text_Printf(D_0051E438, D_0043A0A0[id].font, x2 + D_0043A0A0[id].dx + 0x46, y, D_0043A0A0[id].label,
                            x2 + 0x82 + off.v[n % 7], col, name);
                func_003B2C28(x2 + 0x82, y, k, id == sel);
                if (id == sel) {
                    func_003BB890(0, x2 + 0x70, y, 5);
                    func_003BB890(1, x2 + 0xE4, y, 5);
                }
            }
            y += 8;
            x2 += 0x1E;
            col = D_0043B7F8[0] == 8 ? 0x16 : func_003B2B30(i, 0);
            Text_Printf(D_0051E3C8, 10, 4, col, x2, y);
            if (col == 0xB) {
                func_003B2B58(x2, y);
            }
            y += 0x12;
            col = func_003B2B30(i, 1);
            if (col == 0xB) {
                func_003B2B58(x2, y);
                func_003BB890(1, x2 + 0xDD, y, 5);
            }
            if (flag) {
                v = 0;
                col = 0x16;
            } else {
                v = D_00439C9C[i];
            }
            Text_Printf(D_0051E3E0, 10, 4, col, x2, y, v ? 0xC : 0x10, D_0043A060[v]);
            y += 0x12;
            col = D_0043B7F8[0] == 8 ? 0x16 : func_003B2B30(i, 2);
            Text_Printf(D_0051E400, 10, 4, col, x2, y);
            if (col == 0xB) {
                func_003B2B58(x2, y);
            }
            y += 0x12;
            Text_Printf(D_0051E418, 10, 4, func_003B2B30(i, 3), x2, y);
        }
    }
    if (gMode == 9) {
        Text_Printf(D_0051E448, 0x96, 0x28, 10, 5, 0x13);
        if (!drawn) {
            Text_Printf(D_0051E468, 0x44, 0xB4, 10, 5, 0xB);
        }
        if (all & 0x900) {
            gSubMode = 1;
            func_003B2DC8();
            func_003BAA90();
        }
    } else if (all & 0x800) {
        func_003B2DC8();
        ret = 1;
    }
    if (gMode == 9) {
        func_003CF608(0);
    } else {
        func_003B1890();
    }
    return ret;
}
#else
INCLUDE_ASM("asm/nonmatchings/options", Options_ControllerSetup);
#endif

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E490);

typedef struct {
    const char *name;
    u8 id;
    u8 unk5;
    u8 unk6;
    u8 unk7;
} OptMenuItem;
extern const OptMenuItem D_0051EFE0[];
extern const char D_0051E490[];
extern const char D_0051E0E0[];
s32 func_003BFEB0(void);
typedef struct {
    u8 pad0[0x38];
    s32 unk38;
} OptUnlock;

void func_003B3F88(OptUnlock *s) {
    s32 i;
    s32 y = 0xFA;
    s32 col;
    s32 w;
    s32 ok = func_003BFEB0();

    for (i = 0; i < 9; i++) {
        const OptMenuItem *m = &D_0051EFE0[i];
        if (m->id == 8 && !ok) {
            continue;
        }
        col = 0x16;
        if ((s->unk38 >> i) & 1) {
            col = m->unk6;
        }
        w = Text_Printf(D_0051E490, 10, 4, m->name);
        Text_Printf(D_0051E0E0, 600 - w, y, 10, 4, col, m->name);
        y += 0x11;
    }
    D_0043FD10[8].v[0].y = 0x8020;
    D_0043FD10[8].v[1].y = 0x8020;
    D_0043FD10[8].v[2].y = y * 16 + 0x7100;
    D_0043FD10[8].v[3].y = y * 16 + 0x7100;
    func_003BB680(&D_0043FD10[8], 0xFFB00, 0);
}

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E4A0);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E4B8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E4D0);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E4E8);
INCLUDE_RODATA("asm/nonmatchings/options", D_0051E500);

extern const char D_0051E4A0[];
extern const char D_0051E4B8[];
extern const char D_0051E4D0[];
extern const char D_0051E4E8[];
extern const char D_0051E500[];
typedef struct {
    u8 *value;
    u8 cur;
    u8 pad5[3];
    const char *name;
    s8 type;
    u8 count;
    u8 padE[2];
    const char *str[11];
    s16 unk3C;
    s16 unk3E;
} OptEntry;
s32 func_003E08D0(s32 n);

/* NON_MATCHING: 62 lines, the pad deltas are if-converted to movn (original sltiu/negu + s16/s8 extend), u16 compares of the cursor, delay-slot fills in the item loop */
#if 0
void func_003B40C0(s32 pad, s32 trig, s32 hold) {
    OptEntry *it;
    s16 d;
    s8 dx;
    u8 n;
    s32 i, y, sel, c1, c2;

    d = (pad & 0x1000) ? -!((pad >> 14) & 1) : (pad >> 14) & 1;
    D_005F6D8E += d;
    if (d) {
        D_005F6DAC = -1;
        func_003B18E0(2);
    }
    if (D_005F6D8E < 0) {
        D_005F6D8E = 13;
    } else if (D_005F6D8E >= 14) {
        D_005F6D8E = 0;
    }
    it = &((OptEntry *)D_00439CE0)[D_005F6D8E];
    if (it->type == 0) {
        n = 0;
        for (i = 0; i < 10; i++) {
            if (it->str[i]) {
                n++;
            }
        }
    } else {
        n = it->count + 1;
    }
    if (it->type == 2) {
        if (trig & 0x8FC) {
            if (D_005F6D8E == 13) {
                gSubMode = 1;
                func_003B18E0(0);
            } else if (D_005F6D8E == 12) {
                if (D_005F6DAC < 0) {
                    D_005F6DAC = 0;
                    func_003B18E0(0);
                } else if (D_005F6DAC == 1) {
                    func_003B18E0(0);
                    D_005F6DAC = -1;
                    func_003B1820();
                } else if (D_005F6DAC == 0) {
                    func_003B18E0(1);
                    D_005F6DAC = -1;
                }
            }
        }
        if (D_005F6DAC >= 0) {
            if (trig & 0x8000) {
                D_005F6DAC = 1;
                func_003B18E0(2);
            } else if (trig & 0x2000) {
                D_005F6DAC = 0;
                func_003B18E0(2);
            }
        }
    } else if (!(hold & 0x5000)) {
        dx = (pad & 0x8000) ? -!((pad >> 13) & 1) : (pad >> 13) & 1;
        *it->value += dx;
        if (it->type == 0) {
            if ((s8)*it->value < 0) {
                *it->value = n - 1;
            } else if (*it->value >= n) {
                *it->value = 0;
            }
            if (dx) {
                func_003B18E0(2);
            }
        } else if (it->type == 1) {
            if ((s8)*it->value < 0) {
                *it->value = 0;
            } else if (*it->value >= n) {
                *it->value = n - 1;
            } else if (dx) {
                func_003B18E0(2);
            }
        }
    }
    Text_Printf(D_0051E4A0, 0xC2, 0x28, 10, 5, 0x13);
    if (func_003B1468()) {
        func_003DEB80(0);
        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
        if (it->value) {
            func_003E08D0(*it->value);
        }
        func_003E5268(0x1E, 0x186, D_005F6D8E + 6, 0x60606080);
    }
    y = 0x5A;
    for (i = 0; (u32)i < 14; i++, y += 0x14) {
        it = &((OptEntry *)D_00439CE0)[i];
        sel = 0;
        c1 = 5;
        c2 = 0xC;
        if (it->type != 2 && it->cur == *it->value) {
            c2 = 0x10;
        }
        if (i == D_005F6D8E) {
            c1 = 0xB;
            sel = 1;
            func_003B15F8((s32)it->name, y, 0);
            func_003B3F88((OptUnlock *)it);
        }
        if (it->type == 0) {
            Text_Printf(D_0051E4B8, 0x28, y, 10, 4, c1, it->name, c2, it->str[*it->value]);
            if (sel) {
                if (it->unk3C >= 0) {
                    func_003BB890(0, it->unk3C, y, 5);
                }
                if (it->unk3E >= 0) {
                    func_003BB890(1, it->unk3E, y, 5);
                }
            }
        } else if (it->type == 1) {
            if (*it->value == 0) {
                Text_Printf(D_0051E4D0, 0x28, y, 10, 4, c1, it->name, c2, *it->value);
            } else {
                Text_Printf(D_0051E4E8, 0x28, y, 10, 4, c1, it->name, c2, *it->value);
            }
            if (sel) {
                if (it->unk3C >= 0 && *it->value) {
                    func_003BB890(0, it->unk3C, y, 5);
                }
                if (it->unk3E >= 0 && *it->value < 10) {
                    func_003BB890(1, it->unk3E, y, 5);
                }
            }
        } else {
            if (i == 12 && c1 != 0xB) {
                c1 = 0x10;
            }
            Text_Printf(D_0051E0E0, 0x28, y, 10, 4, c1, it->name);
            if (i == 12 && D_005F6DAC >= 0) {
                Text_Printf(D_0051E500, 0xC8, y, 10, 4, D_005F6DAC == 1 ? 0xB : 0x16, D_005F6DAC == 0 ? 0xB : 0x16);
                if (D_005F6DAC) {
                    func_003BB890(0, it->unk3C, y, 5);
                } else {
                    func_003BB890(1, it->unk3E, y, 5);
                }
            }
        }
    }
    D_005F5C78 = (s8)D_00439C60[0x2F];
    D_005F5C76 = (s8)D_00439C60[0x2E];
    func_003B18F8(trig, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B40C0);
#endif

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E518);

typedef struct {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} OptRect;
typedef struct {
    OptRect r[6];
} OptRectTbl;
extern const OptRectTbl D_0051E518;
extern const char D_005F64B8[];
extern const char D_005F64C0[];

void func_003B4718(void) {
    OptQuad q;
    OptRectTbl t = D_0051E518;
    OptRect *r;
    s32 i;
    s32 x, y, w, h;

    for (i = 0; i < 6; i++) {
        r = &t.r[i];
        q = D_0043FD10[17];
        x = r->x * 16;
        y = r->y * 16;
        w = r->w * 16;
        h = r->h * 16;
        q.v[0].x += x;
        q.v[1].x += x + w;
        q.v[2].x += x;
        q.v[3].x += x + w;
        q.v[0].y += y;
        q.v[1].y += y;
        q.v[2].y += y + h;
        q.v[3].y += y + h;
        func_003BB680(&q, 0xFFFF0, 0);
    }
    Text_Printf(D_005F64B8, 0x90, 0xEC, 5);
    Text_Printf(D_005F64C0, 0x1E0, 0xEC, 5);
    Text_Printf(D_005F64A8, 0x138, 0x6C, 5);
    Text_Printf(D_005F64B0, 0x138, 0x16C, 5);
}

void func_003B4930(void) {
    D_005F6DA8 = 0;
}

INCLUDE_RODATA("asm/nonmatchings/options", D_0051E548);

/* NON_MATCHING: 58 lines, registers of the pad deltas (dx kept in a1 in the original) */
#if 0
void func_003B4938(s32 pad, s32 trig) {
    s32 dx, dy, v;
    u8 *blk;

    dx = (pad >> 13) & 1;
    if (pad & 0x8000) {
        dx = (dx - 1) * 4;
    } else {
        dx = dx * 4;
    }
    dy = (pad >> 14) & 1;
    if (pad & 0x1000) {
        dy = (dy - 1) * 2;
    } else {
        dy = dy * 2;
    }
    blk = D_00439C60;
    v = (s8)blk[0x34] + dx;
    if ((v < 0 ? -v : v) < 0x30) {
        blk[0x34] += dx;
        if (dx) {
            Snd_PlaySe(0x103F, 0);
        }
    }
    blk = D_00439C60;
    v = (s8)blk[0x35] + dy;
    if ((v < 0 ? -v : v) < 0x18) {
        blk[0x35] += dy;
        if (dy) {
            Snd_PlaySe(0x103F, 0);
        }
    }
    if ((trig & 0x10) && D_005F6DA8) {
        Snd_PlaySe(0x1040, 0);
        D_00439C60[0x35] = 0;
        D_00439C60[0x34] = 0;
    }
    D_005F6DA8 = 1;
    Gfx_SetDisplayOffset(((s8 *)D_00439C60)[0x34] + D_005F6DB0, ((s8 *)D_00439C60)[0x35] + D_005F6DB4);
    func_003B4718();
    func_003B15F8((s32)D_005F6320, 0x178, 0);
    Text_Printf(D_0051E548, 10, 4, 0x46, 0x178, 5, D_005F6320);
    Text_Printf(D_005F64C8, 0x28, 0x178, 8);
    func_003B15F8((s32)D_005F6478, 0x1A0, 0);
    Text_Printf(D_0051E548, 10, 4, 0x46, 0x1A0, 5, D_005F6478);
    Text_Printf(D_005F64D0, 0x28, 0x1A0, 8);
}
#else
INCLUDE_ASM("asm/nonmatchings/options", func_003B4938);
#endif

void func_003B4B18(void) {
    D_005F62AC = 0;
}


void Mode_Options(void) {
    s32 z = D_005F6730;
    s32 a = D_005F5CE0;
    s32 b = D_005F5CDC;
    s32 c = D_005F5CD8;
    s32 i;

    func_003B1860();
    D_005F6730 = 0;
    {
        s32 d = 4;
        if (gSubMode == 7) {
            d = -4;
        }
        D_005F62AC += d;
    }
    func_003CF608(0);
    func_003C4868();
    Sprite_SetBlend(1, 0);
    D_005F6738 = 0x80808080;
    func_003BF2F8(D_0043B7F8, &D_005F62AC);
    D_005F6730 = z;
    if (gSubMode != 6 || func_003616C8() != 0) {
        if (func_003B1378()) {
            gSubMode = 7;
        }
    }
    switch (gSubMode) {
    case 0:
        func_003B1980();
        D_005F6D8C = 0;
        gSubMode = 1;
        func_003992C8(0);
        break;
    case 1:
        func_003B19C0(a, b);
        break;
    case 2:
        func_003B40C0(a, b, c);
        break;
    case 3:
        if (Options_ControllerSetup()) {
            gSubMode = 1;
        }
        break;
    case 4:
        func_003B2978(a, b);
        break;
    case 5:
        func_003B1E78(a, b);
        break;
    case 6:
        if (func_00360F40(a, b) == 0) {
            gSubMode = 1;
        }
        break;
    case 7:
        if ((s16)D_005F62AC <= 0) {
            func_003638F0();
            for (i = 0; i < 4; i++) {
                Input_BuildMapping(i);
            }
            Options_Apply();
            func_003BD240();
            Mode_Change(3);
        }
        break;
    }
    func_003B1890();
}

void func_003B4D20(s32 a, s32 b, s32 c) {
    func_003B15F8(a, b, c);
}

void func_003B4D28(s32 i) {
    func_003B18E0(i);
}

s32 Rng_GetSeed(void) {
    return D_00439C7C[0];
}

void Rng_SetSeed(s32 seed) {
    D_00439C7C[0] = seed;
}

