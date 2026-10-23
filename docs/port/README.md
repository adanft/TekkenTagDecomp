# PC port: build pipeline and notes

The PC port is built from the same C as the matching decompilation. Nothing of the game is committed: the build
takes the game's data from the split of **your own** `SLUS_200.01` (the one the matching build already uses) and
writes everything under `build/port/`, which git ignores.

## Status (T21, 2026-10-22)

- Every game C file the matching build links (59) compiles for 32-bit x86 Linux with the host gcc, with the
  NON_MATCHING drafts switched on: every game function comes from C, except the hand-written EE / VU0 routines
  listed under [Hand replacements](#hand-replacements).
- The program links into `build/port/ttt` (32-bit ELF) with generated stubs. It is not expected to run yet: the
  PS2 memory regions (T22), the EE float model and the PS2's libm (T23) and the kernel / SIF / CD layer (T24) come
  next.
- Last clean build: 62 objects (59 game, 3 port), 1270 data objects, 22 names placed by address, 190 stubs
  (102 Sony SDK `sce*`, 39 other library functions by address, 24 kernel calls, 13 libgcc soft-float helpers,
  9 libm, 3 libmpeg internals).
- `TTT_CC=clang` (game code through clang 23) is not there yet: 55 of 62 objects; the other 7 stop on an implicit
  declaration followed by a definition with another return type, which gcc accepts with `-fpermissive`.

## Build

```sh
.venv/bin/python configure.py && ninja        # the matching build: splits your SLUS_200.01 into asm/
python3 port/tools/build.py                   # -> build/port/ttt
```

`build.py` runs the three steps below in order and fails if any of them fails. Each can also be run on its own
from the repository root. Heavy runs go through `scripts/cool.sh`.

| Step | Tool | Writes |
|---|---|---|
| 1 | `port/tools/gen_data.py` | `build/port/gen/data/*.s` (host assembly of the game's data), `build/port/obj_data/*.o`, `build/port/gen/chunks.txt` |
| 2 | `port/tools/undefined.py [-v]` | `build/port/gen/src/*.c` (sources with the drafts on), `build/port/obj/*.o`, `build/port/undefined.txt` (what a link still needs, by kind), `build/port/objects.txt` |
| 3 | `port/tools/link.py` | `build/port/gen/symbols.ld`, `build/port/gen/stubs.c`, `build/port/stubs.txt`, `build/port/ttt` and its map |

Shared modules: `toolchain.py` (paths, compiler flags, `TTT_CC`), `portsrc.py` (the list of game sources from
`config/SLUS_200.01.yaml` and the switch that turns drafts on). Their pure helpers are tested by
`tests/test_port_tools.py`.

## Architecture

### Game code

- The sources are the ones in `src/`. `portsrc.py` copies a source that still has `INCLUDE_ASM` lines to
  `build/port/gen/src/`, turning the `#if 0` around each draft into `#if 1`; the `INCLUDE_ASM` in the `#else` then
  drops out. A function with an `INCLUDE_ASM` and no C anywhere stops the build (`no C for:`); there are none.
- Flags (`toolchain.GAME_CFLAGS`): `-m32 -std=gnu89 -O2 -fno-strict-aliasing -ffp-contract=off -fcommon
  -fno-builtin -msoft-float -mno-sse -mno-mmx -malign-double -fpermissive`, plus `-include port_compat.h`.
  Software float, so that T23 can put the EE float model behind the float operations (`__addsf3`...); no SSE so
  no float work goes to vector registers; `-malign-double` gives 64-bit members the PS2's 8-byte alignment.
- `port/include/port_compat.h` is forced in front of every game source. It defines `PORT`, takes over
  `include/types.h` (`s64` / `u64` are `long long`: `long` is 64-bit on the PS2 and 32-bit here; `u128` is
  `unsigned _BitInt(128)` aligned to 16) and turns the split-inclusion macros (`INCLUDE_ASM`, `INCLUDE_RODATA`,
  `LIT4_*`, `RODATA_ALIGN*`, `ASM_STUB_*`) into nothing.
- `include/ee_asm.h` (included by `common.h`) names the single EE instructions the game writes as inline assembly:
  `EE_DI` / `EE_EI`, `EE_SYNC*`, `EE_NOP`, `EE_MFC0_STATUS`, `EE_SQRT_S`, `EE_WAIT_BC0F`. The matching build gets
  the exact statement the source had before; the PC build gets `port/src/plat_ee.c`, or nothing for the
  instructions that only order memory or the pipeline.
- The port's own C lives in `port/src/` and is compiled with the same software-float ABI as the game
  (`toolchain.PORT_CFLAGS`), because the game calls it with float arguments and results.

### Game data

- `gen_data.py` converts, for the host assembler, the data the matching build links from the split:
  every `asm/data/*.{data,rodata,lit4,sdata,sbss,bss}.s`, every table a C file takes with `INCLUDE_RODATA`, and
  the data blocks splat moved into the files of functions that are still drafts (strings and constants the draft
  C refers to by name). `.word` becomes `.long`, `.align N` becomes `.p2align N`, a word that points at a label
  inside library assembly becomes 0, and each chunk starts at its PS2 offset modulo 16. Jump tables are left out:
  the C makes its own. Everything goes into writable sections (the game patches some read-only tables).
- Each split chunk gets a label `__chunk_<file>`; `chunks.txt` records its PS2 address and its size.
- `link.py` asks the linker what nobody defines, then places what it can by PS2 address in
  `build/port/gen/symbols.ld`: a name inside a split chunk becomes that chunk's label plus the offset (labels splat
  did not make, such as `D_00437A50` inside `.data`), and a name outside the program image (hardware registers like
  `D_11000010`, the end of `.bss` `D_1FCDEF0`, constants splat took for addresses such as `D_100000`) becomes the
  absolute address. Everything left becomes a stub.

### Stubs

- A generated stub (`build/port/gen/stubs.c`) prints `ttt: not implemented yet: <name>` and exits with code 3.
  `build/port/stubs.txt` is the to-do list of the platform layer.
- `port/src/plat_stub.c` has the same helper (`Port_Missing`) for the port's own placeholders. Library functions
  move out of the generated list as they get real implementations (T24 and later).
- No host libm is linked. Under `-msoft-float` the game expects float results in `eax`; the host's `sinf` /
  `cosf` / `sqrtf` return them on the x87 stack, so a link with `-lm` would give wrong values without any error.
  They stay stubs until the PS2's newlib maths library is built in software float (T23). The host's i386 libgcc
  has no soft-float helpers (`__addsf3`...), so they are stubs too until `softfloat_ps2` (T23).
- The host C library still provides the integer-only functions the game calls (`memcpy`, `memset`, `strlen`,
  `strcmp`, `strcat`, `sprintf`, `printf`, `abs`, `exit`) and, for now, `rand` / `srand`. The game's random numbers
  must come from the PS2's newlib `rand` (simulation state, `docs/known_bugs.md`): T23.

### Addresses

`port/src/port_addr.h` has `Port_Ptr` / `Port_Addr`, plain casts in the 32-bit build (a game address is a host
address). The game also uses PS2 addresses by number (scratchpad `0x70000000`, hardware registers `0x10000000`,
VU0 memory `0x11000000`, the heap after `.bss`): they link as absolute addresses and fault when used until T22 maps
those regions.

## Hand replacements

Routines written in EE / VU0 assembly that the PC build cannot compile. They are guarded with `#ifndef PORT` in
`src/` (matched C made of inline assembly) or left off by `portsrc.REPLACED` (drafts around that assembly), and
`port/src/vu0_pc.c` supplies their names: two in C, the rest as placeholders that report themselves.

| Function | File | What it is | PC version |
|---|---|---|---|
| `Mtx_Unit`, `Mtx_Zero` | `theater_c.c` | identity / zero 4x4 matrix (`sq $0`) | C |
| `func_003435C0`, `func_003436B8`, `func_003437B8`, `func_003438B8` | `dma_gif.c` | VU0 macro-mode skinning loops (`vcallms`, jumps into their own bodies) | placeholder |
| `func_003E8FA0` | `tekken_bowl.c` | VU0 sin / cos (result in vf8) | placeholder |
| `func_003E9038`, `func_003E9120`, `func_003E91D0`, `func_003E92C0` | `tekken_bowl.c` | VU0 matrix rotations around it (results left in vf4-vf7) | placeholder |
| `func_003EA978`, `func_003EAF78`, `func_003EBC98`, `func_003EC388`, `func_003ECAD8`, `func_003ED338`, `func_003EEF00` | `tekken_bowl.c` | m2c drafts around that VU0 code (register names the C does not hold, raw `sqrt.s` words) | placeholder |
| `RotMtxXYZ` (inline helper of a draft) | `model_anim_b.c` | VU0 rotation x / y / z through `func_003E8FA0` | placeholder `Port_Vu0RotMtxXYZ` |
| `func_0034A180` | `geometry_b.c` | entry one instruction before `func_0034A184` (its first instruction is the delay slot of `func_0034A148`'s return) | placeholder |

Smaller hooks (all `#ifdef PORT` / `#ifndef PORT`, matching objects unchanged):

- `dma_spr.c` `func_0035D588` / `func_0035D608`: the 32-byte `ld` / `sd` copy of the scratchpad header is four
  `u64` copies.
- `dma_gif.c` draft `func_00343340`: its `lqc2` into vf8-vf11 is `Port_Vu0LoadVf` (the VU0 register file
  `gPortVu0Vf`, `plat_ee.c`).
- `modes.c`, `select_results.c`, `theater.c`, `effects_draw.c`: the `.set fptoui, __fixunssfsi` / `.set sqrtf, sqrt`
  aliases exist for the PS2 compiler's helper names and stay out of the host build.
- `battle_flow.c`: a prototype of the variadic `Text_Printf` before its first caller.

## Porting gotchas found so far

1. **No `mode(TI)` on 32-bit x86.** `u128` is `unsigned _BitInt(128)` with 16-byte alignment: gcc and clang both
   accept it in gnu89 mode, and it takes the game's `= 0`, casts and shifts (a struct would not).
2. **gcc 14+ turns old C into errors** (int / pointer conversions, incompatible pointer types). `-fpermissive`
   makes them warnings again for C; clang needs one `-Wno-error=` per diagnostic (`toolchain.CLANG_CFLAGS`).
3. **Drafts compiled together disagree.** Each draft compiled alone with the PS2 compiler; all of them in one file
   on gcc showed implicit declarations followed by definitions of another type (`dma_gif.c`), a draft written
   against an older structure (`select_results_b.c` `func_003C68F0`) and two m2c drafts declaring the same names
   with different types (`gallery_jpeg.c`). Fixed inside the drafts (dead code for the matching build).
4. **Data without labels.** C (mostly drafts) names data in the middle of a split chunk, or in a function's own
   split file; the matching build either never links that C (drafts) or resolves them through splat's
   `undefined_syms_auto.txt`. Here they are placed by address (`symbols.ld`) or taken from the function's file (`gen_data.py`).
5. **Calls relying on leftover registers.** `demo.c` calls `func_0034A180` with three arguments; the routine reads
   a fourth (`$a2`) that the callers leave from earlier code. The PC version must find which value that is.
6. **Soft-float ABI.** Float results travel in `eax` under `-msoft-float` and on the x87 stack in host libraries:
   no host libm, and port code that the game calls is built with the game's float flags.

## Next

T22 memory map (PS2 regions, heap at the PS2 addresses), T23 EE float model, `softfloat_ps2`, newlib libm and
`rand`, T24 kernel / SIF / CD replacements and VBlank, then the placeholders above by hand.
