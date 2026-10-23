"""The PC build's memory map (port/src/plat_mem.c), checked on the built program: build/port/ttt (made from your own
SLUS_200.01 by port/tools/build.py). Skipped when the program has not been built."""

import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "build/port/ttt"
CHUNKS = ROOT / "build/port/gen/chunks.txt"


@unittest.skipUnless(EXE.exists() and CHUNKS.exists(), "build/port/ttt not built (port/tools/build.py)")
class MemoryMapTest(unittest.TestCase):
    def run_ttt(self, **env):
        return subprocess.run([str(EXE)], env=dict(os.environ, **env), capture_output=True, text=True, timeout=60)

    def test_self_test(self):
        """EE RAM and its uncached aliases, the game image, scratchpad and hardware registers (plat_mem.c)."""
        r = self.run_ttt(TTT_SELFTEST="mem")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: memory self-test OK", r.stdout)

    def test_data_at_ps2_addresses(self):
        """Every split data chunk and every D_XXXXXXXX data label sits at its PS2 address."""
        out = subprocess.run(["nm", str(EXE)], capture_output=True, text=True, check=True).stdout
        syms = {}
        for l in out.splitlines():
            m = re.match(r"([0-9a-f]{8}) ([dDbB]) (\S+)$", l)
            if m:
                syms[m.group(3)] = int(m.group(1), 16)
        for l in CHUNKS.read_text().splitlines():
            label, addr, _ = l.split()
            self.assertEqual(syms.get(label), int(addr, 16), label)
        labels = [(s, a) for s, a in syms.items() if re.fullmatch(r"D_[0-9A-F]{8}", s)]
        self.assertGreater(len(labels), 1000)
        wrong = [s for s, a in labels if a != int(s[2:], 16)]
        self.assertEqual(wrong, [])

    def test_boot_stops_at_a_stub_not_a_fault(self):
        """The game starts on the mapped memory and runs until the first function the port does not provide."""
        r = self.run_ttt()
        self.assertEqual(r.returncode, 3, r.stdout + r.stderr)
        self.assertIn("ttt: not implemented yet: ", r.stdout)


if __name__ == "__main__":
    unittest.main()
