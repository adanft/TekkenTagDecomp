/*
 * TTT_SELFTEST=vu1, second part: the predecoded interpreter (Vu_Run with a VuOp cache, vu_micro.c) against the
 * reference one (Vu_RunRef) on random programs, and the SSE arithmetic (vu_simd_inl.h: add, subtract, multiply,
 * max, min on four lanes, divide and square root) against the scalar model (softfloat_ps2_inl.h).
 *
 * The programs mix every implemented upper and lower instruction (and some unimplemented ones) with random fields,
 * dest masks, broadcasts, the I bit, forward branches with their delay slots, flag reads, Q / P, XTOP / XITOP and
 * XGKICK (its packets are hashed); the registers and data memory start random, with operands of every exponent class
 * (0, 255, the edges of the range). Three programs run in a row on the same pair of units, half the time with the
 * end-of-program settling off (VU0's way: results in flight carry into the next program), and between programs some
 * instructions are rewritten in place, which the cache must notice. Both units must end each program in the same
 * state (Vu_SameState) with the same packets.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "softfloat_ps2_inl.h"
#include "vu_micro.h"
#include "vu_simd_inl.h"

static uint32_t sSeed;
static uint32_t rnd(void) {
    sSeed ^= sSeed << 13;
    sSeed ^= sSeed >> 17;
    sSeed ^= sSeed << 5;
    return sSeed;
}

/* an operand: any bits, or a float of an edge exponent class */
static uint32_t operand(void) {
    uint32_t r = rnd();
    switch (rnd() & 7) {
    case 0: return r & 0x807FFFFFu;                                /* exponent 0 */
    case 1: return r | 0x7F800000u;                                /* exponent 255 */
    case 2: return (r & 0x807FFFFFu) | ((1 + (rnd() & 1)) << 23);  /* the smallest */
    case 3: return (r & 0x807FFFFFu) | ((253 + (rnd() & 1)) << 23); /* the largest */
    case 4: return r & 0xFFFFu;                                    /* small integers (ITOF) */
    default: return (r & 0x80FFFFFFu) | ((110 + rnd() % 35) << 23); /* ordinary */
    }
}

/* ------------------------------------------------------------------------------------------- lane operations */

static int lanes(unsigned long *count) {
    unsigned long n, bad = 0;
    uint32_t csr = _mm_getcsr();
    int c;

    _mm_setcsr(V4_MXCSR);
    for (n = 0; n < 100000; n++) {
        uint32_t a[4], b[4], r[6][4];
        for (c = 0; c < 4; c++) {
            a[c] = operand();
            b[c] = (rnd() & 3) == 0 ? a[c] ^ (rnd() & 0x80000000u) ^ (rnd() & 3) : operand();
        }
        V4_STORE(r[0], v4_add(V4_LOAD(a), V4_LOAD(b)));
        V4_STORE(r[1], v4_sub(V4_LOAD(a), V4_LOAD(b)));
        V4_STORE(r[2], v4_mul(V4_LOAD(a), V4_LOAD(b)));
        V4_STORE(r[3], v4_max(V4_LOAD(a), V4_LOAD(b)));
        V4_STORE(r[4], v4_min(V4_LOAD(a), V4_LOAD(b)));
        V4_STORE(r[5], v4_add(V4_LOAD(a), V4_LOAD(a)));
        for (c = 0; c < 4; c++) {
            uint32_t want[6], got;
            int k;
            want[0] = sf_vadd(a[c], b[c]);
            want[1] = sf_vadd(a[c], b[c] ^ SF_SIGN);
            want[2] = sf_vmul(a[c], b[c]);
            want[3] = sf_key(a[c]) >= sf_key(b[c]) ? a[c] : b[c];
            want[4] = sf_key(a[c]) <= sf_key(b[c]) ? a[c] : b[c];
            want[5] = sf_vadd(a[c], a[c]);
            for (k = 0; k < 6; k++) {
                if (r[k][c] != want[k] && bad++ < 5) {
                    printf("ttt: vu fast: lane operation %d of %08X, %08X: %08X, the model %08X\n", k, a[c], b[c],
                           r[k][c], want[k]);
                }
            }
            *count += 6;
            got = v1_div(a[c], b[c]); /* the divide and square root of DIV / SQRT / RSQRT and the EFU */
            if (got != sf_div_core(a[c], b[c], 0) && bad++ < 5) {
                printf("ttt: vu fast: divide %08X / %08X: %08X, the model %08X\n", a[c], b[c], got,
                       sf_div_core(a[c], b[c], 0));
            }
            got = v1_sqrt(a[c]);
            if (got != sf_sqrt_core(a[c], 0) && bad++ < 5) {
                printf("ttt: vu fast: square root of %08X: %08X, the model %08X\n", a[c], got, sf_sqrt_core(a[c], 0));
            }
            *count += 2;
        }
    }
    _mm_setcsr(csr);
    return (int)(bad > 0x7FFFFFFF ? 0x7FFFFFFF : bad);
}

