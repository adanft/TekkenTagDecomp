"""Tests for the pure helpers of scripts/apply_names.py (synthetic data only)."""

import importlib.util
import io
import os
import re
import sys
import tempfile
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


class RenameIdentifier(unittest.TestCase):
    def test_whole_identifiers_only(self):
        text = "Old(); Old2(); xOld(); my_Old(); p = &Old;\n"
        self.assertEqual(apply_names.rename_identifier(text, "Old", "New"),
                         "New(); Old2(); xOld(); my_Old(); p = &New;\n")

    def test_skips_members_strings_and_chars(self):
        text = 's.Old = p->Old + Old; printf("Old %d", Old); c = \'O\';\n'
        self.assertEqual(apply_names.rename_identifier(text, "Old", "New"),
                         's.Old = p->Old + New; printf("Old %d", New); c = \'O\';\n')

    def test_include_asm_argument(self):
        text = 'INCLUDE_ASM("asm/nonmatchings/340000", Old);\n'
        self.assertEqual(apply_names.rename_identifier(text, "Old", "New"),
                         'INCLUDE_ASM("asm/nonmatchings/340000", New);\n')


class Routing(unittest.TestCase):
    FILES = {
        "config/symbols/game_a.txt": "// range: 0x00340000 0x00341000\n// range: 0x00342000 0x00343000\n",
        "config/symbols/libx.txt": "// range: 0x00341000 0x00342000\nLibFn = 0x00341000; // type:func\n",
    }

    def test_routes_by_range(self):
        ranges = apply_names.file_ranges(self.FILES)
        self.assertEqual(apply_names.route(0x340000, ranges), "config/symbols/game_a.txt")
        self.assertEqual(apply_names.route(0x342FFC, ranges), "config/symbols/game_a.txt")
        self.assertEqual(apply_names.route(0x341FFC, ranges), "config/symbols/libx.txt")
        with self.assertRaises(ValueError):
            apply_names.route(0x343000, ranges)

    def test_overlapping_ranges_refused(self):
        files = dict(self.FILES, **{"config/symbols/bad.txt": "// range: 0x00340800 0x00340900\n"})
        with self.assertRaisesRegex(ValueError, "overlap"):
            apply_names.file_ranges(files)

    def test_misplaced_entries(self):
        files = dict(self.FILES)
        files["config/symbols/game_a.txt"] += "Wrong = 0x00341800; // type:func\n"
        self.assertEqual(apply_names.misplaced(files), [("config/symbols/game_a.txt", "Wrong", 0x341800)])
        self.assertEqual(apply_names.misplaced(self.FILES), [])


class WriteAtomic(unittest.TestCase):
    def test_replaces_content_and_leaves_no_temp(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "f.txt"
            path.write_text("old\n")
            os.chmod(path, 0o640)
            apply_names.write_atomic(path, "new\n")
            self.assertEqual(path.read_text(), "new\n")
            self.assertEqual(os.listdir(tmp), ["f.txt"])
            self.assertEqual(path.stat().st_mode & 0o777, 0o640)


class Apply(unittest.TestCase):
    """apply() on a synthetic project: symbol files, src/ and include/."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        sym = self.root / "config" / "symbols"
        sym.mkdir(parents=True)
        (sym / "game_a.txt").write_text("// game_a\n// range: 0x00340000 0x00341000\n"
                                        "Old = 0x00340100; // type:func // guess\n")
        (sym / "data.txt").write_text("// range: 0x00415700 0x02000000\n")
        (self.root / "src").mkdir()
        (self.root / "include").mkdir()
        self.c = self.root / "src" / "340000.c"
        self.c.write_text('INCLUDE_ASM("asm/nonmatchings/340000", func_00340200);\n'
                          "void f(void) { Old(); s.Old = 1; OldX(); }\n")
        self.h = self.root / "include" / "a.h"
        self.h.write_text("void Old(void);\n")

    def tearDown(self):
        self.tmp.cleanup()

    def run_apply(self, *args, **kw):
        out = io.StringIO()
        apply_names.apply(self.root, *args, out=out, **kw)
        return out.getvalue()

    def read(self, name):
        return (self.root / "config" / "symbols" / name).read_text()

    def test_new_name_goes_to_its_range_file(self):
        self.run_apply("func_00340200", "DoThing")
        self.assertIn("DoThing = 0x00340200; // type:func\n", self.read("game_a.txt"))
        self.assertIn('INCLUDE_ASM("asm/nonmatchings/340000", DoThing);', self.c.read_text())

    def test_data_name_goes_to_data_file(self):
        self.run_apply("D_00500000", "gTable")
        self.assertEqual(self.read("data.txt"), "// range: 0x00415700 0x02000000\ngTable = 0x00500000;\n")

    def test_address_after_text_is_data(self):
        self.run_apply("0x500000", "gTable")
        self.assertIn("gTable = 0x00500000;\n", self.read("data.txt"))

    def test_written_types_are_splat_types(self):
        """splat 0.50 stops on a `type:` it does not know (`type:data` included)."""
        from splat.util import symbols as splat_symbols
        import spimdisasm

        known = splat_symbols.splat_sym_types | spimdisasm.common.gKnownTypes
        self.run_apply("func_00340200", "DoThing")
        self.run_apply("D_00500000", "gTable")
        for name in ("game_a.txt", "data.txt"):
            for kind in re.findall(r"type:(\S+)", self.read(name)):
                self.assertTrue(kind in known or kind[0].isupper(), f"{name}: type:{kind}")

    def test_rename_keeps_attributes_and_scopes_old_name(self):
        self.run_apply("Old", "New")
        self.assertIn("New = 0x00340100; // type:func // guess\n", self.read("game_a.txt"))
        self.assertNotIn("Old =", self.read("game_a.txt"))
        self.assertIn("void f(void) { New(); s.Old = 1; OldX(); }", self.c.read_text())
        self.assertEqual(self.h.read_text(), "void New(void);\n")

    def test_round_trip(self):
        before = {p: p.read_text() for p in self.root.rglob("*") if p.is_file()}
        self.run_apply("Old", "New")
        self.run_apply("New", "Old")
        self.assertEqual({p: p.read_text() for p in self.root.rglob("*") if p.is_file()}, before)

    def test_address_outside_every_range_refused(self):
        with self.assertRaisesRegex(ValueError, "0x00341000"):
            self.run_apply("0x341000", "Nowhere")
        self.assertNotIn("Nowhere", self.read("game_a.txt"))

    def test_name_already_used_elsewhere_refused(self):
        with self.assertRaisesRegex(ValueError, "Old"):
            self.run_apply("func_00340200", "Old")

    def test_dry_run_writes_nothing(self):
        before = {p: p.read_text() for p in self.root.rglob("*") if p.is_file()}
        self.run_apply("func_00340200", "DoThing", dry_run=True)
        self.assertEqual({p: p.read_text() for p in self.root.rglob("*") if p.is_file()}, before)


if __name__ == "__main__":
    unittest.main()
