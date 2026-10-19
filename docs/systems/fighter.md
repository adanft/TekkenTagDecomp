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
- A record at 0x8B8010 (`.bss`, five words and a list of 16 move ids): `func_0037EA08` (matching C) clears it and
  `func_0037EE18` (C attempt) tells whether the move `unk10C` is in the list (0 entries are empty) [V code; I use].
  In state 2 `func_0037EA58` (matching C, T14 b7) counts the frames of the combo (word 0xC, at most 0x34BC0) and prints
  the timer (`func_0037EE68`, minutes'seconds"hundredths, menu item 8) and the counter "COMBO n:m" (`func_0037EEF8`)
  [V].
- The dummy's actions (T14 b7, matching C): `func_0037DA08` starts its stance move for a state 0-13 (moves 0x3A,
  0x50, 0xD6, 0xE4, facing `unk3A` turned by a quarter or a half); `func_0037DC98` dispatches the action `kind` of the
  settings (6-10 to `func_0037E0E8` when `unkF7` is set, which holds pad bits 0x60 / 0x90 / 4 / 0x2000 / 0x8000 for
  move kinds 9-10; 13 to `func_0037DDD0`; 1-4, 14 to their handlers, 5 / 11 set `unk12C`); `func_0037C370` tells
  whether the current move is one of the two listed in `D_005073E0` and at one of its listed frames [V code].

## 4. The CPU opponent (0x3800D8-0x38BF98)

- State: `gCpuStates` (0x8B8078), one `CpuState` of 0x3E8 bytes per side. While a side thinks, `gCpu` points to its
  state, `gCpuSelf` to its fighter and `gCpuOpponent` to the other one (set by `Cpu_GetInput` and
  `func_003809C0`) [V].
- `Cpu_GetInput(f, &held, &pressed)` (matching C since T14 b5): selects the side's state (`gCpu`, `gCpuSelf`,
  `gCpuOpponent` from `f->unk20`); unless `func_00393810` says the CPU may not act, it runs `Cpu_Think` (0x380F38),
  then returns the buttons `gCpu->unk6` and their rising edge against the previous frame (`unk8`) [V].
- `Cpu_Think` (matching C since T14 b7), once per frame and CPU side, in this order [V code; I meaning of the
  deciders]:
  1. notes the own move frame (`unk44`, `unk4` counts the frames it changed), the flags of both fighters' moves
     (own `unk8` / `unk4` in `unk32C` / `unk330`, the opponent's in `unk338` / `unk33C`, low halves in `unk334` /
     `unk340`) and adds the damage the own fighter took this frame (`unk144 << 12`) to `unk27C[member]`;
  2. if a command is still playing (`func_00381418`, see Commands below) it keeps it (`D_005F6120 = 0`); otherwise
     the first decider of a fixed list that returns non-zero sets the input, and `D_005F6120` keeps its number:
     1 `func_00381D30`, 2 `func_00381BF8` (answer while the own fighter is hit, `unkE0`), 3 `func_00388DE0` (own move
     with flags `0x4C02`), 5 `func_00381408`, 4 `func_00388FA8`, 6 / 0x16 / 7 the character hooks (`func_0038A408`
     opponent's hook, `func_0038AD00`, `func_0038A500` own hook), 0x17 `func_0038AC40`, 8 `func_00388170`,
     9 `func_00388DA8` (mode `D_005F5CBC`: repeat the previous buttons' high nibble on the CPU's frame parity),
     0xA `func_00385C80` (continue a running plan, then list the candidates), 0x16 `func_0038ABE8`,
     0xB `func_003864E8`, 0xC `func_00389B70`, 0xD `func_00389D58`, 0x14 `func_0038A780`, 0xE `func_00387600`,
     0xF `func_003879A0`, 0x10 `func_00387DA8`, 0x15 `func_0038A928` (play the planned command `unk264`),
     0x11 `func_00388278`, 0x12 `func_00387120`, 0x13 `func_00388840`, 0x63 `func_00388D70`;
  3. drops button 4 for character 0x21 without `unk401`, and in the mode `D_005F5CBC` unless `D_005F6144`;
  4. runs `func_00380B10` (C attempt): counts the CPU timers down (`unk4C`, `unk5C`, `unk60`-`unk6C`, `unk84`,
     `unkCC`, `unkD0`, `unk320`), resets the plans when the own fighter is hit (`unk12F`), keeps the opponent's moves
     that took life in `unk288[0..3]` (newest first; `unk298`: the last three were the same move) with a chance of
     `4096 - unk38E`, and makes the CPU adapt: each time it is hit (outside the mode `D_005F5CBC`) the chances
     `unk364` and `unk368` grow by 10 (up to `unk366` / `unk36A`) and, while `unk2C0` is positive, `unk372` by 40 and
     `unk36E` by 20 (up to `unk374` / `unk370`). Life (`unk3F4`) is compared with the previous frame's (`unk40`).
- Deciders follow one pattern: a few state tests, then chances `(rand() & 0xFFF) < gCpu->unkXXX` against the
  parameter block (`CpuState + 0x344`, s16 chances in 4096), then `Cpu_PickCands(filter)` and `func_003853E8` to play
  one of the accepted candidates, or a pad word written to `unk6`. Examples matched in T14 b7: `func_00388DE0` tries the
  filters `func_00384B68` (chance `unk3AC`), `func_00384B80` (`unk3B4`), `func_00384B50` (`unk3AA`);
  `func_00389B70` plays `func_00389B50` / `func_00384D78` candidates or side-steps (`func_0038A690(-1)`);
  `func_00381BF8` answers when hit with a command `func_00381B10` picked at random from `func_00381818`'s list for
  frame `cmd->unkB + 1` of the own move (`unk124` / `unk128`), at most `unk356` times (`unkFC`).
- Deciders matched or read in T14 b8 (what each one reacts to) [V code; I meaning of the pad words and counters]:
  - 4 `func_00388FA8` (C attempt): the opponent's move of flags 0x404 (own `unk330`). With a command of situation 0x34
    (`func_00386388`) it presses 0x10 every `unkA0` frames; otherwise it lists the own move's commands that apply now
    (`func_003835A0`), builds a mask of answers from the opponent's move (window `unk2C` / `unk2D`, kinds 0x10F / 0x607,
    flags 0x10000 / 0x200000 / 2, `unk2A8`, `unk34`, rounds `unk20` / `unk24`, frame parity, character 0xE's move 0xBA9)
    and draws from `D_005078E0` until an answer is in the mask: 1 holds 0x1000, 2 holds 0x80 (0x4080 every 8 frames),
    4 holds 0x8000, 8 / 0x10 / 0x20 play a `func_00383F10` / `func_003840F8` / `func_00384578` candidate.
  - 6 / 7 / 0x16 `func_0038A408` / `func_0038A500` / `func_0038AD00` (matching C): the character hooks below.
  - 0xE / 0xF / 0x10 `func_00387600` / `func_003879A0` / `func_00387DA8`: follow-up plans. Decider 0xA lists them
    (`func_003856C0` into `unk134`, `func_003857F8` into `unk154`, `func_00385A60` into `unk228`); while the plan runs
    (`unk12C` / `unk14C` positive, `unk220` not negative) the decider plays one of its commands that applies now, at
    random, with the chance `unk378` / always / `unk380` and the limits `unk376` / `unk37A` / `unk37E`, `unk258` < `unk25C`
    and the filter `func_00387518`; otherwise the plan ends.
  - 0x11 `func_00388278`: reacts to the own angle `unk40` and the opponent's early window (`unk19`): side-step pad words
    0x6000 / 0x2000 / 0x8000 / 0x4040 / 0x4020 with the chances `unk390` / `unk38E`; fighters of variant 7 step by
    `unk40` (inline `Cpu_Variant7Step`).
  - 0x12 `func_00387120`: the general attack choice. Waits while `unk74` / `unk64` run, acts less often from round
    `unk20` 2 on, picks candidates (`func_003865A0` with the wait `unk94`; at low life `func_00383EF0` ones), drops
    `func_00384B80` ones unless `unk3E4` is 1, plays one (`func_003854B0`), may start the plans `unk12C` / `unk14C` and
    keeps an attacking move in `unk90` (which the candidate list skips once with a chance of 7 in 8).
  - 0x13 `func_00388840`: the waits (`unk4C`, `unk50`) and the situation of `func_00388530` (0-10): play a candidate of
    `func_00384988` / `func_003849A0` / `func_003849B8` / `func_00384A50`, a side step, the command `D_005074BA`, or wait
    4-59 frames holding 0x2000 / 0x6000 / 0x8000 / 0xC000 (by `unk54`).
  - 1 `func_00381D30` (T14 b9, C draft): not a decider but the per-frame analysis, it always returns 0. It refreshes
    the CPU's view of the fight: whether the own / the opponent's move changed (`unk1C` / `unk29C`), the distance
    `D_005F5C28` with its change `unk34` and a small wobble `unk28` from `gSinTable`, the range class `unk20` (0-5,
    against the character row `unk3C4`-`unk3CA` of `D_00432E10`) and the distance class `unk24` (1-5, `unk3CE`-`unk3D6`),
    the opponent's attack state (`unk2AC` attacking, `unk2A8` frames to its window, `unk2B0` / `unk2B4` / `unk2BC`), the
    answer flag `unk98` (the own move 0x2021 / 0x2829 / 0x1052 against the kinds 0x412 / 0x217 / 0x10F / 0x31F / 0x706),
    the waits `unk70` / `unk94`, the life comparison `unk3E0` / `unk3E4` (team life of `D_00440940`; in practice mode the
    damage `unk27C` the CPU took) and the opponent's last 20 moves in `unk2D0`; all chances that answer attacks
    (`unk360`, `unk362`, `unk368`, `unk36A`) drop to 0 while the opponent's red life is at least a tenth of its life.
  - 0xD `func_00389D58` (T14 b9, C draft): acts on the situation code of `func_00389488` (0-10, C draft): 0 while a plan
    runs or the own fighter is hit, 2 without a pending step; with the opponent busy (`unkF8`) 4, then the character's
    special plays 6 / 7 / 8 / 9 at distances 8-9 / 22-26 / 4-10 (chance `unk3A2`, `func_00383D10`, the per-character table
    `D_00507620`: a flag and three command lists), 1 / 2 against the opponent's moves of flag 0x400000 (chances `unk382` /
    `unk3A6`), 2 to start or keep an attack (`func_00388F78`), 1 or 10 to repeat a remembered move and 5 at distance 9-11
    against kind 0x412. The decider then picks candidates (codes 1 / 6 / 7 / 10), uses `func_00384D78` (code 2,
    matching C since T14 b9: the answer to an opponent far away or recovering, -1 with a delay `unk270` inside the
    opponent's window), runs the step plan `unkF8` (codes 3 / 4: guard 0x8000, step 0x2000, the command `D_005074A2` or
    0x3000, ended when `D_005F5C28` reaches 2951, 300 later against variant 4), side-steps (5) or plays the character's
    command lists (8 / 9) [V code; I meaning].
  - Situation codes of decider 0x13 (`func_00388530`, T14 b9, C draft, 39 lines from matching): a random draw
    `Cpu_RandBelow(0x1000)` read through bands that depend on the round `unk20` (while the wait `unk94` is at most 35)
    or on `unk24` (otherwise): 0-10 as listed for `func_00388840`; codes 3 and 6 are kept only with the chances
    `unk394` and, against moves of flag 0x80000 or with `unk340` set, `unk38E` [V code; I meaning].
