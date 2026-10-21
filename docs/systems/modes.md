# Game modes, menus and the game setup

The mode dispatch, the flow between screens, the game types and what each one sets for the fight, the options, the
unlock flags and their save block. T18 batch 1 covered `src/modes.c`, `src/modes_b.c` and `src/battle_mode.c` (section
6); batch 2 the character select, the results, the pause menu, the options and the ending / credits code (sections 7-13). Labels as in [../game_overview.md](../game_overview.md): **[V]** read in the code,
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
| 8 | `Mode_Ending` | ending / movie player: entered through `Mode_EnterEnding(ret, a, b)`, ends with `Mode_Change(2)` when `ret` is 1, else `gMode = ret` [V code, I role] |
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
  `Mode_EnterEnding(1, ...)` with per-character file numbers when `D_005F5CF0` is set [V calls, I roles].
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
points each item at its byte; `Options_Apply` copies them into the `D_005F5Cxx` words that the set-up functions above
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

- `+0x10` (u64): one clear bit per character, set by `Unlock_SetClearBit(char)` from the ending (`func_003AA8A8`, mode 8);
  `func_003BFED8` (any bit set) unlocks THEATER in the main menu.
- `+0x18` (u32): the unlock step counter, `Unlock_Step` (++) on a new clear bit, or by `func_003BFDF8` after 50
  matches counted in `+0x20` (u16, `setup[0x1A]` added per match, not for types 8 and 9). `Unlock_ApplyAll` applies every
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

Batch 1 left `select_results`, `pause_results`, `options`, `menu_common` and `loading_title` to batch 2 (sections 7-13);
`theater`, `menu_bg`, `gallery_jpeg`, `tekken_bowl` and `demo` are still to do.

## 7. Character select (mode 5) and the tag partner

`Mode_CharSelect` 0x3CB900 (`src/select_results_b.c`, draft) [V code, I roles]:

- **Sub-states 0-5**: 0 set-up (`func_003CB508` initialises both sides, `func_003C9778` fills the 36-cell portrait grid
  `D_00440A68` from the selectable set `D_0043B968`), the per-side input step, the start when both sides are full, the
  BGM timer, the multitap check, and the exit through `func_003CB610`. Before the screen shows, `func_003C45B0` (a 7-step
  load state machine on `D_01FAC908`) stops the BGM, loads the image banks 3 and 4 and every portrait (0x7B = empty) and
  uploads the textures 0xF5-0xFA, drawing the progress with `func_003C3FB8` ("STAGE n" / "FINAL STAGE" / "FIGHT" /
  "PRACTICE" by game type) [V].
- **Per side** (`SelState`, `D_00440D08 + side * 0x1C`: state, cursor cell, character, costume): `func_003C9AC0` moves the
  cursor over the grid (cell 0x36 is skipped, 0x37 is the random cell), `func_003C99C8` rolls a random character (0x22
  characters, locked ones and the current one skipped), `func_003C9ED8` picks the costume from the button pressed (0x80,
  0x40, 0x10, 0x20, 4, 0x800 give costumes 0-4; random for a CPU side or in team mode; a costume already taken by the
  other member is changed) [V]. `func_003CA470` is the per-side step that calls them [I, not attempted].
