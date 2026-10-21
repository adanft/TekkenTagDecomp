/* 0x355D30-0x3564C8 (module theater, docs/game_overview.md section 4; docs/systems/modes.md). */
#include "common.h"

extern s16 gMode;
extern s16 gSubMode;
extern s32 D_005F7050;
extern s8 D_005F7054;
extern s8 D_005F6840;
extern s32 D_01FC48E0[];

void Bgm_Stop(void);
void Mode_MusicPlayer(void);

INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522048);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522058);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522068);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522078);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522088);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522098);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_005220A8);
/* Mode 14: set-up of the music player (stops the BGM, track list 0-10), then mode 15. */
void Mode_EnterMusicPlayer(void) {
    s32 i;

    Bgm_Stop();
    gMode = 15;
    D_005F7050 = -1;
    gSubMode = 0;
    D_005F7054 = 0;
    D_005F6840 = 0;
    for (i = 0; i < 11; i++) {
        D_01FC48E0[i] = i;
    }
}

INCLUDE_RODATA("asm/nonmatchings/theater_b", D_005220B8);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_005220D0);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_005220F8);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522108);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522120);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522130);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_00522148);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_005221A0);
INCLUDE_RODATA("asm/nonmatchings/theater_b", D_005221B0);
#if 0
/* NON_MATCHING: 560 lines: m2c draft (the 0x58-byte bar table copy D_00522148 written as a struct copy) */
#define M2C_UNK u8
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
#define M2C_BITWISE(type, expr) ((type)(s32)(expr))
typedef struct { u8 b[0x58]; } MpBars;
s32 Bgm_Play(s8);
s32 Iop_StreamBusy(s8);
s32 Mode_Change(s32);
s32 Snd_PlaySe(s32, s32, ...);
s32 Sprite_SetBlend(s32, s32);
s32 Text_Printf(void *, s32, s32, s32);
s32 Ui_DrawWindow(s32, s32, s32, s32);
s32 func_003543F0();
s32 func_003B1378();
s32 func_003BB7F0(s32, s32, s32, s32);
s32 func_003BF2F8(void *, void *);
s32 func_003C4868();
s32 func_003DBE98(s32, s32, s32, s32);
s32 Menu_DrawCursorBar(s32, s32, s32, s32);
extern u8 D_0043B7F8[];
extern u8 D_00455D80[];
extern u8 D_005220B8[];
extern u8 D_005220D0[];
extern u8 D_005220F8[];
extern u8 D_00522108[];
extern u8 D_00522120[];
extern u8 D_00522130[];
extern u8 D_00522148[];
extern u8 D_005221A0[];
extern u8 D_005221B0[];
extern s32 D_005F5CDC;
extern s32 D_005F5CE0;
extern u8 D_005F62AC[];
extern s32 D_005F6730;
extern u8 D_005F683F;
extern u16 D_005F704C;