/* ------------------------------------------------------------------------------------------- random programs */

#define PROG 48 /* instructions per program */
static uint8_t sMicro[0x4000];
static uint8_t sData[2][0x4000];
static uint32_t sVf[2][32][4];
static uint16_t sVi[2][16];
static uint32_t sAcc[2][4], sQ[2];
static VuOp sOps[0x800];
static uint64_t sHash[2];

static void hash_kick(int k, uint32_t qw) {
    const uint8_t *m = sData[k] + (qw & 0x3FF) * 16;
    uint64_t w[2];
    memcpy(w, m, 16);
    sHash[k] = (sHash[k] ^ w[0] ^ qw) * 0x100000001B3ull + w[1];
}
static void kick0(uint32_t qw) { hash_kick(0, qw); }
static void kick1(uint32_t qw) { hash_kick(1, qw); }

static uint32_t upper(void) {
    static const uint8_t kSpecial[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                       0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
                                       0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
                                       0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x33};
    uint32_t fields = rnd() & 0x01FFFFC0u, k = rnd() % 16; /* dest, ft, fs, fd */

    if (k == 0) {
        return 0x000002FFu | (rnd() & 0x38000000u); /* NOP (with any M / D / T bits) */
    }
    if (k < 9) {
        return fields | (rnd() % 0x30);
    }
    if (k == 9) {
        return fields | (0x30 + rnd() % 12); /* not implemented */
    }
    k = kSpecial[rnd() % sizeof kSpecial];
    return (fields & ~0x7C3u) | ((k >> 2) << 6) | (k & 3) | 0x3C;
}

static uint32_t lower(uint32_t pc) {
    static const uint8_t kOps[] = {0x00, 0x01, 0x04, 0x05, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x18, 0x1A, 0x1B,
                                   0x1C, 0x20, 0x21, 0x28, 0x29, 0x2C, 0x2D, 0x2E, 0x2F, 0x40, 0x40, 0x40, 0x40,
                                   0x40, 0x40, 0x40, 0x40, 0x03};
    static const uint8_t kSpecial[] = {0x30, 0x31, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E,
                                       0x3F, 0x64, 0x68, 0x69, 0x6C, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x78,
                                       0x79, 0x7A, 0x7B, 0x7D, 0x7E};
    static const uint8_t kInt[] = {0x30, 0x31, 0x32, 0x34, 0x35, 0x36};
    uint32_t op = kOps[rnd() % sizeof kOps], lo = rnd() & 0x01FFFFFFu, room = PROG - 1 - pc;

    if (rnd() % 8 == 0) {
        return 0x8000033Cu; /* NOP */
    }
    if (op >= 0x20 && op < 0x30) { /* a forward branch inside the program (it ends on the E bit in any case) */
        lo = (lo & ~0x7FFu) | (room > 1 ? rnd() % room : 0);
    }
    if (op == 0x40) {
        if (rnd() % 4 == 0) {
            return (0x40u << 25) | (lo & ~0x3Fu) | kInt[rnd() % sizeof kInt];
        }
        op = kSpecial[rnd() % sizeof kSpecial];
        return (0x40u << 25) | (lo & ~0x7FFu) | ((op >> 2) << 6) | (op & 3) | 0x3C;
    }
    return (op << 25) | lo;
}

static void put(uint32_t pc, uint32_t lo, uint32_t hi) {
    memcpy(sMicro + pc * 8, &lo, 4);
    memcpy(sMicro + pc * 8 + 4, &hi, 4);
}

static void program(uint32_t base) {
    uint32_t k;
    for (k = 0; k < PROG; k++) {
        uint32_t hi = upper(), lo = lower(k);
        if (rnd() % 10 == 0) {
            hi |= 0x80000000u; /* I bit: the lower word is I */
            lo = operand();
        }
        put(base + k, lo, hi);
    }
    put(base + PROG - 2, 0x8000033Cu, 0x400002FFu); /* E bit; its delay slot is the last one */
}

