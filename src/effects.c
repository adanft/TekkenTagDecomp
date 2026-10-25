/* 0x3A04B8-0x3A6460 (module effects, docs/game_overview.md section 4; docs/systems/render.md section 7): the update
 * callbacks and spawners of the effect objects (include/effect.h): animated bank sprites, the stage ambient effects and
 * lens flare, hit sparks and flashes, the hit log (records the hit events of a frame and replays them), the body-part
 * glows, the move effects of characters 0x14 and 0x21 and the hit-burst particles. Its .rodata (0x5194E0-0x519940) and
 * .lit4 (0x5F51A4-0x5F5300) come right before effects_draw's, so the two ranges are probably one object (see
 * src/effects_draw.c). Every table is included in place with INCLUDE_RODATA (force_not_migration in data.txt);
 * D_00519918, read by effects_draw, is included here at the end so that effects_draw's .rodata starts on the 16-byte
 * boundary its jump tables need. While a float function is assembly, LIT4_WORD in front of its INCLUDE_ASM emits its
 * .lit4 words in place. Every NON_MATCHING draft (#if 0) compiles when enabled. Built without -ffast-math
 * (configure.py NO_FAST_MATH): func_003A35C8 keeps x + 0.0f and a division by 3.0f (docs/decomp_guide.md, "Lessons from
 * T17 batch 4"). */
#include "common.h"
#include "fighter.h"
#include "effect.h"

/* Animated sprites, the stage ambient effects and the lens flare. */

/* Sub-state of the sprite effects of this group, an overlay of EffectSub (at Effect + 0x20) that names the
 * kind-specific words past its 0x98 bytes. */
typedef struct {
    /* 0x00 */ f32 unk0;
    /* 0x04 */ f32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ void *unk10;       /* current animation frame record (FxFrame) */
    /* 0x14 */ u16 unk14;         /* frame count */
    /* 0x16 */ u16 unk16;         /* frame */
    /* 0x18 */ u16 unk18;         /* animation index in the bank */
    /* 0x1A */ u16 unk1A;         /* bank (func_00343F08 / func_00343F20 index) */
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
    /* 0x98 */ u8 pad98[0x48];
    /* 0xE0 */ f32 unkE0[3];      /* previous scaled frame offset */
    /* 0xEC */ u8 padEC[0x34];
    /* 0x120 */ f32 unk120;
    /* 0x124 */ f32 unk124;
} FxSubX;

/* Animation frame record: offset (scaled by unk30), then two floats (unk54, unk58). */
typedef struct {
    /* 0x00 */ f32 unk0[3];
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;
} FxFrame;

/* Animation header returned by Fx_GetBankEntry. */
typedef struct {
    /* 0x00 */ s32 unk0;          /* frame count */
    /* 0x04 */ u8 pad4[0x30];
    /* 0x34 */ u16 unk34;
    /* 0x36 */ u16 unk36;
} FxAnim;                           /* followed by the frames (FxFrame) */

typedef struct { u8 c[4]; } FxRgbaC;

/* Emitter places, 0x20 bytes (.rodata D_005194E0 / D_005195A0). */
typedef struct {
    /* 0x00 */ f32 unk0;
    /* 0x04 */ f32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ s32 unk10;         /* spawn period in frames */
    /* 0x14 */ u8 pad14[0xC];
} FxPlace;

/* Emitter places with a turn, 0x20 bytes (.rodata D_00519660). */
typedef struct {
    /* 0x00 */ f32 unk0;
    /* 0x04 */ f32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ u8 pad14[0xC];
} FxPlaceF;

/* Emitter places with colour, 0x30 bytes (.data D_00436A30 / D_00436AC0). */
typedef struct {
    /* 0x00 */ f32 unk0;
    /* 0x04 */ f32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ FxRgbaC unk20;
    /* 0x24 */ f32 unk24;
    /* 0x28 */ f32 unk28;
    /* 0x2C */ f32 unk2C;
} FxPlaceC;

/* Light places, 0x30 bytes (.data D_00436B50 / D_00436EE0), ended by a negative unk20. */
typedef struct {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ u8 pad2[0xE];
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ f32 unk1C;
    /* 0x20 */ f32 unk20;         /* period (0: fixed) */
    /* 0x24 */ f32 unk24;
    /* 0x28 */ u8 pad28[8];
} FxLight;

/* Per-slot lens-flare state, 0x20 bytes (.bss D_008C47A0, nine slots), written by func_003A1C78. */
typedef struct {
    /* 0x00 */ s32 unk0;          /* screen x (GS units, 0x8000 = centre) */
    /* 0x04 */ s32 unk4;          /* screen y */
    /* 0x08 */ s32 unk8;          /* depth (65000) */
    /* 0x0C */ s32 unkC;          /* 10 */
    /* 0x10 */ f32 unk10;         /* size */
    /* 0x14 */ f32 unk14;         /* alpha */
    /* 0x18 */ u8 pad18[8];
} FxGlow;

/* Lens-flare element definitions, 12 bytes (.rodata D_005196C0, nine). */
typedef struct {
    /* 0x00 */ u16 unk0;          /* sprite (unk1E of the effect) */
    /* 0x04 */ f32 unk4;          /* size */
    /* 0x08 */ f32 unk8;          /* alpha */
} FxGlowDef;

/* Sub-state of the lens-flare sprite (func_003A1BC8): the position words are screen integers. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u8 pad10[0xA];
    /* 0x1A */ u16 unk1A;
    /* 0x1C */ u16 unk1C;
    /* 0x1E */ u16 unk1E;
    /* 0x20 */ u8 pad20[0x20];
    /* 0x40 */ s32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ u8 pad48[0xC];
    /* 0x54 */ f32 unk54;
    /* 0x58 */ f32 unk58;
    /* 0x5C */ u8 pad5C[0x34];
    /* 0x90 */ s32 unk90;
    /* 0x94 */ s16 unk94;
    /* 0x96 */ s16 unk96;
} Fx1BC8Sub;

/* Fighter slot as seen here (0xB8A0 bytes, gFighters). */
typedef struct {
    /* 0x0000 */ u8 pad0[0x127];
    /* 0x0127 */ u8 unk127;       /* active */
    /* 0x0128 */ u8 pad128[0x468];
    /* 0x0590 */ FVec4 unk590;    /* a joint position */
    /* 0x05A0 */ u8 pad5A0[0xC00];
    /* 0x11A0 */ FVec4 unk11A0;   /* a joint position */
    /* 0x11B0 */ u8 pad11B0[0xB8A0 - 0x11B0];
} FxFighter;

extern u16 D_005F5CB6;
extern const FxPlace D_005194E0[];
extern const FxPlace D_005195A0[];
extern const FxPlaceF D_00519660[];
extern FxPlaceC D_00436A30[];
extern FxPlaceC D_00436AC0[];
extern FxLight D_00436B50[];
extern FxLight D_00436EE0[];
extern FxGlow D_008C47A0[];
extern const FxGlowDef D_005196C0[];
extern Fighter gFighters[4];
#define FX_FIGHTERS ((FxFighter *)gFighters)
extern s32 D_00415808[];
extern f32 *D_005F6CD4;
extern f32 D_005F6218;
extern FVec4 D_00437A50, D_00437A60, D_00437A70, D_00437A80, D_00437A90, D_00437AA0, D_00437AB0;
void func_00366EA8(Vu0Matrix m);
void func_00366EB8(Vu0Matrix m);
void func_0034A5D0(Vu0Matrix m, f32 *v);
s32 Tag_StepCounters(s32 side, s32 a);
float fabsf(float);
void sceVu0ApplyMatrix(f32 *out, Vu0Matrix m, f32 *v);
void sceVu0RotTransPers(s32 *out, Vu0Matrix m, f32 *v, s32 mode);

f32 Fx_RandFloat(void);
void *Fx_GetBankEntry(void *bank, u16 i);
void Wind_Sample(f32 x, f32 y, f32 z, f32 *out);
void func_00346600(void);

void func_003A04B8(Effect *e);
void func_003A08D8(Effect *e);
void func_003A0A90(Effect *e);
void func_003A0CE8(Effect *e);
void func_003A1068(Effect *e);
void func_003A1298(Effect *e);
void func_003A1378(void);
void func_003A1510(Effect *e);
void func_003A16C0(Effect *e);
void func_003A18F8(Effect *e);
void func_003A1B48(void);
void func_003A1BC8(Effect *e);
void func_003A1C78(Effect *e);
void func_003A23E0(Effect *e);

/* Hit sparks and flashes, the hit log, the body-part glows and the hit-burst particles. */
#include "fighter.h"

/* ---- types ---- */

/* Four colour bytes copied as one word (lwl / lwr). */
typedef struct {
    u8 r, g, b, a;
} FxRgba;

/* A hit event as func_0035BF10 receives it (and as the hit ring D_008C4C80 records it): an integer position, a
 * direction (unk10 / unk14 give the spark's yaw), and the kind / argument bytes the ring adds. Size 0x18. */
typedef struct {
    /* 0x00 */ s32 pos[3];
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s16 unk14;
    /* 0x16 */ u8 unk16;          /* kind (first argument of func_0035BF10) */
    /* 0x17 */ u8 unk17;          /* second argument */
} HitRec;

/* One frame of the hit ring: its first record and how many records it holds. */
typedef struct {
    /* 0x0 */ HitRec *rec;
    /* 0x4 */ s32 n;
} HitFrame;

/* Per-stage spark parameters (23 entries at D_00437AC0, indexed by the stage id D_005F5CB6). */
typedef struct {
    /* 0x0 */ s16 unk0;           /* spark count */
    /* 0x4 */ f32 unk4;           /* divides 400 */
    /* 0x8 */ FxRgba unk8;        /* colour */
} FxStageParam;

/* A parameter record of D_00436688 (func_00343F38): scale, one word, two floats. */
typedef struct {
    /* 0x00 */ f32 unk0[3];
    /* 0x0C */ s32 unkC;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
} FxParam;

/* An animation record of a bank (Fx_GetBankEntry): frame count, two halfwords at 0x34, frames from 0x38. */
typedef struct {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ u8 pad2[0x32];
    /* 0x34 */ u16 unk34;
    /* 0x36 */ s16 unk36;
} FxAnimH; /* size 0x38 */

/* An effect kind of func_00347730: its update callback and the colour of the light flash. */
typedef struct {
    /* 0x0 */ EffectFunc update;
    /* 0x4 */ u8 rgb[4];
} FxKind;

/* The drawing state of the particle effects func_003A35C8 / func_003A3928 / func_003A3C08: EffectSub plus per-frame
 * steps at 0xE0 and an eased range at 0x120. */
typedef struct {
    /* 0x000 */ EffectSub b;
    /* 0x098 */ u8 pad98[0x48];
    /* 0x0E0 */ f32 unkE0;        /* step of b.unk54 (fade) / phase */
    /* 0x0E4 */ f32 unkE4;        /* step of unkE8 / phase step */
    /* 0x0E8 */ f32 unkE8;        /* frame (float), copied to b.unk1E */
    /* 0x0EC */ f32 unkEC;        /* step of b.unk44 */
    /* 0x0F0 */ u8 padF0[0x30];
    /* 0x120 */ f32 unk120;       /* radius from */
    /* 0x124 */ f32 unk124;       /* radius to */
    /* 0x128 */ f32 unk128;       /* ease phase */
} FxSub3A35C8;

/* A fighter slot seen as its joint matrices (0xC0 apart from 0xDB0; the position is row 3). */
typedef struct {
    /* 0x00 */ Vu0Matrix m;
    /* 0x40 */ u8 pad40[0x80];
} FxJoint;
typedef struct {
    /* 0x0000 */ u8 pad0[0x22];
    /* 0x0022 */ s16 unk22;
    /* 0x0024 */ u8 pad24[6];
    /* 0x002A */ s16 unk2A;       /* character */
    /* 0x002C */ u8 pad2C[0xD84];
    /* 0x0DB0 */ FxJoint joint[1];
} FxFighterJoints;

typedef struct {
    /* 0x00 */ u8 pad0[0x1C];
    /* 0x1C */ s32 unk1C;
} Fx3B87B0Part;
typedef struct {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 unk4;         /* count of unk194 */
    /* 0x008 */ s32 unk8;
    /* 0x00C */ Fx3B87B0Part *unkC[1];   /* indexed from 0x62 */
} Fx3B87B0;

typedef struct {
    f32 v[4];
} __attribute__((aligned(16))) FxVec4S;

/* Rodata pairs read by func_003A4370 (index by a move's byte). */
typedef struct {
    s32 unk0;
    s32 unk4;
} Fx197D0;

/* ---- data ---- */

