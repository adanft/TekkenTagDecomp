"""The 64-bit program (TTT_CC=clang64, T42 b3; the default build since T42 b5, docs/port/README.md "64-bit"):
port/tools/build.py builds an x86-64 program into build/port/ (the 32-bit gcc build, build/port-gcc/ttt, is not
touched). The game's code goes through
port/tools/ptr32.py (its pointers 4 bytes wide), clang -emit-llvm, port/tools/irfix.py and llc; the port's code is
ordinary 64-bit code; the program is linked at 0x08000000 with the heap guard page (link.py BRK_GUARD). The self-tests
pass, a crash report is written, and the 2551-VBlank idle run and the scripted fight end in exactly the gcc build's
state: the same state line, the reference hash, the same hash for every region and the same pictures. Skipped
without clang, llc or libclang's Python bindings; slow (a build and five long runs, a few minutes):
TTT_SLOW_TESTS=1."""

import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from test_port_clang import FIGHT_HASH, SELFTESTS, run
from test_port_crash import no_core
from test_port_data import IDLE_HASH
from test_port_float import check_no_host_float_code
from test_port_input import IN_THE_FIGHT, TO_THE_FIGHT, stream
from test_port_platform import GCC_EXE as EXE, ROOT, disc

C64 = ROOT / "build/port"  # the default build
C64_EXE = C64 / "ttt"


def have_bindings():
    """Whether an interpreter that port/tools/undefined.py would use for ptr32.py has libclang's Python bindings."""
    sys.path.insert(0, str(ROOT / "port/tools"))
    try:
        import undefined
        py = undefined.ptr32_python()
    finally:
        sys.path.pop(0)
    return subprocess.run([py, "-c", "import clang.cindex"], capture_output=True).returncode == 0


def elf_header(path):
    """(class, machine) of an ELF file: (2, 62) for x86-64."""
    b = Path(path).read_bytes()[:20]
    return b[4], int.from_bytes(b[18:20], "little")


class Clang64Helpers(unittest.TestCase):
    """The variant's pure helpers (no compiler needed)."""

    @classmethod
    def setUpClass(cls):
        sys.path.insert(0, str(ROOT / "port/tools"))
        import irfix
        import link
        import toolchain
        cls.irfix, cls.link, cls.toolchain = irfix, link, toolchain

    @classmethod
    def tearDownClass(cls):
        sys.path.remove(str(ROOT / "port/tools"))

    def test_irfix_static_data(self):
        """Addresses in static data become i32: a plain one through ptrtoint, an offset one (clang's switch tables) with
        the offset applied to the ordinary pointer first."""
        line = ("@t = private unnamed_addr constant [2 x ptr addrspace(271)] [ptr addrspace(271) getelementptr inbounds "
                "nuw (i8, ptr addrspace(271) addrspacecast (ptr @D_008B8068 to ptr addrspace(271)), i32 7), "
                "ptr addrspace(271) addrspacecast (ptr @D to ptr addrspace(271))], align 4")
        self.assertEqual(self.irfix.data_line(line),
                         "@t = private unnamed_addr constant [2 x i32] [i32 ptrtoint (ptr getelementptr inbounds nuw "
                         "(i8, ptr @D_008B8068, i32 7) to i32), i32 ptrtoint (ptr @D to i32)], align 4")

    def test_data_table_as_assembly(self):
        """The 64-bit variant writes the rewrite table as assembly (clang takes no truncated symbol + addend as a C
        initializer there), with the same entries in the same order."""
        layout = {"ranges": [(0x415700, 0x10)], "words": [(0x415714, "gThing", -4), (0x415710, "func_00350000", 0)],
                  "zeros": [0x415718], "payload": (0x10, "00" * 20), "elf": ("SLUS_200.01", 1, "00" * 20)}
        c = self.link.data_table(layout, asm=True)
        self.assertIn('"\\t.long 0x00415710, func_00350000+(0)\\n"\n'
                      '        "\\t.long 0x00415714, gThing+(-4)\\n"\n'
                      '        "\\t.long 0x00415718, 0\\n"', c)
        self.assertIn("const uint32_t gPortDataFixN = 3;", c)
        self.assertNotIn("const uint32_t gPortDataFix[][2] = {", c)

    def test_bits(self):
        """-m64 for -m32 and no -malign-double in the 64-bit variant; the 32-bit builds' flags unchanged."""
        flags = ["-m32", "-O2", "-malign-double"]
        if self.toolchain.BITS64:
            self.assertEqual(self.toolchain.bits(flags), ["-m64", "-O2"])
        else:
            self.assertEqual(self.toolchain.bits(flags), flags)


