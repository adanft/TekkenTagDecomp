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
    /* 0x010 */ s32 unk10;
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
    /* 0x090 */ s32 unk90;
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
    /* 0x154 */ s32 unk154;
    /* 0x158 */ s32 unk158;
    /* 0x15C */ s32 unk15C;
    /* 0x160 */ s32 unk160;
    /* 0x164 */ s32 unk164;
    /* 0x168 */ s32 unk168;
    /* 0x16C */ s32 unk16C;
    /* 0x170 */ s32 unk170;
    /* 0x174 */ s32 unk174;
    /* 0x178 */ s32 unk178;
    /* 0x17C */ s32 unk17C;
    /* 0x180 */ s32 unk180;
    /* 0x184 */ s32 unk184;
    /* 0x188 */ s32 unk188;
    /* 0x18C */ s32 unk18C;
    /* 0x190 */ s32 unk190;
    /* 0x194 */ s32 unk194;
    /* 0x198 */ s32 unk198;
    /* 0x19C */ s32 unk19C;
    /* 0x1A0 */ s32 unk1A0;
    /* 0x1A4 */ s32 unk1A4;
    /* 0x1A8 */ s32 unk1A8;
    /* 0x1AC */ s32 unk1AC;
    /* 0x1B0 */ s32 unk1B0;
    /* 0x1B4 */ s32 unk1B4;
    /* 0x1B8 */ s32 unk1B8;
    /* 0x1BC */ s32 unk1BC;
    /* 0x1C0 */ s32 unk1C0;
    /* 0x1C4 */ s32 unk1C4;
    /* 0x1C8 */ s32 unk1C8;
    /* 0x1CC */ s32 unk1CC;
    /* 0x1D0 */ s32 unk1D0;
    /* 0x1D4 */ s32 unk1D4;
    /* 0x1D8 */ s32 unk1D8;
    /* 0x1DC */ s32 unk1DC;
    /* 0x1E0 */ s32 unk1E0;
    /* 0x1E4 */ s32 unk1E4;
    /* 0x1E8 */ s32 unk1E8;
    /* 0x1EC */ s32 unk1EC;
    /* 0x1F0 */ s32 unk1F0;
    /* 0x1F4 */ s32 unk1F4;
    /* 0x1F8 */ s32 unk1F8;
    /* 0x1FC */ s32 unk1FC;
    /* 0x200 */ s32 unk200;
    /* 0x204 */ s32 unk204;
    /* 0x208 */ s32 unk208;
    /* 0x20C */ s32 unk20C;
    /* 0x210 */ s32 unk210;
    /* 0x214 */ s32 unk214;
    /* 0x218 */ s32 unk218;
    /* 0x21C */ s32 unk21C;
    /* 0x220 */ s32 unk220;
    /* 0x224 */ s32 unk224;
    /* 0x228 */ s32 unk228;
    /* 0x22C */ s32 unk22C;
    /* 0x230 */ s32 unk230;
    /* 0x234 */ s32 unk234;
    /* 0x238 */ s32 unk238;
    /* 0x23C */ s32 unk23C;
    /* 0x240 */ s32 unk240;
    /* 0x244 */ s32 unk244;
    /* 0x248 */ s32 unk248;
    /* 0x24C */ s32 unk24C;
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

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_005064E0);

/* NON_MATCHING: the result lives in a1 instead of v1, as func_0037DC38. */
#if 0
s32 func_0037BC00(s32 kind) {
    s32 r;

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
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BC00);
#endif

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

/* NON_MATCHING: 2 lines differ: the screen position is copied with lh in the original, lhu in ours (the halves
 * as ints there). */
#if 0
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
            m->unk4 = xy[0];
            m->unk6 = xy[1];
            n++;
            if (n >= 4) {
                n = 0;
            }
            p->unk38 = n;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BC98);
#endif

/* NON_MATCHING: 13 lines differ: `n + 1` is computed into v0 before the loop and `n++` is scheduled inside the
 * body, where the original keeps n in s1 and increments it in the delay slot of the loop branch. */
#if 0
void *memset(void *p, s32 c, s32 n);
void func_0035C400(s32 x, s32 y, s32 z);

void func_003D0548(s16 x, s16 y, s16 kind, s32 alpha);

/* Draws the four hit markers of the practice display, oldest first: each projected to the screen and faded out over
 * its last 32 frames (alpha 0x80 down to 4) [V code; I meaning]. */
void func_0037BDD8(Unk37BC60 *p) {
    s32 i;
    s32 n = p->unk38 + 1;
    Unk37BC60Mark *m;
    s32 alpha;
    s16 v[4];
    s16 xy[2];

    for (i = 0; i < 4; i++) {
        if (n >= 4) {
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
        n++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037BDD8);
#endif

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037C1B8);

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

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00506BF0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037CDA0);

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
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00506D20);

