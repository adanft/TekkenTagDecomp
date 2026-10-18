#!/usr/bin/env python3
"""Assembler and disassembler for the game's VU microcode packets (src/vu0/*.vsm, src/vu1/*.vsm).

  vuasm.py -o out.bin a.vsm [b.vsm ...]    assemble, concatenated in the order given
  vuasm.py --disasm FILE START END [NAME]  print a listing of FILE[START:END] (file offsets; a DMA chain)
  vuasm.py [--check]                       round trip over every packet of the executable (iso/SLUS_200.01):
                                           disassemble, assemble back, compare; and compare the listings

The packets are DMA chains for VIF0/VIF1 (docs/systems/vu1/README.md). Listing syntax (`;` starts a comment):
    .dmatag <id> <qwc>       a DMA tag (id: refe cnt next ref refs call ret end); the two words that follow it in
                             the tag's quadword are VIF codes. A tag's qwc is checked against the quadwords up to
                             the next tag (or the end of the file).
    .vif 0x........          a VIF code
    .word 0x........         any other 32-bit word (STMASK/UNPACK data)
    .mpg <addr>              a VIF MPG code loading the instructions that follow (up to the next directive) at
                             <addr> (in 64-bit words) in micro memory
    .include "file"          the lines of another listing (path relative to this one)
    label:
        upper | lower [flags]   flags: any of I E M D T (bits 31..27 of the upper word)
Branch targets are labels or absolute addresses; a label's value is the instruction's address in micro memory,
and it is visible only in the file that defines it.
With the I flag the lower word is a float constant: `loi 0x3F800000`. `raw 0x........` in either half gives the
word as is, for an encoding the mnemonics do not reproduce (the disassembler writes it for unknown words)."""
import re
import struct
import sys
from pathlib import Path

XYZW = "xyzw"
FLAGS = {"I": 31, "E": 30, "M": 29, "D": 28, "T": 27}
DMA_IDS = ["refe", "cnt", "next", "ref", "refs", "call", "ret", "end"]

# Upper instructions. Opcode = bits 5..0; 0x3C..0x3F select the special table, indexed by bits 10..6 << 2 | 1..0.
U_BC = {0x00: "add", 0x04: "sub", 0x08: "madd", 0x0C: "msub", 0x10: "max", 0x14: "mini", 0x18: "mul"}
U_QI = {0x1C: ("mul", "q"), 0x1D: ("max", "i"), 0x1E: ("mul", "i"), 0x1F: ("mini", "i"), 0x20: ("add", "q"),
        0x21: ("madd", "q"), 0x22: ("add", "i"), 0x23: ("madd", "i"), 0x24: ("sub", "q"), 0x25: ("msub", "q"),
        0x26: ("sub", "i"), 0x27: ("msub", "i")}
U_3 = {0x28: "add", 0x29: "madd", 0x2A: "mul", 0x2B: "max", 0x2C: "sub", 0x2D: "msub", 0x2E: "opmsub", 0x2F: "mini"}
US_BC = {0x00: "adda", 0x04: "suba", 0x08: "madda", 0x0C: "msuba", 0x18: "mula"}
US_CONV = {0x10: "itof0", 0x11: "itof4", 0x12: "itof12", 0x13: "itof15", 0x14: "ftoi0", 0x15: "ftoi4",
           0x16: "ftoi12", 0x17: "ftoi15", 0x1D: "abs"}
US_QI = {0x1C: ("mula", "q"), 0x1E: ("mula", "i"), 0x20: ("adda", "q"), 0x21: ("madda", "q"), 0x22: ("adda", "i"),
         0x23: ("madda", "i"), 0x24: ("suba", "q"), 0x25: ("msuba", "q"), 0x26: ("suba", "i"), 0x27: ("msuba", "i")}
US_3 = {0x28: "adda", 0x29: "madda", 0x2A: "mula", 0x2C: "suba", 0x2D: "msuba", 0x2E: "opmula"}
US_CLIP, US_NOP = 0x1F, 0x2F

