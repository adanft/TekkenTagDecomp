/*
 * Float self-test (TTT_SELFTEST=float): the EE float model (softfloat_ps2.c), the vector-unit entry points, the maths
 * library (third_party/newlib_libm) and newlib's rand, checked on the running program. Prints a summary and exits.
 *
 * Groups:
 *   pcsx2  port/tests/ps2float/pcsx2_v2.7.303.txt: 1500 inputs through every FPU and vector-unit operation class,
 *          result bits printed by test.c running in PCSX2 2.7.303 (hardware-level data of the reference project; the
 *          EE is the same chip). VERIFIED.
 *   rules  single cases that follow from the EE's documented behaviour (no denormals, no infinity / NaN, clamping,
 *          division by zero) and from the model the pcsx2 group verified.
 *   libm   cases whose result the algorithm fixes exactly on any number model (fmodf is integer arithmetic,
 *          sinf / cosf of 0, logf of 1...), and the error results of the executable's wrappers (wrappers.c).
 *          The general libm results need a PCSX2 run of port/tests/ps2float/libm_test.c (PENDING); when
 *          port/tests/ps2float/pcsx2_libm.txt exists it is compared too.
 *   rand   newlib's rand / srand (the executable's 0x40FBD0 / 0x40FBC0 algorithm) on the game's own state word.
 *   libvu0 the fields the SDK's VU0 helpers (plat_vu0.c) write, read off the executable's code (0x34A7A8..): which
 *          of x / y / z / w each one computes, keeps or clears. A w left over where the library writes 0 turns a
 *          rotation into a projective matrix (the fighters' spiky heads and hair of T29).
 * The data directory is TTT_FLOATDATA, or port/tests/ps2float next to build/port/ttt.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "softfloat_ps2_inl.h"

typedef uint32_t w32; /* a float's bits; every float function below is soft-float: bits in, bits out */

w32 __addsf3(w32, w32), __subsf3(w32, w32), __mulsf3(w32, w32), __divsf3(w32, w32);
int32_t __fixsfsi(w32);
uint32_t __fixunssfsi(w32);
w32 __floatsisf(int32_t), __floatunsisf(uint32_t);
int __eqsf2(w32, w32), __ltsf2(w32, w32);
w32 Port_FpuSqrt(w32), Port_FpuMadd(w32, w32, w32);
w32 Port_VuAdd(w32, w32), Port_VuSub(w32, w32), Port_VuMul(w32, w32), Port_VuDiv(w32, w32), Port_VuSqrt(w32);
w32 Port_VuMadd(w32, w32, w32);
w32 L_sinf(w32) __asm__("sinf");
w32 L_cosf(w32) __asm__("cosf");
w32 L_asinf(w32) __asm__("asinf");
w32 L_acosf(w32) __asm__("__ieee754_acosf");
w32 L_atanf(w32) __asm__("atanf");
w32 L_atan2f(w32, w32) __asm__("atan2f");
w32 L_sqrtf(w32) __asm__("sqrtf");
w32 L_ieee_sqrtf(w32) __asm__("__ieee754_sqrtf");
w32 L_logf(w32) __asm__("logf");
w32 L_powf(w32, w32) __asm__("powf");
w32 L_fmodf(w32, w32) __asm__("fmodf");
w32 L_fabsf(w32) __asm__("fabsf");
w32 L_floorf(w32) __asm__("Nl_floorf");
int rand(void);
void srand(unsigned);
extern int *D_004F5A8C; /* _impure_ptr: errno first, the rand state at +0x58 */

static int sFailed;

static void data_dir(char *out, size_t n) {
    const char *e = getenv("TTT_FLOATDATA");
    char exe[1024];
    ssize_t k;
    char *slash;

    if (e != NULL) {
        snprintf(out, n, "%s", e);
        return;
    }
    out[0] = 0;
    k = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (k <= 0) {
        return;
    }
    exe[k] = 0;
    slash = strrchr(exe, '/');
    if (slash != NULL) {
        *slash = 0;
        snprintf(out, n, "%s/../../port/tests/ps2float", exe); /* build/port/ttt -> port/tests/ps2float */
    }
}