extern Fighter gFighters[4];
extern u16 D_005F5CB6;                /* stage id */
extern FxStageParam D_00437AC0[];
extern FxRgba D_005F6220[];
extern s16 D_005F6CD8;                /* hit ring: recording */
extern HitFrame *D_005F6CDC;          /* hit ring: frame being recorded */
extern HitFrame *D_005F6CE0;          /* hit ring: frame being replayed */
extern u16 D_005F6CE4;                /* hit ring: records this frame */
extern HitRec *D_005F6CE8;            /* hit ring: next free record */
extern HitFrame D_008C48C0[120];
extern HitRec D_008C4C80[60];
extern HitFrame D_008C5220[8][120];
extern HitRec D_008C7020[8][60];
extern s32 D_005F6CF0[2];
extern s32 D_005F6CF4;               /* D_005F6CF0[1], named by splat */
extern u16 D_00437BE0[];
extern FxVec4S D_005197C0;
extern s32 D_005197D0[];
extern u8 D_005F6228[4];

/* ---- functions of other groups ---- */

f32 Fx_RandFloat(void);
void func_003A05F8(Effect *e);
void func_003A06F0(Effect *e);
void func_003A2470(f32 *pos);
void func_0035BF10(s32 a, s32 b, HitRec *p, s32 d);
struct FighterMove *func_00368248(s32 id);
s32 func_003ABCC0(void);
FxKind *func_00347730(s32 id);
s32 func_0035C970(u32 i);
void Light_StartFlash(f32 *pos, u8 *rgb);
/* Update callback: the glow on a fighter's body part (e->unk1A: bit of D_005F6CF0 / D_005F6CF4 set by
 * func_003A4370), following its joint; spawns func_003A4128 sparks and func_003A3FC0 bursts, flashes the light. */
void func_003A3C08(Effect *e);
float fabsf(float);
/* Update callback: rising burst of func_003A3C08. */
void func_003A3FC0(Effect *e);
/* Update callback: falling spark of func_003A3C08. */
void func_003A4128(Effect *e);
Fx3B87B0 *func_003B87B0(s32 id);
void sceVu0ApplyMatrix(f32 *out, Vu0Matrix m, f32 *v);
s32 func_00365AC0(Fighter *f, s16 a);

/* The special hit flash of func_003A2BC0: effect kind 0x22, bank 9. */
void func_003A2E20(HitRec *p);

/* ---- functions ---- */

/* Move effects of characters 0x14 and 0x21 and of move kind 0x19, and the hit effects with bit 0x80. */
#include "fighter.h"

/* ---- types ---- */

/* Fighter slot fields read by the effects (overlay, size of Fighter): the ground position at 0x590 and an array of
 * 0xC0-byte records of three matrices at 0xDB0, whose first matrix's translation is a joint position
 * (func_003A50B8) and whose record 13 (0x1770) places the move effects (func_003A4EE8, func_003A5810) [I]. */
typedef struct {
    /* 0x00 */ Vu0Matrix unk0;
    /* 0x40 */ Vu0Matrix unk40;
    /* 0x80 */ Vu0Matrix unk80;
} FighterFxMtx; /* 0xC0 */

typedef struct {
    /* 0x0000 */ u8 pad0[0x590];
    /* 0x0590 */ f32 unk590[4];
    /* 0x05A0 */ u8 pad5A0[0x810];
    /* 0x0DB0 */ FighterFxMtx unkDB0[227]; /* 0x1770 = unkDB0[13] */
    /* 0xB7F0 */ u8 padB7F0[0xB0];
} FighterFx;

#define FX_FIGHTER(f) ((FighterFx *)(f))

/* Spark-like effects keep their previous position at sub + 0xE0. */
typedef struct {
    /* 0x00 */ EffectSub b;
    /* 0x98 */ u8 pad98[0x48];
    /* 0xE0 */ f32 unkE0[4];
} FxSparkSub;

/* The hit record handed to func_0035BF10 (Unk8A7AB0 in src/fight_misc.c): a position and a direction. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ u8 padC[0x4];
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s16 unk14;
} FxHitPos;

/* ---- externs ---- */
extern Fighter gFighters[4];
extern Fighter D_00884EE0;
extern TeamMember D_00440940[2][2];
extern u8 D_0043B7F8[];
extern FVec4 D_00437BF0[];
extern FVec4 D_00437C30[];                       /* = D_00437BF0 + 0x40 */
extern u8 D_005F6230[4];
extern FxRgba D_005F6238[];
extern const s32 D_005198A0[];
extern const u8 D_005198C8[];

void sceVu0ApplyMatrix(f32 *out, Vu0Matrix m, f32 *v);
void Light_StartFlash(f32 *pos, u8 *rgb);
u16 func_00346378(s32 side);
void func_00346330(s32 side);
s32 func_003ABCD0(void);
f32 *func_0034CB88(s32 i, s32 j);
f32 Fx_RandFloat(void);                         /* (f32)(rand() & 0xFFFFFF) / 16777216 */
float fabsf(float);

/* h2's vector helpers */
void func_003A3438(f32 *dir, f32 *angles);      /* angles[0] = pitch, angles[1] = yaw of dir */
void func_003A34C8(EffectSub *s, f32 speed);     /* velocity from yaw only (unk80, unk88) */
void func_003A3518(EffectSub *s, f32 speed);     /* velocity from pitch and yaw (unk80..unk88) */
s32 func_003A3588(s32 side, s32 kind);           /* 1 when the side's move is not of this kind, else frame < unk2C */

/* callbacks of other groups */
void func_003A35C8(Effect *e);
void func_003A3928(Effect *e);

/* this group */
void func_003A44F8(Effect *e);
void func_003A4970(Effect *e);
void func_003A4C28(Effect *e);
s32 func_003A4EE8(Fighter *f);
void func_003A50B8(Effect *e);
void func_003A54D8(s32 side);
void func_003A55E0(Effect *e);
s32 func_003A5810(Fighter *f);
void func_003A5910(Effect *e);
void func_003A5B78(Effect *e);
void func_003A5D98(Effect *e);

/* ---- functions ---- */

/* Animated sprite: plays the frames of its animation once, then frees itself. */
void func_003A04B8(Effect *e) {
    EffectSub *s = &e->sub;
    FxFrame *fr = s->unk10;
    Unk8BF3A0 *b;

    s->unk60[0] = fr->unk0[0] * s->unk30[0];
    s->unk60[1] = fr->unk0[1] * s->unk30[1];
    s->unk60[2] = fr->unk0[2] * s->unk30[2];
    s->unk60[3] = 1.0f;
    s->unk54 = fr->unkC;
    s->unk58 = fr->unk10;
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        if (s->unk1C != 0) {
            b = func_00343F08(s->unk1A);
            if (s->unk58 < 1.0f) {
                if (++s->unk1E >= b->unk2) {
                    s->unk1E = b->unk2 - 1;
                }
            }
        }
        fr++;
        s->unk10 = fr;
        s->unk16++;
    }
    if (s->unk16 >= s->unk14) {
        func_003465C8(e);
    }
}

/* Animated sprite turning by unk84 per frame. */
void func_003A05F8(Effect *e) {
    EffectSub *s = &e->sub;
    FxFrame *fr = s->unk10;

    s->unk60[0] = fr->unk0[0] * s->unk30[0];
    s->unk60[1] = fr->unk0[1] * s->unk30[1];
    s->unk60[2] = fr->unk0[2] * s->unk30[2];
    s->unk60[3] = 1.0f;
    s->unk54 = fr->unkC;
    s->unk58 = fr->unk10;
    s->unk70[1] += s->unk80[1];
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        fr++;
        s->unk10 = fr;
        s->unk16++;
    }
    if (s->unk16 >= s->unk14) {
        func_003465C8(e);
    }
}

/* Animated sprite whose frame offsets move its position, drifting along Wind_Sample by 0.5-1 x unk120. */
void func_003A06F0(Effect *e) {
    FxSubX *s = (FxSubX *)&e->sub;
    FxFrame *fr = s->unk10;
    f32 cur[3];
    f32 d[3];
    f32 r;

    if (e->unk18 == 0) {
        s->unkE0[0] = fr->unk0[0] * s->unk30[0];
        s->unkE0[1] = fr->unk0[1] * s->unk30[1];
        s->unkE0[2] = fr->unk0[2] * s->unk30[2];
        s->unk120 = 12.0f;
        e->unk18++;
    }
    cur[0] = fr->unk0[0] * s->unk30[0];
    cur[1] = fr->unk0[1] * s->unk30[1];
    cur[2] = fr->unk0[2] * s->unk30[2];
    s->unk0 += cur[0] - s->unkE0[0];
    s->unk4 += cur[1] - s->unkE0[1];
    s->unk8 += cur[2] - s->unkE0[2];
    s->unk54 = fr->unkC;
    s->unk58 = fr->unk10;
    s->unk70[1] += s->unk80[1];
    s->unkE0[0] = cur[0];
    s->unkE0[1] = cur[1];
    s->unkE0[2] = cur[2];
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        Wind_Sample(s->unk0, s->unk4, s->unk8, d);
        r = (Fx_RandFloat() * 0.5f + 0.5f) * s->unk120;
        s->unk0 += d[0] * r;
        fr++;
        s->unk10 = fr;
        s->unk4 += d[1] * r;
        s->unk8 += d[2] * r;
        s->unk16++;
    }
    if (s->unk16 >= s->unk14) {
        func_003465C8(e);
    }
}

/* As func_003A06F0 without the turn, drifting by 0-1 x unk120. */
void func_003A08D8(Effect *e) {
    FxSubX *s = (FxSubX *)&e->sub;
    FxFrame *fr = s->unk10;
    f32 cur[3];
    f32 d[3];
    f32 r;

    if (e->unk18 == 0) {
        s->unkE0[0] = fr->unk0[0] * s->unk30[0];
        s->unkE0[1] = fr->unk0[1] * s->unk30[1];
        s->unkE0[2] = fr->unk0[2] * s->unk30[2];
        e->unk18++;
    }
    cur[0] = fr->unk0[0] * s->unk30[0];
    cur[1] = fr->unk0[1] * s->unk30[1];
    cur[2] = fr->unk0[2] * s->unk30[2];
    s->unk0 += cur[0] - s->unkE0[0];
    s->unk4 += cur[1] - s->unkE0[1];
    s->unk8 += cur[2] - s->unkE0[2];
    s->unk54 = fr->unkC;
    s->unk58 = fr->unk10;
    s->unkE0[0] = cur[0];
    s->unkE0[1] = cur[1];
    s->unkE0[2] = cur[2];
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        Wind_Sample(s->unk0, s->unk4, s->unk8, d);
        r = Fx_RandFloat() * s->unk120;
        s->unk0 += d[0] * r;
        fr++;
        s->unk10 = fr;
        s->unk4 += d[1] * r;
        s->unk8 += d[2] * r;
        s->unk16++;
    }
    if (s->unk16 >= s->unk14) {
        func_003465C8(e);
    }
}

INCLUDE_RODATA("asm/nonmatchings/effects", D_005194E0);
INCLUDE_RODATA("asm/nonmatchings/effects", D_005195A0);
/* Emitter of stages 4 / 15: six places, each spawning a func_003A08D8 sprite of bank 6 every unk10 frames. */
void func_003A0A90(Effect *e) {
    const FxPlace *p;
    f32 drift;
    s32 i;
    Unk8BF350 *bank;
    Effect *n;
    FxSubX *s;
    FxAnim *a;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        e->unk1C++;
    }
    switch (e->unk18) {
    case 0:
        e->unk18++;
        break;
    case 1:
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            bank = func_00343F20(6);
            if (bank->unk0 != NULL) {
                if (D_005F5CB6 == 4) {
                    drift = 6.0f;
                    p = D_005194E0;
                } else {
                    drift = 25.0f;
                    p = D_005195A0;
                }
                for (i = 0; i < 6; i++, p++) {
                    if (e->unk1C % p->unk10 == 0) {
                        n = func_00345780(func_003A08D8);
                        s = (FxSubX *)&n->sub;
                        if (n != NULL) {
                            n->flags |= 4;
                            s->unk1A = 6;
                            s->unk18 = rand() % bank->unk4;
                            if (s->unk18 >= 0x10) {
                                s->unk18 = 0xF;
                            }
                            a = Fx_GetBankEntry(bank->unk0, s->unk18);
                            s->unk14 = a->unk0;
                            s->unk96 = 0x64;
                            s->unk16 = 0;
                            s->unk20 = a->unk36;
                            s->unk1C = a->unk34;
                            a++;
                            s->unk10 = a;
                            s->unk0 = p->unk0;
                            s->unk4 = p->unk4;
                            s->unk8 = p->unk8;
                            s->unk52 = 0xAA;
                            s->unk48 = 450.0f;
                            s->unk90 = 0x400;
                            s->unk40 = 0;
                            s->unk50 = 0xA0;
                            s->unk51 = 0xA0;
                            s->unk30[0] = 4.0f;
                            s->unk30[1] = 4.0f;
                            s->unk30[2] = 4.0f;
                            s->unk120 = drift;
                        }
                    }
                }
            }
        }
        break;
    }
}

