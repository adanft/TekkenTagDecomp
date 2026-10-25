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
/* NON_MATCHING: about 460 lines: rewritten from the asm (T41 b0), register allocation and block order not tried; the
 * original copies the bar colour table D_00522148 to its stack (ldl / ldr) and reads the copy. */
/* A track of the music player list D_00455D80 (22 x 8 bytes). */
typedef struct {
    /* 0x0 */ s8 id; /* the BGM; -2 for RETURN TO THEATER MENU */
    /* 0x1 */ u8 pad1[3];
    /* 0x4 */ s32 name; /* the title (a string address) */
} MpTrack;

/* The colours of one bar of the level meter (D_00522148, 11 x 8 bytes). */
typedef struct {
    /* 0x0 */ u32 c0;
    /* 0x4 */ u32 c1;
} MpBar;

void Bgm_Play(s32 id);
s32 Iop_StreamBusy(void);
void Mode_Change(s32);
void Snd_PlaySe(s32 id, s32 player);
void Sprite_SetBlend(s32 a, s32 b);
s32 Text_Printf(const char *fmt, ...);
void Ui_DrawWindow(s32 x, s32 y, s32 w, s32 h, s32 z, u32 c0, u32 c1);
u8 func_003543F0(void);
s32 func_003B1378(void);
void func_003BB7F0(s32 kind, s32 x, s32 y, s32 id);
void func_003BF2F8(u8 *p, u16 *alpha);
void func_003C4868(void);
void func_003DBE98(u32 x, u32 y, u32 w, u32 h, u32 z, u32 c0, u32 c1, u32 c2, u32 c3);
void Menu_DrawCursorBar(s32 x, s32 y, s32 w, s32 z);
extern u8 D_0043B7F8[];
extern MpTrack D_00455D80[];
extern const char D_005220B8[];
extern const char D_005220D0[];
extern const char D_005220F8[];
extern const char D_00522108[];
extern const char D_00522120[];
extern const char D_00522130[];
extern const MpBar D_00522148[];
extern const char D_005221A0[];
extern const char D_005221B0[];
extern s32 D_005F5CDC;
extern s32 D_005F5CE0;
extern u16 D_005F62AC;
extern s32 D_005F6730;
extern s8 D_005F683F;
extern u16 D_005F704C;

/* Mode 15: the music player. 22 rows in two columns (the last returns to the theater), the playing track marked,
 * and a level meter of 11 bars that scrolls while the stream runs. */
void Mode_MusicPlayer(void) {
    s32 pad;
    s32 rep;
    s32 prev;
    s32 stop;
    s32 back;
    s32 i;
    s32 k;
    s32 v;
    s32 x;
    s32 y;
    s32 col;
    s32 w;
    MpTrack *t;

    pad = D_005F5CDC;
    prev = D_005F6840;
    rep = D_005F5CE0;
    if (func_003543F0() == 0) {
        D_005F6730 = 0xFFF70;
        func_003C4868();
    }
    D_005F6730 = 0xFFF80;
    Sprite_SetBlend(1, 0);
    func_003BF2F8(D_0043B7F8, &D_005F62AC);
    Text_Printf(D_005220B8, 0xC0, 0x28, 0xA, 5, 0xD);
    if (rep & 0x4000) {
        D_005F6840++;
    }
    if (rep & 0x1000) {
        D_005F6840--;
    }
    if (D_005F6840 < 0) {
        D_005F6840 = 0x15;
    }
    if (D_005F6840 >= 0x16) {
        D_005F6840 = 0;
    }
    if (pad & 0x2000) {
        D_005F6840 += 0xB;
    }
    if (pad & 0x8000) {
        D_005F6840 -= 0xB;
    }
    if (D_005F6840 < 0) {
        D_005F6840 = 0;
    } else if (D_005F6840 >= 0x16) {
        D_005F6840 = 0x15;
    }
    if (prev != D_005F6840) {
        Snd_PlaySe(3, 0);
    }
    Ui_DrawWindow(0x40, 0x38, 0x200, 0x108, 0xFFFA, 0xFF40, 0x3030);
    x = 0x50;
    y = 0x4B;
    Ui_DrawWindow(0x40, 0x15E, 0x200, 0x28, 0xFFFE, 0xFF40, 0x3030);
    if (D_005F6840 == 0x15) {
        Text_Printf(D_005220D0, 0x50, 0x174, 0xA, 4, 5);
    } else {
        Text_Printf(D_005220F8, 0x50, 0x174, 0xB, 8);
        Text_Printf(D_00522108, 0xC4, 0x174, 0xA, 4, 5);
        Text_Printf(D_00522120, 0x50, 0x188, 0xB, 8);
        Text_Printf(D_00522130, 0xC4, 0x188, 0xA, 4, 5);
    }
    stop = pad & 0xBC;
    back = pad & 0x100;
    for (i = 0; i < 0x16; i++) {
        t = &D_00455D80[i];
        if (i == D_005F6840 && (pad & 0x840)) {
            if (t->id != -2) {
                Bgm_Play(t->id);
                for (k = 0; k < 11; k++) {
                    v = i + k - 11;
                    D_01FC48E0[k] = v;
                    if (v >= 0x16) {
                        D_01FC48E0[k] = v - 0x16;
                    } else if (v < 0) {
                        D_01FC48E0[k] = v + 0x16;
                    }
                }
                D_005F7054 = 0;
            }
            D_005F7050 = t->id;
        }
        if (i == D_005F6840) {
            col = 0xB;
        } else {
            col = (D_005F7050 == t->id) ? 0x10 : 5;
        }
        if (Iop_StreamBusy() != 0) {
            for (k = 0; k < 11; k++) {
                if (D_005F7054 >= 11 - k && D_01FC48E0[k] == i) {
                    func_003DBE98(x, y - 0x15, 0xF0, 0x18, 0xFFFD, D_00522148[k].c0, D_00522148[k].c1,
                                  D_00522148[k].c0, D_00522148[k].c1);
                }
            }
        }
        if (i == D_005F6840) {
            w = Text_Printf(D_005221A0, x, y, 0xA, 4, col, t->name);
            Menu_DrawCursorBar(x, y, ((w > 0x8C) ? 0x8C : w) + 0x50, 0xFFFF0);
        }
        if (D_005F7050 == t->id) {
            func_003BB7F0(1, x - 0x10, y, 5);
        }
        Text_Printf(D_005221B0, x, y, 0xA, 4, col, t->name);
        y += 0x18;
        if (y >= 0x14B) {
            y = 0x4B;
            x += 0x100;
        }
    }
    D_005F683F++;
    if ((u32)(D_005F7050 + 2) < 2) {
        D_005F7054 = 0;
    } else if (D_005F683F >= 4) {
        for (k = 0; k < 11; k++) {
            if (++D_01FC48E0[k] >= 0x16) {
                D_01FC48E0[k] = 0;
            }
        }
        D_005F683F = 0;
        if (D_005F7054 < 11) {
            D_005F7054++;
        }
    }
    if (stop != 0) {
        Bgm_Stop();
        D_005F7050 = -1;
    }
    if (back != 0 || D_005F7050 == -2) {
        Bgm_Stop();
        Snd_PlaySe(5, 0);
        gMode = D_005F704C;
    }
    if (func_003B1378() != 0) {
        Mode_Change(3);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/theater_b", Mode_MusicPlayer);
#endif
