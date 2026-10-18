"""Tests for scripts/extract_disc.py on small synthetic data (no real game bytes).

Run: .venv/bin/python -m unittest discover -s tests -v
"""

import importlib.util
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPT = ROOT / "scripts" / "extract_disc.py"

_spec = importlib.util.spec_from_file_location("extract_disc", SCRIPT)
extract_disc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(extract_disc)

M64 = (1 << 64) - 1
BASE = 0x4820
BIAS = 0x350A


def make_elf(vaddr, payload, file_offset=0x1000):
    """A minimal little-endian ELF32 with one PT_LOAD segment mapping `payload` at `vaddr`."""
    header = bytearray(52)
    header[:4] = b"\x7fELF"
    header[4:7] = bytes([1, 1, 1])
    struct.pack_into("<HHIIIIIHHHHHH", header, 16, 2, 8, 1, vaddr, 52, 0, 0, 52, 32, 1, 40, 0, 0)
    phdr = struct.pack("<8I", 1, file_offset, vaddr, vaddr, len(payload), len(payload), 7, 0x1000)
    image = bytearray(file_offset) + payload
    image[:52] = header
    image[52:84] = phdr
    return bytes(image)


def check_word(file_id, size):
    return (file_id * 0x40000 + 0x11F3C + ((size + 0x7FF) >> 11)) & 0xFFFFFFFF


def table_bytes(rows):
    """rows: (sector, size, ee_index) per file id; size -1 or 0 means no data."""
    out = b""
    for i, (sector, size, ee) in enumerate(rows):
        out += struct.pack("<4I", sector, (size + BIAS) & 0xFFFFFFFF, check_word(i, size), ee)
    return out


def keystream(file_id, words):
    k = ((file_id + 0xF3C) * 218319 + (file_id - 0x348F) * 0x1234567) & M64
    for _ in range(words):
        yield k & 0xFFFFFFFF
        k = (k * 0x71C3AB9 + 0x3941) & M64


def scramble_reference(file_id, data):
    """XOR of the first 0x800 bytes, written independently of the script."""
    buf = bytearray(data) + bytes(-len(data) % 4)
    n = min(len(buf), 0x800) // 4
    for w, key in enumerate(keystream(file_id, n)):
        v = struct.unpack_from("<I", buf, 4 * w)[0] ^ key
        struct.pack_into("<I", buf, 4 * w, v)
    return bytes(buf[:len(data)])


class KeyTest(unittest.TestCase):
    def test_seed_follows_the_formula(self):
        for i in (0, 1, 0x2C, 747):
            want = ((i + 0xF3C) * 218319 + (i - 0x348F) * 0x1234567) & M64
            self.assertEqual(extract_disc.key_seed(i), want)

    def test_next_is_a_64_bit_lcg(self):
        self.assertEqual(extract_disc.key_next(0), 0x3941)
        self.assertEqual(extract_disc.key_next(M64), (M64 * 0x71C3AB9 + 0x3941) & M64)


class DescrambleTest(unittest.TestCase):
    def test_matches_the_reference_xor(self):
        data = bytes(range(256)) * 12  # 3072 bytes, longer than the scrambled part
        self.assertEqual(extract_disc.descramble(5, data), scramble_reference(5, data))

    def test_only_the_first_0x800_bytes_change(self):
        data = bytes(0x1000)
        out = extract_disc.descramble(7, data)
        self.assertNotEqual(out[:0x800], data[:0x800])
        self.assertEqual(out[0x800:], data[0x800:])

    def test_is_its_own_inverse(self):
        data = bytes((i * 37) & 0xFF for i in range(0x900))
        self.assertEqual(extract_disc.descramble(9, extract_disc.descramble(9, data)), data)

    def test_short_and_unaligned_files(self):
        for n in (0, 1, 3, 6, 0x7FF):
            data = bytes(i & 0xFF for i in range(n))
            out = extract_disc.descramble(3, data)
            self.assertEqual(len(out), n)
            self.assertEqual(out, scramble_reference(3, data))

    def test_key_depends_on_the_id(self):
        data = bytes(64)
        self.assertNotEqual(extract_disc.descramble(1, data), extract_disc.descramble(2, data))


class TableTest(unittest.TestCase):
    ADDR = 0x500000

    def table(self, rows, **kw):
        elf = make_elf(self.ADDR - 0x40, bytes(0x40) + table_bytes(rows))
        return extract_disc.read_table(elf, addr=self.ADDR, count=len(rows), **kw)

    def test_decodes_sector_size_and_flags(self):
        rows = [(BASE, 0x1234, 0), (BASE + 3, 0, 0xFFFF), (BASE + 3, -1, 2), (BASE + 3, 0x800, 0xFFFF)]
        t = self.table(rows)
        self.assertEqual([e.id for e in t], [0, 1, 2, 3])
        self.assertEqual([e.size for e in t], [0x1234, 0, -1, 0x800])
        self.assertEqual([e.offset for e in t], [0, 3 * 2048, 3 * 2048, 3 * 2048])
        self.assertEqual([e.scrambled for e in t], [True, False, False, False])

    def test_id_0x2c_is_never_scrambled(self):
        rows = [(BASE, 16, i) for i in range(0x2D)]
        t = self.table(rows)
        self.assertTrue(t[0x2B].scrambled)
        self.assertFalse(t[0x2C].scrambled)

    def test_wrong_check_word_is_refused(self):
        raw = bytearray(table_bytes([(BASE, 100, 0), (BASE + 1, 100, 1)]))
        raw[16 + 8] ^= 1
        elf = make_elf(self.ADDR, bytes(raw))
        with self.assertRaisesRegex(extract_disc.Refused, "id 1"):
            extract_disc.read_table(elf, addr=self.ADDR, count=2)

    def test_sector_below_the_base_is_refused(self):
        with self.assertRaises(extract_disc.Refused):
            self.table([(BASE - 1, 10, 0)])

    def test_address_outside_the_elf_is_refused(self):
        elf = make_elf(self.ADDR, table_bytes([(BASE, 1, 0)]))
        with self.assertRaises(extract_disc.Refused):
            extract_disc.read_table(elf, addr=self.ADDR + 0x100, count=1)
        with self.assertRaises(extract_disc.Refused):
            extract_disc.read_table(b"not an elf", addr=self.ADDR, count=1)


