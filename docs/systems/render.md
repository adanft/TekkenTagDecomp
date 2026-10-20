# Rendering (`scene3d`, `model_draw`, `hud`, `effects`)

How a frame gets from the game state to the GS, and the modules decompiled for it: in T17 batch 1 (2026-10-20)
`src/scene3d.c`, `src/scene3d_b.c`, `src/scene3d_c.c`, `src/scene3d_d.c` (the `scene3d` range 0x34C508-0x3543F0 without
the skeleton cut), `src/model_draw.c` (0x3DB758-0x3DC9C8) and `src/hud.c` (0x3C1A78-0x3C3150); in T17 batch 2 the effects,
`src/effects.c` (0x3A04B8-0x3A6460) and `src/effects_draw.c` (0x3A6460-0x3A9678), section 7.

Tags: **[V]** read in matching C or in the disassembly, **[I]** inferred.

## 1. Summary

- **Nothing is drawn by the GPU's own API: the game writes GS packets by hand.** Every draw path ends in GIF packets
  (sprites, triangles, strips with A+D register writes) that reach the GS through DMA, either straight from main RAM
  (GIF channel 2, `D_008BD310` packet buffers) or built in the scratchpad and streamed through a memory FIFO (section 2).
  VU1 has one program (docs/systems/vu1); fighters are skinned on **VU0** by hand-written EE loops that call VU0
  micro-programs one vertex at a time (`dma_gif`, `func_003435C0`..) [V].
- **A frame is two halves of one main-loop iteration** (`main`, src/main.c): first the draw passes of the previous
  update (stage, fighters, effects, HUD packets already queued), then the update itself (`Mode_Dispatch`, which runs the
  fight simulation and queues the 2D HUD), then the services (`Frame_End`). The VBlank handler (`Gfx_VSyncHandler`) flips
  the buffers and kicks the display DMA chain of the current phase [V].
- **Module names were provisional and partly wrong**: `model_draw` draws no models (2D UI primitives, stage-object
  draw hooks, text helpers); `scene3d` holds the fighters' secondary motion (wind, dangling joints, accessory parts), the
  sword trail, the fighter lights and look-at, and a GS texture upload; `hud` is the fight HUD [V].

## 2. Frame pipeline

`main` (src/main.c), once per VBlank [V]:

| Step | Function | What |
|---|---|---|
| 1 | `Frame_Begin` | `Vu0_LoadMicrocode`, `Gfx_SetPhase(0)`, scratchpad stream reset `func_0035D790(D_005F5C00)`, `Gfx_SetFrameReady`, `Gfx_WaitVSync` |
| 2 | `func_00357BD0` | `Gfx_SetPhase(1)`, show the buffer of the field parity |
| 3 | `func_00357A50` | stream state 2 (`func_0035DB90`), stage pass `func_00357C88` (stage objects `func_003DB758` / `func_003DB7B0` around the stage geometry), flush |
| 4 | `func_00357A78` | fighter pass 0: for each drawn fighter `func_00357640` (its DMA packet through the scratchpad), `Fx_DrawFighterTex` (texture animation uploads, section 7), `func_00343340(f, 0)` (VU0 matrices, then the skinned parts) |
| 5 | `func_00357B28` | stream state 4, stage overlays (`func_003E7270` on stages 2, 0x13, 0x16), model pass |
| 6 | `func_00357B50` | stream state 5, `func_00399918`; effect pass `func_00346830`; fighter pass 1 `func_00343340(f, 1)`; then for each fighter the sword effect `func_00350C40` (section 3); effects `func_003469D8`, the textured quads `Fx_DrawQuads`; 2D lists `func_00399AF8`, the fade and title logo `func_003A6C08` |
| 7 | `Frame_PreUpdate`, `Mode_Dispatch` | input, RNG, one update of the mode (the fight: `Battle_UpdateFrame`, whose `func_0036EA80` queues the HUD, section 5) |
| 8 | `func_00346608`, `FlushCache(0)`, `Frame_End` | effect update; services, `sceGsSyncPath`, frame-time sample, `Gfx_SetPhase(2)`, movie |

