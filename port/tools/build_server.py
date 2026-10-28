#!/usr/bin/env python3
"""Builds the matchmaking server and the manifest signer (T46, docs/port/matchmaking.md): port/server/ttt-server.c and
port/server/ttt-sign.c, with the host's C compiler (CC, else cc), into build/server/. Native programs for the machine
that runs them, separate from the game (port/tools/build.py) and from its toolchain.

    python3 port/tools/build_server.py [--out DIR]
    python3 port/tools/build_server.py --win64 [--out DIR]   (Windows x86-64 programs, cross-built with mingw-w64's
                                                              gcc: ttt-server.exe, ttt-sign.exe in build/server-win64/)
"""

import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "port/server"
PROGRAMS = ("ttt-server", "ttt-sign")
FLAGS = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-D_POSIX_C_SOURCE=200809L", "-D_DEFAULT_SOURCE"]
# Windows (--win64): mingw-w64's gcc, Winsock and the system's random generator, everything linked in (-static: no
# DLL besides the system's); Windows 8 and later (WSAPoll, BCryptGenRandom).
MINGW_CC = "x86_64-w64-mingw32-gcc"
WIN64_FLAGS = ["-D_WIN32_WINNT=0x0602", "-static"]
WIN64_LIBS = ["-lws2_32", "-lbcrypt"]


def build_linux(cc, name, exe):
    """Compiles and links one Linux program. glibc 2.38's headers send strtol / strtoul to __isoc23_* names under
    _GNU_SOURCE (GLIBC_2.38 symbols): the object calls the plain ones instead, the same functions for the decimal
    numbers the programs read, so they start on older systems too (T44, docs/port/release.md "Portability")."""
    obj = exe.with_suffix(".o")
    r = subprocess.run(cc + FLAGS + ["-c", "-o", str(obj), str(SRC / f"{name}.c")], capture_output=True, text=True)
    if r.returncode == 0:
        und = subprocess.run(["nm", "-u", str(obj)], capture_output=True, text=True, check=True).stdout.split()
        names = sorted({n for n in und if n.startswith("__isoc23_")})
        if names:
            r = subprocess.run(["objcopy"] + [f"--redefine-sym={n}={n[len('__isoc23_'):]}" for n in names] + [str(obj)],
                               capture_output=True, text=True)
    if r.returncode == 0:
        r = subprocess.run(cc + ["-o", str(exe), str(obj)], capture_output=True, text=True)
    obj.unlink(missing_ok=True)
    return r


def build(out, win64=False):
    cc = shlex.split(os.environ.get("CC", "")) or [shutil.which("cc") or shutil.which("gcc") or "cc"]
    cc = [MINGW_CC] if win64 else cc
    out.mkdir(parents=True, exist_ok=True)
    for name in PROGRAMS:
        exe = name + (".exe" if win64 else "")
        if win64:
            cmd = cc + FLAGS + WIN64_FLAGS + ["-o", str(out / exe), str(SRC / f"{name}.c")] + WIN64_LIBS
            r = subprocess.run(cmd, capture_output=True, text=True)
        else:
            r = build_linux(cc, name, out / exe)
        if r.returncode != 0:
            sys.stderr.write(r.stdout + r.stderr)
            print(f"build_server: {name} FAILED")
            return 1
    names = [n + (".exe" if win64 else "") for n in PROGRAMS]
    shown = [str((out / n).relative_to(ROOT) if (out / n).is_relative_to(ROOT) else out / n) for n in names]
    print(f"build_server: OK -> {', '.join(shown)}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--win64", action="store_true", help="Windows x86-64 programs (mingw-w64 cross compiler)")
    a = ap.parse_args()
    out = a.out or ROOT / ("build/server-win64" if a.win64 else "build/server")
    return build(out.resolve(), a.win64)


if __name__ == "__main__":
    sys.exit(main())
