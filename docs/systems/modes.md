# Game modes, menus and the game setup

The mode dispatch, the flow between screens, the game types and what each one sets for the fight, the options, the
unlock flags and their save block. First pass of T18 (batch 1); the C files are `src/modes.c`, `src/modes_b.c` and
`src/battle_mode.c` (section 6). Labels as in [../game_overview.md](../game_overview.md): **[V]** read in the code,
**[I]** inferred.

## 1. Dispatch

`Mode_Dispatch` (`src/main.c`) runs one handler per frame from the 17-entry table `jtbl_004FB970` on `gMode`
(0x5F5C20, s16); each handler has its own `switch (gSubMode)` (0x5F5C22) [V]. Two ways change the mode:

- `Mode_Change(next)` 0x3B90C0 [V]: stops the sound effects (`func_003753D0`) and the BGM, stores `next` in `D_005F6509`
  and the old mode in `D_005F650A`, then sets `gMode` to `next` directly when `next` is 3 and a movie or loading is still
  running (`func_003BDEE0` / `func_003BDEB8`), or when leaving the options (9) or the gallery (16); otherwise to 1
  (`Mode_Transition`, which fades and loads, then enters `D_005F6509`); `gSubMode = 0`. Unless `next` or the current mode
  is 3, 6 or 7, it first calls `func_003992C8(0)` [I: screen fade].
- Direct stores to `gMode` / `gSubMode` (most screen-to-screen steps below) [V].

| Mode | Handler | Identity |
|---:|---|---|
| 0 | `Mode_Boot` 0x3B9628 | boot screens, memory card load; `src/modes.c` [V] |
| 1 | `Mode_Transition` 0x3B9178 | fade / load, then `D_005F6509` [V] |
| 2 | `Mode_Title` 0x3BCC88 | title, copyright, attract cycle [V] |
| 3 | `Mode_MainMenu` | main menu [V] |
| 4 | `Mode_Battle` 0x35A6A0 | every fight of every game type [V] |
| 5 | `Mode_CharSelect` 0x3CB900 | character select; returns to `gMode = setup[0x12]`, `gSubMode = setup[0x13]` [V] |
| 6 | `func_003C8A50` | name entry set-up, then mode 7 [V code, I role] |
| 7 | `Mode_NameEntry` | name entry (`NameEntry_Init`), ends with `Mode_Change(3)` [V] |
| 8 | `func_003ABBD0` | ending / movie player: entered through `func_003ABDA0(ret, a, b)`, ends with `Mode_Change(2)` when `ret` is 1, else `gMode = ret` [V code, I role] |
| 9 | `Mode_Options` 0x3B4B20 | options [V] |
| 10 | `Mode_Results` 0x3B6258 | results and records; to mode 6 for a new record of game types 3 / 5 [V] |
| 11 | `Mode_Loading` | "LOADING..." screen of the attract path, entered by `Mode_EnterLoading(target)` from the title, the theater and `func_003A9690`; ends with `Mode_Change(3)` or `func_003BDF08` (title, sub-state 6) [V] |
| 12 | `func_00354BE0` | theater set-up, then mode 13 [V] |
| 13 | `Mode_Theater` 0x354D68 | theater menu [V] |
| 14 | `func_00355D30` | music player set-up (`Bgm_Stop`), then mode 15 [V] |
| 15 | `Mode_MusicPlayer` 0x355D90 | music player [V] |
| 16 | `Mode_Gallery` 0x3E5A98 | screenshot gallery [V] |

## 2. Flow

```
boot (0) -> title (2) -> main menu (3) --Game_Start(type, sides)--> fight (4, sub 0)
fight (4, sub 0) -> character select (5) -> fight (4, sub 1) -> ... match end handler per game type
  arcade: next stage or continue ("CONTINUE? %d", `Game_UpdateContinue`) / game over -> ending (8) / name entry (6, 7)
  results (10) -> name entry (6, 7) for a new record -> main menu (3)
main menu -> options (9), theater (12 -> 13, 14 -> 15), gallery (16); title idle -> attract demo (game type 9)
```

