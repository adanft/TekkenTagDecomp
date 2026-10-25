"""Crash reports (port/src/plat_crash.c, T41, docs/port/README.md "Crash reports"), checked on the built program
build/port/ttt: TTT_CRASH_TEST makes the chosen fault at a VBlank, and the report file must be there with its fields.
Core dumps are switched off for these runs (RLIMIT_CORE 0) and they run in a temporary directory: no core file and no
report lands in the repository. Skipped without the program or without disc data."""

import os
import re
import resource
import signal
import subprocess
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, disc

SYMS = EXE.with_name(EXE.name + ".syms")


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def crash_run(where, kind, at="30"):
    """A headless run that faults at VBlank `at`, with its settings file (and so its user directory) in `where`."""
    e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
    e.update(TTT_DISC=str(disc()), TTT_MOVIES="0", TTT_FRAMES="100", TTT_SEED="1", TTT_SAVES=str(Path(where) / "saves"),
             TTT_CONFIG=str(Path(where) / "settings.ini"), TTT_TEXTURES=str(Path(where) / "textures"),
             TTT_CRASH_TEST=f"{kind}@{at}")
    return subprocess.run([str(EXE)], env=e, cwd=where, capture_output=True, text=True, timeout=120,
                          preexec_fn=no_core)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class CrashReport(unittest.TestCase):
    KINDS = {"segv": signal.SIGSEGV, "stack": signal.SIGSEGV, "ill": signal.SIGILL, "fpe": signal.SIGFPE,
             "bus": signal.SIGBUS, "abrt": signal.SIGABRT}

    def report(self, kind, at="30"):
        with tempfile.TemporaryDirectory(prefix="ttt-crash-") as d:
            r = crash_run(d, kind, at)
            sig = self.KINDS[kind]
            self.assertEqual(r.returncode, -sig, r.stdout[-1500:] + r.stderr[-1500:])
            m = re.search(r"ttt: crash: (\w+); report written to (\S+)\n", r.stderr)
            self.assertIsNotNone(m, r.stderr[-1500:])
            self.assertEqual(m.group(1), sig.name)
            path = Path(m.group(2))
            self.assertEqual(path.parent, Path(d) / "crash")
            self.assertRegex(path.name, r"^ttt-crash-\d{8}-\d{6}-\d+\.txt$")
            self.assertEqual(sorted(p.name for p in Path(d).iterdir() if p.name.startswith("core")), [])
            return path.read_text()

    def fields(self, text):
        return dict(l.split(": ", 1) for l in text.splitlines()[1:] if ": " in l and not l.startswith(" "))

    def check_common(self, text, sig):
        f = self.fields(text)
        self.assertTrue(text.startswith("Tekken Tag Tournament PC port: crash report\n"))
        self.assertTrue(f["signal"].startswith(f"{sig.name} ({int(sig)})"), f["signal"])
        self.assertRegex(f["registers"], r"^eip 0x[0-9a-f]{8} esp 0x[0-9a-f]{8} ")
        self.assertEqual(f["last vblank"], "30")
        self.assertIn("thread", f)
        self.assertRegex(f["build"], r"^[0-9a-f]{7,}(-dirty)?$|^unknown$")
        self.assertTrue(f["renderer"].startswith("no window"), f["renderer"])
        self.assertIn("video.renderer=gl", f["settings"])
        self.assertIn("game.movies=0 (env)", f["settings"])
        self.assertEqual(f["game data"], "USA release (verified)")
        self.assertLess(len(text), 8192)  # a few words of the machine, nothing of the game's memory
        return f

    def test_segv(self):
        """A write to a page with no access: the fault address, the function it happened in (from build/port/ttt.syms)
        and, through the EE stack pointer Port_OffEe left, the game's functions with their PS2 addresses."""
        text = self.report("segv")
        f = self.check_common(text, signal.SIGSEGV)
        self.assertRegex(f["fault address"], r"^0x[0-9a-f]{8}$")
        self.assertTrue(f["function"].startswith("Port_CrashVBlank+0x"), f["function"])
        self.assertIn("stack code address: ", text)
        self.assertRegex(text, r"EE stack code address: 0x[0-9a-f]{8} \w+\+0x[0-9a-f]{8} \(EE 0x00[0-9a-f]{6}\)")

    def test_bad_stack_pointer(self):
        """A fault with the stack pointer on an unmapped page: the handler runs on its own stack (sigaltstack)."""
        f = self.check_common(self.report("stack"), signal.SIGSEGV)
        self.assertTrue(f["function"].startswith("Port_CrashVBlank+0x"), f["function"])

    def test_other_signals(self):
        """SIGILL, SIGFPE, SIGBUS and SIGABRT: each written, and the program ends by the same signal."""
        for kind in ("ill", "fpe", "bus", "abrt"):
            with self.subTest(kind=kind):
                f = self.check_common(self.report(kind), self.KINDS[kind])
                if kind in ("bus", "abrt"):  # raised, not a fault: no address
                    self.assertTrue(f["fault address"].startswith("(none"), f["fault address"])
                else:
                    self.assertTrue(f["function"].startswith("Port_CrashVBlank+0x"), f["function"])


@unittest.skipUnless(SYMS.exists(), "build/port/ttt.syms not built (port/tools/build.py)")
class SymbolTable(unittest.TestCase):
    def test_names_and_addresses_only(self):
        """ttt.syms: "<host address> <PS2 address> <name>" lines in address order, the game's functions with the PS2
        address their name gives."""
        lines = SYMS.read_text().splitlines()
        self.assertGreater(len(lines), 1000)
        addrs = []
        for l in lines:
            self.assertRegex(l, r"^[0-9a-f]{8} [0-9a-f]{8} \S+$")
            addrs.append(int(l[:8], 16))
        self.assertEqual(addrs, sorted(addrs))
        m = [l for l in lines if l.endswith(" func_0035D790")]
        self.assertEqual(len(m), 1)
        self.assertEqual(m[0].split()[1], "0035d790")


if __name__ == "__main__":
    unittest.main()
