/*
 * VU0 micro mode: the VIF0 side that loads the game's VU0 microcode (MPG) and an interpreter that runs it when the
 * EE starts a program (vcallms, port/src/vu0_pc.c). The game's four skinning programs (src/vu0/skin_*.vsm) run
 * unchanged on it.
 *
 * Structure after the reference project's VU1 interpreter (port/src/gs/gs_vu1.c of Tenkaichi3Decomp): upper and
 * lower halves decoded per instruction, the upper result committed after the lower half read its operands,
 * branches and the end bit taking effect after the following instruction, the Q register's latency (div / sqrt 7,
 * rsqrt 13 steps) and the four-step delay of the clipping and MAC flags. Unlike that file the arithmetic is the EE
 * float model's vector unit (softfloat_ps2_inl.h: no host floating point): the results feed the game's DMA
 * packets, and the port keeps one number model. Not modelled: stalls (the programs are hand-scheduled), the MAC
 * flag's underflow / overflow bits and the status flag (no program reads them). Instructions outside the set the
 * game's programs use are counted and reported once.
 *
 * VU0 memory is the EE's view of it (plat_mem.c): micro memory at 0x11000000 (4 KB), data memory at 0x11004000
 * (4 KB). The registers are shared with macro mode: gPortVu0Vf (plat_ee.c) and gPortVu0Vi.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "softfloat_ps2_inl.h"

extern float gPortVu0Vf[32][4]; /* plat_ee.c */
uint16_t gPortVu0Vi[16];         /* vi0..vi15 (ctc2 / cfc2 in macro mode) */
uint32_t gPortVu0Acc[4];         /* ACC, shared with macro mode (vu0_pc.c) */
uint32_t gPortVu0Q = 0x3F800000u; /* Q */

#define MICRO ((uint8_t *)0x11000000u)
#define DATA ((uint8_t *)0x11004000u)
#define VF(r) ((uint32_t *)gPortVu0Vf[(r)])

typedef struct FlagStep {
    uint32_t cycle, value;
} FlagStep;

static struct {
    uint32_t i, qPending;
    uint32_t qReady, cycle;
    uint32_t clip, clipLatest;
    FlagStep clipQueue[8], macQueue[8];
    int clipHead, clipCount, macHead, macCount;
    uint32_t mac;
} vu;

static unsigned sUnknown[2][128];

/* ---------------------------------------------------------------------------------------------- VIF0 */

static struct {
    uint32_t pending;  /* data words still owed to the current code */
    uint32_t kind;     /* 0 none, 1 MPG, 2 skipped data */
    uint32_t mpgAddr;  /* byte address in micro memory of the next MPG word */
    uint32_t mask;
} vif;

/* The VIF0 codes and data of a DMA block, in order (plat_hw.c): STCYCL / STMOD / STMASK / MARK / flushes are
   accepted, MPG loads micro memory; anything with data the port does not use yet (STROW, STCOL, UNPACK) has its
   data skipped and is reported once. */