void Mode_MusicPlayer(void) {
    u8 sp[0x200];
    s32 subroutine_arg0, subroutine_arg1, subroutine_arg2, subroutine_arg3, subroutine_arg4, subroutine_arg5;
    M2C_UNK sp10;
    M2C_UNK sp14;
    s32 sp70;
    s32 sp74;
    s32 sp78;
    M2C_UNK *var_a0;
    M2C_UNK *var_a0_2;
    M2C_UNK *var_t4;
    M2C_UNK *var_v1;
    s32 *temp_a0_2;
    s32 *temp_a0_3;
    s32 temp_a2;
    s32 temp_s0;
    s32 temp_v0_7;
    s32 temp_v1;
    s32 temp_v1_3;
    s32 var_a1;
    s32 var_a1_2;
    s32 var_a1_3;
    s32 var_s0;
    s32 var_s1;
    s32 var_s1_2;
    s32 var_s2;
    s32 var_s3;
    s32 var_s4;
    s32 var_s5;
    s32 var_s7;
    s32 var_v0_2;
    s32 var_v0_3;
    s32 var_v0_4;
    s32 var_v0_5;
    s8 temp_a0;
    s8 temp_s1;
    s8 temp_v0;
    s8 temp_v0_2;
    s8 temp_v0_3;
    s8 temp_v0_4;
    s8 temp_v0_5;
    s8 temp_v0_6;
    s8 var_a2_2;
    s8 var_v0;
    u8 temp_v1_2;
    u8 var_a2;

    temp_s1 = D_005F6840;
    temp_s0 = D_005F5CE0;
    sp70 = D_005F5CDC;
    if (func_003543F0() == 0) {
        D_005F6730 = 0xFFF70;
        func_003C4868();
    }
    D_005F6730 = 0xFFF80;
    Sprite_SetBlend(1, 0);
    func_003BF2F8(((u8 *)&D_0043B7F8), ((u8 *)&D_005F62AC));
    Text_Printf(((u8 *)&D_005220B8), 0xC0, 0x28, 0xA);
    var_a2 = (u8) D_005F6840;
    if (temp_s0 & 0x4000) {
        temp_v0 = var_a2 + 1;
        D_005F6840 = temp_v0;
        var_a2 = (u8) temp_v0;
    }
    temp_v0_2 = var_a2 - 1;
    if (temp_s0 & 0x1000) {
        D_005F6840 = temp_v0_2;
        var_a2 = (u8) temp_v0_2;
    }
    var_v0 = (s8) var_a2;
    if (var_a2 & 0x80) {
        var_a2 = 0x15;
        D_005F6840 = 0x15;
        var_v0 = 0x15;
    }
    if (var_v0 >= 0x16) {
        D_005F6840 = 0;
        var_a2 = 0;
    }
    temp_v0_3 = var_a2 + 0xB;
    if (sp70 & 0x2000) {
        D_005F6840 = temp_v0_3;
        var_a2 = (u8) temp_v0_3;
    }
    temp_v0_4 = var_a2 - 0xB;
    if (sp70 & 0x8000) {
        D_005F6840 = temp_v0_4;
        var_a2 = (u8) temp_v0_4;
    }
    if ((s8) var_a2 < 0) {
        var_a2 = 0;
        D_005F6840 = 0;
        goto block_18;
    }
    var_v0_2 = var_a2 << 0x18;
    if ((s8) var_a2 >= 0x16) {
        var_a2 = 0x15;
        D_005F6840 = 0x15;
block_18:
        var_v0_2 = var_a2 << 0x18;
    }
    if (temp_s1 != (var_v0_2 >> 0x18)) {
        Snd_PlaySe(3, 0, (s8) var_a2);
    }
    var_s7 = 0x50;
    Ui_DrawWindow(0x40, 0x38, 0x200, 0x108);
    var_s3 = 0x4B;
    Ui_DrawWindow(0x40, 0x15E, 0x200, 0x28);
    if (D_005F6840 == 0x15) {
        Text_Printf(((u8 *)&D_005220D0), 0x50, 0x174, 0xA);
    } else {
        Text_Printf(((u8 *)&D_005220F8), 0x50, 0x174, 0xB);
        Text_Printf(((u8 *)&D_00522108), 0xC4, 0x174, 0xA);
        Text_Printf(((u8 *)&D_00522120), 0x50, 0x188, 0xB);
        Text_Printf(((u8 *)&D_00522130), 0xC4, 0x188, 0xA);
    }
    var_s1 = 0;
    sp78 = sp70 & 0xBC;
    sp74 = sp70 & 0x100;
    temp_v0_5 = D_005F6840;
    do {
        var_a2_2 = temp_v0_5;
        if (var_s1 == temp_v0_5) {
            if (sp70 & 0x840) {
                var_s2 = var_s1 * 8;
                temp_a0 = *(var_s2 + ((u8 *)&D_00455D80));
                if (temp_a0 != -2) {
                    var_s5 = var_s3 + 0x18;
                    Bgm_Play(temp_a0);
                    var_a2_2 = (s8) (u8) D_005F6840;
                    var_s4 = var_s1 + 1;
                    var_a1 = 0;
                    var_v0_3 = 0 * 4;
                    do {
                        temp_v1 = var_s1 + (var_a1 - 0xB);
                        temp_a0_2 = var_v0_3 + ((u8 *)&D_01FC48E0);
                        *temp_a0_2 = temp_v1;
                        if (temp_v1 >= 0x16) {
                            var_v0_4 = temp_v1 - 0x16;
                            goto block_33;
                        }
                        if (temp_v1 < 0) {
                            var_v0_4 = temp_v1 + 0x16;
block_33:
                            *temp_a0_2 = var_v0_4;
                        }
                        var_a1 += 1;
                        var_v0_3 = var_a1 * 4;
                    } while (var_a1 < 0xB);
                    D_005F7054 = 0;
                } else {
                    var_s5 = var_s3 + 0x18;
                    var_s4 = var_s1 + 1;
                }
                D_005F7050 = (s32) *(var_s2 + ((u8 *)&D_00455D80));
            } else {
                var_s2 = var_s1 * 8;
                var_s5 = var_s3 + 0x18;
                var_s4 = var_s1 + 1;
            }
            var_a0 = ((u8 *)&D_00455D80);
            if (var_s1 == var_a2_2) {

            } else {
                goto block_42;
            }
        } else {
            var_s2 = var_s1 * 8;
            var_s5 = var_s3 + 0x18;
            var_s4 = var_s1 + 1;
block_42:
            var_a0 = (M2C_UNK *) *(var_s2 + ((u8 *)&D_00455D80));
        }
        if (Iop_StreamBusy((s8) var_a0) != 0) {
            var_a1_2 = 0;
            var_t4 = ((u8 *)&D_01FC48E0);
            temp_v0_6 = D_005F7054;
            do {
                var_s0 = var_a1_2 + 1;
                if ((temp_v0_6 >= (0xB - var_a1_2)) && (var_s1 == *((var_a1_2 * 4) + var_t4))) {
                    var_s0 = var_a1_2 + 1;
                    var_a1_3 = var_s3 - 0x15;
                    *(MpBars *)sp = *(MpBars *)D_00522148;
                    func_003DBE98(var_s7, var_a1_3, 0xF0, 0x18);
                }
                var_a1_2 = var_s0;
            } while (var_a1_2 < 0xB);
        }
        if (var_s1 == D_005F6840) {
            temp_v0_7 = Text_Printf(((u8 *)&D_005221A0), var_s7, var_s3, 0xA);
            Menu_DrawCursorBar(var_s7, var_s3, ((temp_v0_7 <= 0x8C) ? temp_v0_7 : 0x8C) + 0x50, 0xFFFF0);
        }
        if (D_005F7050 == *(var_s2 + ((u8 *)&D_00455D80))) {
            func_003BB7F0(1, var_s7 - 0x10, var_s3, 5);
        }
        temp_a2 = var_s3;
        var_s3 = var_s5;
        Text_Printf(((u8 *)&D_005221B0), var_s7, temp_a2, 0xA);
        var_s1 = var_s4;
        if (var_s3 >= 0x14B) {
            var_s3 = 0x4B;
            var_s7 += 0x100;
        }
    } while (var_s1 < 0x16);
    temp_v1_2 = D_005F683F + 1;
    D_005F683F = temp_v1_2;
    if ((u32) (D_005F7050 + 2) >= 2U) {
        if ((s8) temp_v1_2 >= 4) {
            var_s1_2 = 0;
            var_v0_5 = 0 * 4;
            do {
                temp_a0_3 = var_v0_5 + ((u8 *)&D_01FC48E0);
                temp_v1_3 = *temp_a0_3 + 1;
                *temp_a0_3 = temp_v1_3;
                if (temp_v1_3 >= 0x16) {
                    *temp_a0_3 = 0;
                }
                var_s1_2 += 1;
                var_v0_5 = var_s1_2 * 4;
            } while (var_s1_2 < 0xB);
            D_005F683F = 0;
            if ((s8) (u8) D_005F7054 < 0xB) {
                D_005F7054 = (u8) D_005F7054 + 1;
            }
        }
    } else {
        D_005F7054 = 0;
    }
    if (sp78 != 0) {
        Bgm_Stop();
        D_005F7050 = -1;
    }
    if ((sp74 != 0) || (D_005F7050 == -2)) {
        Bgm_Stop();
        Snd_PlaySe(5, 0);
        gMode = D_005F704C;
    }
    if (func_003B1378() != 0) {
        Mode_Change(3);
    }
}
#undef M2C_UNK
#undef M2C_FIELD
#undef M2C_BITWISE
#else
INCLUDE_ASM("asm/nonmatchings/theater_b", Mode_MusicPlayer);
#endif
