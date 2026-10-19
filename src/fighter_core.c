/* 0x37BA88-0x38BF98 (module fighter_core, docs/game_overview.md section 4; docs/systems/fighter.md). Despite the
 * module name this is not the move interpreter or hit detection: 0x37BA88-0x3800D8 is the practice mode support
 * (cursor sounds, hit markers, the command recorder gPracRecorder and its playback, the command lists
 * gPracCmdLists, guard / action conditions, the practice reset), and 0x3800D8-0x38BF98 is the CPU opponent
 * (Cpu_GetInput: one call per frame per CPU side, a candidate list of moves filtered by predicates, scripted
 * button commands, 35 move handlers of the table D_00507A88). No .rodata, .lit4 or .sdata of its own is emitted:
 * the tables it reads stay in the split data. */
#include "common.h"
#include "fighter.h"

typedef struct {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ s16 unk6;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
} Unk37BC60Mark;

typedef struct {
    /* 0x00 */ u8 pad0[0x38];
    /* 0x38 */ u8 unk38;
    /* 0x39 */ u8 pad39[3];
    /* 0x3C */ Unk37BC60Mark unk3C[4];
} Unk37BC60;

typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 unk8;
} Unk8A7B80;

extern s32 D_005F5F9C;
extern s32 D_005F5FA0;
extern s32 D_005F6BF8;
extern Unk8A7B80 D_008A7B80;
extern Fighter gFighters[4];

/* Declared returning a value here: the calls are not turned into tail calls. */
s32 Snd_PlaySe(s32 id, s32 player);
s32 func_003794A0(s32 item);
void func_0037BF48(s32 arg0, s32 arg1);
s32 func_0037D4F8(Fighter *f);

/* Practice mode command recorder (gPracRecorder, one per player). */
typedef struct {
    /* 0x0 */ u16 unk0; /* frames held */
    /* 0x2 */ u16 unk2; /* buttons */
} CmdStep;

typedef struct {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 unk4; /* step count */
    /* 0x008 */ s32 unk8; /* playback step */
    /* 0x00C */ s32 unkC; /* frames left of the step */
    /* 0x010 */ CmdStep unk10[0x100];
} CmdTrack;

typedef struct {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 unk4;
    /* 0x008 */ s32 unk8;
    /* 0x00C */ s32 unkC;
    /* 0x010 */ s32 unk10;
    /* 0x014 */ s32 unk14;
    /* 0x018 */ s32 unk18;
    /* 0x01C */ s32 unk1C;
    /* 0x020 */ u16 unk20;
    /* 0x022 */ u16 unk22;
    /* 0x024 */ u16 unk24;
    /* 0x026 */ u16 unk26[0x100];
    /* 0x226 */ CmdStep unk226[0x200];
    /* 0xA26 */ u8 padA26[2];
    /* 0xA28 */ CmdTrack unkA28[2];
} CmdRecorder;

extern CmdRecorder gPracRecorder[2];
extern u8 D_0042F268[];
extern Fighter D_00884EE0;

void func_0037AF80(void *a, Fighter *b, Fighter *c);
void func_0037B4F8(void *a, Fighter *b, Fighter *c);
void func_0037C748(s32 player);
void func_0037C750(s32 player);
void Prac_RecSaveTrack(s32 player, s32 track);
void func_0037C8C8(s32 player);
void Prac_PlayBegin(s32 player, s32 track);
u16 Prac_PlayStep(s32 player, s32 track);

/* Practice mode command lists (gPracCmdLists, one per character slot, indexed by Fighter.unk22). */
typedef struct {
    /* 0x00 */ u16 unk0; /* step count */
    /* 0x02 */ u16 unk2; /* first step */
    /* 0x04 */ u16 unk4;
    /* 0x06 */ u16 unk6;
    /* 0x08 */ u16 unk8;
    /* 0x0A */ u16 unkA;
    /* 0x0C */ u16 unkC;
    /* 0x0E */ u16 unkE;
    /* 0x10 */ u16 unk10;
    /* 0x12 */ u16 unk12[8];
    /* 0x22 */ u16 unk22[16];
} CmdListEntry;

typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u16 unk6;
} CmdListStep;

typedef struct {
    /* 0x0000 */ s32 unk0; /* entry count */
    /* 0x0004 */ s32 unk4; /* step count */
    /* 0x0008 */ CmdListEntry unk8[0x7B];
    /* 0x1FBE */ CmdListStep unk1FBE[0x3DA];
    /* 0x3E8E */ u8 pad3E8E[2];
    /* 0x3E90 */ s32 unk3E90;
    /* 0x3E94 */ s32 unk3E94[16];
    /* 0x3ED4 */ s32 unk3ED4;
    /* 0x3ED8 */ s32 unk3ED8[0x80];
} CmdList;

typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u16 unk6;
} Unk8B7EF0;

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
} Unk431848;

extern CmdList gPracCmdLists[4];
extern Unk8B7EF0 D_008B7EF0[2];
extern Unk431848 gPracReset;
extern u8 D_0043B7F8[];
extern s32 D_005F60A0;

s32 func_0036CFC0(u16 buttons);
s32 func_0037D3A0(Fighter *f, s32 i);
u32 func_0037D478(Fighter *f);
u16 func_0037D498(Fighter *f, s32 i);
void Prac_LoadCmdIntoTrack(Fighter *f, s32 i);
void func_0037CC60(s32 player);

extern u32 D_005F5C28;

