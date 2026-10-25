/* 0x35D048-0x35E808: file loads by number, the scratchpad buffers (Spr_Setup) and transfers to and from the
 * scratchpad, the DMAC and SIF helpers. */
#include "common.h"

/* Table of file ids, read from the original executable. */
extern const s16 D_004FBF50[];
extern s32 D_005F6B38;
extern s32 D_005F6B3C;
extern s32 D_00415F80[];
extern s32 D_00415FE0[];
/* Fixed buffers in main RAM below the executable. */
extern u8 D_200000[];
extern u8 D_300000[];

/* The scratchpad header (0x70000000): up to three buffers of `size` bytes after 0x70000100 (Spr_Setup). */
typedef struct {
    /* 0x0 */ u32 buf[3];
    /* 0xC */ u16 size;
    /* 0xE */ u8 count;
    /* 0xF */ u8 unkF;
    /* 0x10 */ u32 unk10;
    /* 0x14 */ u32 unk14;
    /* 0x18 */ u32 unk18;
} SprHeader;

#define SPR ((SprHeader *)0x70000000)

/* State of the scratchpad streaming (gSprDma). `pending` counts the transfers still running; the DMA
 * interrupt handler lowers it. */
typedef struct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ volatile u8 pending;
    /* 0x02 */ u8 unk2;
    /* 0x03 */ u8 unk3;
    /* 0x04 */ u8 state;
    /* 0x08 */ u32 unk8;
    /* 0x0C */ u32 unkC;
    /* 0x10 */ u32 unk10;
    /* 0x14 */ u32 unk14;
    /* 0x18 */ u32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ void *unk20;
    /* 0x24 */ s32 unk24;
} SprDma;

extern volatile SprDma gSprDma;

typedef struct {
    /* 0x000 */ u8 pad0[0x100];
    /* 0x100 */ s32 unk100;
} Unk415E80;

extern Unk415E80 D_00415E80;

typedef struct {
    u64 d[4];
} SprBlock;

void exit(s32 status) __attribute__((noreturn));
s32 File_Request(s32 id, u32 dest, s32 flags, s32 toIop);
s32 File_Release(s32 handle);
s32 File_CountQueued(void);
s32 File_LoadSyncIop(s32 id, u32 dest);
s32 func_0035CE40(s32 id, u32 dest);
s32 func_0035CE60(s32 id);
s32 func_0035CED8(s32 id);
s32 func_0035D3A0(s32 id, u32 dest);
s32 sceOpen(const char *path, s32 flags);
s32 sceClose(s32 fd);
void Spr_WaitChannel(void);
void func_0035D8C8(void);
void func_0035D6E0(void);
void func_0035D738(void);
s32 func_0035E600(volatile u32 *chan);
u32 Dma_Wait(volatile u32 *chan);
s32 Dma_SendChain(volatile u32 *chan, void *tag);
void func_0035D958(volatile u32 *chan, s32 state);
void func_0035DA08(void);
void func_0035DEF0(void);
s32 Spr_WaitIdle(void);
s32 EnableDmac(s32 channel);
void Dma_SendNormal(volatile u32 *chan, void *addr, u32 qwc);
void Dma_AddRefTag(u8 **pkt, s32 i);
void func_00343178(void *addr, s32 qwc);
void func_0035DA70(void);
void func_0035DA78(void);
void func_0035DC20(void);
void func_0035DF68(volatile SprDma *s);
void func_0035DFA8(volatile SprDma *s);
s32 func_0035DFF0(volatile SprDma *s);
void func_0035E048(volatile SprDma *s);
void func_0035E100(volatile SprDma *s);
s32 func_0035E228(s32 channel);
s32 func_0035E2E8(s32 channel);
s32 iDisableDmac(s32 channel);
s32 AddDmacHandler(s32 channel, s32 (*handler)(s32), s32 next);
s32 DisableDmac(s32 channel);
void func_0035E3B0(void);

s32 func_0035D048(s32 n, u32 dest) {
    return func_0035CE40(D_004FBF50[n], dest);
}

void func_0035D078(void) {
    D_005F6B3C = 2;
    D_005F6B38 = -1;
}

void func_0035D090(void) {
    if (D_005F6B38 >= 0) {
        File_Release(D_005F6B38);
    }
    D_005F6B3C = 1;
    D_005F6B38 = -1;
}

