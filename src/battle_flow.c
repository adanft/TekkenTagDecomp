/* 0x3682F8-0x3745E8 (module battle_flow, docs/game_overview.md section 4; docs/systems/fighter.md section 5). The
 * fight itself: the per-frame update of the two fighters on screen (Battle_UpdateFrame, called by Mode_Battle), the
 * input fetch of each fighter (func_0036C680), the round flow and its texts ("ROUND %d", "K.O.", "TIME UP"), and
 * the end of the object (0x374520-0x3745E8). The start is provisional [I]; the end is
 * where the sound object starts (Snd_Init, .rodata 0x500780). The object's .rodata (0x4FF8D0-0x500780: tables, the
 * round texts, jump tables) is included in place with INCLUDE_RODATA, except the jump tables of the functions that
 * are C. Its .lit4 (0.6f and 1.4f, 0x5F5168) comes from Fighter_ApplyDamage (the damage). */
#include "common.h"
#include "fighter.h"

/* A 10-byte entry of D_005E8978: lists ended by unk0 == 0xFFFF, a move record's unk14 is the index of its first
 * entry. unk0 is a condition (func_00369008), unk2 the damage (bits 0-13) and its kind (bits 14-15), unk4 / unk6 the
 * reaction move of the other fighter, chosen by the distance against unk8 (Battle_ResolveHits) [I]. */
typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u16 unk6;
    /* 0x8 */ u16 unk8;
} Unk5E8978;

extern Fighter gFighters[4];
extern Fighter D_00884EE0;
extern Unk5E8978 D_005E8978[];

/* 0x879610: two 16-byte records just before gFighters, distances to the fighters in D_005F5C38 [I]. */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 unk8;
    /* 0xC */ s32 unkC;
} Unk879610;

extern Unk879610 D_00879610[2];

/* 0x38-byte records of D_0046BE78; unk1C is a move id (func_003688B0) [I]. */
typedef struct {
    /* 0x00 */ u8 pad0[0x1C];
    /* 0x1C */ u16 unk1C;
    /* 0x1E */ u8 pad1E[0x1A];
} Unk46BE78;

/* 0x1C-byte launch reaction records: D_00500678 (8, in .rodata) and D_005F2BA8 (func_00374010) [I]. */
typedef struct {
    /* 0x00 */ u16 unk0;                      /* move id */
    /* 0x02 */ u16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ u16 unk6;
    /* 0x08 */ s16 unk8;
    /* 0x0A */ u16 unkA;
    /* 0x0C */ u8 padC[0x10];
} Unk500678;

extern Unk46BE78 D_0046BE78[];
extern Unk500678 D_00500678[];
extern Unk500678 D_005F2BA8[];
extern s32 D_00415808[];
extern u32 D_00415718[];
extern u32 D_00415728[];
extern s32 D_00879630[4];
extern s32 D_005F6BE0;
extern s32 D_005F6BE8;
extern s32 D_005F6BEC;
extern s16 D_005F5C2C;
extern u32 D_005F5C38[2];

extern u8 D_0043B7F8[];
extern u16 gFrameCount;
extern s32 D_005F5E88;
extern s32 D_005F5E8C;
extern s32 D_005F5E90;
extern s32 D_005F5C40[2];
extern s32 D_005F5C48[2];
extern s32 gRoundTimer;
extern u16 D_005F5CAC;
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 D_005F5EE8;
extern s32 D_005F5EEC;
extern s32 gRoundPhase;
extern s32 D_005F5EF4;
extern s32 D_005F5EF8;
extern s32 D_005F5F08;
extern s32 gRoundNumber;
extern s32 D_005F5F0C;
extern s32 D_005F5F10;
extern s32 D_005F5F20;
extern s32 D_005F5F24;
extern s32 D_005F5F28;
extern s32 gBattleParity;

void Snd_UpdateDelayed(void);
s32 rand(void);
/* The move record (Fighter fields 0xB8, 0xC0 and 0x188) of a move id. */
void *func_00368218(Fighter *f, s16 move);
void func_003685B8(Fighter *f, Fighter *o, s16 pose);
s32 func_00368958(Fighter *f, Fighter *o);
MoveCmd *Move_FindCommand(MoveCmd *list, s16 frame, Fighter *f, Fighter *o);
Unk5E8978 *func_0036C1A0(Fighter *f, Fighter *o, struct FighterMove *m);
s32 func_00374048(Fighter *f, Fighter *o);
u16 func_0036C918(u16 held, u16 pressed);
void func_0036C988(Fighter *f, u16 held, u16 pressed);
void func_0036CA68(Fighter *f, u16 held, u16 pressed);
s32 func_0036CFC0(u16 held);
Unk500678 *func_00374010(s32 i);
void func_0036BD98(Fighter *f);
s16 func_0036BE08(Fighter *f, s16 kind);
s32 func_003B6220(void);
s32 func_003D29E8(Fighter *f, s16 move);
u16 Math_Atan2(s16 x, s16 y);
u32 Math_Isqrt(u32 x);
void func_0034E8B0(Fighter *f);
void func_003532A0(void);
struct FighterMove *func_00368248(s32 id);
void func_003688B0(Fighter *a, Fighter *b);
void Fighter_StartCommandMove(Fighter *a, Fighter *b);
s32 func_00368B08(Fighter *a, Fighter *b);
void func_00368B90(Fighter *a, Fighter *b);
void Fighter_StepMove(Fighter *f, Fighter *o);
void func_00371B78(Fighter *f);
void func_00372BD0(Fighter *f);
void func_00377AE0(Fighter *a, Fighter *b);
void Prac_RecEnd(s32 player);
void Prac_RecReset(s32 player);
void func_00364698(void);
void func_00368398(Fighter *f, Fighter *o);
void func_0036C480(Fighter *f);
void func_003730E8(Fighter *f, Fighter *o, s32 side);
void func_00373080(Fighter *f, Fighter *o);
void func_00373398(Fighter *f);
void func_00375468(void);
void func_00376C30(void);
s32 func_003B00F0(s32 port);
void Battle_InitTeamLife(void);
void func_00341AE8(Fighter *a, Fighter *b);
void func_0034F288(Fighter *f);
void func_0034F3D8(Fighter *f);
void func_0035B9A8(Fighter *a, Fighter *b);
void func_0035C238(void);
void func_0035C240(s32 *pos);
void func_0035C2C8(s16 i);
void func_0035C320(void);
s32 func_00364B50(void);
s32 func_00365D00(void);
void func_0036DDD0(Fighter *a, Fighter *b);
s32 func_0036DE10(void);
void Battle_UpdatePause(void);
s32 func_0036E158(void);
s32 func_0036E178(void);
void Battle_UpdateRound(Fighter *a, Fighter *b);
void func_0036EA80(Fighter *a, Fighter *b);
void func_0036FB40(Fighter *a, Fighter *b);
void Battle_UpdateFacing(Fighter *a, Fighter *b);
void Battle_UpdateDistance(void);
void func_00371A48(void);
void func_00371B18(Fighter *a, Fighter *b);
void Fighter_UpdateMotion(Fighter *f);
void func_00371C08(Fighter *a, Fighter *b);
void Battle_ApplyVelocity(Fighter *a, Fighter *b);
void Battle_ResolveHits(Fighter *a, Fighter *b);
void Fighter_ApplyDamage(Fighter *a, Fighter *b);
void func_003729B0(Fighter *a, Fighter *b);
void func_00372A10(Fighter *a, Fighter *b);
void Battle_StepMoves(Fighter *a, Fighter *b);
void func_00372BA8(Fighter *a, Fighter *b);
void func_00372E98(Fighter *a, Fighter *b);
void func_00375B60(Fighter *a, Fighter *b);
void func_003772F0(Fighter *a, Fighter *b);
void func_00377AE8(Fighter *a, Fighter *b);
void func_00378150(Fighter *f, s32 n);
void func_00378600(Fighter *f);
void func_003796B8(void);
void func_0037A258(void);
void func_0037D668(Fighter *f);
void func_0038D518(Fighter *a, Fighter *b, s32 *state);
void func_003A3148(s16 n);
void func_003A6390(Fighter *f, s32 n);
void func_003BAEF0(void);
void func_003C1B38();
s32 func_003D3228(s32 a, s32 b);
void func_003D3528(void);
void func_003D3C68(f32 x, f32 y, f32 z, f32 w);
void func_003DB690(void);
void func_003DB698(s32 n);
void func_0037C8C8(s32 player);
s32 func_00369008(Fighter *f, Fighter *o, s32 id);
void func_0036C448(Fighter *f);
void Fighter_ResetInputHistory(Fighter *f);
void func_0036C680(Fighter *f);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003682F8);

#if 0
/* NON_MATCHING: the pose choice stores to unk184 in each branch instead of one shared store, and the move pointer
 * stores are scheduled differently */
void func_00368398(Fighter *f, Fighter *o) {
    u16 pad;
    s16 frame;
    void *m;

    if (f->unk1E == 0) {
        pad = D_005F5C40[0];
    } else {
        pad = D_005F5C40[1];
    }
    if (gRoundNumber == 1) {
        if (f->unkB12C != NULL) {
            f->unk184 = f->unkB12C->unk4;
        } else if (pad & 0x90) {
            f->unk184 = 4;
        } else if (pad & 0x60) {
            f->unk184 = 6;
        } else if (rand() & 1) {
            f->unk184 = 4;
        } else {
            f->unk184 = 6;
        }
        if (f->unk2A == 0x21 && func_003B6220() != 0) {
            f->unk184 = 9;
        }
    } else {
        f->unk184 = 5;
    }
    m = func_00368218(f, (s16)f->unk184);
    f->unk188 = m;
    f->unkC0 = m;
    f->unkB8 = m;
    if (func_003B6220() != 0) {
        frame = func_003D29E8(f, (s16)f->unk184);
        if (f->unkC0->unk18 < frame + 0x5A) {
            frame = f->unkC0->unk18 - 0x5A;
        }
    } else {
        frame = 1;
        if (gRoundNumber == 1 && f->unkC0->unk18 >= 0x97) {
            frame = f->unkC0->unk18 - 0x95;
        }
    }
    f->unkC4 = 1;
    f->unk178 = 1;
    f->unk17A = func_0036BE08(f, 6);
    f->unk17C = 0;
    f->unk17E = 0;
    f->unk182 = 0;
    f->unk180 = 0;
    func_0036BD98(f);
    Fighter_StepMove(f, o);
    f->unkC4 = frame;
    f->unkBC = frame;
    f->unkC6 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00368398);
#endif

void func_00368528(Fighter *f, Fighter *o, s16 pose) {
    u16 pad = D_005F5C40[f->unk1E];
    s16 p = pose;

    if (pad & 0x20) {
        p = 3;
    }
    if (pad & 0x40) {
        p = 2;
    }
    if (pad & 0x10) {
        p = 1;
    }
    if (pad & 0x80) {
        p = 0;
    }
    func_003685B8(f, o, p);
    func_003685B8(o, f, p);
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003685B8);

#if 0
/* NON_MATCHING: the unk3C store and the unkB8 / unkC4 stores are scheduled earlier than in the original */
void func_00368748(Fighter *f, Fighter *o) {
    struct FighterUnkB12C *p = f->unkB12C;
    void *m;

    if (p != NULL) {
        f->unk184 = p->unk4;
        f->unk4 = f->unk51C = p->unk6;
        f->unkC = f->unk524 = p->unk8;
        f->unk1A = f->unk40 = f->unk3C = p->unkA;
    } else {
        f->unk184 = 0x12AC;
    }
    m = func_00368218(f, f->unk184);
    f->unkC6 = 0;
    f->unk188 = m;
    f->unk178 = 1;
    f->unkC0 = m;
    f->unkB8 = m;
    f->unkC4 = 1;
    f->unk17A = func_0036BE08(f, 6);
    f->unk17C = 0;
    f->unk17E = 0;
    f->unk182 = 0;
    f->unk180 = 0;
    func_0036BD98(f);
    Fighter_StepMove(f, o);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00368748);
#endif

#if 0
/* NON_MATCHING: the stores to unkB8 and unkC4 are scheduled before unk188 / unk178; the original keeps them after */
void func_00368818(Fighter *f, Fighter *o, s16 id) {
    void *m;

    f->unk184 = id;
    m = func_00368218(f, id);
    f->unkC6 = 0;
    f->unk188 = m;
    f->unk178 = 1;
    f->unkC0 = m;
    f->unkB8 = m;
    f->unkC4 = 1;
    f->unk17A = func_0036BE08(f, 6);
    f->unk17C = 0;
    f->unk17E = 0;
    f->unk182 = 0;
    f->unk180 = 0;
    f->unk130 = 0;
    f->unk131 = 0;
    func_0036BD98(f);
    Fighter_StepMove(f, o);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00368818);
#endif

#if 0
/* NON_MATCHING: v1 / a1 / a2 swapped around the table lookup, and the unkC4 store order */
void func_003688B0(Fighter *f, Fighter *o) {
    Unk5E8978 *e = func_0036C1A0(o, f, o->unk188);
    u16 id = D_0046BE78[e->unk4].unk1C;

    f->unk184 = id;
    f->unk188 = func_00368218(f, id);
    f->unk178 = 1;
    f->unkC4 = 1;
    f->unk17A = func_0036BE08(f, 0x24);
    f->unk17C = o->unk17C;
    o->unk17C = 0;
    f->unk17E = 0;
    f->unk182 = 0;
    f->unk180 = 0;
    func_0036BD98(f);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003688B0);
#endif