- Candidate lists (T14 b8, matching C): the walkers of a move's command list (shared lists of 0x8004 included) test
  `CPU_CMD_FOR_VARIANT`, `Cpu_IsCmdAllowed`, the frame window, `Cmd_CheckCondition` and `Cmd_CheckSituation`:
  `func_00382AD8` fills `gCpuCands` for decider 0xA (and, when the window of the last command still holds and its move
  differs, the commands of that move starting at frame 0 or 1; filter `unkBC` through `Cpu_RejectCands`),
  `func_003835A0` the candidates of decider 4, `func_00381818` / `func_003814C8` the answers when hit (decider 2),
  `func_003838B0` whether a command of flag 0x400000 applies, `func_00385DF8(id)` (C attempt) whether move `id` can be
  started now. `func_003814C8` maps the kinds 0x3D-0x3F of the command picked to the codes 1-3.
- Move kinds (`func_00383BD8`, matching C): the low half of a move's `unk8` classifies it (0x10F, 0x217, 0x31F, 0x412,
  0x51F) and the opponent's move flags (`unk33C`: 0x4C02, bits 0x400 / 4 / 1 / 2) say which kinds answer it, in two
  tables (`which` 0 / 1) [V code; I meaning: likely high / low / mid answers].
- Command availability (`Cpu_IsCmdAllowed`, a static inline of the source used by ten CPU functions, T14 b7): a few
  moves depend on the fighter's variant `unk28` and on chance (0xEE3 not with variant 4 in the mode `D_005F5CBC`,
  0x181 / 0x197 only with `unk3DC`, 0x4CA never, 0x3BC and 0x1245 / 0x1253 one time in 8 for some variants,
  0x1246 / 0x1255 not with variants 0 / 0xD, 0x1262 seven times in 8 with variant 4) [V code].
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
  `func_00381418` advances it each frame (a queued word `unkA` first), both matching C since T14 b5 [V].
  `func_00381278(code, move)` (C attempt, 13 lines) starts a move's input code: 0x8003 arms the fighter's `unk12B`,
  0x8001 / 0x8002 and codes above 0x8005 play the command list `func_0036CF30` returns, any other code becomes pad bits
  at once: the highest direction bit of `(code >> 5) & 0x1FF` through `D_00507410` / `D_00507428`, buttons from bits
  0-3 (`D_00507440`) and bit 14 (`D_00507448`); the move is kept in `unk8C` [V]. The side-step
  commands of `D_00507452` (`func_0038A690`, matching C) are chosen from the opponent's body angle and the fighters'
  `unk624`, randomly near 0 / 0x8000 [V code; I meaning].
