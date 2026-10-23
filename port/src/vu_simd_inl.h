/*
 * The vector unit's add, multiply, max and min on four lanes at once (SSE2: every x86-64 CPU has it, and the 32-bit
 * port uses it in the files built with -msse2, toolchain.SIMD_FILES). Each lane is exactly the scalar model of
 * softfloat_ps2_inl.h (sf_vadd, sf_vmul, sf_key): max / min in integer instructions, add / multiply on the SSE unit
 * in a rounding mode that makes it compute the model (below). TTT_SELFTEST=vu1 compares them with the scalar model
 * on edge and random operands (vu_fasttest.c).
 */
#ifndef PORT_VU_SIMD_INL_H
#define PORT_VU_SIMD_INL_H
#include <emmintrin.h>
#include <stdint.h>
#include "softfloat_ps2_inl.h"

typedef __m128i V4;

#define V4_SET1(x) _mm_set1_epi32((int)(x))
#define V4_LOAD(p) _mm_loadu_si128((const __m128i *)(const void *)(p))
#define V4_STORE(p, v) _mm_storeu_si128((__m128i *)(void *)(p), (v))

static inline __attribute__((always_inline)) V4 v4_sel(V4 m, V4 a, V4 b) { /* m ? a : b, per lane */
    return _mm_or_si128(_mm_and_si128(m, a), _mm_andnot_si128(m, b));
}

/* sf_in: exponent 255 is the largest number (sign | FLT_MAX). e: the exponent fields of the result. */
static inline __attribute__((always_inline)) V4 v4_in(V4 a, V4 *e) {
    V4 ex = _mm_srli_epi32(_mm_slli_epi32(a, 1), 24);
    V4 big = _mm_cmpeq_epi32(ex, V4_SET1(255));
    *e = _mm_sub_epi32(ex, _mm_srli_epi32(big, 31)); /* 255 -> 254 */
    return v4_sel(big, _mm_or_si128(_mm_and_si128(a, V4_SET1(0x80000000u)), V4_SET1(0x7F7FFFFF)), a);
}

/*
 * Add and multiply on the host's SSE single-precision unit, which gives the model's results exactly when its MXCSR
 * rounds toward zero and flushes tiny results (V4_MXCSR, set around the micro program by vu_micro.c):
 *   - the model cuts the exact sum / product toward zero to 24 bits, as IEEE round-toward-zero does;
 *   - a result whose exact value is below 2^-126 is +/-0 with the result's sign in the model; under round-toward-
 *     zero "tiny before rounding" and "tiny after rounding" are the same, so the flush-to-zero bit gives that;
 *   - an overflow is +/-FLT_MAX in the model, as round-toward-zero gives;
 *   - operands: exponent 0 is +/-0 and exponent 255 the largest number in the model, so both are mapped first
 *     (v4_in); the host then never sees a denormal, infinity or NaN, and x - x is +0 in both.
 * No other floating-point instruction is used; vu_fasttest.c checks every lane against the integer model.
 */
#define V4_MXCSR 0xFF80u /* all exceptions masked, round toward zero (0x6000), flush to zero (0x8000) */

static inline __attribute__((always_inline)) V4 v4_op_in(V4 a) {
    V4 e, r = v4_in(a, &e);
    return _mm_andnot_si128(_mm_andnot_si128(V4_SET1(0x80000000u), _mm_cmpeq_epi32(e, _mm_setzero_si128())), r);
}

/* sf_vmul on four lanes. */
static inline __attribute__((always_inline)) V4 v4_mul(V4 a, V4 b) {
    return _mm_castps_si128(_mm_mul_ps(_mm_castsi128_ps(v4_op_in(a)), _mm_castsi128_ps(v4_op_in(b))));
}

/* sf_vadd on four lanes. */
static inline __attribute__((always_inline)) V4 v4_add(V4 a, V4 b) {
    return _mm_castps_si128(_mm_add_ps(_mm_castsi128_ps(v4_op_in(a)), _mm_castsi128_ps(v4_op_in(b))));
}

/* The vector unit's divide and square root of one lane (sf_div_core / sf_sqrt_core with the cut toward zero) on the
   SSE unit under V4_MXCSR: IEEE division and square root are exact before the rounding, which then cuts. The cases
   the host would give infinity or NaN for are the model's own (x / 0 and 0 / 0: +/-FLT_MAX). */
