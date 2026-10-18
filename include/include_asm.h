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

/* In front of the INCLUDE_ASM of a function whose jump table must start on a 16-byte boundary of the object's
 * .rodata: the compiler aligns jump tables to 16 (`.align 4`), the generated .s file only to 8. */
#ifndef RODATA_ALIGN16
#define RODATA_ALIGN16() \
    __asm__( \
        ".section .rodata\n" \
        "    .balign 16\n" \
        ".section .text" \
    )
#endif

/* In front of the INCLUDE_ASM of a function that owns a float constant of the file's .lit4 pool: emits the
 * constant under the label the function's .s file uses, in place, so the pool keeps its order. */
#ifndef LIT4_WORD
#define LIT4_WORD(NAME, HEX) \
    __asm__( \
        ".section .lit4\n" \
        #NAME ":\n" \
        "    .word " #HEX "\n" \
        ".section .text" \
    )
#endif

/* Around the C attempt of a function that stays INCLUDE_ASM when later functions of the file only match once the
 * compiler has seen its definition: the attempt is compiled but the assembler skips it (`.if 0`), the code comes
 * from the INCLUDE_ASM that follows. The attempt must not define function-local static data.
 *     ASM_STUB_BEGIN();
 *     void f(void) { ... }
 *     ASM_STUB_END();
 *     INCLUDE_ASM("asm/nonmatchings/...", f); */
#ifndef ASM_STUB_BEGIN
#define ASM_STUB_BEGIN() __asm__(".if 0")
#define ASM_STUB_END() __asm__(".endif")
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
#ifndef RODATA_ALIGN16
#define RODATA_ALIGN16()
#endif
#ifndef LIT4_WORD
#define LIT4_WORD(NAME, HEX)
#endif
#ifndef ASM_STUB_BEGIN
#define ASM_STUB_BEGIN() extern int asm_stub_begin_
#define ASM_STUB_END() extern int asm_stub_end_
#endif

#endif /* !defined(M2CTX) && !defined(PERMUTER) */

#endif /* INCLUDE_ASM_H */
