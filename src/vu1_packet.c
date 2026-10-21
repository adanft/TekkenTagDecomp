/* 0x3420C8-0x342E78 (module vu1_packet; docs/systems/render.md section 12.1): fighter shadows (the planar shadow
 * matrix D_01FC49F0) and fixed VIF1 / DIRECT packets. One object [V: its own .lit4 pool 0x5F57EC-0x5F5804, a subsegment
 * out of text order, see the end of the file]. Every NON_MATCHING draft (#if 0) compiles when enabled. */
#include "common.h"
#include "fighter.h"

extern Fighter gFighters[];

typedef f32 Vu0Matrix[4][4];

/* VIF1 packet of the SDK (sceVif1Packet). */
typedef struct {
    /* 0x0 */ u32 *pCurrent;
    /* 0x4 */ u32 *pBase;
    /* 0x8 */ u32 *pDmaTag;
    /* 0xC */ u32 *pVifCode;
    /* 0x10 */ u32 pad10[4];
} Vif1Packet;

/* The drawn object as func_00341B70 sees it, with its position at 0xB0. */
typedef struct {
    /* 0x000 */ u8 pad0[0xB0];
    /* 0x0B0 */ f32 pos[3];
    /* 0x0BC */ u8 padBC[0x88];
    /* 0x144 */ s32 model;
} DrawObj;

typedef struct {
    /* 0x000 */ u8 pad0[0x1E];
    /* 0x01E */ s16 unk1E;
    /* 0x020 */ u8 pad20[0x570];
    /* 0x590 */ f32 pos[3];
} Unk3422C0;

extern f32 D_005F694C;
extern Vu0Matrix D_01FC49F0;
extern u16 D_0046B280[];
extern s32 D_0046B254[];
extern u8 D_0046B180[];
extern u8 D_0046B240[];
extern u32 D_0046B174[];
extern u8 D_00522F90[];