void Port_Vif0Feed(const uint32_t *w, uint32_t n) {
    static int reported;
    uint32_t k;

    for (k = 0; k < n; k++) {
        uint32_t code, cmd, num, imm;

        if (vif.pending) {
            if (vif.kind == 1) {
                memcpy(MICRO + (vif.mpgAddr & 0xFFC), &w[k], 4);
                vif.mpgAddr += 4;
            }
            vif.pending--;
            continue;
        }
        code = w[k];
        cmd = (code >> 24) & 0x7F;
        num = (code >> 16) & 0xFF;
        imm = code & 0xFFFF;
        if (cmd == 0x4A) { /* MPG: num instructions (0 = 256) at imm * 8 */
            vif.kind = 1;
            vif.mpgAddr = (imm * 8) & 0xFFF;
            vif.pending = (num ? num : 256) * 2;
        } else if (cmd == 0x20) { /* STMASK: one word */
            vif.kind = 2;
            vif.pending = 1;
        } else if (cmd == 0x30 || cmd == 0x31) { /* STROW / STCOL: four words */
            vif.kind = 2;
            vif.pending = 4;
        } else if (cmd >= 0x60) { /* UNPACK: not used by the game on VIF0 so far */
            uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, cnt = num ? num : 256;
            uint32_t bits = (32u >> vl) * (vn + 1);
            vif.kind = 2;
            vif.pending = (cnt * bits + 31) / 32;
            if (!reported++) {
                fprintf(stderr, "ttt: vif0: UNPACK 0x%08x skipped (not implemented)\n", code);
            }
        } else if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) { /* MSCAL / MSCALF / MSCNT */
            if (!reported++) {
                fprintf(stderr, "ttt: vif0: program start 0x%08x ignored (not implemented)\n", code);
            }
        }
        /* NOP, STCYCL, OFFSET, BASE, ITOP, STMOD, MSKPATH3, MARK, FLUSH*: nothing to do here */
    }
}

/* ---------------------------------------------------------------------------------------------- arithmetic */

static uint32_t vadd(uint32_t a, uint32_t b) { return sf_add_core(a, b, 0); }
static uint32_t vsub(uint32_t a, uint32_t b) { return sf_add_core(a, b ^ SF_SIGN, 0); }
static uint32_t vmul(uint32_t a, uint32_t b) { return sf_mul_core(a, b, 0); }
static uint32_t vmax(uint32_t a, uint32_t b) { return sf_key(a) >= sf_key(b) ? a : b; }
static uint32_t vmin(uint32_t a, uint32_t b) { return sf_key(a) <= sf_key(b) ? a : b; }

/* vftoiN: a * 2^frac cut toward zero, saturating. */
static uint32_t ftoi(uint32_t a, int frac) {
    uint32_t m = SF_MANT(a), v;
    int e = (int)SF_EXP(a) - 127 + frac;

    if (SF_EXP(a) == 0 || e < 0) {
        return 0;
    }
    if (e >= 31) {
        return (a & SF_SIGN) ? 0x80000000u : 0x7FFFFFFFu;
    }
    v = e >= 23 ? m << (e - 23) : m >> (23 - e);
    return (a & SF_SIGN) ? 0u - v : v;
}

/* vitofN: the integer cut to 24 bits, / 2^frac. */
static uint32_t itof(uint32_t v, int frac) {
    uint32_t f = sf_itof((int32_t)v);

    if (f == 0 || frac == 0) {
        return f;
    }
    return SF_EXP(f) > (uint32_t)frac ? f - ((uint32_t)frac << 23) : (f & SF_SIGN);
}

/* MAC flags of one result field c (x = 0): zero and sign. */
static uint32_t mac_bits(uint32_t r, int c) {
    uint32_t bit = 1u << (3 - c), m = 0;

    if (SF_EXP(r) == 0) {
        m |= bit;
    }
    if (r & SF_SIGN) {
        m |= bit << 4;
    }
    return m;
}

static void queue(FlagStep *q, int *head, int *count, uint32_t value) {
    int slot = (*head + *count) & 7;

    q[slot].cycle = vu.cycle + 4;
    q[slot].value = value;
    if (*count < 8) {
        (*count)++;
    } else {
        *head = (*head + 1) & 7;
    }
}

/* ---------------------------------------------------------------------------------------------- interpreter */

#define DEST(n) ((dest >> (3 - (n))) & 1)
#define LDEST(n) ((ld >> (3 - (n))) & 1)
#define MEMQ(a) (DATA + (((uint32_t)(a)) & 0xFF) * 16)
#define SETVI(r, v)                                                                                                \
    do {                                                                                                           \
        if ((r) & 15) {                                                                                            \
            gPortVu0Vi[(r) & 15] = (uint16_t)(v);                                                                  \
        }                                                                                                          \
    } while (0)
#define VI(r) gPortVu0Vi[(r) & 15]

