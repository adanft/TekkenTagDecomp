/* 0x340000-0x341B28 (module render_fighter_a; docs/systems/render.md section 12): the stage renderer, formerly
 * src/340000.c: the stage scene D_005F6780 (depth merge sort, level of detail), the stage draw func_00340E28 and the
 * per-frame scene update func_00341AE8 that Battle_UpdateFrame calls. One object [V: its .lit4 pool 0x5F5378-0x5F539C,
 * a subsegment; LIT4_WORD lines for the functions still in assembly]; the end is [I]. Every NON_MATCHING draft (#if 0)
 * compiles when enabled. */
#include "common.h"

/* A key of the depth sort (func_00340BF0). */
typedef struct {
    /* 0x0 */ f32 key;
    /* 0x4 */ s32 index;
} SortKey;

/* The merge sort's indexes (file statics of the original [I]). */
extern s32 D_005F6F1C;
extern s32 D_005F6F20;
extern s32 D_005F6F24;
extern s32 D_005F6F28;

/* One drawn entry of the scene (0x150 bytes). */
typedef struct {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ u8 pad4[0x14C];
} SceneEnt;

/* The scene behind D_005F6780: the camera position at 0x14, a vector at 0x40, the number of
 * sorted entries, the entries and the two key arrays of the depth sort [V offsets; I roles]. */
typedef struct {
    /* 0x00000 */ s16 unk0;
    /* 0x00002 */ s16 unk2;
    /* 0x00004 */ s16 levels;
    /* 0x00006 */ u8 pad6[0xE];
    /* 0x00014 */ f32 x;
    /* 0x00018 */ f32 y;
    /* 0x0001C */ f32 z;
    /* 0x00020 */ u8 pad20[0x10];
    /* 0x00030 */ f32 thr[4];
    /* 0x00040 */ u128 unk40;
    /* 0x00050 */ u8 pad50[0x2A4];
    /* 0x002F4 */ s32 count;
    /* 0x002F8 */ u8 pad2F8[0x150];
    /* 0x00448 */ SceneEnt ent[256];
    /* 0x15448 */ u8 pad15448[0x3EB8];
    /* 0x19300 */ SortKey keys[256];
    /* 0x19B00 */ SortKey tmp[256];
} Scene;

extern Scene *D_005F6780;

void func_00340BF0(SortKey *keys, SortKey *tmp, s32 lo, s32 hi);

/* Table of 0x30-byte records at 0x521700 (D_00521700), indexed by id. Only the fields used so far are named. */
typedef struct {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ u8 pad6[0xE];
    /* 0x14 */ f32 unk14;
    /* 0x18 */ u8 pad18[0x18];
} Unk521700;

extern Unk521700 D_00521700[];

void func_00357258(f32 *values, s32 count);
float sqrtf(float x);
void func_0035B4B0(void *a, void *b);
void func_00366ED0(void);
void func_00341218(void);
void func_003D1040(void);
void Wind_Update(void);
extern u8 D_00415808[];
extern u8 D_00436530[];

/* When record `id` has unk4 == 1, passes 27 copies of its unk14 to func_00357258. */
void func_00340000(s32 id) {
    f32 values[27];
    s32 i;
    Unk521700 *rec = &D_00521700[id];

    if (rec->unk4 == 1) {
        for (i = 0; i < 27; i++) {
            values[i] = rec->unk14;
        }
        func_00357258(values, 27);
    }
}

LIT4_WORD(D_005F5378, 0x477FFF00);
LIT4_WORD(D_005F537C, 0x47435000);
LIT4_WORD(D_005F5380, 0x477FFF00);
LIT4_WORD(D_005F5384, 0x459C4000);
LIT4_WORD(D_005F5388, 0x461C4000);
LIT4_WORD(D_005F538C, 0x464B2000);
INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_00340070);

s16 func_00340398(s32 id) {
    return D_00521700[id].unk4;
}

s16 func_003403B8(void) {
    return D_005F6780->unk0;
}

/* NON_MATCHING: 8 lines: load and store order of the camera words */
#if 0
void func_003403C8(f32 *v) {
    Scene *s = D_005F6780;
    f32 y = s->y;

    v[3] = 1.0f;
    v[0] = s->x;
    v[1] = s->z;
    v[2] = y;
}
#else
INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_003403C8);
#endif

