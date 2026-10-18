"""Tests for the pure helpers of scripts/fdiff.py (synthetic data only, no disc or build needed)."""

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
_spec = importlib.util.spec_from_file_location("fdiff", ROOT / "scripts" / "fdiff.py")
fdiff = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(fdiff)

READELF = """\
Symbol table '.symtab' contains 6 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     7: 00000000     0 FILE    LOCAL  DEFAULT  ABS src/a.c
  9426: 00340070   804 FUNC    GLOBAL DEFAULT    2 func_00340070
 15793: 00340070   804 OBJECT  GLOBAL DEFAULT    2 func_00340070.NON_MATCHING
 17246: 00340af0    60 FUNC    GLOBAL DEFAULT    2 DoThing
 17300: 00350000 0x1a2b0 FUNC   GLOBAL DEFAULT    2 BigOne
"""

OBJDUMP = """\
blob.bin:     file format binary


Disassembly of section .data:

00340000 <.data>:
  340000:\t24030030 \tli\tv1,48
  340004:\t1465000f \tbne\tv1,a1,0x340044
  340008:\t0c0d5c96 \tjal\t0x357258
  34000c:\t00000000 \tnop
"""


class Symbols(unittest.TestCase):
    def test_functions_from_readelf(self):
        syms = fdiff.parse_symbols(READELF)
        self.assertEqual(syms["func_00340070"], (0x340070, 804, "FUNC"))
        self.assertEqual(syms["func_00340070.NON_MATCHING"], (0x340070, 804, "OBJECT"))
        self.assertEqual(syms["DoThing"], (0x340AF0, 60, "FUNC"))
        self.assertEqual(syms["BigOne"], (0x350000, 0x1A2B0, "FUNC"))
        self.assertNotIn("src/a.c", syms)

    def test_functions_skip_entry_points(self):
        syms = dict(fdiff.parse_symbols(READELF), func_00340100=(0x340100, 0, "FUNC"))
        self.assertEqual(set(fdiff.functions(syms)), {"func_00340070", "DoThing", "BigOne"})
        self.assertEqual(fdiff.functions(syms)["DoThing"], (0x340AF0, 60))

    def test_placeholder_address(self):
        self.assertEqual(fdiff.placeholder_addr("func_00340AF0"), 0x340AF0)
        self.assertEqual(fdiff.placeholder_addr("D_005F6780"), 0x5F6780)
        self.assertIsNone(fdiff.placeholder_addr("DoThing"))
        self.assertIsNone(fdiff.placeholder_addr("func_0034"))

    def test_symbol_addrs_file(self):
        text = "// comment\nDoThing = 0x00340AF0; // type:func\n\nbad line\ngTable = 0x521700; // size:0x30\n"
        self.assertEqual(fdiff.parse_symbol_addrs(text), {"DoThing": 0x340AF0, "gTable": 0x521700})

    def test_file_ranges(self):
        text = "// libx: what\n// range: 0x00340000 0x00340100\n//range:0x400000 0x400010\nA = 0x00340000;\n"
        self.assertEqual(fdiff.parse_file_ranges(text), [(0x340000, 0x340100), (0x400000, 0x400010)])
        with self.assertRaises(ValueError):
            fdiff.parse_file_ranges("// range: 0x00340100 0x00340000\n")

    def test_symbol_files_and_known_names(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / fdiff.SYMBOL_DIR).mkdir(parents=True)
            (root / fdiff.SYMBOL_DIR / "b.txt").write_text("B = 0x00400000; // type:func\n")
            (root / fdiff.SYMBOL_DIR / "a.txt").write_text("A = 0x00340000; // type:func\n")
            (root / fdiff.SYMBOL_DIR / "notes.md").write_text("C = 0x00500000;\n")
            self.assertEqual(fdiff.symbol_files(root), ["config/symbols/a.txt", "config/symbols/b.txt"])
            self.assertEqual(fdiff.known_names(root), {"A": 0x340000, "B": 0x400000})

    def test_address_argument(self):
        self.assertEqual(fdiff.parse_address("0x340AF0"), 0x340AF0)
        self.assertEqual(fdiff.parse_address("00340af0"), 0x340AF0)
        self.assertIsNone(fdiff.parse_address("func_00340AF0"))

    def test_original_end_is_next_start(self):
        starts = [0x340000, 0x340070, 0x340AF0]
        self.assertEqual(fdiff.original_end(starts, 0x340000, 0x6C), 0x340070)
        self.assertEqual(fdiff.original_end(starts, 0x340AF0, 0x3C), 0x340B2C)  # last: own size