static FILE *open_data(const char *name) {
    char dir[1100], path[1300];

    data_dir(dir, sizeof dir);
    snprintf(path, sizeof path, "%s/%s", dir, name);
    return fopen(path, "r");
}

/* Reads one line of hexadecimal words; returns how many. */
static int read_words(FILE *f, uint32_t *w, int max) {
    char line[512], *p, *end;
    int n = 0;

    if (fgets(line, sizeof line, f) == NULL) {
        return -1;
    }
    p = line;
    while (n < max) {
        unsigned long v = strtoul(p, &end, 16);
        if (end == p) {
            break;
        }
        w[n++] = (uint32_t)v;
        p = end;
    }
    return n;
}

/* ---- pcsx2: test.c's columns ---- */
static const char *const kOpCols[] = {"fpu_add", "fpu_sub", "fpu_mul", "fpu_div", "fpu_sqrt", "fpu_madd", "fpu_itof",
                                      "fpu_ftoi", "vu_add", "vu_sub", "vu_mul", "vu_div", "vu_sqrt", "vu_madd",
                                      "vu_itof", "vu_ftoi"};

static void model_row(uint32_t a, uint32_t b, int32_t n, uint32_t *r) {
    uint32_t ea = (a >> 23) & 0xFF;
    uint32_t p = Port_VuMul(a, b);

    r[0] = __addsf3(a, b);
    r[1] = __subsf3(a, b);
    r[2] = __mulsf3(a, b);
    r[3] = __divsf3(a, b);
    r[4] = Port_FpuSqrt(a);
    r[5] = Port_FpuMadd(__addsf3(0, a), a, b); /* adda.s ACC = 0 + x; madd.s ACC + x * y */
    r[6] = __floatsisf(n);
    r[7] = (uint32_t)__fixsfsi((a & 0x807FFFFFu) | ((120 + (ea & 31)) << 23));
    r[8] = Port_VuAdd(a, b);
    r[9] = Port_VuSub(a, b);
    r[10] = p;
    r[11] = Port_VuDiv(a, b);
    r[12] = Port_VuSqrt(a);
    r[13] = Port_VuMadd(p, a, b); /* vmula ACC = x * y; vmadd ACC + x * y */
    r[14] = __floatsisf(n);       /* vitof0: the same cut as cvt.s.w */
    r[15] = (uint32_t)__fixsfsi(a);
}

static void group_pcsx2(void) {
    FILE *f = open_data("pcsx2_v2.7.303.txt");
    uint32_t w[19], r[16];
    int bad[16] = {0}, rows = 0, i;

    if (f == NULL) {
        printf("ttt: float: pcsx2: data file not found (TTT_FLOATDATA)\n");
        sFailed = 1;
        return;
    }
    while (read_words(f, w, 19) == 19) {
        model_row(w[0], w[1], (int32_t)w[2], r);
        for (i = 0; i < 16; i++) {
            if (r[i] != w[3 + i]) {
                if (bad[i]++ == 0) {
                    printf("ttt: float: pcsx2 %s a %08x b %08x n %08x: PCSX2 %08x, model %08x\n", kOpCols[i], w[0],
                           w[1], w[2], w[3 + i], r[i]);
                }
            }
        }
        rows++;
    }
    fclose(f);
    for (i = 0; i < 16; i++) {
        if (bad[i]) {
            printf("ttt: float: pcsx2 %-8s %d of %d differ\n", kOpCols[i], bad[i], rows);
            sFailed = 1;
        }
    }
    if (rows != 1500) {
        printf("ttt: float: pcsx2: %d rows read, 1500 expected\n", rows);
        sFailed = 1;
    }
    printf("ttt: float: pcsx2 (verified): %d cases x 16 operation classes\n", rows);
}

/* ---- single cases ---- */
static int sCases;

static void expect(const char *what, uint32_t got, uint32_t want) {
    sCases++;
    if (got != want) {
        printf("ttt: float: %s: %08x, expected %08x\n", what, got, want);
        sFailed = 1;
    }
}

