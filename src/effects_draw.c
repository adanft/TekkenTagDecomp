/* 0x3A6460-0x3A9678 (module effects_draw, docs/game_overview.md section 4; docs/systems/render.md section 7): the
 * screen fade, the title logo, the stage texture layers (animated VRAM uploads) and the background effects of stages 4
 * and 12, the fighters' texture animation channels, a queue of textured quads, character 0x1D's special attack volume,
 * and helpers shared with src/effects.c (random float, bank entries, angles). A cut of the effects object (its .rodata
 * 0x519940-0x519B30 and .lit4 0x5F5300-0x5F531C follow effects' without a gap), not a proven object edge. Every
 * NON_MATCHING draft (#if 0) compiles when enabled. Built without -ffast-math (configure.py NO_FAST_MATH):
 * func_003A8D08 has the errno check of sqrtf. */
#include "common.h"
#include "fighter.h"
#include "effect.h"

/* Screen fade, title logo, stage texture layers and the background effects of stages 4 and 12. */
/* h4: effects_draw 0x3A6460-0x3A8090: the screen fade, the title logo, the stage's animated texture layers
 * (D_008C9D30, packets D_008C9DB0) and the stage 4 / stage 12 layer effects. */

/* Screen fade (a full-screen sprite drawn with alpha blending). */
extern s32 D_005F6CF8; /* active */
extern s32 D_005F6CFC; /* blend mode: 0 alpha blend (As = level), 1 additive, else subtractive (FIX = level) */
extern f32 D_005F6D00; /* level (0-128) */
extern f32 D_005F6D04; /* step per frame */
extern s32 D_005F6D08; /* frame */
extern s32 D_005F6D0C; /* frame count */
extern u8 D_005F6D10;  /* colour r */
extern u8 D_005F6D11;  /* colour g */
extern u8 D_005F6D12;  /* colour b */

typedef struct {
    /* 0x0 */ s32 state;
    /* 0x4 */ s32 bank;
    /* 0x8 */ f32 alpha;
    /* 0xC */ s32 frame;
} Unk8C9D20;
extern Unk8C9D20 D_008C9D20;

typedef struct {
    /* 0x0 */ u16 state;
    /* 0x4 */ s32 pos;   /* 24.8 */
    /* 0x8 */ s32 speed; /* 24.8 */
    /* 0xC */ s32 len;
} Unk8C9D30;
extern Unk8C9D30 D_008C9D30[8];

/* GS register values as the effect packets hold them. */
typedef struct {
    /* 0x0 */ u16 x;
    /* 0x2 */ u16 y;
    /* 0x4 */ u32 z;
} __attribute__((aligned(8))) GsXyz;
typedef struct {
    /* 0x0 */ u8 r;
    /* 0x1 */ u8 g;
    /* 0x2 */ u8 b;
    /* 0x3 */ u8 a;
    /* 0x4 */ f32 q;
} GsRgbaq;
typedef union {
    u64 d;
    struct {
        u64 a : 2;
        u64 b : 2;
        u64 c : 2;
        u64 d : 2;
        u64 pad8 : 24;
        u64 fix : 8;
        u64 pad40 : 24;
    } f;
} GsAlphaReg;

/* Two vertices of a sprite (passed to func_003457C0). */
typedef struct {
    /* 0x00 */ GsXyz xyz0;
    /* 0x08 */ GsRgbaq rgbaq0;
    /* 0x10 */ f32 s0;
    /* 0x14 */ f32 t0;
    /* 0x18 */ GsAlphaReg alpha;
    /* 0x20 */ GsXyz xyz1;
    /* 0x28 */ GsRgbaq rgbaq1;
    /* 0x30 */ f32 s1;
    /* 0x34 */ f32 t1;
    /* 0x38 */ u64 unk38;
} FxSprite;

/* The effects' work area in the scratchpad after Spr_Setup(3, 0x230) (0x70000100). */
typedef struct {
    /* 0x000 */ u8 pad0[0x1D0];
    /* 0x1D0 */ s32 x;
    /* 0x1D4 */ s32 y;
    /* 0x1D8 */ s32 z;
    /* 0x1DC */ s32 pad1DC;
    /* 0x1E0 */ u32 pkt;
    /* 0x1E4 */ u8 pad1E4[0xC];
    /* 0x1F0 */ FxSprite spr;
} FxSprWork;
#define FXW ((FxSprWork *)0x70000100)

void FlushCache(s32 op);
u32 Spr_Setup(u32 count, s32 reserved);
u32 func_00342FB0(void);
void func_00343178(u32 spr, u32 qwc);

void Fade_Draw(void);
void Fade_Update(void);
s32 func_003A6A90(void);
void func_003A7558(void);
void func_003A73F8(s32 i, s32 speed);
void func_003A7430(s32 i, s32 speed, u32 pos);
void func_003A7490(s32 i, s32 pos);
void func_003457C0(u32 pk, FxSprite *v, s32 i);
Unk8BF3A0 *func_00343F08(s32 i);
void func_00344030(s32 i, s32 frame, u32 pk);
extern s16 D_00519918[];

