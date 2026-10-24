"""The PC build's movies (T33: movie.c's PC player, port/src/plat_movie.c, third_party/pl_mpeg2, the movie audio of
plat_bgm.c), checked on the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py) and
your own disc. The game's intro movie (movie 0) starts at VBlank 176 of the boot. Frames and sounds are written to
temporary directories only and deleted: nothing decoded from the game's data is kept."""

import hashlib
import struct
import tempfile
import unittest
import wave
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt
from test_port_input import peek, stream
from test_port_gs import read_png

START = 0x0800
SKIPPED = "5F6CC4"  # D_005F6CC4 (low half): 1 when the movie was skipped (or could not start)
SAMPLES_PER_VBLANK = 48000 * 1001 // 60000


def grey(path):
    """(mean, spread) of a dumped frame's brightness: (r + g + b) / 3 over every pixel."""
    w, h, rows = read_png(path)
    vals = [(r[i] + r[i + 1] + r[i + 2]) // 3 for r in rows for i in range(0, w * 3, 3 * 7)]
    mean = sum(vals) / len(vals)
    return mean, max(vals) - min(vals)


def peak(path, first_vblank, last_vblank):
    """The largest sample magnitude of a TTT_WAV file between two VBlanks."""
    with wave.open(str(path)) as w:
        w.setpos(first_vblank * SAMPLES_PER_VBLANK)
        data = w.readframes((last_vblank - first_vblank) * SAMPLES_PER_VBLANK)
    return max(abs(v) for v in struct.unpack(f"<{len(data) // 2}h", data))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Movies(unittest.TestCase):
    def play(self, out, frames, presses=(), movies="1", **env):
        p = Path(out) / "in.txt"
        p.write_text(stream(*presses))
        r = run_ttt(timeout=300, TTT_MOVIES=movies, TTT_FRAMES=str(frames), TTT_SEED="1", TTT_INPUT_PLAY=str(p),
                    TTT_DUMP=str(out), TTT_WAV=str(Path(out) / "out.wav"), TTT_PEEK=SKIPPED, **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return r

    def test_intro_plays_deterministically(self):
        """The boot plays the intro: the dumped frames at VBlanks 400 and 600 are pictures (not blank, not equal),
        the movie's audio sounds while they show, the game waits in the title mode (2) for the whole run, and a
        second run gives the same frames and the same WAV byte for byte."""
        digests = []
        for _ in range(2):
            with tempfile.TemporaryDirectory(prefix="ttt-movie-") as out:
                r = self.play(out, 601, TTT_DUMP_EVERY="200")
                self.assertRegex(r.stdout, r"ttt: modes: 0@1 2@174\n")
                self.assertEqual(peek(r, SKIPPED)[0] & 0xFFFF, 0)
                a, b = Path(out) / "frame_00400.png", Path(out) / "frame_00600.png"
                for f in (a, b):
                    mean, spread = grey(f)
                    self.assertGreater(mean, 10, f)
                    self.assertGreater(spread, 60, f)
                self.assertNotEqual(a.read_bytes(), b.read_bytes())
                self.assertGreater(peak(Path(out) / "out.wav", 400, 600), 2000)
                h = hashlib.sha256()
                for f in sorted(Path(out).glob("frame_*.png")) + [Path(out) / "out.wav"]:
                    h.update(f.name.encode() + f.read_bytes())
                digests.append(h.hexdigest())
        self.assertEqual(digests[0], digests[1])

    def test_start_skips(self):
        """Start (from the 11th picture on) skips the intro, as func_0039CAC8 does: the skipped flag is set, the
        audio stops, and the title goes on to the main menu (mode 3)."""
        with tempfile.TemporaryDirectory(prefix="ttt-movie-") as out:
            r = self.play(out, 450, ((260, START, 4),), TTT_DUMP_EVERY="1000")
            self.assertEqual(peek(r, SKIPPED)[0] & 0xFFFF, 1)
            self.assertRegex(r.stdout, r"ttt: modes: 0@1 2@174 1@\d+ 3@\d+")
            self.assertLess(peak(Path(out) / "out.wav", 330, 450), 2000)

    def test_off(self):
        """TTT_MOVIES=0 skips the movies as before T33 (the boot reaches the main menu at VBlank 183)."""
        with tempfile.TemporaryDirectory(prefix="ttt-movie-") as out:
            r = self.play(out, 200, movies="0", TTT_DUMP_EVERY="1000")
            self.assertRegex(r.stdout, r"ttt: modes: 0@1 2@174 1@178 3@183")
            self.assertIn("movie (skipped)", r.stdout)


if __name__ == "__main__":
    unittest.main()
