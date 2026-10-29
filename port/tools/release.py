#!/usr/bin/env python3
"""Cuts a player release of the PC port (T44, docs/port/release.md): builds the programs, packages them with the
translations, the licences of the vendored code, a player README and the documentation, checks that no archive holds
any game data, and writes the archives and SHA256SUMS to dist/. Publishes nothing (no upload, no tag, no push).

    scripts/cool.sh python3 port/tools/release.py              build both targets, package, check -> dist/
    python3 port/tools/release.py --no-build                   package the programs already built
    python3 port/tools/release.py --targets linux              one target (linux, windows)
    python3 port/tools/release.py --check ARCHIVE...           the content check alone (exit 1 on a problem)
    python3 port/tools/release.py --portability                the Linux / Windows portability report alone
    python3 port/tools/release.py --sdl3-dll PATH              ship this SDL3.dll in the Windows archive (CI release)

The version is `git describe --always --dirty` of the checkout, the same text the build embeds (TTT_GIT: `ttt
--version`, the crash report, the overlay's About); a tree with local changes is refused unless --allow-dirty.

Archives: dist/ttt-<version>-linux-x86_64.tar.gz (the programs keep their execute bits) and
dist/ttt-<version>-windows-x86_64.zip, the same bytes for the same programs (sorted entries, the commit's time, no
owner names). The game is never in them: an allowlist of names, a list of forbidden names (disc images, the game's
executable, data files, saves, dumps, keys), the sizes and SHA-1 of the game's files from port/tools/data_layout.txt,
and, when the game's files are on this machine, 64-byte samples of them searched for in every archived file.

SDL3.dll is shipped in the Windows archive only when --sdl3-dll (or TTT_SDL3_DLL) names one: the CI release builds it
from SDL's source (port/release/Dockerfile.ci). Its zlib licence comes from --sdl3-license (TTT_SDL3_LICENSE), or is
looked for beside the DLL (../share/licenses/SDL3/LICENSE.txt, LICENSE.txt); without a DLL the archive is as before."""

import argparse
import fnmatch
import gzip
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RELEASE = ROOT / "port/release"
LAYOUT = ROOT / "port/tools/data_layout.txt"
MINGW = "x86_64-w64-mingw32"

