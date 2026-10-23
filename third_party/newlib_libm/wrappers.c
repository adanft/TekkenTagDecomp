/*
 * The float maths wrappers of SLUS_200.01 for the PC port of Tekken Tag Tournament: newlib 1.10.0's wf_asin.c,
 * wf_atan2.c, wf_fmod.c, wf_log.c and wf_pow.c, built WITHOUT _IEEE_LIBM (the executable's versions read
 * __fdlib_version, call matherr and set errno: 0x407A28 asinf, 0x407B28 atan2f, 0x407C50 fmodf, 0x407D90 logf,
 * 0x407ED8 powf), plus the bit tests isnanf / finitef (0x40B6A8 / 0x40B598) and the newlib errno.
 *
 * Written here from the executable's control flow (this port did not copy newlib's wf_*.c): the result of an
 * error case is newlib's `struct exception` retval, a double that the PS2's software-double library (gcc 2.96
 * fp-bit) converts back to float; those conversions are reproduced on the bit patterns, so no double arithmetic is
 * done here. matherr is newlib's default (returns 0). __fdlib_version and the errno cell are the game's own, in EE
 * RAM: D_005F4848 (1, _XOPEN_ in the executable's data) and the `_errno` of the reentrancy structure that
 * _impure_ptr (D_004F5A8C) points to.
 *
 * The error results are NOT checked against a console or PCSX2 (docs/port/README.md, "EE float model").
 */
#include "fdlibm.h"

extern int D_005F4848;        /* __fdlib_version (_LIB_VERSION) */
extern int *D_004F5A8C;       /* _impure_ptr: struct _reent, `int _errno` first */

#define LIB_VERSION D_005F4848
#define V_IEEE (-1)
#define V_SVID 0
#define V_POSIX 2
#define EDOM_ 33
#define ERANGE_ 34

static void set_errno(int e) { D_004F5A8C[0] = e; }
static float word(__uint32_t w) { float f; SET_FLOAT_WORD(f, w); return f; }

/* Results of (float)retval through fp-bit: +-0.0, +-1.0, +-HUGE_VAL (infinity), NaN 0x7FF8000000000000 (fp-bit
   keeps the double's fraction shifted down 30 bits and sets its single-precision quiet bit 0x100000), +-HUGE
   (= +-FLT_MAX as a double, exact). */
#define R_ZERO 0x00000000u
#define R_ONE 0x3f800000u
#define R_INF 0x7f800000u
#define R_NINF 0xff800000u
#define R_NAN 0x7fb00000u
#define R_HUGE 0x7f7fffffu
#define R_NHUGE 0xff7fffffu

/* (float)(double)x for a NaN pattern x, through fp-bit: the fraction survives, both quiet bits are set. */
static __uint32_t nan_roundtrip(__uint32_t x) { return x | 0x300000u; }

int isnanf(float x) {
	__int32_t ix;
	GET_FLOAT_WORD(ix, x);
	return (__uint32_t)(0x7f800000 - (ix & 0x7fffffff)) >> 31;
}

int finitef(float x) {
	__int32_t ix;
	GET_FLOAT_WORD(ix, x);
	return (__uint32_t)((ix & 0x7fffffff) + (__int32_t)0x80800000) >> 31;
}

/* The common tail: matherr (newlib's default, 0) unless POSIX, then errno. */
static void report(int posix_errno, int matherr_errno) {
	if (LIB_VERSION == V_POSIX)
		set_errno(posix_errno);
	else
		set_errno(matherr_errno);
}

float asinf(float x) {
	float z = __ieee754_asinf(x);

	if (LIB_VERSION == V_IEEE || isnanf(x)) return z;
	if (fabsf(x) > word(R_ONE)) {
		report(EDOM_, EDOM_);
		return word(R_ZERO);
	}
	return z;
}

float atan2f(float y, float x) {
	float z = __ieee754_atan2f(y, x);

	if (LIB_VERSION == V_IEEE || isnanf(x) || isnanf(y)) return z;
	if (x == word(R_ZERO) && y == word(R_ZERO)) {
		report(EDOM_, EDOM_);
		return word(R_ZERO);
	}
	return z;
}

float fmodf(float x, float y) {
	float z = __ieee754_fmodf(x, y);
	__uint32_t r;

	if (LIB_VERSION == V_IEEE || isnanf(y) || isnanf(x)) return z;
	if (y == word(R_ZERO)) {
		GET_FLOAT_WORD(r, x);
		r = LIB_VERSION == V_SVID ? r : R_NAN;
		report(EDOM_, EDOM_);
		return word(r);
	}
	return z;
}

float logf(float x) {
	float z = __ieee754_logf(x);
	__uint32_t r;

	if (LIB_VERSION == V_IEEE || isnanf(x) || x > word(R_ZERO)) return z;
	r = LIB_VERSION == V_SVID ? R_NHUGE : R_NINF;
	if (x == word(R_ZERO))
		report(ERANGE_, EDOM_);	/* SING */
	else
		report(EDOM_, EDOM_);	/* DOMAIN */
	return word(r);
}

/* y / 2 is not an integer (rint(y * 0.5) != y * 0.5 in the executable, on a finite y). */
static int half_not_integer(__uint32_t y) {
	__int32_t e = (__int32_t)((y >> 23) & 0xff) - 127 - 1; /* exponent of y / 2 */
	__uint32_t m = (y & 0x7fffff) | 0x800000;

	if ((y & 0x7fffffff) == 0) return 0;
	if (e < 0) return 1;
	if (e >= 23) return 0;
	return (m & ((1u << (23 - e)) - 1)) != 0;
}

float powf(float x, float y) {
	float z = __ieee754_powf(x, y);
	__uint32_t ux, uy, uz;

	GET_FLOAT_WORD(ux, x);
	GET_FLOAT_WORD(uy, y);
	GET_FLOAT_WORD(uz, z);
	if (LIB_VERSION == V_IEEE || isnanf(y)) return z;
	if (isnanf(x)) {
		if (y == word(R_ZERO)) {		/* powf(NaN, 0.0) */
			if (LIB_VERSION == V_POSIX) return word(R_ONE);
			set_errno(EDOM_);
			return word(nan_roundtrip(ux));
		}
		return z;
	}
	if (x == word(R_ZERO)) {
		if (y == word(R_ZERO)) {		/* powf(0.0, 0.0) */
			if (LIB_VERSION != V_SVID) return word(R_ONE);
			set_errno(EDOM_);
			return word(R_ZERO);
		}
		if (finitef(y) && y < word(R_ZERO)) {	/* powf(0.0, negative) */
			report(EDOM_, EDOM_);
			return word(LIB_VERSION == V_SVID ? R_ZERO : R_NINF);
		}
		return z;
	}
	if (!finitef(z)) {
		if (finitef(x) && finitef(y)) {
			if (isnanf(z)) {		/* neg**non-integer */
				report(EDOM_, EDOM_);
				return word(LIB_VERSION == V_SVID ? R_ZERO : R_NAN);
			}
			/* overflow */
			{
				int neg = x < word(R_ZERO) && half_not_integer(uy);
				__uint32_t r = LIB_VERSION == V_SVID ? R_HUGE : R_INF;

				report(ERANGE_, ERANGE_);
				return word(neg ? r | 0x80000000u : r);
			}
		}
		return z;
	}
	if (z == word(R_ZERO) && finitef(x) && finitef(y)) {	/* underflow */
		report(ERANGE_, ERANGE_);
		return word(R_ZERO);
	}
	(void)uz;
	return z;
}
