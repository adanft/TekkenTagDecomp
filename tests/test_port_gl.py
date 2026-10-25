"""The PC build's GL back end and GS threads (T30 batch 4), checked on the built program build/port/ttt (made from your
own SLUS_200.01 by port/tools/build.py): the GL back end's batching and hazard rules on made-up primitives
(TTT_SELFTEST=gl, no GL context needed), the two GS threads against one and none, and the TTT_TIME_FROM measurement;
the video options of T37 (internal resolution, widescreen, the game's flicker filter, low latency) in a hidden window.
Texture packs (T39): dumps named as PCSX2 names them, replacements drawn instead, in a hidden window. Skipped when the
program has not been built; the game runs are also skipped without disc data."""

import os
import re
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt
from test_port_window import SDL32, png_rgb


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

    def test_texture_pack_names(self):
        """PCSX2's texture names (T39) on a made-up GS memory: the stored blocks of a PSMCT32 texture in block-grid
        order, the expanded texels of a small one and of a CLAMP region, TEXA in the bits, the CLUT's hash; names read
        back from file names, malformed ones refused (no GL context, no files)."""
        r = run_ttt(TTT_SELFTEST="texpack", TTT_FRAMES="1")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertIn("ttt: texpack self-test OK", r.stdout)


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



def gl_run(d, name, frames="301", every="150", **env):
    """A run with the GL back end in a hidden window (SDL's offscreen driver), dumps into d/name."""
    out = Path(d) / name
    out.mkdir()
    r = run_ttt(timeout=600, TTT_FRAMES=frames, TTT_SEED="1", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER="gl",
                TTT_AUDIO="0", TTT_VSYNC="0", SDL_VIDEO_DRIVER="offscreen", TTT_DUMP=str(out), TTT_DUMP_EVERY=every, **env)
    return r, out


