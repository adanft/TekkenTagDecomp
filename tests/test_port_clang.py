"""The game code through clang (TTT_CC=clang, T42 b1, docs/port/README.md gotcha 32): port/tools/build.py compiles
every object into its own tree, build/port-clang/ (the default build/port/ttt is not touched), the program's
self-tests pass, and the 2551-VBlank idle run ends in exactly the gcc build's state: the same state line (seed, RNG
spin, hash, cycles) and the same hash for every region, the reference hash of tests/test_port_data.py. Skipped without
clang; slow (a build and two idle runs, a few minutes): TTT_SLOW_TESTS=1."""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

from test_port_data import IDLE_HASH
from test_port_platform import EXE, ROOT, disc

CLANG_EXE = ROOT / "build/port-clang/ttt"
SELFTESTS = ("hw", "rpc", "state", "mem", "pad", "gs", "vu1", "spu", "mc", "ee", "settings")


def run(exe, saves, timeout=900, **env):
    """The program `exe` headless with only the given TTT_ variables, the memory card and settings in `saves` (the same
    path for both builds: the card's file path passes through EE RAM)."""
    shutil.rmtree(saves, ignore_errors=True)
    os.makedirs(saves)
    e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
    if disc() is not None:
        e["TTT_DISC"] = str(disc())
    e.update(TTT_MOVIES="0", TTT_SAVES=saves, TTT_CONFIG=os.path.join(saves, "settings.ini"),
             TTT_TEXTURES=os.path.join(saves, "textures"), **env)
    return subprocess.run([str(exe)], env=e, capture_output=True, text=True, timeout=timeout)


@unittest.skipUnless(shutil.which("clang"), "no clang")
@unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (a few minutes): TTT_SLOW_TESTS=1")
class ClangBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        before = EXE.stat().st_mtime_ns if EXE.exists() else None
        cls.build = subprocess.run([sys.executable, str(ROOT / "port/tools/build.py")], cwd=ROOT,
                                   env=dict(os.environ, TTT_CC="clang"), capture_output=True, text=True, timeout=3600)
        cls.gcc_untouched = (EXE.stat().st_mtime_ns if EXE.exists() else None) == before

    def test_build(self):
        """Every object compiles and links, into build/port-clang/ only."""
        out = self.build.stdout + self.build.stderr
        self.assertEqual(self.build.returncode, 0, out[-3000:])
        m = re.search(r"undefined: (\d+) of (\d+) objects compiled \((\d+) failed\)", out)
        self.assertIsNotNone(m, out[-3000:])
        self.assertEqual((m.group(1), m.group(3)), (m.group(2), "0"))
        self.assertIn("link OK -> build/port-clang/ttt", out)
        self.assertTrue(self.gcc_untouched)

    def test_selftests(self):
        """The self-tests of the platform, the GS / VU1, sound, memory card, the EE interpreter and the settings."""
        self.assertTrue(CLANG_EXE.exists())
        with tempfile.TemporaryDirectory() as d:
            for name in SELFTESTS:
                with self.subTest(name=name):
                    r = run(CLANG_EXE, os.path.join(d, "saves"), timeout=600, TTT_SELFTEST=name)
                    self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                    self.assertRegex(r.stdout, rf"ttt: {name if name != 'mem' else 'memory'} self-test OK")

    @unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_idle_run_same_state(self):
        """The idle run (TTT_FRAMES=2551 TTT_SEED=1) on both builds: the same state line, the reference hash, and the
        same hash for every region (TTT_HASH_REGIONS), so no stale stack byte of either compiler reaches the state."""
        self.assertTrue(CLANG_EXE.exists())
        with tempfile.TemporaryDirectory() as d:
            res = {}
            for name, exe in (("gcc", EXE), ("clang", CLANG_EXE)):
                r = run(exe, os.path.join(d, "saves"), TTT_FRAMES="2551", TTT_SEED="1", TTT_HASH_REGIONS="1")
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                state = re.search(r"ttt: state: .*", r.stdout)
                self.assertIsNotNone(state, r.stdout[-2000:])
                res[name] = (state.group(0), re.findall(r"ttt: hash-region (\S+) (\w+)", r.stdout))
            self.assertIn(f"hash {IDLE_HASH} ", res["gcc"][0])
            self.assertEqual(res["clang"][0], res["gcc"][0])
            self.assertTrue(res["gcc"][1])
            self.assertEqual(res["clang"][1], res["gcc"][1])


if __name__ == "__main__":
    unittest.main()
