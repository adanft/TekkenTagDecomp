/* 0x341B28-0x3420C8 (module render_fighter_a; docs/systems/render.md section 12.3): the VU1 model renderer the stage
 * draw lists end in (Model_Draw, Model_Render) and its VIF1 packet senders. Another object than 0x340000-0x341B28 [V: its
 * .lit4 words lie at 0x5F5700, apart from that object's pool]; the start is [I]. Every NON_MATCHING draft (#if 0)
 * compiles when enabled. */
#include "common.h"

#include "fighter.h"

typedef f32 Vu0Matrix[4][4];

/* VIF1 packet of the SDK (sceVif1Packet). */
typedef struct {
    /* 0x0 */ u32 *pCurrent;
    /* 0x4 */ u32 *pBase;
    /* 0x8 */ u32 *pDmaTag;
    /* 0xC */ u32 *pVifCode;
    /* 0x10 */ u32 pad10[4];
} Vif1Packet;

/* A model's VIF1 data in a loaded file: a 0x70-byte header (part count at 0x10, then one 8-byte record per part
 * whose first word is the part's size in quadwords), then the parts, 16-byte aligned. */
typedef struct {
    /* 0x00 */ u8 pad0[0x10];
    /* 0x10 */ s32 count;
    /* 0x14 */ s32 part[1][2];
} VifModel;

/* A table of model files: per file, the models by index. */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ VifModel **models;
} ModelFile;

/* The fields of the drawn object that select its model (stage objects, fighters' parts). */
typedef struct {
    /* 0x000 */ u8 pad0[0x140];
    /* 0x140 */ s32 file;
    /* 0x144 */ s32 model;
} ModelRef;

