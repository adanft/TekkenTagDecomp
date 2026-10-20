# Animation and skeleton (`skeleton`, `motion`)

What the fight simulation reads from the animation side, and the two modules that produce it: `src/skeleton.c`
(0x34E748-0x34FF10, 16 functions, cut out of the `scene3d` range) and `src/motion.c` (0x3639F0-0x364448, 19
functions, cut out of the `fighter_setup` range; `.rodata` 0x4FC610-0x4FC630). Written in T16 batch 1 (2026-10-19).

Tags: **[V]** read in matching C or in the disassembly, **[I]** inferred.

## 1. Summary

- **The simulation's fighter position is written by the skeleton.** `Fighter.unk51C`-`unk524` (the position every
  distance, facing and push test uses) is the translation of the root joint, set each frame by `Skel_SetRootMatrix`
  (0x34E938) from the base position `unk4`-`unkC` minus the root motion of the current motion frame [V].
- **Hit shapes are joint positions.** `Skel_PlaceHitShapes` (0x34F3D8) puts the 14 body spheres and the 8 push spheres
  on joints given by two `.data` tables, and the end points of the attack volumes on joints named by the current move
  record; `Skel_SetVolumeStarts` (0x34F288), called at the start of the next frame before the move step, sets the start
  points. An attack volume is therefore the segment a joint swept over one frame (or the segment between two joints)
  [V code; I wording].
- **The camera reaches the simulation through the skeleton.** `Skel_ProjectJoints` (0x34FD90) projects joints 2, 11,
  14 and 17 with the camera matrix `D_00436530` into `unk624`-`unk62C`, which `Cmd_CheckSituation` (kind 0x5C) and the
  camera read [V].
- **Motion data is integer.** Poses are 57 s16 channels per frame (3 root channels and 3 rotation angles for each of 18
  joints), decoded from a delta / run-length bit stream (`Motion_DecodeFrame`, 0x363D00). The skeleton is built with
  the GTE-style fixed-point library (`src/gte.c`) and `gSinTable` in fights; a float path (sinf, cosf, a float Euler
  matrix) runs only while the flag `D_005F6254` is set, which the fight set-up clears (section 5) [V code; I modes].

## 2. Units and layouts

| Quantity | Unit | Evidence |
|---|---|---|
| Positions | s32 world units, y down | as in [stage.md](stage.md) section 2 |
| Pose angles | 16 bits per turn; `>> 4` gives the 12-bit angle of `Gte_RotMatrixXYZ`; the float path uses `2 pi / 0x10000` per unit (`ANGLE_RAD`, `.lit4` 0x38C90FDB) | `Skel_SetRootMatrix`, `Skel_RotMatrixFromAngles` [V] |
| Matrices | `GteMatrix`: s16 rotation, 0x1000 = 1.0, s32 translation | include/gte.h [V] |
| Root channel 2 (distance) | u16 world units, `& 0x7FFF` after scaling | `Motion_SampleRoot` [V] |
| Motion scale | `Fighter.unk4FC`, 0x1000 = 1.0, applied to root channels 1 and 2 | `Motion_GetScale` [V] |

**A joint** (`SkelJoint`, 0x50 bytes, src/skeleton.c) [V offsets; I names]:

| Offset | Field |
|---|---|
| 0x04 | local `GteMatrix`: rotation from the pose, translation = the bone's offset (set at load, not per frame) |
| 0x24 | world `GteMatrix` (parent world x local); its translation at +0x38 is the joint position |
| 0x48 | parent joint |

