/* 0x398510-0x3994C0: graphics core (GS setup, VBlank handler, frame pacing) and small helpers in front of it.
 * docs/game_overview.md section 2. */
#include "common.h"
#include "gfx.h"

extern GfxState gGfx;
extern volatile s32 gFrameReady;
extern DmaPacketRef D_004362A0[];
extern u32 D_004364C0[][2];
extern u64 D_004364D0[];
extern u64 D_004364E8[];
extern Unk8BD310 D_008BD310;
extern Unk8BD390 D_008BD390[];
extern Unk8BD390 D_008BEC90[];
extern u64 D_00518BD0[];
extern u64 D_00518BE0[];
extern u64 D_00518BF0[];
extern u64 D_00518BF8[];
extern u64 D_00518C00[];
extern u64 D_00518C08[];
extern u8 D_004F5F00[];
extern u8 D_004F7A00[];

float sinf(float x);
float sqrtf(float x);
void FlushCache(s32 op);
s32 AddIntcHandler(s32 cause, s32 (*handler)(s32), s32 next);
s32 RemoveIntcHandler(s32 cause, s32 id);
s32 EnableIntc(s32 cause);
s32 DisableIntc(s32 cause);
void sceGsResetGraph(s32 mode, s32 inter, s32 omode, s32 ffmode);
void sceGsSetDefDBuff(GsDBuff *db, s32 psm, s32 w, s32 h, s32 ztest, s32 zpsm, s32 clear);
void sceGsSetDefDispEnv(GsDispEnv *disp, s16 psm, s16 w, s16 h, s16 dx, s16 dy);
void sceGsPutDispEnv(GsDispEnv *disp);
s32 sceGsSwapDBuff(GsDBuff *db, s32 id);
s32 sceGsSyncV(s32 mode);
void func_0039BC00(s32 x);
s32 func_0039BC08(s16 *v, s32 *a, s32 *b, s32 *c);
void Gte_RotMatrixZYX(s16 *v, s32 arg);
void func_003430D8(void *dst, DmaPacketRef *ref);
void func_00343128(void *dst, void *src, s32 qwc);
void func_00343178(void *addr, s32 qwc);
void Dma_SendChain(u32 channel, void *chain);
void Dma_Wait(u32 channel);
void func_00342E38(void);
void func_0039C788(void);
void func_003637D0(void);
void func_00399CC8(void);
u32 func_003639B8(void);
u32 func_003639C0(void);
void func_00399E50(void *addr, s32 a, s32 b);
void Pad_Init(void);
s32 Rng_SpinNext(void);
s32 func_0035E198(void);
void func_0035E6B8(DmaPacketRef *ref);
void Spr_Setup(s32 a, s32 b);
DmaPacketRef *Dma_GetPacket(s32 i);
void func_00398858(u32 addr, s32 a, s32 b);
void Gfx_InstallVSyncHandler(s32 mode);
s32 Gfx_VSyncHandler(s32 cause);
void Gfx_PutDispEnv(GsDispEnv *disp);
void Gfx_SetDispEnv(GsDispEnv *disp, s32 psm, s32 w, s32 h, s32 dx, s32 dy, s32 second);
void func_003993B0(void);
void func_00399418(void);
void func_00399308(void);
void func_003990A8(s32 unused);
void func_00399488(void);

s32 func_00398510(void) {
    return 0;
}

s32 func_00398518(void) {
    return 0;
}

s32 func_00398520(void) {
    return 0;
}

s32 func_00398528(void) {
    return 0;
}

s32 func_00398530(void) {
    return 0;
}

s32 func_00398538(void) {
    return 0;
}

void func_00398540(s32 x) {
    func_0039BC00(x);
}

/* Square root in 20.12 fixed point. */
s32 func_00398548(s32 x) {
    f32 f = x * (1.0f / 4096.0f);

    do {
    } while (0); /* fake (decomp-permuter): keeps the 4096.0f load after the sqrt.s */
    return sqrtf(f) * 4096.0f;
}

/* Sine in 20.12 fixed point of an angle in 1/4096 turns. */
s32 func_00398588(s32 angle) {
    return sinf(angle * 3.141592f * 2.0f * (1.0f / 4096.0f)) * 4096.0f;
}

