"""Paths and compiler settings of the PC build, shared by gen_data.py, undefined.py, link.py and build.py.

Everything the build writes goes under build/port/ (ignored by git: it holds data generated from the user's own
executable). The first target is 32-bit x86 Linux with the host gcc: the game keeps addresses in 4-byte fields.
TTT_CC=clang compiles the game code with clang instead (same flags), into build/port/obj_clang."""
import os
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
VARIANT = os.environ.get("TTT_CC", "gcc")
BUILD = ROOT / "build/port"
GEN = BUILD / "gen"                       # generated sources: game C with the drafts switched on, data, stubs
OBJ = BUILD / ("obj" if VARIANT == "gcc" else "obj_" + VARIANT)
DATA_OBJ = BUILD / "obj_data"             # the game's data, assembled from GEN/data
EXE = BUILD / ("ttt" if VARIANT == "gcc" else "ttt_" + VARIANT)
YAML = ROOT / "config/SLUS_200.01.yaml"
ASM = ROOT / "asm"                        # the matching build's split (configure.py), made from the user's executable

# The game's C flags, as for the reference port: gnu89 like the original code, no strict aliasing (the game reads
# 16-bit vectors through 32-bit stores), no fused multiply-add, software float (the EE float model replaces the
# host's in T23), no SSE/MMX so no float work goes to vector registers, 8-byte alignment of 64-bit members as on the
# PS2. -fpermissive: the game's local prototypes disagree with definitions in places (an error since gcc 14).
GAME_CFLAGS = ["-m32", "-std=gnu89", "-O2", "-g", "-fno-strict-aliasing", "-ffp-contract=off", "-fcommon", "-w",
               "-fno-pic", "-fno-stack-protector", "-fno-builtin", "-msoft-float", "-mno-sse", "-mno-mmx",
               "-malign-double", "-fpermissive", "-Iinclude", "-Iport/include", "-include", "port_compat.h"]
# The port's own C (port/src) that the game calls: 32-bit, and software float like the game code, or float arguments
# and results would travel in other registers (x87 st0 against eax) between the two.
PORT_CFLAGS = ["-m32", "-std=gnu99", "-O2", "-g", "-fno-pic", "-fno-strict-aliasing", "-ffp-contract=off", "-w",
               "-fno-builtin", "-msoft-float", "-mno-sse", "-mno-mmx", "-malign-double", "-Iinclude", "-Iport/include",
               "-Iport/src"]


# clang has no -fpermissive for C: the same diagnostics are turned back into warnings one by one.
CLANG_CFLAGS = ["-Wno-error=int-conversion", "-Wno-error=incompatible-pointer-types", "-Wno-error=return-mismatch",
                "-Wno-error=implicit-function-declaration", "-Wno-error=incompatible-function-pointer-types",
                "-Wno-error=implicit-int"]


def cc():
    """The C compiler of the game code for this variant."""
    return "clang" if VARIANT == "clang" else "gcc"


def game_cflags():
    """GAME_CFLAGS for this variant."""
    return GAME_CFLAGS + CLANG_CFLAGS if VARIANT == "clang" else GAME_CFLAGS
