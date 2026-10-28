#!/usr/bin/env python3
"""Compile the game sources and the port's own code to 32-bit host objects, and list what a link still needs.

Every game C file the matching build links is compiled with the drafts switched on (portsrc.py), so every game
function comes from C. The symbols that no object defines are listed by kind in build/port/undefined.txt: game
functions (none should be left), library functions (Sony SDK, libc, libgcc: the platform layer replaces them),
data (placed at its PS2 address by link.py; plat_data.c loads it from the user's disc) and names with no PS2 address (host library, compiler helpers).

Usage: port/tools/undefined.py [-v]      (objects go to build/port/obj; -v lists every name)"""
import collections
import concurrent.futures
import os
import pathlib
import re
import shutil
import subprocess
import sys

from toolchain import (BITS64, BUILD, GEN, HOT_CFLAGS, HOT_FILES, IMGUI, IMGUI_OBJCOPY, IMGUI_SOURCES, MPEG2, NEWLIB,
                       NOPLT_FILES, OBJ, ROOT, SIMD_CFLAGS, SIMD_FILES, VU_FILES, cc, clang_noplt_renames, cxx,
                       game_cflags, newlib_cflags, noplt_redefine_args, overlay_cxxflags, port_cflags, target, tool,
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


def ptr32_python():
    """An interpreter with libclang's Python bindings (clang.cindex) for ptr32.py: this one, or the system's python3
    (a virtual environment usually has no bindings; the distribution's clang package installs them system-wide)."""
    try:
        import clang.cindex  # noqa: F401
        return sys.executable
    except ImportError:
        return shutil.which("python3") or sys.executable


def is_game_source(src):
    """Whether src is one of the game's C files (src/, or its prepared copy under the build's gen/src)."""
    return src.parent in (ROOT / "src", GEN / "src") or GEN / "src" in src.parents


def compile_game64(compiler, src, obj, flags):
    """A game file in the 64-bit variant: preprocess (with the forced include) -> ptr32.py (every pointer of the game's
    text 4 bytes wide) -> clang -emit-llvm -> irfix.py -> llc (static relocation model: the program is not PIE and lies
    below 4 GB). The intermediate files stay next to the object (<name>.i, <name>.p32.i, <name>.ll)."""
    pre, p32, ll = obj.with_suffix(".i"), obj.with_suffix(".p32.i"), obj.with_suffix(".ll")
    nocomp = [a for a in flags if a != "-c"]
    r = subprocess.run([compiler, "-E"] + nocomp + [str(src), "-o", str(pre)], cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        return r
    std = next((a for a in flags if a.startswith("-std=")), "-std=gnu89")
    r = subprocess.run([ptr32_python(), str(pathlib.Path(__file__).with_name("ptr32.py")), str(pre), str(p32),
                        "-x", "cpp-output", *target(), "-m64", std, "-fms-extensions", "-w"] +
                       [a for a in flags if a.startswith("-Wno-error=")],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        return r
    c2, skip = [], False  # the forced include is in the preprocessed text already
    for a in flags:
        if skip:
            skip = False
        elif a == "-include":
            skip = True
        else:
            c2.append(a)
    r = subprocess.run([compiler, "-S", "-emit-llvm"] + c2 + ["-x", "cpp-output", str(p32), "-o", str(ll)],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode:
        return r
    import irfix
    text, _ = irfix.fix(ll.read_text())
    ll.write_text(text)
    return subprocess.run(["llc", "-O2", "-filetype=obj", "-relocation-model=static", str(ll), "-o", str(obj)],
                          cwd=ROOT, capture_output=True, text=True)


def compile_one(job):
    """Compiles one job (compiler, source, object, flags[, objcopy arguments]); an object built with -fno-plt has its
    C library calls renamed to the GOT thunks where the compiler cannot do it (toolchain.noplt_redefine_args)."""
    compiler, src, obj, flags = job[:4]
    redefine = list(job[4]) if len(job) > 4 else []
    if BITS64 and is_game_source(src):
        r = compile_game64(compiler, src, obj, flags)
    else:
        r = subprocess.run([compiler, "-c"] + flags + [str(src), "-o", str(obj)],
                           cwd=ROOT, capture_output=True, text=True)
    if r.returncode == 0 and src.parent == ROOT / "port/src" and src.name in VU_FILES:
        redefine = vu_redefine_args()
    if r.returncode == 0 and "-fno-plt" in flags and clang_noplt_renames():
        und = subprocess.run([tool("nm"), "-u", str(obj)], capture_output=True, text=True, check=True).stdout.split()
        redefine += noplt_redefine_args([n for n in und if n != "U"],
                                        keep=[a.split("=")[1] for a in redefine if a.startswith("--redefine-sym=")])
    if r.returncode == 0 and redefine:
        r = subprocess.run([tool("objcopy")] + redefine + [str(obj)], cwd=ROOT, capture_output=True, text=True)
    return src, obj, r.returncode, r.stderr


def git_id():
    """The source's commit (short), with -dirty for local changes; "unknown" outside git."""
    r = subprocess.run(["git", "describe", "--always", "--dirty"], cwd=ROOT, capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 and r.stdout.strip() else "unknown"


def jobs():
    """(compiler, source, object, flags) of everything to compile, and {source name: names without C}."""
    out, missing = [], {}
    for f in portsrc.sources():
        src, _, miss = portsrc.prepare(f)
        if miss:
            missing[f.name] = miss
        out.append((cc(), src, OBJ / f"{f.stem}.o", game_cflags() + [f"-I{ROOT / 'src'}"]))
    for f in sorted((ROOT / "port/src").glob("*.c")):
        out.append((cc(), f, OBJ / f"pc_{f.stem}.o", port_cflags() + (HOT_CFLAGS if f.name in HOT_FILES else [])
                    + (SIMD_CFLAGS if f.name in SIMD_FILES else []) + (["-fno-plt"] if f.name in NOPLT_FILES else [])
                    + ([f'-DTTT_GIT="{git_id()}"'] if f.name == "plat_crash.c" else [])))  # the crash report's build id
    for f in sorted((ROOT / "port/src").glob("*.cpp")):  # the settings overlay (T36), with the build's id for its About
        out.append((cxx(), f, OBJ / f"pc_{f.stem}.o", overlay_cxxflags() + [f'-DTTT_GIT="{git_id()}"']))
    for n in IMGUI_SOURCES:  # Dear ImGui
        out.append((cxx(), IMGUI / n, OBJ / f"im_{pathlib.Path(n).stem}.o", overlay_cxxflags(),
                    IMGUI_OBJCOPY.get(n, [])))
    for f in sorted(NEWLIB.glob("*.c")):  # the PS2's maths library, on the EE float model
        out.append((cc(), f, OBJ / f"nl_{f.stem}.o", newlib_cflags()))
    for f in sorted(MPEG2.glob("*.c")):  # the MPEG-2 video decoder of the movies (integer only), -O3 like the hot paths
        out.append((cc(), f, OBJ / f"tp_{f.stem}.o", port_cflags() + HOT_CFLAGS))
    return out, missing


def main():
    OBJ.mkdir(parents=True, exist_ok=True)
    for old in [*OBJ.glob("*.o"), *OBJ.glob("*.i"), *OBJ.glob("*.ll")]:
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
    defined, undef = set(), collections.Counter()
    out = subprocess.run([tool("nm"), "-A"] + objs, capture_output=True, text=True, check=True).stdout
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
    (BUILD / "objects.txt").write_text("\n".join(objs) + "\n")
    if bad or missing:
        sys.exit(1)


if __name__ == "__main__":
    main()
