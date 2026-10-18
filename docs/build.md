# Build

```sh
.venv/bin/python configure.py && ninja
```

`configure.py` needs `iso/SLUS_200.01` ([setup.md](setup.md), step 5). It runs splat on
`config/SLUS_200.01.yaml` when the split is missing or older than the config (`--split` forces it) and writes
`build.ninja`. ninja then:

1. assembles every file under `asm/` with the project's binutils (`include/macro.inc` defines splat's labels);
2. links them with the generated `build/SLUS_200.01.ld` into `build/SLUS_200.01.elf`;
3. flattens that with `objcopy -O binary` into `build/SLUS_200.01`;
4. checks its SHA-1 against the original (`build/SLUS_200.01.ok` is written only on a match).

The build counts only if ninja ends without `FAILED`. Editing the config or `configure.py` makes ninja run
`configure.py` again by itself.

`asm/`, `assets/`, `build/` and `build.ninja` are generated from the disc and ignored by git.

## Why the whole file is split

The executable keeps its section headers and some sections that are not loaded, so a plain link would not
give the same file. The config splits all of it by file offset, and the linker places every segment at its file
offset, so the flattened link is the original ELF byte for byte:

| File offset | Address | Segment | Contents |
|---|---|---|---|
| 0x000000 | 0x33F000 | `header` | ELF header, program headers, zero padding (binary) |
| 0x001000 | 0x340000 | `main` | `.text` |
| 0x0D6700 | 0x415700 | `main` | `.data`, `.vudata` (0x4F5F00) |
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

## Known assembler warnings

`.end directive without a preceding .ent directive` (twice in `asm/1000.s`): two small functions that are also
jump table targets get splat's `jlabel` instead of `glabel`. Harmless; the bytes are unchanged.
