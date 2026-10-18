#!/usr/bin/env python3
"""Reports how much of the code is decompiled to C, from the last linked build.

Usage: .venv/bin/python scripts/progress.py   (after ninja)

Reads the symbol table of build/SLUS_200.01.elf. Every function is a FUNC symbol; one still in assembly (a split
file or an INCLUDE_ASM) also has the `<name>.NON_MATCHING` marker splat's macro defines, one compiled from C has
none. FUNC symbols of size 0 (entry points inside other functions) are not counted. Sizes are the symbol
sizes, so alignment padding between functions is not counted.

Library code (Sony SDK, libc, libgcc) is linked as it is, not decompiled: the address ranges in
config/library_ranges.txt (optional; one `0xSTART 0xEND [label]` per line, end excluded, `#` comments, no
overlaps) are counted apart from the game code, with one line per label (functions, how many are named, bytes).
"""

import re
import sys

import fdiff

ROOT = fdiff.ROOT
RANGES = "config/library_ranges.txt"
MARKER = ".NON_MATCHING"


def parse_labeled_ranges(text):
    """[(start, end, label)] from the ranges file, by address; the label defaults to "library". Raises ValueError on
    a malformed line or on overlapping ranges."""
    ranges = []
    for n, line in enumerate(text.splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        m = re.fullmatch(r"(0x[0-9A-Fa-f]+)\s+(0x[0-9A-Fa-f]+)(?:\s+(\S.*))?", line)
        if not m or int(m.group(1), 16) >= int(m.group(2), 16):
            raise ValueError(f"line {n}: expected `0xSTART 0xEND [label]` with START < END: {line}")
        ranges.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3) or "library"))
    ranges.sort()
    for (lo, hi, a), (lo2, hi2, b) in zip(ranges, ranges[1:]):
        if lo2 < hi:
            raise ValueError(f"ranges overlap: {a} {lo:#x}-{hi:#x} and {b} {lo2:#x}-{hi2:#x}")
    return ranges


def parse_ranges(text):
    """[(start, end)] from the ranges file (see parse_labeled_ranges)."""
    return [(lo, hi) for lo, hi, _ in parse_labeled_ranges(text)]


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


def tally_by_label(syms, labeled):
    """[(label, {funcs, named, bytes})] per library label, in address order of each label's first range; a function
    counts as named when its name is not a splat placeholder."""
    out = {label: {"funcs": 0, "named": 0, "bytes": 0} for _, _, label in labeled}
    for name, (addr, size) in fdiff.functions(syms).items():
        for lo, hi, label in labeled:
            if lo <= addr < hi:
                t = out[label]
                t["funcs"] += 1
                t["bytes"] += size
                t["named"] += fdiff.placeholder_addr(name) is None
                break
    return list(out.items())


def percent(part, total):
    return 100 * part / total if total else 0.0


def line(label, t):
    return (f"{label}: {t['funcs_c']} / {t['funcs']} functions ({percent(t['funcs_c'], t['funcs']):.2f}%), "
            f"{t['bytes_c']:#x} / {t['bytes']:#x} bytes ({percent(t['bytes_c'], t['bytes']):.2f}%) in C")


def main():
    if not (ROOT / fdiff.ELF).exists():
        sys.exit(f"progress.py: no {fdiff.ELF}, run ninja first")
    labeled = parse_labeled_ranges((ROOT / RANGES).read_text()) if (ROOT / RANGES).exists() else []
    library = [(lo, hi) for lo, hi, _ in labeled]
    syms = fdiff.parse_symbols(fdiff.sh(ROOT / (fdiff.BINUTILS + "readelf"), "-sW", ROOT / fdiff.ELF))
    game, lib = tally(syms, library)
    if library:
        print(line("game", game))
        print(line("library", lib))
        for label, t in tally_by_label(syms, labeled):
            print(f"  {label}: {t['funcs']} functions ({t['named']} named), {t['bytes']:#x} bytes")
    else:
        print(line("all code (no config/library_ranges.txt yet)", game))


if __name__ == "__main__":
    main()