#if 0
/* NON_MATCHING: the unkC0 store is one slot earlier than in the original (decomp-permuter matches it only with a fake
 * `m++; m--;` after the unk188 store) */
s32 func_00368958(Fighter *f, Fighter *o) {
    void *m;

    f->unkFC = func_00374048(f, o);
    f->unk184 = func_00374010(f->unkFC)->unk0;
    m = func_00368218(f, f->unk184);
    f->unk188 = m;
    f->unkB8 = m;
    f->unkC0 = m;
    f->unk17A = func_0036BE08(f, 0x26);
    f->unk17C = 0;
    f->unk17E = 0;
    f->unk182 = 0;
    f->unk180 = 0;
    f->unk130 = 0;
    f->unkD2 = 0;
    f->unk131 = 1;
    func_0036BD98(f);
    Fighter_StepMove(f, o);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00368958);
#endif

void Fighter_StartCommandMove(Fighter *f, Fighter *o) {
    MoveCmd *c;

    f->unk148 = 0;
    if (f->unk12F != 0) {
        c = Move_FindCommand((MoveCmd *)f->unkC0->unkC, f->unkC4, f, o);
        if (c->unk0 != 0x8000) {
            u16 id = c->unk6;

            f->unk184 = id;
            f->unk188 = func_00368218(f, id);
            f->unk178 = c->unkD;
            f->unkD8 = c->unkC - f->unkC4;
            f->unkF3 = 0;
            f->unk12F = 0;
            f->unk130 = 0;
            f->unk131 = 0;
            f->unk14E = 0;
            o->unk132 = 0;
            o->unkF1 = 0;
            o->unkF4 = 0;
            o->unkF5 = 0;
            f->unk17A = func_0036BE08(f, c->unk8 & 0x7F);
            f->unk17C = 0;
            f->unk17E = 0;
            f->unk182 = 0;
            f->unk180 = 0;
            f->unk148 = o->unkCA / 2 + 0x19;
        }
    }
}

s32 func_00368B08(Fighter *f, Fighter *o) {
    if ((o->unk188 == NULL || o->unk17A != 0x20) && f->unk3F4 == 0 && f->unk131 != 0
        && !(((struct FighterMove *)func_00368218(f, f->unk184))->unk4 & 0x604)) {
        return func_00368958(f, o);
    }
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00368B90);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00368F78);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00369008);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_004FF8D0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00369188);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003691D8);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_004FF8E0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003693E0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", Move_FindCommand);

s32 func_0036A008(u8 *list, u16 v);
s32 func_003682A8(s32 a, s16 b);

/* The extra condition of a command entry (Move_FindCommand), selected by unk3 & 0x7F (0x60-0x66): both fighters below
 * 0x2AAA in unk40 [I: height], and per kind the other fighter's hit flag unk12F, the attack level unkD0 of the current
 * move (0x412, 0x217, 0x10F) with unk4 among the move's four bytes at unkC0 + 0x28, or the current move id unk10C. */
#if 0
/* NON_MATCHING: `c` and the result are in swapped registers (t0 / a3) and the failing tests of cases 0x61-0x64 branch
 * to a shared `r = 0` the original keeps in each delay slot */
s32 func_00369E10(MoveCmd *c, Fighter *o, Fighter *f) {
    s32 r = 0;

    switch (c->unk3 & 0x7F) {
    case 0x60:
        r = o->unk12F != 0 && (s16)o->unk40 < 0x2AAA && (s16)f->unk40 < 0x2AAA;
        break;
    case 0x61:
        if (!(f->unkC0->unk24 & 8) && (s16)o->unk40 < 0x2AAA && (s16)f->unk40 < 0x2AAA && f->unkD0 == 0x412) {
            r = func_0036A008(&f->unkC0->unk28, c->unk4);
        }
        break;
    case 0x62:
        if (!(f->unkC0->unk24 & 8) && (s16)o->unk40 < 0x2AAA && (s16)f->unk40 < 0x2AAA && f->unkD0 == 0x217) {
            r = func_0036A008(&f->unkC0->unk28, c->unk4);
        }
        break;
    case 0x63:
        if (!(f->unkC0->unk24 & 8) && (s16)o->unk40 < 0x2AAA && (s16)f->unk40 < 0x2AAA && f->unkD0 == 0x10F) {
            r = func_0036A008(&f->unkC0->unk28, c->unk4);
        }
        break;
    case 0x64:
        if (!(f->unkC0->unk24 & 8) && (s16)o->unk40 < 0x2AAA && (s16)f->unk40 < 0x2AAA
            && (f->unkD0 == 0x412 || f->unkD0 == 0x217)) {
            r = func_0036A008(&f->unkC0->unk28, c->unk4);
        }
        break;
    case 0x65:
        r = (u16)c->unk4 == f->unk10C && (s16)o->unk40 < 0x2AAA && (s16)f->unk40 < 0x2AAA;
        break;
    case 0x66:
        if ((s16)o->unk40 < 0x2AAA && (s16)f->unk40 < 0x2AAA) {
            r = func_003682A8(f->unkC0->unk0, c->unk4);
        }
        break;
    }
    return r;
}
#else
INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_004FFA60);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00369E10);
#endif

#if 0
/* NON_MATCHING: the loop is entered at its test in the original (`b` to the compare); ours copies the test to the
 * top */
s32 func_0036A008(u8 *list, u16 v) {
    s32 r = 0;
    s16 i;

    if (v != 0) {
        i = 0;
        while (*list++ != v) {
            i++;
            if (i >= 4) {
                goto end;
            }
        }
    }
    r = 1;
end:
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036A008);
#endif

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036A050);

s16 func_0036A2D0(Fighter *f) {
    s16 n = 0;

    if (!(f->unkC0->unk24 & 0x10)) {
        n = f->unk146 + f->unk160 / 2;
    }
    f->unk162 = f->unk188->unk18;
    if (n >= 0x42) {
        n = 0x42;
    }
    return n;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036A330);

#if 0
/* NON_MATCHING: a first draft from m2c with the types fixed (gotos kept): 1484 instructions instead of 1546, 1089
 * differ. It shows the structure read in docs/systems/fighter.md section 5.3: the frame step, the start of the
 * pending move (tag, resets, flags from the record), the switch on the move kind unk17A (0x2E kinds) and the common
 * tail; the block order, the shared tails of the kinds and the register allocation are far from the original. */
