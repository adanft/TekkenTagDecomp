/* 0x356A70-0x3572A0 (module draw_list; docs/systems/render.md section 12.4): no draw list, the stage model banks
 * relocated in place and GS image uploads. The end is the object edge before crt0 (padding at the end of the file).
 * Every NON_MATCHING draft (#if 0) compiles when enabled. */
#include "common.h"

/* A relocatable table in a loaded model file: a count and the file offset of its entries, turned into pointers in
 * place (entry offsets are relative to the table). */
typedef struct {
    /* 0x0 */ u32 count;
    /* 0x4 */ void **items;
} MdlTable;

/* One loaded model bank (0x70 bytes): its root table and the per-entry sub-tables. */
typedef struct {
    /* 0x00 */ MdlTable *root;
    /* 0x04 */ MdlTable *sub[27];
} MdlSlot;

extern s32 D_005F7048;
extern s8 D_005F7044;
extern MdlSlot D_01FC4680[];
extern u8 D_01FC4840[];
extern u8 D_01FC4590[];
extern s32 D_01FC4580[];
extern f32 D_01FC47D0[];
extern s8 D_00521FF0[];

void sceGifPkInit(void *pk, void *base);
void sceGifPkEnd(void *pk, u32 a, u32 b, u32 c);
void sceGifPkTerminate(void *pk);
void FlushCache(s32 op);
void func_0035E6B8(void *tag);
u32 func_00354290(void *pk, void *rec, u32 base);
s32 func_003678A8(void);
s32 func_003678B8(void);
void func_003E13C8(f32 x);
s32 func_00340398(void);
void func_00340000(s32 id);
s32 func_00356A78(void *p, s32 a, s32 b);
MdlSlot *func_00356E38(s32 i);
s32 func_00356F48(s32 x);
void func_00356F68(MdlSlot *s, u32 i, MdlTable *t, s32 a, s32 kind);
void func_00357028(MdlSlot *s, MdlTable *t, s32 a, s32 *kinds);

s32 func_00356A70(void) {
    return D_005F7048;
}

INCLUDE_ASM("asm/nonmatchings/draw_list", func_00356A78);

/* Uploads a chain of GS image records now through a GIF packet built in `buf`; returns its size in 256-byte blocks. */
u32 func_00356D78(void *rec, u32 base, void *buf) {
    u128 pk;
    u32 r;

    sceGifPkInit(&pk, buf);
    r = func_00354290(&pk, rec, base);
    sceGifPkEnd(&pk, 0, 0, 0);
    sceGifPkTerminate(&pk);
    FlushCache(0);
    func_0035E6B8(buf);
    return r;
}

u32 func_00356E00(void *rec, u32 base) {
    return func_00356D78(rec, base, D_01FC4840);
}

void func_00356E20(s32 x, void *p, s32 a) {
    D_005F7044 = x;
    func_00356A78(p, a, 0);
}

MdlSlot *func_00356E38(s32 i) {
    return &D_01FC4680[i];
}

s32 func_00356E50(s32 i) {
    MdlSlot *s = NULL;

    if (func_003678B8() != 0 && func_003678A8() != 0) {
        s = func_00356E38(D_00521FF0[i]);
    } else if (func_003678B8() != 0) {
        s = func_00356E38(1);
    } else if (func_003678A8() != 0) {
        s = func_00356E38(0);
    }
    return *(s32 *)s;
}

s32 func_00356EE8(s32 side) {
    switch (side) {
    case 0:
        if (func_003678A8() != 0) {
            return 1;
        }
        break;
    case 1:
        if (func_003678B8() != 0) {
            return 1;
        }
        break;
    }
    return 0;
}

/* NON_MATCHING: 7 lines: the original tests x ^ 1 with movn into the return register */
#if 0
s32 func_00356F48(s32 x) {
    if (x == 0) {
        return 2;
    }
    return (x == 1) ? 3 : 2;
}
#else
INCLUDE_ASM("asm/nonmatchings/draw_list", func_00356F48);
#endif

void func_00356F68(MdlSlot *s, u32 i, MdlTable *t, s32 a, s32 kind) {
    u32 n;
    u32 k;
    void **items;
    u8 *p;

    s->sub[i] = t;
    if (t != NULL) {
        n = t->count;
        t->items = (void **)((u8 *)t->items + (u32)t);
        items = s->sub[i]->items;
        for (k = 0; k < n; k++) {
            p = items[k];
            if (p != NULL) {
                items[k] = p + (u32)t;
                func_00356A78(p + (u32)t, a, kind);
            }
        }
    }
}

void func_00357028(MdlSlot *s, MdlTable *t, s32 a, s32 *kinds) {
    u32 n;
    u32 k;
    void **items;

    s->root = t;
    n = t->count;
    t->items = (void **)((u8 *)t->items + (u32)t);
    items = s->root->items;
    for (k = 0; k < n; k++) {
        if (items[k] != NULL) {
            items[k] = (u8 *)items[k] + (u32)t;
        }
        if (kinds[k] == 3) {
            func_003E13C8(D_01FC47D0[k]);
        }
        func_00356F68(s, k, items[k], a, kinds[k]);
    }
}

u32 func_00357128(s32 i, void *rec, u32 base) {
    return func_00356D78(rec, base, D_01FC4590 + i * 0x50);
}

/* NON_MATCHING: 16 lines: id / slot / value take s2 / s0 / s1 here, s0 / s1 / s2 in the original */
#if 0
void func_00357160(s32 id, s32 slot, MdlTable *t, s32 a) {
    s32 kinds[27];
    s32 k;
    s32 i;

    kinds[0] = 5;
    k = func_00356F48(func_00340398());
    for (i = 1; i < 27; i++) {
        kinds[i] = k;
    }
    if (k == 3) {
        func_00340000(id);
    }
    D_01FC4580[slot] = a;
    func_00357028(func_00356E38(slot), t, a, kinds);
}
#else
INCLUDE_ASM("asm/nonmatchings/draw_list", func_00357160);
#endif

void func_00357230(s32 slot, MdlSlot *s, MdlTable *t, s32 a, s32 *kinds) {
    D_01FC4580[slot] = a;
    func_00357028(s, t, a, kinds);
}

/* Copies `count` floats into D_01FC47D0 (per-entry values, func_00340000). */
void func_00357258(f32 *values, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        D_01FC47D0[i] = values[i];
    }
}

/* crt0 (_start, 0x3572A0) starts on a 16-byte boundary: the nops before it are padding, not part of the last function
 * (the generic split chunk carried them before this file existed). */
__asm__(".section .text\n    .balign 16\n");
