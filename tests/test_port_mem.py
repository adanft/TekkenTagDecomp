"""The PC build's memory map (port/src/plat_mem.c), checked on the built program build/port/ttt (port/tools/build.py;
the game's data is loaded from your disc at start, plat_data.c). Skipped when the program has not been built."""

import os
import re
import subprocess
import unittest
from pathlib import Path

from test_port_platform import EXE, ROOT


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class MemoryMapTest(unittest.TestCase):
    def run_ttt(self, **env):
        return subprocess.run([str(EXE)], env=dict(os.environ, **env), capture_output=True, text=True, timeout=60)

    def test_self_test(self):
        """EE RAM and its uncached aliases, the game image, scratchpad and hardware registers (plat_mem.c)."""
        r = self.run_ttt(TTT_SELFTEST="mem")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: memory self-test OK", r.stdout)

    def test_data_names_at_ps2_addresses(self):
        """Every D_XXXXXXXX data name the program uses is its PS2 address (an absolute symbol: the data itself is not
        linked in, plat_data.c copies it there from the disc)."""
        out = subprocess.run(["nm", str(EXE)], capture_output=True, text=True, check=True).stdout
        syms = {}
        for l in out.splitlines():
            m = re.match(r"([0-9a-f]{8}|[0-9a-f]{16}) ([aAdDbB]) (\S+)$", l)  # (ELF64: 16 digits)
            if m:
                syms[m.group(3)] = int(m.group(1), 16)
        labels = [(s, a) for s, a in syms.items() if re.fullmatch(r"D_[0-9A-F]{8}", s)]
        self.assertGreater(len(labels), 1000)
        wrong = [s for s, a in labels if a != int(s[2:], 16)]
        self.assertEqual(wrong, [])
        self.assertEqual(syms.get("__ttt_data_start"), 0x415700)

    def test_no_section_at_ps2_addresses(self):
        """No section of the program lies in EE RAM: nothing of the game is linked in."""
        out = subprocess.run(["readelf", "-SW", str(EXE)], capture_output=True, text=True, check=True).stdout
        for m in re.finditer(r"\]\s+(\S+)\s+\S+\s+([0-9a-f]{8}) [0-9a-f]+ ([0-9a-f]+)", out):
            addr, size = int(m.group(2), 16), int(m.group(3), 16)
            self.assertFalse(addr != 0 and addr < 0x02000000 and size != 0, m.group(1))

    @unittest.skipUnless((ROOT / "iso/TEKKEN.BIN").exists() or os.environ.get("TTT_DISC"), "no disc data")
    def test_boot_runs_on_the_map_without_a_fault(self):
        """The game starts on the mapped memory and runs its first frames headless (tests/test_port_platform.py
        checks how far)."""
        r = self.run_ttt(TTT_FRAMES="60", TTT_SEED="1")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: headless: 60 vblanks", r.stdout)


if __name__ == "__main__":
    unittest.main()