class Disassembly(unittest.TestCase):
    def test_parse_objdump(self):
        rows = fdiff.parse_objdump(OBJDUMP)
        self.assertEqual(rows[0], (0x340000, 0x24030030, "li\tv1,48"))
        self.assertEqual(rows[3], (0x34000C, 0, "nop"))
        self.assertEqual(len(rows), 4)

    def test_normalize_branch_inside_is_relative(self):
        self.assertEqual(fdiff.normalize("bne\tv1,a1,0x340044", 0x340000, 0x48, {}), "bne\tv1,a1,.+0x44")

    def test_normalize_call_uses_name(self):
        names = {0x357258: "func_00357258"}
        self.assertEqual(fdiff.normalize("jal\t0x357258", 0x340000, 0x48, names), "jal\tfunc_00357258")
        self.assertEqual(fdiff.normalize("jal\t0x400000", 0x340000, 0x48, names), "jal\t0x400000")

    def test_normalize_leaves_other_instructions(self):
        self.assertEqual(fdiff.normalize("lui\tv0,0x52", 0x340000, 0x48, {}), "lui\tv0,0x52")
        self.assertEqual(fdiff.normalize("jr\tra", 0x340000, 0x48, {}), "jr\tra")

    def test_strip_padding_only_while_longer(self):
        orig = [(0, 1, "a"), (4, 0, "nop"), (8, 0, "nop")]
        self.assertEqual(fdiff.strip_padding(orig, 2), orig[:2])
        self.assertEqual(fdiff.strip_padding(orig, 3), orig)
        self.assertEqual(fdiff.strip_padding([(0, 5, "x"), (4, 6, "y")], 1), [(0, 5, "x"), (4, 6, "y")])


class Align(unittest.TestCase):
    def row(self, word, text):
        return (0, word, text)

    def test_identical(self):
        a = [self.row(1, "li\tv1,48"), self.row(2, "jr\tra")]
        rows = fdiff.align(a, a, lambda r: r[2], lambda r: r[2])
        self.assertEqual([m for m, _, _ in rows], [" ", " "])

    def test_relocation_only_difference(self):
        a = [self.row(1, "jal\t0x1000")]
        b = [self.row(2, "jal\t0x2000")]
        rows = fdiff.align(a, b, lambda r: "jal\tf", lambda r: "jal\tf")
        self.assertEqual(rows[0][0], "~")

    def test_changed_inserted_deleted(self):
        a = [self.row(1, "a"), self.row(2, "b"), self.row(3, "c"), self.row(4, "d")]
        b = [self.row(1, "a"), self.row(9, "x"), self.row(3, "c"), self.row(4, "d"), self.row(5, "e")]
        rows = fdiff.align(a, b, lambda r: r[2], lambda r: r[2])
        self.assertEqual([m for m, _, _ in rows], [" ", "!", " ", " ", ">"])
        self.assertIsNone(rows[4][1])

    def test_deleted_line_keeps_alignment(self):
        a = [self.row(1, "a"), self.row(2, "b"), self.row(3, "c")]
        b = [self.row(1, "a"), self.row(3, "c")]
        rows = fdiff.align(a, b, lambda r: r[2], lambda r: r[2])
        self.assertEqual([m for m, _, _ in rows], [" ", "<", " "])
        self.assertIsNone(rows[1][2])


if __name__ == "__main__":
    unittest.main()
