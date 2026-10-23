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
   moved only because something before it changed size. It stops when ninja fails (the exit status decides: a C
   file that does not compile prints no `error`); `--no-build` skips the build but refuses an out-of-date image
   (`ninja -n` has work to do). Several runs may overlap (a lock in `build/fdiff.lock`, private temporary files).
3. For a near miss (registers, instruction order), put the attempt back in `#if 0` and run
   `.venv/bin/python scripts/permute.py src/<file>.c func_XXXXXXXX` (setup in `permuter/<Func>/`, the
   original from `asm/nonmatchings/`); add `--run` to start decomp-permuter (threads: `-jN` / `-j N`, else
   `$COOL_JOBS`, else every core). Run heavy commands (ninja, configure.py, the tests, permuter runs) through
   `scripts/cool.sh`, and only one permuter at a time. A score of 0 ignores some differences: apply the candidate by
   hand and check it with fdiff.
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
| Flags | `-O2 -fno-strength-reduce -ffast-math -fno-strict-aliasing` (nine objects without `-ffast-math`, `NO_FAST_MATH` in `configure.py`) |
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

**`-fno-strict-aliasing`** (T13 batch 2). This compiler turns strict aliasing on at `-O2`, but the game was built
without it: `Gte_ApplyMatrix` (0x39B1E0) reloads the 16-bit input vector after every store to the 32-bit output
vector, which only happens when the two may alias. Every C file of batch 1 compiles to the same assembly with and
without the flag, so it is set for the whole build in `configure.py`.

## Assembler prelude (include/gcc_prelude.inc)

The compiler leaves part of the encoding to the assembler. The game's assembler (1999-2000) behaved like the
modern gas in nearly everything, including filling delay slots in reorder mode and the hazard nop it emits after an
FPU compare when gcc's next line is `.set noreorder` (`c.le.s / nop / bc1t` is in the original). These differ, each
checked against original bytes:

| Compiler writes | Original | Modern gas | Prelude |
|---|---|---|---|
| `mtc1 $4,$f0` / `cvt.s.w $f0,$f0` | no nop between | hazard nop | macro to `.word` (T13) |
| `mfc0 $2,$12` / `and $2,$2,$3` | no nop between | hazard nop | macro to `.word` (T13) |
| `mfc1 $2,$f1` / `subu $2,$0,$2` | no nop between | hazard nop | macro to `.word` (T14) |
| `move $4,$5` | `daddu` | `or` | macro to `daddu` |
| `break 7` | code in bits 6-15 (`0x000001CD`) | code in bits 16-25 | `.word` |
| `cvt.w.s $f2,$f1` | function 0x24 (`0x460008A4`) | refused for the R5900 | `.word` |
| `sqrt.s $f12,$f12` | operand in the ft field (`0x460C0304`) | operand in fs | `.word` |
| `li.s $f20,2048.0` / prelude `mtc1 $0,$f24` | no nop between | hazard nop before the data word | `lui $1` + prelude `mtc1` (`fix_compiler_asm`, T17 b4) |
| `c.lt.s $f0,$f1` / `bc1f` left to the assembler | no nop between | hazard nop | compare as `.word` (`fix_compiler_asm`, T17 b4) |
| `nop / nop / sqrt.s` (noreorder) / `jal atan2f` | `sqrt.s` in the jal's delay slot | data word, `nop` in the slot | `jal` moved before it (`fix_compiler_asm`, T17 b4) |

The last three rows need the neighbouring lines, which a gas macro cannot see: `configure.py --fix-asm` rewrites the
compiler's output before it is assembled (build.md, "C files"). The prelude is also read in front of `INCLUDE_ASM` code. `break`, `cvt.w.s`, `sqrt.s`, `mtc1`, `mfc0` and `mfc1` become
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

- `fdiff.py` takes the build side only from the symbol table of `build/SLUS_200.01.elf` (address and size of each
  function) and the original side only from names: the address a `func_XXXXXXXX` name or `config/symbols/*.txt`
  gives, never the build address. A function with neither is refused (add it to its symbol file); an address on the
  command line is an original address, and the original function ends at the next original function start.
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

### Lessons from T13 batch 2 (2026-10-18)

Order of statements:

- **Write stores in their natural (field) order.** The scheduler moves the last store of a run of independent
  stores to the front, so the assembly order is not the source order: `mc->unk1C = a; mc->unk48 = b; mc->op = 0x11;
  mc->unk10 = c;` comes out as 0x10, 0x1C, 0x48, 0x0C (`func_0035EB20`). Clearing a struct field by field, then the
  next one (`func_003455D0`, `func_00343F88`, `func_003456B0`, `Spr_Setup`), matched once written in field order;
  copying the assembly order did not. When the order still differs, a permutation of the statements finds it fast.
- An argument computed from a call (`rand()`) is evaluated after the plain field loads of the other arguments;
  when the original loads `f->unk1E` / `f->unk2C` after the `rand()` call, the source put the value in a local
  first (`code = t[...]; Snd_PlayVoice(f->unk1E, f->unk2C, code);`, `func_00376410`).

Calls and returns:

- A call whose value is dropped at the end of a function becomes `j f` (tail call), whatever the return types;
  `return f(x);` keeps the `jal` and the epilogue. Several sound functions are non-void and return what they call,
  with no value on the other paths (`s32 func_00374D90(...) { if (...) { ... return Snd_PlaySeEx(...); } }`, the
  K&R style of implicit `int`); write them that way rather than as `void`.
- A `return;` in the middle of a non-void function stops the tail call of the last call (`Snd_PlaySeEx` got a `jal`
  until the early exit was a jump to the end of the function).
- A function that returns the value of a request only on one path is `s32 r = f(); if (r == 0) {...} return r;`
  (`func_0035EA08`); with `void` the compiler schedules the prologue differently.
- A pointer to a global set before a call is kept in a saved register only when it is a copy of another local
  (`McState *state = &gMc; ... sceMcInit(); mc = state;`, `Mc_Init`); `McState *mc = &gMc;` alone is rebuilt with
  `lui / addiu` after the call. With the pointer used after a call (`SprDma *s = &gSprDma; f(); s->state`), the
  address stays in `$s0` (`func_0035DC20`).

Types and constants:

- `x & 0xFFFFCFFF` (an unsigned literal) is built with `lui / ori`; `x & ~0x3000` (an `int`) with
  `addiu -12289` (`func_0035E8D8`).
- `lhu` on a signed field means the field is unsigned or only copied; `lh` for comparisons and arithmetic. An
  argument loaded with `lhu` is an unsigned 16-bit field (`SndDelayed.id`).
- `volatile`: a struct shared with an interrupt handler (`gSprDma`, its `pending` counter) is declared volatile; then
  its address is built in a register (`lui / addiu`) and every field access stays in place (`func_0035DEF0`,
  `func_0035DF68`, `func_0035E610`). For hardware registers, `*(volatile u32 *)0x1000A000` is reached through a
  register built with `lui / ori`; a non-volatile `*(u32 *)0x1000A000` gets the assembler's absolute form (`lui $1,
  0x1001` + `-0x6000`) but its loads can be merged.
- A table of one entry still calls `rand()` for its index: `tbl[rand() % 1]` and `tbl[((rand() & 0xFF) * 1) >> 8]`
  compile differently (scalar load after the call against a saved base address); `func_003769D0` uses both forms.
- `seq[(s16)f->unk166]` is `sll 16 / sra 14` (index sign-extended, times 4).

Objects and data:

- A C file that emits a jump table makes its `.rodata` 16-aligned, so the object's `.rodata` starts on a 16-byte
  boundary. `sound`'s first table could not be `D_00500758` (0x500758): that table and the four functions that read
  it (0x374520-0x3745E8, now the end of `src/battle_flow.c`) belong to the object before, and `sound.c` starts at
  `Snd_Init` with its `.rodata` at 0x500780. All of `sound`'s `.rodata` tables have `force_not_migration` and are
  included in place; the jump tables of functions still in assembly are included too
  (`INCLUDE_RODATA(..., jtbl_005007B0)`), so the order holds whichever functions are C.
- A table of 8 bytes or less in `.data` still needs an incomplete declaration (`extern s32 D_00415FE0[];`), or the
  compiler reaches it through `$gp` (also for `D_00500CB0`-`D_00500CB8`, 4-byte words in `.rodata`).
- A struct handed to a function at an offset of its parent (`p + 0x20`) is a member reached through a pointer
  (`Unk3456B0Sub *s = &p->sub;`), not fields of the parent: the compiler then keeps `p + 0x20` in a register.

Not matched yet (closest attempts in `#if 0`):

- Identical `switch` case bodies are cross-jumped into one; the original keeps four copies (`Snd_AllocVoice`).
- Scratchpad words reached by absolute address in every statement (`func_00342FB0`), register allocation
  (`Snd_PlaySeEx`, `func_0035E8D8`, `func_0035D238`), a loop that copies the scratchpad header with four `ld`/`sd`
  (`func_0035D588`, `func_0035D608`), DMA register waits that reload by absolute address (`func_0035E6B8`,
  `func_0035E758`).

Tools:

- Short permuter runs (15-25 minutes, `-j2`-`-j4`) found matches for store order and register allocation
  (`func_00374B48`, `Snd_StopAllSe`, `Snd_PlayVoice`, `func_00376150`, `func_0035E388`, `Mc_Init`); their output is
  noisy (dead stores, `do {} while (0)`), so each was rewritten as plain C and checked again. `func_00376150`
  matched with a mask variable (`s32 mask = 0xFFF; seq = *p & mask;`).

### Lessons from T13 batch 3 (2026-10-18)

Hardware registers and waits:

- A wait on a DMA register whose load stays inside the loop in the assembler's absolute form (`lui v0, 0x1001` /
  `lw v0, -0x3000(v0)` on every pass) is a non-volatile access in a loop written with `goto` (`wait: if (*(u32
  *)0x1000D000 & 0x100) goto wait;`, `func_00343178`): a `while` loop lets loop.c hoist the load, and `volatile`
  builds the address in a register with `lui / ori`.
- The BC0F wait in front of a toSPR transfer is inline assembly (`1: nop` x5, `bc0f 1b`; gas fills the delay slot);
  with the register writes after it inside `do { } while (0)` (a macro of the original) the registers come out as
  in the original (`func_00343128`).
- Interrupt-side code ends with `__asm__("nop"); __asm__("sync.l"); __asm__("ei");` when a `nop` precedes the `sync`
  (`func_0035E1C8`); handlers that return 0 need `return 0;` inside the `if` as well as at the end, or the
  `move v0, zero` is scheduled differently (`func_0035E228`, `func_0035E2E8`).
- After stores through the `volatile` gSprDma pointer, the original loads 0x1000D010 (absolute form) in place; ours
  moved in front of the stores, because the scheduler knows that a pointer set once to `&gSprDma` cannot alias a
  constant address. decomp-permuter found `s++; s--;` (the pointer is then set more than once and its base is
  unknown): `func_0035DB90`, `func_0035DDE0`, `func_0035DE68` match with it, marked `fake`. The original probably
  reached the struct in a way the compiler could not trace; `func_0035DA78` and `func_0035DC58` still differ.

Switches and jump tables:

- A switch with a range of cases that do the same as `default` keeps them as cases: `case 3: default:` gives the
  original's `slti 3 / beqz` ladder (`func_0035E2E8`); cases `-7 ... -3` that only `break` (`func_0035F180`).
- If-converted returns (`movn`) instead of branches: write `if (c) return x; break;` per case rather than `return c
  ? 0 : x;` (`func_003755C8`); a small switch inside an `if` gives the original `beq` ladder where two `if`s do not.
- A C file that now emits a jump table itself loses the padding word the jump table's `.s` file carried: put
  `RODATA_ALIGN8()` (include/include_asm.h) in front of the next `INCLUDE_RODATA` (`D_00500CB0` after
  `func_003757B8`).
- `memcard` and `dma_gif` got `.rodata` subsegments (0x4FBFC0-0x4FC280, 0x519340-0x5193B8). splat writes the file
  of a rodata symbol only when the C file names it in an `INCLUDE_RODATA` line, so add the lines (and the
  `force_not_migration` entries) before running `configure.py`. `asm/` is git-ignored: search it with `fd -I`.

Calls and declarations:

- A call whose argument register is set but that the callee ignores: the original called it with an argument.
  Define the callee without a prototype (`void *func_00345780() { ... }`) so the caller can pass one
  (`func_00346030`).
- The return type of an SDK prototype changes register allocation in the caller: `s32 sceSifLoadFileReset(void)`
  (not `void`) matched `Sys_BootIop`.
- Stack slots of address-taken locals follow first use; a local copied from an address-taken one
  (`func_00343CA8(&n, &segs, id); count = n;`) lives in a saved register as in the original.

Accepted fakes from decomp-permuter, marked `fake` in the source: an empty `do { } while (0)` in
`func_00398548` (keeps the 4096.0f load after `sqrt.s`), the address of a local taken in `func_0035D790`, the
`s++; s--;` above. Other permuter finds were natural once rewritten: a block-local copy (`Gfx_KickDisplayChain`,
`File_Update`), a chained assignment (`Gfx_SetDisplayOffset`), the declaration order of two locals
(`func_0035C698`), a local loaded before a store (`func_0035C8D0`, `Str_ComparePrefix`).

Assembler: `mfc1` got a hazard `nop` from gas that the original does not have (`func_00346230`); the prelude has an
`mfc1` macro since T14 (the clean rebuild stayed byte-identical and `func_00346230` matches).

### Lessons from T14 batch 1 (2026-10-18)

- **Check that the build really ran.** A C file that fails to compile (conflicting prototypes) or to link (a `.data`
  byte reached through `$gp`) leaves the old `build/SLUS_200.01` in place, and `fdiff.py --no-build` then reports OK
  against stale bytes. After a batch, confirm ninja ended with the SHA1 step before trusting any OK. (F01: fdiff.py
  now stops on a failed build, and `--no-build` refuses an out-of-date image.)
- A function whose size changes moves every later function, `.data`, `.sdata` and `.bss` symbol: in a batch, wrap the
  size-changing misses first, rebuild, then judge the rest (their diffs were only relocations).
- `(fl & 0x400000) == 0` as a value compiles to `sra / xori / andi`; the original's `lui / and / sltiu` comes from
  `r = 0; if (!(fl & 0x400000)) r = 1;`. Flag words compared that way are `s32`.
- An extra `move a0, s0` in front of a call whose first argument is already in `a0` comes from an inline function:
  the getter `static inline struct FighterMove *Fighter_GetMove(Fighter *f)` reproduces it (`func_0037DDE0`,
  `func_0037DE40`). gcc 2.9 at `-O2` inlines only functions marked `inline`.
- Duplicated `return 0` blocks in the original come from nested `if`s that each end in `return 0;`
  (`func_0037E090`); identical tails that end the same way are cross-jumped into one (`Prac_PlayStep`).
- A narrow parameter (`u16`, `s16`) is masked or sign-extended by the callee; a caller that extends an argument with
  `sll / sra` while the callee does not passes `(s16)i` to an `s32` parameter (`func_0037D798`).
- Clamps: `n > 0x1FF ? 0 : n` gives `slt / movn`, `n <= 0x1FF ? n : 0` gives `movz`; each function uses one form.
- A struct of two `u16` copied as a whole is `lwl / lwr / swl / swr` (`Prac_RecSaveTrack`).
- `fighter_core` emits no `.rodata`, `.lit4` or `.sdata` of its own (no floats, switches kept as ifs), so one C file
  covers the whole range without data subsegments; functions with jump tables stay in assembly for now.
- Not solved: a padding `nop` that 8-aligns the label after a mid-function `jr ra` in 7 functions of the executable
  (`func_0037DFC8`); the compiler emits `.p2align 2` there.

### Lessons from T14 batch 2 (2026-10-18)

- **splat writes `asm/nonmatchings/<file>/<func>.s` only for the names the C file lists in `INCLUDE_ASM`.** A label
  splat calls `jlabel func_XXXXXXXX` (a jump-table target that looks like a function, 0x36BF8C and 0x36BF94 after
  `func_0036BF58`) needs its own `INCLUDE_ASM` line, or its bytes are missing and the link fails with an undefined
  reference from the jump table. List every `glabel` and `jlabel` of the range when creating a C file.
- A store chain `a->x = a->y = v;` stores `y` first; the original order of two such stores shows which one is outer.
- A call left as `jal` at the end of a function (no tail call) means the function returns that value:
  `s32 f(...) { ... return g(); }` (`Battle_UpdateFrame`), or `if (...) { return g(); }` with no value on the other
  path (`func_00368B08`).
- A value copied to a field and passed on (`lhu` / `sh` / `sll`+`sra` for the call) is a local of the unsigned type:
  `u16 id = c->unk6; f->unk184 = id; f(id)` (`Fighter_StartCommandMove`).
- A global's address passed as an argument while the other accesses go through a pointer register
  (`lui / addiu D_00879B5C`) is `&gFighters[0].unk51C` written out; fdiff accepts the different symbol name for the
  same address.
- A `.data` byte read by C (`D_0043B7F8`) must be an incomplete array (`extern u8 D_0043B7F8[];`) or the link fails
  with `relocation truncated to fit: R_MIPS_GPREL16`.
- A fixed mask kept in a variable (`u16 mask = 0xF;`) changes the register order of an `&` (`func_0036CD78`, found by
  decomp-permuter in a few minutes).
- Not solved, all store / block scheduling: the stores of a move record pointer to 0x188 / 0xC0 / 0xB8 next to
  constant stores (`func_00368748`, `func_00368818`, `func_00368958`), two `return 1` blocks kept apart
  (`func_0036BF00`), a loop entered at its test (`func_0036A008`), an argument shifted with `dsrl`
  (`func_0036C918`). decomp-permuter matches two of them only with fakes (`m++; m--;`, `if (f) return 1; else
  return 1;`), which were not taken.
- `fdiff.py` runs must not overlap: several at once on the same build report wrong sizes and false differences.
  (F01: they shared `build/fdiff/*.bin`; each run now disassembles through its own temporary file and builds and
  reads the build under a lock, so parallel runs are safe.)

### Lessons from T14 batch 3 (2026-10-18)

Objects and data:

- `battle_flow` and `fight_misc` got their own `.rodata` subsegments (0x4FF8D0-0x500780 and 0x506150-0x506260) and
  `fight_misc` a `.lit4` one (0x5F5170, 4 bytes). As for `sound`, every table, string and jump table of the range
  has `force_not_migration` in `data.txt` and an `INCLUDE_RODATA` at its place in the C file, so the order holds
  whichever functions are C. A jump table the C now emits loses the padding word of its `.s` file: the next
  `INCLUDE_RODATA` needs `RODATA_ALIGN8()` (after `func_0036C680` and `func_00378398`); a table whose size is already a
  multiple of 8 needs nothing.
