"""Tests for scripts/vuasm.py, the VU microcode assembler and disassembler (synthetic words only, no disc needed).

Run: .venv/bin/python -m unittest discover -s tests -v
"""

import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location("vuasm", ROOT / "scripts" / "vuasm.py")
vuasm = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(vuasm)

XYZW = 0xF << 21
XYZ = 0xE << 21


def upper(dest=0, ft=0, fs=0, fd=0, op=0):
    return dest | ft << 16 | fs << 11 | fd << 6 | op


def special(code, dest=0, ft=0, fs=0):
    """Upper special form: 11-bit opcode split over bits 10..6 and 1..0, bits 5..2 = 0b1111."""
    return dest | ft << 16 | fs << 11 | (code >> 2) << 6 | 0x3C | (code & 3)


def lower(op, dest=0, it=0, is_=0, imm=0):
    return op << 25 | dest | it << 16 | is_ << 11 | imm


def lspecial(code, dest=0, it=0, is_=0, fsf=0, ftf=0):
    return 0x40 << 25 | ftf << 23 | fsf << 21 | dest | it << 16 | is_ << 11 | (code >> 2) << 6 | 0x3C | (code & 3)


# (text, word): every upper form, with the word built field by field from the VU instruction formats.
UPPER = [
    ("nop", 0x2FF),
    ("addx.xyzw vf3, vf1, vf2x", upper(XYZW, 2, 1, 3, 0x00)),
    ("subw.xyz vf3, vf1, vf2w", upper(XYZ, 2, 1, 3, 0x07)),
    ("maddy.xyzw vf4, vf5, vf6y", upper(XYZW, 6, 5, 4, 0x09)),
    ("msubz.w vf4, vf5, vf6z", upper(1 << 21, 6, 5, 4, 0x0E)),
    ("maxx.xy vf1, vf2, vf0x", upper(0xC << 21, 0, 2, 1, 0x10)),
    ("miniw.xyzw vf7, vf8, vf9w", upper(XYZW, 9, 8, 7, 0x17)),
    ("mulx.xyzw vf10, vf11, vf12x", upper(XYZW, 12, 11, 10, 0x18)),
    ("mulq.xyzw vf1, vf2, q", upper(XYZW, 0, 2, 1, 0x1C)),
    ("maxi.xyzw vf1, vf2, i", upper(XYZW, 0, 2, 1, 0x1D)),
    ("muli.xyz vf1, vf2, i", upper(XYZ, 0, 2, 1, 0x1E)),
    ("minii.x vf1, vf2, i", upper(8 << 21, 0, 2, 1, 0x1F)),
    ("addq.xyzw vf1, vf2, q", upper(XYZW, 0, 2, 1, 0x20)),
    ("maddq.xyzw vf1, vf2, q", upper(XYZW, 0, 2, 1, 0x21)),
    ("addi.xyzw vf1, vf2, i", upper(XYZW, 0, 2, 1, 0x22)),
    ("maddi.xyzw vf1, vf2, i", upper(XYZW, 0, 2, 1, 0x23)),
    ("subq.xyzw vf1, vf2, q", upper(XYZW, 0, 2, 1, 0x24)),
    ("msubq.xyzw vf1, vf2, q", upper(XYZW, 0, 2, 1, 0x25)),
    ("subi.xyzw vf1, vf2, i", upper(XYZW, 0, 2, 1, 0x26)),
    ("msubi.xyzw vf1, vf2, i", upper(XYZW, 0, 2, 1, 0x27)),
    ("add.xyzw vf3, vf1, vf2", upper(XYZW, 2, 1, 3, 0x28)),
    ("madd.xyzw vf3, vf1, vf2", upper(XYZW, 2, 1, 3, 0x29)),
    ("mul.xyz vf3, vf1, vf2", upper(XYZ, 2, 1, 3, 0x2A)),
    ("max.xyzw vf3, vf1, vf2", upper(XYZW, 2, 1, 3, 0x2B)),
    ("sub.xyzw vf3, vf1, vf2", upper(XYZW, 2, 1, 3, 0x2C)),
    ("msub.xyzw vf3, vf1, vf2", upper(XYZW, 2, 1, 3, 0x2D)),
    ("opmsub.xyz vf3, vf1, vf2", upper(XYZ, 2, 1, 3, 0x2E)),
    ("mini.xyzw vf3, vf1, vf2", upper(XYZW, 2, 1, 3, 0x2F)),
    ("addax.xyzw acc, vf1, vf2x", special(0x00, XYZW, 2, 1)),
    ("subay.xyzw acc, vf1, vf2y", special(0x05, XYZW, 2, 1)),
    ("maddaz.xyzw acc, vf1, vf2z", special(0x0A, XYZW, 2, 1)),
    ("msubaw.xyzw acc, vf1, vf2w", special(0x0F, XYZW, 2, 1)),
    ("itof0.xyzw vf1, vf2", special(0x10, XYZW, 1, 2)),
    ("itof4.xyzw vf1, vf2", special(0x11, XYZW, 1, 2)),
    ("itof12.xy vf1, vf2", special(0x12, 0xC << 21, 1, 2)),
    ("itof15.xyzw vf1, vf2", special(0x13, XYZW, 1, 2)),
    ("ftoi0.xyzw vf1, vf2", special(0x14, XYZW, 1, 2)),
    ("ftoi4.xyzw vf1, vf2", special(0x15, XYZW, 1, 2)),
    ("ftoi12.xyzw vf1, vf2", special(0x16, XYZW, 1, 2)),
    ("ftoi15.xyzw vf1, vf2", special(0x17, XYZW, 1, 2)),
    ("mulax.xyzw acc, vf1, vf2x", special(0x18, XYZW, 2, 1)),
    ("mulaw.xyzw acc, vf1, vf2w", special(0x1B, XYZW, 2, 1)),
    ("mulaq.xyzw acc, vf1, q", special(0x1C, XYZW, 0, 1)),
    ("abs.xyzw vf1, vf2", special(0x1D, XYZW, 1, 2)),
    ("mulai.xyzw acc, vf1, i", special(0x1E, XYZW, 0, 1)),
    ("clipw.xyz vf1, vf2w", special(0x1F, XYZ, 2, 1)),
    ("addaq.xyzw acc, vf1, q", special(0x20, XYZW, 0, 1)),
    ("maddaq.xyzw acc, vf1, q", special(0x21, XYZW, 0, 1)),
    ("addai.xyzw acc, vf1, i", special(0x22, XYZW, 0, 1)),
    ("maddai.xyzw acc, vf1, i", special(0x23, XYZW, 0, 1)),
    ("subaq.xyzw acc, vf1, q", special(0x24, XYZW, 0, 1)),
    ("msubaq.xyzw acc, vf1, q", special(0x25, XYZW, 0, 1)),
    ("subai.xyzw acc, vf1, i", special(0x26, XYZW, 0, 1)),
    ("msubai.xyzw acc, vf1, i", special(0x27, XYZW, 0, 1)),
    ("adda.xyzw acc, vf1, vf2", special(0x28, XYZW, 2, 1)),
    ("madda.xyzw acc, vf1, vf2", special(0x29, XYZW, 2, 1)),
    ("mula.xyzw acc, vf1, vf2", special(0x2A, XYZW, 2, 1)),
    ("suba.xyzw acc, vf1, vf2", special(0x2C, XYZW, 2, 1)),
    ("msuba.xyzw acc, vf1, vf2", special(0x2D, XYZW, 2, 1)),
    ("opmula.xyz acc, vf1, vf2", special(0x2E, XYZ, 2, 1)),
]

