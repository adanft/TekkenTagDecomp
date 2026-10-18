#ifndef PAD_H
#define PAD_H

#include "types.h"

/* libpad port and slot of one controller connector, gPadSlots[5]: ports 0 and 1 without the multitap, then the
 * multitap slots of port 0. */
typedef struct {
    /* 0x0 */ s16 port;
    /* 0x2 */ s16 slot;
} PadSlot;

/* State of one connector (gPadPorts[5], 0x8BCC80). `phase` drives the libpad set-up sequence of Pad_Update:
 * 0 disconnected, 8 waiting for the pad to report its mode, 0x10-0x12 lock analog mode, 0x20-0x24 pressure mode
 * and actuators, 0x30 ready. */
typedef struct {
    /* 0x00 */ s16 phase;
    /* 0x02 */ s16 wait;      /* frames left before asking the pad for its mode */
    /* 0x04 */ u16 modeId;    /* scePadInfoMode current id (4 digital, 7 analog), 0 when unknown */
    /* 0x06 */ s16 unk6;
    /* 0x08 */ s32 type;      /* byte 1 of the last scePadRead data: high nibble = pad kind, low = data size */
    /* 0x0C */ s32 state;     /* scePadGetState */
    /* 0x10 */ u16 buttons;   /* pressed buttons, 1 = pressed */
    /* 0x12 */ s16 actCount;  /* scePadInfoAct(-1): number of actuators */
    /* 0x14 */ u8 analog[4];  /* right x, right y, left x, left y; 0x80 = centre */
    /* 0x18 */ u8 pressure[12];
} PadPort; /* size 0x24 */

#define PAD_SLOT_COUNT 5

#endif
