/* 0x358A38-0x35C2B0 (module battle_mode, docs/game_overview.md section 4; docs/systems/modes.md): the fight mode
 * Mode_Battle (mode 4) and its helpers. .rodata 0x4FB9C0-0x4FBB28 is its own (subsegment, every table included in
 * place). */
#include "common.h"

#include "fighter.h"
#include "gte.h"

/* A team's entry list in the team modes (D_0043B7F8 + 0x42 + side * 0x1E) [V offsets; I names]. */
typedef struct {
    /* 0x00 */ u8 id[8];      /* characters */
    /* 0x08 */ u8 val[8];     /* costumes */
    /* 0x10 */ s8 unk10[8];   /* 1 once the member fought, 2 when out (func_0035A1E8) */
    /* 0x18 */ u8 pad18[2];
    /* 0x1A */ u8 unk1A;
    /* 0x1B */ u8 n;          /* number of members */
    /* 0x1C */ u8 unk1C;
    /* 0x1D */ u8 unk1D;      /* won rounds */
} SelTeam;

/* A record of one team-mode match (the characters and costumes of both teams, id = chr * 8 + costume). */
typedef struct {
    /* 0x0 */ s16 a[2][2];
    /* 0x8 */ s16 b;
    /* 0xA */ s16 c;
    /* 0xC */ s16 d;
    /* 0xE */ s16 e;
} SelSlot;

/* A side's win counter in the versus modes. */
typedef struct {
    /* 0x0 */ u16 wins;
    /* 0x2 */ u16 pad2;
    /* 0x4 */ s16 unk4;
} VsRec;

/* The game setup (D_0043B7F8): +0 game type, the human / CPU sides, the match counters; the tail is used differently
 * per game type [V offsets; I names]. */
typedef struct {
    /* 0x00 */ u8 unk0;       /* game type */
    /* 0x01 */ u8 pad1;
    /* 0x02 */ u8 unk2;
    /* 0x03 */ u8 unk3;
    /* 0x04 */ u8 pad4[4];
    /* 0x08 */ u8 unk8;
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 padA[5];
    /* 0x0F */ u8 unkF;       /* set once the mode was set up */
    /* 0x10 */ u8 unk10;
    /* 0x11 */ u8 unk11;
    /* 0x12 */ u8 unk12;
    /* 0x13 */ u8 unk13;
    /* 0x14 */ u8 pad14[6];
    /* 0x1A */ u8 unk1A;      /* number of human players */
    /* 0x1B */ u8 unk1B;      /* human sides, bit per side */
    /* 0x1C */ u8 unk1C;      /* the human side */
    /* 0x1D */ u8 unk1D;      /* the CPU side */
    /* 0x1E */ u8 unk1E;
    /* 0x1F */ u8 unk1F;      /* match result kind (1, 2: a side won; 3: draw) [I] */
    /* 0x20 */ u8 unk20;      /* winner side */
    /* 0x21 */ u8 unk21;      /* loser side */
    /* 0x22 */ u8 pad22[2];
    /* 0x24 */ s32 unk24;     /* matches won in a row / stage number */
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ u8 pad38[8];
    /* 0x40 */ union {
        struct {
            u8 unk40;          /* current match record */
            u8 unk41;
            SelTeam team[2];
            SelSlot slot[15];
        } t;
        struct {
            s32 unk40;
            s32 unk44;
        } a;
        struct {
            VsRec rec[2];
            u16 unk4C;          /* draws */
            u8 pad4E[2];
            s32 unk50;
        } v;
    } u;
} GameSetup;

/* An opponent preset of D_00440A00 (5 bytes): the two characters, their costumes and the stage. */
typedef struct {
    s8 chr[2];
    s8 cost[2];
    s8 stage;
} Preset;

/* A character pair rule of D_005207F0 (5 bytes). */
typedef struct {
    s8 a;
    s8 pad1;
    s8 b;
    s8 pad3[2];
} PairRule;

/* A camera key: angles at 0..2, positions at 5..7 (9 words). */
typedef struct {
    s32 w[9];
} CamPos;

typedef struct {
    s32 w[4];
} HitPos;

/* A hit event (as func_0035BF10 receives it). */
typedef struct {
    /* 0x00 */ HitPos pos;
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s16 unk14;
    /* 0x16 */ u8 unk16;
    /* 0x17 */ u8 unk17;
} HitRec;

#define WRAP18(x) ((((x) & 0x20000) == 0) ? ((x) & 0x3FFFF) : ((x) | ~0x3FFFF))

extern Fighter gFighters[4];
extern s16 gMode;
extern s16 gSubMode;
extern u16 gFrameCount;
extern s32 gRoundNumber;
extern const s16 gSinTable[];
extern GameSetup D_0043B7F8;
extern TeamMember D_00440940[2][2];
extern Preset D_00440A00[];
extern u8 D_0043B990[];
extern s8 D_0043B9B8[];
extern s16 D_004FF690[];
extern const s16 D_004FB9C0[];
extern const char D_004FB9F8[];
extern const char D_004FBA10[];
extern const PairRule D_005207F0[];
extern CamPos D_00415808;
extern GteMatrix D_00436530;
extern s32 D_004333E4[];
extern u8 D_005F5C5A;
extern u16 D_005F5C74;
extern u16 D_005F5C80[2];
extern u16 D_005F5C82;
extern s8 D_005F5C98[2][2];
extern s8 D_005F5C99[3];
extern s8 D_005F5CA0[2][2];
extern s8 D_005F5CA1[3];
extern u16 D_005F5CA8[2];
extern s16 D_005F5CAA;
extern u16 D_005F5CB0[2];
extern u16 D_005F5CB2;
extern s16 D_005F5CB4;
extern u16 D_005F5CB6;
extern u16 D_005F5CBA;
extern u8 D_005F5CBE;
extern s32 D_005F5CC0;
extern s32 D_005F5CC4;
extern s16 D_005F5CC8[2];
extern u8 D_005F5CCC;
extern s32 D_005F5CD0;
extern s16 D_005F5CF0;
extern s32 D_005F5CF4;
extern s32 D_005F5EE0;
extern s16 D_005F64D8;
extern s32 D_005F6510;
extern s32 D_005F6668[2];
extern s32 D_005F666C;
extern s32 D_005F6678;
extern s32 D_005F6B14;
extern s16 D_005F6B18;
extern s16 D_005F6B20[2];
extern s16 D_005F6B28[2];
extern s16 D_005F72A0[];
extern CamPos D_005F72B0;

