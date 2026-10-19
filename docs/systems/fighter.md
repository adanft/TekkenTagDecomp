# Fighters, the fight simulation, practice mode and the CPU opponent

What the decompiled code shows about the fighter state, its input, the per-frame fight simulation
(`src/battle_flow.c`, module `battle_flow`, 0x3682F8-0x3745E8, section 5), the hit test (`src/fight_misc.c`, module
`fight_misc`, 0x376C30-0x3790C8, section 5.5), the practice mode tools and the CPU opponent (`src/fighter_core.c`,
module `fighter_core`, 0x37BA88-0x38BF98). The `fighter_core` name came from the first map
([game_overview.md](../game_overview.md) section 4); that code turned out to be practice mode and the CPU, while the
simulation is in `battle_flow`.

Tags: **[V]** read in decompiled, matching C or in the disassembly, **[I]** inferred.

## 1. The fighter slot

- `gFighters` (0x879640) holds 4 slots of 0xB8A0 bytes (`Fighter`, [include/fighter.h](../../include/fighter.h)):
  slots 0 and 1 are the two sides on screen, 2 and 3 the tag partners [V for the count and the size; I for the
  roles]. `D_00884EE0` is slot 1 (0x879640 + 0xB8A0) [V].
- The opponent of a fighter is `&gFighters[(f->unk1E + 1) & 1]`: field 0x1E is the side (0 or 1) [V].
- Field 0xC0 points to the record of the current move (`struct FighterUnkC0`): a move id at 0x10 (compared with the
  fighter's own move id at 0x10C), flag words at 0x04, 0x08 and 0x24, and a frame window at 0x2C / 0x2D that the
  code compares with the fighter's frame counter at 0xC4 (`o->unkC0->unk2D >= o->unkC4`) [V for the accesses; I for
  the meaning].
- Field 0x188 points to the record of the move that starts next (the pending move, id in `unk184`), or is NULL; then
  `func_00368218(f, move id)` looks it up. `Fighter_StepMove` moves it to `unkC0` when the move starts and clears it
  (T14 b3) [V]. Every reader goes through the same inlined helper (`Fighter_GetMove` in `src/fighter_core.c`): the
  original's extra `move a0, s0` before the call only appears with an inline function [V].
- Field 0x04 is the X position: the practice reset puts the two fighters at minus and plus half the distance
  `gPracReset.unk10` (default 0x4B0) and copies it to 0x51C; 0x3A is the facing angle (0x4000 / -0x4000) [V for the
  stores; I for the names].

## 2. Input of a fighter, one frame

`func_0036C680(Fighter *f)` (module `battle_flow`, matching C since T14 b3) fills the fighter's buttons each frame: it
switches on the input source `f->unk420` (forced to 2 when `D_005F6678` is set) and writes the buttons held at 0x432,
those of the previous frame at 0x434 and the buttons pressed this frame (`held & (held ^ previous)`) at 0x436; bit
0x400 of the pressed buttons goes to `unk140`, and `Fighter_RecordInput` adds the frame to the history [V]. Sources
0 (and any value above 4) clear the buttons. Pad bit 0x04 is masked while `unk402` is set, and for the pad source it
is also set from bit 0x400 of the port word (bit 0x400 itself for character 0x21 with `unk418 == 0`) [V]. The
sources it calls:

- the pad of the side, through `func_0035B9E8` / `func_0036C578` / `func_0036CF88` [V calls];
- the CPU opponent: `func_00380A50(f, &f->unk432, &f->unk436)`, which jumps to `Cpu_GetInput` [V];
- the practice playback: `func_0037C950(side)` = `Prac_PlayStep(side, 0)`, whose result also goes to 0x430 [V];
- the practice recorder records the pad buttons through `Prac_RecInput` [V call].

`f->unk430` is also written by the practice dummy conditions (0x37DDD0-0x37E1D8): values 0x1000, 0x2000, 0x4000,
0x8000, 0xC000 and 0x60, which are pad direction and button bits; the dummy's guard and reaction settings select
one of these functions through jump tables [V for the values; I for "guard / reaction"].

## 3. Practice mode support (0x37BA88-0x3800D8)

