"""Mods (T49, docs/port/mods.md, port/src/plat_mods.c): music files that replace the game's BGM tracks in the host's
output only, and mod texture folders merged into the texture-pack lookup, checked on the built program build/port/ttt.
Every file a test uses is made here (sine waves, one-colour PNGs) in a temporary folder and deleted: no game audio or
picture is ever written to the repository. Skipped when the program has not been built; the game runs are also skipped
without disc data."""

import math
import re
import struct
import tempfile
import unittest
from pathlib import Path

from test_port_input import IN_THE_FIGHT, TO_THE_FIGHT, stream
from test_port_platform import EXE, disc, run_ttt
from test_port_sound import frames_for, wav_frames
from test_port_window import NO_SDL, SDL_LIBS, png_rgb


def wav_file(path, frames, rate, channels=1, bits=16, loop=None):
    """A PCM WAV of `frames` (a list of per-frame tuples of `channels` samples in 16-bit range), at `bits` per sample;
    `loop` (start, end inclusive) adds a smpl chunk."""
    def sample(v):
        if bits == 8:
            return struct.pack("<B", (v >> 8) + 128)
        if bits == 16:
            return struct.pack("<h", v)
        if bits == 24:
            return struct.pack("<i", v << 8)[:3]
        return struct.pack("<i", v << 16)
    data = b"".join(sample(v) for f in frames for v in f)
    align = channels * bits // 8
    fmt = struct.pack("<HHIIHH", 1, channels, rate, rate * align, align, bits)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    if len(data) & 1:
        chunks += b"\0"
    if loop is not None:
        smpl = struct.pack("<9I", 0, 0, 0, 60, 0, 0, 0, 1, 0) + struct.pack("<6I", 0, 0, loop[0], loop[1], 0, 0)
        chunks += b"smpl" + struct.pack("<I", len(smpl)) + smpl
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks)


def sine(freq, rate, seconds, amp=8000):
    return [(int(round(amp * math.sin(2 * math.pi * freq * i / rate))),) for i in range(int(rate * seconds))]


