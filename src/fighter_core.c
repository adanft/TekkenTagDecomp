/* 0x37BA88-0x38BF98 (module fighter_core, docs/game_overview.md section 4; docs/systems/fighter.md). Despite the
 * module name this is not the move interpreter or hit detection: 0x37BA88-0x3800D8 is the practice mode support
 * (cursor sounds, hit markers, the command recorder gPracRecorder and its playback, the command lists
 * gPracCmdLists, guard / action conditions, the practice reset), and 0x3800D8-0x38BF98 is the CPU opponent
 * (Cpu_GetInput: one call per frame per CPU side, a candidate list of moves filtered by predicates, scripted
 * button commands, 35 move handlers of the table D_00507A80). No .lit4 or .sdata of its own is emitted.
 * .rodata 0x5064C0-0x507EE0 is this file's subsegment and every table, string and jump table in it is included in
 * place with INCLUDE_RODATA (force_not_migration in data.txt), except the jump tables of functions that are C. The
 * range is not one object's: 0x506528-0x506BF0 (the practice menu strings of the .data table 0x42F140-0x42F268 and
 * four tables read only by the practice functions 0x3790C8-0x37B4F8 of asm/3A0C8.s) sits between the constants of
 * func_0037BB70-func_0037BF48 and those of func_0037CDA0, so that object ends between 0x37BF48 and 0x37C370 and
 * starts before 0x37BA88. The symbols at odd addresses (D_00507452 ...) are inside the file of the symbol before. */
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
extern u16 D_005F5CBC; /* a game mode flag tested by the CPU and the round flow */

void func_0034CB38(s32 arg0);
void func_0035B9A8(Fighter *a, Fighter *b);
struct FighterMove *func_00368218(Fighter *f, s16 move);
void func_00368818(Fighter *a, Fighter *b, s32 arg2);
void func_003730E8(Fighter *a, Fighter *b, s32 arg2);
void func_00373390(Fighter *f, s32 arg1);
void func_0037DA08(Fighter *f, s32 state);
s32 func_0037DC38(s32 state);
void Cam_Reset(void);
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
    /* 0x2 */ u8 unk2;  /* condition (Cmd_CheckCondition) */
    /* 0x3 */ u8 unk3;
    /* 0x4 */ s16 unk4;
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
    /* 0x04 */ struct FighterMove *unk4;
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
    /* 0x010 */ struct FighterMove **unk10; /* move records of the CPU fighter by id (func_003835A0) */
    /* 0x014 */ s32 unk14;
    /* 0x018 */ s32 unk18;
    /* 0x01C */ s32 unk1C;
    /* 0x020 */ s32 unk20;
    /* 0x024 */ s32 unk24;
    /* 0x028 */ s32 unk28;
    /* 0x02C */ s32 unk2C;
    /* 0x030 */ s32 unk30;
    /* 0x034 */ s32 unk34;
    /* 0x038 */ s32 unk38;
    /* 0x03C */ s32 unk3C;
    /* 0x040 */ s32 unk40;
    /* 0x044 */ s32 unk44;                     /* frame of the own move last seen (Cpu_Think) */
    /* 0x048 */ s32 unk48;
    /* 0x04C */ s32 unk4C;
    /* 0x050 */ s32 unk50;
    /* 0x054 */ s32 unk54;
    /* 0x058 */ s32 unk58;
    /* 0x05C */ s32 unk5C;
    /* 0x060 */ s32 unk60;
    /* 0x064 */ s32 unk64;
    /* 0x068 */ s32 unk68;
    /* 0x06C */ s32 unk6C;
    /* 0x070 */ s32 unk70;
    /* 0x074 */ s32 unk74;
    /* 0x078 */ s32 unk78;
    /* 0x07C */ s32 unk7C;
    /* 0x080 */ s32 unk80;
    /* 0x084 */ s32 unk84;
    /* 0x088 */ struct FighterMove *unk88;     /* own move when the candidates were listed */
    /* 0x08C */ struct FighterMove *unk8C;     /* move of the command started (func_00381278), cleared each frame */
    /* 0x090 */ struct FighterMove *unk90;     /* skipped once by func_00382AD8 */
    /* 0x094 */ s32 unk94;
    /* 0x098 */ s32 unk98;
    /* 0x09C */ s32 unk9C;
    /* 0x0A0 */ s32 unkA0;
    /* 0x0A4 */ s32 unkA4;
    /* 0x0A8 */ struct FighterMove *unkA8;
    /* 0x0AC */ s32 unkAC; /* candidate count */
    /* 0x0B0 */ s32 unkB0; /* candidates picked */
    /* 0x0B4 */ s32 (*unkB4)();               /* answer hook for the opponent's character (func_0038BEE0) */
    /* 0x0B8 */ s32 (*unkB8)();               /* hook for the CPU's own character (func_0038BEE0) */
    /* 0x0BC */ s32 (*unkBC)(CpuCand *c);
    /* 0x0C0 */ s32 unkC0;
    /* 0x0C4 */ s32 unkC4;
    /* 0x0C8 */ s32 unkC8;
    /* 0x0CC */ s32 unkCC;                    /* the hooks run only while negative (func_0038A408) */
    /* 0x0D0 */ s32 unkD0;
    /* 0x0D4 */ s32 unkD4;
    /* 0x0D8 */ s32 unkD8;
    /* 0x0DC */ s32 unkDC;
    /* 0x0E0 */ s32 unkE0;
    /* 0x0E4 */ s32 unkE4;
    /* 0x0E8 */ s32 unkE8;
    /* 0x0EC */ s32 unkEC;
    /* 0x0F0 */ s32 unkF0;
    /* 0x0F4 */ s32 unkF4;
    /* 0x0F8 */ s32 unkF8;
    /* 0x0FC */ s32 unkFC;                    /* moves with flag 0x400000 picked (func_00385250) */
    /* 0x100 */ s32 unk100;
    /* 0x104 */ s32 unk104;
    /* 0x108 */ struct FighterMove *unk108;    /* own move when the last answer to a hit was chosen (func_00381BF8) */
    /* 0x10C */ s32 unk10C;
    /* 0x110 */ s32 unk110;
    /* 0x114 */ s32 unk114;
    /* 0x118 */ s32 unk118;
    /* 0x11C */ s32 unk11C;
    /* 0x120 */ s32 unk120;
    /* 0x124 */ s32 unk124;                    /* frame of the own move to play unk128 at, -1 none */
    /* 0x128 */ CpuCmd *unk128;                /* command chosen by func_00381B10 */
    /* 0x12C */ s32 unk12C;
    /* 0x130 */ s32 unk130;
    /* 0x134 */ MoveCmd *unk134[4];                /* commands listed by func_003856C0 (unk130 of them) */
    /* 0x144 */ s32 unk144;
    /* 0x148 */ s32 unk148;
    /* 0x14C */ s32 unk14C;
    /* 0x150 */ s32 unk150;
    /* 0x154 */ CpuCmd *unk154[0x30];             /* commands listed by func_003857F8 (unk150 of them) */
    /* 0x214 */ s32 unk214;
    /* 0x218 */ s32 unk218;
    /* 0x21C */ s32 unk21C;
    /* 0x220 */ s32 unk220;
    /* 0x224 */ s32 unk224;
    /* 0x228 */ CpuCmd *unk228[10];             /* commands listed by func_00385A60 (unk224 of them) */
    /* 0x250 */ s32 unk250;
    /* 0x254 */ s32 unk254;
    /* 0x258 */ s32 unk258;
    /* 0x25C */ s32 unk25C;
    /* 0x260 */ s32 unk260;
    /* 0x264 */ CpuCmd *unk264;                /* command planned by func_0038A928 */
    /* 0x268 */ s32 unk268;
    /* 0x26C */ s32 unk26C;
    /* 0x270 */ s32 unk270;
    /* 0x274 */ s32 unk274;
    /* 0x278 */ s32 unk278;
    /* 0x27C */ s32 unk27C[2];                /* per team member, kept at most its maximum life (func_0038ADF8) */
    /* 0x284 */ s32 unk284;
    /* 0x288 */ s32 unk288[4];                 /* the opponent's moves when they took life (func_00380B10, newest
                                                * first); [3] also holds the flag of character 0xE's moves 0xBA9 /
                                                * 0x1A6 */
    /* 0x298 */ s32 unk298;
    /* 0x29C */ s32 unk29C;
    /* 0x2A0 */ s32 unk2A0;
    /* 0x2A4 */ s32 unk2A4;
    /* 0x2A8 */ s32 unk2A8;
    /* 0x2AC */ s32 unk2AC;
    /* 0x2B0 */ s32 unk2B0;
    /* 0x2B4 */ s32 unk2B4;
    /* 0x2B8 */ s32 unk2B8;
    /* 0x2BC */ s32 unk2BC;
    /* 0x2C0 */ s32 unk2C0;
    /* 0x2C4 */ s32 unk2C4;
    /* 0x2C8 */ s32 unk2C8;
    /* 0x2CC */ s32 unk2CC;
    /* 0x2D0 */ struct FighterMove *unk2D0[0x14];
    /* 0x320 */ s32 unk320;
    /* 0x324 */ s32 unk324;
    /* 0x328 */ s32 unk328;
    /* 0x32C */ u32 unk32C;                    /* own move: flags unk8 (Cpu_Think, each frame) */
    /* 0x330 */ union {
        u32 w;
        u16 h;
    } unk330;                                  /* own move: flags unk4 */
    /* 0x334 */ s32 unk334;                    /* own move: low half of unk8 */
    /* 0x338 */ union {
        u32 w;
        u16 h;
    } unk338;                                  /* the opponent's move: flags unk8 */
    /* 0x33C */ union {
        u32 w;
        u16 h;
    } unk33C;                                  /* the opponent's move: flags unk4 */
    /* 0x340 */ u32 unk340;                /* attack level of the move to answer [I] */
    /* 0x344 */ u8 unk344[0x8];                /* parameter block of 0x80 bytes copied from D_00431918 (func_00380420), to 0x3C4 [I] */
    /* 0x34C */ s16 unk34C[5];                 /* base delay and four random additions (func_00385250) [I] */
    /* 0x356 */ s16 unk356;                    /* limit of unkFC (func_00381B10) */
    /* 0x358 */ s16 unk358;
    /* 0x35A */ s16 unk35A;                    /* chance in 4096 to answer when hit (func_00381BF8) */
    /* 0x35C */ s16 unk35C;
    /* 0x35E */ s16 unk35E;                    /* chance in 4096 to try the predicate func_00384610 first */
    /* 0x360 */ s16 unk360;
    /* 0x362 */ s16 unk362;
    /* 0x364 */ s16 unk364;
    /* 0x366 */ s16 unk366;
    /* 0x368 */ s16 unk368;
    /* 0x36A */ s16 unk36A;
    /* 0x36C */ s16 unk36C;
    /* 0x36E */ s16 unk36E;
    /* 0x370 */ s16 unk370;
    /* 0x372 */ s16 unk372;
    /* 0x374 */ s16 unk374;
    /* 0x376 */ s16 unk376;
    /* 0x378 */ s16 unk378;
    /* 0x37A */ s16 unk37A;
    /* 0x37C */ s16 unk37C;
    /* 0x37E */ s16 unk37E;
    /* 0x380 */ s16 unk380;
    /* 0x382 */ s16 unk382;                   /* chance in 4096 to run a hook (func_0038A408) */
    /* 0x384 */ s16 unk384;
    /* 0x386 */ s16 unk386;
    /* 0x388 */ s16 unk388;
    /* 0x38A */ s16 unk38A;
    /* 0x38C */ s16 unk38C;
    /* 0x38E */ s16 unk38E;
    /* 0x390 */ s16 unk390;
    /* 0x392 */ s16 unk392;
    /* 0x394 */ s16 unk394;
    /* 0x396 */ s16 unk396;
    /* 0x398 */ s16 unk398;
    /* 0x39A */ s16 unk39A;
    /* 0x39C */ s16 unk39C;
    /* 0x39E */ s16 unk39E;
    /* 0x3A0 */ s16 unk3A0;
    /* 0x3A2 */ s16 unk3A2;
    /* 0x3A4 */ s16 unk3A4;
    /* 0x3A6 */ s16 unk3A6;
    /* 0x3A8 */ s16 unk3A8;
    /* 0x3AA */ s16 unk3AA;
    /* 0x3AC */ s16 unk3AC;
    /* 0x3AE */ s16 unk3AE;
    /* 0x3B0 */ s16 unk3B0;
    /* 0x3B2 */ s16 unk3B2;
    /* 0x3B4 */ s16 unk3B4;
    /* 0x3B6 */ s16 unk3B6;
    /* 0x3B8 */ s16 unk3B8;
    /* 0x3BA */ s16 unk3BA;
    /* 0x3BC */ s16 unk3BC;
    /* 0x3BE */ s16 unk3BE;
    /* 0x3C0 */ s16 unk3C0;
    /* 0x3C2 */ s16 unk3C2;
    /* 0x3C4 */ s16 unk3C4;
    /* 0x3C6 */ s16 unk3C6;
    /* 0x3C8 */ s16 unk3C8;
    /* 0x3CA */ s16 unk3CA;
    /* 0x3CC */ s16 unk3CC;
    /* 0x3CE */ s16 unk3CE;
    /* 0x3D0 */ s16 unk3D0;
    /* 0x3D2 */ s16 unk3D2;
    /* 0x3D4 */ s16 unk3D4;
    /* 0x3D6 */ s16 unk3D6;
    /* 0x3D8 */ s16 unk3D8;
    /* 0x3DA */ s16 unk3DA;
    /* 0x3DC */ s32 unk3DC;
    /* 0x3E0 */ s32 unk3E0;
    /* 0x3E4 */ s32 unk3E4;
} CpuState;

extern CpuState *gCpu;
extern CpuState gCpuStates[2];
extern Fighter *gCpuSelf;
extern Fighter *gCpuOpponent;
extern s32 D_005F6C08;
extern CpuCand gCpuCands[];
extern struct FighterMove **D_005F6C00[2]; /* per slot: move records by id */
extern s32 D_004318D8[][6];
extern const s16 D_00507860[];
extern const s16 D_00507868[];

s32 rand(void);
s32 func_003840F8(CpuCand *c);
s32 func_00383B88(struct FighterMove *m, s32 arg1, s32 arg2);

s32 Cpu_SetCommand(u16 *cmd);
void func_00380568(Fighter *f);
void Cpu_GetInput(Fighter *f, u16 *held, u16 *pressed);
extern const u16 D_005074A2[];
extern const u16 D_0050748A[];

/* The practice dummy settings D_008B8068 (func_0037EF60 sets their defaults) [I]. */
typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ u8 unk4;
    /* 0x5 */ u8 unk5;
    /* 0x6 */ u8 unk6;
    /* 0x7 */ u8 unk7;
    /* 0x8 */ u8 unk8;
    /* 0x9 */ u8 unk9;
    /* 0xA */ u8 unkA;
    /* 0xB */ u8 unkB;
    /* 0xC */ u8 unkC;
    /* 0xD */ u8 unkD;
    /* 0xE */ u8 unkE;
    /* 0xF */ u8 unkF;
} Unk8B8068;
extern Unk8B8068 D_008B8068;
Unk8B8068 *func_0037EF50(void);

/* A D_005E8978 entry (as in src/battle_flow.c). */
typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ u16 unk4;
    /* 0x6 */ u16 unk6;
    /* 0x8 */ u16 unk8;
} Unk5E8978;
Unk5E8978 *func_0036C1A0(Fighter *f, Fighter *o, struct FighterMove *m);

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

void func_0037BAB0(s32 arg0, s32 arg1) {
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

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005064C0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005064D0);

typedef struct {
    /* 0x0 */ f32 unk0[4];
} __attribute__((aligned(8))) Color4f;

extern const Color4f D_005064C0;
extern const Color4f D_005064D0;
void func_00353C40(s32 id, Color4f *color);
void func_00353C68(s32 id, s32 arg1);

void func_0037BB70(Fighter *f, s32 on) {
    Color4f off = D_005064C0;
    Color4f lit = D_005064D0;

    if (on) {
        func_00353C40(f->unk1E, &lit);
        func_00353C68(f->unk1E, 1);
    } else {
        func_00353C40(f->unk1E, &off);
        func_00353C68(f->unk1E, 1);
    }
}

s32 func_0037BC00(s32 kind) {
    s32 r = 5;

    do { /* fake: the loop notes keep r out of the jump-table index register */
        switch (kind) {
        case 1:
            r = 3;
            break;
        case 2:
            r = 2;
            break;
        case 4:
            r = 1;
            break;
        case 3:
        case 5:
            r = 4;
            break;
        case 0:
        case 8:
            r = 5;
            break;
        case 6:
        case 7:
        case 9:
            r = 0;
            break;
        }
        return r;
    } while (0);
}

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

void *memset(void *p, s32 c, s32 n);
extern s32 D_0042F060[]; /* joint of each body sphere [I] */
s32 func_0037BC00(s32 kind);

/* Adds a hit marker when the other fighter `o` was hit (unk12F): at the attacker's attack volume end for moves below
 * kind 0x18, else at the joint of the body sphere that was hit; its kind from func_0037BC00 (5: none), 64 frames, and
 * its screen position [V code; I meaning]. */
void func_0037BC98(Unk37BC60 *p, Fighter *f, Fighter *o) {
    s32 x;
    s32 y;
    s32 z;
    s32 kind;
    s32 n;
    Unk37BC60Mark *m;
    s16 v[4];
    s16 xy[2];

    if (o->unk12F != 0) {
        if (f->unkC0->unk28 >= 0x18) {
            s32 k = D_0042F060[o->unk39C.unk18];

            x = ((FighterJoints *)o)->unk668[k].unk0.unk0[0];
            y = ((FighterJoints *)o)->unk668[k].unk0.unk0[1];
            z = ((FighterJoints *)o)->unk668[k].unk0.unk0[2];
        } else {
            x = f->unk19C;
            y = f->unk1A0;
            z = f->unk1A4;
        }
        kind = func_0037BC00(f->unkD4);
        if (kind != 5) {
            n = p->unk38;
            m = &p->unk3C[n];
            m->unk8 = x;
            m->unkC = y;
            m->unk10 = z;
            m->unk0 = kind;
            m->unk2 = 0x40;
            memset(v, 0, 8);
            func_0035C400(x, y, z);
            func_0039BC08(v, xy, 0, 0);
            {
                s32 sx = xy[0];
                s32 sy = xy[1];

                m->unk4 = sx;
                m->unk6 = sy;
            }
            n++;
            if (n >= 4) {
                n = 0;
            }
            p->unk38 = n;
        }
    }
}

/* Draws the four hit markers of the practice display, oldest first: each projected to the screen and faded out over
 * its last 32 frames (alpha 0x80 down to 4) [V code; I meaning]. */
void *memset(void *p, s32 c, s32 n);
void func_0035C400(s32 x, s32 y, s32 z);

void func_003D0548(s16 x, s16 y, s16 kind, s32 alpha);

/* Draws the four hit markers of the practice display, oldest first: each projected to the screen and faded out over
 * its last 32 frames (alpha 0x80 down to 4) [V code; I meaning]. */
void func_0037BDD8(Unk37BC60 *p) {
    s32 i;
    s32 n = p->unk38;
    Unk37BC60Mark *m;
    s32 alpha;
    s16 v[4];
    s16 xy[2];

    for (i = 0; i < 4; i++) {
        if (++n >= 4) {
            n = 0;
        }
        m = &p->unk3C[n];
        alpha = 0x80;
        if (m->unk2 != 0) {
            if (m->unk2 < 0x21) {
                alpha = m->unk2 * 4;
                if (alpha > 0x80) {
                    alpha = 0x80;
                }
            }
            memset(v, 0, 8);
            func_0035C400(m->unk8, m->unkC, m->unk10);
            func_0039BC08(v, xy, 0, 0);
            func_003D0548(xy[0], m->unk6, m->unk0, alpha);
            m->unk2--;
        }
    }
}

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


void func_00380420(s32 arg0, s32 arg1, s32 arg2);

void func_0037BF48(s32 arg0, s32 arg1) {
    s32 a = 1;
    s32 b = 2;

    switch (arg1) {
    case 0:
        a = 0;
        b = 1;
        break;
    case 1:
        a = 1;
        b = 2;
        break;
    case 2:
        a = 2;
        b = 3;
        break;
    case 3:
        a = 3;
        b = 1;
        break;
    case 4:
        a = 4;
        b = 3;
        break;
    }
    func_00380420(arg0, a, b);
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506528);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506538);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506548);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506558);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506578);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506598);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005065B8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005065D8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005065F8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506618);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506638);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506658);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506678);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506698);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005066B8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005066D8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005066F8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506718);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506738);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506758);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506778);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506798);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005067B8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005067D8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005067F8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506818);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506838);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506858);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506878);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506898);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005068B8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005068D8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005068F8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506918);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506928);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506938);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_0050693C);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005069F8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506A38);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506A60);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506AA0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506AC0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506B00);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506B20);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506B60);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506B80);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506BC0);

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

/* Copies the practice menu settings (func_003794A0 items, D_0042F268 bytes) into the dummy settings D_008B8068 [V]. */
void func_0037C0A8(void) {
    Unk8B8068 *d = func_0037EF50();

    d->unk4 = func_003794A0(0);
    d->unk5 = D_0042F268[0x9C];
    d->unk6 = func_003794A0(3);
    d->unk7 = func_003794A0(5);
    d->unk8 = func_003794A0(6);
    d->unk9 = func_003794A0(7);
    d->unkA = func_003794A0(4);
    d->unkB = func_003794A0(8);
    d->unkC = func_003794A0(9);
    d->unkD = func_003794A0(10);
    d->unkE = D_0042F268[0];
    d->unkF = D_0042F268[1];
}

s32 func_0037C168(void) {
    if (((gFighters[0].unkC0->unk24 >> 20) & 3) == 2 || ((gFighters[1].unkC0->unk24 >> 20) & 3) == 2) {
        return 1;
    }
    return 0;
}

extern u16 D_0042F272[];
extern TeamMember D_00440940[2][2];
void func_00377F58(Fighter *f);
void func_00377FB8(Fighter *f);

/* Body sphere radii in practice mode: with the practice menu at 7, D_0042F272 clear and item 0 at 2, character 0x1A
 * gets the larger radii (func_00377FB8) while the other fighter, character 0x1D with costume 4, plays move 0x1449
 * inside its attack window; otherwise both get the normal radii (func_00377F58) [V code; I meaning]. */
void func_0037C1B8(void) {
    s32 a;
    s32 b;
    s32 x0;
    s32 x1;

    if (D_0043B7F8[0] == 7 && D_0042F272[0] == 0 && func_003794A0(0) == 2) {
        b = 0;
        a = 0;
        if (gFighters[0].unk2A == 0x1D) {
            a = D_00440940[gFighters[0].unk1E][gFighters[0].unk0].unk4 == 4;
        }
        if (gFighters[1].unk2A == 0x1D) {
            b = D_00440940[gFighters[1].unk1E][gFighters[1].unk0].unk4 == 4;
        }
        x1 = x0 = 0;
        if (gFighters[0].unk2A == 0x1A) {
            x0 = 1;
        }
        if (gFighters[1].unk2A == 0x1A) {
            x1 = 1;
        }
        if (x0 && b) {
            if (gFighters[1].unk10C == 0x1449 && gFighters[1].unkC4 >= gFighters[1].unkC0->unk2C &&
                gFighters[1].unkC4 <= gFighters[1].unkC0->unk2D) {
                func_00377FB8(&gFighters[0]);
            }
        } else if (a && x1) {
            if (gFighters[0].unk10C == 0x1449 && gFighters[0].unkC4 >= gFighters[0].unkC0->unk2C &&
                gFighters[0].unkC4 <= gFighters[0].unkC0->unk2D) {
                func_00377FB8(&D_00884EE0);
            }
        }
        return;
    }
    func_00377F58(&gFighters[0]);
    func_00377F58(&D_00884EE0);
}

/* Moves with listed frames (D_005073E0: two {key, 0-ended frame list} entries, keys from func_00368260). */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 *unk4;
} Unk5073E0;

extern const Unk5073E0 D_005073E0[2];
s32 func_00368260(s32 chr, s32 move);

/* Whether the fighter's current move is one of D_005073E0: 0 not listed, 1 at one of its listed frames, 2 at another
 * frame [V]. */
s32 func_0037C370(Fighter *f) {
    s32 key = func_00368260(f->unk28, f->unk10C);
    s32 i;
    s32 n;
    s32 *p;
    const Unk5073E0 *e;

    for (i = 0, e = D_005073E0, n = -1; i < 2; i++, e++) {
        if (key == e->unk0) {
            n = i;
            break;
        }
    }
    if (n == -1) {
        return 0;
    }
    for (p = D_005073E0[n].unk4; *p != 0; p++) {
        if (*p == f->unkC4) {
            return 1;
        }
    }
    return 2;
}

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

void Prac_RecInput(s32 player, u16 buttons) {
    s32 n;
    CmdRecorder *r = &gPracRecorder[player];

    if (r->unk0 == 0) {
        r->unk20 = buttons & 0xF4F4;
        if (r->unk20 != r->unk22) {
            n = r->unk18;
            n++;
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

void func_0037CC60(s32 player) {
    s32 i = gPracRecorder[player].unk18;
    s32 n = 0;
    u16 b;
    char buf[16];
    CmdStep *h = (CmdStep *)((u8 *)gPracRecorder[0].unk226 + player * sizeof(CmdRecorder));

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

/* The practice recorder's menu state (D_0042F370) [I]. */
typedef struct {
    /* 0x0 */ s32 unk0; /* 1 while button 2 is held */
    /* 0x4 */ s32 unk4; /* selection 0-3 */
    /* 0x8 */ s32 unk8;
} Unk42F370;

extern Unk42F370 D_0042F370;
extern s32 D_005F6080; /* recorder state 0-4 */
extern u16 D_005F6084; /* buttons held the previous frame */

void Prac_ResetPositions(Fighter *f);

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00506BF0);

/* The practice recorder's controls, once per frame for fighter f, from the buttons held (D_0042F268 + 2) and pressed
 * (+ 4) [V code; I meaning]. State 0: while button 2 is held, buttons 0x1000 / 0x4000 move the selection unk4 (0-3);
 * releasing it resets the positions and starts playback (selection 0, state 3) or sets unk8 (selection 1); button 1
 * goes to state 1. State 1 starts recording at the first direction or button (state 2), button 1 stops it (state 0).
 * States 3-4 play the track back as the fighter's input (unk420 = 4, unk430) until button 1 or its end; returns 1
 * while playing. */
/* NON_MATCHING: 25 lines differ, all the swap of s0 / s1 between the pressed buttons and the D_0042F370 pointer
 * (declaration orders did not change it). The inner switch needs `case 2: break;` and the cases in the order 0, 1,
 * 2 for the original's `beq 1 / slti 2 / bnez` tree. */
#if 0
s32 func_0037CDA0(Fighter *f) {
    Unk42F370 *m = &D_0042F370;
    s32 r = 0;
    u16 *pad = (u16 *)D_0042F268;
    u16 held = pad[1];
    u16 pressed = pad[2];
    s16 side = f->unk1E;
    u16 rel = D_005F6084 & ~held;

    D_005F6084 = held;
    switch (D_005F6080) {
    case 0:
        if (m->unk8 != 0) {
            if (rel & 2) {
                m->unk8 = 0;
            }
            break;
        }
        if (pressed & 2) {
            m->unk4 = 0;
        }
        if (held & 2) {
            m->unk0 = 1;
            if (pressed & 0x1000) {
                m->unk4--;
            }
            if (pressed & 0x4000) {
                m->unk4++;
            }
            m->unk4 = m->unk4 < 0 ? 0 : (m->unk4 < 4 ? m->unk4 : 3);
        } else {
            m->unk0 = 0;
        }
        if (rel & 2) {
            Prac_ResetPositions(f);
            switch (m->unk4) {
            case 0:
                D_005F6080 = 3;
                break;
            case 1:
                m->unk8 = m->unk4;
                break;
            case 2:
                break;
            }
        } else if (pressed & 1) {
            D_005F6080 = 1;
        }
        break;
    case 1:
        if (pressed & 0xF4F4) {
            Prac_RecBegin(side);
            D_005F6080 = 2;
        }
    case 2:
        if (pressed & 1) {
            func_0037C678(side);
            D_005F6080 = 0;
        }
        break;
    case 3:
        func_0037C8C8(side);
        D_005F6080 = 4;
    case 4:
        f->unk420 = 4;
        r = 1;
        f->unk430 = func_0037C950(side);
        if ((pressed & 1) || Prac_IsPlaying(side) == 0) {
            D_005F6080 = 0;
        }
        break;
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037CDA0);
#endif
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506C10);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506C20);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506C38);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506C50);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506C68);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506C78);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506C88);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506C98);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506CA8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506CB8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506CC8);

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

/* Loads command list `slot` (gPracCmdLists, at most 4) from the stream `src` of u16 words: the entry and step
 * counts, each entry (nine words, 8 + 16 words of steps), each step (four words); then lists the entries with unk8
 * set (at most 16, unk3E94) and those with unk6 set (at most 128, unk3ED8) [V]. */
