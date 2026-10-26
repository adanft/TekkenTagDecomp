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
        # the settings file and the overlay (T36): their C library calls go through the GOT too (-fno-plt), and no C++
        # run time is linked
        overlay = {"qsort", "vsnprintf", "strncmp", "strchr", "strncpy", "memchr", "localtime_r", "time", "setenv",
                   "unsetenv", "mkdir", "access", "strerror", "atexit", "__cxa_atexit",
                   # the texture packs (T39, gs_texpack.c with stb_image / stb_image_write: the C library functions
                   # only it calls; the others it shares with older code stay among the 48 entries, some now in .plt.got)
                   "opendir", "readdir", "closedir", "stat",
                   # the crash reports and the background check of TEKKEN.BIN (T41, plat_crash.c, plat_data.c)
                   "sigaction", "sigaltstack", "syscall", "raise", "abort", "open", "read", "write", "close", "statx",
                   # the setup (T43, plat_setup.c, plat_file.c, plat_paths.c)
                   "realpath"}
        self.assertEqual(sorted(set(plt) & overlay), [])
        self.assertEqual([n for n in plt if n.startswith(("_Z", "__cxa", "__gxx"))], [])

    def test_window_keys(self):
        """The keyboard's default mapping and the quit keys on made-up SDL events (TTT_SELFTEST=window): a Return tap
        between two VBlanks is pad 1's Start once, keypad Enter pad 2's, Escape (by key code or by place, not its key
        repeat) and the close button ask to end the program, Return does not."""
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


OVERLAY = re.compile(r"ttt: overlay: (\d+) frames drawn \((gl|sw)\), pages (\d)/7, key capture (\d) "
                     r"\(pad 1 square: (\w+)\)")
WINDOW_TEST = "ttt: window: overlay test: F1 opens 1, presses blocked 1, Escape asks 1"


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
class Overlay(unittest.TestCase):
    """The settings overlay (T36) in a hidden window, driven from inside (TTT_OVERLAY_TEST=1: made-up key events through
    the window's own handling, nothing sent to the desktop): F1 opens it, every page is drawn, a key pressed while it is
    open does not reach the pads, a key is bound through the "press a key" path (and written to the settings file),
    Escape asks "Quit?" instead of quitting; the game's state is the one of a run without a window."""

    def check(self, renderer, driver, frames):
        with tempfile.TemporaryDirectory(prefix="ttt-overlay-") as d:
            cfg = Path(d) / "settings.ini"
            r = run_ttt(TTT_FRAMES=frames, TTT_SEED="1", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER=renderer,
                        TTT_AUDIO="0", SDL_VIDEO_DRIVER=driver, TTT_OVERLAY_TEST="1", TTT_CONFIG=str(cfg))
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            if renderer == "gl" and "OpenGL renderer" not in r.stdout:
                self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
            m = OVERLAY.search(r.stdout)
            self.assertIsNotNone(m, r.stdout[-2000:] + r.stderr[-2000:])
            self.assertEqual(m.group(2), renderer)
            self.assertGreater(int(m.group(1)), 40)
            self.assertEqual((m.group(3), m.group(4), m.group(5)), ("7", "1", "Space"))
            self.assertIn(WINDOW_TEST, r.stdout)
            self.assertNotIn("ImGui check failed", r.stderr)
            self.assertIn("keys.p1.square = Space\n", cfg.read_text())
            # T34: a gamepad binding through the capture path (a made-up gamepad event), saved by name
            self.assertIn("ttt: overlay: gamepad capture 1 (pad 2 cross: north)", r.stdout)
            self.assertIn("pad.p2.cross = north\n", cfg.read_text())
            plain = run_ttt(TTT_FRAMES=frames, TTT_SEED="1")
            self.assertIsNotNone(state(plain), plain.stdout[-2000:])
            self.assertEqual(state(r), state(plain))

    def test_overlay_gl(self):
        """OpenGL (SDL's offscreen driver; skipped without an OpenGL 3.3 context). Headless, the GL back end shows its
        first pictures after about 650 VBlanks."""
        self.check("gl", "offscreen", "900")

    def test_overlay_sw(self):
        """The software renderer through SDL's renderer (dummy video driver)."""
        self.check("sw", "dummy", "400")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