- splat did not migrate the small 4-byte tables `D_00506248`-`D_0050625C` into their functions even with a single
  reader: the link failed with undefined references until they got `force_not_migration` and an `INCLUDE_RODATA`.
- A `.lit4` subsegment is only possible once its functions are C: an `INCLUDE_ASM` function does not carry its float
  constant (`D_005F5170` stayed undefined until `func_00378760` was C). `battle_flow` therefore has no `.lit4`
  subsegment yet (`func_003720C8` owns its two words).
- Data order checks object edges: `func_003790C8` reads `.data` at 0x42F270, after the practice object's own data
  (0x42F140-0x42F268), so it opens the practice object; the `fight_misc` C file ends at 0x3790C8.

Code shapes:

- A float constant used as a divisor stays a `div.s` only when it comes from a variable assigned after the calls
  (`turn = 6.2831855f; ... / turn`, `func_00378760`): written as a literal, `-ffast-math` folds `x * 65536 / 2pi` into
  one multiplication; assigned before the calls, the constant is kept in a saved FPU register.
- Arguments: `atan2f(b[2] - a[2], b[0] - a[0])` evaluates its second argument first, as the original does; with
  locals computed in order the subtractions come out swapped.
- `return f(x);` in every arm of a switch: identical `jal` + epilogue tails are cross-jumped unless the arms differ in
  the delay slot; the original kept a `do { } while (...)` loop's `return` separate when written as `for (;;)` with
  `continue` (`func_0036C318`), and `cur == f()` rather than `f() == cur` for the operand order of `beq`.
- A result variable distinct from the parameter (`s16 r = kind; switch (kind) { ... r = ... }`), with the cases in
  the order of their code, matched `func_0036BE08`; the same switch with the parameter reassigned put it in a saved
  register.
- A switch with four cases 1-4 compiled to a `beq` ladder; the original's jump table has a fifth entry for 0, so the
  source had `case 0: break;` (`func_00378150`, `func_00378398`). A range test `x < 1 || x > 4` (else branch first)
  gave the original's `addiu -1 / sltiu 4 / bnezl`.
- `(s16)(f->unk3C - 0x8000)` gives `lhu / addiu -0x8000 / sll / sra` only when the field is `s16`; on a `u16` field
  the compiler adds 0x8000 from a hoisted register. A `u32` parameter turns `-0xAAA` into `lui / ori`.
- Three-store sequences of a `void` function that look like a returned pointer (`move v0, a3`) were three plain
  array stores: `v0` was just a temporary (`func_00378368`, found from a decomp-permuter result that compiled the
  function as `void`).
- Block-local variables again: a value stored to two fields in one `case` (`func_0036C680`, case 3) matched only as a
  block-local, not as the function-scope variable also used by another case.
- `r = a && b && c;` gives the original's `move r, zero` in the delay slots of each failing test (`func_00369E10`,
  cases 0x60 and 0x65).

Not matched yet (closest attempts in `#if 0`): register allocation (`func_0036C278`, `func_00369E10`,
`func_00378E70`), the fighter addresses rebuilt in the middle of `func_0036D5D0` (two more saved registers for their
`%hi` parts), the block order and copies of `func_00376CF8` (3 instructions short).

Tools:

