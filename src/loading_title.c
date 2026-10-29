/* 0x3A9678-0x3ADFA8 (module loading_title, docs/game_overview.md section 4; docs/systems/modes.md). .rodata
 * 0x519B30-0x51DE30 is its own (subsegment, every string and table included in place); the start is an object edge
 * [V: its .rodata follows effects_draw's], the end is [I: the data runs of the four modules up to modes are in module
 * order]. */
#include "common.h"

#include "fighter.h"

extern Fighter gFighters[4];
extern u8 D_0043B7F8[];
extern s16 gMode;
extern s16 gSubMode;
extern u16 gFrameCount;
extern s32 D_005F625C;
extern s16 D_005F6D32;
extern s16 D_005F6D24;
extern s32 D_005F6D3C;
extern s16 D_005F629A;
extern u8 D_005F6D7C;

/* A credits line (12 bytes). */
typedef struct {
    /* 0x0 */ s16 kind;
    /* 0x2 */ s16 pad2;
    /* 0x4 */ s32 value;
    /* 0x8 */ s16 unk8;
    /* 0xA */ s16 unkA;
} LtLine;

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ const LtLine *unk10;
} LtTextState;

extern LtTextState D_008D7DE0;
extern u8 D_005F6D68[4];
extern u8 D_005F6D78[2][2];
extern s32 D_005F6274;
extern s16 D_005F6268;
extern s16 D_005F626A;
extern const s8 D_0051B678[];
extern u8 *D_00415840[];

void Mode_EnterLoading(s16 next);
void func_00363150(Fighter *f);

/* An effect object (dma_gif's pool): a frame counter at +0x1C. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ void *unk10;
    /* 0x14 */ u16 unk14;
    /* 0x16 */ u16 unk16;
    /* 0x18 */ u8 pad18[2];
    /* 0x1A */ s16 unk1A;
    /* 0x1C */ u8 pad1C[0x24];
    /* 0x40 */ s32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ u8 unk50[4];
    /* 0x54 */ f32 unk54;
    /* 0x58 */ f32 unk58;
    /* 0x5C */ s32 unk5C;
    /* 0x60 */ f32 pos[3];
    /* 0x6C */ u8 pad6C[8];
    /* 0x74 */ f32 unk74;
    /* 0x78 */ u8 pad78[8];
    /* 0x80 */ f32 unk80;
    /* 0x84 */ f32 unk84;
    /* 0x88 */ f32 unk88;
    /* 0x8C */ u8 pad8C[4];
    /* 0x90 */ s32 unk90;
    /* 0x94 */ s16 unk94;
    /* 0x96 */ s16 unk96;
    /* 0x98 */ u8 pad98[0x88];
    /* 0x120 */ f32 unk120;
} LtDraw;

/* The first three words of LtDraw seen as a float position (func_003ACF50). */
typedef struct {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
} LtPos;

typedef struct {
    /* 0x00 */ u8 pad0[0x14];
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s16 unk18;
    /* 0x1A */ s16 unk1A;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ LtDraw d;
} LtFx;

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
} LtSlot;

extern LtSlot D_008D78E0[0x40];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s16 D_005F5CBC;
extern s16 D_005F6D3A;
extern s16 D_005F6D56;
extern s16 D_005F6D48;
extern const LtLine D_0051B880[];
extern const char D_0051DBE8[];
extern const char D_0051DC08[];
extern const char D_0051DC28[];
extern const char D_0051DC48[];
s32 Text_Printf(const char *fmt, ...);
s32 func_003AA5E8(void);
void func_003465C8(LtFx *e);
int rand(void);
float sinf(float);
float cosf(float);
f32 Fx_RandFloat(void);
void func_003A87E8(void);
void Bgm_Stop(void);
void func_003ABDC8(void);
void func_003AC9D8(u32 n);
void func_003AA5F0(void);
s32 func_003AA8A8(void);
void func_003A96B0(s32 n);
void Mode_Change(s32 mode);
s32 func_003ADA08(u8 *set, s32 i);
extern u8 D_0043B968[];
extern u8 D_005F5CBE;
extern s32 D_005F5CC0;
extern const char D_0051DC70[];
extern TeamMember D_00440940[2][2];
extern u16 D_005F5CB0[2];
extern s32 D_005F5C40[2];
extern s32 D_00415718[];
extern s32 D_005F5F10;
extern s32 D_005F6260;
extern s16 D_005F6D20;
extern u8 D_00437CE0[][0x18];
extern u8 *D_005F6258;
extern s32 D_005F67BC;
extern s32 D_005F5CD0;
void func_003ADB70(s32 side, s32 member, s32 n);
s32 func_003ADBE8(s32 a, s32 b, s32 *out);
s32 func_003AFFB8(s32 i, s32 a);
s32 func_003BFDC8(void);
void func_00353D58(void);
void func_00353DB0(void);
void func_00353658(void);
void func_00353E08(void);
void func_003533E8(s32 a);
void func_003C1C90(void);
void func_003B8D60(void);
void CamScript_Load(s32 n);
void func_00393708(s32 a);
void Motion_LoadBanks(s32 src);
void func_00345678(void);
void func_003A9688(s32 a);
extern s32 D_005F6730;
extern u64 D_005F6738;
extern u64 D_005F6740;
extern s32 D_005F6D58;
typedef struct {
    u8 b[0x48];
} LtQuad;

extern LtQuad D_0043FD10[];
extern u8 D_00437D10[][0x10];
void func_003BB4E8(void *q, u16 *alpha, s32 unused);
void func_003CE5D8(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, s32 tex);
void func_003CF850(s32 cx, s32 cy, s32 r);
extern u8 D_00884EE0[];
extern f32 D_008D7E00[4][4];
extern u16 D_005F5CB6;
void Mtx_Unit(f32 (*m)[4]);
void sceVu0RotMatrix(f32 (*m0)[4], f32 (*m1)[4], f32 *rot);
void func_003D10D0(s32 id, void *m, f32 s);

/* Per side of the ending scene (D_008D78B0, 2 x 0xC). */
typedef struct {
    /* 0x0 */ s16 state;
    /* 0x2 */ s16 pad2[2];
    /* 0x6 */ s16 clip;
    /* 0x8 */ s16 len;
    /* 0xA */ s16 frame;
} LtSide;

extern LtSide D_008D78B0[2];
extern s16 D_005F6D30;
extern s32 D_005F6D28;
extern s32 D_005F6D2C;
extern s32 D_00415808[];
extern u8 D_00436530[];
s32 func_00392F80(s32 n, s32 frame);
void func_0035B4B0(void *a, void *b);
void func_003532A0(void);
void func_00341218(void);
void func_003D1040(void);
void Wind_Update(void);
void func_00363630(s32 a);
void func_00364080(s32 i);
void func_0034E8B0(Fighter *f);
void func_00366ED0(void);
void func_003DB690(void);
void func_003ADE18(void);
void func_003AC7C0(LtFx *e);
void func_003AC5D0(LtFx *e);
void func_003ACB08(LtFx *e);
void func_003A9898(void);
void func_003AA640(void);
extern LtSide D_008D78C8[2];
extern s16 D_005F6D54;
extern s32 D_005F6D4C;
extern s32 D_005F6D50;
extern s16 D_005F6D40;
s32 func_003AA460(void);
void func_00378398(Fighter *f);
void func_00378600(Fighter *f);

LtFx *func_00345780(void (*fn)(LtFx *e));
void func_003CFAD8(s32 a, s32 b, s32 c);
s32 func_003B87E0(s32 id);
void func_0035D2E0(s32 a);
void func_0035D300(void);
void Iop_SetEeStreamBase(s32 a);
void Bgm_Play(s32 n);
void func_003A6440(Fighter *f);
s32 func_003AE3C0(Fighter *f);
void func_003ACF50(LtFx *e);
void func_003ACCD0(LtFx *e);
void func_003A96B8(LtFx *e);
extern s16 D_005F6D5C;
extern s16 D_005F6D5E;
extern s32 D_005F6D60;
extern s16 D_005F6D64;
extern s16 D_005F6D38;
extern s16 D_005F6278;
extern u16 D_005F626E;
extern u16 D_005F626C;
extern u8 D_005F6D70[2];
s32 func_003ABCD0(void);
void func_003ADAA0(Fighter *f);
extern s16 D_005F629C;
extern s32 D_005F62A0;
extern const char D_0051DD48[];
extern const char D_0051DD60[];
void func_003753A8(void);
s32 func_00365D10(s32 idx, s32 *out);
void func_003992C8(s32 value);
u32 func_003712C0(void);
void Snd_PlaySe(s32 id, s32 player);
s32 Battle_UpdateFrame(void);
s32 func_00364EE8(void);
void func_00366090(void);
LtFx *func_003464F8(void (*fn)(LtFx *e));
void func_003ADFA8(void);
void func_003AD2C8(LtFx *e);
extern const char D_0051DCE0[];
extern const char D_0051DCF8[];
extern const char D_0051DD18[];
extern const char D_0051DD38[];

/* A record time entry (D_00440D50 + 0xD0, 8 bytes per course) [I]. */
typedef struct {
    /* 0x0 */ s32 time;
    /* 0x4 */ s32 unk4;
} LtRecord;

typedef struct {
    /* 0x00 */ u8 padD0[0xD0];
    /* 0xD0 */ LtRecord rec[8];
} LtRecords;

extern LtRecords D_00440D50;

/* The time display's view of LtDraw (func_003AD2C8). */
typedef struct {
    /* 0x00 */ s32 time;
    /* 0x04 */ s32 total;
    /* 0x08 */ s32 x;
    /* 0x0C */ s32 y;
    /* 0x10 */ s32 mode;
} LtTime;

void func_003A9678(s32 v) {
    D_005F625C = v;
}

s16 func_003A9680(void) {
    return D_005F6D32;
}

void func_003A9688(s32 a) {
    D_005F6D32 = a;
}

void func_003A9690(void) {
    Mode_EnterLoading(1);
}

s32 func_003A9698(void) {
    return D_005F6D24 & 1;
}

s32 func_003A96A8(void) {
    return D_005F6D24;
}

void func_003A96B0(s32 n) {
    D_005F6D24 = n;
}

void func_003A96B8(LtFx *e) {
    LtDraw *d = &e->d;
    u8 *f = (u8 *)gFighters;

    if (e->unk18 == 0) {
        e->unk14 |= 2;
        d->unk1A = 10;
        d->unk54 = 1.0f;
        d->unk40 = 1;
        d->unk44 = 64.0f;
        d->unk90 = 0x80;
        d->unk48 = 1200.0f;
        d->unk58 = 0;
        d->unk96 = 0;
        d->unk4C = 10;

        e->unk18++;
        return;
    }
    if (e->unk1A == 0) {
        d->pos[0] = *(f32 *)(f + 0x1E60) + 0.0f;
        d->pos[1] = *(f32 *)(f + 0x1E64) - 13.0f;
        d->pos[2] = *(f32 *)(f + 0x1E68) + 39.0f;
    } else {
        d->pos[0] = *(f32 *)(f + 0x1DA0) + 0.0f;
        d->pos[1] = *(f32 *)(f + 0x1DA4) - 13.0f;
        d->pos[2] = *(f32 *)(f + 0x1DA8) + 39.0f;
    }
}

