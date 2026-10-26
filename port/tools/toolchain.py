"""Paths and compiler settings of the PC build, shared by gen_data.py, undefined.py, link.py and build.py.

Everything the build writes goes under build/port/ (ignored by git: it holds data generated from the user's own
executable). The first target is 32-bit x86 Linux with the host gcc: the game keeps addresses in 4-byte fields.
TTT_CC=clang compiles the game code with clang instead (same flags) into its own tree, build/port-clang/ (program
build/port-clang/ttt), so the default build/port/ttt is never replaced. In that variant every C file of the program
is clang's (the game, port/src, newlib's libm, the MPEG-2 decoder, the generated tables and stubs), the C++ overlay
clang++'s, and clang drives the link (docs/port/README.md gotcha 32)."""
import os
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
VARIANT = os.environ.get("TTT_CC", "gcc")
BUILD = ROOT / ("build/port" if VARIANT == "gcc" else "build/port-" + VARIANT)
GEN = BUILD / "gen"                       # generated sources: game C with the drafts switched on, data, stubs
OBJ = BUILD / "obj"
DATA_OBJ = BUILD / "obj_data"             # the game's data, assembled from GEN/data
EXE = BUILD / "ttt"
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

# The settings overlay (T36, docs/port/settings.md): Dear ImGui (third_party/imgui, MIT, v1.92.9) and
# port/src/overlay.cpp, C++ that runs on the window thread only and computes in host float (SSE, and x87 where the
# 32-bit ABI returns a float); nothing of the simulation reads it. No C++ run time: no exceptions, no RTTI, no guarded
# statics, so nothing of libstdc++ is linked. -fno-plt: the C library calls go through the GOT, so the program's PLT,
# and the addresses of the game's functions after it, do not change (port_host.h). Maths through imconfig_ttt.h (the
# host libm is not linked; the game's sinf / sqrtf are the PS2's, in software float). tests/test_port_float.py lets host
# float instructions into the functions these objects define, and nowhere else.
IMGUI = ROOT / "third_party/imgui"
IMGUI_SOURCES = ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp", "imgui_demo.cpp",
                 "backends/imgui_impl_opengl3.cpp")
OVERLAY_CXXFLAGS = ["-m32", "-std=c++17", "-O2", "-g", "-fno-pic", "-fno-plt", "-fno-exceptions", "-fno-rtti",
                    "-fno-threadsafe-statics", "-fno-stack-protector", "-fno-strict-aliasing", "-msse2", "-mfpmath=sse",
                    "-fno-math-errno", "-w", '-DIMGUI_USER_CONFIG="imconfig_ttt.h"', "-Iport/src", f"-I{IMGUI}"]
# ImGui's libGL loader registers a handler with atexit (a new PLT entry through libc_nonshared); the port loads GL
# through SDL instead, so after compiling, that call is sent to a no-op in overlay.cpp (objcopy arguments).
IMGUI_OBJCOPY = {"backends/imgui_impl_opengl3.cpp": ["--redefine-sym=atexit=ttt_imgl3w_atexit"]}
# Port C files whose new C library calls must not add PLT entries: all their external calls through the GOT.
NOPLT_FILES = ("plat_settings.c", "gs_texpack.c", "plat_crash.c")


def host_float_objects(obj=None):
    """The objects (in `obj`, default OBJ) whose functions may hold host float instructions: the overlay and Dear
    ImGui."""
    obj = OBJ if obj is None else obj
    return [obj / f"pc_{p.stem}.o" for p in sorted((ROOT / "port/src").glob("*.cpp"))] + \
           [obj / f"im_{pathlib.Path(n).stem}.o" for n in IMGUI_SOURCES]


# Port files that are VU0 macro code on the PS2: their float operations are the vector unit's, not the FPU's
# (port/src/softfloat_ps2.c). After compiling, their calls to the software-float helpers are renamed.
VU_FILES = ("plat_vu0.c", "vu0_pc.c")
VU_RENAMES = {"__addsf3": "Port_VuAdd", "__subsf3": "Port_VuSub", "__mulsf3": "Port_VuMul", "__divsf3": "Port_VuDiv",
              "sqrtf": "Port_VuSqrt"}


def vu_redefine_args():
    """objcopy arguments that send a VU file's float operations to the vector unit's arithmetic."""
    return [f"--redefine-sym={a}={b}" for a, b in sorted(VU_RENAMES.items())]