The draw passes are switched per mode by `Mode_SetDrawFlags` (`gDrawFighterCount`, `gDrawStagePass`, `gDrawModelPass`,
`gDrawEffectPass`).

**Packet paths** [V code of src/dma_spr.c, src/gfx.c, src/dma_gif.c; I roles]:

- **Scratchpad stream (MFIFO).** Most 3D packets are built in the 16 KB scratchpad (0x70000000, `Spr_Setup` hands out a
  double buffer) and sent with the fromSPR channel (0x1000D000) into a ring buffer in main RAM that the DMAC drains into
  VIF1 or the GIF: the ELF's `.mfifo` section (0x200000, 0x140000 bytes) holds the rings, `D_CTRL` MFD selects VIF1
  (`func_0035DA78`, ring 0x300000, 0x40000 bytes) or GIF (`func_0035DC58`, ring 0x200000, 0x80000 / 0x100000 bytes),
  `D_RBOR` / `D_RBSR` set the ring. The states 1-6 of `gSprDma` follow the passes above; the DMAC interrupt handler
  keeps the drain going (`func_0035E1C8`..).
- **Template headers.** A packet starts with a 2-quadword header (DMA tag + GIF tag) kept in `.data`, copied into the
  scratchpad with a toSPR transfer (`func_00343128`, channel 0x1000D400), filled with vertex quadwords by the EE, and sent
  on by `func_00343178` (the sword effect's six senders, src/scene3d_b.c).
- **Direct GIF buffers.** 2D drawing (menus, HUD, windows) writes GIF data straight into two per-frame buffers of
  `D_008BD310` (src/gfx.c): buffer A (0x8000 bytes per half, `func_003994C0` returns the write pointer,
  `func_00399580(end)` closes it) and buffer B (0x30000 per half, `func_00399520` / `func_00399650`); both print "(zs)..."
  overflow messages and `exit` when full. Buffer A also keeps a list of referenced packets (`D_008BD390`). These are
  packet lists, not a z-sort [V].
- **Texture upload.** `func_003542E8` (src/scene3d_d.c) builds a libpkt GIF packet that references a chain of GS image
  records (0x60-byte header with BITBLTBUF/TRXPOS/TRXREG/TRXDIR, then the image), with each record's destination base
  pointer moved to the requested VRAM block (`func_00354120`) and a TEXFLUSH at the end (`func_00354290`); it waits for
  the GIF channel, flushes the cache and starts the chain (`func_0035E6B8`). `func_00354368` returns the chain's size in
  256-byte VRAM blocks.
- **Display.** `Gfx_VSyncHandler` shows the finished buffer (DISPFB / DISPLAY) and, by phase, starts the display DMA
  chain on GIF (phase 0, 3) or VIF1 (phase 1).

Screen space for 2D: x, y are pixels of a 640 x 448 frame from the top-left; the primitives add (0x6C0, 0x720) =
(2048 - 320, 2048 - 224) in 12.4 fixed point and draw in GS context 2 [V].

## 3. The sword effect (`src/scene3d_b.c`, `src/scene3d_c.c`)

Fighters with the sword (character 4, or 15 / 33 with costume 4) get a trail and a glow, drawn after their model by
`func_00350C40` (from step 6) [V code; I look]:

- **History.** Per side, three blade points (`D_00454470`, or `D_004544A0` for weapon kinds 0x41-0x45 and 0x78) go through
  the joint matrix at `Fighter + 0x3CF0` into three "history" matrices `D_01FC2420[side]` (rows = last 4 positions).
  Each frame outside the pause the ring `D_01FC25A0[side]` (75 entries x 3 points) shifts by 5 and gets 5 new entries
  from a cubic Lagrange spline over the history (`func_003518B8` weights, `func_00351978`). `func_00350578` restarts it
  (also from move events and the floor re-centring), `func_00350728` decides each frame from the move id whether it is
  on (`D_005F6FD8`).
- **Trail.** The 75 x 3 points are projected with `sceVu0RotTransPersN`; 73 x 2 textured quads use their own screen
  position as texture coordinates on a copy of the frame buffer (TEX0 PSMCT24, 1024 x 256), with alpha fading 75 .. 3:
  a heat-haze look. Vertices off screen set the ADC bit so the GS skips that triangle.
- **Glow.** Three fans (12, 8, 8 triangles) of fixed shapes, rotated along the blade's screen direction and stretched by
  its screen length, at a constant depth.
- Character 20 has its own two-part swing (`func_003519E0`, `func_00351A60`, `func_00351AA0`) applied as Z rotations to
  two model parts.

## 4. Fighter state for drawing (`src/scene3d.c`, `src/scene3d_d.c`)

- **Wind** (`func_0034E018`, first in `Skel_Update` outside mode 5): per side a float vector `D_01FAB740` from
  `func_003D3D08(position)`, its length `D_005F6E20`, a fixed-point copy `D_01FAB6D0` and strength `D_005F6E18` [V].
- **Extra joints 18+** (`func_0034C508`, from `Skel_ComputeWorld`): a fixed-point pendulum: the tip is averaged over a
  few frames, blended with the joint's down axis and the wind, and turned into yaw / pitch with the GTE-style helpers;
  frozen while paused [V].
- **Accessory parts** (`func_0034D258`, from `func_003478F0`): per slot, special handlers (character 0x21's floating
  body, joint-bound parts, parts toggled by state) or a float spring (lag, gravity, wind, push-out of the fighter's
  collision shapes, angle limits, stretch, floor clamp) built in a scratchpad work area; `func_0034E148` sets the part
  records up [V draft].
- **Look-at** (`func_00351E98`, from `func_003478F0` before the draw): neck, head and eyes turned toward a target within
  the move's tracking window, yaw clamped to +-40 degrees, pitch -32.5 .. 15, eased in; the joints' view and screen
  matrices are rebuilt [V draft].
- **Lights** (`D_01FC4210`, 0x340 bytes in the libvu0 light layout): a global set from the stage
  (`Light_SetupStage`), two fighter slots (current, stage base, flash colour) and an extra set. `Light_UpdateFighter`
  (from `Skel_Update`) takes lights 0-1 from the base set, aims light 2 from the fighter's matrix (faded by the distance
  to the stage light and the angle to light 0, coloured per character) and blends in a flash started by effects
  (`Light_StartFlash`: position, colour, 54 frames). `Light_GetFighterMatrices` builds the VU0 normal-light and
  light-colour matrices that `func_00343340` uploads before skinning [V].