/* NON_MATCHING: 10 lines: the first loop's `i < count` test folds to beqz (i known 0) where the original keeps slt with i in a3; t5 copy scheduled earlier */
#if 0
void func_0037D0F8(u16 *src, s32 slot) {
    CmdList *l;
    CmdListEntry *e;
    CmdListStep *st;
    s32 i;
    s32 j;

    i = 0;
    if (slot < 4) {
        l = &gPracCmdLists[slot];
        l->unk0 = 0;
        if (src != NULL) {
            l->unk0 = *src++;
            l->unk4 = *src++;
            for (e = l->unk8; i < l->unk0; i++, e++) {
                e->unk0 = *src++;
                e->unk2 = *src++;
                e->unk4 = *src++;
                e->unk6 = *src++;
                e->unk8 = *src++;
                e->unkA = *src++;
                e->unkC = *src++;
                e->unkE = *src++;
                e->unk10 = *src++;
                for (j = 0; j < 8; j++) {
                    e->unk12[j] = *src++;
                }
                for (j = 0; j < 16; j++) {
                    e->unk22[j] = *src++;
                }
            }
            for (i = 0, st = l->unk1FBE; i < l->unk4; i++, st++) {
                st->unk0 = *src++;
                st->unk2 = *src++;
                st->unk4 = *src++;
                st->unk6 = *src++;
            }
            l->unk3E90 = 0;
            for (i = 0, e = l->unk8; i < l->unk0; i++, e++) {
                if (e->unk8 != 0) {
                    l->unk3E94[l->unk3E90] = i;
                    l->unk3E90++;
                    if (l->unk3E90 >= 16) {
                        break;
                    }
                }
            }
            l->unk3ED4 = 0;
            for (i = 0, e = l->unk8; i < l->unk0; i++, e++) {
                if (e->unk6 != 0) {
                    l->unk3ED8[l->unk3ED4] = i;
                    l->unk3ED4++;
                    if (l->unk3ED4 >= 0x80) {
                        break;
                    }
                }
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037D0F8);
#endif

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
/* NON_MATCHING: 19 lines: loop registers (t5 / t6 swapped) and the count reload scheduled into the loop; statement order from autoswap2 */
void Prac_LoadCmdIntoTrack(Fighter *f, s32 i) {
    CmdRecorder *r = &gPracRecorder[f->unk1E];
    CmdList *p = &gPracCmdLists[f->unk22];
    CmdListEntry *e = &p->unk8[i];
    CmdListStep *s = &p->unk1FBE[e->unk2];
    u16 *t = r->unk26;
    CmdStep *d = r->unkA28[0].unk10;
    s32 n = 0;

    gPracReset.unk8 = e->unkA;
    gPracReset.unkC = e->unkC;
    gPracReset.unk10 = e->unkE;
    gPracReset.unk14 = e->unk10;
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

void func_0037D668(Fighter *f) {
    Unk8B7EF0 *p = &D_008B7EF0[f->unk1E];

    switch (p->unk0) {
        case 0:
            break;
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

s32 func_0037D710(Fighter *f, s32 i) {
    Unk8B7EF0 *p = &D_008B7EF0[f->unk1E];

    if (i >= func_0037D478(f) || func_0037D498(f, i) == 0) {
        return 0;
    }
    p->unk6 = i;
    Prac_LoadCmdIntoTrack(f, (s16)i);
    return 1;
}

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
    Cam_Reset();
    func_00390E08(f, o);
    func_0037D8B8();
    func_0034CB38(0);
    func_0034CB38(1);
}


/* Starts the practice dummy's stance move for `state` (0x3A, 0x50, 0xD6, 0xE4) facing unk3A, turned by a quarter or a
 * half for some states [V code; I meaning]. */
void func_0037DA08(Fighter *f, s32 state) {
    switch (state) {
    case 0:
        f->unk1A = f->unk3C = f->unk3A;
        func_00368818(f, f, 0x3A);
        break;
    case 1:
        f->unk1A = f->unk3C = f->unk3A;
        func_00368818(f, f, 0x50);
        break;
    case 2:
        f->unk1A = f->unk3C = f->unk3A + 0x4000;
        func_00368818(f, f, 0x3A);
        break;
    case 3:
        f->unk1A = f->unk3C = f->unk3A - 0x8000;
        func_00368818(f, f, 0x3A);
        break;
    case 4:
        f->unk1A = f->unk3C = f->unk3A - 0x4000;
        func_00368818(f, f, 0x3A);
        break;
    case 5:
        f->unk1A = f->unk3C = f->unk3A;
        func_00368818(f, f, 0xD6);
        break;
    case 6:
        f->unk1A = f->unk3C = f->unk3A + 0x4000;
        func_00368818(f, f, 0xD6);
        break;
    case 7:
        f->unk1A = f->unk3C = f->unk3A - 0x8000;
        func_00368818(f, f, 0xD6);
        break;
    case 8:
        f->unk1A = f->unk3C = f->unk3A - 0x4000;
        func_00368818(f, f, 0xD6);
        break;
    case 9:
        f->unk1A = f->unk3C = f->unk3A;
        func_00368818(f, f, 0xE4);
        break;
    case 10:
        f->unk1A = f->unk3C = f->unk3A + 0x4000;
        func_00368818(f, f, 0xE4);
        break;
    case 11:
        f->unk1A = f->unk3C = f->unk3A - 0x8000;
        func_00368818(f, f, 0xE4);
        break;
    case 12:
        f->unk1A = f->unk3C = f->unk3A - 0x4000;
        func_00368818(f, f, 0xE4);
        break;
    case 13:
        f->unk1A = f->unk3C = f->unk3A;
        func_00368818(f, f, 0x3A);
        break;
    }
}

s32 func_0037DBE8(void) {
    return func_0037DC38(gPracReset.unk8);
}

s32 func_0037DC10(void) {
    return func_0037DC38(gPracReset.unkC);
}

RODATA_ALIGN16();
s32 func_0037DC38(s32 state) {
    s32 r = 0;

    do { /* fake: the loop notes keep r out of the jump-table index register */
        switch (state) {
        case 1:
            r = 2;
            break;
        case 0:
        case 2:
        case 3:
        case 4:
            r = 0;
            break;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
            r = 13;
            break;
        case 13:
            r = 1;
            break;
        }
        return r;
    } while (0);
}

s32 func_0037DC88(void) {
    return gPracReset.unk14;
}


/* The practice dummy's reaction to the action `kind` of its settings: kinds 6-10 while unkF7 is set go to
 * func_0037E0E8, 13 to func_0037DDD0; otherwise, when func_0037DDE0 allows it, 1-4 and 14 run their handler and 5 / 11
 * set unk12C [V code; I meaning]. */
void func_0037DC98(Fighter *f, Fighter *o, s32 kind) {
    f->unk12C = 0;
    switch (kind) {
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
        if (f->unkF7 != 0) {
            func_0037E0E8(f, o, kind);
            return;
        }
        break;
    case 13:
        func_0037DDD0(f);
        return;
    }
    if (func_0037DDE0(f) != 0) {
        switch (kind) {
        case 0:
            break;
        case 1:
            func_0037E010(f, o);
            break;
        case 2:
            func_0037DFF0(f);
            break;
        case 3:
            func_0037E180(f, o);
            break;
        case 4:
            func_0037E090(f, o);
            break;
        case 5:
        case 11:
            f->unk12C = 1;
            break;
        case 14:
            func_0037DE40(f, o);
            break;
        }
    }
}

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

s32 func_0037DFC8(Fighter *f) {
    do { /* fake: the loop label aligns the second return like the original's nop */
        if (f->unk12F != 0) {
            return 1;
        }
    } while (0);
    return f->unkF8 == 3;
}

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


u16 func_0037E1D8(Fighter *f, s32 arg1, s32 a, s32 b);

s32 func_0037E0E8(Fighter *f, s32 arg1, s32 kind) {
    s32 bits;

    if (f->unkD5 == 9 || f->unkD5 == 10) {
        switch (kind) {
        case 6:
            bits = 0x60;
            break;
        case 7:
            bits = 0x90;
            break;
        case 9:
            bits = 0x2000;
            break;
        case 10:
            bits = 0x8000;
            break;
        case 8:
            bits = 4;
            break;
        default:
            return 0;
        }
        f->unk430 = func_0037E1D8(f, arg1, bits, bits);
        return 1;
    }
    return 0;
}

s32 func_0037E180(Fighter *f, Fighter *o) {
    f->unk430 = 0x4000;
    if ((o->unkD0 & 8) && o->unkC0->unk2D >= o->unkC4) {
        f->unk430 = 0xC000;
        return 1;
    }
    return f->unk10C == f->unkC0->unk10;
}

/* A copy of a fighter's input history (FighterUnk438, 0x88 bytes) [I]. */
typedef struct {
    /* 0x00 */ s32 unk0[0x22];
} Unk8B7F00;

extern Unk8B7F00 D_008B7F00[2];
void Fighter_RecordInput(Fighter *f, u16 held, u16 pressed);
s32 func_00368F78(Fighter *f, MoveCmd *c);
MoveCmd *func_0037E468(Fighter *f, Fighter *o, MoveCmd *c, s32 frame);

/* Whether the practice dummy's buttons `a` / `b` would start a command now: records them in a scratch copy of the
 * input history (the real one is saved in D_008B7F00[side] and restored), looks for a command of the current move
 * at the current frame (func_0037E468) or, at the list end, of the next move at frame 1, and returns a | b when one
 * applies, else 0 [V code; I meaning]. */
u16 func_0037E1D8(Fighter *f, s32 arg1, s32 a, s32 b) {
    u16 held = a;
    u16 pressed = b;
    FighterUnk438 *h = &f->unk438;
    Unk8B7F00 *save = &D_008B7F00[f->unk1E];
    MoveCmd *c;
    u16 end;
    u16 r;

    *save = *(Unk8B7F00 *)h;
    Fighter_RecordInput(f, held, pressed);
    c = func_0037E468(f, (Fighter *)arg1, (MoveCmd *)f->unkC0->unkC, f->unkC4);
    end = c->unk0;
    /* fake: the second search as a comma expression inside the test keeps `a | b` computed in each delay slot */
    if (end != 0x8000 ||
        ((func_00368F78(f, c) == 0 ||
          (c = func_0037E468(f, (Fighter *)arg1, (MoveCmd *)func_00368218(f, c->unk6)->unkC, 1), 1)) &&
         c->unk0 != end)) {
        r = held | pressed;
    } else {
        r = 0;
    }
    *(Unk8B7F00 *)h = *save;
    return r;
}

extern CpuCmd D_0047F780[];
s32 Cmd_MatchInput(Fighter *f, u16 code, u16 dir, u16 held, u16 pressed);
s32 Cmd_CheckCondition(Fighter *f, Fighter *o, s16 id);
s32 Cmd_CheckSituation(MoveCmd *c, Fighter *f, Fighter *o);

/* func_003691D8 for the practice dummy: the first entry of the command list c that applies at `frame` (window,
 * input, condition, kind below 0x60 and situation); kind 0x2E jumps to the list end, kind 0x1C is skipped while
 * unk170 is set. With a move record override (unk188) it returns the end entry at once [V code; I meaning]. */
MoveCmd *func_0037E468(Fighter *f, Fighter *o, MoveCmd *c, s32 frame) {
    u16 dir = f->unk438.unk7C;
    u16 held = f->unk438.unk80;
    u16 pressed = f->unk438.unk7E;
    MoveCmd *ret;

    if (f->unk188 != NULL) {
        while (c->unk0 != 0x8000) {
            c++;
        }
    }
    for (; c->unk0 != 0x8000; c++) {
        if (c->unk0 == 0x8004) {
            ret = c;
            c = (MoveCmd *)&D_0047F780[ret->unk6];
        } else if (c->unk0 == 0x8005) {
            c = ret;
            continue;
        }
        if (frame >= c->unkB && frame <= c->unkC && Cmd_MatchInput(f, c->unk0, dir, held, pressed) &&
            Cmd_CheckCondition(f, o, c->unk2) && (c->unk3 & ~0x80) < 0x60 &&
            (c->unk3 == 0 || Cmd_CheckSituation(c, f, o))) {
            if ((u16)c->unk8 == 0x2E) {
                while (c->unk0 != 0x8000) {
                    c++;
                }
            }
            if ((u16)c->unk8 != 0x1C || f->unk170 == 0) {
                return c;
            }
        }
    }
    return c;
}

/* Whether the practice dummy can act now: a command of the current move applies at the current frame (as
 * func_0037E468 without the input, kinds 0x3D-0x3F counting at once, input 0x10 of kind 2 never), or the move is near
 * its end (from 5 frames before the end entry's window to its end, with unkDA positive) and a command of the next
 * move applies at frame 1 [V code; I meaning]. */
s32 func_0037E670(Fighter *f, Fighter *o) {
    s32 next = 0;
    MoveCmd *c = (MoveCmd *)f->unkC0->unkC;
    s16 frame = f->unkC4;
    MoveCmd *ret;

    if (f->unk188 != NULL) {
        while (c->unk0 != 0x8000) {
            c++;
        }
    }
    for (; c->unk0 != 0x8000; c++) {
        if (c->unk0 == 0x8004) {
            ret = c;
            c = (MoveCmd *)&D_0047F780[ret->unk6];
        } else if (c->unk0 == 0x8005) {
            c = ret;
            continue;
        }
        if (frame >= c->unkB && frame <= c->unkC && Cmd_CheckCondition(f, o, c->unk2) && (c->unk3 & ~0x80) < 0x60 &&
            (c->unk0 != 0x10 || c->unk3 != 0 || (u16)c->unk8 != 2)) {
            if (c->unk3 >= 0x3D && c->unk3 <= 0x3F) {
                break;
            }
            if (c->unk3 == 0 || Cmd_CheckSituation(c, f, o)) {
                if ((u16)c->unk8 == 0x2E) {
                    while (c->unk0 != 0x8000) {
                        c++;
                    }
                }
                if ((u16)c->unk8 != 0x1C || f->unk170 == 0) {
                    break;
                }
            }
        }
    }
    if (c->unk0 != 0x8000) {
        return 1;
    }
    if (f->unkC4 >= c->unkB - 5 && f->unkC4 < c->unkB) {
        next = f->unkDA > 0;
    }
    if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && f->unkDA > 0) {
        next = 1;
    }
    if (next) {
        frame = 1;
        for (c = (MoveCmd *)func_00368218(f, c->unk6)->unkC; c->unk0 != 0x8000; c++) {
            if (c->unk0 == 0x8004) {
                ret = c;
                c = (MoveCmd *)&D_0047F780[ret->unk6];
            } else if (c->unk0 == 0x8005) {
                c = ret;
                continue;
            }
            if (frame >= c->unkB && frame <= c->unkC && Cmd_CheckCondition(f, o, c->unk2) &&
                (c->unk3 & ~0x80) < 0x60 && (c->unk0 != 0x10 || c->unk3 != 0 || (u16)c->unk8 != 2) &&
                Cmd_CheckSituation(c, f, o)) {
                break;
            }
        }
        if (c->unk0 != 0x8000) {
            return 1;
        }
    }
    return c->unk0 != 0x8000;
}

/* A practice record (0x8B8010, .bss): five counters and a list of 16 move ids (func_0037EE18) [I]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14[16];
} Unk8B8010;

extern Unk8B8010 D_008B8010;

/* Clears D_008B8010 [V]. */
void func_0037EA08(void) {
    Unk8B8010 *d = &D_008B8010;
    s32 i;

    d->unk0 = 0;
    d->unk4 = 0;
    d->unk8 = 0;
    d->unkC = 0;
    d->unk10 = 0;
    for (i = 0; i < 16; i++) {
        d->unk14[i] = 0;
    }
}


void func_0037EE68(s32 frames);
void func_0037EEF8(void);

/* Per frame while the practice record D_008B8010 is in state 2: counts the frames of the combo (at most 0x34BC0) and
 * prints the timer (menu item 8) and the combo counter [V]. */
void func_0037EA58(void) {
    Unk8B8010 *d = &D_008B8010;

    switch (d->unk0) {
    case 0:
        break;
    case 1:
        break;
    case 2:
        if (d->unk8 != 0) {
            d->unkC++;
        }
        if ((u32)d->unkC > 0x34BC0) {
            d->unkC = 0x34BC0;
        }
        if (func_003794A0(8) == 1) {
            func_0037EE68(d->unkC);
        }
        func_0037EEF8();
        break;
    case 3:
        break;
    case 4:
        break;
    }
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506DD8);
extern s32 D_005F60A4;
extern s32 D_005F60A8;
extern s32 D_005F60AC;
extern s16 D_005F62AC;
extern s32 D_005F5C40[2];
extern s32 D_005F5C48[2];
s32 Text_Printf(const char *fmt, ...);
extern const char D_00506DD8[];
void func_0037B8D8(Fighter *f, s32 a, s32 b);
s32 func_0037EE18(Fighter *f);
void func_003DEC20(void);
void func_003DD158(s32 side);
void func_003DD188(s32 x, s32 y, s32 side, s32 a, s32 b);
void Prac_DrawCongratulations(Fighter *f, s32 n);

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00506DF0);

/* The practice record's steps, once per frame with the pad words (held at +2, pressed at +4) [V code; I meaning]:
 * 0 loads the record (func_0037EA08, func_0037D3E0) and waits 90 frames, 1 prints D_00506DD8 while it counts down
 * (sounds 0x105A / 0x105B), 2 plays the record's moves one by one (func_0037EE18 checks each, sound 0x103D) and then
 * waits 180 frames in 3, where button 0x800 or, at the end, buttons 0xF0 stop it and holding 0x4003 for 1800 frames
 * calls func_0037BA88; 4 stops. Returns 0 once it stops. D_005F60A8 / D_005F60AC detect the first frame of a step. */
/* NON_MATCHING: 4 lines: the d->unk4 increment uses v0 in the original, v1 here (plus D_005F5C44 / D_005F5C4C read by their own symbols) */
#if 0
s32 func_0037EB00(u16 *pad, Fighter *f) {
    Unk8B8010 *d = &D_008B8010;
    s32 r = 1;
    s32 n = D_005F60A8 + 1;
    s32 first = n != D_005F60AC;

    D_005F60A8 = n;
    D_005F60AC = n;
    switch (d->unk0) {
    case 0:
        d->unk4 = 0;
        func_0037EA08();
        func_0037D3E0(f, d->unk4, d->unk14);
        n = func_0037D380(f);
        d->unk8 = n;
        d->unk10 = 0x5A;
        d->unk0 = 1;
        break;
    case 1:
        Text_Printf(D_00506DD8, 0xC0, 0xDC, 0xA, 2);
        if (first && d->unk10 == 0x5A) {
            Snd_PlaySe(0x105A, 0);
        }
        if (--d->unk10 <= 0) {
            d->unk10 = 0;
            d->unk0 = 2;
            Snd_PlaySe(0x105B, 0);
        }
        break;
    case 2:
        if (d->unk8 != 0) {
            func_0037B8D8(f, func_0037D3A0(f, d->unk4), 0);
        } else {
            func_003DEC20();
            func_003DD158(f->unk1E);
            func_003DD188(0x140, 0x64, f->unk1E, 0, 0);
        }
        if (d->unk8 != 0 && func_0037EE18(f) != 0) {
            Snd_PlaySe(0x103D, 0);
            d->unk4++;
            if ((u32) d->unk4 >= (u32) func_0037D380(f)) {
                d->unk10 = 0xB4;
                d->unk0 = 3;
            } else {
                func_0037D3E0(f, d->unk4, d->unk14);
                d->unk8 = func_0037D380(f);
            }
        }
        break;
    case 3:
        if (first && d->unk10 == 0x78) {
            Snd_PlaySe(0x1058, 0);
        }
        if (d->unk10 == 0x78) {
            D_005F62AC = 0;
        }
        if (d->unk10 < 0x78) {
            Prac_DrawCongratulations(f, d->unkC);
        }
        if (pad[2] & 0x800) {
            D_005F5C40[0] &= 0xFFFFF7FF;
            D_005F5C48[0] &= 0xFFFFF7FF;
            D_005F5C40[1] &= 0xFFFFF7FF;
            D_005F5C48[1] &= 0xFFFFF7FF;
            r = 0;
            d->unk0 = 0;
        }
        if (--d->unk10 <= 0) {
            d->unk10 = 0;
            if (pad[2] & 0xF0) {
                d->unk0 = 0;
                r = 0;
            }
            if ((pad[1] & 0x4003) == 0x4003) {
                if (pad[2] & 0x4003) {
                    D_005F60A4 = 0;
                }
                if (++D_005F60A4 == 0x708) {
                    func_0037BA88();
                }
            } else {
                D_005F60A4 = 0;
            }
        }
        break;
    case 4:
        d->unk0 = 0;
        r = 0;
        break;
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EB00);
#endif

/* Whether the move f->unk10C is in the list of D_008B8010 (zero entries are empty) [V]. */
s32 func_0037EE18(Fighter *f) {
    s32 i = 0;
    s32 r;
    Unk8B8010 *d;
    s16 id;

    r = 0;
    d = &D_008B8010;
    id = f->unk10C;
    i = r;
    for (; i < 16; i++) {
        if (d->unk14[i] != 0 && d->unk14[i] == id) {
            r = 1;
            break;
        }
    }
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506E08);

/* NON_MATCHING: 13 lines differ: the constant 60 of the three divisions lives in t4 (and a second copy in v1)
 * where the original keeps one copy in v1. */
#if 0
extern const char D_00506E08[];
s32 Text_Printf(const char *fmt, ...);

/* Prints a frame count as minutes, seconds and hundredths, at most 99'59"99 [V]. */
void func_0037EE68(s32 frames) {
    s32 t = 0x57E40;
    s32 s;
    s32 f;

    if (frames < 0x57E40) {
        t = frames;
    }
    s = t / 60;
    f = t % 60;

    Text_Printf(D_00506E08, 0xF0, 0x50, 0xA, 4, 0x13, s / 60, s % 60, f * 100 / 60);
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EE68);
#endif

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506E28);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506E40);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506E60);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506E70);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506E80);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506E90);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506EA0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506EB0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506EC0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506ED0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506EE0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506EF0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506F00);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506F10);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506F20);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506F30);

s32 Text_Printf(const char *fmt, ...);
extern const char D_00506E28[];
extern const char D_00506E40[];

/* Prints the combo counter of the practice record D_008B8010 ("COMBO n:m") [V]. */
void func_0037EEF8(void) {
    Unk8B8010 *d = &D_008B8010;
    s32 n = d->unk4;

    if (d->unk8 != 0) {
        Text_Printf(D_00506E28, 0x190, 0x50, 0xA, 4, 0xA, n + 1, d->unk8);
    } else {
        Text_Printf(D_00506E40, 0x190, 0x50, 0xA, 4, 0xA, n);
    }
}


Unk8B8068 *func_0037EF50(void) {
    return &D_008B8068;
}

void func_0037EF60(void) {
    D_008B8068.unk0 = 0x26;
    D_008B8068.unk2 = 0x50;
    D_008B8068.unk4 = 0;
    D_008B8068.unk5 = 0;
    D_008B8068.unk6 = 0;
    D_008B8068.unk7 = 0;
    D_008B8068.unk8 = 1;
    D_008B8068.unk9 = 0;
    D_008B8068.unkA = 1;
    D_008B8068.unkB = 0;
    D_008B8068.unkC = 0;
    D_008B8068.unkD = 0;
    D_008B8068.unkE = 0;
    D_008B8068.unkF = 1;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506F40);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506F90);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506FA8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506FB8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506FC8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506FD8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506FE8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00506FF8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507008);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507048);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507058);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507090);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005070A0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005070E8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005070F8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507120);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507128);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507178);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507190);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005071A0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005071B0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005071C0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005071D0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005071E0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005071F0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507200);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507210);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507228);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507238);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507250);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507260);
/* A practice menu line (D_00506F40 / D_00507008 / D_00507058 / D_005070A0, 8 bytes): bit 0 selectable, bit 1 shows a
 * value of kind flags >> 8 (0-4), and its label [I]. */
typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u8 pad2[2];
    /* 0x4 */ const char *unk4;
} PracMenuItem;

extern const PracMenuItem D_00506F40[];
extern const PracMenuItem D_00507008[];
extern const PracMenuItem D_00507058[];
extern const PracMenuItem D_005070A0[];
extern const char *D_00431860[];
extern const char *D_00431870[];
extern const char *D_004318A0[];
extern const char *D_004318B8[];
extern const char *D_004318C0[];
extern const s8 D_005070F8[];
extern const u16 D_00507120[];
extern const u16 D_00507128[][10];
extern const u16 D_00507178[];
extern const u16 D_00507190[];
extern const char D_005071A0[];
extern const char D_005071B0[];
extern const char D_005071C0[];
extern const char D_005071D0[];
extern const char D_005071E0[];
extern const char D_005071F0[];
extern const char D_00507200[];
extern const char D_00507210[];
extern const char D_00507228[];
extern const char D_00507238[];
extern const char D_00507250[];
extern const char D_00507260[];
const char **func_0035C710(s16 id);
s32 func_003B1468(void);
void func_003BB890(s32 a, s32 x, s32 y, s32 b);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 b, s32 color);
void Ui_DrawWindow(s32 x, s32 y, s32 w, s32 h, s32 a, s32 b, s32 c);
void func_003DEB80(s32 a);
void func_003E08D0(s32 n);
void Text_DrawId(s32 x, s32 y, s32 id, s32 color);
void Menu_DrawCursorBar(s32 x, s32 y, s32 w, s32 color);

/* Shows the practice help box of text `id` [I]. */
#define PRAC_HELP(y, id)                                                                                             \
    do {                                                                                                             \
        func_003DEB80(0);                                                                                            \
        func_003DBCF0(0, (y) - 1, 0x280, 0x30, 0xFFFE, 0x18181860);                                                  \
        Text_DrawId(0x1E, (y), (id), 0x60606080);                                                                  \
    } while (0)

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00507270);

/* Draws the practice menu D_008B8068.unk4 (four pages of lines) at unk0 / unk2 with the cursor unk5 (kept below the
 * count of selectable lines): the frame, the title D_00431860, the help box of the page or the selected line
 * (D_00507120 / D_00507128 / D_00507178 / D_00507190, while func_003B1468), each label (colour 0xB when selected,
 * with the marker func_003BB890 / Menu_DrawCursorBar) and its value right-aligned at +0x17C: 0 the setting unk6
 * (D_00431870, with D_00507250 / D_00507260 for 6 / 7), 1 unkA (D_004318A0), 2 the per-page values unk7-unkB
 * (D_004318B8, func_003E08D0 for the selected one), 3 the character name of the fighter unkE / unkF (func_0035C710)
 * and its D_004318C0 text, 4 the count unkD against the character's limit D_005070F8 [V calls; I meaning]. */
/* NON_MATCHING: a draft (about 400 lines differ): the original keeps the line count and the selectable index on the
 * stack, x + 8 and the line y in saved registers, and shares the cursor tail of the value kinds through a result
 * register; the value kinds and calls follow the original. */
#if 0
void func_0037EFB0(void) {
    Unk8B8068 *s = &D_008B8068;
    s32 x = s->unk0;
    s32 n;
    const PracMenuItem *items;
    s32 cnt;
    s32 i;
    s32 idx;
    s32 y;
    s32 color;
    s32 w;
    s32 x0;
    s32 sel;
    s32 v;
    s32 lim;
    s32 k;
    u16 h;
    const char **name;

    switch (s->unk4) {
    case 0:
        n = 10;
        items = D_00506F40;
        break;
    case 1:
        n = 8;
        items = D_00507008;
        break;
    case 2:
        n = 7;
        items = D_00507058;
        break;
    case 3:
        n = 9;
        items = D_005070A0;
        break;
    default:
        return;
    }
    cnt = 0;
    for (i = 0; i < n; i++) {
        cnt += items[i].unk0 & 1;
    }
    if (cnt != 0) {
        s->unk5 %= cnt;
    }
    y = (u16)s->unk2 + 0x3D;
    Ui_DrawWindow(x, (u16)s->unk2, 0x194, n * 0x15 + 0x44, 0xFFFEC, 0x2020278, 0x41825E);
    Text_Printf(D_005071A0, 0xA, 5, 7, x + 0x1C, (u16)s->unk2 + 0x1A, D_00431860[s->unk4]);
    if (func_003B1468() != 0) {
        PRAC_HELP(0x178, D_00507120[s->unk4]);
    }
    for (i = 0, idx = 0; i < n; i++) {
        color = 5;
        if ((items[i].unk0 & 1) && idx == s->unk5) {
            color = 0xB;
        }
        if (color == 0xB) {
            Menu_DrawCursorBar(x - 0x2C, y, 0x19C, 0xFFFF0);
        }
        Text_Printf(D_005071A0, 0xA, 4, color, x + 8, y, items[i].unk4);
        if (i == 0 && color == 0xB) {
            func_003BB890(1, x + 8 + Text_Printf(D_005071B0, 0xA, 4, items[0].unk4) + 6, y, 5);
        }
        if (func_003B1468() != 0 && (items[i].unk0 & 1) && idx == s->unk5) {
            h = D_00507128[s->unk4][i];
            if (h == 1) {
                PRAC_HELP(0x18C, D_00507178[s->unk6]);
            } else if (h != 0) {
                PRAC_HELP(0x18C, h);
            }
        }
        if (items[i].unk0 & 2) {
            k = -1;
            switch (items[i].unk0 >> 8) {
            case 3:
                name = func_0035C710(gFighters[(s->unkC != 0 ? s->unkF : s->unkE) & 1].unk26);
                w = Text_Printf(D_005071C0, 0xA, 4, *name, D_004318C0[s->unkC]);
                Text_Printf(D_005071D0, 0xA, 4, 0x14, x + 8 - w + 0x17C, y, *name, D_004318C0[s->unkC]);
                k = 2;
                break;
            case 4:
                v = gFighters[(s->unkC != 0 ? s->unkF : s->unkE) & 1].unk2A;
                lim = D_005070F8[(u32)v < 0x22 ? v : 0];
                if (lim == 0) {
                    w = Text_Printf(D_005071E0, 0xA, 4);
                    Text_Printf(D_005071F0, 0xA, 4, 0x17, x + 8 - w + 0x17C, y);
                } else if (s->unkD >= lim) {
                    w = Text_Printf(D_00507200, 0xA, 4, s->unkD);
                    Text_Printf(D_00507210, 0xA, 4, 0x17, x + 8 - w + 0x17C, y);
                } else {
                    w = Text_Printf(D_00507228, 0xA, 4, s->unkD + 1);
                    Text_Printf(D_00507238, 0xA, 4, 0x17, x + 8 - w + 0x17C, y, s->unkD + 1);
                }
                k = 3;
                break;
            case 0:
                v = s->unk6;
                w = Text_Printf(D_005071B0, 0xA, 4, D_00431870[v]);
                x0 = x + 8 - w + 0x17C;
                Text_Printf(D_005071A0, 0xA, 4, 0x14, x0, y, D_00431870[v]);
                if ((u32)(v - 6) < 2) {
                    Text_Printf(v == 7 ? D_00507250 : D_00507260, 0xB, 1, x0 + w - 0x16, y);
                }
                k = 3;
                break;
            case 1:
                w = Text_Printf(D_005071B0, 0xA, 4, D_004318A0[s->unkA]);
                Text_Printf(D_005071A0, 0xA, 4, 0x14, x + 8 - w + 0x17C, y, D_004318A0[s->unkA]);
                if (s->unk5 == 3) {
                    func_003BB890(1, x + 8 + 0x182, y, 5);
                }
                if (func_003B1468() != 0 && idx == s->unk5) {
                    PRAC_HELP(0x18C, D_00507190[s->unkA]);
                }
                break;
            case 2:
                v = -1;
                sel = 0;
                switch (s->unk4) {
                case 0:
                    if (i == 5) {
                        v = s->unk7;
                        sel = s->unk5 == 4;
                    } else if (i == 6) {
                        v = s->unk8;
                        sel = s->unk5 == 5;
                    } else if (i == 7) {
                        v = s->unk9;
                        sel = s->unk5 == 6;
                    }
                    break;
                case 1:
                    if (i == 4) {
                        v = s->unk8;
                        sel = s->unk5 == 4;
                    } else if (i == 5) {
                        v = s->unk9;
                        sel = s->unk5 == 5;
                    }
                    break;
                case 2:
                    if (i == 3) {
                        v = s->unk8;
                        sel = s->unk5 == 3;
                    } else if (i == 4) {
                        v = s->unkB;
                        sel = s->unk5 == 4;
                    }
                    break;
                case 3:
                    if (i == 5) {
                        v = s->unk8;
                        sel = s->unk5 == 4;
                    } else if (i == 6) {
                        v = s->unk9;
                        sel = s->unk5 == 5;
                    }
                    break;
                }
                if (v >= 0) {
                    w = Text_Printf(D_005071B0, 0xA, 4, D_004318B8[v]);
                    Text_Printf(D_005071A0, 0xA, 4, v != 0 ? 0x10 : 0x16, x + 8 - w + 0x17C, y, D_004318B8[v]);
                    if (sel) {
                        func_003BB890(1, x + 8 + 0x182, y, 5);
                        func_003E08D0(v);
                    }
                }
                break;
            }
            if (k >= 0 && s->unk5 == k) {
                func_003BB890(1, x + 8 + 0x182, y, 5);
            }
        }
        y += 0x15;
        if (i == 0) {
            y += 0xA;
        }
        switch (s->unk4) {
        case 0:
            if (i == 7) {
                y += 0xA;
            }
            break;
        case 1:
            if (i == 5) {
                y += 0xA;
            }
            break;
        case 2:
            if (i == 4) {
                y += 0xA;
            }
            break;
        case 3:
            if (i == 6) {
                y += 0xA;
            }
            break;
        }
        idx += items[i].unk0 & 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EFB0);
#endif

s32 func_0037FA30(void) {
    return func_003794A0(0) == 0;
}

typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s16 unk8;
    /* 0xA */ s16 unkA;
    /* 0xC */ s16 unkC;
} Unk4318C8;
extern Unk4318C8 D_004318C8;

void func_0037FA50(void) {
    D_004318C8.unk0 = 0;
    D_004318C8.unk2 = 0;
    D_004318C8.unk4 = 0;
    D_004318C8.unk6 = 0;
    D_004318C8.unk8 = 0;
    D_004318C8.unkA = 0;
    D_004318C8.unkC = 0;
}

extern u32 D_00415718[];
s32 func_003AFFB8(s32 i, s32 a);

/* The pad bits of both team members of side *p (D_00415718 by func_003AFFB8) into D_004318C8: held unk6 / unk8, newly
 * pressed unkA, released unkC [V code; I meaning]. */
#if 0
/* NON_MATCHING: 12 lines differ: the original loads the second table entry first and keeps old ^ new in v0 */
void func_0037FA78(u8 *p) {
    Unk4318C8 *s = &D_004318C8;
    u8 side = *p;
    Fighter *f = &gFighters[side];
    u16 held = D_00415718[func_003AFFB8(side, f->unk0)] | D_00415718[func_003AFFB8(side, f->unk2)];
    u16 diff = s->unk8 ^ held;

    s->unkC = ~held & diff;
    s->unkA = held & diff;
    s->unk6 = held;
    s->unk8 = held;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FA78);
#endif

void func_0036C448(Fighter *f);

void func_0037FB30(s32 side) {
    D_004318C8.unk0 = 0;
    func_0037CA98(side);
    func_0036C448(&gFighters[side]);
}

void func_0036C480(Fighter *f);
void func_0036C490(Fighter *f);
void func_0036C4A0(Fighter *f);
void func_0036C4B0(Fighter *f);
void func_0037FE58(s32 player);

/* The practice dummy's recorder, once per frame for fighter f while func_0037FA30 allows it, driven by the menu
 * buttons D_004318C8.unkA while unk6 bit 0x100 is set [V code; I meaning]: 0x20 starts recording (state 1, the
 * recording begins at the next direction or button, states 2-3, until the track is full or 0x100 stops it; then 180
 * frames of state 4 with three countdown sounds), 0x80 / 0x40 replay the track (state 6-7) repeatedly or once, 0x100
 * stops. Returns 1 while the dummy is driven (unk420 4 during playback, 1 after), then prints the menu
 * (func_0037FE58). */
s32 func_0037FB70(Fighter *f) {
    Unk4318C8 *s = &D_004318C8;
    s32 r = 0;
    s16 side = f->unk1E;
    s32 stop;
    s32 rec;
    s32 once;
    s32 loop;

    if (func_0037FA30() == 0) {
        return 0;
    }
    stop = 0;
    rec = 0;
    once = 0;
    loop = 0;
    if (s->unk6 & 0x100) {
        if (s->unkA & 0x80) {
            loop = 1;
        } else if (s->unkA & 0x40) {
            once = 1;
        } else if (s->unkA & 0x20) {
            rec = 1;
        } else if (s->unkA & 0x100) {
            stop = 1;
        }
    }
    switch (s->unk0) {
    case 0:
        if (rec) {
            s->unk2 = 0;
            s->unk0 = 1;
            func_0037CA98(side);
            func_0036C4A0(f);
        } else if (func_0037C828(side) != 0) {
            if (loop) {
                r = 1;
                func_0036C4B0(f);
                s->unk2 = 1;
                s->unk0 = 6;
            } else if (once) {
                r = 1;
                func_0036C4B0(f);
                s->unk2 = 0;
                s->unk0 = 6;
            }
        }
        break;
    case 1:
        if (stop) {
            s->unk0 = 0;
            break;
        }
        if (!(D_005F5C48[f->unk1E] & 0xF4FE)) {
            break;
        }
        s->unk0 = 2;
    case 2:
        Prac_RecBegin(side);
        s->unk0 = 3;
    case 3:
        if (stop) {
            func_0037C698(side);
            s->unk0 = 0;
        }
        if (func_0037C8A0(side) != 0) {
            func_0037C698(side);
            s->unk0 = 4;
            s->unk4 = 0xB4;
        }
        break;
    case 4:
        r = 1;
        func_0036C480(f);
        if (s->unkA & 0x100) {
            func_0036C490(f);
            s->unk0 = 0;
        } else if (s->unk4 > 0) {
            if (s->unk4 == 0xB4 || s->unk4 == 0xA0 || s->unk4 == 0x8C) {
                Snd_PlaySe(0x103D, 0);
            }
            s->unk4--;
        }
        break;
    case 5:
        r = 1;
        if (stop) {
            f->unk420 = 1;
            s->unk0 = 0;
        } else if (s->unk4 > 0) {
            s->unk4--;
        } else {
            s->unk0 = 6;
        }
        break;
    case 6:
        func_0037C8D0(side);
        s->unk0 = 7;
    case 7:
        f->unk420 = 4;
        r = 1;
        f->unk430 = func_0037C970(side);
        if (stop) {
            f->unk420 = 1;
            s->unk0 = 0;
        } else if (Prac_IsPlaying(side) == 0) {
            if (s->unk2 != 0) {
                s->unk4 = func_0037C878(side);
                s->unk0 = 5;
            } else {
                f->unk420 = 1;
                s->unk0 = 0;
            }
        }
        break;
    }
    func_0037FE58(side);
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005072B0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005072D0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507300);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507330);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507340);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507350);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507370);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507388);
extern u16 gFrameCount;
s32 func_0037C828(s32 player);
extern const char D_005072B0[];
extern const char D_005072D0[];
extern const char D_00507300[];
extern const char D_00507330[];
extern const char D_00507340[];
extern const char D_00507350[];
extern const char D_00507370[];
extern const char D_00507388[];