typedef struct {
    /* 0x00 */ u32 *cur;
    /* 0x04 */ u128 *base;
    /* 0x08 */ u32 pad8[2];
    /* 0x10 */ u128 buf[1];
} GifPacket;
void sceGifPkInit(void *pk, void *base);
void sceGifPkRef(void *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceGifPkTerminate(void *pk);
u32 *sceGifPkReserve(void *pk, u32 n);
u32 sceGifPkSize(void *pk);
extern u128 D_00455E30[];
/* Per layer, 128 frames of a 3-quadword GIF packet (a REF tag to the frame's image upload first). */
extern u128 D_008C9DB0[8][128][3];

extern u16 D_005F5CB6;
s32 func_003BB4D8(void);
void func_003A76C0(Effect *e);
void func_003A7590(Effect *e);

extern s32 D_005F5F10;

/* A layer animation driven by a frame list (func_003A7FA8). */
typedef struct {
    /* 0x00 */ u8 head[0x20];
    /* 0x20 */ s16 layer;
    /* 0x22 */ s16 unk22;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s16 *seq;
} FxLayerSeqE;
typedef struct {
    /* 0x0 */ s16 layer;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s16 *seq;
} FxLayerSeq;

extern s16 D_00519990[];
extern s16 D_005199A8[];
extern s16 D_005199C0[];
extern s16 D_005199D8[];
extern s16 D_005199F0[];
extern s16 D_00519A08[];
extern s16 D_00519A20[];
extern s16 D_00519A68[];
extern s16 D_00519AB0[];

/* A layer of the stage's layer list (func_00356A70): where its frames go in GS memory. */
typedef struct {
    /* 0x0 */ s16 layer;
    /* 0x2 */ u16 dbp;
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u16 unk6;
    /* 0x8 */ u16 unk8;
    /* 0xA */ u16 unkA;
    /* 0xC */ u16 dbw;
    /* 0xE */ u16 dpsm;
} FxLayerDef;
typedef struct {
    /* 0x0 */ s32 count;
    /* 0x4 */ FxLayerDef def[1];
} FxLayerList;
/* A GS image upload record as the layer frames use it. */
typedef struct {
    /* 0x00 */ u8 pad0[0x14];
    /* 0x14 */ u32 unk14;
    /* 0x18 */ u8 pad18[0xC];
    /* 0x24 */ u32 unk24;
    /* 0x28 */ u8 pad28[8];
    /* 0x30 */ u32 unk30;
    /* 0x34 */ u32 unk34;
    /* 0x38 */ u8 pad38[0x18];
    /* 0x50 */ u32 qwc;
    /* 0x54 */ u8 pad54[0xC];
    /* 0x60 */ u32 data[1];
} FxImageRec;

/* A libgraph GIF packet header (sceGifPacket). */
typedef struct {
    /* 0x0 */ u32 *cur;
    /* 0x4 */ u128 *base;
    /* 0x8 */ u32 pad8[2];
} GifPacketHead;

FxLayerList *func_00356A70(void);
void *func_00367878(void);
void *Fx_GetBankEntry(u32 *bank, u32 i);
s32 func_00364550(void *p);
u32 func_00354120(void *pk, void *rec, u32 base);

/* A background actor of stage 4: its texture layer, a timer, and the frame list it plays. */
typedef struct {
    /* 0x0 */ s16 layer;
    /* 0x2 */ s16 timer;
    /* 0x4 */ s16 active;
    /* 0x8 */ s16 *seq;
    /* 0xC */ s16 unkC;
} FxStageActor;
extern FxStageActor D_008D5DB0[3]; /* layers 4-6 */
extern FxStageActor D_008D5DE0[4]; /* layers 0-3 */
extern FVec4 D_00437C50[];         /* positions of layers 4-6 */
extern FVec4 D_00437C80[];         /* positions of layers 0-3 */

void func_00366EA8(Vu0Matrix m);
void sceVu0RotTransPers(s32 *out, Vu0Matrix m, f32 *v, s32 mode);
void func_00353C78(s32 i, s32 on);
s32 func_0035A648(void);
void func_00374D30(s32 id, s32 player);

/* Fighter texture animation channels, the textured quad queue, character 0x1D's special attack volume, helpers. */
#include "fighter.h"

/* Per side (4) and channel (2): a texture animation channel (D_008D5E20), its frames' GS upload packets in
 * D_008D5EA0. Modes: 0 off, 1 set up, 2 hold a frame, 3 play once then hold, 4 loop [V code; I meaning]. */
typedef struct {
    /* 0x0 */ u16 unk0; /* mode */
    /* 0x2 */ u16 pad2;
    /* 0x4 */ s32 unk4; /* frame << 8 */
    /* 0x8 */ s32 unk8; /* speed (0x100 = one frame per frame) */
    /* 0xC */ s32 unkC; /* frame count (x256 in unk4) */
} Unk8D5E20;
extern Unk8D5E20 D_008D5E20[4][2];

extern u16 D_005F6240;
extern u8 D_005F6254;
extern u16 D_005F6D18[2];
extern u16 D_008D7820[][0x24];
extern u8 D_005F6248[4];
extern FVec4 D_00437CD0;
extern Fighter gFighters[4];

/* A GS vertex of the scratchpad work area: XYZ2, RGBAQ, ST, and (first vertex only) the ALPHA register. */
typedef struct {
    /* 0x0 */ u8 c[4];
    /* 0x4 */ f32 q;
} FxRgbaq5;
typedef struct {
    u64 a : 2;
    u64 b : 2;
    u64 c : 2;
    u64 d : 2;
    u64 pad8 : 24;
    u64 fix : 8;
    u64 pad40 : 24;
} GsAlpha5;
typedef struct {
    /* 0x00 */ u16 x;
    /* 0x02 */ u16 y;
    /* 0x04 */ s32 z;
    /* 0x08 */ FxRgbaq5 c;
    /* 0x10 */ f32 s;
    /* 0x14 */ f32 t;
    /* 0x18 */ GsAlpha5 alpha;
} FxVert;
typedef struct {
    /* 0x00 */ u8 pad0[0x90];
    /* 0x90 */ u8 *unk90;
    /* 0x94 */ u8 pad94[0xC];
    /* 0xA0 */ FxVert v[4];
} FxSprWork5;
/* A screen position: x, y, z (fixed point) and a pad word. */
typedef s32 FxPos[4];
/* A queued quad (36 at D_008D6AA0). */
typedef struct {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ u16 unk2;
    /* 0x04 */ u8 pad4[0xC];
    /* 0x10 */ s32 unk10[4][4];
    /* 0x50 */ u8 unk50[4];
    /* 0x54 */ u8 pad54[0xC];
} Unk8D6AA0;
typedef struct {
    u8 c[4];
} Rgba5;
extern Unk8D6AA0 D_008D6AA0[];
typedef struct {
    /* 0x00 */ u8 pad0[0x10];
    /* 0x10 */ u64 unk10;
} FxTexRec;

void func_003A88B0(s32 i, FxPos *pos, u8 *rgba, FxSprWork5 *w);
void Fx_AddQuad(FxPos *pos, Rgba5 *c, u16 a, u16 b);

/* A libgraph GIF packet header followed by its buffer (dma_gif.c). */
typedef struct {
    /* 0x00 */ u32 *cur;
    /* 0x04 */ u128 *base;
    /* 0x08 */ u32 pad8[2];
    /* 0x10 */ u128 buf[1];
} GifPacket5;
#define SPR_PK ((GifPacket5 *)0x70000100)

/* Per side and channel, 8 frames of 0x30 bytes: a GIF qword first. */
typedef struct {
    /* 0x00 */ u128 unk0;
    /* 0x10 */ u8 pad10[0x20];
} Unk8D5EA0;
extern Unk8D5EA0 D_008D5EA0[4][2][8];
extern u128 D_00455E30[];

/* A texture placement entry of a character (func_003AF528): the channel it goes to and the GS image position. */
typedef struct {
    /* 0x0 */ s16 idx;
    /* 0x2 */ u16 unk2; /* added to the side's base block pointer */
    /* 0x4 */ u16 unk4; /* DSAX */
    /* 0x6 */ u16 unk6; /* DSAY */
    /* 0x8 */ u16 unk8; /* RRW */
    /* 0xA */ u16 unkA; /* RRH */
    /* 0xC */ u16 unkC; /* DBW */
    /* 0xE */ u16 unkE; /* DPSM */
} FxTexEntry;
typedef struct {
    /* 0x0 */ s32 n;
    /* 0x4 */ FxTexEntry e[1];
} FxTexTbl;
/* A GS image upload record (GsImageRec of scene3d_d.c) seen as words. */
typedef struct {
    /* 0x00 */ u32 pad0[5];
    /* 0x14 */ u32 unk14;
    /* 0x18 */ u32 pad18[3];
    /* 0x24 */ u32 unk24;
    /* 0x28 */ u32 pad28[2];
    /* 0x30 */ u32 unk30;
    /* 0x34 */ u32 unk34;
    /* 0x38 */ u32 pad38[6];
    /* 0x50 */ u32 unk50;
} FxImageRec5;
typedef struct {
    /* 0x0 */ u32 *cur;
    /* 0x4 */ u128 *base;
    /* 0x8 */ u32 pad8[2];
} GifPkHdr;

extern s16 gMode;
extern u8 D_0043B800[];
extern u32 D_00437CC0[];
FxTexTbl *func_003AF528(s16 ch);
u32 *func_00367270(s16 ch);

void func_003A88A0(Fighter *f);

extern Vu0Matrix D_00433440;
extern Vu0Matrix D_00433400;
extern u8 D_005F6250[];
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
float fabsf(float x);

void sceVu0ApplyMatrix(f32 *out, f32 (*m)[4], f32 *v);
void Light_StartFlash(f32 *pos, u8 *rgb);
s32 func_003A0180(s32 a, s32 b);
f32 func_003A95D8(f32 x, f32 z, f32 a);
f32 func_003A9528(f32 y, f32 x);

/* NON_MATCHING (4 lines): `li s2, 0x1000` / `lui s3, 0x1100` of the packet header are materialised in the other
 * order (every order of the header stores was tried).
 * Draws the screen fade: a full-screen sprite strip of colour (D_005F6D10-12) blended by mode D_005F6CFC at level
 * D_005F6D00. */
#if 0
void Fade_Draw(void) {
    FxSprWork *s = FXW;
    FxSprite *v;
    u64 *pk;
    u64 *p;
    u64 *q;
    u32 i;
    f32 a;
    u16 x;
    u16 y;

    s->x = 0x8000;
    s->y = 0x8000;
    s->z = 0xFFFF;
    switch (D_005F6CFC) {
    case 0:
        s->spr.alpha.f.a = 0;
        s->spr.alpha.f.b = 1;
        s->spr.alpha.f.c = 0;
        s->spr.alpha.f.d = 1;
        s->spr.alpha.f.fix = 0;
        a = D_005F6D00;
        break;
    case 1:
        s->spr.alpha.f.a = 0;
        s->spr.alpha.f.b = 2;
        s->spr.alpha.f.c = 2;
        s->spr.alpha.f.d = 1;
#ifdef PORT
        /* PC build: through u32, as gcc converts it into the 8-bit field (__fixunssfsi); clang's 32-bit software
         * float cannot convert a float to a 64-bit integer (an internal error) */
        s->spr.alpha.f.fix = (u32)D_005F6D00;
#else
        s->spr.alpha.f.fix = D_005F6D00;
#endif
        a = 128.0f;
        break;
    case 2:
    default:
        s->spr.alpha.f.a = 2;
        s->spr.alpha.f.b = 0;
        s->spr.alpha.f.c = 2;
        s->spr.alpha.f.d = 1;
#ifdef PORT
        s->spr.alpha.f.fix = (u32)D_005F6D00; /* as above */
#else
        s->spr.alpha.f.fix = D_005F6D00;
#endif
        a = 128.0f;
        break;
    }
    s->spr.rgbaq0.r = D_005F6D10;
    s->spr.rgbaq0.g = D_005F6D11;
    s->spr.rgbaq0.b = D_005F6D12;
    s->spr.rgbaq0.a = a;
    s->spr.rgbaq0.q = 1.0f;
    s->spr.rgbaq1 = s->spr.rgbaq0;
    s->spr.xyz0.z = s->z;
    x = s->x;
    y = s->y;
    s->spr.xyz1 = s->spr.xyz0;
    s->spr.xyz0.x = x - 0x1400;
    s->spr.xyz0.y = y - 0xE00;
    s->spr.xyz1.x = x + 0x1400;
    s->spr.xyz1.y = y + 0xE00;
    pk = (u64 *)s->pkt;
    q = pk + 2;
    v = &FXW->spr;
    ((u16 *)pk)[0] = 0xF;
    ((u32 *)pk)[1] = 0;
    ((u32 *)pk)[2] = 0x11000000;
    ((u16 *)pk)[1] = 0x1000;
    ((u32 *)pk)[3] = 0x5000000F;
    pk[2] = 0x1123400000000003;
    q[1] = 0xE;
    q[2] = v->alpha.d;
    q[3] = 0x43;
    q[4] = 0x53005;
    q[5] = 0x48;
    q[6] = *(u64 *)&v->rgbaq0;
    q[7] = 1;
    q[8] = 0x240000000000800A;
    q[9] = 0x55;
    v->xyz1.x = s->spr.xyz0.x;
    p = pk + 12;
    for (i = 0; i < 10; i++) {
        u16 nx = s->spr.xyz1.x;

        s->spr.xyz0.x = nx;
        s->spr.xyz1.x = nx + 0x400;
        p[0] = *(u64 *)&s->spr.xyz0;
        p[1] = *(u64 *)&s->spr.xyz1;
        p += 2;
    }
    func_00343178(pk, (u128 *)p - (u128 *)pk);
}
#else
INCLUDE_ASM("asm/nonmatchings/effects_draw", Fade_Draw);
#endif

/* Starts a fade in from colour (r, g, b) over n frames. */
void func_003A6770(s32 n, u8 r, u8 g, u8 b) {
    if (n != 0) {
        D_005F6D10 = r;
        D_005F6D11 = g;
        D_005F6D12 = b;
        D_005F6D00 = 0.0f;
        D_005F6D08 = 0;
        D_005F6CFC = 0;
        D_005F6D04 = 128.0f / n;
        D_005F6D0C = n;
        D_005F6CF8 = 1;
    }
}

/* Starts a fade out to colour (r, g, b) over n frames. */
void func_003A67D0(s32 n, u8 r, u8 g, u8 b) {
    if (n != 0) {
        D_005F6D10 = r;
        D_005F6D11 = g;
        D_005F6D12 = b;
        D_005F6D00 = 128.0f;
        D_005F6D08 = 0;
        D_005F6CFC = 0;
        D_005F6D04 = -(128.0f / n);
        D_005F6D0C = n;
        D_005F6CF8 = 1;
    }
}

/* Shows colour (r, g, b) at level a (clamped to 0-128) for one frame. */
void func_003A6830(u8 r, u8 g, u8 b, f32 a) {
    if (a < 0.0f) {
        a = 0.0f;
    }
    if (a > 128.0f) {
        a = 128.0f;
    }
    D_005F6D00 = a;
    D_005F6D04 = 0.0f;
    D_005F6D10 = r;
    D_005F6D11 = g;
    D_005F6D12 = b;
    D_005F6D08 = 1;
    D_005F6CF8 = 1;
    D_005F6D0C = 0;
    D_005F6CFC = 0;
}

/* Fills the screen with colour (r, g, b) for one frame. */
void func_003A6898(s32 r, s32 g, s32 b) {
    D_005F6D00 = 128.0f;
    D_005F6D04 = 0.0f;
    D_005F6D10 = r;
    D_005F6D11 = g;
    D_005F6D12 = b;
    D_005F6D08 = 1;
    D_005F6CF8 = 1;
    D_005F6D0C = 0;
    D_005F6CFC = 0;
}

/* Fills the screen with colour (r, g, b) for n frames. */
void func_003A68D0(s32 n, s32 r, s32 g, s32 b) {
    D_005F6D00 = 128.0f;
    D_005F6D04 = 0.0f;
    D_005F6D10 = r;
    D_005F6D11 = g;
    D_005F6D12 = b;
    D_005F6D08 = 0;
    D_005F6D0C = n;
    D_005F6CF8 = 1;
    D_005F6CFC = 0;
}

/* Fade update: draws it, steps the level when not paused, and ends it after its frame count. */
void Fade_Update(void) {
    Fade_Draw();
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        D_005F6D00 += D_005F6D04;
        D_005F6D08++;
    }
    if (D_005F6D08 >= D_005F6D0C) {
        D_005F6CF8 = 0;
    }
}

