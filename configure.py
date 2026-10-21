#!/usr/bin/env python3
"""Splits the executable with splat and writes build.ninja, which rebuilds it and checks its SHA-1.

Usage: .venv/bin/python configure.py [--split] && ninja

Needs iso/SLUS_200.01 (scripts/setup_disc.py). splat runs when its output is missing, unfinished or older than the
config or a symbol file (config/symbols/*.txt, each listed in the config); --split runs it again anyway (needed
after adding an INCLUDE_ASM to a C file). A split starts from an empty asm/, so no file of an earlier layout stays
behind, and only a split that ran to the end writes build/split.done. ninja assembles every split file, compiles
every C file the config links (tracking the headers each one includes), links them with the generated linker
script into build/SLUS_200.01.elf, flattens that with objcopy into build/SLUS_200.01 and compares its SHA-1 with
the original.
"""

import argparse
import os
import re
import shlex
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
YAML = "config/SLUS_200.01.yaml"
SYMBOL_DIR = "config/symbols"
# What splat reads: a change to any of them reruns it.
SPLIT_INPUTS = [YAML] + sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / SYMBOL_DIR).glob("*.txt"))
ORIGINAL = "iso/SLUS_200.01"
ELF_SHA1 = "c7ad9a022bdc836827a0cc093f91b4281836f2bb"
LD_SCRIPT = "build/SLUS_200.01.ld"
LD_SCRIPTS = [LD_SCRIPT, "build/undefined_funcs_auto.txt", "build/undefined_syms_auto.txt"]
# Everything splat writes besides LD_SCRIPTS: emptied before each split.
ASM_DIR = "asm"
# Written last by a split that ran to the end; a split that fails or is killed leaves none, so it runs again.
SPLIT_DONE = "build/split.done"
BINUTILS = "tools/binutils/mips-ps2-decompals-"

AS_FLAGS = "-EL -march=r5900 -mabi=eabi -G0 -no-pad-sections -Iinclude"

# The game code was built with ee-gcc 2.9-ee-991111-01 (docs/decomp_guide.md, "Compiler"). Its driver only
# compiles to assembly here (-S); the project's gas assembles that behind include/gcc_prelude.inc, which encodes a
# few instructions the way the game's assembler did.
CC = "tools/compilers/ee-gcc2.9-991111-01/bin/ee-gcc"
# -fno-strict-aliasing: the compiler turns strict aliasing on at -O2, but the game reloads a 16-bit vector after
# every store to a 32-bit one (the GTE-style math at 0x39A2B8); the earlier C files compile the same either way.
CC_FLAGS = "-O2 -fno-strength-reduce -ffast-math -fno-strict-aliasing -Iinclude"
# C files (paths) whose object was built without -ffast-math: their float code keeps `x + 0.0f`, `-x + c` and
# divisions by constants, and sqrtf has its errno check (docs/decomp_guide.md, "Lessons from T17 batch 4").
NO_FAST_MATH = ("src/effects.c", "src/effects_draw.c", "src/loading_title.c", "src/scene3d.c", "src/model_anim.c", "src/model_anim_b.c", "src/menu_bg.c",
                "src/gallery_jpeg.c", "src/tekken_bowl.c")
# o64, not eabi: with eabi the modern gas expands address macros (la, addu with an immediate) to daddiu/daddu,
# while the game's assembler wrote addiu/addu. The code is the same either way, so the object's ABI tag is set
# to eabi64 afterwards (configure.py --eabi64), or the linker would refuse to mix it with the split files.
CC_AS_FLAGS = "-EL -march=r5900 -mabi=o64 -no-pad-sections -mno-pdr -Iinclude"
# Small-data threshold for the compiler and the assembler. Keys are path prefixes of C files; G_DEFAULT is the
# compiler's own default (8 bytes), seen in the game code reaching .sdata and .lit4 through $gp.
G_FLAGS = {}
G_DEFAULT = "-G8"

# ELF header field e_flags of a 32-bit object, and the value the split files and the original executable carry:
# noreorder, R5900, EABI64, MIPS III.
E_FLAGS_OFFSET = 0x24
EABI64_FLAGS = 0x20924001

