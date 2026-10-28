"""The Windows x86-64 program (TTT_CC=win64, T42 b7, docs/port/windows.md): the clang64 program cross-built for Windows
with clang (--target=x86_64-w64-mingw32) and mingw-w64's binutils, headers, libraries and linker:
build/port-win64/ttt.exe.

Static checks of the linked program (no Windows needed): a PE image at 0x08000000 with neither address randomisation nor
relocations, the game's data names absolute in EE RAM, the imports expected (the system's DLLs only: winpthreads and
libgcc linked in), the crash report's symbol table, every object the Linux build has, and no host float instruction in
the game's or the port's functions (the float rule of tests/test_port_float.py on the PE). With TTT_SLOW_TESTS=1 the
program is built first (a few minutes); otherwise an existing build is checked. When `wine` is on PATH the program is
also run: the self-tests and the 2551-VBlank idle run's reference hash. Skipped without mingw-w64 / clang / llc."""

import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from test_port_data import IDLE_HASH
from test_port_platform import ROOT, disc

MINGW = "x86_64-w64-mingw32"
WIN = ROOT / "build/port-win64"
WIN_EXE = WIN / "ttt.exe"
TOOLS = ROOT / "port/tools"
HAVE = all(shutil.which(t) for t in (f"{MINGW}-gcc", f"{MINGW}-nm", f"{MINGW}-objdump", "clang", "llc"))
WINE = shutil.which("wine")
# What the program may import: the system's own DLLs and the Universal C Runtime (api-ms-win-crt-*, mingw-w64's
# default C library); SDL3.dll is loaded at run time (plat_window.c), not imported.
ALLOWED_DLLS = re.compile(r"^(kernel32|user32|shell32|ws2_32|bcrypt|msvcrt|api-ms-win-crt-[a-z]+-l1-1-0)\.dll$", re.I)
NEEDED_DLLS = {"kernel32.dll", "ws2_32.dll", "bcrypt.dll", "shell32.dll"}
# The scans of tests/test_port_float.py: x87 instructions that compute or move floats, scalar / packed SSE float ones.
X87 = re.compile(r"\t(fld|fst|fadd|fsub|fmul|fdiv|fsqrt|fsin|fcos|fsincos|fpatan|fptan|fild|fist|fcom|fucom|fyl2x"
                 r"|f2xm1|fscale|fprem|frndint|fchs|fabs)\w*\b")
SSE = re.compile(r"\t\w+(ss|sd|ps|pd)\s.*%xmm")
FUNC = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")
WINE_SELFTESTS = ("mem", "hw", "ee", "gs", "float", "vu1", "vu0", "state")


def pe_headers(path):
    """{image_base, dll_characteristics, subsystem, reloc_size, sections: {name: (rva, size)}} of a PE32+ file."""
    b = Path(path).read_bytes()
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    assert b[pe:pe + 4] == b"PE\0\0"
    nsec, = struct.unpack_from("<H", b, pe + 6)
    optsz, = struct.unpack_from("<H", b, pe + 20)
    opt = pe + 24
    assert struct.unpack_from("<H", b, opt)[0] == 0x20B  # PE32+
    secs = {}
    for i in range(nsec):
        s = opt + optsz + i * 40
        size, rva = struct.unpack_from("<II", b, s + 8)  # VirtualSize, VirtualAddress
        secs[b[s:s + 8].rstrip(b"\0").decode()] = (rva, size)
    return {"image_base": struct.unpack_from("<Q", b, opt + 24)[0],
            "subsystem": struct.unpack_from("<H", b, opt + 68)[0],
            "dll_characteristics": struct.unpack_from("<H", b, opt + 70)[0],
            "reloc_size": struct.unpack_from("<I", b, opt + 112 + 5 * 8 + 4)[0],  # data directory 5: base relocations
            "sections": secs}


def imported_dlls(path):
    out = subprocess.run([f"{MINGW}-objdump", "-p", str(path)], capture_output=True, text=True, check=True).stdout
    return sorted({m.lower() for m in re.findall(r"DLL Name: (\S+)", out)})


def defined(objs, kinds="tTW"):
    """Names the objects define (functions by default), as tests/test_port_float.py's functions()."""
    out = set()
    for o in objs:
        syms = subprocess.run([f"{MINGW}-nm", "--defined-only", str(o)], capture_output=True, text=True,
                              check=True).stdout
        out |= {l.split()[2].split(".")[0] for l in syms.splitlines() if len(l.split()) == 3 and l.split()[1] in kinds}
    return out


