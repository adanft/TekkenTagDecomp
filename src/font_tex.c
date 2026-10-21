/* 0x3608B8-0x3621F8 (module font_tex; docs/systems/render.md section 12): the memory card screens that continue
 * src/memcard.c (message and YES / NO windows animated by func_003604C8..func_00360530, the menu state machine
 * func_00360F40, the card check and error screens), then the GS image upload through VIF1 DIRECTHL (Gfx_PkAddImage,
 * func_00361EA8). The start follows memcard's object [I: memcard's .rodata ends where this one's begins, 0x4FC280];
 * the end is a cut [I]: its .rodata (0x4FC280-0x4FC610, a subsegment) runs up to motion's. Every NON_MATCHING draft
 * (#if 0) compiles when enabled. */
#include "common.h"

/* A window rectangle animated between two states (src/memcard.c func_003604C8..func_00360530). */
typedef struct {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 w;
    /* 0x0C */ s32 h;
    /* 0x10 */ s32 color;
    /* 0x14 */ s32 t;
    /* 0x18 */ s32 to[5];
    /* 0x2C */ s32 from[5];
} McWindow; /* 0x40 */

typedef struct {
    /* 0x00 */ s32 state;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 sel;
    /* 0x0C */ s32 yes;
    /* 0x10 */ s32 timer;
    /* 0x14 */ u8 unk14;
    /* 0x15 */ u8 unk15;
    /* 0x16 */ u8 unk16;
    /* 0x17 */ u8 unk17;
    /* 0x18 */ u8 unk18;
    /* 0x19 */ u8 pad19[3];
    /* 0x1C */ s32 held;
    /* 0x20 */ s32 pressed;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ McWindow win;
    /* 0x68 */ McWindow ask;
} McMenu;

/* One of the five texture images placed in VRAM by func_00361EA8 (D_006E3550): its data, a "left out" flag and its size
 * in 256-byte blocks. */
typedef struct {
    /* 0x0 */ void *data;
    /* 0x4 */ u16 skip;
    /* 0x6 */ u16 size;
} TexSlot;

extern TexSlot D_006E3550[];

/* The upload packet of one image (D_006E3500): its size in quadwords and its address. */
typedef struct {
    /* 0x0 */ u16 qwc;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ u8 *addr;
    /* 0x8 */ u64 unk8;
} TexUpload;

/* A DMA tag written at the end of a scratchpad buffer. */
typedef union {
    u128 q;
    u16 h[8];
} SprTag;

extern TexUpload D_006E3500[];
extern u8 D_006D7500[];
extern u8 D_00617500[];
u32 func_00364550(void *data);
s32 func_00364558(u32 i, void *data);
void *func_00364528(u32 i, void *data);
s32 func_003623B8(void *data, s32 vram, u8 **pk, u16 skip, u8 **end);
void Dma_AddRefTag(u8 **pkt, s32 i);
void FlushCache(s32 op);
u32 Spr_Setup(u32 count, s32 reserved);
void func_00343128(u32 spr, void *src, u32 qwc);
u32 Dma_Wait(volatile u32 *chan);
void *func_00342FB0(void);
void func_0035E6B8(void *tag);
void func_00361E20(s32 *order, TexSlot *slots);
void func_00361EA8(s32 *blocks, s32 *subBlocks);

/* VIF1 packet of the SDK (sceVif1Packet). */
typedef struct {
    /* 0x0 */ u32 *pCurrent;
    /* 0x4 */ u32 *pBase;
    /* 0x8 */ u32 *pDmaTag;
    /* 0xC */ u32 *pVifCode;
    /* 0x10 */ u32 pad10[4];
} Vif1Packet;

typedef union {
    u64 d[2];
    u128 q;
} GifTag;

extern GifTag D_00416190;
extern u128 D_004FC350[];

void sceVif1PkCnt(Vif1Packet *pk, u32 flag);
void sceVif1PkOpenDirectHLCode(Vif1Packet *pk, s32 irq);
void sceVif1PkCloseDirectHLCode(Vif1Packet *pk);
void sceVif1PkOpenGifTag(Vif1Packet *pk, u128 tag);
void sceVif1PkCloseGifTag(Vif1Packet *pk);
void sceVif1PkAddGsAD(Vif1Packet *pk, s32 addr, u64 data);
void sceVif1PkRef(Vif1Packet *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);