- Per-character behaviour: the table `D_00507A80` (12-byte `{key, handlerA, handlerB}`, 33 searched by
  `func_0038BEE0`; the code builds the address 0x507A88 of the first `handlerB`) gives each character's move handlers;
  28 of the handlers it points to are `return -1;` stubs (no special handling) [V]. `func_0038B718` (matching C)
  answers move 0x1407 at frames 0x18-0x19 with the command `D_00507618`; `func_0038BC00` (key 3, matching C) plays one
  of the commands `D_00507A52` / `D_00507A32` / `D_00507A12` (seven words each) depending on the opponent's move flags
  2 / 1 and `unk2A4` [V code].
- Hooks (T14 b6; matching C since T14 b8): `func_0038A408` runs the hook `gCpu->unkB4` (the opponent's
  character's handler from `D_00507A88`) while `unkCC < 0`, with a chance of `unk382` in 4096; its result is -1 (nothing),
  -2 (acted), -3 (play one of the picked candidates at random, `func_003853E8`), 0x2000 / 0x8000 (hold that pad bit and
  call `func_00380540(0x10)`) or any other pad word to hold in `unk6`. `func_0038A500` and `func_0038AD00` are the same
  with the own character's hook `unkB8` and `func_0038BF38` [V code; I meaning].
- Playing a candidate (matching C): `func_003853E8` picks one of the `unkB0` picked candidates with `rand()` and the
  seed `D_005F6C08` (stepped `x * 5 + 3`), starts its command (`func_00381278`) and commits to it with `func_00385250`,
  which counts moves of flag 0x400000 in `unkFC` and, for an attack (`unk2C` window), sets the wait `unk94` to
  `unk34C[0]` plus one of `unk34C[1..4]` at random and resets `unk64` / `unk68` / `unk6C` / `unk5C` / `unk50` / `unk4C`
  [V]. `func_00384408` and `func_00384C98` (matching C) are candidate filters on the move flags (0x412, 0x400000,
  0x20000, 0x80000 / 0x200000 for moves with an early window, 0x10000) and `func_00383B88` [V code; I meaning].
- Difficulty: `func_00380420` (matching C since T14 b7) selects `gCpu` for a side and copies the 0x80-byte parameter
  block `D_00431918[level][kind]` (5 x 8 blocks, indices clamped to 4 / 7) to `CpuState + 0x344`; the block holds the
  s16 chances the deciders test (`unk34C`-`unk3C2`). `func_0037BF48` maps the practice setting 0-4 to the pairs
  (0, 1), (1, 2), (2, 3), (3, 1), (4, 3) [V read; I "difficulty"].

## 5. The fight simulation (`battle_flow`)

### 5.1 One frame: `Battle_UpdateFrame` (0x36D860)

`Mode_Battle` calls `Battle_UpdateFrame` once per frame for the two fighters on screen, `gFighters[0]` and
`D_00884EE0` (slot 1). The function is matching C; the order below is [V], the roles of the callees still in
assembly are [I] unless marked.

1. `unk24 = unk0 * 2 + unk1E` for both fighters; `func_003C1B38()`; game type 7 (`D_0043B7F8[0]`) calls the
   practice hook `func_003796B8`.
2. Pause: while `D_005F5EE8` is set, `Battle_UpdatePause` (0x36DEA8, C draft since T14 b9) turns the Start button
   (0x800) of each side that may pause (`D_005F5CB0` / `D_005F5CB2`) into the pause bits `D_005F5F08` (1 / 2 for the
   side), copied to `D_005F5EE4` / `D_005F5F0C`; nothing pauses while `func_0039C738` / `func_003B7B38`, option byte 0x11
   or `D_005F5EE0` hold. With option byte 1 and `func_0036E158() == 1` a request opens the pause (`func_003B7AC8`) and the
   same side closes it (`func_003B7B40`), otherwise a pause closes at once; a change lowers the music to 0x40
   (`Bgm_SetVolumeFade`, `func_00375428`) or restores 0x80. In practice mode (type 7) the practice menu decides instead
   (`func_003795D8`, `func_00379628` / `func_00379638`, or `func_00379648` sets `D_005F5EE4` directly), and no pause
   starts while a fighter's move has kind 2 in bits 20-21 of `unk24` [V code; I meaning]. Otherwise the words are
   cleared.
3. When the game is paused (`D_005F5EE0` or `D_005F5EE4`), only the drawing side runs (animation, effects) and
   nothing below happens.
4. `D_005F5EF8 = func_00364B50()`, then `Battle_UpdateRound` (0x36E198, matching C since T14 b6, the round state
   machine of section 5.11; reads and writes `gRoundPhase`). When
   `D_005F5EF8` is set (a frame without simulation, e.g. a replay or an intro), only animation and effects step,
   by `func_00365D00()` frames.
5. Otherwise the simulation, in this order:
   1. `gBattleParity ^= 1`; round HUD [I] (`func_0036EA80`: "ROUND %d", "READY!", "K.O.", "TIME UP" ...);
      `func_0036DDD0`; camera shake step `func_0035C320` [I]; `func_00341AE8`; `func_0034F288` per fighter.
   2. **`Battle_StepMoves`** (0x372A88): the move step `Fighter_StepMove` (0x36A428) of both fighters, the one
      picked by `gBattleParity` first. When one of them reaches the end of a move of kind 0x20 (a throw [I]), the
      other first starts the move given by `D_0046BE78[entry.unk4].unk1C` (kind 0x24, `Fighter_StartThrownMove`).
   3. `Battle_UpdateFacing` (0x371330): `Math_Atan2` of the vector between the fighters (`unk51C` / `unk524`) into
      `unk3A` (the other side gets `+0x8000`); `unk48` comes from the camera angle `D_00415808[1]`.
   4. `Battle_UpdateDistance` (0x371458, C draft since T14 b9) writes the fighter distance `D_005F5C28` (`Math_Isqrt` of
      the `unk51C` / `unk524` difference) and `D_005F5F34` (40 less for each fighter of character 0xB or 0x14). While
      the fighters are 0x1D00 or more apart, a fighter whose move lacks flag 0x2000 (bit 13 of `unkC0->unk24`) is pulled
      back to its anchor `D_00879610[i]` (at once, or by an eighth per frame while `D_005F6BE8[i]` keeps the flag); the
      anchors follow the fighters otherwise [V code; I meaning: the stage limit]. `func_00371A48` the distances to the
      two points `D_00879610` (`D_005F5C38`).
   5. `Battle_UpdateMovement` (0x36FB40, 0x12E0 bytes, C draft in `#if 0`, read in full; integer only, no floats): the
      per-frame movement and flags of both fighters (section 5.12); `func_003772F0` unless a fighter has `unkE0` set.
   6. Skeleton and animation side (`func_00371B18`, `func_00378150`, `func_00378600`, `func_003BAEF0`,
      `func_003A6390`), `Fighter_UpdateMotion` (position delta of the frame into `unk3C4`-`unk3CC`),
      `func_0034F3D8`, the camera targets (`func_003D3C68` with `unk51C`, `unk520`, `-unk524`, `unk122`).
   7. `Battle_ApplyVelocity` (0x371C68): while `gRoundPhase < 0xE`, adds `unk3E8`-`unk3F0` to both position
      triples `unk4`-`unkC` and `unk51C`-`unk524` of both fighters (not on frame 1 of move 0x1019).
   8. **`Battle_ResolveHits`** (0x371D68, matching C since T14 b5): clears the hit flags (`unk12F`, `unk132`, `unk133`,
      `unk13B`, `unk14C`), runs the hit test (section 5.5) through `func_003772B0(a, b, &a->unk39C, &b->unk39C)` and, for
      each attacker `p` and defender `q` in turn, lets the hit count when the attack level `p->unkD0` (bit 0 added
      against a crouching `q`, `unk11A == 2`) meets the defender's guard mask `q->unkCC & 7`, the attack has not hit yet
      (`unkF1`) and both fighters are drawn (`unk127`); the throw and tag exceptions are in section 5.8. The attacker gets
      `unkF1`, `unkF4`, `unkF5`, `unk132`, the defender `unkF3`, `unk12F`, the hit freeze `unk14C` (the attacker move's
      byte 0x2E, signed), the hit kind `unk11C` (the attacker's `unk11B`) and the pairing record `unk18C` (section 5.8);
      `unk14E = 4` when the attack does damage or the defender is juggled. Last, `unk133` is set for a fighter whose move
      reaches the end of its window (`unkC0->unk2D == unkC4`) without a hit (`unkF5`) [V].
   9. `func_00371C08` (attack volume motion), `func_003729B0`: **`Fighter_StartCommandMove`** for both, in parity
      order (section 5.2), `func_00372BA8`, **`Fighter_ApplyDamage` (0x3720C8): the damage of the frame** (section
      5.6), fighter voices (`func_00375B60`), `Snd_UpdateDelayed`, the practice dummy (`func_0037D668`).
   10. **`Fighter_UpdateInput`** per fighter: when `unk414` is set, `func_0036C680` fetches this frame's buttons
       (pad, CPU or practice playback, section 2) and records them (section 5.2).
   11. `unkC6 = unkC4`; `func_00372A10`: `func_00368B90` and `func_00368B08` for both in parity order (the launch
       reaction, `func_00368958`: kind 0x26, record `func_00374010(Fighter_GetLaunchKind())` from `D_00500678`); end of frame
       helpers. `func_00368B90` (C draft since T14 b9) starts the next move: a hit with the attacker's hit kind 0 (or 2
       without a launch) takes the hit reaction of `func_0036A050` (matching C since T14 b9: kind 0x25-0x2D and the
       reaction move from the pairing record `unk18C`, 0x1C-0x37 by state, front or back by `unkCC` bit 2; a launch uses
       the launch record), otherwise the command entry found at the current frame (`func_003691D8`) starts its move
       (`unkF8` 2), or at the list end the next move's list at frame 1 (`unkF8` 3), or past the move's length the end
       entry's move (`unkF8` 4) [V code; I meaning].
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
  `D_0047F780`). `Move_FindCommand` (0x369CB0, matching C since T14 b5) walks it at the current frame `unkC4`: an
  entry applies when the frame lies in `unkB..unkC`, its input code matches (`Cmd_MatchInput`), its condition `unk2`
  holds (`Cmd_CheckCondition`) and, for condition kinds `unk3 & 0x7F` of 0x61 or more, `Cmd_CheckThrowCondition` agrees (kinds below
  0x61 never apply here); markers 0x8004 / 0x8005 enter and leave a shared list, 0x8000 ends it and is returned when
  nothing matched [V].