/* Prints the practice dummy's menu texts for its state D_004318C8.unk0: the help lines (state 0, two or three as
 * func_0037C828 says), or a blinking title (gFrameCount bit 4) and the line D_00507350 (states 1-7) [V]. */
void func_0037FE58(s32 player) {
    s32 more;
    s32 y;

    switch (D_004318C8.unk0) {
    case 0:
        more = func_0037C828(player);
        y = more ? 0x186 : 0x19A;
        Text_Printf(D_005072B0, 0x168, y, 0xA, 4, 0x17);
        y += 0x14;
        Text_Printf(D_005072D0, 0x190, y, 0xA, 4, 7, 0xB, 0xA, 6, 0xB, 0xA, 8, 0xB, 0xA, 6);
        if (more) {
            Text_Printf(D_00507300, 0x190, y + 0x14, 0xA, 4, 7, 0xB, 0xA, 6, 0xB, 0xA, 8, 0xB, 0xA, 6);
        }
        break;
    case 1:
        if (gFrameCount & 0x10) {
            Text_Printf(D_00507330, 0xC4, 0x1AE, 0xA, 4, 0x10);
        }
        break;
    case 2:
    case 3:
        if (gFrameCount & 0x10) {
            Text_Printf(D_00507340, 0xC4, 0x1AE, 0xA, 4, 0xC);
        }
        Text_Printf(D_00507350, 0x190, 0x1AE, 0xA, 4, 7, 0xB, 0xA, 6);
        break;
    case 4:
        if (gFrameCount & 0x10) {
            Text_Printf(D_00507370, 0xC4, 0x1AE, 0xA, 4, 0xC);
        }
        Text_Printf(D_00507350, 0x190, 0x1AE, 0xA, 4, 7, 0xB, 0xA, 6);
        break;
    case 5:
    case 6:
    case 7:
        if (gFrameCount & 0x10) {
            Text_Printf(D_00507388, 0xC4, 0x1AE, 0xA, 4, 7);
        }
        Text_Printf(D_00507350, 0x190, 0x1AE, 0xA, 4, 7, 0xB, 0xA, 6);
        break;
    }
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005073C0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005073D0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005073E0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005073F0);

s32 func_003800D8(s32 n) {
    s32 r = n * ((rand() + D_005F6C08) & 0x7FFF);

    D_005F6C08 = D_005F6C08 * 5 + 3;
    return r >> 15;
}

/* func_003800D8 as the callers that inline it see it (the factor stays in a register over the rand() call). */
static inline s32 Cpu_RandBelow(s32 n) {
    s32 r = n * ((rand() + D_005F6C08) & 0x7FFF);

    D_005F6C08 = D_005F6C08 * 5 + 3;
    return r >> 15;
}

/* A CPU parameter block (0x80 bytes, CpuState + 0x344): D_00431918[level][kind] [I]. */
typedef struct {
    /* 0x00 */ s32 unk0[0x20];
} CpuParams;

extern CpuParams D_00431918[5][8];

/* An entry of the arcade difficulty table D_00432D18 (D_005F6130 entries) [I]. */
typedef struct {
    /* 0x0 */ u16 unk0; /* from this value of D_005F5CC0 on */
    /* 0x2 */ u16 unk2; /* level */
    /* 0x4 */ u16 unk4; /* lowest kind */
    /* 0x6 */ u16 unk6; /* highest kind */
} Unk432D18;

extern Unk432D18 D_00432D18[];
extern s32 D_005F6130;
extern s32 D_005F5CC0;
extern u8 D_005F5CCD;
extern CpuParams D_00432D90;

/* Picks the CPU's parameter block for gCpu [V code; I meaning]: in the option mode 4 from the first entry of
 * D_00432D18 that D_005F5CC0 reaches (its level, or level 1 half the time when it is not 0, and a kind drawn between
 * its bounds), in mode 3 level 1, otherwise the level of option byte 0xD; the kind is option word 0x24 unless
 * D_005F5CCD is 8 or more (kind 8). Out-of-range choices fall back to level 1, kind 6; the mode D_005F5CBC uses the
 * block D_00432D90. Keeps the level in unk3DC and the larger of unk37A / unk37E in unk25C. */
/* NON_MATCHING: 41 lines: the original addresses the table entry as &D_00432D18[i] + 2 / + 4 in registers and keeps `unk6 - (unk4 - 1)` unfolded (the D_005F5CCD test is duplicated per branch here as in the original) */
#if 0
void func_00380120(void) {
    u8 *opt = D_0043B7F8;
    u32 level = 0;
    u32 kind = 0;
    s32 i;
    s16 n;

    if (opt[0] == 4) {
        for (i = 0; i < D_005F6130; i++) {
            if (D_00432D18[i].unk0 >= D_005F5CC0) {
                if (!(func_003800D8(0xFF) & 1)) {
                    if (D_00432D18[i].unk2 != 0) {
                        level = 1;
                    } else {
                        level = D_00432D18[i].unk2;
                    }
                } else {
                    level = D_00432D18[i].unk2;
                }
                kind = D_00432D18[i].unk4 + func_003800D8(D_00432D18[i].unk6 - (D_00432D18[i].unk4 - 1));
                break;
            }
        }
    } else {
        if (opt[0] == 3) {
            level = 1;
            if (D_005F5CCD < 8) {
                kind = *(s32 *)&opt[0x24];
            } else {
                kind = 8;
            }
        } else {
            level = opt[0xD];
            if (D_005F5CCD < 8) {
                kind = *(s32 *)&opt[0x24];
            } else {
                kind = 8;
            }
        }
    }
    if (level >= 5 || kind >= 8) {
        level = 1;
        kind = 6;
    }
    if (D_005F5CBC != 0) {
        *(CpuParams *)gCpu->unk344 = D_00432D90;
    } else {
        *(CpuParams *)gCpu->unk344 = D_00431918[level][kind];
    }
    gCpu->unk3DC = level;
    n = gCpu->unk37A;
    if (n < gCpu->unk37E) {
        n = gCpu->unk37E;
    }
    gCpu->unk25C = n;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380120);
#endif

/* Selects the CPU state of `side` (gCpu) and copies the parameter block D_00431918[level][kind] into it; the indices
 * are clamped to 1, 4 and 7 [V]. */
void func_00380420(s32 side, s32 level, s32 kind) {
    CpuState *c;

    if (side >= 2) {
        side = 1;
    }
    if (level >= 5) {
        level = 4;
    }
    if (kind >= 8) {
        kind = 7;
    }
    c = &gCpuStates[side];
    gCpu = c;
    *(CpuParams *)c->unk344 = D_00431918[level][kind];
}

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

struct FighterMove **func_00368200(s32 side);
void *func_0038BEE0(s32 which, s32 key);

/* The CPU's attack parameters per character (D_00432E10, 12 bytes: CpuState + 0x3C4 for its own character, + 0x3D0
 * for the opponent's) [I]. */
typedef struct {
    /* 0x0 */ s16 unk0[6];
} Unk432E10;

extern Unk432E10 D_00432E10[];

/* Resets both CPU states at the start of a round [V code; I meaning]: sides, fighters, move records by id
 * (func_00368200), timers (-1) and counters (0), the remembered moves, the parameter block (func_00380120) and the
 * character rows of D_00432E10, the hooks of both characters (func_0038BEE0); in the mode D_005F5CBC the random
 * delays unk34C[1..4] are halved and unk364 quartered. The first wait unk94 is 60 plus a random share of unk34C[1],
 * unk5C 50-97; all candidates are marked unused. */
/* NON_MATCHING: 101 lines, mostly registers: the stores follow the original's order but gCpu and the
 * gCpuSelf pointer take a1 / a0 where the original uses v1 / a1, and the loop addresses are built in another order. */
#if 0
void func_00380568(Fighter *f) {
    u16 side;
    s32 i;
    CpuState *c;
    s32 r;

    for (side = 0; side < 2; side++) {
        c = &gCpuStates[side];
        gCpuOpponent = &gFighters[1 - side];
        c->unk4 = side;
        gCpuSelf = &gFighters[side];
        c->unk0 = side;
        gCpu = c;
        gCpu->unk10 = func_00368200(side);
        D_005F6C00[side] = func_00368200(side);
        c = gCpu;
        c->unk8 = 0;
        c->unk6 = 0;
        c->unk44 = -1;
        c->unk40 = gCpuSelf->unk3F4;
        c->unk48 = 0;
        c->unk54 = 0;
        c->unk70 = -1;
        c->unk74 = 0;
        c->unkCC = -1;
        c->unk6C = -1;
        c->unk4C = -1;
        c->unk50 = 0;
        c->unk88 = NULL;
        c->unk108 = NULL;
        c->unk84 = -1;
        c->unk58 = 0;
        c->unk68 = -1;
        c->unk64 = -1;
        c->unk78 = -1;
        c->unk3C = 0;
        c->unk38 = 0;
        c->unk60 = -1;
        c->unk90 = NULL;
        c->unk28 = 0;
        c->unk2C = 0;
        c->unkD0 = -1;
        c->unk324 = 0;
        c->unk320 = -1;
        c->unk328 = 0;
        c->unk9C = -1;
        c->unk98 = 0;
        c->unkB0 = 0;
        c->unkAC = 0;
        c->unk12C = 0;
        c->unk130 = 0;
        c->unk144 = 0;
        c->unk150 = 0;
        c->unk214 = 0;
        c->unk21C = 0;
        c->unk220 = 0;
        c->unk224 = 0;
        c->unk250 = 0;
        c->unk258 = 0;
        c->unkFC = 0;
        c->unk100 = 0;
        c->unk124 = -1;
        c->unk128 = NULL;
        c->unk260 = 0;
        c->unk268 = 0;
        c->unk26C = 0;
        c->unk270 = 0;
        c->unk274 = 0;
        c->unk278 = 0;
        c->unk27C[0] = 0;
        c->unk27C[1] = 0;
        c->unkC = NULL;
        c->unkA = 0;
        c->unkA4 = 0;
        c->unkA8 = NULL;
        c->unkA0 = 0;
        c->unk2C0 = -1;
        c->unk2C4 = 0;
        for (i = 0; i < 3; i++) {
            c->unk288[i] = 0;
        }
        for (i = 0; i < 0x14; i++) {
            c->unk2D0[i] = NULL;
        }
        c->unkF8 = 0;
        c->unkE4 = 0;
        c->unkE0 = 0;
        c->unkEC = 0;
        c->unkE8 = 0;
        c->unkF0 = 0;
        c->unkF4 = 0;
        func_00380120();
        *(Unk432E10 *)&gCpu->unk3C4 = D_00432E10[gCpuSelf->unk2A];
        *(Unk432E10 *)&gCpu->unk3D0 = D_00432E10[gCpuOpponent->unk2A];
        gCpu->unkB4 = func_0038BEE0(1, gFighters[1 - side].unk28);
        gCpu->unkB8 = func_0038BEE0(0, gFighters[side].unk28);
        if (D_005F5CBC != 0) {
            gCpu->unk34C[1] /= 2;
            gCpu->unk34C[2] /= 2;
            gCpu->unk34C[3] /= 2;
            gCpu->unk34C[4] /= 2;
            gCpu->unk364 /= 4;
        }
        r = ((rand() + D_005F6C08) & 0x7FFF) >> 8;
        D_005F6C08 = D_005F6C08 * 5 + 3;
        gCpu->unk94 = gCpu->unk34C[1] * r / 256 + 0x3C;
        gCpu->unk5C = Cpu_RandBelow(0x30) + 0x32;
        for (i = 0; i < 0xB4; i++) {
            gCpuCands[i].unk0 = -1;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380568);
#endif

void *func_0038BEE0(s32 which, s32 key);

/* Selects the CPU state of the side opposite to `side` (when that fighter is CPU-driven, unk418): gCpuSelf, gCpuOpponent,
 * gCpu, and the two per-character handlers of D_00507A80 for both characters [V]. */
void func_003809C0(s32 side) {
    Fighter *self = &gFighters[1 - side];

    if (self->unk418 != 0) {
        gCpuSelf = self;
        gCpuOpponent = &gFighters[side];
        gCpu = &gCpuStates[1 - side];
        gCpu->unkB4 = func_0038BEE0(1, gCpuOpponent->unk28);
        gCpu->unkB8 = func_0038BEE0(0, self->unk28);
    }
}

void func_00380A50(Fighter *f, u16 *held, u16 *pressed) {
    Cpu_GetInput(f, held, pressed);
}

s32 func_00393810(void);
s32 Cpu_Think(void);

void Cpu_GetInput(Fighter *f, u16 *held, u16 *pressed) {
    s16 side = f->unk20;
    u16 b;

    gCpu = &gCpuStates[side];
    gCpuSelf = &gFighters[side];
    gCpuOpponent = &gFighters[1 - side];
    if (func_00393810() == 0) {
        Cpu_Think();
        b = gCpu->unk6;
        *pressed = b & (b ^ gCpu->unk8);
        gCpu->unk8 = b;
        *held = b;
    } else {
        *pressed = 0;
        *held = 0;
    }
}

/* NON_MATCHING: about 156 lines differ, nearly all register names: the reloaded gCpu goes to a3 where the
 * original uses t0 (a3 holds the 0xBA9 flag there), which shifts the allocation of the whole function. */
#if 0
/* Per frame after Cpu_Think: counts the CPU timers down, notes the opponent's character 0xE moves 0xBA9 / 0x1A6 and
 * the throw-like own move kinds 0x1052 / 0x2829 (unk58), resets the plans when the CPU's fighter is hit (unk12F) or
 * the opponent's unk133 is set, keeps the opponent's moves that took life in unk288 (unk298: the last three were the
 * same move), and raises the chances unk364 (+10, at most unk366), unk368 (+10, at most unk36A) and, while unk2C0 is
 * positive, unk372 (+40, at most unk374) and unk36E (+20, at most unk370) each time the CPU is hit outside the mode
 * D_005F5CBC [V code; I meaning]. */
void func_00380B10(void) {
    u32 fl = gCpu->unk330.w;
    struct FighterMove *m = gCpu->unk8C;
    u16 lo;
    s32 t;
    s32 i;
    CpuState *c;

    gCpu->unk54 = (fl & 0x205) == 1;
    if (m != NULL && gCpu->unk6C < 0 && m->unk19 != 0) {
        gCpu->unk6C = m->unk19;
    }
    if (gCpu->unk6C >= 0) {
        gCpu->unk6C--;
    }
    if (gCpu->unk4C >= 0) {
        gCpu->unk4C--;
    }
    if (gCpu->unk5C >= 0) {
        gCpu->unk5C--;
    }
    if (gCpu->unk84 >= 0) {
        gCpu->unk84--;
    }
    if (gCpu->unk68 >= 0) {
        gCpu->unk68--;
    }
    if (gCpu->unk64 >= 0) {
        gCpu->unk64--;
    }
    if (gCpu->unk60 >= 0) {
        gCpu->unk60--;
    }
    if (gCpu->unkCC >= 0) {
        gCpu->unkCC--;
    }
    if (gCpu->unkD0 >= 0) {
        gCpu->unkD0--;
    }
    if (gCpu->unk320 >= 0) {
        gCpu->unk320--;
    }
    gCpu->unk2B8 = gCpu->unk2AC;
    t = 0;
    if (gCpuOpponent->unk28 == 0xE && (gCpuOpponent->unk10C == 0xBA9 || gCpuOpponent->unk10C == 0x1A6)) {
        t = 1;
    }
    gCpu->unk288[3] = t;
    lo = fl;
    if (gCpu->unkA4 != 0 && gCpu->unkA8 != gCpuSelf->unkC0) {
        gCpu->unkA4 = 0;
        gCpu->unkA8 = NULL;
    }
    if (gCpu->unk58 != 0) {
        gCpu->unk58 = lo == 0x1052 || lo == 0x2829;
        if (gCpu->unk58 == 0 && gCpu->unkF4 != 0) {
            gCpu->unkF4 = 0;
        }
    } else {
        gCpu->unk58 = lo == 0x1052 || lo == 0x2829;
        gCpu->unkF4 = 0;
    }
    if (gCpuSelf->unk12F != 0) {
        gCpu->unk5C = 0x28;
        gCpu->unk6C = -1;
        gCpu->unk100 = 0;
        gCpu->unk50 = 0;
        gCpu->unk4C = 0;
        gCpu->unk220 = 0;
        gCpu->unk12C = 0;
        gCpu->unk14C = 0;
    }
    if (gCpuSelf->unk133 != 0 && gCpu->unk24 < 3) {
        gCpu->unk84 = 0x1E;
    }
    if (gCpuOpponent->unk133 != 0 && gCpu->unk20 < 2 && (rand() & 0xFFF) < gCpu->unk368) {
        gCpu->unk94 -= 3;
    }
    if ((rand() & 0xFFF) >= gCpu->unk38E && gCpuSelf->unk3F4 < gCpu->unk40) {
        s32 om;

        gCpu->unk94 -= 3;
        om = (s32)gCpuOpponent->unkC0;
        for (i = 3; i > 0; i--) {
            gCpu->unk288[i] = gCpu->unk288[i - 1];
        }
        gCpu->unk288[0] = om;
        gCpu->unk298 = om == gCpu->unk288[1] && om == gCpu->unk288[2];
    }
    gCpu->unk40 = gCpuSelf->unk3F4;
    if (gCpuSelf->unk12F != 0 && D_005F5CBC == 0) {
        c = gCpu;
        c->unk5C = 0x28;
        c->unk6C = -1;
        c->unk50 = 0;
        c->unk4C = 0;
        c->unk364 += 10;
        c->unk368 += 10;
        c->unk364 = c->unk364 < c->unk366 ? c->unk364 : c->unk366;
        c->unk368 = c->unk368 < c->unk36A ? c->unk368 : c->unk36A;
        if (c->unk2C0 != 0 && c->unk2C0 > 0) {
            c->unk372 += 40;
            c->unk36E += 20;
            c->unk372 = c->unk372 < c->unk374 ? c->unk372 : c->unk374;
            c->unk36E = c->unk36E < c->unk370 ? c->unk36E : c->unk370;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380B10);
#endif

extern s32 D_005F6120;
extern u16 D_005F5CBC;
extern s32 D_005F6144;
s32 func_00381D30(void);
s32 func_00381BF8(void);
s32 func_00388DE0(void);
s32 func_00381408(void);
s32 func_00388FA8(void);
s32 func_0038A408(void);
s32 func_0038AD00(void);
s32 func_0038A500(void);
s32 func_0038AC40(void);
s32 func_00388170(void);
s32 func_00388DA8(void);
s32 func_00385C80(void);
s32 func_0038ABE8(void);
s32 func_003864E8(void);
s32 func_00389B70(void);
s32 func_00389D58(void);
s32 func_0038A780(void);
s32 func_00387600(void);
s32 func_003879A0(void);
s32 func_00387DA8(void);
s32 func_0038A928(void);
s32 func_00388278(void);
s32 func_00387120(void);
s32 func_00388840(void);
s32 func_00388D70(void);
s32 func_00381418(void);
void func_00380B10(void);

/* One frame of the CPU side gCpu: notes the move frames and flags of both fighters, adds the damage taken to
 * unk27C, then lets the first decider of a fixed priority list that acts choose the input; D_005F6120 keeps which
 * one (0 while a command is still playing) [V]. */
s32 Cpu_Think(void) {
    s32 frame;
    s32 prev;

    gCpu->unk8C = 0;
    prev = gCpu->unk44;
    frame = gCpuSelf->unkC4;
    gCpu->unk44 = frame;
    if (frame != prev) {
        gCpu->unk4++;
    }
    gCpu->unk32C = gCpuSelf->unkC0->unk8;
    gCpu->unk330.w = gCpuSelf->unkC0->unk4;
    gCpu->unk334 = (u16)gCpuSelf->unkC0->unk8;
    gCpu->unk338.w = gCpuOpponent->unkC0->unk8;
    gCpu->unk33C.w = gCpuOpponent->unkC0->unk4;
    gCpu->unk340 = (u16)gCpuOpponent->unkC0->unk8;
    gCpu->unk27C[gCpuSelf->unk0] += gCpuSelf->unk144 << 12;
    if (func_00381418() != 0) {
        D_005F6120 = 0;
    } else if (func_00381D30() != 0) {
        D_005F6120 = 1;
    } else if (func_00381BF8() != 0) {
        D_005F6120 = 2;
    } else if (func_00388DE0() != 0) {
        D_005F6120 = 3;
    } else if (func_00381408() != 0) {
        D_005F6120 = 5;
    } else if (func_00388FA8() != 0) {
        D_005F6120 = 4;
    } else if (func_0038A408() != 0) {
        D_005F6120 = 6;
    } else if (func_0038AD00() != 0) {
        D_005F6120 = 0x16;
    } else if (func_0038A500() != 0) {
        D_005F6120 = 7;
    } else if (func_0038AC40() != 0) {
        D_005F6120 = 0x17;
    } else if (func_00388170() != 0) {
        D_005F6120 = 8;
    } else if (func_00388DA8() != 0) {
        D_005F6120 = 9;
    } else if (func_00385C80() != 0) {
        D_005F6120 = 0xA;
    } else if (func_0038ABE8() != 0) {
        D_005F6120 = 0x16;
    } else if (func_003864E8() != 0) {
        D_005F6120 = 0xB;
    } else if (func_00389B70() != 0) {
        D_005F6120 = 0xC;
    } else if (func_00389D58() != 0) {
        D_005F6120 = 0xD;
    } else if (func_0038A780() != 0) {
        D_005F6120 = 0x14;
    } else if (func_00387600() != 0) {
        D_005F6120 = 0xE;
    } else if (func_003879A0() != 0) {
        D_005F6120 = 0xF;
    } else if (func_00387DA8() != 0) {
        D_005F6120 = 0x10;
    } else if (func_0038A928() != 0) {
        D_005F6120 = 0x15;
    } else if (func_00388278() != 0) {
        D_005F6120 = 0x11;
    } else if (func_00387120() != 0) {
        D_005F6120 = 0x12;
    } else if (func_00388840() != 0) {
        D_005F6120 = 0x13;
    } else if (func_00388D70() != 0) {
        D_005F6120 = 0x63;
    }
    if (((gCpuSelf->unk2A == 0x21 && gCpuSelf->unk401 == 0) || (D_005F5CBC != 0 && D_005F6144 == 0)) &&
        (gCpu->unk6 & 4)) {
        gCpu->unk6 = 0;
    }
    func_00380B10();
    return 0;
}

extern const u16 D_005073F0[];

/* Starts a command of the CPU: the buttons of its first word (bits 8-11: pad 0x80, 0x10, 0x40, 0x20) and its
 * direction (bits 0-3, mapped through D_005073F0) are held this frame, the rest of the list plays from unkC [V]. */
s32 Cpu_SetCommand(u16 *cmd) {
    u16 c = *cmd++;
    s16 w;
    u16 b;

    if (*cmd == 0) {
        gCpu->unkC = NULL;
    } else {
        gCpu->unkC = cmd;
    }
    w = c;
    b = 0;
    if (w & 0x100) {
        b = 0x80;
    }
    if (w & 0x200) {
        b |= 0x10;
    }
    if (w & 0x400) {
        b |= 0x40;
    }
    if (w & 0x800) {
        b |= 0x20;
    }
    b |= D_005073F0[w & 0xF];
    gCpu->unk6 = b;
    return 0;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507410);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507428);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507440);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507448);

/* NON_MATCHING: 13 lines differ: gCpu and the %hi of D_00507410 take t4 / t3 the other way round, and the two
 * ors of the button loop have their operands swapped. */
#if 0
extern const u16 D_00507410[10];
extern const u16 D_00507428[10];
extern const u16 D_00507440[4];
extern const u16 D_00507448[];
u16 *func_0036CF30(u16 code);

/* Starts the input of a move command for the CPU: 0x8003 arms the fighter's unk12B, the codes 0x8001, 0x8002 and those
 * above 0x8005 play their command list (func_0036CF30), any other code is turned into pad bits at once: the highest
 * direction bit of (code >> 5) & 0x1FF through D_00507410 / D_00507428, the four button bits (D_00507440) and bit 14
 * (D_00507448). The move is kept in gCpu->unk8C [V]. */
s32 func_00381278(u16 cmd, struct FighterMove *m) {
    s32 r;
    s32 i;
    u16 dir;
    s32 bits;
    u16 n;
    u16 d;
    s32 hold;

    if (cmd == 0x8003) {
        gCpuSelf->unk12B = 1;
        gCpu->unk6 = 0;
        return 1;
    }
    if (cmd > 0x8005 || cmd == 0x8001 || cmd == 0x8002) {
        Cpu_SetCommand(func_0036CF30(cmd) + 1);
        r = -1;
    } else {
        dir = (cmd >> 5) & 0x1FF;
        hold = (cmd >> 14) & 1;
        bits = 0;
        for (i = 9; i > 0; i--) {
            if (D_00507410[i] & dir) {
                break;
            }
        }
        d = D_00507428[i];
        for (i = 0; i < 4; i++) {
            if (cmd & 1) {
                n = D_00507440[i] | bits;
            } else {
                n = bits;
            }
            bits = n;
            cmd >>= 1;
        }
        if (hold) {
            bits |= D_00507448[0];
        }
        r = gCpu->unk6 = d | bits;
    }
    gCpu->unk8C = m;
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381278);
#endif

s32 func_00381408(void) {
    return gCpuSelf->unk188 != NULL;
}

/* Steps the CPU's command: a queued word unkA first, else the next word of the list unkC (the list ends at a 0
 * word), decoded as in Cpu_SetCommand; returns 0 when nothing is left [V]. */
s32 func_00381418(void) {
    CpuState *s = gCpu;
    s16 *p;
    u16 c;
    s16 w;
    u16 b;

    if (s->unkA != 0) {
        s->unk6 = s->unkA;
        s->unkA = 0;
        return 1;
    }
    p = (s16 *)s->unkC;
    if (p == NULL) {
        return 0;
    }
    c = p[0];
    s->unkC = (u16 *)(p + 1);
    if (p[1] == 0) {
        s->unkC = NULL;
    }
    w = c;
    b = 0;
    if (w & 0x100) {
        b = 0x80;
    }
    if (w & 0x200) {
        b |= 0x10;
    }
    if (w & 0x400) {
        b |= 0x40;
    }
    if (w & 0x800) {
        b |= 0x20;
    }
    b |= D_005073F0[w & 0xF];
    gCpu->unk6 = b;
    return 1;
}

s32 Cmd_CheckCondition(Fighter *f, Fighter *o, s16 id);
s32 Cmd_CheckSituation(MoveCmd *c, Fighter *f, Fighter *o);
void func_00385250(CpuCmd *c, s32 n);

/* Inlined by ten CPU functions: whether the command may be used now. A few moves depend on the fighter's unk28
 * (variant) and on chance: 0xEE3 not with variant 4 in the mode D_005F5CBC, 0x181 / 0x197 only with unk3DC set, 0x4CA
 * never, 0x3BC (variant 0) and 0x1245 / 0x1253 (variants 0, 0xD) one time in 8, 0x1246 / 0x1255 not with variants
 * 0 / 0xD, 0x1262 seven times in 8 with variant 4 [V code; I meaning]. */
static inline s32 Cpu_IsCmdAllowed(CpuCmd *c) {
    s32 r = 1;

    switch (c->unk6) {
    case 0xEE3:
        r = !(D_005F5CBC != 0 && gCpuSelf->unk28 == 4);
        break;
    case 0x1262:
        if (gCpuSelf->unk28 == 4) {
            r = (rand() & 0x70) != 0;
        }
        break;
    case 0x1245:
    case 0x1253:
        if (gCpuSelf->unk28 == 0 || gCpuSelf->unk28 == 0xD) {
            r = (rand() & 0x70) == 0;
        }
        break;
    case 0x3BC:
        if (gCpuSelf->unk28 == 0) {
            r = (rand() & 0x70) == 0;
        }
        break;
    case 0x1246:
    case 0x1255:
        if (gCpuSelf->unk28 == 0 || gCpuSelf->unk28 == 0xD) {
            return 0;
        }
        break;
    case 0x4CA:
        return 0;
    case 0x181:
    case 0x197:
        return gCpu->unk3DC != 0;
    }
    return r;
}

/* Used by the CPU's command-list walkers (a macro: the test is not turned into a value): whether the command entry is
 * for the fighter's variant unk28 (condition 0: any, 1-0x21: variant + 1, 0x22-0x42: variant + 0x22) [V code; I
 * meaning]. */
#define CPU_CMD_FOR_VARIANT(c)                                                                                         \
    ((c)->unk2 == 0 || ((c)->unk2 < 0x22 && gCpuSelf->unk28 == (c)->unk2 - 1) ||                                    \
     ((c)->unk2 < 0x43 && gCpuSelf->unk28 == (c)->unk2 - 0x22))

extern CpuCmd D_0047F780[];
s32 func_00384D68(u16 *cmd);

/* Returns the input code of one command, picked at random (func_003800D8), of the own move's command list (looking
 * into the shared lists of 0x8004) that is inside its frame window, for the fighter's variant, allowed
 * (Cpu_IsCmdAllowed) and whose situation holds or whose kind (unk3) is 0x3D-0x3F; those kinds give the codes 1 / 2 / 3
 * instead (0 with none) [V]. */
s32 func_003814C8(void) {
    CpuCmd *list[16];
    CpuCmd **out = list;
    CpuCmd *e = (CpuCmd *)gCpuSelf->unkC0->unkC;
    s32 n = 0;
    s32 cnt;
    CpuCmd *c;
    s32 j;
    s32 ok;
    Fighter *f;
    CpuCmd *p;

    for (; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = &D_0047F780[e->unk6];
        } else {
            cnt = 1;
            c = e;
        }
        for (j = 0; j < cnt; j++, c++) {
            f = gCpuSelf;
            ok = 0;
            if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && CPU_CMD_FOR_VARIANT(c) && Cpu_IsCmdAllowed(c) &&
                (Cmd_CheckSituation((MoveCmd *)c, f, gCpuOpponent) || c->unk3 == 0x3F || c->unk3 == 0x3D ||
                 c->unk3 == 0x3E)) {
                ok = 1;
            }
            if (ok) {
                *out++ = c;
                n++;
            }
        }
    }
    if (n > 0) {
        p = list[func_003800D8(n)];
        if (p->unk3 == 0x3F) {
            return 3;
        }
        if (p->unk3 == 0x3D) {
            return 1;
        }
        if (p->unk3 == 0x3E) {
            return 2;
        }
        return p->unk0;
    }
    return 0;
}

/* Lists in `list` up to six commands of the own move's command list (looking into the shared lists of 0x8004) that
 * are for the fighter's variant, allowed (Cpu_IsCmdAllowed) and whose situation holds; commands with bit 0x4000 only
 * when unk3E4 is set; returns their number [V]. */
s32 func_00381818(CpuCmd **list) {
    CpuCmd *e = (CpuCmd *)gCpuSelf->unkC0->unkC;
    s32 cnt;
    CpuCmd *c;
    s32 j;
    s32 n = 0;

    for (; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = &D_0047F780[e->unk6];
        } else {
            cnt = 1;
            c = e;
        }
        for (j = 0; j < cnt; j++, c++) {
            if (CPU_CMD_FOR_VARIANT(c) && Cpu_IsCmdAllowed(c) &&
                Cmd_CheckSituation((MoveCmd *)c, gCpuSelf, gCpuOpponent)) {
                if (!func_00384D68(&c->unk0) || (gCpu->unk3E4 != 0 && func_00384D68(&c->unk0))) {
                    *list++ = c;
                    if (++n >= 6) {
                        break;
                    }
                }
            }
        }
    }
    return n;
}

s32 func_00381818(CpuCmd **list);

/* While unkFC is below unk356 - 1, with a chance of 0xB33 in 4096: picks one of the commands func_00381818 lists at
 * random (the seeded rand of func_003800D8) and plans it for frame cmd->unkB + 1 of the own move (unk124 / unk128);
 * with none listed, unkFC becomes -1 [V]. */
void func_00381B10(void) {
    CpuCmd *list[8];
    s32 n;
    s32 x;

    if (gCpu->unkFC >= 0 && gCpu->unkFC < gCpu->unk356 - 1 && (rand() & 0xFFF) < 0xB33) {
        n = func_00381818(list);
        if (n > 0) {
            gCpu->unk108 = gCpuSelf->unkC0;
            gCpu->unkFC++;
            x = n * ((rand() + D_005F6C08) & 0x7FFF);
            D_005F6C08 = D_005F6C08 * 5 + 3;
            gCpu->unk124 = list[x >> 15]->unkB + 1;
            gCpu->unk128 = list[x >> 15];
        } else {
            gCpu->unk128 = NULL;
            gCpu->unkFC = -1;
            gCpu->unk124 = -1;
        }
    }
}

void func_00381B10(void);
s32 func_003814C8(void);
s32 func_00381278(u16 cmd, struct FighterMove *m);

/* Decider 2 of Cpu_Think: while the CPU's fighter has unkE0 set. Positive: plays the command planned by func_00381B10
 * at its frame (planning a new one when the own move changed). Negative: once per own move, with a chance of unk35A in
 * 4096, starts the command func_003814C8 returns. Clears the plan when unkE0 is 0 (returns 0) [V]. */