# (text, pc, word) for the lower forms that do not branch.
LOWER = [
    ("nop", 0x8000033C),
    ("lq.xyzw vf1, 2(vi3)", lower(0x00, XYZW, 1, 3, 2)),
    ("sq.xyz vf4, -1(vi5)", lower(0x01, XYZ, 5, 4, 0x7FF)),
    ("ilw.x vi1, 4(vi2)", lower(0x04, 8 << 21, 1, 2, 4)),
    ("isw.y vi1, -4(vi2)", lower(0x05, 4 << 21, 1, 2, 0x7FC)),
    ("iaddiu vi1, vi0, 0x3ff", lower(0x08, 0, 1, 0, 0x3FF)),
    ("isubiu vi1, vi1, 0x7fff", lower(0x09, 0xF << 21, 1, 1, 0x7FF)),
    ("fceq vi1, 0x123", lower(0x10, imm=0x123)),
    ("fcset 0xfff", lower(0x11, imm=0xFFF)),
    ("fcand vi1, 0xffffff", 0x12 << 25 | 0xFFFFFF),
    ("fcor vi1, 0x1", lower(0x13, imm=1)),
    ("fseq vi2, 0x800", lower(0x14, 1 << 21, 2)),
    ("fsset 0x10", lower(0x15, imm=0x10)),
    ("fsand vi2, 0x81", lower(0x16, 0, 2, 0, 0x81)),
    ("fsor vi2, 0x2", lower(0x17, 0, 2, 0, 2)),
    ("fmeq vi3, vi4", lower(0x18, 0, 3, 4)),
    ("fmand vi3, vi4", lower(0x1A, 0, 3, 4)),
    ("fmor vi3, vi4", lower(0x1B, 0, 3, 4)),
    ("fcget vi5", lower(0x1C, 0, 5)),
    ("jr vi15", lower(0x24, 0, 0, 15)),
    ("jalr vi15, vi2", lower(0x25, 0, 15, 2)),
    ("iadd vi1, vi2, vi3", lower(0x40, 0, 3, 2, 1 << 6 | 0x30)),
    ("isub vi1, vi2, vi3", lower(0x40, 0, 3, 2, 1 << 6 | 0x31)),
    ("iaddi vi1, vi2, -1", lower(0x40, 0, 1, 2, 31 << 6 | 0x32)),
    ("iand vi1, vi2, vi3", lower(0x40, 0, 3, 2, 1 << 6 | 0x34)),
    ("ior vi1, vi2, vi3", lower(0x40, 0, 3, 2, 1 << 6 | 0x35)),
    ("move.xyzw vf1, vf2", lspecial(0x30, XYZW, 1, 2)),
    ("mr32.xyzw vf1, vf2", lspecial(0x31, XYZW, 1, 2)),
    ("lqi.xyzw vf1, (vi2++)", lspecial(0x34, XYZW, 1, 2)),
    ("sqi.xyzw vf1, (vi2++)", lspecial(0x35, XYZW, 2, 1)),
    ("lqd.xyzw vf1, (--vi2)", lspecial(0x36, XYZW, 1, 2)),
    ("sqd.xyzw vf1, (--vi2)", lspecial(0x37, XYZW, 2, 1)),
    ("div q, vf1x, vf2y", lspecial(0x38, 0, 2, 1, 0, 1)),
    ("sqrt q, vf2w", lspecial(0x39, 0, 2, 0, 0, 3)),
    ("rsqrt q, vf1z, vf2w", lspecial(0x3A, 0, 2, 1, 2, 3)),
    ("waitq", lspecial(0x3B)),
    ("mtir vi1, vf2y", lspecial(0x3C, 0, 1, 2, 1)),
    ("mfir.xyzw vf1, vi2", lspecial(0x3D, XYZW, 1, 2)),
    ("ilwr.x vi1, (vi2)", lspecial(0x3E, 8 << 21, 1, 2)),
    ("iswr.w vi1, (vi2)", lspecial(0x3F, 1 << 21, 1, 2)),
    ("rnext.xyzw vf1, r", lspecial(0x40, XYZW, 1)),
    ("rget.x vf1, r", lspecial(0x41, 8 << 21, 1)),
    ("rinit r, vf2x", lspecial(0x42, 0, 0, 2, 0)),
    ("rxor r, vf2y", lspecial(0x43, 0, 0, 2, 1)),
    ("mfp.xyzw vf5, p", lspecial(0x64, XYZW, 5)),
    ("xtop vi1", lspecial(0x68, 0, 1)),
    ("xitop vi1", lspecial(0x69, 0, 1)),
    ("xgkick vi12", lspecial(0x6C, 0, 0, 12)),
    ("esadd.xyz p, vf1", lspecial(0x70, XYZ, 0, 1)),
    ("ersadd.xyz p, vf1", lspecial(0x71, XYZ, 0, 1)),
    ("eleng.xyz p, vf1", lspecial(0x72, XYZ, 0, 1)),
    ("erleng.xyz p, vf1", lspecial(0x73, XYZ, 0, 1)),
    ("eatanxy.xy p, vf1", lspecial(0x74, 0xC << 21, 0, 1)),
    ("eatanxz.xz p, vf1", lspecial(0x75, 0xA << 21, 0, 1)),
    ("esum.xyzw p, vf1", lspecial(0x76, XYZW, 0, 1)),
    ("esqrt p, vf1x", lspecial(0x78, 0, 0, 1, 0)),
    ("ersqrt p, vf1y", lspecial(0x79, 0, 0, 1, 1)),
    ("ercpr p, vf1z", lspecial(0x7A, 0, 0, 1, 2)),
    ("waitp", lspecial(0x7B)),
    ("esin p, vf3y", lspecial(0x7C, 0, 0, 3, 1)),
    ("eatan p, vf1w", lspecial(0x7D, 0, 0, 1, 3)),
    ("eexp p, vf1x", lspecial(0x7E, 0, 0, 1, 0)),
]