@unittest.skipUnless(HAVE, "no mingw-w64 cross tools / clang / llc")
class Win64Helpers(unittest.TestCase):
    """The variant's settings, read by a fresh interpreter with TTT_CC=win64 (no build)."""

    def test_variant(self):
        code = ("import toolchain as t, link as l\n"
                "print(t.EXE.relative_to(t.ROOT), t.BITS64, t.WIN64, t.target(), t.tool('nm'), t.TEXT64)\n"
                "print(' '.join(t.port_cflags()))\n"
                "lay = {'ranges': [(0x415700, 16)], 'words': [], 'zeros': [0x415704], 'payload': (16, '00' * 20),"
                " 'elf': ('SLUS_200.01', 1, '00' * 20)}\n"
                "print('.section .rdata,\\\\\"dr\\\\\"' in l.data_table(lay))\n"
                "print(l.image_script(dict(lay, bss=(0x1000000, 0x10))).splitlines()[-1])\n")
        r = subprocess.run([sys.executable, "-c", code], cwd=TOOLS, env=dict(os.environ, TTT_CC="win64"),
                           capture_output=True, text=True, check=True)
        first, flags, table, script = r.stdout.splitlines()
        self.assertEqual(first, f"build/port-win64/ttt.exe True True ['--target={MINGW}'] {MINGW}-nm 134217728")
        for f in (f"--target={MINGW}", "-m64", "-msoft-float", "-mno-x87", "-fms-extensions", "-D__CRT__NO_INLINE",
                  "-D_FILE_OFFSET_BITS=64"):
            self.assertIn(f, flags.split())
        self.assertNotIn("-malign-double", flags.split())
        self.assertEqual(table, "True")  # the rewrite table in COFF's read-only data section
        self.assertEqual(script, "__executable_start = ADDR(.text);")


@unittest.skipUnless(shutil.which(f"{MINGW}-gcc"), "no mingw-w64 cross compiler")
class Win64Server(unittest.TestCase):
    def test_server_cross_build(self):
        """The matchmaking server and the manifest signer (port/server, docs/port/matchmaking.md) cross-build for
        Windows with -Werror, importing only the system's DLLs (Winsock, the random generator, the C runtime)."""
        with tempfile.TemporaryDirectory() as d:
            r = subprocess.run([sys.executable, str(TOOLS / "build_server.py"), "--win64", "--out", d],
                               capture_output=True, text=True, timeout=300)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            for name in ("ttt-server.exe", "ttt-sign.exe"):
                dlls = imported_dlls(Path(d) / name)
                self.assertEqual([x for x in dlls if not ALLOWED_DLLS.match(x)], [], name)
                self.assertIn("bcrypt.dll", dlls)
            self.assertIn("ws2_32.dll", imported_dlls(Path(d) / "ttt-server.exe"))


