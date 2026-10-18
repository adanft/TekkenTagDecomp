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
    /* 0x00 */ u8 pad0[0x20];
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
Unk3456B0 *func_003464F8(void);
void func_003456B0(Unk3456B0 *p);
u16 *func_003A9488(s32 id, s32 side);
void func_00343F50(void);
void func_0035E1C8(u32 addr);
s32 func_003BB4D8(void);
s32 func_003ABCD0(void);

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

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003430D8);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00343128);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00343178);

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

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00343340);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003435C0);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003436B8);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003437B8);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003438B8);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003439B0);

/* Reads a block header: `*a` = word 0, `*b` = the block address plus word 1; returns the data after the header. */
s32 *func_00343CA8(s32 *a, s32 *b, s32 *c) {
    *a = c[0];
    *b = c[1] + (s32)c;
    return c + 4;
}

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00343CC8);

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

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00344030);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00344150);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00344318);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00344A28);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00344BE0);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00344EA8);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00345070);

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

void *func_00345780(void) {
    void *p = func_003464F8();

    if (p != NULL) {
        func_003456B0(p);
        return p;
    }
    return NULL;
}

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003457C0);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_003459D0);

s32 func_00345FB8(void) {
    return 0x2C000;
}

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00345FC8);

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00346030);

void func_00346210(void) {
    D_008C2F90[0].count = 0;
    D_008C2F90[1].count = 0;
    D_008C2F90[0].unkC04 = 0;
    D_008C2F90[1].unkC04 = 0;
}

INCLUDE_ASM("asm/nonmatchings/dma_gif", func_00346230);

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