The root is a joint at `Fighter + 0x504` (local translation at 0x51C = `unk51C`-`unk524`); joints 0-17 start at
`Fighter + 0x630` (joint *j*'s position at `0x668 + 0x50 * j`, the `FighterJoints` view of include/fighter.h); joints
18 to `unkB050 - 1` are per-character extras (`func_0034C508` builds their local matrix: a fixed-point pendulum pushed by the side's wind, [render.md](render.md) section 4), and 22 / 23 serve as virtual
points for some states (section 3.3). Indices up to 23 are accepted by the hit shapes.

**A pose** (`D_00416A18`, `.data`): 57 s16: channels 0-2 the root (angle, height, distance), then three angles per joint
(`Skel_GetPoseAngles` returns `-x, -y, z`) [V]. The root of the *base* motion (`Fighter.unkB8`) is sampled separately
into `D_005F5E70` (`.sdata`, 3 s16) and copied to `unk30` / `unk32` / `unk34` [V].

**A motion** (what `FighterMove.unk0` points to) [V decoder; I names]:

| Offset | Content |
|---|---|
| 0x00 | u8 frame count |
| 0x02 | the first frame, 57 raw s16 |
| 0x74 | the last frame, 57 raw s16 |
| 0xE6 | u16 per 32-frame block: offset of the block's streams from the motion start, in halfwords |

Frame 0 is the first raw frame and frame *count* - 1 or later the last one. For a frame *f* in between, each channel
starts from its first-frame value and applies `((f - 1) & 31) + 1` steps of block `(f - 1) / 32`. A channel's stream
starts with a 5-bit header (the stream's length in halfwords), then 4-bit codes read from 16-bit words: 0 no change;
1-6 add a biased delta (6, 4 or 2 bits: -84.., -20.., -4.., +1.., +5.., +21..); 7 a raw 12-bit value; code + 8 repeats
the code for `(next 5 bits) + 1` steps. Deltas and raw values are shifted left by 2 for channels 1 and 2 (root height
and distance) and by 4 for the others (angles). The 4-bit code, bias and width persist from one channel to the next.

**Motion banks** (`Motion_LoadBanks` 0x363FB8): an archive is decompressed into a buffer; entries 0-2 are three banks
(`D_005F6B74`, `D_005F6B78`, `D_005F6B7C`, counts `D_005F5E30`-`D_005F5E38`) of clips `{u16 frames, u16 groups,
frames x groups x 3 s16}` stored raw. Bank 0 replaces the per-move motion while `D_005F6254` is set
(`func_003640B0`, clip `D_005F5E3C`); banks 1 and 2 serve `Fighter.unkB088` / `unkB074[2]` (`func_00364298`,
`func_00364390`), with 0 meaning the character's own data (`func_003672F0`, `func_003672D0`) [V code; I roles].

## 3. What the functions do

### 3.1 Per frame (`Skel_Update`, 0x34E748)

`func_00371B18` (battle_flow) calls `Skel_Update` (through the tail-call stub 0x34E8B0) for each drawn fighter
(`unk127`); it runs on simulated and on non-simulated frames alike ([fighter.md](fighter.md) section 5.1). In order [V]:

1. `func_0034E018` (the side's wind vector, render.md section 4), `func_003ADFB0`, `func_00365300(f)`; `func_00353280(side)` when `func_00346378(side)`.
2. `Motion_SampleRoot(unkB8->unk0, D_005F5E70, unkBC - 1, f)`: the root channels of the base motion;
   `Motion_SamplePose(unkC0->unk0, D_00416A18, unkC4 - 1)`: the whole pose of the current move at its frame `unkC4`.
3. `unk30` / `unk32` / `unk34` = root channels (`unk34 & 0x7FFF`); `func_0034FA60`: when `unk126` is set (cleared
   there), the base position `unk4` / `unkC` is moved so that the root stays where it was, using the root at frame
   `unkBC - unkDA - 1` [V code; I purpose].
4. The motion blend (`func_00373E38` in game mode 5, else `func_00373428` when `D_005F5F10` frames are stepped;
   battle_flow, NON_MATCHING drafts), `func_00365248`.
5. `Skel_SetRootMatrix` (section 3.2), `func_003632E0`, `func_00362C28`, `func_00350728` (blade trail on / off by move, render.md section 3).
6. Unless `Tag_StepCounters(slot, 1)` (a per-slot countdown in `model_anim`): `Skel_ComputeWorld` (section 3.2),
   `func_003478F0(f, 0)`, `Light_UpdateFighter(side, f + 0x590)` (the fighter's lights, [render.md](render.md) section 4), `func_0034FB48` (`Tag_SetCounter2(slot, 1)` for a move with
   flag 0x10000, then `Skel_ProjectJoints`).

### 3.2 Matrices

- `Skel_SetRootMatrix` (0x34E938): the root rotation from `(-unk18, unk3C, -(unk1C - 0x8000))` after
  `func_00365108(f, &r)` (in a frame without simulation `unk3C` takes the result); `Gte_RotMatrixXYZ` of the angles
  `>> 4`, or the float matrix `func_00349EE0` of the angles in radians (x 4096, truncated). Translation: `unk4`-`unkC`,
  then, when `unk11D == 0`, minus the root motion: y -= height (channel 1, or the blended height `func_003A0340` while
  `unkB5DC` blends, which also sets `unkB884 = (unkB880 << 12) / unkB878`), x -= distance * sin(unk1A + channel 0), z
  -= distance * cos(...) (`gSinTable`, `/ 4096` rounding toward zero; or `sinf` / `cosf`); with `unk11D == 2` only y.
  Last `func_00364F38(f)` [V].
- `Skel_ComputeWorld` (0x34EF20): in game mode 5 the root is scaled by `func_003AE6C0(character)`; root world = root
  local; for joints 0-17 the local rotation comes from the pose (`Skel_RotMatrixFromAngles`), blended by
  `func_003A02C0` with `unkB630[j]` while `unkB5DC` is set (not for joint 0), and is kept in `unkB390[j]`; then world =
  parent world x local (`Gte_CompMatrix`). With `unkB130` bit 0, `func_003626B8` may give a replacement local matrix
  whose rotation is also copied to `D_008791A0[slot]`. Joints 18 and up follow the same way after `func_0034C508` [V].
- `Skel_RotMatrixFromAngles` (0x34ED90): `func_003986D8` (fixed point) or the float Euler matrix of the negated
  angles [V].

### 3.3 Hit shapes

- `Skel_PlaceHitShapes` (0x34F3D8), after `Fighter_UpdateMotion` [V]:
  - virtual joints: state `unk28 == 4` puts joint 22 at joint 10's matrix x (100, 0, 720) and joint 23 at x (100, 0,
    360); `unk28 == 0xE` for characters 0x14 and 0xF puts joint 23 at joint 2 x (800, 0, 600); `unk28 == 0x1A` or
    `0x20` puts joint 22 at joint 6 x (100, 0, -400) (the offsets are `.sdata` constants `D_005F5E50`-`D_005F5E68`);
  - body sphere *i* (0-13) is placed on joint `D_0042F060[i]`; sphere 8 is raised by 120 and sphere 11 by 60 (y down);
    push sphere *i* (0-7) on joint `D_0042EFB8[i]`; `unk388`-`unk390` copy sphere 0's centre;
  - when `unk118` is set: attack volume 0's end on joint `unkC0->unk28` (and its start on `unk29` when non-zero),
    volume 1's end on `unk2A` (start on `unk2B`); joint 0x13 means joint 6 for character 0xE; 24 or more is ignored.
- `Skel_SetVolumeStarts` (0x34F288), first in the simulated frame: for `unk118` 1 the start of volume 0 on joint
  `unk28` when `unk29` is 0; for 2 also volume 1's start on `unk2A` when `unk2B` is 0. The start is thus last frame's
  joint position [V].
- The radii stay those of the character tables ([fighter.md](fighter.md) section 5.5).

### 3.4 Screen positions and replacements

- `Skel_ProjectJoints` (0x34FD90): for each entry of the joint order `D_0051DE50` (18 bytes) that is 11, 2, 14 or 17,
  `func_0034FD10` sets the GTE rotation and translation to camera x joint world, and `func_00398600(0, 0, 0, out)`
  projects the origin: joint 11 to `unk624` (and the return value to `unk626`), joints 2, 14 and 17 to the x of
  `unk628`, `unk62A`, `unk62C` [V code; I "screen"].
- `func_0034FBA0(side)` copies the bone offsets of joints 5, 6, 18, 19, 20 (`D_004FC668`) into `D_008791A0[side]`;
  `func_0034FC68` / `func_0034FC88` return those matrices (joints 5 and 6 while `unkB130` bit 0 is set) [V].

### 3.5 Motion sampling

- `Motion_Sample` (0x3639F0, kind 1: 3 channels, else 57): `func_003640B0` (bank 0) while `D_005F6254` is set, else
  `Motion_DecodeFrame`; always returns 0, so the `>> 13` branch of `Motion_SampleRoot` / `func_00363BD8` (with a clamp of
  small negatives) is dead code [V].
- `Motion_SampleRoot` (0x363AB8) scales root channels 1 and 2 by `Motion_GetScale` (`unk4FC`, or the other fighter's
  while `unkE0 < 0`); `func_00363BD8` decodes the same frame twice and scales it the same way; `Motion_SamplePose`
  (0x363BD0) decodes all channels [V].

## 4. What the simulation reads

| Field | Written by | Read by |
|---|---|---|
| `unk51C`-`unk524` (position) | `Skel_SetRootMatrix` (root translation) | distances, facing, push, camera, CPU (fighter.md) |
| `unk30`, `unk32`, `unk34` (root channels) | `Skel_Update` | battle_flow |
| `unk1F0[14]` body spheres, `unk308[8]` push spheres, `unk388` | `Skel_PlaceHitShapes` | hit test, `Fighter_Push` |
| `unk190`-`unk1BC` attack volumes | `Skel_SetVolumeStarts`, `Skel_PlaceHitShapes`, character 0x1D's code (fight_misc) | hit test, `func_00371C08` |
| joint positions (`+0x668`) | `Skel_ComputeWorld` | move events 6-11 (effects at a joint), CPU (`fighter_core`), stage camera |
| `unk624`-`unk62C` | `Skel_ProjectJoints` | `Cmd_CheckSituation` kind 0x5C, camera |
| `unkB884`, `unkB5D2` | `Skel_SetRootMatrix` | the motion blend |

The other `model_anim` functions the simulation calls are not animation: `func_003D29E8` looks up a start frame for a
move by character (`D_00447480`), `Fighter_LogMove` keeps a per-member list at `Fighter + 0x64`, `Tag_StepCounters` /
`Tag_SetCounter` are the per-side tag counters `D_01FB6ED0` / `D_01FB6EE0` [V code; I roles].

## 5. Determinism

- In fights everything above is integer: `Gte_*` (s16 x s16 / 4096 rounding toward zero), `gSinTable`, and the
  decoder. The float path (sinf, cosf, `func_00349EE0`, truncating `cvt.w.s`) is taken only while `D_005F6254` is set:
  `func_003A9510(1)` from `func_003B8D60` and `func_003B8ED8` (reached from loading / title code and
  `func_003F0C00`), `func_003A9510(0)` from the fight set-up `func_003B8AA0` and four other callers [V calls; I modes].
- `Skel_Update` runs on frames without simulation too (pause, replay steps): its outputs (position, spheres) are
  recomputed from the move state each frame, so they are a function of the simulation state plus the camera state.
- The camera matrix `D_00436530` feeds `unk624`-`unk62C`, which a command condition reads: the camera
  ([stage.md](stage.md) section 5) belongs to the simulated state.
- `Motion_LoadBanks` calls `exit(0)` when the archive does not decompress into its buffer.

## 6. Status (T16 batch 1)

30 of the 35 functions match: `skeleton.c` 13 / 16, `motion.c` 17 / 19 (including every function of the hit
shapes: `Skel_Update`, `Skel_ComputeWorld`, `Skel_PlaceHitShapes`, `Skel_SetVolumeStarts`). Compiling drafts in
`#if 0`: `Skel_SetRootMatrix` (registers of the float path), `func_0034FBA0`, `Skel_ProjectJoints` (one alignment
nop), `Motion_DecodeFrame` (registers only), `Motion_CopyFrame`. The drafts follow the original control flow, so the
behaviour above can be read from them. See [decomp_guide.md](../decomp_guide.md), "Lessons from T16 batch 1".

## 7. Open

- The meaning of the root channel 0 (an angle added to the facing `unk1A`) against `unk3C`, and of `unk11D`.
- What `func_00365300`, `func_00365248`, `func_003632E0`, `func_00362C28` and `func_003478F0` add (fighter_setup /
  geometry, still assembly); `func_00365108` adjusts the root angles. `func_00350728`, `func_0034E018` and
  `Light_UpdateFighter` are rendering state ([render.md](render.md)).
- The bone offsets (local translations) are set outside this module (character load, `func_00368168` [I]).