# Lower instructions. Opcode = bits 31..25; 0x40 selects bits 5..0, where 0x3C..0x3F select the special table.
L_MEM = {0x00: "lq", 0x01: "sq", 0x04: "ilw", 0x05: "isw"}
L_IMM15 = {0x08: "iaddiu", 0x09: "isubiu"}
L_FC = {0x10: "fceq", 0x11: "fcset", 0x12: "fcand", 0x13: "fcor"}
L_FS = {0x14: "fseq", 0x15: "fsset", 0x16: "fsand", 0x17: "fsor"}
L_FM = {0x18: "fmeq", 0x1A: "fmand", 0x1B: "fmor"}
L_BR2 = {0x28: "ibeq", 0x29: "ibne"}
L_BR1 = {0x2C: "ibltz", 0x2D: "ibgtz", 0x2E: "iblez", 0x2F: "ibgez"}
L_INT3 = {0x30: "iadd", 0x31: "isub", 0x34: "iand", 0x35: "ior"}
# special code -> (mnemonic, operand form)
L_SP = {0x30: ("move", "ft_fs"), 0x31: ("mr32", "ft_fs"), 0x34: ("lqi", "lqi"), 0x35: ("sqi", "sqi"),
        0x36: ("lqd", "lqd"), 0x37: ("sqd", "sqd"), 0x38: ("div", "q_fsf_ftf"), 0x39: ("sqrt", "q_ftf"),
        0x3A: ("rsqrt", "q_fsf_ftf"), 0x3B: ("waitq", "none"), 0x3C: ("mtir", "it_fsf"), 0x3D: ("mfir", "ft_is"),
        0x3E: ("ilwr", "it_(is)"), 0x3F: ("iswr", "it_(is)"), 0x40: ("rnext", "ft_r"), 0x41: ("rget", "ft_r"),
        0x42: ("rinit", "r_fsf"), 0x43: ("rxor", "r_fsf"), 0x64: ("mfp", "ft_p"), 0x68: ("xtop", "it"),
        0x69: ("xitop", "it"), 0x6C: ("xgkick", "is"), 0x70: ("esadd", "p_fs"), 0x71: ("ersadd", "p_fs"),
        0x72: ("eleng", "p_fs"), 0x73: ("erleng", "p_fs"), 0x74: ("eatanxy", "p_fs"), 0x75: ("eatanxz", "p_fs"),
        0x76: ("esum", "p_fs"), 0x78: ("esqrt", "p_fsf"), 0x79: ("ersqrt", "p_fsf"), 0x7A: ("ercpr", "p_fsf"),
        0x7B: ("waitp", "none"), 0x7C: ("esin", "p_fsf"), 0x7D: ("eatan", "p_fsf"), 0x7E: ("eexp", "p_fsf")}
L_SP_BY_NAME = {name: (code, form) for code, (name, form) in L_SP.items()}
L_NOP = 0x8000033C   # move with an empty destination mask
U_NOP = 0x2FF


def inv(table):
    return {v: k for k, v in table.items()}


def dest(d):
    return "".join(c for i, c in enumerate(XYZW) if d >> (3 - i) & 1)


def sext(value, bits):
    return value - (1 << bits) if value >> (bits - 1) & 1 else value


# ------------------------------------------------------------------------------------------------ disassembly

def dis_upper(w):
    """Upper word (without the flag bits 31..27) -> text, or None for a word the mnemonics do not cover."""
    d, ft, fs, fd, op, bc = w >> 21 & 15, w >> 16 & 31, w >> 11 & 31, w >> 6 & 31, w & 63, w & 3
    D = dest(d)
    if op < 0x1C:
        name = U_BC.get(op & ~3)
        return f"{name}{XYZW[bc]}.{D} vf{fd}, vf{fs}, vf{ft}{XYZW[bc]}" if name else None
    if op in U_QI:
        name, src = U_QI[op]
        return f"{name}{src}.{D} vf{fd}, vf{fs}, {src}"
    if op in U_3:
        return f"{U_3[op]}.{D} vf{fd}, vf{fs}, vf{ft}"
    if op < 0x3C:
        return None
    sp = fd << 2 | bc
    if sp == US_NOP:
        return "nop"
    if sp < 0x10 or 0x18 <= sp < 0x1C:
        name = US_BC.get(sp & ~3)
        return f"{name}{XYZW[bc]}.{D} acc, vf{fs}, vf{ft}{XYZW[bc]}" if name else None
    if sp in US_CONV:
        return f"{US_CONV[sp]}.{D} vf{ft}, vf{fs}"
    if sp == US_CLIP:
        return f"clipw.{D} vf{fs}, vf{ft}w"
    if sp in US_QI:
        name, src = US_QI[sp]
        return f"{name}{src}.{D} acc, vf{fs}, {src}"
    if sp in US_3:
        return f"{US_3[sp]}.{D} acc, vf{fs}, vf{ft}"
    return None


