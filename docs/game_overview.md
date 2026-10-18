# Game code overview

Top-level structure of the game's own code in `SLUS_200.01` (0x340000-0x3F3320 minus libvu0 and crt0, see
[text_map.md](text_map.md)): boot, main loop and frame pacing, mode dispatch, a provisional module map, the
important globals, the random number generators and the file loader. Function names are the ones in
`config/symbol_addrs.txt`; `func_XXXXXXXX` and `D_XXXXXXXX` are still unnamed.

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
6. `Mc_Init` 0x360410 (`sceMcInit`), `func_00361B18`, `func_00399FD8` (sound: `sceSdRemote`), `File_Init`
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
- `Frame_End` 0x357BF0 [V]: `func_00374A88` (sound), `File_Update`, `Snd_Flush`, `Mc_Update`, `func_0035DF38`,
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

- `D_00507A88`: 12-byte entries `{key, handlerA, handlerB}` searched by `func_0038BEE0` (callers 0x380568,
  0x3809C0) in `fighter_core`: 35 handlers 0x38AFC0-0x38BE78 [I: move/command handlers].
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
are the starting point for the per-module C files.

| Module | Start | End | Funcs | Evidence |
|---|---|---|---:|---|
| `render_fighter_a` | 0x340000 | 0x3420C8 | 22 | sin/cos, VU0 matrices, fighter slots 0x879640; draw passes only |
| `vu1_packet` | 0x3420C8 | 0x342E78 | 12 | VIF1 packets with VU1 DIRECT code, `.vudata` 0x4FAF80-0x4FB800 |
| `dma_gif` | 0x342E78 | 0x346398 | 52 | `Dma_SendChain`, `Dma_Wait`, `Vu0_LoadMicrocode` [V]; GIF packets; reached by every mode |
| `geometry` | 0x346398 | 0x34C508 | 53 | Inner/outer products, normalize, handler table `D_005193B8`; libvu0 (0x34A7A8-0x34B4E8) sits inside |
| `scene3d` | 0x34C508 | 0x3543F0 | 89 | projections (RotTransPersN), matrices; modes 4, 5, 8, 11 |
| `theater` | 0x3543F0 | 0x356A70 | 24 | modes 12-15, "THEATER", "MUSIC PLAYER" |
| `draw_list` | 0x356A70 | 0x357358 | 15 | GIF packet end/terminate, FlushCache; used by the draw passes; crt0 follows |
| `main` | 0x357358 | 0x358A38 | 40 | `Sys_BootIop`, `Sys_Init`, `main`, frame phases, `Mode_Dispatch` [V start: first function after crt0] |
| `battle_mode` | 0x358A38 | 0x35C2B0 | 67 | `Mode_Battle`, "GAME OVER", "A NEW CHALLENGER!" |
| `rng_misc` | 0x35C2B0 | 0x35D048 | 47 | `Rng_*` [V], small helpers |
| `dma_spr` | 0x35D048 | 0x35E878 | 66 | scratchpad 0x70000000 setup (`Spr_Setup`), DMAC handler, toSPR waits; `sceOpen`/`sceClose` in one uncalled function 0x35D3C0; no `$gp` |
| `memcard` | 0x35E878 | 0x3608B8 | 40 | `sceMc*` wrappers, `Mc_Update`, "BASLUS-20001TekkenTT"; no `$gp` |
| `font_tex` | 0x3608B8 | 0x3621F8 | 30 | VIF1 DIRECTHL image transfers, GIF tags; "YES NO" [I: font and texture upload] |
| `fighter_setup` | 0x3621F8 | 0x3682F8 | 159 | fighter matrices (0x8794C0...), init functions of `Sys_Init` |
| `battle_flow` | 0x3682F8 | 0x374520 | 144 | "ROUND %d", "K.O.", "READY!", "TAG BATTLE", "DRAW" [I: round flow and fight HUD] |
| `sound` | 0x374520 | 0x376C30 | 69 | `sceSdRemote` callers, `Snd_PlaySe` 0x374CD8 (56 callers, many of them menu screens) |
| `fight_misc` | 0x376C30 | 0x3790D8 | 30 | 64-bit division, atan2; modes 4, 5, 8, 11 |
| `practice` | 0x3790D8 | 0x37BA88 | 43 | "FREESTYLE", "VS CPU", "(CPU)", "(YOU)", "TYPE %d", "RANDOM" |
| `fighter_core` | 0x37BA88 | 0x38BF98 | 277 | largest `rand` user, no strings, handler table `D_00507A88`; reached only by modes 4, 5, 8, 11 [I: move interpreter, hits, AI] |
| `camera_stage` | 0x38BF98 | 0x396D00 | 105 | `sceVu0ViewScreenMatrix` (0x38DA28), camera/stage tables 0x433330 [I] |
| `pad` | 0x396D00 | 0x397958 | 12 | `scePad*`, `sceMtap*` [V block] |
| `file` | 0x397958 | 0x398940 | 56 | `File_*` [V block] (section 7) |
| `gfx` | 0x398940 | 0x3994C0 | 23 | `Gfx_*` [V block] |
| `zsort` | 0x3994C0 | 0x399FD8 | 15 | "(zs)list over" overflow `printf` + `exit` [I: z-sorted draw list] |
| `sound_if` | 0x399FD8 | 0x39BBF0 | 30 | byte command queue (`D_005F61F0` write pointer, `D_005F61EC` count) sent with `sceSdRemote` by `Snd_Flush` [V] |
| `screenshot` | 0x39BBF0 | 0x39C7B8 | 20 | `sceGsExecStoreImage` (`Scrn_Capture`) |
| `movie` | 0x39C7B8 | 0x3A04B8 | 111 | `sceMpeg*`, IPU, a thread and an INTC handler (0x39CDC0), semaphore ring buffer 0x39F138-0x39FE00 [V for the calls] |
| `effects` | 0x3A04B8 | 0x3A6460 | 58 | rand, sin/cos, projections; fight modes |
| `effects_draw` | 0x3A6460 | 0x3A9678 | 50 | GIF packets, RotTransPers |
| `loading_title` | 0x3A9678 | 0x3ADFA8 | 83 | mode 8, "LOADING...", "REPLAY", "TEKKEN TAG TOURNAMENT", "STAGE %d" |
| `menu_common` | 0x3ADFA8 | 0x3B1478 | 82 | 3D menu helpers (rotations), shared by modes 2-11 |
| `options` | 0x3B1478 | 0x3B4D50 | 38 | `Mode_Options`, `scePadInfoAct` (vibration test) |
| `pause_results` | 0x3B4D50 | 0x3B9178 | 75 | `Mode_Results`, "PAUSE P%d", "RESET?", "CAPTURE?", "FIGHT" |
| `modes` | 0x3B9178 | 0x3BDF60 | 85 | `Mode_Change`, `Mode_Transition`, `Mode_Boot`, `Mode_Title`, mode 3, mode 11, "CONTINUE? %d" |
| `select_results` | 0x3BDF60 | 0x3CD298 | 201 | `Mode_CharSelect`, modes 6-7, "WINS %d", ranking ("ST/ND/RD/TH"), `Text_Printf` 0x3C6C60 |
| `sprite2d` | 0x3CD298 | 0x3D0660 | 59 | GIF sprite packets, reached by all modes |
| `model_anim` | 0x3D0660 | 0x3DB758 | 173 | heaviest libvu0 user (matrices, rotations); handler tables 0x44B230-0x44C1A0, 0x520FC0 [I: models, skeletons, animation] |
| `model_draw` | 0x3DB758 | 0x3DC9C8 | 18 | VU1 DIRECT code packets (`.vudata` 0x4FAF80-0x4FB800, shared with `vu1_packet`) |
| `demo` | 0x3DC9C8 | 0x3E1C68 | 51 | "DEMO", VIF1 DIRECT code |
| `menu_bg` | 0x3E1C68 | 0x3E55C8 | 21 | flavour texts ("TARGET", "POWER", "ANGLE", "BOSKONOVITCH LABO LTD"); used by menus, music player, gallery, bowl; calls `srand` |
| `gallery_jpeg` | 0x3E55C8 | 0x3E8AD8 | 32 | `Mode_Gallery`, "JFIF" [I: screenshot JPEG codec] |
| `tekken_bowl` | 0x3E8AD8 | 0x3F3320 | 136 | "THE KING OF IRONBOWL", "SPEED", "NICE COVER", "BONUS AT 200 PTS", "JUKEBOX" |

