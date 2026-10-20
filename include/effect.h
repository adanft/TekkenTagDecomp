/* Effect objects (src/effects.c, src/effects_draw.c; docs/systems/render.md section 7): the object pool of dma_gif
 * (func_003464F8 / func_00345780 / func_00346030, there called Unk3456B0) with an update callback each, run every frame
 * by func_00346608. */
#ifndef EFFECT_H
#define EFFECT_H
#include "common.h"

typedef f32 Vu0Matrix[4][4];
typedef f32 FVec4[4] __attribute__((aligned(16)));

/* An effect object from the pool of dma_gif (func_003464F8 / func_00345780 / func_00346030, there called
 * Unk3456B0): a list node, the update callback, flags, and the drawing state at +0x20 (set to defaults by
 * func_003456B0). Kind-specific data follows the base part of the state. */
typedef struct Effect Effect;
typedef void (*EffectFunc)(Effect *e);

typedef struct {
    /* 0x00 */ f32 unk0;          /* position x */
    /* 0x04 */ f32 unk4;          /* position y */
    /* 0x08 */ f32 unk8;          /* position z */
    /* 0x0C */ f32 unkC;
    /* 0x10 */ void *unk10;       /* current animation frame record */
    /* 0x14 */ u16 unk14;         /* frame count */
    /* 0x16 */ u16 unk16;         /* frame */
    /* 0x18 */ u16 unk18;
    /* 0x1A */ u16 unk1A;
    /* 0x1C */ u16 unk1C;
    /* 0x1E */ u16 unk1E;
    /* 0x20 */ u16 unk20;
    /* 0x22 */ u16 unk22;
    /* 0x24 */ u8 pad24[0xC];
    /* 0x30 */ f32 unk30[4];      /* scale */
    /* 0x40 */ s32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ u8 unk50;
    /* 0x51 */ u8 unk51;
    /* 0x52 */ u8 unk52;
    /* 0x53 */ u8 unk53;
    /* 0x54 */ f32 unk54;
    /* 0x58 */ f32 unk58;
    /* 0x5C */ u8 pad5C[4];
    /* 0x60 */ f32 unk60[4];
    /* 0x70 */ f32 unk70[4];
    /* 0x80 */ f32 unk80[4];
    /* 0x90 */ s32 unk90;
    /* 0x94 */ s16 unk94;
    /* 0x96 */ s16 unk96;
} EffectSub;

struct Effect {
    /* 0x00 */ Effect *next;
    /* 0x04 */ Effect *prev;
    /* 0x08 */ EffectFunc update;
    /* 0x0C */ Effect *unkC;
    /* 0x10 */ Effect **slot;     /* its slot in a side ring (D_008C2788) or NULL */
    /* 0x14 */ s32 flags;         /* 0x10000: free at the end of the frame (func_003465C8); 4: drawn */
    /* 0x18 */ s16 unk18;         /* often a state */
    /* 0x1A */ s16 unk1A;
    /* 0x1C */ s32 unk1C;         /* often a frame counter */
    /* 0x20 */ EffectSub sub;
};

/* Pause test used everywhere: D_005F5EE0 || D_005F5EE4 || func_003A9698(). */
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
s32 func_003A9698(void);

int rand(void);
float sinf(float);
float cosf(float);
float atan2f(float, float);
float sqrtf(float);

Effect *func_00345780();              /* K&R: new effect with update callback (argument), defaults set */
Effect *func_00346030(s32 side, EffectFunc update); /* effect in a side ring */
void func_003465C8(Effect *e);        /* flags |= 0x10000 (free) */
Effect *func_003464F8();              /* raw pool alloc (callback argument) */

/* 26 records at D_008BF3A0 (animation banks). */
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
Unk8BF3A0 *func_00343F08(s32 i);
Unk8BF350 *func_00343F20(s32 i);
Unk8BF3A0 *func_00343F38(s32 i);

#endif
