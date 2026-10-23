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
s32 func_00340398(s32 id);
void func_00340000(s32 id);
s32 func_00356A78(u8 *p, s32 a, s32 b);
void func_003E1330(void *p, s32 n);
void Light_SetupStage(s32 stage, void *p);
MdlSlot *func_00356E38(s32 i);
s32 func_00356F48(s32 x);
void func_00356F68(MdlSlot *s, u32 i, MdlTable *t, s32 a, s32 kind);
void func_00357028(MdlSlot *s, MdlTable *t, s32 a, s32 *kinds);

s32 func_00356A70(void) {
    return D_005F7048;
}

/* NON_MATCHING: 129 lines (first draft); its jump table jtbl_00522030 is draw_list's .rodata subsegment since T19 b2 (drop the
 * RODATA_ALIGN16 / INCLUDE_RODATA in the #else once the C emits it) */
#if 0
s32 func_00356A78(u8 *p, s32 base, s32 kind) {
    s32 size[12];
    s32 skip[12];
    u8 *start;
    s32 seg;
    s32 acc;
    s32 reg;
    s32 n;
    s32 i;
    s32 cnt;
    s32 m;
    s32 flag;
    u8 *q;
    u32 w0;
    u32 w1;
    u32 w2;
    u32 w3;

    seg = 0;
    start = (u8 *)(((u32)p + 0x7F) & ~0xF);
    acc = 0;
    switch (kind) {
    case 3:
        reg = 0x640;
        break;
    case 0:
    case 1:
    case 2:
    case 4:
    default:
        reg = 0x50;
        break;
    case 5:
        reg = 0x3A2;
        break;
    }
    n = *(s32 *)(p + 0x10);
    for (i = 0; i < n; i++) {
        size[i] = ((s32 *)(p + 0x14))[i * 2] << 4;
        skip[i] = ((s32 *)(p + 0x14))[i * 2 + 1];
    }
    cnt = *(s32 *)(p + 0x6C);
    q = (u8 *)(((u32)(p + 0x70) + 0xF) & ~0xF) + 0x24;
    for (; cnt >= 0; cnt--) {
        if (q - start == acc + size[seg] - skip[seg]) {
            acc += size[seg];
            q += skip[seg];
            seg++;
        }
        w0 = ((u32 *)q)[0x10];
        w1 = ((u32 *)q)[0x11];
        w2 = ((u32 *)q)[0x12];
        w3 = ((u32 *)q)[0x13];
        if (kind < 5) {
            w0 = (w0 & 0xFFFFC000) | (((w0 & 0x3FFF) + base) & 0x3FFF);
            w1 = (w1 & 0xFFF8001F) | (((((w1 >> 5) & 0x3FFF) + base) & 0x3FFF) << 5);
        } else if (kind == 5) {
            ((u32 *)q)[8] = 0x46;
            ((u32 *)q)[11] = 0x42;
            ((u32 *)q)[9] = 0;
            ((u32 *)q)[10] = 0x42;
            ((u32 *)q)[12] = kind;
            w0 = 0x20A13940;
            w1 = 0xE;
        }
        ((u32 *)q)[0x10] = w0;
        ((u32 *)q)[0x11] = w1;
        ((u32 *)q)[0x12] = w2;
        ((u32 *)q)[0x13] = w3;
        w0 = ((u32 *)q)[0x14];
        m = w0 & 0x7FFF;
        flag = (w0 & 0x8000) >> 15;
        w1 = ((u32 *)q)[0x15];
        w2 = ((u32 *)q)[0x16];
        w3 = ((u32 *)q)[0x17];
        if ((u32)kind < 6 && (u32)kind >= 4) {
            w1 = (w1 | 0x200000) & 0xFFBFFFFF;
        }
        ((u32 *)q)[0x14] = w0;
        ((u32 *)q)[0x15] = w1;
        ((u32 *)q)[0x16] = w2;
        ((u32 *)q)[0x17] = w3;
        q += 0x68;
        q += m * 4;
        q += 8;
        q += m * 8;
        q += 8;
        if (kind == 3) {
            func_003E1330(q, m);
        }
        q += m * 0x18;
        if (flag) {
            *(u32 *)q = reg | 0x15000000;
            q += 4;
        }
    }
    q = (u8 *)(((u32)q + 0xF) & ~0xF);
    if (kind == 0) {
        Light_SetupStage(D_005F7044, q);
        D_005F7048 = (s32)(q + 0xB0);
    }
}
#else
RODATA_ALIGN16();
INCLUDE_RODATA("asm/nonmatchings/draw_list", jtbl_00522030);
INCLUDE_ASM("asm/nonmatchings/draw_list", func_00356A78);
#endif

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
    k = func_00356F48(func_00340398(id)); /* $a0 still holds id at the PS2 call */
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
