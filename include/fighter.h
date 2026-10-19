#ifndef FIGHTER_H
#define FIGHTER_H

#include "types.h"

/* A move record (0x34 bytes). Fighter fields 0xB8, 0xC0 (the current move) and 0x188 (the move looked up by id,
 * or NULL: func_00368218) point to one, and so does each CPU candidate (CpuCand.unk4 in src/fighter_core.c). unk2C /
 * unk2D bound a frame window compared with Fighter.unkC4 [I]. */
struct FighterMove {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u32 unk4;                      /* flags */
    /* 0x08 */ u32 unk8;                      /* flags */
    /* 0x0C */ u8 *unkC;                      /* its command list (MoveCmd entries) */
    /* 0x10 */ u16 unk10;                     /* move id */
    /* 0x12 */ s16 unk12;                     /* facing offset applied when the move starts */
    /* 0x14 */ u16 unk14;                     /* index of its first entry in D_005E8978 */
    /* 0x16 */ u8 pad16[0x2];
    /* 0x18 */ u8 unk18;                      /* length in frames */
    /* 0x19 */ u8 unk19;
    /* 0x1A */ u8 pad1A[0x1];
    /* 0x1B */ u8 unk1B;
    /* 0x1C */ u16 *unk1C;
    /* 0x20 */ u8 pad20[0x4];
    /* 0x24 */ u32 unk24;                     /* flags */
    /* 0x28 */ u8 unk28;
    /* 0x29 */ u8 unk29;
    /* 0x2A */ u8 unk2A;
    /* 0x2B */ u8 unk2B;
    /* 0x2C */ u8 unk2C;
    /* 0x2D */ u8 unk2D;
    /* 0x2E */ u8 pad2E[0x2];
    /* 0x30 */ u16 unk30;                     /* hit effect kind (func_00377AE8) [I] */
};

/* One entry of a move's command list (0xE bytes; the lists are in D_0047F780 and a move record points to its own at
 * 0x0C). Move_FindCommand walks a list at the current frame: unk0 is the input to match (func_0036CBE8 against the input
 * history) or a marker (0x8000 end, 0x8004 continue in the shared list unk6, 0x8005 return), unk2 a condition
 * (func_00369008), unkB..unkC the frame window, unk6 the move it starts [V for the walk; I for the meanings]. */
typedef struct {
    /* 0x0 */ u16 unk0;                       /* input, or 0x8000 / 0x8004 / 0x8005; 0x10 and bit 0x4000 tested */
    /* 0x2 */ u8 unk2;                        /* condition id (func_00369008) */
    /* 0x3 */ u8 unk3;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ u16 unk6;                       /* move started */
    /* 0x8 */ s16 unk8;                       /* & 0x7F: kind passed to func_0036BE08 */
    /* 0xA */ u8 unkA;
    /* 0xB */ u8 unkB;                        /* first frame */
    /* 0xC */ u8 unkC;                        /* last frame */
    /* 0xD */ u8 unkD;
} MoveCmd;

typedef struct {
    /* 0x0 */ s32 unk0;                       /* set when unk4 holds a command */
    /* 0x4 */ MoveCmd unk4;
} MoveCmdSlot;

/* Fighter field 0x4C0: the command entry that started the current move and flags derived from it (func_0036BCD0)
 * [I]. */
typedef struct {
    /* 0x00 */ MoveCmd unk0;
    /* 0x0E */ u8 unkE;                       /* set when unk0 was just received */
    /* 0x0F */ u8 unkF;
    /* 0x10 */ u8 unk10;                      /* kind above 0x8005 */
    /* 0x11 */ u8 unk11;                      /* kind 0x10 */
    /* 0x12 */ u8 unk12;                      /* kind has bit 0x4000 */
} MoveCmdState;

