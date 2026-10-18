# Setup (Linux x86-64)

Everything is installed inside the project: no root, no system packages. You need `python3` (3.10 or newer),
`git`, `curl` and `tar`.

## 1. Python tools

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

This installs splat (splits the executable), m2c (first C draft of a function) and the modules decomp-permuter
needs. Every version is pinned in `requirements.txt`.

## 2. MIPS binutils

The decompals build of GNU binutils 2.40 for the PS2's CPU (R5900), release v0.10:

```sh
mkdir -p tools/binutils
curl -L -o /tmp/binutils-ps2.tar.gz \
  https://github.com/decompals/binutils-mips-ps2-decompals/releases/download/v0.10/binutils-mips-ps2-decompals-linux-x86-64.tar.gz
echo "9fe31ea3ee1a37536f9f0e2e12c668e7c3cb99e59f5154bd2f1fc4762473094c  /tmp/binutils-ps2.tar.gz" | sha256sum -c
tar xzf /tmp/binutils-ps2.tar.gz -C tools/binutils
```

The tools are `tools/binutils/mips-ps2-decompals-as`, `-ld`, `-objcopy`, `-objdump` and so on.

## 3. decomp-permuter

```sh
git clone https://github.com/simonlindholm/decomp-permuter tools/decomp-permuter
git -C tools/decomp-permuter checkout 059609d4aec73eb0650726772954e1ad575825f8
```

Run it with the project's Python: `.venv/bin/python tools/decomp-permuter/permuter.py`.

## 4. The game's compiler

Sony's ee-gcc 2.9-ee-991111-01 for Linux (a 32-bit x86 build), as the decomp.me compiler archive:

```sh
mkdir -p tools/compilers/ee-gcc2.9-991111-01
curl -L -o /tmp/ee-gcc2.9-991111-01.tar.xz \
  https://github.com/decompme/compilers/releases/download/compilers/ee-gcc2.9-991111-01.tar.xz
echo "ed684fd98f89d36b0121caab311052089103e3b36241fcef4338cc9ea41c75b8  /tmp/ee-gcc2.9-991111-01.tar.xz" | sha256sum -c
tar xJf /tmp/ee-gcc2.9-991111-01.tar.xz -C tools/compilers/ee-gcc2.9-991111-01
```

The build runs `tools/compilers/ee-gcc2.9-991111-01/bin/ee-gcc` with `-S` only; the project's binutils assemble
its output. Why this version: [decomp_guide.md](decomp_guide.md).

## 5. Check

```sh
.venv/bin/python scripts/check_tools.py
```

Every required tool must show `OK`. Tools marked `later` (objdiff, Ghidra, PCSX2) are set up in later steps and
do not fail the check.

## 6. Your disc

The game files come from your own Tekken Tag Tournament disc (USA, SLUS-20001), as an ISO or a BIN/CUE dump:

```sh
.venv/bin/python scripts/setup_disc.py path/to/disc.cue     # or disc.bin, or disc.iso
```

The script reads the disc's file system itself (no extra tool), checks that `SLUS_200.01` is the unmodified USA
executable (size and SHA-1) before writing anything, then copies every file of the disc into `iso/` (about
640 MB). Any other file, game or region is refused with a message and exit code 1. Running it again skips files
already there; `--force` rewrites them. `--save-iso out.iso` only converts a BIN/CUE into an ISO.

`iso/` is ignored by git: nothing from the disc is ever committed.

The script's tests build tiny disc images of their own and need no disc:

```sh
.venv/bin/python -m unittest discover -s tests -v
```

## 7. Build

```sh
.venv/bin/python configure.py && ninja
```

Splits the executable and rebuilds it byte for byte; see [build.md](build.md).