/* Draws texture bank `bank` as a sprite of (2w x 2h) pixels around (x, y) (12.4 fixed point), blended at alpha. */
s32 func_003A6978(s32 bank, s32 x, s32 y, s32 w, s32 h, f32 alpha) {
    FxSprWork *s = FXW;

    s->y = y;
    s->x = x;
    s->spr.alpha.f.a = 0;
    s->spr.alpha.f.b = 2;
    s->spr.alpha.f.c = 2;
    s->spr.alpha.f.d = 1;
    s->z = 0xFFFE;
#ifdef PORT
    /* PC build: through u32, as gcc converts it into the 8-bit field (__fixunssfsi); clang's 32-bit software
     * float cannot convert a float to a 64-bit integer (an internal error) */
    s->spr.alpha.f.fix = (u32)((1.0f - alpha) * 128.0f);
#else
    s->spr.alpha.f.fix = (1.0f - alpha) * 128.0f;
#endif
    s->spr.rgbaq0.r = 0x80;
    s->spr.rgbaq0.g = 0x80;
    s->spr.rgbaq0.b = 0x80;
    s->spr.rgbaq0.a = 0x80;
    s->spr.rgbaq0.q = 1.0f;
    FXW->spr.rgbaq1 = FXW->spr.rgbaq0;
    s->spr.xyz0.z = s->z;
    s->spr.xyz1 = s->spr.xyz0;
    s->spr.xyz0.x = s->x - w * 16;
    s->spr.xyz0.y = s->y - h * 16;
    s->spr.xyz1.x = s->x + w * 16;
    s->spr.xyz1.y = s->y + h * 16;
    s->spr.s0 = 0.0f;
    s->spr.t0 = 0.0f;
    s->spr.s1 = 1.0f;
    s->spr.t1 = 1.0f;
    func_003457C0(s->pkt, &FXW->spr, bank);
}

