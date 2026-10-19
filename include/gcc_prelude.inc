# Put in front of ee-gcc's assembly output (and of the asm a C file pulls in with INCLUDE_ASM) so the modern gas
# encodes it the way the game's 1999-2000 assembler did. Each rule was checked against the original bytes.
#
# Not needed for this compiler: the hazard nop gas keeps after an FPU compare is in the original too
# (`c.le.s / nop / bc1t`), and the compiler itself emits the nops the R5900 needs in front of div.s and sqrt.s
# and inside short loops. The hazard nops gas adds after mtc1, mfc0 and mfc1 are not in the original (macros below).

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

# \sym = the number of a general register operand written `$N` or by its name (`$v0`); 99 for anything else.
.macro __gpr_num sym, reg
    .set \sym, 99
    .irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
        .ifc \reg,$\n
            .set \sym, \n
        .endif
    .endr
    .set .L__gn, 0
    .irp name, zero, at, v0, v1, a0, a1, a2, a3, t0, t1, t2, t3, t4, t5, t6, t7, s0, s1, s2, s3, s4, s5, s6, s7, t8, t9, k0, k1, gp, sp, fp, ra
        .ifc \reg,$\name
            .set \sym, .L__gn
        .endif
        .set .L__gn, .L__gn + 1
    .endr
.endm

# `mfc0 rt, rd`: the game's assembler put no hazard nop after it when the next instruction reads rt (the modern gas
# does): `di / sync.p / mfc0 $2, $12 / and $2, $2, $3` in File_Init. Emitted as data, so gas sees no CP0 load.
.macro mfc0 rt, rd
    __gpr_num .L__rt, \rt
    __gpr_num .L__rd, \rd
    .if (.L__rt > 31) || (.L__rd > 31)
        .error "gcc_prelude.inc: mfc0 \rt, \rd: unknown register"
    .endif
    .word 0x40000000 | (.L__rt << 16) | (.L__rd << 11)
.endm

# `mtc1 rt, fs`: the game's assembler put no hazard nop after it when the next instruction reads fs
# (`mtc1 $4,$f0 / cvt.s.w $f0,$f0`), and never moved it into the delay slot of a branch behind it. Emitted as data,
# so gas does neither. (The `lui $at / mtc1 $at` that gas makes of `li.s` does not go through this macro.)
.macro mtc1 rt, fs
    __gpr_num .L__rt, \rt
    __fpr_num .L__fs, \fs
    .if (.L__rt > 31) || (.L__fs > 31)
        .error "gcc_prelude.inc: mtc1 \rt, \fs: unknown register"
    .endif
    .word 0x44800000 | (.L__rt << 16) | (.L__fs << 11)
.endm

# `mfc1 rt, fs`: as for mfc0, the game's assembler put no hazard nop after it when the next instruction reads rt
# (the modern gas does): `mfc1 $2,$f1 / subu $2,$0,$2` in func_00346230. Emitted as data.
.macro mfc1 rt, fs
    __gpr_num .L__rt, \rt
    __fpr_num .L__fs, \fs
    .if (.L__rt > 31) || (.L__fs > 31)
        .error "gcc_prelude.inc: mfc1 \rt, \fs: unknown register"
    .endif
    .word 0x44000000 | (.L__rt << 16) | (.L__fs << 11)
.endm