INCLUDE_RODATA("asm/nonmatchings/effects", D_00519660);
/* NON_MATCHING: 50 lines: loads of the place table (D_00519660) are hoisted above the stores of the animation fields
 * in the original; pointer or decl access do not reproduce it. */
/* Emitter of stage 5: three places of func_003A04B8 sprites of bank 6, staggered by 34 frames. */
#if 0
void func_003A0CE8(Effect *e) {
    const FxPlaceF *p;
    s32 i;
    s32 j;
    s32 n;
    Unk8BF350 *bank;
    Effect *f;
    FxSubX *s;
    FxAnim *a;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        e->unk1C++;
    }
    switch (e->unk18) {
    case 0:
        bank = func_00343F20(6);
        n = ((FxAnim *)Fx_GetBankEntry(bank->unk0, 0))->unk0;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < n / 34; j++) {
                f = func_00345780(func_003A04B8);
                s = (FxSubX *)&f->sub;
                if (f != NULL) {
                    f->flags |= 4;
                    s->unk1A = 6;
                    s->unk18 = rand() % bank->unk4;
                    a = Fx_GetBankEntry(bank->unk0, s->unk18);
                    s->unk14 = a->unk0;
                    s->unk16 = j * 34;
                    p = &D_00519660[i];
                    s->unk20 = a->unk36;
                    s->unk1C = a->unk34;
                    a++;
                    s->unk10 = (FxFrame *)a + j * 34;
                    s->unk0 = p->unk0;
                    s->unk4 = p->unk4;
                    s->unk8 = p->unk8;
                    s->unk70[1] = p->unk10;
                    s->unk40 = 1;
                    s->unk30[0] = 1.2f;
                    s->unk90 = 0x400;
                    s->unk48 *= 1.43f;
                    s->unk96 = 0;
                }
            }
        }
        e->unk18++;
        break;
    case 1:
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && e->unk1C % 34 == 0) {
            bank = func_00343F20(6);
            if (bank->unk0 != NULL) {
                for (i = 0; i < 3; i++) {
                    f = func_00345780(func_003A04B8);
                    s = (FxSubX *)&f->sub;
                    if (f != NULL) {
                        f->flags |= 4;
                        s->unk1A = 6;
                        s->unk18 = rand() % bank->unk4;
                        a = Fx_GetBankEntry(bank->unk0, s->unk18);
                        p = &D_00519660[i];
                                s->unk14 = a->unk0;
                        s->unk16 = 0;
                        s->unk20 = a->unk36;
                        s->unk1C = a->unk34;
                        a++;
                        s->unk10 = a;
                        s->unk0 = p->unk0;
                        s->unk4 = p->unk4;
                        s->unk8 = p->unk8;
                        s->unk70[1] = p->unk10;
                        s->unk96 = 0;
                        s->unk40 = 1;
                        s->unk30[0] = 1.2f;
                        s->unk48 *= 1.43f;
                        s->unk90 = 0x400;
                    }
                }
            }
        }
        break;
    }
}
#else
LIT4_WORD(D_005F51A4, 0x3FB70A3D);
LIT4_WORD(D_005F51A8, 0x3F99999A);
LIT4_WORD(D_005F51AC, 0x3F99999A);
LIT4_WORD(D_005F51B0, 0x3FB70A3D);
INCLUDE_ASM("asm/nonmatchings/effects", func_003A0CE8);
#endif

/* Emitter of stages 8 / 17 / 20: every 32 frames three coloured func_003A04B8 sprites of bank 6. */
void func_003A1068(Effect *e) {
    FxPlaceC *p;
    s32 i;
    Unk8BF350 *bank;
    Effect *f;
    FxSubX *s;
    FxAnim *a;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        e->unk1C++;
    }
    switch (e->unk18) {
    case 0:
        e->unk18++;
        break;
    case 1:
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            if (!(e->unk1C & 0x1F)) {
                bank = func_00343F20(6);
                if (bank->unk0 != NULL) {
                    for (i = 0; i < 3; i++) {
                        if (e->unk1A == 0) {
                            p = &D_00436A30[i];
                        } else {
                            p = &D_00436AC0[i];
                        }
                        f = func_00345780(func_003A04B8);
                        s = (FxSubX *)&f->sub;
                        if (f != NULL) {
                            f->flags |= 4;
                            s->unk1A = 6;
                            s->unk18 = rand() % bank->unk4;
                            a = Fx_GetBankEntry(bank->unk0, s->unk18);
                            s->unk14 = a->unk0;
                            s->unk16 = 0;
                            s->unk20 = a->unk36;
                            s->unk1C = a->unk34;
                            s->unk70[1] = 1.5707964f;
                            a++;
                            s->unk10 = a;
                            s->unk40 = 0;
                            s->unk0 = p->unk0;
                            s->unk4 = p->unk4;
                            s->unk8 = p->unk8;
                            s->unk30[0] = p->unk10;
                            s->unk30[1] = p->unk14;
                            s->unk30[2] = p->unk18;
                            *(FxRgbaC *)&s->unk50 = p->unk20;
                            s->unk44 = p->unk24;
                            s->unk48 = p->unk28;
                            s->unk90 = 0x400;
                            s->unk96 = 0xC8;
                        }
                    }
                }
            }
        }
        break;
    }
}

/* Pulsing light: alpha (cos(phase) + 1) / 2, phase advanced by unk124 and wrapped at 2pi. */
void func_003A1298(Effect *e) {
    FxSubX *s = (FxSubX *)&e->sub;

    if (e->unk18 == 0) {
        e->flags |= 4;
        s->unk40 = 1;
        s->unk90 = 0x400;
        s->unk4C = 0xA;
        s->unk96 = 0;
        s->unk58 = 0.0f;
        e->unk18++;
    } else {
        s->unk58 = (cosf(s->unk120) + 1.0f) * 0.5f;
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            s->unk120 += s->unk124;
        }
        if (s->unk120 > 6.2831855f) {
            s->unk120 -= 6.2831855f;
        }
    }
}

/* Spawns the stage lights of stages 9 / 11 / 18 (func_003A1298), one per record up to a negative period. */
void func_003A1378(void) {
    FxLight *l;
    s32 i;
    Effect *e;
    FxSubX *s;

    switch (D_005F5CB6) {
    case 9:
    case 0x12:
        l = D_00436B50;
        break;
    case 0xB:
        l = D_00436EE0;
        break;
    default:
        return;
    }
    i = 0;
    while (l->unk20 >= 0.0f) {
        e = func_00345780(func_003A1298);
        s = (FxSubX *)&e->sub;
        if (e != NULL) {
            e->unk1A = i;
            s->unk0 = l->unk10 * 1000.0f;
            s->unk4 = l->unk14 * 1000.0f;
            s->unk8 = l->unk18 * 1000.0f;
            i++;
            if (l->unk0 < 2) {
                s->unk1A = 6;
                s->unk44 = 160.0f;
            } else {
                s->unk1A = 0xA;
                s->unk44 = 240.0f;
            }
            if (l->unk20 != 0.0f) {
                s->unk124 = 6.2831855f / l->unk20;
                if ((s16)l->unk0 == 1) {
                    s->unk120 = 3.1415927f;
                } else {
                    s->unk120 = Fx_RandFloat() * 3.1415927f;
                }
            } else {
                s->unk124 = 0.0f;
                s->unk120 = 3.1415927f;
            }
            s->unk54 = l->unk24;
        }
        l++;
    }
}

/* Emitter of stage 11: every 32 frames one func_003A04B8 sprite of bank 7 at a fixed place. */
void func_003A1510(Effect *e) {
    Unk8BF350 *bank;
    Effect *f;
    FxSubX *s;
    FxAnim *a;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        e->unk1C++;
    }
    switch (e->unk18) {
    case 0:
        e->unk18++;
        break;
    case 1:
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            if (!(e->unk1C & 0x1F)) {
                bank = func_00343F20(7);
                if (bank->unk0 != NULL) {
                    f = func_00345780(func_003A04B8);
                    s = (FxSubX *)&f->sub;
                    if (f != NULL) {
                        f->flags |= 4;
                        s->unk1A = 8;
                        s->unk18 = rand() % bank->unk4;
                        a = Fx_GetBankEntry(bank->unk0, s->unk18);
                        s->unk14 = a->unk0;
                        s->unk16 = 0;
                        s->unk20 = a->unk36;
                        s->unk1C = a->unk34;
                        s->unk40 = 0;
                        a++;
                        s->unk10 = a;
                        s->unk0 = -2680.0f;
                        s->unk4 = -4240.0f;
                        s->unk8 = -20240.0f;
                        s->unk30[0] = 20.0f;
                        s->unk30[1] = 7.29f;
                        s->unk30[2] = 12.79f;
                        s->unk48 = 140.0f;
                        s->unk44 = 128.0f;
                        s->unk90 = 0x400;
                        s->unk96 = 0x190;
                    }
                }
            }
        }
        break;
    }
}

/* Firefly circling the origin (radius unk8C, angle unk70[1], height unk124 + bob), twinkling. */
void func_003A16C0(Effect *e) {
    FxSubX *s = (FxSubX *)&e->sub;
    f32 t;

    if (e->unk18 == 0) {
        e->flags |= 4;
        s->unk40 = 0;
        s->unk96 = 0;
        s->unk1A = 6;
        s->unk51 = s->unk50 = 0x40;
        s->unk52 = 0x80;
        s->unk90 = 0x400;
        s->unk44 = 128.0f;
        s->unk54 = 10.0f;
        s->unk124 = s->unk4 = Fx_RandFloat() * 3500.0f;
        s->unk120 = 2.0f * (Fx_RandFloat() * 3.1415927f);
        s->unk80[3] = Fx_RandFloat() * 5000.0f + 12000.0f;
        s->unk70[1] = (f32)e->unk1A * 0.31415927f;
        s->unk80[0] = (2.0f * Fx_RandFloat() - 1.0f) * 0.002f;
        if (s->unk80[0] > 0.0f) {
            s->unk80[0] += 0.001f;
        } else {
            s->unk80[0] -= 0.001f;
        }
        s->unk58 = 0.0f;
        s->unk80[1] = Fx_RandFloat() * 3.1415927f;
        s->unk80[2] = Fx_RandFloat() * 0.01f;
        e->unk18++;
    }
    s->unk0 = sinf(s->unk70[1]) * s->unk80[3];
    s->unk8 = cosf(s->unk70[1]) * s->unk80[3];
    s->unk4 = sinf(s->unk120) * 3000.0f + s->unk124;
    s->unk58 = (sinf(s->unk80[1]) + 1.0f) * 0.5f;
    s->unk54 = s->unk58 * 8.0f + 10.0f;
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        s->unk70[1] += s->unk80[0];
        s->unk80[1] += s->unk80[2];
        s->unk120 += Fx_RandFloat() * 0.01f;
    }
}

/* Smaller firefly of the same kind (radius 6800-8800, height 1340-1840). */
void func_003A18F8(Effect *e) {
    FxSubX *s = (FxSubX *)&e->sub;

    if (e->unk18 == 0) {
        e->flags |= 4;
        s->unk40 = 0;
        s->unk96 = 0;
        s->unk1A = 6;
        s->unk51 = s->unk50 = 0x40;
        s->unk52 = 0x80;
        s->unk90 = 0x400;
        s->unk44 = 128.0f;
        s->unk54 = 5.0f;
        s->unk124 = s->unk4 = Fx_RandFloat() * 500.0f + 1340.0f;
        s->unk120 = 2.0f * (Fx_RandFloat() * 3.1415927f);
        s->unk80[3] = Fx_RandFloat() * 2000.0f + 6800.0f;
        s->unk70[1] = (f32)e->unk1A * 0.20943952f;
        s->unk80[0] = (2.0f * Fx_RandFloat() - 1.0f) * 0.002f;
        if (s->unk80[0] > 0.0f) {
            s->unk80[0] += 0.001f;
        } else {
            s->unk80[0] -= 0.001f;
        }
        s->unk58 = 0.0f;
        s->unk80[1] = Fx_RandFloat() * 3.1415927f;
        s->unk80[2] = Fx_RandFloat() * 0.01f;
        e->unk1C = e->unk1A;
        e->unk18++;
    }
    s->unk0 = sinf(s->unk70[1]) * s->unk80[3];
    s->unk8 = cosf(s->unk70[1]) * s->unk80[3];
    s->unk4 = sinf(s->unk120) * 500.0f + s->unk124;
    s->unk58 = (sinf(s->unk80[1]) + 1.0f) * 0.5f;
    s->unk54 = s->unk58 * 3.0f + 5.0f;
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        s->unk70[1] += s->unk80[0];
        s->unk80[1] += s->unk80[2];
        s->unk120 += Fx_RandFloat() * 0.01f;
    }
}

