/* 0x3E55C8-0x3E8AD8 (module gallery_jpeg, docs/game_overview.md section 4; docs/systems/modes.md). */
#include "common.h"

extern u32 *D_005F708C;
extern u8 *D_005F7090;
extern s32 D_01FC51A0[3];

void func_003E8F58(s32);
s32 func_003E8B08(void);
void func_003E8D80(u8);
s32 func_003E8B40(void);
s32 func_003E8F20(void);
void Bs_Init(s32, s32);
void Jpeg_WriteHeader(void);
void Jpeg_WriteEoi(void);
s32 Jpeg_CheckEoi(void);

/* Gallery state (D_01FC4A90). */
typedef struct {
    s16 unk0;
    s16 unk2;
    s16 unk4;
    s16 unk6;
    s16 unk8;
    s16 unkA;
} GalState;
extern GalState D_01FC4A90;
extern u16 D_005F62AC;
extern u16 D_005F5CB6;
extern s32 D_005F7094;
/* The codec's blocks: four luma blocks, then Cb and Cr (64 words each). */
extern s32 D_01FC4AA0[4][64];
extern s32 D_01FC4EA0[64];
extern s32 D_01FC4FA0[64];

s32 func_003BFEF8(void);
void func_003BFE78(s32 m);
void func_003CF608(s32 a);
void func_003B8A50(void);
void Gallery_DrawList(s32);
s32 func_003B8838(void);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 b, s32 color);
void Fade_DrawScreen(s32);
void func_003C4868(void);
void Sprite_SetBlend(s32 a, s32 b);
void func_003BF2F8(u8 *p, u16 *alpha);
s32 Text_Printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
void Ui_DrawWindow(u32 x, u32 y, u32 w, u32 h, u32 z, u32 c0, u32 c1);
void Menu_DrawCursorBar(s32, s32, s32, s32);
extern u8 D_0043B7F8[];
extern s16 D_0046B4A0[];
extern const char D_005233F0[];
extern const char D_00523408[];
extern const char D_00523418[];
extern const char D_00523428[];
extern const char D_00523438[];
extern const char D_00523448[];
extern const char D_00523458[];
extern const char D_00523470[];
extern const char D_00523488[];
extern const char D_00523498[];
extern s32 D_005F5CDC;
extern s32 D_005F5CE0;
s32 func_003B1378(void);
void func_003992C8(s32 value);
void Mode_Change(s32 mode);
void Snd_PlaySe(s32 id, s32 player);
s32 func_003E5350(void);
void func_003E52E8(s8 *st);
void func_003DEB80(s32 a);
s32 Text_Width(s32 n);
s32 func_0035E9B8(s32 slot, s32 buf);
u32 func_0035EA90(void);
s32 func_0035EA08(s32 slot);
s32 Jpeg_DecodeBegin(u8 *a, s32 b, s32 c);
s32 Jpeg_DecodeStrip(void);
void func_00399820(u64 *p, s32 qwc, u64 a, s32 b);
u64 *func_00399520(void);
void func_00399650(u64 *p);
void func_003BB890(s32 a, s32 x, s32 y, s32 b);
s32 func_003E5980(s32 x, s32 y, s32 a);
s32 Text_DrawId(s32, s32, s32, s32);
s32 func_003E6DA0(s32, s32);
s32 func_003E7210(s32);
u32 *func_003E6B10(f32, u32 *);
void func_003E7270(void *m);
void Jpeg_DecodeMcu(void);
void Jpeg_YccToRgb(s32, s32);
u8 Jpeg_ClampByte(f32);
void Jpeg_RgbToYcc(s32, s32);
void Jpeg_EncodeMcu(void);
s32 Bs_GetBitsN(s32);
s32 func_003E8EB0(void);
void Jpeg_DecodeBlock(s32);
void Jpeg_Idct(f32 *);
void Jpeg_Fdct(f32 *, s32);
void Jpeg_EncodeBlock(s32);

/* Huffman encoder table: number of symbols, code lengths, codes. */
typedef struct {
    s32 n;
    u8 *size;
    u16 *code;
} HuffEnc;
extern HuffEnc D_0046B550;
extern HuffEnc D_0046B588;
extern HuffEnc D_0046B788;
extern HuffEnc D_0046B988;
extern u8 D_0046B4A8[];
extern u8 D_0046B4E8[];
extern s32 D_01FC50A0[64];
extern const f32 D_005234F0[];
extern const u8 D_00523518[];
f32 Jpeg_Cos(s32, s32);
void func_003E8DB0(s32);
void func_003E8DF0(u8 *, s32);
/* The JPEG comment string "TTT_PS2 Takuro Eika" (D_00523558), copied to the stack. */
typedef struct {
    u8 s[20];
} JpegComment;
extern const JpegComment D_00523558;
extern u8 D_005F6980[5];
extern u8 D_0046B998[];
extern u8 D_0046B9C0[];
extern u8 D_0046B9E8[];
extern u8 D_0046BAA0[];
void func_003E8D18(s32, s32);

INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_005233F0);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523408);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523418);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523428);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523438);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523448);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523458);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523470);
void Gallery_DrawList(s32 full) {
    char buf[64];
    GalState *g = &D_01FC4A90;
    s32 base;
    s8 *st;
    s32 i, x, y, w, col;

    base = func_003B8838();
    if (D_01FC4A90.unk0 == 7 && g->unk6 >= 0) {
        func_003DBCF0(0, 0, 640, 448, 0xFFFF, 0x80);
    }
    if (g->unk8 < 0x80) {
        if (g->unk6 >= 0) {
            col = 0x80;
            if (g->unk0 == 7) {
                col = 0x80 - g->unk4 * 8;
            }
            Fade_DrawScreen(col < 0 ? 0 : col);
        } else {
            func_003C4868();
        }
        Sprite_SetBlend(1, 0);
        func_003BF2F8(D_0043B7F8, &D_005F62AC);
    }
    if (full != 0) {
        Text_Printf(D_005233F0, 0xF8, 0x28, 10, 5, 0x13);
        st = (s8 *)(base + 0x8E860);
        for (i = 0; i < 12; i++) {
            x = i <= 5 ? 100 : 350;
            y = (i % 6) * 32 + 110;
            col = D_0046B4A0[st[i]];
            if (g->unk2 == i) {
                col = st[i] == 2 ? 9 : 11;
            }
            sprintf(buf, D_00523408, i + 1);
            w = Text_Printf(D_00523418, 0xFFF00, x, y, 10, 4, col, buf);
            if (g->unk2 == i) {
                Menu_DrawCursorBar(x - 40, y, w + 80, 0xFF0);
            }
        }
        if (g->unk2 == 12) {
            col = 11;
            Menu_DrawCursorBar(310, 306, 140, 0xFF0);
        } else {
            col = 0x13;
        }
        Text_Printf(D_00523428, 350, 306, 10, 4, col);
        Ui_DrawWindow(100, 350, 440, 40, 0xFFFE, 0xFF40, 0x4040);
        if (g->unk0 != 6) {
            Text_Printf(D_00523438, 106, 372, 11, 8);
            Text_Printf(D_00523448, 106, 394, 10, 3, 5);
            Text_Printf(D_00523458, 196, 372, 10, 4, 5);
            Text_Printf(D_00523470, 196, 392, 10, 4, 5);
        }
    }
    if (g->unk8 > 0) {
        Fade_DrawScreen(g->unk8);
    }
}

