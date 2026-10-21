/* 0x3CD298-0x3D0660 (module sprite2d, docs/game_overview.md section 4; docs/systems/render.md section 14): the 2D
 * sprite and text primitives of the HUD and the menus. .rodata 0x520648-0x520C40 and .lit4 0x5F5364-0x5F5370 are its
 * own (subsegments). Every NON_MATCHING draft (#if 0) compiles when enabled. */
#include "common.h"
#include "fighter.h"

/* ---- sprite2d part A: shared declarations ---- */
typedef struct {
    /* 0x0 */ u8 r;
    /* 0x1 */ u8 g;
    /* 0x2 */ u8 b;
    /* 0x3 */ u8 a;
    /* 0x4 */ u8 pad4[2];
} S2aColor;

/* GS image of a texture or CLUT: TBP0 / TBW / PSM, or CBP / - / CPSM. */
typedef struct {
    /* 0x0 */ u32 bp;
    /* 0x4 */ u32 bw;
    /* 0x8 */ u32 psm;
} S2aImage;

/* D_00440F60: the sprite table, 0x20 bytes per entry. */
typedef struct {
    /* 0x00 */ S2aImage *img;
    /* 0x04 */ s32 u; /* CBP offset for a CLUT entry */
    /* 0x08 */ s32 v;
    /* 0x0C */ s32 w;
    /* 0x10 */ s32 h;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ u8 unk1C[4];
} S2aTexEnt;

/* D_00440F20: the current sprite (TEX0, screen rectangle, texel rectangle, four colours). */
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
} S2aSprite;

extern S2aSprite D_00440F20;
extern s32 D_005F6730;
extern u64 D_005F6748;

extern S2aColor D_0043B840[];
extern u64 D_005F6738;

u64 *func_003994C0(void);
u64 *func_00399520(void);
void func_00399580(u64 *p);
void func_00399650(u64 *p);
void func_00399728(s32 a, s32 b);
void func_00399820(s32 a, s32 b);
void func_003637D0(void);
void func_003638F0(void);
void func_003CF608(s32 a);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_SetBlend(s32 a, s32 b);
void Sprite_Draw(s32 a, s32 b, s32 c, s32 x, s32 y);
void Sprite_DrawScaled(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);
void Sprite_DrawScaledBlended(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);
typedef struct {
    /* 0x0 */ u64 test;
    /* 0x8 */ u64 alpha;
} S2aBlend;


/* ---- second half (func_003CE5D8-func_003D0620) declarations ---- */
typedef struct S2bPair {
    s32 unk0;
    s32 unk4;
} S2bPair;

typedef struct S2bChar {
    u8 pad0[4];
    u8 unk4;
} S2bChar;

extern const S2bPair D_005206B8[];
extern S2bChar *D_00415840[];
extern u64 *D_00444240[];
extern const s32 D_00520990[];
extern u8 D_005F6778[];

s32 func_0035D048(s32 n, u32 dest);
u8 *Str_Copy(u8 *dst, const u8 *src);
void Sprite_Draw(s32 a, s32 b, s32 c, s32 x, s32 y);

s64 func_003CEA80(s32 n, u64 *dest, s32 size);
s32 func_003CEAC8(u64 *p);
u64 *func_003CEB08(u64 *p, s32 n);
void func_003CF1C0(s32 a, s32 b, s32 x, s32 y);
void func_003CF1E8(s32 i, s32 x, s32 y);
s32 func_003CF230(s32 chr, s32 costume);
s32 func_003CF3D8(u32 v);
s32 func_003CF4C8(s32 a, s32 i);
s32 func_003CF4E8(s32 i);

typedef struct S2bName {
    /* 0x0 */ u8 str[4];
    /* 0x4 */ u16 len;
    /* 0x6 */ u8 unk6;
    /* 0x7 */ u8 unk7;
    /* 0x8 */ u16 unk8;
    /* 0xA */ u16 unkA;
    /* 0xC */ u16 unkC;
    /* 0xE */ u16 unkE;
    /* 0x10 */ s8 pad0;
    /* 0x11 */ s8 pad1;
} S2bName;

extern Fighter gFighters[];
extern const u32 D_00520970[];
extern u8 *D_004443D8[];

f32 sqrtf(f32);
f32 sinf(f32);
f32 cosf(f32);
u8 *func_0035C6F0(s32 i);
s32 Str_ComparePrefix(u8 *str, u8 *prefix);

s32 func_003CF488(s32 v);
u32 Num_ToBcd(u32 v);
void func_003CF408(f32 *v, f32 angle);
void func_003D01C0(u8 *dst, u32 i);
s32 func_003D0220(u8 *s);
s32 func_003D0290(S2bName *p);
void NameEntry_Init(S2bName *p, u16 a, u16 b, u16 c);
void Fighter_ClearLife(s32 side);

/* Bare sqrt.s as in model_anim_b.c (keeps the loads after it in place). */
static inline f32 S2bSqrtS(f32 x) {
    f32 r;

    __asm__ volatile("nop\n\tnop\n\tsqrt.s %0, %1" : "=f"(r) : "f"(x));
    return r;
}

/* A sprite image of the banks (D_00440F60, 0x20 bytes): its GS buffer, its rectangle in the buffer and its upload
 * packet (BITBLTBUF / TRXPOS / TRXREG / TRXDIR then the image) inside the loaded bank. */
typedef struct S2bImage {
    /* 0x00 */ u32 *buf; /* base pointer, width, format */
    /* 0x04 */ s32 x;
    /* 0x08 */ s32 y;
    /* 0x0C */ s32 w;
    /* 0x10 */ s32 h;
    /* 0x14 */ u64 *pk;
    /* 0x18 */ s32 qwc;
    /* 0x1C */ s32 unk1C;
} S2bImage;

extern S2bImage D_00440F60[];
extern u8 D_01FAD3C0[];
extern u8 D_01FAD340[];
extern u8 D_00455E30[];
extern s32 D_005F6730;
extern u64 D_005F6738;
extern const S2bPair D_00520C18[];

