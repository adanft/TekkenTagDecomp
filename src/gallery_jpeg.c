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
void func_00399820(const void *addr, s32 n);
u64 *func_00399520(void);
void func_00399650(u64 *p);
void func_003BB890(s32 a, s32 x, s32 y, s32 b);
s32 func_003E5980(s32 x, s32 y, s32 a);
s32 Text_DrawId(s32, s32, s32, s32);
s32 func_003E6DA0(s32, s32);
s32 func_003E7210(s32);
u32 *func_003E6B10(u32 *p, f32 spread, f32 y);
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
/* NON_MATCHING: 829 lines: readable draft of the 16 states (cases 8/9 and 12/13 merged; call arguments checked against the asm); the C emits its own jump table, so jtbl_005234B0 must go when it is enabled */
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
            func_003DBCF0(0, 0, 640, 448, 0xFFFF, D_01FC4A90.unk4 * 16);
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
            func_003DBCF0(0, 0, 640, 448, 0xFFFF, D_01FC4A90.unk4 * 21 + 2);
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
        p[2] = (((u64)0x010A2300 << 16) | 0x10A) << 16;
        p[3] = 0x50;
        p[4] = (u64)(D_01FC4A90.unk4 * 224) << 48;
        p[5] = 0x51;
        p[6] = 0xE000000280;
        p[7] = 0x52;
        p[8] = 0;
        p[9] = 0x53;
        p[10] = 0x080000000000E900;
        p[11] = 0;
        func_00399820(p, 0x6906);
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
        /* State 8 fades the box in (alpha taken before the step) and draws the texts before YES / NO; state 9
         * draws YES / NO and the arrow first. */
        if (D_01FC4A90.unk0 == 8) {
            Gallery_DrawList(0);
            a = D_01FC4A90.unk4 * 21 + 2;
            if (a > 0x80) {
                a = 0x80;
            }
            if (D_01FC4A90.unk4 < 1000) {
                D_01FC4A90.unk4++;
            }
        } else {
            a = 0x80;
        }
        func_003DBCF0(0, 0, 640, 448, 0xFFFF, a);
        if (D_01FC4A90.unk0 == 9) {
            Text_Printf(D_00523498, 204, 246, 10, 8, D_01FC4A90.unkA == 1 ? 0xB : 0x16,
                        D_01FC4A90.unkA == 0 ? 0xB : 0x16);
            func_003BB890(D_01FC4A90.unkA != 1, 317, 256, 5);
        }
        func_003DEB80(0);
        Text_DrawId(320 - Text_Width(0xC2) / 2, 170, 0xC2, 0x40404080);
        Text_DrawId(320 - Text_Width(0xC3) / 2, 190, 0xC3, 0x40404080);
        sprintf(buf, D_00523408, D_01FC4A90.unk2 + 1);
        Text_Printf(D_00523488, 210, 164, 10, 4, 5, buf);
        if (D_01FC4A90.unk0 == 8) {
            Text_Printf(D_00523498, 204, 246, 10, 8, D_01FC4A90.unkA == 1 ? 0xB : 0x16,
                        D_01FC4A90.unkA == 0 ? 0xB : 0x16);
            func_003BB890(D_01FC4A90.unkA != 1, 317, 256, 5);
        }
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
        func_003DBCF0(0, 0, 640, 448, 0xFFFF, 0x80);
        func_003DEB80(0);
        if (D_01FC4A90.unk0 == 12) {
            Text_DrawId(320 - Text_Width(0xBF) / 2, 180, 0xBF, 0x40404080);
            sprintf(buf, D_00523408, D_01FC4A90.unk2 + 1);
            Text_Printf(D_00523488, 206, 172, 10, 4, 0x13, buf);
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
        a = D_01FC4A90.unk4 * 16;
        if (a > 0x80) {
            a = 0x80;
        }
        if (D_01FC4A90.unk4 < 1000) {
            D_01FC4A90.unk4++;
        }
        func_003DBCF0(0, 0, 640, 448, 0xFFFF, a);
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
/* NON_MATCHING: written from the asm, not tuned. Eight random particles (0x20 bytes of halfwords each) after the
 * VIF code word at p, closed by an UNPACK code: x / z spread over +/-spread / 2, height y, a direction and a speed
 * of up to 12 (x16) in the x-y plane, then a random phase below 0x100 (0x200 when spread > 10000). */
f32 cosf(f32);
f32 sqrtf(f32);
int rand(void);
u32 *func_003E6B10(u32 *p, f32 spread, f32 y) {
    s16 *q;
    u32 *end;
    f32 a, r, c, s;
    s32 i;

    q = (s16 *)(p + 1);
    end = p + 1;
    func_003E6AA8(p);
    i = 0;
    do {
        q[0] = (s32)(spread * ((f32)(rand() & 0xFFFFFF) * 5.9604645e-8f - 0.5f));
        q[1] = (s32)y;
        q[2] = (s32)(spread * ((f32)(rand() & 0xFFFFFF) * 5.9604645e-8f - 0.5f));
        q[3] = 1;
        a = (f32)(rand() & 0xFFFFFF) * 5.9604645e-8f;
        a = (a + a) * 3.14159274f - 3.14159274f;
        r = (f32)(rand() & 0xFFFFFF) * 5.9604645e-8f * 12.0f;
        c = cosf(a);
        q[4] = (s32)(r * c * 16.0f);
        q[5] = -0x181;
        s = sqrtf(1.0f - c * c);
        if (a > 0.0f) {
            q[6] = (s32)(r * s * 16.0f);
        } else {
            q[6] = (s32)(-r * s * 16.0f);
        }
        q[7] = 0;
        q[8] = 0x5DC0;
        q[9] = 0x5DC0;
        if (spread > 10000.0f) {
            q[10] = rand() & 0x1FF;
            q[11] = 0x200;
        } else {
            q[10] = rand() & 0xFF;
            q[11] = 0x100;
        }
        q[12] = 0;
        q[13] = 0;
        q[14] = 0;
        q[15] = 0;
        i++;
        end += 8;
        q += 16;
    } while (i < 8);
    func_003E6AB0(end, 1, 0);
    return end;
}
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
/* NON_MATCHING: written from the asm, not tuned. Builds the VIF1 packet of the VU1 particle program
 * (src/vu1/sprites.vsm) at p: a GIF tag and register list, the program's constants, five unit matrices, then
 * `big ? 130 : 5` batches of 8 far particles (MSCAL 0x708, then MSCNT) and `big ? 70 : 2` batches of near ones,
 * padded with zero words. Records the blocks func_003E7270 updates every frame and returns the size in
 * quadwords. */
u8 *func_00343F08(s32 i);
extern s32 D_005F706C;
extern s32 D_005F7070;
extern u32 *D_005F7074;
extern f32 *D_005F7078;
extern f32 *D_005F707C;
extern u32 *D_005F7080;
extern u32 *D_005F7084;
extern f32 *D_005F7088;
s32 func_003E6DA0(s32 arg0, s32 big) {
    u32 *base = (u32 *)arg0;
    u32 *p;
    f32 *m;
    u32 far, near;
    u32 i;
    s32 size;

    if (big != 0) {
        far = 0x82;
        near = 0x46;
    } else {
        far = 5;
        near = 2;
    }
    base[2] = 0;
    base[0] = 0x11000000;
    base[1] = 0x01000404;
    func_003E6AA8(base + 3);
    D_005F7074 = base + 4;
    *(u64 *)(base + 4) = *(u64 *)((u8 *)func_00343F08(6) + 0x10);
    *(u64 *)(base + 6) = 6;
    *(u64 *)(base + 8) = 5;
    *(u64 *)(base + 10) = 8;
    *(u64 *)(base + 12) = 0x53005;
    *(u64 *)(base + 14) = 0x47;
    *(u64 *)(base + 16) = 0x44;
    *(u64 *)(base + 18) = 0x42;
    base[22] = 0x80;
    base[23] = 0x60;
    base[20] = 0x80;
    base[21] = 0x80;
    D_005F7078 = (f32 *)(base + 24);
    base[27] = 0x38C9539C; /* (f32 words) */
    base[25] = 0x38AF9220;
    base[29] = 0x10000000;
    base[30] = 0xE;
    base[32] = 0x8020;
    base[33] = 0x902A4000;
    base[34] = 0x24242421;
    base[35] = 4;
    base[28] = 4;
    base[31] = 0;
    D_005F7080 = base + 36;
    base[38] = 0x37EFACAD;
    base[39] = 0x4708B800; /* 35000.0f */
    base[37] = 0x1FF;
    D_005F7088 = (f32 *)(base + 40);
    m = (f32 *)(base + 56);
    m[15] = 0.0f;
    m[11] = 0.0f;
    m[8] = 0.0f;
    m[7] = 0.0f;
    m[5] = 0.0f;
    m[3] = 0.0f;
    m[1] = 0.0f;
    m[0] = 0.0f;
    m[2] = 1.0f;
    m[14] = 1.0f;
    m[13] = 1.0f;
    m[12] = 1.0f;
    m[10] = 1.0f;
    m[9] = 1.0f;
    m[6] = 1.0f;
    m[4] = 1.0f;
    m = (f32 *)(base + 72);
    for (i = 0; i < 16; i++) {
        ((u32 *)m)[i] = 0;
    }
    m[8] = -7000.0f;
    m[0] = 7000.0f;
    m[6] = 7000.0f;
    m[14] = -7000.0f;
    m = (f32 *)(base + 88);
    for (i = 0; i < 5; i++) {
        m[2] = 1.0f;
        m[0] = 1.0f;
        m[6] = 0.0f;
        m[4] = 0.0f;
        m[3] = 1.0f;
        m[1] = 1.0f;
        m[7] = 0.0f;
        m[5] = 0.0f;
        m += 8;
    }
    p = base + 128;
    func_003E6AB0(p, 0, 0);
    p[0] = 0x03000020;
    p++;
    p[0] = 0x02000210;
    p = func_003E6B10(p + 1, 35000.0f, 12288.0f);
    *p++ = 0x14000708;
    for (i = 1; i < far; i++) {
        p = func_003E6B10(p, 35000.0f, 12288.0f);
        *p++ = 0x17000000;
    }
    *p++ = 0x10000000;
    func_003E6AA8(p);
    p++;
    D_005F707C = (f32 *)p;
    p[1] = 0x38FB3339; /* (f32 words) */
    p[3] = 0x38C9539C;
    p += 4;
    func_003E6AB0(p, 0, 5);
    func_003E6AA8(p);
    p++;
    D_005F7084 = p;
    p++;
    p[0] = 0xFF;
    p++;
    p[0] = 0x392EC33E; /* (f32 word) */
    p++;
    ((f32 *)p)[0] = 6000.0f;
    p++;
    func_003E6AB0(p, 0, 8);
    func_003E6AA8(p);
    p++;
    m = (f32 *)p;
    for (i = 0; i < 16; i++) {
        ((u32 *)m)[i] = 0;
    }
    m[8] = -3000.0f;
    m[0] = 3000.0f;
    m[6] = 3000.0f;
    m[14] = -3000.0f;
    p += 16;
    func_003E6AB0(p, 0, 0x11);
    p = func_003E6B10(p, 6000.0f, 6144.0f);
    *p++ = 0x14000708;
    for (i = 1; i < near; i++) {
        p = func_003E6B10(p, 6000.0f, 6144.0f);
        *p++ = 0x17000000;
    }
    i = ((u32)p >> 2) & 3;
    size = (u8 *)p - (u8 *)base;
    if (i != 0) {
        do {
            *p++ = 0;
            i--;
        } while (i != 0);
        size = (u8 *)p - (u8 *)base;
    }
    size >>= 4;
    D_005F706C = 0;
    D_005F7070 = size;
    return size;
}
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
/* NON_MATCHING: written from the asm, not tuned. Per frame: when the particle packet exists (func_003678C8), sets
 * its GIF register, view matrix and the far / near emitter positions around the camera (angle from D_00415808[1],
 * position from D_00415808[5] / [7]) and the frame counter, then sends the packet to VIF1 by reference; the counter
 * advances while the game runs (not paused, not mode 5, D_005F5F10 set). */
/* VIF1 packet of the SDK (sceVif1Packet). */
typedef struct {
    /* 0x0 */ u32 *pCurrent;
    /* 0x4 */ u32 *pBase;
    /* 0x8 */ u32 *pDmaTag;
    /* 0xC */ u32 *pVifCode;
    /* 0x10 */ u32 pad10[4];
} Vif1Packet;
void FlushCache(s32 op);
u32 Spr_Setup(u32 count, s32 reserved);
void func_00343178(u32 spr, u32 qwc);
u32 Dma_Wait(volatile u32 *chan);
void sceVif1PkInit(Vif1Packet *pk, u32 base);
void sceVif1PkRef(Vif1Packet *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceVif1PkTerminate(Vif1Packet *pk);
u32 sceVif1PkSize(Vif1Packet *pk);
s32 func_003678C8(void);
void func_00344150(void);
void func_003622F8(void);
s32 func_003A9698(void);
extern s32 D_00415808[];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 D_005F5F10;
extern s16 gMode;
void func_003E7270(void *arg0) {
    Vif1Packet pk;
    f32 *mtx = arg0;
    s32 data;
    s32 i;
    s16 ang;
    s32 a;
    f32 c, s;

    data = func_003678C8();
    if (data == 0) {
        return;
    }
    func_00344150();
    func_003622F8();
    *(u64 *)D_005F7074 = *(u64 *)((u8 *)func_00343F08(6) + 0x10);
    for (i = 0; i < 16; i++) {
        D_005F7088[i] = mtx[i];
    }
    ang = (s16)(-(s32)*(u16 *)((u8 *)D_00415808 + 4) * 16);
    a = ang & 0xFFFF;
    if (a <= 0x7FFF) {
        c = cosf(9.58738019e-05f * (f32)a);
    } else {
        c = cosf((f32)(a - 0x10000) * 9.58738019e-05f);
    }
    if (ang < 0) {
        s = -sqrtf(1.0f - c * c);
    } else {
        s = sqrtf(1.0f - c * c);
    }
    D_005F7078[0] = 17500.0f + s * 30000.0f + (f32)D_00415808[5];
    D_005F7078[2] = 17500.0f - c * 30000.0f - (f32)D_00415808[7];
    D_005F707C[0] = 3000.0f + s * 6000.0f + (f32)D_00415808[5];
    D_005F707C[2] = 3000.0f - c * 6000.0f - (f32)D_00415808[7];
    *D_005F7080 = D_005F706C;
    *D_005F7084 = D_005F706C;
    FlushCache(0);
    Spr_Setup(1, 0x200);
    sceVif1PkInit(&pk, *(u32 *)0x70000000);
    Dma_Wait((volatile u32 *)0x1000D400);
    sceVif1PkRef(&pk, (void *)data, D_005F7070, 0, 0, 0);
    sceVif1PkTerminate(&pk);
    func_00343178((u32)pk.pBase, sceVif1PkSize(&pk));
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0 && gMode != 5 && D_005F5F10 != 0) {
        D_005F706C++;
    }
}
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