/* Spawns 20 func_003A16C0 and 30 func_003A18F8 fireflies (stage 14). */
void func_003A1B48(void) {
    s32 i;
    Effect *e;

    for (i = 0; i < 0x14; i++) {
        e = func_00345780(func_003A16C0);
        if (e != NULL) {
            e->unk1A = i;
        }
    }
    for (i = 0; i < 0x1E; i++) {
        e = func_00345780(func_003A18F8);
        if (e != NULL) {
            e->unk1A = i;
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/effects", D_005196C0);
/* Lens-flare sprite of slot unk1A: copies screen position, size and alpha from the slot state of func_003A1C78. */
void func_003A1BC8(Effect *e) {
    FxGlow *g = &D_008C47A0[e->unk1A];
    Fx1BC8Sub *s = (Fx1BC8Sub *)&e->sub;

    if (e->unk18 == 0) {
        e->flags = 0x88;
        s->unk1A = 5;
        s->unk1E = D_005196C0[e->unk1A].unk0;
        s->unk40 = 1;
        s->unk90 = 0x400;
        s->unk96 = 0;
        s->unk58 = 0.0f;
        e->unk18++;
    }
    s->unk0 = g->unk0;
    s->unk4 = g->unk4;
    s->unk8 = g->unk8;
    s->unkC = g->unkC;
    s->unk54 = g->unk10;
    s->unk44 = g->unk14;
}

INCLUDE_RODATA("asm/nonmatchings/effects", D_005196C4);
INCLUDE_RODATA("asm/nonmatchings/effects", jtbl_00519730);
/* NON_MATCHING: 75 lines: register allocation (a few FP/GPR temporaries), one hazard nop before an unfilled bc1f
 * (assembler limit), divisors kept as locals. */
/* Lens flare of the stages 1 / 7 / 10-13 / 16: projects the sun position (D_005F6CD4), and while it is on screen
 * lays the nine flare elements along the line through the screen centre, faded near the edges and by the distance
 * of the fighters' joints from the sun's line of sight. */
#if 0
void func_003A1C78(Effect *e) {
    s32 scr[4];
    Vu0Matrix cam;
    FVec4 v;
    FVec4 w;
    Vu0Matrix m;
    s32 i;
    s32 j;
    Effect *f;
    FxGlow *g;
    f32 k;
    f32 dx;
    f32 dy;
    f32 d;
    f32 ang;
    f32 fade;
    f32 range;
    f32 fadeY;
    f32 t;
    f32 min;
    f32 m2;
    f32 sx;
    f32 sy;
    f32 r;

    if (e->unk18 == 0) {
        switch (D_005F5CB6) {
        case 1:
            D_005F6CD4 = D_00437A50;
            D_005F6218 = 0.6f;
            break;
        case 7:
            D_005F6CD4 = D_00437A60;
            D_005F6218 = 0.3f;
            break;
        case 10:
            D_005F6CD4 = D_00437A70;
            D_005F6218 = 0.6f;
            break;
        case 11:
            D_005F6CD4 = D_00437A80;
            D_005F6218 = 0.6f;
            break;
        case 12:
            D_005F6CD4 = D_00437A90;
            D_005F6218 = 0.6f;
            break;
        case 13:
            D_005F6CD4 = D_00437AA0;
            D_005F6218 = 0.7f;
            break;
        case 16:
            D_005F6CD4 = D_00437AB0;
            D_005F6218 = 0.6f;
            break;
        default:
            func_003465C8(e);
            return;
        }
        for (i = 0; i < 9; i++) {
            f = func_00345780(func_003A1BC8);
            if (f != NULL) {
                f->unk1A = i;
            }
        }
        e->unk18++;
    }
    for (i = 0; i < 9; i++) {
        g = &D_008C47A0[i];
        g->unk10 = 0.0f;
        g->unk14 = 0.0f;
    }
    func_00366EA8(cam);
    sceVu0RotTransPers(scr, cam, D_005F6CD4, 1);
    if (scr[3] >= 0) {
        dx = 0x8000 - scr[0];
        dy = 0x8000 - scr[1];
        d = sqrtf(dx * dx + dy * dy) * 1.5f;
        ang = atan2f(dx, dy);
        if (d > 8000.0f) {
            return;
        }
        fade = 1.0f;
        if (d >= 6000.0f) {
            range = 2000.0f;
            fade = (8000.0f - d) / range;
        }
        fadeY = 1.0f;
        t = fabsf(dy);
        if (t <= 2000.0f) {
            fadeY = (2000.0f - t) * 0.00025f + 1.0f;
        }
        func_00366EB8(m);
        sceVu0ApplyMatrix(v, m, D_005F6CD4);
        min = 1e10f;
        v[0] -= (f32)D_00415808[5];
        v[1] -= (f32)-D_00415808[6];
        v[2] -= (f32)-D_00415808[7];
        func_0034A5D0(m, v);
        v[0] = -(f32)D_00415808[5];
        v[1] = -(f32)-D_00415808[6];
        v[2] = -(f32)-D_00415808[7];
        sceVu0ApplyMatrix(m[3], m, v);
        if (FX_FIGHTERS[0].unk127 != 0 && Tag_StepCounters(0, 0) == 0) {
            sceVu0ApplyMatrix(w, m, FX_FIGHTERS[0].unk590);
            t = w[0] * w[0] + w[1] * w[1];
            if (t < min) {
                m2 = t;
            } else {
                m2 = min;
            }
            min = m2;
            sceVu0ApplyMatrix(w, m, FX_FIGHTERS[0].unk11A0);
            t = w[0] * w[0] + w[1] * w[1];
            if (t < min) {
                m2 = t;
            } else {
                m2 = min;
            }
            min = m2;
        }
        if (FX_FIGHTERS[1].unk127 != 0 && Tag_StepCounters(1, 0) == 0) {
            sceVu0ApplyMatrix(w, m, FX_FIGHTERS[1].unk590);
            t = w[0] * w[0] + w[1] * w[1];
            if (t < min) {
                m2 = t;
            } else {
                m2 = min;
            }
            min = m2;
            sceVu0ApplyMatrix(w, m, FX_FIGHTERS[1].unk11A0);
            t = w[0] * w[0] + w[1] * w[1];
            if (t < min) {
                m2 = t;
            } else {
                m2 = min;
            }
            min = m2;
        }
        if (min < 250000.0f) {
            range = 6.25e10f;
            fade *= min * min / range;
        }
        r = d * 0.125f;
        dy = 32768.0f;
        sx = sinf(ang) * r;
        sy = cosf(ang) * r;
        k = D_005F6218;
        dx = dy;
        for (j = 4; j < 9; j++) {
            g = &D_008C47A0[j];
            g->unk0 = dx;
            g->unk8 = 65000;
            g->unk4 = dy;
            g->unkC = 10;
            dx += sx;
            dy += sy;
            g->unk10 = D_005196C0[j].unk4 * fadeY;
            g->unk14 = D_005196C0[j].unk8 * fade * k;
        }
        dx = 32768.0f - sx;
        dy = 32768.0f - sy;
        for (j = 3; j >= 0; j--) {
            g = &D_008C47A0[j];
            g->unk0 = dx;
            g->unk8 = 65000;
            g->unk4 = dy;
            g->unkC = 10;
            dx -= sx;
            dy -= sy;
            g->unk10 = D_005196C0[j].unk4 * fadeY;
            g->unk14 = D_005196C0[j].unk8 * fade * k;
        }
    }
}
#else
LIT4_WORD(D_005F5238, 0x3F19999A);
LIT4_WORD(D_005F523C, 0x3E99999A);
LIT4_WORD(D_005F5240, 0x3F19999A);
LIT4_WORD(D_005F5244, 0x3F19999A);
LIT4_WORD(D_005F5248, 0x3F19999A);
LIT4_WORD(D_005F524C, 0x3F333333);
LIT4_WORD(D_005F5250, 0x3F19999A);
LIT4_WORD(D_005F5254, 0x45BB8000);
LIT4_WORD(D_005F5258, 0x3983126F);
LIT4_WORD(D_005F525C, 0x501502F9);
LIT4_WORD(D_005F5260, 0x48742400);
LIT4_WORD(D_005F5264, 0x5168D4A5);
INCLUDE_ASM("asm/nonmatchings/effects", func_003A1C78);
#endif

/* Stage 20: spawns every animation of bank 7 at pos as func_003A04B8 sprites. */
void func_003A2298(f32 *pos) {
    s32 i;
    Unk8BF350 *bank;
    Effect *e;
    FxSubX *s;
    FxAnim *a;

    if (D_005F5CB6 == 0x14) {
        bank = func_00343F20(7);
        if (bank->unk0 != NULL) {
            for (i = 0; i < bank->unk4; i++) {
                e = func_00345780(func_003A04B8);
                s = (FxSubX *)&e->sub;
                if (e != NULL) {
                    e->flags |= 1;
                    s->unk18 = i;
                    a = Fx_GetBankEntry(bank->unk0, s->unk18);
                    s->unk14 = a->unk0;
                    s->unk16 = 0;
                    s->unk1A = a->unk36 + 8;
                    s->unk20 = a->unk36;
                    s->unk1C = a->unk34;
                    s->unk40 = 1;
                    a++;
                    s->unk10 = a;
                    s->unk0 = pos[0];
                    s->unk4 = pos[1];
                    s->unk8 = pos[2];
                    s->unk30[0] = 8.16f;
                    s->unk30[1] = 8.16f;
                    s->unk30[2] = 8.16f;
                    s->unk44 = 180.0f;
                    s->unk48 = 75.0f;
                    s->unk90 = 0x80;
                    s->unk96 = 0x1E;
                }
            }
        }
    }
}

/* Flash sprite: steps its bank animation frame (unk1E) and frees itself at the end. */
void func_003A23E0(Effect *e) {
    EffectSub *s = &e->sub;
    Unk8BF3A0 *b;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        b = func_00343F08(s->unk1A);
        if (++s->unk1E >= b->unk2) {
            func_003465C8(e);
        }
    }
}

/* Spawns one func_003A23E0 flash at pos. */
void func_003A2470(f32 *pos) {
    s32 i;
    Effect *e;
    EffectSub *s;

    for (i = 0; i < 1; i++) {
        e = func_00345780(func_003A23E0);
        s = &e->sub;
        if (e != NULL) {
            s->unk0 = pos[0];
            s->unk4 = pos[1];
            s->unk8 = pos[2];
            e->flags |= 8;
            s->unk1A = 6;
            s->unk1E = 0;
            s->unk40 = 0;
            if (D_005F5CB6 == 0xD) {
                s->unk50 = 0x87;
                s->unk51 = 0x87;
                s->unk52 = 0x87;
                s->unk44 = 255.0f;
                s->unk48 = 592.0f;
            } else {
                s->unk44 = 240.0f;
                s->unk48 = 592.0f;
            }
            s->unk90 = 0x400;
            s->unk96 = 0xC;
            s->unk54 = 1.5f;
            s->unk58 = 0.0f;
            s->unk4C = 0x64;
        }
    }
}

/* Stage effect controller: on its first active frame spawns the stage's emitters, then the glow controller. */
void Fx_UpdateStageAmbient(Effect *e) {
    Effect *f;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        e->unk1C++;
    }
    switch (e->unk18) {
    case 0:
        e->unk18 = 1;
    case 1:
        if (e->unk1C > 0) {
            switch (D_005F5CB6) {
            case 4:
            case 15:
                func_003464F8(func_003A0A90);
                break;
            case 5:
                func_003464F8(func_003A0CE8);
                break;
            case 11:
                func_003464F8(func_003A1510);
            case 9:
            case 18:
                func_003A1378();
                break;
            case 8:
                f = func_003464F8(func_003A1068);
                if (f != NULL) {
                    f->unk1A = 0;
                }
                break;
            case 17:
            case 20:
                f = func_003464F8(func_003A1068);
                if (f != NULL) {
                    f->unk1A = 1;
                }
                break;
            case 14:
                func_003A1B48();
                break;
            }
            func_003464F8(func_003A1C78);
            e->unk18 = 2;
        }
    case 2:
        break;
    case 3:
        func_00346600();
        break;
    }
}

/* The spark parameters of a stage (0 when the id is out of range). */
FxStageParam *func_003A2738(u32 stage) {
    if (stage > 22) {
        stage = 0;
    }
    return &D_00437AC0[stage];
}

/* Spawns the impact sparks of a hit (kind 0-3, func_0035BF10's second argument): bank 0/3/1/2 of D_008BF350, one
 * effect per animation (kind 3: a random animation, the stage's colour and count, D_00437AC0; on stages 10 and 13 also
 * func_003A2470). */
void func_003A2758(s16 kind, HitRec *p, s32 flag, f32 f) {
    Unk8BF350 *bank = func_00343F20(0);
    f32 sc = 400.0f;
    FxRgba col = D_005F6220[0];
    f32 ang;
    s32 b;
    s32 a;
    s16 n;
    FxParam *prm;
    s16 i;
    Effect *e;
    EffectSub *s;
    FxAnimH *an;

    if (kind & 0x80) {
        return;
    }
    ang = atan2f(p->unk10, -(f32)p->unk14);
    switch (kind) {
    case 0:
        a = 0;
        bank = func_00343F20(0);
        b = 0;
        n = bank->unk4;
        break;
    case 1:
        a = 3;
        bank = func_00343F20(3);
        b = 3;
        n = bank->unk4;
        break;
    case 2:
        a = 1;
        bank = func_00343F20(1);
        b = 1;
        n = bank->unk4;
        break;
    case 3:
        a = 2;
        bank = func_00343F20(2);
        b = 2;
        if (flag != 0) {
            n = 3;
            if (p->pos[1] >= -500) {
                sc = 400.0f / D_00437AC0[D_005F5CB6].unk4;
                col = D_00437AC0[D_005F5CB6].unk8;
            }
        } else {
            n = D_00437AC0[D_005F5CB6].unk0;
            sc = 400.0f / D_00437AC0[D_005F5CB6].unk4;
            col = D_00437AC0[D_005F5CB6].unk8;
            if (D_005F5CB6 == 10 || D_005F5CB6 == 13) {
                f32 v[4];
                v[0] = p->pos[0];
                v[1] = 100.0f;
                v[2] = -(f32)p->pos[2];
                v[3] = 1.0f;
                func_003A2470(v);
            }
        }
        break;
    default:
        return;
    }
    if (bank->unk0 == NULL) {
        return;
    }
    switch (kind) {
    case 0:
        prm = (FxParam *)func_00343F38(0x24);
        break;
    case 2:
        prm = (FxParam *)func_00343F38(0x25);
        break;
    case 1:
        prm = (FxParam *)func_00343F38(0x26);
        break;
    default:
        prm = NULL;
        break;
    }
    for (i = 0; i < n; i++) {
        if (kind == 3) {
            e = func_00345780(func_003A06F0);
        } else {
            e = func_00345780(func_003A05F8);
        }
        if (e != NULL) {
            s = &e->sub;
            e->flags |= 1;
            s->unk1A = a;
            if (b == 2) {
                s->unk18 = rand() % bank->unk4;
            } else {
                s->unk18 = i;
            }
            an = Fx_GetBankEntry(bank->unk0, s->unk18);
            s->unk14 = an->unk0;
            s->unk16 = 0;
            s->unk1C = an->unk34;
            s->unk20 = an->unk36;
            s->unk10 = ++an;
            s->unk0 = p->pos[0];
            s->unk4 = -(f32)p->pos[1];
            s->unk8 = -(f32)p->pos[2];
            s->unk90 = 0x80;
            s->unk96 = 0x1E;
            s->unk70[1] = ang;
            s->unk80[1] = f;
            if (prm != NULL) {
                s->unk30[0] = prm->unk0[0];
                s->unk30[1] = prm->unk0[1];
                s->unk30[2] = prm->unk0[2];
                s->unk40 = prm->unkC;
                s->unk44 = prm->unk10;
                s->unk48 = prm->unk14;
            }
            switch (kind) {
            case 3:
                s->unk40 = 0;
                *(FxRgba *)&s->unk50 = col;
                s->unk96 = 100;
                s->unk48 = sc;
                break;
            case 0:
            case 2:
                s->unk4C = 40;
                break;
            default:
                s->unk4C = 40;
                break;
            }
        }
    }
}

/* Spawns a fighter's hit flash (side 0/1): the effect kind of its character / costume (func_0035C970 /
 * func_00347730), a light flash, one effect per animation of bank side + 4 in the side ring. Characters 0x13 / 0x1D /
 * 0xF / 0x21 in moves 0x25 / 0x26 get func_003A2E20 instead. */
void func_003A2BC0(s16 side, s16 unused, HitRec *p, s32 d) {
    Fighter *f;
#ifdef PORT
    f32 pos[4]; /* Light_StartFlash copies four floats: pos[3] is a stale stack word on the EE (sp + 12), 0 here */
#else
    f32 pos[3];
#endif
    f32 ang;
    s16 c;
    s16 base;
    FxKind *k;
    EffectFunc cb;
    Unk8BF350 *bank;
    s16 i;
    Effect *e;
    EffectSub *s;
    FxAnimH *an;
    struct FighterMove *m;

    if (side >= 2) {
        return;
    }
    f = &gFighters[side];
    if (f->unk2A == 0x13 || f->unk2A == 0x1D || f->unk2A == 0xF || f->unk2A == 0x21) {
        m = f->unkC0;
        if (m == func_00368248(0x25) || m == func_00368248(0x26)) {
            if (func_00343F20(9)->unk0 != NULL) {
                func_003A2E20(p);
            }
            return;
        }
    }
    pos[0] = p->pos[0];
    pos[1] = -(f32)p->pos[1];
    pos[2] = -(f32)p->pos[2];
#ifdef PORT
    pos[3] = 0.0f;
#endif
    ang = atan2f(p->unk10, -(f32)p->unk14);
    c = func_0035C970(f->unk26);
    base = side * 3 + 14;
    k = func_00347730(c);
    if (k == NULL) {
        return;
    }
    cb = k->update;
    bank = func_00343F20(side + 4);
    if (bank->unk0 == NULL) {
        return;
    }
    Light_StartFlash(pos, k->rgb);
    for (i = 0; i < bank->unk4; i++) {
        e = func_00346030(side, cb);
        if (e != NULL) {
            e->flags |= 2;
            if (side != 0) {
                e->flags |= 0x20;
            }
            s = &e->sub;
            s->unk18 = i;
            an = Fx_GetBankEntry(bank->unk0, s->unk18);
            s->unk14 = an->unk0;
            s->unk16 = 0;
            s->unk1A = base + an->unk36;
            s->unk1C = an->unk34;
            s->unk20 = an->unk36;
            s->unk22 = c;
            s->unk10 = ++an;
            s->unk0 = pos[0];
            s->unk4 = pos[1];
            s->unk8 = pos[2];
            s->unk70[1] = ang;
        }
    }
}

void func_003A2E20(HitRec *p) {
#ifdef PORT
    f32 pos[4]; /* Light_StartFlash copies four floats: pos[3] is a stale stack word on the EE (sp + 12), 0 here */
#else
    f32 pos[3];
#endif
    f32 ang;
    FxKind *k;
    EffectFunc cb;
    Unk8BF350 *bank;
    s16 i;
    Effect *e;
    EffectSub *s;
    FxAnimH *an;

    if (func_003ABCC0() != 0) {
        return;
    }
    pos[0] = p->pos[0];
    pos[1] = -(f32)p->pos[1];
    pos[2] = -(f32)p->pos[2];
#ifdef PORT
    pos[3] = 0.0f;
#endif
    ang = atan2f(p->unk10, -(f32)p->unk14);
    k = func_00347730(0x22);
    if (k == NULL) {
        return;
    }
    cb = k->update;
    bank = func_00343F20(9);
    if (bank->unk0 == NULL) {
        return;
    }
    Light_StartFlash(pos, k->rgb);
    for (i = 0; i < bank->unk4; i++) {
        e = func_00345780(cb);
        if (e != NULL) {
            s = &e->sub;
            e->flags |= 8;
            s->unk18 = i;
            an = Fx_GetBankEntry(bank->unk0, s->unk18);
            s->unk14 = an->unk0;
            s->unk16 = 0;
            s->unk1A = an->unk36 + 12;
            s->unk1C = an->unk34;
            s->unk20 = an->unk36;
            s->unk22 = 0x22;
            s->unk10 = ++an;
            s->unk0 = pos[0];
            s->unk4 = pos[1];
            s->unk8 = pos[2];
            s->unk70[1] = ang;
        }
    }
}

/* Hit ring: start recording (frame 0, record 0). */
void HitLog_Reset(void) {
    D_005F6CD8 = 1;
    D_005F6CDC = D_008C48C0;
    D_008C48C0[0].n = 0;
    D_005F6CE8 = D_008C4C80;
    D_008C48C0[0].rec = D_008C4C80;
    D_005F6CE4 = 0;
}

/* Hit ring: resume recording. */
void HitLog_Enable(void) {
    D_005F6CD8 = 1;
}

/* Hit ring: record one hit event (at most 60 per pass of the frame ring, D_005F6CE4). */
void HitLog_Add(s16 kind, s16 arg, HitRec *p) {
    HitRec *w;

    if (D_005F6CD8 != 0 && ++D_005F6CE4 <= 60) {
        D_005F6CDC->n++;
        w = D_005F6CE8;
        w->pos[0] = p->pos[0];
        w->pos[1] = p->pos[1];
        w->pos[2] = p->pos[2];
        w->unk10 = p->unk10;
        w->unk12 = p->unk12;
        w->unk14 = p->unk14;
        w->unk16 = kind;
        D_005F6CE8->unk17 = arg;
        D_005F6CE8++;
        if (D_005F6CE8 - D_008C4C80 >= sizeof(D_008C4C80) / sizeof(HitRec)) {
            D_005F6CE8 = D_008C4C80;
        }
    }
}

/* Hit ring: next frame. */
void HitLog_NextFrame(void) {
    HitFrame *fr;

    if (D_005F6CD8 != 0) {
        fr = D_005F6CDC + 1;
        D_005F6CDC = fr;
        if (fr - D_008C48C0 >= sizeof(D_008C48C0) / sizeof(HitFrame)) {
            fr = D_008C48C0;
            D_005F6CE4 = 0;
            D_005F6CDC = fr;
        }
        fr->n = 0;
        fr->rec = D_005F6CE8;
    }
}

/* Hit ring: stop recording. */
void HitLog_Disable(void) {
    D_005F6CD8 = 0;
}

/* Hit ring: start the replay n frames back. */
void HitLog_Rewind(s16 n) {
    HitFrame *fr = D_005F6CDC - n;

    D_005F6CE0 = fr;
    if (fr - D_008C48C0 < 0) {
        D_005F6CE0 = fr + 120;
    }
}

/* Hit ring: replay the events of one frame through func_0035BF10, then step to the next frame. */
void HitLog_ReplayFrame(s16 on) {
    HitRec tmp;
    HitRec *r;
    s16 n;

    if (on != 0) {
        n = D_005F6CE0->n;
        r = D_005F6CE0->rec;
        while (n > 0) {
            tmp.pos[0] = r->pos[0];
            tmp.pos[1] = r->pos[1];
            tmp.pos[2] = r->pos[2];
            tmp.unk10 = r->unk10;
            tmp.unk12 = r->unk12;
            tmp.unk14 = r->unk14;
            func_0035BF10(r->unk16, r->unk17, &tmp, 0);
            r++;
            if (r - D_008C4C80 >= sizeof(D_008C4C80) / sizeof(HitRec)) {
                r = D_008C4C80;
            }
            n--;
        }
        D_005F6CE0++;
        if (D_005F6CE0 - D_008C48C0 >= sizeof(D_008C48C0) / sizeof(HitFrame)) {
            D_005F6CE0 = D_008C48C0;
        }
    }
}

/* Hit ring: save the ring into slot (records and the last 120 frames). */
void HitLog_Save(s16 slot) {
    s16 i;
    s16 j;
    HitFrame *src;
    HitFrame *dst;

    for (i = 0; i < 60; i++) {
        D_008C7020[slot][i] = D_008C4C80[i];
    }
    src = D_005F6CDC;
    dst = &D_008C5220[slot][119];
    for (j = 120; j > 0; j--) {
        if (src - D_008C48C0 < 0) {
            src = &D_008C48C0[119];
        }
        *dst = *src;
        dst--;
        src--;
    }
}

/* Hit ring: restore slot (0-7), replay position `back` frames before the end. */
void HitLog_Load(s16 slot, s16 back) {
    s16 i;

    if ((u16)slot < 8) {
        for (i = 0; i < 60; i++) {
            D_008C4C80[i] = D_008C7020[slot][i];
        }
        for (i = 0; i < 120; i++) {
            D_008C48C0[i] = D_008C5220[slot][i];
        }
        D_005F6CE0 = &D_008C48C0[119 - back];
    }
}

/* Yaw (out[1]) and pitch (out[0]) of a direction vector. */
void func_003A3438(f32 *v, f32 *out) {
    f32 x;
    f32 yaw = atan2f(v[0], v[2]);
    f32 s;

    x = fabsf(v[0]);
    s = sinf(yaw);

    if (s == 0.0f) {
        s = 1e-6f;
    }
    s = atan2f(v[1], fabsf(x / s));
    out[1] = yaw;
    out[0] = s;
}

/* Horizontal velocity of length r along the yaw unk70[1]. */
void func_003A34C8(EffectSub *s, f32 r) {
    s->unk80[0] = sinf(s->unk70[1]) * r;
    s->unk80[2] = cosf(s->unk70[1]) * r;
}

/* Velocity of length r along pitch unk70[0] and yaw unk70[1]. */
void func_003A3518(EffectSub *s, f32 r) {
    f32 h;

    s->unk80[1] = sinf(s->unk70[0]) * r;
    h = cosf(s->unk70[0]) * r;
    s->unk80[0] = sinf(s->unk70[1]) * h;
    s->unk80[2] = cosf(s->unk70[1]) * h;
}

/* 0 while the fighter's current move has id byte `id` and its frame is at most the move's unk2C, else 1. */
s32 func_003A3588(s32 side, s32 id) {
    Fighter *f = &gFighters[side & 1];
    struct FighterMove *m = f->unkC0;

    if (id != m->unk28) {
        return 1;
    }
    return f->unkC4 < m->unk2C;
}

/* Update callback: a particle of the hit burst (func_003A6110) on an eased orbit.
 * The original keeps x + 0.0f and the division by 3.0f, so the file is built without -ffast-math. */
void func_003A35C8(Effect *e) {
    FxSub3A35C8 *s = (FxSub3A35C8 *)&e->sub;
    f32 r;
    f32 h;

    if (e->unk18 == 0) {
        s->b.unk70[0] += (Fx_RandFloat() * 2.0f - 1.0f) * 1.5707964f * 2.0f / 3.0f;
        s->b.unk70[1] += (Fx_RandFloat() * 2.0f - 1.0f) * 1.5707964f * 2.0f / 3.0f;
        s->unk120 = Fx_RandFloat() * 512.0f + 0.0f;
        s->unk124 = Fx_RandFloat() * 512.0f + 1664.0f;
        s->unk128 = 0.0f;
        s->b.unk14 = (rand() & 0x1F) + 10;
        s->b.unk16 = 0;
        if (s->b.unk1A == 4) {
            s->unkE0 = -(s->b.unk54 * 0.5f / s->b.unk14);
        } else {
            s->unkE0 = -(s->b.unk54 / s->b.unk14);
        }
        func_00343F08(s->b.unk1A);
        s->b.unk90 = 0x80;
        s->b.unk96 = 10;
        s->b.unk40 = 1;
        s->b.unk58 = 0.0f;
        s->unkE8 = s->b.unk1E;
        s->unkEC = 0.0f;
        s->unkE4 = 10.0f / s->b.unk14;
        e->unk18++;
    } else {
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        s->unk128 += 3.1415927f / s->b.unk14;
        r = s->unk124 - s->unk120;
        r *= (cosf(s->unk128) + 1.0f) * 0.06247f + 0.5f;
        r += s->unk120;
        s->b.unk80[1] = sinf(s->b.unk70[0]) * r;
        h = cosf(s->b.unk70[0]) * r;
        s->b.unk80[0] = sinf(s->b.unk70[1]) * h;
        s->b.unk80[2] = cosf(s->b.unk70[1]) * h;
        s->b.unk0 += s->b.unk80[0] * 0.03125f;
        s->b.unk4 += s->b.unk80[1] * 0.03125f;
        s->b.unk8 += s->b.unk80[2] * 0.03125f;
        s->b.unk54 += s->unkE0;
        s->b.unk44 += s->unkEC;
        s->unkE8 += s->unkE4;
        s->b.unk16++;
    }
    s->b.unk1E = s->unkE8;
    if (s->b.unk16 >= s->b.unk14) {
        func_003465C8(e);
    }
    }
}