s32 func_003985D8(s32 x) {
    return sqrtf(x);
}

s32 func_00398600(s32 x, s16 y, s16 z, u16 *out) {
    s16 v[3];
    s32 ret;
    s32 a;
    s32 b;
    s32 c;

    v[0] = x;
    v[1] = y;
    v[2] = z;
    ret = func_0039BC08(v, &a, &b, &c);
    *out = a;
    return ret;
}

s32 func_00398658(s16 *v, s32 *out) {
    s32 ret;
    s32 a;
    s32 b;
    s32 c;

    ret = func_0039BC08(v, &a, &b, &c);
    *out = a;
    return ret;
}

void func_00398690(s16 x, s16 y, s16 z, s32 arg) {
    s16 v[3];

    v[0] = x >> 4;
    v[1] = y >> 4;
    v[2] = z >> 4;
    Gte_RotMatrixZYX(v, arg);
}

void func_003986D8(s16 *v, s32 arg) {
    func_00398690(v[0], v[1], v[2], arg);
}

s32 func_003986F0(void) {
    return 0x10000;
}

s32 func_003986F8(void) {
    return 0x60000;
}

DmaPacketRef *Dma_GetPacket(s32 i) {
    return &D_004362A0[i];
}

/* Appends the DMA tag of packet `i` (read through the uncached accelerated mapping) to `*pkt` as a REF tag. */
void Dma_AddRefTag(DmaTag **pkt, s32 i) {
    DmaTag *p = *pkt;

    p->q = ((DmaTag *)((u32)D_004362A0 | 0x30000000))[i].q;
    p->h[1] = 0x3000;
    p++;
    *pkt = p;
}

/* Turns the tag at `*pkt` into a CNT tag and skips it and its data. */
void Dma_CloseCntTag(u128 **pkt) {
    u128 *p = *pkt;

    ((u16 *)p)[1] = 0x1000;
    *pkt = p + (((u16 *)p)[0] + 1);
}

/* Copies packet `i` to `*pkt` through toSPR DMA and closes it as a CNT tag. */
void func_00398778(u128 **pkt, s32 i) {
    func_003430D8(*pkt, Dma_GetPacket(i));
    Dma_Wait(0x1000D400);
    Dma_CloseCntTag(pkt);
}

void func_003987C0(void *dst, s32 i) {
    DmaPacketRef *ref = Dma_GetPacket(i);

    func_00343128(dst, ref->addr, ref->qwc);
    Dma_Wait(0x1000D400);
}

void func_00398800(u8 **pkt, DmaPacketRef *ref) {
    s32 qwc = ref->qwc;

    func_00343128(*pkt, ref->addr, qwc);
    *pkt += qwc * 16;
    Dma_Wait(0x1000D400);
}

/* Writes three GS register values into the packet at `addr` (uncached). */
void func_00398858(u32 addr, s32 a, s32 b) {
    u64 *p = (u64 *)(addr | 0x20000000);

    if (gGfx.unk26E) {
        u64 x = D_00518BF8[0];
        u64 y = D_00518C00[0];
        u64 z = D_00518C08[0];

        p[6] = z;
        p[4] = y;
        p[2] = x;
    } else {
        u64 x = D_00518BD0[a];
        u64 y = D_00518BE0[b];
        u64 z = D_00518BF0[0];

        p[6] = z;
        p[4] = y;
        p[2] = x;
    }
    EE_SYNC_L();
}

/* GS address (in words / 64) of the frame buffer shown. */
u32 func_003988E0(void) {
    return (u32)gGfx.dispfb[gGfx.parity & 1] << 5;
}

s32 Gfx_GetVSyncCount(void) {
    return gGfx.vsyncCount;
}

s32 func_00398918(void) {
    return gGfx.unk258;
}

s32 func_00398928(void) {
    return gGfx.parity;
}

void func_00398938(void) {
}

/* GS reset (interlaced NTSC, FRAME mode, 640x224 double buffer), Timer 0, VU memory, VU microcode, pads, then
 * the VBlank handler. */
