/* 0x3994C0-0x399FC8 (module zsort, docs/game_overview.md section 4). */
#include "common.h"
#include "gfx.h"

typedef f32 FVec4[4] __attribute__((aligned(16)));

typedef union {
    u128 q;
    u16 h[8];
    u32 w[4];
} ZsTag;

extern Unk8BD310 D_008BD310;
extern Unk8BD390 D_008BD390[];
extern Unk8BD390 D_008BEC90[];
extern s32 D_005F61E0;
extern s32 D_005F61E4;
extern f32 D_004334C0[4][4];
extern const char D_00518CB0[];
extern const char D_00518CD0[];
extern const char D_00518D08[];
extern const char D_00518D30[];
extern const char D_00518D58[];
extern const char D_00518D90[];
extern const char D_00518DB8[];
extern const char D_00518DC8[];
extern const char D_00518E00[];
extern const u16 D_00518E38[][8];

s32 printf(const char *fmt, ...);
void exit(s32 status) __attribute__((noreturn));
void FlushCache(s32 op);
u32 Spr_Setup(s32 count, s32 reserved);
ZsTag *func_00342FB0(void);
void func_00343178(void *addr, s32 qwc);
u32 func_003639B8(void);
u32 func_003639C0(void);
void func_003993B0(void);
void func_00399418(void);
void *Dma_GetPacket(s32 i);
void func_0035E6B8(void *tag);
void sceVu0CopyMatrix(void *dst, void *src);
void sceVu0RotTransPers(s32 *out, void *m, f32 *in, s32 mode);
void sceVu0ApplyMatrix(f32 *out, f32 (*m)[4], f32 *v);
void sceVu0DivVector(f32 *out, f32 *in, f32 q);

u32 func_003994C0(void);
u32 func_00399520(void);
void func_00399580(u32 p);
void func_00399650(u32 p);
void func_00399728(const void *addr, s32 n);
void func_00399820(const void *addr, s32 n);
void func_00399918(void);
void func_00399AF8(void);
void func_00399CC8(void);
void func_00399CE8(void *m);
s32 func_00399CF8(f32 *in, s32 *out, s32 clip);
s32 func_00399DC8(f32 *in, s32 *out);
void func_00399E50(u64 *p, s32 idx, s32 base);

u32 func_003994C0(void) {
    Unk8BD310 *d = &D_008BD310;

    if (d->unk78 == 0) {
        printf(D_00518CD0, d->unk50);
        exit(1);
    }
    if (d->unk60[d->unk50] != 0) {
        printf(D_00518D08, d->unk50);
        return 0;
    }
    return d->unk40;
}

u32 func_00399520(void) {
    Unk8BD310 *d = &D_008BD310;

    if (d->unk7C == 0) {
        printf(D_00518CD0, d->unk54);
        exit(1);
    }
    if (d->unk68[d->unk54] != 0) {
        printf(D_00518D08, d->unk54);
        return 0;
    }
    return d->unk48;
}

void func_00399580(u32 p) {
    Unk8BD310 *d = &D_008BD310;
    u32 base;

    if (d->unk78 == 0) {
        printf(D_00518CD0, d->unk50);
        exit(1);
    }
    base = func_003639B8() + (d->unk50 << 15);
    if (p >= base + 0x7000) {
        d->unk60[d->unk50] = 1;
        printf(D_00518D30, (s32)(p - base) >> 4, d->unk50);
        if (p >= base + 0x8000) {
            exit(1);
        }
    }
    D_008BD310.unk40 = p;
}

void func_00399650(u32 p) {
    Unk8BD310 *d = &D_008BD310;
    u32 base;

    if (d->unk7C == 0) {
        printf(D_00518D58, d->unk54);
        exit(1);
    }
    base = func_003639C0() + d->unk54 * 0x30000;
    if (p >= base + 0x2C000) {
        d->unk68[d->unk54] = 1;
        printf(D_00518D90, (s32)(p - base) >> 4, d->unk54);
        if (p >= base + 0x30000) {
            exit(1);
        }
    }
    D_008BD310.unk48 = p;
}

