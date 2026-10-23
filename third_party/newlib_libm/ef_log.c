/* ef_log.c -- float version of e_log.c.
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 *
 * PC port of Tekken Tag Tournament: this file was not in the set copied for the port, so it is written here after
 * fdlibm's algorithm as newlib 1.10.0 has it, operation for operation as __ieee754_logf of SLUS_200.01
 * (0x4093F0) performs it. The constants are written as their bit patterns (the values that executable holds), so
 * no conversion of decimal constants can change them.
 */

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

#include "fdlibm.h"

static float bits(__uint32_t w) { float f; SET_FLOAT_WORD(f, w); return f; }

#define ln2_hi bits(0x3f317180u)	/* 6.9313812256e-01 */
#define ln2_lo bits(0x3717f7d1u)	/* 9.0580006145e-06 */
#define two25  bits(0x4c000000u)	/* 3.355443200e+07 */
#define Lg1 bits(0x3f2aaaabu)		/* 6.6666668653e-01 */
#define Lg2 bits(0x3ecccccdu)		/* 4.0000000596e-01 */
#define Lg3 bits(0x3e924925u)		/* 2.8571429849e-01 */
#define Lg4 bits(0x3e638e29u)		/* 2.2222198546e-01 */
#define Lg5 bits(0x3e3a3325u)		/* 1.8183572590e-01 */
#define Lg6 bits(0x3e1cd04fu)		/* 1.5313838422e-01 */
#define Lg7 bits(0x3e178897u)		/* 1.4798198640e-01 */
#define zero bits(0)
#define half bits(0x3f000000u)
#define third bits(0x3eaaaaabu)		/* (float)0.33333333333333333 */

float __ieee754_logf(float x)
{
	float hfsq,f,s,z,R,w,t1,t2,dk;
	__int32_t k,ix,i,j;

	GET_FLOAT_WORD(ix,x);

	k=0;
	if (ix < 0x00800000) {			/* x < 2**-126  */
	    if ((ix&0x7fffffff)==0)
		return bits(0xff800000u);	/* log(+-0)=-inf (-two25/zero, folded by the compiler) */
	    if (ix<0) return (x-x)/zero;	/* log(-#) = NaN */
	    k -= 25; x *= two25; /* subnormal number, scale up x */
	    GET_FLOAT_WORD(ix,x);
	}
	if (ix > 0x7f7fffff) return x+x;
	k += (ix>>23)-127;
	ix &= 0x007fffff;
	i = (ix+(0x95f64<<3))&0x800000;
	SET_FLOAT_WORD(x,ix|(i^0x3f800000));	/* normalize x or x/2 */
	k += (i>>23);
	f = x-bits(0x3f800000u);
	if((0x007fffff&(15+ix))<16) {	/* |f| < 2**-20 */
	    if(f==zero) { if(k==0) return zero;  else {dk=(float)k;
				 return dk*ln2_hi+dk*ln2_lo;}}
	    R = f*f*(half-third*f);
	    if(k==0) return f-R; else {dk=(float)k;
		     return dk*ln2_hi-((R-dk*ln2_lo)-f);}
	}
	s = f/(bits(0x40000000u)+f);
	dk = (float)k;
	z = s*s;
	i = ix-(0x6147a<<3);
	w = z*z;
	j = (0x6b851<<3)-ix;
	t1= w*(Lg2+w*(Lg4+w*Lg6));
	t2= z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7)));
	i |= j;
	R = t2+t1;
	if(i>0) {
	    hfsq=half*f*f;
	    if(k==0) return f-(hfsq-s*(hfsq+R)); else
		     return dk*ln2_hi-((hfsq-(s*(hfsq+R)+dk*ln2_lo))-f);
	} else {
	    if(k==0) return f-s*(f-R); else
		     return dk*ln2_hi-((s*(f-R)-dk*ln2_lo)-f);
	}
}
