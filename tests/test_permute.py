"""Tests for the pure helpers of scripts/permute.py (synthetic data only)."""

import importlib.util
import subprocess
import sys
import tempfile
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


class CheckArgs(unittest.TestCase):
    """The C file and the function are checked before anything is written."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / "src" / "sub").mkdir(parents=True)
        (self.root / "src" / "a.c").write_text('INCLUDE_ASM("asm/nonmatchings/a", func_00100000);\nvoid Foo(void) {\n}\n')
        (self.root / "src" / "sub" / "b.c").write_text("void Bar(void) {\n}\n")
        (self.root / "src" / "v.s").write_text("Foo:\n")
        (self.root / "configure.py").write_text("Foo\n")

    def check(self, src, func):
        return permute.check_args(src, func, self.root)

    def test_valid(self):
        self.assertEqual(self.check("src/a.c", "Foo"), "src/a.c")
        self.assertEqual(self.check("./src/a.c", "func_00100000"), "src/a.c")
        self.assertEqual(self.check(str(self.root / "src" / "sub" / "b.c"), "Bar"), "src/sub/b.c")

    def test_file_outside_src_or_not_c(self):
        for src in ("src/../configure.py", "configure.py", "src/v.s", "src/missing.c", "src", "/etc/passwd"):
            with self.subTest(src=src), self.assertRaisesRegex(ValueError, "not a C file under src/"):
                self.check(src, "Foo")

    def test_function_name(self):
        for func in ("", "../../x", "a/b", "Foo bar", "1abc", "Foo;"):
            with self.subTest(func=func), self.assertRaisesRegex(ValueError, "not a function name"):
                self.check("src/a.c", func)

    def test_function_not_in_file(self):
        with self.assertRaisesRegex(ValueError, "no Bar in src/a.c"):
            self.check("src/a.c", "Bar")
        with self.assertRaisesRegex(ValueError, "no Fo in src/a.c"):
            self.check("src/a.c", "Fo")


class CommandLine(unittest.TestCase):
    def run_permute(self, *args):
        return subprocess.run([sys.executable, str(ROOT / "scripts" / "permute.py"), *args], cwd=ROOT,
                              capture_output=True, text=True)

    def test_bad_arguments_give_a_message_not_a_traceback(self):
        for args in (["src/no_such_file.c", "Foo"], ["src/main.c", "../escape"], ["src/main.c", "NoSuchFunction"]):
            with self.subTest(args=args):
                r = self.run_permute(*args)
                self.assertNotEqual(r.returncode, 0)
                self.assertTrue(r.stderr.startswith("permute.py: "), r.stderr)
                self.assertNotIn("Traceback", r.stderr)
                self.assertFalse((permute.WORK / args[1]).exists())


if __name__ == "__main__":
    unittest.main()
