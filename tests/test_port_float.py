"""The PC build's EE float model (port/src/softfloat_ps2.c), maths library (third_party/newlib_libm) and newlib rand,
checked on the built program build/port/ttt (TTT_SELFTEST=float, port/src/plat_floattest.c), plus the build's rule
that no host float code is linked. The program tests are skipped when the program has not been built."""

import importlib.util
import re
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "build/port/ttt"
TOOLS = ROOT / "port" / "tools"
sys.path.insert(0, str(TOOLS))


def _load(name):
    spec = importlib.util.spec_from_file_location("port_" + name, TOOLS / f"{name}.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


toolchain = _load("toolchain")

# x87 instructions that compute or move floats, and scalar SSE float instructions (on an %xmm register).
X87 = re.compile(r"\t(fld|fst|fadd|fsub|fmul|fdiv|fsqrt|fsin|fcos|fsincos|fpatan|fptan|fild|fist|fcom|fucom|fyl2x"
                 r"|f2xm1|fscale|fprem|frndint|fchs|fabs)\w*\b")
SSE = re.compile(r"\t\w+(ss|sd|ps|pd)\s.*%xmm")
FUNC = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")


class VuRenameTest(unittest.TestCase):
    def test_vu_files_get_the_vector_unit_arithmetic(self):
        args = toolchain.vu_redefine_args()
        self.assertIn("--redefine-sym=__addsf3=Port_VuAdd", args)
        self.assertIn("--redefine-sym=__subsf3=Port_VuSub", args)
        self.assertIn("--redefine-sym=__mulsf3=Port_VuMul", args)
        self.assertIn("--redefine-sym=__divsf3=Port_VuDiv", args)
        self.assertIn("plat_vu0.c", toolchain.VU_FILES)
        self.assertIn("vu0_pc.c", toolchain.VU_FILES)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class FloatSelfTest(unittest.TestCase):
    def test_float_self_test(self):
        """PCSX2 data for every FPU / VU operation class, EE rules, exact libm cases and wrapper error results, and
        newlib's rand stream on the game's state word."""
        r = subprocess.run([str(EXE)], env={"TTT_SELFTEST": "float"}, capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: float self-test OK", r.stdout)
        self.assertIn("pcsx2 (verified): 1500 cases", r.stdout)
        self.assertIn("ttt: float: libvu0:", r.stdout)

    def test_no_host_float_code(self):
        """Every float operation goes through the EE model: the program's code has no x87 float instruction, and SSE
        float instructions only in the functions of toolchain.SIMD_FILES (the predecoded VU interpreter, which runs
        the model on the SSE unit in round-toward-zero / flush-to-zero mode, and its test against the integer
        model)."""
        allowed = set()
        for name in toolchain.SIMD_FILES:
            obj = toolchain.OBJ / f"pc_{Path(name).stem}.o"
            self.assertTrue(obj.exists(), obj)
            syms = subprocess.run(["nm", "--defined-only", str(obj)], capture_output=True, text=True, check=True).stdout
            allowed |= {l.split()[2] for l in syms.splitlines() if len(l.split()) == 3 and l.split()[1] in "tT"}
        self.assertIn("run_fast", allowed)
        out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", str(EXE)], capture_output=True, text=True,
                             check=True).stdout
        bad, fn = [], None
        for line in out.splitlines():
            m = FUNC.match(line)
            if m:
                fn = m.group(1).split(".")[0]  # run_fast, or run_fast.part.0 / .constprop.0 / .isra.0
                continue
            if X87.search(line) or (SSE.search(line) and fn not in allowed):
                bad.append(f"{fn}: {line.strip()}")
        self.assertEqual(bad[:10], [])

    def test_no_temporary_host_float(self):
        self.assertFalse((ROOT / "port/src/plat_float_tmp.c").exists())


if __name__ == "__main__":
    unittest.main()