/* Update callback: a drifting particle of the hit burst (func_003A6110). */
void func_003A3928(Effect *e) {
    FxSub3A35C8 *s = (FxSub3A35C8 *)&e->sub;

    if (e->unk18 == 0) {
        s->b.unk0 += (rand() & 0xFF) - 0x7F;
        s->b.unk4 += (rand() & 0xFF) - 0x7F;
        s->b.unk8 += (rand() & 0xFF) - 0x7F;
        s->b.unk70[0] += (Fx_RandFloat() * 2.0f - 1.0f) * 0.09817477f;
        s->b.unk70[1] += (Fx_RandFloat() * 2.0f - 1.0f) * 0.09817477f;
        func_003A3518(&s->b, Fx_RandFloat() * 159.0f);
        s->b.unk0 += s->b.unk80[0] * 0.125f;
        s->b.unk4 += s->b.unk80[1] * 0.125f;
        s->b.unk8 += s->b.unk80[2] * 0.125f;
        s->b.unk14 = (rand() & 0x1F) + 20;
        s->b.unk16 = 0;
        if (s->b.unk1A == 4) {
            s->unkE0 = -(s->b.unk54 * 0.5f / s->b.unk14);
        } else {
            s->unkE0 = -(s->b.unk54 / s->b.unk14);
        }
        func_00343F08(s->b.unk1A);
        s->b.unk90 = 0x80;
        s->b.unk96 = 10;
        s->b.unk40 = 1;
        s->b.unk58 = 0.0f;
        s->unkE8 = s->b.unk1E;
        s->unkEC = 0.0f;
        s->unkE4 = 10.0f / s->b.unk14;
        e->unk18++;
    } else {
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        s->b.unk0 += s->b.unk80[0];
        s->b.unk4 += s->b.unk80[1];
        s->b.unk8 += s->b.unk80[2];
        s->b.unk54 += s->unkE0;
        s->b.unk44 += s->unkEC;
        s->unkE8 += s->unkE4;
        s->b.unk16++;
    }
    s->b.unk1E = s->unkE8;
    if (s->b.unk16 >= s->b.unk14) {
        func_003465C8(e);
    }
    }
}