void func_003A97B0(void) {
    D_005F5CBC = 1;
    D_005F5F10 = 1;
    D_005F6260 = 4;
    D_005F5EE0 = 0;
    D_005F5EE4 = 0;
    D_005F6D20 = 0;
    D_005F6D24 = 0;
    D_005F6D32 = 0;
    if (func_003BFDC8() == 0) {
        D_005F6258 = D_00437CE0[0];
    } else {
        D_005F6258 = D_00437CE0[D_005F625C & 1];
        if (D_005F625C & 1) {
            D_005F6260 = 0xD;
        }
    }
    D_005F67BC = 0;
    D_005F625C++;
    func_00353D58();
    func_00353DB0();
    func_00353658();
    func_00353E08();
    func_003533E8(0);
    func_003C1C90();
    func_003B8D60();
    CamScript_Load(0);
    func_00393708(D_005F5CD0);
    Motion_LoadBanks(1);
    func_00345678();
    func_003A9688(1);
}

/* NON_MATCHING: 34 lines, saved-register allocation of the side / fighter / flag locals (s0-s2 permuted) */
#if 0
void func_003A9898(void) {
    u32 flag;
    s32 i;

    if (D_005F6D30 == 0) {
        D_005F6D30 = func_00392F80(D_005F6D28, D_005F6D2C);
        if (!(D_005F6D24 & 1)) {
            D_005F6D2C++;
        }
    }
    func_0035B4B0(D_00415808, D_00436530);
    func_003532A0();
    func_00341218();
    func_003D1040();
    Wind_Update();
    func_00363630(D_005F6D28);
    for (i = 0; i < 2; i++) {
        Fighter *f = &gFighters[i];
        LtSide *s = &D_008D78B0[i];

        switch (s->state) {
        case 0:
            f->unk127 = 0;
            break;
        case 1:
            f->unk127 = 1;
            if (!(D_005F6D24 & 1)) {
                s->state = 2;
            }
            break;
        case 2:
            f->unk127 = 1;
            f->unkBC = f->unkC4 = s->frame;
            flag = f->unkC0->unk24;
            f->unkC0->unk24 = flag & ~0x400000;
            flag = (flag >> 22) & 1;
            func_00364080(s->clip);
            f->unkB074[0] = f->unkB074[1] = f->unkB088 = s->clip + 0x32;
            f->unkB06C[0] = f->unkB06C[1] = f->unkB084 = f->unkC4;
            func_0034E8B0(f);
            f->unkC0->unk24 = (f->unkC0->unk24 & ~0x400000) | (flag << 22);
            if (!(D_005F6D24 & 1)) {
                s->frame++;
            }
            if (s->frame > s->len) {
                s->state = 0;
            }
            break;
        }
    }
    func_00366ED0();
    func_003DB690();
    gFighters[0].unkC6 = gFighters[0].unkC4;
    gFighters[1].unkC6 = gFighters[1].unkC4;
}
#else
INCLUDE_ASM("asm/nonmatchings/loading_title", func_003A9898);
#endif

/* A command of the ending scene script (D_005F6D1C, 10 bytes; D_00519B90 is the first script) [V offsets]. */
typedef struct {
    /* 0x0 */ s16 frame;
    /* 0x2 */ s16 op;
    /* 0x4 */ s16 a;
    /* 0x6 */ s16 b;
    /* 0x8 */ s16 c;
} LtCmd;

/* The point handed to func_003A2BC0 (battle_mode's HitRec) [I]. */
typedef struct {
    /* 0x00 */ s32 pos[3];
    /* 0x0C */ s32 padC;
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s16 unk14;
    /* 0x16 */ u8 pad16[2];
} LtHit;

extern const LtCmd D_00519B90[];
extern const LtCmd *D_005F6D1C;
extern s16 D_005F6D22;
extern s32 D_005F5F20;
extern s32 D_005F5CDC;
extern s32 gRoundPhase;
void func_003AE1E8(void);
void func_0035C8D0(Fighter *f, s32 character, s32 costume);
void func_00367450(Fighter *f);
void func_003420C8(s32 n);
void func_00368168(Fighter *f);
s32 func_00367310(s16 slot);
void func_003AF540(Fighter *f, s32 a, s32 b);
void func_003AFDA8(s32 side, s32 slot, s32 b);
s16 func_00368288(s16 n);
void func_00367938(u16 n);
void func_003645D8(s32 n);
void func_00340070(u16 n);
void Wind_Init(s32 stage);
void func_00366DB0(u16 n);
void func_00363618(void);
void func_0038D508(void);
void func_00373380(Fighter *f);
void func_00373038(Fighter *a, Fighter *b);
void Cam_Reset(void);
s32 func_003963C0(u32 n, Fighter *a, Fighter *b);
void func_00346600(void);
void func_00344BE0(void);
void Fx_StartStageLayers(void);
void func_003A6BD0(void);
void Bgm_PlayFlag(s32 track);
s32 Bgm_IsPlaying(void);
void func_003A67D0(s32 n, u8 r, u8 g, u8 b);
void func_003A6770(s32 n, u8 r, u8 g, u8 b);
void func_003A68D0(s32 n, s32 r, s32 g, s32 b);
void func_003ADFB0(void);
void *func_00368218(Fighter *f, s16 move);
void func_003465E0(void);
void func_00353278(void);
void func_0034CB38(s32 slot);
void func_003A7FA8(u32 layer, u32 kind);
void func_00363628(s32 a);
void func_003A6BC0(void);
void func_003A2BC0(s16 side, s16 unused, LtHit *p, s32 d);
void func_003A8890(Fighter *f);
void func_003A88A0(Fighter *f);
s32 func_003BB308(void);
void func_003A7558(void);
void func_00344A28(Fighter *f);
void Fx_LoadFighterTex(Fighter *f);
s32 func_003A9AB8(void);