void func_00399728(const void *addr, s32 n) {
    Unk8BD310 *d = &D_008BD310;
    s32 i;
    u32 cur;
    s32 qwc;

    D_005F61E0 = 1;
    if (d->unk78 == 0) {
        printf(D_00518CD0, d->unk50);
        exit(1);
    }
    i = d->unk58;
    if (i >= 800) {
        printf(D_00518DB8);
        exit(1);
    }
    cur = d->unk40;
    qwc = (s32)(cur - d->unk44) >> 4;
    if (qwc <= 0) {
        D_008BD390[i].unk0 = (u32)addr;
        D_008BD390[i].unk4 = n;
        D_008BD390[i + 1].unk0 = cur;
        d->unk44 = cur;
        d->unk58 = i + 1;
    } else {
        D_008BD390[i].unk4 = qwc;
        D_008BD390[i + 1].unk0 = (u32)addr;
        D_008BD390[i + 1].unk4 = n;
        D_008BD390[i + 2].unk0 = cur;
        d->unk44 = cur;
        d->unk58 = i + 2;
    }
}

void func_00399820(const void *addr, s32 n) {
    Unk8BD310 *d = &D_008BD310;
    s32 i;
    u32 cur;
    s32 qwc;

    D_005F61E4 = 1;
    if (d->unk7C == 0) {
        printf(D_00518D58, d->unk54);
        exit(1);
    }
    i = d->unk5C;
    if (i >= 100) {
        printf(D_00518DB8);
        exit(1);
    }
    cur = d->unk48;
    qwc = (s32)(cur - d->unk4C) >> 4;
    if (qwc <= 0) {
        D_008BEC90[i].unk0 = (u32)addr;
        D_008BEC90[i].unk4 = n;
        D_008BEC90[i + 1].unk0 = cur;
        d->unk4C = cur;
        d->unk5C = i + 1;
    } else {
        D_008BEC90[i].unk4 = qwc;
        D_008BEC90[i + 1].unk0 = (u32)addr;
        D_008BEC90[i + 1].unk4 = n;
        D_008BEC90[i + 2].unk0 = cur;
        d->unk4C = cur;
        d->unk5C = i + 2;
    }
}

void func_00399918(void) {
    Unk8BD310 *d = &D_008BD310;
    s32 i;
    s32 n;
    s32 k;
    u32 addr;
    ZsTag *p;
    ZsTag *q;

    if (d->unk78 == 0) {
        if (D_005F61E0 != 0) {
            printf(D_00518DC8, d->unk50);
            exit(1);
        }
        return;
    }
    D_005F61E0 = 0;
    func_00399728(D_00518CB0, 2);
    i = 0;
    D_008BD390[d->unk58].unk4 = n = (s32)(d->unk40 - d->unk44) >> 4;
    d->unk58++;
    FlushCache(0);
    Spr_Setup(2, 0);
    for (; i < D_008BD310.unk58; i++) {
        n = D_008BD390[i].unk4;
        if (n > 0) {
            p = func_00342FB0();
            addr = D_008BD390[i].unk0;
            q = p;
            do {
                k = n;
                if (k > 0xFFFF) {
                    k = 0xFFFF;
                }
                q->w[1] = addr;
                addr += k * 16;
                q->h[1] = 0x3000;
                q->h[0] = k;
                q->w[2] = 0;
                n -= k;
                q->w[3] = 0;
                q++;
            } while (n != 0);
            func_00343178(p, q - p);
        }
    }
    D_008BD310.unk50 = 1 - D_008BD310.unk50;
    func_003993B0();
}