u32 Spr_Setup(u32 count, s32 reserved);
void func_00343128(u32 spr, void *src, u32 qwc);
void func_00343178(u32 spr, u32 qwc);
u32 Dma_Wait(volatile u32 *chan);
void sceVif1PkInit(Vif1Packet *pk, u32 base);
void sceVif1PkRef(Vif1Packet *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceVif1PkTerminate(Vif1Packet *pk);
u32 sceVif1PkSize(Vif1Packet *pk);
void sceVif1PkCnt(Vif1Packet *pk, u32 flag);
void sceVif1PkAlign(Vif1Packet *pk, u32 bit, u32 pos);
u32 *sceVif1PkReserve(Vif1Packet *pk, u32 count);
void func_00342E78(Vu0Matrix *a, Vu0Matrix *b, void *pkt);
void func_003421D8(void *a);
void Model_SendParts(VifModel *m);
void Model_Render(VifModel *m, Vu0Matrix *a, ModelRef *o, u32 kind);
extern u32 D_0046B174[];

void sceVu0ApplyMatrix(f32 *out, Vu0Matrix m, f32 *v);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0RotMatrixY(Vu0Matrix m0, Vu0Matrix m1, f32 ry);
f32 sinf(f32 x);
f32 cosf(f32 x);
void func_00356840(void);
void func_00356710(Vu0Matrix m);
void func_00366EA8(Vu0Matrix m);
void func_00366EB8(Vu0Matrix m);
f32 func_00366EC8(void);
void func_00353D68(Vu0Matrix *out);
VifModel *func_00367898(void);
extern s32 D_00415808[];
extern Vu0Matrix D_00433400;

extern Fighter gFighters[];
extern s16 gMode;
extern u16 D_005F5CB6;
extern u8 D_0043B7F8[];
extern f32 D_00433440[];
s32 func_0036E158(void);
s32 func_003E9400(void);
void func_00342FA8(f32 x);

/* Draws object `o` with its model: file o->file of `files`, model o->model. */
void Model_Draw(ModelFile **files, ModelRef *o, Vu0Matrix *a, u32 kind) {
    ModelFile *f;
    VifModel *m;

    files += o->file;
    f = files[1];
    if (f != NULL) {
        m = f->models[o->model];
        if (m != NULL) {
            Model_Render(m, a, o, kind);
        }
    }
}

/* NON_MATCHING: 16 lines: o and the model table swap s0 / s1 */
#if 0
void func_00341B70(ModelFile **files, ModelRef *o, void *a) {
    VifModel **models;

    if (files[1] != NULL) {
        models = files[1]->models;
        if (models[o->model] != NULL) {
            func_003421D8(a);
            Model_SendParts(models[o->model]);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/render_fighter_b", func_00341B70);
#endif

/* Sends the two matrices of a model through VIF1: a header copied to the scratchpad, then 0x48 words of matrices
 * (func_00342E78). */
void Model_SendMatrices(Vu0Matrix *a, Vu0Matrix *b) {
    Vif1Packet pk;
    u32 spr;
    u32 *p;

    spr = Spr_Setup(0, 0x130);
    func_00343128(spr + 0x10, (void *)D_0046B174[0], 0x12);
    sceVif1PkInit(&pk, spr);
    Dma_Wait((volatile u32 *)0x1000D400);
    sceVif1PkCnt(&pk, 0);
    sceVif1PkAlign(&pk, 2, 0);
    pk.pBase[3] = 0x11000000;
    sceVif1PkReserve(&pk, 0x48);
    func_00342E78(a, b, (void *)(spr + 0x10));
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.pBase, sceVif1PkSize(&pk));
}

/* Sends the parts of a model to VIF1 as DMA references (its header goes to the scratchpad first). */
void Model_SendParts(VifModel *m) {
    Vif1Packet pk;
    u32 spr;
    s32 *part;
    u8 *data;
    s32 n;
    s32 i;
    s32 qwc;

    i = 0;
    spr = Spr_Setup(1, 0x70);
    func_00343128(spr, m, 7);
    part = ((VifModel *)spr)->part[0];
    data = (u8 *)(((s32)m + 0x7F) & ~0xF);
    sceVif1PkInit(&pk, *(u32 *)0x70000000);
    Dma_Wait((volatile u32 *)0x1000D400);
    n = ((VifModel *)spr)->count;
    for (; i < n; i++) {
        qwc = *part;
        sceVif1PkRef(&pk, data, qwc, 0, 0, 0);
        part += 2;
        data += qwc << 4;
    }
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.pBase, sceVif1PkSize(&pk));
}

/* NON_MATCHING: 4 lines: the original keeps the li.s load of 1 / 32767 before the last jal (nop in the slot), gas moves it into the slot */
#if 0
/* The model renderer (from Model_Draw): kinds 0-5 send the object's matrices first; kinds 0 and 4 also set the
 * fog / depth factor of func_00342FA8 for the fight (from the nearer fighter's depth) before and 1 / 32767 after. */
void Model_Render(VifModel *m, Vu0Matrix *a, ModelRef *o, u32 kind) {
    f32 x0;
    f32 x1;
    f32 d;

    switch (kind) {
    case 0:
    case 4:
        if (gMode == 4) {
            if (func_0036E158() != 0 && D_005F5CB6 != 12) {
                x0 = gFighters[0].unk620 - 500.0f;
                x1 = gFighters[1].unk620 - 500.0f;
                if (x0 < x1) {
                    d = x0;
                } else {
                    d = x1;
                }
                if (0.0f < d) {
                    func_00342FA8((d + d) / (D_00433440[10] * d + D_00433440[14]));
                }
            } else if (gMode == 4 && D_0043B7F8[0] == 8 && func_003E9400() != 0) {
                func_00342FA8(14000.0f / (D_00433440[10] * 7000.0f + D_00433440[14]));
            }
        }
        Model_SendMatrices((Vu0Matrix *)o, a);
        func_00342FA8(1.0f / 32767.0f);
        break;
    case 1:
    case 2:
    case 3:
    case 5:
        Model_SendMatrices((Vu0Matrix *)o, a);
        break;
    }
    Model_SendParts(m);
}
#else
INCLUDE_ASM("asm/nonmatchings/render_fighter_b", Model_Render);
#endif

void func_00341F68(Vu0Matrix a, Vu0Matrix b) {
    Vu0Matrix m0;
    Vu0Matrix m1;
    Vu0Matrix m2;
    Vu0Matrix light[2];
    Vu0Matrix m3;
    Vu0Matrix rot;
    f32 ang;

    func_00356840();
    sceVu0MulMatrix(m0, a, b);
    func_00366EA8(m1);
    func_00353D68(light);
    ang = func_00366EC8();
    func_00366EB8(m3);
    func_00356710(rot);
    sceVu0RotMatrixY(rot, rot, ang);
    sceVu0MulMatrix(m2, D_00433400, rot);
    light[0][0][3] = sinf(0.0f);
    light[0][1][3] = cosf(0.0f);
    m2[3][0] = (f32)D_00415808[5] - m3[3][0];
    m2[3][1] = -(f32)D_00415808[6] - m3[3][1];
    m2[3][2] = -(f32)D_00415808[7] - m3[3][2];
    func_00356710(rot);
    sceVu0RotMatrixY(rot, rot, -ang);
    sceVu0ApplyMatrix(m2[3], rot, m2[3]);
    Model_Render(func_00367898(), light, (ModelRef *)m1, 0);
}