/* NON_MATCHING: 148 lines, saved/argument register allocation of the loop and table locals and store scheduling in the script ops */
#if 0
s32 func_003A9AB8(void) {
    LtSide *s0 = &D_008D78B0[0];
    LtSide *s1 = &D_008D78B0[1];
    Fighter *f0 = &gFighters[0];
    Fighter *f1 = &gFighters[1];
    LtSide *s;
    Fighter *f;
    s16 *tbl;
    LtHit hit;
    s32 done = 0;
    s32 i;
    s32 k;
    s32 m;
    s32 chara;
    s32 costume;
    void *mv;
    LtFx *e;

    D_005F5F20 = -1;
    switch (D_005F6D20) {
    case 0:
        D_005F5CB6 = D_005F6260;
        D_005F6D1C = D_00519B90;
        for (i = 0, s = D_008D78B0; i < 2; i++, s++) {
            s->state = 0;
            s->frame = 0;
            if (i != 0) {
                s->pad2[0] = 7;
            } else {
                s->pad2[0] = 9;
            }
            s->pad2[1] = 0;
            s->clip = 0;
        }
        D_00440940[0][0].unk0 = s0->pad2[0];
        D_00440940[0][0].unk4 = s0->pad2[1];
        D_00440940[1][0].unk0 = s1->pad2[0];
        D_00440940[1][0].unk4 = s1->pad2[1];
        func_003AE1E8();
        for (i = 0; i < 6; i++) {
            f = &gFighters[i & 1];
            tbl = (s16 *)D_005F6258 + i * 2;
            func_0035C8D0(f, tbl[0], tbl[1]);
            f->unk22 = i;
            func_00367450(f);
        }
        func_003420C8(6);
        for (i = 0; i < 2; i++) {
            f = &gFighters[i];
            func_0035C8D0(f, D_00440940[i][0].unk0, D_00440940[i][0].unk4);
            f->unk22 = i;
            func_00368168(f);
            m = func_00367310(f->unk22);
            func_003AF540(f, 0, m);
            func_003AFDA8(i, f->unk22, m);
        }
        f0->unk28 = func_00368288(f0->unk28);
        f1->unk28 = func_00368288(f1->unk28);
        func_00367938(D_005F5CB6);
        func_003645D8(0);
        func_003645D8(1);
        func_003645D8(2);
        func_00340070(D_005F5CB6);
        Wind_Init(D_005F5CB6);
        func_00366DB0(D_005F5CB6);
        func_00363618();
        gRoundPhase = 0;
        func_0038D508();
        f0->unk127 = f1->unk127 = 1;
        func_00373380(f0);
        func_00373380(f1);
        func_00373038(f0, f1);
        f1->unk4 = 0;
        f1->unk40 = -0x8000;
        f0->unk4 = 0;
        f1->unk8 = 0;
        f0->unk8 = 0;
        f1->unkC = 0;
        f0->unkC = 0;
        f1->unk18 = 0;
        f0->unk18 = 0;
        f1->unk1A = -0x8000;
        f0->unk1A = -0x8000;
        f1->unk1C = 0;
        f0->unk1C = 0;
        f0->unk3C = -0x8000;
        f0->unk40 = -0x8000;
        f1->unk3C = -0x8000;
        f0->unk51C = 0;
        f0->unk524 = 0;
        f1->unk51C = 0;
        f1->unk524 = 0;
        Cam_Reset();
        func_003963C0(0, f0, f1);
        func_00346600();
        func_00344BE0();
        Fx_StartStageLayers();
        func_003A6BD0();
        D_005F6D28 = 0;
        D_005F6D2C = 0;
        D_005F6D30 = 0;
        Bgm_PlayFlag(1);
        D_005F6D22 = 300;
        D_005F6D20 = 1;
        break;
    case 1:
        if (--D_005F6D22 >= 0 && Bgm_IsPlaying() == 0) {
            break;
        }
        func_003A6770(0x2C, 0xFF, 0xFF, 0xFF);
        D_005F6D22 = 0x2C;
        D_005F6D20 = 2;
        break;
    case 2:
        if (--D_005F6D22 > 0) {
            break;
        }
        func_003992C8(0);
        D_005F6D22 = 0;
        D_005F6D20 = 3;
        /* fallthrough */
    case 3:
        func_003ADFB0();
        while (D_005F6D22 >= D_005F6D1C->frame) {
            if (D_005F6D1C->op == -1) {
                done = 1;
                break;
            }
            switch (D_005F6D1C->op) {
            case 0:
            case 14:
                k = D_005F6D1C->a;
                if ((u16)k >= 6) {
                    k = 0;
                }
                tbl = (s16 *)D_005F6258 + k * 2;
                costume = tbl[1];
                chara = tbl[0];
                D_00440940[0][0].unk4 = costume;
                D_00440940[0][0].unk0 = chara;
                func_0035C8D0(f0, chara, costume);
                func_00368168(f0);
                f0->unk22 = k;
                m = func_00367310(gFighters[0].unk22);
                func_003AF540(&gFighters[0], 0, m);
                func_003AFDA8(0, gFighters[0].unk22, m);
                f0->unk28 = func_00368288(f0->unk28);
                if (D_005F6D1C->op != 14) {
                    s0->frame = 0;
                    s0->state = 0;
                }
                s0->pad2[0] = chara;
                s0->pad2[1] = costume;
                func_00344A28(f0);
                Fx_LoadFighterTex(f0);
                break;
            case 1:
                k = D_005F6D1C->a;
                if ((u16)k >= 6) {
                    k = 0;
                }
                tbl = (s16 *)D_005F6258 + k * 2;
                costume = tbl[1];
                chara = tbl[0];
                D_00440940[1][0].unk4 = costume;
                D_00440940[1][0].unk0 = chara;
                func_0035C8D0(f1, chara, costume);
                func_00368168(f1);
                f1->unk22 = k;
                m = func_00367310(gFighters[1].unk22);
                func_003AF540(&gFighters[1], 0, m);
                func_003AFDA8(1, gFighters[1].unk22, m);
                f1->unk28 = func_00368288(f1->unk28);
                s1->pad2[0] = chara;
                s1->pad2[1] = costume;
                s1->frame = 0;
                s1->state = 0;
                func_00344A28(f1);
                Fx_LoadFighterTex(f1);
                break;
            case 2:
                if (s0->clip != D_005F6D1C->a) {
                    mv = func_00368218(f0, D_005F6D1C->a);
                    f0->unkB8 = f0->unkC0 = mv;
                }
                s0->clip = D_005F6D1C->a;
                s0->frame = D_005F6D1C->b;
                s0->state = 2;
                s0->len = D_005F6D1C->c;
                break;
            case 3:
                if (s1->clip != D_005F6D1C->a) {
                    mv = func_00368218(f1, D_005F6D1C->a);
                    f1->unkB8 = f1->unkC0 = mv;
                }
                s1->clip = D_005F6D1C->a;
                s1->frame = D_005F6D1C->b;
                s1->state = 2;
                s1->len = D_005F6D1C->c;
                break;
            case 4:
                func_003465E0();
                func_00353278();
                D_005F6D28 = D_005F6D1C->a;
                D_005F6D2C = D_005F6D1C->b;
                func_0034CB38(0);
                func_0034CB38(1);
                break;
            case 5:
                func_003A67D0(D_005F6D1C->a, 0xFF, 0xFF, 0xFF);
                break;
            case 6:
                func_003A6770(D_005F6D1C->a, 0xFF, 0xFF, 0xFF);
                break;
            case 7:
                if (D_005F5CB6 == 4) {
                    func_003A7FA8(D_005F6D1C->a, D_005F6D1C->b);
                }
                break;
            case 8:
                func_00363628(D_005F6D1C->a);
                break;
            case 9:
                func_003A6BC0();
                break;
            case 10:
                f = &gFighters[D_005F6D1C->a & 1];
                hit.pos[0] = ((FighterJoints *)f)->unk668[D_005F6D1C->b].unk0.unk0[0];
                hit.pos[1] = ((FighterJoints *)f)->unk668[D_005F6D1C->b].unk0.unk0[1];
                hit.pos[2] = ((FighterJoints *)f)->unk668[D_005F6D1C->b].unk0.unk0[2];
                hit.unk14 = 0;
                hit.unk12 = 0;
                hit.unk10 = 0;
                func_003A2BC0(D_005F6D1C->a, 0, &hit, 0);
                break;
            case 11:
                func_003A68D0(D_005F6D1C->a, 0xFF, 0xFF, 0xFF);
                break;
            case 12:
                e = func_00345780(func_003A96B8);
                e->unk1A = 0;
                e = func_00345780(func_003A96B8);
                e->unk1A = 1;
                f = &gFighters[D_005F6D1C->a & 1];
                if (D_005F6D1C->b == 0) {
                    func_003A88A0(f);
                } else {
                    func_003A8890(f);
                }
                break;
            case 13:
                Bgm_Play(1);
                break;
            }
            D_005F6D1C++;
        }
        func_003A9898();
        if ((D_005F5CDC & 0x800) || (func_003BB308() == 0xD && (D_005F5CDC & 0xFC))) {
            D_005F6D24 = 1;
            D_005F6D20 = 4;
            D_005F6D22 = 3;
            break;
        }
        if (!(D_005F6D24 & 1)) {
            D_005F6D22++;
        }
        if (D_005F6D22 > 0x1C20) {
            done = 1;
        }
        break;
    case 4:
        func_003ADFB0();
        func_003A9898();
        done = -1;
        if (--D_005F6D22 > done) {
            done = 0;
        }
        break;
    default:
        done = 1;
        break;
    }
    if (done) {
        func_003A9688(0);
        D_005F6D24 = 0;
        func_003ADFA8();
        Bgm_Stop();
        func_00346600();
        func_003A6BD0();
        func_003A7558();
        func_003A87E8();
    }
    return done;
}
#else
INCLUDE_RODATA("asm/nonmatchings/loading_title", jtbl_00519B30);

INCLUDE_ASM("asm/nonmatchings/loading_title", func_003A9AB8);

INCLUDE_RODATA("asm/nonmatchings/loading_title", jtbl_00519B50);
#endif

void func_003AA410(LtFx *e) {
    D_005F6278 = 1;
    if (func_003AA5E8() == 0) {
        if (--e->unk1C <= 0) {
            func_003465C8(e);
            D_005F6278 = 0;
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519B90);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519EC0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519ED0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519EE0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519EF0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519EF8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F08);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F18);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F28);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F38);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F48);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F58);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F68);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F78);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F88);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519F98);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519FA8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519FB8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519FC8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519FD8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519FE8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_00519FF8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A008);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A018);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A028);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A030);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A040);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A050);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A060);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A070);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A080);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A090);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A0A0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A0B0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A0C0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A0C8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A0D8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A0E8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A0F0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A188);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A1F0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A278);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A300);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A418);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A468);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A4B8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A540);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A5C8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A618);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A6A0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A6F0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A740);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A7B8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A830);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A880);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A8D0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A920);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051A9E8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051AA60);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051AB50);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051ABA0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051AC18);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051ACB0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051AD10);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051ADA8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051AE10);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051AE60);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051AF60);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051AFB0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B028);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B240);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B2A8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B2E8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B358);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B448);

s32 func_003AA460(void) {
    return D_0051B678[D_005F6268];
}

s32 func_003AA478(void) {
    return D_005F6D3C;
}

s32 func_003AA480(void) {
    s32 r = 0;

    if (func_003ABCD0() != 0) {
        r = D_005F626E != 0;
    }
    return r;
}

void func_003AA4B8(LtFx *e) {
    D_005F626E = 1;
    if (func_003AA5E8() == 0) {
        if (--e->unk1C < 0) {
            D_005F626E = 0;
            func_003465C8(e);
        }
    }
}

s32 func_003AA510(void) {
    s32 r = 0;

    if (func_003ABCD0() != 0) {
        r = D_0051B678[D_005F6268] == 0xD;
    }
    return r;
}

s32 func_003AA558(void) {
    s32 r = 0;

    if (func_003ABCD0() != 0) {
        r = D_0051B678[D_005F6268] == 9;
    }
    return r;
}

s32 func_003AA5A0(void) {
    s32 r = 0;

    if (func_003ABCD0() != 0) {
        r = D_0051B678[D_005F6268] == 0x11;
    }
    return r;
}

s32 func_003AA5E8(void) {
    return 0;
}

void func_003AA5F0(void) {
    D_005F5EE0 = 0;
    D_005F5CBC = 1;
    D_005F5EE4 = 0;
    D_005F6D38 = 0;
    D_005F6D3A = 0;
    D_005F6D56 = 0;
    D_005F6D3C = 0;
    D_005F6D48 = 0;
    D_005F626C = 0;
    D_005F626E = 0;
    D_005F6278 = 0;
    func_003ABDC8();
    func_003A87E8();
    Bgm_Stop();
}

/* NON_MATCHING: 100 lines, the switch value and the loop locals get other saved registers (sibling of func_003A9898) */
#if 0
void func_003AA640(void) {
    s32 i;

    if (D_005F6D54 == 0) {
        D_005F6D54 = func_00392F80(D_005F6D4C, D_005F6D50);
        if (D_005F6D54 != 0) {
            D_005F6D40 = 1;
        }
        if (!(D_005F6D56 & 1)) {
            D_005F6D50++;
        }
    }
    func_0035B4B0(D_00415808, D_00436530);
    func_003532A0();
    func_00341218();
    func_003D1040();
    Wind_Update();
    for (i = 0; i < 2; i++) {
        LtSide *s = &D_008D78C8[i];
        Fighter *f = &gFighters[i];
        u32 flag;
        s32 k;
        s32 v;

        switch (s->state) {
        case 0:
            f->unk127 = 0;
            break;
        case 1:
            f->unk127 = 1;
            if (!(D_005F6D56 & 1)) {
                s->state = 2;
            }
            break;
        case 2:
            f->unk127 = 1;
            k = func_003AA460();
            v = D_005F6D50;
            if (k == 0x14) {
                v++;
            }
            f->unkBC = f->unkC4 = v;
            flag = f->unkC0->unk24;
            f->unkC0->unk24 = flag & ~0x400000;
            flag = (flag >> 22) & 1;
            if (s->clip != 2) {
                func_00364080(s->clip);
                f->unkB084 = D_005F6D50;
                f->unkB074[0] = f->unkB074[1] = f->unkB088 = s->clip + 0x32;
            } else {
                func_00364080(1);
                f->unkB074[1] = 0x33;
                f->unkB084 = D_005F6D50;
                f->unkB088 = s->clip + 0x32;
                f->unkB074[0] = 0x33;
            }
            f->unkB06C[0] = f->unkB06C[1] = D_005F6D50;
            func_0034E8B0(f);
            f->unkC0->unk24 = (f->unkC0->unk24 & ~0x400000) | (flag << 22);
            func_00378398(f);
            func_00378600(f);
            break;
        }
    }
    func_00366ED0();
    func_003DB690();
    gFighters[0].unkC6 = gFighters[0].unkC4;
    gFighters[1].unkC6 = gFighters[1].unkC4;
}
#else
INCLUDE_ASM("asm/nonmatchings/loading_title", func_003AA640);
#endif

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B488);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B678);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B790);
/* An ending script entry (D_0051B488, 12 bytes per ending) [V offsets; I names]. */
typedef struct {
    /* 0x0 */ const LtCmd *script;
    /* 0x4 */ u16 stage;
    /* 0x6 */ u16 pad6;
    /* 0x8 */ s16 *pairs;
} LtEnding;