static inline __attribute__((always_inline)) uint32_t v1_div(uint32_t a, uint32_t b) {
    uint32_t sign = (a ^ b) & 0x80000000u;
    a = sf_in(a);
    b = sf_in(b);
    if (SF_EXP(b) == 0) {
        return sign | SF_FMAX;
    }
    if (SF_EXP(a) == 0) {
        return sign;
    }
    return (uint32_t)_mm_cvtsi128_si32(_mm_castps_si128(
        _mm_div_ss(_mm_castsi128_ps(_mm_cvtsi32_si128((int)a)), _mm_castsi128_ps(_mm_cvtsi32_si128((int)b)))));
}
static inline __attribute__((always_inline)) uint32_t v1_sqrt(uint32_t a) {
    a = sf_in(a) & ~SF_SIGN;
    if (SF_EXP(a) == 0) {
        return 0;
    }
    return (uint32_t)_mm_cvtsi128_si32(_mm_castps_si128(_mm_sqrt_ss(_mm_castsi128_ps(_mm_cvtsi32_si128((int)a)))));
}

/* vitofN (itof in vu_micro.c): the integer cut to 24 bits (the SSE conversion under V4_MXCSR), / 2^frac. */
static inline __attribute__((always_inline)) V4 v4_itof(V4 v, uint32_t frac) {
    V4 f = _mm_castps_si128(_mm_cvtepi32_ps(v)), ok;
    if (frac == 0) {
        return f;
    }
    ok = _mm_cmpgt_epi32(_mm_srli_epi32(_mm_slli_epi32(f, 1), 24), V4_SET1(frac));
    return v4_sel(ok, _mm_sub_epi32(f, V4_SET1(frac << 23)), _mm_and_si128(f, V4_SET1(0x80000000u)));
}

/* vftoiN (ftoi): a * 2^frac cut toward zero, saturating; exponent 0 gives 0. */
static inline __attribute__((always_inline)) V4 v4_ftoi(V4 a, uint32_t frac) {
    V4 ex = _mm_srli_epi32(_mm_slli_epi32(a, 1), 24);
    V4 sat = _mm_cmpgt_epi32(_mm_add_epi32(ex, V4_SET1(frac)), V4_SET1(157)); /* exponent - 127 + frac >= 31 */
    V4 r = _mm_cvttps_epi32(_mm_castsi128_ps(_mm_add_epi32(a, V4_SET1(frac << 23))));
    r = v4_sel(sat, _mm_xor_si128(_mm_srai_epi32(a, 31), V4_SET1(0x7FFFFFFF)), r);
    return _mm_andnot_si128(_mm_cmpeq_epi32(ex, _mm_setzero_si128()), r);
}

static inline __attribute__((always_inline)) V4 v4_sub(V4 a, V4 b) {
    return v4_add(a, _mm_xor_si128(b, V4_SET1(0x80000000u)));
}

/* sf_key: exponent 0 is 0, exponent 255 the largest, negative numbers below. */
static inline __attribute__((always_inline)) V4 v4_key(V4 a) {
    V4 e, m = _mm_and_si128(v4_in(a, &e), V4_SET1(0x7FFFFFFF)), neg = _mm_srai_epi32(a, 31);
    m = _mm_andnot_si128(_mm_cmpeq_epi32(e, _mm_setzero_si128()), m);
    return _mm_sub_epi32(_mm_xor_si128(m, neg), neg);
}

/* vmax / vmin: key(a) >= key(b) ? a : b, key(a) <= key(b) ? a : b. */
static inline __attribute__((always_inline)) V4 v4_max(V4 a, V4 b) {
    return v4_sel(_mm_cmpgt_epi32(v4_key(b), v4_key(a)), b, a);
}
static inline __attribute__((always_inline)) V4 v4_min(V4 a, V4 b) {
    return v4_sel(_mm_cmpgt_epi32(v4_key(a), v4_key(b)), b, a);
}

/* the lanes of a dest mask (x = bit 3) as all-ones lanes */
static inline __attribute__((always_inline)) V4 v4_mask(uint32_t dest) {
    return _mm_cmpgt_epi32(_mm_and_si128(V4_SET1(dest), _mm_set_epi32(1, 2, 4, 8)), _mm_setzero_si128());
}

#endif