s32 func_0035D0C8(s32 n, u32 dest) {
    if (D_005F6B3C != 2) {
        exit(1);
    }
    D_005F6B38 = File_Request(D_004FBF50[n], dest, 0, 0);
    D_005F6B3C = 3;
    return 1;
}

#if 0
/* NON_MATCHING: the store of state 4 lands in the delay slot of the branch to the return; the original sets the
 * return value there. */
s32 func_0035D120(s32 abort) {
    if (abort) {
        if (D_005F6B3C == 3 && D_005F6B38 >= 0) {
            File_Release(D_005F6B38);
        }
        return 1;
    }
    switch (D_005F6B3C) {
    case 3:
        if (File_CountQueued() > 0) {
            return 0;
        }
        File_Release(D_005F6B38);
        D_005F6B3C = 4;
        return 1;
    case 2:
        return 0;
    case 4:
        return 1;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035D120);
#endif

s32 func_0035D1C8(s32 n) {
    return func_0035CED8(D_004FBF50[n]);
}

s32 func_0035D1F8(u32 dest) {
    return func_0035CE40(0x85, dest);
}

s32 func_0035D218(u32 dest) {
    return func_0035CE40(0xC, dest);
}

#if 0
/* NON_MATCHING: 7 lines: the quotient of the division takes a0 (computed after li -1) where the original builds it in a2 first */
s32 func_0035D238(s32 n, u32 dest) {
    s32 m;

    if (n == 0) {
        n = 0x29;
    } else if (n < 0x40) {
        n *= 2;
        n += 0x2B;
    } else {
        m = n - 0x40;
        n = m / 4 * 7 + 0xB7 + (m & 3);
    }
    return func_0035CE40(n, dest);
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035D238);
#endif

s32 func_0035D2A0(s32 id, u32 dest) {
    return func_0035CE40(id, dest);
}

s32 func_0035D2C0(u32 dest) {
    return File_LoadSyncIop(2, dest);
}

s32 func_0035D2E0(u32 dest) {
    return func_0035CE40(0x2C, dest);
}

s32 func_0035D300(void) {
    return func_0035D3A0(0x2D, 0x82D90);
}

s32 func_0035D328(void) {
    return func_0035D3A0(1, 0x5010);
}

s32 func_0035D348(s32 n) {
    return func_0035D3A0(n * 7 + 0xBB, 0x82D90);
}

s32 func_0035D378(s32 n, u32 dest) {
    return func_0035D3A0(n * 3 + 0x158, dest);
}

s32 func_0035D3A0(s32 id, u32 dest) {
    return File_LoadSyncIop(id, dest);
}

/* 1 when `path` can be opened (no callers). */
s32 func_0035D3C0(const char *path) {
    s32 found = 0;
    s32 fd = sceOpen(path, 1);

    if (fd >= 0) {
        found = 1;
        sceClose(fd);
    }
    return found;
}

s32 func_0035D400(void) {
    return 0;
}

s32 func_0035D408(s32 dest, s32 id) {
    if (func_0035CE60(id) != 0) {
        return func_0035CE40(id, dest);
    }
    return 0;
}

void func_0035D458(void) {
    SPR->buf[0] = 0;
    SPR->buf[1] = 0;
    SPR->buf[2] = 0;
    SPR->size = 0;
    SPR->count = 0;
    D_00415F80[0] = 0;
}

/* Splits the scratchpad after `reserved` bytes from 0x70000100 into `count` (1-3) equal buffers. */
/* Splits the scratchpad after `reserved` bytes from 0x70000100 into `count` (1-3) equal buffers. */
u32 Spr_Setup(u32 count, s32 reserved) {
    u32 base = ((reserved + 0xF) & 0x3FF0) + 0x70000100;
    u32 size = 0x70004000 - base;

    SPR->buf[0] = 0;
    SPR->buf[1] = 0;
    SPR->buf[2] = 0;
    SPR->size = 0;
    SPR->count = count;
    switch (count) {
        case 1:
            size &= 0x3FF0;
            *(u32 *)0x70000000 = base;
            *(u16 *)0x7000000C = size;
            break;
        case 2:
            *(u32 *)0x70000000 = base;
            size = (size >> 1) & 0x3FF0;
            *(u16 *)0x7000000C = size;
            *(u32 *)0x70000004 = base + size;
            break;
        case 3:
            *(u32 *)0x70000000 = base;
            size = (size * 0x5555 >> 16) & 0x3FF0;
            *(u16 *)0x7000000C = size;
            *(u32 *)0x70000004 = base + size;
            *(u32 *)0x70000008 = base + size + size;
            break;
    }
    Spr_WaitChannel();
    return 0x70000100;
}

/* Saves the scratchpad header (0x100 bytes) to D_00415E80 with interrupts off. */
void func_0035D588(void) {
    u8 *src;
    u8 *dst;
    u32 status;
    u8 *end;

    do {
        EE_DI();
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    dst = (u8 *)&D_00415E80;
    src = (u8 *)0x70000000;
    end = (u8 *)0x70000100;
loop:
#ifdef PORT
    ((u64 *)dst)[0] = ((u64 *)src)[0];
    ((u64 *)dst)[1] = ((u64 *)src)[1];
    ((u64 *)dst)[2] = ((u64 *)src)[2];
    ((u64 *)dst)[3] = ((u64 *)src)[3];
#else
    __asm__ volatile(
        "ld $6, 0(%0)\n"
        "ld $7, 8(%0)\n"
        "ld $8, 16(%0)\n"
        "ld $9, 24(%0)\n"
        : : "r"(src) : "$6", "$7", "$8", "$9", "memory");
    __asm__ volatile(
        "sd $6, 0(%0)\n"
        "sd $7, 8(%0)\n"
        "sd $8, 16(%0)\n"
        "sd $9, 24(%0)\n"
        : : "r"(dst) : "$6", "$7", "$8", "$9", "memory");
#endif
    src += 0x20;
    dst += 0x20;
    if (src != end) {
        goto loop;
    }
    D_00415E80.unk100 = 1;
    EE_EI();
}

/* Restores the scratchpad header from D_00415E80 with interrupts off. */
void func_0035D608(void) {
    u8 *src;
    u8 *dst;
    u32 status;
    u8 *end;

    do {
        EE_DI();
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    src = (u8 *)&D_00415E80;
    dst = (u8 *)0x70000000;
    end = (u8 *)&D_00415E80.unk100;
loop:
#ifdef PORT
    ((u64 *)dst)[0] = ((u64 *)src)[0];
    ((u64 *)dst)[1] = ((u64 *)src)[1];
    ((u64 *)dst)[2] = ((u64 *)src)[2];
    ((u64 *)dst)[3] = ((u64 *)src)[3];
#else
    __asm__ volatile(
        "ld $6, 0(%0)\n"
        "ld $7, 8(%0)\n"
        "ld $8, 16(%0)\n"
        "ld $9, 24(%0)\n"
        : : "r"(src) : "$6", "$7", "$8", "$9", "memory");
    __asm__ volatile(
        "sd $6, 0(%0)\n"
        "sd $7, 8(%0)\n"
        "sd $8, 16(%0)\n"
        "sd $9, 24(%0)\n"
        : : "r"(dst) : "$6", "$7", "$8", "$9", "memory");
#endif
    src += 0x20;
    dst += 0x20;
    if (src != end) {
        goto loop;
    }
    D_00415E80.unk100 = 0;
    EE_EI();
}

void *func_0035D680(void) {
    return D_00415E80.unk100 == 0 ? (void *)0x70000000 : (void *)&D_00415E80;
}

s32 func_0035D698(u32 size) {
    if (gSprDma.unk1C != 0) {
        return 0;
    }
    if (size > 0x100000) {
        return 0;
    }
    gSprDma.unk1C = 1;
    return 1;
}

s32 func_0035D6D0(void) {
    return D_00415FE0[0];
}

/* Sets bit 16 of DMAC register 0x1000F590 until 0x1000F520 shows it. */
void func_0035D6E0(void) {
    if (!(*(volatile u32 *)0x1000F520 & 0x10000)) {
        do {
            *(volatile u32 *)0x1000F590 = 0x10000;
            EE_HW_POLL();
        } while (!(*(volatile u32 *)0x1000F520 & 0x10000));
    }
}

void func_0035D738(void) {
    if (*(volatile u32 *)0x1000F520 & 0x10000) {
        do {
            *(volatile u32 *)0x1000F590 = 0;
            EE_HW_POLL();
        } while (*(volatile u32 *)0x1000F520 & 0x10000);
    }
}

void func_0035E388(s32 channel);

/* Resets the scratchpad streaming: clears the header through toSPR DMA, stops the ring buffer and re-arms both
 * channels, then restarts the stream (func_0035DA78). */
void func_0035D790(s32 arg) {
    s32 *busy;
    volatile u32 *spr = (volatile u32 *)0x1000D400;
    volatile u32 **pspr; /* fake (decomp-permuter): keeps spr in a register */
    u128 tag;

    gSprDma.unk20 = 0;
    busy = (s32 *)&gSprDma.unk1C;
    pspr = &spr;
    if (*busy != 0) {
        u8 *base = D_200000;

        gSprDma.unk1C = 0;
        gSprDma.unk20 = (u32)(base + 0x80000);
    }
    Dma_Wait(spr);
    spr[0x20] = Spr_Setup(0, 0x100);
#ifdef PORT
    /* the original sends `tag` uninitialised: stale stack bytes, and the channel's MADR keeps its stack address, both
     * set by the compiler's frames. Here a zeroed quadword at a fixed EE RAM address (docs/port/README.md gotcha 32) */
    *PORT_EE_SPARE_QW = 0;
    Dma_SendNormal(*pspr, PORT_EE_SPARE_QW, 1);
#else
    Dma_SendNormal(*pspr, &tag, 1);
#endif
    func_0035E388(9);
    Dma_Wait(spr);
    *(volatile u32 *)0x1000E000 &= 0xFFFFFFFD;
    Dma_Wait((volatile u32 *)0x10009000);
    Dma_Wait((volatile u32 *)0x1000A000);
    *(volatile u32 *)0x1000E010 = 2;
    EE_SYNC();
    EE_SYNC_P();
    *(volatile u32 *)0x1000E010 = 4;
    EE_SYNC();
    EE_SYNC_P();
    gSprDma.unk2 = 0;
    gSprDma.unk3 = 0;
    gSprDma.pending = 0;
    gSprDma.unk0 = 0;
    gSprDma.state = 0;
    EnableDmac(1);
    EnableDmac(2);
    gSprDma.unk24 = arg;
    func_0035DA78();
}

/* Waits until the toSPR channel has passed the tag the fromSPR side (0x70000014) is reading. */
void func_0035D8C8(void) {
    volatile u32 *chan = (volatile u32 *)0x1000D000;
    volatile SprDma *s;
    u32 madr;
    volatile u32 *cur;

    Dma_Wait((volatile u32 *)0x1000D000);
    madr = chan[4];
    s = &gSprDma;
    for (;;) {
        __asm__ volatile("" ::: "memory"); /* fake */
        cur = *(volatile u32 **)0x70000014;
        if (madr >= cur[12]) {
            break;
        }
        if (s->pending == 0 && func_0035E600(cur) == 0 && cur == *(volatile u32 **)0x70000014) {
            break;
        }
    }
}

void func_0035D958(volatile u32 *chan, s32 state) {
    volatile SprDma *s = &gSprDma;
    s32 running;

    func_0035D8C8();
    func_0035D6E0();
    s->state = state;
    running = (chan[0] >> 8) & 1;
    if (running) {
        chan[0] &= ~0x100;
    }
    func_0035D738();
    *(volatile u32 *)0x1000E000 &= ~0xC;
    EE_SYNC();
    if (running) {
        chan[0] |= 0x100;
    }
}

void func_0035DA08(void) {
    u8 *base = (u8 *)Spr_Setup(0, 0x100);
    u8 *pkt = base;

    Dma_AddRefTag(&pkt, 0x10);
    pkt -= 0x10;
    ((u16 *)pkt)[1] = 0;
    func_00343178(base, 8);
    Dma_Wait((volatile u32 *)0x1000D000);
}

void func_0035DA70(void) {
    func_0035DA08();
}

#if 0

/* NON_MATCHING: 25 lines: the original reloads 0x1000D010 by absolute address after each store to gSprDma; the compiler forwards the stored value */
void func_0035DA78(void) {
    volatile SprDma *s;
    u8 *ring;
    s32 size;
    s32 i;

    SPR->unk18 = 0x1000D000;
    SPR->unk14 = 0x10009000;
    s = &gSprDma;
    SPR->unk10 = 0;
    ring = D_300000;
    size = 0x40000;
    i = 0;
    do {
        i++;
    } while (!((size >> i) & 1));
    SPR->unkF = i;
    Dma_Wait((volatile u32 *)0x1000D000);
    Dma_Wait((volatile u32 *)0x10009000);
    *(volatile u32 *)0x1000E050 = (u32)ring;
    *(volatile u32 *)0x1000E040 = 0x3FFF0;
    *(volatile u32 *)0x1000E000 = (*(volatile u32 *)0x1000E000 & 0xFFFFFFF3) | 8;
    *(u32 *)0x1000D010 = *(volatile u32 *)0x1000E050;
    s->state = 1;
    s->unk8 = *(u32 *)0x1000D010;
    s->pending++;
    *(u32 *)0x10009030 = *(u32 *)0x1000D010;
    func_0035DF68(s);
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035DA78);
#endif

void func_0035DB90(void) {
    volatile SprDma *s = &gSprDma;
    u32 status;

    func_0035DA70();
    *(u32 *)0x70000018 = 0x1000D000;
    do {
        EE_DI();
        s++; /* fake (decomp-permuter): s set more than once, so the scheduler keeps the 0x1000D010 */
        s--; /* load after the stores through it, as in the original */
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    s->pending++;
    s->state = 2;
    s->unkC = *(u32 *)0x1000D010;
    func_0035DFA8(s);
    EE_NOP();
    EE_SYNC_L();
    EE_EI();
}

void func_0035DC20(void) {
    volatile SprDma *s = &gSprDma;

    Spr_WaitIdle();
    func_0035D958((volatile u32 *)0x10009000, s->state);
}

#if 0
/* NON_MATCHING: 13 lines: the 0x1000D010 loads move in front of the gSprDma stores (see func_0035DDE0) and registers differ */
void func_0035DC58(void) {
    volatile SprDma *s = &gSprDma;
    SprHeader *spr = SPR;
    u8 *ring;
    s32 size;
    s32 i;
    s32 wait;
    u32 status;

    func_0035DC20();
    size = s->unk20 != 0 ? 0x80000 : 0x100000;
    ring = D_200000;
    spr->unk18 = 0x1000D000;
    spr->unk14 = 0x1000A000;
    spr->unk10 = 0;
    for (i = 0; !((size >> i) & 1); i++) {
    }
    spr->unkF = i;
    Dma_Wait((volatile u32 *)0x1000D000);
    Dma_Wait((volatile u32 *)0x1000A000);
    *(volatile u32 *)0x1000E050 = (u32)ring;
    *(volatile u32 *)0x1000E040 = size - 0x10;
    *(volatile u32 *)0x1000E000 |= 0xC;
    *(u32 *)0x1000D010 = (u32)ring;
    wait = s->unk24;
    do {
        EE_DI();
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    s->pending++;
    s->state = 3;
    s->unk10 = *(u32 *)0x1000D010;
    s->unk0 = 1;
    *(u32 *)0x1000A030 = *(u32 *)0x1000D010;
    if (wait == 0 || s->unk2 >= 2) {
        func_0035DFF0(s);
    }
    EE_NOP();
    EE_SYNC_L();
    EE_EI();
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035DC58);
#endif

void func_0035DDE0(void) {
    volatile SprDma *s = &gSprDma;
    u32 status;

    s++; /* fake (decomp-permuter): s set more than once, so the scheduler keeps the 0x1000D010 load after the */
    s--; /* stores through it, as in the original */
    *(u32 *)0x70000018 = 0x1000D000;
    do {
        EE_DI();
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    s->pending++;
    s->state = 4;
    s->unk14 = *(u32 *)0x1000D010;
    func_0035E048(s);
    EE_NOP();
    EE_SYNC_L();
    EE_EI();
}

void func_0035DE68(void) {
    volatile SprDma *s = &gSprDma;
    u32 status;

    s++; /* fake (decomp-permuter): s set more than once, so the scheduler keeps the 0x1000D010 load after the */
    s--; /* stores through it, as in the original */
    *(u32 *)0x70000018 = 0x1000D000;
    do {
        EE_DI();
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    s->pending++;
    s->state = 5;
    s->unk18 = *(u32 *)0x1000D010;
    func_0035E100(s);
    EE_NOP();
    EE_SYNC_L();
    EE_EI();
}

void func_0035DEF0(void) {
    if (gSprDma.state < 6) {
        Spr_WaitIdle();
        func_0035D958((volatile u32 *)0x1000A000, 6);
    }
}

void func_0035DF38(void) {
    func_0035DEF0();
    *(u32 *)0x70000018 = 0x1000D000;
}

void func_0035DF68(volatile SprDma *s) {
    if (s->state != 0 && s->unk3 != 0) {
        s->pending--;
        Dma_SendChain((volatile u32 *)0x10009000, (void *)s->unk8);
    }
}

void func_0035DFA8(volatile SprDma *s) {
    if (s->state >= 2 && s->unk2 != 0) {
        s->pending--;
        Dma_SendChain((volatile u32 *)0x10009000, (void *)s->unkC);
    }
}

s32 func_0035DFF0(volatile SprDma *s) {
    if (s->unk0 != 0 && s->state >= 3) {
        s->unk0 = 0;
        s->pending--;
        return Dma_SendChain((volatile u32 *)0x1000A000, (void *)s->unk10);
    }
}

void func_0035E048(volatile SprDma *s) {
    volatile u32 *chan;

    if (s->state >= 4 && s->unk2 >= 2 && s->unk3 >= 2) {
        s->pending--;
        if (s->state < 6) {
            Dma_Wait((volatile u32 *)0x1000D000);
            *(volatile u32 *)0x1000E000 = (*(volatile u32 *)0x1000E000 & 0xFFFFFFF3) | 8;
        }
        chan = (volatile u32 *)0x10009000;
        Dma_SendChain(chan, (void *)s->unk14);
        ((SprHeader *)func_0035D680())->unk14 = (u32)chan;
    }
}

void func_0035E100(volatile SprDma *s) {
    volatile u32 *chan;

    if (s->state >= 5 && s->unk2 >= 3) {
        s->pending--;
        if (s->state < 6) {
            Dma_Wait((volatile u32 *)0x1000D000);
            *(volatile u32 *)0x1000E000 |= 0xC;
        }
        chan = (volatile u32 *)0x1000A000;
        Dma_SendChain(chan, (void *)s->unk18);
        ((SprHeader *)func_0035D680())->unk14 = (u32)chan;
    }
}

s32 func_0035E198(void) {
    s32 r = 0;

    if (gSprDma.state >= 5) {
        r = gSprDma.unk3 >= 3;
    }
    return r;
}

void func_0035E1C8(void) {
    u32 status;

    if (gSprDma.unk20 == 0) {
        do {
            EE_DI();
            EE_SYNC_P();
            EE_MFC0_STATUS(status);
        } while (status & 0x10000);
        func_0035DFF0(&gSprDma);
        EE_NOP();
        EE_SYNC_L();
        EE_EI();
    }
}

s32 func_0035E228(s32 channel) {
    volatile SprDma *s = &gSprDma;

    if (channel == 1) {
        s->unk2++;
        switch (s->unk2) {
        case 1:
            func_0035DFA8(s);
            break;
        case 2:
            func_0035DFF0(s);
            func_0035E048(s);
            break;
        case 3:
            func_0035E100(s);
        default:
            iDisableDmac(1);
            break;
        }
        EE_SYNC_L();
        EE_EI();
        return 0;
    }
    return 0;
}

s32 func_0035E2E8(s32 channel) {
    volatile SprDma *s = &gSprDma;

    if (channel == 2) {
        s->unk3++;
        switch (s->unk3) {
        case 1:
            func_0035DF68(s);
            break;
        case 2:
            func_0035E048(s);
            break;
        case 3:
        default:
            iDisableDmac(2);
            break;
        }
        EE_SYNC_L();
        EE_EI();
        return 0;
    }
    return 0;
}

void func_0035E370(void) {
    *(u32 *)0x70000018 = 0x1000D400;
}

void func_0035E388(s32 channel) {
    u32 stat = *(volatile u32 *)0x1000E020;

    stat = (stat & 0xFFFF0000) | (1 << channel);
    *(volatile u32 *)0x1000E020 = stat;
}

void func_0035E3B0(void) {
    u32 stat = *(volatile u32 *)0x1000E020;

    *(volatile u32 *)0x1000E020 = stat & 0xFFFF0000;
    *(volatile u32 *)0x1000E010 = stat & 0x3FF;
}

/* Waits for the DMA channel the scratchpad header names at 0x70000018, if any. */
void Spr_WaitChannel(void) {
    volatile u32 *chan = *(volatile u32 **)0x70000018;

    if (chan != NULL) {
        Dma_Wait(chan);
    }
}

#if 0
/* NON_MATCHING: 46 lines: the stores are right; the scheduling of the scratchpad header stores between the gSprDma ones and the registers differ */
void func_0035E400(void) {
    volatile u32 *chan = (volatile u32 *)0x1000D400;
    SprHeader *spr = SPR;

    gSprDma.unk0 = 0;
    gSprDma.pending = 0;
    gSprDma.unk2 = 0;
    gSprDma.unk3 = 0;
    gSprDma.state = 6;
    gSprDma.unkC = 0;
    gSprDma.unk10 = 0;
    spr->unk14 = 0;
    spr->unk10 = 0;
    spr->unkF = 0;
    spr->unk18 = (u32)chan;
    gSprDma.unk24 = 0;
    gSprDma.unk18 = 0;
    gSprDma.unk1C = 0;
    gSprDma.unk14 = 0;
    gSprDma.unk20 = 0;
    *(volatile u32 *)0x1000E000 &= 0xFFFFFFFD;
    func_0035E3B0();
    *(volatile u32 *)0x10008000 |= 0x40;
    *(volatile u32 *)0x10009000 |= 0x40;
    *(volatile u32 *)0x1000A000 &= 0xFFFFFFBF;
    *chan |= 0x40;
    AddDmacHandler(1, func_0035E228, 0);
    DisableDmac(1);
    AddDmacHandler(2, func_0035E2E8, 0);
    DisableDmac(2);
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E400);
#endif

void func_0035E518(volatile u32 *chan, void *tag) {
    u32 addr = ((u32)tag + 0x10000000) & 0x8FFFFFFF;
    u32 chcr = Dma_Wait(chan) & ~0xC;

    chan[12] = addr;
    chan[8] = 0;
    chan[0] = ((chcr | 4) & ~1) | 0x100;
}

void func_0035E588(volatile u32 *chan, void *addr, u32 qwc) {
    u32 madr = ((u32)addr + 0x10000000) & 0x8FFFFFFF;
    u32 chcr = Dma_Wait(chan) & ~0xC;

    chan[4] = madr;
    chan[8] = qwc;
    chan[0] = (chcr & ~1) | 0x100;
}

s32 func_0035E600(volatile u32 *chan) {
    return (chan[0] >> 8) & 1;
}

s32 Spr_WaitIdle(void) {
    volatile SprDma *s = &gSprDma;

    do {
    } while (s->pending != 0);
    return 0;
}

void func_0035E640(u32 sprAddr, void *addr, u32 qwc) {
    volatile u32 *chan = (volatile u32 *)0x1000D000;
    u32 spr = sprAddr & 0x3FF0;
    u32 chcr = Dma_Wait(chan);

    chan[4] = (u32)addr;
    chan[32] = spr;
    chan[8] = qwc;
    chan[0] = (chcr & ~0xC) | 0x101;
}

void func_0035E6B8(void *tag) {
    u32 chcr;
    u32 madr;

    func_0035DEF0();
    madr = ((u32)tag + 0x10000000) & 0x8FFFFFFF;
wait:
    EE_HW_POLL();
    chcr = *(u32 *)0x1000A000;
    if (chcr & 0x100) {
        goto wait;
    }
    *(u32 *)0x1000A030 = madr;
    *(u32 *)0x1000A020 = 0;
    *(u32 *)0x1000A000 = (chcr & ~0xC) | 0x105;
busy:
    EE_HW_POLL();
    if (*(u32 *)0x1000A000 & 0x100) {
        goto busy;
    }
}

void func_0035E758(void *addr, u32 qwc) {
    u32 chcr;
    u32 madr;

    func_0035DEF0();
    madr = ((u32)addr + 0x10000000) & 0x8FFFFFFF;
wait:
    EE_HW_POLL();
    chcr = *(u32 *)0x1000A000;
    if (chcr & 0x100) {
        goto wait;
    }
    *(u32 *)0x1000A010 = madr;
    *(u32 *)0x1000A020 = qwc;
    *(u32 *)0x1000A000 = (chcr & ~0xC) | 0x101;
busy:
    EE_HW_POLL();
    if (*(u32 *)0x1000A000 & 0x100) {
        goto busy;
    }
}