# The VU microcode packets in .vudata (0x4F5F00-0x4FAF70) are assembled from listings (scripts/vuasm.py,
# docs/systems/vu1/README.md): the object splat cuts from that range is built from VU_ASM instead, which includes
# the assembled packets. Listings include the program listings of their folder, so each packet depends on all of them.
VU_SPLIT_ASM = "asm/data/vudata.data.s"
VU_ASM = "src/vu1/vudata.s"
VU_PACKETS = {"build/vu/vu0_full.bin": "src/vu0/vu0_full.vsm", "build/vu/vu0_small.bin": "src/vu0/vu0_small.vsm",
              "build/vu/vu1.bin": "src/vu1/vu1.vsm"}

# The original link aligned .sbss to 64 (0x5F6B00) after .sdata ends at 0x5F6AE4; splat's script only aligns to 16.
BSS_SECTION = ".main_bss (NOLOAD) :"
BSS_ALIGNED = ".main_bss ALIGN(64) (NOLOAD) :"


def run(cmd):
    print("$", " ".join(shlex.quote(str(c)) for c in cmd))
    subprocess.run([str(c) for c in cmd], check=True, cwd=ROOT)


def align_bss(script):
    """The linker script with the bss output section aligned like the original's .sbss."""
    if BSS_ALIGNED in script:
        return script
    if script.count(BSS_SECTION) != 1:
        raise ValueError(f"expected one '{BSS_SECTION}' in the linker script")
    return script.replace(BSS_SECTION, BSS_ALIGNED)


def objects(script):
    """Object files named by the linker script, in link order, without repeats."""
    return list(dict.fromkeys(re.findall(r"^\s+(build/\S+\.o)\(", script, re.M)))


def source_of(obj):
    """The file an object is built from: build/asm/x.o -> asm/x.s, build/src/x.o -> src/x.c."""
    rel = Path(obj).relative_to("build")
    return str(rel.with_suffix(".c" if rel.parts[0] == "src" else ".s"))


def included_asm(c_text):
    """The split files a C file pulls in with INCLUDE_ASM or INCLUDE_RODATA("folder", name), in order:
    folder/name.s."""
    return [f"{folder}/{name}.s" for folder, name in
            re.findall(r'^\s*INCLUDE_(?:ASM|RODATA)\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)', c_text, re.M)]


def g_flag(src):
    """The -G flag a C file is compiled and assembled with."""
    return next((g for prefix, g in G_FLAGS.items() if src.startswith(prefix)), G_DEFAULT)


def cc_flags(src):
    """The compiler flags of a C file (without -G): CC_FLAGS, less -ffast-math for the files in NO_FAST_MATH."""
    flags = CC_FLAGS.split()
    if src in NO_FAST_MATH:
        flags.remove("-ffast-math")
    return " ".join(flags)


# splat writes the VU0 accumulator operand of the function files as a bare `ACC` (the generic chunks have `$ACC`),
# which gas refuses: `vmulay.yz  ACC, $vf8, $vf4y`. The same holds for the Q register (`vdiv  Q, $vf0w, $vf13w`,
# `vmulq.xyz  $vf18, $vf13, Q`), which can stand anywhere in the operand list.
BARE_ACC = re.compile(r"^(.*\*/\s+v[a-z]+(?:\.[xyzw]+)?\s+)ACC\b", re.M)
BARE_Q = re.compile(r"^(.*\*/\s+v[a-z]+(?:\.[xyzw]+)?\s+(?:[^\n]*?[\s,])?)Q\b", re.M)


def fix_split_asm(text):
    """Split assembly with every bare `ACC` or `Q` operand of a VU0 macro instruction written `$ACC` / `$Q`."""
    text = BARE_ACC.sub(r"\1$ACC", text)
    while True:
        fixed = BARE_Q.sub(r"\1$Q", text)
        if fixed == text:
            return text
        text = fixed


# R5900 FPU compares: the condition field of the four the CPU has (c.f.s, c.eq.s, c.lt.s, c.le.s).
FPU_COMPARES = {"f": 0x30, "eq": 0x32, "lt": 0x34, "le": 0x36}
ASM_INSN = re.compile(r"^\t(\.?[a-z][\w.]*)(?:\t(.*))?$")