- Cursor sound effects for the practice menus (`D_005F5F9C`, `D_005F5FA0`, sound effects 0x101B, 0x1039-0x103C) [V].
- Hit markers: four markers of 0x14 bytes (a world position projected to the screen with `func_0039BC08`, drawn by
  `func_003D0548` for 0x40 frames, fading out over the last 0x20) [V for the code; I for "hit marker"].
- The command recorder `gPracRecorder` (0x42F380, one `CmdRecorder` of 0x1248 bytes per side) [V]:
  - a ring of 0x200 steps `{u16 frames, u16 buttons}` at 0x226 (`Prac_RecInput` adds a step when the buttons masked
    with 0xF4F4 change, otherwise counts frames up to 0xFFFF);
  - two saved tracks of up to 0x100 steps at 0xA28 and 0xE38 (`CmdTrack`, 0x410 bytes), filled from the ring by
    `Prac_RecSaveTrack` between the start and end marks 0x10 / 0x14;
  - playback: `Prac_PlayBegin` / `Prac_PlayStep` return one step's buttons per frame and count its frames down;
    `Prac_IsPlaying` reads field 0.
- The command lists `gPracCmdLists` (0x8A7B90, 4 x 0x40D8 bytes, indexed by `Fighter.unk22`): up to 0x7B entries of
  0x42 bytes, up to 0x3DA steps of 8 bytes, and two index lists (16 and 0x80 entries) [V layout]. `Prac_LoadCmdIntoTrack`
  copies one entry's steps into track 0 of the recorder, so the dummy can play a listed command [V].
- `Prac_ResetPositions` places both fighters and resets their state (`func_00368818`, `func_003730E8`,
  `func_00373390`, `func_0037DA08`) [V calls].

## 4. The CPU opponent (0x3800D8-0x38BF98)

- State: `gCpuStates` (0x8B8078), one `CpuState` of 0x3E8 bytes per side. While a side thinks, `gCpu` points to its
  state, `gCpuSelf` to its fighter and `gCpuOpponent` to the other one (set by `Cpu_GetInput` and
  `func_003809C0`) [V].
- `Cpu_GetInput(f, &held, &pressed)` [V read, not matched yet]: unless `func_00393810` says the CPU may not act, it
  runs `Cpu_Think` (0x380F38), then returns the buttons `gCpu->unk6` and their rising edge against the previous frame
  (`unk8`).
- Randomness: `func_003800D8(n)` returns `n * ((rand() + seed) & 0x7FFF) >> 15` and steps its own seed
  `D_005F6C08 = D_005F6C08 * 5 + 3` [V]. The sound code uses it as well (voice choice), so it is a shared
  "random below n" helper.
- Moves are chosen from a candidate list `gCpuCands` (0x8B8848, 12-byte `CpuCand {s16 state; move record; command}`,
  `gCpu->unkAC` entries). `Cpu_PickCands(filter)` marks the candidates a predicate accepts (state 0 -> 1) and
  `Cpu_RejectCands(filter)` removes them (state -> -1), both keeping the picked count `gCpu->unkB0` [V]. About 40
  small predicates (0x383ED8-0x384D68) test the candidate's move flags (high / low / throw-like bits of
  `unk4` / `unk8`, the frame window) and its command [V code; I meaning of each bit].
- Commands: a command is a list of 16-bit words, direction in bits 0-3 (mapped through the table `D_005073F0`) and
  buttons in bits 8-11 (pad bits 0x80, 0x10, 0x40, 0x20); `Cpu_SetCommand` (0x3811E8) starts one and
  `func_00381418` advances it each frame [V read].
- Per-character behaviour: the table `D_00507A88` (12-byte `{key, handlerA, handlerB}`, 33 searched by
  `func_0038BEE0`) gives each character's move handlers; 28 of the handlers it points to are `return -1;` stubs (no special
  handling) [V].
- Difficulty: `func_00380420` copies a 0x80-byte parameter block from `D_00431918[level][kind]` (5 x 8 blocks) to
  `CpuState + 0x344` [V read; I "difficulty"].

## 5. The fight simulation (`battle_flow`)

### 5.1 One frame: `Battle_UpdateFrame` (0x36D860)

`Mode_Battle` calls `Battle_UpdateFrame` once per frame for the two fighters on screen, `gFighters[0]` and
`D_00884EE0` (slot 1). The function is matching C; the order below is [V], the roles of the callees still in
assembly are [I] unless marked.

