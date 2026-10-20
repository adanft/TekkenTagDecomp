/* 0x35E808-0x3608B8: the memory card service: requests, the libmc call sequence run by Mc_Update every frame,
 * the save data check. */
#include "common.h"
#include "memcard.h"

extern McState gMc;
extern s32 D_0061737C[];

s32 sceMcInit(void);
s32 sceMcSync(s32 mode, s32 *cmd, s32 *result);
s32 sceMcOpen(s32 port, s32 slot, s32 name, s32 mode);
s32 sceMcClose(s32 fd);
s32 sceMcMkdir(s32 port, s32 slot, s32 name);
s32 func_00406D08(s32 port, s32 slot, s32 *type, s32 *free, s32 *format);
s32 func_004069B0(s32 fd, s32 buf, s32 size);
s32 func_00406A88(s32 fd, s32 buf, s32 size);
s32 func_00407138(s32 port, s32 slot, s32 name);
s32 func_00406E50(s32 port, s32 slot, s32 a, s32 b, s32 c, s32 d);
s32 func_00406FC0(s32 port, s32 slot, s32 name, s32 x);
s32 func_004070A8(s32 port, s32 slot);
s32 func_004074F8(s32 port, s32 slot);
s32 Mc_Request(s32 op);
s32 func_0035E878(void);
s32 func_0035E8D8(void);
u8 *func_0035C528(u8 *dst, const u8 *src, s32 n);
s32 FlushCache(s32 mode);
s32 Text_Printf(const char *fmt, ...);
void func_003B4D20(const char *label, s32 y, s32 z);
void func_003BB890(s32 a, s32 x, s32 y, s32 b);
s32 func_003B1468(void);
void func_003DEB80(s32 a);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 z, u32 color);
s32 func_003E08D0(s32 a);
void func_003E5268(s32 x, s32 y, const char *text, u32 color);

/* The entries of the memory card menu (.data). */
typedef struct {
    /* 0x0 */ s32 type;
    /* 0x4 */ const char *help;
    /* 0x8 */ const char *label;
} McMenuItem;

extern McMenuItem D_00416040[];
extern const char *D_00416070[];

typedef struct {
    /* 0x00 */ u8 pad0[8];
    /* 0x08 */ s32 sel;
    /* 0x0C */ u8 padC[0xC];
    /* 0x18 */ u8 unk18;
} Unk360670;
void Ui_DrawWindow(s32 x, s32 y, s32 w, s32 h, s32 z, u32 c0, u32 c1);

/* The blocks of the save data (system, options, play counters, records). */
extern u8 D_00439C60[];
extern u8 D_00440D58[];
extern u8 D_00415D60[];
extern u8 D_0046BC70[];

/* The save file names (icons, screenshots), reached from .data; read from the original executable. */
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FBFC0);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FBFD0);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FBFE0);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FBFF0);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC000);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC010);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC020);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC030);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC040);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC050);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC060);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC070);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC080);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC090);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC0A0);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC0B0);

/* Requests operation `op`; refused (1) while another one is pending. */
s32 Mc_Request(s32 op) {
    if (gMc.request == 0) {
        gMc.request = op;
        gMc.unk10 = 1;
        return 0;
    }
    return 1;
}

s32 func_0035E838(void) {
    return Mc_Request(0x12);
}

s32 func_0035E858(void) {
    return Mc_Request(0x1A);
}

s32 func_0035E878(void) {
    return Mc_Request(3);
}

s32 func_0035E898(void) {
    return Mc_Request(0x10);
}

s32 func_0035E8B8(void) {
    return Mc_Request(0x25);
}

s32 func_0035E8D8(void) {
    u32 flags;
    s32 busy;

    busy = gMc.request != 0;
    flags = gMc.flags;
    flags &= 0xFFFFCFFF;
    if (gMc.format == 0) {
        flags |= 4;
    }
    if (busy || gMc.unkCC != 0) {
        flags |= 0x8000;
    }
    return flags;
}

s32 func_0035E920(void) {
    s32 flags = gMc.flags;

    if (gMc.format == 0) {
        flags |= 4;
    }
    if (gMc.request != 0 || gMc.unkCC != 0) {
        flags |= 0x8000;
    }
    return flags;
}

s32 func_0035E958(s32 a, s32 b, s32 c) {
    McState *mc = &gMc;
    s32 r = Mc_Request(0x1D);

    if (r == 0) {
        mc->unkB0 = a;
        mc->unkB4 = c;
        mc->unkB8 = b;
    }
    return r;
}

s32 func_0035E9B8(s32 a, s32 b) {
    McState *mc = &gMc;
    s32 r = Mc_Request(0x20);

    if (r == 0) {
        mc->unkB0 = a;
        mc->unkB8 = b;
    }
    return r;
}