void func_0036A330(Fighter *f, Fighter *o, s16 n);
void func_0036C128(Fighter *f);
void func_00370F28(Fighter *f, Fighter *o, s32 a, s32 b);
void func_003D2A38(Fighter *f, Fighter *o);
s32 func_0035BA00(s32 side);
void Fighter_TagSwap(s32 side);
void Snd_StopVoiceSeq(Fighter *f);
void func_00374148(Fighter *f, Fighter *o);
void Battle_UpdateFacing(Fighter *a, Fighter *b);
s32 func_0036BF00(Fighter *f);
s16 func_0036C278(s32 cur);
s16 func_0036C318(s16 cur, s16 k);
void func_0036C228(Fighter *f, Fighter *o, struct FighterMove *m);
void func_00368168(Fighter *f);
void func_0036BCD0(Fighter *f, s32 arg1);
void func_0036BDA0(Fighter *f);
s16 func_0036A2D0(Fighter *f);
void Fighter_StepMove(Fighter *f, Fighter *o) {
    s16 temp_a0;
    s16 temp_a0_6;
    s16 temp_a1_8;
    s16 temp_a2_5;
    s16 temp_v0;
    s16 temp_v0_10;
    s16 temp_v0_11;
    s16 temp_v0_13;
    s16 temp_v0_15;
    s16 temp_v0_20;
    s16 temp_v0_29;
    s16 temp_v0_2;
    s16 temp_v0_8;
    s16 temp_v0_9;
    s16 temp_v1;
    s16 temp_v1_2;
    s16 var_v0_11;
    s16 var_v0_14;
    s16 var_v0_18;
    s16 var_v0_19;
    s16 var_v0_2;
    s16 var_v0_9;
    s16 *temp_a1_10;
    s32 temp_a1_9;
    s32 temp_s3;
    s16 *temp_v0_26;
    s32 temp_v0_28;
    s16 *var_a0;
    s16 *var_a2;
    s32 var_s0;
    s32 var_v0_12;
    s32 var_v0_13;
    s16 *var_v0_15;
    s32 var_v0_17;
    s32 var_v0_3;
    s32 var_v1;
    struct FighterMove *temp_a0_8;
    struct FighterUnk18C *temp_a3_2;
    struct FighterUnk18C *temp_t1;
    struct FighterMove *temp_t2;
    struct FighterMove *temp_t2_10;
    struct FighterMove *temp_t2_11;
    struct FighterMove *temp_t2_12;
    struct FighterMove *temp_t2_13;
    struct FighterMove *temp_t2_14;
    struct FighterMove *temp_t2_2;
    struct FighterMove *temp_t2_3;
    struct FighterMove *temp_t2_4;
    struct FighterMove *temp_t2_5;
    struct FighterMove *temp_t2_6;
    struct FighterMove *temp_t2_7;
    struct FighterMove *temp_t2_8;
    struct FighterMove *temp_t2_9;
    struct FighterMove *temp_a0_3;
    struct FighterMove *temp_a0_4;
    struct FighterMove *temp_a2_7;
    struct FighterMove *temp_v1_4;
    u16 temp_a0_5;
    u16 temp_a0_7;
    u16 temp_a1;
    u16 temp_a1_11;
    u16 temp_a1_2;
    u16 temp_a1_4;
    u16 temp_a1_5;
    u16 temp_a1_6;
    u16 temp_a1_7;
    u16 temp_a2_2;
    u16 temp_a2_3;
    u16 temp_a2_4;
    u16 temp_a2_6;
    u16 temp_t3;
    u16 temp_v0_23;
    u16 temp_v0_24;
    u16 temp_v0_25;
    u16 temp_v0_3;
    u16 temp_v0_6;
    u16 temp_v1_5;
    u16 temp_v1_6;
    u16 temp_v1_7;
    u16 var_a0_2;
    u16 var_v0_16;
    u16 var_v0_6;
    u16 var_v1_2;
    u16 var_v1_3;
    u16 var_v1_4;
    u32 temp_a3;
    s16 *temp_v0_27;
    u32 temp_v0_5;
    MoveCmd *var_s0_2;
    u8 temp_a0_2;
    u8 temp_a1_3;
    u8 temp_a2;
    u8 temp_v0_12;
    u8 temp_v0_14;
    u8 temp_v0_16;
    u8 temp_v0_17;
    u8 temp_v0_18;
    u8 temp_v0_19;
    u8 temp_v0_21;
    u8 temp_v0_22;
    u8 temp_v0_4;
    u8 temp_v0_7;
    u8 temp_v1_3;
    u8 var_v0;
    u8 var_v0_10;
    u8 var_v0_4;
    u8 var_v0_5;
    u8 var_v0_7;
    u8 var_v0_8;

    if (f->unk127 != 0) {
        temp_v0 = (u16) f->unk170 - 1;
        f->unk170 = temp_v0;
        if (temp_v0 & 0x8000) {
            f->unk170 = 0;
        }
        var_s0 = 1;
        if (f->unk188 != NULL) {
            temp_v0_2 = f->unkDA;
            if (temp_v0_2 > 0) {
                temp_a0 = f->unkC4;
                var_s0 = 0;
                if (temp_a0 < f->unk178) {
                    if (temp_a0 >= (s32) f->unkC0->unk18) {
                        var_s0 = 1;
                    }
                } else {
                    goto block_11;
                }
            } else if ((temp_v0_2 < 0) && ((temp_v1 = f->unkC4, var_s0 = 0, ((f->unk178 < temp_v1) == 0)) || (temp_v1 <= 0))) {
block_11:
                var_s0 = 1;
            }
        } else {
            var_s0 = 0;
        }
        func_0036BCD0(f, var_s0);
        if (var_s0 != 0) {
            if ((u16) f->unk180 != 0) {
                if (D_005F5EF4 != 0) {
                    var_s0_2 = (MoveCmd *)f->unkC0->unkC;
                    if (var_s0_2->unk0 != 0x8000) {
                        do {
                            var_s0_2++;
                        } while (var_s0_2->unk0 != 0x8000);
                    }
                    temp_v0_3 = var_s0_2->unk6;
                    f->unk184 = temp_v0_3;
                    f->unk188 = func_00368218(f, (s16) temp_v0_3);
                    f->unkD8 = var_s0_2->unkC - (u16) f->unkC4;
                    f->unk17A = func_0036BE08(f, var_s0_2->unk8 & 0x7F);
                    f->unk17C = var_s0_2->unk8 & 0x8000;
                    f->unk180 = 0;
                    f->unk182 = 0;
                    f->unk17E = var_s0_2->unk8 & 0x4000;
                    if (f->unkDA < 0) {
                        var_v0 = var_s0_2->unkC;
                    } else {
                        var_v0 = var_s0_2->unkD;
                    }
                    f->unk178 = (s16) var_v0;
                    if (f->unk178 == 0xFF) {
                        f->unk178 = (s16) f->unkC0->unk18;
                    }
                    f->unkF8 = 2;
                    f->unk12D = 1;
                } else {
                    temp_a1 = (u16) f->unk182;
                    if (temp_a1 != 0) {
                        if ((f->unk418 == 0) && (f->unk2A == 0x21)) {
                            var_v0_2 = func_0036C278(f->unk28);
                        } else {
                            var_v0_2 = func_0036C318(f->unk28, (s16) temp_a1);
                        }
                        f->unk28 = var_v0_2;
                        func_00368168(f);
                    } else {
                        Fighter_TagSwap((s32) f->unk1E);
                        f->unkB134 = 1;
                        Battle_UpdateFacing(f, o);
                    }
                    f->unk188 = func_00368218(f, (s16) f->unk184);
                    func_0036C228(o, f, o->unkC0);
                }
                if (o->unkC0->unk4 & 0x400) {
                    o->unk100 = -1;
                }
                f->unk182 = 0;
                f->unk180 = 0;
            }
            func_003D2A38(f, o);
            func_0036BDA0(f);
            if (f->unkF8 == 3) {
                f->unk178 = 1;
            }
            f->unkF2 = 0;
            f->unkD6 = -1;
            f->unk11D = 0;
            f->unkF0 = 0;
            f->unk11E = 0;
            f->unk110 = o->unk51C;
            f->unkDA = 1;
            f->unk114 = o->unk524;
            f->unkF1 = 0;
            f->unk46 = 0;
            f->unk11A = 0;
            f->unk438.unk82 = 0;
            f->unk11F = 0;
            f->unkE4 = 0;
            f->unk120 = 0;
            f->unk122 = 0;
            f->unk124 = 0;
            f->unk14E = 0;
            if (f->unk148 != 0) {
                f->unkCA = (s16) (u16) f->unk148;
            } else {
                func_0036C228(f, o, f->unk188);
            }
            temp_t2 = f->unk188;
            temp_a2 = f->unk12C;
            temp_a3 = temp_t2->unk4;
            f->unkCC = (s32) temp_a3;
            temp_a1_2 = temp_t2->unk8;
            f->unkD5 = (temp_a3 >> 0xB) & 0x1F;
            f->unkD4 = (u8) (temp_a1_2 >> 8);
            f->unkD2 = (u16) f->unkCC & 0x18;
            f->unkD0 = temp_a1_2;
            if ((temp_a2 == 0) && (temp_a3 & 0x100)) {
                f->unkD2 = 0;
                f->unkCC = temp_a3 & 0xFFFFFFE7;
            }
            if (f->unk130 != 0) {
                f->unkF6 = 1;
            } else {
                f->unkF6 = 0;
            }
            temp_a1_3 = f->unk12F;
            if (temp_a1_3 == 0) {
                f->unkF3 = 0;
                f->unkFE = 0;
                f->unk100 = 0;
            }
            if (f->unk132 == 0) {
                f->unkF4 = 0;
            }
            temp_a0_2 = f->unk4C0.unk11;
            f->unkF5 = 0;
            if (temp_a0_2 != 0) {
                if (f->unk131 == 0) {
                    if (f->unkF7 == 0) {
                        f->unkF9 = 0;
                    } else {
                        goto block_91;
                    }
                } else {
block_91:
                    f->unkF9 = 1;
                }
            } else {
                temp_v0_4 = f->unk131;
                f->unkF9 = temp_v0_4;
                f->unkF7 = temp_v0_4;
            }
            var_v0_3 = temp_a0_2 & 0xFF;
            if (temp_a1_3 & 0xFF) {
                var_v0_3 = temp_a0_2 & 0xFF;
                if (f->unk40C != 0) {
                    f->unkE4 = (s16) f->unk14C;
                    var_v0_3 = temp_a0_2 & 0xFF;
                }
            }
            if (var_v0_3 != 0) {
                if (f->unkE0 != 0) {
                    f->unkE2 = (s16) (u16) f->unkE0;
                    goto block_101;
                }
                var_v1 = f->unkF8 & 0xFF;
                if (f->unkE2 == 0) {
                    goto block_100;
                }
            } else {
block_100:
                f->unkE2 = 0;
block_101:
                var_v1 = f->unkF8 & 0xFF;
            }
            if (var_v1 != 3) {
                if (var_v1 != 1) {
                    if (temp_t2->unk0 != f->unkC0->unk0) {
                        goto block_108;
                    }
                } else {
                    goto block_107;
                }
            } else {
block_107:
block_108:
                f->unk134 = 1;
            }
            temp_v0_5 = f->unkC0->unk24;
            temp_s3 = (temp_v0_5 >> 0x13) & 1;
            if (temp_v0_5 & 0x200) {
                f->unk16A = 0x78;
            }
            if (temp_t2->unk24 & 8) {
                f->unk164 = 9;
            }
            if (f->unk17C != 0) {
                f->unk17C = 0;
                f->unk3C = (u16) f->unk3C - 0x8000;
                f->unk1A -= 0x8000;
            }
            if ((f->unk12D != 0) && (f->unk17A != 0x24)) {
                temp_v0_6 = (u16) f->unk3C + f->unkC0->unk12;
                f->unk1A = temp_v0_6;
                f->unk3C = (s16) temp_v0_6;
            }
            f->unk12D = 0;
            func_00370F28(f, o, (s32) temp_a2, (s32) temp_a3);
            if (!(f->unk188->unk4 & 0x01000000)) {
                f->unk129 = 0;
            } else {
                f->unk129 = func_0035BA00((s32) f->unk1E) + 1;
            }
            temp_t3 = (u16) f->unk17A;
            switch ((s16) temp_t3) {                /* switch 1 */
            case 0:                                 /* switch 1 */
                temp_a1_4 = (u16) f->unk3A;
                f->unkBC = 1;
                f->unk1A = temp_a1_4;
                f->unk4 = f->unk51C;
                f->unkC = f->unk524;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unk3C = (s16) temp_a1_4;
block_125:
                f->unkE0 = 0;
                break;
            case 1:                                 /* switch 1 */
                f->unk1A = (u16) f->unk3C;
                /* fallthrough */
            case 2:                                 /* switch 1 */
            case 3:                                 /* switch 1 */
                if (func_0036BF00(f) != 0) {
                    f->unkC4 = (u16) f->unkC4 + 1;
                    f->unkBC = (u16) f->unkBC + 1;
                } else {
                    f->unkBC = 1;
                    f->unkC4 = 1;
                }
                temp_t2_2 = f->unk188;
                if ((s32) temp_t2_2->unk18 < f->unkC4) {
                    temp_v0_7 = temp_t2_2->unk18;
                    f->unkBC = (s16) temp_v0_7;
                    f->unkC4 = (s16) temp_v0_7;
                }
                f->unk11E = 0xB;
                f->unk4 = f->unk51C;
                f->unkC = f->unk524;
                f->unk126 = 1;
                f->unkB8 = temp_t2_2;
                f->unkE0 = 0;
                break;
            case 4:                                 /* switch 1 */
                f->unkC4 = 1;
                temp_v1_2 = (u16) f->unkBC + 1;
                temp_a0_3 = f->unkB8;
                f->unkBC = temp_v1_2;
                if ((s32) temp_a0_3->unk18 < temp_v1_2) {
                    f->unkBC = (s16) temp_a0_3->unk18;
                }
                temp_a1_5 = (u16) f->unk3C;
                f->unkD6 = (s16) (u16) f->unkD8;
                f->unk11E = 0xB;
                f->unk1A = temp_a1_5;
                if ((s16) f->unk40 < 0x4FA5) {
                    temp_v0_8 = f->unk188->unk2C - 1;
                    f->unk15C = temp_v0_8;
                    if (temp_v0_8 <= 0) {
                        f->unk15C = 1;
                    }
                    f->unk15E = (s16) ((s16) ((u16) f->unk3A - temp_a1_5) / (s16) f->unk15C);
                }
                goto block_125;
            case 5:                                 /* switch 1 */
                f->unkC4 = 1;
                temp_v0_9 = (u16) f->unkBC + 1;
                temp_a0_4 = f->unkB8;
                f->unkBC = temp_v0_9;
                if ((s32) temp_a0_4->unk18 < temp_v0_9) {
                    f->unkBC = (s16) temp_a0_4->unk18;
                }
                f->unkD6 = (s16) (u16) f->unkD8;
                f->unk1A = (u16) f->unk3C;
                if ((s16) f->unk40 < 0x4FA4) {
                    var_v0_4 = 4;
                } else {
                    var_v0_4 = 6;
                }
                f->unk11E = var_v0_4;
                f->unk15C = 0;
                f->unk15E = 0;
                f->unkE0 = 0;
                /* fallthrough */
                break;
            case 6:                                 /* switch 1 */
                f->unkBC = 1;
                f->unk1A = (u16) f->unk3C;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                if (temp_s3 != 0) {
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                }
                goto block_125;
            case 7:                                 /* switch 1 */
                temp_t2_3 = f->unk188;
                temp_v1_3 = temp_t2_3->unk18;
                f->unk1A = (u16) f->unk3C;
                f->unkBC = (s16) temp_v1_3;
                f->unkC4 = (s16) temp_v1_3;
                f->unkB8 = temp_t2_3;
                if (temp_s3 != 0) {
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                }
                f->unkE0 = 0;
                f->unkDA = -1;
                break;
            case 8:                                 /* switch 1 */
                temp_v0_10 = (u16) f->unkC4 - 1;
                f->unkC4 = temp_v0_10;
                if ((temp_v0_10 << 0x10) <= 0) {
                    f->unkC4 = 1;
                }
                f->unkBC = (s16) (u16) f->unkC4;
                f->unk1A = (u16) f->unk3C;
                f->unkDA = -1;
                f->unk4 = f->unk51C;
                f->unkC = f->unk524;
                f->unk126 = 1;
                f->unkB8 = f->unk188;
                goto block_125;
            case 9:                                 /* switch 1 */
                f->unkBC = 1;
                f->unk1A = (u16) f->unk44;
                f->unk11E = 5;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unkE0 = 0;
                f->unk15C = 0;
                f->unk15E = 0;
block_314:
                if (temp_s3 != 0) {
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                }
                break;
            case 10:                                /* switch 1 */
                f->unk1A = (u16) f->unk3C;
block_158:
                temp_t2_4 = f->unk188;
                f->unkBC = 1;
                f->unkC4 = 1;
                f->unkB8 = temp_t2_4;
                if (temp_t2_4->unk2C == 0) {
                    temp_v1_4 = f->unkC0;
                    if ((u8) temp_v1_4->unk18 < 0x10U) {
                        var_v0_5 = temp_v1_4->unk18;
                    } else {
                        var_v0_5 = 0x10;
                    }
                } else {
                    var_v0_5 = temp_t2_4->unk2C - 1;
                }
                f->unk15C = (s16) var_v0_5;
                var_v1_2 = (u16) f->unk15C;
                if (f->unk15C <= 0) {
                    var_v1_2 = 1;
                    f->unk15C = 1;
                }
                var_v0_6 = (u16) f->unk3A;
                if ((u16) f->unk40 < 0x4000U) {

                } else {
                    var_v0_6 -= 0x8000;
                }
                f->unk44 = (s16) var_v0_6;
                f->unk15E = (s16) ((s16) ((u16) f->unk44 - (u16) f->unk3C) / (s16) var_v1_2);
                if (temp_s3 != 0) {
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                }
                var_v0_7 = 0xB;
block_235:
                f->unkE0 = 0;
                f->unk11E = var_v0_7;
                break;
            case 11:                                /* switch 1 */
                f->unk1A = (u16) f->unk44;
                goto block_158;
            case 12:                                /* switch 1 */
                f->unkBC = 1;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                if ((s16) f->unk40 < 0x4FA4) {
                    var_v1_3 = (u16) f->unk3A;
                    var_v0_8 = 4;
                } else {
                    var_v1_3 = (u16) f->unk44;
                    var_v0_8 = 6;
                }
                f->unk11E = var_v0_8;
                f->unk1A = var_v1_3;
                f->unkE0 = 0;
                f->unk15C = 0;
                f->unk15E = 0;
                goto block_314;
            case 13:                                /* switch 1 */
                f->unkBC = 1;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unk1A = (u16) f->unk44;
                if ((s16) f->unk40 < 0x4FA4) {
                    f->unk11E = 3;
                    f->unk1A = (u16) f->unk3A;
                } else {
                    f->unk11E = 6;
                }
                f->unkE0 = 0;
                f->unk15C = 0;
                f->unk15E = 0;
                goto block_314;
            case 14:                                /* switch 1 */
                f->unkBC = 1;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unk1A = (u16) f->unk44;
                if ((s16) f->unk40 < 0x4FA4) {
                    f->unk11E = 2;
                    f->unk1A = (u16) f->unk3A;
                } else {
                    f->unk11E = 6;
                }
                f->unkE0 = 0;
                f->unk15C = 0;
                f->unk15E = 0;
                goto block_314;
            case 15:                                /* switch 1 */
                f->unk11E = 1;
                f->unk1A = (u16) f->unk44;
                f->unkC4 = 1;
                f->unkBC = 1;
                f->unkB8 = f->unk188;
                f->unkE0 = 0;
                f->unk15C = 0;
                f->unk15E = 0;
                goto block_314;
            case 16:                                /* switch 1 */
                temp_t2_5 = f->unk188;
                temp_a1_6 = (u16) f->unk3A;
                f->unk11E = 0xB;
                f->unkC4 = 1;
                f->unkBC = 1;
                f->unkB8 = temp_t2_5;
                f->unk1A = temp_a1_6;
                temp_v0_11 = temp_t2_5->unk2C - 1;
                f->unk15C = temp_v0_11;
                if (temp_v0_11 <= 0) {
                    f->unk15C = 1;
                }
                f->unkF0 = 1;
                f->unkE0 = 0;
                f->unkE8 = 0;
                f->unkEC = 0;
                f->unk15E = (s16) ((s16) (temp_a1_6 - (u16) f->unk3C) / (s16) f->unk15C);
                goto block_314;
            case 18:                                /* switch 1 */
                if (func_0036BF00(f) != 0) {
                    f->unkC4 = (u16) f->unkC4 + 1;
                    f->unkBC = (u16) f->unkBC + 1;
                } else {
                    f->unkBC = 1;
                    f->unkC4 = 1;
                }
                temp_t2_6 = f->unk188;
                if ((s32) temp_t2_6->unk18 < f->unkC4) {
                    temp_v0_12 = temp_t2_6->unk18;
                    f->unkBC = (s16) temp_v0_12;
                    f->unkC4 = (s16) temp_v0_12;
                }
                temp_a0_5 = (u16) f->unk3A;
                f->unkF0 = 1;
                f->unk11E = 0xB;
                f->unkB8 = temp_t2_6;
                f->unkE0 = 0;
                f->unkE8 = 0;
                f->unkEC = 0;
                f->unk1A = temp_a0_5;
                temp_v0_13 = temp_t2_6->unk2C - 1;
                f->unk15C = temp_v0_13;
                if (temp_v0_13 <= 0) {
                    f->unk15C = 1;
                }
                var_v0_9 = temp_a0_5 - (u16) f->unk3C;
block_198:
                f->unk15E = (s16) (var_v0_9 / f->unk15C);
block_199:
                if (temp_s3 != 0) {
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                }
                break;
            case 19:                                /* switch 1 */
                if (func_0036BF00(f) != 0) {
                    f->unkC4 = (u16) f->unkC4 + 1;
                    f->unkBC = (u16) f->unkBC + 1;
                } else {
                    f->unkBC = 1;
                    f->unkC4 = 1;
                }
                temp_t2_7 = f->unk188;
                if ((s32) temp_t2_7->unk18 < f->unkC4) {
                    temp_v0_14 = temp_t2_7->unk18;
                    f->unkBC = (s16) temp_v0_14;
                    f->unkC4 = (s16) temp_v0_14;
                }
                temp_a1_7 = (u16) f->unk3A;
                f->unkF0 = 1;
                f->unk11E = 0xB;
                f->unk3C = (u16) f->unk3C - 0x8000;
                f->unkB8 = temp_t2_7;
                f->unkE0 = 0;
                f->unkE8 = 0;
                f->unkEC = 0;
                f->unk1A = temp_a1_7;
                temp_v0_15 = temp_t2_7->unk2C - 1;
                f->unk15C = temp_v0_15;
                if (temp_v0_15 <= 0) {
                    f->unk15C = 1;
                }
                var_v0_9 = temp_a1_7 - (u16) f->unk3C;
                goto block_198;
            case 20:                                /* switch 1 */
                if (func_0036BF00(f) != 0) {
                    f->unkC4 = (u16) f->unkC4 + 1;
                    f->unkBC = (u16) f->unkBC + 1;
                } else {
                    f->unkBC = 1;
                    f->unkC4 = 1;
                }
                temp_t2_8 = f->unk188;
                if ((s32) temp_t2_8->unk18 < f->unkC4) {
                    temp_v0_16 = temp_t2_8->unk18;
                    f->unkBC = (s16) temp_v0_16;
                    f->unkC4 = (s16) temp_v0_16;
                }
                f->unkB8 = temp_t2_8;
                if ((s16) f->unk40 < 0x4FA4) {
                    var_v0_10 = 4;
                } else {
block_216:
                    var_v0_10 = 6;
                }
block_217:
                f->unk11E = var_v0_10;
                f->unk4 = f->unk51C;
                f->unkC = f->unk524;
                f->unk126 = 1;
block_218:
                f->unkE0 = 0;
                f->unk15C = 0;
                f->unk15E = 0;
                break;
            case 21:                                /* switch 1 */
                if (func_0036BF00(f) != 0) {
                    f->unkC4 = (u16) f->unkC4 + 1;
                    f->unkBC = (u16) f->unkBC + 1;
                } else {
                    f->unkBC = 1;
                    f->unkC4 = 1;
                }
                temp_t2_9 = f->unk188;
                if ((s32) temp_t2_9->unk18 < f->unkC4) {
                    temp_v0_17 = temp_t2_9->unk18;
                    f->unkBC = (s16) temp_v0_17;
                    f->unkC4 = (s16) temp_v0_17;
                }
                f->unkB8 = temp_t2_9;
                if ((s16) f->unk40 < 0x4FA4) {
                    var_v0_10 = 2;
                } else {
                    goto block_216;
                }
                goto block_217;
            case 22:                                /* switch 1 */
                if (func_0036BF00(f) != 0) {
                    f->unkC4 = (u16) f->unkC4 + 1;
                    f->unkBC = (u16) f->unkBC + 1;
                } else {
                    f->unkBC = 1;
                    f->unkC4 = 1;
                }
                temp_t2_10 = f->unk188;
                if ((s32) temp_t2_10->unk18 < f->unkC4) {
                    temp_v0_18 = temp_t2_10->unk18;
                    f->unkBC = (s16) temp_v0_18;
                    f->unkC4 = (s16) temp_v0_18;
                }
                f->unk11E = 8;
                f->unk4 = f->unk51C;
                f->unkC = f->unk524;
                f->unk126 = 1;
                f->unkB8 = temp_t2_10;
                goto block_218;
            case 23:                                /* switch 1 */
                f->unkBC = 1;
                f->unk1A = (u16) f->unk3C;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                if (temp_s3 != 0) {
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                }
                var_v0_7 = 9;
                goto block_235;
            case 24:                                /* switch 1 */
                f->unkBC = 1;
                f->unk1A = (u16) f->unk44;
                f->unk11E = 8;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unkE0 = 0;
                goto block_314;
            case 25:                                /* switch 1 */
                f->unkE0 = 0;
                if (func_0036BF00(f) != 0) {
                    f->unkC4 = (u16) f->unkC4 + 1;
                    f->unkBC = (u16) f->unkBC + 1;
                } else {
                    f->unkBC = 1;
                    f->unkC4 = 1;
                }
                temp_t2_11 = f->unk188;
                if ((s32) temp_t2_11->unk18 < f->unkC4) {
                    temp_v0_19 = temp_t2_11->unk18;
                    f->unkBC = (s16) temp_v0_19;
                    f->unkC4 = (s16) temp_v0_19;
                }
                f->unk11E = 8;
                f->unk4 = f->unk51C;
                f->unkC = f->unk524;
                f->unk126 = 1;
                f->unkB8 = temp_t2_11;
                break;
            case 26:                                /* switch 1 */
            case 27:                                /* switch 1 */
                temp_v0_20 = (u16) f->unkC4 + 1;
                temp_t2_12 = f->unk188;
                f->unkC4 = temp_v0_20;
                f->unkBC = (u16) f->unkBC + 1;
                if ((s32) temp_t2_12->unk18 < temp_v0_20) {
                    temp_v0_21 = temp_t2_12->unk18;
                    f->unkBC = (s16) temp_v0_21;
                    f->unkC4 = (s16) temp_v0_21;
                }
                f->unkB8 = temp_t2_12;
                f->unk1A = (u16) f->unk3C;
                if ((s16) temp_t3 == 0x1A) {
                    f->unk11A = 1;
                } else {
                    f->unk11A = 2;
                }
                break;
            case 28:                                /* switch 1 */
                f->unkBC = 1;
                f->unk1A = (u16) f->unk3C;
                f->unk11E = 0xA;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unkE0 = 0;
                f->unk15C = 0;
                f->unk15E = 0;
                if (temp_s3 != 0) {
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                }
                f->unk170 = 0x18;
                break;
            case 29:                                /* switch 1 */
                if (func_0036BF00(f) != 0) {
                    f->unkC4 = (u16) f->unkC4 + 1;
                    f->unkBC = (u16) f->unkBC + 1;
                } else {
                    f->unkBC = 1;
                    f->unkC4 = 1;
                }
                temp_t2_13 = f->unk188;
                if ((s32) temp_t2_13->unk18 < f->unkC4) {
                    temp_v0_22 = temp_t2_13->unk18;
                    f->unkBC = (s16) temp_v0_22;
                    f->unkC4 = (s16) temp_v0_22;
                }
                f->unk11E = 0xA;
                f->unk1A = (u16) f->unk3C;
                f->unkB8 = temp_t2_13;
                f->unkE0 = 0;
                f->unk15C = 0;
                f->unk15E = 0;
                goto block_199;
            case 30:                                /* switch 1 */
                f->unkFA = 1;
                f->unk8 = f->unk520;
                f->unkFC = 4;
                f->unk184 = func_00374010(4)->unk0;
                func_00374148(f, o);
                temp_a2_2 = (u16) f->unk178;
                f->unkFE = 1;
                f->unk11D = 1;
                f->unkE0 = 0;
                f->unkBC = (s16) temp_a2_2;
                f->unkB8 = f->unk188;
                f->unk100 = (u16) f->unk100 + 1;
                f->unkC4 = (s16) temp_a2_2;
                f->unk160 = func_0036A2D0(f);
                f->unk11E = 0xB;
                if (!(f->unkC0->unk24 & 0x10)) {
block_261:
                    f->unk1A = (u16) f->unk44;
                } else {
block_260:
                    f->unk1A = (u16) f->unk44 + f->unk18C->unk0;
                }
block_262:
                f->unk15C = 0;
                f->unk15E = 0;
                if (temp_s3 != 0) {
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                }
                break;
            case 31:                                /* switch 1 */
            case 32:                                /* switch 1 */
                temp_a2_3 = (u16) f->unkE0;
                temp_t2_14 = f->unk188;
                f->unkBC = 1;
                f->unkC4 = 1;
                f->unkB8 = temp_t2_14;
                if ((s16) temp_a2_3 == 0) {
                    temp_a1_8 = (u16) f->unk3A - 0x8000;
                    f->unk3C = temp_a1_8;
                    temp_a0_6 = o->unk42;
                    switch (temp_a0_6) {            /* switch 2; irregular */
                    default:                        /* switch 2 */
                        var_v0_11 = temp_a1_8 - 0x4000;
                        if (temp_a0_6 != 3) {

                        } else {
block_272:
                            f->unk3C = var_v0_11;
                        }
                        break;
                    case 1:                         /* switch 2 */
                        var_v0_11 = temp_a1_8 + 0x4000;
                        goto block_272;
                    case 2:                         /* switch 2 */
                        var_v0_11 = temp_a1_8 - 0x8000;
                        goto block_272;
                    }
                    temp_a0_7 = (u16) f->unk3C;
                    f->unk15C = 0x10;
                    f->unk1A = temp_a0_7;
                    f->unk15E = (s16) ((s16) ((u16) o->unk3C - temp_a0_7) / 16);
                    goto block_278;
                }
                if ((s16) temp_a2_3 >= 0) {
                    var_v0_12 = temp_a2_3 << 0x10;
                    if (temp_t2_14->unk24 & 0x1000) {
                        f->unk3C = (u16) f->unk3C + (u16) f->unkC0->unk12;
                        temp_v1_5 = o->unk1A;
                        f->unk1A = temp_v1_5;
                        f->unk3C = (s16) temp_v1_5;
                        f->unk4 = o->unk51C;
                        f->unkC = o->unk524;
                        goto block_281;
                    }
                } else {
                    temp_v0_23 = o->unk1A;
                    f->unk1A = temp_v0_23;
                    f->unk3C = (s16) temp_v0_23;
block_278:
                    f->unk4 = o->unk51C;
                    f->unkC = o->unk524;
block_281:
                    var_v0_12 = temp_a2_3 << 0x10;
                }
                f->unk150 = 0;
                f->unk152 = 0;
                f->unk154 = 0;
                if (var_v0_12 < 0) {
                    f->unkE0 = 1;
                } else {
                    f->unkE0 = temp_a2_3 + 1;
                }
                break;
            case 36:                                /* switch 1 */
                if (f->unkE0 >= 0) {
                    if (((func_0036C1A0(f, o, f->unkC0)->unk2 != 0) && (f->unkC4 < (s32) f->unkC0->unk2C)) || (o->unk16A != 0)) {
                        f->unk120 = 1;
                    }
                }
                temp_a0_8 = f->unk188;
                f->unkF2 = 1;
                f->unkC4 = 1;
                f->unkBC = 1;
                f->unkB8 = temp_a0_8;
                if (!((u16) f->unkE0 & 0x8000)) {
                    temp_a1_9 = f->unk524;
                    temp_v0_24 = (u16) f->unk3C - 0x8000;
                    f->unk1A = temp_v0_24;
                    f->unk4 = f->unk51C;
                    f->unkC = temp_a1_9;
                    f->unk3C = (s16) temp_v0_24;
                    Snd_StopVoiceSeq(f);
                    goto block_295;
                }
                var_v0_13 = (u16) f->unkE0 << 0x10;
                if (temp_a0_8->unk24 & 0x1000) {
                    temp_v0_25 = (u16) f->unk3C + (u16) f->unkC0->unk12;
                    f->unk1A = temp_v0_25;
                    f->unk4 = f->unk51C;
                    f->unkC = f->unk524;
                    f->unk3C = (s16) temp_v0_25;
block_295:
                    var_v0_13 = (u16) f->unkE0 << 0x10;
                }
                var_v0_14 = -1;
                if (var_v0_13 <= 0) {
                    var_v0_14 = (u16) f->unkE0 - 1;
                }
                f->unkE0 = var_v0_14;
                func_0036A330(f, o, f->unkE0);
                f->unk150 = 0;
                f->unk152 = 0;
                f->unk154 = 0;
                break;
            case 37:                                /* switch 1 */
                var_v0_15 = f->unk18C->unk18;
                f->unk158 = var_v0_15;
                var_a0 = var_v0_15 + 1;
                var_a2 = var_v0_15 + 2;
block_300:
                f->unk158 = var_a0;
                f->unk150 = var_v0_15[0];
                f->unk158 = var_a2;
                f->unk152 = var_v0_15[1];
                f->unk154 = 8;
                f->unkBC = 1;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unk156 = (u16) f->unk3A + (f->unk18C->unk0 - 0x8000);
                f->unkE0 = 0;
                f->unk11E = 7;
                f->unk1A = (u16) f->unk44;
                f->unk15C = 0;
                f->unk15E = 0;
                goto block_314;
            case 38:                                /* switch 1 */
                f->unk8 = f->unk520;
                func_00374148(f, o);
                temp_a2_4 = (u16) f->unk178;
                f->unkE0 = 0;
                f->unkBC = (s16) temp_a2_4;
                f->unkB8 = f->unk188;
                f->unk11D = 1;
                f->unkFE = 1;
                f->unk100 = (u16) f->unk100 + 1;
                f->unkC4 = (s16) temp_a2_4;
                f->unk160 = func_0036A2D0(f);
                f->unk11E = 0xB;
                if (!(f->unkC0->unk24 & 0x10)) {
                    goto block_261;
                }
                goto block_260;
                goto block_262;
            case 39:                                /* switch 1 */
                f->unk160 = func_0036A2D0(f);
                temp_a3_2 = f->unk18C;
                temp_v0_26 = temp_a3_2->unk14;
                f->unk158 = temp_v0_26;
                f->unk158 = temp_v0_26 + 1;
                f->unk150 = temp_v0_26[0];
                f->unk158 = temp_v0_26 + 2;
                f->unk152 = temp_v0_26[1];
                f->unk154 = 8;
                f->unkBC = 1;
                f->unk160 = 0;
                f->unkC4 = 1;
                f->unk156 = (u16) f->unk3A + (temp_a3_2->unk10 - 0x8000);
block_309:
                f->unkB8 = f->unk188;
                f->unkE0 = 0;
                f->unk11E = 0xB;
                f->unk1A = (u16) f->unk3C;
                f->unk15C = 0;
                f->unk15E = 0;
                goto block_199;
            case 40:                                /* switch 1 */
                f->unk160 = func_0036A2D0(f);
                var_v0_15 = f->unk18C->unkC;
                f->unk158 = var_v0_15;
                var_a0 = var_v0_15 + 1;
                var_a2 = var_v0_15 + 2;
                goto block_300;
            case 41:                                /* switch 1 */
            case 42:                                /* switch 1 */
            case 43:                                /* switch 1 */
            case 44:                                /* switch 1 */
            case 45:                                /* switch 1 */
                f->unk160 = func_0036A2D0(f);
                temp_t1 = f->unk18C;
                temp_a2_5 = f->unk17A;
                temp_v0_27 = temp_t1->unk4;
                f->unk158 = temp_v0_27;
                temp_a1_10 = temp_v0_27 + 2;
                f->unk158 = temp_v0_27 + 1;
                f->unk150 = temp_v0_27[0];
                f->unk158 = temp_a1_10;
                f->unk152 = temp_v0_27[1];
                f->unk154 = 8;
                if (temp_a2_5 == 0x2C) {
                    var_v1_4 = (u16) f->unk44;
                } else {
                    var_v1_4 = (u16) f->unk3A;
                }
                f->unk156 = var_v1_4 + (temp_t1->unk0 - 0x8000);
                func_0036C128(f);
                f->unkBC = 1;
                f->unkC4 = 1;
                goto block_309;
            case 33:                                /* switch 1 */
                temp_a2_6 = (u16) f->unk3A;
                f->unkBC = 1;
                f->unk1A = temp_a2_6;
                f->unk4 = f->unk51C;
                f->unkC = f->unk524;
                f->unk11E = 8;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unk3C = (s16) temp_a2_6;
                goto block_125;
            case 34:                                /* switch 1 */
                f->unkBC = 1;
                f->unk1A = (u16) f->unk44;
                f->unk11E = 0xC;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unkE0 = 0;
                goto block_314;
            case 35:                                /* switch 1 */
                f->unkBC = 1;
                f->unk1A = (u16) f->unk44;
                f->unk11E = 0xD;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unkE0 = 0;
                goto block_314;
            default:                                /* switch 1 */
                temp_v1_6 = (u16) f->unk3A;
                f->unkBC = 1;
                f->unk3C = (s16) temp_v1_6;
                f->unkC4 = 1;
                f->unkB8 = f->unk188;
                f->unk1A = temp_v1_6;
                goto block_314;
            }
            temp_v1_7 = (u16) f->unk10C;
            f->unk10C = (s16) f->unk184;
            f->unk176 = (s16) temp_v1_7;
            f->unk184 = -1;
            f->unkC0 = f->unk188;
            f->unk188 = NULL;
            if (temp_t2->unk2C != 0) {
                if (temp_t2->unk2A != 0) {
                    f->unk118 = 2;
                } else if (temp_t2->unk28 != 0) {
                    f->unk118 = 1;
                } else {
                    goto block_323;
                }
            } else {
block_323:
                f->unk118 = 0;
            }
            var_v0_16 = 1;
            if (f->unk134 == 0) {
                var_v0_16 = (u16) f->unkC4;
            }
            f->unkC8 = (s16) var_v0_16;
            f->unk10E = (s16) (u16) f->unk17A;
            f->unk17E = 0;
            f->unk17A = 0;
            f->unk17C = 0;
        } else {
            f->unk134 = 0;
            f->unkC8 = (u16) f->unkC8 + 1;
            var_a0_2 = (u16) f->unkE4;
            if (f->unkE4 > 0) {
                var_v0_17 = var_a0_2 << 0x10;
                if (f->unkFE != 0) {
                    f->unkE4 = 0;
                    var_a0_2 = 0;
                    goto block_20;
                }
            } else {
block_20:
                var_v0_17 = var_a0_2 << 0x10;
            }
            temp_v0_28 = var_v0_17 >> 0x10;
            if (temp_v0_28 < 0) {
                var_v0_18 = var_a0_2 + 1;
                goto block_31;
            }
            if (temp_v0_28 <= 0) {
                goto block_33;
            }
            if (f->unkFE != 0) {
block_33:
                if (f->unkD6 >= 0) {
                    temp_a2_7 = f->unkC0;
                    temp_a1_11 = (u16) f->unkC4;
                    if (f->unkC4 == temp_a2_7->unk1B) {
                        if (f->unkD6 > 0) {
                            f->unkD6 = (u16) f->unkD6 - 1;
                            goto block_40;
                        }
                        if (f->unkD6 == 0) {
                            var_v0_19 = temp_a1_11 + 1;
                            f->unkB8 = temp_a2_7;
                            goto block_46;
                        }
                    } else {
                        f->unkC4 = temp_a1_11 + 1;
block_40:
                        f->unkBC = (u16) f->unkBC + 1;
                    }
                } else if (f->unkFE == 0) {
                    temp_v0_29 = (u16) f->unkC4 + (u16) f->unkDA;
                    f->unkC4 = temp_v0_29;
                    if ((temp_v0_29 << 0x10) <= 0) {
                        f->unkC4 = 1;
                    }
                    f->unkBC = (s16) (u16) f->unkC4;
                } else if (f->unkC4 != f->unkC0->unk1B) {
                    var_v0_19 = (u16) f->unkC4 + 1;
                    goto block_46;
                }
            } else {
                var_v0_18 = var_a0_2 - 1;
block_31:
                f->unkE4 = var_v0_18;
                if (f->unkE4 == 0) {
                    var_v0_19 = (u16) f->unkC4 + 1;
                    f->unkB8 = f->unkC0;
block_46:
                    f->unkBC = var_v0_19;
                    f->unkC4 = var_v0_19;
                }
            }
        }
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_004FFA80);
INCLUDE_ASM("asm/nonmatchings/battle_flow", Fighter_StepMove);
#endif


void func_0036BC50(Fighter *f) {
    MoveCmdState *p = &f->unk4C0;

    p->unkE = 0;
    p->unkF = 0;
    p->unk10 = 0;
    p->unk11 = 0;
    p->unk12 = 0;
    p->unk0.unk0 = 0;
    p->unk0.unk2 = 0;
    p->unk0.unk3 = 0;
    p->unk0.unk4 = 0;
    p->unk0.unk6 = 0;
    p->unk0.unk8 = 0;
    p->unk0.unkA = 0;
    p->unk0.unkB = 0;
    p->unk0.unkC = 0;
    p->unk0.unkD = 0;
}

void func_0036BC98(Fighter *f, MoveCmd *cmd) {
    f->unk4C0.unkE = 1;
    f->unk4C0.unk0 = *cmd;
}

void func_0036BCD0(Fighter *f, s32 arg1) {
    MoveCmdState *p = &f->unk4C0;

    p->unk10 = 0;
    p->unk11 = 0;
    if (arg1 != 0) {
        p->unkF = 0;
    }
    if (p->unkE != 0) {
        p->unkE = 0;
        if (p->unk0.unk0 != 0x8000) {
            if (p->unk0.unk0 > 0x8005) {
                p->unkF = 1;
                p->unk10 = 1;
            }
            if (p->unk0.unk0 == 0x10) {
                p->unk11 = 1;
            } else if (p->unk0.unk0 & 0x4000) {
                p->unk12 = 1;
            } else {
                p->unk12 = 0;
            }
        }
    }
}

void func_0036BD50(Fighter *f) {
    f->unk4D4.unk0 = 0;
    f->unk4E8.unk0 = 0;
}

void func_0036BD60(Fighter *f, MoveCmd *cmd) {
    f->unk4D4.unk0 = 1;
    f->unk4D4.unk4 = *cmd;
}

void func_0036BD98(Fighter *f) {
    f->unk4D4.unk0 = 0;
}

void func_0036BDA0(Fighter *f) {
    f->unk4E8 = f->unk4D4;
}

s32 func_0036BDD0(Fighter *f) {
    MoveCmdSlot *s = &f->unk4E8;

    if (s->unk0 == 0 || s->unk4.unk0 == 0x8000 || s->unk4.unk0 == 0x10) {
        return 1;
    }
    return 0;
}

s32 func_0036BEC0(Fighter *f);

/* The kind of a move about to start, adjusted to the current move (kinds 2, 0xC-0xF, 0x14, 0x15, 0x19) [I]. */
s16 func_0036BE08(Fighter *f, s16 kind) {
    s16 r = kind;

    switch (kind) {
    case 2:
        r = func_0036BEC0(f) ? 0x14 : kind;
        break;
    case 0xC:
    case 0xD:
    case 0xE:
    case 0xF:
        r = f->unk188->unk2C ? kind : 0xB;
        break;
    case 0x14:
    case 0x15:
        r = func_0036BEC0(f) ? kind : 0xB;
        break;
    case 0x19:
        r = func_0036BEC0(f) ? 0x16 : kind;
        break;
    case 0x10:
        r = 0x10;
        break;
    }
    return r;
}

s32 func_0036BEC0(Fighter *f) {
    struct FighterMove *m = f->unk188;
    s32 r = 0;

    if (m->unk2C != 0 && f->unk178 + 1 < m->unk2D) {
        r = m->unk18 >= m->unk2D;
    }
    return r;
}

#if 0
/* NON_MATCHING: the separate `return 1` block for unk2C == 0 is merged with the final one (2 instructions short;
 * decomp-permuter keeps them apart only with a fake `if (f) return 1; else return 1;`) */
s32 func_0036BF00(Fighter *f) {
    struct FighterMove *m = f->unk188;

    if (f->unkF8 == 3) {
        return 0;
    }
    if (m->unk2C != 0) {
        if (m->unk18 >= m->unk2C && m->unk2D >= f->unkC4) {
            return 1;
        }
        return 0;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036BF00);
#endif

INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_004FFBA0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036BF58);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036BF8C);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036BF94);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036BFA0);