1. `unk24 = unk0 * 2 + unk1E` for both fighters; `func_003C1B38()`; game type 7 (`D_0043B7F8[0]`) calls the
   practice hook `func_003796B8`.
2. Pause: while `D_005F5EE8` is set, `Battle_UpdatePause` (0x36DEA8) turns the Start button (0x800) of either side
   into the pause words `D_005F5F08` / `D_005F5EE4` and fades the BGM; otherwise the words are cleared.
3. When the game is paused (`D_005F5EE0` or `D_005F5EE4`), only the drawing side runs (animation, effects) and
   nothing below happens.
4. `D_005F5EF8 = func_00364B50()`, then `Battle_UpdateRound` (0x36E198, asm with a jump table; reads and writes
   `gRoundPhase`). When
   `D_005F5EF8` is set (a frame without simulation, e.g. a replay or an intro), only animation and effects step,
   by `func_00365D00()` frames.
5. Otherwise the simulation, in this order:
   1. `gBattleParity ^= 1`; round HUD [I] (`func_0036EA80`: "ROUND %d", "READY!", "K.O.", "TIME UP" ...);
      `func_0036DDD0`; camera shake step `func_0035C320` [I]; `func_00341AE8`; `func_0034F288` per fighter.
   2. **`Battle_StepMoves`** (0x372A88): the move step `Fighter_StepMove` (0x36A428) of both fighters, the one
      picked by `gBattleParity` first. When one of them reaches the end of a move of kind 0x20 (a throw [I]), the
      other first starts the move given by `D_0046BE78[entry.unk4].unk1C` (kind 0x24, `func_003688B0`).
   3. `Battle_UpdateFacing` (0x371330): `Math_Atan2` of the vector between the fighters (`unk51C` / `unk524`) into
      `unk3A` (the other side gets `+0x8000`); `unk48` comes from the camera angle `D_00415808[1]`.
   4. `Battle_UpdateDistance` (0x371458, asm) writes the fighter distance `D_005F5C28` [I]; `func_00371A48` the
      distances to the two points `D_00879610` (`D_005F5C38`).
   5. `func_0036FB40` (0x12E0 bytes, asm, `Math_Atan2` / `Math_Sin` / `Math_Cos`, `.lit4` floats): movement and
      pushing [I]; `func_003772F0` unless a fighter has `unkE0` set.
   6. Skeleton and animation side (`func_00371B18`, `func_00378150`, `func_00378600`, `func_003BAEF0`,
      `func_003A6390`), `Fighter_UpdateMotion` (position delta of the frame into `unk3C4`-`unk3CC`),
      `func_0034F3D8`, the camera targets (`func_003D3C68` with `unk51C`, `unk520`, `-unk524`, `unk122`).
   7. `Battle_ApplyVelocity` (0x371C68): while `gRoundPhase < 0xE`, adds `unk3E8`-`unk3F0` to both position
      triples `unk4`-`unkC` and `unk51C`-`unk524` of both fighters (not on frame 1 of move 0x1019).
   8. **`Battle_ResolveHits`** (0x371D68, asm): clears the hit flags (`unk12F`, `unk132`, `unk133`, `unk13B`,
      `unk14C`), runs the hit test (section 5.5) through `func_003772B0(a, b, &a->unk39C, &b->unk39C)`. A hit counts
      when the attack level `unkD0` meets the defender's mask `unkCC & 7` and the attack has not hit yet (`unkF1`);
      the attacker gets `unkF1`, `unkF4`, `unkF5`, `unk132`, the defender `unkF3`, `unk12F`, `unk14C` (the attacker
      move's byte 0x2E) and `unk11C` (hit kind). The reaction is the attacker move's first `D_005E8978` entry whose
      condition holds (`func_0036C1A0`), field `unk6` when the distance `D_005F5C28` is below its `unk8`, else `unk4`
      [V read of the asm; I meanings].
   9. `func_00371C08` (attack volume motion), `func_003729B0`: **`Fighter_StartCommandMove`** for both, in parity
      order (section 5.2), `func_00372BA8`, **`func_003720C8`: the damage of the frame** (section 5.6), fighter
      voices (`func_00375B60`), `Snd_UpdateDelayed`, the practice dummy (`func_0037D668`).
   10. **`Fighter_UpdateInput`** per fighter: when `unk414` is set, `func_0036C680` fetches this frame's buttons
       (pad, CPU or practice playback, section 2) and records them (section 5.2).
   11. `unkC6 = unkC4`; `func_00372A10`: `func_00368B90` and `func_00368B08` for both in parity order (the launch
       reaction, `func_00368958`: kind 0x26, record `func_00374010(func_00374048())` from `D_00500678`); end of frame
       helpers.
6. It returns `func_0036E158()` (`D_005F5E88`, the end-of-round step).

The input is read at the end of the frame (step 10), after the move step: what a player presses in frame N acts
in frame N+1 [V order]. For netplay the inputs of both sides enter only through `func_0036C680` (and the CPU,
`fighter_core`), so replacing its sources is enough to drive the simulation.

### 5.2 Input history and commands

- `Fighter.unk438` (`FighterUnk438`, include/fighter.h): a ring of 60 entries, one per frame
  (`Fighter_RecordInput` 0x36C860, `Fighter_ResetInputHistory` 0x36C528) [V]. An entry is the direction 0-15 of the
  held pad bits (`func_0036CFC0`) or'ed with the pressed buttons moved to bits 4-8 (pad 0x80 -> 0x10, 0x10 -> 0x20,
  0x40 -> 0x40, 0x20 -> 0x80, 0x04 -> 0x100) [V for the recorder; `func_0036C918` read, not matched]. Pad bit 0x04 is
  dropped while the fighters are far apart (`D_005F5C38[side] >= 0x1D15`) or `D_005F5EF4` is set [V].
