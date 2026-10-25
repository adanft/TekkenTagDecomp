"""The PC build's GS path (port/src/gs_core.c, gs_vif1.c, gs_vu1.c on vu_micro.c): DMA -> VIF1 (-> VU1) -> GIF -> GS
memory and the software reference rasteriser, checked on the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py).
Skipped when the program has not been built; the frame dump runs are also skipped without disc data. The frames are
written to a temporary directory and removed: they are pictures of the game and never belong in the repository."""

import re
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt


def read_png(path):
    """(width, height, rows of RGB bytes) of an 8-bit RGB PNG with filter 0 rows, as the port writes them."""
    data = Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos, idat, width, height = 8, b"", 0, 0
    while pos < len(data):
        n, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        crc = struct.unpack(">I", data[pos + 8 + n:pos + 12 + n])[0]
        if zlib.crc32(kind + body) != crc:
            raise ValueError(f"bad CRC in {kind!r}")
        if kind == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", body[:10])
            if (depth, colour) != (8, 2):
                raise ValueError("not 8-bit RGB")
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = width * 3 + 1
    rows = [raw[y * stride + 1:(y + 1) * stride] for y in range(height)]
    if any(raw[y * stride] != 0 for y in range(height)):
        raise ValueError("unexpected row filter")
    return width, height, rows


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class GsSelfTest(unittest.TestCase):
    def test_gs(self):
        """GS memory swizzles (PSMCT32/24/16/16S, PSMT8/8H/4/4HL/4HH, the Z formats) against the GS's layout and
        between the per-pixel and the upload paths; a GIF PACKED packet on the GIF channel drawing a sprite (flat,
        textured through a CLUT, alpha blended, Z tested, scissored); the read circuits' PMODE merge (ALP or circuit 1
        alpha, SLBG, DISPLAY position); VIF1 UNPACK modes, STCYCL, masks and DIRECT."""
        r = run_ttt(TTT_SELFTEST="gs", TTT_FRAMES="1")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertNotIn("FAILED", r.stdout)
        self.assertIn("ttt: gs self-test OK", r.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class Vu1SelfTest(unittest.TestCase):
    def test_vu1(self):
        """VU1 micro mode (port/src/vu_micro.c, gs_vu1.c) on hand-assembled programs sent through VIF1 (MPG, MSCAL,
        MSCNT): upper / lower pairing, branch and E-bit delay slots, Q and P latency, MAC and clipping flag delay,
        integer and EFU instructions, XTOP / XITOP through the double buffer, XGKICK to the GS (also across the end of
        VU memory), and no instruction left unimplemented."""
        r = run_ttt(TTT_SELFTEST="vu1")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertNotIn("FAILED", r.stdout)
        self.assertIn("ttt: vu1 self-test OK", r.stdout)

    def test_predecoded_interpreter_agrees_with_the_reference(self):
        """The predecoded interpreter (Vu_Run with a VuOp cache) and the one-step reference (Vu_RunRef) end random
        programs of every implemented instruction (dest masks, broadcasts, branches, flags, Q / P, XGKICK) in the same
        registers, memory, pipeline state and kicked packets; its 4-lane add / multiply / max / min agree with the
        scalar model on edge and random operands; rewriting micro memory is seen by the cache."""
        r = run_ttt(TTT_SELFTEST="vu1")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        m = re.search(r"ttt: vu fast: (\d+) random programs, (\d+) instructions, (\d+) differ; "
                      r"(\d+) lane operations, (\d+) differ", r.stdout)
        self.assertTrue(m, r.stdout[-2000:])
        self.assertGreaterEqual(int(m.group(1)), 2000)
        self.assertEqual(m.group(3), "0")
        self.assertGreaterEqual(int(m.group(4)), 1000000)
        self.assertEqual(m.group(5), "0")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class FrameDump(unittest.TestCase):
    FRAMES = 360
    EVERY = 60

    def test_dump_writes_the_displayed_frames_and_leaves_the_game_alone(self):
        """TTT_DUMP writes the displayed buffer every TTT_DUMP_EVERY VBlanks as PNG; the title screen has a picture
        (many colours); dumping does not change the game's state (the same hash as a run without it)."""
        with tempfile.TemporaryDirectory(prefix="ttt-gs-") as tmp:
            r = run_ttt(TTT_FRAMES=str(self.FRAMES), TTT_SEED="1", TTT_DUMP=tmp, TTT_DUMP_EVERY=str(self.EVERY))
            self.assertEqual(r.returncode, 0, r.stdout[-3000:] + r.stderr[-3000:])
            files = sorted(Path(tmp).glob("frame_*.png"))
            self.assertEqual([f.name for f in files],
                             [f"frame_{n:05d}.png" for n in range(self.EVERY, self.FRAMES + 1, self.EVERY)])
            colours = 0
            for f in files:
                w, h, rows = read_png(f)
                self.assertEqual((w, h), (640, 448))
                colours = max(colours, len({row[i:i + 3] for row in rows[::4] for i in range(0, len(row), 12)}))
            self.assertGreater(colours, 16, "every dumped frame is (nearly) one colour")
        plain = run_ttt(TTT_FRAMES=str(self.FRAMES), TTT_SEED="1")
        state = [re.search(r"ttt: state: .*", x.stdout) for x in (r, plain)]
        self.assertTrue(all(state), plain.stdout[-2000:])
        self.assertEqual(state[0].group(0), state[1].group(0))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Vu1Check(unittest.TestCase):
    FRAMES = 1200  # the demo fight draws its fighters through VU1 from about VBlank 1090

    def test_game_microcode_runs_the_same_on_both_interpreters(self):
        """TTT_VU_CHECK=1 runs every VU1 program of the game twice, on the reference interpreter (a copy of the unit)
        and on the predecoded one: the registers, VU memory, pipeline state and kicked packets agree, and the game's
        state is the same as without the check."""
        r = run_ttt(timeout=300, TTT_FRAMES=str(self.FRAMES), TTT_SEED="1", TTT_VU_CHECK="1")
        self.assertEqual(r.returncode, 0, r.stdout[-3000:] + r.stderr[-3000:])
        m = re.search(r"ttt: vu1 check: (\d+) runs, (\d+) differ", r.stdout)
        self.assertTrue(m, r.stdout[-2000:])
        self.assertGreater(int(m.group(1)), 10000)
        self.assertEqual(m.group(2), "0")
        plain = run_ttt(timeout=300, TTT_FRAMES=str(self.FRAMES), TTT_SEED="1")
        state = [re.search(r"ttt: state: .*", x.stdout) for x in (r, plain)]
        self.assertTrue(all(state), plain.stdout[-2000:])
        self.assertEqual(state[0].group(0), state[1].group(0))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class RasterThreads(unittest.TestCase):
    FRAMES = 420
    EVERY = 30

    def test_raster_threads_draw_the_same_frames(self):
        """TTT_GS_THREADS=n splits the drawing between n workers (gs_core.c): the frames, the report and the state
        hash are the same as drawing on the game's thread alone (TTT_GS_THREADS=0)."""
        runs = []
        for n in ("0", "4"):
            with tempfile.TemporaryDirectory(prefix="ttt-gs-") as tmp:
                r = run_ttt(timeout=300, TTT_FRAMES=str(self.FRAMES), TTT_SEED="1", TTT_DUMP=tmp, TTT_DUMP_EVERY=str(self.EVERY),
                            TTT_GS_THREADS=n)
                self.assertEqual(r.returncode, 0, r.stdout[-3000:] + r.stderr[-3000:])
                frames = {f.name: f.read_bytes() for f in sorted(Path(tmp).glob("frame_*.png"))}
                runs.append((frames, [l for l in r.stdout.splitlines() if l.startswith("ttt: ")]))
        self.assertEqual(len(runs[0][0]), self.FRAMES // self.EVERY)
        self.assertEqual(sorted(runs[0][0]), sorted(runs[1][0]))
        for name in runs[0][0]:
            self.assertEqual(runs[0][0][name], runs[1][0][name], name)
        self.assertEqual(runs[0][1], runs[1][1])


def row_profile(path):
    """The mean of each row of a dumped picture, averaged over two rows (the doubled lines of interlaced frame mode)."""
    w, h, rows = read_png(path)
    r = [sum(row) / len(row) for row in rows]
    return [(r[y] + r[y + 1]) / 2 for y in range(h - 1)]


def vertical_shift(a, b, most=2.0, step=0.125):
    """The vertical shift s (in rows, a multiple of step within +-most) for which b(y + s), linearly interpolated, is
    closest to a(y) on average: how far the content of picture b moved from picture a."""
    best = None
    for i in range(int(2 * most / step) + 1):
        s, err = -most + i * step, 0.0
        for y in range(16, len(a) - 16):
            k, f = int(y + s), y + s - int(y + s)
            err += abs(a[y] - (b[k] * (1 - f) + b[k + 1] * f))
        if best is None or err < best[1]:
            best = (s, err)
    return best[0]


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class FieldPlacement(unittest.TestCase):
    """docs/port/video.md, Field placement: the game's flicker filter makes each field's 224-line picture from the even
    or the odd lines of its frame, and the odd field is shown one raster line lower, as a TV does. Consecutive VBlank
    pictures of the main menu (the scripted run of test_port_input.py, VBlanks 290-330) stay in place: before the fix
    the picture moved 0.75-1.25 rows up and down at every VBlank (the content of the odd lines one line higher), now
    at most 0.5 (what remains is the filter's own difference between the fields, about 0.25). The pictures are written
    to a temporary directory and removed. Software renderer about 15 s, the GL one in a hidden window about 10 s."""

    FROM, TO = 290, 330

    def shifts(self, out):
        prev, shifts = None, []
        for v in range(self.FROM, self.TO + 1):
            p = row_profile(Path(out) / f"frame_{v:05d}.png")
            if prev is not None:
                shifts.append(vertical_shift(prev, p))
            prev = p
        return shifts

    def run_menu(self, d, **env):
        from test_port_input import TO_THE_FIGHT, stream
        p = Path(d) / "in.txt"
        p.write_text(stream(*TO_THE_FIGHT))
        out = Path(d) / "dump"
        out.mkdir()
        r = run_ttt(timeout=600, TTT_FRAMES=str(self.TO), TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_DUMP=str(out),
                    TTT_DUMP_EVERY="1", TTT_DUMP_FROM=str(self.FROM), **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return r, out

    def check(self, shifts):
        self.assertEqual(len(shifts), self.TO - self.FROM)
        self.assertLessEqual(max(abs(s) for s in shifts), 0.5, shifts)

    def test_software(self):
        with tempfile.TemporaryDirectory(prefix="ttt-field-") as d:
            _, out = self.run_menu(d)
            self.check(self.shifts(out))

    def test_gl_with_and_without_the_flicker_filter(self):
        """Without the filter (video.deflicker 0) the frame itself is shown, the same in both fields: no shift."""
        from test_port_window import SDL32
        if not any(p.exists() for p in SDL32):
            self.skipTest("no 32-bit SDL3 (Arch: lib32-sdl3)")
        gl = dict(TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER="gl", TTT_AUDIO="0", TTT_VSYNC="0",
                  SDL_VIDEO_DRIVER="offscreen")
        for deflicker in ("1", "0"):
            with tempfile.TemporaryDirectory(prefix="ttt-field-") as d:
                r, out = self.run_menu(d, TTT_DEFLICKER=deflicker, **gl)
                if "OpenGL renderer" not in r.stdout:
                    self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
                shifts = self.shifts(out)
                self.check(shifts)
                if deflicker == "0":
                    self.assertEqual(set(shifts), {0.0}, shifts)


if __name__ == "__main__":
    unittest.main()