void func_0036C128(Fighter *f) {
    u16 st = f->unk17A;

    if ((u16)(st - 0x29) < 2 || (s16)st == 0x2D) {
        if (f->unk16E != 0) {
            if (f->unk174 == (s16)st) {
                f->unk16C++;
            }
        } else {
            f->unk16C = 0;
        }
        f->unk16E = f->unk188->unk18 + 10;
        f->unk174 = st;
    }
}

Unk5E8978 *func_0036C1A0(Fighter *f, Fighter *o, struct FighterMove *m) {
    Unk5E8978 *e = &D_005E8978[m->unk14];

    while (e->unk0 != 0xFFFF) {
        if (func_00369008(f, o, (s16)e->unk0) != 0) {
            break;
        }
        e++;
    }
    return e;
}

void func_0036C228(Fighter *f, Fighter *o, struct FighterMove *m) {
    Unk5E8978 *e = func_0036C1A0(f, o, m);
    s32 kind = (e->unk2 & 0xC000) >> 14;

    f->unkCA = e->unk2 & 0x3FFF;
    f->unk11B = kind;
    if (kind == 1) {
        f->unkCA = 0;
    }
}

s32 func_003800D8(s32 n);
s16 func_003AD8E0(s16 x);
s32 func_003ADA08(u8 *set, s32 i);
extern u8 D_0043B968[];

