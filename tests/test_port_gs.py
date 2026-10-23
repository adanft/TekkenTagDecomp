"""The PC build's GS path (port/src/gs_core.c, gs_vif1.c): DMA -> VIF1 -> GIF -> GS memory and the software reference
rasteriser, checked on the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py).
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
        textured through a CLUT, alpha blended, Z tested, scissored); VIF1 UNPACK modes, STCYCL, masks and DIRECT."""
        r = run_ttt(TTT_SELFTEST="gs", TTT_FRAMES="1")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertNotIn("FAILED", r.stdout)
        self.assertIn("ttt: gs self-test OK", r.stdout)


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


if __name__ == "__main__":
    unittest.main()
