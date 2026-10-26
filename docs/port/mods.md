# PC port: mods (custom music and stage textures)

T49. Mods are folders you drop into the mods folder. A mod can replace the game's music tracks and its textures (stage
textures included). Mods change what you **hear and see**, never what the game computes: the game's state, its state
hash, rollback and netplay are the same with or without them ([Determinism and netplay](#determinism-and-netplay)).

Code: `port/src/plat_mods.c` (the folder, the load order, the music), `port/src/mods_vorbis.cpp` (Ogg Vorbis through
`third_party/stb/stb_vorbis.c`, v1.22, public domain or MIT: `third_party/stb/LICENSE`), the mods' part of
`port/src/gs_texpack.c` (textures) and the Mods page of the F1 overlay (`port/src/overlay.cpp`).

## The mods folder

```
<mods>/<mod name>/07.ogg                  music: BGM track 7
<mods>/<mod name>/music/21 - stage.wav    music may also sit in music/
<mods>/<mod name>/music/21 - stage.loop   optional loop points of that file
<mods>/<mod name>/textures/**/<name>.png  replacement textures (texture-pack names), any sub-folder
```

`<mods>` is `mods.dir` (`TTT_MODS`) when set, else `mods/` in the player's data folder ([install.md](install.md)):
- beside the program when it is portable (`portable.txt`),
- the repository's `mods/` for a checkout's `build/port/ttt` (ignored by git),
- else `$XDG_DATA_HOME/ttt/mods` (`~/.local/share/ttt/mods`).

Every folder in `<mods>` is one mod, named by its folder. A quick music-only setup is a single mod called `music`:
`<mods>/music/07.ogg`. Folder names with a comma are skipped (the settings' lists use commas).

## Load order and conflicts

The mods are listed in their load order: first the names in `mods.order`, in that order, then the others by name.
The **first enabled mod wins** a conflict: when two mods have a file for the same track, or the same texture name, the
one higher in the list is used and the other's file is hidden. A mod is enabled unless its name is in `mods.disabled`.
Mod textures also win over the texture pack's `replacements/` ([textures.md](textures.md)).

The overlay (F1, **Mods**) shows the list:
- a check box per mod (on / off) and Up / Down (the order),
- the tracks each mod replaces and, in orange, the tracks an earlier mod hides,
- the number of texture files of each mod and how many of them an earlier mod (or the pack) hides,
- the track playing now and the file replacing it,
- "Read the folder again" (new mods without a restart),
- the switches "Music mods" (`mods.music`) and "Texture mods" (`mods.textures`).

Changes are written to the settings file at once (`mods.order` lists every mod in the new order, `mods.disabled` the
switched-off ones). Music follows at once: the track playing switches to the new winner at the time the track has
reached. Textures follow at the next start (the folders are read once a run, as for texture packs).

| Setting | Variable | Default | |
|---|---|---|---|
| `mods.music` | `TTT_MOD_MUSIC` | 1 | Music mods on / off. |
| `mods.textures` | `TTT_MOD_TEXTURES` | 1 | Texture mods on / off (OpenGL; read at start). |
| `mods.dir` | `TTT_MODS` | (empty) | The mods folder; read at start. |
| `mods.order` | `TTT_MOD_ORDER` | (empty) | Mod names, commas between them. |
| `mods.disabled` | `TTT_MOD_DISABLED` | (empty) | Mod names, commas between them. |

## Music

### Files

- **Name:** the track number (0-27, one or two digits) first, then anything, then `.wav` or `.ogg` (any case):
  `7.wav`, `07.ogg`, `21 - stage.ogg`. When one mod has several files for a track, the one whose name sorts first is
  used.
- **WAV:** PCM, 8, 16, 24 or 32-bit integers, mono or stereo (beyond two channels, the first two), any rate from
  4 kHz to 192 kHz. Float and compressed WAVs are refused (`ttt: mods: <file>: not read ...`).
- **Ogg Vorbis:** any rate and channel count (mono plays on both sides).
- **Length:** up to about 10 minutes (30 million frames); longer files are cut there.
- Files are decoded whole into memory and resampled to the output's 48 kHz by linear interpolation. The step is exact
  (whole frames plus a remainder in 1/48000 of a frame), so a track never drifts against the game.

### Loops

In this order:
1. A sidecar text file with the same name and `.loop` (`21 - stage.loop` for `21 - stage.ogg`): `start end` in the
   file's own frames (samples per channel), end excluded (0 or no end: the end of the file). `once` (or `none`, `off`)
   plays the file once. `#` starts a comment.
2. A WAV `smpl` chunk's first loop (start, end included, as the format has it).
3. Ogg comments `LOOPSTART` and `LOOPLENGTH` (or `LOOPEND`), in frames, the usual convention of game-music files.
4. Otherwise the whole file loops when the game's track loops, and plays once when the game's track plays once.

The file plays from its start whenever the game starts the track. When the game's track ends or stops, the file
stops with it; a file shorter than a non-looping track leaves silence after its end.

### Volume and fades

The file plays at the game's music volume as the game sets it: the option volume (the game's own BGM option) times
the fade, then the SPU2's core 0 master, core 1 AVOL and core 1 master volumes the stream voices go through (all at
full in this game). The game's per-track volume scale (0x2000-0x3CCC) is not applied: a file is played at its own level,
so a file normalised like ordinary music sounds a little louder than the original. In mono (the game's sound option)
both sides get `(left + right) * 0.65`, as the game does for its stream. The port's own volumes (`audio.music`,
`audio.volume`, mute) apply on top, as for the game's music ([sound.md](sound.md) section 4.1).

### How it is played

The game keeps playing its own stream: the BGM engine (`plat_bgm.c`), the stream voices 22-23 of core 0, the status
bits the game reads and the timing of everything stay as without mods. After each VBlank's mix `plat_snd.c` hands
the SPU2 model's two buses (the whole mix and its music part) to `Mods_MusicMix`. While the engine plays table track
n (state 4) and an enabled mod has a file for n, the music part is subtracted from the mix and the file's samples are
added instead. Only the window's sound and `TTT_WAV` change. The movie track (code 0x5C) is never replaced.

With a window and its sound on, a file is decoded on a background thread when its track starts: the track is silent
until then (a few milliseconds for a WAV, about a second for a few minutes of Ogg Vorbis), and the file then joins at
the time the track has reached. The last file played stays in memory, so a track that starts again (the next round, a
retry) plays at once. Headless runs (`TTT_WAV`, tests) decode at once, so their output is the same on every run.
Each replacement is logged: `ttt: mods: track 21: <file> (VBlank 620)`.

### The tracks

The game registers 28 BGM tracks at boot (command 0x205E, [sound.md](sound.md) section 3). `TTT_MODS_TRACKS=1`
prints them as the program reads them from your disc, then the game's stage-to-track table (`D_004FB9C0`, read from
the game's data at run time), then each track start:

```
ttt: mods: table track  2: disc, loops, 93 s
ttt: mods: table stage 12: track 21
ttt: mods: track 21 starts (VBlank 620)
```

What that gives with the USA disc, and where each track plays. The lengths are the original files'; "where" comes
from the game's calls of `Bgm_Play` (`src/`) and from runs: the idle attract run (`TTT_FRAMES=2551`) plays track 1 at
VBlank 793; the select replay and scripted fight of `tests/test_port_input.py` play track 0 at VBlank 226 and
track 21 at VBlank 620.

| Track | Kind (game's table) | Where it plays |
|---|---|---|
| 0 | SPU RAM, loops | Character select (`Mode_CharSelect`) |
| 1 | disc, once, 37 s | The attract cycle's loading screen (mode 11, `func_003A9AB8`) |
| 2 | disc, loops, 93 s | After a fight (`Mode_Battle` sub-states 17 and 20), results (`func_003C4AE0`) |
| 3 | EE memory, loops, 16 s | Started by `func_003AD240` with the EE stream base [role not identified] |
| 4 | disc, loops, 16 s | No direct call found (the Theater's music player plays any track) |
| 5 | disc, loops, 13 s | Name entry (`Mode_NameEntry`), Tekken Bowl (`func_003F1570`) |
| 6 | disc, once, 182 s | Ending (`Mode_Ending`, `func_003AA8A8`) |
| 7 | disc, once, 47 s | Ending, the other variant (`D_005F6274` 0xC / 0xD), and after the ending is skipped with Start |
| 8 | disc, loops, 10 s | `Mode_Battle` sub-state 10, before the name entry (game over [I]) |
| 9-22 | disc, loops, 57-109 s | Stage music, by stage id (`D_005F5CB6`), below; game type 8 picks one of 9-22 at random (or 12) |
| 23-27 | disc, once, 1-4 s | Short pieces; no direct call found (Tekken Bowl's options table `D_00455D80`, the music player) |

Stage id -> track (the game's table, as printed by `TTT_MODS_TRACKS=1`):

| Stages | Track | Stages | Track |
|---|---|---|---|
| 0 | 9 | 12 (the scripted fight's) | 21 |
| 1, 15, 21 | 10 | 13, 10 | 19 |
| 2, 19, 22 | 11 | 11 | 20 |
| 3 | 12 | 14, 23, 24, 25 | 22 |
| 4 | 13 | 7, 16 | 16 |
| 5 | 14 | 8, 17, 20 | 17 |
| 6 | 15 | 9, 18 | 18 |

The stages' names are not established in this decompilation yet; the overlay's "Music now" line and the log show the
track while you play.

## Stage textures

Texture mods are texture packs in a mod folder ([textures.md](textures.md)): OpenGL only, PCSX2's names. They are read
with the pack's `replacements/` (mods first, in load order), even when `video.texture_replace` is off (then only the
mods' folders are read).

Making a stage retexture mod:
1. Turn on "Dump textures" (F1, Video; or `TTT_TEXTURE_DUMP=1`) and play a fight on the stage. Every new texture is
   written to `<textures>/SLUS-20001/dumps/<name>.png`.
2. Pick the stage's textures among the dumps (the floor, the walls of the background, the sky) and edit them. Keep the
   file names; any size works (a texture drawn at twice the size is sharper at higher internal resolutions). Alpha 128
   is opaque, as the game's blending reads it.
3. Put the edited files in `<mods>/<your mod>/textures/` (sub-folders allowed), turn dumping off, restart. The run
   reports `ttt: textures: mods: 1 mod folder, <k> replacements from mods, ...`.

A mod can carry a whole pack's folder (`textures/SLUS-20001/replacements/...`); a `dumps/` folder inside a mod is
skipped. Limits are those of texture packs: textures the GPU drew, textures larger than 1024 and some region-clamped
ones cannot be replaced (textures.md, Names).

## Stage geometry and new stages (assessed, not done)

- **The port cannot swap stage geometry from the host side the way it swaps textures.** The GS sees only the
  primitives the game's VU1 programs computed, in screen space, with no stable identity of the model they came from.
  A host-side mesh replacement would need its own renderer of the stage, outside the game's VU1 path.
- **Replacing the game's stage model data** (the files the game loads from `TEKKEN.BIN`, `func_00367938` and the
  stage's drawing code, [systems/stage.md](../systems/stage.md)) is possible in principle: the stages are open floors
  with no geometry in the simulation, so a new model would not change the fight's rules. But the loaded data lives in
  EE memory, which the state hash covers, and the files' sizes and the loader's timing would change the game's state.
  Such a mod is a modified disc: the port marks it `[modified game data]` ([data.md](data.md)) and netplay peers would
  need identical data. Not done.
- **New stages** need new entries in the game's stage tables, new camera values (`D_00507FE0`), music and effect
  tables, and the select screen's choices: game code and data changes, not a mod folder. Not done.

## Determinism and netplay

- Nothing in `plat_mods.c` is in a state (`plat_state.c`): the mod list, the files and the playback are host data. The
  state hash is unchanged with mods: the idle run (`TTT_FRAMES=2551 TTT_SEED=1`, 17 dumps) gives `0x8ba37caeb2bacfa8`
  and the scripted fight `0xcce5eb554fa51395`, with music mods for all 28 tracks, headless; the GL fight with those and
  a texture mod replacing all 403 textures it names gives `0xcce5eb554fa51395` too.
- Frames the rollback runs again (`gPortResim`) are not mixed again, so a mod file's playback never repeats or skips
  because of a rollback; it follows the track the game plays when the frames are heard.
- Mods are cosmetic: two netplay peers can use different mods. `Mods_Summary` gives one line, for example
  `music 2 tracks, textures 1 mod (stage, music)`; netplay's HELLO / WELCOME carry it after the player's name
  ([netplay.md](netplay.md)), each side prints `ttt: net: mods: ...` and `ttt: net: the peer's mods: ...`, and the
  overlay's Netplay page shows both. Information only: it is not among the terms the handshake compares, so it never
  blocks a match.

## Tests

`tests/test_port_mods.py` makes every file it uses (sine waves, one-colour PNGs, with ffmpeg an Ogg Vorbis file) in a
temporary folder:
- `ModsSelfTest` (`TTT_SELFTEST=mods`): the load order from `mods.order`, conflicts, WAV formats (8 / 16 / 24 / 32-bit,
  mono / stereo), the sidecar and `smpl` loops, the resampler, and the overlay's calls (move, disable) written to the
  settings file and read back at the next start; the environment's lists win over the file.
- `MusicMods`: a 1 kHz sine (22.05 kHz) replacing track 0 in the select replay: `TTT_WAV` holds the tone during the
  track and the game's own sound before it, `mods.music` off gives the run without mods byte for byte, the state hash
  is unchanged; the scripted fight with a file for every track keeps its state hash.
- `VorbisMods` (with ffmpeg and libvorbis): an Ogg file's length and loop comments, and its tone in the select replay.
- `TextureMods` (OpenGL): two mods replacing the same textures; the first in `mods.order` is drawn, the reversed order
  draws the other, the state hash is unchanged.

## Open

- Not heard by a person yet: loudness against the original tracks, and the async start on slow disks.
- No per-track crossfade or gapless switch: a new file starts when the game starts the track.
- Stage names for the track list; the roles of tracks 3, 4 and 23-27.
- MP3 / FLAC are not read (WAV and Ogg Vorbis only).