RODATA_ALIGN16();
INCLUDE_RODATA("asm/nonmatchings/effects", D_005197C0);
void func_003A3C08(Effect *e) {
    FxSub3A35C8 *s = (FxSub3A35C8 *)&e->sub;
    s32 bits;
    FxFighterJoints *f;
    Effect *n;
    EffectSub *ns;
    FxJoint *jt;
    s32 j;
    s32 k;
    s32 i;
    Fx3B87B0 *p;
    FxVec4S v;

    if (e->unk18 == 0) {
        e->flags |= 8;
        s->b.unk1A = 1;
        s->b.unk90 = 0x400;
        s->b.unk40 = 1;
        s->b.unk44 = 192.0f;
        s->b.unk54 = 0.3f;
        s->b.unk4C = 50;
        s->b.unk96 = 0;
        s->b.unk58 = 0.0f;
        s->unkE0 = Fx_RandFloat() * 3.1415927f;
        s->unkE4 = 1.5707964f;
        e->unk18++;
    }
    if (!(e->flags & 0x20)) {
        bits = D_005F6CF0[0];
        f = (FxFighterJoints *)&gFighters[0];
    } else {
        bits = D_005F6CF4;
        f = (FxFighterJoints *)&gFighters[1];
    }
    if (!((bits >> e->unk1A) & 1)) {
        e->flags &= ~8u;
        return;
    }
    e->flags |= 8;
    if ((rand() & 0xF) == 0 && D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        n = func_00345780(func_003A4128);
        if (n != NULL) {
            ns = &n->sub;
            ns->unk0 = s->b.unk0;
            ns->unk4 = s->b.unk4;
            ns->unk8 = s->b.unk8;
        }
    }
    if (bits < 0 && D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        n = func_00345780(func_003A3FC0);
        if (n != NULL) {
            ns = &n->sub;
            ns->unk0 = s->b.unk0;
            ns->unk4 = s->b.unk4;
            ns->unk8 = s->b.unk8;
        }
    }
    j = 0;
    switch (e->unk1A) {
    case 6:
        j = 0x2D;
        break;
    case 10:
        j = 0x46;
        break;
    case 14:
        j = 0x58;
        break;
    case 17:
        j = 0x61;
        break;
    }
    s->b.unk0 = *(f32 *)((u8 *)f + j * 0xC0 + 0xDE0);
    s->b.unk4 = *(f32 *)((u8 *)f + j * 0xC0 + 0xDE4);
    s->b.unk8 = *(f32 *)((u8 *)f + j * 0xC0 + 0xDE8);
    if (f->unk2A == 0x14 && e->unk1A == 6) {
        p = func_003B87B0(f->unk22);
        if (p != NULL) {
            k = -1;
            for (i = 0; i < p->unk4; i++) {
                if (p->unkC[i + 0x62]->unk1C == 0x15) {
                    k = i;
                    break;
                }
            }
            if (k >= 0) {
                k += 0x62;
                v = D_005197C0;
                sceVu0ApplyMatrix(&s->b.unk0, f->joint[k].m, v.v);
            }
        }
    }
    s->b.unk54 = sinf(s->unkE0) * 0.05f + 0.3f;
    s->b.unk58 = (1.0f - sinf(s->unkE0)) * 0.2f;
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        s->unkE0 += s->unkE4;
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && (rand() & 0x80)) {
            Light_StartFlash(&s->b.unk0, D_005F6228);
        }
    }
}

void func_003A3FC0(Effect *e) {
    EffectSub *s = &e->sub;

    if (e->unk18 == 0) {
        e->flags |= 8;
        s->unk1A = 1;
        s->unk90 = 0x400;
        s->unk40 = 1;
        s->unk44 = 140.0f;
        s->unk54 = 0.25f;
        s->unk4C = 50;
        s->unk14 = 0x20;
        s->unk96 = 0;
        s->unk58 = 0.0f;
        s->unk80[1] = 0.0f;
        s->unk16 = 0;
        e->unk18++;
    }
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        s->unk80[1] -= 0.75f;
        s->unk4 += s->unk80[1];
        s->unk54 -= 0.2f / s->unk14;
        s->unk58 += 1.0f / s->unk14;
        s->unk16++;
    }
    if (s->unk16 >= s->unk14) {
        func_003465C8(e);
    }
}

void func_003A4128(Effect *e) {
    EffectSub *s = &e->sub;

    if (e->unk18 == 0) {
        e->flags |= 8;
        s->unk96 = 0;
        s->unk44 = 128.0f;
        s->unk58 = 0.0f;
        s->unk80[1] = 0.0f;
        s->unk1A = 1;
        s->unk90 = 0x400;
        s->unk40 = 1;
        s->unk54 = 0.08f;
        s->unk4C = 50;
        s->unk0 += Fx_RandFloat() * 256.0f - 128.0f;
        s->unk8 += Fx_RandFloat() * 256.0f - 128.0f;
        s->unk16 = 0;
        s->unk14 = 0x20;
        e->unk18++;
    }
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        s->unk80[1] += 1.125f;
        s->unk4 += s->unk80[1];
        s->unk58 += 1.0f / s->unk14;
        s->unk16++;
    }
    if (s->unk16 >= s->unk14) {
        func_003465C8(e);
    }
}

