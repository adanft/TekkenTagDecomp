# PC port: video options

T37. Options of the OpenGL back end (`port/src/gs_gl.c`): internal resolution, widescreen, the game's flicker filter,
smooth scaling and low input lag. The software renderer stays the 1x 4:3 reference and ignores them. All of them are
presentation only: nothing the game reads changes (the state hash is the same with every combination).

Code: `port/src/gs_gl.c` (everything), `port/src/plat_settings.c` (the keys), `port/src/overlay.cpp` (the Video page),
`port/src/plat_window.c` (the window's width for the aspect). Tests: `tests/test_port_gl.py` (class `Video`),
`TTT_SELFTEST=gl` and `TTT_SELFTEST=settings`.

| Key | Values | Default | Variable | What |
|---|---|---|---|---|
| `video.resolution` | 1-8 | 1 | `TTT_RESOLUTION` | Each GS pixel of a target drawn as N x N pixels. |
| `video.aspect` | `4:3` / `16:9` / `21:9` | `4:3` | `TTT_ASPECT` | The 3D scene widened to that shape; the HUD and menus stay 4:3 in the middle. |
| `video.deflicker` | 0 / 1 | 1 | `TTT_DEFLICKER` | 0: the game's frame is shown at its full 448 lines, without the game's vertical blur. |
| `video.smooth` | 0 / 1 | 1 | `TTT_SMOOTH` | The picture scaled into the window bilinear (1) or nearest (0, sharp pixels). |
| `video.low_latency` | 0 / 1 | 0 | `TTT_LOW_LATENCY` | A frame is presented as soon as the game has finished it, and the driver queues no frame. |

All of them apply at once (at the next picture). A new resolution or aspect writes every target back into GS memory
and makes it again at the new size, so the picture just after the change is drawn from 1x content once.

## What the game does

Found with a trace of the primitives of one fight VBlank (scripted fight, VBlank 1250):

- **The frame and the display buffer.** The game draws its frame at 640 x 448 (24 bits, block 0, with depth), with the
  HUD drawn last into the same buffer. At the start of the next frame it copies that frame into a second buffer of
  640 x 224 (block 0x1180) with UV sprites in strips of 64 x 8 pixels, two passes per strip: one bilinear sprite that
  takes two lines into one, then the same half a line lower at half alpha. The display (`SMODE2` frame mode, both read
  circuits on 0x1180, `PMODE` blend 0x7F) shows those 224 lines doubled. That is the game's flicker filter: the
  picture has half its vertical resolution and a vertical blur.
- **The copy's sprites** run from x 0 to x 63.5 (12.4: 0 .. 1016) with U 0.5 .. 64: the GS covers pixels 0..63, but a
  finer scale has a gap of half a pixel between strips.
- **Render to texture.** Shadows: a 256 x 256 16-bit target (block 0x3940) is cleared, the fighter's silhouette drawn
  into it, and it is projected on the floor (perspective, bilinear). The frame buffer's top byte holds 4 / 8-bit fonts
  (PSMT8H / 4HL / 4HH textures), and a CLUT load reads the frame once per VBlank (T30 batch 4).
- **Projection.** `Cam_SetDistance` (`src/stage.c`) builds the two view-screen matrices (`sceVu0ViewScreenMatrix`,
  centre 2048, 2048) and scales x and y by 1.25; they are in EE RAM and are hashed. The VU1 programs clip against the
  GS's coordinate range (a guard band of +-2048 around the centre, `docs/systems/vu1/README.md`), not the 640 x 448
  screen: triangles beyond the screen's sides reach the GS (seen down to x = -172 in the trace). The EE culls stage
  floor cells against the 640 x 448 screen (`func_003408B0`) and objects with `sceVu0ClipAll`.
- **Effects.** No full-screen blur or glow pass in the fight besides the flicker filter's copy; the stage lighting is
  blending (Cd * As), not a texture read of the frame. The flicker filter is the only effect worth a switch.

## Internal resolution

A target's texture is (width + 2 margins) x rows GS pixels, N GL pixels each (it was 1024 rows high at 1x before;
now as many rows as the target has: a target that grows is written back and made again).

