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


if __name__ == "__main__":
    unittest.main()
