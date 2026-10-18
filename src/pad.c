/* 0x396D00-0x397948: pads and multitap (libpad, libmtap). */
#include "common.h"
#include "pad.h"

extern s32 gMultitap;
extern PadSlot gPadSlots[PAD_SLOT_COUNT];
extern u8 gPadDmaBuffers[PAD_SLOT_COUNT][0x100];
extern PadPort gPadPorts[PAD_SLOT_COUNT];
extern f32 gPlayerAxes[4][4];
extern u8 gPlayerAnalog[4][4];
extern u8 gPlayerPressure[4][12];

void exit(s32 status) __attribute__((noreturn));
s32 sceMtapInit(void);
s32 sceMtapPortOpen(s32 port);
s32 sceMtapPortClose(s32 port);
s32 sceMtapGetConnection(s32 port);
s32 scePadInit(s32 mode);
s32 scePadEnd(void);
s32 scePadPortOpen(s32 port, s32 slot, void *addr);
s32 scePadPortClose(s32 port, s32 slot);
s32 scePadRead(s32 port, s32 slot, u8 *rdata);
s32 scePadGetState(s32 port, s32 slot);
s32 scePadGetReqState(s32 port, s32 slot);
s32 scePadInfoMode(s32 port, s32 slot, s32 term, s32 offs);
s32 scePadSetMainMode(s32 port, s32 slot, s32 offs, s32 lock);
s32 scePadInfoAct(s32 port, s32 slot, s32 actno, s32 term);
s32 scePadSetActAlign(s32 port, s32 slot, const u8 *data);
s32 scePadInfoPressMode(s32 port, s32 slot);
s32 scePadEnterPressMode(s32 port, s32 slot);
u16 Pad_GetExtraButtons(s32 port, u8 *rdata);
void func_003BA830(s32 port, s32 ready);
void func_003BA938(s32 oldMultitap, s32 multitap);

typedef struct {
    /* Connector of each player: [0] without the multitap, [1] with it; -1 = none. */
    s16 playerPort[2][4];
    /* Actuator alignment for scePadSetActAlign: small motor on 0, large motor on 1. */
    u8 actAlign[6];
} PadPortMap;

/* Tables of pad.c's .rodata, read from the original executable (INCLUDE_RODATA). */
extern const PadPortMap D_00509D00;
/* Button bit of each of the first 8 pressure bytes. */
extern const u16 D_00509D18[8];
/* Analog axis value of each 16-step band of a stick byte. */
extern const f32 D_00509DF8[16];

INCLUDE_RODATA("asm/nonmatchings/pad", D_00509D00);

/* Opens the multitap on port 0 and the five connectors (exits on failure). */
void Pad_Init(void) {
    s32 ret;
    s32 i;

    gMultitap = 0;
    ret = sceMtapInit();
    if (ret != 1) {
        exit(1);
    }
    if (sceMtapPortOpen(0) != ret) {
        exit(1);
    }
    scePadInit(0);
    gMultitap = sceMtapGetConnection(0) == 1;
    for (i = 0; i < PAD_SLOT_COUNT; i++) {
        if (scePadPortOpen(gPadSlots[i].port, gPadSlots[i].slot, gPadDmaBuffers[i]) != 1) {
            exit(1);
        }
        gPadPorts[i].phase = 0;
        gPadPorts[i].actCount = 0;
    }
}

INCLUDE_RODATA("asm/nonmatchings/pad", D_00509D18);

/* Reads connector `port` into gPadPorts: buttons, analog sticks and pressures. */
s32 Pad_ReadPort(s32 port) {
    u8 rdata[32];
    s32 type;
    u32 bits;
    s32 i;
    s32 j;

    if (scePadRead(gPadSlots[port].port, gPadSlots[port].slot, rdata) == 0) {
        return 0;
    }
    if (rdata[0] != 0) {
        return 0;
    }
    type = rdata[1];
    gPadPorts[port].type = type;
    if (type == 0x53) {
        gPadPorts[port].buttons = ~rdata[2] << 8;
        bits = rdata[3] ^ 0xFFFF;
        for (j = 0; j < 8; j++) {
            if (bits & 1) {
                gPadPorts[port].buttons |= D_00509D18[j];
            }
            bits >>= 1;
        }
    } else {
        gPadPorts[port].buttons = ~((rdata[2] << 8) | rdata[3]);
    }
    if ((gPadPorts[port].type & 0xF0) == 0x70) {
        for (i = 0; i < 4; i++) {
            gPadPorts[port].analog[i] = rdata[4 + i];
        }
    } else {
        if ((gPadPorts[port].type & 0xF0) == 0x20) {
            gPadPorts[port].buttons |= Pad_GetExtraButtons(port, rdata);
        }
        for (i = 0; i < 4; i++) {
            gPadPorts[port].analog[i] = 0x80;
        }
    }
    if (gPadPorts[port].type == 0x79) {
        for (i = 0; i < 12; i++) {
            gPadPorts[port].pressure[i] = rdata[8 + i];
        }
    } else {
        for (i = 0; i < 12; i++) {
            gPadPorts[port].pressure[i] = 0;
        }
    }
    return 0;
}