## 5. The fight HUD (`src/hud.c`)

`Hud_Draw` runs once per update from `func_0036EA80` (battle_flow) while `D_005F5C74` is set; all elements are sprites of
the sprite2d module (`func_003CE258` native size, `func_003CE318` stretched, `func_003CE2B8` / `func_003CE398` the same
alpha-blended; colour `D_005F6738`, Z `D_005F6730 >> 4`) [V]:

- **Order**: Z for the frame, round markers (`Hud_DrawMarkers`), for each side two life bars (`Hud_DrawLifeBar(side,
  member, fade, partner)`: the active member first, the partner second), then the timer.
- **Life bars**: 240 x 28 px plus 9 px caps; side 0 from x 29 to 269, side 1 from 370 to 610, both filled from the inner
  end, so they drain toward the centre. Length = `240 * (value / 100) / (max / 100)` on the 20.12 values of the member
  record (`TeamMember.unkC` life, `unk10` red life, `unk14` maximum). Drawn in order: background, red life, life, and a
  white damage trail that falls by max / 400 per frame; rising life is drawn climbing by max / 800 per frame.
- **Partner bar and tag**: the active bar sits at y 15, the partner's 15 px lower and further out; a tag (counter
  `func_003D3228(side, 0)` between 1 and 15) slides them past each other over 30 frames. A side with one member is
  drawn 8 px lower.