void func_00399AF8(void) {
    Unk8BD310 *d = &D_008BD310;
    s32 i;
    s32 n;
    s32 k;
    u32 addr;
    ZsTag *p;
    ZsTag *q;

    if (d->unk7C == 0) {
        if (D_005F61E4 != 0) {
            printf(D_00518E00, d->unk54);
            exit(1);
        }
        return;
    }
    n = (s32)(d->unk48 - d->unk4C) >> 4;
    D_005F61E4 = 0;
    if (n > 0) {
        D_008BEC90[d->unk5C++].unk4 = n;
        i = 0;
        FlushCache(0);
        Spr_Setup(2, 0);
        for (; i < D_008BD310.unk5C; i++) {
            n = D_008BEC90[i].unk4;
            if (n > 0) {
                p = func_00342FB0();
                addr = D_008BEC90[i].unk0;
                q = p;
                do {
                    k = n;
                    if (k > 0xFFFF) {
                        k = 0xFFFF;
                    }
                    q->w[1] = addr;
                    addr += k * 16;
                    q->h[1] = 0x3000;
                    q->h[0] = k;
                    q->w[2] = 0;
                    n -= k;
                    q->w[3] = 0;
                    q++;
                } while (n != 0);
                func_00343178(p, q - p);
            }
        }
    }
    D_008BD310.unk54 = 1 - D_008BD310.unk54;
    func_00399418();
}

void func_00399CC8(void) {
    func_0035E6B8(Dma_GetPacket(0));
}

void func_00399CE8(void *m) {
    sceVu0CopyMatrix(&D_008BD310, m);
}

s32 func_00399CF8(f32 *in, s32 *out, s32 clip) {
    s32 r = 0;

    sceVu0RotTransPers(out, &D_008BD310, in, 1);
    if (clip >= 0) {
        out[2] = 0xFFFF0;
    }
    if (out[0] > 0x6C00 && out[0] < 0x9400 && out[1] > 0x7200) {
        r = out[1] < 0x8E00;
    }
    if (out[0] < 0) {
        out[0] = 0;
    } else if (out[0] > 0xFFF0) {
        out[0] = 0xFFF0;
    }
    if (out[1] < 0) {
        out[1] = 0;
    } else if (out[1] > 0xFFF0) {
        out[1] = 0xFFF0;
    }
    return r;
}

s32 func_00399DC8(f32 *in, s32 *out) {
    FVec4 t;
    FVec4 u;

    sceVu0ApplyMatrix(t, D_004334C0, in);
    sceVu0DivVector(u, t, t[3]);
    out[3] = 0;
    out[0] = (s32)u[0] - 0x6C0;
    out[1] = ((s32)u[1] - 0x720) * 2;
    out[2] = u[2];
    return 0;
}

/* NON_MATCHING: 44 lines: register allocation; the original keeps the row end x/y and adds 8 later where gcc here folds x + e[6] - 8 + 8 */
#if 0
void func_00399E50(u64 *p, s32 idx, s32 base) {
    const u16 *e = D_00518E38[idx];
    s32 rows;
    s32 cols;
    s32 x, y, u, v;
    s32 x0, y0, u0, v0;
    s16 *h;

    *p++ = (u64)(e[0] * e[1]) | 0x8000 | ((u64)0xA400 << 48);
    *p++ = 0x5353053530;
    h = (s16 *)p;
    y = e[2];
    x = 8;
    for (rows = e[0]; rows > 0; rows--) {
        x0 = x;
        y0 = y;
        x = x + e[6] - 8;
        y = y + e[4] - 8;
        v = base << 4;
        u = e[3];
        for (cols = e[1]; cols > 0; cols--) {
            v0 = v;
            u0 = u;
            v += e[7];
            u += e[5];
            *(u64 *)&h[0] = 0x316;
            h[4] = x0;
            h[5] = v0;
            h[8] = y0;
            h[9] = u0;
            h[12] = x;
            h[13] = v;
            h[16] = y;
            h[17] = u;
            if (v0 == 0) {
                v0 = 0x10;
                u0 += 0x10;
            } else {
                v0 -= 0x10;
            }
            v -= 0x10;
            h[0x21] = v;
            *(u64 *)&h[20] = 0x356;
            v += 0x10;
            h[25] = v0;
            h[29] = u0;
            h[24] = x0;
            h[28] = y0;
            h[32] = x;
            h[36] = y;
            h[37] = u;
            h += 40;
        }
        x += 8;
        y += 8;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/zsort", func_00399E50);
#endif
