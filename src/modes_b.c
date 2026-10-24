/* 0x3BB310-0x3BDF60 (module modes, docs/game_overview.md section 4; docs/systems/modes.md): the loading, title and
 * main menu modes with their helpers. .rodata 0x51EB00-0x51F150 is its own (subsegment, every string and table
 * included in place); the start is an object edge [V: its .rodata follows scene3d's], the end is [I]. */
#include "common.h"

#include "fighter.h"

typedef struct {
    const u8 *str;
    u16 w;
} MbTextW;

/* GS quad of four vertices (XYZ2 then RGBAQ words) with an alpha limit in front. */
typedef union {
    u64 d;
    struct {
        s32 lo;
        s32 hi;
    } w;
    u8 b[8];
} GsWord;

typedef struct {
    /* 0x00 */ u16 alpha;
    /* 0x02 */ u16 pad2[3];
    /* 0x08 */ struct {
        GsWord xyz;
        GsWord rgba;
    } v[4];
} MbQuad;

extern s16 gMode;
extern s16 gSubMode;
extern u16 gFrameCount;
extern u8 D_0043B7F8[];

extern s16 D_005F6E30;
extern s16 D_005F6E32;
extern s16 D_005F6E38[4];
extern s32 D_005F6730;
extern u64 D_005F6738;
extern s32 D_005F6528;
extern const char D_005F6518[];
extern const char D_005F6520[];

extern const char D_0051EB00[];
extern const char D_0051EB20[];

s32 Text_Printf(const char *fmt, ...);
void func_003992C8(s32 value);
void func_003A97B0(void);
s32 func_003A9AB8(void);
void Mode_Change(s32 mode);
void Snd_PlaySe(s32 id, s32 player);
void func_003A67D0(s32 n, u8 r, u8 g, u8 b);
s32 func_0039C778(void);
u64 *func_00399520(void);
void func_00399650(u64 *p);
s32 func_003C71F0(s32 id);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_SetBlend(s32 a, s32 b);
void Sprite_Draw(s32 a, s32 b, s32 c, s32 x, s32 y);
s32 Pad_IsMultitap(void);
s32 func_003B0818(void);
s32 func_003B00F0(s32 port);

void Mode_Loading(void);
void Mode_EnterLoading(s16 next);
s32 func_003BB4D8(void);
void func_003BB4E8(MbQuad *q, u16 *alpha, s32 unused);
void func_003BB680(MbQuad *q, s32 z, s32 unused);
void func_003BB7F0(s32 kind, s32 x, s32 y, s32 id);
void func_003BB890(s32 which, s32 x, s32 y, s32 c);
void func_003BB8B0(s32 v);
s32 func_003BB8B8(s32 d, s32 lim);
void func_003BB8E8(s32 i);
void func_003BB900(s32 x);
s32 func_003BB938(void);
void func_003BDF08(void);

typedef struct {
    const char *name;
    s32 w;
    s32 color;
} MbTitle;
typedef struct {
    MbTitle e[9];
} MbTitleTbl;
typedef struct {
    const char *name;
    u16 color;
    u16 w;
} MbName;
typedef struct {
    MbName e[4];
} MbNameTbl;
typedef struct {
    const char *str;
    u16 x[2];
} MbDigits;

extern u8 D_005F5CBE;
extern s32 D_005F5CC0;
extern s32 D_005F6530;
extern s32 D_005F6534;
extern u16 D_005F5CB0[4];
extern const MbTitleTbl D_0051EC18;
extern const char D_0051EC88[];
extern const MbNameTbl D_0051ECC8;
extern const MbDigits D_0051ECE8[];
extern const MbDigits D_0051ECF0[];
extern const MbDigits D_0051ECF8[];
extern const char D_0051ED00[];
extern const char D_0051ED10[];
extern const char D_0051ED28[];
extern const char D_0051ED40[];

void Time_FramesToClock(s32 t, s32 *a, s32 *b, s32 *c);
s32 func_003B0458(void);
s32 Pad_CountReady(void);

void func_003BBC08(void);
void func_003BBC40(void);
void func_003BBD88(s32 right);
void func_003BBE78(void);
void func_003BBF00(s32 stage, s32 t, s32 x, s32 y);
s32 func_003BBFD0(s32 i);

extern u16 D_005F5CB4;
s32 func_0035A578(void);
s32 func_0035A1E8(u8 *o);

void func_003BC048(u8 *o, s32 i, s32 st, s32 wait);
s32 func_003BC370(u8 *o, s32 i);
s32 func_003BC508(void);
void func_003BC540(void);
void func_003BC600(void);
void func_003BC650(s32 side, s32 a, s32 b, s32 x, s32 y, s32 c, s32 d);

typedef struct {
    const char *name;
    u8 id;
    u8 unk5;
    u8 unk6;
    u8 unk7;
} MbMenuItem;

extern s16 D_005F65A0;
extern s32 D_005F65DC;
extern s8 D_005F650E;
extern const MbMenuItem D_0051EFE0[];
extern const MbTextW D_0051EE68[];

s32 Pad_GetPlayerMode(s32 player);
void func_003B9078(s32 a);

void func_003BC828(s32 side, s32 x, s32 y, s32 c);
void func_003BC868(s32 side, s32 idx, const MbTextW *tbl, s32 x, s32 y, s32 c);
void func_003BC998(void);
void func_003BCA30(s32 side, s32 x, s32 y, s32 c);
s32 func_003BCBA0(void);
void func_003BD130(s16 *ids, s16 *n);
void func_003BD240(void);
s32 func_003BDEB8(void);
s32 func_003BDEE0(void);
s32 func_003BDF38(void);