# (text, word) for branches at pc 10, with label "T" at 5 (offset -6) and "U" at 20 (offset +9).
BRANCH = [
    ("b T", lower(0x20, imm=0x7FA)),
    ("bal vi15, U", lower(0x21, 0, 15, 0, 9)),
    ("ibeq vi1, vi2, T", lower(0x28, 0, 1, 2, 0x7FA)),
    ("ibne vi1, vi2, U", lower(0x29, 0, 1, 2, 9)),
    ("ibltz vi3, T", lower(0x2C, 0, 0, 3, 0x7FA)),
    ("ibgtz vi3, U", lower(0x2D, 0, 0, 3, 9)),
    ("iblez vi3, T", lower(0x2E, 0, 0, 3, 0x7FA)),
    ("ibgez vi3, U", lower(0x2F, 0, 0, 3, 9)),
]
LABELS = {"T": 5, "U": 20}


class Upper(unittest.TestCase):
    def test_encode(self):
        for text, word in UPPER:
            with self.subTest(text):
                self.assertEqual(vuasm.asm_upper(text), word)

    def test_decode(self):
        for text, word in UPPER:
            with self.subTest(text):
                self.assertEqual(vuasm.dis_upper(word), text)

    def test_unknown_is_none(self):
        self.assertIsNone(vuasm.dis_upper(0x30))           # opcode 0x30 is not defined
        self.assertIsNone(vuasm.dis_upper(special(0x2B)))  # special 0x2B is not defined

    def test_bad_text(self):
        for text in ("frob.xyzw vf1, vf2", "addx.xyzw vf1, vf2, vf3y", "add.xyzq vf1, vf2, vf3", "add.xyzw vf32, vf1, vf2"):
            with self.subTest(text), self.assertRaises(ValueError):
                vuasm.asm_upper(text)


