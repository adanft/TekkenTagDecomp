"""Synthetic stream sectors through the real BGM/SPU implementations, without a disc or full port build."""
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


@unittest.skipUnless(shutil.which("clang"), "clang is required for the native BGM fixture")
class StreamRepeatAddress(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="ttt-bgm-test-")
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = Path(cls.tmp.name) / "bgm"
        r = subprocess.run([
            shutil.which("clang"), "-std=c11", "-fms-extensions", "-O1",
            "-I", str(ROOT / "port/src"), str(ROOT / "tests/fixtures/bgm_stream.c"),
            str(ROOT / "port/src/plat_bgm.c"), str(ROOT / "port/src/spu2.c"), "-o", str(cls.exe),
        ], capture_output=True, text=True, timeout=30)
        if r.returncode:
            raise AssertionError(f"native fixture compile exit {r.returncode}\n{r.stdout}\n{r.stderr}")

    def check_case(self, mode, *args):
        r = subprocess.run([str(self.exe), mode, *map(str, args)],
                           capture_output=True, text=True, timeout=10)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(r.stderr, "")
        self.assertRegex(r.stdout, rf"\Abgm fixture OK {re.escape(mode)} pcm=[0-9a-f]{{8}} reads=\d+\n\Z")

    def test_interior_markers_do_not_skip_payload(self):
        self.check_case("loop")

    def test_terminal_halves_preserve_all_payload(self):
        for sectors in (1, 2, 3, 4):
            for piece in (1, 128):
                with self.subTest(sectors=sectors, service_piece=piece):
                    self.check_case("finite", sectors, piece)

    def test_stream_lock_does_not_leak_to_resident(self):
        self.check_case("resident")

    def test_ordinary_stream_and_independent_effect(self):
        self.check_case("ordinary")

    def test_held_stream_releases(self):
        self.check_case("hold")

    def test_stop_and_fade_stop(self):
        for mode in ("stop", "fade"):
            with self.subTest(mode=mode):
                self.check_case(mode)

    def test_stray_end_flags_do_not_stall_the_stream(self):
        """Issue 13: after an arcade ending with three fighters the music was gone for good. Track 3 streams from fighter
        slot 2's memory, which the ending's third fighter overwrites; the stray end flags switched voice 22 off, so no
        half was refilled and the engine stayed in its playing state, ignoring every later request."""
        self.check_case("garbage")

    def test_registered_state_restores_repeat_traversal(self):
        self.check_case("restore")


if __name__ == "__main__":
    unittest.main()