void Mode_Gallery(void);

void Gallery_Enter(void) {
    if (func_003BFEF8() != 0) {
        D_01FC4A90.unk0 = 0;
    } else {
        D_01FC4A90.unk0 = 14;
    }
    func_003BFE78(1);
    D_01FC4A90.unk6 = -1;
    D_005F62AC = 0;
    D_01FC4A90.unk2 = 0;
    D_01FC4A90.unk4 = 0;
    D_01FC4A90.unk8 = 0;
    func_003CF608(0);
    Gallery_DrawList(0);
    func_003B8A50();
}

s32 func_003E5980(s32 x, s32 y, s32 a) {
    s32 r;

    Text_DrawId(x, y, 0x2C, a);
    y += 40;
    Text_DrawId(x, y, 0x2D, a);
    y += 20;
    Text_DrawId(x, y, 0x2E, a);
    y += 20;
    Text_DrawId(x, y, 0x2F, a);
    y += 40;
    Text_DrawId(x, y, 0x30, a);
    y += 20;
    Text_DrawId(x, y, 0x31, a);
    y += 40;
    Text_DrawId(x, y, 0x32, a);
    y += 20;
    Text_DrawId(x, y, 0x33, a);
    y += 20;
    Text_DrawId(x, y, 0x34, a);
    y += 20;
    r = Text_DrawId(x, y, 0x35, a);
    return r;
}

INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523488);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523498);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", jtbl_005234B0);
#if 0
/* NON_MATCHING: 829 lines: readable draft of the 16 states (cases 8/9 and 12/13 merged, some Text_Printf / func_003DBCF0 arguments guessed); the C emits its own jump table, so jtbl_005234B0 must go when it is enabled */
void Mode_Gallery(void) {
    char buf[64];
    s32 base;
    s8 *st;
    s32 pad, trg, n, a;
    u32 r;
    u64 *p;

    base = func_003B8838();
    st = (s8 *)(base + 0x8E860);
    if (func_003B1378() != 0 && D_01FC4A90.unk0 != 7 && D_01FC4A90.unk0 != 3 && D_01FC4A90.unk0 != 4 &&
        D_01FC4A90.unk0 != 9 && D_01FC4A90.unk0 != 10) {
        func_003992C8(1);
        Mode_Change(3);
    }
    if (func_003E5350() == 0 && D_01FC4A90.unk0 != 7 && D_01FC4A90.unk0 != 14 && D_01FC4A90.unk0 != 15) {
        D_01FC4A90.unk6 = -1;
        D_01FC4A90.unk0 = 11;
        D_01FC4A90.unk8 = 0;
    }
    switch (D_01FC4A90.unk0) {
    case 0:
        D_01FC4A90.unk6 = -1;
        D_01FC4A90.unk2 = 0;
        Gallery_DrawList(0);
        D_01FC4A90.unk0 = 1;
        func_003992C8(0);
        break;
    case 1:
        func_003992C8(1);
        func_003E52E8(st);
        D_01FC4A90.unk0 = 2;
        D_01FC4A90.unk4 = 0;
        D_01FC4A90.unk8 = 0;
        while (D_01FC4A90.unk2 < 12 && st[D_01FC4A90.unk2] == 0) {
            D_01FC4A90.unk2++;
        }
        break;
    case 2:
        func_003992C8(0);
        D_005F62AC += 4;
        if (D_01FC4A90.unk8 > 0) {
            D_01FC4A90.unk8 -= 16;
        }
        if (D_01FC4A90.unk4 <= 0) {
            Gallery_DrawList(1);
        } else {
            D_01FC4A90.unk4--;
            Gallery_DrawList(0);
            func_003DBCF0(0, 0, 640, 448, 0xFFFE, D_01FC4A90.unk4 * 16);
        }
        trg = D_005F5CE0;
        if (trg == 0x8000) {
            Snd_PlaySe(3, 0);
            if (D_01FC4A90.unk2 < 12) {
                D_01FC4A90.unk2 = (D_01FC4A90.unk2 + 6) % 12;
            }
            while (D_01FC4A90.unk2 >= 0 && st[D_01FC4A90.unk2] == 0) {
                D_01FC4A90.unk2--;
            }
            if (D_01FC4A90.unk2 < 0) {
                D_01FC4A90.unk2 = 12;
            }
        }
        if (trg == 0x2000) {
            Snd_PlaySe(3, 0);
            if (D_01FC4A90.unk2 < 12) {
                D_01FC4A90.unk2 = (D_01FC4A90.unk2 + 6) % 12;
                while (D_01FC4A90.unk2 < 12 && st[D_01FC4A90.unk2] == 0) {
                    D_01FC4A90.unk2++;
                }
            }
        }
        if (trg == 0x1000) {
            Snd_PlaySe(3, 0);
            D_01FC4A90.unk2--;
            while (D_01FC4A90.unk2 >= 0 && st[D_01FC4A90.unk2] == 0) {
                D_01FC4A90.unk2--;
            }
            if (D_01FC4A90.unk2 < 0) {
                D_01FC4A90.unk2 = 12;
            }
        }
        pad = D_005F5CDC;
        if (trg == 0x4000) {
            Snd_PlaySe(3, 0);
            if (++D_01FC4A90.unk2 > 12) {
                D_01FC4A90.unk2 = 0;
            }
            while (D_01FC4A90.unk2 < 12 && st[D_01FC4A90.unk2] == 0) {
                D_01FC4A90.unk2++;
            }
        }
        if (pad & 0xF0) {
            if (D_01FC4A90.unk2 < 12) {
                if (st[D_01FC4A90.unk2] == 1) {
                    Snd_PlaySe(5, 0);
                    D_01FC4A90.unk0 = D_01FC4A90.unk2 != D_01FC4A90.unk6 ? 3 : 6;
                    D_01FC4A90.unk4 = 0;
                }
            } else {
                Snd_PlaySe(5, 0);
                D_01FC4A90.unk0 = 7;
                D_01FC4A90.unk4 = 0;
            }
        }
        if ((pad & 0x100) || (D_01FC4A90.unk2 == 12 && (pad & 0x80C))) {
            Snd_PlaySe(5, 0);
            D_01FC4A90.unk4 = 0;
            D_01FC4A90.unk0 = 7;
        }
        if ((pad & 0x800) && D_01FC4A90.unk2 < 12) {
            Snd_PlaySe(5, 0);
            D_01FC4A90.unkA = 0;
            D_01FC4A90.unk0 = 8;
            D_01FC4A90.unk4 = 0;
        }
        break;
    case 3:
        if (++D_01FC4A90.unk4 < 7) {
            Gallery_DrawList(0);
            func_003DBCF0(0, 0, 640, 448, 0xFFFE, D_01FC4A90.unk4 * 16);
            func_003DEB80(0);
            Text_DrawId(320 - Text_Width(0x52) / 2, 180, 0x52, 0x40404080);
            Text_DrawId(320 - Text_Width(0x53) / 2, 208, 0x53, 0x40404080);
        } else {
            func_003992C8(1);
            if (func_0035E9B8(D_01FC4A90.unk2, base + 0x69060) == 0) {
                D_01FC4A90.unk0 = 4;
            }
        }
        break;
    case 4:
        func_003992C8(1);
        r = func_0035EA90();
        if (r != 0) {
            if (r < 3) {
                if (Jpeg_DecodeBegin((u8 *)(base + 0x60), base + 0x69060, 0x25800) != 0) {
                    st[D_01FC4A90.unk2] = 2;
                    D_01FC4A90.unk0 = 12;
                } else {
                    D_01FC4A90.unk4 = 0;
                    D_01FC4A90.unk0 = 5;
                }
            } else {
                D_01FC4A90.unk0 = 13;
            }
        }
        break;
    case 5:
        func_003992C8(1);
        if (Jpeg_DecodeStrip() != 0) {
            st[D_01FC4A90.unk2] = 2;
            D_01FC4A90.unk0 = 12;
        }
        /* GS image upload of the decoded 640x224 strip at y = unk4 * 224 (packet at base). */
        p = (u64 *)base;
        p[0] = 0x1000000000008004;
        p[1] = 0xE;
        p[2] = (u64)((0x010A2300 << 16) | 0x10A) << 16;
        p[3] = 0x50;
        p[4] = (u64)(D_01FC4A90.unk4 * 224) << 48;
        p[5] = 0x51;
        p[6] = 0xE000000280;
        p[7] = 0x52;
        p[8] = 0;
        p[9] = 0x53;
        p[10] = 0x080000000000E900;
        p[11] = 0;
        func_00399820(p, 0x6906, 0xE000000280, 0x53);
        p = func_00399520();
        *p++ = 0x1000000000008001;
        *p++ = 0xE;
        *p++ = 0;
        *p++ = 0x3F;
        func_00399650(p);
        if (++D_01FC4A90.unk4 >= 2) {
            D_01FC4A90.unk0 = 6;
        }
        break;
    case 6:
        D_01FC4A90.unk8 += 16;
        if (D_01FC4A90.unk8 > 0x80) {
            D_01FC4A90.unk8 = 0x80;
        }
        if (D_01FC4A90.unk6 == D_01FC4A90.unk2) {
            Gallery_DrawList(1);
        } else {
            Fade_DrawScreen(D_01FC4A90.unk8);
        }
        if (D_005F5CDC & 0xF0) {
            Snd_PlaySe(5, 0);
            D_01FC4A90.unk0 = 2;
            D_01FC4A90.unk6 = D_01FC4A90.unk2;
            D_01FC4A90.unk4 = 0;
        }
        break;
    case 7:
        D_005F62AC -= 4;
        Gallery_DrawList(0);
        D_01FC4A90.unk4++;
        if ((s16)D_005F62AC <= 0) {
            func_003992C8(1);
            Mode_Change(3);
        }
        break;
    case 8:
    case 9:
        if (D_01FC4A90.unk0 == 8) {
            Gallery_DrawList(0);
            if (D_01FC4A90.unk4 < 1000) {
                D_01FC4A90.unk4++;
            }
        }
        func_003DBCF0(0, 0, 640, 448, 0xFFFE, 0x60);
        func_003DEB80(0);
        Text_DrawId(320 - Text_Width(0xC2) / 2, 170, 0xC2, 0x40404080);
        Text_DrawId(320 - Text_Width(0xC3) / 2, 190, 0xC3, 0x40404080);
        sprintf(buf, D_00523408, D_01FC4A90.unk2 + 1);
        Text_Printf(D_00523488, 210, 164, 10, 4, 5, buf);
        Text_Printf(D_00523498, 204, 246, 10, 4, 5, 0x3F);
        func_003BB890(D_01FC4A90.unkA != 1, 317, 256, 5);
        if (D_01FC4A90.unk0 == 8) {
            if (D_005F5CDC & 0xA000) {
                Snd_PlaySe(3, 0);
                D_01FC4A90.unkA = D_01FC4A90.unkA == 0;
            } else if (D_005F5CDC & 0xF0) {
                if (D_01FC4A90.unkA == 1) {
                    Snd_PlaySe(5, 0);
                    D_01FC4A90.unk4 = 0;
                    D_01FC4A90.unk0 = 9;
                } else {
                    Snd_PlaySe(0x103E, 0);
                    D_01FC4A90.unk4 = 8;
                    D_01FC4A90.unk0 = 2;
                }
            }
        } else {
            if (D_01FC4A90.unk4 > 0) {
                func_003992C8(1);
            }
            if (D_01FC4A90.unk4 >= 3) {
                if (func_0035EA08(D_01FC4A90.unk2) == 0) {
                    D_01FC4A90.unk0 = 10;
                }
            } else {
                D_01FC4A90.unk4++;
            }
        }
        break;
    case 10:
        func_003992C8(1);
        r = func_0035EA90();
        if (r != 0) {
            if (r < 3) {
                st[D_01FC4A90.unk2] = 0;
                while (D_01FC4A90.unk2 < 12 && st[D_01FC4A90.unk2] == 0) {
                    D_01FC4A90.unk2++;
                }
            }
            D_01FC4A90.unk4 = 8;
            D_01FC4A90.unk0 = 2;
        }
        break;
    case 11:
        func_003992C8(0);
        D_005F62AC += 4;
        for (n = 0; n < 13; n++) {
            st[n] = 0;
        }
        D_01FC4A90.unk2 = 12;
        Gallery_DrawList(1);
        if (func_003E5350() != 0) {
            D_01FC4A90.unk0 = 1;
        }
        if (D_005F5CDC & 0x1FC) {
            Snd_PlaySe(5, 0);
            D_01FC4A90.unk4 = 0;
            D_01FC4A90.unk0 = 7;
        }
        break;
    case 12:
    case 13:
        func_003992C8(0);
        func_003DBCF0(0, 0, 640, 448, 0xFFFE, 0x60);
        func_003DEB80(0);
        if (D_01FC4A90.unk0 == 12) {
            Text_DrawId(320 - Text_Width(0xBF) / 2, 180, 0xBF, 0x40404080);
            sprintf(buf, D_00523408, D_01FC4A90.unk2 + 1);
            Text_Printf(D_00523488, 206, 172, 10, 4, 5, buf);
            Text_DrawId(320 - Text_Width(0xC0) / 2, 180, 0xC0, 0x40404080);
            Text_DrawId(320 - Text_Width(0xC1) / 2, 208, 0xC1, 0x40404080);
        } else {
            Text_DrawId(320 - Text_Width(0x55) / 2, 180, 0x55, 0x40404080);
        }
        if (D_005F5CDC & 0xF0) {
            D_01FC4A90.unk0 = 2;
            D_01FC4A90.unk4 = 8;
            D_01FC4A90.unk6 = -1;
        }
        break;
    case 14:
        func_003992C8(0);
        func_003C4868();
        func_003DEB80(0);
        if (D_01FC4A90.unk4 < 1000) {
            D_01FC4A90.unk4++;
        }
        func_003DBCF0(0, 0, 640, 448, 0xFFFE, 0x60);
        if (D_01FC4A90.unk4 >= 9) {
            func_003E5980(50, 80, 0x58585880);
        }
        if ((D_005F5CDC & 0xF0) && D_01FC4A90.unk4 >= 121) {
            Snd_PlaySe(5, 0);
            D_01FC4A90.unk4 = 0;
            D_01FC4A90.unk0 = 15;
        }
        break;
    case 15:
        a = 0x40 - (D_01FC4A90.unk4++ << 2);
        if (a < 0) {
            a = 0;
        }
        func_003E5980(50, 80, a << 24 | a << 16 | a << 8 | 0x80);
        if (D_01FC4A90.unk4 >= 18) {
            func_003992C8(1);
            Mode_Change(3);
        }
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/gallery_jpeg", Mode_Gallery);
#endif

void func_003E6AA8(u32 *a) {
    D_005F708C = a;
}

void func_003E6AB0(u32 *end, s32 eop, s32 qwc) {
    u32 *p = D_005F708C;
    s32 n = end - p - 1;
    if (eop != 0) {
        *p = 0x6D008000 | ((n >> 1) & 0xFF) << 16 | (qwc & 0x3FF);
    } else {
        *p = 0x6C000000 | ((n >> 2) & 0xFF) << 16 | (qwc & 0x3FF);
    }
}

LIT4_WORD(D_005F58DC, 0x40490FDB);
LIT4_WORD(D_005F58E0, 0x461C4000);
#if 0
/* NON_MATCHING: rough m2c-based draft (8 random particles per packet with rand / cosf), not tuned */
#define M32(p, o) (*(s32 *)((u8 *)(p) + (o)))
f32 cosf(f32);
f32 rand();
u8 *func_003E6B10(f32 arg1, s32 arg0) {
    u8 spbuf[0x400];
    f32 temp_f0;
    f32 temp_f1;
    f32 temp_f20;
    f32 temp_f21;
    f32 temp_f25;
    f32 var_v0;
    s16 var_v0_2;
    s16 var_v1;
    s32 var_s3;
    u8 *var_s0;
    u8 *var_s2;

    var_s2 = arg0 + 4;
    var_s3 = 0;
    var_s0 = var_s2;
    func_003E6AA8((u32 *)(0));
    temp_f25 = 3.14159274f;
    do {
        M32(var_s0, 0x2) = (s16) rand();
        M32(var_s0, 0x0) = (s16) 0;
        temp_f0 = (f32) ((s32) rand() & 0xFFFFFF);
        M32(var_s0, 0x6) = 1;
        M32(var_s0, 0x4) = (s16) 0;
        temp_f21 = (2.0f * ((f32) ((s32) rand() & 0xFFFFFF) * 5.9604645e-8f) * temp_f25) - temp_f25;
        temp_f20 = (f32) ((s32) rand() & 0xFFFFFF) * 5.9604645e-8f * 12.0f;
        temp_f1 = cosf(temp_f21);
        M32(var_s0, 0x8) = (s16) 0;
        M32(var_s0, 0xA) = -0x181;
        if (temp_f21 > 0.0f) {
            0;
            var_v0 = temp_f1;
        } else {
            0;
            var_v0 = 1.0f - (temp_f1 * temp_f1);
        }
        M32(var_s0, 0xC) = (s16) var_v0;
        M32(var_s0, 0xE) = 0;
        M32(var_s0, 0x10) = 0x5DC0;
        M32(var_s0, 0x12) = 0x5DC0;
        if (10000.0f < arg0) {
            var_v0_2 = (s32) rand() & 0x1FF;
            var_v1 = 0x200;
        } else {
            var_v0_2 = (s32) rand() & 0xFF;
            var_v1 = 0x100;
        }
        M32(var_s0, 0x14) = var_v0_2;
        M32(var_s0, 0x16) = var_v1;
        M32(var_s0, 0x18) = 0;
        var_s3 += 1;
        M32(var_s0, 0x1A) = 0;
        var_s2 += 0x20;
        M32(var_s0, 0x1C) = 0;
        M32(var_s0, 0x1E) = 0;
        var_s0 += 0x20;
    } while (var_s3 < 8);
    func_003E6AB0((u32 *)(var_s2), 1, 0);
    return var_s2;
}
#undef M32
#else
INCLUDE_ASM("asm/nonmatchings/gallery_jpeg", func_003E6B10);
#endif

LIT4_WORD(D_005F58E4, 0x38AF9220);
LIT4_WORD(D_005F58E8, 0x38C9539C);
LIT4_WORD(D_005F58EC, 0x37EFACAD);
LIT4_WORD(D_005F58F0, 0x4708B800);
LIT4_WORD(D_005F58F4, 0x45DAC000);
LIT4_WORD(D_005F58F8, 0x4708B800);
LIT4_WORD(D_005F58FC, 0x4708B800);
LIT4_WORD(D_005F5900, 0x38FB3339);
LIT4_WORD(D_005F5904, 0x38C9539C);
LIT4_WORD(D_005F5908, 0x392EC33E);
LIT4_WORD(D_005F590C, 0x45BB8000);
LIT4_WORD(D_005F5910, 0x453B8000);
LIT4_WORD(D_005F5914, 0x45BB8000);
LIT4_WORD(D_005F5918, 0x45BB8000);
#if 0
/* NON_MATCHING: rough m2c-based draft (VU1 particle packet set-up), not tuned */
#define M32(p, o) (*(s32 *)((u8 *)(p) + (o)))
u8 *func_00343F08();
s32 *func_003E6B10(f32, s32 *);
extern s32 D_005F706C;
extern s32 D_005F7070;
extern u8 *D_005F7074;
extern u8 *D_005F7078;
extern s32 *D_005F707C;
extern u8 *D_005F7080;
extern s32 *D_005F7084;
extern u8 *D_005F7088;
s32 func_003E6DA0(s32 arg0_, s32 arg1) {
    u8 *arg0 = (u8 *)arg0_;
    u8 spbuf[0x400];
    f32 temp_f3;
    f32 temp_f3_2;
    s32 *temp_s0_10;
    s32 *temp_s0_11;
    s32 *temp_s0_4;
    s32 *temp_s0_5;
    s32 *temp_s0_6;
    s32 *temp_s0_7;
    s32 *temp_s0_8;
    s32 *temp_s0_9;
    s32 *temp_t2;
    s32 *temp_v0;
    s32 *temp_v0_2;
    s32 *temp_v0_3;
    s32 *temp_v0_4;
    s32 *temp_v0_5;
    s32 *temp_v0_6;
    s32 *temp_v1_2;
    s32 *var_s0;
    s32 *var_s0_2;
    s32 temp_v1;
    s32 var_s2_6;
    s32 var_v1;
    u32 var_s2;
    u32 var_s2_2;
    u32 var_s2_3;
    u32 var_s2_4;
    u32 var_s2_5;
    u32 var_s4;
    u32 var_s5;
    u8 *temp_s0;
    u8 *temp_s0_2;
    u8 *temp_s0_3;
    u8 *var_a0;

    if (arg1 != 0) {
        var_s4 = 0x82;
        var_s5 = 0x46;
    } else {
        var_s4 = 5;
        var_s5 = 2;
    }
    M32(arg0, 0x8) = 0;
    M32(arg0, 0x0) = 0x11000000;
    M32(arg0, 0x4) = 0x01000404;
    func_003E6AA8((u32 *)(arg0 + 0xC));
    var_s2 = 0;
    D_005F7074 = arg0 + 0x10;
    temp_s0 = arg0 + 0x60;
    M32(arg0, 0x10) = (s64) M32(func_00343F08(6), 0x10);
    M32(arg0, 0x18) = 6;
    M32(arg0, 0x20) = 5;
    M32(arg0, 0x28) = 8;
    M32(arg0, 0x30) = 0x53005;
    M32(arg0, 0x38) = 0x47;
    M32(arg0, 0x40) = 0x44;
    M32(arg0, 0x48) = 0x42;
    M32(arg0, 0x58) = 0x80;
    M32(arg0, 0x5C) = 0x60;
    M32(arg0, 0x50) = 0x80;
    M32(arg0, 0x54) = 0x80;
    D_005F7078 = temp_s0;
    M32(temp_s0, 0xC) = (f32) 8.37186817e-05f;
    M32(temp_s0, 0x4) = (f32) 0;
    M32(arg0, 0x74) = 0x10000000;
    M32(arg0, 0x78) = 0xE;
    M32(arg0, 0x80) = 0x8020;
    M32(arg0, 0x84) = 0x902A4000;
    M32(arg0, 0x88) = 0x24242421;
    M32(arg0, 0x8C) = 4;
    M32(arg0, 0x70) = 4;
    M32(arg0, 0x7C) = 0;
    D_005F7080 = arg0 + 0x90;
    M32(arg0, 0x98) = (f32) 35000.0f;
    M32(arg0, 0x9C) = (f32) 0;
    M32(arg0, 0x94) = 0x1FF;
    D_005F7088 = arg0 + 0xA0;
    temp_s0_2 = arg0 + 0xE0;
    M32(temp_s0_2, 0x3C) = 0.0f;
    M32(temp_s0_2, 0x2C) = 0.0f;
    temp_f3 = -7000.0f;
    M32(temp_s0_2, 0x20) = 0.0f;
    M32(temp_s0_2, 0x1C) = 0.0f;
    M32(temp_s0_2, 0x14) = 0.0f;
    M32(temp_s0_2, 0xC) = 0.0f;
    M32(temp_s0_2, 0x4) = 0.0f;
    M32(arg0, 0xE0) = 0.0f;
    M32(temp_s0_2, 0x8) = 1.0f;
    M32(temp_s0_2, 0x38) = 1.0f;
    M32(temp_s0_2, 0x34) = 1.0f;
    M32(temp_s0_2, 0x30) = 1.0f;
    M32(temp_s0_2, 0x28) = 1.0f;
    M32(temp_s0_2, 0x24) = 1.0f;
    M32(temp_s0_2, 0x18) = 1.0f;
    M32(temp_s0_2, 0x10) = 1.0f;
    temp_s0_3 = arg0 + 0x120;
    do {
        temp_v0 = (var_s2 * 4) + temp_s0_3;
        var_s2 += 1;
        *temp_v0 = 0;
    } while (var_s2 < 0x10U);
    M32(temp_s0_3, 0x20) = temp_f3;
    var_s2_2 = 0;
    M32(arg0, 0x120) = (f32) 7000.0f;
    M32(temp_s0_3, 0x18) = (f32) 7000.0f;
    M32(temp_s0_3, 0x38) = temp_f3;
    var_a0 = arg0 + 0x160;
    temp_v1_2 = var_a0 + 0xA0;
    do {
        M32(var_a0, 0x8) = 1.0f;
        var_s2_2 += 1;
        M32(var_a0, 0x0) = 1.0f;
        M32(var_a0, 0x18) = 0.0f;
        M32(var_a0, 0xC) = 0.0078125;
        M32(var_a0, 0x4) = 1.0f;
        M32(var_a0, 0x1C) = 0.0f;
        M32(var_a0, 0x14) = 0.0f;
        var_a0 += 0x20;
    } while (var_s2_2 < 5U);
    func_003E6AB0((u32 *)(temp_v1_2), 0, 0);
    var_s2_3 = 1;
    M32(temp_v1_2, 0x0) = 0x03000020;
    M32(temp_v1_2, 0x4) = 0x02000210;
    temp_v0_2 = func_003E6B10(35000.0f, temp_v1_2 + 4 + 4);
    *temp_v0_2 = 0x14000708;
    var_s0 = temp_v0_2 + 4;
    if (var_s4 > 1U) {
        do {
            var_s2_3 += 1;
            temp_v0_3 = func_003E6B10(35000.0f, var_s0);
            *temp_v0_3 = 0x17000000;
            var_s0 = temp_v0_3 + 4;
        } while (var_s2_3 < var_s4);
    }
    var_s2_4 = 0;
    *var_s0 = 0x10000000;
    temp_s0_4 = var_s0 + 4;
    func_003E6AA8((u32 *)(temp_s0_4));
    temp_s0_5 = temp_s0_4 + 4;
    D_005F707C = temp_s0_5;
    M32(temp_s0_5, 0x4) = (f32) 0.000119781536f;
    M32(temp_s0_5, 0xC) = (f32) 9.60000034e-05f;
    temp_s0_6 = temp_s0_5 + 0x10;
    func_003E6AB0((u32 *)(temp_s0_6), 0, 5);
    func_003E6AA8((u32 *)(temp_s0_6));
    temp_s0_7 = temp_s0_6 + 4;
    D_005F7084 = temp_s0_7;
    temp_s0_8 = temp_s0_7 + 4;
    M32(temp_s0_7, 0x4) = 0xFF;
    temp_s0_9 = temp_s0_8 + 4;
    M32(temp_s0_8, 0x4) = (f32) 0.000166666665f;
    M32(temp_s0_9, 0x4) = (f32) 6000.0f;
    temp_s0_10 = temp_s0_9 + 4 + 4;
    func_003E6AB0((u32 *)(temp_s0_10), 0, 8);
    func_003E6AA8((u32 *)(temp_s0_10));
    temp_s0_11 = temp_s0_10 + 4;
    temp_t2 = temp_s0_11 + 0x40;
    temp_f3_2 = -3000.0f;
    do {
        temp_v0_4 = &temp_s0_11[var_s2_4];
        var_s2_4 += 1;
        *temp_v0_4 = 0;
    } while (var_s2_4 < 0x10U);
    M32(temp_s0_11, 0x20) = temp_f3_2;
    M32(temp_s0_10, 0x4) = (f32) 3000.0f;
    M32(temp_s0_11, 0x18) = (f32) 3000.0f;
    var_s2_5 = 1;
    M32(temp_s0_11, 0x38) = temp_f3_2;
    func_003E6AB0((u32 *)(temp_t2), 0, 0x11);
    temp_v0_5 = func_003E6B10(6000.0f, temp_t2);
    *temp_v0_5 = 0x14000708;
    var_s0_2 = temp_v0_5 + 4;
    if (var_s5 > 1U) {
        do {
            var_s2_5 += 1;
            temp_v0_6 = func_003E6B10(6000.0f, var_s0_2);
            *temp_v0_6 = 0x17000000;
            var_s0_2 = temp_v0_6 + 4;
        } while (var_s2_5 < var_s5);
    }
    var_s2_6 = ((u32) var_s0_2 >> 2) & 3;
    var_v1 = (u8 *)var_s0_2 - arg0;
    if (var_s2_6 != 0) {
        do {
            *var_s0_2 = 0;
            var_s2_6 -= 1;
            var_s0_2 += 4;
        } while (var_s2_6 != 0);
        var_v1 = (u8 *)var_s0_2 - arg0;
    }
    temp_v1 = var_v1 >> 4;
    D_005F706C = 0;
    D_005F7070 = temp_v1;
    return temp_v1;
}
#undef M32
#else
INCLUDE_ASM("asm/nonmatchings/gallery_jpeg", func_003E6DA0);
#endif

#if 0
s32 func_003E7210(s32 a) {
    s32 hi = 0;

    switch (D_005F5CB6) {
    case 19:
        hi = 1;
    case 2:
    case 22:
        break;
    default:
        return 0;
    }
    return func_003E6DA0(a, hi) * 16;
}
#else
INCLUDE_ASM("asm/nonmatchings/gallery_jpeg", func_003E7210);
#endif

LIT4_WORD(D_005F591C, 0x4688B800);
LIT4_WORD(D_005F5920, 0x453B8000);
LIT4_WORD(D_005F5924, 0x38C90FDB);
LIT4_WORD(D_005F5928, 0x38C90FDB);
LIT4_WORD(D_005F592C, 0x46EA6000);
LIT4_WORD(D_005F5930, 0x45BB8000);
#if 0
/* NON_MATCHING: rough m2c-based draft (per-frame particle matrix and DMA kick), not tuned */
#define M32(p, o) (*(s32 *)((u8 *)(p) + (o)))
s32 Dma_Wait();
s32 FlushCache(s32, s32 *);
s32 Spr_Setup(s32, s32);
f32 cosf(f32);
void func_00343178(s32, s32);
u8 *func_00343F08();
void func_00344150();
void func_003622F8();
s32 func_003678C8();
s32 func_003A9698();
s32 sceVif1PkInit(u8 *, s32);
s32 sceVif1PkRef(u8 *, s32, s32, s32);
s32 sceVif1PkSize(u8 *);
s32 sceVif1PkTerminate(u8 *);
extern u8 D_00415808[];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 D_005F5F10;
extern s32 D_005F706C;
extern s32 D_005F7070;
extern s64 *D_005F7074;
extern u8 *D_005F7078;
extern u8 *D_005F707C;
extern s32 *D_005F7080;
extern s32 *D_005F7084;
extern s32 D_005F7088;
extern s16 gMode;
void func_003E7270(s32 arg0) {
    u8 spbuf[0x400];
    f32 *temp_v1;
    f32 temp_f5;
    f32 var_f0;
    f32 var_f1;
    f32 temp_f20;
    s16 temp_s0;
    s32 temp_a0;
    s32 temp_v0;
    s32 temp_v0_2;
    s32 var_a0;

    temp_v0 = func_003678C8();
    temp_f20 = 3000.0f;
    if (temp_v0 != 0) {
        func_00344150();
        func_003622F8();
        var_a0 = 0;
        *D_005F7074 = M32(func_00343F08(6), 0x10);
        do {
            temp_v0_2 = var_a0 * 4;
            temp_v1 = temp_v0_2 + arg0;
            var_a0 += 1;
            *(s32 *)(temp_v0_2 + D_005F7088) = *temp_v1;
        } while (var_a0 < 0x10);
        temp_s0 = (s32) M32(((u8 *)&D_00415808), 0x4) * -0x10;
        temp_a0 = temp_s0 & 0xFFFF;
        if (temp_a0 <= 0x7FFF) {
            var_f1 = (f32) temp_a0;
            var_f0 = 9.58738019e-05f;
        } else {
            var_f1 = 9.58738019e-05f;
            var_f0 = (f32) (temp_a0 + 0xFFFF0000);
        }
        temp_f5 = cosf(var_f0 * var_f1);
        if (temp_s0 < 0) {
            0;
        } else {
            0;
        }
        M32(D_005F7078, 0x0) = (f32) ((f32) temp_f20 + (0 * 30000.0f) + (f32) M32(((u8 *)&D_00415808), 0x14));
        M32(D_005F7078, 0x8) = (f32) (((f32) temp_f20 - (temp_f5 * 30000.0f)) - (f32) M32(((u8 *)&D_00415808), 0x1C));
        M32(D_005F707C, 0x0) = (f32) (0 + (0 * 6000.0f) + (f32) M32(((u8 *)&D_00415808), 0x14));
        M32(D_005F707C, 0x8) = (f32) ((0 - (temp_f5 * 6000.0f)) - (f32) M32(((u8 *)&D_00415808), 0x1C));
        *D_005F7080 = D_005F706C;
        *D_005F7084 = D_005F706C;
        FlushCache(0, D_005F7080);
        Spr_Setup(1, 0x200);
        sceVif1PkInit(spbuf, *(s32 *)0x70000000);
        Dma_Wait(0x1000D400);
        sceVif1PkRef(spbuf, temp_v0, D_005F7070, 0);
        sceVif1PkTerminate(spbuf);
        func_00343178(0 /* stack arg 1 */, sceVif1PkSize(spbuf));
        if ((D_005F5EE0 == 0) && (D_005F5EE4 == 0) && (func_003A9698() == 0) && (gMode != 5) && (D_005F5F10 != 0)) {
            D_005F706C += 1;
        }
    }
}
#undef M32
#else
INCLUDE_ASM("asm/nonmatchings/gallery_jpeg", func_003E7270);
#endif

void Jpeg_WriteHeader(void) {
    JpegComment com;
    s32 i;

    func_003E8D80(0xFF);
    func_003E8D80(0xD8);
    func_003E8D80(0xFF);
    func_003E8D80(0xE0);
    func_003E8DB0(16);
    func_003E8DF0(D_005F6980, 5);
    func_003E8DB0(0x102);
    func_003E8D80(0);
    func_003E8DB0(1);
    func_003E8DB0(1);
    func_003E8D80(0);
    func_003E8D80(0);
    com = D_00523558;
    func_003E8D80(0xFF);
    func_003E8D80(0xFE);
    func_003E8DB0(22);
    func_003E8DF0(com.s, 20);
    func_003E8D80(0xFF);
    func_003E8D80(0xDB);
    func_003E8DB0(67);
    func_003E8D80(0);
    for (i = 0; i < 64; i++) {
        func_003E8D80(D_0046B4A8[D_00523518[i]]);
    }
    func_003E8D80(0xFF);
    func_003E8D80(0xDB);
    func_003E8DB0(67);
    func_003E8D80(1);
    for (i = 0; i < 64; i++) {
        func_003E8D80(D_0046B4E8[D_00523518[i]]);
    }
    func_003E8DF0(D_0046B998, 33);
    func_003E8DF0(D_0046B9C0, 33);
    func_003E8DF0(D_0046B9E8, 183);
    func_003E8DF0(D_0046BAA0, 183);
    func_003E8D80(0xFF);
    func_003E8D80(0xC0);
    func_003E8DB0(17);
    func_003E8D80(8);
    func_003E8DB0(448);
    func_003E8DB0(640);
    func_003E8D80(3);
    func_003E8D80(0);
    func_003E8D80(0x22);
    func_003E8D80(0);
    func_003E8D80(1);
    func_003E8D80(0x11);
    func_003E8D80(1);
    func_003E8D80(2);
    func_003E8D80(0x11);
    func_003E8D80(1);
    func_003E8D80(0xFF);
    func_003E8D80(0xDA);
    func_003E8DB0(12);
    func_003E8D80(3);
    for (i = 0; i < 3; i++) {
        func_003E8D80(i);
        func_003E8D80(i != 0 ? 0x11 : 0);
    }
    func_003E8D80(0);
    func_003E8D80(63);
    func_003E8D80(0);
}

void Jpeg_WriteEoi(void) {
    func_003E8D80(0xFF);
    func_003E8D80(0xD9);
}

void Jpeg_RgbToYcc(s32 mx, s32 my) {
    s32 b, c, off, i, j;
    u8 *src, *src2;
    f32 *y, *cb, *cr;
    f32 r, g, bl;

    my *= 8;
    off = (my * 640 + mx * 16) * 3;
    for (b = 0; b < 4; b++) {
        switch (b) {
        case 0:
            c = 0;
            break;
        case 1:
            c = 4;
            off += 24;
            break;
        case 2:
            c = 32;
            off += 0x3BE8;
            break;
        case 3:
            c = 36;
            off += 24;
            break;
        default:
            D_005F7094 = 1;
            c = 0;
            break;
        }
        src = D_005F7090 + off;
        y = (f32 *)D_01FC4AA0[b];
        cb = (f32 *)D_01FC4EA0 + c;
        cr = (f32 *)D_01FC4FA0 + c;
        src2 = src + 0x780;
        i = 4;
        do {
            for (j = 8; j > 0; j--) {
                r = *src++;
                g = *src++;
                bl = *src++;
                *y++ = r * 0.299f + g * 0.587f + bl * 0.114f - 128.0f;
                if ((j & 1) == 0) {
                    *cb++ = -(r * 0.1687f) - g * 0.3313f + bl * 0.5f;
                    *cr++ = r * 0.5f - g * 0.4187f - bl * 0.0813f;
                }
            }
            cb += 4;
            cr += 4;
            for (j = 8; j > 0; j--) {
                r = *src2++;
                g = *src2++;
                bl = *src2++;
                *y++ = r * 0.299f + g * 0.587f + bl * 0.114f - 128.0f;
            }
            i--;
            src += 0xEE8;
            src2 += 0xEE8;
        } while (i > 0);
    }
}

INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_005234F0);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523518);
INCLUDE_RODATA("asm/nonmatchings/gallery_jpeg", D_00523558);
f32 Jpeg_Cos(s32 x, s32 u) {
    s32 k;
    x *= 2;
    k = (x * u + u) & 31;
    if (k < 9) {
        return D_005234F0[k];
    }
    if (k < 17) {
        return -D_005234F0[16 - k];
    }
    if (k < 25) {
        return -D_005234F0[k - 16];
    }
    return D_005234F0[32 - k];
}

