"""The program through clang (TTT_CC=clang, T42 b1 / b2, docs/port/README.md gotcha 32): port/tools/build.py compiles
every object into its own tree, build/port-clang/ (the gcc build, build/port-gcc/ttt, is not touched), every C object
by clang (the game, port/src, newlib's libm, the MPEG-2 decoder, the generated table and stubs) and the overlay by clang++,
with the gcc build's PLT (the C library calls gcc sends through the GOT go through GOT thunks) and no host float code
outside the overlay and the SIMD files. The program's self-tests and a crash report work, and the 2551-VBlank idle run
and the scripted fight end in exactly the gcc build's state: the same state line (seed, RNG spin, hash, cycles), the
reference hashes, the same hash for every region and the same pictures. Skipped without clang; slow (a build and four
long runs, a few minutes): TTT_SLOW_TESTS=1."""

import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from test_port_crash import no_core
from test_port_data import IDLE_HASH
from test_port_float import check_no_host_float_code, check_overlay_links_no_host_maths_or_cxx_runtime
from test_port_input import IN_THE_FIGHT, TO_THE_FIGHT, stream
from test_port_platform import GCC_EXE as EXE, ROOT, disc

CLANG = ROOT / "build/port-clang"
CLANG_EXE = CLANG / "ttt"
# The scripted fight (TO_THE_FIGHT + IN_THE_FIGHT, 1451 VBlanks), determinism.md. R2: 0xcce5eb554fa51395 before; its
# 100 frames of forward walk no longer end in Yoshimitsu's fall (docs/port/README.md gotcha 33).
FIGHT_HASH = "0xa1623a6d44cb57a6"
SELFTESTS = ("hw", "rpc", "state", "mem", "pad", "gs", "vu1", "spu", "mc", "ee", "settings", "float", "window", "gl",
             "texpack")


def run(exe, saves, timeout=900, **env):
    """The program `exe` headless with only the given TTT_ variables, the memory card and settings in `saves` (the same
    path for both builds: the card's file path passes through EE RAM)."""
    shutil.rmtree(saves, ignore_errors=True)
    os.makedirs(saves)
    e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
    if disc() is not None:
        e["TTT_DISC"] = str(disc())
    e.update(TTT_MOVIES="0", TTT_SAVES=saves, TTT_CONFIG=os.path.join(saves, "settings.ini"),
             TTT_TEXTURES=os.path.join(saves, "textures"), **env)
    return subprocess.run([str(exe)], env=e, capture_output=True, text=True, timeout=timeout)


def plt_names(exe):
    """The names of the program's PLT entries (.plt and .plt.got), in address order."""
    out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", "-j", ".plt", "-j", ".plt.got", str(exe)],
                         capture_output=True, text=True, check=True).stdout
    return re.findall(r"<(\w+)@plt>:", out)


def compilers(objs):
    """{object name: the compiler its .comment section names, or "none"}."""
    out = {}
    for o in objs:
        t = subprocess.run(["readelf", "-p", ".comment", str(o)], capture_output=True, text=True).stdout
        m = re.search(r"(clang version [\d.]+|GCC: \([^)]*\) [\d.]+)", t)
        out[o.name] = m.group(1) if m else "none"
    return out