void func_0034CB38(s32 arg0);
void func_0035B9A8(Fighter *a, Fighter *b);
struct FighterMove *func_00368218(Fighter *f, s16 move);
void func_00368818(Fighter *a, Fighter *b, s32 arg2);
void func_003730E8(Fighter *a, Fighter *b, s32 arg2);
void func_00373390(Fighter *f, s32 arg1);
void func_0037DA08(Fighter *f, s32 state);
s32 func_0037DC38(s32 state);
void func_0038C0F0(void);
void func_00390E08(Fighter *a, Fighter *b);

/* The move data of the fighter's current move (inlined at every use). */
static inline struct FighterMove *Fighter_GetMove(Fighter *f) {
    if (f->unk188 != NULL) {
        return f->unk188;
    }
    return func_00368218(f, f->unkC0->unk10);
}

/* CPU opponent. */
typedef struct {
    /* 0x0 */ u16 unk0; /* command word: direction in bits 0-3, buttons in bits 8-11 */
    /* 0x2 */ u8 pad2[4];
    /* 0x6 */ u16 unk6;
    /* 0x8 */ u16 unk8;
    /* 0xA */ u8 padA;
    /* 0xB */ u8 unkB;
    /* 0xC */ u8 unkC;
    /* 0xD */ u8 unkD;
} CpuCmd;

/* A move the CPU may choose (gCpuCands, CpuState.unkAC of them). */
typedef struct {
    /* 0x00 */ s16 unk0; /* 0 candidate, 1 picked, -1 rejected */
    /* 0x02 */ u8 pad2[2];
    /* 0x04 */ struct FighterUnkC0 *unk4;
    /* 0x08 */ CpuCmd *unk8;
} CpuCand;

/* CPU opponent state (gCpuStates, one per side; gCpu points to the one thinking). */
typedef struct {
    /* 0x000 */ s32 unk0; /* side [I] */
    /* 0x004 */ u16 unk4; /* frame counter [I] */
    /* 0x006 */ u16 unk6; /* buttons to hold this frame */
    /* 0x008 */ u16 unk8; /* buttons of the previous frame */
    /* 0x00A */ u16 unkA;
    /* 0x00C */ u16 *unkC; /* command sequence being played */
    /* 0x010 */ u8 pad10[0x10];
    /* 0x020 */ s32 unk20;
    /* 0x024 */ s32 unk24;
    /* 0x028 */ u8 pad28[0x24];
    /* 0x04C */ s32 unk4C;
    /* 0x050 */ s32 unk50;
    /* 0x054 */ u8 pad54[8];
    /* 0x05C */ s32 unk5C;
    /* 0x060 */ u8 pad60[0xC];
    /* 0x06C */ s32 unk6C;
    /* 0x070 */ u8 pad70[0x1C];
    /* 0x08C */ s32 unk8C;
    /* 0x090 */ u8 pad90[8];
    /* 0x098 */ s32 unk98;
    /* 0x09C */ u8 pad9C[0x10];
    /* 0x0AC */ s32 unkAC; /* candidate count */
    /* 0x0B0 */ s32 unkB0; /* candidates picked */
    /* 0x0B4 */ s32 unkB4;
    /* 0x0B8 */ s32 unkB8;
    /* 0x0BC */ s32 (*unkBC)(CpuCand *c);
    /* 0x0C0 */ u8 padC0[0x1EC];
    /* 0x2AC */ s32 unk2AC;
    /* 0x2B0 */ u8 pad2B0[0x20];
    /* 0x2D0 */ struct FighterUnkC0 *unk2D0[0x14];
    /* 0x320 */ u8 pad320[0x1C];
    /* 0x33C */ s32 unk33C;
    /* 0x340 */ u8 pad340[4];
    /* 0x344 */ u8 unk344[0x80];
    /* 0x3C4 */ u8 pad3C4[0x1C];
    /* 0x3E0 */ s32 unk3E0;
    /* 0x3E4 */ u8 pad3E4[4];
} CpuState;

extern CpuState *gCpu;
extern Fighter *gCpuSelf;
extern Fighter *gCpuOpponent;
extern s32 D_005F6C08;
extern CpuCand gCpuCands[];
extern s32 D_004318D8[][6];
extern const s16 D_00507860[];
extern const s16 D_00507868[];

s32 rand(void);
s32 func_003840F8(CpuCand *c);

void Cpu_SetCommand(u16 *cmd);
void func_00380568(Fighter *f);
void Cpu_GetInput(Fighter *f, u16 *held, u16 *pressed);
extern const u16 D_005074A2[];
extern const u16 D_0050748A[];

/* ---- functions ---- */
void func_0037BA88(void) {
    D_005F5FA0 = 1;
}

void func_0037BA98(void) {
    D_005F5F9C = 0;
    D_005F5FA0 = 0;
}

void func_0037BAA8(void) {
    D_005F5F9C = 0;
}

#if 0
/* NON_MATCHING: reorg fills the delay slot of the `changed` test with `ld s0` (beqzl); the original has `andi` (beqz). */
s32 func_0037BAB0(s32 arg0, s32 arg1) {
    s32 changed = 0;

    if (D_005F5FA0 == 0) {
        return;
    }
    if (arg0 != 0) {
        changed = arg0 != D_005F5F9C;
    }
    D_005F5F9C = arg0;
    if (arg1 & 0x10) {
        if (changed) {
            Snd_PlaySe(0x101B, 0);
        }
    } else if (arg1 & 0x20) {
        if (!changed) {
            return;
        }
        if (arg1 & 1) {
            Snd_PlaySe(0x1039, 0);
        }
        if (arg1 & 2) {
            Snd_PlaySe(0x103A, 0);
        }
        if (arg1 & 4) {
            Snd_PlaySe(0x103B, 0);
        }
        if (arg1 & 8) {
            Snd_PlaySe(0x103C, 0);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BAB0);
#endif

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BB70);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BC00);

