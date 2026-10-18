#!/usr/bin/env python3
"""Read IOP modules (.IRX): module header, import/export tables, strings, annotated R3000 disassembly.

Usage: .venv/bin/python scripts/irx.py COMMAND FILE [ARGS]

  info FILE                 .iopmod header (name, version, entry, gp, sizes), sections, imports, exports
  strings FILE [MIN]        strings in .rodata/.data with their addresses (default MIN 4)
  dis FILE [START [END]]    objdump -m mips:3000 of .text, calls to import stubs and lui/lo string or data
                            references annotated (addresses are module-relative, as loaded at 0)

An IRX is a relocatable MIPS ELF; its imports are stub tables in .text (magic 0x41E00000, library name, then
`jr $ra; addiu $zero, $zero, N` per function N) and its exports are tables of function pointers (magic
0x41C00000). Import names come from the standard IOP library export order (ordinal -> name); an ordinal not
in the table prints as `library_N`.
"""

import re
import struct
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OBJDUMP = ROOT / "tools" / "binutils" / "mips-ps2-decompals-objdump"

IMPORT_MAGIC = 0x41E00000
EXPORT_MAGIC = 0x41C00000
JR_RA = 0x03E00008

# Standard IOP library exports by ordinal (0-3 are start/reinit/stop/query in every library).
KNOWN = {
    "cdvdman": {4: "sceCdInit", 5: "sceCdStandby", 6: "sceCdRead", 7: "sceCdSeek", 8: "sceCdGetError",
                9: "sceCdGetToc", 10: "sceCdSearchFile", 11: "sceCdSync", 12: "sceCdGetDiskType",
                13: "sceCdDiskReady", 14: "sceCdTrayReq", 15: "sceCdStop", 16: "sceCdPosToInt",
                17: "sceCdIntToPos", 24: "sceCdReadClock", 28: "sceCdStatus", 37: "sceCdCallback",
                38: "sceCdPause", 39: "sceCdBreak", 56: "sceCdStInit", 57: "sceCdStRead", 58: "sceCdStSeek",
                59: "sceCdStStart", 60: "sceCdStStat", 61: "sceCdStStop", 75: "sceCdMmode"},
    "sysmem": {4: "AllocSysMemory", 5: "FreeSysMemory", 6: "QueryMemSize", 7: "QueryMaxFreeMemSize",
               8: "QueryTotalFreeMemSize", 9: "QueryBlockTopAddress", 10: "QueryBlockSize", 14: "Kprintf"},
    "loadcore": {3: "GetLibraryEntryTable", 4: "FlushIcache", 5: "FlushDcache", 6: "RegisterLibraryEntries",
                 7: "ReleaseLibraryEntries"},
    "intrman": {4: "RegisterIntrHandler", 5: "ReleaseIntrHandler", 6: "EnableIntr", 7: "DisableIntr",
                8: "CpuDisableIntr", 9: "CpuEnableIntr", 17: "CpuSuspendIntr", 18: "CpuResumeIntr",
                23: "QueryIntrContext"},
    "sifman": {5: "sceSifInit", 6: "sceSifSetDChain", 7: "sceSifSetDma", 8: "sceSifDmaStat",
               29: "sceSifCheckInit"},
    "sifcmd": {4: "sceSifInitCmd", 5: "sceSifExitCmd", 6: "sceSifGetSreg", 7: "sceSifSetSreg",
               8: "sceSifSetCmdBuffer", 10: "sceSifAddCmdHandler", 11: "sceSifRemoveCmdHandler",
               12: "sceSifSendCmd", 13: "isceSifSendCmd", 14: "sceSifInitRpc", 15: "sceSifBindRpc",
               16: "sceSifCallRpc", 17: "sceSifRegisterRpc", 18: "sceSifCheckStatRpc", 19: "sceSifSetRpcQueue",
               20: "sceSifGetNextRequest", 21: "sceSifExecRequest", 22: "sceSifRpcLoop",
               23: "sceSifGetOtherData", 24: "sceSifRemoveRpc", 25: "sceSifRemoveRpcQueue"},
    "stdio": {4: "printf"},
    "thbase": {4: "CreateThread", 5: "DeleteThread", 6: "StartThread", 7: "StartThreadArgs", 8: "ExitThread",
               9: "ExitDeleteThread", 10: "TerminateThread", 11: "iTerminateThread", 12: "DisableDispatchThread",
               13: "EnableDispatchThread", 14: "ChangeThreadPriority", 15: "iChangeThreadPriority",
               16: "RotateThreadReadyQueue", 17: "iRotateThreadReadyQueue", 18: "ReleaseWaitThread",
               19: "iReleaseWaitThread", 20: "GetThreadId", 22: "ReferThreadStatus", 23: "iReferThreadStatus",
               24: "SleepThread", 25: "WakeupThread", 26: "iWakeupThread", 27: "CancelWakeupThread",
               28: "iCancelWakeupThread", 29: "SuspendThread", 30: "iSuspendThread", 31: "ResumeThread",
               32: "iResumeThread", 33: "DelayThread", 34: "GetSystemTime", 35: "SetAlarm", 36: "iSetAlarm",
               37: "CancelAlarm", 38: "iCancelAlarm", 39: "USec2SysClock", 40: "SysClock2USec"},
    "thsemap": {4: "CreateSema", 5: "DeleteSema", 6: "SignalSema", 7: "iSignalSema", 8: "WaitSema",
                9: "PollSema", 10: "ReferSemaStatus", 11: "iReferSemaStatus"},
    "thevent": {4: "CreateEventFlag", 5: "DeleteEventFlag", 6: "SetEventFlag", 7: "iSetEventFlag",
                8: "ClearEventFlag", 9: "iClearEventFlag", 10: "WaitEventFlag", 11: "PollEventFlag"},
    "timrman": {4: "AllocHardTimer", 5: "ReferHardTimer", 6: "FreeHardTimer", 7: "SetTimerMode",
                8: "GetTimerStatus", 9: "SetTimerCounter", 10: "GetTimerCounter", 11: "SetTimerCompare",
                12: "GetTimerCompare", 13: "SetHoldMode", 14: "GetHoldMode", 15: "GetHoldReg",
                16: "GetHardTimerIntrCode"},
    "sio2man": {},
    "ioman": {4: "open", 5: "close", 6: "read", 7: "write", 8: "lseek", 9: "ioctl", 10: "remove", 11: "mkdir",
              12: "rmdir", 13: "dopen", 14: "dclose", 15: "dread", 16: "getstat", 17: "chstat", 20: "AddDrv",
              21: "DelDrv"},
}