class Lower(unittest.TestCase):
    def test_encode(self):
        for text, word in LOWER:
            with self.subTest(text):
                self.assertEqual(vuasm.asm_lower(text, 0, {}), word)

    def test_decode(self):
        for text, word in LOWER:
            with self.subTest(text):
                self.assertEqual(vuasm.dis_lower(word, 0, lambda a: f"L{a:04X}"), text)

    def test_branches(self):
        for text, word in BRANCH:
            with self.subTest(text):
                self.assertEqual(vuasm.asm_lower(text, 10, LABELS), word)
                names = {5: "T", 20: "U"}
                self.assertEqual(vuasm.dis_lower(word, 10, names.get), text)

    def test_branch_to_number(self):
        self.assertEqual(vuasm.asm_lower("b 0x5", 10, {}), lower(0x20, imm=0x7FA))

    def test_unknown_is_none(self):
        self.assertIsNone(vuasm.dis_lower(0x02 << 25, 0, str))      # opcode 0x02 is not defined
        self.assertIsNone(vuasm.dis_lower(lspecial(0x50), 0, str))  # special 0x50 is not defined

    def test_out_of_range(self):
        for text in ("lq.xyzw vf1, 1024(vi2)", "iaddiu vi1, vi2, 0x8000", "iaddi vi1, vi2, 16", "b NOWHERE"):
            with self.subTest(text), self.assertRaises(ValueError):
                vuasm.asm_lower(text, 0, {})


