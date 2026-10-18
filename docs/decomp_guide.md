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
`Name = 0xADDRESS; // type:func` (data: `// size:0x10`, or `// type:s32 size:0x4` with a splat type or a
capitalised struct name; splat rejects `type:data`), with `// guess` last when the name is
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
  section 5). Size and type go in the attributes (`// size:0x18`, `// type:PadPort`) once known.
- **Data tables stay in `INCLUDE_RODATA`; never transcribe game tables into C.** A table of game values (track
  lists, lookup tables, string tables, float tables) comes from the user's executable at build time: declare it
  `extern const` with its type and include it with `INCLUDE_RODATA("asm/nonmatchings/<file>", D_XXXXXXXX);`
  where the compiler would have emitted it. A table only one C function reads needs
  `force_not_migration:True` (and its `size:`) in `data.txt`, or splat folds it into that function's asm file
  and writes no file to include. C keeps only code, the constants the compiler makes from it (float literals,
  jump tables) and format strings inside functions.
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
| `mtc1 $4,$f0` / `cvt.s.w $f0,$f0` | no nop between | hazard nop | macro to `.word` (T13) |
| `mfc0 $2,$12` / `and $2,$2,$3` | no nop between | hazard nop | macro to `.word` (T13) |
| `move $4,$5` | `daddu` | `or` | macro to `daddu` |
| `break 7` | code in bits 6-15 (`0x000001CD`) | code in bits 16-25 | `.word` |
| `cvt.w.s $f2,$f1` | function 0x24 (`0x460008A4`) | refused for the R5900 | `.word` |
| `sqrt.s $f12,$f12` | operand in the ft field (`0x460C0304`) | operand in fs | `.word` |

The prelude is also read in front of `INCLUDE_ASM` code. `break`, `cvt.w.s`, `sqrt.s`, `mtc1` and `mfc0` become
data, so gas never moves them into a delay slot (for `mtc1` that is what the original shows); the compiler only emits them inside `.set noreorder` blocks or where nothing
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

T13 (`main`, `rng_misc`, `pad`, `file`, `gfx`) needed no `-G` other than 8. Every global there is reached through
`$gp` when it is a scalar of up to 8 bytes in `.sdata`/`.sbss`, and with `lui` otherwise, with two patterns:

- `D_005F5C10` lies in `.sdata` but `Sys_BootIop` takes its address with `lui / addiu`: declare it as an
  incomplete array (`extern s32 D_005F5C10[];`), which is never small data.
- The four draw-pass words at 0x5F6B00 are read through `$gp` at offsets 0, 4, 8 and 0xC: four separate
  4-byte variables (`gDrawFighterCount`, `gDrawStagePass`, `gDrawModelPass`, `gDrawEffectPass`), not one
  16-byte array (which `-G8` would reach with `lui`).
- Tables in `.data`/`.rodata`/`.bss` of 8 bytes or less still need an incomplete or larger declaration
  (`extern u64 D_00518BF8[];`), or the link fails with `relocation truncated to fit: R_MIPS_GPREL16`.

## Matching notes

- Float constants fold bit-exactly in the compiler: `1.0f / 2046.0f` and `1.0f / 32767.0f` give the original
  `.lit4` words.
- The order of independent loads and stores comes from the source: `func_003403C8` (reads three floats of a
  struct, writes them out in another order) and `func_00342E38` (stores to VU0 memory) differ only in that so
  far.
- A store to the same global in both branches of an `if` matched only as one store after the `if/else`
  (`v = cond ? a : b; g = v;`), not as two stores (`func_00342360`).
- `sqrtf` needs a prototype (`float sqrtf(float);`); with `-ffast-math` the compiler inlines it.

- `fdiff.py` prints OK only for identical instruction words (a relocated call or branch target, `~`, counts as
  a difference) whose `$gp` loads and stores reach the same bytes and, where both sides name the address, the
  same symbol: a float literal one ulp off (`3.1415925f` gives 0x40490FDA, the original 0x40490FD8 is
  `3.141592f`) or two globals swapped at the same offset now fail. Data reached with `lui` (`.rodata` tables)
  is not compared; `cmp -l build/SLUS_200.01 iso/SLUS_200.01` gives those addresses.

### Lessons from T13 (2026-10-18)

Objects and data:

- The `main` segment uses `subalign: null`, so each object keeps its own section alignment, as in the original
  link: `.text` objects start at any multiple of 8 (`main.c` at 0x357358 right after crt0, `file.c` at
  0x397948, `gfx.c` at 0x398510). With splat's default `SUBALIGN(16)` a boundary had to be a multiple of 16.
- A C file that emits `.rodata` (jump tables, string literals, `static const` tables) needs a `.rodata`
  subsegment (`[offset, .rodata, name]`, with the dot) at the object's first rodata byte. splat then moves each
  function's own rodata into its `INCLUDE_ASM` file, and a symbol used by several functions or only by `.data`
  (the file table, the IRX path strings) is included with `INCLUDE_RODATA` at its place in the file.
