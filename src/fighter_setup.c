/* 0x3621F8-0x3639F0 (module fighter_setup, docs/game_overview.md section 4). */
#include "common.h"
#include "fighter.h"
#include "gte.h"

/* A joint record (0x50 bytes): its rotation matrix first. */
typedef struct {
    /* 0x00 */ s16 m[3][3];
    /* 0x12 */ u8 pad12[0x3E];
} FsJoint;

/* Fighter fields 0xB050-0xB134 as this file reads them (an overlay with the full slot size, as FighterSkel in
 * src/skeleton.c). */
typedef struct {
    /* 0x0000 */ u8 pad0[0x634];
    /* 0x0634 */ FsJoint unk634[24];
    /* 0x0DB4 */ u8 padDB4[0xA29E];
    /* 0xB052 */ s16 unkB052;
    /* 0xB054 */ s16 unkB054;
    /* 0xB056 */ s16 unkB056;
    /* 0xB058 */ s16 unkB058;
    /* 0xB05A */ s16 unkB05A[2];
    /* 0xB05E */ s16 unkB05E[2];
    /* 0xB062 */ s16 unkB062[2];
    /* 0xB066 */ s16 unkB066;
    /* 0xB068 */ s16 unkB068;
    /* 0xB06A */ u8 padB06A[0x2];
    /* 0xB06C */ s32 unkB06C[2];
    /* 0xB074 */ s32 unkB074[2];
    /* 0xB07C */ f32 unkB07C[2];
    /* 0xB084 */ s32 unkB084;
    /* 0xB088 */ s32 unkB088;
    /* 0xB08C */ f32 unkB08C;
    /* 0xB090 */ f32 unkB090;
    /* 0xB094 */ u8 padB094[0x8C];
    /* 0xB120 */ s16 unkB120;
    /* 0xB122 */ s16 unkB122;
    /* 0xB124 */ s16 unkB124;
    /* 0xB126 */ s16 unkB126;
    /* 0xB128 */ u8 padB128[0x8];
    /* 0xB130 */ u16 unkB130;
    /* 0xB132 */ u8 padB132[0x76E];
} FighterFs; /* size 0xB8A0 */

#define FS(f) ((FighterFs *)(f))

/* A scratchpad upload block (0x10 bytes, D_006E3500[5]): qword count and source address. */
typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ u32 unk4;
    /* 0x8 */ u64 unk8;
} SprBlock;

typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u16 unk6;
} SprBlockState;

/* A GS image record's transfer header (BITBLTBUF, TRXPOS, TRXREG, TRXDIR as A+D pairs) and its data. */
typedef struct {
    /* 0x00 */ u8 pad0[0x10];
    /* 0x10 */ u64 unk10;                     /* BITBLTBUF */
    /* 0x18 */ u64 unk18[7];
    /* 0x50 */ u32 unk50;                     /* qword count, bit 15: last block */
    /* 0x54 */ u8 pad54[0xC];
    /* 0x60 */ u128 unk60[0];
} FsImgSrc;

/* One block of an image upload chain (0x80 bytes): DMA cnt tag, VIF1 DIRECT 6, GIF tag A+D, the header, the image GIF
 * tag, then a DMA ref tag to the data and VIF1 DIRECT n. */
typedef struct {
    /* 0x00 */ u16 qwc;
    /* 0x02 */ u16 id;
    /* 0x04 */ u32 addr;
    /* 0x08 */ u32 vif0;
    /* 0x0C */ u32 vif1;
    /* 0x10 */ u64 giftag[2];
    /* 0x20 */ u64 ad[8];
    /* 0x60 */ u64 imgtag[2];
    /* 0x70 */ u16 qwc2;
    /* 0x72 */ u16 id2;
    /* 0x74 */ void *addr2;
    /* 0x78 */ u32 vif2;
    /* 0x7C */ u32 vif3;
} FsImgPk;

extern SprBlock D_006E3500[];
extern SprBlockState D_006E3550[];
extern s32 D_005F6B48;

extern s32 D_005F5E04;
extern s32 D_005F6B70;
typedef struct {
    f32 x;
    f32 y;
    f32 z;
} FsV3;
extern s32 D_004161D8[];
extern FsV3 D_004161E0[][7];
extern FsV3 D_00416288[][7];
extern FsV3 D_006E3578[];
extern u8 *D_005F5E08;
extern u8 *D_005F5E0C;
extern u8 *D_005F5E10;

