#!/usr/bin/env python3
"""Names a symbol and replaces the placeholders of named symbols in the C sources.

Usage: .venv/bin/python scripts/apply_names.py [OLD NEW] [--dry-run]

With OLD NEW, first sets the name of OLD (a placeholder such as func_00340AF0, a name already in the symbol
file, or an address) to NEW in config/symbol_addrs.txt. Then, in every file under src/ and include/, replaces
each func_XXXXXXXX / D_XXXXXXXX whose address has a name in the symbol file with that name, and a renamed
symbol's former name with the new one.

splat reads the symbol file, so run `configure.py --split` afterwards: the split assembly and the
asm/nonmatchings/ file names then carry the new names (INCLUDE_ASM lines have already been updated here).
"""

import argparse
import re
import sys

import fdiff

ROOT = fdiff.ROOT
SYMBOL_FILE = fdiff.SYMBOL_FILES[0]
PLACEHOLDER_WORD = re.compile(r"\b(?:func|D)_[0-9A-F]{8}\b")


def valid_name(name):
    """A C identifier that is not itself a placeholder."""
    return bool(re.fullmatch(r"[A-Za-z_]\w*", name)) and fdiff.placeholder_addr(name) is None


def rename_map(names):
    """placeholder -> name for every named address (from name -> address). Both spellings (func_, D_) are mapped:
    only the one splat used for that address occurs in the sources."""
    out = {}
    for name, addr in names.items():
        if valid_name(name):
            out[f"func_{addr:08X}"] = name
            out[f"D_{addr:08X}"] = name
    return out


def rename_text(text, mapping):
    """text with whole-word occurrences of each key of mapping replaced."""
    if not mapping:
        return text
    words = re.compile(r"\b(" + "|".join(map(re.escape, sorted(mapping, key=len, reverse=True))) + r")\b")
    return words.sub(lambda m: mapping[m.group(1)], text)


def resolve(old, names):
    """The address OLD stands for: a placeholder, a name in the symbol file, or an address."""
    addr = fdiff.placeholder_addr(old)
    if addr is None:
        addr = names.get(old)
    if addr is None and re.fullmatch(r"0x[0-9A-Fa-f]+", old):
        addr = int(old, 16)
    if addr is None:
        raise ValueError(f"{old} is neither a placeholder, a name in {SYMBOL_FILE}, nor an address")
    return addr


def set_symbol(text, name, addr, kind):
    """Symbol file text with `name = addr; // type:kind`: replaces the line of addr, or appends one."""
    names = fdiff.parse_symbol_addrs(text)
    if name in names and names[name] != addr:
        raise ValueError(f"{name} already names {names[name]:#x}")
    entry = f"{name} = 0x{addr:08X};"
    lines = text.splitlines(keepends=True)
    for i, line in enumerate(lines):
        m = fdiff.SYMBOL_LINE.match(line)
        if m and int(m.group(2), 16) == addr:
            lines[i] = entry + line[m.end():]
            return "".join(lines)
    if text and not text.endswith("\n"):
        text += "\n"
    return text + f"{entry} // type:{kind}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("names", nargs="*", metavar="OLD NEW")
    parser.add_argument("--dry-run", action="store_true", help="print what would change, write nothing")
    args = parser.parse_args()
    if len(args.names) not in (0, 2):
        parser.error("give OLD NEW, or nothing to only apply the symbol file")

    path = ROOT / SYMBOL_FILE
    text = path.read_text() if path.exists() else ""
    names = fdiff.parse_symbol_addrs(text)
    mapping = {}
    if args.names:
        old, new = args.names
        if not valid_name(new):
            sys.exit(f"apply_names.py: {new} is not a valid name")
        try:
            addr = resolve(old, names)
            kind = "data" if old.startswith("D_") else "func"
            text = set_symbol(text, new, addr, kind)
        except ValueError as e:
            sys.exit(f"apply_names.py: {e}")
        if old in names:
            mapping[old] = new
        names = fdiff.parse_symbol_addrs(text)
        print(f"{SYMBOL_FILE}: {new} = 0x{addr:08X}")
        if not args.dry_run:
            path.write_text(text)
    mapping.update(rename_map(names))

    for src in sorted((ROOT / "src").rglob("*.c")) + sorted((ROOT / "include").rglob("*.h")):
        old = src.read_text()
        new = rename_text(old, mapping)
        if new != old:
            print(f"{'would update' if args.dry_run else 'updated'} {src.relative_to(ROOT)}")
            if not args.dry_run:
                src.write_text(new)


if __name__ == "__main__":
    main()
