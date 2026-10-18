"""Tests for the pure helpers of scripts/names_from_strings.py (synthetic data only)."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
_spec = importlib.util.spec_from_file_location("names_from_strings", ROOT / "scripts" / "names_from_strings.py")
nfs = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(nfs)

DATA = """\
nonmatching D_00500000

dlabel D_00500000
    /* 1C1000 00500000 */ .asciz "sceGsSyncPath: DMA Ch.1 does not terminate\\r\\n"
    /* 00 */
.align 2

dlabel D_00500040
    /* 1C1040 00500040 */ .asciz "_sliceA0(): error happens"

dlabel D_00500080
    /* 1C1080 00500080 */ .asciz "AUTO SAVE:   "
"""

TEXT = """\
glabel func_00340000
    /* 1000 00340000 3C040050 */  lui        $a0, %hi(D_00500000)
    /* 1004 00340004 24840000 */  addiu      $a0, $a0, %lo(D_00500000)
endlabel func_00340000

glabel func_00340100
    /* 1100 00340100 3C040050 */  lui        $a0, %hi(D_00500040)
    /* 1104 00340104 24840040 */  addiu      $a0, $a0, %lo(D_00500080)
"""


class Strings(unittest.TestCase):
    def test_names_in_string(self):
        self.assertEqual(nfs.names_in_string("sceGsSyncPath: DMA Ch.1 does not terminate\\r\\n"),
                         ["sceGsSyncPath"])
        self.assertEqual(nfs.names_in_string("_sliceA0(): error happens"), ["_sliceA0"])
        self.assertEqual(nfs.names_in_string("Ncmd fail sema cur_cmd:%d"), [])
        self.assertEqual(nfs.names_in_string("AUTO SAVE:   "), [])
        self.assertEqual(nfs.names_in_string("%Z%H%V%c%SPOWER     :%8x"), [])

    def test_strings_by_label(self):
        self.assertEqual(nfs.strings_by_label(DATA), {
            "D_00500000": "sceGsSyncPath: DMA Ch.1 does not terminate\\r\\n",
            "D_00500040": "_sliceA0(): error happens",
            "D_00500080": "AUTO SAVE:   ",
        })

    def test_refs_by_function(self):
        self.assertEqual(nfs.refs_by_function(TEXT), {
            "func_00340000": {"D_00500000"},
            "func_00340100": {"D_00500040", "D_00500080"},
        })


class Suggest(unittest.TestCase):
    def test_one_to_one_only(self):
        strings = {"D_1": "sceA: x", "D_2": "sceB: y", "D_3": "sceB: z", "D_4": "sceC: w", "D_5": "sceD: v"}
        refs = {
            "func_00000010": {"D_1"},           # 1:1 -> suggested
            "func_00000020": {"D_2"},           # sceB used by two functions -> skipped
            "func_00000030": {"D_3"},
            "func_00000040": {"D_4", "D_5"},    # two names -> skipped
            "Named": {"D_1"},                   # already named -> ignored (and does not count as owner)
        }
        self.assertEqual(nfs.suggest(strings, refs), [("sceA", 0x10, "sceA: x")])


if __name__ == "__main__":
    unittest.main()
