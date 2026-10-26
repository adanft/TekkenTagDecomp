"""The setup (T43, port/src/plat_setup.c, docs/port/install.md): the player's disc checked and the data file made by the
program itself, with `ttt --setup <disc>` or on the setup screen (Dear ImGui, in a hidden window driven from inside by
TTT_SETUP_TEST: no input reaches the desktop), and where the files go (port/src/plat_paths.c).

The real disc is needed for the runs that make a data file (TTT_DISC or iso/, as tests/test_port_data.py); the refusals
use tiny fake discs made here (tests/test_setup_disc.py's ISO 9660 writer). Every run has its data, settings and save
folders in a temporary directory: never the player's.
"""

import hashlib
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt
from test_port_window import NO_SDL, SDL_LIBS
from test_setup_disc import GAME_FILES, make_iso, to_bin

FRAMES = "200"


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


def clean_env(tmp, **env):
    """No TTT_ variable from outside, the per-user folders in `tmp` (XDG_DATA_HOME, XDG_CONFIG_HOME, HOME)."""
    e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
    e.update(XDG_DATA_HOME=str(Path(tmp) / "data"), XDG_CONFIG_HOME=str(Path(tmp) / "config"),
             HOME=str(Path(tmp) / "home"), TTT_MOVIES="0", TTT_AUDIO="0")
    e.update(env)
    return e


def run(exe, tmp, *args, timeout=300, **env):
    return subprocess.run([str(exe), *map(str, args)], env=clean_env(tmp, **env), cwd=tmp, capture_output=True,
                          text=True, timeout=timeout)


def payload_sha1(dat):
    """The payload SHA-1 a data file's header holds (plat_data.c: at 24)."""
    return Path(dat).read_bytes()[24:44].hex()


