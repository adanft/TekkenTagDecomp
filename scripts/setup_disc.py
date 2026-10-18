#!/usr/bin/env python3
"""Checks your own Tekken Tag Tournament disc (USA, SLUS-20001) and copies its files into iso/.

Usage: python3 scripts/setup_disc.py <disc.iso | disc.cue | disc.bin> [--out DIR] [--force]
       python3 scripts/setup_disc.py <disc.cue | disc.bin> --save-iso <out.iso>

Takes an ISO (2048-byte sectors) or a raw BIN/CUE (2352-byte sectors, MODE2 or MODE1) and reads its ISO9660
file system directly, with no external tool. Before writing anything it checks that the disc boots SLUS_200.01
and that this executable has the expected size and SHA-1; any other file or disc is refused with exit code 1.
Then every file of the disc is copied into iso/ (about 640 MB), streamed. Files already there with the right size
are skipped, so the script can be run again; --force rewrites them. --save-iso only converts a BIN/CUE into an ISO.
"""

import argparse
import hashlib
import os
import re
import shutil
import sys
from collections import namedtuple
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ELF_NAME = "SLUS_200.01"
ELF_SIZE = 2_873_728
ELF_SHA1 = "c7ad9a022bdc836827a0cc093f91b4281836f2bb"

SECTOR = 2048
CHUNK = 512  # sectors read at a time (1 MB of data)
# (raw sector size, offset of the 2048 data bytes, name): plain ISO, then raw CD sectors.
LAYOUTS = ((2048, 0, "ISO"), (2352, 24, "MODE2/2352"), (2352, 16, "MODE1/2352"))
CUE_MODES = {"MODE2/2352", "MODE1/2352", "MODE1/2048"}

Entry = namedtuple("Entry", "path lba size is_dir")


class Refused(Exception):
    """The input is not the expected disc, or cannot be read; the message says why."""


