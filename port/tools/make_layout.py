#!/usr/bin/env python3
"""Write port/tools/data_layout.txt: where the game's data lies and which of its words the PC build rewrites, with
no byte of the data itself. A developer runs it with the matching build's split and the user's executable at hand;
the port is then built without either, and takes the data from the user's disc when it runs (port/src/plat_data.c,
docs/port/data.md).

    .venv/bin/python configure.py && python3 port/tools/gen_data.py && python3 port/tools/make_layout.py [--elf F]

How: gen_data.py assembles every split data file (data, rodata, lit4, sdata, the INCLUDE_RODATA tables and the data
blocks of functions still taken from assembly) into a section of its own at its PS2 address. Each assembled section
is compared with the user's SLUS_200.01 at that address. A word the host linker fills in (`.long name`) is listed
when its name is not one of the game's data addresses (`D_XXXXXXXX`): a function, or a variable the port's C may
define (then it holds a host address); a word gen_data.py writes as 0 (a jump-table label of library assembly) is
listed as `zero`. Every other byte must equal the executable's, or the tool stops. The file holds:

    elf <name> <size> <sha1>         the executable the layout was made from (the unmodified USA release)
    payload <bytes> <sha1>           the initialised data, the ranges' bytes in address order (what ttt.dat holds)
    range <addr> <size>              initialised data at its PS2 address (adjacent files merged)
    bss <addr> <size>                the zeroed data (.sbss, .bss): one span
    nobits <addr> <size>             each zeroed file (names inside them resolve to their address)
    word <addr> <name> <addend>      a word the linker fills in with name + addend
    zero <addr>                      a word that is 0 in the port
    label <name> <addr>              a data name of the split with no address in config/symbols or in its spelling

Addresses and sizes are hexadecimal."""
import argparse
import hashlib
import pathlib
import re
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from toolchain import DATA_OBJ, GEN, ROOT  # noqa: E402
from undefined import symbol_addresses  # noqa: E402

LAYOUT = ROOT / "port/tools/data_layout.txt"
DATA_NAME = re.compile(r"(?:D|func|jtbl)_([0-9A-F]{1,8})$")


def elf_sections(blob):
    """{name: (type, addr, offset, size, link, entsize)} of an ELF32 little-endian file, and its section list."""
    shoff, = struct.unpack_from("<I", blob, 32)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", blob, 46)
    raw = [struct.unpack_from("<10I", blob, shoff + i * shentsize) for i in range(shnum)]
    stroff = raw[shstrndx][4]
    out = []
    for s in raw:
        name = blob[stroff + s[0]:blob.index(b"\0", stroff + s[0])].decode()
        out.append((name, s[1], s[3], s[4], s[5], s[6], s[9]))  # name type addr offset size link entsize
    return out


