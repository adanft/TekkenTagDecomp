"""Tests for the pure helpers of configure.py (no disc or splat run needed).

Run: .venv/bin/python -m unittest discover -s tests -v
"""

import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent

_spec = importlib.util.spec_from_file_location("configure", ROOT / "configure.py")
configure = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(configure)

SCRIPT = """SECTIONS
{
    .main 0x340000 : AT(main_ROM_START) SUBALIGN(16)
    {
        build/asm/1000.o(.text*);
        build/asm/1000.o(.data*);
        build/asm/data/D6700.data.o(.data*);
    }
    main_bss_VRAM = ADDR(.main_bss);
    .main_bss (NOLOAD) : SUBALIGN(16)
    {
        build/asm/data/2B7AF0.sbss.o(.sbss COMMON .scommon);
    }
}
"""


class AlignBss(unittest.TestCase):
    def test_aligns_bss_section(self):
        out = configure.align_bss(SCRIPT)
        self.assertIn(".main_bss ALIGN(64) (NOLOAD) : SUBALIGN(16)", out)
        self.assertEqual(out.replace(configure.BSS_ALIGNED, configure.BSS_SECTION), SCRIPT)

    def test_idempotent(self):
        once = configure.align_bss(SCRIPT)
        self.assertEqual(configure.align_bss(once), once)

    def test_refuses_unknown_script(self):
        with self.assertRaises(ValueError):
            configure.align_bss(SCRIPT.replace(".main_bss (NOLOAD)", ".other (NOLOAD)"))


class Objects(unittest.TestCase):
    def test_link_order_without_repeats(self):
        self.assertEqual(configure.objects(SCRIPT),
                         ["build/asm/1000.o", "build/asm/data/D6700.data.o", "build/asm/data/2B7AF0.sbss.o"])

    def test_source_of(self):
        self.assertEqual(configure.source_of("build/asm/data/D6700.data.o"), "asm/data/D6700.data.s")
        self.assertEqual(configure.source_of("build/asm/1000.o"), "asm/1000.s")

    def test_source_of_c_file(self):
        self.assertEqual(configure.source_of("build/src/340000.o"), "src/340000.c")
        self.assertEqual(configure.source_of("build/src/sys/heap.o"), "src/sys/heap.c")


class VuMicrocode(unittest.TestCase):
    def test_packets_object_is_built_from_the_wrapper(self):
        self.assertEqual(configure.asm_source(configure.VU_SPLIT_ASM), configure.VU_ASM)
        self.assertEqual(configure.asm_source("asm/data/D6700.data.s"), "asm/data/D6700.data.s")

    def test_wrapper_includes_every_packet(self):
        text = (ROOT / configure.VU_ASM).read_text()
        for binary in configure.VU_PACKETS:
            self.assertIn(f'.incbin "{binary}"', text)

    def test_packet_depends_on_its_folder_listings(self):
        listings = configure.vu_listings("src/vu1/vu1.vsm")
        self.assertIn("src/vu1/vu1.vsm", listings)
        self.assertIn("src/vu1/mesh.vsm", listings)
        self.assertFalse([p for p in listings if not p.startswith("src/vu1/")])


class IncludedAsm(unittest.TestCase):
    def test_in_order(self):
        text = ('#include "common.h"\n'
                'INCLUDE_ASM("asm/nonmatchings/340000", func_00340070);\n'
                'void f(void) {}\n'
                '  INCLUDE_ASM( "asm/nonmatchings/340000" , func_00340398 );\n')
        self.assertEqual(configure.included_asm(text),
                         ["asm/nonmatchings/340000/func_00340070.s", "asm/nonmatchings/340000/func_00340398.s"])

    def test_ignores_comments_and_definitions(self):
        text = ('/* INCLUDE_ASM("asm/x", in_comment); */\n'
                '#define INCLUDE_ASM(FOLDER, NAME) x\n')
        self.assertEqual(configure.included_asm(text), [])

    def test_rodata_includes_are_split_files_too(self):
        # A C file whose functions are all C still includes its tables from the split: without the dependency ninja
        # compiles it while a second split has emptied asm/ (the flaky clean rebuild of T17 b3).
        text = ('INCLUDE_RODATA("asm/nonmatchings/file", gFileTable);\n'
                'INCLUDE_ASM("asm/nonmatchings/file", func_00397948);\n'
                '#define INCLUDE_RODATA(FOLDER, NAME) x\n')
        self.assertEqual(configure.included_asm(text),
                         ["asm/nonmatchings/file/gFileTable.s", "asm/nonmatchings/file/func_00397948.s"])