def fake_disc(where, files=None, kind="iso"):
    """A small ISO 9660 disc in `where`: an .iso, a raw .bin with its .cue, or a folder with lower-case names (as a
    mounted disc shows them). Not the game: its SLUS_200.01 is made up."""
    files = dict(GAME_FILES if files is None else files)
    where = Path(where)
    if kind == "dir":
        d = where / "fake-disc"
        for name, data in files.items():
            p = d / name.lower()
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
        return d
    iso = make_iso(files)
    if kind == "iso":
        p = where / "fake.iso"
        p.write_bytes(iso)
        return p
    (where / "fake.bin").write_bytes(to_bin(iso))
    p = where / "fake.cue"
    p.write_text('FILE "fake.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')
    return p


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class Refusals(unittest.TestCase):
    """`ttt --setup` with a disc that is not the unmodified USA release: refused with the reason and exit code 4, no data
    file written, nothing remembered. Made-up discs: no real one needed."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-setup-")
        self.t = Path(self.tmp.name)
        self.dat = self.t / "out" / "ttt.dat"
        self.cfg = self.t / "settings.ini"

    def tearDown(self):
        self.tmp.cleanup()

    def setup(self, disc_path):
        return run(EXE, self.t, "--setup", disc_path, TTT_DATA=str(self.dat), TTT_CONFIG=str(self.cfg))

    def refused(self, r, code, *texts):
        self.assertEqual(r.returncode, code, r.stdout + r.stderr)
        for t in texts:
            self.assertIn(t, r.stderr)
        self.assertFalse(self.dat.exists())
        self.assertFalse(self.cfg.exists())

    def test_modified_executable_in_every_format(self):
        """The fake disc's SLUS_200.01 differs: found and refused alike in an .iso, a raw .bin / .cue (2352-byte
        sectors) and a folder with lower-case names."""
        want = hashlib.sha1(GAME_FILES["SLUS_200.01"]).hexdigest()
        for kind in ("iso", "cue", "dir"):
            with self.subTest(kind):
                r = self.setup(fake_disc(self.t, kind=kind))
                self.refused(r, 4, "ttt: setup: unknown or modified disc", f"SLUS_200.01 has SHA-1 {want}",
                             "Only the unmodified USA release")

    def test_other_region(self):
        """A disc that boots another executable (another region's release) is named as such."""
        files = {k: v for k, v in GAME_FILES.items() if k != "SLUS_200.01"}
        files["SYSTEM.CNF"] = b"BOOT2 = cdrom0:\\SCES_500.01;1\r\nVER = 1.00\r\nVMODE = PAL\r\n"
        files["SCES_500.01"] = GAME_FILES["SLUS_200.01"]
        r = self.setup(fake_disc(self.t, files))
        self.refused(r, 4, "boots SCES_500.01", "not the USA release")

    def test_another_game(self):
        """An image without TEKKEN.BIN: not this game, with what it boots."""
        files = {"SYSTEM.CNF": b"BOOT2 = cdrom0:\\SLUS_999.99;1\r\n", "SLUS_999.99": b"\x7fELF"}
        r = self.setup(fake_disc(self.t, files))
        self.refused(r, 4, "boots SLUS_999.99 and has no TEKKEN.BIN")

    def test_no_disc(self):
        """A folder with nothing of a disc, and a path that does not exist: exit code 2."""
        empty = self.t / "empty"
        empty.mkdir()
        for p in (empty, self.t / "missing.iso"):
            with self.subTest(p.name):
                self.refused(self.setup(p), 2, "no Tekken Tag Tournament disc at")

    def test_screen_needs_a_window(self):
        """`--setup` without a disc in a run without a window says how to give one."""
        r = run(EXE, self.t, "--setup", TTT_DATA=str(self.dat), TTT_CONFIG=str(self.cfg), TTT_WINDOW="0")
        self.refused(r, 2, "ttt --setup <disc>")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class CommandLineSetup(unittest.TestCase):
    """`ttt --setup <disc>` on a copy of the program outside the repository (an installed program): the data file goes
    to $XDG_DATA_HOME/ttt, the disc is remembered in the settings file, and a later run with no TTT_DISC plays from it
    as the repository's build does. With portable.txt beside the program, both stay beside it."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="ttt-setup-cli-")
        t = Path(cls.tmp.name)
        cls.ref = t / "ref.dat"
        r = run_ttt(TTT_SELFTEST="mem", TTT_DATA=str(cls.ref))  # the data file the program makes on its own
        assert r.returncode == 0, r.stdout + r.stderr
        cls.want = state(run_ttt(TTT_FRAMES=FRAMES, TTT_SEED="1"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def install(self, where, portable=False):
        where.mkdir()
        exe = where / "ttt"
        shutil.copy2(EXE, exe)
        if portable:
            (where / "portable.txt").write_text("")
        return exe

    def test_installed(self):
        t = Path(self.tmp.name) / "installed"
        exe = self.install(t)
        r = run(exe, t, "--setup", disc())
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: setup: done", r.stdout)
        dat = t / "data" / "ttt" / "ttt.dat"
        self.assertEqual(hashlib.sha1(dat.read_bytes()).hexdigest(), hashlib.sha1(self.ref.read_bytes()).hexdigest())
        self.assertTrue(Path(str(dat) + ".disc").exists())  # TEKKEN.BIN's check recorded (T41)
        cfg = t / "config" / "ttt" / "settings.ini"
        self.assertIn(f"game.disc = {disc().resolve()}", cfg.read_text())
        self.assertFalse((t / "ttt.dat").exists())
        r = run(exe, t, TTT_FRAMES=FRAMES, TTT_SEED="1", TTT_SAVES=str(t / "saves"))
        self.assertEqual(state(r), self.want, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertIn("ttt: data: USA release (verified)", r.stdout)

    def test_portable(self):
        t = Path(self.tmp.name) / "portable"
        exe = self.install(t, portable=True)
        r = run(exe, t, "--setup", disc())
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(payload_sha1(t / "ttt.dat"), payload_sha1(self.ref))
        self.assertIn("game.disc = ", (t / "settings.ini").read_text())
        self.assertFalse((t / "data").exists())
        self.assertFalse((t / "config").exists())

    def test_variables_still_win(self):
        """TTT_DATA and TTT_CONFIG over the per-user folders."""
        t = Path(self.tmp.name) / "vars"
        exe = self.install(t)
        r = run(exe, t, "--setup", disc(), TTT_DATA=str(t / "mine.dat"), TTT_CONFIG=str(t / "mine.ini"))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(payload_sha1(t / "mine.dat"), payload_sha1(self.ref))
        self.assertIn("game.disc = ", (t / "mine.ini").read_text())
        self.assertFalse((t / "data").exists())

    def test_no_disc_without_setup(self):
        """Installed, never set up, no disc anywhere, no window: `ttt: no disc` and exit code 2, as before T43."""
        t = Path(self.tmp.name) / "nodisc"
        exe = self.install(t)
        r = run(exe, t, TTT_FRAMES=FRAMES)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn("ttt: no disc: run `ttt --setup <disc>`", r.stderr)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(any(p.exists() for p in SDL_LIBS), NO_SDL)
class SetupScreen(unittest.TestCase):
    """The setup screen in a hidden window (SDL's offscreen driver), driven from inside by TTT_SETUP_TEST=<disc>: it
    lists the disc's folder in the built-in browser, types the path and presses "Check and install"."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-setup-screen-")
        self.t = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def screen(self, disc_path, renderer, **env):
        return run(EXE, self.t, TTT_SETUP_TEST=str(disc_path), TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1",
                   TTT_RENDERER=renderer, SDL_VIDEO_DRIVER="offscreen", TTT_DATA=str(self.t / "ttt.dat"),
                   TTT_CONFIG=str(self.t / "settings.ini"), TTT_SAVES=str(self.t / "saves"), **env)

    def report(self, r):
        m = re.search(r"ttt: setup: screen test: (\d+) frames drawn \((\w+)\), browser (-?\d+) entries, error shown "
                      r"(\d+) frames", r.stdout)
        self.assertIsNotNone(m, r.stdout[-2000:] + r.stderr[-2000:])
        return int(m.group(1)), m.group(2), int(m.group(3)), int(m.group(4))

    def test_refused_disc_shows_why(self):
        """A fake disc: the screen draws, the browser lists the folder holding it, the reason is shown, and the test
        run ends with exit code 4; nothing is written. With both renderers."""
        fake = fake_disc(self.t)
        for renderer in ("gl", "sw"):
            with self.subTest(renderer):
                r = self.screen(fake, renderer, TTT_FRAMES="100")
                if renderer == "gl" and "OpenGL renderer" not in r.stdout:
                    continue  # no GL context here: the software renderer's case still runs
                self.assertEqual(r.returncode, 4, r.stdout[-2000:] + r.stderr[-2000:])
                frames, drawn, entries, errors = self.report(r)
                self.assertEqual(drawn, renderer)
                self.assertGreaterEqual(frames, 8)
                self.assertGreaterEqual(entries, 1)  # fake.iso, at least
                self.assertGreaterEqual(errors, 3)
                self.assertIn("ttt: setup: unknown or modified disc", r.stderr)
                self.assertFalse((self.t / "ttt.dat").exists())

    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_installs_and_starts_the_game(self):
        """The real disc: the data file is made, the disc remembered, and the game runs on to the same state as a run
        given the disc by TTT_DISC."""
        want = state(run_ttt(TTT_FRAMES=FRAMES, TTT_SEED="1", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1",
                             TTT_RENDERER="sw", TTT_AUDIO="0", SDL_VIDEO_DRIVER="offscreen"))
        r = self.screen(disc(), "sw", TTT_FRAMES=FRAMES, TTT_SEED="1")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertIn("ttt: setup: done", r.stdout)
        frames, drawn, entries, errors = self.report(r)
        self.assertGreaterEqual(frames, 8)
        self.assertGreaterEqual(entries, 1)
        self.assertEqual(errors, 0)
        self.assertTrue((self.t / "ttt.dat").exists())
        self.assertIn("game.disc = ", (self.t / "settings.ini").read_text())
        self.assertEqual(state(r), want)


if __name__ == "__main__":
    unittest.main()
