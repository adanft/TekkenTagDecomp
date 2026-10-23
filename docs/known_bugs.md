# Known quirks of the original (keep, do not fix)

This file lists behaviour of Tekken Tag Tournament (SLUS-20001) that looks like a bug, an accident of the hardware or a
side effect of the code order, and that a native PC port must **keep**. "Keep" means the port produces the same
observable state as the original on the PS2: the same memory after the same frame, the same `rand()` stream, the same
CPU choices. A port that "fixes" one of these entries no longer matches PCSX2 and breaks recorded replays.

How the tasks use it:

- **T26** (headless boot and fight compared with PCSX2 memory dumps): every `[?]` entry is a check to run in PCSX2;
  every entry is a first suspect when a state comparison diverges.
- **T27** (per-frame state hash, save / restore, synctest): the entries of the RNG, timing and memory groups name
  state that must be hashed, saved and restored, or turned into an input.
- T22 (memory map), T23 (EE float model) and T24 (kernel, VBlank) implement what the memory, float and timing entries
  require.

Tags: `[V]` verified from the code (C or assembly read), `[I]` inferred, `[?]` needs a PCSX2 check in T26. Locations
are `src/file.c:line` at the time of writing (T20); the function name is the stable reference. Only behaviour is
described; no game data is reproduced.

## 1. Simulation and fight

### No stage collision: the floor is the plane y = 0

- **Where**: `src/battle_flow.c:4564` `Battle_UpdateMovement` (the snap at 4585-4591).
- **Original**: no wall, ring-out or floor height exists in the simulation. A fighter above the floor (`unk8 < 0`, y
  grows downwards) gains 6 of fall speed per frame and is snapped to y = 0 when it crosses it. The stage index is never
  read for gameplay ([systems/stage.md](systems/stage.md) section 1).
- **Port**: do not add stage geometry or collision; a port that "places" fighters on a stage mesh changes positions.
- **Status**: `[V]` code; `[I]` that no asm-only module reads stage geometry.

### The fight area is infinite and re-centred at ±0x78000

- **Where**: `src/battle_flow.c:6003` `func_00372E98`.
- **Original**: when either fighter's x or z leaves -0x78000..0x78000, both fighters are moved so that the first one
  stands at 0, the shift is stored in both fighters' `unk10` / `unk14`, and the camera is reset (`Cam_Reset`). The
  test is skipped (shift cleared) while the camera holds (`func_0038E2D0`).
- **Port**: keep the re-centring and the camera reset it causes; the jump in coordinates is part of the state.
- **Status**: `[V]`.

### Alternating step priority from the frame counter

- **Where**: `src/battle_flow.c:3694` `func_0036E138`.
- **Original**: `gBattleParity` flips every simulated frame and decides which fighter steps first; its initial value
  at the round reset is `gFrameCount & 1`, a main-loop counter that also runs in menus.
- **Port**: `gFrameCount` must be synchronised state (or the parity forced at round start); a port that resets the
  counter per fight changes which fighter wins simultaneous hits.
- **Status**: `[V]`.

### A move's first damage entry gets its change twice

- **Where**: `src/battle_flow.c:202` `func_003682F8`.
- **Original**: applying a move record's kind-0xC damage changes adds the change to the first entry of the
  `D_005E8978` list once before the loop and again in the loop.
- **Port**: keep the double add.
- **Status**: `[V]` code; `[I]` that the effect shows in gameplay.

### Juggle damage truncated to 16 bits

- **Where**: `src/battle_flow.c:5618` `Fighter_ApplyDamage` (lines 5732, 5741, 5745).
- **Original**: the juggle scaling (`* 80 / 100`, `/ 2`) stores its result through an `s16` cast before it is shifted
  into the 20.12 life.
