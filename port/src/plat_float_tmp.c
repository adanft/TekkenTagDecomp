/*
 * TEMPORARY host float, until the EE float model (T23: softfloat_ps2, the PS2's newlib libm).
 *
 * The game is compiled with -msoft-float: every float operation is a call to a libgcc helper (__addsf3 ...) and
 * float values travel as 32-bit words (results in eax). The host's i386 libgcc has no such helpers and the host
 * libm returns on the x87 stack, so both are written here with x87 instructions on the words. The results are IEEE
 * single precision rounded to nearest, NOT the EE's (truncation, no NaN / infinity / denormals): enough for the
 * start to run, not for a simulation that matches the console. T23 replaces this file as a whole.
 *
 * Built like the rest of port/src with -msoft-float: no C float arithmetic here, only words and inline x87.
 */
#include <stdint.h>

typedef uint32_t f32w; /* a float's bits */

#define X87_BIN(name, insn)                                                                                         \
    f32w name(f32w a, f32w b) {                                                                                     \
        f32w r;                                                                                                     \
        __asm__ volatile("flds %1\n\t" insn " %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b));                          \
        return r;                                                                                                   \
    }
X87_BIN(__addsf3, "fadds")
X87_BIN(__subsf3, "fsubs")
X87_BIN(__mulsf3, "fmuls")
X87_BIN(__divsf3, "fdivs")

/* -1 a < b, 0 equal, 1 a > b, 2 unordered. */
static int cmp(f32w a, f32w b) {
    uint16_t sw;
    __asm__ volatile("flds %2\n\tflds %1\n\tfucompp\n\tfnstsw %0" : "=a"(sw) : "m"(a), "m"(b));
    /* C3 C2 C0: 000 a > b, 001 a < b, 100 equal, 111 unordered */
    if ((sw & 0x4500) == 0x4500) {
        return 2;
    }
    if (sw & 0x4000) {
        return 0;
    }
    return (sw & 0x0100) ? -1 : 1;
}
int __eqsf2(f32w a, f32w b) { return cmp(a, b) != 0; }
int __nesf2(f32w a, f32w b) { return cmp(a, b) != 0; }
int __ltsf2(f32w a, f32w b) { int c = cmp(a, b); return c == 2 ? 1 : c; }
int __lesf2(f32w a, f32w b) { int c = cmp(a, b); return c == 2 ? 1 : c; }
int __gtsf2(f32w a, f32w b) { int c = cmp(a, b); return c == 2 ? -1 : c; }
int __gesf2(f32w a, f32w b) { int c = cmp(a, b); return c == 2 ? -1 : c; }
int __unordsf2(f32w a, f32w b) { return cmp(a, b) == 2; }

/* float -> int, toward zero; out of range saturates as the EE's cvt.w.s does. */
int32_t __fixsfsi(f32w a) {
    int e = (int)((a >> 23) & 0xFF) - 127;
    uint32_t m = (a & 0x7FFFFF) | 0x800000;
    int32_t v;
    if (e < 0) {
        return 0;
    }
    if (e >= 31) {
        return (a >> 31) ? INT32_MIN : INT32_MAX;
    }
    v = (int32_t)(e >= 23 ? m << (e - 23) : m >> (23 - e));
    return (a >> 31) ? -v : v;
}
uint32_t __fixunssfsi(f32w a) {
    if (a >> 31) {
        return 0;
    }
    if (((a >> 23) & 0xFF) >= 127 + 31) {
        int e = (int)((a >> 23) & 0xFF) - 127;
        return e >= 32 ? 0xFFFFFFFFu : ((a & 0x7FFFFF) | 0x800000) << (e - 23);
    }
    return (uint32_t)__fixsfsi(a);
}
f32w __floatsisf(int32_t i) {
    f32w r;
    __asm__ volatile("fildl %1\n\tfstps %0" : "=m"(r) : "m"(i));
    return r;
}
f32w __floatunsisf(uint32_t u) {
    uint64_t q = u;
    f32w r;
    __asm__ volatile("fildq %1\n\tfstps %0" : "=m"(r) : "m"(q));
    return r;
}

/* ---- libm (single precision, x87) ---- */
#define X87_UN(name, body)                                                                                          \
    f32w name(f32w a) {                                                                                             \
        f32w r;                                                                                                     \
        __asm__ volatile("flds %1\n\t" body "\n\tfstps %0" : "=m"(r) : "m"(a));                                     \
        return r;                                                                                                   \
    }
X87_UN(sinf, "fsin")
X87_UN(cosf, "fcos")
X87_UN(sqrtf, "fsqrt")
X87_UN(logf, "fldln2\n\tfxch\n\tfyl2x")
f32w fabsf(f32w a) { return a & 0x7FFFFFFFu; }

f32w atan2f(f32w y, f32w x) {
    f32w r;
    __asm__ volatile("flds %1\n\tflds %2\n\tfpatan\n\tfstps %0" : "=m"(r) : "m"(y), "m"(x));
    return r;
}
f32w asinf(f32w x) { /* atan2(x, sqrt(1 - x * x)) */
    f32w r;
    __asm__ volatile("flds %1\n\tfld %%st(0)\n\tfmul %%st(0), %%st(0)\n\tfld1\n\tfsub %%st(1), %%st\n\t"
                     "fstp %%st(1)\n\tfsqrt\n\tfpatan\n\tfstps %0"
                     : "=m"(r) : "m"(x));
    return r;
}
f32w fmodf(f32w x, f32w y) {
    f32w r;
    __asm__ volatile("flds %2\n\tflds %1\n"
                     "1:\tfprem\n\tfnstsw %%ax\n\ttestw $0x400, %%ax\n\tjnz 1b\n\t"
                     "fstp %%st(1)\n\tfstps %0"
                     : "=m"(r) : "m"(x), "m"(y) : "eax");
    return r;
}
/* x ** y = 2 ** (y * log2 x), x > 0; x == 0 gives 0, x < 0 with an integer y the signed result. */
f32w powf(f32w x, f32w y) {
    f32w r, ax = x & 0x7FFFFFFFu;
    int odd = 0;
    if (y == 0 || y == 0x80000000u) {
        return 0x3F800000u;
    }
    if (ax == 0) {
        return 0;
    }
    if (x >> 31) {
        int32_t iy = __fixsfsi(y);
        if (__floatsisf(iy) != y) {
            return 0x7FC00000u;
        }
        odd = iy & 1;
    }
    __asm__ volatile("flds %2\n\tflds %1\n\tfyl2x\n\t"
                     "fld %%st(0)\n\tfrndint\n\tfxch\n\tfsub %%st(1), %%st\n\tf2xm1\n\tfld1\n\tfaddp\n\tfscale\n\t"
                     "fstp %%st(1)\n\tfstps %0"
                     : "=m"(r) : "m"(ax), "m"(y));
    return odd ? r | 0x80000000u : r;
}
