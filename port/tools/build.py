#!/usr/bin/env python3
"""Build the PC port from a clean tree: every object (undefined.py), the link with stubs (link.py). Needs neither the
game's executable nor its split: the game's data is not linked in, the program reads it from your disc when it runs
(port/tools/data_layout.txt, port/src/plat_data.c, docs/port/data.md). Everything is written under build/port/
(build/port-<TTT_CC>/ for the 32-bit variants).

Usage: python3 port/tools/build.py [-v]      -> build/port/ttt (x86-64 Linux through clang, docs/port/README.md 64-bit)
       TTT_CC=gcc python3 port/tools/build.py    (the 32-bit x86 program through gcc -> build/port-gcc/ttt)
       TTT_CC=clang python3 port/tools/build.py  (the 32-bit program through clang -> build/port-clang/ttt)"""
import pathlib
import subprocess
import sys

TOOLS = pathlib.Path(__file__).resolve().parent


def step(name, *args):
    r = subprocess.run([sys.executable, str(TOOLS / name), *args])
    return r.returncode == 0


def main():
    compiled = step("undefined.py", *sys.argv[1:])  # failures are listed; link what compiled, to see what is left
    linked = step("link.py")
    if not (compiled and linked):
        sys.exit("build: FAILED (" + ", ".join(n for n, s in (("compile", compiled), ("link", linked)) if not s) + ")")
    print("build: OK")


if __name__ == "__main__":
    main()