# The oldest C library the Linux programs may need (docs/port/release.md "Portability"): glibc 2.34 merged libpthread
# and libdl into libc and gave __libc_start_main a new version, so a program linked on a newer system needs 2.34 at
# least; newer symbol versions than this fail the release. BASELINE is the older system the report compares with.
MAX_GLIBC = (2, 34)
BASELINE_GLIBC = (2, 31)
LINUX_LIBS = {"libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "ld-linux-x86-64.so.2"}
# Windows: the system's DLLs and the Universal C Runtime only (winpthreads and libgcc are linked in; SDL3.dll is
# loaded at run time and is not an import).
WINDOWS_DLLS = {"kernel32.dll", "user32.dll", "shell32.dll", "ws2_32.dll", "bcrypt.dll", "advapi32.dll"}

# Documentation for players, bundled as docs/<name>.md (the rest of docs/port is about the decompilation).
DOCS = ("install.md", "settings.md", "input.md", "video.md", "sound.md", "netplay.md", "matchmaking.md", "mods.md",
        "textures.md", "translations.md", "accessibility.md", "cheats.md")
# The vendored code's licences (port/release/LICENSES.txt lists what each is used for).
LICENSES = {"imgui.txt": "third_party/imgui/LICENSE.txt", "pl_mpeg2.txt": "third_party/pl_mpeg2/LICENSE",
            "stb.txt": "third_party/stb/LICENSE", "xxhash.txt": "third_party/xxhash/LICENSE",
            "tweetnacl.txt": "third_party/tweetnacl/LICENSE", "newlib.txt": "third_party/newlib_libm/COPYING.NEWLIB"}
# The port's own code (the repository's LICENSE), bundled beside the vendored code's licences.
PORT_LICENSE = "ttt.txt"
PORT_LICENSE_NOTE = ("\nThe port's own code (the programs' platform layer, the matchmaking server and\n"
                     "the notice signer) is licensed under the GNU Affero General Public License\n"
                     "v3.0 only: ttt.txt.\n")
# Linked into the Windows programs from the cross compiler: winpthreads and the mingw-w64 run time (notice required),
# libgcc (GCC Runtime Library Exception). Looked for where distributions install them.
WINDOWS_LICENSES = {
    "mingw-w64-runtime.txt": ("/usr/share/licenses/mingw-w64-winpthreads/COPYING.MinGW-w64-runtime.txt",
                              "/usr/share/licenses/mingw-w64-crt/COPYING.MinGW-w64-runtime.txt",
                              "/usr/share/doc/mingw-w64-common/copyright",
                              f"/usr/{MINGW}/share/licenses/winpthreads/COPYING"),
    "gcc-runtime-library-exception.txt": ("/usr/share/licenses/libgcc/RUNTIME.LIBRARY.EXCEPTION",
                                          "/usr/share/licenses/gcc/RUNTIME.LIBRARY.EXCEPTION",
                                          "/usr/share/doc/gcc-mingw-w64-base/copyright"),
}
WINDOWS_ROWS = (
    "  mingw-w64 run time, winpthreads  MIT-style / ZPL (see file)  mingw-w64-runtime.txt       ttt.exe, ttt-*.exe\n"
    "  libgcc                          GPL + runtime exception     gcc-runtime-library-exception.txt  ttt.exe, ttt-*.exe\n")

# SDL3.dll in the Windows archive (--sdl3-dll): its name below the top folder, its licence file, and the paragraph of port/release/LICENSES.txt that says SDL 3 is not part of the release, replaced.
SDL3_DLL = "SDL3.dll"
SDL3_LICENSE = "sdl3.txt"
SDL3_ABSENT = re.compile(r"SDL 3 is not part of this release:.*?\n\n", re.S)
SDL3_PRESENT = ("SDL3.dll is {name}, built unmodified from its source release; the program\n"
                "loads it from beside ttt.exe when it opens its window.\n\n")
# The readme's SDL3.dll requirement (port/release/README.windows.txt, {sdl3_dll}).
SDL3_README_ABSENT = (
    "- SDL3.dll - NOT included. Download SDL 3's official Windows release\n"
    "  (SDL3-<version>-win32-x64.zip) from https://github.com/libsdl-org/SDL/releases\n"
    "  and put the SDL3.dll from it next to ttt.exe. Without it the program runs\n"
    "  without a window.")
SDL3_README_PRESENT = (
    "- SDL3.dll - included in this archive, next to ttt.exe ({name}, zlib\n"
    "  licence: LICENSES\\sdl3.txt). Keep it beside ttt.exe.")

TARGETS = {
    "linux": {
        "suffix": "linux-x86_64", "ext": ".tar.gz", "readme": "README.linux.txt", "newline": "\n", "sdl": "libSDL3.so.0",
        # archive name -> source, and whether it is a program (stripped of its debug sections)
        "files": {"ttt": ("build/port/ttt", True), "ttt.syms": ("build/port/ttt.syms", False),
                  "ttt-server": ("build/server/ttt-server", True), "ttt-sign": ("build/server/ttt-sign", True)},
        "objcopy": "objcopy",
    },
    "windows": {
        "suffix": "windows-x86_64", "ext": ".zip", "readme": "README.windows.txt", "newline": "\r\n", "sdl": "SDL3.dll",
        "files": {"ttt.exe": ("build/port-win64/ttt.exe", True), "ttt.exe.syms": ("build/port-win64/ttt.exe.syms", False),
                  "ttt-server.exe": ("build/server-win64/ttt-server.exe", True),
                  "ttt-sign.exe": ("build/server-win64/ttt-sign.exe", True)},
        "objcopy": f"{MINGW}-objcopy",
    },
}

# Names an archive may hold (below its top folder), and names it never may (any folder, case ignored).
ALLOWED = ("README.txt", "VERSION.txt", "CHANGELOG.txt", "lang/*.json", "LICENSES/*.txt", "docs/*.md")
FORBIDDEN = ("*.iso", "*.bin", "*.cue", "*.img", "*.mdf", "*.mds", "*.nrg", "*.chd", "*.cso", "*.ecm", "*.elf",
             "slus_*", "sces_*", "scps_*", "*.irx", "system.cnf", "ttt.dat*", "*.disc", "*.mcd", "*.ps2", "*.vmc",
             "*.key", "settings.ini", "portable.txt", "*.ppm", "*.png", "*.bmp", "*.tga", "*.dds", "*.wav", "*.ogg",
             "*.vag", "*.mp3", "*.raw", "*.pss", "*.ipu", "*.m2v", "*.state", "*.rec", "*.dll", "*.so", "*.so.*")
FORBIDDEN_DIRS = ("saves", "textures", "mods", "crash", "iso", "frames", "build", "replacements")


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, **kw)


