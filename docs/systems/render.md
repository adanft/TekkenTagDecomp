# Rendering (`scene3d`, `model_draw`, `hud`, `effects`, `model_anim`, `dma_gif`, `sprite2d`)

How a frame gets from the game state to the GS, and the modules decompiled for it: in T17 batch 1 (2026-10-20)
`src/scene3d.c`, `src/scene3d_b.c`, `src/scene3d_c.c`, `src/scene3d_d.c` (the `scene3d` range 0x34C508-0x3543F0 without
the skeleton cut), `src/model_draw.c` (0x3DB758-0x3DC9C8) and `src/hud.c` (0x3C1A78-0x3C3150); in T17 batch 2 the effects,
`src/effects.c` (0x3A04B8-0x3A6460) and `src/effects_draw.c` (0x3A6460-0x3A9678), section 7; in T17 batch 3 the
`model_anim` module, `src/model_anim.c` (0x3D0660-0x3D3630) and `src/model_anim_b.c` (0x3D3630-0x3DB758), section 11; in
T17 batch 5 the stage renderer and the VU1 model path, `src/render_fighter_a.c` (0x340000-0x341B28),
`src/render_fighter_b.c` (0x341B28-0x3420C8), `src/vu1_packet.c` (0x3420C8-0x342E78) and `src/draw_list.c`
(0x356A70-0x3572A0), section 12, and the memory card screens with the GS image upload, `src/font_tex.c`
(0x3608B8-0x3621F8), section 13; in T17 batch 6 the fighter draw path of `src/dma_gif.c` (section 12.1), the 2D sprite
and text primitives `src/sprite2d.c` (0x3CD298-0x3D0660, section 14) and the functions left over in batch 5.

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
  `Wind_Sample(position)`, its length `D_005F6E20`, a fixed-point copy `D_01FAB6D0` and strength `D_005F6E18` [V].
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
the sprite2d module (section 14: `Sprite_Draw` native size, `Sprite_DrawScaled` stretched, `Sprite_DrawBlended` / `Sprite_DrawScaledBlended` the same
alpha-blended; colour `D_005F6738`, Z `D_005F6730 >> 4`) [V]:

- **Order**: Z for the frame, round markers (`Hud_DrawMarkers`), for each side two life bars (`Hud_DrawLifeBar(side,
  member, fade, partner)`: the active member first, the partner second), then the timer.
- **Life bars**: 240 x 28 px plus 9 px caps; side 0 from x 29 to 269, side 1 from 370 to 610, both filled from the inner
  end, so they drain toward the centre. Length = `240 * (value / 100) / (max / 100)` on the 20.12 values of the member
  record (`TeamMember.unkC` life, `unk10` red life, `unk14` maximum). Drawn in order: background, red life, life, and a
  white damage trail that falls by max / 400 per frame; rising life is drawn climbing by max / 800 per frame.
- **Partner bar and tag**: the active bar sits at y 15, the partner's 15 px lower and further out; a tag (counter
  `Tag_StepCounters(side, 0)` between 1 and 15) slides them past each other over 30 frames. A side with one member is
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
  BITBLTBUF (T28, done: `port/src/gs_core.c`, docs/port/README.md "GS path"). The ADC bit, context 2, the 12.4 offsets
  and alpha / test presets must be honoured.
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
- **Models (section 11)**: the stage scenes and the round camera are float / libvu0 code on the EE and stay C; what the
  port replaces is underneath: the renderer the draw lists end in (`Model_Draw` / `Model_Render`, section 12; the VU0
  skinning of `dma_gif` for the fighters), the
  stage objects' VIF1 packets (VU1 microcode, a template in `.vudata` 0x4FAF80-0x4FB800 plus DMA references into the
  model banks), `sceVu0ClipAll` and the inline VU0 macro code of `func_003DA138` / `func_003DAA80` (plain float math
  in a port), and the scratchpad work areas (`Spr_Setup`, toSPR copies, `Dma_Wait`).
- **The stage wind and the stage animations use `rand()` and the CPU generator** (`func_003800D8` in the round camera,
  `rand()` through `func_003D8128` and `Fx_RandFloat` in the stage animations): as for the effects (section 7.1) they
  must run every frame in the original order, drawn or not, to keep the `rand()` stream of the CPU opponent [I: which
  of them run during a fight depends on the stage].

- **The VU1 model path (section 12)**: `Model_Render` ends every stage / scene model in VIF1 packets (a 0x48-word matrix
  block behind the template header `D_0046B174`, then DMA references to the model's parts with `UNPACK` codes inside
  the file data); the port needs the VU1 program of docs/systems/vu1 (or a native replacement that reads the same part
  data), plus the shadow packets of `vu1_packet` (DIRECT GIF code and the shadow matrix `D_01FC49F0`).
- **Model files are relocated in place** (`draw_list`, section 12.4): offsets become pointers by adding the file's
  address. A 64-bit port must keep the 32-bit layout (offsets, not host pointers) or translate on load.
- **The stage scene update runs in the simulation**: `func_00341AE8` is called by `Battle_UpdateFrame` every simulated
  frame (camera set-up, the scene build `func_00341218`, `func_003D1040`, `Wind_Update`); it must keep its place in the
  frame even when nothing is drawn (`Wind_Update` and the stage animations draw from `rand()`, section 11).