#if 0
/* NON_MATCHING: 93 lines: gcc hoists 0.0f and the sqrt(2) literal out of the loops and spills the output pointer as u128 */
void Jpeg_Fdct(f32 *in, s32 chroma) {
    s32 *out = D_01FC50A0;
    u8 *qt;
    s32 u, v, x, y;
    f32 sum, cy, r, d;
    f32 *p;

    if (chroma != 0) {
        qt = D_0046B4E8;
    } else {
        qt = D_0046B4A8;
    }
    for (v = 0; v < 8; v++) {
        for (u = 0; u < 8; u++) {
            sum = 0.0f;
            p = in;
            for (y = 0; y < 8; y++) {
                cy = Jpeg_Cos(y, v);
                for (x = 0; x < 8; x++) {
                    sum += *p++ * cy * Jpeg_Cos(x, u);
                }
            }
            r = 0.5f;
            if (sum < 0.0f) {
                r = -0.5f;
            }
            if (v != 0) {
                if (u != 0) {
                    d = *qt++ * 4;
                } else {
                    d = *qt++ * 4 * 1.4142135f;
                }
            } else if (u != 0) {
                d = *qt++ * 4 * 1.4142135f;
            } else {
                d = *qt++ * 8;
            }
            *out++ = sum / d + r;
        }
    }
}
#else
LIT4_WORD(D_005F5950, 0x3FB504F3);
INCLUDE_ASM("asm/nonmatchings/gallery_jpeg", Jpeg_Fdct);
#endif