def version():
    """The checkout's `git describe --always --dirty` (port/tools/undefined.py git_id: the build's TTT_GIT)."""
    r = run(["git", "describe", "--always", "--dirty"])
    if r.returncode != 0 or not r.stdout.strip():
        sys.exit("release: not a git checkout (git describe failed)")
    return r.stdout.strip()


def commit_time():
    """The commit's time (SOURCE_DATE_EPOCH wins): the archives' timestamps, so the same programs give the same bytes."""
    if os.environ.get("SOURCE_DATE_EPOCH"):
        return int(os.environ["SOURCE_DATE_EPOCH"])
    return int(run(["git", "log", "-1", "--format=%ct"]).stdout.strip())


# ---------------------------------------------------------------------------------------------------------- build


def build(targets):
    """The programs of each target, through the repository's build scripts (heavy: run release.py under cool.sh)."""
    steps = []
    if "linux" in targets:
        steps += [({}, ["port/tools/build.py"]), ({}, ["port/tools/build_server.py"])]
    if "windows" in targets:
        steps += [({"TTT_CC": "win64"}, ["port/tools/build.py"]), ({}, ["port/tools/build_server.py", "--win64"])]
    for env, args in steps:
        e = {k: v for k, v in os.environ.items() if k != "TTT_CC"}
        e.update(env)
        print(f"release: building: {' '.join([f'{k}={v}' for k, v in env.items()] + args)}", flush=True)
        r = subprocess.run([sys.executable, *args], cwd=ROOT, env=e, capture_output=True, text=True)
        if r.returncode != 0:
            sys.stderr.write((r.stdout + r.stderr)[-4000:])
            sys.exit(f"release: build failed: {' '.join(args)}")


def check_embedded_version(target, ver):
    """The game program must be built from this source: its embedded TTT_GIT is the version."""
    exe = ROOT / TARGETS[target]["files"]["ttt.exe" if target == "windows" else "ttt"][0]
    if not exe.exists():
        sys.exit(f"release: {exe.relative_to(ROOT)} is not built (drop --no-build, or build it)")
    if ver.encode() + b"\0" not in exe.read_bytes():
        sys.exit(f"release: {exe.relative_to(ROOT)} was not built from {ver} (rebuild, or drop --no-build)")


# --------------------------------------------------------------------------------------------------- portability


def glibc_report(path):
    """{'needed': [...], 'versions': {version: [symbols]}} of a Linux ELF program."""
    needed = re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", run(["readelf", "-d", str(path)]).stdout)
    versions = {}
    for line in run(["objdump", "-T", str(path)]).stdout.splitlines():
        m = re.search(r"\((GLIBC_[0-9.]+)\)\s+(\S+)$", line) or re.search(r"\s(GLIBC_[0-9.]+)\s+(\S+)$", line)
        if m and "*UND*" in line:
            versions.setdefault(m.group(1), []).append(m.group(2))
    return {"needed": needed, "versions": versions}


def glibc_tuple(v):
    return tuple(int(x) for x in v.split("_")[1].split("."))


def pe_imports(path):
    return re.findall(r"DLL Name: (\S+)", run([f"{MINGW}-objdump", "-p", str(path)]).stdout)