- `ninja` output contains `error` only for some failures: a C file that does not compile ("structure has no member")
  leaves the old build in place, and fdiff then reports OK. Check that the SHA1 step ran (or the exit status) after
  every change. (F01: fdiff.py goes by ninja's exit status and checks that the image is up to date afterwards.)

### Lessons from T14 batch 4 (2026-10-18)

Objects and data:

- A C file can cover a few functions in the middle of a large assembly range when it emits no data of its own:
  `[0x82470, c, select_results]` / `[0x82A78, asm]` (0x3C1470-0x3C1A78, two of its four functions in C) and
  `[0x94288, c, model_anim]` / `[0x943F0, asm]` (one function). Such a cut is not evidence for an object edge.
- `battle_flow` got its `.lit4` subsegment (`[0x2B6168, .lit4, battle_flow]`, 0.6f and 1.4f) in the same change that
  made `Fighter_ApplyDamage` C; the float literals in the source produce the original words.
- A jump table the C now emits (`Fighter_SetPushRadii`, `jtbl_005061E0`) drops its `INCLUDE_RODATA`; the next table is
  emitted by C as well and is 16-aligned, so no `RODATA_ALIGN8()` was needed.

Code shapes:

- `lbu` loads of fields that other functions read with `lw` are `s32` fields narrowed to a `u8` parameter: the team
  member's character and costume (`TeamMember.unk0` / `unk4`) are `s32`, and the narrowing explains the separate
  `lui / addiu` of `D_00440940` and `D_00440944` for each argument in `Fighter_ApplyDamage`.
- The original reuses one temporary for unrelated values: the defender's own damage and the extra damage share `n`
  with the scripted damage (registers `a2` as in the original only that way, `Fighter_ApplyDamage`).
- `for (i = 0, p = a; i < 2; i++)` instead of `p = a; for (i = 0; ...)` gave the original prologue and loop entry
  order (`move s6, zero` before the pointer copy).
- A clamp placed after the join of two assignments (`if (c) t = x; else t = y; t = t < 0 ? 0 : t;`) keeps its `-1`
  local; inside one branch CSE reuses a `-1` from before the calls (`func_00378810`, `func_00378A28`).
- A value assigned in both branches and then copied (`up = 110` / `220`, `sy = up`) gives the original `move s5, s3`
  (`func_00378A28`).
- A fighter address recomputed after a call (`mult side * 0xB8A0` again) is a second pointer variable
  (`Fighter *g = &gFighters[side];`, `Battle_RecoverLife`); reassigning the first one keeps it in its saved register.
- An index read before the stores that change it is a local taken first (`s16 n = f->unk2;` then `f->unk2 ^= 1`,
  `Fighter_TagSwap`).
- A `switch` whose decision tree tests `== 0` first, then `> 0`, then `== -1`, has a third case above 0 that shares
  the default code: `case -1: ... case 0: ... case 1: default: ...` (`Fighter_ApplyDamage`); with only `-1` and `0`
  the tree tests `-1` first.
- `if (bit) { if (w != 0) { ... } else on = 1; } else on = 1;` reproduces one shared `on = 1` block placed after the
  chain, reached from a `beql` delay slot (`Fighter_SetPushRadii`); `((u16)kind >> 12) & 1` gives `srl` where
  `(kind >> 12) & 1` used the value before the sign extension.

Accepted fakes from decomp-permuter (marked in the source): `do { d->unk12A = 1; } while (0);` gives the constant 1
its saved register (loop weight) and one index through a reused variable moves a register (`Fighter_ApplyDamage`);
the address of a local (`pa = &a`) fixes the order of a table value and the rounding `-1` in `func_00378A28` and
`func_00378E70` (which was NON_MATCHING since T14 b3).

Not matched yet (closest attempts in `#if 0`): `func_00378810` (the store to `unk14` before the one to `unk18`; every
other instruction matches), `Fighter_Push` (338 of 490 instructions, register allocation and order),
`Fighter_StepMove` (an m2c draft with fixed types, 1484 instructions instead of 1546).

Tools:

- m2c needs the jump table file next to the function (`m2c ... Fighter_StepMove.s jtbl_004FFA80.s`), or it stops at
  the `jr`.
- A long permuter run lost its folder under `build/permuter/`. Neither splat nor `configure.py` removes anything
  under `build/` (a re-split only empties `asm/`); the clean rebuild (`rm -rf build`) does. F01 moved the folders to
  `permuter/<Func>/` (ignored by git), which a re-split and a clean rebuild leave alone.
- `pkill -f 'decomp-permute[r]'` also kills its own shell when the same command line names `decomp-permuter/...`
  (starting the permuter in the same command): run the `pkill` alone.

### Lessons from T14 batch 5 (2026-10-19)

Code shapes:

- **`abs()` is the compiler's builtin** (`s32 abs(s32 x);`): `x = abs(x)` gives `bltzl x / negu x` in place, `abs(a)`
  into another variable `bgez / move / negu`. A written `if (x < 0) x = -x;` is if-converted to `slt / negu / movz`
  instead (`func_00370ED0`, `Fighter_StartLaunch`).
- **Store runs**: the scheduler emits the last store of a run of independent stores first: source `[s1 ... sn]` gives
  `sn, s1, ..., sn-1` (`func_0038A690`: `5C, 50, 4C, 6C` in the source for `6C, 5C, 50, 4C` in the assembly). A chain
  `a = b = v` stores `a` first. Field order in the source is usually right; when it is not, try the rotation.
- A result variable returned at the end (`r` in a saved register, `move v0, s2` before the epilogue) needs `goto end`
  for the early exits: `return r;` in the middle lets the compiler substitute the known constant (`func_0036CDD8`,
  `func_003847D8`).
- A load hoisted above an `if` in the assembly was read before the test in the source (`len = f->unkC0->unk18;` first,
  `func_003738C0`); a value loaded once and used by later tests after stores is a local (`frame = f->unkC4;`,
  `func_003737B0`).
- `x - 0x8000` on a `u16` operand narrows to `li 0x8000 / addu`; on an `s16` operand to `addiu -0x8000`. A sign
  extension done as `(w << 16) + 0xC0000000 >> 16` is `(u16)w + 0xC000` (`func_00370F28`).
- Range tests: `a == 0x39C` then `a == 0x39D` kept as two branches is a `switch` with a `case 0x39C: break;`
  (`func_0038B698`); `beq 1 / slti 2 / beq 2 / beq 3` trees come from four cases with `case 0: break;`.
- A three-byte `== 0` test merged into one `ld` and a mask (`func_0036CA68`) needs the struct holding the bytes to be
  8-aligned: `FighterUnk438` got `__attribute__((aligned(8)))` (no other function changed).
- `if (x) { ...; return 1; } return 0;` against `if (!x) return 0;`: the block placed first in the assembly is the
  one written first; flip the test when the return blocks are in the other order (`func_00383D10`, `Cmd_MatchInput`).
- A small local array initialised from `.rodata` (`ldl / ldr / lb` into the stack) is a struct copy from an `extern
  const` of the right size (`Dir9 t = D_004FF8D0;`, `func_00369188`): the data stays in the split.

Not matched yet (closest attempts in `#if 0`): `Fighter_StepMove` (394 of 1546 instructions differ, store order
inside the cases), `Fighter_StartLaunch` (the launch: two branches moved out of line), `func_00370F28` (register
allocation), `Cmd_CheckCondition` (one `sltiu` cross-jumped), `func_0036CAF8` (a peeled loop entry), and a few register
allocations listed in their notes.

Tools:

- `scripts/permute.py` wrote an `objdump.sh` that stopped at the first symbol after the function, and splat's `jlabel`
  jump-table targets are symbols: for a function with a jump table the permuter compared only its head. (Fixed in T14
  b6: `<.L...>` label lines neither end nor appear in the scored disassembly.)
- Decomp-permuter's score 0 can hide one differing instruction (`func_0036F010`): check with fdiff.
- For a large function, trying the orders of each run of plain stores (a few hundred orders per run, scored against
  `target.o`) converged faster than the permuter (`Fighter_StepMove`, 494 to 394 differing lines in fdiff).

### Lessons from T14 batch 6 (2026-10-19)

Objects and data:

- The jump tables of `Fighter_GetContinueFrame`, `Cmd_CheckSituation`, `Battle_UpdateRound` and `Fighter_RunMoveEvents`
  now come from the C; their `INCLUDE_RODATA` lines went away and no `RODATA_ALIGN8()` was needed (the next tables
  start on 16-byte boundaries). The two `jlabel` pieces `func_0036BF8C` / `func_0036BF94` are case blocks of
  `Fighter_GetContinueFrame` and left with its `INCLUDE_ASM`.
- `fighter_core`'s `.rodata` (0x5064C0 on) interleaves with the practice object's (`asm/3A0C8.s` reads up to
  0x506BC0): a subsegment needs both objects' tables, about 200 symbols. Until then only its functions without
  `.rodata` are decompiled (52 of the 81 left at the start of the batch).

Code shapes:

- **A call result stored to several fields** is a chained assignment: `f->unkC0 = m = func_00368218(f, id);` then the
  other stores of `m` (`func_00368818`, `func_00368748`, `func_00368958`, open since T14 b2). For three stores of one
  value the order was `f->unk1A = f->unk3C = f->unk40 = v`.
- **The return type of a callee changes the caller's registers even when the value is dropped**: `func_003B5DB0`
  returns 1, and declaring it `s32` instead of `void` fixed the last two lines of `Battle_UpdateRound` (found by
  decomp-permuter as `volatile char`).
- A jump table that starts at 0 with the default block placed first: `case 0: default: return 0;` at the top of the
  switch (`Fighter_GetContinueFrame`).
- A field tested and then reloaded for the switch (`lbu` twice) is read once into a local for the test (`u8 flags =
  c->unk3; if (flags & 0x80)`, then `switch (c->unk3 & 0x7F)`, `Cmd_CheckSituation`, from a decomp-permuter hint).
- Operand order of a comparison picks registers: `D_005F5C28 <= unk4` / `D_005F5C28 >= unk4` let two cases share their
  compare tail where `unk4 >= D` did not.
- Adjacent fields tested together are merged by the compiler: `f->unk3E8 != 0 || f->unk3EC != 0` is one `ld`,
  `f->unk160 == 0 && f->unk162 == 0` one `lwu` with a DImode compare (`dsll32 / dsra32`); write the plain tests.
- `if (bit) g = 0x5A; else g = 0xF0;` (a global stored in both arms) keeps the branch; through a local the compiler
  makes a `movn`. `if (--g < 0) g = 0;` keeps the counter in one register for the later test.
- A pointer to an options array passed to a call (`u8 *opt = D_0043B7F8; func_003AE440(opt); opt[0x1F]`) reproduces
  the original's mix of `0(s2)` and `%lo(D_0043B7F8)(s3)` accesses (`Battle_UpdateRound`).
- A remainder after a fix-up of the quotient (`q = n / 32; if (q == 0) q = 3; r = n % 32;`) gives the original's
  `move` copy of `q` before the `movz` (`Fighter_RunMoveEvents`).
- A 12-byte position copied from a joint array: field copies through an overlay struct with the array as a member
  (`((FighterJoints *)f)->unk668[arg].unk0`); a struct copy comes out as `ldl / ldr`.
- A `do { } while (p->x != 0)` behind an `if (p->x != 0)` is cross-jumped into one test; a `while` loop (rotated by the
  compiler) keeps the original's separate entry test (`Fighter_RunMoveEvents`).
- `Unk8B8010 *d = &D_008B8010;` with `d->unk14[i]` gives `addu base, index` (`func_0037EA08`); `k = (rand() & 3) + 1;`
  before `gCpu->unk34C[k]` keeps the `gCpu` load after the call (`func_00385250`).

Not matched yet (closest attempts in `#if 0`): `Battle_UpdateMovement` (draft, about 720 lines), `Cmd_CheckThrowCondition`
(the per-case `r = 0`), `func_0037EE18` (3 lines), `func_0037FA78` (12 lines), `func_0038A408` (13 lines: `>= 0` as `nor /
srl`, a hoisted `r = 1`).

Tools:

- `scripts/permute.py`: the scored disassembly covers the whole function (jump-table labels skipped), `-j N` is accepted,
  and the default thread count is `$COOL_JOBS` (from `scripts/cool.sh`), else every core.
- decomp-permuter's zero scores for `func_0036BF00` and `func_0036F010` were fakes again (`if (x) return 1; else
  return 1;`, a temporary for the constant 3); its best for `func_00368818` (score 20) was the natural chained assignment.
- `func_003713F0` matched as written once the header gained fields used by other functions (its permuter base score was
  0): re-check old NON_MATCHING drafts with fdiff after struct changes.

### Lessons from T14 batch 7 (2026-10-19)

Objects and data:

- `fighter_core` got its `.rodata` subsegment (`[0x1C74C0, .rodata, fighter_core]`, 0x5064C0-0x507EE0, 216 symbols with
  `force_not_migration`). The compiler emits file-scope tables at their declaration (checked with a test file: statics
  are not deferred), so `.rodata` follows the source order of functions and tables. The practice object of
  `asm/3A0C8.s` therefore runs past 0x37BF48 (its menu strings and tables at 0x506528-0x506BF0 come after the
  constants of `func_0037BB70`-`func_0037BF48`) and ends before 0x37C370. A C file's `.rodata` must be one contiguous
  range, so the object's tail stays in `fighter_core.c`, its tables included in place like the others.
- The `INCLUDE_RODATA` lines were placed by a script: each symbol in front of its first reader, clamped so that the
  address order holds against the jump tables a function will emit once it is C (a table after a jump table goes
  after that function). splat writes no file for symbols at odd addresses (`D_00507452` ...): they stay inside the file
  of the symbol before, so they get no `INCLUDE_RODATA` line.
- A C jump table drops the padding words of its `.s` file: the next included jump table needs `RODATA_ALIGN16()`
  (`jtbl_00506D20` after `func_0037DA08`). fdiff does not see this; check the SHA-1 after each new C jump table.
- A table whose code builds the address of its second field (`%lo(D_00507A88)` with loads at -8 and -4) starts 8
  bytes earlier: the handler table is `D_00507A80`.
- A data change re-runs `configure.py` and the split from the current sources; an attempt left enabled at that moment
  loses its `.s` file, and reverting the C then fails to assemble. Run `configure.py --split` after reverting.

Code shapes:

- **Inline functions of the source**: the same switch on move ids in ten CPU functions is one `static inline`
  (`Cpu_IsCmdAllowed`), the same `||` test in two (`Cpu_IsOpponentBusy`, materialised into a register as a return
  value), and the seeded random (`func_003800D8`) is inlined where its factor sits in a saved register over the
  `rand()` call (`Cpu_RandBelow`). Inside the inline, cases that `return` a value go straight to v0, cases that set
  the result variable go through the copy at the end.
- A predicate shared by several functions that the original tests with branches only (no 0/1 value in a register) is
  a macro, not an inline function: `CPU_CMD_FOR_VARIANT` (`func_003856C0`, 11 lines left); written as a `static inline`
  returning `a || b || c` it was materialised into t0 first.
- Store order again: two stores before a branch come out in the reverse order of the source (`unk130 = 0; unk12C = 0`
  for 12C then 130), three as the rotation `sn, s1, s2` (`func_00381B10`, `func_00388DE0`, `func_00385C80`).
- Narrow loads: `(u16)p->w == C` on a `u32` field compiles to `lhu` (combine narrows the masked load); a union
  `{u32 w; u16 h;}` was used for `CpuState.unk330` / `unk338` / `unk33C`, where both widths appear.
- A loop that walks a table with a pointer next to its index (`addiu a1, 8` and `addiu a0, 1`) needs both in the
  source (`for (i = 0, e = tbl, n = -1; ...; i++, e++)`, `func_0037C370`); the place of `n = -1` in the for-init put
  its `li` into the delay slot.
- Range tests that the original keeps as two branches are nested `if`s (`if (d < 0x1E) { if (d >= 2 ...`,
  `func_00389B70`); `if (n < 0x1E && n >= 2)` folds to `addiu / sltiu`.
- A dispatcher of value ranges is a `switch` with `case 6: ... case 10:` (gcc builds `slti` range tests and places
  the case blocks in source order, `func_0037DC98`); written as `if`s the blocks came out in another order.
- A boolean computed before an unrelated early exit (`hit = (rand() & 0xFFF) < gCpu->unk35E; if (gCpu->unk94 > 0 ||
  !hit)`) keeps the original's `slt` ahead of the test (`func_0038BC00`).
- `memset` with the standard prototype is expanded inline (`sd zero`); the original calls it. A prototype with an `s32`
  length (`void *memset(void *, s32, s32)`) is not the builtin and gives the call (`func_0037BC98`).

Not matched yet (closest attempts in `#if 0`): `func_0038A408` / `func_0038A500` / `func_0038AD00` (2 lines: `!(x < 0)`
becomes `nor / srl`, the original `slti / xori`), `func_0038A780` (3), `func_0037BC98` (2, `lh` for the screen
position), `func_00381278` (13), `func_00388170` (13), `func_00380B10` (about 150, register names only),
`func_0038A928` (72, mostly offsets), and others listed in their notes. `Fighter_StepMove`: 394 to 368 lines with one
marked fake from decomp-permuter (the constant 8 in a variable, `fl = 8; /* fake */`); `Battle_UpdateMovement` stays at
about 724 (decomp-permuter: 28480 to 27185 in its score, only by a change of the meaning, not taken).

Tools:

- `fdiff.py` only compares the function: a C jump table can still shift the next table; always confirm the SHA-1
  step after enabling a function that emits `.rodata`.

### Lessons from T14 batch 8 (2026-10-19)

Code shapes:

- **The command-list walkers set the entry count in both arms**: `if (e->unk0 == 0x8004) { cnt = e->unkD; c = ...; }
  else { cnt = 1; c = e; }`. With `cnt = 1;` hoisted above the `if` the count took a later saved register in every
  walker (`func_003856C0` and `func_00386388`, NON_MATCHING since T14 b7, matched with this alone).
- **The order of initialised locals picks saved registers**: `func_003835A0` matched only with `n = 0` declared after
  the other four initialised locals (a sweep of the 120 orders found it in a minute).
- **A constant compared through a variable or an inline parameter** keeps the comparison the expander chose for a
  register: `func_003853E8() >= 0` written in place folds to `nor / srl`, through `static inline s32
  Cpu_PlayPicked(s32 min)` called with 0 it stays `slti / xori` (the `!(x < 0)` trio, found by decomp-permuter as an
  inline with a parameter); `unk94 <= 0x23` gives `slti 36`, `unk94 <= lim` with `lim = 0x23` the original
  `li 35 / slt` (`func_00388840`). Both are marked fake.
- `D_005F5EE8 == (gRoundNumber == 1 ? 0x3D : 1)` loads the frame before the round test, as the original
  (`func_0036EC50`); `gRoundNumber == 1 ? t == 0x3D : t == 1` loads it in each arm.
- A shared "fail" block placed between the success code and an early test comes from a `goto` into the failing `if`
  (`if (m->unk2C != 0 && (...)) { fail: ...; return 0; }`, `func_00387600`, `func_00387DA8`); a shared `return 0` placed
  after the last test likewise (`if (k < 0) { fail: return 0; }`, `func_00387120`).
- A duplicated block of the original (the candidate test four times in `func_00382AD8`) is a macro of the source; its
  `ok` flag matched as a block-local of the macro (`{ s32 ok = 0; ... }`), and a stale pointer used after the loop is
  the last value of the loop variable.
- `(c->unk3 & ~0x80) < 0x60` gives `slti`, `(c->unk3 & 0x7F) < 0x60` `sltiu` (`func_003691D8`).
- An `extern const s64` table read into an `s32` is `ld / dsll32 / dsra32` (`D_005078E0`, `func_00388FA8`).

Objects and data:

- `func_00388840` emits its jump table: `jtbl_005078B0` left the source and `D_005078E0` needed `RODATA_ALIGN8()` (the C
  table is 0x2C bytes, the `.s` file had a padding word); fdiff said OK before the SHA-1 step caught it.
- `CpuState` gained the arrays the walkers fill (`unk154[0x30]`, `unk228[10]`) and typed `unk10` (move records by id)
  and `unk90`; `Fighter` gained `unk10` / `unk14` (the origin shift of `func_00372E98`).

Not matched yet (closest attempts in `#if 0`): `func_00385DF8` (142 lines, the original spills `id` next to the result
and keeps two constants in fp), `func_00388FA8` (87 lines, mostly offsets: one cross-jumped early return and two
if-conversions; decomp-permuter went from 2795 to 2290 in about 20 minutes), `func_00373E38` (49 lines, a block placed
between the switch cases), `func_00380B10` (decomp-permuter: 2515 to 960 in about 50 minutes, no natural change found).

Tools:

- A by-hand sweep that writes each permutation of a few statements (initialisers, a run of stores) into the source and
  runs `fdiff.py` (about 0.5 s per build of `fighter_core.c`) found several matches faster than decomp-permuter
  (`func_003835A0`, `func_00387600`, `func_003879A0`, `func_00387DA8`, `func_003685B8`).

### Lessons from T14 batch 9 (2026-10-19)

Code shapes:

- **`n > K` as a value keeps `li K / slt` only through a jump on GT**: `if (n <= K) r = a; else r = b;` gives the
  original's `li K / slt / movn` (the MIPS `sgt` expander forces a non-zero constant into a register);
  `r = n > K ? 0 : a` and `if (n > K) r = 0;` are canonicalised to `slti K+1` with `movz` (`func_00388530`).
- A flag set from a bit test is `if (x & 0x100) stop = 1;` for the original's `andi / sltu`; `stop = (x & 0x100) != 0`
  becomes a bit extract `srl / andi` (`func_0037FB70`, `Battle_UpdatePause`).
- A switch tree `beq 1 / slti 2 / bnez` over the cases 0 and 1 comes from three cases with `case 2: break;`, written
  in the order 0, 1, 2 (the order puts case 0's block first, `func_0037CDA0`).
- Two stores before a shared tail again: `s->unk0 = 4; s->unk4 = 0xB4;` stores unk4 first, which lets the case
  cross-jump into another case's identical `unk4 / unk0` tail (`func_0037FB70`).
- A 0x80 / 0x88-byte struct copy with a run-time alignment test (`or / andi 7`, an `ldl / ldr` loop beside an
  `ld / sd` loop and an `ldl / ldr` tail) is a copy through a 4-aligned struct type (`struct { s32 w[0x22]; }`), not
  through the 8-aligned `FighterUnk438` (`func_0037E1D8`, `func_00380120`).
- `Cpu_RandBelow` multiplies `n * x` like `func_003800D8`: with a variable `n` (`w - 4` in `func_00384D78`) only that
  order gives the original `mult v0, s0, v0`; the callers with constants compile the same either way.
- Small local arrays with initialisers have their constants in `.sdata` (`D_005F5EC0`, `D_005F5EC8`, 8 bytes, reached
  with `lui / addiu` and copied with `ldl / ldr`): declare them `extern s32 D_xxx[];` and copy them as a struct
  (`func_0036F0D8` matched first try).
- `func_0037E1D8` matched only with the second search written as a comma expression inside the test (marked fake);
  the plain `if` forms keep the `a | b` result in a saved register.

Objects and data:

- New C jump tables: `func_0037FB70` (`jtbl_00507290`) and `func_0037FE58` (`jtbl_005073A0`), both ending on a
  16-byte boundary, so nothing was added; the `.s` files of tables
  that stay with drafts keep their `INCLUDE_RODATA` lines next to the `#if 0` block.
- `Fighter` gained `unk5C`-`unk62`, the blend fields `unkB390[18][16]`, `unkB5D0` / `unkB5D4`, `unkB630[18][16]`,
  `unkB888` / `unkB88C` / `unkB890` / `unkB891`; `FighterUnk18C` the reaction move ids at 0x1C-0x36 (`func_0036A050`).
  No other function changed.

Not matched yet: every function of `fighter_core`, `battle_flow` and `fight_misc` now has C; the new drafts are
listed with one-line reasons in their `NON_MATCHING` notes ([systems/fighter.md](systems/fighter.md) section 5.13).
The closest: `func_0037EB00` (3 lines, one register), `func_0037C1B8` (7, the two flags built from one constant),
`func_00384210` (11), `func_0037D0F8` (12), `func_0037CDA0` (25, an s0 / s1 swap).

Tools:

- For a sweep, score the fdiff output without relocated branch targets and `$gp` offsets: a size change moves every
  later target and data address, so the raw "lines differ" count hides real progress.
- decomp-permuter: `func_00388FA8` 2795 to 2125 in about 25 minutes (by moving a `mask = 7` store into another branch,
  a change of meaning); `func_00385DF8` 2715 to 695 in about 15 minutes, only through a `(double)` cast of a call
  result (a fake, not taken); `Fighter_StepMove` 7765 to 7635 in about 15 minutes with a new temporary (not taken).

### Lessons from T15 batch 1 (2026-10-19)

Objects and data:

- The asm chunk after `fighter_core` (0x38BF98-0x396D00) is one object, `src/stage.c` (`[0x4CF98, c, stage]`), and its
  `.rodata` is the whole chunk 0x507EE0-0x509D00 up to `pad`'s (`[0x1C8EE0, .rodata, stage]`, 99 symbols with
  `force_not_migration`). The `INCLUDE_RODATA` lines were placed by script in front of each symbol's first reader, clamped
  to address order; tables read only from `.data` (`D_00508BA0`-`D_005095A0`) sit between their neighbours.
- When a function with a jump table becomes C, its `INCLUDE_RODATA` of the table must go in the same change, or the link
  fails with undefined `.L` labels from the included table. `Cam_Update`'s two tables (`jtbl_00507F70`,
  `jtbl_00507FC0`) needed no `RODATA_ALIGN`.
- The file's `.lit4` words (0x5F5174-0x5F519C) belong to four float functions; they stay in the generic `.lit4` chunk
  while those functions are assembly (`LIT4_WORD` would allow a subsegment before all four are C).
- An 8-byte `.bss` pair (`D_008BC710`) is small data under `-G8`: declare it with its real, larger struct
  (`Unk8BC710`, 0x28 bytes) or as an incomplete array, or the link fails with `R_MIPS_GPREL16`.

Code shapes:

- **`-(n - 0x1D)` is folded to `0x1D - n` (`li / subu`)** by fold-const for an unsigned `n`; the original's `addiu -29 /
  negu` comes from `-(n - 0x1E + 1)` (`func_00393C78`): the two constants fold into a PLUS_EXPR, which negation keeps.
- **`x >= 0` as a value** gives `nor / srl`; the original's `slti x, 0 / xori 1` came from comparing with the result
  variable that holds 0 (`r = 0; ... r = x >= r;`, `func_00393810`, marked fake), the same mechanism as the T14 b8 inline
  parameter.
- **A dropped call result still sets registers**: declaring `func_0038C1E8` `s32` instead of `void` made the load after
  the call use `v1` and matched `Cam_Update`; an unused third argument passed by the caller (`func_00396320(a, b,
  -1)`) must be in the prototype or the call is one instruction short.
- **Nested switches whose inner cases jump into the outer cases** are an inner `switch` of `goto`s to labels placed on
  the outer `case` bodies (`Cam_Update`); an outer range with a trailing `case 19: break;` gives the original's
  `sltiu 20`.
- Store order again: four stores in a branch came out as the original only with the first store of the asm written last
  (`func_0038D1B8`, `D_00433330` 0x3C / 0x38 / 0x00 / 0x40); a sweep of the 24 orders found it in about a minute.
- A counter stored back before its test (`n = D; n++; D = n; if (n >= 10) { D = 0; n = 0; }`) matched where `n = ++D`
  and `D = n = D + 1` did not (`func_0038C048`).
- `if (a > 0 || b < 0) { ... } return 1;` nested (m2c's shape) keeps a shared `return 1` block at the end; early returns
  duplicate it (`func_00393CB8`, still a draft).

Not matched yet: 36 compiled drafts with their differences noted, mostly store order and register allocation; 16 raw
m2c drafts (the fight camera `func_0038C1E8` and its orbit and throw functions of 680-1060 instructions, and the four
float functions), which do not compile yet ([systems/stage.md](systems/stage.md) section 4.5).

Tools:

- For a module with many small functions, a generator that rebuilds the C file from per-function snippets and a state
  list (C / draft / asm), plus a sweep that writes each variant, builds and runs `fdiff.py`, kept the build
  byte-identical between attempts (about 3 s per variant for `stage.c`).

### Lessons from T15 batch 2 (2026-10-19)

Objects and data:

- **A `.lit4` subsegment before its functions are C**: `[0x2B6174, .lit4, stage]` in the yaml, and `LIT4_WORD(D_005F5174,
  0x487FFF00);` (include/include_asm.h) inside the `#else` in front of each float function's `INCLUDE_ASM`, one per word
  in pool order. The clean rebuild stayed byte-identical; a function that becomes C drops its `LIT4_WORD` lines.
- **An 8-byte array the original reaches through `$gp` needs its size in its first declaration**: `extern s32
  D_005F6170[];` is never small data (reached with `lui`), and a later `[2]` redeclaration does not change that; with
  `[2]` at the top `func_0038EC08` went from 180 to 53 lines and `func_00390E08` lost one line. Declaring the per-fighter
  words `D_005F6150` / `D_005F6158` as `[2]` arrays instead of four scalars took `func_0038C1E8` from 8 to 4 lines.
- A field indexed by a fighter slot is an array member: `D_00433330.unk7C[slot]` (`s32 unk7C[2]`) gives the original's
  `&D_00433330` + 0x7C, where `(&D_00433330.unk7C)[slot]` folded the offset into the symbol (`func_0038C1E8`, 2 lines).
- An initialised local array (`s32 kind[2] = { x, y };`, built in a temporary and copied whole) is written with an
  array typedef (`S32x2 kind = { x, y };`) so that no `] = {` appears in the sources (the guard against transcribed
  tables); a struct wrapper (`struct { s32 v[2]; }`) compiled 4 instructions shorter.

Code shapes:

- **An aggregate initialiser gives the stack pair**: `U32Pair k = { a, b };` matched `func_0038F6B0` (the two values stored
  to a temporary and copied with `ldl / sdl`), where separate stores to a temporary pair and a copy took other registers.
- **Return types again**: `func_0038DC68` returns the distance it computes; with an `s32` prototype `func_0038E610`
  matched after one swapped store. `Cam_SetDistance`, `func_0038E6D0`, `func_0038FD90` and `func_00391358` end with
  `move v0, zero`: `s32` and `return 0;`.
- `x += f() - K;` keeps the original's `(x - K) + f()` (`addiu` on the loaded field, then the add); `x = x - K + f();`
  is reassociated into `x + (f() - K)` (`func_00396AB0`).
- `if (r > 0) return 0; if (r < 0) return 1; return 0;` keeps the original's `bgtz` and `slti`; every single-expression
  form (`r > 0 ? 0 : r < 0`, a result variable) was if-converted to `movn` (`func_003927F0`).
- A chained store `a[i] = b[i] = 0;` in a loop gave the original's store order with the base registers in source order
  (`Cam_Reset`); written as two statements, either the stores or the bases came out swapped.
- The clamp `d < lo ? lo : (d > hi ? hi : d)` gave the original's `bnel` / `movz` (`Cam_SetDistance`); the `if` forms
  branch the other way.
- Float functions need file-scope prototypes of `atan2f` and `sqrtf`: an implicit declaration is `double` and the link
  fails on `fptodp`.

Assembler limits (both solved in T17 batch 4 by `configure.py`'s rewrite of the compiler's output):

- gas puts a hazard `nop` between the `lui / mtc1 $at` it makes of `li.s` and a following prelude `mtc1` `.word`
  (`mtc1 $0, $f24`): `Cam_SetDistance` matches in every instruction but has this extra `nop`.
- The prelude's `sqrt.s` `.word` cannot fill a delay slot, but the original has `sqrt.s` in the delay slot of `jal
  atan2f` (`func_00392F80`, `func_003932F0`, `func_00393550`).

Not matched yet: the 47 drafts of [systems/stage.md](systems/stage.md) section 4.5. Three functions (`func_0038C1E8`,
`func_0038D290`, `func_0038D3D0`) differ only in where the store to `D_00433330.unk3C` lands next to the mode store
`unk0`, which every store order tried (all 120 / 24 orders of the blocks) left in place.

Tools:

- Several agents tested drafts against the one `src/stage.c` at the same time through a script that swaps the draft in,
  runs `fdiff.py` and restores the file under a file lock; the restore needs a `SIGTERM` handler, or killing a sweep
  leaves the file changed.
- decomp-permuter on `func_00395718` (2 lines) found only a change of meaning; on `func_0038D3D0` its best was score 20
  (from 50), not a match; on `func_00394300` nothing beat the base score in about 87,000 iterations (a few minutes).

### Lessons from T16 batch 1 (2026-10-19)

Objects and data:

- Two new C files cut out of large assembly chunks: `src/skeleton.c` (`[0xF748, c, skeleton]` / `[0x10F10, asm]`,
  0x34E748-0x34FF10, inside the `scene3d` range) and `src/motion.c` (`[0x249F0, c, motion]` / `[0x25448, asm]`,
  0x3639F0-0x364448, inside `fighter_setup`), with their own symbol files (`game_skeleton.txt`, `game_motion.txt`; the
  old modules got two `// range:` lines each). The clean rebuild stayed byte-identical with every function still in
  `INCLUDE_ASM` before any C was written.
- **Data order is not text order here.** The skeleton functions' `.sdata` (0x5F5E50-0x5F5E78), `.lit4` (0x5F5114-0x5F5154)
  and `.rodata` (`D_004FC668`) sit between those of `fighter_setup`'s 0x3625B0-0x3645D8 and 0x366DB0- parts, although
  their code is at 0x34E748: an object's data position cannot place its code. A `.lit4` subsegment still works for the
  cut (`[0x2B6114, .lit4, skeleton]`, `LIT4_WORD` for the float function left in assembly); `Skel_RotMatrixFromAngles`
  (C) emits its six words in place.
- A function with a jump table in a cut needs a `.rodata` subsegment before it can be C: without one the linker cannot
  resolve the table's `.L` labels once the function's `.s` file is gone. `motion` got `[0x1BD610, .rodata, motion]`
  (only `jtbl_004FC610`, `force_not_migration`, `INCLUDE_RODATA` in front of the decoder).
- Fighter offsets beyond what `Fighter` names are reached through overlay structs with the full slot size
  (`FighterSkel`, 0xB8A0 bytes, joints at 0x630; `FighterBlend`, a `GteMatrix` array at 0xB390): with a
  `GteMatrix`-typed view the blend copy `unkB390[i] = j->unk4` addresses `f + i * 32 + 0x8000 + 0x3390` as the original;
  the `s16 [18][16]` field of `Fighter` hoisted the whole 0xB390 instead.

Code shapes:

- **The original's angle conversion is a macro on a masked value**: `((a) & 0xFFFF) <= 0x7FFF ? K * ((a) & 0xFFFF) :
  (((a) & 0xFFFF) - 0x10000) * K` gives `lhu / li 0x7FFF / slt`; a `(u16)` cast is shortened by the front end to a sign
  test (`lh / bltz`), an int temporary changes the registers (`Skel_RotMatrixFromAngles`). The constant is `6.2831855f /
  65536.0f` (0x38C90FDB, folded exactly).
- **`x ^ C` against 0 on an `s16` global**: written in place the front end narrows it to `lhu`; the original's `lh /
  xori / bnez` came from an `s32` local initialised from the global in the declarations (`s32 mode = gMode;`,
  `Skel_ComputeWorld`).
- **A mask that is not folded**: `(m->unk24 & 0x7FFFF) & 0x10000` with both constants in registers is a bit field: the
  low 19 bits read through a `{ u32 lo : 19; u32 hi : 13; }` view (`func_0034FB48`); written with two `&` the tree folds
  them into one.
- **One loop variable for every loop** (`i` for the 3-step vector adds and for the sphere loops) gave the original's
  registers in `Skel_PlaceHitShapes`; a second counter took other registers in the small loops.
- **Re-reading a pointer field instead of a local**: `f->unkC0->unk28`, `f->unkC0->unk29` ... written each time (CSE
  merges the loads) reproduced the copies of the move pointer in `t0` / `a3` (`Skel_SetVolumeStarts`,
  `Skel_PlaceHitShapes`); a local `m` gave one register.
- Joints addressed as `SKEL(f)->unk630[2].unk24` (constant offsets from `f`) matched where a local pointer to the joint
  array kept a second base register.
- A clamp of a frame index loaded right after a call matched with the load placed before the NULL test of the call's
  result (`func_00364298`); `j--` on a parameter before two indexed loads reproduced `addiu a1, a1, -1` and a later
  `j - 1` as `addiu v0, a1, -1` (`func_00364390`).
- decomp-permuter (score 0 in under 10 minutes) matched `Motion_SampleRoot` and `func_00363BD8` with the `>> 12` branch
  computing the height into its own temporary first (`y2 = out[1] * scale / 0x1000; out[2] = ...; out[1] = y2;`); the
  temporary shared with the other branch gave other registers.
- `exit()` must be declared `noreturn`: the code after the call is the next block (`Motion_LoadBanks`).
- The EE passes a fifth argument in `t0` (EABI): `func_003640B0` tail-calls `func_003641C0(out, frame, i, n, clip)`.
- `-(x - 0x8000)` folds to `0x8000 - x`; `-(x - 0x8001 + 1)` keeps `addiu -0x8000 / negu` (marked fake in a draft).

Not matched yet (closest attempts in `#if 0`): `Skel_SetRootMatrix` (about 38 lines, registers of the float path),
`func_0034FBA0` (three multiplications of the joint index in the original), `Skel_ProjectJoints` (1 line: the loop's
alignment nop), `Motion_DecodeFrame` (59 lines, registers only; decomp-permuter: no natural change in 10 minutes), `Motion_CopyFrame`
(5 lines; decomp-permuter 10 minutes, only fakes). [systems/animation.md](systems/animation.md) describes what they do.

### Lessons from T17 batch 1 (2026-10-20)

Objects and data:

- **A callee-saved register keeps a 128-bit save mode for the rest of the file.** Once a function of a C file holds a
  `u128` in `s1`, this compiler saves `s1` with `sq` / `lq` in every later function of the same file, even where it holds
  an int (checked with test files). The original saves `s1` with `sd` again from `func_00350578`, so the six packet
  senders 0x34FF10-0x3504C0 end an object: `src/scene3d_b.c` holds them, `src/scene3d_c.c` starts at 0x3504C0, and
  `func_00350578` / `func_00351978` match only that way. `func_003517F0` / `func_00351860` keep the original's `sq` save
  of `s2` from `func_00350C40` (a draft), so they stay NON_MATCHING until it is C.
- **A `.lit4` pool out of text order**: `func_00351AA0`'s words (0x5F5614) come before those of `func_00350C40`
  (0x5F5650) although its code is later, and `func_003518B8`'s word (0x5F570C) sits after `render_fighter_a`'s. The
  subsegments follow the pool: `scene3d_c` (0x5F5614-0x5F5668) emits both functions' `LIT4_WORD` lines in front of
  `func_00350C40`; `func_003518B8`'s word stays in the generic chunk (it cannot be C in this file until that is solved).
  `scene3d_d` (0x5F5668-0x5F5700) and `scene3d` (0x5F5350-0x5F535C) are in text order.
- `src/scene3d.c` got a `.rodata` subsegment for one jump table and one 16-byte vector (0x51EA60-0x51EB00): the C jump table
  of `func_0034CA70`, then `RODATA_ALIGN16()` and `INCLUDE_RODATA(D_0051EAF0)` (`force_not_migration`). Data order again
  ignores text order: `scene3d`'s `.rodata` sits among `modes`', `scene3d_c`'s among `draw_list`'s.
- The HUD (0x3C1A78-0x3C3150) is a cut inside `select_results` (its `.rodata` strings follow `func_003C1350`'s jump table
  in text order), with its own symbol file `game_hud.txt`; `model_draw` is one object range with no data of its own.
- A struct that splat cut into several labels must stay one object: the light block `D_01FC4210` (0x340 bytes) matched
  only as one struct (`D_01FC4210.slot[i].cur.dir[2]`), with fdiff accepting `D_01FC4210 + 0x80` for splat's
  `D_01FC4290`; the same for `D_00440940 + 0x28` / `D_00440968` (`func_003C1B38`).

Code shapes:

- **12-byte vector copies** (`ld/sd` + `lw/sw`) are `memcpy(d, s, 12)` through 16-aligned pointer types
  (`typedef f32 FVec4[4] __attribute__((aligned(16)))`), often in a small inline (`CopyV3`); a struct copy gives
  `ldl / ldr`. Declare the aligned typedef once per file: a second aligned `f32[4]` typedef changed the alignment of
  earlier arrays.
- A full 16-byte copy whose address is recomputed per copy is an inline `*(u128 *)d = *(u128 *)s` (`func_00353E60`,
  `func_003531F0`).
- Columns written through a byte offset (`*(f32 *)((u8 *)x + o)` with `o = j * 4`) give the original `addu base, index`
  order (`Light_GetFighterMatrices`).
- GIF packet code matches as plain `*p++ = (u64)...` expressions; the zero extensions (`dsll32 / dsrl32`) show which
  parameters are `u32` (`func_003DC188` was 117 lines off with `s32` texture coordinates).
- `if ((v & 0xFF) == 0) return v >> 8; v += 0x100; return v >> 8;` keeps the original `bnezl` (rounding up to 256-byte
  blocks, `func_00354368`); an `if` around the add becomes `movn`.
- `Hud_DrawKoPercent` matched only with the constant 10 written twice (`up = 10; if (pa == 100) down = 10;`) and the
  comparisons on the fields; a dead `+= 100` before a call is kept by the original (`func_003C1A78`).
- decomp-permuter took `Hud_DrawMarkers` from 52 differing lines to 1 in 20 minutes: the first loop reuses the counter
  `ix` of a later loop, and one test goes through an inline (`return h->unk20 != 0;`, marked fake). The last line is
  `if (k == n) h->unk10 = n;` storing k's register where the original stores n's; no source order or type tried changes it.
- A switch with a jump table needs enough cases for gcc to choose a table: the `func_00350C40` draft has two filler
  cases (marked [I]) so that it emits the original's 0x68-entry `jtbl_00521E40`.

Compiler and assembler limits found (the `-ffast-math` and hazard-nop ones solved in T17 batch 4):

- **`-ffast-math` folds what `func_0034CC38` / `func_0034E148` keep**: `x + 0.0f`, `x / 12.0f * s` as `div.s` then `mul.s`,
  `x * 3.141593f / 180.0f`. Without the flag they come within 9 and 23 lines, but the same file's `func_0034E018` /
  `func_0034D258` have a bare `sqrt.s`, which this compiler emits only with `-ffast-math` (`-fno-math-errno` does not
  exist in it). Either another sqrt (inline asm, macro) without `-ffast-math`, or the functions come from different
  objects; a per-file flag experiment is open.
- **No hazard `nop` before an unfilled `bc1f`**: after `c.lt.s`, the original has no `nop` before a `bc1f` whose delay
  slot gcc left empty (`func_00352F50`, about ten places in `func_00351E98`, `func_00340528`); a compare before a filled
  or likely branch keeps it. The 1999 gas did not insert it in reorder mode; a prelude change is needed.
- The `li.s` / prelude `mtc1` hazard `nop` of T15 b2 again (`func_003518B8`), and a `.p2align 3` that the compiler puts
  before the label after a `b` where the original has none (`func_00353448`).
- The sword effect moves its 64-bit TEX0 value into a 128-bit register with `por`; this gcc refuses every u64 -> u128
  conversion ("unsupported wide integer operation"), so the draft goes through memory.

Tools:

- Helper agents compiled single functions in a scratch directory with the build's compiler and compared them with the
  split `.s` (objdump with relocations, branch targets relative to the function, `.lit4` and jump-table references
  normalised), so five of them could work at once without touching the build; the parent then rebuilt and ran fdiff.
  `li.s` literals from C show as `R_MIPS_LITERAL .lit4` relocations, which such a comparison must treat like the
  original's `R_MIPS_GPREL16 D_005F5xxx`.

### Lessons from T17 batch 2 (2026-10-20)

Objects and data:

- `effects` and `effects_draw` are one data run: `.rodata` 0x5194E0-0x519B30 and `.lit4` 0x5F51A4-0x5F531C with no gap at
  the module edge. `D_00519918` (read by `func_003A6A90`, 8-aligned) cannot start the `.rodata` of an object with jump
  tables, so 0x3A6460 is a cut: `src/effects.c` includes `D_00519918` at the end of its `.rodata` and `src/effects_draw.c`
  starts at `jtbl_00519940` (`[0x1DA4E0, .rodata, effects]`, `[0x1DA940, .rodata, effects_draw]`, `.lit4` at 0x2B61A4 /
  0x2B6300). The data before (`D_005193B8`, `D_005F51A0`) belongs to `geometry`'s 0x347xxx functions.
- Two C jump tables needed alignment: `RODATA_ALIGN16()` before `D_005197C0` (after the C table of
  `Fx_UpdateStageAmbient`), and `RODATA_ALIGN16()` at the end of `effects_draw.c` because the next object's `.rodata`
  (still in the generic chunk, whose jump tables are 16-aligned) starts at 0x519B30. fdiff said OK for every function
  before the SHA-1 step caught both; `cmp -l` plus the build addresses of the `D_` symbols show the first shifted table.
- Shared types for the effect objects are in `include/effect.h` (the pool object of `dma_gif`, its drawing state at
  +0x20); kind-specific state is an overlay per kind (`FxSparkSub`, `FxSubX` ...).

Compiler flags:

- **Both files look built without `-ffast-math`.** `func_003A35C8` keeps `x + 0.0f` (0.0 in a saved FPU register) and a
  `div.s` by 3.0f, and `func_003A8D08` has the errno expansion of `sqrtf` (`sqrt.s`, `c.eq.s`, `bc1t`, a call); both
  match without the flag except for hazard nops, and every other function of the two files compiles the same either
  way (checked on the 48 functions of two of the five groups). Without the flag the original's libcalls are `sqrt` (0x407818) and
  `__fixunssfsi` (0x40EBA0) where this compiler emits `sqrtf` and `fptoui`. A per-file flag in `configure.py` (like
  `G_FLAGS`) is the follow-up; `scene3d.c`'s float functions (T17 b1) may be the same case.
- Without the flag, `x < 0 ? -x : x` is not `abs.s`; `fabsf()` is (`func_003A95D8`). With it, `R() * 63.0f * 1.25f`
  folds into one multiplication: separate statements keep the two (`func_003A4970`).

Code shapes:

- **Frame counters wrap in two statements**: `s->unk1E++; s->unk1E %= 30;` (the first store is dead and deleted) and
  `s->unk1E = s->unk16 >> 1; s->unk1E %= 24;` matched six functions; a single expression ties the registers differently.
- **A frame pointer past a header** is `a++; s->unk10 = a;` written after the last header read (the in-place increment
  keeps the `addiu` after the loads); `a + 1` or `&a->frames[0]` compute it right after the call. In the spawn loops of
  the hit sparks the same shape is `s->unk10 = ++an;`.
- Update callbacks that end in `if (...) func_003465C8(e);` keep the tail call only with `else` around the per-frame
  part; a `return;` at the end of the init block turns it into `jal` + epilogue.
- K&R `int` functions again: `func_003A4EE8` / `func_003A5810` / `func_003A6978` return nothing but are `s32`, because
  their callers `return f(...)` (`jal` plus a shared epilogue instead of a tail call).
- Switch trees with an extra test come from extra labels: `case 0: case 2:` sharing a block with a separate `default`
  (`== 2; < 3; == 3`, `func_003A2758`), `case 2: break;` (`== 3; slti 4; == 4`, `Fx_DrawFighterTex`), `case 2: default:`
  (`Fade_Draw`).
- `&gFighters[side == 0]` is folded into a branch; `k = side == 0; &gFighters[k]` keeps `sltiu / mult` (`func_003A54D8`).
- `t < m ? t : m` is the EE `min.s`; the original's branch with `mov.s` in the delay slot is `if (t < m) r = t; else r =
  m;`. A divisor the original keeps as `div.s` under `-ffast-math` sits in a local assigned right before the division
  (as T15's `turn`, `func_003A1C78`).
- Scratchpad work areas: every access through one struct pointer to 0x70000100 reproduces the original mix of
  register-based and absolute (`lui $at`) addresses, because reload spills the base under pressure (`Fade_Draw` went
  from 175 to 4 lines); two stores to the same scratchpad word in a loop keep the absolute form only when one goes
  through the work-area struct (`Fx_DrawQuads`).
- A stack GIF packet header declared as a 16-byte struct (not `u128`) lands at sp+0; a TImode scalar moves to the stack
  only at first use (`Fx_LoadStageLayers`).
- A store run of constants found by a sweep of two store positions (`func_003A55E0`: `unk1A = 2` first, `unk90` after
  `unk54`), after decomp-permuter (score 60 to 20 in 20 minutes) had shown that the order mattered.
- The `.lit4` word 0x3D4CCCCE is `(0.6f - 0.2f) / 8.0f` folded in float precision, not `0.05f` (0x3D4CCCCD).

Accepted fake: an empty `__asm__ volatile("")` after the last store of `func_003A6BD0` keeps it out of the `jr ra` delay
slot as in the original (marked in the source).

Not matched yet (closest attempts in `#if 0`): hazard nops only (`func_003A1378`, `func_003A3438`, `func_003A4128`,
`func_003A4C28`, `func_003A6110`, `func_003A95D8`: a `nop` gas inserts before an unfilled `bc1f` / `bc1t` or between
`li.s` and a prelude `mtc1`), the `-ffast-math` pair above, `Fx_AddQuad` (1 line, `addu` operand order), `Fade_Draw` (4,
two constants), `func_003A44F8` (13), `func_003A76C0` (19), `func_003A88B0` (24), `func_003A0CE8` (50, table loads
hoisted above stores), `func_003A1C78` (75).