/* Picks at random (func_003800D8, the CPU's generator) a character other than `cur` among 0-0x21, leaving out
 * 0xB, 0xE-0x11, 0x14-0x16, 0x1B, 0x1D and 0x21 [I: characters that have no such variant]. */
#if 0
/* NON_MATCHING: the count and `cur` are allocated to swapped registers (a3 / t0) */
s16 func_0036C278(s32 cur) {
    s32 list[34];
    s32 i, c;
    s32 n = 0;
    s32 *p = list;

    for (i = 0; i < 0x22; i++) {
        switch (i) {
        case 0xB:
        case 0xE:
        case 0xF:
        case 0x10:
        case 0x11:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x1B:
        case 0x1D:
        case 0x21:
            break;
        default:
            if (cur != i) {
                *p++ = i;
                n++;
            }
            break;
        }
    }
    if (n != 0) {
        c = list[func_003800D8(n)];
    } else {
        c = cur;
    }
    return func_003AD8E0(c * 8);
}
#else
INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_004FFC60);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036C278);
#endif

/* The character a move of kind k turns the fighter into [I]: k = 0x10 a random one allowed by D_0043B968 other than
 * `cur`, 0xF and 0x21; k = 0x23 a random one outside the list of func_0036C278; otherwise character k - 1. */
