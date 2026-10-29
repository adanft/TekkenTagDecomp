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
make_layout = _load("make_layout")
undefined = _load("undefined")
proto_audit = _load("proto_audit")

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

    def test_names_inside_the_data_or_outside_the_image(self):
        spans = [(0x415700, 0x100), (0x5F7180, 0x200)]
        addr = {"gThing": 0x415710}
        script, rest = link.resolve(["gThing", "D_005F7200", "D_11000010", "D_100000", "D_00420000", "memcpy"],
                                    addr, spans)
        self.assertEqual(script, ["D_005F7200 = 0x5F7200;", "D_100000 = 0x100000;",
                                  "D_11000010 = 0x11000010;", "gThing = 0x415710;"])
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


LAYOUT = """\
# comment
elf SLUS_200.01 2873728 c7ad9a022bdc836827a0cc093f91b4281836f2bb
payload 108 0123456789abcdef0123456789abcdef01234567
range 00415700 100
range 00415804 8
bss 005F6B00 880
nobits 005F6B00 680
nobits 005F7180 200
word 00415710 func_00350000 0
word 00415714 gThing -4
zero 00415718
label gOther 00415800
"""


class LayoutTest(unittest.TestCase):
    """port/tools/data_layout.txt (make_layout.py): read by link.py for the image bounds, the names and the table
    plat_data.c loads the game's data from the disc with."""

    def layout(self, text=LAYOUT):
        import tempfile
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
            f.write(text)
        try:
            return link.read_layout(Path(f.name))
        finally:
            Path(f.name).unlink()

    def test_read(self):
        lay = self.layout()
        self.assertEqual(lay["ranges"], [(0x415700, 0x100), (0x415804, 8)])
        self.assertEqual(lay["bss"], (0x5F6B00, 0x880))
        self.assertEqual(lay["nobits"], [(0x5F6B00, 0x680), (0x5F7180, 0x200)])
        self.assertEqual(lay["words"], [(0x415710, "func_00350000", 0), (0x415714, "gThing", -4)])
        self.assertEqual(lay["zeros"], [0x415718])
        self.assertEqual(lay["labels"], {"gOther": 0x415800})
        self.assertEqual(lay["payload"], (0x108, "0123456789abcdef0123456789abcdef01234567"))

    def test_unknown_line_refused(self):
        with self.assertRaises(ValueError):
            self.layout(LAYOUT + "data 00415700 de ad\n")

    def test_image_bounds_are_absolute_symbols(self):
        s = link.image_script(self.layout())
        self.assertIn("__ttt_data_start = 0x00415700;", s)
        self.assertIn("__ttt_data_end = 0x0041580C;", s)
        self.assertIn("__ttt_bss_start = 0x005F6B00;", s)
        self.assertIn("__ttt_bss_end = 0x005F7380;", s)
        self.assertNotIn("SECTIONS", s)  # nothing is linked at the PS2 addresses

    def test_overlap_refused(self):
        lay = self.layout()
        lay["ranges"] = [(0x415700, 0x10), (0x415708, 4)]
        with self.assertRaises(ValueError):
            link.image_script(lay)

    def test_data_after_bss_refused(self):
        lay = self.layout()
        lay["ranges"].append((0x5F6B10, 4))
        with self.assertRaises(ValueError):
            link.image_script(lay)

    def test_data_table(self):
        c = link.data_table(self.layout(), asm=False)  # the 32-bit builds' C form (x86-64's: test_port_clang64)
        self.assertIn("extern char func_00350000[];", c)
        self.assertIn("{0x00415700u, 0x100u},", c)
        self.assertIn("{0x00415710u, (uint32_t)(uintptr_t)func_00350000 + (0)},", c)
        self.assertIn("{0x00415714u, (uint32_t)(uintptr_t)gThing + (-4)},", c)
        self.assertIn("{0x00415718u, 0},", c)
        self.assertIn("const uint32_t gPortDataFixN = 3;", c)
        self.assertIn("const uint32_t gPortDataSize = 0x108u;", c)
        self.assertIn("gPortDataSha1[20] = {0x01, 0x23,", c)
        self.assertIn("gPortElfSha1[20] = {0xc7, 0xad,", c)

    def test_data_table_keeps_only_host_names(self):
        c = link.data_table(self.layout(), keep={"func_00350000"}, asm=False)
        self.assertNotIn("gThing", c)
        self.assertIn("const uint32_t gPortDataFixN = 2;", c)  # the zero word stays


