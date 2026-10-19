#ifndef FIGHTER_H
#define FIGHTER_H

#include "types.h"

/* A move record: what fighter field 0xC0 points to (the current move), and the move of each CPU candidate
 * (CpuCand.unk4 in src/fighter_core.c). unk2C / unk2D bound a frame window compared with Fighter.unkC4 [I]. */
struct FighterUnkC0 {
    /* 0x00 */ u8 pad0[0x4];
    /* 0x04 */ s32 unk4;                      /* flags */
    /* 0x08 */ s32 unk8;                      /* flags */
    /* 0x0C */ u8 *unkC;
    /* 0x10 */ u16 unk10;                     /* move id */
    /* 0x12 */ u8 pad12[0x7];
    /* 0x19 */ u8 unk19;
    /* 0x1A */ u8 pad1A[0x2];
    /* 0x1C */ u16 *unk1C;
    /* 0x20 */ u8 pad20[0x4];
    /* 0x24 */ u32 unk24;                     /* flags */
    /* 0x28 */ u8 unk28;
    /* 0x29 */ u8 pad29[0x3];
    /* 0x2C */ u8 unk2C;
    /* 0x2D */ u8 unk2D;
};

/* What fighter field 0x188 points to (move data, or NULL: func_00368218 looks it up by move id) [I]. */
struct FighterMove {
    /* 0x0 */ u8 pad0[4];
    /* 0x4 */ u32 unk4; /* flags */
};

/* A fighter slot (gFighters, 4 x 0xB8A0 at 0x879640). Only the fields the decompiled code uses are named. */
typedef struct {
    /* 0x0000 */ s16 unk0;
    /* 0x0002 */ u8 pad2[0x2];
    /* 0x0004 */ s32 unk4;
    /* 0x0008 */ u8 pad8[0x16];
    /* 0x001E */ s16 unk1E;                   /* set to 0, 1, 0, 1 at boot [I: side]; the player of its sound effects */
    /* 0x0020 */ u16 unk20;                   /* set to the slot index at boot */
    /* 0x0022 */ s16 unk22;                   /* command list slot (D_008A7B90) [I] */
    /* 0x0024 */ u8 pad24[0x2];
    /* 0x0026 */ s16 unk26;                   /* character * 8 + costume */
    /* 0x0028 */ s16 unk28;
    /* 0x002A */ s16 unk2A;                   /* character */
    /* 0x002C */ s16 unk2C;                   /* index of its voice set (D_00501808) */
    /* 0x002E */ u8 pad2E[0xC];
    /* 0x003A */ s16 unk3A;                   /* facing angle (0x4000 / -0x4000 at a practice reset) [I] */
    /* 0x003C */ u8 pad3C[0x6];
    /* 0x0042 */ s16 unk42;
    /* 0x0044 */ u8 pad44[0x72];
    /* 0x00B6 */ s16 unkB6;
    /* 0x00B8 */ u8 padB8[0x8];
    /* 0x00C0 */ struct FighterUnkC0 *unkC0;
    /* 0x00C4 */ s16 unkC4;                   /* frame of the current move [I] */
    /* 0x00C6 */ u8 padC6[0x4];
    /* 0x00CA */ s16 unkCA;
    /* 0x00CC */ u8 padCC[0x4];
    /* 0x00D0 */ u16 unkD0;
    /* 0x00D2 */ u8 padD2[0xA];
    /* 0x00DC */ s16 unkDC;                   /* voice sequence playing, -1 none */
    /* 0x00DE */ s16 unkDE;
    /* 0x00E0 */ s16 unkE0;
    /* 0x00E2 */ u8 padE2[0x16];
    /* 0x00F8 */ u8 unkF8;
    /* 0x00F9 */ u8 padF9[0x13];
    /* 0x010C */ s16 unk10C;                  /* current move id [I] */
    /* 0x010E */ u8 pad10E[0x19];
    /* 0x0127 */ u8 unk127;                   /* drawn when non-zero */
    /* 0x0128 */ u8 pad128[0x7];
    /* 0x012F */ u8 unk12F;
    /* 0x0130 */ u8 unk130;
    /* 0x0131 */ u8 unk131;
    /* 0x0132 */ u8 unk132;
    /* 0x0133 */ u8 pad133[0x1];
    /* 0x0134 */ u8 unk134;
    /* 0x0135 */ u8 pad135[0x1];
    /* 0x0136 */ u8 unk136;
    /* 0x0137 */ u8 unk137;
    /* 0x0138 */ u8 unk138;
    /* 0x0139 */ u8 pad139[0x2D];
    /* 0x0166 */ u16 unk166;                  /* step of the voice sequence */
    /* 0x0168 */ s16 unk168;                  /* frames since the voice sequence started */
    /* 0x016A */ u8 pad16A[0x1E];
    /* 0x0188 */ struct FighterMove *unk188;
    /* 0x018C */ u8 pad18C[0x268];
    /* 0x03F4 */ s32 unk3F4;
    /* 0x03F8 */ u8 pad3F8[0xA];
    /* 0x0402 */ s8 unk402;
    /* 0x0403 */ u8 pad403[0x15];
    /* 0x0418 */ s32 unk418;
    /* 0x041C */ u8 pad41C[0x4];
    /* 0x0420 */ s32 unk420;                  /* input source, switch in func_0036C680; 3 = practice playback [I] */
    /* 0x0424 */ u8 pad424[0xC];
    /* 0x0430 */ u16 unk430;                  /* input forced by the practice dummy (pad direction bits) [I] */
    /* 0x0432 */ u16 unk432;                  /* buttons held this frame (func_0036C680) [V] */
    /* 0x0434 */ u8 pad434[0x2];
    /* 0x0436 */ u16 unk436;                  /* buttons pressed this frame [V] */
    /* 0x0438 */ u8 pad438[0xE4];
    /* 0x051C */ s32 unk51C;
    /* 0x0520 */ u8 pad520[0xB380];
} Fighter;

#endif
