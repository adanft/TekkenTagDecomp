#!/usr/bin/env python3
"""Builds the matchmaking server and the manifest signer (T46, docs/port/matchmaking.md): port/server/ttt-server.c and
port/server/ttt-sign.c, with the host's C compiler (CC, else cc), into build/server/. Native programs for the machine
that runs them, separate from the game (port/tools/build.py) and from its toolchain.

    python3 port/tools/build_server.py [--out DIR]
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


def build(out):
    cc = shlex.split(os.environ.get("CC", "")) or [shutil.which("cc") or shutil.which("gcc") or "cc"]
    out.mkdir(parents=True, exist_ok=True)
    for name in PROGRAMS:
        cmd = cc + FLAGS + ["-o", str(out / name), str(SRC / f"{name}.c")]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write(r.stdout + r.stderr)
            print(f"build_server: {name} FAILED")
            return 1
    print(f"build_server: OK -> {', '.join(str((out / n).relative_to(ROOT) if (out / n).is_relative_to(ROOT) else out / n) for n in PROGRAMS)}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", type=Path, default=ROOT / "build/server")
    return build(ap.parse_args().out.resolve())


if __name__ == "__main__":
    sys.exit(main())
