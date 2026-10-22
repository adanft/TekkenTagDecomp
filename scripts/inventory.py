#!/usr/bin/env python3
"""Lists the game functions that are not C yet and why (docs/t19_inventory.md).

Usage: .venv/bin/python scripts/inventory.py [--list]   (after ninja)

A function is not C when the last linked build (build/SLUS_200.01.elf) has its `<name>.NON_MATCHING` marker, as
for scripts/progress.py. Each one is then:

- asm-only, "no C file": its address lies in an assembly chunk of the config, no C file names it;
- asm-only, "not attempted (in a C file)": a C file includes it with INCLUDE_ASM and has no draft for it;
- draft: its INCLUDE_ASM sits in the `#else` of an `#if 0` block, the closest attempt. The block's NON_MATCHING note
  (inside the `#if 0` part, or the comment right above `#if 0`; a `NON_MATCHING:` note first) gives a cause by keywords (classify_note), a rough
  grouping only.

Modules come from the C file's name, or for assembly chunks from the `// range:` lines of
config/symbols/game_<module>.txt. Prints a Markdown summary; `--list` adds one line per function.
"""

import re
import sys
from collections import Counter, defaultdict

import fdiff
import progress

ROOT = fdiff.ROOT
INCLUDE_ASM = re.compile(r'^\s*INCLUDE_ASM\("[^"]*",\s*(\w+)\);')
RANGE = re.compile(r"^//\s*range:\s*(0x[0-9A-Fa-f]+)\s+(0x[0-9A-Fa-f]+)")

# (cause, keywords), tried in order on the lower-cased note; the first hit wins.
CAUSES = [
    ("hand-written assembly", ("hand-written", "handwritten")),
    ("m2c rough draft", ("m2c", "rough", "first draft", "raw draft", "not compared", "not tuned")),
    ("gas hazard", ("hazard",)),
    ("data layout / .lit4 pool", (".lit4", "lit4", "pool", ".sdata", "$gp offset", "sq / lq", "save mode",
                                  "rodata", "data layout")),
    ("store order", ("store order", "order of the stores", "position stores", "stores are scheduled",
                     "order of the", "stores ")),
    ("block layout / alignment", ("block", "cross-jump", "loop is entered", "aligns", "laid out", "placed")),
    ("register allocation / scheduling", ("register", "schedul", "swap", "spill", "hoist", "delay slot",
                                          "allocation", "stack slot", "lines", "lives in", "load", "fold",
                                          "constant", "instruction")),
]
OTHER = "other"


def classify_note(note):
    """The cause group of a NON_MATCHING note (CAUSES), or "other"."""
    low = note.lower()
    for cause, words in CAUSES:
        if any(w in low for w in words):
            return cause
    return OTHER


def _comment_note(lines, tag="NON_MATCHING"):
    """The text of the first comment in `lines` that contains `tag`, joined into one line, or None."""
    text = "\n".join(lines)
    for m in re.finditer(r"/\*(.*?)\*/", text, re.S):
        if tag in m.group(1):
            body = " ".join(l.strip().lstrip("*").strip() for l in m.group(1).splitlines())
            return " ".join(body.split())
    return None


def scan_source(text):
    """name -> ("asm", None) or ("draft", note) for every INCLUDE_ASM of a C file's text."""
    lines = text.splitlines()
    out = {}
    stack = []  # one entry per open #if: [is_if0, line of the #if, in_else, line of the #else]
    for i, line in enumerate(lines):
        s = line.strip()
        if s.startswith("#if"):
            stack.append([s == "#if 0", i, False, None])
        elif s.startswith("#else") and stack:
            stack[-1][2], stack[-1][3] = True, i
        elif s.startswith("#endif") and stack:
            stack.pop()
        m = INCLUDE_ASM.match(line)
        if not m:
            continue
        top = stack[-1] if stack else None
        if top and top[0] and top[2]:
            start = top[1]
            above = start
            while above > 0 and (lines[above - 1].strip().startswith(("/*", "*", "*/"))
                                 or lines[above - 1].rstrip().endswith("*/")):
                above -= 1
            inside, before = lines[start + 1:top[3]], lines[above:start]
            note = next((n for n in (_comment_note(inside, "NON_MATCHING:"), _comment_note(before, "NON_MATCHING:"),
                                     _comment_note(inside), _comment_note(before)) if n is not None), None)
            out[m.group(1)] = ("draft", note or "")
        else:
            out[m.group(1)] = ("asm", None)
    return out