Tools:

- Five helper agents each owned a group of functions in a scratch file that included a shared header, compared with a
  scratch comparator (the build's compiler and prelude, objdump of both sides, `.lit4` loads compared by value); the
  parent merged the five files into the two C files by script, then confirmed every function with fdiff and the SHA-1.
  Name clashes between the groups' types and prototypes were fixed at the merge; callee prototypes that differed
  (`Fx_GetBankEntry`'s index as `u16` or `u32`) had to be unified and re-checked.
- Such a comparator must not end a function at splat's `jlabel` case labels, and must compare `%hi` / `%lo` of data
  symbols by value (splat leaves some as literal immediates).

### Lessons from T17 batch 3 (2026-10-20)

Objects and data:

- **Data order finds object edges inside a module.** `model_anim`'s `.rodata` (0x520C40-0x521700, then 0x521B80-0x521CE0),
  `.lit4` (0x5F5370-0x5F5378, then 0x5F539C-0x5F5614) and `.sbss` (0x5F6ED0-0x5F6F18, then 0x5F6F2C-) are each split by
  data of the object at 0x340000 (`D_00521700`, `D_005F5378`-`D_005F5398`, `D_005F6F1C`-`D_005F6F28`), so the module is two
  objects and the edge lies where both pools change hands: 0x3D3630 (`src/model_anim.c` / `src/model_anim_b.c`). No
  function of the range saves a register with `sq`, so the u128 edge test of T17 b1 gave nothing here.
- **splat writes the VU0 accumulator as a bare `ACC`** in the function files of a C segment (`vmulax.xy ACC, ...` in
  `func_003DA138` / `func_003DAA80`; the generic chunk had `$ACC`), which gas refuses. `src/model_anim_b.c` defines gas
  macros for the three mnemonics that rewrite the operand and then use the real instruction (each macro purges itself
  around the use and is defined again); inline asm in C must spell it `$ACC`. (Fixed centrally in T17 batch 4.)
- The clean rebuild once failed with missing `asm/nonmatchings/file/*.s` right after `configure.py` (a second split ran
  while `file.c` compiled); running `ninja` again built it. (Cause found and fixed in T17 batch 4.)
- Three C jump tables (`func_003D85D8`, `func_003D8E30` here; `func_003D2B88` stays a draft) needed no `RODATA_ALIGN`.

Compiler:

- **Both `model_anim` objects look built without `-ffast-math`**, like `effects` (T17 b2): `func_003D1468`, `func_003D4B18`,
  `func_003D5798` and `func_003D5998` match only without the flag (`-x + c` kept as `neg.s` / `add.s`, `-atan2f(...) + pi/2`
  not reassociated), and every other function compiles the same either way. Their bare `sqrt.s` (no errno call) is an
  inline-asm square root (`nop; nop; sqrt.s`, a `static inline` of the file, marked [I] / fake), which also keeps loads
  after it as the original; the builtin `sqrtf` folds `x / sqrtf(y)` into `rsqrt.s` and schedules across it.
- With `-ffast-math`, a constant divisor stays `div.s` only from a local assigned right before the division, one local
  per constant; `(y - x) * 10.0f` must be its own statement or the 10 and the 0.01 fold.

Code shapes:

- **Two-case switches**: the original `==1; slti 2; ==2; ==3` trees come from extra labels: `case 0: case 2: break; case 1:
  case 3:` (`Model_FlushDrawLists`), `case 2: default:` (round camera), `case 0:` plus `default:` for `switch (i % 4)`;
  the source order of the cases sets the layout. A test of a call result against 0 then 1 is a `switch` (`func_003D3528`).
- **Loop initialisers**: `for (i = 1, cnt = 0; ...)` gave the original `li a2, 1` / `move a3, zero` order where every
  order of separate statements kept them swapped (`Fighter_LogMove`, found after the helper's sweep).
- **12-byte copies through an inline** `CopyV3(FVec4 *d, FVec4 *s) { memcpy(d, s, 12); }` let the compiler hoist the
  pointers into saved registers before a loop as the original (`func_003D1B68`); a plain `memcpy` was 38 lines off.
- **A base pointer in a local** (`birds = D_00444D20; b = &birds[i];`) gives the EE `mtlo` / `madd` address arithmetic;
  `&D_00444D20[i]` gives `mult` / `addu` (`func_003D4830`).
- **Unit matrices** store 1.0f as integer words (`((s32 *)m)[k] = 0x3F800000`, k = 0, 5, 10, 15, marked fake): the float
  stores give `mtc1` / `swc1` where the original has `lui` / `sw`. Chained float zeros (`a = b = c = 0.0f`) give `mtc1
  zero` + `swc1` where separate statements give `sw zero`.
- A 16-byte `.rodata` vector copied with `ld` / `sd` needs an `aligned(8)` struct type, with `lq` / `sq` an `aligned(16)`
  one; without the attribute the copy is `ldl` / `ldr`.
- Non-constant aggregate initialisers (`GteVector v = {x, y, z, 0}`) reproduce the original stack temporary and its
  struct copy; later locals reuse its slot, so declaration order sets the frame (`func_003D2330`).
- A sorted list insert whose first test uses the new object is a `while (p && cmp) { prev = p; p = p->next; }` (gcc
  copies the test in front of the loop, `func_003DB578`); a five-value equality test is a `switch` (`func_003DB2A8`).
- Float and integer parameters are passed in separate register files, but their order in the prototype still changes
  the scheduling of the caller's loads (`Model_QueueDraw(ctx, res, kind, f32 scale, list, f32 depth)`).

Types: the 0x70-byte model node is `ModelNode` in `src/model_anim.c` and `StgObj` in `src/model_anim_b.c`; the stage
objects have a set-up view (`StageObjSetup` / `StageObjSetupList`) and an update / draw view (`StageObj` / `StageObjList`)
of the same memory, joined by casts at the two calls between them (no code change). `src/model_draw.c` declares
`StageObjList.obj[39]`; the stage code uses 36 objects and data at 0x1440.

Not matched yet: 32 drafts with one-line reasons in their `NON_MATCHING` notes (render.md section 9): five need the file
built without `-ffast-math`, five are hazard nops, the rest register allocation and scheduling (`func_003D1980` 2 lines,
`func_003D0E18` 5, `func_003D8D38` 9, `func_003D3FF8` 10, `func_003D7E10` 10 ...) and four large drafts (`func_003D62A0`,
`func_003D7378`, `func_003DA138`, `func_003DAA80`, the last two with inline VU0 code).

Tools:

- Six helper agents each owned a group in a scratch file compared by a scratch comparator (the build's compiler and
  prelude, objdump of both sides, `.lit4` loads by value, `%hi` / `%lo` by address); the parent merged the groups by script
  (OK functions replace their `INCLUDE_ASM` and `LIT4_WORD` lines, drafts go into `#if 0` with the note), unified the
  clashing types and prototypes by hand, and confirmed every function with fdiff and the SHA-1. The comparator must skip
  objdump's `.L` symbols (splat's `jlabel` case labels), or it cuts functions with jump tables short.

### Lessons from T17 batch 4 (2026-10-20)

Build:

- **Objects built without `-ffast-math`** are listed in `NO_FAST_MATH` (`configure.py`): `effects.c`, `effects_draw.c`,
  `scene3d.c`, `model_anim.c`, `model_anim_b.c`. Each of the five compiles every function that already matched to the same
  section bytes and relocations without the flag (each object built both ways and compared), so the flag could go per
  file; seven drafts then matched. `scene3d.c` is in the list on weaker evidence: its two float drafts went from 61 and 77
  lines to 10 and 21, and none of its drafts got worse.
- **The flaky clean rebuild**: `included_asm` read only `INCLUDE_ASM`, and `file.c` (all C) pulls in only
  `INCLUDE_RODATA` tables, so its compile did not wait for the split step. ninja reruns the split whenever one of its
  outputs has no `.ninja_log` entry (a split file that is new since the last build), and that split empties `asm/`
  while `file.c` compiles. `INCLUDE_RODATA` files are now dependencies too. The first clean build afterwards did run
  the second split (the new outputs) and passed.
- **Bare `ACC`** is fixed after each split (`fix_split_asm`, only after a VU0 mnemonic); `src/model_anim_b.c` lost its gas
  macros.
- Every C object depends on `configure.py`, whose code rewrites the compiler's output and sets the ABI tag.
- `scripts/permute.py`'s `compile.sh` still uses `CC_FLAGS` (with `-ffast-math`) and skips `--fix-asm`: a follow-up before
  permuting a function of the five files or one with these hazards.

Assembler (`configure.py --fix-asm`, docs/build.md "C files"; each rule checked by assembling the snippet against the
original bytes, then by the SHA-1 with every C file rewritten):

- **FPU compare before a branch**: gcc writes `#nop` after `c.cond.s`. When its next line is `.set noreorder` (a branch
  with a filled slot), gas inserts the nop on the directive and the original has it; when the next line is a `bc1t` /
  `bc1f` that gcc left to the assembler, the original has none. The compare then becomes a data word. A data word makes
  gas insert the nops still owed to the instruction before it, but only after a compare, an `mtc1` (also the one of
  `li.s`) or `ctc1` / `cfc1` (tried: `lwc1`, `lw`, `mflo`, `mfhi`, `mult`, `div`, FPU arithmetic, `lq`, `mfsa`, `jal` and
  `lui` give none).
- **`li.s` before a data word**: gas never moves the `lui / mtc1` of `li.s` into a delay slot and adds no nop between it
  and an instruction that reads the register, only before data. Written out as `lui $1` + the prelude's `mtc1`, the one
  difference goes away; `li.s` from the `.lit4` pool (one `lwc1`, which gas does move into delay slots) stays as it is.
- **`sqrt.s` in a `jal` delay slot**: in the original all 9 `sqrt.s` next to a `jal` sit in its delay slot, none before
  it, while all 31 `div.s` next to a `jal` come before it. gcc writes `nop / nop / sqrt.s` as its own noreorder block, so
  the game's assembler moved the last instruction of that block into the slot of a following `jal` (the modern gas never
  moves noreorder instructions), for `sqrt.s` only. No function matches through this rule yet [I]: the three stage
  drafts that need it let gcc fill the slot itself.

Compiler:

- **The libcall names of `func_003A8D08`**: without `-ffast-math` this compiler calls `sqrtf` for the errno fallback and
  `fptoui` for float to `u32`; the original calls `sqrt` and `__fixunssfsi`. No ee-gcc of decomp.me names them that way
  (990721 to 2.96, also with `-msingle-float` / `-msoft-float`); `-fshort-double` gives `sqrt` for the `sqrt()` builtin
  but still `fptoui`; an asm label on the prototype (`sqrtf(f32) __asm__("sqrt")`) drops the builtin and its inline
  `sqrt.s`. It matched with the assembler equates `.set sqrtf, sqrt` / `.set fptoui, __fixunssfsi` (marked fake), which
  relocate the calls to the original symbols.
- **One object, two square roots**: `effects_draw` has the errno form (`func_003A8D08`) and a bare `sqrt.s`
  (`func_003A76C0`), as do `scene3d` and `model_anim_b`; the bare ones use the inline-asm `SqrtS` of T17 b3 (fake).
  `func_003D7A40` matched with `SqrtS` in place of the `do {} while (0)` fake, which had added a `.p2align 3` nop.
- With the file's flag, `func_003A6110`'s C jump table ends at 0x519914 and `D_00519918` needs `RODATA_ALIGN8()` after it
  (fdiff said OK, the SHA-1 step caught it); its `jtbl_` include goes away and `D_00519918` moves after the function.
- `func_003518B8` (`scene3d_c`) now matches in every instruction, but its `.lit4` word lies after `render_fighter_a`'s pool:
  as C the word lands in `scene3d_c`'s pool, which moves `.sdata` by 16 (seen as a `$gp` offset in other functions).
- Prototypes fixed without a byte change: `func_003A6390` / `func_003A5FA0` return `s32` in `battle_flow.c`,
  `Wind_Init(s32)`, and `model_draw.c`'s `StageObjList` has `obj[36]` followed by 0x234 bytes, like `model_anim_b.c`'s.

Matched (20): `Cam_SetDistance`, `func_00352F50`, `func_003A1378`, `func_003A3438`, `func_003A35C8`, `func_003A4128`,
`func_003A4C28`, `func_003A6110`, `func_003A8D08`, `func_003A95D8`, `func_003D1468`, `func_003D3C68`, `func_003D4B18`,
`func_003D4F58`, `func_003D5798`, `func_003D5998`, `func_003D7A40`, `func_003D8038`, `func_003D8088`, `func_003D80E8`.
Not matched: `func_00392F80` (14 lines), `func_003932F0` (39), `func_00393550` (26), `func_0034CC38` (10), `func_0034E148`
(21), `func_003518B8` (the `.lit4` word). The counts in the `NON_MATCHING` notes of the five files without `-ffast-math`
were measured again with fdiff, one draft at a time (enabling all drafts of a file at once moves the `$gp` offsets);
earlier counts came from the helpers' scratch comparator.

### Lessons from T17 batch 5 (2026-10-21)

Tools:

- **`scripts/permute.py` compiles like the build**: its `compile.sh` takes `configure.cc_flags(src)` (no `-ffast-math`
  for the `NO_FAST_MATH` files) and runs `configure.py --fix-asm` between the compiler and `as`
  (`tests/test_permute.py` runs the generated script with stand-ins: RED with 2 failures, then GREEN). Its first run
  matched `func_00360E88` (16 lines off) in 30 seconds; the same shape (`e = err;` before the last two tests, a return
  through `e`, both marked fake) then matched `func_00360DE0` and `func_00360E38`.
- The session scratchpad under `/tmp` was emptied by a reboot in the middle of the batch (the helpers' scratch files and
  part of a C file went with it). Drafts were then kept in the repository's C files from the start (`#if 0` above the
  `INCLUDE_ASM`, checked one by one with a comparator that enables a single draft in a copy of the file), so a lost
  scratchpad costs nothing.
