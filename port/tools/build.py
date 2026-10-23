#!/usr/bin/env python3
"""Build the PC port from a clean tree: the game's data (gen_data.py), every object (undefined.py), the link with
stubs (link.py). Needs the matching build's split (`.venv/bin/python configure.py`), which reads your own
SLUS_200.01; everything is written under build/port/.

Usage: python3 port/tools/build.py [-v]      -> build/port/ttt (32-bit x86 Linux)
       TTT_CC=clang python3 port/tools/build.py  (game code through clang -> build/port/ttt_clang)"""
import pathlib
import subprocess
import sys

TOOLS = pathlib.Path(__file__).resolve().parent


def step(name, *args):
    r = subprocess.run([sys.executable, str(TOOLS / name), *args])
    return r.returncode == 0


def main():
    ok = step("gen_data.py")
    if not ok:
        sys.exit("build: gen_data.py failed")
    compiled = step("undefined.py", *sys.argv[1:])  # failures are listed; link what compiled, to see what is left
    linked = step("link.py")
    if not (compiled and linked):
        sys.exit("build: FAILED (" + ", ".join(n for n, s in (("compile", compiled), ("link", linked)) if not s) + ")")
    print("build: OK")


if __name__ == "__main__":
    main()
