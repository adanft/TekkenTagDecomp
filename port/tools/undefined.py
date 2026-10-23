#!/usr/bin/env python3
"""Compile the game sources and the port's own code to 32-bit host objects, and list what a link still needs.

Every game C file the matching build links is compiled with the drafts switched on (portsrc.py), so every game
function comes from C. The symbols that no object defines are listed by kind in build/port/undefined.txt: game
functions (none should be left), library functions (Sony SDK, libc, libgcc: the platform layer replaces them),
data (gen_data.py provides the split data) and names with no PS2 address (host library, compiler helpers).

Usage: port/tools/undefined.py [-v]      (objects go to build/port/obj; -v lists every name)"""
import collections
import concurrent.futures
import os
import re
import subprocess
import sys

from toolchain import (BUILD, DATA_OBJ, HOT_CFLAGS, HOT_FILES, NEWLIB, NEWLIB_CFLAGS, OBJ, PORT_CFLAGS, ROOT, SIMD_CFLAGS,
                       SIMD_FILES, VU_FILES, cc, game_cflags,
                       vu_redefine_args)
import portsrc

TEXT_START, TEXT_END = 0x340000, 0x415700  # .text of SLUS_200.01; .data and the rest follow


def library_ranges():
    """[(start, end, label)] of the library code inside .text (config/library_ranges.txt)."""
    out = []
    for l in (ROOT / "config/library_ranges.txt").read_text().splitlines():
        m = re.match(r"0x([0-9A-Fa-f]+) 0x([0-9A-Fa-f]+) (\S+)", l)
        if m:
            out.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
    return out


LIBRARY = library_ranges()


def symbol_addresses():
    """name -> PS2 address, from config/symbols/*.txt."""
    out = {}
    for p in (ROOT / "config/symbols").glob("*.txt"):
        for l in p.read_text().splitlines():
            m = re.match(r"(\w+)\s*=\s*0x([0-9A-Fa-f]+)", l)
            if m:
                out[m.group(1)] = int(m.group(2), 16)
    return out


def classify(sym, addr):
    """Kind of an undefined symbol, from its PS2 address (symbol files, or a func_/D_/jtbl_ name)."""
    a = addr.get(sym)
    if a is None:
        m = re.match(r"(?:func|D|jtbl)_([0-9A-F]{8})$", sym)
        a = int(m.group(1), 16) if m else None
    if a is None:
        return "no address (host library or compiler helper)"
    if a >= TEXT_END or a < TEXT_START:
        return "data"
    if any(s <= a < e for s, e, _ in LIBRARY):
        return "library function (SDK / libc)"
    return "game function"


def compile_one(job):
    compiler, src, obj, flags = job
    r = subprocess.run([compiler, "-c"] + flags + [str(src), "-o", str(obj)],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode == 0 and src.parent == ROOT / "port/src" and src.name in VU_FILES:
        r = subprocess.run(["objcopy"] + vu_redefine_args() + [str(obj)], cwd=ROOT, capture_output=True, text=True)
    return src, obj, r.returncode, r.stderr


def jobs():
    """(compiler, source, object, flags) of everything to compile, and {source name: names without C}."""
    out, missing = [], {}
    for f in portsrc.sources():
        src, _, miss = portsrc.prepare(f)
        if miss:
            missing[f.name] = miss
        out.append((cc(), src, OBJ / f"{f.stem}.o", game_cflags() + [f"-I{ROOT / 'src'}"]))
    for f in sorted((ROOT / "port/src").glob("*.c")):
        out.append(("gcc", f, OBJ / f"pc_{f.stem}.o", PORT_CFLAGS + (HOT_CFLAGS if f.name in HOT_FILES else [])
                    + (SIMD_CFLAGS if f.name in SIMD_FILES else [])))
    for f in sorted(NEWLIB.glob("*.c")):  # the PS2's maths library, on the EE float model
        out.append(("gcc", f, OBJ / f"nl_{f.stem}.o", NEWLIB_CFLAGS))
    return out, missing


def main():
    OBJ.mkdir(parents=True, exist_ok=True)
    for old in OBJ.glob("*.o"):
        old.unlink()
    todo, missing = jobs()
    workers = int(os.environ.get("COOL_JOBS", os.cpu_count() or 4))
    with concurrent.futures.ThreadPoolExecutor(workers) as ex:
        res = list(ex.map(compile_one, todo))
    bad = [(s, e) for s, _, rc, e in res if rc]
    for s, e in bad:
        first = next((l for l in e.splitlines() if "error" in l), e.strip()[:200])
        print(f"FAILED {s.name}: {first[:240]}")
    for name, miss in missing.items():
        print(f"no C for: {name}: {' '.join(miss)}")
    objs = [str(o) for _, o, rc, _ in res if rc == 0]
    print(f"undefined: {len(objs)} of {len(res)} objects compiled ({len(bad)} failed)")
    data = sorted(str(o) for o in DATA_OBJ.glob("*.o"))
    defined, undef = set(), collections.Counter()
    out = subprocess.run(["nm", "-A"] + objs + data, capture_output=True, text=True, check=True).stdout
    for l in out.splitlines():
        m = re.match(r"\S+:\s*[0-9a-f]*\s+(\w)\s+(\S+)$", l)
        if not m:
            continue
        if m.group(1) == "U":
            undef[m.group(2)] += 1
        elif m.group(1).isupper():
            defined.add(m.group(2))
    addr = symbol_addresses()
    kinds = collections.defaultdict(list)
    for s in undef:
        if s not in defined:
            kinds[classify(s, addr)].append(s)
    print(f"undefined: {len(defined)} global symbols defined, {sum(map(len, kinds.values()))} still undefined")
    for k, v in sorted(kinds.items()):
        print(f"{len(v):6d}  {k}")
        if "-v" in sys.argv:
            print("        " + " ".join(sorted(v)))
    (BUILD / "undefined.txt").write_text("".join(f"{k}\t{s}\n" for k, v in sorted(kinds.items()) for s in sorted(v)))
    (BUILD / "objects.txt").write_text("\n".join(objs + data) + "\n")
    if bad or missing:
        sys.exit(1)


if __name__ == "__main__":
    main()
