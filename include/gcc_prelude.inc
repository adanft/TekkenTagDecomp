# Put in front of ee-gcc's assembly output (and of the asm a C file pulls in with INCLUDE_ASM) so the modern gas
# encodes it the way the game's 1999-2000 assembler did. Each rule was checked against the original bytes.
#
# Not needed for this compiler: the hazard nops gas keeps after an FPU compare or mtc1/mfc1 are in the original
# too (`c.le.s / nop / bc1t`), and the compiler itself emits the nops the R5900 needs in front of div.s and sqrt.s
# and inside short loops.

# Register moves were 64-bit: `move` is daddu, not gas's `or`.
.macro move dst, src
    daddu \dst, \src, $0
.endm

# `break N` (division-by-zero traps) put N in the low code field, where the two-operand form `break 0, N` has it.
.macro break code, code2
    .ifb \code2
        .word (\code << 6) | 0xD
    .else
        .word (\code << 16) | (\code2 << 6) | 0xD
    .endif
.endm

# \sym = the number of an FPU register operand written `$fN`; 99 for anything else.
.macro __fpr_num sym, reg
    .set \sym, 99
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
        .ifc \reg,$f\n
            .set \sym, \n
        .endif
    .endr
.endm

# `cvt.w.s fd, fs`: the R5900 float-to-int conversion (it truncates). The original has it with the standard
# encoding (function 0x24); the modern gas refuses the mnemonic for this CPU.
.macro cvt.w.s fd, fs
    __fpr_num .L__fd, \fd
    __fpr_num .L__fs, \fs
    .if (.L__fd > 31) || (.L__fs > 31)
        .error "gcc_prelude.inc: cvt.w.s \fd, \fs: operands must be written $fN"
    .endif
    .word 0x46000024 | (.L__fs << 11) | (.L__fd << 6)
.endm

# `sqrt.s fd, ft`: the R5900 takes the operand from the ft field (bits 16-20); the modern gas puts it in fs.
.macro sqrt.s fd, ft
    __fpr_num .L__fd, \fd
    __fpr_num .L__ft, \ft
    .if (.L__fd > 31) || (.L__ft > 31)
        .error "gcc_prelude.inc: sqrt.s \fd, \ft: operands must be written $fN"
    .endif
    .word 0x46000004 | (.L__ft << 16) | (.L__fd << 6)
.endm
