"""Tests for scripts/irx.py (synthetic IRX data only)."""

import importlib.util
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location("irx", ROOT / "scripts" / "irx.py")
irx = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(irx)

JR_RA = 0x03E00008


def words(*ws):
    return b"".join(struct.pack("<I", w) for w in ws)


def import_table(name, version, ordinals):
    out = words(0x41E00000, 0) + struct.pack("<HH", version, 0) + name.encode().ljust(8, b"\0")
    for n in ordinals:
        out += words(JR_RA, 0x24000000 | n)
    return out + words(0, 0)


def export_table(name, version, funcs):
    out = words(0x41C00000, 0) + struct.pack("<HH", version, 0) + name.encode().ljust(8, b"\0")
    return out + words(*funcs) + words(0)


def iopmod(entry, gp, text, data, bss, version, name):
    return words(0xFFFFFFFF, entry, gp, text, data, bss) + struct.pack("<H", version) + name.encode() + b"\0"


def elf(sections):
    """Minimal ELF32 little-endian relocatable file: [(name, type, addr, bytes)]."""
    names = b"\0"
    offs = {}
    for name, *_ in sections + [(".shstrtab",)]:
        offs[name] = len(names)
        names += name.encode() + b"\0"
    body = b""
    headers = [b"\0" * 40]
    pos = 52
    for name, stype, addr, blob in sections + [(".shstrtab", 3, 0, names)]:
        headers.append(struct.pack("<10I", offs[name], stype, 0, addr, pos + len(body), len(blob), 0, 0, 4, 0))
        body += blob
    shoff = 52 + len(body)
    ehdr = b"\x7fELF\x01\x01\x01" + b"\0" * 9 + struct.pack(
        "<HHIIIIIHHHHHH", 0xFF80, 8, 1, 0, 0, shoff, 0, 52, 32, 0, 40, len(headers), len(headers) - 1)
    return ehdr + body + b"".join(headers)


class ElfTest(unittest.TestCase):
    def test_sections_and_iopmod(self):
        text = words(JR_RA, 0)
        data = elf([(".iopmod", 0x70000080, 0, iopmod(0x10, 0x8000, 8, 0, 0x20, 0x0102, "TestDrv")),
                    (".text", 1, 0, text)])
        secs = irx.sections(data)
        self.assertEqual(secs[".text"].addr, 0)
        self.assertEqual(irx.section_bytes(data, secs[".text"]), text)
        mod = irx.iopmod(irx.section_bytes(data, secs[".iopmod"]))
        self.assertEqual((mod.name, mod.version, mod.entry, mod.gp), ("TestDrv", 0x0102, 0x10, 0x8000))
        self.assertEqual((mod.text_size, mod.data_size, mod.bss_size), (8, 0, 0x20))

    def test_not_elf(self):
        with self.assertRaises(ValueError):
            irx.sections(b"not an elf file at all, padded" * 4)


class TableTest(unittest.TestCase):
    def test_imports(self):
        blob = words(0, 0) + import_table("cdvdman", 0x101, [10, 6]) + import_table("stdio", 0x102, [4])
        libs = irx.imports(blob, base=0x100)
        self.assertEqual([(l.name, l.version) for l in libs], [("cdvdman", 0x101), ("stdio", 0x102)])
        stubs = libs[0].stubs
        self.assertEqual(stubs, [(0x100 + 8 + 20, 10), (0x100 + 8 + 28, 6)])
        self.assertEqual(libs[1].stubs[0][1], 4)

    def test_import_names(self):
        self.assertEqual(irx.import_name("cdvdman", 10), "sceCdSearchFile")
        self.assertEqual(irx.import_name("sifcmd", 17), "sceSifRegisterRpc")
        self.assertEqual(irx.import_name("nosuchlib", 4), "nosuchlib_4")
        self.assertEqual(irx.import_name("cdvdman", 999), "cdvdman_999")

    def test_stub_map(self):
        libs = irx.imports(import_table("thbase", 0x101, [4, 6]), base=0)
        self.assertEqual(irx.stub_map(libs), {20: "thbase:CreateThread", 28: "thbase:StartThread"})

    def test_exports(self):
        blob = words(7) + export_table("mydrv", 0x100, [0x40, 0x80, 0x90]) + words(0x41C00000)
        exps = irx.exports(blob, base=0)
        self.assertEqual(len(exps), 1)
        self.assertEqual((exps[0].name, exps[0].version, exps[0].funcs), ("mydrv", 0x100, [0x40, 0x80, 0x90]))

    def test_truncated_table_is_ignored(self):
        self.assertEqual(irx.imports(words(0x41E00000, 0)), [])
        self.assertEqual(irx.exports(words(0x41C00000)), [])


