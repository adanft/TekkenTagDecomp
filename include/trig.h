#ifndef TRIG_H
#define TRIG_H

#include "types.h"

/* Sine and cosine of a 16-bit angle (0x10000 = one turn; callers pass signed angles, only the low 16 bits count),
 * from the table gSinTable, clamped to +-0xFFF and returned * 8 (src/main.c). */
s16 Math_Sin(s32 angle);
s16 Math_Cos(s32 angle);

#endif
