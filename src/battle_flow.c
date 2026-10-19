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
 * entry. unk0 is a condition (Cmd_CheckCondition), unk2 the damage (bits 0-13) and its kind (bits 14-15), unk4 / unk6 the
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
extern TeamMember D_00440940[2][2];
extern u16 D_004EFD44[];

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
extern u32 D_005F5C28;
extern u16 D_005F5CBA;
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
s32 Fighter_GetLaunchKind(Fighter *f, Fighter *o);
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
s32 Cmd_CheckCondition(Fighter *f, Fighter *o, s16 id);
s32 func_00369E10(MoveCmd *c, Fighter *f, Fighter *o);
s32 func_0036CD10(u16 cmd, u16 held, u16 pressed);
s32 func_0036CD78(u16 a, u16 b);
s32 func_0036CAF8(Fighter *f, s32 dir, s32 n);
s32 func_0036CDD8(Fighter *f, s16 *seq, u16 held, u16 pressed);
void func_0036C448(Fighter *f);
void Fighter_ResetInputHistory(Fighter *f);
void func_0036C680(Fighter *f);

/* Applies a move record's damage changes (unk20, kind 0xC) to its D_005E8978 entries; the first entry gets the change
 * twice, as the original does [V]. */
void func_003682F8(struct FighterMove *m) {
    struct MoveMod *p;
    Unk5E8978 *e;
    u16 n;

    for (p = m->unk20; p->unk0 != 0; p++) {
        n = p->unk2 & 0xFF;
        if ((p->unk2 >> 8) == 0xC) {
            e = &D_005E8978[m->unk14];
            e->unk2 += n;
            while (e->unk0 != 0xFFFF) {
                e->unk2 += n;
                e++;
            }
        }
    }
}

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

    f->unkFC = Fighter_GetLaunchKind(f, o);
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

/* Whether a command entry may start now: within its frame window c->unkB..unkC while the move plays forwards, or up
 * to 5 frames before the window when a button (history bits 4-8) is pressed this frame [V code; I meaning]. */
s32 func_00368F78(Fighter *f, MoveCmd *c) {
    s32 r = 0;

    if ((f->unkC4 >= c->unkB - 5 && f->unkC4 < c->unkB && f->unkDA > 0
         && (f->unk438.unk4[f->unk438.unk0] & 0x1F0))
        || (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && f->unkDA > 0)) {
        r = 1;
    }
    return r;
}

#if 0
/* NON_MATCHING: 16 lines differ: the 0xED test's sltiu is cross-jumped into the shared == tail (the original keeps
 * it), the last test loads unk28 into the other register, and the branch-likely choices follow */
s16 func_003AD8E0(s16 x);

/* A command entry's condition (MoveCmd.unk2): 0 always; 1-0x21 the fighter's variant unk28 is id - 1; 0x22-0x42 it is
 * not id - 0x22; 0x43-0x63 the other fighter's unk28 is id - 0x43; 0x64-0x84 the partner's character class
 * (func_003AD8E0) is id - 0x64; 0x85-0xA6 the character unk2A is id - 0x85; 0xA7-0xC8 the partner's character is
 * id - 0xA7 (both partner tests fail while unk402 is set); 0xC9-0xEA the character is not id - 0xC9; 0xEB unk418;
 * 0xEC unk124; 0xED unk124 == 0; above, always [V code; I meaning]. */
s32 Cmd_CheckCondition(Fighter *f, Fighter *o, s16 id) {
    s32 c;

    if (id == 0) {
        return 1;
    }
    if (id >= 0xEB) {
        if (id == 0xEB) {
            return f->unk418;
        }
        if (id == 0xEC) {
            return f->unk124;
        }
        if (id == 0xED) {
            return f->unk124 == 0;
        }
        return 1;
    }
    if (id >= 0xC9) {
        return id - 0xC9 != f->unk2A;
    }
    if (id >= 0xA7) {
        c = D_00440940[f->unk1E][f->unk2].unk0;
        if (f->unk402 != 0) {
            return 0;
        }
        return id - 0xA7 == c;
    }
    if (id >= 0x85) {
        return id - 0x85 == f->unk2A;
    }
    if (id >= 0x64) {
        c = D_00440940[f->unk1E][f->unk2].unk0;
        if (f->unk402 != 0) {
            return 0;
        }
        return id - 0x64 == func_003AD8E0(c * 8);
    }
    if (id >= 0x43) {
        return id - 0x43 == o->unk28;
    }
    if (id >= 0x22) {
        return id - 0x22 != f->unk28;
    }
    return f->unk28 == id - 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", Cmd_CheckCondition);
#endif

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_004FF8D0);
#if 0
/* NON_MATCHING: 2 lines differ: the original returns the | 0x20 value without the final andi 0xFF and masks only the
 * other path; no type or form tried gives both */
/* A copy of the 9 bytes of D_004FF8D0 (a local array of the original, initialised from .rodata). */
typedef struct {
    u8 b[9];
} Dir9;
extern const Dir9 D_004FF8D0;

/* One input history entry (func_0036C918) as a code: the D_004FF8D0 byte of its direction 1-9, or'ed with 0x20 when a
 * button is pressed [V code; I meaning]. */
u8 func_00369188(u16 code) {
    Dir9 t = D_004FF8D0;
    u8 c = t.b[(code & 0xF) - 1];

    if (code & 0x1F0) {
        return c + 0x20;
    }
    return c;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00369188);
#endif

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003691D8);

INCLUDE_RODATA("asm/nonmatchings/battle_flow", jtbl_004FF8E0);
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003693E0);

extern MoveCmd D_0047F780[];
s32 Cmd_MatchInput(Fighter *f, u16 code, u16 dir, u16 held, u16 pressed);

/* The first entry of a command list that applies at `frame` (see docs/systems/fighter.md section 5.2): 0x8004 enters
 * the shared list D_0047F780[unk6], 0x8005 returns from it, 0x8000 ends the list (and is returned when nothing
 * matched) [V]. */
MoveCmd *Move_FindCommand(MoveCmd *c, s16 frame, Fighter *f, Fighter *o) {
    u16 dir = f->unk438.unk7C;
    u16 pressed = f->unk438.unk7E;
    u16 held = f->unk438.unk80;
    MoveCmd *ret;

    for (; c->unk0 != 0x8000; c++) {
        if (c->unk0 == 0x8004) {
            ret = c;
            c = &D_0047F780[ret->unk6];
        } else if (c->unk0 == 0x8005) {
            c = ret;
            continue;
        }
        if (frame >= c->unkB && frame <= c->unkC && Cmd_MatchInput(f, c->unk0, dir, held, pressed)
            && Cmd_CheckCondition(f, o, c->unk2) && (c->unk3 & ~0x80) >= 0x61 && func_00369E10(c, f, o)) {
            return c;
        }
    }
    return c;
}

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

