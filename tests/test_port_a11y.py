"""T47 / T48 on the built program (docs/port/accessibility.md, docs/port/translations.md): the screen reader's texts go
to a file (TTT_SR_LOG), never to the speakers.

- The game's announcements (ui.announce_game): the scripted run of tests/test_port_input.py (main menu, character
  select, round 1) says the screens, the main menu's item, the characters and the round, read from the game's memory,
  and its state hash is the one of the same run without them. In Spanish too.
- The overlay in a hidden window (SDL's dummy video driver, TTT_OVERLAY_TEST: made-up events inside the program, nothing
  sent to the desktop) in Spanish: its windows and focused items are said in Spanish, the font has the accented letters.
- Off by default: no file, nothing said.

Skipped without the program, the disc data, or (the overlay) the 32-bit SDL3."""

import re
import tempfile
import unittest
from pathlib import Path

from test_port_input import IN_THE_FIGHT, TO_THE_FIGHT, stream
from test_port_platform import EXE, disc, run_ttt
from test_port_window import SDL32

FIGHT_FRAMES = "1451"


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class GameAnnouncements(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="ttt-a11y-")
        d = Path(cls.tmp.name)
        cls.input = d / "in.txt"
        cls.input.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
        cls.plain = run_ttt(timeout=300, TTT_FRAMES=FIGHT_FRAMES, TTT_SEED="1", TTT_INPUT_PLAY=str(cls.input))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def announce(self, lang):
        log = Path(self.tmp.name) / f"sr-{lang}.txt"
        r = run_ttt(timeout=300, TTT_FRAMES=FIGHT_FRAMES, TTT_SEED="1", TTT_INPUT_PLAY=str(self.input),
                    TTT_SCREEN_READER="1", TTT_ANNOUNCE_GAME="1", TTT_SR_LOG=str(log), TTT_LANG=lang)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertIsNotNone(state(self.plain), self.plain.stdout[-2000:])
        self.assertEqual(state(r), state(self.plain), "the announcements changed the game's state")
        self.assertRegex(r.stdout, r"ttt: screen reader: game announcements \d+, output log")
        return [line for line in log.read_text(encoding="utf-8").splitlines() if line != "--"]

    def test_english(self):
        said = self.announce("en")
        self.assertIn("Main menu", said)
        self.assertIn("Character select", said)
        self.assertIn("Round 1. Fight!", said)
        self.assertTrue(any(re.fullmatch(r"Player 1: [A-Z][A-Z .\-]+", s) for s in said), said)
        # the main menu's items are the game's own strings, read at run time: some text between the two screens
        menu = said[said.index("Main menu") + 1:said.index("Character select")]
        self.assertTrue(menu and all(re.fullmatch(r"[ -~]+", s) for s in menu), said)

    def test_spanish(self):
        said = self.announce("es")
        self.assertIn("Menú principal", said)
        self.assertIn("Selección de personaje", said)
        self.assertIn("Asalto 1. ¡A luchar!", said)
        self.assertTrue(any(s.startswith("Jugador 1: ") for s in said), said)

    def test_off_by_default(self):
        log = Path(self.tmp.name) / "off.txt"
        r = run_ttt(timeout=300, TTT_FRAMES="400", TTT_SEED="1", TTT_INPUT_PLAY=str(self.input), TTT_SR_LOG=str(log),
                    TTT_ANNOUNCE_GAME="1")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertNotIn("screen reader", r.stdout)
        self.assertFalse(log.exists() and log.read_text().strip(), "said something while off")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL32), "no 32-bit SDL3 (Arch: lib32-sdl3)")
class OverlayInSpanish(unittest.TestCase):
    def test_overlay_speaks_spanish(self):
        with tempfile.TemporaryDirectory(prefix="ttt-a11y-ovl-") as d:
            log = Path(d) / "sr.txt"
            r = run_ttt(TTT_FRAMES="400", TTT_SEED="1", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1", TTT_RENDERER="sw",
                        TTT_AUDIO="0", SDL_VIDEO_DRIVER="dummy", TTT_OVERLAY_TEST="1", TTT_LANG="es",
                        TTT_SCREEN_READER="1", TTT_SR_LOG=str(log), TTT_UI_SCALE="150", TTT_HIGH_CONTRAST="1")
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            self.assertNotIn("ImGui check failed", r.stderr)
            m = re.search(r"ttt: overlay: language (\w+), accented glyphs (\d+)/(\d+), screen reader (\d+) texts \((\w+)\)",
                          r.stdout)
            self.assertIsNotNone(m, r.stdout[-2000:])
            self.assertEqual(m.group(1), "es")
            self.assertEqual(m.group(2), m.group(3), "the font lacks accented letters")
            self.assertEqual(m.group(5), "log")
            self.assertRegex(r.stdout, r"pages 8/8")
            said = log.read_text(encoding="utf-8").splitlines()
            self.assertIn("Configuración (F1)", said)
            self.assertIn("¿Salir?", said)
            self.assertIn("Sí, salir", said)
            self.assertIn("No", said)  # the arrow keys moved the focus to the other button
            self.assertGreaterEqual(int(m.group(4)), 4)
            plain = run_ttt(TTT_FRAMES="400", TTT_SEED="1")
            self.assertEqual(state(r), state(plain))


if __name__ == "__main__":
    unittest.main()
