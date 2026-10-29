# Tekken Tag Tournament PC port

A native PC port of **Tekken Tag Tournament** (PlayStation 2, 2000), built from a matching decompilation of the
game's executable. The game's own code is compiled for the PC and runs on a new platform layer (OpenGL renderer,
software sound chip, keyboard and gamepad input, memory cards, disc access). It is **not an emulator**, and this
repository contains **no game data**: you need your own copy of the game.

## Status

- **Matching decompilation.** The executable `SLUS_200.01` is rebuilt byte for byte (SHA-1
  `c7ad9a022bdc836827a0cc093f91b4281836f2bb`) from the C in this repository and the split of your own copy of the
  executable; functions not yet written as matching C still come from that split's assembly. See
  [docs/build.md](docs/build.md).
- **PC port on Linux.** Every game function comes from C, apart from a few hand-written replacements of
  vector-unit routines. The game boots, runs its menus, fights, movies and
  modes in real time in a window, with:
  - OpenGL 3.3 rendering with resolution scaling (1x to 8x), 16:9 / 21:9 widescreen and an option to turn off the
    game's flicker filter ([video](docs/port/video.md));
  - the game's sound and music on a software model of the PS2's sound chip ([sound](docs/port/sound.md));
  - keyboard and up to four gamepads, remappable, with vibration ([input](docs/port/input.md));
  - a settings overlay on F1, a fast-forward key and a setup screen that checks your disc on the first run
    ([settings](docs/port/settings.md), [install](docs/port/install.md));
  - rollback netplay over UDP and an optional self-hosted matchmaking server ([netplay](docs/port/netplay.md),
    [matchmaking](docs/port/matchmaking.md));
  - texture packs in PCSX2's layout, mods (music and textures) and a cheat page ([textures](docs/port/textures.md),
    [mods](docs/port/mods.md), [cheats](docs/port/cheats.md));
  - accessibility options (interface scale, high contrast, screen reader) and translations of the port's own text
    ([accessibility](docs/port/accessibility.md), [translations](docs/port/translations.md));
  - crash reports and a portable mode.
- **PC port on Windows.** The Windows build cross-compiles with mingw-w64 but **has never been run**
  ([windows](docs/port/windows.md)).

Open items are listed in the port's notes ([docs/port/README.md](docs/port/README.md)).

## Tested only on Arch Linux

The Linux build has been tested on **Arch Linux only**. Other distributions with glibc 2.34 or newer are expected to
work but have not been tried. The **Windows build is untested**: it may not start at all. Reports from other systems
are welcome.

## Requirements

- A 64-bit x86 CPU (x86-64).
- A GPU and driver with OpenGL 3.3.
- Linux with glibc 2.34 or newer (Ubuntu 22.04, Debian 12, Fedora 35 or later) and SDL 3; or Windows 10 / 11,
  64-bit (untested).
- About 10 MB of disk space for the program, its data file and your settings. The disc or disc image is read where
  it is and never copied.
- Your own disc of the game (next section).

## The disc you need

**Tekken Tag Tournament for PlayStation 2, USA release, version 2.00 (SLUS-20001)**, in any of these forms:

- the disc itself, mounted;
- a folder with the disc's files copied into it;
- a disc image: `.iso`, or `.bin` + `.cue`.

The program checks the disc before using it: `SLUS_200.01` must be the unmodified USA executable, 2,873,728 bytes
with SHA-1 `c7ad9a022bdc836827a0cc093f91b4281836f2bb`, and the game's other files must match that release too. Other
regions (for example the European release), other versions of the USA disc, other games and modified discs are refused.
Disc databases list this disc as "Tekken Tag Tournament (USA) (v2.00)". Details:
[docs/port/install.md](docs/port/install.md).

## Installing a release

Release archives contain the program, the matchmaking server, translations, documentation and licences; never any
part of the game.

**Linux** (`ttt-<version>-linux-x86_64.tar.gz`):