s16 func_0036C318(s16 cur, s16 k) {
    s16 id = k - 1;
    u8 c;

    if (id == 0xF) {
        for (;;) {
            c = func_003800D8(0x22);
            if (!func_003ADA08(D_0043B968, c)) {
                continue;
            }
            if (cur == func_003AD8E0(c * 8) || c == 0xF || c == 0x21) {
                continue;
            }
            return func_003AD8E0(c * 8);
        }
    }
    if (id == 0x22) {
        for (;;) {
            c = func_003800D8(0x22);
            switch (c) {
            case 0xB:
            case 0xE:
            case 0xF:
            case 0x10:
            case 0x11:
            case 0x14:
            case 0x15:
            case 0x16:
            case 0x1B:
            case 0x1D:
            case 0x21:
                continue;
            }
            return func_003AD8E0(c * 8);
        }
    }
    return func_003AD8E0(id * 8);
}

void func_0036C420(Fighter *f, s32 arg1) {
    f->unk41C = arg1;
    f->unk424 = arg1 != 0 ? 2 : 1;
    f->unk42C = f->unk1E;
    func_0036C448(f);
}

void func_0036C448(Fighter *f) {
    f->unk420 = f->unk424;
    f->unk428 = f->unk42C;
    f->unk418 = f->unk41C;
}

void func_0036C468(Fighter *f) {
    f->unk414 = 1;
    f->unk410 = 0;
    Fighter_ResetInputHistory(f);
}

void func_0036C480(Fighter *f) {
    f->unk414 = 0;
    Fighter_ResetInputHistory(f);
}

void func_0036C490(Fighter *f) {
    f->unk414 = 1;
    f->unk410 = 1;
}

void func_0036C4A0(Fighter *f) {
    f->unk410 = 1;
}

void func_0036C4B0(Fighter *f) {
    f->unk420 = 0;
    f->unk414 = 1;
}

void Fighter_UpdateInput(Fighter *f) {
    if (f->unk414 != 0) {
        func_0036C680(f);
    }
    Prac_RecEnd(f->unk1E);
}

void func_0036C4F8(Fighter *f) {
    func_0037C8C8(f->unk1E);
    f->unk420 = 3;
}

void Fighter_ResetInputHistory(Fighter *f) {
    FighterUnk438 *s = &f->unk438;
    s16 *p = s->unk4;
    s32 i;

    for (i = 0; i < 0x3C; i++) {
        *p++ = 5;
    }
    s->unk0 = 0;
    s->unk7C = 0x200;
    s->unk7E = 0;
    s->unk80 = 0;
    f->unk410 = 0;
    f->unk140 = 0;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036C578);

s32 func_0035B9E8(s32 side);
void func_0036C578(s32 i, s32 *held, s32 *pressed);
void func_00380A50(Fighter *f, u16 *held, u16 *pressed);
u16 func_0037C950(s32 player);
void Prac_RecInput(s32 player, u16 buttons);
void func_0037CFA0();
u16 Input_MirrorLeftRight(s32 side, u16 b);
void Fighter_RecordInput(Fighter *f, u16 held, u16 pressed);
extern s32 D_005F6678;

/* The buttons of a fighter for this frame, from its input source unk420 (2, the CPU, while D_005F6678 is set):
 * 0 none, 1 the pad of its port unk428 (mirrored for side 1), 2 the CPU opponent, 3 the practice playback, 4 the
 * buttons forced by the practice dummy (unk430). Held buttons go to unk432, their rising edge to unk436, and the
 * frame is recorded in the input history (Fighter_RecordInput). Pad bit 0x04 is masked while unk402 is set. */
void func_0036C680(Fighter *f) {
    s32 src = f->unk420;
    u16 b;
    u16 cur, prev;

    if (D_005F6678 != 0) {
        src = 2;
    }
    switch (src) {
    case 0:
    default:
        f->unk436 = 0;
        f->unk434 = 0;
        f->unk432 = 0;
        break;
    case 1: {
        s32 side = func_0035B9E8(f->unk1E);

        func_0036C578(f->unk428, &D_005F5C40[f->unk428], &D_005F5C48[f->unk428]);
        b = Input_MirrorLeftRight(side, D_005F5C40[f->unk428]);
        f->unk432 = b;
        if (f->unk402 != 0) {
            f->unk432 = b & 0xFFFB;
        } else if (D_005F5C40[f->unk428] & 0x400) {
            if (f->unk418 == 0 && f->unk2A == 0x21) {
                f->unk432 = b | 0x400;
            } else {
                f->unk432 = f->unk432 | 4;
            }
        }
        break;
    }
    case 2:
        func_00380A50(f, &f->unk432, &f->unk436);
        f->unk434 = 0;
        break;
    case 3: {
        u16 play = func_0037C950(f->unk1E);

        f->unk430 = play;
        f->unk432 = play;
        break;
    }
    case 4:
        f->unk432 = f->unk430;
        break;
    }
    if (f->unk402 != 0) {
        f->unk432 &= 0xFFFB;
    }
    if (src == 1 || src == 2) {
        Prac_RecInput(f->unk1E, f->unk432);
    }
    cur = f->unk432;
    if (f->unk410 != 0) {
        f->unk410 = 0;
        f->unk434 = cur;
    }
    prev = f->unk434;
    f->unk434 = cur;
    f->unk436 = cur & (cur ^ prev);
    f->unk140 = (f->unk436 >> 10) & 1;
    func_0037CFA0(f->unk436);
    Fighter_RecordInput(f, f->unk432, f->unk436);
}

void Fighter_RecordInput(Fighter *f, u16 held, u16 pressed) {
    if (D_005F5C38[f->unk1E] >= 0x1D15 || D_005F5EF4 != 0) {
        held &= 0xFFFB;
        pressed &= 0xFFFB;
    }
    f->unk438.unk0 = f->unk438.unk0 > 0x3A ? 0 : f->unk438.unk0 + 1;
    f->unk438.unk4[f->unk438.unk0] = func_0036C918(held, pressed);
    func_0036C988(f, held, pressed);
    func_0036CA68(f, held, pressed);
}

#if 0
/* NON_MATCHING: the original masks `pressed` with andi and shifts it with dsrl (64-bit); ours keeps it in a register
 * and uses srl */
u16 func_0036C918(u16 held, u16 pressed) {
    u16 dir = func_0036CFC0(held);
    u16 b = (pressed >> 3) & 0x10;

    if (pressed & 0x10) {
        b |= 0x20;
    }
    if (pressed & 0x40) {
        b |= 0x40;
    }
    if (pressed & 0x20) {
        b |= 0x80;
    }
    if (pressed & 4) {
        b |= 0x100;
    }
    return b | dir;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036C918);
#endif

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036C988);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036CA68);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036CAF8);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036CBE8);

#if 0
/* NON_MATCHING: the shared `return 1` block is placed after the second test; the original keeps it after the first */
s32 func_0036CD10(u16 cmd, u16 held, u16 pressed) {
    u16 dir;
    s32 r;

    if ((cmd & 0x401F) == 0x10) {
        return 1;
    }
    if (!(cmd & 0x10)) {
        dir = cmd & 0x400F;
        if (dir != 0 || pressed != 0) {
            r = 0;
            if (dir & pressed) {
                r = dir == (dir & held);
            }
            return r;
        }
        return 1;
    }
    return cmd & (pressed & 0x400F);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036CD10);
#endif

s32 func_0036CD78(u16 a, u16 b) {
    u16 hi;
    u16 lo;
    u16 mask;

    if ((a & 0xF) != (b & 0xF)) {
        return 0;
    }
    hi = a >> 8;
    lo = b >> 4;
    if ((hi & 0x1F) == 0x10) {
        return 1;
    }
    mask = 0xF;
    if (hi & 0x10) {
        return hi & mask & lo;
    }
    return ((lo ^ hi) & mask) == 0;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036CDD8);

/* 8-aligned after the jump table of func_0036C680 (0x4FFD34). */
RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_004FFD38);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_004FFFD4);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_004FFFE0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036CF30);

u16 Input_MirrorLeftRight(s32 side, u16 b) {
    if (side != 0) {
        u16 m = (b & 0x2000) ? 0x8000 : 0;

        if (b & 0x8000) {
            m |= 0x2000;
        }
        b = m | (b & 0x5FFF);
    }
    return b;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036CFC0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036D010);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500000);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500010);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_0050003C);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500104);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500108);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500114);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500120);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500204);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500240);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500300);

extern TeamMember D_00440940[2][2];
extern s32 D_005F5CD0;
extern u16 D_005F5CBC;
extern s32 D_005F5F00;
extern u16 D_005F5CB6;
extern s32 D_005F5EFC;
extern s32 D_005F6670[2];
void func_0035BFF8(s32 kind, HitContact *hc);
void func_0035C0D8(s16 n, Fighter *f, s16 side, HitContact *hc);
s32 func_003CF278(u8 c0, u8 k0, u8 c1, u8 k1, s32 kind, s32 n);
s32 func_003AE3C0(Fighter *f);
void func_00364678(Fighter *f);
void func_0038D508(void);
s32 func_003ABCC0(void);
void func_00393718(s32 a, u16 b);
void func_003645D8(s32 n);
void func_0035C230(void);
void func_0035C2B0(void);
void func_003B4DC0(Fighter *a, Fighter *b);
void func_00390E08(Fighter *a, Fighter *b);
void func_00395400(s32 n, Fighter *a, Fighter *b);
void func_00380560(void);
void func_003C1C98(void);
void func_003533E8(s32 n);
void func_00378018(void);
void func_00366DB0(u16 n);
void func_003465E0(void);
void func_003A8B08(void);
void func_00344318(void);
void func_003A82C8(Fighter *f);
void func_003A96B0(s32 n);
void func_003790D8(void);
void func_00346600(void);
void func_00344BE0(void);
void func_003A42B8(void);
void func_003A5FA0(void);
void func_003A7198(void);
void func_0037D630(void);
void func_003753A8(void);
void func_003751A0(u16 n);
void func_00375868(u16 n);
void func_003768C0(Fighter *a, Fighter *b, s32 n);
void func_003772D8(void);
void func_00373038(Fighter *a, Fighter *b);
void func_00373380(Fighter *f);

/* Start of a round (from Mode_Battle): the round number, the round timer in frames from the time setting
 * D_0043B7F8[0xB] (0x870, 0xB04, 0x10A4, 0x1608, 0x1B30), the round flags, then the reset of both fighters
 * (func_0036E138, func_00373038) and of the systems around them. */
#if 0
/* NON_MATCHING: the original rebuilds the two fighter addresses after func_00395400 and keeps their %hi parts in
 * s3 / s4 (a 0x60 frame) */
void func_0036D5D0(void) {
    Fighter *a = gFighters;
    Fighter *b = &D_00884EE0;

    gRoundNumber++;
    D_005F5EE8 = 0;
    D_005F5EEC = 0;
    switch (D_0043B7F8[0xB]) {
    case 0:
        gRoundTimer = 0x870;
        break;
    case 1:
        gRoundTimer = 0xB04;
        break;
    case 2:
        gRoundTimer = 0x10A4;
        break;
    case 3:
        gRoundTimer = 0x1608;
        break;
    case 4:
    case 5:
        gRoundTimer = 0x1B30;
        break;
    }
    D_005F5EF4 = 0;
    gRoundPhase = 0;
    D_005F5EE0 = 0;
    D_005F5F08 = 0;
    D_005F5EE4 = 0;
    D_005F5F0C = 0;
    D_005F5CAC = 0;
    func_0038D508();
    if (D_005F5CBC != 0 && func_003ABCC0() == 0) {
        func_00393718(D_005F5CD0, D_005F5CB6);
    }
    b->unk127 = 1;
    a->unk127 = 1;
    D_005F5EFC = 0;
    D_005F5F00 = 0;
    D_005F5EF8 = 0;
    func_0036E138();
    func_003645D8(2);
    func_00373380(a);
    func_00373380(b);
    func_003772D8();
    func_0035C230();
    func_0035C2B0();
    func_00373038(a, b);
    func_003B4DC0(a, b);
    func_00390E08(a, b);
    func_00395400(0, a, b);
    func_00380560();
    func_003C1C98();
    D_005F6670[0] = 0;
    D_005F6670[1] = 0;
    D_00440940[0][0].unk2C = D_00440940[0][0].unk10;
    D_00440940[0][1].unk2C = D_00440940[0][1].unk10;
    D_00440940[1][0].unk2C = D_00440940[1][0].unk10;
    D_00440940[1][1].unk2C = D_00440940[1][1].unk10;
    func_003533E8(0);
    func_00378018();
    func_00366DB0(D_005F5CB6);
    D_0043B7F8[0x3A] = 2;
    D_005F5F10 = 1;
    D_0043B7F8[0x39] = 0;
    func_003465E0();
    func_003A8B08();
    func_00344318();
    func_003A82C8(a);
    func_003A82C8(b);
    func_003A96B0(0);
    if (D_0043B7F8[0] == 7) {
        func_003790D8();
        func_00346600();
        func_00344BE0();
        func_003A42B8();
        func_003A5FA0();
        func_003A7198();
    }
    func_0037D630();
    func_003753A8();
    func_003751A0(D_005F5CB6);
    func_00375868(D_005F5CB6);
    func_003768C0(a, b, 1);
}
#else
INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_00500400);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036D5D0);
#endif

