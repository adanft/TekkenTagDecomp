"""Tests for the pure helpers of configure.py (no disc or splat run needed).

Run: .venv/bin/python -m unittest discover -s tests -v
"""

import importlib.util
import unittest
from pathlib import Path

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


if __name__ == "__main__":
    unittest.main()
