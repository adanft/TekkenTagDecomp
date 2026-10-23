#!/usr/bin/env python3
"""Turn the game data the decompilation still takes from the split into host assembly for the PC build.

The matching build links the game's data (.data, .rodata, .lit4, .sdata, .sbss, .bss and the tables that C files
include with INCLUDE_RODATA) from files that splat generates out of the user's own SLUS_200.01 (asm/). Those keep
pointers symbolic (`.word SomeFunc`), which is what a PC build needs: the host linker fills in the PC addresses.
This tool rewrites them for the host assembler: `.word` -> `.long` (a "word" is 2 bytes on x86), `.align N` ->
`.p2align N`, a reference to a label inside an assembly function (a jump table of library code the PC build does
not use) -> 0, and every chunk starts at the same offset modulo 16 as on the PS2, so data that must be 16-byte
aligned stays aligned. Everything goes into writable sections (the game patches some of its read-only tables).

Usage: port/tools/gen_data.py          (needs the matching build's split: .venv/bin/python configure.py)
Output: build/port/gen/data/*.s and build/port/obj_data/*.o: generated from the user's game, never committed."""
import re
import subprocess
import sys

from toolchain import ASM, DATA_OBJ, GEN, ROOT
import portsrc

KINDS = ("data", "rodata", "lit4", "sdata", "sbss", "bss")
SECTION = {k: ".bss" if k in ("bss", "sbss") else ".data" for k in KINDS}
PLAIN = (".byte", ".short", ".float", ".asciz", ".ascii", ".space", ".double")


def chunk_address(text):
    """PS2 address of the first item of a split data file, from splat's comments (`/* offset vram bytes */` for
    data, `/* vram */` for bss) or, failing that, from a `D_XXXXXXXX` label."""
    for l in text.splitlines():
        m = re.match(r"\s*/\* ([0-9A-F]+)(?: ([0-9A-F]{8}))?(?: [0-9A-F]+)? \*/\s*\.", l)
        if m:
            return int(m.group(2) or m.group(1), 16)
    m = re.search(r"dlabel D_([0-9A-F]{8})", text)
    return int(m.group(1), 16) if m else 0


def convert(text, kind, addr=None, label=None):
    """Host assembly for one split data file of the given kind; `label` names its first byte (the PS2 address
    `addr`), so that link.py can place names that point inside it."""
    if addr is None:
        addr = chunk_address(text)
    out = [f".section {SECTION[kind]}", ".p2align 4"]
    if addr & 15:
        out.append(f".space {addr & 15}")
    if label:
        out += [f".globl {label}", f"{label}:"]
    for l in text.splitlines():
        s = re.sub(r"/\*.*?\*/", "", l).strip()
        if not s or s.startswith((".include", ".section", "nonmatching", "enddlabel")):
            continue
        m = re.match(r"dlabel (\S+)", s)
        if m:
            out += [f".globl {m.group(1)}", f"{m.group(1)}:"]
        elif s.startswith(".word"):
            arg = s[5:].strip()
            out.append("    .long 0" if arg.startswith(".L") else f"    .long {arg}")
        elif s.startswith(".align"):
            out.append(f"    .p2align {s[6:].strip()}")
        elif s.startswith(PLAIN):
            out.append("    " + s)
        else:
            raise ValueError(f"unhandled line: {s}")
    return "\n".join(out) + "\n"


def included_tables(c_text):
    """(folder, name) of every INCLUDE_RODATA table of a C file, jump tables excepted: those belong to functions the
    matching build takes from assembly, and the PC build compiles their C, which makes its own table."""
    return [(d, n) for d, n in re.findall(r'^INCLUDE_RODATA\("([^"]+)", (\w+)\);', c_text, re.M)
            if not n.startswith("jtbl_")]


def function_data(text):
    """[(kind, text)] of the data blocks in a split function file: splat moves the data only that function uses (its
    strings, float constants) into the function's file, and the draft C of the function refers to it by name. Jump
    tables are left out (the C makes its own), and so is the code."""
    out = []
    for block in re.split(r"^(?=\.section )", text, flags=re.M):
        m = re.match(r"\.section \.(\w+)", block)
        if not m or m.group(1) not in KINDS:
            continue
        kept = re.sub(r"^dlabel jtbl_\w+\n.*?^enddlabel jtbl_\w+\n", "", block, flags=re.M | re.S)
        if "dlabel " in kept:
            out.append((m.group(1), kept))
    return out


def main():
    data = ASM / "data"
    if not data.is_dir():
        sys.exit(f"missing {data.relative_to(ROOT)}: run `.venv/bin/python configure.py` first (it splits your SLUS_200.01)")
    dst = GEN / "data"
    dst.mkdir(parents=True, exist_ok=True)
    for old in dst.glob("*.s"):
        old.unlink()
    n, chunks = 0, {}
    for src in sorted(data.glob("*.s")):
        parts = src.name.split(".")
        if len(parts) != 3 or parts[1] not in KINDS:
            continue  # the ELF header and trailer: not part of the program
        text = src.read_text()
        label = "__chunk_" + src.name[:-2].replace(".", "_")
        (dst / src.name).write_text(convert(text, parts[1], label=label))
        chunks[label] = (chunk_address(text), DATA_OBJ / (src.name[:-2] + ".o"))
        n += 1
    m = 0
    for c in portsrc.sources():
        for folder, name in included_tables(c.read_text()):
            src = ROOT / folder / f"{name}.s"
            if not src.exists():
                sys.exit(f"missing {src.relative_to(ROOT)}: run `.venv/bin/python configure.py --split`")
            (dst / f"inc_{name}.rodata.s").write_text(convert(src.read_text(), "rodata"))
            m += 1
    k = 0
    for c in portsrc.sources():
        for folder, name in re.findall(r'^INCLUDE_ASM\("([^"]+)", (\w+)\);', c.read_text(), re.M):
            if name in portsrc.REPLACED:
                continue
            for i, (kind, block) in enumerate(function_data((ROOT / folder / f"{name}.s").read_text())):
                (dst / f"fn_{name}_{i}.{kind}.s").write_text(convert(block, kind))
                k += 1
    print(f"gen_data: {n} data chunks, {m} included tables and {k} function data blocks written to "
          f"{dst.relative_to(ROOT)}")
    DATA_OBJ.mkdir(parents=True, exist_ok=True)
    for old in DATA_OBJ.glob("*.o"):
        old.unlink()
    for f in sorted(dst.glob("*.s")):
        r = subprocess.run(["as", "--32", str(f), "-o", str(DATA_OBJ / (f.name[:-2] + ".o"))],
                           capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"gen_data: {f.name}: {r.stderr[:300]}")
    print(f"gen_data: {len(list(DATA_OBJ.glob('*.o')))} data objects assembled")
    # where each split chunk lies on the PS2 and how long it is here (the same: every item keeps its size)
    lines = []
    for label, (addr, obj) in sorted(chunks.items(), key=lambda kv: kv[1][0]):
        lines.append(f"{label} 0x{addr:08X} 0x{section_size(obj):X}\n")
    (GEN / "chunks.txt").write_text("".join(lines))


def section_size(obj):
    """Size of the one data section (.data or .bss) of an assembled chunk."""
    out = subprocess.run(["size", "-A", str(obj)], capture_output=True, text=True, check=True).stdout
    return sum(int(m.group(2)) for m in re.finditer(r"^(\.data|\.bss)\s+(\d+)", out, re.M))


if __name__ == "__main__":
    main()