void Jpeg_EncodeBlock(s32 comp) {
    HuffEnc *t;
    s32 diff, a, nbits, run, k, sym;
    s32 zrl = 0x97;

    if (comp != 0) {
        t = &D_0046B588;
    } else {
        t = &D_0046B550;
    }
    diff = D_01FC50A0[0] - D_01FC51A0[comp];
    a = diff < 0 ? -diff : diff;
    D_01FC51A0[comp] = D_01FC50A0[0];
    nbits = 0;
    while (a != 0) {
        a >>= 1;
        nbits++;
    }
    if (nbits > t->n) {
        nbits = t->n;
        D_005F7094 = 1;
    }
    func_003E8D18(t->code[nbits], t->size[nbits]);
    if (nbits != 0) {
        if (diff < 0) {
            diff--;
        }
        func_003E8D18(diff, nbits);
    }
    if (comp != 0) {
        t = &D_0046B988;
    } else {
        t = &D_0046B788;
    }
    run = 0;
    for (k = 1; k < 64; k++) {
        diff = D_01FC50A0[D_00523518[k]];
        a = diff < 0 ? -diff : diff;
        if (a != 0) {
            while (run >= 16) {
                run -= 16;
                func_003E8D18(t->code[zrl], t->size[zrl]);
            }
            nbits = 0;
            while (a > 0) {
                a >>= 1;
                nbits++;
            }
            sym = run * 10 + nbits + (run == 15);
            if (sym >= t->n) {
                sym = t->n;
                D_005F7094 = 1;
            }
            run = 0;
            func_003E8D18(t->code[sym], t->size[sym]);
            if (diff < 0) {
                diff--;
            }
            func_003E8D18(diff, nbits);
        } else if (k == 63) {
            func_003E8D18(t->code[0], t->size[0]);
        } else {
            run++;
        }
    }
}

