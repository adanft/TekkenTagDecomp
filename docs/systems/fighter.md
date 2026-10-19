# Fighters, practice mode and the CPU opponent

What the decompiled code of `src/fighter_core.c` (module `fighter_core`, 0x37BA88-0x38BF98) shows about the fighter
state, its input, the practice mode tools and the CPU opponent. The module name came from the first map
([game_overview.md](../game_overview.md) section 4); the code turned out not to be the move interpreter or the hit
detection, which live elsewhere (callers in `battle_flow`, `fighter_setup`, `model_anim`; T14 follow-ups).

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

## 5. Open

- The per-frame fighter update, the move interpreter, hit detection, damage, throws and tag changes are outside this
  module; the next candidates are `battle_flow` (0x3682F8-0x3745E8, which holds `func_0036C680`) and `fighter_setup`.
- The meaning of `Fighter.unk420` values other than 3 (practice playback) and of the move flag bits.
