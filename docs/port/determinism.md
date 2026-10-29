# PC port: determinism, state hash, save / load, synctest

T27. The base of rollback netplay (T45): one number that says whether two runs are in the same state, a copy of the
whole simulation that can be put back at a VBlank, and a test mode that does that every few VBlanks and compares.
F07 added saves with the GL back end, page tracking (a save copies what changed) and the rollback test.
Code: `port/src/plat_state.c` (and `plat_state.h`), with each module registering its own part of the state.
Tests: `tests/test_port_state.py`.

## Switches

| Variable | Effect |
|---|---|
| `TTT_HASH_EVERY=<n>` | Print `ttt: hash <vblank> <16 hex digits>` at every n-th VBlank boundary. |
| `TTT_SAVESTATE=<vblank>:<file>` | Save the state at that VBlank boundary into the file, and go on. |
| `TTT_LOADSTATE=<file>` | Load the file at the first poll, and go on from the VBlank it was saved at. Refused (exit code 2) if it is not a state of this build. |
| `TTT_SYNCTEST=<n>` | At a boundary save a checkpoint, run n VBlanks, save, load the checkpoint, run the same n VBlanks again, save, compare the two saves; then the next checkpoint. The report adds a `ttt: synctest:` line (checks, divergences, sizes and times); a divergence prints the region and the first differing word (with the game's data label for EE RAM). |
| `TTT_SYNCTEST_FROM=<vblank>` | Start the synctest there (default: the first boundary). |
| `TTT_HASH_REGIONS=1` | With `TTT_FRAMES`: each hashed region's own hash at the end (to see where two runs part). |
| `TTT_HASH_DUMP=<file>` | With `TTT_FRAMES`: EE RAM as the hash sees it, at the end (to compare two builds word by word). |
| `TTT_STATE_STACK=1` | With `TTT_FRAMES`: the EE stack words that look like this process's heap or C library (there should be none). |
| `TTT_SELFTEST=state` | The hash's host-address rule, an in-process save / load round trip, and page tracking's saves and loads against whole copies. |
| `TTT_STATE_TRACK=0` / `1` | Page tracking (F07, below): on by default for the synctest and the rollback test, `0` turns it off (whole copies), `1` turns it on for anything that saves. |
| `TTT_STATE_TRACK_CHECK=1` | Every save into and load from a tracked buffer is compared with what a whole copy gives; the `ttt: state-track:` line counts them and the wrong ones. |
| `TTT_ROLLBACK=<n>` | The rollback test (F07, below): at every VBlank, the state of n VBlanks before is loaded and those n VBlanks run again (n up to 16). The report adds a `ttt: rollback:` line (times per VBlank against 16.68 ms). |
| `TTT_ROLLBACK_FROM=<vblank>` | Start it there (default: the first boundary). |
| `TTT_ROLLBACK_CHECK=1` | Compare the state each rollback comes back to with the one saved the first time. |
| `TTT_ROLLBACK_DRAW=0` | The frames run again are not drawn: their DMA into the GS path and their pictures are dropped, and the states leave out the GS's side (VIF1, VU1, the GS front end, GS memory), so nothing waits for the GS threads. |

State files hold the game's memory: keep them out of the repository (put them under `build/` or outside).

The `TTT_FRAMES` report's `ttt: state:` line now ends with `hash 0x<16 hex digits>` (the gameplay hash below) instead
of the old `sbss-hash` (an FNV-1a of the first 4 KB of `.sbss`, which held two host code addresses, F04).

## The state boundary

The game's code runs natively on the EE stack (`plat_main.c`), so its registers are the host CPU's and its call stack
is a native one. The boundary is the first top-level poll after a VBlank (`plat_hw.c`: after the VBlank's handlers ran,
outside interrupt handlers). There `Port_StateBoundary` is a stack switch: it pushes the callee-saved registers on the
EE stack, keeps the stack pointer and runs the rest on a service stack. A save copies the regions and that stack pointer;
a load copies them back and sets the stack pointer, so that the switch returns into the boundary the state was saved
at, with its frames (the EE stack is in EE RAM, part of the state). A state holds no host heap or library address, only
addresses of the program's own code and variables, so a file loads into a fresh process of the same build (a build id,
the program's code hashed with the region list, is checked).

The in-process API for rollback (`plat_state.h`): code that runs inside the boundary calls `Port_StateSave(buf)` (copy
now) and `Port_StateLoad(buf)` (put back; when the boundary returns, the game goes on from the saved boundary).
`Port_StateSize()` gives the buffer size. A save is refused (and says why) while a movie plays (its decoder is host
memory) and while the game has a memory card file open. With page tracking on (`Port_StateTrack(1)`, below), a save
into a buffer that holds an earlier save of this process copies only the pages written since, and a load from one
only those pages: the caller keeps saved buffers unchanged (copying a whole buffer is fine). `gPortStateNoGs` leaves
the GS's side out of saves and loads (below).

### What is in a state

| Region | Where | Hashed |
|---|---|---|
| EE RAM, 32 MB (data, bss, fixed buffers, the EE stack) | `plat_state.c` | yes, without the EE stack and the program's ELF header |
| Scratchpad, EE hardware registers (timers, DMA, VIF, GIF, INTC...) | `plat_state.c` | yes |
| VU0 memory / VU1 memory | `plat_state.c` | VU0 yes, VU1 no |
| GS privileged registers | `plat_state.c` | yes, CSR only by FIELD / VSINT, SIGLBLID not |
| Clock, VBlank count, timers' fractions, INTC / DMAC handlers, masks and pending causes; the report's modes | `plat_hw.c` | yes (report: no) |
| COP0 Status (interrupts on), VU0 registers (vf, vi, ACC, Q), VIF0, VU0's pipeline | `plat_ee.c`, `vu0_micro.c` | yes |
| VU1 registers and pipeline, VU1 memory, VIF1, the GS front end (registers, GIF paths, CLUT state), GS memory (4 MB), the CLUT buffer, the GS threads' pending CSR bits | `gs_vu1.c`, `gs_vif1.c`, `gs_core.c`, `gs_async.c` | no (the GS threads' side, `PS_GSSIDE`: the game never reads it back) |
| SPU2 model (cores, 2 MB sound RAM), libspu2 core, BGM timer phase, BGM streamer | `spu2.c`, `plat_snd.c`, `plat_bgm.c` | yes |
| SPU2 interpolation tail (each voice's previous block's samples 25 and 26, F06) | `spu2.c` | no (only what is heard depends on it; [sound.md](sound.md) section 2) |
| RSPU2DRV status, movie stream arguments | `plat_file.c` | yes |
| libgraph modes; kernel threads, semaphores, modules, SIF handlers | `plat_lib.c`, `plat_sys.c` | yes |
| The pads as latched at the last VBlank | `plat_pad.c` | yes |
| libmc: the request in flight, each card's directory state | `plat_mc.c` | yes |

