# Decompiling (working notes)

The goal is C that compiles to the same bytes as the original with the game's own compiler. How the build
compiles a C file and links it: [build.md](build.md), "C files".

## Matching loop

1. Pick a function (`func_XXXXXXXX`) in a C file's range. Write the attempt above its `INCLUDE_ASM` line as
   `#if 0` C `#else` INCLUDE_ASM(...) `#endif` while it does not match, so the build stays byte-identical.
2. To test it, enable the C (drop the `#if 0` and the INCLUDE_ASM) and run
   `.venv/bin/python scripts/fdiff.py func_XXXXXXXX` (a name or an address works too). It rebuilds and compares
   the function's linked bytes with the original: `OK`, or both disassemblies aligned, with `!` on a different
   instruction, `<` / `>` on one only in the original / the build, and `~` on a call or branch whose target
   moved only because something before it changed size.
3. For a near miss (registers, instruction order), put the attempt back in `#if 0` and run
   `.venv/bin/python scripts/permute.py src/<file>.c func_XXXXXXXX` (setup in `build/permuter/<Func>/`, the
   original from `asm/nonmatchings/`); add `--run -j8` to start decomp-permuter. A score of 0 ignores some
   differences: apply the candidate by hand and check it with fdiff.
4. When fdiff says OK, remove the `#if 0` / INCLUDE_ASM pair and run `.venv/bin/python configure.py && ninja`:
   the build is the final word (SHA-1). `.venv/bin/python scripts/progress.py` prints the share of code in C.

Names are in `config/symbols/` (see "Naming" below). Library code (Sony SDK, newlib, libgcc) is not decompiled:
`config/library_ranges.txt` lists its ranges (`0xSTART 0xEND label`, one per line, no overlaps) and `progress.py`
counts it apart, per library. Pick functions to decompile outside those ranges; [text_map.md](text_map.md) maps
the whole `.text`.

## Naming

### Symbol files

`config/symbols/` has one file per library (`libc.txt`, `libkernl.txt` ...), one per provisional game module
(`game_<module>.txt`, [game_overview.md](game_overview.md) section 4) and `data.txt` for everything after
`.text`. Each file starts with its address ranges, `// range: 0xSTART 0xEND` (end excluded, one or more lines);
ranges never overlap, so every address has one file. Entries are in splat's format, one per line:
`Name = 0xADDRESS; // type:func` (or `// type:data size:0x10`, ...), with `// guess` last when the name is
inferred. An entry stays in the file whose range holds its address, even when its prefix names another module
(module boundaries are provisional; a comment says so). Library files keep the order of the library's code,
game files the address order.

splat reads the files listed in `symbol_addrs_path` of `config/SLUS_200.01.yaml`; `configure.py` stops when that
list and the folder differ, so a new file goes in both. `tests/test_symbols.py` checks the ranges, that every
entry lies in its file's range, that library files match `config/library_ranges.txt`, and that names and
addresses are unique.

Add or change names with `.venv/bin/python scripts/apply_names.py OLD NEW` (OLD is a placeholder, a name or an
address; `--dry-run` shows the changes). It renames the entry where the address already is, or adds it to the
file whose range contains the address (an address no range covers is refused: add a `// range:` line or a new
file first), refuses a name used for another address, and replaces the placeholders and the old name in `src/`
and `include/` (whole identifiers, not struct members, not inside strings). ninja then splits again and the
assembly takes the new names. `scripts/names_from_strings.py` prints names suggested by the strings a function
references (`"sceGsSyncPath: DMA ..."`); check each one, a string can also name a callee.

### Rules

- **Placeholders.** `func_XXXXXXXX` (function), `D_XXXXXXXX` (data) and `jtbl_XXXXXXXX` (jump table) are splat's
  names for unnamed symbols, with the address in upper-case hex. Keep them until there is evidence for a name.
- **Game code: `Module_VerbObject`.** A short PascalCase prefix for the system the function serves (`File_`,
  `Snd_`, `Bgm_`, `Gfx_`, `Pad_`, `Mode_`, `Rng_`, `Movie_` ...), then a verb and its object: `File_LoadSync`,
  `Snd_StopAllSe`, `Gfx_WaitVSync`, `Pad_GetPlayerInput`. Use `Get`/`Set`/`Is` for accessors and `Handler` for
  callbacks (`Gfx_VSyncHandler`); a game mode handler is `Mode_<Screen>` (`Mode_Title`). Reuse an existing
  prefix before adding one. The prefix names the system, not the file, so a name survives a module boundary
  moving.