def parse_module_ranges(stem, text):
    """[(start, end, module)] from the `// range:` lines of config/symbols/<stem>.txt (`game_` dropped)."""
    module = stem[5:] if stem.startswith("game_") else stem
    return [(int(m.group(1), 16), int(m.group(2), 16), module)
            for m in (RANGE.match(l) for l in text.splitlines()) if m]


def build_rows(not_c, sources, modules):
    """[(name, address, module, category, cause)] by address for the functions of `not_c` (name -> address);
    `sources` maps a C file's stem to scan_source() output, `modules` lists (start, end, module)."""
    where = {}
    for stem, funcs in sources.items():
        for name, kind in funcs.items():
            where[name] = (stem, kind)
    rows = []
    for name, addr in sorted(not_c.items(), key=lambda kv: kv[1]):
        if name in where:
            stem, (kind, note) = where[name]
            if kind == "draft":
                rows.append((name, addr, stem, "draft", classify_note(note)))
            else:
                rows.append((name, addr, stem, "asm-only", "not attempted (in a C file)"))
        else:
            mod = next((m for lo, hi, m in modules if lo <= addr < hi), "?")
            rows.append((name, addr, mod, "asm-only", "no C file"))
    return rows


def summarize(rows):
    """Counts per category, per draft cause and per module (module -> {category: n})."""
    module = defaultdict(Counter)
    for _, _, mod, cat, _ in rows:
        module[mod][cat] += 1
    return {
        "category": dict(Counter(r[3] for r in rows)),
        "cause": dict(Counter(r[4] for r in rows if r[3] == "draft")),
        "asm": dict(Counter(r[4] for r in rows if r[3] == "asm-only")),
        "module": {m: dict(c) for m, c in module.items()},
    }


def report(rows, listing=False):
    s = summarize(rows)
    out = [f"Not C: {len(rows)} functions; " + ", ".join(f"{k} {v}" for k, v in sorted(s['category'].items())), ""]
    out += ["| Asm-only kind | Functions |", "|---|---:|"]
    out += [f"| {k} | {v} |" for k, v in sorted(s["asm"].items(), key=lambda kv: -kv[1])]
    out += ["", "| Draft cause | Functions |", "|---|---:|"]
    out += [f"| {k} | {v} |" for k, v in sorted(s["cause"].items(), key=lambda kv: -kv[1])]
    out += ["", "| Module | Asm-only | Drafts |", "|---|---:|---:|"]
    for mod in sorted(s["module"], key=lambda m: min(r[1] for r in rows if r[2] == m)):
        c = s["module"][mod]
        out.append(f"| `{mod}` | {c.get('asm-only', 0)} | {c.get('draft', 0)} |")
    if listing:
        out += ["", "| Function | Address | Module | Category | Cause |", "|---|---|---|---|---|"]
        out += [f"| `{n}` | {a:#x} | {m} | {c} | {k} |" for n, a, m, c, k in rows]
    return "\n".join(out)


def main():
    if not (ROOT / fdiff.ELF).exists():
        sys.exit(f"inventory.py: no {fdiff.ELF}, run ninja first")
    library = progress.parse_ranges((ROOT / progress.RANGES).read_text())
    syms = fdiff.parse_symbols(fdiff.sh(ROOT / (fdiff.BINUTILS + "readelf"), "-sW", ROOT / fdiff.ELF))
    not_c = {n: a for n, (a, _) in fdiff.functions(syms).items()
             if n + progress.MARKER in syms and not progress.in_ranges(a, library)}
    sources = {p.stem: scan_source(p.read_text()) for p in sorted((ROOT / "src").glob("*.c"))}
    modules = []
    for p in sorted((ROOT / fdiff.SYMBOL_DIR).glob("game_*.txt")):
        modules += parse_module_ranges(p.stem, p.read_text())
    print(report(build_rows(not_c, sources, modules), "--list" in sys.argv[1:]))


if __name__ == "__main__":
    main()