s16 func_0036A2D0(Fighter *f, Fighter *o) {
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

/* The frames of a reaction (kind 0x24, Fighter_StepMove): unk162 from the attacker's move record (its byte 0x1B, else
 * the longer of the two lengths, 0-300), unk160 from the reaction entry's damage (func_0036C1A0) as 0x22-0x42 [I]. */
void func_0036A330(Fighter *f, Fighter *o) {
    struct FighterMove *m = o->unk188;
    s32 d;

    if (m->unk1B != 0) {
        f->unk162 = m->unk1B + f->unk188->unk18;
    } else {
        d = m->unk18 - f->unk188->unk18;
        if (d > 0) {
            f->unk162 = f->unk188->unk18 + d;
        } else {
            f->unk162 = m->unk18;
        }
        if (f->unk162 < 0) {
            f->unk162 = 0;
        }
        if (f->unk162 > 300) {
            f->unk162 = 300;
        }
    }
    f->unk160 = (func_0036C1A0(o, f, o->unk188)->unk2 << 5) / 45 + 0x22;
    if (f->unk160 > 0x41) {
        f->unk160 = 0x42;
    }
    if (f->unk160 < 0x23) {
        f->unk160 = 0x22;
    }
}

#if 0
/* NON_MATCHING: rewritten from the assembly in T14 b5 in the original's case order with its shared tails (the frame
 * step, the move start, the 0x2E kinds, the common tail); 394 of 1546 instruction lines still differ in fdiff: the
 * order of the stores inside the cases (orders searched per run of stores), the register numbering of the move record
 * / kind kept for the tail (t3 / t5 against t2 / t3) and a few blocks the original cross-jumps */
void func_0036A330(Fighter *f, Fighter *o);
void func_00370F28(Fighter *f, Fighter *o);
void func_003D2A38(Fighter *f, Fighter *o);
s32 func_0035BA00(s32 side);
void Fighter_TagSwap(s32 side);
void Snd_StopVoiceSeq(Fighter *f);
void Fighter_StartLaunch(Fighter *f, Fighter *o);
s32 func_0036BF00(Fighter *f);
s16 func_0036C278(s32 cur);
s16 func_0036C318(s16 cur, s16 k);
void func_0036C228(Fighter *f, Fighter *o, struct FighterMove *m);
void func_00368168(Fighter *f);
void func_0036BCD0(Fighter *f, s32 arg1);
void func_0036BDA0(Fighter *f);
void func_0036C128(Fighter *f);

/* The frame of a move that continues from the current one (func_0036BF00), else frame 1, clamped to its length. */
#define STEP_CONTINUE(f)                                                                                              \
    if (func_0036BF00(f) != 0) {                                                                                      \
        f->unkC4++;                                                                                                   \
        f->unkBC++;                                                                                                   \
    } else {                                                                                                          \
        f->unkBC = f->unkC4 = 1;                                                                                      \
    }                                                                                                                 \
    if (f->unk188->unk18 < f->unkC4) {                                                                                \
        f->unkBC = f->unkC4 = f->unk188->unk18;                                                                       \
    }

void Fighter_StepMove(Fighter *f, Fighter *o) {
    s32 start;
    s32 snap;
    MoveCmd *c;
    u16 id;
    u32 fl;
    u16 w;
    struct FighterMove *m;
    struct FighterUnk18C *p;
    Unk5E8978 *e;

    if (f->unk127 == 0) {
        return;
    }
    f->unk170--;
    if (f->unk170 < 0) {
        f->unk170 = 0;
    }
    start = 1;
    if (f->unk188 != NULL) {
        if (f->unkDA > 0) {
            start = 0;
            if (f->unkC4 >= f->unk178 || f->unkC4 >= f->unkC0->unk18) {
                start = 1;
            }
        } else if (f->unkDA < 0) {
            start = 0;
            if (f->unkC4 <= f->unk178 || f->unkC4 <= 0) {
                start = 1;
            }
        }
    } else {
        start = 0;
    }
    func_0036BCD0(f, start);
    if (start == 0) {
        f->unkC8++;
        f->unk134 = 0;
        if (f->unkE4 > 0 && f->unkFE != 0) {
            f->unkE4 = 0;
        }
        if (f->unkE4 < 0 || (f->unkE4 > 0 && f->unkFE == 0)) {
            if (f->unkE4 < 0) {
                f->unkE4++;
            } else {
                f->unkE4--;
            }
            if (f->unkE4 != 0) {
                return;
            }
            f->unkBC = f->unkC4 = f->unkC4 + 1;
            f->unkB8 = f->unkC0;
            return;
        }
        {
            if (f->unkD6 >= 0) {
                if (f->unkC4 == f->unkC0->unk1B) {
                    if (f->unkD6 > 0) {
                        f->unkD6--;
                        f->unkBC++;
                    } else if (f->unkD6 == 0) {
                        f->unkB8 = f->unkC0;
                        f->unkBC = f->unkC4 = f->unkC4 + 1;
                    }
                } else {
                    f->unkC4++;
                    f->unkBC++;
                }
            } else if (f->unkFE != 0) {
                if (f->unkC4 != f->unkC0->unk1B) {
                    f->unkBC = f->unkC4 = f->unkC4 + 1;
                }
            } else {
                f->unkC4 += f->unkDA;
                if (f->unkC4 <= 0) {
                    f->unkC4 = 1;
                }
                f->unkBC = f->unkC4;
            }
            return;
        }
    }

    if (f->unk180 != 0) {
        if (D_005F5EF4 != 0) {
            c = (MoveCmd *)f->unkC0->unkC;
            while (c->unk0 != 0x8000) {
                c++;
            }
            id = c->unk6;
            f->unk184 = id;
            f->unk188 = func_00368218(f, id);
            f->unkD8 = c->unkC - f->unkC4;
            f->unk17A = func_0036BE08(f, c->unk8 & 0x7F);
            f->unk17C = c->unk8 & 0x8000;
            f->unk17E = c->unk8 & 0x4000;
            f->unk180 = 0;
            f->unk182 = 0;
            if (f->unkDA < 0) {
                f->unk178 = c->unkC;
            } else {
                f->unk178 = c->unkD;
            }
            if (f->unk178 == 0xFF) {
                f->unk178 = f->unkC0->unk18;
            }
            f->unkF8 = 2;
            f->unk12D = 1;
        } else {
            if (f->unk182 != 0) {
                if (f->unk418 == 0 && f->unk2A == 0x21) {
                    f->unk28 = func_0036C278(f->unk28);
                } else {
                    f->unk28 = func_0036C318(f->unk28, f->unk182);
                }
                func_00368168(f);
            } else {
                Fighter_TagSwap(f->unk1E);
                f->unkB134 = 1;
                Battle_UpdateFacing(f, o);
            }
            f->unk188 = func_00368218(f, f->unk184);
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
    f->unkE4 = 0;
    f->unkD6 = -1;
    f->unk120 = 0;
    f->unkF0 = 0;
    f->unk122 = 0;
    f->unk11F = 0;
    f->unk110 = o->unk51C;
    f->unk114 = o->unk524;
    f->unk11A = 0;
    f->unk46 = 0;
    f->unkF1 = 0;
    f->unk11D = 0;
    f->unk14E = 0;
    f->unk124 = 0;
    f->unkF2 = 0;
    f->unk438.unk82 = 0;
    f->unkDA = 1;
    f->unk11E = 0;
    if (f->unk148 != 0) {
        f->unkCA = f->unk148;
    } else {
        func_0036C228(f, o, f->unk188);
    }
    m = f->unk188;
    fl = m->unk4;
    w = m->unk8;
    f->unkCC = fl;
    f->unkD5 = (fl >> 11) & 0x1F;
    f->unkD4 = w >> 8;
    f->unkD2 = f->unkCC & 0x18;
    f->unkD0 = w;
    if (f->unk12C == 0 && (fl & 0x100)) {
        f->unkD2 = 0;
        f->unkCC = fl & 0xFFFFFFE7;
    }
    if (f->unk130 != 0) {
        f->unkF6 = 1;
    } else {
        f->unkF6 = 0;
    }
    if (f->unk12F == 0) {
        f->unkF3 = 0;
        f->unkFE = 0;
        f->unk100 = 0;
    }
    if (f->unk132 == 0) {
        f->unkF4 = 0;
    }
    f->unkF5 = 0;
    if (f->unk4C0.unk11 != 0) {
        f->unkF9 = f->unk131 != 0 || f->unkF7 != 0;
    } else {
        f->unkF9 = f->unkF7 = f->unk131;
    }
    if (f->unk12F != 0 && f->unk40C != 0) {
        f->unkE4 = f->unk14C;
    }
    if (f->unk4C0.unk11 != 0 && f->unkE0 != 0) {
        f->unkE2 = f->unkE0;
    } else if (f->unk4C0.unk11 == 0 || f->unkE2 == 0) {
        f->unkE2 = 0;
    }
    if (f->unkF8 == 3 || f->unkF8 == 1 || m->unk0 != f->unkC0->unk0) {
        f->unk134 = 1;
    }
    snap = f->unkC0->unk24 >> 19;
    snap &= 1;
    if (f->unkC0->unk24 & 0x200) {
        f->unk16A = 0x78;
    }
    if (m->unk24 & 8) {
        f->unk164 = 9;
    }
    if (f->unk17C != 0) {
        f->unk17C = 0;
        f->unk3C -= 0x8000;
        f->unk1A -= 0x8000;
    }
    if (f->unk12D != 0 && f->unk17A != 0x24) {
        f->unk1A = f->unk3C = f->unk3C + f->unkC0->unk12;
    }
    f->unk12D = 0;
    func_00370F28(f, o);
    if (f->unk188->unk4 & 0x1000000) {
        f->unk129 = func_0035BA00(f->unk1E) + 1;
    } else {
        f->unk129 = 0;
    }

    switch (f->unk17A) {
    case 0:
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unkB8 = f->unk188;
        f->unk4 = f->unk51C;
        f->unk3C = f->unk3A;
        f->unkC = f->unk524;
        f->unk1A = f->unk3A;
        f->unkE0 = 0;
        break;
    case 1:
        f->unk1A = f->unk3C;
    case 2:
    case 3:
        STEP_CONTINUE(f);
        f->unk11E = 0xB;
        f->unk4 = f->unk51C;
        f->unkC = f->unk524;
        f->unk126 = 1;
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        break;
    case 4:
        f->unkC4 = 1;
        f->unkBC++;
        if (f->unkB8->unk18 < f->unkBC) {
            f->unkBC = f->unkB8->unk18;
        }
        f->unkD6 = f->unkD8;
        f->unk11E = 0xB;
        f->unk1A = f->unk3C;
        if ((s16)f->unk40 < 0x4FA5) {
            f->unk15C = f->unk188->unk2C - 1;
            if (f->unk15C <= 0) {
                f->unk15C = 1;
            }
            f->unk15E = (s16)(f->unk3A - f->unk1A) / f->unk15C;
        }
        f->unkE0 = 0;
        break;
    case 5:
        f->unkC4 = 1;
        f->unkBC++;
        if (f->unkB8->unk18 < f->unkBC) {
            f->unkBC = f->unkB8->unk18;
        }
        f->unkD6 = f->unkD8;
        f->unk1A = f->unk3C;
        if ((s16)f->unk40 < 0x4FA4) {
            f->unk11E = 4;
        } else {
            f->unk11E = 6;
        }
        f->unk15C = 0;
        f->unk15E = 0;
        f->unkE0 = 0;
        break;
    case 0x11:
        break;
    case 6:
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unkB8 = f->unk188;
        f->unk1A = f->unk3C;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        f->unkE0 = 0;
        break;
    case 7:
        f->unkBC = f->unkC4 = f->unk188->unk18;
        f->unk1A = f->unk3C;
        f->unkB8 = f->unk188;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        f->unkE0 = 0;
        f->unkDA = -1;
        break;
    case 8:
        f->unkC4--;
        if (f->unkC4 <= 0) {
            f->unkC4 = 1;
        }
        f->unkBC = f->unkC4;
        f->unk1A = f->unk3C;
        f->unkDA = -1;
        f->unk4 = f->unk51C;
        f->unkC = f->unk524;
        f->unk126 = 1;
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        break;
    case 9:
        f->unkBC = 1;
        f->unkB8 = f->unk188;
        f->unkC4 = 1;
        f->unk1A = f->unk44;
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk15E = 0;
        f->unk11E = 5;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0xA:
        f->unk1A = f->unk3C;
        goto turn;
    case 0xB:
        f->unk1A = f->unk44;
    turn:
        m = f->unk188;
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unkB8 = m;
        if (m->unk2C != 0) {
            f->unk15C = m->unk2C - 1;
        } else if (f->unkC0->unk18 >= 0x10) {
            f->unk15C = 0x10;
        } else {
            f->unk15C = f->unkC0->unk18;
        }
        if (f->unk15C <= 0) {
            f->unk15C = 1;
        }
        if (f->unk40 < 0x4000) {
            f->unk44 = f->unk3A;
        } else {
            f->unk44 = f->unk3A - 0x8000;
        }
        f->unk15E = (s16)(f->unk44 - f->unk3C) / f->unk15C;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        f->unkE0 = 0;
        f->unk11E = 0xB;
        break;
    case 0xC:
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unkB8 = f->unk188;
        if ((s16)f->unk40 < 0x4FA4) {
            f->unk11E = 4;
            f->unk1A = f->unk3A;
        } else {
            f->unk11E = 6;
            f->unk1A = f->unk44;
        }
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0xD:
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unk1A = f->unk44;
        f->unkB8 = f->unk188;
        if ((s16)f->unk40 < 0x4FA4) {
            f->unk11E = 3;
            f->unk1A = f->unk3A;
        } else {
            f->unk11E = 6;
        }
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0xE:
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unk1A = f->unk44;
        f->unkB8 = f->unk188;
        if ((s16)f->unk40 < 0x4FA4) {
            f->unk11E = 2;
            f->unk1A = f->unk3A;
        } else {
            f->unk11E = 6;
        }
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0xF:
        f->unkBC = 1;
        f->unkB8 = f->unk188;
        f->unkC4 = 1;
        f->unk15E = 0;
        f->unk1A = f->unk44;
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk11E = 1;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x10:
        m = f->unk188;
        f->unk11E = 0xB;
        f->unkC4 = 1;
        f->unkBC = 1;
        f->unkB8 = m;
        f->unk1A = f->unk3A;
        f->unk15C = m->unk2C - 1;
        if (f->unk15C <= 0) {
            f->unk15C = 1;
        }
        f->unk15E = (s16)(f->unk3A - f->unk3C) / f->unk15C;
        f->unkF0 = 1;
        f->unkE0 = 0;
        f->unkE8 = 0;
        f->unkEC = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x12:
        STEP_CONTINUE(f);
        f->unkF0 = 1;
        f->unk11E = 0xB;
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        f->unkE8 = 0;
        f->unkEC = 0;
        f->unk1A = f->unk3A;
        f->unk15C = f->unk188->unk2C - 1;
        if (f->unk15C <= 0) {
            f->unk15C = 1;
        }
        f->unk15E = (s16)(f->unk3A - f->unk3C) / f->unk15C;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x13:
        STEP_CONTINUE(f);
        f->unkF0 = 1;
        f->unk11E = 0xB;
        f->unk3C -= 0x8000;
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        f->unkE8 = 0;
        f->unkEC = 0;
        f->unk1A = f->unk3A;
        f->unk15C = f->unk188->unk2C - 1;
        if (f->unk15C <= 0) {
            f->unk15C = 1;
        }
        f->unk15E = (s16)(f->unk3A - f->unk3C) / f->unk15C;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x14:
        STEP_CONTINUE(f);
        f->unkB8 = f->unk188;
        if ((s16)f->unk40 < 0x4FA4) {
            f->unk11E = 4;
        } else {
            f->unk11E = 6;
        }
        f->unk4 = f->unk51C;
        f->unkC = f->unk524;
        f->unk126 = 1;
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk15E = 0;
        break;
    case 0x15:
        STEP_CONTINUE(f);
        f->unkB8 = f->unk188;
        if ((s16)f->unk40 < 0x4FA4) {
            f->unk11E = 2;
        } else {
            f->unk11E = 6;
        }
        f->unk4 = f->unk51C;
        f->unkC = f->unk524;
        f->unk126 = 1;
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk15E = 0;
        break;
    case 0x16:
        STEP_CONTINUE(f);
        f->unk11E = 8;
        f->unk4 = f->unk51C;
        f->unkC = f->unk524;
        f->unk126 = 1;
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk15E = 0;
        break;
    case 0x17:
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unkB8 = f->unk188;
        f->unk1A = f->unk3C;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        f->unkE0 = 0;
        f->unk11E = 9;
        break;
    case 0x18:
        f->unkC4 = 1;
        f->unkE0 = 0;
        f->unkB8 = f->unk188;
        f->unk1A = f->unk44;
        f->unkBC = 1;
        f->unk11E = 8;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x19:
        f->unkE0 = 0;
        STEP_CONTINUE(f);
        f->unk11E = 8;
        f->unk4 = f->unk51C;
        f->unkC = f->unk524;
        f->unk126 = 1;
        f->unkB8 = f->unk188;
        break;
    case 0x1A:
    case 0x1B:
        f->unkC4++;
        f->unkBC++;
        if (f->unk188->unk18 < f->unkC4) {
            f->unkBC = f->unkC4 = f->unk188->unk18;
        }
        f->unkB8 = f->unk188;
        f->unk1A = f->unk3C;
        if (f->unk17A == 0x1A) {
            f->unk11A = 1;
        } else {
            f->unk11A = 2;
        }
        break;
    case 0x1C:
        f->unkC4 = 1;
        f->unkE0 = 0;
        f->unkBC = 1;
        f->unk1A = f->unk3C;
        f->unk15C = 0;
        f->unkB8 = f->unk188;
        f->unk15E = 0;
        f->unk11E = 0xA;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        f->unk170 = 0x18;
        break;
    case 0x1D:
        STEP_CONTINUE(f);
        f->unk11E = 0xA;
        f->unk1A = f->unk3C;
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x1E:
        f->unkFA = 1;
        f->unk8 = f->unk520;
        f->unkFC = 4;
        f->unk184 = func_00374010(4)->unk0;
        Fighter_StartLaunch(f, o);
        f->unk100++;
        f->unk11D = 1;
        f->unkFE = 1;
        f->unkE0 = 0;
        f->unkBC = f->unk178;
        f->unkB8 = f->unk188;
        f->unkC4 = f->unk178;
        f->unk160 = func_0036A2D0(f, o);
        f->unk11E = 0xB;
        if (f->unkC0->unk24 & 0x10) {
            f->unk1A = f->unk44 + f->unk18C->unk0;
        } else {
            f->unk1A = f->unk44;
        }
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x1F:
    case 0x20:
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unkB8 = f->unk188;
        if (f->unkE0 == 0) {
            f->unk3C = f->unk3A - 0x8000;
            switch (o->unk42) {
            case 0:
                break;
            case 1:
                f->unk3C += 0x4000;
                break;
            case 2:
                f->unk3C -= 0x8000;
                break;
            case 3:
                f->unk3C -= 0x4000;
                break;
            }
            f->unk1A = f->unk3C;
            f->unk15C = 0x10;
            f->unk15E = (s16)(o->unk3C - f->unk3C) / f->unk15C;
            f->unk4 = o->unk51C;
            f->unkC = o->unk524;
        } else if (f->unkE0 < 0) {
            f->unk1A = f->unk3C = o->unk1A;
            f->unk4 = o->unk51C;
            f->unkC = o->unk524;
        } else if (f->unk188->unk24 & 0x1000) {
            f->unk3C += f->unkC0->unk12;
            f->unk1A = f->unk3C = o->unk1A;
            f->unk4 = o->unk51C;
            f->unkC = o->unk524;
        }
        f->unk150 = 0;
        f->unk152 = 0;
        f->unk154 = 0;
        if (f->unkE0 < 0) {
            f->unkE0 = 1;
        } else {
            f->unkE0++;
        }
        break;
    case 0x24:
        if (f->unkE0 >= 0) {
            e = func_0036C1A0(f, o, f->unkC0);
            if ((e->unk2 != 0 && f->unkC4 < f->unkC0->unk2C) || o->unk16A != 0) {
                f->unk120 = 1;
            }
        }
        f->unkF2 = f->unkC4 = f->unkBC = 1;
        f->unkB8 = f->unk188;
        if (f->unkE0 >= 0) {
            f->unk1A = f->unk3C = f->unk3C - 0x8000;
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
            Snd_StopVoiceSeq(f);
        } else if (f->unk188->unk24 & 0x1000) {
            f->unk1A = f->unk3C = f->unk3C + f->unkC0->unk12;
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        if (f->unkE0 > 0) {
            f->unkE0 = -1;
        } else {
            f->unkE0--;
        }
        func_0036A330(f, o);
        f->unk150 = 0;
        f->unk152 = 0;
        f->unk154 = 0;
        break;
    case 0x25:
        p = f->unk18C;
        f->unk158 = p->unk18;
        f->unk150 = *f->unk158++;
        f->unk152 = *f->unk158++;
        f->unk154 = 8;
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unk156 = f->unk3A + (s16)(p->unk0 + 0x8000);
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        f->unk11E = 7;
        f->unk1A = f->unk44;
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x26:
        f->unk8 = f->unk520;
        Fighter_StartLaunch(f, o);
        f->unk100++;
        f->unkE0 = 0;
        f->unkBC = f->unk178;
        f->unkB8 = f->unk188;
        f->unk11D = 1;
        f->unkFE = 1;
        f->unkC4 = f->unk178;
        f->unk160 = func_0036A2D0(f, o);
        f->unk11E = 0xB;
        if (f->unkC0->unk24 & 0x10) {
            f->unk1A = f->unk44 + f->unk18C->unk0;
        } else {
            f->unk1A = f->unk44;
        }
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x27:
        f->unk160 = func_0036A2D0(f, o);
        p = f->unk18C;
        f->unk158 = p->unk14;
        f->unk150 = *f->unk158++;
        f->unk152 = *f->unk158++;
        f->unk154 = 8;
        f->unkBC = 1;
        f->unk156 = f->unk3A + (s16)(p->unk10 + 0x8000);
        f->unk160 = 0;
        f->unkC4 = 1;
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        f->unk11E = 0xB;
        f->unk1A = f->unk3C;
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x28:
        f->unk160 = func_0036A2D0(f, o);
        p = f->unk18C;
        f->unk158 = p->unkC;
        f->unk150 = *f->unk158++;
        f->unk152 = *f->unk158++;
        f->unk154 = 8;
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unk156 = f->unk3A + (s16)(p->unk0 + 0x8000);
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        f->unk11E = 7;
        f->unk1A = f->unk44;
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
        f->unk160 = func_0036A2D0(f, o);
        p = f->unk18C;
        f->unk158 = p->unk4;
        f->unk150 = *f->unk158++;
        f->unk152 = *f->unk158++;
        f->unk154 = 8;
        if (f->unk17A == 0x2C) {
            f->unk156 = f->unk44 + (s16)(p->unk0 + 0x8000);
        } else {
            f->unk156 = f->unk3A + (s16)(p->unk0 + 0x8000);
        }
        func_0036C128(f);
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unkB8 = f->unk188;
        f->unkE0 = 0;
        f->unk11E = 0xB;
        f->unk1A = f->unk3C;
        f->unk15C = 0;
        f->unk15E = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x21:
        f->unkBC = 1;
        f->unkB8 = f->unk188;
        f->unkC4 = 1;
        f->unk3C = f->unk3A;
        f->unk1A = f->unk3A;
        f->unkC = f->unk524;
        f->unk4 = f->unk51C;
        f->unkE0 = 0;
        f->unk11E = 8;
        break;
    case 0x22:
        f->unkC4 = 1;
        f->unkE0 = 0;
        f->unkB8 = f->unk188;
        f->unk1A = f->unk44;
        f->unkBC = 1;
        f->unk11E = 0xC;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    case 0x23:
        f->unkB8 = f->unk188;
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unk1A = f->unk44;
        f->unk11E = 0xD;
        f->unkE0 = 0;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    default:
        f->unkBC = 1;
        f->unkC4 = 1;
        f->unk1A = f->unk3A;
        f->unk3C = f->unk3A;
        f->unkB8 = f->unk188;
        if (snap) {
            f->unk4 = f->unk51C;
            f->unkC = f->unk524;
        }
        break;
    }
    f->unk176 = f->unk10C;
    f->unk10C = f->unk184;
    f->unk184 = -1;
    f->unkC0 = f->unk188;
    f->unk188 = NULL;
    if (f->unkC0->unk2C == 0) {
        f->unk118 = 0;
    } else if (f->unkC0->unk2A != 0) {
        f->unk118 = 2;
    } else if (f->unkC0->unk28 != 0) {
        f->unk118 = 1;
    } else {
        f->unk118 = 0;
    }
    if (f->unk134 != 0) {
        f->unkC8 = 1;
    } else {
        f->unkC8 = f->unkC4;
    }
    f->unk10E = f->unk17A;
    f->unk17E = 0;
    f->unk17A = 0;
    f->unk17C = 0;
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

/* The hit reaction direction unk172 of the attacker f against o, while f's hit lands (unk132) and o is neither juggled
 * nor protected: by o's facing quadrant unk42 (0, 1, 3, 2) and o's guard mask, 0xE-0x11 (mask 0x80 and 0x200), 0xA-0xD
 * (0x80), 2-5 (0x40), 6-9 (0x20) when f's body angle unk40 is below 0x2AAA, 0x12 / 0x13 (0x02) when it is 0x5556 or
 * more; else -1 [V code; I meaning]. */
void func_0036BFA0(Fighter *f, Fighter *o) {
    s16 r = -1;

    if (f->unk132 != 0 && o->unk135 == 0 && o->unk139 == 0) {
        if ((s16)f->unk40 < 0x2AAA) {
            if (o->unkCC & 0x80) {
                if (o->unkCC & 0x200) {
                    if (o->unk42 == 0) {
                        r = 0xE;
                    } else if (o->unk42 == 1) {
                        r = 0x10;
                    } else if (o->unk42 == 3) {
                        r = 0x11;
                    } else if (o->unk42 == 2) {
                        r = 0xF;
                    }
                } else {
                    if (o->unk42 == 0) {
                        r = 0xA;
                    } else if (o->unk42 == 1) {
                        r = 0xC;
                    } else if (o->unk42 == 3) {
                        r = 0xD;
                    } else if (o->unk42 == 2) {
                        r = 0xB;
                    }
                }
            } else if (o->unkCC & 0x40) {
                if (o->unk42 == 0) {
                    r = 2;
                } else if (o->unk42 == 1) {
                    r = 4;
                } else if (o->unk42 == 3) {
                    r = 5;
                } else if (o->unk42 == 2) {
                    r = 3;
                }
            } else if (o->unkCC & 0x20) {
                if (o->unk42 == 0) {
                    r = 6;
                } else if (o->unk42 == 1) {
                    r = 8;
                } else if (o->unk42 == 3) {
                    r = 9;
                } else if (o->unk42 == 2) {
                    r = 7;
                }
            }
        } else if ((s16)f->unk40 >= 0x5556) {
            if (o->unkCC & 2) {
                if (o->unk42 == 0) {
                    r = 0x12;
                } else if (o->unk42 == 2) {
                    r = 0x13;
                }
            }
        }
    }
    f->unk172 = r;
}

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
        if (Cmd_CheckCondition(f, o, (s16)e->unk0) != 0) {
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

typedef struct {
    /* 0x0 */ s32 unk0;                       /* 0: up, 1: down */
    /* 0x4 */ s32 unk4;                       /* frames left */
} Unk42D2C0;
extern Unk42D2C0 D_0042D2C0[];

/* The pad's sidestep shortcuts [I]: a press of pad bit 0x02 (0x08) adds up 0x1000 (down 0x4000) to this frame's
 * presses and, on its first frame, to the held buttons, and keeps the shortcut for 0x23 frames; while it runs and the
 * same bit is held with none of the buttons 0xF4, the held and pressed buttons are cut to the directions [V code]. */
void func_0036C578(s32 i, u16 *held, u16 *pressed) {
    Unk42D2C0 *e = &D_0042D2C0[i];
    u16 h = D_005F5C40[i];
    u16 p = D_005F5C48[i];
    u16 addh = 0;
    u16 addp = 0;

    if (p & 2) {
        e->unk0 = 0;
        e->unk4 = 0x23;
        addp = 0x1000;
    } else if (p & 8) {
        e->unk0 = 1;
        addp = 0x4000;
        e->unk4 = 0x23;
    }
    if (e->unk4 > 0) {
        e->unk4--;
        if (e->unk4 >= 0x22) {
            addh = 0x4000;
            if (e->unk0 == 0) {
                addh = 0x1000;
            }
        }
        if ((e->unk0 == 0 && (h & 2)) || (e->unk0 == 1 && (h & 8))) {
            if (!(*held & 0xF4)) {
                *held &= 0xFFF;
                *pressed &= 0xFFF;
            }
        }
    }
    *held |= addh;
    *pressed |= addp;
}

s32 func_0035B9E8(s32 side);
void func_0036C578(s32 i, u16 *held, u16 *pressed);
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

        func_0036C578(f->unk428, (u16 *)&D_005F5C40[f->unk428], (u16 *)&D_005F5C48[f->unk428]);
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

s32 func_0036CFC0(u16 held);

/* This frame's input as bits: the direction bit 1 << (4 + direction) at unk7C, and the buttons held (unk80) and pressed
 * (unk7E) as 1 (0x80), 2 (0x10), 4 (0x40), 8 (0x20) and 0x4000 (0x04, tag) [V code; I names]. */
void func_0036C988(Fighter *f, u16 held, u16 pressed) {
    u16 dir;
    u16 h;
    u16 p;

    dir = 1 << ((func_0036CFC0(held) & 0xF) + 4);
    if (held & 0x80) {
        h = 1;
    } else {
        h = 0;
    }
    if (held & 0x10) {
        h |= 2;
    }
    if (held & 0x40) {
        h |= 4;
    }
    if (held & 0x20) {
        h |= 8;
    }
    if (held & 4) {
        h |= 0x4000;
    }
    if (pressed & 0x80) {
        p = 1;
    } else {
        p = 0;
    }
    if (pressed & 0x10) {
        p |= 2;
    }
    if (pressed & 0x40) {
        p |= 4;
    }
    if (pressed & 0x20) {
        p |= 8;
    }
    if (pressed & 4) {
        p |= 0x4000;
    }
    f->unk438.unk80 = h;
    f->unk438.unk7E = p;
    f->unk438.unk7C = dir;
}

/* Latches the buttons 0x80 / 0x10 pressed this frame (unk83 / unk84) and the pair 0x90 (unk85) while unk82 is set
 * [V code; I meaning]. */
void func_0036CA68(Fighter *f, u16 held, u16 pressed) {
    u16 b10;
    u16 b80;

    if (f->unk438.unk82 == 0 || (f->unk438.unk83 == 0 && f->unk438.unk84 == 0 && f->unk438.unk85 == 0)) {
        b80 = pressed & 0x80;
        b10 = pressed & 0x10;
        if (((held & 0x90) ^ 0x90) == 0 && (b80 != 0 || b10 != 0)) {
            f->unk438.unk85 = 1;
        } else if (b80 != 0) {
            f->unk438.unk83 = 1;
        } else if (b10 != 0) {
            f->unk438.unk84 = 1;
        } else {
            f->unk438.unk85 = 0;
            f->unk438.unk83 = 0;
            f->unk438.unk84 = 0;
        }
    }
}

#if 0
/* NON_MATCHING: 22 lines differ: the original enters the neutral loop through a peeled first step that reuses the 5
 * already in a register, and reloads 5 inside the loop */
/* A double tap: `dir` in this frame's history entry, then neutral (5), then `dir` again, the taps within `n` entries
 * (0x8001 forward 6, 0x8002 back 4) [V code; I meaning]. */
s32 func_0036CAF8(Fighter *f, s32 dir, s32 n) {
    s32 i = f->unk438.unk0;
    s16 *hist = f->unk438.unk4;
    s32 r = 0;
    s32 x;

    if (dir == (hist[i] & 0xF)) {
        i = i < 1 ? 0x3B : i - 1;
        if ((hist[i] & 0xF) == 5) {
            n--;
            while (n > 0) {
                i = i < 1 ? 0x3B : i - 1;
                x = hist[i] & 0xF;
                if (x != 5) {
                    if (dir != x) {
                        goto end;
                    }
                    while (n > 0) {
                        i = i < 1 ? 0x3B : i - 1;
                        if (dir != (hist[i] & 0xF)) {
                            break;
                        }
                        n--;
                    }
                    break;
                }
                n--;
            }
            r = n > 0;
        }
    }
end:
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036CAF8);
#endif

/* Whether the input code of a command entry is entered (Move_FindCommand): below 0x8000 a direction / button test of
 * this frame (func_0036CD10; bits 0x3FE0 also need one of this frame's direction bits); 0x8001 / 0x8002 a double tap
 * forward / back within 20 entries (func_0036CAF8); 0x8003 the flag unk12B, or for character 0x21 the button latch
 * unk140 while the round runs (both clear unk12B); above 0x8005 a sequence of D_004EFD44 (func_0036CDD8) [V code; I
 * meaning]. */
s32 Cmd_MatchInput(Fighter *f, u16 code, u16 dir, u16 held, u16 pressed) {
    if ((s16)code >= 0) {
        if ((code & 0x3FE0) && !(code & dir)) {
            return 0;
        }
        return func_0036CD10(code, held, pressed);
    }
    if (code <= 0x8005) {
        switch (code) {
        case 0x8000:
            return 0;
        case 0x8001:
            return func_0036CAF8(f, 6, 0x14);
        case 0x8002:
            return func_0036CAF8(f, 4, 0x14);
        case 0x8003:
            if (f->unk12B || (f->unk2A == 0x21 && D_005F5EF4 == 0 && f->unk140)) {
                f->unk12B = 0;
                return 1;
            }
            return 0;
        }
        return 0;
    }
    return func_0036CDD8(f, (s16 *)&D_004EFD44[code], held, pressed);
}

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

/* Whether a command sequence (`seq`: a frame count, then 16-bit steps ended by 0) was entered: its last step against
 * this frame (direction and buttons, func_0036CD10), then each earlier step against the input history, walking back at
 * most seq[0] entries; a history entry with a button that does not match ends the search [V code; I meaning]. */
s32 func_0036CDD8(Fighter *f, s16 *seq, u16 held, u16 pressed) {
    s32 r = 0;
    s16 *hist = f->unk438.unk4;
    s16 *p;
    s32 n;
    s32 i;
    u16 cur;
    u16 e;

    i = f->unk438.unk0;
    n = seq[0];
    for (p = seq + 1; *p != 0; p++) {
    }
    p--;
    cur = *p;
    if ((cur & 0xF) == cur) {
        i = (u32)i > 0x3A ? 0 : i + 1;
    } else {
        e = hist[i];
        if ((cur & 0xF) != (e & 0xF)) {
            goto end;
        }
        if (!func_0036CD10(cur >> 8, held, pressed)) {
            goto end;
        }
        if (--p == seq) {
            r = 1;
            goto end;
        }
        cur = *p;
    }
    while (n > 0) {
        i = i < 1 ? 0x3B : i - 1;
        e = hist[i];
        if (func_0036CD78(cur, e)) {
            if (--p == seq) {
                r = 1;
                break;
            }
            cur = *p;
        } else if (e >> 4) {
            break;
        }
        n--;
    }
end:
    return r;
}

/* 8-aligned after the jump table of func_0036C680 (0x4FFD34). */
RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_004FFD38);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_004FFFD4);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_004FFFE0);
extern const u16 D_004FFFD4[];
extern const u16 D_004FFFE0[];

/* The input sequence of a command code: codes above 0x8005 index D_004EFD44, 0x8001 / 0x8002 are the two double taps
 * (D_004FFFD4, D_004FFFE0), others have none [V code; I meaning]. */
u16 *func_0036CF30(u16 code) {
    if (code <= 0x8005) {
        switch (code) {
        case 0x8001:
            return (u16 *)D_004FFFD4;
        case 0x8002:
            return (u16 *)D_004FFFE0;
        }
        return NULL;
    }
    return &D_004EFD44[code];
}

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

extern const u16 D_004FFD38[];

/* The direction 1-9 (numpad) of the pad bits 0xF000, looked up in D_004FFD38; 5 (neutral) when none [V code]. */
s32 func_0036CFC0(u16 held) {
    const u16 *p;
    s32 i;
    s32 dir;

    held &= 0xF000;
    for (i = 0, dir = 5, p = D_004FFD38; i < 9; i++, p++) {
        if (held == *p) {
            dir = i + 1;
            break;
        }
    }
    return dir;
}

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
extern u16 D_005F5C72;
extern const char D_00500470[];
extern const char D_00500488[];
extern const char D_005004A0[];

/* The round start texts by the round timer D_005F5EE8 (a longer window in round 1): "ROUND %d" (D_00500488), or
 * D_00500470 when D_005F5C72 is 1 and neither fighter has unk402, then "READY!" (D_005004A0) [V calls; I texts]. */
void func_0036EDE0(void) {
    if ((gRoundNumber == 1 ? D_005F5EE8 < 0x69 : D_005F5EE8 < 0x2D)
        && (gRoundNumber == 1 ? D_005F5EE8 >= 0x42 : D_005F5EE8 >= 6)) {
        if (D_005F5C72 == 1 && gFighters[0].unk402 == 0 && gFighters[1].unk402 == 0) {
            Text_Printf(D_00500470, 0x74, 0xDC, 0xA, 2);
        } else {
            Text_Printf(D_00500488, 0xA5, 0xDC, 0xA, 2, gRoundNumber);
        }
    } else if (gRoundNumber == 1 ? D_005F5EE8 >= 0x6A : D_005F5EE8 >= 0x2E) {
        Text_Printf(D_005004A0, 0xC0, 0xDC, 0xA, 2);
    }
}

void Snd_PlaySe(s32 id, s32 player);

/* A sound effect chosen by the round result bits D_005F5CAC while D_005F5F28 is 1 [I]. */
void func_0036EEE0(void) {
    if (D_005F5F28 == 1) {
        if (D_005F5CAC & 1) {
            Snd_PlaySe(0x1054, 0);
        } else if (D_005F5CAC & 8) {
            Snd_PlaySe(0x1058, 0);
        } else if (D_005F5CAC & 4) {
            Snd_PlaySe(0x1000, 0);
        } else if (D_005F5CAC & 0x10) {
            Snd_PlaySe(0x1057, 0);
        } else {
            Snd_PlaySe(0x1059, 0);
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004B0);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004C0);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004D8);
INCLUDE_RODATA("asm/nonmatchings/battle_flow", D_005004F0);
extern const char D_005004B0[];
extern const char D_005004C0[];
extern const char D_005004D8[];
extern const char D_005004F0[];
s32 Text_Printf(const char *fmt, ...);

/* The round end text chosen by the result bits D_005F5CAC (Battle_SetRoundResult) [V calls; I meaning]. */
void func_0036EF58(void) {
    if (D_005F5CAC & 1) {
        Text_Printf(D_005004B0, 0xBC, 0xDC, 0xA, 0xC);
    } else if (D_005F5CAC & 8) {
        Text_Printf(D_005004C0, 0xAA, 0xDC, 0xA, 0xB);
    } else if (D_005F5CAC & 4) {
        Text_Printf(D_005004D8, 0x72, 0xDC, 0xA, 0xC);
    } else if (D_005F5CAC & 0x10) {
        Text_Printf(D_005004F0, 0xF5, 0xDC, 0xA, 0xC, 0xC);
    } else {
        Text_Printf(D_005004F0, 0xF5, 0xDC, 0xA, 0xC, 0xC);
    }
}

#if 0
/* NON_MATCHING: 1 line differs: the original loads D_005F5CBA a second time for the shift (ours reuses the first
 * load) */
void Snd_PlaySe(s32 id, s32 player);
void func_00350578(Fighter *f, s32 side);

/* At the end of a round (D_005F5F28 1: the result sound effect 0x1000 on a draw by unkB4, else 0x1052 / 0x1053 by the
 * winner's bit in D_005F5CBA), then, below state 6, func_00350578 for both fighters [V calls; I meaning]. */
void func_0036F010(void) {
    s32 id;
    s32 side;

    if (D_005F5F28 == 1) {
        if (gFighters[0].unkB4 == gFighters[1].unkB4) {
            id = 0x1000;
        } else {
            side = 1;
            if (gFighters[1].unkB4 < gFighters[0].unkB4) {
                side = 0;
            }
            if (D_005F5CBA == 3 || ((D_005F5CBA >> side) & 1)) {
                id = 0x1052;
            } else {
                id = 0x1053;
            }
        }
        Snd_PlaySe(id, 0);
    }
    if ((u32)D_005F5F28 < 6) {
        func_00350578(&gFighters[0], gFighters[0].unk1E);
        func_00350578(&D_00884EE0, gFighters[1].unk1E);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F010);
#endif

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

/* The life gauge unk408 of both sides: 10, else the average (one member: the value) of the members' life against their
 * maximum in 20.12, 0 when a member in the team has no life left, at least 1 [V code; I meaning]. */
void Battle_UpdateGauges(Fighter *a, Fighter *b) {
    Fighter *f;
    s32 side;
    s32 m;
    s32 any;
    s32 cnt;
    u32 pct[2];

    for (side = 0, f = a; side < 2; side++, f = b) {
        f->unk408 = 10;
        any = 0;
        cnt = 0;
        for (m = 0; m < 2; m++) {
            if (D_00440940[side][m].unk8 == 1) {
                cnt++;
                if (D_00440940[side][m].unkC == 0) {
                    any = 1;
                }
                pct[m] = ((u32)(D_00440940[side][m].unkC / 2) << 12) / (D_00440940[side][m].unk14 / 2);
            } else {
                pct[m] = 0;
            }
        }
        if (cnt != 1) {
            f->unk408 = (pct[0] + pct[1]) >> 1;
        } else {
            f->unk408 = pct[0] + pct[1];
        }
        if (any == 1) {
            f->unk408 = 0;
        } else if (f->unk408 == 0) {
            f->unk408 = 1;
        }
    }
}

/* The result bits of the round in D_005F5CAC: 1 time up with both standing, 0x20 draw (6 when both gauges are empty),
 * 2 K.O., 8 when the winning side lost no red life (perfect) and was not chip damaged, else 0x10 when the winner's
 * gauge is at most 0xCC [V code; I meaning]. */
void Battle_SetRoundResult(Fighter *a, Fighter *b) {
    s32 r = 0;
    Fighter *w;
    Fighter *l;
    s32 side;

    if (gRoundTimer <= 0 && a->unk408 != 0 && b->unk408 != 0) {
        r = 1;
    }
    if (a->unk408 == b->unk408) {
        r |= 0x20;
        if (a->unk408 == 0) {
            r |= 6;
        }
    } else {
        if (b->unk408 < a->unk408) {
            w = a;
            side = 0;
            l = b;
        } else {
            w = b;
            side = 1;
            l = a;
        }
        if (l->unk408 == 0) {
            r |= 2;
            if (D_00440940[side][0].unk2C == D_00440940[side][0].unk10
                && D_00440940[side][1].unk2C == D_00440940[side][1].unk10 && D_005F6670[side] == 0) {
                r |= 8;
            } else if (w->unk408 <= 0xCC) {
                r |= 0x10;
            }
        }
    }
    D_005F5CAC = r;
}

s32 func_0036F658(void) {
    return (D_005F5CAC >> 3) & 1;
}

s32 func_0036F668(void) {
    return (D_005F5CAC >> 4) & 1;
}

extern u8 D_005F5CBE;

s32 func_0036F678(void) {
    switch (D_005F5CBA) {
    case 0:
    case 2:
    default:
        return 0;
    case 1:
        return 1;
    case 3:
        return D_005F5CBE;
    }
}

#if 0
/* NON_MATCHING: 16 lines differ: the winner / loser pointers of a draw are allocated to swapped registers (a3 / a2)
 * and the option test's blocks are ordered differently */
extern u16 D_005F5C70;

/* Counts the round: the side with the higher gauge unk408 wins it (unkB4 = 1 / -1, unk5A + 1); on a draw both get a
 * win point (the leader, or func_0036F678's pick, first) and the rounds left D_005F5C72 drop by one; with the option
 * D_0043B7F8[5] a draw at match point decides the match, else a side reaching D_005F5C70 wins it and the other loses
 * its extra point; the match result goes to func_0036F860 when the match is over [V code; I meanings]. */
void func_0036F6B8(Fighter *a, Fighter *b) {
    Fighter *x;
    Fighter *y;

    if (a->unk408 > b->unk408) {
        a->unkB4 = 1;
        b->unkB4 = -1;
        a->unk5A++;
    } else if (a->unk408 < b->unk408) {
        a->unkB4 = -1;
        b->unkB4 = 1;
        b->unk5A++;
    } else {
        D_005F5C72--;
        if (a->unk5A > b->unk5A || (a->unk5A >= b->unk5A && func_0036F678() == 0)) {
            x = a;
            y = b;
        } else {
            x = b;
            y = a;
        }
        x->unk5A++;
        y->unk5A++;
        y->unkB4 = -1;
        x->unkB4 = -1;
        if (D_0043B7F8[5] != 0) {
            if (x->unk5A >= D_005F5C70 && y->unk5A < D_005F5C70) {
                x->unkB4 = 1;
            }
        } else {
            if (x->unk5A >= D_005F5C70) {
                x->unkB4 = 1;
            }
            if (y->unk5A >= D_005F5C70) {
                y->unk5A--;
            }
        }
    }
    if (gRoundNumber < D_005F5C72 && a->unk5A < D_005F5C70 && b->unk5A < D_005F5C70) {
        return;
    }
    func_0036F860(a, b);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_0036F6B8);
#endif

s32 func_0036F678(void);

/* The round winner from the fighters' unk5A (func_0036F678 on a draw): D_0043B7F8[0x1F] = 1, 2 or 3, [0x20] / [0x21]
 * the two sides' results [I]. */
void func_0036F860(Fighter *a, Fighter *b) {
    s32 r1;
    s32 kind;
    s32 r0;

    if (a->unk5A > b->unk5A) {
        r1 = 1;
        kind = 1;
        r0 = 0;
    } else if (a->unk5A < b->unk5A) {
        r0 = 1;
        r1 = 0;
        kind = 2;
    } else {
        r0 = func_0036F678();
        kind = 3;
        r1 = (r0 + 1) & 1;
    }
    D_0043B7F8[0x1F] = kind;
    D_0043B7F8[0x20] = r0;
    D_0043B7F8[0x21] = r1;
}

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

void func_003A8890(Fighter *f);
void func_003A88A0(Fighter *f);

/* Follows move flag 0x20000 (unk13E) and calls func_003A8890 / func_003A88A0 when it turns on / off; `reset` clears
 * it [V code; I meaning]. */
void func_00370E20(Fighter *f, s32 reset) {
    if (reset != 0) {
        f->unk13F = 0;
        f->unk13E = 0;
    } else {
        f->unk13E = (f->unkC0->unk24 >> 17) & 1;
        if (f->unk13E != f->unk13F) {
            if (f->unk13E != 0) {
                func_003A8890(f);
            } else {
                func_003A88A0(f);
            }
        }
        f->unk13F = f->unk13E;
    }
}

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

s32 abs(s32 x);

/* What is left of `lim` beyond |b| when |a| + |b| exceeds it, with the sign of a; else 0 [I: overlap along an axis]. */
s32 func_00370ED0(s16 a, s16 b, s32 lim) {
    s32 n = abs(b);
    s16 r;

    if (lim < n + abs(a)) {
        r = lim - n;
        if (a < 0) {
            return -r;
        }
        return r;
    }
    return 0;
}

#if 0
/* NON_MATCHING: 88 lines differ: register allocation (t0 / t1 swapped for the -1 and table constants, a2 / a3 for the
 * angles) and the order of the unk48 - 0x4000 computation */
s32 func_00366108(s32 dx, s32 dz);

/* Per frame, for both fighters (from Fighter_StepMove): the body angle against the facing (unk3E, unk40) and its
 * quadrant unk42, the facing target unk44 when far apart, the direction to the other fighter against the camera
 * (unk4C) and the quadrants / octants of the body angle against the camera (unk50-unk58) [V code; I meaning]. */
void func_00370F28(Fighter *f, Fighter *o) {
    Fighter *a;
    Fighter *b;
    s32 i;
    u16 c;
    u16 d;
    s16 v;
    s16 e;
    s16 r;
    s32 unused[12]; /* not referenced: the original frame has 0x30 bytes of locals below the caller-save slots */

    for (i = 0, a = f, b = o; i < 2; i++, a = o, b = f) {
        a->unk3E = a->unk3C - a->unk3A;
        a->unk42 = a->unk3E + 0x2000;
        if ((u16)a->unk42 < 0x4000) {
            a->unk42 = 0;
        } else if (a->unk42 >= 0) {
            a->unk42 = 1;
        } else if ((u16)a->unk42 <= 0xBFFF) {
            a->unk42 = 2;
        } else {
            a->unk42 = 3;
        }
        a->unk40 = a->unk3E;
        if ((s16)a->unk3E < 0) {
            a->unk40 = 0xFFFF - a->unk3E;
        }
        if (D_005F5C28 > 0x200) {
            if (a->unk40 < 0x4000) {
                a->unk44 = a->unk3A;
            } else {
                a->unk44 = a->unk3A - 0x8000;
            }
        }
        a->unk4C = (func_00366108(a->unk51C - b->unk51C, a->unk524 - b->unk524) - D_00415808[1]) % 0x1000;
        if (a->unk4C < 0) {
            a->unk4C += 0x1000;
        }
        c = a->unk3C;
        d = c - a->unk48;
        if ((s16)d < 0) {
            d = 0xFFFF - d;
        }
        if (d < 0x4000) {
            r = a->unk48;
        } else {
            r = a->unk48 - 0x8000;
        }
        v = (u16)a->unk48 + 0xC000;
        d = c - v;
        a->unk50 = r;
        if ((s16)d < 0) {
            d = 0xFFFF - d;
        }
        if (d < 0x4000) {
            r = v;
        } else {
            r = v - 0x8000;
        }
        a->unk52 = r;
        e = c - (s16)((0x400 - D_00415808[1] % 0x1000) << 4);
        v = e + 0x2000;
        if ((u16)v < 0x4000) {
            a->unk54 = 0;
        } else if (v >= 0) {
            a->unk54 = 1;
        } else if ((u16)v <= 0xBFFF) {
            a->unk54 = 2;
        } else {
            a->unk54 = 3;
        }
        if ((s16)(e + 0x4000) < 0) {
            a->unk56 = 2;
        } else {
            a->unk56 = 0;
        }
        v = e + 0x1000;
        if ((u16)v < 0x2000) {
            a->unk58 = 0;
        } else if ((u16)v < 0x4000) {
            a->unk58 = 1;
        } else if ((u16)v < 0x6000) {
            a->unk58 = 2;
        } else if (v >= 0) {
            a->unk58 = 3;
        } else if ((u16)v <= 0x9FFF) {
            a->unk58 = 4;
        } else if ((u16)v <= 0xBFFF) {
            a->unk58 = 5;
        } else if ((u16)v <= 0xDFFF) {
            a->unk58 = 6;
        } else {
            a->unk58 = 7;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00370F28);
#endif

/* Move state 9 (unkD5): from the frame unkC0->unk19 of the move on, sets bit 4 of the guard mask unkCC [V code; I
 * meaning]. */
void func_00371258(Fighter *f) {
    switch (f->unkD5) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        break;
    case 9:
        if (f->unkC0->unk19 != 0 && f->unkC4 >= f->unkC0->unk19) {
            f->unkCC |= 4;
        }
        break;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        break;
    }
}

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

s16 func_003772B0(Fighter *a, Fighter *b, HitContact *ca, HitContact *cb);

/* The hits of the frame (Battle_UpdateFrame step 8): clears the hit flags, runs the hit test (func_003772B0, bit 0 when
 * a hits b, bit 1 when b hits a) and lets each attacker p hit its defender q when its attack level unkD0 meets q's mask
 * unkCC & 7 (bit 0 added against a crouching q, unk11A == 2), with these exceptions: a throw (level 0x10F) misses a q in
 * move state 0xE (unkD5) between the frames unk19 and unk1A - 5 of its move or juggled (unk135); level 0x907 only hits a
 * juggled q or one in move flag 0x400; nothing hits q right after a tag (unkB134), and character 0x1D's moves
 * 0x12A2-0x12A5 never hit. A hit sets p's unkF1 / unkF4 / unkF5 / unk132 and q's unkF3 / unk12F, the hit freeze unk14C
 * (byte 0x2E of p's move), the hit kind unk11C, and the pairing record q->unk18C: the reaction entry's unk6 (close,
 * with unk13B) or unk4 (far, by the distance D_005F5C28 against its unk8) as an index of D_0046BE78, 0x592 or 0 for
 * none [V code; I meanings]. */
void Battle_ResolveHits(Fighter *a, Fighter *b) {
    Fighter *x;
    Fighter *p;
    Fighter *q;
    s16 i;
    s32 hits;
    s32 bit;
    u16 lvl;
    u16 id;

    for (i = 0, x = a; i < 2; i++, x = b) {
        x->unk12F = 0;
        x->unk132 = 0;
        x->unk133 = 0;
        x->unk14C = 0;
        x->unk13B = 0;
    }
    hits = func_003772B0(a, b, &a->unk39C, &b->unk39C);
    for (p = a, q = b, bit = 1, i = 0; i < 2; i++, p = b, q = a, bit = 2) {
        if (p->unkD0 == 0x10F && q->unkD5 == 0xE
            && ((q->unkC4 >= q->unkC0->unk19 && q->unkC4 <= q->unkC0->unk1A - 5) || q->unk135 != 0)) {
            continue;
        }
        if (p->unkD0 == 0x907 && q->unk135 == 0 && !(q->unkC0->unk4 & 0x400)) {
            continue;
        }
        if (q->unkB134 != 0) {
            continue;
        }
        if (p->unk2A == 0x1D && (u16)(p->unk10C - 0x12A2) < 4) {
            continue;
        }
        lvl = p->unkD0;
        if (q->unk11A == 2) {
            lvl |= 1;
        }
        if (p->unk127 == 0 || q->unk127 == 0 || p->unkF1 != 0 || !(hits & bit) || !(lvl & (u16)q->unkCC & 7)) {
            continue;
        }
        if (p->unkCA == 0 && q->unk139 != 0) {
            continue;
        }
        p->unkF1 = 1;
        p->unkF4 = 1;
        p->unkF5 = 1;
        p->unk132 = 1;
        q->unkF3 = 1;
        q->unk12F = 1;
        q->unk14C = (s8)p->unkC0->unk2E;
        q->unk11C = p->unk11B;
        if (D_005F5C28 < func_0036C1A0(p, q, p->unkC0)->unk8) {
            id = func_0036C1A0(p, q, p->unkC0)->unk6;
            if (id == 0x592) {
                q->unk18C = NULL;
            } else if (id == 0) {
                q->unk18C = NULL;
            } else {
                q->unk18C = (struct FighterUnk18C *)&D_0046BE78[func_0036C1A0(p, q, p->unkC0)->unk6];
                q->unk13B = 1;
            }
        } else {
            id = func_0036C1A0(p, q, p->unkC0)->unk4;
            if (id == 0x592) {
                q->unk18C = NULL;
            } else if (id == 0) {
                q->unk18C = NULL;
            } else {
                q->unk18C = (struct FighterUnk18C *)&D_0046BE78[func_0036C1A0(p, q, p->unkC0)->unk4];
            }
        }
        if (p->unkCA != 0 || q->unk135 != 0) {
            q->unk14E = 4;
        }
    }
    for (i = 0, p = a; i < 2; i++, p = b) {
        if (p->unkC0->unk2D == p->unkC4 && p->unkF5 == 0) {
            p->unk133 = 1;
        }
    }
}

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

#if 0
/* NON_MATCHING: 11 lines differ: the order of the position stores (unk70C ... unk710) and the registers of their two
 * constants */
extern u16 D_005F5C62;
void Snd_StopVoiceSeq(Fighter *f);
void func_00377F58(Fighter *f);
void func_0034E8B8(Fighter *f);
void func_00373218(Fighter *f);
void func_0036BC50(Fighter *f);
void func_0036BD50(Fighter *f);

/* Puts a fighter at its round start: side 0 at x -1000 facing 0x4000, side 1 at 1000 facing -0x4000, both on z 0,
 * velocity and gauge cleared, then the move state (func_00373218), voice, body spheres and input state reset [V]. */
void func_003730E8(Fighter *f, Fighter *o, s32 side) {
    f->unkB12C = NULL;
    f->unk4 = side == 0 ? -1000 : 1000;
    f->unk8 = 0;
    f->unkC = 0;
    f->unk18 = 0;
    if (side == 0) {
        f->unk1A = 0x4000;
    } else {
        f->unk1A = -0x4000;
    }
    f->unk1C = 0;
    f->unk40 = f->unk3C = f->unk1A;
    if (side == 0) {
        f->unk624 = -100;
    } else {
        f->unk624 = 100;
    }
    f->unk3E8 = 0;
    f->unk3EC = 0;
    f->unk3F0 = 0;
    f->unk408 = 0;
    f->unk70C = -0x591;
    f->unk6B8 = f->unk4;
    f->unk6BC = -0x41B;
    f->unk6C0 = f->unkC;
    f->unk51C = f->unk4;
    f->unk524 = f->unkC;
    f->unk708 = f->unk4;
    f->unk710 = f->unkC;
    func_00373218(f);
    Snd_StopVoiceSeq(f);
    func_00377F58(f);
    func_0034E8B8(f);
    func_00371B78(f);
    f->unkB132 = 1;
    f->unkB134 = 0;
    f->unk12A = 0;
    f->unk12B = 0;
    if (D_005F5C62 != 0 && f->unk418 == 0) {
        f->unk12C = 1;
    } else {
        f->unk12C = 0;
    }
    func_0036BC50(f);
    func_0036BD50(f);
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003730E8);
#endif

/* Clears the move and hit state of a fighter for a new round (then func_00370E20(f, 1)) [V stores]. */
void func_00373218(Fighter *f) {
    f->unkD6 = -1;
    f->unkE4 = 0;
    f->unkF0 = 0;
    f->unkFE = 0;
    f->unk100 = 0;
    f->unk106 = 0;
    f->unk108 = 0;
    f->unk10A = 0;
    f->unk119 = 0;
    f->unkF4 = 0;
    f->unkF5 = 0;
    f->unkF3 = 0;
    f->unkF6 = 0;
    f->unkF7 = 0;
    f->unk11B = 0;
    f->unk438.unk82 = 0;
    f->unk120 = 0;
    f->unk11D = 0;
    f->unkF9 = 0;
    f->unk122 = 0;
    f->unkE2 = 0;
    f->unkB4 = 0;
    f->unkB6 = 0;
    f->unk14E = 0;
    f->unk150 = 0;
    f->unk154 = 0;
    f->unk15C = 0;
    f->unk15E = 0;
    f->unk160 = 0;
    f->unk162 = 0;
    f->unk16C = 0;
    f->unk16E = 0;
    f->unk16A = 0;
    f->unk170 = 0;
    f->unk12D = 0;
    f->unk172 = 0;
    f->unk12F = 0;
    f->unk132 = 0;
    f->unk133 = 0;
    f->unk13A = 0;
    f->unk13B = 0;
    f->unk130 = 0;
    f->unk131 = 0;
    f->unk138 = 0;
    f->unk146 = 0;
    f->unk135 = 0;
    f->unk14A = 0;
    f->unk14C = 0;
    f->unk126 = 0;
    f->unk12E = 0;
    f->unkB878 = 0;
    f->unkB880 = 0;
    f->unkB884 = 0;
    f->unkB870[0] = 0;
    f->unkB870[1] = 0;
    f->unkB870[2] = 0;
    f->unk3E8 = 0;
    f->unk3EC = 0;
    f->unk3F0 = 0;
    func_00370E20(f, 1);
}

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

void func_00373400(Fighter *f);

/* Resets the motion blend of a fighter (func_00373400 first) [I]. */
void func_00373398(Fighter *f) {
    s32 i;

    func_00373400(f);
    f->unkB5E0 = 0;
    f->unkB5DC = 0;
    f->unkB878 = 0;
    f->unkB880 = 0;
    f->unkB884 = 0;
    for (i = 0; i < 3; i++) {
        f->unkB870[i] = 0;
    }
}

/* Keeps the current move record, unk32 and frame as the start of a motion blend [I]. */
void func_00373400(Fighter *f) {
    f->unkB5E8 = f->unkC0;
    f->unkB5D2 = f->unk32;
    f->unkB5F0 = f->unkC4;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373428);

#if 0
/* NON_MATCHING: 34 lines differ: the original keeps the move record in two registers (t0 and a0) and computes unk2C -
 * (unkC4 + 1) apart from the window test */
/* Starts a blend at the end of the current move (state 1, up to 16 frames before its window, 4 or 16 frames without
 * one, at most the frames left) when the move changed or unk134 is set; returns whether a blend runs [I]. */
s32 func_003736B8(Fighter *f) {
    struct FighterMove *m = f->unkC0;
    s32 rem;
    s32 n;

    if (f->unkB5E8 == m && f->unkF8 != 3) {
        return 0;
    }
    if (f->unk134 != 0 || f->unkB5E8 != m) {
        rem = m->unk18 - f->unkC4;
        if (rem < 0) {
            rem = 0;
        }
        n = 0;
        if (rem != 0) {
            if (m->unk2C != 0) {
                if (f->unkC4 < m->unk2C - 1) {
                    n = m->unk2C - (f->unkC4 + 1);
                    if (n > 16) {
                        n = 16;
                    }
                }
            } else {
                n = 4;
                if (f->unkE0 < 0) {
                    n = 16;
                }
            }
        }
        if (rem < n) {
            n = rem;
        }
        if (n >= 2) {
            f->unkB5DC = 1;
            f->unkB5E0 = 1;
            f->unkB5E4 = n;
        }
    }
    return f->unkB5E0;
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_003736B8);
#endif

s32 func_0036BF58(Fighter *f);

/* Starts a blend into the pending move (state 2, unkB5E4 frames: those left before it starts) unless the current move is
 * still inside its window unkC0->unk2D or the pending move's window holds the frame it will start on
 * (func_0036BF58); returns whether a blend runs [I]. */
s32 func_003737B0(Fighter *f) {
    struct FighterMove *m;
    s32 n;
    s16 frame;

    if (f->unk188 != NULL && f->unkF8 != 3 && f->unkC4 < f->unk178) {
        m = func_00368218(f, f->unk184);
        frame = f->unkC4;
        f->unkB5E4 = f->unk178 - frame + 1;
        if (f->unkC0->unk2D >= frame) {
            f->unkB5E4 = 0;
        } else if (m->unk2C != 0) {
            n = func_0036BF58(f) + 1;
            if (n >= m->unk2C && n <= m->unk2D) {
                f->unkB5E4 = 0;
            }
        }
        if (f->unkB5E4 < 2) {
            f->unkB5E4 = 0;
        } else {
            f->unkB5DC = 2;
            f->unkB5E0 = 1;
        }
    }
    return f->unkB5E0;
}

/* Starts a blend over the last frames of the current move when no move is pending and fewer than 16 frames are left
 * (3, unkB5E4 frames); returns whether a blend is running [I]. */
s32 func_003738C0(Fighter *f) {
    s32 len = f->unkC0->unk18;
    s32 left;
    s32 n;

    if (f->unk188 == NULL) {
        left = len - f->unkC4;
        if (left < 0x10) {
            n = left + 1;
            if (f->unkDA <= 0) {
                n = f->unkC4;
            }
            if (n >= 2) {
                f->unkB5DC = 3;
                f->unkB5E0 = 1;
                f->unkB5E4 = n;
            }
        }
    }
    return f->unkB5E0;
}

INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373930);

#if 0
/* NON_MATCHING: 13 lines differ: block order; the original places the case 2 / 3 'unkB878 < unkB880' branch after
 * case 1 and case 1's negative branch as a return */
/* Steps the blend weight unkB880 by the blend state unkB5DC: 0 clears, 1 counts down to 0, 2 / 3 count up to unkB878 [I]. */
void func_00373D48(Fighter *f) {
    switch (f->unkB5DC) {
    case 2:
    case 3:
        if (f->unkB880 < f->unkB878) {
            f->unkB880++;
        } else if (f->unkB878 < f->unkB880) {
            f->unkB880 = f->unkB878;
        }
        break;
    case 0:
        f->unkB880 = 0;
        f->unkB878 = 0;
        break;
    case 1:
        if (f->unkB880 > 0) {
            f->unkB880--;
        } else if (f->unkB880 < 0) {
            f->unkB880 = 0;
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", func_00373D48);
#endif

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

s32 abs(s32 x);

/* The launch kind (unkFC, record func_00374010) of a fighter thrown in the air by the other's move: the move's byte
 * 0x2F, else 1 for a fast fall (unk3E0 >= 110), 2 / 3 / 0 by the facing quadrant unk42; while the fighter is in move
 * flag 0x200 or moves 0x129B-0x129F: 7 for a fast fall, else 5 / 6 by the height |unk520| [V code; I meaning]. */
s32 Fighter_GetLaunchKind(Fighter *f, Fighter *o) {
    s32 r = 0;
    s32 c;
    s16 h;

    c = 0;
    do { /* fake */
        if (!(f->unkCC & 0x200)) {
            c = 1;
        }
        if ((u16)(f->unk10C - 0x129B) <= 2) {
            c = 0;
        }
        if (f->unk10C == 0x129E && f->unkC4 >= 0x13) {
            c = 0;
        }
        if (f->unk10C == 0x129F && f->unkC4 >= 0x13) {
            c = 0;
        }
    } while (0);
    if (c != 0) {
        if (o->unkC0->unk2F != 0) {
            r = o->unkC0->unk2F;
        } else if (f->unk3E0 >= 0x6E) {
            r = 1;
        } else if (f->unk10C != 0x1279) {
            if (f->unk42 == 3) {
                r = 2;
            } else if (f->unk42 == 1) {
                r = 3;
            } else {
                r = 0;
            }
        }
    } else if (f->unk3E0 >= 0x6E) {
        r = 7;
    } else {
        h = abs(f->unk520);
        r = 5;
        if (!(h > 0x2DF)) {
            r = 6;
        }
    }
    return r;
}

#if 0
/* NON_MATCHING: 148 lines differ: the original places the juggle branch (unk100 != 0) as an 8-aligned block before
 * its test, reached by a backward branch, and the t - 1 + t1 branch out of line after the first if (a loop or goto
 * structure not found yet); register allocation follows from it */
s16 Math_Sin(s32 angle);
s32 abs(s32 x);
s16 Math_Cos(s32 angle);
void func_00363AB8(s32 move, s16 *pos, s32 frame, Fighter *f);

/* The launch of a fighter (Fighter_StepMove kinds 0x1E and 0x26, record func_00374010(unkFC)): the height to reach
 * (record unk2, else the move's position at its last frame, func_00363AB8), the launch speed unk104 (0-100, more with
 * each hit of the juggle unk100) split by the facing into unk106 / unk10A, the vertical speed unk108 and the frames in
 * the air unk178, so that the arc lands where the move continues [V code; I meaning]. */
void Fighter_StartLaunch(Fighter *f, Fighter *o) {
    Unk500678 *rec;
    struct FighterMove *m;
    s32 len;
    s32 n;
    s16 h;
    s32 y;
    s32 dy;
    s32 speed;
    s32 vy;
    s32 t;
    s32 t1;
    s32 cs;
    s16 pos[4];

    rec = func_00374010(f->unkFC);
    m = f->unk188;
    f->unk178 = t1 = 1;
    len = m->unk1B;
    if (len <= 0) {
        len = m->unk18;
    }
    y = -f->unk520;
    if (rec->unk2 != 0) {
        h = f->unk102 = rec->unk2;
    } else {
        n = m->unk1B - 1;
        if (n < 0) {
            n = m->unk18;
        }
        func_00363AB8(m->unk0, pos, n, f);
        h = pos[1];
        f->unk102 = h;
    }
    speed = (f->unk100 + 1) * 7 + 10;
    speed = f->unk146 + speed;
    if (f->unkFC == 4) {
        speed += 10;
    }
    dy = h - y;
    if (speed < 0) {
        speed = 0;
    }
    if (speed > 100) {
        speed = 100;
    }
    f->unk104 = speed;
    f->unk106 = f->unk104 * Math_Sin((s16)(f->unk3A - 0x8000)) / 0x8000;
    cs = Math_Cos((s16)(f->unk3A - 0x8000));
    vy = f->unk100 * 7 - 40;
    vy = f->unk146 * 4 - ((f->unk3C8 + f->unk3E0) >> 3) - vy;
    if (vy < -100) {
        vy = -100;
    }
    if (vy > 100) {
        vy = 100;
    }
    f->unk108 = -vy;
    n = abs(vy * vy + dy * 12);
    f->unk10A = f->unk104 * cs / 0x8000;
    t = (vy + Math_Isqrt(n)) / 6;
    if (t <= 0) {
        t = 1;
    }
    t1 = len - t + 1;
    if (t1 <= 0) {
        t1 = 1;
    }
    if (rec->unk6 != 0) {
        t += t1 - rec->unk6;
        t1 = rec->unk6;
    } else if ((f->unkCC & 4) || t1 == 1) {
        if (f->unk100 != 0) {
            if (f->unkC0->unk0 == f->unk188->unk0) {
                if (abs(f->unkC4 - t1) < 10) {
                    n = f->unkC4 - 10;
                    if (n <= 0) {
                        n = 1;
                    }
                    t += t1 - n;
                    t1 = n;
                }
            }
        } else {
            t += 10;
            t1 = len - t + 1;
            if (t1 <= 0) {
                t1 = 1;
            }
        }
    } else {
        t = t - 1 + t1;
        t1 = 1;
    }
    f->unk178 = t1;
    if (rec->unk4 != 0) {
        f->unk108 = rec->unk4;
    }
    if (rec->unk6 != 0) {
        f->unk178 = rec->unk6;
    }
    f->unk158 = &rec->unk8;
    if (rec->unk8 != 0) {
        f->unk150 = *f->unk158++;
        f->unk152 = *f->unk158++;
    } else {
        f->unk150 = 0;
        f->unk158 += 2;
        f->unk152 = 0;
    }
    f->unk154 = 8;
    f->unk156 = f->unk3A - 0x8000;
    if (rec->unk4 == 0) {
        n = t + 1;
        if (vy * n - 3 * n * n + y < h) {
            f->unk108 = -((dy + 3 * n * n) / n);
        }
        n = len - t1 + 4;
        if (n < t) {
            f->unk108 = -((3 * n * n + dy) / n);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle_flow", Fighter_StartLaunch);
#endif

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
