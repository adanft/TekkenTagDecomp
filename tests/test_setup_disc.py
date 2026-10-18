"""Tests for scripts/setup_disc.py on small synthetic disc images (no real disc needed).

Run: .venv/bin/python -m unittest discover -s tests -v
"""

import hashlib
import importlib.util
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "scripts" / "setup_disc.py"

_spec = importlib.util.spec_from_file_location("setup_disc", SCRIPT)
setup_disc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(setup_disc)

SECTOR = 2048
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"
ELF = b"\x7fELF" + bytes(range(256)) * 20
SYSTEM_CNF = b"BOOT2 = cdrom0:\\SLUS_200.01;1\r\nVER = 2.00\r\nVMODE = NTSC\r\n"
GAME_FILES = {
    "SYSTEM.CNF": SYSTEM_CNF,
    "SLUS_200.01": ELF,
    "TEKKEN.BIN": bytes(range(251)) * 40,  # spans several sectors
    "IRX/PADMAN.IRX": b"padman" * 100,
    "IRX/EMPTY.IRX": b"",
}


def both16(v):
    return struct.pack("<H", v) + struct.pack(">H", v)


def both32(v):
    return struct.pack("<I", v) + struct.pack(">I", v)


def dir_record(name, lba, size, is_dir):
    pad = b"\x00" if len(name) % 2 == 0 else b""
    length = 33 + len(name) + len(pad)
    return (bytes([length, 0]) + both32(lba) + both32(size) + bytes(7) + bytes([2 if is_dir else 0, 0, 0])
            + both16(1) + bytes([len(name)]) + name + pad)