- **GS image uploads through VIF1 DIRECTHL** (`Gfx_PkAddImage`, section 13): BITBLTBUF / TRXPOS / TRXREG / TRXDIR then
  IMAGE GIF tags of at most 0x7FFF quadwords; the port's GS core takes them like the GIF-path uploads.

**Final list at the close of T17 (2026-10-21)**: what the PC port replaces, everything else in sections 2-14 stays the
game's C:

| Replaced | Original | Where |
|---|---|---|
| GS (rasteriser, VRAM, texture uploads) | GIF PACKED / REGLIST / IMAGE data, A+D writes, BITBLTBUF uploads | every packet path of section 2, `Gfx_PkAddImage`, `Sprite_SendImagePacket` |
| DMA channels, scratchpad, MFIFO rings | toSPR / fromSPR, VIF1 / GIF channels, `Dma_Wait`, BC0F waits | `dma_spr`, `dma_gif`, `gfx`, `vu1_packet` |
| VU0 skinning | `func_003435C0` .. `func_003438B8` (hand-written VU0 macro code) and the VU0 data memory set up by `func_00343340` / `func_003439B0` | `src/dma_gif.c`, section 12.1 |
| VU0 micro-programs and libvu0 | `vcallms` programs of `.vudata` (`Vu0_LoadMicrocode`), `sceVu0*` | everywhere (plain float math) |
| VU1 program | the `.vudata` packets of docs/systems/vu1 run by `Model_Render`, the stage objects and the shadow / reflection packets | `render_fighter_b`, `model_anim_b`, `vu1_packet` |
| Display | VBlank handler, double buffer, display DMA chain | `gfx` |
| 64-bit layout of model files | in-place relocation (offset + file address) | `draw_list`, `dma_gif` (`func_00343CC8`), `sprite2d` banks |

Must keep running in the original order even when nothing is drawn (they draw from `rand()` or the CPU generator, or
step the simulation): the effect update (section 7.1), the stage wind and stage animations (section 11), the scene update
`func_00341AE8` with the scene build `func_00341218` (section 12.1), the round camera.

## 9. Status

