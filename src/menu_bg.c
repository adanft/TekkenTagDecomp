/* 0x3E1C68-0x3E55C8 (module menu_bg, docs/game_overview.md section 4; docs/systems/modes.md). */
#include "common.h"

/* The menu text table: NUL-separated strings, "\\" escapes, "\\n" new line. */
extern u8 *D_0046AF40[];
/* Proportional font widths from ' '. */
extern s8 D_004572E0[];

u8 *func_003E4FA0(u8 *p);
s32 func_0035EA80(void);
s32 func_00361B58(void);
void func_003DFA00(const u8 *fmt, ...);
s32 func_003E06A0(u8 *s);
extern const u8 D_005233E0[];

/* Menu window quad (options.c's OptQuad): alpha limit, then four vertices (XYZ2 word, RGBAQ word). */
typedef struct {
    u16 x;
    u16 y;
    u32 z;
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    u32 q;
} MbgVtx;
typedef struct {
    u16 alpha;
    u16 pad2[3];
    MbgVtx v[4];
} __attribute__((aligned(8))) MbgQuad;
extern MbgQuad D_0043FD10[];
void func_003BB680(MbgQuad *q, s32 z, s32 unused);
u64 *func_00399520(void);
void func_00399650(u64 *p);
void func_003E0460(s32 x, s32 y, s32 w, s32 h, s32 c, s32 color);
extern s8 *D_01FC4A30[];

typedef f32 MbgMat[4][4] __attribute__((aligned(16)));
typedef struct {
    f32 x, y, z, w;
} __attribute__((aligned(8))) MbgVec;
/* Menu background state (D_01FC4A40). */
typedef struct {
    s16 unk0;
    u16 unk2;   /* frame counter */
    s16 unk4;   /* background kind */
    s16 unk6;   /* window height */
    u8 pad8[0x2E];
    s16 unk36;  /* timer */
    u8 pad38[8];
    f32 unk40;
    f32 unk44;
    f32 unk48;
} MbgState;
extern MbgState D_01FC4A40;
extern MbgMat D_004334C0;
extern u32 D_0046B320[];
extern const MbgVec D_005233B0;
extern const char D_005233C0[];
extern const char D_00522FF0[];
s32 func_003DB808(s32 i, f32 *out);
s32 func_003DB8A8(s32 i);
void sceVu0RotTransPers(s32 *out, MbgMat m, f32 *v, s32 mode);
void Gfx_DrawRect(s32 x0, s32 y0, s32 x1, s32 y1, s32 color);
s32 Text_Printf(const char *fmt, ...);
typedef f32 MbgVec4[4] __attribute__((aligned(16)));
void sceVu0CopyMatrix(MbgMat m0, MbgMat m1);
void Mtx_Unit(MbgMat m);
f32 func_0034A570(f32 a);
void sceVu0RotMatrixY(MbgMat m0, MbgMat m1, f32 ry);
void sceVu0MulMatrix(MbgMat m0, MbgMat m1, MbgMat m2);
void func_00349D70(MbgMat m, void *in);
void sceVu0RotTransPersN(s32 (*out)[4], MbgMat m, MbgVec4 *v, s32 n, s32 mode);
extern MbgMat D_00433440;
extern MbgVec4 D_0046B3F0[8];
extern s16 D_0046B470[12][2];
void Gfx_DrawLine(s32 *p0, s32 *p1, u32 color, s32 mode);
void func_003E1700(s32 (*scr)[4], u32 prim, s32 n);
extern MbgVec4 D_0046B330[8];
void srand(u32 seed);
f32 sinf(f32 x);
f32 cosf(f32 x);
f32 fabsf(f32 x);

/* A bare sqrt.s with the two nops the compiler puts in front of it: this object is built without -ffast-math, where
 * sqrtf has an errno fallback (as effects_draw.c's SqrtS) [I]. */
static inline f32 SqrtS(f32 x) {
    f32 r;
    EE_SQRT_S(r, x);
    return r;
}

/* A 16-bit angle (1/65536 turn) in radians. */
#define MBG_RAD(a) ((f32)((s32)(a) > 0x7FFF ? (a) - 0x10000 : (a)) * 9.58738019e-05f)

/* The objects of the Tekken Bowl aim line [I]: position at +0 / +8, active flag at +0x170. */
typedef struct {
    f32 x;
    f32 y;
    f32 z;
    u8 padC[0x164];
    s32 active;
    u8 pad174[0xC];
} MbgObj;
typedef struct {
    u8 pad0[0x1A];
    u16 angle;
} MbgAim;
void func_003E4860(MbgAim *aim, MbgObj *o, f32 *pos);
s32 func_003F1F08(void);
void func_003DEB80(s32 a);
void func_003E1580(s32 x0, s32 y0, s32 x1, s32 y1, s32 z, u32 color, s32 prim);
void func_003E1C68(MbgObj *o);
void func_003E3178(s32 sel);
void func_003E38F0(void);
void Bowl_DrawCube(void);
void Bowl_DrawFloorGrid(void);
void func_003E45E0(void);
void Bowl_DrawBarcode(s32 x, s32 y);
void Bowl_DrawHud(MbgAim *f, MbgObj *o, f32 *pos, s16 pad, s16 arg4);
extern u32 D_0046B310[];
extern const char D_005F6978[8];
extern char *D_0046B3B0[];
void func_003E2A08(s32 x, s32 y, s32 w, s32 h);
void func_003DBAA8(u32 x0, u32 x1, u32 y0, u32 y1);
void func_003E28A8(f32 x, f32 y, f32 w, f32 h, f32 ang);
extern const char D_005231A8[];
extern const char D_005231C8[];
extern const char D_005231E8[];
extern const char D_00523208[];
extern const char D_00523228[];
extern const char D_00523248[];
extern const char D_00523268[];
extern const char D_00523278[];
extern const char D_00523298[];
extern const char D_005232B8[];
extern const char D_005232D8[];
extern const char D_005232F8[];
extern const char D_00523318[];
extern const char D_00523330[];
extern const char D_00523350[];
extern const char D_00523370[];
extern const char D_00523388[];
int rand(void);
extern const char D_00523008[];
extern const char D_00523028[];
extern const char D_00523048[];
extern const char D_00523068[];
extern const char D_00523088[];
extern const char D_005230B0[];

INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00522FC0);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00522FF0);
LIT4_WORD(D_005F5804, 0x3E99999A);
LIT4_WORD(D_005F5808, 0x40490FDC);
LIT4_WORD(D_005F580C, 0x3F333333);
LIT4_WORD(D_005F5810, 0x3F4CCCCD);
LIT4_WORD(D_005F5814, 0x3ECCCCCD);
LIT4_WORD(D_005F5818, 0xBE060A93);
LIT4_WORD(D_005F581C, 0x3E860A93);
LIT4_WORD(D_005F5820, 0x3D4CCCCD);
LIT4_WORD(D_005F5824, 0x3E4CCCCD);
LIT4_WORD(D_005F5828, 0x3F32B8C4);
LIT4_WORD(D_005F582C, 0xBD4CCCCD);
LIT4_WORD(D_005F5830, 0x3E4CCCCD);
LIT4_WORD(D_005F5834, 0x3FB33333);
LIT4_WORD(D_005F5838, 0x3DCCCCCD);
LIT4_WORD(D_005F583C, 0x3FA66666);
LIT4_WORD(D_005F5840, 0x3FC90FDC);
#if 0
/* NON_MATCHING: rough m2c-based draft (target scope rings / grid of the bowl HUD), not tuned */
#define M32(p, o) (*(s32 *)((u8 *)(p) + (o)))
void Gfx_DrawTriangle(f32 *, s32);
void sceVu0AddVector(f32 *v0, f32 *v1, f32 *v2);
void sceVu0ScaleVector(f32 *v0, f32 *v1, f32 s);
extern u8 D_00522FC0[];
void func_003E1C68(MbgObj *arg0) {
    u8 spbuf[0x400];
    f32 box[12] __attribute__((aligned(16)));
    s32 scr[12] __attribute__((aligned(16)));
    s32 k;
    s32 sp1B4;
    s32 sp1C0;
    s32 sp1C4;
    s32 sp1D0;
    s32 sp1D4;
    f32 sp10;
    f32 sp18;
    f32 sp20;
    f32 sp28;
    f32 sp30;
    f32 sp34;
    s32 sp38;
    s32 sp3C;
    f32 sp40;
    f32 sp44;
    s32 sp48;
    f32 sp50;
    f32 sp54;
    s32 sp58;
    f32 sp60;
    f32 sp64;
    s32 sp1B0;
    f32 temp_a0;
    f32 temp_f0;
    f32 temp_f0_2;
    f32 temp_f0_3;
    f32 temp_f0_4;
    f32 temp_f0_5;
    f32 temp_f0_6;
    f32 temp_f0_7;
    f32 temp_f0_8;
    f32 temp_f1;
    f32 temp_f1_2;
    f32 temp_f1_3;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f20_3;
    f32 temp_f20_4;
    f32 temp_f20_5;
    f32 temp_f20_6;
    f32 temp_f21;
    f32 temp_f22;
    f32 temp_f22_2;
    f32 temp_f22_3;
    f32 temp_f23;
    f32 temp_f23_2;
    f32 temp_f25;
    f32 temp_f26;
    f32 temp_f26_2;
    f32 temp_f26_3;
    f32 temp_f26_4;
    f32 temp_f26_5;
    f32 temp_f26_6;
    f32 temp_f26_7;
    f32 temp_f2;
    f32 temp_f4;
    f32 temp_f4_2;
    f32 temp_f6;
    f32 temp_f8;
    f32 var_f0;
    f32 var_f0_2;
    f32 var_f1;
    f32 var_f1_2;
    f32 var_f1_3;
    f32 var_f27;
    f32 var_f27_2;
    f32 var_f27_3;
    f32 var_f2;
    f32 var_f2_2;
    f32 var_f2_3;
    f32 var_f3;
    f32 var_f3_2;
    f32 var_f3_3;
    f32 var_f4;
    f32 var_f5;
    f32 var_f5_2;
    s16 temp_s0_3;
    s16 temp_s0_5;
    s32 temp_a0_2;
    s32 temp_s0;
    s32 temp_s0_2;
    s32 temp_s2;
    s32 temp_s3;
    s32 temp_s4;
    s32 temp_s4_2;
    s32 temp_s5;
    s32 var_hi;
    s32 var_s0;
    s32 var_s2;
    s32 var_s2_2;
    s32 var_s2_3;
    s32 var_s2_4;
    s32 var_s5;
    s32 var_s6;
    s32 var_s6_2;
    s32 var_s6_3;
    s32 var_s6_4;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v1;
    u16 temp_a2;
    u16 temp_a2_2;
    u16 temp_a2_3;
    u16 temp_a3;
    u32 temp_s0_4;
    u32 temp_s1;
    u8 *temp_v0;
    u8 *temp_v0_2;
    u8 *temp_v1;
    u8 *temp_v1_2;

    var_s6 = 0;
    /* The pins' bounding box on the floor (sp+0: min, sp+0x10: max, sp+0x20: their middle; D_00522FC0 the start),
     * projected (sp+0x1B0) to size the target (T41 b0: the m2c draft kept the three vectors as scalars and wrote the
     * vector results over its other locals). */
    for (k = 0; k < 12; k++) {
        box[k] = ((f32 *)D_00522FC0)[k];
    }
    var_f1 = box[0];
    var_f2 = box[2];
    var_f5 = box[4];
    var_f3 = box[6];
    var_v0 = 0 * 0x180;
    do {
        temp_v1 = var_v0 + (u8 *)arg0;
        if (M32(temp_v1, 0x170) == 0) {
            var_s6 += 1;
        } else {
            temp_f0 = M32(temp_v1, 0x0);
            if (temp_f0 < var_f1) {
                box[0] = temp_f0;
                var_f1 = temp_f0;
            }
            temp_f0_2 = -M32(temp_v1, 0x8);
            if (temp_f0_2 < var_f2) {
                box[2] = temp_f0_2;
                var_f2 = temp_f0_2;
            }
            if (var_f5 < temp_f0) {
                box[4] = temp_f0;
                var_f5 = temp_f0;
            }
            var_s6 += 1;
            if (var_f3 < temp_f0_2) {
                box[6] = temp_f0_2;
                var_f3 = temp_f0_2;
            }
        }
        var_v0 = var_s6 * 0x180;
    } while (var_s6 < 0xA);
    box[6] = var_f3 + 200.0f;
    box[0] = var_f1 - 200.0f;
    box[2] = var_f2 - 200.0f;
    box[4] = var_f5 + 200.0f;
    sceVu0AddVector(&box[8], box, &box[4]);
    sceVu0ScaleVector(&box[8], &box[8], 0.5f);
    sceVu0RotTransPersN((void *)scr, D_004334C0, (void *)box, 3, 0);
    sp1B0 = scr[0];
    sp1B4 = scr[1];
    sp1C0 = scr[4];
    sp1C4 = scr[5];
    sp1D0 = scr[8];
    sp1D4 = scr[9];
    var_v0_2 = sp1B0 - sp1C0;
    if (var_v0_2 < 0) {
        var_v0_2 = -var_v0_2;
    }
    var_v1 = sp1B4 - sp1C4;
    if (var_v1 < 0) {
        var_v1 = -var_v1;
    }
    temp_f1 = (f32) var_v0_2 * 0.5f;
    var_f0 = (f32) var_v1 * 0.5f;
    M32(((u8 *)&D_01FC4A40), 0x10) = (s32) ((sp1D0 >> 4) - 0x6C0);
    M32(((u8 *)&D_01FC4A40), 0x14) = (s32) ((sp1D4 >> 4) - 0x720);
    var_f2_2 = var_f0;
    sp18 = var_f0;
    if (var_f0 < temp_f1) {
        var_f2_2 = temp_f1;
    }
    M32(((u8 *)&D_01FC4A40), 0x20) = var_f2_2;
    temp_a3 = (u16) M32(((u8 *)&D_01FC4A40), 0x4);
    if (M32(((u8 *)&D_01FC4A40), 0x4) != 2) {
        if (M32(((u8 *)&D_01FC4A40), 0x4) >= 3) {
            if (M32(((u8 *)&D_01FC4A40), 0x4) != 3) {
                return;
            }
            temp_a2 = M32(((u8 *)&D_01FC4A40), 0x2);
            var_f5_2 = var_f2_2;
            var_s6_2 = 3;
            temp_s0 = (s32) ((s16) temp_a2 + ((u32) (temp_a2 << 0x10) >> 0x1F)) >> 1;
            temp_s0_2 = (temp_s0 >= 0xA) ? 9 : temp_s0;
            do {
                var_s2 = 0;
                var_f27 = (f32) ((s16) temp_a2 - var_s6_2) * 0.0500000007f;
                if (temp_s0_2 > 0) {
                    var_s5 = var_s6_2 - 1;
loop_62:
                    temp_f23 = 0.200000003f;
                    temp_f21 = (f32) var_s6_2 * 0.5f;
                    temp_f26 = (((f32) (0x14 - (s16) temp_a2) + temp_f21) * temp_f23) + 1.0f;
                    var_f1_2 = 1.0f;
                    if (temp_f26 >= 1.0f) {
                        var_f1_2 = temp_f26;
                    }
                    temp_f0_3 = func_0034A570(var_f27);
                    temp_f20 = sinf(temp_f0_3);
                    temp_f0_4 = (cosf(temp_f0_3) * (var_f1_2 * var_f5_2) * 480.0f) / 480.0f;
                    temp_f26_2 = (((f32) ((var_s2 * 2) - ((s16) M32(((u8 *)&D_01FC4A40), 0x2) - 0x14)) + temp_f21) * temp_f23) + 1.0f;
                    temp_a0 = temp_f0_4;
                    sp38 = 0xFFFFF;
                    var_f1_3 = 1.0f;
                    sp3C = 0;
                    sp30 = temp_a0;
                    sp34 = temp_f0_4;
                    if (temp_f26_2 >= 1.0f) {
                        var_f1_3 = temp_f26_2;
                    }
                    var_f0_2 = var_f1_3 * M32(((u8 *)&D_01FC4A40), 0x20) * 80.0f;
                    temp_f26_3 = var_f0_2 / 756.0f;
                    if (temp_f26_3 < 80.0f) {
                        var_f0_2 = (80.0f - temp_f26_3) * 0.5f;
                    }
                    var_s2 += 1;
                    func_003E0460(temp_a0, temp_f0_4, var_f0_2, var_f0_2, 0, 0);
                    var_f27 = temp_f0_3 + 0.6981318f;
                    if (var_s2 < temp_s0_2) {
                        var_f5_2 = M32(((u8 *)&D_01FC4A40), 0x20);
                        goto loop_62;
                    }
                    var_f5_2 = M32(((u8 *)&D_01FC4A40), 0x20);
                } else {
                    var_s5 = var_s6_2 - 1;
                }
                var_s6_2 = var_s5;
            } while (var_s5 >= 0);
            var_f27_2 = (f32) (0x14 - (s16) temp_a2) * (-0.0500000007f);
            if (var_f27_2 > 0.0f) {
                var_f27_2 = 0.0f;
            }
            var_s2_2 = 0;
            sp38 = 0xFFFFF;
            sp58 = 0xFFFFF;
            sp48 = 0xFFFFF;
loop_77:
            temp_f26_4 = (((f32) (0x14 - (s16) temp_a2) + ((f32) var_s5 * 0.5f)) * 0.200000003f) + 1.0f;
            var_f4 = 1.0f;
            if (temp_f26_4 >= 1.0f) {
                var_f4 = temp_f26_4;
            }
            var_s2_2 += 1;
            temp_f22 = 0.100000001f;
            temp_f26_5 = var_f5_2 * var_f4 * 1.39999998f;
            temp_f25 = 1.29999995f;
            temp_f23_2 = var_f27_2 + temp_f22;
            sp30 = cosf(var_f27_2) * temp_f26_5;
            temp_f22_2 = var_f27_2 - temp_f22;
            sp34 = (sinf(var_f27_2) * temp_f26_5 * 480.0f) / 480.0f;
            sp40 = cosf(temp_f23_2) * temp_f26_5 * temp_f25;
            sp44 = (sinf(temp_f23_2) * temp_f26_5 * temp_f25 * 480.0f) / 480.0f;
            sp50 = cosf(temp_f22_2) * temp_f26_5 * temp_f25;
            sp54 = (sinf(temp_f22_2) * temp_f26_5 * temp_f25 * 480.0f) / 480.0f;
            Gfx_DrawTriangle(&sp30, *((M32(((u8 *)&D_01FC4A40), 0x4) * 4) + ((u8 *)&D_0046B320)) | 0x40);
            var_f0 = 1.57079649f;
            var_f27_2 += var_f0;
            if (var_s2_2 < 4) {
                var_f5_2 = M32(((u8 *)&D_01FC4A40), 0x20);
                goto loop_77;
            }
            goto block_80;
        }
        if (M32(((u8 *)&D_01FC4A40), 0x4) >= 0) {
            temp_s0_3 = ((s16) M32(((u8 *)&D_01FC4A40), 0x2) >= 0x25) ? 1 : (s16) M32(((u8 *)&D_01FC4A40), 0x2);
            var_s6_3 = ((temp_s0_3 >= 0x11) ? 0x10 : temp_s0_3) - 1;
            temp_a2_2 = M32(((u8 *)&D_01FC4A40), 0x2);
            if (var_s6_3 >= 0) {
                do {
                    temp_f4 = (f32) (0x14 - (s16) temp_a2_2) + ((f32) var_s6_3 * 0.5f);
                    temp_f20_2 = (temp_f4 * 0.300000012f) + 1.0f;
                    var_f3_2 = 1.0f;
                    if (temp_f20_2 >= 1.0f) {
                        var_f3_2 = temp_f20_2;
                    }
                    var_f2_3 = 0.0f;
                    temp_f26_6 = M32(((u8 *)&D_01FC4A40), 0x20) * var_f3_2;
                    if (temp_f4 >= 0.0f) {
                        var_f2_3 = temp_f4;
                    }
                    temp_f0_5 = func_0034A570((var_f2_3 * 3.14159298f) / 40.0f);
                    temp_f20_3 = sinf(temp_f0_5);
                    temp_f0_6 = cosf(temp_f0_5);
                    temp_f2 = -temp_f26_6;
                    temp_f4_2 = temp_f20_3 * temp_f2;
                    temp_f0_7 = temp_f0_6 * temp_f2;
                    temp_s4 = *((M32(((u8 *)&D_01FC4A40), 0x4) * 4) + ((u8 *)&D_0046B320)) | (0x80 - (var_s6_3 * 7));
                    var_s6_3 -= 1;
                    temp_f8 = ((temp_f4_2 + (temp_f0_6 * temp_f26_6)) * 480.0f) / 480.0f;
                    temp_f6 = (f32) sp1D0;
                    temp_f1_2 = temp_f6 + (temp_f0_7 - temp_f4_2);
                    temp_f0_8 = temp_f6 + (temp_f0_7 - (temp_f20_3 * temp_f26_6));
                    sp30 = temp_f8;
                    sp50 = temp_f1_2;
                    sp34 = temp_f1_2;
                    sp40 = temp_f1_2;
                    sp44 = temp_f1_2;
                    sp54 = temp_f0_8;
                    sp60 = temp_f0_8;
                    sp64 = temp_f0_8;
                    Gfx_DrawRect(temp_f8, temp_f1_2, temp_f1_2, temp_f1_2, 0);
                    Gfx_DrawRect(sp40, sp44, sp60, sp64, 0);
                    Gfx_DrawRect(sp60, sp64, sp50, sp54, 0);
                    Gfx_DrawRect(sp50, sp54, sp30, sp34, 0);
                } while (var_s6_3 >= 0);
            }
            if ((s16) temp_a2_2 >= 0x15) {
                temp_f20_4 = M32(((u8 *)&D_01FC4A40), 0x20);
                temp_s1 = sp1D0 + (s32)(temp_f20_4 * 0.699999988f);
                temp_s3 = temp_s1 + 0x180;
                temp_s0_4 = sp1D4 - (s32)(temp_f20_4 * 0.800000012f);
                temp_s2 = temp_s0_4 - 0x100;
                Gfx_DrawRect((f32) temp_s1, (f32) temp_s0_4, (f32) temp_s3, (f32) temp_s2, 0);
                Gfx_DrawRect((f32) temp_s3, (f32) temp_s2, (f32) (temp_s1 + 0x4C0), (f32) temp_s2, 0);
                Text_Printf(D_00522FF0, ((u32)temp_s1 >> 4) - 0x6A8, ((u32)temp_s0_4 >> 4) - 0x728, 0xB, 6, 6);
            }
        }
        goto block_80;
    }
    temp_s0_5 = ((s16) M32(((u8 *)&D_01FC4A40), 0x2) >= 0x21) ? 1 : (s16) M32(((u8 *)&D_01FC4A40), 0x2);
    var_s6_4 = ((temp_s0_5 >= 0xD) ? 0xC : temp_s0_5) - 1;
    temp_a2_3 = M32(((u8 *)&D_01FC4A40), 0x2);
    if (var_s6_4 >= 0) {
loop_39:
        temp_f20_5 = (((f32) (0x14 - (s16) temp_a2_3) + ((f32) var_s6_4 * 0.5f)) * 0.400000006f) + 1.0f;
        var_f3_3 = 1.0f;
        if (temp_f20_5 >= 1.0f) {
            var_f3_3 = temp_f20_5;
        }
        temp_f26_7 = M32(((u8 *)&D_01FC4A40), 0x20) * var_f3_3;
        var_f27_3 = (-0.130899712f);
        temp_s4_2 = *(((s32) (temp_a3 << 0x10) >> 0xE) + ((u8 *)&D_0046B320)) | (0x80 - (var_s6_4 * 7));
        temp_f22_3 = 0.261799425f;
        var_s2_3 = 0;
        temp_s5 = var_s6_4 - 1;
        do {
            temp_f20_6 = sinf(var_f27_3);
            temp_v0 = &sp30 + (var_s2_3 * 0x10);
            var_s2_3 += 1;
            M32(temp_v0, 0x0) = (f32) (cosf(var_f27_3) * temp_f26_7);
            temp_f1_3 = (temp_f20_6 * temp_f26_7 * 480.0f) / 480.0f;
            M32(temp_v0, 0xC) = 0;
            M32(temp_v0, 0x8) = 0xFFFFF;
            var_f0 = (f32) sp1D4 + temp_f1_3;
            M32(temp_v0, 0x4) = temp_f1_3;
            var_f27_3 += temp_f22_3;
        } while (var_s2_3 < 0x18);
        var_s2_4 = 1;
        var_hi = 1 % 6;
loop_44:
        if (var_hi == 0) {
            var_s0 = var_s2_4 + 1;
        } else if (var_s6_4 != 0) {
            var_f0 = M32(((u8 *)&D_01FC4A40), 0x20);
            var_s0 = var_s2_4 + 1;
            if (var_f0 < temp_f26_7) {
                temp_v0_2 = &sp30 + (var_s2_4 * 0x10);
                temp_v1_2 = &sp30 + (var_s0 * 0x10);
                Gfx_DrawRect(M32(temp_v0_2, 0x0), M32(temp_v0_2, 0x4), M32(temp_v1_2, 0x0), M32(temp_v1_2, 0x4), 0);
            }
        } else {
            temp_a0_2 = var_s2_4 * 0x10;
            Gfx_DrawLine((s32 *)(spbuf + temp_a0_2), (s32 *)(spbuf + temp_a0_2 + 0x10), temp_s4_2, 1);
            var_s0 = var_s2_4 + 1;
        }
        var_s2_4 = var_s0;
        if (var_s2_4 < 0x17) {
            var_hi = var_s2_4 % 6;
            goto loop_44;
        }
        var_s6_4 = temp_s5;
        if (temp_s5 >= 0) {
            goto loop_39;
        }
    }
block_80:
    return;
}
#undef M32
#else
INCLUDE_ASM("asm/nonmatchings/menu_bg", func_003E1C68);
#endif

