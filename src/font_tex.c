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
s32 func_003E5020(s32 id);
s32 func_003E5140(s32 id);
void func_003E5268(s32 x, s32 y, s32 id, s32 color);
void func_003DEB80(s32 a);

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
            tw = func_003E5020(first + i);
            if (maxw < tw) {
                maxw = tw;
            }
            rows += func_003E5140(first + i);
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
                func_003E5268(0x140 - (func_003E5020(first + i) >> 1), m->win.y + i * 0x12 + 8, first + i, 0x60606080);
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

INCLUDE_ASM("asm/nonmatchings/font_tex", func_00360F40);

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

INCLUDE_ASM("asm/nonmatchings/font_tex", func_00361EA8);
