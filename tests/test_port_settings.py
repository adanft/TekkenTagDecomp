"""The player's settings (T36, port/src/plat_settings.c, docs/port/settings.md), checked on the built program
build/port/ttt: the file's round trip (comments and unknown keys kept, written through a temporary file), the
environment's precedence, key names and rebinding (TTT_SELFTEST=settings), and a file value reaching a run as its
environment variable's default. Skipped when the program has not been built; the run also without disc data."""

import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class SettingsSelfTest(unittest.TestCase):
    def test_settings_self_test(self):
        with tempfile.TemporaryDirectory(prefix="ttt-settings-") as d:
            cfg = Path(d) / "sub" / "settings.ini"
            cfg.parent.mkdir()
            r = run_ttt(TTT_SELFTEST="settings", TTT_CONFIG=str(cfg))
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("ttt: settings self-test OK", r.stdout)
            self.assertEqual(sorted(p.name for p in cfg.parent.iterdir()), ["settings.ini"])  # no temporary file left

    def test_default_path_follows_xdg(self):
        """Without TTT_CONFIG the file is $XDG_CONFIG_HOME/ttt/settings.ini; a missing one is the defaults, and a run
        that changes nothing writes nothing."""
        with tempfile.TemporaryDirectory(prefix="ttt-xdg-") as d:
            env = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
            env.update(XDG_CONFIG_HOME=d, TTT_SELFTEST="pad")
            r = subprocess.run([str(EXE)], env=env, capture_output=True, text=True, timeout=60)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertEqual(list(Path(d).iterdir()), [])


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class SettingsReachTheRun(unittest.TestCase):
    FRAMES = "600"

    def run_movies(self, cfg_text, movies_env):
        """A headless run whose TTT_MOVIES comes from the settings file unless `movies_env` sets it."""
        with tempfile.TemporaryDirectory(prefix="ttt-settings-run-") as d:
            cfg = Path(d) / "settings.ini"
            cfg.write_text(cfg_text)
            env = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
            env.update(TTT_DISC=str(disc()), TTT_SAVES=d, TTT_CONFIG=str(cfg), TTT_FRAMES=self.FRAMES, TTT_SEED="1")
            if movies_env is not None:
                env["TTT_MOVIES"] = movies_env
            r = subprocess.run([str(EXE)], env=env, capture_output=True, text=True, timeout=300)
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            self.assertIsNotNone(state(r), r.stdout[-2000:])
            return state(r)

    def test_file_value_is_the_default_and_the_environment_wins(self):
        off_by_env = self.run_movies("", "0")
        self.assertEqual(self.run_movies("game.movies = 0\n", None), off_by_env)
        self.assertNotEqual(self.run_movies("game.movies = 0\n", "1"), off_by_env)


if __name__ == "__main__":
    unittest.main()