s32 func_0035EA08(s32 a) {
    McState *mc = &gMc;
    s32 r = Mc_Request(0x23);

    if (r == 0) {
        mc->unkB0 = a;
    }
    return r;
}

/* Index of the first clear bit of D_0061737C (0-11), or -1. */
s32 func_0035EA48(void) {
    s32 bits = D_0061737C[0];
    s32 i;

    for (i = 0; i < 12; i++) {
        if (!((bits >> i) & 1)) {
            return i;
        }
    }
    return -1;
}

s32 func_0035EA80(void) {
    return D_0061737C[0];
}

s32 func_0035EA90(void) {
    s32 flags = func_0035E8D8();

    if (flags & 0x8000) {
        return 0;
    }
    if (flags & 1) {
        return 9;
    }
    if (flags & 8) {
        return 8;
    }
    if (flags & 4) {
        return 7;
    }
    if (flags & 0x400) {
        return 6;
    }
    if (flags & 0x100) {
        return 5;
    }
    if (flags & 0x200) {
        return 4;
    }
    if (flags & 0x4000) {
        return 3;
    }
    if (flags & 2) {
        return 2;
    }
    return 1;
}

void func_0035EB20(McState *mc, s32 a, s32 b, s32 c) {
    mc->unk1C = a;
    mc->unk48 = b;
    mc->op = 0x11;
    mc->unk10 = c;
}

void func_0035EB38(McState *mc, s32 name, s32 mode, s32 buf, s32 size, s32 x) {
    mc->unk18 = name;
    mc->unk28 = mode;
    mc->unk20 = buf;
    mc->unk24 = size;
    mc->op = 8;
    mc->unk10 = x;
}

void func_0035EB58(McState *mc, s32 name, s32 mode, s32 buf, s32 size, s32 x) {
    mc->unk18 = name;
    mc->unk28 = mode;
    mc->unk20 = buf;
    mc->unk24 = size;
    mc->op = 0xB;
    mc->unk10 = x;
}

void func_0035EB78(McState *mc, s32 name, s32 x) {
    mc->unk18 = name;
    mc->op = 0xE;
    mc->unk10 = x;
}

void func_0035EB90(McState *mc, s32 x) {
    mc->unk10 = x;
    mc->op = 0xF;
}

/* Packs the save data into `buf`: a checksum byte, then the four blocks, scrambled. */
void func_0035EBA0(u8 *buf) {
    u8 *p;
    u8 sum;
    u8 key;
    u32 i;

    func_0035C528(buf + 1, D_00439C60, 0x68);
    func_0035C528(buf + 0x69, D_00440D58, 0xC8);
    func_0035C528(buf + 0x131, D_00415D60, 0x116);
    func_0035C528(buf + 0x247, D_0046BC70, 0x206);
    sum = 0;
    p = buf + 1;
    for (i = 1; i < 0x480; i++, p++) {
        sum += *p;
    }
    sum = -sum;
    buf[0] = sum;
    key = sum;
    for (i = 1, p = buf + 1; i < 0x480; i++, p++) {
        key = *p + (key * 5 + 1);
        *p = key;
    }
    FlushCache(0);
}

/* Unscrambles the save data in `buf` and, when its checksum is right, copies the four blocks back (1). */
s32 func_0035EC98(u8 *buf) {
    s32 ok = 0;
    u8 *p;
    u8 sum;
    u8 prev;
    s32 c;
    u8 v;
    u32 i;

    sum = buf[0];
    prev = sum;
    for (i = 1, p = buf + 1; i < 0x480; i++, p++) {
        c = *p;
        v = c - (prev * 5 + 1);
        prev = c;
        *p = v;
        sum += v;
    }
    if (sum == 0) {
        func_0035C528(D_00439C60, buf + 1, 0x68);
        func_0035C528(D_00440D58, buf + 0x69, 0xC8);
        func_0035C528(D_00415D60, buf + 0x131, 0x116);
        func_0035C528(D_0046BC70, buf + 0x247, 0x206);
        ok = 1;
    }
    return ok;
}

void func_0035ED68(McState *mc) {
    s32 flags = 0;
    if (mc->unk5C == 0) {
        flags |= 1;
    }
    if (mc->unk60 != 0) {
        flags |= 2;
    }
    if (mc->type != 2) {
        flags |= 8;
    }
    if (mc->unk70 != 0) {
        flags |= 0x4000;
    }
    if (mc->unk64 != 0) {
        flags |= 0x100;
    }
    if (mc->unk68 != 0) {
        flags |= 0x400;
    }
    if (mc->unk6C == 0) {
        flags |= 0x200;
    }
    if (mc->free < 100) {
        flags |= 0x2000;
    }
    if (mc->unk78 >= 12) {
        flags |= 0x1000;
    }
    mc->flags = flags;
}

