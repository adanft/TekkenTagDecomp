"""The PC build's pads (port/src/plat_pad.c: libpad fed once per VBlank, TTT_INPUT_PLAY / TTT_INPUT_RECORD), checked on
the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py). Skipped when the program has
not been built; the replay runs are also skipped without disc data."""

import os
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


def peek(r, addr):
    """The words TTT_PEEK printed for addr (hex string as passed) at the end of the run."""
    m = re.search(rf"ttt: peek {int(addr, 16):08X}:((?: [0-9A-F]{{8}})+)", r.stdout)
    return [int(w, 16) for w in m.group(1).split()] if m else None


START, RIGHT, DOWN, SQUARE, TRIANGLE, CROSS = 0x0800, 0x2000, 0x4000, 0x0080, 0x0010, 0x0040
# Start at the main menu (ARCADE), then at the character select: right, down, square (Yoshimitsu), right, square
# (Nina, the tag partner). The stage loads and stage 1 is fought (mode 4, sub-state 8) by VBlank 830.
TO_THE_FIGHT = ((200, START, 6), (330, RIGHT, 4), (360, DOWN, 4), (400, SQUARE, 4), (440, RIGHT, 4),
                (470, SQUARE, 4))
# In round 1: forward for 100 VBlanks, then left punch, right punch, left kick.
IN_THE_FIGHT = ((1150, RIGHT, 100), (1300, SQUARE, 4), (1340, TRIANGLE, 4), (1380, CROSS, 4))
SEL_SIDE0 = "440D08"  # SelState side 0 (D_00440CE8 + 0x20): state, cursor cell, ..., character at +0x10
MODE = "5F5C20"  # gMode (s16), gSubMode (s16)
FIGHTER0 = "879644"  # gFighters[0].unk4 / unk8 / unkC: the position
FIGHTER1 = "884EE4"  # gFighters[1], the same


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


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class CharacterSelect(unittest.TestCase):
    def run_select(self, *presses):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*presses))
            r = run_ttt(TTT_FRAMES="345", TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_PEEK=f"{SEL_SIDE0}:5")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return peek(r, SEL_SIDE0)

    def test_the_cursor_moves(self):
        """Right on the d-pad moves side 1's cursor to the next cell and the side keeps choosing (state 1). Before T34
        b2 the m2c draft of func_003CA470 read the pad words as bytes: the cursor stayed and a press picked at once."""
        idle = self.run_select((200, START, 6))
        moved = self.run_select((200, START, 6), (330, RIGHT, 4))
        self.assertEqual(idle[0], 1)
        self.assertEqual(moved[0], 1)
        self.assertEqual(moved[1], idle[1] + 1)
        self.assertNotEqual(moved[4], idle[4])  # the character under the cursor


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (about three minutes): TTT_SLOW_TESTS=1")
class Fight(unittest.TestCase):
    FRAMES = "1450"

    def play(self, *presses):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*presses))
            r = run_ttt(timeout=900, TTT_FRAMES=self.FRAMES, TTT_SEED="1", TTT_INPUT_PLAY=str(p),
                        TTT_PEEK=f"{MODE},{FIGHTER0}:3,{FIGHTER1}:3")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return r

    def test_arcade_to_the_fight(self):
        """The pads take the game from the main menu through the character select (two members) and the stage load
        to round 1 of the fight (mode 4, sub-state 8); the fighters' moves follow the pad (forward, punches, a kick),
        and the same inputs replay the same fight."""
        a = self.play(*TO_THE_FIGHT, *IN_THE_FIGHT)
        b = self.play(*TO_THE_FIGHT, *IN_THE_FIGHT)
        idle = self.play(*TO_THE_FIGHT)
        modes = re.search(r"ttt: modes: (.*)", a.stdout).group(1)
        self.assertRegex(modes, r"\b3@\d+ 4@\d+ 5@\d+ 4@\d+$")  # main menu, fight set-up, select, fight
        self.assertEqual(peek(a, MODE), [0x00080004])
        self.assertEqual(peek(idle, MODE), [0x00080004])
        self.assertEqual(state(a), state(b))
        self.assertEqual(peek(a, FIGHTER0), peek(b, FIGHTER0))
        self.assertNotEqual(peek(a, FIGHTER0), peek(idle, FIGHTER0))
        self.assertNotEqual(state(a), state(idle))

    def test_stages_of_other_seeds(self):
        """The boot seed picks the stage: seed 3 the lanes stage (func_003D5B58 hung in its placement loop, then
        func_003D5E80 crashed in Model_QueueDraw), 9 the stage whose func_003D43A8 got its pointer as a float, 32 a
        stage with func_003E6DA0's particles (func_003E6B10 wrote at EE address 0). Each reaches round 1."""
        for seed in ("3", "9", "32"):
            with self.subTest(seed=seed), tempfile.TemporaryDirectory() as d:
                p = Path(d) / "in.txt"
                p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
                r = run_ttt(timeout=900, TTT_FRAMES=self.FRAMES, TTT_SEED=seed, TTT_INPUT_PLAY=str(p),
                            TTT_PEEK=MODE)
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                self.assertEqual(peek(r, MODE), [0x00080004])


if __name__ == "__main__":
    unittest.main()
