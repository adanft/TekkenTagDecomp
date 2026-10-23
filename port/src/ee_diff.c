/*
 * Draft divergence check (TTT_DIFF): runs a game function twice on the same memory, once as the port's C (a draft or
 * a hand replacement) and once as the original EE code of the executable through a small EE interpreter, and prints
 * the words of EE RAM / scratchpad where the two results differ. The port continues with the C result.
 *
 * The interpreter covers user-mode EE code as the game's leaf and mid-level functions use it: the integer core with
 * the EE's 64-bit registers, the 128-bit loads / stores and the MMI instructions met so far, the FPU with the EE float
 * model (softfloat_ps2_inl.h: the same arithmetic as the port's C), and COP2 macro mode (libvu0 and the hand-written
 * VU0 routines) with the vector unit's arithmetic. Calls are interpreted too (the callee's original code), so the
 * check compares the whole call tree. Instructions, sections and data come from the executable (TTT_DIFF_ELF, default
 * build/SLUS_200.01 next to build/port/ttt, the matching build's output = your SLUS_200.01): .text, .rodata and .lit4 are read from its image
 * (the port does not keep .lit4 at its address), everything else from the running program's memory (the port keeps
 * the game's data at its PS2 addresses). Anything outside that set (an unknown instruction, a call outside .text,
 * a syscall, the VU0 flags) stops the check with a report; the port continues as if the check had not run.
 *
 *   TTT_DIFF=<vblank>[:<calls>]  check the hooked calls (PORT_DIFF in the game sources) from that VBlank on, at most
 *                                <calls> per function (default 1).
 *   TTT_DIFF_ONLY=<name,...>     only these functions.
 *
 * No host floating point: the float operations are softfloat_ps2_inl.h's on bit patterns.
 */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "softfloat_ps2_inl.h"

uint32_t Port_VBlankCount(void);
extern float gPortVu0Vf[32][4];

#define RAM_LO 0x00010000u
#define RAM_HI 0x02000000u
#define SPR_LO 0x70000000u
#define SPR_SIZE 0x4000u
#define RET_MAGIC 0x00000010u /* return address of the outermost call: no code there */
#define MAX_STEPS 200000000u

/* ------------------------------------------------------------------------------------------- the executable */

typedef struct {
    uint32_t lo, hi;
} Range;

static uint8_t *sImg;           /* the loadable segment */
static uint32_t sImgLo, sImgHi; /* its addresses */
static Range sRo[3];            /* .text, .rodata, .lit4 */
static uint32_t sBssEnd;        /* the end of .bss: the EE stack lies above it */
static uint32_t sGp;
static int sImgState; /* 0 not loaded, 1 ok, -1 failed */