- **Port**: keep the 16-bit truncation (a port with wider damage values differs only on overflow, which needs a very
  large damage, but the cast is the original's).
- **Status**: `[V]` code; `[?]` whether any move reaches the 16-bit limit.

### Red-life loss in single-precision floats

- **Where**: `src/battle_flow.c:5618` `Fighter_ApplyDamage` (5662, 5678, 5754, 5760).
- **Original**: the red life (`unk3F8`, s32 20.12) is converted to `f32`, loses `(damage << 12) * 0.6f` (or `1.4f`
  in a juggle) and is converted back with `cvt.w.s` (truncation). The red life reaching 0 is a K.O. condition.
- **Port**: run these operations on the EE float model (section 7); an SSE rounding difference of one unit can move
  the K.O. frame in an edge case.
- **Status**: `[V]` code; `[?]` the EE result against SSE for real damage values.

### The CPU of the attract demo acts on alternate frames

- **Where**: `src/fighter_core.c:7077` `func_00388DA8`.
- **Original**: with the demo parameter block (`D_005F5CBC`) set, the CPU takes its new input only on frames where
  `gFrameCount & 1` equals its side byte `gCpu->unk0`.
- **Port**: keep the frame-parity gate; it ties the demo to `gFrameCount`.
- **Status**: `[V]` code; `[I]` role.

### Intro and win poses from the frame counter

- **Where**: `src/battle_flow.c:309` `func_003685B8` (`gFrameCount % 3`, line 329); `src/battle_flow.c:3897` (win pose
  `gFrameCount & 1`).
- **Original**: the intro pose of character 0x21 and the win pose depend on the main-loop counter.
- **Port**: see "16-bit frame counter" (section 8); the counter must match the original's at that frame.
- **Status**: `[V]`.

## 2. Camera

### The camera is simulation state and draws random numbers

- **Where**: `src/stage.c:3850` `Cam_PickReaction`, `src/stage.c:4075` `func_00394110`, `src/stage.c:4946`
  `func_00395EE0`, and the other reaction / replay cameras ([systems/stage.md](systems/stage.md) section 5).
- **Original**: `Cam_Update` runs inside `Battle_UpdateFrame`; reaction cameras draw from newlib `rand()` and from the
  CPU generator `func_003800D8`, depending on the stage's camera style. The camera yaw enters `Fighter.unk48`, which the
  command tests read. `func_00395EE0` also skips work on even `gFrameCount`.
- **Port**: run the camera every simulated frame, in the original order, even headless or with a free camera; a
  different camera (or none) changes the CPU's random choices.
- **Status**: `[V]` calls.

### A third LCG for the special ending

- **Where**: `src/stage.c:4780` `func_003957A0`; seeded in `src/rng_misc.c:133` `Rng_Init`.
- **Original**: `D_005F5CF4 = (D_005F5CF4 + 1) * 0x10DCD`, seeded with the boot seed.
- **Port**: keep it as state (hash and save it in T27).
- **Status**: `[V]`.

## 3. Effects

### Effects draw from the gameplay `rand()` stream

- **Where**: `src/effects.c`, `src/effects_draw.c` (all callbacks), run from `func_00346608` in the main loop
  (`src/main.c:405`).
- **Original**: effects use the same newlib `rand()` as the CPU and the camera: character 0x1D's attack volume (18
  calls), stage-coloured sparks, stage ambient effects, hit bursts, glows ([systems/render.md](systems/render.md)
  section 7.1). The CPU's choices therefore depend on the stage, the characters and the effects alive.
- **Port**: run every effect callback each frame in the original order even when nothing is drawn; do not give the
  effects their own generator, and do not cull effects off screen.
- **Status**: `[V]` calls; `[I]` consequence.

### Body-part glow draws `rand()` before the pause test

- **Where**: `src/effects.c:1978` `func_003A3C08` (line 2017).
- **Original**: `(rand() & 0xF) == 0` is evaluated first in the condition, before the pause flags (`D_005F5EE0`,
  `D_005F5EE4`, `func_003A9698`), so the glow consumes a random number every frame while a part is lit, paused or not.
  Other callbacks (`src/effects.c:2299`, `2558`) test the pause first.
