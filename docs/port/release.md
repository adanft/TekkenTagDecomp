# PC port: releases (T44)

`port/tools/release.py` builds the Linux and Windows programs, packages each with what a player needs, checks that
no archive holds anything of the game, and writes the archives and their checksums to `dist/` (ignored by git). It
publishes nothing: no upload, no tag, no push. Putting the archives somewhere is a separate, human decision.

**No game data, ever.** The archives hold the port's programs, its translations, documentation and licences. The
player supplies their own disc; the program checks it and makes its data file from it on the first run
([install.md](install.md)). The disc image, the game's executable (`SLUS_200.01`), `ttt.dat` and its `.disc` record,
memory cards, texture dumps, sound, recordings, state files and signing keys are never packaged, and the release
refuses (and deletes) an archive in which its check finds any of them.

## Cutting a release

1. Commit everything (a tree with local changes is refused: `--allow-dirty` is for tests). Optionally name the
   version with an annotated tag (`git tag -a v0.1.0 -m "..."`): the version is `git describe --always --dirty`, the
   text the build embeds (lightweight tags are not used by it), e.g. `v0.1.0`, `v0.1.0-3-gabc1234` or `e34d8d5`.
2. Build, package and check, quietly:

   ```sh
   scripts/cool.sh python3 port/tools/release.py              # both targets
   scripts/cool.sh python3 port/tools/release.py --targets linux
   python3 port/tools/release.py --no-build                   # package what build/ already holds (same version)
   ```

   The build steps are the usual ones (`port/tools/build.py`, `TTT_CC=win64 port/tools/build.py`,
   `port/tools/build_server.py [--win64]`); the Windows target needs clang and mingw-w64 ([windows.md](windows.md)).
   `--no-build` refuses programs whose embedded version is not the checkout's.
3. Check the result: the release prints the portability report, each archive's size and content check, and the
   SHA-256 of each archive (`dist/SHA256SUMS`, `sha256sum -c` format). Then
   `TTT_SLOW_TESTS=1 python3 -m unittest discover -s tests -p test_port_release.py` packages again into a temporary
   folder, inspects both archives with its own lists, and runs the extracted Linux program as a player would; with
   the disc in `iso/` it sets up from it and plays the idle run to the reference hash.

Other entry points: `release.py --check <archive>...` (the content check alone, exit 1 on a problem) and
`release.py --portability` (the report alone). `--sdl3-dll PATH` (or `TTT_SDL3_DLL`) ships that `SDL3.dll` in the
Windows archive ([Releases from CI](#releases-from-ci)); without it the archives are as described below.

## Releases from CI

`.github/workflows/release.yml` builds both archives on GitHub Actions. It runs when a `v*` tag is pushed, or by hand
(Actions > release > Run workflow on any branch or tag). It does not run on every push to `main`: a run builds the
whole environment (LLVM, mingw-w64, SDL 3) and both targets, and its archives are only needed for a release.

1. **Environment.** `docker build` of `port/release/Dockerfile.ci`: Ubuntu 24.04 with clang / clang++ / llc of
   LLVM 23 (apt.llvm.org) under their plain names, libclang and its Python bindings (`ptr32.py`), gcc and binutils,
   mingw-w64 with POSIX threads, and **SDL 3 cross-built for Windows from its source release** (the exact tag is the
   `SDL3` argument, `3.4.18`, the version the port is tested with on Linux), installed with its `LICENSE.txt`. The
   Linux programs load SDL 3 at run time and are built without it, so no Linux SDL 3 is built.
2. **Build.** The checkout is mounted at its own path (the content check refuses an archive holding the checkout's
   path, and a short one such as `/src` would match ordinary text) and `port/release/ci_build.sh` runs inside the
   container as the runner's user. On a tag it makes the tag annotated locally if it is not (`git describe`, the
   version, names annotated tags only, and the checkout may fetch a tag without its tag object) and refuses to go on
   unless `git describe` gives exactly the tag. Then `release.py --targets linux,windows` builds, checks the
   portability limits and the content, and writes `dist/` as locally, with `SDL3.dll` and `LICENSES/sdl3.txt` in the
   Windows archive (`TTT_SDL3_DLL`, `TTT_SDL3_LICENSE`, `TTT_SDL3_VERSION` from the image).
3. **Artifacts.** The two archives, `SHA256SUMS` and the portability report are uploaded as the run's artifact
   `ttt-<tag>` (`ttt-dev` for a run that is not on a tag). Archive names inside follow `git describe` as always:
   `ttt-v0.1.0-...` on a tag, `ttt-<describe>-...` otherwise.
4. **Draft release.** On a tag only: `gh release create <tag> --draft --generate-notes` with the archives and
   `SHA256SUMS` attached (the job has `contents: write`). Publishing the draft is a human decision. Nothing is
   signed: the program has no updater that would check a signature.

The workflow uses only the repository: no disc, no game file, no secret besides the job's own token. A run on the
same ref waits for the previous one (`concurrency`). The environment and the workflow have not been run yet (no
container was built for this change); the first tag build is their test.

When `SDL3.dll` is shipped, the Windows `README.txt` says it is included (instead of the download instructions) and
`LICENSES/README.txt` lists SDL 3 (zlib) in place of the note that SDL 3 is not part of the release. The content
check allows exactly one DLL, `SDL3.dll` at the top of a Windows archive; every other `.dll` and any `.so` stays
forbidden.

The same programs give the same archives, byte for byte: sorted entries, the commit's time on every entry (or
`SOURCE_DATE_EPOCH`), no owner names, gzip without a timestamp, and the PE programs' link time (which `objcopy` sets
to "now") set to the commit's time with its checksum recomputed.