def portability(paths):
    """The report's lines and its problems (a library or symbol version beyond what the release allows)."""
    lines, problems = [], []
    for p in paths:
        name = p.name
        if p.suffix == ".exe":
            dlls = pe_imports(p)
            bad = [d for d in dlls if d.lower() not in WINDOWS_DLLS and not d.lower().startswith("api-ms-win-crt-")]
            lines.append(f"{name}: imports {', '.join(dlls)}")
            if not dlls:
                problems.append(f"{name}: no imports read ({MINGW}-objdump missing?)")
            problems += [f"{name}: imports {d} (not a system DLL)" for d in bad]
            continue
        rep = glibc_report(p)
        if not rep["needed"] or not rep["versions"]:
            problems.append(f"{name}: no libraries or symbol versions read (readelf / objdump missing?)")
        vers = sorted(rep["versions"], key=glibc_tuple)
        top = vers[-1] if vers else "none"
        lines.append(f"{name}: needs {', '.join(rep['needed'])}; newest symbol version {top}")
        for v in vers:
            if glibc_tuple(v) > BASELINE_GLIBC:
                lines.append(f"  {v} (newer than glibc {'.'.join(map(str, BASELINE_GLIBC))}): "
                             f"{', '.join(sorted(set(rep['versions'][v])))}")
            if glibc_tuple(v) > MAX_GLIBC:
                problems.append(f"{name}: needs {v} ({', '.join(sorted(set(rep['versions'][v])))}); the release allows "
                                f"glibc {'.'.join(map(str, MAX_GLIBC))}")
        problems += [f"{name}: needs {lib} (not part of glibc)" for lib in rep["needed"] if lib not in LINUX_LIBS]
    return lines, problems


# ------------------------------------------------------------------------------------------------------- staging


def changelog(ver):
    """CHANGELOG.txt: the port's commits since the previous tag (all of them before the first release)."""
    r = run(["git", "describe", "--tags", "--abbrev=0", "HEAD^"])
    prev = r.stdout.strip() if r.returncode == 0 else ""
    log = run(["git", "log", "--reverse", "--date=short", "--format=%ad %s", f"{prev}..HEAD" if prev else "HEAD", "--",
               "port", "docs/port"]).stdout
    keep = [l for l in log.splitlines() if re.match(r"\S+ (feat|fix|perf)(\(port[^)]*\))?!?: ", l)]
    head = f"Changes since {prev}" if prev else "The PC port's changes up to this release"
    out = [f"Tekken Tag Tournament PC port {ver}", "", head + ":", ""]
    out += [f"- {l}" for l in keep] or ["- (no port changes)"]
    return "\n".join(out) + "\n"


def sdl3_license(dll, given):
    """The licence file of the SDL3.dll being shipped: `given`, or the one installed beside it."""
    if given:
        cands = [Path(given)]
    else:
        cands = [dll.parent.parent / "share/licenses/SDL3/LICENSE.txt", dll.parent / "LICENSE.txt",
                 dll.parent / "LICENSE.SDL.txt"]
    found = next((c for c in cands if c.is_file()), None)
    if found is None:
        sys.exit(f"release: the licence of {dll} is not found ({cands[0]}; pass --sdl3-license)")
    return found