void sceGifPkInit(void *pk, void *base);
void sceGifPkRef(void *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceGifPkEnd(void *pk, u32 a, u32 b, u32 c);
u32 sceGifPkSize(void *pk);
void sceGifPkTerminate(void *pk);
void FlushCache(s32 op);
void func_0035E6B8(void *tag);
u32 Dma_Wait(volatile u32 *chan);
void exit(s32 status) __attribute__((noreturn));
void func_003637D0(void);
void func_003638F0(void);
u64 *func_003639E0(void);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_SetBlend(s32 a, s32 b);
void Sprite_DrawScaledBlended(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);

void func_003CEB68(s32 img);
void func_003CEC98(s32 img);
void Sprite_SendImagePacket(u64 *pk, s32 qwc);
void func_003CEEC8(s32 img, s32 bank, u64 *data);
void Sprite_UploadBank(s32 bank, u64 *data, s32 size, s32 r);
s32 Sprite_LoadBank(s32 bank, u64 *dest, s32 size);
void func_003CF608(s32 a);
void func_003D0548(s32 x, s32 y, s32 kind, s32 alpha);
void func_003D0620(void);

/* Pair rules of the partner counter (D_005207F0, 0x4C entries): character 0x22 matches any, the masks select
 * costumes, op -1 subtracts, 1 adds, -9 clears. */
typedef struct S2bPairRule {
    /* 0x0 */ s8 chr0;
    /* 0x1 */ s8 mask0;
    /* 0x2 */ s8 chr1;
    /* 0x3 */ s8 mask1;
    /* 0x4 */ s8 op;
} S2bPairRule;

extern const S2bPairRule D_005207F0[];

/* Ten 0x18-byte records at D_00520A68 (splat cut the table after its first 8 bytes: D_00520A70 is field +8). */
typedef struct S2bSrc {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
} S2bSrc;

typedef struct S2bDst {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
} S2bDst;

extern const S2bSrc D_00520A68[];
extern const S2bSrc D_00520A70[];
extern S2bDst D_00444310[];

s32 func_003CF278(u8 c0, u8 k0, u8 c1, u8 k1, s32 kind, s32 n);
void func_003CF500(void);

u64 *Sprite_BeginPacket(s32 buf);
void Sprite_EndPacket(s32 buf, u64 *end);
void func_003CE5D8(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, s32 tex);
void func_003CE920(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 alpha);
extern u64 D_005F6740;
void func_003CE770(u32 x0, u32 y0, u64 c0, u32 x1, u32 y1, u64 c1, u32 x2, u32 y2, u64 c2, u32 x3, u32 y3, u64 c3);

/* Per-set layout tables copied to the stack by func_003CF608. */
typedef struct S2bSetCols {
    s32 v[6][4];
} S2bSetCols;

typedef struct S2bSetPos {
    s32 v[6][2];
} S2bSetPos;

extern const S2bSetCols D_00520B58;
extern const S2bSetPos D_00520BB8;
void func_003CF850(s32 cx, s32 cy, s32 r);
void func_003CFAD8(s32 r, s32 pct, s32 alpha);
extern u32 D_00415728[];
extern u32 D_00415738[];
extern const u8 D_00520BE8[];
s32 func_0035C508(s32 min, s32 value, s32 max);
s32 NameEntry_Step(S2bName *p, s32 mode);
void func_003CFE38(s32 x, s32 y, s32 w, s32 h, u8 *c);


extern const S2aBlend D_00520648[];
u64 *Sprite_BeginPacket(s32 buf);
void Sprite_EndPacket(s32 buf, u64 *p);
void Sprite_SetTexture(s32 tex, s32 clut);
void Sprite_DrawCurrent(s32 a);
void Sprite_DrawCurrentGouraud(s32 a);
s32 func_003CD298(void);
void Sprite_DrawBlended(s32 a, s32 b, s32 c, s32 x, s32 y);
typedef f32 S2aVec[4];
typedef f32 S2aMtx[4][4];
void Mtx_Unit(S2aMtx m);
void sceVu0RotMatrixZ(S2aMtx m0, S2aMtx m1, f32 rz);
void sceVu0ApplyMatrix(f32 *out, S2aMtx m, f32 *v);
/* D_00440D50: the two record tables (ten scores, ten times) and the last entry. */
typedef struct {
    /* 0x0 */ s32 score;
    /* 0x4 */ u8 c0;
    /* 0x5 */ u8 c1;
    /* 0x6 */ u8 name[4];
    /* 0xA */ u8 padA[2];
} S2aScoreEnt;

typedef struct {
    /* 0x0 */ u8 c0;
    /* 0x1 */ u8 c1;
    /* 0x2 */ u16 time;
    /* 0x4 */ u8 name[4];
} S2aTimeEnt;

typedef struct {
    /* 0x000 */ S2aScoreEnt *lastScore;
    /* 0x004 */ S2aTimeEnt *lastTime;
    /* 0x008 */ S2aScoreEnt score[10];
    /* 0x080 */ S2aTimeEnt time[10];
    /* 0x0D0 */ u8 unkD0[0x40];
    /* 0x110 */ s16 unk110;
    /* 0x112 */ u8 unk112;
    /* 0x113 */ u8 unk113;
    /* 0x114 */ u8 unk114;
    /* 0x115 */ u8 unk115;
    /* 0x116 */ u8 unk116;
} S2aRecords;

/* Result of func_003CD0E0. */
typedef struct {
    /* 0x0 */ s32 score;
    /* 0x4 */ u16 side;
    /* 0x6 */ u8 c0;
    /* 0x7 */ u8 c1;
} S2aScoreReq;

extern S2aRecords D_00440D50;
extern u8 D_005F6728[];
s32 func_003CD0E0(S2aScoreReq *r);
s32 func_003AFFB8(s32 i, s32 a);
u8 *Str_Copy(u8 *dst, const u8 *src);
/* D_00440940: per side, character and costume of the two team members (0x60 bytes). */
typedef struct {
    /* 0x00 */ s32 chr0;
    /* 0x04 */ s32 costume0;
    /* 0x08 */ u8 pad8[0x28];
    /* 0x30 */ s32 chr1;
    /* 0x34 */ s32 costume1;
    /* 0x38 */ u8 pad38[0x28];
} S2aTeam;

typedef struct {
    /* 0x0 */ s32 idx;
    /* 0x4 */ s32 val;
} S2aPair;

extern S2aTeam D_00440940[];
s32 func_003CF230(s32 chr, s32 costume);
s32 func_003CD1E8(s32 time, s32 c0, s32 c1);
s32 Sort_PairsByValue(S2aPair *a, s32 n, s32 desc);
/* ---- end part A ---- */

/* NON_MATCHING: register allocation in the search loop (j / 12 / entry pointer in a1 / a2 / v1, original a0 / a1 /
 * a2) and the last store out of the Str_Copy delay slot, 28 lines */
#if 0
s32 func_003CD298(void) {
    S2aScoreReq r;
    S2aRecords *t;
    S2aScoreEnt *e;
    S2aScoreEnt *q;
    s32 rank;
    s32 i;
    s32 j;
    u8 c0;
    u8 c1;

    rank = func_003CD0E0(&r);
    if (rank == 0x7B) {
        return 0;
    }
    t = &D_00440D50;
    t->unk110 = 0;
    j = 9;
    c0 = r.c0;
    c1 = r.c1;
    e = &t->score[rank];
    for (i = 0; i < 10; i++) {
        q = &t->score[i];
        if ((c0 == q->c0 && c1 == q->c1) || (c1 == q->c0 && c0 == q->c1)) {
            j = i;
        }
    }
    for (i = j; i > rank; i--) {
        S2aScoreEnt *d = &t->score[i];
        S2aScoreEnt *o = &t->score[i - 1];
        d->score = o->score;
        d->c0 = o->c0;
        d->c1 = o->c1;
        d->name[0] = o->name[0];
        d->name[1] = o->name[1];
        d->name[2] = o->name[2];
        d->name[3] = o->name[3];
    }
    t->unk113 = r.side;
    t->lastScore = e;
    t->unk112 |= 1;
    t->unk115 = func_003AFFB8(r.side, 0);
    t->unk116 = func_003AFFB8(r.side, 1);
    t->unk114 = c0;
    e->score = r.score;
    e->c0 = c0;
    e->c1 = c1;
    Str_Copy(e->name, D_005F6728);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CD298);
#endif

s32 func_003CD440(s32 side, s32 time) {
    S2aTimeEnt tmp[11];
    S2aPair pairs[11];
    S2aRecords *t;
    S2aTimeEnt *src;
    S2aTimeEnt *dst;
    S2aPair *pr;
    s32 c0;
    s32 c1;
    s32 i;
    s32 v;

    c0 = func_003CF230(D_00440940[side].chr0, D_00440940[side].costume0);
    c1 = func_003CF230(D_00440940[side].chr1, D_00440940[side].costume1);
    if (func_003CD1E8(time, c0, c1) == 0) {
        return 0;
    }
    t = &D_00440D50;
    t->unk113 = side;
    t->unk110 = 1;
    src = t->time;
    t->unk115 = func_003AFFB8(side, 0);
    t->unk116 = func_003AFFB8(side, 1);
    t->unk114 = c0;
    pr = pairs;
    dst = tmp;
    for (i = 0; i < 10; i++) {
        dst->c0 = src->c0;
        dst->c1 = src->c1;
        v = src->time;
        dst->time = v;
        Str_Copy(dst->name, src->name);
        pr->val = v;
        pr->idx = i;
        if ((c0 == dst->c0 && c1 == dst->c1) || (c1 == dst->c0 && c0 == dst->c1)) {
            pr->val = 0;
        }
        src++;
        dst++;
        pr++;
    }
    dst->c0 = c0;
    dst->c1 = c1;
    dst->time = time;
    Str_Copy(dst->name, D_005F6728);
    pr->idx = i;
    pr->val = time;
    pr = pairs;
    Sort_PairsByValue(pr, 11, 0);
    for (i = 0; i < 10; i++) {
        v = pr[i].idx;
        src = &t->time[i];
        dst = &tmp[v];
        if (v == 10) {
            t->lastTime = src;
        }
        src->c0 = dst->c0;
        src->c1 = dst->c1;
        src->time = dst->time;
        Str_Copy(src->name, dst->name);
    }
    t->unk112 |= 2;
    return 1;
}

void func_003CD678(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        S2aColor *c = &D_0043B840[i];
        c->r = 0xFF;
        c->g = 0xFF;
        c->b = 0xFF;
        c->a = 0xFF;
    }
}

