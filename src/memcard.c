/* 0x35E808-0x3608B8: the memory card service: requests, the libmc call sequence run by Mc_Update every frame,
 * the save data check. */
#include "common.h"
#include "memcard.h"

extern McState gMc;
extern s32 D_0061737C[];

s32 sceMcInit(void);
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

#if 0
/* NON_MATCHING: register allocation only (the flags in $a1, not $v1). */
s32 func_0035E8D8(void) {
    u32 flags = gMc.flags & 0xFFFFCFFF;

    if (gMc.format == 0) {
        flags |= 4;
    }
    if (gMc.request != 0 || gMc.unkCC != 0) {
        flags |= 0x8000;
    }
    return flags;
}
#else
INCLUDE_ASM("asm/nonmatchings/memcard", func_0035E8D8);
#endif

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

INCLUDE_ASM("asm/nonmatchings/memcard", func_0035EBA0);

INCLUDE_ASM("asm/nonmatchings/memcard", func_0035EC98);

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

INCLUDE_ASM("asm/nonmatchings/memcard", func_0035F180);

INCLUDE_ASM("asm/nonmatchings/memcard", func_0035F2D8);

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

INCLUDE_ASM("asm/nonmatchings/memcard", func_00360530);

INCLUDE_ASM("asm/nonmatchings/memcard", func_00360670);
