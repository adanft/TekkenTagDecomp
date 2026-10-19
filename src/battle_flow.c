/* 0x3682F8-0x3745E8 (module battle_flow, docs/game_overview.md section 4; docs/systems/fighter.md section 5). The
 * fight itself: the per-frame update of the two fighters on screen (Battle_UpdateFrame, called by Mode_Battle), the
 * input fetch of each fighter (func_0036C680), the round flow and its texts ("ROUND %d", "K.O.", "TIME UP"), and
 * the end of the object (0x374520-0x3745E8). The start is provisional [I]; the end is
 * where the sound object starts (Snd_Init, .rodata 0x500780). No .rodata, .lit4 or .sdata of its own is emitted:
 * the tables, strings and jump tables it reads stay in the split data, and functions with jump tables or float
 * constants stay in assembly. */
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
s16 func_0036BE08(Fighter *f, s32 kind);
s32 func_003B6220(void);
s32 func_003D29E8(Fighter *f, s16 move);
u16 Math_Atan2(s16 x, s16 y);
u32 Math_Isqrt(u32 x);
void func_0034E8B0(Fighter *f);
void func_003532A0(void);
struct FighterUnkC0 *func_00368248(s32 id);
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
void func_003C1470(void);
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
void func_003720C8(Fighter *a, Fighter *b);
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
        if (((struct FighterMove *)f->unkC0)->unk18 < frame + 0x5A) {
            frame = ((struct FighterMove *)f->unkC0)->unk18 - 0x5A;
        }
    } else {
        frame = 1;
        if (gRoundNumber == 1 && ((struct FighterMove *)f->unkC0)->unk18 >= 0x97) {
            frame = ((struct FighterMove *)f->unkC0)->unk18 - 0x95;
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

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00369188);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003691D8);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003693E0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", Move_FindCommand);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00369E10);

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

INCLUDE_ASM("asm/nonmatchings/battle_flow", Fighter_StepMove);

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

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036BE08);

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

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036C278);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036C318);

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

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036C680);

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

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036D5D0);

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
            func_003720C8(f0, f1);
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

INCLUDE_ASM("asm/nonmatchings/battle_flow", Battle_UpdateRound);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EA80);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EC50);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EDE0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EEE0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036EF58);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F010);

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
        f->unk178 = ((struct FighterMove *)f->unkC0)->unk18;
    }
}

void func_0036F918(void) {
    func_00364698();
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F920);

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
    struct FighterUnkC0 *m = func_00368248(0x1019);
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

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003720C8);

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

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00372BD0);

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00372E98);

void func_00373038(Fighter *a, Fighter *b) {
    func_00373080(a, b);
    func_00373080(b, a);
    func_003C1470();
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