int rand(void);
u32 Math_Isqrt(u32 x);
void Bgm_Stop(void);
void Bgm_Play(s32 track);
void Snd_PlaySe(s32 id, s32 player);
s32 Text_Printf(const char *fmt, ...);
void Mode_Change(s32 mode);
void Hud_Init(GameSetup *g);
s32 Battle_UpdateFrame(void);
void HitLog_Add(s16 kind, s16 arg, HitRec *p, s32 d);
void HitLog_Reset(void);
void HitLog_NextFrame(void);
GteMatrix *Gte_MulMatrix(GteMatrix *m0, GteMatrix *m1);
GteMatrix *Gte_MulMatrix2(GteMatrix *m0, GteMatrix *m1);
void Gte_ApplyMatrixXyz(GteMatrix *a, s32 x, s32 y, s32 z, s32 *ox, s32 *oy, s32 *oz);
void func_00346600(void);
void func_003533E8(s32 n);
void func_0035CB70(s32 character);
void func_0035CBC0(s32 a, s32 b);
void func_0035CC68(s32 a, s32 b);
void func_0035CCE0(s32 i);
void func_003647E8(void);
s32 func_00366108(s32 dx, s32 dz);
void func_0036D010(s16 cpu1, s16 cpu2);
void func_0036D5D0(void);
s32 func_00374530(void);
s32 func_00374540(void);
void func_00375190(void);
void func_003753D0(void);
void func_00393708(s32 a);
s32 func_00393810(void);
s32 func_003985D8(s32 x);
void func_003992C8(s32 value);
void func_003A2758(s16 kind, HitRec *p, s32 flag, f32 f);
void func_003A2BC0(s16 side, s16 unused, HitRec *p, s32 d);
void func_003A6110(s16 side, s16 kind, HitRec *p);
void Mode_EnterEnding(s32 a, s32 b, s32 c);
void CharSet_Copy(void *a, void *b);
void func_003ADA18(s32 chr, void *buf);
void func_003ADB20(void);
void func_003ADB70(s32 side, s32 member, s32 n);
s32 func_003ADB88(void);
void func_003ADB90(s32 n);
void func_003ADC78(void);
void func_003ADD60(s32 a);
void func_003ADDC8(s32 side, s32 member);
s32 func_003ADDD8(s32 side);
void func_003ADE18(void);
void func_003ADFA8(void);
void func_003ADFB0(void);
void func_003AE170(s32 *c);
void func_003AE210(GameSetup *g);
void func_003AE3E0(s32 a);
s32 func_003AE420(s32 side, s32 member);
s32 func_003AE608(s32 a, s32 b);
s32 func_003AE670(void);
s32 func_003AE698(void);
void func_003AE738(s32 a, s32 b);
void func_003B0260(void);
void func_003B02C8(s32 a);
s32 func_003B1198(void);
s32 func_003B1330(void);
s32 func_003B1378(void);
s32 func_003B15D0(s32 a);
s32 func_003B5328(void);
void func_003B5CA8(void);
void func_003B6218(void);
s32 func_003B6220(void);
void func_003B6C00(GameSetup *g);
s32 func_003B6C68(GameSetup *g);
void func_003B7A58(void);
void func_003B7A68(void);
void Pause_Update(void);
void func_003BA4B8(s32 a);
void func_003BA4D8(s32 side);
void func_003BA528(s32 a);
void func_003BA548(void);
s16 Game_UpdateContinue(s32 a, s32 b);
void func_003BA818(s32 a, s32 b);
void func_003BAA90(void);
void func_003BB8E8(s32 side);
void func_003BF8C8(void);
void func_003BFA58(void);
void func_003BFCF8(void);
void func_003BFDF8(s32 a);
s32 func_003BFEC0(void);
s32 func_003C0210(s32 chr, s32 c);
void Arcade_BuildOpponents(s32 a);
void Team_BuildRandom(void);
void func_003C1240(void);
s32 func_003C12D0(s32 a, s32 b);
void func_003C18B8(void);
void func_003C1EC0(void);
s32 func_003C45B0(GameSetup *g);
void func_003C4868(void);
void func_003C4AE0(void);
s32 Results_DrawSurvival(s32 a);
void func_003C9358(s32 a);
void func_003CA418(s32 a);
void func_003CD030(s32 n, s32 c0, s32 k0, s32 c1, s32 k1, s32 side, s32 time);
s32 func_003CD1E8(s32 time, s32 c0, s32 c1);
s32 func_003CD298(void);
s32 func_003CD440(s32 side, s32 time, s32 c);
void func_003CD6C0(s32 n, s32 c0, s32 k0, s32 c1, s32 k1);
s32 func_003CF230(s32 chr, s32 costume);
s32 func_003D7DA8(void *pos);
s32 func_003F0008(void);
void func_003F1878(s32 a);

void func_00358A38(void);
s32 func_00358A98(s8 *p);
void func_00358B08(GameSetup *g);
void func_00358B40(GameSetup *g);
s32 func_00358B48(GameSetup *p, s32 k, s32 id, s32 val);
void func_00358BD8(GameSetup *s, s32 side);
void func_00358DA8(GameSetup *s);
void func_00358ED0(GameSetup *g);
void func_00358ED8(GameSetup *g);
void func_00358F20(GameSetup *g);
void func_00358F40(GameSetup *g);
void func_00358F60(GameSetup *g);
void func_00358F80(s32 n);
void func_00359038(GameSetup *g);
void func_00359380(GameSetup *g);
s32 func_003593B0(SelTeam *t);
void func_003593F0(GameSetup *g);
void func_00359698(GameSetup *g);
void func_003596D8(GameSetup *g);
void func_00359B98(GameSetup *g);
void func_00359C00(GameSetup *g);
void func_00359C08(GameSetup *g);
void func_00359C40(GameSetup *g);
void func_00359FB8(GameSetup *g);
s32 func_0035A1E8(GameSetup *s);
void func_0035A280(GameSetup *g);
void func_0035A310(GameSetup *g);
void func_0035A330(GameSetup *g);
void func_0035A458(GameSetup *g);
void func_0035A460(GameSetup *g);
void func_0035A468(GameSetup *g);
void func_0035A4F0(s32 n);
void func_0035A518(void);
s32 func_0035A520(void);
s32 func_0035A530(void);
s32 func_0035A558(void);
s32 func_0035A568(void);
s32 func_0035A578(void);
s32 func_0035A588(void);
s32 func_0035A598(void);
s32 func_0035A5A8(void);
s32 func_0035A5D0(void);
s32 func_0035A648(void);
s32 func_0035A670(void);
void Mode_Battle(void);
s32 func_0035B488(void);
void func_0035B4B0(s32 *c, GteMatrix *m);
void func_0035B690(s32 *p, s32 *out);
void func_0035B760(s32 *p, s32 *out);
void func_0035B7A8(CamPos *c);
void func_0035B858(CamPos *c);
void func_0035B8B8(s32 *p);
void func_0035B9A8(Fighter *a, Fighter *b);
PORT_RET(s16) func_0035B9E8(s16 i);
PORT_RET(s16) func_0035BA00(s16 i);
void func_0035BA18(s32 i);
void func_0035BA58(Fighter *f, s32 unused, s32 *v, s32 *out);
void func_0035BB58(Fighter *f, s32 arg);
void func_0035BD20(Fighter *f, s32 arg);
void func_0035BF10(s16 kind, s16 arg, HitRec *p, s32 d);
void func_0035BF70(s32 kind, HitRec *p, s32 d);
void func_0035BFF8(s32 arg, HitRec *p);
void func_0035C070(s32 kind, s32 arg, HitRec *p);
void func_0035C0D8(s16 dmg, s32 unused, s16 side, HitRec *p);
void func_0035C230(void);
void func_0035C238(void);
void func_0035C240(HitPos *pos);