@dataclass
class Section:
    name: str
    type: int
    addr: int
    offset: int
    size: int


@dataclass
class IopMod:
    entry: int
    gp: int
    text_size: int
    data_size: int
    bss_size: int
    version: int
    name: str


@dataclass
class Library:
    name: str
    version: int
    addr: int
    stubs: list = field(default_factory=list)  # (stub address, ordinal)
    funcs: list = field(default_factory=list)  # export tables: function addresses


def sections(data):
    """Section headers of an ELF32 little-endian file, by name."""
    if len(data) < 52 or data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ValueError("not an ELF32 little-endian file")
    shoff, = struct.unpack_from("<I", data, 32)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 46)
    hdrs = [struct.unpack_from("<10I", data, shoff + i * shentsize) for i in range(shnum)]
    strtab = hdrs[shstrndx]
    names = data[strtab[4]:strtab[4] + strtab[5]]
    out = {}
    for h in hdrs[1:]:
        name = names[h[0]:names.index(b"\0", h[0])].decode("ascii", "replace")
        out[name] = Section(name, h[1], h[3], h[4], h[5])
    return out


def section_bytes(data, sec):
    return data[sec.offset:sec.offset + sec.size]


def iopmod(blob):
    """The .iopmod header: module-info pointer, entry, gp, text/data/bss sizes, version, name."""
    _, entry, gp, text, dat, bss = struct.unpack_from("<6I", blob, 0)
    version, = struct.unpack_from("<H", blob, 24)
    name = blob[26:].split(b"\0", 1)[0].decode("ascii", "replace")
    return IopMod(entry, gp, text, dat, bss, version, name)


