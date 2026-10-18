#!/usr/bin/env python3
"""Compares one function of the linked build with the original executable.

Usage: .venv/bin/python scripts/fdiff.py <function or address> [--no-build]

Runs `ninja build/SLUS_200.01` first (--no-build skips it), then takes the function's bytes from the flattened
build (build/SLUS_200.01) and from iso/SLUS_200.01. Prints `OK` only when the instruction words are identical
and every load or store through $gp reaches the same data in both: the same bytes (a .lit4 constant one ulp
off differs) and, where both sides have a name for the address, the same name (two globals swapped at the
same offset differ). Otherwise it prints the data that differs, or both disassemblies side by side, aligned,
with a mark on each line that differs:

  !  different instruction        <  only in the original        >  only in the build
  ~  same instruction, different call or branch target (relocated: something else moved; still a difference)

The function's address and size in the build come from the symbol table of build/SLUS_200.01.elf; its original
address from its placeholder name (func_XXXXXXXX) or config/symbols/*.txt. The original function ends where
the next one starts; the alignment nops after it are dropped while it is longer than the build's.
Exit status: 0 when it prints OK, 1 otherwise.
"""

import argparse
import difflib
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ELF = "build/SLUS_200.01.elf"
BUILT = "build/SLUS_200.01"
ORIGINAL = "iso/SLUS_200.01"
# Symbol files (splat format), one per library or game module; each lists in `// range:` lines the addresses it
# names (docs/decomp_guide.md, "Naming").
SYMBOL_DIR = "config/symbols"
SCRATCH = "build/fdiff"
BINUTILS = "tools/binutils/mips-ps2-decompals-"
# Address of file offset 0 (config/SLUS_200.01.yaml: the header segment): address = file offset + LOAD_BASE.
LOAD_BASE = 0x33F000
# $gp of the original (config/SLUS_200.01.yaml, gp_value); the build's is its `_gp` symbol.
ORIGINAL_GP = 0x5FD0F0
GP = 28
# Bytes accessed by the R5900 loads and stores, by opcode: lb lh lw lbu lhu lwu sb sh sw ld sd lq sq lwc1 swc1
# lqc2 sqc2.
ACCESS_SIZE = {0x20: 1, 0x21: 2, 0x23: 4, 0x24: 1, 0x25: 2, 0x27: 4, 0x28: 1, 0x29: 2, 0x2B: 4, 0x37: 8, 0x3F: 8,
               0x1E: 16, 0x1F: 16, 0x31: 4, 0x39: 4, 0x36: 16, 0x3E: 16}

PLACEHOLDER = re.compile(r"(?:func|D)_([0-9A-F]{8})")
SYMBOL_LINE = re.compile(r"\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;")
RANGE_LINE = re.compile(r"\s*//\s*range:\s*(0x[0-9A-Fa-f]+)\s+(0x[0-9A-Fa-f]+)\s*$")
BRANCH = re.compile(r"(b\w*|j|jal)\t(?:.*,)?(0x[0-9a-f]+)$")


def parse_symbols(readelf):
    """name -> (address, size, type) from `readelf -sW` output, for FUNC and OBJECT symbols."""
    syms = {}
    for line in readelf.splitlines():
        p = line.split()
        if len(p) == 8 and p[3] in ("FUNC", "OBJECT") and p[6] != "UND":
            syms[p[7]] = (int(p[1], 16), int(p[2], 0), p[3])
    return syms


def gp_value(readelf):
    """Value of the `_gp` symbol in `readelf -sW` output, or None."""
    for line in readelf.splitlines():
        p = line.split()
        if len(p) == 8 and p[7] == "_gp":
            return int(p[1], 16)
    return None


def functions(syms):
    """name -> (address, size) of the functions among parse_symbols() output. FUNC symbols of size 0 are entry
    points or labels inside another function and are left out."""
    return {n: (a, s) for n, (a, s, t) in syms.items() if t == "FUNC" and s > 0}


def placeholder_addr(name):
    """The address a splat placeholder name (func_XXXXXXXX, D_XXXXXXXX) stands for, or None."""
    m = PLACEHOLDER.fullmatch(name)
    return int(m.group(1), 16) if m else None