- **Partner counter** (`TeamMember.unk28`, 100 at rest, pair rules of `func_003CF278` on a tag): at 120 or more the bar
  flashes and the active fighter's `unk400` is set (`func_003C1B38`) [V code; I "rage"].
- **Name plate**: under the partner bar, 124 x 34, with the character's name sprite.
- **Round markers**: 22 px apart from x 250 leftwards (side 0) and 364 rightwards (side 1), y 60; the newest win blinks
  for 180 frames. Game type 2 shows team-member squares instead.
- **Timer**: `(gRoundTimer * 6 / 7 - 1) / 60`, so one displayed second is 70 frames (the round lengths 0x870 .. 0x1B30
  show 30 / 40 / 60 / 80 / 99); two 40 x 47 digits at x 281 / 320, y 6, or the infinity sprite when the time option is 5.
- **K.O. text**: `Hud_DrawKoPercent` prints both sides' life percentages (gauge `Fighter.unk408`) at y 100, pushed 10
  apart when equal, the leader in colour 11.

`Hud_InitRound` (round start) resets the trails and refill values, `Hud_Init` (from `Mode_Battle`) the HUD state.

## 6. 2D primitives (`src/model_draw.c`)

Written straight into the GIF buffers of section 2, GS context 2 [V]: a scissor (`func_003DB9E8` / `func_003DBAA8`, buffer A
/ B), flat rectangles with alpha test or blending (`func_003DBB68`, `func_003DBCF0`), a four-colour Gouraud quad
(`func_003DBE98`), a 9-slice window frame of 16 x 16 tiles from the skin `D_00443780` (`func_003DC188`) and the window
(`Ui_DrawWindow`: frame plus a gradient 8 px larger). Also the stage-object draw hooks (`func_003DB758` /
`func_003DB7B0` call `D_005F6FD4` on the lists `D_01FBF680` / `D_01FC0D00`), text helpers (`Str_Next`, `Str_SkipN`,
column counts: codes 1-5 and 0xFE one column, others two) and two per-slot bank helpers for the demo module.

## 7. Effects (`src/effects.c`, `src/effects_draw.c`)

T17 batch 2 (2026-10-20). Both files read and write **effect objects** (include/effect.h): the object pool of `dma_gif`
(`func_003464F8`, `func_00345780(update)`, `func_00346030(side, update)` for the two per-side rings) [V].

- **An effect** is a 0x20-byte head (list links, the **update callback** at +8, flags at +0x14: 0x10000 = free at the
  end of the frame (`func_003465C8`), 4 = drawn; a state halfword at +0x18 and a counter at +0x1C) and a drawing state
  from +0x20 that `func_003456B0` sets to defaults: position, the current frame record, frame count and frame, the bank
  sprite and its frame, scale (+0x30), colour (+0x50), size (+0x54), alpha (+0x58), angles (+0x70) and velocity (+0x80),
  then data of the kind (trail positions, timers) [V].
- **Every frame** `func_00346608` (step 8 of section 2, after the update of the mode) first runs `func_003A4370` (the
  body-part bits below), frees flagged objects, then calls each live object's callback once (while `D_005F5F10` is
  set); the effect pass `func_00346830` (step 6) draws the flagged ones from their bank sprites [V calls; I draw details].
  Almost every callback advances only outside the pause (`D_005F5EE0 || D_005F5EE4 || func_003A9698()`), but several
  draw random numbers before that test (below).
- **Animation banks**: `Fx_GetBankEntry(bank, i)` returns entry `min(i, n - 1)` of an offset table (`bank[0]` = n); an
  effect animation is a 0x38-byte header (frame count, sprite and bank frame ids at 0x34 / 0x36) followed by 0x14-byte
  frame records (offset scaled by the effect's scale, size, alpha). `func_003A04B8` (and its turning / drifting variants
  `func_003A05F8`, `func_003A06F0`, `func_003A08D8`, the flash `func_003A23E0`) plays one and frees itself at the end
  [V].

