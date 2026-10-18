#!/usr/bin/env python3
"""Call graph and cross references read from the split assembly (asm/), for mapping the game's modules.

Usage: .venv/bin/python scripts/callgraph.py COMMAND [ARGS]   (after configure.py has split the ELF)

  callers NAME          functions that call NAME (jal, or j as a tail call)
  callees NAME          functions NAME calls, with their library label (config/library_ranges.txt)
  reach NAME            everything reachable from NAME by direct calls, grouped by library label
  ptr NAME              functions and data that take NAME's address (function pointers, tables)
  xref SYMBOL           functions that reference a data symbol (%hi/%lo/%gp_rel)
  strings NAME          strings in the data symbols NAME references
  tables [MIN]          data symbols holding at least MIN (default 4) function pointers
  range START END       summary of [START, END): functions, calls into each library, called from outside, strings

NAME is a symbol as it appears in the assembly (func_XXXXXXXX or a name from config/symbol_addrs.txt).
Only direct references are seen: a call through a register (jalr) counts as one indirect call of its function.
"""

import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RANGES = ROOT / "config" / "library_ranges.txt"

_GLABEL = re.compile(r"^\s*glabel\s+(\S+)")
_NONMATCHING = re.compile(r"^\s*nonmatching\s+(\S+),\s*(0x[0-9A-Fa-f]+)")
_INSN = re.compile(r"^\s*/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+[0-9A-Fa-f]{8}\s*\*/\s+(\S+)\s*(.*)$")
_RELOC = re.compile(r"%(?:hi|lo|gp_rel)\(([A-Za-z_.$][\w.$]*)")
_DLABEL = re.compile(r"^\s*dlabel\s+(\S+)")
_DATA = re.compile(r"^\s*/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})(?:\s+[0-9A-Fa-f]+)?\s*\*/\s+\.(\w+)\s+(.*)$")
_ESC = re.compile(r"\\(x[0-9A-Fa-f]{2}|.)")
_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "\\": "\\", '"': '"', "'": "'"}


@dataclass
class Func:
    name: str
    addr: int
    size: int = 0
    calls: set = field(default_factory=set)
    refs: set = field(default_factory=set)
    fptrs: set = field(default_factory=set)
    indirect: int = 0


@dataclass
class Data:
    name: str
    addr: int
    strings: list = field(default_factory=list)
    words: list = field(default_factory=list)


def _unescape(s):
    return _ESC.sub(lambda m: chr(int(m.group(1)[1:], 16)) if m.group(1)[0] == "x" and len(m.group(1)) == 3
                    else _ESCAPES.get(m.group(1), m.group(1)), s)


def parse_text(lines):
    """{name: Func} from spimdisasm .text listings. Symbols named in %hi/%lo/%gp_rel are split into function
    pointers (a glabel of the listing, or func_XXXXXXXX) and data references."""
    funcs, sizes, cur, symrefs = {}, {}, None, {}
    for line in lines:
        m = _NONMATCHING.match(line)
        if m:
            sizes[m.group(1)] = int(m.group(2), 16)
            continue
        m = _GLABEL.match(line)
        if m:
            cur = None
            name = m.group(1)
            symrefs[name] = set()
            funcs[name] = cur = Func(name, -1, sizes.get(name, 0))
            continue
        if line.strip().startswith("endlabel"):
            cur = None
            continue
        m = _INSN.match(line)
        if not m or cur is None:
            continue
        addr, op, args = int(m.group(1), 16), m.group(2), m.group(3)
        if cur.addr < 0:
            cur.addr = addr
        if op in ("jal", "j") and not args.startswith(".") and not args.startswith("$"):
            cur.calls.add(args.split()[0])
        elif op == "jalr":
            cur.indirect += 1
        symrefs[cur.name].update(_RELOC.findall(args))
    for name, syms in symrefs.items():
        for s in syms:
            (funcs[name].fptrs if s in funcs or s.startswith("func_") else funcs[name].refs).add(s)
    return funcs


def parse_data(lines):
    """{name: Data} from spimdisasm data listings: strings with their addresses, and .word entries that name a
    symbol (pointers), in order."""
    data, cur = {}, None
    for line in lines:
        m = _DLABEL.match(line)
        if m:
            cur = data[m.group(1)] = Data(m.group(1), -1)
            continue
        if line.strip().startswith("enddlabel"):
            cur = None
            continue
        m = _DATA.match(line)
        if not m or cur is None:
            continue
        addr, directive, value = int(m.group(1), 16), m.group(2), m.group(3).strip()
        if cur.addr < 0:
            cur.addr = addr
        if directive in ("asciz", "ascii") and value.startswith('"'):
            cur.strings.append((addr, _unescape(value[1:value.rindex('"')])))
        elif directive == "word" and re.match(r"[A-Za-z_.$]", value):
            cur.words.append(value.split()[0])
    return data