/* A message of the memory card screens: color and a run of text lines. */
typedef struct {
    /* 0x0 */ s32 color;
    /* 0x4 */ u8 pad4;
    /* 0x5 */ u8 lines;
    /* 0x6 */ u16 first;
} McMsg;

extern McMsg D_00416078[];
extern McMenu D_006173F8;
extern u8 D_005F6B44;
extern s8 D_00439C92[];
extern s8 D_0061740E[];

void func_003604C8(McWindow *v, s32 a, s32 b, s32 c, s32 d, s32 e);
void func_003604F0(McWindow *v, s32 a, s32 b, s32 c, s32 d, s32 e);
void func_00360530(McWindow *v);
s32 Text_Printf(const char *fmt, ...);
void func_003BB890(s32 a, s32 x, s32 y, s32 b);
void func_003B4D28(s32 se);
s32 func_0035C508(s32 lo, s32 v, s32 hi);
s32 func_0035E920(void);
s32 func_0035E8D8(void);
s32 func_0035E838(void);
s32 func_0035E858(void);
s32 func_0035E878(void);
s32 func_00360E88(s32 err);
extern s32 D_005F5CDC;
void func_00360A30(McMenu *m, s32 msg);
void func_003608E0(McMenu *m);
void func_00360B90(McMenu *m);
s32 func_00360DE0(s32 err);
s32 func_00360E38(s32 err);
s32 Text_Width(s32 id);
s32 Text_LineCount(s32 id);
void Text_DrawId(s32 x, s32 y, s32 id, s32 color);
void func_003DEB80(s32 a);

/* The entries of the memory card menu (.data; src/memcard.c). */
typedef struct {
    /* 0x0 */ s32 type;
    /* 0x4 */ const char *help;
    /* 0x8 */ const char *label;
} McMenuItem;

extern McMenuItem D_00416040[];
extern s8 D_00439C60[];
extern s32 D_005F6B40;
void func_00360670(McMenu *m);
void Options_Apply(void);
s32 func_0035E898(void);
s32 func_0035E8B8(void);

void func_003608B8(McMenu *m) {
    D_005F6B44 = 0xFF;
    func_003604C8(&m->ask, 0x140, 0x138, 0, 0, 0);
}

/* NON_MATCHING: 13 lines: both window-call arms end in a branch to the join in the original (no fall-through), and the
 * prologue order differs */