- `Input_MirrorLeftRight` (0x36CF88) swaps the pad's left and right bits (0x8000 / 0x2000) for side 1 [V].
- A move record points (+0x0C) to its command list of `MoveCmd` entries (0xE bytes; shared lists in
  `D_0047F780`). `Move_FindCommand` (0x369CB0) walks it at the current frame `unkC4`: an entry applies when the
  frame lies in `unkB..unkC`, its input matches (`func_0036CBE8`), its condition `unk2` holds (`func_00369008`:
  character `unk28` / `unk2A`, side tables `D_00440940`, `unk418`, `unk124` ...) and `func_00369E10` agrees; markers
  0x8004 / 0x8005 enter and leave a shared list, 0x8000 ends it [V read of the asm; I meanings]. Input codes:
  without bit 0x8000 a direction / button test of this frame (`func_0036CD10`, `func_0036CD78`); 0x8001 / 0x8002
  a double tap forward / back (direction 6 / 4 twice, neutral 5 between, within 0x14 history entries,
  `func_0036CAF8`); 0x8003 a flag (`unk12B`); above 0x8005 a sequence from `D_004EFD44` (`func_0036CDD8`) [V code
  read; I names].
- `Fighter_StartCommandMove` (0x368A00): while `unk12F` is set, the first matching entry of the current move's list
  starts its move `unk6` with first frame `unkD`, kind `unk8 & 0x7F`, and clears the other fighter's hit flags;
  `unk148 = other.unkCA / 2 + 0x19` [V].
- The matched entry of the current move is kept at `unk4C0` (`MoveCmdState`), a buffered one at `unk4D4` and a copy
  for the next frame at `unk4E8` [V copies; I roles].

### 5.3 Moves and the move step

- Starting a move (`func_00368818` and its variants) [V]: `unk184` = move id, `unk188` = `unkC0` = `unkB8` = its
  record (`func_00368218`), `unkC4` (frame) = `unk178` = 1, `unk17A` = `func_0036BE08(f, kind)` (kind 6 for plain
  moves, 0x24 thrown [I], 0x26 launched [I]), `unk17C`-`unk182` cleared, the buffered command dropped
  (`func_0036BD98`), then one `Fighter_StepMove`. `func_0036BE08` (matching C) adjusts the kind to the current move:
  2 becomes 0x14 and 0x19 becomes 0x16 when `func_0036BEC0` holds, 0xC-0xF and 0x14 / 0x15 become 0xB when the
  move has no frame window [V].