s32 func_00381BF8(void) {
    CpuCmd *c;
    s32 same;
    s32 n;

    if (gCpuSelf->unkE0 == 0) {
        gCpu->unk128 = NULL;
        gCpu->unk108 = NULL;
        gCpu->unkFC = 0;
        gCpu->unk124 = -1;
        return 0;
    }
    gCpu->unk5C = 0x28;
    gCpu->unk6C = -1;
    gCpu->unk50 = 0;
    gCpu->unk4C = 0;
    same = gCpuSelf->unkC0 == gCpu->unk108;
    if (gCpuSelf->unkE0 > 0) {
        if (!same) {
            func_00381B10();
        }
        if (gCpuSelf->unkC4 == gCpu->unk124) {
            c = gCpu->unk128;
            func_00381278(c->unk0, D_005F6C00[(s16)gCpuSelf->unk20][c->unk6]);
        } else {
            gCpu->unk6 = 0;
        }
    } else if (same) {
        gCpu->unk6 = 0;
    } else {
        gCpu->unk108 = gCpuSelf->unkC0;
        gCpu->unkFC--;
        if ((rand() & 0xFFF) < gCpu->unk35A) {
            n = func_003814C8();
            if (n != 0) {
                func_00381278(n, NULL);
            } else {
                gCpu->unk6 = 0;
            }
        }
    }
    return 1;
}

extern const s16 gSinTable[];
s32 func_0038ACA8(void);

/* Decider 1 of Cpu_Think: the per-frame analysis, always 0 [V code; I meaning; m2c's control flow with fixed types].
 * It refreshes the CPU's view: the chance block zeroed when the opponent's red life is a tenth of its life or more,
 * whether the own / the opponent's move changed (unk1C / unk29C, unk3C counts the frames of the own move), the
 * window counters unk258 / unk2C0, the distance D_005F5C28 and its change unk34 with a wobble unk28 from gSinTable,
 * the range class unk20 (0-5, against unk3C4-unk3CA) and the distance class unk24 (1-5, unk3CE-unk3D6), the
 * opponent's attack state (unk2AC, unk2B0, unk2B4, unk2BC, unk2A8 frames to its window, unkC0 / unkC4 counters),
 * unk98 (the own move answers the opponent's kind: 0x2021 / 0x2829 / 0x1052 against 0x412 / 0x217 / 0x10F / 0x31F /
 * 0x706), the waits unk70 / unk94, unk7C / unk80, the life comparison unk3E0 / unk3E4 (team life of D_00440940, in
 * practice the CPU's own damage unk27C), and remembers the opponent's last 20 moves in unk2D0. */
/* NON_MATCHING: m2c's control flow with fixed types (about 490 lines differ, mostly registers and block order); a
 * starting point only. */
#if 0
s32 func_00381D30(void) {
    CpuState *temp_t1;
    CpuState *var_t1;
    s16 temp_a0_4;
    s16 temp_t3;
    s16 temp_t4;
    s16 temp_v1_5;
    s32 *temp_v0_9;
    s32 *temp_v1_10;
    s32 temp_a1;
    s32 temp_a2;
    s32 temp_a2_2;
    s32 temp_a2_3;
    s32 temp_a2_4;
    s32 temp_a2_5;
    s32 temp_a3;
    s32 temp_s0;
    s32 temp_s1;
    s32 temp_v0_10;
    s32 temp_v0_3;
    s32 temp_v0_4;
    s32 temp_v0_5;
    s32 temp_v0_6;
    s32 temp_v0_8;
    s32 temp_v1;
    s32 temp_v1_2;
    s32 var_a0;
    s32 var_a0_2;
    s32 var_a1;
    s32 var_a1_2;
    s32 var_a1_3;
    s32 var_a2;
    s32 var_a2_2;
    s32 var_a2_3;
    s32 var_a2_4;
    s32 var_a3;
    s32 var_t0;
    s32 var_t2;
    s32 var_v0;
    s32 var_v0_10;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v0_5;
    s32 var_v0_6;
    s32 var_v0_7;
    s32 var_v0_8;
    s32 var_v0_9;
    struct FighterMove *temp_a0;
    struct FighterMove *temp_a0_3;
    struct FighterMove *temp_v0_11;
    struct FighterMove *temp_v1_3;
    struct FighterMove *temp_v1_4;
    u16 temp_a0_2;
    u32 temp_v1_6;
    u32 temp_v1_7;
    u32 temp_v1_8;
    u32 var_v0_4;
    u8 temp_v0;
    u8 temp_v0_2;

    gCpu->unkBC = NULL;
    if (!(gCpuSelf->unkC0->unk24 & 0x20)) {
        gCpu->unk144 = 0;
    }
    gCpu->unkE4 = gCpu->unkE0;
    gCpu->unkEC = gCpu->unkE8;
    gCpu->unkE0 = 0;
    gCpu->unkE8 = 0;
    if (((s32) gCpuOpponent->unk3F8 / 10) >= gCpuOpponent->unk3F4) {
        gCpu->unk36A = 0;
        gCpu->unk360 = 0;
        gCpu->unk362 = 0;
        gCpu->unk368 = 0;
    }
    if (((rand() & 0xFFF) >= gCpu->unk38E) && ((s16) gCpuSelf->unk40 >= 0x2000) &&
        ((s16) gCpuOpponent->unk40 < 0x6001) && (gCpu->unk24 < 3)) {
        gCpu->unkE4 = 1;
    }
    var_a1 = 0;
    if ((gCpuSelf->unkC0 != gCpu->unk88) || (gCpuSelf->unkC4 < gCpu->unk78)) {
        var_a1 = 1;
    }
    gCpu->unk1C = var_a1;
    var_a2 = 0;
    if ((gCpuOpponent->unkC0 != (struct FighterMove *) gCpu->unk284) || (gCpuOpponent->unkC4 < gCpu->unk2A0)) {
        var_a2 = 1;
    }
    gCpu->unk29C = var_a2;
    if (gCpu->unk88 != gCpuSelf->unkC0) {
        gCpu->unk3C = 0;
    } else {
        gCpu->unk3C += 1;
    }
    gCpu->unk88 = gCpuSelf->unkC0;
    gCpu->unk78 = (s32) gCpuSelf->unkC4;
    if (gCpu->unk330.w & 0x400000) {
        gCpu->unk6C = -1;
        gCpu->unk5C = 0x28;
        gCpu->unk50 = 0;
        gCpu->unk4C = 0;
    }
    temp_v0 = gCpuSelf->unkC0->unk2C;
    if (temp_v0 != 0) {
        if (temp_v0 == gCpuSelf->unkC4) {
            gCpu->unk258 += 1;
        } else if (temp_v0 == 0) {
            goto block_25;
        }
    } else {
    block_25:
        if (!(gCpu->unk32C & 0x80000)) {
            if (gCpuOpponent->unkF3 == 0) {
                gCpu->unk258 = 0;
            }
        }
    }
    temp_v0_2 = gCpuOpponent->unkC0->unk2C;
    if (temp_v0_2 != 0) {
        if (temp_v0_2 == gCpuOpponent->unkC4) {
            gCpu->unk2C0 += 1;
        } else if (temp_v0_2 == 0) {
            goto block_35;
        }
    } else {
    block_35:
        if (!(gCpu->unk338.w & 0x80000) && (gCpuSelf->unkF3 == 0)) {
            gCpu->unk2C0 = -1;
        }
    }
    temp_a2 = gCpu->unk2C + (rand() & 0x3F);
    gCpu->unk2C = temp_a2;
    temp_v1 = gSinTable[temp_a2 & 0xFFF] * 0xC0;
    gCpu->unk34 = D_005F5C28 - gCpu->unk30;
    gCpu->unk30 = (s32) D_005F5C28;
    temp_v0_3 = temp_v1 / 4096;
    temp_a2_2 = ((D_005F5C28 - gCpu->unk3DA) + temp_v0_3) - 0x80;
    gCpu->unk28 = temp_v0_3;
    if (temp_a2_2 < gCpu->unk3C4) {
        if (gCpuSelf->unk3E8 == 0) {
            if (gCpuSelf->unk3F0 == 0) {
                if ((gCpuOpponent->unk3E8 != 0) || (gCpuOpponent->unk3F0 != 0)) {
                    goto block_43;
                }
                gCpu->unk20 = 1;
                goto block_47;
            }
            goto block_44;
        }
    block_43:
    block_44:
        gCpu->unk20 = 0;
        if (gCpu->unk7C == 0) {
            gCpu->unk94 -= 1;
        block_47:
            var_t1 = gCpu;
            if (var_t1->unk7C != 0) {
                var_t1->unk38 = 0;
            } else {
                var_t1->unk94 -= 1;
                goto block_52;
            }
        } else {
            var_t1 = gCpu;
        block_52:
            var_t1->unk38 = 0;
        }
    } else {
        var_v0 = 2;
        if ((temp_a2_2 < gCpu->unk3C6) || (var_v0 = 3, ((temp_a2_2 < gCpu->unk3C8) != 0))) {
            gCpu->unk38 = 0;
            gCpu->unk20 = var_v0;
        } else if (temp_a2_2 < gCpu->unk3CA) {
            gCpu->unk20 = 4;
            temp_a1 = gCpu->unk38 + 4 + ((u32) ((rand() + D_005F6C08) & 0x7FFF) >> 0xE);
            D_005F6C08 = (D_005F6C08 * 5) + 3;
            gCpu->unk38 = temp_a1;
        } else {
            gCpu->unk20 = 5;
            temp_v0_4 = ((rand() + D_005F6C08) & 0x7FFF) * 3;
            D_005F6C08 = (D_005F6C08 * 5) + 3;
            gCpu->unk38 = gCpu->unk38 + 5 + (temp_v0_4 >> 0xF);
        }
    }
    if ((gCpu->unk7C == 0) || (gCpuOpponent->unk135 != 0)) {
        gCpu->unk94 -= 1;
    }
    temp_v1_2 = gCpu->unk34;
    temp_a2_3 = (D_005F5C28 - gCpu->unk3CE) + gCpu->unk28;
    temp_a2_4 = (temp_v1_2 <= -1) ? (temp_a2_3 + temp_v1_2) : temp_a2_3;
    var_v0_2 = 1;
    if (temp_a2_4 >= (gCpu->unk3D0 + 0x80)) {
        var_v0_2 = 2;
        if (temp_a2_4 >= (gCpu->unk3D2 + 0x100)) {
            var_v0_2 = 3;
            if (temp_a2_4 >= (gCpu->unk3D4 + 0x200)) {
                var_v0_2 = 4;
                if (temp_a2_4 >= (gCpu->unk3D6 + 0x280)) {
                    var_v0_2 = 5;
                }
            }
        }
    }
    gCpu->unk24 = var_v0_2;
    if ((gCpuOpponent->unkC0->unk2C == 0) && (gCpu->unk33C.w & 0x80000)) {
        gCpu->unk2B0 += 1;
    } else {
        gCpu->unk2B0 = 0;
    }
    var_a2_2 = 0;
    if ((gCpu->unk2B0 > 0) || ((temp_v1_3 = gCpuOpponent->unkC0, (temp_v1_3->unk2C != 0)) &&
                               ((temp_v1_3->unk2D + 1) >= gCpuOpponent->unkC4))) {
        var_a2_2 = 1;
    }
    gCpu->unk2AC = var_a2_2;
    if ((gCpu->unk2AC != 0) && (gCpuOpponent->unkC4 < (gCpuOpponent->unkC0->unk2C - 3))) {
        gCpu->unkC0 += 1;
    } else {
        gCpu->unkC0 = 0;
    }
    if ((gCpuSelf->unkC0->unk2C != 0) && ((gCpuOpponent->unkC0->unk2D + 1) < gCpuOpponent->unkC4)) {
        gCpu->unkC4 += 1;
    } else {
        gCpu->unkC4 = 0;
    }
    if (gCpu->unk2AC != 0) {
        temp_v0_5 = gCpu->unk9C;
        var_v0_3 = temp_v0_5 - 1;
        if (temp_v0_5 > 0) {
            goto block_88;
        }
    } else {
        var_v0_3 = -1;
    block_88:
        gCpu->unk9C = var_v0_3;
    }
    var_v0_4 = gCpu->unk33C.w;
    if (gCpu->unk2AC == 0) {
        if (var_v0_4 & 0x200000) {
            gCpu->unk2AC = 1;
        }
        var_v0_4 = gCpu->unk33C.w;
    }
    if (!(var_v0_4 & 0x80000)) {
        gCpu->unk2B4 = 0;
    } else {
        gCpu->unk2B4 += 1;
    }
    var_a2_3 = 0;
    if ((gCpu->unk338.w & 0x400000) && ((temp_v1_4 = gCpuOpponent->unkC0, (temp_v1_4->unk2C == 0)) ||
                                        ((s32) temp_v1_4->unk2D >= gCpuOpponent->unkC4))) {
        var_a2_3 = 1;
    }
    gCpu->unk2BC = var_a2_3;
    if ((gCpu->unk2AC != 0) || (var_v0_5 = 0x3E7, (gCpu->unk2BC != 0))) {
        temp_a0 = gCpuOpponent->unkC0;
        if (temp_a0->unk2C != 0) {
            temp_v1_5 = gCpuOpponent->unkC4;
            var_v0_6 = 0x3E5;
            if ((s32) temp_a0->unk2D >= temp_v1_5) {
                temp_v0_6 = temp_a0->unk2C - temp_v1_5;
                gCpu->unk2A8 = temp_v0_6;
                if (temp_v0_6 >= 0x65) {
                    var_v0_6 = 0x3E6;
                    goto block_107;
                }
            } else {
            block_107:
                gCpu->unk2BC = 0;
                gCpu->unk2A8 = var_v0_6;
                gCpu->unk2AC = 0;
            }
        } else {
            gCpu->unk2A8 = 0xA;
            var_v0_5 = (((MoveCmd *) gCpuOpponent->unkC0->unkC)->unkC - gCpuOpponent->unkC4) + 1;
            goto block_110;
        }
    } else {
        gCpu->unkF0 = 0;
    block_110:
        gCpu->unk2A8 = var_v0_5;
    }
    gCpu->unk2CC = gCpuOpponent->unk70C < -0xC1B;
    gCpu->unk2C4 = gCpuOpponent->unk70C;
    if (((s16) gCpuOpponent->unk40 >= 0x3001) ||
        (!(gCpu->unk33C.w & 0x200000) && (temp_v1_6 = gCpu->unk340, (temp_v1_6 != 0x607)) && (temp_v1_6 != 0x706))) {
        gCpu->unkF8 = 0;
    }
    if ((gCpu->unk84 >= 0) && (gCpu->unk24 < 2) && ((rand() & 0xFFF) >= gCpu->unk38E)) {
        gCpu->unkF0 = 1;
    }
    temp_v1_7 = gCpu->unk33C.w;
    var_a0 = 0;
    if ((temp_v1_7 & 0x204) && ((temp_v1_7 & 0xFFFF) != 0x4C02)) {
        var_a0 = gCpuOpponent->unk135 == 0;
    }
    gCpu->unk2A4 = var_a0;
    var_a1_2 = 0;
    if (gCpuOpponent->unk28 == 4) {
        var_a1_2 = gCpuOpponent->unk10C == 0x5C7;
    }
    gCpu->unkC8 = var_a1_2;
    if (var_a1_2 != 0) {
        gCpu->unk2C0 = -1;
    }
    temp_a0_2 = gCpu->unk330.h;
    if (((u32) D_005F5C28 < 0xB87U) && (gCpu->unkC8 == 0)) {
        if (gCpu->unk2BC != 0) {
            if (gCpu->unk340 != 0x217) {
                if (temp_a0_2 != 0x2021) {
                    var_v0_7 = 0x2829;
                    goto block_161;
                }
                goto block_164;
            }
            goto block_166;
        }
        if ((gCpu->unk2AC != 0) && ((s16) gCpuSelf->unk40 < 0x3001)) {
            temp_v1_8 = gCpu->unk340;
            if (temp_v1_8 != 0x412) {
                if (temp_v1_8 < 0x413U) {
                    if (temp_v1_8 != 0x217) {
                        if (temp_v1_8 < 0x218U) {
                            if (temp_v1_8 != 0x10F) {
                                gCpu->unk98 = 0;
                            } else {
                                gCpu->unk98 = temp_a0_2 == 0x2829;
                            }
                        } else if (temp_v1_8 != 0x31F) {
                            gCpu->unk98 = 0;
                        } else if (temp_a0_2 != 0x2829) {
                            if (temp_a0_2 != 0x1052) {
                                gCpu->unk98 = 0;
                            } else {
                                goto block_157;
                            }
                        } else {
                        block_157:
                            gCpu->unk98 = 1;
                        }
                    } else {
                        gCpu->unk98 = temp_a0_2 == 0x1052;
                    }
                } else if (temp_v1_8 != 0x607) {
                    if (temp_v1_8 < 0x608U) {
                        gCpu->unk98 = 0;
                    } else if (temp_v1_8 != 0x706) {
                        gCpu->unk98 = 0;
                    } else {
                        if (temp_a0_2 != 0x2829) {
                            var_v0_7 = 0x2021;
                            goto block_161;
                        }
                        goto block_164;
                    }
                } else {
                    gCpu->unk98 = 0;
                }
            } else if ((temp_a0_2 != 0x2021) && (var_v0_7 = 0x1052, (temp_a0_2 != 0x2829))) {
            block_161:
                if (temp_a0_2 != var_v0_7) {
                    gCpu->unk98 = 0;
                } else {
                    goto block_164;
                }
            } else {
            block_164:
                gCpu->unk98 = 1;
            }
        } else {
            goto block_166;
        }
    } else {
    block_166:
        gCpu->unk98 = 0;
    }
    temp_t1 = gCpu;
    var_v0_8 = -1;
    if (temp_t1->unk330.w & 1) {
        if (temp_t1->unk54 > 0) {
            var_v0_8 = temp_t1->unk70 - 1;
            goto block_172;
        }
        if (!(gCpuSelf->unkC0->unk24 & 0x10)) {
            temp_s0 = rand();
            temp_s1 = rand();
            gCpu->unk70 = (temp_s0 & 7) + (temp_s1 & 7) + ((rand() & 0xF) + 0x14);
        }
    } else {
    block_172:
        temp_t1->unk70 = var_v0_8;
    }
    if (gCpuSelf->unkC0->unk2C != 0) {
        temp_t1->unk7C = 1;
    } else {
        temp_t1->unk7C = 0;
    }
    temp_a0_3 = gCpuSelf->unkC0;
    var_a2_4 = 0;
    if ((temp_a0_3->unk4 & 0x200000) && (temp_a0_3->unk8 & 0x200000)) {
        var_a2_4 = (s32) temp_a0_3->unk2D >= gCpuSelf->unkC4;
    }
    gCpu->unk80 = var_a2_4;
    temp_t3 = gCpuSelf->unk2;
    temp_t4 = gCpuSelf->unk0;
    temp_a2_5 = D_00440940[gCpuSelf->unk1E][temp_t4].unk14;
    var_t2 = D_00440940[gCpuSelf->unk1E][temp_t4].unkC;
    temp_a3 = D_00440940[gCpuSelf->unk1E][temp_t3].unk14;
    var_a1_3 = D_00440940[gCpuSelf->unk1E][temp_t3].unkC;
    if (D_0043B7F8[0] == 7) {
        temp_v1_10 = &gCpu->unk27C[temp_t3];
        temp_v0_8 = *temp_v1_10 - (temp_a3 / 6000);
        *temp_v1_10 = temp_v0_8;
        if (temp_v0_8 < 0) {
            *temp_v1_10 = 0;
        }
        temp_v0_9 = gCpu->unk27C;
        var_t2 = temp_a2_5 - temp_v0_9[temp_t4];
        var_a1_3 = temp_a3 - temp_v0_9[temp_t3];
    }
    temp_v0_10 = gCpu->unk50;
    if (D_005F5CBC == 0) {
        var_t0 = 0;
        if (temp_v0_10 == 0) {
            var_v0_9 = temp_a2_5 >> 1;
            if (gCpuSelf->unk2A == 0x21) {
                var_v0_9 = temp_a2_5 >> 1;
                if (gCpuSelf->unk401 != 0) {
                    goto block_194;
                }
            } else {
                goto block_194;
            }
        }
    } else {
        var_t0 = 0;
        if ((temp_v0_10 == 0) && ((gCpuSelf->unk2A != 0x21) || (gCpuSelf->unk401 != 0))) {
            var_v0_9 = (s32) (temp_a2_5 * 0xA) >> 4;
        block_194:
            if ((var_t2 >= var_v0_9) || (var_t2 >= var_a1_3) || (var_a1_3 < ((s32) (temp_a3 * 0xB) >> 4))) {
                if (var_t2 < (temp_a2_5 / 3)) {
                    var_v0_10 = temp_a2_5 >> 3;
                    if (var_a1_3 >= (temp_a3 >> 1)) {
                        if (var_t2 >= var_a1_3) {
                            goto block_200;
                        }
                        goto block_203;
                    }
                    goto block_201;
                }
            block_200:
                var_v0_10 = temp_a2_5 >> 3;
            block_201:
                if ((var_t2 < var_v0_10) && ((temp_a3 / 6) < var_a1_3)) {
                    goto block_203;
                }
            } else {
            block_203:
                var_t0 = 1;
            }
        }
    }
    gCpu->unk3E0 = var_t0;
    var_a3 = 0;
    if ((gCpuSelf->unk2A != 0x21) || (gCpuSelf->unk401 != 0)) {
        var_a3 = var_t2 < var_a1_3;
    }
    gCpu->unk3E4 = var_a3;
    if ((D_005F5CBC == 0) || (D_005F6144 != 0)) {
        if (D_0043B7F8[0] == 5) {
            goto block_211;
        }
    } else {
    block_211:
        gCpu->unk3E0 = 0;
        gCpu->unk3E4 = 0;
    }
    temp_a0_4 = gCpuOpponent->unk10C;
    if ((temp_a0_4 == 0x338) || (temp_a0_4 == 0x33A)) {
        if (gCpu->unk330.h == 0x4C02) {
            gCpu->unk3E0 = 0;
        }
    }
    if (gCpu->unk29C != 0) {
        temp_v0_11 = gCpuOpponent->unkC0;
        if (temp_v0_11->unk2C != 0) {
            var_a0_2 = 0x14;
            if (!(temp_v0_11->unk8 & 0x400000)) {
                for (; var_a0_2 > 0; var_a0_2--) {
                    gCpu->unk2D0[var_a0_2] = gCpu->unk2D0[var_a0_2 - 1];
                }
                gCpu->unk2D0[0] = gCpuOpponent->unkC0;
            }
        }
    }
    func_0038ACA8();
    gCpu->unk284 = (s32) gCpuOpponent->unkC0;
    if (gCpuSelf->unkF3 != 0) {
        gCpu->unk26C = 0;
    }
    gCpu->unk274 -= 1;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381D30);
#endif


/* The candidate test of func_00382AD8 (a macro: its code is there four times): a command inside its frame window whose
 * condition, Cpu_IsCmdAllowed and situation hold becomes a candidate unless its move has flag 0x800000; the first one
 * whose move is unk90 is skipped when `excl` is set. At 0xB4 candidates the list is full. */
#define CPU_ADD_CAND(c)                                                                                                \
    {                                                                                                                  \
    s32 ok = 0;                                                                                                            \
    if (f->unkC4 >= (c)->unkB && f->unkC4 <= (c)->unkC && Cmd_CheckCondition(f, gCpuOpponent, (c)->unk2) &&          \
        Cpu_IsCmdAllowed(c)) {                                                                                         \
        ok = Cmd_CheckSituation((MoveCmd *)(c), f, gCpuOpponent) != 0;                                                 \
    }                                                                                                                  \
    if (ok) {                                                                                                          \
        if (n >= 0xB4) {                                                                                               \
            full = 1;                                                                                                  \
            break;                                                                                                     \
        }                                                                                                              \
        m = moves[(c)->unk6];                                                                                          \
        out->unk4 = m;                                                                                                 \
        if (!(m->unk4 & 0x800000)) {                                                                                   \
            if (excl && m == gCpu->unk90) {                                                                            \
                excl = 0;                                                                                              \
            } else {                                                                                                   \
                out->unk0 = 0;                                                                                         \
                n++;                                                                                                   \
                out->unk8 = (c);                                                                                       \
                out++;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    }                                                                                                                  \
    }

s32 Cpu_RejectCands(s32 (*filter)(CpuCand *));

/* Lists the candidates of decider 0xA (gCpuCands, returns their number): the commands of the move unk88's command list
 * (shared lists of 0x8004 included), then, when the window of the last command looked at holds and its move is not the
 * current one, the commands of that move that start at frame 0 or 1. With a chance of 7 in 8 (always in the mode
 * D_005F5CBC) the move unk90 is skipped once. The filter unkBC then rejects some (Cpu_RejectCands, unkAC keeps the count
 * before) [V code; I meaning of unk90: the move played last]. */
s32 func_00382AD8(void) {
    CpuCand *out = gCpuCands;
    s32 n = 0;
    Fighter *f = gCpuSelf;
    s32 excl;
    struct FighterMove **moves;
    s32 full;
    CpuCmd *e;
    CpuCmd *c;
    s32 j;
    s32 cnt;
    struct FighterMove *m;

    excl = 0;
    if ((rand() & 7) || D_005F5CBC) {
        excl = 1;
    }
    full = 0;
    moves = gCpu->unk10;
    for (c = e = (CpuCmd *)gCpu->unk88->unkC; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            for (j = 0, cnt = e->unkD, c = &D_0047F780[e->unk6]; j < cnt; j++, c++) {
                CPU_ADD_CAND(c)
            }
            if (full) {
                break;
            }
        } else {
            c = e;
            CPU_ADD_CAND(c)
        }
    }
    if (!full && c->unkB <= f->unkC4 && f->unkC4 <= c->unkC) {
        struct FighterMove *next = D_005F6C00[(s16)f->unk20][c->unk6];

        if (next != f->unkC0) {
            for (e = (CpuCmd *)next->unkC; e->unk0 != 0x8000; e++) {
                if (e->unk0 == 0x8004) {
                    for (j = 0, cnt = e->unkD, c = &D_0047F780[e->unk6]; j < cnt; j++, c++) {
                        if (c->unkB < 2) {
                            CPU_ADD_CAND(c)
                        }
                    }
                    if (full) {
                        break;
                    }
                } else if (e->unkB < 2) {
                    CPU_ADD_CAND(e)
                }
            }
        }
    }
    if (gCpu->unkBC != NULL) {
        gCpu->unkAC = n;
        n -= Cpu_RejectCands(gCpu->unkBC);
    }
    return n;
}

/* Fills gCpuCands (up to 0xB4) with the commands of the move unk88's command list (shared lists of 0x8004 included)
 * that are inside their frame window, whose condition holds, that are allowed (Cpu_IsCmdAllowed) and whose situation
 * holds, except moves of flag 0x800000 (unk4) and commands with bit 0x4000 while unk3E0 is clear; the move records
 * come from unk10. Returns the count [V]. */
s32 func_003835A0(void) {
    CpuCand *out = gCpuCands;
    struct FighterMove **moves = gCpu->unk10;
    CpuCmd *e = (CpuCmd *)gCpu->unk88->unkC;
    Fighter *f = gCpuSelf;
    s32 n = 0;
    s32 cnt;
    CpuCmd *c;
    s32 j;
    s32 ok;
    struct FighterMove *m;

    for (; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = &D_0047F780[e->unk6];
        } else {
            cnt = 1;
            c = e;
        }
        for (j = 0; j < cnt; j++, c++) {
            ok = 0;
            if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && Cmd_CheckCondition(f, gCpuOpponent, c->unk2) &&
                Cpu_IsCmdAllowed(c)) {
                ok = Cmd_CheckSituation((MoveCmd *)c, f, gCpuOpponent) != 0;
            }
            if (ok) {
                if (n >= 0xB4) {
                    break;
                }
                m = moves[c->unk6];
                out->unk4 = m;
                if (!(m->unk4 & 0x800000) && (!(c->unk0 & 0x4000) || gCpu->unk3E0 != 0)) {
                    out->unk0 = 0;
                    n++;
                    out->unk8 = c;
                    out++;
                }
            }
        }
    }
    return n;
}

/* Whether the own move's command list (shared lists of 0x8004 included) has a command inside its frame window, whose
 * condition holds, that is allowed (Cpu_IsCmdAllowed), whose situation holds and whose move (from unk10) has flag
 * 0x400000 [V code; I meaning]. */
s32 func_003838B0(void) {
    s32 r = 0;
    Fighter *f = gCpuSelf;
    CpuCmd *e = (CpuCmd *)f->unkC0->unkC;
    struct FighterMove **moves = gCpu->unk10;
    s32 cnt;
    CpuCmd *c;
    s32 j;
    s32 ok;

    for (; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = &D_0047F780[e->unk6];
        } else {
            cnt = 1;
            c = e;
        }
        for (j = 0; j < cnt; j++, c++) {
            ok = 0;
            if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && Cmd_CheckCondition(f, gCpuOpponent, c->unk2) &&
                Cpu_IsCmdAllowed(c)) {
                ok = Cmd_CheckSituation((MoveCmd *)c, f, gCpuOpponent) != 0;
            }
            if (ok && (moves[c->unk6]->unk8 & 0x400000)) {
                r = 1;
                break;
            }
        }
    }
    return r;
}

s32 func_00383B88(struct FighterMove *m, s32 arg1, s32 arg2) {
    s32 fl = m->unk8 & 0xFFFF0000;

    if (!(fl & 0x1C0000)) {
        fl |= 0xC0000;
    }
    return (fl & D_004318D8[arg2][arg1]) != 0;
}

/* Whether the move `m` answers the opponent's move by its kind (the low half of unk8: 0x10F, 0x217, 0x31F, 0x412,
 * 0x51F) against the opponent's move flags (gCpu->unk33C: 0x4C02 always, else bits 0x400, 4, 1, 2), for the two
 * tables `which` 0 and 1 [V code; I meaning]. */
s32 func_00383BD8(struct FighterMove *m, s32 which) {
    u16 kind = m->unk8;
    u32 fl = gCpu->unk33C.w;
    u16 lo = fl;

    switch (which) {
    case 0:
        if (lo == 0x4C02) {
            return 1;
        }
        if (fl & 0x400) {
            return kind == 0x412 || kind == 0x217 || kind == 0x31F;
        }
        if (fl & 4) {
            return kind == 0x10F || kind == 0x51F;
        }
        if (fl & 1) {
            return kind == 0x217;
        }
        if (fl & 2) {
            return kind == 0x10F;
        }
        break;
    case 1:
        if (lo == 0x4C02) {
            return 1;
        }
        if (fl & 0x400) {
            return kind == 0x412 || kind == 0x217 || kind == 0x31F;
        }
        if (fl & 4) {
            return kind == 0x10F || kind == 0x51F;
        }
        if (fl & 1) {
            return kind == 0x217 || kind == 0x10F;
        }
        if (fl & 2) {
            return kind == 0x10F || kind == 0x217 || kind == 0x412;
        }
        break;
    }
    return 0;
}


s32 func_00383D10(CpuCand *c) {
    if (gCpu->unk2AC == 0 || D_005F5C28 >= 0xB87 || (s16)gCpuOpponent->unk40 >= 0x5000 || gCpu->unk98 != 0) {
        return 0;
    }
    return func_00383B88(gCpuOpponent->unkC0, gCpu->unk24, (s32)c);
}

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

s32 Cpu_RejectCands(s32 (*filter)(CpuCand *)) {
    CpuCand *c = gCpuCands;
    s32 i = 0;
    s32 n = 0;

    for (; i < gCpu->unkAC; i++, c++) {
        if (c->unk0 >= 0 && filter(c) != 0) {
            s16 old = c->unk0;

            c->unk0 = -1;
            if (old > 0) n++;
        }
    }
    gCpu->unkB0 -= n;
    return n;
}

s32 func_00383ED8(CpuCand *c) {
    return ((c->unk8->unk0 >> 14) ^ 1) & 1;
}

s32 func_00383EF0(CpuCand *c) {
    return c->unk4->unk24 & 0x200;
}

s32 func_00383F00(CpuCand *c) {
    return c->unk4->unk24 & 0x20;
}

/* Candidate filter: command 0x8001 always; moves of flag 0x80000 only when func_00384B80 accepts them; a move accepted
 * by func_00384B80 while unk3E0 is set when it has an attack window; else attacks (window, no flag 0x400000 / 0x20000)
 * that func_00383B88 rates for the round, or of flag 0x10000. Moves with unk19 set need flag 0x80000 or 0x200000 [V]. */
s32 func_00384B80(CpuCand *c);

/* Candidate filter: command 0x8001 always; moves of flag 0x80000 only when func_00384B80 accepts them; a move accepted
 * by func_00384B80 while unk3E0 is set when it has an attack window; else attacks (window, no flag 0x400000 / 0x20000)
 * that func_00383B88 rates for the round, or of flag 0x10000. Moves with unk19 set need flag 0x80000 or 0x200000 [V]. */
s32 func_00383F10(CpuCand *c) {
    struct FighterMove *m = c->unk4;
    CpuCmd *cmd = c->unk8;
    u32 fl = m->unk4;

    if (m->unk19 != 0 && !(fl & 0x80000) && !(m->unk8 & 0x200000)) {
        return 0;
    }
    if (cmd->unk0 == 0x8001) {
        goto accept;
    }
    if ((fl & 0x80000) && !func_00384B80(c)) {
        return 1;
    }
    if (func_00384B80(c) && gCpu->unk3E0 != 0) {
        if (m->unk2C == 0) {
            goto reject;
        }
        return 1;
    }
    if (m->unk2C == 0) {
        goto reject;
    }
    if (c->unk4->unk8 & 0x400000) {
        return 0;
    }
    if (fl & 0x20000) {
        return 0;
    }
    if (func_00383B88(m, gCpu->unk20, 0) || (fl & 0x10000)) {
        return 1;
    }
reject:
    return 0;
accept:
    return 1;
}