- **Rasterising.** The GS's pixel (x, y) is the first of its N x N pixels: GL pixel i samples the GS position i / N
  (the vertex shader adds 0.5 / N, not 0.5). So the first sample of each GS pixel is where the GS samples it, the
  others lie inside the pixel, and a UV sprite never samples the texel before its first one: no seams at the edges of
  sprites and glyphs. The scissor is tested on the GS pixel (`gl_FragCoord / N`).
- **Sprite edges.** At N > 1 a sprite's edges move up to whole pixels (`align_axis`), UV moving at the same rate: it
  covers the GS pixels it covers at 1x, no part of a pixel more or less. The game's copy strips (x .. x + 63.5) then
  meet without the half-pixel gap.
- **GS memory semantics.** GS memory (the 4 MB mirror) stays 1x and authoritative. A resolve (a target written back,
  T30) reads each GS pixel's first sample, the value the GS would have drawn; the loads of new targets read the mirror
  (each GS pixel N x N times). Everything read back to the CPU, every CLUT load, every reinterpretation (an 8-bit or
  4-bit texture over a frame buffer, the fonts in the top byte, a 16-bit texture over a 32-bit buffer) reads GS memory
  as at 1x.
- **Snapshots: the game sampling its own frames.** A texture that is a colour target in a format that reads the same
  values (32 or 24 bits over a 32-bit target, 16 over 16; same block and width) is sampled from that target's
  snapshot at full resolution instead of from GS memory: the flicker filter's copy, the shadow target. The snapshot is
  the target copied (rows drawn since the last copy) when a texture reads it, before the draws that read it run, so
  it holds what GS memory would hold at that point. Sampling follows the GS's rules at the finer step: the texel
  `(u * N) >> 4` (nearest) or the four around it (bilinear, 4-bit fractions), wrap modes on the GS texel, and the GS's
  storage (16 bits: 5 bits per channel and the alpha bit) and TEXA.
- **The display** of a scaled target reads the target (or a snapshot, below) at full resolution; other buffers read GS
  memory.
- **Not scaled.** Lines and points stay one GL pixel wide. At 1x 4:3 with the flicker filter none of this is used: the
  pictures are the T30 ones byte for byte.

## Widescreen

`16:9` and `21:9` add 78 or 203 GS pixels left and right of the 640 x 448 picture (796 x 448 and 1046 x 448, the
pixels as square as at 4:3: the window has always shown 640 x 448 with square pixels), and the window becomes as much
wider.

- **No change to the projection.** Since the VU1 programs clip at the guard band, the scene's triangles beyond the
  screen are in the GS's commands already: 640-wide targets (the frame, its depth buffer, the display buffer) get
  margins, and the scissor of the full-width draws opens into them. The 3D scene shows more at the sides with the same
  projection; the game's camera, culling and EE RAM are untouched (the state hash is the same: `tests/test_port_gl.py`
  checks 4:3 1x, 16:9 2x and 21:9 without the filter over the scripted fight with `TTT_SLOW_TESTS=1`).
- **Sprites** (the HUD, menu pictures, text) keep their 4:3 places in the middle. A sprite that covers the first or
  last pixel of a row goes on into the margin when it is untextured (the clears of colour and depth, fades) or when it
  is a copy of a widescreen target's snapshot (the flicker filter's copy: U goes on at the same rate, into the source's
  margin).
- **Pillarbox.** Margins drawn in this picture or the last one are shown; otherwise (menus, the character select, the
  stage intro) they are black.