- **Main menu** `Mode_MainMenu` [V]: the menu table `D_0051EFE0` (12 entries of 8 bytes: name, id, locked flag,
  colour, kept in the executable) lists the entries ARCADE, VS BATTLE, TEAM BATTLE, TIME ATTACK, SURVIVAL, two 1-on-1
  entries (ids 5 and 6), PRACTICE, TEKKEN BOWL, THEATER, GALLERY, OPTIONS. Ids 0-8 call `Game_Start(id, sides)`, so
  the menu id is the game type; OPTIONS sets `gMode = 9`, THEATER 12, GALLERY 16. 480 idle frames return to the title
  (`Mode_Change(2)`). Ids 1 and 6 assign the pad ports first (`func_003BCFB8`).
- **`Game_Start(type, sides)`** [V] writes the game type to `D_0043B7F8[0]` (the game setup, section 3), clears the
  per-game fields (stage counter `+0x24`, play time `+0x2C`, 0x130 bytes at `+0x40`), clears both sides' life
  (`Fighter_ClearLife`) and team tables, marks the human sides (`D_005F5CB0[side] = 1`), runs the per-type set-up through
  `jtbl_0051E9D0` (section 3) and sets `gMode = 4`, `gSubMode = 0` itself.
- **Fight** `Mode_Battle` [V]: sub-state 0 stores `setup[0x12] = 4`, `setup[0x13] = 1` and sets `gMode = 5` (character
  select), except for game type 9; there is no separate loading mode on this path, the loads are sub-states of mode 4.
  At the end of a match `jtbl_004FBB00` picks a handler by game type: 0 `func_00359C40` (arcade: next stage or continue),
  1 and 6 `func_00359FB8` (`gMode = 4`, back through the character select), 2 `func_0035A280`, 3 `func_0035A310`, 4
  `func_0035A330`, 5 `func_0035A460`, 7 `func_0035A458`, 8 `func_0035A468` (`gMode = 10`). Other exits: `gMode = 6`
  (name entry), `gMode = 10`, `Mode_Change(3)` (sub-state 0x19, and through `func_003B7A68`), and the ending through
  `func_003ABDA0(1, ...)` with per-character file numbers when `D_005F5CF0` is set [V calls, I roles].
- **`Mode_Battle` sub-states** [V code, I roles]: 0 set-up (type 9 starts the demo, the others go to the character
  select); 1 per-type set-up callbacks (`&D_0043B7F8` passed); 2 match set-up (human / CPU sides from `D_005F5CB0` /
  `D_005F5CB2` into `setup[0x1A]`-`[0x1D]`, the opponent, `Hud_Init`); 3 wait for `func_003C45B0`; 4 CPU sides
  (`func_0036D010`); 5 BGM from the stage table `D_004FB9C0`; 6 round set-up (the intro runs on round 1 only, not in types
  5, 6, 9); 7 intro; 8 fight (`Battle_UpdateFrame` until it returns 5 or more, 6 quits), then the match-end handler; 9
  next match; 10-11 continue (`Game_UpdateContinue`, 9 to 0 with a voice per digit, buttons speed it up); 12-13 "GAME
  OVER" for 150 frames (not in types 2, 4, 8), then mode 6; 14-15 "A NEW CHALLENGER!" for 82 frames; 16 end of the demo
  (after 0xE11 frames); 17 results (mode 10); 18-21 survival and team result screens; 22 and the default
  `Mode_Change(3)`; 23-25 delayed exits. Arcade (type 0) takes its opponents from `D_00440A00[setup[0x24]]` (two characters,
  costumes, stage); after 8 wins `D_005F5CF0 = 1` and the ending follows. Survival draws a random CPU pair
  (`func_003596D8`, pair rules `D_005207F0`, 8-entry history `D_005F72A0`); Tekken Bowl uses stage 0x14 and
  `func_003F0008` instead of `Battle_UpdateFrame`. The stage is `D_005F5CB6`, the characters / costumes `D_005F5C98` /
  `D_005F5CA0` `[side][member]` and the team records `D_00440940`.
- **Title** `Mode_Title` [V]: rotates an attract variant in `D_005F6508.unk5` (0 = the loading screen, mode 11; 1 and 2 =
  `func_003C9358`); sub-state 10 starts a demo game, `Game_Start(9, 0)`; START leads to `Mode_Change(3)`.

## 3. Game types and the game setup