/* A camera as func_003D2270 fills it: a matrix and the position at +0x30 [I]. */
typedef struct {
    /* 0x00 */ f32 m[3][4];
    /* 0x30 */ f32 pos[4];
} LtCam;

typedef struct {
    /* 0x0 */ s16 v[4];
} LtSVec;

extern const LtEnding D_0051B488[];
extern const char D_0051B790[];
extern const LtCmd *D_005F6D34;
extern s16 *D_005F6D44;
extern u16 D_005F6270;
extern const LtSVec D_005F6280;
extern u8 D_0087A3F0[];
extern u8 D_00879BD0[];
s32 func_003BFDD8(s32 n);
void Unlock_SetClearBit(s32 n);
void func_0039C7D8(s32 a);
void Bgm_SetVolumeFade(s32 vol, s32 frames);
void func_00343F88(void);
void func_003533F8(s32 n);
void func_003B8ED8(void);
void func_003753D0(void);
void func_00378368(s32 a, s32 b, s32 c);
void *Gte_ApplyMatrix(void *m, LtSVec *v, s32 *out);
void func_003D2270(LtCam *cam);
void func_003A2758(s16 kind, LtHit *p, s32 flag, f32 f);
void Credits_Update(void);
s32 func_003ABDC0(void);
void func_003ABE20(void);
void func_003ABE30(void);
void func_003AC558(LtFx *e);
void func_003AC8B0(LtFx *e);
void func_003AC978(LtFx *e);
void func_003ACAB0(LtFx *e);
void func_003ACEE0(void);
void func_003AD1D8(LtFx *e);
void func_003AD240(void);
s32 func_003AD678(void);
void func_003AA4B8(LtFx *e);
void func_003AA410(LtFx *e);