- A script that rewrites a draft block must match only blocks that contain no other `#if 0`: a non-greedy
  `#if 0 ... INCLUDE_ASM(f)` pattern spans every earlier block and deleted eight functions once.

Objects and data:

- **Pool order inside one object again** (`vu1_packet`, 0x5F57EC-0x5F5804): the words of the last function
  (`func_00342E38`) come first, then those of `func_003424A8`, `func_00342818`, `func_00342A38` and the first function
  `func_003420C8`. With `func_00342E38` in C, its two literals are emitted by the compiler and the other words follow as
  `LIT4_WORD` lines at the end of the file; a C function whose word must come later cannot use a literal, and reading the
  word by name from C (`extern f32 D_005F5800;`, defined by a later `LIT4_WORD` in the same file) crashes gas
  ("Internal error in nopic_need_relax"), so `func_003420C8` stays assembly although every instruction matches.
- In `render_fighter_a` the pool is in text order (0x5F5378-0x5F539C): `LIT4_WORD` lines in front of the assembly
  functions and the C literal of `func_00340408` in between give the original bytes.
- **The object before crt0 ends with padding**: `_start` (0x3572A0) is 16-aligned, and the three `nop`s after
  `func_00357258` belong to that alignment; `src/draw_list.c` ends with `.balign 16` in the text section (without it
  every later address moved by 8).
- A table read by one function that splat does not migrate into the function's `.s` (`D_004FC350`, read by
  `Gfx_PkAddImage`) is undefined at link time once the object has a `.rodata` subsegment: it needs
  `force_not_migration:True` and an `INCLUDE_RODATA` (with `RODATA_ALIGN16()` after a C jump table).
- New `INCLUDE_ASM` lines (a function moved back to assembly) need `configure.py --split` before ninja finds the `.s`.

Code shapes:

- **Prototypes of drafts**: a function whose draft is in `#if 0` has no prototype unless one is written; an implicit
  `int f()` made the compiler treat `v0` as set by the call and moved a later value from `v0` to `v1`
  (`func_00361978`, matched in the scratch file, 5 lines off in the build). Declare every draft's prototype.
- Parameters narrower than `int` show as masks at the function entry (`andi a1, 0xFFFF` for a `u16`), not at the use
  (`Gfx_PkAddImage`); a 64-bit GS field built from an `s32` has `dsll32` only, from a `u32` `dsll32` / `dsrl32`.
- `sceVif1Packet` takes 0x20 bytes of stack (`pBase` at +4 is what `func_00343178` gets); a 0x10-byte declaration moved
  every stack offset by 0x10.
- Inner `switch` on a three-way result keeps the original `beqzl` store (`func_00361710`); `if / else if` gave a branch
  and a fall-through. Card-status bits tested in several states are best written inline (`st & 0x8000` in each case),
  the compiler places the copies where the original has them.
- Struct copies of 8-byte `{f32, s32}` records with 4-byte alignment are `ldl / ldr / sdl / sdr` (`func_00340BF0`, the
  merge sort); its comparison matched only as `tmp[j].key >= keys[i].key` (operand order sets the load order).
- A value stored and kept (`*out = sum / n; d = *out;`) gave the original `div.s` into `$f0` then `mov.s`
  (`func_00340408`).

Assembler (open): the original keeps a `li.s` load from `.lit4` in front of a `jal` with a `nop` in the delay slot
(`Model_Render`), where gas moves it into the slot; elsewhere such loads do sit in delay slots. Likely the old gas's
relaxable gp-relative frag for `li.s` (it does not swap an instruction from a variant frag) [I]; a `--fix-asm` rule
needs more cases before it can be written.

Matched (57): `func_003608B8`, `func_00360A08`, `func_00360A30`, `func_00360C98`, `func_00360CA0`, `func_00360D48`,
`func_00360D50`, `func_00360DE0`, `func_00360E38`, `func_00360E88`, `func_00360EE0`, `func_00360F28`, `func_00361678`,
`func_003616C8`, `func_003616F8`, `func_00361710`, `func_00361968`, `func_00361978`, `func_00361AA8`, `func_00361B18`,
`func_00361B28`, `func_00361B58`, `Gfx_PkAddImage`, `func_00361D50`, `func_00361D70` (font_tex, 25); `func_00356A70`,
`func_00356D78`, `func_00356E00`, `func_00356E20`, `func_00356E38`, `func_00356E50`, `func_00356EE8`, `func_00356F68`,
`func_00357028`, `func_00357128`, `func_00357230`, `func_00357258` (draw_list, 12); `Model_Draw`, `Model_SendMatrices`,
`Model_SendParts`, `func_00341F68` (render_fighter_b, 4); `func_00342128`, `func_003421D8`, `func_003422C0`,
`func_00342360`, `func_00342390`, `func_00342400`, `func_00342988`, `func_00342E38` (vu1_packet, 8); `func_00340398`,
`func_003403B8`, `func_003403F8`, `func_00340408`, `func_00340528`, `func_00340BF0`, `func_00340DF8`, `func_00341AE8`
(render_fighter_a, 8). Not matched (drafts) and not attempted: render.md section 9.

### Lessons from T17 batch 6 (2026-10-21)

Objects and data:

- **`sprite2d` is one C file with data subsegments**: `[0x8E298, c, sprite2d]`, `[0x1E1648, .rodata, sprite2d]`
  (0x520648-0x520C40, 11 tables with `force_not_migration`, each included in front of its first reader) and
  `[0x2B6364, .lit4, sprite2d]` (three words in text order: `LIT4_WORD` lines until the readers were C, then the C literals).
  Its data starts right after `select_results`' (`D_00520638`, `D_005F535C`), so 0x3CD298 is a cut, not a proven edge. The
  clean rebuild stayed byte-identical with every function still `INCLUDE_ASM` before any C was written.
- **A jump table among other objects' data blocks the C**: `func_00356A78`'s `jtbl_00522030` sits between `Model_Render`'s
  `jtbl_00522010` and other objects' tables in the generic `.rodata`, and `draw_list` has no `.rodata` subsegment, so a C
  switch there would move the table; the function stays a draft. `func_00360F40`'s table (inside `font_tex`'s subsegment)
  landed at its place with no `RODATA_ALIGN`: the `.s` file before it ends 16-aligned.
- **The `vu1_packet` pool cannot be reproduced by C literals**: reading a `.lit4` word by name from C crashes gas
  (`Internal error in nopic_need_relax`) whether the `LIT4_WORD` label is defined before or after the reader (tested), so
  the four functions whose words follow `func_00342E38`'s stay drafts (`func_00342818` 9 lines, `func_003420C8` all
  instructions).
- A struct view across files: the sprite table `D_00440F60` is one 0x20-byte record read as a texture / CLUT entry by one
  half of `sprite2d` and as an image record by the other; the merged file keeps one declaration and casts at the two
  readers (no code change).

Code shapes:

- **Range tests kept as separate branches with `li K / slt`**: `func_003408B0` matched only with its screen test in an
  inline whose right and bottom bounds are parameters (`Scene_IsOff(c, 0x940, 0x8E0)`, marked fake): written in place,
  `x < 0x6C0 || x > 0x940` folds to `addiu / sltiu` (range test) or `slti 0x941`. The same function needed a pointer local
  for the second loop (`p = top[i]`) to drop two saved registers.
- **Scratchpad header words through one struct pointer and a `switch`** (`SprHeader *s = (SprHeader *)0x70000000`,
  `switch (s->count)`): `func_00342FB0` (NON_MATCHING since T13 b2) matched; absolute addresses let gcc keep 0x70000004 in a
  register.
- **A run of four-word vector stores** (`v = D_005F6780->box[i]; v[0..3] = ...`) took the original's register rotation only
  through a `static inline` helper called per corner; a separate `u128 *` pointer for a 16-byte copy (marked fake) forces the
  reload of a global pointer that `*(u128 *)&s->f` does not (`func_00340070`, 110 to 9 lines).
- `sinf(-f())` under `-ffast-math` evaluated `f()` twice; the negated angle goes through a local (`func_00340E28`).
- **sprite2d GIF code**: header-then-body packets match with the body through a second pointer (`q = p + 6; q[i] = ...`),
  `*p++` puts an `addiu` after every store; packet fields read as whole `u64` locals reproduce `ld` plus shifts where
  `(pk[0] >> 32) & 0xFFF` narrows to `lwu` / `lhu`; the OR order inside a BITBLTBUF expression sets the emitted order; a
  `prim = D_005F6748 | 0x316` local puts the `$gp` load first.
- The colour constants 0x80 and 0x808000 stay two hoisted registers only through `u64` locals (fake), every constant form
  folds them to 0x808080 (`Sprite_DrawCurrentGouraud`, still 2 lines: the original stores the GIF tag word p[1] before p[0]).
- A switch of 28 states matched first try once the cases were written in the order of their code with the original's
  fall-throughs (`func_00360F40`); one value held in the same argument register across three loops came from one
  function-scope variable (`func_00361EA8`, 96 to 29 lines).
- A 16-byte record copied with two `ld / sd` pairs needs a `u64` member (8-aligned struct); a bitfield struct for a 64-bit GS
  register gives `lw` + `andi 0x3FFF` where a mask narrows to `lhu`.

Tools:

- The cmp.py-style comparator marks original instructions `<` and compiled ones `>`; several NON_MATCHING notes had the
  direction reversed.
- decomp-permuter scored `func_00343340` (2 lines in the comparator) at 4865: its inline asm `lqc2` block and the VU0
  memory stores make the permuter's base differ; the run was stopped. Helpers worked in copies outside the repository
  (`~/.cache`, which survives a reboot) with the comparator; the parent merged whole files and confirmed with fdiff and the
  SHA-1.

Matched (52): the 46 C functions of `src/sprite2d.c` (`Sprite_*`, `Num_ToBcd`, `NameEntry_Init`, `Fighter_ClearLife` and 25
placeholders), `func_00342FB0` (dma_gif), `func_00340588`, `func_003408B0`, `func_00340BA0`, `func_00340E28`
(render_fighter_a), `func_00360F40` (font_tex). Not matched: the drafts of [systems/render.md](systems/render.md) section 9,
carried to T19. Left in assembly on purpose: the four hand-written VU0 routines of `dma_gif` (`func_003435C0` ..
`func_003438B8`).

### Lessons from T18 batch 1 (2026-10-21)

Objects and data:

- **Data order splits the `modes` module into two objects**: `scene3d`'s `.rodata` (0x51EA60-0x51EB00) sits between the
  tables of 0x3B9178-0x3BB228 and those of 0x3BB310 on, so `src/modes.c` (0x3B90C0-0x3BB310, `.rodata` 0x51E970-0x51EA60)
  and `src/modes_b.c` (0x3BB310-0x3BDF60, `.rodata` 0x51EB00-0x51F150) are separate files. `Mode_Change` moved from
  `game_pause_results.txt` to `game_modes.txt` (the range edge moved to 0x3B90C0); `modes.c` starting there is a cut [I].
- **`battle_mode` follows `main`'s `.rodata`**: `main`'s ends at 0x4FB9B4 and the 12 zero bytes up to 0x4FB9C0 are the
  alignment of `battle_mode`'s jump-table `.rodata`; they stay in the generic chunk (`[0x1BC9B4, rodata]`) and
  `src/battle_mode.c` starts with `RODATA_ALIGN16()` and its subsegment at 0x4FB9C0.
- The three C files were created with every function `INCLUDE_ASM` and every `.rodata` symbol `force_not_migration` +
  `INCLUDE_RODATA` in front of its first reader (a generator script over the split), and the clean rebuild was
  byte-identical before any C was written.
- Two C jump tables shorter than their `.s` files (which carried the padding): `Mode_Title`'s table needed
  `RODATA_ALIGN8()` before `D_0051EFE0`, `Mode_Transition`'s `RODATA_ALIGN16()` before the included `jtbl_0051E9B0`. As
  before, fdiff said OK and only the SHA-1 step caught them; `cmp -l` showed the shifted table at once.

Code shapes:

- **Float locals per `case`**: declaring the fade's `t` / `d` inside each `case` block gave the original FPU registers
  (`Mode_Transition` matched, `Mode_Boot` went from 138 to 18 lines); a divisor kept as `div.s` again comes from a local
  (`d = 30.0f`).
- **Elapsed frames** are `(gFrameCount - start) & 0xFFFF`; a `(u16)` cast narrows the timer load to `lhu`. Pad words tested
  with a mask that fits 16 bits (`& 0xF4`) are read with `lhu` through `(u16)` casts, with `& 0x800` with `lw`.
- **Switch trees again**: `==0; slti 3; ==3` came from `case 1: case 2:` and `case 3:` only; `beq 1; slti 2; ==2` needed
  an added `case 0: break;` (`func_00359C40`).
- `s16` globals copied through `u16` locals (`u16 p0 = D[0], p1 = D[1]; E[0] = p0; E[1] = p1;`) give the original `lhu lhu
  sh sh` order; an 18-bit wrap matched only as `(x & 0x20000) == 0 ? (x & 0x3FFFF) : (x | ~0x3FFFF)`.
- Screen x as `i * 320 + (i == 0 ? 0x90 : 0xB0)` gave the single `mult` + `movz` of `func_003BC048` (the `if` forms were
  47-82 lines off); a callee the caller passes an extra argument register to gets an unused trailing parameter
  (`func_003BB4E8`).
- One field read through the pointer parameter and the same global read directly in the same function is kept that way
  in the source (`func_003BC370`); the merged form changes the registers.

Tools:

- Three helpers each owned one file in a scratch copy under `~/.cache` with a scratch comparator (the build's flags,
  `--fix-asm` and prelude; the targets copied out of `asm/nonmatchings` so a re-split could not pull them away); the parent
  merged each result by script (OK blocks replace `INCLUDE_ASM` and drop their own `jtbl_` includes, drafts go into
  `#if 0`). The comparator reports false differences for branches to splat's `jlabel` case labels (a relocation in the
  original) and for jump-table addends in files with several tables; turning `jlabel X` into a local label fixes the
  first.
- decomp-permuter on `func_003BAD98` (4 lines) found nothing below its base score in about 135,000 iterations.

Matched (136): 39 of `src/modes.c` (`Mode_Change`, `Mode_Transition`, the per-type set-ups `func_003B9AE0`..`func_003BA118`,
`Game_UpdateContinue`, the win streak and vibration helpers), 34 of `src/modes_b.c` (`Mode_Loading`, `Mode_EnterLoading`,
`Mode_Title`, the side status texts, the controller warnings, the menu list), 63 of `src/battle_mode.c`. Not matched: the 17
drafts of [systems/modes.md](systems/modes.md) section 6. Renamed: `Mode_MainMenu`, `Mode_Loading`, `Mode_EnterLoading`,
`Game_Start`, `Game_UpdateContinue`, `Mode_NameEntry` (guess).

### Lessons from T18 batch 2 (2026-10-21)

Objects and data:

- **Five C files from three assembly chunks, data-order edges**: the `.rodata` of 0x3A9678-0x3B90C0 runs in module order
  (`loading_title` 0x519B30, `menu_common` 0x51DE30, `options` 0x51DEF0, `pause_results` 0x51E580 up to `modes`'
  0x51E970), each start 16-aligned as an object with jump tables needs; no function of the range saves with `sq`, so
  the u128 edge test gave nothing and the inner edges stay [I]. `select_results` became two files around the `hud`
  cut: `src/select_results.c` (0x3BDF60-0x3C1A78, now from the module start) and `src/select_results_b.c`; hud's strings
  0x51F678-0x51F6C0 stay in a generic `rodata` chunk between their subsegments, so `src/hud.c` was not touched.
- The skeletons were generated by script (every function `INCLUDE_ASM`, every `.rodata` symbol `force_not_migration` +
  `INCLUDE_RODATA` in front of its first reader, clamped after the jump table of an earlier reader), `.lit4`
  subsegments got `LIT4_WORD` lines (pool order = text order here), and the clean rebuild was byte-identical before any
  C. Tables read only by other objects (`main.c`'s input tables at 0x51DEF0, `rng_misc.c`'s character sets at
  0x51E850) are included in place like the rest.
- **`loading_title` is built without `-ffast-math`** (`NO_FAST_MATH`): `func_003A96B8` keeps `x + 0.0f`, and
  `func_003ACCD0` / `func_003ACF50` (`div.s` by `1.0f / n`) matched with the flag off; every function that already
  matched still did.
- Jump tables the C now emits: one `RODATA_ALIGN16()` (`D_0051DE50` after `func_003AEF88`) and one `RODATA_ALIGN8()`
  (`D_005202E8` after `Records_DrawRank`'s 0x84-byte table); a function's own strings that the script had placed after it
  had to move in front of it once its switch became C. `Text_Printf`'s 0x49-entry table was split by splat into
  `jtbl_00520010` plus raw words in `D_00520058`, which `.data` references: a data-layout change is needed before it can
  be C.
- A C float literal in a function whose words are not at the end of the pool (`func_003AA8A8`, `func_003ACB08`) changes
  the pool and moves every `.sdata` `$gp` offset by 16: both stay drafts with their `LIT4_WORD` lines in the `#else`.

Code shapes:

- **Chained assignments set store order** again: `q.v[3].r = q.v[2].r = x;` for the menu quads (`func_003BF180`,
  `func_003BF2F8`), `f->unkB8 = f->unkC0 = mv` in the ending scripts, `D_005F6D5C = D_005F6D60 = 4` sharing one register.
- A struct field beyond 0x7FFF (`Fighter` + 0x8800) matched only as explicit `(s32)f + i * 64 + 0x8800` arithmetic; the
  member access splits the offset as 0x8000 + low.
- `li a1, K` before a `jal` with the store of K in its delay slot can be a leftover argument register: `func_003AD678`
  matched once `func_003464F8` had one parameter.
- Locals the original keeps at fixed stack slots were arrays (`s32 cnt[2]`), which restored the frame of `func_003B6C68`;
  a prologue `memset(sp, 0, 8)` call is a struct initialised with `= { 0 }`.
- Two-case trees again: `case 0: break;` gives `beq 1; slti 2; ==2` (`func_003CC3E0`); a `goto` to a block placed after
  the shared tail call reproduced the block order there (the `if / else` forms were 134 lines off).
- `x = abs(v) >> 4` from a sine table matched `bltzl / negu` only with the table value in its own block-scoped temporary.
- A `void` case ending in a call needs `return;` after it for the tail call `j` (`Credits_Update`).
- Stack copies of `.rodata` tables are struct copies from an `extern const` struct (`WordPair2 t = D_005205B8;`), not
  array initialisers.
- Not solved: the three records lists of the options (`func_003B2090`, `func_003B2358`, `func_003B2670`) share `if (D ==
  0) col = 2; else if (D == max) col = 1;`, which this compiler if-converts to `xor / movz` where the original keeps
  `bnel / beql`; every natural form tried keeps the `movz` (`col--; col--;` gets to 2 lines, not taken).

Accepted fakes (marked): an empty `__asm__ volatile("")` keeping a store or call out of the tail position
(`func_003B7A58`, `func_003BFDF8`, `func_003BF820`), `func_003B7B18` returning `u64` for its `jal` + epilogue, a
separate `d &= 1;` (`func_003B2978`), `.set fptoui, __fixunssfsi` (`func_003C0098`), a `volatile` read of `D_005F5CB2`
(`func_003C3FB8`).

Tools:

- Each helper worked in a full copy of the repository under `~/.cache` (its own `build/`, with `iso/`, `tools/` and
  `.venv/` linked), ran `fdiff.py` and the SHA-1 step there, and handed back whole C files; the parent copied them in and
  confirmed with the clean rebuild and fdiff. A copy needs `configure.py --split` after a function goes back from C to
  a draft. Three helpers per round, three rounds (the usage limit stopped the first round once; the helpers resumed).

Matched (378): 74 of `src/loading_title.c`, 72 of `src/menu_common.c`, 27 of `src/options.c`, 65 of
`src/pause_results.c`, 58 new in `src/select_results.c` (`func_003C18B8` among them), 82 of `src/select_results_b.c`.
Not matched: the 85 drafts of [systems/modes.md](systems/modes.md) section 14; `func_003CA470` (1062 instructions) not
attempted. Renamed (20): `Mode_Ending` / `Mode_EnterEnding` (guess), `Credits_Update`, `CharSet_Copy` / `Clear` / `Count`,
`Options_Apply`, `Options_ControllerSetup`, `Pause_Update`, `Time_FramesToClock`, `Unlock_ApplyAll`, `Unlock_Step`,
`Unlock_SetClearBit`, `Team_BuildRandom`, `Arcade_BuildOpponents`, `Records_InitDefault`, `Records_DrawRank`,
`Text_PutGlyph`, `Results_DrawStageTimes`, `Results_DrawSurvival`.

### Lessons from T18 batch 3 (2026-10-21)

Objects and data:

- **The `theater` range is four objects by data order** [I edges]: `.rodata` runs `theater_d` (0x521CE0), `scene3d_c` /
  `scene3d_d` / `draw_list`'s tables, `theater_b` (the music player, 0x522048), `theater_c` (one jump table, 0x5221C0),
  `theater` (0x5221E0), then `demo`, `geometry`, `vu1_packet`, `menu_bg`, `gallery_jpeg`, `tekken_bowl`; `.data` and
  `.sbss` agree. A jump table makes an object's `.rodata` 16-aligned (`.align 4`, checked with a test compile), so the
  music player (0x522048, 8-aligned) cannot own `jtbl_005221C0`: `func_003564C8` starts another object. Strings read only
  by `.data` tables (track and ending names) belong to the object whose `.data` points at them.
- **`.balign` in `RODATA_ALIGN8()` does raise the section alignment**: `src/theater_d.c` starts with it so its `.rodata`
  lands at 0x521CE0, after the 4 padding bytes behind `model_anim_b`'s 0x15C-byte `.rodata` (without it the object was
  placed at 0x521CDC).
- **splat writes the VU0 `Q` register bare** (`vdiv Q, $vf0w, $vf13w`, `vmulq.xyz $vf18, $vf13, Q`) in function files,
  which gas refuses: `fix_split_asm` now writes `$Q` like `$ACC` (anywhere in the operand list of a VU0 instruction).
- **Two splat "functions" were compiler epilogues**: `Mtx_Unit` / `Mtx_Zero` are inline assembly ending in their own `jr
  $31`; the compiler's `jr ra / nop` after each asm block became the labels 0x356738 and 0x356754 (not 8-aligned). The C
  file reproduces the bytes with no `INCLUDE_ASM` for them; the game count went from 2815 to 2813.
- **Three more objects without `-ffast-math`** (`NO_FAST_MATH`): `menu_bg.c` (`func_003E45E0` keeps `neg.s` + `add.s`),
  `gallery_jpeg.c` (`Jpeg_Idct` keeps `coef * c * cv`), `tekken_bowl.c` (`x + 0.0f` / `x - 0.0f` kept in `func_003EF3A8`
  and `Bowl_PinHomePos`); every function that already matched still does.
- Tables read by several functions must be included in front of the first jump table the C emits, or the table lands
  at the wrong `.rodata` offset; a draft's jump-table `INCLUDE_RODATA` and its `LIT4_WORD` lines go in the draft's
  `#else`, with the data around them moved outside the `#if` so the order holds.
- A non-small-data global of 8 bytes or less (`.bss`, `.data`) declared as a scalar is reached through `$gp` and the link
  fails (`R_MIPS_GPREL16` / "small-data section exceeds 64KB"): declare it `[]`; `D_005F5CB0` (`.sdata`) must be `u16[2]`.

Code shapes:

- `s32 r = f(); return r;` keeps `jal` + epilogue; `return f();`, a void call or an `int` function with no `return` give
  the tail `j` (`Jpeg_EncodedSize`, `Jpeg_EncodeEnd`, `Jpeg_EncodeBegin`; `Text_DrawId` is `s32` with no `return`).
- A separate `x *= 2;` keeps a constant multiplication from folding into the next one (`Jpeg_Cos`, `Jpeg_RgbToYcc`).
- A chained store `a = b = c = d = 0.0f` is emitted a, d, c, b (`func_003EBB80`); `a = b = x` stores `b` first.
- A global float updated in place (`D += v`) kept the original FPU registers where a local copy did not (`Bowl_DrawMissFoul`).
- GS packet builders match as flat `*p++ = (u64)...` with the OR terms in GS field order (BITBLTBUF: SBW, SPSM, DBP,
  DBW, DPSM); stores of a +-r cube in ascending offset order give the original's scheduled order (`func_003E13C8`).
- libvu0-style VU0 code matches as C calls between separate `__asm__ volatile` blocks (vf4-vf7 survive the calls,
  `func_003E9038`); a leaf VU0 routine as one asm block, gas filling the `jr` slot (`func_003E92C0`).
- Bit readers / writers with tail recursion match as real recursion (`func_003E8D18`).

Tools:

- m2c: `.venv/bin/python -m m2c.main -t mipsee-gcc-c` (the EE target; `mips-gcc-c` loses float arguments), with
  `--context` on the preprocessed file and `--valid-syntax`; its output compiles once `M2C_FIELD` / `M2C_UNK` /
  `M2C_BITWISE` are defined per draft block (`M2C_BITWISE` through an `s32` cast, else a float to `s64` conversion needs
  the missing `__fixsfdi`), float arguments are reordered by hand, inlined `.rodata` strings are mapped back to their `D_`
  symbols, and scalar globals become arrays.
