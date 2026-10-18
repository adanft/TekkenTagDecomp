#!/usr/bin/env python3
"""Splits the executable with splat and writes build.ninja, which rebuilds it and checks its SHA-1.

Usage: .venv/bin/python configure.py [--split] && ninja

Needs iso/SLUS_200.01 (scripts/setup_disc.py). splat runs when its output is missing or older than the config;
--split runs it again anyway (needed after adding an INCLUDE_ASM to a C file). ninja assembles every split file,
compiles every C file the config links, links them with the generated linker script into build/SLUS_200.01.elf,
flattens that with objcopy into build/SLUS_200.01 and compares its SHA-1 with the original.
"""

import argparse
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
YAML = "config/SLUS_200.01.yaml"
ORIGINAL = "iso/SLUS_200.01"
ELF_SHA1 = "c7ad9a022bdc836827a0cc093f91b4281836f2bb"
LD_SCRIPT = "build/SLUS_200.01.ld"
LD_SCRIPTS = [LD_SCRIPT, "build/undefined_funcs_auto.txt", "build/undefined_syms_auto.txt"]
BINUTILS = "tools/binutils/mips-ps2-decompals-"

AS_FLAGS = "-EL -march=r5900 -mabi=eabi -G0 -no-pad-sections -Iinclude"

# The game code was built with ee-gcc 2.9-ee-991111-01 (docs/decomp_guide.md, "Compiler"). Its driver only
# compiles to assembly here (-S); the project's gas assembles that behind include/gcc_prelude.inc, which encodes a
# few instructions the way the game's assembler did.
CC = "tools/compilers/ee-gcc2.9-991111-01/bin/ee-gcc"
CC_FLAGS = "-O2 -fno-strength-reduce -ffast-math -Iinclude"
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
    """The asm files a C file pulls in with INCLUDE_ASM("folder", name), in order: folder/name.s."""
    return [f"{folder}/{name}.s" for folder, name in
            re.findall(r'^\s*INCLUDE_ASM\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)', c_text, re.M)]


def g_flag(src):
    """The -G flag a C file is compiled and assembled with."""
    return next((g for prefix, g in G_FLAGS.items() if src.startswith(prefix)), G_DEFAULT)


def set_eabi64(elf):
    """A 32-bit little-endian ELF object (bytes) with its e_flags set to EABI64_FLAGS."""
    if elf[:6] != b"\x7fELF\x01\x01":
        raise ValueError("not a 32-bit little-endian ELF file")
    return elf[:E_FLAGS_OFFSET] + EABI64_FLAGS.to_bytes(4, "little") + elf[E_FLAGS_OFFSET + 4:]


def split(force):
    ld = ROOT / LD_SCRIPT
    if not force and ld.exists() and ld.stat().st_mtime >= (ROOT / YAML).stat().st_mtime:
        return
    run([sys.executable, "-m", "splat", "split", YAML])
    ld.write_text(align_bss(ld.read_text()))


def write_ninja():
    script = (ROOT / LD_SCRIPT).read_text()
    objs = objects(script)
    c_objs = [o for o in objs if source_of(o).endswith(".c")]
    asm_objs = [o for o in objs if o not in c_objs]
    includes = {o: included_asm((ROOT / source_of(o)).read_text()) for o in c_objs}
    headers = " ".join(str(p.relative_to(ROOT)) for p in sorted((ROOT / "include").rglob("*.h")))
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
        "rule cc",
        f"  command = {CC} {CC_FLAGS} $gflag -S $in -o $out.s && "
        f"$as {CC_AS_FLAGS} $gflag include/gcc_prelude.inc $out.s -o $out && "
        f"{sys.executable} configure.py --eabi64 $out",
        "  description = CC $in",
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
        f"build build.ninja: configure | configure.py {YAML}",
        "",
        "rule split",
        f"  command = {sys.executable} configure.py --split",
        "  description = SPLIT $in",
        "",
        # splat writes the linker script and the assembly (also the files INCLUDE_ASM pulls into C files), so a
        # missing one reruns it
        f"build {' '.join(LD_SCRIPTS)} {' '.join(source_of(o) for o in asm_objs)} "
        f"{' '.join(f for o in c_objs for f in includes[o])}: split {YAML}",
        "",
    ]
    for obj in asm_objs:
        out.append(f"build {obj}: as {source_of(obj)} | include/macro.inc")
    for obj in c_objs:
        src = source_of(obj)
        out.append(f"build {obj}: cc {src} | {headers} include/gcc_prelude.inc include/macro.inc "
                   f"{' '.join(includes[obj])}")
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
    args = parser.parse_args()
    if args.eabi64:
        path = Path(args.eabi64)
        path.write_bytes(set_eabi64(path.read_bytes()))
        return
    if not (ROOT / ORIGINAL).exists():
        sys.exit(f"missing {ORIGINAL}: run scripts/setup_disc.py with your disc first (docs/setup.md)")
    split(args.split)
    write_ninja()


if __name__ == "__main__":
    main()