/* NON_MATCHING: 645 lines, lit4: the C pi literal shifts the $gp offsets of the whole object (about 280 lines are real register/scheduling differences) */
#if 0
s32 func_003AA8A8(void) {
    LtSide *s0 = &D_008D78C8[0];
    LtSide *s1 = &D_008D78C8[1];
    Fighter *f0 = &gFighters[0];
    Fighter *f1 = &gFighters[1];
    const LtCmd *cmd;
    const LtEnding *end;
    LtSide *s;
    Fighter *f;
    LtFx *e;
    s16 *p;
    f32 *j;
    f32 *j2;
    LtHit hit;
    LtHit hit2;
    LtHit hit3;
    LtCam cam;
    LtCam cam2;
    LtSVec sv;
    s32 done = 0;
    u32 pad;
    u32 mask;
    s16 i;
    s32 k;
    s32 m;
    s32 chara;
    s32 costume;
    s32 side;
    s32 other;
    s32 n;
    s32 flag;
    void *mv;

    if ((u32)(D_005F6274 - 0xC) < 2) {
        pad = D_005F5CDC;
        mask = 0x8FC;
    } else {
        pad = func_003712C0();
        mask = 0x800;
    }
    Credits_Update();
    D_005F6D56 = 0;
    func_003A96B0(0);
    switch (D_005F6D38) {
    case 0:
        func_00346600();
        if ((u32)(D_005F6274 - 0xC) < 2) {
            D_005F6D38 = 1;
            D_005F6D3C = 2;
        } else {
            D_005F6D38 = 2;
        }
        break;
    case 1:
        Text_Printf(D_0051B790, 10, 5, 0x195, 0x1A0, 0);
        if (--D_005F6D3C < 0) {
            func_003992C8(1);
            D_005F6D38 = 5;
        }
        break;
    case 2:
        chara = D_0051B678[D_005F6268];
        costume = D_0051B678[D_005F626A];
        if (func_003BFDD8(chara) != 0 && func_003BFDD8(costume) == 0) {
            chara = costume;
            D_005F6268 = D_005F626A;
        }
        Unlock_SetClearBit(chara);
        if ((D_005F6268 >> 3) == 0x21) {
            func_003992C8(0);
            func_0039C7D8(2);
            D_005F6D38 = 4;
        } else {
            func_003AD240();
            D_005F6D38 = 3;
        }
        break;
    case 3:
        if (func_003AD678() != 0) {
            D_005F5CBC = 1;
            D_005F6D38 = 5;
            func_003753A8();
            Bgm_SetVolumeFade(0, 600);
        }
        break;
    case 4:
        D_005F6270 = 6;
        Bgm_PlayFlag(6);
        D_005F6D38 = 6;
        D_005F6D3C = 600;
        break;
    case 5:
        func_00346600();
        func_00343F88();
        D_005F6D56 = 0;
        D_005F5F10 = 1;
        D_005F67BC = 0;
        func_00353D58();
        func_00353DB0();
        func_00353658();
        func_00353E08();
        k = D_0051B678[D_005F6268];
        func_003533F8(k);
        end = &D_0051B488[k];
        D_005F6D34 = end->script;
        D_005F5CB6 = end->stage;
        func_003A9688(k + 2);
        D_005F6D44 = end->pairs;
        func_003C1C90();
        func_003B8ED8();
        CamScript_Load(func_003A9680());
        func_00393708(D_005F5CD0);
        Motion_LoadBanks(func_003A9680());
        costume = D_005F6268 & 7;
        for (i = 0, s = D_008D78C8; i < 2; i++, s++) {
            s->state = 0;
            s->frame = 0;
            p = &D_005F6D44[i * 2];
            if (p[0] > 0) {
                s->pad2[0] = p[0];
                s->pad2[1] = p[1];
                if (s->pad2[1] < 0) {
                    s->pad2[1] = costume;
                }
            } else {
                s->pad2[1] = 0;
                s->pad2[0] = 0;
            }
            s->clip = 0;
        }
        D_00440940[0][0].unk0 = s0->pad2[0];
        D_00440940[0][0].unk4 = s0->pad2[1];
        D_00440940[1][0].unk0 = s1->pad2[0];
        D_00440940[1][0].unk4 = s1->pad2[1];
        func_003AE1E8();
        p = D_005F6D44;
        i = 0;
        while (p[0] >= 0) {
            chara = p[0];
            costume = p[1];
            if (costume < 0) {
                if (chara == 2) {
                    costume = D_005F6268 & 6;
                } else {
                    costume = D_005F6268 & 7;
                }
            }
            side = i & 1;
            p += 2;
            f = &gFighters[side];
            func_0035C8D0(f, chara, costume);
            f->unk22 = i;
            func_00367450(f);
            m = func_00367310(f->unk22);
            func_003AF540(f, 0, m);
            func_003AFDA8(side, f->unk22, m);
            i++;
            D_005F6D48++;
            if (i >= 4) {
                break;
            }
        }
        for (i = 0; i < 2; i++) {
            f = &gFighters[i];
            func_0035C8D0(f, D_00440940[i][0].unk0, D_00440940[i][0].unk4);
            f->unk22 = i;
            func_00368168(f);
        }
        func_003420C8(4);
        f0->unk28 = func_00368288(f0->unk28);
        f1->unk28 = func_00368288(f1->unk28);
        func_00367938(D_005F5CB6);
        func_003645D8(0);
        func_003645D8(1);
        func_003645D8(2);
        func_00340070(D_005F5CB6);
        Wind_Init(D_005F5CB6);
        func_00366DB0(D_005F5CB6);
        gRoundPhase = 0;
        func_0038D508();
        func_00373380(f0);
        func_00373380(f1);
        func_00373038(f0, f1);
        f1->unk127 = 0;
        f1->unk40 = -0x8000;
        f0->unk127 = 0;
        f1->unk4 = 0;
        f0->unk4 = 0;
        f1->unk8 = 0;
        f0->unk8 = 0;
        f1->unkC = 0;
        f0->unkC = 0;
        f1->unk18 = 0;
        f0->unk18 = 0;
        f1->unk1A = -0x8000;
        f0->unk1A = -0x8000;
        f1->unk1C = 0;
        f0->unk1C = 0;
        f0->unk3C = -0x8000;
        f0->unk40 = -0x8000;
        f1->unk3C = -0x8000;
        f0->unk51C = 0;
        f0->unk524 = 0;
        f1->unk51C = 0;
        f1->unk524 = 0;
        Cam_Reset();
        func_003963C0(0, f0, f1);
        func_00344BE0();
        Fx_StartStageLayers();
        func_003A6BD0();
        D_005F6D4C = 0;
        D_005F6D50 = 0;
        D_005F6D54 = 0;
        D_005F6D40 = 0;
        func_003ADFA8();
        func_003992C8(0);
        if ((u32)(D_005F6274 - 0xC) < 2) {
            D_005F6270 = 7;
        } else {
            D_005F6270 = 6;
        }
        Bgm_PlayFlag(D_005F6270);
        D_005F6D3C = 600;
        D_005F6D38 = 6;
        break;
    case 6:
        if (D_005F6D56 & 1) {
            break;
        }
        if (--D_005F6D3C >= 0 && Bgm_IsPlaying() == 0) {
            break;
        }
        Bgm_Play(D_005F6270);
        if ((u32)(D_005F6274 - 0xC) < 2) {
            D_005F6D3C = 0;
            D_005F6D38 = 9;
            break;
        }
        func_003ABE20();
        if ((D_005F6268 >> 3) == 0x21) {
            D_005F6D38 = 10;
            break;
        }
        D_005F6D3C = 0x120C;
        D_005F6D38 = 7;
        break;
    case 7:
        func_003ADFA8();
        if (D_005F6D56 & 1) {
            break;
        }
        if (pad & 0x800) {
            Snd_PlaySe(4, 0);
            func_003ABE30();
            Bgm_Stop();
            D_005F6270 = 7;
            Bgm_PlayFlag(7);
            D_005F6D3C = 600;
            D_005F6D38 = 8;
            break;
        }
        if (--D_005F6D3C < 0) {
            D_005F6D3C = 0;
            D_005F6D38 = 9;
        }
        break;
    case 8:
        if (D_005F6D56 & 1) {
            break;
        }
        if (--D_005F6D3C >= 0 && Bgm_IsPlaying() == 0) {
            break;
        }
        func_003A6BD0();
        Bgm_Play(D_005F6270);
        D_005F6D3C = 0;
        D_005F6D38 = 9;
        break;
    case 9:
        D_005F5F20 = -1;
        if (pad & mask) {
            func_003ABE30();
            Snd_PlaySe(4, 0);
            D_005F6D3C = 4;
            D_005F6D38 = 11;
        }
        func_003ADFB0();
        while (D_005F6D3C >= D_005F6D34->frame) {
            if (D_005F6D34->op == -1) {
                D_005F6D40 = 1;
                break;
            }
            cmd = D_005F6D34;
            switch (cmd->op) {
            case 0:
                k = cmd->a;
                if (k < 0 || k >= D_005F6D48) {
                    k = 0;
                }
                p = &D_005F6D44[k * 2];
                costume = p[1];
                chara = p[0];
                if (costume < 0) {
                    costume = D_005F6268 & 7;
                }
                D_00440940[0][0].unk0 = chara;
                D_00440940[0][0].unk4 = costume;
                func_0035C8D0(f0, chara, costume);
                func_00368168(f0);
                f0->unk22 = k;
                m = func_00367310(gFighters[0].unk22);
                func_003AF540(&gFighters[0], 0, m);
                Fx_LoadFighterTex(f0);
                func_003AFDA8(0, gFighters[0].unk22, m);
                f0->unk28 = func_00368288(f0->unk28);
                s0->pad2[0] = chara;
                s0->pad2[1] = costume;
                s0->frame = 0;
                s0->state = 0;
                func_00344A28(f0);
                break;
            case 1:
                k = cmd->a;
                if (k < 0 || k >= D_005F6D48) {
                    k = 0;
                }
                p = &D_005F6D44[k * 2];
                costume = p[1];
                chara = p[0];
                if (costume < 0) {
                    costume = D_005F6268 & 7;
                }
                D_00440940[1][0].unk4 = costume;
                D_00440940[1][0].unk0 = chara;
                func_0035C8D0(f1, chara, costume);
                func_00368168(f1);
                f1->unk22 = k;
                m = func_00367310(gFighters[1].unk22);
                func_003AF540(&gFighters[1], 0, m);
                func_00344A28(f1);
                func_003AFDA8(1, gFighters[1].unk22, m);
                f1->unk28 = func_00368288(f1->unk28);
                s1->pad2[0] = chara;
                s1->pad2[1] = costume;
                s1->frame = 0;
                s1->state = 0;
                Fx_LoadFighterTex(f1);
                break;
            case 2:
                if (s0->clip != cmd->a) {
                    mv = func_00368218(f0, cmd->a);
                    f0->unkB8 = f0->unkC0 = mv;
                }
                s0->clip = D_005F6D34->a;
                s0->frame = D_005F6D34->b;
                s0->state = 2;
                s0->len = D_005F6D34->c;
                break;
            case 3:
                if (s1->clip != cmd->a) {
                    mv = func_00368218(f1, cmd->a);
                    f1->unkB8 = f1->unkC0 = mv;
                }
                s1->clip = D_005F6D34->a;
                s1->frame = D_005F6D34->b;
                s1->state = 2;
                s1->len = D_005F6D34->c;
                break;
            case 4:
                func_003465E0();
                D_005F6D4C = D_005F6D34->a;
                D_005F6D50 = D_005F6D34->b;
                break;
            case 5:
            case 6:
            case 19:
            case 20:
                e = func_00345780(func_003AC5D0);
                if (e == NULL) {
                    break;
                }
                switch (D_005F6D34->op) {
                case 6:
                    e->unk1A = 3;
                    break;
                case 5:
                    e->unk1A = 2;
                    e->d.unk44 = D_005F6D34->b;
                    break;
                case 19:
                    e->unk1A = 0;
                    break;
                case 20:
                    e->unk1A = 1;
                    break;
                }
                e->unk1C = D_005F6D34->a;
                break;
            case 21:
                e = func_00345780(func_003AC7C0);
                if (e != NULL) {
                    cmd = D_005F6D34;
                    e->unk1A = cmd->a;
                    e->d.unk44 = cmd->b;
                    e->unk1C = cmd->c;
                }
                break;
            case 10:
                f = &gFighters[cmd->c & 1];
                if (cmd->b == 0x4003) {
                    sv = D_005F6280;
                    Gte_ApplyMatrix((u8 *)f + 0x974, &sv, hit.pos);
                    hit.pos[0] += *(s32 *)((u8 *)f + 0x988);
                    hit.pos[1] += *(s32 *)((u8 *)f + 0x98C);
                    hit.pos[2] += *(s32 *)((u8 *)f + 0x990);
                } else {
                    j = (f32 *)((u8 *)f + cmd->b * 0xC0 + 0xDB0 + 0x30);
                    hit.pos[0] = j[0];
                    hit.pos[1] = -(s32)j[1];
                    hit.pos[2] = -(s32)j[2];
                    hit.unk14 = 0;
                    hit.unk12 = 0;
                    hit.unk10 = 0;
                }
                func_003A2BC0(D_005F6D34->a, 0, &hit, 0);
                break;
            case 15:
                e = func_00345780(func_003AC558);
                if (e != NULL) {
                    e->unk1A = D_005F6D34->a & 1;
                    e->unk1C = D_005F6D34->b;
                }
                break;
            case 16:
                func_00378368(cmd->a, cmd->b, cmd->c);
                break;
            case 17:
                side = cmd->a & 1;
                other = side ^ 1;
                n = (cmd->a & 6) >> 1;
                if (cmd->b == 0x4002) {
                    func_003D2270(&cam);
                    j = cam.pos;
                    j2 = (f32 *)(D_0087A3F0 + D_005F6D34->c * 0xC0 + other * 0xB8A0 + 0x30);
                } else {
                    j = (f32 *)(D_0087A3F0 + cmd->b * 0xC0 + side * 0xB8A0 + 0x30);
                    j2 = (f32 *)(D_0087A3F0 + cmd->c * 0xC0 + other * 0xB8A0 + 0x30);
                }
                hit2.pos[0] = j[0];
                hit2.pos[1] = -(s32)j[1];
                hit2.pos[2] = -(s32)j[2];
                hit2.unk10 = j[0] - j2[0];
                hit2.unk12 = -(s32)(j[1] - j2[1]);
                hit2.unk14 = -(s32)(j[2] - j2[2]);
                func_003A2758(n, &hit2, 0, 0.0f);
                break;
            case 18:
                side = cmd->a & 1;
                k = cmd->b;
                if (k == 0x62) {
                    j = (f32 *)(D_00879BD0 + side * 0xB8A0);
                    hit3.pos[0] = j[0];
                    hit3.pos[1] = 0;
                    flag = 0;
                    hit3.pos[2] = -(s32)j[2];
                } else if (k == 0x4000) {
                    func_003ACEE0();
                    break;
                } else if (k == 0x4001) {
                    flag = 0;
                    hit3.pos[1] = D_00415808[6] - 800;
                    hit3.pos[0] = D_00415808[5];
                    hit3.pos[2] = D_00415808[7];
                } else if (k == 0x4002) {
                    func_003D2270(&cam2);
                    flag = 1;
                    hit3.pos[0] = cam2.pos[0];
                    hit3.pos[1] = -(s32)cam2.pos[1];
                    hit3.pos[2] = -(s32)cam2.pos[2];
                } else {
                    flag = 1;
                    j = (f32 *)(D_0087A3F0 + k * 0xC0 + side * 0xB8A0 + 0x30);
                    hit3.pos[0] = j[0];
                    hit3.pos[1] = -(s32)j[1];
                    hit3.pos[2] = -(s32)j[2];
                }
                hit3.unk14 = 0;
                hit3.unk12 = 0;
                hit3.unk10 = 0;
                func_003A2758(3, &hit3, flag, (f32)D_005F6D34->c * 2.0f * 3.1415927f * (1.0f / 4096.0f));
                break;
            case 22:
                e = func_00345780(func_003AC8B0);
                if (e != NULL) {
                    e->d.unk0 = D_005F6D34->a;
                    e->d.unk4 = D_005F6D34->b;
                    e->unk1C = (D_005F6D34->c & 0xFF00) >> 8;
                    e->unk1A = D_005F6D34->c & 0xFF;
                }
                break;
            case 23:
                e = func_00345780(func_003AC978);
                if (e != NULL) {
                    e->unk1A = 0xDC;
                    e->unk1C = D_005F6D34->a;
                }
                break;
            case 24:
                e = func_00345780(func_003ACAB0);
                if (e != NULL) {
                    e->unk1A = D_005F6D34->a;
                    e->unk1C = D_005F6D34->b;
                }
                break;
            case 25:
                e = func_00345780(func_003ACB08);
                if (e != NULL) {
                    if (D_005F6D34->a != 0) {
                        e->unk14 |= 0x20;
                    }
                    e->unk1A = D_005F6D34->b;
                    e->unk1C = D_005F6D34->c;
                }
                break;
            case 26:
                D_005F626C = cmd->a & 1;
                break;
            case 27:
                func_003465E0();
                break;
            case 28:
                f = &gFighters[cmd->a & 1];
                if (f->unk2A == 0x14 || f->unk2A == 0x1D) {
                    *(s16 *)((u8 *)f + 0xB052) = cmd->b;
                }
                break;
            case 29:
                e = func_00345780(func_003AA4B8);
                if (e != NULL) {
                    e->unk1C = D_005F6D34->a;
                }
                break;
            case 12:
                f = &gFighters[cmd->a & 1];
                if (cmd->b == 0) {
                    func_003A88A0(f);
                } else {
                    func_003A8890(f);
                }
                break;
            case 30:
                e = func_00345780(func_003AD1D8);
                if (e != NULL) {
                    e->unk1C = D_005F6D34->a;
                }
                break;
            case 31:
                if (cmd->a & 1) {
                    func_0034CB38(0);
                }
                if (D_005F6D34->a & 2) {
                    func_0034CB38(1);
                }
                break;
            case 32:
                e = func_00345780(func_003AA410);
                if (e != NULL) {
                    e->unk1C = D_005F6D34->a;
                }
                break;
            }
            D_005F6D34++;
        }
        func_003AA640();
        if (!(D_005F6D56 & 1)) {
            D_005F6D3C++;
        }
        if (D_005F6D3C > 0x1C20) {
            D_005F6D40 = 1;
        }
        if (D_005F6D40 != 0) {
            if (D_005F6270 == 7) {
                func_003ADFA8();
                D_005F6D3C = 0;
                D_005F6D38 = 12;
            } else {
                func_003ADFA8();
                D_005F6D3C = 0;
                D_005F6D38 = 10;
            }
        }
        break;
    case 10:
        func_003ADFA8();
        if (D_005F6D56 & 1) {
            break;
        }
        if (pad & 0x800) {
            Snd_PlaySe(4, 0);
            func_003ABE30();
            D_005F6D38 = 12;
            break;
        }
        if (func_003ABDC0() == 0) {
            break;
        }
        func_003ABE30();
        D_005F6D38 = 12;
        break;
    case 11:
        D_005F6D56 = 1;
        func_003ADFB0();
        func_003AA640();
        if (--D_005F6D3C >= 0) {
            break;
        }
        func_003992C8(1);
        func_003ADFA8();
        D_005F6D38 = 12;
        break;
    case 12:
        D_005F6D56 = 0;
        func_003ABE30();
        func_003ADFA8();
        func_003A87E8();
        func_003A6BD0();
        if ((u32)(D_005F6274 - 0xC) < 2) {
            Bgm_SetVolumeFade(0, 0x3C);
        } else {
            Bgm_Stop();
        }
        func_003753D0();
        done = 1;
        func_00346600();
        break;
    default:
        done = 1;
        break;
    }
    return done;
}
#else
INCLUDE_RODATA("asm/nonmatchings/loading_title", jtbl_0051B7B0);