void func_0037BC60(Unk37BC60 *p) {
    Unk37BC60Mark *m;
    s32 i;

    p->unk38 = 0;
    m = p->unk3C;
    for (i = 0; i < 4; i++) {
        m->unk0 = 5;
        m->unk2 = 0;
        m->unk4 = 0;
        m->unk6 = 0;
        m++;
    }
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BC98);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BDD8);

void func_0037BEF0(s32 arg0) {
    func_0037BF48(arg0, 1);
}

void func_0037BEF8(s32 arg0) {
    s32 v = func_003794A0(4);

    if (v != D_005F6BF8) {
        func_0037BF48(arg0, v);
    }
    D_005F6BF8 = v;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BF48);

s32 func_0037BFB8(s32 arg0, Fighter *arg1, Fighter *arg2) {
    return func_0037D4F8(func_003794A0(9) == 0 ? arg1 : arg2) != 0;
}

void func_0037C000(void) {
    D_008A7B80.unk0 = 0;
    D_008A7B80.unk4 = 0;
    D_008A7B80.unk8 = 0;
}

void func_0037C018(void) {
    D_008A7B80.unk0 = 1;
    D_008A7B80.unk4 = 1;
    D_008A7B80.unk8 = 0;
}

void func_0037C038(void) {
    D_008A7B80.unk0 = 1;
    D_008A7B80.unk8 = 1;
    D_008A7B80.unk4 = 0;
}

s32 func_0037C058(void) {
    s32 r = 0;

    if (D_008A7B80.unk0 != 0) {
        r = D_008A7B80.unk4 != 0;
    }
    return r;
}

s32 func_0037C080(void) {
    s32 r = 0;

    if (D_008A7B80.unk0 != 0) {
        r = D_008A7B80.unk8 != 0;
    }
    return r;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037C0A8);

s32 func_0037C168(void) {
    if (((gFighters[0].unkC0->unk24 >> 20) & 3) == 2 || ((gFighters[1].unkC0->unk24 >> 20) & 3) == 2) {
        return 1;
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037C1B8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037C370);

void func_0037C438(void) {
    func_0037AF80(D_0042F268, &gFighters[0], &D_00884EE0);
}

void func_0037C458(void) {
    func_0037B4F8(D_0042F268, &gFighters[0], &D_00884EE0);
}

void Prac_RecReset(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];
    CmdStep *s;
    s32 i;

    r->unk0 = 0;
    r->unk4 = 0;
    r->unk8 = 0;
    r->unkC = 0;
    r->unk10 = 0;
    r->unk14 = 0;
    r->unk18 = 0;
    r->unk1C = 0;
    r->unk20 = 0;
    r->unk22 = 0;
    s = r->unk226;
    for (i = 0; i < 0x200; i++) {
        s->unk0 = 0;
        s->unk2 = 0;
        s++;
    }
    r->unkA28[0].unk0 = 0;
    r->unkA28[0].unk4 = 0;
    r->unkA28[0].unk8 = 0;
    r->unkA28[0].unkC = 0;
    s = r->unkA28[0].unk10;
    for (i = 0; i < 0x100; i++) {
        s->unk0 = 0;
        s->unk2 = 0;
        s++;
    }
    r->unkA28[1].unk0 = 0;
    r->unkA28[1].unk4 = 0;
    r->unkA28[1].unk8 = 0;
    r->unkA28[1].unkC = 0;
    s = r->unkA28[1].unk10;
    for (i = 0; i < 0x100; i++) {
        s->unk0 = 0;
        s->unk2 = 0;
        s++;
    }
}

#if 0
/* NON_MATCHING: the recorder pointer and the new index take a3/a2 the other way round. */
void Prac_RecInput(s32 player, u16 buttons) {
    s32 n;
    CmdRecorder *r = &gPracRecorder[player];

    if (r->unk0 == 0) {
        r->unk20 = buttons & 0xF4F4;
        if (r->unk20 != r->unk22) {
            n = r->unk18 + 1;
            n = n < 0 ? 0x1FF : (n > 0x1FF ? 0 : n);
            r->unk226[n].unk2 = r->unk20;
            r->unk226[n].unk0 = 1;
            r->unk18 = n;
            r->unk1C++;
        } else if (r->unk226[r->unk18].unk0 < 0xFFFF) {
            r->unk226[r->unk18].unk0++;
        }
        r->unk22 = r->unk20;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", Prac_RecInput);
#endif

CmdTrack *Prac_RecGetTrack(s32 player, s32 track) {
    if (track == 0) {
        return &gPracRecorder[player].unkA28[0];
    }
    return &gPracRecorder[player].unkA28[1];
}

void Prac_RecBegin(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];

    r->unk4 = 1;
    r->unk1C = 0;
}

void func_0037C678(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];

    r->unk4 = 2;
}

void func_0037C698(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];

    r->unk4 = 3;
}

void Prac_RecEnd(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];
    s32 mode = r->unk4;

    if (mode == 1) {
        r->unk10 = r->unk18;
    } else if (mode == 2) {
        r->unk14 = r->unk18;
        func_0037C748(player);
    } else if (mode == 3) {
        r->unk14 = r->unk18;
        func_0037C750(player);
    }
    r->unk4 = 0;
}

void func_0037C748(s32 player) {
    Prac_RecSaveTrack(player, 0);
}

void func_0037C750(s32 player) {
    Prac_RecSaveTrack(player, 1);
}