void func_003CD6C0(s32 i, u8 r, u8 g, u8 b, u8 a) {
    S2aColor *c = &D_0043B840[i];

    c->r = r;
    c->g = g;
    c->b = b;
    c->a = a;
}

void func_003CD6E8(void) {
    func_003CF608(4);
    func_003637D0();
    Sprite_LoadBank(6, func_003639E0(), 0x249F0);
    func_003638F0();
}

void func_003CD728(void) {
    Sprite_SetFilter(1, 0);
    Sprite_SetBlend(1, 1);
    D_005F6738 = 0x80404040;
    Sprite_Draw(1, 0x107, 0x108, 0, 0);
    Sprite_SetFilter(1, 1);
}

void func_003CD788(void) {
    Sprite_SetFilter(1, 0);
    Sprite_SetBlend(1, 1);
    D_005F6738 = 0x80808080;
    Sprite_Draw(1, 0xBE, 0xBF, 0xE4, 0xC8);
    Sprite_SetFilter(1, 1);
}

void func_003CD7E8(s32 buf, s32 i) {
    S2aTexEnt *e = (S2aTexEnt *)&D_00440F60[i];
    s32 a = e->unk14;
    s32 b = e->unk18;

    if (buf == 0) {
        func_00399728(a, b);
    } else {
        func_00399820(a, b);
    }
}

u64 *Sprite_BeginPacket(s32 buf) {
    if (buf == 0) {
        return func_003994C0();
    }
    return func_00399520();
}

void Sprite_EndPacket(s32 buf, u64 *p) {
    if (buf == 0) {
        func_00399580(p);
    } else {
        func_00399650(p);
    }
}

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520648);

void Sprite_SetBlend(s32 a, s32 b) {
    u64 *p = Sprite_BeginPacket(a);

    *p++ = 0x3000000000008001;
    *p++ = 0xEEE;
    *p++ = D_00520648[b].test;
    *p++ = 0x48;
    *p++ = D_00520648[b].alpha;
    *p++ = 0x43;
    *p++ = 0;
    *p++ = 9;
    Sprite_EndPacket(a, p);
}

void Sprite_SetFilter(s32 a, s32 b) {
    u64 *p = Sprite_BeginPacket(a);

    *p++ = 0x1000000000008001;
    *p++ = 0xE;
    *p++ = 1 | ((u64)b << 5) | ((u64)b << 6);
    *p++ = 0x15;
    Sprite_EndPacket(a, p);
}

void Sprite_TexFlush(s32 a) {
    u64 *p = Sprite_BeginPacket(a);

    *p++ = 0x1000000000008001;
    *p++ = 0xE;
    *p++ = 0;
    *p++ = 0x3F;
    Sprite_EndPacket(a, p);
}

void Sprite_SetTexture(s32 tex, s32 clut) {
    S2aTexEnt *c = (S2aTexEnt *)&D_00440F60[clut];
    S2aTexEnt *t = (S2aTexEnt *)&D_00440F60[tex];
    S2aSprite *s = &D_00440F20;

    s->tex0 = (u64)t->img->bp | ((u64)t->img->bw << 14) | ((u64)t->img->psm << 20) | ((u64)10 << 26) | ((u64)10 << 30)
            | ((u64)1 << 34) | ((u64)(c->img->bp + c->u) << 37) | ((u64)c->img->psm << 51) | ((u64)1 << 61);
    s->x = 0;
    s->y = 0;
    s->w = t->w;
    s->h = t->h;
    s->u = t->u;
    s->v = t->v;
    s->tw = t->w;
    s->th = t->h;
    s->unk28 = 0x80;
    s->unk2C = 0x80;
    s->unk30 = 0x80;
    s->unk34 = 0x80;
    s->rot = 0.0f;
}

