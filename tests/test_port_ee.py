"""The draft divergence check of the PC build (port/src/ee_diff.c): a small EE interpreter runs the executable's own
code next to the port's C. Checked on the built program build/port/ttt; needs the matching build's output
build/SLUS_200.01 (made from your own SLUS_200.01), else skipped.

- TTT_SELFTEST=ee: the interpreter against port code known to behave like the original (libvu0, matched game C,
  newlib's libm, the VU0 rotation builders), on fixed and generated inputs.
- TTT_DIFF on the attract demo's fight (slow, about two minutes; TTT_SLOW_TESTS=1): the fighter skeleton of
  func_003478F0 and the calls it makes (PORT_DIFF hooks) give the same memory as the original code."""

import os
import re
import unittest

from test_port_platform import EXE, ROOT, disc, run_ttt

ELF = ROOT / "build/SLUS_200.01"


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(ELF.exists(), "build/SLUS_200.01 not built (configure.py + ninja)")
class EeSelfTest(unittest.TestCase):
    def test_interpreter_agrees_with_the_port_code_it_checks_against(self):
        r = run_ttt(TTT_SELFTEST="ee")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: ee self-test OK", r.stdout)

    def test_known_libm_divergences_are_only_the_third_party_constants(self):
        r = run_ttt(TTT_SELFTEST="ee")
        known = re.findall(r"ee: KNOWN (\S+):", r.stdout)
        self.assertTrue(set(known) <= {"asinf", "__ieee754_asinf", "atan2f"}, r.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(ELF.exists(), "build/SLUS_200.01 not built (configure.py + ninja)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (about two minutes): TTT_SLOW_TESTS=1")
class SkeletonDrafts(unittest.TestCase):
    HOOKS = ["func_003478F0", "func_003DD918", "func_003DE0D8", "func_00347E28", "func_00351E98", "func_0034D258",
             "func_003DE4E0", "func_00351AA0", "func_0034CC38"]

    def test_fighter_skeleton_matches_the_original_code(self):
        r = run_ttt(timeout=900, TTT_FRAMES="1601", TTT_SEED="1", TTT_DIFF="1600:2", TTT_DIFF_ONLY=",".join(self.HOOKS))
        self.assertEqual(r.returncode, 0, r.stderr)
        lines = re.findall(r"^ee-diff (\S+) \(VBlank \d+\): (.*)$", r.stdout, re.M)
        seen = {name for name, _ in lines}
        self.assertEqual(seen, set(self.HOOKS), r.stdout[-4000:])
        for name, what in lines:
            self.assertTrue(what.startswith("same,"), f"{name}: {what}")


if __name__ == "__main__":
    unittest.main()