void Prac_RecSaveTrack(s32 player, s32 track) {
    CmdRecorder *r = &gPracRecorder[player];
    CmdStep *h = r->unk226;
    CmdTrack *t = Prac_RecGetTrack(player, track);
    s32 i = r->unk10;
    s32 n = 0;
    CmdStep *d;
    s32 k;

    t->unk10[0] = h[i];
    d = &t->unk10[1];
    if (i != r->unk14) {
        do {
            i++;
            k = i < 0 ? 0x1FF : (i <= 0x1FF ? i : 0);
            n++;
            i = k;
            if (n >= 0x100) {
                break;
            }
            *d++ = h[k];
        } while (k != r->unk14);
    }
    t->unk4 = n;
    t->unk0 = 0;
}

s32 func_0037C828(s32 player) {
    CmdTrack *t = &gPracRecorder[player].unkA28[1];
    s32 r = 0;

    if (t->unk0 != t->unk4 && (t->unk10[t->unk0].unk2 != 0 || t->unk10[t->unk0].unk0 != 0)) {
        r = 1;
    }
    return r;
}

u16 func_0037C878(s32 player) {
    CmdTrack *t = &gPracRecorder[player].unkA28[1];

    return t->unk10[t->unk4].unk0;
}

s32 func_0037C8A0(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];

    return r->unk1C >= 0xFE;
}

void func_0037C8C8(s32 player) {
    Prac_PlayBegin(player, 0);
}

void func_0037C8D0(s32 player) {
    Prac_PlayBegin(player, 1);
}

void Prac_PlayBegin(s32 player, s32 track) {
    CmdRecorder *r = &gPracRecorder[player];
    CmdTrack *t = Prac_RecGetTrack(player, track);

    if (t->unk0 >= 0x100) {
        r->unk0 = 0;
    } else {
        t->unk8 = t->unk0;
        r->unk0 = 1;
        r->unk24 = r->unk26[0];
        r->unk20 = t->unk10[0].unk2;
        t->unkC = t->unk10[0].unk0;
        r->unk22 = r->unk20;
    }
}

u16 func_0037C950(s32 player) {
    return Prac_PlayStep(player, 0);
}

u16 func_0037C970(s32 player) {
    return Prac_PlayStep(player, 1);
}

u16 Prac_PlayStep(s32 player, s32 track) {
    CmdRecorder *r = &gPracRecorder[player];
    CmdTrack *t = Prac_RecGetTrack(player, track);
    s32 step = t->unk8;
    u16 ret;

    if (r->unk0 == 0) {
        return 0;
    }
    if (step == t->unk4) {
        r->unk0 = 0;
        return 0;
    }
    if (t->unkC > 0) {
        ret = t->unk10[step].unk2;
        t->unkC--;
    } else {
        step++;
        t->unk8 = step;
        if (step == t->unk4) {
            r->unk0 = 0;
            return 0;
        }
        r->unk24 += r->unk26[step];
        ret = t->unk10[step].unk2;
        t->unkC = t->unk10[step].unk0 - 1;
    }
    return ret;
}

s32 Prac_IsPlaying(s32 player) {
    return gPracRecorder[player].unk0;
}

u16 func_0037CA78(s32 player) {
    return gPracRecorder[player].unk24;
}

void func_0037CA98(s32 player) {
    gPracRecorder[player & 1].unk0 = 0;
}

void func_0037CAB8(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];

    r->unk8 = r->unk18;
}

void func_0037CAD8(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];

    r->unkC = r->unk18;
}

void func_0037CAF8(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];

    r->unk8 = r->unkC;
}

void func_0037CB18(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];
    s32 i = r->unk8;
    s32 n = 0;
    u16 b;
    char buf[16];
    CmdStep *h = r->unk226;

    do {
        b = h[i].unk2;
        buf[0] = func_0036CFC0(b) + '0';
        buf[1] = (b & 0x80) ? 'A' : ' ';
        buf[2] = (b & 0x10) ? 'B' : ' ';
        buf[3] = (b & 0x40) ? 'C' : ' ';
        buf[4] = (b & 0x20) ? 'D' : ' ';
        buf[5] = (b & 4) ? 'E' : ' ';
        buf[6] = 0;
        if (i == r->unkC) {
            break;
        }
        i++;
        i = i < 0 ? 0x1FF : (i > 0x1FF ? 0 : i);
        n++;
    } while (n < 0x10);
}

void func_0037CC58(s32 player) {
    func_0037CC60(player);
}

#if 0
/* NON_MATCHING: the original keeps `gPracRecorder + 0x226` and the player offset apart; ours folds them, and the registers follow. */
void func_0037CC60(s32 player) {
    CmdRecorder *r = &gPracRecorder[player];
    s32 i = r->unk18;
    s32 n = 0;
    u16 b;
    char buf[16];
    CmdStep *h = r->unk226;

    do {
        b = h[i].unk2;
        buf[0] = func_0036CFC0(b) + '0';
        buf[1] = (b & 0x80) ? 'A' : ' ';
        buf[2] = (b & 0x10) ? 'B' : ' ';
        buf[3] = (b & 0x40) ? 'C' : ' ';
        buf[4] = (b & 0x20) ? 'D' : ' ';
        buf[5] = (b & 4) ? 'E' : ' ';
        buf[6] = 0;
        i--;
        i = i < 0 ? 0x1FF : (i > 0x1FF ? 0 : i);
        n++;
    } while (n < 0x10);
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037CC60);
#endif

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037CDA0);

void func_0037CFA0(void) {
}

