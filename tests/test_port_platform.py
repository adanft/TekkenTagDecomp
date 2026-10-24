"""The PC build's platform layer (port/src/plat_hw.c, plat_sys.c, plat_file.c), checked on the built program
build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py). Skipped when the program has not been built;
the headless runs are also skipped when no disc data is found (TTT_DISC, or iso/ from scripts/setup_disc.py)."""

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "build/port/ttt"


def disc():
    """The disc the port reads: TTT_DISC (a directory or an .iso), else the repository's iso/ directory."""
    d = os.environ.get("TTT_DISC")
    if d:
        return Path(d) if Path(d).exists() else None
    return ROOT / "iso" if (ROOT / "iso/TEKKEN.BIN").exists() else None


def run_ttt(timeout=120, **env):
    e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
    if disc() is not None:
        e["TTT_DISC"] = str(disc())
    e.update(env)
    return subprocess.run([str(EXE)], env=e, capture_output=True, text=True, timeout=timeout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class SelfTest(unittest.TestCase):
    def check(self, name):
        r = run_ttt(TTT_SELFTEST=name)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn(f"ttt: {name} self-test OK", r.stdout)

    def test_hw(self):
        """DMA completion (STR cleared, scratchpad transfers copied, DMAC handler run), the VBlank counter and its
        interrupt handler, Timer 0 counting at BUSCLK/256, interrupts held while disabled."""
        self.check("hw")

    def test_rpc(self):
        """SIF RPC dispatch: bind answers at once, calls of unknown servers return harmless replies, sceSdRemote's
        status and file commands, module loads and the IOP reboot succeed."""
        self.check("rpc")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class HeadlessBoot(unittest.TestCase):
    FRAMES = 600

    def test_boot_reaches_the_title_loop(self):
        """Init runs, the main loop runs one update per VBlank, and the boot mode hands over to the title."""
        r = run_ttt(TTT_FRAMES=str(self.FRAMES), TTT_SEED="1")
        out = r.stdout + r.stderr
        self.assertEqual(r.returncode, 0, out[-3000:])
        self.assertNotIn("not implemented yet", out)
        m = re.search(r"ttt: headless: (\d+) vblanks, (\d+) frames", out)
        self.assertIsNotNone(m, out[-3000:])
        self.assertEqual(int(m.group(1)), self.FRAMES)
        self.assertGreater(int(m.group(2)), self.FRAMES // 4)
        modes = re.search(r"ttt: modes: (.*)", out).group(1)
        self.assertIn("0@", modes)  # Mode_Boot
        self.assertRegex(modes, r"\b2@")  # Mode_Title reached

    def test_pinned_seed_is_deterministic(self):
        """With TTT_SEED and the headless clock two runs end in the same state."""
        a = run_ttt(TTT_FRAMES="240", TTT_SEED="0x1234")
        b = run_ttt(TTT_FRAMES="240", TTT_SEED="0x1234")
        state = [re.search(r"ttt: state: .*", r.stdout) for r in (a, b)]
        self.assertTrue(all(state), a.stdout[-2000:])
        self.assertEqual(state[0].group(0), state[1].group(0))
        self.assertIn("seed 0x1234", state[0].group(0))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class ProgramElsewhere(unittest.TestCase):
    """The program copied or linked under a long directory path (T32 batch 1: a copy crashed in func_00345410 at
    boot). The cause was not the path's length but the default disc, looked up beside the program: with no disc the
    reads failed silently and the boot ran over memory they never filled. Now a missing disc stops the program at
    the first read with exit code 2, and a copy finds TTT_DISC or iso/ beside it."""

    FRAMES = "200"

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-longpath-")
        deep = Path(self.tmp.name)
        for i in range(12):
            deep = deep / f"a-rather-long-directory-name-number-{i:02d}"
        deep.mkdir(parents=True)
        self.dir = deep
        self.copy = deep / "ttt"
        shutil.copy2(EXE, self.copy)

    def tearDown(self):
        self.tmp.cleanup()

    def run_at(self, exe, disc_dir=None, **env):
        e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
        if disc_dir is not None:
            e["TTT_DISC"] = str(disc_dir)
        e.update(env)
        return subprocess.run([str(exe)], env=e, cwd=self.dir, capture_output=True, text=True, timeout=120)

    def state(self, r):
        self.assertEqual(r.returncode, 0, (r.stdout + r.stderr)[-3000:])
        m = re.search(r"ttt: state: .*", r.stdout)
        self.assertIsNotNone(m, r.stdout[-3000:])
        return m.group(0)

    def test_no_disc_stops_cleanly(self):
        """Without a disc anywhere the program says so on stderr and exits with code 2 (no segfault)."""
        r = self.run_at(self.copy, TTT_FRAMES=self.FRAMES)
        self.assertEqual(r.returncode, 2, (r.stdout + r.stderr)[-3000:])
        self.assertIn("ttt: no disc", r.stderr)

    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_copy_runs_like_the_original(self):
        """A copy under a long path, given the disc by TTT_DISC or by iso/ beside it, and a symbolic link to the
        built program there (which finds the repository's disc), end in the state of the original."""
        want = self.state(run_ttt(TTT_FRAMES=self.FRAMES, TTT_SEED="1"))
        self.assertEqual(self.state(self.run_at(self.copy, disc(), TTT_FRAMES=self.FRAMES, TTT_SEED="1")), want)
        (self.dir / "iso").symlink_to(disc().resolve(), target_is_directory=disc().is_dir())
        if disc().is_dir():
            self.assertEqual(self.state(self.run_at(self.copy, TTT_FRAMES=self.FRAMES, TTT_SEED="1")), want)
        if not os.environ.get("TTT_DISC"):
            link = self.dir / "ttt-link"
            link.symlink_to(EXE)
            self.assertEqual(self.state(self.run_at(link, TTT_FRAMES=self.FRAMES, TTT_SEED="1")), want)


if __name__ == "__main__":
    unittest.main()