void Jpeg_EncodeMcu(void) {
    s32 i;
    for (i = 0; i < 4; i++) {
        Jpeg_Fdct((f32 *)D_01FC4AA0[i], 0);
        Jpeg_EncodeBlock(0);
    }
    Jpeg_Fdct((f32 *)D_01FC4EA0, 1);
    Jpeg_EncodeBlock(1);
    Jpeg_Fdct((f32 *)D_01FC4FA0, 2);
    Jpeg_EncodeBlock(2);
}

s32 Jpeg_EncodeRow(void) {
    s32 x, y;
    s32 r;
    for (y = 0; y < 1; y++) {
        for (x = 0; x < 40; x++) {
            Jpeg_RgbToYcc(x, y);
            Jpeg_EncodeMcu();
        }
    }
    r = func_003E8B40();
    return r;
}

s32 Jpeg_EncodeBegin(u8 *a, s32 b, s32 c) {
    s32 r;
    D_005F7090 = a;
    D_01FC51A0[2] = 0;
    D_01FC51A0[1] = 0;
    D_01FC51A0[0] = 0;
    Bs_Init(b, c);
    Jpeg_WriteHeader();
    r = func_003E8B40();
    return r;
}

s32 Jpeg_EncodeEnd(void) {
    s32 r;
    Jpeg_WriteEoi();
    r = func_003E8B40();
    return r;
}