void func_0037CFA8(void) {
    CmdList *p = gPracCmdLists;
    CmdListEntry *e;
    CmdListStep *s;
    s32 i, j, k;

    for (i = 0; i < 4; i++) {
        p->unk0 = 0;
        p->unk4 = 0;
        p->unk3E90 = 0;
        for (j = 0; j < 0x10; j++) {
            p->unk3E94[j] = 0;
        }
        p->unk3ED4 = 0;
        for (j = 0; j < 0x80; j++) {
            p->unk3ED8[j] = 0;
        }
        e = p->unk8;
        for (j = 0; j < 0x7B; j++) {
            e->unk0 = 0;
            e->unk2 = 0;
            e->unk4 = 0;
            e->unk6 = 0;
            e->unk8 = 0;
            e->unkA = 0;
            e->unkC = 0;
            e->unkE = 0;
            e->unk10 = 0;
            for (k = 0; k < 8; k++) {
                e->unk12[k] = 0;
            }
            for (k = 0; k < 0x10; k++) {
                e->unk22[k] = 0;
            }
            e++;
        }
        s = p->unk1FBE;
        for (j = 0; j < 0x3DA; j++) {
            s->unk0 = 0;
            s->unk2 = 0;
            s->unk4 = 0;
            s->unk6 = 0;
            s++;
        }
        p++;
    }
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037D0F8);

s32 func_0037D380(Fighter *f) {
    return gPracCmdLists[f->unk22].unk3ED4;
}

s32 func_0037D3A0(Fighter *f, s32 i) {
    i = i < 0 ? 0 : (i < 0x80 ? i : 0x7F);
    return gPracCmdLists[f->unk22].unk3ED8[i];
}

void func_0037D3E0(Fighter *f, s32 i, s32 *out) {
    CmdList *p = &gPracCmdLists[f->unk22];
    s32 k = 0;
    CmdListEntry *e = &p->unk8[func_0037D3A0(f, i)];

    do {
        *out++ = e->unk22[k];
        k++;
    } while (k < 0x10);
}

CmdList *func_0037D460(s32 i) {
    return &gPracCmdLists[i];
}

u32 func_0037D478(Fighter *f) {
    return gPracCmdLists[f->unk22].unk0;
}

u16 func_0037D498(Fighter *f, s32 i) {
    return gPracCmdLists[f->unk22].unk8[i].unk0;
}

u16 func_0037D4C8(Fighter *f, s32 i) {
    return gPracCmdLists[f->unk22].unk8[i].unk4;
}

s32 func_0037D4F8(Fighter *f) {
    return gPracCmdLists[f->unk22].unk3E90;
}

s32 func_0037D518(Fighter *f, u32 i) {
    if (i >= 0x10) {
        return 0;
    }
    return gPracCmdLists[f->unk22].unk3E94[i];
}