/* Title logo: plays the frames of D_00519918 on texture bank 8 (state 1 starts it, a negative frame ends it). */
s32 func_003A6A90(void) {
    Unk8C9D20 *d = &D_008C9D20;
    Unk8BF3A0 *rec;
    u16 f;

    switch (d->state) {
    case 0:
        break;
    case 1:
        d->state = 2;
        d->bank = 8;
        d->alpha = 0.0f;
        d->frame = 0;
        break;
    case 2:
        rec = func_00343F08(d->bank);
        f = D_00519918[d->frame];
        rec->unk4 = f;
        func_00344030(d->bank, (s16)f, *(u32 *)0x70000000);
        *(u32 *)0x700002E0 = func_00342FB0();
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            d->frame++;
            if (D_00519918[d->frame] < 0) {
                d->state = 0;
            }
        }
        return func_003A6978(d->bank, 0x8000, 0x8000, 0x80, 0x100, d->alpha);
    }
}

/* Starts the title logo (func_003A6A90). */
void func_003A6BC0(void) {
    D_008C9D20.state = 1;
}

/* Stops the fade and the title logo. */
void func_003A6BD0(void) {
    D_005F6CF8 = 0;
    D_008C9D20.state = 0;
    D_005F6CFC = 0;
    D_005F6D00 = 0.0f;
    D_005F6D04 = 0.0f;
    D_005F6D08 = 0;
    D_005F6D0C = 0;
    D_005F6D10 = 0;
    D_005F6D11 = 0;
    D_005F6D12 = 0;
    __asm__ volatile(""); /* fake: keeps the last store out of the jr delay slot, as in the original */
}

/* Effect overlay pass (end of the frame): sets up the scratchpad work area, then draws the title logo and the fade. */
void func_003A6C08(void) {
    FlushCache(0);
    Spr_Setup(3, 0x230);
    if (D_008C9D20.state != 0) {
        *(u32 *)0x700002E0 = func_00342FB0();
        func_003A6A90();
    }
    if (D_005F6CF8 == 1) {
        *(u32 *)0x700002E0 = func_00342FB0();
        Fade_Update();
    }
}

/* Draws the animated texture layers: sends each shown layer's current frame upload, then advances the playing ones. */
void Fx_DrawStageLayers(void) {
    GifPacket *pk;
    s32 i;
    s32 n = 0;
    Unk8C9D30 *l;

    Spr_Setup(1, 0);
    pk = (GifPacket *)*(u32 *)0x70000000;
    sceGifPkInit(pk, pk->buf);
    for (i = 0, l = D_008C9D30; i < 8; i++, l++) {
        s32 f = l->pos >> 8;

        if (l->state >= 2) {
            pk->buf[n++] = D_008C9DB0[i][f][0];
            sceGifPkReserve(pk, 4);
            switch (l->state) {
            case 2:
                break;
            case 3:
                if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && D_005F5F10 != 0) {
                    l->pos += l->speed;
                }
                if ((l->pos >> 8) >= l->len) {
                    l->pos = 0;
                    l->state = 2;
                }
                break;
            case 4:
                if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && D_005F5F10 != 0) {
                    l->pos += l->speed;
                }
                if ((l->pos >> 8) >= l->len) {
                    l->pos = 0;
                }
                break;
            }
        }
    }
    if (n != 0) {
        sceGifPkRef(pk, D_00455E30, 2, 0x10000000, 0x50000002, 0);
        sceGifPkTerminate(pk);
        func_00343178((u32)pk->base, sceGifPkSize(pk));
    }
}

/* Builds the packets of the stage's animated texture layers: for each layer of the list, one image upload packet per
 * frame of its bank (GS address `base` + the layer's offset). */
void Fx_LoadStageLayers(s32 base) {
    GifPacketHead pk;
    FxLayerDef *defs[8];
    void *bank;
    FxLayerList *list;
    void *frames;
    FxImageRec *img;
    FxLayerDef *d;
    Unk8C9D30 *l;
    s32 i;
    s32 k;
    u32 j;
    u32 n;
    u32 *w;
    u32 dst;
    u32 qwc;

    list = func_00356A70();
    for (i = 0; i < 8; i++) {
        defs[i] = NULL;
    }
    func_003A7558();
    if (list->count <= 0) {
        return;
    }
    if (list->count > 8) {
        return;
    }
    for (i = 0; i < list->count; i++) {
        k = list->def[i].layer;
        defs[k] = &list->def[i];
    }
    if (func_00367878() == NULL) {
        return;
    }
    bank = Fx_GetBankEntry(func_00367878(), 0);
    for (i = 0, l = D_008C9D30; i < list->count; i++, l++) {
        d = defs[i];
        if (d != NULL && i < func_00364550(bank)) {
            frames = Fx_GetBankEntry(bank, i);
            l->len = *(s32 *)frames;
            l->state = 1;
            for (k = 0; k < l->len; k++) {
                img = Fx_GetBankEntry(frames, k);
                sceGifPkInit(&pk, D_008C9DB0[i][k]);
                dst = base + d->dbp;
                qwc = img->qwc | 0x8000;
                img->unk14 = dst + (d->dbw << 16) + (d->dpsm << 24);
                img->unk24 = d->unk4 + (d->unk6 << 16);
                img->unk30 = d->unk8;
                img->unk34 = d->unkA;
                img->qwc = qwc;
                if (D_005F5CB6 == 4 && i >= 0 && i < 4 && k == 1) {
                    w = img->data;
                    n = (qwc & 0x7FFF) * 4;
                    for (j = 0; j < n; j++) {
                        *w++ = 0;
                    }
                }
                func_00354120(&pk, img, dst);
            }
        }
    }
    FlushCache(0);
}

/* Clears the texture layers. */
void func_003A7190(void) {
    func_003A7558();
}

/* Starts the animated texture layers of the stage D_005F5CB6 (and its extra effects). */
void Fx_StartStageLayers(void) {
    s32 i;
    Effect *e;

    switch (D_005F5CB6) {
    case 1:
        func_003A7430(0, 0x40, 0);
        break;
    case 2:
        func_003A7430(0, 0xC0, 0);
        break;
    case 3:
        func_003A7430(0, 0xC0, 0);
        func_003A7430(1, 0xC0, 0);
        func_003A7430(2, 0xC0, 0x10);
        break;
    case 4:
        func_003A7490(0, 1);
        func_003A7490(2, 1);
        func_003A7490(1, 1);
        func_003A7490(3, 1);
        func_003A7490(4, 3);
        func_003A7490(5, 3);
        func_003A7490(6, 3);
        if (func_003BB4D8() == 0) {
            func_003464F8(func_003A76C0);
        }
        break;
    case 6:
        func_003A7430(0, 0x80, 0);
        func_003A7430(1, 0x100, 0);
        func_003A7430(2, 0x100, 0);
        break;
    case 8:
    case 17:
    case 20:
        func_003A7430(0, 0x100, 0);
        break;
    case 9:
    case 18:
        func_003A7430(0, 0x100, 0);
        func_003A7430(1, 0x100, 8);
        break;
    case 11:
        func_003A7430(0, 0x80, 0);
        break;
    case 12:
        for (i = 0; i < 3; i++) {
            e = func_003464F8(func_003A7590);
            if (e != NULL) {
                e->unk1A = i;
            }
        }
        break;
    case 14:
        func_003A7430(0, 0x80, 0);
        func_003A7430(1, 0x80, 0);
        break;
    }
}

/* Plays layer i once from frame 0 at `speed` frames/256 per frame (state 3, back to 2 at the end). */
void func_003A73F8(s32 i, s32 speed) {
    if (i < 8) {
        Unk8C9D30 *l = &D_008C9D30[i];
        if (l->state != 0) {
            l->speed = speed;
            l->state = 3;
            l->pos = 0;
        }
    }
}

/* Loops layer i from frame pos at `speed` (state 4). */
void func_003A7430(s32 i, s32 speed, u32 pos) {
    if (i < 8) {
        Unk8C9D30 *l = &D_008C9D30[i];
        if (l->state != 0) {
            pos = l->len != 0 ? pos % l->len : 0;
            l->state = 4;
            l->pos = pos << 8;
            l->speed = speed;
        }
    }
}