s32 Jpeg_EncodedSize(void) {
    s32 r = func_003E8B08();
    return r;
}

void Jpeg_SkipHeader(void) {
    func_003E8F58(0x287);
}

s32 Jpeg_CheckEoi(void) {
    if (func_003E8F20() != 0xFF) {
        return 1;
    }
    return func_003E8F20() != 0xD9;
}

u8 Jpeg_HuffDecode(u8 *bits, u8 *vals) {
    s32 k = 0;
    s32 c = 0;
    s32 len = 1;
    s32 code, i;
    code = Bs_GetBitsN(2);
    do {
        i = bits[len];
        while (i-- > 0) {
            if (c++ == code) {
                return vals[k];
            }
            k++;
        }
        len++;
        code = code << 1 | func_003E8EB0();
        c <<= 1;
    } while (len < 16);
    D_005F7094 = 1;
    return 0;
}

void Jpeg_DecodeBlock(s32 comp) {
    u8 *qt;
    u8 *ht;
    u8 *vals;
    u8 s, size;
    s32 v, k, z, run;

    if (comp != 0) {
        qt = D_0046B4E8;
    } else {
        qt = D_0046B4A8;
    }
    if (comp != 0) {
        ht = D_0046B9C0 + 5;
    } else {
        ht = D_0046B998 + 5;
    }
    s = Jpeg_HuffDecode(ht, ht + 16);
    v = Bs_GetBitsN(s);
    if ((v >> (s - 1)) == 0) {
        v += 1 - (1 << s);
    }
    D_01FC51A0[comp] += v;
    D_01FC50A0[0] = D_01FC51A0[comp] * qt[0];
    if (comp != 0) {
        ht = D_0046BAA0 + 5;
    } else {
        ht = D_0046B9E8 + 5;
    }
    vals = ht + 16;
    k = 1;
    while (k < 64) {
        s = Jpeg_HuffDecode(ht, vals);
        if (s == 0) {
            do {
                D_01FC50A0[D_00523518[k++]] = 0;
            } while (k < 64);
            break;
        }
        run = s >> 4;
        if (run + k >= 64) {
            D_005F7094 = 1;
            run = 63 - k;
        }
        while (--run != -1) {
            D_01FC50A0[D_00523518[k++]] = 0;
        }
        size = s & 0xF;
        if (size != 0) {
            z = D_00523518[k++];
            v = Bs_GetBitsN(size);
            if ((v >> (size - 1)) == 0) {
                v += 1 - (1 << size);
            }
            D_01FC50A0[z] = v * qt[z];
        } else {
            D_01FC50A0[D_00523518[k++]] = 0;
        }
    }
}

