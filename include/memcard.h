#ifndef MEMCARD_H
#define MEMCARD_H

#include "types.h"

/* The memory card service state (gMc). A request (Mc_Request) sets `request`; Mc_Update runs it over
 * several frames with the libmc calls, `phase` being the libmc call in flight. */
typedef struct {
    /* 0x00 */ s32 port;
    /* 0x04 */ s32 slot;
    /* 0x08 */ s32 phase;     /* libmc call waited for by sceMcSync */
    /* 0x0C */ s32 op;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;     /* file name */
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;     /* buffer */
    /* 0x24 */ s32 unk24;     /* size */
    /* 0x28 */ s32 unk28;     /* open mode */
    /* 0x2C */ s32 fd;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ u8 pad3C[0xC];
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 type;      /* sceMcGetInfo */
    /* 0x50 */ s32 free;
    /* 0x54 */ s32 format;
    /* 0x58 */ s32 unk58;
    /* 0x5C */ s32 unk5C;
    /* 0x60 */ s32 unk60;
    /* 0x64 */ s32 unk64;
    /* 0x68 */ s32 unk68;
    /* 0x6C */ s32 unk6C;
    /* 0x70 */ s32 unk70;
    /* 0x74 */ u32 flags;
    /* 0x78 */ s32 unk78;
    /* 0x7C */ s32 unk7C;
    /* 0x80 */ u8 pad80[0x30];
    /* 0xB0 */ s32 unkB0;
    /* 0xB4 */ s32 unkB4;
    /* 0xB8 */ s32 unkB8;
    /* 0xBC */ u8 padBC[0xC];
    /* 0xC8 */ s32 request;
    /* 0xCC */ s32 unkCC;
    /* 0xD0 */ s32 unkD0;
    /* 0xD4 */ u8 unkD4[16];
    /* 0xE4 */ u8 unkE4[16];
} McState;

#endif
