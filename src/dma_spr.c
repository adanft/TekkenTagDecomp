/* 0x35D048-0x35E808: file loads by number, the scratchpad buffers (Spr_Setup) and transfers to and from the
 * scratchpad, the DMAC and SIF helpers. */
#include "common.h"

/* Table of file ids, read from the original executable. */
extern const s16 D_004FBF50[];
extern s32 D_005F6B38;
extern s32 D_005F6B3C;
extern s32 D_00415F80[];
extern s32 D_00415FE0[];

/* The scratchpad header (0x70000000): up to three buffers of `size` bytes after 0x70000100 (Spr_Setup). */
typedef struct {
    /* 0x0 */ u32 buf[3];
    /* 0xC */ u16 size;
    /* 0xE */ u8 count;
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

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035D120);

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
/* NON_MATCHING: the original keeps n and n - 0x40 apart (the division rounds with n - 0x3D); the compiler reuses one
 * register. */
s32 func_0035D238(s32 n, u32 dest) {
    s32 id;

    if (n == 0) {
        id = 0x29;
    } else if (n < 0x40) {
        id = n * 2 + 0x2B;
    } else {
        n -= 0x40;
        id = n / 4 * 7 + (n & 3) + 0xB7;
    }
    return func_0035CE40(id, dest);
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
#if 0
/* NON_MATCHING: the copy loop: the original increments the source before the destination and pads the branch, not the
 * loop head. */
void func_0035D588(void) {
    u8 *src;
    u8 *dst;
    u32 status;

    do {
        __asm__ volatile("di");
        __asm__ volatile("sync.p");
        __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    } while (status & 0x10000);
    dst = (u8 *)&D_00415E80;
    for (src = (u8 *)0x70000000; src != (u8 *)0x70000100; src += 0x20, dst += 0x20) {
        __asm__ volatile(
            "ld $6, 0(%0)\n"
            "ld $7, 8(%0)\n"
            "ld $8, 16(%0)\n"
            "ld $9, 24(%0)\n"
            "sd $6, 0(%1)\n"
            "sd $7, 8(%1)\n"
            "sd $8, 16(%1)\n"
            "sd $9, 24(%1)\n"
            : : "r"(src), "r"(dst) : "$6", "$7", "$8", "$9", "memory");
    }
    D_00415E80.unk100 = 1;
    __asm__ volatile("ei");
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035D588);
#endif

/* Restores the scratchpad header from D_00415E80 with interrupts off. */
#if 0
/* NON_MATCHING: same copy loop as func_0035D588. */
void func_0035D608(void) {
    u8 *src;
    u8 *dst;
    u32 status;

    do {
        __asm__ volatile("di");
        __asm__ volatile("sync.p");
        __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    } while (status & 0x10000);
    dst = (u8 *)0x70000000;
    for (src = (u8 *)&D_00415E80; src != (u8 *)&D_00415E80.unk100; src += 0x20, dst += 0x20) {
        __asm__ volatile(
            "ld $6, 0(%0)\n"
            "ld $7, 8(%0)\n"
            "ld $8, 16(%0)\n"
            "ld $9, 24(%0)\n"
            "sd $6, 0(%1)\n"
            "sd $7, 8(%1)\n"
            "sd $8, 16(%1)\n"
            "sd $9, 24(%1)\n"
            : : "r"(src), "r"(dst) : "$6", "$7", "$8", "$9", "memory");
    }
    D_00415E80.unk100 = 0;
    __asm__ volatile("ei");
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035D608);
#endif

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
        } while (!(*(volatile u32 *)0x1000F520 & 0x10000));
    }
}

void func_0035D738(void) {
    if (*(volatile u32 *)0x1000F520 & 0x10000) {
        do {
            *(volatile u32 *)0x1000F590 = 0;
        } while (*(volatile u32 *)0x1000F520 & 0x10000);
    }
}

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035D790);

/* Waits until the toSPR channel has passed the tag the fromSPR side (0x70000014) is reading. */
#if 0
/* NON_MATCHING: the loop reloads the pointer at 0x70000014 at its head; the compiler reuses the value it just compared
 * (one branch target differs). */
void func_0035D8C8(void) {
    volatile u32 *chan = (volatile u32 *)0x1000D000;
    volatile SprDma *s;
    u32 madr;
    volatile u32 *cur;

    Dma_Wait((volatile u32 *)0x1000D000);
    madr = chan[4];
    s = &gSprDma;
    for (;;) {
        cur = *(volatile u32 **)0x70000014;
        if (madr >= cur[12]) {
            break;
        }
        if (s->pending == 0 && func_0035E600(cur) == 0 && cur == *(volatile u32 *volatile *)0x70000014) {
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035D8C8);
#endif

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
    __asm__ volatile("sync");
    if (running) {
        chan[0] |= 0x100;
    }
}

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035DA08);

void func_0035DA70(void) {
    func_0035DA08();
}

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035DA78);

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035DB90);

void func_0035DC20(void) {
    volatile SprDma *s = &gSprDma;

    Spr_WaitIdle();
    func_0035D958((volatile u32 *)0x10009000, s->state);
}

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035DC58);

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035DDE0);

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035DE68);

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

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E048);

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E100);

s32 func_0035E198(void) {
    s32 r = 0;

    if (gSprDma.state >= 5) {
        r = gSprDma.unk3 >= 3;
    }
    return r;
}

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E1C8);

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E228);

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E2E8);

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

INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E400);

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

#if 0
/* NON_MATCHING: the original reaches the DMA registers by absolute address and still reloads CHCR in the last wait
 * loop; volatile makes the compiler build the address in a register, non-volatile reuses the stored value. */
void func_0035E6B8(void *tag) {
    u32 chcr;

    func_0035DEF0();
    do {
        chcr = *(volatile u32 *)0x1000A000;
    } while (chcr & 0x100);
    *(volatile u32 *)0x1000A030 = ((u32)tag + 0x10000000) & 0x8FFFFFFF;
    *(volatile u32 *)0x1000A020 = 0;
    *(volatile u32 *)0x1000A000 = (chcr & ~0xC) | 0x105;
    while (*(volatile u32 *)0x1000A000 & 0x100) {
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E6B8);
#endif

#if 0
/* NON_MATCHING: same register access problem as func_0035E6B8. */
void func_0035E758(void *addr, u32 qwc) {
    u32 chcr;

    func_0035DEF0();
    do {
        chcr = *(volatile u32 *)0x1000A000;
    } while (chcr & 0x100);
    *(volatile u32 *)0x1000A010 = ((u32)addr + 0x10000000) & 0x8FFFFFFF;
    *(volatile u32 *)0x1000A020 = qwc;
    *(volatile u32 *)0x1000A000 = (chcr & ~0xC) | 0x101;
    while (*(volatile u32 *)0x1000A000 & 0x100) {
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_spr", func_0035E758);
#endif
