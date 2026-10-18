"""Tests for the pure helpers of scripts/progress.py (synthetic data only)."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
_spec = importlib.util.spec_from_file_location("progress", ROOT / "scripts" / "progress.py")
progress = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(progress)


class Ranges(unittest.TestCase):
    def test_parse(self):
        text = "# library code\n0x350000 0x351000 libgraph\n\n0x360000 0x360100  # no label\n"
        self.assertEqual(progress.parse_ranges(text), [(0x350000, 0x351000), (0x360000, 0x360100)])

    def test_parse_refuses_bad_lines(self):
        for bad in ("0x10\n", "0x20 0x10\n", "zzz 0x10\n"):
            with self.assertRaises(ValueError):
                progress.parse_ranges(bad)

    def test_in_ranges_is_half_open(self):
        ranges = [(0x10, 0x20)]
        self.assertTrue(progress.in_ranges(0x10, ranges))
        self.assertFalse(progress.in_ranges(0x20, ranges))
        self.assertFalse(progress.in_ranges(0x0F, ranges))


class Tally(unittest.TestCase):
    SYMS = {
        "func_A": (0x100, 0x40, "FUNC"),
        "func_B": (0x140, 0x20, "FUNC"),
        "func_B.NON_MATCHING": (0x140, 0x20, "OBJECT"),
        "func_C": (0x200, 0x10, "FUNC"),
        "func_C.NON_MATCHING": (0x200, 0x10, "OBJECT"),
        "D_300": (0x300, 4, "OBJECT"),
    }

    def test_all_code_without_ranges(self):
        game, lib = progress.tally(self.SYMS, [])
        self.assertEqual(game, {"funcs": 3, "funcs_c": 1, "bytes": 0x70, "bytes_c": 0x40})
        self.assertEqual(lib, {"funcs": 0, "funcs_c": 0, "bytes": 0, "bytes_c": 0})

    def test_library_ranges_split(self):
        game, lib = progress.tally(self.SYMS, [(0x200, 0x300)])
        self.assertEqual(game, {"funcs": 2, "funcs_c": 1, "bytes": 0x60, "bytes_c": 0x40})
        self.assertEqual(lib, {"funcs": 1, "funcs_c": 0, "bytes": 0x10, "bytes_c": 0})

    def test_entry_points_inside_functions_not_counted(self):
        syms = dict(self.SYMS, func_ALT=(0x150, 0, "FUNC"))
        self.assertEqual(progress.tally(syms, [])[0]["funcs"], 3)

    def test_percent(self):
        self.assertEqual(progress.percent(1, 4), 25.0)
        self.assertEqual(progress.percent(0, 0), 0.0)
        self.assertAlmostEqual(progress.percent(2, 3), 66.6667, places=3)


if __name__ == "__main__":
    unittest.main()