def _fpr(reg):
    m = re.fullmatch(r"\$f(\d+)", reg.strip())
    if not m or int(m.group(1)) > 31:
        raise ValueError(f"not an FPU register: {reg}")
    return int(m.group(1))


def fix_compiler_asm(text):
    """ee-gcc's assembly with three rewrites where the game's assembler and the modern gas differ (each one checked
    against the original bytes, docs/build.md "C files"):

    - `li.s $fN, x` whose value gas builds with `lui $at / mtc1` (low 16 bits zero) is written out with the prelude's
      `mtc1` (data), so gas puts no hazard nop between it and a following prelude data word (`mtc1 $0, $f24`);
      `li.s` of 0.0 is the prelude's `mtc1 $0, $fN`;
    - an FPU compare right before a bc1t / bc1f that gcc left to gas (reorder mode) becomes data, so gas adds no
      hazard nop between them (the original has none; the nop at a following `.set noreorder` stays);
    - a `sqrt.s` ending a noreorder block (`nop / nop / sqrt.s`) followed by `jal sym` goes into that jal's delay
      slot, where the game's assembler put it (the prelude's data word could not move there)."""
    lines = text.split("\n")
    out = []
    reorder = True
    i = 0
    while i < len(lines):
        line = lines[i]
        m = ASM_INSN.match(line)
        op, args = (m.group(1), m.group(2) or "") if m else (None, "")
        if op == ".set" and args.strip() in ("noreorder", "reorder"):
            reorder = args.strip() == "reorder"
        if reorder and op == "li.s":
            reg, value = (a.strip() for a in args.split(",", 1))
            bits = struct.unpack("<I", struct.pack("<f", float(value)))[0]
            if bits == 0:
                out.append(f"\tmtc1\t$0,{reg}")
                i += 1
                continue
            if bits & 0xFFFF == 0:
                out += ["\t.set\tnoat", f"\tlui\t$1,0x{bits >> 16:X}", f"\tmtc1\t$1,{reg}", "\t.set\tat"]
                i += 1
                continue
        cond = re.fullmatch(r"c\.(\w+)\.s", op or "")
        if reorder and cond:
            j = i + 1
            while j < len(lines) and (not lines[j].strip() or lines[j].lstrip().startswith("#")):
                j += 1
            nxt = ASM_INSN.match(lines[j]) if j < len(lines) else None
            if nxt and nxt.group(1) in ("bc1t", "bc1f"):
                if cond.group(1) not in FPU_COMPARES:
                    raise ValueError(f"no R5900 encoding for {line.strip()}")
                fs, ft = (_fpr(a) for a in args.split(","))
                word = 0x46000000 | FPU_COMPARES[cond.group(1)] | (ft << 16) | (fs << 11)
                out.append(f"\t.word\t0x{word:08X}\t# {op} {args.strip()}")
                i += 1
                continue
        if not reorder and op == "sqrt.s" and i + 2 < len(lines) and lines[i + 1] == "\t.set\treorder":
            j = i + 2
            while j < len(lines) and (not lines[j].strip() or lines[j].lstrip().startswith("#")):
                j += 1
            nxt = ASM_INSN.match(lines[j]) if j < len(lines) else None
            if nxt and nxt.group(1) == "jal" and not (nxt.group(2) or "").strip().startswith("$"):
                out += [lines[j], line, lines[i + 1]] + lines[i + 2:j]
                reorder = True
                i = j + 1
                continue
        out.append(line)
        i += 1
    return "\n".join(out)


def set_eabi64(elf):
    """A 32-bit little-endian ELF object (bytes) with its e_flags set to EABI64_FLAGS."""
    if elf[:6] != b"\x7fELF\x01\x01":
        raise ValueError("not a 32-bit little-endian ELF file")
    return elf[:E_FLAGS_OFFSET] + EABI64_FLAGS.to_bytes(4, "little") + elf[E_FLAGS_OFFSET + 4:]