/* The VU interpreter's 32-bit add and multiply (sf_vadd / sf_vmul) against the model's sf_add_core / sf_mul_core:
   2 million pairs, half of them with close exponents and mantissas (cancellation), plus every exponent field. */
static void group_vu_fast(void) {
    uint32_t x = 2463534242u, n, bad = 0, e;

    for (n = 0; n < 2000000u; n++) {
        uint32_t a, b;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        a = x;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        b = x;
        if (n & 1) {
            b = ((a & 0xFF800000u) ^ ((b & 3) == 0 ? SF_SIGN : 0)) + (((b >> 2) % 64) - 32) * 0x800000u;
            b |= (x >> 9) & ((n & 2) ? 0x7FFFFFu : 0x7Fu);
        }
        bad += sf_vadd(a, b) != sf_add_core(a, b, 0);
        bad += sf_vmul(a, b) != sf_mul_core(a, b, 0);
    }
    for (e = 0; e < 256; e++) {
        uint32_t a = e << 23 | 0x2AAAAAu, b = 0x3F800000u | 0x155555u;
        bad += sf_vadd(a, b) != sf_add_core(a, b, 0) || sf_vadd(a, b ^ SF_SIGN) != sf_add_core(a, b ^ SF_SIGN, 0);
        bad += sf_vmul(a, b) != sf_mul_core(a, b, 0);
    }
    expect("sf_vadd / sf_vmul agree with the model", bad, 0);
}

static void group_rules(void) {
    sCases = 0;
    expect("denormal + 0 is 0", __addsf3(0x00000001u, 0), 0);
    expect("denormal * 1 is 0", __mulsf3(0x007FFFFFu, 0x3F800000u), 0);
    expect("min normal / 2 flushes", __divsf3(0x00800000u, 0x40000000u), 0);
    expect("max + max clamps", __addsf3(0x7F7FFFFFu, 0x7F7FFFFFu), 0x7F7FFFFFu);
    expect("-max * 2 clamps", __mulsf3(0xFF7FFFFFu, 0x40000000u), 0xFF7FFFFFu);
    expect("inf pattern is max", __addsf3(0x7F800000u, 0), 0x7F7FFFFFu);
    expect("NaN pattern is max", __mulsf3(0x7FC00000u, 0x3F800000u), 0x7F7FFFFFu);
    expect("1 / 0 is max", __divsf3(0x3F800000u, 0), 0x7F7FFFFFu);
    expect("-1 / 0 is -max", __divsf3(0xBF800000u, 0), 0xFF7FFFFFu);
    expect("1 / -0 is -max", __divsf3(0x3F800000u, 0x80000000u), 0xFF7FFFFFu);
    expect("0 / 0 is max", __divsf3(0, 0), 0x7F7FFFFFu);
    expect("x - x is +0", __subsf3(0xC1200000u, 0xC1200000u), 0);
    expect("div.s rounds to nearest (1 / 30)", __divsf3(0x3F800000u, 0x41F00000u), 0x3D088889u);
    expect("vdiv cuts (1 / 30)", Port_VuDiv(0x3F800000u, 0x41F00000u), 0x3D088888u);
    expect("mul.s cuts (1/3 * 3)", __mulsf3(0x3EAAAAABu, 0x40400000u), 0x3F800000u);
    expect("mul.s cuts (0.1 * 3)", __mulsf3(0x3DCCCCCDu, 0x40400000u), 0x3E999999u);
    expect("add.s guard bit (1 + 2^-24 ulp)", __addsf3(0x3F800000u, 0x33800001u), 0x3F800000u);
    expect("sqrt.s of -4 is 2", Port_FpuSqrt(0xC0800000u), 0x40000000u);
    expect("sqrt.s rounds to nearest (sqrt 2)", Port_FpuSqrt(0x40000000u), 0x3FB504F3u);
    expect("vsqrt cuts (sqrt 2)", Port_VuSqrt(0x40000000u), 0x3FB504F3u);
    expect("sqrt.s rounds to nearest (sqrt 3)", Port_FpuSqrt(0x40400000u), 0x3FDDB3D7u);
    expect("vsqrt cuts (sqrt 3)", Port_VuSqrt(0x40400000u), 0x3FDDB3D7u);
    expect("sqrt.s rounds to nearest (sqrt 5)", Port_FpuSqrt(0x40A00000u), 0x400F1BBDu);
    expect("vsqrt cuts (sqrt 5)", Port_VuSqrt(0x40A00000u), 0x400F1BBCu);
    expect("sqrtf is sqrt.s", L_sqrtf(0x40A00000u), 0x400F1BBDu);
    expect("cvt.s.w cuts 2^31-1", __floatsisf(0x7FFFFFFF), 0x4EFFFFFFu);
    expect("cvt.s.w cuts 16777217", __floatsisf(16777217), 0x4B800000u);
    expect("cvt.s.w -16777219", __floatsisf(-16777219), 0xCB800001u);
    expect("cvt.w.s cuts 2.9", (uint32_t)__fixsfsi(0x4039999Au), 2);
    expect("cvt.w.s cuts -2.9", (uint32_t)__fixsfsi(0xC039999Au), (uint32_t)-2);
    expect("cvt.w.s saturates 3e9", (uint32_t)__fixsfsi(0x4F32D05Eu), 0x7FFFFFFFu);
    expect("cvt.w.s saturates -3e9", (uint32_t)__fixsfsi(0xCF32D05Eu), 0x80000000u);
    expect("float -> u32 of 3e9", __fixunssfsi(0x4F32D05Eu), 3000000000u);
    expect("u32 -> float of 0xFFFFFFFF (-1 + 2^32, guard step)", __floatunsisf(0xFFFFFFFFu), 0x4F800000u);
    expect("-0 == +0", (uint32_t)__eqsf2(0x80000000u, 0), 0);
    expect("denormal == 0", (uint32_t)__eqsf2(0x00000005u, 0), 0);
    expect("-1 < 1", (uint32_t)(__ltsf2(0xBF800000u, 0x3F800000u) < 0), 1);
    expect("inf pattern == max", (uint32_t)__eqsf2(0x7F800000u, 0x7F7FFFFFu), 0);
    printf("ttt: float: rules: %d cases\n", sCases);
}

