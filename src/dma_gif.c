/* 0x342E78-0x346398: DMA helpers (channel kicks and waits, scratchpad transfers), VU0 microcode upload and the
 * matrices of the VU1 packets. */
#include "common.h"
#include "fighter.h"

typedef f32 Vu0Matrix[4][4];

/* A VU1 packet with four matrices after its tag; the last column of the third holds four scalars. */
typedef struct {
    /* 0x00 */ u64 tag[2];
    /* 0x10 */ Vu0Matrix m[4];
} VuMatrixPacket;

/* 26 records at D_008BF3A0 (and a table of the same layout in .data, D_00436688). */
typedef struct {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ u16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ u8 pad6[0x12];
} Unk8BF3A0;

typedef struct {
    /* 0x0 */ u16 *unk0;
    /* 0x4 */ u16 unk4;
} Unk8BF350;

/* Two lists of 0x7F entries of 0x18 bytes, one per side (D_008C2F90). */
typedef struct {
    /* 0x000 */ s32 count;
    /* 0x004 */ u8 unk4[0xC00];
    /* 0xC04 */ u16 unkC04;
} Unk8C2F90; /* size 0xC08 */

/* Per side, six pointers from func_003A9488 and 12 words (D_008BF2C0, D_008BF308). */
typedef struct {
    /* 0x00 */ u16 *ptr[6];
    /* 0x18 */ s32 unk18[11];
    /* 0x44 */ s32 unk44;
} Unk8BF2C0; /* size 0x48 */

/* The part of an object (from func_003464F8) that func_003456B0 sets up, at +0x20. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s16 unk14[8];
    /* 0x24 */ u8 pad24[0xC];
    /* 0x30 */ f32 unk30[4];
    /* 0x40 */ s32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ u8 unk50;
    /* 0x51 */ u8 unk51;
    /* 0x52 */ u8 unk52;
    /* 0x53 */ u8 pad53;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ f32 unk58;
    /* 0x5C */ u8 pad5C[4];
    /* 0x60 */ f32 unk60[4];
    /* 0x70 */ f32 unk70[4];
    /* 0x80 */ f32 unk80[4];
    /* 0x90 */ s32 unk90;
    /* 0x94 */ s16 unk94;
    /* 0x96 */ s16 unk96;
} Unk3456B0Sub;

typedef struct {
    /* 0x00 */ u8 pad0[8];
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ void *unk10;   /* its slot in D_008C2788 */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s16 unk18;
    /* 0x1A */ s16 unk1A;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ Unk3456B0Sub sub;
} Unk3456B0;

extern Unk8BF2C0 D_008BF2C0;
extern Unk8BF2C0 D_008BF308;
extern Unk8BF3A0 D_008BF3A0[26];
extern Unk8BF3A0 D_00436688[];
extern Unk8BF350 D_008BF350[];
extern Unk8C2F90 D_008C2F90[2];
extern f32 D_005F693C;
extern f32 D_005F6940;
extern f32 D_005F6944;
extern f32 D_005F6948;
extern s16 gMode;
extern u8 D_004F5F00[];
extern u8 D_004F6C80[];
extern u8 D_004FA7E0[];
extern u8 D_100000[];

extern Fighter gFighters[4];