@unittest.skipUnless(HAVE, "no mingw-w64 cross tools / clang / llc")
class Win64Build(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get("TTT_SLOW_TESTS") == "1":
            r = subprocess.run([sys.executable, str(TOOLS / "build.py")], cwd=ROOT,
                               env=dict(os.environ, TTT_CC="win64"), capture_output=True, text=True, timeout=3600)
            cls.build_out = r.stdout + r.stderr
            if r.returncode != 0:
                raise AssertionError("win64 build failed:\n" + cls.build_out[-3000:])
        if not WIN_EXE.exists():
            raise unittest.SkipTest("build/port-win64/ttt.exe not built (TTT_CC=win64 port/tools/build.py, or "
                                    "TTT_SLOW_TESTS=1)")

    def test_every_object(self):
        """Every object of the program compiled (the Linux build's list, when there is one: the same files)."""
        objs = [Path(l) for l in (WIN / "objects.txt").read_text().splitlines() if l]
        self.assertGreater(len(objs), 130)
        linux = ROOT / "build/port/objects.txt"
        if linux.exists():
            self.assertEqual(sorted(Path(l).name for l in linux.read_text().splitlines() if l),
                             sorted(o.name for o in objs))
        stubs = (WIN / "stubs.txt").read_text().split()
        if (ROOT / "build/port/stubs.txt").exists():
            self.assertEqual(stubs, (ROOT / "build/port/stubs.txt").read_text().split())

    def test_image(self):
        """A 64-bit console program at 0x08000000: no dynamic base (0x40), no high-entropy addresses (0x20), no
        relocations; the code first, at 0x08001000, and the program's end below 0x10000000 (the PS2's registers)."""
        h = pe_headers(WIN_EXE)
        self.assertEqual(h["image_base"], 0x08000000)
        self.assertEqual(h["dll_characteristics"] & 0x60, 0)
        self.assertEqual(h["reloc_size"], 0)
        self.assertNotIn(".reloc", h["sections"])
        self.assertEqual(h["subsystem"], 3)
        self.assertEqual(h["sections"][".text"][0], 0x1000)
        end = max(rva + size for rva, size in h["sections"].values())
        self.assertLess(0x08000000 + end, 0x10000000)
        bss = h["sections"][".bss"]
        self.assertLess(0x08000000 + bss[0] + bss[1], 0x09000000)  # inside the state hash's window (plat_state.c)

    def test_imports(self):
        dlls = imported_dlls(WIN_EXE)
        self.assertEqual([d for d in dlls if not ALLOWED_DLLS.match(d)], [])
        self.assertEqual(sorted(NEEDED_DLLS - set(dlls)), [])

    def test_symbols(self):
        """The game's data names are absolute addresses in EE RAM, __executable_start is the code's start, and link.py
        wrote the crash report's table (ttt.exe.syms) with the game's functions."""
        nm = subprocess.run([f"{MINGW}-nm", str(WIN_EXE)], capture_output=True, text=True, check=True).stdout
        syms = {f[2]: (int(f[0], 16), f[1]) for f in map(str.split, nm.splitlines()) if len(f) == 3}
        self.assertEqual(syms["__ttt_data_start"][0], 0x00415700)
        self.assertEqual(syms["__ttt_data_start"][1], "A")
        self.assertEqual(syms["__executable_start"][0], 0x08001000)
        for name in ("__wrap_main", "Port_StateBoundary", "Port_OffEe", "PlatOs_MemPrepare", "PlatOs_StackWide"):
            self.assertIn(name, syms)
            self.assertTrue(0x08001000 <= syms[name][0] < syms["etext"][0], name)
        table = WIN_EXE.with_name("ttt.exe.syms").read_text().splitlines()
        self.assertGreater(len(table), 5000)
        self.assertTrue(any(l.split()[1] == "00340000" for l in table))

    def test_no_host_float_code(self):
        """tests/test_port_float.py's rule on the PE: no x87 and no SSE float instruction in a function the game's or
        the port's objects define, except the SIMD files and the overlay / Dear ImGui (whose functions must use host
        float). The C library and mingw-w64's run time linked in are not ours and not scanned."""
        obj = WIN / "obj"
        sys.path.insert(0, str(TOOLS))
        try:
            import toolchain
            simd = defined([obj / f"pc_{Path(n).stem}.o" for n in toolchain.SIMD_FILES])
            host = toolchain.host_float_objects(obj)
        finally:
            sys.path.remove(str(TOOLS))
        overlay = defined(host)
        self.assertIn("Overlay_Render", overlay)
        ours = defined([o for o in obj.glob("*.o") if o not in host] + [WIN / "data_table.o", WIN / "stubs.o"])
        self.assertIn("run_fast", simd)
        self.assertIn("func_00340000", ours)
        out = subprocess.run([f"{MINGW}-objdump", "-d", "--no-show-raw-insn", "-j", ".text", str(WIN_EXE)],
                             capture_output=True, text=True, check=True).stdout
        bad, fn, seen, scanned = [], None, 0, set()
        for line in out.splitlines():
            m = FUNC.match(line)
            if m:
                fn = m.group(1).split(".")[0]
                continue
            if fn in overlay:
                seen += bool(X87.search(line) or SSE.search(line))
            elif fn in ours:
                scanned.add(fn)
                if X87.search(line) or (SSE.search(line) and fn not in simd):
                    bad.append(f"{fn}: {line.strip()}")
        self.assertEqual(bad[:10], [])
        self.assertGreater(seen, 0)
        self.assertGreater(len(scanned), 3000)

    @unittest.skipUnless(WINE, "wine is not installed")
    def test_wine_selftests(self):
        with tempfile.TemporaryDirectory() as d:
            for name in WINE_SELFTESTS:
                with self.subTest(name=name):
                    r = wine_run(os.path.join(d, "saves"), timeout=900, TTT_SELFTEST=name, TTT_FRAMES="1")
                    self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                    self.assertRegex(r.stdout, rf"ttt: {name if name != 'mem' else 'memory'} self-test OK")

    @unittest.skipUnless(WINE, "wine is not installed")
    @unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
    def test_wine_idle_hash(self):
        """The 2551-VBlank idle run under Wine ends in the Linux builds' state (the reference hash), 17 pictures."""
        with tempfile.TemporaryDirectory() as d:
            dump = os.path.join(d, "dump")
            os.makedirs(dump)
            r = wine_run(os.path.join(d, "saves"), timeout=3600, TTT_FRAMES="2551", TTT_SEED="1", TTT_DUMP=dump,
                         TTT_DUMP_EVERY="150")
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            state = re.search(r"ttt: state: .*", r.stdout)
            self.assertIsNotNone(state, r.stdout[-2000:])
            self.assertIn(f"hash {IDLE_HASH} ", state.group(0))
            self.assertEqual(len(os.listdir(dump)), 17)


def wine_run(saves, timeout, **env):
    """The Windows program under Wine, as test_port_clang.run runs a Linux one: only the given TTT_ variables, the
    memory card and settings in `saves`, no movies; Wine's own messages off."""
    shutil.rmtree(saves, ignore_errors=True)
    os.makedirs(saves)
    e = {k: v for k, v in os.environ.items() if not k.startswith("TTT_")}
    e.setdefault("WINEDEBUG", "-all")
    if disc() is not None:
        e["TTT_DISC"] = str(disc())
    e.update(TTT_MOVIES="0", TTT_SAVES=saves, TTT_CONFIG=os.path.join(saves, "settings.ini"),
             TTT_TEXTURES=os.path.join(saves, "textures"), **env)
    return subprocess.run([WINE, str(WIN_EXE)], env=e, capture_output=True, text=True, timeout=timeout)


if __name__ == "__main__":
    unittest.main()