def dis_lower(w, pc, label):
    """Lower word -> text, or None. label(address) names a branch target (None: write the address)."""
    op, d, it, is_, id_ = w >> 25, w >> 21 & 15, w >> 16 & 31, w >> 11 & 31, w >> 6 & 31
    D = dest(d)
    imm11 = sext(w & 0x7FF, 11)
    fsf, ftf = XYZW[w >> 21 & 3], XYZW[w >> 23 & 3]

    def target():
        a = pc + 1 + imm11
        return label(a) or f"{a:#x}"

    if op in L_MEM:
        n = L_MEM[op]
        if n == "lq":
            return f"lq.{D} vf{it}, {imm11}(vi{is_})"
        if n == "sq":
            return f"sq.{D} vf{is_}, {imm11}(vi{it})"
        return f"{n}.{D} vi{it}, {imm11}(vi{is_})"
    if op in L_IMM15:
        return f"{L_IMM15[op]} vi{it}, vi{is_}, {(w >> 10 & 0x7800) | (w & 0x7FF):#x}"
    if op in L_FC:
        return f"fcset {w & 0xFFFFFF:#x}" if op == 0x11 else f"{L_FC[op]} vi1, {w & 0xFFFFFF:#x}"
    if op in L_FS:
        imm12 = (w >> 10 & 0x800) | (w & 0x7FF)
        return f"fsset {imm12:#x}" if op == 0x15 else f"{L_FS[op]} vi{it}, {imm12:#x}"
    if op in L_FM:
        return f"{L_FM[op]} vi{it}, vi{is_}"
    if op == 0x1C:
        return f"fcget vi{it}"
    if op == 0x20:
        return f"b {target()}"
    if op == 0x21:
        return f"bal vi{it}, {target()}"
    if op == 0x24:
        return f"jr vi{is_}"
    if op == 0x25:
        return f"jalr vi{it}, vi{is_}"
    if op in L_BR2:
        return f"{L_BR2[op]} vi{it}, vi{is_}, {target()}"
    if op in L_BR1:
        return f"{L_BR1[op]} vi{is_}, {target()}"
    if op != 0x40:
        return None
    f = w & 63
    if f in L_INT3:
        return f"{L_INT3[f]} vi{id_}, vi{is_}, vi{it}"
    if f == 0x32:
        return f"iaddi vi{it}, vi{is_}, {sext(id_, 5)}"
    if f < 0x3C:
        return None
    if w == L_NOP:
        return "nop"
    entry = L_SP.get(id_ << 2 | (w & 3))
    if entry is None:
        return None
    n, form = entry
    return n + {
        "ft_fs": f".{D} vf{it}, vf{is_}",
        "lqi": f".{D} vf{it}, (vi{is_}++)",
        "sqi": f".{D} vf{is_}, (vi{it}++)",
        "lqd": f".{D} vf{it}, (--vi{is_})",
        "sqd": f".{D} vf{is_}, (--vi{it})",
        "q_fsf_ftf": f" q, vf{is_}{fsf}, vf{it}{ftf}",
        "q_ftf": f" q, vf{it}{ftf}",
        "none": "",
        "it_fsf": f" vi{it}, vf{is_}{fsf}",
        "ft_is": f".{D} vf{it}, vi{is_}",
        "it_(is)": f".{D} vi{it}, (vi{is_})",
        "ft_r": f".{D} vf{it}, r",
        "r_fsf": f" r, vf{is_}{fsf}",
        "ft_p": f".{D} vf{it}, p",
        "it": f" vi{it}",
        "is": f" vi{is_}",
        "p_fs": f".{D} p, vf{is_}",
        "p_fsf": f" p, vf{is_}{fsf}",
    }[form]


# --------------------------------------------------------------------------------------------------- assembly

def reg(tok, kind):
    m = re.fullmatch(kind + r"(\d+)", tok)
    if not m or int(m.group(1)) > 31:
        raise ValueError(f"expected a {kind} register, got '{tok}'")
    return int(m.group(1))


def regc(tok):
    """vfNc -> (N, component index)."""
    m = re.fullmatch(r"vf(\d+)([xyzw])", tok)
    if not m or int(m.group(1)) > 31:
        raise ValueError(f"expected vfN plus a component, got '{tok}'")
    return int(m.group(1)), XYZW.index(m.group(2))


def dmask(s):
    if s is None:
        return 0
    if not re.fullmatch(r"x?y?z?w?", s):
        raise ValueError(f"bad destination mask '{s}'")
    return sum(1 << (3 - XYZW.index(c)) for c in s)


def split_ins(text):
    m = re.match(r"(\S+)\s*(.*)", text.strip())
    if not m:
        raise ValueError("empty instruction")
    name, rest = m.group(1), m.group(2)
    mask = None
    if "." in name:
        name, mask = name.split(".", 1)
    ops = [o.strip() for o in rest.split(",")] if rest else []
    return name, mask, ops


def expect(ops, n):
    if len(ops) != n:
        raise ValueError(f"expected {n} operands, got {len(ops)}")
    return ops


def special_word(code, d=0, ft=0, fs=0):
    return d | ft << 16 | fs << 11 | (code >> 2) << 6 | 0x3C | (code & 3)


