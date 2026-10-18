#!/usr/bin/env python3
"""Names a symbol and replaces the placeholders of named symbols in the C sources.

Usage: .venv/bin/python scripts/apply_names.py [OLD NEW] [--dry-run]

With OLD NEW, first sets the name of OLD (a placeholder such as func_00340AF0, a name already in a symbol file,
or an address) to NEW in config/symbols/: in the file that already has the address, or else in the file whose
`// range:` lines contain it. Then, in every file under src/ and include/, replaces each func_XXXXXXXX /
D_XXXXXXXX whose address has a name in the symbol files with that name, and a renamed symbol's former name with
the new one (whole identifiers only; not struct members after `.` or `->`, not inside string or character
literals). Every file is written whole (a temporary file, then renamed over the old one).

splat reads the symbol files, so run `configure.py --split` afterwards: the split assembly and the
asm/nonmatchings/ file names then carry the new names (INCLUDE_ASM lines have already been updated here).
Naming rules: docs/decomp_guide.md, "Naming".
"""

import argparse
import os
import re
import sys
import tempfile
from pathlib import Path

import fdiff

ROOT = fdiff.ROOT
# End of .text (docs/text_map.md): a name at or after it is data.
TEXT_END = 0x415700
TOKEN = re.compile(r"\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'|\d\w*|[A-Za-z_]\w*")


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


def rename_identifier(text, old, new):
    """text with the C identifier old replaced by new: whole identifiers only, not a member after `.` or `->`, not
    inside a string or character literal."""
    def sub(m):
        word = m.group(0)
        if word != old:
            return word
        before = text[:m.start()].rstrip()
        return word if before.endswith((".", "->")) else new
    return TOKEN.sub(sub, text)


def resolve(old, names):
    """The address OLD stands for: a placeholder, a name in the symbol files, or an address."""
    addr = fdiff.placeholder_addr(old)
    if addr is None:
        addr = names.get(old)
    if addr is None and re.fullmatch(r"0x[0-9A-Fa-f]+", old):
        addr = int(old, 16)
    if addr is None:
        raise ValueError(f"{old} is neither a placeholder, a name in {fdiff.SYMBOL_DIR}, nor an address")
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


def load_symbols(root=ROOT):
    """path (relative to root) -> text of every symbol file."""
    return {path: (Path(root) / path).read_text() for path in fdiff.symbol_files(root)}


def file_ranges(files):
    """[(start, end, path)] from the `// range:` lines of the symbol files (path -> text), by address. Raises
    ValueError when two ranges overlap: an address must have one file."""
    ranges = sorted((lo, hi, path) for path, text in files.items() for lo, hi in fdiff.parse_file_ranges(text))
    for (lo, hi, a), (lo2, hi2, b) in zip(ranges, ranges[1:]):
        if lo2 < hi:
            raise ValueError(f"ranges overlap: {a} {lo:#x}-{hi:#x} and {b} {lo2:#x}-{hi2:#x}")
    return ranges


def route(addr, ranges):
    """The symbol file whose range contains addr (ranges from file_ranges)."""
    for lo, hi, path in ranges:
        if lo <= addr < hi:
            return path
    raise ValueError(f"no file in {fdiff.SYMBOL_DIR} has a range with 0x{addr:08X}: add a `// range:` line")


def misplaced(files):
    """[(path, name, address)] of the entries outside the ranges of their own file."""
    out = []
    for path, text in files.items():
        ranges = fdiff.parse_file_ranges(text)
        for name, addr in fdiff.parse_symbol_addrs(text).items():
            if not any(lo <= addr < hi for lo, hi in ranges):
                out.append((path, name, addr))
    return out


def write_atomic(path, text):
    """Writes text to path through a temporary file in the same folder, renamed over it (keeps the file mode)."""
    path = Path(path)
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.", suffix=".tmp")
    try:
        with os.fdopen(fd, "w") as f:
            f.write(text)
        if path.exists():
            os.chmod(tmp, path.stat().st_mode & 0o7777)
        os.replace(tmp, path)
    except BaseException:
        os.unlink(tmp)
        raise


def all_names(files):
    names = {}
    for text in files.values():
        names.update(fdiff.parse_symbol_addrs(text))
    return names


def apply(root, old=None, new=None, dry_run=False, out=sys.stdout):
    """Names OLD as NEW (when given), then updates the sources under root. Raises ValueError on a bad request,
    before anything is written."""
    root = Path(root)
    files = load_symbols(root)
    names = all_names(files)
    renamed = None
    if old is not None:
        if not valid_name(new):
            raise ValueError(f"{new} is not a valid name")
        addr = resolve(old, names)
        if new in names and names[new] != addr:
            raise ValueError(f"{new} already names {names[new]:#x}")
        path = next((p for p, text in files.items() if addr in fdiff.parse_symbol_addrs(text).values()), None)
        path = path or route(addr, file_ranges(files))
        kind = "data" if old.startswith("D_") or addr >= TEXT_END else "func"
        files[path] = set_symbol(files[path], new, addr, kind)
        print(f"{path}: {new} = 0x{addr:08X}", file=out)
        if not dry_run:
            write_atomic(root / path, files[path])
        if old in names and old != new:
            renamed = (old, new)
        names = all_names(files)
    mapping = rename_map(names)

    for src in sorted((root / "src").rglob("*.c")) + sorted((root / "include").rglob("*.h")):
        text = src.read_text()
        changed = rename_text(text, mapping)
        if renamed:
            changed = rename_identifier(changed, *renamed)
        if changed != text:
            print(f"{'would update' if dry_run else 'updated'} {src.relative_to(root)}", file=out)
            if not dry_run:
                write_atomic(src, changed)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("names", nargs="*", metavar="OLD NEW")
    parser.add_argument("--dry-run", action="store_true", help="print what would change, write nothing")
    args = parser.parse_args()
    if len(args.names) not in (0, 2):
        parser.error("give OLD NEW, or nothing to only apply the symbol files")
    try:
        apply(ROOT, *args.names, dry_run=args.dry_run)
    except ValueError as e:
        sys.exit(f"apply_names.py: {e}")


if __name__ == "__main__":
    main()