static void group_libm(void) {
    int *reent = D_004F5A8C;
    FILE *f;

    sCases = 0;
    expect("sinf(0)", L_sinf(0), 0);
    expect("sinf(-0)", L_sinf(0x80000000u), 0x80000000u);
    expect("sinf(2^-30) = x", L_sinf(0x30800000u), 0x30800000u);
    expect("cosf(0)", L_cosf(0), 0x3F800000u);
    expect("atan2f(0, 1)", L_atan2f(0, 0x3F800000u), 0);
    expect("atanf(0)", L_atanf(0), 0);
    expect("asinf(0)", L_asinf(0), 0);
    expect("logf(1)", L_logf(0x3F800000u), 0);
    expect("fabsf(-3)", L_fabsf(0xC0400000u), 0x40400000u);
    expect("floorf(-1.5)", L_floorf(0xBFC00000u), 0xC0000000u);
    expect("floorf(2.75)", L_floorf(0x40300000u), 0x40000000u);
    expect("fmodf(5.5, 2)", L_fmodf(0x40B00000u, 0x40000000u), 0x3FC00000u);
    expect("fmodf(-7, 3)", L_fmodf(0xC0E00000u, 0x40400000u), 0xBF800000u);
    expect("fmodf(6, 3) = +0", L_fmodf(0x40C00000u, 0x40400000u), 0);
    expect("fmodf(-6, 3) = -0", L_fmodf(0xC0C00000u, 0x40400000u), 0x80000000u);
    expect("fmodf(1, 3) = 1", L_fmodf(0x3F800000u, 0x40400000u), 0x3F800000u);
    expect("fmodf(1000.25, 0.5)", L_fmodf(0x447A1000u, 0x3F000000u), 0x3E800000u);
    expect("powf(2, 0) = 1", L_powf(0x40000000u, 0), 0x3F800000u);
    expect("powf(x, 1) = x", L_powf(0x40490FDBu, 0x3F800000u), 0x40490FDBu);
    expect("ieee754_sqrtf(4)", L_ieee_sqrtf(0x40800000u), 0x40000000u);
    expect("acosf(1) = 0", L_acosf(0x3F800000u), 0);
    /* error paths of the executable's wrappers (_LIB_VERSION 1, _XOPEN_); the errno cell is the game's */
    reent[0] = 0;
    expect("asinf(2) = +0 (DOMAIN)", L_asinf(0x40000000u), 0);
    expect("asinf(2) errno EDOM", (uint32_t)reent[0], 33);
    expect("atan2f(0, 0) = +0 (DOMAIN)", L_atan2f(0x80000000u, 0x80000000u), 0);
    expect("logf(0) = -inf pattern (SING)", L_logf(0), 0xFF800000u);
    expect("logf(-1) = -inf pattern (DOMAIN)", L_logf(0xBF800000u), 0xFF800000u);
    expect("powf(0, 0) = 1", L_powf(0, 0), 0x3F800000u);
    expect("powf(0, -1) = -inf pattern", L_powf(0, 0xBF800000u), 0xFF800000u);
    expect("fmodf(1, 0) = fp-bit NaN", L_fmodf(0x3F800000u, 0), 0x7FB00000u);
    reent[0] = 0;
    printf("ttt: float: libm: %d exact / error-path cases (general results PENDING a PCSX2 run of libm_test.c)\n",
           sCases);
    f = open_data("pcsx2_libm.txt");
    if (f != NULL) {
        /* libm_test.c's columns: a b c sinf cosf atanf atan2f asinf acosf logf powf fmodf floorf */
        uint32_t w[13], r[10];
        int bad = 0, rows = 0, i;

        while (read_words(f, w, 13) == 13) {
            r[0] = L_sinf(w[0]);
            r[1] = L_cosf(w[0]);
            r[2] = L_atanf(w[0]);
            r[3] = L_atan2f(w[0], w[1]);
            r[4] = L_asinf(w[2]);
            r[5] = L_acosf(w[2]);
            r[6] = L_logf(w[0] & 0x7FFFFFFFu);
            r[7] = L_powf(w[0] & 0x7FFFFFFFu, w[2]);
            r[8] = L_fmodf(w[0], w[1]);
            r[9] = L_floorf(w[0]);
            for (i = 0; i < 10; i++) {
                if (r[i] != w[3 + i] && bad++ < 5) {
                    printf("ttt: float: libm row %d column %d: PCSX2 %08x, port %08x\n", rows, 3 + i, w[3 + i], r[i]);
                }
            }
            rows++;
        }
        fclose(f);
        printf("ttt: float: libm (PCSX2 data): %d rows, %d results differ\n", rows, bad);
        sFailed |= bad != 0;
    }
}

