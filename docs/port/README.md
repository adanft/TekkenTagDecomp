# PC port: build pipeline and notes

The PC port is built from the same C as the matching decompilation. Nothing of the game is committed or linked in:
the build needs neither your `SLUS_200.01` nor its split, and the program takes the game's data from **your own
disc** when it runs (`ttt.dat`, [data.md](data.md)). Everything is written under `build/port/`, which git ignores.

## Status (T27, 2026-10-24)

- Every game C file the matching build links (59) compiles for 32-bit x86 Linux with the host gcc, with the
  NON_MATCHING drafts switched on: every game function comes from C, except the hand-written EE / VU0 routines
  listed under [Hand replacements](#hand-replacements).
- The program links into `build/port/ttt` (32-bit ELF) and runs on the PS2's memory map ([Memory map](#memory-map))
  with the platform layer of [Platform layer](#platform-layer): kernel, SIF / IOP, the disc, DMA, timers and
  VBlank. Pointed at your disc (`TTT_DISC`, default `iso/`), it boots through `Sys_Init`, enters the main loop
  with one update per VBlank and runs the boot mode, the title and the main menu:
  `TTT_FRAMES=600` reports `modes: 0@1 2@168 1@172 3@177` (mode@VBlank: boot, title, transition, main menu) and
  590 frames (6 VBlanks later since T35 with an empty memory card: `2@174 1@178 3@183`, see below). Left alone, the menu returns to the title after about 480 frames and enters the attract cycle's
  loading mode (11) at VBlank 699 and runs the attract demo (mode 11, sub-state 2) through the VU0 blend
  (`func_0034A180`) and the VU0 skinning programs: `TTT_FRAMES=3000 TTT_SEED=1` ends at VBlank 3000 (2606 frames)
  with no stop, and two runs with the same seed end in the same state (`sbss-hash 0x82bb5654` in T29 batch 2, where the fighter packets are sent with their real size; the joint
  matrices of batch 3 change it again: `TTT_FRAMES=2551 TTT_SEED=1` ends with `0x27e2f897`; batch 4, with the libvu0 / libpkt / VU0 rotation fixes, with
  `0x577e0d2f`; batch 5, with the bone pose of `func_003DD918`, with `0x8dd3eebb`).
- The GS path draws ([GS path](#gs-path)): the display lists go DMA -> VIF1 -> GIF -> GS memory and the software
  reference rasteriser; `TTT_DUMP=<dir>` writes the displayed picture as PNG. The title and the main menu come out
  complete (logo, menu text, highlight bar). Since T29 the game's VU1 programs run ([VU1](#vu1)): the attract path's
  loading scene (mode 11) shows the 3D stage, textured and filtered, with its sky, smoke and lightning effects, and
  from the camera cut (script frame 488, about VBlank 1571) the two fighters of the demo fight. T29 batch 2 fixed the
  picture that froze on the lightning flash there (the fighter packets' size is read from a DMA tag that toSPR copies
  into the scratchpad, gotcha 15), the title's header text (the same cause), and the blocky stage textures (bilinear
  filtering, and the precision of S / Q for far vertices: [GS path](#gs-path)). T29 batch 3 fixed the fighters' heads
  and hair, which came out as long spikes (gotcha 16: `sceVu0Normalize` kept a w the library clears), and the dump
  merges both read circuits as PMODE asks. T29 batch 4 brought the heads back (gotcha 18) and batch 5 removed their last
  spikes (gotcha 19: the m2c draft of `func_003DD918`, the per-frame pose of bones 7-0x1C, read a garbage
  translation), found with the [draft divergence check](#draft-divergence-check).
- With the real libgraph set-up (T28) the game reads the display position DX 636 / DY 50 back into `options.c`'s
  `D_005F6DB0` / `D_005F6DB4` (zeros with the old placeholders), and new port code moves host addresses the game
  keeps in `.sbss`: the state hash of a given `TTT_FRAMES` / `TTT_SEED` differs from T23's. The GS itself writes no
  game memory: the same run with the VIF1 and GIF channels cut off from it gives the same hash.
- Speed (T30 batch 1): the headless `TTT_FRAMES=2551 TTT_SEED=1 TTT_DUMP_EVERY=150` run takes 112 s instead of 386 s
  (6.6 -> 22.8 VBlanks/s; 11.7 with `TTT_GS_THREADS=0`), with the same 17 frames, report and `sbss-hash 0x8dd3eebb`
  ([Speed](#speed)). The title and menus run in real time; the demo fight does not yet (about 11 VBlanks/s).
- Speed (T30 batch 2): the same run in 60-65 s, the fight's VU1 part (VBlanks 1101-2551) at about 25 VBlanks/s
  instead of 12, with the same frames, report and hash in both thread modes: a predecoded VU interpreter (2.5 times
  the old one) and less waiting between the raster threads. The raster workers are now the limit ([Speed](#speed)).
- A window (T30 batch 1): real-time runs show the picture in an SDL3 window ([Window](#window)).
- GPU drawing (T30 batch 3): in a window the GS draws on the GPU (OpenGL 3.3, `port/src/gs_gl.c`,
  `TTT_RENDERER=gl`, the default when the window opens; `sw` keeps the software rasteriser) ([GL back end](#gl-back-end)),
  and VIF1, the VU1 programs and the GS front end run on a GS thread of their own (`port/src/gs_async.c`) with the same
  results ([Speed](#speed)). The scripted fight (`tests/test_port_input.py`, VBlanks 1101-1451) runs at about 40
  VBlanks/s with GL and 23 with the software rasteriser; its dumps differ from the software ones by 1-2 levels on
  average. 60 is not reached yet: the window thread's GL calls are the limit (Speed).
- GL fights at 60 (T30 batch 4): the scripted fight runs at 62-70 VBlanks/s with GL (software rasteriser: 22-24),
  with the same pictures as batch 3's GL back end (the dumps every 150 VBlanks are byte for byte the same). Fewer draw
  calls (an alpha test known for every pixel of a primitive needs no second pass, a repeated pass shares the depth
  write of the first), vertex and state buffers written unsynchronised instead of given new storage at every flush
  (the driver's page faults were a third of the window thread), texture hazards from the texels a primitive reads,
  CLUT loads from GPU-drawn pages done on the GPU (no read back), and VIF1 / VU1 and the GIF / GS front end on two
  threads with fewer wake-ups between the threads ([Speed](#speed), [GL back end](#gl-back-end)).
- libm (T30 batch 3, F03): `third_party/newlib_libm` gives the executable's asinf / atan2f bits (`TTT_SELFTEST=ee`:
  no difference left); the 2551-VBlank run keeps its frames and `sbss-hash 0xcf8bdf40` (those inputs never come up).
- Placeholders still reached on that path (listed at the end of a `TTT_FRAMES` run): `scePadGetState` (no pad), and
  with `TTT_MOVIES=0` the boot movie (skipped). libmc is real since T35 ([Memory cards](#memory-cards)).
- Movies and menu texts (T33 batch 1): the game's MPEG-2 movies play ([movies.md](movies.md)): the intro starts at
  VBlank 176 of the boot (2034 pictures, about 68 s, skippable with Start from the 11th picture), its audio on the BGM
  voices. `TTT_MOVIES=0` skips them as before; the tests run with it unless they ask for movies, and the 2551-VBlank
  run with `TTT_MOVIES=0` keeps its 17 frames and modes, its hash moving to `0x6f92650e` only in the two function
  pointers of gotcha 20 (word by word against T35's build). Without `TTT_MOVIES=0` that run is the intro from VBlank
  176 to its end. The memory card menu's message windows and the option menus' help line show their text again
  (gotchas 25, 26).
- Memory cards (T35): libmc over host files, the first slot (port 0) an empty formatted 8 MB card by default; the game's own menus
  save (OPTIONS > MEMORY CARD (PS2)) and the next boot loads the save. The boot's card check now sees an empty card
  instead of no card: one more request (GetDir) and the fixed busy polls make the boot 6 VBlanks longer, and every
  frame of a 1300-VBlank run is the old run's frame 6 VBlanks later (checked with `TTT_DUMP_EVERY=2`). So the
  2551-VBlank run (with an empty `TTT_SAVES`) ends with `sbss-hash 0xad2d0dea` and other frames at the dump points
  (modes `0@1 2@174 1@178 3@183 1@666 2@704 11@705`); with `TTT_MC0=none TTT_MC_POLLS=0` (no card, no busy poll)
  it has the old 17 frames and modes, and the hash differs from `0x4a16b393` only in the two moved function
  pointers of gotcha 20 (`0x5F6FD0` / `0x5F6FD4`: the new `plat_mc.o` moves the game code), checked word by word.
- Input (T34 batch 1): two DualShock 2 pads from the keyboard and SDL gamepads, latched once per VBlank, with
  recording and replay of the pad states ([Input](#input)). Real-time runs have the pads; headless runs have none
  unless asked, so the runs above keep their reports and hashes. With input the character select is reached
  (Start at the main menu); two drafts that stopped it there are fixed (gotcha 21). T34 batch 2: the pads play the
  game from the main menu to the fight. The character select's per-side step `func_003CA470` is written from the asm
  (gotcha 22), four calls that lacked an argument the callee reads now pass it (gotcha 23), and the
  [draft divergence check](#draft-divergence-check) finds every frame of the character select and of round 1 with
  inputs agreeing with the original code: ARCADE, two characters picked with the d-pad and a button, the stage loads,
  the fight (sub-state 8) runs by VBlank 830 and the fighters walk, punch and kick as the pad says
  ([Input](#input)). The CPU's decisions, the camera volume and the fighters' wind follow the original again
  (gotchas 23, 24): the wind of `Skel_Update` changes the attract demo's hair from VBlank 1650 on, so the 2551-VBlank
  run ends with `sbss-hash 0xcf8bdf40` (the wind words `D_005F6E18` / `D_005F6E20` of both sides, and the two moved
  function pointers of gotcha 20; with those six words set back the memory hashes to the old `0x341e8d2f`).
- Settings (T36, [settings.md](settings.md)): F1 opens a Dear ImGui overlay in the window (video, audio, key
  rebinding, game, about), kept in `~/.config/ttt/settings.ini` (`TTT_CONFIG`; environment variables still win);
  closing the window or Escape asks "Quit?" first. The overlay is the only host float code besides the VU
  interpreter's SSE, on the window thread, and nothing of the simulation reads it.
- Determinism (T27, [determinism.md](determinism.md)): the `ttt: state:` line's hash is now a 64-bit XXH3 of the
  whole gameplay state (EE RAM without the EE stack, scratchpad, hardware registers, VU0, the port's own state), with
  every host address in the game's memory counted as one constant, so that it does not move when the code changes
  size (F04). The 2551-VBlank run keeps its 17 frames and ends with `hash 0xd36c82e9a145875a`. The whole state
  (40 MB) can be saved at a VBlank boundary and loaded in the same or a fresh process (`TTT_SAVESTATE`,
  `TTT_LOADSTATE`, `Port_StateSave` / `Port_StateLoad`), and `TTT_SYNCTEST` runs every few VBlanks twice from a
  loaded state and compares. It found the fight's state depending on the GS threads' timing (stale stack words the
  game reads, left by the port's waits) and the C library's addresses on the EE stack; both fixed.
- Floats are the EE's: every float operation of the game and of `port/src` goes through the EE float model
  (`port/src/softfloat_ps2.c`), the maths library is the PS2's newlib (`third_party/newlib_libm`), `rand` / `srand`
  are newlib's on the game's own state word. No host float code is linked ([EE float model](#ee-float-model)).
- Last clean build: 76 objects (59 game, 17 port), 1270 data objects, 22 names placed by address, 43 stubs (none
  reached on the path above).
- `TTT_CC=clang` (game code through clang 23) is not there yet: 55 of 62 objects; the other 7 stop on an implicit
  declaration followed by a definition with another return type, which gcc accepts with `-fpermissive`.

## Build

```sh
python3 port/tools/build.py                   # -> build/port/ttt (no game file needed)
build/port/ttt                                # real time, in a window (needs the 32-bit SDL3, see Window)
TTT_FRAMES=600 build/port/ttt                 # headless, 600 VBlanks, then a report
```

`build.py` runs the two steps below in order and fails if any of them fails. Each can also be run on its own
from the repository root. Heavy runs go through `scripts/cool.sh`. The first run of the program checks your disc
and writes `build/port/ttt.dat` (about a second, [data.md](data.md)).

| Step | Tool | Writes |
|---|---|---|
| 1 | `port/tools/undefined.py [-v]` | `build/port/gen/src/*.c` (sources with the drafts on), `build/port/obj/*.o`, `build/port/undefined.txt` (what a link still needs, by kind), `build/port/objects.txt` |
| 2 | `port/tools/link.py` | `build/port/gen/image.ld`, `build/port/gen/symbols.ld`, `build/port/gen/data_table.c` (from `port/tools/data_layout.txt`), `build/port/gen/stubs.c`, `build/port/stubs.txt`, `build/port/ttt` and its map |

Developer only, when the decompilation's data split changes (needs the split of your `SLUS_200.01`):
`.venv/bin/python configure.py && python3 port/tools/gen_data.py && python3 port/tools/make_layout.py` rewrites
`port/tools/data_layout.txt` ([data.md](data.md)).

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

Since T25 the program carries none of the game's data: [data.md](data.md) (inventory, the data file, the disc
checks). The layout it loads by is made by the developer tools below.

- `gen_data.py` converts, for the host assembler, the data the matching build links from the split:
  every `asm/data/*.{data,rodata,lit4,sdata,sbss,bss}.s`, every table a C file takes with `INCLUDE_RODATA`, and
  the data blocks splat moved into the files of functions that are still drafts (strings and constants the draft
  C refers to by name). `.word` becomes `.long`, `.align N` becomes `.p2align N`, a word that points at a label
  inside library assembly becomes 0. Jump tables are left out: the C makes its own. Each file goes into a writable
  section of its own, `.ttt.<file>` (the game patches some read-only tables), aligned only as far as its PS2 address
  is, and `image.txt` lists each section with its PS2 address, size and kind.
- `make_layout.py` compares each assembled file with the executable at its PS2 address and writes
  `port/tools/data_layout.txt`: the initialised ranges (0x00415700-0x005F6AE4), the zeroed span (0x005F6B00 to
  0x01FCDEF0), the words the host linker fills in with a name that is not a data address, the words that are 0
  here, and the payload's size and SHA-1. No data byte.
- `link.py` turns the layout into `image.ld` (absolute `__ttt_data_*` / `__ttt_bss_*`: nothing is linked at the
  PS2 addresses) and `data_table.c` (`gPortDataRanges`, `gPortDataFix`, the hashes). The game's code stays native
  wherever the host linker puts it (0x08049000 on).
- `link.py` asks the linker what nobody defines, then places what it can by PS2 address in
  `build/port/gen/symbols.ld`: a name inside the layout's ranges becomes its PS2 address (labels splat did not
  make, such as `D_00437A50` inside `.data`, and every data label the C uses), and so does a name outside the
  program image (hardware registers like `D_11000010`, the end of `.bss` `D_1FCDEF0`, constants splat took for
  addresses such as `D_100000`). Everything left becomes a stub. A word of the layout whose name lands at its own
  PS2 address keeps the disc's value; the others (273: functions, and names the port defines) are rewritten.

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
| `0x00010000`-`0x02000000` | 32 MB less 64 KB | EE RAM: one shared memory object (`memfd`); the game's initialised data is copied into it from the disc (`plat_data.c`), its bss is the object's zeroes. Holds the game's data and bss at their addresses, the fixed buffers below the program, and the PS2 stack area above the bss. |
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
| `plat_data.c` | The game's initialised data from the disc into EE RAM at start, through the data file `ttt.dat`; the disc's SHA-1 checks and the refusal of a modified disc ([data.md](data.md)) |
| `plat_file.c` | The disc (`TTT_DISC`: a directory with `TEKKEN.BIN` or the `.iso`; without it the program stops at the first read, exit code 2) and `sceSdRemote`: RSPU2DRV's file reads (0x204E / 0x2045), status (0x2030), the sound commands passed to `plat_snd.c`, the movie stream's arguments (0x2058) |
| `spu2.c` | The SPU2 model: 2 MB sound RAM, 2 x 24 voices, PS-ADPCM, pitch, ADSR, the 48 kHz mix, `TTT_SELFTEST=spu` ([sound.md](sound.md)) |
| `plat_snd.c` | libspu2's commands of RSPU2DRV on the model, the per-VBlank mix, `TTT_WAV`, the ring to the window's audio ([sound.md](sound.md)) |
| `plat_bgm.c` | RSPU2DRV's BGM stream engine: the 28 tracks, the sound queue (0x7F0F), status bits 4 / 6, the double buffers ([sound.md](sound.md)) |
| `plat_lib.c` | libgraph (`sceGsSyncV` waits for the next VBlank; `sceGsResetGraph`, `sceGsSetDefDBuff`, `sceGsSetDefDispEnv`, `sceGsPutDispEnv`, `sceGsSwapDBuff` build the environments and write the GS), a cache call |
| `plat_movie.c` | The movies' pictures (the video of the interleaved file, PSS split, `third_party/pl_mpeg2` MPEG-2 decoder, the IPU's RGBA32 layout) and audio track (the driver's per-movie table), `TTT_MOVIES` ([movies.md](movies.md)) |
| `plat_mc.c` | libmc: memory cards as directories under `TTT_SAVES`, asynchronous requests finished by `sceMcSync` after a fixed number of polls, atomic writes, `TTT_SELFTEST=mc` ([Memory cards](#memory-cards)) |
| `plat_pad.c` | libpad / libmtap: two DualShock 2 pads on ports 0 and 1, no multitap, the per-VBlank latch, `TTT_INPUT_PLAY` / `TTT_INPUT_RECORD`, `TTT_SELFTEST=pad` ([Input](#input)) |
| `gs_core.c` | GS: GIF paths (PACKED / REGLIST / IMAGE), registers, 4 MB GS memory with its layout, CLUT buffer, the software reference rasteriser, the frame dump, `TTT_SELFTEST=gs` |
| `gs_vif1.c` | VIF1: UNPACK into VU1 memory, MPG, DIRECT / DIRECTHL to the GIF, MSCAL / MSCALF / MSCNT run the VU1 program |
| `gs_vu1.c` | VU1: registers, the program runs, XGKICK on PATH1, `TTT_SELFTEST=vu1` |
| `vu_micro.c` | The reference micro-mode interpreter shared by VU0 and VU1 (`Vu_RunRef`, one step per instruction in integer arithmetic), `Vu_SameState` |
| `vu_fast.c` | The predecoded interpreter the game runs on (`Vu_Run` with a `VuOp` cache) ([VU1](#vu1), [Speed](#speed)) |
| `vu_micro_inl.h` / `vu_simd_inl.h` | Their shared arithmetic and pipeline helpers / the vector unit's arithmetic on the SSE unit (four lanes, exact) |
| `vu_fasttest.c` | `TTT_SELFTEST=vu1`'s second part: random programs on both interpreters, the SSE arithmetic against the model |
| `plat_pkt.c` | libpkt: VIF1 and GIF packet builders |
| `plat_vu0.c` | libvu0 in C (column-major matrices; `func_0034A868` is `sceVu0Normalize`) |
| `plat_main.c` | Reads the settings file, `TTT_SELFTEST=settings` / `pad`, opens the window (on the host stack), then runs the game's `main` on the EE stack |
| `plat_settings.c` | The settings file (`key = value`, unknown lines kept, atomic writes), the environment's precedence, key names, the keyboard's bindings ([settings.md](settings.md)) |
| `overlay.cpp` / `overlay.h` / `imconfig_ttt.h` | The F1 overlay and the quit question: Dear ImGui (`third_party/imgui`) on the window thread, its SDL glue, its software-renderer path, its maths ([settings.md](settings.md)) |
| `plat_state.c` | The state: the regions the modules register, the gameplay hash, save / load at the VBlank boundary (a stack switch), `Port_OffEe`, the synctest ([determinism.md](determinism.md)) |
| `plat_window.c` | The SDL3 window: SDL loaded at run time, its own thread, the GL context ([Window](#window)), the audio stream, F1 / the quit question / the live settings, `TTT_SELFTEST=window` |
| `gs_gl.c` | The GS's OpenGL 3.3 back end on the window thread ([GL back end](#gl-back-end)) |
| `gs_texpack.c` | Texture packs for the GL back end: PCSX2's texture names (XXH3), PNG dumps and replacements on a worker thread, `third_party/stb` ([textures.md](textures.md)) |
| `gs_async.c` | The GS threads: VIF1 and VU1, then the GIF and the GS front end, beside the game ([Speed](#speed)) |
| `gs_int.h` | The GS commands and helpers both back ends share |
| `port_host.h` | Host library calls added since T30, declared `noplt` (gotcha 20) |
| `softfloat_ps2.c` | The EE float model: `__addsf3`... for the FPU, `Port_Vu*` for the vector unit |
| `plat_libc.c` | newlib's `rand` / `srand` (the executable's 0x40FBD0 / 0x40FBC0) on `_impure_ptr->_rand_next` |
| `plat_floattest.c` | `TTT_DUMP=<dir>` | Write the displayed picture as `<dir>/frame_<vblank>.png` (8-bit RGB) every `TTT_DUMP_EVERY` VBlanks. Put it under `build/` (ignored by git): the pictures are the game's. |
| `TTT_DUMP_EVERY=<n>` | The dump's period in VBlanks (default 60). |
| `TTT_SELFTEST=gs` | GS memory layout of every format, the upload paths, a GIF PACKED packet on the GIF channel (flat, CLUT-textured, blended, depth-tested, scissored sprites, a triangle), the read circuits' merge (ALP / circuit 1 alpha, SLBG, DISPLAY position), VIF1 UNPACK modes / STCYCL / masks / double buffer / MPG, DIRECT and FINISH (`tests/test_port_gs.py`). |
| `TTT_SELFTEST=float` |
| `vu0_micro.c` | VIF0 (MPG uploads) and the VU0 program start (vcallms) on `vu_fast.c` |
| `ee_diff.c` | The draft divergence check: an EE interpreter for the executable's code, `PORT_DIFF`, `TTT_DIFF`, `TTT_SELFTEST=ee` |

- **Polling.** `EE_HW_POLL()` (`include/ee_asm.h`, nothing in the matching build) sits in every game loop that waits
  on the hardware: `Dma_Wait` and the other CHCR loops of `dma_gif.c` / `dma_spr.c`, the D_ENABLER loops, Timer 0 in
  `Frame_PreUpdate`, `Gfx_WaitVSync`. Each poll advances the clock and brings the hardware up to it. A new hang
  in a register loop means a missing hook.
- **Clock.** Headless (`TTT_HEADLESS=1`, implied by `TTT_FRAMES`): one poll is one Timer 0 tick (256 bus cycles),
  nothing sleeps, a run repeats exactly (polls per frame, `gRngSpin`, the boot seed). Otherwise the clock is the
  host's monotonic time: VBlanks at real 59.94 Hz, seed and spin generator vary per run as on the console; a poll
  more than 1 ms from the next VBlank sleeps 0.2 ms when the game is waiting (T38 b0). Host nanoseconds become bus
  cycles exactly (`ns_to_cycles`): until T38 b2 the factor was truncated to 147 cycles per microsecond (147.456), and
  the real-time clock ran 0.3 % slow, 59.76 VBlanks a second ([video.md, Frame pacing](video.md#frame-pacing)). When the host falls more than a VBlank behind, the lost time is
  dropped (at most one VBlank per poll): a burst of VBlanks in one poll ran the game's VBlank handler again before the
  DMA it waits for could finish, and the game hung in `Dma_SendChain` (seen when SDL's start-up stalled a VBlank).
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
- **Sound (T32 batch 1).** RSPU2DRV's commands run on a software SPU2 ([sound.md](sound.md)): the sound effects
  and voices from the banks in SPU2 RAM, and the BGM stream engine (disc, SPU-resident and EE-memory tracks, the
  sound queue's play / hold / stop / fades / volume / stereo). Mixed at each VBlank (800.8 frames at 48 kHz) on the
  game's thread, so what the game reads back is the same on every run; heard through SDL3 in the window, dumped with
  `TTT_WAV`. The engine now reports a held track ready (status bit 4), which the attract cycle's loading mode waits
  for: the music starts at VBlank 786 instead of after the stub's 300-frame timeout, the attract cycle runs about
  300 VBlanks earlier from the frame at 750 on, and the 2551-VBlank run ends with `sbss-hash 0x4a16b393` (with bit 4
  masked: the old 17 frames, and only `gIopStatus` and the two moved function pointers of gotcha 20 differ).
  Boot fix: a copy of the program away from the repository found no disc and segfaulted in `func_00345410` (the
  reads failed silently and the boot ran over unfilled memory); now it looks for `iso/` beside the program too and
  stops at the first read when there is no disc (`tests/test_port_platform.py`, `ProgramElsewhere`).
- **Movies.** `func_0039C878` asks `Port_MoviePlayable()` (0 for now) and skips the movie as when the stream engine
  never goes idle.

Switches:

| Variable | Effect |
|---|---|
| `TTT_DISC=<dir or .iso>` | The disc: a directory with `TEKKEN.BIN` (as `scripts/setup_disc.py` copies it to `iso/`) or the ISO 9660 image. Default: `iso/` of the repository (`build/port/ttt` -> `../../iso`), else `iso/` beside the program, else `iso/` in the working directory. Read only. Without a disc (and without a data file) the program stops with `ttt: no disc` on stderr and exit code 2. |
| `TTT_DATA=<file>` | The data file holding the game's initialised data taken from the disc ([data.md](data.md)). Default: `ttt.dat` beside the program. Made on the first run. |
| `TTT_ALLOW_MODIFIED=1` | Run a disc whose files differ from the USA release (otherwise refused with exit code 4); the run is marked `[modified game data]`. |
| `TTT_FRAMES=<n>` | Stop after n VBlanks with a report: VBlanks and frames, `modes:` (mode@VBlank of each change), last mode and sub-state, `state:` (seed, `gRngSpin`, the gameplay hash of [determinism.md](determinism.md), cycles), placeholders hit. Exit code 0. Implies headless. |
| `TTT_HASH_EVERY=<n>` / `TTT_SAVESTATE=<vblank>:<file>` / `TTT_LOADSTATE=<file>` / `TTT_SYNCTEST=<n>` | The gameplay hash every n VBlanks, a save, a load, the synctest ([determinism.md](determinism.md)). `TTT_SELFTEST=state` checks the hash's rule and an in-process save / load. |
| `TTT_HEADLESS=0/1` | Force the real-time or the headless clock. |
| `TTT_SEED=<n>` | Pin the boot seed `Rng_Init` takes from Timer 0 (`src/rng_misc.c`, `docs/known_bugs.md` section 8). With the headless clock the seed is already repeatable; this sets its value, and makes real-time runs start alike. |
| `TTT_MOVIES=0` | Skip the movies (the intro at boot, the attract and ending movies) as before T33: the player sees the stream never go idle. Default: they play ([movies.md](movies.md)). |
| `TTT_SELFTEST=hw / rpc / mem` | Check the DMA, timers, VBlank and interrupts / the SIF, RPC and disc reads / the memory map, and exit (`tests/test_port_platform.py`, `tests/test_port_mem.py`). |
| `TTT_DUMP=<dir>` | Write the displayed picture as `<dir>/frame_<vblank>.png` (8-bit RGB) every `TTT_DUMP_EVERY` VBlanks. Put it under `build/` (ignored by git): the pictures are the game's. |
| `TTT_DUMP_EVERY=<n>` | The dump's period in VBlanks (default 60). |
| `TTT_SELFTEST=gs` | GS memory layout of every format, the upload paths, a GIF PACKED packet on the GIF channel (flat, CLUT-textured, blended, depth-tested, scissored sprites, a triangle), VIF1 UNPACK modes / STCYCL / masks / double buffer / MPG, DIRECT and FINISH (`tests/test_port_gs.py`). |
| `TTT_SELFTEST=float` | The EE float model against PCSX2 data, EE rules, exact libm cases, newlib `rand`, the fields libvu0's helpers write (`tests/test_port_float.py`). `TTT_FLOATDATA` overrides the data directory (`port/tests/ps2float`). |
| `TTT_SELFTEST=vu1` | Hand-assembled VU1 programs sent through VIF1: pairing, delay slots, E bit, Q / P latency, flag delays, integer / EFU instructions, XTOP / XITOP, XGKICK; then 9000 random programs on the predecoded and the reference interpreter, and the SSE arithmetic against the integer model on 3.2 million operations (`ttt: vu fast: ...`, `tests/test_port_gs.py`). |
| `TTT_VU_CHECK=1` | Every VU1 program also runs on the reference interpreter, from a copy of the unit; the report adds `ttt: vu1 check: N runs, M differ` (the 2551-VBlank run: 1294286 runs, 0 differ). Twice as slow; the game's state is the same. |
| `TTT_VU1_CALLS=1` | With `TTT_FRAMES`: the report also lists the VU1 runs per entry address. |
| `TTT_GS_TRACE=1` | Per VBlank: pixels written per frame buffer (FBP) with the last FRAME / TEX0 / TEST / ALPHA, each GIF path's tag count and tag state, DISPFB1 / 2, PMODE, DISPLAY1 / 2 and BGCOLOR, every transfer (BITBLTBUF / TRXPOS / TRXREG / TRXDIR), and the first GIF tags with reserved bits set (`bad tag`, with the DMA block they came in). |
| `TTT_GS_TRACE_PRIMS=<vblank>` | Every vertex kick drawn before that VBlank (type, context, coordinates, UV / STQ, colour, TEX0 / TEX1 / CLAMP / FRAME / TEST / ALPHA). Vertices that only queue (XYZ3 / ADC) print as `gs-adc`. With `TTT_GS_TRACE_PIXEL=<x>,<y>`: every write to that pixel then, with its source and result, and every rejected one (`gs-reject`: scissor, alpha, destination alpha or depth test). |
| `TTT_SELFTEST=ee` | The EE interpreter of the draft divergence check against port code known to agree with the executable (libvu0, matched C, libm, the VU0 rotation builders) on fixed and 4000 generated inputs (`tests/test_port_ee.py`). Needs `build/SLUS_200.01`. |
| `TTT_DIFF=<vblank>[:<calls>]` | From that VBlank on, each `PORT_DIFF` hook runs its call twice, as the port's C and as the executable's code, and prints the differing words ([Draft divergence check](#draft-divergence-check)). `TTT_DIFF_ONLY=<name,...>` limits it; `TTT_DIFF_TRACE=1` also prints the calls each side makes (`c-call` / `ee-call`). |
| `TTT_GS_TEXDUMP=<vblank>:<tex0 hex>:<x>:<y>:<w>:<h>` | With `TTT_DUMP`: writes that region of the texture TEX0 describes (through a CLUT loaded from it) as `tex_<vblank>.png`. |
| `TTT_STUB_RETURN=<n>` | Generated stubs return n instead of stopping. |
| `TTT_GS_THREADS=<n>` | Raster threads ([Speed](#speed)). Default: one per processor but one, at most 8; `0` draws on the game's thread. Same results either way. The GS traces force 0. |
| `TTT_GS_STATS=1` | With raster threads: a `gs-stats:` line on stderr at the report (commands, serial ones, barriers, the game thread's CPU time and how much of it went waiting for the raster threads / the GS thread, the GS thread's, the GIF thread's and the GL window thread's CPU time). |
| `TTT_TIME_FROM=<vblank>` | Measurements: the GS path is drained at that VBlank and the report says `ttt: time: vblanks <from>-<end> in <n> ms` (drained again at the end). Nothing else changes. |
| `TTT_WINDOW=0/1` | The window ([Window](#window)): default on in real time, off headless and for self-tests. `1` with `TTT_FRAMES` shows a headless run as it goes. |
| `TTT_VSYNC=0` | The window presents without the renderer's vsync. |
| `TTT_PACE_FROM=<vblank>` | The GL back end's frame-pacing report (`ttt: gl: pacing` lines, [video.md](video.md#frame-pacing)) starts at that VBlank's picture (default 600). |
| `TTT_CONFIG=<file>` | The settings file (default `$XDG_CONFIG_HOME/ttt/settings.ini` or `~/.config/ttt/settings.ini`). Its values are the defaults of the variables of [settings.md](settings.md) (`TTT_RENDERER`, `TTT_VSYNC`, `TTT_MOVIES`, `TTT_SCALE`, `TTT_VOLUME`...); a variable set in the environment wins. `TTT_SELFTEST=settings` checks the file's round trip. |
| `TTT_NO_QUIT_CONFIRM=1` | Close / Escape quit at once instead of asking "Quit?". |
| `TTT_OVERLAY_TEST=1` | Tests: the overlay driven from inside a window run ([settings.md](settings.md)). |
| `TTT_RESOLUTION=1..8`, `TTT_ASPECT=4:3 / 16:9 / 21:9`, `TTT_DEFLICKER=0`, `TTT_SMOOTH=0`, `TTT_LOW_LATENCY=1` | The GL back end's video options (T37, [video.md](video.md)): internal resolution, widescreen, the game's flicker filter off, nearest scaling into the window, low input lag. Presentation only (the state is the same). |
| `TTT_TEXTURE_DUMP=1`, `TTT_TEXTURE_REPLACE=1`, `TTT_TEXTURES=<folder>` | Texture packs (T39, [textures.md](textures.md)), GL only: write each new texture as `<folder>/SLUS-20001/dumps/<name>.png`, draw `replacements/<name>.png` instead, PCSX2's names (XXH3 of the texture's data and CLUT, format bits). Presentation only (the state is the same). `TTT_SELFTEST=texpack` checks the names. |
| `TTT_RENDERER=gl / sw` | In a window: the GS draws on the GPU (`gl`, default; falls back to `sw` when no OpenGL 3.3 core context can be made) or with the software rasteriser (`sw`). Headless runs and dumps without a window are always `sw`. |
| `TTT_WINDOW_HIDDEN=1` | The window stays hidden (tests, measurements; with `SDL_VIDEO_DRIVER=offscreen` no display is needed). |
| `TTT_GS_ASYNC=0 / 1 / 2` | The GS threads ([Speed](#speed)): 1 one GS thread, 2 two (VIF1 / VU1, then the GIF / GS front end). Default 2 with GL, 1 with raster threads, 0 for traces and self-tests. The same results in every mode (`tests/test_port_gl.py`). |
| `TTT_SELFTEST=gl` | The GL back end's rules on made-up primitives, no GL context: the alpha test's outcome over a primitive's alpha range, the shared depth write, the texels a UV primitive reads, the CLUT halves of a load (`tests/test_port_gl.py`). |
| `TTT_SELFTEST=window` | The keyboard's default mapping and the quit keys on made-up SDL events (no SDL, no window). |
| `TTT_SDL_LIB=<path>` | The SDL3 library to load (default `libSDL3.so.0`). |
| `TTT_PADS=0/1/2` | Pads connected ([Input](#input)). Default: 2 in real time, 0 headless, 2 with `TTT_INPUT_PLAY`. |
| `TTT_INPUT_PLAY=<file>` | The pads' states come from that recording instead of the keyboard and gamepads. |
| `TTT_INPUT_RECORD=<file>` | Write the pads' states latched at each VBlank to that file (a line per change). |
| `TTT_SELFTEST=pad` | The recording's line format, libpad's read data and the answers of the game's pad set-up sequence (`tests/test_port_input.py`). |
| `TTT_WAV=<file>` | Write the run's sound as a 48 kHz 16-bit stereo WAV (headless runs too; the header is finished at the `TTT_FRAMES` report). Put it outside the repository: the sound is the game's ([sound.md](sound.md)). |
| `TTT_AUDIO=0` | The window opens no audio device (default: SDL's default playback device; `SDL_AUDIO_DRIVER=dummy` / `disk` for tests). |
| `TTT_SND_TRACE=1` | Print every sound command with its VBlank (and the struct of the 0x7xxx sets). |
| `TTT_SELFTEST=spu` | The SPU2 model on synthetic ADPCM blocks: decoder, loop flags, key status, pitch, ADSR, a deterministic mix (`tests/test_port_sound.py`). |

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

## Speed

T30 batch 1 measured the headless run with `perf` and changed only how the results are computed, never the results:
every step was checked against the frames, the report and the state hash of the 2551-VBlank run
(`TTT_FRAMES=2551 TTT_SEED=1 TTT_DUMP_EVERY=150`: 17 frames, `sbss-hash 0x8dd3eebb`) and of a 1800-VBlank run.
Times on a 24-thread x86-64 under `scripts/cool.sh` (the thermal governor's quota makes them vary by up to 2x):

| Build | 2551 VBlanks | VBlanks/s |
|---|---|---|
| before (T29 batch 5) | 386 s | 6.6 |
| T30, `TTT_GS_THREADS=0` | 218 s | 11.7 |
| T30, 8 raster threads (default here) | 112 s | 22.8 |
| T30 batch 2, `TTT_GS_THREADS=0` | 188 s | 13.6 |
| T30 batch 2, 8 raster threads | 60-65 s | 39-42 |

T30 batch 2 timed the fight on its own (VBlanks 1101-2551, where VU1 draws the fighters; the 1100-VBlank run
subtracted), the batch 1 build next to it on the same day: 118-128 s -> 53-59 s (11.4-12.2 -> 24.7-27.4 VBlanks/s);
the game thread's CPU time without its waits 80.6 s -> 37.6 s (55.6 -> 25.9 ms per VBlank). The thermal governor
throttles the whole slice when hot, so wall times move by 10-20% between runs; CPU times (`TTT_GS_STATS`) less.

- **Profile before:** rasteriser about 75% (`vram_rw`, `draw_triangle`, `pixel`, `texel`, per-pixel 64-bit divisions in
  `__divdi3`), VU1 interpreter about 15%. In the demo fight the GS writes about 2.2 million pixels per VBlank (1.8
  million into the 640 x 448 frame alone).
- **Rasteriser (`gs_core.c`).** The registers a primitive reads are decoded once per primitive (`Draw`) instead of per
  pixel; a pixel's frame / depth word is addressed once for its read and write (`VLoc`); a triangle's rows are cut to
  the span inside all three edges, exactly, and along a span the barycentric fractions are running quotients and the
  attributes running sums (no division or 64-bit multiply per pixel); a sprite's U is a running quotient; an indexed
  texture's CLUT is expanded once while CLUT, CPSM / CSA and TEXA stay; the level of detail is only computed when
  MMAG and MMIN filter differently. `-O3` for `gs_core.c`, `vu_micro.c` and `vu_fast.c` (`toolchain.HOT_FILES`).
- **Raster threads.** The front end (GIF, registers, the vertex queue: the game's thread) turns primitives and the
  operations on GS memory into commands in a ring; each worker runs every command and draws only the rows of its
  8-row bands (16 before T30 batch 2; 8- and 4-bit formats pack a column's four rows into a word, inside one band).
  That is the serial result while,
  since the last barrier, GS memory is written through one frame and one depth layout and nothing read (texture,
  CLUT, the window's picture) is written: the front end tracks the 8 KB pages written and read since the last
  barrier and puts a barrier in front of a command that breaks this, or runs it on one worker (serial) when it
  breaks it alone (frame, depth and texture pages of one primitive overlapping). Uploads, local copies and the dump's
  picture are serial; CLUT loads run on every worker, each into its own CLUT. In the 2551-VBlank run: 38 million
  commands, 0.25% serial, 0.1% after a barrier. The register side effects the game polls (SIGNAL / FINISH / LABEL
  in CSR) stay on the game's thread.
- **VU.** The vector unit's add and multiply in 32-bit arithmetic (`sf_vadd` / `sf_vmul`: 7 guard bits and a sticky
  unit instead of 64-bit mantissas and a normalisation loop; `TTT_SELFTEST=float` compares them with the model on 2
  million pairs); the MAC flags are worked out only when an instruction reads them; `memcpy` is inlined in the hot
  files (`-fno-builtin` sent every 4-byte instruction fetch through the PLT).
- **Predecoded VU interpreter (T30 batch 2, `vu_fast.c`).** The fight runs 1.8 million VU1 instructions per VBlank
  (`mesh.vsm`). Each micro-memory slot is decoded once into a `VuOp` (operation, operands, dest mask, branch target);
  the raw words are compared at each fetch, so an MPG or any other write to micro memory is seen. One dispatch per
  half (the upper on operation and second operand together); Q, P and the flag queues are brought up to the current
  step only where an instruction reads them and at the end, which gives the same values; the next pc skips the
  branch state unless something branches or ends. The upper half runs on four lanes at once in SSE2
  (`vu_simd_inl.h`): add, multiply, divide, square root and the int / float conversions on the SSE unit with MXCSR
  set to round toward zero and flush to zero for the program (put back around XGKICK and at the end). Under that mode
  the SSE unit computes the EE float model exactly: the model cuts the exact result toward zero, and with a cut
  "tiny before rounding" is "tiny after rounding", so flush to zero gives the model's +/-0 below 2^-126; overflow
  gives +/-FLT_MAX as the model; operands of exponent 0 / 255 are mapped to +/-0 / +/-FLT_MAX first. Max / min stay
  integer compares. 5 ns per VU instruction instead of 14 on captured fight programs; in the game 2.5 times fewer
  cycles than the old interpreter, 13 ms of a fight VBlank. `Vu_RunRef` keeps the one-step integer interpreter as the specification:
  `TTT_SELFTEST=vu1` runs random programs through both, `TTT_VU_CHECK=1` every program of the game.
- **Ring (T30 batch 2).** The ring's counters each have a cache line (the workers' tails shared one, written at every
  command by eight threads); workers read the head once per batch of commands, the front end re-reads the tails only
  when the ring looks full, and the workers wake sleepers every 32 commands, at barriers and before they wait instead
  of after every command (a fence each). Waits spin 100 pauses before sleeping instead of 4000: spinning used the
  governor's quota the drawing needed. 8-row bands give a 448-row frame 7 bands for each of 8 workers (16-row bands
  gave four workers 4 and four 3). 8 workers stay the best count here (6: 73 s, 12: 77 s).
- **VIF1 UNPACK.** The game's UNPACKs (unmasked, STMOD 0: V2 / V3 / V4-32, V4-8, S-32) write whole vectors from the
  data: a direct path without the mask / mode per field.
- **Left:** the raster workers are the limit in the fight: each spends about 26 ms of a fight VBlank drawing (texel
  address and CLUT per texel, the bilinear filter, the pixel's read-modify-write, 64-bit divisions for S / Q), and at
  the 140000 barriers / serial commands of the run (render-to-texture passes: a texture read from pages drawn since
  the last barrier) they wait for each other. The game's thread needs about 26 ms per fight VBlank (VU1 13 ms, the
  rest DMA, VIF, the GS front end). 60 VBlanks/s needs both halved: a GPU back end for the drawing, and the VU1 runs
  on a thread of their own (with the VIF / GS front end) are the next steps.
- **GS thread (T30 batch 3, `gs_async.c`).** The DMA channels still carry VIF1 / GIF transfers out at the same polls
  and finish them at once; the data (copied: the game reuses its buffers and the MFIFO ring) and each VBlank's display
  registers go into a 32 MB queue that the GS thread carries out in order: the same VIF codes, VU1 programs, GIF
  packets, commands and pictures. Nothing the game reads comes from that work: VU1 memory is written by the game only
  in `Gfx_Init` after `sceGsResetGraph`, which waits for the queue (as do the report and the self-tests); the VIF1
  registers are not modelled; of GS CSR the game reads FIELD only, and the SIGNAL / FINISH / LABEL bits the packets set
  are written into CSR / SIGLBLID by the game's thread at the next VBlank (a race on CSR with the VBlank's FIELD flip
  is avoided). The 2551-VBlank run gives the same report, frames and hash; `tests/test_port_window.py` compares
  `TTT_GS_ASYNC=0` and `1` runs byte for byte. Headless with the software rasteriser the raster workers stay the limit
  (the run takes the same 55-65 s).
- **GL (T30 batch 3), the scripted fight** (`TTT_INPUT_PLAY` of `tests/test_port_input.py`, `TTT_FRAMES` 1101 and
  1451 subtracted, hidden window, `TTT_VSYNC=0`, the headless clock: as fast as it goes): GL 8.7 s for the 350
  VBlanks (40 VBlanks/s), software rasteriser 14.9 s (23.5). CPU per fight VBlank: the game's thread about 10 ms (the
  VU0 skinning programs are two thirds of it), the GS thread about 15 ms (VU1 about 9), the window thread about 21 ms:
  the GL thread is the limit, mostly in the driver's draw calls (about 2700 batches and 5400 draw calls per VBlank,
  the two-pass alpha test of the fighters' and stage's FB_ONLY batches, which only hold primitives with one state
  record) and 32-bit glvnd's per-call TLS lookups. Times vary by 10-20% with the thermal governor.
- **GL fights at 60 VBlanks/s (T30 batch 4).** Measured with `TTT_TIME_FROM=1101` in the 1451-VBlank scripted fight
  (the GS path drained at VBlank 1101 and at the end, so one run times the 350 fight VBlanks; the batch 3 numbers
  subtracted two runs, which moves by 20% with the thermal governor). Per fight VBlank:

  | | batch 3 | batch 4 |
  |---|---|---|
  | VBlanks/s, GL | 40 | 62-70 |
  | VBlanks/s, software rasteriser | 23.5 | 22-24 |
  | draw calls / batches | 5300 / 2700 | 2000 / 1650 |
  | resolves / read backs (GPU -> CPU) | 47 / 1 | 16 / 0 |
  | game thread CPU | 10 ms | 9 ms |
  | GS thread CPU (VIF1, VU1, GIF, front end) | 15.7 ms | 12.2 (VIF1, VU1) + 4.3 (GIF, front end) |
  | window thread CPU (GL) | 19.5 ms | 5.9 ms |

  - The window thread lost a third of its time in the kernel: the vertex and state buffers got new storage
    (`glBufferData` with no data) at every flush, about 50 per VBlank, and the driver faulted in fresh pages for each
    (700 page faults per VBlank). They are rings now, written with `glMapBufferRange` unsynchronised after what the
    GPU may still read, and given new storage only when they wrap (the state records' index is absolute in the ring).
  - Draw calls: see [GL back end](#gl-back-end) (the alpha test's outcome per primitive, the shared depth write).
  - Texture hazards: a UV primitive's texels are taken from its UV as the shader samples them (no margin for nearest
    filtering, the sprite's right edge left out, CLAMP clamped): the HUD font in the frame buffer's top byte no longer
    resolved the depth buffer at every glyph, nor a strip copy its destination; resolves fell from 47 to 16.
  - The GS path on two threads (`TTT_GS_ASYNC=2`, the default with GL): VIF1 and the VU1 programs on one, the GIF
    and the GS front end on the other, through a second 32 MB queue (XGKICK writes its packet straight into it). The
    VU1 thread, about 12 ms per fight VBlank (VU1 itself 10), is now the limit. With the software rasteriser the
    default stays one GS thread: its eight raster workers want the processors, and a third GS thread slowed it.
  - Wake-ups: a thread waiting for room in a queue or the ring now waits for a quarter of it, and a sleeping reader
    is woken for 256 KB of records (or a VBlank, or when its writer waits) instead of at every record: each wake-up
    is a futex call for both, and the GS thread woke the GIF thread at every XGKICK. The GL thread publishes its place
    in the ring every 64 commands (the cache line went back and forth at every command).
  - VU0 (game thread): the skinning programs already run on the predecoded interpreter (`vu_fast.c`, since batch 2);
    the not-implemented counters (256 words) were summed after each program, now every 1024 programs.
- **Left:** the VU1 thread (the predecoded interpreter, 2 million VU1 instructions per fight VBlank) and the game
  thread (VU0 skinning about two thirds) are now the limits, with the window thread about half busy; the preparing
  thread for the GL batches was not needed for 60. Times vary by 10-20% with the thermal governor.
- **Real time (T38 b0).** The numbers above use the headless clock. On the real-time clock (59.94 VBlanks a second of
  wall time) the scripted fight made only about 400 game frames in 1451 VBlanks, with either renderer, from the
  character select on (about one frame in five VBlanks), although the machine runs it at 84 VBlanks/s headless.
  - The game's rate: `main` (src/main.c) waits for one VBlank per loop (`Frame_Begin` hands the frame to the VBlank
    handler and waits in `Gfx_WaitVSync`; `Frame_PreUpdate` then waits for Timer 0 to pass 0x724, 3.2 ms), so the PS2
    makes one game frame per VBlank whenever a frame's work fits; headless the replay makes 1401 frames in 1451
    VBlanks (the stage load holds the loop for the rest).
  - Cause: `Port_HwPoll` gave the host CPU back (`nanosleep` 0.2 ms) at every real-time poll more than 1 ms before
    the next VBlank. Those polls are not only the waits: a frame's work polls hundreds of times (each DMA transfer it
    starts is carried out by the next poll, which the game waits for), so a character select frame of about 3 ms
    (headless) took about 80 ms. Measured with a per-30-VBlank log: the same VU0 program count per frame as headless,
    a fifth of the frames per second; with only the sleep removed the frame counts match headless, so neither the
    clock's catch-up rule, the GS threads, the ring nor the present was the cause, and the cgroup quota of
    `scripts/cool.sh` gives the same counts as outside it.
  - Fix (`rt_waiting` in `plat_hw.c`): a real-time poll sleeps only when the last 64 polls did no work (no DMA
    transfer carried out, no VBlank raised; the interrupt handlers follow from these): the game's wait loops, never
    the polls inside a frame's work. The check is out of line and only called in real time: a first version that
    counted the work inside `Port_HwPoll` / `dma_poll` changed the idle run's hash (only the scratchpad region,
    `TTT_HASH_REGIONS=1`: the host frames of the poll leave different bytes there), as any change to the code of the
    headless poll path can. Headless runs are unchanged (idle run hash, GL fight hash).
  - Scripted fight, 1451 VBlanks, hidden window (`SDL_VIDEO_DRIVER=offscreen`, `TTT_HEADLESS=0`), game frames:

    | | before | after |
    |---|---|---|
    | GL, outside `cool.sh` | 395 | 1391-1400 |
    | GL, under `cool.sh` | 399 | 1394-1397 |
    | software, outside `cool.sh` | 389 | 1194-1200 |
    | software, under `cool.sh` | 467 | 1195-1199 |
    | headless (the intended count) | 1401 | 1401 |

    The software renderer still drops frames in the fight: its raster workers manage about 23 fight VBlanks/s
    (above). Before the fix the replay's presses at fixed VBlanks reached a slower game, so those runs never reached
    the fight; now they reach it at the same VBlank as headless (about 562).
  - `tests/test_port_window.py` (`RealTime`) runs the replay for 1000 real-time VBlanks with GL in a hidden window
    (about 20 s; skipped without SDL3 or an OpenGL 3.3 context) and asks for at least 90% of the headless 950 frames.
  - The exit crash at 8x (T37: SIGSEGV after a complete report in about half of the 8x fight runs here): the window
    thread's release of the GL context, the window and SDL takes 0.03 s at 1x but at 8x 0.6 s after 300 VBlanks,
    1.9 s after 760 and more after the fight, and the report waited only 2 s for it, then called `exit()`. The cores:
    the window thread still inside `SDL_Quit`, the game's thread at an address outside every loaded library (most
    likely an exit handler of a library `SDL_Quit` was unloading). The report now waits up to 30 s, and a run with a
    window ends with `_exit` after flushing (`Port_WindowExit`; the port registers no exit handlers): 16 of 16 8x
    fight runs exit cleanly (4 of 8 crashed before).
  - Stage drafts (T38 batch 1). The stage depends on the boot seed (the spin generator in real time, so it varies
    from run to run there; `TTT_SEED` pins it headless). Running the scripted fight headless with `TTT_SEED=1..32`
    found every failure the real-time runs had shown, now repeatable:
    - Lanes stage (seeds 3, 5 and 19 with these fixes): the draft of `func_003D5B58` (src/model_anim_b.c) placed the 40 parts at
      `z = 20000 - r * 5000` where the original scales by 40000 (`D_005F5474`). Parts of a lane must be 4000 apart
      and only one may lie within 4000 of z = 20000, so a 5000-wide band holds two parts per lane: 20 places for 40
      parts, and the placement loop (no polls inside) never ends. The draft of `func_003D5E80` then passed
      `Model_QueueDraw` its list number where the scale goes (prototype `(.., s32, f32, ..)` written in EE register
      order, int and float arguments interleaved differently): a SIGSEGV in `Model_QueueDraw`.
    - Seeds 9, 13, 23, 25: `func_00341218` (src/render_fighter_a.c) declared `func_003D43A8(f32, f32, f32 *)`
      and called it with the pointer last; the definition takes `(f32 *pos, f32 scale, f32 off)`. On the EE both
      put the pointer in `$a0` and the floats in `$f12` / `$f13`, so the matching build is the same; on the PC the
      pointer arrived as 120.0f. Fixed in the prototype and the call (SHA-1 kept). Seed 32 reaches the next item.
    - `func_003E6B10` (src/gallery_jpeg.c, reached through `func_003E7210` from `func_00367938` when
      `D_005F5CB6` is 2, 19 or 22) called `func_003E6AA8(0)` where the original passes its packet pointer, so `func_003E6AB0` wrote its
      UNPACK code at EE address 0; it also missed its second float argument (`$f13`, the particles' height) and
      declared `f32 rand()`. Its caller `func_003E6DA0` (byte offsets applied to `s32 *`, float constants converted
      to integers) and the per-frame `func_003E7270` (wrong emitter constants, a null DMA source) were rough m2c
      drafts too; the three are now written from the asm. `TTT_DIFF` with temporary hooks (seed 32, VBlanks 617 and
      620) finds `func_003E7210` (so `func_003E6DA0` and `func_003E6B10`, rand calls included) equal to the
      original; `func_003E7270` differs only in the fromSPR ring buffer, where the check's second transfer
      (`func_00343178` runs on both sides) puts the same packet 0x160 bytes further on. `Mode_Gallery`'s calls were checked against the asm
      (depth 0xFFFF and the fade alphas of `func_003DBCF0`, the YES / NO colours, a 32-bit overflow in the
      BITBLTBUF word, `func_00399820` takes two arguments), and `func_003BB890` (src/modes_b.c) now passes the x,
      y and colour every caller gives it to its `Text_Printf` (SHA-1 kept).
    - Prototypes of one function that differ between files are the common cause: the EE ABI hides a different
      order of int and float arguments, and a missing argument only reads a leftover register there.
    - The `RealTime` test passes 12 of 12 runs with these fixes (1 of 12 failed before, in `func_003E6B10`).

## Window

`port/src/plat_window.c` shows the picture of each VBlank (both read circuits merged, as the dump) in an SDL3 window.

- **SDL3 at run time.** `dlopen("libSDL3.so.0")` (or `TTT_SDL_LIB`) and a table of the 20 functions used: the program
  does not link SDL, headless runs and the tests do not need it, and without it the run says
  `ttt: window: cannot load ...: running headless` and goes on. The program is 32-bit, so it needs the 32-bit
  library: on Arch `lib32-sdl3` from the multilib repository (`sudo pacman -S lib32-sdl3` with `[multilib]` enabled
  in `/etc/pacman.conf`); on Debian / Ubuntu `libsdl3-0:i386` where packaged. Only pointer and integer arguments
  cross into SDL (no `SDL_FRect`: the port has no host floating point).
- **When.** Real-time runs (`build/port/ttt` without `TTT_FRAMES`) open it; `TTT_WINDOW=0` keeps them headless,
  `TTT_WINDOW=1` opens it for a `TTT_FRAMES` run too. Closing it or Escape asks "Quit?" (T36), F1 opens the settings
  overlay ([settings.md](settings.md)). The window thread also reads the keyboard and the gamepads ([Input](#input)).
- **Threads.** SDL runs on a thread of its own, started before the game (`plat_main.c`, on the host stack): the game's
  thread runs on the PS2 stack in EE RAM, 200 KB that grow into the game's `.bss`. Each VBlank the raster workers
  draw the picture into one of three slots (in order with the drawing); the newest finished one is copied to the
  window thread, which uploads it as a texture (RGB24, linear scaling, letterboxed) and presents it with the
  renderer's vsync (`TTT_VSYNC=0`: without). The game's pace is the clock's: 59.94 Hz in real time, as fast as it
  goes headless (the window then skips pictures).
- `TTT_FRAMES` runs with a window end with `ttt: window: N pictures handed over, M shown` (GL: `ttt: window: OpenGL,
  M pictures shown`), then the window thread releases the window and its GL context before the program exits.
- **GL (T30 batch 3).** With `TTT_RENDERER=gl` (default) the window gets an OpenGL 3.3 core context (`SDL_GL_*`,
  loaded from SDL too; GL itself through `SDL_GL_GetProcAddress`) and the window thread is the GS's back end: it runs
  the commands the GS thread queues and presents each VBlank's picture (letterboxed blit), skipping the presentation
  of a picture when a newer one is already queued. Events are read every 4 ms. When the context or the back end
  cannot be made the run says why and uses the software renderer.
- **Closing (T30 batch 3; T36 asks first, `TTT_NO_QUIT_CONFIRM=1` does not).** Escape (by key code or by the key's
  place) and the close button end the program from the window thread itself: it releases the GL context / renderer and the window, then `_exit(0)`, whatever the game's
  thread is doing (before, the game's thread exited at its next VBlank, which never came when it waited on the back
  end, and exit() raced the window thread's driver calls). A key pressed and released between two VBlanks still
  reaches the pads for one VBlank (Return tapped quickly was lost before). `TTT_SELFTEST=window` checks both on
  made-up events; no input is sent to the desktop.

## GL back end

`port/src/gs_gl.c`, on the window thread; the software rasteriser stays the reference.

- **GS memory** stays on the CPU for uploads, local copies and CLUT loads and is mirrored in a 1024 x 1024 RGBA8UI
  texture (a GS word per texel). Every texture read decodes the swizzled layout in the shader from the mirror (the
  page tables as textures), so formats, CLUT tricks and reinterpretations read what the GS would.
- **Targets.** FRAME / ZBUF buffers are RGBA16F / DEPTH32F textures, loaded from the mirror when made or grown and
  written back into it (a resolve) only when something reads the pages drawn since the last one: a texture (UV
  primitives count only their texels), a CLUT load, the display, an upload, another target. A 24-bit target owns the
  low 3 bytes only: the game keeps PSMT8H / 4HL / 4HH fonts in its frame buffer's top byte. Per page the back end
  knows the bytes the CPU wrote (pushed before the next GPU use, masked) and the bytes the GPU wrote (read back
  before the CPU writes or reads them: in the fight about one CLUT load per VBlank reads the frame).
- **Primitives** become integer vertices and a state record in a texture buffer; the shader does the GS's per-pixel
  work in integers (addressing, wrap modes, CLUT, TEXA, bilinear with 4-bit fractions, TFX / TCC, fog, alpha test,
  scissor, FBA); blending is dual-source with the batch's equation (Cs * |ks| + Cd * |kd|, add / subtract / reverse),
  on float targets so that Cd * As with As up to 0xFF (the stages' lighting passes) is not clamped. FB_ONLY /
  ZB_ONLY / RGB_ONLY draw twice (passing, then failing pixels) and such batches hold one state record each, as the
  GS's own: merging them reordered a surface's base, light and add passes per pixel and darkened the stage.
- **Display:** both read circuits merged as PMODE asks (the shader version of the dump's), into a picture of the
  display's size; `TTT_DUMP` with a GL window writes that picture (read back), so GL and software dumps compare
  directly.
- **Batching (T30 batch 4).** An alpha test whose outcome is the same for every pixel of a primitive needs no test: the
  alpha the shader tests is bounded per primitive (the vertices' alpha, one less with Gouraud shading for the GPU's
  interpolation; a CLUT row's smallest and largest entry alpha, TEXA for 24 / 16 bits; TFX / TCC; bilinear stays
  between its texels), so the primitive is drawn in one pass with the passing masks, or the failing ones (FB_ONLY:
  colour only), and joins the batch of any state record. Most FB_ONLY surfaces of the fight are 4 / 8-bit CLUT textures
  with alpha 0x80 against GEQUAL 0x80 (always pass) or below it (always fail). A primitive that should not write depth
  may when its vertices (positions and Z, in order: the GPU makes the same fragments and depths) are those of the last
  primitive that wrote depth wherever it passed its test, its scissor inside that one's and its test GEQUAL / GREATER:
  the depth there is already its own, so the multi-pass surfaces keep one depth mask. Draw calls per fight VBlank fell
  from 5300 to 2000; the pictures are the same.
- **CLUT loads from GPU-drawn pages (T30 batch 4).** In the fight a CLUT load reads the frame buffer once per VBlank
  (a 4-bit CLUT at block 0x20, inside the frame). Instead of reading those pages back (which made the window thread
  wait for the GPU), the load runs on the GPU: the CLUT buffer is a row of a 512-wide R16UI texture, made from the
  mirror for the halves the load writes and from the CPU copy or the previous row for the others; the CLUT rows the
  draws use are expanded from it on the GPU when they read halves only the GPU has. The same load again on the buffer
  it left and an unchanged mirror is skipped (the game sends TEX0 with CLD 1 before every batch). Read backs are left
  for uploads into GPU-drawn pages (rare).
- **Compared** (dumps every 150 VBlanks of the scripted fight, 1451 VBlanks): mean difference 0.02-1.8 levels per
  channel, under 0.05% of the pixels more than 24 levels off (edges, the title's lightning glow); the game's state is
  the same. Not modelled (counted in the `ttt: gl:` report line, all 0 on that path): DATE, C = Ad on a 32 / 16-bit
  frame, partial FBMSK bytes, COLCLAMP 0, PABE with a subtracting equation.
- **Video options (T37, [video.md](video.md)).** Internal resolution 1x-8x (GS memory stays 1x: resolves read each
  pixel's first sample; textures that are colour targets in the same format sample a full-resolution snapshot),
  widescreen 16:9 / 21:9 (margins on 640-wide targets: the 3D scene the VU1 programs already clip at the guard band,
  the HUD in the 4:3 middle), the game's flicker filter off (its 448-line frame shown instead of the 224-line copy),
  smooth / nearest scaling, low input lag (present at the game's copy, `glFinish`); one present per picture (T36
  presented twice).
- **No host float:** the file is integer only (as the rest of `port/src`); floats exist only in the shaders on the
  GPU, and nothing the game reads comes from it (`test_no_host_float_code` needs no exception for it).

## Input

`port/src/plat_pad.c` answers libpad and libmtap as the game uses them (`src/pad.c`); `plat_window.c` reads the
keyboard and the gamepads on the window thread.

- **What the game sees.** Two DualShock 2 pads, on ports 0 and 1 (slot 0), no multitap: `scePadGetState` stable (6),
  analog id 7 (current and extended), pressure mode entered at once, 2 actuators, every request complete at once.
  `Pad_Update` walks them to ready (phase 0x30) in a few frames; from then on `scePadRead` returns libpad's data of
  kind 0x79: the buttons active low, the sticks (right x, right y, left x, left y; 0x80 centre), 12 pressure bytes
  (0xFF down, 0 up). Vibration (`scePadSetActDirect`) is accepted and not played.
- **Once per VBlank.** At each VBlank, before the game's VBlank handler, the pads' state is latched from one source
  (`Port_PadVBlank`, called by `plat_hw.c`); every read until the next VBlank returns it. The window thread keeps the
  keyboard and gamepad state under a mutex and the game thread takes a copy at that point only.
- **Pads connected** (`TTT_PADS`): 2 in real time, 0 in headless runs (as before T34: same reports, frames and hashes;
  the report still lists `scePadGetState` for them), 2 with `TTT_INPUT_PLAY`. `TTT_FRAMES` runs with pads add
  `ttt: input: N pads, M vblanks with a button down`.
- **Record and replay.** `TTT_INPUT_RECORD=<file>` writes the latched states; `TTT_INPUT_PLAY=<file>` feeds them back
  instead of the keyboard. The file is text: `ttt-input 1 pads 2`, then one line per VBlank where the state changes,
  `<vblank> <buttons pad 1> <sticks pad 1> <buttons pad 2> <sticks pad 2>` in hex, buttons with 1 = pressed in
  libpad's order (0x8000 left, 0x4000 down, 0x2000 right, 0x1000 up, 0x0800 Start, 0x0400 R3, 0x0200 L3,
  0x0100 Select, 0x80 square, 0x40 cross, 0x20 circle, 0x10 triangle, 8 R1, 4 L1, 2 R2, 1 L2), sticks as 8 hex
  digits (right x, right y, left x, left y); `#` starts a comment. Under the headless clock a run's VBlanks are the
  same every time, so a recording replays the same run exactly (`tests/test_port_input.py`: Start at the main menu,
  two replays end in the same state, which differs from the run without a button). A real-time recording replays the
  same inputs at the same VBlanks, but the real-time clock itself varies from run to run; to record a run that
  replays exactly, play it headless in a window (as fast as the host goes, not at 59.94 Hz):
  `TTT_HEADLESS=1 TTT_WINDOW=1 TTT_PADS=2 TTT_SEED=1 TTT_INPUT_RECORD=run.txt build/port/ttt`, then replay with
  `TTT_FRAMES=<n> TTT_SEED=1 TTT_INPUT_PLAY=run.txt`.

Controls (the key's place, whatever the keyboard layout; Tekken's default: square left punch, triangle right
punch, cross left kick, circle right kick). The keyboard's keys are the defaults: F1's Controls page rebinds them and
the settings file keeps them (`keys.p1.square = Space`, [settings.md](settings.md)):

| PS2 button | Pad 1 keys | Pad 2 keys | SDL gamepad |
|---|---|---|---|
| D-pad | W A S D | arrows | d-pad (left stick: the analog stick) |
| Square (LP) / Triangle (RP) | U / I | keypad 4 / 5 | west / north (X / Y on Xbox) |
| Cross (LK) / Circle (RK) | J / K | keypad 1 / 2 | south / east (A / B on Xbox) |
| L1 / R1 | Y / O | keypad 7 / 6 | shoulders |
| L2 / R2 | H / L | keypad 8 / 9 | triggers past half way |
| Start / Select | Return / Backspace | keypad Enter / keypad + | Start / Back |
| L3 / R3 | | | stick buttons |

The first two gamepads connected become pads 1 and 2 (`ttt: input: pad N: <name>`), together with the keyboard. Escape
or closing the window asks "Quit?". While the F1 overlay is open the pads see nothing pressed. Gamepad buttons are not
rebindable yet.

From the main menu to the fight (T34 batch 2): Start at the main menu starts ARCADE; in the character select the
d-pad moves the cursor, a punch / kick button picks the character (its costume), and a second pick takes the tag
partner; then the stage loads and the fight starts (sub-state 8 by VBlank 830 of a headless run), and the fighters
follow the pad. `tests/test_port_input.py` replays it: the cursor test (always), and with `TTT_SLOW_TESTS=1` the whole
path, twice with the same result, and against the same run without the fight's inputs. In a window (SDL on X11) the
keys reach pad 1 in a real-time run: D / S / U moved the cursor right and down and picked with square
(`TTT_INPUT_RECORD` shows 0x2000, 0x4000, 0x0080 at the VBlanks of the presses). `TTT_PEEK=<addr>[:<words>],...`
prints EE RAM words at the end of a `TTT_FRAMES` run (`ttt: peek ...`), which the tests read (`gMode` / `gSubMode` at
0x5F5C20, the select state at 0x440D08, the fighters' positions at 0x879644 / 0x884EE4).

## Memory cards

`port/src/plat_mc.c` answers libmc as `src/memcard.c` uses it (T35). The game's card service `Mc_Update` runs one step
a frame: a request (check 3, save 0x12, load 0x1A, screenshots 0x1D / 0x20 / 0x23, delete 0x25, unformat 0x10) becomes
a sequence of libmc calls, each started and then polled with `sceMcSync(1, ...)` until it reports the end.

- **Where.** Card port 0 is the directory `<saves>/mc0/`: the card path `/BASLUS-20001TekkenTT/icon.sys` is the file
  `<saves>/mc0/BASLUS-20001TekkenTT/icon.sys`. `<saves>` is `TTT_SAVES`, else the repository's `saves/` when the
  program is `build/port/ttt` of a checkout, else `saves/` beside the program. Nothing is created until the game
  writes. The files hold data from your disc (the icons) and your records: keep them out of git (`saves/` must be in
  `.gitignore`). Tests and idle runs use a temporary `TTT_SAVES` (`tests/test_port_platform.py`'s `run_ttt`).
- **Cards.** `TTT_MC0` / `TTT_MC1` = `formatted` (port 0's default: an empty formatted card while the directory does
  not exist) | `unformatted` (until the game formats it; formatting creates the directory) | `none` (port 1's
  default). Formatting erases nothing on the host; unformatting lasts for the session only. Free space: 8000 one-KB
  clusters on an empty card [I: the order of an 8 MB card after its file system, not measured], less ceil(size / 1024)
  per file and ceil((entries + 2) / 2) per directory.
- **Timing.** A call only registers its request; `sceMcSync(1, ...)` answers 0 (busy) twice, then carries the request
  out on the host files, stores the command number and result and answers 1 (`TTT_MC_POLLS=<n>` changes the two).
  Results and output arguments (GetInfo's type / free / format, GetDir's table, a read's data) arrive at that last
  poll, as mcserv's reply does. The game polls once a frame, so a request takes the same frames in every run.
- **Result codes.** 0; GetInfo -1 new formatted card (first call), -2 unformatted, -10 no card (type 0); -3 card full
  (a write past the free space), -4 no such file or directory (also mkdir of an existing name), -5 not permitted
  (wrong open mode, a directory, an open file deleted), -6 directory not empty, -7 more than 4 open files. Paths are
  absolute or relative to the port's `sceMcChdir` directory; `..` stops at the card's root.
- **Atomic writes.** A file opened for writing (the game uses mode 0x203, read-write and create, no truncation) is
  kept in memory; `sceMcClose` writes `.ttt-tmp-<name>` beside it, flushes it (fsync), renames it over the file and
  flushes the directory. A crash leaves the old save or the new one; `.ttt-tmp-` names are never listed.
- **Listings.** GetDir (`/BASLUS-20001TekkenTT/*`, up to 24 entries) lists `.` and `..` first in a subdirectory, then
  the files sorted by name, as 64-byte `sceMcTblGetDir` entries (dates in JST from the host's modification time,
  size, attributes 0x8497 / 0x8427, name at 0x20). The check compares the save's two dates with the ones it saw last
  (`gMc.unkD4` / `unkE4`) to clear the 'card changed' bit 2.
- **The game's flow.** Boot (`Mode_Boot` sub-state 1, `func_00361710`): no card or no save data goes on silently;
  a save is loaded (request 0x1A: chdir, open, read 0x480 bytes, close, unscramble and checksum `func_0035EC98`).
  OPTIONS > MEMORY CARD (PS2): LOAD DATA, SAVE DATA, AUTO SAVE (turning it on saves at once), EXIT. A save writes
  `icon.sys`, `xiao2n.ico`, `xiao2c.ico`, `xiao2d.ico` (files 0x89-0x8C of the disc) and `BASLUS-20001TekkenTT`
  (0x480 bytes: a checksum byte and four scrambled blocks, `func_0035EBA0`: system data `D_00439C60`, options
  `D_00440D58`, `D_00415D60`, records `D_0046BC70`). The screenshot files of the gallery are `D_00416010`'s names.
- **Mc_Update's draft** (m2c) had never run past 'no card'. T35 checked it case by case against the asm and fixed
  it: GetDir got a null table instead of `D_005F7300`; the open / read / write / close sequences got size 0 and next
  operation 0 instead of 0x480 / the icon's size (`func_0035CED8`) / the screenshot's size and their follow-up
  operations 0x17 / 0x19 / 0x1C / 0x1F / 0x22; a close that succeeded ended the request (op 2) instead of going on to
  the next operation; the format / unformat results were never read; ops 3 and 31 set `unkD0` (the original goes to
  op 4 without it).
- **Tests.** `TTT_SELFTEST=mc` (the layer on a temporary directory) and `tests/test_port_mc.py`: the boot check
  (empty card: flags 0x202, nothing created; no card), LOAD DATA on an empty card (error state 6, message 0xB, 'no
  save data'), and the save through the menus by `TTT_INPUT_PLAY` (AUTO SAVE on, YES): the five files appear, 0x480
  bytes of save data, no temporary file; a second run loads the save at boot (the AUTO SAVE byte `D_00439C92` is 1,
  0 after an empty card's boot).
- **Texts (T33 batch 1).** The message windows' text and the option menus' help line (`Text_DrawId`) were empty: not
  a card problem but the command printer's m2c draft and an uninitialised width (gotchas 25, 26). The window was two
  rows high, not one, for the same reason. `tests/test_port_mc.py` checks the grey text pixels of both in a dump.

## VU1

`port/src/gs_vu1.c` on the micro-mode interpreter (shared with VU0, `vu0_micro.c`): `port/src/vu_fast.c`, predecoded,
with `port/src/vu_micro.c` (`Vu_RunRef`) as its reference ([Speed](#speed)). The game's original VU1 programs
(`src/vu1/*.vsm`, uploaded by the boot packet) run unchanged:

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
- **Arithmetic.** The EE float model's vector unit (`softfloat_ps2_inl.h`; `vu_fast.c` gets the same results from
  the SSE unit in round-toward-zero / flush-to-zero mode): add / sub / mul cut toward zero, div / sqrt cut. ERLENG = 1 / sqrt(x^2 + y^2 + z^2) in that order; EATANxy / xz / EATAN use the VU manual's
  series (pi/4 + 8 odd terms of t = (y - x) / (y + x), the constants PCSX2 uses) [I: the EFU's internal order].
- **Instructions.** Every one the game's VU0 and VU1 programs use (listing survey: the upper set of `mesh.vsm`...
  `sprites.vsm`, lower `xgkick`, `xtop`, `eatanxy` / `eatanxz`, `erleng`, `mfp`, `waitp`, ...) plus XITOP, ESADD,
  ERSADD, ELENG, ESUM, ESQRT, ERSQRT, ERCPR, EATAN. Anything else is counted and reported in the `TTT_FRAMES` line
  `ttt: vu1: ... N not implemented` (0 on the attract path).
- **XGKICK.** The GIF packet at the given quadword of VU1 memory, tag by tag up to EOP (read across the end of
  memory), goes to `Gs_Gif(1, ...)` at once.
- **Cost.** About 1.35 million program runs and 2.8 billion VU instructions in 3000 VBlanks of the attract path
  (`mesh.vsm`, entry 0x050, is almost all of it). See [Speed](#speed) for the run times since T30.
- The reference project's interpreter computes with the host's float unit, so its structure and decoding were
  reused, not its code.

## Draft divergence check

Every rendering bug of T29 came from C that did not do what the asm does: a draft (m2c or typed, never compared) or a
port replacement. `port/src/ee_diff.c` compares such C with the original directly:

- **The interpreter.** User-mode EE code as the game's functions use it: the integer core on 64-bit registers, `lq` /
  `sq`, the MMI instructions met so far (`pextlw`, `pextuw`, `pcpyld`, `pcpyud`, `por`, `pand`, `mult1`...), the FPU on
  the EE float model (`softfloat_ps2_inl.h`, the arithmetic the port's C gets), COP2 macro mode (libvu0, the
  hand-written VU0 routines) on the vector unit's arithmetic. Calls are interpreted too: the whole call tree is the
  original's. Instructions and `.text` / `.rodata` / `.lit4` come from `build/SLUS_200.01`, else the disc's
  `SLUS_200.01` (`TTT_DIFF_ELF`), the data from the running program (the game's data sits at its PS2 addresses). Anything else (an
  unknown instruction, a jump outside `.text`, a syscall, the VU0 flag registers) stops the check with a report. No
  host float.
- **The hook.** `PORT_DIFF(addr, fn, sig, args...)` (`port_compat.h`, under `#ifdef PORT` in the game sources) calls fn
  as before; with `TTT_DIFF` it snapshots EE RAM and the scratchpad, runs the C, restores the snapshot, runs the
  original from `addr` on its own stack, compares, and leaves the C's result in place. Only the port's stack frames
  (from the end of `.bss` up) are left out: the game's `.bss` reaches 0x01FCDEF0, close to the stack. Hooks today:
  `func_003478F0` (the whole fighter skeleton, `skeleton.c`) and the calls of its draft (`func_003DD918`,
  `func_003DE0D8`, `func_00347E28`, `func_00351E98`, `func_0034D258`, `func_003DE4E0`, `func_00351AA0`), and
  `func_0034CC38` / `func_0034C300` inside `func_0034D258`.
- **Result (VBlank 1600, first fighter):** all of them agree with the original after the `func_003DD918` fix
  (`tests/test_port_ee.py`, slow test, `TTT_SLOW_TESTS=1`). The scratch word `func_0034C300` left different in its
  CamVol view is gone since T34 batch 2: the drafts `func_0034B4E8` / `func_0034BDD8` stored `sqrt` back over the
  squared length / the discriminant the original keeps there (gotcha 24). `TTT_SELFTEST=ee`
  found two libm differences in `third_party/newlib_libm`, which has newer fdlibm constants than the executable's
  newlib: `ef_asin.c` used `pio2_hi` 0x3FC90FDB / `pio2_lo` -4.37e-8 where the executable has 0x3FC90FDA / 0x33A22168
  (asinf off by up to 2 units in the last place for |x| >= 0.5; `ef_acos.c` already had the old ones), and atan2f's
  |y / x| > 2^26 result was one unit off. T30 batch 3 (F03) fixed both: `ef_asin.c` has the executable's constants,
  and the atan2f difference came from `sf_atan.c`, whose |x| >= 2^34 result `atanhi[3] + atanlo[3]` gcc folded at
  compile time with round-to-nearest (0x3FC90FDB) where the executable adds at run time with the EE's rounding toward
  zero (0x3FC90FDA): the sum now reads `atanhi[3]` through a volatile. The self-test has no tolerated difference left.
- **What the original cannot do here** (T34 batch 2). `FlushCache` (syscall 0x64) does nothing, as the port's;
  `bc0f` / `bc0t` see the DMA done; `ei` / `di` do nothing. The calls that talk to the IOP (`sceSifCallRpc`,
  `sceSifCheckStatRpc`, `sceSdRemote`) or kick and wait for the DMA hardware (the game functions with `EE_HW_POLL` in
  their C: `Dma_Wait`, `Dma_SendChain`, `Dma_SendNormal`, `func_00343178`, `func_0035D6E0`, `func_0035D738`,
  `func_0035E6B8`, `func_0035E758`, `Gfx_WaitVSync`) go to the port's own functions with the same arguments (a0-a3,
  t0-t3, then the stack), so a sound effect or a load no longer stops the check. Their host-side state sees the call
  twice (both runs): a sound request is queued twice, a transfer runs twice. A function pointer the port's C stored in
  game memory is a host address: a call through it goes to the original function of the same name (`func_XXXXXXXX`,
  or the name's address in `build/SLUS_200.01.elf`), and a word where the C has the host address and the original
  the PS2 one counts as equal. `.rodata` is read from the running memory where the port keeps the object at its
  address (`build/port/gen/image.txt`): the game writes some of it (the move table `D_00531EA0` gets its motion
  pointers when a character loads); the rest of `.rodata` (the jump tables) still comes from the executable.
- **Tools.** `TTT_DIFF_TRACE=<depth>` prints the original's calls to that depth (`ee-call`, indented; 1 = the hooked
  function's own calls); `TTT_DIFF_WATCH=<hex addr>` prints every store of the original code to that word (pc, ra);
  a load or store outside memory names the address and `ra`. With gdb on the C side (a hardware watchpoint on the same
  word, from a breakpoint on the hooked function at the VBlank: `break Battle_UpdateFrame if sVBlanks == 1032`) the
  two writers are found in a minute.
- **The path to the fight (T34 batch 2).** Hooks: `Mode_MainMenu`, `Mode_Battle` and `Mode_CharSelect`
  (`Mode_Dispatch`, `main.c`), `func_003CA470` (both sides), `Battle_UpdateFrame` in `Mode_Battle`'s sub-state 8, and
  `Skel_Update` through `func_0034E8B0` (one fighter's frame). With the inputs of `tests/test_port_input.py`
  (ARCADE, two characters, the fight with a walk, punches and a kick), every frame from the main menu (VBlank 177) to
  the end of the check (1449) agrees with the original except for these known differences:
  - `gGfx.field` / `vsyncCount` (`0x8BD2F0`-`0x8BD2F8`) in frames that load: the VBlank interrupt counts on while
    the C runs a load, not while the original is interpreted;
  - the game's own uninitialised locals, garbage on both sides: the preview fighters' joint rows `m2` (`func_003564C8`
    passes its stack `m` to `func_00349C50` as the screen matrix; VBlanks 342, 618, 710, 801), the camera key words
    3, 4 and 8 that `func_0035B690` never writes (`func_003D2818` copies them to `D_00415808`), and the fourth
    components of a hit flash (`Light_StartFlash`);
  - asinf's last bits (the libm constants above) in the fighters' turned upper bodies (`func_0034A0D0` of
    `geometry_b.c`, VBlanks 815, 1163-1165, 1384);
  - the sign of a zero in one joint matrix (VBlanks 620, 803): the hand-written `func_00349D70` multiplies the vf9 the
    previous VU0 code left, and the port's libvu0 (C on its own registers) leaves another one.
- **Limits.** The check covers the hooked calls in the frames asked for (a path not taken there is not covered).
  About 0.3 s per checked call (two 32 MB copies); a whole fight frame (about 440,000 instructions) takes about 1 s;
  the stage load of VBlank 617 (256 million instructions, the wind generator's warm-up) about two minutes.

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
- `movie.c` draft `func_0039C878`: `Port_MoviePlayable()` before the movie, then the PC player `Port_MoviePlay` in
  place of `Movie_Open` / `func_0039CAC8` / `Movie_Close` (threads, IPU and VBlank interrupt), and `Port_GifSync`
  after each GS kick (gotcha 27) ([movies.md](movies.md)).
- `menu_bg.c` `Text_LineCount`: its width starts at the text id, as the original's register does (gotcha 26).
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
   `undefined_syms_auto.txt`. Here they are placed by address (`symbols.ld`): since T25 every data name is.
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
19. **m2c locals that are fields.** The m2c draft of `demo.c` `func_003DD918` (the per-frame pose of bones 7-0x1C of
    every fighter, head and face among them, from the record `func_00364298` returns) blended each bone's translation
    towards `sp30`, a local of its own, where the asm reads `sp + 0x30`: the translation row of the scratch matrix at
    `sp`. The head's bones got garbage offsets (0, 1.3e-38, 1.57...) and the face and hair vertices skinned to them shot
    off: the last spikes (`gs-adc` z 0xFFFFAA). The same draft wrote only x of the scale vector it keeps at
    `f + 0xB0D0` and handed four separate locals to `sceVu0MulVector` as one vector (bones 13 / 14: the odd scales of
    joints 11-14 seen in T29 batch 3), and bone 10 reads its translation at record + 0x0C, not 0x12. Rewritten after the
    asm (typed, `#if 0`); `TTT_DIFF` showed 75 differing words in that call before, none after.

20. **The state hash covers host code addresses** (until T27: the hash now counts every host address as one
    constant, [determinism.md](determinism.md)). The game keeps function pointers in `.sbss` (callbacks), and the
    `TTT_FRAMES` state hash covers that memory. A port change that adds a host library function to the PLT (T30: the
    first `pthread_create`) moves every game function by 16 bytes per entry and changes the hash of an unchanged run.
    Host functions new since T30 are declared `noplt` in `port/src/port_host.h` (called through the GOT); the PLT
    stays the one of T29 and `tests/test_port_window.py` checks it. Changes to the port's own code do not move game
    code (the game's objects are linked first), with one exception: gcc puts constructors (`.text.startup`) and the
    cold parts of functions (`.cold`, `.text.unlikely`) in sections the linker places before every `.text`, so a new
    `__attribute__((constructor))` in `port/src` moved the game (T34: the pad self-test runs from `__wrap_main`
    instead). Changing a game draft moves the game functions after it and so changes the hash too, with the same
    frames: T34's two `select_results_b.c` fixes give `sbss-hash 0x341e8d2f` for the 2551-VBlank run, and that memory
    with the two moved pointers (`func_003D9588`, `func_003DA138`) set back to their old addresses hashes to
    `0x8dd3eebb` again.

21. **Draft calls through a cast take any argument count.** m2c drafts call functions through
    `#define f ((s32 (*)())f)`: `func_003CA470` called the 7-argument `func_003C9AC0` with 4, so it wrote through a
    garbage `moved` pointer (crash on entering the character select). The asm passes a0-a3 and t0-t2: the state block,
    the side, the button, the previous button, `&moved`, the side number and the word at `state + 8`. And a typed
    table must have the size of its entries: `func_003C68F0` read its 0x20-byte font table as `SelState[]`, so kind 11
    got the bytes 0x58 further (the hex digits next to it) and `Text_PutGlyph` divided by a zero column count
    (SIGFPE on "FREE PLAY").

22. **m2c drafts that guess the size of an access.** The m2c draft of `func_003CA470` (the character select's step
    per side, 1292 lines of asm) wrote every access as a byte or a word at an offset: it read the pad words
    `D_00415728` / `D_00415738` as bytes (the d-pad bits 0x1000-0x8000 never arrived: the cursor never moved), the
    member counts `D_005F62A8[side]` (s16) as bytes and the game type (a byte) as a word, and it called `Text_Printf`
    with two arguments short. Written again from the asm with the game's types (`SelState`, `SelSide`, `GameSetup`, the
    member slots of `D_00440C28` as `{chr, cos, s16 id}`); `TTT_DIFF` then finds the select screen equal to the
    original frame by frame.
23. **Callees that read a register the caller left.** Like gotcha 5, found on the way to the fight: matched C
    `Skel_Update` calls `func_0034E018()` (the per-side wind) with no argument, but its `$a0` is still `f` and the
    callee reads it; `draw_list.c`'s draft `func_00357160` calls `func_00340398()` with `id` still in `$a0`;
    `stage.c`'s draft `func_00395718` passes `a0` only to `func_0035BB58`, which reads `$a1` too; and the draft
    `func_0035C850` called `func_003AE3C0()` with no fighter (SIGSEGV when the fight starts). The port passes them.
    A scan of every `src/*.c` declaration against the definitions' parameter counts lists the rest (callers that
    pass fewer arguments than the callee reads): `func_003DC188` (`demo.c`), `Ui_DrawWindow` / `func_003DBE98`
    (`theater.c`, `theater_b.c`), off the path to the fight.
24. **m2c drafts that change the order or the home of a value.** The CPU's decider `func_003865A0` (`fighter_core.c`)
    stepped the seed word `D_005F6C08` before adding it to `rand()`; the asm adds the old seed and stores the new one
    after (`(rand() + seed) & 0x7FFF`, then `seed = seed * 5 + 3`). From the first frame of the fight on, the CPU took
    other decisions (and more random numbers) than the original. The camera-volume drafts `func_0034B4E8` /
    `func_0034BDD8` (`geometry_c.c`) stored a `sqrt` back over the value it was taken from, where the original keeps
    it in a register: `unk274` must stay the squared distance, which the cylinder test compares with squared
    distances later.
25. **m2c drafts of variadic functions read nothing.** The command printer `func_003DFA00` (`demo.c`: position,
    colour, width and strings as `0xFF` commands, the start of every `Text_DrawId`) was an m2c draft that never read
    its variadic arguments and stored uninitialised locals as the text position and colour: the memory card
    messages and the option menus' help line were drawn off screen, in colour 0. Typed from the asm (T33 batch 1),
    reading its arguments as 4-byte i386 stack slots like `Text_Printf` (`DFA_ARG`). `TTT_DIFF` on the new
    `Mode_Options` hook (`main.c`) found it (`D_005F7060` 0 against 0x0186001E) and now finds VBlanks 500-1000 of
    the AUTO SAVE flow equal to the original code.
26. **Matched C that reads an uninitialised variable.** `Text_LineCount` (`menu_bg.c`, matching) adds glyph widths to
    a `w` it never sets: the original's `w` is `s0`, which its only caller `func_00360A30` (`font_tex.c`) holds as
    the text id it passes. A host garbage `w` made `w / 580` one more line, and the message window two rows high.
    Under `PORT` it starts as the id.
27. **Two GS kicks with no wait between.** The port's `sceGsSyncPath` returns at once and a DMA transfer happens at
    the next poll, so a second kick of the GIF channel before a poll replaced the first: `func_0039D320`'s GS set-up
    packet (FRAME, TEST, SCISSOR of the movie's frame buffers) was never sent, and the movie's sprites drew nothing.
    `Port_GifSync` (`plat_hw.c`) carries a started GIF transfer out at once, without moving the clock; the movie
    code calls it after each kick. With `TTT_MOVIES=0` the set-up packet is now sent too: the dumps and the state do
    not change.
28. **Stale stack words carry the host into the game's state.** The game reads uninitialised locals (the camera key of
    `func_003D2818`, `Light_StartFlash`'s colour): on the PS2 the stale words are the same every run; here they were
    whatever the port's code had left on the EE stack, the GS threads' waiting times and the C library's addresses
    among them, so the fight's state differed from run to run with raster threads. The port's work that can wait or
    calls into the C library now runs off the EE stack (`Port_OffEe`, `plat_state.c`), and both reads start from 0
    under `PORT` ([determinism.md](determinism.md)). `TTT_SYNCTEST` (whole states, stack included) finds such cases.

## EE float model

- **Arithmetic** (`port/src/softfloat_ps2_inl.h`, `softfloat_ps2.c`; the hardware-level model of the reference
  project): no denormals (exponent 0 is zero), no infinity / NaN (exponent 255 is an ordinary number, results clamp
  to +/-0x7F7FFFFF), add / sub / mul cut toward zero, the FPU's add.s keeps one guard bit of the smaller operand,
  div.s and sqrt.s round to nearest, the vector unit's vdiv / vsqrt cut; x / 0 gives +/-max; comparisons see
  denormals as zero. `sqrtf` is `sqrt.s` (ee-gcc inlines it; its errno fallback never runs on the EE).
- **Vector unit**: `port/src/plat_vu0.c` and `vu0_pc.c` are VU0 macro code on the PS2; the build renames their
  `__addsf3` / `__subsf3` / `__mulsf3` / `__divsf3` / `sqrtf` calls to `Port_VuAdd`... (`toolchain.VU_FILES`,
  objcopy), so C written with floats gets the vector unit's results. `vu_micro.c` / `vu_fast.c` run the VU0 and VU1
  microcode on the same arithmetic. No host floating point, with one exception: `vu_fast.c` computes the vector
  unit's model on the SSE unit in a mode where its results are the model's, exactly ([Speed](#speed)), checked
  against the integer model (`TTT_SELFTEST=vu1`, `TTT_VU_CHECK`). `tests/test_port_float.py` checks the program's
  code for x87 float instructions, and for SSE float instructions outside `toolchain.SIMD_FILES`.
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

The fighters' heads and hair are clean since T29 batch 5 (gotcha 19). The draft divergence check covers the skeleton
in one frame; hooks on the rest of the fight's path (the fighter draw from `func_00357640`, the stage, the effects,
more frames, both fighters) are the systematic way on. More speed in fights: the VU1 thread and the game thread's VU0
skinning are the limits now ([Speed](#speed)); T31's native replacements. libgraph
(`plat_lib.c`): the DBuff buffer pairing and SwapDBuff's ids differ from the library with the same GS result. The
seven Tekken Bowl m2c drafts around VU0 code (`portsrc.REPLACED`, still placeholders: not reached by the attract
demo); the libm PCSX2 run; the other game types after the character select (VS, team, survival: the same step
function, not checked frame by frame) and the leftover-register calls of gotcha 23 off the fight's path; the multitap,
vibration and gamepad rebinding; the open movie items of [movies.md](movies.md) (the attract and ending movies through the
game's own paths, the driver's movie timing).
