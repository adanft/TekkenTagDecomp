"""The PC build's state (port/src/plat_state.c, T27): the gameplay hash, save / load at a VBlank boundary and the
synctest, checked on the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py). Skipped
when the program has not been built; the runs are also skipped without disc data. States, pictures and dumps go to
temporary directories only (they hold the game's memory)."""

import os
import re
import tempfile
import unittest
from pathlib import Path

from test_port_input import IN_THE_FIGHT, TO_THE_FIGHT, stream
from test_port_platform import EXE, disc, run_ttt
from test_port_window import SDL32


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


def modes(r):
    m = re.search(r"ttt: modes: (.*)", r.stdout)
    return m.group(1) if m else None


def pictures(d):
    return {p.name: p.read_bytes() for p in sorted(Path(d).glob("frame_*.png"))}


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class StateSelfTest(unittest.TestCase):
    def test_state(self):
        """Host addresses in the game's memory hash as one constant (the two .sbss callbacks of gotcha 20 hash the
        same at other host addresses, other data does not), and a boundary that saves, a change, a boundary that loads
        bring the memory back and continue after the saving boundary."""
        r = run_ttt(TTT_SELFTEST="state")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: state self-test OK", r.stdout)
        # page tracking (F07): tested where the kernel offers it (Linux 6.7: asynchronous write protection)
        self.assertRegex(r.stdout, r"ttt: state: page tracking(: \d+ pages tracked| not offered here)")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Hash(unittest.TestCase):
    def test_hash_every(self):
        """TTT_HASH_EVERY prints the hash at every n-th VBlank boundary; printing it changes nothing."""
        a = run_ttt(TTT_FRAMES="300", TTT_SEED="1", TTT_HASH_EVERY="100")
        b = run_ttt(TTT_FRAMES="300", TTT_SEED="1")
        self.assertEqual(a.returncode, 0, a.stdout[-2000:] + a.stderr[-2000:])
        lines = re.findall(r"ttt: hash (\d+) ([0-9a-f]{16})", a.stdout)
        self.assertEqual([v for v, _ in lines], ["100", "200"])
        self.assertNotEqual(lines[0][1], lines[1][1])
        self.assertRegex(state(a), r"hash 0x[0-9a-f]{16}")
        self.assertEqual(state(a), state(b))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class SaveLoad(unittest.TestCase):
    def save_and_continue(self, at, end, every, **env):
        """A run to `end` that saves at `at`, and a fresh process that loads the state and runs to `end`: their
        pictures after `at`, modes and state lines."""
        with tempfile.TemporaryDirectory(prefix="ttt-state-") as d:
            d = Path(d)
            (d / "a").mkdir()
            (d / "b").mkdir()
            f = d / "state.bin"
            a = run_ttt(timeout=900, TTT_FRAMES=str(end), TTT_SEED="1", TTT_DUMP=str(d / "a"), TTT_DUMP_EVERY=str(every),
                        TTT_SAVESTATE=f"{at}:{f}", **env)
            self.assertEqual(a.returncode, 0, a.stdout[-2000:] + a.stderr[-2000:])
            self.assertRegex(a.stdout, rf"ttt: savestate: vblank {at} to ")
            b = run_ttt(timeout=900, TTT_FRAMES=str(end), TTT_SEED="1", TTT_DUMP=str(d / "b"), TTT_DUMP_EVERY=str(every),
                        TTT_LOADSTATE=str(f), **env)
            self.assertEqual(b.returncode, 0, b.stdout[-2000:] + b.stderr[-2000:])
            self.assertRegex(b.stdout, rf"ttt: loadstate: vblank {at} from ")
            pa, pb = pictures(d / "a"), pictures(d / "b")
            pa = {k: v for k, v in pa.items() if int(k[6:11]) > at}
        return a, b, pa, pb

    def test_load_in_a_fresh_process(self):
        """A state saved at VBlank 700 (the attract cycle's loading mode starts at 705) and loaded by another process
        continues to VBlank 820 with the same pictures, modes and state as the run that saved it."""
        a, b, pa, pb = self.save_and_continue(700, 820, 40)
        self.assertEqual(sorted(pa), ["frame_00720.png", "frame_00760.png", "frame_00800.png"])
        self.assertEqual(pa, pb)
        self.assertEqual(modes(a), modes(b))
        self.assertEqual(state(a), state(b))

    def test_refuses_what_is_not_a_state(self):
        """A file that is not a state of this build stops the program (exit code 2) at the first poll."""
        with tempfile.TemporaryDirectory(prefix="ttt-state-") as d:
            f = Path(d) / "junk.bin"
            f.write_bytes(b"not a state" * 100)
            r = run_ttt(TTT_FRAMES="100", TTT_LOADSTATE=str(f))
        self.assertEqual(r.returncode, 2, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertIn("ttt: TTT_LOADSTATE", r.stderr)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Synctest(unittest.TestCase):
    def test_attract_load(self):
        """TTT_SYNCTEST over the attract cycle's loading mode (VBlanks 650-800): every pair of passes from a loaded
        checkpoint ends in the same state, and the run ends where a run without the synctest ends."""
        a = run_ttt(timeout=600, TTT_FRAMES="800", TTT_SEED="1", TTT_SYNCTEST="2", TTT_SYNCTEST_FROM="650")
        b = run_ttt(timeout=600, TTT_FRAMES="800", TTT_SEED="1")
        self.assertEqual(a.returncode, 0, a.stdout[-2000:] + a.stderr[-2000:])
        m = re.search(r"ttt: synctest: (\d+) checks of 2 vblanks, (\d+) diverged, (\d+) skipped", a.stdout)
        self.assertIsNotNone(m, a.stdout[-2000:])
        self.assertGreaterEqual(int(m.group(1)), 70)
        self.assertEqual((m.group(2), m.group(3)), ("0", "0"))
        self.assertEqual(state(a), state(b))


TRACK = re.compile(r"ttt: state-track: ([^;]+); (\d+) pages tracked, .*?(\d+) of (\d+) saves and (\d+) of (\d+) loads by "
                   r"page .*; (\d+) checked, (\d+) wrong")
SYNC = re.compile(r"ttt: synctest: (\d+) checks of \d+ vblanks, (\d+) diverged, (\d+) skipped; state \d+ bytes, "
                  r"save (\d+) us, load (\d+) us")
ROLLBACK = re.compile(r"ttt: rollback: (\d+) rollbacks of (\d+) vblanks( undrawn)?, (\d+) checked, (\d+) diverged, "
                      r"(\d+) skipped")


def tracking(test, r):
    """The state-track report line's groups; skips the test where the kernel offers no page tracking."""
    m = TRACK.search(r.stdout)
    test.assertIsNotNone(m, r.stdout[-2000:])
    if m.group(1) != "on":
        test.skipTest("no page tracking here: " + m.group(1))
    return m


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Tracking(unittest.TestCase):
    """F07 (docs/port/determinism.md): page tracking. Saves into and loads from buffers this process saved copy only
    the pages written since; TTT_STATE_TRACK_CHECK compares every such save and load with what a whole copy gives."""

    def test_tracked_saves_and_loads_equal_whole_copies(self):
        """The synctest over the attract cycle's loading (VBlanks 650-800) with page tracking (its default) and every
        tracked save and load checked: none differs from a whole copy, nearly all are by page, and the synctest's
        result and the run's end are those of whole copies (TTT_STATE_TRACK=0)."""
        a = run_ttt(timeout=600, TTT_FRAMES="800", TTT_SEED="1", TTT_SYNCTEST="2", TTT_SYNCTEST_FROM="650",
                    TTT_STATE_TRACK_CHECK="1")
        b = run_ttt(timeout=600, TTT_FRAMES="800", TTT_SEED="1", TTT_SYNCTEST="2", TTT_SYNCTEST_FROM="650",
                    TTT_STATE_TRACK="0")
        for r in (a, b):
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        m = tracking(self, a)
        inc_saves, saves, inc_loads, loads, checked, wrong = (int(g) for g in m.groups()[2:])
        self.assertEqual(wrong, 0, a.stdout[-2000:])
        self.assertEqual(checked, inc_saves + inc_loads)
        self.assertGreaterEqual(inc_saves, saves - 3)  # the three synctest buffers' first saves are whole
        self.assertEqual(inc_loads, loads)
        self.assertNotIn("ttt: state-track:", b.stdout)
        sa, sb = SYNC.search(a.stdout), SYNC.search(b.stdout)
        self.assertEqual(sa.groups()[:3], sb.groups()[:3])
        self.assertEqual(sa.groups()[1:3], ("0", "0"))
        self.assertEqual(state(a), state(b))

    def test_rollback(self):
        """TTT_ROLLBACK=4 over VBlanks 700-800: at every VBlank the state of 4 VBlanks before is loaded and those
        VBlanks run again (saved by page), drawn or not (TTT_ROLLBACK_DRAW=0: the GS's side is left out of the
        states); each rollback comes back to the state saved the first time, and the run ends as one without."""
        plain = run_ttt(timeout=600, TTT_FRAMES="800", TTT_SEED="1")
        for draw in ("1", "0"):
            r = run_ttt(timeout=900, TTT_FRAMES="800", TTT_SEED="1", TTT_ROLLBACK="4", TTT_ROLLBACK_FROM="700",
                        TTT_ROLLBACK_CHECK="1", TTT_ROLLBACK_DRAW=draw, TTT_STATE_TRACK_CHECK="1")
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            m = ROLLBACK.search(r.stdout)
            self.assertIsNotNone(m, r.stdout[-2000:])
            self.assertEqual(m.group(3) is not None, draw == "0")
            self.assertGreaterEqual(int(m.group(1)), 95)
            self.assertEqual(int(m.group(4)), int(m.group(1)))
            self.assertEqual(m.group(5, 6), ("0", "0"))
            self.assertEqual(tracking(self, r).group(8), "0")
            self.assertEqual(state(r), state(plain))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
class Gl(unittest.TestCase):
    """F07: save, load and the synctest with the GL back end (a hidden window, SDL's offscreen driver): a save first
    has the window thread make GS memory whole on the CPU and drop its targets, a load has it take the loaded memory."""

    def run_gl(self, **env):
        r = run_ttt(timeout=900, TTT_SEED="1", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER="gl", TTT_AUDIO="0",
                    TTT_VSYNC="0", SDL_VIDEO_DRIVER="offscreen", **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        if "OpenGL renderer" not in r.stdout:
            self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
        return r

    def test_synctest(self):
        """The synctest over VBlanks 650-800 with GL: every pair of passes ends in the same state, GS memory
        included (read back from the GPU), and the run ends where the software run without the synctest ends."""
        a = self.run_gl(TTT_FRAMES="800", TTT_SYNCTEST="2", TTT_SYNCTEST_FROM="650", TTT_STATE_TRACK_CHECK="1")
        b = run_ttt(timeout=600, TTT_FRAMES="800", TTT_SEED="1")
        m = SYNC.search(a.stdout)
        self.assertIsNotNone(m, a.stdout[-2000:])
        self.assertGreaterEqual(int(m.group(1)), 70)
        self.assertEqual(m.group(2, 3), ("0", "0"))
        g = re.search(r"ttt: gl: state: (\d+) saves, (\d+) loads", a.stdout)
        self.assertIsNotNone(g, a.stdout[-2000:])
        self.assertGreaterEqual(int(g.group(2)), int(m.group(1)))
        self.assertEqual(tracking(self, a).group(8), "0")
        self.assertEqual(state(a), state(b))

    def test_load_in_a_fresh_process(self):
        """A state saved with GL at VBlank 700 and loaded by a fresh GL process (whose window thread has drawn nothing
        yet) continues with the same pictures, modes and state; the run that saved draws what a run without the
        save draws."""
        with tempfile.TemporaryDirectory(prefix="ttt-state-") as d:
            d = Path(d)
            for k in ("a", "b", "c"):
                (d / k).mkdir()
            f = d / "state.bin"
            dump = dict(TTT_FRAMES="820", TTT_DUMP_EVERY="40")
            a = self.run_gl(TTT_DUMP=str(d / "a"), TTT_SAVESTATE=f"700:{f}", **dump)
            self.assertRegex(a.stdout, r"ttt: savestate: vblank 700 to ")
            b = self.run_gl(TTT_DUMP=str(d / "b"), TTT_LOADSTATE=str(f), **dump)
            self.assertRegex(b.stdout, r"ttt: loadstate: vblank 700 from ")
            c = self.run_gl(TTT_DUMP=str(d / "c"), TTT_DUMP_FROM="720", **dump)
            pa, pb, pc = pictures(d / "a"), pictures(d / "b"), pictures(d / "c")
            pa = {k: v for k, v in pa.items() if int(k[6:11]) > 700}
        self.assertEqual(sorted(pb), ["frame_00720.png", "frame_00760.png", "frame_00800.png"])
        self.assertEqual(pa, pb)
        self.assertEqual(pa, pc)
        self.assertEqual(modes(a), modes(b))
        self.assertEqual(state(a), state(b))
        self.assertEqual(state(a), state(c))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (several minutes): TTT_SLOW_TESTS=1")
class Fight(unittest.TestCase):
    def run_fight(self, **env):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
            r = run_ttt(timeout=1800, TTT_SEED="1", TTT_INPUT_PLAY=str(p), **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return r

    def test_synctest_in_the_fight(self):
        """The scripted fight (round 1 from VBlank 830, inputs from 1150) under TTT_SYNCTEST=1 for 400 VBlanks: no
        divergence, and the same end as the fight without it (one raster thread layout or another)."""
        a = self.run_fight(TTT_FRAMES="1250", TTT_SYNCTEST="1", TTT_SYNCTEST_FROM="850", TTT_GS_THREADS="0",
                           TTT_GS_ASYNC="0")
        b = self.run_fight(TTT_FRAMES="1250")
        # every VBlank from 851 to 1249 checked (the report at 1250 comes first)
        self.assertRegex(a.stdout, r"ttt: synctest: 399 checks of 1 vblanks, 0 diverged, 0 skipped")
        self.assertEqual(state(a), state(b))

    def test_load_in_the_fight(self):
        """A state saved in the fight while the pads play (VBlank 1200) continues in a fresh process with the same
        pictures and state."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
            a, b, pa, pb = SaveLoad.save_and_continue(self, 1200, 1350, 50, TTT_INPUT_PLAY=str(p))
        self.assertEqual(sorted(pa), ["frame_01250.png", "frame_01300.png", "frame_01350.png"])
        self.assertEqual(pa, pb)
        self.assertEqual(state(a), state(b))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class CheatUnlock(unittest.TestCase):
    """T40 (docs/port/cheats.md): cheats.unlock_all, off by default, raises the game's own unlock flags in the save
    block D_00439C60 (the step counter to the unlock table's last threshold, every theater character's clear bit,
    the GALLERY / TEKKEN BOWL / THEATER bytes) and the game's Unlock_ApplyAll at boot rebuilds the selectable
    characters from them: more characters than a fresh card has. It changes the game's state, so the hash differs;
    with it off nothing is written."""

    UNLOCKS = re.compile(r"ttt: unlocks: counter (\d+), clear bits 0x([0-9a-f]+), gallery (\d+), bowl (\d+), "
                         r"theater (\d+), (\d+) selectable characters; cheat (on|off)")

    def test_unlock_everything(self):
        off = run_ttt(TTT_FRAMES="300", TTT_SEED="1")
        on = run_ttt(TTT_FRAMES="300", TTT_SEED="1", TTT_CHEAT_UNLOCK="1")
        for r in (off, on):
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        a, b = self.UNLOCKS.search(off.stdout), self.UNLOCKS.search(on.stdout)
        self.assertIsNotNone(a, off.stdout[-2000:])
        self.assertIsNotNone(b, on.stdout[-2000:])
        self.assertEqual(a.groups()[:5] + (a.group(7),), ("0", "0" * 16, "0", "0", "0", "off"))
        self.assertGreater(int(b.group(1)), 0)
        self.assertNotEqual(int(b.group(2), 16), 0)
        self.assertEqual(b.groups()[2:5] + (b.group(7),), ("1", "1", "1", "on"))
        self.assertGreater(int(b.group(6)), int(a.group(6)))
        h = re.compile(r"hash (0x[0-9a-f]+)")
        self.assertNotEqual(h.search(on.stdout).group(1), h.search(off.stdout).group(1))


if __name__ == "__main__":
    unittest.main()