def asm_upper(text):
    name, mask, o = split_ins(text)
    if name == "raw":
        return int(expect(o, 1)[0], 0)
    if name == "nop":
        expect(o, 0)
        return U_NOP
    d = dmask(mask) << 21
    if name == "clipw":
        expect(o, 2)
        ft, c = regc(o[1])
        if c != 3:
            raise ValueError("clip compares with the w component")
        return special_word(US_CLIP, d, ft, reg(o[0], "vf"))
    if name in inv(US_CONV):
        expect(o, 2)
        return special_word(inv(US_CONV)[name], d, reg(o[0], "vf"), reg(o[1], "vf"))
    if o and o[0] == "acc":
        expect(o, 3)
        fs = reg(o[1], "vf")
        if o[2] in ("q", "i") and (name[:-1], name[-1]) in inv(US_QI):
            if name[-1] != o[2]:
                raise ValueError(f"'{name}' takes {name[-1]}")
            return special_word(inv(US_QI)[(name[:-1], o[2])], d, 0, fs)
        if name in inv(US_3) and re.fullmatch(r"vf\d+", o[2]):
            return special_word(inv(US_3)[name], d, reg(o[2], "vf"), fs)
        if name[:-1] in inv(US_BC) and name[-1] in XYZW:
            ft, bc = regc(o[2])
            if bc != XYZW.index(name[-1]):
                raise ValueError("the components of the mnemonic and of the operand differ")
            return special_word(inv(US_BC)[name[:-1]] | bc, d, ft, fs)
        raise ValueError(f"unknown upper instruction '{text}'")
    expect(o, 3)
    fd, fs = reg(o[0], "vf"), reg(o[1], "vf")
    if o[2] in ("q", "i") and (name[:-1], name[-1]) in inv(U_QI):
        if name[-1] != o[2]:
            raise ValueError(f"'{name}' takes {name[-1]}")
        return d | fs << 11 | fd << 6 | inv(U_QI)[(name[:-1], o[2])]
    if name in inv(U_3) and re.fullmatch(r"vf\d+", o[2]):
        return d | reg(o[2], "vf") << 16 | fs << 11 | fd << 6 | inv(U_3)[name]
    if name[:-1] in inv(U_BC) and name[-1] in XYZW:
        ft, bc = regc(o[2])
        if bc != XYZW.index(name[-1]):
            raise ValueError("the components of the mnemonic and of the operand differ")
        return d | ft << 16 | fs << 11 | fd << 6 | inv(U_BC)[name[:-1]] | bc
    raise ValueError(f"unknown upper instruction '{text}'")


def mem(tok):
    m = re.fullmatch(r"(-?\d+)\(vi(\d+)\)", tok)
    if not m or int(m.group(2)) > 31:
        raise ValueError(f"expected imm(viN), got '{tok}'")
    imm = int(m.group(1))
    if not -1024 <= imm < 1024:
        raise ValueError("offset out of range")
    return imm & 0x7FF, int(m.group(2))


def addr_reg(tok, pattern):
    m = re.fullmatch(pattern, tok)
    if not m or int(m.group(1)) > 31:
        raise ValueError(f"bad address operand '{tok}'")
    return int(m.group(1))


