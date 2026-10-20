# Game code overview

Top-level structure of the game's own code in `SLUS_200.01` (0x340000-0x3F3320 minus libvu0 and crt0, see
[text_map.md](text_map.md)): boot, main loop and frame pacing, mode dispatch, a provisional module map, the
important globals, the random number generators and the file loader. Function names are the ones in
`config/symbols/`; `func_XXXXXXXX` and `D_XXXXXXXX` are still unnamed.

Every claim is tagged:

- **[V]** verified: read in the disassembly (the instruction sequence does this).
- **[I]** inferred: from strings, callers, call graph reach or the library's API; not proven.
- **[PCSX2]** needs a runtime check (no emulator or BIOS was available when this was written).

Read statically from the split assembly. `scripts/callgraph.py` answers the call graph questions used here
(`callers`, `callees`, `reach`, `ptr`, `xref`, `strings`, `tables`, `range`).

## 1. Boot path

| Step | Address | What it does |
|---|---|---|
| `_start` (crt0) | 0x3572A0 | [V] Zeroes 0x5F6B00-0x1FCDEF0 (`.sbss` + `.bss`), SetupThread (syscall 0x3C: gp 0x5FD0F0, stack -1, stack size 0x100000, args 0x1FC63C0, `_root`), SetupHeap (syscall 0x3D: heap start 0x1FCDEF0, size -1), FlushCache(0), `ei`, `main(argc, argv)`, then Exit with its result. |
| `main` | 0x3577E8 | [V] `__main()` (constructors), `Sys_Init()`, then the main loop (section 2). Never returns. |
| `Sys_Init` | 0x357430 | [V] Init sequence below, then mode 0, sub-state 0, frame counter 0, FlushCache(0). |

`Sys_Init` order [V for the order and the SDK calls; roles of unnamed game functions are I]:

1. `Sys_BootIop` 0x357358: `sceSifInitRpc(0)`, `sceCdInit(0)`, `sceCdMmode(1)` (CD media); reboot the IOP with
   `cdrom:\IRX\IOPRP165.IMG;1` (retry until it succeeds), wait in `sceSifSyncIop`; again `sceSifInitRpc`,
   `sceCdInit`, `sceCdMmode(1)`; `sceSifLoadFileReset`; then `sceSifLoadModule` for the six entries of the
   pointer table 0x415700, each retried while the result is negative: `cdrom:\IRX\SIO2MAN.IRX;1`,
   `MTAPMAN.IRX`, `PADMAN.IRX`, `MCMAN.IRX`, `MCSERV.IRX`, `RSPU2DRV.IRX` (same path form). Module result
   word: 0x5F5C10.
2. `sceDmaReset(1)`, `sceVpu0Reset()`, `sceGsResetPath()`, write 4 to GIF_MODE (0x10003010: IMT, intermittent
   PATH3).
3. `func_0035D458` [I: DMA/scratchpad state], `func_0035E400` (installs a DMAC interrupt handler with
   `AddDmacHandler`).
4. `Gfx_Init` 0x398940 (section 3), which ends by installing the VBlank handler.
5. `sceSdRemoteInit` 0x404AD8 (librspu2: binds RPC 0x80000601 of RSPU2DRV, retrying; then `sceSdRemote(1, 1)`).
6. `Mc_Init` 0x360410 (`sceMcInit`), `func_00361B18`, `Snd_InitDriver` 0x399FD8 (RSPU2DRV command 0x2000), `File_Init`
   0x397B40, `func_0039C7B8` (movie state reset).
7. Four fighter slots at 0x879640 + n * 0xB8A0: halfwords +0x1E / +0x20 set to (0,0), (1,1), (0,2), (1,3) [V]
   [I: side and slot index].
8. About 25 game initialisers (0x3B9058, 0x3B8A50, 0x357600, 0x3585E0, 0x3BAA90, 0x39A140, 0x3BFBB0,
   0x39C110, `func_0039BBF0(0x100, 0xF0)`, `func_00398540(0x352)`, `Rng_Init`, 0x393898, 0x3941F0, 0x3673F8,
   0x367930, 0x3455D0, 0x346430, 0x3A6BD0, 0x3A7190, `func_003C12D0(1, -1)`, 0x3B7A58, `func_003A9688(0)`,
   0x37CFA8, 0x3DE930, 0x35D078) and `Mode_Change(2)` [I: queue mode 2 after the boot mode].