def make_iso(files):
    """A minimal ISO9660 image (2048-byte sectors) holding `files` ({"DIR/NAME": bytes}); one directory level."""
    dirs = {"": []}
    for path in files:
        parent = path.rpartition("/")[0]
        dirs.setdefault(parent, [])
        dirs[parent].append(path)
        if parent and parent not in dirs[""]:
            dirs[""].append(parent)
    # Layout: 16 system sectors, PVD, terminator, one sector per directory, then the files.
    dir_lba = {d: 18 + i for i, d in enumerate(dirs)}
    lba = 18 + len(dirs)
    file_lba = {}
    for path, data in files.items():
        file_lba[path] = lba
        lba += max(1, -(-len(data) // SECTOR))
    total = lba
    image = bytearray(total * SECTOR)

    def record_of(path):
        name = path.rpartition("/")[2].encode()
        if path in dirs:
            return dir_record(name, dir_lba[path], SECTOR, True)
        return dir_record(name + b";1", file_lba[path], len(files[path]), False)

    for d, children in dirs.items():
        parent = d.rpartition("/")[0] if d else ""
        body = dir_record(b"\x00", dir_lba[d], SECTOR, True) + dir_record(b"\x01", dir_lba[parent], SECTOR, True)
        for child in sorted(children):
            body += record_of(child)
        assert len(body) <= SECTOR
        image[dir_lba[d] * SECTOR:dir_lba[d] * SECTOR + len(body)] = body
    for path, data in files.items():
        image[file_lba[path] * SECTOR:file_lba[path] * SECTOR + len(data)] = data

    pvd = bytearray(SECTOR)
    pvd[0:7] = b"\x01CD001\x01"
    pvd[8:40] = b"PLAYSTATION".ljust(32)
    pvd[40:72] = b"TEST".ljust(32)
    pvd[80:88] = both32(total)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(SECTOR)
    pvd[156:190] = dir_record(b"\x00", dir_lba[""], SECTOR, True)
    pvd[881] = 1
    image[16 * SECTOR:17 * SECTOR] = pvd
    image[17 * SECTOR:17 * SECTOR + 7] = b"\xffCD001\x01"
    return bytes(image)


def to_bin(iso):
    """Wraps 2048-byte sectors into raw MODE2/2352 sectors (sync, header, subheader, data, empty EDC/ECC)."""
    out = bytearray()
    for i in range(len(iso) // SECTOR):
        m, s, f = (i + 150) // 4500, (i + 150) // 75 % 60, (i + 150) % 75
        header = bytes(int(str(x), 16) for x in (m, s, f)) + b"\x02"
        out += SYNC + header + bytes(8) + iso[i * SECTOR:(i + 1) * SECTOR] + bytes(280)
    return bytes(out)


def expect_of(elf):
    return len(elf), hashlib.sha1(elf).hexdigest()


class Tmp(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.iso_bytes = make_iso(GAME_FILES)
        self.iso = self.tmp / "game.iso"
        self.iso.write_bytes(self.iso_bytes)
        self.bin = self.tmp / "game.bin"
        self.bin.write_bytes(to_bin(self.iso_bytes))

    def tearDown(self):
        self._tmp.cleanup()

    def cli(self, *args):
        return subprocess.run([sys.executable, SCRIPT, *map(str, args)], capture_output=True, text=True, timeout=60)


class Conversion(Tmp):
    def test_bin_converts_to_the_same_iso(self):
        out = self.tmp / "out.iso"
        setup_disc.convert_bin(self.bin, out)
        self.assertEqual(out.read_bytes(), self.iso_bytes)

    def test_cli_saves_iso_from_bin(self):
        out = self.tmp / "saved.iso"
        r = self.cli(self.bin, "--save-iso", out)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(out.read_bytes(), self.iso_bytes)

    def test_partial_last_sector_is_refused(self):
        self.bin.write_bytes(to_bin(self.iso_bytes) + b"\x00" * 100)
        with self.assertRaises(setup_disc.Refused):
            setup_disc.convert_bin(self.bin, self.tmp / "out.iso")


class Reading(Tmp):
    def test_every_layout_reads_the_same_files(self):
        cue = self.tmp / "game.cue"
        cue.write_text('FILE "game.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')
        for image in (self.iso, self.bin, cue):
            with self.subTest(image=image.name), setup_disc.open_disc(image) as disc:
                for path, data in GAME_FILES.items():
                    self.assertEqual(disc.read(path), data, path)

    def test_cue_naming_a_missing_bin_falls_back_to_its_own_name(self):
        cue = self.tmp / "game.cue"
        cue.write_text('FILE "Renamed (USA).bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')
        with setup_disc.open_disc(cue) as disc:
            self.assertEqual(disc.read("SYSTEM.CNF"), SYSTEM_CNF)

    def test_lookup_is_case_insensitive_and_reports_missing_files(self):
        with setup_disc.open_disc(self.iso) as disc:
            self.assertEqual(disc.find("irx/padman.irx").size, len(GAME_FILES["IRX/PADMAN.IRX"]))
            self.assertIsNone(disc.find("IRX/NOPE.IRX"))
            self.assertIsNone(disc.find("NOPE/PADMAN.IRX"))

    def test_lists_the_whole_tree(self):
        with setup_disc.open_disc(self.iso) as disc:
            self.assertEqual(sorted(e.path for e in disc.walk()), sorted(GAME_FILES))

    def test_unsafe_names_are_refused(self):
        self.iso.write_bytes(make_iso({"SYSTEM.CNF": SYSTEM_CNF, "..": b"x"}))
        with setup_disc.open_disc(self.iso) as disc, self.assertRaises(setup_disc.Refused):
            list(disc.walk())


class Setup(Tmp):
    def test_right_disc_extracts_everything(self):
        out = self.tmp / "iso"
        for image in (self.iso, self.bin):
            with self.subTest(image=image.name):
                setup_disc.setup(image, out, expect=expect_of(ELF), force=True)
                for path, data in GAME_FILES.items():
                    self.assertEqual((out / path).read_bytes(), data, path)
                self.assertEqual(list(out.rglob("*.part")), [])

    def test_rerun_skips_files_already_extracted(self):
        out = self.tmp / "iso"
        first = setup_disc.setup(self.iso, out, expect=expect_of(ELF))
        self.assertEqual(first, (len(GAME_FILES), 0))
        again = setup_disc.setup(self.iso, out, expect=expect_of(ELF))
        self.assertEqual(again, (0, len(GAME_FILES)))
        (out / "IRX" / "PADMAN.IRX").write_bytes(b"short")
        fixed = setup_disc.setup(self.iso, out, expect=expect_of(ELF))
        self.assertEqual(fixed, (1, len(GAME_FILES) - 1))
        self.assertEqual((out / "IRX" / "PADMAN.IRX").read_bytes(), GAME_FILES["IRX/PADMAN.IRX"])

    def test_wrong_elf_hash_is_refused_and_nothing_is_written(self):
        out = self.tmp / "iso"
        r = self.cli(self.iso, "--out", out)
        self.assertEqual(r.returncode, 1)
        self.assertIn("SLUS_200.01", r.stderr)
        self.assertFalse(out.exists() and any(out.iterdir()))

    def test_wrong_elf_size_is_refused(self):
        with self.assertRaisesRegex(setup_disc.Refused, "size"):
            setup_disc.setup(self.iso, self.tmp / "iso", expect=(len(ELF) + 1, hashlib.sha1(ELF).hexdigest()))

    def test_non_disc_file_is_refused(self):
        text = self.tmp / "README.md"
        text.write_text("# not a disc\n" * 500)
        r = self.cli(text, "--out", self.tmp / "iso")
        self.assertEqual(r.returncode, 1)
        self.assertIn("not a disc image", r.stderr)

    def test_missing_file_is_refused(self):
        r = self.cli(self.tmp / "nothing.iso", "--out", self.tmp / "iso")
        self.assertEqual(r.returncode, 1)
        self.assertIn("no such file", r.stderr)

    def test_other_game_is_refused(self):
        self.iso.write_bytes(make_iso({"SYSTEM.CNF": b"BOOT2 = cdrom0:\\SLUS_999.99;1\r\n", "SLUS_999.99": ELF}))
        with self.assertRaisesRegex(setup_disc.Refused, "SLUS_999.99"):
            setup_disc.setup(self.iso, self.tmp / "iso", expect=expect_of(ELF))

    def test_disc_without_system_cnf_is_refused(self):
        self.iso.write_bytes(make_iso({"SLUS_200.01": ELF}))
        with self.assertRaisesRegex(setup_disc.Refused, "SYSTEM.CNF"):
            setup_disc.setup(self.iso, self.tmp / "iso", expect=expect_of(ELF))


if __name__ == "__main__":
    unittest.main()