s32 func_00384038(CpuCand *c) {
    struct FighterMove *m = c->unk4;
    u32 fl = m->unk4;
    u32 lvl;

    if (m->unk19 != 0 && !(fl & 0x80000) && !(m->unk8 & 0x200000)) {
        return 0;
    }
    if (m->unk2C == 0) {
        return 0;
    }
    lvl = m->unk8;
    if ((u16)lvl == 0x412 || (u16)lvl == 0x706) {
        return 0;
    }
    if (lvl & 0x400000) {
        return 0;
    }
    if (fl & 0x20000) {
        return 0;
    }
    if (func_00383B88(m, gCpu->unk20, 0) != 0 || (fl & 0x10000)) {
        return 1;
    }
    return 0;
}

s32 func_003840F8(CpuCand *c) {
    s32 fl = c->unk4->unk8;
    s32 r = 0;

    if ((fl & 0xFFFF) == 0x10F || (fl & 0x10000)) {
        r = 1;
    }
    return r;
}

s32 func_00384130(CpuCand *c) {
    struct FighterMove *m = c->unk4;
    u32 fl = m->unk4;
    u8 w;

    if (func_0036C1A0(gCpuSelf, gCpuOpponent, m)->unk2 <= 0) {
        return 0;
    }
    if (c->unk4->unk4 & 0x800000) {
        return 0;
    }
    w = m->unk2C;
    if (w == 0 || c->unk4->unk19 != 0 || (c->unk4->unk8 & 0x400000)) {
        return 0;
    }
    if (w >= 0xC) {
        return 0;
    }
    if (fl & 0x20000) {
        return 0;
    }
    if (func_00383B88(m, gCpu->unk20, 0) != 0 || (fl & 0x10000)) {
        return 1;
    }
    return 0;
}

/* Whether candidate c answers the opponent's move: no flag 0x20000 / 0x400000, an attack window (unk2C) without an
 * early one (unk19), func_0036C1A0's entry not negative, no flag 0x800000, func_00383B88; then by the opponent's move
 * flags unk33C: with bit 2 any kind but 0x412, with bit 1 neither 0x412 nor 0x10F, with bit 4 neither 0x412 nor
 * 0x217 [V code; I meaning]. */
s32 func_00384210(CpuCand *c) {
    struct FighterMove *m = c->unk4;
    u32 fl;

    if ((m->unk4 & 0x20000) || (m->unk8 & 0x400000) || m->unk2C == 0 || m->unk19 != 0) {
        return 0;
    }
    if (func_0036C1A0(gCpuSelf, gCpuOpponent, m)->unk2 < 0) {
        goto fail;
    }
    if (c->unk4->unk4 & 0x800000) {
        return 0;
    }
    if (func_00383B88(m, gCpu->unk20, 0) == 0) {
        goto fail;
    }
    fl = gCpu->unk33C.w;
    if (fl & 2) {
        if ((u16)m->unk8 != 0x412) {
            goto one;
        }
        return 0;
    }
    if (fl & 1) {
        if ((u16)m->unk8 == 0x412) {
            goto fail;
        }
        if ((u16)m->unk8 != 0x10F) {
            goto one;
        }
    fail:
        return 0;
    }
    if (fl & 4) {
        if ((u16)m->unk8 == 0x412) {
            goto fail;
        }
        if ((u16)m->unk8 == 0x217) {
            goto fail;
        }
    }
one:
    return 1;
}

s32 func_00384338(CpuCand *c) {
    struct FighterMove *m = c->unk4;
    s32 fl = m->unk4;
    s32 r = 0;

    if ((fl & 0x80000) && m->unk19 == 0) {
        if (!(fl & 0x20000)) {
            r = 1;
        }
    }
    return r;
}

s32 func_00383BD8(struct FighterMove *m, s32 n);

s32 func_00384370(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;

    if (m->unk2C != 0 && m->unk19 == 0 && !(m->unk8 & 0x400000) && !(m->unk4 & 0x20000)
        && func_00383B88(m, gCpu->unk20, 0) != 0) {
        r = func_00383BD8(c->unk4, 1) != 0;
    }
    return r;
}

/* Whether candidate c is usable here: its move has an attack window (unk2C) and no early window (unk19), is not attack
 * level 0x412, has neither flag 0x400000 nor 0x20000, and passes func_00383B88 and func_00383BD8 [V code; I meaning]. */
s32 func_00384408(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;
    u32 fl;

    if (m->unk2C != 0 && m->unk19 == 0) {
        fl = m->unk8;
        if ((u16)fl != 0x412 && !(fl & 0x400000) && !(m->unk4 & 0x20000) && func_00383B88(m, gCpu->unk20, 0) != 0) {
            r = func_00383BD8(c->unk4, 1) != 0;
        }
    }
    return r;
}

s32 func_003844B0(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;

    if (m->unk2C != 0) {
        if (m->unk4 & 0x20000) {
            goto end;
        }
        if (func_0036C1A0(gCpuSelf, gCpuOpponent, m)->unk2 >= 0) {
            struct FighterMove *m2 = c->unk4;

            if (m2->unk4 & 0x800000) {
                goto end;
            }
            if (m2->unk8 & 0x400000) {
                goto end;
            }
            if (m2->unk19 != 0) {
                goto end;
            }
            if (func_00383B88(m2, gCpu->unk20, 0) == 0) {
                goto end;
            }
            r = (c->unk8->unk0 & 0x3FE0) == 0;
        }
    }
end:
    return r;
}

s32 func_00384578(CpuCand *c) {
    struct FighterMove *m;
    s32 fl;

    if (c->unk8->unk0 == 0x8001) {
        return 1;
    }
    m = c->unk4;
    fl = m->unk4;
    if ((fl & 0x80000) && !(m->unk8 & 0x600000) && !(fl & 0x20000)) {
        return 1;
    }
    return 0;
}

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
    struct FighterMove *m = c->unk4;
    s32 r = 0;

    if ((m->unkC[3] & 0x7F) == 0x38) {
        if (!(m->unk8 & 0x400000)) {
            r = 1;
        }
    }
    return r;
}


/* A candidate move with damage (reaction entry unk2 > 0) the CPU can reach in time: its window start unk2C below the
 * reaction time unk2A8, not flagged 0x400000 / 0x800000 / 0x20000, and accepted by func_00383B88 [V code; I meaning]. */
s32 func_00384708(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;
    u8 w = m->unk2C;
    u32 fl;

    if (!(m->unk8 & 0x400000) && func_0036C1A0(gCpuSelf, gCpuOpponent, m)->unk2 > 0) {
        struct FighterMove *m2 = c->unk4;

        fl = m2->unk4;
        if (fl & 0x800000) {
            goto end;
        }
        if (w == 0) {
            goto end;
        }
        if (m2->unk19 != 0 && !(fl & 0x80000)) {
            goto end;
        }
        if (w < gCpu->unk2A8) {
            if (fl & 0x20000) {
                goto end;
            }
            r = func_00383B88(m2, gCpu->unk20, 0) != 0;
        }
    }
end:
    return r;
}

s32 func_003847D8(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;
    u8 w = m->unk2C;
    u32 fl;

    if (!(m->unk8 & 0x400000) && func_0036C1A0(gCpuSelf, gCpuOpponent, m)->unk2 > 0) {
        struct FighterMove *m2 = c->unk4;

        fl = m2->unk4;
        if (fl & 0x800000) {
            goto end;
        }
        if (w != 0 && (u16)m2->unk8 != 0x412 && w < gCpu->unk2A8) {
            if (fl & 0x20000) {
                goto end;
            }
            r = func_00383B88(m2, gCpu->unk20, 0) != 0;
        }
    }
end:
    return r;
}