void func_003E28A8(f32 x, f32 y, f32 w, f32 h, f32 ang) {
    MbgMat m;
    MbgMat cam;
    s32 scr[8][4];
    f32 fy = (s32)y;
    f32 a;

    D_0046B330[7][0] = w;
    D_0046B330[6][0] = w;
    D_0046B330[3][0] = w;
    D_0046B330[2][0] = w;
    D_0046B330[7][1] = h;
    D_0046B330[5][1] = h;
    Mtx_Unit(m);
    if ((s16)D_01FC4A40.unk2 < 16) {
        a = ang * (16 - (s16)D_01FC4A40.unk2) * 0.03125f;
    } else {
        a = 0.0f;
    }
    sceVu0RotMatrixY(m, m, a);
    m[3][2] = 6000.0f;
    sceVu0CopyMatrix(cam, D_00433440);
    cam[2][0] += x + -320.0f;
    cam[2][1] += fy + -224.0f;
    sceVu0MulMatrix(m, cam, m);
    sceVu0RotTransPersN(scr, m, D_0046B330, 8, 0);
    func_003E1700(scr, 0x20203046, 0x44);
    func_003E1700(&scr[4], 0x1046, 0x44);
}

#if 0
/* NON_MATCHING: 143 lines: rough draft of the glitch grid (srand seed = timer / 384); stack slots and the loop registers not matched */
void func_003E2A08(s32 x, s32 y, s32 w, s32 h) {
    s32 p[5][4];
    s32 i, j, k, r, odd, bx, d;
    u32 yy;

    for (i = 0; i < 5; i++) {
        p[i][2] = 0xFFFE0;
    }
    func_003DBAA8(x + 2, x + w - 2, ((u32)y >> 1) + 10, ((u32)(y + h) >> 1) - 2);
    bx = x - 14;
    yy = y - 14;
    srand((s16)D_01FC4A40.unk36 / 384);
    if (((s16)D_01FC4A40.unk36 / 48) & 1) {
        d = (s16)((s16)D_01FC4A40.unk36 % 8);
        yy -= d * 6;
        if (rand() & 1) {
            bx -= (s16)((s16)D_01FC4A40.unk36 % 8) * 6;
        } else {
            bx += (s16)((s16)D_01FC4A40.unk36 % 8) * 4;
        }
    }
    for (i = 0; i < 12; i++) {
        odd = i & 1;
        for (j = 0; j < 5; j++) {
            r = rand();
            p[1][0] = (bx + i * 16 + 0x6C0) << 4;
            p[1][1] = ((yy + j * 48 + odd * 24 + 0x720) << 4) + 0x100;
            if (r & ((D_01FC4A40.unk2 >> 5) & 0x3F)) {
                Gfx_DrawLine(p[0], p[1], 0xA0A0A080, 0);
                if ((r & (((u16)D_01FC4A40.unk36 >> 6) & 0x3F)) && odd == 0) {
                    p[3][1] = p[0][1];
                    p[3][0] = p[0][0] + 0x40;
                    p[4][0] = p[1][0] + 0x40;
                    p[4][1] = p[1][1];
                    Gfx_DrawLine(p[3], p[4], 0xA0A0A080, 0);
                }
            }
            p[2][0] = p[0][0] + 0x100;
            p[2][1] = p[0][1] - 0x80;
            if (r & (((u16)D_01FC4A40.unk36 >> 5) & 0x3F)) {
                Gfx_DrawLine(p[0], p[2], 0xA0A0A080, 0);
                if ((r & (((u16)D_01FC4A40.unk36 >> 6) & 0x3F)) && odd != 0) {
                    p[4][1] = p[2][1] + (odd << 6);
                    p[3][0] = p[0][0] + (odd << 6);
                    p[3][1] = p[0][1] + (odd << 5);
                    p[4][0] = p[2][0];
                    Gfx_DrawLine(p[3], p[4], 0xA0A0A080, 0);
                }
            }
            p[2][1] = p[1][1] + 0x80;
            p[2][0] = p[1][0] + 0x100;
            if (r & (((u16)D_01FC4A40.unk36 >> 4) & 0x3F)) {
                Gfx_DrawLine(p[1], p[2], 0xA0A0A080, 0);
                if ((r & (((u16)D_01FC4A40.unk36 >> 5) & 0x3F)) && odd != 0) {
                    p[4][1] = p[2][1] - (odd << 6);
                    p[3][0] = p[1][0] + (odd << 6);
                    p[3][1] = p[1][1] - (odd << 5);
                    p[4][0] = p[2][0];
                    Gfx_DrawLine(p[3], p[4], 0xA0A0A080, 0);
                }
            }
        }
    }
    func_003DBAA8(0, 639, 0, 447);
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_bg", func_003E2A08);
#endif

INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523008);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523028);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523048);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523068);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523088);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005230B0);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005230D0);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005230E0);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005230F0);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523100);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523118);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523128);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523138);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523150);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523168);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523180);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523198);
void Bowl_DrawBarcode(s32 x, s32 y) {
    s32 i, j, n, h, bx;

    switch (D_01FC4A40.unk4) {
    case 2:
        Text_Printf(D_00523008, 0xFFFD0, x - 18, y + 8, 2, 5);
        Text_Printf(D_00523028, 0xFFFD0, x + 10, y + 64, 2, 5);
        Text_Printf(D_00523048, 0xFFFD0, x, y + 50, 2, 6);
        break;
    case 3:
        Text_Printf(D_00523068, 0xFFFD0, x - 8, y + 8, 2, 5);
        Text_Printf(D_00523088, 0xFFFD0, x - 20, y + 64, 2, 5);
        Text_Printf(D_005230B0, 0xFFFD0, x, y + 50, 2, 6);
        break;
    }
    srand(D_01FC4A40.unk4);
    for (i = 0; i < 27; i++) {
        n = (rand() >> 1) & 3;
        if (n == 0) {
            n = 1;
        }
        h = 28;
        if (i < 2 || i > 24 || (i >= 12 && i < 15)) {
            h = 38;
        }
        for (j = 0; j < n; j++) {
            bx = (i * 4 + (x + j) + 0x6CA) << 4;
            Gfx_DrawRect(bx, (y + 0x720) << 4, bx, (y + h + 0x720) << 4, 0xFF0F3780);
        }
    }
}