40,029,888 bytes (`TTT_SELFTEST=state`). Not saved, drained or rebuilt instead: the GS threads' queues and the raster workers (`Gs_Finish` at
the save or load: they are idle there), the predecoded VU programs (each slot is checked against micro memory when it
runs), the GL back end's targets and caches (below), the window, the audio ring, the WAV file, open files. Things that
come from the switches stay the loading process's: `TTT_FRAMES`, the replay file (its position follows the loaded
VBlank count), `TTT_SAVES`.

### With the GL back end (F07)

GS memory is gs_core.c's 4 MB on the CPU, mirrored on the GPU, where the render targets draw; what a target drew
reaches the CPU copy only when something needs it there ([video.md](video.md)). A save therefore first queues a
`CMD_STATE` for the window thread and waits for it (`gs_core.c` `gs_state_before`, `gs_gl.c` `state_cmd`): every target
is written back into the mirror and dropped, the pages the GPU wrote are read back, the CLUT halves only the GPU had
are read back from its CLUT row, and the CLUT buffer goes into the state. After a load the window thread drops its
targets without writing them back, takes the loaded CLUT buffer and pushes all of GS memory into the mirror again.
Both end in the same place: no target, the mirror equal to the CPU copy, nothing cached that depends on the past
(`state_reset`). So a run that saved and a run that loaded the same state send the same commands to the same GPU state
and draw the same: the synctest compares GS memory too and finds no difference, and pictures after a load in a fresh
process are byte for byte those of the run that saved, which are those of a run that never saved. What a dropped target
loses is only what GS memory does not hold (its resolution above 1x, colour values above 255, the low bits of a 16-bit
frame); the next frames draw it again. A load at the first poll of a fresh process starts the back end first (no
command has been sent yet), so the loaded memory is pushed before anything is drawn. Video options that keep target
snapshots between frames (internal resolution above 1x, widescreen, flicker filter off, low latency) and the window's
skipped pictures are not covered by these checks.