def asm_lower(text, pc, labels):
    name, mask, o = split_ins(text)
    if name in ("raw", "loi"):
        return int(expect(o, 1)[0], 0)
    if name == "nop":
        expect(o, 0)
        return L_NOP
    d = dmask(mask) << 21

    def rel(tok):
        if tok in labels:
            a = labels[tok]
        elif re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", tok):
            a = int(tok, 0)
        else:
            raise ValueError(f"unknown label '{tok}'")
        v = a - (pc + 1)
        if not -1024 <= v < 1024:
            raise ValueError("branch out of range")
        return v & 0x7FF

    def imm(tok, lo, hi):
        v = int(tok, 0)
        if not lo <= v < hi:
            raise ValueError(f"immediate {tok} out of range")
        return v

    if name in inv(L_MEM):
        expect(o, 2)
        off, base = mem(o[1])
        r = reg(o[0], "vi" if name[0] == "i" else "vf")
        if name == "sq":
            return 0x01 << 25 | d | base << 16 | r << 11 | off
        return inv(L_MEM)[name] << 25 | d | r << 16 | base << 11 | off
    if name in inv(L_IMM15):
        expect(o, 3)
        v = imm(o[2], 0, 0x8000)
        return inv(L_IMM15)[name] << 25 | (v & 0x7800) << 10 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11 | (v & 0x7FF)
    if name in inv(L_FC):
        if name == "fcset":
            return 0x11 << 25 | imm(expect(o, 1)[0], 0, 1 << 24)
        expect(o, 2)
        if o[0] != "vi1":
            raise ValueError(f"'{name}' writes vi1")
        return inv(L_FC)[name] << 25 | imm(o[1], 0, 1 << 24)
    if name in inv(L_FS):
        if name == "fsset":
            v, it = imm(expect(o, 1)[0], 0, 1 << 12), 0
        else:
            expect(o, 2)
            v, it = imm(o[1], 0, 1 << 12), reg(o[0], "vi")
        return inv(L_FS)[name] << 25 | (v & 0x800) << 10 | it << 16 | (v & 0x7FF)
    if name in inv(L_FM):
        expect(o, 2)
        return inv(L_FM)[name] << 25 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11
    if name == "fcget":
        return 0x1C << 25 | reg(expect(o, 1)[0], "vi") << 16
    if name == "b":
        return 0x20 << 25 | rel(expect(o, 1)[0])
    if name == "bal":
        expect(o, 2)
        return 0x21 << 25 | reg(o[0], "vi") << 16 | rel(o[1])
    if name == "jr":
        return 0x24 << 25 | reg(expect(o, 1)[0], "vi") << 11
    if name == "jalr":
        expect(o, 2)
        return 0x25 << 25 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11
    if name in inv(L_BR2):
        expect(o, 3)
        return inv(L_BR2)[name] << 25 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11 | rel(o[2])
    if name in inv(L_BR1):
        expect(o, 2)
        return inv(L_BR1)[name] << 25 | reg(o[0], "vi") << 11 | rel(o[1])
    if name in inv(L_INT3):
        expect(o, 3)
        return 0x40 << 25 | reg(o[2], "vi") << 16 | reg(o[1], "vi") << 11 | reg(o[0], "vi") << 6 | inv(L_INT3)[name]
    if name == "iaddi":
        expect(o, 3)
        return 0x40 << 25 | reg(o[0], "vi") << 16 | reg(o[1], "vi") << 11 | (imm(o[2], -16, 16) & 31) << 6 | 0x32
    if name not in L_SP_BY_NAME:
        raise ValueError(f"unknown lower instruction '{text}'")
    code, form = L_SP_BY_NAME[name]

    def sp(it=0, is_=0, fsf=0, ftf=0, dd=d):
        return 0x40 << 25 | ftf << 23 | fsf << 21 | dd | it << 16 | is_ << 11 | (code >> 2) << 6 | 0x3C | (code & 3)

    if form == "none":
        expect(o, 0)
        return sp()
    if form in ("it", "is"):
        r = reg(expect(o, 1)[0], "vi")
        return sp(r) if form == "it" else sp(0, r)
    if form == "ft_fs":
        expect(o, 2)
        return sp(reg(o[0], "vf"), reg(o[1], "vf"))
    if form in ("lqi", "sqi", "lqd", "sqd"):
        expect(o, 2)
        vi = addr_reg(o[1], r"\(vi(\d+)\+\+\)" if form[2] == "i" else r"\(--vi(\d+)\)")
        vf = reg(o[0], "vf")
        return sp(vf, vi) if form[0] == "l" else sp(vi, vf)
    if form == "q_fsf_ftf":
        expect(o, 3)
        if o[0] != "q":
            raise ValueError(f"'{name}' writes q")
        (fs, fsf), (ft, ftf) = regc(o[1]), regc(o[2])
        return sp(ft, fs, fsf, ftf, 0)
    if form == "q_ftf":
        expect(o, 2)
        if o[0] != "q":
            raise ValueError(f"'{name}' writes q")
        ft, ftf = regc(o[1])
        return sp(ft, 0, 0, ftf, 0)
    if form == "it_fsf":
        expect(o, 2)
        fs, fsf = regc(o[1])
        return sp(reg(o[0], "vi"), fs, fsf, 0, 0)
    if form == "ft_is":
        expect(o, 2)
        return sp(reg(o[0], "vf"), reg(o[1], "vi"))
    if form == "it_(is)":
        expect(o, 2)
        return sp(reg(o[0], "vi"), addr_reg(o[1], r"\(vi(\d+)\)"))
    if form in ("ft_r", "ft_p"):
        expect(o, 2)
        if o[1] != form[-1]:
            raise ValueError(f"'{name}' reads {form[-1]}")
        return sp(reg(o[0], "vf"))
    if form in ("r_fsf", "p_fs", "p_fsf"):
        expect(o, 2)
        if o[0] != form[0]:
            raise ValueError(f"'{name}' writes {form[0]}")
        if form == "p_fs":   # the mask names the components read (the game's code: erleng.xyz, eatanxz.xz)
            return sp(0, reg(o[1], "vf"))
        fs, fsf = regc(o[1])
        return sp(0, fs, fsf, 0, 0)
    raise ValueError(f"unknown lower instruction '{text}'")


