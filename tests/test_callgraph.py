"""Tests for scripts/callgraph.py (synthetic splat/spimdisasm assembly only)."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
_spec = importlib.util.spec_from_file_location("callgraph", ROOT / "scripts" / "callgraph.py")
callgraph = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(callgraph)

TEXT = r"""
.section .text, "ax"
.align 3
nonmatching func_00100000, 0x20

glabel func_00100000
    /* 1000 00100000 F0FFBD27 */  addiu      $sp, $sp, -0x10
    /* 1004 00100004 0000BFFF */  jal        func_00100020
    /* 1008 00100008 00000000 */   nop
    /* 100C 0010000C 1C9A828F */  lw         $v0, %gp_rel(D_00500000 + 0xC)($gp)
    /* 1010 00100010 5000043C */  lui        $a0, %hi(D_00500010)
    /* 1014 00100014 00008424 */  addiu      $a0, $a0, %lo(D_00500010)
    /* 1018 00100018 0800E003 */  j          sceGsSyncV
    /* 101C 0010001C 00000000 */   nop
endlabel func_00100000

.align 3
nonmatching func_00100020, 0x18

glabel func_00100020
    /* 1020 00100020 0000BFFF */  lui        $a1, %hi(func_00100000)
    /* 1024 00100024 2000A524 */  addiu      $a1, $a1, %lo(func_00100000)
    /* 1028 00100028 09F84000 */  jalr       $v0
    /* 102C 0010002C 00000000 */   nop
  .L00100030:
    /* 1030 00100030 FDFF0010 */  b          .L00100030
    /* 1034 00100034 00000000 */   nop
endlabel func_00100020
"""

DATA = r"""
.section .rodata, "a"
nonmatching D_00500010

dlabel D_00500010
    /* 1BC880 00500010 */ .asciz "cdrom:\\IRX\\PADMAN.IRX;1"
    /* 6364 */
.align 2
    /* 1BC89C 0050002C */ .asciz "second"
enddlabel D_00500010

nonmatching D_00500040

dlabel D_00500040
    /* D6700 00500040 00001000 */ .word func_00100000
    /* D6704 00500044 20001000 */ .word func_00100020
    /* D6708 00500048 10005000 */ .word D_00500010
    /* D670C 0050004C 00000000 */ .word 0x00000000
enddlabel D_00500040
"""


class ParseText(unittest.TestCase):
    def setUp(self):
        self.funcs = callgraph.parse_text(TEXT.splitlines())

    def test_functions_with_addresses_and_sizes(self):
        self.assertEqual(sorted(self.funcs), ["func_00100000", "func_00100020"])
        f = self.funcs["func_00100000"]
        self.assertEqual((f.addr, f.size), (0x100000, 0x20))

    def test_calls_include_tail_jumps(self):
        f = self.funcs["func_00100000"]
        self.assertEqual(f.calls, {"func_00100020", "sceGsSyncV"})

    def test_data_refs_drop_offsets(self):
        self.assertEqual(self.funcs["func_00100000"].refs, {"D_00500000", "D_00500010"})

    def test_function_pointers_and_indirect_calls(self):
        g = self.funcs["func_00100020"]
        self.assertEqual(g.fptrs, {"func_00100000"})
        self.assertEqual(g.indirect, 1)
        self.assertEqual(g.calls, set())

    def test_local_branches_are_not_calls(self):
        self.assertNotIn(".L00100030", self.funcs["func_00100020"].calls)


class ParseData(unittest.TestCase):
    def setUp(self):
        self.data = callgraph.parse_data(DATA.splitlines())

    def test_strings_keep_their_addresses(self):
        d = self.data["D_00500010"]
        self.assertEqual(d.addr, 0x500010)
        self.assertEqual(d.strings, [(0x500010, r"cdrom:\IRX\PADMAN.IRX;1"), (0x50002C, "second")])

    def test_words_naming_symbols(self):
        d = self.data["D_00500040"]
        self.assertEqual(d.words, ["func_00100000", "func_00100020", "D_00500010"])


class Graph(unittest.TestCase):
    def setUp(self):
        self.g = callgraph.Graph(callgraph.parse_text(TEXT.splitlines()), callgraph.parse_data(DATA.splitlines()))

    def test_callers(self):
        self.assertEqual(self.g.callers("func_00100020"), ["func_00100000"])

    def test_pointer_users_include_tables(self):
        self.assertEqual(self.g.pointer_users("func_00100000"), ["D_00500040", "func_00100020"])

    def test_tables_list_function_pointer_arrays(self):
        self.assertEqual(self.g.tables(min_funcs=2), [("D_00500040", 0x500040, 2)])

    def test_strings_of_a_function(self):
        self.assertEqual(self.g.strings("func_00100000"),
                         [(0x500010, r"cdrom:\IRX\PADMAN.IRX;1"), (0x50002C, "second")])

    def test_reachable_from(self):
        self.assertEqual(self.g.reachable("func_00100000"), {"func_00100000", "func_00100020", "sceGsSyncV"})

    def test_label_for_address(self):
        ranges = [(0x100000, 0x100020, "libx")]
        self.assertEqual(callgraph.label_of(0x100010, ranges), "libx")
        self.assertIsNone(callgraph.label_of(0x100020, ranges))


if __name__ == "__main__":
    unittest.main()