- **Edge artifacts.** The EE culls stage floor cells and objects against the 4:3 view: at 21:9 the floor's far corners
  are missing in some camera angles (black wedges at the bottom corners of the stage intro); at 16:9 they are rarely
  visible. 2D triangles drawn up to the screen's sides (the screen transitions' iris) stop at the 4:3 edges. Extending
  the culling would change EE RAM (the packets are built there) and the hash, so it is not done.

## The flicker filter (`video.deflicker 0`)

When the display buffer was last drawn by a copy that takes two lines into one from a target's snapshot (the game's
filter above), and the display reads it with lines doubled, the picture shows that snapshot instead, one line per
line: the frame as drawn, 448 lines, without the blur. It is the same frame at the same VBlank (the snapshot the copy
read). Other screens show as before.

## Smooth scaling

The picture is scaled into the window with `GL_LINEAR` (default) or `GL_NEAREST` (sharp pixels at whole window
scales).

## Low input lag

- **A double present fixed (all modes).** Since T36 each VBlank's picture was presented twice (the picture's blit
  presented, then `show` presented again): with vsync two refresh waits per game frame, so the window thread could show
  only 30 pictures a second, fell behind and skipped. The scripted fight's headless run reported 1974 presents for 1460
  pictures; now one each.
- **Early present.** With `video.low_latency 1` a frame goes to the screen when the game has finished it: when the
  game's copy into the display buffer is complete (its last cell: two passes over the buffer's last pixel), or at its
  first sprite without the flicker filter (the frame is the snapshot then). The display registers are the last
  VBlank's. The next VBlank's picture is then not presented again (it is that picture). Without the copy (other
  screens) pictures are presented at the VBlanks as before.
- **No queued frames.** After each swap `glFinish` waits for the GPU (the driver cannot queue frames ahead).
- **Measured** (the report's `ttt: gl: video ...` line: from the queueing of the VBlank after which the shown frame was
  complete, to the present returning; scripted fight from window picture 600 on, real time `TTT_HEADLESS=0`, hidden
  window with SDL's offscreen driver, so the swap does not wait for a display):

  | | frames presented at the VBlank | frames presented early |
  |---|---|---|
  | standard | mean 17.6 ms, max 23.6 ms | - |
  | low latency, flicker filter on | 18.6 ms (VBlanks the game did not finish a frame for) | mean 11.4 ms, max 19.0 ms |
  | low latency, flicker filter off | 18.4 ms | mean 9.0 ms, max 18.2 ms |

  The game's own part, from the pad latched at a VBlank to the first VBlank picture that differs (a punch at VBlank
  1300 against the same fight without it, GL dumps every VBlank): 4 VBlanks, the same with every option. Low latency
  presents that frame about half a VBlank sooner in this setup (the game issues its copy some milliseconds after the
  VBlank). With vsync on a real display the early present should catch the refresh before the next VBlank's; that, and
  the frames `glFinish` keeps the driver from queueing, could not be measured here (no display).

## Speed

Scripted fight, VBlanks 1101-1451 (`TTT_TIME_FROM=1101`, hidden window, `TTT_VSYNC=0`, headless clock; RTX 4060;
varies by 10-20 % with the thermal governor):

| | VBlanks/s |
|---|---|
| T36 (before) | 65 |
| 1x 4:3 | 84 (the double present gone) |
| 2x | 68 |
| 4x | 60 |
| 16:9 1x | 70 |
| flicker filter off 1x | 66 |
| 8x | 29 |
| 8x 21:9, filter off | 22 |

## Open

- An exit-time crash was seen once in four 8x runs (after the report, as the window thread released the context);
  not reproduced at 1x-4x.
- In real time with a hidden offscreen window the scripted fight makes about 400 game frames in 1451 VBlanks (the
  T36 build too, with either renderer): the game misses VBlanks there; the latency figures above are from that setup.
- Lines and points are not widened at higher resolutions; textures that are part of a target but at another block
  (a sub-rectangle) read GS memory at 1x.