Memory [V]: the game never calls `malloc`/`free` (only newlib's stdio does); every buffer is static `.bss`,
which ends at 0x1FCDEF0, 32 MB minus 0x32110 bytes. With a 1 MB stack requested at the top of RAM, the last
part of `.bss` and the stack share address space [I, PCSX2: check where the kernel puts the stack].

## 2. Main loop and frame rate

`main` 0x3577E8 [V]:

```c
for (;;) {
    Frame_Begin();             /* 0x357B70: VU0 microcode for the mode, then wait for VBlank start */
    func_00357BD0();           /* Gfx_SetPhase(1), start of the frame's display list */
    func_00357A50();           /* draw passes (stage/background, ...)        [I] */
    func_00357A78();           /* draw the fighters: slots 0..D_005F6B00-1  [I] */
    func_00357B28();
    func_00357B50();           /* z-sorted list flush (func_00399918)        [I] */
    Frame_PreUpdate();         /* 0x357888: frame counter, input, RNG reseed */
    Mode_Dispatch();           /* 0x3578F0: one update of the current mode */
    if (D_005F6B00[3]) func_00346608();
    D_005F5C00 = func_00356A20(D_005F5CB6);   /* byte tables 0x455D50 / 0x455D68, 23 entries */
    D_005F5C04 = func_00356A48(D_005F5CB6);
    FlushCache(0);
    Frame_End();               /* 0x357BF0: services (sound, files, memory card, movie) */
}
```

The draw passes run before the update in each iteration, so a frame shows the state left by the previous
`Mode_Dispatch` [V for the order].

### Pacing

- `Gfx_Init` 0x398940 [V]: `sceGsResetGraph(0, 1, 2, 1)` = reset, interlaced, NTSC, FRAME mode;
  `sceGsSetDefDBuff(&gGfx, PSMCT24, 640, 224, ZGREATER (3), PSMZ16S (0x3A), clear)`; Timer 0 mode
  (0x10000010) = 0x82: counting, clock BUSCLK/256 (576 kHz); clears VU0 and VU1 micro/data memory
  (0x11000000-0x1100FFFF); uploads VU0 microcode (0x4F5F00) through VIF0 DMA and VU1 microcode (0x4F7A00)
  through VIF1 DMA (`Dma_SendChain` 0x343028 + `Dma_Wait` 0x343278); `Pad_Init`; waits for a VSync
  (`sceGsSyncV(0)`); `Gfx_InstallVSyncHandler(0)`.
- `Gfx_InstallVSyncHandler` 0x398FC0 [V]: `AddIntcHandler(2, Gfx_VSyncHandler, 0)`, `EnableIntc(2)`: cause 2
  is INTC VBLANK_START. No other INTC handler is installed except the movie player's (0x39CDC0, also cause 2
  [I]).
- `Gfx_VSyncHandler` 0x398E48 [V]: if the frame-ready flag `D_005F6C70` is set (or gGfx+0x264 is set), it swaps
  the display buffers (`func_00398D40`), reads the FIELD bit of GS CSR (0x12001000 bit 13), and depending on a
  state byte kicks a DMA chain on VIF1 (0x10009000) or GIF (0x1000A000) (`func_00398DD8`); then clears
  `D_005F6C70` and increments the VBlank counter gGfx+0x254 (`Gfx_GetVSyncCount`).
- `Frame_Begin` 0x357B70 [V]: `Vu0_LoadMicrocode` (one of three VU0 programs, chosen by the mode), interrupts
  off, `Gfx_SetPhase(0)`, `func_0035D790(D_005F5C00)`, `Gfx_SetFrameReady()` (`D_005F6C70 = 1`), interrupts on,
  `Gfx_WaitVSync()`: spins (calling `Rng_SpinNext`) until the handler clears the flag, then zeroes Timer 0.
- `Frame_PreUpdate` 0x357888 [V]: increments the frame counter `D_005F5C18`, busy-waits until Timer 0 reaches
  0x724 (1828 ticks, about 3.2 ms after VBlank start) [I: lets the pad data of this VBlank arrive], then
  `Pad_Update`, `func_00358758`, `func_003BAE40`, `Rng_Reseed`.
- `Frame_End` 0x357BF0 [V]: `Snd_UpdateVoices` 0x374A88, `File_Update`, `Snd_Flush`, `Mc_Update`, `func_0035DF38`,
  `sceGsSyncPath(0, 0)`, then samples Timer 0 into `D_005F5C08` and sets `D_005F5C0C = (T0 >= 0x246E)`
  (9326 ticks, about 16.2 ms, read by `func_003459D0` [I: a frame-overrun flag]), `Gfx_SetPhase(2)`,
  `Movie_Update(0)`, `func_0039C2C0`.

**Simulation rate [V, timing PCSX2]: one game update per VBlank, NTSC, 59.94 Hz.** Each iteration calls
`Mode_Dispatch` exactly once, and each iteration waits in `Frame_Begin` for the next VBLANK_START interrupt to
clear the flag it has just set. When the work of a frame overruns, the loop waits for the following VBlank:
the game slows down; there is no frame skipping and no fixed-step catch-up loop. The image is 640x224 per field
in interlaced FRAME mode [I: one 224-line image shown per field, so 60 images per second]. Nothing in the main
loop or the frame counter users divides the update to 30 Hz (counter uses are `& 1`, `& 0x10`, `& 0x20`
blink-style tests [I]); whether the fight logic inside `Mode_Battle` steps on every call is left to T14.

## 3. Modes and state machines

There is no task list or object scheduler at the top: a mode number selects one handler per frame.

| Global | Address | Meaning |
|---|---|---|
| mode | `D_005F5C20` (s16) | [V] index into `jtbl_004FB970`, 17 entries |
| sub-state | `D_005F5C22` (s16) | [V] each handler's own `switch`; reset to 0 by `Mode_Change` |
| next / previous mode | `D_005F6509` / `D_005F650A` (u8) | [V] written by `Mode_Change` |
| draw control | `D_005F6B00` (4 words) | [V] set by `Mode_SetDrawFlags` from the mode: +0 fighters to draw (0, 2 or 4), +4/+8/+0xC flags read by the draw passes |

`Mode_Dispatch` 0x3578F0 [V]: `func_003ADFA8()`, `switch (mode)` through `jtbl_004FB970`, then
`Mode_SetDrawFlags(mode)` (0x3576C0, its own table `jtbl_004FB950` for modes 4-11). `Mode_Change(next)`
0x3B90C0 [V] stores next and previous mode, sets the mode to 1 (or straight to `next` in some cases), sub-state
0.