/* NON_MATCHING: the result lives in a1 instead of v1 (the original reuses v1 for the jump-table index). */
#if 0
s32 func_0037DC38(s32 state) {
    s32 r;

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
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037DC38);
#endif

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037E1D8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037E468);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037E670);

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
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00506DF0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EB00);

/* Whether the move f->unk10C is in the list of D_008B8010 (zero entries are empty) [V]. */
#if 0
/* NON_MATCHING: 3 lines differ: the original clears r first and copies it into i (move v1, a1) after loading id */
s32 func_0037EE18(Fighter *f) {
    s32 i;
    s32 r;
    Unk8B8010 *d;
    s16 id;

    r = i = 0;
    d = &D_008B8010;
    id = f->unk10C;
    for (; i < 16; i++) {
        if (d->unk14[i] != 0 && d->unk14[i] == id) {
            r = 1;
            break;
        }
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EE18);
#endif

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

/* NON_MATCHING: the original loads the format's %hi in each branch and unk4 before the test; ours hoists the lui
 * above the branch. */
#if 0
s32 Text_Printf(const char *fmt, ...);
extern const char D_00506E28[];
extern const char D_00506E40[];

/* Prints the combo counter of the practice record D_008B8010 ("COMBO n:m") [V]. */
void func_0037EEF8(void) {
    Unk8B8010 *d = &D_008B8010;

    if (d->unk8 != 0) {
        Text_Printf(D_00506E28, 0x190, 0x50, 0xA, 4, 0xA, d->unk4 + 1, d->unk8);
    } else {
        Text_Printf(D_00506E40, 0x190, 0x50, 0xA, 4, 0xA, d->unk4);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EEF8);
#endif


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
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00507270);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037EFB0);

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

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00507290);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FB70);

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005072B0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005072D0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507300);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507330);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507340);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507350);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507370);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507388);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_005073A0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0037FE58);

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
    s32 r = ((rand() + D_005F6C08) & 0x7FFF) * n;

    D_005F6C08 = D_005F6C08 * 5 + 3;
    return r >> 15;
}

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380120);

/* A CPU parameter block (0x80 bytes, CpuState + 0x344): D_00431918[level][kind] [I]. */
typedef struct {
    /* 0x00 */ s32 unk0[0x20];
} CpuParams;

extern CpuParams D_00431918[5][8];

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00380568);

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003814C8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381818);

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00381D30);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00382AD8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003835A0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003838B0);

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

/* NON_MATCHING: 27 lines differ: the original keeps separate return 0 / return 1 blocks at the end where ours
 * folds the last tests into sltu, and its 0x8001 test branches to the return 1 block. */
#if 0
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
        return 1;
    }
    if ((fl & 0x80000) && !func_00384B80(c)) {
        return 1;
    }
    if (func_00384B80(c) && gCpu->unk3E0 != 0) {
        if (m->unk2C == 0) {
            return 0;
        }
        return 1;
    }
    if (m->unk2C == 0) {
        return 0;
    }
    if (c->unk4->unk8 & 0x400000) {
        return 0;
    }
    if (fl & 0x20000) {
        return 0;
    }
    if (func_00383B88(m, gCpu->unk20, 0)) {
        return 1;
    }
    if (fl & 0x10000) {
        return 1;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00383F10);
#endif

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384210);

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

#if 0
/* NON_MATCHING: 13 lines differ: the reloaded move record's register and the order of the exit blocks (as
 * func_003847D8) */
s32 func_003844B0(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;

    if (m->unk2C != 0) {
        if (m->unk4 & 0x20000) {
            goto end;
        }
        if (func_0036C1A0(gCpuSelf, gCpuOpponent, m)->unk2 >= 0) {
            m = c->unk4;
            if (m->unk4 & 0x800000) {
                goto end;
            }
            if (m->unk8 & 0x400000) {
                goto end;
            }
            if (m->unk19 != 0) {
                goto end;
            }
            if (func_00383B88(m, gCpu->unk20, 0) == 0) {
                goto end;
            }
            r = (c->unk8->unk0 & 0x3FE0) == 0;
        }
    }
end:
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003844B0);
#endif

#if 0
/* NON_MATCHING: the result lives in a2 instead of v0, and the original has a load-delay nop. */
s32 func_00384578(CpuCand *c) {
    struct FighterMove *m;
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
    struct FighterMove *m = c->unk4;
    s32 r = 0;

    if ((m->unkC[3] & 0x7F) == 0x38) {
        if (!(m->unk8 & 0x400000)) {
            r = 1;
        }
    }
    return r;
}

