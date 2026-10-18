# Build

```sh
.venv/bin/python configure.py && ninja
```

`configure.py` needs `iso/SLUS_200.01` ([setup.md](setup.md), step 6) and the game's compiler (step 4). It runs
splat on `config/SLUS_200.01.yaml` when the split is missing or older than the config (`--split` forces it) and
writes `build.ninja`. ninja then:

1. assembles every split file under `asm/` with the project's binutils (`include/macro.inc` defines splat's
   labels);
2. compiles every C file the config links (a `c` subsegment) with ee-gcc and assembles the output (below);
3. links everything with the generated `build/SLUS_200.01.ld` into `build/SLUS_200.01.elf`;
4. flattens that with `objcopy -O binary` into `build/SLUS_200.01`;
5. checks its SHA-1 against the original (`build/SLUS_200.01.ok` is written only on a match).

The build counts only if ninja ends without `FAILED`. Editing the config, `config/symbols/*.txt` or
`configure.py` makes ninja run `configure.py` again by itself (splat reads the symbol file, so a new name
reaches the split assembly).

`asm/`, `assets/`, `build/` and `build.ninja` are generated from the disc and ignored by git.

## C files

A C file covers one whole object: a `[offset, c, name]` subsegment in the config, compiled from `src/name.c`.
Its start and the start of the next subsegment must be function starts at multiples of 16, because the
generated linker script aligns every object to 16. Functions not decompiled yet stay in the file as
`INCLUDE_ASM("asm/nonmatchings/name", func);`: splat writes one file per such function under
`asm/nonmatchings/name/` (run `configure.py --split` after adding one) and the macro (`include/include_asm.h`)
assembles it in place.

Each C file is built in three steps, because the 1999 compiler driver cannot run its own assembler here:

1. `ee-gcc -O2 -fno-strength-reduce -ffast-math -G8 -S` (the flags are in `configure.py`; `G_FLAGS` sets
   another `-G` for a path prefix);
2. the project's `as` with `-mabi=o64`, reading `include/gcc_prelude.inc` first (instructions the game's
   assembler encoded differently);
3. `configure.py --eabi64` sets the object's ABI tag to EABI64 like the split files, so the linker accepts both.

Why these choices: [decomp_guide.md](decomp_guide.md).

## Why the whole file is split

The executable keeps its section headers and some sections that are not loaded, so a plain link would not
give the same file. The config splits all of it by file offset, and the linker places every segment at its file
offset, so the flattened link is the original ELF byte for byte:

| File offset | Address | Segment | Contents |
|---|---|---|---|
| 0x000000 | 0x33F000 | `header` | ELF header, program headers, zero padding (binary) |
| 0x001000 | 0x340000 | `main` | `.text` |
| 0x0D6700 | 0x415700 | `main` | `.data` |
| 0x1B6F00 | 0x4F5F00 | `main` | `.vudata`: VU microcode packets, built from listings (below) |
| 0x1BBF70 | 0x4FAF70 | `main` | rest of `.vudata` (VIF1 block template) |
| 0x1BC880 | 0x4FB880 | `main` | `.rodata` |
| 0x2B6100 | 0x5F5100 | `main` | `.lit4` (gp = 0x5FD0F0 = `.lit4` + 0x7FF0) |
| 0x2B6C00 | 0x5F5C00 | `main` | `.sdata` |
| - | 0x5F6B00 | `main` | `.sbss`, `.bss` up to 0x1FCDEF0 (not in the file) |
| 0x2B7AF0 | - | `trailer` | `.reginfo`, VU overlay sections (`.DVP.*`), section names and headers (binary) |

Inside the loaded image, address = file offset + 0x33F000. Two details of the original link are reproduced:

- splat's linker script pads segments to 16 bytes, so `trailer` starts at 0x2B7AF0, after the 12 zero bytes
  that follow `.sdata`;
- the original aligned `.sbss` to 64 (0x5F6B00); `configure.py` adds `ALIGN(64)` to the bss output section of
  the generated linker script.

The `trailer` segment has its own `exclusive_ram_id`: given address 0 in the global range, splat would turn
constants such as 0xFFFF into labels that do not exist.

## VU microcode

The VU0 and VU1 microcode packets in `.vudata` (0x4F5F00-0x4FAF70) come from the listings in `src/vu0/` and
`src/vu1/` ([systems/vu1/README.md](systems/vu1/README.md)). ninja assembles each packet listing with
`scripts/vuasm.py` into `build/vu/*.bin`, and the object splat cuts from that range (`asm/data/vudata.data.s`) is
built from `src/vu1/vudata.s` instead, which includes the binaries under the labels the code uses. A packet is
rebuilt when any listing in its folder changes; run `configure.py` after adding a listing file.
`.venv/bin/python scripts/vuasm.py` checks every packet of the original against a disassemble/assemble round
trip and against the listings.

## Known assembler warnings

`.end directive without a preceding .ent directive` (twice in `asm/1B30.s`): two small functions that are also
jump table targets get splat's `jlabel` instead of `glabel`. Harmless; the bytes are unchanged.
