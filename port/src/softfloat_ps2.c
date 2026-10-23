/*
 * The Emotion Engine's single-precision arithmetic for the whole program (replaces the host's float entirely).
 *
 * Every game source and every file of port/src is compiled with -msoft-float: the compiler emits no float
 * instruction and calls these routines for each float operation, with floats travelling as 32-bit patterns in
 * integer registers. They implement the EE's FPU (softfloat_ps2_inl.h has the model and its evidence), so the
 * simulation does the same arithmetic on every host and nothing of the host's float unit (rounding mode, x87
 * precision, NaN handling) can leak into it.
 *
 * Code that is VU0 macro code on the PS2 (port/src/plat_vu0.c, vu0_pc.c) gets the vector unit's arithmetic instead:
 * the build renames its calls to __addsf3 ... to the Port_Vu* routines at the end of this file
 * (port/tools/toolchain.py VU_FILES).
 *
 * Doubles: the game uses none (the PS2 library's double code is only reached on error paths of the maths
 * wrappers, which third_party/newlib_libm/wrappers.c handles on bit patterns).
 */
#include <stdint.h>
#include "softfloat_ps2_inl.h"

typedef uint32_t f32w; /* a float's bits */

f32w __addsf3(f32w a, f32w b) { return sf_fpu_add(a, b); }
f32w __subsf3(f32w a, f32w b) { return sf_fpu_add(a, b ^ SF_SIGN); }
f32w __mulsf3(f32w a, f32w b) { return sf_mul_core(a, b, 0); }
f32w __divsf3(f32w a, f32w b) { return sf_div_core(a, b, 1); }
f32w __negsf2(f32w a) { return a ^ SF_SIGN; }

/* sqrt.s (the game's sqrtf: ee-gcc inlines it as sqrt.s; its errno fallback is never taken on the EE). */
f32w Port_FpuSqrt(f32w a) { return sf_sqrt_core(a, 1); }
f32w sqrtf(f32w a) { return sf_sqrt_core(a, 1); }

/* add.s / sub.s / mul.s / div.s under names the VU files' rename leaves alone: EE FPU code inside a library the
   port writes as VU code (libvu0's ViewScreenMatrix, the angle fold of its Rot* functions, plat_vu0.c). */
f32w Port_FpuAdd(f32w a, f32w b) { return sf_fpu_add(a, b); }
f32w Port_FpuSub(f32w a, f32w b) { return sf_fpu_add(a, b ^ SF_SIGN); }
f32w Port_FpuMul(f32w a, f32w b) { return sf_mul_core(a, b, 0); }
f32w Port_FpuDiv(f32w a, f32w b) { return sf_div_core(a, b, 1); }

/* madd.s / msub.s: ACC +/- fs * ft, the product formed first, each step an FPU operation. */
f32w Port_FpuMadd(f32w acc, f32w s, f32w t) { return sf_fpu_add(acc, sf_mul_core(s, t, 0)); }
f32w Port_FpuMsub(f32w acc, f32w s, f32w t) { return sf_fpu_add(acc, sf_mul_core(s, t, 0) ^ SF_SIGN); }

/* Comparisons: c.eq.s / c.lt.s / c.le.s never see "unordered"; libgcc's convention: <0, 0, >0. */
static int cmp(f32w a, f32w b) {
    int32_t x = sf_key(a), y = sf_key(b);

    return x < y ? -1 : x > y ? 1 : 0;
}
int __eqsf2(f32w a, f32w b) { return cmp(a, b); }
int __nesf2(f32w a, f32w b) { return cmp(a, b); }
int __ltsf2(f32w a, f32w b) { return cmp(a, b); }
int __lesf2(f32w a, f32w b) { return cmp(a, b); }
int __gtsf2(f32w a, f32w b) { return cmp(a, b); }
int __gesf2(f32w a, f32w b) { return cmp(a, b); }
int __unordsf2(f32w a, f32w b) {
    (void)a;
    (void)b;
    return 0;
}

/* cvt.w.s: cut toward zero; out of range gives the largest / smallest integer. */
int32_t __fixsfsi(f32w a) { return sf_ftoi(a); }

/* The PS2 library's float -> unsigned (libgcc __fixunssfsi, 0x40EBA0): values of 2^31 and above go through
   (x - 2^31) and get the top bit. */
uint32_t __fixunssfsi(f32w a) {
    if (cmp(a, 0x4F000000u) >= 0) {
        return (uint32_t)sf_ftoi(sf_fpu_add(a, 0xCF000000u)) | SF_SIGN;
    }
    return (uint32_t)sf_ftoi(a);
}

/* cvt.s.w: the integer cut to 24 bits. */
f32w __floatsisf(int32_t v) { return sf_itof(v); }

/* unsigned -> float as ee-gcc does it: convert as signed, add 2^32 when the top bit was set. */
f32w __floatunsisf(uint32_t v) {
    f32w f = sf_itof((int32_t)v);

    return (v & SF_SIGN) ? sf_fpu_add(f, 0x4F800000u) : f;
}

/* ---- the vector unit (VU0 macro mode): no add guard-bit step, divide and square root cut toward zero ---- */
f32w Port_VuAdd(f32w a, f32w b) { return sf_add_core(a, b, 0); }
f32w Port_VuSub(f32w a, f32w b) { return sf_add_core(a, b ^ SF_SIGN, 0); }
f32w Port_VuMul(f32w a, f32w b) { return sf_mul_core(a, b, 0); }
f32w Port_VuDiv(f32w a, f32w b) { return sf_div_core(a, b, 0); }
f32w Port_VuSqrt(f32w a) { return sf_sqrt_core(a, 0); }
/* vmadd / vmsub: ACC +/- fs * ft, the product formed first. */
f32w Port_VuMadd(f32w acc, f32w s, f32w t) { return sf_add_core(acc, sf_mul_core(s, t, 0), 0); }
f32w Port_VuMsub(f32w acc, f32w s, f32w t) { return sf_add_core(acc, sf_mul_core(s, t, 0) ^ SF_SIGN, 0); }