/* Fighter field 0x438: the input history (Fighter_ResetInputHistory resets it; Move_FindCommand matches commands against it) [I]. */
typedef struct {
    /* 0x00 */ u32 unk0;                      /* last written entry of unk4 */
    /* 0x04 */ s16 unk4[0x3C];                /* one entry per frame: direction 0-15 | buttons << 4 (func_0036C918) */
    /* 0x7C */ s16 unk7C;                     /* this frame: direction bit (func_0036C988) */
    /* 0x7E */ s16 unk7E;                     /* this frame: buttons pressed */
    /* 0x80 */ s16 unk80;                     /* this frame: buttons held */
    /* 0x82 */ u8 unk82;                      /* cleared when a move starts (Fighter_StepMove) */
} FighterUnk438;

/* What Fighter.unkB12C points to: a move and a start position used instead of the defaults [I]. */
struct FighterUnkB12C {
    /* 0x0 */ u8 pad0[0x4];
    /* 0x4 */ u16 unk4;                       /* move id */
    /* 0x6 */ s16 unk6;                       /* x */
    /* 0x8 */ s16 unk8;                       /* z */
    /* 0xA */ u16 unkA;                       /* angle */
};

/* An attack volume (Fighter + 0x190, up to 4 of 0x18 bytes): a segment from a start point (unk0..unk8) to an end point
 * (unkC..unk14), tested against the other fighter's body spheres by func_00376CF8 [V layout; I meaning]. */
typedef struct {
    /* 0x00 */ s32 unk0;                      /* start x */
    /* 0x04 */ s32 unk4;                      /* start y */
    /* 0x08 */ s32 unk8;                      /* start z */
    /* 0x0C */ s32 unkC;                      /* end x */
    /* 0x10 */ s32 unk10;                     /* end y */
    /* 0x14 */ s32 unk14;                     /* end z */
} AttackVol;

/* A body sphere (Fighter + 0x1F0, 14 of 0x14 bytes): a centre, its radius and the radius squared, set per character
 * from the table D_0042F098 (func_00377F58) [V layout; I meaning]. */
typedef struct {
    /* 0x00 */ s32 unk0;                      /* x */
    /* 0x04 */ s32 unk4;                      /* y */
    /* 0x08 */ s32 unk8;                      /* z */
    /* 0x0C */ s16 unkC;                      /* radius */
    /* 0x0E */ u8 padE[0x2];
    /* 0x10 */ s32 unk10;                     /* radius * radius */
} BodySphere;

/* Fighter + 0x39C: where the last hit landed on this fighter (func_003770E0): the attack volume's end point, its
 * direction and the body sphere it touched [V stores; I meaning]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ u8 padC[0x4];
    /* 0x10 */ s16 unk10;                     /* end - start, x */
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s16 unk14;
    /* 0x16 */ u8 pad16[0x2];
    /* 0x18 */ s16 unk18;                     /* index of the body sphere */
    /* 0x1A */ u8 pad1A[0x2];
} HitContact;

/* What Fighter.unk18C points to: the data of a throw-like pairing (an angle offset and lists of s16 pairs read
 * through the cursor Fighter.unk158, Fighter_StepMove kinds 0x1E and 0x25-0x2D) [V offsets; I meaning]. */
struct FighterUnk18C {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ u8 pad2[0x2];
    /* 0x04 */ s16 *unk4;
    /* 0x08 */ u8 pad8[0x4];
    /* 0x0C */ s16 *unkC;
    /* 0x10 */ s16 unk10;
    /* 0x12 */ u8 pad12[0x2];
    /* 0x14 */ s16 *unk14;
    /* 0x18 */ s16 *unk18;
};

/* A push sphere (Fighter + 0x308, 8 of 0x10 bytes): a centre and a radius; Fighter_Push pushes the two fighters apart
 * where their spheres overlap, Fighter_SetPushRadii sets the radii per move [V layout; I meaning]. */
typedef struct {
    /* 0x00 */ s32 unk0;                      /* x */
    /* 0x04 */ s32 unk4;                      /* y */
    /* 0x08 */ s32 unk8;                      /* z */
    /* 0x0C */ s16 unkC;                      /* radius, 0: off */
    /* 0x0E */ u8 padE[0x2];
} PushSphere;