- **Tag partner**: each side picks its members one after the other into `D_00440C28` (8 member slots of 0xC bytes per
  side, count `D_005F62A8[side]`); `func_003C9518` undoes the last pick (and the partner side's when `D_005F6EAA` is set),
  with sound 3. `func_003CB610` commits: characters and costumes to the team records `D_00440940[side][member]` (through
  `func_003ADB70`) and to `D_005F5C98` / `D_005F5CA0` `[side][member]`, the team list for game type 2, and
  `D_005F5C80[side] = 1` [V]. The partner flags of the game setup (`+0x3C + side * 2 + member`) are read and written by
  `func_003AE420` / `func_003AE400` (`src/menu_common.c`) [V].
- **Drawing**: `func_003CCCB8` per frame (background, the grid `func_003CC6C8`, the side panels `func_003CC3E0`, the
  countdown `func_003CC190`, the cursors `func_003CC9E0`, pulsing between 0x60 and 0xF0, split when both sides are on the
  same cell), the portraits `func_003CC260`, the team icons `func_003C3DC0` (8 per team, an absent member dimmed), the
  panel frames `func_003C3370` / `func_003C3648` (nine quads) and the per-side panel `func_003BE3E8` of
  `src/select_results.c` (name, partner label, the 0-9 digit cursor of the handicap) [V].
- **Pads and multitap**: `func_003CBE18`, `func_003CBED8` and `func_003CA428` map the ports and the button masks;
  `menu_common`'s port tables `D_008D7E68` (4 x 8 bytes) and side lists `D_008D7E88` with their helpers 0x3AFFE0-0x3B06C8
  and the join rules `func_003B0820` (Start joins a port, left / right changes its side, confirm locks it; per game type a
  free side, pairs, a per-side limit and a player count) [V].
- **Preview fighters**: `func_003AEF88(side, ...)` loads the chosen character and costume step by step (`D_00437E30[side]`,
  `func_0035C8D0`, the model set-up `func_003AF7D0` / `func_003AF540` / `func_003AFAA0`) and places it at a start position
  from `D_00437D40` (`D_00437D80` for game type 5); `func_003AEA40` steps its preview move [V code, I role].
- **CPU opponents and random teams**: `Arcade_BuildOpponents` fills the arcade order `D_00440A00` (7 entries of 5 bytes: two
  opponents from a 34 x 3 preference table against the player's team, a random team from the 0x4C pairs of `D_005207F0`,
  the rest at random through `func_003800D8`, costumes 0-4 with per-character exceptions) [V]; `Team_BuildRandom` builds
  random teams into `D_00440940`; the opponent pools `D_01FAC7C0[2][23]` are refilled by `func_003C0F48` and drawn by
  `func_003C11B0` [V].
- `func_003C1350` gives a member's maximum life by game type (VS handicap for types 1 / 6, character 0x21 x1.5) [V]; the
  winner of a team battle or survival match regains 15% of each member's maximum before the next match (`func_003C1620`,
  draft) [V].

## 8. Results, records and name entry (modes 10, 6, 7)

- **`Mode_Results` 0x3B6258** (`src/pause_results.c`) [V]: sub-state 0 stops the BGM, calls `func_003CD6E8` for game types
  3 / 5 / 8 and `func_003C4AE0`; sub-state 1 goes to the name entry (`gMode = 6`) for types 3 / 5 when
  `Results_DrawStageTimes` returns 1, and type 8 polls `func_003B6228` before `Mode_Change(3)`; sub-state 2 is
  `Mode_Change(3)`.
- **Screens** [V]: `Results_DrawStageTimes` ("TIME ATTACK RESULTS" / "1 ON 1 RESULTS", one stage time every 30 frames, the
  total and "NEW RECORD !!"), `Results_DrawSurvival` ("SURVIVAL RESULTS", a counting win total and the rank title), both
  left after 3600 frames or a button (`func_003C4AF8` fade, `func_003B6BD8`); the scrolling result rows of
  `func_003B6C68` with the portraits `func_003B6620` / `func_003B6968` / `func_003B6AC0` (team battle and survival [I]);
  `func_003B6408` prints three record counters with blinking digits; `Time_FramesToClock` splits frames into minutes,
  seconds and hundredths (capped at 0x57E3F).
- **Records block `D_00440D50`** [V]: ten score entries of 12 bytes at +8 (score, two characters, name), ten time entries of
  8 bytes at +0x80, eight per-stage results of 8 bytes at +0xD0, display state +0x110-0x1BC (list, scroll, per-character
  usage). `Records_InitDefault` resets it (default score 0x57030); `func_003C80E8` sorts the scores, `func_003C8270` the
  times, `func_003C84B8` ranks the characters by use; `func_003C8710(0/1/2)` draws one list, `Records_DrawRank` the
  ST / ND / RD / TH ordinals; `func_003CD0E0` tests the eight-stage total for a new record, `func_003CD1E8` finds a time's
  rank. The memory card save packs it (section 4, `D_00440D58`).
- **Name entry** `Mode_NameEntry` 0x3C8A88 (17 sub-states, draft) [V]: loads the records first (`D_005F6E84` steps 0-4,
  memory card reads and fades), scrolls the list in, edits the name through `NameEntry_Step`, then leaves to mode 2 or
  3. `func_003C8A50` (mode 6) sets it up.
- **`Text_Printf`** 0x3C6C60 (draft) [V]: the game's text formatter, `%H` / `%V` pixel position, `%h` / `%v` cell
  position, `%F` font, `%S` scale, `%c` colour, `%T` measure only, `%Z` / `%P` depth, `%d %D %x %b %s %C` with width and
  zero padding; returns the width. `Text_PutGlyph` queues one glyph sprite (at most 100, kind 10 proportional widths from
  `D_0051FF08`, kind 11 fixed) and returns the advance.

