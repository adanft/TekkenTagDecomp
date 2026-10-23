/*
 * Forced in front of the newlib float maths sources (-include newlib_shim.h) for the PC port of Tekken Tag
 * Tournament. The sources are newlib 1.10.0's float fdlibm, as linked into SLUS_200.01 (0x407640-0x40B860: the same
 * functions, constants and call graph in its disassembly). This supplies the newlib-internal macros and types they
 * expect so they build with the host's headers. They are compiled with software float like the game, so every float
 * operation in them goes through the EE float model of port/src/softfloat_ps2.c.
 *
 * The library's internal helpers that glibc's libc.so also exports get a private name, so nothing outside the
 * maths library can bind to the soft-float versions by accident (or the other way round). fabsf keeps its name:
 * the game calls it.
 */
#ifndef NEWLIB_SHIM_H
#define NEWLIB_SHIM_H
#include <stdint.h>
#include <math.h>
#define _DEFUN(name, arglist, args) name(args)
#define _AND ,
#define _CONST const
#define _PARAMS(paramlist) paramlist
#define _EXFUN(name, proto) name proto
#ifndef __int32_t
#define __int32_t int32_t
#define __uint32_t uint32_t
#endif
#define scalbnf Nl_scalbnf
#define copysignf Nl_copysignf
#define floorf Nl_floorf
#define isnanf Nl_isnanf
#define finitef Nl_finitef
/* glibc's math.h declares these through token pasting, which the renames above do not reach: without these
   prototypes a call would be an implicit declaration (an int result, the float argument promoted to double). */
float Nl_scalbnf(float, int);
float Nl_copysignf(float, float);
float Nl_floorf(float);
int Nl_isnanf(float);
int Nl_finitef(float);
#endif