extern u8 *D_005F5E14;
extern u8 *D_005F5E18;
extern u8 *D_005F5E1C;
extern u8 *D_005F5E20;
extern u8 *D_005F5E24;
extern u8 *D_005F5E28;
extern u8 *D_005F5E2C;
extern u8 D_006E3600[];
extern u8 D_0085A600[];

u32 Spr_Setup(u32 count, s32 reserved);
void func_00343128(u32 spr, u32 src, u32 qwc);
u32 Dma_Wait(volatile u32 *chan);
u32 func_00342FB0(void);
void func_00343178(u32 spr, u32 qwc);
s32 Tag_StepCounters(s32 side, s32 n);
s32 rand(void);
s32 abs(s32 x);
f32 sinf(f32);
f32 cosf(f32);
void func_00351A60(s32 side, s32 kind, s32 n);
s32 func_003664E0(Fighter *f, s32 i);
s32 func_00366518(Fighter *f);
s32 func_00366540(Fighter *f);
s32 func_00366568(Fighter *f);
s32 func_00366590(Fighter *f);
s32 func_003665B8(Fighter *f);
s32 func_003AF358(void);
s32 func_003A9698(void);
u8 func_003A9520(void);
s32 func_003E94D0(void);
s32 func_003E9500(void);
s32 func_00365520(Fighter *f, u8 code);
void func_00365610(Fighter *f, s32 a, s32 b);
extern s16 gMode;
extern u8 D_004161A0[];
extern u8 D_004161B8[];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
void func_00346398(void);
s32 func_00346430(void);
s32 func_003986F0(void);
s32 func_003986F8(void);
s32 func_00345FB8(void);
void func_00399488(void);
void func_003994A8(void);

void func_003621F8(s32 i);
void func_003622F8(void);
void func_00362318(void);
void func_00362328(void);
void func_00362378(u128 **dst, u128 *src, s32 n);
s32 func_003623B8(void *data, s32 vram, u8 **pk, s32 copy, u8 **end);
void func_003625B0(s32 angle, f32 (*m)[4]);
void func_003626B0(void);
s32 func_003626B8(Fighter *f, s32 j, void **m);
void Skel_GetPoseAngles(s32 j, GteSVector *r);
void func_003986D8(s16 *v, void *m);
s32 func_003659B0(Fighter *f, s32 v);
extern const s16 gSinTable[];
extern s32 D_005F6B50;
extern s32 D_005F6B54;
extern s32 D_005F6B58;
extern s16 D_005F6B60;
extern s16 D_005F6B62;
extern s16 D_005F6B64;
extern GteSVector D_005F6B68;
void func_00362B18(Fighter *f);
void func_00362B38(Fighter *f, s32 a, s32 b, s32 c);
void func_00362C28(Fighter *f);
void func_00362E90(Fighter *f, s32 mask, s32 kind, s32 n);
void func_00363068(Fighter *f, s32 n);
void func_003630D0(Fighter *f, s32 n);
void func_00363110(Fighter *f, s32 n);
void func_00363150(Fighter *f);
void func_00363270(s16 *p, s16 *step, s16 target);
void func_003632E0(Fighter *f);
void func_00363618(void);
void func_00363628(void);
void func_00363630(s32 i);
void func_003637B0(void);
s32 func_003637D0(void);
s32 func_00363888(void);
s32 func_003638F0(void);

/* Uploads the blocks of D_006E3500[i] to the scratchpad and on, in pieces of what the scratchpad holds. */
void func_003621F8(s32 i) {
    SprBlock *b;
    u32 max;
    u32 n;

    if ((&D_006E3550[i])->unk4 != 0 && D_005F6B48 != i) {
        D_005F6B48 = i;
        b = (SprBlock *)Spr_Setup(2, 0x10);
        *b = D_006E3500[i];
        max = *(u16 *)0x7000000C >> 4;
        while (b->unk0 != 0) {
            n = b->unk0;
            if (max < n) {
                n = max;
            }
            func_00343128(*(u32 *)0x70000004, b->unk4, n);
            Dma_Wait((volatile u32 *)0x1000D400);
            b->unk0 -= n;
            b->unk4 += n * 16;
            func_00343178(func_00342FB0(), n);
        }
    }
}

void func_003622F8(void) {
    if (D_006E3550[0].unk4 != 0) {
        D_005F6B48 = 5;
    }
}

