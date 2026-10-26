# PC port: input (pads, gamepads, multitap, vibration)

T34. The game reads its pads through libpad and libmtap (`src/pad.c`); the port answers them (`port/src/plat_pad.c`)
from the keyboard and SDL gamepads (`port/src/plat_window.c`, window thread) or from a recording. The basics (what the
game sees, the per-VBlank latch, recording and replay, the default keys) are in the README's [Input](README.md#input)
section; the settings are in [settings.md](settings.md). This page covers the gamepads, the multitap and the
vibration.

Tests: `TTT_SELFTEST=pad` (`tests/test_port_input.py`, class `PadSelfTest`), `TTT_SELFTEST=settings`
(`tests/test_port_settings.py`), the overlay's capture path (`tests/test_port_window.py`, class `Overlay`), the
multitap recording (`tests/test_port_input.py`, `test_multitap_recording`).

## What the game does with its pads

Read from the code, not from a manual:

- **Multitap: yes, on port 0 only.** `Pad_Init` opens libmtap on port 0 and `Pad_Update` follows
  `sceMtapGetConnection(0)` every frame (`src/pad.c:57-65`, `src/pad.c:156`). The game polls five connectors,
  `gPadSlots` (`0x436280`): port 0 slot 0, port 1 slot 0, then port 0 slots 1-3. Its player map `D_00509D00`
  (`src/pad.c:36-43`; bytes in the executable: `0, 1, -1, -1` without the multitap, `0, 2, 3, 4` with it) puts
  players 1-2 on ports 0 and 1 without it and players 1-4 on slots 1A-1D with it: port 1 is then read by nobody.
  The modes check it: `Pad_CountReady` counts 4 players with the multitap (`src/pad.c:345-357`), and `src/modes_b.c`
  (lines 486, 532-553, 654, 1141-1148, 1305-1414), `src/select_results_b.c:4221`, `src/pause_results.c:1651` and
  `src/tekken_bowl.c:1726` branch on `Pad_IsMultitap` / `Pad_CountReady` (four-player team and tag set-ups, TEKKEN BOWL).
- **Vibration: yes.** `Pad_Update` sets the actuator alignment `00 01 FF FF FF FF` (`src/pad.c:249`: data byte 0 the
  small motor, byte 1 the large one) and `func_003BA888` (`src/modes.c:1041-1058`) sends `scePadSetActDirect` with the
  small motor on / off and the large one 0-255 when the player's vibration switch (`func_003BA818`, from the options'
  controller set-up `src/options.c:299,1048-1074` and battle mode `src/battle_mode.c:975-981,1117-1121`) is on and the
  game is not paused (`func_003B6220`). `func_003BA938` stops port 1's motors when the multitap appears
  (`src/modes.c:1059-1065`); the options menu offers vibration only for pads with more than one actuator
  (`src/options.c:964`).
- **Digital play.** The fight reads the d-pad: `Input_Update` (`src/main.c`) takes the buttons, and the sticks only
  add two "stick pushed fully" bits (`Pad_AddStickBits`, `src/main.c:936-952`): nothing on that path turns the
  sticks into directions. The port therefore lets the left stick press the d-pad (`input.stick_dpad`, on by default); the stick
  bytes still reach the game as they are (after the dead zone).

## Gamepads

- **Which gamepad is which pad.** The first four gamepads SDL finds are pads 1-4, in the order they connect
  (`ttt: input: pad N: <name>`); a removed gamepad frees its pad (its buttons are released) and the next one connected
  takes the first free pad. The keyboard plays pads 1 and 2 together with gamepads 1 and 2.
- **Bindings** (`pad.p1.cross = south` ... `pad.p4.r3`, settings.md): each pad has its own gamepad layout, by default
  the d-pad, south cross, east circle, west square, north triangle, shoulders L1 / R1, triggers L2 / R2 (past half way),
  start Start, back Select, the stick clicks L3 / R3. Inputs are SDL's gamepad buttons by position (`south`, `east`,
  `west`, `north`, `back`, `start`, `left_stick`, `right_stick`, `left_shoulder`, `right_shoulder`, `dpad_up` ...,
  `misc1`, the paddles, `touchpad`, `misc2`-`misc6`) and `left_trigger` / `right_trigger`; `none` unbinds. One input
  presses one button of its pad: binding it to another button gives that button the old input (they swap). The guide
  button is reserved (it opens the settings, as F1 does).
- **Where the mapping happens.** The window thread keeps each gamepad raw (buttons down, buttons pressed since the
  last VBlank, the six axes); `Port_WindowPads` makes them into libpad's bits and stick bytes at the VBlank, on the
  game's thread, through the current bindings and options. A tap between two VBlanks still counts once; a rebinding
  acts at the next VBlank. Recordings hold the result, so replays do not depend on the bindings or the options.
- **Sticks** (integers only, `stick()` in `plat_window.c`). `input.deadzone` (0-90 %, default 20): inside that radius
  the stick is centred (0x80); outside it each axis is stretched back to the full range, so the edge still reaches
  0x00 / 0xFF. `input.stick_dpad`: past half way, the left stick presses the d-pad in eight 45-degree sectors (a
  diagonal presses both directions within 22.5 degrees of it). The right stick never presses the d-pad.