## What an archive holds

`dist/ttt-<version>-linux-x86_64.tar.gz` (execute bits kept) and `dist/ttt-<version>-windows-x86_64.zip`, each with one
top folder `ttt-<version>-<target>/`:

| File | Linux | Windows |
|---|---|---|
| The program | `ttt` | `ttt.exe` |
| Its symbol names (crash reports: `<program>.syms` beside it, `plat_crash.c`) | `ttt.syms` | `ttt.exe.syms` |
| Matchmaking server and notice signer ([matchmaking.md](matchmaking.md)) | `ttt-server`, `ttt-sign` | `ttt-server.exe`, `ttt-sign.exe` |
| Translations, found beside the program (`lang/`, [translations.md](translations.md)) | `lang/en.json`, `lang/es.json` | same |
| The player README (`port/release/README.<target>.txt`) | `README.txt` (tested on Arch Linux only) | `README.txt` (CRLF), marked **untested on Windows**, with the `SDL3.dll` instructions or the note that it is included |
| `SDL3.dll` (only with `--sdl3-dll`: the CI release) | no | yes, with `LICENSES/sdl3.txt` (zlib) |
| `VERSION.txt` (version, target, commit), `CHANGELOG.txt` (the port's `feat` / `fix` / `perf` commits since the previous tag) | yes | yes |
| `docs/`: install, settings, input, video, sound, netplay, matchmaking, mods, textures, translations, accessibility, cheats | yes | yes |
| `LICENSES/`: an index (`port/release/LICENSES.txt`), the port's own licence (`ttt.txt`, the repository's `LICENSE`: AGPL-3.0-only) and the licences of Dear ImGui, pl_mpeg2, stb, xxHash, TweetNaCl, newlib's libm | yes | yes, plus the mingw-w64 run time's (winpthreads, linked in) and GCC's runtime library exception (libgcc) |

The programs are stripped of their debug sections (`objcopy --strip-debug`): those hold the checkout's paths, and
`ttt` shrinks from 9 MB to 3.1 MB (`ttt.exe` from 12 MB). The symbol table stays (the state's region names, `plat_state.c`, and
`ee_diff.c` read it). Dear ImGui's `IM_ASSERT` texts name their file through `__FILE__`, compiled with
`-fmacro-prefix-map=<checkout>/=` (toolchain.py), so no path of the build machine is left in any file; the check
refuses one that holds it.

`SDL3.dll` is shipped only when `--sdl3-dll` names one (the CI release builds it from SDL's source); a local
release without it says where to get it. Nothing else not built here is shipped.

`ttt --version` prints `ttt <version> (<platform>)` and `languages: en es ...` (the catalogs it found) and exits,
with no disc and no window (`Port_VersionArgs`, plat_crash.c, after the settings and the language are read).

## The content check

`release.check` runs on every archive before it is kept; `tests/test_port_release.py` repeats the name checks with
lists of its own.

- **Allowlist**: below the top folder only the files above (`lang/*.json`, `LICENSES/*.txt`, `docs/*.md` one level
  deep). One top folder; no links, devices or paths leaving it.
- **Forbidden names**, in any folder: disc images (`.iso .bin .cue .img .mdf .nrg .chd .cso ...`), `SLUS_*` /
  `SCES_*`, `.elf`, `.irx`, `SYSTEM.CNF`, `ttt.dat*`, `.disc`, memory cards (`.mcd .ps2 .vmc`), `settings.ini`,
  `portable.txt`, keys (`.key`), pictures and sound (`.png .ppm .bmp .tga .dds .wav .ogg .vag ...`), movies
  (`.pss .ipu .m2v`), states and recordings, shared libraries (`.dll .so`; only `SDL3.dll` at the top of a Windows
  archive is allowed), and folders `saves/ textures/ mods/
  crash/ iso/ frames/ build/ replacements/`.
- **The game's files by size and SHA-1**, from `port/tools/data_layout.txt` (the executable, 2873728 bytes,
  `c7ad9a02...`; the data payload, `37051598...`).
- **Samples of the game's files on this machine** (when present: `TTT_DISC`, `iso/`, `build/SLUS_200.01`,
  `build/port/ttt.dat`, the player's data folder): 64-byte windows with at least 24 different byte values, every 256
  bytes of files up to 16 MB (the executable: 4283, the data file: 1097, `RSPU2DRV.IRX`: 129) and 4096 places in
  larger ones (`TEKKEN.BIN`: 3306), each searched for in every archived file. A file of the same size is compared by
  hash too.