#if 0
/* NON_MATCHING: same instructions, but the loads of the entry fields and the gPracReset stores are scheduled in another order. */
void Prac_LoadCmdIntoTrack(Fighter *f, s32 i) {
    CmdList *p = &gPracCmdLists[f->unk22];
    CmdListEntry *e = &p->unk8[i];
    CmdRecorder *r = &gPracRecorder[f->unk1E];
    CmdListStep *s = &p->unk1FBE[e->unk2];
    CmdStep *d = r->unkA28[0].unk10;
    u16 *t = r->unk26;
    s32 n = 0;

    gPracReset.unk14 = e->unk10;
    gPracReset.unk8 = e->unkA;
    gPracReset.unkC = e->unkC;
    gPracReset.unk10 = e->unkE;
    if (e->unk0 != 0) {
        do {
            n++;
            d->unk2 = s->unk2;
            d->unk0 = s->unk0;
            *t = s->unk4;
            s++;
            d++;
            t++;
        } while (n < e->unk0);
    }
    r->unkA28[0].unk0 = 0;
    r->unkA28[0].unk4 = e->unk0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", Prac_LoadCmdIntoTrack);
#endif

void func_0037D630(void) {
    Unk8B7EF0 *p = D_008B7EF0;
    s32 i;

    for (i = 0; i < 2; i++) {
        p->unk0 = 0;
        p->unk2 = 0;
        p->unk6 = 0;
        p->unk4 = 0;
        p++;
    }
}

#if 0
/* NON_MATCHING: the original switch tests `< 2` between the two cases (slti / bnez); ours compares each case only. */
void func_0037D668(Fighter *f) {
    Unk8B7EF0 *p = &D_008B7EF0[f->unk1E];

    switch (p->unk0) {
        case 1:
            func_0037C8C8(f->unk1E);
            f->unk420 = 3;
            p->unk0 = 2;
        case 2:
            f->unk420 = 3;
            if (Prac_IsPlaying(f->unk1E) == 0) {
                p->unk0 = 0;
            }
            break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037D668);
#endif

#if 0
/* NON_MATCHING: the shared `return 0` block sits after the first test in the original (bnezl), at the end in ours. */
s32 func_0037D710(Fighter *f, s32 i) {
    Unk8B7EF0 *p = &D_008B7EF0[f->unk1E];

    if (i < func_0037D478(f)) {
        if (func_0037D498(f, i) == 0) {
            return 0;
        }
        p->unk6 = i;
        Prac_LoadCmdIntoTrack(f, (s16)i);
        return 1;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037D710);
#endif

s32 func_0037D798(Fighter *f, s32 i) {
    Unk8B7EF0 *p = &D_008B7EF0[f->unk1E];

    if (i >= func_0037D478(f) || func_0037D498(f, i) == 0) {
        return 0;
    }
    p->unk6 = i;
    p->unk0 = 1;
    Prac_LoadCmdIntoTrack(f, (s16)i);
    return 1;
}

void func_0037D828(Fighter *f) {
    D_008B7EF0[f->unk1E].unk0 = 0;
}

void func_0037D848(void) {
    gPracReset.unk0 = 0;
    gPracReset.unk4 = 0;
    gPracReset.unk8 = 0;
    gPracReset.unkC = 0;
    gPracReset.unk10 = 0x4B0;
    gPracReset.unk14 = 0;
}

void func_0037D870(void) {
    gPracReset.unk4 = 0;
}

s32 func_0037D880(void) {
    return gPracReset.unk4;
}

void func_0037D890(void) {
    D_005F60A0 = 0;
}

s32 func_0037D898(void) {
    if (D_0043B7F8[0] == 7) {
        return D_005F60A0;
    }
    return 0;
}

void func_0037D8B8(void) {
    D_005F60A0 = 1;
}

void Prac_ResetPositions(Fighter *f) {
    Fighter *o = &gFighters[(f->unk1E + 1) & 1];
    s32 dist = gPracReset.unk10;
    s32 half;

    gPracReset.unk4 = 1;
    func_00368818(f, o, 0x3A);
    func_003730E8(f, o, 0);
    f->unk3A = 0x4000;
    half = dist / 2;
    f->unk4 = f->unk51C = -half;
    func_00368818(o, f, 0x3A);
    func_003730E8(o, f, 1);
    o->unk4 = o->unk51C = half;
    o->unk3A = -0x4000;
    func_00373390(f, 4);
    func_00373390(o, 4);
    func_0037DA08(f, gPracReset.unk8);
    func_0037DA08(o, gPracReset.unkC);
    func_0035B9A8(f, o);
    func_0038C0F0();
    func_00390E08(f, o);
    func_0037D8B8();
    func_0034CB38(0);
    func_0034CB38(1);
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037DA08);

s32 func_0037DBE8(void) {
    return func_0037DC38(gPracReset.unk8);
}

s32 func_0037DC10(void) {
    return func_0037DC38(gPracReset.unkC);
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037DC38);

s32 func_0037DC88(void) {
    return gPracReset.unk14;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037DC98);

s32 func_0037DDD0(Fighter *f) {
    f->unk430 = 0;
    return 0;
}

s32 func_0037DDE0(Fighter *f) {
    if (Fighter_GetMove(f)->unk4 & 0x204) {
        f->unk430 = 0x1000;
    } else {
        f->unk430 = 0;
    }
    return f->unk430 == 0;
}

s32 func_0037DE40(Fighter *f) {
    u16 r;

    f->unk430 = 0;
    if (Fighter_GetMove(f)->unk4 & 0x204) {
        r = 0x1000;
    } else if (f->unk42 != 0) {
        r = 0x8000;
    } else if (D_005F5C28 > 0xC80) {
        r = 0x2000;
    } else if (f->unk28 == 6 && f->unk10C == 0xA28) {
        r = 0x60;
    } else {
        goto end;
    }
    f->unk430 = r;
end:
    return f->unk430 == 0;
}

#if 0
/* NON_MATCHING: the original places the func_00368218 call block at the end of the function and branches back; ours keeps it inline. */
s32 func_0037DEE0(Fighter *f) {
    Fighter *o = &gFighters[(f->unk1E + 1) & 1];

    if (!(Fighter_GetMove(f)->unk4 & 0x204) && f->unk42 == 0 && D_005F5C28 <= 0xC80) {
        if (o->unkD0 != 0x412 || o->unkC0->unk2D < o->unkC4) {
            if (f->unk10C != 0x3A) {
                return (f->unkC0->unk24 & 0xC0) != 0;
            }
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037DEE0);
#endif

#if 0
/* NON_MATCHING: the original has a nop that aligns the second return to 8 bytes; not reproduced. */
s32 func_0037DFC8(Fighter *f) {
    if (f->unk12F != 0) {
        return 1;
    }
    return f->unkF8 == 3;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037DFC8);
#endif

s32 func_0037DFF0(Fighter *f) {
    f->unk430 = 0x4000;
    return f->unk10C == f->unkC0->unk10;
}

s32 func_0037E010(Fighter *f, Fighter *o) {
    if ((o->unkD0 & 0x10) && o->unkC0->unk2D >= o->unkC4) {
        f->unk430 = 0x8000;
        return 1;
    }
    return 0;
}

s32 func_0037E050(Fighter *f, Fighter *o) {
    if ((o->unkD0 & 8) && o->unkC0->unk2D >= o->unkC4) {
        f->unk430 = 0xC000;
        return 1;
    }
    return 0;
}

s32 func_0037E090(Fighter *f, Fighter *o) {
    if (o->unkC0->unk2D >= o->unkC4) {
        if (o->unkD0 & 8) {
            f->unk430 = 0xC000;
            return 1;
        }
        if (o->unkD0 & 0x10) {
            f->unk430 = 0x8000;
            return 1;
        }
        return 0;
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037E0E8);

s32 func_0037E180(Fighter *f, Fighter *o) {
    f->unk430 = 0x4000;
    if ((o->unkD0 & 8) && o->unkC0->unk2D >= o->unkC4) {
        f->unk430 = 0xC000;
        return 1;
    }
    return f->unk10C == f->unkC0->unk10;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037E1D8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037E468);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037E670);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EA08);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EA58);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EB00);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EE18);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EE68);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EEF8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EF50);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EF60);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EFB0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FA30);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FA50);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FA78);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FB30);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FB70);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FE58);

s32 func_003800D8(s32 n) {
    s32 r = n * ((rand() + D_005F6C08) & 0x7FFF);

    D_005F6C08 = D_005F6C08 * 5 + 3;
    return r >> 15;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380120);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380420);

void func_00380528(s32 arg0, s32 arg1) {
    gCpu->unk4C = arg0;
    gCpu->unk50 = arg1;
    gCpu->unk5C = arg0 + 0x3C;
}

