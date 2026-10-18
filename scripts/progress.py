#!/usr/bin/env python3
"""Reports how much of the code is decompiled to C, from the last linked build.

Usage: .venv/bin/python scripts/progress.py   (after ninja)

Reads the symbol table of build/SLUS_200.01.elf. Every function is a FUNC symbol; one still in assembly (a split
file or an INCLUDE_ASM) also has the `<name>.NON_MATCHING` marker splat's macro defines, one compiled from C has
none. FUNC symbols of size 0 (entry points inside other functions) are not counted. Sizes are the symbol
sizes, so alignment padding between functions is not counted.

Library code (Sony SDK, libc, libgcc) is linked as it is, not decompiled: the address ranges in
config/library_ranges.txt (optional; one `0xSTART 0xEND [label]` per line, end excluded, `#` comments) are
counted apart from the game code.
"""

import re
import sys

import fdiff

ROOT = fdiff.ROOT
RANGES = "config/library_ranges.txt"
MARKER = ".NON_MATCHING"


def parse_ranges(text):
    """[(start, end)] from the ranges file; raises ValueError on a malformed line."""
    ranges = []
    for n, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        m = re.fullmatch(r"(0x[0-9A-Fa-f]+)\s+(0x[0-9A-Fa-f]+)(\s+\S.*)?", line)
        if not m or int(m.group(1), 16) >= int(m.group(2), 16):
            raise ValueError(f"line {n}: expected `0xSTART 0xEND [label]` with START < END: {line}")
        ranges.append((int(m.group(1), 16), int(m.group(2), 16)))
    return ranges


def in_ranges(addr, ranges):
    return any(lo <= addr < hi for lo, hi in ranges)


def tally(syms, library):
    """(game, library) counters of functions and bytes, in total and in C, from parse_symbols() output."""
    game, lib = ({"funcs": 0, "funcs_c": 0, "bytes": 0, "bytes_c": 0} for _ in range(2))
    for name, (addr, size) in fdiff.functions(syms).items():
        c = name + MARKER not in syms
        t = lib if in_ranges(addr, library) else game
        t["funcs"] += 1
        t["bytes"] += size
        t["funcs_c"] += c
        t["bytes_c"] += size if c else 0
    return game, lib


def percent(part, total):
    return 100 * part / total if total else 0.0


def line(label, t):
    return (f"{label}: {t['funcs_c']} / {t['funcs']} functions ({percent(t['funcs_c'], t['funcs']):.2f}%), "
            f"{t['bytes_c']:#x} / {t['bytes']:#x} bytes ({percent(t['bytes_c'], t['bytes']):.2f}%) in C")


def main():
    if not (ROOT / fdiff.ELF).exists():
        sys.exit(f"progress.py: no {fdiff.ELF}, run ninja first")
    library = parse_ranges((ROOT / RANGES).read_text()) if (ROOT / RANGES).exists() else []
    syms = fdiff.parse_symbols(fdiff.sh(ROOT / (fdiff.BINUTILS + "readelf"), "-sW", ROOT / fdiff.ELF))
    game, lib = tally(syms, library)
    if library:
        print(line("game", game))
        print(line("library", lib))
    else:
        print(line("all code (no config/library_ranges.txt yet)", game))


if __name__ == "__main__":
    main()
