"""Tests for the pure helpers of scripts/permute.py (synthetic data only)."""

import importlib.util
import os
import re
import shlex
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


class ObjdumpScript(unittest.TestCase):
    """objdump.sh keeps the whole function: splat's jump-table labels (`jlabel .L...`) are symbols in target.o, but
    the compiler's are not, so the scored disassembly must neither stop at them nor print them."""

    DUMP = """
target.o:     file format elf32-tradlittlemips

Disassembly of section .text:

00000000 <func_00100000>:
   0:\t27bdfff0 \taddiu\tsp,sp,-16
   4:\t00800008 \tjr\ta0
   8:\t00000000 \tnop

0000000c <.L0010000C>:
   c:\t24020001 \tli\tv0,1

00000010 <.L00100010>:
  10:\t03e00008 \tjr\tra
  14:\t27bd0010 \taddiu\tsp,sp,16

00000018 <func_00100018>:
  18:\t03e00008 \tjr\tra
  1c:\t00000000 \tnop
"""

    def filter(self, func):
        """Runs the awk program of the generated objdump.sh on DUMP."""
        script = permute.objdump_sh(func)
        prog = re.search(r"\| awk '(.*)'\s*$", script, re.M)
        self.assertIsNotNone(prog, script)
        r = subprocess.run(["awk", prog.group(1)], input=self.DUMP, capture_output=True, text=True, check=True)
        return r.stdout

    def test_whole_function_through_jump_table_labels(self):
        out = self.filter("func_00100000")
        self.assertTrue(out.startswith("00000000 <func_00100000>:\n"), out)
        self.assertIn("li\tv0,1", out)
        self.assertIn("addiu\tsp,sp,16", out)
        self.assertNotIn("<.L", out)
        self.assertNotIn("func_00100018", out)

    def test_next_function_alone(self):
        out = self.filter("func_00100018")
        self.assertTrue(out.startswith("00000018 <func_00100018>:\n"), out)
        self.assertNotIn("sp,sp", out)


class CompileScript(unittest.TestCase):
    """compile.sh compiles like the build's cc rule: the file's own flags (configure.cc_flags, without -ffast-math for
    the files in NO_FAST_MATH) and the configure.py --fix-asm rewrite between the compiler and the assembler. The
    generated script runs with stand-ins for the compiler (records its arguments, writes a fixed .s) and the assembler
    (copies its input .s to the output)."""

    ASM = "\t.text\n\tli.s\t$f0,2048.0\n"

    def run_compile_sh(self, src):
        tmp = Path(tempfile.mkdtemp())
        self.addCleanup(lambda: subprocess.run(["rm", "-rf", str(tmp)]))
        log = tmp / "cc.args"
        (tmp / "asm.txt").write_text(self.ASM)
        cc = tmp / "cc"
        cc.write_text(f'#!/bin/bash\nprintf "%s\\n" "$@" > {shlex.quote(str(log))}\n'
                      'while [ $# -gt 0 ]; do [ "$1" = -o ] && out="$2"; shift; done\n'
                      f'cp {shlex.quote(str(tmp / "asm.txt"))} "$out"\n')
        as_ = tmp / "as"
        as_.write_text('#!/bin/bash\nwhile [ $# -gt 0 ]; do if [ "$1" = -o ]; then out="$2"; shift; else prev="$1"; fi;'
                       ' shift; done\ncp "$prev" "$out"\n')
        cc.chmod(0o755)
        as_.chmod(0o755)
        script = permute.compile_sh(src)
        for real, stub in ((permute.ROOT / permute.configure.CC, cc), (permute.AS, as_)):
            self.assertIn(shlex.quote(str(real)), script)
            script = script.replace(shlex.quote(str(real)), shlex.quote(str(stub)))
        (tmp / "compile.sh").write_text(script)
        (tmp / "in.c").write_text("int x;\n")
        subprocess.run(["bash", str(tmp / "compile.sh"), str(tmp / "in.c"), "-o", str(tmp / "out.o")], check=True,
                       capture_output=True, text=True)
        return log.read_text().split("\n"), (tmp / "out.o").read_text()

    def test_fast_math_file_keeps_the_flag(self):
        args, _ = self.run_compile_sh("src/main.c")
        self.assertNotIn("src/main.c", permute.configure.NO_FAST_MATH)
        self.assertIn("-ffast-math", args)

    def test_no_fast_math_file_drops_the_flag(self):
        src = permute.configure.NO_FAST_MATH[0]
        args, _ = self.run_compile_sh(src)
        self.assertNotIn("-ffast-math", args)
        for flag in permute.configure.cc_flags(src).split():
            if not flag.startswith("-I"):
                self.assertIn(flag, args)

    def test_fix_asm_runs_before_the_assembler(self):
        _, out = self.run_compile_sh("src/main.c")
        self.assertEqual(out, permute.configure.fix_compiler_asm(self.ASM))
        self.assertIn("lui\t$1,0x4500", out)
        self.assertNotIn("li.s", out)


class DefaultJobs(unittest.TestCase):
    """--run without -jN uses $COOL_JOBS (set by scripts/cool.sh), else every core."""

    def test_default_is_every_core(self):
        self.assertEqual(permute.default_jobs({}), str(os.cpu_count() or 1))

    def test_cool_jobs(self):
        self.assertEqual(permute.default_jobs({"COOL_JOBS": "2"}), "2")

    def test_bad_cool_jobs_falls_back(self):
        for bad in ("", "0", "-3", "four", "2x"):
            with self.subTest(bad=bad):
                self.assertEqual(permute.default_jobs({"COOL_JOBS": bad}), str(os.cpu_count() or 1))


class ParseArgs(unittest.TestCase):
    """Command line: <src> <Func> [--run] [-jN | -j N] [-- permuter options]."""

    def test_forms(self):
        env = {}
        self.assertEqual(permute.parse_args(["src/a.c", "F"], env), (["src/a.c", "F"], False, str(os.cpu_count() or 1), []))
        self.assertEqual(permute.parse_args(["src/a.c", "F", "--run", "-j8"], env), (["src/a.c", "F"], True, "8", []))
        self.assertEqual(permute.parse_args(["src/a.c", "F", "--run", "-j", "2"], env), (["src/a.c", "F"], True, "2", []))
        self.assertEqual(permute.parse_args(["src/a.c", "F", "--run", "--", "--stop-on-zero"], {"COOL_JOBS": "3"}),
                         (["src/a.c", "F"], True, "3", ["--stop-on-zero"]))

    def test_bad(self):
        for args in (["src/a.c"], ["src/a.c", "F", "-j"], ["src/a.c", "F", "-j", "x"], ["src/a.c", "F", "-x"],
                     ["src/a.c", "F", "G"]):
            with self.subTest(args=args), self.assertRaises(ValueError):
                permute.parse_args(args, {})


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