- The compiler aligns jump tables to 16 (`.align 4`); a jump table that comes from an `INCLUDE_ASM` file is only
  8-aligned: put `RODATA_ALIGN16();` in front of the `INCLUDE_ASM` (`Pad_Update`, `File_Update`). Objects are
  not padded at the end: the assembly chunk after a C file's `.rodata` starts where that file ends (0x509E38
  after `pad.c`), and a C file's `.lit4` holds only its own constants (`[0x2B619C, .lit4, gfx]`).
- Function-local `static const` tables are emitted in `.rodata` in front of their function, so their
  `INCLUDE_RODATA` goes right before that function (`D_00509D18` before `Pad_ReadPort`). A table of 8 bytes or
  less is small data under `-G8`, so a small table the original addresses with `lui` belongs to a larger object
  (the actuator table is the tail of the 0x18-byte player/port struct `D_00509D00` in `pad.c`).

Code shapes:

- Tail calls (`j f` after the epilogue) come only from void functions calling void functions:
  `return f(x);` in a function that returns a value is a `jal` plus the epilogue. A wrapper that returns
  what it calls (`s32 f(s32 x) { return File_LoadSync(x); }`) keeps its frame.
- `volatile` keeps a store out of the compiler's delay slots (`gFrameReady = 1` before `j $31`, since the flag
  is cleared by the VBlank handler), and a `volatile` struct member is reached as `lui / addiu / lw 0x254(reg)`
  instead of `lw %lo(gGfx+0x254)` (`Gfx_GetVSyncCount`).
- A constant that needs `lui / ori` and is subtracted inside a loop shows as `subu` from a hoisted register only
  when it comes from a variable (`s32 skip = 0xCF7 * 0x800; ... size - skip`, `File_RegisterIopFiles`);
  written inline it becomes `addu` of the negated constant.
- `slt $v1, $v1(0x7FF), $a0` + `movz` is `n = 0x800; if (size < 0x800) n = size;`; a ternary or
  `if (size > 0x7FF) size = 0x800` folds to `slti` (MIN_EXPR).
- A value stored to a field and then compared is read once only through a local (`type = rdata[1];
  port->type = type; if (type == 0x53)`); otherwise the store forces a reload.
- Two loops with the counter in different registers come from two counter variables (`i` / `j`,
  `Pad_ReadPort`).
- A struct copy of 16 bytes is two `ld`/`sd` pairs; the original's `lq` / `sq` plus a halfword store that stays
  behind it comes from a union of `u128` and `u16[8]` (`Dma_AddRefTag`): through two unrelated pointer types
  the scheduler moves the `sh` above the `sq`.
- Different registers for the same values in the two arms of an `if` (no cross-jumped tail) come from
  block-local variables in each arm (`func_00398858`); stores in each arm are emitted rotated (source
  `p[6]; p[4]; p[2]` gives `0x10, 0x30, 0x20`).
- `beq`/`bne` ladders before a jump table: a `switch` whose cases are laid out in source order; to get the
  original layout, list the cases in the order of their code (`Mode_SetDrawFlags`: `case 4, 8, 11` before
  `case 5`). `sltiu` on the selector means an unsigned parameter.
- A register that is not v0 for a value copied after a call (`lhu v1` / `sh v1`) means the call's return value
  is still live: the function returns it (`func_00398600`).
- `di / sync.p / mfc0 / and / nop / bnez` is three separate `__asm__` statements in a `do { } while
  (status & 0x10000)` loop (the compiler counts each asm statement as one instruction for the short-loop nops);
  interrupt handlers end with `__asm__("sync.l"); __asm__("ei");` and a second `ei` lands in the `j $31` delay
  slot, because the compiler emits `j $31` in reorder mode.
- `func_00398548` (fixed-point sqrt) matches only with `-fno-schedule-insns -fno-schedule-insns2`: the constant
  4096.0f is loaded after the `sqrt.s`. Its neighbour `func_00398588` is scheduled, so the flags were not set per
  file; the cause is open.

Tools:

- splat 0.50 rejects `// type:data` (`Unrecognized symbol type`); `apply_names.py` writes new data entries with
  no type, to be completed by hand with a splat type (`s32`, `u16`, ...) or a capitalised custom one
  (`PadPort`).
- A permuter run on `Pad_Update` (two hoisted `lui 0x51` in swapped saved registers) found nothing better than
  the base in 150,000 iterations.

## Decompiled so far

| File | Range | C / functions | Notes |
|---|---|---:|---|
| `src/340000.c` | 0x340000-0x340B30 | 2 / 11 | `func_00340000`, `func_00340AF0` |
| `src/main.c` | 0x357358-0x358A38 | 33 / 40 | boot, main loop, frame phases, mode dispatch, input state |
| `src/rng_misc.c` | 0x35C2B0-0x35D048 | 42 / 47 | RNG, string and table helpers, play counters |
| `src/pad.c` | 0x396D00-0x397948 | 10 / 11 | libpad/libmtap set-up sequence, per-player input |
| `src/file.c` | 0x397948-0x398510 | 27 / 29 | file requests, descrambling, IOP status |
| `src/gfx.c` | 0x398510-0x3994C0 | 42 / 51 | GS set-up, VBlank handler, display packet, helpers |

Functions left as `INCLUDE_ASM` carry their closest attempt above them in `#if 0` with a `NON_MATCHING:` note.