- Three helpers in full repository copies under `~/.cache` (one per file group, two rounds each: matching, then compiling
  drafts of the rest); the parent copied the files in and confirmed with the clean rebuild and fdiff. decomp-permuter on
  `Jpeg_RgbToYcc` (2 lines) found nothing below its base in 15 minutes.

Matched (188): 51 in the theater files and `demo.c`, 15 in `menu_bg.c`, 24 in `gallery_jpeg.c`, 98 in `tekken_bowl.c`.
Not matched: the drafts of [systems/modes.md](systems/modes.md) section 20, `func_003CA470` among them; nothing is left
unattempted in the T18 files. Renamed (67): `Mode_EnterTheater`, `Mode_EnterMusicPlayer`, `Theater_OpenMusicPlayer`,
`Mtx_Unit`, `Mtx_Zero`, `PracFont_*` (3), `Prac_DrawCongratulations`, `Prac_DrawDemoPrompt`, `Gfx_BuildImageUpload`,
`Gfx_DrawLine` / `Rect` / `Triangle`, `Text_Get` / `Width` / `LineCount` / `DrawId`, `Fade_DrawScreen`,
`Menu_DrawCursorBar`, `Gallery_DrawList`, `Gallery_Enter`, `Jpeg_*` (22), `Bowl_*` (17), `Pause_BowlTop5`,
`Pause_BowlOptions`, `Bs_*` (4, guess).

### Lessons from T19 batch 1 (2026-10-21)

Inventory:

- `scripts/inventory.py` lists the game functions still marked `.NON_MATCHING` in the build. Each is asm-only (no C
  file, or not attempted in one) or a draft with a cause taken from its `NON_MATCHING:` note
  ([t19_inventory.md](t19_inventory.md)). At the start of T19 there were 817: 381 in six modules without a C file,
  5 never attempted, and 431 drafts.

Objects and data:

- **Nine C files from the last assembly chunks.** The skeletons were generated by script and the clean build was
  byte-identical on the first try:
  - `geometry` became three files around libvu0: `geometry.c` (0x346398-0x347760, `.lit4` 0x5F51A0), `geometry_b.c`
    (0x347760-0x34A7A8, `.rodata` 0x522DE0-0x522FC0, `.lit4` 0x5F573C-0x5F57EC) and `geometry_c.c`
    (0x34B4E8-0x34C508, `.lit4` 0x5F5160-0x5F5168). The edges follow the data: `geometry`'s first object has its
    `.lit4` word just before `effects`', the second its pools after `demo`'s.
  - `fighter_setup` became two files around `motion`: `fighter_setup.c` (`.lit4` 0x5F5100-0x5F5114) and
    `fighter_setup_b.c` (`.rodata` 0x4FC630-0x4FF8D0).
  - Also `practice.c` (`.rodata` 0x506260-0x5064C0), `zsort.c`, `screenshot.c` (`.rodata` 0x518E70-0x518EA8, the
    jump table of `func_0039C2C0`) and `movie.c`.
  - Every `.rodata` symbol in these subsegments got `force_not_migration` and an `INCLUDE_RODATA` in front of its first
    reader. `LIT4_WORD` lines were emitted in pool order before the reader that needs the highest word.
- **A C file needs a data subsegment only for data the compiler emits.** That means jump tables and float literals.
  Strings and tables read as `extern` stay in the generic chunks, so `zsort.c` and `movie.c` have no data
  subsegments at all.
- **One subsegment may hold other objects' tables.** `fighter_setup_b`'s `.rodata` runs from 0x4FC630 to battle_flow's
  0x4FF8D0 and includes `skeleton`'s `D_004FC668` and `gte`'s `gSinTable` in place. The other files read them as
  `extern`.
- **Hand-written VU0 routines can be C.** A function body made of one `.set noreorder` asm block (local labels,
  `%%hi` / `%%lo`) reproduces a routine, and `return p;` with `register u128 *p __asm__("$5")` gives the final
  `jr ra / daddu v0, a1`. This matched `dma_gif`'s four routines. `geometry_b`'s seven routines
  (`func_00349CF8`-`func_0034A184`) stay drafts: they jump into each other's labels and `func_0034A180` starts in the
  delay slot of `func_0034A148`'s return.
- `func_0034A570` matches in every instruction, but its `.lit4` words lie before `func_00349D70`'s, so it stays a
  draft. Its `LIT4_WORD` lines sit in `func_00349D70`'s `#else`.

Code shapes:

- **Mode dispatch on a global re-read each time.** `fighter_setup`'s replay recorder (`D_005F5E48`) matched as an
  `if / else if` chain. A `switch` made a jump table and shifted `.rodata`.
- **First loop step not peeled.** `i = 0; if (id != t[i]) while (++i < N) {...}` keeps the original's run-time
  increment. A `for` loop from 1 makes gcc peel the first step (`func_00366410`).
- **Loop bound through the global.** In zsort, `i < D_008BD310.unk58` rather than `i < d->unk58` reproduced the
  original's pointer copy and its duplicated load in the loop test.
- **Booleans and clamps.**
  - `ok = 0; if (a == b) ok = 1;` gives the original's `movz` (`func_0037AF80`).
  - The menu wrap matched as `v < lo ? hi : (v > hi ? lo : v)` (`func_0037A5C8`, `func_0037A970`).
- **Two flag bits tested with one `ld`** need the object struct declared `__attribute__((aligned(8)))`
  (`func_00346608`, the 0x160-byte object of `geometry.c`).
- **Object lists.**
  - Load `prev` / `next` once into temporaries, with one `prev` temporary per list.
  - Put counter updates last.
  - A shared `return 0` between the tests comes from a `goto` out of a nested `if`.
- **GS packet builders.** `movie.c`'s builders match as libgraph `SCE_GS_SET`-style OR chains of `(u64)` casts on
  `u32` parameters. Nested OR groupings set the order of a 64-bit field (`func_00366880`).
- **Small `.data` pairs.** The 8-byte flag pairs at 0x42F140 / 0x42F148 must be incomplete arrays, or the link fails
  with `R_MIPS_GPREL16`.

Tools:

- Three helpers, each in a full copy under `~/.cache`. Two used a scratch comparator: it compiles the file like the
  build and scores a variant in about 0.3 s without ninja, which made store-order sweeps fast. The parent copied whole
  C files in, ran `configure.py --split` and confirmed every function with fdiff and the SHA-1. All three stopped once
  on the usage limit and resumed from their saved files.
- When the shell defines `cat` as an alias for `bat` with a pager, scripts that call `cat` hang. Use
  `bat --paging=never` or `tail`.

