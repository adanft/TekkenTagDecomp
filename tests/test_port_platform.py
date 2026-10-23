"""The PC build's platform layer (port/src/plat_hw.c, plat_sys.c, plat_file.c), checked on the built program
build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py). Skipped when the program has not been built;
the headless runs are also skipped when no disc data is found (TTT_DISC, or iso/ from scripts/setup_disc.py)."""

import os
import re
import subprocess
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


if __name__ == "__main__":
    unittest.main()
