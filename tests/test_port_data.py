"""The game's data from the user's disc (port/src/plat_data.c, T25, docs/port/data.md), checked on the built program
build/port/ttt: the data file made from the disc, the idle run on it, the refusal of a modified disc and the absence
of the game's data from the program. Skipped without the program or without disc data. Data files and modified disc
copies go to temporary directories only (they hold the game's data)."""

import hashlib
import os
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, ROOT, disc, run_ttt

LAYOUT = ROOT / "port/tools/data_layout.txt"
# The idle run (TTT_FRAMES=2551 TTT_SEED=1, a dump every 150 VBlanks) as it was with the data linked into the
# program before T25: its state hash and the SHA-1 of its 17 pictures (names and bytes in order). The run on the data
# loaded from the disc must give the same. Before T42 b1 the hash was 0xd36c82e9a145875a: two values the game takes
# from uninitialised stack bytes (dma_spr.c func_0035D790's DMA tag, Light_StartFlash's fourth float) are now fixed
# under PORT, so gcc and clang builds end alike (tests/test_port_clang.py). The pictures changed with the field
# placement (docs/port/video.md: an odd field's picture one raster line lower; the state hash did not):
# d9e628faa2fc1641224b77af0ad0f13da8a0f6f1 before. R2 (2026-10-29): 0x8ba37caeb2bacfa8 before; the 94 data words that
# are integers equal to a function's address (test_integer_words_keep_the_disc_value) keep the disc's value instead of
# a host address, the only words of EE RAM that changed (same spin, cycles and pictures; docs/port/README.md gotcha 33).
IDLE_HASH = "0x61dc0b370ff7019b"
IDLE_PICTURES = "ad85b47ca328ddc43b05d2a1f1158219aee67b6e"


def layout(kind):
    return [l.split()[1:] for l in LAYOUT.read_text().splitlines() if l.startswith(kind + " ")]


