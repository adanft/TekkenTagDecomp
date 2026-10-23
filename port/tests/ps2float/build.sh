#!/bin/sh
# Builds the PCSX2 test ELFs with the decompilation's PS2 toolchain (tools/ of this repository):
#   test.elf       every FPU / VU operation class on 1500 inputs (test.c; its PCSX2 2.7.303 output is
#                  pcsx2_v2.7.303.txt, from the reference project: the same chip)
#   libm_test.elf  the game's own maths library (libm_test.c), packed with your SLUS_200.01
# Run each with `PCSX2 -nogui -fastboot -logfile <log> -- <elf>`; the results arrive in the log.
# Usage: port/tests/ps2float/build.sh <path of SLUS_200.01> <output directory>
set -e
D="$(cd "$(dirname "$0")" && pwd)"
R="$D/../../.."
OUT="$2"
CC="$R/tools/compilers/ee-gcc2.9-991111-01/bin/ee-gcc"
B="$R/tools/binutils/mips-ps2-decompals-"
AS="${B}as -EL -march=r5900 -mabi=o64 -no-pad-sections -mno-pdr -G0 -I$R/include"
PRE="$R/include/gcc_prelude.inc"  # the game's assembler conventions (configure.py)
mkdir -p "$OUT"
"$CC" -O1 -G0 -fno-strict-aliasing -S "$D/test.c" -o "$OUT/test.s"
$AS "$PRE" "$OUT/test.s" -o "$OUT/test.o"
$AS "$D/crt0.s" -o "$OUT/crt0.o"
"${B}ld" -EL -Ttext 0x100000 -e _start -o "$OUT/test.elf" "$OUT/crt0.o" "$OUT/test.o"
"$CC" -O1 -G0 -fno-strict-aliasing -S "$D/libm_test.c" -o "$OUT/libm_test.s"
$AS "$PRE" "$OUT/libm_test.s" -o "$OUT/libm_test.o"
$AS "$D/crt0_libm.s" -o "$OUT/crt0_libm.o"
"${B}ld" -EL -Ttext 0x01000000 -e _start -o "$OUT/libm_drv.elf" "$OUT/crt0_libm.o" "$OUT/libm_test.o"
python3 "$D/pack_libm.py" "$1" "$OUT/libm_drv.elf" "$OUT/libm_test.elf"
