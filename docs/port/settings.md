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
| Audio | Volume 0-100, mute (what the window plays; the game's own mix is unchanged) | at once |
| Controls | The keyboard's keys for pads 1 and 2: click a key, press the new one (Escape cancels; F1 and Escape cannot be bound; a key presses one button only, so the button that had it gets the old key). "Default keys". The gamepads on pads 1 and 2 (read only) | at once |
| Game | Movies on / off | next start |
| | Pause the game while the overlay is open (default on) | at once |
| | The disc, the data file, verified or `[modified game data]`, the settings file and a write error if any (read only) | |
| About | The build (`git describe`), `[modified game data]` when set, Dear ImGui's version | |

A setting whose environment variable is set shows `(set by TTT_...)` and cannot be changed there (the variable wins).
Mouse and keyboard work in the overlay (arrows / Tab / Return / Space navigate). Without the overlay (SDL without the
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
  (`0xd36c82e9a145875a`) are unchanged, and `tests/test_port_window.py` checks that a run with the overlay driven
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

Where: `TTT_CONFIG`, else `$XDG_CONFIG_HOME/ttt/settings.ini`, else `~/.config/ttt/settings.ini`. Nothing is written
until a setting is changed in the overlay; then the whole file is written at once: a temporary file beside it
(`settings.ini.tmp`), flushed and synced, renamed over the old one, so a crash leaves the old file or the new one.

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
| `audio.volume` | 0-100 | 100 | `TTT_VOLUME` |
| `audio.mute` | 0 / 1 | 0 | `TTT_MUTE` |
| `game.movies` | 0 / 1 | 1 | `TTT_MOVIES` |
| `game.pause` | 0 / 1 | 1 | `TTT_PAUSE_IN_MENU` |
| `keys.p1.up` ... `keys.p2.select` | a key name (`W`, `Up`, `Keypad 4`, `Left Shift`, `F5`, `Space`; any case) or `Scancode <n>` | the [Input](README.md#input) table | |

The buttons of `keys.pN.<button>`: up, down, left, right, square, triangle, cross, circle, l1, r1, l2, r2, start,
select. Keys are by place (SDL / USB scancodes), whatever the keyboard layout.

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

## The quit question

The window's close button, SDL's quit event or Escape (key code or place, not its key repeat) open "Quit Tekken Tag
Tournament?" (Yes / No). Escape or the close button again, or Yes, quit (the window thread releases the overlay, the
GL context / renderer and the window, then `_exit(0)`). `TTT_NO_QUIT_CONFIRM=1` quits at once, as before T36.

## Switches

| Variable | Effect |
|---|---|
| `TTT_CONFIG=<file>` | The settings file. |
| `TTT_NO_QUIT_CONFIRM=1` | Close / Escape quit without asking. |
| `TTT_SCALE`, `TTT_FULLSCREEN`, `TTT_FPS_METER`, `TTT_VOLUME`, `TTT_MUTE`, `TTT_PAUSE_IN_MENU`, `TTT_RESOLUTION`, `TTT_ASPECT`, `TTT_DEFLICKER`, `TTT_SMOOTH`, `TTT_LOW_LATENCY` | The settings above, over the file. |
| `TTT_OVERLAY_TEST=1` | Tests: with a window, the overlay is driven from inside (made-up key events through the window's own handling at fixed frames: F1, a key pressed while open, Escape; every page drawn; a key bound through the capture path) and the report adds `ttt: overlay: ...` and `ttt: window: overlay test: ...`. Nothing is sent to the desktop. |
| `TTT_SELFTEST=settings` | The file's round trip (comments, sections and unknown keys kept, no temporary file left), the environment's precedence, clamping, key names both ways, rebinding (swaps, F1 / Escape refused). Needs `TTT_CONFIG` (it writes that file). |

## Dear ImGui

`third_party/imgui`: Dear ImGui v1.92.9 (MIT, `third_party/imgui/LICENSE.txt`), from
`https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9.tar.gz` (SHA-256
`af97ed649182c39314320514a672b82008ab462b9293fe23d37b30bfa5d05519`), unmodified: the core files and the OpenGL 3
renderer backend (`backends/imgui_impl_opengl3*`). Built as C++17 with `g++ -m32`, `-fno-exceptions -fno-rtti
-fno-threadsafe-statics`, no libstdc++ (nothing of the C++ run time is referenced), `-fno-plt` (its C library calls go
through the GOT: the program's PLT, and so every game function's address, is unchanged), SSE maths
(`port/tools/toolchain.py`).

- No SDL3 backend of ImGui: it links SDL, which the port loads at run time. The platform side is in `overlay.cpp`
  (raw SDL events by offset, as `plat_window.c` reads them).
- OpenGL through ImGui's own loader, filled from `SDL_GL_GetProcAddress` (`imgl3wInit2`); its `atexit` call (which
  would add a PLT entry) is renamed after compiling to a no-op in `overlay.cpp` (`toolchain.IMGUI_OBJCOPY`).
- The software renderer's overlay draws with `SDL_RenderGeometryRaw` (a small backend in `overlay.cpp`).
- ImGui keeps no files (`IMGUI_DISABLE_FILE_FUNCTIONS`: no `imgui.ini`); demo windows and debug tools are compiled out.

## Open

- No gamepad navigation of the overlay and no gamepad rebinding (the gamepads keep the fixed layout of the Input
  table); no text input (nothing needs it yet).
- The software renderer's overlay switches SDL's logical presentation off while it draws; checked with SDL's dummy
  driver only (no picture comparison).
- T37's video options are in [video.md](video.md); T40 adds game speed and the cheats page.