/* Spawns the eight body-part glows (four per side, D_00437BE0 gives the parts) and clears their bits. */
void func_003A42B8(void) {
    s32 i;
    s32 j;
    Effect *e;

    for (i = 0; i < 2; i++) {
        D_005F6CF0[i] = 0;
        for (j = 0; j < 4; j++) {
            e = func_00345780(func_003A3C08);
            if (e != NULL) {
                e->unk1A = D_00437BE0[j];
                if (i != 0) {
                    e->flags |= 0x20;
                }
            }
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/effects", D_005197D0);
/* Every frame: the body-part bits of each fighter from its move's bytes 0x28-0x2B (D_005197D0); 0x440 when all four
 * are 0. */
void func_003A4370(void) {
    s32 i;
    s32 j;
    Fighter *f;
    struct FighterMove *m;
    s32 v[4];

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        D_005F6CF0[0] = 0;
        D_005F6CF4 = 0;
        for (i = 0; i < 2; i++) {
            f = &gFighters[i];
            m = f->unkC0;
            if (func_00365AC0(f, f->unk16A) != 0) {
                if (m->unk28 + m->unk29 + m->unk2A + m->unk2B == 0) {
                    D_005F6CF0[i] |= 0x440;
                } else if (!(m->unk2D < f->unkC4)) {
                    v[0] = m->unk28;
                    v[1] = m->unk29;
                    v[2] = m->unk2A;
                    v[3] = m->unk2B;
                    for (j = 0; j < 4; j++) {
                        if (D_005197D0[v[j] * 2 + 1] != 0) {
                            D_005F6CF0[i] = D_005F6CF0[i] | (1 << D_005197D0[v[j] * 2 + 1]) | 0x80000000;
                        }
                    }
                }
            }
        }
    }
}

/* NON_MATCHING: 13 lines: register allocation only: side shift in v1 (orig a0), f1/f2 swapped for the literals of
 * sub-cases 0 and 2, v0/v1 swapped in sub-case 2. */
#if 0
void func_003A44F8(Effect *e) {
    FxSparkSub *x = (FxSparkSub *)&e->sub;
    EffectSub *s = &x->b;
    Effect *n;
    s32 side;
    EffectSub *ns;

    side = (e->flags >> 5) & 1;
    if ((e->flags & 0x10) && func_003A3588(side, 0x18) != 0) {
        e->flags &= 0xFFFFFFEF;
    }
    switch (e->unk18) {
    case 0:
        s->unk70[0] += (2.0f * Fx_RandFloat() - 1.0f) * 0.09817477f;
        s->unk70[1] += (2.0f * Fx_RandFloat() - 1.0f) * 0.049087387f;
        func_003A3518(s, 160.0f);
        x->unkE0[0] = s->unk0;
        x->unkE0[1] = s->unk4;
        x->unkE0[2] = s->unk8;
        if (e->flags & 0x20) {
            s->unk1A = 0x17;
        } else {
            s->unk1A = 0x14;
        }
        s->unk1E = rand() % 30;
        s->unk90 = 0x400;
        s->unk96 = 10;
        s->unk40 = 1;
        s->unk44 = 192.0f;
        s->unk54 = 0.2f;
        s->unk58 = 0;
        e->unk1A = 0;
        e->unk18++;
        break;
    case 1:
        switch (e->unk1A) {
        case 0:
            s->unk54 = s->unk16 * ((0.6f - 0.2f) / 8.0f) + 0.2f;
            if (s->unk16 >= 8) {
                s->unk16 = 0;
                s->unk54 = 0.6f;
                s->unk14 = ((rand() >> 2) & 3) + 5;
                func_003A3518(s, 256.0f);
                e->unk1A++;
            }
            break;
        case 1:
            if (s->unk16 >= s->unk14) {
                s->unk16 = 0;
                e->unk1A++;
            }
            break;
        case 2:
            s->unk58 = s->unk16 * 0.071428575f;
            if (s->unk16 >= 7) {
                func_003465C8(e);
                return;
            }
            break;
        }
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            s->unk1E++;
            s->unk1E %= 30;
            x->unkE0[0] = s->unk0;
            x->unkE0[1] = s->unk4;
            x->unkE0[2] = s->unk8;
            s->unk4 += s->unk80[1];
            s->unk16++;
            s->unk0 += s->unk80[0];
            s->unk8 += s->unk80[2];
            if (s->unk4 < 216.0f) {
                s->unk70[0] = 0;
                s->unk80[1] = 0;
                s->unk4 = 216.0f;
                func_003A34C8(s, 256.0f);
            }
            if (!(rand() & 0x3F)) {
                n = func_00345780(func_003A4970);
                if (n != NULL) {
                    ns = &n->sub;
                    ns->unk0 = s->unk0;
                    ns->unk4 = s->unk4;
                    ns->unk8 = s->unk8;
                    ns->unk70[0] = s->unk70[0];
                    ns->unk70[1] = s->unk70[1];
                    if (e->flags & 0x20) {
                        n->flags |= 0x20;
                    }
                }
            }
            if (s->unk4 <= 216.0f && rand() % 63 == 0) {
                n = func_00345780(func_003A4C28);
                if (n != NULL) {
                    n->sub.unk0 = s->unk0;
                    n->sub.unk8 = s->unk8;
                    if (e->flags & 0x20) {
                        n->flags |= 0x20;
                    }
                }
            }
            if ((u16)e->unk1A < 2 && D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && !(rand() & 0xF)) {
                Light_StartFlash(&s->unk0, D_005F6230);
            }
        }
        break;
    }
}
#else
LIT4_WORD(D_005F52A0, 0x3DC90FDB);
LIT4_WORD(D_005F52A4, 0x3D490FDB);
LIT4_WORD(D_005F52A8, 0x3E4CCCCD);
LIT4_WORD(D_005F52AC, 0x3D4CCCCE);
LIT4_WORD(D_005F52B0, 0x3E4CCCCD);
LIT4_WORD(D_005F52B4, 0x3F19999A);
LIT4_WORD(D_005F52B8, 0x3D924925);
INCLUDE_ASM("asm/nonmatchings/effects", func_003A44F8);
#endif

void func_003A4970(Effect *e) {
    EffectSub *s = &e->sub;
    f32 r;
    f32 speed;

    switch (e->unk18) {
    case 0:
        e->flags |= 2;
        r = 0.09817477f;
        s->unk0 += (2.0f * Fx_RandFloat() - 1.0f) * 128.0f;
        s->unk4 += (2.0f * Fx_RandFloat() - 1.0f) * 128.0f;
        s->unk8 += (2.0f * Fx_RandFloat() - 1.0f) * 128.0f;
        s->unk16 = 0;
        s->unk14 = (rand() & 0x1F) + 0x10;
        speed = Fx_RandFloat() * 63.0f;
        speed *= 1.25f;
        speed += 32.0f;
        s->unk70[0] += (2.0f * Fx_RandFloat() - 1.0f) * r;
        s->unk70[1] += (2.0f * Fx_RandFloat() - 1.0f) * r;
        func_003A3518(s, speed);
        if (e->flags & 0x20) {
            s->unk1A = 0x17;
        } else {
            s->unk1A = 0x14;
        }
        s->unk1E = rand() % 30;
        s->unk90 = 0x80;
        s->unk96 = 10;
        s->unk40 = 1;
        s->unk44 = 192.0f;
        s->unk54 = 0.2f;
        s->unk58 = 0;
        e->unk18++;
        break;
    case 1:
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            s->unk80[1] += 1.5f;
            s->unk0 += s->unk80[0];
            s->unk4 += s->unk80[1];
            s->unk16++;
            s->unk8 += s->unk80[2];
            s->unk1E++;
            s->unk1E %= 30;
        }
        if (s->unk4 < 0.0f || s->unk16 >= s->unk14) {
            func_003465C8(e);
        }
        break;
    }
}

void func_003A4C28(Effect *e) {
    EffectSub *s = &e->sub;
    Effect *n;
    EffectSub *ns;

    switch (e->unk18) {
    case 0:
        e->flags |= 2;
        s->unk80[0] = 0;
        s->unk80[1] = 0;
        s->unk80[2] = 0;
        s->unk4 = 366.0f;
        s->unk14 = (rand() & 0x1F) + 60;
        s->unk16 = 0;
        if (e->flags & 0x20) {
            s->unk1A = 0x18;
        } else {
            s->unk1A = 0x15;
        }
        s->unk1E = rand() % 30;
        s->unk90 = 0x400;
        s->unk96 = 10;
        s->unk40 = 1;
        s->unk44 = 128.0f;
        s->unk54 = Fx_RandFloat() * 0.3f + 0.2f;
        s->unk58 = 0;
        e->unk1A = 0;
        e->unk18++;
        break;
    case 1:
        switch (e->unk1A) {
        case 0:
            if (s->unk16 >= s->unk14) {
                s->unk16 = 0;
                s->unk14 = 32;
                e->unk1A++;
            }
            break;
        case 1:
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                s->unk58 += 1.0f / s->unk14;
            }
            if (s->unk16 >= s->unk14) {
                func_003465C8(e);
                return;
            }
            break;
        }
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            s->unk1E++;
            s->unk1E %= 30;
            s->unk16++;
        }
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && !(rand() & 0x1F)) {
            n = func_00345780(func_003A4970);
            if (n != NULL) {
                ns = &n->sub;
                ns->unk0 = s->unk0;
                ns->unk4 = s->unk4;
                ns->unk8 = s->unk8;
                ns->unk70[0] = 1.5707964f;
                ns->unk70[1] = 0;
                if (e->flags & 0x20) {
                    n->flags |= 0x20;
                }
            }
        }
        break;
    }
}

s32 func_003A4EE8(Fighter *f) {
    FVec4 v[2];
    f32 dir[4];
    s32 i;
    s32 alt = 0;
    Effect *e;
    EffectSub *s;

    if ((f->unk1E == 0 && func_00346378(1) != 0) || (f->unk1E == 1 && func_00346378(0) != 0)) {
        alt = 1;
    }
    for (i = 0; i < 2; i++) {
        if (i == 0) {
            sceVu0ApplyMatrix(v[0], FX_FIGHTER(f)->unkDB0[13].unk0, D_00437BF0[f->unk1E * 2]);
        } else {
            sceVu0ApplyMatrix(v[1], FX_FIGHTER(f)->unkDB0[13].unk0, D_00437BF0[f->unk1E * 2 + i]);
        }
    }
    e = func_00346030(f->unk1E, func_003A44F8);
    if (e != NULL) {
        if (alt == 0) {
            e->flags |= 0x12;
        } else {
            e->flags |= 2;
        }
        if (f->unk1E != 0) {
            e->flags |= 0x20;
        }
        s = &e->sub;
        s->unk0 = v[0][0];
        s->unk4 = v[0][1];
        s->unk8 = v[0][2];
        dir[0] = v[1][0] - v[0][0];
        dir[1] = v[1][1] - v[0][1];
        dir[2] = v[1][2] - v[0][2];
        func_003A3438(dir, s->unk70);
    }
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        Light_StartFlash(v[0], D_005F6230);
    }
}

void func_003A50B8(Effect *e) {
    EffectSub *s = &e->sub;
    Fighter *f;
    Effect *n;
    EffectSub *ns;

    if (e->flags & 0x20) {
        f = &gFighters[0];
    } else {
        f = &D_00884EE0;
    }
    switch (e->unk18) {
    case 0:
        s->unk0 = FX_FIGHTER(f)->unkDB0[s->unk18].unk0[3][0];
        s->unk4 = FX_FIGHTER(f)->unkDB0[s->unk18].unk0[3][1];
        s->unk8 = FX_FIGHTER(f)->unkDB0[s->unk18].unk0[3][2];
        if (e->flags & 0x20) {
            s->unk1A = 0x17;
        } else {
            s->unk1A = 0x14;
        }
        s->unk1E = rand() % 30;
        s->unk90 = 0x400;
        s->unk96 = 10;
        s->unk40 = 1;
        s->unk44 = 128.0f;
        s->unk54 = 0.4f;
        s->unk58 = 0;
        s->unk4C = 200;
        s->unk14 = (rand() & 0xF) + 60;
        s->unk16 = 0;
        s->unk22 = f->unk2A;
        e->unk1A = 0;
        e->unk18++;
        break;
    case 1:
        switch (e->unk1A) {
        case 0:
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                s->unk1E++;
                s->unk1E %= 30;
                s->unk16++;
            }
            if (e->flags & 0x20) {
                func_00346330(0);
            } else {
                func_00346330(1);
            }
            s->unk0 = FX_FIGHTER(f)->unkDB0[s->unk18].unk0[3][0];
            s->unk4 = FX_FIGHTER(f)->unkDB0[s->unk18].unk0[3][1];
            s->unk8 = FX_FIGHTER(f)->unkDB0[s->unk18].unk0[3][2];
            if (s->unk16 >= s->unk14) {
                s->unk16 = 0;
                s->unk14 = 16;
                s->unk80[1] = 0;
                e->unk1A++;
            }
            break;
        case 1:
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                s->unk1E++;
                s->unk1E %= 30;
                s->unk16++;
                s->unk80[1] += 1.5f;
                s->unk58 += 1.0f / s->unk14;
                s->unk4 += s->unk80[1];
                if (s->unk16 >= s->unk14) {
                    func_003465C8(e);
                    return;
                }
            }
            break;
        }
        if (s->unk22 != f->unk2A) {
            func_003465C8(e);
            return;
        }
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && !(rand() & 0xF)) {
            Light_StartFlash(&s->unk0, D_005F6230);
        }
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && !(rand() & 0x1F)) {
            n = func_00345780(func_003A4970);
            if (n != NULL) {
                ns = &n->sub;
                ns->unk0 = s->unk0;
                ns->unk4 = s->unk4;
                ns->unk8 = s->unk8;
                ns->unk70[0] = 1.5707964f;
                ns->unk70[1] = 0;
                if (e->flags & 0x20) {
                    n->flags |= 0x20;
                }
            }
        }
        break;
    }
}

