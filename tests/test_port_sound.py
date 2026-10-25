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
        """Synthetic ADPCM blocks: the decoder (filters 0-2, shifts, clamping, history), the Gaussian interpolation
        (reference values, a voice against the decoded stream), SpuSt's two-half buffer, the voices' loop flags (end
        without repeat stops the voice with key status 3 and ENDX, end + repeat loops to the loop-start block), pitch,
        the ADSR phases and key off, and a deterministic, non-silent mix of both cores."""
        r = run_ttt(TTT_SELFTEST="spu")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: spu self-test OK", r.stdout)


SPU2_C = Path(__file__).resolve().parent.parent / "port/src/spu2.c"
# SHA-1 of the SPU's Gaussian interpolation table (512 little-endian int16) as published in nocash's psx-spx SPU
# documentation ("4-Point Gaussian Interpolation"); DuckStation's spu.cpp carries the same values.
GAUSS_SHA1 = "03cd36a66e9195873a45fdea621e1adb8b0d09ba"
# The windows (oldest .. new) of the self-test's digest, in its order.
GAUSS_WINDOWS = ((0, 32767, 0, 0), (0, 0, 32767, 0), (1000, -2000, 3000, -4000), (32767, 32767, 32767, 32767),
                 (-32768, 12345, -777, 30000), (100, 200, 300, 400))


def gauss_table():
    """The model's kGauss table, read from port/src/spu2.c."""
    src = SPU2_C.read_text()
    body = src[src.index("kGauss[512] = {"):]
    body = body[body.index("{") + 1: body.index("};")]
    return [int(v, 16) for v in re.findall(r"-?0x[0-9A-Fa-f]+", body)]


def gauss(table, w, i):
    """The 4-point interpolation as psx-spx describes it: each tap (weight * sample) >> 15, summed."""
    return sum((table[k] * s) >> 15 for k, s in zip((0xFF - i, 0x1FF - i, 0x100 + i, i), w))


class GaussTable(unittest.TestCase):
    def test_table_is_the_published_one(self):
        """The table is the hardware's (checked by digest), and each phase's four weights sum to 0x7F7F-0x7F81, so an
        interpolated sample stays within 16 bits (psx-spx)."""
        t = gauss_table()
        self.assertEqual(len(t), 512)
        self.assertEqual(hashlib.sha1(struct.pack("<512h", *t)).hexdigest(), GAUSS_SHA1)
        sums = {t[0xFF - i] + t[0x1FF - i] + t[0x100 + i] + t[i] for i in range(256)}
        self.assertEqual((min(sums), max(sums)), (0x7F7F, 0x7F81))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class GaussSelfTest(unittest.TestCase):
    def test_interpolation_digest(self):
        """TTT_SELFTEST=spu prints a digest of the model's interpolation over all 256 phases of six windows; this
        recomputes it here from the table, independently of the C code."""
        r = run_ttt(TTT_SELFTEST="spu")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        t, h = gauss_table(), 2166136261
        for w in GAUSS_WINDOWS:
            for i in range(256):
                h = ((h ^ (gauss(t, w, i) & 0xFFFFFFFF)) * 16777619) & 0xFFFFFFFF
        self.assertIn(f"ttt: spu gauss digest 0x{h:08x}", r.stdout)


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
        starts with the title (the first effect at VBlank 175: the boot's check of the empty memory card, GetInfo then
        GetDir, takes 6 VBlanks more than the 'no card' answer before T35, which had it at 169)."""
        with tempfile.TemporaryDirectory() as d:
            a, ra = self.dump(d, "a.wav")
            b, rb = self.dump(d, "b.wav")
        self.assertEqual(hashlib.sha1(a).hexdigest(), hashlib.sha1(b).hexdigest())
        self.assertEqual(re.search(r"ttt: state: .*", ra.stdout).group(0), re.search(r"ttt: state: .*", rb.stdout).group(0))
        frames = list(wav_frames(a))
        self.assertEqual(len(frames), frames_for(self.FRAMES))
        loud = [i for i, (l, r) in enumerate(frames) if abs(l) > 1000 or abs(r) > 1000]
        self.assertGreater(len(loud), RATE)  # more than a second of clearly audible sound
        self.assertEqual([f for f in frames[: frames_for(166)] if f != (0, 0)], [])  # boot: silence
        self.assertTrue(any(f != (0, 0) for f in frames[frames_for(174): frames_for(181)]))  # the title's effect
        # the select screen's BGM (track 0, from VBlank 225): both channels sound, unlike the mono effects
        select = frames[frames_for(250): frames_for(320)]
        self.assertTrue(any(l != r for l, r in select))
        self.assertGreater(sum(1 for f in select if f != (0, 0)), len(select) * 9 // 10)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class HostVolumes(unittest.TestCase):
    """T40 (docs/port/sound.md, Volumes): the master, music and effects volumes scale the port's output (TTT_WAV here,
    the window's stream alike) and nothing the game sees: the run of WavDump at other volumes keeps its state hash,
    master 50 % halves every sample, music 0 silences the select screen's BGM (voices 22-23 of core 0) and keeps the
    title's effect, effects 0 does the opposite, both 0 is silence."""

    FRAMES = WavDump.FRAMES

    def dump(self, d, name, **env):
        p = Path(d) / "in.txt"
        p.write_text(stream(*TO_THE_FIGHT))
        wav = Path(d) / name
        r = run_ttt(TTT_FRAMES=str(self.FRAMES), TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_WAV=str(wav), **env)
        self.assertEqual(r.returncode, 0, r.stdout[-3000:] + r.stderr)
        return list(wav_frames(wav.read_bytes())), re.search(r"ttt: state: .*", r.stdout).group(0)

    def test_volumes_scale_the_output_only(self):
        with tempfile.TemporaryDirectory() as d:
            full, s0 = self.dump(d, "full.wav")
            half, s1 = self.dump(d, "half.wav", TTT_VOLUME="50")
            nomusic, s2 = self.dump(d, "nomusic.wav", TTT_MUSIC_VOLUME="0")
            nofx, s3 = self.dump(d, "nofx.wav", TTT_EFFECTS_VOLUME="0")
            none, s4 = self.dump(d, "none.wav", TTT_MUSIC_VOLUME="0", TTT_EFFECTS_VOLUME="0")
        self.assertEqual({s1, s2, s3, s4}, {s0})
        self.assertEqual(len(half), len(full))
        bad = [(a, b) for fa, fb in zip(full, half) for a, b in zip(fa, fb) if abs(a) < 32767 and abs(int(a / 2) - b) > 1]
        self.assertEqual(bad[:5], [])
        self.assertTrue(any(f != (0, 0) for f in half))
        title, select = slice(frames_for(174), frames_for(181)), slice(frames_for(250), frames_for(320))
        self.assertTrue(any(f != (0, 0) for f in full[title]))
        self.assertEqual(nomusic[title], full[title])  # the title's effect, before any music
        self.assertEqual([f for f in nofx[title] if f != (0, 0)], [])
        quiet = lambda fs: sum(1 for l, r in fs if abs(l) > 64 or abs(r) > 64)
        self.assertGreater(quiet(nofx[select]), quiet(full[select]) * 9 // 10)  # the BGM stays
        self.assertLess(quiet(nomusic[select]), quiet(full[select]) // 10)
        self.assertEqual([f for f in none if f != (0, 0)], [])


if __name__ == "__main__":
    unittest.main()