def asm_source(src):
    """The file an asm object is assembled from: the split file, or VU_ASM for the microcode packets."""
    return VU_ASM if src == VU_SPLIT_ASM else src


def vu_listings(packet):
    """Listing files a packet's assembly reads: every .vsm in the packet listing's folder."""
    return sorted(str(p.relative_to(ROOT)) for p in (ROOT / packet).parent.glob("*.vsm"))


def yaml_symbol_paths(text):
    """The paths of the config's `symbol_addrs_path` option: one path, or a block list of `- path` lines."""
    lines = text.splitlines()
    for i, line in enumerate(lines):
        m = re.match(r"(\s*)symbol_addrs_path:\s*([^#\s]*)", line)
        if not m:
            continue
        if m.group(2):
            return [m.group(2)]
        paths = []
        for item in lines[i + 1:]:
            m = re.match(r"\s*-\s*([^#\s]+)", item)
            if not m:
                break
            paths.append(m.group(1))
        return paths
    return []


def check_symbol_paths(listed, present):
    """Raises ValueError unless the config lists exactly the symbol files present: splat silently skips a missing
    file and never reads one that is not listed."""
    missing, unlisted = sorted(set(listed) - set(present)), sorted(set(present) - set(listed))
    if missing or unlisted:
        raise ValueError(f"{YAML} symbol_addrs_path: listed but missing: {', '.join(missing) or '-'}; "
                         f"present but not listed: {', '.join(unlisted) or '-'}")


def split_is_current():
    """Whether the last split ran to the end after the last change to any splat input."""
    done = ROOT / SPLIT_DONE
    return (done.exists() and all((ROOT / f).exists() for f in LD_SCRIPTS)
            and done.stat().st_mtime >= max((ROOT / f).stat().st_mtime for f in SPLIT_INPUTS))


def split(force):
    """Runs splat from a clean asm/ unless the split is current (and not forced). Nothing else under build/ is
    touched, so build/ keeps what other tools put there."""
    if not force and split_is_current():
        return
    (ROOT / SPLIT_DONE).unlink(missing_ok=True)
    for f in LD_SCRIPTS:
        (ROOT / f).unlink(missing_ok=True)
    shutil.rmtree(ROOT / ASM_DIR, ignore_errors=True)
    try:
        run([sys.executable, "-m", "splat", "split", YAML])
        for path in (ROOT / ASM_DIR).rglob("*.s"):
            text = path.read_text()
            if "ACC" in text and fix_split_asm(text) != text:
                path.write_text(fix_split_asm(text))
        ld = ROOT / LD_SCRIPT
        tmp = ld.with_name(ld.name + ".tmp")
        tmp.write_text(align_bss(ld.read_text()))
        os.replace(tmp, ld)
    except BaseException:
        # An unfinished linker script must not pass for a split: ninja and the next run see it missing.
        (ROOT / LD_SCRIPT).unlink(missing_ok=True)
        raise
    (ROOT / SPLIT_DONE).parent.mkdir(parents=True, exist_ok=True)
    (ROOT / SPLIT_DONE).write_text("")