class AnnotateTest(unittest.TestCase):
    def test_calls_and_string_refs(self):
        lines = [
            "     100:\t3c040001 \tlui\ta0,0x1",
            "     104:\t0c000014 \tjal\t0x50",
            "     108:\t24849c20 \taddiu\ta0,a0,-25568",
            "     10c:\t3c020001 \tlui\tv0,0x1",
            "     110:\t8c43a090 \tlw\tv1,-24432(v0)",
        ]
        strings = {0x9C20: "\\TEKKEN.BIN;1"}
        out = irx.annotate(lines, {0x50: "cdvdman:sceCdSearchFile"}, strings)
        self.assertIn("# cdvdman:sceCdSearchFile", out[1])
        self.assertIn('# 0x9c20 "\\TEKKEN.BIN;1"', out[2])
        self.assertIn("# 0xa090", out[4])
        self.assertEqual(out[0], lines[0])

    def test_register_tracking(self):
        lines = [
            "     200:\t3c040001 \tlui\ta0,0x1",
            "     204:\t2484af40 \taddiu\ta0,a0,-20672",
            "     208:\tac800004 \tsw\tzero,4(a0)",
            "     20c:\t3c100001 \tlui\ts0,0x1",
            "     210:\t00808021 \taddu\ts0,a0,zero",
            "     214:\t8e060000 \tlw\ta2,8(s0)",
            "     218:\t00008021 \taddu\ts0,zero,zero",
            "     21c:\t8e060000 \tlw\ta2,0(s0)",
            "     220:\t3c058000 \tlui\ta1,0x8000",
            "     224:\t34a50601 \tori\ta1,a1,0x601",
            "     228:\t3c050001 \tlui\ta1,0x1",
            "     22c:\t0c000014 \tjal\t0x50",
            "     230:\t00000000 \tsll\tzero,zero,0x0",
            "     234:\t8ca30000 \tlw\tv1,0(a1)",
        ]
        out = irx.annotate(lines, {}, {0xAF40: "a\nb"})
        self.assertIn('# 0xaf40 "a\\nb"', out[1])
        self.assertIn("# 0xaf44", out[2])
        self.assertIn("# 0xaf48", out[5])
        self.assertEqual(out[7], lines[7])
        self.assertIn("# 0x80000601", out[9])
        self.assertEqual(out[13], lines[13])  # a1 is clobbered by the call

    def test_branch_target_resets_tracking(self):
        lines = [
            "     300:\t3c100001 \tlui\ts0,0x1",
            "     304:\t10400002 \tbeq\tv0,zero,0x30c",
            "     308:\t8e060000 \tlw\ta2,4(s0)",
            "     30c:\t8e060000 \tlw\ta2,4(s0)",
        ]
        out = irx.annotate(lines, {}, {})
        self.assertIn("# 0x10004", out[2])
        self.assertEqual(out[3], lines[3])

    def test_strings(self):
        blob = b"\0\0abc\0hello world\0\x01\x02xy\0"
        self.assertEqual(irx.find_strings(blob, base=0x10, minlen=3), {0x12: "abc", 0x16: "hello world"})


if __name__ == "__main__":
    unittest.main()
