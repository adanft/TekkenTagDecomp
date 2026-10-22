# Stage and camera (`camera_stage`, `src/stage.c`)

What the fight simulation asks of the stage, and the module that answers: `src/stage.c`, 0x38BF98-0x396D00 (the
`camera_stage` range of [game_overview.md](../game_overview.md) section 4), 105 functions, `.rodata` 0x507EE0-0x509D00.
Written in T15 batch 1, completed in batch 2 (2026-10-19).

Tags: **[V]** read in matching C or in the disassembly, **[I]** inferred.

## 1. Summary

- **There is no stage geometry in the simulation** [V for the callers read; I for the asm-only modules]. Tekken Tag's stages are open floors: no walls, no ring-out, no
  floor height. The simulation calls no stage function for collision; nothing reads a per-stage bound, wall list or
  height map. What stands in for them is in `battle_flow`:
  - the floor is the plane y = 0: a fighter above it (`unk8 < 0`, y grows downwards) drifts back with `unk108 += 6`
    and is snapped to 0 (`Battle_UpdateMovement`, [fighter.md](fighter.md) section 5.12);
  - the fighters can be at most about 0x1D00 apart: beyond that, a fighter whose move lacks flag 0x2000 is pulled back
    to its anchor `D_00879610[i]` (`Battle_UpdateDistance`, fighter.md section 5.1);
  - the floor is infinite: when a fighter's x or z leaves -0x78000..0x78000, `func_00372E98` moves both fighters so
    that the first one stands at 0, keeps the shift in `unk10` / `unk14` and resets the camera (`Cam_Reset`, this
    module) and `func_00350578` for both (fighter.md section 5.11).
- **The stage index** `D_005F5CB6` (u16, 0-0x18 used; 23 values in the byte tables 0x455D50 / 0x455D68) selects data
  that is drawn or heard, not simulated: the stage model and textures (`func_00367938`, `func_00340070`,
  `Wind_Init`), the music (`func_003751A0`, `func_00375868`), the camera distances and camera style (`D_00507FE0`, below)
  and the demo camera scripts (`Stage_GetCamScriptBank`). The camera style matters to the simulation (section 5, item 3).
- **The camera is what the simulation depends on**: `Cam_Update` runs inside `Battle_UpdateFrame` every simulated
  frame, advances the round's camera phase `gRoundPhase`, writes the camera angles that `Battle_UpdateFacing` reads,
  and draws random numbers from the same generators as the CPU opponent. Section 5 lists the four channels; a port that
  replays fights must run this module exactly (or reproduce those four effects).

## 2. Units

| Quantity | Unit | Evidence |
|---|---|---|
| Positions | s32 world units; y down (heights are negative) | round start at x = +-1000 (`func_003730E8`); camera heights -0x4B0, -0x590, -0x1F40 |
| Fight area | infinite, re-centred at +-0x78000 | `func_00372E98` [V] |
| Angles (simulation, `gSinTable`) | 12 bits per turn (0x1000), cosine at +0x400 | `Math_Sin` / `Math_Cos`, every camera function [V] |
| Angles (camera keys) | 18 bits per turn (`<< 6` of the 12-bit angle) | `func_0038E308` stores `atan << 6`, `Cam_SetKey` divides by 64 [V] |
| Sine values | 4096 = 1.0 (results `>> 12` or `/ 4096`) | [V] |
| Blend weights | 0x1000 = 1.0 | `D_00433330.unk4[0..4]`, `func_0038CFC0` [V] |
| Camera distance | the projection distance given to `sceVu0ViewScreenMatrix` (`Cam_SetDistance`), clamped to the stage's minimum and maximum | [V code; I meaning] |

## 3. Data