def disc_file(name):
    """The bytes of a root file of the disc (a directory, or an ISO 9660 image), or None."""
    d = disc()
    if d is None:
        return None
    if d.is_dir():
        p = d / name
        return p.read_bytes() if p.exists() else None
    with open(d, "rb") as f:
        f.seek(16 * 2048)
        pvd = f.read(2048)
        lba, size = struct.unpack_from("<I", pvd, 158)[0], struct.unpack_from("<I", pvd, 166)[0]
        f.seek(lba * 2048)
        root = f.read(size)
        i = 0
        while i < len(root):
            n = root[i]
            if n == 0:
                i = (i // 2048 + 1) * 2048
                continue
            ident = root[i + 33:i + 33 + root[i + 32]].split(b";")[0].decode("ascii", "replace")
            if ident == name:
                f.seek(struct.unpack_from("<I", root, i + 2)[0] * 2048)
                return f.read(struct.unpack_from("<I", root, i + 10)[0])
            i += n
    return None


def header(path):
    b = Path(path).read_bytes()
    return {"magic": b[:8], "size": struct.unpack_from("<I", b, 8)[0], "flags": struct.unpack_from("<I", b, 12)[0],
            "payload": struct.unpack_from("<I", b, 16)[0], "sha1": b[24:44].hex(), "elf": b[44:64].hex(),
            "data": b[64:]}


def disc_copy(where, damage=()):
    """A disc directory in `where` with links to the real disc's files, except the (root-relative) files of
    `damage`: copies with one byte changed."""
    src = disc()
    out = Path(where) / "disc"
    out.mkdir()
    for p in src.rglob("*"):
        rel = p.relative_to(src)
        if p.is_dir():
            (out / rel).mkdir(parents=True, exist_ok=True)
        elif str(rel) in damage:
            b = bytearray(p.read_bytes())
            b[0x2000 if len(b) > 0x4000 else len(b) // 2] ^= 0x01
            (out / rel).write_bytes(bytes(b))
        else:
            (out / rel).symlink_to(p.resolve())
    return out


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None and (disc().is_dir() or disc().suffix.lower() == ".iso"), "no disc data")
class DataFileTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="ttt-data-")
        cls.dat = Path(cls.tmp.name) / "ttt.dat"
        cls.first = run_ttt(TTT_SELFTEST="mem", TTT_DATA=str(cls.dat))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_made_from_the_disc(self):
        """The first run checks the disc and writes the data file: the payload the layout expects, the SHA-1 of the
        disc's SLUS_200.01 (the port's own SHA-1, checked against Python's), not marked modified."""
        self.assertEqual(self.first.returncode, 0, self.first.stdout + self.first.stderr)
        self.assertIn("ttt: memory self-test OK", self.first.stdout)
        h = header(self.dat)
        size, sha1 = layout("payload")[0]
        self.assertEqual((h["magic"], h["size"], h["flags"]), (b"TTTDAT\0\1", 64, 0))
        self.assertEqual(h["payload"], int(size, 16))
        self.assertEqual(len(h["data"]), int(size, 16))
        self.assertEqual(hashlib.sha1(h["data"]).hexdigest(), sha1)
        self.assertEqual(h["sha1"], sha1)
        self.assertEqual(h["elf"], hashlib.sha1(disc_file("SLUS_200.01")).hexdigest())

    def test_payload_is_the_executables_bytes(self):
        """The payload is the disc executable's bytes at the layout's ranges, in address order."""
        elf = disc_file("SLUS_200.01")
        want = b"".join(elf[0x1000 + int(a, 16) - 0x340000:][:int(n, 16)] for a, n in layout("range"))
        self.assertEqual(header(self.dat)["data"], want)

    def test_integer_words_keep_the_disc_value(self):
        """R2: the layout lists the data words the split named after a function because they equal its address; for
        the kernel calls at 0x400000-0x400520 and three game functions those words are integers (command entries,
        move flags, packed bytes), never pointers, and must keep the disc's value. With StartThread's host address in
        the command entry at 0x485C28 (move 0x40), Yoshimitsu's forward walk went on to a fall (move 0x813)."""
        names = {"StartThread", "SleepThread", "ExitThread", "iTerminateThread", "SignalSema", "WaitSema", "GsPutIMR",
                 "_iWakeupThread", "DisableDispatchThread", "func_00340000", "func_003A0020", "func_00403240"}
        addrs = [int(a, 16) for a, name, _ in layout("word") if name in names]
        self.assertGreater(len(addrs), 80)
        self.assertIn(0x485C28, addrs)
        r = run_ttt(TTT_FRAMES="2", TTT_SEED="1", TTT_DATA=str(self.dat), TTT_PEEK=",".join(f"{a:X}" for a in addrs))
        self.assertEqual(r.returncode, 0, (r.stdout + r.stderr)[-3000:])
        got = {int(a, 16): int(w, 16) for a, w in re.findall(r"ttt: peek ([0-9A-F]{8}): ([0-9A-F]{8})", r.stdout)}
        elf = disc_file("SLUS_200.01")
        for a in addrs:
            self.assertEqual(got.get(a), struct.unpack_from("<I", elf, 0x1000 + a - 0x340000)[0], f"word 0x{a:08X}")

    def test_damaged_file_made_again(self):
        """A data file whose payload does not match its SHA-1 is made again from the disc."""
        with tempfile.TemporaryDirectory(prefix="ttt-data-") as d:
            bad = Path(d) / "ttt.dat"
            b = bytearray(self.dat.read_bytes())
            b[64 + 0x1000] ^= 0xFF
            bad.write_bytes(bytes(b))
            r = run_ttt(TTT_SELFTEST="mem", TTT_DATA=str(bad))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("is damaged", r.stderr)
            self.assertEqual(bad.read_bytes(), self.dat.read_bytes())

    def test_idle_run_from_the_data_file(self):
        """The idle run on the data loaded from the data file: the same pictures and state hash as with the data
        linked into the program before T25 (the hash as moved by T42 b1 and R2, IDLE_HASH)."""
        with tempfile.TemporaryDirectory(prefix="ttt-dump-") as d:
            r = run_ttt(timeout=900, TTT_FRAMES="2551", TTT_SEED="1", TTT_DUMP=d, TTT_DUMP_EVERY="150",
                        TTT_DATA=str(self.dat))
            self.assertEqual(r.returncode, 0, (r.stdout + r.stderr)[-3000:])
            self.assertNotIn("making it again", r.stderr)
            self.assertIn(f"hash {IDLE_HASH} ", r.stdout)
            h = hashlib.sha1()
            pics = sorted(Path(d).glob("frame_*.png"))
            self.assertEqual(len(pics), 17)
            for p in pics:
                h.update(p.name.encode())
                h.update(p.read_bytes())
            self.assertEqual(h.hexdigest(), IDLE_PICTURES)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None and disc().is_dir(), "no disc directory (iso/ from scripts/setup_disc.py)")