class GFlag(unittest.TestCase):
    def test_default_and_prefix(self):
        old = dict(configure.G_FLAGS)
        try:
            configure.G_FLAGS.clear()
            configure.G_FLAGS["src/lib/"] = "-G0"
            self.assertEqual(configure.g_flag("src/lib/dma.c"), "-G0")
            self.assertEqual(configure.g_flag("src/340000.c"), configure.G_DEFAULT)
        finally:
            configure.G_FLAGS.clear()
            configure.G_FLAGS.update(old)


class CcFlags(unittest.TestCase):
    def test_default_flags(self):
        with mock.patch.object(configure, "NO_FAST_MATH", ()):
            self.assertEqual(configure.cc_flags("src/main.c"), configure.CC_FLAGS)

    def test_file_without_fast_math(self):
        with mock.patch.object(configure, "NO_FAST_MATH", ("src/effects.c",)):
            flags = configure.cc_flags("src/effects.c").split()
            self.assertNotIn("-ffast-math", flags)
            self.assertEqual(flags, [f for f in configure.CC_FLAGS.split() if f != "-ffast-math"])
            # a path, not a prefix of another file's name
            self.assertIn("-ffast-math", configure.cc_flags("src/effects_draw.c").split())

    def test_listed_files_exist(self):
        for src in configure.NO_FAST_MATH:
            self.assertTrue((ROOT / src).is_file(), src)


class FixSplitAsm(unittest.TestCase):
    def test_bare_accumulator_gets_its_dollar(self):
        line = "    /* 9BE08 003DAE08 BD41C44A */  vmulay.yz  ACC, $vf8, $vf4y\n"
        self.assertEqual(configure.fix_split_asm(line),
                         "    /* 9BE08 003DAE08 BD41C44A */  vmulay.yz  $ACC, $vf8, $vf4y\n")

    def test_leaves_everything_else(self):
        text = ("    /* AE38 00349E38 BC49E74B */  vmulax.xyzw $ACC, $vf9, $vf7x\n"
                "    /* 0 0 0 */  addiu      $a0, $a0, %lo(ACC_TABLE)\n"
                '.asciz "ACC, ok"\n'
                "glabel ACC\n")
        self.assertEqual(configure.fix_split_asm(text), text)


