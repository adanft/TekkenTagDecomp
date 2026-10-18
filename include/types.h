#ifndef TYPES_H
#define TYPES_H

/* ee-gcc: int and pointers are 32-bit, long is 64-bit. */
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long s64;
typedef unsigned long u64;
typedef float f32;
/* 128-bit integer (lq / sq, por), Sony's u_long128. */
typedef unsigned int u128 __attribute__((mode(TI)));

#endif