void func_003E3078(s32 x, s32 y, s32 n, s32 val, s32 size, s32 color) {
    s32 d;
    while (n-- > 0) {
        d = (val < 0 ? -val : val) % 10;
        val /= 10;
        func_003E0460((x + 0x6C0) << 4, (y + 0x720) << 4, size, size, D_01FC4A30[0][d], color);
        x += (size << 4) / 100;
    }
}

INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005231A8);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005231C8);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005231E8);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523208);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523228);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523248);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523268);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523278);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523298);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005232B8);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005232D8);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005232F8);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523318);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523330);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523350);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523370);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_00523388);
#if 0
/* NON_MATCHING: 218 lines: rough draft (case layout, argument registers and the telemetry arithmetic not matched yet) */
void func_003E3178(s32 sel) {
    s32 r = rand();
    s16 t = D_01FC4A40.unk2;
    s32 i, k, base, sym, h, c;

    switch (D_01FC4A40.unk4) {
    case 0:
    case 1:
        Text_Printf(D_005F6978, 11);
        Text_Printf(D_005231A8, 0xFFFD0, 228, 360, 1, 6, (r >> 6) + (t & 0xFF) + 0x2400875);
        Text_Printf(D_005231C8, 0xFFFD0, 228, 370, 1, 6, (r >> 6) + (t & 0xFF) + 0x78);
        Text_Printf(D_005231E8, 0xFFFD0, 228, 390, 1, 6, (r >> 8) + (t & 0x7C00) + 0x466A7E0);
        Text_Printf(D_00523208, 0xFFFD0, 228, 400, 1, 6, (r >> 8) + (t & 0x2FF80) + 0x7E902805);
        Text_Printf(D_00523228, 0xFFFD0, 404, 400, 2, 6, (r >> 6) + (u64)(t & 0x2FF80) + 0x8DA0C478);
        Text_Printf(D_00523248, 0xFFFD0, 404, 410, 2, 6, (r >> 11) + ((t & 0x2FF) << 4) + 0x49FD6CD3);
        base = t / 4;
        for (i = 0; i < 10; i++) {
            Text_Printf(D_00523268, 50, i * 10 + 80, 3, 6, D_0046B3B0[((((base + i) * 5 - 1) >> 1) & 7) + 8]);
        }
        if (D_01FC4A40.unk4 == 1) {
            Text_Printf(D_00523278, 0xFFFD0, 228, 380, 1, 6);
            Text_Printf(D_00523298, 0xFFFD0, 228, 420, 1, 6);
        } else {
            Text_Printf(D_005232B8, 0xFFFD0, 228, 380, 1, 6, r + 0x7B);
            Text_Printf(D_005232D8, 0xFFFD0, 228, 420, 1, 6, r >> 12);
        }
        break;
    case 2:
        if (t >= 17) {
            Bowl_DrawBarcode(428, 354);
            func_003E2A08(420, 88, 122, 340);
            Text_Printf(D_005232F8, 30, 130, 6, 6, (r >> 7) + (t & 0xFF) + 0x9AB);
            Text_Printf(D_00523318, 30, 155, 6, 6, (r >> 13) + (t & 0xFF) + 0x78);
            Text_Printf(D_00523330, 30, 180, 6, 6, (r >> 15) + 0x7B);
            Text_Printf(D_00523350, 30, 205, 6, 6, (r >> 11) + (t & 0xBFE) + 0x3A04D);
            Text_Printf(D_00523370, 30, 255, 6, 6, (r >> 8) + (t & 0x2FF80) + 0xEB);
            Text_Printf(D_00523388, 30, 280, 6, 6, (r >> 9) + (t & 0xFF) + 0x1486D3);
            base = t / 4;
            for (i = 0; i < 10; i++) {
                Text_Printf(D_00523268, 412, i * 10 + 250, 6, 6, D_0046B3B0[(((base + i) * 5 + 1) * 2 & 0x1C) / 4]);
            }
            func_003E28A8(20.0f, 90.0f, 1000.0f, 1400.0f, -3.1415935f);
            func_003E28A8(420.0f, 40.0f, 800.0f, 1200.0f, 3.1415935f);
            func_003E28A8(410.0f, 216.0f, 1100.0f, 800.0f, 3.1415935f);
        }
        break;
    case 3:
        sym = D_01FC4A30[3][sel + 0x10];
        k = t & 0x1F;
        base = t / 8;
        c = D_0046B320[3] | 0x80;
        for (i = 0; i < 5; i++) {
            h = ((base + i) * 5 + 1) & 0xE;
            func_003E0460(0x6D40, (i * 20 + 0x784) << 4, 70, 70, D_01FC4A30[3][h], c);
            func_003E0460(0x6E00, (i * 20 + 0x784) << 4, 70, 70, D_01FC4A30[3][h + 1], c);
            func_003E3078(54, i * 20 + 100, 8, (base + i) * 0x2384723, 70, c);
        }
        for (i = 7; i >= 0; i--) {
            h = i * 40 + 200 - k * 8;
            if (h < 0) {
                h = 0;
            }
            func_003E0460(0x74C0, 0x88E0, h, h, sym, D_0046B320[D_01FC4A40.unk4] | (0x80 - i * 16));
        }
        break;
    }
}
#else
LIT4_WORD(D_005F5848, 0xC0490FDC);
LIT4_WORD(D_005F584C, 0x40490FDC);
LIT4_WORD(D_005F5850, 0x44898000);
INCLUDE_ASM("asm/nonmatchings/menu_bg", func_003E3178);
#endif