/* Releases the buttons, centres the sticks and clears the pressures of connector `port`. */
void Pad_ClearPort(s32 port) {
    s32 i;

    gPadPorts[port].buttons = 0;
    for (i = 0; i < 4; i++) {
        gPadPorts[port].analog[i] = 0x80;
    }
    for (i = 0; i < 12; i++) {
        gPadPorts[port].pressure[i] = 0;
    }
}

/* Once per frame: follows the multitap and steps each connector through the libpad set-up sequence, then reads
 * the ready ones. */
#if 0
/* NON_MATCHING: only the registers of two hoisted `lui 0x51` swap (s7/s8: jump table and D_00509D00 high). */
void Pad_Update(void) {
    s32 oldMultitap = gMultitap;
    s32 changed;
    s32 i;
    s32 port;
    s32 slot;
    s32 state;
    s32 ext;
    s32 oldType;

    gMultitap = sceMtapGetConnection(0) == 1;
    func_003BA938(oldMultitap, gMultitap);
    changed = gMultitap != oldMultitap;
    for (i = 0; i < PAD_SLOT_COUNT; i++) {
        port = gPadSlots[i].port;
        slot = gPadSlots[i].slot;
        state = scePadGetState(port, slot);
        gPadPorts[i].state = state;
        if (state == 0 || changed) {
            gPadPorts[i].phase = 0;
        }
        switch (gPadPorts[i].phase) {
        case 0:
            Pad_ClearPort(i);
            gPadPorts[i].type = 0;
            if (state != 0) {
                gPadPorts[i].wait = 2;
                gPadPorts[i].phase = 8;
                func_003BA830(i, 0);
            }
            break;
        case 8:
            if (gPadPorts[i].wait <= 0) {
                gPadPorts[i].wait = 0;
                state = scePadInfoMode(port, slot, 1, 0);
                if (state == 0) {
                    break;
                }
                ext = scePadInfoMode(port, slot, 2, 0);
                if (ext > 0) {
                    state = ext;
                }
                gPadPorts[i].modeId = state;
                switch (state) {
                case 4:
                    gPadPorts[i].phase = 0x10;
                    break;
                case 7:
                    gPadPorts[i].phase = 0x20;
                    break;
                default:
                    gPadPorts[i].phase = 0x30;
                    break;
                }
            } else {
                gPadPorts[i].wait--;
            }
            break;
        case 0x10:
            if (scePadInfoMode(port, slot, 2, 0) == 0) {
                gPadPorts[i].phase = 0x30;
                break;
            }
            gPadPorts[i].phase = 0x11;
            Pad_ClearPort(i);
            break;
        case 0x11:
            if (scePadSetMainMode(port, slot, 1, 3) == 1) {
                gPadPorts[i].phase = 0x12;
            }
            break;
        case 0x12:
            state = scePadGetReqState(port, slot);
            if (state == 1) {
                gPadPorts[i].phase = 0;
            } else if (state == 0) {
                gPadPorts[i].phase = 0;
            }
            break;
        case 0x20:
            if (scePadInfoPressMode(port, slot) == 1) {
                gPadPorts[i].phase = 0x21;
            } else {
                gPadPorts[i].phase = 0x23;
            }
            Pad_ClearPort(i);
            break;
        case 0x21:
            if (scePadEnterPressMode(port, slot) == 1) {
                gPadPorts[i].phase = 0x22;
            }
            break;
        case 0x22:
            state = scePadGetReqState(port, slot);
            if (state == 1) {
                gPadPorts[i].phase = 0;
            } else if (state == 0) {
                gPadPorts[i].phase = 0x23;
            }
            break;
        case 0x23:
            if (state == 6) {
                gPadPorts[i].actCount = scePadInfoAct(port, slot, -1, 0);
                scePadSetActAlign(port, slot, D_00509D00.actAlign);
                gPadPorts[i].phase = 0x24;
            } else if (state == 2) {
                gPadPorts[i].phase = 0x30;
                gPadPorts[i].actCount = 0;
            }
            break;
        case 0x24:
            state = scePadGetReqState(port, slot);
            if (state == 1) {
                gPadPorts[i].phase = 0;
            } else if (state == 0) {
                gPadPorts[i].phase = 0x30;
                func_003BA830(i, 1);
            }
            break;
        case 0x30:
            if (state == 6 || state == 2) {
                oldType = gPadPorts[i].type;
                Pad_ReadPort(i);
                if (oldType != 0 && gPadPorts[i].type != oldType) {
                    gPadPorts[i].phase = 0;
                } else if (gPadPorts[i].modeId & 0xF) {
                    if (gPadPorts[i].modeId != (gPadPorts[i].type & 0xF0) >> 4) {
                        gPadPorts[i].phase = 0;
                    }
                }
            }
            break;
        }
    }
}
#else
RODATA_ALIGN16();
INCLUDE_ASM("asm/nonmatchings/pad", Pad_Update);
#endif