extern const MbTextW D_0051EEE0[];
extern const MbTextW D_0051EEA8[];
extern const u8 D_0051EF10[];
extern const char D_0051EF20[];
extern const u8 D_0051EF30[];
extern const char D_0051EF40[];
extern const char D_0051EF70[];

u8 *Str_Copy(u8 *dst, const u8 *src);
s32 func_003B0078(s32 a, s32 port);
void func_003C4868(void);
void func_003C4930(void);
void func_003C4A28(s32 kind);
void func_003BCBC8(u32 state);

extern s32 D_00415728[];
void func_003B0298(s32 a);
void func_003B01D0(s32 a, s32 port, s32 b);
void func_003B00A8(s32 a, s32 port, s32 side);
void func_003B00D8(s32 port, s32 a);
void func_003AFFE0(s32 a, s32 port);
s32 func_003BFEB0(void);
s32 func_003BFED8(void);
s32 func_003BFEE8(void);
s16 func_003BCFB8(s32 id);

extern Fighter gFighters[4];
extern u16 D_005F5C74;
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern const char D_005F6538[];
extern const char D_005F6540[];
extern const char D_0051EB48[];
s32 func_003B6220(void);
s32 func_003B0340(void);
s32 func_003A9698(void);
s32 func_003B01B0(s32 port);
s32 func_003AFFB8(s32 i, s32 a);
void func_003BB9C0(void);

extern const s16 D_0051ED60[];
extern const s16 D_0051ED68[];
extern const char D_0051ED70[];
extern const char D_0051ED88[];
extern const char D_0051EDA0[];
extern const char D_0051EDB8[];
extern const char D_0051EDC8[];
extern const char D_0051EDE0[];
extern const char D_0051EDF8[];
extern s32 D_005F5F08;
extern s32 D_005F5CC4;
extern const char *D_00440658[];
s32 func_0035A588(void);
s32 func_0035A558(void);
s32 func_0035A520(void);

typedef struct {
    s32 unk0;
    u8 unk4;
    u8 unk5;
    u8 pad6[2];
} MbTitleState;

extern MbTitleState D_005F6508;
extern s32 D_005F6510;
extern s32 D_005F65A4;
extern s32 D_005F65A8;
s32 func_003B1118(void);
void func_003BAA90(void);
void Bgm_Stop(void);
void func_003C9358(s32 a);
void func_0039C7D8(s32 a);
void func_003B8A50(void);
s32 func_0039C868(void);
void func_003A6898(s32 r, s32 g, s32 b);
void func_003A6770(s32 n, u8 r, u8 g, u8 b);
void func_003B88B8(void);
void func_003A6BD0(void);
void Game_Start(s32 a, s32 b);
void Mode_Title(void);

typedef struct {
    u16 a;
    u16 b;
} MbFade;

extern s32 D_005F5CD8;
extern s32 D_005F5CDC;
extern s32 D_005F6E48;
extern MbFade D_005F6E40;
extern u16 D_005F6E44;
extern s16 D_005F6E46;
extern s16 D_005F65D8;
extern u8 D_005F6E50;
extern u8 D_005F6E51;
extern u8 D_005F6E52;
extern MbQuad D_0043FEC0[];
extern MbQuad D_0043FE30[];
extern const char D_0051F0B0[];
extern const char D_0051F0C0[];
void func_00374520(void);
void func_00374548(void);
s32 func_003BFEA0(s32 bit);
void func_003B0358(void);
s16 func_003B0348(void);
void func_003B0380(s32 side);
void func_003B0378(s32 a);
void func_003BFE78(s32 a);
void Unlock_ApplyAll(void);
void func_003B4B18(void);
void Gallery_Enter(void);
void func_003B8900(void);
void func_003DBAA8(u32 x0, u32 x1, u32 y0, u32 y1);
void func_003CCF40(s32 a, s32 b, s32 c, s32 d);
s32 func_003BFF40(void);
void Mode_MainMenu(void);

