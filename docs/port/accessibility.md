# PC port: accessibility (T48)

Everything here is off by default. Turn it on from the overlay's Accessibility page (F1), the settings file, or the
variables ([settings.md](settings.md)).

| Setting | Variable | What |
|---|---|---|
| `ui.scale` (50-300, %) | `TTT_UI_SCALE` | The overlay's and the setup screen's text and widgets, larger or smaller. |
| `ui.high_contrast` | `TTT_HIGH_CONTRAST` | White on black with white borders, and yellow for the focus, check marks and sliders. |
| `ui.screen_reader` | `TTT_SCREEN_READER` | Speaks the overlay's focused item and the setup screen's messages. |
| `ui.announce_game` | `TTT_ANNOUNCE_GAME` | With the screen reader on, also announces the game's menus and fights. |
| `ui.language` | `TTT_LANG` | The interface's language ([translations.md](translations.md)). Speech uses the same language. |

## Screen reader output

`port/src/plat_speech.c` sends each text to the first output that works:

1. **`TTT_SR_LOG=<file>`.** Each text is appended to the file as a line, and nothing is spoken. A line `--` marks an
   interruption (the focus moved before the last text was finished). Tests use this output. Another program can use it
   too, for example a braille bridge reading a FIFO.
2. **speech-dispatcher through `libspeechd.so.2`.** The library is loaded at run time with `dlopen`, so there is no link
   dependency and no PLT entry. The connection is `spd_open("ttt", "main", NULL, SPD_MODE_SINGLE)`, texts are sent at
   priority `SPD_TEXT`, a new focus calls `spd_cancel`, and the language is set with `spd_set_language`. The program is
   32-bit, so it needs the 32-bit library, which most distributions do not ship (Arch has no `lib32-speech-dispatcher`).
3. **speech-dispatcher's `spd-say` command.** It works whatever the program's bitness. It runs as
   `spd-say -l <lang> -- <text>`, preceded by `spd-say -C` for an interruption.
4. **Nothing.** The program says so once on stderr.

Speech runs on a thread of its own with a queue of 16 texts. A new focus drops whatever is still waiting. Callers never
wait.

### The overlay

The overlay uses Dear ImGui's test-engine hooks: `IMGUI_ENABLE_TEST_ENGINE` in `port/src/imconfig_ttt.h`. No test
engine is linked. With the hooks, every widget hands its label and state to `overlay.cpp`. The hooks run only while the
screen reader is on (`GImGui->TestEngineHookItems`).

After each frame, the overlay speaks what changed:

- **The focused window.** For example "Settings (F1)", "Quit?", "Setup".
- **The item with the keyboard or gamepad focus.** It speaks:
  - the label;
  - the state of check boxes and radio buttons ("checked" or "not checked");
  - the value of sliders and of the language list.

  A binding button speaks "Pad 1, Up: W", not just "W".
- **On the setup screen,** each new reason why a disc was refused, and each stage of the check.

Navigation is ImGui's own:

| Input | Effect |
|---|---|
| Arrow keys, Tab, or the d-pad | Move the focus. |
| Enter or the south button | Activate the focused item. |
| Escape or the east button | Go back. |

## The game's menus and fights

**Feasibility.** The decompiled code identifies, with evidence, the state that the menus and fights keep in memory.
Reading that state needs no change to the game, so a minimal subset is implemented in `port/src/plat_a11y.c`.

**When the reads happen.** The reads run once per VBlank. They happen in `plat_hw.c`'s VBlank, off the EE stack and
before the game's VBlank handler, so each read sees one whole frame.

**What the reads cannot change.** The reads never write game memory and never call game code. A run makes the same
announcements every time. Its state hash is the hash of the same run without them. `tests/test_port_a11y.py` checks this
on the scripted fight.

**The names spoken.** Menu items and character names are the game's own strings, read from its memory at run time.
Nothing of them is in this repository.

What is spoken:

- **The screen**, when `gMode` changes and holds for three VBlanks: "Title screen", "Main menu", "Character select",
  "Options", "Results", "Ending", "Theater", "Music player", "Gallery", "Name entry". Boot, loading and the fight itself
  are not spoken. A fight's rounds are spoken instead.
