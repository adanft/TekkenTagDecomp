"""The PC build's window (port/src/plat_window.c: SDL3 loaded at run time) and the host calls added with it and the GS
threads, checked on the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py).
Skipped when the program has not been built; the runs are also skipped without disc data, and the SDL run when the
32-bit SDL3 library is not installed. The SDL run uses SDL's dummy video driver: no window appears."""

import re
import subprocess
import unittest
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt

SDL32 = [Path("/usr/lib32/libSDL3.so.0"), Path("/usr/lib/i386-linux-gnu/libSDL3.so.0")]


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class HostCalls(unittest.TestCase):
    def test_new_host_calls_bypass_the_plt(self):
        """The game keeps host code addresses in .sbss, which the state hash covers: the threads and the window call
        libpthread / libdl through the GOT (port_host.h), so the PLT, and every game function's address, stay put."""
        out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", str(EXE)], capture_output=True, text=True,
                             check=True).stdout
        plt = re.findall(r"<(\w+)@plt>:", out)
        self.assertTrue(plt)
        self.assertEqual([n for n in plt if n.startswith(("pthread_", "dl", "sysconf"))], [])


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


if __name__ == "__main__":
    unittest.main()
