# PC port: settings overlay, settings file, quit question

T36. In the window, **F1** opens a settings window over the picture; the choices are kept in a text file. Closing the
window or **Escape** asks "Quit?" first (Escape again, or Yes, quits).

Code: `port/src/overlay.cpp` (the overlay, Dear ImGui), `port/src/plat_settings.c` (the file, the key names, the
bindings), `port/src/plat_window.c` (F1, the quit question, applying the settings), `port/src/imconfig_ttt.h` (ImGui's
configuration). Tests: `tests/test_port_settings.py`, `tests/test_port_window.py` (class `Overlay`),
`tests/test_port_float.py`.

## The overlay

| Page | What | When it applies |
|---|---|---|
| Video | Renderer (OpenGL / software) | next start |
| | V-sync, window scale (1x-6x of 640x448), fullscreen, frame-time meter (FPS, mean and worst frame time of the window) | at once |
| | OpenGL only (T37, [video.md](video.md)): internal resolution 1x-8x, aspect 4:3 / 16:9 / 21:9, the game's flicker filter, smooth scaling, low input lag | at once |
| Audio | Master, music and effects volumes 0-100, mute (T40: the port's output, window and `TTT_WAV`; the game's own sound state is unchanged, [sound.md](sound.md) section 4.1) | at once |
| Controls | The keyboard's keys for pads 1 and 2: click a key, press the new one (Escape cancels; F1, Escape and Tab cannot be bound; a key presses one button only, so the button that had it gets the old key). "Default keys". The gamepads of pads 1-4 (names) and each pad's gamepad buttons (T34, [input.md](input.md): click, press the gamepad's button or trigger; guide or a key cancels; guide cannot be bound), "Default buttons" | at once |
| | Stick dead zone 0-90 %, left stick as the d-pad, vibration | at once |
| | Multitap (four pads on port 1A-1D) | next start |
| Game | Movies on / off | next start |
| | Pause the game while the overlay is open (default on) | at once |
| | Game speed 0.25x-4x and the speed while Tab is held 1x-4x (T40, [Game speed](#game-speed)) | at once |
| | The disc, the data file, verified or `[modified game data]`, the settings file and a write error if any (read only) | |
| Cheats | Unlock everything (T40, off by default; it modifies the game's state, [cheats.md](cheats.md)) | at once |
| Mods | Music mods and texture mods on / off; the mods folder's mods in load order: each on or off, up / down, the tracks and textures each replaces, what an earlier mod hides, the track playing now; "Read the folder again" (T49, [mods.md](mods.md)) | music at once, textures next start |
| Accessibility | Language (T47, [translations.md](translations.md): automatic from the locale, or a catalog), interface size 50-300 %, high contrast, screen reader and the game's announcements (T48, [accessibility.md](accessibility.md)) | at once |
| About | The build (`git describe`), `[modified game data]` when set, Dear ImGui's version | |
| Netplay | The matchmaking server (address, name, direct connection first), create a room, join one by its code or quick match (each starts the game again in a session), the session's state, path, round trip and rollbacks, chat (T46, [matchmaking.md](matchmaking.md)) | server and name at once; a session from the start |

A setting whose environment variable is set shows `(set by TTT_...)` and cannot be changed there (the variable wins).
Mouse, keyboard and gamepads work in the overlay (arrows / Tab / Return / Space navigate; a gamepad's guide button opens
and closes it, the d-pad or left stick moves, south activates, east goes back). Without the overlay (SDL without the
functions it needs, or ImGui's GL renderer did not start) the run says `ttt: window: no settings overlay (F1)` and
Escape / close quit at once, as before T36.

### While it is open

- **Input.** The pads see nothing pressed (`Port_WindowPads` returns neutral buttons and centred sticks). A key or
  gamepad button pressed while the overlay is open does not reach the pads, and one still held when it closes counts
  only from its next press (or the keyboard's auto-repeat); releases always do (no button stays stuck).
- **Pause** (`game.pause`, default on). In real time the game's thread waits at its next poll outside the interrupt
  handlers (`plat_hw.c`, off the EE stack) until the overlay closes; the real-time clock's origin moves by the time
  waited, so no VBlank is dropped or added and the simulation goes on from the same state. Headless runs never pause.
  The window keeps drawing the last picture with the overlay over it.
- **Determinism.** Nothing of the simulation reads the overlay's output. The game's thread sees only two integers,
  both at the VBlank boundary: the pads' state (latched once per VBlank as before) and the pause flag (an atomic
  read). The settings the window applies (vsync, window size, fullscreen, volume, bindings) act on the window, the
  audio device and the keyboard mapping, all on the window thread. The idle run's 17 frames and hash
  (`0xd36c82e9a145875a` then, `0x8ba37caeb2bacfa8` since T42 b1) are unchanged, and `tests/test_port_window.py` checks that a run with the overlay driven
  through all its pages ends in the state of a run without a window.

### Host floating point

ImGui computes in host `float`. The port otherwise has no host float code (the game and the port run on the EE float
model, `softfloat_ps2.c`). The exception is limited to the overlay and ImGui:

- they run on the window thread only and never touch game memory;
- the C side passes them integers and pointers only (`overlay.h`); values the game could use stay integers in the
  settings (`plat_settings.c` has no float);
- `tests/test_port_float.py` lets x87 and SSE float instructions into the functions these objects define
  (`toolchain.host_float_objects()`: `pc_overlay.o` and `im_*.o`) and nowhere else, and checks they call no C library
  maths function (the program's `sinf` / `sqrtf` are the PS2's newlib in software float, which return in `eax`) and no
  C++ run time. ImGui's maths functions come from `imconfig_ttt.h` (SSE square root, polynomials).

## The settings file

Where: `TTT_CONFIG`, else `settings.ini` beside the program when it is portable (`portable.txt` beside it,
[install.md](install.md)), else `$XDG_CONFIG_HOME/ttt/settings.ini`, else `~/.config/ttt/settings.ini`. Nothing is written
until a setting is changed in the overlay; then the whole file is written at once: a temporary file beside it
(`settings.ini.tmp`), flushed and synced, renamed over the old one, so a crash leaves the old file or the new one.
The file's directory is also the port's user directory: crash reports go to its `crash/` subdirectory
([README](README.md#crash-reports)), with every setting's value at the time.

Format: one `key = value` per line. Blank lines, comments (`#`, `;`), `[section]` lines and keys this program does not
know are kept as they are, in place; a changed key's line is replaced, a new key is added at the end.

| Key | Values | Default | Variable |
|---|---|---|---|
| `video.renderer` | `gl` / `sw` | `gl` | `TTT_RENDERER` |
| `video.vsync` | 0 / 1 | 1 | `TTT_VSYNC` |
| `video.scale` | 1-6 (window 640x448 times it) | 2 | `TTT_SCALE` |
| `video.fullscreen` | 0 / 1 | 0 | `TTT_FULLSCREEN` |
| `video.fps_meter` | 0 / 1 | 0 | `TTT_FPS_METER` |
| `video.resolution` | 1-8 (OpenGL: GS pixels drawn N x N) | 1 | `TTT_RESOLUTION` |
| `video.aspect` | `4:3` / `16:9` / `21:9` (OpenGL; other text: 4:3) | `4:3` | `TTT_ASPECT` |
| `video.deflicker` | 0 / 1 (the game's flicker filter) | 1 | `TTT_DEFLICKER` |
| `video.smooth` | 0 / 1 (bilinear scaling into the window) | 1 | `TTT_SMOOTH` |
| `video.low_latency` | 0 / 1 | 0 | `TTT_LOW_LATENCY` |
| `video.texture_replace` | 0 / 1 (OpenGL: draw texture-pack replacements, [textures.md](textures.md)) | 0 | `TTT_TEXTURE_REPLACE` |
| `video.texture_dump` | 0 / 1 (OpenGL: write new textures as PNG) | 0 | `TTT_TEXTURE_DUMP` |
| `video.texture_dir` | a folder (read at start; empty: `textures/` of the checkout, else in the data folder, [install.md](install.md)) | (empty) | `TTT_TEXTURES` |
| `audio.volume` | 0-100 | 100 | `TTT_VOLUME` |
| `audio.mute` | 0 / 1 | 0 | `TTT_MUTE` |
| `audio.music` | 0-100 (the BGM and movie voices) | 100 | `TTT_MUSIC_VOLUME` |
| `audio.effects` | 0-100 (every other voice) | 100 | `TTT_EFFECTS_VOLUME` |
| `mods.music` | 0 / 1 (music mods, [mods.md](mods.md)) | 1 | `TTT_MOD_MUSIC` |
| `mods.textures` | 0 / 1 (texture mods, OpenGL; read at start) | 1 | `TTT_MOD_TEXTURES` |
| `mods.dir` | a folder (read at start; empty: `mods/` of the checkout, else in the data folder) | (empty) | `TTT_MODS` |
| `mods.order` | mod folder names, commas between them: the load order (the first wins a conflict) | (empty) | `TTT_MOD_ORDER` |
| `mods.disabled` | mod folder names, commas between them: the mods switched off | (empty) | `TTT_MOD_DISABLED` |
| `game.movies` | 0 / 1 | 1 | `TTT_MOVIES` |
| `game.disc` | the disc's folder or image (written by the setup, [install.md](install.md); read at start) | (empty) | `TTT_DISC` |
| `game.pause` | 0 / 1 | 1 | `TTT_PAUSE_IN_MENU` |
| `game.speed` | 25-400 (percent; the overlay offers 25, 50, 75, 100, 150, 200, 300, 400) | 100 | `TTT_SPEED` |
| `game.fast_forward` | 100-400 (percent, while Tab is held) | 300 | `TTT_FAST_FORWARD` |
| `cheats.unlock_all` | 0 / 1 ([cheats.md](cheats.md)) | 0 | `TTT_CHEAT_UNLOCK` |
| `input.vibration` | 0 / 1 (the game's vibration on the gamepads, [input.md](input.md)) | 1 | `TTT_VIBRATION` |
| `input.deadzone` | 0-90 (percent of the sticks' travel) | 20 | `TTT_DEADZONE` |
| `input.stick_dpad` | 0 / 1 (the left stick also presses the d-pad) | 1 | `TTT_STICK_DPAD` |
| `input.multitap` | 0 / 1 (read at start: four pads on port 1A-1D) | 0 | `TTT_MULTITAP` |
| `ui.language` | a catalog's code (`en`, `es`, `es_AR` falls back to `es`), empty or `auto`: from `LANGUAGE`, `LC_ALL`, `LC_MESSAGES`, `LANG` ([translations.md](translations.md)) | (empty) | `TTT_LANG` |
| `ui.scale` | 50-300 (percent: the overlay's text and widgets) | 100 | `TTT_UI_SCALE` |
| `ui.high_contrast` | 0 / 1 (white on black, yellow focus) | 0 | `TTT_HIGH_CONTRAST` |
| `ui.screen_reader` | 0 / 1 (say the focused item, [accessibility.md](accessibility.md)) | 0 | `TTT_SCREEN_READER` |
| `ui.announce_game` | 0 / 1 (with the screen reader: the game's menus and fights, read only) | 0 | `TTT_ANNOUNCE_GAME` |
| `keys.p1.up` ... `keys.p2.select` | a key name (`W`, `Up`, `Keypad 4`, `Left Shift`, `F5`, `Space`; any case) or `Scancode <n>` | the [Input](README.md#input) table | |
| `pad.p1.up` ... `pad.p4.r3` | a gamepad input (`south`, `east`, `west`, `north`, `back`, `start`, `left_stick`, `right_stick`, `left_shoulder`, `right_shoulder`, `dpad_up` / `_down` / `_left` / `_right`, `misc1`-`misc6`, `left_paddle1` ..., `touchpad`, `left_trigger`, `right_trigger`; any case) or `none` | [input.md](input.md) | |

The buttons of `keys.pN.<button>`: up, down, left, right, square, triangle, cross, circle, l1, r1, l2, r2, start,
select; `pad.pN.<button>` adds l3 and r3. Keys are by place (SDL / USB scancodes), whatever the keyboard layout;
gamepad inputs by position (SDL's names: south is A on an Xbox pad, cross on a PlayStation one).

**Environment variables win.** At start (`Settings_Load`, in `__wrap_main` before anything else) a file value
becomes its variable (`setenv`) only when the variable is not set; the code that reads the variables reads the file's
choice without knowing about the file. Values out of range are clamped.

Example:

```ini
# my settings
video.scale = 3
audio.volume = 60
keys.p1.square = Space
```

## Game speed

T40. `game.speed` runs the game faster or slower in real time; holding Tab runs it at `game.fast_forward` instead
(Tab is reserved like F1 and Escape: never a pad button, and pressed while the overlay is open it does nothing).

- **How.** The real-time clock (`plat_hw.c`, `rt_scale`) runs at the speed's percent of the host's monotonic clock.
  The window sets the wanted speed; the game's thread takes it at its next poll and rebases the scaled clock there,
  so the game's time stays continuous across a change (nothing is skipped or repeated). The game runs exactly the
  same frames: only how many VBlanks a wall second holds changes. The overlay's pause and the "more than a VBlank
  behind" drop work on the scaled clock.
- **Limits.** Above 1x the game goes as fast as the host renders: when a frame's work takes longer than the scaled
  VBlank the clock drops the lost time, as at 1x after a stall (4x measured about 3.2x on the attract sequence with GL in a hidden window, V-sync off).
  With V-sync on, the window's present may hold fast-forward to the display's rate.
- **Sound.** While the speed is not 1x the window plays nothing (no resampling: pitch-shifted or time-stretched game
  sound is not worth its cost here); `TTT_WAV` still gets every frame at the game's pace, so a WAV is the same at any
  speed. Back at 1x the window plays again.
- **Headless runs** never read the speed (their clock is the VBlank count), and the state hash and the gameplay
  fingerprint do not depend on it: the scaling code is out of line and only on the real-time path (the headless
  path's host stack bytes reach the hashed scratchpad region, [determinism.md](determinism.md)).
- **Netplay / synctest.** The simulation is unchanged, but peers advance by wall time: a rollback session (T45) must
  run every peer at 1x (or agree on one speed) and ignore Tab; `TTT_SYNCTEST` is headless and unaffected.
- Real-time runs without a window keep 1x (the window applies the setting).

## The quit question

The window's close button, SDL's quit event or Escape (key code or place, not its key repeat) open "Quit Tekken Tag
Tournament?" (Yes / No). Escape or the close button again, or Yes, quit (the window thread releases the overlay, the
GL context / renderer and the window, then `_exit(0)`). `TTT_NO_QUIT_CONFIRM=1` quits at once, as before T36.

## Switches

| Variable | Effect |
|---|---|
| `TTT_CONFIG=<file>` | The settings file. |
| `TTT_NO_QUIT_CONFIRM=1` | Close / Escape quit without asking. |
| `TTT_SCALE`, `TTT_FULLSCREEN`, `TTT_FPS_METER`, `TTT_VOLUME`, `TTT_MUTE`, `TTT_MUSIC_VOLUME`, `TTT_EFFECTS_VOLUME`, `TTT_SPEED`, `TTT_FAST_FORWARD`, `TTT_CHEAT_UNLOCK`, `TTT_PAUSE_IN_MENU`, `TTT_RESOLUTION`, `TTT_ASPECT`, `TTT_DEFLICKER`, `TTT_SMOOTH`, `TTT_LOW_LATENCY`, `TTT_TEXTURE_REPLACE`, `TTT_TEXTURE_DUMP`, `TTT_TEXTURES`, `TTT_MOD_MUSIC`, `TTT_MOD_TEXTURES`, `TTT_MODS`, `TTT_MOD_ORDER`, `TTT_MOD_DISABLED`, `TTT_VIBRATION`, `TTT_DEADZONE`, `TTT_STICK_DPAD`, `TTT_MULTITAP`, `TTT_LANG`, `TTT_UI_SCALE`, `TTT_HIGH_CONTRAST`, `TTT_SCREEN_READER`, `TTT_ANNOUNCE_GAME` | The settings above, over the file. |
| `TTT_OVERLAY_TEST=1` | Tests: with a window, the overlay is driven from inside (made-up key events through the window's own handling at fixed frames: F1, a key pressed while open, Escape; every page drawn; a key and a gamepad button bound through the capture paths) and the report adds `ttt: overlay: ...` and `ttt: window: overlay test: ...`. Nothing is sent to the desktop. There are nine pages since T49 (Mods) and T46 (Netplay), and after Escape the arrow keys move the focus in the "Quit?" question; the report adds the language, the font's accented letters and the screen reader's count. |
| `TTT_SELFTEST=settings` | The file's round trip (comments, sections and unknown keys kept, no temporary file left), the environment's precedence, clamping, key names both ways, rebinding (swaps, F1 / Escape / Tab refused), the gamepads' bindings (defaults, swaps within a pad, guide refused,
`none`, names both ways, the file's round trip), the `input.*` options, the T40 defaults; T47: the catalog reader (escapes, UTF-8, broken files refused), the English fallback, the placeholder checks, the language's round trip through `ui.language` / `TTT_LANG` to `port/lang/es.json` and back, `es_AR` to `es`, the locale's detection, the picker's list. Needs `TTT_CONFIG` (it writes that file). |

## Dear ImGui

`third_party/imgui`: Dear ImGui v1.92.9 (MIT, `third_party/imgui/LICENSE.txt`), from
`https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9.tar.gz` (SHA-256
`af97ed649182c39314320514a672b82008ab462b9293fe23d37b30bfa5d05519`), unmodified: the core files and the OpenGL 3
renderer backend (`backends/imgui_impl_opengl3*`). Built as C++17 with `clang++ -m64` (`g++ -m32` in the 32-bit
gcc build), `-fno-exceptions -fno-rtti -fno-threadsafe-statics`, no libstdc++ (nothing of the C++ run time is referenced), `-fno-plt` (its C library calls go
through the GOT: the program's PLT, and so every game function's address, is unchanged), SSE maths
(`port/tools/toolchain.py`).

- No SDL3 backend of ImGui: it links SDL, which the port loads at run time. The platform side is in `overlay.cpp`
  (raw SDL events by offset, as `plat_window.c` reads them).
- OpenGL through ImGui's own loader, filled from `SDL_GL_GetProcAddress` (`imgl3wInit2`); its `atexit` call (which
  would add a PLT entry) is renamed after compiling to a no-op in `overlay.cpp` (`toolchain.IMGUI_OBJCOPY`).
- The software renderer's overlay draws with `SDL_RenderGeometryRaw` (a small backend in `overlay.cpp`).
- ImGui keeps no files (`IMGUI_DISABLE_FILE_FUNCTIONS`: no `imgui.ini`); demo windows and debug tools are compiled out.

## Open

- No text input in the overlay (nothing needs it yet). Gamepad navigation is checked on made-up events only.
- The software renderer's overlay switches SDL's logical presentation off while it draws; checked with SDL's dummy
  driver only (no picture comparison).
- T37's video options are in [video.md](video.md).
- Game speed: a Tab held while the window loses focus stays held until Tab is pressed and released again; no
  resampled sound off 1x; the speed is not measured with V-sync on a real display (no visible windows in tests).