/* Runs the program at micro-memory byte address `addr` until its end (E bit and the instruction after it). */
void Port_Vu0CallMs(uint32_t addr) {
    int ending = 0, branch = 0;
    uint32_t target = 0, pc = (addr / 8) & 0x1FF, guard, c;

    for (guard = 0; guard < 1000000; guard++) {
        uint32_t lo, hi, dest, ft, fs, fd, op, bc, next = (pc + 1) & 0x1FF;
        uint32_t res[4], wmask = 0, *wr = NULL, bcv = 0;
        int wacc = 0, lower = 1, fmac = 0;

        memcpy(&lo, MICRO + pc * 8, 4);
        memcpy(&hi, MICRO + pc * 8 + 4, 4);
        vu.cycle++;
        if (vu.qReady != 0 && vu.cycle >= vu.qReady) {
            gPortVu0Q = vu.qPending;
            vu.qReady = 0;
        }
        while (vu.clipCount != 0 && vu.clipQueue[vu.clipHead].cycle <= vu.cycle) {
            vu.clip = vu.clipQueue[vu.clipHead].value;
            vu.clipHead = (vu.clipHead + 1) & 7;
            vu.clipCount--;
        }
        while (vu.macCount != 0 && vu.macQueue[vu.macHead].cycle <= vu.cycle) {
            vu.mac = vu.macQueue[vu.macHead].value;
            vu.macHead = (vu.macHead + 1) & 7;
            vu.macCount--;
        }
        dest = (hi >> 21) & 15;
        ft = (hi >> 16) & 31;
        fs = (hi >> 11) & 31;
        fd = (hi >> 6) & 31;
        op = hi & 0x3F;
        bc = hi & 3;
        bcv = VF(ft)[bc];
        if (hi & 0x80000000u) { /* I bit: the lower word is the I register's value */
            vu.i = lo;
            lower = 0;
        }
#define A(c) VF(fs)[c]
#define B(c) VF(ft)[c]
#define ACC(c) gPortVu0Acc[c]
#define EACH for (c = 0; c < 4; c++) if (DEST(c))
#define TO_FD wr = VF(fd); wmask = dest; fmac = 1
#define TO_ACC wacc = 1; wmask = dest; fmac = 1
        /* ---- upper instruction ---- */
        if (hi == 0x000002FFu || (hi & 0x7FFFFFFFu) == 0x000002FFu) {
            goto upper_done;
        }
        memcpy(res, op >= 0x3C ? gPortVu0Acc : VF(fd), 16);
        if (op < 0x3C) {
            switch (op) {
            case 0x00: case 0x01: case 0x02: case 0x03: EACH res[c] = vadd(A(c), bcv); TO_FD; break;
            case 0x04: case 0x05: case 0x06: case 0x07: EACH res[c] = vsub(A(c), bcv); TO_FD; break;
            case 0x08: case 0x09: case 0x0A: case 0x0B: EACH res[c] = vadd(ACC(c), vmul(A(c), bcv)); TO_FD; break;
            case 0x0C: case 0x0D: case 0x0E: case 0x0F: EACH res[c] = vsub(ACC(c), vmul(A(c), bcv)); TO_FD; break;
            case 0x10: case 0x11: case 0x12: case 0x13: EACH res[c] = vmax(A(c), bcv); wr = VF(fd); wmask = dest; break;
            case 0x14: case 0x15: case 0x16: case 0x17: EACH res[c] = vmin(A(c), bcv); wr = VF(fd); wmask = dest; break;
            case 0x18: case 0x19: case 0x1A: case 0x1B: EACH res[c] = vmul(A(c), bcv); TO_FD; break;
            case 0x1C: EACH res[c] = vmul(A(c), gPortVu0Q); TO_FD; break;
            case 0x1D: EACH res[c] = vmax(A(c), vu.i); wr = VF(fd); wmask = dest; break;
            case 0x1E: EACH res[c] = vmul(A(c), vu.i); TO_FD; break;
            case 0x1F: EACH res[c] = vmin(A(c), vu.i); wr = VF(fd); wmask = dest; break;
            case 0x20: EACH res[c] = vadd(A(c), gPortVu0Q); TO_FD; break;
            case 0x21: EACH res[c] = vadd(ACC(c), vmul(A(c), gPortVu0Q)); TO_FD; break;
            case 0x22: EACH res[c] = vadd(A(c), vu.i); TO_FD; break;
            case 0x23: EACH res[c] = vadd(ACC(c), vmul(A(c), vu.i)); TO_FD; break;
            case 0x24: EACH res[c] = vsub(A(c), gPortVu0Q); TO_FD; break;
            case 0x25: EACH res[c] = vsub(ACC(c), vmul(A(c), gPortVu0Q)); TO_FD; break;
            case 0x26: EACH res[c] = vsub(A(c), vu.i); TO_FD; break;
            case 0x27: EACH res[c] = vsub(ACC(c), vmul(A(c), vu.i)); TO_FD; break;
            case 0x28: EACH res[c] = vadd(A(c), B(c)); TO_FD; break;
            case 0x29: EACH res[c] = vadd(ACC(c), vmul(A(c), B(c))); TO_FD; break;
            case 0x2A: EACH res[c] = vmul(A(c), B(c)); TO_FD; break;
            case 0x2B: EACH res[c] = vmax(A(c), B(c)); wr = VF(fd); wmask = dest; break;
            case 0x2C: EACH res[c] = vsub(A(c), B(c)); TO_FD; break;
            case 0x2D: EACH res[c] = vsub(ACC(c), vmul(A(c), B(c))); TO_FD; break;
            case 0x2E: /* OPMSUB */
                res[0] = vsub(ACC(0), vmul(A(1), B(2)));
                res[1] = vsub(ACC(1), vmul(A(2), B(0)));
                res[2] = vsub(ACC(2), vmul(A(0), B(1)));
                wr = VF(fd); wmask = 0xE; fmac = 1;
                break;
            case 0x2F: EACH res[c] = vmin(A(c), B(c)); wr = VF(fd); wmask = dest; break;
            default: sUnknown[0][op]++; break;
            }
        } else {
            uint32_t sp = (fd << 2) | bc;
            switch (sp) {
            case 0x00: case 0x01: case 0x02: case 0x03: EACH res[c] = vadd(A(c), bcv); TO_ACC; break;
            case 0x04: case 0x05: case 0x06: case 0x07: EACH res[c] = vsub(A(c), bcv); TO_ACC; break;
            case 0x08: case 0x09: case 0x0A: case 0x0B: EACH res[c] = vadd(ACC(c), vmul(A(c), bcv)); TO_ACC; break;
            case 0x0C: case 0x0D: case 0x0E: case 0x0F: EACH res[c] = vsub(ACC(c), vmul(A(c), bcv)); TO_ACC; break;
            case 0x10: case 0x11: case 0x12: case 0x13: /* ITOF0 / 4 / 12 / 15 */
                memcpy(res, VF(ft), 16);
                EACH res[c] = itof(A(c), sp == 0x10 ? 0 : sp == 0x11 ? 4 : sp == 0x12 ? 12 : 15);
                wr = VF(ft); wmask = dest;
                break;
            case 0x14: case 0x15: case 0x16: case 0x17: /* FTOI0 / 4 / 12 / 15 */
                memcpy(res, VF(ft), 16);
                EACH res[c] = ftoi(A(c), sp == 0x14 ? 0 : sp == 0x15 ? 4 : sp == 0x16 ? 12 : 15);
                wr = VF(ft); wmask = dest;
                break;
            case 0x18: case 0x19: case 0x1A: case 0x1B: EACH res[c] = vmul(A(c), bcv); TO_ACC; break;
            case 0x1C: EACH res[c] = vmul(A(c), gPortVu0Q); TO_ACC; break;
            case 0x1D: memcpy(res, VF(ft), 16); EACH res[c] = A(c) & 0x7FFFFFFFu; wr = VF(ft); wmask = dest; break;
            case 0x1E: EACH res[c] = vmul(A(c), vu.i); TO_ACC; break;
            case 0x1F: { /* CLIP: fs.xyz against +/-|ft.w| */
                int32_t w = sf_key(B(3) & 0x7FFFFFFFu);
                uint32_t f = (sf_key(A(0)) > w) | (sf_key(A(0)) < -w) << 1 | (sf_key(A(1)) > w) << 2 |
                             (sf_key(A(1)) < -w) << 3 | (sf_key(A(2)) > w) << 4 | (sf_key(A(2)) < -w) << 5;
                vu.clipLatest = ((vu.clipLatest << 6) | f) & 0xFFFFFF;
                queue(vu.clipQueue, &vu.clipHead, &vu.clipCount, vu.clipLatest);
                break;
            }
            case 0x20: EACH res[c] = vadd(A(c), gPortVu0Q); TO_ACC; break;
            case 0x21: EACH res[c] = vadd(ACC(c), vmul(A(c), gPortVu0Q)); TO_ACC; break;
            case 0x22: EACH res[c] = vadd(A(c), vu.i); TO_ACC; break;
            case 0x23: EACH res[c] = vadd(ACC(c), vmul(A(c), vu.i)); TO_ACC; break;
            case 0x24: EACH res[c] = vsub(A(c), gPortVu0Q); TO_ACC; break;
            case 0x25: EACH res[c] = vsub(ACC(c), vmul(A(c), gPortVu0Q)); TO_ACC; break;
            case 0x26: EACH res[c] = vsub(A(c), vu.i); TO_ACC; break;
            case 0x27: EACH res[c] = vsub(ACC(c), vmul(A(c), vu.i)); TO_ACC; break;
            case 0x28: EACH res[c] = vadd(A(c), B(c)); TO_ACC; break;
            case 0x29: EACH res[c] = vadd(ACC(c), vmul(A(c), B(c))); TO_ACC; break;
            case 0x2A: EACH res[c] = vmul(A(c), B(c)); TO_ACC; break;
            case 0x2C: EACH res[c] = vsub(A(c), B(c)); TO_ACC; break;
            case 0x2D: EACH res[c] = vsub(ACC(c), vmul(A(c), B(c))); TO_ACC; break;
            case 0x2E: /* OPMULA */
                res[0] = vmul(A(1), B(2));
                res[1] = vmul(A(2), B(0));
                res[2] = vmul(A(0), B(1));
                wacc = 1; wmask = 0xE; fmac = 1;
                break;
            case 0x2F: break; /* NOP */
            default: sUnknown[0][64 + (sp & 63)]++; break;
            }
        }
        if (fmac) {
            uint32_t m = 0;
            for (c = 0; c < 4; c++) {
                if ((wmask >> (3 - c)) & 1) {
                    m |= mac_bits(res[c], (int)c);
                }
            }
            queue(vu.macQueue, &vu.macHead, &vu.macCount, m);
        }
upper_done:
        /* ---- lower instruction ---- */
        if (lower && lo != 0x8000033Cu) {
            uint32_t lop = lo >> 25, it = (lo >> 16) & 31, is = (lo >> 11) & 31, id = (lo >> 6) & 31, ld = (lo >> 21) & 15;
            int32_t imm11 = (int32_t)(lo << 21) >> 21;
            uint32_t imm15 = ((lo >> 10) & 0x7800) | (lo & 0x7FF);
            uint32_t boff = (pc + 1 + (uint32_t)imm11) & 0x1FF;

            switch (lop) {
            case 0x00: { /* LQ */
                const uint8_t *m = MEMQ((int32_t)VI(is) + imm11);
                for (c = 0; c < 4; c++) if (LDEST(c) && it) memcpy(&VF(it)[c], m + c * 4, 4);
                break;
            }
            case 0x01: { /* SQ */
                uint8_t *m = MEMQ((int32_t)VI(it) + imm11);
                for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &VF(is)[c], 4);
                break;
            }
            case 0x04: { /* ILW */
                const uint8_t *m = MEMQ((int32_t)VI(is) + imm11);
                for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w; memcpy(&w, m + c * 4, 4); SETVI(it, w); }
                break;
            }
            case 0x05: { /* ISW */
                uint8_t *m = MEMQ((int32_t)VI(is) + imm11);
                for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w = VI(it); memcpy(m + c * 4, &w, 4); }
                break;
            }
            case 0x08: SETVI(it, VI(is) + imm15); break; /* IADDIU */
            case 0x09: SETVI(it, VI(is) - imm15); break; /* ISUBIU */
            case 0x10: SETVI(1, (vu.clip & 0xFFFFFF) == (lo & 0xFFFFFF)); break; /* FCEQ */
            case 0x11: vu.clip = vu.clipLatest = lo & 0xFFFFFF; vu.clipCount = 0; break; /* FCSET */
            case 0x12: SETVI(1, (vu.clip & lo & 0xFFFFFF) != 0); break; /* FCAND */
            case 0x13: SETVI(1, ((vu.clip | lo) & 0xFFFFFF) == 0xFFFFFF); break; /* FCOR */
            case 0x18: SETVI(it, (vu.mac & 0xFFFF) == (VI(is) & 0xFFFF)); break; /* FMEQ */
            case 0x1A: SETVI(it, vu.mac & VI(is)); break; /* FMAND */
            case 0x1B: SETVI(it, vu.mac | VI(is)); break; /* FMOR */
            case 0x1C: SETVI(it, vu.clip & 0xFFF); break; /* FCGET */
            case 0x20: branch = 2; target = boff; break; /* B */
            case 0x21: SETVI(it, (pc + 2) & 0x1FF); branch = 2; target = boff; break; /* BAL */
            case 0x24: branch = 2; target = VI(is) & 0x1FF; break; /* JR */
            case 0x25: { uint32_t t = VI(is) & 0x1FF; SETVI(it, (pc + 2) & 0x1FF); branch = 2; target = t; break; }
            case 0x28: if (VI(it) == VI(is)) { branch = 2; target = boff; } break; /* IBEQ */
            case 0x29: if (VI(it) != VI(is)) { branch = 2; target = boff; } break; /* IBNE */
            case 0x2C: if ((int16_t)VI(is) < 0) { branch = 2; target = boff; } break;
            case 0x2D: if ((int16_t)VI(is) > 0) { branch = 2; target = boff; } break;
            case 0x2E: if ((int16_t)VI(is) <= 0) { branch = 2; target = boff; } break;
            case 0x2F: if ((int16_t)VI(is) >= 0) { branch = 2; target = boff; } break;
            case 0x40: {
                uint32_t f = lo & 0x3F;
                if (f < 0x3C) {
                    switch (f) {
                    case 0x30: SETVI(id, VI(is) + VI(it)); break; /* IADD */
                    case 0x31: SETVI(id, VI(is) - VI(it)); break; /* ISUB */
                    case 0x32: SETVI(it, VI(is) + (uint32_t)((int32_t)(lo << 21) >> 27)); break; /* IADDI */
                    case 0x34: SETVI(id, VI(is) & VI(it)); break; /* IAND */
                    case 0x35: SETVI(id, VI(is) | VI(it)); break; /* IOR */
                    default: sUnknown[1][f]++; break;
                    }
                } else {
                    uint32_t sp = (id << 2) | (lo & 3), fsf = (lo >> 21) & 3, ftf = (lo >> 23) & 3;
                    switch (sp) {
                    case 0x30: if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) VF(it)[c] = VF(is)[c]; } break; /* MOVE */
                    case 0x31: if (it) { uint32_t s[4]; memcpy(s, VF(is), 16); /* MR32 */
                                         for (c = 0; c < 4; c++) if (LDEST(c)) VF(it)[c] = s[(c + 1) & 3]; } break;
                    case 0x34: { const uint8_t *m = MEMQ(VI(is)); /* LQI */
                                 if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(&VF(it)[c], m + c * 4, 4); }
                                 SETVI(is, VI(is) + 1); break; }
                    case 0x35: { uint8_t *m = MEMQ(VI(it)); /* SQI */
                                 for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &VF(is)[c], 4);
                                 SETVI(it, VI(it) + 1); break; }
                    case 0x36: { const uint8_t *m; SETVI(is, VI(is) - 1); m = MEMQ(VI(is)); /* LQD */
                                 if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(&VF(it)[c], m + c * 4, 4); } break; }
                    case 0x37: { uint8_t *m; SETVI(it, VI(it) - 1); m = MEMQ(VI(it)); /* SQD */
                                 for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &VF(is)[c], 4); break; }
                    case 0x38: vu.qPending = sf_div_core(VF(is)[fsf], VF(it)[ftf], 0); vu.qReady = vu.cycle + 7; break; /* DIV */
                    case 0x39: vu.qPending = sf_sqrt_core(VF(it)[ftf], 0); vu.qReady = vu.cycle + 7; break; /* SQRT */
                    case 0x3A: vu.qPending = sf_div_core(VF(is)[fsf], sf_sqrt_core(VF(it)[ftf], 0), 0); /* RSQRT */
                               vu.qReady = vu.cycle + 13; break;
                    case 0x3B: if (vu.qReady != 0) { gPortVu0Q = vu.qPending; vu.qReady = 0; } break; /* WAITQ */
                    case 0x3C: SETVI(it, VF(is)[fsf]); break; /* MTIR */
                    case 0x3D: if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) VF(it)[c] = (uint32_t)(int32_t)(int16_t)VI(is); } break; /* MFIR */
                    case 0x3E: { const uint8_t *m = MEMQ(VI(is)); /* ILWR */
                                 for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w; memcpy(&w, m + c * 4, 4); SETVI(it, w); } break; }
                    case 0x3F: { uint8_t *m = MEMQ(VI(is)); /* ISWR */
                                 for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w = VI(it); memcpy(m + c * 4, &w, 4); } break; }
                    case 0x7B: break; /* WAITP */
                    default: sUnknown[1][64 + (sp & 63)]++; break;
                    }
                }
                break;
            }
            default: sUnknown[1][lop & 63]++; break;
            }
        }
        /* ---- commit the upper instruction's result ---- */
        if (wr != NULL && wr != VF(0)) {
            for (c = 0; c < 4; c++) if ((wmask >> (3 - c)) & 1) wr[c] = res[c];
        } else if (wacc) {
            for (c = 0; c < 4; c++) if ((wmask >> (3 - c)) & 1) gPortVu0Acc[c] = res[c];
        }
        VF(0)[0] = VF(0)[1] = VF(0)[2] = 0;
        VF(0)[3] = 0x3F800000u;
        gPortVu0Vi[0] = 0;
        if (ending) {
            break;
        }
        if (hi & 0x40000000u) {
            ending = 1;
        }
        if (branch == 1) {
            pc = target;
            branch = 0;
        } else {
            if (branch == 2) {
                branch = 1;
            }
            pc = next;
        }
    }
    {
        static int reported;
        unsigned k, n = 0;
        for (k = 0; k < 128; k++) {
            n += sUnknown[0][k] + sUnknown[1][k];
        }
        if (n != 0 && !reported++) {
            fprintf(stderr, "ttt: vu0: %u instructions not implemented (program 0x%03X)\n", n, addr);
        }
        if (guard >= 1000000 && reported++ < 2) {
            fprintf(stderr, "ttt: vu0: program 0x%03X did not end\n", addr);
        }
    }
}