#if 0
/* NON_MATCHING: 15 lines differ: as func_003847D8 (the reloaded move record's register and the exits' result
 * register) */

/* A candidate move with damage (reaction entry unk2 > 0) the CPU can reach in time: its window start unk2C below the
 * reaction time unk2A8, not flagged 0x400000 / 0x800000 / 0x20000, and accepted by func_00383B88 [V code; I meaning]. */
s32 func_00384708(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;
    u8 w = m->unk2C;
    u32 fl;

    if (!(m->unk8 & 0x400000) && func_0036C1A0(gCpuSelf, gCpuOpponent, m)->unk2 > 0) {
        m = c->unk4;
        fl = m->unk4;
        if (fl & 0x800000) {
            goto end;
        }
        if (w == 0) {
            goto end;
        }
        if (m->unk19 != 0 && !(fl & 0x80000)) {
            goto end;
        }
        if (w < gCpu->unk2A8) {
            if (fl & 0x20000) {
                goto end;
            }
            r = func_00383B88(m, gCpu->unk20, 0) != 0;
        }
    }
end:
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384708);
#endif

#if 0
/* NON_MATCHING: 14 lines differ: the reloaded move record goes to a2 instead of a0 and the early exits load r from
 * its register in the original */
s32 func_003847D8(CpuCand *c) {
    s32 r = 0;
    struct FighterMove *m = c->unk4;
    u8 w = m->unk2C;
    u32 fl;

    if (!(m->unk8 & 0x400000) && func_0036C1A0(gCpuSelf, gCpuOpponent, m)->unk2 > 0) {
        m = c->unk4;
        fl = m->unk4;
        if (fl & 0x800000) {
            goto end;
        }
        if (w != 0 && (u16)m->unk8 != 0x412 && w < gCpu->unk2A8) {
            if (fl & 0x20000) {
                goto end;
            }
            r = func_00383B88(m, gCpu->unk20, 0) != 0;
        }
    }
end:
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003847D8);
#endif

#if 0
/* NON_MATCHING: 14 lines differ: the original keeps the move record in a0 (over the candidate) and its flags from the
 * delay-slot load; ours keeps it in a1 and reloads the flags */

s32 func_00384898(CpuCand *c) {
    struct FighterMove *m = c->unk4;

    if (m->unk2C != 0) {
        if ((m->unk4 & 0x20001) != 1) {
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
    struct FighterMove *m = c->unk4;
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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00384D78);

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
/* NON_MATCHING: 16 lines differ: register allocation of the random value and the CPU level (a0 / v1), and the sum
 * kept in s1 before the return */
s32 func_00384988(CpuCand *c);
s32 func_003847D8(CpuCand *c);
s32 func_00384708(CpuCand *c);

s32 func_00385178(void) {
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

/* NON_MATCHING: 11 lines differ: the output pointer, the entry count and the next entry take t0 / t2 / t1 where
 * the original has t1 / t0 / t2. */
#if 0
extern MoveCmd D_0047F780[];

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
        cnt = 1;
        if (e->unk0 == 0x8004) {
            cnt = e->unkD;
            c = &D_0047F780[e->unk6];
        } else {
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
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003856C0);
#endif

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003857F8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385A60);

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00385DF8);

#if 0
/* NON_MATCHING: 10 lines differ: the list pointer and the entry count are allocated to swapped registers (a1 / a2) */
extern MoveCmd D_0047F780[];

/* Whether the current move of the CPU's fighter has a command entry of condition kind `kind` (unk3 & 0x7F), looking
 * into the shared lists (0x8004, unkD entries) [V code; I meaning]. */
s32 func_00386388(s32 kind) {
    MoveCmd *c = (MoveCmd *)gCpuSelf->unkC0->unkC;
    MoveCmd *p;
    s32 n;
    s32 i;

    for (; c->unk0 != 0x8000; c++) {
        n = 1;
        if (c->unk0 == 0x8004) {
            n = c->unkD;
            p = &D_0047F780[c->unk6];
        } else {
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
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00386388);
#endif

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


INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003865A0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00387120);

/* NON_MATCHING: 21 lines differ: m and r take s1 / s0 the other way round, and the last test is if-converted
 * (slti / xori) where the original branches. */
#if 0
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
        r = 1;
    } else if (c->unk20 >= 4) {
        r = 1;
    } else if ((c->unk33C.w & 4) && gCpuOpponent->unk135 == 0 && !(m->unk8 & 0x10000) && (u16)m->unk4 != 0x10F &&
               (u16)m->unk4 != 0x51F) {
        r = 1;
    } else if ((s16)gCpuSelf->unk40 >= 0x3801) {
        r = 1;
    }
end:
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00387518);
#endif

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00387600);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_003879A0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00387DA8);