RODATA_ALIGN16();
void func_00358A38(void) {
    func_003753D0();
    func_00375190();
    Bgm_Stop();
    if (func_003AE670() != 0) {
        gFighters[0].unk127 = 0;
        gFighters[1].unk127 = 0;
    }
    D_005F5C82 = 0;
    D_005F5C80[0] = 0;
    gSubMode = 0;
}
s32 func_00358A98(s8 *p) {
    s8 buf[0x22];
    s32 i;
    s32 n = 0;
    for (i = 0; i < 0x22; i++) {
        if (p[i] != 0) {
            buf[n] = i;
            n++;
        }
    }
    return buf[rand() % n];
}
void func_00358B08(GameSetup *m) {
    if (m->unkF == 0) {
        Arcade_BuildOpponents(D_005F5CB0[0] == 0);
        func_003C1240();
    }
}
void func_00358B40(GameSetup *m) {
    func_00358B08(m);
}
s32 func_00358B48(GameSetup *p, s32 k, s32 id, s32 val) {
    s32 i;
    s32 r = 0;
    for (i = 0; i < p->u.t.team[k].n; i++) {
        if (p->u.t.team[k].id[i] == id) {
            if (p->u.t.team[k].val[i] == val) r = 1;
            break;
        }
    }
    return r;
}
void func_00358BD8(GameSetup *s, s32 side) {
    s8 buf[0x30];
    s32 i;
    s8 chr;
    s32 c;
    s32 k;
    CharSet_Copy(D_0043B990, buf);
    for (i = 0; i < s->u.t.team[side].n; i++) {
        chr = func_00358A98(buf);
        func_003ADA18(chr, buf);
        c = rand() % 5;
        k = chr * 8 + c;
        if (c == 2 || c == 3) {
            if (D_004FF690[k] < 0 || D_0043B9B8[chr] == 0) {
                c = func_003C0210(chr, c);
            }
        } else if (c >= 4) {
            if (D_004FF690[k] < 0 || func_003AE608(k, c) == 0) {
                c = func_003C0210(chr, c);
            }
        }
        if (func_00358B48(s, side ^ 1, chr, c)) {
            c = func_003C0210(chr, c);
        }
        s->u.t.team[side].id[i] = chr;
        s->u.t.team[side].val[i] = c;
    }
}
void func_00358DA8(GameSetup *s) {
    s32 i, j;
    s->unk24 = 0;
    s->u.t.unk40 = 0;
    if (D_005F5CB0[0] == 0) {
        func_00358BD8(s, 0);
        s->u.t.team[0].unk1A = 0;
    } else {
        if (*(volatile u16 *)&D_005F5CB2 == 0) /* fake */ {
            func_00358BD8(s, 1);
            s->u.t.team[1].unk1A = 0;
        }
    }
    for (i = 0; i < 8; i++) {
        s->u.t.team[0].unk10[i] = 0;
        s->u.t.team[1].unk10[i] = 0;
    }
    for (i = 0; i < 15; i++) {
        for (j = 0; j < 4; j++) {
            s->u.t.slot[i].a[j >> 1][j & 1] = -1;
        }
        s->u.t.slot[i].b = -1;
        s->u.t.slot[i].c = -1;
        s->u.t.slot[i].d = -1;
        s->u.t.slot[i].e = -1;
    }
}
void func_00358ED0(GameSetup *m) {
    func_00358B08(m);
}
void func_00358ED8(GameSetup *m) {
    s32 i;
    func_00358B08(m);
    for (i = 0; i < 8; i++) {
        D_005F72A0[i] = -1;
    }
}
void func_00358F20(GameSetup *m) {
    if (m->unkF == 0) {
        func_003C1240();
    }
}
void func_00358F40(GameSetup *m) {
    func_00358B08(m);
    func_003AE3E0(1);
}
void func_00358F60(GameSetup *m) {
    if (m->unkF == 0) {
        Arcade_BuildOpponents(D_005F5CB0[0] == 0);
    }
}
INCLUDE_RODATA("asm/nonmatchings/battle_mode", D_004FB9C0);
void func_00358F80(s32 n) {
    s32 t;
    if (n < 0) {
        Bgm_Stop();
        return;
    }
    if ((u32)n >= 0x1A) n = 0;
    if (D_0043B7F8.unk0 == 8) {
        func_003BFCF8();
        if (func_003BFEC0() != 0) t = 0xC;
        else t = rand() % 14 + 9;
        func_003F1878(t);
        Bgm_Play(t);
    } else {
        Bgm_Play(D_004FB9C0[n]);
    }
}
void func_00359038(GameSetup *s) {
    u8 side, other, m, n;
    if (s->unk1A == 1) {
        D_005F5CA8[s->unk1D] = 1;
        for (side = 0, other = 1; side < 2; side++, other--) {
            if (D_005F6668[side] != 1) continue;
            if (func_003ADB88()) {
                func_003ADB70(side, 0, 0);
                func_003ADB70(side, 1, 0);
            } else {
                func_003ADB70(side, 0, 1);
                func_003ADB70(side, 1, 1);
            }
            D_00440940[side][0].unk0 = D_00440A00[s->unk24].chr[0];
            D_00440940[side][0].unk4 = D_00440A00[s->unk24].cost[0];
            D_00440940[side][1].unk0 = D_00440A00[s->unk24].chr[1];
            D_00440940[side][1].unk4 = D_00440A00[s->unk24].cost[1];
            for (m = 0; m < 2; m++) {
                for (n = 0; n < 2; n++) {
                    if (D_00440940[other][m].unk0 == D_00440940[side][n].unk0 &&
                        D_00440940[other][m].unk4 == D_00440940[side][n].unk4) {
                        D_00440940[side][n].unk4 = func_003C0210(D_00440940[other][m].unk0, D_00440940[other][m].unk4);
                        func_003ADB70(side, n, 0);
                        D_00440A00[s->unk24].cost[n] = D_00440940[side][n].unk4;
                    }
                }
            }
        }
        D_005F5CB6 = D_00440A00[s->unk24].stage;
    }
    if (s->unk1A < 2) {
        if (s->unk24 == 7) s->unk1E = 0;
        else s->unk1E = D_005F5C5A;
    } else {
        s->unk1E = D_005F5C5A;
        D_005F5CB6 = func_003C12D0(0, -1);
    }
}
void func_00359380(GameSetup *m) {
    func_00359038(m);
    m->unk1E = D_005F5C5A;
}
s32 func_003593B0(SelTeam *m) {
    s32 i;
    for (i = 0; i < m->n; i++) {
        if (m->unk10[i] == 0) return i;
    }
    return -1;
}
void func_003593F0(GameSetup *s) {
    s32 side, m, k, sw;
    SelTeam *t;
    SelSlot *slots;
    u8 kind = s->unk1A;
    if (kind == 1) D_005F5CA8[s->unk1D] = kind;
    for (side = 0; side < 2; side++) {
        t = &s->u.t.team[side];
        for (m = 0; m < 2; m++) {
            k = func_003593B0(t);
            if (func_003AE420(side, m) != 0) {
                if (k == -1) {
                gFighters[side].unk402 = 1;
                func_003ADDC8(side, m ^ 1);
                func_003ADB70(side, m, 1);
                break;
                }
            if (s->unk24 != 0) {
                func_003ADDC8(side, m);
                func_003ADB70(side, m, 0);
            }
            D_00440940[side][m].unk0 = t->id[k];
            D_00440940[side][m].unk4 = t->val[k];
            t->unk10[k] = 1;
            if ((s->unk1B >> side) & 1) func_0035CB70(t->id[k]);
                continue;
            }
            func_003ADB70(side, m, 1);
        }
    }
    for (side = 0; side < 2; side++) {
        sw = func_003ADDD8(side);
        for (m = 0; m < 2; m++) {
            s32 j = m ^ sw;
            s->u.t.slot[s->u.t.unk40].a[side][j] = (u16)D_00440940[side][j].unk4 + (u16)D_00440940[side][j].unk0 * 8;
            if (gFighters[side].unk402 != 0) break;
        }
    }
    s->u.t.unk40++;
    D_005F5CB6 = func_003C12D0(0, -1);
    s->unk1E = 0;
}
void func_00359698(GameSetup *m) {
    func_00359038(m);
    if (m->unk24 == 7) m->unk1E = 0;
    else m->unk1E = 1;
}
/* NON_MATCHING: pair-table loop: the rule pointer is copied (move v0,v1) and the base sits in a1 in the original (about 8 lines) */
#if 0
/* NON_MATCHING: pair-table loop: the rule pointer is copied (move v0,v1) and the base sits in a1 in the original (about 8 lines) */
void func_003596D8(GameSetup *s) {
    s8 avail[0x30];
    s8 pairs[0x4C][2];
    s32 cpu = s->unk1D;
    s32 pl = s->unk1C;
    s32 i, n, k, id, flag;
    s32 c0, c1;
    s32 id0, id1;
    s32 p0, p1;

    D_005F5CA8[cpu] = 1;
    CharSet_Copy(D_0043B990, avail);
    if ((u32)s->unk24 < 3) {
        func_003ADA18(0xE, avail);
        func_003ADA18(0x14, avail);
        func_003ADA18(0x1D, avail);
        func_003ADA18(0xF, avail);
    }
    if (rand() & 0xF) {
        for (i = 0; i < 8; i++) {
            if (D_005F72A0[i] >= 0) func_003ADA18(D_005F72A0[i], avail);
        }
        c0 = (s16)func_00358A98(avail);
        func_003ADA18(c0, avail);
        c1 = (s16)func_00358A98(avail);
    } else {
        n = 0;
        for (i = 0; i < 0x4C; i++) {
            const PairRule *e = &D_005207F0[i];
            c0 = e->a;
            c1 = e->b;
            if (c0 != 0x22 && c1 != 0x22 && avail[c0] != 0 && avail[c1] != 0) {
                pairs[n][0] = c0;
                pairs[n][1] = c1;
                n++;
            }
        }
        k = rand() % n;
        c0 = pairs[k][0];
        c1 = pairs[k][1];
    }
    flag = 0;
    D_00440940[cpu][0].unk0 = c0;
    D_00440940[cpu][1].unk0 = c1;
    func_003ADB70(cpu, 0, 0);
    func_003ADB70(cpu, 1, 0);
    p0 = D_00440940[pl][0].unk0;
    p1 = D_00440940[pl][1].unk0;
    id0 = p0 * 8 + D_00440940[pl][0].unk4;
    id1 = p1 * 8 + D_00440940[pl][1].unk4;
    if (p0 == 0 || p1 == 0) flag = 1;
    for (i = 0; i < 2; i++) {
        k = rand() % 5;
        c0 = (s16)D_00440940[cpu][i].unk0;
        c1 = (s16)D_00440940[cpu][i ^ 1].unk0;
        if (c0 == 0x1D && c1 == 0x1E) k = 4;
        else if (c0 == 0xB && c1 == 7) k = 1;
        else if (c0 == 0xB && c1 == 0xD) k = 2;
        else if (c0 == 0xB && c1 == 0) k = 2;
        else if (c0 == 0x1D) k = 0;
        else if (c0 == 0xB) k = flag ? 2 : k;
        id = c0 * 8 + k;
        if (k == 2 || k == 3) {
            if (D_004FF690[id] < 0 || D_0043B9B8[c0] == 0) {
                k = func_003C0210(c0, k);
                id = c0 * 8 + k;
            }
        } else if (k >= 4) {
            if (D_004FF690[id] < 0 || func_003AE608(c0, k) == 0) {
                k = func_003C0210(c0, k);
                id = c0 * 8 + k;
            }
        }
        if (id0 == id || id1 == id) k = func_003C0210(c0, k);
        D_00440940[cpu][i].unk4 = k;
    }
    {
        s32 j = (s->unk24 * 2) & 7;
        c0 = (s16)D_00440940[cpu][0].unk0;
        c1 = (s16)D_00440940[cpu][1].unk0;
        D_005F72A0[j] = c0;
        D_005F72A0[j + 1] = c1;
    }
    D_005F5CB6 = func_003C12D0(0, -1);
    s->unk1E = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_mode", func_003596D8);
#endif
void func_00359B98(GameSetup *m) {
    D_005F5CA8[m->unk1D] = 1;
    if (func_00374530() != 0) {
        D_005F5CB6 = func_00374540();
    } else {
        D_005F5CB6 = func_003C12D0(0, -1);
    }
    m->unk1E = 0;
}
void func_00359C00(GameSetup *m) {
    func_00359038(m);
}
void func_00359C08(GameSetup *m) {
    func_00359038(m);
    D_005F5CB6 = 0x14;
    m->unk1E = 0xA;
}
void func_00359C40(GameSetup *s) {
    u8 type;
    switch (s->unk1A) {
    case 1:
        if (s->unk20 == s->unk1C) {
            s->unk30 = 0;
            s->unk34 += s->unk2C;
            func_003CD030(s->unk24, D_005F5C98[s->unk20][0], D_005F5CA0[s->unk20][0], D_005F5C99[s->unk20 * 2], D_005F5CA1[s->unk20 * 2], s->unk20, s->unk2C);
            s->unk2C = 0;
            func_003CD6C0(s->unk24, D_00440940[s->unk21][0].unk0, D_00440940[s->unk21][0].unk4, D_00440940[s->unk21][1].unk0, D_00440940[s->unk21][1].unk4);
            s->unk24++;
            D_005F5CCC = s->unk24;
            if (D_005F5CCC >= 7) D_005F5CCC = 7;
            if ((u32)s->unk24 >= 8) {
                func_003B6218();
                if (D_0043B7F8.unk0 != 8) {
                    s->u.a.unk40 = s->unk20;
                    s->u.a.unk44 = D_005F5C98[s->unk20][0] * 4 + D_005F5CA0[s->unk20][0];
                    if (D_0043B7F8.unk0 == 3) func_003CD298();
                }
                if (D_0043B7F8.unk0 != 8) {
                    func_0035CB70(D_005F5C98[s->unk20][0]);
                    if (D_0043B7F8.unk0 != 5 && D_0043B7F8.unk0 != 6) func_0035CB70(D_005F5C99[s->unk20 * 2]);
                }
                D_005F5CB0[s->unk20] = 0;
                D_005F5C74 = 0;
                if (D_0043B7F8.unk0 == 0) {
                    D_005F5CF0 = 1;
                    gSubMode = 9;
                } else {
                    gSubMode = 0xC;
                }
                if (D_0043B7F8.unk0 == 3) gSubMode = 0x11;
                if (D_0043B7F8.unk0 == 5) gSubMode = 0x11;
            } else {
                gSubMode = 9;
            }
        } else {
            s->unk30 = s->unk2C;
            D_005F5CB0[s->unk21] = 0;
            D_005F5C80[s->unk21] = 0;
            func_0035CB70(D_005F5C98[s->unk21][0]);
            if (D_0043B7F8.unk0 != 5) func_0035CB70(D_005F5C99[s->unk21 * 2]);
            gSubMode = 0xA;
        }
        break;
    case 2:
        s->unk28++;
        func_0035CBC0(D_005F5C98[s->unk20][0], D_005F5C98[s->unk21][0]);
        if (D_0043B7F8.unk0 != 5) func_0035CBC0(D_005F5C99[s->unk20 * 2], D_005F5C99[s->unk21 * 2]);
        if (s->unk0 != 1) func_003BA528(s->unk21);
        D_005F5CC8[s->unk21] = 0;
        func_003BA4D8(s->unk20);
        gSubMode = 9;
        break;
    case 0:
        break;
    }
}
void func_00359FB8(GameSetup *v) {
    u8 kind = v->unk1F;
    u8 w = v->unk20;
    u8 l = v->unk21;
    u32 n;
    s32 i;
    v->unk28++;
    switch (kind) {
    case 1:
    case 2:
        n = v->u.v.rec[w].wins;
        n = n + 1;
        if (n > 0xFFFF) n = 0xFFFF;
        v->u.v.rec[w].wins = n;
        func_0035CBC0(D_005F5C98[w][0], D_005F5C98[l][0]);
        if (D_0043B7F8.unk0 != 6) func_0035CBC0(D_005F5C99[w * 2], D_005F5C99[l * 2]);
        func_0035CB70(D_005F5C98[w][0]);
        if (D_0043B7F8.unk0 != 6) func_0035CB70(D_005F5C99[w * 2]);
        func_003BA4D8(v->unk20);
        break;
    case 3:
        n = v->u.v.unk4C;
        n = n + 1;
        if (n > 0xFFFF) n = 0xFFFF;
        v->u.v.unk4C = n;
        func_0035CC68(D_005F5C98[0][0], D_005F5C98[1][0]);
        if (D_0043B7F8.unk0 != 6) func_0035CC68(D_005F5C98[0][1], D_005F5C98[1][1]);
        func_003BA4B8(D_005F5CBE);
        break;
    }
    gSubMode = 0;
    gMode = 4;
    v->u.v.unk50 = 0xF0;
    func_003BFA58();
    for (i = 0; i < 2; i++) {
        v->u.v.rec[i].unk4 = 0x180;
        D_005F5C80[i] = 0;
        if (func_003AE670()) {
            gFighters[i].unk127 = 0;
            if (D_0043B7F8.unk0 != 6) gFighters[i + 2].unk127 = 0;
        }
        func_003AE738(1, i);
    }
}
s32 func_0035A1E8(GameSetup *s) {
    s32 r = 0;
    s32 i, j, found, n;
    SelTeam *t;
    for (i = 0; i < 2; i++) {
        t = &s->u.t.team[i];
        n = t->n;
        found = 0;
        for (j = 0; j < n; j++) {
            if ((u8)t->unk10[j] != 2) found |= 1;
        }
        if (!found) {
            D_005F5CB4 = 1;
            r |= 1 << i;
        }
    }
    return r;
}
void func_0035A280(GameSetup *s) {
    s->unk24++;
    gSubMode = 9;
    switch (s->unk1F) {
    case 1:
    case 2:
        s->u.t.team[s->unk20].unk1D++;
        break;
    case 3:
        s->u.t.unk41++;
        break;
    }
    if (func_0035A1E8(s)) gSubMode = 0x14;
}
void func_0035A310(GameSetup *m) {
    func_00359C40(m);
    func_003C9358(0);
}
void func_0035A330(GameSetup *s) {
    s32 a;
    s32 side;
    if (s->unk20 == s->unk1C) {
        s->unk30 = 0;
        s->unk34 += s->unk2C;
        func_003BA4D8(s->unk1C);
        side = s->unk1C;
        a = func_003CF230(D_00440940[side][0].unk0, D_00440940[side][0].unk4);
        D_005F5CC4 = func_003CD1E8(D_005F5CC0, a, func_003CF230(D_00440940[side][1].unk0, D_00440940[side][1].unk4));
        s->unk24++;
        gSubMode = 9;
    } else {
        D_005F5CB0[s->unk21] = 0;
        D_005F5C80[s->unk21] = 0;
        func_0035CB70(D_005F5C98[s->unk21][0]);
        func_0035CB70(D_005F5C99[s->unk21 * 2]);
        gSubMode = 0x12;
    }
}
void func_0035A458(GameSetup *m) {
    func_00359C40(m);
}
void func_0035A460(GameSetup *m) {
    func_00359C40(m);
}
void func_0035A468(GameSetup *m) {
    s32 mode;
    func_003B6218();
    D_005F5CB0[m->unk20] = 0;
    D_005F5C80[m->unk20] = 0;
    switch (m->unk1A) {
    case 2:
        func_003BA528(m->unk21);
    case 0:
    case 1:
    default:
        mode = 10;
    }
    D_005F5C74 = 0;
    gMode = mode;
    gSubMode = 0;
}
void func_0035A4F0(s32 n) {
    if (n != 0x18) D_005F5CB4 = 1;
    gSubMode = n;
    D_005F5EE0 = 1;
    D_005F6510 = 0;
}
void func_0035A518(void) {
    func_0035A4F0(0x17);
}
s32 func_0035A520(void) {
    return gSubMode != 9;
}
s32 func_0035A530(void) {
    s32 r = 0;
    if (gMode == 4) r = gSubMode == 7 || gSubMode == 8;
    return r;
}
s32 func_0035A558(void) { return gSubMode == 0xD; }
s32 func_0035A568(void) { return gSubMode == 0xF; }
s32 func_0035A578(void) { return gSubMode == 0xB; }
s32 func_0035A588(void) { return gSubMode == 9; }
s32 func_0035A598(void) { return gSubMode == 2; }
s32 func_0035A5A8(void) {
    s32 r = 0;
    if (gSubMode == 8 || gSubMode == 0x13) r = 1;
    return r;
}
s32 func_0035A5D0(void) {
    s32 r = 0;
    if ((func_0035A578() && D_005F6510 >= 2) || (func_0035A568() && D_005F6510 < 2) || (func_0035A588() && D_005F6510 >= 2)) r = 1;
    return r;
}
s32 func_0035A648(void) {
    s32 r = 0;
    if (gMode == 4) r = gSubMode == 7 || gSubMode == 8;
    return r;
}
s32 func_0035A670(void) {
    s32 r = 0;
    if (gSubMode == 0x17 || gSubMode == 0x19 || gSubMode == 0x18) r = 1;
    return r;
}
INCLUDE_RODATA("asm/nonmatchings/battle_mode", D_004FB9F8);
INCLUDE_RODATA("asm/nonmatchings/battle_mode", D_004FBA10);
INCLUDE_RODATA("asm/nonmatchings/battle_mode", jtbl_004FBA30);
INCLUDE_RODATA("asm/nonmatchings/battle_mode", jtbl_004FBAA0);
INCLUDE_RODATA("asm/nonmatchings/battle_mode", jtbl_004FBAD0);
INCLUDE_RODATA("asm/nonmatchings/battle_mode", jtbl_004FBB00);
/* NON_MATCHING: register allocation and scheduling (about 200 real lines; the sub-state constants live in a1) */
#if 0
/* NON_MATCHING: register allocation and scheduling (about 200 real lines; the sub-state constants live in a1) */
void Mode_Battle(void) {
    GameSetup *g = &D_0043B7F8;
    s32 i;
    s32 r;
    s32 p1, p2;
    s32 res;
    u8 type;
    u16 sub = gSubMode;

    if (sub - 7 < 2U) {
        if (D_0043B7F8.unk0 == 9 && func_003B1198() != 0) {
            func_0035A4F0(0x19);
        }
        if (func_003B1378() != 0) {
            func_0035A4F0(0x17);
        }
        sub = gSubMode;
    }
    switch ((s16)sub) {
    case 0:
        func_00393708(D_005F5CD0);
        if (g->unkF == 0) {
            func_003ADB20();
            func_003ADB90(1);
        }
        i = 0;
        if (g->unk0 == 9) {
            func_003B7A58();
            Team_BuildRandom();
            D_005F5CB2 = 1;
            gSubMode = sub = 1;
            D_005F6678 = 1;
            D_005F5CA8[0] = 0;
            D_005F5CAA = 0;
            D_005F5CB0[0] = 1;
            D_005F6B14 = 0;
            for (; i < 4; i++) {
                func_003BA818(i, 0);
            }
            D_005F5CF0 = 0;
            goto state1;
        }
        for (; i < 4; i++) {
            func_003BA818(i, func_003B15D0(i));
        }
        g->unk12 = 4;
        g->unk13 = 1;
        gMode = 5;
        gSubMode = sub = 0;
        func_003CA418(5);
        if (func_003AE670() != 0) {
            func_003ADFB0();
            return;
        }
        if (func_003AE698() != 0) {
            func_003C4868();
            return;
        }
        break;
    case 1:
        D_005F5CF0 = 0;
    state1:
        func_003AE3E0(0);
        func_003ADD60(1);
        func_003C18B8();
        switch (g->unk0) {
        case 0:
            func_00358B08(g);
            break;
        case 1:
            func_00358B40(g);
            break;
        case 2:
            func_00358DA8(g);
            break;
        case 3:
            func_00358ED0(g);
            break;
        case 4:
            func_00358ED8(g);
            break;
        case 7:
            func_00358F20(g);
            break;
        case 5:
        case 6:
            func_00358F40(g);
            break;
        case 8:
            func_00358F60(g);
            break;
        case 9:
            break;
        }
        g->unkF = 1;
        gSubMode = sub = 2;
    case 2:
        p1 = 0;
        func_003B6218();
        Bgm_Stop();
        p2 = 0;
        D_005F666C = 1;
        D_005F6668[0] = 1;
        if (D_005F5CB0[0] != 0) {
            D_005F6668[0] = 0;
            p1 = 1;
        }
        if (D_005F5CB2 != 0) {
            D_005F666C = 0;
            p2 = 1;
        }
        g->unk1B = p1 | (p2 << 1);
        g->unk1A = p1 + p2;
        D_005F5CBA = g->unk1B;
        switch (g->unk1B) {
        case 1:
            g->unk1D = 1;
            g->unk1C = 0;
            break;
        case 0:
        case 3:
            g->unk1C = 0;
            g->unk1D = 0;
            break;
        case 2:
            g->unk1C = 1;
            g->unk1D = 0;
            break;
        }
        g->unk1F = 0;
        g->unk2C = g->unk30;
        g->unk20 = 0;
        g->unk21 = 0;
        func_003ADD60(0);
        switch (g->unk0) {
        case 0:
        case 9:
            func_00359038(g);
            break;
        case 1:
            func_00359380(g);
            break;
        case 2:
            func_003593F0(g);
            break;
        case 3:
            func_00359698(g);
            break;
        case 4:
            func_003596D8(g);
            break;
        case 7:
            func_00359B98(g);
            break;
        case 5:
        case 6:
            func_00359C00(g);
            break;
        case 8:
            func_00359C08(g);
            break;
        }
        func_003AE210(g);
        func_003ADC78();
        func_003BF8C8();
        D_005F6510 = gFrameCount;
        Hud_Init(g);
        gSubMode = sub = 3;
    case 3:
        if (func_003C45B0(g) != 0) gSubMode = sub = 4;
        break;
    case 4:
        func_0036D010(D_005F5CA8[0], D_005F5CAA);
        gSubMode = sub = 5;
    case 5:
        func_00358F80(D_005F5CB6);
        i = 0;
        if (g->unk0 != 9) {
            for (; i < 4; i++) {
                func_003BA818(i, func_003B15D0(i));
            }
        } else {
            for (; i < 4; i++) {
                func_003BA818(i, 0);
            }
        }
        D_005F64D8 = 0;
        gSubMode = sub = 6;
        func_003992C8(0);
        break;
    case 6:
        D_005F5C74 = 1;
        func_003ADE18();
        func_003B5CA8();
        func_003BB8E8(0);
        func_003BB8E8(1);
        func_0036D5D0();
        D_005F5CB4 = 0;
        g->unk11 = 0;
        func_003C1EC0();
        if (g->unk0 != 9 && (u32)(D_0043B7F8.unk0 - 5) >= 2 && gRoundNumber == 1) {
            gSubMode = sub = 7;
            D_005F6510 = 5;
            D_005F64D8 = 0;
            D_005F5C74 = 0;
            break;
        }
        gSubMode = sub = 8;
        func_003533E8(0);
        break;
    case 7:
        if (func_003B5328() != 0) {
            gSubMode = sub = 8;
            func_003533E8(0);
            D_005F5C74 = 1;
        }
        if (D_005F6510 < 0) {
            if (g->unk2 != 0 && func_003B1330() != 0) {
                gSubMode = sub = 0xE;
                goto state14;
            }
        } else {
            D_005F6510--;
        }
        break;
    case 8:
        if (g->unk2 != 0 && func_003B1330() != 0) {
            gSubMode = sub = 0xE;
            goto state14;
        }
        if (D_0043B7F8.unk0 == 8) {
            r = func_003F0008();
        } else {
            r = Battle_UpdateFrame();
        }
        if (g->unk0 == 9) {
            if (func_00393810() == 0) D_005F6B14++;
            if (r >= 3 || D_005F6B14 >= 0xE11) {
                gSubMode = sub = 0x10;
                func_00346600();
                break;
            }
        }
        if (D_0043B7F8.unk0 != 7) Pause_Update();
        if (r < 5) break;
        if (r == 6) {
            gSubMode = sub = 0x16;
            break;
        }
        if (D_0043B7F8.unk0 != 8) func_003647E8();
        if (g->unk1F == 0) {
            gSubMode = sub = 6;
            D_005F64D8 = 0;
            break;
        }
        switch (g->unk0) {
        case 0:
            func_00359C40(g);
            break;
        case 1:
        case 6:
            func_00359FB8(g);
            break;
        case 2:
            func_0035A280(g);
            break;
        case 3:
            func_0035A310(g);
            break;
        case 4:
            func_0035A330(g);
            break;
        case 7:
            func_0035A458(g);
            break;
        case 5:
            func_0035A460(g);
            break;
        case 8:
            func_0035A468(g);
            break;
        case 9:
            break;
        }
        func_003BFDF8(g->unk1A);
        func_003BAA90();
        if (gSubMode == 9) {
            func_003C1EC0();
            D_005F6510 = 0;
            D_005F5EE0 = 1;
        }
        Bgm_Stop();
        break;
    case 9:
        Battle_UpdateFrame();
        if (++D_005F6510 >= 3) {
            func_003992C8(1);
            func_003ADFA8();
            if (D_005F5CF0 != 0) {
                s32 w = (s16)g->u.a.unk40;
                Mode_EnterEnding(1, D_00440940[w][0].unk0 * 8 + D_00440940[w][0].unk4, D_00440940[w][1].unk0 * 8 + D_00440940[w][1].unk4);
            } else {
                gSubMode = sub = 2;
            }
        }
        break;
    case 10:
        if (g->unk9 == 1) {
            func_003ADB20();
            func_003B0260();
        }
        func_003ADB90(0);
        Bgm_Play(8);
        Snd_PlaySe(0x1055, 0);
        func_003BA548();
        g->unk10 = 1;
        gSubMode = sub = 0xB;
        D_005F6B18 = 0;
        D_005F6510 = 0;
    case 11:
        Battle_UpdateFrame();
        res = D_005F6B18;
        if (res == 0) {
            res = D_005F6B18 = Game_UpdateContinue(g->unk21, g->unk20);
        } else {
            D_005F5EE0 = 1;
            D_005F6510++;
        }
        switch ((s16)res) {
        case 0:
            break;
        case 1:
            if (D_005F6510 >= 3) {
                gSubMode = sub = 2;
                goto end11;
            }
            break;
        case 2:
            if (D_005F6510 >= 3) {
                Bgm_Stop();
                gSubMode = sub = 0;
            end11:
                func_003ADFA8();
                func_003992C8(1);
                g->unk10 = 0;
                func_0035CCE0(1);
                func_003BA4B8(g->unk20);
            }
            break;
        default:
            gSubMode = sub = 0xC;
            g->unk10 = 0;
            func_003B02C8(g->unk21);
            break;
        }
        break;
    case 12:
        D_005F5CB0[g->unk21] = 0;
        D_005F5CC8[g->unk21] = 0;
        Bgm_Stop();
        switch (D_0043B7F8.unk0) {
        case 2:
        case 4:
        case 8:
            D_005F6510 = 0x96;
            break;
        default:
            D_005F6510 = 0;
            Snd_PlaySe(0x1056, 0);
            break;
        }
        gSubMode = sub = 0xD;
    case 13:
        type = D_0043B7F8.unk0;
        if (type == 2) {
            func_003ADFB0();
            if (D_0043B7F8.unk0 == type) goto skip13;
        }
        if (D_0043B7F8.unk0 != 4) {
            if (D_0043B7F8.unk0 == 8) goto skip13b;
            Text_Printf(D_004FB9F8, 0xA, 1, 0x7B, 0x168);
        }
    skip13:
        if (D_0043B7F8.unk0 != 8) Battle_UpdateFrame();
    skip13b:
        if (++D_005F6510 >= 0x97) {
            if (D_0043B7F8.unk0 == 4) {
                func_003CD440(g->unk21, D_005F5CC0, D_005F5CC8[g->unk21]);
                func_003B02C8(g->unk21);
            }
            gMode = 6;
        }
        break;
    case 14:
    state14:
        func_003753D0();
        func_00375190();
        Bgm_Stop();
        Snd_PlaySe(0x41, 0);
        Snd_PlaySe(0x42, 0);
        D_005F6510 = 0x52;
        g->unk11 = 1;
        D_005F5CB4 = 1;
        if (func_003AE670() != 0) {
            gFighters[0].unk127 = 0;
            gFighters[1].unk127 = 0;
        }
        if (D_0043B7F8.unk8 != 0) goto end14;
        gSubMode = sub = 0xF;
        if (func_003B6220() != 0) D_005F5EE0 = 1;
    case 15:
        if (Battle_UpdateFrame() > 0 || D_005F6510 < 0x43) D_005F5EE0 = 1;
        if (D_005F6510 & 0x18) {
            Text_Printf(D_004FBA10, 0x19, 0xDC, 0xA, 8, 2);
        }
        if (--D_005F6510 > 0) break;
        func_003992C8(1);
        func_003ADFA8();
    end14:
        if (g->unk3 != 0) {
            D_005F5C82 = 0;
            D_005F5C80[0] = 0;
            func_003B0260();
        }
        gSubMode = sub = 0;
        break;
    case 16:
        Battle_UpdateFrame();
        Bgm_Stop();
        func_003BAA90();
        gMode = 6;
        break;
    case 17:
        func_003BAA90();
        gSubMode = sub = 0;
        gMode = 0xA;
        Bgm_Play(2);
        break;
    case 18:
        func_003BAA90();
        func_003C4AE0();
        gSubMode = sub = 0x13;
    case 19:
        if (func_003B1378() != 0) func_0035A4F0(0x17);
        D_005F5C74 = 0;
        gFighters[0].unk127 = 0;
        gFighters[1].unk127 = 0;
        func_003ADFB0();
        Battle_UpdateFrame();
        if (Results_DrawSurvival(g->unk20) == 1) gSubMode = sub = 0xC;
        break;
    case 20:
        Bgm_Play(2);
        gSubMode = sub = 0x15;
        gFighters[1].unk127 = 0;
        gFighters[0].unk127 = 0;
        func_003BAA90();
        func_003B6C00(g);
    case 21:
        if (func_003B1378() != 0) func_0035A4F0(0x17);
        func_003ADFB0();
        Battle_UpdateFrame();
        if (func_003B6C68(g) != 0) gSubMode = sub = 0xC;
        break;
    case 23:
    case 24:
    case 25:
        if (D_005F6510++ >= 4) {
            func_00346600();
            func_003992C8(1);
            func_003ADFA8();
            D_005F5EE0 = 0;
            if (gSubMode == 0x17) {
                func_003B7A68();
            } else if (gSubMode == 0x19) {
                Mode_Change(3);
            } else if (gSubMode == 0x18) {
                func_00358A38();
            }
        } else if (D_0043B7F8.unk0 == 8) {
            func_003F0008();
        } else {
            Battle_UpdateFrame();
        }
        break;
    case 22:
    default:
        func_003BAA90();
        Mode_Change(3);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_mode", Mode_Battle);
#endif
s32 func_0035B488(void) {
    s32 r = 0;
    if (D_0043B7F8.unk0 == 8) r = gSubMode == 7;
    return r;
}
void func_0035B4B0(s32 *c, GteMatrix *m) {
    GteMatrix t;
    s32 a, b;
    s16 sa, ca, sb, cb;

    a = -c[2];
    b = -c[1];
    sa = gSinTable[a & 0xFFF];
    sb = gSinTable[b & 0xFFF];
    ca = gSinTable[(a & 0xFFF) + 0x400];
    cb = gSinTable[(b & 0xFFF) + 0x400];
    t.m[0][0] = ca;
    t.m[0][1] = -sa;
    t.m[0][2] = 0;
    t.m[1][0] = sa;
    t.m[1][1] = ca;
    t.m[1][2] = 0;
    t.m[2][0] = 0;
    t.m[2][1] = 0;
    t.m[2][2] = 0x1000;
    m->m[0][0] = cb;
    m->m[0][1] = 0;
    m->m[0][2] = -sb;
    m->m[1][0] = 0;
    m->m[1][1] = 0x1000;
    m->m[1][2] = 0;
    m->m[2][0] = sb;
    m->m[2][1] = 0;
    m->m[2][2] = cb;
    Gte_MulMatrix(m, &t);
    a = -c[0] & 0xFFF;
    sa = gSinTable[a];
    ca = gSinTable[a + 0x400];
    t.m[0][0] = 0x1000;
    t.m[0][1] = 0;
    t.m[0][2] = 0;
    t.m[1][0] = 0;
    t.m[1][1] = ca;
    t.m[1][2] = sa;
    t.m[2][0] = 0;
    t.m[2][1] = -sa;
    t.m[2][2] = ca;
    Gte_MulMatrix2(&t, m);
    m->m[1][0] = m->m[1][0] * 0x1300 / 0x1000;
    m->m[1][1] = m->m[1][1] * 0x1300 / 0x1000;
    m->m[1][2] = m->m[1][2] * 0x1300 / 0x1000;
    Gte_ApplyMatrixXyz(m, -c[5], -c[6], -c[7], &m->t[0], &m->t[1], &m->t[2]);
    func_003AE170(c);
}
void func_0035B690(s32 *p, s32 *out) {
    s32 dy = p[4] - p[1];
    s32 dx = p[3] - p[0];
    s32 dz = p[5] - p[2];
    s32 a;
    a = func_00366108(Math_Isqrt(dx * dx + dz * dz), dy);
    out[0] = (a & 0x800) ? (a & 0xFFF) - 0x1000 : (a & 0xFFF);
    out[1] = func_00366108(dx, dz) - 0x400;
    out[2] = p[6];
    out[5] = p[0];
    out[6] = p[1];
    out[7] = p[2];
}
void func_0035B760(s32 *p, s32 *out) {
    func_0035B690(p, out);
    out[0] <<= 6;
    out[1] <<= 6;
    out[2] <<= 6;
}
void func_0035B7A8(CamPos *c) {
    D_00415808 = *c;
    D_00415808.w[0] /= 64;
    D_00415808.w[1] /= 64;
    D_00415808.w[2] /= 64;
    func_0035B4B0(D_00415808.w, &D_00436530);
}
void func_0035B858(CamPos *c) {
    D_00415808 = *c;
    func_0035B4B0(c->w, &D_00436530);
}
void func_0035B8B8(s32 *p) {
    s16 dx = p[3] - p[0];
    s16 dz = p[5] - p[2];
    s16 dy = p[4] - p[1];
    s32 h = dx * dx + dz * dz;
    Math_Isqrt(h + dy * dy);
    D_00415808.w[0] = func_00366108(Math_Isqrt(h), dy);
    D_00415808.w[1] = func_00366108(dx, dz) - 0x400;
    D_00415808.w[2] = p[6];
    D_00415808.w[5] = p[0];
    D_00415808.w[6] = p[1];
    D_00415808.w[7] = p[2];
    p[7] = 0;
    func_0035B858(&D_00415808);
}
void func_0035B9A8(Fighter *a, Fighter *b) {
    u16 p0 = D_005F6B20[0];
    u16 p1 = D_005F6B20[1];
    D_005F6B28[0] = p0;
    D_005F6B28[1] = p1;
    D_005F6B20[0] = a->unk624 >= b->unk624;
    D_005F6B20[1] = b->unk624 >= a->unk624;
    __asm__ volatile(""); /* fake */
}
PORT_RET(s16) func_0035B9E8(s16 i) {
    return D_005F6B20[i];
}
PORT_RET(s16) func_0035BA00(s16 i) {
    return D_005F6B28[i];
}
void func_0035BA18(s32 i) {
    u16 p0, p1;
    D_005F6B20[i] = 0;
    D_005F6B20[(i + 1) & 1] = 1;
    p0 = D_005F6B20[0];
    p1 = D_005F6B20[1];
    D_005F6B28[0] = p0;
    D_005F6B28[1] = p1;
    __asm__ volatile(""); /* fake */
}
void func_0035BA58(Fighter *f, s32 unused, s32 *v, s32 *out) {
    s32 p[8];
    CamPos q;
    s32 *a = &f->unk51C;
    s32 *b = &f->unk708;
    s32 t;
    out[5] = p[0] = v[0];
    p[6] = 0;
    out[6] = p[1] = v[1];
    out[7] = p[2] = v[2];
    p[3] = a[0];
    p[4] = a[1];
    p[5] = a[2];
    func_0035B760(p, out);
    p[4] = b[1] - 0x122;
    func_0035B760(p, q.w);
    t = q.w[0] + 0xFA4;
    if (out[0] < t) t = out[0];
    out[0] = t;
    p[4] = 0;
    func_0035B760(p, q.w);
    t = q.w[0] - 0x1B05;
    out[0] = out[0] < t ? out[0] : t;
    if (D_004333E4[0] != 0 && out[0] < 0) out[0] = 0;
}
/* NON_MATCHING: scheduling of the LCG product and the angle terms, stores to D_005F72B0 / the stack swapped */
#if 0
/* NON_MATCHING: scheduling of the LCG product and the angle terms, stores to D_005F72B0 / the stack swapped */
void func_0035BB58(Fighter *f, s32 arg) {
    s32 p[3];
    s32 *pos = &f->unk51C;
    s32 *j = &f->unk708;
    s16 r;
    s32 ang;
    s32 dist;
    s32 d;
    s32 x;

    D_005F5CF4 = (u64)(u32)(D_005F5CF4 + 1) * 0x10DCD;
    r = (((u64)((u32)D_005F5CF4 >> 16) * 0xE3) >> 16) - 0x71;
    d = func_003D7DA8(f);
    ang = r + 0x400;
    ang -= d / 16;
    ang &= 0xFFF;
    dist = D_005F5CD0 * 0x1450 / 0x3BB + 0x5A0;
    x = f->unk51C + dist * gSinTable[ang + 0x400] / 4096;
    p[0] = x;
    D_005F72B0.w[5] = x;
    p[2] = D_005F72B0.w[7] = pos[2] + dist * gSinTable[ang] / 4096;
    if (D_004333E4[0] != 0) {
        D_005F72B0.w[6] = j[1] - 0x122;
        D_005F72B0.w[6] = p[1] = j[1] - 0x122;
    } else {
        D_005F72B0.w[6] = p[1] = -0x352;
    }
    func_0035BA58(f, arg, p, D_005F72B0.w);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_mode", func_0035BB58);
#endif
void func_0035BD20(Fighter *f, s32 arg) {
    CamPos *c = &D_005F72B0;
    s32 *pos = &f->unk51C;
    s32 dx = f->unk51C - c->w[5];
    s32 dy = pos[1] - c->w[6];
    s32 dz = pos[2] - c->w[7];
    s32 d = func_003985D8(dx * dx + dy * dy + dz * dz);
    s32 p[3];
    CamPos q;
    s32 t;

    if (d - D_005F5CD0 * 0x1450 / 0x3BB > 0) {
        c->w[5] += dx * 8 / d;
        c->w[6] += dy * 8 / d;
        c->w[7] += dz * 8 / d;
    }
    p[0] = c->w[5];
    p[1] = c->w[6];
    p[2] = c->w[7];
    func_0035BA58(f, arg, p, q.w);
    t = q.w[0] - D_005F72B0.w[0];
    c->w[0] = D_005F72B0.w[0] + WRAP18(t) / 32;
    t = q.w[1] - D_005F72B0.w[1];
    D_005F72B0.w[1] = D_005F72B0.w[1] + WRAP18(t) / 32;
    func_0035B7A8(&D_005F72B0);
}
void func_0035BF10(s16 kind, s16 arg, HitRec *p, s32 d) {
    s32 k = kind & 0x7F;
    if (kind & 0x80) {
        func_003A6110(k, arg, p);
    } else switch (k) {
    case 0:
    case 1:
        func_003A2BC0(k, arg, p, d);
        break;
    case 2:
        func_003A2758(arg, p, 1, 0.0f);
        break;
    }
}
void func_0035BF70(s32 kind, HitRec *p, s32 d) {
    if (func_0035B488() == 0) {
        func_0035BF10(kind, 0, p, d);
        HitLog_Add(kind, 0, p, d);
    }
}
void func_0035BFF8(s32 arg, HitRec *p) {
    if (func_0035B488() == 0) {
        func_0035BF10(2, arg, p, 0);
        HitLog_Add(2, arg, p, 0);
    }
}
void func_0035C070(s32 kind, s32 arg, HitRec *p) {
    s16 k = kind | 0x80;
    func_0035BF10(k, arg, p, 0);
    HitLog_Add(k, arg, p, 0);
}
void func_0035C0D8(s16 dmg, s32 unused, s16 side, HitRec *p) {
    Fighter *f;
    HitRec h;
    s16 dx, dy, dz;
    func_0035BFF8(0, p);
    f = gFighters;
    if (side != 0) f++;
    if (f->unkC0->unk28 == 0x18) {
        func_0035C070(side, 10, p);
        return;
    }
    if (dmg >= 0x15) {
        func_0035BF70(side, p, 1);
        dx = f->unk19C - f->unk190;
        dy = f->unk1A0 - f->unk194;
        dz = f->unk1A4 - f->unk198;
        h.pos = p->pos;
        h.unk10 = dx;
        h.unk12 = dy;
        h.unk14 = dz;
        if (f->unk16A == 0 && f->unk400 == 0) {
            func_0035C070(side, 1, &h);
            func_0035C070(side, 0, &h);
        } else {
            func_0035C070(side, 0xC, &h);
            func_0035C070(side, 0xB, &h);
        }
    }
}
void func_0035C230(void) {
    HitLog_Reset();
}
void func_0035C238(void) {
    HitLog_NextFrame();
}
void func_0035C240(HitPos *pos) {
    HitRec h;
    h.pos = *pos;
    h.pos.w[1] = 0;
    h.unk10 = 0;
    h.unk12 = 0;
    h.unk14 = 0;
    func_003A2758(3, &h, 0, 0.0f);
    HitLog_Add(2, 3, &h, 0);
}