## 9. Pause menu and between-match swaps

- **`Pause_Update`** 0x3B7B98 (draft), called by `Mode_Battle` while paused [V]: an 8-entry cursor `D_005F6DE0` (entries
  skipped by game type, `func_003BFEF8` and `func_003B7B18`); the flag word `D_005F6DE6` holds the open sub-screen: 0x08
  the yes / no confirm (entry 3: `func_003B7998(1)`, else `func_0035A518`, quit), 0x02 `func_003DD098`, 0x04 the screen
  position (`func_003B4938`), 0x10 the controller set-up (`Options_ControllerSetup`), 0x20 `func_003F3118`, 0x40
  `func_003F1CB8`; sound 5 on a confirm, 3 on a move. `func_003B7998` / `func_003B79E0` / `func_003B7A68` /
  `func_003B7AC8` open, test, quit and close it [V code, I roles].
- **Between rounds and matches**: `func_003B5328` (6 phases on `D_005F64D8`, called from `battle_mode`) and
  `func_003B5DB0` (from `battle_flow`) swap in the tag partner or re-place the fighters (`func_0035C8D0`,
  `Tag_SetCounter`, life reset); the start positions come from character-pair tables (`func_003B4F88`, `D_0043A180`...,
  `Fighter.unkB12C`, adjusted by the 39 entries of `D_0043B5C0`); `func_003B77B8` / `func_003B7888` orbit the camera
  (radius 5000) [V code, I roles].

## 10. Options screen (mode 9)

`Mode_Options` 0x3B4B20 (`src/options.c`) [V]: sub-state 0 loads sprite bank 8 (`func_003B1980`); 1 the main list
`func_003B19C0` (six rows from `D_00439CC8`; confirm sets `gSubMode = row + 2`, back goes to 7); 2 the game options
`func_003B40C0` (14 items of 0x40 bytes at `D_00439CE0`: list, number or action; item 12 is the yes / no reset of
`func_003B1820`, item 13 exits); 3 the controller set-up `Options_ControllerSetup` (per pad: button assignment
`D_00439CA0[pad * 10 + id]`, vibration `D_00439C9C[pad]` tested with `scePadInfoAct`, reset, exit; rebuilds the input map
with `Input_BuildMapping`); 4 the records pages `func_003B2978` (three lists `func_003B2090` / `func_003B2358` /
`func_003B2670`); 5 the screen position `func_003B4938` (clamped to +-0x30 / +-0x18); 6 the memory card screen
(`func_00360F40`); 7 a fade-out that rebuilds the input maps, applies the options (`Options_Apply`, section 4, also the
display offset from block bytes 0x34 / 0x35 and stereo from 0x2D, then `Unlock_ApplyAll`) and `Mode_Change(3)`.
`Rng_GetSeed` / `Rng_SetSeed` (block +0x1C) sit in the same object.

## 11. Memory card save and load triggers

[V calls, I roles] The save block (section 4) is marked dirty by `func_00361968` from every unlock setter and from the
options (leaving the main list, a button assignment); the card screen itself is the options' sub-state 6 (row 6 of the
main list calls `func_00361678`), and the boot loads the card (`Mode_Boot`, section 1). `Mode_NameEntry` reads the records through
the memory card before the list (its steps 0-4). The card screens are `src/font_tex.c` ([render.md](render.md) section
13).

## 12. Unlocks (`src/select_results.c`)

`Unlock_ApplyAll` 0x3BFBB0 [V] clears the selectable sets (`func_003BFA78`, `CharSet_Copy` of `D_0051E8F0` into
`D_0043B9E0`) and applies every entry of `D_00440818` up to the step counter as a switch over eight kinds (character
sets through `func_0035CA00` / `func_0035C998` / `func_0035CA68` / `func_0035CAE0`, the Tekken Bowl level, the theater
flag, the gallery). `Unlock_Step` (+0x18 ++, apply, save dirty), `Unlock_SetClearBit` (bit n of +0x10, a new bit steps),
`func_003BFDF8` (the 50-match counter), `func_003BFE78` / `func_003BFEA0` (the "new" bits), `func_003BFAA8` /
`func_003BFB18` (the last reached / next kind-1 argument). The selectable sets are 0x22-byte flag arrays with the
`CharSet_*` helpers of `src/loading_title.c` (copy, clear, count, AND, OR, any common); the default sets are the
`.rodata` tables 0x51E850-0x51E970 of `pause_results`.

## 13. Ending, credits and the menu helpers