void func_003403F8(u128 *v) {
    *v = D_005F6780->unk40;
}

/* Level 1-5 of a distance: the mean of the positive w of four vectors (65535 when none), written to *out, against the
 * scene's thresholds thr[0..3] (only `levels` of them used). */
s32 func_00340408(f32 (*v)[4], f32 *out) {
    f32 n = 0.0f;
    f32 sum;
    f32 d = 65535.0f;
    s32 i;

    sum = n;
    for (i = 0; i < 4; i++) {
        if (0.0f < v[i][3]) {
            sum += v[i][3];
            n += 1.0f;
        }
    }
    if (n != 0.0f) {
        *out = sum / n;
        d = *out;
    }
    if (D_005F6780->levels >= 6 && D_005F6780->thr[3] < d) {
        return 5;
    }
    if (D_005F6780->levels >= 5 && D_005F6780->thr[2] < d) {
        return 4;
    }
    if (D_005F6780->levels >= 4 && D_005F6780->thr[1] < d) {
        return 3;
    }
    if (D_005F6780->levels >= 3 && D_005F6780->thr[0] < d) {
        return 2;
    }
    return 1;
}

s32 func_00340528(f32 x, f32 a, f32 b) {
    if (b < a) {
        return x <= a && b <= x;
    }
    return a <= x && x <= b;
}

INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_00340588);

INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_003408B0);

/* 1 when the point (x1, y1) is within `radius` of (x0, y0). */
s32 func_00340AF0(f32 x0, f32 y0, f32 radius, f32 x1, f32 y1) {
    return sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) <= radius;
}

/* NON_MATCHING: 12 lines: dir[0] / dir[1] take each other's registers and the 1.0f store moves */
#if 0
void func_00340B30(f32 *dir, f32 *p, f32 *out) {
    out[1] = 0.0f;
    out[3] = 1.0f;
    out[0] = p[0] + -p[1] * dir[0] / dir[1];
    out[2] = p[2] + -p[1] * dir[2] / dir[1];
}
#else
INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_00340B30);
#endif

/* NON_MATCHING: 4 lines: the original keeps a copy of the scene pointer and the 0x448 offset in the store */
#if 0
void func_00340BA0(void) {
    s32 i;

    D_005F6780->count = 0;
    for (i = 0; i < 256; i++) {
        D_005F6780->keys[i].index = i;
        D_005F6780->ent[i].unk0 = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_00340BA0);
#endif

/* Merge sort of keys[lo..hi], largest key first, through tmp. */
void func_00340BF0(SortKey *keys, SortKey *tmp, s32 lo, s32 hi) {
    s32 mid;

    if (lo < hi) {
        mid = (lo + hi) >> 1;
        func_00340BF0(keys, tmp, lo, mid);
        func_00340BF0(keys, tmp, mid + 1, hi);
        D_005F6F28 = 0;
        for (D_005F6F1C = lo; D_005F6F1C <= mid; D_005F6F1C++) {
            tmp[D_005F6F28++] = keys[D_005F6F1C];
        }
        D_005F6F24 = lo;
        D_005F6F1C = mid + 1;
        D_005F6F20 = 0;
        while (D_005F6F1C <= hi && D_005F6F20 < D_005F6F28) {
            if (tmp[D_005F6F20].key >= keys[D_005F6F1C].key) {
                keys[D_005F6F24++] = tmp[D_005F6F20++];
            } else {
                keys[D_005F6F24++] = keys[D_005F6F1C++];
            }
        }
        while (D_005F6F20 < D_005F6F28) {
            keys[D_005F6F24++] = tmp[D_005F6F20++];
        }
    }
}

void func_00340DF8(void) {
    func_00340BF0(D_005F6780->keys, D_005F6780->tmp, 0, D_005F6780->count - 1);
}

INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_00340E28);

LIT4_WORD(D_005F5394, 0x40555555);
LIT4_WORD(D_005F5398, 0x39AEC33E);
INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_00341218);

void func_00341AE8(void) {
    func_0035B4B0(D_00415808, D_00436530);
    func_00366ED0();
    func_00341218();
    func_003D1040();
    Wind_Update();
}