class KindTest(unittest.TestCase):
    def test_kinds(self):
        guess = extract_disc.guess_kind
        self.assertEqual(guess(b"", 0), "empty")
        self.assertEqual(guess(b"VAGp" + bytes(60), 64), "vag")
        pack = struct.pack("<5I", 2, 20, 8, 28, 4) + bytes(12)
        self.assertEqual(guess(pack, len(pack)), "pack")
        bad_pack = struct.pack("<5I", 2, 20, 8, 28, 400) + bytes(12)
        self.assertEqual(guess(bad_pack, len(bad_pack)), "unknown")
        adpcm = bytes([0x2A, 0x00] + [0x55] * 14) * 8 + bytes([0x0C, 0x04] + [0x11] * 14)
        self.assertEqual(guess(adpcm, len(adpcm)), "adpcm")
        self.assertEqual(guess(bytes([0x7F, 0x55] * 72), 144), "unknown")
        self.assertEqual(guess(bytes(range(64)), 64), "unknown")
        gs = struct.pack("<QQQQ", 0x4000000000000001, 0xEEEE, 0, 0x50) + bytes(32)
        self.assertEqual(guess(gs, len(gs)), "gs-image")
        self.assertEqual(guess(gs[:24] + b"\x51" + gs[25:], len(gs)), "unknown")
        movie = adpcm + b"\x00\x00\x01\xba\x44" + bytes(11)
        self.assertEqual(guess(movie, len(movie)), "mpeg-ps")


class ExtractTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        count = extract_disc.TABLE_COUNT
        files = {0: b"VAGp" + bytes(3000), 1: struct.pack("<3I", 1, 12, 4) + b"abcd" + bytes(5000),
                 3: bytes(range(256)) * 9, 0x2C: b"plain sound data" * 10}
        rows, bin_data, ee = [], bytearray(), {1, 3, 0x2C}
        for i in range(count):
            data = files.get(i, b"")
            size = len(data) if data else (-1 if i == 2 else 0)
            rows.append((BASE + len(bin_data) // 2048, size, i if i in ee else 0xFFFF))
            stored = scramble_reference(i, data) if i in ee and i != 0x2C else data
            bin_data += stored + bytes(-len(stored) % 2048)
        self.files = files
        addr = extract_disc.TABLE_ADDR
        (self.dir / "SLUS_200.01").write_bytes(make_elf(addr, table_bytes(rows)))
        (self.dir / "TEKKEN.BIN").write_bytes(bytes(bin_data))
        self.out = self.dir / "out"

    def tearDown(self):
        self.tmp.cleanup()

    def run_script(self, *args):
        return subprocess.run([sys.executable, "-I", SCRIPT, "--elf", self.dir / "SLUS_200.01",
                               "--bin", self.dir / "TEKKEN.BIN", "--out", self.out, *map(str, args)],
                              capture_output=True, text=True, timeout=60)

    def test_extracts_every_entry_descrambled_with_a_manifest(self):
        r = self.run_script("--verify")
        self.assertEqual(r.returncode, 0, r.stderr)
        for i in range(extract_disc.TABLE_COUNT):
            self.assertEqual((self.out / f"{i:03d}.bin").read_bytes(), self.files.get(i, b""), i)
        rows = [line.split("\t") for line in (self.out / "manifest.tsv").read_text().splitlines()]
        self.assertEqual(rows[0][:4], ["id", "offset", "size", "scrambled"])
        self.assertEqual(len(rows), extract_disc.TABLE_COUNT + 1)
        by_id = {int(r[0]): r for r in rows[1:]}
        self.assertEqual(by_id[0][5], "vag")
        self.assertEqual(by_id[1][5], "pack")
        self.assertEqual(by_id[2][2], "-1")
        self.assertEqual(by_id[2][5], "empty")
        self.assertEqual(by_id[0x2C][3], "0")
        self.assertEqual(by_id[3][3], "1")
        self.assertIn("verified", r.stdout)

    def test_truncated_bin_is_refused(self):
        path = self.dir / "TEKKEN.BIN"
        path.write_bytes(path.read_bytes()[:2048])
        r = self.run_script()
        self.assertEqual(r.returncode, 1)
        self.assertIn("past the end", r.stderr)

    def test_missing_input_is_refused(self):
        (self.dir / "TEKKEN.BIN").unlink()
        r = self.run_script()
        self.assertEqual(r.returncode, 1)
        self.assertIn("error", r.stderr)


if __name__ == "__main__":
    unittest.main()
