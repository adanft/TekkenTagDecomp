/* 0x35C2B0-0x35D048: random numbers and small helpers (character tables, counters, file loads by number).
 * docs/game_overview.md section 6. */
#include "common.h"
#include "fighter.h"

/* A character's play counters (D_00415D60.rec). */
typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u16 unk6;
} Unk415D66;

typedef struct {
    /* 0x00 */ u16 unk0[3];
    /* 0x06 */ Unk415D66 rec[34];
} Unk415D60;

/* An entry of the character table D_00415840. */
typedef struct {
    /* 0x0 */ u8 unk0[4];
    /* 0x4 */ u8 unk4;
    /* 0x5 */ u8 unk5;
    /* 0x6 */ u8 unk6;
    /* 0x7 */ u8 unk7;
    /* 0x8 */ u8 unk8;
} Unk415840;


typedef struct {
    /* 0x00 */ u8 unk0[0x14];
    /* 0x14 */ s32 unk14[3];
} Unk436530;

extern s16 *D_005F6B2C;
extern s16 D_005F6B30;
extern s32 gRngSpin;
extern s32 D_005F5CDC;
extern s32 D_005F5CF4;
extern s16 gMode;
extern s16 D_004FBB38[];
extern s16 *D_00415830[];
extern s32 D_00415808[];
extern Unk436530 D_00436530;
extern s32 *D_00415CC0[];
extern Unk415840 *D_00415840[];
extern Unk415D60 D_00415D60;
extern u8 D_0043B968[];
extern u8 D_0043B990[];
extern u8 D_0043B9B8[];
extern u8 D_0043B9E0[];
extern u8 D_0051E850[];
extern u8 D_0051E878[];
extern u8 D_0051E8A0[];
extern u8 D_0051E918[];
extern u8 D_0051E940[];

void func_003BB1F8(s32 i);
s32 Rng_GetSeed(void);
void Rng_SetSeed(s32 seed);
void srand(u32 seed);
s32 Gfx_GetVSyncCount(void);
void Gte_ApplyMatrixXyz(Unk436530 *a, s32 x, s32 y, s32 z, s32 *ox, s32 *oy, s32 *oz);
void Gte_SetRotMatrix(Unk436530 *a);
void Gte_SetTransMatrix(s32 *m);
void func_003ADA28(u8 *a, u8 *b, u32 *out);
s32 CharSet_Count(u32 *set);
s32 func_003ADA08(u32 *set, s32 i);
s32 func_003AE3C0(Fighter *f);
s16 func_003AD8E0(s16 x);
void func_00366380(Fighter *o);
void CharSet_Clear(void *set);
void func_003AD910(void *set, s32 i);
s32 func_003AD920(void *a, void *b);
void func_003AD960(void *a, void *b, void *c);
void CharSet_Copy(void *a, void *b);
void func_00361968(void);
s32 File_LoadSync(s32 id, u32 dest);
s32 File_Exists(s32 id);
s32 File_GetSize(s32 id);
s32 Rng_SpinNext(void);
u32 Math_Permille(u32 a, u32 b);
void Stat_Increment(u16 *p);
void func_0035CE00(void);
s32 func_0035CE40(s32 id, u32 dest);
s32 func_0035CE60(s32 id);
s32 func_0035CED8(s32 id);
Unk415840 *func_0035C710(s32 i);

void func_0035C2B0(void) {
    D_005F6B30 = 0;
    D_005F6B2C = D_004FBB38;
}

void func_0035C2C8(s16 i) {
    D_005F6B30 = 1;
    D_005F6B2C = D_00415830[i];
    func_003BB1F8(i);
}

void func_0035C2F8(s16 i) {
    D_005F6B30 = 1;
    D_005F6B2C = D_00415830[i];
}

/* Steps the s16 sequence started by func_0035C2C8 / func_0035C2F8 (-0x8000 ends it) into D_00415808. */
void func_0035C320(void) {
    s16 *p;
    s32 v;

    if (D_005F6B30 != 0) {
        p = D_005F6B2C;
        v = *p;
        D_005F6B2C = p + 1;
        if (v == -0x8000) {
            D_005F6B30 = 0;
            D_005F6B2C = D_004FBB38;
            v = 0;
        }
        D_00415808[0] += v;
    }
}