void Sprite_DrawCurrent(s32 a) {
    u64 *p = Sprite_BeginPacket(a);
    S2aSprite *s = &D_00440F20;
    u64 prim = D_005F6748 | 0x316;

    p[0] = 0x7400000000008001;
    p[1] = 0x5353170;
    p[2] = prim;
    p[3] = s->tex0;
    p[4] = D_005F6738;
    p[5] = (u64)((s->u << 4) | 8) | ((u64)((s->v << 4) | 8) << 16);
    p[6] = (u64)((s->x + 0x6C0) << 4) | ((u64)((s->y + 0x720) << 4) << 16) | ((u64)(D_005F6730 >> 4) << 32);
    p[7] = (u64)(((s->u + s->tw - 1) << 4) | 8) | ((u64)(((s->v + s->th - 1) << 4) | 8) << 16);
    p[8] = (u64)((s->x + s->w + 0x6C0) << 4) | ((u64)((s->y + s->h + 0x720) << 4) << 16) | ((u64)(D_005F6730 >> 4) << 32);
    Sprite_EndPacket(a, p + 10);
}

void func_003CDC10(s32 a) {
    u64 *p = Sprite_BeginPacket(a);
    S2aSprite *s = &D_00440F20;
    u64 prim = D_005F6748 | 0x314;
    u32 u0, v0, u1, v1;
    u32 x0, y0, x1, y1;

    p[0] = 0xB400000000008001;
    p[1] = 0x53535353170;
    p[2] = prim;
    p[3] = s->tex0;
    p[4] = D_005F6738;
    u0 = (s->u << 4) | 8;
    v0 = (s->v << 4) | 8;
    u1 = ((s->u + s->tw - 1) << 4) | 8;
    v1 = ((s->v + s->th - 1) << 4) | 8;
    p[5] = u0 | ((u64)v0 << 16);
    p[7] = u1 | ((u64)v0 << 16);
    p[9] = u0 | ((u64)v1 << 16);
    p[11] = u1 | ((u64)v1 << 16);
    x0 = (s->x + 0x6C0) << 4;
    y0 = (s->y + 0x720) << 4;
    x1 = (s->x + s->w + 0x6C0) << 4;
    y1 = (s->y + s->h + 0x720) << 4;
    p[6] = x0 | ((u64)y0 << 16) | ((u64)(D_005F6730 >> 4) << 32);
    p[8] = x1 | ((u64)y0 << 16) | ((u64)(D_005F6730 >> 4) << 32);
    p[10] = x0 | ((u64)y1 << 16) | ((u64)(D_005F6730 >> 4) << 32);
    p[12] = x1 | ((u64)y1 << 16) | ((u64)(D_005F6730 >> 4) << 32);
    Sprite_EndPacket(a, p + 14);
}

void Sprite_DrawCurrentRotated(s32 a) {
    S2aVec v;
    S2aMtx m;
    u64 *p = Sprite_BeginPacket(a);
    S2aSprite *s = &D_00440F20;
    u64 prim = D_005F6748 | 0x314;
    u32 u0, v0, u1, v1;
    f32 hw, hh;
    s32 z;

    p[0] = 0xB400000000008001;
    p[1] = 0x53535353170;
    p[2] = prim;
    p[3] = s->tex0;
    p[4] = D_005F6738;
    u0 = (s->u << 4) | 8;
    v0 = (s->v << 4) | 8;
    u1 = ((s->u + s->tw - 1) << 4) | 8;
    v1 = ((s->v + s->th - 1) << 4) | 8;
    p[5] = u0 | ((u64)v0 << 16);
    p[7] = u1 | ((u64)v0 << 16);
    p[9] = u0 | ((u64)v1 << 16);
    p[11] = u1 | ((u64)v1 << 16);
    Mtx_Unit(m);
    sceVu0RotMatrixZ(m, m, s->rot);
    hw = (s->w << 4) >> 1;
    m[3][0] = (s->x + 0x6C0) << 4;
    m[3][1] = (s->y + 0x720) << 4;
    hh = (s->h << 4) >> 1;
    z = D_005F6730 >> 4;
    v[0] = -hw;
    v[1] = -hh;
    v[2] = 0.0f;
    v[3] = 1.0f;
    sceVu0ApplyMatrix(v, m, v);
    p[6] = (s32)v[0] | ((u64)(s32)v[1] << 16) | ((u64)z << 32);
    v[0] = hw;
    v[1] = -hh;
    sceVu0ApplyMatrix(v, m, v);
    p[8] = (s32)v[0] | ((u64)(s32)v[1] << 16) | ((u64)z << 32);
    v[0] = -hw;
    v[1] = hh;
    sceVu0ApplyMatrix(v, m, v);
    p[10] = (s32)v[0] | ((u64)(s32)v[1] << 16) | ((u64)z << 32);
    v[0] = hw;
    v[1] = hh;
    sceVu0ApplyMatrix(v, m, v);
    p[12] = (s32)v[0] | ((u64)(s32)v[1] << 16) | ((u64)z << 32);
    Sprite_EndPacket(a, p + 14);
}

/* NON_MATCHING: the GIF tag stores p[0] / p[1] come out swapped; the r / gb locals (fake) keep the colour constants
 * 0x80 and 0x808000 apart as in the original, 2 lines */