def stage(target, ver, top, sdl3=None):
    """Writes the target's files under `top` (the archive's top folder); returns the archive names. `sdl3`: (dll,
    licence, version text) of an SDL3.dll to ship in the Windows archive, or None."""
    t = TARGETS[target]
    nl = t["newline"]
    top.mkdir(parents=True)
    for name, (src, program) in t["files"].items():
        src = ROOT / src
        if not src.exists():
            sys.exit(f"release: {src.relative_to(ROOT)} is missing (build it, or drop --no-build)")
        if program:  # the debug sections hold the checkout's paths; the symbol table stays (plat_state.c, ee_diff.c)
            r = run([t["objcopy"], "--strip-debug", str(src), str(top / name)])
            if r.returncode != 0:
                sys.exit(f"release: {t['objcopy']} --strip-debug {src.name}: {r.stderr.strip()}")
            if name.endswith(".exe"):
                pe_stamp(top / name, commit_time())
            os.chmod(top / name, 0o755)
        else:
            shutil.copyfile(src, top / name)
    (top / "lang").mkdir()
    for f in sorted((ROOT / "port/lang").glob("*.json")):
        shutil.copyfile(f, top / "lang" / f.name)
    (top / "docs").mkdir()
    for d in DOCS:
        shutil.copyfile(ROOT / "docs/port" / d, top / "docs" / d)
    lic = top / "LICENSES"
    lic.mkdir()
    for name, src in LICENSES.items():
        shutil.copyfile(ROOT / src, lic / name)
    shutil.copyfile(ROOT / "LICENSE", lic / PORT_LICENSE)
    rows = ""
    if target == "windows":
        for name, choices in WINDOWS_LICENSES.items():
            found = next((Path(c) for c in choices if Path(c).is_file()), None)
            if found is None:
                sys.exit(f"release: the licence {name} of the cross compiler's run time is not found ({choices[0]})")
            shutil.copyfile(found, lic / name)
        rows = WINDOWS_ROWS
    index = (RELEASE / "LICENSES.txt").read_text()
    readme = (RELEASE / t["readme"]).read_text().replace("{version}", ver)
    if target == "windows" and sdl3:
        dll, dll_lic, sdl_ver = sdl3
        shutil.copyfile(dll, top / SDL3_DLL)
        shutil.copyfile(dll_lic, lic / SDL3_LICENSE)
        name = f"SDL {sdl_ver}" if sdl_ver else "SDL 3"
        rows += ("  " + f"SDL 3{f' ({sdl_ver})' if sdl_ver else ''}".ljust(32) + "zlib".ljust(28) + SDL3_LICENSE.ljust(28)
                 + "SDL3.dll (window, sound, input)\n")
        index, n = SDL3_ABSENT.subn(SDL3_PRESENT.format(name=name), index)
        if n != 1:
            sys.exit("release: port/release/LICENSES.txt no longer has the paragraph about SDL 3 to replace")
        readme = readme.replace("{sdl3_dll}", SDL3_README_PRESENT.format(name=name))
    else:
        readme = readme.replace("{sdl3_dll}", SDL3_README_ABSENT)
    index = index.replace("{windows_rows}\n", rows).replace("{sdl}", t["sdl"]) + PORT_LICENSE_NOTE
    write_text(lic / "README.txt", index, nl)
    write_text(top / "README.txt", readme, nl)
    commit = run(["git", "rev-parse", "HEAD"]).stdout.strip()
    when = time.strftime("%Y-%m-%d", time.gmtime(commit_time()))
    write_text(top / "VERSION.txt", f"Tekken Tag Tournament PC port\nversion: {ver}\ntarget: {t['suffix']}\n"
               f"source: commit {commit} ({when})\n", nl)
    write_text(top / "CHANGELOG.txt", changelog(ver), nl)
    return sorted(str(p.relative_to(top)) for p in top.rglob("*") if p.is_file())


def pe_stamp(path, when):
    """A PE program's link time (COFF TimeDateStamp, which objcopy sets to now) set to `when`, and its checksum
    recomputed: the same program gives the same bytes."""
    b = bytearray(path.read_bytes())
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    if b[pe:pe + 4] != b"PE\0\0":
        sys.exit(f"release: {path.name}: not a PE program")
    struct.pack_into("<I", b, pe + 8, when & 0xFFFFFFFF)
    sum_at = pe + 24 + 64  # the optional header's CheckSum
    struct.pack_into("<I", b, sum_at, 0)
    s = 0
    for i in range(0, len(b) - len(b) % 2, 2):
        s += b[i] | b[i + 1] << 8
        s = (s & 0xFFFF) + (s >> 16)
    if len(b) % 2:
        s += b[-1]
    s = (s & 0xFFFF) + (s >> 16)
    struct.pack_into("<I", b, sum_at, ((s + (s >> 16)) & 0xFFFF) + len(b))
    path.write_bytes(bytes(b))


def write_text(path, text, nl):
    path.write_bytes(text.replace("\r\n", "\n").replace("\n", nl).encode("utf-8"))


# ------------------------------------------------------------------------------------------------------ archives


def archive(target, ver, top, out):
    """The archive of `top` (its folder name as the top entry), reproducible; its path."""
    t = TARGETS[target]
    path = out / f"{top.name}{t['ext']}"
    when = commit_time()
    entries = sorted(p for p in top.rglob("*"))
    if t["ext"] == ".tar.gz":
        with open(path, "wb") as f, gzip.GzipFile(filename="", mode="wb", fileobj=f, mtime=0, compresslevel=9) as gz:
            with tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as tar:
                for p in [top, *entries]:
                    info = tar.gettarinfo(str(p), arcname=str(p.relative_to(top.parent)))
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mtime = when
                    info.mode = 0o755 if p.is_dir() or os.access(p, os.X_OK) else 0o644
                    if p.is_file():
                        with open(p, "rb") as fh:
                            tar.addfile(info, fh)
                    else:
                        tar.addfile(info)
    else:
        stamp = time.gmtime(max(when, 315532800))[:6]  # zip times start in 1980
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for p in [top, *entries]:
                name = str(p.relative_to(top.parent)) + ("/" if p.is_dir() else "")
                info = zipfile.ZipInfo(name, stamp)
                info.create_system = 3
                info.external_attr = ((0o40755 if p.is_dir() else 0o100755 if os.access(p, os.X_OK) else 0o100644)
                                      << 16)
                info.compress_type = zipfile.ZIP_DEFLATED
                z.writestr(info, b"" if p.is_dir() else p.read_bytes())
    return path


