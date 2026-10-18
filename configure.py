#!/usr/bin/env python3
"""Splits the executable with splat and writes build.ninja, which rebuilds it and checks its SHA-1.

Usage: .venv/bin/python configure.py [--split] && ninja

Needs iso/SLUS_200.01 (scripts/setup_disc.py). splat runs when its output is missing or older than the config;
--split runs it again anyway. ninja assembles every split file, links them with the generated linker script into
build/SLUS_200.01.elf, flattens that with objcopy into build/SLUS_200.01 and compares its SHA-1 with the original.
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
    """The assembly file an object is built from: build/asm/x.o -> asm/x.s."""
    return str(Path(obj).relative_to("build").with_suffix(".s"))


def split(force):
    ld = ROOT / LD_SCRIPT
    if not force and ld.exists() and ld.stat().st_mtime >= (ROOT / YAML).stat().st_mtime:
        return
    run([sys.executable, "-m", "splat", "split", YAML])
    ld.write_text(align_bss(ld.read_text()))


def write_ninja():
    script = (ROOT / LD_SCRIPT).read_text()
    objs = objects(script)
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
        # splat writes the linker script and the sources, so a missing one reruns it
        f"build {' '.join(LD_SCRIPTS)} {' '.join(source_of(o) for o in objs)}: split {YAML}",
        "",
    ]
    for obj in objs:
        out.append(f"build {obj}: as {source_of(obj)} | include/macro.inc")
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
    args = parser.parse_args()
    if not (ROOT / ORIGINAL).exists():
        sys.exit(f"missing {ORIGINAL}: run scripts/setup_disc.py with your disc first (docs/setup.md)")
    split(args.split)
    write_ninja()


if __name__ == "__main__":
    main()