/* Shows frame pos of layer i (state 2). */
void func_003A7490(s32 i, s32 pos) {
    if (i < 8) {
        Unk8C9D30 *l = &D_008C9D30[i];
        if (l->state != 0) {
            if (l->len == 0) {
                pos = 0;
            } else {
                pos %= l->len;
            }
            l->state = 2;
            l->pos = pos << 8;
        }
    }
}

/* Hides layer i (state 1). */
void func_003A74F0(s32 i) {
    if (i < 8) {
        D_008C9D30[i].state = 1;
    }
}

/* Hides every layer and resets its speed. */
void func_003A7518(void) {
    s32 i;
    Unk8C9D30 *l = D_008C9D30;

    for (i = 0; i < 8; i++, l++) {
        l->state = 1;
        l->pos = 0;
        l->speed = 0x100;
    }
}

/* Clears every layer (no frames). */
void func_003A7558(void) {
    s32 i;
    Unk8C9D30 *l = D_008C9D30;

    for (i = 0; i < 8; i++, l++) {
        l->state = 0;
        l->pos = 0;
        l->len = 0;
        l->speed = 0x100;
    }
}

/* Update of a blinking layer (stage 12, layer e->unk1A): waits on frame 0, starts the animation at random
 * (7 in 1000 per frame), and goes back to frame 0 when it has played once. */
void func_003A7590(Effect *e) {
    switch (e->unk18) {
    case 0:
        func_003A7490(e->unk1A, 0);
        e->unk18++;
        break;
    case 1:
        if (rand() % 1000 < 7) {
            func_003A73F8(e->unk1A, 0x80);
            e->unk1C = 0;
            e->unk18++;
        }
        break;
    case 2:
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && D_005F5F10 != 0) {
            e->unk1C += 0x80;
            if ((D_008C9D30 + e->unk1A)->len < e->unk1C >> 8) {
                func_003A7490(e->unk1A, 0);
                e->unk18--;
            }
        }
        break;
    }
}

INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_00519990);
INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_005199A8);
INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_005199C0);
INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_005199D8);
INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_005199F0);
INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_00519A08);
INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_00519A20);
INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_00519A68);
INCLUDE_RODATA("asm/nonmatchings/effects_draw", D_00519AB0);
/* NON_MATCHING (13 lines, the file built without -ffast-math and the square roots as SqrtS): the spill slots of the
 * two hoisted stack addresses (sp+0x40 / sp+0x10) are swapped and `near = 0` / `id -= 4` of the second loop are
 * scheduled after the call.
 * Stage 4 background actors (update callback): layers 0-3 and 4-6 play random frame lists, with a sound and a light
 * change when the actor is near the screen centre. Calls rand() every frame. */
#if 0
/* A bare sqrt.s with the two nops the compiler puts in front of it: the file is built without -ffast-math (the errno
 * check of func_003A8D08), where sqrtf() would add the check and a call (fake: as in src/model_anim_b.c). */
static inline f32 SqrtS(f32 x) {
    f32 r;

    EE_SQRT_S(r, x);
    return r;
}

void func_003A76C0(Effect *e) {
    FxStageActor *a;
    FxStageActor *b;
    Unk8C9D30 *l;
    Unk8C9D30 *l2;
    s32 id;
    s16 *seq;
    s32 near;
    s32 r;
    s32 k2;
    s32 i;
    s32 k;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && D_005F5F10 != 0) {
        e->unk1C++;
    }
    switch (e->unk18) {
    case 0:
        for (i = 0, a = D_008D5DB0; i < 3; i++, a++) {
            a->active = 0;
            a->layer = i + 4;
            a->timer = rand() % 240;
        }
        for (i = 0, b = D_008D5DE0; i < 4; i++, b++) {
            b->layer = i;
            b->active = 0;
            b->timer = rand() % 1000;
        }
        e->unk18++;
        break;
    case 1:
        for (i = 0, b = D_008D5DE0; i < 4; i++, b++) {
            {
                Vu0Matrix m;
                s32 v[4];

                id = b->layer;
                func_00366EA8(m);
                near = 0;
                id <<= 4;
                sceVu0RotTransPers(v, m, (f32 *)((u8 *)D_00437C80 + id), 1);
                if (v[3] > 0) {
                    f32 dx = 0x8000 - v[0];
                    f32 dy = 0x8000 - v[1];

                    if (SqrtS(dx * dx + dy * dy) < 10000.0f) { /* fake: SqrtS */
                        near = 1;
                    }
                }
            }
            if (b->active == 0) {
                if (rand() % 1000 < 6 && D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                    b->active = 1;
                    switch (rand() % 3) {
                    case 0:
                        b->seq = D_00519A20;
                        if (near && func_0035A648() != 0) {
                            func_00374D30(0x1072, 0);
                        }
                        break;
                    case 1:
                        b->seq = D_00519A68;
                        if (near && func_0035A648() != 0) {
                            func_00374D30(0x1074, 0);
                        }
                        break;
                    case 2:
                        b->seq = D_00519AB0;
                        if (near && func_0035A648() != 0) {
                            func_00374D30(0x1075, 0);
                        }
                        break;
                    }
                }
            } else {
                l = &D_008C9D30[b->layer];
                if (l->state != 0) {
                    l->state = 2;
                    seq = b->seq;
                    l->pos = *seq << 8;
                    if (near) {
                        if (*seq == 0) {
                            func_00353C78(0, 1);
                            func_00353C78(1, 1);
                        } else {
                            func_00353C78(0, 0);
                            func_00353C78(1, 0);
                        }
                    }
                    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && D_005F5F10 != 0) {
                        seq++;
                    }
                    b->seq = seq;
                    if (*seq < 0) {
                        b->active = 0;
                        b->timer++;
                        l->pos = 0x100;
                        func_00353C78(0, 0);
                        func_00353C78(1, 0);
                    }
                }
            }
        }
        for (i = 0, a = D_008D5DB0; i < 3; i++, a++) {
            {
                s32 v[4];
                Vu0Matrix m;

                id = a->layer;
                id -= 4;
                near = 0;
                func_00366EA8(m);
                id <<= 4;
                sceVu0RotTransPers(v, m, (f32 *)((u8 *)D_00437C50 + id), 1);
                if (v[3] > 0) {
                    f32 dx = 0x8000 - v[0];
                    f32 dy = 0x8000 - v[1];

                    if (SqrtS(dx * dx + dy * dy) < 5000.0f) { /* fake: SqrtS */
                        near = 1;
                    }
                }
            }
            if (a->active == 0) {
                if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && D_005F5F10 != 0) {
                    a->timer++;
                }
                if (a->timer >= 240) {
                    a->timer = 0;
                    if (rand() % 100 < 30) {
                        a->active = 1;
                        a->unkC = 0;
                        a->seq = D_00519990;
                        if (near && func_0035A648() != 0) {
                            func_00374D30(0x1073, 0);
                        }
                    }
                } else if (a->timer >= 168) {
                    if (rand() % 1000 < 20) {
                        r = rand() % 3;
                        a->active = 1;
                        switch (r) {
                        case 0:
                            a->seq = D_005199A8;
                            break;
                        case 1:
                            a->seq = D_005199C0;
                            break;
                        case 2:
                            a->seq = D_005199D8;
                            break;
                        }
                        a->unkC = 0;
                    }
                } else {
                    if (rand() % 1000 < 10 && D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                        k2 = rand() & 1;
                        a->active = 1;
                        switch (k2) {
                        case 0:
                            a->seq = D_005199F0;
                            break;
                        case 1:
                            a->seq = D_00519A08;
                            break;
                        }
                        a->unkC = 0;
                    }
                }
            } else {
                l2 = &D_008C9D30[a->layer];
                k = 0;
                if (l2->state != 0) {
                    l2->state = 2;
                    seq = a->seq;
                    l2->pos = *seq << 8;
                    switch (a->layer) {
                    case 4:
                        break;
                    case 5:
                        k = 3;
                        break;
                    case 6:
                        k = 1;
                        break;
                    }
                    if (*seq == 0 && D_008C9D30[k].state != 0) {
                        D_008C9D30[k].state = 2;
                        D_008C9D30[k].pos = 0;
                        D_008D5DE0[k].active = 0;
                        if (near) {
                            func_00353C78(0, 1);
                            func_00353C78(1, 1);
                        }
                    } else {
                        D_008C9D30[k].state = 2;
                        D_008C9D30[k].pos = 0x100;
                    }
                    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && D_005F5F10 != 0) {
                        seq++;
                    }
                    a->seq = seq;
                    if (*seq < 0) {
                        a->active = 0;
                        l2->pos = 0x300;
                        func_00353C78(0, 0);
                        func_00353C78(1, 0);
                    }
                }
            }
        }
        break;
    }
}
#else
LIT4_WORD(D_005F5300, 0x461C4000);
LIT4_WORD(D_005F5304, 0x459C4000);
INCLUDE_ASM("asm/nonmatchings/effects_draw", func_003A76C0);
#endif