def listing(body):
    return "\n".join(line.strip() for line in body.strip().splitlines()) + "\n"


class Assemble(unittest.TestCase):
    def test_packet(self):
        text = listing("""
            .dmatag cnt 0x1        ; 1 quadword after the tag
            .vif 0x00000000
            .vif 0x00000000
            .vif 0x01000404
            .vif 0x20000000
            .word 0x3F800000
            .vif 0x05000000
            .dmatag cnt 0x4
            .vif 0x00000000
            .vif 0x00000000
            .vif 0x00000000
            .mpg 0x10
            start:
                nop | iaddiu vi1, vi0, 0x3
            loop:
                nop | iaddi vi1, vi1, -1
                nop | ibne vi1, vi0, loop
                nop | nop [E]
                nop | nop
            .vif 0x00000000
            .vif 0x00000000
            .vif 0x00000000
            .vif 0x00000000
            .dmatag end 0x0
            .vif 0x00000000
            .vif 0x00000000
        """)
        words = struct.unpack("<32I", vuasm.assemble_text(text, "t.vsm"))
        self.assertEqual(words[:8], (0x10000001, 0, 0, 0, 0x01000404, 0x20000000, 0x3F800000, 0x05000000))
        self.assertEqual(words[8:14], (0x10000004, 0, 0, 0, 0, 0x4A050010))
        self.assertEqual(words[14:18], (0x10010003, 0x2FF, 0x80000000 | 1 << 16 | 1 << 11 | 31 << 6 | 0x32, 0x2FF))
        self.assertEqual(words[18], 0x52000000 | 1 << 16 | 0x7FE)   # ibne back to "loop" (0x11) from 0x12
        self.assertEqual(words[20:24], (0x8000033C, 0x2FF | 1 << 30, 0x8000033C, 0x2FF))
        self.assertEqual(words[24:], (0, 0, 0, 0, 0x70000000, 0, 0, 0))

    def test_mpg_header_and_flags(self):
        text = listing("""
            .mpg 0x4
                nop | nop [E]
                nop | loi 0x3F800000 [I]
        """)
        words = struct.unpack("<5I", vuasm.assemble_text(text, "t.vsm"))
        self.assertEqual(words, (0x4A020004, 0x8000033C, 0x2FF | 1 << 30, 0x3F800000, 0x2FF | 1 << 31))

    def test_256_instructions_count_as_zero(self):
        text = ".mpg 0x0\n" + "nop | nop\n" * 256
        self.assertEqual(struct.unpack_from("<I", vuasm.assemble_text(text, "t.vsm"))[0], 0x4A000000)

    def test_dma_size_checked(self):
        text = listing("""
            .dmatag cnt 0x2
            .vif 0x00000000
            .vif 0x00000000
            .word 0x0
            .word 0x0
            .word 0x0
            .word 0x0
        """)
        with self.assertRaises(SystemExit):
            vuasm.assemble_text(text, "t.vsm")

    def test_include(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "prog.vsm").write_text(".mpg 0x0\nnop | nop [E]\nnop | nop\n")
            (Path(tmp) / "pkt.vsm").write_text('.vif 0x00000000\n.include "prog.vsm"\n.vif 0x00000000\n')
            words = struct.unpack("<7I", vuasm.assemble(Path(tmp) / "pkt.vsm"))
        self.assertEqual(words, (0, 0x4A020000, 0x8000033C, 0x2FF | 1 << 30, 0x8000033C, 0x2FF, 0))

    def test_labels_are_local_to_their_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "a.vsm").write_text(".mpg 0x0\nentry:\nnop | b entry\n")
            (Path(tmp) / "b.vsm").write_text(".mpg 0x10\nentry:\nnop | b entry\n")
            (Path(tmp) / "pkt.vsm").write_text('.include "a.vsm"\n.include "b.vsm"\n')
            words = struct.unpack("<6I", vuasm.assemble(Path(tmp) / "pkt.vsm"))
        self.assertEqual(words, (0x4A010000, 0x40000000 | 0x7FF, 0x2FF, 0x4A010010, 0x40000000 | 0x7FF, 0x2FF))

    def test_errors(self):
        for text in ("nop | nop\n", ".mpg 0x0\nnop\n", ".mpg 0x0\na:\na:\nnop | nop\n", ".mpg 0x0\n" + "nop | nop\n" * 257):
            with self.subTest(text[:20]), self.assertRaises(SystemExit):
                vuasm.assemble_text(text, "t.vsm")