/* A fighter slot (gFighters, 4 x 0xB8A0 at 0x879640). Only the fields the decompiled code uses are named. */
typedef struct {
    /* 0x0000 */ s16 unk0;
    /* 0x0002 */ s16 unk2;
    /* 0x0004 */ s32 unk4;
    /* 0x0008 */ s32 unk8;
    /* 0x000C */ s32 unkC;
    /* 0x0010 */ u8 pad10[0xA];
    /* 0x001A */ u16 unk1A;
    /* 0x001C */ u8 pad1C[0x2];
    /* 0x001E */ s16 unk1E;                   /* set to 0, 1, 0, 1 at boot [I: side]; the player of its sound effects */
    /* 0x0020 */ u16 unk20;                   /* set to the slot index at boot */
    /* 0x0022 */ s16 unk22;                   /* command list slot (gPracCmdLists) [I] */
    /* 0x0024 */ s16 unk24;                   /* unk0 * 2 + unk1E, set each frame by Battle_UpdateFrame */
    /* 0x0026 */ s16 unk26;                   /* character * 8 + costume */
    /* 0x0028 */ s16 unk28;
    /* 0x002A */ s16 unk2A;                   /* character */
    /* 0x002C */ s16 unk2C;                   /* index of its voice set (D_00501808) */
    /* 0x002E */ s16 unk2E;
    /* 0x0030 */ u8 pad30[0xA];
    /* 0x003A */ s16 unk3A;                   /* facing angle (0x4000 / -0x4000 at a practice reset) [I] */
    /* 0x003C */ s16 unk3C;                   /* facing angle of the body [I] */
    /* 0x003E */ u8 pad3E[0x2];
    /* 0x0040 */ u16 unk40;
    /* 0x0042 */ s16 unk42;
    /* 0x0044 */ s16 unk44;                   /* facing target of a turn (Fighter_StepMove) */
    /* 0x0046 */ s16 unk46;
    /* 0x0048 */ s16 unk48;                   /* angle from the camera angle D_00415808[1] (Battle_UpdateFacing) */
    /* 0x004A */ u8 pad4A[0x6C];
    /* 0x00B6 */ s16 unkB6;
    /* 0x00B8 */ struct FighterMove *unkB8;
    /* 0x00BC */ s16 unkBC;
    /* 0x00BE */ u8 padBE[0x2];
    /* 0x00C0 */ struct FighterMove *unkC0;
    /* 0x00C4 */ s16 unkC4;                   /* frame of the current move [I] */
    /* 0x00C6 */ s16 unkC6;                   /* unkC4 of the previous frame */
    /* 0x00C8 */ s16 unkC8;                   /* frames since the move started (Fighter_StepMove) */
    /* 0x00CA */ s16 unkCA;
    /* 0x00CC */ s32 unkCC;                   /* flags */
    /* 0x00D0 */ u16 unkD0;
    /* 0x00D2 */ u16 unkD2;
    /* 0x00D4 */ u8 unkD4;                    /* move flags unk8 >> 8 */
    /* 0x00D5 */ u8 unkD5;                    /* move flags (unk4 >> 11) & 0x1F */
    /* 0x00D6 */ s16 unkD6;
    /* 0x00D8 */ s16 unkD8;
    /* 0x00DA */ s16 unkDA;
    /* 0x00DC */ s16 unkDC;                   /* voice sequence playing, -1 none */
    /* 0x00DE */ s16 unkDE;
    /* 0x00E0 */ s16 unkE0;
    /* 0x00E2 */ s16 unkE2;
    /* 0x00E4 */ s16 unkE4;                   /* frames the move is held (hit freeze) [I] */
    /* 0x00E6 */ u8 padE6[0x2];
    /* 0x00E8 */ s32 unkE8;
    /* 0x00EC */ s32 unkEC;
    /* 0x00F0 */ u8 unkF0;
    /* 0x00F1 */ s8 unkF1;
    /* 0x00F2 */ u8 unkF2;
    /* 0x00F3 */ u8 unkF3;
    /* 0x00F4 */ s8 unkF4;
    /* 0x00F5 */ s8 unkF5;
    /* 0x00F6 */ u8 unkF6;
    /* 0x00F7 */ u8 unkF7;
    /* 0x00F8 */ u8 unkF8;
    /* 0x00F9 */ u8 unkF9;
    /* 0x00FA */ s16 unkFA;
    /* 0x00FC */ s16 unkFC;
    /* 0x00FE */ s16 unkFE;
    /* 0x0100 */ s16 unk100;
    /* 0x0102 */ u8 pad102[0xA];
    /* 0x010C */ s16 unk10C;                  /* current move id [I] */
    /* 0x010E */ s16 unk10E;                  /* kind of the current move (unk17A when it started) */
    /* 0x0110 */ s32 unk110;                  /* the other fighter's x when the move started */
    /* 0x0114 */ s32 unk114;                  /* the other fighter's z when the move started */
    /* 0x0118 */ u8 unk118;
    /* 0x0119 */ u8 pad119[0x1];
    /* 0x011A */ u8 unk11A;
    /* 0x011B */ s8 unk11B;
    /* 0x011C */ u8 pad11C[0x1];
    /* 0x011D */ u8 unk11D;
    /* 0x011E */ u8 unk11E;                   /* state code set per move kind (Fighter_StepMove) */
    /* 0x011F */ u8 unk11F;
    /* 0x0120 */ u8 unk120;
    /* 0x0121 */ u8 pad121[0x1];
    /* 0x0122 */ s16 unk122;
    /* 0x0124 */ s16 unk124;
    /* 0x0126 */ u8 unk126;
    /* 0x0127 */ u8 unk127;                   /* drawn when non-zero */
    /* 0x0128 */ u8 pad128[0x1];
    /* 0x0129 */ u8 unk129;
    /* 0x012A */ u8 unk12A;                   /* juggle: red life drops by 1.4x (Fighter_ApplyDamage) */
    /* 0x012B */ u8 pad12B[0x1];
    /* 0x012C */ u8 unk12C;
    /* 0x012D */ u8 unk12D;
    /* 0x012E */ u8 unk12E;
    /* 0x012F */ u8 unk12F;
    /* 0x0130 */ u8 unk130;
    /* 0x0131 */ u8 unk131;
    /* 0x0132 */ u8 unk132;
    /* 0x0133 */ u8 pad133[0x1];
    /* 0x0134 */ u8 unk134;
    /* 0x0135 */ u8 unk135;
    /* 0x0136 */ u8 unk136;
    /* 0x0137 */ u8 unk137;
    /* 0x0138 */ u8 unk138;
    /* 0x0139 */ u8 pad139[0x1];
    /* 0x013A */ u8 unk13A;
    /* 0x013B */ u8 unk13B;                   /* counter hit [I] */
    /* 0x013C */ u8 unk13C;                   /* number of attack volumes at unk190 */
    /* 0x013D */ u8 unk13D;                   /* 1: the other's damage, 2: own, 3: heal (Fighter_ApplyDamage) */
    /* 0x013E */ u8 pad13E[0x2];
    /* 0x0140 */ s8 unk140;
    /* 0x0141 */ u8 pad141[0x1];
    /* 0x0142 */ u16 unk142;                  /* damage taken this frame (Fighter_ApplyDamage) */
    /* 0x0144 */ u16 unk144;
    /* 0x0146 */ s16 unk146;
    /* 0x0148 */ s16 unk148;
    /* 0x014A */ s16 unk14A;                  /* damage that replaces unkCA when non-zero */
    /* 0x014C */ u16 unk14C;                  /* hit freeze of the next move (the attacker move's byte 0x2E) */
    /* 0x014E */ s16 unk14E;
    /* 0x0150 */ s16 unk150;
    /* 0x0152 */ s16 unk152;
    /* 0x0154 */ s16 unk154;
    /* 0x0156 */ s16 unk156;
    /* 0x0158 */ s16 *unk158;                 /* cursor in a list of struct FighterUnk18C */
    /* 0x015C */ s16 unk15C;                  /* frames of a turn spread over the move */
    /* 0x015E */ s16 unk15E;                  /* turn per frame */
    /* 0x0160 */ s16 unk160;
    /* 0x0162 */ s16 unk162;
    /* 0x0164 */ s16 unk164;
    /* 0x0166 */ u16 unk166;                  /* step of the voice sequence */
    /* 0x0168 */ s16 unk168;                  /* frames since the voice sequence started */
    /* 0x016A */ s16 unk16A;                  /* airborne / juggled [I] */
    /* 0x016C */ u16 unk16C;
    /* 0x016E */ s16 unk16E;
    /* 0x0170 */ s16 unk170;
    /* 0x0172 */ u8 pad172[0x2];
    /* 0x0174 */ s16 unk174;
    /* 0x0176 */ s16 unk176;                  /* previous move id */
    /* 0x0178 */ s16 unk178;
    /* 0x017A */ s16 unk17A;
    /* 0x017C */ s16 unk17C;
    /* 0x017E */ s16 unk17E;
    /* 0x0180 */ s16 unk180;
    /* 0x0182 */ s16 unk182;
    /* 0x0184 */ u16 unk184;
    /* 0x0186 */ u8 pad186[0x2];
    /* 0x0188 */ struct FighterMove *unk188;
    /* 0x018C */ struct FighterUnk18C *unk18C;
    /* 0x0190 */ s32 unk190;
    /* 0x0194 */ s32 unk194;
    /* 0x0198 */ s32 unk198;
    /* 0x019C */ s32 unk19C;
    /* 0x01A0 */ s32 unk1A0;
    /* 0x01A4 */ s32 unk1A4;
    /* 0x01A8 */ u8 pad1A8[0x48];
    /* 0x01F0 */ BodySphere unk1F0[14];       /* body spheres hit by the other fighter's attack volumes */
    /* 0x0308 */ PushSphere unk308[8];      /* push spheres (Fighter_Push) */
    /* 0x0388 */ u8 pad388[0x14];
    /* 0x039C */ HitContact unk39C;           /* where the last hit landed (func_003770E0) */
    /* 0x03B8 */ s32 unk3B8;                  /* unk51C / unk520 / unk524 of the previous frame */
    /* 0x03BC */ s32 unk3BC;
    /* 0x03C0 */ s32 unk3C0;
    /* 0x03C4 */ s32 unk3C4;                  /* movement of the last frame (Fighter_UpdateMotion) */
    /* 0x03C8 */ s32 unk3C8;
    /* 0x03CC */ s32 unk3CC;
    /* 0x03D0 */ s32 unk3D0;                  /* unk19C - unk190 ... (func_00371C08) */
    /* 0x03D4 */ s32 unk3D4;
    /* 0x03D8 */ s32 unk3D8;
    /* 0x03DC */ s32 unk3DC;                  /* the opponent's unk3D0..unk3D8 while its unk132 is set */
    /* 0x03E0 */ s32 unk3E0;
    /* 0x03E4 */ s32 unk3E4;
    /* 0x03E8 */ s32 unk3E8;                  /* added to unk4..unkC and unk51C..unk524 (Battle_ApplyVelocity) */
    /* 0x03EC */ s32 unk3EC;
    /* 0x03F0 */ s32 unk3F0;
    /* 0x03F4 */ s32 unk3F4;                  /* life, 20.12 fixed point (Fighter_ApplyDamage) */
    /* 0x03F8 */ s32 unk3F8;                  /* red (recoverable) life, 20.12 */
    /* 0x03FC */ s32 unk3FC;                  /* maximum life, 20.12 */
    /* 0x0400 */ u8 unk400;
    /* 0x0401 */ u8 pad401[0x1];
    /* 0x0402 */ s8 unk402;
    /* 0x0403 */ u8 pad403[0x9];
    /* 0x040C */ s32 unk40C;                  /* still standing: cleared at 0 life / red life
                                                 * (Fighter_ApplyDamage), read by Battle_IsRoundOver */
    /* 0x0410 */ s32 unk410;
    /* 0x0414 */ s32 unk414;                  /* input is read each frame when non-zero (Fighter_UpdateInput) */
    /* 0x0418 */ s32 unk418;
    /* 0x041C */ s32 unk41C;
    /* 0x0420 */ s32 unk420;                  /* input source, switch in func_0036C680; 3 = practice playback [I] */
    /* 0x0424 */ s32 unk424;                  /* input source to restore (func_0036C448) */
    /* 0x0428 */ s32 unk428;
    /* 0x042C */ s32 unk42C;
    /* 0x0430 */ u16 unk430;                  /* input forced by the practice dummy (pad direction bits) [I] */
    /* 0x0432 */ u16 unk432;                  /* buttons held this frame (func_0036C680) [V] */
    /* 0x0434 */ u16 unk434;                  /* buttons held the previous frame */
    /* 0x0436 */ u16 unk436;                  /* buttons pressed this frame [V] */
    /* 0x0438 */ FighterUnk438 unk438;
    /* 0x04BC */ u8 pad4BC[0x4];
    /* 0x04C0 */ MoveCmdState unk4C0;
    /* 0x04D4 */ MoveCmdSlot unk4D4;       /* a command matched this frame, buffered [I] */
    /* 0x04E8 */ MoveCmdSlot unk4E8;       /* copy of unk4D4 kept for the next frame [I] */
    /* 0x04FC */ u8 pad4FC[0x20];
    /* 0x051C */ s32 unk51C;
    /* 0x0520 */ s32 unk520;
    /* 0x0524 */ s32 unk524;
    /* 0x0528 */ u8 pad528[0x1E0];
    /* 0x0708 */ s32 unk708;
    /* 0x070C */ s32 unk70C;
    /* 0x0710 */ s32 unk710;
    /* 0x0714 */ u8 pad714[0x14DC];
    /* 0x1BF0 */ f32 unk1BF0[4][4];             /* a matrix (func_00378760) */
    /* 0x1C30 */ u8 pad1C30[0x94FC];
    /* 0xB12C */ struct FighterUnkB12C *unkB12C;  /* start position and move of the round (or NULL) [I] */
    /* 0xB130 */ u8 padB130[0x4];
    /* 0xB134 */ s32 unkB134;                 /* set by a tag (Fighter_StepMove) */
    /* 0xB138 */ u8 padB138[0x768];
} Fighter;

/* A team member's record, per side and member (D_00440940, 2 x 2 x 0x30): the character and costume, and the life
 * of the member while it is out (Battle_InitTeamLife sets it at round start, Battle_RecoverLife keeps it and lets the
 * partner recover, Fighter_TagSwap copies it back on a tag) [V offsets; I names]. */
typedef struct {
    /* 0x00 */ s32 unk0;                      /* character */
    /* 0x04 */ s32 unk4;                      /* costume */
    /* 0x08 */ u8 pad8[0x4];
    /* 0x0C */ s32 unkC;                      /* life, 20.12 */
    /* 0x10 */ s32 unk10;                     /* red life */
    /* 0x14 */ s32 unk14;                     /* maximum life */
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ u8 pad20[0x8];
    /* 0x28 */ s32 unk28;                     /* damage statistic: sum of func_003CF278 per hit taken [I] */
    /* 0x2C */ s32 unk2C;                     /* red life at the start of the round (func_0036D5D0) */
} TeamMember;

#endif