LIT4_WORD(D_005F531C, 0x40490FDB);
INCLUDE_ASM("asm/nonmatchings/loading_title", func_003AA8A8);

INCLUDE_RODATA("asm/nonmatchings/loading_title", jtbl_0051B7F0);
#endif

void Mode_Ending(void) {
    switch (gSubMode) {
    case 0:
        func_003AA5F0();
        gSubMode = 1;
        break;
    case 1:
        if (func_003AA8A8() != 0) {
            func_003A96B0(0);
            gSubMode = 2;
        }
        break;
    case 2:
        if (D_005F6274 == 1) {
            Mode_Change(2);
            return;
        }
        gSubMode = 0;
        gMode = D_005F6274;
        break;
    default:
        gSubMode = 0;
        break;
    }
}

s32 func_003ABC80(void) {
    if (gMode == 8 && gSubMode == 1 && D_005F6D38 == 9 && D_005F626C != 0) {
        return gSubMode;
    }
    return 0;
}

s32 func_003ABCC0(void) {
    return gMode == 8;
}

s32 func_003ABCD0(void) {
    s32 r = 0;

    if (gMode == 8 && (D_005F6D38 == 9 || D_005F6D38 == 5)) {
        r = 1;
    }
    return r;
}

s32 func_003ABD08(void) {
    if (func_003ABCD0() == 0) {
        return 0;
    }
    return D_005F6278 == 0;
}

s32 func_003ABD38(void) {
    s32 r = 0;

    if (gMode == 8 && (D_005F6D38 == 9 || D_005F6D38 == 5 || D_005F6D38 == 0xB)) {
        r = 1;
    }
    return r;
}

s32 func_003ABD78(void) {
    s32 r = 0;

    if (gMode == 8) {
        r = (u16)(D_005F6D38 - 2) < 2;
    }
    return r;
}

void Mode_EnterEnding(s32 ret, s32 a, s32 b) {
    D_005F6274 = ret;
    D_005F6268 = a;
    D_005F626A = b;
    gMode = 8;
    gSubMode = 0;
}

s32 func_003ABDC0(void) {
    return D_005F629A;
}

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051B880);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CCD8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CCF0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CD00);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CD20);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CD38);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CD48);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CD58);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CD68);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CD88);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CD98);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CDB0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CDD0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CDE8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CE08);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CE18);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CE28);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CE50);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CE60);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CE78);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CE98);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CEA8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CEB8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CEC8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CED8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CEE8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CEF8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF08);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF18);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF30);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF40);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF50);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF60);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF70);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF80);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CF90);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CFA0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CFB0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CFC0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CFD8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051CFF0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D000);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D018);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D028);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D038);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D050);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D060);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D078);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D088);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D098);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D0B0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D0D0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D0E8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D100);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D118);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D130);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D148);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D158);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D168);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D188);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D198);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D1A8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D1B8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D1C8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D1E0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D1F8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D208);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D218);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D228);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D238);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D248);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D258);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D268);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D278);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D290);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D2A8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D2C0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D2D0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D2F0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D300);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D310);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D330);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D340);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D350);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D360);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D370);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D380);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D390);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D3A0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D3B0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D3C0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D3D0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D3E0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D3F8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D408);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D418);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D430);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D448);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D458);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D468);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D478);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D488);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D498);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D4A8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D4B8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D4C8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D4E0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D4F0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D518);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D528);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D540);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D560);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D570);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D580);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D590);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D5B0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D5C8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D5E0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D600);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D618);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D630);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D640);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D658);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D670);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D680);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D698);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D6A8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D6C0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D6D0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D6E0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D708);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D718);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D738);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D758);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D768);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D778);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D788);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D798);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D7A8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D7B8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D7C8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D7D8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D7E8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D800);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D810);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D830);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D840);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D850);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D860);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D870);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D880);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D890);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D8A0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D8B8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D8D0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D8E0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D8F8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D908);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D920);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D930);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D948);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D960);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D970);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D990);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D9A8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D9B8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D9C8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D9E0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051D9F8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DA08);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DA20);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DA30);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DA40);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DA58);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DA78);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DA88);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DA98);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DAB0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DAD0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DAE0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB08);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB18);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB30);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB40);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB50);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB60);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB78);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB88);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DB98);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DBA8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DBC0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DBD0);

void func_003ABDC8(void) {
    s32 i;
    LtSlot *p;

    D_005F629A = 0;
    p = D_008D78E0;
    for (i = 0; i < 0x40; i++) {
        p->unk0 = 0;
        p++;
    }
    D_008D7DE0.unk0 = 0;
    D_008D7DE0.unk4 = 0;
    D_008D7DE0.unk8 = 0;
    D_008D7DE0.unkC = 0;
    D_008D7DE0.unk10 = D_0051B880;
}

void func_003ABE20(void) {
    D_008D7DE0.unk0 = 1;
}

void func_003ABE30(void) {
    D_008D7DE0.unk0 = 0;
}

LtSlot *func_003ABE40(s32 i) {
    s32 k;
    s32 found = -1;

    for (k = 0; k < 0x40; k++) {
        if (D_008D78E0[i].unk0 == 0) {
            found = i;
            break;
        }
        i++;
        if (i >= 0x40) {
            i = 0;
        }
    }
    if (found < 0) {
        return NULL;
    }
    D_008D78E0[found].unk0 = 1;
    return &D_008D78E0[found];
}

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DBE8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DC08);

void func_003ABEC0(void) {
    s32 w = Text_Printf(D_0051DBE8, 10, 5);

    Text_Printf(D_0051DC08, 10, 5, (0x280 - w) / 2, 200, 11);
}

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DC28);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DC48);

void func_003ABF18(void) {
    s32 w = Text_Printf(D_0051DC28, 10, 5);

    Text_Printf(D_0051DC48, 10, 5, (0x280 - w) / 2, 200, 11);
}

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DC70);

s32 func_003ABF70(const LtLine *l) {
    switch (l->kind) {
    case 0:
        return 0x30;
    case 1:
        return 0x130;
    case 2:
        return 0x260 - Text_Printf(D_0051DC70, 10, 4, l->value);
    case 3:
    case 4:
        return (0x280 - Text_Printf(D_0051DC70, 10, 4, l->value)) / 2;
    }
    return 0;
}

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DC98);

extern const char D_0051DC98[];
void func_003A67D0(s32 n, u8 r, u8 g, u8 b);
void func_003A6770(s32 n, u8 r, u8 g, u8 b);
s32 func_003BFEB0(void);

void Credits_Update(void) {
    LtTextState *st = &D_008D7DE0;
    LtSlot *s;
    s32 i;
    s32 wait = 0xAA;

    switch (st->unk0) {
    case 0:
        break;
    case 1:
        func_003A67D0(0xAB, 0, 0, 0);
        st->unk4 = wait;
        st->unk0 = 2;
        break;
    case 2:
        func_003ABEC0();
        if (func_003AA5E8() == 0 && --st->unk4 < 0) {
            st->unk4 = wait;
            st->unk0 = 3;
        }
        break;
    case 3:
        func_003ABEC0();
        if (func_003AA5E8() == 0 && --st->unk4 < 0) {
            st->unk4 = wait;
            st->unk0 = 4;
            func_003A6770(0xAB, 0, 0, 0);
            return;
        }
        break;
    case 4:
        func_003ABEC0();
        if (func_003AA5E8() == 0 && --st->unk4 < 0) {
            func_003A67D0(0xAB, 0, 0, 0);
            st->unk4 = wait;
            st->unk0 = 5;
        }
        break;
    case 5:
        func_003ABF18();
        if (func_003AA5E8() == 0 && --st->unk4 < 0) {
            st->unk4 = wait;
            st->unk0 = 6;
        }
        break;
    case 6:
        func_003ABF18();
        if (func_003AA5E8() == 0 && --st->unk4 < 0) {
            st->unk4 = wait;
            st->unk0 = 7;
            func_003A6770(0xAC, 0, 0, 0);
            return;
        }
        break;
    case 7:
        func_003ABF18();
        if (func_003AA5E8() == 0 && --st->unk4 < 0) {
            st->unk0 = 8;
        }
        break;
    case 8:
        st->unk0 = 9;
        st->unk4 = 0;
        s = func_003ABE40(st->unkC);
        if (s != NULL) {
            if (++st->unkC >= 0x40) {
                st->unkC = 0;
            }
            s->unk8 = 0x1E000;
            s->unkC = st->unk10->unk8;
            s->unk10 = st->unk10->value;
            s->unk4 = func_003ABF70(st->unk10);
            st->unk10++;
        }
    case 9:
        for (i = 0, s = D_008D78E0; i < 0x40; i++, s++) {
            if (s->unk0 != 0) {
                Text_Printf(D_0051DC98, 10, 4, s->unk4, s->unk8 >> 8, s->unkC, s->unk10);
                if (func_003AA5E8() == 0) {
                    s->unk8 -= 0x180;
                    if (s->unk8 < -0x20) {
                        s->unk0 = 0;
                    }
                }
            }
        }
        if (func_003AA5E8() != 0) {
            break;
        }
        if (st->unk10->unk8 < 0) {
            st->unk4 = 0;
            st->unk0 = 0xA;
            break;
        }
        st->unk4 += 0x180;
        if ((st->unk4 >> 8) < 0x20) {
            break;
        }
        st->unk4 = 0;
        s = func_003ABE40(st->unkC);
        if (s == NULL) {
            break;
        }
        s->unk8 = 0x1E000;
        switch (st->unk10->unkA) {
        case 0:
            s->unkC = st->unk10->unk8;
            s->unk10 = st->unk10->value;
            s->unk4 = func_003ABF70(st->unk10);
            break;
        case 1:
            if (func_003BFEB0() > 0) {
                st->unk10++;
            }
            s->unkC = st->unk10->unk8;
            s->unk10 = st->unk10->value;
            s->unk4 = func_003ABF70(st->unk10);
            st->unk10++;
            if (func_003BFEB0() != 0) {
                return;
            }
            break;
        default:
            return;
        }
        st->unk10++;
        break;
    case 10:
        for (i = 0, s = D_008D78E0; i < 0x40; i++, s++) {
            if (s->unk0 != 0) {
                Text_Printf(D_0051DC98, 10, 4, s->unk4, s->unk8 >> 8, s->unkC, s->unk10);
                if (func_003AA5E8() == 0) {
                    s->unk8 -= 0x180;
                    if (s->unk8 < -0x20) {
                        s->unk0 = 0;
                    }
                }
            }
        }
        if (func_003AA5E8() != 0) {
            break;
        }
        st->unk4 += 0x180;
        if (st->unk4 > 0xCFFF) {
            st->unk4 = 0;
            st->unk0 = 0xB;
        }
        break;
    case 11:
        for (i = 0, s = D_008D78E0; i < 0x40; i++, s++) {
            if (s->unk0 != 0) {
                Text_Printf(D_0051DC98, 10, 4, s->unk4, s->unk8 >> 8, s->unkC, s->unk10);
            }
        }
        if (func_003AA5E8() != 0) {
            break;
        }
        if (++st->unk4 >= 0xD2) {
            D_005F629A = 1;
        }
        break;
    }
}