- **Port**: keep the operand order of each condition; a "cleaner" order changes the stream while paused.
- **Status**: `[V]`.

### Stage 12 layer effects not paused-gated

- **Where**: `src/effects_draw.c:900` `func_003A7590` (line 907); `src/effects_draw.c:949` `func_003A76C0` (lines 975,
  1000, 1087, 1104).
- **Original**: `rand() % 1000` runs every frame regardless of the pause; stage 4's actors draw partly before the pause
  test.
- **Port**: as above.
- **Status**: `[V]`.

### Effects run only while the effect pass flag is set

- **Where**: `src/main.c:405` (`if (gDrawEffectPass) func_00346608();`), flag set at `src/main.c:346` / `388`.
- **Original**: the effect update (and its `rand()` calls) is tied to a draw flag.
- **Port**: drive the effect update by the same flag, not by whether the port renders.
- **Status**: `[V]`.

## 4. Render

### Light reset clears the stack, not the light vectors

- **Where**: `src/scene3d_d.c:675` `func_00353178`.
- **Original**: the quadword clears meant for each slot's `unk100` / `flash` and the flash position are stored over
  the stack copies of their addresses, so those vectors keep their old values after the reset.
- **Port**: keep the stale vectors (do not clear them); they are drawing state, but the T26 memory comparison sees
  them.
- **Status**: `[V]`.

### Z-sort list overflow exits the game

- **Where**: `src/zsort.c` (`func_003994C0` at line 61 and the other list functions, `exit(1)` at 66, 80, 95, 102,
  114, 121).
- **Original**: a full Z-sort list prints "(zs)list over" and calls `exit`.
- **Port**: keep the same capacities (a larger list hides a state the original never reaches); map `exit` to a
  reported fatal error.
- **Status**: `[V]` code; `[?]` whether any fight reaches it.

## 5. Modes and menus

### Theater entry reads one byte past its id copy

- **Where**: `src/theater.c:424` `Mode_EnterTheater` (loop at 449-467).
- **Original**: the 41-entry movie list is filled from a 40-byte stack copy of the id list; the last iteration reads
  byte 40, one past the copy, so entry 40 takes a stack byte as its id (then gets `unk5 = 0`).
- **Port**: reproduce the byte the original reads (the stack layout of the PS2 build) or the value PCSX2 shows;
  reading past a C array on the host is undefined.
- **Status**: `[V]` read; `[?]` the value of that byte in PCSX2.

### Attract demo is CPU against CPU seeded from time

- **Where**: `src/select_results.c:1340` `Team_BuildRandom`; `src/rng_misc.c:133` `Rng_Init`; `src/rng_misc.c:125`
  `Rng_Reseed`.
- **Original**: the demo builds random teams with `rand()` and lets two CPUs fight; `rand()` was seeded from the EE
  Timer 0 count at boot and is reseeded every frame outside fights with the VBlank count added
  ([systems/modes.md](systems/modes.md) section 16).
- **Port**: pin the boot seed and the VBlank count for a deterministic demo; do not replace the seed with a constant
  in normal play.
- **Status**: `[V]`.

### Tekken Bowl HUD reseeds `rand()`

- **Where**: `src/menu_bg.c:677` `Bowl_DrawBarcode` (`srand` at 692); `src/menu_bg.c:591` `func_003E2A08` (draft,
  `srand` at 602).
- **Original**: two HUD drawing functions call `srand` with values from the HUD state, resetting the game's `rand()`
  sequence mid-frame.
- **Port**: keep the `srand` calls in the drawing code; they change the numbers the rest of the frame draws.
- **Status**: `[V]`.

## 6. Memory card

### Save checksum and scramble