@unittest.skipUnless(shutil.which("clang"), "no clang")
@unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (a few minutes): TTT_SLOW_TESTS=1")
class ClangBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        before = EXE.stat().st_mtime_ns if EXE.exists() else None
        cls.build = subprocess.run([sys.executable, str(ROOT / "port/tools/build.py")], cwd=ROOT,
                                   env=dict(os.environ, TTT_CC="clang"), capture_output=True, text=True, timeout=3600)
        cls.gcc_untouched = (EXE.stat().st_mtime_ns if EXE.exists() else None) == before

    def test_build(self):
        """Every object compiles and links, into build/port-clang/ only."""
        out = self.build.stdout + self.build.stderr
        self.assertEqual(self.build.returncode, 0, out[-3000:])
        m = re.search(r"undefined: (\d+) of (\d+) objects compiled \((\d+) failed\)", out)
        self.assertIsNotNone(m, out[-3000:])
        self.assertEqual((m.group(1), m.group(3)), (m.group(2), "0"))
        self.assertIn("link OK -> build/port-clang/ttt", out)
        self.assertTrue(self.gcc_untouched)

    def test_every_object_by_clang(self):
        """Every object of the program is clang's: the game, port/src (C, and the C++ overlay by clang++), Dear ImGui,
        newlib's libm, the MPEG-2 decoder, the data table and the stubs. The GOT thunks are clang's assembly."""
        objs = sorted((CLANG / "obj").glob("*.o"))
        by = compilers(objs + [CLANG / "data_table.o", CLANG / "stubs.o"])
        for prefix in ("pc_", "nl_", "tp_", "im_"):
            self.assertTrue(any(n.startswith(prefix) for n in by), prefix)
        self.assertGreater(len(by), 100)
        self.assertEqual({n: c for n, c in by.items() if not c.startswith("clang version")}, {})
        self.assertTrue((CLANG / "got_thunks.o").exists())

    @unittest.skipUnless(EXE.exists(), "build/port-gcc/ttt not built (TTT_CC=gcc port/tools/build.py)")
    def test_plt_as_gcc(self):
        """The C library calls the gcc build sends through the GOT (`noplt`, -fno-plt) go through the GOT thunks: the
        PLT is the gcc build's, entry for entry (48), so none of tests/test_port_window.py's names is in it."""
        gcc, clang = plt_names(EXE), plt_names(CLANG_EXE)
        self.assertEqual(len(gcc), 48)
        self.assertEqual(clang, gcc)
        syms = subprocess.run(["nm", str(CLANG_EXE)], capture_output=True, text=True, check=True).stdout
        for f in ("pthread_create", "dlopen", "stat", "sigaction", "vsprintf", "qsort"):
            self.assertRegex(syms, rf" [tT] __ttt_got_{f}\n", f)

    def test_no_host_float_code(self):
        """tests/test_port_float.py's rules on the clang program: no x87 or SSE float instruction outside the SIMD files
        and the overlay (-mno-x87 with -msoft-float), and the overlay calls no C library maths and no C++ run time."""
        check_no_host_float_code(self, CLANG_EXE, CLANG / "obj")
        check_overlay_links_no_host_maths_or_cxx_runtime(self, CLANG / "obj")

    def test_selftests(self):
        """The self-tests of the platform, the GS / VU1, sound, memory card, the EE interpreter, the settings, the EE
        float model, the window's key mapping, the GL back end and the texture packs."""
        self.assertTrue(CLANG_EXE.exists())
        with tempfile.TemporaryDirectory() as d:
            for name in SELFTESTS:
                with self.subTest(name=name):
                    r = run(CLANG_EXE, os.path.join(d, "saves"), timeout=600, TTT_SELFTEST=name, TTT_FRAMES="1")
                    self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                    self.assertRegex(r.stdout, rf"ttt: {name if name != 'mem' else 'memory'} self-test OK")
                    if name == "float":
                        self.assertIn("pcsx2 (verified): 1500 cases", r.stdout)

    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_crash_report(self):
        """A fault (TTT_CRASH_TEST, tests/test_port_crash.py) gives the crash report on the clang program too."""
        with tempfile.TemporaryDirectory(prefix="ttt-crash-") as d:
            e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
            e.update(TTT_DISC=str(disc()), TTT_MOVIES="0", TTT_FRAMES="100", TTT_SEED="1",
                     TTT_SAVES=os.path.join(d, "saves"), TTT_CONFIG=os.path.join(d, "settings.ini"),
                     TTT_TEXTURES=os.path.join(d, "textures"), TTT_CRASH_TEST="segv@30")
            r = subprocess.run([str(CLANG_EXE)], env=e, cwd=d, capture_output=True, text=True, timeout=300,
                               preexec_fn=no_core)
            self.assertEqual(r.returncode, -signal.SIGSEGV, r.stdout[-1500:] + r.stderr[-1500:])
            m = re.search(r"ttt: crash: SIGSEGV; report written to (\S+)\n", r.stderr)
            self.assertIsNotNone(m, r.stderr[-1500:])
            text = Path(m.group(1)).read_text()
            self.assertTrue(text.startswith("Tekken Tag Tournament PC port: crash report\n"))
            self.assertRegex(text, r"\nsignal: SIGSEGV \(11\)")

    def same_state(self, reference, frames, **env):
        """Runs both builds with TTT_SEED=1 and the given variables: the same state line, the reference hash, the same
        hash for every region (TTT_HASH_REGIONS), so no stale stack byte of either compiler reaches the state, and the
        same pictures (TTT_DUMP every 150 VBlanks). Returns the number of pictures."""
        self.assertTrue(CLANG_EXE.exists())
        with tempfile.TemporaryDirectory() as d:
            res = {}
            for name, exe in (("gcc", EXE), ("clang", CLANG_EXE)):
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
            self.assertEqual(res["clang"][0], res["gcc"][0])
            self.assertTrue(res["gcc"][1])
            self.assertEqual(res["clang"][1], res["gcc"][1])
            self.assertEqual(sorted(res["clang"][2]), sorted(res["gcc"][2]))
            self.assertTrue(res["clang"][2] == res["gcc"][2], "the pictures differ")
            return len(res["gcc"][2])

    @unittest.skipUnless(EXE.exists(), "build/port-gcc/ttt not built (TTT_CC=gcc port/tools/build.py)")
    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_idle_run_same_state(self):
        """The idle run (TTT_FRAMES=2551 TTT_SEED=1, 17 pictures) ends in the gcc build's state."""
        self.assertEqual(self.same_state(IDLE_HASH, "2551"), 17)

    @unittest.skipUnless(EXE.exists(), "build/port-gcc/ttt not built (TTT_CC=gcc port/tools/build.py)")
    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_fight_same_state(self):
        """The scripted fight (tests/test_port_input.py: to round 1, then forward, punches and a kick; 1451 VBlanks)
        ends in the gcc build's state."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
            self.same_state(FIGHT_HASH, "1451", TTT_INPUT_PLAY=str(p))


if __name__ == "__main__":
    unittest.main()
