/*
 * The Emotion Engine's single-precision arithmetic on bit patterns: the core of softfloat_ps2.c, as inline
 * functions so the vector-unit helpers can use the same number model without a call per operation.
 *
 * Hardware-level model taken from the Tenkaichi3Decomp PC port (port/src/softfloat_ps2_inl.h), whose float test
 * (port/tests/ps2float here) was run in PCSX2 2.7.303: on 1500 inputs per operation class every FPU and vector-unit
 * result below agrees with PCSX2. The model:
 *   - an exponent field of 0 is zero (no denormals in or out); an exponent field of 255 is an ordinary number that
 *     counts as the largest one (no infinity, no NaN);
 *   - add / subtract / multiply cut the exact result toward zero; overflow gives +/-FLT_MAX (0x7F7FFFFF), underflow
 *     gives +/-0;
 *   - the FPU's add.s / sub.s first clear the low bits of the smaller operand (one guard bit, no sticky bit), the
 *     vector unit's adds do not;
 *   - the FPU's div.s and sqrt.s round to nearest, the vector unit's vdiv / vsqrt cut toward zero; x / 0 is
 *     +/-FLT_MAX; a square root is taken of the absolute value.
 * No host floating point is used.
 */
#ifndef PORT_SOFTFLOAT_PS2_INL_H
#define PORT_SOFTFLOAT_PS2_INL_H
#include <stdint.h>

#define SF_SIGN 0x80000000u
#define SF_FMAX 0x7F7FFFFFu
#define SF_EXP(x) (((x) >> 23) & 0xFF)
#define SF_MANT(x) (((x) & 0x7FFFFF) | 0x800000)

/* An operand with exponent 255 (infinity / NaN patterns) counts as the largest number. */
static inline uint32_t sf_in(uint32_t a) {
    return SF_EXP(a) == 255 ? (a & SF_SIGN) | SF_FMAX : a;
}

/* Packs sign, exponent and a mantissa in [2^23, 2^24); out of range gives +/-FLT_MAX or +/-0. */
static inline uint32_t sf_pack(uint32_t sign, int32_t e, uint32_t m) {
    if (e >= 255) {
        return sign | SF_FMAX;
    }
    if (e <= 0) {
        return sign;
    }
    return sign | ((uint32_t)e << 23) | (m & 0x7FFFFF);
}

/* Rounds a mantissa with 32 extra bits (and "more below" in sticky) to nearest even, renormalising. */
static inline uint32_t sf_round_pack(uint32_t sign, int32_t e, uint64_t m, int sticky, int nearest) {
    uint32_t hi = (uint32_t)(m >> 32);
    uint32_t lo = (uint32_t)m;

    if (nearest && (lo & 0x80000000u) && ((lo & 0x7FFFFFFFu) || sticky || (hi & 1))) {
        hi++;
        if (hi == 0x1000000) {
            hi >>= 1;
            e++;
        }
    }
    return sf_pack(sign, e, hi);
}

/* a + b, exact, then cut toward zero (nearest = 0) or rounded to nearest even. */
static inline uint32_t sf_add_core(uint32_t a, uint32_t b, int nearest) {
    uint64_t ma, mb;
    uint32_t sign;
    int32_t e, d;
    int sticky = 0;

    a = sf_in(a);
    b = sf_in(b);
    if (SF_EXP(a) == 0 && SF_EXP(b) == 0) {
        return a & b & SF_SIGN;
    }
    if (SF_EXP(b) == 0) {
        return a;
    }
    if (SF_EXP(a) == 0) {
        return b;
    }
    if ((a & 0x7FFFFFFF) < (b & 0x7FFFFFFF)) {
        uint32_t t = a;
        a = b;
        b = t;
    }
    sign = a & SF_SIGN;
    e = (int32_t)SF_EXP(a);
    d = e - (int32_t)SF_EXP(b);
    ma = (uint64_t)SF_MANT(a) << 32;
    mb = (uint64_t)SF_MANT(b) << 32;
    if ((a ^ b) & SF_SIGN) {
        /* the bits of b shifted out below 2^-32 make the exact difference smaller: one unit there, then cut */
        uint64_t lost = d > 32;

        sticky = (int)lost;
        mb = d >= 64 ? 0 : mb >> d;
        ma -= mb + lost;
        if (ma == 0) {
            return 0; /* x - x is +0 */
        }
        while (!(ma & ((uint64_t)1 << 55))) {
            ma <<= 1;
            e--;
        }
    } else {
        sticky = d > 32;
        ma += d >= 64 ? 0 : mb >> d;
        if (ma & ((uint64_t)1 << 56)) {
            sticky |= (int)(ma & 1);
            ma >>= 1;
            e++;
        }
    }
    return sf_round_pack(sign, e, ma, sticky, nearest);
}

/* a * b, cut toward zero or rounded to nearest even. */
static inline uint32_t sf_mul_core(uint32_t a, uint32_t b, int nearest) {
    uint32_t sign = (a ^ b) & SF_SIGN;
    uint64_t m;
    int32_t e;

    a = sf_in(a);
    b = sf_in(b);
    if (SF_EXP(a) == 0 || SF_EXP(b) == 0) {
        return sign;
    }
    m = (uint64_t)SF_MANT(a) * SF_MANT(b);
    e = (int32_t)SF_EXP(a) + (int32_t)SF_EXP(b) - 127;
    if (m & ((uint64_t)1 << 47)) {
        e++;
        m <<= 8; /* 24 result bits above bit 32 */
    } else {
        m <<= 9;
    }
    return sf_round_pack(sign, e, m, 0, nearest);
}

