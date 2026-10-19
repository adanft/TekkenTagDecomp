# Fighters, the fight simulation, practice mode and the CPU opponent

What the decompiled code shows about the fighter state, its input, the per-frame fight simulation
(`src/battle_flow.c`, module `battle_flow`, 0x3682F8-0x3745E8, section 5), the practice mode tools and the CPU
opponent (`src/fighter_core.c`, module `fighter_core`, 0x37BA88-0x38BF98). The `fighter_core` name came from the
first map ([game_overview.md](../game_overview.md) section 4); that code turned out to be practice mode and the CPU,
while the simulation is in `battle_flow` and the hit test in `fight_misc` (0x3770E0).

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
- Field 0x188 points to the move data of the current move, or is NULL; then `func_00368218(f, move id)` looks it up.
  Every reader goes through the same inlined helper (`Fighter_GetMove` in `src/fighter_core.c`): the original's
  extra `move a0, s0` before the call only appears with an inline function [V].
- Field 0x04 is the X position: the practice reset puts the two fighters at minus and plus half the distance
  `gPracReset.unk10` (default 0x4B0) and copies it to 0x51C; 0x3A is the facing angle (0x4000 / -0x4000) [V for the
  stores; I for the names].

## 2. Input of a fighter, one frame

`func_0036C680(Fighter *f)` (module `battle_flow`) fills the fighter's buttons each frame: it switches on the input
source `f->unk420` (forced to 2 when `D_005F6678` is set) and writes the buttons held at 0x432 and the buttons
pressed this frame at 0x436 [V]. The sources it calls:

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
      `unk14C`), runs the hit test `func_003770E0` (in `fight_misc`, reached through `func_003772B0`): up to 4 attack
      volumes of 0x18 bytes at `Fighter + 0x190` (start and end point) against the 14 body spheres of 0x14 bytes at
      `+0x1F0` of the other fighter (`func_00376CF8`), the contact going to `+0x39C`. A hit counts when the attack
      level `unkD0` meets the defender's mask `unkCC & 7` and the attack has not hit yet (`unkF1`); the attacker
      gets `unkF1`, `unkF4`, `unkF5`, `unk132`, the defender `unkF3`, `unk12F`, `unk14C` (the attacker move's byte
      0x2E) and `unk11C` (hit kind). The reaction is the attacker move's first `D_005E8978` entry whose condition
      holds (`func_0036C1A0`), field `unk6` when the distance `D_005F5C28` is below its `unk8`, else `unk4` [V read
      of the asm; I meanings].
   9. `func_00371C08` (attack volume motion), `func_003729B0`: **`Fighter_StartCommandMove`** for both, in parity
      order (section 5.2), `func_00372BA8`, `func_003720C8` (floats; effects [I]), fighter voices
      (`func_00375B60`), `Snd_UpdateDelayed`, the practice dummy (`func_0037D668`).
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

### 5.3 Moves, damage, round

- Starting a move (`func_00368818` and its variants) [V]: `unk184` = move id, `unk188` = `unkC0` = `unkB8` = its
  record (`func_00368218`), `unkC4` (frame) = `unk178` = 1, `unk17A` = `func_0036BE08(f, kind)` (kind 6 for plain
  moves, 0x24 thrown [I], 0x26 launched [I]), `unk17C`-`unk182` cleared, the buffered command dropped
  (`func_0036BD98`), then one `Fighter_StepMove`.
- `Fighter_StepMove` (0x36A428, 0x1828 bytes, asm with a jump table): counts `unk170` down,
  advances `unkC4`, holds the move while `unkE4` (hit freeze) runs, ends it at `unk178` / the record's length
  `unk18`, and starts the follow-up moves [I: partial read].
- Damage: the attacker's `unkCA` = `entry.unk2 & 0x3FFF` of the `D_005E8978` entry that applies, `unk11B` its kind
  (`entry.unk2 >> 14`, kind 1 means no damage) (`func_0036C228`) [V code; I "damage"]. The life is `unk40C`
  (`Battle_IsRoundOver`: a life at 0 or `gRoundTimer <= 0` ends the round) [V code; I names]. Where the life is
  reduced is not found yet.
- Round start: `func_0036D5D0` (battle init, from `Mode_Battle`) calls `func_0036E138` (round state reset) and
  `func_00373038` -> `func_00373080` per fighter: intro move `func_00368398` (move 4 or 6 from the held pad buttons,
  `rand() & 1` when none; 5 after the first round; 9 for character 0x21), input reset [V calls; I roles].

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
- Floats: the matched part uses integers only; `func_0036FB40` and `func_003720C8` (asm) read `.lit4` floats and
  sin / cos, so they need a check for float determinism on the PC port.

## 6. Open

- Where the life (`unk40C`) is reduced; the meanings of the move kinds (`unk17A`) and of `Fighter_StepMove`.
- `func_0036FB40` (movement / pushing [I]) and `Battle_UpdateRound` (round states) are in assembly (jump tables).
- Tag in / out: not identified in `battle_flow`; the per-slot words at `Fighter + 0xB5D0`-`0xB884`
  (`func_00373398`) and the 4 words `D_00879630` (`func_00373E18`) are candidates.
- The meaning of `Fighter.unk420` values other than 3 (practice playback) and of the move flag bits.