1. Install SDL 3 (your distribution's package, or built from libsdl.org); without it the program runs without
   a window.
2. Extract the archive anywhere and run `./ttt`.
3. The first run opens the setup screen: give it your disc, folder or image and press **Check and install**. Later
   runs start the game directly.

**Windows** (`ttt-<version>-windows-x86_64.zip`, untested):

1. Extract the archive to a folder. Official releases include `SDL3.dll` next to `ttt.exe`.
2. Run `ttt.exe`; the first run opens the same setup screen.

Without a window, `ttt --setup <disc or image>` does the same in one command. Each archive's `README.txt` explains
where files are stored, the controls and how to play online.

## Building from source

### Matching build

Needs Linux x86-64, Python 3.10 or newer and your disc. [docs/setup.md](docs/setup.md) installs the toolchain inside
the project (Python tools, PS2 binutils, the game's original compiler) and copies your disc into `iso/`:

```sh
python3 -m venv .venv && .venv/bin/python -m pip install -r requirements.txt
.venv/bin/python scripts/setup_disc.py path/to/disc.iso     # or disc.cue
.venv/bin/python configure.py && ninja                       # build/SLUS_200.01, checked against the original
```

### PC port

Needs clang and llc (LLVM; developed with LLVM 23), libclang's Python bindings (`clang.cindex`) and gcc. No game file
is needed to build; the program reads your disc when it runs.

```sh
python3 port/tools/build.py                   # build/port/ttt (Linux x86-64)
python3 port/tools/build_server.py            # build/server/ttt-server, ttt-sign
TTT_CC=win64 python3 port/tools/build.py      # build/port-win64/ttt.exe (needs mingw-w64)
build/port/ttt                                # first run: the setup screen asks for your disc
```

Release archives are made by `port/tools/release.py`, locally or by the GitHub Actions workflow on a version tag
([docs/port/release.md](docs/port/release.md)). Tests: `.venv/bin/python -m unittest discover -s tests`.

## Repository layout

| Path | Contents |
|---|---|
| `src/`, `include/` | The game's decompiled C and headers |
| `config/` | The splat configuration and symbol names of the executable |
| `configure.py` | Writes the matching build's `build.ninja` |
| `port/src/` | The PC platform layer (graphics, sound, input, files, netplay, overlay) |
| `port/tools/` | The port's build, link and release tools |
| `port/server/` | The matchmaking server and notice signer |
| `port/lang/` | Translations of the port's own text |
| `port/release/` | Player READMEs, the licence index and the CI build environment of the release archives |
| `third_party/` | Vendored libraries, each with its own licence |
| `scripts/` | Decompilation helpers (disc setup, tool check, permuter) |
| `tests/` | Unit and integration tests |
| `docs/` | Documentation of the game, the decompilation and the port ([docs/README.md](docs/README.md)) |

## Licence

- **The port's own code** (the platform layer, tools, server, tests and documentation written for this project) is
  licensed under the **GNU Affero General Public License v3.0 only** ([LICENSE](LICENSE)).
  Copyright (C) 2026 adanft.
- **The game's code** in `src/` and `include/` is a decompilation of the original executable. It is the work of the
  original rights holders and is **not licensed by this project**.
- **`third_party/`** keeps the licences of its projects:

  | Project | Licence |
  |---|---|
  | Dear ImGui | MIT |
  | pl_mpeg2 | MIT |
  | stb (stb_image, stb_vorbis) | MIT or public domain |
  | xxHash | BSD 2-clause |
  | TweetNaCl | public domain |
  | newlib libm (fdlibm) | BSD-style |

  The Windows archives also carry the licences of the mingw-w64 run time, GCC's runtime library exception and
  SDL 3 (zlib). Each archive's `LICENSES/` folder holds the full texts.

## Legal

This project is not affiliated with, endorsed by or sponsored by Bandai Namco Entertainment. Tekken and Tekken Tag
Tournament are trademarks of their respective owners. The project exists for preservation, study and
interoperability.

No game assets are distributed: no disc image, executable, data file, texture, sound or movie of the game is in this
repository or in its releases. To play you need your own legally obtained copy of the game. Never commit or share
game data, including the data file and memory cards the program creates from your disc.