The game setup is the byte block `D_0043B7F8` (`.data`) [V layout of the fields below]: `+0` game type, `+0xB` round
time setting (0-3 timed, 5 no limit; the frames are in `Battle_UpdateRound`, [fighter.md](fighter.md) section 5.11), `+0xC`
guard damage, `+0xD` CPU difficulty level, `+0x12` / `+0x13` mode and sub-state to return to after the character select,
`+0x1A` matches of this game, `+0x1E` fight count (wins needed = `+0x1E` + 1, at most 5, and the most rounds `2n - 1`,
`battle_flow.c`), `+0x24` stage counter, `+0x2C` play time (u32), `+0x52 + side * 0x1E` team member states (team
battle), `+0x3C + side * 2 + member` partner flags.

| Type | Menu entry | Set-up (`jtbl_0051E9D0`) | What it sets [V] |
|---:|---|---|---|
| 0 | ARCADE | `func_003B9AE0` | all fight settings from the options; `[1] = [2] = [4] = 1` |
| 1 | VS BATTLE | `func_003B9B90` | time and guard damage from the options, difficulty 1, `[5] = 1`, per-side handicap words `[0x40/0x42/0x44 + 6 * side]` |
| 2 | TEAM BATTLE | `func_003BA080` | time, guard damage and difficulty from the options, `[5] = [6] = 1`; the HUD draws team squares |
| 3 | TIME ATTACK | `func_003B9E08` | time 2, difficulty 1, guard damage 0 |
| 4 | SURVIVAL | `func_003B9FE8` | time 2, difficulty 1, `[6] = 1`; the CPU level comes from the win count (section 5) |
| 5 | 1-on-1 entry (id 5) | `func_003B9EA8` | like arcade |
| 6 | 1-on-1 VS entry (id 6) | `func_003B9C80` | like VS |
| 7 | PRACTICE | `func_003B9D70` | time 5 (no limit), difficulty 1; practice hooks in the fight ([fighter.md](fighter.md)) |
| 8 | TEKKEN BOWL | `func_003B9F50` | time 2, difficulty 1; the bowling game of `tekken_bowl` |
| 9 | (attract demo) | `func_003BA118` | `[7] = 1`, time 2, difficulty from the options, `D_005F5CBC = 1` (the CPU's special block) |

The side status text over the fight (`func_003BC048` / `func_003BC370`, `src/modes_b.c`) follows the type: "STAGE%2d" /
"FINAL" and the time for arcade, "TIME ATTACK" (3), "WINS %d" or "SURVIVAL" (4), "TEAM BATTLE" (2), nothing for practice
(7) and Tekken Bowl (8), "GAME OVER" for the demo (9) [V].

## 4. Options and unlocks: the save block `D_00439C60`

The options and the unlock state are one 0x68-byte block, `D_00439C60` [V]. The options menu's item table (`D_00439CE0`)
points each item at its byte; `func_003B1488` copies them into the `D_005F5Cxx` words that the set-up functions above
read:

| Block byte | Option | Copied to | Setup byte |
|---|---|---|---|
| `+0x26` | difficulty level | `D_005F5C58` | `+0xD` |
| `+0x27` | fight count | `D_005F5C5A` | `+0x1E` (per match, `func_00359038` / `func_00359380`) |
| `+0x28` | round time | `D_005F5C66` | `+0xB` |
| `+0x29` | character change at continue | `D_005F5C6A` / `D_005F5C68` | `+3`, `+9` |
| `+0x2A` | guard damage | `D_005F5C60` | `+0xC` |
| `+0x2B` | character select cursor | `D_005F5C64` | `+0xA` |
| `+0x2C` | quick select | `D_005F5C6E` | `+8` |
| `+0x30` | chicken marking | `D_005F5C7A` | `+0xE` |

Sound options (`+0x2D`-`+0x2F`) and help messages (`+0x36`) go to the sound code only. There is no life option;
VS / team VS handicaps are the per-side words above [I].

Unlocks [V]:

- `+0x10` (u64): one clear bit per character, set by `func_003BFD68(char)` from the ending (`func_003AA8A8`, mode 8);
  `func_003BFED8` (any bit set) unlocks THEATER in the main menu.
- `+0x18` (u32): the unlock step counter, `func_003BFCC0` (++) on a new clear bit, or by `func_003BFDF8` after 50
  matches counted in `+0x20` (u16, `setup[0x1A]` added per match, not for types 8 and 9). `func_003BFBB0` applies every
  entry of the unlock table `D_00440818` (`{u16 threshold, u16 kind, u32 arg}`, in the executable) whose threshold the
  counter reached: the kinds add characters to the selectable sets (`D_0043B968`, `D_0043B990`, `D_0043B9B8`,
  `D_0043B9E0`), set the Tekken Bowl level `+0x22` or the gallery flag `+0x23`. The sets are rebuilt from the counter, not
  saved [I].
- `+0x22` (u8, 0-3): `func_003BFEB0` unlocks TEKKEN BOWL; `+0x23`: `func_003BFEE8` unlocks GALLERY.
- `+0x24`: "new entry" bits (`func_003BFE78` sets, `func_003BFEA0` tests 4 / 2 / 1): a new unlocked entry blinks and
  takes the cursor (`D_005F6E50`-`D_005F6E52`).

Every setter calls `func_00361968` (`D_0061740E[0] = 1`, the save is dirty [I]). The memory card save
(`func_0035EBA0`, `src/memcard.c`) packs `D_00439C60` (0x68 bytes), `D_00440D58` (0xC8), `D_00415D60` (0x116) and
`D_0046BC70` (0x206) into a 0x480-byte buffer with one checksum byte, scrambled with `key = b + 5 * key + 1`;
`func_0035EC98` loads it back [V].

## 5. CPU difficulty

`func_00380120` (`src/fighter_core.c`, draft) picks the CPU parameter block [V]: survival (type 4) takes level and kind
from the table `D_00432D18` by the win count `D_005F5CC0` with `rand()`; time attack (3) level 1; every other type level =
`setup[0xD]` (the difficulty option for arcade, team, type 5 and the demo, 1 for the rest). The kind is the stage counter
`setup[0x24]` (8 and over falls back to level 1 / kind 6). The block `D_00431918[level][kind]` (5 x 8 blocks of 0x80
bytes) is copied to `gCpu + 0x344`; the demo (`D_005F5CBC`) uses `D_00432D90` instead. Practice maps its settings 0-4 with
`func_0037BF48` ([fighter.md](fighter.md) section 4).

## 6. Code (T18 batch 1)

| File | Range | C / functions | Drafts (`#if 0`, `NON_MATCHING` note, each compiles) |
|---|---|---:|---|
| `src/modes.c` | 0x3B90C0-0x3BB310 | 39 / 46 | `Mode_Boot` (18 lines, FPU registers of two fades), `Game_Start` (8, clear loop registers), `func_003BA480` (14, result built in `a1`), `func_003BA770` (18, a case tail cross-jumped), `func_003BAB58` (60, vibration script registers), `func_003BAC88` (12, `lh` against `lhu` + `sll / sra`), `func_003BAD98` (4, delay slot; decomp-permuter 10 minutes: nothing below its base) |
| `src/modes_b.c` | 0x3BB310-0x3BDF60 | 34 / 40 | `Mode_MainMenu` (about 114 real lines: `fp` holds the menu table's `%hi`, `s5` / `s6`, delay slots), `func_003BB8B8` (10, `movz` against a branch), `func_003BC370` (20, block layout and two hoisted constants), `func_003BCA30` (17, the constant 1 not hoisted), `func_003BCFB8` (3, one register), `func_003BD240` (2, a delay slot and a loop alignment `nop`) |
| `src/battle_mode.c` | 0x358A38-0x35C2B0 | 63 / 67 | `Mode_Battle` (902 against 890 instructions, register allocation and scheduling), `func_003596D8` (about 8 real lines, the survival pair-table loop), `func_00359FB8` (register allocation of the versus result counters), `func_0035BB58` (LCG product and angle terms scheduled differently) |

Not attempted in T18 batch 1 (left for the next batches): `select_results` (`Mode_CharSelect`, `Mode_NameEntry`, the
unlock functions `func_003BFxxx`), `pause_results` (`Mode_Results`, pause menu), `options` (`Mode_Options`,
`func_003B1488`), `menu_common`, `loading_title` (mode 8), `theater`, `menu_bg`, `gallery_jpeg`, `tekken_bowl`, `demo`.
