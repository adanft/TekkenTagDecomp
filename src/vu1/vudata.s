/* .vudata 0x4F5F00-0x4FAF70: the VU microcode packets (docs/systems/vu1/README.md). Each build/vu/*.bin is
   assembled from a listing by scripts/vuasm.py (configure.py). The two VU0 packets at 0x4F5F00 and 0x4F6C80 are
   byte-identical, so both come from src/vu0/vu0_full.vsm. The labels are the ones the game code uses:
   Gfx_Init sends D_004F5F00 (VIF0) and D_004F7A00 (VIF1), Vu0_LoadMicrocode one of the three VU0 packets. */
.include "macro.inc"

.section .data, "wa"

dlabel D_004F5F00
.incbin "build/vu/vu0_full.bin"
enddlabel D_004F5F00

dlabel D_004F6C80
.incbin "build/vu/vu0_full.bin"
enddlabel D_004F6C80

dlabel D_004F7A00
.incbin "build/vu/vu1.bin"
enddlabel D_004F7A00

dlabel D_004FA7E0
.incbin "build/vu/vu0_small.bin"
enddlabel D_004FA7E0