#if 0
/* NON_MATCHING: 3 lines: the func_00383B88 result is copied to v1 for its test (v0 in the original) and the first bne is a bnel */
s32 func_00384898(CpuCand *c) {
    struct FighterMove *m = c->unk4;
    u32 fl = m->unk4;

    if (m->unk2C != 0) {
        if ((fl & 0x20001) != 1) {
            return 0;
        }
        if (func_00383B88(m, gCpu->unk20, 0) != 0) {
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384898);
#endif

s32 func_003848F0(CpuCand *c) {
    s32 fl = c->unk4->unk4;

    if (c->unk8->unk0 == 0x8002) {
        return 0;
    }
    return (fl & 0xFFFF) == 0x1052;
}

s32 func_00384920(CpuCand *c) {
    struct FighterMove *m = c->unk4;
    s32 b = m->unk4 & 1;
    s16 r = 0;

    if (b == 1) {
        r = m->unk2C != 0 ? 0 : b;
    }
    return r;
}

s32 func_00384950(CpuCand *c) {
    struct FighterMove *m = c->unk4;
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

s32 func_00384A10(CpuCand *c) {
    u16 id = c->unk8->unk6;
    const s16 *t = D_00507868;
    u32 i;

    for (i = 0; i < 1; i++) {
        if (id == *t) {
            return 1;
        }
        t++;
    }
    return 0;
}

s32 func_003849D0(CpuCand *c);

s32 func_00384A50(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;

    if (m->unk2C == 0 && m->unk19 == 0 && !(m->unk4 & 0x20000) && !(c->unk8->unk0 & 0x4000)) {
        r = func_003849D0(c) == 0;
    }
    return r;
}

s32 func_00384AB8(CpuCand *c) {
    struct FighterMove *m = c->unk4;
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
    struct FighterMove *m = c->unk4;
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

s32 func_00384C98(CpuCmd *k);

s32 func_00384BF0(CpuCand *c) {
    CpuCmd *k = c->unk8;
    s32 r = 0;

    if (k->unkB == 1 && k->unkC > k->unkD && k->unkD != 1 && k->unkC != 0xFF) {
        r = 0;
        if (func_00384C98(k) != 0) {
            r = 1;
        }
    }
    return r;
}

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

/* Whether the CPU command c is worth trying as an answer: a move with an early window (unk19) needs flag 0x80000 or
 * 0x200000; it must have an attack window (unk2C), no flag 0x400000 / 0x20000, and either pass func_00383B88 or have
 * flag 0x10000 [V code; I meaning]. */
s32 func_00384C98(CpuCmd *c) {
    struct FighterMove *m = D_005F6C00[(s16)gCpuSelf->unk20][c->unk6];
    u32 fl = m->unk4;

    if (m->unk19 != 0 && !(fl & 0x80000) && !(m->unk8 & 0x200000)) {
        return 0;
    }
    if (m->unk2C == 0 || (m->unk8 & 0x400000) || (fl & 0x20000)) {
        return 0;
    }
    if (func_00383B88(m, gCpu->unk20, 0) != 0 || (fl & 0x10000)) {
        return 1;
    }
    return 0;
}

s32 func_00384D68(u16 *cmd) {
    return *cmd & 0x4000;
}

s32 func_003848F0(CpuCand *c);
s32 func_00384920(CpuCand *c);

/* The CPU's answer to an opponent that is far away or recovering (decider 0xC's candidates, func_00389B70) [V code; I
 * meaning]: none while D_005F5C28 is 2951 or more. With unk2BC it picks func_00384920 candidates (not against kind
 * 0x217, only with unk3DC) and, from unk24 3 on, func_003849A0 ones. With unk2AC it answers the opponent's move kind
 * unk340: 0x412 (func_00384920 against the moves 0x2021 / 0x2829, otherwise func_003848F0 and one time in 50
 * func_00384920), 0x217 (func_003848F0), 0x31F / 0x10F (rarely func_003848F0 / from unk24 3 on func_003849A0, else
 * it sets unk26C and a delay unk270 within the opponent's remaining window and returns -1), 0x706 (func_00384920).
 * Below unk34 -70 and before unk24 3 it picks func_003849A0 candidates instead. Returns the count picked. */
s32 func_00384D78(void) {
    s32 n = 0;
    s32 w;

    if (D_005F5C28 >= 0xB87) {
        return 0;
    }
    if (gCpu->unk2BC != 0) {
        if (gCpu->unk340 != 0x217 && gCpu->unk3DC != 0) {
            n = Cpu_PickCands(func_00384920);
        }
        if (gCpu->unk24 >= 3) {
            n += Cpu_PickCands(func_003849A0);
        }
        return n;
    }
    if (gCpu->unk2AC != 0) {
        switch (gCpu->unk340) {
        case 0x412:
            switch ((u16)gCpuSelf->unkCC) {
            case 0x2021:
            case 0x2829:
                n = Cpu_PickCands(func_00384920);
                break;
            default:
                n = Cpu_PickCands(func_003848F0);
                if ((rand() & 0xFFF) % 50 == 0 && gCpu->unk3DC != 0) {
                    n += Cpu_PickCands(func_00384920);
                }
                if (gCpu->unk24 >= 3) {
                    n += Cpu_PickCands(func_003849A0);
                }
                break;
            }
            break;
        case 0x217:
            n = Cpu_PickCands(func_003848F0);
            break;
        case 0x31F:
            if (Cpu_RandBelow(0x3FFF) != 0) {
                n = Cpu_PickCands(func_003848F0);
                break;
            }
            gCpu->unk26C = 1;
            w = gCpuOpponent->unkC0->unk2C - gCpuOpponent->unkC4;
            if (w - 4 > 0) {
                gCpu->unk270 = Cpu_RandBelow(w - 4) + 4;
            } else if (w > 0) {
                gCpu->unk270 = Cpu_RandBelow(4);
            } else {
                gCpu->unk270 = 0;
            }
            return -1;
        case 0x10F:
            if (gCpu->unk24 >= 3) {
                n = Cpu_PickCands(func_003849A0);
                break;
            }
            gCpu->unk26C = 1;
            w = gCpuOpponent->unkC0->unk2C - gCpuOpponent->unkC4;
            if (w - 4 > 0) {
                gCpu->unk270 = Cpu_RandBelow(w - 4) + 4;
            } else if (w > 0) {
                gCpu->unk270 = Cpu_RandBelow(4);
            } else {
                gCpu->unk270 = 0;
            }
            return -1;
        case 0x706:
            n = Cpu_PickCands(func_00384920);
            if (gCpu->unk24 >= 3) {
                n += Cpu_PickCands(func_003849A0);
            }
            break;
        }
    }
    if (gCpu->unk34 < -0x46 && gCpu->unk24 < 3) {
        n = Cpu_PickCands(func_003849A0);
    }
    return n;
}

s32 func_00384610(CpuCand *c);
s32 func_003845D8(CpuCand *c);

s32 func_003850B8(void) {
    s32 n = 0;

    if (gCpu->unk20 >= 2) {
        return 0;
    }
    if ((rand() & 0xFFF) < gCpu->unk35E) {
        n = Cpu_PickCands(func_00384610);
        if (n <= 0 && gCpu->unk20 < 2) {
            n = Cpu_PickCands(func_003845D8);
        } else {
            gCpu->unk100 = 1;
        }
    } else if (gCpu->unk20 < 2) {
        n = Cpu_PickCands(func_003845D8);
    }
    return n;
}

#if 0
/* NON_MATCHING: 16 lines: register allocation of the random value and the CPU level (a0 / v1), and the sum kept in s1 before the return (the caller passes gCpu, unused here) */
s32 func_00384988(CpuCand *c);
s32 func_003847D8(CpuCand *c);
s32 func_00384708(CpuCand *c);

s32 func_00385178(CpuState *unused) {
    CpuState *c = gCpu;
    s32 n = 0;
    s32 r = (rand() >> 1) & 0xFFF;
    s32 (*f)(CpuCand *);

    switch (c->unk20) {
    case 1:
        break;
    case 2:
        if (r < 0x20) {
            n = Cpu_PickCands(func_00384988);
        }
        break;
    case 3:
        if (r < 0x400) {
            n = Cpu_PickCands(func_00384988);
        }
        break;
    }
    if (gCpu->unk33C.w & 1) {
        f = func_003847D8;
    } else {
        f = func_00384708;
    }
    n += Cpu_PickCands(f);
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385178);
#endif

/* Commits the CPU to the command c: its move (D_005F6C00 by the fighter's slot unk20) counts in unkFC when it has flag
 * 0x400000; an attack (window unk2C) sets the wait unk94 to unk34C[0] plus one of unk34C[1..4] at random, and resets
 * unk64 / unk68 / unk6C to -1, unk5C to 0x28, unk50 / unk4C to 0 [V code; I meaning]. */
void func_00385250(CpuCmd *c, s32 n) {
    struct FighterMove *m = D_005F6C00[(s16)gCpuSelf->unk20][c->unk6];

    if (m->unk8 & 0x400000) {
        gCpu->unkFC++;
    }
    if (m->unk2C != 0) {
        s32 k = (rand() & 3) + 1;
        s16 r = gCpu->unk34C[k];

        gCpu->unk64 = -1;
        gCpu->unk94 = gCpu->unk34C[0] + r;
        if (gCpu->unk68 > 0) {
            gCpu->unk68 = -1;
        }
        gCpu->unk6C = -1;
        gCpu->unk5C = 0x28;
        gCpu->unk4C = 0;
        gCpu->unk50 = 0;
    }
}

u16 func_00385310(s32 x) {
    return x;
}

/* The candidate picked by the CPU: the `n`-th picked one (state > 0), counted from the start on frames of the CPU's
 * parity, from the end on the others; -1 when there are fewer [V code; I meaning]. */
s32 func_00385318(s32 n) {
    s32 i;

    if ((gCpu->unk4 & 1) == gCpu->unk0) {
        for (i = 0; i < gCpu->unkAC; i++) {
            if (gCpuCands[i].unk0 > 0) {
                if (--n < 0) {
                    return i;
                }
            }
        }
    } else {
        for (i = gCpu->unkAC - 1; i >= 0; i--) {
            if (gCpuCands[i].unk0 > 0) {
                if (--n < 0) {
                    return i;
                }
            }
        }
    }
    return -1;
}

s32 func_00381278(u16 cmd, struct FighterMove *m);

/* Plays one of the gCpu->unkB0 picked candidates chosen at random (rand() and the CPU's own seed D_005F6C08, which
 * steps as x * 5 + 3): its command through func_00381278, then func_00385250; returns its index, or -1 [V code]. */
s32 func_003853E8(void) {
    s32 n = gCpu->unkB0;
    s32 x;
    s32 i;

    if (n <= 0) {
        return -1;
    }
    x = n * ((rand() + D_005F6C08) & 0x7FFF);
    D_005F6C08 = D_005F6C08 * 5 + 3;
    i = func_00385318(x >> 15);
    if (i < 0) {
        return -1;
    }
    func_00381278(func_00385310(gCpuCands[i].unk8->unk0), gCpuCands[i].unk4);
    func_00385250(gCpuCands[i].unk8, 1);
    return i;
}

/* NON_MATCHING: 34 lines differ: the candidate count and the planned wait take s1 / s0 the other way round,
 * which reorders the window arithmetic of the plan. */
#if 0
/* As func_003853E8, but a candidate that func_00384BF0 rejects is, with a chance of unk3B8 in 4096, planned instead of
 * played: func_0038A928 starts it later at a random frame of its window (unk260 / unk264 / unk268) [V]. */
s32 func_003854B0(void) {
    s32 n = gCpu->unkB0;
    s32 x;
    s32 i;
    CpuCmd *c;
    s32 frame;
    s32 first;
    s32 wait;

    if (n <= 0) {
        return -1;
    }
    x = n * ((rand() + D_005F6C08) & 0x7FFF);
    D_005F6C08 = D_005F6C08 * 5 + 3;
    i = func_00385318(x >> 15);
    if (i < 0) {
        return -1;
    }
    if (func_00384BF0(&gCpuCands[i]) == 0) {
        func_00381278(func_00385310(gCpuCands[i].unk8->unk0), gCpuCands[i].unk4);
        func_00385250(gCpuCands[i].unk8, 1);
    } else if ((rand() & 0xFFF) < gCpu->unk3B8) {
        c = gCpuCands[i].unk8;
        frame = gCpuSelf->unkC4;
        first = c->unkD;
        n = c->unkC;
        if (first >= frame) {
            wait = first - frame + 1;
            n = (n - first) >> 1;
            x = n * ((rand() + D_005F6C08) & 0x7FFF);
            D_005F6C08 = D_005F6C08 * 5 + 3;
            gCpu->unk260 = 1;
            gCpu->unk6 = 0;
            gCpu->unk268 = wait + ((gCpuCands[i].unk8->unkC - gCpuCands[i].unk8->unkD) >> 1) + (x >> 15);
            gCpu->unk264 = gCpuCands[i].unk8;
        } else {
            n -= frame;
            x = n * ((rand() + D_005F6C08) & 0x7FFF);
            D_005F6C08 = D_005F6C08 * 5 + 3;
            gCpu->unk260 = 1;
            gCpu->unk6 = 0;
            gCpu->unk268 = x >> 15;
            gCpu->unk264 = gCpuCands[i].unk8;
        }
    } else {
        func_00381278(func_00385310(gCpuCands[i].unk8->unk0), gCpuCands[i].unk4);
        func_00385250(gCpuCands[i].unk8, 1);
    }
    return i;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003854B0);
#endif

/* Lists in unk134 up to four entries of the command list of the move unk88 (looking into the shared lists of 0x8004)
 * that are for the fighter's variant and whose move has flag 0x20 (unk24); returns their number, also in unk130 [V]. */
s32 func_003856C0(void) {
    s32 cnt;
    s32 n = 0;
    MoveCmd *e = (MoveCmd *)gCpu->unk88->unkC;
    MoveCmd **out = gCpu->unk134;
    MoveCmd *c;
    s32 j;

    for (; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = (MoveCmd *)&D_0047F780[e->unk6];
        } else {
            cnt = 1;
            c = e;
        }
        for (j = 0; j < cnt; j++, c++) {
            if (CPU_CMD_FOR_VARIANT(c) && (D_005F6C00[(s16)gCpuSelf->unk20][c->unk6]->unk24 & 0x20)) {
                *out++ = c;
                if (++n >= 4) {
                    break;
                }
            }
        }
    }
    gCpu->unk130 = n;
    return n;
}

/* Lists in unk154 up to 0x30 commands of the move unk88's command list (looking into the shared lists of 0x8004)
 * that are for the fighter's variant and whose move: is not of flag 0x400000 while unk2A4 is set; has flag 0x200000
 * (unk8) or 0x80000 (unk4), or else flag 0x400000 and passes a chance of unk35E in 4096; lacks flag 0x20000; has no
 * early window (unk19) unless the variant is not 0 or a roll of 13 in 4096 passes; and passes func_00383B88 for
 * round unk20. Returns their number, also in unk150 [V]. */
s32 func_003857F8(void) {
    CpuCmd **out = gCpu->unk154;
    s32 n = 0;
    s32 r = rand() & 0xFFF;
    CpuCmd *e = (CpuCmd *)gCpu->unk88->unkC;
    s32 cnt;
    CpuCmd *c;
    s32 j;
    s32 ok;
    struct FighterMove *m;

    for (; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = &D_0047F780[e->unk6];
        } else {
            cnt = 1;
            c = e;
        }
        for (j = 0; j < cnt; j++, c++) {
            if (!CPU_CMD_FOR_VARIANT(c)) {
                continue;
            }
            m = D_005F6C00[(s16)gCpuSelf->unk20][c->unk6];
            if (gCpu->unk2A4 != 0 && (m->unk8 & 0x400000)) {
                continue;
            }
            if (!(m->unk8 & 0x200000) && !(m->unk4 & 0x80000)) {
                if (!(m->unk8 & 0x400000) || (rand() & 0xFFF) >= gCpu->unk35E) {
                    continue;
                }
            }
            if (m->unk4 & 0x20000) {
                continue;
            }
            ok = 0;
            if (m->unk19 == 0 || gCpuSelf->unk28 != 0 || r < 0xD) {
                ok = 1;
            }
            if (ok && func_00383B88(m, gCpu->unk20, 0)) {
                *out++ = c;
                if (++n >= 0x30) {
                    break;
                }
            }
        }
    }
    gCpu->unk150 = n;
    return n;
}

/* Lists in unk228 up to ten commands of the move unk88's command list (shared lists of 0x8004 included) that start
 * at frame 1 and end at their unkD (unkB == 1, unkC == unkD), are for the fighter's variant, survive a chance of
 * unk38E in 4096 when their move has flag 0x20 (unk24) and have no bit 0x4000 unless unk3E0 is set; unk254 keeps the
 * latest end frame below 0x3C. Returns their number, also in unk224; does nothing (and clears unk21C) when unk21C is
 * set [V]. */
s32 func_00385A60(void) {
    s32 n = 0;
    CpuCmd *e;
    s32 cnt;
    CpuCmd *c;
    s32 j;
    struct FighterMove *m;

    if (gCpu->unk21C != 0) {
        gCpu->unk21C = 0;
        return 0;
    }
    gCpu->unk254 = 0;
    for (e = (CpuCmd *)gCpu->unk88->unkC; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = &D_0047F780[e->unk6];
        } else {
            cnt = 1;
            c = e;
        }
        for (j = 0; j < cnt; j++, c++) {
            if (c->unkB == 1 && c->unkC == c->unkD && CPU_CMD_FOR_VARIANT(c)) {
                m = D_005F6C00[(s16)gCpuSelf->unk20][c->unk6];
                if ((m->unk24 & 0x20) && (rand() & 0xFFF) < gCpu->unk38E) {
                    continue;
                }
                if (gCpu->unk3E0 == 0 && (c->unk0 & 0x4000)) {
                    continue;
                }
                gCpu->unk228[n] = c;
                if (c->unkC < 0x3C && gCpu->unk254 < c->unkC) {
                    gCpu->unk254 = c->unkC;
                }
                if (++n >= 10) {
                    break;
                }
            }
        }
    }
    gCpu->unk224 = n;
    return n;
}

s32 func_003856C0(void);
s32 func_003857F8(void);
s32 func_00385A60(void);
s32 func_00382AD8(void);
s32 func_00383ED8(CpuCand *c);
s32 Cpu_PickCands(s32 (*filter)(CpuCand *));
s32 func_003853E8(void);

/* Decider 10 of Cpu_Think. With unk1C set, continues one of three running plans (unk12C: func_003856C0, unk14C:
 * func_003857F8, else while unk80 is clear and the own move has an attack window: func_00385A60), holding the
 * previous buttons' high nibble while it goes on and clearing it when it ends. Then lists the candidates
 * (func_00382AD8) and, when unk3C is at least 0xDD, plays one that func_00383ED8 accepts [V]. */
s32 func_00385C80(void) {
    s32 r = 0;
    CpuState *c = gCpu;
    Fighter *f;

    c->unkB0 = 0;
    c->unkAC = 0;
    if (c->unk1C != 0) {
        f = gCpuSelf;
        c->unk88 = f->unkC0;
        c->unk78 = f->unkC4;
        c->unk260 = 0;
        c->unk270 = 0;
        c->unk26C = 0;
        if (c->unk12C > 0) {
            if (func_003856C0() > 0) {
                r = 1;
                gCpu->unk6 = gCpu->unk8 & 0xF000;
            } else {
                gCpu->unk130 = 0;
                gCpu->unk12C = 0;
            }
        } else if (c->unk14C > 0) {
            if (func_003857F8() > 0) {
                r = 1;
                gCpu->unk6 = gCpu->unk8 & 0xF000;
            } else {
                gCpu->unk150 = 0;
                gCpu->unk14C = 0;
            }
        } else if (c->unk80 == 0) {
            if (f->unkC0->unk2C != 0) {
                if (func_00385A60() > 0) {
                    r = 1;
                    gCpu->unk220++;
                    gCpu->unk6 = gCpu->unk8 & 0xF000;
                } else {
                    gCpu->unk224 = 0;
                    gCpu->unk220 = 0;
                }
            } else {
                c->unk224 = 0;
                c->unk150 = 0;
                c->unk130 = 0;
                c->unk220 = 0;
                c->unk14C = 0;
                c->unk12C = 0;
                c->unk100 = 0;
            }
        }
    }
    gCpu->unk78 = gCpuSelf->unkC4;
    gCpu->unkAC = func_00382AD8();
    if (gCpu->unk3C >= 0xDD && Cpu_PickCands(func_00383ED8) > 0) {
        func_003853E8();
        return 1;
    }
    return r;
}

/* NON_MATCHING: 142 lines differ, register allocation: the original spills `id` to the stack next to the result and
 * keeps D_0047F780 (first walk) and the constant 1 (second walk) in fp; ours keeps `id` in fp. */
#if 0
/* Whether the own move's command list (shared lists of 0x8004 included) has a command for move `id` inside its frame
 * window whose condition, Cpu_IsCmdAllowed and situation hold; if not, and the move of the list's end entry is another
 * move whose window holds now, looks for such a command starting at frame 1 in that move's list [V code; I meaning]. */
s32 func_00385DF8(s16 id) {
    s32 r = 0;
    Fighter *f = gCpuSelf;
    CpuCmd *e = (CpuCmd *)f->unkC0->unkC;
    s32 cnt;
    CpuCmd *c;
    s32 j;
    s32 ok;
    struct FighterMove *m;

    for (; e->unk0 != 0x8000; e++) {
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = &D_0047F780[e->unk6];
        } else {
            cnt = 1;
            c = e;
        }
        for (j = 0; j < cnt; j++, c++) {
            if (c->unk6 == id) {
                ok = 0;
                if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && Cmd_CheckCondition(f, gCpuOpponent, c->unk2) &&
                    Cpu_IsCmdAllowed(c)) {
                    ok = Cmd_CheckSituation((MoveCmd *)c, f, gCpuOpponent) != 0;
                }
                if (ok) {
                    r = 1;
                    break;
                }
            }
        }
    }
    if (r == 0) {
        m = D_005F6C00[(s16)f->unk20][e->unk6];
        if (m != f->unkC0 && f->unkC4 >= e->unkB && f->unkC4 <= e->unkC) {
            for (e = (CpuCmd *)m->unkC; e->unk0 != 0x8000; e++) {
                if (e->unk0 == 0x8004) {
                    cnt = e->unkD;
                    c = &D_0047F780[e->unk6];
                } else {
                    cnt = 1;
                    c = e;
                }
                for (j = 0; j < cnt; j++, c++) {
                    if (id == c->unk6 && c->unkB == 1) {
                        ok = 0;
                        if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC &&
                            Cmd_CheckCondition(f, gCpuOpponent, c->unk2) && Cpu_IsCmdAllowed(c)) {
                            ok = Cmd_CheckSituation((MoveCmd *)c, f, gCpuOpponent) != 0;
                        }
                        if (ok) {
                            r = 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385DF8);
#endif


/* Whether the current move of the CPU's fighter has a command entry of condition kind `kind` (unk3 & 0x7F), looking
 * into the shared lists (0x8004, unkD entries) [V code; I meaning]. */
s32 func_00386388(s32 kind) {
    MoveCmd *c = (MoveCmd *)gCpuSelf->unkC0->unkC;
    MoveCmd *p;
    s32 n;
    s32 i;

    for (; c->unk0 != 0x8000; c++) {
        if (c->unk0 == 0x8004) {
            n = c->unkD;
            p = (MoveCmd *)&D_0047F780[c->unk6];
        } else {
            n = 1;
            p = c;
        }
        for (i = 0; i < n; i++, p++) {
            if ((p->unk3 & ~0x80) == kind) {
                return 1;
            }
        }
    }
    return 0;
}

s32 func_00386428(void) {
    if (gCpuSelf->unk42 != 0) {
        return 0;
    }
    gCpu->unk5C = 0x28;
    gCpu->unk6C = -1;
    gCpu->unk50 = 0;
    gCpu->unk4C = 0;
    return gCpu->unk340 == 0x217 ? 0x8000 : 0xC000;
}

s32 func_00386470(void) {
    if (gCpuSelf->unk42 != 0) {
        return 0;
    }
    gCpu->unk5C = 0x28;
    gCpu->unk6C = -1;
    gCpu->unk50 = 0;
    gCpu->unk4C = 0;
    switch (gCpu->unk340) {
    case 0x217:
    case 0x31F:
    case 0x412:
        return 0x8000;
    }
    return 0xC000;
}

s32 func_00383ED8(CpuCand *c);

s32 func_003864E8(void) {
    CpuState *c = gCpu;
    Fighter *s;

    if (c->unk80 == 0 || c->unkAC == 0 || c->unk288[3] != 0) {
        return 0;
    }
    s = gCpuSelf;
    if (c->unk2A8 >= s->unkC0->unk2C - s->unkC4 && c->unk20 < 3 && s->unk42 != 2) {
        return 1;
    }
    if (Cpu_PickCands(func_00383ED8) <= 0) {
        return 1;
    }
    if (func_003853E8() >= 0) {
        gCpu->unk94 = -1;
        return 1;
    }
    return 1;
}


/* Per character (fighter unk28): whether it has the special plays of situations 6, 8 and 9 and their commands
 * (D_00507620, 16 bytes) [I]. */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ u16 *unk4;
    /* 0x8 */ u16 *unk8;
    /* 0xC */ u16 *unkC;
} Unk507620;

extern Unk507620 D_00507620[];
extern const u16 D_00507482[];
s32 func_00383F00(CpuCand *c);
s32 func_00383F10(CpuCand *c);
s32 func_00384130(CpuCand *c);
s32 func_00384210(CpuCand *c);
s32 func_00384338(CpuCand *c);
s32 func_00384370(CpuCand *c);
s32 func_00384408(CpuCand *c);
s32 func_003844B0(CpuCand *c);
s32 func_00384578(CpuCand *c);
s32 func_00384648(CpuCand *c);
s32 func_00384688(CpuCand *c);
s32 func_003846D0(CpuCand *c);
s32 func_00384950(CpuCand *c);
s32 func_00384988(CpuCand *c);
s32 func_003849B8(CpuCand *c);
s32 func_00384A50(CpuCand *c);
s32 func_00384AB8(CpuCand *c);
s32 func_00384B08(CpuCand *c);
s32 func_003850B8(void);
s32 func_00385178(CpuState *c);
s32 func_00385DF8(s16 id);
void func_00380528(s32 a, s32 b);
s32 func_0038AEA0(Fighter *f);

/* The general candidate list of decider 0x12 (func_00387120) with the wait `wait` [V code; I meaning; a draft of
 * m2c's control flow]: drops func_00383F00 candidates after a hit (unk144), sets the step distance unk9C, then by the
 * opponent's state (busy unk2A4 / unkC8, kind 0x4C02, flags 1 / 2 / 0x605, unk135 / unk139) and the own timers (unk64,
 * unk68, unk60) picks one of about twenty candidate filters with chances unk360 / unk362 / unk368 / unk384 / unk38E /
 * unk390, plays the character's special command list (D_00507620.unkC, func_00385DF8) or a side step; returns the
 * count picked, or -1 / 0. */
/* NON_MATCHING: m2c's control flow with fixed types (about 750 lines differ); a starting point only. */
#if 0
s32 func_003865A0(s32 wait) {
    CpuState *cpu;
    CpuState *temp_a1;
    CpuState *temp_a1_3;
    CpuState *temp_a1_4;
    s16 temp_a2;
    s16 var_s0;
    s32 (*var_a0_2)(CpuCand *);
    s32 (*var_a0_3)(CpuCand *);
    s32 (*var_a0_5)(CpuCand *);
    s32 (*var_a0_6)(CpuCand *);
    s32 (*var_a0_7)(CpuCand *);
    s32 (*var_a0_8)(CpuCand *);
    s32 (*var_a0_9)(CpuCand *);
    s32 temp_a0_2;
    s32 temp_s0_2;
    s32 temp_s1_2;
    s32 temp_v0;
    s32 temp_v1;
    s32 temp_v1_4;
    s32 var_a0;
    s32 var_a0_4;
    s32 var_s0_3;
    s32 var_s0_4;
    s32 var_s0_5;
    s32 var_s2;
    s32 var_s2_2;
    s32 var_s2_3;
    s32 var_s3;
    s32 var_v0;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v0_4;
    s32 var_v0_5;
    s32 var_v0_6;
    struct FighterMove *temp_a0;
    u16 *var_s0_2;
    u16 temp_v1_2;
    u32 temp_s0;
    u32 temp_s1;
    u8 temp_a1_2;
    u8 temp_v1_3;

    temp_a1 = gCpu;
    var_s3 = wait;
    var_s0 = temp_a1->unk358;
    if (temp_a1->unk144 != 0) {
        Cpu_RejectCands(func_00383F00);
    }
    var_v0_2 = temp_a1->unk9C;
    if ((temp_a1->unkC0 >= 0x1A) && (var_v0_2 < 0)) {
        if (temp_a1->unk98 == 0) {
            temp_v1 = gCpu->unk2A8 - ((rand() & 7) + 4);
            gCpu->unk9C = temp_v1;
            if (temp_v1 < 4) {
                gCpu->unk9C = 3;
            }
        }
        var_v0_2 = temp_a1->unk9C;
    }
    var_v0 = 0;
    if (var_v0_2 <= 0) {
        if ((temp_a1->unkE4 != 0) || (temp_a1->unkEC != 0) ||
            ((temp_a1->unkC0 >= 0x1A) &&
             (temp_v1_2 = (u16) gCpu->unk38E, (((rand() & 0xFFF) < (s16) temp_v1_2 / 2) == 0))) ||
            (((u32) (temp_a1->unkC4 - 1) < 0x13U) && ((rand() & 0xFFF) >= gCpu->unk38E))) {
            var_s3 = -1;
        }
        if (((rand() & 0xFFF) < gCpu->unk390) && (gCpuOpponent->unk135 != 0) && (gCpu->unk33C.h != 0x4C02)) {
            temp_a0 = gCpuOpponent->unkC0;
            temp_a2 = gCpuOpponent->unkC4;
            temp_v1_3 = temp_a0->unk19;
            temp_a1_2 = temp_a0->unk1A;
            if ((temp_a2 >= (temp_v1_3 + ((s32) (temp_a1_2 - temp_v1_3) >> 1))) && ((s32) temp_a1_2 >= temp_a2) &&
                ((s16) gCpuSelf->unk40 < 0x4001) && (gCpu->unk20 == 2)) {
                gCpu->unk94 = -1;
            }
        }
        temp_a1_3 = gCpu;
        if ((func_0038AEA0(gCpuOpponent) != 0) && (temp_a1_3->unk20 < 3) && ((rand() & 0xFFF) < gCpu->unk368)) {
            gCpu->unk94 -= 2;
        }
        var_a0 = 0;
        if ((temp_a1_3->unk33C.w & 0x605) || (gCpuOpponent->unk135 != 0) || (gCpuOpponent->unk139 != 0) ||
            (temp_a1_3->unk2A8 < 0xA)) {
            var_a0 = 1;
        }
        if (var_a0 != 0) {
            temp_a1_3->unkEC = 0;
            var_s0 = 0;
        }
        if (temp_a1_3->unk2A4 == 0) {
            goto block_69;
        }
        if (temp_a1_3->unkC8 != 0) {
        block_69:
            if (temp_a1_3->unk33C.h != 0x4C02) {
                var_v0_3 = temp_a1_3->unk68;
                goto block_102;
            }
            temp_a1_4 = gCpu;
            if ((rand() & 0xFFF) >= temp_a1_4->unk384) {
                var_v0_3 = temp_a1_4->unk68;
                goto block_102;
            }
            if (temp_a1_4->unk33C.w & 2) {
                var_v0_3 = temp_a1_4->unk68;
            block_102:
                var_a0_2 = func_00384B08;
                if (var_v0_3 >= 0) {
                    var_s2 = Cpu_PickCands(func_00384B08);
                    if (var_s2 > 0) {
                        gCpu->unk68 = -1;
                    }
                    goto block_199;
                }
                if (temp_a1_3->unk2B0 == 0) {
                    if (((temp_a1_3->unk2AC != 0) || (temp_a1_3->unk2BC != 0)) && (temp_a1_3->unkC8 == 0)) {
                        if ((temp_a1_3->unkE4 != 0) || (cpu = gCpu, (((rand() & 0xFFF) < cpu->unk368) != 0))) {
                            goto block_121;
                        }
                        var_v0 = 0;
                        if (var_s3 <= 0) {
                            if (!(cpu->unk33C.w & 1)) {
                                var_a0_3 = func_00383F10;
                            } else {
                                var_a0_3 = func_00384038;
                            }
                            goto block_168;
                        }
                    } else if (var_s3 <= 0) {
                        if (temp_a1_3->unk64 >= 0) {
                            var_s2 = Cpu_PickCands(func_00384338);
                            gCpu->unk64 = -1;
                            goto block_199;
                        }
                        if ((temp_a1_3->unkEC != 0) || ((rand() & 0xFFF) < var_s0)) {
                            temp_v0 = func_003850B8();
                            var_v0 = temp_v0;
                            if (temp_v0 == 0) {
                                goto block_142;
                            }
                        } else {
                        block_142:
                            var_s0_2 = D_00507620[gCpuSelf->unk28].unkC;
                            if ((gCpu->unk60 < 0) && (var_s0_2 != NULL) && (gCpu->unk7C == 0) &&
                                !(gCpu->unk330.w & 1) && (gCpuSelf->unk10C != var_s0_2[1])) {
                                var_a0_4 = 0;
                                if ((gCpuSelf->unk40 < 0x2001) && (gCpuSelf->unk135 == 0) && (gCpu->unk20 < 3) &&
                                    (gCpu->unk2A4 == 0)) {
                                loop_152:
                                    if ((s16) var_s0_2[0] < (rand() & 0xFFF)) {
                                        var_s0_2 += 8;
                                        var_a0_4 = 0;
                                        if ((s16) *var_s0_2 < 0) {

                                        } else {
                                            goto loop_152;
                                        }
                                    } else if (func_00385DF8(var_s0_2[1]) != 0) {
                                        Cpu_SetCommand(var_s0_2 + 2);
                                        gCpu->unk14C = 1;
                                        var_a0_4 = 1;
                                        gCpu->unk60 = (rand() & 0xF) + 0x1C;
                                    } else {
                                        goto block_157;
                                    }
                                }
                            } else {
                            block_157:
                                var_a0_4 = 0;
                            }
                            var_v0 = -1;
                            if (var_a0_4 == 0) {
                                if ((gCpu->unk20 < 3) && ((rand() & 0x3F) < 0xA) && (gCpuSelf->unk135 == 0) &&
                                    !(gCpu->unk330.w & 1)) {
                                    var_a0_3 = func_00384920;
                                    goto block_168;
                                }
                                if ((rand() & 0x3F) < 0x20) {
                                    if (rand() & 1) {
                                        var_a0_3 = func_00384578;
                                    } else {
                                        var_a0_3 = func_00384370;
                                    }
                                    goto block_168;
                                }
                                if ((gCpu->unk64 < 0) && (gCpu->unk68 < 0) && !(gCpu->unk330.w & 1) &&
                                    (gCpuSelf->unk135 == 0) && ((rand() & 0xFFF) < 0x198) &&
                                    (var_s2 = Cpu_PickCands(func_00384920), (var_s2 > 0)) && (gCpu->unk3DC != 0)) {
                                    gCpu->unk94 = -1;
                                    gCpu->unk64 = 0x12;
                                } else {
                                    if ((rand() & 0xFFF) < gCpu->unk360) {
                                        var_a0_5 = func_00384210;
                                        goto block_195;
                                    }
                                    if ((rand() & 0xFFF) >= gCpu->unk362) {
                                        if ((gCpu->unk33C.w & 1) && ((rand() & 0xFFF) >= gCpu->unk38E)) {
                                            var_a0_5 = func_00384038;
                                            goto block_195;
                                        }
                                        if ((rand() & 0x3F) < 0xA) {
                                            var_a0_5 = func_003844B0;
                                            goto block_195;
                                        }
                                        if ((gCpuSelf->unk135 == 0) && (gCpu->unk68 < 0) &&
                                            (D_005F6C08 = (D_005F6C08 * 5) + 3,
                                             (((s32) ((u32) ((rand() + D_005F6C08) & 0x7FFF) >> 3) < 0x200) != 0)) &&
                                            (var_s2_2 = Cpu_PickCands(func_00384AB8), (var_s2_2 > 0))) {
                                            gCpu->unk68 = 4;
                                            temp_s1 = (u32) ((rand() + D_005F6C08) & 0x7FFF) >> 0xD;
                                            D_005F6C08 = (D_005F6C08 * 5) + 3;
                                            temp_s0 = (u32) ((rand() + D_005F6C08) & 0x7FFF) >> 0xD;
                                            D_005F6C08 = (D_005F6C08 * 5) + 3;
                                            temp_s0_2 = temp_s0 + ((u32) ((rand() + D_005F6C08) & 0x7FFF) >> 0xD);
                                            D_005F6C08 = (D_005F6C08 * 5) + 3;
                                            gCpu->unk68 = temp_s1 + 1 + temp_s0_2;
                                        } else {
                                            D_005F6C08 = (D_005F6C08 * 5) + 3;
                                            if ((s32) ((u32) ((rand() + D_005F6C08) & 0x7FFF) >> 3) < 0xA) {
                                                var_a0_5 = func_00384A50;
                                                goto block_195;
                                            }
                                            var_v0_4 = Cpu_PickCands(func_00383F10);
                                            goto block_197;
                                        }
                                    } else {
                                        var_a0_5 = func_00384130;
                                    block_195:
                                        var_v0_4 = Cpu_PickCands(var_a0_5);
                                    block_197:
                                        var_s2_2 = var_v0_4;
                                    }
                                    var_s2 = var_s2_2 + Cpu_PickCands(func_00384578);
                                }
                                goto block_199;
                            }
                        }
                    } else {
                        var_v0 = 0;
                        if (temp_a1_3->unk20 < 3) {
                            if ((rand() & 0xFFF) < 4) {
                                var_v0 = 0;
                                if (!(gCpu->unk330.w & 1)) {
                                    var_v0 = Cpu_PickCands(func_00384920);
                                    if (var_v0 <= 0) {
                                        goto block_135;
                                    }
                                }
                            } else {
                            block_135:
                                var_v0 = 0;
                            }
                        }
                    }
                } else if ((temp_a1_3->unkE4 == 0) && (cpu = gCpu, (((rand() & 0xFFF) < cpu->unk368) == 0))) {
                    var_v0 = 0;
                    if (var_s3 <= 0) {
                        if (!(cpu->unk33C.w & 1)) {
                            var_a0_3 = func_00383F10;
                        } else {
                            var_a0_3 = func_00384038;
                        }
                    block_168:
                        var_s2 = Cpu_PickCands(var_a0_3);
                        goto block_199;
                    }
                } else {
                block_121:
                    var_s2 = func_00385178(gCpu);
                    goto block_199;
                }
            } else {
                temp_v1_4 = temp_a1_4->unk20;
                var_s0_3 = 0;
                if (temp_v1_4 >= 3) {
                    if (temp_a1_4->unk34 < 0x33) {
                        if (!(rand() & 7)) {
                            var_a0_6 = func_00384950;
                            goto block_86;
                        }
                        goto block_84;
                    }
                block_84:
                    var_s0_3 = Cpu_PickCands(func_00384988);
                    if (var_s0_3 > 0) {
                        func_00380528(0xFA, 2);
                    }
                } else if (temp_v1_4 >= 2) {
                    var_a0_6 = func_00384950;
                    if (!(rand() & 0xF)) {
                        var_a0_7 = func_00384988;
                    } else {
                    block_86:
                        var_a0_7 = var_a0_6;
                    }
                    var_s0_3 = Cpu_PickCands(var_a0_7);
                }
                var_s2 = var_s0_3;
                if (temp_a1_4->unk20 < 4) {
                    var_v0 = var_s2;
                    if (temp_a1_4->unk2CC == 0) {
                        if ((rand() & 0xFFF) >= gCpu->unk38E) {
                            gCpu->unk94 = -1;
                            if (gCpuOpponent->unk135 == 0) {
                                if (((rand() & 0x3F) < 0x20) && (gCpu->unkBC == NULL)) {
                                    var_a0_8 = func_00384408;
                                } else {
                                    goto block_100;
                                }
                            } else {
                            block_100:
                                var_a0_8 = func_00384370;
                            }
                            var_s2 = var_s0_3 + Cpu_PickCands(var_a0_8);
                        }
                        goto block_199;
                    }
                } else {
                block_199:
                    var_v0 = var_s2;
                }
            }
        } else {
            var_s0_4 = 0;
            if (temp_a1_3->unk7C != 0) {
                var_s2_3 = 0;
            } else {
                temp_s1_2 = ((rand() & 0xFFF) < gCpu->unk38E) ? var_s3 : -1;
                if (((s16) gCpuSelf->unk40 < 0x4001) || (rand() & 7) ||
                    (var_s0_4 = Cpu_PickCands(func_003849B8), var_s2_3 = var_s0_4, (var_s0_4 <= 0))) {
                    var_s2_3 = var_s0_4;
                    if (temp_s1_2 < 0) {
                        temp_a0_2 = rand() & 0xFFF;
                        if (temp_a0_2 < 0x334) {
                            var_a0_9 = func_003846D0;
                            goto block_60;
                        }
                        if (temp_a0_2 < 0x3AF) {
                            var_s0_5 = -1;
                            func_0038A690(rand() & 1);
                        } else {
                            if (temp_a0_2 < 0x6E2) {
                                var_a0_9 = func_00384648;
                                goto block_60;
                            }
                            if (temp_a0_2 < 0x75D) {
                                var_v0_5 = temp_a0_2 < 0x829;
                                if (!(gCpu->unk330.w & 1)) {
                                    var_a0_9 = func_00384920;
                                    goto block_60;
                                }
                                goto block_58;
                            }
                            var_v0_5 = temp_a0_2 < 0x829;
                        block_58:
                            if (var_v0_5 != 0) {
                                var_a0_9 = func_003849A0;
                            block_60:
                                var_v0_6 = Cpu_PickCands(var_a0_9);
                                goto block_65;
                            }
                            if (((u32) (gCpu->unk20 - 2) < 2U) && (var_s0_5 = -1, ((temp_a0_2 < 0x9C3) != 0))) {
                                Cpu_SetCommand((u16 *) D_00507482);
                            } else {
                                var_v0_6 = Cpu_PickCands(func_00384688);
                            block_65:
                                var_s0_5 = var_v0_6;
                            }
                        }
                        var_s2_3 = var_s0_5;
                    }
                }
            }
            var_v0 = var_s2_3;
            if (var_s2_3 < 0) {
                var_v0 = -1;
            }
        }
    }
    return var_v0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003865A0);
#endif


s32 func_003865A0(s32 wait);
s32 func_003854B0(void);

/* Decider 0x12 of Cpu_Think, the general attack choice [V code; I meaning]: holds the previous buttons' high nibble while
 * unk74 counts down or unk64 is at least 5; nothing from round unk20 4 on, with unk2CC set or once unk258 reaches unk25C;
 * in round 3 it acts one time in 12 and from round 2 on not one time in 8. While unk50 is set it holds 0x8000 when unk9C
 * is positive or unk2AC is set before unk24 4, and drops out one time in 16 against an opponent's move of flag 4. Then
 * picks candidates (func_003865A0 with the wait unk94; from unk24 3 on, when nothing was picked, the own fighter is not
 * airborne and its life is at most a tenth of its red life: func_00383EF0 candidates one time in 4 with a chance of
 * unk368), rejects func_00384B80 ones unless unk3E4 is 1 and plays one (func_003854B0). The move played can start the
 * follow-up plans: unk12C for a move of flag 0x20 (unk24, not in the mode D_005F5CBC), unk14C half the time when the
 * own move has flag 0x80000 or three times in four for a move of flag 0x80000 (unk4) that func_00384B80 rejects or
 * accepts with unk3E4 set. A started move of kind 0x2021 / 0x2829 sets unk74 to 12; a move with an attack window or
 * flag 0x80000 is kept in unk90. A negative pick (func_003865A0) resets the timers and returns 1. */
s32 func_00387120(void) {
    s32 r;
    s32 k;
    struct FighterMove *m;
    u16 kind;

    if (gCpu->unk74 > 0) {
        gCpu->unk74--;
        gCpu->unk6 = gCpu->unk8 & 0xF000;
        return 1;
    }
    if (gCpu->unk64 >= 5) {
        gCpu->unk6 = gCpu->unk8 & 0xF000;
        return 1;
    }
    if (gCpu->unk20 >= 4 || gCpu->unk2CC != 0) {
        return 0;
    }
    if (gCpu->unk258 >= gCpu->unk25C) {
        goto fail;
    }
    if (gCpu->unk20 >= 3 && Cpu_RandBelow(12) != 0) {
        goto fail;
    }
    if (gCpu->unk20 >= 2 && Cpu_RandBelow(8) == 0) {
        goto fail;
    }
    if (gCpu->unk50 != 0) {
        if (gCpu->unk9C > 0 || (gCpu->unk2AC != 0 && gCpu->unk24 < 4)) {
            gCpu->unk6 = 0x8000;
            return 1;
        }
        if ((gCpu->unk33C.w & 4) && Cpu_RandBelow(0x1000) < 0x100) {
            goto fail;
        }
    }
    r = func_003865A0(gCpu->unk94);
    if (r == 0 && gCpu->unk24 >= 3 && gCpuSelf->unk16A == 0 && gCpuSelf->unk3F8 / 10 >= gCpuSelf->unk3F4 &&
        !(rand() & 3) && (rand() & 0xFFF) < gCpu->unk368) {
        r = Cpu_PickCands(func_00383EF0);
    }
    if (r < 0) {
        gCpu->unk6C = -1;
        gCpu->unk5C = 0x28;
        gCpu->unk4C = 0;
        gCpu->unk50 = 0;
        return 1;
    }
    if (gCpu->unk3E4 != 1) {
        Cpu_RejectCands(func_00384B80);
    }
    k = func_003854B0();
    if (k < 0) {
    fail:
        return 0;
    }
    gCpu->unk5C = 0x28;
    gCpu->unk6C = -1;
    gCpu->unk50 = 0;
    gCpu->unk4C = 0;
    m = gCpuCands[k].unk4;
    if (D_005F5CBC == 0 && (m->unk24 & 0x20)) {
        gCpu->unk12C = 1;
    } else if ((gCpu->unk330.w & 0x80000) && (rand() & 0x3F) < 0x20) {
        gCpu->unk14C = 1;
    } else if (((m->unk4 & 0x80000) && !func_00384B80(&gCpuCands[k])) ||
               ((m->unk4 & 0x80000) && func_00384B80(&gCpuCands[k]) && gCpu->unk3E4 != 0)) {
        if ((rand() & 0x3F) < 0x30) {
            gCpu->unk14C = 1;
        }
    }
    kind = 0;
    if (gCpu->unk8C != NULL) {
        kind = gCpu->unk8C->unk4;
    }
    if (kind == 0x2021 || kind == 0x2829) {
        gCpu->unk74 = 12;
    }
    if (m->unk2C != 0 || (m->unk4 & 0x80000)) {
        gCpu->unk90 = m;
    }
    return 1;
}

/* A filter of the CPU's answers: fails with a chance of unk38E in 4096 and one time in 32; then passes in the first two
 * rounds of unk20 while unk84 is not negative, from round 4 on, when the opponent's move has flag 4 (its unk135 clear)
 * and the move `m` is not of flag 0x10000 nor 0x10F / 0x51F, or when the own unk40 is at least 0x3801 [V]. */
s32 func_00387518(struct FighterMove *m) {
    s32 r = 0;
    CpuState *c;

    if ((rand() & 0xFFF) < gCpu->unk38E) {
        goto end;
    }
    if ((rand() & 0x1F) == 0) {
        goto end;
    }
    c = gCpu;
    if (c->unk20 < 2 && c->unk84 >= 0) {
        goto one;
    }
    if (c->unk20 >= 4) {
        goto one;
    }
    if ((c->unk33C.w & 4) && gCpuOpponent->unk135 == 0 && !(m->unk8 & 0x10000) && (u16)m->unk4 != 0x10F &&
        (u16)m->unk4 != 0x51F) {
        goto one;
    }
    if ((s16)gCpuSelf->unk40 < 0x3801) {
        goto end;
    }
one:
    r = 1;
end:
    return r;
}

/* Decider 0xE of Cpu_Think, the follow-up plan of func_003856C0 (while unk12C > 0): of the unk130 commands listed in
 * unk134, takes those inside their frame window whose condition, Cpu_IsCmdAllowed and situation hold. While the own
 * unk40 is below 0x2000, with a chance of 4096 - unk378 it holds the previous buttons' high nibble, else plays one
 * of them at random (seeded) when its move has no attack window or the plan has not run more than unk376 times and
 * unk2A8 reaches the window. Otherwise the plan ends (unk12C / unk130 cleared, unk144 set) [V]. */
s32 func_00387600(void) {
    s32 n = 0;
    CpuCmd *list[4];
    s32 i;
    CpuCmd *c;
    Fighter *f;
    s32 ok;
    s32 k;
    struct FighterMove *m;

    if (gCpu->unk12C <= 0) {
        return 0;
    }
    for (i = 0; i < gCpu->unk130; i++) {
        f = gCpuSelf;
        c = (CpuCmd *)gCpu->unk134[i];
        ok = 0;
        if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && Cmd_CheckCondition(f, gCpuOpponent, c->unk2) &&
            Cpu_IsCmdAllowed(c)) {
            ok = Cmd_CheckSituation((MoveCmd *)c, f, gCpuOpponent) != 0;
        }
        if (ok) {
            list[n++] = c;
        }
    }
    if (n <= 0 || (s16)gCpuSelf->unk40 >= 0x2000) {
        goto fail;
    }
    if ((rand() & 0xFFF) >= gCpu->unk378) {
        gCpu->unk6 = gCpu->unk8 & 0xF000;
        return 1;
    }
    k = n * ((rand() + D_005F6C08) & 0x7FFF) >> 15;
    D_005F6C08 = D_005F6C08 * 5 + 3;
    m = D_005F6C00[(s16)gCpuSelf->unk20][list[k]->unk6];
    if (m->unk2C != 0 && (++gCpu->unk12C > gCpu->unk376 || gCpu->unk2A8 < m->unk2C)) {
    fail:
        gCpu->unk12C = 0;
        gCpu->unk130 = 0;
        gCpu->unk144 = 1;
        return 0;
    }
    func_00381278(list[k]->unk0, D_005F6C00[(s16)gCpuSelf->unk20][list[k]->unk6]);
    func_00385250(list[k], 1);
    return 1;
}

s32 func_00387518(struct FighterMove *m);

/* Decider 0xF of Cpu_Think, the follow-up plan of func_003857F8 (while unk14C > 0): of the unk150 commands listed in
 * unk154, takes those inside their frame window whose condition, Cpu_IsCmdAllowed and situation hold (bit 0x4000 only
 * with unk3E0). While the own unk40 is below 0x2000 it picks one at random (seeded); a move of flag 0x200000 counts in
 * unk14C. When its attack window (unk2C) is beyond unk2A8, unk14C passes unk37A, unk258 reaches unk25C or the filter
 * func_00387518 returns non-zero, the plan ends and the previous buttons' high nibble is held; otherwise the command is played,
 * and unless the move has flag 0x80000 the plan ends after it with a chance of 4096 - unk37C. No command: the plan
 * ends [V]. */
s32 func_003879A0(void) {
    s32 n = 0;
    CpuCmd *list[0x30];
    s32 i;
    CpuCmd *c;
    Fighter *f;
    s32 ok;
    s32 k;
    struct FighterMove *m;

    if (gCpu->unk14C <= 0) {
        return 0;
    }
    for (i = 0; i < gCpu->unk150; i++) {
        f = gCpuSelf;
        c = gCpu->unk154[i];
        ok = 0;
        if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && Cmd_CheckCondition(f, gCpuOpponent, c->unk2) &&
            Cpu_IsCmdAllowed(c)) {
            ok = Cmd_CheckSituation((MoveCmd *)c, f, gCpuOpponent) != 0;
        }
        if (ok && (gCpu->unk3E0 != 0 || !(c->unk0 & 0x4000))) {
            list[n++] = gCpu->unk154[i];
        }
    }
    if (n <= 0 || (s16)gCpuSelf->unk40 >= 0x2000) {
        gCpu->unk14C = 0;
        gCpu->unk150 = 0;
        return 0;
    }
    k = n * ((rand() + D_005F6C08) & 0x7FFF) >> 15;
    D_005F6C08 = D_005F6C08 * 5 + 3;
    m = D_005F6C00[(s16)gCpuSelf->unk20][list[k]->unk6];
    if (m->unk8 & 0x200000) {
        gCpu->unk14C++;
    }
    if (m->unk2C != 0 && (gCpu->unk2A8 < m->unk2C || gCpu->unk37A < gCpu->unk14C || gCpu->unk258 >= gCpu->unk25C ||
                          func_00387518(m))) {
        gCpu->unk14C = 0;
        gCpu->unk150 = 0;
        gCpu->unk6 = gCpu->unk8 & 0xF000;
        return 1;
    }
    if (!(m->unk4 & 0x80000) && (rand() & 0xFFF) >= gCpu->unk37C) {
        gCpu->unk14C = 0;
        gCpu->unk150 = 0;
    }
    func_00381278(list[k]->unk0, m);
    func_00385250(list[k], 1);
    return 1;
}

/* Decider 0x10 of Cpu_Think, the follow-up plan of func_00385A60 (while unk220 is not negative): of the unk224
 * commands listed in unk228, takes those inside their frame window whose condition, Cpu_IsCmdAllowed and situation
 * hold. While the own unk40 is below 0x2000, with a chance of 4096 - unk380 it holds the previous buttons' high
 * nibble, else plays one of them at random (seeded) unless its move has an attack window and unk220 reached unk37E,
 * unk258 reached unk25C, unk2A8 is below the window or func_00387518 returns non-zero. Otherwise the plan ends
 * (unk220 / unk224 cleared) [V]. */
s32 func_00387DA8(void) {
    s32 n = 0;
    CpuCmd *list[10];
    s32 i;
    CpuCmd *c;
    Fighter *f;
    s32 ok;
    s32 k;
    struct FighterMove *m;

    if (gCpu->unk220 < 0) {
        return 0;
    }
    for (i = 0; i < gCpu->unk224; i++) {
        f = gCpuSelf;
        c = gCpu->unk228[i];
        ok = 0;
        if (f->unkC4 >= c->unkB && f->unkC4 <= c->unkC && Cmd_CheckCondition(f, gCpuOpponent, c->unk2) &&
            Cpu_IsCmdAllowed(c)) {
            ok = Cmd_CheckSituation((MoveCmd *)c, f, gCpuOpponent) != 0;
        }
        if (ok) {
            list[n++] = gCpu->unk228[i];
        }
    }
    if (n <= 0 || (s16)gCpuSelf->unk40 >= 0x2000) {
        goto fail;
    }
    if ((rand() & 0xFFF) >= gCpu->unk380) {
        gCpu->unk6 = gCpu->unk8 & 0xF000;
        return 1;
    }
    k = n * ((rand() + D_005F6C08) & 0x7FFF) >> 15;
    D_005F6C08 = D_005F6C08 * 5 + 3;
    m = D_005F6C00[(s16)gCpuSelf->unk20][list[k]->unk6];
    if (m->unk2C != 0 && (gCpu->unk220 >= gCpu->unk37E || gCpu->unk258 >= gCpu->unk25C || gCpu->unk2A8 < m->unk2C ||
                          func_00387518(m))) {
    fail:
        gCpu->unk220 = 0;
        gCpu->unk224 = 0;
        return 0;
    }
    func_00381278(list[k]->unk0, D_005F6C00[(s16)gCpuSelf->unk20][list[k]->unk6]);
    func_00385250(list[k], 1);
    return 1;
}

/* Decider 8 of Cpu_Think. Waits while the opponent's attack is more than 30 frames from its window (when unk9C < 0);
 * then holds the pad word of func_00386428 (unkA4 set, unk98 clear), or, while the own fighter's unk11A is set, the one
 * of func_00386470 with a chance of unk3B6 in 4096, or else repeats the previous buttons' high nibble while unk98 is
 * set [V]. */
s32 func_00386428(void);
s32 func_00386470(void);

/* Decider 8 of Cpu_Think. Waits while the opponent's attack is more than 30 frames from its window (when unk9C < 0);
 * then holds the pad word of func_00386428 (unkA4 set, unk98 clear), or, while the own fighter's unk11A is set, the one
 * of func_00386470 with a chance of unk3B6 in 4096, or else repeats the previous buttons' high nibble while unk98 is
 * set [V]. */
s32 func_00388170(void) {
    s32 r = 0;

    if (gCpu->unk9C < 0) {
        Fighter *o = gCpuOpponent;

        if (o->unkC0->unk2C != 0 && o->unkC0->unk2C - 0x1E >= o->unkC4) {
            return 0;
        }
    }
    if (gCpu->unkA4 != 0 && gCpu->unk98 == 0) {
        s32 v = func_00386428();

        if (v != 0) {
            r = 1;
            gCpu->unk6 = v;
        }
    } else if (gCpuSelf->unk11A != 0) {
        s32 v = func_00386470();

        if (v != 0 && (rand() & 0xFFF) < gCpu->unk3B6) {
            gCpu->unk6 = v;
            r = 1;
        }
    } else if (gCpu->unk98 != 0) {
        r = 1;
        gCpu->unk6 = gCpu->unk8 & 0xF000;
    }
    return r;
}

s32 func_00386428(void);

/* Inlined twice by func_00388278: for a fighter of variant 7, holds 0x8000 when the own unk40 is at least 0x4000, else
 * 0x2000, and returns 1 [V code]. */
static inline s32 Cpu_Variant7Step(void) {
    if (gCpuSelf->unk28 == 7) {
        gCpu->unk6 = (s16)gCpuSelf->unk40 >= 0x4000 ? 0x8000 : 0x2000;
        return 1;
    }
    return 0;
}

/* Decider 0x11 of Cpu_Think (only while unk7C is clear) [V code; I meaning]:
 * - against an opponent's move with an early window (unk19) while unk24 < 2, with a chance of unk390 in 4096: holds
 *   0x6000 when the own unk40 is below 0x4000 and unk34 below -10, or 0x2000 when the opponent's move has more than 20
 *   frames left (unk1A - unkC4) and unk40 is above 0x4000;
 * - with unk40 above 0x6000 while unk24 < 2: holds nothing when the opponent's unk135 is set, 0x4040 / 0x4020 by the
 *   frame parity when unk2A4 is set, else with a chance of unk38E the step of Cpu_Variant7Step or 0x2000, or waits
 *   (unk94 = -2, returns 0);
 * - with unk40 above 0x1000, a chance of unk40 in 0x8000 and one of 4096 - unk38E: calls func_00380540(8) and holds
 *   the step of Cpu_Variant7Step, the pad word of func_00386428 (with unk2AC, one time in 8), 0x8000 (half the time
 *   while unk24 < 3) or 0x2000. */
s32 func_00388278(void) {

    if (gCpu->unk7C != 0) {
        return 0;
    }
    if (gCpuOpponent->unkC0->unk19 != 0 && gCpu->unk24 < 2 && (rand() & 0xFFF) < gCpu->unk390) {
        if ((s16)gCpuSelf->unk40 < 0x4000 && gCpu->unk34 < -0xA) {
            gCpu->unk6 = 0x6000;
            return 1;
        }
        if (gCpuOpponent->unkC0->unk1A - gCpuOpponent->unkC4 >= 0x15 && (s16)gCpuSelf->unk40 >= 0x4001) {
            gCpu->unk6 = 0x2000;
            return 1;
        }
    }
    if ((s16)gCpuSelf->unk40 >= 0x6001 && gCpu->unk24 < 2) {
        if (gCpuOpponent->unk135 != 0) {
            gCpu->unk6 = 0;
        } else if (gCpu->unk2A4 != 0) {
            gCpu->unk6 = (gCpu->unk4 & 1) == gCpu->unk0 ? 0x4040 : 0x4020;
        } else if ((rand() & 0xFFF) < gCpu->unk38E) {
            if (!Cpu_Variant7Step()) {
                gCpu->unk6 = 0x2000;
            }
        } else {
            gCpu->unk94 = -2;
            return 0;
        }
    } else {
        if ((s16)gCpuSelf->unk40 < 0x1001 || rand() >= (s16)gCpuSelf->unk40 || (rand() & 0xFFF) < gCpu->unk38E) {
            return 0;
        }
        func_00380540(8);
        if (!Cpu_Variant7Step()) {
            if (gCpu->unk2AC != 0 && !(rand() & 7)) {
                gCpu->unk6 = func_00386428();
            } else if (gCpu->unk24 < 3 && (rand() & 1)) {
                gCpu->unk6 = 0x8000;
            } else {
                gCpu->unk6 = 0x2000;
            }
        }
    }
    return 1;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005074C8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005074D8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005074E8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005074F0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507520);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507548);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507558);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507578);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507588);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005075D8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005075F0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005075F8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507608);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507610);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507618);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507620);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507860);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507868);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00507870);