s32 Battle_UpdateFrame(void) {
    Fighter *f0 = &gFighters[0];
    Fighter *f1 = &D_00884EE0;

    gFighters[0].unk24 = f0->unk1E + gFighters[0].unk0 * 2;
    D_00884EE0.unk24 = f1->unk1E + D_00884EE0.unk0 * 2;
    func_003C1B38();
    if (D_0043B7F8[0] == 7) {
        func_003796B8();
    }
    if (D_005F5EE8 == 0) {
        D_005F5EE4 = 0;
        D_005F5F0C = 0;
        D_005F5F08 = 0;
    } else {
        Battle_UpdatePause();
    }
    D_005F5F10 = 1;
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0) {
        func_003D3528();
        D_005F5EF8 = func_00364B50();
        Battle_UpdateRound(f0, f1);
        if (D_005F5EF8 != 0) {
            D_005F5F10 = func_00365D00();
            func_0036DDD0(f0, f1);
            func_0035C320();
            func_00341AE8(f0, f1);
            func_00371B18(f0, f1);
            func_00378150(f0, D_005F5F10);
            func_00378150(f1, D_005F5F10);
            func_00378600(f0);
            func_00378600(f1);
            func_003A6390(f0, D_005F5F10);
            func_003A6390(f1, D_005F5F10);
            func_003A3148((s16)D_005F5F10);
            func_003DB698(D_005F5F10);
            Snd_UpdateDelayed();
        } else {
            gBattleParity = (gBattleParity + 1) & 1;
            func_0036EA80(f0, f1);
            func_0036DDD0(f0, f1);
            func_0035C320();
            func_00341AE8(f0, f1);
            func_0034F288(f0);
            func_0034F288(f1);
            Battle_StepMoves(f0, f1);
            Battle_UpdateFacing(f0, f1);
            Battle_UpdateDistance();
            func_00371A48();
            func_0036FB40(f0, f1);
            if (f0->unkE0 == 0 && f1->unkE0 == 0) {
                func_003772F0(f0, f1);
            }
            func_00371B18(f0, f1);
            func_00378150(f0, 1);
            func_00378150(f1, 1);
            func_00378600(f0);
            func_00378600(f1);
            func_003BAEF0();
            func_003A6390(f0, 1);
            func_003A6390(f1, 1);
            Fighter_UpdateMotion(f0);
            Fighter_UpdateMotion(f1);
            func_0034F3D8(f0);
            func_0034F3D8(f1);
            if (f0->unk137 != 0 && f0->unk127 != 0) {
                func_0035C240(&gFighters[0].unk51C);
                func_0035C2C8(0);
                func_003D3C68((f32)f0->unk51C, (f32)f0->unk520, (f32)-f0->unk524, (f32)f0->unk122);
            }
            if (f1->unk137 != 0 && f1->unk127 != 0) {
                func_0035C240(&D_00884EE0.unk51C);
                func_0035C2C8(0);
                func_003D3C68((f32)f1->unk51C, (f32)f1->unk520, (f32)-f1->unk524, (f32)f1->unk122);
            }
            func_00377AE8(f0, f1);
            if (gRoundPhase < 0xE && func_003D3228(0, 0) == 0 && func_003D3228(1, 0) == 0) {
                Battle_ApplyVelocity(f0, f1);
            }
            Battle_ResolveHits(f0, f1);
            func_00371C08(f0, f1);
            func_00371C08(f1, f0);
            func_003729B0(f0, f1);
            func_00372BA8(f0, f1);
            Fighter_ApplyDamage(f0, f1);
            if (D_005F5F24 == 0 && func_0036E178() == 0) {
                if (f0->unk127 != 0) {
                    func_00375B60(f0, f1);
                }
                if (f1->unk127 != 0) {
                    func_00375B60(f1, f0);
                }
            }
            Snd_UpdateDelayed();
            if (func_0036DE10() == 0) {
                func_0035C238();
            }
            if (D_0043B7F8[0] == 7) {
                func_0037A258();
            }
            func_0037D668(f0);
            func_0037D668(f1);
            Fighter_UpdateInput(f0);
            Fighter_UpdateInput(f1);
            f0->unkC6 = f0->unkC4;
            f1->unkC6 = f1->unkC4;
            func_00372A10(f0, f1);
            D_005F5EEC = D_005F5EE8;
            func_003DB690();
            func_00372E98(f0, f1);
        }
    } else {
        func_003D3528();
        if (D_005F5EF8 != 0) {
            func_00341AE8(f0, f1);
            func_00371B18(f0, f1);
            func_00378150(f0, 1);
            func_00378150(f1, 1);
            func_00378600(f0);
            func_00378600(f1);
            func_003A6390(f0, 0);
            func_003A6390(f1, 0);
            func_003A3148(0);
            func_003DB690();
        } else {
            func_0036EA80(f0, f1);
            func_00341AE8(f0, f1);
            func_00371B18(f0, f1);
            func_00378600(f0);
            func_00378600(f1);
            func_003DB690();
        }
    }
    return func_0036E158();
}

void func_0036DDD0(Fighter *a, Fighter *b) {
    func_0038D518(a, b, &gRoundPhase);
    func_0035B9A8(a, b);
}

s32 func_0036DE10(void) {
    return (u32)(gRoundPhase - 0x11) < 2;
}

s32 func_0036DE20(void) {
    return gRoundPhase == 0xD;
}

s32 func_0036DE30(void) {
    s32 r = 0;

    if (gRoundPhase == 0x10 || gRoundPhase == 0xE || gRoundPhase == 0xF) {
        r = 1;
    }
    return r;
}

s32 func_0036DE60(void) {
    s32 r = 0;

    if (func_0036DE10() != 0 || func_0036DE30() != 0) {
        r = 1;
    }
    return r;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", Battle_UpdatePause);

void func_0036E138(void) {
    D_005F5E90 = 0;
    D_005F5E8C = 0;
    gBattleParity = gFrameCount & 1;
    D_005F5E88 = 0;
    D_005F5F28 = 0;
}

s32 func_0036E158(void) {
    return D_005F5E88;
}

s32 func_0036E160(void) {
    return (u32)(D_005F5E88 - 1) < 2;
}

s32 func_0036E170(void) {
    return D_005F5E90;
}

s32 func_0036E178(void) {
    s32 r = 0;

    if (D_005F5E90 != 0) {
        r = D_005F5E88 == 5;
    }
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_00500420);
INCLUDE_ASM("asm/nonmatchings/battle_flow", Battle_UpdateRound);

extern u16 D_005F5C74;
void func_003C2EF8(void);
void func_003BB9C0(void);
void func_003C2D48(Fighter *a, Fighter *b);
void func_0036EDE0(void);
void func_0036EC50(void);
void func_0036EF58(void);
void func_0036EEE0(void);
void func_0036F0D8(void);
void func_0036F010(void);
s32 func_0039C778(void);
s32 func_0035A5D0(void);
void func_003BBE78(void);
void func_003C2F40(void);
void func_003BC540(void);

/* The round texts and their sounds, by the end-of-round step D_005F5E88: 0 "ROUND %d" / "READY!" (func_0036EDE0,
 * func_0036EC50), 2 the K.O. / time-up texts (func_0036EF58), 4 the win / lose / draw texts (func_0036F0D8). */
void func_0036EA80(Fighter *a, Fighter *b) {
    if (D_0043B7F8[0] == 9) {
        func_003C2EF8();
        return;
    }
    if (D_005F5CBC == 0 && D_0043B7F8[0x11] == 0) {
        switch (D_005F5E88) {
        case 0:
            if (D_005F5EE8 != 0) {
                func_0036EDE0();
                func_0036EC50();
            }
            break;
        case 1:
            func_003BB9C0();
            break;
        case 2:
            func_0036EF58();
            if (D_005F5CAC & 1) {
                func_003C2D48(a, b);
            }
            if (D_005F5F08 == 0) {
                func_0036EEE0();
            }
            break;
        case 3:
            if (D_005F5CAC & 1) {
                func_003C2D48(a, b);
            }
            break;
        case 4:
            func_0036F0D8();
            if (D_005F5CAC & 1) {
                func_003C2D48(a, b);
            }
        case 5:
            if (D_0043B7F8[0x1F] != 0 || D_005F5F00 == 0 || D_005F5F08 != 0) {
                func_0036F010();
            }
            break;
        }
    }
    if (D_005F5C74 != 0 && func_0039C778() == 0 && func_0035A5D0() == 0) {
        if (D_0043B7F8[4] != 0) {
            func_003BBE78();
        }
        func_003C2F40();
        func_003BC540();
    }
}

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500458);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EC50);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500470);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500488);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004A0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EDE0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EEE0);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004B0);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004C0);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004D8);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004F0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EF58);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F010);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500500);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500510);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500520);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500538);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500548);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500558);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F0D8);

s32 Battle_IsRoundOver(Fighter *a, Fighter *b) {
    s32 r = 0;

    if (a->unk40C == 0 || b->unk40C == 0 || gRoundTimer <= 0) {
        r = 1;
    }
    return r;
}

s32 func_0036F3F8(void) {
    return D_005F5CAC & 1;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F408);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F570);

s32 func_0036F658(void) {
    return (D_005F5CAC >> 3) & 1;
}

s32 func_0036F668(void) {
    return (D_005F5CAC >> 4) & 1;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F678);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F6B8);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F860);

void func_0036F8D8(Fighter *f) {
    if (f->unk188 != NULL && f->unk12F == 0 && f->unk4C0.unk12 != 0) {
        f->unk188 = NULL;
        f->unk178 = f->unkC0->unk18;
    }
}

void func_0036F918(void) {
    func_00364698();
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F920);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_00500570);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_005005B0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036FB40);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00370E20);

#if 0
/* NON_MATCHING: the original clamps with slt / movz on a sign-extended v and an unextended lim; ours branches */
s16 func_00370E98(s16 v, s32 lim) {
    if (v >= 0) {
        return lim < v ? lim : v;
    }
    lim = -lim;
    return v < lim ? lim : v;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00370E98);
#endif

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00370ED0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00370F28);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_005005E0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00371258);

u32 func_003712C0(void) {
    u32 r = 0;
    s32 i;

    for (i = 0; i < 4; i++) {
        if (func_003B00F0(i) != 0) {
            r |= D_00415728[i];
        }
    }
    return r;
}

void Battle_UpdateFacing(Fighter *a, Fighter *b) {
    s32 dx = b->unk51C - a->unk51C;
    s32 dz = b->unk524 - a->unk524;
    u16 angle = Math_Atan2(dz, dx);

    D_005F5C2C = angle;
    a->unk3A = angle;
    a->unk48 = ((0x400 - D_00415808[1] % 0x1000) * 16) % 0xFFFF;
    b->unk3A = angle - 0x8000;
    b->unk48 = a->unk48 - 0x8000;
}

#if 0
/* NON_MATCHING: register allocation of the loop (the original keeps the table bases in t1 / t3 / t4) */
void func_003713F0(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        D_00879610[i].unk0 = gFighters[i].unk4;
        D_00879610[i].unk8 = gFighters[i].unkC;
    }
    D_005F6BEC = 0;
    D_005F6BE0 = 0x64;
    D_005F6BE8 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003713F0);
#endif

INCLUDE_ASM("asm/nonmatchings/battle_flow", Battle_UpdateDistance);

void func_00371A48(void) {
    s32 *pos[2];
    s32 i;

    pos[0] = &gFighters[0].unk51C;
    pos[1] = &D_00884EE0.unk51C;
    for (i = 0; i < 2; i++) {
        s32 *p = pos[i];
        s32 dx = D_00879610[1 - i].unk0 - p[0];
        s32 dz = D_00879610[1 - i].unk8 - p[2];

        D_005F5C38[i] = Math_Isqrt(dx * dx + dz * dz);
    }
}

void func_00371B18(Fighter *a, Fighter *b) {
    func_003532A0();
    if (a->unk127 != 0) {
        func_0034E8B0(a);
    }
    if (b->unk127 != 0) {
        func_0034E8B0(b);
    }
}

void func_00371B78(Fighter *f) {
    f->unk3B8 = f->unk51C;
    f->unk3BC = f->unk520;
    f->unk3C0 = f->unk524;
}

void Fighter_UpdateMotion(Fighter *f) {
    f->unk3C4 = f->unk51C - f->unk3B8;
    f->unk3C8 = f->unk520 - f->unk3BC;
    f->unk3CC = f->unk524 - f->unk3C0;
    func_00371B78(f);
    if (f->unk122 < f->unk3C8) {
        f->unk122 = f->unk3C8;
    }
}

