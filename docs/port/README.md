# PC port: build pipeline and notes

The PC port is built from the same C as the matching decompilation. Nothing of the game is committed: the build
takes the game's data from the split of **your own** `SLUS_200.01` (the one the matching build already uses) and
writes everything under `build/port/`, which git ignores.

## Status (T29, 2026-10-23)

- Every game C file the matching build links (59) compiles for 32-bit x86 Linux with the host gcc, with the
  NON_MATCHING drafts switched on: every game function comes from C, except the hand-written EE / VU0 routines
  listed under [Hand replacements](#hand-replacements).
- The program links into `build/port/ttt` (32-bit ELF) and runs on the PS2's memory map ([Memory map](#memory-map))
  with the platform layer of [Platform layer](#platform-layer): kernel, SIF / IOP, the disc, DMA, timers and
  VBlank. Pointed at your disc (`TTT_DISC`, default `iso/`), it boots through `Sys_Init`, enters the main loop
  with one update per VBlank and runs the boot mode, the title and the main menu:
  `TTT_FRAMES=600` reports `modes: 0@1 2@168 1@172 3@177` (mode@VBlank: boot, title, transition, main menu) and
  590 frames. Left alone, the menu returns to the title after about 480 frames and enters the attract cycle's
  loading mode (11) at VBlank 699 and runs the attract demo (mode 11, sub-state 2) through the VU0 blend
  (`func_0034A180`) and the VU0 skinning programs: `TTT_FRAMES=3000 TTT_SEED=1` ends at VBlank 3000 (2606 frames)
  with no stop, and two runs with the same seed end in the same state (`sbss-hash 0x82bb5654` in T29 batch 2, where the fighter packets are sent with their real size; the joint
  matrices of batch 3 change it again: `TTT_FRAMES=2551 TTT_SEED=1` ends with `0x27e2f897`; batch 4, with the libvu0 / libpkt / VU0 rotation fixes, with
  `0x577e0d2f`).
- The GS path draws ([GS path](#gs-path)): the display lists go DMA -> VIF1 -> GIF -> GS memory and the software
  reference rasteriser; `TTT_DUMP=<dir>` writes the displayed picture as PNG. The title and the main menu come out
  complete (logo, menu text, highlight bar). Since T29 the game's VU1 programs run ([VU1](#vu1)): the attract path's
  loading scene (mode 11) shows the 3D stage, textured and filtered, with its sky, smoke and lightning effects, and
  from the camera cut (script frame 488, about VBlank 1571) the two fighters of the demo fight. T29 batch 2 fixed the
  picture that froze on the lightning flash there (the fighter packets' size is read from a DMA tag that toSPR copies
  into the scratchpad, gotcha 15), the title's header text (the same cause), and the blocky stage textures (bilinear
  filtering, and the precision of S / Q for far vertices: [GS path](#gs-path)). T29 batch 3 fixed the fighters' heads
  and hair, which came out as long spikes (gotcha 16: `sceVu0Normalize` kept a w the library clears), and the dump
  merges both read circuits as PMODE asks.
- With the real libgraph set-up (T28) the game reads the display position DX 636 / DY 50 back into `options.c`'s
  `D_005F6DB0` / `D_005F6DB4` (zeros with the old placeholders), and new port code moves host addresses the game
  keeps in `.sbss`: the state hash of a given `TTT_FRAMES` / `TTT_SEED` differs from T23's. The GS itself writes no
  game memory: the same run with the VIF1 and GIF channels cut off from it gives the same hash.
- Placeholders still reached on that path (listed at the end of a `TTT_FRAMES` run): `scePadGetState` (no pad),
  libmc (no card), the boot movie (skipped).
- Floats are the EE's: every float operation of the game and of `port/src` goes through the EE float model
  (`port/src/softfloat_ps2.c`), the maths library is the PS2's newlib (`third_party/newlib_libm`), `rand` / `srand`
  are newlib's on the game's own state word. No host float code is linked ([EE float model](#ee-float-model)).
- Last clean build: 76 objects (59 game, 17 port), 1270 data objects, 22 names placed by address, 43 stubs (none
  reached on the path above).
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
- `port/src/plat_stub.c` has the same helper (`Port_Missing`) for the port's own placeholders; it also prints where
  the run is (VBlank, frame, modes). Library functions move out of the generated list as they get real
  implementations. `Port_Hit(name)` records a placeholder that answers without doing the real work (and the
  generated stubs under `TTT_STUB_RETURN`); a `TTT_FRAMES` run lists them at its end.
- No host libm is linked. Under `-msoft-float` the game expects float results in `eax`; the host's `sinf` /
  `cosf` / `sqrtf` return them on the x87 stack, so a link with `-lm` would give wrong values without any error.
  The float helpers and the maths library are the port's own ([EE float model](#ee-float-model)).
- The host C library still provides the integer-only functions the game calls (`memcpy`, `memset`, `strlen`,
  `strcmp`, `strcat`, `sprintf`, `printf`, `abs`, `exit`). `rand` / `srand` are newlib's (`port/src/plat_libc.c`).

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

- The registers read back what was last written (0 at start). The platform layer changes them only when the game
  polls ([Platform layer](#platform-layer)): timer counts, DMA completion, GS CSR, D_ENABLER.
- Not identity-mapped: the first 64 KB of EE RAM (Linux refuses addresses below `vm.mmap_min_addr`, 0x10000; they
  are reachable at `0x20000000 + addr`) and KSEG0 / KSEG1 (`0x80000000` / `0xA0000000`), which the game does not
  use. The program's own code, the C library and the host heap stay above `0x08000000`. The game runs on a stack in EE
  RAM, below 0x02000000 as on the PS2 (`port/src/plat_main.c`, the link wraps `main`): the game hands addresses
  of its locals to the DMA, whose address form (`addr + 0x10000000`, masked) only works below 0x10000000.
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

## Platform layer

The Sony libraries and the hardware the game talks to, in `port/src/`. Nothing runs beside the game: the game's own
wait loops drive the hardware.

| File | What |
|---|---|
| `plat_hw.c` | Bus-cycle clock, timers 0-3, VBlank (59.94 Hz, GS CSR FIELD / VSINT), INTC / DMAC handlers (`AddIntcHandler`, `AddDmacHandler`, `EnableIntc`...), DMA channels, MFIFO, `TTT_FRAMES` report, `TTT_SELFTEST=hw` |
| `plat_sys.c` | Kernel (cache, threads, semaphores), SIF (IOP reboot, module loads, commands, `sceSifBindRpc` / `sceSifCallRpc` and the table of IOP servers), CD init, `TTT_SELFTEST=rpc` |
| `plat_file.c` | The disc (`TTT_DISC`: a directory with `TEKKEN.BIN` or the `.iso`) and `sceSdRemote`: RSPU2DRV's file reads (0x204E / 0x2045), status (0x2030), harmless replies to the sound and movie commands |
| `plat_lib.c` | libgraph (`sceGsSyncV` waits for the next VBlank; `sceGsResetGraph`, `sceGsSetDefDBuff`, `sceGsSetDefDispEnv`, `sceGsPutDispEnv`, `sceGsSwapDBuff` build the environments and write the GS), libpad / libmtap (no pad, no multitap), libmc (no card), movie skip, a cache call |
| `gs_core.c` | GS: GIF paths (PACKED / REGLIST / IMAGE), registers, 4 MB GS memory with its layout, CLUT buffer, the software reference rasteriser, the frame dump, `TTT_SELFTEST=gs` |
| `gs_vif1.c` | VIF1: UNPACK into VU1 memory, MPG, DIRECT / DIRECTHL to the GIF, MSCAL / MSCALF / MSCNT run the VU1 program |
| `gs_vu1.c` | VU1: registers, the program runs, XGKICK on PATH1, `TTT_SELFTEST=vu1` |
| `vu_micro.c` | The micro-mode interpreter shared by VU0 and VU1 |
| `plat_pkt.c` | libpkt: VIF1 and GIF packet builders |
| `plat_vu0.c` | libvu0 in C (column-major matrices; `func_0034A868` is `sceVu0Normalize`) |
| `plat_main.c` | Runs the game's `main` on the EE stack |
| `softfloat_ps2.c` | The EE float model: `__addsf3`... for the FPU, `Port_Vu*` for the vector unit |
| `plat_libc.c` | newlib's `rand` / `srand` (the executable's 0x40FBD0 / 0x40FBC0) on `_impure_ptr->_rand_next` |
| `plat_floattest.c` | `TTT_DUMP=<dir>` | Write the displayed picture as `<dir>/frame_<vblank>.png` (8-bit RGB) every `TTT_DUMP_EVERY` VBlanks. Put it under `build/` (ignored by git): the pictures are the game's. |
| `TTT_DUMP_EVERY=<n>` | The dump's period in VBlanks (default 60). |
| `TTT_SELFTEST=gs` | GS memory layout of every format, the upload paths, a GIF PACKED packet on the GIF channel (flat, CLUT-textured, blended, depth-tested, scissored sprites, a triangle), the read circuits' merge (ALP / circuit 1 alpha, SLBG, DISPLAY position), VIF1 UNPACK modes / STCYCL / masks / double buffer / MPG, DIRECT and FINISH (`tests/test_port_gs.py`). |
| `TTT_SELFTEST=float` |
| `vu0_micro.c` | VIF0 (MPG uploads) and the VU0 program start (vcallms) on `vu_micro.c` |

- **Polling.** `EE_HW_POLL()` (`include/ee_asm.h`, nothing in the matching build) sits in every game loop that waits
  on the hardware: `Dma_Wait` and the other CHCR loops of `dma_gif.c` / `dma_spr.c`, the D_ENABLER loops, Timer 0 in
  `Frame_PreUpdate`, `Gfx_WaitVSync`. Each poll advances the clock and brings the hardware up to it. A new hang
  in a register loop means a missing hook.
- **Clock.** Headless (`TTT_HEADLESS=1`, implied by `TTT_FRAMES`): one poll is one Timer 0 tick (256 bus cycles),
  nothing sleeps, a run repeats exactly (polls per frame, `gRngSpin`, the boot seed). Otherwise the clock is the
  host's monotonic time: VBlanks at real 59.94 Hz, seed and spin generator vary per run as on the console; a poll
  more than 1 ms from the next VBlank sleeps 0.2 ms.
- **Interrupts.** VBlank start raises INTC cause 2 and a finished DMA its DMAC channel; handlers run inside the poll
  if interrupts are on (`Port_Di` / `Port_Ei`), else at the next `ei`, with interrupts off while they run.
- **DMA.** A channel with STR set is carried out at the next poll: normal, source chain (refe / cnt / next / ref /
  refs / call / ret / end) and the scratchpad's destination chain. toSPR / fromSPR copy the data; VIF0 feeds
  `vu0_micro.c`, VIF1 and GIF feed the GS path (with TTE the VIF1 tag's upper half first; toSPR with TTE copies
  each whole tag quadword into the scratchpad in front of its data); IPU / SIF only walk their lists. A block that follows its tag inside the MFIFO ring is read across the ring's end. In MFIFO mode (D_CTRL.MFD, `dma_spr.c` streams the display
  lists through it) fromSPR writes into the ring and the drain channel stays busy while the ring is empty.
- **IOP.** Reboot and module loads succeed. The game's remote calls all go through `sceSdRemote` (RSPU2DRV); the
  server table of `plat_sys.c` also answers padman, mcserv and mtapman with zeros. A 0x204E read copies the
  archive's sectors (size rounded up to 16 bytes [I]) into EE memory at once and shows status bit 0 for one poll;
  0x2045 reads go into a 2 MB SPU2 RAM image. 0x2058 (movie start) is refused, 0x205A(1) reports file mode.
- **Movies.** `func_0039C878` asks `Port_MoviePlayable()` (0 for now) and skips the movie as when the stream engine
  never goes idle.

Switches:

| Variable | Effect |
|---|---|
| `TTT_DISC=<dir or .iso>` | The disc: a directory with `TEKKEN.BIN` (as `scripts/setup_disc.py` copies it to `iso/`) or the ISO 9660 image. Default: `iso/` of the repository (next to `build/port/ttt`). Read only. |
| `TTT_FRAMES=<n>` | Stop after n VBlanks with a report: VBlanks and frames, `modes:` (mode@VBlank of each change), last mode and sub-state, `state:` (seed, `gRngSpin`, a hash of the first 4 KB of `.sbss`, cycles), placeholders hit. Exit code 0. Implies headless. |
| `TTT_HEADLESS=0/1` | Force the real-time or the headless clock. |
| `TTT_SEED=<n>` | Pin the boot seed `Rng_Init` takes from Timer 0 (`src/rng_misc.c`, `docs/known_bugs.md` section 8). With the headless clock the seed is already repeatable; this sets its value, and makes real-time runs start alike. |
| `TTT_SELFTEST=hw / rpc / mem` | Check the DMA, timers, VBlank and interrupts / the SIF, RPC and disc reads / the memory map, and exit (`tests/test_port_platform.py`, `tests/test_port_mem.py`). |
| `TTT_DUMP=<dir>` | Write the displayed picture as `<dir>/frame_<vblank>.png` (8-bit RGB) every `TTT_DUMP_EVERY` VBlanks. Put it under `build/` (ignored by git): the pictures are the game's. |
| `TTT_DUMP_EVERY=<n>` | The dump's period in VBlanks (default 60). |
| `TTT_SELFTEST=gs` | GS memory layout of every format, the upload paths, a GIF PACKED packet on the GIF channel (flat, CLUT-textured, blended, depth-tested, scissored sprites, a triangle), VIF1 UNPACK modes / STCYCL / masks / double buffer / MPG, DIRECT and FINISH (`tests/test_port_gs.py`). |
| `TTT_SELFTEST=float` | The EE float model against PCSX2 data, EE rules, exact libm cases, newlib `rand`, the fields libvu0's helpers write (`tests/test_port_float.py`). `TTT_FLOATDATA` overrides the data directory (`port/tests/ps2float`). |
| `TTT_SELFTEST=vu1` | Hand-assembled VU1 programs sent through VIF1: pairing, delay slots, E bit, Q / P latency, flag delays, integer / EFU instructions, XTOP / XITOP, XGKICK (`tests/test_port_gs.py`). |
| `TTT_VU1_CALLS=1` | With `TTT_FRAMES`: the report also lists the VU1 runs per entry address. |
| `TTT_GS_TRACE=1` | Per VBlank: pixels written per frame buffer (FBP) with the last FRAME / TEX0 / TEST / ALPHA, each GIF path's tag count and tag state, DISPFB1 / 2, PMODE, DISPLAY1 / 2 and BGCOLOR, every transfer (BITBLTBUF / TRXPOS / TRXREG / TRXDIR), and the first GIF tags with reserved bits set (`bad tag`, with the DMA block they came in). |
| `TTT_GS_TRACE_PRIMS=<vblank>` | Every vertex kick drawn before that VBlank (type, context, coordinates, UV / STQ, colour, TEX0 / TEX1 / CLAMP / FRAME / TEST / ALPHA). Vertices that only queue (XYZ3 / ADC) print as `gs-adc`. With `TTT_GS_TRACE_PIXEL=<x>,<y>`: every write to that pixel then, with its source and result, and every rejected one (`gs-reject`: scissor, alpha, destination alpha or depth test). |
| `TTT_GS_TEXDUMP=<vblank>:<tex0 hex>:<x>:<y>:<w>:<h>` | With `TTT_DUMP`: writes that region of the texture TEX0 describes (through a CLUT loaded from it) as `tex_<vblank>.png`. |
| `TTT_STUB_RETURN=<n>` | Generated stubs return n instead of stopping. |

## GS path

`port/src/gs_core.c` and `gs_vif1.c`, fed by the DMA channels of `plat_hw.c` at the poll that carries a transfer out:

```
VIF1 channel (ch 1)  ->  VIF1 (UNPACK / MPG / MSCAL -> VU1 memory; DIRECT / DIRECTHL -> PATH2) --+
GIF channel (ch 2)   ->  PATH3 ---------------------------------------------------------------+-> GIF -> GS
VU1 XGKICK (gs_vu1.c) ->  PATH1 ---------------------------------------------------------------+
```

- **GIF.** Each path keeps its own tag (a packet may span transfers). PACKED (PRIM, RGBAQ with the Q of the last ST,
  ST, UV, XYZF2 / XYZ2 with the ADC bit, FOG, XYZF3 / XYZ3, A+D, NOP), REGLIST, IMAGE; the tag's PRE / PRIM.
- **GS memory.** 4 MB with the GS's layout: pages of 32 blocks, the per-format block order and columns of PSMCT32 / 24
  / 16 / 16S, PSMT8 / 4 / 8H / 4HL / 4HH and the Z formats (the colour layouts with the page's blocks XOR 24). The game
  draws its 2D frame into PSMCT24 FBP 0 (640 x 448, XYOFFSET 0x6C0 / 0x720, context 1 and 2 alike) and shows a 640 x
  224 copy at FBP 0x8C in interlaced frame mode (SMODE2 3, DISPLAY 2560 x 448).
- **Transfers.** Host -> GS (row-table fast paths for 32 / 16 / 16S / 8 / 4 bits, generic for the rest), local ->
  local at TRXDIR; GS -> host is counted, not carried out.
- **Registers.** PRIM / PRMODE / PRMODECONT, the two contexts (TEX0 / TEX1 / TEX2 / CLAMP / XYOFFSET / SCISSOR / ALPHA /
  TEST / FBA / FRAME / ZBUF), TEXA, TEXCLUT, FOGCOL, COLCLAMP, PABE, BITBLTBUF / TRXPOS / TRXREG / TRXDIR, SIGNAL /
  FINISH / LABEL (CSR and SIGLBLID in the privileged registers). The CLUT is the GS's 1 KB buffer, loaded on TEX0 /
  TEX2 for indexed formats by CLD (0-5), CSM1 (the 8-bit table's index bits 3 / 4 exchanged) or CSM2, CSA for 4-bit
  palettes. [I] CLD 4 / 5 also reload when GS memory was written since the last load.
- **Rasteriser.** Integer only (no host float, as the rest of the port): 12.4 coordinates, sprites with the GS's
  half-open edges, triangles (lists, strips, fans) by edge functions with the top-left rule and 24-bit barycentric
  fractions (Gouraud colour, fog, depth, UV, perspective STQ decoded from the float bits on a per-primitive scale
  that puts the largest of S / T / Q near 2^36: far vertices have Q about 2^-14, which a fixed 2^20 scale left with
  6 bits and blocky textures), lines, points. Per pixel:
  scissor, alpha test with its fail modes, destination alpha test, depth test and write (ZMSK), texture function
  (TFX / TCC) with CLAMP's wrap modes, fog, blending ((A - B) * C >> 7) + D with PABE and COLCLAMP, FBA, FBMSK.
  Texture filter per TEX1: LOD = K (LCM 1) or (log2(1/|Q|) << L) + K; LOD <= 0 takes MMAG, else MMIN; linear
  (MMAG 1, MMIN 1 / 4 / 5) weighs the four texels around (u - 0.5, v - 0.5) with the 4-bit fractions, after the
  CLUT; the game uses TEX1 0x61 (LCM, MMAG and MMIN linear) on the 3D stage. Level 0 only (no mip-maps: MXL is 0
  in the game's TEX1 values seen); no dithering or anti-aliasing.
- **Display and dump.** The libgraph set-up writes PMODE / SMODE2 / DISPFB2 / DISPLAY2 / BGCOLOR and the game writes
  DISPFB1 / DISPLAY1 itself (`Gfx_ShowBuffer`). At each VBlank start, before the handlers run, `TTT_DUMP` merges the
  two read circuits as the PCRTC does: each circuit is DISPFB's buffer from (DBX, DBY), (DW + 1) / (MAGH + 1) pixels by
  (DH + 1) / (MAGV + 1) lines placed at its DISPLAY (DX / (MAGH + 1), DY / (MAGV + 1)), interlaced frame mode doubling
  each buffer line; the background is circuit 2 (EN2 and SLBG 0) or BGCOLOR (SLBG 1, or where circuit 2 shows
  nothing), and circuit 1 (EN1) goes over it as c1 * a + bg * (255 - a), a = ALP (MMOD 1) or circuit 1's own alpha
  with 0x80 = 1.0 (MMOD 0), in integers rounded to nearest [I: the PCRTC's rounding; a PSMCT24 buffer's alpha is taken
  as 0x80]. The picture has the size and position of the first enabled circuit. The game sets PMODE 0x7F23 (both
  circuits, MMOD 1, ALP 0x7F) with the same DISPFB / DISPLAY on both, so its picture is circuit 1's exactly. The PNG
  writer is the port's own (stored deflate blocks, no library).
- **VU1.** See [VU1](#vu1).

## VU1

`port/src/gs_vu1.c` on the micro-mode interpreter `port/src/vu_micro.c` (shared with VU0, `vu0_micro.c`). The game's
original VU1 programs (`src/vu1/*.vsm`, uploaded by the boot packet) run unchanged:

- **When.** VIF1 MSCAL / MSCALF start the program at the given instruction, MSCNT continues where the last one stopped
  (the instruction after the E bit's delay slot: the programs put `b entry` there). TOP / ITOP are the values the VIF
  latched at that code (BASE / OFFSET double buffer). The program runs to its end at once: the VIF waits for VU1 before
  it starts the next one, so the order of UNPACKs, DIRECTs and kicks is kept.
- **Interpreter.** Upper and lower halves per instruction; the upper result is committed after the lower half read
  its operands (`add vf1 | sq vf1` stores the old vf1); branches and the E bit take effect after their delay slot;
  the I bit loads I. Q latency (div / sqrt 7, rsqrt 13 instructions, WAITQ), P latency (the EFU: erleng 24, eatan 54...,
  WAITP, MFP), the MAC and clipping flags four instructions late (FMAND, FCAND, FCGET...). One step is one
  instruction; stalls are not modelled (the units interlock on the vector registers, and the programs are
  hand-scheduled). Not modelled: the status flag and the MAC flag's overflow / underflow bits (no program reads them).
  VU1 finishes Q / P / flags still in flight at the end of a program; VU0 keeps them in flight (as before T29).
- **Arithmetic.** The EE float model's vector unit (`softfloat_ps2_inl.h`, no host float): add / sub / mul cut toward
  zero, div / sqrt cut. ERLENG = 1 / sqrt(x^2 + y^2 + z^2) in that order; EATANxy / xz / EATAN use the VU manual's
  series (pi/4 + 8 odd terms of t = (y - x) / (y + x), the constants PCSX2 uses) [I: the EFU's internal order].
- **Instructions.** Every one the game's VU0 and VU1 programs use (listing survey: the upper set of `mesh.vsm`...
  `sprites.vsm`, lower `xgkick`, `xtop`, `eatanxy` / `eatanxz`, `erleng`, `mfp`, `waitp`, ...) plus XITOP, ESADD,
  ERSADD, ELENG, ESUM, ESQRT, ERSQRT, ERCPR, EATAN. Anything else is counted and reported in the `TTT_FRAMES` line
  `ttt: vu1: ... N not implemented` (0 on the attract path).
- **XGKICK.** The GIF packet at the given quadword of VU1 memory, tag by tag up to EOP (read across the end of
  memory), goes to `Gs_Gif(1, ...)` at once.
- **Cost.** About 1.35 million program runs and 2.8 billion VU instructions in 3000 VBlanks of the attract path
  (`mesh.vsm`, entry 0x050, is almost all of it); a `TTT_FRAMES=3000` run took about 4.5 minutes instead of 1, and
  about 8.5 since the bilinear filter (four texel reads per textured pixel).
- The reference project's interpreter computes with the host's float unit, so its structure and decoding were
  reused, not its code.

## Hand replacements

Routines written in EE / VU0 assembly that the PC build cannot compile. They are guarded with `#ifndef PORT` in
`src/` (matched C made of inline assembly) or left off by `portsrc.REPLACED` (drafts around that assembly), and
`port/src/vu0_pc.c` supplies their names: two in C, the rest as placeholders that report themselves.

| Function | File | What it is | PC version |
|---|---|---|---|
| `Mtx_Unit`, `Mtx_Zero` | `theater_c.c` | identity / zero 4x4 matrix (`sq $0`) | C |
| `func_003435C0`, `func_003436B8`, `func_003437B8`, `func_003438B8` | `dma_gif.c` | VU0 macro-mode skinning loops (`vcallms`, jumps into their own bodies) | C loop; the VU0 programs (`src/vu0/skin_*.vsm`) run on `vu0_micro.c` |
| `func_003E8FA0` | `tekken_bowl.c` | VU0 sin / cos (result in vf8) | C on the VU0 registers |
| `func_003E9038`, `func_003E9120`, `func_003E91D0`, `func_003E92C0` | `tekken_bowl.c` | VU0 matrix rotations around it (results left in vf4-vf7) | C on the VU0 registers |
| `func_003EA978`, `func_003EAF78`, `func_003EBC98`, `func_003EC388`, `func_003ECAD8`, `func_003ED338`, `func_003EEF00` | `tekken_bowl.c` | m2c drafts around that VU0 code (register names the C does not hold, raw `sqrt.s` words) | placeholder |
| `RotMtxXYZ` (inline helper of a draft) | `model_anim_b.c` | VU0 rotation x / y / z through `func_003E8FA0` | C `Port_Vu0RotMtxXYZ` |
| `func_00349D70`, `func_00349EE0`, `func_0034A050`, `func_0034A0D0` | `geometry_b.c` | VU0 rotation builders on the sin / cos kernel `func_00349CF8` (rows 0-2 only) | C on the VU0 registers, instruction by instruction (gotcha 18) |
| `func_0034A180` | `geometry_b.c` | entry one instruction before `func_0034A184` (its first instruction is the delay slot of `func_0034A148`'s return): d = a * k + b * (1 - k) | C; four arguments (gotcha 5) |

Smaller hooks (all `#ifdef PORT` / `#ifndef PORT`, matching objects unchanged):

- `dma_spr.c` `func_0035D588` / `func_0035D608`: the 32-byte `ld` / `sd` copy of the scratchpad header is four
  `u64` copies.
- `dma_gif.c` draft `func_00343340`: its `lqc2` into vf8-vf11 is `Port_Vu0LoadVf` (the VU0 register file
  `gPortVu0Vf`, `plat_ee.c`).
- `modes.c`, `select_results.c`, `theater.c`, `effects_draw.c`: the `.set fptoui, __fixunssfsi` / `.set sqrtf, sqrt`
  aliases exist for the PS2 compiler's helper names and stay out of the host build.
- `battle_flow.c`: a prototype of the variadic `Text_Printf` before its first caller.
- `select_results_b.c` draft `Text_Printf`: reads its variadic arguments as 4-byte i386 stack slots (`TP_ARG`),
  not from the EABI register save area.
- `dma_gif.c` `func_00345780`: takes the update callback its callers pass and hands it to `func_003464F8` (on the
  EE it is still in `$a0`).
- `rng_misc.c` `Rng_Init`: the seed goes through `Port_BootSeed` (Timer 0 brought up to date, or `TTT_SEED`).
- `movie.c` draft `func_0039C878`: `Port_MoviePlayable()` before the movie.
- `PORT_RET(t)` on 21 functions returning `s8` / `u8` / `s16` / `u16` that some callers declare `s32`.
- `EE_HW_POLL()` in the hardware wait loops (above).

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
   a fourth (`$a2`, the blend partner b of d = a * k + b * (1 - k)). The asm shows its value: the callers in
   `func_003DD918` / `func_003DE0D8` load `$a2` with the destination (or, in two cases, the previous frame's matrix
   at +0xB0A0 / +0xB0E0), and `func_003DD8E8` passes its own `$a2` through. Under `PORT` both functions take it as a
   named argument. Likewise `render_fighter_b.c` `func_00341B70` calls `func_003421D8(a)`, which reads `$a1`: still
   `o` there.
6. **Soft-float ABI.** Float results travel in `eax` under `-msoft-float` and on the x87 stack in host libraries:
   no host libm, and port code that the game calls is built with the game's float flags.
7. **The ELF headers land in EE RAM.** With the game's data as the lowest segment, `ld` puts the program's headers
   in front of it (`PHDR` at 0x00415034, inside the PS2's code range). The C library may still read them, so
   `plat_mem.c` copies EE RAM from the start of that page, not from the first data byte.
8. **Placing data at its address needs exact sizes.** Each data file is placed by its first item's PS2 address;
   it keeps the PS2 layout only because every item keeps its size and the file's `.align` lines are no stricter
   than its address. The `nm` check of `tests/test_port_mem.py` catches a file that drifts.

9. **Narrow return values.** On the EE a function returning `s16` hands back the value sign-extended in `$v0`, so a
   caller with an `s32` prototype gets -1; i386 gcc leaves the upper half of `eax` as it is (`func_003B0078`
   returned 0xFFFF and indexed `gFighters[0xFFFF]`). `PORT_RET(t)` makes those definitions return `int` in the
   port. 31 mismatched declarations of 21 functions were found by a scan of `src/`; new ones need the same.
10. **Leftover argument registers, again.** `func_00345780()` is defined without parameters and calls
    `func_003464F8()` without arguments: on the EE its caller's argument is still in `$a0` and becomes the new
    object's update callback.
11. **EABI varargs in drafts.** A draft that walks the EABI register save area (`__builtin_next_arg(fmt) - 7 * 8`,
    8-byte slots) reads garbage on i386. The m2c draft `func_003DFA00` (demo.c) has the same shape and is not on the
    path yet.
12. **DMA from the stack.** The game sends quadwords from its stack to the scratchpad; the DMA address form only
    works for addresses below 0x10000000, hence the EE stack.
13. **Waits without a poll.** `func_00343178` waits for room in the MFIFO ring by calling `func_0035E1C8` (di / ei
    only): on the PS2 the drain runs meanwhile; here it needs an `EE_HW_POLL()` in that loop, or the demo hangs as
    soon as the ring fills.

14. **`long` constants in 64-bit fields.** Drafts written for the EE, where `long` is 64-bit, build GIF tags with
    `0x8000UL << 44`; on i386 the shift is taken modulo 32 and the IMAGE tag of every stage texture upload
    (`fighter_setup.c` `func_003623B8`) became a PACKED one, so the stage was drawn without its textures. Those drafts
    (`fighter_setup.c`, `fighter_setup_b.c` `func_003665E0`, `theater_d.c`, and since T29 batch 2 the m2c draft
    `func_003DFF30` of `demo.c`, next to `func_003DFA00`) use `ULL` / 64-bit casts now. A compile of every game and
    port source with `-Wshift-count-overflow` reports none; it does not see `(0x4343 << 16 | 0x4343) << 16`, an
    `int` that overflows before it is widened (two more in that draft, fixed alike).
15. **DMA tags the game reads back.** `func_00357640` copies a fighter's packet into the scratchpad with a toSPR
    chain with TTE (`func_003430D8`, CHCR 0x145) and sends `pkt[0]` quadwords from `pkt + 16`: the QWC of the tag
    that TTE put there. Without the tag copy it read whatever the scratchpad held (15038 quadwords from VBlank 1570
    on), the GIF channel took a vertex quadword for a GIF tag and its IMAGE NLOOP swallowed every later PATH3 packet,
    the display copy into FBP 0x8C among them: the frozen picture. `TTT_GS_TRACE=1` shows such a path (`loops`
    never back to 0, `bad tag`).

16. **Library fields the C did not write.** libvu0's `sceVu0Normalize` (0x34A868) clears the result register
    (`vsub.xyzw vf6, vf0, vf0`) and writes only xyz, so w is 0; the port's C kept the input's w. The game's
    `func_00348988` / `func_00348C68` build matrices on the stack whose w column they never set and rely on that
    normalize (and `sceVu0OuterProduct`, `vsub.w`) to clear it. With the stack garbage kept, the local matrices of the
    secondary joints `func_00347E28` builds (joints 30, 32, 33, 57 and 58 in the demo fight) had w terms up to 7.7e8, and
    parent world * local mixed the parent's translation into the rotation rows: the skinned head and hair vertices
    shot off as spikes while the VU0 skinning itself was exact. Found by logging every skinned vertex with its bone
    blocks (the interpreter's results, guard-band ADC bits included, agreed with a recomputation), then the joint
    whose world matrix was not a rotation, then a gdb watchpoint on its local matrix. `sceVu0InversMatrix` keeps
    row 3's w the same way. `TTT_SELFTEST=float` checks these fields against the executable's code.

17. **libpkt adds its counts.** The SDK's packet builders (`plat_pkt.c`, checked against asm/B4320.s in T29 batch
    4) ADD to the counts already in a tag: `sceVif1PkTerminate` adds (end - tag) / 16 - 1 to the DMA tag's QWC,
    `Close*Code` the whole quadwords to the VIF code, `sceVif1PkCloseGifTag` ceil(n / NREG) to NLOOP.
    `Gfx_PkAddImage` (font_tex.c) presets NLOOP in an IMAGE tag and closes it with nothing after it; the old C
    replaced NLOOP by 0. The packet struct keeps pGifTag at 0x14 (the C had 0x10), Init / Reset write only
    pCurrent, pBase and pDmaTag, `Size` rounds down, any non-zero irq sets bit 31. `TTT_SELFTEST=hw` checks them.
18. **Hand-written VU0 routines that keep a row.** `geometry_b.c`'s rotation builders `func_00349D70` /
    `func_00349EE0` / `func_0034A050` / `func_0034A0D0` store rows 0-2 of the matrix only (`func_00349D70` also
    m[3][3] = 1.0). `func_00347760` builds the local matrices of joints 7-0x1C with `func_00349D70` over matrices
    whose row 3 already holds the bone offsets; their drafts started from `Mtx_Unit` and cleared the offsets, so the
    head and its children collapsed onto the neck: the missing heads of the demo fight (VBlank 1950: no fighter
    vertex, drawn or ADC, reached the GS above the neck; `TTT_GS_TRACE_PRIMS` now prints ADC vertices as `gs-adc`
    and `TTT_GS_TRACE_PIXEL` the reason a pixel was rejected, `gs-reject`). The four are now C on the VU0 registers
    in `vu0_pc.c` (`portsrc.REPLACED`), with the libvu0 sin / cos series (coefficients `D_0046B040`); the drafts keep
    row 3 too. libvu0 itself (`plat_vu0.c`) was checked the same way: `sceVu0Rot*` use the library's series (0x34AAF0,
    coefficients 0x4F5700) instead of libm, `sceVu0ViewScreenMatrix` and the angle fold use the FPU's add.s / div.s
    (`Port_Fpu*`), `sceVu0TransMatrix` reads tv before storing, `sceVu0ClipAll` tests the sticky sign / zero flags
    (it was a placeholder returning 0). `TTT_SELFTEST=float` checks them with values from the executable's code.

## EE float model

- **Arithmetic** (`port/src/softfloat_ps2_inl.h`, `softfloat_ps2.c`; the hardware-level model of the reference
  project): no denormals (exponent 0 is zero), no infinity / NaN (exponent 255 is an ordinary number, results clamp
  to +/-0x7F7FFFFF), add / sub / mul cut toward zero, the FPU's add.s keeps one guard bit of the smaller operand,
  div.s and sqrt.s round to nearest, the vector unit's vdiv / vsqrt cut; x / 0 gives +/-max; comparisons see
  denormals as zero. `sqrtf` is `sqrt.s` (ee-gcc inlines it; its errno fallback never runs on the EE).
- **Vector unit**: `port/src/plat_vu0.c` and `vu0_pc.c` are VU0 macro code on the PS2; the build renames their
  `__addsf3` / `__subsf3` / `__mulsf3` / `__divsf3` / `sqrtf` calls to `Port_VuAdd`... (`toolchain.VU_FILES`,
  objcopy), so C written with floats gets the vector unit's results. `vu_micro.c` runs the VU0 and VU1 microcode on
  the same arithmetic. No host floating point anywhere (`tests/test_port_float.py` checks the program's code for x87 /
  SSE float instructions).
- **Constants**: no `eeconst` pass. The reference needed one because BT3's compiler truncated decimal constants;
  TTT's (ee-gcc 2.9-991111 with its assembler) rounds them to nearest like the host compiler: the executable holds
  0.1f as 0x3DCCCCCD, its newlib `__kernel_sinf` S1 as 0xBE2AAAAB (BT3: 0xBE2AAAAA), `(float)0.333...` in
  `__ieee754_logf` as 0x3EAAAAAB, and ee-gcc 2.9 folds 1.0f / 30.0f to 0x3D088889.
- **libm** (`third_party/newlib_libm`, newlib 1.10.0's float fdlibm, built in software float through
  `newlib_shim.h`): the sources the reference copied, plus `ef_log.c` and `ef_fmod.c` written after the executable's
  `__ieee754_logf` / `__ieee754_fmodf` (constants as bit patterns), and `wrappers.c`: the executable's wrappers are
  built without `_IEEE_LIBM` (they read `__fdlib_version` = 1, `_XOPEN_`, call matherr and set errno, both in the
  game's memory); their error results are reproduced on bit patterns (fp-bit's double conversions included).
- **Test** (`TTT_SELFTEST=float`, `port/tests/ps2float`): `pcsx2_v2.7.303.txt` is the reference project's PCSX2
  2.7.303 output of `test.c` (1500 inputs x 16 FPU / VU operation classes; hardware-level, the same chip): all
  agree. 38 EE-rule cases, 29 libm cases fixed by the algorithm or the wrappers' control flow, newlib `rand`.
  PENDING (needs PCSX2 and a BIOS): the general libm results of TTT's own library (`libm_test.c` packed with your
  executable by `pack_libm.py`; its output saved as `pcsx2_libm.txt` is compared automatically), and the wrappers'
  NaN result (0x7FB00000, from gcc 2.96 fp-bit's rules).

## Next

The fighters' heads are drawn since T29 batch 4 (gotcha 18), but the head and hair still show spikes (VBlank 1950: a
spike above the head, hair strands pulled off): the remaining drafts on the skeleton path (`func_00347E28` secondary
joints, `func_00351E98` look-at, `func_0034D258` accessory springs, all m2c / typed drafts) are next, checked the
same way against their asm; `TTT_GS_TRACE_PRIMS` shows the spike vertices as `gs-adc` with z 0xFFFFAA. libgraph
(`plat_lib.c`): the DBuff buffer pairing and SwapDBuff's ids differ from the library with the same GS result. The
seven Tekken Bowl m2c drafts around VU0 code (`portsrc.REPLACED`, still placeholders: not reached by the attract
demo); the libm PCSX2 run; pads, memory cards, sound (RSPU2DRV) and movies.
