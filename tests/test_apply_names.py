"""Tests for the pure helpers of scripts/apply_names.py (synthetic data only)."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
_spec = importlib.util.spec_from_file_location("apply_names", ROOT / "scripts" / "apply_names.py")
apply_names = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(apply_names)


class Mapping(unittest.TestCase):
    def test_placeholders_to_names(self):
        names = {"InRadius": 0x340AF0, "gTable": 0x521700, "func_00340000": 0x340000}
        self.assertEqual(apply_names.rename_map(names), {
            "func_00340AF0": "InRadius", "D_00340AF0": "InRadius",
            "func_00521700": "gTable", "D_00521700": "gTable",
        })

    def test_rename_text_whole_words_only(self):
        text = ('INCLUDE_ASM("asm/nonmatchings/340000", func_00340AF0);\n'
                "x = func_00340AF0(D_00521700[1]); func_00340AF00 = 1; my_func_00340AF0();\n")
        out = apply_names.rename_text(text, {"func_00340AF0": "InRadius", "D_00521700": "gTable"})
        self.assertEqual(out, 'INCLUDE_ASM("asm/nonmatchings/340000", InRadius);\n'
                              "x = InRadius(gTable[1]); func_00340AF00 = 1; my_func_00340AF0();\n")

    def test_rename_text_ignores_lowercase_hex(self):
        self.assertEqual(apply_names.rename_text("func_00340af0", {"func_00340AF0": "X"}), "func_00340af0")


class SymbolFile(unittest.TestCase):
    TEXT = "// header\nOld = 0x00340AF0; // type:func\ngTable = 0x00521700; // size:0x30\n"

    def test_set_new_entry_keeps_others(self):
        out = apply_names.set_symbol(self.TEXT, "Start", 0x340000, "func")
        self.assertEqual(out, self.TEXT + "Start = 0x00340000; // type:func\n")

    def test_set_renames_existing_address(self):
        out = apply_names.set_symbol(self.TEXT, "InRadius", 0x340AF0, "func")
        self.assertEqual(out, "// header\nInRadius = 0x00340AF0; // type:func\ngTable = 0x00521700; // size:0x30\n")

    def test_set_refuses_name_on_other_address(self):
        with self.assertRaises(ValueError):
            apply_names.set_symbol(self.TEXT, "gTable", 0x340000, "func")

    def test_resolve_old_name(self):
        names = {"Old": 0x340AF0}
        self.assertEqual(apply_names.resolve("func_00340AF0", names), 0x340AF0)
        self.assertEqual(apply_names.resolve("Old", names), 0x340AF0)
        self.assertEqual(apply_names.resolve("0x340AF0", names), 0x340AF0)
        with self.assertRaises(ValueError):
            apply_names.resolve("Unknown", names)

    def test_valid_name(self):
        self.assertTrue(apply_names.valid_name("InRadius_2"))
        for bad in ("2x", "a b", "func_00340AF0", ""):
            self.assertFalse(apply_names.valid_name(bad), bad)


if __name__ == "__main__":
    unittest.main()