/* The situation code of decider 0x13 (func_00388840), 0-10, drawn at random (Cpu_RandBelow(0x1000)) [V code; I
 * meaning]. 0 after unk288[3]. While the wait unk94 is at most 35 it depends on the round unk20, otherwise on unk24;
 * the bands of the draw differ per round (unk36C in 1000ths, unk330 bit 2, unk38 below 721 with unk2A4 clear). The
 * codes 3 and 6 stay only with the chance unk394 (second table) and unk38E (when the opponent's move has flag 0x80000
 * or unk340 is set). */
/* NON_MATCHING: 39 lines differ: gCpu and its copy swap a2 / a3, switch1 case 2 (r = 1) and case 5 (r = 1) are
 * branch-shaped in the original where ours hoists the store or makes a movz, and the 0x22D test of the second
 * table. `n > K` written as `if (n <= K) r = a; else r = b;` gives the original's `li K / slt / movn`; the limit
 * 0x23 in a variable as in func_00388840. */
#if 0
s32 func_00388530(void) {
    s32 r = 0;
    s32 n = Cpu_RandBelow(0x1000);
    CpuState *c;
    s32 lim = 0x23; /* fake */

    if (gCpu->unk288[3] != 0) {
        return 0;
    }
    c = gCpu;
    switch (gCpu->unk94 > lim) {
    case 0:
        switch (gCpu->unk20) {
        case 0:
        case 1:
            if (n <= 0x13) {
                r = 3;
            } else {
                r = 0;
            }
            break;
        case 2:
            r = 5;
            if (n >= 0x7A) {
                if (n < 0x1EB) {
                    r = 1;
                } else {
                    r = 3;
                    if (n >= 0x214) {
                        r = (n < (c->unk36C << 12) / 1000) * 8;
                    }
                }
            }
            break;
        case 3:
            r = 2;
            if (n >= 0x666) {
                if (n < 0xB33) {
                    r = 1;
                } else {
                    r = 0;
                }
            }
            break;
        case 4:
            r = 2;
            if (n >= 0xCC) {
                if (n <= 0x598) {
                    r = 4;
                } else {
                    r = 1;
                }
            }
            break;
        case 5:
            r = 4;
            if (c->unk38 < 0x2D1 && gCpu->unk2A4 == 0 && n >= 0x11E) {
                r = 1;
            }
            break;
        }
        break;
    case 1:
        switch (gCpu->unk24) {
        case 0:
        case 1:
            r = 6;
            if (n >= 0x199) {
                if (n <= 0x332) {
                    r = 3;
                } else {
                    r = 0;
                }
            }
            break;
        case 2:
            r = 2;
            if (n >= 0x28) {
                if (n <= 0x16F) {
                    r = 3;
                } else {
                    r = 0;
                }
            }
            break;
        case 3:
            if (n < 0x1EB) {
                r = 2;
            } else if (n < 0x214) {
                r = 5;
            } else if (n >= 0x218) {
                if (n < 0x22D) {
                    r = 1;
                } else if (n < 0x241) {
                    r = (c->unk330.w << 2) & 8;
                }
            } else {
                r = 3;
            }
            break;
        case 4:
            if (n < 0x80) {
                r = 2;
            } else if (n < 0x90) {
                r = 5;
            } else if (n < 0xC0) {
                r = 8;
            } else if (n < 0x3C0) {
                r = 1;
            } else {
                r = 0;
            }
            break;
        case 5:
            r = 4;
            if (c->unk38 < 0x2D1 && gCpu->unk2A4 == 0 && n >= 0x51) {
                r = 2;
                if (n >= 0xCC) {
                    r = 1;
                    if (n >= 0x400) {
                        if (n <= 0x5EA) {
                            r = 5;
                        } else {
                            r = 0;
                        }
                    }
                }
            }
            break;
        }
        if (r == 3 || r == 6) {
            if (!((rand() & 0xFFF) < gCpu->unk394)) {
                r = 0;
            }
        }
        break;
    }
    if ((r == 3 || r == 6) && ((gCpu->unk33C.w & 0x80000) || gCpu->unk340 != 0)) {
        if ((rand() & 0xFFF) < gCpu->unk38E) {
            r = 0;
        }
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388530);
#endif

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00507890);

void func_0038A690(s32 n);
s32 func_00388530(void);
extern const u16 D_005074BA[];

/* Decider 0x13 of Cpu_Think (only while unkCC is negative) [V code; I meaning]. While the wait unk4C runs: in round 0,
 * after unk288[3] with D_005F5C28 below 7001, or (chance 4096 - unk38E) when func_00383D10 says so, the timers are
 * reset and it goes on below; with unk50 == 2 it holds 0x8000 (resetting the timers: after unk288[3], or one time in
 * 128 while unk24 < 3) or 0x2000; else, while unk94 is at most 35 in unk24 2, one time in 16 it plays a func_00384988
 * candidate (wait 0), otherwise it holds the previous buttons' high nibble. Without a wait (unk7C clear, unk70 / unk6C /
 * unk68 negative) and with unk5C / unk6C negative it acts on the situation func_00388530 returns: 1 / 4 / 6 / 9 / 10
 * play a candidate of func_00384988 / func_00384988 (wait 0xFA, kind 2) / func_003849A0 (unk5C 0x3C) / func_003849B8 /
 * func_00384A50, 8 plays one after a side step (unk24 >= 3 or unk2A8 >= 11); 2 (or 1 without a candidate) waits 4-11
 * frames (rounds 0-2), 10-19 (round 3) or 15-29 and holds 0x2000 (0x6000 with unk54); 5 (or 4) waits 40-59 frames and
 * holds the same; 3 (or 6) waits 20-24 frames and holds 0x8000 (0xC000 with unk54); 7 plays the command D_005074BA
 * (unk5C 0x50) half the time. */
s32 func_00388840(void) {
    s32 lim = 0x23; /* fake: the limit in a variable gives the original `li 35 / slt` */

    if (gCpu->unkCC >= 0) {
        return 0;
    }
    if (gCpu->unk4C >= 0) {
        if (gCpu->unk288[3] != 0 && D_005F5C28 < 0x1B59) {
            gCpu->unk5C = 0x28;
            gCpu->unk6C = -1;
            gCpu->unk50 = 0;
            gCpu->unk4C = 0;
        } else if (gCpu->unk20 <= 0 || ((rand() & 0xFFF) >= gCpu->unk38E && func_00383D10((CpuCand *)1))) {
            gCpu->unk5C = 0x28;
            gCpu->unk50 = 0;
            gCpu->unk4C = 0;
            gCpu->unk6C = -1;
        } else {
            if (gCpu->unk50 == 2) {
                if (gCpu->unk288[3] != 0 && D_005F5C28 < 0x1B59) {
                    gCpu->unk5C = 0x28;
                    gCpu->unk6C = -1;
                    gCpu->unk50 = 0;
                    gCpu->unk4C = 0;
                    gCpu->unk6 = 0x8000;
                } else if (gCpu->unk24 < 3 && (rand() & 0xFFF) < 0x20) {
                    gCpu->unk5C = 0x28;
                    gCpu->unk6C = -1;
                    gCpu->unk50 = 0;
                    gCpu->unk4C = 0;
                    gCpu->unk6 = 0x8000;
                } else {
                    gCpu->unk6 = 0x2000;
                }
                return 1;
            }
            if (gCpu->unk94 <= lim && gCpu->unk24 == 2 && !(rand() & 0xF) && Cpu_PickCands(func_00384988) > 0 &&
                func_003853E8() >= 0) {
                func_00380540(0);
                return 1;
            }
            gCpu->unk6 = gCpu->unk8 & 0xF000;
            return 1;
        }
    } else if (gCpu->unk7C != 0 || gCpu->unk70 >= 0 || gCpu->unk6C >= 0 || gCpu->unk68 >= 0) {
        return 0;
    }
    if (gCpu->unk5C >= 0 || gCpu->unk6C >= 0) {
        return 0;
    }
    switch (func_00388530()) {
    case 1:
        if (Cpu_PickCands(func_00384988) > 0) {
            func_00380540(0);
            break;
        }
    case 2:
        switch (gCpu->unk20) {
        case 0:
        case 1:
        case 2:
            func_00380540(Cpu_RandBelow(8) + 4);
            break;
        case 3:
            func_00380540(Cpu_RandBelow(10) + 10);
            break;
        default:
            func_00380540(Cpu_RandBelow(15) + 15);
            break;
        }
        gCpu->unk6 = gCpu->unk54 > 0 ? 0x6000 : 0x2000;
        return 1;
    case 4:
        if (Cpu_PickCands(func_00384988) > 0) {
            func_00380528(0xFA, 2);
            break;
        }
    case 5:
        func_00380540(Cpu_RandBelow(20) + 0x28);
        gCpu->unk6 = gCpu->unk54 > 0 ? 0x6000 : 0x2000;
        return 1;
    case 7:
        if (rand() & 0x40) {
            return 0;
        }
        gCpu->unkC = (u16 *)D_005074BA;
        gCpu->unk5C = 0x50;
        return 1;
    case 6:
        if (Cpu_PickCands(func_003849A0) > 0) {
            gCpu->unk5C = 0x3C;
            break;
        }
    case 3:
        func_00380540(Cpu_RandBelow(5) + 0x14);
        gCpu->unk6 = gCpu->unk54 > 0 ? 0xC000 : 0x8000;
        return 1;
    case 9:
        if (Cpu_PickCands(func_003849B8) <= 0) {
            return 0;
        }
        func_00380540(Cpu_RandBelow(5) + 0x14);
        break;
    case 8:
        if (gCpu->unk24 >= 3 || gCpu->unk2A8 >= 0xB) {
            func_0038A690(-1);
        }
        break;
    case 10:
        if (gCpu->unk5C < 0 && Cpu_PickCands(func_00384A50) <= 0) {
            return 0;
        }
        break;
    case 0:
    default:
        return 0;
    }
    return func_003853E8() >= 0;
}

s32 func_00388D70(void) {
    if (gCpu->unk70 >= 0) {
        gCpu->unk6 = 0x4000;
    } else if (gCpu->unk6C >= 0) {
        gCpu->unk6 = gCpu->unk8 & 0xF000;
    } else {
        gCpu->unk6 = 0;
    }
    return 1;
}

extern u16 D_005F5CBC;
extern u16 gFrameCount;

s32 func_00388DA8(void) {
    if (D_005F5CBC != 0 && (gFrameCount & 1) == gCpu->unk0) {
        gCpu->unk6 = gCpu->unk8 & 0xF000;
        return 1;
    }
    return 0;
}

s32 func_003835A0(void);
s32 func_00384B50(CpuCand *c);
s32 func_00384B68(CpuCand *c);
s32 func_00384B80(CpuCand *c);

/* Decider 3 of Cpu_Think: while the own move has flags unk4 == 0x4C02 (low half) and no attack window. Character 0x21
 * sets unk278 with a chance of 25 in 4096 (unk3DC below 2); then the candidates of func_003835A0 are tried with the
 * filters func_00384B68 (chance unk3AC, also sets unk94 = -1, unkE0 = unkE4 = 1), func_00384B80 (unk3B4) and
 * func_00384B50 (unk3AA); without one the pad is released [V]. */
s32 func_00388DE0(void) {
    CpuState *c = gCpu;
    Fighter *f;

    if (c->unk330.h != 0x4C02) {
        return 0;
    }
    f = gCpuSelf;
    if (f->unkC0->unk2C != 0) {
        return 0;
    }
    c->unk5C = 0x28;
    c->unk6C = -1;
    c->unk50 = 0;
    c->unk4C = 0;
    if ((u32)c->unk3DC < 2 && f->unk2A == 0x21 && (rand() & 0xFFF) < 0x19) {
        gCpu->unk278 = 1;
    }
    gCpu->unk88 = gCpuSelf->unkC0;
    gCpu->unkB0 = 0;
    gCpu->unkAC = func_003835A0();
    if (gCpu->unkAC > 0) {
        if ((rand() & 0xFFF) < gCpu->unk3AC && Cpu_PickCands(func_00384B68) > 0) {
            gCpu->unkE0 = 1;
            gCpu->unkE4 = 1;
            gCpu->unk94 = -1;
            func_003853E8();
            return 1;
        }
        if ((rand() & 0xFFF) < gCpu->unk3B4 && Cpu_PickCands(func_00384B80) > 0) {
            func_003853E8();
            return 1;
        }
        if ((rand() & 0xFFF) < gCpu->unk3AA && Cpu_PickCands(func_00384B50) > 0) {
            func_003853E8();
            return 1;
        }
    }
    gCpu->unk6 = 0;
    return 1;
}

s32 func_00388F78(void) {
    s32 r = 0;

    if ((gCpu->unk338.h != 0 && gCpu->unk338.h != 0x607) || gCpu->unk2B0 > 0) {
        r = 1;
    }
    return r;
}

RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005078E0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00507910);

/* NON_MATCHING: 87 lines differ, mostly branch offsets: the 0xBA9 early return is cross-jumped into the `mask == 0`
 * one, `mask = rand() & 0x3F ? 2 : 4` and the 0x38 / 0x28 choice are if-converted (movn / movz) where the original
 * branches, and the shared `unk48 = 0` stores are placed differently. With it the jump table jtbl_00507910 comes from
 * the C (drop its INCLUDE_RODATA). */
#if 0
extern const s64 D_005078E0[];
s32 func_00386388(s32 kind);
s32 func_003835A0(void);
s32 func_00384578(CpuCand *c);
s32 func_00383F10(CpuCand *c);

/* Decider 4 of Cpu_Think, the answer to an opponent's move of flags 0x404 (own unk330, own fighter's unk135 clear)
 * [V code; I meaning]: counts the frames (unk324, from 0 while unk320 is negative); with a command of situation 0x34
 * (func_00386388) it presses 0x10 every unkA0 frames (unkA0 refilled from the first four parameters at random).
 * Otherwise it lists the own move's commands that apply now (func_003835A0) and, from a mask of answers chosen from
 * the opponent's move (window unk2C / unk2D, kinds 0x10F / 0x607, flags 0x10000 / 0x200000 / 2, the ranges unk2A8 /
 * unk34, the rounds unk20 / unk24, the frame parity, character 0xE's move 0xBA9), draws an answer from D_005078E0
 * until one is in the mask: 1 holds 0x1000, 2 holds 0x80 (0x4080 every 8 frames), 4 holds 0x8000, 8 / 0x10 / 0x20 play a
 * candidate of func_00383F10 / func_003840F8 / func_00384578 (unk320 = 50); without one, 0x1000 every 16 frames after
 * 180 frames (unk328), else nothing. */
s32 func_00388FA8(void) {
    s32 mask;
    s32 k;

    if (gCpuSelf->unk135 != 0 || !(gCpu->unk330.w & 0x404)) {
        gCpu->unk48 = 0;
        gCpu->unk328 = 0;
        return 0;
    }
    if (gCpu->unk320 >= 0) {
        gCpu->unk324++;
    } else {
        gCpu->unk324 = 0;
    }
    if (func_00386388(0x34)) {
        gCpu->unk48 = 1;
        if (--gCpu->unkA0 < 0) {
            gCpu->unkA0 = ((s16 *)gCpu->unk344)[rand() & 3];
            gCpu->unk6 = 0x10;
        } else {
            gCpu->unk6 = 0;
        }
        return 1;
    }
    gCpu->unk88 = gCpuSelf->unkC0;
    gCpu->unkB0 = 0;
    gCpu->unkAC = func_003835A0();
    if (gCpu->unkAC <= 0) {
        gCpu->unk6 = 0;
        return 1;
    }
    gCpu->unk48 = 1;
    gCpu->unk5C = 0x28;
    gCpu->unk6C = -1;
    gCpu->unkCC = 0x10;
    gCpu->unk50 = 0;
    gCpu->unk4C = 0;
    if (++gCpu->unk328 >= 0xB4) {
        mask = 0x3F;
    } else {
        if (((gCpu->unk33C.w & 0x200000) && gCpuOpponent->unkC0->unk2C == 0) ||
            (gCpu->unk340 == 0x607 && !(gCpu->unk338.w & 0x10000))) {
            if (gCpuOpponent->unkC4 < gCpuOpponent->unkC0->unk2D && gCpuOpponent->unk28 == 0xE && gCpuOpponent->unk10C == 0xBA9 && D_005F5C28 < 2000) {
                gCpu->unk6 = 0;
                return 1;
            }
        }
        if (gCpuOpponent->unkC0->unk2C != 0) {
            if (gCpu->unk2A8 >= 0xC || gCpuOpponent->unkC0->unk2D < gCpuOpponent->unkC4) {
                if (gCpu->unk20 < 2) {
                    gCpu->unk48 = 0;
                    mask = (gCpu->unk33C.w & 2) ? 0x38 : 0x28;
                } else {
                    mask = 0;
                }
            } else if (gCpu->unk340 == 0x10F || (gCpu->unk338.w & 0x10000)) {
                mask = 0;
                if ((gCpu->unk4 & 1) == gCpu->unk0) {
                    gCpu->unk48 = 0;
                    if (rand() & 2) {
                        mask = 0x38;
                    } else if (gCpuOpponent->unkC0->unk19 == 0 && (s16)gCpuSelf->unk40 < 0x4001 &&
                               !(gCpu->unk33C.w & 0x200000)) {
                        mask = 4;
                    } else if (rand() & 0x3F) {
                        mask = 2;
                    } else {
                        mask = 4;
                    }
                }
            } else {
                if (gCpu->unk24 >= 3 || (gCpu->unk24 >= 2 && gCpu->unk34 >= 0xB)) {
                    mask = 1;
                } else {
                    mask = (rand() & 0xF) ? 0x3F : 0;
                }
                gCpu->unk48 = 0;
            }
        } else if (gCpu->unk20 < 3) {
            mask = 0;
            if ((gCpu->unk4 & 1) == gCpu->unk0) {
                gCpu->unk48 = 0;
                if (gCpu->unk94 >= 0) {
                    mask = 7;
                } else if (rand() & 0x3F) {
                    mask = 7;
                } else {
                    mask = 0x38;
                    gCpu->unk48 = 0;
                }
            }
        } else {
            mask = 0x3F;
            if (gCpu->unk24 < 4) {
                gCpu->unk48 = 0;
            } else {
                gCpu->unk48 = 0;
                mask = 7;
            }
        }
    }
    if (mask == 0x80) {
        gCpu->unk6 = 0;
        return 0;
    }
    if (mask == 0) {
        gCpu->unk6 = 0;
        return 1;
    }
    do {
        k = D_005078E0[func_003800D8(6)];
    } while (!(mask & k));
    switch (k) {
    case 1:
        gCpu->unk6 = 0x1000;
        return 1;
    case 2:
        gCpu->unk6 = (gCpu->unk4 & 7) ? 0x80 : 0x4080;
        return 1;
    case 4:
        gCpu->unk6 = 0x8000;
        return 1;
    case 8:
        Cpu_PickCands(func_00383F10);
        break;
    case 16:
        Cpu_PickCands(func_003840F8);
        break;
    case 32:
        Cpu_PickCands(func_00384578);
        break;
    }
    if (gCpu->unkB0 > 0) {
        if (func_003853E8() >= 0) {
            gCpu->unk320 = 0x32;
        }
    } else if (gCpu->unk328 < 0xB4 || ((gCpu->unk4 >> 1) & 7)) {
        gCpu->unk6 = 0;
    } else {
        gCpu->unk6 = 0x1000;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388FA8);
#endif

s32 func_00383D10(CpuCand *c);
s32 func_00388F78(void);
s32 func_0038ACA8(void);

/* The situation code of decider 0xD (func_00389D58), 0-10 [V code; I meaning]: 0 while a plan runs (unk2C0) or the
 * own fighter is hit (unkC8), 2 without unk9C / unk7C; otherwise, while the opponent is busy (unkF8, or unk2AC before
 * round 4; unkF8 becomes 2 after its window or with unk42 2, 1 with chance unk382 against kinds 0x607 / 0x706 / flag
 * 0x200000) it returns 4 for an early unkF8 1, then the character plays 6 / 7 / 8 / 9 at distances 8-9 / 22-26 /
 * 4-10 (chance unk3A2, func_00383D10, D_00507620), 1 / 2 against the opponent's moves of flag 0x400000 (chances unk382
 * / unk3A6, unkF0 2), 2 to keep or start an attack (func_00388F78, chances unk364 / unk3BC / unk366, unkF0 5), 1 or
 * 10 to repeat a remembered move (chances unk36A / unk368, 10 one time in 64 against kind 0x412) and 5 at distance
 * 9-11 against kind 0x412 (chance unk36C). */
/* NON_MATCHING: a draft from m2c's control flow (about 200 lines differ): the original keeps gCpu in a1 through
 * most tests, reloads it after each rand(), and lays several shared return blocks out differently. */
#if 0
s32 func_00389488(void) {
    CpuState *c = gCpu;
    Fighter *self;
    struct FighterMove *m;
    s32 hit;
    s16 id;
    s16 ch;
    u32 k;
    s32 near;
    s32 w;

    if (c->unk2C0 >= 0) {
        return 0;
    }
    if (c->unkC8 != 0) {
        return 0;
    }
    if (c->unk9C == 0 && c->unk7C == 0) {
        return 2;
    }
    if (c->unkF8 == 0 && (c->unk2AC == 0 || c->unk24 >= 4)) {
        return 0;
    }
    m = gCpuOpponent->unkC0;
    if ((m->unk2C != 0 && m->unk2D < gCpuOpponent->unkC4) || gCpuOpponent->unk42 == 2) {
        c->unkF8 = 2;
    }
    if (c->unkF8 == 0 && (c->unk340 == 0x607 || c->unk340 == 0x706 || (c->unk33C.w & 0x200000)) &&
        (rand() & 0xFFF) < gCpu->unk382) {
        gCpu->unkF8 = 1;
    }
    if (c->unkD0 < 0 && c->unkF8 != 2 && c->unkF8 != 0) {
        return 4;
    }
    self = gCpuSelf;
    if ((u32)(c->unk2A8 - 8) < 2 && c->unk334 == 0 && self->unk135 == 0 && D_00507620[self->unk28].unk0 != 0 &&
        !(gCpuOpponent->unkC0->unk24 & 8) && gCpuOpponent->unk135 == 0 && func_00383D10(NULL) != 0 &&
        (rand() & 0xFFF) < gCpu->unk3A2) {
        return 6;
    }
    if (self->unk28 == 9 && c->unk2A8 < 0x1B && c->unk2A8 >= 0x16 && c->unk334 == 0 && self->unk135 == 0 &&
        func_00383D10(NULL) != 0 && (rand() & 0xFFF) < gCpu->unk3A2) {
        return 7;
    }
    if ((u32)(c->unk2A8 - 4) < 7 && c->unk334 == 0 && gCpuSelf->unk135 == 0 && gCpuOpponent->unk135 == 0 &&
        func_00383D10(NULL) != 0 && (rand() & 0xFFF) < gCpu->unk3A2) {
        k = gCpu->unk340;
        if ((k == 0x412 || k == 0x217 || k == 0x607 || k == 0x706) && D_00507620[gCpuSelf->unk28].unk4 != NULL) {
            return 8;
        }
        if (gCpu->unk340 == 0x10F && D_00507620[gCpuSelf->unk28].unk8 != NULL) {
            return 9;
        }
    }
    if (c->unk338.w & 0x400000) {
        hit = 0;
        id = gCpuOpponent->unk10C;
        ch = gCpuOpponent->unk28;
        if (id == 0x1C1 || id == 0x1B8 || id == 0x1B9 || (ch == 0xF && id == 0x1AB) || (ch == 0 && id == 0x1AB) ||
            (ch == 3 && id == 0x189)) {
            hit = 1;
        }
        if (hit && (rand() & 0xFFF) < gCpu->unk382) {
            return 1;
        }
        if ((gCpu->unk338.w & 0x400000) && c->unk2A8 < 0x1E && (rand() & 0xFFF) < gCpu->unk3A6) {
            gCpu->unkF0 = 2;
            return 2;
        }
    }
    if (func_00388F78() != 0) {
        if (gCpu->unk98 != 0 || gCpu->unk7C != 0) {
            return 0;
        }
        if (gCpu->unkF0 != 0) {
            return 2;
        }
        if ((rand() & 0xFFF) < gCpu->unk364 || (func_0038ACA8() >= gCpu->unk3BA && (rand() & 0xFFF) < gCpu->unk3BC) ||
            (gCpu->unk2B0 >= 0x1F && (rand() & 0xFFF) < gCpu->unk366)) {
            gCpu->unkF0 = 5;
            return 2;
        }
    }
    c = gCpu;
    if (c->unk9C >= 9 && (rand() & 0x1F) == 0) {
        return 1;
    }
    if (c->unk298 != 0 && (rand() & 0xF) == 0) {
        return 1;
    }
    w = c->unkC8;
    if (gCpuOpponent->unkC0 == (struct FighterMove *)c->unk288[0]) {
        if (w != 0) {
            goto late;
        }
        if ((rand() & 0xFFF) < gCpu->unk36A) {
            return 1;
        }
        w = gCpu->unkC8;
    }
    if (w == 0 && (rand() & 0xFFF) < gCpu->unk368) {
        if (gCpu->unk340 != 0x412 || (rand() & 0x3F)) {
            return 1;
        }
        return 10;
    }
late:
    if (c->unk2B0 >= 0x1F && (rand() & 0xFFF) < gCpu->unk36A) {
        if (gCpu->unk340 != 0x412 || (rand() & 0x3F)) {
            return 1;
        }
        return 10;
    }
    near = 0;
    if ((u32)(c->unk2A8 - 9) < 3 && (s16)gCpuSelf->unk40 < 0x4000) {
        near = !(c->unk330.w & 0x400000);
    }
    if (near && c->unk24 == 2 && !(c->unk330.w & 1) && gCpuSelf->unk135 == 0 && c->unk7C == 0 &&
        c->unk340 == 0x412 && (rand() & 0xFFF) < gCpu->unk36C) {
        return 5;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00389488);
#endif

s32 func_00384708(CpuCand *c);

s32 func_00389B50(void) {
    return Cpu_PickCands(func_00384708);
}

s32 func_00389B50(void);
s32 func_00388F78(void);
s32 func_00384D78(void);
s32 func_00386428(void);
void func_0038A690(s32 n);

/* Decider 0xC of Cpu_Think (unk2AC set, unk2C0 not negative, unk24 below 4): plays a func_00389B50 candidate (unkE4
 * set or chance unk36E, unk2A8 in 2-29); else, while unk7C is clear and func_00388F78 holds, a func_00384D78
 * candidate (unkF0 set or chance unk372, the opponent's unk40 below 0x5000) or the pad word of func_00386428; else,
 * with a chance of unk36C, a side step (func_0038A690(-1), unk94 -= 10) when unk2A8 is 9-11, the own unk40 is below
 * 0x4000, the own move lacks flag 0x400000 and unk24 is at least 2 [V]. */
s32 func_00389B70(void) {
    CpuState *c = gCpu;
    s32 n;
    s32 ok;

    if (c->unk2AC == 0 || c->unk2C0 < 0 || c->unk24 >= 4) {
        return 0;
    }
    if (c->unkE4 != 0 || (rand() & 0xFFF) < gCpu->unk36E) {
        s32 d = gCpu->unk2A8;

        if (d < 0x1E) {
            if (d >= 2 && func_00389B50() > 0 && func_003853E8() >= 0) {
                return 1;
            }
        }
    }
    if (gCpu->unk7C == 0 && func_00388F78() != 0 && (gCpu->unkF0 != 0 || (rand() & 0xFFF) < gCpu->unk372) &&
        (s16)gCpuOpponent->unk40 < 0x5000) {
        n = func_00384D78();
        if (n > 0 && func_003853E8() >= 0) {
            return 1;
        }
        if (n != 0 && gCpu->unk3DC < 3) {
            return 1;
        }
        gCpu->unk6 = func_00386428();
        return 1;
    }
    if ((rand() & 0xFFF) < gCpu->unk36C) {
        ok = 0;
        if (gCpu->unk2A8 >= 9 && gCpu->unk2A8 < 12 && (s16)gCpuSelf->unk40 < 0x4000) {
            if (!(gCpu->unk330.w & 0x400000)) {
                ok = 1;
            }
        }
        if (ok && gCpu->unk24 >= 2) {
            func_0038A690(-1);
            gCpu->unk94 -= 10;
            return 1;
        }
    }
    return 0;
}

/* fake: the hook deciders' `func_003853E8() >= 0` matches (slti / xori) only with the 0 passed in through an inline
 * function's parameter; written in place it folds to nor / srl. */
static inline s32 Cpu_PlayPicked(s32 min) {
    return func_003853E8() >= min;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507990);
extern const s32 D_00507990[8];
s32 func_00384D78(void);
s32 func_00386428(void);
s32 func_0038A5F8(void);
s32 func_00384130(CpuCand *c);
s32 func_00384708(CpuCand *c);
s32 func_003847D8(CpuCand *c);
s32 func_00384898(CpuCand *c);
s32 func_00384038(CpuCand *c);
s32 func_003849A0(CpuCand *c);
s32 func_003849D0(CpuCand *c);
s32 func_00384A10(CpuCand *c);

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_005079B0);

/* Decider 0xD of Cpu_Think: acts on the situation code of func_00389488 [V code; I meaning]. 10 / 1 / 6 / 7 pick
 * candidates (func_00384920 with unk94 -1 and unkE0 set; func_00384130 / func_003847D8 / func_00384708 by the
 * opponent's kind, plus func_00384898 against 0x412; func_003849D0; func_00384A10) and play one (timers reset, unk5C
 * 40); 2 uses func_00384D78 or holds func_00386428's pad word; 3 / 4 run the step plan unkF8 (3 and 4 guard or step
 * by the opponent's angle and the distance unk2A8, 5 holds 0x2000 or side-steps, 6 plays D_005074A2 or holds 0x3000,
 * 8 plays func_003849D0 / func_003849A0 candidates; it ends when D_005F5C28 reaches 2951, 300 later against variant
 * 4), 5 side-steps, 8 / 9 play the character's commands of D_00507620. */
/* NON_MATCHING: a draft from m2c's control flow (about 280 lines differ): the returns of the step plan (unkF8)
 * share result blocks in the original (a result variable set before common exits), which ours emits per case. */
#if 0
s32 func_00389D58(void) {
    s32 ok = 0;
    s32 n = 0;
    CpuState *c;
    s32 d;

    switch (func_00389488()) {
    case 0:
        break;
    case 10:
        n = Cpu_PickCands(func_00384920);
        if (n > 0) {
            ok = 1;
            gCpu->unk94 = -1;
            gCpu->unkE0 = 1;
        }
        break;
    case 1:
        if (gCpu->unk2AC == 0) {
            n = Cpu_PickCands(func_00384130);
        } else if (gCpu->unk340 == 0x607 || gCpu->unk340 == 0x706 || (gCpu->unk33C.w & 0x200001)) {
            n = Cpu_PickCands(func_003847D8);
        } else {
            n = Cpu_PickCands(func_00384708);
        }
        ok = n > 0;
        if (gCpu->unk340 == 0x412) {
            n += Cpu_PickCands(func_00384898);
            ok = n > 0;
        }
        break;
    case 2:
        gCpu->unk278 = 1;
        n = func_00384D78();
        if (n == 0) {
            gCpu->unk6 = func_00386428();
            return 1;
        }
        ok = 1;
        if (n == -1) {
            return 1;
        }
        break;
    case 3:
    case 4:
        c = gCpu;
        if (c->unk2CC != 0) {
            return 0;
        }
        d = D_005F5C28;
        if (c->unkF8 == 1) {
            if (c->unk20 < 3 && !((rand() & 0xFFF) < gCpu->unk38E)) {
                gCpu->unkF8 = 3;
            } else {
                gCpu->unkF8 = D_00507990[func_003800D8(7)];
            }
            c = gCpu;
        }
        if (gCpuOpponent->unk28 == 4) {
            d -= 300;
        }
        if ((u32)(c->unkF8 - 1) >= 2 && d >= 0xB87) {
            c->unk6 = 0;
            c->unkF8 = 0;
            return 1;
        }
        switch (c->unkF8) {
        case 3:
            c->unk5C = 0x28;
            c->unk50 = 0;
            c->unk4C = 0;
            c->unk6C = -1;
            if (gCpuOpponent->unkC0->unk2C != 0 && gCpuOpponent->unkC0->unk2D < gCpuOpponent->unkC4) {
                return 0;
            }
            if (c->unk2A8 >= 0x1D) {
                if (c->unk20 >= 3) {
                    gCpu->unk6 = func_0038A5F8();
                    gCpu->unk94 = rand() & 0xF;
                    return 1;
                }
                return Cpu_PickCands(func_00384038) <= 0;
            }
            if (c->unk2A8 >= 9) {
                if (d >= 0xB87) {
                    c->unk6 = 0;
                    return 1;
                }
                if (c->unk20 >= 3) {
                    if ((s16)gCpuOpponent->unk40 < 0x4000) {
                        c->unk6 = 0x8000;
                        c->unkF8 = 4;
                        return 1;
                    }
                    return 0;
                }
                if (Cpu_PickCands(func_00384038) > 0) {
                    gCpu->unk94 = -1;
                    return 0;
                }
                return 1;
            }
            if (d >= 0xB87 || (s16)gCpuOpponent->unk40 >= 0x4000) {
                c->unk6 = 0;
            } else {
                c->unk6 = 0x8000;
            }
            return 1;
        case 4:
            c->unk6C = -1;
            c->unk5C = 0x28;
            c->unk50 = 0;
            c->unk4C = 0;
            if ((s16)gCpuOpponent->unk40 > 0x4000 && c->unk24 >= 2) {
                c->unk6 = 0x2000;
                return 1;
            }
            if (d >= 0xB87) {
                gCpu->unk6 = 0;
                return 1;
            }
            if (gCpu->unk2A8 >= 9 && gCpu->unk20 < 2) {
                if (Cpu_PickCands(func_00384038) > 0) {
                    gCpu->unk94 = -1;
                    return 0;
                }
                return 1;
            }
            Cpu_SetCommand((u16 *)D_005074A2);
            return 1;
        case 5:
            if (!(c->unk330.w & 0x400000) && (rand() & 7) == 0 && gCpu->unk2A8 >= 7) {
                if ((s16)gCpuOpponent->unk40 >= 0x4000) {
                    gCpu->unkF8 = 3;
                }
                func_0038A690(-1);
                return 1;
            }
            c->unk6 = 0x2000;
            return 0;
        case 6:
            c->unk5C = 0x28;
            c->unk6C = -1;
            c->unk50 = 0;
            c->unk4C = 0;
            if (d >= 0xB87) {
                c->unk6 = 0;
                return 1;
            }
            if (c->unk2A8 >= 0x1D) {
                c->unk6 = c->unk24 > 0 ? 0x8000 : 0;
                return 1;
            }
            if (c->unk24 >= 2 || c->unk2A8 < 8) {
                Cpu_SetCommand((u16 *)D_005074A2);
                return 1;
            }
            c->unk6C = 0x14;
            c->unk6 = 0x3000;
            return 1;
        case 8:
            if (D_00507620[gCpuSelf->unk28].unk0 == 0) {
                c->unkF8 = 1;
                return 0;
            }
            c->unk5C = 0x28;
            c->unk6C = -1;
            c->unk50 = 0;
            c->unk4C = 0;
            if (d >= 0xB87) {
                c->unk6 = 0;
                return 1;
            }
            if (c->unk2A8 >= 0x1D) {
                c->unk6 = c->unk24 > 0 ? 0x8000 : 0;
                return 1;
            }
            if (c->unk24 < 3 && c->unk2A8 < 8) {
                Cpu_PickCands(func_003849D0);
            } else {
                Cpu_PickCands(func_003849A0);
            }
            func_003853E8();
            return 1;
        default:
            return 0;
        }
    case 5:
        func_0038A690(-1);
        gCpu->unk94 -= 10;
        return 1;
    case 6:
        n = Cpu_PickCands(func_003849D0);
        ok = n > 0;
        break;
    case 7:
        n = Cpu_PickCands(func_00384A10);
        ok = n > 0;
        break;
    case 8:
        Cpu_SetCommand(D_00507620[gCpuSelf->unk28].unk4);
        return 1;
    case 9:
        Cpu_SetCommand(D_00507620[gCpuSelf->unk28].unk8);
        return 1;
    }
    if (ok && n > 0 && Cpu_PlayPicked(0)) {
        gCpu->unk6C = -1;
        gCpu->unk5C = 0x28;
        gCpu->unk50 = 0;
        gCpu->unk4C = 0;
    }
    return ok;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00389D58);
#endif

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_005079E0);

