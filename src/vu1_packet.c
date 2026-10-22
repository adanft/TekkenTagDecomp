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
    /* 0x020 */ s16 unk20;
    /* 0x022 */ s16 unk22;
    /* 0x024 */ u8 pad24[0x103];
    /* 0x127 */ u8 unk127;
    /* 0x128 */ u8 pad128[0x468];
    /* 0x590 */ f32 pos[3];
    /* 0x59C */ u8 pad59C[0x84];
    /* 0x620 */ f32 unk620;                   /* depth of the fighter (Model_Render) */
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
void Mtx_Unit(Vu0Matrix m);
void func_00341B70(void *files, DrawObj *o, Vu0Matrix m);
void *memset(void *d, s32 c, s32 n);
void sceVu0RotMatrixZ(Vu0Matrix m0, Vu0Matrix m1, f32 rz);

/* A fighter model's shadow data (func_00367350): its packet size at 0xC, the number of parts at 0x10 and their sizes
 * in quadwords from 0x14 (8 bytes each). */
typedef struct {
    /* 0x00 */ u8 pad0[0xC];
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14[1][2];
} ShadowMdl;

/* Extra joints of a fighter model (func_003B87C8): their count at 4 and joint indexes at 0x60. */
typedef struct {
    /* 0x00 */ u8 pad0[4];
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 pad8[0x58];
    /* 0x60 */ s32 unk60[1];
} ShadowJoints;

ShadowMdl *func_00367350(s32 id);
ShadowJoints *func_003B87C8(s32 id);
extern u32 D_004FAF80[];
extern u32 D_004FAFA0[];
extern u32 D_004FB800[];
extern s32 D_0046B2B0[];
extern u8 D_0046B2F0[];
extern f32 D_00433440[];
extern Vu0Matrix D_00433400;
extern Vu0Matrix D_00433480;
void sceVif1PkCloseDirectCode(Vif1Packet *pk);
s32 Tag_StepCounters(s32 side, s32 a);
void func_003424A8(Unk3422C0 *f, f32 (*v)[4]);
void func_00342818(Unk3422C0 *f);
void func_00342A38(Unk3422C0 *f);
void func_00342F00(Vu0Matrix *a, Vu0Matrix *b, Vu0Matrix *c, Vu0Matrix *d, void *pkt);

/* Its .lit4 word (5800.0f) lies last in the object's pool, after the words of later functions: LIT4_ORDER(3). */
LIT4_ORDER(3);
void func_003420C8(s32 n) {
    s32 i = 0;

    D_005F694C = 3200.0f;
    while (i < n) {
        if (gFighters[i++].unkB130 & 2) {
            D_005F694C = 5800.0f;
        }
    }
}
LIT4_ORDER(0);

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

    Mtx_Unit(t);
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

