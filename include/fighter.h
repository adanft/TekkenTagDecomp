#ifndef FIGHTER_H
#define FIGHTER_H

#include "types.h"

/* A fighter slot (gFighters, 4 x 0xB8A0 at 0x879640). Only the fields the decompiled code uses are named. */
typedef struct {
    /* 0x0000 */ s16 unk0;
    /* 0x0002 */ u8 pad2[0x1C];
    /* 0x001E */ s16 unk1E;   /* set to 0, 1, 0, 1 at boot [I: side] */
    /* 0x0020 */ u16 unk20;   /* set to the slot index at boot */
    /* 0x0022 */ s16 unk22;
    /* 0x0024 */ u8 pad24[2];
    /* 0x0026 */ s16 unk26;   /* character * 8 + costume */
    /* 0x0028 */ s16 unk28;
    /* 0x002A */ s16 unk2A;   /* character */
    /* 0x002C */ s16 unk2C;
    /* 0x002E */ u8 pad2E[0xF9];
    /* 0x0127 */ u8 unk127;   /* drawn when non-zero */
    /* 0x0128 */ u8 pad128[0xB8A0 - 0x128];
} Fighter;

#endif