| File | Range | C / functions | Drafts (`#if 0`, compile when enabled), carried to T19 |
|---|---|---:|---|
| `src/hud.c` | 0x3C1A78-0x3C3150 | 10 / 12 | `Hud_DrawMarkers` (1 line: one store takes the register of an equal value), `Hud_DrawLifeBar` (registers and stack slots) |
| `src/model_draw.c` | 0x3DB758-0x3DC9C8 | 18 / 18 | - |
| `src/scene3d.c` | 0x34C508-0x34E748 | 8 / 15 | `func_0034C508`, `func_0034CC38` (10 lines, scheduling), `func_0034D108` (8), `func_0034D258`, `func_0034E018` (18), `func_0034E148` (21, loop layout), `func_0034E640` |
| `src/scene3d_b.c` | 0x34FF10-0x3504C0 | 6 / 6 | - |
| `src/scene3d_c.c` | 0x3504C0-0x351E98 | 9 / 15 | `func_00350728`, `func_00350C40`, `func_003517F0`, `func_00351860`, `func_003518B8` (every instruction matches; its `.lit4` word lies outside the object's pool), `func_00351AA0` |
| `src/scene3d_d.c` | 0x351E98-0x3543F0 | 34 / 37 | `func_00351E98`, `func_00353448` (1 line, an alignment nop), `func_00354120` |
| `src/effects.c` | 0x3A04B8-0x3A6460 | 55 / 58 | `func_003A0CE8` (50 lines, table loads hoisted), `func_003A1C78` (75, registers), `func_003A44F8` (13, registers) |
| `src/model_anim.c` | 0x3D0660-0x3D3630 | 69 / 72 | `func_003D0E18` (5, a v0 / v1 swap), `func_003D1980` (2, store order), `func_003D2B88` (203, registers of the round-end bonus) |
| `src/model_anim_b.c` | 0x3D3630-0x3DB758 | 82 / 101 | 19 drafts: registers / scheduling (`func_003D3B80`, `func_003D3FF8`, `func_003D43A8`, `func_003D4728`, `func_003D52B0`, `func_003D5B58`, `func_003D5E80`, `func_003D6BC0`, `func_003D7E10`, `func_003D8158`, `func_003D8D38`, `func_003D9280`, `func_003D9468`, `func_003D9588`, `func_003D9BE8`), large drafts (`func_003D62A0`, `func_003D7378`, `func_003DA138`, `func_003DAA80`) |
| `src/effects_draw.c` | 0x3A6460-0x3A9678 | 46 / 50 | `Fade_Draw` (4, two constants swapped), `func_003A76C0` (13, spill slots and order), `func_003A88B0` (24, a folded count), `Fx_AddQuad` (1, `addu` operands) |
| `src/render_fighter_a.c` | 0x340000-0x341B28 | 14 / 18 | `func_00340070` (9, two float registers, a `lui` scheduled later), `func_003403C8` (8, load / store order), `func_00340B30` (12, registers), `func_00341218` (268, registers of the scene build) |
| `src/render_fighter_b.c` | 0x341B28-0x3420C8 | 4 / 6 | `func_00341B70` (16, s0 / s1 swapped), `Model_Render` (4, the `li.s` before the last `jal`, section 12) |
| `src/vu1_packet.c` | 0x3420C8-0x342E78 | 8 / 12 | `func_003420C8` (every instruction matches), `func_00342818` (9), `func_003424A8` (104), `func_00342A38` (82): all four own `.lit4` words that the original pool puts after `func_00342E38`'s, so none can be C in place (see the file) |
| `src/dma_gif.c` | 0x342E78-0x346398 | 39 / 52 | `func_00343340` (2, one temporary register), `func_00343178` (12, the DMA wait loop shape), `func_00344BE0` (23, a1 / a2 swap), `func_003459D0` (78), `func_00345070` (84), `func_003457C0` (98), `func_00344318` (108), `func_00343CC8` (147), `func_003439B0` (167): registers; the four VU0 routines `func_003435C0` .. `func_003438B8` are hand-written assembly and stay so |
| `src/draw_list.c` | 0x356A70-0x3572A0 | 12 / 15 | `func_00356F48` (7, `xori` / `movn` form), `func_00357160` (16, s0-s2 rotated), `func_00356A78` (129, first draft; its jump table lies among other objects' `.rodata`, so it cannot be C in place) |
| `src/font_tex.c` | 0x3608B8-0x3621F8 | 26 / 30 | `func_003608E0` (13, block layout), `func_00360B90` (6, registers of one argument), `func_00361E20` (30, registers), `func_00361EA8` (29, registers) |
| `src/sprite2d.c` | 0x3CD298-0x3D0660 | 46 / 59 | `Sprite_DrawCurrentGouraud` (2, two GIF tag stores swapped), `func_003CF608` (4, t0 / t1), `func_003CEEC8` (18), `func_003CE920` (22), `func_003CD298` (28), `func_003CF850` (31), `func_003CF500` (35), `func_003CF278` (38, `lb` + `sllv` bit test), `NameEntry_Step` (43), `func_003CE5D8` (45), `func_003CE770` (51), `func_003CFAD8` (161), `func_003CFE38` (324) |

The reasons are in each draft's `NON_MATCHING` note (counts are fdiff lines) and in [decomp_guide.md](../decomp_guide.md),
"Lessons from T17 batch 1" to "batch 6". T17 batch 4 matched the 20 drafts that waited for the build: the hazard nops
gas added (before an unfilled `bc1f` / `bc1t` after an FPU compare, between `li.s` and a prelude `mtc1`) are gone
(`configure.py` rewrites the compiler's output), and `effects.c`, `effects_draw.c`, `scene3d.c` and both `model_anim`
objects are built without `-ffast-math`.

**T19 batch 2** (2026-10-22): matched `Fx_AddQuad`, `func_003420C8` and `func_00342818` (`.lit4` order through
`LIT4_ORDER`), `func_0034A570`, `func_003518B8` and `Model_Render` (words in another part of the pool: `LIT4_REF`; the
`li.s` before `Model_Render`'s last `jal` stays out of its delay slot), `func_003403C8`, `func_00340B30`,
`func_00341B70`, `func_003D43A8`, `func_003D8D38`, `func_003D4728`, `func_003CF608`, `func_003CE920`, `func_003CE770`,
`Sprite_DrawCurrentGouraud`, `func_0034D108`, `func_0034E640`, `func_00354120`, `func_00360B90`. `draw_list` and
`render_fighter_b` have `.rodata` subsegments (0x522030 / 0x522010), so `func_00356A78`'s jump table can come from C.
The drafts left: [../t19_inventory.md](../t19_inventory.md).

**T19 batch 4** (2026-10-22): matched `func_003D0E18` (`model_anim`, builds the scene's node tree). Its callee
`func_003D1D98` is declared `s32`, though it returns nothing: as `void`, the node counter lands in another register.
The drafts left: [../t19_inventory.md](../t19_inventory.md).

## 10. Open

- Not decompiled yet in the render paths: `zsort` (0x3994C0-0x399FC8), the effect pool itself (`func_003464F8` ..
  `func_00346830`, `geometry` range) and the dispatcher `func_0035BF10` (T19 or the module batches that own them).
- How the original wrote the 64-bit to 128-bit move of the sword effect's TEX0 (`por`), which this compiler refuses.
- The VU1 program the model parts run on (section 12) and the stage objects' VU1 program.
- How the original produced `.lit4` pools out of text order (`vu1_packet`, `geometry_b`) and words in another part
  of the pool (`Model_Render`, `func_003518B8`); the build reproduces them with `LIT4_ORDER` / `LIT4_REF` (T19 b2), the
  mechanism is unknown.
- How the original wrote the bare `sqrt.s` of the objects built without `-ffast-math` (`scene3d`, `effects_draw`, both
  `model_anim` objects) and of `sprite2d`'s `func_003CF408`: the C uses an inline-asm square root (marked fake), while
  `func_003A8D08` has the errno form of `sqrtf` with the libcalls `sqrt` and `__fixunssfsi` (decomp_guide.md, "Lessons
  from T17 batch 4").

## 11. Models (`src/model_anim.c`, `src/model_anim_b.c`)

T17 batch 3 (2026-10-20). The `model_anim` range holds no fighter models (those are `skeleton`, `dma_gif` and
`render_fighter_a`): it is the **stage scenery** (animated stage parts and stage objects), the stage wind, the round
camera shots, and the fight bookkeeping the simulation calls (tag counters, move log). Two objects: 0x3D0660-0x3D3630
and 0x3D3630-0x3DB758 (game_overview.md section 4) [V].

### 11.1 Model hierarchy (`src/model_anim.c`, 0x3D0C68-0x3D22E0)

- **Scenes**: `D_00520FC0` (`.rodata`, 0x10-byte entries `{ModelDef *def, s32, draw(nodes), init(nodes)}`). A
  `ModelDef` lists parent / child links (`s16[2]`), node definitions (0x40 bytes: indices, rotation, translation, update
  and init callbacks), per-part kinds and floats. `func_003D0CC8(scene)` resets the pools and copies the kinds and
  floats into `D_00444C60` / `D_00445620`; `func_003D0E18` builds the node tree (from the stage set-up in
  `fighter_setup`, 0x367970 / 0x367BF0); `func_003D1040` sets the camera matrices and calls the scene's draw callback
  (from `loading_title`, 0x3A991C) [V].
- **Nodes** (`ModelNode`, 0x70 bytes, pool `D_00445690[64]`, counter `D_005F677C`): camera slot, flags (view-relative,
  Euler order, drawn), draw list, parent, children (pool `D_01FAD710`), word and float arrays (pool `D_01FAD810`), part
  index, resource set, scale, rotation (+0x40), translation (+0x50), update callback (+0x60). `func_003D1B68` copies a
  subtree [V].
- **Matrix stack** (VU0 matrices through libvu0): current matrix `D_01FAD6D0`, stack `D_01FAD450[10]`, depth
  `D_005F6EF0`; `Model_ClearMatrixStack`, `Model_PushMatrix`, `Model_PopMatrix`, `Model_LoadMatrix`, `Model_MulMatrix`,
  `Model_GetMatrix`, `Model_LoadIdentity` [V].
- **Draw path**: `func_003D10D0(part, m, scale)` / `func_003D1160(node, m, scale)` walk a subtree (`func_003D1468`,
  recursive: rotation + translation into the matrix stack, camera x world, depth); `Model_QueueDraw` copies the 0x150-byte
  draw context (five matrices and the part) into a draw object (pool `D_01FADC10[96]`, 0x160 bytes) and queues
  `{depth, object}` in one of four lists `D_01FB6110[4][96]` (counts `D_01FB6100`). `Model_FlushDrawLists(a)` runs from
  the stage pass (`func_00357C88`, `func_00357D38`): lists 0-1 with `a = 0`, lists 2-3 with `a = 1`; lists 1 and 3 are
  merge-sorted by decreasing depth (`func_003D1F00`, back to front, the translucent lists [I]) and every entry goes to
  `Model_Draw(res, obj, lights, kind)` (section 12: VIF1 packets for the VU1 program) [V calls; I roles].

### 11.2 Stage animations (`src/model_anim_b.c`, 0x3D47A8-0x3D8158)

Per-stage node update callbacks (handler arrays `D_0044B230`, `D_0044B800`, `D_0044BEA0`, `D_0044C1A0` in `.data`,
game_overview.md section 3) that move parts of the scene every frame [V code; I looks]: a flock of ten birds of four
parts each and a leader (`func_003D4830` / `func_003D4B18`), parts on ten lanes wrapping around the camera
(`func_003D5B58` / `func_003D5E80`), orbiting and bobbing parts, parts that turn by the clock or toward a target, a
40-node ring with a Hermite curve in Newton form (`func_003D6BC0`, curve at `D_00444D78`), debris blown off by blast
points (`func_003D7378`), a camera node (`func_003D7AB0`), stereo panning of stage sounds from distance and view
direction (`func_003D4F58`, `func_003D5998`, `func_003D62A0`, through `sound`'s `func_00375930` / `func_003758F0`).
The work area `D_00444CD0` (`.data`) is a per-stage union. Helpers: `func_003D8128` (`rand() * 2^-31`), `func_003D80E8` /
`func_003D8088` / `func_003D8038` (angle wraps), `func_003D47A8` / `func_003D47E8` (random ranges).

### 11.3 Stage wind (`src/model_anim_b.c`, 0x3D3630-0x3D47A8)

`Wind_Init(stage)` (battle_flow, round set-up) starts the stage's generators (`D_00447EC0`, 0xC bytes per stage); each
generator (`D_00447FE0`, 3 x 0x1060) is a Lorenz attractor (`Wind_StepLorenz`: sigma 10, rho 28, beta 8/3, dt 0.01)
sampled into 0x400 values; four gusts (`D_0044B100`, 0x30 bytes: radial pushes that fade). `Wind_Update` steps them every
unpaused frame and records a 120-frame ring (`D_01FB6EF0`) that replays reuse; `Wind_Sample(x, y, z, out)` gives the
wind at a point (the fighters' secondary motion, section 4, and effects). It is float code fed by the round's time
only, so it is picture state unless something in the simulation reads it [I: no simulation reader found].

### 11.4 Stage objects (`src/model_anim_b.c`, 0x3D8158-0x3DB758)

- **Lists** `D_01FBF680` / `D_01FC0D00` (0x1680 bytes): up to 36 objects of 0x90 bytes (matrix, definition, animation,
  position, Y rotation, scale, wait, repeat count, frame, animation id, bounding box, bank, clip flags), bounding boxes at
  +0x1440, the clip volume at +0x1640, a bone parent table at +0x1660, the count at +0x1678. Set up by `func_003DB578`
  from the stage's entry in `D_00453FC8` (`func_003D9280` places each object from its 0x20-byte definition; random
  variants and repeat chains `func_003D8908` / `func_003D8B20` / `func_003D8D38` / `func_003D8E30`); banks of models
  `D_01FC2380[16]` and the hook pair by `func_003DB368`; flags `D_005F67B8` [V code; I names].
- **Per frame**: `func_003DB698(frames)` (from battle_flow with `D_005F5F10`, the frames stepped) calls the update hook
  `D_005F6FD0(list, t, n)` on each active list: `func_003D9588` or `func_003D9BE8` step the animations (`func_00367CD0`
  frame counts), sort or push the animated objects, build their matrices and clip them with `sceVu0ClipAll` (a shear
  matrix gives the planar shadow on y = 100 along the light of `func_00353C88(0)`); `func_003DB2A8` loops a stage sound
  on stage 15 [V].
- **Draw**: the stage pass calls `D_005F6FD4(list)` (`func_003DB758` / `func_003DB7B0`, src/model_draw.c):
  `func_003DA138` / `func_003DAA80` build the 18 bone matrices with inline VU0 macro code (`vmula` / `vmadd` rotations,
  sin / cos from `func_003E8FA0`) in a scratchpad work area (`Spr_Setup(1, 0x34C0)`), then send VIF1 packets: the
  template `D_004FAF80`-`D_004FB800` (VU1 code and set-up), the matrices, and DMA references into the bank's data; each
  object is drawn again with the shadow matrix when its shadow is visible [V calls; I details].
- **GS images of model files**: `func_003D8158` relocates a model file's image records and patches them for draw modes
  0-3; `func_003D83A8` uploads a record chain at once (libpkt, `func_00354290`, `func_0035E6B8`) [V].

### 11.5 Round camera and fight bookkeeping (`src/model_anim.c`)

- **Round camera shots** (0x3D0660-0x3D0C68, 0x3D22E0-0x3D29E8): `func_003D0660` picks one of 14 shot sequences with the
  CPU generator `func_003800D8` and keeps the last hit position; `func_003D0748` places the eye of the next shot from
  `D_00520C40` (0x14-byte shots: target source, rotation and random spread, distance) chosen by `D_00520F88`, around a
  fighter or the hit position; `func_003D0A38` eases the target; `func_003D2330` / `func_003D2818` set up and play a
  scripted move (`D_00521140`, quarter-sine interpolation). Camera keys go through `func_0035B690` / `func_0035B858`
  (src/stage.c's camera). Because they draw from the CPU generator, these run in the simulation order [V calls].
- **Move log**: `Fighter_LogMove` (when a move starts) counts flagged moves per team member (`Fighter + 0x5C`) and moves
  repeated within the member's last 20 (`+0x60`, log at `+0x64`); `Fighter_ClearMoveLog` resets it;
  `func_003D2B88` (round end) pays a bonus `D_005F5CC8[side]` when the counts pass per-character thresholds
  (`D_00521190`), rescaled by the time left [V code; I meaning of the bonus].
- **Tag**: `Tag_StepCounters` counts the per-side counters `D_01FB6ED0` / `D_01FB6EE0` down (1 finishes the swap,
  `func_003D33F8`; 0 lets `Battle_RecoverLife` run, `func_003D3528`); see fighter.md section 5.7 [V].

**Compiler**: both objects are built without `-ffast-math` (`configure.py` `NO_FAST_MATH`, T17 batch 4): five functions
match only that way (`func_003D1468`, `func_003D4B18`, `func_003D4F58`, `func_003D5798`, `func_003D5998`: `-x + c` kept as
`neg.s` / `add.s`, divisions by constants kept), every other function compiles the same either way; their bare `sqrt.s`
comes from an inline-asm square root, not from `sqrtf` (decomp_guide.md, "Lessons from T17 batch 3").

## 12. Stage renderer and the VU1 model path (`render_fighter_a`, `render_fighter_b`, `vu1_packet`, `draw_list`)

T17 batch 5 (2026-10-21). The module names are provisional and wrong again: `render_fighter_a` is the **stage**
renderer, the fighters are drawn by `dma_gif` (VU0) and `main`. Objects: 0x340000-0x341B28 and 0x341B28-0x3420C8 are two
objects [V: their `.lit4` words lie in two pools, 0x5F5378-0x5F539C and 0x5F5700-0x5F570C]; 0x3420C8-0x342E78 is a third
[V: its own pool 0x5F57EC-0x5F5804]; the edge at 0x341B28 is [I] (no data between 0x341AE8 and 0x341DC8).

### 12.1 The fighter draw path, end to end

1. **Simulation** (`Battle_UpdateFrame`, fighter.md section 5): the skeleton (`Skel_Update`, animation.md), then
   `func_00341AE8` per simulated frame: camera and scene set-up (`func_0035B4B0(D_00415808, D_00436530)`,
   `func_00366ED0`), the stage scene build `func_00341218`, `func_003D1040`, `Wind_Update` [V calls].
2. **Fighter pass 0** (`func_00357A78`, section 2): per drawn fighter `func_00357640(i)` sends the fighter's DMA packet
   (`func_003430D8` builds it from the fighter's model bank `func_003B8798(f->unk22)`) through the scratchpad,
   `Fx_DrawFighterTex` uploads its texture animation, `func_00343340(f, 0)` (`src/dma_gif.c`) loads the view matrix, the
   light matrices (`Light_GetFighterMatrices`) and the camera offset into VU0 data memory and calls `func_003439B0` for
   each of the ten part slots of the model, which runs the hand-written VU0 skinning loops (`func_003435C0` ..
   `func_003438B8`, assembly) and streams GIF packets [V].

   The skinning in detail (T17 b6, [V code of the drafts; I roles of the VU0 programs]):
   - `func_00343340(f, pass)` writes VU0 data memory (0x11004000): the matrix `D_004334C0` at quadwords 4-7, three rows
     from `Light_GetFighterMatrices` at 8-10, the light vector (`D_00415808` minus the fighter position, normalised by
     `func_0034A868`) at 11, the matrix `D_00433400` at 0xFC-0xFF, and loads four more rows into `vf8`-`vf11`. The model's
     part table (`func_003B87B0(f->unk22)->unk8 + 0x2C`) holds per pass two rows of five part ids; `D_005F67C0 == 1`
     skips the second row (C, 2 instructions off).
   - `func_003439B0(part, mats)` streams the part's segments (`func_00343CA8` finds them) through two scratchpad buffers,
     fetching the next segment by DMA while the current one is processed. Type 1 records open a block (copied as they
     are, they set the mode of the following records), type 2 records are expanded into VU1-ready vertex packets by the
     four hand-written VU0 routines `func_003435C0` .. `func_003438B8` (`lqc2` / `ctc2` / `vcallms` macro mode, jumps
     through `$t4` into an unrolled body: written in assembly, not compiler output), type 0 uploads up to n skin
     matrices from `mats` (0xC0 bytes per joint) to VU0 memory. Each finished segment is sent with `func_00343178`.
   - `func_00343CC8(part, tbp)` relocates a loaded part in place: segment offsets become addresses, type 1 records get
     log2 of the texture size, type 2 records get their TBP0 / CBP moved by `tbp` and vertex indexes scaled by 12.
   - `func_00344318` rebuilds the fighters' texture upload packets per texture set (`unk22` bits 0-1) into `D_00168A30`
     (`func_00354120`, VRAM base 0x3B40) and keeps a TEX0 value per record; `func_003459D0` draws a billboard effect
     object (projection, depth size, culling, ALPHA / RGBA, the quad through `func_003457C0`).
3. **Fighter pass 1** (`func_00357B50`): `func_00343340(f, 1)` for the second set of parts, then the sword effect
   (section 3), effects, the HUD lists (section 5) [V].
4. **Shadows** (`vu1_packet`, from the stage pass `func_00340E28`): `func_00342128` builds the planar shadow matrix
   `D_01FC49F0` from the light direction of the fighter's side (`func_00353C88`), scaled by 1 / `D_005F694C` (3200, or
   5800 when a fighter has `unkB130` bit 1, `func_003420C8`); `func_003422C0` draws a model relative to the fighter
   with it; `func_00342390` / `func_00342400` / `func_00342988` send fixed VIF1 / DIRECT packets (`D_0046B180`,
   `D_0046B240`, `D_00522F90`); `func_003424A8(f, offset)` draws the fighter's shadow: the `.vudata` template
   0x4FAF80-0x4FB800 (VU1 code and set-up) copied to the scratchpad, the shadow matrix (light direction of the side scaled
   by 256 / `D_005F694C`, centred at 2048) times each of the 15 skeleton joints (`D_0046B2B0`, joints 11 and 14 turned by
   -150 degrees about Z) and the model's extra joints (`func_003B87C8`), then DMA references to the shadow model's parts
   (`func_00367350`); `func_00342818(f)` draws the side's shadow and, for side 0 with the partner drawn, the partner's
   shadow within 1590 units; `func_00342A38(f)` draws the fighter again flattened onto one depth through the camera
   matrices (`D_00433400`, `D_00433480`, y mirrored; the reflection of stage 14 [I]) [V drafts; I roles].

### 12.2 Stage scene (`src/render_fighter_a.c`)

`D_005F6780` points to the scene (0x1A300 bytes): the camera position (floats at 0x14), up to five level-of-detail
thresholds (`levels`, `thr[4]` at 0x30), the floor boxes at 0x60, the 16 x 16 cell grid of model ids at 0xF0, 256 entries of
0x150 bytes at 0x300 (matrices, position, light set at 0xC0, file / model / flags at 0x140), 256 matrices at 0x15300, and
the depth-sort keys at 0x19300 with their work array at 0x19B00 [V offsets; I roles]. `func_00340BA0` clears the keys, `func_00340DF8` sorts them with the
merge sort `func_00340BF0` (largest key first, indexes in `.sbss` 0x5F6F1C-0x5F6F28), `func_00340408` picks a level 1-5
from the mean depth of four corners (65535 when none is in front), `func_00340528` tests a value between two bounds,
`func_00340AF0` a point within a radius, `func_00340B30` meets a line with the floor plane y = 0.

T17 b6 [V code]: `func_00340070(stage)` sets the scene up from the stage's model bank (`func_00356E50`: level count, grid
size, stage width and depth) and record `D_00521700[stage]`: the cell grid (`(row % rows) * cols + col % cols`), the floor
box corners (scaled by the record's factor) and, when the record has a raised floor height, a second box; the level
thresholds (stage 0: 8000; `func_003AA5A0()`: 0 / 50000; otherwise 5000 / 8000 / 10000 / 13000, and 65535). `func_00341218`
(every simulated frame, from `func_00341AE8`) builds the scene in a scratchpad work area: view matrices, the camera's grid
cell (cell origin to `unk40`, wrapped `row` / `col`), the fighters' shadow centres projected along the light, then for
a rows x cols window of cells around the camera a distance cull (`unk20`), a screen cull (`func_003408B0`: the floor
corners projected, `func_00340588` tests whether a box edge crosses the 640 x 448 screen), a level of detail
(`func_00340408`), an entry (matrices, position, model id from the grid, shadow flags within `unk28` of a fighter's
shadow centre) and its sort key; it ends with the depth sort. `func_00340E28(pass)` returns when the pass's bank is not
loaded, builds the light set rotated by the light angle, draws every sorted entry with `Model_Draw` (on pass 0 with
`unk2 == 1` through `func_00342F98` with the entry's value at 0x13C), then the fighter shadows: `func_00342A38` per drawn
fighter when `D_005F5CB6 == 14`, else `func_00342818` / `func_00342128` per fighter and `func_003422C0` for every entry
flagged in that fighter's shadow; it ends with `func_00342988`. `func_00340000` /
`func_00357258` give every one of the 27 stage model entries the light value of the stage record (`D_00521700`, 0x30
bytes per stage). `func_00340E28(pass)` is the stage draw of the stage pass (`func_00357C88`, `func_00357D38`); it calls
`Model_Draw` and the shadow senders above; `func_00341F68` draws one model with the camera (`D_00433440`, `D_00433400`)
[V calls].

### 12.3 The VU1 model renderer (`src/render_fighter_b.c`)

- `Model_Draw(files, obj, matrices, kind)` (from `Model_FlushDrawLists`, section 11.1, and `func_00340E28`): picks model
  `obj->model` (+0x144) of file `obj->file` (+0x140) and calls `Model_Render` [V].
- `Model_Render(model, matrices, obj, kind)`: kinds 0-5 first send the object's matrix and the given matrices
  (`Model_SendMatrices`: a header copied to the scratchpad from `D_0046B174`, a 0x48-word block filled by
  `func_00342E78`, `src/dma_gif.c`); kinds 0 and 4 also set the depth factor of `func_00342FA8` for the fight: from the
  nearer fighter's depth `Fighter.unk620` (minus 500) when `func_0036E158()` and the stage is not 12, or from a fixed
  7000 / 14000 when `D_0043B7F8[0] == 8` and `func_003E9400()`, then reset to 1 / 32767 after the matrices. Every kind
  then sends the parts (`Model_SendParts`: the 0x70-byte header to the scratchpad, then one DMA reference per part, each
  part's size in quadwords from the header, the parts 16-byte aligned after it) [V code; I meaning of the factor].
- `func_00341B70` is the same with the parts drawn through the shadow matrix sender `func_003421D8` [V].

`Model_Render` is the one function left out by an assembler difference: the original keeps the `li.s` of 1 / 32767
in front of the last `jal` with a `nop` in the delay slot, where gas moves the load into the slot (decomp_guide.md,
"Lessons from T17 batch 5").

### 12.4 Model bank loading (`src/draw_list.c`)

Not a draw list: the module relocates loaded model files in place. `func_00357160(id, slot, file, a)` loads stage model
bank `slot` (`D_01FC4680`, 0x70 bytes per slot: a root table and 27 sub-tables): the 27 entry kinds are 5, then 2 or 3
from the stage's record (`func_00356F48(func_00340398())`), and kind 3 entries get their light value
(`func_00340000`); `func_00357028` turns the root table's offsets into pointers (offset + file address) and
`func_00356F68` each sub-table's, calling `func_00356A78` (a draft, a jump table on the kind: patches the GS registers
of every part's packets, sets up the stage light on kind 0, `Light_SetupStage`) on each entry. `func_00356D78` uploads a
chain of GS image records through a GIF packet (as `func_003542E8`, plus `sceGifPkEnd`); `func_00356E00` /
`func_00357128` upload into fixed buffers; `func_00356E50` / `func_00356EE8` pick the bank of a fighter side from
`func_003678A8` / `func_003678B8` [V code; I meaning of the kinds].

## 13. Memory card screens and the GS image upload (`src/font_tex.c`)

The `font_tex` module holds no font: 0x3608B8-0x361B88 continues the memory card code of `src/memcard.c` and
0x361B88-0x3621F8 uploads images into GS memory [V].

- **Screens**: the menu state `D_006173F8` (`McMenu`: state, next state, selection, YES / NO cursor, timer, message,
  held and pressed buttons, then two animated windows of `func_003604C8` .. `func_00360530`). `func_00360A30(m, msg)`
  opens message `msg` (`D_00416078`: color, line count, first text line) sized to its widest line; `func_00360B90`
  draws it; `func_003608E0` draws the YES / NO window ("%H%V%F%S%cYES %cNO"); `func_00360CA0` reads the YES / NO input;
  `func_00360D50` moves the menu selection; `func_00360DE0` / `func_00360E38` / `func_00360E88` classify a card status
  (bits 1, 8, 4, 0x400, 0x100, 0x200, 0x4000). `func_00360F40` is the menu state machine (28 states, jump table,
  C since T17 b6: states 0-2 the entry list, then four YES / NO flows that check the card, start a request (0x1A, 0x12,
  0x10, 0x25) and show the result, 26-27 write the option byte back); `func_00361978` the card check at boot and `func_00361710` the error screen (both C, request functions of
  `src/memcard.c`). Sound effects go through `func_003B4D28` [V].
- **GS image upload**: `Gfx_PkAddImage(pk, dbp, dpsm, dbw, data, qwc, x, y, w, h)` adds BITBLTBUF / TRXPOS / TRXREG /
  TRXDIR as A+D through VIF1 DIRECTHL (`D_004FC350`, the A+D GIF tag), then the image as DMA references of at most 0x7FFF
  quadwords each behind an IMAGE GIF tag (`D_00416190`, NLOOP patched). `func_00361EA8` (assembly) places the five
  texture images of `D_006E3550` (`TexSlot`: data, a skip flag, size in 256-byte blocks; `func_00361D50` sets one) from
  block 0x2300: `func_00361D70` sorts them by size, `func_00361E20` leaves out the largest until the rest fits in 0x1640
  blocks, then the packets are streamed through the scratchpad [V code; I which images].

## 14. 2D sprites and text (`src/sprite2d.c`)

T17 batch 6 (2026-10-21). The `sprite2d` range 0x3CD298-0x3D0660 is one C file with its own `.rodata` (0x520648-0x520C40)
and `.lit4` (0x5F5364-0x5F5370) subsegments; its start follows `select_results`' data without a gap, so 0x3CD298 is a cut
[I], the end is `model_anim`'s start. Everything is written into the GIF buffers of section 2 (A or B, chosen by the first
argument), GS context 2 [V]:

- **Packets and state**: `Sprite_BeginPacket(buf)` / `Sprite_EndPacket(buf, p)` open and close buffer A or B;
  `Sprite_SetBlend` writes TEST_2 / ALPHA_2 from the blend table `D_00520648[mode]` and CLAMP_2, `Sprite_SetFilter`
  TEX1_2 (bilinear on / off), `Sprite_TexFlush` TEXFLUSH. Draw state: Z `D_005F6730 >> 4`, colour `D_005F6738`,
  ALPHA `D_005F6740`, PRIM bits `D_005F6748`; eight colours `D_0043B840`.
- **The current sprite** `D_00440F20` (0x40 bytes: TEX0, screen and texel rectangles, four alphas, rotation):
  `Sprite_SetTexture(tex, clut)` builds TEX0 from two entries of the sprite table `D_00440F60` (0x20 bytes: GS buffer,
  rectangle, upload packet); `Sprite_DrawCurrent` draws it as a SPRITE, `func_003CDC10` as a strip,
  `Sprite_DrawCurrentRotated` turned around its corner (libvu0 RotZ), `Sprite_DrawCurrentGouraud` with four alphas.
  `Sprite_Draw` / `Sprite_DrawBlended` (native size) and `Sprite_DrawScaled` / `Sprite_DrawScaledBlended` (w x h) are what
  the HUD and the menus call; `func_003CE418` / `func_003CE508` take a texel sub-rectangle.
- **Untextured primitives**: an alpha strip of four vertices (`func_003CE5D8`), a Gouraud strip with a colour per vertex
  (`func_003CE770`), an alpha triangle (`func_003CE920`), the spotlight and iris masks of the screen transitions (the
  screen darkened outside a circle `func_003CF850` or an ellipse `func_003CFAD8`, 15 segments per quadrant, step
  3.14 / 30) and a window bevel of three gradient quads (`func_003CFE38`).
- **Image banks**: `Sprite_LoadBank(bank, dest, size)` loads a bank file (`func_0035D048`) and `Sprite_UploadBank` sends
  its images; per image `func_003CEB68` / `func_003CEC98` link it to its packet and patch BITBLTBUF, `func_003CEEC8`
  records its rectangle and `Sprite_SendImagePacket` sends the packet over GIF DMA (channel 2). `func_003CF488` /
  `func_003CF4C8` / `func_003CF4E8` map an image to its bank (`D_00520990`); sprite sets (`func_003CF608`, `D_00444310`,
  defaults `D_00520A68`) place layouts.
- **Not sprites**: the score and time record tables (`func_003CD298`, `func_003CD440`, `D_00440D50`), the partner-counter
  pair rules `func_003CF278` (`D_005207F0`), `Num_ToBcd`, the three-letter name entry (`NameEntry_Init`, `NameEntry_Step`,
  letters `D_00520BE8`, reserved names `D_004443D8`), `Fighter_ClearLife`, the practice hit marker `func_003D0548`
  (`D_00520C18`) and the practice set-up `func_003D0620` [V].

