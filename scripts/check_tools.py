#!/usr/bin/env python3
"""Checks that every tool the project needs runs from the project directory.

Usage: python3 scripts/check_tools.py

Prints one line per tool and exits with 1 if a required tool is missing or broken.
Tools marked "later" are reported but never fail the check. Setup steps: docs/setup.md.
"""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VENV_PY = ROOT / ".venv" / "bin" / "python"
BINUTILS = ROOT / "tools" / "binutils" / "mips-ps2-decompals-"
PERMUTER = ROOT / "tools" / "decomp-permuter"
PERMUTER_COMMIT = "059609d4aec73eb0650726772954e1ad575825f8"
SPLAT_VERSION = "0.50.0"

# R5900-only instructions: they assemble only with an assembler that knows the PS2's CPU.
R5900_TEST = ".set noreorder\n.text\nf:\n    pmaddw $a0, $a1, $a2\n    jr $ra\n    mult1 $a0, $a1\n"


def run(cmd, **kw):
    """(ok, first line of output) of a command, run from the project root."""
    try:
        r = subprocess.run([str(c) for c in cmd], cwd=ROOT, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, errors="replace", timeout=60, **kw)
    except (OSError, subprocess.TimeoutExpired) as e:
        return False, str(e)
    lines = r.stdout.strip().splitlines()
    return r.returncode == 0, (lines[0] if lines else "")


def venv_module(module, version_of=None):
    """Imports a module with the project's Python; reports the installed version of a distribution."""
    if not VENV_PY.exists():
        return False, "no .venv"
    code = f"import {module}"
    if version_of:
        code += f"; import importlib.metadata as m; print(m.version({version_of!r}))"
    ok, out = run([VENV_PY, "-c", code])
    return ok, (out or "ok") if ok else out


def splat():
    ok, out = venv_module("splat", "splat64")
    if ok and out != SPLAT_VERSION:
        return False, f"{out}, expected {SPLAT_VERSION}"
    if ok:
        ok, help_line = run([VENV_PY, "-m", "splat", "--help"])
        if not ok:
            return False, help_line
    return ok, out


def m2c():
    if not VENV_PY.exists():
        return False, "no .venv"
    ok, out = run([VENV_PY, "-m", "m2c.main", "--help"])
    return ok, "ok" if ok else out


def permuter():
    script = PERMUTER / "permuter.py"
    if not script.exists():
        return False, f"missing {script.relative_to(ROOT)}"
    if not VENV_PY.exists():
        return False, "no .venv"
    ok, out = run([VENV_PY, script, "--help"])
    if not ok:
        return False, out
    _, head = run(["git", "-C", PERMUTER, "rev-parse", "HEAD"])
    if head != PERMUTER_COMMIT:
        return False, f"at {head[:12]}, expected {PERMUTER_COMMIT[:12]}"
    return True, PERMUTER_COMMIT[:12]


def binutil(name):
    exe = Path(str(BINUTILS) + name)
    if not exe.exists():
        return False, f"missing {exe.relative_to(ROOT)}"
    return run([exe, "--version"])


def as_r5900():
    exe = Path(str(BINUTILS) + "as")
    if not exe.exists():
        return False, "no assembler"
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / "t.s"
        src.write_text(R5900_TEST)
        ok, out = run([exe, "-EL", "-march=r5900", "-mabi=o64", src, "-o", Path(tmp) / "t.o"])
    return ok, "assembles R5900 code" if ok else out


def later(*names):
    """A tool installed in a later task: reports where it was found, never fails."""
    for name in names:
        path = shutil.which(name)
        if path:
            return True, path
    return None, "not installed yet"


REQUIRED = [
    ("python venv", lambda: run([VENV_PY, "--version"]) if VENV_PY.exists() else (False, "no .venv")),
    ("splat64", splat),
    ("m2c", m2c),
    ("decomp-permuter deps", lambda: venv_module("nacl, toml, Levenshtein")),
    ("decomp-permuter", permuter),
    *[(f"binutils {n}", lambda n=n: binutil(n)) for n in ("as", "ld", "objcopy", "objdump", "readelf", "nm", "ar")],
    ("as -march=r5900", as_r5900),
]

LATER = [
    ("ee-gcc (T05)", lambda: (True, "tools/compilers/") if (ROOT / "tools" / "compilers").is_dir()
     else (None, "not installed yet")),
    ("objdiff", lambda: later("objdiff", "objdiff-cli")),
    ("Ghidra", lambda: later("ghidra", "ghidraRun")),
    ("PCSX2", lambda: later("pcsx2", "pcsx2-qt", "PCSX2")),
]


def main():
    if os.name != "posix":
        sys.exit("check_tools.py expects Linux (see docs/setup.md)")
    failed = []
    width = max(len(name) for name, _ in REQUIRED + LATER)
    print(f"{'tool':<{width}}  {'status':<7}  detail")
    for name, check in REQUIRED:
        ok, detail = check()
        print(f"{name:<{width}}  {'OK' if ok else 'MISSING':<7}  {detail}")
        if not ok:
            failed.append(name)
    for name, check in LATER:
        found, detail = check()
        print(f"{name:<{width}}  {'found' if found else 'later':<7}  {detail}")
    if failed:
        print(f"\n{len(failed)} required tool(s) missing: {', '.join(failed)}. See docs/setup.md.")
        return 1
    print("\nAll required tools run from the project.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
