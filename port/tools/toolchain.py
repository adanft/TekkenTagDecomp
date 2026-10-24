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
# and results would travel in other registers (x87 st0 against eax) between the two. No stack protector: a state
# (plat_state.c) keeps the port's frames on the EE stack, and a fresh process has another canary.
PORT_CFLAGS = ["-m32", "-std=gnu99", "-O2", "-g", "-fno-pic", "-fno-strict-aliasing", "-ffp-contract=off", "-w",
               "-fno-stack-protector", "-fno-builtin", "-msoft-float", "-mno-sse", "-mno-mmx", "-malign-double", "-Iinclude", "-Iport/include",
               "-Iport/src"]

# The port's hot paths (the VU interpreter, the GS rasteriser) at -O3: integer code, the same results (T30).
HOT_FILES = ("vu_micro.c", "vu_fast.c", "gs_core.c")
HOT_CFLAGS = ["-O3"]
# The predecoded vector-unit interpreter and its test use SSE2 (vu_simd_inl.h: four lanes at once; the vector unit's
# add, multiply, divide and square root on the SSE unit in round-toward-zero / flush-to-zero mode, which computes the
# EE float model exactly). x86-64 CPUs all have SSE2. No float value crosses a function boundary (-msoft-float stays),
# and these are the only files with SSE float instructions (tests/test_port_float.py).
SIMD_FILES = ("vu_fast.c", "vu_fasttest.c")
SIMD_CFLAGS = ["-msse2"]

# newlib's float maths library (the PS2's own libm, third_party/newlib_libm): built like the port's code, in software
# float on the EE model, with its shim header in front.
NEWLIB = ROOT / "third_party/newlib_libm"
NEWLIB_CFLAGS = ["-m32", "-std=gnu89", "-O2", "-g", "-fno-pic", "-fno-strict-aliasing", "-ffp-contract=off", "-w",
                 "-fno-builtin", "-fno-stack-protector", "-Werror=implicit-function-declaration", "-msoft-float", "-mno-sse", "-mno-mmx", "-malign-double",
                 f"-I{NEWLIB}", "-include", "newlib_shim.h"]

# The MPEG-2 video decoder of the movies (third_party/pl_mpeg2, MIT, derived from PL_MPEG): integer only, built with
# the port's flags (T33).
MPEG2 = ROOT / "third_party/pl_mpeg2"

# Port files that are VU0 macro code on the PS2: their float operations are the vector unit's, not the FPU's
# (port/src/softfloat_ps2.c). After compiling, their calls to the software-float helpers are renamed.
VU_FILES = ("plat_vu0.c", "vu0_pc.c")
VU_RENAMES = {"__addsf3": "Port_VuAdd", "__subsf3": "Port_VuSub", "__mulsf3": "Port_VuMul", "__divsf3": "Port_VuDiv",
              "sqrtf": "Port_VuSqrt"}


def vu_redefine_args():
    """objcopy arguments that send a VU file's float operations to the vector unit's arithmetic."""
    return [f"--redefine-sym={a}={b}" for a, b in sorted(VU_RENAMES.items())]


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