| Symbol | Where | Layout and use |
|---|---|---|
| `D_00433330` | `.data`, `Unk433330` in `src/stage.c` | the camera state: +0 camera mode (1 default blend, 2 preset, 6 script, 7, 8, 0xA, 0xC-0xF reaction cameras) [I names], +4 / +0x18 the five key weights now and on the previous frame, +0x30 / +0x34 the side the camera favours, +0x38-+0x50 counters and the current preset, +0x54 / +0x58 the camera number and its preset index (`Cam_PresetIndex`), +0x5C the entry picked, +0x60 the move whose camera is playing, +0x64-+0x6C the orbit of the fight camera, +0x7C `unk7C[2]` the move that last started a hit camera, per fighter slot `(s16)Fighter.unk20`, +0x84 script frame, +0x8C-+0x94 script origin, +0xA0 mirror flag, +0xA4-+0xB4 the stage's five values from `D_00507FE0` (distance, minimum, maximum, +0xB0 script scaling, +0xB4 the stage's camera style 0-2), +0xC0 the "fighters in a throw" state (`func_0038C188`), +0xC4 a hold flag read by `func_0038E2D0` [V offsets; I names] |
| `D_008B90B8` | `.bss`, 5 x 0x1C | camera keys `{x, y, z, pitch, yaw, roll, distance}`: key 0 the fight camera, 1 the other blend input, 2 the script / hit camera, 3 the current copy, 4 a saved key (`func_0038EAA0`) [V layout; I roles] |
| `D_00415808` | `.data` | camera angles used everywhere: [0] pitch, [1] yaw (12-bit), [2] roll (0), [5..7] position; written by `Cam_SetKey` [V] |
| `D_00507FE0` | `.rodata`, 25 x 5 s16 | per stage: default, minimum and maximum camera distance, script scaling flag and camera style 0-2 (`func_0038DBC0`, `func_0038DB98`; a stage above 0x18 uses entry 0) [V] |
| `D_00508B60` | `.rodata`, s16 | camera number to preset index (`Cam_PresetIndex`: numbers below 0x1E by table, 0x1E-0x231 give -1, -2 ..., a negative index is a script) [V] |
| `D_00508908` | `.rodata`, 0x14-byte records | presets: kind, count, four yaw and four distance choices (`func_00394110` picks one of each with `rand() & 3`) [V] |
| `D_00434C78` | `.data`, 0x134 x 12 | per move kind (`unkC0->unk24 >> 23`): a weighted list of camera reactions `{flags, weight, number[2]}`, its count and two ranges; `func_00393898` turns the weights into cumulative 0-0x1000 thresholds once [V] |
| `D_005095C0`, `D_00509700`, `D_00509820` | `.rodata` | hit and throw camera lists (`func_003943A8`, `func_00394300`), normalised by `func_003941F0` [V] |
| `D_00433010` | `.data`, 10 x 0x50 | demo camera sequences, 10 steps `{phase, frames}` each (`func_0038BF98`, `func_0038C048`) [V] |
| `D_008B92D0`, `D_008B92E8`, `D_008B9280`-`D_008B92C0` | `.bss` | camera script player: decoded eye / target (s16), the 0x3200-byte stream buffer and the 8 channel accumulators (section 4.3) [V] |
| `D_00433520` | `.data` | the two fighters' tracking points (x, min(head y, y), z), refreshed every 4 frames in the replay camera (`func_003967E0`) [V] |

## 4. What the functions do

### 4.1 Per frame

`func_0036DDD0` (battle_flow) calls `Cam_Update(a, b, &gRoundPhase)` (matching C), then `func_0035B9A8`. It returns at
once when the demo camera script is playing (`D_005F5CBC` set, `func_00393788` finishing it), stores
`func_0038C188()` (whether one fighter is in a throw, move kind 0xAE) and switches on the phase:

| Phase | Camera | Next phase |
|---|---|---|
| 0 | intro: in the demo a random sequence of `D_00433010` (`func_0038BF98`), else the default blend set-up | 1, or the demo kind's |
| 1 | the intro blend (`func_0038CFC0`) | 0xD when the blend ends |
| 2, 3 | cameras of `model_anim` (`func_003D2818`, `func_003D0A38`) | unchanged |
| 4, 0xD | the fight camera `func_0038C1E8` (4 resets first) | in the demo, `func_0038C048(0xD)` |
| 5-0xC | four demo camera kinds (orbit, sweep, follow, swing: `func_00395D20`, `func_00395EE0`, `func_00396320`, `func_00396620`), odd phases set up, even phases step | `func_0038C048(phase)` |
| 0xE, 0xF | win camera of side 0 / 1 (`func_00395718`, then `func_00395750` every frame) | 0x10 |
| 0x11, 0x12 | K.O. camera (`func_00395C90` on the side that lost, by life `unk3F4 == 0`) | unchanged |
| 0x13 | nothing | |

