# PC port: determinism, state hash, save / load, synctest

T27. The base of rollback netplay (T45): one number that says whether two runs are in the same state, a copy of the
whole simulation that can be put back at a VBlank, and a test mode that does that every few VBlanks and compares.
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
| `TTT_SELFTEST=state` | The hash's host-address rule and an in-process save / load round trip. |

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
memory), while the game has a memory card file open, and with the GL back end (below).

### What is in a state

| Region | Where | Hashed |
|---|---|---|
| EE RAM, 32 MB (data, bss, fixed buffers, the EE stack) | `plat_state.c` | yes, without the EE stack and the program's ELF header |
| Scratchpad, EE hardware registers (timers, DMA, VIF, GIF, INTC...) | `plat_state.c` | yes |
| VU0 memory / VU1 memory | `plat_state.c` | VU0 yes, VU1 no |
| GS privileged registers | `plat_state.c` | yes, CSR only by FIELD / VSINT, SIGLBLID not |
| Clock, VBlank count, timers' fractions, INTC / DMAC handlers, masks and pending causes; the report's modes | `plat_hw.c` | yes (report: no) |
| COP0 Status (interrupts on), VU0 registers (vf, vi, ACC, Q), VIF0, VU0's pipeline | `plat_ee.c`, `vu0_micro.c` | yes |
| VU1 registers and pipeline, VIF1, the GS front end (registers, GIF paths, CLUT state), GS memory (4 MB), the CLUT buffer, the GS threads' pending CSR bits | `gs_vu1.c`, `gs_vif1.c`, `gs_core.c`, `gs_async.c` | no (the GS threads' side: the game never reads it back) |
| SPU2 model (cores, 2 MB sound RAM), libspu2 core, BGM timer phase, BGM streamer | `spu2.c`, `plat_snd.c`, `plat_bgm.c` | yes |
| SPU2 interpolation tail (each voice's previous block's samples 25 and 26, F06) | `spu2.c` | no (only what is heard depends on it; [sound.md](sound.md) section 2) |
| RSPU2DRV status, movie stream arguments | `plat_file.c` | yes |
| libgraph modes; kernel threads, semaphores, modules, SIF handlers | `plat_lib.c`, `plat_sys.c` | yes |
| The pads as latched at the last VBlank | `plat_pad.c` | yes |
| libmc: the request in flight, each card's directory state | `plat_mc.c` | yes |

40,029,888 bytes (`TTT_SELFTEST=state`). Not saved, drained or rebuilt instead: the GS threads' queues and the raster workers (`Gs_Finish` at
the save or load: they are idle there), the predecoded VU programs (each slot is checked against micro memory when it
runs), the GL back end, the window, the audio ring, the WAV file, open files. Things that come from the switches stay
the loading process's: `TTT_FRAMES`, the replay file (its position follows the loaded VBlank count), `TTT_SAVES`.

### Times

Measured with `TTT_SYNCTEST` (buffers warm): a save copies the 40 MB in about 3 ms, a load in about 3 ms. Before that,
a save or a load waits for the GS threads to finish what is queued: nothing with `TTT_GS_THREADS=0 TTT_GS_ASYNC=0`,
up to seconds headless with the raster threads (the game thread runs that far ahead of the software rasteriser); the
lines say how much of the time that wait was. A save to a file adds the file. Rollback will want less copying (page
tracking of what changed, as the reference project does) and the GL back end's support.

## The gameplay hash

XXH3-64 (`third_party/xxhash`, BSD-2, scalar code, no new PLT entry) over every region marked hashed above, each region
hashed alone and the region hashes hashed together.

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

Values (sw, `TTT_SEED=1 TTT_MOVIES=0`, empty card): the 2551-VBlank idle run ends with `hash 0xd36c82e9a145875a`
(17 frames unchanged), the scripted fight of `tests/test_port_input.py` (1450 VBlanks) with `0xaeed4a40f6029792`.

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

- **GL back end.** GS memory is partly on the GPU only (resolved targets), so saves are refused with
  `TTT_RENDERER=gl` (the hash works). Loading needs the mirror pushed again and the GPU-newer pages read back first.
- **Speed for rollback.** 40 MB per save; the GS drain before each save takes as long as the GS threads lag.
- **Uninitialised reads.** Stale stack words are now the same in every run of a build, but they can still hold
  build-dependent values (a frame layout, a code address): the cross-build check (a padding build, `TTT_HASH_DUMP`)
  found the two above; other paths of the game (menus, other modes) are not checked yet.
- **Real time.** The real-time clock is moved to the loaded cycle count after a load; rollback with live input (the
  window's pads) is T45.