#if 0
void Sprite_DrawCurrentGouraud(s32 a) {
    u64 *p = Sprite_BeginPacket(a);
    S2aSprite *s = &D_00440F20;
    u32 u0, v0, u1, v1;
    u32 x0, y0, x1, y1;
    u64 r = 0x80;      /* fake */
    u64 gb = 0x808000; /* fake */

    p[0] = 0xE400000000008001;
    p[1] = 0x53153153153170;
    p[2] = 0x35C;
    p[3] = s->tex0;
    p[4] = ((u64)s->unk28 << 24) | r | gb;
    p[7] = ((u64)s->unk2C << 24) | r | gb;
    p[10] = ((u64)s->unk30 << 24) | r | gb;
    p[13] = ((u64)s->unk34 << 24) | r | gb;
    u0 = (s->u << 4) | 8;
    v0 = (s->v << 4) | 8;
    u1 = ((s->u + s->tw - 1) << 4) | 8;
    v1 = ((s->v + s->th - 1) << 4) | 8;
    p[5] = u0 | ((u64)v0 << 16);
    p[8] = u1 | ((u64)v0 << 16);
    p[11] = u0 | ((u64)v1 << 16);
    p[14] = u1 | ((u64)v1 << 16);
    x0 = (s->x + 0x6C0) << 4;
    y0 = (s->y + 0x720) << 4;
    x1 = (s->x + s->w + 0x6C0) << 4;
    y1 = (s->y + s->h + 0x720) << 4;
    p[6] = x0 | ((u64)y0 << 16) | ((u64)(D_005F6730 >> 4) << 32);
    p[9] = x1 | ((u64)y0 << 16) | ((u64)(D_005F6730 >> 4) << 32);
    p[12] = x0 | ((u64)y1 << 16) | ((u64)(D_005F6730 >> 4) << 32);
    p[15] = x1 | ((u64)y1 << 16) | ((u64)(D_005F6730 >> 4) << 32);
    Sprite_EndPacket(a, p + 16);
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", Sprite_DrawCurrentGouraud);
#endif

void Sprite_Draw(s32 a, s32 b, s32 c, s32 x, s32 y) {
    D_005F6748 = 0;
    Sprite_SetTexture(b, c);
    D_00440F20.x = x;
    D_00440F20.y = y;
    Sprite_DrawCurrent(a);
}

void Sprite_DrawBlended(s32 a, s32 b, s32 c, s32 x, s32 y) {
    D_005F6748 = 0x40;
    Sprite_SetTexture(b, c);
    D_00440F20.x = x;
    D_00440F20.y = y;
    Sprite_DrawCurrent(a);
}

void Sprite_DrawScaled(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h) {
    D_005F6748 = 0;
    Sprite_SetTexture(b, c);
    D_00440F20.x = x;
    D_00440F20.y = y;
    D_00440F20.w = w;
    D_00440F20.h = h;
    Sprite_DrawCurrent(a);
}

void Sprite_DrawScaledBlended(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h) {
    D_005F6748 = 0x40;
    Sprite_SetTexture(b, c);
    D_00440F20.x = x;
    D_00440F20.y = y;
    D_00440F20.w = w;
    D_00440F20.h = h;
    Sprite_DrawCurrent(a);
}

void func_003CE418(s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 du, s32 dv, s32 tw, s32 th, s32 c0, s32 c1,
                   s32 c2, s32 c3) {
    Sprite_SetTexture(tex, clut);
    D_00440F20.x = x;
    D_00440F20.y = y;
    D_00440F20.w = w;
    D_00440F20.h = h;
    D_00440F20.u += du;
    D_00440F20.v += dv;
    D_00440F20.tw = tw;
    D_00440F20.th = th;
    D_00440F20.unk28 = c0;
    D_00440F20.unk2C = c1;
    D_00440F20.unk30 = c2;
    D_00440F20.unk34 = c3;
    Sprite_DrawCurrentGouraud(1);
}

void func_003CE508(s32 a, s32 tex, s32 clut, s32 x, s32 y, s32 w, s32 h, s32 du, s32 dv, s32 tw, s32 th) {
    D_005F6748 = 0x40;
    Sprite_SetTexture(tex, clut);
    D_00440F20.x = x;
    D_00440F20.y = y;
    D_00440F20.w = w;
    D_00440F20.h = h;
    D_00440F20.u += du;
    D_00440F20.v += dv;
    D_00440F20.tw = tw;
    D_00440F20.th = th;
    Sprite_DrawCurrent(a);
}

/* NON_MATCHING: the constant header stores and the lq reload of the 9th argument are scheduled earlier than in the original (registers and spills already match), 45 lines */
#if 0
void func_003CE5D8(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, s32 tex) {
    s32 z = D_005F6730 >> 4;
    u64 *p = Sprite_BeginPacket(1);
    u64 *q;

    p[0] = 0x2000000000000001;
    p[1] = 0xEE;
    p[2] = 0x5100D;
    p[3] = 0x48;
    p[4] = D_005F6740;
    p[5] = 0x43;
    q = p + 6;
    q[0] = 0x6400000000008001;
    q[1] = 0x555510;
    q[2] = ((s64)tex << 6) | 0x304;
    q[3] = D_005F6738;
    q[4] = (s64)(x0 * 16 + 0x6C00) | ((s64)(y0 * 16 + 0x7200) << 16) | ((s64)z << 32);
    q[5] = (s64)(x1 * 16 + 0x6C00) | ((s64)(y1 * 16 + 0x7200) << 16) | ((s64)z << 32);
    q[6] = (s64)(x2 * 16 + 0x6C00) | ((s64)(y2 * 16 + 0x7200) << 16) | ((s64)z << 32);
    q[7] = (s64)(x3 * 16 + 0x6C00) | ((s64)(y3 * 16 + 0x7200) << 16) | ((s64)z << 32);
    Sprite_EndPacket(1, p + 14);
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CE5D8);
#endif

/* NON_MATCHING: y0 / x2 swapped between s4 and s5 and the header stores scheduled earlier, 51 lines */
#if 0
void func_003CE770(u32 x0, u32 y0, u64 c0, u32 x1, u32 y1, u64 c1, u32 x2, u32 y2, u64 c2, u32 x3, u32 y3, u64 c3) {
    s32 z = D_005F6730 >> 4;
    u64 *p = Sprite_BeginPacket(1);

    p[0] = 0x9400000000008001;
    p[1] = 0x515151510;
    p[2] = 0x30C;
    p[3] = c0;
    p[4] = (u64)((x0 + 0x6C0) << 4) | ((u64)((y0 + 0x720) << 4) << 16) | ((s64)z << 32);
    p[5] = c1;
    p[6] = (u64)((x1 + 0x6C0) << 4) | ((u64)((y1 + 0x720) << 4) << 16) | ((s64)z << 32);
    p[7] = c2;
    p[8] = (u64)((x2 + 0x6C0) << 4) | ((u64)((y2 + 0x720) << 4) << 16) | ((s64)z << 32);
    p[9] = c3;
    p[10] = (u64)((x3 + 0x6C0) << 4) | ((u64)((y3 + 0x720) << 4) << 16) | ((s64)z << 32);
    Sprite_EndPacket(1, p + 12);
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CE770);
#endif

/* NON_MATCHING: saved-register allocation of z / x0 / x1 (s5 / s6 / s4 in the original), every instruction otherwise in place, 22 lines */
#if 0
void func_003CE920(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 alpha) {
    s32 z = D_005F6730 >> 4;
    u64 *p = Sprite_BeginPacket(1);
    u64 *q;

    p[0] = 0x2000000000000001;
    p[1] = 0xEE;
    p[2] = 0x5100D;
    p[3] = 0x48;
    p[4] = 0x44;
    p[5] = 0x43;
    q = p + 6;
    q[0] = 0x7400000000008001;
    q[1] = 0x5151510;
    q[2] = 0x34B;
    q[3] = 0;
    q[4] = (s64)(x0 * 16 + 0x6C00) | ((s64)(y0 * 16 + 0x7200) << 16) | ((s64)z << 32);
    q[5] = (s64)alpha << 24;
    q[6] = (s64)(x1 * 16 + 0x6C00) | ((s64)(y1 * 16 + 0x7200) << 16) | ((s64)z << 32);
    q[7] = 0;
    q[8] = (s64)(x2 * 16 + 0x6C00) | ((s64)(y2 * 16 + 0x7200) << 16) | ((s64)z << 32);
    Sprite_EndPacket(1, p + 16);
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CE920);
#endif

/* s64 return [I]: an s32 return loses the original's `move v0, v1` after the shift. */
s64 func_003CEA80(s32 n, u64 *dest, s32 size) {
    D_00444240[n] = dest;
    return func_0035D048(n, (u32)dest) / 8;
}

s32 func_003CEAC8(u64 *p) {
    s32 n = (s32)(*p & 0x7FFF) * (s32)(*p >> 60) + 1;
    p += n * 2;
    return n + (s32)(*p & 0x7FFF) + 1;
}

u64 *func_003CEB08(u64 *p, s32 n) {
    s32 i;

    for (i = 0; i < n; i++) {
        p += func_003CEAC8(p) * 2;
    }
    return p;
}

void func_003CEB68(s32 img) {
    s32 bank = func_003CF488(img);
    s32 i = func_003CF4C8(img, bank);
    u32 *buf = D_00440F60[img].buf;
    u64 *pk = func_003CEB08(D_00444240[bank], i);
    u64 a, b;

    D_00440F60[img].pk = pk;
    D_00440F60[img].qwc = func_003CEAC8(pk);
    pk += 2;
    *pk = ((u64)buf[2] << 24) | ((u64)buf[0] << 32) | ((u64)buf[1] << 48) | ((u64)buf[2] << 56);
    pk += 2;
    a = pk[0];
    b = pk[2];
    D_00440F60[img].x = (a >> 32) & 0xFFF;
    D_00440F60[img].y = (a >> 48) & 0xFFF;
    D_00440F60[img].w = b & 0xFFF;
    D_00440F60[img].h = (b >> 32) & 0xFFF;
}

void func_003CEC98(s32 img) {
    s32 bank = func_003CF488(img);
    s32 i = func_003CF4C8(img, bank);
    u32 *buf = D_00440F60[img].buf;
    u64 *pk = func_003CEB08(D_00444240[bank], i);
    s32 x;

    D_00440F60[img].pk = pk;
    D_00440F60[img].qwc = func_003CEAC8(pk);
    pk += 4;
    x = (pk[0] >> 32) & 0xFFF;
    pk[0] = 0;
    pk += 2;
    if ((s32)(pk[0] & 0xFFF) == 0x10) {
        x *= 4;
    }
    D_00440F60[img].x = x;
    pk -= 4;
    pk[0] |= (u64)(buf[0] + x) << 32;
    pk[0] |= (u64)buf[1] << 48;
    pk[0] |= (u64)buf[2] << 56;
}

void Sprite_SendImagePacket(u64 *pk, s32 qwc) {
    pk[(s32)(*pk & 0x7FFF) * (s32)(*pk >> 60) * 2 + 2] |= 0x8000;
    sceGifPkInit(D_01FAD3C0, D_01FAD340);
    sceGifPkRef(D_01FAD3C0, pk, qwc, 0, 0, 0);
    sceGifPkRef(D_01FAD3C0, D_00455E30, 2, 0, 0, 0);
    sceGifPkEnd(D_01FAD3C0, 0, 0, 0);
    if (sceGifPkSize(D_01FAD3C0) + 1 >= 8) {
        exit(1);
    }
    sceGifPkTerminate(D_01FAD3C0);
    FlushCache(0);
    func_0035E6B8(D_01FAD340);
    Dma_Wait((volatile u32 *)0x1000A000);
}

/* NON_MATCHING: the original keeps a dead `li v0, 0x3B` from an `img == 0x39 || img == 0x3B` test and addresses TRXPOS through pk + 4 in a3, 18 lines */
#if 0
void func_003CEEC8(s32 img, s32 bank, u64 *data) {
    s32 i = func_003CF4C8(img, bank);
    u32 *buf = D_00440F60[img].buf;
    u64 *pk = func_003CEB08(data, i);
    s32 qwc = func_003CEAC8(pk);
    u64 *p = pk + 4;
    u64 a, b;
    s32 x;

    if (D_00440F60[img].y != 1234) {
        pk[2] = ((u64)buf[2] << 24) | ((u64)buf[0] << 32) | ((u64)buf[1] << 48) | ((u64)buf[2] << 56);
        if (img == 0x3B || img == 0x39) {
            if (img == 0x39) {
                *p = (*p & 0xF8000000FFFFFFFF) | 0x0140000000000000;
            } else if (img == 0x3B) {
                *p = (*p & 0xF8000000FFFFFFFF) | 0x0140008000000000;
            }
        }
        a = p[0];
        b = p[2];
        x = (a >> 32) & 0xFFF;
        D_00440F60[img].x = x;
        D_00440F60[img].y = (a >> 48) & 0xFFF;
        D_00440F60[img].w = b & 0xFFF;
        D_00440F60[img].h = (b >> 32) & 0xFFF;
    } else {
        x = (pk[4] >> 32) & 0xFFF;
        pk[4] = 0;
        if ((s32)(pk[6] & 0xFFF) == 0x10) {
            x *= 4;
        }
        D_00440F60[img].x = x;
        pk[2] |= (u64)(buf[0] + x) << 32;
        pk[2] |= (u64)buf[1] << 48;
        pk[2] |= (u64)buf[2] << 56;
    }
    Sprite_SendImagePacket(pk, qwc);
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CEEC8);
#endif

void Sprite_UploadBank(s32 bank, u64 *data, s32 size, s32 r) {
    s32 i;
    s32 last;

    i = func_003CF4E8(bank);
    last = func_003CF4E8(bank + 1) - 1;
    for (; i <= last; i++) {
        func_003CEEC8(i, bank, data);
    }
}

s32 Sprite_LoadBank(s32 bank, u64 *dest, s32 size) {
    s32 r = func_0035D048(bank, (u32)dest);

    Sprite_UploadBank(bank, dest, size, r);
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_005206B8);

void func_003CF1C0(s32 a, s32 b, s32 x, s32 y) {
    Sprite_Draw(a, D_005206B8[b].unk0, 0x4C, x, y);
}

void func_003CF1E8(s32 i, s32 x, s32 y) {
    if (i < 0x27) {
        if (i >= 0) {
            Sprite_Draw(1, D_005206B8[i].unk0, 0x4C, x - D_005206B8[i].unk4, y);
        }
    }
}

s32 func_003CF230(s32 chr, s32 costume) {
    if (chr == -1) {
        return -1;
    }
    if ((u32)chr >= 0x22) {
        return -1;
    }
    return D_00415840[chr * 8 + costume]->unk4;
}

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_005207F0);