def write_ninja():
    script = (ROOT / LD_SCRIPT).read_text()
    objs = objects(script)
    c_objs = [o for o in objs if source_of(o).endswith(".c")]
    asm_objs = [o for o in objs if o not in c_objs]
    includes = {o: included_asm((ROOT / source_of(o)).read_text()) for o in c_objs}
    out = [
        f"as = {BINUTILS}as",
        f"ld = {BINUTILS}ld",
        f"objcopy = {BINUTILS}objcopy",
        f"as_flags = {AS_FLAGS}",
        "",
        "rule as",
        "  command = $as $as_flags $in -o $out",
        "  description = AS $in",
        "",
        # -Wp,-MD: the preprocessor lists every header the file includes in $out.d (ee-gcc 2.9 has no -MF), which
        # ninja keeps in .ninja_deps, so a header change rebuilds exactly the objects that include it.
        "rule cc",
        f"  command = {CC} $cflags $gflag -Wp,-MD,$out.d -S $in -o $out.s && "
        f"{sys.executable} configure.py --fix-asm $out.s && "
        f"$as {CC_AS_FLAGS} $gflag include/gcc_prelude.inc $out.s -o $out && "
        f"{sys.executable} configure.py --eabi64 $out",
        "  description = CC $in",
        "  depfile = $out.d",
        "  deps = gcc",
        "",
        "rule ld",
        "  command = $ld -EL $scripts -Map $out.map -o $out",
        "  description = LD $out",
        "",
        "rule flatten",
        "  command = $objcopy -O binary --gap-fill=0x00 $in $out",
        "  description = OBJCOPY $out",
        "",
        "rule sha1",
        f"  command = echo '{ELF_SHA1}  $in' | sha1sum --check --quiet && touch $out",
        "  description = SHA1 $in",
        "",
        "rule configure",
        f"  command = {sys.executable} configure.py",
        "  description = CONFIGURE",
        "  generator = 1",
        "",
        f"build build.ninja: configure | configure.py {' '.join(SPLIT_INPUTS)}",
        "",
        "rule split",
        f"  command = {sys.executable} configure.py --split",
        "  description = SPLIT $in",
        "",
        # splat writes the linker script and the assembly (also the files INCLUDE_ASM pulls into C files), so a
        # missing one reruns it
        f"build {SPLIT_DONE} {' '.join(LD_SCRIPTS)} {' '.join(source_of(o) for o in asm_objs)} "
        f"{' '.join(f for o in c_objs for f in includes[o])}: split {' '.join(SPLIT_INPUTS)}",
        "",
        "rule vuasm",
        f"  command = {sys.executable} scripts/vuasm.py -o $out $in",
        "  description = VUASM $out",
        "",
    ]
    for binary, listing in VU_PACKETS.items():
        out.append(f"build {binary}: vuasm {listing} | scripts/vuasm.py {' '.join(vu_listings(listing))}")
    for obj in asm_objs:
        src = asm_source(source_of(obj))
        extra = f" {' '.join(VU_PACKETS)}" if src == VU_ASM else ""
        out.append(f"build {obj}: as {src} | include/macro.inc{extra}")
    for obj in c_objs:
        src = source_of(obj)
        # configure.py: the rewrite of the compiler's output (--fix-asm) and the ABI tag (--eabi64) run from it
        out.append(f"build {obj}: cc {src} | include/gcc_prelude.inc include/macro.inc configure.py "
                   f"{' '.join(includes[obj])}")
        out.append(f"  cflags = {cc_flags(src)}")
        out.append(f"  gflag = {g_flag(src)}")
    out += [
        "",
        f"build build/SLUS_200.01.elf: ld | {' '.join(objs)} {' '.join(LD_SCRIPTS)}",
        f"  scripts = {' '.join('-T ' + s for s in LD_SCRIPTS)}",
        "build build/SLUS_200.01: flatten build/SLUS_200.01.elf",
        "build build/SLUS_200.01.ok: sha1 build/SLUS_200.01",
        "default build/SLUS_200.01.ok",
        "",
    ]
    (ROOT / "build.ninja").write_text("\n".join(out))


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--split", action="store_true", help="run splat even if its output is up to date")
    parser.add_argument("--eabi64", metavar="OBJ", help="set the ABI tag of a compiled object to EABI64 and exit")
    parser.add_argument("--fix-asm", metavar="S", help="rewrite the compiler's assembly in place (fix_compiler_asm) "
                        "and exit")
    args = parser.parse_args()
    if args.fix_asm:
        path = Path(args.fix_asm)
        path.write_text(fix_compiler_asm(path.read_text()))
        return
    if args.eabi64:
        path = Path(args.eabi64)
        path.write_bytes(set_eabi64(path.read_bytes()))
        return
    if not (ROOT / ORIGINAL).exists():
        sys.exit(f"missing {ORIGINAL}: run scripts/setup_disc.py with your disc first (docs/setup.md)")
    try:
        check_symbol_paths(yaml_symbol_paths((ROOT / YAML).read_text()), SPLIT_INPUTS[1:])
    except ValueError as e:
        sys.exit(f"configure.py: {e}")
    split(args.split)
    write_ninja()


if __name__ == "__main__":
    main()