void func_003AC558(LtFx *e) {
    if (func_003AA5E8() == 0) {
        if (--e->unk1C < 0) {
            func_003465C8(e);
        } else {
            func_003A6440(&gFighters[e->unk1A & 1]);
        }
    }
}

void func_003AC5D0(LtFx *e) {
    LtDraw *d = &e->d;
    u16 a;
    LtQuad *q;

    if (e->unk18 == 0) {
        if (e->unk1C == 0) {
            e->unk1C = 1;
        }
        switch (e->unk1A) {
        case 0:
            d->unk44 = 128.0f;
            d->unk48 = -(128.0f / e->unk1C);
            d->unk1A = 9;
            break;
        case 1:
            d->unk44 = 0.0f;
            d->unk48 = 128.0f / e->unk1C;
            d->unk1A = 9;
            break;
        case 2:
            d->unk48 = -(d->unk44 / e->unk1C);
            d->unk1A = 10;
            break;
        case 3:
            d->unk44 = 0.0f;
            d->unk48 = 128.0f / e->unk1C;
            d->unk1A = 10;
            break;
        default:
            func_003465C8(e);
            return;
        }
        D_005F6D58 = D_005F6730;
        e->unk18++;
    }
    q = &D_0043FD10[(u16)d->unk1A];
    a = d->unk44;
    D_005F6730 = 0xFFF00;
    func_003BB4E8(q, &a, 1);
    if (func_003AA5E8() == 0) {
        d->unk44 += d->unk48;
        if (128.0f < d->unk44) {
            d->unk44 = 128.0f;
        }
        if (d->unk44 < 0.0f) {
            d->unk44 = 0.0f;
        }
        if (--e->unk1C <= 0) {
            D_005F6730 = D_005F6D58;
            func_003465C8(e);
        }
    }
}

void func_003AC7C0(LtFx *e) {
    LtDraw *d = &e->d;
    u16 a;
    LtQuad *q;

    if (e->unk18 == 0) {
        switch (e->unk1A) {
        case 0:
        case 1:
            d->unk1A = 9;
            break;
        case 2:
        case 3:
            d->unk1A = 10;
            break;
        default:
            func_003465C8(e);
            return;
        }
        D_005F6D58 = D_005F6730;
        e->unk18++;
    }
    q = &D_0043FD10[(u16)d->unk1A];
    a = d->unk44;
    D_005F6730 = 0xFFF00;
    func_003BB4E8(q, &a, 1);
    if (func_003AA5E8() == 0) {
        if (--e->unk1C <= 0) {
            D_005F6730 = D_005F6D58;
            func_003465C8(e);
        }
    }
}

void func_003AC8B0(LtFx *e) {
    LtDraw *d = &e->d;

    if (e->unk18 == 0) {
        d->unk54 = 640.0f;
        d->unk58 = (640.0f - e->unk1A) / e->unk1C;
        e->unk18++;
    }
    func_003CF850(d->unk0, d->unk4, d->unk54);
    if (func_003AA5E8() == 0) {
        if (e->unk1C > 0) {
            d->unk54 -= d->unk58;
            e->unk1C--;
        }
    }
}

void func_003AC978(LtFx *e) {
    func_003CFAD8(e->unk1A, 0x85, 0xC0);
    if (func_003AA5E8() == 0) {
        if (--e->unk1C <= 0) {
            func_003465C8(e);
        }
    }
}

void func_003AC9D8(u32 n) {
    s32 saved;
    u8 *p;

    if (n < 3) {
        p = D_00437D10[n];
        saved = D_005F6730;
        D_005F6730 = 0xFFEF0;
        D_005F6738 = (u64)p[0] | (u64)p[1] << 8 | (u64)p[2] << 16 | (u64)p[3] << 24;
        D_005F6740 = (u64)p[4] | (u64)p[5] << 2 | (u64)p[6] << 4 | (u64)p[7] << 6 | (u64)p[8] << 32;
        func_003CE5D8(0, 0, 0x280, 0, 0, 0x1C0, 0x280, 0x1C0, 1);
        D_005F6730 = saved;
    }
}

void func_003ACAB0(LtFx *e) {
    func_003AC9D8(e->unk1A);
    if (func_003AA5E8() == 0) {
        e->unk1C--;
    }
    if (e->unk1C <= 0) {
        func_003465C8(e);
    }
}

void func_003ACB08(LtFx *e) {
    LtDraw *d = &e->d;
    s32 id = e->unk1A & 0x3FFF;
    s16 flag = e->unk1A & 0x4000;
    u8 *base;
    void *m;

    if (e->unk18 == 0) {
        if (!(e->unk14 & 0x20)) {
            base = (u8 *)gFighters;
        } else {
            base = D_00884EE0;
        }
        switch (D_005F5CB6) {
        case 3:
            if (id != 0x13) {
                goto kill;
            }
            if (flag == 0) {
                m = base + 0x2A30;
            } else {
                f32 r[4] __attribute__((aligned(16)));

                Mtx_Unit(D_008D7E00);
                r[0] = -0.34906584f;
                r[1] = 0.7853982f;
                r[2] = 0.0f;
                r[3] = 1.0f;
                sceVu0RotMatrix(D_008D7E00, D_008D7E00, r);
                D_008D7E00[3][3] = 1.0f;
                D_008D7E00[3][2] = 1500.0f;
                D_008D7E00[3][0] = 0.0f;
                D_008D7E00[3][1] = 0.0f;
                m = D_008D7E00;
            }
            break;
        case 7:
            switch (id) {
            case 1:
                m = base + 0x2A30;
                break;
            case 0:
                m = base + 0x3CF0;
                break;
            case 2:
                m = base + 0x2A30;
                break;
            default:
                goto kill;
            }
            break;
        case 5:
            if (id != 0) {
                goto kill;
            }
            m = base + 0x2A30;
            break;
        default:
        kill:
            func_003465C8(e);
            return;
        }
        d->unk10 = m;
        e->unk18++;
    }
    func_003D10D0(id, d->unk10, 1.0f);
    if (func_003AA5E8() == 0) {
        e->unk1C--;
    }
    if (e->unk1C <= 0) {
        func_003465C8(e);
    }
}

void func_003ACCD0(LtFx *e) {
    LtDraw *d = &e->d;
    f32 a;
    f32 r;

    if (e->unk18 == 0) {
        e->unk14 |= 0x81;
        d->unk58 = 0;
        d->unk40 = 0;
        d->unk96 = 0;
        d->unk16 = 0;
        d->unk1A = 2;
        d->unk90 = 0x400;
        d->unk44 = 128.0f;
        d->unk50[0] = 0x5A;
        d->unk50[1] = 0x49;
        d->unk50[2] = 0x42;
        d->unk14 = (rand() & 0xF) + 0x1E;
        d->unk54 = Fx_RandFloat() + 3.0f;
        if (e->unk1A == 0) {
            a = Fx_RandFloat() * 3.1415927f;
            d->unk0 = 0x6C00;
            d->unk4 = 0x8200;
        } else {
            a = -(Fx_RandFloat() * 3.1415927f);
            d->unk0 = 0x9400;
            d->unk4 = 0x8200;
        }
        r = (Fx_RandFloat() * 16.0f + 8.0f) * 16.0f;
        d->unk80 = sinf(a) * r;
        d->unk84 = cosf(a) * r;
        d->unk8 = 0xFDE8;
        d->unkC = 10;
        e->unk18++;
    }
    if (func_003AA5E8() == 0) {
        d->unk0 += (s32)d->unk80;
        d->unk4 += (s32)d->unk84;
        d->unk58 += 1.0f / d->unk14;
        if (++d->unk16 >= d->unk14) {
            func_003465C8(e);
        }
    }
}

void func_003ACEE0(void) {
    s32 i;
    LtFx *e;

    for (i = 0; i < 10; i++) {
        e = func_00345780(func_003ACCD0);
        if (e != NULL) {
            e->unk1A = 0;
        }
        e = func_00345780(func_003ACCD0);
        if (e != NULL) {
            e->unk1A = 1;
        }
    }
}

void func_003ACF50(LtFx *e) {
    LtDraw *d = &e->d;
    u8 *f = (u8 *)gFighters;
    LtPos *p = (LtPos *)d;

    if (e->unk18 == 0) {
        e->unk14 |= 8;
        d->unk58 = 0;
        d->unk40 = 0;
        d->unk96 = 0;
        d->unk16 = 0;
        d->unk1A = 8;
        d->unk90 = 0x400;
        d->unk44 = 128.0f;
d->unk50[0] = 0x88;
d->unk50[1] = 0x88;
d->unk50[2] = 0x88;
        d->unk14 = (rand() & 0xF) + 100;
        d->unk54 = Fx_RandFloat() + 1.0f;
        d->unk74 = Fx_RandFloat() * 2.0f * 3.1415927f;
        p->x = *(f32 *)(f + 0x1620);
        p->y = *(f32 *)(f + 0x1624);
        p->z = *(f32 *)(f + 0x1628);
        p->x += (Fx_RandFloat() * 2.0f - 1.0f) * 100.0f;
        p->y += (Fx_RandFloat() * 2.0f - 1.0f) * 100.0f;
        p->z += (Fx_RandFloat() * 2.0f - 1.0f) * 100.0f;
        d->unk120 = Fx_RandFloat() * 30.0f + 30.0f;
        e->unk18++;
    }
    d->unk80 = sinf(d->unk74) * d->unk120;
    d->unk88 = cosf(d->unk74) * d->unk120;
    if (func_003AA5E8() == 0) {
p->x += d->unk80;
d->unk120 -= 0.3f;
p->z += d->unk88;
d->unk74 += 0.05235988f;
        if (d->unk120 < 0.0f) {
            d->unk120 = 0.0f;
        }
        d->unk54 += 2.0f / d->unk14;
        d->unk58 += 1.0f / d->unk14;
        if (++d->unk16 >= d->unk14) {
            func_003465C8(e);
        }
    }
}

