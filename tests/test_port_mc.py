"""The PC build's memory cards (port/src/plat_mc.c: libmc over host files under TTT_SAVES), checked on the built program
build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py). Skipped when the program has not been built;
the game runs are also skipped without disc data. Every run uses a temporary saves directory."""

import re
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt
from test_port_input import peek, stream
from test_port_gs import read_png

UP, DOWN, LEFT, RIGHT, START, CROSS = 0x1000, 0x4000, 0x8000, 0x2000, 0x0800, 0x0040
MC_FLAGS = "617374"  # gMc.flags (D_00617300 + 0x74): 1 no card, 2 card changed, 0x200 no save data, 0x8000 busy (added)
OPTION = "439C90"  # the word holding D_00439C92, the AUTO SAVE option byte (byte 2), part of the saved block
MENU = "6173F8"  # the memory card menu (D_006173F8): state, next state, entry, YES chosen, timer, message id (byte 1)
MAIN = "BASLUS-20001TekkenTT"

# Main menu: up to OPTIONS, Start; four times down to MEMORY CARD (PS2), cross: the memory card menu (VBlank ~520).
TO_THE_CARD_MENU = ((200, UP, 4), (230, START, 4), (420, DOWN, 4), (440, DOWN, 4), (460, DOWN, 4), (480, DOWN, 4),
                    (510, CROSS, 4))