/* NON_MATCHING: the original tests `mask & (1 << k)` with lb + sllv; plain C becomes srav / andi and the (s8) cast here gives lbu, 38 lines */
#if 0
s32 func_003CF278(u8 c0, u8 k0, u8 c1, u8 k1, s32 kind, s32 n) {
    s32 i;
    const S2bPairRule *e;
    for (i = 0; i < 0x4C; i++) {
        e = &D_005207F0[i];
        if (e->chr0 == c0 || e->chr0 == 0x22) {
            if (e->mask0 & (s8)(1 << k0)) {
                if (e->chr1 == c1 || e->chr1 == 0x22) {
                    if (e->mask1 & (s8)(1 << k1)) {
                        if (e->op == -1) {
                kind -= n;
            } else if (e->op >= 0) {
                if (e->op == 1) {
                    kind += n;
                }
            } else if (e->op == -9) {
                kind = 0;
            }
                    }
                }
            }
        }
    }
    return kind;
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CF278);
#endif

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520970);

u32 Num_ToBcd(u32 v) {
    u32 r;
    const u32 *p;
    s32 sh;

    if (v > 100000000) {
        r = 0x99999999;
    } else {
        r = 0;
        sh = 28;
        for (p = D_00520970; *p != 0; p++, sh -= 4) {
            if (v >= *p) {
                r |= (v / *p) << sh;
                v %= *p;
            }
        }
        r += v;
    }
    return r;
}