class RealTime(unittest.TestCase):
    """The game's pace on the real-time clock (59.94 VBlanks a second of wall time), GL in a hidden window (SDL's
    offscreen driver): the scripted run of tests/test_port_input.py (main menu, character select, stage load, round 1)
    makes one game frame per VBlank as headless, where the same 1000 VBlanks make 950 frames (the main loop waits for
    one VBlank per frame; the loads hold it longer). Before the fix the polls inside a frame slept and the character
    select and the fight made about one frame in five VBlanks. About 20 s."""

    VBLANKS, HEADLESS_FRAMES = 1000, 950

    def test_real_time_keeps_the_game_frame_rate(self):
        from test_port_input import IN_THE_FIGHT, TO_THE_FIGHT, stream

        with tempfile.TemporaryDirectory(prefix="ttt-rt-") as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
            r = run_ttt(TTT_FRAMES=str(self.VBLANKS), TTT_HEADLESS="0", TTT_SEED="1", TTT_INPUT_PLAY=str(p),
                        TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER="gl", TTT_AUDIO="0", TTT_VSYNC="0",
                        SDL_VIDEO_DRIVER="offscreen")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        if "OpenGL renderer" not in r.stdout:
            self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
        m = re.search(r"ttt: real time: (\d+) vblanks, (\d+) frames", r.stdout)
        self.assertIsNotNone(m, r.stdout[-2000:])
        self.assertEqual(int(m.group(1)), self.VBLANKS)
        self.assertGreaterEqual(int(m.group(2)), self.HEADLESS_FRAMES * 9 // 10, r.stdout[-2000:])
        self.assertRegex(re.search(r"ttt: modes: (.*)", r.stdout).group(1), r"\b5@\d+ 4@\d+$")  # in the fight



@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
class Pacing(unittest.TestCase):
    """T38 (docs/port/video.md, Frame pacing): the intervals between the window's presents in real time over round 1
    of the scripted fight (VBlanks 1101-1451, TTT_PACE_FROM), GL in a hidden window (SDL's offscreen driver, so the
    swap waits for no display). Each picture is presented one VBlank (16.683 ms) after the last: the median is checked
    to 0.03 ms (the histogram's bins are 20 us wide), which catches a clock off by 0.2 % (before T38 b2 the real-time
    clock ran at 59.76 Hz: median 16.73 ms). The tails allow host stalls (a CPU quota's throttling pauses every thread
    for up to 100 ms): p95 under 1.5 VBlanks, at most a tenth of the intervals longer. About 25 s."""

    VBLANKS, FROM = 1451, 1101

    def test_presents_follow_the_vblanks(self):
        from test_port_input import IN_THE_FIGHT, TO_THE_FIGHT, stream

        with tempfile.TemporaryDirectory(prefix="ttt-pace-") as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
            r = run_ttt(TTT_FRAMES=str(self.VBLANKS), TTT_HEADLESS="0", TTT_SEED="1", TTT_INPUT_PLAY=str(p),
                        TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER="gl", TTT_AUDIO="0", TTT_VSYNC="0",
                        TTT_PACE_FROM=str(self.FROM), SDL_VIDEO_DRIVER="offscreen")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        if "OpenGL renderer" not in r.stdout:
            self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
        m = re.search(r"ttt: gl: pacing of presents from VBlank (\d+): (\d+) intervals, mean ([\d.]+) p50 ([\d.]+) "
                      r"p95 ([\d.]+) p99 ([\d.]+) max ([\d.]+) ms, (\d+) over 1.5 VBlanks", r.stdout)
        self.assertIsNotNone(m, r.stdout[-2000:])
        n, p50, p95, late = int(m.group(2)), float(m.group(4)), float(m.group(5)), int(m.group(8))
        self.assertEqual(int(m.group(1)), self.FROM)
        span = self.VBLANKS - self.FROM
        self.assertGreaterEqual(n, span * 9 // 10, m.group(0))
        self.assertAlmostEqual(p50, 16.683, delta=0.03, msg=m.group(0))
        self.assertLess(p95, 25.0, m.group(0))
        self.assertLessEqual(late, span // 10, m.group(0))
        self.assertRegex(r.stdout, r"ttt: gl: pacing: \d+ VBlank pictures, \d+ without a new game frame")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
class GameSpeed(unittest.TestCase):
    """T40 (docs/port/settings.md, Game speed): game.speed scales the real-time clock. The idle run's VBlanks 300-900
    (the attract sequence, GL in a hidden window, TTT_TIME_FROM) take 600 / 59.94 = 10.0 s of wall time at 1x: at 2x
    about 5 s (the same game frames, twice as many a wall second), at 0.5x about 20 s. The bounds allow the measuring
    run's own costs and host stalls (a CPU quota's throttling). About 30 s."""

    def wall_ms(self, speed):
        r = run_ttt(timeout=300, TTT_FRAMES="900", TTT_TIME_FROM="300", TTT_HEADLESS="0", TTT_SEED="1", TTT_WINDOW="1",
                    TTT_WINDOW_HIDDEN="1", TTT_RENDERER="gl", TTT_AUDIO="0", TTT_VSYNC="0", TTT_SPEED=speed,
                    SDL_VIDEO_DRIVER="offscreen")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        if "OpenGL renderer" not in r.stdout:
            self.skipTest("no OpenGL 3.3 context here: " + r.stdout[-500:])
        self.assertIn(f"ttt: speed: {speed}% at the end", r.stdout)
        frames = int(re.search(r"ttt: real time: 900 vblanks, (\d+) frames", r.stdout).group(1))
        self.assertGreater(frames, 800, r.stdout[-2000:])  # one game frame a VBlank, as at 1x
        return int(re.search(r"ttt: time: vblanks 300-900 in (\d+) ms", r.stdout).group(1))

    def test_fast_forward(self):
        ms = self.wall_ms("200")
        self.assertGreaterEqual(ms, 4500)
        self.assertLessEqual(ms, 6500)

    def test_slow_motion(self):
        ms = self.wall_ms("50")
        self.assertGreaterEqual(ms, 19000)
        self.assertLessEqual(ms, 23000)


if __name__ == "__main__":
    unittest.main()