# LOAD DATA (the first entry), cross.
LOAD = ((660, CROSS, 4),)
# Down twice to AUTO SAVE, right turns it on, which asks to save at once (YES / NO, NO chosen): left to YES, cross.
AUTO_SAVE_ON = ((600, DOWN, 4), (620, DOWN, 4), (650, RIGHT, 4), (740, LEFT, 4), (770, CROSS, 4))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class McSelfTest(unittest.TestCase):
    def test_mc(self):
        """libmc on a temporary directory: the polls of the asynchronous model, GetInfo's new-card / no-card /
        unformatted answers, mkdir / chdir / open / read / write / close / delete / GetDir with their result codes,
        the atomic write at close, the free space of an 8 MB card, a full card, the open-file limit, paths that try to
        leave the card, format and unformat."""
        with tempfile.TemporaryDirectory(prefix="ttt-mc-") as d:
            r = run_ttt(TTT_SELFTEST="mc", TMPDIR=d)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("ttt: mc self-test OK", r.stdout)
            self.assertEqual(list(Path(d).iterdir()), [])  # cleaned up


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class GameSaves(unittest.TestCase):
    def play(self, saves, frames, presses=(), **env):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*presses))
            r = run_ttt(timeout=300, TTT_FRAMES=str(frames), TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_SAVES=str(saves),
                        TTT_PEEK=f"{MC_FLAGS},{OPTION},{MENU}:6", **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return r

    def option(self, r):
        return (peek(r, OPTION)[0] >> 16) & 0xFF

    def test_boot_check(self):
        """At boot the game checks the card: an empty card (the default) is a new card with no save data (flags 0x202,
        no message at boot, nothing created on the host); without a card (TTT_MC0=none) it is 'no card'. Requests
        finish after a fixed number of polls: with no card and no busy poll the boot keeps the timing it had before
        T35 (main menu at VBlank 177), the empty card's check (GetInfo, then GetDir) adds 6 VBlanks."""
        with tempfile.TemporaryDirectory() as saves:
            r = self.play(saves, 200)
            self.assertEqual(peek(r, MC_FLAGS), [0x202])
            self.assertEqual(self.option(r), 0)
            self.assertRegex(r.stdout, r"ttt: modes: 0@1 2@174 1@178 3@183")
            self.assertEqual(list(Path(saves).iterdir()), [])
            r = self.play(saves, 200, TTT_MC0="none", TTT_MC_POLLS="0")
            self.assertEqual(peek(r, MC_FLAGS)[0] & 1, 1)
            self.assertRegex(r.stdout, r"ttt: modes: 0@1 2@168 1@172 3@177")

    def test_load_with_no_save_data(self):
        """LOAD DATA in OPTIONS > MEMORY CARD on an empty card ends in the error state (6) with message 0xB, the
        'no save data' message the status bit 0x200 selects."""
        with tempfile.TemporaryDirectory() as saves:
            r = self.play(saves, 760, TO_THE_CARD_MENU + LOAD)
            menu = peek(r, MENU)
            self.assertEqual(menu[0], 6)
            self.assertEqual((menu[5] >> 8) & 0xFF, 0xB)

    def test_save_then_load_at_boot(self):
        """Turning AUTO SAVE on in the memory card menu saves at once through the game's own flow (mkdir, icon.sys
        and the three icons from the disc, the 0x480-byte save data): the files appear under mc0/, written atomically
        (no temporary file left), the status is clean. A second run loads the save at boot: the option byte is 1 there
        and 0 after a boot on an empty card."""
        with tempfile.TemporaryDirectory() as saves:
            r = self.play(saves, 1000, TO_THE_CARD_MENU + AUTO_SAVE_ON)
            menu = peek(r, MENU)
            self.assertEqual(peek(r, MC_FLAGS), [0])
            self.assertEqual((menu[0], (menu[5] >> 8) & 0xFF), (2, 0xF))  # the 'saved' message
            d = Path(saves) / "mc0" / MAIN
            names = sorted(p.name for p in d.iterdir())
            self.assertEqual(names, sorted([MAIN, "icon.sys", "xiao2c.ico", "xiao2d.ico", "xiao2n.ico"]))
            self.assertEqual((d / MAIN).stat().st_size, 0x480)
            self.assertFalse([p for p in d.iterdir() if p.name.startswith(".ttt-tmp-")])
            saved = (d / MAIN).read_bytes()

            again = self.play(saves, 260)
            self.assertEqual(self.option(again), 1)
            self.assertEqual(peek(again, MC_FLAGS), [0])
            self.assertEqual((d / MAIN).read_bytes(), saved)  # loading writes nothing
        with tempfile.TemporaryDirectory() as empty:
            self.assertEqual(self.option(self.play(empty, 260)), 0)

    def test_menu_texts_drawn(self):
        """The memory card menu's texts are drawn (T33 b1): the help line at the bottom (Text_DrawId at y 0x186) and
        the message of the message window ('no save data' after LOAD DATA on an empty card). Both were empty: the m2c
        draft of the command printer func_003DFA00 never read its variadic arguments (position, colour, width), and
        Text_LineCount added to an uninitialised width, which made the window two rows high (TTT_DIFF on Mode_Options
        now finds VBlanks 500-1000 of the AUTO SAVE flow equal to the original code). The text is grey (r = g
        = b, about 0xA6); the help bar is near black and the window brown, so grey pixels are text."""
        with tempfile.TemporaryDirectory() as saves, tempfile.TemporaryDirectory() as dump:
            r = self.play(saves, 760, TO_THE_CARD_MENU + LOAD, TTT_DUMP=dump, TTT_DUMP_EVERY="760")
            menu = peek(r, MENU)
            self.assertEqual((menu[0], (menu[5] >> 8) & 0xFF), (6, 0xB))
            w, h, rows = read_png(Path(dump) / "frame_00760.png")
            self.assertEqual((w, h), (640, 448))

            def grey(y0, y1, x0, x1):
                return sum(1 for y in range(y0, y1) for x in range(x0, x1)
                           if rows[y][3 * x] == rows[y][3 * x + 1] == rows[y][3 * x + 2] >= 0x90)

            self.assertGreater(grey(388, 412, 0, 640), 300)  # "Loads game data from MEMORY CARD(PS2)."
            self.assertGreater(grey(222, 256, 76, 564), 600)  # "No data for this game found on MEMORY CARD(PS2)..."


if __name__ == "__main__":
    unittest.main()