INCLUDE_RODATA("asm/nonmatchings/effects", D_005198A0);
void func_003A54D8(s32 side) {
    Effect *e;
    FighterFx *o;
    s32 k;
    s32 i;

    if (gFighters[side].unk2A == 0x14) {
        for (i = 0; i < 10; i++) {
            e = func_00345780(func_003A50B8);
            if (e != NULL) {
                if (side != 0) {
                    e->flags |= 0x22;
                } else {
                    e->flags |= 2;
                }
                e->sub.unk18 = D_005198A0[i];
            }
        }
        e = func_00345780(func_003A4C28);
        if (e != NULL) {
            k = side == 0;
            o = FX_FIGHTER(&gFighters[k]);
            e->sub.unk0 = o->unk590[0];
            e->sub.unk8 = o->unk590[2];
            if (side != 0) {
                e->flags |= 0x20;
            }
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/effects", D_005198C8);
void func_003A55E0(Effect *e) {
    FxSparkSub *x = (FxSparkSub *)&e->sub;
    f32 r;

    if ((e->flags & 0x10) && func_003A3588((e->flags >> 5) & 1, 0x19) != 0) {
        e->flags &= 0xFFFFFFEF;
    }
    x->unkE0[0] = x->b.unk0;
    x->unkE0[1] = x->b.unk4;
    x->unkE0[2] = x->b.unk8;
    if (e->unk18 == 0) {
        x->b.unk1A = 2;
        x->b.unk40 = 0;
        x->b.unk58 = 0.0f;
        x->b.unk96 = 0;
        x->b.unk51 = 0;
        x->b.unk54 = 0.35f;
        x->b.unk90 = 0x400;
        x->b.unk50 = 0x80;
        x->b.unk52 = 0x80;
        r = 0.09817477f;
        x->b.unk70[0] += (2.0f * Fx_RandFloat() - 1.0f) * r;
        x->b.unk70[1] += (2.0f * Fx_RandFloat() - 1.0f) * r;
        func_003A3518(&x->b, 48.0f);
        x->b.unk16 = 0;
        x->b.unk14 = 21;
        e->unk18++;
    } else {
        x->b.unk58 = (f32)x->b.unk16 / ((f32)x->b.unk14 * 1.3f);
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            x->b.unk0 += x->b.unk80[0];
            x->b.unk4 += x->b.unk80[1];
            x->b.unk8 += x->b.unk80[2];
            if (++x->b.unk16 >= x->b.unk14) {
                func_003465C8(e);
                return;
            }
        }
        if ((e->flags & 0x10) && D_005198C8[x->b.unk16] == 0) {
            e->flags &= 0xFFFFFFEF;
        }
    }
}

s32 func_003A5810(Fighter *f) {
    FVec4 v[2];
    f32 dir[4];
    s32 i;
    Effect *e;
    EffectSub *s;
    s32 fl;

    for (i = 0; i < 2; i++) {
        sceVu0ApplyMatrix(v[i], FX_FIGHTER(f)->unkDB0[13].unk0, D_00437C30[i]);
    }
    e = func_00346030(f->unk1E, func_003A55E0);
    if (e != NULL) {
        fl = e->flags;
        e->flags = fl | 0x12;
        s = &e->sub;
        if (f->unk1E != 0) {
            e->flags = fl | 0x32;
        }
        s->unk0 = v[0][0];
        s->unk4 = v[0][1];
        s->unk8 = v[0][2];
        dir[0] = v[1][0] - v[0][0];
        dir[1] = v[1][1] - v[0][1];
        dir[2] = v[1][2] - v[0][2];
        func_003A3438(dir, s->unk70);
    }
}

void func_003A5910(Effect *e) {
    EffectSub *s = &e->sub;
    s32 fl = e->flags;
    Fighter *f;
    f32 *p;
    f32 a[4];
    f32 b[4];

    if (!(fl & 0x20)) { f = &gFighters[0]; } else { f = &D_00884EE0; }
    switch (e->unk18) {
    case 0:
        if (e->flags & 0x20) { s->unk1A = 0x18; } else { s->unk1A = 0x15; }
        s->unk90 = 0x400;
        s->unk40 = 1;
        s->unk44 = 160.0f;
        s->unk54 = 0.4f;
        s->unk4C = 100;
        s->unk1E = 0;
        s->unk96 = 0;
        s->unk58 = 0;
        s->unk16 = 0;
        if (f->unk2A == 0x21 && f->unk127 != 0) {
            e->unk18 = 3;
        } else {
            e->unk18 = 1;
            e->flags &= 0xFFFFFFF7;
        }
        break;
    case 1:
        if (f->unk2A == 0x21 && f->unk127 != 0) {
            e->unk18 = e->unk1C = 2;
        }
        break;
    case 2:
        if (--e->unk1C <= 0) {
            e->unk18 = 3;
        }
        break;
    case 3:
        e->flags = fl | 8;
        p = func_0034CB88(f->unk1E, 1);
        a[0] = p[0];
        a[1] = p[1];
        a[2] = p[2];
        p = func_0034CB88(f->unk1E, 2);
        b[0] = p[0];
        b[1] = p[1];
        b[2] = p[2];
        s->unk0 = (a[0] + b[0]) * 0.5f;
        s->unk4 = (a[1] + b[1]) * 0.5f;
        s->unk8 = (a[2] + b[2]) * 0.5f;
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            s->unk16++;
            s->unk1E = s->unk16 >> 1;
            s->unk1E %= 24;
        }
        if (f->unk2A != 0x21 || f->unk127 == 0) {
            e->unk18 = 1;
            e->flags &= 0xFFFFFFF7;
        }
        break;
    }
}

void func_003A5B78(Effect *e) {
    EffectSub *s = &e->sub;
    s32 fl = e->flags;
    Fighter *f;
    f32 *p;

    if (!(fl & 0x20)) {
        f = &gFighters[0];
    } else {
        f = &D_00884EE0;
    }
    switch (e->unk18) {
    case 0:
        if (e->flags & 0x20) {
            s->unk1A = 0x17;
        } else {
            s->unk1A = 0x14;
        }
        s->unk1E = 0;
        s->unk96 = 0;
        s->unk58 = 0;
        s->unk90 = 0x400;
        s->unk40 = 1;
        s->unk44 = 128.0f;
        s->unk54 = 0.5f;
        s->unk4C = 100;
        s->unk16 = rand() % 24;
        if (f->unk2A == 0x21 && f->unk127 != 0) {
            e->unk18 = 3;
        } else {
            e->unk18 = 1;
            e->flags &= 0xFFFFFFF7;
        }
        break;
    case 1:
        if (f->unk2A == 0x21 && f->unk127 != 0) {
            e->unk18 = e->unk1C = 2;
        }
        break;
    case 2:
        if (--e->unk1C <= 0) {
            e->unk18 = 3;
        }
        break;
    case 3:
        e->flags = fl | 8;
        p = func_0034CB88(f->unk1E, e->unk1A + 1);
        s->unk0 = p[0];
        s->unk4 = p[1];
        s->unk8 = p[2];
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            s->unk1E = ++s->unk16;
            s->unk1E %= 24;
        }
        if (f->unk2A != 0x21 || f->unk127 == 0) {
            e->unk18 = 1;
            e->flags &= 0xFFFFFFF7;
        }
        break;
    }
}

void func_003A5D98(Effect *e) {
    EffectSub *s = &e->sub;
    s32 fl = e->flags;
    Fighter *f;
    f32 *p;

    if (!(fl & 0x20)) { f = &gFighters[0]; } else { f = &D_00884EE0; }
    switch (e->unk18) {
    case 0:
        if (e->flags & 0x20) { s->unk1A = 0x19; } else { s->unk1A = 0x16; }
        s->unk90 = 0x400;
        s->unk40 = 1;
        s->unk44 = 128.0f;
        s->unk54 = 1.4f;
        s->unk4C = 200;
        s->unk1E = 0;
        s->unk96 = 0;
        s->unk58 = 0;
        s->unk16 = 0;
        if (f->unk2A == 0x21 && f->unk127 != 0) {
            e->unk18 = 3;
        } else {
            e->unk18 = 1;
            e->flags &= 0xFFFFFFF7;
        }
        break;
    case 1:
        if (f->unk2A == 0x21 && f->unk127 != 0) {
            e->unk18 = e->unk1C = 2;
        }
        break;
    case 2:
        if (--e->unk1C <= 0) {
            e->unk18 = 3;
        }
        break;
    case 3:
        e->flags = fl | 8;
        p = func_0034CB88(f->unk1E, 0);
        s->unk0 = p[0];
        s->unk4 = p[1];
        s->unk8 = p[2];
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            s->unk16++;
            s->unk1E = s->unk16 >> 1;
            s->unk1E %= 24;
        }
        if (f->unk2A != 0x21 || f->unk127 == 0) {
            e->unk18 = 1;
            e->flags &= 0xFFFFFFF7;
        }
        break;
    }
}

s32 func_003A5FA0(void) {
    s32 kind;
    s32 i;
    s32 j;
    Effect *e;

    if (func_003ABCD0() != 0) {
        return 0;
    }
    if (D_0043B7F8[0] == 8) {
        return 0;
    }
    kind = 0;
    if (D_00440940[0][0].unk0 == 0x21 || D_00440940[0][1].unk0 == 0x21) {
        kind = 1;
    }
    if (D_00440940[1][0].unk0 == 0x21 || D_00440940[1][1].unk0 == 0x21) {
        kind |= 2;
    }
    if (kind == 0) {
        return 0;
    }
    for (i = 0; i < 2; i++) {
        e = func_00345780(func_003A5910);
        if (e != NULL && i != 0) {
            e->flags |= 0x20;
        }
        for (j = 0; j < 2; j++) {
            e = func_00345780(func_003A5B78);
            if (e != NULL) {
                e->unk1A = j;
                if (i != 0) {
                    e->flags |= 0x20;
                }
            }
        }
        e = func_00345780(func_003A5D98);
        if (e != NULL && i != 0) {
            e->flags |= 0x20;
        }
    }
    return kind;
}

void func_003A6110(s16 side, s16 kind, FxHitPos *p) {
    f32 pos[3];
    f32 dir[3];
    FxRgba col = D_005F6238[0];
    f32 yaw;
    f32 pitch;
    f32 sn;
    f32 ax;
    EffectFunc cb;
    s16 type;
    f32 scale;
    f32 size;
    s16 start;
    s16 n;
    s16 i;
    Effect *e;
    EffectSub *s;

    pos[0] = p->unk0;
    pos[1] = -(f32)p->unk4;
    pos[2] = -(f32)p->unk8;
    dir[0] = p->unk10;
    dir[1] = -(f32)p->unk12;
    dir[2] = -(f32)p->unk14;
    yaw = atan2f(dir[0], dir[2]);
    ax = fabsf(dir[0]);
    sn = sinf(yaw);
    if (sn == 0.0f) {
        sn = 1e-6f;
    }
    pitch = atan2f(dir[1], fabsf(ax / sn));
    switch (kind) {
    case 0:
        cb = func_003A35C8;
        type = 4;
        scale = 0.4f;
        size = 128.0f;
        start = 0;
        n = 10;
        break;
    case 11:
        cb = func_003A35C8;
        type = 4;
        scale = 0.4f;
        size = 128.0f;
        start = 10;
        n = 10;
        break;
    case 1:
        cb = func_003A3928;
        type = 4;
        scale = 0.4f;
        size = 128.0f;
        start = 0;
        n = 18;
        break;
    case 12:
        cb = func_003A3928;
        type = 4;
        scale = 0.4f;
        size = 128.0f;
        start = 10;
        n = 18;
        break;
    case 10:
        func_003A54D8(side);
        return;
    default:
        return;
    }
    for (i = 0; i < n; i++) {
        e = func_00345780(cb);
        s = &e->sub;
        if (e != NULL) {
            e->flags |= 1;
            s->unk1A = type;
            s->unk1E = start;
            s->unk54 = scale;
            s->unk44 = size;
            *(FxRgba *)&s->unk50 = col;
            s->unk0 = pos[0];
            s->unk4 = pos[1];
            s->unk8 = pos[2];
            s->unk70[0] = pitch;
            s->unk70[1] = yaw;
        }
    }
}

/* The jump table of func_003A6110 ends at 0x519914; the table read by func_003A6A90 is 8-aligned. */
RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/effects", D_00519918);

s32 func_003A6390(Fighter *f, s32 n) {
    struct FighterMove *m;

    if (n != 0) {
        m = f->unkC0;
        switch (m->unk28) {
        case 0x18:
            if (f->unk2A == 0x14 && f->unkC4 >= m->unk2C && m->unk2D >= f->unkC4) {
                return func_003A4EE8(f);
            }
            break;
        case 0x19:
            if (f->unkC4 >= m->unk2C && m->unk2D >= f->unkC4) {
                return func_003A5810(f);
            }
            break;
        }
    }
}

void func_003A6440(Fighter *f) {
    if (f->unk2A == 0x14) {
        func_003A4EE8(f);
    }
}