def parse_symbol_addrs(text):
    """name -> address from a splat symbol file (`name = 0xADDR; // attributes`)."""
    return {m.group(1): int(m.group(2), 16) for m in map(SYMBOL_LINE.match, text.splitlines()) if m}


def parse_file_ranges(text):
    """[(start, end)] from the `// range: 0xSTART 0xEND` lines of a symbol file (end excluded)."""
    ranges = []
    for m in map(RANGE_LINE.match, text.splitlines()):
        if m:
            lo, hi = int(m.group(1), 16), int(m.group(2), 16)
            if lo >= hi:
                raise ValueError(f"range {m.group(1)} {m.group(2)}: start must be below end")
            ranges.append((lo, hi))
    return ranges


def symbol_files(root=ROOT):
    """The symbol files, as paths relative to root, sorted."""
    return sorted(p.relative_to(root).as_posix() for p in (Path(root) / SYMBOL_DIR).glob("*.txt"))


def parse_address(arg):
    """An address given on the command line (0x340AF0 or 00340AF0), or None for a name."""
    return int(arg, 16) if re.fullmatch(r"(0x)?[0-9A-Fa-f]{6,8}", arg) else None


def original_end(starts, addr, size):
    """End of the original function at addr: the next function start, or addr + size for the last one."""
    return next((s for s in sorted(starts) if s > addr), addr + size)


def parse_objdump(text):
    """(address, word, instruction text) of each line of `objdump -D -z -b binary` output."""
    rows = []
    for m in re.finditer(r"^\s+([0-9a-f]+):\t([0-9a-f]{8}) \t(.*)$", text, re.M):
        rows.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3).strip()))
    return rows


def normalize(text, base, size, names):
    """Instruction text with call and branch targets made independent of where the code was linked: a target
    inside the function becomes .+offset, one outside it the name of the function there (when known)."""
    m = BRANCH.match(text)
    if not m:
        return text
    target = int(m.group(2), 16)
    if base <= target < base + size:
        new = f".+{target - base:#x}"
    else:
        new = names.get(target, m.group(2))
    return text[:m.start(2)] + new


def strip_padding(rows, length):
    """rows without trailing nops (word 0), removed only while there are more than `length` rows."""
    rows = list(rows)
    while len(rows) > length and rows[-1][1] == 0:
        rows.pop()
    return rows


def align(orig, mine, norm_orig, norm_mine):
    """Aligns two lists of (address, word, text) rows on their normalized texts. Returns (mark, orig row or None,
    build row or None) per line; marks as in the module description, ' ' for identical words."""
    a = [norm_orig(r) for r in orig]
    b = [norm_mine(r) for r in mine]
    out = []
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        if tag == "equal":
            for i, j in zip(range(i1, i2), range(j1, j2)):
                out.append((" " if orig[i][1] == mine[j][1] else "~", orig[i], mine[j]))
            continue
        n = max(i2 - i1, j2 - j1)
        for k in range(n):
            left = orig[i1 + k] if i1 + k < i2 else None
            right = mine[j1 + k] if j1 + k < j2 else None
            out.append(("!" if left and right else "<" if left else ">", left, right))
    return out


def gp_access(word):
    """(offset, size) of a load or store relative to $gp, or None for any other instruction."""
    size = ACCESS_SIZE.get(word >> 26)
    if size is None or (word >> 21) & 31 != GP:
        return None
    off = word & 0xFFFF
    return off - 0x10000 if off & 0x8000 else off, size


def compare(orig, mine, orig_gp, mine_gp, look_orig, look_mine):
    """(ok, data) for two lists of (address, word, text) rows. ok only when the words are identical and data is
    empty. data lists, when the words are identical, each load or store through $gp whose target differs:
    (row index, original address, build address, (name, bytes) in the original, (name, bytes) in the build),
    from look_orig / look_mine(address, size) -> (name or None, bytes). Names are compared only when both
    sides have one."""
    if [r[1] for r in orig] != [r[1] for r in mine]:
        return False, []
    data = []
    for i, (_, word, _) in enumerate(orig):
        access = gp_access(word)
        if access is None:
            continue
        off, size = access
        a, b = orig_gp + off, mine_gp + off
        (an, ab), (bn, bb) = look_orig(a, size), look_mine(b, size)
        if ab != bb or (an and bn and an != bn):
            data.append((i, a, b, (an, ab), (bn, bb)))
    return not data, data