- `Cmd_CheckSituation` (0x3693E0, matching C since T14 b6, used by the CPU code) tests the situation kinds
  `unk3 & 0x7F` 0-0x5F of a command entry; with bit 0x80 it first requires the partner state `func_0035B9E8(side) + 1`
  to equal (`unk4` set: differ from) the other fighter's `unk129` [V code; I names]: 0 always; 1 this fighter's hit
  lands (`unk132`); 2-0x13 the reaction direction `unk172` (computed by `func_0036BFA0` when 0) equals the kind and
  `unk4 >= D_005F5F34`; 0x14-0x17 the same for the direction pairs 2 / 6, 3 / 7, 4 / 8, 5 / 9; 0x18 / 0x19 the fighter
  distance `D_005F5C28` at most / at least `unk4`; 0x1A an unguarded hit; 0x1B-0x23 the other fighter guarded
  (`unk130`), this fighter's end-of-window flag `unk133`, the other's move kind other than 0x20, its damage `unkCA` and
  its guard bits `unkCC & 7` / `& 0x407`; 0x24-0x29 the body angle `unk40` / `unk3E`; 0x2A-0x31 the quadrant `unk42` of
  this / the other fighter; 0x33 the other's attack level 0x412 before its window at a distance below 0x700; 0x34 / 0x35
  `unk160` and `unk162` both zero / not; 0x36 / 0x37 a velocity `unk3E8`-`unk3F0` (0x37: and the other not juggled nor
  crouching); 0x38-0x3B the other's flag 4; 0x3C the other's `unk13A` without juggle; 0x3D-0x3F the latched buttons
  `unk438.unk83`-`unk85` while standing (each sets `unk82`); 0x40 life 0 and `D_005F5F30` clear; 0x41-0x44 the partner
  state, with the body angle; 0x45 no launch record and standing; 0x46 / 0x47 airborne `unk16A`; 0x48-0x4F the direction
  to the other fighter `unk4C` in 0x200-wide sectors (with the angle); 0x50-0x5B the camera quadrant `unk54` with the
  partner state; 0x5C the three values `unk628`-`unk62C` all above 0x214 or all below -0x14; 0x5D distance below
  0x1D01; 0x5E `func_0035B9E8 != func_0035BA00`; 0x5F the side is behind (`unkB4 < 0`), `func_0036F3F8`, no
  `D_005F5F30`, and the match can end this round (round `gRoundNumber >= D_005F5C72` or a side at `D_005F5C70` win
  points); higher kinds never [V].
