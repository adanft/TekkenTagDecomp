#ifndef INCLUDE_ASM_H
#define INCLUDE_ASM_H

/* INCLUDE_ASM(FOLDER, NAME) puts a function that is not decompiled yet into the object, from the file splat writes
 * for it (asm/nonmatchings/<file>/<NAME>.s), so a C file can cover its whole address range from the start.
 * Tools that only parse the C (m2c context, decomp-permuter) see nothing. */
#if !defined(M2CTX) && !defined(PERMUTER)

#ifndef INCLUDE_ASM
#define INCLUDE_ASM(FOLDER, NAME) \
    __asm__( \
        ".section .text\n" \
        "    .set noat\n" \
        "    .set noreorder\n" \
        "    .include \"" FOLDER "/" #NAME ".s\"\n" \
        "    .set reorder\n" \
        "    .set at\n" \
    )
#endif
#ifndef INCLUDE_RODATA
#define INCLUDE_RODATA(FOLDER, NAME) \
    __asm__( \
        ".section .rodata\n" \
        "    .include \"" FOLDER "/" #NAME ".s\"\n" \
        ".section .text" \
    )
#endif

/* splat's glabel / endlabel / nonmatching macros used by the included files. */
__asm__(".include \"macro.inc\"\n");

#else

#ifndef INCLUDE_ASM
#define INCLUDE_ASM(FOLDER, NAME)
#endif
#ifndef INCLUDE_RODATA
#define INCLUDE_RODATA(FOLDER, NAME)
#endif

#endif /* !defined(M2CTX) && !defined(PERMUTER) */

#endif /* INCLUDE_ASM_H */
