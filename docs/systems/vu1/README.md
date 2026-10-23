# VU microcode

All of the game's VU microcode is in `.vudata` (0x4F5F00-0x4FAF70): four DMA chains with VIF `MPG` codes, three for
VU0 and one for VU1. `.vutext` is empty. The build assembles these packets from the listings in `src/vu0/` and
`src/vu1/` with `scripts/vuasm.py`, and the executable keeps its SHA-1.

Labels: **[V]** Verified (code, data or a byte comparison), **[I]** Inferred (what the instructions do, not
confirmed by running them).

## Quick path

```sh
.venv/bin/python scripts/vuasm.py                    # round trip: every packet of the ELF and its listing
.venv/bin/python scripts/vuasm.py --disasm iso/SLUS_200.01 0x1B8A00 0x1BB7E0 vu1   # a packet (file offsets)
.venv/bin/python configure.py && ninja               # build/vu/*.bin -> src/vu1/vudata.s -> SHA-1 check
```

After adding a listing file, run `configure.py` again so ninja knows about it.

## Packets [V]

| Address | Label | Size | VIF | Listing | Sent by |
|---|---|---|---|---|---|
| 0x4F5F00 | `D_004F5F00` | 0xD80 | VIF0 | `src/vu0/vu0_full.vsm` | `Gfx_Init` (once), `Vu0_LoadMicrocode` |
| 0x4F6C80 | `D_004F6C80` | 0xD80 | VIF0 | `src/vu0/vu0_full.vsm` (byte-identical copy) | `Vu0_LoadMicrocode` |
| 0x4F7A00 | `D_004F7A00` | 0x2DE0 | VIF1 | `src/vu1/vu1.vsm` | `Gfx_Init` (once) |
| 0x4FA7E0 | `D_004FA7E0` | 0x790 | VIF0 | `src/vu0/vu0_small.vsm` | `Vu0_LoadMicrocode` in mode 5 |

Each packet is: DMA `cnt` (STCYCL 4,4, STMASK 0, STMOD 0), DMA `cnt` with one MPG code per program separated by
VIF NOPs, then DMA `end`. The VU1 packet also sets BASE 0 / OFFSET 0x200, unpacks two quadwords of constants to
VU1 memory 0..1 and runs MSCAL 0x4 (init).

`Vu0_LoadMicrocode` (0x3432A0, from `Frame_Begin` every frame) picks: mode 5 (`Mode_CharSelect`) -> 0x4FA7E0;
otherwise 0x4F6C80 when `func_003BB4D8()` or `func_003ABCD0()` is non-zero and the byte at 0x885007 is 0, else
0x4F5F00. The last two are the same bytes, so only mode 5 changes the microcode.

The rest of `.vudata` (0x4FAF70-0x4FB810) is not microcode: a VIF1 block template (UNPACK 0x85 quadwords to
VU1 memory 0, MSCAL 0x55A) that `vu1_packet` copies to the scratchpad and patches (`model_draw` does not use it, T17 b1). It stays split
data.

### Cross-check with the ELF's overlay sections [V]

The unloaded tail holds `.DVP.ovlytab` (28 entries of 12 bytes: name offset, LMA, VMA), `.DVP.ovlystrtab` and 28
`.DVP.overlay..<VMA>.<unit>.<line>.<n>` sections, one per MPG block. Each LMA is the address of an MPG block's
code in `.vudata`, each VMA is 8 x the MPG address, each section size is 8 x the instruction count: all 28 match
the packets. The unit number is one per packet (55283, 111347, 55299, 111299, in address order) [I: one source
file each]; `unknvma` names the second half of the 504-instruction VU1 program, split by the assembler at 256
instructions. The overlay sections' own bytes are not the code (zeros or unrelated bytes); they stay in the
`trailer` blob.

## VU0 programs