- Input codes (`Cmd_MatchInput`, matching C) [V code; I names]: without bit 0x8000 a direction / button test of this
  frame (`func_0036CD10`; codes with bits 0x3FE0 also need one of this frame's direction bits `unk438.unk7C`); 0x8001
  / 0x8002 a double tap forward / back (`func_0036CAF8`: direction 6 / 4 in this frame's entry, then neutral 5, then
  the same direction again, within 0x14 history entries); 0x8003 the armed flag `unk12B`, or for character 0x21 the
  latched button `unk140` while the round runs (both clear `unk12B`); above 0x8005 a sequence of `D_004EFD44`
  (`func_0036CDD8`, matching C: `seq[0]` is a frame budget, the last step is tested against this frame, the earlier
  ones walking back through the history; a history entry whose buttons do not match ends the search). This frame's
  direction bit and button words come from `func_0036C988` (matching C: `unk7C = 1 << (4 + direction)`, buttons 0x80,
  0x10, 0x40, 0x20, 0x04 as 1, 2, 4, 8, 0x4000 in `unk80` held / `unk7E` pressed); `func_0036CA68` (matching C) latches
  the presses of 0x80, 0x10 and the pair 0x90 in `unk83`-`unk85` while `unk82` is set.
- Conditions (`Cmd_CheckCondition`, C attempt in `#if 0`, read in full) by id: 0 always; 1-0x21 the variant `unk28` is
  id - 1; 0x22-0x42 it is not id - 0x22; 0x43-0x63 the other fighter's `unk28` is id - 0x43; 0x64-0x84 the partner's
  character class (`func_003AD8E0`) is id - 0x64; 0x85-0xA6 the character `unk2A` is id - 0x85; 0xA7-0xC8 the partner
  (`D_00440940[side][unk2]`) is character id - 0xA7 (both partner tests fail while `unk402` is set); 0xC9-0xEA the
  character is not id - 0xC9; 0xEB `unk418` (CPU-driven); 0xEC `unk124`; 0xED `unk124 == 0`; higher ids always [V].
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
- **`Fighter_StepMove`** (0x36A428, asm, 0x1828 bytes; rewritten in T14 b5 from the assembly in the original's case
  order with its shared tails, C attempt in `#if 0`, about 390 of 1546 instructions still differ, mostly the store
  order inside the cases) [V unless marked]:
  - nothing happens while `unk127` is 0; `unk170` counts down to 0;
  - the pending move (`unk188`) starts when `unkC4 >= unk178` or the current record's length `unk18` is reached
    (`unkDA > 0`; `unkDA < 0` plays backwards to frame 0; `unkDA == 0` starts at once). A move never ends by itself:
    it runs until a pending move starts;
  - otherwise the frame advances: `unkC8++`, `unk134 = 0`; a hit freeze `unkE4 > 0` is dropped while `unkFE` is set
    (launched); a non-zero freeze counts towards 0 (negative values count up) and holds the frame, and the frame
    after it is `unkC4 + 1` with `unkB8 = unkC0`; else, with `unkD6 >= 0` the frame `unkC0->unk1B` is held `unkD6`
    more times, with `unkD6 < 0` and `unkFE` set the frame advances by one until `unk1B`, otherwise `unkC4 += unkDA`
    (at least 1); `unkBC` follows `unkC4`;
  - when a move starts: the tag / character change of section 5.7, `func_003D2A38`, `func_0036BDA0`, then the
    resets (`unkD6 = -1`, `unkDA = 1`, `unkE4 = 0`, hit bytes cleared), `unk110` / `unk114` = the other fighter's
    position, the damage `unkCA` (`unk148` when set, else `func_0036C228`), the flags from the record: `unkCC` =
    `unk4`, `unkD5 = (unk4 >> 11) & 0x1F` (move state), the attack level `unkD0` = the low half of `unk8`, `unkD4 =
    unk8 >> 8`, the guard mask `unkD2 = unkCC & 0x18` (both cleared for flag 0x100 unless `unk12C`); the hit freeze
    `unkE4 = unk14C` when the fighter was hit (`unk12F`) and still stands (`unk40C`); `unkE2` keeps the throw counter
    `unkE0` across a command 0x10 move; `unk134` is set when the record changes or `unkF8` is 1 / 3; the old move's
    flag 0x200 sets `unk16A = 0x78` (airborne for 120 frames [I]); the new move's flag 0x8 sets `unk164 = 9`; its
    flag 0x1000000 sets `unk129` from `func_0035BA00`; `func_00370F28` updates the facing quadrants (section 5.9);
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
  | 0x1E, 0x26 | launch (section 5.10): frame `unk178`, `unkFE = 1`, `unk11D = 1`, `unk100++`, `Fighter_StartLaunch`, `unk160 = func_0036A2D0`; 0x1E also queues move `func_00374010(4)` (`unkFA = 1`, `unkFC = 4`) |
  | 0x1F, 0x20 | throw pairing: `unkE0` counts frames; on the first one the fighter turns towards the other (by `o->unk42`: +0x4000, 0x8000, -0x4000) over 16 frames and takes its position |
  | 0x21, 0x22, 0x23 | facing `unk3A` and snap (0x21); `unk11E` 8, 0xC, 0xD |
  | 0x24 | thrown / hit reaction: `unk120` set when the reaction entry (`func_0036C1A0`) does damage before the attacker's window or the attacker is airborne, body turned by 0x8000 and snapped on the first frame (`unkE0 >= 0`), `Snd_StopVoiceSeq`, `unkE0` counts down, `func_0036A330` (the reaction's frames `unk162` and the damage-scaled `unk160`, 0x22-0x42) |
  | 0x25, 0x27-0x2D | paired paths read through `unk18C` (`struct FighterUnk18C`) into `unk150`-`unk158`, `unk156` the angle; 0x29-0x2D call `func_0036C128` |
  | other | frame 1, facing `unk3A` |
- **Move events** (`Fighter_RunMoveEvents`, 0x372BD0, matching C since T14 b6, called by `func_00372BA8` for both
  fighters in step 9 before the damage) [V code; I names]: `unk13D` is cleared, then, while the fighter is drawn, every
  entry of the current record's `unk20` list (`struct MoveMod`: frame `unk0`, code `unk2` = kind << 8 | argument) whose
  frame is this frame (`unkC4`, once: only when `unk134` is set or the frame changed since `unkC6`) runs: kinds 1-3
  camera shake `func_0035C2C8(0-2)`; 4 an effect at the fighter's position (`func_0035C240`, `func_003BB228(side, 2)`,
  the camera target `func_003D3C68`); 6 / 7 an effect for the own / other side at joint `argument` (`func_0035BF70`,
  joint positions every 0x50 bytes from `Fighter + 0x668`); 8-11 `func_0035BFF8(0 / 1 / 3 / 2)` at the joint; **0xC /
  0xD / 0xE the scripted damage**: `unk14A = argument`, `unk13D = 1 / 2 / 3` (section 5.6); 0xF-0x6E a sound or voice
  `func_00362E90(f, (kind - 0xF) / 32 or 3, (kind - 0xF) % 32, argument)` with `unk11F = 1` (kind 0x18 maps the
  number through `func_00366540`). Kind 0xC also feeds `func_003682F8` (damage changes on load).