- **Where**: `src/memcard.c:256` `func_0035EBA0` (save), `src/memcard.c:282` `func_0035EC98` (load).
- **Original**: four blocks packed into a 0x480-byte buffer with one checksum byte and the scramble
  `key = b + 5 * key + 1` ([systems/modes.md](systems/modes.md) section 4).
- **Port**: keep the format byte for byte so real PS2 saves load and the port's saves load on a PS2.
- **Status**: `[V]`.

## 7. Floats and the EE FPU

### EE FPU is not IEEE 754

- **Where**: every game float operation; on the simulation path `Fighter_ApplyDamage` (section 1) and the character
  0x1D attack volume `src/fight_misc.c:899` `func_00378760` (`atan2f` of points moved by `sceVu0ApplyMatrix`).
- **Original**: the R5900 FPU rounds toward zero, flushes denormals to zero and has no infinities or NaN (results
  clamp to the largest value); `cvt.w.s` truncates.
- **Port**: done in T23: every float operation runs on the EE model (`port/src/softfloat_ps2.c`; the FPU's add keeps one
  guard bit, its div.s / sqrt.s round to nearest under PCSX2) and newlib's libm (`third_party/newlib_libm`); no host
  float code is linked.
- **Status**: `[V]` for the operation classes (1500 PCSX2 2.7.303 results per class, `port/tests/ps2float`); `[?]` the
  game's own libm results (test ELF built, PCSX2 run pending).

### Nine objects built without `-ffast-math`

- **Where**: `configure.py:51` `NO_FAST_MATH`: `effects.c`, `effects_draw.c`, `loading_title.c`, `scene3d.c`,
  `model_anim.c`, `model_anim_b.c`, `menu_bg.c`, `gallery_jpeg.c`, `tekken_bowl.c`.
- **Original**: these keep `x + 0.0f`, `-x + c`, divisions by constants and the `sqrtf` errno check; the rest of the
  game was built with `-ffast-math` (folded constants, bare `sqrt.s`).