def label_of(addr, ranges):
    for lo, hi, label in ranges:
        if lo <= addr < hi:
            return label
    return None


class Graph:
    def __init__(self, funcs, data):
        self.funcs, self.data = funcs, data
        self._callers, self._ptrs, self._xrefs = {}, {}, {}
        for f in funcs.values():
            for c in f.calls:
                self._callers.setdefault(c, set()).add(f.name)
            for p in f.fptrs:
                self._ptrs.setdefault(p, set()).add(f.name)
            for r in f.refs:
                self._xrefs.setdefault(r, set()).add(f.name)
        for d in data.values():
            for w in d.words:
                if w in funcs or w.startswith("func_"):
                    self._ptrs.setdefault(w, set()).add(d.name)

    def callers(self, name):
        return sorted(self._callers.get(name, ()))

    def pointer_users(self, name):
        return sorted(self._ptrs.get(name, ()))

    def xrefs(self, name):
        return sorted(self._xrefs.get(name, ()))

    def tables(self, min_funcs=4):
        out = []
        for d in self.data.values():
            n = sum(1 for w in d.words if w in self.funcs or w.startswith("func_"))
            if n >= min_funcs:
                out.append((d.name, d.addr, n))
        return sorted(out, key=lambda t: t[1])

    def strings(self, name):
        out = []
        for r in sorted(self.funcs[name].refs):
            if r in self.data:
                out.extend(self.data[r].strings)
        return out

    def reachable(self, name):
        seen, todo = set(), [name]
        while todo:
            n = todo.pop()
            if n in seen:
                continue
            seen.add(n)
            if n in self.funcs:
                todo.extend(self.funcs[n].calls)
        return seen

    def addr(self, name):
        f = self.funcs.get(name)
        if f is not None:
            return f.addr
        m = re.fullmatch(r"func_([0-9A-Fa-f]{8})", name)
        return int(m.group(1), 16) if m else -1


def load(root=ROOT):
    text, data = [], []
    for p in sorted((root / "asm").rglob("*.s")):
        lines = p.read_text(errors="replace").splitlines()
        (data if "data" in p.relative_to(root / "asm").parts[:1] else text).extend(lines)
    return Graph(parse_text(text), parse_data(data))


def load_ranges(path=RANGES):
    out = []
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].split()
        if len(line) >= 2:
            out.append((int(line[0], 16), int(line[1], 16), line[2] if len(line) > 2 else "library"))
    return out


def _lab(g, ranges, name):
    return label_of(g.addr(name), ranges) or "game"


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    g, ranges, cmd, args = load(), load_ranges(), argv[1], argv[2:]
    if cmd == "callers":
        for c in g.callers(args[0]):
            print(f"0x{g.addr(c):08X} {c}")
    elif cmd == "callees":
        for c in sorted(g.funcs[args[0]].calls, key=g.addr):
            print(f"0x{g.addr(c):08X} {c} [{_lab(g, ranges, c)}]")
        if g.funcs[args[0]].indirect:
            print(f"(+{g.funcs[args[0]].indirect} indirect)")
    elif cmd == "reach":
        groups = {}
        for c in g.reachable(args[0]):
            groups.setdefault(_lab(g, ranges, c), []).append(c)
        for lab, names in sorted(groups.items()):
            print(f"{lab} ({len(names)}): " + " ".join(sorted(names, key=g.addr)))
    elif cmd == "ptr":
        print("\n".join(g.pointer_users(args[0])))
    elif cmd == "xref":
        for c in g.xrefs(args[0]):
            print(f"0x{g.addr(c):08X} {c}")
    elif cmd == "strings":
        for a, s in g.strings(args[0]):
            print(f"0x{a:08X} {s!r}")
    elif cmd == "tables":
        for name, addr, n in g.tables(int(args[0]) if args else 4):
            print(f"0x{addr:08X} {name} {n}")
    elif cmd == "range":
        lo, hi = int(args[0], 16), int(args[1], 16)
        inside = [f for f in g.funcs.values() if lo <= f.addr < hi]
        names = {f.name for f in inside}
        libs, outside, strs = {}, set(), []
        for f in inside:
            for c in f.calls:
                lab = _lab(g, ranges, c)
                if lab != "game":
                    libs.setdefault(lab, set()).add(c)
            for c in g.callers(f.name) + g.pointer_users(f.name):
                if c not in names:
                    outside.add(c)
            strs.extend(g.strings(f.name))
        print(f"{len(inside)} functions, 0x{sum(f.size for f in inside):X} bytes")
        for lab, cs in sorted(libs.items()):
            print(f"  {lab}: " + " ".join(sorted(cs)))
        print(f"  used from outside by {len(outside)}: " + " ".join(sorted(outside, key=g.addr)[:20]))
        for a, s in sorted(set(strs))[:40]:
            print(f"  0x{a:08X} {s!r}")
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