def sh(*cmd):
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, check=True, capture_output=True, text=True).stdout


def disassemble(data, addr, name):
    blob = ROOT / SCRATCH / name
    blob.parent.mkdir(parents=True, exist_ok=True)
    blob.write_bytes(data)
    return parse_objdump(sh(ROOT / (BINUTILS + "objdump"), "-D", "-z", "-b", "binary", "-m", "mips:5900", "-EL",
                            f"--adjust-vma={addr:#x}", blob))


def read_at(path, addr, size):
    with open(ROOT / path, "rb") as f:
        f.seek(addr - LOAD_BASE)
        return f.read(size)


def known_names(root=ROOT):
    """name -> address over all symbol files."""
    names = {}
    for path in symbol_files(root):
        names.update(parse_symbol_addrs((Path(root) / path).read_text()))
    return names


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("function", help="function name or address")
    parser.add_argument("--no-build", action="store_true", help="do not run ninja first")
    args = parser.parse_args()

    if not args.no_build:
        r = subprocess.run(["ninja", BUILT], cwd=ROOT, capture_output=True, text=True)
        if r.returncode:
            sys.exit(r.stdout + r.stderr + "fdiff.py: the build failed")
    readelf = sh(ROOT / (BINUTILS + "readelf"), "-sW", ROOT / ELF)
    syms = parse_symbols(readelf)
    funcs = functions(syms)
    named = known_names()

    def orig_addr(name):
        return named.get(name, placeholder_addr(name) or funcs[name][0])

    addr = parse_address(args.function)
    if addr is not None:
        name = next((n for n, (a, _) in funcs.items() if a == addr), None) or \
            next((n for n in funcs if orig_addr(n) == addr), None)
    else:
        name = args.function if args.function in funcs else None
    if name is None:
        sys.exit(f"fdiff.py: no function {args.function} in {ELF}")

    built_addr, size = funcs[name]
    start = orig_addr(name)
    end = original_end({orig_addr(n) for n in funcs}, start, size)
    mine = disassemble(read_at(BUILT, built_addr, size), built_addr, "build.bin")
    orig = strip_padding(disassemble(read_at(ORIGINAL, start, end - start), start, "original.bin"), len(mine))

    orig_names = {a: n for n, a in named.items()}
    built_names = {a: n for n, (a, s, t) in syms.items() if t == "OBJECT" and not n.endswith(".NON_MATCHING")}
    ok, data = compare(orig, mine, ORIGINAL_GP, gp_value(readelf) or ORIGINAL_GP,
                       lambda a, n: (orig_names.get(a), read_at(ORIGINAL, a, n)),
                       lambda a, n: (built_names.get(a), read_at(BUILT, a, n)))
    if ok:
        print("OK" if built_addr == start else f"OK (linked at {built_addr:#x}, original {start:#x})")
        return
    if data:
        print(f"{name}: same instructions, different data through $gp")
        for i, a, b, (an, ab), (bn, bb) in data:
            print(f"  {orig[i][0]:08x}  {orig[i][2].expandtabs(8):<28} original {a:#x} {an or ''} {ab.hex()}, "
                  f"build {b:#x} {bn or ''} {bb.hex()}")
        print(f"{len(data)} data references differ")
        sys.exit(1)
    by_orig = {orig_addr(n): n for n in funcs}
    by_built = {a: n for n, (a, _) in funcs.items()}
    rows = align(orig, mine,
                 lambda r: normalize(r[2], start, end - start, by_orig),
                 lambda r: normalize(r[2], built_addr, size, by_built))
    marks = [m for m, _, _ in rows]
    real = sum(m in "!<>" for m in marks)
    print(f"{name}: original {start:#x} ({len(orig)} instructions), build {built_addr:#x} ({len(mine)})")
    for mark, left, right in rows:
        lt = f"{left[0]:08x}  {left[2]}" if left else ""
        rt = f"{right[0]:08x}  {right[2]}" if right else ""
        print(f"{mark} {lt.expandtabs(8):<44} {rt.expandtabs(8)}")
    print(f"{real} lines differ, {marks.count('~')} relocated call/branch targets differ")
    sys.exit(1)


if __name__ == "__main__":
    main()