- `Fighter_GetContinueFrame` (0x36BF58, matching C): for the move kinds that keep the frame running (2, 0x12-0x16,
  0x19, 0x1A, 0x1D, 0x2E) the frame `unk178` the pending move starts on, else 0 (`func_003737B0`'s blend test) [V].
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
- `Math_Sin` / `Math_Cos` / `Math_Atan2` are integer table lookups (`gSinTable`), and `Battle_UpdateMovement` (section
  5.12) uses only those [V]. The hit test is integer as well (fixed point 20.12, 64-bit products, `__divdi3`) [V].
- Floats in the simulation path [V]: (1) the red-life loss in `Fighter_ApplyDamage` (`(f32)red - (f32)(damage << 12) *
  0.6f` or `1.4f`, back with `cvt.w.s`), which decides when `unk40C` drops (K.O.); (2) the attack volume of character
  0x1D, whose angle comes from `atan2f` of two points moved with `sceVu0ApplyMatrix` (`func_00378760`). The EE FPU is
  not IEEE 754 (it rounds toward zero, has no denormals or infinities) and `atan2f` is newlib's: two PC peers agree
  if they run the same binary with SSE single precision, but matching a PS2 needs EE-style rounding [I: EE FPU
  behaviour from the R5900 manual, not measured here]. `Fighter_RunMoveEvents` and `Battle_UpdateFrame` use floats only for
  the camera targets (`func_003D3C68`).
- More randomness: `func_0036C278` / `func_0036C318` (character variant of a tag-like command, `unk182`) draw with
  `func_003800D8` (`rand()` plus its own seed `D_005F6C08`); the tag swap reloads the character through
  `func_0035C850` -> `Rng_PickRandomEntry` -> `Rng_SpinNext` (characters 0xF and 0x21) [V calls]. Netplay must keep
  `rand()`, `D_005F6C08` and `gRngSpin` in step on both machines.
- The win pose at the end of a round is `func_00368528(winner, loser, gFrameCount & 1)` (section 5.11), and the CPU's
  candidate pick uses `rand()` with `D_005F6C08` (`func_003853E8`) [V].
- Frame counters that gameplay reads: `gFrameCount` (parity at round start, pose of character 0x21, win pose), the round timer
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

### 5.8 Throws (T14 b5)

There is no separate throw routine: a throw is a move whose attack level is 0x10F, resolved by the hit pass and the
move step like any attack, with a pairing record that drives both fighters [V code; I for "throw" and the names].

- **Attack levels** (`unkD0`, the low half of the move record's `unk8`): 0x10F throw; the CPU code also tests 0x412,
  0x217 (other grab kinds [I]: the break conditions below accept them) and 0x706 [V values; I meanings].
- **Grab range** is the hit test of section 5.5 (the throw's attack volumes against the body spheres); there is no
  distance check of its own. `Battle_ResolveHits` (matching C) adds three exceptions [V]:
  - a throw misses a defender in move state 0xE (`unkD5`) between the frames `unk19` and `unk1A - 5` of its move, or a
    juggled defender (`unk135`) [I: throw-immune states];
  - attack level 0x907 only hits a juggled defender or one whose move has flag 0x400;
  - nothing hits a fighter in the frame after a tag (`unkB134`), and the moves 0x12A2-0x12A5 of character 0x1D never
    hit.
- **Pairing**: a hit sets the defender's `unk18C` to a record of `D_0046BE78` (0x38 bytes, `struct FighterUnk18C` in
  include/fighter.h: an angle offset, three lists of s16 pairs at 0x04, 0x0C, 0x14 / 0x18, and the move id of the
  thrown fighter at 0x1C), indexed by the attacker move's first reaction entry (`D_005E8978`, `func_0036C1A0`): its
  `unk6` when the fighter distance `D_005F5C28` is below the entry's `unk8` (close; also sets `unk13B`), else its
  `unk4`; 0x592 and 0 mean no record [V].
- **Thrower**: move kinds 0x1F / 0x20 (`Fighter_StepMove`) turn the thrower to the other fighter by its quadrant `unk42`
  over 16 frames and put it at the other's position, `unkE0` counting the frames [V]. When a kind-0x20 move reaches
  its end, `Battle_StepMoves` first starts the victim's move (`Fighter_StartThrownMove`, 0x3688B0, matching C since
  T14 b6) [V]: the thrower's pending move (`unk188`) gives its first reaction entry `e = func_0036C1A0(thrower, victim,
  thrower->unk188)`, and the victim starts move `D_0046BE78[e->unk4].unk1C` at frame 1 as kind 0x24 (`unkC4` =
  `unk178` = 1), takes over the thrower's `unk17C` (which is cleared), clears `unk17E` / `unk180` / `unk182` and drops
  its buffered command; the move step then runs it.
- **Victim**: kind 0x24 (thrown / reaction) turns the body by 0x8000, snaps the position, stops the voice and sets the
  reaction length (`func_0036A330`); kinds 0x25 and 0x27-0x2D play the record's paired paths (`unk150`-`unk158`, the
  angle `unk156` from the record's offset) [V].
- **Throw break**: a command entry whose condition kind (`unk3 & 0x7F`, `Cmd_CheckThrowCondition` 0x369E10, C attempt in `#if 0`) is
  0x63 applies only while the other fighter's current attack level is 0x10F (0x61: 0x412, 0x62: 0x217, 0x64: either),
  both fighters face each other (`unk40 < 0x2AAA`), the attacker's move lacks flag 0x8, and the entry's `unk4` is one
  of the four bytes 0x28-0x2B of the attacker's move (`func_0036A008`, the break buttons [I]); the **break window** is
  the entry's frame window `unkB..unkC` of the escape command list (`Move_FindCommand`) [V code; I meaning].
- **Throw damage** [V code; I that throws use it]: the throw's hit itself does the move's damage like any hit
  (`unkCA`, `func_0036C228`, `Fighter_ApplyDamage`, section 5.6). During the paired animation the damage comes from the
  move events of section 5.3: an event of kind 0xC / 0xD / 0xE at frame N of a record sets `unk14A` to the event's
  argument and `unk13D` to 1 / 2 / 3 in that frame (`Fighter_RunMoveEvents`, cleared again the next frame), and
  `Fighter_ApplyDamage`, which runs right after in the same frame, applies it: 1 the other fighter's `unk14A` (or its
  `unkCA`), 2 the own `unk14A` / `unkCA`, both times 13/10 with the rage flag `unk400`, the red life losing 0.6 of it,
  and 3 heals. So the damage of a throw is the sum of the event arguments placed on the frames of the thrower's and the
  victim's records, in whole points, applied on exactly those frames. `func_003682F8` (matching C) adds a record's kind
  0xC changes to all its `D_005E8978` entries when the record is loaded (the first entry twice) [V].
- **Throw positions** [V code of the C draft; I meaning]: the thrower snaps to the victim (kinds 0x1F / 0x20 above);
  the victim's kind 0x24 turns it by 0x8000 and snaps it to `unk51C` / `unk524`; the paired paths (`unk150`-`unk158`) move
  the victim each frame by `unk152` plus the next s16 of the path (`*unk158++`, `unk154` entries; with `unk16C > 0`
  also `50 * n - 10 * (n - 1)`) along the angle `unk156`, through `Math_Sin` / `Math_Cos` / 32767
  (`Battle_UpdateMovement`); the "slide" state `unkF0` (1 then 2) moves a fighter from its record's frame `unk19` to
  `unk1A` by `(min(D_005F5C28, 0xC00) - 500) / frames` units a frame along its facing `unk3A`, then snaps it to
  `unk51C` / `unk524`.