s32 func_003CF3D8(u32 v) {
    s32 n = 0;

    while (v != 0) {
        n++;
        v >>= 4;
    }
    return n;
}


void func_003CF408(f32 *v, f32 angle) {
    f32 len = S2bSqrtS(v[0] * v[0] + v[1] * v[1]); /* fake: S2bSqrtS */

    angle *= 3.14f;
    v[0] = cosf(angle) * len;
    v[1] = sinf(angle) * len;
}

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520990);

s32 func_003CF488(s32 v) {
    s32 i;

    for (i = 0; i < 0x35; i++) {
        if (v < D_00520990[i]) {
            return i - 1;
        }
    }
    return 0;
}

s32 func_003CF4C8(s32 a, s32 i) {
    return a - D_00520990[i];
}

s32 func_003CF4E8(s32 i) {
    return D_00520990[i];
}

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520A68);

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520A70);

/* NON_MATCHING: which loop invariant stays outside a register (the original rebuilds &D_00520A70 in the loop, this one &D_00444310 + 4), 35 lines */
#if 0
void func_003CF500(void) {
    s32 i;
    s32 v;

    for (i = 0; i < 10; i++) {
        v = D_00520A70[i].unk0;
        if (v != 0) {
            D_00444310[i].unk0 = D_00520A68[i].unk0;
            D_00444310[i].unk4 = v;
            D_00444310[i].unk8 = D_00520A68[i].unkC;
            D_00444310[i].unkC = D_00520A68[i].unk10;
            D_00444310[i].unk10 = D_00520A68[i].unk14;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CF500);
#endif

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520B58);

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520BB8);