def _word(blob, off):
    return struct.unpack_from("<I", blob, off)[0] if off + 4 <= len(blob) else None


def _header(blob, off):
    if off + 20 > len(blob):
        return None
    version, = struct.unpack_from("<H", blob, off + 8)
    name = blob[off + 12:off + 20].split(b"\0", 1)[0].decode("ascii", "replace")
    return version, name


def imports(blob, base=0):
    """Import stub tables in a .text image loaded at `base`."""
    libs = []
    for off in range(0, len(blob) - 3, 4):
        if _word(blob, off) != IMPORT_MAGIC or (hdr := _header(blob, off)) is None:
            continue
        lib = Library(hdr[1], hdr[0], base + off)
        pos = off + 20
        while (a := _word(blob, pos)) == JR_RA and (b := _word(blob, pos + 4)) is not None \
                and b >> 16 == 0x2400:
            lib.stubs.append((base + pos, b & 0xFFFF))
            pos += 8
        libs.append(lib)
    return libs


def exports(blob, base=0):
    """Export tables (function pointer lists, zero-terminated) in a .text image loaded at `base`."""
    libs = []
    for off in range(0, len(blob) - 3, 4):
        if _word(blob, off) != EXPORT_MAGIC or (hdr := _header(blob, off)) is None:
            continue
        lib = Library(hdr[1], hdr[0], base + off)
        pos = off + 20
        while (w := _word(blob, pos)) not in (None, 0):
            lib.funcs.append(w)
            pos += 4
        libs.append(lib)
    return libs


def import_name(lib, ordinal):
    return KNOWN.get(lib, {}).get(ordinal, f"{lib}_{ordinal}")


def stub_map(libs):
    return {addr: f"{lib.name}:{import_name(lib.name, n)}" for lib in libs for addr, n in lib.stubs}


def find_strings(blob, base=0, minlen=4):
    out = {}
    for m in re.finditer(rb"[\x20-\x7E\t\n]{%d,}\0" % minlen, blob):
        out[base + m.start()] = m.group()[:-1].decode("ascii")
    return out