- **Library code keeps its own names.** Sony SDK functions use the SDK name (`sceCdRead`, `sceGsSyncPath`,
  `sceVu0ApplyMatrix`), kernel syscall stubs the EE kernel call (`_` in front when a C wrapper uses the stub),
  newlib and libgcc their source names (`rand`, `__divdi3`). Never give library code a game-style name; an
  internal library function with no known name stays a placeholder.
- **Data.** Globals are `g` + PascalCase (`gFrameCount`, `gFileTable`, [game_overview.md](game_overview.md)
  section 5). Size and type go in the attributes (`// type:data size:0x18`) once known.
- **Structs.** A struct whose role is unknown is `UnkXXXXXX`, XXXXXX being the address (six hex digits) of the
  global or function that shows its layout (`Unk8BD0A0` for the struct at 0x8BD0A0); unknown fields are
  `unkXXX` by hex offset (`unk254`). Rename both once the role is Verified (`GfxState`, `vsyncCount`).
- **`// guess`** marks an Inferred name: taken from strings the function references, its callers, the call graph,
  the library's API or its position in link order, not from reading what it does. Remove it when the name
  becomes Verified.
- **Verified** names: the binary states the name (the function's own debug or error string, a syscall number, a
  hardware register it drives), or its code was read and does what the name says, or it was decompiled to
  matching C that shows it. Anything else is Inferred. The docs use the same two labels
  ([README.md](README.md)).

## Compiler