void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);
void Dma_SendChain(volatile u32 *chan, void *tag);
s32 func_003B8820(void);
void func_0035D028(s32 addr, s32 size);
Unk3456B0 *func_003464F8(); /* no prototype: func_00344BE0 passes it a callback */
void func_003456B0(Unk3456B0 *p);
u16 *func_003A9488(s32 id, s32 side);
void func_00343F50(void);
void func_0035E1C8(void);
s32 func_003BB4D8(void);
s32 func_003ABCD0(void);
u32 Dma_Wait(volatile u32 *chan);
void func_00343178(u32 spr, u32 qwc);
void sceGifPkInit(void *pk, void *base);
void sceGifPkReserve(void *pk, u32 n);
void sceGifPkRef(void *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceGifPkTerminate(void *pk);
u32 sceGifPkSize(void *pk);

/* Two rings of 256 object slots (D_008C2788), one per side: the oldest slot and the next one to try. */
typedef struct {
    /* 0x000 */ s16 oldest;
    /* 0x002 */ s16 next;
    /* 0x004 */ Unk3456B0 *slot[256];
} Unk8C2788; /* size 0x404 */

extern Unk8C2788 D_008C2788[2];
/* GS packets of the 26 records, 46 frames of 0x30 bytes each (fixed buffer below the executable). */
typedef struct {
    u128 q[3];
} GsFrame;

extern GsFrame D_168A30[][46];
extern u8 D_00455E30[];

/* A segment of a part (func_00343CA8): address, size in quadwords, size of its VU1 packet. */
typedef struct {
    /* 0x0 */ u32 addr;
    /* 0x4 */ u32 qwc;
    /* 0x8 */ u32 size;
} PartSeg;

/* A libgraph GIF packet header followed by its buffer. */
typedef struct {
    /* 0x00 */ u32 *cur;
    /* 0x04 */ u128 *base;
    /* 0x08 */ u32 pad8[2];
    /* 0x10 */ u128 buf[1];
} GifPacket;

void func_00342E78(Vu0Matrix *a, Vu0Matrix *b, VuMatrixPacket *pkt) {
    sceVu0CopyMatrix(pkt->m[0], a[0]);
    sceVu0CopyMatrix(pkt->m[3], a[1]);
    sceVu0CopyMatrix(pkt->m[1], b[0]);
    sceVu0CopyMatrix(pkt->m[2], b[1]);
    pkt->m[2][0][3] = D_005F6944;
    pkt->m[2][1][3] = D_005F6948;
    pkt->m[2][2][3] = D_005F6940;
    pkt->m[2][3][3] = D_005F693C;
}

void func_00342F00(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2, Vu0Matrix m3, VuMatrixPacket *pkt) {
    sceVu0CopyMatrix(pkt->m[0], m0);
    sceVu0CopyMatrix(pkt->m[1], m1);
    sceVu0CopyMatrix(pkt->m[2], m2);
    sceVu0CopyMatrix(pkt->m[3], m3);
    pkt->m[2][0][3] = D_005F6944;
    pkt->m[2][1][3] = D_005F6948;
    pkt->m[2][2][3] = D_005F6940;
    pkt->m[2][3][3] = D_005F693C;
}

void func_00342F98(f32 v) {
    D_005F693C = v;
}

f32 func_00342FA0(void) {
    return D_005F693C;
}

void func_00342FA8(f32 v) {
    D_005F6940 = v;
}

/* Rotates the first two or three words of the scratchpad (count at 0x7000000E) and returns the new first. */
#if 0
/* NON_MATCHING: the original reaches each scratchpad word with its own absolute address (lui per access); the compiler
 * shares the base. */
s32 func_00342FB0(void) {
    u8 n = *(u8 *)0x7000000E;
    s32 first = *(s32 *)0x70000000;
    s32 t;

    if (n == 2) {
        t = *(s32 *)0x70000004;
        *(s32 *)0x70000004 = first;
        *(s32 *)0x70000000 = t;
    } else if (n == 3) {
        *(s32 *)0x70000000 = *(s32 *)0x70000004;
        *(s32 *)0x70000004 = *(s32 *)0x70000008;
        *(s32 *)0x70000008 = first;
    }
    return *(s32 *)0x70000000;
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00342FB0);
#endif

/* Starts DMA channel `chan` in chain mode on the tag at `tag` (scratchpad addresses get the SPR bit). */
void Dma_SendChain(volatile u32 *chan, void *tag) {
    u32 addr = ((u32)tag + 0x10000000) & 0x8FFFFFFF;
    u32 chcr;

    do {
        chcr = chan[0];
    } while (chcr & 0x100);
    chan[12] = addr;
    chan[8] = 0;
    chan[0] = (chcr & ~0xC) | 0x105;
}

/* Starts DMA channel `chan` in normal mode: `qwc` quadwords from `addr`. */
void Dma_SendNormal(volatile u32 *chan, void *addr, u32 qwc) {
    u32 madr = ((u32)addr + 0x10000000) & 0x8FFFFFFF;
    u32 chcr;

    do {
        chcr = chan[0];
    } while (chcr & 0x100);
    chan[4] = madr;
    chan[8] = qwc;
    chan[0] = (chcr & ~0xC) | 0x101;
}

/* Starts a toSPR chain transfer from `tag` to scratchpad offset `spr` once the previous one is done (BC0F). */
void func_003430D8(u32 spr, void *tag) {
    volatile u32 *chan = (volatile u32 *)0x1000D400;

    spr &= 0x3FF0;
    __asm__ volatile("1:\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tbc0f 1b");
    *(volatile u32 *)0x1000E010 = 0x200;
    chan[12] = (u32)tag;
    chan[32] = spr;
    chan[8] = 0;
    chan[0] = 0x145;
}

/* Starts a toSPR transfer of `qwc` quadwords from `src` to scratchpad offset `spr` once the previous one is done. */
void func_00343128(u32 spr, void *src, u32 qwc) {
    volatile u32 *chan = (volatile u32 *)0x1000D400;

    spr &= 0x3FF0;
    /* the wait and the register writes come from one macro (do { } while (0)) */
    do {
        __asm__ volatile("1:\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tbc0f 1b");
        *(volatile u32 *)0x1000E010 = 0x200;
        chan[4] = (u32)src;
        chan[32] = spr;
        chan[8] = qwc;
        chan[0] = 0x141;
    } while (0);
}

#if 0
/* Sends `qwc` quadwords from the scratchpad ring buffer (fromSPR) once that many are ready, waiting for the
 * stream (func_0035E1C8) while they are not. */
/* NON_MATCHING: the loops are written with goto (a `while` loop hoists the 0x1000D000 load); the first test of the
 * retry loop and the registers of qwc/madr still differ. */
void func_00343178(u32 spr, u32 qwc) {
    u32 sadr = spr & 0x3FF0;
    u32 n = *(u32 *)0x70000010;
    u32 madr;
    u32 end;

wait:
    if (*(u32 *)0x1000D000 & 0x100) {
        goto wait;
    }
    madr = *(u32 *)0x1000D010;
retry:
    if (n <= qwc) {
        end = (*(u32 **)0x70000014)[12];
        if (madr >= end) {
            end += 1 << *(u8 *)0x7000000F;
        }
        n = (end - madr) >> 4;
        *(u32 *)0x70000010 = n;
        if (n <= qwc) {
            func_0035E1C8();
            n = *(u32 *)0x70000010;
            goto retry;
        }
    }
    *(u32 *)0x70000010 = n - qwc;
    *(u32 *)0x1000D080 = sadr;
    *(u32 *)0x1000D020 = qwc;
    *(u32 *)0x1000D000 = 0x101;
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00343178);
#endif

/* Waits until DMA channel `chan` is idle; returns its last CHCR. */
u32 Dma_Wait(volatile u32 *chan) {
    u32 chcr;

    do {
        chcr = chan[0];
    } while (chcr & 0x100);
    return chcr;
}

/* Uploads the VU0 microcode of the current mode (DMA channel 0, VIF0). */
void Vu0_LoadMicrocode(void) {
    if (gMode == 5) {
        Dma_SendChain((volatile u32 *)0x10008000, D_004FA7E0);
    } else if ((func_003BB4D8() != 0 || func_003ABCD0() != 0) && gFighters[1].unk127 == 0) {
        Dma_SendChain((volatile u32 *)0x10008000, D_004F6C80);
    } else {
        Dma_SendChain((volatile u32 *)0x10008000, D_004F5F00);
    }
}

#if 0
typedef struct {
    /* 0x000 */ u8 pad0[0x1E];
    /* 0x01E */ s16 unk1E;
    /* 0x020 */ u8 pad20[2];
    /* 0x022 */ s16 unk22;
    /* 0x024 */ u8 pad24[0x56C];
    /* 0x590 */ f32 unk590[3];
    /* 0x59C */ u8 pad59C[0x814];
    /* 0xDB0 */ u8 unkDB0[1];
} Unk343340;

typedef struct {
    /* 0x00 */ u8 pad0[8];
    /* 0x08 */ u8 *unk8;
} Unk3B87B0;

extern u128 D_00433400[4];
extern u128 D_004334C0[4];
extern s32 D_00415808[];
extern s32 D_005F67C0;

Unk3B87B0 *func_003B87B0(s32 id);
void Light_GetFighterMatrices(s32 id, u128 *m);
void func_0034A868(f32 *out, f32 *in);
void func_003439B0(s32 a, u8 *b);

#define VU0_MEM ((u128 *)0x11004000)

/* NON_MATCHING: the matrix uploads match; the order of the index terms (j * 4 + i * 0x14 + side * 0x28) and the
 * loop-constant registers differ in the two copies of the part loop. */
/* Uploads the matrices of an object to VU0 data memory and draws its parts (func_003439B0); with D_005F67C0 == 1
 * the second row of parts is skipped. */
void func_00343340(Unk343340 *o, s32 side) {
    u128 m[8];
    f32 v[4];
    Unk3B87B0 *p;
    s32 i;
    s32 j;
    s32 a;
    u128 *vu = VU0_MEM;

    p = func_003B87B0(o->unk22);
    Light_GetFighterMatrices(o->unk1E, m);
    vu[4] = D_004334C0[0];
    vu[5] = D_004334C0[1];
    vu[6] = D_004334C0[2];
    vu[7] = D_004334C0[3];
    vu[8] = m[0];
    vu[9] = m[1];
    vu[10] = m[2];
    v[0] = D_00415808[5] - o->unk590[0];
    v[1] = -D_00415808[6] - o->unk590[1];
    v[2] = -D_00415808[7] - o->unk590[2];
    func_0034A868(v, v);
    vu[11] = *(u128 *)v;
    vu[0xFC] = D_00433400[0];
    vu[0xFD] = D_00433400[1];
    vu[0xFE] = D_00433400[2];
    vu[0xFF] = D_00433400[3];
    __asm__ volatile(
        "lqc2 $vf8, 0x0(%0)\n"
        "lqc2 $vf9, 0x10(%0)\n"
        "lqc2 $vf10, 0x20(%0)\n"
        "lqc2 $vf11, 0x30(%0)\n"
        : : "r"(&m[4]));
    if (side == 0) {
        for (i = 0; i < 2; i++) {
            if (D_005F67C0 == 1 && i == D_005F67C0) {
                continue;
            }
            for (j = 0; j < 5; j++) {
                a = *(s32 *)(p->unk8 + (j * 4 + i * 0x14 + side * 0x28) + 0x2C);
                if (a != 0) {
                    func_003439B0(a, o->unkDB0);
                }
            }
        }
    } else {
        for (i = 0; i < 2; i++) {
            if (D_005F67C0 == 1 && i == D_005F67C0) {
                continue;
            }
            for (j = 0; j < 5; j++) {
                a = *(s32 *)(p->unk8 + (j * 4 + i * 0x14 + side * 0x28) + 0x2C);
                if (a != 0) {
                    func_003439B0(a, o->unkDB0);
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00343340);
#endif

/* func_003435C0, func_003436B8, func_003437B8 and func_003438B8 are handwritten VU0 macro-mode assembly (lqc2,
 * ctc2, vcallms, jumps through $t4 into the middle of the unrolled body), not compiler output: they stay in
 * assembly. */
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003435C0);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003436B8);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003437B8);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003438B8);

#if 0
u128 *func_003435C0(u128 **dst, u128 *src, s32 n, u128 *hdr);
u128 *func_003436B8(u128 **dst, u128 *src, s32 n, s64 a);
u128 *func_003437B8(u128 **dst, u128 *src, s32 n, s64 a);
u128 *func_003438B8(u128 **dst, u128 *src, s32 n, u128 *hdr);

/* NON_MATCHING: same logic; the stack slots (the header copy at sp+0 in the original) and the register allocation
 * differ. */
/* Streams the segments of part `id` through the scratchpad, expanding them into VU1 packets: type 1 records start a
 * block, type 2 records are expanded by the VU0 routines, type 0 uploads matrices (from `mats`) to VU0 memory. */
void func_003439B0(s32 id, u8 *mats) {
    u128 *dst;
    PartSeg *segs;
    u32 n0;
    u128 hdr;
    u32 count;
    PartSeg *seg;
    PartSeg *next;
    u128 *buf;
    u128 *out;
    u128 *p;
    s32 done;
    s32 flag;
    s64 mode;
    u32 n;
    u32 *k;
    u128 *vu;
    u128 *m;
    u32 w;

    Spr_Setup(2, 0);
    func_00343CA8((s32 *)&n0, (s32 *)&segs, (s32 *)id);
    seg = segs;
    func_00343128(*(u32 *)0x70000004, (void *)seg->addr, seg->qwc);
    count = n0;
    Dma_Wait(0x1000D400);
    while (count != 0) {
        buf = (u128 *)func_00342FB0();
        out = buf + 0x170;
        dst = buf + 0x171;
        if (count >= 2) {
            func_00343128(*(u32 *)0x70000004, (void *)seg[1].addr, seg[1].qwc);
        } else {
            Dma_Wait(0x1000D400);
        }
        p = buf;
        count--;
        next = seg + 1;
        do {
            hdr = *p;
            switch (((u32 *)&hdr)[3]) {
            case 2:
                dst[0] = p[0];
                dst[1] = p[1];
                dst[2] = p[2];
                w = ((u32 *)p)[8];
                done = (w & 0x8000) >> 15;
                dst += 3;
                if (flag != 0) {
                    p = func_003438B8(&dst, p + 3, w & 0x7FFF, p);
                } else if (mode == 0) {
                    p = func_003435C0(&dst, p + 3, w & 0x7FFF, p);
                } else if ((s32)(mode & 0xFFFFFFFF) != 0) {
                    p = func_003436B8(&dst, p + 3, w & 0x7FFF, (s32)(mode & 0xFFFFFFFF));
                } else {
                    p = func_003437B8(&dst, p + 3, w & 0x7FFF, mode >> 32);
                }
                break;
            case 1:
                done = 0;
                dst[0] = p[0];
                dst[1] = p[1];
                flag = ((u32 *)dst)[7];
                dst[2] = p[2];
                mode = *(s64 *)((u8 *)p + 0x28);
                dst += 3;
                p += 3;
                break;
            case 0:
                vu = (u128 *)0x110040C0;
                k = (u32 *)(p + 1);
                for (n = ((u32 *)&hdr)[0]; n != 0; n--) {
                    m = (u128 *)(mats + *k * 0xC0);
                    k += 2;
                    vu[4] = m[0];
                    vu[0] = m[4];
                    vu[8] = m[8];
                    vu[5] = m[1];
                    vu[6] = m[2];
                    vu[7] = m[3];
                    vu[1] = m[5];
                    vu[2] = m[6];
                    vu[3] = m[7];
                    vu[9] = m[9];
                    vu[10] = m[10];
                    vu += 12;
                }
                done = 1;
                break;
            }
        } while (!done);
        if (seg->size != 0) {
            *out = 0;
            *(u32 *)out = seg->size | 0x10000000;
            func_00343178((u32)out, seg->size + 1);
        } else {
            Dma_Wait(0x1000D000);
        }
        seg = next;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003439B0);
#endif

/* Reads a block header: `*a` = word 0, `*b` = the block address plus word 1; returns the data after the header. */
s32 *func_00343CA8(s32 *a, s32 *b, s32 *c) {
    *a = c[0];
    *b = c[1] + (s32)c;
    return c + 4;
}

#if 0
/* NON_MATCHING: first draft; the loop structure (the record loop restarts on a reloaded quadword), the 64-bit
 * masks and the registers differ widely. */
/* Relocates a part (header as for func_00343CA8): adds the base to each segment address, then fixes its records:
 * type 1 gets log2 of the texture sizes, type 2 gets TBP0/CBP moved by `tbp` and its vertex indices scaled by 12. */
void func_00343CC8(s32 *part, u32 tbp) {
    u32 count;
    PartSeg *segs;
    PartSeg *seg;
    u32 n;
    u8 *p;
    u128 q;
    s32 done;
    u64 tbp64;
    u64 cbp64;
    u64 tex0;
    u32 w;
    u32 h;
    s32 lw;
    s32 lh;
    u32 info;
    u32 cnt;
    u32 k;
    u8 *rec;
    s32 m;

    func_00343CA8((s32 *)&count, (s32 *)&segs, part);
    n = count;
    seg = segs;
    if (n != 0) {
        tbp64 = tbp;
        cbp64 = tbp64 << 37;
        do {
            seg->addr += (u32)(part + 4);
            p = (u8 *)seg->addr;
            do {
                q = *(u128 *)p;
                switch (((u32 *)&q)[3]) {
                case 1:
                    done = (*(u32 *)p & 0x8000) >> 15;
                    w = *(u32 *)(p + 0x28);
                    lw = 0;
                    for (k = w & 0xFF; k >= 2; k >>= 1) {
                        lw++;
                    }
                    h = *(u32 *)(p + 0x2C);
                    *(u32 *)(p + 0x28) = (w & 0xFFFFFF00) | lw;
                    lh = 0;
                    for (k = h & 0xFF; k >= 2; k >>= 1) {
                        lh++;
                    }
                    *(u32 *)(p + 0x2C) = (h & 0xFFFFFF00) | lh;
                    p += 0x30;
                    break;
                case 0:
                    done = 1;
                    break;
                case 2:
                    tex0 = *(u64 *)(p + 0x18);
                    info = *(u32 *)(p + 0x20);
                    *(u64 *)(p + 0x10) = (tex0 & 0xFFF8001FFFFFC000LL) | ((tex0 + tbp64) & 0x3FFF)
                                         | ((tex0 + cbp64) & 0x7FFE000000000LL);
                    cnt = info & 0x7FFF;
                    done = (info & 0x8000) >> 15;
                    p += 0x30;
                    for (k = 0; k < cnt; k++) {
                        rec = p;
                        m = *(u32 *)(rec + 0xC) & 7;
                        p += m * 32 + 32;
                        while (m > 0) {
                            m--;
                            ((s32 *)(rec + 0x10))[m] *= 12;
                        }
                    }
                    break;
                }
            } while (!done);
            seg++;
        } while (--n != 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00343CC8);
#endif

Unk8BF3A0 *func_00343F08(s32 i) {
    return &D_008BF3A0[i];
}

Unk8BF350 *func_00343F20(s32 i) {
    return &D_008BF350[i];
}

Unk8BF3A0 *func_00343F38(s32 i) {
    return &D_00436688[i];
}

void func_00343F50(void) {
    Unk8BF3A0 *p = D_008BF3A0;
    s32 i;

    for (i = 0; i < 26; i++) {
        p->unk0 = 0;
        p->unk2 = 0;
        p->unk4 = 0;
        p++;
    }
}

void func_00343F88(void) {
    Unk8BF3A0 *p;
    s32 i;

    for (i = 6; i < 26; i++) {
        p = &D_008BF3A0[i];
        p->unk0 = 0;
        p->unk2 = 0;
        p->unk4 = 0;
    }
    D_008BF2C0.unk18[0] = 0;
    D_008BF2C0.unk18[1] = 0;
    D_008BF2C0.unk18[2] = 0;
    D_008BF2C0.unk18[3] = 0;
    D_008BF2C0.unk18[4] = 0;
    D_008BF2C0.unk18[5] = 0;
    D_008BF2C0.unk18[6] = 0;
    D_008BF2C0.unk18[7] = 0;
    D_008BF2C0.unk18[8] = 0;
    D_008BF2C0.unk18[9] = 0;
    D_008BF2C0.unk18[10] = 0;
    D_008BF2C0.unk44 = 0;
    D_008BF308.unk18[0] = 0;
    D_008BF308.unk18[1] = 0;
    D_008BF308.unk18[2] = 0;
    D_008BF308.unk18[3] = 0;
    D_008BF308.unk18[4] = 0;
    D_008BF308.unk18[5] = 0;
    D_008BF308.unk18[6] = 0;
    D_008BF308.unk18[7] = 0;
    D_008BF308.unk18[8] = 0;
    D_008BF308.unk18[9] = 0;
    D_008BF308.unk18[10] = 0;
    D_008BF308.unk44 = 0;
}

/* Uploads frame `frame` (clamped) of record `i` to the GS through the scratchpad, or only waits for the fromSPR
 * channel when the record is not ready (flags & 3). */
void func_00344030(s32 i, s32 frame, GifPacket *pk) {
    Unk8BF3A0 *p = &D_008BF3A0[i];

    if ((*(u64 *)p & 3) != 3) {
        Dma_Wait((volatile u32 *)0x1000D000);
        return;
    }
    sceGifPkInit(pk, pk->buf);
    if (frame < 0) {
        frame = 0;
    }
    if (frame >= p->unk2) {
        frame = p->unk2 - 1;
    }
    pk->buf[0] = D_168A30[i][frame].q[0];
    sceGifPkReserve(pk, 4);
    sceGifPkRef(pk, D_00455E30, 2, 0, 0, 0);
    sceGifPkTerminate(pk);
    func_00343178((u32)pk->base, sceGifPkSize(pk));
}

/* The GIF tag func_00344150 copies (A+D, one register), read from the original executable. */
INCLUDE_RODATA("asm/nonmatchings/dma_gif", D_00519340);

typedef struct {
    u64 d[2];
} GifTag128;

/* The texture record D_008BF430: ready flags (bits 0-1 of the first doubleword) and the upload parameters. */
typedef union {
    u64 flags;
    struct {
        /* 0x00 */ u8 pad0[6];
        /* 0x06 */ u16 unk6;
        /* 0x08 */ u8 unk8;
        /* 0x09 */ u8 unk9;
        /* 0x0A */ u8 unkA;
        /* 0x0B */ u8 unkB;
        /* 0x0C */ u16 unkC;
        /* 0x0E */ u8 unkE;
    } s;
} Unk8BF430;

/* A texture block header (func_003A9488): image and CLUT sizes. */
typedef struct {
    /* 0x00 */ u8 pad0[0x16];
    /* 0x16 */ u16 unk16;
    /* 0x18 */ u8 pad18[0xC];
    /* 0x24 */ u32 unk24;
    /* 0x28 */ u8 pad28[8];
    /* 0x30 */ u32 unk30;
    /* 0x34 */ u32 unk34;
    /* 0x38 */ u8 pad38[0x18];
    /* 0x50 */ u32 unk50;
} TexBlock;

typedef struct {
    /* 0x00 */ u32 *cur;
    /* 0x04 */ u128 *base;
    /* 0x08 */ u32 pad8[6];
} Vif1Packet;

extern Unk8BF430 D_008BF430;
extern s32 D_008BF330[];
extern const GifTag128 D_00519340;

u32 Spr_Setup(u32 count, s32 reserved);
void sceVif1PkInit(Vif1Packet *pk, u32 base);
void sceVif1PkCnt(Vif1Packet *pk, u32 flag);
void sceVif1PkOpenDirectCode(Vif1Packet *pk, s32 irq);
void sceVif1PkOpenGifTag(Vif1Packet *pk, u128 tag);
void sceVif1PkAddGsAD(Vif1Packet *pk, s32 addr, u64 data);
void sceVif1PkCloseGifTag(Vif1Packet *pk);
void sceVif1PkCloseDirectCode(Vif1Packet *pk);
void sceVif1PkTerminate(Vif1Packet *pk);
u32 sceVif1PkSize(Vif1Packet *pk);
void func_00361B88(Vif1Packet *pk, s32 a, s32 b, s32 c, void *data, s32 qwc, s32 w, s32 h, s32 tw, s32 th);

/* Uploads the texture and CLUT of D_008BF330 to the GS through the scratchpad, or only waits for the fromSPR
 * channel when D_008BF430 is not ready. */
void func_00344150(void) {
    Unk8BF430 *t = &D_008BF430;
    Vif1Packet pk;
    GifTag128 tag;
    TexBlock *blk;
    TexBlock *clut;
    u8 *data;
    u32 mask;
    s32 n;

    if ((t->flags & 3) != 3) {
        Dma_Wait((volatile u32 *)0x1000D000);
        return;
    }
    blk = (TexBlock *)func_003A9488((s32)func_003A9488(D_008BF330[0], 0), 1);
    mask = 0x7FF0000;
    data = (u8 *)blk + 0x60;
    sceVif1PkInit(&pk, Spr_Setup(1, 0));
    n = blk->unk50 & 0x7FFF;
    func_00361B88(&pk, t->s.unk6, t->s.unk9, t->s.unk8, data, n,
                  blk->unk24 & 0x7FF, (blk->unk24 & mask) >> 16, 1 << t->s.unkA, 1 << t->s.unkB);
    clut = (TexBlock *)((u8 *)blk + (n * 16 + 0x60));
    func_00361B88(&pk, t->s.unkC, t->s.unkE, clut->unk16 & 0x3F, (u8 *)clut + 0x60, clut->unk50 & 0x7FFF,
                  clut->unk24 & 0x7FF, (clut->unk24 & mask) >> 16, clut->unk30 & 0x7FF, clut->unk34 & 0x7FF);
    tag = D_00519340;
    sceVif1PkCnt(&pk, 0);
    sceVif1PkOpenDirectCode(&pk, 0);
    sceVif1PkOpenGifTag(&pk, *(u128 *)&tag);
    sceVif1PkAddGsAD(&pk, 0x3F, 0);
    sceVif1PkCloseGifTag(&pk);
    sceVif1PkCloseDirectCode(&pk);
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.base, sceVif1PkSize(&pk));
}

/* Not written in C yet (T13 batch 3): 452 instructions building the VU1 packets of the side textures. */
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00344318);

typedef struct {
    /* 0x00 */ u8 pad0[0x1E];
    /* 0x1E */ s16 unk1E;
    /* 0x20 */ u8 pad20[2];
    /* 0x22 */ s16 unk22;
    /* 0x24 */ u8 pad24[6];
    /* 0x2A */ s16 unk2A;
} Unk344A28;

s32 func_00367250(s32 id);
void func_00344318(void);

/* Looks up the data blocks of an object (func_00367250) for both sides into D_008BF308 / D_008BF2C0, in the first
 * or second slot pair by unk1E; types 0x14, 0x1D and 0x21 use a second level of blocks. */
void func_00344A28(Unk344A28 *o) {
    s32 id = func_00367250(o->unk22);
    s32 a;
    s32 b;

    if (id != 0) {
        if (o->unk2A == 0x14 || o->unk2A == 0x1D || o->unk2A == 0x21) {
            a = (s32)func_003A9488(id, 0);
            b = (s32)func_003A9488(id, 1);
        } else {
            a = id;
            b = 0;
        }
        if (o->unk1E == 0) {
            D_008BF308.unk18[0] = (s32)func_003A9488(a, 0);
            D_008BF2C0.unk18[0] = (s32)func_003A9488(a, 1);
            D_008BF308.unk18[1] = (s32)func_003A9488(a, 0);
            D_008BF2C0.unk18[1] = (s32)func_003A9488(a, 1);
            if (b != 0) {
                D_008BF308.unk18[8] = (s32)func_003A9488(b, 0);
                D_008BF308.unk18[9] = (s32)func_003A9488(b, 0);
            } else {
                D_008BF308.unk18[8] = 0;
                D_008BF308.unk18[9] = 0;
            }
        } else {
            D_008BF308.unk18[2] = (s32)func_003A9488(a, 0);
            D_008BF2C0.unk18[2] = (s32)func_003A9488(a, 1);
            D_008BF308.unk18[3] = (s32)func_003A9488(a, 0);
            D_008BF2C0.unk18[3] = (s32)func_003A9488(a, 1);
            if (b != 0) {
                D_008BF308.unk18[10] = (s32)func_003A9488(b, 0);
                D_008BF308.unk44 = (s32)func_003A9488(b, 0);
            } else {
                D_008BF308.unk18[10] = 0;
                D_008BF308.unk44 = 0;
            }
        }
        func_00344318();
    }
}

#if 0
/* The two fighters' entries of D_00440940 (0x60 bytes each). */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 pad8[0x28];
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ u8 pad38[0x28];
} Unk440940;

extern Unk440940 D_00440940[2];
extern u8 D_0043B7F8[];
extern u16 *D_008BF33C[];
s32 func_003ABCC0(void);
u32 func_00364550(u16 *blk);
void func_003A25C0(void);

/* NON_MATCHING: the loop counter and the 0x60 stride constant are in swapped registers (a1/a2). */
/* Picks the extra texture set (slot 7) from the two fighters' entries (types 0x13, 0xF and 0x21 paired with 0x1D),
 * marks records 12-13, then rebuilds the packets (func_00344318) and adds the object func_003A25C0. */
void func_00344BE0(void) {
    u32 mask;
    s32 i;
    s32 t;
    u32 n;
    s32 k;

    D_008BF350[9].unk0 = NULL;
    D_008BF350[9].unk4 = 0;
    if (func_003BB4D8() == 0 && func_003ABCC0() == 0 && (u32)(D_0043B7F8[0] - 5) >= 2) {
        mask = 0;
        for (i = 0; i < 2; i++) {
            t = D_00440940[i].unk0;
            if ((t == 0x13 || t == 0xF || t == 0x21) && D_00440940[i].unk30 == 0x1D && D_00440940[i].unk34 != 4
                && gFighters[i].unk402 == 0) {
                mask |= (1 << (i * 2)) | (1 << (i * 2 + 1));
            }
            t = D_00440940[i].unk30;
            if ((t == 0x13 || t == 0xF || t == 0x21) && D_00440940[i].unk0 == 0x1D && D_00440940[i].unk4 != 4
                && gFighters[i].unk402 == 0) {
                mask |= 1 << (i * 2);
            }
        }
        if (mask & 1) {
            if (!(mask & 2)) {
                D_008BF2C0.unk18[7] = D_008BF2C0.unk18[0];
                D_008BF308.unk18[7] = D_008BF308.unk18[0];
            } else {
                D_008BF2C0.unk18[7] = D_008BF2C0.unk18[1];
                D_008BF308.unk18[7] = D_008BF308.unk18[1];
            }
        } else if (mask & 4) {
            if (!(mask & 8)) {
                D_008BF2C0.unk18[7] = D_008BF2C0.unk18[2];
                D_008BF308.unk18[7] = D_008BF308.unk18[2];
            } else {
                D_008BF2C0.unk18[7] = D_008BF2C0.unk18[3];
                D_008BF308.unk18[7] = D_008BF308.unk18[3];
            }
        }
        if (mask & 5) {
            n = func_00364550(D_008BF33C[0]);
            if (n >= 2) {
                n = 0;
            }
            for (k = 0; k < n; k++) {
                D_008BF3A0[k + 12].unk0 |= 1;
            }
            D_008BF350[9].unk0 = (u16 *)D_008BF2C0.unk18[7];
            D_008BF350[9].unk4 = *(u16 *)D_008BF2C0.unk18[7];
        }
    }
    func_00344318();
    func_003464F8(func_003A25C0);
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00344BE0);
#endif

/* As func_00344A28 for one slot only, chosen by the two low bits of unk22. */
void func_00344EA8(Unk344A28 *o) {
    s32 id = func_00367250(o->unk22);
    s32 a;
    s32 b;
    u16 sel;

    if (id != 0) {
        if (o->unk2A == 0x14 || o->unk2A == 0x1D || o->unk2A == 0x21) {
            a = (s32)func_003A9488(id, 0);
            b = (s32)func_003A9488(id, 1);
        } else {
            a = id;
            b = 0;
        }
        sel = o->unk22;
        if (!(sel & 1)) {
            if (!(sel & 2)) {
                D_008BF308.unk18[0] = (s32)func_003A9488(a, 0);
                D_008BF2C0.unk18[0] = (s32)func_003A9488(a, 1);
                if (b != 0) {
                    D_008BF308.unk18[8] = (s32)func_003A9488(b, 0);
                } else {
                    D_008BF308.unk18[8] = 0;
                }
            } else {
                D_008BF308.unk18[1] = (s32)func_003A9488(a, 0);
                D_008BF2C0.unk18[1] = (s32)func_003A9488(a, 1);
                if (b != 0) {
                    D_008BF308.unk18[9] = (s32)func_003A9488(b, 0);
                } else {
                    D_008BF308.unk18[9] = 0;
                }
            }
        } else {
            if (!(sel & 2)) {
                D_008BF308.unk18[2] = (s32)func_003A9488(a, 0);
                D_008BF2C0.unk18[2] = (s32)func_003A9488(a, 1);
                if (b != 0) {
                    D_008BF308.unk18[10] = (s32)func_003A9488(b, 0);
                } else {
                    D_008BF308.unk18[10] = 0;
                }
            } else {
                D_008BF308.unk18[3] = (s32)func_003A9488(a, 0);
                D_008BF2C0.unk18[3] = (s32)func_003A9488(a, 1);
                if (b != 0) {
                    D_008BF308.unk44 = (s32)func_003A9488(b, 0);
                } else {
                    D_008BF308.unk44 = 0;
                }
            }
        }
    }
}

#if 0
u16 *func_00367868(void);
u32 func_00364550(u16 *blk);

/* NON_MATCHING: first draft; the three cases share their tail in the original and the registers differ. */
/* Marks the stage texture records 6-13 from the stage data (func_00367868): up to three blocks, each with up to two
 * texture sets for the two sides. */
void func_00345070(void) {
    u16 *data;
    u16 *blk;
    u16 *p;
    s32 n;
    s32 i;
    s32 k;
    u32 cnt;
    u32 sub;

    data = func_00367868();
    for (i = 6; i < 14; i++) {
        D_008BF3A0[i].unk0 = 0;
    }
    if (data != NULL) {
        n = func_00364550(data);
        if ((u32)n < 4) {
            for (i = 0; i < n; i++) {
                blk = func_003A9488((s32)data, i);
                cnt = func_00364550(blk);
                switch (i) {
                case 0:
                    p = func_003A9488((s32)blk, 0);
                    D_008BF308.unk18[4] = (s32)p;
                    sub = func_00364550(p);
                    if (sub >= 3) {
                        sub = 0;
                    }
                    for (k = 0; k < sub; k++) {
                        D_008BF3A0[k + 6].unk0 |= 1;
                    }
                    if (cnt >= 2) {
                        p = func_003A9488((s32)blk, 1);
                        D_008BF2C0.unk18[4] = (s32)p;
                        D_008BF350[i + 6].unk0 = p;
                        D_008BF350[i + 6].unk4 = *p;
                    }
                    break;
                case 1:
                    p = func_003A9488((s32)blk, 0);
                    D_008BF308.unk18[5] = (s32)p;
                    sub = func_00364550(p);
                    if (sub >= 3) {
                        sub = 0;
                    }
                    for (k = 0; k < sub; k++) {
                        D_008BF3A0[k + 8].unk0 |= 1;
                    }
                    if (cnt >= 2) {
                        p = func_003A9488((s32)blk, 1);
                        D_008BF2C0.unk18[5] = (s32)p;
                        D_008BF350[i + 6].unk0 = p;
                        D_008BF350[i + 6].unk4 = *p;
                    }
                    break;
                case 2:
                    p = func_003A9488((s32)blk, 0);
                    D_008BF308.unk18[6] = (s32)p;
                    sub = func_00364550(p);
                    if (sub >= 3) {
                        sub = 0;
                    }
                    for (k = 0; k < sub; k++) {
                        D_008BF3A0[k + 10].unk0 |= 1;
                    }
                    if (cnt >= 2) {
                        p = func_003A9488((s32)blk, 1);
                        D_008BF2C0.unk18[6] = (s32)p;
                        D_008BF350[i + 6].unk0 = p;
                        D_008BF350[i + 6].unk4 = *p;
                    }
                    break;
                }
            }
            if (func_003BB4D8() != 0) {
                p = (u16 *)func_003B8820();
                if (p != NULL) {
                    D_008BF308.unk18[5] = (s32)func_003A9488((s32)func_003A9488((s32)p, 0), 0);
                    D_008BF3A0[8].unk0 |= 1;
                    D_008BF308.unk18[6] = (s32)func_003A9488((s32)func_003A9488((s32)p, 1), 0);
                    D_008BF3A0[10].unk0 |= 1;
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00345070);
#endif

void *func_003453F0(void) {
    return D_100000;
}

s32 func_00345400(void) {
    return 0x68A28;
}

/* Looks up the six data blocks of both sides (func_003A9488) and marks the first six records of D_008BF3A0. */
void func_00345410(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f) {
    D_008BF308.ptr[0] = func_003A9488(a, 0);
    D_008BF308.ptr[1] = func_003A9488(b, 0);
    D_008BF308.ptr[2] = func_003A9488(c, 0);
    D_008BF308.ptr[3] = func_003A9488(d, 0);
    D_008BF308.ptr[4] = func_003A9488(e, 0);
    D_008BF308.ptr[5] = func_003A9488(f, 0);
    D_008BF2C0.ptr[0] = func_003A9488(a, 1);
    D_008BF2C0.ptr[1] = func_003A9488(b, 1);
    D_008BF2C0.ptr[2] = func_003A9488(c, 1);
    D_008BF2C0.ptr[3] = func_003A9488(d, 1);
    D_008BF2C0.ptr[4] = 0;
    D_008BF2C0.ptr[5] = 0;
    D_008BF350[0].unk0 = D_008BF2C0.ptr[0];
    D_008BF350[0].unk4 = *D_008BF2C0.ptr[0];
    D_008BF350[1].unk0 = D_008BF2C0.ptr[2];
    D_008BF350[1].unk4 = *D_008BF2C0.ptr[2];
    D_008BF350[2].unk0 = D_008BF2C0.ptr[3];
    D_008BF350[2].unk4 = *D_008BF2C0.ptr[3];
    D_008BF350[3].unk0 = D_008BF2C0.ptr[1];
    D_008BF350[3].unk4 = *D_008BF2C0.ptr[1];
    D_008BF3A0[0].unk0 |= 1;
    D_008BF3A0[1].unk0 |= 1;
    D_008BF3A0[3].unk0 |= 1;
    D_008BF3A0[2].unk0 |= 1;
    D_008BF3A0[4].unk0 |= 1;
    D_008BF3A0[5].unk0 |= 1;
}

void func_003455D0(void) {
    D_008BF2C0.ptr[0] = 0;
    D_008BF2C0.ptr[1] = 0;
    D_008BF2C0.ptr[2] = 0;
    D_008BF2C0.ptr[3] = 0;
    D_008BF2C0.ptr[4] = 0;
    D_008BF2C0.ptr[5] = 0;
    D_008BF2C0.unk18[0] = 0;
    D_008BF2C0.unk18[1] = 0;
    D_008BF2C0.unk18[2] = 0;
    D_008BF2C0.unk18[3] = 0;
    D_008BF2C0.unk18[4] = 0;
    D_008BF2C0.unk18[5] = 0;
    D_008BF2C0.unk18[6] = 0;
    D_008BF2C0.unk18[7] = 0;
    D_008BF2C0.unk18[8] = 0;
    D_008BF2C0.unk18[9] = 0;
    D_008BF2C0.unk18[10] = 0;
    D_008BF2C0.unk44 = 0;
    D_008BF308.ptr[0] = 0;
    D_008BF308.ptr[1] = 0;
    D_008BF308.ptr[2] = 0;
    D_008BF308.ptr[3] = 0;
    D_008BF308.ptr[4] = 0;
    D_008BF308.ptr[5] = 0;
    D_008BF308.unk18[0] = 0;
    D_008BF308.unk18[1] = 0;
    D_008BF308.unk18[2] = 0;
    D_008BF308.unk18[3] = 0;
    D_008BF308.unk18[4] = 0;
    D_008BF308.unk18[5] = 0;
    D_008BF308.unk18[6] = 0;
    D_008BF308.unk18[7] = 0;
    D_008BF308.unk18[8] = 0;
    D_008BF308.unk18[9] = 0;
    D_008BF308.unk18[10] = 0;
    D_008BF308.unk44 = 0;
    func_00343F50();
}

void func_00345678(void) {
    s32 p = func_003B8820();

    if (p != 0) {
        func_0035D028(p, 0x24E00);
    }
}

/* Default state of an object: no rotation, unit scales, colour 0x80, 0x3C at +0xB6. */
void func_003456B0(Unk3456B0 *p) {
    Unk3456B0Sub *s = &p->sub;

    s->unk0 = 0;
    s->unk4 = 0;
    s->unk8 = 0;
    s->unkC = 1.0f;
    s->unk10 = 0;
    s->unk14[0] = 0;
    s->unk14[1] = 0;
    s->unk14[2] = 0;
    s->unk14[3] = 0;
    s->unk14[4] = 0;
    s->unk14[5] = 0;
    s->unk14[6] = 0;
    s->unk14[7] = 0;
    s->unk30[0] = 1.0f;
    s->unk30[1] = 1.0f;
    s->unk30[2] = 1.0f;
    s->unk30[3] = 1.0f;
    s->unk40 = 1;
    s->unk44 = 128.0f;
    s->unk48 = 400.0f;
    s->unk4C = 0;
    s->unk50 = 0x80;
    s->unk51 = 0x80;
    s->unk52 = 0x80;
    s->unk54 = 0;
    s->unk58 = 1.0f;
    s->unk60[0] = 0;
    s->unk60[1] = 0;
    s->unk60[2] = 0;
    s->unk60[3] = 1.0f;
    s->unk70[0] = 0;
    s->unk70[1] = 0;
    s->unk70[2] = 0;
    s->unk70[3] = 1.0f;
    s->unk80[0] = 0;
    s->unk80[1] = 0;
    s->unk80[2] = 0;
    s->unk80[3] = 1.0f;
    s->unk90 = 0;
    s->unk94 = 0;
    s->unk96 = 0x3C;
}

/* Defined without a prototype: func_00346030 calls it with an argument it ignores. */
void *func_00345780() {
    void *p = func_003464F8();

    if (p != NULL) {
        func_003456B0(p);
        return p;
    }
    return NULL;
}

#if 0
/* Two GS vertices (ST and XYZ register values) of a line, read as words and as fields. */
typedef union {
    u64 d[8];
    struct {
        /* 0x00 */ u16 x0;
        /* 0x02 */ u16 pad2[3];
        /* 0x08 */ u64 unk8;
        /* 0x10 */ f32 q0;
        /* 0x14 */ u32 pad14;
        /* 0x18 */ u64 unk18;
        /* 0x20 */ u16 x1;
        /* 0x22 */ u16 pad22[3];
        /* 0x28 */ u64 unk28;
        /* 0x30 */ f32 q1;
        /* 0x34 */ u32 pad34;
        /* 0x38 */ u64 unk38;
    } s;
} GsLine;

/* NON_MATCHING: same logic; register allocation of the t registers differs (decomp-permuter: 3470 -> 1425). */
/* Builds the GS packet of a line split into 1-16 segments by its length (interpolating x and q), and sends it
 * through the scratchpad. */
void func_003457C0(u8 *pk, GsLine *v, s32 i) {
    Unk8BF3A0 *rec = &D_008BF3A0[i];
    s32 d;
    u32 n;
    s32 shift;
    s32 qwc;
    u64 *q;
    u64 *p;
    f32 step;
    u16 x0;
    s32 dx;
    u32 k;

    d = v->s.x1 - v->s.x0;
    if (d < 0) {
        d = -d;
    }
    if (d < 0x400) {
        n = 1;
        shift = 0;
    } else if (d < 0x1000) {
        n = 4;
        shift = 2;
    } else if (d < 0x2000) {
        n = 8;
        shift = 3;
    } else {
        n = 0x10;
        shift = 4;
    }
    qwc = n * 2 + 7;
    ((u16 *)pk)[1] = 0x1000;
    ((u32 *)pk)[2] = 0x11000000;
    ((u32 *)pk)[3] = qwc | 0x50000000;
    ((u64 *)pk)[2] = 0x112B400000000005LL;
    ((u16 *)pk)[0] = qwc;
    ((u32 *)pk)[1] = 0;
    q = (u64 *)(pk + 0x10);
    q[1] = 0xE;
    q[3] = 7;
    q[2] = *(u64 *)((u8 *)rec + 0x10);
    q[5] = 0x43;
    q[4] = v->s.unk18;
    q[6] = 0x53005;
    q[7] = 0x48;
    q[8] = 5;
    q[9] = 9;
    q[13] = 0x5252;
    q[10] = v->s.unk8;
    q[11] = 1;
    q[12] = n | 0x8000 | 0x4400000000000000LL;
    step = v->s.q1 - v->s.q0;
    if (n != 1) {
        step /= n;
    }
    x0 = v->s.x0;
    dx = v->s.x1 - x0;
    v->s.q1 = v->s.q0;
    v->s.x1 = x0;
    p = (u64 *)(pk + 0x80);
    for (k = 0; k < n; k++) {
        v->s.q0 = v->s.q1;
        v->s.x0 = v->s.x1;
        v->s.q1 = v->s.q0 + step;
        v->s.x1 = x0 + ((dx * (s32)(k + 1)) >> shift);
        p[0] = v->d[2];
        p[1] = v->d[0];
        p[2] = v->d[6];
        p[3] = v->d[4];
        p += 4;
    }
    func_00343178((u32)pk, ((u8 *)p - pk) >> 4);
}
#else
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003457C0);
#endif

/* Not written in C yet (T13 batch 3): 378 instructions, scratchpad matrices (sceVu0*), unaligned 64-bit copies. */
INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003459D0);

s32 func_00345FB8(void) {
    return 0x2C000;
}

void func_00345FC8(void) {
    s32 i;
    s32 j;

    for (i = 0; i < 2; i++) {
        D_008C2788[i].next = 0;
        D_008C2788[i].oldest = 0;
        for (j = 0; j < 0x100; j++) {
            D_008C2788[i].slot[j] = NULL;
        }
    }
}

/* Gets an object for `side`: a new one (func_00345780) in a free slot of that side's ring or, failing that, of the
 * other ring; with no free slot, the oldest object of the side is reset and reused. */
Unk3456B0 *func_00346030(s32 side, s32 arg) {
    Unk8C2788 *own;
    Unk8C2788 *other;
    s32 found = -1;
    s32 i;
    s32 n;
    s32 off;
    Unk3456B0 *o;

    side &= 1;
    own = &D_008C2788[side];
    off = 0x404;
    if (side) {
        off = 0;
    }
    other = (Unk8C2788 *)((u8 *)D_008C2788 + off);
    i = own->next;
    for (n = 0; n < 0x100; n++) {
        if (own->slot[i] == NULL) {
            found = i;
            break;
        }
        i++;
        if (i >= 0x100) {
            i = 0;
        }
    }
    if (found < 0) {
        i = other->next;
        for (n = 0; n < 0x100; n++) {
            if (other->slot[i] == NULL) {
                found = i | 0x8000;
                break;
            }
            i++;
            if (i >= 0x100) {
                i = 0;
            }
        }
        if (found < 0) {
            i = own->oldest;
            own->oldest++;
            if (own->oldest >= 0x100) {
                own->oldest = 0;
            }
            o = own->slot[i];
            if (o != NULL) {
                o->unk8 = arg;
                o->unk14 = 0;
                o->unk18 = 0;
                o->unk1A = 0;
                o->unk1C = 0;
                o->unkC = 0;
            }
            func_003456B0(o);
            return o;
        }
    }
    o = func_00345780(arg);
    if (o != NULL) {
        if (!(found & 0x8000)) {
            own->slot[found] = o;
            o->unk10 = &own->slot[found];
            own->next++;
            if (own->next >= 0x100) {
                own->next = 0;
            }
        } else {
            s32 k = found & 0x7FFF;

            other->slot[k] = o;
            o->unk10 = &other->slot[k];
            other->next++;
            if (other->next >= 0x100) {
                other->next = 0;
            }
        }
    }
    return o;
}

void func_00346210(void) {
    D_008C2F90[0].count = 0;
    D_008C2F90[1].count = 0;
    D_008C2F90[0].unkC04 = 0;
    D_008C2F90[1].unkC04 = 0;
}

typedef struct {
    /* 0x00 */ f32 unk0;
    /* 0x04 */ f32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ u8 padC[0xD4];
    /* 0xE0 */ f32 unkE0;
    /* 0xE4 */ f32 unkE4;
    /* 0xE8 */ f32 unkE8;
} Unk346230Sub;

typedef struct {
    /* 0x00 */ u8 pad0[0x14];
    /* 0x14 */ s32 unk14;
    /* 0x18 */ u8 pad18[8];
    /* 0x20 */ Unk346230Sub sub;
} Unk346230;

/* Appends an entry (six words) to the list of the object's side (bit 5 of unk14), up to 0x7F. */
void func_00346230(Unk346230 *o) {
    Unk346230Sub *s = &o->sub;
    Unk8C2F90 *l = &D_008C2F90[(o->unk14 >> 5) & 1];
    s32 n = l->count;
    s32 *e;

    if (n < 0x7F) {
        l->count = n + 1;
        e = (s32 *)&l->unk4[n * 0x18];
        e[0] = s->unkE0;
        e[1] = -(s32)s->unkE4;
        e[2] = -(s32)s->unkE8;
        e[3] = o->sub.unk0;
        e[4] = -(s32)s->unk4;
        e[5] = -(s32)s->unk8;
    }
}

void *func_003462F0(s32 side) {
    return D_008C2F90[side & 1].unk4;
}

s32 func_00346310(s32 side) {
    return D_008C2F90[side & 1].count;
}

void func_00346330(s32 side) {
    D_008C2F90[side & 1].unkC04 = 1;
}

void func_00346358(s32 side) {
    D_008C2F90[side & 1].unkC04 = 0;
}

u16 func_00346378(s32 side) {
    return D_008C2F90[side & 1].unkC04;
}