- Outside the C files of this batch: the throw move data are in `D_0046BE78` (.data 0x46BE78), `D_005E8978` (.data
  0x5E8978) and the command lists `D_0047F780` / move records (data, not decompiled); `func_00368B08` / `func_00368958`
  (launch reaction, `battle_flow`, C attempts) and `func_0036A008` (`battle_flow`, C attempt) [V addresses].

### 5.9 Facing, positions and round results (T14 b5)

- `func_00370F28` (C attempt) runs for both fighters at every move start: `unk3E = unk3C - unk3A` (body against
  facing), its quadrant `unk42` (0 front, 1 / 3 the sides, 2 back), `unk40 = |unk3E|`, the facing target `unk44` when
  the fighters are more than 0x200 apart, the direction to the other fighter against the camera `unk4C` (0-0xFFF,
  `func_00366108`), and quadrants / octants of the body against the camera `unk50`-`unk58` [V code; I meanings].
- `func_003730E8` (C attempt) puts a fighter at its round start: side 0 at x = -1000 facing 0x4000, side 1 at 1000
  facing -0x4000, z 0, velocity and gauge cleared, then `func_00373218` (matching C: clears the move and hit state),
  voice, body spheres and input state [V].
- `Battle_UpdateGauges` (matching C): the gauge `unk408` of each side, the average of the team members' life against their
  maximum in 20.12 (one member in the team: its value), 0 when a member has no life left, at least 1 [V].
- `Battle_SetRoundResult` (matching C): the round result bits `D_005F5CAC`: 1 time up with both standing, 0x20 draw (6 when
  both gauges are empty), 2 K.O., 8 when the winner lost no red life and took no chip damage (`D_005F6670`), else 0x10
  when the winner's gauge is at most 0xCC; `func_0036EF58` / `func_0036EEE0` (matching C) show the matching text and
  sound effect [V code; I names].
- `func_0036BFA0` (matching C): the attacker's hit reaction direction `unk172` from the defender's quadrant and guard
  mask (0xE-0x11, 0xA-0xD, 2-9, 0x12 / 0x13, else -1) [V code; I meaning].
- `func_0036C578` (C attempt): pad shortcut [I: side step]: a press of pad bit 0x02 (0x08) adds up 0x1000 (down
  0x4000) for one frame and keeps the shortcut for 0x23 frames [V code].

### 5.10 Launches (T14 b5)

- `Fighter_GetLaunchKind` (matching C) picks the launch record `unkFC` (`func_00374010`): the attacker move's byte 0x2F when
  set, else 1 for a fast fall (`unk3E0 >= 110`), 2 / 3 / 0 by the quadrant `unk42`; 7 / 5 / 6 for fighters in flag 0x200
  moves or moves 0x129B-0x129F (by the fall speed and the height `|unk520|` against 0x2DF) [V code; I meaning].