`Battle_UpdateRound` sets phases 0xE-0x12 for the round end, and `Battle_ApplyVelocity` only moves the fighters while
`gRoundPhase < 0xE` (fighter.md section 5.1), so the camera's own phase changes (1 to 0xD, 0xE / 0xF to 0x10) are part
of the simulation's state [V].

### 4.2 The fight camera

`func_0038C1E8` (885 instructions, a draft two lines from matching) runs the camera mode `D_00433330.unk0`; its cases
are in the source order 1, 2, 8, 14, 15, 12, 7, 10, 13, 3, 6, 4, 9, 11, 5, default (`Cam_Reset`) [V]:

- **Mode 1, the default orbit**: unless a reaction camera starts (`func_0038DEA0`), it picks the favoured side from
  `Fighter.unkE0` and the fighter in the lead from `D_005F6150` / `D_005F6158` (two words per fighter: `Fighter.unk624`
  now and on the previous frame; -1 on a tie), calls `func_00393E08` and goes to mode 6 (no preset) or 3 (a preset).
- **Blend modes** ramp the key weights `unk4[]` by `(unk3C << 12) / unk40` and count `unk3C` / `unk4C` down. Mode 6
  takes its weight from `func_00391170`'s result * 32; a negative result makes the whole step return 0.
- **Then every frame**: the orbit keys come from `func_00391170` or the throw camera `func_00394648` while `unk4[1] > 0`
  and from `func_00390FA8` while `unk4[0] > 0`; the weights above 0 form a bit mask that picks one key or two keys
  blended (masks 0x11 / 0x12 blend with key 4 by a cosine ease, `gSinTable[(w / 2 - 0x400) & 0xFFF] / 2 + 0x800`), and
  `unk4[]` is saved in `unk18[]`. `unk7C[slot]` is cleared when that fighter's move is no longer the one that started
  the hit camera.

The functions it calls [V code; I names of the roles]:

| Function | Role |
|---|---|
| `func_0038DEA0` | starts a reaction camera when a move hits or throws (unless `unk7C[0]` / `unk7C[1]` hold one): the side of the attack (`func_003943A8`, which looks the move up in the hit / throw lists), the lead, then by the stage's reaction kind (`func_003953B0`, 3-17) the mode 0xA, 0xD, 0xE / 0xF, 0xC / 8 or 7 / 8, its counters and facing; returns 1 when a camera was set. `func_00394300` draws from the lists with `rand() & 0xFFF`; numbers go through `Cam_PresetIndex`; scripts via `func_003928B8`, presets via `func_0038D1B8` / `func_0038D290` / `func_0038D3D0` |
| `func_0038EC08` | not "the default key" (T15 b1): it updates the fighters' tracking points `D_00433520` (x, head height `unk70C - 0x122`, z) and keeps them as the previous frame's in `D_00433540`. A fighter that is not tracked on its own follows the other's motion; `D_005F6170` / `D_005F6174` ease a point in by a quarter of the gap a frame; throws (move kinds 0x17-0x1A) pull the thrown fighter's point toward the thrower (0x17 by 6/128, 0x18 / 0x1A by `(0x2198 - isqrt(d^2) / 64) / 1250`); `Fighter.unkE0` is set for the call and restored |
| `func_0038F7F8` | the yaw of the camera for a key slot: from the fighter behind (`unk624`) to the one in front, compared with key 3's yaw (18 bits wrapped), driving a swing state machine in `D_008B9148[slot]` (state `unk0`, position / velocity `unk4` / `unk8` clamped to +-0xDDD0 or `func_0038F6B0`'s limit by the throw kinds 0x13-0x16, frame counter `unk20`, a sine-shaped target eased by at most 0x100 a frame) |
| `func_0038FD90`, `func_00391358` | the orbit key (twins, about 1060 instructions each): key idx looks at the midpoint of `D_00433520` from (pitch, yaw); the distance grows until both fighters fit on screen (projected with `unkA4`; `D_005F6180` = more than 0x188 apart on screen); the per-key offsets `D_008B9148[idx].unk28`-`unk30` ease in or decay; key 1 rises by at most 0x1C a frame (`D_005F6148` / `D_005F614C`); `D_008B9148[0].unk24` says the camera moved more than 0x3E8 squared in x / z. The twin starts its lift at -0x12C and re-clamps the depths to 1 when too wide |
| `func_00394648` | the throw cameras, by the definition `D_005F6C28` points to (`unk0` kind 0-17, two distances interpolated over the throw by `unk4C / unk50` and scaled by `unkA4 / 955`, pitch and yaw choices): 0 the orbit (`func_0038FD90`), 1 the eye on a sphere around `D_008BC680`, 3-5 / 11-13 an offset rotated through the GTE by the fighter's facing, 6-8 / 14-16 interpolated yaw and pitch, 9 / 17 the same around a fixed target, others `Cam_Reset`; target and eye heights clamped to -0x578..-0x384 |
| `func_0038CFC0`, `func_0038E6D0` | blend keys 0 and 2 by the weights; interpolate two keys (looked-at points `func_0038DC68` and distances linearly, angles the shorter way round), making the result key 3 |
| `Cam_SetKey` | makes a key current (key 3) and writes `D_00415808` |
| `Cam_SetDistance` | the projection: the distance clamped to the stage's minimum and maximum, `sceVu0ViewScreenMatrix` into `D_00433440` and `D_00433480` (centre 2048 / 2048, near 64, far 196608, z range up to 262140 / 196604), both widened by 1.25 |