Called with `vcallms` (byte address) by four hand-written EE loops, one vertex per call [V]. Each skins a vertex
with up to four bones and returns colour (vf20) and screen position (vf22); the EE stores ST, RGBA and XYZF2 of
a GIF packet.

| Address | vcallms | Instr. | Listing | Caller | Role [I] |
|---|---|---:|---|---|---|
| 0x000 | 0x0 | 71 | `skin_diffuse.vsm` | `func_003435C0` | diffuse lighting with lights in bone space |
| 0x04A | 0x250 | 148 | `skin_specular.vsm` | `func_003436B8` | diffuse + specular highlight (exponent 2^n) |
| 0x0EA | 0x750 | 118 | `skin_gloss.vsm` | `func_003437B8` | highlight only, world -> screen via memory 4..7 |
| 0x186 | 0xC30 | 81 | `skin_envmap.vsm` | `func_003438B8` | environment-mapped ST, constant colour |
| 0x04A (mode 5) | 0x250 | 71 | `skin_diffuse_world.vsm` | `func_003436B8` | diffuse only |
| 0x0EA (mode 5) | 0x750 | 4 | `skin_hide.vsm` | `func_003437B8` | marks every vertex "not drawn" |

0x000 and 0x186 are the same in both packets (shared listings).

### VU0 conventions

| Item | Contents |
|---|---|
| Vertex record (EE, a1) [V] | +0x00 vf7 (ST, copied out), +0x10 four bone addresses -> vi2..vi5 (0 ends), +0x20 + 0x20*i normal_i -> vf2..vf5, +0x30 + 0x20*i position_i -> vf26..vf29 |
| Registers set per model [V] | vf8..vf11 light colour matrix (3 lights + ambient); vf6/vi6 specular intensity and exponent steps |
| Bone block [I] | rows 0..3 bone -> screen, 4..7 bone -> world, 8..10 light directions in bone space |
| Data memory, vi0-relative [I] | 0 guard-band scale (w: colour clamp 255), 1 ADC word, 4..7 world -> screen, 8..10 world light directions, 11 eye vector, 252..255 environment matrix (copied from `D_00433400` at 0x343444 [V]) |
| Clipping [I] | guard-band test of the last three vertices (`fcand vi1, 0x3FFFF`); outside -> XYZF.w = memory 1 (ADC) |

## VU1 programs

| Address | Instr. | Listing | Selected by | Role [I unless noted] |
|---|---:|---|---|---|
| 0x000 | 2 | `stub.vsm` | - | ends at once |
| 0x004 | 38 | `init.vsm` | boot packet MSCAL 0x4 [V] | clears registers, loads constants (below) |
| 0x032 | 19 | `object.vsm` | MSCAL in template 0x46B160 (`render_fighter_a`) [V] | per-object constants from TOP |
| 0x050 | 504 | `mesh.vsm` | `func_00356A78` kinds 0, 1, 2, 4 [V] | rigid batch, 8 variants by segment flags (lit, prelit, tinted, 4 env-map kinds, flat depth) |
| 0x258 | 72 | `skin2.vsm` | `func_003D8158` kind 3 [V] | two-matrix skin, constant colour |
| 0x2BC | 75 | `skin2_depth.vsm` | `func_003D8158` kind 2 [V] | as skin2, depth from the bone matrix |
| 0x320 | 52 | `skin2_flat.vsm` | `func_003D8158` kind 1 [V] | two-matrix skin, no ST, no clipping |
| 0x3A2 | 56 | `projtex.vsm` | `func_00356A78` kind 5 [V] | projected texture coordinates |
| 0x3E8 | 69 | `lit_fast.vsm` | not found | lit, pipelined, no clipping |
| 0x438 | 253 | `strip.vsm` | not found | triangle strips with back-face culling; lit or env-mapped |
| 0x55A | 6 | `lights.vsm` | template at 0x4FAF80 [V] | light colours from memory 126..129 |
| 0x564 | 77 | `skin2_lit.vsm` | `func_003D8158` kinds 0, >= 4 [V] | two-matrix skin with lighting |
| 0x640 | 86 | `lattice.vsm` | `func_00356A78` kind 3 [V] | 2x2x2 lattice (trilinear) deformation |
| 0x708 | 109 | `sprites.vsm` | `func_003E6DA0` MSCAL then MSCNT [V] | quads built around points |
| 0x780 | 19 | `object2.vsm` | not found | copy of `object.vsm` |

