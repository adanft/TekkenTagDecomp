#ifndef FIGHTER_H
#define FIGHTER_H

#include "types.h"

/* What fighter field 0xC0 points to (move or animation state). */
struct FighterUnkC0 {
    /* 0x00 */ u8 pad0[0x1C];
    /* 0x1C */ u16 *unk1C;
    /* 0x20 */ u8 pad20[8];
    /* 0x28 */ u8 unk28;
};

/* A fighter slot (gFighters, 4 x 0xB8A0 at 0x879640). Only the fields the decompiled code uses are named. */
typedef struct {
    /* 0x0000 */ s16 unk0;
    /* 0x0002 */ u8 pad2[0x1C];
    /* 0x001E */ s16 unk1E;   /* set to 0, 1, 0, 1 at boot [I: side]; the player of its sound effects */
    /* 0x0020 */ u16 unk20;   /* set to the slot index at boot */
    /* 0x0022 */ s16 unk22;
    /* 0x0024 */ u8 pad24[2];
    /* 0x0026 */ s16 unk26;   /* character * 8 + costume */
    /* 0x0028 */ s16 unk28;
    /* 0x002A */ s16 unk2A;   /* character */
    /* 0x002C */ s16 unk2C;   /* index of its voice set (D_00501808) */
    /* 0x002E */ u8 pad2E[0x88];
    /* 0x00B6 */ s16 unkB6;
    /* 0x00B8 */ u8 padB8[8];
    /* 0x00C0 */ struct FighterUnkC0 *unkC0;
    /* 0x00C4 */ s16 unkC4;
    /* 0x00C6 */ u8 padC6[4];
    /* 0x00CA */ s16 unkCA;
    /* 0x00CC */ u8 padCC[0x10];
    /* 0x00DC */ s16 unkDC;   /* voice sequence playing, -1 none */
    /* 0x00DE */ s16 unkDE;
    /* 0x00E0 */ u8 padE0[0x47];
    /* 0x0127 */ u8 unk127;   /* drawn when non-zero */
    /* 0x0128 */ u8 pad128[0x3E];
    /* 0x0166 */ u16 unk166;  /* step of the voice sequence */
    /* 0x0168 */ s16 unk168;  /* frames since the voice sequence started */
    /* 0x016A */ u8 pad16A[0x2AE];
    /* 0x0418 */ s32 unk418;
    /* 0x041C */ u8 pad41C[0xB8A0 - 0x41C];
} Fighter;

#endif