/* Update: shows the next frame of the list on the layer each frame; at the end of the list (a negative frame)
 * the layer gets its rest frame back and the effect ends. */
void func_003A7EB0(Effect *e) {
    FxLayerSeq *s = (FxLayerSeq *)&e->sub;
    Unk8C9D30 *l = &D_008C9D30[((FxLayerSeqE *)e)->layer];
    s16 *seq;

    if (l->state == 0) {
        func_003465C8(e);
        return;
    }
    l->state = 2;
    seq = s->seq;
    l->pos = *seq << 8;
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        seq++;
    }
    s->seq = seq;
    if (*seq < 0) {
        l->state = 2;
        switch (s->layer) {
        case 0:
        case 1:
        case 2:
        case 3:
            l->pos = 0x100;
            break;
        case 4:
        case 5:
        case 6:
            l->pos = 0x300;
            break;
        }
        func_003465C8(e);
    }
}

/* Plays frame list `kind` (0-8) once on layer `layer` (0-6). */
void func_003A7FA8(u32 layer, u32 kind) {
    Effect *e = func_003464F8(func_003A7EB0);

    if (e != NULL && layer < 7) {
        FxLayerSeq *s = (FxLayerSeq *)&e->sub;

        s->layer = layer;
        s->unk2 = 0;
        switch (kind) {
        case 0:
            s->seq = D_00519990;
            break;
        case 1:
            s->seq = D_005199A8;
            break;
        case 2:
            s->seq = D_005199C0;
            break;
        case 3:
            s->seq = D_005199D8;
            break;
        case 4:
            s->seq = D_005199F0;
            break;
        case 5:
            s->seq = D_00519A08;
            break;
        case 6:
            s->seq = D_00519A20;
            break;
        case 7:
            s->seq = D_00519A68;
            break;
        case 8:
        default:
            s->seq = D_00519AB0;
            break;
        }
    }
}

/* Per frame for each side (main.c): sends the current frame of each active texture animation channel (its GS upload
 * packet from D_008D5EA0) through the scratchpad and advances modes 3 / 4 unless the game is paused. */
void Fx_DrawFighterTex(u32 side) {
    Unk8D5E20 *p;
    s32 j;
    s32 n = 0;
    s32 k;

    Spr_Setup(0, 0xB0);
    sceGifPkInit(SPR_PK, SPR_PK->buf);
    if (side < 4) {
        p = D_008D5E20[side];
        for (j = 0; j < 2; j++, p++) {
            k = p->unk4 >> 8;
            if (p->unk0 >= 2) {
                SPR_PK->buf[n++] = D_008D5EA0[side][j][k].unk0;
                sceGifPkReserve(SPR_PK, 4);
                switch (p->unk0) {
                case 2:
                    break;
                case 3:
                    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                        p->unk4 += p->unk8;
                    }
                    if ((p->unk4 >> 8) >= p->unkC) {
                        p->unk4 = 0;
                        p->unk0 = 2;
                    }
                    break;
                case 4:
                    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                        p->unk4 += p->unk8;
                    }
                    if ((p->unk4 >> 8) >= p->unkC) {
                        p->unk4 = 0;
                    }
                    break;
                }
            }
        }
    }
    if (n != 0) {
        sceGifPkRef(SPR_PK, D_00455E30, 2, 0, 0, 0);
        sceGifPkTerminate(SPR_PK);
        func_00343178((u32)SPR_PK->base, sceGifPkSize(SPR_PK));
    }
}

/* At a character load / tag swap (Fighter_TagSwap): sets up the side's texture animation channels from the
 * character's placement table (func_003AF528, at most 2 entries) and image bank (func_00367270): channel `idx` gets
 * mode 1 and one GS image upload packet per frame in D_008D5EA0 (BITBLTBUF / TRXPOS / TRXREG from the entry). In
 * gMode 5 the side is unk22 instead of the slot unk20. */
void Fx_LoadFighterTex(Fighter *f) {
    s16 side;
    s16 ch;
    FxTexTbl *t;
    GifPkHdr pk;
    FxTexEntry *ptrs[2];
    s32 i;
    s32 k;
    u32 base;
    u32 *bank;
    u32 *b2;
    Unk8D5E20 *p;
    FxTexEntry *e;
    FxImageRec5 *r;
    u32 tbp;

    if (gMode == 5) {
        side = f->unk22;
    } else {
        side = f->unk20;
    }
    ch = f->unk22;
    t = func_003AF528(ch);
    for (i = 0; i < 2; i++) {
        ptrs[i] = NULL;
    }
    for (i = 0; i < 2; i++) {
        D_008D5E20[side][i].unk0 = 0;
        D_008D5E20[side][i].unk4 = 0;
        D_008D5E20[side][i].unk8 = 0x100;
    }
    if (t->n <= 0) {
        return;
    }
    if (t->n >= 3) {
        return;
    }
    for (i = 0; i < t->n; i++) {
        k = t->e[i].idx;
        ptrs[k] = &t->e[i];
    }
    if (func_00367270(ch) == NULL) {
        return;
    }
    bank = Fx_GetBankEntry(func_00367270(ch), 0);
    p = D_008D5E20[side];
    base = D_00437CC0[side];
    for (i = 0; i < t->n; i++, p++) {
        e = ptrs[i];
        if (e != NULL && i < func_00364550(bank)) {
            b2 = Fx_GetBankEntry(bank, i);
            p->unkC = b2[0];
            p->unk0 = 1;
            for (k = 0; k < p->unkC; k++) {
                r = Fx_GetBankEntry(b2, k);
                sceGifPkInit((GifPacket5 *)&pk, &D_008D5EA0[side][i][k].unk0);
                tbp = base + e->unk2;
                r->unk14 = tbp + (e->unkC << 16) + (e->unkE << 24);
                r->unk24 = e->unk4 + (e->unk6 << 16);
                r->unk30 = e->unk8;
                r->unk34 = e->unkA;
                r->unk50 |= 0x8000;
                func_00354120(&pk, r, tbp);
            }
        }
    }
    if (gMode != 5 || D_0043B800[0] == 0) {
        if (f->unk2A == 9) {
            func_003A88A0(f);
        }
    }
}

/* Channel (side i, channel j), when set up: play once from frame 0 at speed v (mode 3). */
void func_003A8620(s32 i, s32 j, s32 v) {
    Unk8D5E20 *p;

    if (i < 4 && j < 2) {
        p = &D_008D5E20[i][j];
        if (p->unk0 != 0) {
            p->unk0 = 3;
            p->unk4 = 0;
            p->unk8 = v;
        }
    }
}

/* Channel: loop from frame t (modulo the frame count) at speed v (mode 4). */
void func_003A8668(s32 i, s32 j, s32 v, u32 t) {
    Unk8D5E20 *p;

    if (i < 4 && j < 2) {
        p = &D_008D5E20[i][j];
        if (p->unk0 != 0) {
            t = p->unkC != 0 ? t % p->unkC : 0;
            p->unk0 = 4;
            p->unk4 = t << 8;
            p->unk8 = v;
        }
    }
}

