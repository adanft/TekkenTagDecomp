"""Tests for the pure helpers of the PC port build pipeline (port/tools), on synthetic data only."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "port" / "tools"
sys.path.insert(0, str(TOOLS))


def _load(name):
    spec = importlib.util.spec_from_file_location("port_" + name, TOOLS / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


portsrc = _load("portsrc")
gen_data = _load("gen_data")
link = _load("link")
undefined = _load("undefined")

DRAFTS = """\
#include "common.h"

void func_A(void) {
}

/* NON_MATCHING: 12 lines: register allocation */
#if 0
void func_C(s32 a) {
#if 0
    inner();
#endif
}
#else
INCLUDE_ASM("asm/nonmatchings/x", func_C);
#endif

ASM_STUB_BEGIN();
void func_D(void) {
}
ASM_STUB_END();
INCLUDE_ASM("asm/nonmatchings/x", func_D);

INCLUDE_ASM("asm/nonmatchings/x", func_E);
"""


class EnableAttemptsTest(unittest.TestCase):
    def test_draft_switched_on(self):
        text, done, missing = portsrc.enable_attempts(DRAFTS)
        self.assertIn("#if 1\nvoid func_C(s32 a) {", text)
        self.assertIn("func_C", done)

    def test_inner_conditional_untouched(self):
        text, _, _ = portsrc.enable_attempts(DRAFTS)
        self.assertIn("#if 0\n    inner();", text)

    def test_asm_stub_counts_as_c(self):
        _, done, _ = portsrc.enable_attempts(DRAFTS)
        self.assertIn("func_D", done)

    def test_missing_c_reported(self):
        _, _, missing = portsrc.enable_attempts(DRAFTS)
        self.assertEqual(missing, ["func_E"])

    def test_replaced_draft_stays_off(self):
        text, done, missing = portsrc.enable_attempts(DRAFTS, replaced={"func_C"})
        self.assertIn("#if 0\nvoid func_C(s32 a) {", text)
        self.assertNotIn("func_C", done)
        self.assertEqual(missing, ["func_E"])

    def test_sources_from_config(self):
        yaml = "      - [0x1000, c, render_a]\n      - [0xB7A8, asm] # lib\n      - [0x2000, c, main]\n" \
               "      - [0x1BC880, .rodata, main]\n"
        self.assertEqual(portsrc.c_names(yaml), ["render_a", "main"])


DATA = """\
.include "macro.inc"

.section .data, "wa"

nonmatching D_00415704

dlabel D_00415704
    /* D6704 00415704 08B94F00 */ .word D_004FB908
    /* D6708 00415708 00000000 */ .word .L00340010
    /* D670C 0041570C 0100 */ .short 0x1
    /* D670E 0041570E 02 */ .byte 0x2
.align 3
    /* D6710 00415710 0000C03F */ .float 1.5
enddlabel D_00415704
"""

BSS = """\
.section .bss, "wa"

nonmatching D_005F7188, 0x120

dlabel D_005F7188
    /* 005F7188 */ .space 0x120
"""

RODATA = """\
.section .rodata

.align 3
nonmatching Str_Name

dlabel Str_Name
    /* 1BC880 004FB881 */ .asciz "cdrom:\\\\IRX"
    /* 6364726F6D3A */
.align 2
enddlabel Str_Name
"""


class ConvertTest(unittest.TestCase):
    def test_address_of_data_chunk(self):
        self.assertEqual(gen_data.chunk_address(DATA), 0x415704)

    def test_address_of_bss_chunk(self):
        self.assertEqual(gen_data.chunk_address(BSS), 0x5F7188)

    def test_address_of_named_rodata(self):
        self.assertEqual(gen_data.chunk_address(RODATA), 0x4FB881)

    def test_word_becomes_long(self):
        out = gen_data.convert(DATA, "data")
        self.assertIn(".long D_004FB908", out)
        self.assertNotIn(".word", out)

    def test_local_code_label_becomes_zero(self):
        self.assertIn(".long 0\n", gen_data.convert(DATA, "data"))

    def test_label_is_global(self):
        out = gen_data.convert(DATA, "data")
        self.assertIn(".globl D_00415704\nD_00415704:", out)

    def test_own_section_aligned_to_the_address(self):
        out = gen_data.convert(DATA, "data", section="D6700_data")
        self.assertTrue(out.startswith('.section .ttt.D6700_data, "aw", @progbits\n.p2align 2\n'))
        self.assertNotIn(".space 4\n", out)  # placed at its PS2 address by the link: no padding in front

    def test_align_is_power_of_two(self):
        self.assertIn(".p2align 3", gen_data.convert(DATA, "data"))

    def test_bss_section(self):
        out = gen_data.convert(BSS, "bss", section="b")
        self.assertTrue(out.startswith('.section .ttt.b, "aw", @nobits\n.p2align 3\n'))
        self.assertIn(".space 0x120", out)

    def test_rodata_is_writable_data(self):
        out = gen_data.convert(RODATA, "rodata", section="r")
        self.assertTrue(out.startswith('.section .ttt.r, "aw", @progbits\n.p2align 0\n'))
        self.assertIn('.asciz "cdrom:\\\\IRX"', out)

    def test_unknown_directive_refused(self):
        with self.assertRaises(ValueError):
            gen_data.convert("dlabel D_00400000\n    .quad 1\n", "data")

    def test_included_tables_skip_jump_tables(self):
        c = 'INCLUDE_RODATA("asm/nonmatchings/main", D_004FB880);\n' \
            'INCLUDE_RODATA("asm/nonmatchings/main", jtbl_004FB970);\n'
        self.assertEqual(gen_data.included_tables(c), [("asm/nonmatchings/main", "D_004FB880")])


FUNC = """\
.section .rodata
.align 3
nonmatching D_004FC280