void Jpeg_Idct(f32 *out) {
    s32 x, y, u, v;
    s32 *p;
    f32 sum, cv;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            sum = 0.0f;
            p = D_01FC50A0;
            for (v = 0; v < 8; v++) {
                if (v != 0) {
                    cv = Jpeg_Cos(y, v);
                    sum += *p++ * 0.70710677f * cv;
                    for (u = 1; u < 8; u++) {
                        sum += *p++ * cv * Jpeg_Cos(x, u);
                    }
                } else {
                    sum += *p++ * 0.5f;
                    for (u = 1; u < 8; u++) {
                        sum += *p++ * 0.70710677f * Jpeg_Cos(x, u);
                    }
                }
            }
            *out++ = sum * 0.25f;
        }
    }
}

void Jpeg_DecodeMcu(void) {
    s32 i;
    for (i = 0; i < 4; i++) {
        Jpeg_DecodeBlock(0);
        Jpeg_Idct((f32 *)D_01FC4AA0[i]);
    }
    Jpeg_DecodeBlock(1);
    Jpeg_Idct((f32 *)D_01FC4EA0);
    Jpeg_DecodeBlock(2);
    Jpeg_Idct((f32 *)D_01FC4FA0);
}

u8 Jpeg_ClampByte(f32 x) {
    if (x < 0.0f) {
        return 0;
    }
    if (x > 255.0f) {
        return 255;
    }
    return (s32)x;
}

#if 0
/* NON_MATCHING: 18 lines: the two line pointers (dst / dst2) take s1 / s0 the other way round */
void Jpeg_YccToRgb(s32 mx, s32 my) {
    s32 b, c, off, i, j;
    u8 *dst2, *dst;
    f32 *y, *cb, *cr;
    f32 l, vb, vr;

    vr = vb = 0.0f;
    my *= 8;
    off = (my * 0x500 + mx * 16) * 3;
    for (b = 0; b < 4; b++) {
        switch (b) {
        case 0:
            c = 0;
            break;
        case 1:
            c = 4;
            off += 24;
            break;
        case 2:
            c = 32;
            off += 0x3BE8;
            break;
        case 3:
            c = 36;
            off += 24;
            break;
        default:
            c = 0;
            D_005F7094 = 1;
            break;
        }
        dst = D_005F7090 + off;
        y = (f32 *)D_01FC4AA0[b];
        cb = (f32 *)D_01FC4EA0 + c;
        cr = (f32 *)D_01FC4FA0 + c;
        dst2 = dst + 0x780;
        for (i = 4; i > 0; i--) {
            for (j = 8; j > 0; j--) {
                l = *y++ + 128.0f;
                if ((j & 1) == 0) {
                    vb = *cb++;
                    vr = *cr++;
                }
                *dst++ = Jpeg_ClampByte(l + vr * 1.402f);
                *dst++ = Jpeg_ClampByte(l - vb * 0.34414f - vr * 0.71414f);
                *dst++ = Jpeg_ClampByte(l + vb * 1.772f);
            }
            dst += 0xEE8;
            cb -= 4;
            cr -= 4;
            for (j = 8; j > 0; j--) {
                l = *y++ + 128.0f;
                if ((j & 1) == 0) {
                    vb = *cb++;
                    vr = *cr++;
                }
                *dst2++ = Jpeg_ClampByte(l + vr * 1.402f);
                *dst2++ = Jpeg_ClampByte(l - vb * 0.34414f - vr * 0.71414f);
                *dst2++ = Jpeg_ClampByte(l + vb * 1.772f);
            }
            cb += 4;
            cr += 4;
            dst2 += 0xEE8;
        }
    }
}
#else
LIT4_WORD(D_005F595C, 0x3FB374BC);
LIT4_WORD(D_005F5960, 0x3EB0331E);
LIT4_WORD(D_005F5964, 0x3F36D1E1);
LIT4_WORD(D_005F5968, 0x3FE2D0E5);
INCLUDE_ASM("asm/nonmatchings/gallery_jpeg", Jpeg_YccToRgb);
#endif

s32 Jpeg_DecodeStrip(void) {
    s32 x, y;
    for (y = 0; y < 14; y++) {
        for (x = 0; x < 40; x++) {
            Jpeg_DecodeMcu();
            Jpeg_YccToRgb(x, y);
        }
    }
    x = 0;
    if (D_005F7094 != 0 || func_003E8B40() != 0) {
        x = 1;
    }
    return x;
}

s32 Jpeg_DecodeBegin(u8 *a, s32 b, s32 c) {
    s32 r;
    D_005F7090 = a;
    r = 0;
    D_005F7094 = 0;
    D_01FC51A0[2] = 0;
    D_01FC51A0[1] = 0;
    D_01FC51A0[0] = 0;
    Bs_Init(b, c);
    Jpeg_SkipHeader();
    if (D_005F7094 != 0 || func_003E8B40() != 0) {
        r = 1;
    }
    return r;
}

s32 Jpeg_DecodeEnd(void) {
    s32 r = 0;
    if (Jpeg_CheckEoi() != 0 || func_003E8B40() != 0) {
        r = 1;
    }
    return r;
}