static void group_rand(void) {
    uint32_t save = (uint32_t)D_004F5A8C[0x58 / 4];

    sCases = 0;
    srand(1);
    expect("rand #1 after srand(1)", (uint32_t)rand(), 1103527590u);
    expect("rand #2", (uint32_t)rand(), 377401575u);
    expect("rand #3", (uint32_t)rand(), 662824084u);
    expect("rand #4", (uint32_t)rand(), 1147902781u);
    expect("rand #5", (uint32_t)rand(), 2035015474u);
    expect("state word in EE RAM (_impure_ptr + 0x58)", (uint32_t)D_004F5A8C[0x58 / 4], 0xF94BDF32u);
    D_004F5A8C[0x58 / 4] = (int)save;
    printf("ttt: float: rand: %d cases\n", sCases);
}

/* ---- libvu0 fields ---- */
void func_0034A868(uint32_t *v0, const uint32_t *v1); /* sceVu0Normalize */
void sceVu0OuterProduct(uint32_t *v0, const uint32_t *v1, const uint32_t *v2);
void sceVu0InversMatrix(uint32_t *m0, const uint32_t *m1);
void sceVu0DivVectorXYZ(uint32_t *v0, const uint32_t *v1, w32 q);
void sceVu0ScaleVectorXYZ(uint32_t *v0, const uint32_t *v1, w32 s);
void sceVu0UnitMatrix(uint32_t *m);
void sceVu0RotMatrixX(uint32_t *m0, const uint32_t *m1, w32 rx);
void sceVu0RotMatrixY(uint32_t *m0, const uint32_t *m1, w32 ry);
void sceVu0RotMatrixZ(uint32_t *m0, const uint32_t *m1, w32 rz);
void sceVu0TransMatrix(uint32_t *m0, const uint32_t *m1, const uint32_t *tv);
void sceVu0ViewScreenMatrix(uint32_t *m, w32 scrz, w32 ax, w32 ay, w32 cx, w32 cy, w32 zmin, w32 zmax, w32 nearz,
                            w32 farz);