/* Once per frame outside fights: adds the VBlank count to the seed and reseeds rand. */
void Rng_Reseed(void) {
    if (D_005F5CDC != 0) {
        Rng_SetSeed(Rng_GetSeed() + Gfx_GetVSyncCount());
        srand(Rng_GetSeed());
    }
}

/* Boot: seeds rand with the Timer 0 count. */
void Rng_Init(void) {
    s32 seed = *(volatile s32 *)0x10000000;
#ifdef PORT
    seed = Port_BootSeed(seed); /* Timer 0 brought up to date, or TTT_SEED (port/src/plat_hw.c) */
#endif

    Rng_SetSeed(seed);
    srand(seed);
    D_005F5CF4 = seed;
}

void func_0035C400(s32 x, s32 y, s32 z) {
    s32 m[8];

    Gte_ApplyMatrixXyz(&D_00436530, x, y, z, &m[5], &m[6], &m[7]);
    m[5] += D_00436530.unk14[0];
    m[6] += D_00436530.unk14[1];
    m[7] += D_00436530.unk14[2];
    Gte_SetRotMatrix(&D_00436530);
    Gte_SetTransMatrix(m);
}

/* strcpy returning the end of the copy. */
u8 *Str_Copy(u8 *dst, const u8 *src) {
    u8 c;

    while ((c = *src++) != 0) {
        *dst++ = c;
    }
    *dst = 0;
    return dst;
}

/* -1 when `str` does not start with `prefix`, else 1 when `str` is longer, 0 when equal. */
s32 Str_ComparePrefix(u8 *str, u8 *prefix) {
    u32 c = *prefix;

    if (c != 0) {
        do {
            if (*str != c) {
                return -1;
            }
            prefix++;
            str++;
        } while ((c = *prefix) != 0);
    }
    return *str != *prefix;
}

/* `value` wrapped into [min, max]: below min gives max, above max gives min. */
s32 func_0035C508(s32 min, s32 value, s32 max) {
    if (value < min) {
        value = max;
    } else if (max < value) {
        value = min;
    }
    return value;
}

u8 *func_0035C528(u8 *dst, const u8 *src, s32 n) {
    s32 i;

    for (i = 0; i < n; i++) {
        *dst++ = *src++;
    }
    return dst;
}

/* a * 1000 / b, 1000 when a > b, 0 when b is 0. */
u32 Math_Permille(u32 a, u32 b) {
    if (a > b) {
        return 1000;
    }
    while (b > 0x3FFFFF) {
        b >>= 1;
        a >>= 1;
    }
    if (b == 0) {
        return 0;
    }
    return a * 1000 / b;
}