class FixCompilerAsm(unittest.TestCase):
    """ee-gcc output rewritten where the game's assembler and the modern gas differ (docs/build.md)."""

    def fix(self, *lines):
        return configure.fix_compiler_asm("".join(self.line(l) + "\n" for l in lines)).splitlines()

    @staticmethod
    def line(text):
        return text if text.endswith(":") else "\t" + text

    def test_li_s_with_lui_form_moves_without_hazard(self):
        self.assertEqual(self.fix("li.s\t$f0,2.04800000000000000000e3", "mtc1\t$0,$f24"),
                         ["\t.set\tnoat", "\tlui\t$1,0x4500", "\tmtc1\t$1,$f0", "\t.set\tat", "\tmtc1\t$0,$f24"])

    def test_li_s_zero_and_negative_zero(self):
        self.assertEqual(self.fix("li.s\t$f2,0.00000000000000000000e0"), ["\tmtc1\t$0,$f2"])
        self.assertEqual(self.fix("li.s\t$f2,-0.00000000000000000000e0")[1:3], ["\tlui\t$1,0x8000", "\tmtc1\t$1,$f2"])

    def test_li_s_from_the_literal_pool_unchanged(self):
        lines = ["li.s\t$f0,1.10000002384185791016e0"]
        self.assertEqual(self.fix(*lines), ["\t" + lines[0]])

    def test_li_s_in_noreorder_unchanged(self):
        lines = [".set\tnoreorder", "li.s\t$f0,1.00000000000000000000e0", ".set\treorder"]
        self.assertEqual(self.fix(*lines), ["\t" + l for l in lines])

    def test_li_s_pool_word_moves_to_the_marked_lit4_subsection(self):
        # LIT4_ORDER(2) (include/include_asm.h) in front of a function: its pool words go after the words of
        # subsections 0 and 1 of the object's .lit4, through a local label and an explicit %gp_rel load
        out = self.fix("#lit4_order 2", "li.s\t$f0,5.80000000000000000000e3", "s.s\t$f0,D_005F694C",
                       "li.s\t$f1,1.10000002384185791016e0")
        self.assertEqual(out, ["\t#lit4_order 2",
                               "\t.pushsection\t.lit4,2", ".Llit4_0:", "\t.word\t0x45B54000", "\t.popsection",
                               "\tlwc1\t$f0,%gp_rel(.Llit4_0)($gp)", "\ts.s\t$f0,D_005F694C",
                               "\t.pushsection\t.lit4,2", ".Llit4_1:", "\t.word\t0x3F8CCCCD", "\t.popsection",
                               "\tlwc1\t$f1,%gp_rel(.Llit4_1)($gp)"])

    def test_li_s_pool_word_in_noreorder_moves_too(self):
        out = self.fix("#lit4_order 1", ".set\tnoreorder", "bnez\t$2,$L3", "li.s\t$f0,5.80000000000000000000e3",
                       ".set\treorder")
        self.assertEqual(out[3:8], ["\t.pushsection\t.lit4,1", ".Llit4_0:", "\t.word\t0x45B54000", "\t.popsection",
                                    "\tlwc1\t$f0,%gp_rel(.Llit4_0)($gp)"])

    def test_lit4_order_zero_and_lui_values_unchanged(self):
        lines = ["#lit4_order 1", "#lit4_order 0", "li.s\t$f0,1.10000002384185791016e0"]
        self.assertEqual(self.fix(*lines), ["\t" + l for l in lines])
        out = self.fix("#lit4_order 1", "li.s\t$f0,2.04800000000000000000e3", "li.s\t$f2,0.00000000000000000000e0")
        self.assertEqual(out, ["\t#lit4_order 1", "\t.set\tnoat", "\tlui\t$1,0x4500", "\tmtc1\t$1,$f0", "\t.set\tat",
                               "\tmtc1\t$0,$f2"])

    def test_lit4_ref_loads_the_next_pool_word_from_a_named_word(self):
        # LIT4_REF(D_005F570C): the word lies in another object's part of the pool (a split file defines it); the next
        # pool load reads it there and the compiler's own word goes away. The load stays where the compiler put it
        # (noreorder: the game's assembler did not move these into a delay slot, Model_Render)
        out = self.fix("#lit4_ref D_005F570C", "li.s\t$f2,2.04800000000000000000e3",
                       "li.s\t$f8,1.66666671633720397949e-1", "li.s\t$f9,1.66666671633720397949e-1")
        self.assertEqual(out, ["\t#lit4_ref D_005F570C", "\t.set\tnoat", "\tlui\t$1,0x4500", "\tmtc1\t$1,$f2",
                               "\t.set\tat", "\t.set\tnoreorder", "\tlwc1\t$f8,%gp_rel(D_005F570C)($gp)",
                               "\t.set\treorder", "\tli.s\t$f9,1.66666671633720397949e-1"])
        out = self.fix("#lit4_ref D_005F5708", ".set\tnoreorder", "jal\tfoo", "li.s\t$f12,3.05185094475746154785e-5",
                       ".set\treorder")
        self.assertEqual(out[3], "\tlwc1\t$f12,%gp_rel(D_005F5708)($gp)")
        self.assertEqual(out[4], "\t.set\treorder")

    def test_lit4_keep_holds_the_nth_pool_load_in_place(self):
        # LIT4_KEEP(2): the second pool load from here on stays in front of the branch (noreorder), as the game's
        # assembler left it (Bowl_PinHomePos case 6); other loads and the lui form are untouched
        out = self.fix("#lit4_keep 2", "li.s\t$f0,2.04800000000000000000e3", "li.s\t$f0,1.10000002384185791016e0",
                       "li.s\t$f1,5.80000000000000000000e3", "j\t$L9", "li.s\t$f2,5.80000000000000000000e3")
        self.assertEqual(out, ["\t#lit4_keep 2", "\t.set\tnoat", "\tlui\t$1,0x4500", "\tmtc1\t$1,$f0", "\t.set\tat",
                               "\tli.s\t$f0,1.10000002384185791016e0", "\t.set\tnoreorder",
                               "\tli.s\t$f1,5.80000000000000000000e3", "\t.set\treorder", "\tj\t$L9",
                               "\tli.s\t$f2,5.80000000000000000000e3"])

    def test_mfc1_before_noreorder_reader_gets_the_hazard_nop(self):
        # gcc left `mfc1 $5` to gas and the next block (noreorder) reads $5 first: the game's assembler put a nop
        # there (func_003E92E0), the modern gas does not
        out = self.fix("mfc1\t$5,$f1", ".set\tnoreorder", ".set\tnomacro", "bltz\t$5,$L3", "move\t$6,$0")
        self.assertEqual(out[:2], ["\tmfc1\t$5,$f1", "\tnop"])
        lines = ["mfc1\t$5,$f1", ".set\tnoreorder", "bltz\t$6,$L3", "move\t$7,$0"]
        self.assertEqual(self.fix(*lines), ["\t" + l for l in lines])

    def test_compare_before_unfilled_fpu_branch_is_data(self):
        out = self.fix("c.lt.s\t$f0,$f1", "#nop", "bc1f\t$L4")
        self.assertTrue(out[0].startswith("\t.word\t0x46010034"), out[0])
        self.assertEqual(out[1:], ["\t#nop", "\tbc1f\t$L4"])
        self.assertTrue(self.fix("c.le.s\t$f12,$f14", "bc1t\t$L1")[0].startswith("\t.word\t0x460E6036"))
        self.assertTrue(self.fix("c.eq.s\t$f31,$f2", "bc1t\t$L1")[0].startswith("\t.word\t0x4602F832"))

    def test_compare_before_filled_branch_unchanged(self):
        # gcc's noreorder block: gas's hazard nop at `.set noreorder` is in the original
        lines = ["c.lt.s\t$f12,$f13", "#nop", ".set\tnoreorder", ".set\tnomacro", "bc1fl\t$L3", "sub.s\t$f0,$f12,$f14"]
        self.assertEqual(self.fix(*lines), ["\t" + l for l in lines])
        lines = ["c.lt.s\t$f0,$f1", "add.s\t$f2,$f2,$f3", "bc1f\t$L4"]
        self.assertEqual(self.fix(*lines), ["\t" + l for l in lines])

    def test_sqrt_fills_the_delay_slot_of_a_following_jal(self):
        out = self.fix(".set\tnoreorder", "nop", "nop", "sqrt.s\t$f13,$f13", ".set\treorder", "jal\tatan2f",
                       "mov.s\t$f22,$f0")
        self.assertEqual(out, ["\t.set\tnoreorder", "\tnop", "\tnop", "\tjal\tatan2f", "\tsqrt.s\t$f13,$f13",
                               "\t.set\treorder", "\tmov.s\t$f22,$f0"])

    def test_sqrt_and_div_elsewhere_unchanged(self):
        for lines in ([".set\tnoreorder", "nop", "nop", "div.s\t$f0,$f0,$f1", ".set\treorder", "jal\tfoo"],
                      [".set\tnoreorder", "nop", "nop", "sqrt.s\t$f0,$f0", ".set\treorder", "j\t$31"],
                      [".set\tnoreorder", "nop", "nop", "sqrt.s\t$f0,$f0", ".set\treorder", "$L2:", "jal\tfoo"]):
            self.assertEqual(self.fix(*lines), [self.line(l) for l in lines])