- **Library tables are not the game's**: the game links newlib, so its data holds the same constant tables as the
  programs. The first runs found exactly two such windows: fdlibm's `npio2_hw` (n·π/2, `ef_rem_pio2.c`) in `ttt`
  through the vendored `third_party/newlib_libm`, and the double powers of ten 1e0..1e22 of number conversion in
  `ttt-server.exe` / `ttt-sign.exe` (mingw-w64's run time). Windows that lie inside the vendored libm's tables or the
  standard powers-of-ten tables are therefore not sampled (`vendored_tables`); everything else still is.

Result on the current programs: both archives clean (no name outside the allowlist, no forbidden name, no game file,
none of 13,098 samples found, no checkout path).

## Portability

The Linux programs need glibc and nothing else (`NEEDED`: `libc.so.6` only; SDL 3, OpenGL and speech are loaded at
run time with `dlopen`, so a missing SDL 3 means no window rather than a refusal to start). `objdump -T` gives the
symbol versions they need; compared with glibc 2.31 (Ubuntu 20.04, Debian 11):

| Program | Newest version | Newer than 2.31 |
|---|---|---|
| `ttt` | GLIBC_2.34 | 2.33: `stat`; 2.34: `__libc_start_main`, `dlopen`, `dlsym`, `dlclose`, `dlerror`, `pthread_create`, `pthread_join`, `pthread_detach` |
| `ttt-server`, `ttt-sign` | GLIBC_2.34 | 2.34: `__libc_start_main` |

**Required: glibc 2.34 or newer** (Ubuntu 22.04, Debian 12, Fedora 35, RHEL 9 and later). The 2.34 floor comes from
linking on a system with glibc 2.34+, where libpthread and libdl are part of libc and `__libc_start_main` has a new
version that the start-up code (`crt1.o`) calls; it cannot be lowered by renaming symbols (before 2.34 the main
program's constructors were run by the start-up code itself, so an older glibc under the newer `crt1.o` would skip
them, and the memory map is set up by one). The release
refuses a program needing more than 2.34, and anything besides glibc's libraries.

Fixed for T44: glibc 2.38's headers send `sscanf` / `strtol` / `strtoul` to `__isoc23_*` names (C23's `0b` prefix)
under `_GNU_SOURCE` (all C++, `plat_os_posix.c`, the server), which made all three programs need **GLIBC_2.38**
(Ubuntu 24.04, Debian 13 or later). The x86-64 Linux objects now call the plain names (`objcopy --redefine-sym`, toolchain.py
`isoc23_redefine_args`, build_server.py `build_linux`): the same functions for everything the programs read (no
binary `0b` input). The 32-bit variants keep theirs (their PLT is fixed at 48 entries).

For glibc 2.31-2.33, build the programs in an older system: `port/release/Dockerfile.linux` is a recipe for
Ubuntu 20.04 with LLVM 18 and Python 3.11, **never built or run** (no image was pulled for T44). Note that SDL 3 is
not packaged by distributions that old; players there build SDL 3 themselves.

Windows: the programs import only system DLLs (`KERNEL32`, `USER32`, `SHELL32`, `WS2_32`, `bcrypt`) and the
Universal C Runtime (`api-ms-win-crt-*`, part of Windows 10 and later); winpthreads and libgcc are linked in
(`-static`). The release refuses any other import (for example `libwinpthread-1.dll`). The Windows programs have not
been run (no Windows, no Wine here): the README says so in its first lines.

`dist/ttt-<version>-portability.txt` keeps the report of each release.

## Tests

`tests/test_port_release.py`:

- every run: the checker on made-up archives (forbidden names, links, two top folders, the checkout's path, a
  program embedding 16 KB of the game's executable or data, the executable itself by hash, library tables skipped);
- when `dist/` holds the current version's archives (or always with `TTT_SLOW_TESTS=1`, which packages the programs
  already built into a temporary folder): the allowlist, forbidden names, required files, execute bits, the
  Windows README's warning, `SHA256SUMS`, the portability limits; the Linux archive extracted and run with an empty
  home, no display and SDL's dummy sound: `--version` (version, `languages: en es`, nothing written), the same
  without `lang/` (English only: the catalogs come from beside the program), `TTT_SELFTEST=hw`, the first run with
  no disc and no window (exit 2, `ttt: no disc: ...`, no data file), `--setup` on an empty folder (exit 2);
- `TTT_SLOW_TESTS=1` with the disc in `iso/`: `ttt --setup iso/` in the extracted folder, then the idle run
  (`TTT_FRAMES=2551 TTT_SEED=1`, a picture every 150 VBlanks) ends in `0x8ba37caeb2bacfa8` with 17 pictures.

## Open

- The Windows archive has never run anywhere; the `SDL3.dll` instructions and the paths in its README are from the
  code, not from a test.
- Neither container recipe (`Dockerfile.linux`, `Dockerfile.ci`) nor the release workflow has been run.
- `CHANGELOG.txt` is generated from commit subjects: before the first tag it lists the port's whole history.
