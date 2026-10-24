"""The PC build's GL back end and GS threads (T30 batch 4), checked on the built program build/port/ttt (made from your
own SLUS_200.01 by port/tools/build.py): the GL back end's batching and hazard rules on made-up primitives
(TTT_SELFTEST=gl, no GL context needed), the two GS threads against one and none, and the TTT_TIME_FROM measurement.
Skipped when the program has not been built; the game runs are also skipped without disc data."""

import re
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class GlSelfTest(unittest.TestCase):
    def test_gl_rules(self):
        """The alpha test's outcome over a primitive's alpha range (CLUT rows, TEXA, TFX / TCC, Gouraud), the depth write
        a repeated pass may share, the texels a UV sprite or triangle reads (nearest / bilinear, the sprite's right edge,
        CLAMP / REPEAT) and the CLUT halves a load or read touches: pure rules of gs_gl.c, no GL context."""
        r = run_ttt(TTT_SELFTEST="gl", TTT_FRAMES="1")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertIn("ttt: gl self-test OK", r.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class GsThreads(unittest.TestCase):
    def test_two_gs_threads_change_nothing(self):
        """VIF1 / VU1 and the GIF / GS front end on two threads (TTT_GS_ASYNC=2, the default with the GL back end) give the same state and
        the same pictures as one GS thread (1) and none (0), through the title, the menu and the attract cycle's
        loading."""
        with tempfile.TemporaryDirectory() as d:
            runs = []
            for on in ("0", "1", "2"):
                out = Path(d) / on
                out.mkdir()
                r = run_ttt(TTT_FRAMES="760", TTT_SEED="1", TTT_GS_ASYNC=on, TTT_DUMP=str(out), TTT_DUMP_EVERY="95")
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                runs.append((r, out))
            self.assertIsNotNone(state(runs[0][0]))
            names = sorted(p.name for p in runs[0][1].iterdir())
            self.assertEqual(len(names), 8)
            for r, out in runs[1:]:
                self.assertEqual(state(r), state(runs[0][0]))
                self.assertEqual(names, sorted(p.name for p in out.iterdir()))
                for name in names:
                    self.assertEqual((out / name).read_bytes(), (runs[0][1] / name).read_bytes(), name)

    def test_time_from(self):
        """TTT_TIME_FROM=<vblank> reports the host time from that VBlank to the end of the run, the GS path drained at
        both ends, and changes nothing else."""
        timed = run_ttt(TTT_FRAMES="200", TTT_SEED="1", TTT_TIME_FROM="100")
        plain = run_ttt(TTT_FRAMES="200", TTT_SEED="1")
        self.assertEqual(timed.returncode, 0, timed.stdout[-2000:] + timed.stderr[-2000:])
        self.assertRegex(timed.stdout, r"ttt: time: vblanks 100-200 in \d+ ms")
        self.assertNotIn("ttt: time:", plain.stdout)
        self.assertEqual(state(timed), state(plain))


if __name__ == "__main__":
    unittest.main()