- **`Fighter_StepMove`** (0x36A428, asm, 0x1828 bytes; read in T14 b3) [V unless marked]:
  - nothing happens while `unk127` is 0; `unk170` counts down to 0;
  - the pending move (`unk188`) starts when `unkC4 >= unk178` or the current record's length `unk18` is reached
    (`unkDA > 0`; `unkDA < 0` plays backwards to frame 0; `unkDA == 0` starts at once). A move never ends by itself:
    it runs until a pending move starts;
  - otherwise the frame advances: `unkC8++`; the hit freeze `unkE4` counts towards 0 (cleared when `unkFE` is set)
    and holds `unkC4` while non-zero; with `unkD6 >= 0` the frame `unkC0->unk1B` is held `unkD6` times; with
    `unkD6 < 0` `unkC4 += unkDA`;
  - when a move starts: the tag / character change of section 5.7, `func_003D2A38`, `func_0036BDA0`, then the
    resets (`unkD6 = -1`, `unkDA = 1`, `unkE4 = 0`, hit bytes cleared), `unk110` / `unk114` = the other fighter's
    position, the damage `unkCA` (`unk148` when set, else `func_0036C228`), the flags `unkCC` / `unkD0` / `unkD2` /
    `unkD4` / `unkD5` from the record, the hit freeze `unkE4 = unk14C` when the fighter was hit (`unk12F`) and still
    stands (`unk40C`);
  - a jump table on the move kind `unk17A` (`jtbl_004FFA80`, 0x2E kinds) sets the first frame (1, continue, last
    frame with `unkDA = -1` for kind 7), the facing `unk1A` from `unk3C` / `unk3A`, a state code `unk11E`, a
    position snap, turns spread over the move's frames (0xA, 0xB, 0x10, 0x12, 0x13), throw pairing with the other
    fighter's facing and position over `unkE0` frames (0x1F, 0x20 [I]), launches (0x1E, 0x26: `unkFE = 1`,
    `unk100++`), the reaction pick of kind 0x24 (`func_0036C1A0`) and the paths of kinds 0x25-0x2D;
  - common tail: `unk176` = old move id, `unk10C` = `unk184`, `unk184 = -1`, `unkC0` = `unk188`, `unk188` = NULL,
    `unk10E` = kind, `unk17A`-`unk17E` cleared.
- Round start: `func_0036D5D0` (battle init, from `Mode_Battle`; C attempt in `#if 0`) increments `gRoundNumber`, sets
  `gRoundTimer` in frames from the time setting `D_0043B7F8[0xB]` (0x870, 0xB04, 0x10A4, 0x1608, 0x1B30 for 0-3 and
  4 / 5), clears the round flags, copies each member's red life (`+0x10`) to `+0x2C`, and calls `func_0036E138`
  (round state reset) and `func_00373038` -> `func_00373080` per fighter: intro move `func_00368398` (move 4 or 6
  from the held pad buttons, `rand() & 1` when none; 5 after the first round; 9 for character 0x21), input reset
  [V calls; I roles].

### 5.4 Order, randomness and timing (determinism)

- **Alternating priority** [V]: `gBattleParity` flips every simulated frame, and `Battle_StepMoves`,
  `func_003729B0` and `func_00372A10` process `gBattleParity ? fighter 0 : fighter 1` first. Two machines must agree
  on the parity; its initial value is `gFrameCount & 1` at the round reset (`func_0036E138`), so the main-loop
  frame counter must be synchronised (or the parity forced) at round start.
- `rand()` (newlib, [game_overview.md](../game_overview.md) section 6) in the simulation path: the intro pose
  (`func_00368398`) when no button is held [V]. `gFrameCount % 3` picks the pose of character 0x21 in
  `func_003685B8` [V read].
- The camera angle `D_00415808[1]` enters `Fighter.unk48` (`Battle_UpdateFacing`) [V]; whether `unk48` feeds back
  into gameplay is open.
- `Math_Sin` / `Math_Cos` / `Math_Atan2` are integer table lookups (`gSinTable`), and `func_0036FB40` (movement
  [I]) uses only those [V]. The hit test is integer as well (fixed point 20.12, 64-bit products, `__divdi3`) [V].
