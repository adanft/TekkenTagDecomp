# The game's data on PC: from your own disc (T25)

Rule: no ISO, ELF, asset or extracted table of the game is ever in git or in anything shipped. A release ships only
the program built from this repository's sources; everything of the game comes from the user's own disc when the
program runs. This page lists every piece of game-derived data the port uses, where it comes from, and how the
program gets it.

## Inventory

| What | Size | Origin | How the port gets it |
|---|---|---|---|
| Initialised data at its PS2 addresses: `.data`, `.vudata` (the VU1 microprograms), `.rodata`, `.lit4`, `.sdata` of `SLUS_200.01`, including the `INCLUDE_RODATA` tables and the data blocks of functions still compiled from drafts | 1,958,508 bytes in 83 ranges, 0x00415700-0x005F6AE4 | the disc's `SLUS_200.01` | copied from the disc into EE RAM at start (`port/src/plat_data.c`), through the data file `ttt.dat` |
| `.sbss`, `.bss` | 0x19D73F0 bytes from 0x005F6B00 | zeroes | the EE RAM memory object's zeroes (`plat_mem.c`) |
| Words of that data that hold a host address in the port: the game's tables of function pointers (`.word func_…`, `.word StartThread`…) | 273 words | the decompilation's names | filled in by the host linker (`gPortDataFix` in the generated `build/port/gen/data_table.c`), written over the disc's words after loading |
| Words that point at labels inside library assembly (jump tables the port's C replaces) | 342 words | — | written as 0 (as the build-linked data had them) |
| `TEKKEN.BIN` (the archive: models, textures, sounds, movies) | 630,474,752 bytes | the disc | read at run time by sector (`plat_file.c`), as before |
| `IRX/RSPU2DRV.IRX` (the one-shot movie track's length and volume table) | 66 KB | the disc | read at run time (`plat_movie.c`), as before |
| Draft divergence check (`TTT_DIFF`, a developer tool): the original EE code | the whole executable | `build/SLUS_200.01`, else the disc's `SLUS_200.01` | read when the check runs (`ee_diff.c`) |
| Data layout: where the ranges are, which words are rewritten with which name, the payload's size and SHA-1, names with no address elsewhere | 18 KB text, no data byte | made from the split by `port/tools/make_layout.py` | committed: `port/tools/data_layout.txt` |
| Known hashes: SHA-1 of `SLUS_200.01` (also in `config/SLUS_200.01.yaml`), of the payload, of `RSPU2DRV.IRX` and `TEKKEN.BIN`, and their sizes | numbers | the USA release (SLUS-20001) | in the program (`data_layout.txt`, `plat_data.c`) |

Nothing of the game is linked into the program any more: `build/port/ttt` has no section in EE RAM
(`tests/test_port_mem.py`) and none of the 289 varied 32-byte windows the test samples from the data (one per KB)
is in it, nor any of 1,098 at one per 256 bytes, while the build-linked program held all 82 sampled at one per 4 KB
(`tests/test_port_data.py`). The data names the C uses (`D_XXXXXXXX`, `gFoo`) are absolute symbols at their PS2
addresses (`build/port/gen/symbols.ld`, `image.ld`).

### What the build still needs, and what it does not

- `port/tools/build.py` (`undefined.py` + `link.py`) needs neither `SLUS_200.01` nor its split (`asm/`): it
  compiles `src/` (every function from C: matched code and the drafts) and `port/src`, and takes the data layout
  from `port/tools/data_layout.txt`. Checked: a copy of the tracked files alone (no `asm/`, `build/`, `iso/`) builds
  `build/port/ttt`, which makes the same `ttt.dat` byte for byte.
- No assembly of the game is compiled into the port: the hand-written EE / VU0 routines are replaced by
  `port/src/vu0_pc.c` and `plat_vu0.c`.
- `port/tools/gen_data.py` and `port/tools/make_layout.py` need the split and the executable. A developer runs them
  only when the decompilation's data split changes (a file moves from assembly to C, a name is added, a C file
  defines a variable the data points to):

  ```sh
  .venv/bin/python configure.py && python3 port/tools/gen_data.py && python3 port/tools/make_layout.py
  ```

  `make_layout.py` compares every assembled data file with the executable and stops at any byte it cannot explain.
  A stale layout shows at run time as `the data taken from SLUS_200.01 does not match port/tools/data_layout.txt`.
- The matching build (`configure.py`, `ninja`) of course still needs the executable: it is the proof the C is right.

## The data file and the checks

On start (`Port_MapMemory` in `plat_mem.c`, a constructor, before anything of the game):

1. The data file is `TTT_DATA`, else `ttt.dat` in the data folder: beside the program in a checkout
   (`build/port/ttt.dat`, ignored by git) or when portable, else `$XDG_DATA_HOME/ttt/` ([install.md](install.md)).
   The folder is made when the file is written. If it is whole (header, size, the layout this program expects, payload SHA-1) and unmodified, its payload
   is copied into EE RAM; the disc is then checked lightly on every run: `TEKKEN.BIN`'s size and `RSPU2DRV.IRX`'s
   SHA-1 (22 ms in all), and `TEKKEN.BIN`'s content through its recorded check (below).
2. Otherwise it is made from the disc (`TTT_DISC`, the settings' `game.disc`, `iso/`: [README](README.md),
   "Platform layer"; a folder, `.iso`, or raw `.bin` / `.cue`): `SLUS_200.01`
   (size and SHA-1), `RSPU2DRV.IRX` (SHA-1) and the whole `TEKKEN.BIN` (size and SHA-1) are checked against the USA
   release, the ranges' bytes are taken out of the executable's loadable segment and their SHA-1 compared with the
   layout's, and the file is written (temporary file, `fsync`, rename). About 1.1 s once. If the file cannot be
   written the data is taken from the disc on every run.
3. The 615 words of `gPortDataFix` are written over the loaded data.

Format: `TTTDAT\0\1`, then 32-bit little-endian header size (64), flags (bit 0: modified game data), payload size,
layout check (a word of the SHA-1 of the expected payload size and SHA-1), the payload's SHA-1 (20 bytes), the
source `SLUS_200.01`'s SHA-1 (20 bytes); then the payload: the ranges' bytes in address order, as on the disc. It
holds the user's own game data and is never shipped.

| Case | Result |
|---|---|
| no data file and no disc | a run in real time with a window: the setup screen ([install.md](install.md)); otherwise `ttt: no disc: …` on stderr, exit code 2 (as before) |
| a file of the disc differs from the USA release | `ttt: unknown or modified disc (<disc>): <file> has SHA-1 <x>, expected <y>. …`, exit code 4, no data file written |
| the same with `TTT_ALLOW_MODIFIED=1` | runs; `ttt: [modified game data] <file> differs from the USA release` on stderr, `gPortModifiedData = 1` (for T41's mark), the data file is flagged; a flagged file is used only under `TTT_ALLOW_MODIFIED=1`, otherwise it is made again (refused if the disc is still modified) |
| a damaged data file (payload SHA-1) | `ttt: <file> is damaged: making it again from the disc` |
| `TEKKEN.BIN` changed after the data file was made | found by the background check: `ttt: [modified game data] TEKKEN.BIN differs …` on stderr, the run goes on marked; the next run refuses the disc (exit code 4) unless `TTT_ALLOW_MODIFIED=1` |
| `TTT_SELFTEST=<name>` without a disc | the self-test runs on zeroed EE RAM |

## Data verification and the mark (T41)

What each run verifies, against the hashes of the unmodified USA release:

| Data | When the data file is made | Every later run |
|---|---|---|
| `SLUS_200.01` (the game's initialised data) | size and SHA-1 | the data file's payload SHA-1 against the layout's (the payload is all the game takes from it) |
| `IRX/RSPU2DRV.IRX` (read at run time by the movie player) | SHA-1 | SHA-1 |
| `TEKKEN.BIN` (every file the game reads; from a directory or inside the `.iso`) | size and SHA-1 | size; its SHA-1 from `<data file>.disc` while the host file is unchanged, else hashed again in the background |

`<data file>.disc` (`build/port/ttt.dat.disc` beside the default data file) is one line: `TTTDISC 1 <device>
<inode> <size> <modification time s> <ns> <archive offset> <SHA-1>` for the host file that holds `TEKKEN.BIN` (the
disc directory's `TEKKEN.BIN` or the `.iso`, links followed). Numbers and a hash only: it holds no game data. It is
written when the data file is made (the archive is hashed then anyway) and after each background check; any change
of the host file (copied, replaced, touched) changes the key and the archive is hashed again. The background check
reads with its own file handle on a thread of its own (about a second for 630 MB from the page cache) while the game
starts; the game never waits for it, and nothing of the simulation reads its result. Self-tests (`TTT_SELFTEST`)
skip it. A `TTT_FRAMES` run waits for it before its report line (`ttt: data: …`).

The mark: when any of these differs (`TTT_ALLOW_MODIFIED=1` needed to run at all, except a `TEKKEN.BIN` found
changed by the background check, which marks the run already going), the run shows `[modified game data]`:

- on stderr when found (`ttt: [modified game data] <file> differs from the USA release …`);
- in the window title (`Tekken Tag Tournament [modified game data]`, set again when the background check finds it
  after the window is open);
- in the settings overlay (F1): above every page, and on the Game page's "Game data" line with the file
  (`plat_data.c`'s `Port_DataState`: verified, modified, being checked, not checked);
- in a `TTT_FRAMES` run's report: `ttt: data: USA release (verified)`, `ttt: data: [modified game data] (<file>)`
  or `ttt: data: not checked (<why>)`;
- in a crash report's `game data:` line ([README](README.md#crash-reports)).

Data that is the verified release shows no mark, also under `TTT_ALLOW_MODIFIED=1`: the variable allows a
modified disc, it does not mark one that is not. Texture replacements ([textures.md](textures.md)) draw other
pictures but change no game data: they are a setting (`video.texture_replace`), shown as such in a crash report, and
never mark the data.

The idle run (`TTT_FRAMES=2551 TTT_SEED=1`, a dump every 150 VBlanks) on the data loaded from the file gives the 17
pictures byte for byte and the state hash `0xd36c82e9a145875a` of the build-linked data. EE RAM holds the same
bytes; the only difference is 0x00415000-0x00415700, where the loader used to map the program's ELF header in
front of the data and which the hash leaves out (`plat_state.c`); the host addresses moved inside the hash's fixed
window.

## Shipping model

A release is the program built from the repository: `build/port/ttt` contains the decompiled code, the platform
layer, the layout's addresses and the hashes above, no byte of the game. The player points it at their disc on the
setup screen or with `ttt --setup <disc>` (T43, [install.md](install.md): a mounted disc, a folder of its files, an
`.iso`, or a raw `.bin` / `.cue`); the program checks the disc and writes `ttt.dat` itself, and remembers the disc as
`game.disc`. Nothing a player does needs Python: `scripts/setup_disc.py` (copying a disc into the repository's `iso/`)
and `port/tools/*.py` are developer tools. T44 packages the program alone.

### Before and after T43

| Step | Before | Now |
|---|---|---|
| Disc as a raw `.bin` / `.cue` | `scripts/setup_disc.py` (Python) copied it to `iso/` | read directly (`plat_file.c`) |
| Disc mounted by the system (lower-case names) | not found | found |
| Telling the program where the disc is | `TTT_DISC` or `iso/` beside the program, by hand | the setup screen or `--setup`, remembered as `game.disc` |
| Making `ttt.dat` | first run, beside the program | first run or the setup, in the per-user data folder |
| A refused or missing disc | message and exit code | the same without a window; the setup screen with the reason in a real-time run |

## Files

- `port/tools/make_layout.py` (developer, with the split) -> `port/tools/data_layout.txt` (committed).
- `port/tools/link.py`: `read_layout`, `image_script` (absolute bounds `__ttt_data_*` / `__ttt_bss_*`), `resolve`
  (names in the layout's ranges -> their PS2 address), `data_table` (-> `build/port/gen/data_table.c`: ranges, words,
  hashes; only the words whose name the linker does not place at its own PS2 address).
- `port/src/plat_data.c`: SHA-1, the data file, the disc checks, `Port_LoadGameData`; T41: the `.disc` record, the
  background check, `Port_DataState` / `Port_DataMark` / `Port_DataReport`.
- `port/src/plat_file.c`: `Port_DiscReadFile(NULL, name)` reads a file of the disc's root; `Port_DiscArchiveSize`;
  `Port_DiscArchiveFile` (the host file holding `TEKKEN.BIN` and its offset) and `Port_DiscArchiveReadAt` (a read
  through another handle, in the image's sector layout); T43: raw `.bin` / `.cue`, lower-case folders,
  `Port_DiscUse`, `Port_DiscBootName`.
- `port/src/plat_setup.c`, `port/src/plat_paths.c` (T43): the setup and the data folder; `Port_DataInstall`,
  `Port_DataPending` in `plat_data.c`.
- `port/src/plat_window.c` (title), `port/src/overlay.cpp` (overlay): the mark.
- Tests: `tests/test_port_data.py` (`DataMarkTest`: the mark with a changed `RSPU2DRV.IRX` or `TEKKEN.BIN` in a
  temporary copy of the disc, none otherwise), `tests/test_port_mem.py`, `tests/test_port_tools.py`.