| Mode | Handler | Identity [I unless noted] |
|---:|---|---|
| 0 | `Mode_Boot` 0x3B9628 | first mode after `Sys_Init` [V]; 7 sub-states (`jtbl_0051E9B0`) |
| 1 | `Mode_Transition` 0x3B9178 | the mode `Mode_Change` passes through [V]; fades/loads, then `next` |
| 2 | `Mode_Title` 0x3BCC88 | copyright strings ("ALL RIGHTS RESERVED") |
| 3 | `func_003BD2E0` | main menu ("START", "GALLERY", screenshot list `SCRN-%02d.CPT`) |
| 4 | `Mode_Battle` 0x35A6A0 | fight: 26 sub-states (`jtbl_004FBA30`), game-type switch on `D_0043B7F8[0]` (10 types, `jtbl_004FBAA0`); reaches "PAUSE", "PRACTICE", "K.O.", "ROUND %d", "GAME OVER", "A NEW CHALLENGER!" |
| 5 | `Mode_CharSelect` 0x3CB900 | "PLAYER", "VS", "ON TEAM", "MULTITAP(PS2) DISCONNECTED" |
| 6 | `func_003C8A50` | small (4 callees) |
| 7 | `func_003C8A88` | text-entry style screen (`%H%V%F%c%s`) |
| 8 | `func_003ABBD0` | "LOADING...", "REPLAY"; reaches the whole fight code |
| 9 | `Mode_Options` 0x3B4B20 | "OPTIONS MENU", "CONTROLLER SETUP", "RECORDS", "MEMORY CARD(PS2)" |
| 10 | `Mode_Results` 0x3B6258 | "TIME ATTACK RESULTS", "1 ON 1 RESULTS", "THE KING OF IRONBOWL" |
| 11 | `func_003BB310` | "LOADING..."; reaches the fight code |
| 12 | `func_00354BE0` | theater sub-mode |
| 13 | `Mode_Theater` 0x354D68 | "THEATER", "BGM VOLUME" |
| 14 | `func_00355D30` | one callee (`func_0039A140`) |
| 15 | `Mode_MusicPlayer` 0x355D90 | "MUSIC PLAYER", "PLAY", "STOP" |
| 16 | `Mode_Gallery` 0x3E5A98 | screenshot gallery ("VIEW", "DELETE", `SCRN-%02d.CPT`) |

Other dispatch tables [V for the table, I for the role]:

- `D_00507A80` (named `D_00507A88` before T14 b7, the address the code builds): 12-byte entries `{key, handlerA,
  handlerB}`, 33 of them searched by `func_0038BEE0` (callers 0x380568, 0x3809C0) in `fighter_core`: 35 handlers
  0x38AFC0-0x38BE78, per-character CPU move handlers, 28 of them `return -1` stubs [V, T14 b1].
- `D_005193B8`: 8-byte entries, 36 handlers 0x346BB8-0x347568, returned by `func_00347730` (`geometry`).
- `D_00520FC0` (records with 18 handlers 0x3D2160-0x3D6FD8, used by 0x3D0CC8/0x3D0E18/0x3D1040) and the arrays
  `D_0044B230` (9), `D_0044B800` (21), `D_0044BEA0` (4), `D_0044C1A0` (23) of handlers 0x3D4ED8-0x3D7C38, all in
  `model_anim`.
- Only 52 `jalr` in the whole executable: most dispatch is `switch` jump tables (`jtbl_*` in `.rodata`).

## 4. Module map (provisional)

Built from the call graph and the data each function uses: functions sharing file-local data (strings and
jump tables in `.rodata`, `.lit4` floats, `$gp` statics) were merged into one range, then neighbouring ranges
were grouped by the libraries they call, their strings and the modes that reach them. `.sbss` addresses
referenced grow with the code address over the whole game, so the ranges follow link (file) order. A module is
one or more C files; boundaries are [I] (about one function either way) unless the row says otherwise. These
are the starting point for the per-module C files. The rows `main`, `rng_misc`, `pad`, `file` and `gfx` are the
edges of their C files (T13): the build links them byte-identical, so the edges hold as object boundaries [V],
though an edge could still fall between two objects that are both inside the range.