void func_003AD1D8(LtFx *e) {
    if (func_003AA5E8() == 0) {
        if (!(--e->unk1C & 1)) {
            func_00345780(func_003ACF50);
        }
        if (e->unk1C < 0) {
            func_003465C8(e);
        }
    }
}

void func_003AD240(void) {
    s32 b;

    D_005F5EE0 = 0;
    D_005F5CBC = 1;
    D_005F5EE4 = 0;
    D_005F6D5C = 0;
    D_005F6D5E = 0;
    D_005F6D60 = 0;
    D_005F6D64 = 0;
    b = func_003B87E0(2);
    func_0035D2E0(b);
    func_0035D300();
    Iop_SetEeStreamBase(b);
    Bgm_Play(3);
}

s32 func_003AD2A8(s32 x) {
    return x * 100 / 60;
}

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DCE0);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DCF8);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DD18);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DD38);

void func_003AD2C8(LtFx *e) {
    LtRecords *r = &D_00440D50;
    LtTime *t = (LtTime *)&e->d;
    u8 *g = D_0043B7F8;
    s32 sum;
    s32 i;
    s32 v;

    if (e->unk18 == 0) {
        if (e->unk1A != 7) {
            t->x = 0x180;
            t->y = 0x1A0;
            t->time = func_003AD2A8(r->rec[e->unk1A].time);
            t->mode = 0;
        } else {
            t->x = 0x130;
            t->y = 0x150;
            t->mode = 1;
            sum = 0;
            for (i = 0; i < 8; i++) {
                sum += func_003AD2A8(r->rec[i].time);
            }
            t->total = func_003AD2A8(*(s32 *)(g + 0x34));
            t->time = func_003AD2A8(r->rec[e->unk1A].time) + (t->total - sum);
        }
        e->unk1C = 0x1E;
        e->unk18++;
    }
    Text_Printf(D_0051DCE0, 10, 5, 0, (e->unk1C << 5) + 0x1A0, t->x, e->unk1A + 1);
    if (t->mode == 0) {
        v = t->time;
        Text_Printf(D_0051DCF8, 10, 5, 0, (e->unk1C << 5) + 0x1C0, t->y, v / 6000, v / 100 % 60, v % 100);
    } else {
        v = t->time;
        if (v < 600000) {
            Text_Printf(D_0051DCF8, 10, 5, 0, (e->unk1C << 5) + 0x1C0, t->y, v / 6000, v / 100 % 60, v % 100);
        } else {
            Text_Printf(D_0051DD18, 10, 5, 0, (e->unk1C << 5) + 0x1A8, t->y, v / 6000, v / 100 % 60, v % 100);
        }
        Text_Printf(D_0051DD38, 10, 5, 11, (e->unk1C << 5) + 0x1A0, 0x180);
        v = t->total;
        if (v < 600000) {
            Text_Printf(D_0051DCF8, 10, 5, 11, (e->unk1C << 5) + 0x1C0, 0x1A0, v / 6000, v / 100 % 60, v % 100);
        } else {
            Text_Printf(D_0051DD18, 10, 5, 11, (e->unk1C << 5) + 0x1A8, 0x1A0, v / 6000, v / 100 % 60, v % 100);
        }
    }
    if (e->unk1C > 0) {
        e->unk1C--;
    }
}

INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DD48);
INCLUDE_RODATA("asm/nonmatchings/loading_title", D_0051DD60);

s32 func_003AD678(void) {
    LtFx *e;

    switch (D_005F6D5C) {
    case 0:
        D_005F6D64 = 0;
        D_005F6D5C = 1;
        D_005F629C = 0;
        break;
    case 1:
        func_003753A8();
        if (func_00365D10(D_005F629C, &D_005F62A0) == 0) {
            if (D_005F629C >= 7) {
                D_005F6D60 = 0;
                D_005F6D5C = 5;
            } else {
                D_005F629C++;
            }
            break;
        }
        D_005F6D60 = 0;
        D_005F6D5C = 2;
        func_003992C8(0);
    case 2:
        if (++D_005F6D60 & 0x30) {
            Text_Printf(D_0051DD48, 10, 5, 40, 40, 14);
        }
        if (func_003712C0() & 0x800) {
            Snd_PlaySe(4, 0);
            D_005F6D5C = D_005F6D60 = 4;
            D_005F5EE0 = 1;
            break;
        }
        if (Battle_UpdateFrame() < 3 && func_00364EE8() == 0) {
            return 0;
        }
        func_00366090();
        D_005F6D60 = 0x21;
        D_005F6D5C = 3;
        D_005F5EE0 = 1;
        e = func_003464F8(func_003AD2C8);
        if (e != NULL) {
            e->unk1A = D_005F629C;
        }
        break;
    case 3:
        Text_Printf(D_0051DD48, 10, 5, 40, 40, 14);
        D_005F5EE0 = 1;
        Battle_UpdateFrame();
        if (--D_005F6D60 >= 0) {
            break;
        }
        if (D_005F629C >= 7) {
            goto done;
        }
        D_005F6D5C = 1;
        D_005F629C++;
        goto end;
    case 4:
        Text_Printf(D_0051DD48, 10, 5, 40, 40, 14);
        Text_Printf(D_0051DD60, 10, 5, 0x195, 0x1A0, 0);
        D_005F5EE0 = 1;
        Battle_UpdateFrame();
        if (--D_005F6D60 >= 0) {
            break;
        }
    done:
        D_005F6D60 = 0;
        D_005F6D5C = 5;
    end:
        func_003ADFA8();
        func_003992C8(1);
        return 0;
    case 5:
        D_005F5EE0 = 0;
        return 1;
    }
    return 0;
}

s32 func_003AD8E0(s16 x) {
    return D_00415840[x][7];
}

s32 func_003AD900(s32 a, s32 b, s32 c, s32 d) {
    return a == b ? c : d;
}

void func_003AD910(u8 *set, s32 i) {
    set[i] = 1;
}

s32 func_003AD920(u8 *a, u8 *b) {
    s32 i;

    for (i = 0; i < 0x22; i++) {
        if (a[i] != 0 && b[i] != 0) {
            return 1;
        }
    }
    return 0;
}

void func_003AD960(u8 *a, u8 *b, u8 *out) {
    s32 i;

    for (i = 0; i < 0x22; i++) {
        out[i] = a[i] != 0 || b[i] != 0;
    }
}

void CharSet_Copy(u8 *a, u8 *b) {
    s32 i;

    for (i = 0; i < 0x22; i++) {
        b[i] = a[i];
    }
}

void CharSet_Clear(u8 *set) {
    s32 i;

    for (i = 0; i < 0x22; i++) {
        set[i] = 0;
    }
}

s32 func_003ADA08(u8 *set, s32 i) {
    return set[i];
}

void func_003ADA18(s32 i, u8 *set) {
    set[i] = 0;
}

void func_003ADA28(u8 *a, u8 *b, u8 *out) {
    s32 i;

    for (i = 0; i < 0x22; i++) {
        out[i] = a[i] != 0 && b[i] != 0;
    }
}

s32 CharSet_Count(u8 *set) {
    s32 i;
    s32 n = 0;

    for (i = 0; i < 0x22; i++) {
        if (set[i] != 0) {
            n++;
        }
    }
    return n;
}



void func_003ADAA0(Fighter *f) {
    f->unkB130 = 0;
    switch (f->unk2E) {
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
        f->unkB130 = 0x2003;
        break;
    case 0x4A:
    case 0x4B:
        f->unkB130 = 0x2000;
        break;
    case 0x6A:
    case 0x6C:
        f->unkB130 = 0x202;
        break;
    case 0x6B:
        f->unkB130 = 0x402;
        break;
    }
}

void func_003ADB20(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        D_005F6D78[i][0] = 0;
        D_005F6D78[i][1] = 0;
    }
}

s32 func_003ADB58(s32 side, s32 member) {
    return D_005F6D78[side][member];
}

void func_003ADB70(s32 side, s32 member, s32 n) {
    D_005F6D78[side][member] = n;
}

s32 func_003ADB88(void) {
    return D_005F6D7C;
}

void func_003ADB90(s32 n) {
    D_005F6D7C = n;
}

void func_003ADB98(Fighter *f) {
    *(s16 *)((u8 *)f + 0xB068) = 0;
    *(s16 *)((u8 *)f + 0xB066) = 0;
    func_00363150(f);
}

void func_003ADBB0(Fighter *f) {
    func_003ADAA0(f);
    *(s16 *)((u8 *)f + 0xB068) = 0;
    *(s16 *)((u8 *)f + 0xB066) = 0;
    func_00363150(f);
}

s32 func_003ADBE8(s32 a, s32 b, s32 *out) {
    s32 r = 0;

    if (func_003ADA08(D_0043B968, 0x21) != 0 && a == D_005F5CBE && D_005F5CC0 >= 10 && b == 0xF) {
        *out = 7;
        r = 1;
    }
    return r;
}

void func_003ADC78(void) {
    s32 side;
    s32 m;
    s32 v;

    for (side = 0; side < 2; side++) {
        for (m = 0; m < 2; m++) {
            v = 0;
            if (func_003ADBE8(side, D_00440940[side][m].unk0, &v) != 0) {
                D_00440940[side][m].unk4 = v;
                func_003ADB70(side, m, 0);
            }
        }
    }
}

void func_003ADD60(s32 a) {
    if (a == 0 && D_0043B7F8[0] == 2) {
        return;
    }
    if (gFighters[0].unk402 == 0 || a != 0) {
        D_005F6D68[0] = 0;
    }
    if (gFighters[1].unk402 == 0 || a != 0) {
        D_005F6D68[1] = 0;
    }
}

void func_003ADDC8(s32 side, s32 v) {
    D_005F6D68[side] = v;
}

s32 func_003ADDD8(s32 side) {
    return D_005F6D68[side];
}

void func_003ADDE8(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        D_005F6D70[i] = 0;
    }
}

void func_003ADE18(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        Fighter *fs = gFighters;
        Fighter *f;
        u8 t;

        D_005F6D70[i] = 0;
        f = &fs[i];
        if (f->unk402 != 0) {
            continue;
        }
        if (D_005F5CB0[i] == 0) {
            continue;
        }
        if (D_0043B7F8[0] == 9) {
            continue;
        }
        D_005F6D70[i] = (D_005F5C40[i] >> 2) & 1;
        t = (D_00415718[func_003AFFB8(i, f->unk0)] >> 10) & 1;
        D_005F6D70[i] |= t;
    }
}

s32 func_003ADF30(s32 side) {
    if (D_0043B7F8[0] == 8) {
        return 0;
    }
    if (func_003AE3C0(&gFighters[side]) == 0) {
        return 0;
    }
    return (D_005F6D68[side] + D_005F6D70[side]) & 1;
}

