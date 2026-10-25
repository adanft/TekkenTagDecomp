/*
 * Forced into every game source of the PC build (-include port_compat.h), in front of the game's own headers.
 * The game sources under src/ and include/ are shared with the matching decompilation; differences between the PS2
 * compiler and a host compiler are bridged here, and only where that is impossible by `#ifdef PORT` in the source
 * (each such hook leaves the matching build's objects unchanged). See docs/port/README.md.
 */
#ifndef PORT_COMPAT_H
#define PORT_COMPAT_H

#define PORT 1

/* The game's types (include/types.h): on the PS2 `long` is 64 bits, on 32-bit x86 it is 32. The 128-bit integer of
 * the EE (lq / sq, Sony's u_long128) has no mode(TI) on 32-bit x86, but gcc and clang both have the C23 _BitInt(128)
 * there, which takes the game's `= 0`, casts and shifts; aligned to 16 bytes as on the PS2 (structure layouts). */
#define TYPES_H
typedef signed char s8;
typedef unsigned char u8;
typedef short s16;
typedef unsigned short u16;
typedef int s32;
typedef unsigned int u32;
typedef long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef unsigned _BitInt(128) u128 __attribute__((aligned(16)));

/* Functions and tables the matching build takes from the split: the PC build compiles the C of every function
 * (port/tools/portsrc.py switches the drafts on) and gen_data.py supplies the tables. */
#define INCLUDE_ASM_H
#define INCLUDE_ASM(FOLDER, NAME)
#define INCLUDE_RODATA(FOLDER, NAME)
#define RODATA_ALIGN16()
#define RODATA_ALIGN8()
#define LIT4_WORD(NAME, HEX)
#define LIT4_WORD_AT(N, NAME, HEX)
#define LIT4_ORDER(N) extern int lit4_order_
#define LIT4_REF(NAME) extern int lit4_ref_
#define LIT4_KEEP(N) extern int lit4_keep_
#define ASM_STUB_BEGIN() extern int asm_stub_begin_
#define ASM_STUB_END() extern int asm_stub_end_

/* Platform hooks the game sources call under `#ifdef PORT` (port/src/plat_hw.c). */
int Port_BootSeed(int timer0); /* the seed Rng_Init takes: Timer 0 now, or TTT_SEED */
int Port_MoviePlayable(void);  /* 0 until the movie player exists: func_0039C878 skips the movie */
/* TTT_DIFF (port/src/ee_diff.c): calls fn as usual, and on request compares its result with the original EE code at
   addr. sig: one letter per argument, 'i' for an integer / pointer, 'f' for a float passed as its bits, then ":i" /
   ":f" to compare an integer / float result too. Returns fn's result (as its bits). */
unsigned Port_DiffCall(const char *name, unsigned addr, void *fn, const char *sig, ...);
#define PORT_DIFF(addr, fn, sig, ...) Port_DiffCall(#fn, addr, (void *)fn, sig, __VA_ARGS__)
/* One quadword of EE RAM at a fixed address, for game code that DMAs a stack local whose bytes and address would follow
   the compiler's frame layout (dma_spr.c func_0035D790): above the top of the EE stack (port/src/plat_main.c starts
   it at 0x01FFFF00), so neither the game nor the port's frames reach it. */
#define PORT_EE_SPARE_QW ((u128 *)0x01FFFFF0)

#endif