def members(path):
    """[(name, bytes, regular)] of the entries in an archive (.tar.gz or .zip) other than folders; `regular` is False
    for a link or a device (refused by check)."""
    out = []
    if str(path).endswith(".zip"):
        with zipfile.ZipFile(path) as z:
            for i in z.infolist():
                kind = (i.external_attr >> 16) & 0o170000
                if not i.is_dir():
                    out.append((i.filename, z.read(i), kind in (0, 0o100000)))
    else:
        with tarfile.open(path, "r:gz") as tar:
            for i in tar.getmembers():
                if i.isfile():
                    out.append((i.name, tar.extractfile(i).read(), True))
                elif not i.isdir():
                    out.append((i.name, b"", False))
    return out


# ------------------------------------------------------------------------------------------- no game data inside


def layout_files():
    """[(what, size, sha1)] of the game's files named in data_layout.txt (no byte of the game is in it)."""
    out = []
    for line in LAYOUT.read_text().splitlines():
        w = line.split()
        if len(w) == 4 and w[0] == "elf":
            out.append((w[1], int(w[2]), w[3]))
        elif len(w) == 3 and w[0] == "payload":
            out.append(("ttt.dat payload", int(w[1], 16), w[2]))
    return out


def game_files():
    """The game's files on this machine, if any (to sample): the disc (TTT_DISC, iso/), the build's copy of the
    executable, the data files the program made."""
    disc = [Path(os.environ["TTT_DISC"])] if os.environ.get("TTT_DISC") else []
    out = []
    for d in disc + [ROOT / "iso"]:
        for n in ("SLUS_200.01", "TEKKEN.BIN", "IRX/RSPU2DRV.IRX"):
            if (d / n).is_file():
                out.append(d / n)
    data = Path(os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share") / "ttt/ttt.dat"
    for p in (ROOT / "build/SLUS_200.01", ROOT / "build/port/ttt.dat", data):
        if p.is_file():
            out.append(p)
    return list(dict.fromkeys(out))


SAMPLE = 64            # bytes in a sample
DISTINCT = 24          # a sample needs this many different byte values (no padding, no runs, no simple tables)
DENSE = 16 << 20       # files up to this size (the executable, the data file) are sampled every STEP bytes
STEP = 256
SAMPLES = 4096         # larger ones (TEKKEN.BIN) at this many places


def vendored_tables():
    """Constant tables of the C library code the game's executable links (newlib) and the programs link too: the
    vendored third_party/newlib_libm's (fdlibm's npio2_hw, two_over_pi, PIo2...) as 32-bit little-endian words, and the
    powers of ten of number conversion. The game's data holds the same tables as the programs: a sample inside one of
    them is the open-source library's, not the game's (docs/port/release.md)."""
    out = []
    for src in sorted((ROOT / "third_party/newlib_libm").glob("*.c")):
        text = re.sub(r"^\s*#.*$", "", src.read_text(errors="replace"), flags=re.M)  # no preprocessor lines
        for body in re.findall(r"\[\s*\w*\s*\]\s*=\s*\{([^{}]*)\}", text):
            words = []
            for tok in re.sub(r"/\*.*?\*/", "", body, flags=re.S).split(","):
                tok = tok.strip().rstrip("uUlLfF")
                if not tok:
                    continue
                try:
                    words.append(struct.pack("<I", int(tok, 0) & 0xFFFFFFFF))
                except ValueError:
                    try:
                        words.append(struct.pack("<f", float(tok)))
                    except ValueError:
                        words = []
                        break
            if len(words) >= 4:
                out.append(b"".join(words))
    # Every C library's number conversion (newlib's and mingw-w64's strtod / printf: mprec.c, gdtoa) holds the exact
    # powers of ten as doubles: tens 1e0..1e22, bigtens 1e16..1e256, tinytens 1e-16..1e-256.
    out.append(struct.pack("<23d", *[10.0 ** i for i in range(23)]))
    out.append(struct.pack("<5d", 1e16, 1e32, 1e64, 1e128, 1e256))
    out.append(struct.pack("<5d", 1e-16, 1e-32, 1e-64, 1e-128, 1e-256))
    return out


def samples(path, tables=None):
    """Windows of SAMPLE bytes spread over the file (every STEP bytes, or SAMPLES of them), each varied enough to be the file's own (and not
    inside a vendored library table, vendored_tables)."""
    tables = vendored_tables() if tables is None else tables
    size = path.stat().st_size
    step = STEP if size <= DENSE else max(SAMPLE, size // SAMPLES)
    out = []
    with open(path, "rb") as f:
        for off in range(0, size - SAMPLE + 1, step):
            f.seek(off)
            w = f.read(SAMPLE)
            if len(w) == SAMPLE and len(set(w)) >= DISTINCT and not any(w in tb for tb in tables):
                out.append((off, w))
    return out


def check(path, sampled=None):
    """Problems of an archive: a name outside the allowlist or on the forbidden list, a game file by size and hash,
    a sample of the game's files inside a member, the checkout's path inside a member. [] when clean."""
    problems = []
    mem = members(path)
    tops = {n.split("/", 1)[0] for n, _, _ in mem}
    if len(tops) != 1:
        problems.append(f"{path.name}: not one top folder ({sorted(tops)})")
    target = "windows" if str(path).endswith(".zip") else "linux"
    allowed = ALLOWED + tuple(TARGETS[target]["files"]) + ((SDL3_DLL,) if target == "windows" else ())
    known = {(size, sha) for _, size, sha in layout_files()}
    sizes = {size for size, _ in known}
    root = str(ROOT).encode()
    for name, data, regular in mem:
        rel = name.split("/", 1)[1] if "/" in name else name
        if not regular:
            problems.append(f"{name}: not a regular file (a link or a device)")
        if name.startswith("/") or ".." in name.split("/"):
            problems.append(f"{name}: a path outside the archive's folder")
        low = rel.lower()
        parts = low.split("/")
        if not any(fnmatch.fnmatchcase(rel, a) and rel.count("/") == a.count("/") for a in allowed):
            problems.append(f"{name}: not an allowed file")
        sdl3 = target == "windows" and rel == SDL3_DLL  # the one DLL a Windows archive may hold (--sdl3-dll)
        if (not sdl3 and any(fnmatch.fnmatchcase(parts[-1], f) for f in FORBIDDEN)) or \
                any(p in FORBIDDEN_DIRS for p in parts[:-1]):
            problems.append(f"{name}: a forbidden name (game data, saves, dumps or keys)")
        if len(data) in sizes and (len(data), hashlib.sha1(data).hexdigest()) in known:
            problems.append(f"{name}: is a file of the game (SHA-1 in data_layout.txt)")
        if root in data:
            problems.append(f"{name}: holds the checkout's path {ROOT}")
    for gf, wins in (sampled if sampled is not None else sample_game_files()).items():
        size = gf.stat().st_size
        digest = None
        for name, data, _ in mem:
            if len(data) == size:
                digest = digest or hashlib.sha1(gf.read_bytes()).hexdigest()
                if hashlib.sha1(data).hexdigest() == digest:
                    problems.append(f"{name}: is {gf}")
            hits = [off for off, w in wins if w in data]
            if hits:
                problems.append(f"{name}: holds {len(hits)} of {len(wins)} samples of {gf} (first at offset "
                                f"0x{hits[0]:X})")
    return problems


def sample_game_files():
    tables = vendored_tables()
    return {gf: samples(gf, tables) for gf in game_files()}


# ------------------------------------------------------------------------------------------------------------ main


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_sums(out):
    """dist/SHA256SUMS: every archive in dist/ (sha256sum -c format)."""
    lines = [f"{sha256(p)}  {p.name}\n" for p in sorted(out.iterdir())
             if p.name.endswith((".tar.gz", ".zip")) and p.name.startswith("ttt-")]
    (out / "SHA256SUMS").write_text("".join(lines))
    return lines


def program_paths(targets):
    out = []
    for t in targets:
        out += [ROOT / src for _, (src, prog) in TARGETS[t]["files"].items() if prog]
    return [p for p in out if p.exists()]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--targets", default="linux,windows", help="comma-separated: linux, windows")
    ap.add_argument("--out", type=Path, default=ROOT / "dist")
    ap.add_argument("--no-build", action="store_true", help="package the programs already built")
    ap.add_argument("--allow-dirty", action="store_true", help="release a tree with local changes (testing)")
    ap.add_argument("--check", nargs="+", type=Path, metavar="ARCHIVE", help="only check these archives")
    ap.add_argument("--portability", action="store_true", help="only print the portability report")
    ap.add_argument("--sdl3-dll", type=Path, default=os.environ.get("TTT_SDL3_DLL") or None,
                    help="an SDL3.dll to ship in the Windows archive (default: TTT_SDL3_DLL; none)")
    ap.add_argument("--sdl3-license", type=Path, default=os.environ.get("TTT_SDL3_LICENSE") or None,
                    help="its licence file (default: TTT_SDL3_LICENSE, or the one installed beside the DLL)")
    ap.add_argument("--sdl3-version", default=os.environ.get("TTT_SDL3_VERSION", ""),
                    help="its version, named in the README and the licence index (default: TTT_SDL3_VERSION)")
    a = ap.parse_args()

    if a.check:
        wins = sample_game_files()
        print(f"release: game files sampled on this machine: {len(wins)} ({sum(map(len, wins.values()))} samples)")
        bad = 0
        for p in a.check:
            problems = check(p, wins)
            bad += len(problems)
            print(f"release: {p.name}: {'OK' if not problems else 'PROBLEMS'}")
            for pr in problems:
                print(f"  {pr}")
        return 1 if bad else 0

    targets = [t.strip() for t in a.targets.split(",") if t.strip()]
    for t in targets:
        if t not in TARGETS:
            sys.exit(f"release: unknown target {t} (linux, windows)")
    if a.portability:
        lines, problems = portability(program_paths(targets))
        print("\n".join(lines + [f"PROBLEM: {p}" for p in problems]))
        return 1 if problems else 0

    ver = version()
    if ver.endswith("-dirty") and not a.allow_dirty:
        sys.exit(f"release: the tree has local changes ({ver}); commit them, or pass --allow-dirty for a test")
    if not a.no_build:
        build(targets)
    for t in targets:
        check_embedded_version(t, ver)
    lines, problems = portability(program_paths(targets))
    print("release: portability:\n  " + "\n  ".join(lines))
    if problems:
        sys.exit("release: portability problems:\n  " + "\n  ".join(problems))

    sdl3 = None
    if a.sdl3_dll and "windows" in targets:
        dll = Path(a.sdl3_dll).resolve()
        if not dll.is_file() or dll.read_bytes()[:2] != b"MZ":
            sys.exit(f"release: --sdl3-dll {dll}: not a Windows DLL")
        sdl3 = (dll, sdl3_license(dll, a.sdl3_license), a.sdl3_version.strip())
        print(f"release: shipping {dll.name} ({sdl3[2] or 'version not given'}) in the Windows archive")

    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    wins = sample_game_files()
    print(f"release: game files sampled on this machine: {len(wins)} ({sum(map(len, wins.values()))} samples)")
    made = []
    with tempfile.TemporaryDirectory(prefix="ttt-release-") as tmp:
        for t in targets:
            top = Path(tmp) / f"ttt-{ver}-{TARGETS[t]['suffix']}"
            names = stage(t, ver, top, sdl3)
            path = archive(t, ver, top, out)
            problems = check(path, wins)
            if problems:
                path.unlink()
                sys.exit(f"release: {path.name} REFUSED (deleted):\n  " + "\n  ".join(problems))
            made.append(path)
            print(f"release: {path.relative_to(out.parent) if out.parent in path.parents else path}: "
                  f"{len(names)} files, {path.stat().st_size} bytes, content check OK")
    (out / f"ttt-{ver}-portability.txt").write_text("\n".join(lines) + "\n")
    for line in write_sums(out):
        print(f"release: {line.rstrip()}")
    print("release: OK (nothing published)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