- **Mode 8** `Mode_Ending` 0x3ABBD0 [V code, I role]: sub-state 0 calls `func_003AA5F0`, 1 runs `func_003AA8A8` (the
  ending sequence, 13 states on `D_005F6D38`: the ending of `D_0051B678[character]` with its script, stage and fighter
  pairs from `D_0051B488`, a script of 33 ops: motions, fades, effects `func_003AC5D0`..`func_003AD1D8`, camera, skip in
  states 7 / 9-11) until it returns, 2 `Mode_Change(2)` when the return mode is 1, else `gMode = ret`.
  `Mode_EnterEnding(ret, a, b)` enters it; `func_003A9AB8` is a second script player (10-byte commands `{frame, op, a,
  b, c}` from `D_00519B90`, ops 0-14) driven per frame with `func_003A9898` [V].
- **Credits** `Credits_Update` 0x3AC018 [V]: title cards (states 1-7), then the 12-byte lines of `D_0051B880` scroll up
  from y 480 (0x180 per frame, 8.8 fixed point; a line with `unkA == 1` only when `func_003BFEB0` is set), then after 210
  frames `D_005F629A = 1`.
- **Records attract** `func_003AD678` (8 entries through `func_00365D10`, the time display effect `func_003AD2C8`) [I].
- **menu_common** [V]: the menu camera (`func_003ADFF0` view matrix, block `D_00415808`), per-port pad assignment and the
  menu input tests (`func_003B1118` button 0x800 back to mode 3, `func_003B1378` a combination, `func_003B1200` /
  `func_003B1330` a challenger joining), the select preview fighters (section 7), the game-setup accessors
  (`func_003AE400` / `func_003AE420` partner flags, `func_003B1468` the help-messages byte).

## 14. Code (T18 batch 2)

| File | Range | C / functions | Drafts (`#if 0`, `NON_MATCHING` note, each compiles) |
|---|---|---:|---|
| `src/loading_title.c` | 0x3A9678-0x3ADFA8 | 74 / 83 | 9: `func_003A96B8` (8 lines), `func_003A9898` (34), `func_003A9AB8` (148), `func_003AA640` (100), `func_003AA8A8` (645, lit4 shift), `func_003AC5D0` (28), `func_003AC7C0` (10), `func_003ACB08` (25, lit4 shift), `func_003ADE18` (2); built without `-ffast-math` |
| `src/menu_common.c` | 0x3ADFA8-0x3B1478 | 72 / 82 | 10: `func_003ADFF0` (21), `func_003AE608` (10), `func_003AF540` (41), `func_003AF9D8` (15), `func_003AFAA0` (7), `func_003AFF40` (9), `func_003B0120` (8), `func_003B01D0` (19), `func_003B0820` (154), `func_003B0D50` (16) |
| `src/options.c` | 0x3B1478-0x3B4D50 | 27 / 38 | 11: `Options_Apply` (56), `func_003B15F8` (26), `func_003B19C0` (49), `func_003B2090` (25), `func_003B2358` (29), `func_003B2670` (9), `func_003B2B58` (27), `func_003B2C28` (31), `Options_ControllerSetup` (837), `func_003B40C0` (62), `func_003B4938` (58) |
| `src/pause_results.c` | 0x3B4D50-0x3B90C0 | 65 / 74 | 9: `func_003B5328` (90), `func_003B5CA8` (44), `func_003B5DB0` (2), `func_003B6408` (62), `func_003B6808` (2), `func_003B6C68` (200), `func_003B77B8` (35), `func_003B7888` (41), `Pause_Update` (285) |
| `src/select_results.c` | 0x3BDF60-0x3C1A78 | 60 / 72 | 12: `func_003BE118` (88), `func_003BE3E8` (10), `func_003BEAB0` (139), `func_003BECC0` (4), `func_003BF080` (53), `func_003BF4B8` (87), `func_003BFFB0` (14), `func_003C0210` (17), `Arcade_BuildOpponents` (480), `func_003C1078` (12), `func_003C1350` (2), `func_003C1620` (115) |
| `src/select_results_b.c` | 0x3C3150-0x3CD298 | 82 / 117 | 34 drafts (from 2 lines: `func_003C68F0`, `func_003C9778`, to 549: `func_003C59D8`; `Mode_CharSelect` 80, `Mode_NameEntry` 257, `Text_Printf` 257); `func_003CA470` (1062 instructions) not attempted |

The line counts are fdiff's for one draft enabled at a time; a draft whose size differs also counts shifted `$gp` and
branch offsets. Every draft carries its one-line reason in the source.
