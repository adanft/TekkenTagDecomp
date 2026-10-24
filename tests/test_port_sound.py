"""The PC build's sound (port/src/spu2.c, plat_snd.c, plat_bgm.c: RSPU2DRV's commands on a software SPU2), checked on
the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py). Skipped when the program has
not been built; the WAV runs are also skipped without disc data. The WAV files are written to a temporary directory
and deleted: audio made from the game's data never goes into the repository."""

import hashlib
import re
import struct
import tempfile
import unittest
from pathlib import Path

from test_port_input import TO_THE_FIGHT, stream
from test_port_platform import EXE, disc, run_ttt

RATE = 48000


def frames_for(vblanks):
    """Stereo frames mixed for that many VBlanks: 48000 * 1001 / 60000 = 800.8 a VBlank."""
    return vblanks * 4004 // 5


def wav_frames(data):
    """The 16-bit stereo frames of a WAV as (left, right) tuples, after checking its header."""
    assert data[:4] == b"RIFF" and data[8:16] == b"WAVEfmt ", data[:16]
    fmt, channels, rate, _, align, bits = struct.unpack("<HHIIHH", data[20:36])
    assert (fmt, channels, rate, align, bits) == (1, 2, RATE, 4, 16)
    assert data[36:40] == b"data"
    n = struct.unpack("<I", data[40:44])[0]
    assert n == len(data) - 44
    return struct.iter_unpack("<hh", data[44:])


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class SpuSelfTest(unittest.TestCase):
    def test_spu(self):
        """Synthetic ADPCM blocks: the decoder (filters 0-2, shifts, clamping, history), the voices' loop flags (end
        without repeat stops the voice with key status 3 and ENDX, end + repeat loops to the loop-start block), pitch,
        the ADSR phases and key off, and a deterministic, non-silent mix of both cores."""
        r = run_ttt(TTT_SELFTEST="spu")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: spu self-test OK", r.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class WavDump(unittest.TestCase):
    """A headless run with TTT_WAV through the title, the main menu and the character select (the replay of
    test_port_input: Start, cursor moves, two picks), with its menu sound effects and the select screen's BGM."""

    FRAMES = 520

    def dump(self, d, name):
        p = Path(d) / "in.txt"
        p.write_text(stream(*TO_THE_FIGHT))
        wav = Path(d) / name
        r = run_ttt(TTT_FRAMES=str(self.FRAMES), TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_WAV=str(wav))
        self.assertEqual(r.returncode, 0, r.stdout[-3000:] + r.stderr)
        self.assertIn(f"ttt: sound: {frames_for(self.FRAMES)} frames written to TTT_WAV", r.stdout)
        return wav.read_bytes(), r

    def test_title_and_menu_sound(self):
        """Not silent, the same bytes on two runs (the mix follows the VBlanks, not the host clock), and the sound
        starts with the title (the first effect at VBlank 169)."""
        with tempfile.TemporaryDirectory() as d:
            a, ra = self.dump(d, "a.wav")
            b, rb = self.dump(d, "b.wav")
        self.assertEqual(hashlib.sha1(a).hexdigest(), hashlib.sha1(b).hexdigest())
        self.assertEqual(re.search(r"ttt: state: .*", ra.stdout).group(0), re.search(r"ttt: state: .*", rb.stdout).group(0))
        frames = list(wav_frames(a))
        self.assertEqual(len(frames), frames_for(self.FRAMES))
        loud = [i for i, (l, r) in enumerate(frames) if abs(l) > 1000 or abs(r) > 1000]
        self.assertGreater(len(loud), RATE)  # more than a second of clearly audible sound
        self.assertEqual([f for f in frames[: frames_for(160)] if f != (0, 0)], [])  # boot: silence
        self.assertTrue(any(f != (0, 0) for f in frames[frames_for(168): frames_for(175)]))  # the title's effect
        # the select screen's BGM (track 0, from VBlank 225): both channels sound, unlike the mono effects
        select = frames[frames_for(250): frames_for(320)]
        self.assertTrue(any(l != r for l, r in select))
        self.assertGreater(sum(1 for f in select if f != (0, 0)), len(select) * 9 // 10)


if __name__ == "__main__":
    unittest.main()