void func_00362318(void) {
    D_005F6B48 = 5;
}

void func_00362328(void) {
    s32 i;

    SprBlockState *s;
    SprBlock *b;

    for (i = 4; i >= 0; i--) {
        s = &D_006E3550[i];
        b = &D_006E3500[i];
        s->unk4 = 0;
        s->unk6 = 0;
        b->unk4 = 0;
        b->unk0 = 0;
    }
    D_005F6B48 = 5;
}

/* Copies n qwords to *dst and moves *dst to the next 128-byte boundary. */
void func_00362378(u128 **dst, u128 *src, s32 n) {
    u128 *d = *dst;

    while (n != 0) {
        n--;
        *d++ = *src++;
    }
    *dst = (u128 *)(((u32)d + 0x7F) & 0xFFFFFF80);
}

/* Copies the transfer header of a GS image record into a DMA / VIF1 / GIF upload chain (0x80 bytes per block of the
 * image, its DBP moved to `vram`), referencing the image data in place or, with `copy`, from a copy at *end. Returns
 * the first free 256-word VRAM block after the image. */
/* NON_MATCHING: 28 lines: register allocation (n / img / ref in s1-s3 swapped), the final rounding branch */
#if 0
s32 func_003623B8(void *data, s32 vram, u8 **pk, s32 copy, u8 **end) {
    FsImgSrc *src = data;
    FsImgPk *q = (FsImgPk *)*pk;
    u64 dbp;
    s32 n;
    s32 last;
    u128 *img;
    void *ref;
    u64 off;
    u32 v;

    off = (u32)(vram - (s32)(src->unk10 >> 32 & 0x3FFF));
    do {
        q->qwc = 6;
        q->vif1 = 0x51000006;
        q->id = 0x1000;
        q->vif0 = 0x11000000;
        q->giftag[0] = 0x1000000000000004;
        q->giftag[1] = 0xE;
        q->addr = 0;
        dbp = ((src->unk10 & 0x3FFF00000000) >> 32) + off;
        q->ad[0] = (src->unk10 & ~0x3FFF00000000) | (dbp << 32 & 0x3FFF00000000);
        q->ad[1] = src->unk18[0];
        q->ad[2] = src->unk18[1];
        q->ad[3] = src->unk18[2];
        q->ad[4] = src->unk18[3];
        q->ad[5] = src->unk18[4];
        q->ad[6] = src->unk18[5];
        q->ad[7] = src->unk18[6];
        n = src->unk50 & 0x7FFF;
        last = (src->unk50 & 0x8000) >> 15;
        img = src->unk60;
        if (copy != 0) {
            ref = *end;
            func_00362378((u128 **)end, img, n);
        } else {
            ref = img;
        }
        q->imgtag[0] = (n | 0x8000) | (0x8000ULL << 44);
        q->id2 = 0x3000;
        src = (FsImgSrc *)(img + n);
        q->addr2 = ref;
        q->vif3 = n | 0x51000000;
        q->imgtag[1] = 0;
        q->qwc2 = n;
        q->vif2 = 0;
        q++;
    } while (last == 0);
    v = (s32)(dbp << 40 >> 32) + n * 16;
    *pk = (u8 *)q;
    if ((v & 0xFF) == 0) {
        return v >> 8;
    }
    return (v + 0x100) >> 8;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup", func_003623B8);
#endif

#define ANGLE_RAD (6.2831855f / 65536.0f)
#define ANGLE_TO_RAD2(a) (((a) & 0xFFFF) <= 0x7FFF ? ANGLE_RAD * ((a) & 0xFFFF) : (((a) & 0xFFFF) - 0x10000) * ANGLE_RAD)

/* A rotation matrix about y. */
static inline f32 Fs_AngleToRad(u32 a) {
    return a <= 0x7FFF ? ANGLE_RAD * (s32)a : ((s32)a - 0x10000) * ANGLE_RAD;
}

void func_003625B0(s32 angle, f32 (*m)[4]) {
    u32 a = angle & 0xFFFF;
    f32 s = sinf(Fs_AngleToRad(a));
    f32 c;

    a = angle & 0xFFFF;
    c = cosf(Fs_AngleToRad(a));

    m[0][0] = c;
    m[1][0] = 0;
    m[2][0] = -s;
    m[3][0] = 0;
    m[0][1] = 0;
    m[1][1] = 1.0f;
    m[2][1] = 0;
    m[3][1] = 0;
    m[0][2] = s;
    m[1][2] = 0;
    m[2][2] = c;
    m[3][2] = 0;
    m[0][3] = 0;
    m[1][3] = 0;
    m[2][3] = 0;
    m[3][3] = 1.0f;
}

void func_003626B0(void) {
}

/* Joints 5, 6, 18 and 19 (bat / arm swing of one character [I]): rotates and scales the joint matrix by the swing
 * phase unkB120 and the blend factor. Returns 1 when the joint was changed; *m gets the joint matrix. */
/* NON_MATCHING: 86 lines: register allocation (the joint matrix and the pose height swapped in s4 / s5) and the branch layout of the three cases */
#if 0
s32 func_003626B8(Fighter *f, s32 j, void **m) {
    s32 k;
    s32 jj;
    s16 (*mat)[3];
    s32 y;
    s32 d;
    s32 a;
    s32 t;
    s32 sx;
    s32 sy;
    s32 i;

    switch (j) {
    case 5:
        k = 0;
        jj = 0x12;
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            FS(f)->unkB120++;
        }
        t = -*(s32 *)((u8 *)f + 0x84C) / 2;
        D_005F6B50 = (FS(f)->unkB120 << 5) & 0xFFF;
        D_005F6B54 = (FS(f)->unkB120 << 6) & 0xFFF;
        D_005F6B58 = t;
        if (t > 0x100) {
            D_005F6B58 = 0x100;
        } else if (t < 0) {
            D_005F6B58 = 0;
        }
        break;
    case 6:
        k = 1;
        jj = 5;
        break;
    case 0x12:
        k = 2;
        jj = 0x13;
        break;
    case 0x13:
        k = 3;
        jj = 6;
        break;
    default:
        k = -1;
        jj = j;
        break;
    }
    mat = (s16 (*)[3])((u8 *)f + jj * 0x50 + 0x634);
    if (k >= 0) {
        Skel_GetPoseAngles(5, &D_005F6B68);
        y = D_005F6B68.vz / 4;
        d = func_003659B0(f, FS(f)->unkB122);
        a = D_005F6B58 * (0x100 - d) / 256;
        if (j == 6) {
            D_005F6B60 = 0x4000;
            D_005F6B64 = gSinTable[(D_005F6B54 + (k << 10)) & 0xFFF] * a / 256 + y;
            D_005F6B62 = gSinTable[(D_005F6B50 + (k << 10)) & 0xFFF] / 2 * a / 256;
        } else if (j >= 0x12) {
            D_005F6B60 = 0;
            D_005F6B62 = gSinTable[(D_005F6B54 + (k << 10)) & 0xFFF] * a / 256 + y;
            D_005F6B64 = -(gSinTable[(D_005F6B50 + (k << 10)) & 0xFFF] / 2 * a / 256);
        } else {
            D_005F6B60 = 0;
            D_005F6B64 = gSinTable[(D_005F6B54 + (k << 10)) & 0xFFF] * a / 256 + y;
            D_005F6B62 = gSinTable[(D_005F6B50 + (k << 10)) & 0xFFF] / 2 * a / 256;
        }
        func_003986D8(&D_005F6B60, mat);
        sy = d / 8 + 0x100;
        sx = d / 4 + 0x100;
        if (j == 0x13) {
            sx = 0x200 - sx;
        }
        for (i = 0; i < 3; i++) {
            mat[i][0] = mat[i][0] * sx / 256;
            mat[i][1] = mat[i][1] * sy / 256;
            mat[i][2] = mat[i][2] * sy / 256;
        }
    }
    *m = mat;
    return k >= 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup", func_003626B8);
#endif

void func_00362B18(Fighter *f) {
    FS(f)->unkB088 = 0;
    FS(f)->unkB084 = 0;
    FS(f)->unkB068 = 0;
    FS(f)->unkB066 = 0;
}

void func_00362B38(Fighter *f, s32 a, s32 b, s32 c) {
    if (Tag_StepCounters((s16)f->unk20, 0) & 0xF) {
        FS(f)->unkB068 = 0;
        FS(f)->unkB066 = 0;
    } else {
        if (a == 0) {
            FS(f)->unkB068 = (rand() & 0xFE) + 1;
        } else {
            FS(f)->unkB068 = c;
        }
        if (FS(f)->unkB066 != a) {
            FS(f)->unkB090 = 0;
            FS(f)->unkB066 = a;
            FS(f)->unkB084 = a;
            FS(f)->unkB088 = 0;
            FS(f)->unkB08C = 1.0f / (abs(b) + 1);
        }
    }
}

void func_00362C28(Fighter *f) {
    struct FighterMove *m;

    if (func_003AF358() != 0) {
        return;
    }
    if (D_005F5EE0 != 0) {
        return;
    }
    if (D_005F5EE4 != 0) {
        return;
    }
    if (func_003A9698() != 0) {
        return;
    }
    if (func_003A9520() != 0 && func_003E94D0() != 0) {
        return;
    }
    m = f->unkC0;
    if (m->unk24 & 0x800) {
        func_00362B38(f, func_00366590(f), 4, -1);
    } else if ((m->unk4 & 4) && !((m->unk24 >> 22) & 1)) {
        func_00362B38(f, func_003665B8(f), 8, -1);
    }
    if (FS(f)->unkB066 != 0) {
        if (((s16)func_00366590(f) == FS(f)->unkB066 && !(f->unkC0->unk24 & 0x800)) ||
            ((s16)func_003665B8(f) == FS(f)->unkB066 && !(f->unkC0->unk4 & 4))) {
            func_00362B38(f, 0, 1, 0);
        }
    }
    if (FS(f)->unkB068 == 0) {
        if (FS(f)->unkB066 == 0) {
            func_00362B38(f, func_00366568(f), 1, 3);
        } else if ((s16)func_00366568(f) == FS(f)->unkB066) {
            func_00362B38(f, 0, 1, 0);
        } else {
            func_00362B38(f, func_00366568(f), 0x1E, 0x1E);
        }
    }
    FS(f)->unkB068--;
    if (FS(f)->unkB090 >= 1.0f) {
        FS(f)->unkB090 = 0;
        FS(f)->unkB08C = 0;
    }
    if (FS(f)->unkB08C > 0.0f) {
        FS(f)->unkB090 += FS(f)->unkB08C;
        if (FS(f)->unkB090 > 1.0f) {
            FS(f)->unkB090 = 1.0f;
        }
    }
}

void func_00362E90(Fighter *f, s32 mask, s32 kind, s32 n) {
    s32 d;
    s32 t;
    s32 i;
    s32 m;

    if (kind >= 10) {
        func_00362B38(f, kind - 10, 0x14, n);
        return;
    }
    if (FS(f)->unkB130 & 1) {
        if (n == 0) {
            n = 1;
        }
        d = 0x100 / (abs(n) + 1);
        FS(f)->unkB126 = 0;
        if (kind == 2) {
            t = 0x100;
            FS(f)->unkB124 = 0x100;
            if (FS(f)->unkB122 < 0x100) {
                FS(f)->unkB126 = d;
            }
            func_00351A60(f->unk1E, 1, n);
        } else if (kind == 3) {
            t = 0;
            FS(f)->unkB124 = 0x100;
            if (FS(f)->unkB122 < 0x100) {
                FS(f)->unkB126 = d;
            }
            func_00351A60(f->unk1E, 2, n);
        } else {
            t = 0;
            FS(f)->unkB124 = 0;
            if (FS(f)->unkB122 > 0) {
                FS(f)->unkB126 = d;
            }
            func_00351A60(f->unk1E, 0, n);
        }
        FS(f)->unkB05E[0] = t;
        FS(f)->unkB062[0] = d;
        return;
    }
    m = abs(n) + 1;
    for (i = 0; i < 2; i++) {
        if ((i + 1) & mask) {
            t = 0;
            if (kind != 0) {
                if (kind == 1) {
                    t = 0x10;
                } else {
                    t = kind + 0xF;
                }
            }
            FS(f)->unkB062[i] = 10000 / m;
            FS(f)->unkB06C[i] = t;
            FS(f)->unkB07C[i] = 0;
        }
    }
}

void func_00363068(Fighter *f, s32 n) {
    func_00362E90(f, 1, func_003664E0(f, 1), n);
    func_00362E90(f, 2, func_003664E0(f, 2), n);
}

void func_003630D0(Fighter *f, s32 n) {
    func_00362E90(f, 3, func_00366540(f), n);
}

void func_00363110(Fighter *f, s32 n) {
    func_00362E90(f, 3, func_00366518(f), n);
}

void func_00363150(Fighter *f) {
    s32 i;
    s32 v;

    for (i = 0; i < 2; i++) {
        v = func_003664E0(f, i + 1);
        FS(f)->unkB05A[i] = FS(f)->unkB05E[i] = v;
        FS(f)->unkB074[i] = 0;
        FS(f)->unkB062[i] = 0;
        FS(f)->unkB06C[i] = func_003664E0(f, i + 1);
        FS(f)->unkB07C[i] = 0;
    }
    FS(f)->unkB120 = 0;
    FS(f)->unkB052 = 0;
    FS(f)->unkB054 = 0;
    FS(f)->unkB056 = 0;
    FS(f)->unkB058 = 0;
    FS(f)->unkB122 = 0;
    FS(f)->unkB124 = 0;
    FS(f)->unkB126 = 0;
    func_003630D0(f, 10);
}

/* Steps *p by *step towards target; at the target the step is cleared. */
void func_00363270(s16 *p, s16 *step, s16 target) {
    if (*p < target) {
        *p += *step;
        if (*p >= target) {
            *step = 0;
            *p = target;
        }
    } else if (target < *p) {
        *p -= *step;
        if (*p <= target) {
            *step = 0;
            *p = target;
        }
    }
}

/* NON_MATCHING: 14 lines: f + 0x8000 kept in s0 across the call in the original */
#if 0
void func_003632E0(Fighter *f) {
    s16 k;
    s32 i;
    s32 j;
    u8 *tbl;

    if (D_005F5EE0 != 0) {
        return;
    }
    if (D_005F5EE4 != 0) {
        return;
    }
    if (func_003A9698() != 0) {
        return;
    }
    if (FS(f)->unkB130 & 2) {
        k = FS(f)->unkB052;
        if (k != 3) {
            switch (FS(f)->unkB056) {
            case 0:
                if (k != 0) {
                    if (D_004161A0[++FS(f)->unkB054] == 0xFF) {
                        FS(f)->unkB054 = 0;
                        FS(f)->unkB056 = 1;
                    }
                } else if (FS(f)->unkB054 != 0) {
                    FS(f)->unkB054--;
                }
                break;
            case 1:
                if (k == 2) {
                    if (FS(f)->unkB054 != 0) {
                        FS(f)->unkB054++;
                    }
                    if (D_004161B8[FS(f)->unkB054] == 0xFF) {
                        FS(f)->unkB054 = 0;
                    }
                } else if (k == FS(f)->unkB056) {
                    if (D_004161B8[++FS(f)->unkB054] == 0xFF) {
                        FS(f)->unkB054 = 0;
                    }
                } else {
                    if (D_004161B8[++FS(f)->unkB054] == 0xFF) {
                        FS(f)->unkB056 = 0;
                        FS(f)->unkB054 = 0x10;
                    }
                }
                break;
            }
        }
        if (gMode != 5) {
            if (FS(f)->unkB056 != 0) {
                j = FS(f)->unkB054;
                tbl = D_004161B8;
            } else {
                j = FS(f)->unkB054;
                tbl = D_004161A0;
            }
            FS(f)->unkB058 = func_00365520(f, tbl[j]);
        } else {
            FS(f)->unkB058 = 0;
        }
    }
    if (func_003A9520() != 0 && func_003E9500() != 0) {
        FS(f)->unkB07C[1] = 1.0f;
        FS(f)->unkB07C[0] = 1.0f;
        return;
    }
    if (FS(f)->unkB130 & 1) {
        func_00363270(&FS(f)->unkB05A[0], &FS(f)->unkB062[0], FS(f)->unkB05E[0]);
        func_00363270(&FS(f)->unkB122, &FS(f)->unkB126, FS(f)->unkB124);
        func_00365610(f, (s16)FS(f)->unkB05A[0] >> 4, 0);
    } else {
        for (i = 0; i < 2; i++) {
            if (FS(f)->unkB07C[i] >= 1.0f) {
                FS(f)->unkB07C[i] = 0;
                FS(f)->unkB062[i] = 0;
            }
            if (FS(f)->unkB062[i] > 0) {
                FS(f)->unkB07C[i] += FS(f)->unkB062[i] * 0.0001f;
                if (FS(f)->unkB07C[i] > 1.0f) {
                    FS(f)->unkB07C[i] = 1.0f;
                }
            }
        }
    }
}
#else
LIT4_WORD(D_005F5110, 0x38D1B717);
INCLUDE_ASM("asm/nonmatchings/fighter_setup", func_003632E0);
#endif

void func_00363618(void) {
    D_005F5E04 = 0;
    D_005F6B70 = 0;
}

void func_00363628(void) {
}

/* NON_MATCHING: 27 lines: the dead load of D_004161D8[D_005F6B70] and the D_006E3578 base kept in s4 */
#if 0
void func_00363630(s32 i) {
    s32 n = D_005F5E04;
    s32 j;
    f32 b;
    f32 a;

    if (n != 0) {
        ((volatile s32 *)D_004161D8)[D_005F6B70]; /* fake */
        b = 1.0f;
        a = 0.0f;
        for (j = 0; j < 7; j++) {
            D_006E3578[j].x = D_00416288[i][j].x * a + D_004161E0[i][j].x * b;
            D_006E3578[j].y = D_00416288[i][j].y * a + D_004161E0[i][j].y * b;
            D_006E3578[j].z = D_00416288[i][j].z * a + D_004161E0[i][j].z * b;
        }
        D_005F5E0C = (u8 *)&D_006E3578[3];
        D_005F5E10 = (u8 *)&D_006E3578[6];
        D_005F5E04--;
        D_005F5E08 = (u8 *)D_006E3578;
    } else {
        D_005F5E0C = (u8 *)&D_00416288[i][3];
        D_005F5E08 = (u8 *)D_00416288[i];
        D_005F5E10 = (u8 *)&D_00416288[i][6];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup", func_00363630);
#endif

void func_003637B0(void) {
    D_005F5E14 = 0;
    D_005F5E18 = 0;
    D_005F5E1C = 0;
    D_005F5E20 = 0;
    D_005F5E24 = 0;
    D_005F5E28 = 0;
    D_005F5E2C = 0;
}

s32 func_003637D0(void) {
    u8 *old = D_005F5E14;
    u8 *p;
    u8 *end;
    s32 left;

    if (D_005F5E20 != 0) {
        func_00346398();
    }
    D_005F5E14 = D_006E3600;
    D_005F5E18 = D_005F5E14 + ((func_003986F0() + 0xF) & ~0xF);
    p = D_005F5E18 + ((func_003986F8() + 0xF) & ~0xF);
    end = p + 0x19F10;
    D_005F5E1C = p;
    D_005F5E20 = 0;
    D_005F5E24 = 0;
    D_005F5E28 = end;
    D_005F5E2C = 0;
    left = D_0085A600 - end;
    if (old != D_005F5E14) {
        func_00399488();
    }
    return left;
}

s32 func_00363888(void) {
    if (D_005F5E20 != 0) {
        func_00346398();
    }
    if (D_005F5E14 != 0) {
        func_003994A8();
    }
    D_005F5E14 = 0;
    D_005F5E2C = D_006E3600;
    D_005F5E18 = 0;
    D_005F5E1C = 0;
    D_005F5E20 = 0;
    D_005F5E24 = 0;
    D_005F5E28 = 0;
    return 0x177000;
}

s32 func_003638F0(void) {
    u8 *old = D_005F5E14;
    u8 *old20 = D_005F5E20;

    D_005F5E14 = D_006E3600;
    D_005F5E18 = D_005F5E14 + ((func_003986F0() + 0xF) & ~0xF);
    D_005F5E1C = D_005F5E18 + ((func_003986F8() + 0xF) & ~0xF);
    D_005F5E20 = D_005F5E1C + 0x19F10;
    D_005F5E24 = D_005F5E20 + ((func_00345FB8() + 0xF) & ~0xF);
    D_005F5E28 = 0;
    D_005F5E2C = 0;
    if (old != D_005F5E14) {
        func_00399488();
    }
    if (old20 != D_005F5E20) {
        return func_00346430();
    }
}

u8 *func_003639B8(void) {
    return D_005F5E14;
}

u8 *func_003639C0(void) {
    return D_005F5E18;
}

u8 *func_003639C8(void) {
    return D_005F5E1C;
}

u8 *func_003639D0(void) {
    return D_005F5E20;
}

u8 *func_003639D8(void) {
    return D_005F5E24;
}

u8 *func_003639E0(void) {
    return D_005F5E28;
}

u8 *func_003639E8(void) {
    return D_005F5E2C;
}