/* Sorts `n` {key, value} pairs by value, ascending (desc == 0) or descending. */
#if 0
/* NON_MATCHING: 13 lines: same layout; p / pv / qv take t1 / t2 / t0 where the original has t2 / t0 / t1 */
s32 Sort_PairsByValue(u32 *a, s32 n, s32 desc) {
    u32 *p;
    u32 *q;
    u32 *r;
    u32 tmp[2];
    u32 pv;
    u32 qv;
    s32 i;

    if (n < 2) {
        return 1;
    }
    n--;
    for (i = 0, r = a; i < n; i++, r += 2) {
        p = r;
        q = r + 2;
        do {
            pv = p[1];
            if (desc == 0) {
                qv = q[1];
                if (!(pv < qv)) {
                    break;
                }
            } else {
                qv = q[1];
                if (!(qv < pv)) {
                    break;
                }
            }
            tmp[1] = pv;
            tmp[0] = p[0];
            p[1] = qv;
            p[0] = q[0];
            q[1] = pv;
            q[0] = tmp[0];
            q -= 2;
            p -= 2;
        } while (q != a);
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/rng_misc", Sort_PairsByValue);
#endif

/* The second generator: x = x * 5 + 1, stepped while Gfx_WaitVSync spins. */
s32 Rng_SpinNext(void) {
    return gRngSpin = gRngSpin * 5 + 1;
}

/* Copies `n` bytes as whole words from the word containing `src` to the word containing `dst`. */
u8 *func_0035C698(u8 *dst, u8 *src, s32 n) {
    s32 *s = (s32 *)((s32)src & -4);
    s32 *d = (s32 *)((s32)dst & -4);

    n = (n + ((s32)src & 3) + 3) >> 2;
    while (n > 0) {
        n--;
        *d++ = *s++;
    }
    return dst + ((s32)dst & 3);
}

s32 func_0035C6F0(s32 i) {
    return *D_00415CC0[i];
}

Unk415840 *func_0035C710(s32 i) {
    if (i >= 0x118) {
        i = 0x110;
    }
    return D_00415840[i];
}

Unk415840 *func_0035C738(s32 character, s32 costume) {
    return func_0035C710(character << 3 | costume);
}

s32 func_0035C758(s32 character, s32 costume) {
    if (character >= 0x22 || costume >= 8) {
        character = 0x22;
        costume = 0;
    }
    return character << 3 | costume;
}

u8 func_0035C788(Fighter *o) {
    return D_00415840[o->unk26]->unk4;
}

/* Picks at random (spin generator) one of the 34 characters allowed by both `table` and D_0043B968. */
s32 Rng_PickRandomEntry(u8 *table) {
    u32 set[12];
    s32 i;
    s32 k;
    s32 pick;

    i = 0;
    func_003ADA28(D_0043B968, table, set);
    k = CharSet_Count(set);
    pick = (Rng_SpinNext() & 0xFFF) % k;
    k = 0;
    for (; i < 0x22; i++) {
        if (func_003ADA08(set, i)) {
            if (k == pick) {
                return i;
            }
            k++;
        }
    }
    return 0;
}

#if 0
/* NON_MATCHING: the fighter pointer and the flag are allocated to swapped saved registers (s0/s1). */
void func_0035C850(Fighter *o, s32 flag) {
    u8 *table;

    if ((u32)(func_003AE3C0(o) - 1) < 2 && flag != 0) {
        table = D_0051E940;
    } else if (o->unk2A == 15) {
        table = D_0051E8A0;
    } else {
        return;
    }
    o->unk28 = func_003AD8E0(Rng_PickRandomEntry(table) << 3);
}
#else
INCLUDE_ASM("asm/nonmatchings/rng_misc", func_0035C850);
#endif

void func_0035C8D0(Fighter *o, s32 character, s32 costume) {
    s32 id = character * 8 + (costume & 7);
    Unk415840 *c;
    u8 type;

    o->unk2A = character;
    o->unk26 = id;
    o->unk28 = func_003AD8E0(id);
    if (o->unk2A == 0x21) {
        o->unk28 = 0x1C;
    }
    c = D_00415840[o->unk26];
    type = c->unk6;
    o->unk22 = o->unk20 + o->unk0 * 2;
    o->unk2C = type;
    if (gMode == 5) {
        o->unk22 = o->unk20;
    }
    func_00366380(o);
}

PORT_RET(u8) func_0035C970(u32 i) {
    if (i >= 0x110) {
        i = 0;
    }
    return D_00415840[i]->unk8;
}

void func_0035C998(s32 character) {
    u32 set[12];

    if (character < 0x22) {
        CharSet_Clear(set);
        func_003AD910(set, character);
        if (func_003AD920(D_0051E878, set)) {
            func_003AD960(set, D_0043B968, D_0043B968);
        }
    }
}

void func_0035CA00(s32 character) {
    u32 set[12];

    if (character < 0x22) {
        CharSet_Clear(set);
        func_003AD910(set, character);
        if (func_003AD920(D_0051E878, set)) {
            func_003AD960(set, D_0043B990, D_0043B990);
        }
    }
}

void func_0035CA68(s32 character) {
    u32 set[12];

    if (character < 0x22) {
        CharSet_Clear(set);
        func_003AD910(set, character);
        if (func_003AD920(D_0051E878, set) && func_003AD920(D_0043B968, set)) {
            func_003AD960(set, D_0043B9E0, D_0043B9E0);
        }
    }
}

void func_0035CAE0(void) {
    CharSet_Copy(D_0051E918, D_0043B9B8);
}

void func_0035CAF8(void) {
    CharSet_Copy(D_0051E850, D_0043B968);
    CharSet_Copy(D_0051E850, D_0043B990);
    CharSet_Clear(D_0043B9E0);
    CharSet_Clear(D_0043B9B8);
}

/* Saturating u16 increment. */
void Stat_Increment(u16 *p) {
    s32 v = *p + 1;

    if (v > 0xFFFF) {
        v = 0xFFFF;
    }
    *p = v;
}

void func_0035CB70(s32 character) {
    if (character != -1) {
        Stat_Increment(&D_00415D60.rec[character % 34].unk0);
        func_00361968();
    }
}

void func_0035CBC0(s32 a, s32 b) {
    if (a != -1 && b != -1) {
        Stat_Increment(&D_00415D60.rec[a % 34].unk2);
        Stat_Increment(&D_00415D60.rec[b % 34].unk4);
        func_00361968();
    }
}

void func_0035CC68(s32 a, s32 b) {
    Stat_Increment(&D_00415D60.rec[a % 34].unk6);
    Stat_Increment(&D_00415D60.rec[b % 34].unk6);
    func_00361968();
}

void func_0035CCE0(s32 i) {
    u16 *p = D_00415D60.unk0;

    switch (i) {
    case 0:
        p[0]++;
        break;
    case 1:
        p[1]++;
        break;
    case 2:
        p[2]++;
        break;
    }
}

s32 func_0035CD48(s32 character) {
    s32 i = character % 34;

    return D_00415D60.rec[i].unk0 + D_00415D60.rec[i].unk4 + D_00415D60.rec[i].unk6;
}

s32 func_0035CD90(s32 character) {
    Unk415D66 *r = &D_00415D60.rec[character % 34];

    return (Math_Permille(r->unk2, r->unk2 + r->unk4) << 20) + r->unk2 + (r->unk6 + r->unk4);
}

void func_0035CE00(void) {
    Unk415D66 *r = D_00415D60.rec;
    s32 i;

    for (i = 0; i < 34; i++) {
        r->unk0 = 0;
        r->unk2 = 0;
        r->unk4 = 0;
        r->unk6 = 0;
        r++;
    }
}

void func_0035CE38(void) {
    func_0035CE00();
}

s32 func_0035CE40(s32 id, u32 dest) {
    return File_LoadSync(id, dest);
}

s32 func_0035CE60(s32 id) {
    return File_Exists(id);
}

s32 func_0035CE80(u32 dest, s32 n) {
    s32 id = n * 3 + 0x159;

    if (func_0035CE60(id)) {
        return func_0035CE40(id, dest);
    }
    return 0;
}

s32 func_0035CED8(s32 id) {
    return File_GetSize(id);
}

s32 func_0035CEF8(s32 n) {
    return func_0035CED8(n * 3 + 0x159);
}

s32 func_0035CF20(u32 dest, s32 n) {
    s32 id = n * 7 + 0xBD;

    if (func_0035CE60(id)) {
        return func_0035CE40(id, dest);
    }
    return 0;
}

s32 func_0035CF78(u32 dest, s32 n) {
    s32 id = n * 7 + 0xBC;

    if (func_0035CE60(id)) {
        return func_0035CE40(id, dest);
    }
    return 0;
}

s32 func_0035CFD0(u32 dest, s32 n) {
    s32 id;

    if (n == 1) {
        id = 0x2A;
    } else if ((u32)(n - 2) < 0x27) {
        id = (n - 2) * 2 + 0x30;
    } else if (n == 0x2A) {
        id = 0x80;
    } else {
        return 0;
    }
    return func_0035CE40(id, dest);
}

s32 func_0035D028(u32 dest) {
    return func_0035CE40(0x2B, dest);
}