def tone_ratio(xs, freq, rate=48000):
    """The share of the signal's power at `freq` (Goertzel), 0..1."""
    w = 2 * math.pi * freq / rate
    c = 2 * math.cos(w)
    s1 = s2 = 0.0
    for x in xs:
        s1, s2 = x + c * s1 - s2, s1
    power = s1 * s1 + s2 * s2 - c * s1 * s2
    total = sum(x * x for x in xs)
    return 2 * power / (len(xs) * total) if total else 0.0


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class ModsSelfTest(unittest.TestCase):
    """TTT_SELFTEST=mods on a made-up mods folder: the load order (mods.order first, then names), conflicts (the first
    enabled mod wins a track), WAV formats and loops (smpl chunk, sidecar .loop file), the resampler, and the order and
    enable choices written to the settings file and read back by the next start."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-mods-")
        self.mods = Path(self.tmp.name) / "mods"
        self.cfg = Path(self.tmp.name) / "settings.ini"
        wav_file(self.mods / "alpha" / "00.wav", sine(440, 22050, 0.2), 22050, loop=(1000, 1999))
        wav_file(self.mods / "alpha" / "07 - title.wav", [(1000, -1000)] * 100, 44100, channels=2, bits=24)
        wav_file(self.mods / "beta" / "music" / "00.wav", [(-2000,)] * 300, 32000, bits=8)
        (self.mods / "beta" / "music" / "00.loop").write_text("# frames\n100 200\n")
        wav_file(self.mods / "beta" / "21.wav", [(3000, 4000)] * 50, 48000, channels=2, bits=32)
        (self.mods / "gamma" / "textures" / "pack").mkdir(parents=True)
        (self.mods / "beta" / "notes.txt").write_text("not music")
        self.cfg.write_text("mods.order = gamma, beta\n")

    def tearDown(self):
        self.tmp.cleanup()

    def run_test(self, **env):
        r = run_ttt(TTT_SELFTEST="mods", TTT_MODS=str(self.mods), TTT_CONFIG=str(self.cfg), **env)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: mods self-test OK", r.stdout)
        return r.stdout

    def test_list_files_and_order(self):
        out = self.run_test()
        self.assertIn("ttt: mods: list 0 gamma on music 0x0000000 shadowed 0x0000000 textures 1", out)
        self.assertIn("ttt: mods: list 1 beta on music 0x0200001 shadowed 0x0000000 textures 0", out)
        self.assertIn("ttt: mods: list 2 alpha on music 0x0000081 shadowed 0x0000001 textures 0", out)
        # track 0: beta's (earlier in the order), 8-bit at 32 kHz with its sidecar loop; alpha's 00.wav is hidden
        self.assertIn("ttt: mods: file track 0 mod beta 00.wav: 300 frames at 32000 Hz, loop 100-200 mode 1, "
                      "first -2048 -2048", out)
        self.assertIn("ttt: mods: file track 7 mod alpha 07 - title.wav: 100 frames at 44100 Hz, loop 0-0 mode 0, "
                      "first 1000 -1000", out)
        self.assertIn("ttt: mods: file track 21 mod beta 21.wav: 50 frames at 48000 Hz, loop 0-0 mode 0, "
                      "first 3000 4000", out)
        self.assertIn("ttt: mods: summary: music 3 tracks, textures 1 mod (gamma, beta, alpha)", out)

    def test_smpl_loop_and_choices_round_trip(self):
        (self.mods / "beta" / "music" / "00.wav").unlink()
        out = self.run_test(TTT_MODS_EDIT="1")
        self.assertIn("ttt: mods: file track 0 mod alpha 00.wav: 4410 frames at 22050 Hz, loop 1000-2000 mode 1", out)
        # the edit: alpha (last) moved to the top, the second (gamma) disabled, written at once
        self.assertIn("ttt: mods: edited 0 alpha on", out)
        self.assertIn("ttt: mods: edited 1 gamma off", out)
        text = self.cfg.read_text()
        self.assertIn("mods.order = alpha,gamma,beta\n", text)
        self.assertIn("mods.disabled = gamma\n", text)
        # the next start reads them back
        out = self.run_test()
        self.assertIn("ttt: mods: list 0 alpha on", out)
        self.assertIn("ttt: mods: list 1 gamma off", out)
        self.assertIn("ttt: mods: list 2 beta on", out)
        self.assertIn("ttt: mods: summary: music 3 tracks, textures 0 mods (alpha, beta)", out)
        # the environment wins over the file, as for every setting
        out = self.run_test(TTT_MOD_DISABLED="alpha,beta", TTT_MOD_TEXTURES="0", TTT_MOD_MUSIC="0")
        self.assertIn("ttt: mods: summary: music 0 tracks, textures 0 mods\n", out)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class MusicMods(unittest.TestCase):
    """A made-up 1 kHz sine (22.05 kHz mono WAV, resampled) replaces track 0, the character select's BGM, in the replay
    through the title, the menu and the select (test_port_sound.WavDump): TTT_WAV holds the sine while the track plays
    and the game's own mix elsewhere; the state hash is the one of the run without mods. Over the scripted fight, mods
    for every track change no state either."""

    FRAMES = 520

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-mods-")
        self.d = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def dump(self, name, **env):
        p = self.d / "in.txt"
        p.write_text(stream(*TO_THE_FIGHT))
        wav = self.d / name
        r = run_ttt(TTT_FRAMES=str(self.FRAMES), TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_WAV=str(wav), **env)
        self.assertEqual(r.returncode, 0, r.stdout[-3000:] + r.stderr)
        return list(wav_frames(wav.read_bytes())), r

    def test_sine_replaces_the_select_music(self):
        mods = self.d / "mods"
        wav_file(mods / "test" / "00.wav", sine(1000, 22050, 1.0), 22050)
        plain, r0 = self.dump("plain.wav")
        modded, r1 = self.dump("modded.wav", TTT_MODS=str(mods))
        nomusic, r2 = self.dump("nomusic.wav", TTT_MODS=str(mods), TTT_MUSIC_VOLUME="0")
        off, r3 = self.dump("off.wav", TTT_MODS=str(mods), TTT_MOD_MUSIC="0")
        self.assertIsNotNone(state(r0))
        self.assertEqual({state(r1), state(r2), state(r3)}, {state(r0)})
        m = re.search(r"ttt: mods: track 0: .*00\.wav \(VBlank (\d+)\)", r1.stdout)
        self.assertIsNotNone(m, r1.stdout[-2000:])
        start = int(m.group(1))
        self.assertTrue(220 <= start <= 230, start)
        self.assertEqual(off, plain)  # mods.music off: the game's own output, byte for byte
        self.assertEqual(modded[: frames_for(start - 1)], plain[: frames_for(start - 1)])  # before the track
        select = slice(frames_for(250), frames_for(320))
        # the music part of the modded output (the effects are the same in both): the sine, on both channels
        left = [a[0] - b[0] for a, b in zip(modded[select], nomusic[select])]
        right = [a[1] - b[1] for a, b in zip(modded[select], nomusic[select])]
        self.assertGreater(max(map(abs, left)), 1000)
        self.assertGreater(tone_ratio(left, 1000), 0.95)
        self.assertGreater(tone_ratio(right, 1000), 0.95)
        game = [a[0] - b[0] for a, b in zip(plain[select], nomusic[select])]
        self.assertLess(tone_ratio(game, 1000), 0.2)  # the game's own select music is no 1 kHz tone

    def test_fight_state_with_music_mods_for_every_track(self):
        mods = self.d / "mods"
        for t in range(28):
            wav_file(mods / "all" / f"{t:02d}.wav", sine(200 + 50 * t, 24000, 0.3), 24000)
        p = self.d / "fight.txt"
        p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
        runs = [run_ttt(timeout=300, TTT_FRAMES="1451", TTT_SEED="1", TTT_INPUT_PLAY=str(p), **env)
                for env in ({}, {"TTT_MODS": str(mods), "TTT_WAV": str(self.d / "f.wav")})]
        for r in runs:
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertIsNotNone(state(runs[0]))
        self.assertEqual(state(runs[1]), state(runs[0]))
        played = re.findall(r"ttt: mods: track (\d+): ", runs[1].stdout)
        self.assertIn("0", played)   # the character select
        self.assertIn("21", played)  # the stage of the scripted fight


def ffmpeg_vorbis():
    """ffmpeg with the libvorbis encoder (to make a test file), or None."""
    import shutil
    import subprocess
    exe = shutil.which("ffmpeg")
    if exe is None:
        return None
    r = subprocess.run([exe, "-hide_banner", "-encoders"], capture_output=True, text=True)
    return exe if "libvorbis" in r.stdout else None


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(ffmpeg_vorbis() is not None, "no ffmpeg with libvorbis to make a test file")
class VorbisMods(unittest.TestCase):
    """An Ogg Vorbis file made here by ffmpeg (a 1 kHz sine, 44.1 kHz stereo, LOOPSTART / LOOPLENGTH comments) decodes
    through stb_vorbis (mods_vorbis.cpp) with its loop and, in the replay to the character select, plays as the tone."""

    def setUp(self):
        import subprocess
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-mods-")
        self.d = Path(self.tmp.name)
        self.mods = self.d / "mods"
        (self.mods / "v").mkdir(parents=True)
        subprocess.run([ffmpeg_vorbis(), "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                        "sine=frequency=1000:sample_rate=44100:duration=2", "-ac", "2", "-c:a", "libvorbis",
                        "-metadata", "LOOPSTART=1000", "-metadata", "LOOPLENGTH=44100", str(self.mods / "v" / "00.ogg")],
                       check=True)

    def tearDown(self):
        self.tmp.cleanup()

    def test_decode(self):
        r = run_ttt(TTT_SELFTEST="mods", TTT_MODS=str(self.mods))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: mods: file track 0 mod v 00.ogg: 88200 frames at 44100 Hz, loop 1000-45100 mode 1", r.stdout)

    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_plays_as_the_select_music(self):
        p = self.d / "in.txt"
        p.write_text(stream(*TO_THE_FIGHT))
        out = []
        for name, env in (("a.wav", {}), ("b.wav", {"TTT_MUSIC_VOLUME": "0"})):
            r = run_ttt(TTT_FRAMES="400", TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_WAV=str(self.d / name),
                        TTT_MODS=str(self.mods), **env)
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr)
            out.append(list(wav_frames((self.d / name).read_bytes())))
        select = slice(frames_for(250), frames_for(320))
        left = [a[0] - b[0] for a, b in zip(out[0][select], out[1][select])]
        self.assertGreater(max(map(abs, left)), 1000)
        self.assertGreater(tone_ratio(left, 1000), 0.95)


def solid_png(path, w, h, rgba):
    """An 8-bit RGBA PNG of one colour (made up: no game data)."""
    import zlib

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + bytes(rgba) * w for _ in range(h))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL_LIBS), NO_SDL)
class TextureMods(unittest.TestCase):
    """Two mods with replacements for the same textures (names from a dump run, as tests/test_port_gl.py makes them;
    one colour each, made up here) and the texture pack off: the mod first in the load order is drawn, the other one's
    files are hidden; reversing mods.order reverses it. The state hash is the one of the run without mods."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-mods-")
        self.d = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def run_gl(self, name, **env):
        p = self.d / "in.txt"
        if not p.exists():
            p.write_text(stream(*TO_THE_FIGHT))
        out = self.d / name
        out.mkdir()
        r = run_ttt(timeout=600, TTT_FRAMES="451", TTT_SEED="1", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1",
                    TTT_RENDERER="gl", TTT_AUDIO="0", TTT_VSYNC="0", SDL_VIDEO_DRIVER="offscreen", TTT_DUMP=str(out),
                    TTT_DUMP_EVERY="450", TTT_INPUT_PLAY=str(p), **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        if "OpenGL renderer" not in r.stdout:
            self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
        return r, out / "frame_00450.png"

    @staticmethod
    def share(path, rgb):
        w, h, px = png_rgb(path)
        return sum(1 for i in range(0, len(px), 3)
                   if all(abs(px[i + k] - rgb[k]) < 60 for k in range(3))) / (w * h)

    def test_load_order_decides_and_state_is_unchanged(self):
        tex = self.d / "tex"
        plain, pic0 = self.run_gl("dump", TTT_TEXTURE_DUMP="1", TTT_TEXTURES=str(tex))
        names = sorted(p.name for p in (tex / "SLUS-20001" / "dumps").iterdir())
        self.assertGreater(len(names), 10)
        mods = self.d / "mods"
        for n in names:
            w, h = struct.unpack(">II", (tex / "SLUS-20001" / "dumps" / n).read_bytes()[16:24])
            solid_png(mods / "magenta" / "textures" / "a" / n, w, h, (255, 0, 255, 128))
            solid_png(mods / "green" / "textures" / n, w, h, (0, 255, 0, 128))
            # a pack copied whole into a mod: its dumps/ folder is not read as replacements
            solid_png(mods / "green" / "textures" / "dumps" / n, w, h, (0, 0, 255, 128))
        r1, pic1 = self.run_gl("magenta", TTT_MODS=str(mods), TTT_MOD_ORDER="magenta,green", TTT_TEXTURES=str(tex))
        r2, pic2 = self.run_gl("green", TTT_MODS=str(mods), TTT_MOD_ORDER="green,magenta", TTT_TEXTURES=str(tex))
        for r in (r1, r2):
            self.assertEqual(state(r), state(plain))
            self.assertIn(f"ttt: textures: mods: 2 mod folders, {2 * len(names)} replacements from mods, "
                          f"{len(names)} names hidden by an earlier file", r.stdout)
            self.assertRegex(r.stdout, r"ttt: textures: .*replace on;")
        magenta, green = (255, 0, 255), (0, 255, 0)
        self.assertLess(self.share(pic0, magenta), 0.002)
        self.assertGreater(self.share(pic1, magenta), 0.015)
        self.assertLess(self.share(pic1, green), 0.002)
        self.assertGreater(self.share(pic2, green), 0.015)
        self.assertLess(self.share(pic2, magenta), 0.002)


if __name__ == "__main__":
    unittest.main()