def read_lines(path, text=None, depth=0):
    """(file, line number, text) of a listing, with .include expanded."""
    path = Path(path)
    if depth > 8:
        raise SystemExit(f"{path}: .include nested too deeply")
    if text is None:
        text = path.read_text()
    for num, line in enumerate(text.splitlines(), 1):
        m = re.fullmatch(r'\s*\.include\s+"([^"]+)"\s*(;.*)?', line)
        if m:
            inc = path.parent / m.group(1)
            if not inc.is_file():
                raise SystemExit(f"{path}:{num}: cannot include '{m.group(1)}'")
            yield from read_lines(inc, None, depth + 1)
        else:
            yield str(path), num, line


def assemble_text(text, path="<listing>"):
    """A listing (text; .include paths relative to `path`) -> bytes."""
    items = []   # ("word", value, kind, where) | ("mpg", addr, where) | ("ins", upper, lower, flags, where, pc, src)
    labels = {}   # file -> {label: address}: a label is visible in the file that defines it
    pc = None
    for src, num, line in read_lines(path, text):
        where = f"{src}:{num}"
        line = line.split(";", 1)[0].strip()
        while True:
            m = re.match(r"([A-Za-z_]\w*):\s*", line)
            if not m:
                break
            if pc is None:
                raise SystemExit(f"{where}: label outside a .mpg block")
            scope = labels.setdefault(src, {})
            if m.group(1) in scope:
                raise SystemExit(f"{where}: label '{m.group(1)}' defined twice")
            scope[m.group(1)] = pc
            line = line[m.end():]
        if not line:
            continue
        try:
            if line.startswith("."):
                words = line.split()
                if words[0] in (".vif", ".word"):
                    items.append(("word", int(words[1], 0) & 0xFFFFFFFF, words[0], where))
                elif words[0] == ".dmatag":
                    tid, qwc = DMA_IDS.index(words[1]), int(words[2], 0)
                    if not 0 <= qwc < 0x10000:
                        raise ValueError("qwc out of range")
                    items.append(("dma", tid << 28 | qwc, qwc, where))
                elif words[0] == ".mpg":
                    pc = int(words[1], 0)
                    items.append(("mpg", pc, where))
                    continue
                else:
                    raise ValueError(f"unknown directive '{words[0]}'")
                pc = None
                continue
        except (ValueError, IndexError) as e:
            raise SystemExit(f"{where}: {e}")
        if pc is None:
            raise SystemExit(f"{where}: instruction outside a .mpg block")
        flags = 0
        m = re.search(r"\[([IEMDT]+)\]\s*$", line)
        if m:
            flags = sum(1 << FLAGS[c] for c in m.group(1))
            line = line[:m.start()]
        if "|" not in line:
            raise SystemExit(f"{where}: expected 'upper | lower'")
        up, lo = line.split("|", 1)
        items.append(("ins", up, lo, flags, where, pc, src))
        pc += 1
    out = []
    tag = None   # (index in out, qwc, where) of the last DMA tag

    def check_tag():
        if tag is not None and len(out) - tag[0] != 4 + tag[1] * 4:
            raise SystemExit(f"{tag[2]}: the tag says {tag[1]:#x} quadwords, "
                             f"{(len(out) - tag[0] - 4) / 4:g} follow")

    i = 0
    while i < len(items):
        it = items[i]
        if it[0] == "word":
            out.append(it[1])
        elif it[0] == "dma":
            check_tag()
            tag = (len(out), it[2], it[3])
            out += [it[1], 0]
        elif it[0] == "mpg":
            j = i + 1
            while j < len(items) and items[j][0] == "ins":
                j += 1
            n = j - i - 1
            if not 1 <= n <= 256:
                raise SystemExit(f"{it[2]}: a .mpg block has {n} instructions (1..256 allowed)")
            if not 0 <= it[1] < 0x10000:
                raise SystemExit(f"{it[2]}: address out of range")
            out.append(0x4A000000 | (n & 0xFF) << 16 | it[1])
            for _, up, lo, flags, where, ipc, src in items[i + 1:j]:
                try:
                    lower = int(lo.split()[1], 0) if lo.split()[0] == "loi" else asm_lower(lo, ipc, labels.get(src, {}))
                    if (lo.split()[0] == "loi") != bool(flags >> 31 & 1):
                        raise ValueError("loi needs the I flag and the I flag needs loi")
                    out.append(lower & 0xFFFFFFFF)
                    out.append((asm_upper(up) | flags) & 0xFFFFFFFF)
                except (ValueError, IndexError) as e:
                    raise SystemExit(f"{where}: {e}")
            i = j
            continue
        i += 1
    check_tag()
    return struct.pack(f"<{len(out)}I", *out)


def assemble(path):
    """One listing file -> bytes."""
    return assemble_text(Path(path).read_text(), path)


# ---------------------------------------------------------------------------------------- packet disassembly