class Disc:
    """The ISO9660 file system of a disc image, read sector by sector from any layout in LAYOUTS."""

    def __init__(self, path, raw, offset):
        self.path, self.raw, self.offset = path, raw, offset
        self.file = open(path, "rb")
        self.sectors = os.fstat(self.file.fileno()).st_size // raw
        pvd = self.data(16, 1)
        if int.from_bytes(pvd[128:130], "little") != SECTOR:
            raise Refused(f"{path.name}: unsupported ISO9660 block size")
        root = pvd[156:190]
        self.root = Entry("", int.from_bytes(root[2:6], "little"), int.from_bytes(root[10:14], "little"), True)

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.file.close()

    def chunks(self, lba, size):
        """The `size` bytes stored from sector `lba`, in pieces of at most CHUNK sectors."""
        count = -(-size // SECTOR)
        if lba + count > self.sectors:
            raise Refused(f"{self.path.name} is truncated (sector {lba + count} past its end)")
        while size > 0:
            n = min(count, CHUNK)
            piece = self.data(lba, n)[:size]
            yield piece
            lba, count, size = lba + n, count - n, size - len(piece)

    def data(self, lba, n):
        """The data bytes of `n` sectors from `lba` (the 2048-byte user data of each raw sector)."""
        self.file.seek(lba * self.raw)
        buf = self.file.read(n * self.raw)
        if self.raw == SECTOR:
            return buf
        view = memoryview(buf)
        return b"".join(view[i + self.offset:i + self.offset + SECTOR] for i in range(0, len(buf), self.raw))

    def _record(self, rec, parent):
        if len(rec) < 34 or rec[0] < 34:
            raise Refused(f"{self.path.name}: damaged directory record")
        name = rec[33:33 + rec[32]]
        if name in (b"\x00", b"\x01"):
            return None
        flags = rec[25]
        if flags & 0x80:
            raise Refused(f"{self.path.name}: files split in several extents are not supported")
        name = name.decode("ascii", "replace").split(";")[0].rstrip(".")
        if name in ("", ".", "..") or any(c in name for c in "/\\\x00"):
            raise Refused(f"{self.path.name}: unsafe file name on the disc: {name!r}")
        return Entry(f"{parent}/{name}" if parent else name, int.from_bytes(rec[2:6], "little"),
                     int.from_bytes(rec[10:14], "little"), bool(flags & 2))

    def listdir(self, d):
        """The entries of directory `d` (records never cross a sector; a zero length pads to the next one)."""
        if d.size > 16 << 20:
            raise Refused(f"{self.path.name}: directory {d.path or '/'} is implausibly large")
        data = b"".join(self.chunks(d.lba, d.size))
        out, pos = [], 0
        while pos < len(data):
            n = data[pos]
            if n == 0:
                pos = (pos // SECTOR + 1) * SECTOR
                continue
            entry = self._record(data[pos:pos + n], d.path)
            if entry:
                out.append(entry)
            pos += n
        return out

    def walk(self):
        """Every file on the disc, depth first."""
        todo, seen = [self.root], set()
        while todo:
            d = todo.pop()
            if d.lba in seen:
                continue
            seen.add(d.lba)
            for e in self.listdir(d):
                if e.is_dir:
                    todo.append(e)
                else:
                    yield e

    def find(self, path):
        """The entry at `path` ("IRX/PADMAN.IRX", any case), or None."""
        entry = self.root
        for part in path.upper().split("/"):
            if not entry.is_dir:
                return None
            entry = next((e for e in self.listdir(entry) if e.path.rpartition("/")[2].upper() == part), None)
            if entry is None:
                return None
        return entry

    def read(self, path):
        entry = self.find(path)
        if entry is None:
            raise Refused(f"{self.path.name}: no {path} on the disc")
        return b"".join(self.chunks(entry.lba, entry.size))


def bin_of_cue(cue):
    """The BIN file a CUE sheet points at; its first track must hold data."""
    text = cue.read_text(errors="replace")
    name = re.search(r'^\s*FILE\s+"?([^"\r\n]+?)"?\s+\w+\s*$', text, re.M | re.I)
    mode = re.search(r"^\s*TRACK\s+\d+\s+(\S+)", text, re.M | re.I)
    if not name or not mode:
        raise Refused(f"{cue.name} is not a CUE sheet (no FILE or TRACK line)")
    if mode.group(1).upper() not in CUE_MODES:
        raise Refused(f"{cue.name}: first track is {mode.group(1)}, expected MODE2/2352 (a PS2 data track)")
    for candidate in (cue.parent / name.group(1), cue.with_suffix(".bin"), cue.with_suffix(".BIN")):
        if candidate.is_file():
            return candidate
    raise Refused(f"{cue.name} names {name.group(1)!r}, which is not next to it")


def open_disc(path):
    """A Disc for an ISO, BIN or CUE file; the sector layout is found from the volume descriptor at sector 16."""
    path = Path(path).expanduser()
    if not path.is_file():
        raise Refused(f"no such file: {path}")
    if path.suffix.lower() == ".cue":
        path = bin_of_cue(path)
    size = path.stat().st_size
    with open(path, "rb") as f:
        for raw, offset, layout in LAYOUTS:
            f.seek(16 * raw + offset)
            if f.read(6) == b"\x01CD001":
                if raw != SECTOR and size % raw:
                    raise Refused(f"{path.name} ends with a partial {raw}-byte sector: truncated image")
                return Disc(path, raw, offset)
    raise Refused(f"{path.name} is not a disc image (no ISO9660 volume as {', '.join(l[2] for l in LAYOUTS)})")


def copy_out(disc, entry, dst):
    """Streams one file to `dst` through a .part file, so an interrupted copy never looks complete."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    part = dst.with_name(dst.name + ".part")
    try:
        with open(part, "wb") as f:
            for piece in disc.chunks(entry.lba, entry.size):
                f.write(piece)
        os.replace(part, dst)
    finally:
        part.unlink(missing_ok=True)


def sha1_of(pieces):
    h = hashlib.sha1()
    for piece in pieces:
        h.update(piece)
    return h.hexdigest()


def check_game(disc, expect):
    """Refuses any disc that does not boot the expected, unmodified executable."""
    cnf = disc.find("SYSTEM.CNF")
    if cnf is None:
        raise Refused(f"{disc.path.name} is a disc image but not a PS2 game disc (no SYSTEM.CNF)")
    boot = re.search(rb"BOOT2\s*=\s*cdrom0:\\?([^;\s]+)", b"".join(disc.chunks(cnf.lba, cnf.size)), re.I)
    boot = boot.group(1).decode("ascii", "replace") if boot else "nothing"
    if boot.upper() != ELF_NAME:
        raise Refused(f"this disc boots {boot}, not {ELF_NAME}: it is not Tekken Tag Tournament (USA, SLUS-20001)")
    elf = disc.find(ELF_NAME)
    if elf is None:
        raise Refused(f"SYSTEM.CNF boots {ELF_NAME} but the disc has no such file: damaged image")
    size, sha1 = expect
    if elf.size != size:
        raise Refused(f"{ELF_NAME} has size {elf.size:,}, expected {size:,}. Only the unmodified USA release "
                      "(SLUS-20001) is supported; a patched or damaged image will not work.")
    got = sha1_of(disc.chunks(elf.lba, elf.size))
    if got != sha1:
        raise Refused(f"{ELF_NAME} has SHA-1 {got}, expected {sha1}. Only the unmodified USA release "
                      "(SLUS-20001) is supported; a patched or damaged image will not work.")


def up_to_date(path, entry, expect):
    if not path.is_file() or path.stat().st_size != entry.size:
        return False
    if entry.path.upper() == ELF_NAME:  # small: always re-checked
        with open(path, "rb") as f:
            return sha1_of(iter(lambda: f.read(1 << 20), b"")) == expect[1]
    return True


def setup(image, out, expect=(ELF_SIZE, ELF_SHA1), force=False):
    """Checks the disc, then copies its files into `out`. Returns (files written, files skipped)."""
    out = Path(out)
    with open_disc(image) as disc:
        check_game(disc, expect)
        files = list(disc.walk())
        todo = [e for e in files if force or not up_to_date(out / e.path, e, expect)]
        need = sum(e.size for e in todo)
        if todo:
            out.mkdir(parents=True, exist_ok=True)
            free = shutil.disk_usage(out).free
            if need > free:
                raise Refused(f"{need / 1e6:,.0f} MB needed in {out}, only {free / 1e6:,.0f} MB free")
        for e in todo:
            copy_out(disc, e, out / e.path)
    if not up_to_date(out / ELF_NAME, Entry(ELF_NAME, 0, expect[0], False), expect):
        raise Refused(f"{out / ELF_NAME} does not match after copying: check the disk and run again with --force")
    return len(todo), len(files) - len(todo)


def convert_bin(image, dst):
    """Writes the 2048-byte data of every sector of a BIN (or CUE) image as an ISO file."""
    dst = Path(dst)
    with open_disc(image) as disc:
        part = dst.with_name(dst.name + ".part")
        try:
            with open(part, "wb") as f:
                for lba in range(0, disc.sectors, CHUNK):
                    f.write(disc.data(lba, min(CHUNK, disc.sectors - lba)))
            os.replace(part, dst)
        finally:
            part.unlink(missing_ok=True)
    return disc.sectors


def shown(path):
    try:
        return str(Path(path).resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def main():
    p = argparse.ArgumentParser(description="Check your own Tekken Tag Tournament disc and copy its files.")
    p.add_argument("image", help="disc image: .iso, .cue or .bin")
    p.add_argument("--out", type=Path, help="where the files go (default: iso/)")
    p.add_argument("--force", action="store_true", help="rewrite files that are already there")
    p.add_argument("--save-iso", type=Path, metavar="FILE", help="only convert the image into an ISO file")
    args = p.parse_args()
    if args.save_iso and (args.out or args.force):
        p.error("--save-iso only converts the image; it cannot be combined with --out or --force")
    args.out = args.out or ROOT / "iso"
    try:
        if args.save_iso:
            n = convert_bin(args.image, args.save_iso)
            print(f"{shown(args.save_iso)}: {n:,} sectors of 2048 bytes")
            return 0
        written, skipped = setup(args.image, args.out, force=args.force)
    except (Refused, OSError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    print(f"{ELF_NAME}: size and SHA-1 match the USA release (SLUS-20001)")
    print(f"{shown(args.out)}/: {written} file(s) written, {skipped} already there")
    return 0


if __name__ == "__main__":
    sys.exit(main())