int sceVu0ClipAll(const uint32_t *minv, const uint32_t *maxv, const uint32_t *ms, const uint32_t *vm, int n);

/* sceVu0Rot*: the library's own cos / sin (0x34AAF0, called by RotMatrixX / Y / Z), not libm: the FPU's add.s / sub.s
   folds the angle t into a = pi/2 -/+ t (0x3FC90FDB), the VU0 sums cos = a + w a^3 + z a^5 + y a^7 + x a^9 with the
   coefficients at 0x4F5700 (the highest powers last), and sin = +/-sqrt(1 - cos^2) (vsqrt), negative when t < 0. */
/* The game's hand-written VU0 rotation builders (src/geometry_b.c, 0x349D70 / 0x349EE0 / 0x34A050 / 0x34A0D0) on
   the kernel func_00349CF8 (the library's series, coefficients D_0046B040): they store rows 0-2 only (0x349D70 also
   m[3][3] = 1.0, 0x34A050 row 3 = vf0), so a joint's translation in row 3 survives (joints 7-0x1C of
   func_00347760 keep their bone offsets). */
void func_00349D70(uint32_t *m, const uint32_t *ang);
void func_00349EE0(uint32_t *m, const uint32_t *ang);
void func_0034A050(uint32_t *m, w32 a);
void func_0034A0D0(uint32_t *m, w32 a);

static void group_vu0_rot(void) {
    static const uint32_t ang[4] = {0x3F800000u, 0, 0, 0}; /* (1, 0, 0) */
    uint32_t m[16];
    int i;

    for (i = 0; i < 16; i++) {
        m[i] = 0x40A00000u; /* 5.0 everywhere */
    }
    func_00349D70(m, ang);
    expect("func_00349D70 keeps the translation x", m[12], 0x40A00000u);
    expect("func_00349D70 keeps the translation z", m[14], 0x40A00000u);
    expect("func_00349D70 m[3][3] = 1", m[15], 0x3F800000u);
    expect("func_00349D70 Rx(1) cos (series)", m[5], 0x3F0A513Fu);
    expect("func_00349D70 Rx(1) sin", m[6], 0x3F576AA5u);
    expect("func_00349D70 row 0 w", m[3], 0);
    for (i = 0; i < 16; i++) {
        m[i] = 0x40A00000u;
    }
    func_00349EE0(m, ang);
    expect("func_00349EE0 keeps row 3 (w too)", m[15], 0x40A00000u);
    expect("func_00349EE0 Rx(1) -sin", m[9], 0xBF576AA5u);
    func_0034A0D0(m, 0x3F800000u);
    expect("func_0034A0D0 Rz(1) cos", m[0], 0x3F0A513Fu);
    expect("func_0034A0D0 keeps row 3", m[12], 0x40A00000u);
    expect("func_0034A0D0 row 2 = (0, 0, 1, 0)", m[10], 0x3F800000u);
    func_0034A050(m, 0x3F800000u);
    expect("func_0034A050 Rx(1) sin", m[6], 0x3F576AA5u);
    expect("func_0034A050 row 3 = vf0 (0, 0, 0, 1)", m[12] | m[13] | m[14], 0);
}