- Floats in the simulation path [V]: (1) the red-life loss in `func_003720C8` (`(f32)red - (f32)(damage << 12) *
  0.6f` or `1.4f`, back with `cvt.w.s`), which decides when `unk40C` drops (K.O.); (2) the attack volume of character
  0x1D, whose angle comes from `atan2f` of two points moved with `sceVu0ApplyMatrix` (`func_00378760`). The EE FPU is
  not IEEE 754 (it rounds toward zero, has no denormals or infinities) and `atan2f` is newlib's: two PC peers agree
  if they run the same binary with SSE single precision, but matching a PS2 needs EE-style rounding [I: EE FPU
  behaviour from the R5900 manual, not measured here]. `func_00372BD0` and `Battle_UpdateFrame` use floats only for
  the camera targets (`func_003D3C68`).
- More randomness: `func_0036C278` / `func_0036C318` (character variant of a tag-like command, `unk182`) draw with
  `func_003800D8` (`rand()` plus its own seed `D_005F6C08`); the tag swap reloads the character through
  `func_0035C850` -> `Rng_PickRandomEntry` -> `Rng_SpinNext` (characters 0xF and 0x21) [V calls]. Netplay must keep
  `rand()`, `D_005F6C08` and `gRngSpin` in step on both machines.
- Frame counters that gameplay reads: `gFrameCount` (parity at round start, pose of character 0x21), the round timer
  `gRoundTimer` (frames, set by `func_0036D5D0`), the per-frame life recovery (`func_003C1900`) [V].

### 5.5 Hit test (`fight_misc`, matching C)

- `func_003772B0(a, b, &a->unk39C, &b->unk39C)` returns `(s16)func_003770E0(...)`: bit 0 when `a` hits `b`, bit 1
  when `b` hits `a` [V].
- `func_003770E0` tests the attacker's attack volumes (`AttackVol`, 0x18 bytes: a start and an end point, at
  `Fighter + 0x190`, `unk13C` of them, at most 4) against the 14 body spheres of the defender (`BodySphere`, 0x14
  bytes at `+0x1F0`: centre, radius `unkC`, radius squared `unk10`). Moves of kind 0x19, and of kind 0x18 for
  character 0x14, use the volumes of `func_003462F0(side)` instead (count `func_00346310`). A defender with `unk14E`
  set cannot be hit. The first touching pair wins: the defender's `HitContact` (`+0x39C`) gets the volume's end
  point, its direction (end - start, s16) and the sphere index [V].
- `func_00376CF8` (the volume / sphere test; C attempt in `#if 0`, read in full): a box reject per axis (x, z, y),
  then the segment is cut to the sphere's height band `y - r .. y + r` with 20.12 fixed-point steps, and the point of
  the cut segment closest to the centre is found in the x / z plane (64-bit products divided with `__divdi3`); it
  hits when that distance squared is at most `unk10`. Height only clips the segment: the final test is 2D [V].
- The sphere radii come from the per-character table `D_0042F098` (`func_00377F58`; `func_00377FB8` adds 10) [V];
  their centres are written by the skeleton code [I].
- Character 0x1D has an extra attack volume driven by the four functions `func_00378810`, `func_00378A28`,
  `func_00378C48`, `func_00378E70` (sin / cos of the facing, offsets from `D_00506248`-`D_0050625C`), started by
  moves 0x104F, 0x1051 / 0x1053, 0x1A48, 0x1054 (`func_00378150`) or by a request in `D_008A7B70`
  (`func_00378368`, `func_00378398`) and stored in `D_008A7AD0[side]` / `Fighter + 0x190` (`func_003780C8`,
  `func_00378500`) [V code; I "projectile"].
- The two fighters push each other apart in `func_00377338` (asm, via `func_00377AE0` from `Battle_ApplyVelocity`):
  eight 0x10-byte push spheres at `Fighter + 0x308` per fighter, the result written to the velocities
  `unk3E8`-`unk3F0` [V for the fields; I for the meaning].

### 5.6 Damage and life (`func_003720C8`, asm)

`func_003720C8(a, b)` runs once per frame (step 9) and treats each fighter in turn as the defender `d` of the other
`o` [V read of the asm; I for the names]:

- **Life** is `d->unk3F4`, a 20.12 fixed-point value (damage is subtracted as `damage << 12`); the **red
  (recoverable) life** is `unk3F8`, the maximum `unk3FC`; all three are set at round start from the team member's
  record (`func_003C1470`, which also sets `unk40C = 1`). The life is clamped to the red life every frame.