# clang has no -fpermissive for C: the same diagnostics are turned back into warnings one by one. -mno-x87: on x86
# clang's -msoft-float alone still computes on the x87 unit; without it every float operation is a call of the
# software-float helpers (the EE float model), as with gcc -msoft-float, and floats travel in integer registers.
CLANG_CFLAGS = ["-mno-x87", "-Wno-error=int-conversion", "-Wno-error=incompatible-pointer-types",
                "-Wno-error=return-mismatch", "-Wno-error=implicit-function-declaration",
                "-Wno-error=incompatible-function-pointer-types", "-Wno-error=implicit-int"]


def cc():
    """The C compiler of this variant: every C file of the program (game, port, newlib, MPEG-2, tables, stubs)."""
    return "clang" if VARIANT == "clang" else "gcc"


def cxx():
    """The C++ compiler of the settings overlay and Dear ImGui for this variant."""
    return "clang++" if VARIANT == "clang" else "g++"


def game_cflags():
    """GAME_CFLAGS for this variant."""
    return GAME_CFLAGS + CLANG_CFLAGS if VARIANT == "clang" else GAME_CFLAGS


def port_cflags():
    """PORT_CFLAGS for this variant (port/src and the MPEG-2 decoder)."""
    return PORT_CFLAGS + CLANG_CFLAGS if VARIANT == "clang" else PORT_CFLAGS


def newlib_cflags():
    """NEWLIB_CFLAGS for this variant."""
    return NEWLIB_CFLAGS + CLANG_CFLAGS if VARIANT == "clang" else NEWLIB_CFLAGS


# No PLT entry for a C library call (gcc: `noplt`, -fno-plt). On 32-bit x86 clang has neither: it does not know the
# `noplt` attribute, and its -fno-plt changes nothing there (LLVM honours it on x86-64 only). Its variant reaches the
# same PLT another way: a call goes to a local thunk `__ttt_got_<f>` (`jmp *<f>@GOT`, link.py: the GOT entry -z now
# fills at start, as for gcc's `call *<f>@GOT`), so the C library function gets a GOT entry and no PLT entry. Names
# declared with port_host.h's PORT_NOPLT get the thunk's name by an asm label; in the objects of NOPLT_FILES and of the
# overlay, every call of a C library function is renamed after compiling (noplt_redefine_args).
GOT_THUNK = "__ttt_got_"


def clang_noplt_renames():
    """True when -fno-plt objects need their C library calls renamed to GOT thunks (clang on 32-bit x86)."""
    return VARIANT == "clang"


_HOST_FUNCTIONS = None


def host_functions():
    """Names of the functions the host C library (libc.so.6: libpthread and libdl are part of it) exports."""
    global _HOST_FUNCTIONS
    if _HOST_FUNCTIONS is None:
        import subprocess
        lib = subprocess.run([cc(), "-m32", "-print-file-name=libc.so.6"], capture_output=True, text=True,
                             check=True).stdout.strip()
        out = subprocess.run(["nm", "-D", "--defined-only", lib], capture_output=True, text=True, check=True).stdout
        _HOST_FUNCTIONS = frozenset(f[2].split("@")[0] for f in map(str.split, out.splitlines())
                                    if len(f) == 3 and f[1] in "TWi")
    return _HOST_FUNCTIONS


def noplt_redefine_args(undefined, keep=()):
    """objcopy arguments that send an object's calls of C library functions (`undefined`: its undefined names) to
    their GOT thunks; names in `keep` (renamed otherwise already) stay."""
    host = host_functions()
    return [f"--redefine-sym={n}={GOT_THUNK}{n}" for n in sorted(set(undefined) - set(keep)) if n in host]


def got_thunk_source(names):
    """Assembly of the GOT thunks: one `jmp *<f>@GOT` per name, after every object (link.py), so a new one never moves
    the game's code. Non-PIC: the operand is the GOT entry's absolute address (R_386_GOT32 with no base register)."""
    lines = ["/* Generated by port/tools/link.py: C library calls through the GOT, no PLT entry (toolchain.py). */",
             "\t.text"]
    for n in sorted(names):
        t = GOT_THUNK + n
        lines += [f"\t.globl {t}", f"\t.hidden {t}", f"\t.type {t},@function", "\t.p2align 3", f"{t}:",
                  f"\tjmp *{n}@GOT", f"\t.size {t}, .-{t}"]
    return "\n".join(lines) + "\n"