#if 0
/* NON_MATCHING: 10 lines: store order and register allocation differ around the gGfx/D_008BD310 initialisation and the pmode/ALP writes. */
void Gfx_Init(void) {
    u128 *p;

    sceGsResetGraph(0, 1, 2, 1);
    *(volatile u32 *)0x10000010 = 0x82;
    for (p = (u128 *)0x11000000; p <= (u128 *)0x11000FFF; p++) {
        *p = 0;
    }
    for (p = (u128 *)0x11004000; p <= (u128 *)0x11004FFF; p++) {
        *p = 0;
    }
    for (p = (u128 *)0x11008000; p <= (u128 *)0x1100BFFF; p++) {
        *p = 0;
    }
    for (p = (u128 *)0x1100C000; p <= (u128 *)0x1100FFFF; p++) {
        *p = 0;
    }
    gGfx.vsyncCount = 0;
    gGfx.unk26E = 1;
    gGfx.unk258 = 0;
    gGfx.parity = 0;
    gGfx.field = 0;
    gGfx.unk264 = 0;
    gGfx.phase = 0;
    gGfx.unk26F = 0;
    gGfx.unk26D = 0;
    func_0039C788();
    D_008BD310.unk70 = 0x600;
    D_008BD310.unk40 = 0;
    D_008BD310.unk48 = 0;
    D_008BD310.unk50 = 0;
    D_008BD310.unk54 = 0;
    D_008BD310.unk74 = 0x2800;
    Dma_SendChain(0x10008000, D_004F5F00);
    Dma_Wait(0x10008000);
    func_00342E38();
    Dma_SendChain(0x10009000, D_004F7A00);
    Dma_Wait(0x10009000);
    sceGsSetDefDBuff(&gGfx.dbuff, 1, 640, 224, 3, 0x3A, 1);
    gGfx.dbuff.clear0Rgbaq[2] = 0;
    gGfx.dbuff.clear0Rgbaq[1] = 0;
    gGfx.dbuff.clear0Rgbaq[0] = 0;
    gGfx.dbuff.clear1Rgbaq[2] = 0;
    gGfx.dbuff.clear1Rgbaq[1] = 0;
    gGfx.dbuff.clear1Rgbaq[0] = 0;
    Gfx_SetDispEnv(&gGfx.dbuff.disp[0], 1, 640, 224, 0, 0, 0);
    Gfx_SetDispEnv(&gGfx.dbuff.disp[1], 1, 640, 224, 0, 0, 1);
    gGfx.dbuff.disp[0].pmode.EN1 = 1;
    gGfx.dbuff.disp[1].pmode.EN1 = 1;
    gGfx.dbuff.disp[0].pmode.EN2 = 1;
    gGfx.dbuff.disp[1].pmode.EN2 = 1;
    gGfx.dbuff.disp[0].pmode.MMOD = 1;
    gGfx.dbuff.disp[1].pmode.MMOD = 1;
    gGfx.dispfb[0] = gGfx.dbuff.disp[0].dispfb;
    gGfx.dispfb[1] = gGfx.dbuff.disp[1].dispfb;
    gGfx.display[0] = gGfx.dbuff.disp[0].display;
    gGfx.display[1] = gGfx.dbuff.disp[1].display;
    gGfx.dbuff.disp[0].pmode.ALP = 0x7F;
    gGfx.dbuff.disp[1].pmode.ALP = 0x7F;
    FlushCache(0);
    sceGsSwapDBuff(&gGfx.dbuff, 0);
    sceGsSwapDBuff(&gGfx.dbuff, 1);
    func_003637D0();
    func_00399CC8();
    func_003993B0();
    func_00399418();
    func_00399308();
    gGfx.dy = gGfx.display[0].DY;
    gGfx.dx = gGfx.display[0].DX;
    func_00399E50(Dma_GetPacket(10)->addr, 0, 0);
    func_00399E50(Dma_GetPacket(11)->addr, 0, 1);
    func_00399E50(Dma_GetPacket(13)->addr, 1, 0);
    func_00399E50(Dma_GetPacket(14)->addr, 1, 1);
    FlushCache(0);
    Pad_Init();
    gFrameReady = 0;
    while (sceGsSyncV(0) == 0) {
    }
    Gfx_InstallVSyncHandler(0);
}
#else
INCLUDE_ASM("asm/nonmatchings/gfx", Gfx_Init);
#endif

