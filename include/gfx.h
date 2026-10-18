#ifndef GFX_H
#define GFX_H

#include "types.h"

/* libgraph structures, as far as the game code uses them (Sony's sceGs* layouts). */
typedef struct {
    u64 EN1 : 1;
    u64 EN2 : 1;
    u64 CRTMD : 3;
    u64 MMOD : 1;
    u64 AMOD : 1;
    u64 SLBG : 1;
    u64 ALP : 8;
    u64 zero : 16;
    u64 p0 : 32;
} GsPmode;

typedef struct {
    u64 DX : 12;
    u64 DY : 11;
    u64 MAGH : 4;
    u64 MAGV : 2;
    u64 p0 : 3;
    u64 DW : 12;
    u64 DH : 11;
    u64 p1 : 9;
} GsDisplay;

/* sceGsDispEnv */
typedef struct {
    /* 0x00 */ GsPmode pmode;
    /* 0x08 */ u64 smode2;
    /* 0x10 */ u64 dispfb;
    /* 0x18 */ GsDisplay display;
    /* 0x20 */ u64 bgcolor;
} GsDispEnv;

/* sceGsDBuff: two display environments, then per buffer a GIF tag, sceGsDrawEnv1 and sceGsClear. */
typedef struct {
    /* 0x000 */ GsDispEnv disp[2];
    /* 0x050 */ u64 giftag0[2];
    /* 0x060 */ u64 draw0[16];
    /* 0x0E0 */ u64 clear0[4];
    /* 0x100 */ u8 clear0Rgbaq[8];
    /* 0x108 */ u64 clear0b[7];
    /* 0x140 */ u64 giftag1[2];
    /* 0x150 */ u64 draw1[16];
    /* 0x1D0 */ u64 clear1[4];
    /* 0x1F0 */ u8 clear1Rgbaq[8];
    /* 0x1F8 */ u64 clear1b[7];
} GsDBuff;

/* Display state (gGfx, 0x8BD0A0). */
typedef struct {
    /* 0x000 */ GsDBuff dbuff;
    /* 0x230 */ u64 dispfb[2];   /* DISPFB of each buffer, written to the GS by the VBlank handler */
    /* 0x240 */ GsDisplay display[2];
    /* 0x250 */ s32 field;       /* 1 on the even field (GS CSR FIELD clear) */
    /* 0x254 */ volatile s32 vsyncCount;  /* +1 per VBLANK_START interrupt */
    /* 0x258 */ s32 unk258;      /* field of the last buffer swap */
    /* 0x25C */ s32 parity;      /* buffer shown; counts up while unk26E is 0 */
    /* 0x260 */ s32 handlerId;   /* AddIntcHandler result */
    /* 0x264 */ s32 unk264;      /* swap even without a new frame */
    /* 0x268 */ s16 dx;
    /* 0x26A */ s16 dy;
    /* 0x26C */ u8 phase;        /* Gfx_SetPhase: 0 frame start, 1 draw, 2 end */
    /* 0x26D */ u8 unk26D;
    /* 0x26E */ u8 unk26E;       /* 1: single displayed buffer */
    /* 0x26F */ u8 unk26F;
} GfxState;

/* An entry of the DMA packet table D_004362A0: `qwc` quadwords at `addr`. */
typedef struct {
    /* 0x0 */ u16 qwc;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ void *addr;
    /* 0x8 */ u32 unk8[2];
} DmaPacketRef;

/* A DMA tag quadword in a packet being built. */
typedef union {
    u128 q;
    u16 h[8];
    u32 w[4];
} DmaTag;

/* D_008BD310: the two VIF1 packet buffers of the draw lists. */
typedef struct {
    /* 0x00 */ u8 unk0[0x40];
    /* 0x40 */ u32 unk40;
    /* 0x44 */ u32 unk44;
    /* 0x48 */ u32 unk48;
    /* 0x4C */ u32 unk4C;
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s32 unk54;
    /* 0x58 */ s32 unk58;
    /* 0x5C */ s32 unk5C;
    /* 0x60 */ s32 unk60[2];
    /* 0x68 */ s32 unk68[2];
    /* 0x70 */ s32 unk70;
    /* 0x74 */ s32 unk74;
    /* 0x78 */ s32 unk78;
    /* 0x7C */ s32 unk7C;
} Unk8BD310;

typedef struct {
    /* 0x0 */ u32 unk0;
    /* 0x4 */ s32 unk4;
} Unk8BD390;

#endif