dlabel D_004FC280
    /* 1BD280 004FC280 */ .asciz "%cYES"
    /* 2563594553000000 */
.align 2
enddlabel D_004FC280

.section .rodata
dlabel jtbl_004FC290
    /* 1BD290 004FC290 0C093600 */ .word .L0036090C
enddlabel jtbl_004FC290

.section .lit4
dlabel D_005F5100
    /* 2B6100 005F5100 0000803F */ .float 1.0
enddlabel D_005F5100

.section .text
glabel func_003608E0
    /* 218E0 003608E0 D0FFBD27 */  addiu      $sp, $sp, -0x30
"""


class FunctionDataTest(unittest.TestCase):
    def test_data_blocks_of_a_function_file(self):
        blocks = gen_data.function_data(FUNC)
        self.assertEqual([k for k, _ in blocks], ["rodata", "lit4"])
        self.assertIn("dlabel D_004FC280", blocks[0][1])
        self.assertIn("dlabel D_005F5100", blocks[1][1])

    def test_jump_tables_and_code_left_out(self):
        text = "".join(b for _, b in gen_data.function_data(FUNC))
        self.assertNotIn("jtbl_", text)
        self.assertNotIn("addiu", text)


class StubTest(unittest.TestCase):
    def test_one_stub_per_name(self):
        src = link.stub_source(["sceGsResetPath", "memcpy"])
        self.assertIn('int sceGsResetPath(void) { static int n; return missing("sceGsResetPath", &n); }', src)
        self.assertIn('int memcpy(void) { static int n; return missing("memcpy", &n); }', src)
        self.assertIn('getenv("TTT_STUB_RETURN")', src)

    def test_names_inside_a_data_chunk_or_outside_the_image(self):
        chunks = [("__chunk_D6700", 0x415700, 0x100), ("__chunk_bss", 0x5F7180, 0x200)]
        addr = {"gThing": 0x415710}
        script, rest = link.resolve(["gThing", "D_005F7200", "D_11000010", "D_100000", "D_00420000", "memcpy"],
                                    addr, chunks)
        self.assertEqual(script, ["D_005F7200 = __chunk_bss + 0x80;", "D_100000 = 0x100000;",
                                  "D_11000010 = 0x11000010;", "gThing = __chunk_D6700 + 0x10;"])
        self.assertEqual(rest, ["D_00420000", "memcpy"])

    def test_chunk_label_in_convert(self):
        out = gen_data.convert(DATA, "data", label="__chunk_x")
        self.assertIn(".p2align 2\n.globl __chunk_x\n__chunk_x:\n", out)

    def test_undefined_names_from_linker(self):
        err = "a.o: in function `f':\nx.c:(.text+0x1): undefined reference to `sceSifInitRpc'\n" \
              "x.c:(.text+0x9): undefined reference to `sceSifInitRpc'\nb.o: undefined reference to `D_00500000'\n"
        self.assertEqual(link.undefined_names(err), ["D_00500000", "sceSifInitRpc"])


class ClassifyTest(unittest.TestCase):
    ADDR = {"Game_Main": 0x357800, "sceGsResetPath": 0x400000, "gThing": 0x5F7000}

    def test_game_function(self):
        self.assertEqual(undefined.classify("Game_Main", self.ADDR), "game function")

    def test_library_function(self):
        self.assertEqual(undefined.classify("sceGsResetPath", self.ADDR), "library function (SDK / libc)")

    def test_data(self):
        self.assertEqual(undefined.classify("gThing", self.ADDR), "data")
        self.assertEqual(undefined.classify("D_00500000", {}), "data")

    def test_address_from_name(self):
        self.assertEqual(undefined.classify("func_00360000", {}), "game function")

    def test_host_helper(self):
        self.assertEqual(undefined.classify("__addsf3", {}), "no address (host library or compiler helper)")


if __name__ == "__main__":
    unittest.main()


class ImageScriptTest(unittest.TestCase):
    """The linker script that puts every data file at its PS2 address (one section each, see gen_data.convert)."""
    ENTRIES = [("bss", 0x5F7180, 0x200, True), ("D6700_data", 0x415700, 0x100, False),
               ("x_rodata", 0x415804, 4, False), ("sbss", 0x5F6B00, 0x680, True)]

    def test_data_and_bss_at_their_addresses(self):
        s = link.image_script(self.ENTRIES)
        self.assertIn(".ttt_data 0x00415700 :", s)
        self.assertIn(".ttt_bss 0x005F6B00 (NOLOAD) :", s)
        self.assertLess(s.index("*(.ttt.D6700_data)"), s.index(". = 0x104;"))
        self.assertLess(s.index(". = 0x104;"), s.index("*(.ttt.x_rodata)"))
        self.assertIn(". = 0x680;\n    *(.ttt.bss)", s)
        self.assertIn("INSERT AFTER .bss;", s)

    def test_bounds_named(self):
        s = link.image_script(self.ENTRIES)
        for name in ("__ttt_data_start", "__ttt_data_end", "__ttt_bss_start", "__ttt_bss_end"):
            self.assertIn(name + " = ", s)

    def test_overlap_refused(self):
        with self.assertRaises(ValueError):
            link.image_script([("a", 0x415700, 0x10, False), ("b", 0x415708, 4, False)])

    def test_data_after_bss_refused(self):
        with self.assertRaises(ValueError):
            link.image_script([("a", 0x415700, 0x10, True), ("b", 0x415800, 4, False)])

    def test_section_name_of_a_file(self):
        self.assertEqual(gen_data.section_name("1BBF70.data.s"), "1BBF70_data")
        self.assertEqual(gen_data.section_name("fn_func_003608E0_0.rodata.s"), "fn_func_003608E0_0_rodata")