VIF_NAMES = {0x00: "NOP", 0x01: "STCYCL", 0x02: "OFFSET", 0x03: "BASE", 0x04: "ITOP", 0x05: "STMOD",
             0x06: "MSKPATH3", 0x07: "MARK", 0x10: "FLUSHE", 0x11: "FLUSH", 0x13: "FLUSHA", 0x14: "MSCAL",
             0x15: "MSCALF", 0x17: "MSCNT", 0x20: "STMASK", 0x30: "STROW", 0x31: "STCOL", 0x4A: "MPG",
             0x50: "DIRECT", 0x51: "DIRECTHL"}
UNPACK_FORMATS = ["S-32", "S-16", "S-8", "S-?", "V2-32", "V2-16", "V2-8", "V2-?", "V3-32", "V3-16", "V3-8", "V3-?",
                  "V4-32", "V4-16", "V4-8", "V4-5"]


def vif_data_words(code):
    """The number of 32-bit words of data that follow a VIF code."""
    cmd, num, imm = code >> 24 & 0x7F, code >> 16 & 0xFF, code & 0xFFFF
    if cmd == 0x20:
        return 1
    if cmd in (0x30, 0x31):
        return 4
    if cmd == 0x4A:
        return (num or 256) * 2
    if cmd in (0x50, 0x51):
        return (imm or 0x10000) * 4
    if cmd >= 0x60:
        vn, vl = cmd >> 2 & 3, cmd & 3
        bits = 16 if vl == 3 else (32 >> vl) * (vn + 1)
        return ((num or 256) * bits + 31) // 32
    return 0


def vif_comment(code):
    cmd, num, imm = code >> 24 & 0x7F, code >> 16 & 0xFF, code & 0xFFFF
    if cmd >= 0x60:
        return f"UNPACK {UNPACK_FORMATS[cmd & 15]} num={num or 256} addr={imm & 0x3FF:#x}" + \
            (" usn" if imm & 0x4000 else "") + (" +tops" if imm & 0x8000 else "") + (" masked" if cmd & 0x10 else "")
    name = VIF_NAMES.get(cmd, f"cmd {cmd:#x}")
    if cmd == 0x01:
        return f"STCYCL cl={imm & 0xFF} wl={imm >> 8}"
    if cmd in (0x02, 0x03, 0x04, 0x05, 0x06, 0x07):
        return f"{name} {imm:#x}"
    if cmd in (0x14, 0x15):
        return f"{name} {imm:#x}"
    return name + (" (irq)" if code >> 31 else "")