void func_00380540(s32 arg0) {
    gCpu->unk50 = 0;
    gCpu->unk4C = arg0;
    gCpu->unk5C = arg0 + 0x28;
}

void func_00380558(void) {
}

void func_00380560(Fighter *f) {
    func_00380568(f);
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380568);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003809C0);

void func_00380A50(Fighter *f, u16 *held, u16 *pressed) {
    Cpu_GetInput(f, held, pressed);
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", Cpu_GetInput);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380B10);

INCLUDE_ASM("asm/nonmatchings/fighter_core", Cpu_Think);

INCLUDE_ASM("asm/nonmatchings/fighter_core", Cpu_SetCommand);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381278);

s32 func_00381408(void) {
    return gCpuSelf->unk188 != NULL;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381418);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003814C8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381818);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381B10);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381BF8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381D30);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00382AD8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003835A0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003838B0);

s32 func_00383B88(struct FighterUnkC0 *m, s32 arg1, s32 arg2) {
    s32 fl = m->unk8 & 0xFFFF0000;

    if (!(fl & 0x1C0000)) {
        fl |= 0xC0000;
    }
    return (fl & D_004318D8[arg2][arg1]) != 0;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00383BD8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00383D10);

s32 Cpu_PickCands(s32 (*filter)(CpuCand *)) {
    CpuCand *c = gCpuCands;
    s32 i = 0;
    s32 n = 0;

    for (; i < gCpu->unkAC; i++, c++) {
        if (c->unk0 == 0 && filter(c) != 0) {
            c->unk0 = 1;
            n++;
        }
    }
    gCpu->unkB0 += n;
    return n;
}

#if 0
/* NON_MATCHING: the operands of `addu` for `n += old > 0` are swapped. */
s32 Cpu_RejectCands(s32 (*filter)(CpuCand *)) {
    CpuCand *c = gCpuCands;
    s32 i = 0;
    s32 n = 0;

    for (; i < gCpu->unkAC; i++, c++) {
        if (c->unk0 >= 0 && filter(c) != 0) {
            s16 old = c->unk0;

            c->unk0 = -1;
            n = (old > 0) + n;
        }
    }
    gCpu->unkB0 -= n;
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", Cpu_RejectCands);
#endif

s32 func_00383ED8(CpuCand *c) {
    return ((c->unk8->unk0 >> 14) ^ 1) & 1;
}

s32 func_00383EF0(CpuCand *c) {
    return c->unk4->unk24 & 0x200;
}

s32 func_00383F00(CpuCand *c) {
    return c->unk4->unk24 & 0x20;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00383F10);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384038);

s32 func_003840F8(CpuCand *c) {
    s32 fl = c->unk4->unk8;
    s32 r = 0;

    if ((fl & 0xFFFF) == 0x10F || (fl & 0x10000)) {
        r = 1;
    }
    return r;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384130);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384210);

s32 func_00384338(CpuCand *c) {
    struct FighterUnkC0 *m = c->unk4;
    s32 fl = m->unk4;
    s32 r = 0;

    if ((fl & 0x80000) && m->unk19 == 0) {
        if (!(fl & 0x20000)) {
            r = 1;
        }
    }
    return r;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384370);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384408);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003844B0);

#if 0
/* NON_MATCHING: the result lives in a2 instead of v0, and the original has a load-delay nop. */
s32 func_00384578(CpuCand *c) {
    struct FighterUnkC0 *m;
    s32 fl;

    if (c->unk8->unk0 != 0x8001) {
        m = c->unk4;
        fl = m->unk4;
        if ((fl & 0x80000) && !(m->unk8 & 0x600000)) {
            if (!(fl & 0x20000)) {
                return 1;
            }
        }
        return 0;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384578);
#endif

s32 func_003845D8(CpuCand *c) {
    s32 r = 0;

    if (c->unk4->unk8 & 0x400000) {
        r = (c->unk8->unk0 & 0xBFE0) == 0;
    }
    return r;
}

s32 func_00384610(CpuCand *c) {
    s32 r = 0;

    if (c->unk4->unk8 & 0x400000) {
        r = (c->unk8->unk0 & 0x8200) != 0x200;
    }
    return r;
}

s32 func_00384648(CpuCand *c) {
    s32 fl = c->unk4->unk8;
    s32 r = 0;

    if ((fl & 0x10000) || (fl & 0xFFFF) == 0x51F) {
        if (!(fl & 0x400000)) {
            r = 1;
        }
    }
    return r;
}

s32 func_00384688(CpuCand *c) {
    s32 fl = c->unk4->unk8;
    s32 r = 0;

    if ((fl & 0x10000) || (fl & 0xFFFF) == 0x51F || (fl & 0xFFFF) == 0x10F) {
        if (!(fl & 0x400000)) {
            r = 1;
        }
    }
    return r;
}

s32 func_003846D0(CpuCand *c) {
    struct FighterUnkC0 *m = c->unk4;
    s32 r = 0;

    if ((m->unkC[3] & 0x7F) == 0x38) {
        if (!(m->unk8 & 0x400000)) {
            r = 1;
        }
    }
    return r;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384708);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003847D8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384898);

#if 0
/* NON_MATCHING: the original loads both pointers first and masks the flags with `andi 0xFFFF`; ours uses lhu. */
s32 func_003848F0(CpuCand *c) {
    return c->unk8->unk0 != 0x8002 && (c->unk4->unk4 & 0xFFFF) == 0x1052;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003848F0);
#endif