INCLUDE_RODATA("asm/nonmatchings/pad", D_00509DF8);

/* Analog stick byte (0x80 = centre) to -1.0 .. 1.0 in 16 steps, with a dead zone around the centre. */
f32 Pad_AxisToFloat(u8 value) {
    return D_00509DF8[(value & 0xF0) >> 4];
}

/* Copies the input of `player`'s connector to the per-player arrays and returns its buttons; with no connector
 * the outputs are cleared and 0 is returned. */
u16 Pad_GetPlayerInput(s32 player) {
    s16 port;
    s32 i;

    if (gMultitap == 0) {
        port = D_00509D00.playerPort[0][player];
    } else {
        port = D_00509D00.playerPort[1][player];
    }
    if (port < 0) {
        for (i = 0; i < 4; i++) {
            gPlayerAnalog[player][i] = 0x80;
            gPlayerAxes[player][i] = 0.0f;
        }
        for (i = 0; i < 12; i++) {
            gPlayerPressure[player][i] = 0;
        }
        return 0;
    }
    for (i = 0; i < 4; i++) {
        gPlayerAnalog[player][i] = gPadPorts[port].analog[i];
        gPlayerAxes[player][i] = Pad_AxisToFloat(gPadPorts[port].analog[i]);
    }
    for (i = 0; i < 12; i++) {
        gPlayerPressure[player][i] = gPadPorts[port].pressure[i];
    }
    return gPadPorts[port].buttons;
}

/* 1 when the multitap is connected to port 0. */
s32 Pad_IsMultitap(void) {
    return gMultitap;
}

/* Mode id of `player`'s pad when it is ready, -1 otherwise. */
s32 Pad_GetPlayerMode(s32 player) {
    s16 port;

    if (gMultitap == 0) {
        port = D_00509D00.playerPort[0][player];
    } else {
        port = D_00509D00.playerPort[1][player];
    }
    if (port < 0 || gPadPorts[port].phase != 0x30) {
        return -1;
    }
    return (u16)gPadPorts[port].modeId;
}

/* Number of players with a ready pad (2 connectors without the multitap, 4 with it). */
s32 Pad_CountReady(void) {
    s32 player = 0;
    s32 count = gMultitap == 0 ? 2 : 4;
    s32 ready = 0;

    for (; player < count; player++) {
        if (Pad_GetPlayerMode(player) >= 0) {
            ready++;
        }
    }
    return ready;
}

/* Port and slot of connector `i` packed as port << 16 | slot. */
s32 Pad_GetPortSlot(s32 i) {
    return gPadSlots[i].port << 16 | gPadSlots[i].slot;
}

/* Closes the five connectors, libpad and the multitap. */
void Pad_End(void) {
    s32 i;

    for (i = 0; i < PAD_SLOT_COUNT; i++) {
        if (scePadPortClose(gPadSlots[i].port, gPadSlots[i].slot) != 1) {
            exit(1);
        }
    }
    scePadEnd();
    sceMtapPortClose(0);
}