def disassemble(data, name):
    """A DMA chain (tags, VIF codes and MPG microcode) -> (listing text, number of `raw` halves). Every instruction
    is assembled again and compared; a half that does not come back the same is written as `raw`."""
    if len(data) % 16:
        raise ValueError("a DMA chain is a whole number of quadwords")
    words = struct.unpack(f"<{len(data) // 4}I", data)
    items = []   # ("tag", lo, hi) | ("vif", code) | ("word", w) | ("mpg", addr, [words])
    pos = 0
    while pos < len(words):
        lo, hi = words[pos], words[pos + 1]
        items.append(("tag", lo, hi))
        end = min(len(words), pos + 4 + (lo & 0xFFFF) * 4)
        i = pos + 2
        while i < end:
            code = words[i]
            n = vif_data_words(code)
            if code >> 24 & 0x7F == 0x4A and i + 1 + n <= end:
                items.append(("mpg", code & 0xFFFF, words[i + 1:i + 1 + n]))
            elif i + 1 + n <= end:
                items.append(("vif", code))
                items += [("word", w) for w in words[i + 1:i + 1 + n]]
            else:   # data running past the tag's block: leave it as words
                items += [("word", w) for w in words[i:end]]
                i = end
                continue
            i += 1 + n
        pos = end
    targets = set()
    for it in items:   # first pass: branch targets
        if it[0] == "mpg":
            for k in range(len(it[2]) // 2):
                if not it[2][k * 2 + 1] >> 31:
                    dis_lower(it[2][k * 2], it[1] + k, lambda a: targets.add(a) or "x")
    name_of = {a: f"L{a:04X}" for a in targets}
    out = [f"; {name}", ""]
    raws = 0
    for it in items:
        if it[0] == "tag":
            lo, hi = it[1], it[2]
            if hi == 0 and not lo & ~0x7000FFFF:
                out.append(f".dmatag {DMA_IDS[lo >> 28 & 7]} {lo & 0xFFFF:#x}")
            else:
                out += [f".word 0x{lo:08X}", f".word 0x{hi:08X}"]
            continue
        if it[0] == "vif":
            out.append(f".vif 0x{it[1]:08X}".ljust(24) + f"; {vif_comment(it[1])}")
            continue
        if it[0] == "word":
            f = struct.unpack("<f", struct.pack("<I", it[1]))[0]
            note = f"   ; {f:.9g}" if it[1] and 1e-6 < abs(f) < 1e9 else ""
            out.append(f".word 0x{it[1]:08X}{note}")
            continue
        out.append(f".mpg {it[1]:#x}")
        code = it[2]
        for k in range(len(code) // 2):
            lo, hi = code[k * 2], code[k * 2 + 1]
            pc = it[1] + k
            flags = "".join(c for c, b in FLAGS.items() if hi >> b & 1)
            body = hi & 0x07FFFFFF
            up = dis_upper(body)
            try:
                if up is None or asm_upper(up) != body:
                    up = None
            except (ValueError, IndexError):
                up = None
            if up is None:
                up, raws = f"raw 0x{body:08X}", raws + 1
            note = ""
            if hi >> 31:
                low = f"loi 0x{lo:08X}"
                note = f"   ; {struct.unpack('<f', struct.pack('<I', lo))[0]:.9g}"
            else:
                low = dis_lower(lo, pc, name_of.get)
                try:
                    if low is None or asm_lower(low, pc, {v: k for k, v in name_of.items()}) != lo:
                        low = None
                except (ValueError, IndexError):
                    low = None
                if low is None:
                    low, raws = f"raw 0x{lo:08X}", raws + 1
            if pc in targets:
                out.append(f"{name_of[pc]}:")
            out.append(f"    {up:36s} | {low}{(' [' + flags + ']') if flags else ''}{note}".rstrip())
    return "\n".join(out) + "\n", raws


# -------------------------------------------------------------------------------------------- whole-ELF check

ROOT = Path(__file__).resolve().parent.parent
ELF = ROOT / "iso" / "SLUS_200.01"
VUDATA = 0x4F5F00            # .vudata address
VUDATA_OFFSET = 0x1B6F00     # its file offset
# The microcode packets in .vudata: (symbol address, end, listing). Two VU0 packets are byte-identical and are
# assembled from the same listing.
PACKETS = [
    (0x4F5F00, 0x4F6C80, "src/vu0/vu0_full.vsm"),
    (0x4F6C80, 0x4F7A00, "src/vu0/vu0_full.vsm"),
    (0x4F7A00, 0x4FA7E0, "src/vu1/vu1.vsm"),
    (0x4FA7E0, 0x4FAF70, "src/vu0/vu0_small.vsm"),
]


def mpg_blocks(data):
    """(address, code bytes) of every MPG block in a DMA chain, in order."""
    words = struct.unpack(f"<{len(data) // 4}I", data)
    out, pos = [], 0
    while pos + 1 < len(words):
        end = min(len(words), pos + 4 + (words[pos] & 0xFFFF) * 4)
        i = pos + 2
        while i < end:
            n = vif_data_words(words[i])
            if words[i] >> 24 & 0x7F == 0x4A:
                out.append((words[i] & 0xFFFF, data[(i + 1) * 4:(i + 1 + n) * 4]))
            i += 1 + n
        pos = end
    return out


def check():
    """Round trip of every packet of the executable and comparison with its committed listing, per MPG block
    (one microprogram, or part of one). Returns the number of failures."""
    data = ELF.read_bytes()
    ok = failed = 0
    for start, end, listing in PACKETS:
        blob = data[start - VUDATA + VUDATA_OFFSET:end - VUDATA + VUDATA_OFFSET]
        text, raws = disassemble(blob, f"{start:#x}")
        again = assemble_text(text, "<roundtrip>")
        path = ROOT / listing
        built = assemble(path) if path.exists() else b""
        theirs, mine, listed = mpg_blocks(blob), mpg_blocks(again), mpg_blocks(built)
        good = sum(1 for k, block in enumerate(theirs)
                   if k < len(mine) and mine[k] == block and k < len(listed) and listed[k] == block)
        whole = again == blob and built == blob
        ok, failed = ok + good, failed + len(theirs) - good
        print(f"{start:#010x}..{end:#010x}  {listing:22s} {good}/{len(theirs)} MPG blocks OK, {raws} raw, "
              f"packet {'same' if whole else 'DIFFERS'}")
        if not whole and good == len(theirs):
            failed += 1
    print(f"{ok} OK, {failed} failed")
    return failed


def main():
    a = sys.argv[1:]
    if a and a[0] == "--disasm":
        if len(a) < 4:
            raise SystemExit(__doc__)
        data = Path(a[1]).read_bytes()[int(a[2], 0):int(a[3], 0)]
        text, raws = disassemble(data, a[4] if len(a) > 4 else "")
        sys.stdout.write(text)
        print(f"{raws} raw words", file=sys.stderr)
    elif len(a) >= 3 and a[0] == "-o":
        Path(a[1]).parent.mkdir(parents=True, exist_ok=True)
        Path(a[1]).write_bytes(b"".join(assemble(p) for p in a[2:]))
    elif not a or a == ["--check"]:
        sys.exit(1 if check() else 0)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