/* Channel: hold frame t (modulo the frame count) (mode 2). */
void func_003A86D8(s32 i, s32 j, s32 t) {
    Unk8D5E20 *p;

    if (i < 4 && j < 2) {
        p = &D_008D5E20[i][j];
        if (p->unk0 != 0) {
            if (p->unkC == 0) {
                t = 0;
            } else {
                t = t % p->unkC;
            }
            p->unk0 = 2;
            p->unk4 = t << 8;
        }
    }
}

/* Channel: mode 1. */
void func_003A8748(s32 i, s32 j) {
    Unk8D5E20 *p;

    if (i < 4 && j < 2) {
        p = &D_008D5E20[i][j];
        p->unk0 = 1;
    }
}

/* All channels: mode 1, frame 0, speed 0x100. */
void func_003A8788(void) {
    s32 i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 2; j++) {
            Unk8D5E20 *p = &D_008D5E20[i][j];

            p->unk0 = 1;
            p->unk4 = 0;
            p->unk8 = 0x100;
        }
    }
}

/* All channels off. */
void func_003A87E8(void) {
    s32 i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 2; j++) {
            Unk8D5E20 *p = &D_008D5E20[i][j];

            p->unk0 = 0;
            p->unk4 = 0;
            p->unk8 = 0x100;
        }
    }
}

/* The two channels of side i off. */
void func_003A8848(u32 i) {
    Unk8D5E20 *p;
    s32 j;

    if (i < 4) {
        p = D_008D5E20[i];
        for (j = 0; j < 2; j++, p++) {
            p->unk0 = 0;
            p->unk4 = 0;
            p->unk8 = 0x100;
        }
    }
}

/* Channel 0 of the fighter's slot holds frame 0. */
void func_003A8890(Fighter *f) {
    func_003A86D8((s16)f->unk20, 0, 0);
}

/* Channel 0 of the fighter's slot holds frame 1 (character 9, Fx_LoadFighterTex). */
void func_003A88A0(Fighter *f) {
    func_003A86D8((s16)f->unk20, 0, 1);
}

/* NON_MATCHING: 24 lines; the original keeps p = q + 14 in its own register (a2) and computes the quadword count as
 * (p + 8) - start at run time; every form tried folds the offsets or the count (12).
 * Sends one textured quad (texture record i of func_00343F08, its TEX0) with additive blending (ALPHA A=0 B=2 C=2
 * D=1, FIX = rgba[3]) through the scratchpad work area w (0x70000100). */
#if 0
void func_003A88B0(s32 i, FxPos *pos, u8 *rgba, FxSprWork5 *w) {
    FxVert *v = w->v;
    s32 k;
    u8 *pk;
    u64 *q;
    u64 *p;
    FxTexRec *t;

    w->v[0].alpha.a = 0;
    w->v[0].alpha.b = 2;
    w->v[0].alpha.c = 2;
    w->v[0].alpha.d = 1;
    w->v[0].alpha.fix = rgba[3];
    w->v[0].c.c[0] = rgba[0];
    w->v[0].c.c[1] = rgba[1];
    w->v[0].c.c[2] = rgba[2];
    w->v[0].c.c[3] = 0x80;
    w->v[0].c.q = 1.0f;
    w->v[1].c = w->v[2].c = w->v[3].c = w->v[0].c;
    w->v[0].s = 0;
    w->v[0].t = 0;
    w->v[1].s = 1.0f;
    w->v[1].t = 0;
    w->v[2].s = 0;
    w->v[2].t = 1.0f;
    w->v[3].s = 1.0f;
    w->v[3].t = 1.0f;
    for (k = 0; k < 4; k++) {
        w->v[k].x = pos[k][0];
        w->v[k].y = pos[k][1];
        w->v[k].z = pos[k][2];
    }
    q = (u64 *)w->unk90;
    t = (FxTexRec *)func_00343F08(i);
    ((u16 *)q)[1] = 0x1000;
    ((u16 *)q)[0] = 0xB;
    ((u32 *)q)[2] = 0x11000000;
    ((u32 *)q)[3] = 0x5000000B;
    ((u32 *)q)[1] = 0;
    pk = (u8 *)q;
    q += 2;
    q[0] = 0x112A400000000005LL;
    q[1] = 0xE;
    q[2] = t->unk10;
    q[3] = 7;
    q[4] = *(u64 *)&v[0].alpha;
    q[5] = 0x43;
    q[6] = 0x53005;
    q[7] = 0x48;
    q[8] = 5;
    q[9] = 9;
    q[10] = *(u64 *)&v[0].c;
    q[11] = 1;
    q[12] = 0x8400000000008001LL;
    q[13] = 0x52525252;
    p = (u64 *)pk + 16;
    p[0] = *(u64 *)&v[0].s;
    p[1] = *(u64 *)&w->v[0].x;
    p[2] = *(u64 *)&v[1].s;
    p[3] = *(u64 *)&v[1].x;
    p[4] = *(u64 *)&v[2].s;
    p[5] = *(u64 *)&v[2].x;
    p[6] = *(u64 *)&v[3].s;
    p[7] = *(u64 *)&v[3].x;
    q = p + 8;
    func_00343178((u32)pk, ((u8 *)q - pk) >> 4);
}
#else
INCLUDE_ASM("asm/nonmatchings/effects_draw", func_003A88B0);
#endif

/* Empties the quad queue. */
void Fx_ClearQuads(void) {
    D_005F6240 = 0;
}

/* Per frame (main.c): draws the queued quads (texture upload func_00344030, then func_003A88B0) and empties the
 * queue. The two scratchpad stores are written in two forms on purpose: as the same expression twice, CSE keeps
 * 0x70000190 in a saved register; the original has the assembler's absolute form for both. */
void Fx_DrawQuads(void) {
    Unk8D6AA0 *p;
    s32 i;
    u32 pk;

    if (D_005F6240 != 0) {
        p = D_008D6AA0;
        Spr_Setup(2, 0x120);
        for (i = 0; i < D_005F6240; i++, p++) {
            pk = func_00342FB0();
            *(u32 *)0x70000190 = pk;
            func_00344030(p->unk0, p->unk2, pk);
            ((FxSprWork5 *)0x70000100)->unk90 = (u8 *)func_00342FB0();
            func_003A88B0(p->unk0, p->unk10, p->unk50, (FxSprWork5 *)0x70000100);
        }
        D_005F6240 = 0;
    }
}

/* Queues a quad (at most 36): four screen positions, a colour, texture record a, frame b. */
void Fx_AddQuad(FxPos *pos, Rgba5 *c, u16 a, u16 b) {
    Unk8D6AA0 *p;
    s32 i;

    if (D_005F6240 < 36) {
        p = &D_008D6AA0[D_005F6240++];
        p->unk0 = a;
        p->unk2 = b;
        for (i = 0; i < 4; i++) {
            s32 *d = (s32 *)((u8 *)p + i * 16); /* fake: the row pointer gives addu a0,t1,a0 */
            d[4] = pos[i][0];
            d[5] = pos[i][1];
            d[6] = pos[i][2];
        }
        *(Rgba5 *)p->unk50 = *c;
    }
}

/* Character 0x1D, when its special attack volume starts (func_00378150 / func_00378398, the fight's hit test):
 * restarts the flash counter, draws 18 random frame phases (rand() & 7) and starts a light flash at the fighter. */
void func_003A8C50(s32 side) {
    FVec4 v;
    s32 i;
    Fighter *f;

    D_005F6D18[side] = 0;
    for (i = 0; i < 18; i++) {
        D_008D7820[side][i] = rand() & 7;
    }
    f = &gFighters[side & 1];
    sceVu0ApplyMatrix(v, f->unk1BF0, D_00437CD0);
    Light_StartFlash(v, D_005F6248);
}

/* fake: this compiler names the errno fallback of sqrtf and the float-to-unsigned conversion `sqrtf` and `fptoui`;
 * the original object calls `sqrt` (0x407818) and `__fixunssfsi` (0x40EBA0), so the assembler maps the two names. */
#if !defined(M2CTX) && !defined(PERMUTER) && !defined(PORT)
__asm__(".set sqrtf, sqrt\n.set fptoui, __fixunssfsi");
#endif