/* Buffer shown and field, packed as field << 1 | parity. */
s32 Gfx_GetFieldParity(void) {
    return (gGfx.parity & 1) | (gGfx.field & 1) << 1;
}

void Gfx_SetFrameReady(void) {
    gFrameReady = 1;
}

/* Waits until the VBlank handler has taken the frame (advancing the spin generator meanwhile), then restarts
 * Timer 0. */
void Gfx_WaitVSync(void) {
    do {
        Rng_SpinNext();
        EE_HW_POLL();
    } while (gFrameReady == 1);
    *(volatile u32 *)0x10000000 = 0;
}

void func_00398D20(void) {
    func_003990A8(Gfx_GetFieldParity());
}

/* Shows the buffer selected by the parity: display environment and DISPFB1 / DISPLAY1 / DISPLAY2. */
void Gfx_ShowBuffer(void) {
    s32 i;

    if (gGfx.unk26E == 0) {
        i = gGfx.parity & 1;
    } else {
        i = 1;
    }
    Gfx_PutDispEnv(&gGfx.dbuff.disp[i]);
    *(volatile u64 *)0x12000070 = *(u64 *)&gGfx.dispfb[i];
    *(volatile u64 *)0x12000080 = *(u64 *)&gGfx.display[i];
    *(volatile u64 *)0x120000A0 = *(u64 *)&gGfx.display[i];
}

/* Starts the frame's display DMA chain on `channel`. */
void Gfx_KickDisplayChain(u32 channel) {
    void *chain;

    if (gGfx.unk26E) {
        s32 odd = !gGfx.field;

        chain = (void *)D_004364C0[gGfx.unk26D][odd];
    } else {
        chain = Dma_GetPacket(0x10);
    }
    Dma_SendChain(channel, chain);
}

/* INTC VBLANK_START handler: swaps the buffers when a frame is ready, kicks the display DMA chain for the
 * current phase and counts the VBlank. */
