"""The PC build's pads (port/src/plat_pad.c: libpad fed once per VBlank, TTT_INPUT_PLAY / TTT_INPUT_RECORD), checked on
the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py). Skipped when the program has
not been built; the replay runs are also skipped without disc data."""

import re
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt

IDLE = "80808080"


def stream(*presses, pads=2):
    """A recording: each press is (vblank, buttons of pad 1, VBlanks held)."""
    lines = [f"ttt-input 1 pads {pads}"]
    for v, b, n in presses:
        lines.append(f"{v} {b:04x} {IDLE} 0000 {IDLE}")
        lines.append(f"{v + n} 0000 {IDLE} 0000 {IDLE}")
    return "\n".join(lines) + "\n"


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class PadSelfTest(unittest.TestCase):
    def test_pad(self):
        """The recording's line format, libpad's read data (kind 0x79, buttons active low, sticks, pressure bytes) and
        the answers that take the game's set-up sequence to a ready DualShock 2 on ports 0 and 1."""
        r = run_ttt(TTT_SELFTEST="pad")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: pad self-test OK", r.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Replay(unittest.TestCase):
    FRAMES = "320"
    START = 0x0800

    def play(self, text, **env):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(text)
            rec = Path(d) / "rec.txt"
            r = run_ttt(TTT_FRAMES=self.FRAMES, TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_INPUT_RECORD=str(rec), **env)
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            return r, rec.read_text()

    def test_headless_runs_have_no_pad(self):
        """Without TTT_INPUT_PLAY a headless run has no pad, as before (same state hashes as earlier tasks)."""
        r = run_ttt(TTT_FRAMES="200", TTT_SEED="1")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:])
        self.assertNotIn("ttt: input:", r.stdout)
        self.assertRegex(r.stdout, r"placeholders hit .*scePadGetState")

    def test_start_at_the_main_menu_replays_identically(self):
        """Start pressed at the main menu (VBlank 200) leaves it for the next mode; two replays end in the same state,
        which differs from the run with the same pads and no button."""
        idle, _ = self.play(stream())
        a, rec_a = self.play(stream((200, self.START, 6)))
        b, rec_b = self.play(stream((200, self.START, 6)))
        self.assertIn("ttt: input: 2 pads, 0 vblanks with a button down (played)", idle.stdout)
        self.assertIn("ttt: input: 2 pads, 6 vblanks with a button down (played)", a.stdout)
        modes_idle = re.search(r"ttt: modes: (.*)", idle.stdout).group(1)
        modes = re.search(r"ttt: modes: (.*)", a.stdout).group(1)
        self.assertNotRegex(modes_idle, r"\b4@")
        self.assertRegex(modes, r"\b3@\d+ 4@2\d\d")  # main menu, then the mode Start chose
        self.assertIsNotNone(state(a))
        self.assertEqual(state(a), state(b))
        self.assertNotEqual(state(a), state(idle))
        self.assertEqual(rec_a, rec_b)

    def test_character_select_runs(self):
        """Start at the main menu reaches the character select (mode 5), which no headless run reached before: it
        used to stop on two drafts of select_results_b.c (a 4-argument call of the 7-argument func_003C9AC0, and
        func_003C68F0 stepping its 0x20-byte font table by sizeof(SelState))."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream((200, self.START, 6)))
            r = run_ttt(TTT_FRAMES="700", TTT_SEED="1", TTT_INPUT_PLAY=str(p))
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertRegex(re.search(r"ttt: modes: (.*)", r.stdout).group(1), r"\b5@")

    def test_a_recording_replays_the_run(self):
        """TTT_INPUT_RECORD writes the latched states: playing that file back gives the same run."""
        a, rec = self.play(stream((200, self.START, 6)))
        self.assertIn("200 0800 80808080 0000 80808080", rec)
        self.assertIn("206 0000 80808080 0000 80808080", rec)
        b, rec2 = self.play(rec)
        self.assertEqual(state(a), state(b))
        self.assertEqual(rec, rec2)


if __name__ == "__main__":
    unittest.main()
