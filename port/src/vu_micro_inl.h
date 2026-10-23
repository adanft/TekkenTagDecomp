/* The vector units' arithmetic and pipeline helpers, shared by the reference interpreter (vu_micro.c) and the
   predecoded one (vu_fast.c): the EE float model's vector unit in integer arithmetic, the flags' queues, the end of a
   program. Internal to those two files. */
#ifndef PORT_VU_MICRO_INL_H
#define PORT_VU_MICRO_INL_H
#include <stdint.h>
#include "softfloat_ps2_inl.h"
#include "vu_micro.h"


/* ---------------------------------------------------------------------------------------------- arithmetic */

static inline __attribute__((always_inline)) uint32_t vadd(uint32_t a, uint32_t b) { return sf_vadd(a, b); }
static inline __attribute__((always_inline)) uint32_t vsub(uint32_t a, uint32_t b) { return sf_vadd(a, b ^ SF_SIGN); }
static inline __attribute__((always_inline)) uint32_t vmul(uint32_t a, uint32_t b) { return sf_vmul(a, b); }
static uint32_t vdiv(uint32_t a, uint32_t b) { return sf_div_core(a, b, 0); }
static uint32_t vsqrt(uint32_t a) { return sf_sqrt_core(a, 0); }
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

/* EATAN's series (the VU manual's; the same constants as PCSX2's): atan(x) = pi/4 + sum T_k t^(2k-1), t = (x - 1) /
   (x + 1), here with t given; the terms are added from the first, pi/4 last [I: the EFU's order of operations]. */
static uint32_t eatan_t(uint32_t t) {
    static const uint32_t T[8] = {0x3F7FFFF5u, 0xBEAAA61Cu, 0x3E4C40A6u, 0xBE0E6C63u,
                                  0x3DC577DFu, 0xBD6501C4u, 0x3CB31652u, 0xBB84D7E7u};
    uint32_t t2 = vmul(t, t), tp = t, p = vmul(T[0], t);
    int k;

    for (k = 1; k < 8; k++) {
        tp = vmul(tp, t2);
        p = vadd(p, vmul(T[k], tp));
    }
    return vadd(p, 0x3F490FDBu);
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

/* The MAC flags now: worked out from the result they show the first time they are read. */
static uint32_t mac_now(VuCore *u) {
    if (u->macLazy) {
        uint32_t m = 0, c;
        for (c = 0; c < 4; c++) {
            if ((u->macNow.mask >> (3 - c)) & 1) {
                m |= mac_bits(u->macNow.res[c], (int)c);
            }
        }
        u->mac = m;
        u->macLazy = 0;
    }
    return u->mac;
}

static void queue(VuFlagStep *q, int *head, int *count, uint32_t cycle, uint32_t value) {
    int slot = (*head + *count) & 7;

    q[slot].cycle = cycle + 4;
    q[slot].value = value;
    if (*count < 8) {
        (*count)++;
    } else {
        *head = (*head + 1) & 7;
    }
}

/* Results still in flight at the end of a program are there before the next one starts. */
static void settle(VuCore *u) {
    if (u->qReady != 0) {
        *u->q = u->qPending;
        u->qReady = 0;
    }
    if (u->pReady != 0) {
        u->p = u->pPending;
        u->pReady = 0;
    }
    if (u->clipCount != 0) {
        u->clip = u->clipQueue[(u->clipHead + u->clipCount - 1) & 7].value;
        u->clipCount = 0;
    }
    if (u->macCount != 0) {
        u->macNow = u->macQueue[(u->macHead + u->macCount - 1) & 7];
        u->macLazy = 1;
        u->macCount = 0;
    }
    mac_now(u);
}

#define MEMQ(a) (u->data + (((uint32_t)(a)) & u->dataMask) * 16)
#define SETVI(r, v)                                                                                                    \
    do {                                                                                                               \
        if ((r) & 15) {                                                                                                \
            u->vi[(r) & 15] = (uint16_t)(v);                                                                           \
        }                                                                                                              \
    } while (0)

#endif
