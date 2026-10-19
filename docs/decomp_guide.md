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
| Flags | `-O2 -fno-strength-reduce -ffast-math -fno-strict-aliasing` |
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
modern gas in nearly everything, including filling delay slots in reorder mode and the hazard nop after an FPU
compare (`c.le.s / nop / bc1t` is in the original). Four instructions differ, each checked against original
bytes:

| Compiler writes | Original | Modern gas | Prelude |
|---|---|---|---|
| `mtc1 $4,$f0` / `cvt.s.w $f0,$f0` | no nop between | hazard nop | macro to `.word` (T13) |
| `mfc0 $2,$12` / `and $2,$2,$3` | no nop between | hazard nop | macro to `.word` (T13) |
| `mfc1 $2,$f1` / `subu $2,$0,$2` | no nop between | hazard nop | macro to `.word` (T14) |
| `move $4,$5` | `daddu` | `or` | macro to `daddu` |
| `break 7` | code in bits 6-15 (`0x000001CD`) | code in bits 16-25 | `.word` |
| `cvt.w.s $f2,$f1` | function 0x24 (`0x460008A4`) | refused for the R5900 | `.word` |
| `sqrt.s $f12,$f12` | operand in the ft field (`0x460C0304`) | operand in fs | `.word` |

The prelude is also read in front of `INCLUDE_ASM` code. `break`, `cvt.w.s`, `sqrt.s`, `mtc1`, `mfc0` and `mfc1` become
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

Assembler limits (both need a change of `include/gcc_prelude.inc`, a follow-up):

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

## Decompiled so far

| File | Range | C / functions | Notes |
|---|---|---:|---|
| `src/340000.c` | 0x340000-0x340B30 | 2 / 11 | `func_00340000`, `func_00340AF0` |
| `src/main.c` | 0x357358-0x358A38 | 34 / 40 | boot, main loop, frame phases, mode dispatch, input state |
| `src/rng_misc.c` | 0x35C2B0-0x35D048 | 45 / 47 | RNG, string and table helpers, play counters |
| `src/pad.c` | 0x396D00-0x397948 | 10 / 11 | libpad/libmtap set-up sequence, per-player input |
| `src/file.c` | 0x397948-0x398510 | 29 / 29 | file requests, descrambling, IOP status |
| `src/gfx.c` | 0x398510-0x3994C0 | 47 / 51 | GS set-up, VBlank handler, display packet, helpers |
| `src/dma_gif.c` | 0x342E78-0x346398 | 38 / 52 | DMA kicks and waits, VU0 microcode upload, VU1 matrix packets, side tables; four handwritten VU0 routines stay in assembly |
| `src/dma_spr.c` | 0x35D048-0x35E808 | 53 / 63 | file loads by number, scratchpad buffers and DMA, DMAC helpers |
| `src/memcard.c` | 0x35E808-0x3608B8 | 42 / 43 | memory card requests, libmc call steps, state flags |
| `src/sound.c` | 0x3745E8-0x376C30 | 59 / 65 | SPU2 set-up, sound effect voices and banks, delayed effects, fighter voices |
| `src/sound_if.c` | 0x399FC8-0x39A2B8 | 13 / 13 | sound byte-code queue (BGM, movie audio) |
| `src/gte.c` | 0x39A2B8-0x39BBF0 | 19 / 19 | GTE-style fixed-point matrices and vectors |
| `src/fighter_core.c` | 0x37BA88-0x38BF98 | 220 / 277 | practice mode recorder, command lists, hit markers and dummy actions, the CPU opponent (`Cpu_Think` and its deciders, commands, predicates and handlers, [systems/fighter.md](systems/fighter.md) section 4); `.rodata` 0x5064C0-0x507EE0 included in place (its 0x506528-0x506BF0 belongs to the practice object before) |
| `src/battle_flow.c` | 0x3682F8-0x3745E8 | 120 / 148 | the fight simulation: per-frame update, input fetch and history, command matching and situation tests, move start and move events, hits and throws (`Battle_ResolveHits`, `Fighter_StartThrownMove`), launches, the round state machine (`Battle_UpdateRound`) and results ([systems/fighter.md](systems/fighter.md) section 5); includes the four functions of the former `src/374520.c`; `.rodata` 0x4FF8D0-0x500780 included in place; damage `Fighter_ApplyDamage` and its `.lit4` 0x5F5168 |
| `src/fight_misc.c` | 0x376C30-0x3790C8 | 26 / 29 | the hit test (attack volumes against body spheres), body sphere radii, push sphere radii, the special attack volume of character 0x1D ([systems/fighter.md](systems/fighter.md) section 5.5); `.rodata` 0x506150-0x506260, `.lit4` 0x5F5170 |
| `src/select_results.c` | 0x3C1470-0x3C1A78 | 2 / 4 | team life at round start and its per-frame recovery ([systems/fighter.md](systems/fighter.md) section 5.6); a cut inside the `select_results` module, not an object edge |
| `src/model_anim.c` | 0x3D3288-0x3D33F0 | 1 / 1 | the tag swap ([systems/fighter.md](systems/fighter.md) section 5.7); a cut inside the `model_anim` module, not an object edge |
| `src/stage.c` | 0x38BF98-0x396D00 | 58 / 105 | the fight camera and its per-stage values (module `camera_stage`): the per-frame camera step `Cam_Update` on `gRoundPhase`, camera keys and blends, camera scripts, reaction cameras drawn at random, stage camera distances ([systems/stage.md](systems/stage.md)); `.rodata` 0x507EE0-0x509D00 included in place; `.lit4` 0x5F5174-0x5F519C (`LIT4_WORD` for the float functions still in assembly) |

Functions left as `INCLUDE_ASM` carry their closest attempt above them in `#if 0` with a `NON_MATCHING:` note; in
`fighter_core.c`, `battle_flow.c` and `fight_misc.c` every remaining function has one (T14 b9); in `stage.c` every
remaining function has one that compiles when enabled (T15 b2).