LIT4_WORD(D_005F5854, 0x3E4CCCCD);
LIT4_WORD(D_005F5858, 0x3ECCCCCD);
LIT4_WORD(D_005F585C, 0x40490FDC);
LIT4_WORD(D_005F5860, 0xBF860A93);
LIT4_WORD(D_005F5864, 0x4519C000);
LIT4_WORD(D_005F5868, 0x3E4CCCCD);
LIT4_WORD(D_005F586C, 0x40490FDC);
LIT4_WORD(D_005F5870, 0x3DB2B8C4);
LIT4_WORD(D_005F5874, 0x3D8F5C29);
LIT4_WORD(D_005F5878, 0x45228000);
LIT4_WORD(D_005F587C, 0x3E060A93);
LIT4_WORD(D_005F5880, 0x3ED70A3D);
LIT4_WORD(D_005F5884, 0xBED70A3D);
LIT4_WORD(D_005F5888, 0x40490FDB);
LIT4_WORD(D_005F588C, 0x3DCCCCCD);
#if 0
/* NON_MATCHING: rough m2c-based draft (scales, dial and wave of the bowl HUD), not tuned */
#define M32(p, o) (*(s32 *)((u8 *)(p) + (o)))
void func_003DFF30(f32 a, u32 x, u32 y, u32 size, u32 tile, u32 col);
extern u8 D_01FC4A50[];
extern u8 D_01FC4A6C[];
void func_003E38F0(void) {
    u8 spbuf[0x400];
    s32 temp_x;
    s32 temp_y;
    s32 sp10;
    s32 sp14;
    s32 sp20;
    s32 sp24;
    s32 sp30;
    s32 sp34;
    f32 *temp_v0;
    f32 *temp_v0_2;
    f32 *temp_v1;
    f32 *var_v0_2;
    f32 temp_f12;
    f32 temp_f1;
    f32 temp_f20;
    f32 temp_f20_2;
    f32 temp_f21;
    f32 temp_f21_2;
    f32 temp_f23;
    f32 temp_f23_2;
    f32 temp_f24;
    f32 temp_f24_2;
    f32 temp_f25;
    f32 temp_f25_2;
    f32 temp_f27;
    f32 temp_f27_2;
    f32 temp_f29;
    f32 temp_f2;
    f32 temp_f2_2;
    f32 var_f0;
    f32 var_f0_2;
    f32 var_f0_3;
    f32 var_f24;
    f32 var_f24_2;
    f32 var_f26;
    s32 temp_a0;
    s32 temp_a0_2;
    s32 temp_a0_3;
    s32 temp_a0_4;
    s32 temp_a0_5;
    s32 temp_a1;
    s32 temp_a1_2;
    s32 temp_a1_3;
    s32 temp_a1_4;
    s32 temp_a2;
    s32 temp_a3;
    s32 temp_a3_2;
    s32 temp_s0;
    s32 temp_s2;
    s32 temp_s2_2;
    s32 temp_s2_3;
    s32 var_a0;
    s32 var_f1;
    s32 var_f1_2;
    s32 var_s1;
    s32 var_s1_2;
    s32 var_s1_3;
    s32 var_s1_4;
    s32 var_s1_5;
    s32 var_s1_6;
    s32 var_s1_7;
    s32 var_s1_8;
    s32 var_s1_9;
    s32 var_v0;
    s8 temp_a3_3;
    u8 *temp_s0_2;
    u8 *var_fp;

    /* Flowgraph is not reducible, falling back to gotos-only mode. */
    var_f26 = 0.200000003f;
    if (M32(((u8 *)&D_01FC4A40), 0x4) == 2) {
        goto block_12;
    }
    if (M32(((u8 *)&D_01FC4A40), 0x4) < 3) {
        goto block_4;
    }
    var_fp = ((u8 *)&D_01FC4A40) + 0x24;
    if (M32(((u8 *)&D_01FC4A40), 0x4) == 3) {
        goto block_13;
    }
    goto block_40;
block_4:
    if (M32(((u8 *)&D_01FC4A40), 0x4) < 0) {
        goto block_39;
    }
    var_s1 = 0;
    temp_s2 = D_0046B320[M32(((u8 *)&D_01FC4A40), 0x4)] | 0x80;
    var_fp = ((u8 *)&D_01FC4A40) + 0x24;
    Gfx_DrawRect(0x6D40, 0x72A0, 0x9180, 0x72A0, 0);
    var_f1 = 0;
loop_6:
    temp_a2 = var_s1 * 0x14;
    temp_a3 = !(var_s1 & 3) ? 0x7380 : 0x7320;
    var_s1 += 1;
    temp_a0 = var_f1 + temp_a2;
    temp_a0_2 = (temp_a0 <= 0x13) ? (temp_a0 + 0x244) : temp_a0;
    temp_a0_3 = (((temp_a0_2 >= 0x258) ? (temp_a0_2 - 0x244) : temp_a0_2) + 0x6C0) * 0x10;
    Gfx_DrawRect(temp_a0_3, 0x72A0, temp_a0_3, temp_a3, 0);
    var_f1 = 0;
    if (var_s1 >= 0x1D) {
        goto block_8;
    }
    goto loop_6;
block_8:
    var_s1_2 = 0;
    Gfx_DrawRect(0x9360, 0x73E0, 0x9360, 0x8C40, 0);
    var_f1_2 = 0;
loop_10:
    temp_a3_2 = var_s1_2 * 0x14;
    temp_a0_4 = !(var_s1_2 & 3) ? 0x9280 : 0x92E0;
    var_s1_2 += 1;
    temp_a1 = var_f1_2 + temp_a3_2;
    temp_a1_2 = (temp_a1 <= 0x1D) ? (temp_a1 + 0x186) : temp_a1;
    temp_a1_3 = (((temp_a1_2 >= 0x1A4) ? (temp_a1_2 - 0x186) : temp_a1_2) + 0x720) * 0x10;
    Gfx_DrawRect(temp_a0_4, temp_a1_3, 0x9360, temp_a1_3, 0);
    var_f1_2 = 0;
    if (var_s1_2 < 0x13) {
        goto loop_10;
    }
    goto block_40;
block_12:
    var_fp = ((u8 *)&D_01FC4A40) + 0x24;
    var_f26 = 0.400000006f;
    temp_s2_2 = D_0046B320[2] | 0x40;
    temp_a1_4 = ((s32)(M32(((u8 *)&D_01FC4A40), 0x28)) + 0x720) * 0x10;
    Gfx_DrawRect(0x6C00, temp_a1_4, 0x9400, temp_a1_4, 0);
    temp_s0 = ((s32)(M32(((u8 *)&D_01FC4A40), 0x24)) + 0x6C0) * 0x10;
    Gfx_DrawRect(temp_s0, 0x7200, ((s32)(M32(((u8 *)&D_01FC4A40), 0x24)) + 0x6C0) * 0x10, 0x8E00, 0);
    goto block_40;
block_13:
    temp_f23 = 3.14159298f;
    temp_s2_3 = D_0046B320[3] | 0x80;
    var_s1_3 = 0;
    temp_f21 = (-1.0471977f);
    temp_f20 = 2460.0f;
loop_14:
    temp_f24 = ((((2.0f * (f32) var_s1_3) + 1.0f) * temp_f23) / 36.0f) + temp_f21;
    temp_x = 0x8000 + (s32)(sinf(temp_f24) * temp_f20);
    temp_y = (s32)(cosf(temp_f24) * temp_f20) + 0x7DE0;
    temp_a3_3 = D_01FC4A30[2][var_s1_3];
    var_s1_3 += 1;
    func_003DFF30(-temp_f24, temp_x, temp_y, 0x50, temp_a3_3, temp_s2_3);
    if (var_s1_3 < 0xC) {
        goto loop_14;
    }
    var_s1_4 = (s32) 0.200000003f % 12;
    var_v0 = var_s1_4 < -6;
    if (var_s1_4 < 6) {
        goto block_21;
    }
loop_17:
    var_s1_4 -= 0xC;
    if (var_s1_4 >= 6) {
        goto loop_17;
    }
    var_v0 = var_s1_4 < -6;
    goto block_22;
block_20:
    var_v0 = var_s1_4 < -6;
block_21:
block_22:
    if (var_v0 == 0) {
        goto block_24;
    }
    var_s1_4 += 0xC;
    goto block_20;
block_24:
    temp_f29 = 3.14159298f;
    var_s1_5 = 0;
    temp_f23_2 = 2600.0f;
    temp_f25 = temp_f23_2;
    temp_f27 = 0.130899712f;
    temp_f24_2 = (((f32) var_s1_4 * 3.14159298f) / 18.0f) + 0.087266475f;
    temp_f20_2 = temp_f24_2 - 0.0700000003f;
    temp_f21_2 = temp_f24_2 + 0.0700000003f;
    sinf(temp_f20_2);
    cosf(temp_f20_2);
    sinf(temp_f20_2);
    sp10 = 0 + 0x8000;
    cosf(temp_f20_2);
    sp14 = 0 + 0x7DE0;
    sinf(temp_f21_2);
    sp20 = 0 + 0x8000;
    cosf(temp_f21_2);
    sp24 = 0 + 0x7DE0;
    sinf(temp_f21_2);
    sp30 = 0 + 0x8000;
    cosf(temp_f21_2);
    sp34 = 0 + 0x7DE0;
    func_003E1700(spbuf, 0x80808080, 0x84);
loop_26:
    var_s1_6 = 0;
    var_f24 = (f32) var_s1_5 * temp_f29 * 0.5f;
loop_27:
    temp_s0_2 = spbuf + (var_s1_6 * 0x10);
    var_s1_6 += 1;
    cosf(var_f24);
    temp_f12 = var_f24;
    var_f24 += temp_f27;
    M32(temp_s0_2, 0x0) = (s32) (0 + 0x8000);
    sinf(temp_f12);
    M32(temp_s0_2, 0xC) = 0;
    M32(temp_s0_2, 0x8) = 0xFFFFF;
    M32(temp_s0_2, 0x4) = (s32) (0 + 0x7DE0);
    if (var_s1_6 < 0xD) {
        goto loop_27;
    }
    var_s1_7 = 0;
    var_a0 = 0 * 0x10;
loop_29:
    Gfx_DrawLine(spbuf + var_a0, spbuf + (var_a0 + 0x10), temp_s2_3, 1);
    var_s1_7 += 1;
    var_a0 = var_s1_7 * 0x10;
    if (var_s1_7 < 0xC) {
        goto loop_29;
    }
    var_s1_5 += 1;
    if (var_s1_5 < 4) {
        goto loop_26;
    }
    temp_f27_2 = 0.419999987f;
    var_s1_8 = 0;
    temp_f25_2 = (-0.419999987f);
loop_32:
    var_f0 = 3.14159274f;
    if (var_s1_8 >= 2) {
        goto block_34;
    }
    var_f0 = 0.0f;
block_34:
    var_s1_8 += 1;
    if (!(var_s1_8 & 1)) {
        goto block_36;
    }
    var_f24_2 = var_f0 + temp_f27_2;
    goto block_37;
block_36:
    var_f24_2 = var_f0 + temp_f25_2;
block_37:
    cosf(var_f24_2);
    sinf(var_f24_2);
    cosf(var_f24_2);
    sp10 = 0 + 0x8000;
    sinf(var_f24_2);
    sp14 = 0 + 0x7DE0;
    Gfx_DrawLine(spbuf, &sp10, 0x80000080, 1);
    if (var_s1_8 < 4) {
        goto loop_32;
    }
    goto block_40;
block_39:
    var_fp = ((u8 *)&D_01FC4A40) + 0x24;
block_40:
    var_s1_9 = 0;
loop_41:
    temp_a0_5 = var_s1_9 * 4;
    temp_f1 = *(f32 *)((u8 *)&D_01FC4A50 + temp_a0_5) - *(f32 *)(var_fp + temp_a0_5);
    temp_v0 = ((u8 *)&D_01FC4A6C) + temp_a0_5;
    if (!(temp_f1 < 0.0f)) {
        goto block_47;
    }
    temp_f2 = *temp_v0;
    if (temp_f2 > 0.0f) {
        goto block_44;
    }
    goto block_46;
block_44:
    var_f0_2 = temp_f2 - var_f26;
    goto block_52;
block_46:
    var_f0_2 = temp_f2 - (var_f26 * 0.100000001f);
    goto block_52;
block_47:
    temp_f2_2 = *temp_v0;
    if (temp_f2_2 < 0.0f) {
        goto block_49;
    }
    goto block_51;
block_49:
    var_f0_2 = temp_f2_2 + var_f26;
    goto block_52;
block_51:
    var_f0_2 = temp_f2_2 + (var_f26 * 0.100000001f);
block_52:
    *temp_v0 = var_f0_2;
    temp_v0_2 = ((u8 *)&D_01FC4A6C) + temp_a0_5;
    if (!(fabsf(temp_f1) < var_f26)) {
        goto block_55;
    }
    if (!(fabsf(*temp_v0_2) < var_f26)) {
        goto block_55;
    }
    *temp_v0_2 = 0.0f;
    M32(((u8 *)&D_01FC4A40), 0x34) = (u16) (M32(((u8 *)&D_01FC4A40), 0x34) | (1 << var_s1_9));
block_55:
    temp_v1 = var_fp + temp_a0_5;
    if (M32(((u8 *)&D_01FC4A40), 0x4) != 2) {
        goto block_58;
    }
    var_v0_2 = ((u8 *)&D_01FC4A6C) + temp_a0_5;
    if ((s16) M32(((u8 *)&D_01FC4A40), 0x34) != 3) {
        goto block_59;
    }
    var_f0_3 = *(f32 *)((u8 *)&D_01FC4A50 + temp_a0_5);
    goto block_60;
block_58:
    var_v0_2 = ((u8 *)&D_01FC4A6C) + temp_a0_5;
block_59:
    var_f0_3 = *temp_v1 + *var_v0_2;
block_60:
    var_s1_9 += 1;
    *temp_v1 = var_f0_3;
    if (var_s1_9 < 2) {
        goto loop_41;
    }
    return;
}
#undef M32
#else
INCLUDE_ASM("asm/nonmatchings/menu_bg", func_003E38F0);
#endif

