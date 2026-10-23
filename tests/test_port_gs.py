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


if __name__ == "__main__":
    unittest.main()