int Vu_FastSelfTest(void) {
    VuCore u[2];
    unsigned long ops = 0, instr = 0, programs = 0;
    uint32_t ep, k, c;
    int bad = 0, lanebad;

    sSeed = 0x2545F491u;
    lanebad = lanes(&ops);
    memset(sOps, 0, sizeof sOps);
    for (k = 0; k < 0x800; k++) {
        put(k, 0x8000033Cu, 0x400002FFu); /* anything outside the programs ends at once */
    }
    memset(u, 0, sizeof u);
    for (k = 0; k < 2; k++) {
        u[k].vf = sVf[k];
        u[k].vi = sVi[k];
        u[k].acc = sAcc[k];
        u[k].q = &sQ[k];
        u[k].micro = sMicro;
        u[k].data = sData[k];
        u[k].microMask = 0x7FF;
        u[k].dataMask = 0x3FF;
        u[k].vu1 = 1;
        u[k].kick = k == 0 ? kick0 : kick1;
    }
    u[1].ops = sOps;
    for (ep = 0; ep < 3000; ep++) {
        uint32_t base = (rnd() % 32) * 64;
        int settle = rnd() & 1, run;

        for (k = 0; k < 32; k++) {
            for (c = 0; c < 4; c++) {
                sVf[0][k][c] = operand();
            }
        }
        for (k = 0; k < 16; k++) {
            sVi[0][k] = (uint16_t)rnd();
        }
        for (k = 0; k < 0x4000; k += 4) {
            uint32_t w = operand();
            memcpy(sData[0] + k, &w, 4);
        }
        for (c = 0; c < 4; c++) {
            sAcc[0][c] = operand();
        }
        sQ[0] = operand();
        sVf[0][0][0] = sVf[0][0][1] = sVf[0][0][2] = 0;
        sVf[0][0][3] = 0x3F800000u;
        sVi[0][0] = 0;
        memcpy(sVf[1], sVf[0], sizeof sVf[0]);
        memcpy(sVi[1], sVi[0], sizeof sVi[0]);
        memcpy(sData[1], sData[0], sizeof sData[0]);
        memcpy(sAcc[1], sAcc[0], sizeof sAcc[0]);
        sQ[1] = sQ[0];
        u[0].i = u[1].i = operand();
        u[0].p = u[1].p = operand();
        u[0].top = u[1].top = rnd() & 0x3FF;
        u[0].itop = u[1].itop = rnd() & 0x3FF;
        u[0].settle = u[1].settle = settle;
        program(base);
        for (run = 0; run < 3; run++) {
            uint64_t i0 = u[1].instr;
            int r0, r1;
            if (run > 0) { /* rewrite some instructions in place (also their I / E bits) */
                for (k = 0; k < 4; k++) {
                    uint32_t at = base + rnd() % (PROG - 2);
                    put(at, lower(at - base), upper());
                }
            }
            sHash[0] = sHash[1] = 0;
            r0 = Vu_RunRef(&u[0], base);
            r1 = Vu_Run(&u[1], base);
            instr += (unsigned long)(u[1].instr - i0);
            programs++;
            if (r0 != r1 || sHash[0] != sHash[1] || !Vu_SameState(&u[0], &u[1])) {
                if (bad++ < 5) {
                    printf("ttt: vu fast: random program %lu (episode %u, run %d) differs from the reference\n",
                           programs, ep, run);
                }
            }
        }
        u[0].cycle = u[1].cycle; /* the next episode starts both units equal */
        u[0].instr = u[1].instr;
        u[0].clipCount = u[0].macCount = u[1].clipCount = u[1].macCount = 0;
        u[0].qReady = u[1].qReady = u[0].pReady = u[1].pReady = 0;
        u[0].clip = u[1].clip = u[0].clipLatest = u[1].clipLatest = 0;
        u[0].mac = u[1].mac = u[0].macLazy = u[1].macLazy = 0;
        memcpy(u[0].unknown, u[1].unknown, sizeof u[0].unknown);
    }
    printf("ttt: vu fast: %lu random programs, %lu instructions, %d differ; %lu lane operations, %d differ\n", programs,
           instr, bad, ops, lanebad);
    return bad + lanebad;
}