class ModifiedDiscTest(unittest.TestCase):
    """A disc whose files differ from the USA release is refused (exit code 4) unless TTT_ALLOW_MODIFIED=1."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-mod-")
        self.dat = Path(self.tmp.name) / "ttt.dat"

    def tearDown(self):
        self.tmp.cleanup()

    def test_modified_executable_refused(self):
        d = disc_copy(self.tmp.name, {"SLUS_200.01"})
        r = run_ttt(TTT_FRAMES="1", TTT_DISC=str(d), TTT_DATA=str(self.dat))
        self.assertEqual(r.returncode, 4, r.stdout + r.stderr)
        self.assertIn("ttt: unknown or modified disc", r.stderr)
        self.assertIn("SLUS_200.01 has SHA-1", r.stderr)
        self.assertIn("TTT_ALLOW_MODIFIED=1", r.stderr)
        self.assertFalse(self.dat.exists())

    def test_modified_executable_allowed_and_marked(self):
        """With TTT_ALLOW_MODIFIED=1 the run goes on, says [modified game data] and marks the data file; the
        unmodified disc later makes the file again, unmarked."""
        d = disc_copy(self.tmp.name, {"SLUS_200.01"})
        r = run_ttt(TTT_SELFTEST="mem", TTT_DISC=str(d), TTT_DATA=str(self.dat), TTT_ALLOW_MODIFIED="1")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("[modified game data]", r.stderr)
        self.assertEqual(header(self.dat)["flags"], 1)
        r = run_ttt(TTT_SELFTEST="mem", TTT_DISC=str(d), TTT_DATA=str(self.dat))
        self.assertEqual(r.returncode, 4, r.stdout + r.stderr)
        r = run_ttt(TTT_SELFTEST="mem", TTT_DATA=str(self.dat))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(header(self.dat)["flags"], 0)

    def test_modified_sound_driver_refused_with_a_good_data_file(self):
        """RSPU2DRV.IRX (read at run time) is checked on every run, also when the data file is already made."""
        r = run_ttt(TTT_SELFTEST="mem", TTT_DATA=str(self.dat))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        d = disc_copy(self.tmp.name, {"IRX/RSPU2DRV.IRX"})
        r = run_ttt(TTT_FRAMES="1", TTT_DISC=str(d), TTT_DATA=str(self.dat))
        self.assertEqual(r.returncode, 4, r.stdout + r.stderr)
        self.assertIn("IRX/RSPU2DRV.IRX has SHA-1", r.stderr)


from test_port_window import NO_SDL, SDL_LIBS  # noqa: E402
MARK = "[modified game data]"


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None and disc().is_dir(), "no disc directory (iso/ from scripts/setup_disc.py)")
class DataMarkTest(unittest.TestCase):
    """T41: "[modified game data]" in the report, the window title and the overlay when the data in use differs from
    the USA release (here a copy of the disc with one file changed, made in a temporary directory), nothing otherwise,
    TTT_ALLOW_MODIFIED=1 alone included."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="ttt-mark-")
        cls.dat = Path(cls.tmp.name) / "ttt.dat"
        cls.first = run_ttt(TTT_SELFTEST="mem", TTT_DATA=str(cls.dat))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def data_copy(self, where, cache=True):
        """The data file (and, with `cache`, its record of TEKKEN.BIN's check) copied into `where`."""
        dat = Path(where) / "ttt.dat"
        shutil.copyfile(self.dat, dat)
        if cache:
            shutil.copyfile(str(self.dat) + ".disc", str(dat) + ".disc")
        return dat

    def test_check_recorded(self):
        """Making the data file hashed all of TEKKEN.BIN: `<data file>.disc` keeps its SHA-1 for the file as it is."""
        self.assertEqual(self.first.returncode, 0, self.first.stdout + self.first.stderr)
        line = Path(str(self.dat) + ".disc").read_text()
        self.assertRegex(line, r"^TTTDISC 1 \d+:\d+ \d+ 630474752 \d+ \d+ 0 0ae3157da21c92b94341ef12ceb9afb29bd2b6c0\n$")

    def test_unmodified_no_mark(self):
        for env in ({}, {"TTT_ALLOW_MODIFIED": "1"}):
            with self.subTest(**env), tempfile.TemporaryDirectory(prefix="ttt-mark-") as d:
                r = run_ttt(TTT_FRAMES="30", TTT_DATA=str(self.data_copy(d)), **env)
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                self.assertIn("ttt: data: USA release (verified)\n", r.stdout)
                self.assertNotIn(MARK, r.stdout + r.stderr)

    def test_modified_sound_driver_marked(self):
        with tempfile.TemporaryDirectory(prefix="ttt-mark-") as d:
            disc_dir = disc_copy(d, {"IRX/RSPU2DRV.IRX"})
            r = run_ttt(TTT_FRAMES="30", TTT_DISC=str(disc_dir), TTT_DATA=str(self.data_copy(d)), TTT_ALLOW_MODIFIED="1")
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            self.assertIn(f"ttt: data: {MARK} (IRX/RSPU2DRV.IRX)\n", r.stdout)

    @unittest.skipUnless(any(p.exists() for p in SDL_LIBS), NO_SDL)
    def test_window_title_and_overlay(self):
        """The title and the settings window (TTT_OVERLAY_TEST: F1 opened from inside, a hidden window on SDL's dummy
        driver) carry the mark with the changed disc only."""
        for damage, marked in (((), False), (("IRX/RSPU2DRV.IRX",), True)):
            with self.subTest(marked=marked), tempfile.TemporaryDirectory(prefix="ttt-mark-") as d:
                disc_dir = disc_copy(d, set(damage))
                r = run_ttt(TTT_FRAMES="400", TTT_SEED="1", TTT_DISC=str(disc_dir), TTT_DATA=str(self.data_copy(d)),
                            TTT_ALLOW_MODIFIED="1", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER="sw",
                            TTT_AUDIO="0", SDL_VIDEO_DRIVER="dummy", TTT_OVERLAY_TEST="1",
                            TTT_CONFIG=str(Path(d) / "settings.ini"))
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                title = "Tekken Tag Tournament" + (" " + MARK if marked else "")
                self.assertIn(f'ttt: window: title "{title}"\n', r.stdout)
                n = int(re.search(r"ttt: overlay: data mark drawn (\d+) times", r.stdout).group(1))
                self.assertEqual(n > 0, marked, r.stdout[-1500:])

    def test_modified_archive_found_in_the_background(self):
        """A changed TEKKEN.BIN (one byte, in a copy) with a data file already made: it is hashed on a thread of its
        own while the game runs, the run is marked from then on, and the next run refuses the disc unless
        TTT_ALLOW_MODIFIED=1."""
        src = disc() / "TEKKEN.BIN"
        if shutil.disk_usage(EXE.parent).free < 3 * src.stat().st_size:
            self.skipTest("not enough free space for a copy of TEKKEN.BIN")
        with tempfile.TemporaryDirectory(prefix="ttt-mark-", dir=EXE.parent) as d:  # build/: the same file system
            disc_dir = disc_copy(d)
            (disc_dir / "TEKKEN.BIN").unlink()
            subprocess.run(["cp", "--reflink=auto", str(src), str(disc_dir / "TEKKEN.BIN")], check=True)
            with open(disc_dir / "TEKKEN.BIN", "r+b") as f:
                f.seek(300 << 20)
                b = f.read(1)
                f.seek(300 << 20)
                f.write(bytes([b[0] ^ 1]))
            dat = self.data_copy(d, cache=False)
            r = run_ttt(TTT_FRAMES="30", TTT_DISC=str(disc_dir), TTT_DATA=str(dat))
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            self.assertIn(f"ttt: data: {MARK} (TEKKEN.BIN)\n", r.stdout)
            self.assertIn("the next run refuses this disc unless TTT_ALLOW_MODIFIED=1", r.stderr)
            r = run_ttt(TTT_FRAMES="30", TTT_DISC=str(disc_dir), TTT_DATA=str(dat))
            self.assertEqual(r.returncode, 4, r.stdout[-2000:] + r.stderr[-2000:])
            self.assertIn("TEKKEN.BIN has SHA-1", r.stderr)
            r = run_ttt(TTT_FRAMES="30", TTT_DISC=str(disc_dir), TTT_DATA=str(dat), TTT_ALLOW_MODIFIED="1")
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            self.assertIn(f"ttt: data: {MARK} (TEKKEN.BIN)\n", r.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc_file("SLUS_200.01") is not None, "no disc data")
class NoGameDataInTheProgramTest(unittest.TestCase):
    def test_no_data_window_in_the_program(self):
        """No 32-byte window of the game's initialised data (taken every 1 KB of the layout's ranges from the disc's
        executable, varied ones only: 16 distinct byte values or more) is in the program."""
        elf = disc_file("SLUS_200.01")
        exe = EXE.read_bytes()
        windows = []
        for a, n in layout("range"):
            a, n = int(a, 16), int(n, 16)
            for off in range(0, n - 32, 1024):
                w = elf[0x1000 + a + off - 0x340000:][:32]
                if len(set(w)) >= 16:
                    windows.append((a + off, w))
        self.assertGreater(len(windows), 200)
        found = [hex(a) for a, w in windows if w in exe]
        self.assertEqual(found, [])

    def test_no_word_table_of_the_game(self):
        """The words the program rewrites are host addresses or 0 (gPortDataFix): the layout names no D_ address."""
        self.assertEqual([w for w in layout("word") if re.fullmatch(r"D_[0-9A-F]+", w[1])], [])


if __name__ == "__main__":
    unittest.main()