void func_00371C08(Fighter *a, Fighter *b) {
    a->unk3D0 = a->unk19C - a->unk190;
    a->unk3D4 = a->unk1A0 - a->unk194;
    a->unk3D8 = a->unk1A4 - a->unk198;
    if (a->unk132 != 0) {
        b->unk3DC = a->unk3D0;
        b->unk3E0 = a->unk3D4;
        b->unk3E4 = a->unk3D8;
    }
}

void Battle_ApplyVelocity(Fighter *a, Fighter *b) {
    struct FighterMove *m = func_00368248(0x1019);
    Fighter *f;
    s32 i;

    if ((a->unkC0 != m || a->unkC4 != 1) && (b->unkC0 != m || b->unkC4 != 1) && a->unkE0 == 0 && b->unkE0 == 0) {
        func_00377AE0(a, b);
        f = a;
        for (i = 0; i < 2; i++) {
            f->unk4 += f->unk3E8;
            f->unk8 += f->unk3EC;
            f->unkC += f->unk3F0;
            f->unk51C += f->unk3E8;
            f->unk520 += f->unk3EC;
            f->unk524 += f->unk3F0;
            f = b;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", Battle_ResolveHits);

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* The damage of the frame (docs/systems/fighter.md section 5.6), once per frame for both fighters, each in turn the
 * defender d of the other o. Life (unk3F4), red life (unk3F8) and maximum (unk3FC) are 20.12 fixed point; damage is
 * whole points. unk13D applies scripted damage (1: o's, 2: d's own) or a heal (3); a hit (unk12F) takes |o->unkCA|,
 * as chip damage when guarded or scaled for counter hits, attacks started early, juggles and o's unk400; the red life
 * drops by 0.6 (1.4 in a juggle) of the damage, never below the life. A fighter at 0 red life, or at 0 life (unless
 * func_003AE3C0 is 0), is down (unk40C = 0); the first one down calls func_00364678 with the other. */
void Fighter_ApplyDamage(Fighter *a, Fighter *b) {
    Fighter *d;
    Fighter *o;
    HitContact *hc;
    Fighter *p;
    s32 i;
    s32 chip;
    s32 life;
    s32 n;
    s32 an;
    s32 dmg;
    s32 base;
    s32 win;
    s32 red;

    chip = 0;
    for (i = 0, p = a; i < 2; i++) {
        p->unk13A = 0;
        p->unk138 = 0;
        p->unk146 = 0;
        p->unk142 = 0;
        p->unk144 = 0;
        p->unk130 = 0;
        p->unk131 = 0;
        p = b;
    }

    for (i = 0, d = a, o = b; i < 2; i++) {
        hc = &d->unk39C;
        life = d->unk3F4;
        if (o->unk13D == 1) {
            n = o->unk14A;
            if (n == 0) {
                n = o->unkCA;
            }
            an = ABS(n);
            d->unk142 += an;
            if (o->unk16A == 0 && d->unk16A == 0 && o->unk400 != 0) {
                n = n * 13 / 10;
                an = ABS(n);
            }
            d->unk3F4 = life - (n << 12);
            d->unk146 += n;
            d->unk144 += an;
            d->unk3F8 = (f32)d->unk3F8 - (f32)(n << 12) * 0.6f;
        }
        if (d->unk13D == 2) {
            n = d->unk14A;
            if (n == 0) {
                n = d->unkCA;
            }
            an = ABS(n);
            d->unk142 += an;
            if (o->unk16A == 0 && d->unk16A == 0 && o->unk400 != 0) {
                n = n * 13 / 10;
                an = ABS(n);
            }
            d->unk3F4 -= n << 12;
            d->unk146 += n;
            d->unk144 += an;
            d->unk3F8 = (f32)d->unk3F8 - (f32)(n << 12) * 0.6f;
        }
        if (d->unk13D == 3) {
            n = d->unk14A;
            if (n == 0) {
                n = o->unkCA;
            }
            d->unk3F4 += n << 12;
        }
        if (d->unk12F != 0) {
            dmg = o->unkCA;
            dmg = ABS(dmg);
            base = dmg;
            if (dmg > 0) {
                if ((d->unkD2 & o->unkD0) && (s16)d->unk40 < 0x4000) {
                    if (D_0043B7F8[0xC] != 0 || o->unk16A != 0) {
                        d->unk144 += ABS(o->unkCA / 10);
                        chip = o->unkCA * 0xA000 / 100;
                        d->unk130 = 1;
                        d->unk3F4 -= chip;
                        D_005F6670[d->unk1E] = 1;
                    }
                    if (d->unk3F4 > 0) {
                        func_0035BFF8(2, hc);
                        d->unk130 = 1;
                    } else {
                        func_0035C0D8(chip >> 16, d, 1 - i, hc);
                        d->unk131 = 1;
                    }
                } else {
                    d->unk142 += ABS(dmg);
                    if (d->unk13B != 0) {
                        dmg += dmg / 2;
                    }
                    if ((d->unkCA != 0 && d->unkC4 < d->unkC0->unk2C) || d->unk16A != 0 || o->unk16A != 0
                        || (d->unk12E != 0 && d->unk100 == 0 && d->unkF7 == 0)) {
                        d->unk13A = 1;
                        n = d->unkCA;
                        n = n > -1 ? n : -n;
                        if (o->unk16A != 0 || d->unk16A != 0 || d->unk12E != 0) {
                            n = dmg * 3 / 10;
                        } else {
                            win = d->unkC0->unk2C;
                            if (win == 0 || win < d->unkC4) {
                                win = d->unkC4;
                            }
                            n = n * 3 / 10 * d->unkC4 / win;
                        }
                        dmg = dmg * 12 / 10 + n;
                    }
                    if (d->unk135 != 0 || (d->unkC0->unk4 & 0x400)) {
                        switch (d->unk100) {
                        case -1:
                            d->unk12A = 1;
                            dmg = (s16)(dmg * 80 / 100);
                            d->unk100++;
                            break;
                        case 0:
                            if (func_003AE3C0(o) == 0) {
                                do {
                                    d->unk12A = 1;
                                } while (0); /* fake (decomp-permuter): gives the constant 1 its register (s4) */
                            }
                            dmg = (s16)(dmg * 80 / 100);
                            break;
                        case 1:
                        default:
                            dmg = (s16)(dmg / 2);
                            break;
                        }
                    }
                    if (o->unk16A == 0 && d->unk16A == 0 && o->unk400 != 0) {
                        dmg = dmg * 13 / 10;
                    }
                    d->unk3F4 -= dmg << 12;
                    if (d->unk12A == 1) {
                        red = (f32)d->unk3F8 - (f32)(dmg << 12) * 1.4f;
                        d->unk3F8 = red;
                        if (red < d->unk3F4) {
                            d->unk3F8 = d->unk3F4;
                        }
                    } else {
                        d->unk3F8 = (f32)d->unk3F8 - (f32)(dmg << 12) * 0.6f;
                    }
                    if (dmg >= base) {
                        base = dmg;
                    }
                    func_0035C0D8(base, d, 1 - i, hc);
                    d->unk131 = 1;
                    win = d->unk1E; /* fake (decomp-permuter): one index through a reused variable moves the window
                                     * length (win) from a1 to a0 */
                    D_00440940[d->unk1E][d->unk2].unk28 +=
                        func_003CF278(D_00440940[win][d->unk2].unk0, D_00440940[d->unk1E][d->unk2].unk4,
                                      D_00440940[d->unk1E][d->unk0].unk0, D_00440940[d->unk1E][d->unk0].unk4, 4, 1);
                    d->unk146 += dmg;
                    d->unk144 += ABS(dmg);
                }
            } else if (dmg == 0) {
                d->unk130 = 1;
                D_00440940[d->unk1E][d->unk2].unk28 +=
                    func_003CF278(D_00440940[d->unk1E][d->unk2].unk0, D_00440940[d->unk1E][d->unk2].unk4,
                                  D_00440940[d->unk1E][d->unk0].unk0, D_00440940[d->unk1E][d->unk0].unk4, 6, 1);
                func_0035BFF8(1, hc);
            }
        }
        if (D_005F5EF4 != 0) {
            d->unk3F4 = life;
        }
        if (D_0043B7F8[0] == 7) {
            d->unk3F4 = d->unk3F8 = d->unk3FC;
        }
        if (d->unk3F8 <= 0) {
            d->unk3F8 = 0;
            if (d->unk40C == 1) {
                d->unk40C = 0;
            }
        }
        if (d->unk3F4 <= 0) {
            d->unk3F4 = 0;
            if (func_003AE3C0(d) != 0 && d->unk40C == 1) {
                d->unk40C = 0;
            }
        }
        if (d->unk3F8 < d->unk3F4) {
            d->unk3F4 = d->unk3F8;
        }
        d = b;
        o = a;
    }

    d = a;
    o = b;
    for (i = 0; i < 2; i++) {
        if (d->unk40C == 0 && D_005F5EF4 == 0) {
            d->unk138 = 1;
            if (D_005F5EFC == 0) {
                func_00364678(o);
                D_005F5EFC = 1;
            }
        }
        d = b;
        o = a;
    }
}

void func_003729B0(Fighter *a, Fighter *b) {
    Fighter *first;
    Fighter *second;

    if (gBattleParity != 0) {
        first = a;
        second = b;
    } else {
        first = b;
        second = a;
    }
    Fighter_StartCommandMove(first, second);
    Fighter_StartCommandMove(second, first);
}

void func_00372A10(Fighter *a, Fighter *b) {
    Fighter *first;
    Fighter *second;

    if (gBattleParity != 0) {
        first = a;
        second = b;
    } else {
        first = b;
        second = a;
    }
    func_00368B90(first, second);
    func_00368B90(second, first);
    func_00368B08(first, second);
    func_00368B08(second, first);
}

void Battle_StepMoves(Fighter *a, Fighter *b) {
    Fighter *first;
    Fighter *second;
    s32 doneFirst;
    s32 doneSecond;

    if (gBattleParity != 0) {
        first = a;
        second = b;
    } else {
        first = b;
        second = a;
    }
    doneFirst = 0;
    if (first->unk188 != NULL) {
        doneFirst = first->unkC4 >= first->unk178;
    }
    doneSecond = 0;
    if (second->unk188 != NULL) {
        doneSecond = second->unkC4 >= second->unk178;
    }
    if (doneFirst && first->unk17A == 0x20) {
        func_003688B0(second, first);
        Fighter_StepMove(second, first);
        Fighter_StepMove(first, second);
    } else if (doneSecond && second->unk17A == 0x20) {
        func_003688B0(first, second);
        Fighter_StepMove(first, second);
        Fighter_StepMove(second, first);
    } else {
        Fighter_StepMove(first, second);
        Fighter_StepMove(second, first);
    }
}

void func_00372BA8(Fighter *a, Fighter *b) {
    func_00372BD0(a);
    func_00372BD0(b);
}

INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_00500620);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_00500640);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00372BD0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00372E98);

void func_00373038(Fighter *a, Fighter *b) {
    func_00373080(a, b);
    func_00373080(b, a);
    Battle_InitTeamLife();
    func_00375468();
}

void func_00373080(Fighter *f, Fighter *o) {
    f->unkB12C = NULL;
    func_00368398(f, o);
    func_003730E8(f, o, f->unk1E);
    func_0036C448(f);
    func_0036C480(f);
    Prac_RecReset(f->unk1E);
    func_00376C30();
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003730E8);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373218);

void func_00373320(void) {
    s32 i;

    D_005F5F20 = 0x14;
    for (i = 0; i < 4; i++) {
        func_00373398(&gFighters[i]);
    }
}

void func_00373380(Fighter *f) {
    D_005F5F20 = 0x14;
    func_00373398(f);
}

void func_00373390(Fighter *f, s32 n) {
    D_005F5F20 = n;
    func_00373398(f);
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373398);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373400);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373428);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003736B8);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003737B0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003738C0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373930);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373D48);

void func_00373DE0(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_00879630[i] = 0;
    }
}

void func_00373E18(Fighter *f) {
    D_00879630[(s16)f->unk20] = 5;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373E38);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500678);
Unk500678 *func_00374010(s32 i) {
    if (i < 8) {
        return &D_00500678[i];
    }
    return &D_005F2BA8[i];
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00374048);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00374148);

/* 0x374520-0x3745E8: the last functions of the object. Its .rodata ends with D_00500758, so the sound object starts
 * at Snd_Init with its .rodata at 0x500780 (the 16-byte alignment of its jump tables). */
extern s32 D_005F5ED4;
extern s32 D_005F5ED8;
extern s32 D_005F5EDC;
/* 20 values, read from the original executable. */
extern const s16 D_00500758[20];

void func_00374520(void) {
    D_005F5ED4 = 0;
    D_005F5ED8 = 0;
    D_005F5EDC = 0;
}

s32 func_00374530(void) {
    s32 v = D_005F5ED4;

    D_005F5ED4 = 0;
    return v;
}

s32 func_00374540(void) {
    return D_005F5EDC;
}

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_00500758);
void func_00374548(void) {
    u16 held = D_00415718[0] | D_00415718[1] | D_00415718[2] | D_00415718[3];
    u16 trig = D_00415728[0] | D_00415728[1] | D_00415728[2] | D_00415728[3];

    if ((held & 1) && (trig & 2)) {
        D_005F5ED4 = 1;
        D_005F5ED8++;
        if (D_005F5ED8 >= 20) {
            D_005F5ED8 = 0;
        }
        D_005F5EDC = D_00500758[D_005F5ED8];
    }
}