def object_section(path):
    """(bytes, [(offset, symbol name or None for a section symbol, addend)], {global name: offset}) of the one
    `.ttt.*` section of an assembled data file."""
    blob = path.read_bytes()
    secs = elf_sections(blob)
    idx = next(i for i, s in enumerate(secs) if s[0].startswith(".ttt."))
    _, kind, _, off, size, _, _ = secs[idx]
    data = bytes(size) if kind == 8 else blob[off:off + size]
    sym = next(s for s in secs if s[1] == 2)
    strtab = secs[sym[5]]
    syms = []
    for j in range(sym[4] // 16):
        name_off, value, _, info, _, shndx = struct.unpack_from("<IIIBBH", blob, sym[3] + j * 16)
        name = blob[strtab[3] + name_off:blob.index(b"\0", strtab[3] + name_off)].decode()
        syms.append((name, value, info, shndx))
    defined = {n: v for n, v, info, sh in syms if sh == idx and info >> 4 == 1 and n}
    relocs = []
    for s in secs:
        if s[1] == 9 and s[0] == ".rel" + secs[idx][0]:
            for k in range(s[4] // 8):
                r_off, r_info = struct.unpack_from("<II", blob, s[3] + k * 8)
                if r_info & 0xFF != 1:
                    raise ValueError(f"{path.name}: relocation type {r_info & 0xFF} at 0x{r_off:X}")
                name, _, info, _ = syms[r_info >> 8]
                relocs.append((r_off, name if info & 15 != 3 and name else None, struct.unpack_from("<i", data, r_off)[0]))
    return data, relocs, defined


def merge(ranges):
    """Adjacent (addr, size) pairs joined."""
    out = []
    for a, n in sorted(ranges):
        if out and out[-1][0] + out[-1][1] == a:
            out[-1] = (out[-1][0], out[-1][1] + n)
        else:
            out.append((a, n))
    return out


def ps2_image(elf):
    """addr -> bytes of the executable's loadable segments: a function reading `size` bytes at `addr`."""
    phoff, = struct.unpack_from("<I", elf, 28)
    phnum, = struct.unpack_from("<H", elf, 44)
    segs = []
    for i in range(phnum):
        kind, off, va, _, fsz = struct.unpack_from("<5I", elf, phoff + i * 32)
        if kind == 1 and fsz:
            segs.append((va, off, fsz))

    def read(addr, size):
        """The bytes at addr; past the end of a segment's file part (an assembler's tail padding), zeros."""
        for va, off, fsz in segs:
            if va <= addr < va + fsz:
                got = elf[off + addr - va:off + min(addr + size, va + fsz) - va]
                return got + bytes(size - len(got))
        raise ValueError(f"0x{addr:08X}+0x{size:X} is outside the executable's loadable data")

    def stored(addr, size):
        """How many of the `size` bytes at addr the executable's file holds."""
        for va, off, fsz in segs:
            if va <= addr < va + fsz:
                return min(size, va + fsz - addr)
        return 0
    read.stored = stored
    return read


def build(image_txt, elf, known):
    """Lines of the layout from gen_data.py's image.txt and objects, checked against the executable `elf`."""
    read = ps2_image(elf)
    entries = []
    for l in image_txt.read_text().splitlines():
        name, addr, size, kind = l.split()
        entries.append((name, int(addr, 16), int(size, 16), kind == "nobits"))
    words, zeros, labels, progbits, nobits = [], [], {}, [], []
    for name, addr, size, isbss in sorted(entries, key=lambda e: e[1]):
        obj = DATA_OBJ / (name.rsplit("_", 1)[0] + "." + name.rsplit("_", 1)[1] + ".o")
        data, relocs, defined = object_section(obj)
        if len(data) != size:
            raise ValueError(f"{obj.name}: {len(data)} bytes, image.txt says 0x{size:X}")
        for n, off in defined.items():
            if not n.startswith("__chunk_") and known.get(n) != addr + off and \
                    not (DATA_NAME.match(n) and int(DATA_NAME.match(n).group(1), 16) == addr + off):
                labels[n] = addr + off
        if isbss:
            nobits.append((addr, size))
            continue
        progbits.append((addr, read.stored(addr, size)))
        orig = read(addr, size)
        filled = set()
        for off, sym, addend in relocs:
            filled.update(range(off, off + 4))
            at = addr + off
            if sym is None:  # a label of this same file: a PS2 address, the executable's own word
                continue
            m = DATA_NAME.match(sym)
            if m and sym.startswith("D_"):
                want = (int(m.group(1), 16) + addend) & 0xFFFFFFFF
                if struct.unpack_from("<I", orig, off)[0] != want:
                    raise ValueError(f"0x{at:08X}: .word {sym} + {addend} is not the executable's word")
                continue
            words.append((at, sym, addend))
        for off in range(0, size, 4):
            if data[off:off + 4] != orig[off:off + 4] and off not in filled:
                if data[off:off + 4] != bytes(4) or off % 4:
                    raise ValueError(f"0x{addr + off:08X}: the assembled data differs from the executable")
                zeros.append(addr + off)
        rest = [i for i in range(size - size % 4, size) if data[i] != orig[i]]
        if rest:
            raise ValueError(f"0x{addr + rest[0]:08X}: the assembled data differs from the executable")
    ranges = merge(progbits)
    payload = b"".join(read(a, n) for a, n in ranges)
    lo = min(a for a, _ in nobits)
    hi = max(a + n for a, n in nobits)
    out = [f"elf SLUS_200.01 {len(elf)} {hashlib.sha1(elf).hexdigest()}",
           f"payload {len(payload):X} {hashlib.sha1(payload).hexdigest()}"]
    out += [f"range {a:08X} {n:X}" for a, n in ranges]
    out.append(f"bss {lo:08X} {hi - lo:X}")
    out += [f"nobits {a:08X} {n:X}" for a, n in merge(nobits)]
    out += [f"word {a:08X} {s} {d}" for a, s, d in sorted(words)]
    out += [f"zero {a:08X}" for a in sorted(zeros)]
    out += [f"label {n} {a:08X}" for n, a in sorted(labels.items())]
    return out


HEADER = """\
# The game's data layout for the PC build (port/tools/make_layout.py; docs/port/data.md). No byte of the game's
# data is in this file: addresses, sizes, names and hashes only. The port reads the bytes from the user's disc.
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--elf", default=str(ROOT / "iso/SLUS_200.01"), help="your SLUS_200.01 (default iso/SLUS_200.01)")
    ap.add_argument("--out", default=str(LAYOUT))
    a = ap.parse_args()
    image = GEN / "image.txt"
    if not image.exists():
        sys.exit("make_layout: run port/tools/gen_data.py first (it needs the split: .venv/bin/python configure.py)")
    lines = build(image, pathlib.Path(a.elf).read_bytes(), symbol_addresses())
    pathlib.Path(a.out).write_text(HEADER + "\n".join(lines) + "\n")
    kinds = {}
    for l in lines:
        kinds[l.split()[0]] = kinds.get(l.split()[0], 0) + 1
    print("make_layout: " + ", ".join(f"{n} {k}" for k, n in kinds.items()) + f" -> {a.out}")


if __name__ == "__main__":
    main()