class SetEabi64(unittest.TestCase):
    HEADER = b"\x7fELF\x01\x01\x01" + bytes(0x24 - 7) + (0x20922001).to_bytes(4, "little") + b"rest"

    def test_sets_flags_only(self):
        out = configure.set_eabi64(self.HEADER)
        self.assertEqual(len(out), len(self.HEADER))
        self.assertEqual(int.from_bytes(out[0x24:0x28], "little"), 0x20924001)
        self.assertEqual(out[:0x24], self.HEADER[:0x24])
        self.assertEqual(out[0x28:], b"rest")

    def test_refuses_non_elf32(self):
        with self.assertRaises(ValueError):
            configure.set_eabi64(b"\x7fELF\x02\x01" + bytes(0x40))
        with self.assertRaises(ValueError):
            configure.set_eabi64(b"not an elf")


YAML = """options:
  symbol_addrs_path:
    - config/symbols/libc.txt
    - config/symbols/game_main.txt   # comment
  base_path: ..
segments:
  - name: main
"""


class SymbolPaths(unittest.TestCase):
    def test_yaml_list(self):
        self.assertEqual(configure.yaml_symbol_paths(YAML),
                         ["config/symbols/libc.txt", "config/symbols/game_main.txt"])

    def test_yaml_single_path(self):
        self.assertEqual(configure.yaml_symbol_paths("options:\n  symbol_addrs_path: config/a.txt\n"),
                         ["config/a.txt"])

    def test_listed_must_equal_present(self):
        configure.check_symbol_paths(["b.txt", "a.txt"], ["a.txt", "b.txt"])
        with self.assertRaisesRegex(ValueError, "c.txt"):
            configure.check_symbol_paths(["a.txt"], ["a.txt", "c.txt"])
        with self.assertRaisesRegex(ValueError, "d.txt"):
            configure.check_symbol_paths(["a.txt", "d.txt"], ["a.txt"])