- **The main menu's highlighted item**, by its name in the game's menu table.
- **Character select:** each side's highlighted character, as "Player 1: \<name\>", or "Random".
- **A fight:**
  - "Round N. Fight!" when the round starts.
  - "K.O.", "Time up" or "Draw" when the round ends.
  - "Player N wins the round", with "Perfect!" when the round was perfect.
  - Each fighter's life whenever it falls into a lower quarter, as "Player 1: 48 percent".
  - "Player N tags in \<name\>" when the partner comes in.

A value is spoken only after it has held for three VBlanks, so a cursor that flickers during a transition is not spoken.

### The addresses

Every address below is the game's fixed address: the port maps EE RAM at the PS2's addresses, see `port/src/port_addr.h`.

| What | Where | Evidence |
|---|---|---|
| Screen | `gMode` s16 at 0x005F5C20 (with `gSubMode`) | `config/symbols/data.txt`, `docs/game_overview.md` (modes 2 title, 3 main menu, 4 fight, 5 select...), `tests/test_port_input.py` |
| Main menu cursor | s8 at 0x005F650E, an index into the visible items | `src/modes_b.c:150` (declaration), `:1318` (moved) |
| Main menu items | 12 entries `{const char *name; u8 id, locked, ...}` at 0x0051EFE0 | `src/modes_b.c:140` (`MbMenuItem`) |
| Which items are visible | Locked items are hidden, except id 8, 9 and 10 once their unlock flag is set: 8 TEKKEN BOWL (byte 0x00439C82), 9 THEATER (u64 0x00439C70), 10 GALLERY (byte 0x00439C83) | `src/modes_b.c:1173` (`func_003BD130`); the flags as `func_003BFEB0`..`func_003BFEE8` read them (`src/select_results.c:1007-1021`) |
| Select, each side | `SelSide` at 0x00440D08 + side * 0x1C: +0x10 highlighted character (0x36 none, 0x37 random), +0x14 costume | `src/select_results_b.c:93` |
| Character names | `SelChar *D_00415840[character * 8 + costume]`, `{const char *name; ...}` | `src/select_results.c:125-129`, printed as `%s` at `:361` |
| Round state | s32 at 0x005F5E88: 0 intro, 1 fighting, 2 round over, 3 result | `Battle_UpdateRound`, `docs/systems/fighter.md` 5.11 |
| Round number | `gRoundNumber` s32 at 0x005F5F04, counted at the round's start | `src/battle_flow.c:3336` |
| How the round ended | s32 at 0x005F5CAC: 1 time, 2 K.O., 8 perfect, 0x20 draw | `Battle_SetRoundResult`, `src/battle_flow.c:4313` |
| Fighters on screen | `gFighters` at 0x00879640, 0xB8A0 each (slot = side): +0x0 member in play, +0xB4 round won (1), +0x3F4 life, +0x3FC maximum life (20.12) | `include/fighter.h:380-382`, `src/battle_flow.c:4385` |
| Team members | `TeamMember D_00440940[2][2]` (0x30 each): +0x0 character, +0x4 costume | `include/fighter.h:487-504`, `src/pause_results.c:96` |

### Not covered

These are possible with more reverse engineering:

- The options menu's, the pause menu's (`D_005F6DE0`) and the theater's cursors.
- The game's dialogs and the results screen's text.
- The rounds' timer (`gRoundTimer`).
- The move lists.

Menu and character names are read as printable ASCII. A non-ASCII byte ends a name, and the game's names are plain
capitals.

## Tests

- **`tests/test_port_a11y.py`:** the scripted fight with the announcements on, in English and in Spanish, written to
  `TTT_SR_LOG`. It checks the hash against the same run without them. It also runs the overlay in Spanish in a hidden
  window (SDL's dummy driver, `TTT_OVERLAY_TEST`) and checks what was said, the accented glyphs, a 150 % scale with high
  contrast, and the hash. Finally, it checks that nothing is said by default.
- **`TTT_SELFTEST=settings`:** the `ui.*` defaults and the language's round trip.

No test speaks through the speakers, and no test sends input to the desktop.