## Multitap

`input.multitap` (`TTT_MULTITAP`, off by default, read at start) connects a multitap on port 0 with four pads on slots
1A-1D: `sceMtapGetConnection(0)` answers 1, `scePadGetState` is stable on port 0 slots 0-3 and port 1 is empty, so
pad 2 is the game's player 2 on slot 1B, and players 3 and 4 are pads 3 and 4 (gamepads 3 and 4; the keyboard has no
pads 3 and 4). All four pads are connected whether or not a gamepad is there (an idle pad reads as nothing pressed),
as four DualShock 2 plugged into a real multitap. Off, the port is the two-pad set-up of T34 b1. Headless runs without
a recording have no pads and no multitap (unchanged hashes).

Recordings: with the multitap the file starts `ttt-input 2 pads 4 multitap 1` and each line has four pads
(`<vblank>` then buttons and sticks of pads 1-4); without it the file is the version 1 format, byte for byte as
before. A replay uses the recording's own set-up, whatever `input.multitap` says; version 1 files never have the
multitap. `TTT_PADS` counts up to 4 with the multitap. `tests/test_port_input.py` replays a version 2 file: the game's
`gMultitap` (`0x5F6C5C`) is 1, its connectors 0, 2, 3 and 4 reach phase 0x30 and connector 1 (port 1) stays empty,
Start of pad 1 leaves the main menu, and two replays end in the same state with the same recording.

State: `pad.now` (pads 1 and 2) keeps its layout and stays in the gameplay hash; pads 3 and 4 and the actuator
alignments are a separate region (`pad.more`), saved and loaded but not hashed (what the game makes of them is in its
own memory, which is hashed). The multitap itself is fixed at start, from the setting or the recording.

## Vibration

`scePadSetActDirect` decodes its six bytes through the pad's alignment (`scePadSetActAlign`): the small motor on or
off, the large one 0-255. The window thread plays it on that pad's gamepad with `SDL_RumbleGamepad` (loaded like the
other optional SDL functions; without it there is no vibration): the large motor as the low-frequency rumble
(`value * 257`), the small one as the high-frequency rumble at full strength. A change is sent at once; a running motor
is sent again every half second for one second, so a stopped game or window thread never leaves a gamepad shaking.
Off: `input.vibration` 0, the overlay open, a hidden window (`TTT_WINDOW_HIDDEN`, the tests), a `TTT_INPUT_PLAY` replay
(its motors are not the gamepads'), and the second pass of `TTT_SYNCTEST`. Nothing of it reaches the game's state.

## The overlay with a gamepad

The guide button opens and closes the settings (F1). While they are open the gamepad drives Dear ImGui's gamepad
navigation: the d-pad or the left stick moves, south activates, east goes back (closes a popup). The
Controls page has the keyboard table (pads 1 and 2), the gamepad table of the chosen pad (Pad 1-4: click a button, then
press the gamepad's button or pull a trigger; guide or any key cancels), "Default buttons" for that pad, the four
gamepads' names, and the options: stick dead zone, left stick as d-pad, vibration, multitap (next start).

## Netplay

In a netplay session ([netplay.md](netplay.md)) the game sees two pads, no multitap: the local player's is the
window's first pad (the keyboard's player 1 keys, or the first gamepad), sampled once per VBlank and given to the game
`TTT_NET_DELAY` VBlanks later, as pad 1 on the host and pad 2 on the joining side; the other pad is the peer's.
`TTT_INPUT_PLAY` plays this side's pad from its column of a recording, and `TTT_INPUT_RECORD` writes both pads' final
states, a recording that replays the session offline (with `TTT_MC0=none`). Only the local pad vibrates, on the
window's first gamepad, and only from the first pass through a VBlank (not from the VBlanks a rollback runs again).

## Tests without the desktop

`TTT_SELFTEST=pad` drives the window's own input path with made-up SDL events built in the process: SDL is not
loaded, its gamepad functions are stand-ins (made-up handles, a recorder for `SDL_RumbleGamepad`), so no device is
opened or rumbled. It checks the four-pad lines and the version 2 header, the multitap's slots, a button through the
default bindings to the right slot, a tap counted once, the dead zone, the stick's d-pad sectors and its switch, the
right stick, a trigger as L2, guide pressing nothing, a removed gamepad releasing its buttons, and the vibration (the
game's alignment, small and large motor to the right gamepad, off, `input.vibration` off). The overlay test binds pad
2's cross to north through the gamepad capture path with a made-up event (`pad.p2.cross = north` in the file).

## Open

- Four players with the multitap are checked up to the game's own set-up (`gMultitap`, four ready connectors, the
  main menu); a four-player team or tag battle has not been played through.
- Gamepad navigation and rumble were checked on made-up events only (no physical gamepad in the tests).
- No keyboard pads 3 and 4; no per-gamepad dead zone; analog pressure bytes stay 0 / 0xFF (the triggers are digital,
  as the game reads them).