u32 Spr_Setup(u32 count, s32 reserved);
void func_00343128(u32 spr, void *src, u32 qwc);
void func_00343178(u32 spr, u32 qwc);
u32 Dma_Wait(volatile u32 *chan);
void Dma_AddRefTag(u32 *pkt, s32 i);
u32 func_00342FB0(void);
void sceVif1PkInit(Vif1Packet *pk, u32 base);
void sceVif1PkReset(Vif1Packet *pk);
void sceVif1PkRef(Vif1Packet *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceVif1PkTerminate(Vif1Packet *pk);
u32 sceVif1PkSize(Vif1Packet *pk);
void sceVif1PkCnt(Vif1Packet *pk, u32 flag);
void sceVif1PkAlign(Vif1Packet *pk, u32 bit, u32 pos);
u32 *sceVif1PkReserve(Vif1Packet *pk, u32 count);
void sceVif1PkOpenDirectCode(Vif1Packet *pk, s32 irq);
void sceVif1PkCloseDirectCode(Vif1Packet *pk);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0ScaleVectorXYZ(f32 *out, f32 *in, f32 s);
void func_00353C88(s32 i, f32 *out);
void func_0034A5D0(Vu0Matrix m, f32 *v);
void func_00356710(Vu0Matrix m);
void func_00341B70(void *files, DrawObj *o, Vu0Matrix m);
void func_00342F00(Vu0Matrix *a, Vu0Matrix *b, Vu0Matrix *c, Vu0Matrix *d, void *pkt);

/* NON_MATCHING: every instruction matches, but its .lit4 word (5800.0f) lies last in the object's pool, after the
 * words of later functions, so the C literal would land first */
#if 0
void func_003420C8(s32 n) {
    s32 i = 0;

    D_005F694C = 3200.0f;
    while (i < n) {
        if (gFighters[i++].unkB130 & 2) {
            D_005F694C = 5800.0f;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/vu1_packet", func_003420C8);
#endif

void func_00342128(Unk3422C0 *f) {
    f32 v[4];
    f32 s;

    s = 1.0f / D_005F694C;
    func_00353C88(f->unk1E, v);
    func_0034A5D0(D_01FC49F0, v);
    sceVu0ScaleVectorXYZ(D_01FC49F0[0], D_01FC49F0[0], s);
    sceVu0ScaleVectorXYZ(D_01FC49F0[1], D_01FC49F0[1], s);
    sceVu0ScaleVectorXYZ(D_01FC49F0[2], D_01FC49F0[2], s);
    D_01FC49F0[3][3] = 1.0f;
    D_01FC49F0[3][0] = 0.5f;
    D_01FC49F0[3][2] = 0.0f;
    D_01FC49F0[3][1] = 0.5f;
}

void func_003421D8(Vu0Matrix *a, Vu0Matrix *b) {
    Vif1Packet pk;
    u32 spr;

    spr = Spr_Setup(0, 0x130);
    func_00343128(spr + 0x10, (void *)D_0046B174[0], 0x12);
    sceVif1PkInit(&pk, spr);
    Dma_Wait((volatile u32 *)0x1000D400);
    sceVif1PkCnt(&pk, 0);
    sceVif1PkAlign(&pk, 2, 0);
    pk.pBase[3] = 0x11000000;
    sceVif1PkReserve(&pk, 0x48);
    func_00342F00(b, a, a, a, (void *)(spr + 0x10));
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.pBase, sceVif1PkSize(&pk));
}

void func_003422C0(Unk3422C0 *f, void *files, DrawObj *o) {
    Vu0Matrix m;
    Vu0Matrix t;

    func_00356710(t);
    t[3][0] = o->pos[0] - f->pos[0];
    t[3][1] = o->pos[1] - f->pos[1];
    t[3][2] = o->pos[2] - f->pos[2];
    sceVu0MulMatrix(m, D_01FC49F0, t);
    func_00341B70(files, o, m);
}

void func_00342360(s32 i) {
    u32 v;

    if (i < 0x17) {
        v = D_0046B280[i];
    } else {
        v = 0x40;
    }
    D_0046B254[0] = v;
}

void func_00342390(void) {
    Vif1Packet pk;

    Spr_Setup(1, 0);
    sceVif1PkInit(&pk, *(u32 *)0x70000000);
    sceVif1PkRef(&pk, D_0046B180, 0xC, 0x10000000, 0x5000000C, 0);
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.pBase, sceVif1PkSize(&pk));
}

void func_00342400(void) {
    Vif1Packet pk;
    u32 spr;
    u32 p;

    Spr_Setup(2, 0);
    spr = *(u32 *)0x70000000;
    p = spr;
    Dma_AddRefTag(&p, 0x20);
    func_00343178(spr, (s32)(p - spr) >> 4);
    sceVif1PkInit(&pk, func_00342FB0());
    sceVif1PkRef(&pk, D_0046B240, 4, 0x10000000, 0x50000004, 0);
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.pBase, sceVif1PkSize(&pk));
}

INCLUDE_ASM("asm/nonmatchings/vu1_packet", func_003424A8);

INCLUDE_ASM("asm/nonmatchings/vu1_packet", func_00342818);

void func_00342988(void) {
    Vif1Packet pk;
    u32 spr;

    Spr_Setup(1, 0);
    spr = *(u32 *)0x70000000;
    func_00343128(spr + 0x10, D_00522F90, 3);
    Dma_Wait((volatile u32 *)0x1000D400);
    sceVif1PkInit(&pk, spr);
    sceVif1PkReset(&pk);
    sceVif1PkCnt(&pk, 0);
    sceVif1PkOpenDirectCode(&pk, 0);
    sceVif1PkReserve(&pk, 0xC);
    sceVif1PkCloseDirectCode(&pk);
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.pBase, sceVif1PkSize(&pk));
}

INCLUDE_ASM("asm/nonmatchings/vu1_packet", func_00342A38);

void func_00342E38(void) {
    f32 *vu = (f32 *)0x11004000;

    vu[0] = 1.0f / 2046.0f;
    vu[1] = 1.0f / 2046.0f;
    vu[2] = 1.0f / 32767.0f;
    vu[3] = 255.0f;
    ((s32 *)vu)[7] = 0x8000;
    vu[6] = 0.5f;
}

/* The rest of the object's .lit4 pool (0x5F57EC-0x5F5804): in the original pool the two words of func_00342E38 come
 * first, then those of func_003424A8, func_00342818, func_00342A38 and func_003420C8, so the words of the functions
 * before func_00342E38 are emitted after it. */
LIT4_WORD(D_005F57F4, 0xC0278D37);
LIT4_WORD(D_005F57F8, 0x4A1A4D90);
LIT4_WORD(D_005F57FC, 0xC0278D37);
LIT4_WORD(D_005F5800, 0x45B54000);