@unittest.skipUnless(shutil.which("clang") and have_bindings(), "no clang / libclang's Python bindings")
class Ptr32Rewrite(unittest.TestCase):
    def test_variadic_calls_fill_the_argument_registers(self):
        """A game call of a variadic function gets zeros up to six arguments (the registers its entry stores on the EE
        stack: none left holding stale host values), its pointer arguments widened; a call with six or more and a
        call of a fixed function stay as they are."""
        sys.path.insert(0, str(ROOT / "port/tools"))
        try:
            import undefined
            py = undefined.ptr32_python()
        finally:
            sys.path.pop(0)
        src = (f'# 1 "{ROOT}/src/x.c"\n'
               "int P(const char *f, ...);\nint F(int a);\n"
               "void g(char *s) { P(\"%d\", 1); P(\"%s\", s); P(\"\", 1, 2, 3, 4, 5); F(2); }\n")
        with tempfile.TemporaryDirectory() as d:
            i, o = Path(d) / "x.i", Path(d) / "x.p32.i"
            i.write_text(src)
            r = subprocess.run([py, str(ROOT / "port/tools/ptr32.py"), str(i), str(o), "-fms-extensions", "-m64"],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            out = o.read_text()
        self.assertIn('P("%d", 1, 0LL, 0LL, 0LL, 0LL);', out)
        self.assertIn('P("%s", ((void * __ptr64)(s)), 0LL, 0LL, 0LL, 0LL);', out)
        self.assertIn('P("", 1, 2, 3, 4, 5);', out)
        self.assertIn("F(2);", out)


@unittest.skipUnless(shutil.which("clang") and shutil.which("llc"), "no clang / llc")
@unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (a few minutes): TTT_SLOW_TESTS=1")
class Clang64Build(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not have_bindings():
            raise unittest.SkipTest("no libclang Python bindings (clang.cindex) for port/tools/ptr32.py")
        before = EXE.stat().st_mtime_ns if EXE.exists() else None
        cls.build = subprocess.run([sys.executable, str(ROOT / "port/tools/build.py")], cwd=ROOT,
                                   env=dict(os.environ, TTT_CC="clang64"), capture_output=True, text=True, timeout=3600)
        cls.gcc_untouched = (EXE.stat().st_mtime_ns if EXE.exists() else None) == before

    def test_build(self):
        """Every object compiles and links, into build/port/ only."""
        out = self.build.stdout + self.build.stderr
        self.assertEqual(self.build.returncode, 0, out[-3000:])
        m = re.search(r"undefined: (\d+) of (\d+) objects compiled \((\d+) failed\)", out)
        self.assertIsNotNone(m, out[-3000:])
        self.assertEqual((m.group(1), m.group(3)), (m.group(2), "0"))
        self.assertIn("link OK -> build/port/ttt", out)
        self.assertTrue(self.gcc_untouched)

    def test_x86_64_below_4gb(self):
        """An x86-64 program: every object and the program are ELF64 x86-64; the game's objects come through ptr32.py
        (their rewritten text has `__ptr32 __uptr`); the image starts at 0x08000000 and ends inside the state hash's
        window (below 0x09000000), and the heap guard page is a segment of its own at 0x7FFFF000."""
        objs = sorted((C64 / "obj").glob("*.o"))
        self.assertGreater(len(objs), 100)
        self.assertEqual({o.name for o in objs if elf_header(o) != (2, 62)}, set())
        self.assertEqual(elf_header(C64_EXE), (2, 62))
        self.assertIn("__ptr32 __uptr", (C64 / "obj/main.p32.i").read_text(encoding="latin-1"))
        out = subprocess.run(["readelf", "-lW", str(C64_EXE)], capture_output=True, text=True, check=True).stdout
        loads = [(int(m.group(1), 16), int(m.group(2), 16))
                 for m in re.finditer(r"^\s+LOAD\s+\S+\s+(0x[0-9a-f]+)\s+\S+\s+\S+\s+(0x[0-9a-f]+)", out, re.M)]
        image = [(a, n) for a, n in loads if a < 0x40000000]
        self.assertEqual(min(a for a, _ in image), 0x08000000)
        self.assertLessEqual(max(a + n for a, n in image), 0x09000000)
        self.assertIn((0x7FFFF000, 0x1000), loads)

    def test_no_host_float_code(self):
        """tests/test_port_float.py's rules on the 64-bit program: no x87 or SSE float instruction outside the SIMD files
        and the overlay (-mno-sse -mno-x87 -msoft-float on x86-64 too)."""
        check_no_host_float_code(self, C64_EXE, C64 / "obj")

    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_selftests(self):
        """The self-tests run on the 64-bit program as on the 32-bit one."""
        with tempfile.TemporaryDirectory() as d:
            for name in SELFTESTS:
                with self.subTest(name=name):
                    r = run(C64_EXE, os.path.join(d, "saves"), timeout=600, TTT_SELFTEST=name, TTT_FRAMES="1")
                    self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                    self.assertRegex(r.stdout, rf"ttt: {name if name != 'mem' else 'memory'} self-test OK")

    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_crash_report(self):
        """A fault (TTT_CRASH_TEST) gives the crash report on the 64-bit program too."""
        with tempfile.TemporaryDirectory(prefix="ttt-crash-") as d:
            e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
            e.update(TTT_DISC=str(disc()), TTT_MOVIES="0", TTT_FRAMES="100", TTT_SEED="1",
                     TTT_SAVES=os.path.join(d, "saves"), TTT_CONFIG=os.path.join(d, "settings.ini"),
                     TTT_TEXTURES=os.path.join(d, "textures"), TTT_CRASH_TEST="segv@30")
            r = subprocess.run([str(C64_EXE)], env=e, cwd=d, capture_output=True, text=True, timeout=300,
                               preexec_fn=no_core)
            self.assertEqual(r.returncode, -signal.SIGSEGV, r.stdout[-1500:] + r.stderr[-1500:])
            m = re.search(r"ttt: crash: SIGSEGV; report written to (\S+)\n", r.stderr)
            self.assertIsNotNone(m, r.stderr[-1500:])
            text = Path(m.group(1)).read_text()
            self.assertTrue(text.startswith("Tekken Tag Tournament PC port: crash report\n"))
            self.assertRegex(text, r"\nsignal: SIGSEGV \(11\)")

    def same_state(self, reference, frames, **env):
        """Runs the gcc build and the 64-bit one with TTT_SEED=1 and the given variables: the same state line with the
        reference hash, every region hash alike, the same pictures (every 150 VBlanks). Returns the number of
        pictures."""
        with tempfile.TemporaryDirectory() as d:
            res = {}
            for name, exe in (("gcc", EXE), ("clang64", C64_EXE)):
                dump = os.path.join(d, "dump-" + name)
                os.makedirs(dump)
                r = run(exe, os.path.join(d, "saves"), TTT_FRAMES=frames, TTT_SEED="1", TTT_HASH_REGIONS="1",
                        TTT_DUMP=dump, TTT_DUMP_EVERY="150", **env)
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                state = re.search(r"ttt: state: .*", r.stdout)
                self.assertIsNotNone(state, r.stdout[-2000:])
                pictures = {p.name: p.read_bytes() for p in sorted(Path(dump).iterdir())}
                res[name] = (state.group(0), re.findall(r"ttt: hash-region (\S+) (\w+)", r.stdout), pictures)
            self.assertIn(f"hash {reference} ", res["gcc"][0])
            self.assertEqual(res["clang64"][0], res["gcc"][0])
            self.assertTrue(res["gcc"][1])
            self.assertEqual(res["clang64"][1], res["gcc"][1])
            self.assertEqual(sorted(res["clang64"][2]), sorted(res["gcc"][2]))
            self.assertTrue(res["clang64"][2] == res["gcc"][2], "the pictures differ")
            return len(res["gcc"][2])

    @unittest.skipUnless(EXE.exists(), "build/port-gcc/ttt not built (TTT_CC=gcc port/tools/build.py)")
    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_idle_run_same_state(self):
        """The idle run (TTT_FRAMES=2551 TTT_SEED=1, 17 pictures) ends in the gcc build's state."""
        self.assertEqual(self.same_state(IDLE_HASH, "2551"), 17)

    @unittest.skipUnless(EXE.exists(), "build/port-gcc/ttt not built (TTT_CC=gcc port/tools/build.py)")
    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_fight_same_state(self):
        """The scripted fight (1451 VBlanks) ends in the gcc build's state."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
            self.same_state(FIGHT_HASH, "1451", TTT_INPUT_PLAY=str(p))


if __name__ == "__main__":
    unittest.main()
