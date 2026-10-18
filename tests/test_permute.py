"""Tests for the pure helpers of scripts/permute.py (synthetic data only)."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
_spec = importlib.util.spec_from_file_location("permute", ROOT / "scripts" / "permute.py")
permute = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(permute)


class EnableAttempt(unittest.TestCase):
    def test_live_definition_unchanged(self):
        text = "int f(int x) {\n    return x;\n}\n"
        self.assertEqual(permute.enable_attempt(text, "f"), text)

    def test_enables_if_0_around_target(self):
        text = ("#if 0\nint f(int x) {\n    return x;\n}\n#else\nINCLUDE_ASM(\"asm/nonmatchings/a\", f);\n#endif\n"
                "#if 0\nint g(void) { return 1; }\n#endif\n")
        out = permute.enable_attempt(text, "f")
        self.assertTrue(out.startswith("#if 1\nint f(int x) {"))
        self.assertIn("#if 0\nint g(void)", out)

    def test_prototype_is_not_a_definition(self):
        text = "int f(int x);\nint g(void) { return f(1); }\n"
        with self.assertRaises(ValueError):
            permute.enable_attempt(text, "f")

    def test_missing_function(self):
        with self.assertRaises(ValueError):
            permute.enable_attempt("int g(void) { return 0; }\n", "f")

    def test_definition_in_else_branch_refused(self):
        text = "#if 0\nINCLUDE_ASM(\"x\", f);\n#else\nint f(void) { return 0; }\n#endif\n"
        with self.assertRaises(ValueError):
            permute.enable_attempt(text, "f")


class Settings(unittest.TestCase):
    def test_settings_toml(self):
        out = permute.settings_toml("func_00340AF0", "/w/objdump.sh")
        self.assertIn('func_name = "func_00340AF0"\n', out)
        self.assertIn('compiler_type = "gcc"\n', out)
        self.assertIn('objdump_command = "/w/objdump.sh"\n', out)


if __name__ == "__main__":
    unittest.main()