42 modules cover the 2815 game functions. Sizes are uneven on purpose: `fighter_core`, `select_results`,
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
| 0x5F5CB6 (u16) | | index of the 23-entry byte tables 0x455D50 / 0x455D68 read every frame; read by 60+ functions [I: stage] | I |
| 0x509E40 | `gFileTable` | 748 file entries (section 7) | V |
| 0x8BCD40 / 0x8BCF80 | `gFileReqs` / `gFileQueue` | 24 requests of 0x18 bytes / 24 queue pointers, +0x60 busy word | V |
| 0x5F6C64 / 0x5F6C60 | | pending request count / IOP status word (`sceSdRemote(1, 0x2030)`) | V |
| 0x5F61F0 / 0x5F61EC | | sound command buffer write pointer / count | V |
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

## 7. File loading

Archive format, table fields and the extractor: [systems/files.md](systems/files.md).

The game never calls `sceCdRead`, `sceCdSearchFile` or `sceCdSt*` [V: no callers; only `sceCdInit` and
`sceCdMmode` in `Sys_BootIop`], and the name `TEKKEN.BIN` is not in the executable. Files are read on the IOP
side through RSPU2DRV commands sent with `sceSdRemote` [V for the EE side; the IOP side is T11].

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
  pending, then frees every slot. (`toIop` is the fourth argument's role [I]: it selects 0x2045, skips the
  uncached-address conversion and the descrambling.)