void func_00342400() {
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

/* NON_MATCHING: 104 lines (first draft); its .lit4 word D_005F57F4 follows func_00342E38's in the pool: as C, LIT4_ORDER(1) in front of it and drop its LIT4_WORD */
#if 0
void func_003424A8(Unk3422C0 *f, f32 (*off)[4]) {
    Vif1Packet pk;
    Vu0Matrix t;
    Vu0Matrix r;
    Vu0Matrix m;
    f32 l[4];
    s32 n;
    s32 *sz;
    f32 s;
    ShadowMdl *mdl;
    ShadowJoints *ex;
    u32 spr;
    u8 *dst;
    u8 *data;
    s32 i;
    s32 k;
    s32 q;

    n = D_004FB800 - D_004FAF80;
    mdl = func_00367350(f->unk22);
    s = 256.0f / D_005F694C;
    if (mdl == NULL) {
        return;
    }
    sz = mdl->unk14[0];
    ex = func_003B87C8(f->unk22);
    spr = Spr_Setup(1, 0x900);
    dst = (u8 *)(spr + 0x10);
    func_00343128((u32)dst, D_004FAF80, n >> 2);
    func_00353C88(f->unk1E, l);
    func_0034A5D0(m, l);
    sceVu0ScaleVectorXYZ(m[0], m[0], s);
    sceVu0ScaleVectorXYZ(m[1], m[1], s);
    sceVu0ScaleVectorXYZ(m[2], m[2], s);
    m[3][1] = 2048.0f;
    m[3][3] = 1.0f;
    m[3][0] = 2048.0f;
    m[3][2] = 0.0f;
    Mtx_Unit(t);
    t[3][0] = -f->pos[0] + (*off)[0];
    t[3][1] = -f->pos[1] + (*off)[1];
    t[3][2] = -f->pos[2] + (*off)[2];
    sceVu0MulMatrix(m, m, t);
    Mtx_Unit(r);
    sceVu0RotMatrixZ(r, r, -2.6179938f);
    sceVif1PkInit(&pk, spr);
    Dma_Wait((volatile u32 *)0x1000D400);
    sceVif1PkCnt(&pk, 0);
    sceVif1PkAlign(&pk, 2, 0);
    pk.pBase[3] = 0x11000000;
    sceVif1PkReserve(&pk, n);
    dst += (D_004FAFA0 - D_004FAF80) * 4;
    for (i = 0; i < 15; i++) {
        sceVu0MulMatrix((f32 (*)[4])dst, m, (f32 (*)[4])((u8 *)f + D_0046B2B0[i] * 0xC0 + 0xDB0));
        if (i == 11 || i == 14) {
            sceVu0MulMatrix((f32 (*)[4])dst, (f32 (*)[4])dst, r);
        }
        dst += 0x70;
    }
    for (i = 0; i < ex->unk4; i++) {
        sceVu0MulMatrix((f32 (*)[4])dst, m, (f32 (*)[4])((u8 *)f + ex->unk60[i] * 0xC0 + 0xDB0));
        dst += 0x70;
    }
    k = mdl->unk10;
    data = (u8 *)(((u32)mdl + mdl->unkC + 0xF) & ~0xF);
    for (i = 0; i < k; i++) {
        q = *sz;
        sceVif1PkRef(&pk, data, q, 0, 0, 0);
        sz += 2;
        data += q << 4;
    }
    sceVif1PkTerminate(&pk);
    func_00343178(spr, sceVif1PkSize(&pk));
}
#else
INCLUDE_ASM("asm/nonmatchings/vu1_packet", func_003424A8);
#endif

/* Its .lit4 word (D_005F57F8) follows func_00342E38's and func_003424A8's in the pool: LIT4_ORDER(1). */
LIT4_ORDER(1);
void func_00342818(Unk3422C0 *f) {
    f32 v[2][4];
    f32 l[4];
    Unk3422C0 *g;
    s32 o;

    memset(v, 0, 0x10);
    func_00342390();
    func_003424A8(f, v);
    if (f->unk1E == 0) {
        { s32 o = 0xB8A0; g = (Unk3422C0 *)((u8 *)gFighters + o); }
        if (g->unk127 != 0 && Tag_StepCounters(g->unk20, 0) == 0) {
            func_00353C88(f->unk1E, l);
            v[0][1] = g->pos[1] - f->pos[1];
            v[1][0] += -g->pos[1] * l[0] / l[1] + f->pos[1] * l[0] / l[1];
            v[1][2] += -g->pos[1] * l[2] / l[1] + f->pos[1] * l[2] / l[1];
            v[0][0] = g->pos[0] - f->pos[0];
            v[0][2] = g->pos[2] - f->pos[2];
            v[1][1] = 0.0f;
            if (v[0][0] * v[0][0] + v[0][2] * v[0][2] < 2528100.0f) {
                func_003424A8(g, v);
            }
        }
    }
    func_00342400((s16)(u16)f->unk1E);
}
LIT4_ORDER(0);

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

/* NON_MATCHING: 82 lines (first draft): the fighter flattened onto a constant depth through the camera (a mirror image [I]); its .lit4 word D_005F57FC follows func_00342818's in the pool: as C, LIT4_ORDER(1) (or 2) and drop its LIT4_WORD_AT; was: must follow func_00342E38's in the pool */
#if 0
void func_00342A38(Unk3422C0 *f) {
    Vif1Packet pk;
    Vu0Matrix r;
    Vu0Matrix m;
    f32 l[4];
    s32 n;
    s32 *sz;
    f32 d;
    f32 z;
    ShadowMdl *mdl;
    ShadowJoints *ex;
    Unk3422C0 *g;
    u32 spr;
    u8 *dst;
    u8 *data;
    s32 i;
    s32 k;
    s32 q;

    n = D_004FB800 - D_004FAF80;
    mdl = func_00367350(f->unk22);
    if (mdl == NULL) {
        return;
    }
    ex = func_003B87C8(f->unk22);
    spr = Spr_Setup(1, 0x900);
    dst = (u8 *)(spr + 0x10);
    func_00343128((u32)dst, D_0046B2F0, 2);
    Dma_Wait((volatile u32 *)0x1000D400);
    sceVif1PkInit(&pk, spr);
    sceVif1PkReset(&pk);
    sceVif1PkCnt(&pk, 0);
    sceVif1PkOpenDirectCode(&pk, 0);
    sceVif1PkReserve(&pk, 8);
    sceVif1PkCloseDirectCode(&pk);
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.pBase, sceVif1PkSize(&pk));
    Dma_Wait((volatile u32 *)0x1000D000);
    func_00343128((u32)dst, D_004FAF80, n >> 2);
    g = (Unk3422C0 *)&gFighters[1 - f->unk1E];
    d = f->unk620 + 1000.0f;
    if (g->unk127 != 0 && Tag_StepCounters(g->unk20, 0) == 0) {
        d = (d + (g->unk620 + 1000.0f)) * 0.5f;
    }
    z = D_00433440[10] + D_00433440[14] / d;
    sz = mdl->unk14[0];
    func_00353C88(f->unk1E, l);
    Mtx_Unit(m);
    m[1][1] = -m[1][1];
    sceVu0MulMatrix(m, D_00433400, m);
    sceVu0MulMatrix(m, D_00433480, m);
    Mtx_Unit(r);
    sceVu0RotMatrixZ(r, r, -2.6179938f);
    sceVif1PkInit(&pk, spr);
    Dma_Wait((volatile u32 *)0x1000D400);
    sceVif1PkCnt(&pk, 0);
    sceVif1PkAlign(&pk, 2, 0);
    pk.pBase[3] = 0x11000000;
    sceVif1PkReserve(&pk, n);
    dst += (D_004FAFA0 - D_004FAF80) * 4;
    for (i = 0; i < 15; i++) {
        sceVu0MulMatrix((f32 (*)[4])dst, m, (f32 (*)[4])((u8 *)f + D_0046B2B0[i] * 0xC0 + 0xDB0));
        if (i == 11 || i == 14) {
            sceVu0MulMatrix((f32 (*)[4])dst, (f32 (*)[4])dst, r);
        }
        ((f32 *)dst)[10] = z;
        ((f32 *)dst)[14] = 0.0f;
        dst += 0x70;
    }
    for (i = 0; i < ex->unk4; i++) {
        sceVu0MulMatrix((f32 (*)[4])dst, m, (f32 (*)[4])((u8 *)f + ex->unk60[i] * 0xC0 + 0xDB0));
        ((f32 *)dst)[10] = z;
        ((f32 *)dst)[14] = 0.0f;
        dst += 0x70;
    }
    k = mdl->unk10;
    data = (u8 *)(((u32)mdl + mdl->unkC + 0xF) & ~0xF);
    for (i = 0; i < k; i++) {
        q = *sz;
        sceVif1PkRef(&pk, data, q, 0, 0, 0);
        sz += 2;
        data += q << 4;
    }
    sceVif1PkTerminate(&pk);
    func_00343178(spr, sceVif1PkSize(&pk));
}
#else
INCLUDE_ASM("asm/nonmatchings/vu1_packet", func_00342A38);
#endif

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
 * before func_00342E38 are emitted after it: func_003424A8's in subsection 0 after func_00342E38's literals, then
 * subsections 1 (func_00342818, LIT4_ORDER), 2 (func_00342A38) and 3 (func_003420C8, LIT4_ORDER). */
LIT4_WORD(D_005F57F4, 0xC0278D37);
LIT4_WORD_AT(2, D_005F57FC, 0xC0278D37);