**Kinds** [V code; I where noted]:

| Kind | Functions | What |
|---|---|---|
| Stage ambient | `Fx_UpdateStageAmbient` (spawned by `func_00344BE0`), `func_003A0A90`, `func_003A0CE8`, `func_003A1068`, `func_003A1378` / `func_003A1298`, `func_003A1510`, `func_003A1B48` / `func_003A16C0` / `func_003A18F8` | on its first unpaused frame, per stage: sprite emitters at fixed places (stages 4/15, 5, 8/17/20, 11), pulsing stage lights (9/18, 11), fireflies circling the stage (14) |
| Lens flare | `func_003A1C78`, `func_003A1BC8` | stages 1, 7, 10-13, 16: the projected sun and nine elements along the line through the screen centre, faded by the distance to the centre and by fighter joints in front of the sun [I look] |
| Hit sparks | `func_003A2758` (kind 2 of the dispatcher `func_0035BF10`), `func_003A2738` | sprites of banks 0-3 at the hit; its own kind 3 (the dispatcher's second argument) draws stage-coloured sparks (count and colour per stage, `D_00437AC0`) |
| Hit flash | `func_003A2BC0` (kinds 0-1), `func_003A2E20` | one effect per animation of the side's bank in the side ring and a fighter light flash (`Light_StartFlash`), kind and colour from `func_00347730(func_0035C970(fighter))` |
| Hit bursts | `func_003A6110` (dispatcher kinds with bit 0x80), `func_003A35C8`, `func_003A3928`, `func_003A54D8` / `func_003A50B8` / `func_003A4C28` | 10 or 18 particles on orbits or drifting from the hit along its direction; kind 10 for character 0x14: sparks on ten joints of the opponent and a ground glow |
| Hit log | `HitLog_Add`, `HitLog_NextFrame`, `HitLog_ReplayFrame`, `HitLog_Save` / `HitLog_Load` / `HitLog_Rewind`, `HitLog_Reset` / `HitLog_Enable` / `HitLog_Disable` | the hit events sent through `func_0035BF70` are dispatched (`func_0035BF10`) and recorded (up to 60 per frame, 120 frames, 8 save slots); `Battle_UpdateFrame` replays a recorded frame through the same dispatcher [V]; the replay after a K.O. [I] |
| Body-part glows | `func_003A42B8` (battle init), `func_003A4370` (every frame), `func_003A3C08`, `func_003A4128`, `func_003A3FC0` | eight glows per fight on joints of both fighters, shown while the current move lists that body part (`FighterMove` bytes 0x28-0x2B through `D_005197D0`); pulsing, with sparks and light flashes |
| Move effects | `func_003A6390` (per fighter from `Battle_UpdateFrame`), `func_003A4EE8` / `func_003A44F8` / `func_003A4970`, `func_003A5810` / `func_003A55E0` | character 0x14's move kind 0x18 (sparks with embers and ground glows) and any character's move kind 0x19, inside the move's frame window |
| Character 0x21 aura | `func_003A5FA0` (battle setup), `func_003A5910`, `func_003A5B78`, `func_003A5D98` | four sprites per side on attach points, hidden while that side's fighter is not character 0x21 |
| Character 0x1D volume | `func_003A8C50` (start, from the fight's hit-test code), `func_003A8D08` (draw) | the special attack volume as up to 36 textured segments between its two points, through the quad queue (`Fx_AddQuad`, drawn by `Fx_DrawQuads` in step 6) |
| Screen fade, title logo | `Fade_Draw`, `Fade_Update`, `func_003A6770`-`func_003A68D0`, `func_003A6978`-`func_003A6C08` | a full-screen colour (alpha blend, additive or subtractive) faded over n frames; the title logo's frame list (`D_00519918`) |
| Stage texture layers | `Fx_LoadStageLayers` (battle load), `Fx_StartStageLayers` (battle init), `Fx_DrawStageLayers`, `func_003A73F8`-`func_003A7558`, `func_003A7590`, `func_003A76C0`, `func_003A7EB0`, `func_003A7FA8` | up to 8 animated VRAM uploads per stage (GS image packets per frame, played once, looped or held); stage 12 starts its layers at random, stage 4 has background actors that play random frame lists, sounds and light toggles near the screen centre |
| Fighter textures | `Fx_LoadFighterTex` (character load, tag swap), `Fx_DrawFighterTex` (per frame), `func_003A8620`-`func_003A88A0` | two texture animation channels per fighter slot (GS image uploads per frame; play once, loop or hold) |

Helpers: `Fx_RandFloat` (`(rand() & 0xFFFFFF) / 2^24`), `func_003A3438` (direction to yaw and pitch), `func_003A34C8` /
`func_003A3518` (velocity from the angles), `func_003A3588` (is the side's move of a kind and inside its window),
`func_003A9528` / `func_003A9550` / `func_003A95D8` (angles), `func_003A9510` / `func_003A9520` (the switch
`D_005F6254` that turns on the skeleton's float path, [systems/animation.md](animation.md)).

### 7.1 Random numbers in a fight (determinism)

Effects use only newlib `rand()`, the generator the CPU opponent and the camera use; none uses the CPU's
`func_003800D8` or `gRngSpin` [V: every function of both files]. During a fight `rand()` is drawn by [V]:

- **Inside the simulation step** (`Battle_UpdateFrame`): `func_003A8C50`, 18 calls each time character 0x1D's special
  attack volume starts (`func_00378150` / `func_00378398`, src/fight_misc.c); `func_003A2758`, one call per spark for
  stage-coloured sparks (dispatcher kind 2 with argument 3, also when the hit log replays a frame).
- **In the effect update** (step 8, every frame, outside `Battle_UpdateFrame`):
  - stage ambient effects: emitters of stages 4/15, 5, 8/17/20 and 11 (one call per sprite), lights of stages 9/18/11
    (at the start), drifting sprites (`Fx_RandFloat` every frame), the 50 fireflies of stage 14 (every frame);
  - stage 12's three layer effects: `rand() % 1000` every frame, not paused-gated; stage 4's actors: several calls per
    frame, partly before the pause test;
  - the hit bursts (first frame of each particle), character 0x14's sparks, embers and ground glows (first frame and
    every unpaused frame), move kind 0x19's effect (first frame), character 0x21's aura (4 calls at battle start);
  - the body-part glows: `rand() & 0xF` every frame while a part is lit, **before the pause test** (so also while
    paused), plus calls when not paused.

So the `rand()` stream seen by the CPU opponent and the camera depends on the stage, the characters, the moves, the hits
and the pause. A port that wants the original's behaviour must run `func_00346608` and every callback each frame exactly
as the original, drawing or not; giving the effects their own generator would make the fight deterministic without them
but change the CPU's choices relative to the original (decision for T26) [I consequence].

## 8. What the PC port replaces

- **The GS packet consumer, not the producers.** Packet building stays C; the port needs a GS core that takes the same
  GIF data (PACKED / REGLIST / IMAGE, A+D) from the direct buffers and from the MFIFO rings, and VRAM uploads with
  BITBLTBUF (T28). The ADC bit, context 2, the 12.4 offsets and alpha / test presets must be honoured.
- **DMA / scratchpad / MFIFO**: the scratchpad as memory plus synchronous emulations of toSPR / fromSPR and of the
  ring drain into VIF1 / GIF; the waits (`Dma_Wait`, BC0F) become no-ops once transfers are synchronous (T22, T28).
- **VU0 and VU1**: the VU0 skinning loops (`vcallms`, macro mode) and the VU1 program need an interpreter or native
  replacements (T29, T31); the libvu0 calls (`sceVu0*`) used everywhere in `scene3d` are plain float math.
- **Display**: the VBlank handler's buffer flip and display chain map to presenting the 640 x 448 frame.
- Float rendering code (springs, look-at, lights, the sword effect) affects only the picture; `func_0034E018` /
  `func_0034C508` run inside `Skel_Update` but write only drawing state [I: no simulation reader found].
- **Effects are not picture-only**: their callbacks draw from the `rand()` stream the CPU opponent and the camera use
  (section 7.1), so the effect update must keep running, in the same order, even where nothing is drawn.
- The stage and fighter texture layers (section 7) are GS image uploads per frame: the port's GS core must take them
  like any other VRAM upload.

## 9. Status

| File | Range | C / functions | Drafts (`#if 0`, compile when enabled) |
|---|---|---:|---|
| `src/hud.c` | 0x3C1A78-0x3C3150 | 10 / 12 | `Hud_DrawMarkers` (1 line: one store takes the register of an equal value), `Hud_DrawLifeBar` (registers and stack slots) |
| `src/model_draw.c` | 0x3DB758-0x3DC9C8 | 18 / 18 | - |
| `src/scene3d.c` | 0x34C508-0x34E748 | 8 / 15 | `func_0034C508`, `func_0034CC38`, `func_0034D108`, `func_0034D258`, `func_0034E018`, `func_0034E148`, `func_0034E640` |
| `src/scene3d_b.c` | 0x34FF10-0x3504C0 | 6 / 6 | - |
| `src/scene3d_c.c` | 0x3504C0-0x351E98 | 9 / 15 | `func_00350728`, `func_00350C40`, `func_003517F0`, `func_00351860`, `func_003518B8`, `func_00351AA0` |
| `src/scene3d_d.c` | 0x351E98-0x3543F0 | 33 / 37 | `func_00351E98`, `func_00352F50`, `func_00353448`, `func_00354120` |
| `src/effects.c` | 0x3A04B8-0x3A6460 | 49 / 58 | `func_003A0CE8` (50 lines, table loads hoisted), `func_003A1378` (hazard nop), `func_003A1C78` (75, registers), `func_003A3438` (hazard nop), `func_003A35C8` (needs no `-ffast-math`), `func_003A4128` (hazard nop), `func_003A44F8` (13, registers), `func_003A4C28` (hazard nop), `func_003A6110` (hazard nop) |
| `src/effects_draw.c` | 0x3A6460-0x3A9678 | 44 / 50 | `Fade_Draw` (4, two constants swapped), `func_003A76C0` (19, spill slots and order), `func_003A88B0` (24, a folded count), `Fx_AddQuad` (1, `addu` operands), `func_003A8D08` (needs no `-ffast-math`), `func_003A95D8` (hazard nops) |

The reasons are in each draft's `NON_MATCHING` note and in [decomp_guide.md](../decomp_guide.md), "Lessons from T17
batch 1" and "Lessons from T17 batch 2". "Hazard nop": a `nop` that gas adds and the original lacks (before an unfilled
`bc1f` / `bc1t` after an FPU compare, or between `li.s` and a prelude `mtc1`), a change of `include/gcc_prelude.inc`.

## 10. Open

- The sprite2d module (0x3CD298-0x3D0660), `zsort`, `draw_list`, `vu1_packet` and the fighter draw in `dma_gif` /
  `render_fighter_a` are not decompiled yet (T17 later batches); nor the effect pool itself (`func_003464F8` ..
  `func_00346830` in the `geometry` range) and the dispatcher `func_0035BF10`.
- How the original wrote the 64-bit to 128-bit move of the sword effect's TEX0 (`por`), which this compiler refuses.
- Whether `scene3d.c`'s float functions were built without `-ffast-math` (section in the guide). The effects are: a
  per-file flag for `effects.c` / `effects_draw.c` is the next test (needs `configure.py`), and `func_003A8D08` then also
  needs the original's libcall names (`sqrt`, `__fixunssfsi` where this compiler calls `sqrtf`, `fptoui`).