/* NON_MATCHING: 13 lines differ: gCpu and gCpuOpponent are allocated to a0 / v1 instead of a1 / a0 (and the copy
 * of gCpu to a1 instead of a2). */
#if 0
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
        s32 w = o->unkC0->unk2C;

        if (w != 0 && w - 0x1E >= o->unkC4) {
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
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388170);
#endif

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388278);

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388530);

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00507890);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_005078B0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388840);

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

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_005078E0);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_00507910);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00388FA8);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00389488);

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

INCLUDE_RODATA("asm/nonmatchings/fighter_core", D_00507990);
INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_005079B0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_00389D58);

INCLUDE_RODATA("asm/nonmatchings/fighter_core", jtbl_005079E0);

/* Runs the opponent-character hook unkB4 (while unkCC < 0, with chance unk382 in 4096): -1 nothing, -2 done, -3 play a
 * random picked candidate (func_003853E8), 0x2000 / 0x8000 hold that pad bit and call func_00380540(0x10), any other
 * value is the pad bits to hold (unk6); returns whether the CPU acted [V code; I meaning]. */
/* NON_MATCHING: 2 lines differ: `!(x < 0)` comes out as nor / srl (the original slti / xori); `r = !r` after
 * `r = x < 0` gives srl / sltiu, `(x < 0) ^ 1` and if / else fold the same way. */
#if 0
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
                r = !(func_003853E8() < 0);
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
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A408);
#endif

/* NON_MATCHING: 2 lines differ, as func_0038A408 (the same code with the own character's hook unkB8). */
#if 0
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
                r = !(func_003853E8() < 0);
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
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038A500);
#endif

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

/* NON_MATCHING: 72 lines differ, most of them branch offsets: the early return 0 is cross-jumped into the final
 * one in ours, and ours reuses the move record looked up for the window test where the original looks it up
 * again. */
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
    if (ok && (s16)gCpuSelf->unk40 < 0x2000) {
        f = gCpuSelf;
        m = D_005F6C00[(s16)gCpuSelf->unk20][c->unk264->unk6];
        if (m->unk2C == 0 || c->unk2A8 >= m->unk2C) {
            if (f->unkC4 < c->unk268) {
                return 1;
            }
            func_00381278(c->unk264->unk0, D_005F6C00[(s16)gCpuSelf->unk20][c->unk264->unk6]);
            func_00385250(gCpu->unk264, 1);
            gCpu->unk260 = 0;
            return 1;
        }
    }
    c->unk260 = 0;
    return 0;
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

#if 0
/* NON_MATCHING: 9 lines differ: the original places the shared return 0 at the end and the return 1 of the call path
 * behind a branch */
s32 func_00386428(void);

s32 func_0038AC40(void) {
    s32 v;

    if (gCpu->unk26C == 0) {
        return 0;
    }
    gCpu->unk270--;
    if (gCpu->unk270 == -1) {
        v = func_00386428();
        gCpu->unk26C = 0;
        if (v == 0) {
            return 0;
        }
        gCpu->unk6 = v;
    } else if (gCpu->unk270 < 0) {
        gCpu->unk26C = 0;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AC40);
#endif

#if 0
/* NON_MATCHING: the original copies the move pointer to a second register and pads the loop with a nop. */
s32 func_0038ACA8(void) {
    struct FighterMove *m = gCpuOpponent->unkC0;
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

/* NON_MATCHING: 2 lines differ, as func_0038A408 (the same code calling func_0038BF38). */
#if 0
s32 func_0038BF38(void);

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
                r = !(func_003853E8() < 0);
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
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AD00);
#endif

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

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038AFC0);

INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038B368);

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


extern const u16 D_00507A12[][7];
extern const u16 D_00507A32[][7];
extern const u16 D_00507A52[][7];
s32 func_00385DF8(s32 arg0);
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

#if 0
/* NON_MATCHING: 3 lines differ: the rand() result stays in v0 and the unk2A8 test is not a branch-likely in the
 * original */
s32 func_0038BE78(void) {
    if (gCpuSelf->unk10C == 0xA8B) {
        if ((u16)(gCpuSelf->unkC4 - 0xD) >= 5 || gCpu->unk2A8 >= 10) {
            if ((rand() & 0xF) != 0) {
                return 0x2000;
            }
        }
    }
    return -1;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038BE78);
#endif

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

/* NON_MATCHING: 4 lines differ: the hoisted table address is &D_00507A80[0].unk4 in ours, &...unk8 in the
 * original, and the branch-likely tests which the other way round. */
#if 0
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
            if (which != 0) {
                r = D_00507A80[i].unk4;
            } else {
                r = D_00507A80[i].unk8;
            }
            break;
        }
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_core", func_0038BEE0);
#endif

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
