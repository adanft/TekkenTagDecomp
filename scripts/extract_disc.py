#!/usr/bin/env python3
"""Extracts every file of TEKKEN.BIN from your own disc files in iso/ (put there by setup_disc.py).

Usage: .venv/bin/python -I scripts/extract_disc.py [--elf FILE] [--bin FILE] [--out DIR] [--verify]

The game has no file names: it asks for files by id (0-747) through a table in its executable. This script reads
that table from iso/SLUS_200.01 (only its address, size and format are known here), copies each file out of
iso/TEKKEN.BIN and undoes the XOR scrambling of the first 0x800 bytes exactly as the game does after a load.
Nothing is compressed on the disc. The result is assets/files/NNN.bin (NNN = file id) plus manifest.tsv with id,
byte offset in TEKKEN.BIN, size, scrambled flag, first table sector and a kind guess. --verify reads every file
back, scrambles it again and compares it with TEKKEN.BIN. Format notes: docs/systems/files.md.
"""

import argparse
import struct
import sys
import time
from collections import Counter, namedtuple
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

TABLE_ADDR = 0x509E40  # D_00509E40: 16-byte entries indexed by file id
TABLE_COUNT = 748
SECTOR = 2048
SECTOR_BASE = 0x4820  # table sector of the first byte of TEKKEN.BIN (the IOP adds the file's own LSN)
SIZE_BIAS = 0x350A  # stored size = byte size + SIZE_BIAS
NO_EE_INDEX = 0xFFFF  # fourth word of files the EE never reads (sound, streams)
UNSCRAMBLED_ID = 0x2C  # read by the EE but skipped by the descrambler
SCRAMBLED_BYTES = 0x800
CHUNK = 1 << 20
GS_IMAGE_TAG = struct.pack("<QQ", 0x4000000000000001, 0xEEEE)
M64 = (1 << 64) - 1

Entry = namedtuple("Entry", "id sector size offset ee_index scrambled")


class Refused(Exception):
    """The input is not the expected executable or archive; the message says why."""


def key_seed(file_id):
    """File_KeySeed: first key of a file's XOR stream (64-bit arithmetic)."""
    return ((file_id + 0xF3C) * 218319 + (file_id - 0x348F) * 0x1234567) & M64


def key_next(key):
    """File_KeyNext: the next key (64-bit LCG)."""
    return (key * 0x71C3AB9 + 0x3941) & M64


def descramble(file_id, data):
    """XORs the first 0x800 bytes word by word with the low 32 bits of the key stream (an involution)."""
    head = bytearray(data[:SCRAMBLED_BYTES])
    size = len(head)
    head += bytes(-size % 4)
    key = key_seed(file_id)
    words = list(struct.unpack(f"<{len(head) // 4}I", head))
    for i in range(len(words)):
        words[i] ^= key & 0xFFFFFFFF
        key = key_next(key)
    return struct.pack(f"<{len(words)}I", *words)[:size] + bytes(data[SCRAMBLED_BYTES:])


def vaddr_to_offset(elf, addr, size):
    """File offset of `size` bytes at virtual address `addr`, from the ELF32 program headers."""
    if elf[:4] != b"\x7fELF" or len(elf) < 52:
        raise Refused("not an ELF executable")
    phoff, = struct.unpack_from("<I", elf, 28)
    phentsize, phnum = struct.unpack_from("<HH", elf, 42)
    if phentsize < 20 or phoff + phnum * phentsize > len(elf):
        raise Refused("damaged ELF program headers")
    for i in range(phnum):
        p_type, p_offset, p_vaddr, _, p_filesz = struct.unpack_from("<5I", elf, phoff + i * phentsize)
        if p_type == 1 and p_vaddr <= addr and addr + size <= p_vaddr + p_filesz:
            if p_offset + p_filesz > len(elf):
                break
            return p_offset + addr - p_vaddr
    raise Refused(f"the executable has no loaded data at 0x{addr:X}: not SLUS_200.01")


def read_table(elf, addr=TABLE_ADDR, count=TABLE_COUNT):
    """The file table: one Entry per id. Each third word is a check value that must match."""
    base = vaddr_to_offset(elf, addr, count * 16)
    table = []
    for i in range(count):
        sector, stored, check, ee_index = struct.unpack_from("<4I", elf, base + i * 16)
        size = (stored - SIZE_BIAS) & 0xFFFFFFFF
        size -= (size & 0x80000000) << 1  # signed: -1 marks a missing file
        if check != (i * 0x40000 + 0x11F3C + ((size + 0x7FF) >> 11)) & 0xFFFFFFFF:
            raise Refused(f"file table entry id {i} fails its check word: not the SLUS-20001 executable")
        if sector < SECTOR_BASE:
            raise Refused(f"file table entry id {i} starts before TEKKEN.BIN (sector 0x{sector:X})")
        scrambled = ee_index == i and i != UNSCRAMBLED_ID and size > 0
        table.append(Entry(i, sector, size, (sector - SECTOR_BASE) * SECTOR, ee_index, scrambled))
    return table