- **IOP-requested uploads** [V]: SIF command 1 from the IOP runs `File_SifCmdHandler` (sets `D_005F61D0`); the
  next `File_SendSifCmd` sends `sceSifSendCmd(0, D_008BD090, 0x10, src = D_005F61D4 + w0 * 0x800, dest = w1,
  size = w2 * 0x800)` with the three words at 0x8BD080, and exits if the source base `D_005F61D4` is 0 [I: EE
  data (sound) pulled by the IOP].
- **Descrambling** [V]: after a read to EE memory (not `toIop`) and for every id except 0x2C, the first
  `min(size, 0x800)` bytes are XORed word by word with a key stream: `key = File_KeySeed(id)` =
  `(id + 0xF3C) * 218319 + (id - 0x348F) * 0x1234567` (64-bit), then `key = key * 0x71C3AB9 + 0x3941` per word
  (`File_KeyNext`, 64-bit as well), each word XORed with the low 32 bits of the key.
- `File_RegisterIopFiles` 0x397A00 (from `File_Init`) sends 28 `sceSdRemote(1, 0x205E, ...)` commands built from
  the 10-byte records at 0x50CD00 and the file table, one with `sector + 0xCF7` and `size - 0x67B800` (that is,
  skipping 0xCF7 sectors) [I: streamed sound/movie files the IOP reads by itself].

## 8. VU microcode

`.vudata` is at 0x4F5F00-0x4FB880 [V]. VU1 microcode 0x4F7A00 is uploaded once by `Gfx_Init`. VU0 microcode is
uploaded by `Gfx_Init` (0x4F5F00) and again every frame by `Vu0_LoadMicrocode` from 0x4F5F00, 0x4F6C80 or
0x4FA7E0 depending on the mode. 0x4FAF80-0x4FB800 holds VIF code blocks used by `vu1_packet` and `model_draw`.
Listings and boundaries: T10.

## Open questions

- Runtime confirmation of the 59.94 Hz loop and of the 640x224 field-rendering setup [PCSX2].
- What the IOP does for each `sceSdRemote` command number (0x2030, 0x2045, 0x204E, 0x2050, 0x2051, 0x205E):
  T11.
- Identity of modes 3, 6, 7, 11, 12, 14 and of most functions in `fighter_core`, `scene3d`, `model_anim`.