### 4.3 Camera scripts

`CamScript_Load(file)` loads a bank of scripts (file 0 or 2-0x29 or 0x40-0xA7, 0x14000 bytes) and records each
script's start (`D_008BC4F8`) and length (`D_008BC578`). `CamScript_Step(n, out, flag)` starts script n (it streams the
data 0x40 bytes at a time into `D_008B92E8` with `func_0035C698`) and returns its first byte (the orientation kind
used by `func_003928B8`); with n = 0 it decodes one frame: a 16-bit mask gives each of 8 channels a 2-bit code (0 no
change, 1 repeat the last delta, 2 an s8 delta, 3 an s16 delta), the deltas are accumulated into eye y, target y,
eye x, eye z, target x, target z, distance and an end flag (`D_008B92D0`). `func_00392B68` turns the frame into a key:
eye and target relative to the script origin (`unk8C`-`unk94`), rotated by `unk68`, mirrored with `unkA0`, scaled to
the stage distance when `unkB0` is set [V].

### 4.4 Stage set-up

At the fight set-up (`func_0036D010`, battle_flow): `Stage_GetCamScriptBank(stage)` and `CamScript_Load(0x40 + it)` in the demo,
`func_0038D508`, `func_00393718` (starts the demo's scripted camera with the projection matrix of `func_00366EB8`),
`func_00390E08` (clears the keys and loads the stage's values, `func_0038DBC0`), `func_00395400` (the round-start
zoom of `D_008BC690`, 0x78 frames in round 1, 0x5A later, stepped by `func_003954E8` inside the intro blend), and at boot `func_00393898` and `func_003941F0` normalise the weighted lists once [V].

### 4.5 Status (T15 batch 2)

59 of 105 functions match (`Cam_SetDistance` since T17 batch 4); the other 46 are `NON_MATCHING` drafts in `#if 0`
that all compile (T15 b1 left 16 raw m2c drafts that did not). The file's `.lit4` words (0x5F5174-0x5F519C) are a
subsegment of `src/stage.c`; the three float functions still in assembly keep theirs in place with `LIT4_WORD`.

**T15 status** (what stays `NON_MATCHING`, carried to T19; the closest first, each draft's note has the details):

- 2-4 lines: `func_0038C1E8`, `func_0038D290`, `func_0038D3D0` (the store of r / 5 to `unk3C` lands before the mode
  store in all three: probably one unknown in the struct or the source), `func_00395718` (the prologue's `sd ra`),
  `func_00394300` (one `andi`), `func_0038F768` and `func_003963C0` (register choice / an argument copy).
- `jal atan2f` delay slot: `func_00392F80`, `func_003932F0`, `func_00393550`. The original has the `sqrt.s` in that
  slot; since T17 batch 4 the build puts it there too when gcc leaves the slot to the assembler (`configure.py`
  `fix_compiler_asm`), but these drafts let gcc fill it with another instruction (an argument, `neg.s`, the load of
  `dist`), so their scheduling still differs. (`Cam_SetDistance` matched once gas stopped adding the `li.s` hazard nop.)
- Store order and scheduling, 7-60 lines: `Cam_Reset`, `func_00392888`, `func_00393F60`, `func_00393BF0`,
  `func_00395BF0`, `func_00390FA8`, `func_0038E308`, `func_00390E08` (a second local below the key), `CamScript_Load`,
  `Cam_PickReaction`, `func_00393E08`, `func_00395400`, `func_003959C8`, `func_0038EC08`,
  `func_00393CB8`, `func_00391170`, `func_003967E0`, `func_00396970`, `Cam_SetKey`.
- Register allocation or block layout of larger drafts, 70-200 lines: `func_0038F7F8`, `func_00393898`,
  `func_003941F0`, `func_003954E8`, `func_00396418`, `func_00395EE0`, `CamScript_Step`, `func_003928B8`,
  `func_003943A8`, `func_0038DC68`, `func_003957A0`, `func_00392B68`, `func_0038E6D0`, `func_0038DEA0`.
- Typed drafts of the long functions, a few instructions to 32 off: `func_00394648` (698 lines), `func_0038FD90` (794),
  `func_00391358` (866).

**T19 batch 2** (2026-10-22): matched `CamScript_Load` (`func_0035D238` returns `s32`), `func_0038F768`,
`func_00392888`, `func_00394300`, `func_00395BF0`, `func_003963C0`. The drafts left: [../t19_inventory.md](../t19_inventory.md).

**T19 batch 3** (2026-10-22): no new match; `func_0038D290` got a smaller diff (one store left out of place) and
`Cam_PickReaction`'s note names its `movz` form. The drafts left: [../t19_inventory.md](../t19_inventory.md).

## 5. What the simulation depends on (determinism)

1. **The camera phase** `gRoundPhase` (0x5F5EF0): written by `Cam_Update` (and by `battle_flow`), read by
   `Battle_ApplyVelocity` and the round predicates [V]. The name is from T14 b2; it is the camera's phase as much as the
   round's.
2. **The camera angles** `D_00415808`: `Battle_UpdateFacing` copies the yaw `[1]` into `Fighter.unk48`, which the command
   situation tests use through the quadrants `unk50`-`unk58` (fighter.md sections 5.2, 5.9) [V].
3. **Random numbers**: the camera draws from newlib `rand()` (`Cam_PickReaction`, `func_00394110`, `func_00394300` when a
   move, hit or throw starts a reaction camera; `func_00395D20`, `func_00396620`, `func_00396AB0` in demo and replay
   cameras), from the CPU's generator `func_003800D8` (`D_005F6C08`: `func_0038BF98`, `func_003910D0`, `Cam_PickReaction`
   on stages of camera style 1 / 2, `func_00396AB0`) and from its own LCG `D_005F5CF4 = (D_005F5CF4 + 1) * 0x10DCD` in the
   special ending (`func_003957A0`; `Rng_Init` seeds `D_005F5CF4`). The CPU opponent and the intro poses use the same
   `rand()` state, so a camera that consumes a different number of values changes the CPU's choices [V calls].
4. **The camera state** `D_00433330`: `func_00372E98` asks `func_0038E2D0` (hold flag `unkC4`, both moves' byte 0x2C)
   before re-centring, and resets the camera after it [V].

The fight never reads `D_005F5CB6` for gameplay. The stage reaches the simulation only through the camera: its style
(`D_00507FE0`, fifth value, in `D_00433330.unkB4`) decides whether reaction cameras draw from `rand()` or from the CPU's
generator, and halves the yaw choices (`func_00394110`) [V for the readers found; I that none is missed in the
asm-only modules].

## 6. Open

- The meanings of most camera modes and `D_00433330` fields; `func_003D0A38` / `func_003D2818` (phases 2 and 3) are in
  `model_anim` and not read.
- Names given in T15 b2: `Cam_Update` (0x38D518), `Cam_Reset` (0x38C0F0), `Cam_SetKey` (0x38E4F8), `Cam_SetDistance`
  (0x38DA28), `CamScript_Load` / `CamScript_Step` (0x392E00 / 0x3923F0), `Cam_PickReaction` (0x393A90),
  `Stage_GetCamScriptBank` (0x393838). Candidates once their roles are surer: the fight camera step `func_0038C1E8`,
  the reaction start `func_0038DEA0`, the tracking points `func_0038EC08`, the orbit keys `func_0038FD90` /
  `func_00391358`, the throw camera `func_00394648`.