void Bowl_DrawFloorGrid(void) {
    MbgVec4 v[2];
    s32 scr[2][4];
    f32 z;
    u32 c;

    v[0][0] = -10000.0f;
    v[1][0] = 10000.0f;
    v[0][1] = v[1][1] = 0.0f;
    v[0][3] = v[1][3] = 1.0f;
    z = 11000.0f;
    do {
        v[1][2] = z;
        v[0][2] = z;
        z -= 1000.0f;
        sceVu0RotTransPersN(scr, D_004334C0, v, 2, 0);
        scr[0][2] = scr[1][2] = 0xFFFF0;
        if ((u32)(scr[0][0] - 1) <= 0xFFFE && scr[0][1] > 0 && scr[0][1] <= 0xFFFF && (u32)(scr[1][0] - 1) <= 0xFFFE &&
        scr[1][1] > 0 && scr[1][1] <= 0xFFFF) {
            c = D_0046B320[D_01FC4A40.unk4];
            Gfx_DrawLine(scr[0], scr[1], ((c >> 1) & 0xFF00) | ((c >> 17) & 0xFF) << 16 | (c >> 25) << 24 | 0x80, 1);
        }
    } while (z > -8000.0f);
    v[0][2] = 4000.0f;
    v[1][2] = -8000.0f;
    z = -5000.0f;
    do {
        v[1][0] = z;
        v[0][0] = z;
        z += 1000.0f;
        sceVu0RotTransPersN(scr, D_004334C0, v, 2, 0);
        scr[0][2] = scr[1][2] = 0xFFFF0;
        if ((u32)(scr[0][0] - 1) <= 0xFFFE && scr[0][1] > 0 && scr[0][1] <= 0xFFFF && (u32)(scr[1][0] - 1) <= 0xFFFE &&
        scr[1][1] > 0 && scr[1][1] <= 0xFFFF) {
            c = D_0046B320[D_01FC4A40.unk4];
            Gfx_DrawLine(scr[0], scr[1], ((c >> 1) & 0xFF00) | ((c >> 17) & 0xFF) << 16 | (c >> 25) << 24 | 0x40, 1);
        }
    } while (z < 5000.0f);
}