s32 func_0035EDE8(McState *mc) {
    s32 r = func_00406D08(mc->port, mc->slot, &mc->type, &mc->free, &mc->format);

    if (r == 0) {
        mc->phase = 1;
    }
    return r;
}

s32 func_0035EE38(McState *mc) {
    s32 r = sceMcOpen(mc->port, mc->slot, mc->unk18, mc->unk28);

    if (r == 0) {
        mc->phase = 2;
    }
    return r;
}

s32 func_0035EE80(McState *mc) {
    s32 r = sceMcClose(mc->fd);

    if (r == 0) {
        mc->phase = 3;
    }
    return r;
}

s32 func_0035EEC0(McState *mc) {
    s32 r = func_004069B0(mc->fd, mc->unk20, mc->unk24);

    if (r == 0) {
        mc->phase = 5;
    }
    return r;
}

s32 func_0035EF08(McState *mc) {
    s32 r = func_00406A88(mc->fd, mc->unk20, mc->unk24);

    if (r == 0) {
        mc->phase = 6;
    }
    return r;
}

s32 func_0035EF50(McState *mc) {
    s32 r = func_00407138(mc->port, mc->slot, mc->unk18);

    if (r == 0) {
        mc->phase = 0xF;
    }
    return r;
}

s32 func_0035EF98(McState *mc, s32 a, s32 b, s32 c, s32 d) {
    s32 r = func_00406E50(mc->port, mc->slot, a, b, c, d);

    if (r == 0) {
        mc->phase = 0xD;
        mc->unk28 = b;
        mc->unk34 = c;
        mc->unk30 = d;
        mc->unk1C = a;
    }
    return r;
}

s32 func_0035F028(McState *mc, s32 name) {
    s32 r = sceMcMkdir(mc->port, mc->slot, name);

    if (r == 0) {
        mc->unk1C = name;
        mc->phase = 0xB;
    }
    return r;
}

s32 func_0035F080(McState *mc) {
    s32 r = func_00406FC0(mc->port, mc->slot, mc->unk1C, mc->unk48);

    if (r == 0) {
        mc->phase = 0xC;
    }
    return r;
}

s32 func_0035F0C8(McState *mc) {
    s32 r = func_004070A8(mc->port, mc->slot);

    if (r == 0) {
        mc->phase = 0x10;
    }
    return r;
}

s32 func_0035F108(McState *mc) {
    s32 r = func_004074F8(mc->port, mc->slot);

    if (r == 0) {
        mc->phase = 0x11;
    }
    return r;
}

/* 1 when the `n` bytes at `a` and `b` differ. */
s32 func_0035F148(u8 *a, u8 *b, u32 n) {
    u32 i;

    for (i = 0; i < n; i++, a++, b++) {
        if (*a != *b) {
            return 1;
        }
    }
    return 0;
}

/* Polls the libmc call in flight (sceMcSync) and records its result. */
s32 func_0035F180(McState *mc) {
    s32 r;

    if (mc->phase == 0) {
        return -1;
    }
    r = sceMcSync(1, &mc->phase, &mc->unk58);
    if (r < -1) {
        return r;
    }
    if (r <= 0) {
        return r;
    }
    if (r != 1) {
        return r;
    }
    mc->unk5C = r;
    if (mc->unk58 >= 0) {
        switch (mc->phase) {
        case 1:
            if (mc->format != 0) {
                break;
            }
            mc->unk58 = -2;
            break;
        case 2:
            mc->fd = mc->unk58;
            break;
        case 13:
            mc->unk38 = mc->unk58;
            break;
        case 16:
            mc->format = 1;
            break;
        case 17:
            mc->format = 0;
            break;
        case 18:
            break;
        }
    } else {
        mc->unk64 = 0;
        mc->unk68 = 0;
        mc->unk6C = 0;
        mc->unk14 = r;
        switch (mc->unk58) {
        case -1:
            if (mc->phase == 0x10) {
                mc->unk5C = 0;
            } else {
                mc->unk60 = r;
                mc->format = r;
            }
            break;
        case -2:
            mc->unk60 = r;
            mc->format = 0;
            break;
        case -7:
        case -6:
        case -5:
        case -4:
        case -3:
            break;
        default:
            if (mc->phase != 1 || mc->type != 1) {
                mc->unk5C = 0;
            }
            break;
        }
    }
    mc->phase = 0;
    return r;
}

s32 func_0035F2D8(McState *mc) {
    u32 flags = mc->flags;

    if (mc->format == 0) {
        flags |= 4;
    }
    if (flags & 1) {
        return 9;
    }
    if (flags & 8) {
        return 8;
    }
    if (flags & 4) {
        return 7;
    }
    if (flags & 0x400) {
        return 6;
    }
    if (flags & 0x100) {
        return 5;
    }
    if (flags & 0x200) {
        return 4;
    }
    if (flags & 0x4000) {
        return 3;
    }
    return (flags & 2) ? 2 : 1;
}