#if 0
/* NON_MATCHING: the parity increment is reached with a branch-likely that loads the parity in its delay slot in the original. */
s32 Gfx_VSyncHandler(s32 cause) {
    if (gFrameReady != 0 || gGfx.unk264 != 0) {
        Gfx_ShowBuffer();
        gGfx.field = ~(*(volatile u64 *)0x12001000 >> 13) & 1;
        if (gGfx.unk26E != 0) {
            gGfx.parity = 0;
        } else {
            gGfx.parity++;
        }
        gGfx.unk258 = gGfx.field;
    }
    switch (gGfx.phase) {
    case 0:
        Gfx_KickDisplayChain(0x1000A000);
        break;
    case 1:
        if (gGfx.unk264 != 0 && func_0035E198() != 0) {
            Gfx_KickDisplayChain(0x10009000);
        }
        break;
    case 2:
        break;
    case 3:
        if (gGfx.unk264 != 0 && func_0035E198() != 0) {
            Gfx_KickDisplayChain(0x1000A000);
        }
        break;
    }
    gFrameReady = 0;
    gGfx.vsyncCount++;
    EE_SYNC_L();
    EE_EI();
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/gfx", Gfx_VSyncHandler);
#endif

void Gfx_SetPhase(u8 phase) {
    gGfx.phase = phase;
}

/* Installs Gfx_VSyncHandler on INTC cause 2 (VBLANK_START). */
void Gfx_InstallVSyncHandler(s32 mode) {
    u32 status;

    func_00399488();
    if (mode == 0) {
        gGfx.unk26F = 0;
    } else {
        gGfx.unk26F = 2;
    }
    do {
        EE_DI();
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    gGfx.handlerId = AddIntcHandler(2, Gfx_VSyncHandler, 0);
    EnableIntc(2);
    EE_EI();
}

/* Removes the VBlank handler. */
void Gfx_RemoveVSyncHandler(void) {
    u32 status;

    do {
        EE_DI();
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    DisableIntc(2);
    RemoveIntcHandler(2, gGfx.handlerId);
    EE_EI();
}

/* Builds the frame's display packet in the scratchpad and sends it. */
#if 0
/* NON_MATCHING: first draft; packet building order and registers differ. */
void func_003990A8(s32 unused) {
    u8 *pkt;
    u8 *base;
    s32 parity;
    s32 field;
    s32 i;

    Spr_Setup(1, 0);
    base = *(u8 **)0x70000000;
    parity = gGfx.parity & 1;
    field = gGfx.field & 1;
    pkt = base;
    Dma_AddRefTag((DmaTag **)&pkt, 0);
    func_00398778((u128 **)&pkt, 2);
    *(u64 *)(pkt - 0x30 + 0x10) = D_004364D0[gGfx.unk26F];
    gGfx.unk26F = 0;
    *(u64 *)(pkt - 0x30 + 0x20) = *(u64 *)((u8 *)D_004364E8 + (gGfx.unk264 ? 8 : 0));
    if (gGfx.unk26E) {
        i = 5;
    } else {
        i = parity ? 4 : 3;
    }
    Dma_AddRefTag((DmaTag **)&pkt, i);
    Dma_AddRefTag((DmaTag **)&pkt, 0x20);
    func_00398858(*(u32 *)(pkt - 0xC), parity, field);
    Dma_AddRefTag((DmaTag **)&pkt, 0x21);
    func_00398858(*(u32 *)(pkt - 0xC), parity, field);
    Dma_AddRefTag((DmaTag **)&pkt, 0x10);
    func_00343178(base, (pkt - base) >> 4);
}
#else
INCLUDE_ASM("asm/nonmatchings/gfx", func_003990A8);
#endif

void Gfx_PutDispEnv(GsDispEnv *disp) {
    sceGsPutDispEnv(disp);
}

/* sceGsSetDefDispEnv, then DISPFB for the first (FBP 0) or second (FBP 0x8C) buffer. */
void Gfx_SetDispEnv(GsDispEnv *disp, s32 psm, s32 w, s32 h, s32 dx, s32 dy, s32 second) {
    sceGsSetDefDispEnv(disp, psm, w, h, dx, dy);
    if (second == 0) {
        disp->dispfb = (u64)0 | (u64)(w / 64) << 9 | (u64)psm << 15;
    } else {
        disp->dispfb = (u64)0x8C | (u64)(w / 64) << 9 | (u64)psm << 15;
    }
}

void func_003992C8(s32 value) {
    gGfx.unk264 = value;
}

/* Waits for the next VSync when the field changed since the last swap. */
void func_003992D8(void) {
    if ((gGfx.unk258 & 1) != (gGfx.parity & 1)) {
        sceGsSyncV(0);
    }
}

void func_00399308(void) {
    func_0035E6B8(Dma_GetPacket(1));
}

/* Display position: DX / DY of both DISPLAY registers. */
void Gfx_SetDisplayOffset(s32 dx, s32 dy) {
    gGfx.display[1].DX = gGfx.dx = dx;
    gGfx.display[0].DX = dx;
    gGfx.display[1].DY = gGfx.dy = dy;
    gGfx.display[0].DY = dy;
}

void Gfx_GetDisplayOffset(s32 *dx, s32 *dy) {
    *dx = gGfx.dx;
    *dy = gGfx.dy;
}

void func_003993B0(void) {
    u32 base;
    Unk8BD390 *rec;

    D_008BD310.unk60[D_008BD310.unk50] = 0;
    base = func_003639B8();
    rec = &D_008BD390[0];
    D_008BD310.unk78 = 1;
    rec->unk4 = 0;
    base += D_008BD310.unk50 * 0x2000 * 4;
    D_008BD310.unk44 = base;
    D_008BD310.unk58 = 0;
    D_008BD310.unk40 = base;
    D_008BD390[0].unk0 = base;
}

void func_00399418(void) {
    u32 base;

    D_008BD310.unk68[D_008BD310.unk54] = 0;
    base = func_003639C0();
    D_008BD310.unk7C = 1;
    D_008BEC90[0].unk4 = 0;
    D_008BD310.unk5C = 0;
    D_008BEC90[0].unk0 = D_008BD310.unk48 = D_008BD310.unk4C = base + D_008BD310.unk54 * 0x30000;
}

void func_00399488(void) {
    func_003993B0();
    func_00399418();
}

void func_003994A8(void) {
    D_008BD310.unk78 = 0;
    D_008BD310.unk7C = 0;
}