- `Fighter_StartLaunch` (C attempt, read in full) sets the arc: the height to reach `unk102` (the record's `unk2`, else the
  move's position at its last frame, `func_00363AB8`), the horizontal speed `unk104` (0-100: `unk146 + (unk100 + 1) *
  7 + 10`, +10 for record 4) split by the facing into `unk106` / `unk10A` (`Math_Sin` / `Math_Cos`), the vertical speed
  `unk108` from `unk146 * 4 - (unk3C8 + unk3E0) / 8 - (unk100 * 7 - 40)` (-100..100), the frames in the air `unk178` from
  `(vy + sqrt(|vy^2 + 12 * dy|)) / 6` and the move's remaining frames, so that the arc lands when the move continues;
  integer math only [V code; I meaning].
- `unk100` counts the hits of a juggle (`Fighter_ApplyDamage` scales the damage by it, section 5.6) and raises the
  launch speed [V].

### 5.11 Round flow (`Battle_UpdateRound`, 0x36E198, matching C since T14 b6)

The round state is `D_005F5E88` [V code; I names of the states]. Every frame the function clears `D_005F5F24`, counts
`D_005F5F28` (frames in the current late state) from state 2 on, runs the state, then sets `D_005F5F30` (both fighters
have a start record `unkB12C`), `D_005F5E90` (the state changed this frame) and `D_005F5E8C` (the state of this frame).

| State | What happens | Next |
|---|---|---|
| 0 intro | `D_005F5EE8` counts frames | after 0x97 frames in round 1 (0x5B later): `func_0036C490` for both, state 1 |
| 1 fight | `D_005F5EE8` counts; `D_005F5EF4 = Battle_IsRoundOver()`; while it runs, `gRoundTimer` counts down unless the time option `D_0043B7F8[0xB]` is 5 (no limit), and the play-time word at `D_0043B7F8 + 0x2C` counts up to 0x57E3F | when over: `func_0036F8D8` for both, `Battle_UpdateGauges`, `Battle_SetRoundResult`, `func_0036F6B8` (win points), `func_003AE440`, `func_003D0660`, the flag `D_005F5CB4` (an option-dependent bit test of `D_0043B7F8[0x1B]`), the delay `D_005F5E94` = 90 frames for a time up (result bit 1) else 240, `func_003D35B8`; state 2 |
| 2 round end | a K.O.'d fighter (`unk40C == 0`) or, at a time up, every fighter (and any CPU fighter) gets `func_0036C480`, except a winner (`unkB4 > 0`) whose flag 4 is set (it holds pad 0x1000 through `unk420 = 4` / `unk430`); time up: at frame 0xA0 `func_003D0C08(1)` + `func_0036F918` ends it, at frame 0x32 `func_003D2B88` when the match is decided; K.O.: ends after 0x78 frames (when `D_005F5CBC` is set and `func_003ABCC0` is 0) or when `func_0036F920` says so; other results after 0x258 frames | input reset, voices stopped, sound calls, BGM `func_003751A0(D_005F5CB6)`; state 3 (game type 9: state 5) |
| 3 result | `D_005F5F20 = 2`, voices stopped; unless game type 9, a draw in game types 1 / 2 / 6, or a draw while the match goes on: both fighters drawn, then either the special ending (option `D_0043B7F8[0x1F]`, match decided, `D_0043B7F8[7] == 0`, `D_005F5CBA` 1 with option 0x20 == 1 or 2 with 0x20 == 0, not game type 2: both fighters start their start move `func_00368748`, the winner's `func_003957A0`, `gRoundPhase` 0x12 when side 0 won, 0x11 for side 1) or the win pose (`func_00368528(winner, loser, gFrameCount & 1)`, `gRoundPhase` 0xE / 0xF) | delay 300, `unk16A` cleared, `func_003C1620` and resets; state 4 |
| 4 result wait | `D_005F5F20 = 2`; phase 0xD ends at frame 0x4B, phases 0x11 / 0x12 at frame 0x3C, phase 0x10 when the winner's move (both moves in game type 5 or with the partner flag `unk402`) reaches its last frame; the delay counts down | at the end, the delay at 0, `D_0043B817 == 0` with `D_005F5F00` from frame 2, or pad bit 0x800 (`func_003712C0`): `func_00375448`, `func_00375428`; state 5 |
| 5, 6 | nothing (the mode moves on) | |

Matched in T14 b8 [V code; I meaning]: `func_0036EC50` plays the round call by the frame `D_005F5EE8` (round 1 /
later rounds: 0x3D / 1 sound 0x105A or 0x105C in the last round, 0x64 / 0x28 the round number's sound from the table
`D_00500458`, 0x96 / 0x5A sound 0x105B). `func_0036F920` runs the K.O. steps by the frame `D_005F5F28` (0x10 / 0x11
with the option `D_0043B7F8[0x39]`, which counts up to 3 repeats, 0x32 `func_003D2B88` when the match is decided, 0x6E,
and 0x87, where it returns 1 and sets `D_005F5C74`). `func_003685B8` starts the pose move 0x12A2-0x12A5 chosen by
`func_00368528` (with `unk2E` 0x21 the pose 0 becomes one of the other three by `gFrameCount % 3`; `unkB12C` overrides
move and position). `func_00372E98` keeps the fight near the origin: when a fighter's x / z leaves +-0x78000 both
fighters move so that the first one stands at 0, the shift is kept in `unk10` / `unk14` of both and `func_0038C0F0` /
`func_00350578` follow. `func_003691D8` is `Move_FindCommand` for the situation kinds below 0x60, filtered by the input
class of the newest history entry (`func_00369188` against the entry's `unkA`).

### 5.12 Movement and per-frame flags (`Battle_UpdateMovement`, 0x36FB40, C draft)

Read in full from the assembly; the C draft in `#if 0` follows it (about 720 lines still differ) [V code; I meanings]:

- First `unk136` / `unk137` / `unk139` are cleared and `func_00370F28(b, a)` updates the facing quadrants.
- Per fighter: a fighter not in hit freeze drifts vertically (`unk108 += 6`, `unk8 += unk108` while `unk8 < 0`, else
  both 0); the slide `unkF0` (section 5.8); a launched fighter (`unkFE`) moves by `unk106` / `unk10A` and lands when
  `unk8 >= -unk102` with `unk108 >= 0` (snap to `unk51C` / `unk524`, `unk136` or, for launch record 4, `unk137`, launch
  state cleared); otherwise inside the record's window `unk19 - 1 .. unk1A` the record's push `unk16` moves the fighter
  backwards along `unk1A` with a 24.8 remainder per side (`D_005F6BD0` / `D_005F6BD8`); the turn spread `unk15C` /
  `unk15E`; the paired paths; then the turn rule of the state code `unk11E` (0 body = facing, 1-5 turns towards
  `unk44` / `unk3A` limited by `func_00370E98` / `func_00370ED0`, 6 a turn every 5 frames after the window, 7 / 8 / 12 /
  13 turns spread over the move or 8 frames, 9 a turn towards the opponent's start position when the fighters are at
  least 0x4B0 apart, 10 a fixed +-0x11C7 / 0x71C swing).
- Then per fighter: the timers `unk164`, `unk14E`, `unk160` (faster with buttons held, history bits 0x20 / 0xF0),
  `unk162`, `unk16A` (cleared when a hit lands), `unk16E` / `unk16C`; the attack window flag `unk13C` (2 with the
  record's `unk2A`); the juggle flag `unk135` (`unk100`, or the frame inside `unk19 .. unk1A`); at the record's frame
  `unk1A` the end flags `unk137` (thrown or crouching, `unkD5 == 9` / flag 4) or `unk136`; the guard flags `unkCC` /
  `unkD2` from the record's flags 0x40 / 0x100 and the held direction (0x1052 for direction 4, 0x2829 for 1, a table for
  0x100: 0x1952, 0x3129, 0x2829, 0x1052, 0x2021, 0x842), cleared while juggled or turned away (`unk40 >= 0x4000`); the
  hit sounds (`func_00363110` / `func_003630D0`); `unkB052`; `unk124` for the character pairs 0x12 / 0x1D and 0x1D /
  0x13.

### 5.13 T14 status (2026-10-19, batch 9)

Every function of `fighter_core`, `battle_flow` and `fight_misc` is now C: matching, or a `NON_MATCHING` draft in
`#if 0` beside its `INCLUDE_ASM`. Still `NON_MATCHING` in the simulation path (`battle_flow`, `fight_misc`):
`Fighter_StepMove` (the move step), `Battle_UpdateMovement`, `Fighter_Push`, `Fighter_StartLaunch`,
`Cmd_CheckThrowCondition`, `Cmd_CheckCondition`, `func_00370F28` (facing), `func_0036D5D0`, `func_0036C278`, the motion
blend (`func_00373428`, `func_00373930`, `func_003736B8`, `func_00373D48`, `func_00373E38`), `func_00368B90` (move start),
`func_0036D010` (fight set-up), `Battle_UpdatePause`, `Battle_UpdateDistance`, the input helpers `func_00369188` /
`func_0036A008` / `func_0036C918` / `func_0036CAF8` / `func_0036CD10`, `func_0036F010` / `func_0036F6B8` (round end),
`func_00368398`, `func_0036BF00`, `func_00370E98`, `func_003730E8`, `func_00376CF8` and `func_00378810` (character
0x1D's volume). On the CPU side (`fighter_core`) 57 drafts remain, the largest the deciders `func_00381D30` /
`func_00389D58` / `func_00388FA8`, their situation codes `func_00388530` / `func_00389488`, the candidate list
`func_003865A0` and the CPU set-up `func_00380568`. The drafts compile and follow the original control flow, so the
behaviour can be read from them; only byte identity is missing.

## 6. Open

- The meanings of most move kinds (`unk17A`, table in section 5.3), of the state codes `unk11E` / `unkD5` and of the
  move flag bits; what the push-radius kinds (`unkC0->unk30`) stand for; the attack levels other than 0x10F.
- The functions listed in section 5.13 have C attempts in `#if 0`; the meaning of the round phases
  `gRoundPhase` 0xD-0x12 and of the event kinds 6-11 (effects) is inferred.
- Which records carry the damage events of each throw (the event lists are game data, read only at run time) and
  what the pairing record lists at 0x04-0x18 hold exactly.
- What `D_00440940[side][member].unk28` counts (100 at round start, raised by `func_003CF278` per hit taken, compared
  with 120 by `func_003C1A78`); the meaning of `unk400` (13/10 damage) and of `D_005F6670[side]` (set by chip damage,
  cancels the "perfect" bit).
- The meaning of `Fighter.unk420` values other than 3 (practice playback); what `func_003F0008` (scripted tag swap)
  serves.