class MakeLayoutTest(unittest.TestCase):
    def test_merge_adjacent(self):
        self.assertEqual(make_layout.merge([(0x20, 4), (0x10, 0x10), (0x30, 4)]), [(0x10, 0x14), (0x30, 4)])

    def test_committed_layout_has_no_data_lines(self):
        """The committed layout holds addresses, sizes, names and hashes only."""
        kinds = {l.split()[0] for l in make_layout.LAYOUT.read_text().splitlines() if l and not l.startswith("#")}
        self.assertLessEqual(kinds, {"elf", "payload", "range", "bss", "nobits", "word", "zero", "label"})


class GenDataTest(unittest.TestCase):
    def test_section_name_of_a_file(self):
        self.assertEqual(gen_data.section_name("1BBF70.data.s"), "1BBF70_data")
        self.assertEqual(gen_data.section_name("fn_func_003608E0_0.rodata.s"), "fn_func_003608E0_0_rodata")


class ProtoAuditTest(unittest.TestCase):
    """port/tools/proto_audit.py (T41 b0): i386 slot layouts of prototypes and Text_Printf argument counts."""

    def decl(self, params, ret="I", name="f"):
        return proto_audit.Decl(name, ret, params, "src/x.c", 1, False, "")

    def setUp(self):
        self.types = proto_audit.Types()
        self.types.scan("typedef float sceVu0FVECTOR[4];\ntypedef struct { s32 a; } Box;\ntypedef s32 Id;")
        self.saved, proto_audit.TYPES = proto_audit.TYPES, self.types

    def tearDown(self):
        proto_audit.TYPES = self.saved

    def test_slot_classes(self):
        t = self.types
        self.assertEqual(proto_audit.layout(t, "s32 a, f32 b, u8 *p, u64 q, ..."), ("I", "F", "I", "L", "V"))
        self.assertEqual(proto_audit.layout(t, "void"), ())
        self.assertEqual(proto_audit.layout(t, ""), ("U",))
        self.assertEqual(proto_audit.layout(t, "sceVu0FVECTOR v, Box b, Id i, void (*cb)(s32)"), ("I", "S:Box", "I", "I"))

    def test_dropped_arguments_are_fewer(self):
        ref = self.decl("u32 x, u32 y, u32 w, u32 h, u32 z, u32 c0, u32 c1, u32 c2, u32 c3")
        self.assertEqual(proto_audit.compare(ref, self.decl("s32, s32, s32, s32"))[0], "fewer")

    def test_int_float_order_is_a_swap(self):
        ref = self.decl("s32 a, f32 b")
        self.assertEqual(proto_audit.compare(ref, self.decl("f32 b, s32 a"))[0], "swap")

    def test_variadic_and_fixed_differ(self):
        ref = self.decl("const char *fmt, ...")
        self.assertEqual(proto_audit.compare(ref, self.decl("void *, s32, s32, s32"))[0], "variadic")

    def test_64_bit_halves_are_reported(self):
        """Same i386 stack bytes, but wrong on x86-64 (commit 8f5fd1e): its own kind, listed without -v."""
        ref = self.decl("u32 a, u32 b, u32 lo, u32 hi")
        self.assertEqual(proto_audit.compare(ref, self.decl("u32 a, u32 b, u64 v"))[0], "halves")
        ref = self.decl("u128 tag, u32 n")
        self.assertEqual(proto_audit.compare(ref, self.decl("u32 a, u32 b, u32 c, u32 d, u32 n"))[0], "halves")
        self.assertLess(proto_audit.SEVERITY.index("halves"), proto_audit.SEVERITY.index("class"))

    def test_same_layout_agrees(self):
        self.assertIsNone(proto_audit.compare(self.decl("s32 a, u8 *p"), self.decl("u32, void *")))

    def test_text_printf_directives(self):
        self.assertEqual(proto_audit.tp_count("%H%V%F%S%c%s"), 6)
        self.assertEqual(proto_audit.tp_count("%Z%H%V%F%S%cBGM VOLUME: %cOFF"), 7)
        self.assertEqual(proto_audit.tp_count("%T%F%SRANDOM"), 2)
        self.assertEqual(proto_audit.tp_count("100%% %2d%08x"), 2)