Matched (316): `geometry` 21, `geometry_b` 9, `geometry_c` 2, `fighter_setup` 28, `fighter_setup_b` 97 (both jump
tables now from C), `practice` 38 (`func_003794D8`'s jump table from C), `zsort` 10, `screenshot` 19
(`func_0039C2C0`'s jump table from C), `movie` 88, and `dma_gif`'s four VU0 routines. No `RODATA_ALIGN` was needed
and no file went into `NO_FAST_MATH`.

Not matched: 70 new drafts with one-line notes, `Mc_Update` among them. As C, its strings and its 38-entry jump table
(`D_004FC108`-`jtbl_004FC180`, migrated into `Mc_Update.s`) need `force_not_migration` and `INCLUDE_RODATA` first.
Every game function now has C (asm-only: 0).

### Lessons from T19 batch 2 (2026-10-22)

Build and data layout (each change checked with the clean rebuild and the SHA-1):

- **Pool order from C** (`configure.py --fix-asm`, docs/build.md step 2; `tests/test_configure.py`, RED then GREEN).
  The original `.lit4` pool is not always in text order, and some words sit in another object's part of the pool.
  Three markers in front of a C function steer the compiler's pool loads (`li.s` that gas loads from `.lit4`):
  - `LIT4_ORDER(N)`: the words go into subsection N of `.lit4`, after every word of lower subsections
    (`LIT4_WORD_AT(N, ...)` puts a split word there). Two of `vu1_packet`'s four out-of-order functions matched
    (`func_003420C8` in subsection 3, `func_00342818` in 1; `func_00342A38`'s word is `LIT4_WORD_AT(2, ...)`), and `geometry_b`'s
    `func_0034A570` matched once the VU0 routines' words after it moved to subsection 1.
  - `LIT4_REF(NAME)`: the next pool load reads the split word `NAME` with an explicit `%gp_rel` (the `lwc1` macro of
    gas asserts on a symbol in `.lit4`: "Internal error in nopic_need_relax"; the explicit relocation does not). The
    load is wrapped in `noreorder`: `Model_Render` (three words at 0x5F5700) and `func_003518B8` (0x5F570C) matched.
  - `LIT4_KEEP(N)`: keeps the N-th pool load out of a branch delay slot (`Bowl_PinHomePos`, case 6). Keeping every
    pool load out of delay slots was tried and changes many matched functions: the game's assembler did move them in
    most places.
- **Jump tables among other objects' data**: `render_fighter_b` and `draw_list` got `.rodata` subsegments
  (0x522010, 0x522030; `jtbl_00522030` with `force_not_migration` and `RODATA_ALIGN16()` in front of it), so
  `Model_Render`'s switch is C and `func_00356A78`'s table can be emitted once its draft matches.
- **Split tables**: two `.data` words 0x005200DA were symbolized as `D_00520058 + 0x82` and split `Text_Printf`'s
  0x49-entry table in two. `jtbl_00520010` is one symbol again (size 0x130, `dont_allow_addend:True`); the data words
  stay raw.
- **`Mc_Update`'s strings and jump table** are `force_not_migration` and `INCLUDE_RODATA` in place (the strings before
  the draft, the table in its `#else`), the clean build byte-identical.
- An exact float literal matters: fdiff's "different data through $gp" with one ulp off was the last difference of
  `func_003ACB08` (`-0.34906587f` gave 0xBEB2B8C3, the word is 0xBEB2B8C2 = `-0.34906584f`).
- **One more `--fix-asm` rule**: an `mfc1 $r` that gcc left to gas right before a `.set noreorder` block whose first
  instruction reads `$r` gets the nop the game's assembler put there (`func_003E92E0`; the modern gas adds none). The
  whole build stays byte-identical with it.
- A gas hazard nop between `cfc2.ni` and a branch disappears when the `cfc2.ni` is written as a `.word` inside the
  asm block (`func_003E8FA0`).
- The 128-bit save of s2 in `scene3d_c` (`func_003517F0`, `func_00351860`): a stub giving s2 a TImode use
  (`ASM_STUB_BEGIN`) makes both match, but the wide save then sticks to `func_00351978`, which the original saves
  with `sd`. Not adopted; the mechanism is still open.

Code shapes:

- **Store order and statement order by search.** Exhaustive permutation of marked statement groups (up to 7 lines)
  and a hill-climb that swaps or moves simple statements inside their run matched most of the store-order drafts of
  this batch (`func_003403C8`, `func_003D43A8`, `func_00340B30`, `func_003C9778`, `func_003B77B8` (35 lines),
  `func_003C3648` (70), `func_003CE770` (51), ...). A nonzero result of such a search can reorder a use before its
  definition: only a zero result (then fdiff) is kept, improved nonzero drafts were put back.
- Declaration order permutations and operand commutation (`a + b` / `b + a`) almost never changed the score.
- **A volatile read** (marked fake) reproduced `lhu` + `sll` / `sra` where combine made `lh` (`func_003BAC88`) and a
  load the original does after a copy loop (`func_003D4728`).
- A row pointer `s32 *d = (s32 *)((u8 *)p + i * 16)` set the `addu` operand order (`Fx_AddQuad`); `s32 o = 0xB8A0;
  g = (u8 *)gFighters + o` kept the address unfolded (`func_00342818`); a separate `u8 t` for an OR into a byte array
  (`func_003ADE18`); `y -= 7; y1 = y - 0x13;` kept two adds (`func_003C7428`); a pointer to the element before
  indexing at -0x20 (`func_003E06A0`); an if / else nest with the common case first (`PracFont_StrWidth`); case
  blocks in the order of the original's code (`func_003ACB08`).

More shapes from the helpers' passes:

- An `s32` function that only uses a bare `return;` keeps v0 live at its exit and changes the delay-slot fills;
  `void` fixes it. All conditions in one `if (...) return 1; return 0;` put the result straight in v0.
- `x = 0; if (eq) x = 1;` (or an explicit if / else) flips the movz / movn polarity; a `goto` to a shared `return 1`
  or fail block reproduces shared blocks; `do { ... } while (0)` around a switch and its return (marked fake) kept
  the result out of the jump-table index register.
- Polling loops on hardware registers match as a `goto` loop with plain reads at a constant address; inline asm
  blocks count as one instruction each for gcc's short-loop nop padding (`func_0035D588`).
- A field read through its own symbol (`D_004333D4`, `D_004177E8`), a callee prototype returning `s32` instead of
  `void` (`CamScript_Load`), and an earlier assignment that stops constant propagation (`move rX, rY` of a zero)
  fixed several register differences. `Pad_AddStickBits` had its logic inverted in the old draft.

Tools:

- A scratch comparator compiled one file with one draft enabled like the build and scored it against the
  `INCLUDE_ASM` form in about 0.3 s; the permutation and hill-climb searches ran on it over every draft of 60 lines or
  less (the parent's tools lived under `~/.cache` and were deleted at the end of the batch). Its object cache must be
  keyed on `configure.py` too, or a `--fix-asm` change is not seen.
- A shell loop `until [ "$(pgrep -fc NAME)" = 0 ]` never ends: `pgrep -f` matches the loop's own command line.

Matched (123): `Fx_AddQuad`, `func_003ADE18`, `func_003C9778`, `PracFont_StrWidth`, `func_003C1350`, `func_003420C8`,
`func_00342818`, `func_0034A570`, `func_003518B8`, `Model_Render`, `func_003ACB08`, `func_003403C8`, `func_003D43A8`,
`func_00340B30`, `func_003E06A0`, `func_003CF608`, `func_003D8D38`, `func_003CE920`, `func_003E8FA0`, `func_003ECDE8`,
`func_003C7428`, `func_003BAC88`, `func_003D4728`, `func_003A96B8`, `func_003CE770`, `func_003E9FD8`, `func_003B77B8`,
`func_003C3648`, `Bowl_PinHomePos` (render / modes files, 29); 66 in the simulation and system files (`fighter_core`
29, `battle_flow` 7, `dma_spr` 5, `stage` 6, `movie` 4, `zsort` 2, `practice` 2, `sound` 2, `main` 2, `fighter_setup`
1, `fighter_setup_b` 2, `font_tex` 1, `geometry` 1, `gfx` 1, `skeleton` 1); 28 more in the render / modes files
(`func_00359FB8`, `func_003DD098`, `func_003AFAA0`, `func_003BA480`, `func_003BB8B8`, `func_003BCFB8`, `func_003BD240`,
`func_003B2090`, `func_003B2358`, `func_003B2670`, `func_003B5DB0`, `func_003B6808`, `func_00341B70`, `func_0034D108`,
`func_0034E640`, `func_00354120`, `func_003BECC0`, `func_003BFFB0`, `func_003C0210`, `func_003C1078`, `func_003C8658`,
`func_003CC260`, `Sprite_DrawCurrentGouraud`, `func_003E92E0`, `func_003E9AE8`, `func_003E9C10`, `func_003ECF70`,
`Pause_BowlTop5`). Not matched: the 378 drafts of [t19_inventory.md](t19_inventory.md), each with a one-line note.

### Lessons from T19 batch 3 (2026-10-22)

Matched 4: `func_003BAD98` (load `a` before `b`, both set to 0 in the else branch), `Prac_DrawDemoPrompt`
(`a += 0x60;` on its own, `& 0xFF` moved into each call argument), `Jpeg_RgbToYcc` (row loop as
`i = 4; do { ...; i--; src += 0xEE8; } while (i > 0);`; its `LIT4_WORD` lines dropped, the compiler emits the same
pool words), `func_003CB508` (`p++` in the `for` header, stores reordered).

- **Small tables and `-G8`**: an extern declared with its size of 8 bytes or less (`u16 x[4]`) is read through `$gp`;
  the original addresses `INCLUDE_RODATA` tables with `lui` / `addiu`, so such externs must be incomplete arrays `[]`
  (`func_00381278`, `func_0038AFC0`).
- An `.sdata` word inside a two-element array has its own symbol (`D_005F5C44`); an extern scalar for it reproduces
  the relocation (`func_0037EB00`).
- `on = fl; if (!side) on = 0;` becomes `side ? fl : 0` (`move zero` + `movn`); the original's in-place
  `movz v1,zero,s1` after a copy is not reproduced yet.
- Stack slots of values spilled by gcse follow the expression hash-table bucket order, which depends on pseudo
  numbers; a variable declared inside a nested block gets a later pseudo, function-scope declaration order does not
  change it.
- A volatile `s16` compared with zero compiles to `lhu`, a volatile `s8` to `lbu` + shifts; a plain read gives
  `lh` / `lb` but lands in a delay slot.
- A fake empty `do {} while (0)` can give a later label `.p2align 3` (one alignment `nop`, `func_00353448`); an empty
  `__asm__ volatile("")` instead changes the scheduling around it.

Tools:

- The comparator of batch 2 was rebuilt on `scripts/permute.py`'s work dirs: `compile.sh` on an edited `base.c` and
  the decomp-permuter's `Scorer` on `target.o` (0.2 s per candidate). The `Scorer` asserts on `R_MIPS_PC16`
  relocations (the jump-table `.L` labels of `target.o`); a fallback scored those 26 drafts from a normalized objdump
  diff. Its score can be nonzero for identical code (call-target names, stack offsets): the check is a raw
  encoding and relocation diff, then fdiff.

### Lessons from T19 batch 4 (2026-10-22)

Matched 2 drafts. Both had their register choice set by how a callee was declared, not by their own code:

- `func_003D0E18` (`model_anim`) gets the counter `D_005F6F10` in v0 only when `func_003D1D98` is declared as
  returning a value.
- `func_003EFDB8` (`tekken_bowl`) gets its `i < 13` test in v1 only when `func_003EA4C8` is declared that way.

Both callees are now `s32`, with no `return` and no use of the result. The callee's own code does not change
(`func_003D1D98` still matches). A call to a function returning a value clobbers v0 in the RTL, and that changes the
allocation around the call. When a draft is off by a v0/v1 swap next to a call, try the callee as `s32` before
anything else. A sweep of one `void` callee at a time over all 203 register-allocation drafts found no other case.

The permuter found the first match: its randomizer changes prototypes and return types of other functions, not only
the target. When an output beats the base by more than the target's own diff, diff the whole `source.c`, not only
the target function.

Tools:

- `scripts/permute.py` assembles `target.o` with `jlabel` redefined as a plain label. splat's global `.L` labels made
  gas emit `R_MIPS_PC16` relocations for branches to them (encoded offset -1), and the `Scorer` asserted on them.
  `tests/test_permute.py` assembles a synthetic jump-table function and checks that there are no relocations and the
  offsets are resolved.
- The permuter scores the target with every other definition of `base.c` turned into a prototype (`extract_fn`).
  `func_003EFDB8` scores 10 alone and 15 in its file: `func_003EF818`, the C definition before it, changes its
  registers. `scripts/permuter_main.py` keeps the definitions (runtime patch, forked workers).
- The permuter's C parser drops `__asm__ volatile(...)` statements. In a file with inline asm, the full-file rewrite
  compiles to other code (`dma_gif`: 24 becomes 4879). `permute.py` scores that rewrite at setup and runs the
  permuter alone when it differs.
- The permuter ignores branch targets by default (`--no-ignore-branch-targets` turns that off). A near miss whose
  diff is a branch target scores 0 at once, so `--stop-on-zero` stops on the unchanged source.
- `permute.py` prints the base score with branch targets counted. All 203 register-allocation drafts were ranked by it
  (the inventory's "After T19 batch 4").

## Decompiled so far

| File | Range | C / functions | Notes |
|---|---|---:|---|
| `src/render_fighter_a.c` | 0x340000-0x341B28 | 16 / 18 | the stage renderer (module `render_fighter_a`, formerly `src/340000.c`): stage scene, depth merge sort, level of detail, per-frame scene update `func_00341AE8`, the scene set-up and build, the screen cull, the stage draw `func_00340E28` ([systems/render.md](systems/render.md) section 12); `.lit4` 0x5F5378-0x5F539C |
| `src/render_fighter_b.c` | 0x341B28-0x3420C8 | 6 / 6 | the VU1 model renderer: `Model_Draw`, `Model_Render`, `Model_SendMatrices`, `Model_SendParts` (section 12.3) |
| `src/vu1_packet.c` | 0x3420C8-0x342E78 | 10 / 12 | fighter shadows and fixed VIF1 / DIRECT packets (section 12.1); `.lit4` 0x5F57EC-0x5F5804 out of text order |
| `src/draw_list.c` | 0x356A70-0x3572A0 | 12 / 15 | model bank relocation and GS image uploads (section 12.4); ends with the padding before crt0 |
| `src/font_tex.c` | 0x3608B8-0x3621F8 | 27 / 30 | memory card screens and the GS image upload `Gfx_PkAddImage` (section 13); `.rodata` 0x4FC280-0x4FC610 |
| `src/main.c` | 0x357358-0x358A38 | 36 / 40 | boot, main loop, frame phases, mode dispatch, input state |
| `src/rng_misc.c` | 0x35C2B0-0x35D048 | 45 / 47 | RNG, string and table helpers, play counters |
| `src/pad.c` | 0x396D00-0x397948 | 10 / 11 | libpad/libmtap set-up sequence, per-player input |
| `src/file.c` | 0x397948-0x398510 | 29 / 29 | file requests, descrambling, IOP status |
| `src/gfx.c` | 0x398510-0x3994C0 | 48 / 51 | GS set-up, VBlank handler, display packet, helpers |
| `src/dma_gif.c` | 0x342E78-0x346398 | 43 / 52 | DMA kicks and waits, VU0 microcode upload, VU1 matrix packets, the fighter draw (`func_00343340`, `func_003439B0`, drafts) and its texture packets, side tables; the four hand-written VU0 routines are C around one asm block each (T19 b1) ([systems/render.md](systems/render.md) section 12.1) |
| `src/dma_spr.c` | 0x35D048-0x35E808 | 58 / 63 | file loads by number, scratchpad buffers and DMA, DMAC helpers |
| `src/memcard.c` | 0x35E808-0x3608B8 | 42 / 43 | memory card requests, libmc call steps, state flags |
| `src/sound.c` | 0x3745E8-0x376C30 | 61 / 65 | SPU2 set-up, sound effect voices and banks, delayed effects, fighter voices |
| `src/sound_if.c` | 0x399FC8-0x39A2B8 | 13 / 13 | sound byte-code queue (BGM, movie audio) |
| `src/gte.c` | 0x39A2B8-0x39BBF0 | 19 / 19 | GTE-style fixed-point matrices and vectors |
| `src/fighter_core.c` | 0x37BA88-0x38BF98 | 249 / 277 | practice mode recorder, command lists, hit markers and dummy actions, the CPU opponent (`Cpu_Think` and its deciders, commands, predicates and handlers, [systems/fighter.md](systems/fighter.md) section 4); `.rodata` 0x5064C0-0x507EE0 included in place (its 0x506528-0x506BF0 belongs to the practice object before) |
| `src/battle_flow.c` | 0x3682F8-0x3745E8 | 127 / 148 | the fight simulation: per-frame update, input fetch and history, command matching and situation tests, move start and move events, hits and throws (`Battle_ResolveHits`, `Fighter_StartThrownMove`), launches, the round state machine (`Battle_UpdateRound`) and results ([systems/fighter.md](systems/fighter.md) section 5); includes the four functions of the former `src/374520.c`; `.rodata` 0x4FF8D0-0x500780 included in place; damage `Fighter_ApplyDamage` and its `.lit4` 0x5F5168 |
| `src/fight_misc.c` | 0x376C30-0x3790C8 | 26 / 29 | the hit test (attack volumes against body spheres), body sphere radii, push sphere radii, the special attack volume of character 0x1D ([systems/fighter.md](systems/fighter.md) section 5.5); `.rodata` 0x506150-0x506260, `.lit4` 0x5F5170 |
| `src/select_results.c` | 0x3BDF60-0x3C1A78 | 65 / 72 | the select panels and menu quads, unlocks (`Unlock_*`), arcade opponents and random teams, member life by game type, team life at round start and its per-frame recovery ([systems/fighter.md](systems/fighter.md) section 5.6, [systems/modes.md](systems/modes.md) sections 7, 12); `.rodata` 0x51F150-0x51F678; ends at the `hud` cut |
| `src/model_anim.c` | 0x3D0660-0x3D3630 | 69 / 72 | round camera shots, the model hierarchy of the stage scenes (nodes, VU0 matrix stack `Model_*`, depth-sorted draw lists), the move log (`Fighter_LogMove`) and round-end bonus, the tag counters (`Tag_*`) and the tag swap ([systems/render.md](systems/render.md) section 11, [systems/fighter.md](systems/fighter.md) section 5.7); `.rodata` 0x520C40-0x521700, `.lit4` 0x5F5370-0x5F5378; the end is an object edge [V: data order], the start the module edge [I] |
| `src/model_anim_b.c` | 0x3D3630-0x3DB758 | 85 / 101 | the stage wind (`Wind_*`, Lorenz generators and gusts), the animated stage scenes, GS images of model files, the stage objects (update / VU1 draw hooks, planar shadows) ([systems/render.md](systems/render.md) section 11); `.rodata` 0x521B80-0x521CE0, `.lit4` 0x5F539C-0x5F5614 |
| `src/stage.c` | 0x38BF98-0x396D00 | 65 / 105 | the fight camera and its per-stage values (module `camera_stage`): the per-frame camera step `Cam_Update` on `gRoundPhase`, camera keys and blends, camera scripts, reaction cameras drawn at random, stage camera distances ([systems/stage.md](systems/stage.md)); `.rodata` 0x507EE0-0x509D00 included in place; `.lit4` 0x5F5174-0x5F519C (`LIT4_WORD` for the float functions still in assembly) |
| `src/skeleton.c` | 0x34E748-0x34FF10 | 14 / 16 | the fighter skeleton (module `skeleton`): per-frame update `Skel_Update`, the root matrix that writes the position `unk51C`..`unk524`, joint world matrices, body / push spheres and attack volumes on joints, joint screen positions ([systems/animation.md](systems/animation.md)); `.lit4` 0x5F5114-0x5F5154 (`LIT4_WORD` for `Skel_SetRootMatrix`) |
| `src/motion.c` | 0x3639F0-0x364448 | 17 / 19 | motion data (module `motion`): frame sampling, the delta / run-length frame decoder, motion banks and clips ([systems/animation.md](systems/animation.md)); `.rodata` 0x4FC610-0x4FC630 (the decoder's jump table) included in place |
| `src/hud.c` | 0x3C1A78-0x3C3150 | 10 / 12 | the fight HUD (`Hud_Draw`: life bars with red life, damage trail, partner bar and tag slide, round markers, timer; K.O. percentages), [systems/render.md](systems/render.md) section 5; a cut inside the `select_results` range |
| `src/sprite2d.c` | 0x3CD298-0x3D0660 | 50 / 59 | 2D sprites and text of the HUD and menus (`Sprite_*`), image banks, screen masks, record tables, name entry ([systems/render.md](systems/render.md) section 14); `.rodata` 0x520648-0x520C40, `.lit4` 0x5F5364-0x5F5370 |
| `src/model_draw.c` | 0x3DB758-0x3DC9C8 | 18 / 18 | 2D GS primitives (scissor, rectangles, gradient quad, menu window), stage-object draw hooks, text helpers ([systems/render.md](systems/render.md) section 6); no models |
| `src/scene3d.c` | 0x34C508-0x34E748 | 10 / 15 | fighters' secondary motion: per-side wind, extra-joint pendulums, accessory parts ([systems/render.md](systems/render.md) section 4); `.rodata` 0x51EA60-0x51EB00, `.lit4` 0x5F5350-0x5F535C |
| `src/scene3d_b.c` | 0x34FF10-0x3504C0 | 6 / 6 | the sword effect's packet senders (scratchpad headers); the end is an object edge (128-bit save mode, Lessons from T17 batch 1) |
| `src/scene3d_c.c` | 0x3504C0-0x351E98 | 10 / 15 | the sword trail and glow (`func_00350C40`), its history and spline, character 20's swing ([systems/render.md](systems/render.md) section 3); `.lit4` 0x5F5614-0x5F5668 out of text order |
| `src/scene3d_d.c` | 0x351E98-0x3543F0 | 35 / 37 | look-at, fighter lights (`Light_*`), GS image upload ([systems/render.md](systems/render.md) section 4); `.lit4` 0x5F5668-0x5F5700 |
| `src/effects.c` | 0x3A04B8-0x3A6460 | 55 / 58 | effect callbacks: bank sprites, stage ambient effects and lens flare, hit sparks, flashes and bursts, the hit log, body-part glows, move effects ([systems/render.md](systems/render.md) section 7); `.rodata` 0x5194E0-0x519940, `.lit4` 0x5F51A4-0x5F5300 |
| `src/effects_draw.c` | 0x3A6460-0x3A9678 | 47 / 50 | screen fade, title logo, stage texture layers, fighter texture channels, quad queue, character 0x1D's volume, effect helpers; a cut of the same object; `.rodata` 0x519940-0x519B30, `.lit4` 0x5F5300-0x5F531C |
| `src/battle_mode.c` | 0x358A38-0x35C2B0 | 64 / 67 | `Mode_Battle` (mode 4) and its per-game-type set-up and match-end handlers, arcade opponents, survival pairs, random teams, camera keys and hit-spark helpers ([systems/modes.md](systems/modes.md)); `.rodata` 0x4FB9C0-0x4FBB28 |
| `src/modes.c` | 0x3B90C0-0x3BB310 | 42 / 46 | `Mode_Change`, `Mode_Transition`, `Mode_Boot`, `Game_Start` and the per-type set-ups, the continue countdown, win streaks, controller vibration; `.rodata` 0x51E970-0x51EA60 |
| `src/modes_b.c` | 0x3BB310-0x3BDF60 | 37 / 40 | `Mode_Loading`, controller / multitap warnings, the fight's side status texts, `Mode_Title`, `Mode_MainMenu` and its unlock tests; `.rodata` 0x51EB00-0x51F150 |
| `src/loading_title.c` | 0x3A9678-0x3ADFA8 | 77 / 83 | mode 8: the ending sequence and script player, ending effects, the credits roll, character sets ([systems/modes.md](systems/modes.md) section 13); `.rodata` 0x519B30-0x51DE30, `.lit4` 0x5F531C-0x5F5340; built without `-ffast-math` |
| `src/menu_common.c` | 0x3ADFA8-0x3B1478 | 73 / 82 | menu camera, port assignment and join rules, select preview fighters, game-setup accessors; `.rodata` 0x51DE30-0x51DEF0, `.lit4` 0x5F5340-0x5F5350 |
| `src/options.c` | 0x3B1478-0x3B4D50 | 30 / 38 | `Mode_Options` and its pages ([systems/modes.md](systems/modes.md) section 10); `.rodata` 0x51DEF0-0x51E580 |
| `src/pause_results.c` | 0x3B4D50-0x3B90C0 | 68 / 74 | `Mode_Results`, `Pause_Update`, partner swaps between rounds, start positions (sections 8-9); `.rodata` 0x51E580-0x51E970 |
| `src/select_results_b.c` | 0x3C3150-0x3CD298 | 88 / 117 | `Mode_CharSelect`, results screens, records block, `Text_Printf`, name entry (sections 7-8); `.rodata` 0x51F6C0-0x520648, `.lit4` 0x5F535C-0x5F5364 |
| `src/theater.c` | 0x3543F0-0x355D30 | 7 / 10 | mode 12 `Mode_EnterTheater` and mode 13 `Mode_Theater` (endings, movies, music player) ([systems/modes.md](systems/modes.md) section 15); `.rodata` 0x5221E0-0x522B90 |
| `src/theater_b.c` | 0x355D30-0x3564C8 | 1 / 2 | modes 14-15, the music player; `.rodata` 0x522048-0x5221C0 |
| `src/theater_c.c` | 0x3564C8-0x3567A8 | 5 / 5 | fighter skeleton matrices, `Mtx_Unit` / `Mtx_Zero` (inline asm), vector helpers; `.rodata` 0x5221C0-0x5221E0 |
| `src/theater_d.c` | 0x3567A8-0x356A70 | 4 / 5 | GIF / VIF1 blocks through the scratchpad, DMA chain relocation, stage byte tables; `.rodata` 0x521CE0-0x521E20 |
| `src/demo.c` | 0x3DC9C8-0x3E1C68 | 38 / 51 | the practice command list and move demonstration (not the attract demo), shared 2D line / rect / triangle helpers (section 16); `.rodata` 0x522B90-0x522DE0, `.lit4` 0x5F5710-0x5F573C |
| `src/menu_bg.c` | 0x3E1C68-0x3E55C8 | 15 / 21 | the Tekken Bowl aiming HUD, text table helpers, screen fade, cursor bar (section 18); `.rodata` 0x522FC0-0x5233F0, `.lit4` 0x5F5804-0x5F58DC; built without `-ffast-math` |
| `src/gallery_jpeg.c` | 0x3E55C8-0x3E8AD8 | 25 / 32 | `Mode_Gallery`, the baseline JPEG encoder / decoder (`Jpeg_*`), a stage particle effect (section 18); `.rodata` 0x5233F0-0x523570, `.lit4` 0x5F58DC-0x5F596C; built without `-ffast-math` |
| `src/tekken_bowl.c` | 0x3E8AD8-0x3F3320 | 107 / 136 | Tekken Bowl (game type 8): pins, scoring, results texts, top 5, pause sub-screens, a bit stream, VU0 helpers (section 17); `.rodata` 0x523570-0x523FA0, `.lit4` 0x5F596C-0x5F5BCC; built without `-ffast-math` |
| `src/geometry.c` | 0x346398-0x347760 | 22 / 23 | object lists and effect callbacks, the handler table `D_005193B8` (T19 b1); `.lit4` 0x5F51A0 |
| `src/geometry_b.c` | 0x347760-0x34A7A8 | 10 / 26 | skeleton frames, weight blends, seven hand-written VU0 trig / rotation routines; `.rodata` 0x522DE0-0x522FC0, `.lit4` 0x5F573C-0x5F57EC |
| `src/geometry_c.c` | 0x34B4E8-0x34C508 | 2 / 4 | camera collision against an elliptic cylinder and an ellipsoid; `.lit4` 0x5F5160-0x5F5168 |
| `src/fighter_setup.c` | 0x3621F8-0x3639F0 | 29 / 33 | fighter matrices and set-up, replay recorder; `.lit4` 0x5F5100-0x5F5114 |
| `src/fighter_setup_b.c` | 0x364448-0x3682F8 | 99 / 107 | `Lz_Decompress`, fighter accessors, init functions of `Sys_Init`, texture packets; `.rodata` 0x4FC630-0x4FF8D0 (with `skeleton`'s and `gte`'s tables), `.lit4` 0x5F5154-0x5F5160 |
| `src/practice.c` | 0x3790C8-0x37BA88 | 40 / 44 | practice mode menus (`PracMenu`); `.rodata` 0x506260-0x5064C0 |
| `src/zsort.c` | 0x3994C0-0x399FC8 | 12 / 13 | z-sorted draw lists ("(zs)list over" checks) |
| `src/screenshot.c` | 0x39BBF0-0x39C7B8 | 19 / 20 | GTE-style screen helpers, `Scrn_Capture`, `func_0039C2C0`; `.rodata` 0x518E70-0x518EA8 |
| `src/movie.c` | 0x39C7B8-0x3A04B8 | 92 / 111 | MPEG movie player (Sony libmpeg sample style): IPU, thread, VBlank handler, ring buffer |

Functions left as `INCLUDE_ASM` carry their closest attempt above them in `#if 0` with a `NON_MATCHING:` note; in
`fighter_core.c`, `battle_flow.c` and `fight_misc.c` every remaining function has one (T14 b9); in `stage.c` every
remaining function has one that compiles when enabled (T15 b2), as in `skeleton.c` and `motion.c` (T16 b1) and the T17 files (`hud.c`, `scene3d*.c`, `effects.c`, `effects_draw.c`, `model_anim.c`, `model_anim_b.c`, `render_fighter_a.c`, `render_fighter_b.c`, `vu1_packet.c`, `draw_list.c`, `font_tex.c`, `dma_gif.c`, `sprite2d.c`) and the T18 files (`battle_mode.c`, `modes.c`, `modes_b.c`, `loading_title.c`, `menu_common.c`, `options.c`,
`pause_results.c`, `select_results.c`, `select_results_b.c`, and the batch 3 files `theater*.c`, `demo.c`, `menu_bg.c`,
`gallery_jpeg.c`, `tekken_bowl.c`): every function of the T18 files has C. Since T19 batch 1 every game function has C
([t19_inventory.md](t19_inventory.md)); what is left are drafts.