static void group_libvu0_rot(void) {
    uint32_t m[16], m1[16];
    static const uint32_t I[16] = {0x3F800000u, 0, 0, 0, 0, 0x3F800000u, 0, 0, 0, 0, 0x3F800000u, 0,
                                   0, 0, 0, 0x3F800000u};
    static const uint32_t minv[4] = {0xBF800000u, 0xBF800000u, 0, 0x3F800000u};  /* (-1, -1, 0, 1) */
    static const uint32_t maxv[4] = {0x3F800000u, 0x3F800000u, 0, 0x40000000u};  /* (1, 1, 0, 2) */
    static const uint32_t in[8] = {0x3F000000u, 0, 0, 0x3F99999Au,               /* (0.5, 0, 0, 1.2): inside */
                                   0x40000000u, 0, 0, 0x3F99999Au};              /* x = 2 > w: outside */
    static const uint32_t edge[4] = {0x3F800000u, 0, 0, 0x3F800000u};           /* x = w = max.x w: on the edge */

    sceVu0RotMatrixZ(m, I, 0x3F800000u); /* 1 rad */
    expect("sceVu0RotMatrixZ(1) cos (library series)", m[0], 0x3F0A513Fu);
    expect("sceVu0RotMatrixZ(1) sin (sqrt(1 - cos^2))", m[1], 0x3F576AA5u);
    expect("sceVu0RotMatrixZ(1) -sin", m[4], 0xBF576AA5u);
    expect("sceVu0RotMatrixZ(1) cos at [1][1]", m[5], 0x3F0A513Fu);
    sceVu0RotMatrixZ(m, I, 0x40000000u); /* 2 rad: a = pi/2 - 2 < 0 */
    expect("sceVu0RotMatrixZ(2) cos", m[0], 0xBED51130u);
    expect("sceVu0RotMatrixZ(2) sin", m[1], 0x3F68C7B7u);
    sceVu0RotMatrixX(m, I, 0x3F490FDBu); /* pi/4 */
    expect("sceVu0RotMatrixX(pi/4) cos", m[5], 0x3F3504F2u);
    expect("sceVu0RotMatrixX(pi/4) sin", m[6], 0x3F3504F4u);
    expect("sceVu0RotMatrixX(pi/4) -sin", m[9], 0xBF3504F4u);
    expect("sceVu0RotMatrixX(pi/4) cos at [2][2]", m[10], 0x3F3504F2u);
    sceVu0RotMatrixY(m, I, 0xBF800000u); /* -1 rad: sin = -sqrt */
    expect("sceVu0RotMatrixY(-1) cos", m[0], 0x3F0A513Fu);
    expect("sceVu0RotMatrixY(-1) -sin at [0][2]", m[2], 0x3F576AA5u);
    expect("sceVu0RotMatrixY(-1) sin at [2][0]", m[8], 0xBF576AA5u);
    expect("sceVu0RotMatrixY(-1) [1][1]", m[5], 0x3F800000u);
    sceVu0RotMatrixZ(m, I, 0);
    expect("sceVu0RotMatrixZ(0) sin", m[1], 0);

    /* 0x34AA20: the translation is loaded (lqc2 vf4, 0(a2)) before any store, so tv may be m0's own row 3 */
    memcpy(m1, I, sizeof m1);
    m1[12] = 0x3F800000u;
    memcpy(m, I, sizeof m);
    m[12] = 0x41200000u; /* m0 row 3 x = 10, used as tv */
    sceVu0TransMatrix(m, m1, &m[12]);
    expect("sceVu0TransMatrix tv read before m0 is written", m[12], 0x41300000u); /* 1 + 10 */

    /* 0x34AFB0: az / cz with the FPU's add.s / mul.s / div.s (EE code), not the VU's; m[2][2] = cz, m[3][2] = az */
    sceVu0ViewScreenMatrix(m, 0x44000000u, 0x3F800000u, 0x3F800000u, 0x45000000u, 0x45000000u, 0x3F800000u,
                           0x4B7FFFFFu, 0x3DCCCCCDu, 0x47C35000u);
    expect("sceVu0ViewScreenMatrix cz (FPU add.s / div.s)", m[10], 0xC17C6F8Au);
    expect("sceVu0ViewScreenMatrix az (FPU div.s)", m[14], 0x49CCCCD8u);

    /* 0x34B3F8: 0 as soon as one vertex has x - min.x w, y - min.y w, w - min.w, max.x w - x, max.y w - y and
       max.w - w all > 0 (sticky sign / zero flags clear); 1 when none has (z is not tested) */
    expect("sceVu0ClipAll one vertex inside", (uint32_t)sceVu0ClipAll(minv, maxv, I, in, 2), 0);
    expect("sceVu0ClipAll vertex outside", (uint32_t)sceVu0ClipAll(minv, maxv, I, in + 4, 1), 1);
    expect("sceVu0ClipAll vertex on the edge (zero flag)", (uint32_t)sceVu0ClipAll(minv, maxv, I, edge, 1), 1);
}