### Page tracking (F07)

`Port_StateTrack(1)` (on by default for `TTT_SYNCTEST` and `TTT_ROLLBACK`) has the kernel write-protect the whole
pages of every region of 64 KB or more: EE RAM through all three of its views, GS memory, sound RAM, the EE hardware
registers; 9,743 pages, 38 MB. It uses `userfaultfd` in asynchronous write-protect mode (Linux 6.7, unprivileged
`UFFD_USER_MODE_ONLY`): a write to a protected page is let through by the kernel itself, with no signal, nothing on
any stack (the EE stack and the crash handler are not involved), and writes by the kernel (system calls) and by other
threads (the GS threads, GL read backs) count too. At each save and load one `PAGEMAP_SCAN` per range lists the pages
written since the last scan and protects them again; each scan is an epoch and every page keeps the epoch it was last
seen written in. A buffer saved with tracking on carries its tracking session and epoch in the header; the memory can
differ from it only in pages written in a later epoch (a load's own copies are writes too), so a save into it copies
those pages, and a load from it copies them back. The parts of regions outside whole pages and the small regions are
copied every time. A state written to a file has no session (`token` 0) and loads whole. Where the kernel does not
offer it the switch stays off (`ttt: state-track: <why>`), and every save is a whole copy as before.

`TTT_STATE_TRACK_CHECK=1` compares every save into and load from a tracked buffer with the memory (what a whole copy
gives): no difference in the attract synctest, the fight's synctest (software and GL: GS memory read back from the GPU
included) and the rollback tests. The idle and fight hashes are unchanged: nothing runs unless a state is taken.

### Leaving the GS's side out

The GS threads' regions (`PS_GSSIDE`) are the part of the state a save waits for: `Gs_Finish` waits until the GS
threads, which run behind the game, have done everything queued (headless with the raster threads that is up to the
software rasteriser's lag; with GL the window thread's). With `gPortStateNoGs` set, saves and loads skip those regions
and the GS's hooks, so nothing waits; a state saved so is marked and loading it never touches that side. The game reads
nothing from the GS but CSR's FIELD (game side), so its state is the same. This is what `TTT_ROLLBACK_DRAW=0` uses:
the frames run again are not drawn at all (`gPortNoGs`: `plat_hw.c` drops their DMA into VIF1 and the GIF and their
VBlank picture), the GS only ever sees each frame once, and its side never needs to go back.

### The rollback test

`TTT_ROLLBACK=<n>` is rollback netplay's steady worst case (every peer's inputs arriving n VBlanks late at every
VBlank): at each VBlank a save into a ring of n + 1 buffers, the buffer of n VBlanks before loaded, those VBlanks run
again (`gPortResim`) with a save at each, then the game goes on. With `TTT_ROLLBACK_CHECK=1` the state each rollback
comes back to is compared with the one saved the first time. The scripted fight with rollbacks of 8 from VBlank 1100
or 1150 (software and GL, drawn or not) never diverges and ends with the fight's hash.

### Times

Headless, the scripted fight's synctest (`TTT_SYNCTEST=1` from VBlank 850 to 1250) and rollback test (8 VBlanks from
1150 to 1300), means per save / load besides the wait for the GS threads:

| | Whole copies | Page tracking |
|---|---|---|
| Synctest, software, `TTT_GS_THREADS=0 TTT_GS_ASYNC=0` | 2.4 / 2.5 ms | 0.35 / 0.69 ms |
| Synctest, GL | 2.8 / 2.7 ms (6 ms waiting for the window thread) | 0.49 / 0.77 ms |
| Attract synctest (`TTT_SYNCTEST=2`, VBlanks 650-800), software | 3.4 / 3.3 ms | 0.35 / 0.50 ms |
| Rollback 8, software, drawn | 3.6 / 3.0 ms | 0.60 / 1.26 ms |
| Rollback 8, GL, drawn | | 1.0 / 1.2 ms |
| Rollback 8, GL, undrawn | | 0.28 / 0.32 ms |

A scan takes 50-90 us (200 us with GL's read backs) and finds about 400 pages written per VBlank in the fight
undrawn, 1,000 drawn (GS memory's frame and depth buffers). Undrawn there is no wait at all; drawn, every save waits
for the GS threads (19 ms per save or load with GL, 35 ms with the software raster threads: they draw the frames run
again). Each write to a protected page costs a fault in the kernel (about 1.5 us), paid only between scans.

Budget: per VBlank, a rollback of n costs n frames of the game plus n + 1 saves and a load. The states now cost about
1 ms per VBlank for n = 1 and about 3 ms for n = 8 undrawn (whole copies: about 7 and 33 ms). What does not fit is the
game itself. Until T31 a fight VBlank run again undrawn took about 8.5-10 ms of the game thread's CPU (74 % of its
samples in the VU0 skinning programs' micro-mode interpreter, `vu_fast.c` `run_fast`), so a rollback of 8
(`TTT_ROLLBACK=8 TTT_ROLLBACK_DRAW=0`) took about 100 ms. T31 runs those programs as C (README, "Hand replacements"):

| Scripted fight, headless, software rasteriser, pinned to the P cores, 3 runs each (another build ran beside them) | before (0f522e0) | T31 |
|---|---|---|
| Rollback of 8 undrawn from VBlank 1150: the game thread's CPU per rollback | 72-87 ms | 31-38 ms |
| The same per frame run again | 9.0-10.8 ms | 3.9-4.7 ms (about 27 ms per rollback when the machine is idle) |
| The rollback itself (wall time) per VBlank | 87-176 ms | 41-44 ms |
| A drawn fight VBlank (VBlanks 1101-1451): the game thread's CPU | 11.1-14.1 ms | 6.3-7.9 ms |
| One skinned vertex (`TTT_SELFTEST=vu0`, skin_diffuse, two bones) | about 1500 cycles | about 300 cycles |

The game thread can now run about 250 undrawn fight frames a second (about 100 before), so a rollback of 3 to 4
frames fits next to the real frame; 8 still does not (about 30 ms). What is left of a frame run again: the C
skinning programs about 40 % of the game thread's samples, the EE side's vector-unit float model (`Port_VuMul`,
`Port_VuAdd`, `sf_add_core`, from libvu0's matrix code such as `sceVu0ApplyMatrix`) about 15 %, DMA (`dma_block`)
about 5 %, `memcpy` on the EE stack about 4 %. Drawn, the window thread would also have to draw n + 1 frames per
VBlank (it draws about 60-70 a second): the frames run again must stay undrawn.

## Netplay (T45)

[netplay.md](netplay.md) uses all of the above: a ring of saves with page tracking and the GS's side left out
(`gPortStateNoGs`), rollbacks run undrawn and unheard, the build id and the state hash at the first boundary in the
handshake. Its desync check uses `Port_StateFingerprint`: the hashed regions, those of 64 KB or more as the XXH3 of
their pages' own hashes, each page's hash kept until page tracking sees it written (1-2 ms instead of 11-21 ms);
another value than `Port_StateHash`, also a function of the state alone.

## The gameplay hash

XXH3-64 (`third_party/xxhash`, BSD-2, scalar code, no new PLT entry) over every region marked hashed above, each region
hashed alone and the region hashes hashed together, in the order the regions were registered: the order the port's
constructors run in (priority, then link order). A compiler that runs one of them at another place changes every
hash with the same memory (T42 b2: clang evaluated `plat_state.c`'s `state_memory` at compile time, so EE RAM came
first; `TTT_HASH_REGIONS` lists the regions in that order).

**Host addresses (F04).** The game keeps host code addresses in its memory: callbacks in its tables and object pools
(`.sbss` 0x5F6FD0 / 0x5F6FD4 = `func_003D9588` / `func_003DA138`, the effect pools at 0x76D518...) and values derived
from them. They move whenever the game's code changes size. A first try mapped each word that equals a function's
address to an id from the function's name: not exact. Game data (packed bytes such as 0x08101010, texture words)
holds about a hundred words per run that equal some function's address in one build and not in the next, and some
stored values are a code address plus an offset (one in `D_00550800`). So every word inside a fixed window
`[0x08000000, 0x09000000)`, which holds the whole program image (code and variables), hashes as one constant
(0xC0DEC0DE): the same in every build, for real pointers and for data that happens to fall there alike (about 0.3 % of
EE RAM's words). The window is fixed, not the program's own range, so that a word's class does not depend on the
program's size; the hash says so on stderr if the image ever grows past it. Also left out: the EE stack (return
addresses, frame layout), and the page in front of the game's data at 0x415000, where the loader maps the program's
ELF header (the first LOAD segment starts at file offset 0; the PS2 has the end of its code there).

Checked: a build with 4 KB of padding in the game's code (every game function moved) gives the same per-VBlank and
final hashes as the build without, over the idle run and the scripted fight; `TTT_SELFTEST=state` checks the rule on
the two callbacks. The hash does not depend on the GS thread modes (`TTT_GS_THREADS`, `TTT_GS_ASYNC`).

Values (sw, `TTT_SEED=1 TTT_MOVIES=0`, empty card): the 2551-VBlank idle run ends with `hash 0x61dc0b370ff7019b`
(17 frames unchanged), the scripted fight of `tests/test_port_input.py` (1451 VBlanks, software and GL) with
`0xa1623a6d44cb57a6`. Before T42 b1 they were `0xd36c82e9a145875a` and `0x3c2cd520128fc04d` (`0xaeed4a40f6029792` for
1450 VBlanks at T27): two values the game reads from uninitialised stack bytes (`dma_spr.c` `func_0035D790`'s DMA
tag and its stack address, `Light_StartFlash`'s fourth float) followed the compiler's frame layout; they are fixed
under `PORT` now, and the gcc and clang builds give the same hashes ([README.md](README.md) gotcha 32; since T42 b2
with every object of the clang build clang's). The x86-64 build (T42 b3, `TTT_CC=clang64`, the default since b5,
[README.md](README.md) 64-bit) gives both values too, with every region hash and picture alike: its program lies in
the same window, and the port's hashed state keeps game addresses as 4-byte words. Its synctest and rollback needed
the game's variadic calls padded to six arguments (a variadic entry stores every argument register on the EE stack,
unused ones with leftover host values; README.md 64-bit, Real use).

## Nondeterminism found and fixed

The synctest compares whole states, the EE stack included, so it finds anything that differs between two runs of the
same VBlanks from the same state.

1. **Stale stack words that depend on the host's timing.** The game reads uninitialised locals in places: the camera
   key of `func_003D2818` (`model_anim.c`) leaves words 3, 4 and 8 unset and `func_0035B858` copies all nine into
   `D_00415808`; `Light_StartFlash` (`scene3d_d.c`) copies a 4-word colour with an unset fourth word. Those words are
   whatever an earlier call left on the stack, and the port's waits on the GS threads (ring full, drains, a contended
   lock) left the host's timing there: the fight's state differed from run to run with raster threads (seen in
   `D_00415818`), never with `TTT_GS_THREADS=0`. Now the port's work that can wait runs off the EE stack
   (`Port_OffEe`, called on every path whether it waits or not): the DMA transfers into the GS path and the VBlank's
   port work (`plat_hw.c`), libgraph's GS calls (`plat_lib.c`). Both uninitialised reads are set to 0 under `PORT`
   (the matching build is unchanged).
2. **The C library's frames on the EE stack.** Disc reads (`sceSdRemote`, `sceSifCallRpc`), libmc's host side,
   `printf` / `sprintf`, `memcpy` / `memset` (glibc's call on inside themselves), lazy PLT binding, the draft divergence
   check's `getenv` per hooked call, and the registers `__libc_start_main` left when `main` starts all wrote addresses
   of the C library and of the heap into the EE stack: different in every process (ASLR), and so in a loaded state.
   Now: those calls run off the EE stack (`Port_OffEe`; `--wrap=printf,sprintf,memcpy,memset` in `link.py`, string
   instructions for `memcpy` / `memset` on the EE stack), the program is linked `-z now`, `ee_diff.c` reads its
   switches once at start-up, and `__wrap_main` clears the callee-saved registers. `TTT_STATE_STACK=1` finds none left
   after the idle run.
3. **The boundary's own footprint.** The boundary writes the same bytes on the EE stack in every run (the stack switch
   above), whether it saves, hashes or does nothing; a save made directly from C there would have left its own locals
   (buffer pointers) for the game to find.
4. **Stack protector.** The port's code is built with `-fno-stack-protector` (`toolchain.py`): its frames are on the EE
   stack at the boundary, and a fresh process has another canary.

Results (sw): `TTT_SYNCTEST=4` over the whole idle run and `TTT_SYNCTEST=1` over 600 VBlanks of the scripted fight
(VBlanks 850-1450), and `TTT_SYNCTEST=4` over the fight with raster threads: no divergence, and the same final state
as the runs without the synctest. A state saved at VBlank 700 (idle) or in the fight and loaded in a fresh process
continues with the same pictures, modes and state as the run that saved it.

## Game speed, volumes, cheats (T40)

- **Game speed** scales only the real-time clock (`plat_hw.c` `rt_scale`, out of line, called from the real-time
  branches of `Port_HwPoll` and the overlay's pause only). Headless runs never call it: the idle and GL fight hashes
  are unchanged, and so are real-time runs at 1x. Real-time runs were never hash-reproducible (the RNG spin follows
  the host's timing); at any speed the game runs the same frames on the same inputs. A rollback netplay session
  (T45) must keep every peer at 1x and ignore the fast-forward key.
- **Volumes** act on the mixed output after the SPU2 model ([sound.md](sound.md) section 4.1): no state changes.
- **Cheats** change the game's state on purpose: `cheats.unlock_all` writes the save block's unlock flags at each
  VBlank ([cheats.md](cheats.md)), so a run with it has another hash (the 300-VBlank idle run: 34 selectable
  characters instead of 20). Off (the default) nothing is written and every hash is as before. The write happens in
  the VBlank work, off the EE stack, so the check itself leaves no host bytes in the hashed memory.

## Open

- **The game's frame for rollback.** States cost well under a millisecond per save now (page tracking, the GS's side
  left out); a fight frame run again costs about 4 ms of the game thread since T31 (C skinning programs), so about 3-4
  frames of rollback fit per VBlank, not 8 (see Budget). Next: the C programs themselves (about 40 %, bound by their
  instruction count: the MAC flags' queue, the loads and the 32-bit build's eight vector registers) and the EE side's
  vector-unit float model (about 15 %). Rollback's frames run again must stay undrawn (`gPortNoGs`); T45 decides what
  a GL save does there (no GPU read back is needed when the GS's side is left out).
- **GL with video options.** The GL checks run at 1x without widescreen, with the flicker filter and without low
  latency; the snapshots those options keep between frames are dropped at a save but are not proved equal after it.
- **Older kernels.** Page tracking needs Linux 6.7 (asynchronous userfaultfd write protection and `PAGEMAP_SCAN`);
  before that every save is a whole copy.
- **Uninitialised reads.** Stale stack words are now the same in every run of a build, but they can still hold
  build-dependent values (a frame layout, a code address): the cross-build check (a padding build, `TTT_HASH_DUMP`)
  found the two above; other paths of the game (menus, other modes) are not checked yet.
- **Real time.** The real-time clock is moved to the loaded cycle count after a load. Rollback with live input is
  T45 ([netplay.md](netplay.md)): it runs on the deterministic clock, paced, not on the real-time one.