/* a / b; nearest = 1 rounds to nearest even (the FPU's div.s), 0 cuts (vdiv). x / 0 and 0 / 0 give +/-FLT_MAX. */
static inline uint32_t sf_div_core(uint32_t a, uint32_t b, int nearest) {
    uint32_t sign = (a ^ b) & SF_SIGN;
    uint64_t n, q, rem;
    uint32_t m;
    int32_t e;

    a = sf_in(a);
    b = sf_in(b);
    if (SF_EXP(b) == 0) {
        return sign | SF_FMAX;
    }
    if (SF_EXP(a) == 0) {
        return sign;
    }
    n = (uint64_t)SF_MANT(a) << 26;
    q = n / SF_MANT(b);
    rem = n % SF_MANT(b);
    e = (int32_t)SF_EXP(a) - (int32_t)SF_EXP(b) + 127;
    if (q >= ((uint64_t)1 << 26)) {
        rem |= q & 1;
        q >>= 1;
    } else {
        e--;
    }
    rem |= q & 1; /* q is now in [2^25, 2^26): one more bit goes to the sticky bits */
    q >>= 1;
    m = (uint32_t)(q >> 1); /* 24 bits; q & 1 is the round bit, rem the sticky bits */
    if (nearest && (q & 1) && (rem != 0 || (m & 1))) {
        m++;
        if (m == 0x1000000) {
            m >>= 1;
            e++;
        }
    }
    return sf_pack(sign, e, m);
}

/* sqrt(|a|); nearest = 1 rounds to nearest (the FPU's sqrt.s), 0 cuts (vsqrt). Zero and denormals give +0. */
static inline uint32_t sf_sqrt_core(uint32_t a, int nearest) {
    uint64_t n, r, bit;
    int32_t e;

    a = sf_in(a) & ~SF_SIGN;
    if (SF_EXP(a) == 0) {
        return 0;
    }
    e = (int32_t)SF_EXP(a) - 127;
    n = (uint64_t)SF_MANT(a);
    if (e & 1) {
        n <<= 1;
        e -= 1;
    }
    n <<= 23; /* 2^46 <= n < 2^48, root in [2^23, 2^24) */
    r = 0;
    for (bit = (uint64_t)1 << 46; bit != 0; bit >>= 2) {
        if (n >= r + bit) {
            n -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
    }
    /* n is now the remainder (original n - r^2); r + 1/2 is exceeded when the remainder is above r (no ties) */
    if (nearest && n > r) {
        r++;
        if (r == 0x1000000) {
            r >>= 1;
            e += 2;
        }
    }
    return ((uint32_t)(e / 2 + 127) << 23) | ((uint32_t)r & 0x7FFFFF);
}

/*
 * The FPU's add.s / sub.s. The PS2 adder keeps only ONE bit of the smaller operand below the larger operand's last
 * place and no sticky bits; PCSX2 reproduces this by clearing the smaller operand's low bits before an ordinary
 * truncating add: exponent difference d of 1..24 clears d - 1 bits, 25 or more leaves only the sign.
 */
static inline uint32_t sf_fpu_add(uint32_t a, uint32_t b) {
    int32_t d = (int32_t)SF_EXP(a) - (int32_t)SF_EXP(b);

    if (d >= 25) {
        b &= SF_SIGN;
    } else if (d > 0) {
        b &= 0xFFFFFFFFu << (d - 1);
    } else if (d <= -25) {
        a &= SF_SIGN;
    } else if (d < 0) {
        a &= 0xFFFFFFFFu << (-d - 1);
    }
    return sf_add_core(a, b, 0);
}

/* Operands as the comparisons see them: exponent 0 is +/-0, exponent 255 is +/-FLT_MAX (PCSX2's fpuDouble). */
static inline int32_t sf_key(uint32_t a) {
    a = sf_in(a);
    if (SF_EXP(a) == 0) {
        return 0;
    }
    return (a & SF_SIGN) ? -(int32_t)(a & 0x7FFFFFFF) : (int32_t)(a & 0x7FFFFFFF);
}

/* cvt.s.w / vitof0: the integer cut to 24 bits. */
static inline uint32_t sf_itof(int32_t v) {
    uint32_t sign = v < 0 ? SF_SIGN : 0;
    uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
    int32_t e = 31;

    if (u == 0) {
        return 0;
    }
    while (!(u & 0x80000000u)) {
        u <<= 1;
        e--;
    }
    return sign | ((uint32_t)(e + 127) << 23) | ((u >> 8) & 0x7FFFFF);
}

/* cvt.w.s / vftoi0: cut toward zero; out of range saturates. */
static inline int32_t sf_ftoi(uint32_t a) {
    int32_t e = (int32_t)SF_EXP(a) - 127;
    uint32_t m = SF_MANT(a);
    uint32_t v;

    if (SF_EXP(a) == 0 || e < 0) {
        return 0;
    }
    if (e >= 31) {
        return (a & SF_SIGN) ? (int32_t)0x80000000u : 0x7FFFFFFF;
    }
    v = e >= 23 ? m << (e - 23) : m >> (23 - e);
    return (a & SF_SIGN) ? -(int32_t)v : (int32_t)v;
}

#endif