/* Runs the opponent-character hook unkB4 (while unkCC < 0, with chance unk382 in 4096): -1 nothing, -2 done, -3 play a
 * random picked candidate (func_003853E8), 0x2000 / 0x8000 hold that pad bit and call func_00380540(0x10), any other
 * value is the pad bits to hold (unk6); returns whether the CPU acted [V code; I meaning]. */
/* Decider 6 of Cpu_Think: while unkCC is negative, with a chance of unk382 in 4096, runs the hook unkB4 of the
 * opponent's character: -1 nothing, -2 acted, -3 play a picked candidate, 0x2000 / 0x8000 hold that pad bit and wait
 * 16 frames, any other value is the pad word to hold [V code; I meaning]. */
s32 func_0038A408(void) {
    s32 r = 0;
    s32 c;

    if (gCpu->unkCC < 0 && gCpu->unkB4 != NULL && (rand() & 0xFFF) < gCpu->unk382) {
        c = gCpu->unkB4(gCpu);
        switch (c) {
        case -1:
            break;
        case -3:
            if (gCpu->unkB0 > 0) {
                r = Cpu_PlayPicked(0);
            }
            break;
        default:
            if (c == 0x2000 || c == 0x8000) {
                gCpu->unk6 = c;
                func_00380540(0x10);
            } else {
                gCpu->unk6 = c;
            }
        case -2:
            r = 1;
            break;
        }
    }
    return r;
}

/* Decider 7 of Cpu_Think: func_0038A408 with the own character's hook unkB8 [V code]. */
s32 func_0038A500(void) {
    s32 r = 0;
    s32 c;

    if (gCpu->unkCC < 0 && gCpu->unkB8 != NULL && (rand() & 0xFFF) < gCpu->unk382) {
        c = gCpu->unkB8(gCpu);
        switch (c) {
        case -1:
            break;
        case -3:
            if (gCpu->unkB0 > 0) {
                r = Cpu_PlayPicked(0);
            }
            break;
        default:
            if (c == 0x2000 || c == 0x8000) {
                gCpu->unk6 = c;
                func_00380540(0x10);
            } else {
                gCpu->unk6 = c;
            }
        case -2:
            r = 1;
            break;
        }
    }
    return r;
}

s32 func_0038A5F8(void) {
    s32 r = 0x2000;
    u8 t = 0x28; /* fake (decomp-permuter): loads 0x28 before -1 */

    gCpu->unk6C = -1;
    gCpu->unk5C = t;
    gCpu->unk50 = 0;
    gCpu->unk4C = 0;
    if (((gCpu->unk4 & 1) ^ gCpu->unk0) == 0) {
        r = 0;
    }
    return r;
}

s32 func_0038A630(void) {
    s32 r = 0x8000;

    if ((gCpu->unk4 & 1) == gCpu->unk0) {
        r = 0;
    } else if (gCpu->unk4 % 20 == 0) {
        r = 0x2000;
    }
    gCpu->unk6C = -1;
    gCpu->unk5C = 0x28;
    gCpu->unk4C = 0;
    gCpu->unk50 = 0;
    return r;
}

extern const u16 D_00507452[][12];

/* Starts one of the side-step commands of D_00507452 (`n`, or for -1 the side chosen from the opponent's body angle and
 * the two fighters' unk624, randomly when the angle is near 0 or 0x8000) [V code; I meaning]. */
void func_0038A690(s32 n) {
    Fighter *s;
    Fighter *o;
    u16 ang;
    s32 far;
    s16 a;
    s16 b;

    if (n == -1) {
        o = gCpuOpponent;
        s = gCpuSelf;
        ang = o->unk3E;
        a = s->unk624;
        b = o->unk624;
        far = b < a;
        if (ang > 0x100 && ang < 0x8000) {
            n = 0;
        } else if ((u16)(ang + 0x7FFF) < 0x7EFE) {
            n = 1;
        } else {
            n = (rand() >> 4) & 1;
        }
        if (far) {
            n = 1 - n;
        }
    }
    Cpu_SetCommand((u16 *)D_00507452[n]);
    gCpu->unk5C = 0x28;
    gCpu->unk50 = 0;
    gCpu->unk4C = 0;
    gCpu->unk6C = -1;
}

void func_0038A748(void) {
    gCpu->unkBC = func_003840F8;
}

void func_0038A760(void) {
    Cpu_SetCommand((u16 *)D_005074A2);
}

void func_0038A770(void) {
    Cpu_SetCommand((u16 *)D_0050748A);
}

/* NON_MATCHING: 3 lines differ: the original loads the constant 0x21 before the character (li v1 / lh v0), ours
 * the other way round. */
#if 0
s32 func_00384BB0(CpuCand *c);
s32 func_00384C70(CpuCand *c);

/* Inlined in func_003865A0 and func_0038A780: the opponent's move has flags 0x605, or its unk135 / unk139 is set, or
 * unk2A8 is below 10 [V code; I meaning]. */
static inline s32 Cpu_IsOpponentBusy(void) {
    return (gCpu->unk33C.w & 0x605) || gCpuOpponent->unk135 || gCpuOpponent->unk139 || gCpu->unk2A8 < 10;
}

/* Decider 0x14 of Cpu_Think. With unk3E0 set, plays a candidate accepted by func_00384BB0 (or by func_00384B80 for the
 * first two rounds of unk20 when the opponent's move flags 0x605, its unk135 / unk139 and unk2A8 < 10 all fail), with a
 * chance of unk3B2 in 4096. Otherwise character 0x21 with unk278 set and unk274 negative plays a func_00384C70
 * candidate (chance 1/4) and waits 20-39 frames (unk274) [V]. */
s32 func_0038A780(void) {
    CpuState *c = gCpu;
    Fighter *f = gCpuSelf;

    if (c->unk3E0 != 0) {
        if (c->unk20 >= 2 || Cpu_IsOpponentBusy()) {
            if (Cpu_PickCands(func_00384BB0) > 0 && (rand() & 0xFFF) < gCpu->unk3B2) {
                func_003853E8();
                return 1;
            }
        } else {
            if (Cpu_PickCands(func_00384B80) > 0 && (rand() & 0xFFF) < gCpu->unk3B2) {
                func_003853E8();
                return 1;
            }
        }
    } else {
        if (f->unk2A != 0x21 || c->unk274 >= 0) {
            return 0;
        }
        if (c->unk278 != 0 && f->unk12B != 1 && Cpu_PickCands(func_00384C70) > 0 && (rand() & 0xFFF) < 0x400) {
            gCpu->unk278 = 0;
            gCpu->unk274 = Cpu_RandBelow(20) + 20;
            func_003853E8();
            return 1;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A780);
#endif

/* NON_MATCHING: 34 lines: block layout matches; gCpu takes a1 where the original has a2 (and gCpuSelf a2 / a3, f copied to t0), so most lines differ by register */
#if 0
/* Decider 0x15 of Cpu_Think: plays the command planned in unk264 (while unk260 > 0) once the own move is inside its
 * frame window, its condition, Cpu_IsCmdAllowed and Cmd_CheckSituation hold and the own unk40 is below 0x2000 (and
 * unk2A8 reaches the move's attack window), waiting until frame unk268; otherwise the plan is dropped [V]. */
s32 func_0038A928(void) {
    CpuState *c = gCpu;
    CpuCmd *cmd;
    Fighter *self;
    Fighter *f;
    s32 ok;
    struct FighterMove *m;

    if (c->unk260 <= 0) {
        return 0;
    }
    cmd = c->unk264;
    self = gCpuSelf;
    ok = 0;
    if (self->unkC4 >= cmd->unkB && self->unkC4 <= cmd->unkC && Cmd_CheckCondition(self, gCpuOpponent, cmd->unk2) &&
        Cpu_IsCmdAllowed(cmd)) {
        ok = Cmd_CheckSituation((MoveCmd *)cmd, self, gCpuOpponent) != 0;
    }
    c = gCpu;
    if (!ok || (s16)gCpuSelf->unk40 >= 0x2000) {
        goto fail;
    }
    f = gCpuSelf;
    m = D_005F6C00[(s16)gCpuSelf->unk20][c->unk264->unk6];
    if (m->unk2C != 0 && c->unk2A8 < m->unk2C) {
    fail:
        c->unk260 = 0;
        return 0;
    }
    if (f->unkC4 < c->unk268) {
        return 1;
    }
    func_00381278(c->unk264->unk0, D_005F6C00[(s16)gCpuSelf->unk20][c->unk264->unk6]);
    func_00385250(gCpu->unk264, 1);
    gCpu->unk260 = 0;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A928);
#endif

s32 func_00384C58(CpuCand *c);
s32 func_003853E8(void);

s32 func_0038ABE8(void) {
    if (gCpuSelf->unkF7 != 0 && gCpu->unkAC > 0) {
        if (Cpu_PickCands(func_00384C58) > 0) {
            func_003853E8();
            return 1;
        }
        return 0;
    }
    return 0;
}

s32 func_00386428(void);

s32 func_0038AC40(void) {
    s32 v;

    if (gCpu->unk26C == 0) {
        goto fail;
    }
    gCpu->unk270--;
    if (gCpu->unk270 == -1) {
        v = func_00386428();
        gCpu->unk26C = 0;
        if (v == 0) {
            goto fail;
        }
        gCpu->unk6 = v;
    } else if (gCpu->unk270 < 0) {
        gCpu->unk26C = 0;
    }
    return 1;
fail:
    return 0;
}

s32 func_0038ACA8(void) {
    s32 n = 0;
    s32 i;

    if (gCpuOpponent->unkC0->unk2C != 0) {
        for (i = 0; i < 0x14; i++) {
            if (gCpu->unk2D0[i] == gCpuOpponent->unkC0) {
                n++;
            }
        }
    }
    return n;
}

s32 func_0038BF38(void);

/* Decider 0x16 of Cpu_Think: func_0038A408 with the hook func_0038BF38 [V code]. */
s32 func_0038AD00(void) {
    s32 r = 0;
    s32 c;

    if (gCpu->unkCC < 0 && gCpu->unkB4 != NULL && (rand() & 0xFFF) < gCpu->unk382) {
        c = func_0038BF38();
        switch (c) {
        case -1:
            break;
        case -3:
            if (gCpu->unkB0 > 0) {
                r = Cpu_PlayPicked(0);
            }
            break;
        default:
            if (c == 0x2000 || c == 0x8000) {
                gCpu->unk6 = c;
                func_00380540(0x10);
            } else {
                gCpu->unk6 = c;
            }
        case -2:
            r = 1;
            break;
        }
    }
    return r;
}

#if 0
/* NON_MATCHING: 41 lines differ: the original computes all addresses and loads of both members before the two clamps
 * */
extern TeamMember D_00440940[2][2];

/* Clamps the CPU's per-member values unk27C to the members' maximum life [V code; I meaning]. */
void func_0038ADF8(s32 side) {
    Fighter *f = &gFighters[side];
    CpuState *s = &gCpuStates[side];
    s32 *p0 = &s->unk27C[f->unk0];
    s32 *p1 = &s->unk27C[f->unk2];
    s32 max0 = D_00440940[side][f->unk0].unk14;
    s32 max1 = D_00440940[side][f->unk2].unk14;

    if (max0 - *p0 < 0) {
        *p0 = max0;
    }
    if (max1 - *p1 < 0) {
        *p1 = max1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038ADF8);
#endif

s32 func_0038AEA0(Fighter *f) {
    return f->unk10C == 0x3D;
}

s32 func_0038AEB0(s32 n) {
    return n >= gCpu->unk20 ? -1 : 0x2000;
}

s32 func_0038AED0(void) {
    Fighter *s = gCpuSelf;
    Fighter *o = gCpuOpponent;
    s16 a = s->unk624;
    s16 b = o->unk624;

    if ((u16)(s->unk3E + 0x2000) > 0x4000 || (s->unkC0->unk4 & 0x400000) || D_005F5C28 > 0x1B58) {
        return -1;
    }
    return a < b;
}

void func_0038A690(s32 n);

s32 func_0038AF28(void) {
    Fighter *s = gCpuSelf;
    Fighter *o = gCpuOpponent;
    s16 a = s->unk624;
    s16 b = o->unk624;
    u16 ang = o->unk3E;
    s32 far = b < a;
    s32 r;

    if ((s->unkC0->unk4 & 0x400000) || D_005F5C28 > 0x1B58) {
        return -1;
    }
    if (ang > 0x100 && ang < 0x8000) {
        r = 0;
    } else if ((u16)(ang + 0x7FFF) < 0x7EFE) {
        r = 1;
    } else {
        return -1;
    }
    if (far) {
        r = 1 - r;
    }
    func_0038A690(r);
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507A00);

extern const u16 D_00507A00[4];

/* A move handler (D_00507A80) against the opponent's moves 0x1458 / 0x1459 [V code; I meaning]: by the frame of the
 * opponent's move against its window (unk2C / unk2D), the own body angle unk40 and the round unk20 it holds 0x2000 /
 * 0x6000 / 0x8000 (a side step, a random one of D_00507A00), side-steps with func_0038A690 (returns -2) or does
 * nothing (0); -1 for other moves. */
/* NON_MATCHING: about 130 lines differ: the original keeps two identical func_0038AF28 blocks behind an unk40 test
 * that ours merges, and orders the 0x1459 blocks differently; the control flow and constants follow the original
 * (m2c). */
#if 0
s32 func_0038AFC0(void) {
    s16 frame = gCpuOpponent->unkC4;
    s16 id = gCpuOpponent->unk10C;
    struct FighterMove *m = gCpuOpponent->unkC0;
    s32 d;

    if (id != 0x1458) {
        if (id != 0x1459) {
            return -1;
        }
        gCpu->unkD0 = 8;
        if ((u32)(frame - 9) < 5 && gCpu->unk20 < 2 && (rand() & 0x1F) == 0) {
            func_0038A748();
            goto hold;
        }
        if (frame < m->unk2C - 0x1E) {
            if (gCpu->unk20 < 2) {
                goto hold;
            }
            if (gCpu->unk20 < 3) {
                gCpu->unk94 -= 4;
                return 0x2000;
            }
            if ((s16)gCpuSelf->unk40 >= 0x3800) {
                d = func_0038AF28();
                if (d == -1) {
                    return 0x2000;
                }
                goto step;
            }
            d = func_0038AF28();
            if (d == -1) {
                return 0x2000;
            }
            goto step;
        }
        if (frame < m->unk2C - 0x14) {
            if ((s16)gCpuSelf->unk40 >= 0x3800) {
                if (gCpu->unk20 < 3) {
                    return 0x2000;
                }
                d = func_0038AF28();
                if ((u32)d >= 2) {
                    return 0x2000;
                }
                goto step;
            }
            if (gCpu->unk20 < 3) {
                return 0;
            }
            if (gCpu->unk20 < 4) {
                return 0x2000;
            }
            goto rare;
        }
        if (frame < m->unk2D) {
            if ((s16)gCpuSelf->unk40 >= 0x6000) {
                gCpu->unk94 = 8;
                return 0x8000;
            }
            if ((s16)gCpuSelf->unk40 >= 0x3800) {
                if (gCpu->unk20 < 2) {
                    gCpu->unk94 = 8;
                    return 0;
                }
                return 0x2000;
            }
            d = func_0038AED0();
            if (d == -1) {
                return gCpuSelf->unk42 != 0 ? 0 : 0x2000;
            }
            goto step;
        }
        if (frame < m->unk2D + 0x28) {
            if (gCpu->unk20 >= 2) {
                goto rare;
            }
        } else if (gCpu->unk20 >= 3) {
            goto rare;
        }
    hold:
        gCpu->unk94 = -1;
        return 0x2000;
    rare:
        return (rand() & 7) ? 0 : 0x2000;
    }
    gCpu->unkD0 = 8;
    if (frame >= m->unk2C - 8) {
        if (frame >= m->unk2D) {
            return -1;
        }
        if (gCpuSelf->unk42 == 0) {
            return D_005F5C28 > 0x1B58 ? 0 : 0x6000;
        }
        if (gCpu->unk20 < 2) {
            return D_00507A00[rand() & 3];
        }
        if (gCpu->unk20 < 3) {
            return 0x6000;
        }
        goto rare6;
    }
    d = func_0038AED0();
    if (gCpu->unk20 <= 0) {
        gCpu->unk94 = -1;
        return 0x6000;
    }
    if (gCpu->unk20 < 3) {
        return 0x6000;
    }
    if ((rand() & 7) == 0 && gCpuSelf->unk42 == 0 && (u32)d < 2) {
    step:
        func_0038A690(d);
        return -2;
    }
rare6:
    return (rand() & 7) ? 0 : 0x6000;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AFC0);
#endif

/* A move handler (D_00507A80) against the opponent's moves 0x1A1 / 0x1A8 / 0x5A9 / 0x5C7 / 0x5C9 / 0x5CA-0x5CC /
 * 0x623-0x625 [V code; I meaning]: by the opponent's body angle unk40, the distance unk2A8, the rounds unk20 / unk24
 * and the opponent's move window it waits (unk94), holds 0x2000, steps aside (func_0038A748 / func_0038A760 /
 * func_0038A690, -2) or plays func_0038AEB0's answer; -1 when it does nothing. */
/* NON_MATCHING: a first draft from m2c's control flow; most of the blocks are laid out differently (the original's
 * decision tree tests id < 0x5CD, < 0x5CA, == 0x5A9 ... and shares the -1 exits). */
#if 0
s32 func_0038B368(void) {
    s16 id = gCpuOpponent->unk10C;
    s16 ang = gCpuOpponent->unk40;
    s32 dist = gCpu->unk2A8;
    s32 r = -1;
    s32 k;
    struct FighterMove *m;
    s16 frame;

    switch (id) {
    case 0x1A8:
        if (gCpu->unk24 < 3) {
            gCpu->unk94 = -1;
        }
        break;
    case 0x1A1:
    case 0x5A9:
        m = gCpuOpponent->unkC0;
        if (m->unk2C != 0 && gCpuOpponent->unkC4 > m->unk2D) {
            r = 0;
            gCpu->unk94 = -1;
        } else if (ang > 0x1000) {
            r = gCpu->unk24 > 1 ? 0x2000 : -1;
        } else if (gCpu->unk24 >= 3 && D_005F5C28 < 0xA8D) {
            func_0038A760();
            r = -2;
        } else if (gCpu->unk24 < 2 && dist >= 0xB) {
            r = -1;
        } else if (gCpu->unk24 >= 3 || dist < 0xB) {
            r = 0;
        } else {
            func_0038A690(rand() & 1);
            r = -2;
        }
        break;
    case 0x5C7:
        if (gCpu->unk24 < 3) {
            gCpu->unkD0 = 4;
            func_0038A748();
            gCpu->unk94 = -1;
            r = -1;
        } else {
            r = 0x2000;
        }
        break;
    case 0x5C9:
    case 0x623:
    case 0x624:
    case 0x625:
        if (ang < 0x4000) {
            if (gCpu->unk20 < 2) {
                r = 0;
                gCpu->unk94 = 1;
            } else if (gCpu->unk20 >= 3) {
                r = 0x2000;
            } else {
                func_0038A748();
                gCpu->unk94 = -1;
                r = -1;
            }
        } else {
            if (gCpu->unk20 < 2) {
                gCpu->unk94 = -1;
            }
            r = -1;
        }
        break;
    case 0x5CA:
    case 0x5CB:
    case 0x5CC:
        if (gCpu->unk20 <= 0) {
            m = gCpuOpponent->unkC0;
            frame = gCpuOpponent->unkC4;
            r = 0;
            if (frame < m->unk2C || frame > m->unk2D) {
                func_0038A748();
                r = -2;
                gCpu->unkD0 = 4;
                gCpu->unk94 = -1;
            }
        } else if ((u16)(gCpuOpponent->unk40 - 0x2000) > 0x4000) {
            if (gCpu->unk20 >= 3) {
                gCpu->unkD0 = 0xA;
                k = func_0038AEB0(2);
                if (k >= 0) {
                    return k;
                }
                r = -1;
            } else {
                r = 0;
                if ((rand() & 7) == 0 && !(gCpu->unk330.w & 0x400001)) {
                    func_0038A690(-1);
                    r = -2;
                }
            }
        } else {
            gCpu->unkD0 = 0x14;
            if (gCpu->unk20 >= 2) {
                k = func_0038AEB0(1);
                if (k >= 0) {
                    return k;
                }
            } else {
                func_0038A748();
                gCpu->unk94 = -1;
            }
            r = -1;
        }
        break;
    case 0x5EE:
        break;
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038B368);
#endif

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038B690(void) {
    return -1;
}

s32 func_0038B698(void) {
    switch (gCpuOpponent->unk10C) {
    case 0x39C:
        break;
    case 0x39D:
        if (gCpu->unk24 < 3) {
            if (gCpuOpponent->unkC4 >= 0x10) {
                return 0x8000;
            }
            gCpu->unk94 = -1;
        }
        break;
    }
    return -1;
}

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

extern u16 D_00507618[];

/* Move handler: against move 0x1407 at frames 0x18-0x19 answers with the command D_00507618 (chance 3000 in 4096),
 * else holds 0xC000 (chance 4000 in 4096) while the opponent's unk40 is below 0x5000 [V]. */
s32 func_0038B718(void) {
    Fighter *o = gCpuOpponent;
    s32 r = D_005F5C28;

    if (o->unk10C == 0x1407) {
        if (gCpu->unk334 == 0 && gCpuSelf->unk135 == 0 && (s16)o->unk40 < 0x5000 && o->unkC4 < 0x1A &&
            o->unkC4 >= 0x18 && r < 0xBB8) {
            Cpu_SetCommand(D_00507618);
            return -2;
        }
        if (r < 0xFA0 && (s16)o->unk40 < 0x5000) {
            gCpu->unk6 = 0xC000;
            return -2;
        }
    }
    return -1;
}

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

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507A08);

extern const u16 D_00507A08[4];

/* func_0038AFC0 for the opponent's moves 0x1A6 / 0xBA9 (D_00507A08; holds 0x2000 where that one holds 0x6000 early
 * in 0x1A6) [V code; I meaning]. */
/* NON_MATCHING: about 125 lines differ, as func_0038AFC0 (the same code for other moves): the two identical
 * func_0038AF28 blocks are merged in ours and the blocks are ordered differently. */
#if 0
s32 func_0038B820(void) {
    s16 frame = gCpuOpponent->unkC4;
    s16 id = gCpuOpponent->unk10C;
    struct FighterMove *m = gCpuOpponent->unkC0;
    s32 d;

    if (id != 0x1A6) {
        if (id != 0xBA9) {
            return -1;
        }
        gCpu->unkD0 = 8;
        if ((u32)(frame - 9) < 5 && gCpu->unk20 < 2 && (rand() & 0x1F) == 0) {
            func_0038A748();
            goto hold;
        }
        if (frame < m->unk2C - 0x1E) {
            if (gCpu->unk20 < 2) {
                goto hold;
            }
            if (gCpu->unk20 < 3) {
                gCpu->unk94 -= 4;
                return 0x2000;
            }
            if ((s16)gCpuSelf->unk40 >= 0x3800) {
                d = func_0038AF28();
                if (d == -1) {
                    return 0x2000;
                }
                goto step;
            }
            d = func_0038AF28();
            if (d == -1) {
                return 0x2000;
            }
            goto step;
        }
        if (frame < m->unk2C - 0x14) {
            if ((s16)gCpuSelf->unk40 >= 0x3800) {
                if (gCpu->unk20 < 3) {
                    return 0x2000;
                }
                d = func_0038AF28();
                if ((u32)d >= 2) {
                    return 0x2000;
                }
                goto step;
            }
            if (gCpu->unk20 < 3) {
                return 0;
            }
            if (gCpu->unk20 < 4) {
                return 0x2000;
            }
            goto rare;
        }
        if (frame < m->unk2D) {
            if ((s16)gCpuSelf->unk40 >= 0x6000) {
                gCpu->unk94 = 8;
                return 0x8000;
            }
            if ((s16)gCpuSelf->unk40 >= 0x3800) {
                if (gCpu->unk20 < 2) {
                    gCpu->unk94 = 8;
                    return 0;
                }
                return 0x2000;
            }
            d = func_0038AED0();
            if (d == -1) {
                return gCpuSelf->unk42 != 0 ? 0 : 0x2000;
            }
            goto step;
        }
        if (frame < m->unk2D + 0x28) {
            if (gCpu->unk20 >= 2) {
                goto rare;
            }
        } else if (gCpu->unk20 >= 3) {
            goto rare;
        }
    hold:
        gCpu->unk94 = -1;
        return 0x2000;
    rare:
        return (rand() & 7) ? 0 : 0x2000;
    }
    gCpu->unkD0 = 8;
    if (frame >= m->unk2C - 8) {
        if (frame >= m->unk2D) {
            return -1;
        }
        if (gCpuSelf->unk42 == 0) {
            return D_005F5C28 > 0x1B58 ? 0 : 0x6000;
        }
        if (gCpu->unk20 < 2) {
            return D_00507A08[rand() & 3];
        }
        if (gCpu->unk20 < 3) {
            return 0x6000;
        }
        goto rare;
    }
    d = func_0038AED0();
    if (gCpu->unk20 <= 0) {
        gCpu->unk94 = -1;
        return 0x2000;
    }
    if (gCpu->unk20 < 3) {
        return 0x2000;
    }
    if ((rand() & 7) == 0 && gCpuSelf->unk42 == 0 && (u32)d < 2) {
    step:
        func_0038A690(d);
        return -2;
    }
    return (rand() & 7) ? 0 : 0x2000;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038B820);
#endif

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


extern const u16 D_00507A12[][7];
extern const u16 D_00507A32[][7];
extern const u16 D_00507A52[][7];
s32 func_00385DF8(s16 id);
s32 func_003838B0(void);

/* Move handler (D_00507A80 key 3) run with a chance of unk35E in 4096 while unk94 is not positive: against moves of
 * flag 2 (one time in 8) one of the three commands D_00507A52 (unk14C = 1), against flag 1 one of D_00507A32, with
 * unk2A4 one of D_00507A12; each needs the CPU's move without attack window, unk2A8 of at least 9 and both fighters'
 * unk42 clear (the opponent's may be 2 for the last) [V code; I meaning]. */
s32 func_0038BC00(void) {
    s32 hit = (rand() & 0xFFF) < gCpu->unk35E;

    if (gCpu->unk94 > 0 || !hit) {
        return -1;
    }
    if ((rand() & 7) == 0 && (gCpu->unk33C.w & 2) && gCpu->unk20 < 3 && gCpuOpponent->unk135 == 0 &&
        gCpuOpponent->unk139 == 0 && gCpuSelf->unkC0->unk2C == 0 && gCpu->unk2A8 >= 9 && gCpuSelf->unk42 == 0 &&
        gCpuOpponent->unk42 == 0 && func_00385DF8(0x3F) != 0) {
        Cpu_SetCommand((u16 *)D_00507A52[(rand() & 0xFFF) % 3]);
        gCpu->unk14C = 1;
        return -2;
    }
    if ((gCpu->unk33C.w & 1) && gCpu->unk20 < 2 && gCpuOpponent->unk139 == 0 && gCpuSelf->unkC0->unk2C == 0 &&
        gCpu->unk2A8 >= 9 && gCpuSelf->unk42 == 0 && gCpuOpponent->unk42 == 0 && func_003838B0() != 0) {
        Cpu_SetCommand((u16 *)D_00507A32[rand() & 1]);
        return -2;
    }
    if (gCpu->unk2A4 == 0 || gCpu->unk20 >= 2) {
        return -1;
    }
    if (gCpuOpponent->unk42 != 0 && gCpuOpponent->unk42 != 2) {
        return -1;
    }
    if (gCpuSelf->unk42 != 0 || gCpuOpponent->unk139 != 0 || gCpuSelf->unkC0->unk2C != 0 || gCpu->unk2A8 < 9 ||
        gCpu->unkC8 != 0) {
        return -1;
    }
    if (func_003838B0() != 0) {
        Cpu_SetCommand((u16 *)D_00507A12[rand() & 1]);
        return -2;
    }
    return -1;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BE70(void) {
    return -1;
}

s32 func_0038BE78(void) {
    s32 r = -1;

    if (gCpuSelf->unk10C == 0xA8B) {
        if ((u16)(gCpuSelf->unkC4 - 0xD) >= 5 || gCpu->unk2A8 >= 10) {
            r = rand() & 0xF;
            if (r != 0) {
                return 0x2000;
            }
            r = -1;
        }
    }
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507A80);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C18);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C28);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C38);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C48);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C58);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C68);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C78);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C88);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507C98);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507CA8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507CB8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507CC8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507CD8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507CE8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507CF8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D08);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D18);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D28);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D38);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D48);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D58);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D68);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D78);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D88);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507D98);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507DA8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507DB8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507DC8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507DD8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507DE8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507DF8);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E08);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E18);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E28);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E38);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E48);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E58);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E68);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E78);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E88);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507E98);

/* The per-character CPU hooks: a key (character) and two handlers (D_00507A80, 33 entries searched). */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ void *unk4;
    /* 0x8 */ void *unk8;
} CpuCharHooks;

extern CpuCharHooks D_00507A80[];

/* The handler of character `key` from D_00507A80: unk4 when `which` is set, else unk8; NULL when not listed [V]. */
void *func_0038BEE0(s32 which, s32 key) {
    void *r = NULL;
    u32 i;

    for (i = 0; i < 33; i++) {
        if (key == D_00507A80[i].unk0) {
            if (which == 0) {
                r = D_00507A80[i].unk8;
            } else {
                r = D_00507A80[i].unk4;
            }
            break;
        }
    }
    return r;
}

/* A move handler with nothing to do: -1 = no command. */
s32 func_0038BF38(void) {
    return -1;
}


s32 func_0038BF40(s32 kind) {
    switch (kind) {
    case 13:
        return 4;
    case 6:
        return 5;
    case 8:
        return 7;
    case 10:
        return 9;
    case 12:
        return 11;
    default:
        return 4;
    }
}