- **Port**: build each file with the same float flags as the original; a host compiler must not contract, reassociate
  or fold differently (no `-ffast-math`, no FMA contraction on the game code), and float constants must be ee-gcc's
  values: TTT's compiler rounds decimal constants and folds to nearest like the host (0.1f = 0x3DCCCCCD in the
  executable), so no constant rewriting is needed (unlike the reference project's compiler).
- **Status**: `[V]` (matching builds).

### Skeleton float path only outside fights

- **Where**: `func_003A9510` (switch `D_005F6254`, [systems/animation.md](systems/animation.md) section 5).
- **Original**: fights use the integer skeleton (`Gte_*`, `gSinTable`, rounding toward zero in `/ 4096`); the float
  path (`sinf`, `cosf`, truncating `cvt.w.s`) runs only while the switch is set by loading / title code.
- **Port**: keep the switch; do not move fights to the float path.
- **Status**: `[V]` calls; `[I]` modes.

## 8. RNG, timing and determinism

### newlib `rand()` shared by everything, call order matters

- **Where**: newlib `rand` 0x40FBD0; callers in `fighter_core`, `effects`, `camera_stage` and others
  ([game_overview.md](game_overview.md) section 6).
- **Original**: one LCG for the CPU, the camera, effects, intro poses and menus; the CPU generator `func_003800D8`
  (`src/fighter_core.c:2968`) adds its own seed `D_005F6C08` to `rand()`.
- **Port**: keep every call in its original place and order (including calls whose result is unused, `tbl[rand() % 1]`
  for a one-entry table, and the evaluation order of arguments); use newlib's LCG, not the host `rand()`.
- **Status**: `[V]`.

### Boot seed from Timer 0

- **Where**: `src/rng_misc.c:133` `Rng_Init`.
- **Original**: the seed is the EE Timer 0 count read at boot (Timer 0 is restarted by `Gfx_WaitVSync`).
- **Port**: emulate Timer 0 or make the seed an input; T27 records it.
- **Status**: `[V]`.

### Per-frame reseed with the VBlank count

- **Where**: `src/rng_misc.c:125` `Rng_Reseed`, called from `src/main.c:416` `Frame_PreUpdate`.
- **Original**: while `D_005F5CDC` is set (outside fights), the seed grows by the VBlank count every frame and
  `rand()` is reseeded.
- **Port**: the VBlank count must be emulated (one per field at 59.94 Hz, including fields the game misses).
- **Status**: `[V]` code; `[?]` VBlank counts in PCSX2 during loads.

### Spin generator stepped while waiting for VBlank

- **Where**: `src/gfx.c:349` `Gfx_WaitVSync`; `src/rng_misc.c:262` `Rng_SpinNext`; read by `src/rng_misc.c:307`
  `Rng_PickRandomEntry`.
- **Original**: `gRngSpin = gRngSpin * 5 + 1` once per iteration of the busy wait, so its value depends on how fast
  the CPU ran; it picks random characters (the tag swap of characters 0xF and 0x21).
- **Port**: a native port spins at a different rate. Make `gRngSpin` state recorded per frame (T27) or step it a fixed
  number of times per frame matched against PCSX2.
- **Status**: `[V]` code; `[?]` the iteration count per frame in PCSX2.

### Busy wait on Timer 0 in the frame

- **Where**: `src/main.c:416` `Frame_PreUpdate`.
- **Original**: waits until Timer 0 reaches 0x724 (about 3.2 ms after the VBlank), then reads the pads.
- **Port**: emulate Timer 0 reads (the register at 0x10000000) or skip the wait; the pad read timing changes input
  latency only.
- **Status**: `[V]`.

### 16-bit frame counter

- **Where**: `src/main.c:15` (`u16 gFrameCount`), incremented at `src/main.c:419`.
- **Original**: the counter wraps at 65536 frames (about 18 minutes). `& 1` uses are unaffected, but `% 3`
  (`func_003685B8`) and the timers that subtract it (`src/modes.c:317` and others, masked with `& 0xFFFF`) see the
  wrap.
- **Port**: keep it 16 bits.
- **Status**: `[V]`.

## 9. Memory layout and hardware

### Uncached address aliases

- **Where**: `src/file.c:211` `File_Request` (line 232), `src/file.c:80`.
- **Original**: load destinations become `(addr & 0x0FFFFFFF) | 0x20000000` (the uncached KSEG view).
- **Port**: the memory map (T22) must alias the uncached segment to the same memory.
- **Status**: `[V]`.

### Misaligned file request exits

- **Where**: `src/file.c:211` `File_Request` (line 217); `src/file.c:62` `File_SendSifCmd`.
- **Original**: a destination not 16-byte aligned, or a SIF stream request with no EE stream buffer, calls `exit(1)`.
- **Port**: keep the checks (host allocators must give the same alignment); report instead of silently continuing.
- **Status**: `[V]`.

### Sound queue writers have no bound

- **Where**: `src/sound_if.c:40` `Snd_Flush`; writers such as `Bgm_Play` in the same file.
- **Original**: the writers append byte codes to the 0x30-byte `gSndQueue` without a check; `Snd_Flush` sends the 48
  bytes and the IOP stops at the first 0xFF. Unknown codes hang the IOP loop ([systems/audio.md](systems/audio.md)
  section 4).
- **Port**: keep the queue size and layout; anything written past it lands in the next variable as on the PS2.
- **Status**: `[V]` code; `[?]` whether a frame ever queues more than 0x30 bytes.

### Model files relocated in place with 32-bit offsets

- **Where**: `draw_list`, `dma_gif` (`func_00343CC8`), `sprite2d` banks ([systems/render.md](systems/render.md)
  section 8).
- **Original**: offsets in loaded files become pointers by adding the file's address, in 32-bit words.
- **Port**: keep game memory below 4 GB with 32-bit pointers (T22), or translate on load; struct layouts must stay
  the PS2's.
- **Status**: `[V]`.