/* Not written in C yet (T13 batch 3): 1064 instructions, the libmc call sequence of every request. */
INCLUDE_ASM("asm/nonmatchings/memcard", Mc_Update);

void Mc_Init(void) {
    McState *state = &gMc;
    McState *mc;
    s32 i;

    sceMcInit();
    mc = state;
    mc->port = 0;
    mc->unk14 = 1;
    mc->slot = 0;
    mc->phase = 0;
    mc->op = 0;
    mc->request = 0;
    mc->unkCC = 0;
    mc->unk58 = 0;
    mc->unk5C = 0;
    mc->type = 0;
    mc->free = 0;
    mc->format = 0;
    mc->flags = 0;
    mc->unkD0 = 0;
    for (i = 0; i < 16; i++) {
        mc->unkD4[i] = 0;
        mc->unkE4[i] = 0;
    }
    mc->unk78 = 0;
    mc->unk7C = 0;
    func_0035E878();
}

/* a + (b - a) * t / 256 (wrong argument order: (b, a, t)). */
s32 func_003604B0(s32 b, s32 a, s32 t) {
    return ((b - a) * t >> 8) + a;
}

/* Sets both halves of a 10-word record to (a, b, c, d), word 4 to e. */
void func_003604C8(s32 *v, s32 a, s32 b, s32 c, s32 d, s32 e) {
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
    v[6] = a;
    v[7] = b;
    v[8] = c;
    v[9] = d;
    v[4] = e;
}

/* Sets words 6-9 to (a, b, c, d) and word 14 to e, keeping the previous words 0-4 in 10-13 and 15. */
void func_003604F0(s32 *v, s32 a, s32 b, s32 c, s32 d, s32 e) {
    v[6] = a;
    v[7] = b;
    v[8] = c;
    v[9] = d;
    v[14] = e;
    v[10] = v[0];
    v[11] = v[1];
    v[12] = v[2];
    v[13] = v[3];
    v[15] = v[4];
}

/* Interpolates words 0-4 between the previous (10-13, 15) and the new values (6-9, 14) by word 5 (0-256), the
 * color byte by byte, and draws the rectangle when it has a size. */
void func_00360530(s32 *v) {
    s32 t = v[5];
    u32 from;
    u32 to;
    u32 color = 0;
    s32 i;

    v[0] = func_003604B0(v[6], v[10], t);
    v[1] = func_003604B0(v[7], v[11], t);
    v[2] = func_003604B0(v[8], v[12], t);
    v[3] = func_003604B0(v[9], v[13], t);
    to = v[14];
    from = v[15];
    for (i = 0; i < 4; i++) {
        color |= (func_003604B0((to >> (i * 8)) & 0xFF, (from >> (i * 8)) & 0xFF, t) & 0xFF) << (i * 8);
    }
    v[4] = color;
    if (v[2] != 0 && v[3] != 0) {
        Ui_DrawWindow(v[0], v[1], v[2], v[3], 0xFFFF, color, color);
    }
}

/* Save titles, reached from .data. */
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC220);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC230);
INCLUDE_RODATA("asm/nonmatchings/memcard", D_004FC240);

/* Draws the memory card menu: the four entries (the selected one highlighted), the title and the help line. */
void func_00360670(Unk360670 *m) {
    const char *help;
    u32 i;
    s32 y;
    s32 color;

    y = 0x5A;
    for (i = 0; i < 4; i++, y += 0x1E) {
        color = 5;
        if (i == m->sel) {
            color = 0xB;
            func_003B4D20(D_00416040[i].label, y, 0);
        }
        Text_Printf("%H%V%F%S%c%s", 0x28, y, 0xA, 4, color, D_00416040[i].label);
        if (D_00416040[i].type == 0x10) {
            s32 type = D_00416040[i].type;

            color = m->unk18 ? 0xC : 0x10;
            Text_Printf("%H%V%F%S%c%s", 0xD7, y, 0xA, 4, color, D_00416070[m->unk18]);
            if (D_00416040[m->sel].type == type) {
                func_003BB890(1, 0x113, y, 5);
            }
        }
    }
    Text_Printf("%H%V%F%S%cMEMORY CARD(PS2)", 0x97, 0x28, 0xA, 5, 0x13);
    if (func_003B1468() != 0) {
        help = D_00416040[m->sel].help;
        func_003DEB80(0);
        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
        if (help != NULL) {
            if (D_00416040[m->sel].type == 0x10) {
                func_003E08D0(m->unk18);
            }
            func_003E5268(0x1E, 0x186, help, 0x60606080);
        }
    }
}