static uint32_t rd32le(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static int load_elf(void) {
    const char *path = getenv("TTT_DIFF_ELF");
    FILE *f;
    long n;
    uint8_t *d;
    uint32_t phoff, shoff, i, shstr;
    uint16_t phnum, shnum, shentsize, shstrndx;

    if (sImgState != 0) {
        return sImgState;
    }
    sImgState = -1;
    if (path == NULL) { /* build/SLUS_200.01, next to build/port/ttt */
        static char buf[1100];
        ssize_t k = readlink("/proc/self/exe", buf, 1000);
        char *slash;
        buf[k > 0 ? k : 0] = 0;
        slash = strrchr(buf, '/');
        if (slash != NULL) {
            strcpy(slash + 1, "../SLUS_200.01");
            path = buf;
        } else {
            path = "build/SLUS_200.01";
        }
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        printf("ee-diff: cannot open %s (TTT_DIFF_ELF)\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    d = malloc((size_t)n);
    if (d == NULL || fread(d, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return -1;
    }
    fclose(f);
    phoff = rd32le(d + 28);
    shoff = rd32le(d + 32);
    phnum = rd16le(d + 44);
    shentsize = rd16le(d + 46);
    shnum = rd16le(d + 48);
    shstrndx = rd16le(d + 50);
    for (i = 0; i < phnum; i++) {
        const uint8_t *ph = d + phoff + i * 32;
        uint32_t off = rd32le(ph + 4), va = rd32le(ph + 8), fsz = rd32le(ph + 16);
        if (rd32le(ph) == 1 && fsz != 0) {
            sImg = d + off;
            sImgLo = va;
            sImgHi = va + fsz;
        }
    }
    shstr = rd32le(d + shoff + shstrndx * shentsize + 16);
    for (i = 0; i < shnum; i++) {
        const uint8_t *sh = d + shoff + i * shentsize;
        const char *nm = (const char *)d + shstr + rd32le(sh);
        uint32_t a = rd32le(sh + 12), sz = rd32le(sh + 20);
        int k = strcmp(nm, ".text") == 0 ? 0 : strcmp(nm, ".rodata") == 0 ? 1 : strcmp(nm, ".lit4") == 0 ? 2 : -1;
        if (k >= 0) {
            sRo[k].lo = a;
            sRo[k].hi = a + sz;
        }
        if (strcmp(nm, ".bss") == 0) {
            sBssEnd = (a + sz + 15) & ~15u;
        }
        if (strcmp(nm, ".reginfo") == 0) {
            sGp = rd32le(d + rd32le(sh + 16) + 20);
        }
    }
    if (sImg == NULL || sRo[0].hi == 0 || sGp == 0) {
        printf("ee-diff: %s: no loadable segment / .text / .reginfo\n", path);
        return -1;
    }
    sImgState = 1;
    return 1;
}

/* ------------------------------------------------------------------------------------------- the interpreter */

typedef struct {
    uint64_t lo, hi;
} Gpr;

typedef struct {
    Gpr r[32];
    uint64_t hi, lo, hi1, lo1;
    uint32_t sa;
    uint32_t f[32], acc;
    int fcc;
    uint32_t vf[32][4], vacc[4], q, vi_i;
    uint16_t vi[16];
    uint32_t pc;
    uint64_t steps;
    int trace, depth, tracePending; /* TTT_DIFF_TRACE: the calls the outermost function makes */
    int stop; /* 0 running, 1 returned, -1 failed */
    char why[160];
} Ee;

static void fail(Ee *e, const char *what, uint32_t op) {
    if (e->stop == 0) {
        e->stop = -1;
        snprintf(e->why, sizeof e->why, "%s (op %08X at %08X)", what, op, e->pc);
    }
}

static int is_ro(uint32_t a) {
    int k;
    for (k = 0; k < 3; k++) {
        if (a >= sRo[k].lo && a < sRo[k].hi) {
            return 1;
        }
    }
    return 0;
}

/* Where the data at a is read: the executable's image for its read-only sections, else the running memory. */
static uint8_t *rptr(Ee *e, uint32_t a, uint32_t n) {
    (void)e;
    if (is_ro(a)) {
        return sImg + (a - sImgLo);
    }
    if ((a >= RAM_LO && a + n <= RAM_HI) || (a >= 0x20000000u + RAM_LO && a + n <= 0x22000000u) ||
        (a >= 0x30000000u + RAM_LO && a + n <= 0x32000000u) || (a >= SPR_LO && a + n <= SPR_LO + SPR_SIZE) ||
        a >= 0x08000000u /* host memory: the interpreter's own stack */) {
        return (uint8_t *)(uintptr_t)a;
    }
    return NULL;
}

static uint8_t *wptr(Ee *e, uint32_t a, uint32_t n) {
    if (is_ro(a)) {
        return NULL;
    }
    return rptr(e, a, n);
}

#define LOADN(T, a, out)                                                                                               \
    do {                                                                                                               \
        uint8_t *p_ = rptr(e, (a), sizeof(T));                                                                         \
        if (p_ == NULL) {                                                                                              \
            fail(e, "load outside memory", op);                                                                        \
            return;                                                                                                    \
        }                                                                                                              \
        memcpy(&(out), p_, sizeof(T));                                                                                 \
    } while (0)
#define STOREN(T, a, v)                                                                                                \
    do {                                                                                                               \
        T v_ = (T)(v);                                                                                                 \
        uint8_t *p_ = wptr(e, (a), sizeof(T));                                                                         \
        if (p_ == NULL) {                                                                                              \
            fail(e, "store outside memory", op);                                                                       \
            return;                                                                                                    \
        }                                                                                                              \
        memcpy(p_, &v_, sizeof(T));                                                                                    \
    } while (0)

static uint64_t sx32(uint32_t v) { return (uint64_t)(int64_t)(int32_t)v; }

/* FPU (the EE model of softfloat_ps2.c) */
static uint32_t fadd(uint32_t a, uint32_t b) { return sf_fpu_add(a, b); }
static uint32_t fmul(uint32_t a, uint32_t b) { return sf_mul_core(a, b, 0); }
static uint32_t fdivv(uint32_t a, uint32_t b) { return sf_div_core(a, b, 1); }
/* vector unit (vu_micro.c) */
static uint32_t vadd(uint32_t a, uint32_t b) { return sf_add_core(a, b, 0); }
static uint32_t vsub(uint32_t a, uint32_t b) { return sf_add_core(a, b ^ SF_SIGN, 0); }
static uint32_t vmul(uint32_t a, uint32_t b) { return sf_mul_core(a, b, 0); }
static uint32_t vmax(uint32_t a, uint32_t b) { return sf_key(a) >= sf_key(b) ? a : b; }
static uint32_t vmin(uint32_t a, uint32_t b) { return sf_key(a) <= sf_key(b) ? a : b; }

static uint32_t vftoi(uint32_t a, int frac) {
    uint32_t m = SF_MANT(a), v;
    int x = (int)SF_EXP(a) - 127 + frac;

    if (SF_EXP(a) == 0 || x < 0) {
        return 0;
    }
    if (x >= 31) {
        return (a & SF_SIGN) ? 0x80000000u : 0x7FFFFFFFu;
    }
    v = x >= 23 ? m << (x - 23) : m >> (23 - x);
    return (a & SF_SIGN) ? 0u - v : v;
}

static uint32_t vitof(uint32_t v, int frac) {
    uint32_t f = sf_itof((int32_t)v);
    if (f == 0 || frac == 0) {
        return f;
    }
    return SF_EXP(f) > (uint32_t)frac ? f - ((uint32_t)frac << 23) : (f & SF_SIGN);
}

static void cop2(Ee *e, uint32_t op) {
    uint32_t dest = (op >> 21) & 15, ft = (op >> 16) & 31, fs = (op >> 11) & 31, fd = (op >> 6) & 31, fn = op & 63;
    uint32_t bc = op & 3, res[4], bcv = e->vf[ft][bc];
    uint32_t (*A)[4] = &e->vf[fs], (*B)[4] = &e->vf[ft];
    int c, toacc = 0, tofd = 0;
    uint32_t *wr = NULL;

#define D(c) ((dest >> (3 - (c))) & 1)
#define EACH for (c = 0; c < 4; c++) if (D(c))
#define AC(c) (*A)[c]
#define BC(c) (*B)[c]
    memcpy(res, e->vf[fd], 16);
    if (fn < 0x3C) {
        switch (fn) {
        case 0x00: case 0x01: case 0x02: case 0x03: EACH res[c] = vadd(AC(c), bcv); tofd = 1; break;
        case 0x04: case 0x05: case 0x06: case 0x07: EACH res[c] = vsub(AC(c), bcv); tofd = 1; break;
        case 0x08: case 0x09: case 0x0A: case 0x0B: EACH res[c] = vadd(e->vacc[c], vmul(AC(c), bcv)); tofd = 1; break;
        case 0x0C: case 0x0D: case 0x0E: case 0x0F: EACH res[c] = vsub(e->vacc[c], vmul(AC(c), bcv)); tofd = 1; break;
        case 0x10: case 0x11: case 0x12: case 0x13: EACH res[c] = vmax(AC(c), bcv); tofd = 1; break;
        case 0x14: case 0x15: case 0x16: case 0x17: EACH res[c] = vmin(AC(c), bcv); tofd = 1; break;
        case 0x18: case 0x19: case 0x1A: case 0x1B: EACH res[c] = vmul(AC(c), bcv); tofd = 1; break;
        case 0x1C: EACH res[c] = vmul(AC(c), e->q); tofd = 1; break;
        case 0x1D: EACH res[c] = vmax(AC(c), e->vi_i); tofd = 1; break;
        case 0x1E: EACH res[c] = vmul(AC(c), e->vi_i); tofd = 1; break;
        case 0x1F: EACH res[c] = vmin(AC(c), e->vi_i); tofd = 1; break;
        case 0x20: EACH res[c] = vadd(AC(c), e->q); tofd = 1; break;
        case 0x21: EACH res[c] = vadd(e->vacc[c], vmul(AC(c), e->q)); tofd = 1; break;
        case 0x22: EACH res[c] = vadd(AC(c), e->vi_i); tofd = 1; break;
        case 0x23: EACH res[c] = vadd(e->vacc[c], vmul(AC(c), e->vi_i)); tofd = 1; break;
        case 0x24: EACH res[c] = vsub(AC(c), e->q); tofd = 1; break;
        case 0x25: EACH res[c] = vsub(e->vacc[c], vmul(AC(c), e->q)); tofd = 1; break;
        case 0x26: EACH res[c] = vsub(AC(c), e->vi_i); tofd = 1; break;
        case 0x27: EACH res[c] = vsub(e->vacc[c], vmul(AC(c), e->vi_i)); tofd = 1; break;
        case 0x28: EACH res[c] = vadd(AC(c), BC(c)); tofd = 1; break;
        case 0x29: EACH res[c] = vadd(e->vacc[c], vmul(AC(c), BC(c))); tofd = 1; break;
        case 0x2A: EACH res[c] = vmul(AC(c), BC(c)); tofd = 1; break;
        case 0x2B: EACH res[c] = vmax(AC(c), BC(c)); tofd = 1; break;
        case 0x2C: EACH res[c] = vsub(AC(c), BC(c)); tofd = 1; break;
        case 0x2D: EACH res[c] = vsub(e->vacc[c], vmul(AC(c), BC(c))); tofd = 1; break;
        case 0x2E: /* OPMSUB: xyz */
            res[0] = vsub(e->vacc[0], vmul(AC(1), BC(2)));
            res[1] = vsub(e->vacc[1], vmul(AC(2), BC(0)));
            res[2] = vsub(e->vacc[2], vmul(AC(0), BC(1)));
            dest = 0xE;
            tofd = 1;
            break;
        case 0x2F: EACH res[c] = vmin(AC(c), BC(c)); tofd = 1; break;
        case 0x30: if (fd & 15) e->vi[fd & 15] = (uint16_t)(e->vi[fs & 15] + e->vi[ft & 15]); return; /* VIADD */
        case 0x31: if (fd & 15) e->vi[fd & 15] = (uint16_t)(e->vi[fs & 15] - e->vi[ft & 15]); return; /* VISUB */
        case 0x32: if (ft & 15) e->vi[ft & 15] = (uint16_t)(e->vi[fs & 15] + (uint32_t)((int32_t)(op << 21) >> 27)); return;
        case 0x34: if (fd & 15) e->vi[fd & 15] = e->vi[fs & 15] & e->vi[ft & 15]; return; /* VIAND */
        case 0x35: if (fd & 15) e->vi[fd & 15] = e->vi[fs & 15] | e->vi[ft & 15]; return; /* VIOR */
        default: fail(e, "COP2 macro instruction not modelled", op); return;
        }
    } else {
        uint32_t sp = (fd << 2) | bc, fsf = (op >> 21) & 3, ftf = (op >> 23) & 3;
        memcpy(res, e->vacc, 16);
        switch (sp) {
        case 0x00: case 0x01: case 0x02: case 0x03: EACH res[c] = vadd(AC(c), bcv); toacc = 1; break;
        case 0x04: case 0x05: case 0x06: case 0x07: EACH res[c] = vsub(AC(c), bcv); toacc = 1; break;
        case 0x08: case 0x09: case 0x0A: case 0x0B: EACH res[c] = vadd(e->vacc[c], vmul(AC(c), bcv)); toacc = 1; break;
        case 0x0C: case 0x0D: case 0x0E: case 0x0F: EACH res[c] = vsub(e->vacc[c], vmul(AC(c), bcv)); toacc = 1; break;
        case 0x10: case 0x11: case 0x12: case 0x13:
            memcpy(res, e->vf[ft], 16);
            EACH res[c] = vitof(AC(c), sp == 0x10 ? 0 : sp == 0x11 ? 4 : sp == 0x12 ? 12 : 15);
            wr = e->vf[ft];
            break;
        case 0x14: case 0x15: case 0x16: case 0x17:
            memcpy(res, e->vf[ft], 16);
            EACH res[c] = vftoi(AC(c), sp == 0x14 ? 0 : sp == 0x15 ? 4 : sp == 0x16 ? 12 : 15);
            wr = e->vf[ft];
            break;
        case 0x18: case 0x19: case 0x1A: case 0x1B: EACH res[c] = vmul(AC(c), bcv); toacc = 1; break;
        case 0x1C: EACH res[c] = vmul(AC(c), e->q); toacc = 1; break;
        case 0x1D: memcpy(res, e->vf[ft], 16); EACH res[c] = AC(c) & 0x7FFFFFFFu; wr = e->vf[ft]; break; /* VABS */
        case 0x1E: EACH res[c] = vmul(AC(c), e->vi_i); toacc = 1; break;
        case 0x20: EACH res[c] = vadd(AC(c), e->q); toacc = 1; break;
        case 0x21: EACH res[c] = vadd(e->vacc[c], vmul(AC(c), e->q)); toacc = 1; break;
        case 0x22: EACH res[c] = vadd(AC(c), e->vi_i); toacc = 1; break;
        case 0x23: EACH res[c] = vadd(e->vacc[c], vmul(AC(c), e->vi_i)); toacc = 1; break;
        case 0x24: EACH res[c] = vsub(AC(c), e->q); toacc = 1; break;
        case 0x25: EACH res[c] = vsub(e->vacc[c], vmul(AC(c), e->q)); toacc = 1; break;
        case 0x26: EACH res[c] = vsub(AC(c), e->vi_i); toacc = 1; break;
        case 0x27: EACH res[c] = vsub(e->vacc[c], vmul(AC(c), e->vi_i)); toacc = 1; break;
        case 0x28: EACH res[c] = vadd(AC(c), BC(c)); toacc = 1; break;
        case 0x29: EACH res[c] = vadd(e->vacc[c], vmul(AC(c), BC(c))); toacc = 1; break;
        case 0x2A: EACH res[c] = vmul(AC(c), BC(c)); toacc = 1; break;
        case 0x2C: EACH res[c] = vsub(AC(c), BC(c)); toacc = 1; break;
        case 0x2D: EACH res[c] = vsub(e->vacc[c], vmul(AC(c), BC(c))); toacc = 1; break;
        case 0x2E: /* OPMULA: xyz */
            res[0] = vmul(AC(1), BC(2));
            res[1] = vmul(AC(2), BC(0));
            res[2] = vmul(AC(0), BC(1));
            dest = 0xE;
            toacc = 1;
            break;
        case 0x2F: return; /* VNOP */
        case 0x30: /* VMOVE */
            memcpy(res, e->vf[ft], 16);
            EACH res[c] = AC(c);
            wr = e->vf[ft];
            break;
        case 0x31: /* VMR32 */
            memcpy(res, e->vf[ft], 16);
            EACH res[c] = AC((c + 1) & 3);
            wr = e->vf[ft];
            break;
        case 0x38: e->q = sf_div_core(AC(fsf), BC(ftf), 0); return;                     /* VDIV */
        case 0x39: e->q = sf_sqrt_core(BC(ftf), 0); return;                             /* VSQRT */
        case 0x3A: e->q = sf_div_core(AC(fsf), sf_sqrt_core(BC(ftf), 0), 0); return;    /* VRSQRT */
        case 0x3B: return;                                                              /* VWAITQ */
        case 0x3C: if (ft & 15) e->vi[ft & 15] = (uint16_t)AC(fsf); return;            /* VMTIR */
        case 0x3D: /* VMFIR */
            EACH e->vf[ft][c] = (uint32_t)(int32_t)(int16_t)e->vi[fs & 15];
            e->vf[0][0] = e->vf[0][1] = e->vf[0][2] = 0;
            e->vf[0][3] = 0x3F800000u;
            return;
        default: fail(e, "COP2 macro instruction not modelled", op); return;
        }
    }
    if (tofd) {
        wr = e->vf[fd];
    }
    if (toacc) {
        wr = e->vacc;
    }
    if (wr != NULL) {
        for (c = 0; c < 4; c++) {
            if (D(c)) {
                wr[c] = res[c];
            }
        }
    }
    e->vf[0][0] = e->vf[0][1] = e->vf[0][2] = 0;
    e->vf[0][3] = 0x3F800000u;
#undef D
#undef EACH
#undef AC
#undef BC
}

static uint32_t cop2_ctrl_read(Ee *e, uint32_t r, uint32_t op) {
    if (r < 16) {
        return e->vi[r];
    }
    if (r == 21) {
        return e->vi_i;
    }
    if (r == 22) {
        return e->q;
    }
    fail(e, "COP2 control register not modelled (flags)", op);
    return 0;
}

static void cop2_ctrl_write(Ee *e, uint32_t r, uint32_t v, uint32_t op) {
    if (r < 16) {
        if (r != 0) {
            e->vi[r] = (uint16_t)v;
        }
        return;
    }
    if (r == 21) {
        e->vi_i = v;
        return;
    }
    if (r == 22) {
        e->q = v;
        return;
    }
    fail(e, "COP2 control register not modelled", op);
}

static void cop1(Ee *e, uint32_t op) {
    uint32_t rs = (op >> 21) & 31, ft = (op >> 16) & 31, fs = (op >> 11) & 31, fd = (op >> 6) & 31, fn = op & 63;
    uint32_t a = e->f[fs], b = e->f[ft];

    if (rs == 0) { /* mfc1 */
        if (ft) {
            e->r[ft].lo = sx32(a);
        }
        return;
    }
    if (rs == 4) { /* mtc1 */
        e->f[fs] = (uint32_t)e->r[ft].lo;
        return;
    }
    if (rs == 2 || rs == 6) { /* cfc1 / ctc1: only the condition bit matters here */
        if (rs == 2 && ft) {
            e->r[ft].lo = fs == 31 ? sx32((uint32_t)e->fcc << 23) : fs == 0 ? 0x2E30 : 0;
        }
        if (rs == 6 && fs == 31) {
            e->fcc = (int)((e->r[ft].lo >> 23) & 1);
        }
        return;
    }
    if (rs == 20) { /* W */
        if (fn == 0x20) {
            e->f[fd] = sf_itof((int32_t)a);
            return;
        }
        fail(e, "FPU W instruction not modelled", op);
        return;
    }
    if (rs != 16) {
        fail(e, "FPU instruction not modelled", op);
        return;
    }
    switch (fn) {
    case 0x00: e->f[fd] = fadd(a, b); break;
    case 0x01: e->f[fd] = fadd(a, b ^ SF_SIGN); break;
    case 0x02: e->f[fd] = fmul(a, b); break;
    case 0x03: e->f[fd] = fdivv(a, b); break;
    case 0x04: e->f[fd] = sf_sqrt_core(b, 1); break;
    case 0x05: e->f[fd] = a & 0x7FFFFFFFu; break;
    case 0x06: e->f[fd] = a; break;
    case 0x07: e->f[fd] = a ^ SF_SIGN; break;
    case 0x16: e->f[fd] = fdivv(a, sf_sqrt_core(b, 1)); break; /* rsqrt.s [I: one rounding per step] */
    case 0x18: e->acc = fadd(a, b); break;
    case 0x19: e->acc = fadd(a, b ^ SF_SIGN); break;
    case 0x1A: e->acc = fmul(a, b); break;
    case 0x1C: e->f[fd] = fadd(e->acc, fmul(a, b)); break;
    case 0x1D: e->f[fd] = fadd(e->acc, fmul(a, b) ^ SF_SIGN); break;
    case 0x1E: e->acc = fadd(e->acc, fmul(a, b)); break;
    case 0x1F: e->acc = fadd(e->acc, fmul(a, b) ^ SF_SIGN); break;
    case 0x24: e->f[fd] = (uint32_t)sf_ftoi(a); break;
    case 0x28: e->f[fd] = sf_key(a) >= sf_key(b) ? a : b; break;
    case 0x29: e->f[fd] = sf_key(a) <= sf_key(b) ? a : b; break;
    case 0x30: e->fcc = 0; break;
    case 0x32: e->fcc = sf_key(a) == sf_key(b); break;
    case 0x34: e->fcc = sf_key(a) < sf_key(b); break;
    case 0x36: e->fcc = sf_key(a) <= sf_key(b); break;
    default: fail(e, "FPU instruction not modelled", op); break;
    }
}

static void mmi(Ee *e, uint32_t op) {
    uint32_t rs = (op >> 21) & 31, rt = (op >> 16) & 31, rd = (op >> 11) & 31, sa = (op >> 6) & 31, fn = op & 63;
    Gpr a = e->r[rs], b = e->r[rt], d = e->r[rd];

    switch (fn) {
    case 0x00: case 0x20: { /* madd / madd1 */
        uint64_t *hi = fn ? &e->hi1 : &e->hi, *lo = fn ? &e->lo1 : &e->lo;
        int64_t acc = (int64_t)(((uint64_t)(uint32_t)*hi << 32) | (uint32_t)*lo);
        acc += (int64_t)(int32_t)a.lo * (int32_t)b.lo;
        *lo = sx32((uint32_t)acc);
        *hi = sx32((uint32_t)((uint64_t)acc >> 32));
        d.lo = *lo;
        break;
    }
    case 0x10: d.lo = e->hi1; break; /* mfhi1 */
    case 0x11: e->hi1 = a.lo; return;
    case 0x12: d.lo = e->lo1; break; /* mflo1 */
    case 0x13: e->lo1 = a.lo; return;
    case 0x18: case 0x19: { /* mult1 / multu1 */
        uint64_t p = fn == 0x18 ? (uint64_t)((int64_t)(int32_t)a.lo * (int32_t)b.lo) : (uint64_t)(uint32_t)a.lo * (uint32_t)b.lo;
        e->lo1 = sx32((uint32_t)p);
        e->hi1 = sx32((uint32_t)(p >> 32));
        d.lo = e->lo1;
        break;
    }
    case 0x1A: case 0x1B: { /* div1 / divu1 */
        if ((uint32_t)b.lo != 0) {
            if (fn == 0x1A) {
                int32_t x = (int32_t)a.lo, y = (int32_t)b.lo;
                if (!(x == INT32_MIN && y == -1)) {
                    e->lo1 = sx32((uint32_t)(x / y));
                    e->hi1 = sx32((uint32_t)(x % y));
                }
            } else {
                e->lo1 = sx32((uint32_t)a.lo / (uint32_t)b.lo);
                e->hi1 = sx32((uint32_t)a.lo % (uint32_t)b.lo);
            }
        }
        return;
    }
    case 0x08: /* MMI0 */
        switch (sa) {
        case 0x12: /* pextlw */
            d.lo = (b.lo & 0xFFFFFFFFu) | (a.lo << 32);
            d.hi = (b.lo >> 32) | (a.lo & 0xFFFFFFFF00000000ull);
            break;
        default: fail(e, "MMI0 instruction not modelled", op); return;
        }
        break;
    case 0x28: /* MMI1 */
        switch (sa) {
        case 0x12: /* pextuw */
            d.lo = (b.hi & 0xFFFFFFFFu) | (a.hi << 32);
            d.hi = (b.hi >> 32) | (a.hi & 0xFFFFFFFF00000000ull);
            break;
        default: fail(e, "MMI1 instruction not modelled", op); return;
        }
        break;
    case 0x09: /* MMI2 */
        switch (sa) {
        case 0x08: d.lo = e->hi; d.hi = e->hi1; break; /* pmfhi */
        case 0x09: d.lo = e->lo; d.hi = e->lo1; break; /* pmflo */
        case 0x0E: d.lo = b.lo; d.hi = a.lo; break;    /* pcpyld */
        case 0x12: d.lo = a.lo & b.lo; d.hi = a.hi & b.hi; break;
        case 0x13: d.lo = a.lo ^ b.lo; d.hi = a.hi ^ b.hi; break;
        default: fail(e, "MMI2 instruction not modelled", op); return;
        }
        break;
    case 0x29: /* MMI3 */
        switch (sa) {
        case 0x08: e->hi = a.lo; e->hi1 = a.hi; return; /* pmthi */
        case 0x09: e->lo = a.lo; e->lo1 = a.hi; return; /* pmtlo */
        case 0x0E: d.lo = a.hi; d.hi = b.hi; break;     /* pcpyud */
        case 0x12: d.lo = a.lo | b.lo; d.hi = a.hi | b.hi; break;
        case 0x13: d.lo = ~(a.lo | b.lo); d.hi = ~(a.hi | b.hi); break;
        default: fail(e, "MMI3 instruction not modelled", op); return;
        }
        break;
    default: fail(e, "MMI instruction not modelled", op); return;
    }
    if (rd) {
        e->r[rd] = d;
    }
}

/* One instruction at e->pc (not a delay slot's branch handling: step() does that). Returns the branch target when the
   instruction is a taken branch / jump (with *likely_skip for a likely branch not taken), else 0. */
static void exec(Ee *e, uint32_t op, uint32_t pc, int *branch, uint32_t *target, int *skip) {
    uint32_t o = op >> 26, rs = (op >> 21) & 31, rt = (op >> 16) & 31, rd = (op >> 11) & 31, sh = (op >> 6) & 31;
    uint64_t A = e->r[rs].lo, B = e->r[rt].lo;
    int64_t imm = (int16_t)(op & 0xFFFF);
    uint32_t addr = (uint32_t)(A + (uint64_t)imm);
    uint32_t boff = pc + 4 + (uint32_t)(imm << 2);
    uint64_t r = 0;
    int wr = 0;

#define SETRT(v) do { r = (v); rd = rt; wr = 1; } while (0)
#define SETRD(v) do { r = (v); wr = 1; } while (0)
#define BR(cond, likely) do { if (cond) { *branch = 1; *target = boff; } else if (likely) { *skip = 1; } } while (0)
    switch (o) {
    case 0x00: /* SPECIAL */
        switch (op & 63) {
        case 0x00: SETRD(sx32((uint32_t)B << sh)); break;
        case 0x02: SETRD(sx32((uint32_t)B >> sh)); break;
        case 0x03: SETRD(sx32((uint32_t)((int32_t)B >> sh))); break;
        case 0x04: SETRD(sx32((uint32_t)B << (A & 31))); break;
        case 0x06: SETRD(sx32((uint32_t)B >> (A & 31))); break;
        case 0x07: SETRD(sx32((uint32_t)((int32_t)B >> (A & 31)))); break;
        case 0x08: *branch = 1; *target = (uint32_t)A; if (rs == 31 && e->trace) e->depth--; break; /* jr */
        case 0x09: *branch = 1; *target = (uint32_t)A; SETRD(pc + 8); if (e->trace) e->depth++; break; /* jalr */
        case 0x0A: if (B == 0) SETRD(A); break; /* movz */
        case 0x0B: if (B != 0) SETRD(A); break; /* movn */
        case 0x0F: break;                       /* sync */
        case 0x10: SETRD(e->hi); break;
        case 0x11: e->hi = A; break;
        case 0x12: SETRD(e->lo); break;
        case 0x13: e->lo = A; break;
        case 0x14: SETRD(B << (A & 63)); break;
        case 0x16: SETRD(B >> (A & 63)); break;
        case 0x17: SETRD((uint64_t)((int64_t)B >> (A & 63))); break;
        case 0x18: case 0x19: {
            uint64_t p = (op & 1) == 0 ? (uint64_t)((int64_t)(int32_t)A * (int32_t)B) : (uint64_t)(uint32_t)A * (uint32_t)B;
            e->lo = sx32((uint32_t)p);
            e->hi = sx32((uint32_t)(p >> 32));
            SETRD(e->lo);
            break;
        }
        case 0x1A: case 0x1B:
            if ((uint32_t)B != 0) {
                if ((op & 1) == 0) {
                    int32_t x = (int32_t)A, y = (int32_t)B;
                    if (!(x == INT32_MIN && y == -1)) {
                        e->lo = sx32((uint32_t)(x / y));
                        e->hi = sx32((uint32_t)(x % y));
                    }
                } else {
                    e->lo = sx32((uint32_t)A / (uint32_t)B);
                    e->hi = sx32((uint32_t)A % (uint32_t)B);
                }
            }
            break;
        case 0x20: case 0x21: SETRD(sx32((uint32_t)A + (uint32_t)B)); break;
        case 0x22: case 0x23: SETRD(sx32((uint32_t)A - (uint32_t)B)); break;
        case 0x24: SETRD(A & B); break;
        case 0x25: SETRD(A | B); break;
        case 0x26: SETRD(A ^ B); break;
        case 0x27: SETRD(~(A | B)); break;
        case 0x28: SETRD(e->sa); break;
        case 0x29: e->sa = (uint32_t)A; break;
        case 0x2A: SETRD((int64_t)A < (int64_t)B); break;
        case 0x2B: SETRD(A < B); break;
        case 0x2C: case 0x2D: SETRD(A + B); break;
        case 0x2E: case 0x2F: SETRD(A - B); break;
        case 0x38: SETRD(B << sh); break;
        case 0x3A: SETRD(B >> sh); break;
        case 0x3B: SETRD((uint64_t)((int64_t)B >> sh)); break;
        case 0x3C: SETRD(B << (sh + 32)); break;
        case 0x3E: SETRD(B >> (sh + 32)); break;
        case 0x3F: SETRD((uint64_t)((int64_t)B >> (sh + 32))); break;
        case 0x0D: fail(e, "break", op); return;
        case 0x34: if (A == B) { fail(e, "teq trap", op); return; } break;
        case 0x36: if (A != B) { fail(e, "tne trap", op); return; } break;
        default: fail(e, "SPECIAL instruction not modelled", op); return;
        }
        break;
    case 0x01: /* REGIMM */
        switch (rt) {
        case 0x00: BR((int64_t)A < 0, 0); break;
        case 0x01: BR((int64_t)A >= 0, 0); break;
        case 0x02: BR((int64_t)A < 0, 1); break;
        case 0x03: BR((int64_t)A >= 0, 1); break;
        case 0x10: e->r[31].lo = pc + 8; BR((int64_t)A < 0, 0); break;
        case 0x11: e->r[31].lo = pc + 8; BR((int64_t)A >= 0, 0); break;
        default: fail(e, "REGIMM instruction not modelled", op); return;
        }
        break;
    case 0x02: *branch = 1; *target = ((pc + 4) & 0xF0000000u) | ((op & 0x3FFFFFF) << 2); break;
    case 0x03:
        *branch = 1;
        *target = ((pc + 4) & 0xF0000000u) | ((op & 0x3FFFFFF) << 2);
        e->r[31].lo = pc + 8;
        if (e->trace && e->depth++ == 0) {
            e->tracePending = 1;
        }
        break;
    case 0x04: BR(A == B, 0); break;
    case 0x05: BR(A != B, 0); break;
    case 0x06: BR((int64_t)A <= 0, 0); break;
    case 0x07: BR((int64_t)A > 0, 0); break;
    case 0x08: case 0x09: SETRT(sx32((uint32_t)A + (uint32_t)imm)); break;
    case 0x0A: SETRT((int64_t)A < imm); break;
    case 0x0B: SETRT(A < (uint64_t)imm); break;
    case 0x0C: SETRT(A & (op & 0xFFFF)); break;
    case 0x0D: SETRT(A | (op & 0xFFFF)); break;
    case 0x0E: SETRT(A ^ (op & 0xFFFF)); break;
    case 0x0F: SETRT(sx32((op & 0xFFFF) << 16)); break;
    case 0x11:
        if (rs == 8) { /* bc1f / bc1t / bc1fl / bc1tl */
            BR(e->fcc == (int)(rt & 1), rt & 2);
        } else {
            cop1(e, op);
        }
        break;
    case 0x12:
        if (op & (1u << 25)) {
            cop2(e, op);
        } else if (rs == 1) { /* qmfc2 */
            if (rt) {
                memcpy(&e->r[rt], e->vf[rd], 16);
            }
        } else if (rs == 5) { /* qmtc2 */
            if (rd) {
                memcpy(e->vf[rd], &e->r[rt], 16);
            }
        } else if (rs == 2) { /* cfc2 */
            uint32_t v = cop2_ctrl_read(e, rd, op);
            if (rt) {
                e->r[rt].lo = sx32(v);
            }
        } else if (rs == 6) { /* ctc2 */
            cop2_ctrl_write(e, rd, (uint32_t)B, op);
        } else {
            fail(e, "COP2 transfer not modelled", op);
        }
        break;
    case 0x14: BR(A == B, 1); break;
    case 0x15: BR(A != B, 1); break;
    case 0x16: BR((int64_t)A <= 0, 1); break;
    case 0x17: BR((int64_t)A > 0, 1); break;
    case 0x18: case 0x19: SETRT(A + (uint64_t)imm); break;
    case 0x1A: case 0x1B: { /* ldl / ldr */
        uint32_t s = (addr & 7) << 3;
        uint64_t m;
        LOADN(uint64_t, addr & ~7u, m);
        if (o == 0x1A) {
            SETRT((B & (0x00FFFFFFFFFFFFFFull >> s)) | (m << (56 - s)));
        } else {
            SETRT((B & (s == 0 ? 0 : (0xFFFFFFFFFFFFFF00ull << (56 - s)))) | (m >> s));
        }
        break;
    }
    case 0x1C: mmi(e, op); break;
    case 0x1E: { /* lq */
        Gpr g;
        LOADN(Gpr, addr & ~15u, g);
        if (rt) {
            e->r[rt] = g;
        }
        break;
    }
    case 0x1F: { /* sq */
        uint8_t *p_ = wptr(e, addr & ~15u, 16);
        if (p_ == NULL) {
            fail(e, "store outside memory", op);
            return;
        }
        memcpy(p_, &e->r[rt], 16);
        break;
    }
    case 0x20: { int8_t v; LOADN(int8_t, addr, v); SETRT((uint64_t)(int64_t)v); break; }
    case 0x21: { int16_t v; LOADN(int16_t, addr, v); SETRT((uint64_t)(int64_t)v); break; }
    case 0x22: case 0x26: { /* lwl / lwr */
        uint32_t s = (addr & 3) << 3, m;
        LOADN(uint32_t, addr & ~3u, m);
        if (o == 0x22) {
            SETRT(sx32(((uint32_t)B & (0x00FFFFFFu >> s)) | (m << (24 - s))));
        } else if (s == 0) {
            SETRT(sx32(m));
        } else {
            SETRT((B & 0xFFFFFFFF00000000ull) | (((uint32_t)B & (0xFFFFFF00u << (24 - s))) | (m >> s)));
        }
        break;
    }
    case 0x23: { uint32_t v; LOADN(uint32_t, addr, v); SETRT(sx32(v)); break; }
    case 0x24: { uint8_t v; LOADN(uint8_t, addr, v); SETRT(v); break; }
    case 0x25: { uint16_t v; LOADN(uint16_t, addr, v); SETRT(v); break; }
    case 0x27: { uint32_t v; LOADN(uint32_t, addr, v); SETRT(v); break; }
    case 0x28: STOREN(uint8_t, addr, B); break;
    case 0x29: STOREN(uint16_t, addr, B); break;
    case 0x2A: case 0x2E: { /* swl / swr */
        uint32_t s = (addr & 3) << 3, m;
        LOADN(uint32_t, addr & ~3u, m);
        if (o == 0x2A) {
            STOREN(uint32_t, addr & ~3u, (m & (0xFFFFFF00u << s)) | ((uint32_t)B >> (24 - s)));
        } else {
            STOREN(uint32_t, addr & ~3u, (m & (0x00FFFFFFu >> (24 - s))) | ((uint32_t)B << s));
        }
        break;
    }
    case 0x2B: STOREN(uint32_t, addr, B); break;
    case 0x2C: case 0x2D: { /* sdl / sdr */
        uint32_t s = (addr & 7) << 3;
        uint64_t m;
        LOADN(uint64_t, addr & ~7u, m);
        if (o == 0x2C) {
            STOREN(uint64_t, addr & ~7u, (m & (s == 56 ? 0 : (0xFFFFFFFFFFFFFF00ull << s))) | (B >> (56 - s)));
        } else {
            STOREN(uint64_t, addr & ~7u, (m & (s == 0 ? 0 : (0x00FFFFFFFFFFFFFFull >> (56 - s)))) | (B << s));
        }
        break;
    }
    case 0x2F: break; /* cache */
    case 0x31: { uint32_t v; LOADN(uint32_t, addr, v); e->f[rt] = v; break; } /* lwc1 */
    case 0x33: break;                                                            /* pref */
    case 0x36: { uint32_t v[4]; LOADN(uint32_t[4], addr & ~15u, v); if (rt) memcpy(e->vf[rt], v, 16); break; } /* lqc2 */
    case 0x37: { uint64_t v; LOADN(uint64_t, addr, v); SETRT(v); break; }
    case 0x39: STOREN(uint32_t, addr, e->f[rt]); break; /* swc1 */
    case 0x3E: { /* sqc2 */
        uint8_t *p_ = wptr(e, addr & ~15u, 16);
        if (p_ == NULL) {
            fail(e, "store outside memory", op);
            return;
        }
        memcpy(p_, e->vf[rt], 16);
        break;
    }
    case 0x3F: STOREN(uint64_t, addr, B); break;
    default: fail(e, "instruction not modelled", op); return;
    }
    if (wr && rd != 0) {
        e->r[rd].lo = r;
        if (o == 0x00 || o >= 0x08) {
            /* the upper 64 bits of a GPR keep their value after ordinary instructions on the EE [I: undefined in
               the manual, PCSX2 keeps them]; lq / qmfc2 / MMI set them above */
        }
    }
    e->r[0].lo = e->r[0].hi = 0;
#undef SETRT
#undef SETRD
#undef BR
}

static uint32_t fetch(Ee *e, uint32_t pc) {
    if (pc < sRo[0].lo || pc + 4 > sRo[0].hi || (pc & 3)) {
        fail(e, "jump outside .text", 0);
        return 0;
    }
    return rd32le(sImg + (pc - sImgLo));
}

static void run(Ee *e) {
    while (e->stop == 0) {
        uint32_t pc = e->pc, op, target = 0;
        int branch = 0, skip = 0;

        if (pc == RET_MAGIC) {
            e->stop = 1;
            break;
        }
        if (++e->steps > MAX_STEPS) {
            fail(e, "too many steps", 0);
            break;
        }
        op = fetch(e, pc);
        if (e->stop) {
            break;
        }
        if ((op >> 26) == 0 && (op & 63) == 0x0C) {
            fail(e, "syscall", op);
            break;
        }
        exec(e, op, pc, &branch, &target, &skip);
        if (e->stop) {
            break;
        }
        if (branch) {
            uint32_t dop = fetch(e, pc + 4);
            int b2 = 0, s2 = 0;
            uint32_t t2 = 0;
            if (e->stop) {
                break;
            }
            e->pc = pc + 4;
            exec(e, dop, pc + 4, &b2, &t2, &s2);
            e->steps++;
            e->pc = target;
            if (e->tracePending) {
                const uint32_t *v = (const uint32_t *)(uintptr_t)(uint32_t)e->r[6].lo;
                e->tracePending = 0;
                printf("ee-call %08X %08X %08X %08X %08X %08X f12 %08X f13 %08X", target, (uint32_t)e->r[4].lo,
                       (uint32_t)e->r[5].lo, (uint32_t)e->r[6].lo, (uint32_t)e->r[7].lo, (uint32_t)e->r[8].lo,
                       e->f[12], e->f[13]);
                if (target == 0x34A7A8 && rptr(e, (uint32_t)e->r[6].lo, 16) != NULL) {
                    v = (const uint32_t *)rptr(e, (uint32_t)e->r[6].lo, 16);
                    printf(" v %08X %08X %08X %08X", v[0], v[1], v[2], v[3]);
                }
                printf("\n");
            }
        } else if (skip) {
            e->pc = pc + 8;
        } else {
            e->pc = pc + 4;
        }
    }
}

/* ------------------------------------------------------------------------------------------- the check */

static uint32_t sFromVb, sMaxCalls = 1;
static int sEnabled = -1;
static const char *sOnly;
static uint8_t sStack[1 << 20] __attribute__((aligned(16)));

typedef struct {
    const char *name;
    unsigned calls;
} Count;
static Count sCounts[64];

static unsigned *count_of(const char *name) {
    int k;
    for (k = 0; k < 64; k++) {
        if (sCounts[k].name == NULL) {
            sCounts[k].name = name;
        }
        if (strcmp(sCounts[k].name, name) == 0) {
            return &sCounts[k].calls;
        }
    }
    return &sCounts[63].calls;
}

static int wanted(const char *name) {
    const char *p;
    size_t n = strlen(name);

    if (sEnabled < 0) {
        const char *v = getenv("TTT_DIFF");
        sEnabled = v != NULL && *v != 0;
        if (sEnabled) {
            char *end;
            sFromVb = (uint32_t)strtoul(v, &end, 0);
            if (*end == ':') {
                sMaxCalls = (uint32_t)strtoul(end + 1, NULL, 0);
            }
            sOnly = getenv("TTT_DIFF_ONLY");
        }
    }
    if (!sEnabled || Port_VBlankCount() < sFromVb) {
        return 0;
    }
    if (sOnly != NULL) {
        for (p = sOnly; (p = strstr(p, name)) != NULL; p += n) {
            if ((p == sOnly || p[-1] == ',') && (p[n] == 0 || p[n] == ',')) {
                break;
            }
        }
        if (p == NULL) {
            return 0;
        }
    }
    return *count_of(name) < sMaxCalls;
}

typedef uint32_t (*FnR8)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* Prints the differing words of [lo, hi) between the C result c and the interpreter's i, as runs. */
static unsigned report(const char *name, uint32_t lo, const uint8_t *c, const uint8_t *i, uint32_t size, uint32_t skipLo,
                       uint32_t skipHi, unsigned shown) {
    uint32_t k, n = 0;

    for (k = 0; k < size; k += 4) {
        uint32_t a = lo + k, cv, iv, run = 0;
        if (a >= skipLo && a < skipHi) {
            continue;
        }
        memcpy(&cv, c + k, 4);
        memcpy(&iv, i + k, 4);
        if (cv == iv) {
            continue;
        }
        while (k + run * 4 < size) {
            uint32_t x, y;
            memcpy(&x, c + k + run * 4, 4);
            memcpy(&y, i + k + run * 4, 4);
            if (x == y) {
                break;
            }
            run++;
        }
        if (shown < 400) {
            uint32_t j;
            printf("ee-diff %s %08X +%u words: c/orig", name, a, run);
            for (j = 0; j < run && j < 8; j++) {
                uint32_t x, y;
                memcpy(&x, c + k + j * 4, 4);
                memcpy(&y, i + k + j * 4, 4);
                printf(" %08X/%08X", x, y);
            }
            printf(run > 8 ? " ...\n" : "\n");
        }
        shown++;
        n += run;
        k += run * 4 - 4;
    }
    return n;
}

/* Calls the original code at addr with the arguments of sig (see Port_DiffCall) on the interpreter's own stack. */
static void ee_call(Ee *e, uint32_t addr, const char *sig, const uint32_t *args) {
    uint32_t k, n = (uint32_t)strcspn(sig, ":");
    int ia = 0, fa = 0;

    e->vf[0][0] = e->vf[0][1] = e->vf[0][2] = 0;
    e->vf[0][3] = 0x3F800000u;
    for (k = 0; k < n && k < 8; k++) {
        if (sig[k] == 'f') {
            e->f[12 + fa++] = args[k];
        } else {
            e->r[ia < 4 ? 4 + ia : 8 + (ia - 4)].lo = sx32(args[k]);
            ia++;
        }
    }
    e->r[28].lo = sGp;
    e->r[29].lo = (uint32_t)(uintptr_t)(sStack + sizeof sStack - 64);
    e->r[31].lo = RET_MAGIC;
    e->pc = addr;
    run(e);
}

/* The hook (PORT_DIFF in port_compat.h): calls fn(args) as the port would, and when TTT_DIFF asks for it compares the
   result with the original code at addr. sig: one letter per argument, 'i' integer / pointer register, 'f' float
   register (the bits, passed as the soft-float word). */
uint32_t Port_DiffCall(const char *name, uint32_t addr, void *fn, const char *sig, ...) {
    uint32_t args[8] = {0}, n = (uint32_t)strcspn(sig, ":"), k, rc;
    char ret = sig[n] == ':' ? sig[n + 1] : 0;
    uint8_t *s0, *sc, *spr0, *sprc;
    uint32_t vf0[32][4];
    uint32_t frame = (uint32_t)(uintptr_t)__builtin_frame_address(0);
    uint32_t skipLo, skipHi = frame + 8;
    va_list ap;
    Ee *e;
    unsigned words;

    va_start(ap, sig);
    for (k = 0; k < n && k < 8; k++) {
        args[k] = va_arg(ap, uint32_t);
    }
    va_end(ap);
    if (getenv("TTT_DIFF_TRACE") != NULL && Port_VBlankCount() >= sFromVb && sEnabled > 0) {
        printf("c-call %s", name);
        for (k = 0; k < n && k < 8; k++) {
            printf(" %08X", args[k]);
        }
        if (strcmp(name, "d_ApplyMatrix") == 0) {
            const uint32_t *v = (const uint32_t *)(uintptr_t)args[2];
            printf(" v %08X %08X %08X %08X", v[0], v[1], v[2], v[3]);
        }
        printf("\n");
    }
    if (!wanted(name) || load_elf() < 0) {
        return ((FnR8)fn)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
    }
    (*count_of(name))++;
    /* not restored nor compared: the port's own stack frames, from this one down to the end of .bss */
    skipLo = sBssEnd != 0 && sBssEnd < frame ? sBssEnd : frame - 0x10000u;
    s0 = malloc(RAM_HI - RAM_LO);
    sc = malloc(RAM_HI - RAM_LO);
    spr0 = malloc(SPR_SIZE);
    sprc = malloc(SPR_SIZE);
    e = calloc(1, sizeof *e);
    if (s0 == NULL || sc == NULL || spr0 == NULL || sprc == NULL || e == NULL) {
        printf("ee-diff %s: out of memory\n", name);
        return ((FnR8)fn)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
    }
    memcpy(s0, (void *)(uintptr_t)RAM_LO, RAM_HI - RAM_LO);
    memcpy(spr0, (void *)(uintptr_t)SPR_LO, SPR_SIZE);
    memcpy(vf0, gPortVu0Vf, sizeof vf0);
    /* the port's C */
    rc = ((FnR8)fn)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
    memcpy(sc, (void *)(uintptr_t)RAM_LO, RAM_HI - RAM_LO);
    memcpy(sprc, (void *)(uintptr_t)SPR_LO, SPR_SIZE);
    /* the original, from the same memory (the port's own stack frames below this one excepted) */
    memcpy((void *)(uintptr_t)RAM_LO, s0, skipLo - RAM_LO);
    memcpy((void *)(uintptr_t)skipHi, s0 + (skipHi - RAM_LO), RAM_HI - skipHi);
    memcpy((void *)(uintptr_t)SPR_LO, spr0, SPR_SIZE);
    memcpy(e->vf, vf0, sizeof vf0);
    e->trace = getenv("TTT_DIFF_TRACE") != NULL;
    ee_call(e, addr, sig, args);
    if (e->stop >= 0 && ret != 0) {
        uint32_t ri = ret == 'f' ? e->f[0] : (uint32_t)e->r[2].lo;
        if (ri != rc) {
            printf("ee-diff %s: result %08X, original %08X\n", name, rc, ri);
        }
    }
    if (e->stop < 0) {
        printf("ee-diff %s (VBlank %u): original not run to its end: %s after %llu steps\n", name,
               (unsigned)Port_VBlankCount(), e->why, (unsigned long long)e->steps);
    } else {
        words = report(name, RAM_LO, sc, (const uint8_t *)(uintptr_t)RAM_LO, RAM_HI - RAM_LO, skipLo, skipHi, 0);
        words += report(name, SPR_LO, sprc, (const uint8_t *)(uintptr_t)SPR_LO, SPR_SIZE, 0, 0, 0);
        printf("ee-diff %s (VBlank %u): %s, %u words differ, %llu instructions; args", name,
               (unsigned)Port_VBlankCount(), words ? "DIFFERS" : "same", words, (unsigned long long)e->steps);
        for (k = 0; k < n && k < 8; k++) {
            printf(" %08X", args[k]);
        }
        printf("\n");
    }
    /* the port continues with the C result */
    memcpy((void *)(uintptr_t)RAM_LO, sc, skipLo - RAM_LO);
    memcpy((void *)(uintptr_t)skipHi, sc + (skipHi - RAM_LO), RAM_HI - skipHi);
    memcpy((void *)(uintptr_t)SPR_LO, sprc, SPR_SIZE);
    fflush(stdout);
    free(s0);
    free(sc);
    free(spr0);
    free(sprc);
    free(e);
    return rc;
}

/* ------------------------------------------------------------------------------------------- self-test */

/* TTT_SELFTEST=ee: the interpreter against port code known to behave like the original (libvu0 as audited in T29
   batch 4, matched game C, newlib's libm, the hand-replaced VU0 rotation builders), on fixed inputs. A difference
   means the interpreter (or that port code) is wrong. Needs the executable (TTT_DIFF_ELF); without it: SKIP. */
void sceVu0MulMatrix(), sceVu0InversMatrix(), sceVu0ApplyMatrix(), sceVu0OuterProduct(), func_0034A868();
void sceVu0TransposeMatrix(), sceVu0CopyMatrix(), sceVu0ScaleVector(), sceVu0AddVector(), sceVu0SubVector();
void sceVu0RotMatrixX(), sceVu0RotMatrixY(), sceVu0RotMatrixZ(), sceVu0RotMatrix(), sceVu0TransMatrix();
void sceVu0InnerProduct(), sceVu0UnitMatrix(), func_0034A570(), func_0034A710(), func_00348988();
void func_00349D70(), func_00349EE0(), func_0034A050(), func_0034A0D0();
uint32_t L_atan2f(uint32_t, uint32_t) __asm__("atan2f");
uint32_t L_asinf(uint32_t) __asm__("asinf");
uint32_t L_fmodf(uint32_t, uint32_t) __asm__("fmodf");
uint32_t L_ieee_sqrtf(uint32_t) __asm__("__ieee754_sqrtf");
uint32_t L_ieee_asinf(uint32_t) __asm__("__ieee754_asinf");

enum { OUT = 0, MA = 16, MB = 32, VV = 48, AN = 52, BUFW = 64 };
static uint32_t sIn[BUFW] = {
    /* OUT: garbage, kept where a routine does not write */
    0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u, 0x66666666u, 0x77777777u, 0x3E800000u,
    0x40000000u, 0xC0000000u, 0x3F000000u, 0x3E99999Au, 0x41200000u, 0x42C80000u, 0xC1F00000u, 0x3F800000u,
    /* MA: rotation about z by 30 degrees, a stray w, a translation */
    0x3F5DB3D7u, 0x3F000000u, 0x00000000u, 0x3E99999Au, 0xBF000000u, 0x3F5DB3D7u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u, 0x42C80000u, 0xC2480000u, 0x41200000u, 0x3F800000u,
    /* MB: rotation about x by 45 degrees, translation */
    0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3F3504F3u, 0x3F3504F3u, 0x00000000u,
    0x00000000u, 0xBF3504F3u, 0x3F3504F3u, 0x00000000u, 0x41A00000u, 0x40A00000u, 0xC1200000u, 0x3F800000u,
    /* VV, AN (angles) */
    0x40400000u, 0xC0800000u, 0x41400000u, 0x3F800000u, 0x3F000000u, 0xBE99999Au, 0x3FC00000u, 0x00000000u,
    0x3F333333u, 0xBF4CCCCDu, 0x3E4CCCCDu, 0x3F800000u, 0, 0, 0, 0};

static uint32_t sBufC[BUFW] __attribute__((aligned(16)));
static uint32_t sBufI[BUFW] __attribute__((aligned(16)));

typedef struct {
    const char *name;
    uint32_t addr;
    void *fn;
    const char *sig;
    char ret; /* 0 none, 'f' float in $f0, 'i' integer in $v0 */
    int arg[4]; /* -1: a float constant from fl[], else a word offset into the buffer */
    uint32_t fl[4];
} EeCase;

/* Known divergences of port code the self-test reports without failing (KNOWN lines): the libm of
   third_party/newlib_libm has the newer fdlibm constants pio2_hi 0x3FC90FDB / pio2_lo -4.37e-8 in ef_asin.c (the
   executable's newlib: 0x3FC90FDA / 0x33A22168), and atan2f's |y / x| > 2^26 result differs by one unit the same
   way (pi_o_2 + pi_lo): float results that differ by one or two units in the last place there. */
static int known_libm(const EeCase *t, uint32_t rc, uint32_t ri) {
    uint32_t d = rc > ri ? rc - ri : ri - rc;
    return (strcmp(t->name, "asinf") == 0 || strcmp(t->name, "__ieee754_asinf") == 0 ||
            (strcmp(t->name, "atan2f") == 0 && (rc & 0x7FFFFFFFu) == 0x3FC90FDAu)) &&
           d <= 2 && (rc & SF_SIGN) == (ri & SF_SIGN);
}

static unsigned sKnown;

static int ee_case(const EeCase *t) {
    uint32_t ac[8] = {0}, ai[8] = {0}, rc, ri, k, n = (uint32_t)strlen(t->sig), bad = 0;
    Ee *e = calloc(1, sizeof *e);

    memcpy(sBufC, sIn, sizeof sIn);
    memcpy(sBufI, sIn, sizeof sIn);
    for (k = 0; k < n; k++) {
        if (t->arg[k] < 0) {
            ac[k] = ai[k] = t->fl[k];
        } else {
            ac[k] = (uint32_t)(uintptr_t)(sBufC + t->arg[k]);
            ai[k] = (uint32_t)(uintptr_t)(sBufI + t->arg[k]);
        }
    }
    memcpy(e->vf, gPortVu0Vf, sizeof e->vf);
    rc = ((FnR8)t->fn)(ac[0], ac[1], ac[2], ac[3], ac[4], ac[5], ac[6], ac[7]);
    ee_call(e, t->addr, t->sig, ai);
    if (e->stop < 0) {
        printf("ee: %s: original not run to its end: %s\n", t->name, e->why);
        free(e);
        return 1;
    }
    ri = t->ret == 'f' ? e->f[0] : (uint32_t)e->r[2].lo;
    if (t->ret != 0 && rc != ri) {
        if (known_libm(t, rc, ri)) {
            if (sKnown++ < 4) {
                printf("ee: KNOWN %s: result %08X, original %08X (third_party libm constants)\n", t->name, rc, ri);
            }
        } else {
            printf("ee: %s: result %08X, original %08X\n", t->name, rc, ri);
            bad = 1;
        }
    }
    for (k = 0; k < BUFW; k++) {
        if (sBufC[k] != sBufI[k]) {
            printf("ee: %s: word %u: %08X, original %08X\n", t->name, k, sBufC[k], sBufI[k]);
            bad = 1;
        }
    }
    free(e);
    return (int)bad;
}

/* A float of either sign with exponent in [lo, hi] from the generator state *x. */
static uint32_t rnd_float(uint32_t *x, uint32_t lo, uint32_t hi) {
    uint32_t r;
    *x = *x * 1664525u + 1013904223u;
    r = *x;
    *x = *x * 1664525u + 1013904223u;
    return (r & SF_SIGN) | ((lo + (*x >> 8) % (hi - lo + 1)) << 23) | (r & 0x7FFFFF);
}

/* The same routines on generated inputs: angles, vectors and matrices of the sizes the game uses. */
static unsigned ee_fuzz(void) {
    static const EeCase base[] = {
        {"atan2f", 0x407B28, L_atan2f, "ff", 'f', {-1, -1}},
        {"sceVu0Normalize", 0x34A868, func_0034A868, "ii", 0, {OUT, VV}},
        {"func_00349D70", 0x349D70, func_00349D70, "ii", 0, {MA, AN}},
        {"func_00349EE0", 0x349EE0, func_00349EE0, "ii", 0, {MA, AN}},
        {"sceVu0MulMatrix", 0x34A7D8, sceVu0MulMatrix, "iii", 0, {OUT, MA, MB}},
        {"sceVu0InversMatrix", 0x34A8E8, sceVu0InversMatrix, "ii", 0, {OUT, MA}},
        {"func_0034A570", 0x34A570, func_0034A570, "f", 'f', {-1}},
        {"func_0034A050", 0x34A050, func_0034A050, "if", 0, {MA, -1}},
        {"func_0034A0D0", 0x34A0D0, func_0034A0D0, "if", 0, {MB, -1}},
    };
    uint32_t x = 12345, k, j, w;
    unsigned fails = 0, per[16] = {0};
    static uint32_t saved[BUFW];

    memcpy(saved, sIn, sizeof saved);
    for (k = 0; k < 4000; k++) {
        EeCase t = base[k % (sizeof base / sizeof base[0])];
        uint32_t *in = (uint32_t *)sIn;
        for (w = 0; w < BUFW; w++) {
            in[w] = rnd_float(&x, 118, 133);
        }
        for (w = 0; w < 4; w++) {
            in[AN + w] = rnd_float(&x, 110, 128);
        }
        t.fl[0] = rnd_float(&x, 100, 135);
        t.fl[1] = rnd_float(&x, 100, 135);
        if (t.arg[1] == -1 && t.sig[0] == 'i') {
            t.fl[1] = rnd_float(&x, 110, 129);
        }
        j = k % (sizeof base / sizeof base[0]);
        if (ee_case(&t) != 0 && per[j]++ < 2) {
            printf("ee: (generated case %u)\n", (unsigned)k);
        }
    }
    memcpy((uint32_t *)sIn, saved, sizeof saved);
    for (k = 0; k < sizeof base / sizeof base[0]; k++) {
        if (per[k] != 0) {
            printf("ee: %s: %u of %u generated cases differ\n", base[k].name, per[k],
                   (unsigned)(4000 / (sizeof base / sizeof base[0])));
            fails++;
        }
    }
    return fails;
}

int Port_EeSelfTest(void) {
    static const EeCase cases[] = {
        {"sceVu0MulMatrix", 0x34A7D8, sceVu0MulMatrix, "iii", 0, {OUT, MA, MB}},
        {"sceVu0MulMatrix (in place)", 0x34A7D8, sceVu0MulMatrix, "iii", 0, {MA, MA, MB}},
        {"sceVu0InversMatrix", 0x34A8E8, sceVu0InversMatrix, "ii", 0, {OUT, MA}},
        {"sceVu0ApplyMatrix", 0x34A7A8, sceVu0ApplyMatrix, "iii", 0, {OUT, MA, VV}},
        {"sceVu0OuterProduct", 0x34A820, sceVu0OuterProduct, "iii", 0, {OUT, VV, MA}},
        {"sceVu0InnerProduct", 0x34A840, sceVu0InnerProduct, "ii", 'f', {VV, MB + 4}},
        {"sceVu0Normalize", 0x34A868, func_0034A868, "ii", 0, {OUT, VV}},
        {"sceVu0TransposeMatrix", 0x34A8A0, sceVu0TransposeMatrix, "ii", 0, {OUT, MA}},
        {"sceVu0CopyMatrix", 0x34AA60, sceVu0CopyMatrix, "ii", 0, {OUT, MA}},
        {"sceVu0UnitMatrix", 0x34AAC8, sceVu0UnitMatrix, "i", 0, {OUT}},
        {"sceVu0ScaleVector", 0x34AA08, sceVu0ScaleVector, "iif", 0, {OUT, VV, -1}, {0, 0, 0xBFC00000u}},
        {"sceVu0AddVector", 0x34A9C0, sceVu0AddVector, "iii", 0, {OUT, VV, MA}},
        {"sceVu0SubVector", 0x34A9D8, sceVu0SubVector, "iii", 0, {OUT, VV, MA}},
        {"sceVu0TransMatrix", 0x34AA20, sceVu0TransMatrix, "iii", 0, {OUT, MA, VV}},
        {"sceVu0RotMatrixX", 0x34AC10, sceVu0RotMatrixX, "iif", 0, {OUT, MA, -1}, {0, 0, 0x3F333333u}},
        {"sceVu0RotMatrixY", 0x34ACB8, sceVu0RotMatrixY, "iif", 0, {OUT, MA, -1}, {0, 0, 0xC0200000u}},
        {"sceVu0RotMatrixZ", 0x34AB68, sceVu0RotMatrixZ, "iif", 0, {OUT, MB, -1}, {0, 0, 0x40400000u}},
        {"sceVu0RotMatrix", 0x34AD60, sceVu0RotMatrix, "iii", 0, {OUT, MA, AN}},
        {"atan2f", 0x407B28, L_atan2f, "ff", 'f', {-1, -1}, {0x3F000000u, 0xBF4CCCCDu}},
        {"atan2f (2)", 0x407B28, L_atan2f, "ff", 'f', {-1, -1}, {0xC2C80000u, 0x3DCCCCCDu}},
        {"__ieee754_sqrtf", 0x40A388, L_ieee_sqrtf, "f", 'f', {-1}, {0x3E95F61Au}},
        {"__ieee754_asinf", 0x408B18, L_ieee_asinf, "f", 'f', {-1}, {0xBF3504F3u}},
        {"asinf", 0x407A28, L_asinf, "f", 'f', {-1}, {0xBF3504F3u}},
        {"fmodf", 0x407C50, L_fmodf, "ff", 'f', {-1, -1}, {0x41A00000u, 0x40C90FDBu}},
        {"func_0034A570", 0x34A570, func_0034A570, "f", 'f', {-1}, {0xC1200000u}},
        {"func_0034A710", 0x34A710, func_0034A710, "ii", 0, {OUT, MA}},
        {"func_00348988", 0x348988, func_00348988, "ii", 0, {OUT, MA}},
        {"func_00349D70", 0x349D70, func_00349D70, "ii", 0, {MA, AN}},
        {"func_00349EE0", 0x349EE0, func_00349EE0, "ii", 0, {MA, AN}},
        {"func_0034A050", 0x34A050, func_0034A050, "if", 0, {MA, -1}, {0, 0x3FC90FDBu}},
        {"func_0034A0D0", 0x34A0D0, func_0034A0D0, "if", 0, {MB, -1}, {0, 0xBF800000u}},
    };
    unsigned k, fails = 0;

    if (load_elf() < 0) {
        printf("ttt: ee self-test SKIP (no executable)\n");
        return 0;
    }
    for (k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        fails += (unsigned)ee_case(&cases[k]);
    }
    fails += ee_fuzz();
    printf(fails ? "ttt: ee self-test FAILED (%u of %u)\n" : "ttt: ee self-test OK (%u of %u)",
           fails ? fails : (unsigned)(sizeof cases / sizeof cases[0]), (unsigned)(sizeof cases / sizeof cases[0]));
    if (!fails) {
        printf(", %u known libm results, 4000 generated cases\n", sKnown);
    }
    return (int)fails;
}