def packet(stream):
    """A DMA chain: one cnt tag carrying the VIF stream (padded to quadwords), then an end tag."""
    stream = list(stream) + [0] * (-len(stream) % 4)
    words = [0x10000000 | len(stream) // 4, 0, 0, 0] + stream + [0x70000000, 0, 0, 0]
    return struct.pack(f"<{len(words)}I", *words)


class RoundTrip(unittest.TestCase):
    def test_every_form(self):
        """A microprogram using every implemented form once, disassembled and assembled back."""
        lowers = [t for t, _ in LOWER] + [t.replace("T", "0x5").replace("U", "0x14") for t, _ in BRANCH]
        uppers = [t for t, _ in UPPER]
        n = max(len(lowers), len(uppers))
        code = []
        for i in range(n):
            code += [vuasm.asm_lower(lowers[i % len(lowers)], i, {}), vuasm.asm_upper(uppers[i % len(uppers)])]
        code[-1] |= 1 << 30   # E bit on the last pair
        data = packet([0x4A000000 | n << 16] + code)
        text, raws = vuasm.disassemble(data, "synthetic")
        self.assertEqual(raws, 0, text)
        self.assertEqual(vuasm.assemble_text(text, "synthetic.vsm"), data)

    def test_unknown_words_become_raw(self):
        data = packet([0x4A010000, 0x02 << 25, 0x30])
        text, raws = vuasm.disassemble(data, "raw")
        self.assertEqual(raws, 2)
        self.assertIn("raw 0x", text)
        self.assertEqual(vuasm.assemble_text(text, "raw.vsm"), data)

    def test_loi_and_unpack_data(self):
        data = packet([0x6C010000, 0x3F800000, 0, 0, 0x40000000, 0x14000000, 0x4A010000, 0x3F000000, 0x2FF | 1 << 31])
        text, raws = vuasm.disassemble(data, "data")
        self.assertEqual(raws, 0, text)
        self.assertIn("loi 0x3F000000", text)
        self.assertIn(".word 0x40000000", text)
        self.assertEqual(vuasm.assemble_text(text, "data.vsm"), data)


class VifSize(unittest.TestCase):
    def test_sizes(self):
        self.assertEqual(vuasm.vif_data_words(0x00000000), 0)    # NOP
        self.assertEqual(vuasm.vif_data_words(0x14000004), 0)    # MSCAL
        self.assertEqual(vuasm.vif_data_words(0x20000000), 1)    # STMASK
        self.assertEqual(vuasm.vif_data_words(0x30000000), 4)    # STROW
        self.assertEqual(vuasm.vif_data_words(0x4A030000), 6)    # MPG, 3 instructions
        self.assertEqual(vuasm.vif_data_words(0x4A000000), 512)  # MPG, 256 instructions
        self.assertEqual(vuasm.vif_data_words(0x50000002), 8)    # DIRECT, 2 quadwords
        self.assertEqual(vuasm.vif_data_words(0x6C020000), 8)    # UNPACK V4-32, 2 vectors
        self.assertEqual(vuasm.vif_data_words(0x6E030000), 3)    # UNPACK V4-8, 3 vectors = 12 bytes
        self.assertEqual(vuasm.vif_data_words(0x65030000), 3)    # UNPACK V2-16, 3 vectors = 12 bytes
        self.assertEqual(vuasm.vif_data_words(0x6F030000), 2)    # UNPACK V4-5, 3 vectors = 6 bytes, padded


if __name__ == "__main__":
    unittest.main()