def downscaled(w, h, px, k):
    """A k x k box filter of an RGB picture (w x h, k divides both)."""
    out = bytearray((w // k) * (h // k) * 3)
    for y in range(h // k):
        for x in range(w // k):
            for c in range(3):
                t = 0
                for dy in range(k):
                    row = ((y * k + dy) * w + x * k) * 3 + c
                    t += sum(px[row + dx * 3] for dx in range(k))
                out[(y * (w // k) + x) * 3 + c] = (t + k * k // 2) // (k * k)
    return bytes(out)


def mean_diff(a, b):
    return sum(abs(x - y) for x, y in zip(a, b)) / len(a)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
class Video(unittest.TestCase):
    """T37 (docs/port/video.md) on the title and the main menu (VBlanks 150 and 300)."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def run_gl(self, name, **env):
        r, out = gl_run(self.d, name, **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        if "OpenGL renderer" not in r.stdout:
            self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
        return r, out

    def test_internal_resolution(self):
        """2x and 4x: pictures twice and four times as wide and high whose content is the 1x picture's: the title's
        k x k box-filtered within 2 levels on average; the menu's (textures minified from finer ones: the 1x picture
        aliases what the finer one resolves) compared over 8 x 8 blocks of the 1x picture. The game's state does not
        change."""
        one, d1 = self.run_gl("x1")
        for k in (2, 4):
            r, dk = self.run_gl(f"x{k}", TTT_RESOLUTION=str(k))
            self.assertEqual(state(r), state(one))
            self.assertIn(f"ttt: gl: video {k}x, margin 0", r.stdout)
            for name, n, limit in (("frame_00150.png", 1, 2.0), ("frame_00300.png", 8, 8.0)):
                w, h, a = png_rgb(d1 / name)
                wk, hk, b = png_rgb(dk / name)
                self.assertEqual((wk, hk), (w * k, h * k), name)
                self.assertLess(mean_diff(downscaled(w, h, a, n) if n > 1 else a, downscaled(wk, hk, b, k * n)), limit, f"{k}x {name}")

    def test_widescreen(self):
        """16:9 and 21:9: pictures 796 and 1046 wide (the 640 x 448 picture's pixels, square, widened); the middle 640
        columns are the 4:3 picture (the menus' 2D stays 4:3, the margins of a 2D screen are black); same state."""
        four, d4 = self.run_gl("a43")
        for aspect, width in (("16:9", 796), ("21:9", 1046)):
            r, dw = self.run_gl(aspect.replace(":", "_"), TTT_ASPECT=aspect)
            self.assertEqual(state(r), state(four))
            m = (width - 640) // 2
            for name in ("frame_00150.png", "frame_00300.png"):
                w, h, a = png_rgb(d4 / name)
                ww, hh, b = png_rgb(dw / name)
                self.assertEqual((ww, hh), (width, 448), name)
                mid = b"".join(b[(y * ww + m) * 3:(y * ww + m + 640) * 3] for y in range(hh))
                self.assertLess(mean_diff(a, mid), 2.0, f"{aspect} {name}")
                left = b"".join(b[y * ww * 3:(y * ww + m) * 3] for y in range(hh))
                self.assertLess(sum(left) / len(left), 8.0, f"{aspect} {name}: the title and menu have black margins")

    def test_flicker_filter_and_low_latency(self):
        """Without the game's flicker filter the picture is its frame before the half-height copy (sharper: not the
        same picture); low latency (its frames' snapshots at 1x) changes no picture; same state."""
        on, d1 = self.run_gl("on")
        off, d0 = self.run_gl("off", TTT_DEFLICKER="0")
        ll, dl = self.run_gl("ll", TTT_LOW_LATENCY="1")
        self.assertEqual(state(off), state(on))
        self.assertEqual(state(ll), state(on))
        self.assertIn("flicker filter off", off.stdout)
        self.assertIn("low latency on", ll.stdout)
        w, h, a = png_rgb(d1 / "frame_00300.png")
        w0, h0, b = png_rgb(d0 / "frame_00300.png")
        self.assertEqual((w0, h0), (w, h))
        self.assertGreater(mean_diff(a, b), 0.5)
        for name in ("frame_00150.png", "frame_00300.png"):
            self.assertEqual((dl / name).read_bytes(), (d1 / name).read_bytes(), name)

    @unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (about a minute): TTT_SLOW_TESTS=1")
    def test_aspect_and_scale_change_no_state_over_the_fight(self):
        """The scripted fight (test_port_input.py) with 4:3 at 1x, 16:9 at 2x and 21:9 without the flicker filter:
        the same state hash (the options are presentation only)."""
        from test_port_input import IN_THE_FIGHT, TO_THE_FIGHT, stream
        p = Path(self.d) / "in.txt"
        p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
        runs = [self.run_gl(n, frames="1450", every="725", TTT_INPUT_PLAY=str(p), **env)[0] for n, env in (
            ("f43", {}), ("f169", {"TTT_ASPECT": "16:9", "TTT_RESOLUTION": "2"}),
            ("f219", {"TTT_ASPECT": "21:9", "TTT_DEFLICKER": "0", "TTT_LOW_LATENCY": "1"}))]
        self.assertIsNotNone(state(runs[0]))
        for r in runs[1:]:
            self.assertEqual(state(r), state(runs[0]))


def solid_png(path, w, h, rgba):
    """An 8-bit RGBA PNG of one colour (a made-up replacement: no game data)."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    rows = b"".join(b"\0" + bytes(rgba) * w for _ in range(h))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
                     chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def png_size(path):
    w, h = struct.unpack(">II", path.read_bytes()[16:24])
    return w, h


NAME = re.compile(r"^[0-9a-f]{1,16}-(?:[0-9a-f]{1,16}-)?(?:r\d+x\d+-)?[0-9a-f]{8}\.png$")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
class TexturePacks(unittest.TestCase):
    """T39 (docs/port/textures.md) from the title to the versus screen (the scripted start of tests/test_port_input.py,
    VBlanks 150 to 600: the menu, the character select, the versus screen; the title and the menu draw no texture a pack
    can name), textures in a temporary folder (TTT_TEXTURES): nothing of the game's is kept."""

    PICS = ("frame_00150.png", "frame_00300.png", "frame_00450.png", "frame_00600.png")

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.d = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def run_gl(self, name, **env):
        from test_port_input import TO_THE_FIGHT, stream
        p = Path(self.d) / "in.txt"
        if not p.exists():
            p.write_text(stream(*TO_THE_FIGHT))
        r, out = gl_run(self.d, name, frames="601", TTT_INPUT_PLAY=str(p), **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        if "OpenGL renderer" not in r.stdout:
            self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
        return r, out

    def test_dump_names_are_deterministic_and_replacements_are_drawn(self):
        """Dump mode writes each texture once, as a PNG named from its data, CLUT and format (PCSX2's names): two runs
        write the same files, and the pictures and the state are those of a run without it. A replacement for every
        dumped name (one colour, twice the size: made up here) is drawn instead of the texture in replace mode: the
        fighter on the character select turns that colour, the state hash stays the same."""
        plain, d0 = self.run_gl("plain")
        tx = [Path(self.d) / f"tx{k}" for k in (1, 2)]
        runs = [self.run_gl(f"dump{k}", TTT_TEXTURE_DUMP="1", TTT_TEXTURES=str(t)) for k, t in zip((1, 2), tx)]
        dumps = [t / "SLUS-20001" / "dumps" for t in tx]
        names = sorted(p.name for p in dumps[0].iterdir())
        self.assertGreater(len(names), 10, runs[0][0].stdout[-1500:])
        self.assertEqual([n for n in names if not NAME.match(n)], [])
        self.assertEqual(names, sorted(p.name for p in dumps[1].iterdir()))
        for n in names:
            self.assertEqual((dumps[0] / n).read_bytes(), (dumps[1] / n).read_bytes(), n)
        self.assertRegex(runs[0][0].stdout, rf"ttt: textures: .*dump on, replace off; \d+ textures named, {len(names)} dumped \(0 on")
        for r, out in runs:
            self.assertEqual(state(r), state(plain))
            for pic in self.PICS:
                self.assertEqual((out / pic).read_bytes(), (d0 / pic).read_bytes(), pic)
        # replacements: every dumped name, magenta, twice as wide and high, in a sub-folder
        repl = tx[0] / "SLUS-20001" / "replacements" / "pack"
        repl.mkdir(parents=True)
        for n in names:
            w, h = png_size(dumps[0] / n)
            solid_png(repl / n, 2 * w, 2 * h, (255, 0, 255, 128))
        r, out = self.run_gl("replace", TTT_TEXTURE_REPLACE="1", TTT_TEXTURES=str(tx[0]))
        self.assertEqual(state(r), state(plain))
        m = re.search(r"ttt: textures: .*replace on; .* (\d+) replacements \((\d+) decoded, 0 failed.*?(\d+) uploaded, (\d+) primitives drawn with one", r.stdout)
        self.assertIsNotNone(m, r.stdout[-1500:])
        self.assertEqual(int(m.group(1)), len(names))
        self.assertEqual(int(m.group(2)), len(names))
        self.assertGreater(int(m.group(3)), 0)
        self.assertGreater(int(m.group(4)), 0)

        def magenta(path):
            w, h, px = png_rgb(path)
            return sum(1 for i in range(0, len(px), 3) if px[i] > 200 and px[i + 1] < 60 and px[i + 2] > 200) / (w * h)

        # the character select: the fighter turns magenta (about 3% of the picture)
        self.assertLess(magenta(d0 / "frame_00450.png"), 0.002)
        self.assertGreater(magenta(out / "frame_00450.png"), 0.015)


if __name__ == "__main__":
    unittest.main()