void Bowl_DrawCube(void) {
    MbgMat m;
    MbgMat cam;
    s32 scr[8][4];
    MbgVec4 rot;
    f32 *r = rot;
    s32 *a, *b;
    s32 i, j, col;
    f32 t;

    for (i = 0; i < 4; i++) {
        t = i;
        rot[0] = func_0034A570(D_01FC4A40.unk40 - t * 0.06f);
        rot[1] = func_0034A570(D_01FC4A40.unk44 - t * 0.16f);
        rot[2] = func_0034A570(D_01FC4A40.unk48 - t * 0.02f);
        Mtx_Unit(m);
        func_00349D70(m, r);
        m[3][2] = 2400.0f;
        sceVu0CopyMatrix(cam, D_00433440);
        cam[2][0] += -250.0f;
        cam[2][1] += 16.0f;
        sceVu0MulMatrix(m, cam, m);
        sceVu0RotTransPersN(scr, m, D_0046B3F0, 8, 0);
        col = D_0046B320[D_01FC4A40.unk4] | (0x80 - (i << 5));
        for (j = 0; j < 12; j++) {
            a = scr[D_0046B470[j][0]];
            b = scr[D_0046B470[j][1]];
            Gfx_DrawRect(a[0], a[1], b[0], b[1], col);
        }
    }
    D_01FC4A40.unk40 = func_0034A570(D_01FC4A40.unk40 + 0.03f);
    D_01FC4A40.unk44 = func_0034A570(D_01FC4A40.unk44 + 0.08f);
    D_01FC4A40.unk48 = func_0034A570(D_01FC4A40.unk48 + 0.01f);
}

INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005233B0);
INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005233C0);
void func_003E45E0(void) {
    MbgVec v = D_005233B0;
    u32 scr[4];
    u32 x, y, x1, y1;
    s32 col;

    if (func_003DB8A8(0) == 0) {
        return;
    }
    func_003DB808(0, &v.x);
    v.x = -v.x + 200.0f;
    v.y = -v.y + 1600.0f;
    v.z = -v.z;
    v.w = 1.0f;
    sceVu0RotTransPers((s32 *)scr, D_004334C0, &v.x, 0);
    x = scr[0];
    y = scr[1];
    col = D_0046B320[D_01FC4A40.unk4] | 0x80;
    if (x < 0x7880) {
        return;
    }
    switch (D_01FC4A40.unk4) {
    case 2:
        if (D_01FC4A40.unk2 & 0x40) {
            x1 = x + 0x180;
            y1 = y - 0x100;
            Gfx_DrawRect(x, y, x1, y1, col);
            Gfx_DrawRect(x1, y1, x + 0x4C0, y1, col);
            Text_Printf(D_00522FF0, (x >> 4) - 0x6A8, (y >> 4) - 0x728, 11, 6, 6);
        }
        break;
    case 3:
        if (D_01FC4A40.unk2 & 0x40) {
            y += 0x40;
            x1 = x + 0x180;
            y1 = y + 0xC0;
            Gfx_DrawRect(x, y, x1, y1, col);
            y += 0x20;
            Gfx_DrawRect(x1, y1, x + 0x380, y1, col);
            func_003E0460(x + 0x200, y, 100, 100, D_01FC4A30[3][0x12], col);
            func_003E0460(x + 0x2E0, y, 100, 100, D_01FC4A30[3][0x13], col);
        }
        break;
    case 0:
    case 1:
        x1 = x + 0x180;
        y1 = y - 0x100;
        Gfx_DrawRect(x, y, x1, y1, col);
        Gfx_DrawRect(x1, y1, x + 0x9E0, y1, col);
        Text_Printf(D_005233C0, (x >> 4) - 0x6A8, (y >> 4) - 0x728, 11, 6, 6);
        break;
    }
}