- **`unk40C` is not the life**: it is a "still standing" flag. It is cleared when the red life reaches 0, or when the
  life reaches 0 (except for character 0x21 with `unk401 == 0`, see `func_003AE3C0`); a fighter with `unk40C == 0`
  gets `unk138 = 1` and the first one calls `func_00364678` (the K.O. [I]). `Battle_IsRoundOver` reads it.
- The damage is `|o->unkCA|` (the attacker's move damage, `D_005E8978` entry, `func_0036C228`):
  - guarded (`d->unkD2 & o->unkD0`, `d->unk40 < 0x4000`): chip damage `o->unkCA * 0xA000 / 100` (one tenth, 20.12)
    only when the option `D_0043B7F8[0xC]` is on or the attacker is in a juggle (`unk16A`);
  - a counter hit (`d->unk13B`) adds half; a defender starting its own attack (`d->unkCA` set, before its window
    `unkC0->unk2C`) or airborne (`unk16A`, `unk12E`) takes `damage * 12 / 10` plus 3/10 of its own move's damage
    (scaled by its frame); in a juggle (`unk135` or move flag 0x400) the first hit is scaled by 80/100 and sets
    `unk12A`, later ones (`unk100`) by 1/2; the attacker's `unk400` (ticked by `func_003C1B38` [I: rage]) by
    13/10 [V arithmetic; I names];
  - `life -= damage << 12`; the red life goes down by `(damage << 12) * 0.6f` (`D_005F5168`), or `* 1.4f`
    (`D_005F516C`) when `unk12A` is set, and never below the life;
  - `func_0035C0D8` / `func_0035BFF8` (hit / guard effects [I]) take the contact `unk39C`; `func_003CF278` gets
    both members' character and costume bytes, and its result is added to `D_00440968[side][member]` [I: stats].
- While the round is over (`D_005F5EF4`) the life is put back to its value at the start of the frame; in practice
  mode (`D_0043B7F8[0] == 7`) life and red life are reset to the maximum every frame.
- **Recovery** (`func_003C1900`, from `func_003D3528` at the start of every simulated frame, outside `battle_flow`):
  the active fighter's life and red life are copied to its member record (`D_00440940[side][unk0]` `+0xC` / `+0x10`),
  and the partner's record regains `max / 6000` per frame up to its red life; the active fighter regains life only
  for character 0x21 [V].

### 5.7 Tag in / out (outside `battle_flow`)

- The tag is a command (`MoveCmd.unk8` bit 0x2000 with bits 7-12 zero) whose move flags `func_00368B90` copies to
  `unk180` / `unk182`; pad bit 0x04 (history bit 0x100) is the tag button [I] (Fighter_RecordInput drops it while the
  fighters are far apart or the round is over) [V].
- When the pending move starts, `Fighter_StepMove` (0x36A764) calls **`func_003D3288(side)`** (module
  `select_results` range, asm), unless the round is over (`D_005F5EF4`: it then starts the list's end move): it flips
  `unk0` and `unk2` (`^= 1`) of the same slot, reloads life / red life from the incoming member's record, reloads the
  character (`func_0035C8D0`, `func_0035C850`, `func_00377F58` body spheres ...), sets the per-side tag state
  `D_01FB6ED0[side] = 2` and `Fighter + 0xB134 = 1`; `Fighter_StepMove` then re-aims (`Battle_UpdateFacing`) and
  looks the pending move up again for the new character [V]. Slots 2 and 3 of `gFighters` are not used for it [V].
- With `unk182 != 0` the same command changes the character variant instead (`func_0036C318`, `func_0036C278`, both
  random picks with `func_003800D8`; matching C / `#if 0`) [V].
- The maximum life `unk3FC` is not reloaded on a tag [V]; `func_003F0008` (under `Mode_Battle`, driven by the table
  `D_01FC61E0`) also calls the swap [I: scripted tag].

## 6. Open

- The meanings of most move kinds (`unk17A`) and move flag bits; the per-kind effects of `func_00377B58` (switch on
  `unkC0->unk30`, tables `D_0042EFD8` -> `D_00506150`-`D_005061A0`).
- `func_0036FB40` (movement [I]), `Battle_UpdateRound` (round states), `Fighter_StepMove` and `func_003720C8` (damage)
  are still assembly (large functions with jump tables).
- The meaning of `Fighter.unk420` values other than 3 (practice playback); what `func_003F0008` (scripted tag swap)
  serves.
