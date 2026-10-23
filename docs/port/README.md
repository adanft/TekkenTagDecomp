# PC port: build pipeline and notes

The PC port is built from the same C as the matching decompilation. Nothing of the game is committed: the build
takes the game's data from the split of **your own** `SLUS_200.01` (the one the matching build already uses) and
writes everything under `build/port/`, which git ignores.

## Status (T22, 2026-10-22)

- Every game C file the matching build links (59) compiles for 32-bit x86 Linux with the host gcc, with the
  NON_MATCHING drafts switched on: every game function comes from C, except the hand-written EE / VU0 routines
  listed under [Hand replacements](#hand-replacements).
- The program links into `build/port/ttt` (32-bit ELF) with generated stubs and runs on the PS2's memory map
  ([Memory map](#memory-map)): the game's data at its PS2 addresses, EE RAM with its uncached views, the hardware
  registers and the scratchpad. It starts the game's `main` and stops at the first missing library function,
  `sceSifInitRpc` (the first call of `Sys_Init`). With `TTT_STUB_RETURN=1` it goes on through `Sys_BootIop`,
  `sceDmaReset`, `sceVpu0Reset`, `sceGsResetPath`, the register writes of `Sys_Init`, `func_0035D458`,
  `func_0035E400` and into `Gfx_Init`, where `Dma_Wait` waits forever for the first VIF0 transfer: the registers
  are plain memory and nothing runs the DMA yet. The EE float model and the PS2's libm (T23), the kernel / SIF / CD
  layer and VBlank (T24) and the DMA / GS path (T28) come next.
- Last clean build: 63 objects (59 game, 4 port), 1270 data objects, 22 names placed by address, 190 stubs
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
| 1 | `port/tools/gen_data.py` | `build/port/gen/data/*.s` (host assembly of the game's data), `build/port/obj_data/*.o`, `build/port/gen/chunks.txt`, `build/port/gen/image.txt` |
| 2 | `port/tools/undefined.py [-v]` | `build/port/gen/src/*.c` (sources with the drafts on), `build/port/obj/*.o`, `build/port/undefined.txt` (what a link still needs, by kind), `build/port/objects.txt` |
| 3 | `port/tools/link.py` | `build/port/gen/image.ld`, `build/port/gen/symbols.ld`, `build/port/gen/stubs.c`, `build/port/stubs.txt`, `build/port/ttt` and its map |

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
  inside library assembly becomes 0. Jump tables are left out: the C makes its own. Each file goes into a writable
  section of its own, `.ttt.<file>` (the game patches some read-only tables), aligned only as far as its PS2 address
  is, and `image.txt` lists each section with its PS2 address, size and kind.
- `link.py` turns `image.txt` into `image.ld`, a linker script (`INSERT AFTER .bss`) that puts every one of those
  sections at its PS2 address: the initialised ones in the output section `.ttt_data` (0x00415700 on), `.sbss` and
  `.bss` in `.ttt_bss` (0x005F6B00 to 0x01FCDEF0). Overlapping files are refused. The game's code stays native
  wherever the host linker puts it (0x08049000 on).
- Each split chunk gets a label `__chunk_<file>`; `chunks.txt` records its PS2 address and its size.
- `link.py` asks the linker what nobody defines, then places what it can by PS2 address in
  `build/port/gen/symbols.ld`: a name inside a split chunk becomes that chunk's label plus the offset (labels splat
  did not make, such as `D_00437A50` inside `.data`), and a name outside the program image (hardware registers like
  `D_11000010`, the end of `.bss` `D_1FCDEF0`, constants splat took for addresses such as `D_100000`) becomes the
  absolute address. Everything left becomes a stub.

### Stubs

- A generated stub (`build/port/gen/stubs.c`) prints `ttt: not implemented yet: <name>` and exits with code 3.
  `build/port/stubs.txt` is the to-do list of the platform layer.
- `TTT_STUB_RETURN=<n>` (a developer's switch): every stub prints its first call and returns `n` instead, to see how
  far the start gets past the missing functions; a stub called a million times (a retry loop such as
  `while (sceSifRebootIop(...) == 0)` with `n = 0`) stops the program.
- `port/src/plat_stub.c` has the same helper (`Port_Missing`) for the port's own placeholders. Library functions
  move out of the generated list as they get real implementations (T24 and later).
- No host libm is linked. Under `-msoft-float` the game expects float results in `eax`; the host's `sinf` /
  `cosf` / `sqrtf` return them on the x87 stack, so a link with `-lm` would give wrong values without any error.
  They stay stubs until the PS2's newlib maths library is built in software float (T23). The host's i386 libgcc
  has no soft-float helpers (`__addsf3`...), so they are stubs too until `softfloat_ps2` (T23).
- The host C library still provides the integer-only functions the game calls (`memcpy`, `memset`, `strlen`,
  `strcmp`, `strcat`, `sprintf`, `printf`, `abs`, `exit`) and, for now, `rand` / `srand`. The game's random numbers
  must come from the PS2's newlib `rand` (simulation state, `docs/known_bugs.md`): T23.

### Memory map

The game keeps addresses in 4-byte fields, writes hardware registers by number (`0x10000000`, `0x12000000`), keeps
fixed buffers in EE RAM outside its program (`D_100000`, `D_168A30`, the rings at `D_200000` / `D_300000`), and
makes uncached views of its own variables (`(addr & 0x0FFFFFFF) | 0x20000000` in `file.c` and `movie.c`,
`D_004362A0 | 0x30000000` in `gfx.c`). So the 32-bit build gives it the PS2's address space at the PS2's addresses,
set up by a constructor in `port/src/plat_mem.c` before anything of the game runs:

| PS2 address | Size | Here |
|---|---|---|
| `0x00010000`-`0x02000000` | 32 MB less 64 KB | EE RAM: one shared memory object (`memfd`) mapped over the range the loader filled with `.ttt_data` / `.ttt_bss`, after copying the data into it. Holds the game's data and bss at their addresses, the fixed buffers below the program, and the PS2 stack area above the bss. |
| `0x20000000`, `0x30000000` | 32 MB each | the uncached and uncached-accelerated views: the same memory object mapped again, so a write through any view is seen through the others. |
| `0x10000000` | 64 KB | EE hardware registers (timers, IPU, GIF, VIF, DMA, INTC, SIF): plain memory. |
| `0x11000000` | 64 KB | VU0 / VU1 micro and data memory: plain memory. |
| `0x12000000` | 8 KB | GS privileged registers: plain memory. |
| `0x70000000` | 16 KB | scratchpad. |

- The registers read back what was last written (0 at start). Behaviour behind them (timer counts, DMA completion,
  the GS CSR) belongs to T24 / T28; until then a loop on a register spins forever (`Dma_Wait` in `Gfx_Init`, Timer 0
  in `Frame_PreUpdate`).
- Not identity-mapped: the first 64 KB of EE RAM (Linux refuses addresses below `vm.mmap_min_addr`, 0x10000; they
  are reachable at `0x20000000 + addr`) and KSEG0 / KSEG1 (`0x80000000` / `0xA0000000`), which the game does not
  use. The program's own code, the C library, the host heap and the host stack stay above `0x08000000`; the game's
  functions run on the host stack (as in the reference port's 32-bit build), so a stack address the game stores is a
  host one.
- The mapping refuses to replace anything but the game image in EE RAM (it reads `/proc/self/maps` first) and
  refuses to map over anything at the other addresses (`MAP_FIXED_NOREPLACE`): a clash stops the program with
  `ttt: cannot map ...` (exit code 2).
- `TTT_SELFTEST=mem build/port/ttt` checks the map on the running program (views, data copied, bss zero,
  scratchpad, registers, the helpers below) and exits; `tests/test_port_mem.py` runs it and checks with `nm` that
  every chunk label and every `D_XXXXXXXX` data label of the program sits at its PS2 address.

### Addresses

`port/src/port_addr.h` has `Port_Ptr` / `Port_Addr`, plain casts in the 32-bit build (a game address is a host
address), and helpers for the port's own code: `Port_EePhys` (the physical EE RAM address of any view),
`Port_IsEeRam`, and `Port_DmaPtr` (what a DMA address designates: the scratchpad when bit 31 is set, else EE RAM).
The game's own code needs none of them: it uses its addresses as they are.

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
7. **The ELF headers land in EE RAM.** With the game's data as the lowest segment, `ld` puts the program's headers
   in front of it (`PHDR` at 0x00415034, inside the PS2's code range). The C library may still read them, so
   `plat_mem.c` copies EE RAM from the start of that page, not from the first data byte.
8. **Placing data at its address needs exact sizes.** Each data file is placed by its first item's PS2 address;
   it keeps the PS2 layout only because every item keeps its size and the file's `.align` lines are no stricter
   than its address. The `nm` check of `tests/test_port_mem.py` catches a file that drifts.

## Next

T23 EE float model, `softfloat_ps2`, newlib libm and `rand`; T24 kernel / SIF / CD replacements and VBlank (and the
timer registers); the DMA / GS path (T28); then the placeholders above by hand.