`func_00356A78` and `func_003D8158` write the chosen address into the chain as `MSCALF` (`0x15000000 | addr`)
[V]. The disc's pack files hold many `MSCALF 0x0` codes in VIF context; whether they are patched before use is
open.

### VU1 memory and calling convention

| Item | Contents |
|---|---|
| Double buffer [V] | BASE 0, OFFSET 0x200: each batch is unpacked at TOP = 0x000 or 0x200 (`xtop`) |
| Constants [V] | memory 0 = (0.25, 1/pi, 255, 0.5) -> vf9; memory 1 = (1/2046, 1/2046, 1/32767, 3.0) -> vf31 |
| Fixed registers (init) [V] | vf1 = (1,1,1,1), I = 128, vi12 = 0x7FFF, vi13 = 0x8000 |
| Object constants (0x032/0x780) [V for the loads, I for roles] | TOP+0..3 vf10..13 local -> screen; +4..6 vf2..4 light directions; +8..11 vf5..8 light colours (vf8 ambient; vf5.w, vf6.w fog); +12..14 vf28..30 env matrix; +15 vf21 eye position |
| Batch input [V for the reads] | TOP+0 flags; +1..4 copied to the output; +5 vertex GIF tag (NLOOP, EOP: clear = another segment follows); +6.. vertices (4 quadwords rigid, 5 skinned) |
| Output | TOP+0x106 (skinned: TOP+0xE1/0xE4): the copied header, then ST/Q, RGBA, XYZF2 per vertex; fog in XYZF2.w bits 4..11 [I] |
| Kick [V] | a batch program's first instruction is `xgkick vi7` (the previous batch's output), so each batch is sent by the next call |
| Restart [V] | programs end with `[E]` followed by `b <entry>`: an `MSCNT` runs the same program for the next batch |
| Clipping [I] | xyz * vf31 - 1 against +-1 (the GS coordinate range); `fcand vi1, 0x3FFFF` over the last three vertices sets ADC |

### PC port (T28)

`port/src/gs_vif1.c` carries out the VIF1 side: UNPACK into VU1 data memory (0x1100C000) with STCYCL / STMASK /
STROW / STCOL / STMOD and the BASE / OFFSET double buffer, MPG into micro memory (0x11008000), DIRECT to the GIF.
`MSCAL` / `MSCALF` / `MSCNT` only latch TOP / ITOP and swap the buffer: no program runs, so nothing is kicked on PATH1
and the models are missing from the picture. A headless run to the attract demo (1200 VBlanks) sees 16 MPG uploads
and about 56,000 program calls (`ttt: gs:` line of the `TTT_FRAMES` report). The interpreter (T29) plugs in at
`call()` and kicks with `Gs_Gif(1, ...)`.

## Listing syntax

See the docstring of `scripts/vuasm.py`. Packets use `.dmatag`, `.vif`, `.word`, `.mpg` and `.include`; a
program listing holds one or more `.mpg` blocks of `upper | lower [flags]` pairs. Labels are local to their
file. Every instruction the game uses has a mnemonic (no `raw` words); `vuasm.py` round-trips all 28 MPG blocks.

## Open questions

- Callers of 0x3E8, 0x438 and 0x780.
- What the many `MSCALF 0x0` codes in the disc's model data do (stub, or patched by the game).
- Runtime confirmation of the inferred roles (PCSX2 VU1 debugger, T29).
