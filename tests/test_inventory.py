"""Tests for the pure helpers of scripts/inventory.py (synthetic data only)."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
_spec = importlib.util.spec_from_file_location("inventory", ROOT / "scripts" / "inventory.py")
inventory = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(inventory)

SOURCE = """\
#include "common.h"

/* Header: every NON_MATCHING draft compiles. */

void func_A(void) {
}

INCLUDE_ASM("asm/nonmatchings/x", func_B);

/* NON_MATCHING: 12 lines: register allocation of the loop counter
 * (s1 / s2 swapped). */
#if 0
void func_C(void) {
}
#else
INCLUDE_ASM("asm/nonmatchings/x", func_C);
#endif

#if 0
/* NON_MATCHING: 560 lines: m2c draft */
void func_D(void) {
}
#else
LIT4_WORD(D_005F5000, 0x3F800000);
INCLUDE_ASM("asm/nonmatchings/x", func_D);
#endif

/* NON_MATCHING: 5 lines: store order of the resets */
#if 0
/* NON_MATCHING-draft body: what it computes. */
void func_F(void) {
}
#else
INCLUDE_ASM("asm/nonmatchings/x", func_F);
#endif

#if 0
void func_E(void) {
}
#else
INCLUDE_ASM("asm/nonmatchings/x", func_E);
#endif
"""


class ScanSource(unittest.TestCase):
    def test_kinds_and_notes(self):
        got = inventory.scan_source(SOURCE)
        self.assertEqual(got["func_B"], ("asm", None))
        self.assertEqual(got["func_C"][0], "draft")
        self.assertIn("register allocation", got["func_C"][1])
        self.assertIn("s1 / s2 swapped", got["func_C"][1])
        self.assertEqual(got["func_D"][0], "draft")
        self.assertIn("m2c draft", got["func_D"][1])
        self.assertEqual(got["func_E"], ("draft", ""))
        self.assertIn("store order", got["func_F"][1])
        self.assertNotIn("func_A", got)

    def test_header_note_not_taken(self):
        # The file header mentions NON_MATCHING but belongs to no block.
        self.assertNotIn("Header", inventory.scan_source(SOURCE)["func_C"][1])


class Cause(unittest.TestCase):
    def test_causes(self):
        c = inventory.classify_note
        self.assertEqual(c("560 lines: m2c draft (switch trees)"), "m2c rough draft")
        self.assertEqual(c("first draft; packet building order and registers differ"), "m2c rough draft")
        self.assertEqual(c("1 nop: modern gas puts a hazard nop between cfc2 and beqz"), "gas hazard")
        self.assertEqual(c("25 lines, lit4: the C literals shift the .sdata $gp offsets"), "data layout / .lit4 pool")
        self.assertEqual(c("2 lines: the original saves s2 with sq / lq"), "data layout / .lit4 pool")
        self.assertEqual(c("store order: the last zero store is scheduled 4 slots earlier"), "store order")
        self.assertEqual(c("the order of the position stores (unk70C ... unk710)"), "store order")
        self.assertEqual(c("14 lines, register allocation of the result"), "register allocation / scheduling")
        self.assertEqual(c("the loads are scheduled in another order"), "register allocation / scheduling")
        self.assertEqual(c("the original has a nop that aligns the second return to 8 bytes"), "block layout / alignment")
        self.assertEqual(c("the shared `return 1` block is placed after the second test"), "block layout / alignment")
        self.assertEqual(c("the compiler cross-jumps them into one"), "block layout / alignment")
        self.assertEqual(c("the result lives in a2 instead of v0"), "register allocation / scheduling")
        self.assertEqual(c("the original loads D_005F5CBA a second time"), "register allocation / scheduling")
        self.assertEqual(c("hand-written VU0 assembly (jumps into its own labels)"), "hand-written assembly")
        self.assertEqual(c("something else entirely"), "other")
        self.assertEqual(c(""), "other")


class Build(unittest.TestCase):
    def test_rows(self):
        not_c = {"func_B": 0x100, "func_C": 0x110, "func_X": 0x200}
        sources = {"x": inventory.scan_source(SOURCE)}
        modules = [(0x100, 0x200, "xmod"), (0x200, 0x300, "ymod")]
        rows = inventory.build_rows(not_c, sources, modules)
        self.assertEqual(rows, [
            ("func_B", 0x100, "x", "asm-only", "not attempted (in a C file)"),
            ("func_C", 0x110, "x", "draft", "register allocation / scheduling"),
            ("func_X", 0x200, "ymod", "asm-only", "no C file"),
        ])

    def test_module_ranges(self):
        text = "// range: 0x100 0x200\n// range: 0x300 0x380\nfoo = 0x100; // type:func\n"
        self.assertEqual(inventory.parse_module_ranges("game_x", text), [(0x100, 0x200, "x"), (0x300, 0x380, "x")])

    def test_summary_counts(self):
        rows = [("a", 1, "m", "asm-only", "no C file"), ("b", 2, "m", "draft", "store order"),
                ("c", 3, "n", "draft", "store order")]
        s = inventory.summarize(rows)
        self.assertEqual(s["category"], {"asm-only": 1, "draft": 2})
        self.assertEqual(s["cause"], {"store order": 2})
        self.assertEqual(s["module"]["m"], {"asm-only": 1, "draft": 1})


if __name__ == "__main__":
    unittest.main()
