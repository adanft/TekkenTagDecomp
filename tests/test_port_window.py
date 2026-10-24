"""The PC build's window (port/src/plat_window.c: SDL3 loaded at run time) and the host calls added with it and the GS
threads, checked on the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py).
Skipped when the program has not been built; the runs are also skipped without disc data, and the SDL run when the
32-bit SDL3 library is not installed. The SDL runs use SDL's dummy or offscreen video driver and a hidden window: no
window appears, and no input reaches the desktop (the key mapping is checked on made-up events, TTT_SELFTEST=window)."""

import re
import subprocess
import tempfile
import unittest
import zlib
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt

SDL32 = [Path("/usr/lib32/libSDL3.so.0"), Path("/usr/lib/i386-linux-gnu/libSDL3.so.0")]


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


def png_rgb(path):
    """The pixels of a PNG the port wrote (8-bit RGB, filter 0 rows): (width, height, bytes)."""
    d, i, idat, w, h = path.read_bytes(), 8, b"", 0, 0
    while i < len(d):
        n = int.from_bytes(d[i:i + 4], "big")
        kind, chunk = d[i + 4:i + 8], d[i + 8:i + 8 + n]
        if kind == b"IHDR":
            w, h = int.from_bytes(chunk[:4], "big"), int.from_bytes(chunk[4:8], "big")
        elif kind == b"IDAT":
            idat += chunk
        i += 12 + n
    raw, stride = zlib.decompress(idat), w * 3 + 1
    return w, h, b"".join(raw[y * stride + 1:(y + 1) * stride] for y in range(h))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class HostCalls(unittest.TestCase):
    def test_new_host_calls_bypass_the_plt(self):
        """The game keeps host code addresses in .sbss, which the state hash covers: the threads and the window call
        libpthread / libdl through the GOT (port_host.h), so the PLT, and every game function's address, stay put."""
        out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", str(EXE)], capture_output=True, text=True,
                             check=True).stdout
        plt = re.findall(r"<(\w+)@plt>:", out)
        self.assertTrue(plt)
        self.assertEqual([n for n in plt if n.startswith(("pthread_", "dl", "sysconf", "_exit", "__atomic"))], [])

    def test_window_keys(self):
        """The keyboard's mapping and the quit keys on made-up SDL events (TTT_SELFTEST=window): a Return tap between
        two VBlanks is pad 1's Start once, keypad Enter pad 2's, Escape (by key code or by place) and the close button
        end the program, Return does not."""
        r = run_ttt(TTT_SELFTEST="window")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: window self-test OK", r.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Window(unittest.TestCase):
    FRAMES = "200"

    def test_headless_runs_open_no_window(self):
        """A TTT_FRAMES run is headless: no window unless TTT_WINDOW=1."""
        r = run_ttt(TTT_FRAMES=self.FRAMES, TTT_SEED="1")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertNotIn("ttt: window", r.stdout)

    def test_missing_sdl_falls_back_to_headless(self):
        """Without SDL3 (TTT_SDL_LIB pointing nowhere) the run says so and goes on headless, in the same state."""
        r = run_ttt(TTT_FRAMES=self.FRAMES, TTT_SEED="1", TTT_WINDOW="1", TTT_SDL_LIB="/nonexistent/libSDL3.so.0")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertRegex(r.stdout, r"ttt: window: cannot load /nonexistent/libSDL3\.so\.0 .*: running headless")
        plain = run_ttt(TTT_FRAMES=self.FRAMES, TTT_SEED="1")
        self.assertIsNotNone(state(plain), plain.stdout[-2000:])
        self.assertEqual(state(r), state(plain))

    @unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
    def test_window_shows_the_pictures_and_leaves_the_game_alone(self):
        """With SDL3 (dummy video driver) the window opens, every VBlank's picture is handed over, and the game's state
        is the one of a run without a window."""
        r = run_ttt(TTT_FRAMES=self.FRAMES, TTT_SEED="1", TTT_WINDOW="1", SDL_VIDEO_DRIVER="dummy")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertRegex(r.stdout, r"ttt: window: SDL 3\.\d+\.\d+, renderer \w+")
        m = re.search(r"ttt: window: (\d+) pictures handed over", r.stdout)
        self.assertIsNotNone(m, r.stdout[-2000:])
        self.assertGreater(int(m.group(1)), 0)
        plain = run_ttt(TTT_FRAMES=self.FRAMES, TTT_SEED="1")
        self.assertEqual(state(r), state(plain))

    @unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
    def test_gl_back_end_draws_the_pictures_of_the_software_one(self):
        """TTT_RENDERER=gl (SDL's offscreen driver, a hidden window): the GPU draws the title and the main menu like the
        software rasteriser (the dumps of both at VBlanks 150 and 300 differ by a few levels on average), and the game's
        state is the same. Skipped where SDL gives no OpenGL 3.3 context."""
        with tempfile.TemporaryDirectory() as d:
            gl, sw = Path(d) / "gl", Path(d) / "sw"
            gl.mkdir()
            sw.mkdir()
            r = run_ttt(TTT_FRAMES="301", TTT_SEED="1", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER="gl",
                        SDL_VIDEO_DRIVER="offscreen", TTT_DUMP=str(gl), TTT_DUMP_EVERY="150")
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            if "OpenGL renderer" not in r.stdout:
                self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
            plain = run_ttt(TTT_FRAMES="301", TTT_SEED="1", TTT_DUMP=str(sw), TTT_DUMP_EVERY="150")
            self.assertEqual(state(r), state(plain))
            for name in ("frame_00150.png", "frame_00300.png"):
                w, h, a = png_rgb(gl / name)
                w2, h2, b = png_rgb(sw / name)
                self.assertEqual((w, h), (w2, h2))
                mean = sum(abs(x - y) for x, y in zip(a, b)) / len(a)
                self.assertLess(mean, 4.0, name)

    def test_gs_thread_changes_nothing(self):
        """The GS path on its own thread (TTT_GS_ASYNC=1) gives the same state and the same pictures as on the game's
        thread (TTT_GS_ASYNC=0), through the title, the menu and the attract cycle's loading."""
        with tempfile.TemporaryDirectory() as d:
            runs = []
            for on in ("0", "1"):
                out = Path(d) / on
                out.mkdir()
                r = run_ttt(TTT_FRAMES="760", TTT_SEED="1", TTT_GS_ASYNC=on, TTT_DUMP=str(out), TTT_DUMP_EVERY="95")
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                runs.append((r, out))
            self.assertEqual(state(runs[0][0]), state(runs[1][0]))
            self.assertIsNotNone(state(runs[0][0]))
            names = sorted(p.name for p in runs[0][1].iterdir())
            self.assertEqual(len(names), 8)
            self.assertEqual(names, sorted(p.name for p in runs[1][1].iterdir()))
            for name in names:
                self.assertEqual((runs[0][1] / name).read_bytes(), (runs[1][1] / name).read_bytes(), name)


if __name__ == "__main__":
    unittest.main()
