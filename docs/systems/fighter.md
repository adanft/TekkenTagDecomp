# Fighters, the fight simulation, practice mode and the CPU opponent

What the decompiled code shows about the fighter state, its input, the per-frame fight simulation
(`src/battle_flow.c`, module `battle_flow`, 0x3682F8-0x3745E8, section 5), the hit test and the push
(`src/fight_misc.c`, module `fight_misc`, 0x376C30-0x3790C8, section 5.5), the practice mode tools and the CPU opponent
(`src/fighter_core.c`, module `fighter_core`, 0x37BA88-0x38BF98), and the life and tag functions of two other modules
(`src/select_results.c`, 0x3C1470-0x3C1A78, and `src/model_anim.c`, 0x3D3288-0x3D33F0, sections 5.6 and 5.7). The
`fighter_core` name came from the first map ([game_overview.md](../game_overview.md) section 4); that code turned out to
be practice mode and the CPU, while the simulation is in `battle_flow`.

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
      order (section 5.2), `func_00372BA8`, **`Fighter_ApplyDamage` (0x3720C8): the damage of the frame** (section
      5.6), fighter voices (`func_00375B60`), `Snd_UpdateDelayed`, the practice dummy (`func_0037D668`).
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
- **`Fighter_StepMove`** (0x36A428, asm, 0x1828 bytes; read in T14 b3; an m2c draft with fixed types is in `#if 0`
  since T14 b4, far from matching) [V unless marked]:
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
    `unk10E` = kind, `unk17A`-`unk17E` cleared; `unk118` = 2 / 1 / 0 from the new record's window and bytes 0x2A /
    0x28; `unkC8` = 1 (or the frame when `unk134` is clear).

  The kinds of the jump table, as the draft reads them ("continue" = `func_0036BF00` keeps the frame running, else
  frame 1, clamped to the record's length; "snap" = `unk4` / `unkC` set to `unk51C` / `unk524`; `unk11E` is the
  state code) [V code read; I meanings]:

  | Kind | Effect |
  |---|---|
  | 0 | frame 1, facing `unk3A` (body and move), snap |
  | 1-3 | continue; `unk11E = 0xB`, snap, `unk126 = 1`; 1 takes the facing from `unk3C` |
  | 4, 5 | frame 1, `unkBC` + 1, `unkD6 = unkD8`; 4 turns to `unk3A` over the window, 5 sets `unk11E` 4 or 6 by `unk40` |
  | 6 | plain move: frame 1, facing `unk3C`, snap when the record's flag bit 19 is set |
  | 7 | starts at the last frame and plays backwards (`unkDA = -1`) |
  | 8 | one frame back (at least 1), backwards, snap |
  | 9, 0x18 | facing `unk44`; `unk11E` 5 / 8 |
  | 0xA, 0xB | turn to `unk3A` (or its back when `unk40 >= 0x4000`) over the window, at most 16 frames |
  | 0xC-0xF | `unk11E` = 4 / 3 / 2 / 1 when `unk40 < 0x4FA4`, else 6 (0xF always 1) |
  | 0x10, 0x12, 0x13 | turns spread over the window, `unkF0 = 1`; 0x13 also turns the body by 0x8000 |
  | 0x14, 0x15, 0x16, 0x19 | continue, snap; `unk11E` 4 / 2 or 6, 8 |
  | 0x17, 0x1C, 0x1D | `unk11E` 9, 0xA, 0xA; 0x1C sets `unk170 = 0x18` |
  | 0x1A, 0x1B | continue; `unk11A` = 1 / 2 |
  | 0x1E, 0x26 | launch: frame `unk178`, `unkFE = 1`, `unk11D = 1`, `unk100++`, `func_00374148`, `unk160 = func_0036A2D0`; 0x1E also queues move `func_00374010(4)` (`unkFA = 1`, `unkFC = 4`) |
  | 0x1F, 0x20 | throw pairing: `unkE0` counts frames; on the first one the fighter turns towards the other (by `o->unk42`: +0x4000, 0x8000, -0x4000) over 16 frames and takes its position |
  | 0x21, 0x22, 0x23 | facing `unk3A` and snap (0x21); `unk11E` 8, 0xC, 0xD |
  | 0x24 | thrown / hit reaction: `unk120` from the reaction entry (`func_0036C1A0`), body turned by 0x8000, `Snd_StopVoiceSeq`, `unkE0` counts down, `func_0036A330` |
  | 0x25, 0x27-0x2D | paired paths read through `unk18C` (`struct FighterUnk18C`) into `unk150`-`unk158`, `unk156` the angle; 0x29-0x2D call `func_0036C128` |
  | other | frame 1, facing `unk3A` |
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
- Floats in the simulation path [V]: (1) the red-life loss in `Fighter_ApplyDamage` (`(f32)red - (f32)(damage << 12) *
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
  `gRoundTimer` (frames, set by `func_0036D5D0`), the per-frame life recovery (`Battle_RecoverLife`) [V].

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
- **Push** (`Fighter_Push`, 0x377338, via `func_00377AE0` from `Battle_ApplyVelocity`; C draft in `#if 0`, T14 b4)
  [V read of the asm and the draft; I meanings]: each fighter has eight push spheres (`PushSphere`, 0x10 bytes at
  `Fighter + 0x308`: a centre and a radius `unkC`, 0 = off). The velocities `unk3E8`-`unk3F0` of both are cleared;
  the push depth is the largest `r_a + r_b - sqrt(d^2)` over the pairs of active spheres that overlap on all three
  axes. When both moves are inside a frame window (`unkC0->unk2C`), the two points 200 units ahead of each fighter
  along its facing (`unk1A`) are less than 500 apart on x and z and the fighters move towards each other, the depth
  is at least 400 and the push is "hard". The depth is split between the two by how far each moved since
  `func_003772F0` saved the positions (`D_008A7A90` / `D_008A7AA0`, through `func_003985D8` = integer sqrt), each
  pushed along its own facing turned towards the other (`func_00366108`, angle between the fighters); the result is
  the velocity, halved unless the push is hard. Integer math only (`gSinTable`).
- **Push radii** (`Fighter_SetPushRadii`, 0x377B58, matching C): `func_00377AE8` calls it for both fighters with the
  current move's `unkC0->unk30` (kind 6 of the attacker forces 6 on the defender). The radii come from the
  per-character table `D_0042EFD8[unk2A]` (all four point to `D_00506150`); `kind & 0xF` picks a rule: 1 keeps spheres
  0 and 3 (others radius 1), 5 / 6 the same at half size, 2 makes all of them tiny (1), 3 halves them, 4 turns them
  off, 7 enlarges sphere 0 by 60 and makes 4-7 tiny, 9 enlarges sphere 0 by 60 and makes the others tiny, 8 enlarges
  all by 100, 10-15 use the table; kind 0 turns off the spheres whose bit `kind >> (4 + i)` is set, always or (bit 12)
  only from 8 frames before the window to its end. Then, by the current move: spheres 1 and 2 are off while it has a
  window, spheres 6 and 7 with `unk135` and `unkF3`, spheres 4-7 for move flag 0x400 (`unk24`) [V].
- The special attack volume of character 0x1D (kinds 1-4, T14 b4): `func_00378810` / `func_00378A28` move it 200 /
  102 (204 while `func_003ABCD0` holds) units a frame along the facing, `func_00378A28` also 110 / 220 up;
  `func_00378C48` drops it for 8 frames (800 a frame down to -6400) then sweeps it from side to side by 0x200 a frame
  (`D_005F5F5C`, between -0x2FFF and 0x2FFF) with a random length `rand() & 0x3FF`; `func_00378E70` tilts it by
  -0xAAA at 300 units. `func_00378600` draws it between its two points (`func_003A8D08`) with a size that shrinks
  over the move's window [V, matching C except `func_00378810`].

### 5.6 Damage and life (`Fighter_ApplyDamage`, matching C since T14 b4)

`Fighter_ApplyDamage(a, b)` (0x3720C8) runs once per frame (step 9). It first clears the per-frame hit outputs of both
fighters (`unk130` guarded, `unk131` hit, `unk138`, `unk13A`, `unk142`, `unk144`, `unk146`), then treats each fighter
in turn as the defender `d` of the other `o` [V; names I]:

- **Life** is `d->unk3F4`, 20.12 fixed point (whole damage points are subtracted as `damage << 12`); the **red
  (recoverable) life** is `unk3F8`, the maximum `unk3FC`.
- **Scripted damage** (`unk13D`, before the hit): 1 takes `o->unk14A` (or `o->unkCA` when 0), 2 the defender's own
  `unk14A` / `unkCA`, both times 13/10 when the attacker's `unk400` is set and neither fighter is airborne
  (`unk16A`); the red life loses 0.6 of it. 3 heals: `life += (d->unk14A or o->unkCA) << 12`.
- **A hit** (`d->unk12F`): `damage = |o->unkCA|` (the attacker's move damage, `D_005E8978` entry, `func_0036C228`).
  - Guarded (`d->unkD2 & o->unkD0` and `d->unk40 < 0x4000`): chip damage `o->unkCA * 0xA000 / 100` (one tenth, in
    20.12) only when the option `D_0043B7F8[0xC]` is on or the attacker is airborne; it also sets
    `D_005F6670[side]`. A chip that leaves life calls `func_0035BFF8(2, contact)` (guard effect [I]) and sets
    `unk130`; a chip K.O. calls `func_0035C0D8` and sets `unk131`.
  - Otherwise: a counter hit (`d->unk13B`) adds half. When the defender started its own attack (its `unkCA` set and
    its frame before its window `unk2C`), either fighter is airborne, or `d->unk12E` is set with `unk100` and `unkF7`
    clear: `unk13A = 1` and `damage = damage * 12 / 10 + extra`, `extra` being 3/10 of the damage when someone is
    airborne or `unk12E` is set, else 3/10 of the defender's own move damage times its frame divided by its window
    start. In a juggle (`d->unk135` or move flag 0x400): `unk100 == -1` (first hit) and `unk100 == 0` scale by
    80/100 and set `unk12A` (for 0 only when `func_003AE3C0(o)` is 0), later hits halve it; the result is truncated
    to 16 bits. Then the attacker's `unk400` multiplies by 13/10 when neither is airborne [I: rage].
  - `life -= damage << 12`; the red life loses `(damage << 12) * 1.4f` when `unk12A` is set (never below the life),
    else `* 0.6f` (the only floats of the function, its `.lit4` 0x5F5168).
  - `func_0035C0D8(max(damage, original damage), d, 1 - i, contact)` (hit effect [I]), `unk131 = 1`, `unk146` /
    `unk144` add the damage; `D_00440940[side][unk2].unk28` adds `func_003CF278(character and costume of member unk2,
    of member unk0, 4, 1)`. A hit of 0 damage only sets `unk130`, adds the same with kind 6 and calls
    `func_0035BFF8(1, contact)`.
- While the round is over (`D_005F5EF4`) the life is put back to its value at the start of the frame; in practice
  mode (`D_0043B7F8[0] == 7`) life and red life are reset to the maximum every frame.
- **`unk40C` is not the life**: it is a "still standing" flag, cleared when the red life reaches 0, or when the life
  reaches 0 unless `func_003AE3C0(d)` returns 0 (character 0x21 with `unk401 == 0`; other characters return -1). The
  life is then clamped to the red life. Last, a fighter with `unk40C == 0` (round not over) gets `unk138 = 1`, and
  the first one (`D_005F5EFC` clear) calls `func_00364678` with the other fighter (the K.O. [I]).
  `Battle_IsRoundOver` reads `unk40C`.
- **Round start** (`Battle_InitTeamLife`, 0x3C1470, matching C, `src/select_results.c`): for each side and member
  whose flag `D_0043B7F8[0x3C + side * 2 + member]` is 1 (`func_003AE420`, cleared by `func_003AE400`), the record's
  maximum and red life come from `func_003C1350(character, side)` and its life from the maximum (`+0x18`, `+0x1C`
  cleared); every member's `unk28` restarts at 100, and the side's fighter takes the life, red life and maximum of
  its active member `unk0` with `unk40C = 1` [V].
- **Recovery** (`Battle_RecoverLife`, 0x3C1900, matching C, from `func_003D3528` at the start of every simulated
  frame) [V]: only while `func_0036E158()` is 1 (the round runs), the game is not paused and `func_003A9698` is 0.
  The active fighter regains `max / 6000` per frame up to its red life only when `func_003AE3C0` is 0 (character
  0x21 with `unk401 == 0`) and `unkD2` is 0; its life and red life are copied to its member record
  (`D_00440940[side][unk0]` `+0xC` / `+0x10`); the partner's record regains `max / 6000` per frame up to its red life.

### 5.7 Tag in / out (outside `battle_flow`)

- The tag is a command (`MoveCmd.unk8` bit 0x2000 with bits 7-12 zero) whose move flags `func_00368B90` copies to
  `unk180` / `unk182`; pad bit 0x04 (history bit 0x100) is the tag button [I] (Fighter_RecordInput drops it while the
  fighters are far apart or the round is over) [V].
- When the pending move starts, `Fighter_StepMove` (0x36A764) calls **`Fighter_TagSwap(side)`** (0x3D3288, module
  `model_anim`, matching C in `src/model_anim.c` since T14 b4), unless the round is over (`D_005F5EF4`: it then starts
  the list's end move) [V]. The swap, in order: the incoming member is the old partner `unk2`; `unk0` and `unk2` of
  the same slot flip (`^= 1`); the life and red life come from the incoming member's record (`+0xC`, `+0x10`);
  `func_003C1A78(side)` (the outgoing member's `unk28` back to 100, a check of the incoming one's against 120 [I]);
  `func_003D31F8(side, 2)` sets the per-side tag state `D_01FB6ED0[side] = 2`; the character and costume of the new
  active member are loaded (`func_0035C8D0(f, character, costume)`, `func_0035C850(f, 1)`), then its body
  (`func_00368168`, `func_00377F58` body spheres, `func_003465F0`, `func_00344318`, `func_003A82C8`,
  `func_00370E20(f, 1)`, `func_0038ADF8(side)`); in practice mode (game type 7) `func_0037A970(f)` and
  `func_0037EA08()` follow. `Fighter_StepMove` itself sets `Fighter + 0xB134 = 1`, re-aims (`Battle_UpdateFacing`) and
  looks the pending move up again for the new character [V]. Slots 2 and 3 of `gFighters` are not used for it [V].
- With `unk182 != 0` the same command changes the character variant instead (`func_0036C318`, `func_0036C278`, both
  random picks with `func_003800D8`; matching C / `#if 0`) [V].
- The maximum life `unk3FC` is not reloaded on a tag (`Fighter_TagSwap` copies only `+0xC` / `+0x10`) [V];
  `func_003F0008` (under `Mode_Battle`, driven by the table `D_01FC61E0`) also calls the swap [I: scripted tag].

## 6. Open

- The meanings of most move kinds (`unk17A`, table in section 5.3), of the state code `unk11E` and of the move flag
  bits; what the push-radius kinds (`unkC0->unk30`) stand for.
- `func_0036FB40` (movement [I]), `Battle_UpdateRound` (round states) and `Fighter_StepMove` are still assembly
  (large functions with jump tables); `Fighter_StepMove` and `Fighter_Push` have C drafts in `#if 0`.
- What `D_00440940[side][member].unk28` counts (100 at round start, raised by `func_003CF278` per hit taken, compared
  with 120 by `func_003C1A78`); the meaning of `unk400` (13/10 damage) and of `D_005F6670[side]` (set by chip damage).
- The meaning of `Fighter.unk420` values other than 3 (practice playback); what `func_003F0008` (scripted tag swap)
  serves.