def _is_pack(head, size):
    if len(head) < 12:
        return False
    count, = struct.unpack_from("<I", head)
    end = 4 + 8 * count
    if not 1 <= count <= 4096 or end > min(len(head), size):
        return False
    for i in range(count):
        off, length = struct.unpack_from("<2I", head, 4 + 8 * i)
        if length and (off < end or off % 4 or off + length > size):
            return False
    return True


def _is_adpcm(head):
    blocks = [head[i:i + 2] for i in range(0, len(head) - 15, 16)]
    return len(blocks) >= 8 and all(b[0] >> 4 <= 4 and b[0] & 0xF <= 12 and b[1] <= 7 for b in blocks)


def _is_gs_image(head):
    # GIF tag NLOOP 1, NREG 4, REGS A+D x4, whose first register is BITBLTBUF (0x50): a GS image upload.
    return len(head) >= 32 and head[:16] == GS_IMAGE_TAG and head[24] == 0x50


def guess_kind(head, size):
    """A guess from the first 64 KB: empty, vag, gs-image, pack (count + offset/size pairs), mpeg-ps (an MPEG-2
    pack header, after interleaved sound), adpcm (raw SPU2 blocks) or unknown."""
    if size <= 0:
        return "empty"
    if head[:4] == b"VAGp":
        return "vag"
    if _is_gs_image(head):
        return "gs-image"
    if _is_pack(head, size):
        return "pack"
    if b"\x00\x00\x01\xba" in head:
        return "mpeg-ps"
    if _is_adpcm(head):
        return "adpcm"
    return "unknown"


def chunks(f, offset, size):
    f.seek(offset)
    while size > 0:
        piece = f.read(min(size, CHUNK))
        if not piece:
            raise Refused("TEKKEN.BIN ended early while reading")
        yield piece
        size -= len(piece)


def extract(elf_path, bin_path, out, verify=False):
    """Writes out/NNN.bin for every id and out/manifest.tsv. Returns (table, kind counter, bytes written)."""
    table = read_table(Path(elf_path).read_bytes())
    bin_size = Path(bin_path).stat().st_size
    for e in table:
        if e.size > 0 and e.offset + e.size > bin_size:
            raise Refused(f"file id {e.id} runs past the end of {Path(bin_path).name} "
                          f"(0x{e.offset + e.size:X} > 0x{bin_size:X}): wrong or truncated TEKKEN.BIN")
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    kinds, total, rows = Counter(), 0, ["id\toffset\tsize\tscrambled\tsector\tkind\tmagic"]
    with open(bin_path, "rb") as f:
        for e in table:
            head = b""
            with open(out / f"{e.id:03d}.bin", "wb") as dst:
                for n, piece in enumerate(chunks(f, e.offset, max(e.size, 0))):
                    if n == 0:
                        piece = descramble(e.id, piece) if e.scrambled else piece
                        head = piece[:0x10000]
                    dst.write(piece)
            kind = guess_kind(head, e.size)
            kinds[kind] += 1
            total += max(e.size, 0)
            rows.append(f"{e.id}\t0x{e.offset:X}\t{e.size}\t{int(e.scrambled)}\t0x{e.sector:X}\t{kind}\t{head[:4].hex()}")
        (out / "manifest.tsv").write_text("\n".join(rows) + "\n")
        if verify:
            for e in table:
                data = (out / f"{e.id:03d}.bin").read_bytes()
                again = descramble(e.id, data) if e.scrambled else data
                if again != b"".join(chunks(f, e.offset, max(e.size, 0))):
                    raise Refused(f"file id {e.id}: re-scrambled output differs from TEKKEN.BIN")
    return table, kinds, total


def main():
    p = argparse.ArgumentParser(description="Extract the files of TEKKEN.BIN by id, descrambled.")
    p.add_argument("--elf", type=Path, default=ROOT / "iso" / "SLUS_200.01", help="the game executable")
    p.add_argument("--bin", type=Path, default=ROOT / "iso" / "TEKKEN.BIN", help="the data archive")
    p.add_argument("--out", type=Path, default=ROOT / "assets" / "files", help="output directory")
    p.add_argument("--verify", action="store_true", help="re-scramble every output and compare with TEKKEN.BIN")
    args = p.parse_args()
    start = time.monotonic()
    try:
        table, kinds, total = extract(args.elf, args.bin, args.out, args.verify)
    except (Refused, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    present = sum(1 for e in table if e.size > 0)
    print(f"{args.out}: {len(table)} files ({present} with data, {sum(e.scrambled for e in table)} descrambled), "
          f"{total:,} bytes in {time.monotonic() - start:.1f} s")
    print("kinds: " + ", ".join(f"{k} {n}" for k, n in kinds.most_common()))
    if args.verify:
        print("verified: every file re-scrambles to the bytes in TEKKEN.BIN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