/* Index of the first visible menu entry with the given id (written out three times by the original). */
#define MENU_FIND(i_, n_, ids_, id_)                                                                                   \
    do {                                                                                                               \
        for ((i_) = 0; (i_) < (n_); (i_)++) {                                                                          \
            if (D_0051EFE0[(ids_)[i_]].id == (id_)) {                                                                  \
                D_005F650E = (i_);                                                                                     \
                break;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)


INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EB00);
void Mode_Loading(void) {
    s32 r;

    switch (gSubMode) {
    case 0:
        if (D_005F6E32 == 13) {
            func_003992C8(0);
            gSubMode = 1;
            D_005F6E30 = 3;
            break;
        }
        goto start;
    case 1:
        Text_Printf(D_0051EB00, 10, 5, 0x195, 0x1A0, 0);
        if (--D_005F6E30 < 0) {
            func_003992C8(1);
        start:
            func_003A97B0();
            gSubMode = 2;
        }
        break;
    case 2:
        r = func_003A9AB8();
        if (r == 1) {
            gSubMode = 3;
            D_005F6E30 = 4;
            func_003992C8(1);
        } else if (r == -1) {
            func_003992C8(1);
            if (D_005F6E32 == 13) {
                gMode = D_005F6E32;
            } else {
                Mode_Change(3);
            }
            Snd_PlaySe(4, 0);
        }
        break;
    case 3:
        if (D_005F6E32 == 13) {
            func_003992C8(1);
            gMode = D_005F6E32;
        } else if (--D_005F6E30 <= 0) {
            gSubMode = 3;
            func_003A67D0(0x2B, 0xFF, 0xFF, 0xFF);
            func_003BDF08();
        }
        break;
    }
}
void Mode_EnterLoading(s16 next) {
    D_005F6E32 = next;
    gMode = 11;
    gSubMode = 0;
}
s32 func_003BB4D8(void) {
    return gMode == 11;
}
void func_003BB4E8(MbQuad *q, u16 *alpha, s32 unused) {
    s32 z = D_005F6730 >> 4;
    u64 *p;

    if (func_0039C778() == 0) {
        if (q->alpha < *alpha) {
            *alpha = q->alpha;
        }
        q->v[0].rgba.b[3] = *alpha;
        q->v[1].rgba.b[3] = *alpha;
        q->v[2].rgba.b[3] = *alpha;
        q->v[3].rgba.b[3] = *alpha;
        q->v[0].xyz.w.hi = z;
        q->v[1].xyz.w.hi = z;
        q->v[2].xyz.w.hi = z;
        q->v[3].xyz.w.hi = z;
        p = func_00399520();
        *p++ = 0x91A6400000008001;
        *p++ = 0xEEEEEEEEE;
        *p++ = 0x44;
        *p++ = 0x43;
        *p++ = q->v[0].rgba.d;
        *p++ = 1;
        *p++ = q->v[0].xyz.d;
        *p++ = 5;
        *p++ = q->v[1].rgba.d;
        *p++ = 1;
        *p++ = q->v[1].xyz.d;
        *p++ = 5;
        *p++ = q->v[2].rgba.d;
        *p++ = 1;
        *p++ = q->v[2].xyz.d;
        *p++ = 5;
        *p++ = q->v[3].rgba.d;
        *p++ = 1;
        *p++ = q->v[3].xyz.d;
        *p++ = 5;
        func_00399650(p);
    }
}
void func_003BB680(MbQuad *q, s32 z, s32 unused) {
    u64 *p;

    if (func_0039C778() == 0) {
        z >>= 4;
        D_005F6738 = 0x80808080;
        q->v[0].xyz.w.hi = z;
        q->v[1].xyz.w.hi = z;
        q->v[2].xyz.w.hi = z;
        q->v[3].xyz.w.hi = z;
        p = func_00399520();
        if (p != NULL) {
            *p++ = 0x91A6400000008001;
            *p++ = 0xEEEEEEEEE;
            *p++ = 0x44;
            *p++ = 0x43;
            *p++ = q->v[0].rgba.d;
            *p++ = 1;
            *p++ = q->v[0].xyz.d;
            *p++ = 5;
            *p++ = q->v[1].rgba.d;
            *p++ = 1;
            *p++ = q->v[1].xyz.d;
            *p++ = 5;
            *p++ = q->v[2].rgba.d;
            *p++ = 1;
            *p++ = q->v[2].xyz.d;
            *p++ = 5;
            *p++ = q->v[3].rgba.d;
            *p++ = 1;
            *p++ = q->v[3].xyz.d;
            *p++ = 5;
            func_00399650(p);
        }
    }
}
void func_003BB7F0(s32 kind, s32 x, s32 y, s32 id) {
    s32 save;
    s32 tex;

    y -= 0x14;
    tex = func_003C71F0(id);
    save = D_005F6730;
    D_005F6730 = 0xFFFF0;
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    Sprite_Draw(1, kind != 0 ? 0x82 : 0x81, tex, x, y);
    D_005F6730 = save;
}
/* A selection arrow ("%H%V%c}" or "%H%V%c{") at x, y in colour c; every caller passes all four. */
void func_003BB890(s32 which, s32 x, s32 y, s32 c) {
    if (which != 0) {
        Text_Printf(D_005F6518, x, y, c);
    } else {
        Text_Printf(D_005F6520, x, y, c);
    }
}
void func_003BB8B0(s32 v) {
    D_005F6528 = v;
}
s32 func_003BB8B8(s32 d, s32 lim) {
    s32 v;

    v = D_005F6528;
    v += d;
    D_005F6528 = v;
    if (d > 0) {
        if (lim < v) {
            D_005F6528 = v = lim;
        }
    } else {
        if (v < lim) {
            D_005F6528 = v = lim;
        }
    }
    return v;
}
void func_003BB8E8(s32 i) {
    D_005F6E38[i] = 300;
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EB20);
void func_003BB900(s32 x) {
    if (gFrameCount & 0x10) {
        Text_Printf(D_0051EB20, 0x33, x, 10, 5, 11);
    }
}
s32 func_003BB938(void) {
    s32 found = 0;
    s32 i;

    if (Pad_IsMultitap() == 0) {
        if (func_003B0818() != 0 || func_003B00F0(2) != 0 || func_003B00F0(3) != 0) {
            for (i = 0; i < 4; i++) {
                if (func_003B00F0(i) != 0) {
                    found = 1;
                    break;
                }
            }
        }
    }
    return found;
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EB48);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EB60);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EB70);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EB88);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EBA0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EBB8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EBC8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EBE0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EBF8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EC08);
void func_003BB9C0(void) {
    Fighter *f = gFighters;
    s32 i;
    s32 x;
    s32 port;
    s32 col;
    s32 ch;
    s32 mark;
    const char *s;
    s16 *t;

    if (func_003BB938() != 0) {
        func_003BB900(0xB4);
        return;
    }
    if (func_003B6220() != 0) {
        return;
    }
    if (func_003B0340() == 0 || D_005F5C74 == 0) {
        return;
    }
    if (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
        return;
    }
    if (Pad_IsMultitap()) {
        ch = 'A';
        s = D_005F6538;
        mark = '-';
    } else {
        ch = '1';
        s = D_005F6540;
        mark = 'P';
    }
    for (i = 0, x = 30; i < 2; i++, f++, x = 540) {
        if (D_005F5CB0[i] != 0 && func_003B01B0(i) != 0) {
            t = &D_005F6E38[i];
            if (*t >= 0) {
                if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                    (*t)--;
                }
                port = func_003AFFB8(i, f->unk0);
                col = 22;
                if (Pad_GetPlayerMode(port) >= 0) {
                    col = 11;
                }
                if ((Pad_IsMultitap() || port < 2) && (D_005F6E38[i] & 0x10) && D_005F6E38[i] > 16) {
                    Text_Printf(D_0051EB48, x, 120, 10, 6, col, s, mark, port + ch);
                }
            }
        }
    }
}
void func_003BBC08(void) {
    s32 n = D_005F5CC0;

    if (n != 0) {
        D_005F6530 = D_005F5CBE;
    } else {
        D_005F6530 = 0;
    }
    if (n >= 1000) {
        D_005F6534 = 999;
    } else {
        D_005F6534 = n;
    }
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EC18);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EC88);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EC98);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ECA8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ECB8);
void func_003BBC40(void) {
    MbTitleTbl t = D_0051EC18;

    Text_Printf(D_0051EC88, (0x280 - t.e[D_0043B7F8[0]].w) / 2, 40, 10, 5, t.e[D_0043B7F8[0]].color,
                t.e[D_0043B7F8[0]].name);
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ECC8);
void func_003BBD88(s32 right) {
    MbNameTbl t = D_0051ECC8;
    s32 i;
    s32 x;

    switch (D_0043B7F8[0]) {
    case 7:
        i = 0;
        break;
    case 4:
        i = 1;
        break;
    case 3:
        i = 2;
        break;
    default:
        return;
    }
    if (right == 0) {
        x = 40;
    } else {
        x = 600 - t.e[i].w;
    }
    Text_Printf(D_0051EC88, x, 0x19A, 10, 6, t.e[i].color, t.e[i].name);
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ECE8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ECF0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ECF8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ED00);
void func_003BBE78(void) {
    s32 n = D_005F5CC0;
    const MbDigits *d;
    s32 k;

    if (n >= 1000) {
        n = 999;
    }
    if (n > 0) {
        k = D_005F5CBE != 0;
        if (n < 10) {
            d = D_0051ECE8;
        } else if (n < 100) {
            d = D_0051ECF0;
        } else {
            d = D_0051ECF8;
        }
        Text_Printf(D_0051ED00, 0xFFFD0, d->x[k], 0x1B0, 10, 5, d->str, n);
    }
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ED10);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ED28);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ED40);
void func_003BBF00(s32 stage, s32 t, s32 x, s32 y) {
    s32 m;
    s32 s;
    s32 c;

    Time_FramesToClock(t, &m, &s, &c);
    if (stage < 8 || (gFrameCount & 0x20)) {
        Text_Printf(D_0051ED10, 11, 0, x, y, stage);
    } else {
        Text_Printf(D_0051ED28, 11, 0, x, y);
    }
    Text_Printf(D_0051ED40, 11, 5, x + 0x82, y, m, s, c);
}
s32 func_003BBFD0(s32 i) {
    s32 a = func_003B0458();
    s32 n = Pad_CountReady();
    s32 need = Pad_IsMultitap() ? 4 : 2;

    if (D_005F5CB0[i] != 0 || (need == n && a == need)) {
        return 0;
    }
    return 1;
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ED60);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ED68);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ED70);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051ED88);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EDA0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EDB8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EDC8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EDE0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EDF8);
void func_003BC048(u8 *o, s32 i, s32 st, s32 wait) {
    s32 x;
    s32 y;
    s32 n;
    s32 m;
    s32 k;

    x = i * 320 + (i == 0 ? 0x90 : 0xB0);
    y = D_0051ED60[i];
    if (o[0] == 9) {
        Text_Printf(D_0051ED70, 11, 2, 0xF5, 0x104);
    }
    switch (st) {
    case 1:
    case 2:
        if (D_005F5F08 != 0) {
            break;
        }
        if (func_0035A588() != 0) {
            break;
        }
        if (func_0035A558() != 0) {
            break;
        }
        if (wait != 0 || func_0035A578() != 0) {
            func_003BC828(i, D_0051ED68[i], 0x18, 3);
        }
        break;
    case 3:
        Text_Printf(D_0051ED88, 11, 2, x - 0x48, 0x18);
        break;
    case 4:
        m = *(s32 *)(o + 0x2C) + *(s32 *)(o + 0x34);
        func_003BBF00(*(s32 *)(o + 0x24) + func_0035A520(), m, y, 0x18);
        break;
    case 6:
        n = D_005F5CC0;
        k = D_005F5CC4;
        if (n >= 1000) {
            n = 999;
        }
        Text_Printf(D_0051EDA0, 11, 5, x - 0x10, 0x18, n);
        Text_Printf(D_0051EDB8, 11, 6, x - 0x70, 0x18, D_00440658[k]);
        break;
    case 7:
        Text_Printf(D_0051EDC8, 11, 3, x - 0x40, 0x18);
        break;
    case 5:
        Text_Printf(D_0051EDE0, 11, 6, x - 0x58, 0x18);
        break;
    case 8:
        Text_Printf(D_0051EDF8, 11, 5, x - 0x58, 0x18);
        break;
    case 0:
        break;
    }
}
/* NON_MATCHING: block layout of the case-1 tests and the hoisted constants 1 / 3 (a3 / a2) differ */
#if 0
/* NON_MATCHING: block layout of the case-1 tests and the hoisted constants 1 / 3 (a3 / a2) differ */
s32 func_003BC370(u8 *o, s32 i) {
    s32 r = 0;

    switch (o[0x1A]) {
    case 0:
        break;
    case 1:
        if (i == o[0x1C]) {
            if (D_005F5CB0[i] != 0) {
                if (func_0035A578() == 0) {
                    r = 4;
                    if (D_0043B7F8[0] == 8) {
                        r = 0;
                    }
                }
            } else {
                r = 2;
            }
            if (o[0] == 2) {
                r = 8;
            }
            if (o[0] == 4) {
                r = 6;
            }
            break;
        }
        if (o[0x10] == 0 && (D_005F5CB4 == 0 || o[0x11] != 0)) {
            r = 2;
            if (o[0x11] != 0) {
                r = 0;
            }
        } else if (D_0043B7F8[0] != 2 || func_0035A1E8(o) != 0) {
            r = 0;
            if (func_0035A578() == 0) {
                r = 3;
            }
        } else {
            r = 2;
        }
    tail:
        if (o[0] == 4) {
            r = 7;
        } else if (o[0] == 3) {
            r = 5;
        }
        break;
    case 2:
        if (o[0x1F] == 0 || o[0x1F] == 3) {
            break;
        }
        if (D_0043B7F8[0] == 1 || D_0043B7F8[0] == 6 || D_0043B7F8[0] == 2) {
            break;
        }
        if (o[0x21] != i) {
            break;
        }
        r = 3;
        if ((gFrameCount & 0x18) == 0) {
            r = 0;
        }
        break;
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/modes_b", func_003BC370);
#endif
s32 func_003BC508(void) {
    if (gMode != 5 || D_0043B7F8[8] == 0) {
        return -1;
    }
    return D_0043B7F8[0] == 2;
}
void func_003BC540(void) {
    u8 *o = D_0043B7F8;
    s32 i;
    s32 st;

    for (i = 0; i < 2; i++) {
        st = 0;
        switch (o[0]) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 8:
            st = func_003BC370(o, i);
            break;
        case 7:
            st = 0;
            break;
        case 9:
            st = 2;
            break;
        }
        func_003BC048(o, i, st, func_003BBFD0(i));
    }
}
void func_003BC600(void) {
    func_003BC650(0, 0, 0, 0xD, 0x1C2, 5, 0);
    func_003BC650(1, 0, 0, 0x1CE, 0x1C2, 5, 0);
}
void func_003BC650(s32 side, s32 a, s32 b, s32 x, s32 y, s32 c, s32 right) {
    u8 buf[256];
    s32 n = 0;
    s32 i;
    const MbTextW *e;
    s32 mode;

    if (func_0035A578() == 0) {
        for (i = 0; i < 4; i++) {
            if (Pad_GetPlayerMode(i) >= 0) {
                if (func_003B00F0(i) == 0) {
                    n = 1;
                }
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            if (Pad_GetPlayerMode(i) >= 0) {
                if (func_003B00F0(i) != 0) {
                    n = 1;
                }
            }
        }
    }
    e = &D_0051EEE0[n];
    mode = func_003BC508();
    if (mode < 0) {
        Str_Copy(Str_Copy(buf, D_0051EF10), e->str);
        if (side != 0) {
            x += 16;
        } else {
            x -= 16;
        }
        if (right) {
            x -= e->w;
        }
        Text_Printf((char *)buf, x, y, c, 0);
    } else {
        y = 400;
        x = 160 - Text_Printf(D_0051EF20, 10, 4, e->str) / 2;
        if (side != 0) {
            x += 320;
        }
        Str_Copy(Str_Copy(buf, D_0051EF30), e->str);
        Text_Printf((char *)buf, x, mode != 1 ? y : 0x12A, 10, 4, 0x11);
    }
}
void func_003BC828(s32 side, s32 x, s32 y, s32 c) {
    u16 fc = gFrameCount;

    if (fc & 0x30) {
        if (fc & 0x40) {
            func_003BC650(side, 0, 0, x, y, c, 1);
        } else {
            func_003BCA30(side, x, y, c);
        }
    }
}
void func_003BC868(s32 side, s32 idx, const MbTextW *tbl, s32 x, s32 y, s32 c) {
    u8 buf[100];
    const MbTextW *e = &tbl[idx];
    s32 mode = func_003BC508();

    if (mode < 0) {
        Str_Copy(Str_Copy(buf, D_0051EF10), e->str);
        if (side != 0) {
            x += 16;
        } else {
            x -= 16;
        }
        Text_Printf((char *)buf, x - e->w, y, c);
    } else {
        y = 400;
        x = 160 - Text_Printf(D_0051EF20, 10, 4, e->str) / 2;
        if (side != 0) {
            x += 320;
        }
        Str_Copy(Str_Copy(buf, D_0051EF30), e->str);
        Text_Printf((char *)buf, x, mode != 1 ? y : 0x12A, 10, 4, 0x11, 0);
    }
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EE68);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EE78);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EE98);
void func_003BC998(void) {
    s32 i;
    s32 found;

    if (gFrameCount & 0x30) {
        found = 0;
        for (i = 0; i < 4; i++) {
            if (Pad_GetPlayerMode(i) >= 0) {
                found = 1;
                break;
            }
        }
        func_003BC868(-1, found, D_0051EE68, 0x140, 0x149, 5);
    }
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EEA8);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EEC0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EED0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EEE0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EEF0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EF00);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EF10);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EF20);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EF30);
/* NON_MATCHING: the constant 1 is not hoisted into s3 and the saved registers of x / y / c are rotated */
#if 0
/* NON_MATCHING: the constant 1 is not hoisted into s3 and the saved registers of x / y / c are rotated */
void func_003BCA30(s32 side, s32 x, s32 y, s32 c) {
    s16 i;
    s32 n;

    if (gFrameCount & 0x30) {
        n = 0;
        i = 0;
        if (func_0035A578() == 0) {
            for (; i < 4; i++) {
                if (Pad_GetPlayerMode(i) >= 0 && func_003B00F0(i) == 0) {
                    n = side == 0 ? 1 : 2;
                }
            }
        } else {
            for (; i < 4; i++) {
                if (Pad_GetPlayerMode(i) >= 0 && func_003B00F0(i) != 0 && func_003B0078(1, i) == side) {
                    n = side == 0 ? 1 : 2;
                }
            }
        }
        func_003BC868(side, n, D_0051EEA8, x, y, c);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/modes_b", func_003BCA30);
#endif
s32 func_003BCBA0(void) {
    s32 r = 0;

    if (D_005F65A0 != 0) {
        r = 1;
        if (D_005F65DC != 0) {
            r = 2;
        }
    }
    return r;
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EF40);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EF70);
void func_003BCBC8(u32 state) {
    switch (state) {
    case 2:
        func_003BC998();
    case 3:
        func_003C4868();
        func_003C4930();
    case 4:
        Text_Printf(D_0051EF40, 0x24, 0x199, 11);
        Text_Printf(D_0051EF70, 0x123, 0x1AD, 11);
    case 5:
        func_003C4868();
        if (gMode == 3 && gSubMode > 0) {
            func_003C4A28(func_003BCBA0());
        }
        break;
    case 0:
    case 1:
    case 6:
    case 7:
        break;
    }
}
void Mode_Title(void) {
    MbTitleState *t = &D_005F6508;
    s16 sub = gSubMode;

    if (func_003B1118() != 0 || D_005F65A4 != 0) {
        if (D_005F65A4 != 0) {
            Mode_Change(3);
        }
        D_005F65A4 = 0;
        func_003BAA90();
        if (sub == 7 || sub == 8) {
            func_003BCBC8(5);
        }
        return;
    }
    switch (gSubMode) {
    case 0:
        Bgm_Stop();
        t->unk4 = t->unk5++;
        if (t->unk5 >= 3) {
            t->unk5 = 0;
        }
        if (t->unk4 != 0) {
            Bgm_Stop();
            gSubMode = 1;
            if (t->unk4 == 1) {
                func_003C9358(0);
            }
            if (t->unk4 == 2) {
                func_003C9358(2);
            }
        } else {
            func_003C9358(1);
            Mode_EnterLoading(1);
        }
        D_005F6510 = 0;
        D_005F65A8 = 6;
        break;
    case 1:
        D_005F65A8 = 1;
        if (t->unk4 == 2) {
            func_0039C7D8(0);
        } else {
            func_0039C7D8(1);
        }
        gSubMode = 2;
        break;
    case 2:
        D_005F65A8 = 1;
        func_003B8A50();
        if (func_0039C868() != 0) {
            D_005F65A4 = 1;
            break;
        }
        func_003A6898(0xFF, 0xFF, 0xFF);
        gSubMode = 6;
        break;
    case 3:
        func_003A6770(0x19, 0xFF, 0xFF, 0xFF);
        D_005F65A8 = 0;
        gSubMode = 4;
        D_005F6510 = 0;
    case 4:
        D_005F6510 += 10;
        if (D_005F6510 < 0x100) {
            break;
        }
        D_005F6510 = 0;
        gSubMode = 5;
    case 5:
        func_003A6898(0xFF, 0xFF, 0xFF);
        if (++D_005F6510 < 11) {
            break;
        }
        gSubMode = 6;
        break;
    case 6:
        Bgm_Stop();
        func_003B88B8();
        if (t->unk4 != 0) {
            func_003B9078(1);
        }
        func_003A67D0(0x2B, 0xFF, 0xFF, 0xFF);
        D_005F65A8 = 5;
        D_005F6510 = 0x100;
        gSubMode = 7;
    case 7:
        if (D_005F6510 > 0) {
            D_005F6510 -= 6;
            break;
        }
        func_003A6BD0();
        D_005F6510 = 300;
        gSubMode = 8;
        break;
    case 8:
        D_005F65A8 = 2;
        if (--D_005F6510 != 0) {
            break;
        }
        D_005F6510 = 0x100;
        func_003A6770(0x21, 0, 0, 0);
        gSubMode = 9;
        break;
    case 9:
        if (D_005F6510 > 0) {
            D_005F6510 -= 8;
            break;
        }
        D_005F65A8 = 6;
        gSubMode = 10;
        break;
    case 10:
    default:
        D_005F65A8 = 6;
        Game_Start(9, 0);
        break;
    }
    func_003BCBC8(D_005F65A8);
}
s16 func_003BCFB8(s32 id) {
    s32 r;
    s32 i;
    s32 j;

    func_003B0298(0);
    func_003B0298(1);
    for (j = 0; j < 4; j++) {
        func_003B01D0(0, j, 0);
    }
    r = 0;
    for (i = 0; i < 4; i++) {
        if (D_00415728[i] & 0x8FC) {
            if (Pad_CountReady() == 2) {
                for (j = 0; j < 4; j++) {
                    if (Pad_GetPlayerMode(j) >= 0) {
                        break;
                    }
                }
                j = j != i;
            } else if (Pad_IsMultitap()) {
                j = (i & 2) >> 1;
            } else {
                j = i & 1;
            }
            func_003B00A8(0, i, j);
            func_003B00A8(1, i, j);
            func_003B00D8(i, 1);
            func_003AFFE0(1, i);
            D_005F5CB0[j] = 1;
            r = 1 << j;
            break;
        }
    }
    return r;
}
RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051EFE0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F040);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F050);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F060);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F070);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F080);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F090);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F0A0);
void func_003BD130(s16 *ids, s16 *n) {
    u32 i;
    s32 ok;

    *n = 0;
    for (i = 0; i < 12; i++) {
        ok = 1;
        if (D_0051EFE0[i].unk5 != 0) {
            ok = 0;
            if (D_0051EFE0[i].id == 8) {
                ok = func_003BFEB0();
            } else if (D_0051EFE0[i].id == 9) {
                ok = func_003BFED8();
            } else if (D_0051EFE0[i].id == 10) {
                ok = func_003BFEE8();
            }
        }
        if (ok) {
            *ids++ = i;
            (*n)++;
        }
    }
}
void func_003BD240(void) {
    s16 ids[16];
    s16 n;
    s32 i;

    func_003BD130(ids, &n);
    for (i = 0; i < n; i++) {
        if (D_0051EFE0[ids[i]].id == 11) {
            *(volatile s8 *)&D_005F650E = i; /* fake: keeps the store out of the exit's delay slot */
            break;
        }
    }
}
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F0B0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", D_0051F0C0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", jtbl_0051F0D0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", jtbl_0051F0F0);
INCLUDE_RODATA("asm/nonmatchings/modes_b", jtbl_0051F120);
/* NON_MATCHING: register allocation (fp for the menu table's %hi, s5 / s6), the pad direction branch, delay slots */
#if 0
/* NON_MATCHING: register allocation (fp for the menu table's %hi, s5 / s6), the pad direction branch, delay slots */
void Mode_MainMenu(void) {
    s16 ids[16];
    s16 n;
    s32 pad;
    s32 trig;
    s8 k;
    s32 cnt;
    s32 dk;
    s16 j;
    s16 old;
    s16 kind;
    s32 side;
    s32 stay;
    s32 p;
    s32 blink;
    s32 y;
    s8 i;
    const MbMenuItem *e;
    s32 col;

    pad = D_005F5CD8;
    trig = D_005F5CDC;
    func_003BD130(ids, &n);
    if (D_005F6E48 < 19) {
        D_005F6E48++;
    }
    switch (gSubMode) {
    case 0:
        Bgm_Stop();
        D_005F6510 = 0;
        gSubMode++;
        func_003BAA90();
        D_005F65D8 = 0;
        func_00374520();
        func_003B88B8();
        func_003992C8(0);
        D_005F6E50 = 0;
        D_005F6E51 = 0;
        D_005F6E52 = 0;
        if (func_003BFEB0() != 0 && func_003BFEA0(4) == 0) {
            D_005F6E50 = 1;
            MENU_FIND(i, n, ids, 8);
        }
        if (func_003BFED8() != 0 && func_003BFEA0(2) == 0) {
            D_005F6E51 = 1;
            MENU_FIND(i, n, ids, 9);
        }
        if (func_003BFEE8() != 0 && func_003BFEA0(1) == 0) {
            D_005F6E52 = 1;
            MENU_FIND(i, n, ids, 10);
        }
    case 1:
        if (++D_005F6510 > 480) {
            if (D_005F6510 < 482) {
                func_003992C8(1);
            } else {
                gSubMode++;
            }
            break;
        }
        if (pad & 0xFFFF) {
            D_005F6510 = 0;
        }
        if (D_005F65D8 == 0) {
            i = (pad & 0x1000) ? -!(pad & 0x4000) : (pad >> 14) & 1;
            cnt = D_005F6E48;
            if (cnt < 18) {
                dk = ((trig >> 14) & 1) - ((trig >> 12) & 1);
                if (dk != 0) {
                    cnt = 0;
                    i = dk;
                    D_005F6E48 = 0;
                } else {
                    i = 0;
                }
            }
            if ((trig & 0x5000) && cnt >= 18) {
                D_005F6E48 = 0;
            }
            if (i != 0) {
                Snd_PlaySe(3, 0);
            }
            j = D_005F650E + i;
            if (j < 0) {
                j += n;
            } else if (j >= n) {
                j -= n;
            }
            if ((D_0051EFE0[ids[j]].id == 1 || D_0051EFE0[ids[j]].id == 6) && Pad_CountReady() < 2) {
                if (i == 0) {
                    i = -1;
                } else {
                    i *= 2;
                }
            } else if (D_0051EFE0[ids[j]].id == 10 && func_003BFEE8() == 0) {
                if (i == 0) {
                    i = -1;
                } else {
                    i *= 2;
                }
            }
            D_005F650E += i;
            if (D_005F650E < 0) {
                D_005F650E += n;
            } else if (D_005F650E >= n) {
                D_005F650E -= n;
            }
            D_005F65D8 = i * 26;
        } else {
            D_005F6E40.a = 0;
            D_005F6E40.b = 0;
        }
        old = D_005F65D8;
        if (D_005F65D8 > 0) {
            D_005F65D8 -= 4;
        } else if (D_005F65D8 < 0) {
            D_005F65D8 += 4;
        }
        if ((old ^ D_005F65D8) & 0x8000) {
            D_005F65D8 = 0;
        }
        if (trig & 0xA000) {
            Snd_PlaySe(3, 0);
            func_003B0358();
        }
        D_005F65A0 = func_003B0348();
        switch (D_0051EFE0[ids[D_005F650E]].id) {
        case 1:
            if (Pad_CountReady() < 3) {
                D_005F65DC = 0;
                break;
            }
        case 0:
        case 3:
        case 4:
        case 8:
            D_005F65DC = 1;
            break;
        default:
            D_005F65DC = 0;
            break;
        }
        func_00374548();
        D_005F6E44 = func_003BCFB8(D_0051EFE0[ids[D_005F650E]].id);
        if ((s16)D_005F6E44 != 0) {
            gSubMode = 3;
            D_005F65D8 = 0;
            D_005F6E46 = D_0051EFE0[ids[D_005F650E]].id;
            func_003A6BD0();
            D_005F6E40.a = 0x80;
        }
        break;
    case 2:
        Mode_Change(2);
        func_003B8900();
        break;
    case 3:
        gSubMode = 4;
        break;
    case 4:
        if (D_005F6E44 != 0) {
            kind = D_005F6E46;
            side = D_005F6E44 & 1;
            stay = 0;
            switch (kind) {
            case 1:
            case 6:
                if ((s16)D_005F6E44 != 3) {
                    if (Pad_CountReady() < 3) {
                        for (p = 0; p < 4; p++) {
                            if (Pad_GetPlayerMode(p) >= 0 && func_003B00F0(p) == 0) {
                                break;
                            }
                        }
                        if (p == 4) {
                            gSubMode = 1;
                            stay = 1;
                            break;
                        }
                    } else {
                        p = side * 2;
                        if (Pad_GetPlayerMode(p) < 0) {
                            p ^= 1;
                        }
                        if (Pad_GetPlayerMode(p) < 0) {
                            p ^= 2;
                        }
                        if (Pad_GetPlayerMode(p) < 0) {
                            p ^= 1;
                        }
                    }
                    func_003B00A8(1, p, side);
                    func_003B00D8(p, 1);
                    func_003AFFE0(1, p);
                    func_003B0380(side);
                    D_005F6E44 = 3;
                }
                if (Pad_CountReady() < 3) {
                    func_003B0378(0);
                } else {
                    func_003B0378(D_005F65A0);
                }
                if (kind == 6) {
                    func_003B0378(0);
                }
                Game_Start(kind, (s16)D_005F6E44);
                func_003992C8(1);
                break;
            case 8:
                func_003BFE78(4);
            case 0:
            case 3:
            case 4:
                func_003B0378(D_005F65A0);
                Game_Start(kind, (s16)D_005F6E44);
                func_003992C8(1);
                break;
            case 2:
            case 5:
                func_003B0378(0);
                Game_Start(kind, (s16)D_005F6E44);
                func_003992C8(1);
                break;
            case 7:
                func_003B0378(1);
                Game_Start(kind, (s16)D_005F6E44);
                func_003992C8(1);
                break;
            case 11:
                Unlock_ApplyAll();
                gSubMode = 0;
                gMode = 9;
                func_003B4B18();
                func_003992C8(1);
                break;
            case 9:
                Unlock_ApplyAll();
                func_003992C8(1);
                func_003BFE78(2);
                gMode = 12;
                break;
            case 10:
                Unlock_ApplyAll();
                func_003992C8(1);
                gSubMode = 0;
                gMode = 16;
                func_003B4B18();
                Gallery_Enter();
                break;
            }
            if (!stay) {
                Snd_PlaySe(5, 0);
                func_003B8900();
            }
        }
        break;
    }
    if (gSubMode > 0 && func_0039C778() == 0) {
        Sprite_SetBlend(1, 0);
        blink = 0;
        func_003BB4E8(&D_0043FEC0[0], &D_005F6E40.a, 0);
        func_003BB4E8(&D_0043FEC0[1], &D_005F6E40.a, 0);
        D_005F6E40.a += 8;
        func_003DBAA8(0, 0x27F, 0x13B, 0x1A1);
        y = D_005F65D8 + 0x143;
        if (gSubMode == 1) {
            blink = gSubMode;
            if (D_005F6510 > 0x1D7) {
                blink = 0;
            }
        }
        for (i = 0; i < 5; i++) {
            k = i + D_005F650E - 2;
            if (k < 0) {
                k += n;
            } else if (k >= n) {
                k -= n;
            }
            e = &D_0051EFE0[ids[k]];
            col = e->unk6;
            if ((e->id == 1 || e->id == 6) && Pad_CountReady() < 2) {
                col = 0x16;
            } else if (e->id == 10 && func_003BFEE8() == 0) {
                col = 0x16;
            } else if ((gFrameCount & 4) && blink) {
                if (e->id == 8 && D_005F6E50 != 0) {
                    col = 9;
                }
                if (e->id == 9 && D_005F6E51 != 0) {
                    col = 9;
                }
                if (e->id == 10 && D_005F6E52 != 0) {
                    col = 9;
                }
            }
            Text_Printf(D_0051F0C0, (0x280 - Text_Printf(D_0051F0B0, 10, 5, e->name)) / 2, y, 10, 5, col, e->name);
            y += 26;
        }
        func_003BB680(&D_0043FE30[0], 0xFFFF0, 0);
        func_003BB680(&D_0043FE30[1], 0xFFFF0, 0);
        if (((D_005F6E40.b++ / 24) & 1) && D_005F6E40.a >= 0x80 && blink) {
            func_003CCF40(0xB7, 0x1BD, 0x166, 3);
        }
        func_003DBAA8(0, 0x27F, 0, 0x1BF);
    }
    if (gMode == 3) {
        func_003BCBC8(5);
    }
    if (func_003BFF40() != 0) {
        D_005F6510 = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/modes_b", Mode_MainMenu);
#endif
s32 func_003BDEB8(void) {
    s32 r = 0;

    if (gMode == 2) {
        r = gSubMode == 8;
    }
    return r;
}
s32 func_003BDEE0(void) {
    s32 r = 0;

    if (gMode == 2) {
        r = gSubMode == 7;
    }
    return r;
}
void func_003BDF08(void) {
    func_003B9078(1);
    gSubMode = 6;
    gMode = 2;
}
s32 func_003BDF38(void) {
    s32 r = 0;

    if (gMode == 2) {
        r = gSubMode == 1 || gSubMode == 2;
    }
    return r;
}