/* NON_MATCHING: t0 / t1 swapped for the copied table value and the cols address in the loop, 4 lines */
#if 0
void func_003CF608(s32 set) {
    S2bSetCols cols = D_00520B58;
    S2bSetPos pos = D_00520BB8;
    s32 x;
    s32 i;
    s32 j;

    x = pos.v[set][0];
    D_005F6730 = pos.v[set][1];
    for (i = 0, j = 0; i < 2; i++, j++) {
        D_00444310[i].unk0 = x;
        D_00444310[i].unk4 = D_00520A70[i].unk0;
        D_00444310[i].unk8 = D_00520A70[i].unk4;
        D_00444310[i].unkC = 0x280;
        D_00444310[i].unk10 = cols.v[set][j + 2];
        x += cols.v[set][j];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CF608);
#endif

LIT4_WORD(D_005F5368, 0x3DD65B7B);

/* NON_MATCHING: the radius copy (f20 / f22) is coalesced and the stack slots of z / i / px come out in another order, 31 lines */
#if 0
void func_003CF850(s32 cx, s32 cy, s32 r) {
    s32 z = D_005F6730;
    f32 a = 0.0f;
    f32 fr = r;
    f32 rr;
    f32 step;
    s32 px, py, nx, ny;
    s32 x0p, y0p, x0m, y0m, x1p, y1p, x1m, y1m;
    s32 i;

    D_005F6730 = 0xFFF00;
    rr = fr;
    px = cosf(a) * fr;
    step = 0.10466667f;
    py = sinf(a) * fr;
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    D_005F6738 = 0x80000000;
    D_005F6740 = 0x44;
    for (i = 0; i < 15; i++) {
        a += step;
        y0p = py + cy;
        x0p = px + cx;
        y0m = cy - py;
        x0m = cx - px;
        nx = cosf(a) * rr;
        ny = sinf(a) * rr;
        x1p = nx + cx;
        x1m = cx - nx;
        y1p = ny + cy;
        y1m = cy - ny;
        func_003CE5D8(x0p, y0p, 0x280, y0p, x1p, y1p, 0x280, y1p, 0);
        func_003CE5D8(x0p, y0m, 0x280, y0m, x1p, y1m, 0x280, y1m, 0);
        func_003CE5D8(x0m, y0p, 0, y0p, x1m, y1p, 0, y1p, 0);
        func_003CE5D8(x0m, y0m, 0, y0m, x1m, y1m, 0, y1m, 0);
        px = nx;
        py = ny;
    }
    func_003CE5D8(0, 0, 0x280, 0, 0, y1m, 0x280, y1m, 0);
    func_003CE5D8(0, y1p, 0x280, y1p, 0, 0x1C0, 0x280, 0x1C0, 0);
    D_005F6730 = z;
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CF850);
#endif

LIT4_WORD(D_005F536C, 0x3DD65B7B);

/* NON_MATCHING: first draft (ellipse iris: triangle fans plus four covering quads), float temporaries and stack slots differ, 161 lines */
#if 0
void func_003CFAD8(s32 r, s32 pct, s32 alpha) {
    s32 yb = r + 0xE0;
    s32 yt = 0xE0 - r;
    s32 w = r * pct / 100;
    s32 z = D_005F6730;
    f32 a = 0.0f;
    f32 fr = r;
    f32 fpct = pct;
    f32 hundred;
    f32 step;
    s32 px, py, nx, ny;
    s32 xl, xr;
    s32 i;

    D_005F6730 = 0xFFF00;
    hundred = 100.0f;
    step = 0.10466667f;
    px = cosf(a) * fr * fpct / hundred;
    py = sinf(a) * fr;
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    D_005F6738 = 0x80000000;
    xr = w + 0x140;
    xl = 0x140 - w;
    for (i = 0; i < 15; i++) {
        a += step;
        nx = cosf(a) * fr * fpct / hundred;
        ny = sinf(a) * fr;
        func_003CE920(px + 0x140, py + 0xE0, xr, yb, nx + 0x140, ny + 0xE0, alpha);
        func_003CE920(px + 0x140, 0xE0 - py, xr, yt, nx + 0x140, 0xE0 - ny, alpha);
        func_003CE920(0x140 - px, py + 0xE0, xl, yb, 0x140 - nx, ny + 0xE0, alpha);
        func_003CE920(0x140 - px, 0xE0 - py, xl, yt, 0x140 - nx, 0xE0 - ny, alpha);
        px = nx;
        py = ny;
    }
    D_005F6740 = 0x44;
    func_003CE5D8(0, 0, 0x280, 0, 0, yt + 1, 0x280, yt + 1, 0);
    func_003CE5D8(xr, 0, 0x280, 0, xr, 0x1C0, 0x280, 0x1C0, 0);
    func_003CE5D8(0, 0, xl, 0, 0, 0x1C0, xl, 0x1C0, 0);
    func_003CE5D8(0, yb, 0x280, yb, 0, 0x1C0, 0x280, 0x1C0, 0);
    D_005F6730 = z;
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CFAD8);
#endif

/* NON_MATCHING: first draft (three gradient quads of a window bevel), colour expressions not yet shaped, 324 lines */
#if 0
void func_003CFE38(s32 x, s32 y, s32 w, s32 h, u8 *c) {
    s32 xl = x - w;
    s32 q = h / 4;
    s32 q3 = h * 3 / 4;
    u64 a = (u64)(c[0] * 0x60 / 128) | 0x60000000 | ((u64)(c[2] * 0x60 / 128) << 16) | ((u64)(c[1] * 0x60 / 128) << 8);
    u64 b = (u64)(c[3] * 0x60 / 128) | 0x60000000 | ((u64)(c[5] * 0x60 / 128) << 16) | ((u64)(c[4] * 0x60 / 128) << 8);
    u64 d0 = (u64)(c[0] * 0x70 / 128) | 0x80000000 | ((u64)(c[2] * 0x70 / 128) << 16) | ((u64)(c[1] * 0x70 / 128) << 8);
    u64 d1 = (u64)(c[3] * 0x70 / 128) | 0x80000000 | ((u64)(c[5] * 0x70 / 128) << 16) | ((u64)(c[4] * 0x70 / 128) << 8);
    u64 e = c[0] | 0x80000000 | ((u64)c[2] << 16) | ((u64)c[1] << 8);

    func_003CE770(xl, y, a, x, y, b, xl + q, y + q, a, x + q, y + q, b);
    func_003CE770(xl + q, y + q, d0, x + q, y + q, d1, xl + q, y + h, d0, x + q, y + h, d1);
    func_003CE770(xl, y, e, xl + q, y + q, e, xl, y + q3, e, xl + q, y + h, e);
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", func_003CFE38);
#endif

void func_003D01C0(u8 *dst, u32 i) {
    s32 k;
    u8 *s;

    if (i < 0x27) {
        s = func_0035C6F0(i);
        for (k = 0; k < 3; k++) {
            *dst++ = *s++;
        }
    }
    *dst = 0;
}

s32 func_003D0220(u8 *s) {
    u32 i;

    for (i = 0; i < 6; i++) {
        if (Str_ComparePrefix(D_004443D8[i], s) == 0) {
            return 1;
        }
    }
    return 0;
}

s32 func_003D0290(S2bName *p) {
    s32 n;

    for (n = p->len; n < 3; n++) {
        p->str[n] = ' ';
    }
    p->str[3] = 0;
    p->len = 3;
    if (func_003D0220(p->str)) {
        func_003D01C0(p->str, p->unkA);
    }
    return 1;
}

void NameEntry_Init(S2bName *p, u16 a, u16 b, u16 c) {
    p->unkE = a;
    p->unkA = b;
    p->unkC = c;
    Str_Copy(p->str, D_005F6778);
    p->len = 0;
    p->unk8 = 0;
    p->unk6 = 0;
    p->unk7 = 0;
}

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520BE8);

/* NON_MATCHING: first draft of the name-entry step (switch layout and the pad direction sum), 43 lines */
#if 0
s32 NameEntry_Step(S2bName *p, s32 mode) {
    u16 held;
    u16 trig;
    s32 d;
    u8 c;

    p->unk6 = 0;
    p->unk7 = 0;
    switch (mode) {
    case 1:
        return func_003D0290(p);
    case 0:
        held = (u16)D_00415738[p->pad1] | (u16)D_00415738[p->pad0];
        trig = (u16)D_00415728[p->pad1] | (u16)D_00415728[p->pad0];
        d = ((held >> 13) & 1) - (held >> 15) + ((held >> 14) & 1);
        if (held & 0x1000) {
            d--;
        }
        if (d != 0) {
            p->unk7 = 1;
        }
        p->unk8 = func_0035C508(0, p->unk8 + d, 0x2A);
        if (p->len < 3) {
            c = D_00520BE8[p->unk8];
        } else {
            c = 0;
        }
        p->str[p->len] = c;
        if (trig & 0xF0) {
            p->unk6 = 1;
            switch (c) {
            case 0x3D:
                return func_003D0290(p);
            case 0x3C:
                if (p->len != 0) {
                    p->str[p->len] = ' ';
                    p->len--;
                }
                break;
            default:
                p->str[p->len] = c;
                p->len++;
                if (p->len >= 3) {
                    return func_003D0290(p);
                }
                break;
            }
        }
        return 0;
    case 2:
        func_003D01C0(p->str, p->unkA);
        p->len = 3;
        return 1;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/sprite2d", NameEntry_Step);
#endif

void Fighter_ClearLife(s32 side) {
    gFighters[side].unk3F4 = 0;
}

INCLUDE_RODATA("asm/nonmatchings/sprite2d", D_00520C18);

void func_003D0548(s32 x, s32 y, s32 kind, s32 alpha) {
    s32 z = D_005F6730;
    u32 size;

    D_005F6730 = 0xFFFF0;
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 0);
    size = 0x40;
    if (kind == 0) {
        size = 0x60;
    }
    D_005F6738 = (u64)alpha | ((u64)alpha << 8) | ((u64)alpha << 16) | ((u64)alpha << 24);
    Sprite_DrawScaledBlended(1, D_00520C18[kind].unk0, D_00520C18[kind].unk4, x - size / 2, y - size / 2, size, size);
    D_005F6730 = z;
}

void func_003D0620(void) {
    func_003CF608(2);
    func_003637D0();
    Sprite_LoadBank(9, func_003639E0(), 0x249F0);
    func_003638F0();
}