static void group_libvu0(void) {
    static const uint32_t v[4] = {0x40400000u, 0, 0x40800000u, 0x41200000u};  /* (3, 0, 4, 10) */
    static const uint32_t u[4] = {0x3F800000u, 0, 0, 0x3F000000u};            /* (1, 0, 0, 0.5) */
    static const uint32_t m[16] = {0, 0x3F800000u, 0, 0x3F000000u,           /* rotation about z, w column */
                                   0xBF800000u, 0, 0, 0x3F000000u,           /* filled with 0.5, translation */
                                   0, 0, 0x3F800000u, 0x3F000000u,           /* (1, 2, 3), w 2 */
                                   0x3F800000u, 0x40000000u, 0x40400000u, 0x40000000u};
    uint32_t r[16];

    sCases = 0;
    /* 0x34A868: vmul.xyz / vaddy.x / vaddz.x / vsqrt / vdiv; vsub.xyzw vf6, vf0, vf0; vmulq.xyz vf6, vf4, Q */
    func_0034A868(r, v);
    expect("sceVu0Normalize x (vdiv and vmul cut)", r[0], 0x3F199999u);
    expect("sceVu0Normalize z", r[2], 0x3F4CCCCCu);
    expect("sceVu0Normalize clears w", r[3], 0);
    /* 0x34A820: vopmula.xyz / vopmsub.xyz; vsub.w vf6, vf6, vf6 */
    sceVu0OuterProduct(r, u, v);
    expect("sceVu0OuterProduct clears w", r[3], 0);
    /* 0x34A8E8: rows 0-2 transposed with (0, 0, 0, w) as the fourth row: their w is 0; row 3 keeps m1's w */
    sceVu0InversMatrix(r, m);
    expect("sceVu0InversMatrix row 0 w", r[3], 0);
    expect("sceVu0InversMatrix row 1 w", r[7], 0);
    expect("sceVu0InversMatrix row 2 w", r[11], 0);
    expect("sceVu0InversMatrix row 3 x", r[12], 0xC0000000u);
    expect("sceVu0InversMatrix row 3 y", r[13], 0x3F800000u);
    expect("sceVu0InversMatrix row 3 z", r[14], 0xC0400000u);
    expect("sceVu0InversMatrix row 3 keeps w", r[15], 0x40000000u);
    /* 0x34A978: vmulq.xyz keeps w */
    sceVu0DivVectorXYZ(r, v, 0x40000000u);
    expect("sceVu0DivVectorXYZ keeps w", r[3], 0x41200000u);
    group_libvu0_rot();
    group_vu0_rot();
    printf("ttt: float: libvu0: %d cases\n", sCases);
}

void Port_FloatSelfTest(void) {
    group_pcsx2();
    group_rules();
    group_vu_fast();
    group_libm();
    group_rand();
    group_libvu0();
    if (!sFailed) {
        printf("ttt: float self-test OK\n");
    }
    exit(sFailed);
}