#if 0
/* Draws the YES / NO question window under the message window (sliding in or out when unk15 changes). */
void func_003608E0(McMenu *m) {
    McWindow *ask = &m->ask;
    s32 y;
    s32 a;
    s32 b;

    if (D_005F6B44 != m->unk15) {
        D_005F6B44 = m->unk15;
        if (m->unk17 != 0) {
            func_003604F0(ask, 0xC8, m->win.to[1] + m->win.to[3] + 0x18, 0xF0, 0x32, 0x8060);
        } else {
            func_003604F0(ask, 0x140, 0x138, 0, 0, 0);
        }
    }
    m->ask.t = (8 - m->unk24) << 5;
    func_00360530(ask);
    if (m->unk17 != 0 && m->ask.w == 0xF0) {
        if (m->yes != 0) {
            a = 0xB;
            b = 0x16;
        } else {
            a = 0x16;
            b = 0xB;
        }
        y = m->ask.y;
        Text_Printf("%H%V%F%S%cYES %cNO", m->ask.x + 0xA, y + 0x1E, 0xA, 8, a, b);
        func_003BB890(m->yes == 0, 0x140, y + 0x28, 5);
    }
    m->unk17 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/font_tex", func_003608E0);
#endif

/* Resets the message window (closed, at height y). */
void func_00360A08(McMenu *m, s32 y) {
    m->unk15 = 0;
    m->unk24 = 0;
    func_003604C8(&m->win, 0x140, y, 0, 0, 0);
}

/* Opens message `msg` in the message window: the window is sized to its widest line and its line count. */
void func_00360A30(McMenu *m, s32 msg) {
    McMsg *e;
    s32 i;
    s32 w;
    s32 h;
    s32 maxw;
    s32 rows;
    s32 tw;
    u8 lines;
    u16 first;

    if (msg == m->unk15) {
        return;
    }
    m->unk15 = msg;
    m->unk24 = 8;
    e = &D_00416078[m->unk15];
    rows = 0;
    maxw = 0;
    lines = e->lines;
    w = 0;
    h = 0;
    first = e->first;
    if (lines != 0) {
        for (i = 0; i < lines; i++) {
            tw = Text_Width(first + i);
            if (maxw < tw) {
                maxw = tw;
            }
            rows += Text_LineCount(first + i);
        }
        w = maxw + 0xC;
        h = rows * 0x12 + 0x10;
    }
    func_003604F0(&m->win, 0x140 - (w >> 1), m->win.y, w, h, D_00416078[m->unk15].color);
}

/* NON_MATCHING: 6 lines: the y argument (window y + 8 + 18 per line) is computed with other registers */
#if 0
/* Draws the message window and, once it has opened, the lines of the current message centred. */
void func_00360B90(McMenu *m) {
    McMsg *e;
    s32 i;
    s32 t;
    u8 lines;
    u16 first;

    func_003DEB80(0);
    t = m->unk24;
    if (t != 0) {
        t = --m->unk24;
    }
    m->win.t = (8 - t) << 5;
    func_00360530(&m->win);
    e = &D_00416078[m->unk15];
    lines = e->lines;
    first = e->first;
    if (m->unk24 == 0) {
        if (lines != 0) {
            for (i = 0; i < lines; i++) {
                Text_DrawId(0x140 - (Text_Width(first + i) >> 1), m->win.y + i * 0x12 + 8, first + i, 0x60606080);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/font_tex", func_00360B90);
#endif

void func_00360C98(McMenu *m, s32 yes) {
    m->yes = yes;
}

/* YES / NO input: left / right (0x8000 / 0x2000) move the cursor, a confirm or cancel button (0x8FC) ends with a
 * sound; returns 1 when answered. */
s32 func_00360CA0(McMenu *m) {
    s32 pressed;
    s32 done;
    s32 yes;

    m->unk17 = 1;
    pressed = m->pressed;
    done = (pressed & 0x8FC) != 0;
    if (done) {
        if (m->yes != 0) {
            func_003B4D28(0);
        } else {
            func_003B4D28(1);
        }
    } else {
        yes = m->yes;
        if (pressed & 0x8000) {
            yes = 1;
        }
        if (pressed & 0x2000) {
            yes = 0;
        }
        if (yes != m->yes) {
            func_003B4D28(2);
            m->yes = yes;
        }
    }
    return done;
}

s32 func_00360D48(McMenu *m) {
    return m->yes;
}

/* Moves the selection up (0x1000) / down (bit 14) within 0..3; on a change plays the cursor sound and redraws. */
s32 func_00360D50(McMenu *m) {
    s32 sel = m->sel;
    s32 down = (m->held >> 14) & 1;
    s32 up;

    if (m->held & 0x1000) {
        up = sel - 1;
        sel = up + down;
    } else {
        sel = sel + down;
    }
    sel = func_0035C508(0, sel, 3);
    if (sel != m->sel) {
        func_003B4D28(2);
        m->sel = sel;
        func_00360A30(m, 0);
        return 1;
    }
    return 0;
}

/* Error class of a card status: 1 for the flags shown as a failure (no card, unformatted ...), 0 otherwise. */
s32 func_00360DE0(s32 err) {
    s32 e;

    if (err & 0x1) {
        return 1;
    }
    if (err & 0x8) {
        return 1;
    }
    if (err & 0x4) {
        return 0;
    }
    if (err & 0x400) {
        return 1;
    }
    if (err & 0x100) {
        return 1;
    }
    e = err; /* fake */
    if (e & 0x200) {
        return 0;
    }
    if (e & 0x4000) {
        e = 1; /* fake */
        return e;
    }
    return 0;
}

s32 func_00360E38(s32 err) {
    s32 e;

    if (err & 0x1) {
        return 1;
    }
    if (err & 0x8) {
        return 1;
    }
    if (err & 0x4) {
        return 1;
    }
    if (err & 0x400) {
        return 1;
    }
    if (err & 0x100) {
        return 1;
    }
    e = err; /* fake */
    if (e & 0x200) {
        return 1;
    }
    if (e & 0x4000) {
        e = 1; /* fake */
        return e;
    }
    return 0;
}

s32 func_00360E88(s32 err) {
    s32 e;

    if (err & 0x1) {
        return 2;
    }
    if (err & 0x8) {
        return 2;
    }
    if (err & 0x4) {
        return 1;
    }
    if (err & 0x400) {
        return 2;
    }
    if (err & 0x100) {
        return 2;
    }
    e = err; /* fake */
    if (e & 0x200) {
        return 1;
    }
    if (e & 0x4000) {
        e = 2; /* fake */
        return e;
    }
    return 0;
}

s32 func_00360EE0(McMenu *m) {
    s32 r = 0;

    if (m->pressed & 0x100) {
        func_003B4D28(0);
        r = 1;
        m->state = 0x1A;
    }
    return r;
}

void func_00360F28(McMenu *m, s32 next) {
    m->timer = 0x3C;
    m->state = 4;
    m->unk4 = next;
}

/* The memory card menu (D_006173F8): the entry list (states 0-2; the entry's type in D_00416040 is the next state, type
 * 0x10 toggles the option byte D_00439C92), then four flows that ask YES / NO, check the card status (errors in 6 / 11),
 * start a request and show its result: 5-9 request 0x1A (then Options_Apply and the option byte are reloaded), 10-15
 * request 0x12 (other messages when the card is unformatted), 17-20 request 0x10, 21-24 request 0x25. State 4 waits
 * `timer` frames, then goes to unk4; 26 / 27 leave the menu. Returns 0 once left, else 1. */
s32 func_00360F40(s32 held, s32 pressed) {
    McMenu *m;
    s32 r = 1;
    s32 st;

    m = &D_006173F8;
    m->held = held;
    m->pressed = pressed;
    st = func_0035E8D8();
    switch (D_006173F8.state) {
    case 0:
        func_00360A30(m, 0);
        m->state = 1;
    case 1:
        if (func_00360EE0(m) != 0) {
            break;
        }
        func_00360D50(m);
        D_00439C60[0x32] = m->unk18;
        if (D_00416040[m->sel].type == 0x10) {
            if (m->pressed & 0xA000) {
                func_003B4D28(0);
                func_00360A30(m, 0);
                D_00439C60[0x32] = D_00439C60[0x32] == 0;
                if (D_00439C60[0x32] != 0 && (st & ~0x8000)) {
                    m->state = D_00416040[m->sel].type;
                } else {
                    m->unk18 = D_00439C60[0x32];
                }
            }
        } else if (m->pressed & 0x8FC) {
            func_003B4D28(0);
            m->state = D_00416040[m->sel].type;
        }
        break;
    case 3:
        func_00360A30(m, 1);
        m->timer = 0x3C;
        m->state = 4;
        break;
    case 4:
        if (m->timer != 0) {
            m->timer--;
        } else {
            m->state = m->unk4;
        }
        break;
    case 5:
        func_00360C98(m, 0);
        m->state = 7;
        m->unk4 = 5;
    case 7:
        if (func_00360E38(st) != 0) {
            func_003B4D28(1);
            m->state = 6;
            break;
        }
        func_00360A30(m, 5);
        if (func_00360CA0(m) != 0) {
            if (func_00360D48(m) != 0) {
                m->state = 8;
            } else {
                m->state = 0;
            }
        }
        break;
    case 6:
        if (func_00360E38(st) == 0 && !(st & 0x8000)) {
            m->state = 5;
            break;
        }
        if (st & 1) {
            func_00360A30(m, 2);
        } else if (st & 8) {
            func_00360A30(m, 0xA);
        } else if (st & 4) {
            func_00360A30(m, 9);
        } else if (st & 0x400) {
            func_00360A30(m, 3);
        } else if (st & 0x200) {
            func_00360A30(m, 0xB);
        } else if (st & 0x4000) {
            func_00360A30(m, 4);
        }
        if (func_00360EE0(m) != 0) {
            break;
        }
        if (func_00360D50(m) != 0) {
            m->state = 0;
        } else if (m->pressed & 0x8FC) {
            m->state = 0;
        }
        break;
    case 8:
        func_00360A30(m, 6);
        if (func_0035E858() == 0) {
            func_00360F28(m, 9);
        }
        break;
    case 9:
        if (st & 0x8000) {
            break;
        }
        if (st == 0) {
            func_003B4D28(0);
            func_00360A30(m, 7);
            Options_Apply();
            m->unk18 = D_00439C92[0];
        } else {
            func_00360A30(m, 8);
            func_003B4D28(1);
        }
        m->state = 2;
        break;
    case 10:
    case 16:
        func_00360C98(m, 0);
        m->state = 0xC;
        m->unk4 = 0xA;
    case 12:
        if (func_00360DE0(st) != 0) {
            m->state = 0xB;
            func_003B4D28(1);
            break;
        }
        func_00360A30(m, 0xC);
        if (func_00360CA0(m) != 0) {
            if (func_00360D48(m) != 0) {
                if ((st & 4) || (st & 0x202) == 2) {
                    func_00360C98(m, 0);
                    m->state = 0xD;
                } else {
                    m->state = 0xE;
                }
            } else {
                m->state = 0;
            }
        }
        break;
    case 13:
        if (func_00360DE0(st) != 0) {
            m->state = 0xB;
            func_003B4D28(1);
            break;
        }
        if (st & 4) {
            func_00360A30(m, 0x13);
        } else {
            func_00360A30(m, 0xD);
        }
        if (func_00360CA0(m) != 0) {
            if (func_00360D48(m) != 0) {
                m->state = 0xE;
            } else {
                m->state = 0;
            }
        }
        break;
    case 11:
        if (func_00360DE0(st) == 0 && !(st & 0x8000)) {
            m->state = 0xA;
            break;
        }
        if (st & 1) {
            func_00360A30(m, 2);
        } else if (st & 8) {
            func_00360A30(m, 0x11);
        } else if (st & 0x400) {
            func_00360A30(m, 3);
        } else if (st & 0x100) {
            func_00360A30(m, 0x12);
        } else if (st & 0x4000) {
            func_00360A30(m, 4);
        }
        if (func_00360EE0(m) != 0) {
            break;
        }
    case 2:
        if (func_00360D50(m) != 0) {
            m->state = 0;
        } else if (m->pressed & 0x8FC) {
            m->state = 0;
        }
        break;
    case 14:
        D_005F6B40 = 0;
        if (st & 4) {
            D_005F6B40 = 1;
            func_00360A30(m, 0x14);
        } else {
            func_00360A30(m, 0xE);
        }
        if (func_0035E838() == 0) {
            func_00360F28(m, 0xF);
        }
        break;
    case 15:
        if (D_005F6B40 != 0 && !(st & 4)) {
            D_005F6B40 = 0;
            func_00360A30(m, 0xE);
        }
        if (st & 0x8000) {
            break;
        }
        if (st == 0) {
            m->unk18 = D_00439C92[0];
            func_00360A30(m, 0xF);
            func_003B4D28(0);
        } else {
            func_003B4D28(1);
            if (D_005F6B40 != 0) {
                func_00360A30(m, 0x15);
            } else {
                func_00360A30(m, 0x10);
            }
        }
        m->state = 2;
        break;
    case 17:
        func_00360C98(m, 0);
        m->state = 0x12;
    case 18:
        func_00360A30(m, 0x16);
        if (func_00360CA0(m) != 0) {
            if (func_00360D48(m) != 0) {
                m->state = 0x13;
            } else {
                m->state = 0;
            }
        }
        break;
    case 19:
        func_00360A30(m, 0x17);
        if (func_0035E898() == 0) {
            func_00360F28(m, 0x14);
        }
        break;
    case 20:
        if (st & 0x8000) {
            break;
        }
        func_00360A30(m, 0x18);
        func_003B4D28(0);
        m->state = 2;
        break;
    case 21:
        func_00360C98(m, 0);
        m->state = 0x16;
    case 22:
        func_00360A30(m, 0x19);
        if (func_00360CA0(m) != 0) {
            if (func_00360D48(m) != 0) {
                m->state = 0x17;
            } else {
                m->state = 0;
            }
        }
        break;
    case 23:
        func_00360A30(m, 0x1A);
        if (func_0035E8B8() == 0) {
            func_00360F28(m, 0x18);
        }
        break;
    case 24:
        if (st & 0x8000) {
            break;
        }
        func_00360A30(m, 0x1B);
        func_003B4D28(0);
        m->state = 2;
        break;
    case 26:
        m->state = 0x1B;
    case 27:
        r = 0;
        D_00439C92[0] = m->unk18;
        break;
    }
    func_00360670(m);
    func_00360B90(m);
    func_003608E0(m);
    return r;
}

void func_00361678(void) {
    McMenu *m = &D_006173F8;
    u8 c = D_00439C92[0];

    D_006173F8.state = 0;
    m->unk18 = c;
    m->sel = 0;
    m->yes = 0;
    func_00360A08(m, 0xDC);
    func_003608B8(m);
}

s32 func_003616C8(void) {
    s32 r = 0;

    if ((u32)(D_006173F8.state - 1) < 2 || D_006173F8.state == 0x1B) {
        r = 1;
    }
    return r;
}

void func_003616F8(void) {
    D_006173F8.state = 0;
    func_00360A08(&D_006173F8, 0xB4);
}

/* Memory card error screen: on a card error shows the message for the error bit and waits for a button; request
 * 0x1A retries. Returns 0 once state 6 is reached, else 1. */
s32 func_00361710(void) {
    McMenu *m;
    s32 r = 1;
    s32 st = func_0035E8D8();
    s32 e;

    m = &D_006173F8;
    switch (D_006173F8.state) {
    case 0:
        if (st & 0x8000) {
            break;
        }
        e = func_00360E88(st);
        if (st & 1) {
            e = 1;
        }
        switch (e) {
        case 0:
            m->state = 1;
            break;
        case 1:
            m->state = 6;
            break;
        default:
            m->state = 3;
            break;
        }
        break;
    case 1:
        if (func_0035E858() == 0) {
            m->state = 2;
        }
        break;
    case 2:
        if (st & 0x8000) {
            break;
        }
        if (func_00360E88(st) == 0) {
            m->state = 6;
        } else {
            m->state = 3;
        }
        break;
    case 3:
        if (func_00360E88(st) < 2 && !(st & 0x8000)) {
            m->state = 0;
            break;
        }
        if (st & 1) {
            func_00360A30(m, 0x1C);
        } else if (st & 8) {
            func_00360A30(m, 0x1D);
        } else if (st & 0x100) {
            func_00360A30(m, 0x1E);
        } else if (st & 0x400) {
            func_00360A30(m, 0x1F);
        } else if (st & 0x4000) {
            func_00360A30(m, 0x20);
        }
        if (st & 1) {
            if (D_005F5CDC & 0x800) {
                m->state = 6;
            }
        } else if (D_005F5CDC & 0x10) {
            m->state = 4;
        } else if (D_005F5CDC & 0x40) {
            m->state = 6;
        }
        break;
    case 4:
        func_00360A30(m, 1);
        if (func_0035E858() == 0) {
            m->timer = 0x3C;
            m->state = 5;
        }
        break;
    case 5:
        if (m->timer != 0) {
            m->timer--;
        } else {
            m->state = 0;
        }
        break;
    case 6:
        r = 0;
        break;
    }
    if (!(st & 0x8000)) {
        func_0035E878();
    }
    func_00360B90(m);
    return r;
}

void func_00361968(void) {
    D_0061740E[0] = 1;
}

/* Memory card check: waits for the card status, starts request 0x12, shows the "no card" message (state 3) for
 * `timer` frames or until a button, then closes it. Returns 1 while busy, 2 while a message shows, 0 when done. */
s32 func_00361978(void) {
    McMenu *m;
    s32 r = 1;
    s32 st = func_0035E8D8();

    m = &D_006173F8;
    switch (D_006173F8.state) {
    case 0:
        if (st & 0x8000) {
            break;
        }
        if (st != 0) {
            m->state = 3;
            break;
        }
        m->state = 1;
        break;
    case 1:
        if (func_0035E838() != 0) {
            break;
        }
        m->state = 2;
        break;
    case 2:
        if (st & 0x8000) {
            break;
        }
        if (st == 0) {
            m->state = 5;
            break;
        }
        m->state = 3;
        break;
    case 3:
        r = 2;
        func_00360B90(m);
        if (!(D_005F5CDC & 0x8FC) && m->timer != 0) {
            m->timer--;
            break;
        }
        func_00360A30(m, 0);
        m->state = 4;
        break;
    case 4:
        r = 2;
        func_00360B90(m);
        if (m->unk24 != 0) {
            break;
        }
        m->state = 5;
    case 5:
        r = 0;
        break;
    }
    return r;
}

void func_00361AA8(void) {
    McMenu *m = &D_006173F8;

    if (m->unk16 != 0 && D_00439C92[0] != 0) {
        D_006173F8.state = 0;
    } else {
        m->state = 5;
    }
    m->unk16 = 0;
    m->timer = 0x12C;
    func_00360A08(m, 0xB4);
    func_00360A30(m, 0x21);
}

void func_00361B18(void) {
    D_0061740E[0] = 0;
}

s32 func_00361B28(void) {
    return (func_0035E920() & ~0x8002) == 0;
}

s32 func_00361B58(void) {
    return (func_0035E8D8() & ~0x8002) == 0;
}

/* Adds a GS image transfer to a VIF1 packet: BITBLTBUF / TRXPOS / TRXREG / TRXDIR through DIRECTHL, then the image as
 * DMA references of at most 0x7FFF quadwords, each behind an IMAGE GIF tag (D_00416190). */
void Gfx_PkAddImage(Vif1Packet *pk, u16 dbp, u8 dpsm, u16 dbw, u8 *data, u32 qwc, s32 x, s32 y, u32 w, s32 h) {
    u32 n;

    sceVif1PkCnt(pk, 0);
    sceVif1PkOpenDirectHLCode(pk, 0);
    pk->pCurrent[-2] = 0x10000000;
    sceVif1PkOpenGifTag(pk, D_004FC350[0]);
    sceVif1PkAddGsAD(pk, 0x50, ((u64)dbp << 32) | ((u64)dbw << 48) | ((u64)dpsm << 56));
    sceVif1PkAddGsAD(pk, 0x51, ((u64)x << 32) | ((u64)y << 48));
    sceVif1PkAddGsAD(pk, 0x52, (u64)w | ((u64)h << 32));
    sceVif1PkAddGsAD(pk, 0x53, 0);
    sceVif1PkCloseGifTag(pk);
    sceVif1PkCloseDirectHLCode(pk);
    while (qwc != 0) {
        n = qwc;
        if (qwc > 0x7FFF) {
            n = 0x7FFF;
        }
        D_00416190.d[0] = (D_00416190.d[0] & ~0x7FFF) | (n & 0x7FFF);
        qwc -= n;
        sceVif1PkCnt(pk, 0);
        sceVif1PkOpenDirectHLCode(pk, 0);
        sceVif1PkOpenGifTag(pk, D_00416190.q);
        sceVif1PkCloseGifTag(pk);
        sceVif1PkCloseDirectHLCode(pk);
        sceVif1PkRef(pk, data, n, 0, n | 0x51000000, 0);
        data += n << 4;
    }
}

RODATA_ALIGN16();
INCLUDE_RODATA("asm/nonmatchings/font_tex", D_004FC350);

void func_00361D50(s32 i, u16 size, void *data) {
    TexSlot *s = &D_006E3550[i];

    s->size = size;
    s->data = data;
}

/* Sorts the five slot indexes in `order` by size, largest first (bubble sort). */
void func_00361D70(s32 *order, TexSlot *slots) {
    s32 i;
    s32 j;
    s32 a;
    s32 b;

    for (i = 4; i >= 0; i--) {
        order[i] = i;
    }
    for (j = 0; j < 4; j++) {
        for (i = 3; i >= j; i--) {
            a = order[i];
            b = order[i + 1];
            if (slots[a].size < slots[b].size) {
                order[i] = b;
                order[i + 1] = a;
            }
        }
    }
}

/* NON_MATCHING: 30 lines: same blocks, other registers (the original keeps order in a0 and the flag in t2) */
#if 0
/* Leaves out the largest images (from the end of `order`) until the rest fits in 0x1640 blocks. */
void func_00361E20(s32 *order, TexSlot *slots) {
    u32 total = 0;
    u32 room;
    u32 left;
    s32 i;
    s32 over;
    TexSlot *s;

    for (i = 4; i >= 0; i--) {
        total += slots[i].size;
    }
    room = 0x1640;
    over = room < total;
    for (i = 4; i >= 0; i--) {
        if (over) {
            s = &slots[order[i]];
            if (s->size != 0) {
                left = 0x1840 - s->size;
                total -= s->size;
                if (left < room) {
                    room = left;
                }
                s->skip = 1;
                over = room < total;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/font_tex", func_00361E20);
#endif

/* NON_MATCHING: 29 lines: register allocation only (the original keeps the slot size in a2 in all three loops and
 * data / &cur in s0 / s3, swapped here) */
#if 0
/* Places the five images of D_006E3550 in VRAM from block 0x2300 (largest first, leaving out what does not fit),
 * writing each image's block to `blocks` (the sub-images of slot 4 to `subBlocks`), builds their upload packets one
 * after the other from D_006D7500 into D_006E3500, then streams each packet through the scratchpad to the GIF. */
void func_00361EA8(s32 *blocks, s32 *subBlocks) {
    s32 order[5];
    u8 *cur;
    u8 *end;
    u8 *start;
    TexSlot *s;
    TexUpload *spr;
    void *data;
    s32 vram;
    s32 i;
    s32 j;
    s32 idx;
    u16 size;
    u32 n;
    u32 max;
    u32 count;
    u16 skip;
    SprTag *tag;

    vram = 0x2300;
    func_00361D70(order, D_006E3550);
    func_00361E20(order, D_006E3550);
    for (j = 0; j < 5; j++) {
        i = order[j];
        s = &D_006E3550[i];
        size = s->size;
        if (size != 0) {
            blocks[i] = vram;
            if (s->skip == 0) {
                vram += size;
            }
        }
    }
    end = D_00617500;
    cur = D_006D7500;
    for (i = 4; i >= 0; i--) {
        size = D_006E3550[i].size;
        if (size != 0) {
            skip = D_006E3550[i].skip;
            vram = blocks[i];
            start = cur;
            data = D_006E3550[i].data;
            if (i == 4) {
                count = func_00364550(data);
                for (j = 0; j < count; j++) {
                    if (func_00364558(j, data) != 0) {
                        subBlocks[j] = vram;
                        vram = func_003623B8(func_00364528(j, data), vram, &cur, skip, &end);
                    }
                }
            } else {
                func_003623B8(data, vram, &cur, skip, &end);
            }
            Dma_AddRefTag(&cur, 0x18);
            D_006E3500[i].qwc = (s32)(cur - start) >> 4;
            D_006E3500[i].unk2 = 0;
            D_006E3500[i].addr = start;
        }
    }
    FlushCache(0);
    spr = (TexUpload *)Spr_Setup(2, 0x10);
    max = *(u16 *)0x7000000C - 0x10;
    max = (max >> 7) << 3;
    for (i = 4; i >= 0; i--) {
        size = D_006E3550[i].size;
        if (D_006E3550[i].skip == 0 && size != 0) {
            *spr = D_006E3500[i];
            while (spr->qwc != 0) {
                tag = *(SprTag **)0x70000004;
                n = spr->qwc;
                if (n > max) {
                    n = max;
                }
                func_00343128((u32)tag, spr->addr, n);
                Dma_Wait((volatile u32 *)0x1000D400);
                spr->qwc -= n;
                spr->addr += n << 4;
                tag = (SprTag *)((u8 *)tag + (n << 4));
                tag->q = 0;
                tag->h[1] = 0x7000;
                func_0035E6B8(func_00342FB0());
            }
        }
    }
    Dma_Wait((volatile u32 *)0x1000A000);
}
#else
INCLUDE_ASM("asm/nonmatchings/font_tex", func_00361EA8);
#endif