#if 0
/* NON_MATCHING: the original keeps the flag bit in a register and clears it with movn; ours folds it to sltiu. */
s32 func_00384920(CpuCand *c) {
    struct FighterUnkC0 *m = c->unk4;
    s32 b = m->unk4 & 1;
    s32 r = 0;

    if (b == 1) {
        r = m->unk2C != 0 ? 0 : b;
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384920);
#endif

s32 func_00384950(CpuCand *c) {
    struct FighterUnkC0 *m = c->unk4;
    s32 r = 0;

    if ((m->unk8 & 0x10000) && m->unk19 == 0) {
        r = m->unk2C == 0;
    }
    return r;
}

s32 func_00384988(CpuCand *c) {
    return c->unk8->unk0 == 0x8001;
}

s32 func_003849A0(CpuCand *c) {
    return c->unk8->unk0 == 0x8002;
}

s32 func_003849B8(CpuCand *c) {
    return c->unk4->unk4 & 0x20000;
}

s32 func_003849D0(CpuCand *c) {
    u16 id = c->unk8->unk6;
    const s16 *t = D_00507860;
    u32 i;

    for (i = 0; i < 2; i++) {
        if (id == *t) {
            return 1;
        }
        t++;
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384A10);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384A50);

s32 func_00384AB8(CpuCand *c) {
    struct FighterUnkC0 *m = c->unk4;
    s32 fl;
    s32 r = 0;

    if (m->unk19 != 0) {
        fl = m->unk4;
        if (!(fl & 0x20000) && ((fl & 0x80000) || (m->unk8 & 0x200000))) {
            r = 1;
        }
    }
    return r;
}

s32 func_00384B08(CpuCand *c) {
    struct FighterUnkC0 *m = c->unk4;
    s32 fl;
    s32 r = 0;

    if (m->unk2C != 0 && (((fl = m->unk4) & 0x80000) || (m->unk8 & 0x200000))) {
        if (!(fl & 0x20000)) {
            r = 1;
        }
    }
    return r;
}

s32 func_00384B50(CpuCand *c) {
    return (u32)(c->unk8->unk8 - 0x1C) < 2;
}

s32 func_00384B68(CpuCand *c) {
    return c->unk8->unk8 == 0x1E;
}

s32 func_00384B80(CpuCand *c) {
    CpuCmd *k = c->unk8;
    s32 r = 0;

    if ((k->unk0 & 0x4000) || (k->unk8 & 0x2000)) {
        r = 1;
    }
    return r;
}

s32 func_00384BB0(CpuCand *c) {
    CpuCmd *k = c->unk8;
    s32 r = 0;

    if ((k->unk0 & 0x4000) || (k->unk8 & 0x2000)) {
        if (!(c->unk4->unk8 & 0x400000)) {
            r = 1;
        }
    }
    return r;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384BF0);

s32 func_00384C58(CpuCand *c) {
    return (u32)(c->unk8->unk8 - 0x1A) < 2;
}

s32 func_00384C70(CpuCand *c) {
    u16 fl = c->unk8->unk8;
    s32 r = 0;

    if (fl & 0x1F80) {
        if (fl & 0x2000) {
            r = 1;
        }
    }
    return r;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384C98);

s32 func_00384D68(u16 *cmd) {
    return *cmd & 0x4000;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384D78);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003850B8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385178);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385250);

u16 func_00385310(s32 x) {
    return x;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385318);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003853E8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003854B0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003856C0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003857F8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385A60);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385C80);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385DF8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00386388);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00386428);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00386470);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003864E8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003865A0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00387120);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00387518);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00387600);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003879A0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00387DA8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388170);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388278);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388530);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388840);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388D70);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388DA8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388DE0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388F78);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388FA8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00389488);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00389B50);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00389B70);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00389D58);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A408);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A500);

#if 0
/* NON_MATCHING: the constants 0x28 and -1 are loaded and stored in the other order. */
s32 func_0038A5F8(void) {
    gCpu->unk4C = 0;
    gCpu->unk50 = 0;
    gCpu->unk5C = 0x28;
    gCpu->unk6C = -1;
    return ((gCpu->unk4 & 1) ^ gCpu->unk0) == 0 ? 0 : 0x2000;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A5F8);
#endif

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A630);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A690);

void func_0038A748(void) {
    gCpu->unkBC = func_003840F8;
}

void func_0038A760(void) {
    Cpu_SetCommand((u16 *)D_005074A2);
}

void func_0038A770(void) {
    Cpu_SetCommand((u16 *)D_0050748A);
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A780);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A928);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038ABE8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AC40);

#if 0
/* NON_MATCHING: the original copies the move pointer to a second register and pads the loop with a nop. */
s32 func_0038ACA8(void) {
    struct FighterUnkC0 *m = gCpuOpponent->unkC0;
    s32 n = 0;
    s32 i;

    if (m->unk2C != 0) {
        for (i = 0; i < 0x14; i++) {
            if (gCpu->unk2D0[i] == m) {
                n++;
            }
        }
    }
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038ACA8);
#endif

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AD00);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038ADF8);

s32 func_0038AEA0(Fighter *f) {
    return f->unk10C == 0x3D;
}

s32 func_0038AEB0(s32 n) {
    return n >= gCpu->unk20 ? -1 : 0x2000;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AED0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AF28);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AFC0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038B368);

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B690(void) {
    return -1;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038B698);

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B6E8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B6F0(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B6F8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B700(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B708(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B710(void) {
    return -1;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038B718);

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B7D8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B7E0(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B7E8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B7F0(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B7F8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B800(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B808(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B810(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B818(void) {
    return -1;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038B820);

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBA8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBB0(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBB8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBC0(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBC8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBD0(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBD8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBE0(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBE8(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBF0(void) {
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BBF8(void) {
    return -1;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038BC00);

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BE70(void) {
    return -1;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038BE78);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038BEE0);

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BF38(void) {
    return -1;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038BF40);