_LINE = re.compile(r"^\s*([0-9a-f]+):\t([0-9a-f]{8}) \t(\S+)\t?(.*)$")
_IMM = re.compile(r"^(\w+),(\w+),(-?(?:0x[0-9a-f]+|\d+))$")
_MEM = re.compile(r"^(\w+),(-?\d+)\((\w+)\)$")
_TARGET = re.compile(r"(?:^|,)0x([0-9a-f]+)$")
_GPRS = {"at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "s0", "s1",
         "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "s8", "fp", "ra"}
_SAVED = {"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "fp", "gp", "sp"}
_NO_DEST = {"sw", "sh", "sb", "swl", "swr", "sd", "swc1", "swc2", "jr", "mult", "multu", "div", "divu", "mthi",
            "mtlo", "mtc0", "mtc1", "mtc2", "ctc1", "ctc2", "syscall", "break", "teq", "tne"}


def _note(target, strings):
    if target in strings:
        text = strings[target].replace("\n", "\\n").replace("\t", "\\t")
        return f'0x{target:x} "{text}"'
    return f"0x{target:x}"


def annotate(lines, stubs, strings):
    """Annotate objdump lines (straight-line register tracking, no control flow): calls to import stubs, and
    addresses built with `lui` + `addiu/ori` or used as `offset(reg)` by loads and stores."""
    targets = set()
    for line in lines:
        if (m := _LINE.match(line)) and m.group(3).startswith(("b", "j")) and (t := _TARGET.search(m.group(4))):
            targets.add(int(t.group(1), 16))
    val = {}
    clobber = False
    out = []
    for line in lines:
        m = _LINE.match(line)
        note = None
        if not m:
            out.append(line)
            continue
        if int(m.group(1), 16) in targets:
            val = {}
        op, args = m.group(3), m.group(4)
        dest = args.split(",", 1)[0]
        new = None
        if op in ("jal", "j") and args.startswith("0x") and int(args, 16) in stubs:
            note = stubs[int(args, 16)]
        elif op == "lui":
            new = int(args.split(",")[1], 16) << 16
        elif (imm := _IMM.match(args)) and op in ("addiu", "ori") and imm.group(2) in val:
            k = int(imm.group(3), 0)
            new = (val[imm.group(2)] | k) if op == "ori" else (val[imm.group(2)] + k) & 0xFFFFFFFF
            note = _note(new, strings)
        elif op in ("addu", "or") and args.endswith(",zero") and args.split(",")[1] in val:
            new = val[args.split(",")[1]]
        elif (mem := _MEM.match(args)) and mem.group(3) in val:
            note = _note((val[mem.group(3)] + int(mem.group(2))) & 0xFFFFFFFF, strings)
        if dest in _GPRS and op not in _NO_DEST and not op.startswith(("b", "j")):
            val.pop(dest, None)
            if new is not None:
                val[dest] = new
        if clobber:
            val = {r: v for r, v in val.items() if r in _SAVED}
        clobber = op in ("jal", "jalr")
        out.append(f"{line}\t# {note}" if note else line)
    return out


def load(path):
    data = Path(path).read_bytes()
    secs = sections(data)
    text = section_bytes(data, secs[".text"])
    strings = {}
    for name in (".rodata", ".data"):
        if name in secs:
            strings.update(find_strings(section_bytes(data, secs[name]), secs[name].addr))
    return data, secs, text, strings


def cmd_info(path):
    data, secs, text, _ = load(path)
    if ".iopmod" in secs:
        mod = iopmod(section_bytes(data, secs[".iopmod"]))
        print(f"module {mod.name} version {mod.version >> 8}.{mod.version & 0xFF:02x} entry 0x{mod.entry:x} "
              f"gp 0x{mod.gp:x} text 0x{mod.text_size:x} data 0x{mod.data_size:x} bss 0x{mod.bss_size:x}")
    for s in secs.values():
        if s.size and s.name.startswith("."):
            print(f"  {s.name:10} 0x{s.addr:06x} size 0x{s.size:x}")
    for lib in exports(text, secs[".text"].addr):
        print(f"export {lib.name} 0x{lib.version:x} at 0x{lib.addr:x}: "
              + " ".join(f"{i}=0x{f:x}" for i, f in enumerate(lib.funcs)))
    for lib in imports(text, secs[".text"].addr):
        print(f"import {lib.name} 0x{lib.version:x}: "
              + ", ".join(f"{n} {import_name(lib.name, n)}@0x{a:x}" for a, n in lib.stubs))


def cmd_strings(path, minlen=4):
    data, secs, _, _ = load(path)
    for name in (".rodata", ".data"):
        if name in secs:
            for addr, s in find_strings(section_bytes(data, secs[name]), secs[name].addr, minlen).items():
                print(f"0x{addr:05x} {s!r}")


def cmd_dis(path, start=None, end=None):
    _, secs, text, strings = load(path)
    args = [str(OBJDUMP), "-d", "-m", "mips:3000", "-M", "no-aliases", "-j", ".text"]
    if start is not None:
        args.append(f"--start-address={start:#x}")
    if end is not None:
        args.append(f"--stop-address={end:#x}")
    lines = subprocess.run(args + [str(path)], check=True, capture_output=True, text=True).stdout.splitlines()
    stubs = stub_map(imports(text, secs[".text"].addr))
    print("\n".join(annotate(lines, stubs, strings)))


def main(argv):
    if len(argv) < 3 or argv[1] not in ("info", "strings", "dis"):
        print(__doc__.strip(), file=sys.stderr)
        return 2
    cmd, path = argv[1], argv[2]
    try:
        if cmd == "info":
            cmd_info(path)
        elif cmd == "strings":
            cmd_strings(path, int(argv[3]) if len(argv) > 3 else 4)
        else:
            nums = [int(a, 16) for a in argv[3:5]]
            cmd_dis(path, *nums)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        print(f"irx.py: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