| | |
|---|---|
| Compiler | Sony ee-gcc `2.9-ee-991111-01` (decomp.me archive `ee-gcc2.9-991111-01`; `cc1` SHA-1 `4af299f2cebfa6043e876b34af7f8173d99c43c7`) |
| Flags | `-O2 -fno-strength-reduce -ffast-math` |
| `-G` | `-G8`, the compiler's default (see "Small data" below for the open part) |
| Language | C (the driver's `-lang-c`; the preprocessor is built into `cc1`) |
| Driver use | `ee-gcc ... -S` only; the project's gas assembles the output behind `include/gcc_prelude.inc` |

The executable was built on 2000-11-01 with Sony libraries 1.6.x, yet its code comes from a compiler dated
1999-11-11: the 2.96 compiler of the same SDK generation (`2.96-ee-001003-1`) does not produce it.

### What identifies it (2026-10-18)

Five game functions were written in C and compiled with every Linux ee-gcc of decomp.me, assembled with the
project's gas and compared instruction by instruction with the original. "diff" is the number of differing
instructions with `-O2 -fno-strength-reduce -ffast-math` (`-G8`, `-G0` for the last one); `-` means the output
does not even assemble.

| Function | Shape | 990721 | 991111 | 991111-01 | 991111a | 2.96-ee-001003-1 |
|---|---|---|---|---|---|---|
| `func_00340000` | `x * 0x30` index, store loop, call | 11 | 0 | **0** | 0 | 20 |
| `func_00340AF0` | `sqrtf(dx*dx + dy*dy) <= r` | - | 9 | **0** | 9 | 0 |
| `func_00343278` | busy-wait on a register | - | 0 | **0** | 0 | 2 |
| `func_00341AE8` | five calls, tail call | 0 | 0 | **0** | 0 | 5 |
| `func_00342360` | table lookup with a default | 0 | 0 | **0** | 0 | 13 |

Only 991111-01 matches all five. The evidence, from the most to the least specific:

- **Two nops in front of every `div.s` and `sqrt.s`.** All 268 `div.s` of the executable have them. 991111-01
  emits them for both; 991111 and 991111a (which reports the version 991111-01 too, with the same function
  sizes in its `cc1`) omit them and move `div.s` / `sqrt.s` into delay slots; 990721 emits `c.ole.s`, which the
  R5900 does not have.
- **Multiplication by a constant with `mult`**: `x * 0x30` is `li 0x30 / mult` (the R5900's three-operand
  `mult`) in every 2.9 compiler; 2.96 turns it into shifts and adds.
- **Loops are not strength-reduced**: `for (i = 0; i < 27; i++) a[i] = v;` keeps the index (`sll / addu /
  slti`). Plain `-O2` reverses it into a down-counting pointer loop, so the game was built with
  `-fno-strength-reduce`. `-O1` gives the loop shape but no scheduling, which the original has.
- **`sqrtf` without the errno fallback**: the original has a bare `sqrt.s`; without `-ffast-math` the compiler
  adds a `c.eq.s` check and a call to `sqrtf` (24 differences). One function so far; confirm on more float
  code.
- Short loops padded with nops by the compiler (`lw / andi / nop / nop / nop / bnez`, the R5900 short-loop
  erratum), `move` written as `daddu`, `beql / break 7` division traps, functions aligned to 8 with a nop: all
  as the 991111-01 output.

Also tried: plain `-O2` (8 differences in `func_00340000`, the reversed loop), `-O1` (no scheduling: 16, 2, 0,
4 and 7 differences), `-O3` (same output as `-O2` on these five), and `-G0` against `-G8` (no change for the
first four).
The decomp.me archives `ee-gcc2.95.2-273a`, `2.95.2-274`, `2.95.3-*` (SN Systems builds) and `mwcps2-*`
(Metrowerks) are Windows programs and were not run: the section layout (`.lit4`, `.reginfo`, `.DVP.ovlytab`)
is Sony's GNU toolchain, and the 991111-01 matches leave no room for them.

The other compilers stay in `tools/compilers/` when installed for such checks; the build uses only 991111-01.

## Assembler prelude (include/gcc_prelude.inc)

The compiler leaves part of the encoding to the assembler. The game's assembler (1999-2000) behaved like the
modern gas in nearly everything, including filling delay slots in reorder mode and the hazard nop after an FPU
compare (`c.le.s / nop / bc1t` is in the original). Four instructions differ, each checked against original
bytes:

| Compiler writes | Original | Modern gas | Prelude |
|---|---|---|---|
| `move $4,$5` | `daddu` | `or` | macro to `daddu` |
| `break 7` | code in bits 6-15 (`0x000001CD`) | code in bits 16-25 | `.word` |
| `cvt.w.s $f2,$f1` | function 0x24 (`0x460008A4`) | refused for the R5900 | `.word` |
| `sqrt.s $f12,$f12` | operand in the ft field (`0x460C0304`) | operand in fs | `.word` |

The prelude is also read in front of `INCLUDE_ASM` code. `break`, `cvt.w.s` and `sqrt.s` become data, so gas
never moves them into a delay slot; the compiler only emits them inside `.set noreorder` blocks or where nothing
moves, so this has not mattered yet.

The output is assembled with `-mabi=o64`: with `-mabi=eabi` gas expands address macros (`la`, `addu` with an
immediate) to `daddiu`, the original has `addiu`. The ABI tag is then set to EABI64 (`configure.py --eabi64`).

## Small data (`-G`)

Both kinds of access exist close together. `func_00340070` reaches `D_005F6780` in `.sdata` and
`func_00342E38` its float constants in `.lit4` through `$gp`, which needs `-G` of at least 4 for the compiler
and the assembler. `func_00342360` stores to `D_0046B254` in `.data` with `lui / sw`: with `-G8` an
`extern int` would be reached through `$gp`, so that file was built with `-G0` or declared the variable with a
type larger than 8 bytes (or without a size). `-G8` is the build default; `G_FLAGS` in `configure.py` sets
another value for a path prefix once a file shows which one it needs.

## Matching notes

- Float constants fold bit-exactly in the compiler: `1.0f / 2046.0f` and `1.0f / 32767.0f` give the original
  `.lit4` words.
- The order of independent loads and stores comes from the source: `func_003403C8` (reads three floats of a
  struct, writes them out in another order) and `func_00342E38` (stores to VU0 memory) differ only in that so
  far.
- A store to the same global in both branches of an `if` matched only as one store after the `if/else`
  (`v = cond ? a : b; g = v;`), not as two stores (`func_00342360`).
- `sqrtf` needs a prototype (`float sqrtf(float);`); with `-ffast-math` the compiler inlines it.

## Decompiled so far

| Function | File | Notes |
|---|---|---|
| `func_00340000` | `src/340000.c` | fills 27 floats from a 0x30-byte record and calls `func_00357258` |
| `func_00340AF0` | `src/340000.c` | point within a radius |

`src/340000.c` covers 0x340000-0x340B30; the nine functions between those two are `INCLUDE_ASM`.