#if 0
/* NON_MATCHING: 100 lines: the u16 angle test becomes a bit-15 test (original: slt against 0x7FFF in s5), the frame counter is not kept in s7 */
void func_003E4860(MbgAim *aim, MbgObj *o, f32 *pos) {
    MbgVec4 v[2];
    s32 scr[4][4];
    s32 t = (s16)D_01FC4A40.unk2 & 0x7F;
    s32 i, a;
    f32 best = 100000.0f;
    f32 tn, dz, dx, d;

    for (i = 10; i > 0; i--, o++) {
        if (o->active != 0) {
            tn = sinf(MBG_RAD(aim->angle)) / cosf(MBG_RAD(aim->angle));
            dz = o->z - pos[2];
            dx = o->x - pos[0];
            if (fabsf(-tn * dz + dx) / SqrtS(tn * tn + 1.0f) < 110.0f) {
                d = SqrtS(dz * dz + dx * dx);
                if (d < best) {
                    best = d;
                }
            }
        }
    }
    v[0][0] = pos[0];
    v[0][1] = 250.657898f;
    v[0][2] = -pos[2];
    v[0][3] = 1.0f;
    v[1][0] = pos[0] + sinf(MBG_RAD(aim->angle)) * best;
    v[1][1] = 250.657898f;
    v[1][2] = -pos[2] - cosf(MBG_RAD(aim->angle)) * best;
    v[1][3] = 1.0f;
    sceVu0RotTransPersN(scr, D_004334C0, v, 2, 0);
    t <<= 2;
    if (t & 0x40) {
        a = (0x7F - t) | 0xC0;
    } else {
        a = t | 0xC0;
    }
    if ((u32)(scr[0][0] - 1) <= 0xFFFE && scr[0][1] > 0 && scr[0][1] <= 0xFFFF && scr[0][2] > 0 &&
        scr[0][2] <= 0xFFFFF && (u32)(scr[1][0] - 1) <= 0xFFFE && scr[1][1] > 0 && scr[1][1] <= 0xFFFF &&
        scr[1][2] > 0 && scr[1][2] <= 0xFFFFF) {
        Gfx_DrawLine(scr[0], scr[1], a << 24 | 0x80, 0);
    }
}
#else
LIT4_WORD(D_005F58BC, 0x47C35000);
LIT4_WORD(D_005F58C0, 0x38C90FDB);
LIT4_WORD(D_005F58C4, 0x437AA86C);
LIT4_WORD(D_005F58C8, 0x38C90FDB);
LIT4_WORD(D_005F58CC, 0x38C90FDB);
LIT4_WORD(D_005F58D0, 0x437AA86C);
LIT4_WORD(D_005F58D4, 0x38C90FDB);
LIT4_WORD(D_005F58D8, 0x38C90FDB);
INCLUDE_ASM("asm/nonmatchings/menu_bg", func_003E4860);
#endif

#if 0
/* NON_MATCHING: 102 lines: the character id and the state pointer take s0 / s2 the other way round, the kind table is read through a fresh %hi in the loop */
void Bowl_DrawHud(MbgAim *f, MbgObj *o, f32 *pos, s16 pad, s16 arg4) {
    MbgState *m = &D_01FC4A40;
    s16 id = ((s16 *)f)[0x17];
    s16 t = m->unk2;
    s32 i, top;
    u32 y;

    m->unk4 = 4;
    if ((id >= 0x46 && id < 0x4A) || id == 0x56 || id == 0x57) {
        m->unk4 = 0;
    }
    if (id >= 0x58 && id < 0x5A) {
        m->unk4 = 1;
    }
    if (id >= 0x35 && id < 0x39) {
        m->unk4 = 2;
    }
    if (id >= 0x11 && id < 0x15) {
        m->unk4 = 3;
    }
    if (m->unk4 == 4) {
        if (m->unk0 == 0 && func_003F1F08() != 0 && (pad & 8)) {
            func_003E4860(f, o, pos);
        }
        return;
    }
    top = (0x1C0 - m->unk6) / 2;
    for (i = 0; i < 8; i++) {
        y = (((m->unk36 * 2 - i * 4) & 0x3FE) + 0x720) << 4;
        if (y >= (u32)((top + 0x720) << 4) && y < (u32)((top + m->unk6 + 0x720) << 4)) {
            Gfx_DrawRect(0x6C00, y, 0x9400, y, D_0046B320[m->unk4] | (16 - i * 2));
        }
    }
    func_003E1580(0x6C00, (top + 0x720) << 4, 0x9400, (top + m->unk6 + 0x720) << 4, 0xFFFE0,
                  D_0046B310[m->unk4], 0x41);
    switch (m->unk0) {
    case 0:
        m->unk6 = t * 0x1C0 / 12;
        if (t >= 12) {
            m->unk0 = 1;
            m->unk2 = 0;
        }
        break;
    case 1:
        m->unk6 = 0x1C0;
        if (m->unk4 == 3) {
            func_003DEB80(1);
        }
        func_003E1C68(o);
        func_003E3178(arg4);
        func_003E38F0();
        func_003E45E0();
        switch (m->unk4) {
        case 0:
        case 1:
            Bowl_DrawCube();
            break;
        case 2:
            Bowl_DrawFloorGrid();
            break;
        case 3:
            Bowl_DrawBarcode(430, 360);
            break;
        }
        func_003E4860(f, o, pos);
        break;
    case 2:
        m->unk6 = (12 - t) * 0x1C0 / 12;
        if (t >= 12) {
            m->unk2 = 0;
            m->unk0 = 3;
        }
        break;
    case 3:
        m->unk6 = 0;
        break;
    }
    m->unk2++;
    m->unk36++;
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_bg", Bowl_DrawHud);
#endif

u8 *func_003E4FA0(u8 *p) {
    while (*p++ != 0) {
    }
    return p;
}

u8 *Text_Get(s32 n) {
    s32 i;
    u8 *p = D_0046AF40[0];
    for (i = 0; i < n; i++) {
        p = func_003E4FA0(p);
    }
    return p;
}

s32 Text_Width(s32 n) {
    s32 esc = 0;
    s32 maxw = 0;
    s32 i = 0;
    s32 w = 0;
    u8 *p = D_0046AF40[0];

    for (; i < n; i++) {
        p = func_003E4FA0(p);
    }
    while (*p != 0) {
        if (esc == 1) {
            if (*p == 0x5C) {
                w += D_004572E0[*p - 0x20];
            }
            esc = 0;
            if (*p == 0x6E) {
                maxw = w;
                w = 0;
            }
        } else if (*p == 0x5C) {
            esc = 1;
        } else if (*p == 0x20) {
            w += 4;
        } else {
            w += D_004572E0[*p - 0x20];
        }
        p++;
    }
    return maxw < w ? w : maxw;
}

s32 Text_LineCount(s32 n) {
    s32 lines = 1;
    s32 esc = 0;
    s32 i = 0;
    s32 w;
    u8 *p = D_0046AF40[0];

#ifdef PORT
    /* w is never initialised: the original adds to whatever its caller left in s0, which in its only caller
     * (font_tex.c func_00360A30) is the text id it passes, n. A host garbage w gave two rows instead of one. */
    w = n;
#endif

    for (; i < n; i++) {
        p = func_003E4FA0(p);
    }
    while (*p != 0) {
        if (esc == 1) {
            if (*p == 0x5C) {
                w += D_004572E0[*p - 0x20];
            }
            esc = 0;
            if (*p == 0x6E) {
                lines++;
                w = 0;
            }
        } else if (*p == 0x5C) {
            esc = 1;
        } else if (*p == 0x20) {
            w += 4;
        } else {
            w += D_004572E0[*p - 0x20];
        }
        p++;
    }
    return lines + w / 580;
}

INCLUDE_RODATA("asm/nonmatchings/menu_bg", D_005233E0);
s32 Text_DrawId(s32 x, s32 y, s32 n, s32 color) {
    u8 *p = D_0046AF40[0];
    s32 i;

    func_003DFA00(D_005233E0, x, y, color, 580);
    for (i = 0; i < n; i++) {
        p = func_003E4FA0(p);
    }
    func_003E06A0(p);
}

void func_003E52E8(s8 *st) {
    s32 bits = func_0035EA80();
    s32 i;
    for (i = 0; i < 13; i++) {
        if (((bits >> i) & 1) == 0) {
            st[i] = 0;
        } else {
            st[i] = 1;
        }
    }
}

s32 func_003E5350(void) {
    s32 r = func_00361B58();
    return r;
}

void Fade_DrawScreen(s32 alpha) {
    u64 *p = func_00399520();

    *p++ = 0xA1AB400000008001;
    *p++ = 0x43431EEEEE;
    *p++ = 1;
    *p++ = 0x15;
    *p++ = 0x31001;
    *p++ = 0x48;
    *p++ = 0x20800002A812A300;
    *p++ = 7;
    *p++ = 0;
    *p++ = 9;
    *p++ = (u64)alpha << 32 | 0x64;
    *p++ = 0x43;
    *p++ = 0x8000000080;
    *p++ = 0x8000000080;
    *p++ = 0x800000008;
    *p++ = 0;
    *p++ = 0x720000006C00;
    *p++ = 0x1FFFFFFFF;
    *p++ = 0x37F8000027F8;
    *p++ = 0;
    *p++ = 0xA9F1000093F1;
    *p++ = 0x1FFFFFFFF;
    func_00399650(p);
}

void Menu_DrawCursorBar(s32 x, s32 y, s32 w, s32 z) {
    MbgQuad q;

    x <<= 4;
    w <<= 4;
    y <<= 4;
    q = D_0043FD10[21];
    q.v[0].x += x;
    q.v[2].x += x;
    q.v[3].x = q.v[1].x = q.v[0].x + w;
    q.v[0].y += y;
    q.v[1].y += y;
    q.v[2].y += y;
    q.v[3].y += y;
    func_003BB680(&q, z, 0);
}