| Module | Start | End | Funcs | Evidence |
|---|---|---|---:|---|
| `render_fighter_a` | 0x340000 | 0x3420C8 | 22 | sin/cos, VU0 matrices, fighter slots 0x879640; draw passes only |
| `vu1_packet` | 0x3420C8 | 0x342E78 | 12 | VIF1 packets with VU1 DIRECT code, `.vudata` 0x4FAF80-0x4FB800 |
| `dma_gif` | 0x342E78 | 0x346398 | 52 | `Dma_SendChain`, `Dma_SendNormal`, `Dma_Wait`, `Vu0_LoadMicrocode` [V]; GIF packets; reached by every mode. `src/dma_gif.c` (one file for the range; from about 0x3453F0 the functions set up objects and side tables and may be another object [I]) |
| `geometry` | 0x346398 | 0x34C508 | 53 | Inner/outer products, normalize, handler table `D_005193B8`; libvu0 (0x34A7A8-0x34B4E8) sits inside |
| `scene3d` | 0x34C508 | 0x3543F0 | 73 | Drawing state of the fighters (T17 b1): per-side wind, extra-joint pendulums and accessory parts (`src/scene3d.c`, 0x34C508-0x34E748, `.rodata` 0x51EA60-0x51EB00 and `.lit4` 0x5F5350-0x5F535C subsegments); the sword effect's packet senders (`src/scene3d_b.c`, 0x34FF10-0x3504C0), the sword trail and glow and character 20's swing (`src/scene3d_c.c`, 0x3504C0-0x351E98, `.lit4` 0x5F5614-0x5F5668), look-at, fighter lights `Light_*` and the GS image upload (`src/scene3d_d.c`, 0x351E98-0x3543F0, `.lit4` 0x5F5668-0x5F5700) [V]. Edges: 0x3504C0 is an object edge [V: the 128-bit save mode of `s1` ends there, decomp_guide.md "Lessons from T17 batch 1"]; 0x351E98 is a cut [I]; data order does not follow text order (its `.rodata` lies among `modes`' and `draw_list`'s, `func_00351AA0`'s `.lit4` before `func_00350C40`'s). Without 0x34E748-0x34FF10 (`skeleton`, next row). [systems/render.md](systems/render.md) |
| `skeleton` | 0x34E748 | 0x34FF10 | 16 | The fighter skeleton (T16 b1): `Skel_Update` (per frame, from `func_00371B18` in `Battle_UpdateFrame`), the root matrix that writes the position `unk51C`-`unk524`, joint world matrices (`Gte_CompMatrix`), body / push spheres and attack volumes on joints (`Skel_PlaceHitShapes`, `Skel_SetVolumeStarts`), screen positions of four joints read by the simulation (`Skel_ProjectJoints`) [V]. `src/skeleton.c`, a cut inside the `scene3d` range [I edges]; its `.lit4` (0x5F5114-0x5F5154) is a subsegment. Its `.sdata` (0x5F5E50-0x5F5E78), `.lit4` and `.rodata` (`D_004FC668`) lie between those of `fighter_setup`'s 0x3625B0-0x3645D8 and 0x366DB0- parts, so data order does not follow text order here [V addresses; I cause]. [systems/animation.md](systems/animation.md) |
| `theater` | 0x3543F0 | 0x356A70 | 24 | modes 12-15, "THEATER", "MUSIC PLAYER" |
| `draw_list` | 0x356A70 | 0x357358 | 15 | GIF packet end/terminate, FlushCache; used by the draw passes; crt0 follows |
| `main` | 0x357358 | 0x358A38 | 40 | `Sys_BootIop`, `Sys_Init`, `main`, frame phases, `Mode_Dispatch`, per-player input (`Input_*`) [V start: first function after crt0; its `.rodata` (IRX paths, two jump tables) is the first of the executable, 0x4FB880]. `src/main.c` |
| `battle_mode` | 0x358A38 | 0x35C2B0 | 67 | `Mode_Battle`, "GAME OVER", "A NEW CHALLENGER!" |
| `rng_misc` | 0x35C2B0 | 0x35D048 | 47 | `Rng_*` [V], small helpers (strings, character tables, play counters, file loads by number). `src/rng_misc.c` |
| `dma_spr` | 0x35D048 | 0x35E808 | 63 | file loads by number (0x35D048-0x35D458), scratchpad 0x70000000 setup (`Spr_Setup`), the scratchpad DMA state `gSprDma` shared with the DMAC handler (accessed `volatile`), toSPR / fromSPR waits; `sceOpen`/`sceClose` in one uncalled function 0x35D3C0. `src/dma_spr.c` [V end: the next three functions request memory card operations] |
| `memcard` | 0x35E808 | 0x3608B8 | 43 | `Mc_Request` (0x35E808) and its callers, `sceMc*` wrappers, `Mc_Update`, "BASLUS-20001TekkenTT"; state `gMc` (`McState`, include/memcard.h); no `$gp`. `src/memcard.c` |
| `font_tex` | 0x3608B8 | 0x3621F8 | 30 | VIF1 DIRECTHL image transfers, GIF tags; "YES NO" [I: font and texture upload] |
| `fighter_setup` | 0x3621F8 | 0x3682F8 | 140 | fighter matrices (0x8794C0...), init functions of `Sys_Init`. Without 0x3639F0-0x364448 (`motion`, next row) |
| `motion` | 0x3639F0 | 0x364448 | 19 | Motion data (T16 b1): `Motion_Sample` / `Motion_SampleRoot` / `Motion_SamplePose` (from `Skel_Update` and `Fighter_StartLaunch`), the delta / run-length frame decoder `Motion_DecodeFrame`, the motion banks (`Motion_LoadBanks`, clips by index and frame) [V]. `src/motion.c`, a cut inside `fighter_setup` [I edges]; `.rodata` 0x4FC610-0x4FC630 (the decoder's jump table) is its subsegment. [systems/animation.md](systems/animation.md) |
| `battle_flow` | 0x3682F8 | 0x3745E8 | 148 | The fight simulation (T14 b2): `Battle_UpdateFrame` 0x36D860 (one call per frame from `Mode_Battle`), input history and command matching, move start and the move step, hit resolution, round state and its texts ("ROUND %d", "K.O.", "READY!", "TIME UP") [V calls and data flow; I roles of the asm parts]. `src/battle_flow.c`, one C file for the range: the end is an object edge [V: the `.rodata` of the object ends with `D_00500758`, read by 0x374548, and `sound` starts at `Snd_Init`]; the start is [I] (no `.rodata` evidence: `jtbl_004FF8B0` of `fighter_setup` ends where `D_004FF8D0` of 0x369188 starts, and `D_004FF690` is read from both sides). Its `.rodata` (0x4FF8D0-0x500780: tables, round texts, jump tables) is included in place from the C file since T14 b3; its `.lit4` (0x5F5168, 0.6f and 1.4f) comes from `Fighter_ApplyDamage` (damage, C since T14 b4) [V]. [systems/fighter.md](systems/fighter.md) section 5 |
| `sound` | 0x3745E8 | 0x376C30 | 65 | `sceSdRemote` callers, `Snd_PlaySe` 0x374CD8 (56 callers, many of them menu screens), fighter voices (`Snd_PlayVoice`, voice sequences). `src/sound.c` [V start: its `.rodata` (0x500780-0x500D50, jump tables 16-aligned) cannot start at 0x500758, which belongs with the four functions 0x374520-0x3745E8 to the object before, the end of `battle_flow` (`src/battle_flow.c`)] |
| `fight_misc` | 0x376C30 | 0x3790C8 | 29 | The hit test of the fight (T14 b3): attack volumes against body spheres (`func_003770E0`, 64-bit division), body sphere radii, fighter push (`Fighter_Push`) and push sphere radii (`Fighter_SetPushRadii`), the special attack volume of character 0x1D (atan2f) [V]. `src/fight_misc.c`; `.rodata` 0x506150-0x506260 (tables read through `D_0042EFD8` in `.data`, three jump tables) and `.lit4` 0x5F5170 are its own [V]; the end is [I]: `func_003790C8` reads `.data` placed after the practice object's, so it starts `practice`. [systems/fighter.md](systems/fighter.md) section 5.5 |
| `practice` | 0x3790C8 | 0x37BA88 | 44 | "FREESTYLE", "VS CPU", "(CPU)", "(YOU)", "TYPE %d", "RANDOM" |
| `fighter_core` | 0x37BA88 | 0x38BF98 | 277 | practice mode support (command recorder `gPracRecorder`, command lists, dummy conditions, practice reset) 0x37BA88-0x3800D8 and the CPU opponent (`Cpu_GetInput`, `Cpu_Think`, candidate moves, handler table `D_00507A80`) 0x3800D8-0x38BF98 [V, T14 b1]; not the move interpreter or hit detection. `src/fighter_core.c`, one C file for the range, with the `.rodata` subsegment 0x5064C0-0x507EE0 (T14 b7). The `.rodata` order shows that the object before (`asm/3A0C8.s`, from 0x3790C8) runs past 0x37BF48 and ends before 0x37C370: its menu strings and tables (0x506528-0x506BF0) follow the constants of `func_0037BB70`-`func_0037BF48`, so 0x37BA88 is not an object edge [V order; I edges]. [systems/fighter.md](systems/fighter.md) |
| `camera_stage` | 0x38BF98 | 0x396D00 | 105 | The fight camera and its per-stage values (T15 b1, b2): `Cam_Update` steps the camera every simulated frame from `func_0036DDD0` and advances `gRoundPhase`; camera keys, blends and scripts; reaction cameras drawn with `rand()` and the CPU generator; stage camera distances `D_00507FE0`; `sceVu0ViewScreenMatrix` (`Cam_SetDistance`) [V]. `src/stage.c`, one C file for the range: both ends are object edges [V: the `.rodata` 0x507EE0-0x509D00 starts right after `fighter_core`'s and ends at `pad`'s, and the C file links byte-identical]; its `.lit4` (0x5F5174-0x5F519C) is a subsegment too (T15 b2). No stage geometry exists in the simulation. [systems/stage.md](systems/stage.md) |
| `pad` | 0x396D00 | 0x397948 | 11 | `scePad*`, `sceMtap*` [V block]; `.rodata` 0x509D00-0x509E38. `src/pad.c` |
| `file` | 0x397948 | 0x398510 | 29 | `File_*`, `Iop_*` [V block] (section 7); starts with the SIF command 0 handler; `.rodata` 0x509E40-0x50CE58 (file table, BGM tracks, two jump tables). `src/file.c` |
| `gfx` | 0x398510 | 0x3994C0 | 51 | `Gfx_*` [V block]; 0x398510-0x398858 are small helpers (stubs, fixed-point sqrt/sin, DMA packet table `D_004362A0`) whose `.data` lies between `pad`'s and `gfx`'s, so they open this object [I]. `src/gfx.c` |
| `zsort` | 0x3994C0 | 0x399FC8 | 13 | "(zs)list over" overflow `printf` + `exit` [I: z-sorted draw list] |
| `sound_if` | 0x399FC8 | 0x39A2B8 | 13 | byte command queue (`gSndQueue`, write pointer `gSndQueuePtr`, count `gSndQueueCount`) sent with `sceSdRemote` by `Snd_Flush`, `Bgm_*`; starts with the two empty stubs `Snd_Nop` (called before every libspu2 command) and 0x399FD0 [V]. `src/sound_if.c` |
| `gte` | 0x39A2B8 | 0x39BBF0 | 19 | fixed-point matrix and vector math in the style of the PlayStation GTE library (`Gte_*`, current matrix `gGteRt`, sine table `gSinTable`) [V]; the same code continues into `screenshot` (geometry offset and screen, perspective transforms from 0x39BBF0) [I: end]. `src/gte.c` |
| `screenshot` | 0x39BBF0 | 0x39C7B8 | 20 | `sceGsExecStoreImage` (`Scrn_Capture`) |
| `movie` | 0x39C7B8 | 0x3A04B8 | 111 | `sceMpeg*`, IPU, a thread and an INTC handler (0x39CDC0), semaphore ring buffer 0x39F138-0x39FE00 [V for the calls] |
| `effects` | 0x3A04B8 | 0x3A6460 | 58 | The effect callbacks (T17 b2): animated bank sprites, stage ambient effects and lens flare, hit sparks, flashes and bursts, the hit log (`HitLog_*`, replayed by `Battle_UpdateFrame`), body-part glows, move effects of characters 0x14 / 0x21 and move kind 0x19 [V]. `src/effects.c`; its `.rodata` (0x5194E0-0x519940, ending with `D_00519918` read by `effects_draw`) and `.lit4` (0x5F51A4-0x5F5300) are subsegments. Edges [I]: the start is the module edge; the data before (`D_005193B8`, `D_005F51A0`) belongs to `geometry`'s 0x347xxx functions, so data order does not follow text order here either. [systems/render.md](systems/render.md) section 7 |
| `effects_draw` | 0x3A6460 | 0x3A9678 | 50 | Screen fade (`Fade_*`), title logo, stage texture layers (`Fx_*StageLayers`) and the background effects of stages 4 / 12, fighter texture channels (`Fx_*FighterTex`), the textured quad queue (`Fx_*Quad*`), character 0x1D's special attack volume, helpers (`Fx_RandFloat`, `Fx_GetBankEntry`) [V]. `src/effects_draw.c`, a cut, not an object edge at 0x3A6460 [V: its `.rodata` (0x519940-0x519B30) and `.lit4` (0x5F5300-0x5F531C) follow `effects`' without a gap, and `D_00519918` (8-aligned, read by 0x3A6A90) sits where an object with jump tables could not start its `.rodata`; an edge between 0x3A6A90 and 0x3A7198 is not ruled out]. Both files look built without `-ffast-math` (decomp_guide.md, "Lessons from T17 batch 2") |
| `loading_title` | 0x3A9678 | 0x3ADFA8 | 83 | mode 8, "LOADING...", "REPLAY", "TEKKEN TAG TOURNAMENT", "STAGE %d" |
| `menu_common` | 0x3ADFA8 | 0x3B1478 | 82 | 3D menu helpers (rotations), shared by modes 2-11 |
| `options` | 0x3B1478 | 0x3B4D50 | 38 | `Mode_Options`, `scePadInfoAct` (vibration test) |
| `pause_results` | 0x3B4D50 | 0x3B9178 | 75 | `Mode_Results`, "PAUSE P%d", "RESET?", "CAPTURE?", "FIGHT" |
| `modes` | 0x3B9178 | 0x3BDF60 | 85 | `Mode_Change`, `Mode_Transition`, `Mode_Boot`, `Mode_Title`, mode 3, mode 11, "CONTINUE? %d" |
| `select_results` | 0x3BDF60 | 0x3CD298 | 189 | `Mode_CharSelect`, modes 6-7, "WINS %d", ranking ("ST/ND/RD/TH"), `Text_Printf` 0x3C6C60. Also the team members' life at round start (`Battle_InitTeamLife`) and its per-frame recovery (`Battle_RecoverLife`): `src/select_results.c` covers only 0x3C1470-0x3C1A78 (T14 b4), a cut between two functions, not an object edge (the part emits no data) [systems/fighter.md](systems/fighter.md) section 5.6 | Without 0x3C1A78-0x3C3150 (`hud`, next row).
| `hud` | 0x3C1A78 | 0x3C3150 | 12 | The fight HUD (T17 b1): `Hud_Draw` once per update from battle_flow's `func_0036EA80`: life bars with red life and damage trail, the partner's bar and the tag slide, round markers, the timer (70 frames per displayed second), name plates; the K.O. percentages `Hud_DrawKoPercent`, the round set-up `Hud_InitRound`, the partner counter `TeamMember.unk28` (`func_003C1A78` on a tag, `func_003C1B38` per frame) [V]. `src/hud.c`, a cut inside `select_results`, not an object edge [V: its `.rodata` strings follow `func_003C1350`'s jump table in text order]; draws through `sprite2d`. [systems/render.md](systems/render.md) section 5 |
| `sprite2d` | 0x3CD298 | 0x3D0660 | 59 | GIF sprite packets, reached by all modes |
| `model_anim` | 0x3D0660 | 0x3DB758 | 173 | heaviest libvu0 user (matrices, rotations); handler tables 0x44B230-0x44C1A0, 0x520FC0 [I: models, animation]; the fighter skeleton itself is the `skeleton` module, and the functions the simulation calls here are tag counters and move bookkeeping (`func_003D3228`, `func_003D2A38`, `func_003D29E8`, [systems/animation.md](systems/animation.md) section 4). Also the tag swap `Fighter_TagSwap` (0x3D3288): `src/model_anim.c` covers only 0x3D3288-0x3D33F0 (T14 b4), a cut between two functions, not an object edge [systems/fighter.md](systems/fighter.md) section 5.7 |
| `model_draw` | 0x3DB758 | 0x3DC9C8 | 18 | No models and no VU1 code (T17 b1): 2D GS primitives written into the GIF buffers of `gfx` (scissor, rectangles, gradient quad, the menu window `Ui_DrawWindow`, 18 callers in every mode), the stage-object draw hooks of the stage pass, text helpers (`Str_Next`, `Str_SkipN`), two per-slot bank helpers handed to `demo` [V]. `src/model_draw.c`, one C file, no data of its own; the end may lie a little earlier (the last two functions serve `demo`) [I]. [systems/render.md](systems/render.md) section 6 |
| `demo` | 0x3DC9C8 | 0x3E1C68 | 51 | "DEMO", VIF1 DIRECT code |
| `menu_bg` | 0x3E1C68 | 0x3E55C8 | 21 | flavour texts ("TARGET", "POWER", "ANGLE", "BOSKONOVITCH LABO LTD"); used by menus, music player, gallery, bowl; calls `srand` |
| `gallery_jpeg` | 0x3E55C8 | 0x3E8AD8 | 32 | `Mode_Gallery`, "JFIF" [I: screenshot JPEG codec] |
| `tekken_bowl` | 0x3E8AD8 | 0x3F3320 | 136 | "THE KING OF IRONBOWL", "SPEED", "NICE COVER", "BONUS AT 200 PTS", "JUKEBOX" |

46 modules cover the 2815 game functions. Sizes are uneven on purpose: `fighter_core`, `select_results`,
`model_anim` and `fighter_setup` are several files each; split them when they are decompiled (T13-T18).

## 5. Important globals

| Address | Proposed name | What | Status |
|---|---|---|---|
| 0x5F5C18 (u16) | `gFrameCount` | +1 per main-loop iteration (`Frame_PreUpdate`), 0 at boot; 50+ readers | V |
| gGfx + 0x254 | `gVSyncCount` | +1 per VBLANK_START interrupt (`Gfx_GetVSyncCount`) | V |
| 0x8BD0A0 | `gGfx` | `sceGsDBuff` + display state; +0x250 field bit, +0x25C buffer parity, +0x264/0x26C-0x26F state bytes | V layout of the fields named; roles I |
| 0x5F6C70 | `gFrameReady` | set by `Gfx_SetFrameReady`, cleared by the VBlank handler | V |
| 0x5F5C20 / 0x5F5C22 | `gMode` / `gSubMode` | mode and sub-state (section 3) | V |
| 0x5F6B00 | `gDrawCtl` | draw-pass control, 4 words | V |
| 0x879640 | `gFighters` | 4 slots of 0xB8A0 bytes (0x879640, 0x884EE0, 0x890780, 0x89C020) | V stride and count; "fighter" I |
| 0x43B7F8 | `gGameSetup` | +0 game type (10 values), +0x0F, +0x12, +0x13 bytes; read by 30+ functions | I |
| 0x5F5C08 / 0x5F5C0C | `gFrameT0` / `gFrameOverrun` | Timer 0 at the end of the frame / `>= 0x246E` | V |
| 0x8BCC80 | `gPadPorts` | per-port pad state, 0x24-byte stride (`Pad_Update`) | V stride; content I |
| 0x5F6C5C | `gMultitap` | selects the player-to-port table 0x509D00 (+0 without, +8 with multitap) | I |
| 0x415778 / 0x4157B8 / 0x4157C8 | `gPlayerInput*` | per-player outputs of `Pad_GetPlayerInput`: 4 words, 4 bytes (reset to 0x80: analog sticks), 12 bytes (pressure) per player | I |
| 0x5F5CB6 (u16) | | stage index: the 23-entry byte tables 0x455D50 / 0x455D68 read every frame, stage loading, music, camera values `D_00507FE0` (25 entries); read by 60+ functions; no gameplay reader ([systems/stage.md](systems/stage.md)) | V for the stage use; I for completeness |
| 0x509E40 | `gFileTable` | 748 file entries (section 7) | V |
| 0x8BCD40 / 0x8BCF80 | `gFileReqs` / `gFileQueue` | 24 requests of 0x18 bytes / 24 queue pointers, +0x60 busy word | V |
| 0x5F6C64 / 0x5F6C60 | | pending request count / IOP status word (`sceSdRemote(1, 0x2030)`) | V |
| 0x5F61F0 / 0x5F61EC | | sound command buffer write pointer / count | V |
| 0x5F5F2C | `gBattleParity` | 0/1, flipped every simulated frame; picks which fighter steps first (systems/fighter.md section 5) | V |
| 0x5F5EF0 | `gRoundPhase` | round and camera phase: `Battle_UpdateRound` sets 0xE-0x12 at the round end, the camera step `Cam_Update` switches on it and advances it (1 to 0xD, 0xE / 0xF to 0x10); `Battle_ApplyVelocity` moves the fighters only below 0xE ([systems/stage.md](systems/stage.md) section 4.1) | V |
| 0x5F5F04 | `gRoundNumber` | 1 on the first round (intro poses) | I |
| 0x5F5C30 | `gRoundTimer` | the round ends when it reaches 0 or a life (`Fighter.unk40C`) does (`Battle_IsRoundOver`) | I |
| 0x5F5C28 | | distance between the two fighters, written by `Battle_UpdateDistance` (name not applied: `src/fighter_core.c` reads it) | I |
| RNG state | | see section 6 | V |

## 6. Random numbers

Two generators [V]:

1. **newlib `rand`** 0x40FBD0: `next = next * 0x41C64E6D + 0x3039`, returns `next & 0x7FFFFFFF`; state is the
   word at `_impure_ptr` (0x4F5A8C) + 0x58. 100 game functions call it: 46 in `fighter_core`, 13 in `effects`,
   7 in `camera_stage`, the rest spread over 15 modules.
   - `Rng_Init` 0x35C3C8 (from `Sys_Init`): seed = Timer 0 count at that moment; `Rng_SetSeed(seed)` (0x439C7C),
     `srand(seed)`, copy in `D_005F5CF4`.
   - `Rng_Reseed` 0x35C370 (every frame, from `Frame_PreUpdate`): when `D_005F5CDC` is non-zero,
     `Rng_SetSeed(Rng_GetSeed() + vsync_count)` then `srand(Rng_GetSeed())`. `D_005F5CDC` is used by 12
     functions, among them the theater, music player, options, results and gallery handlers [I: set outside
     fights].
   - `srand` is also called with values derived from data by 0x3E2A08 and 0x3E2E50 (`menu_bg`).
2. **Spin LCG** `Rng_SpinNext` 0x35C678: `x = x * 5 + 1` in `D_005F6B34`, advanced on every iteration of the
   wait loop in `Gfx_WaitVSync`, so its value depends on how long the CPU waited. Read only by
   `Rng_PickRandomEntry` 0x35C7A8 (picks the n-th valid entry among 34 with `x & 0xFFF` modulo the count) [I:
   random character choice].

For a deterministic port and replays: the boot seed (Timer 0) and the spin LCG depend on real time; the
reseed adds the VBlank count. All three must become inputs or saved state (T26-T27).

The effects (`src/effects.c`, `src/effects_draw.c`, T17 b2) draw from `rand()` only (not `func_003800D8` or the spin
LCG): character 0x1D's special attack volume and stage-coloured hit sparks inside the simulation step, and many effect
callbacks every frame from the effect update (stage ambient effects, stage 4 / 12 backgrounds, hit particles, body-part
glows, partly even while paused), so the CPU's and the camera's random numbers depend on them
([systems/render.md](systems/render.md) section 7.1).

The fight camera (`src/stage.c`, T15 b1) draws from `rand()` and from the CPU's generator `func_003800D8` when moves,
hits and throws start reaction cameras, and the special ending advances a third LCG, `D_005F5CF4 = (D_005F5CF4 + 1) *
0x10DCD` (seeded by `Rng_Init`); the camera is part of the synchronised state ([systems/stage.md](systems/stage.md)
section 5).

Inside the fight simulation (T14 b2, [systems/fighter.md](systems/fighter.md) section 5): `rand()` picks the intro
pose of a fighter (`func_00368398`), and `gFrameCount` sets the initial `gBattleParity` (`func_0036E138`) and the
intro of character 0x21 (`gFrameCount % 3`, `func_003685B8`). `gFrameCount` counts main-loop iterations, so it
must be part of the synchronised state too.

## 7. File loading

Archive format, table fields and the extractor: [systems/files.md](systems/files.md).

The game never calls `sceCdRead`, `sceCdSearchFile` or `sceCdSt*` [V: no callers; only `sceCdInit` and
`sceCdMmode` in `Sys_BootIop`], and the name `TEKKEN.BIN` is not in the executable. Files are read on the IOP
side through RSPU2DRV commands sent with `sceSdRemote` [V]; the protocol is in [systems/audio.md](systems/audio.md).

- **File table** `D_00509E40` [V]: 748 entries of 16 bytes, `{u32 sector, u32 size + 0x350A, u32 check,
  u32 id or 0xFFFF}`, indexed by file id. The IOP reads from `TEKKEN.BIN`'s own LSN + `sector - 0x4820`
  (RSPU2DRV finds the LSN with `sceCdSearchFile`) [V]. Details in [systems/files.md](systems/files.md).
- **Requests** [V]: `File_Request(id, dest, flags, toIop)` 0x397E78 (exits if `dest` is not 16-byte aligned;
  `dest` is turned into an uncached address unless `toIop`; id 0xFFF queues a marker that reads nothing) takes a
  free slot (`File_AllocRequest`; state 1 = free), queues it with state 2 (3 for id 0xFFF) and counts it in
  `D_005F6C64`. `File_Update` 0x398100, every frame from `Frame_End`, reads the IOP status
  (`sceSdRemote(1, 0x2030)`), runs `File_SendSifCmd` (below), then steps the head request (`jtbl_0050CE40`):
  state 2 issues `sceSdRemote(1, 0x204E, sector, dest, size, flags)` (0x2045 when `toIop`) and goes to 4;
  state 4 waits until bit 0 of the IOP status clears, descrambles, and goes to 6 (done); state 3 goes to 5
  without a read. `File_Release` 0x397FB0 frees a slot, aborting a running read with
  `sceSdRemote(1, 0x2050)`. `File_WaitAll` 0x397C70 loops on `sceGsSyncV` + `File_Update` until nothing is
  pending, then frees every slot. (`toIop` selects 0x2045, which reads into SPU2 RAM: `dest` is then an SPU2 address [V, T11];
  it also skips the uncached-address conversion and the descrambling.)
- **IOP-requested uploads** [V]: SIF command 1 from the IOP runs `File_SifCmdHandler` (sets `D_005F61D0`); the
  next `File_SendSifCmd` sends `sceSifSendCmd(0, D_008BD090, 0x10, src = D_005F61D4 + w0 * 0x800, dest = w1,
  size = w2 * 0x800)` with the three words at 0x8BD080, and exits if the source base `D_005F61D4` is 0 [V, T11:
  BGM tracks of type 3 streamed from EE memory, see systems/audio.md section 5.2].
- **Descrambling** [V]: after a read to EE memory (not `toIop`) and for every id except 0x2C, the first
  `min(size, 0x800)` bytes are XORed word by word with a key stream: `key = File_KeySeed(id)` =
  `(id + 0xF3C) * 218319 + (id - 0x348F) * 0x1234567` (64-bit), then `key = key * 0x71C3AB9 + 0x3941` per word
  (`File_KeyNext`, 64-bit as well), each word XORed with the low 32 bits of the key.
- `File_RegisterIopFiles` 0x397A00 (from `File_Init`) sends 28 `sceSdRemote(1, 0x205E, ...)` commands built from
  the 10-byte records at 0x50CD00 and the file table, one with `sector + 0xCF7` and `size - 0x67B800` (that is,
  skipping 0xCF7 sectors): the BGM track table of the IOP stream engine [V, T11].

## 8. VU microcode

`.vudata` is at 0x4F5F00-0x4FB880 [V]. VU1 microcode 0x4F7A00 is uploaded once by `Gfx_Init`. VU0 microcode is
uploaded by `Gfx_Init` (0x4F5F00) and again every frame by `Vu0_LoadMicrocode` from 0x4F5F00, 0x4F6C80 or
0x4FA7E0 depending on the mode. 0x4FAF80-0x4FB800 holds VIF code blocks used by `vu1_packet` (`model_draw` does not read them, T17 b1).
Listings and boundaries: T10.

## Open questions

- Runtime confirmation of the 59.94 Hz loop and of the 640x224 field-rendering setup [PCSX2].
- RSPU2DRV commands: answered in [systems/audio.md](systems/audio.md) (T11); its open questions are listed there.
- Identity of modes 3, 6, 7, 11, 12, 14 and of most functions in `fighter_core`, `scene3d`, `model_anim`.