class FakeSplat:
    """Stands in for `splat split`: writes the given asm files and unaligned linker scripts under configure.ROOT,
    or fails half way (after writing the asm files) when fail is set."""

    def __init__(self, files, fail=False):
        self.files, self.fail, self.calls = files, fail, 0

    def __call__(self, cmd):
        self.calls += 1
        root = configure.ROOT
        for name in self.files:
            (root / "asm" / name).parent.mkdir(parents=True, exist_ok=True)
            (root / "asm" / name).write_text("glabel func_00100000\n")
        if self.fail:
            raise subprocess.CalledProcessError(1, cmd)
        for f in configure.LD_SCRIPTS:
            (root / f).parent.mkdir(parents=True, exist_ok=True)
            (root / f).write_text(SCRIPT if f == configure.LD_SCRIPT else "")


class Split(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        for f in configure.SPLIT_INPUTS:
            (self.root / f).parent.mkdir(parents=True, exist_ok=True)
            (self.root / f).write_text("x")
        patcher = mock.patch.object(configure, "ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.addCleanup(self.tmp.cleanup)

    def split(self, splat, force=True):
        with mock.patch.object(configure, "run", splat):
            configure.split(force)

    def test_resplit_removes_files_of_an_earlier_layout(self):
        self.split(FakeSplat(["1E048.s", "nonmatchings/a/func_00100000.s"]))
        self.split(FakeSplat(["1E000.s"]))
        files = sorted(p.relative_to(self.root / "asm").as_posix() for p in (self.root / "asm").rglob("*.s"))
        self.assertEqual(files, ["1E000.s"])

    def test_resplit_keeps_the_rest_of_build(self):
        keep = self.root / "build" / "permuter" / "Func" / "settings.toml"
        keep.parent.mkdir(parents=True)
        keep.write_text("x")
        self.split(FakeSplat(["1000.s"]))
        self.assertTrue(keep.exists())

    def test_linker_script_aligned_and_split_marked_done(self):
        self.split(FakeSplat(["1000.s"]))
        self.assertIn(configure.BSS_ALIGNED, (self.root / configure.LD_SCRIPT).read_text())
        self.assertTrue((self.root / configure.SPLIT_DONE).exists())

    def test_up_to_date_split_is_not_rerun(self):
        self.split(FakeSplat(["1000.s"]))
        splat = FakeSplat(["1000.s"])
        self.split(splat, force=False)
        self.assertEqual(splat.calls, 0)

    def test_failed_split_leaves_no_done_mark_and_reruns(self):
        self.split(FakeSplat(["1000.s"]))
        with self.assertRaises(subprocess.CalledProcessError):
            self.split(FakeSplat(["1000.s", "half.s"], fail=True))
        self.assertFalse((self.root / configure.SPLIT_DONE).exists())
        self.assertFalse((self.root / configure.LD_SCRIPT).exists())
        splat = FakeSplat(["1000.s"])
        self.split(splat, force=False)
        self.assertEqual(splat.calls, 1)
        self.assertFalse((self.root / "asm" / "half.s").exists())


class NinjaFile(unittest.TestCase):
    """build.ninja from a tiny tree: one asm object and one C object."""

    def test_c_objects_have_compiler_dependencies(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "build").mkdir()
            (root / configure.LD_SCRIPT).write_text("        build/asm/1000.o(.text);\n        build/src/x.o(.text);\n")
            (root / "src").mkdir()
            (root / "src" / "x.c").write_text('#include "y.h"\n')
            with mock.patch.object(configure, "ROOT", root):
                configure.write_ninja()
            text = (root / "build.ninja").read_text()
        cc = text[text.index("rule cc"):text.index("rule ld")]
        self.assertIn("-Wp,-MD,$out.d", cc)
        self.assertIn("depfile = $out.d", cc)
        self.assertIn("deps = gcc", cc)
        split = next(line for line in text.splitlines() if line.endswith(f": split {' '.join(configure.SPLIT_INPUTS)}"))
        self.assertIn(configure.SPLIT_DONE, split.split(":")[0].split())

    def test_c_objects_get_their_flags_and_the_asm_fix(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "build").mkdir()
            (root / configure.LD_SCRIPT).write_text("        build/src/x.o(.text);\n        build/src/y.o(.text);\n")
            (root / "src").mkdir()
            (root / "src" / "x.c").write_text('INCLUDE_RODATA("asm/nonmatchings/x", D_00500000);\n')
            (root / "src" / "y.c").write_text("")
            with mock.patch.object(configure, "ROOT", root), \
                    mock.patch.object(configure, "NO_FAST_MATH", ("src/x.c",)):
                configure.write_ninja()
                x_flags = configure.cc_flags("src/x.c")
            text = (root / "build.ninja").read_text()
        cc = text[text.index("rule cc"):text.index("rule ld")]
        self.assertIn("--fix-asm $out.s", cc)
        self.assertLess(cc.index("--fix-asm"), cc.index("include/gcc_prelude.inc"))
        lines = text.splitlines()
        x = lines.index(next(l for l in lines if l.startswith("build build/src/x.o: cc")))
        self.assertIn("asm/nonmatchings/x/D_00500000.s", lines[x])
        # the rewrite of the compiler's assembly lives in configure.py: a change to it rebuilds the C objects
        self.assertIn("configure.py", lines[x].split("|", 1)[1].split())
        self.assertIn(f"  cflags = {x_flags}", lines[x + 1:x + 3])
        self.assertNotIn("-ffast-math", x_flags)
        split = next(l for l in lines if l.endswith(f": split {' '.join(configure.SPLIT_INPUTS)}"))
        self.assertIn("asm/nonmatchings/x/D_00500000.s", split.split(":")[0].split())


if __name__ == "__main__":
    unittest.main()