/* Draws the special attack volume of character 0x1D from point b back to point a: a flash sprite at the fighter for
 * the first 15 frames (D_005F6D18), then up to 36 segments of 500 units as textured quads (texture 0x14 / 0x17 + 1 or
 * + 2 for the first), each with its frame phase D_008D7820[side][k] % 7 advanced every unpaused frame. */
void func_003A8D08(Fighter *f, s32 *a, s32 *b, s32 size) {
    Vu0Matrix m;
    FVec4 va;
    FVec4 vb;
    FVec4 cur;
    FVec4 nxt;
    s32 sc0[4] __attribute__((aligned(16)));
    s32 sc1[4] __attribute__((aligned(16)));
    FxPos quad[4];
    Rgba5 rgba;
    FVec4 w;
    s32 scr[4] __attribute__((aligned(16)));
    FxPos q2[4];
    Rgba5 col;
    s32 tex;
    s32 side;
    s32 r;
    s32 z;
    f32 dx, dy, dz;
    f32 len;
    u32 n;
    u32 k;
    f32 rx, ry, ang;
    f32 c1, s1, c2, s2;

    side = f->unk1E;
    tex = 0x17;
    if (side == 0) {
        tex = 0x14;
    }
    sceVu0MulMatrix(m, D_00433440, D_00433400);
    va[0] = a[0];
    va[1] = -(f32)a[1];
    va[2] = -(f32)a[2];
    va[3] = 1.0f;
    vb[0] = b[0];
    vb[1] = -(f32)b[1];
    vb[2] = -(f32)b[2];
    vb[3] = 1.0f;
    col = *(Rgba5 *)D_005F6250;
    if (D_005F6D18[side] < 15) {
        sceVu0ApplyMatrix(w, f->unk1BF0, D_00437CD0);
        sceVu0RotTransPers(scr, m, w, 1);
        if ((u32)scr[2] <= 0xFDE8 && scr[3] != 0) {
            r = 5529600.0f / scr[3];
            if (scr[0] >= r && scr[1] >= r && 0xFFF0 - r >= scr[0] && 0xFFF0 - r >= scr[1]) {
                z = scr[2] + 300;
                scr[2] = z;
                q2[0][0] = scr[0] - r;
                q2[0][1] = scr[1] - r;
                q2[0][2] = z;
                q2[1][0] = scr[0] + r;
                q2[1][1] = scr[1] - r;
                q2[1][2] = z;
                q2[2][0] = scr[0] - r;
                q2[2][1] = scr[1] + r;
                q2[2][2] = z;
                q2[3][0] = scr[0] + r;
                q2[3][1] = scr[1] + r;
                q2[3][2] = z;
                Fx_AddQuad(q2, &col, tex, D_005F6D18[side]);
                if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                    D_005F6D18[side]++;
                }
            }
        }
    }
    dx = vb[0] - va[0];
    dy = vb[1] - va[1];
    dz = vb[2] - va[2];
    len = sqrtf(dx * dx + dy * dy + dz * dz);
    n = len / 500.0f;
    if (n != 0) {
        /* [I] the original divides by n * 500 and drops the result: only the zero-divisor trap is left; the
         * dividend is unknown. */
        k = size / (n * 500);
    }
    dx /= n;
    dy /= n;
    dz /= n;
    nxt[3] = 1.0f;
    cur[0] = vb[0];
    cur[1] = vb[1];
    cur[2] = vb[2];
    cur[3] = 1.0f;
    nxt[0] = vb[0] - dx;
    nxt[1] = vb[1] - dy;
    nxt[2] = vb[2] - dz;
    rgba.c[0] = 0x80;
    rgba.c[1] = 0x80;
    rgba.c[2] = 0x80;
    rgba.c[3] = 0x80;
    sceVu0RotTransPers(sc0, m, cur, 1);
    if (n > 36) {
        n = 36;
    }
    for (k = 0; k < n; k++) {
        sceVu0RotTransPers(sc1, m, nxt, 1);
        if ((u32)sc0[2] > 0xFDE8 || (u32)sc1[2] > 0xFDE8) {
            goto next;
        }
        if (sc0[0] < 0x6C00 || sc0[1] < 0x7200 || sc0[0] > 0x9400 || sc0[1] > 0x8E00) {
            if (sc1[0] < 0x6C00 || sc1[1] < 0x7200 || sc1[0] > 0x9400 || sc1[1] > 0x8E00) {
                goto next;
            }
        }
        if (sc0[3] == 0) {
            goto next;
        }
        rx = (f32)(size * 0x1C20) / sc0[3];
        if (rx < 0.0f) {
            rx = 0.0f;
        }
        ry = (f32)(size * 0x1C20) / sc0[3];
        if (ry < 0.0f) {
            ry = 0.0f;
        }
        ang = 1.5707964f - atan2f(-(f32)(sc1[1] - sc0[1]), sc1[0] - sc0[0]);
        c1 = rx * cosf(ang);
        s1 = rx * sinf(ang);
        c2 = ry * cosf(ang);
        s2 = ry * sinf(ang);
        quad[0][0] = sc0[0] - (s32)c1;
        quad[0][1] = sc0[1] - (s32)s1;
        quad[0][2] = sc0[2];
        quad[1][0] = sc1[0] - (s32)c2;
        quad[1][1] = sc1[1] - (s32)s2;
        quad[1][2] = sc1[2];
        quad[2][0] = sc0[0] + (s32)c1;
        quad[2][1] = sc0[1] + (s32)s1;
        quad[2][2] = sc0[2];
        quad[3][0] = sc1[0] + (s32)c2;
        quad[3][1] = sc1[1] + (s32)s2;
        quad[3][2] = sc1[2];
        D_008D7820[side][k] %= 7;
        if (k == 0) {
            Fx_AddQuad(quad, &rgba, tex + 2, D_008D7820[side][0]);
        } else {
            Fx_AddQuad(quad, &rgba, tex + 1, D_008D7820[side][k]);
        }
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            D_008D7820[side][k]++;
        }
    next:
        sc0[0] = sc1[0];
        sc0[1] = sc1[1];
        sc0[2] = sc1[2];
        sc0[3] = sc1[3];
        nxt[0] -= dx;
        nxt[1] -= dy;
        nxt[2] -= dz;
    }
}

/* Entry i (clamped to the last) of an offset table: bank[0] entries, offsets at bank[2 * i + 1]. */
void *Fx_GetBankEntry(u32 *bank, u32 i) {
    u32 n = bank[0];

    if (i >= n) {
        i = n - 1;
    }
    return (u8 *)bank + bank[i * 2 + 1];
}

/* A random float in [0, 1). */
f32 Fx_RandFloat(void) {
    return (f32)(u32)(rand() & 0xFFFFFF) / 16777216.0f;
}

/* The float skeleton switch (skeleton.c, motion.c). */
void func_003A9510(s32 v) {
    D_005F6254 = v & 1;
}

u8 func_003A9520(void) {
    return D_005F6254;
}

/* atan2f(y, x) - pi / 2. */
f32 func_003A9528(f32 y, f32 x) {
    return atan2f(y, x) - 1.5707964f;
}

void func_003A9550(f32 *out, f32 a, f32 b, f32 c) {
    out[1] = func_003A0180(a, c);
    out[0] = func_003A9528(b, func_003A95D8(a, c, out[1]));
}

/* |x / sin(a)| when |x| > |z|, else |z / cos(a)| (zero sines / cosines replaced by 1e-5). */
f32 func_003A95D8(f32 x, f32 z, f32 a) {
    f32 s;

    if (fabsf(x) > fabsf(z)) {
        s = sinf(a);
        if (s == 0.0f) {
            s = 1e-05f;
        }
        s = x / s;
    } else {
        s = cosf(a);
        if (s == 0.0f) {
            s = 1e-05f;
        }
        s = z / s;
    }
    return fabsf(s);
}

/* The next object's .rodata (loading_title, still in the generic chunk) starts on a 16-byte boundary: its jump
 * tables are 16-aligned, the split chunk only 8-aligned. */
RODATA_ALIGN16();

