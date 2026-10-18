#ifndef GTE_H
#define GTE_H

#include "types.h"

/* Fixed-point 3D math in the style of the PlayStation GTE library (1.0 = 0x1000), done on the EE CPU. */

/* A rotation matrix (1.0 = 0x1000) and a translation. */
typedef struct {
    /* 0x00 */ s16 m[3][3];
    /* 0x14 */ s32 t[3];
} GteMatrix; /* size 0x20 */

typedef struct {
    /* 0x0 */ s32 vx;
    /* 0x4 */ s32 vy;
    /* 0x8 */ s32 vz;
    /* 0xC */ s32 pad;
} GteVector;

typedef struct {
    /* 0x0 */ s16 vx;
    /* 0x2 */ s16 vy;
    /* 0x4 */ s16 vz;
    /* 0x6 */ s16 pad;
} GteSVector;

#endif
