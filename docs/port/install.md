# Installing the PC port (T43)

The program contains no byte of the game. You need **your own disc of Tekken Tag Tournament, USA release
(SLUS-20001)**; the program reads it, checks it is the unmodified release, and makes its data file from it. No Python
or other tool is needed.

## Quick path

1. Have the disc available in one of these forms:
   - the disc itself, mounted (any folder showing `TEKKEN.BIN`, `SLUS_200.01`, `IRX/`; lower-case names are fine);
   - a folder with the disc's files copied into it;
   - an image of it: `.iso` (2048-byte sectors), or a raw `.bin` with its `.cue` (2352-byte sectors).
2. Start `ttt`. The first time, the **setup screen** opens: type or paste the path (Ctrl+V), or pick it with
   **Browse...** (folders that hold `TEKKEN.BIN` show as `[disc]`, images as `[image]`), or with the system's file
   dialog when your desktop has one. Press **Check and install**.
3. The disc is checked (about two seconds: all of `TEKKEN.BIN` is hashed), the data file is written, and the game
   starts. Later runs start the game directly.

Without a window (scripts, a terminal), the same in one command:

```sh
ttt --setup /path/to/disc-or-image     # exit code 0 done, 2 no disc there / cannot write, 4 refused
```

`ttt --setup` with no path opens the setup screen again (to point the program at another copy of the disc).
`ttt --version` prints the build (`git describe` of its source) and the languages it found catalogs for, and
exits (T44).

## Where files go

| What | Installed (default) | Portable | Override |
|---|---|---|---|
| Data file `ttt.dat` (+ `ttt.dat.disc`, the disc's check record) | `$XDG_DATA_HOME/ttt/`, else `~/.local/share/ttt/` | beside the program | `TTT_DATA=<file>` |
| Memory cards `saves/` | same folder | beside the program | `TTT_SAVES=<dir>` |
| Texture packs `textures/` | same folder | beside the program | `video.texture_dir`, `TTT_TEXTURES` |
| Settings `settings.ini` (holds `game.disc`, the disc you set up) | `$XDG_CONFIG_HOME/ttt/`, else `~/.config/ttt/` | beside the program | `TTT_CONFIG=<file>` |
| Crash reports `crash/` | beside the settings file | beside the program | follows the settings file |
| Translations `lang/<code>.json` (T47, [translations.md](translations.md)) | the data folder's `lang/` (your own), then `lang/` beside the program (a release's), then `port/lang/` of a checkout | beside the program | `TTT_LANG_DIR=<dir>` |
| The disc | where it is: read only, never copied | | `TTT_DISC=<folder or image>` |

- **Portable mode**: put an empty file named `portable.txt` beside the program; everything stays in that folder (a USB
  stick, a folder you move around).
- **A repository build** (`build/port/ttt`) keeps its old places: `build/port/ttt.dat`, the repository's `saves/` and
  `textures/`, the per-user settings file, the repository's `iso/` as the default disc.
- The variables always win over the defaults. The disc set up is remembered as `game.disc` in the settings file;
  `TTT_DISC` in the environment wins over it.
- The data file and the saves hold data from your disc: do not share them.

## What the check accepts

Only the unmodified USA release. `SLUS_200.01`, `IRX/RSPU2DRV.IRX` and all of `TEKKEN.BIN` are compared with the
SHA-1 of that release ([data.md](data.md)).

| You picked | The setup says |
|---|---|
| a folder or path with no disc in it | `no Tekken Tag Tournament disc at <path>: give the disc's folder ... or an image of it` (exit 2) |
| another game | `<path> boots SLUS_999.99 and has no TEKKEN.BIN: it is not Tekken Tag Tournament` (exit 4) |
| another region's release | `<path> boots SCES_500.01: it is not the USA release of Tekken Tag Tournament (SLUS-20001) ...` (exit 4) |
| a modified or damaged disc | `unknown or modified disc (<path>): <file> has SHA-1 <x>, expected <y>. ...` (exit 4) |
| a folder you cannot write as the data folder | `cannot write <file> (set TTT_DATA to a data file in a folder you can write)` (exit 2) |

On the setup screen the reason is shown in red and you can pick again. `TTT_ALLOW_MODIFIED=1` runs a modified disc
anyway, marked `[modified game data]` ([data.md](data.md#data-verification-and-the-mark-t41)).

## When the disc moves

If the disc is not found at the next start (an image renamed, a disc not mounted), a run with a window shows the setup
screen with the reason; a run without one prints `ttt: no disc: run ttt --setup <disc> ...` and exits with code 2.

## Developers

`scripts/setup_disc.py` (copying a disc into the repository's `iso/`) and the Python tools in `port/tools/` are for
working on the decompilation; players never need them. Tests drive the setup screen from inside with
`TTT_SETUP_TEST=<disc>` in a hidden window (`tests/test_port_setup.py`); see [README](README.md#platform-layer).
