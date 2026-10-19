#!/usr/bin/env python3
"""Sets up (and optionally runs) decomp-permuter on one function.

Usage: .venv/bin/python scripts/permute.py <src/file.c> <Func> [--run] [-jN | -j N] [-- permuter options]

<src/file.c> is a C file under src/ (relative to the repository or absolute) and <Func> a C identifier the file
names; anything else stops with a message before a file is written. Writes permuter/<Func>/ (outside build/, so a
re-split or a clean rebuild does not take it away from a running permuter):
  base.c         the C file preprocessed with -DPERMUTER (INCLUDE_ASM expands to nothing), with the attempt of
                 <Func> enabled when it sits in an `#if 0` (the usual form of an attempt:
                 `#if 0` C `#else` INCLUDE_ASM(...) `#endif`);
  target.o       the original function: asm/nonmatchings/<file>/<Func>.s assembled like the build does; for a
                 function that is already C (no .s), the compiled C, once fdiff.py has confirmed it matches;
  compile.sh     `compile.sh in.c -o out.o` with the compiler, flags and -G value of the build (configure.py);
  objdump.sh     the disassembly of <Func> alone (base.c may define other functions), through its jump-table
                 labels (splat's `.L` labels are symbols in target.o, the compiler's are not);
  settings.toml  func_name, compiler_type and objdump_command for the permuter.

--run starts tools/decomp-permuter/permuter.py on the folder (Ctrl-C stops it); -jN or -j N sets its threads (default
$COOL_JOBS when scripts/cool.sh set it, else every core; run it as `scripts/cool.sh .venv/bin/python scripts/permute.py
...`, one permuter at a time) and arguments after `--` go to it (e.g. `-- --stop-on-zero`). Results land in output-<score>-<n>/source.c. A score
of 0 ignores some differences (stack offsets, branch targets): copy a candidate into the source and check it
with fdiff.py and the SHA-1 build before believing it. Nothing under src/ or config/ is changed.
"""

import importlib.util
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

import fdiff

ROOT = fdiff.ROOT
# Not under build/: `rm -rf build` (a clean rebuild) must not pull the folder from under a long permuter run.
WORK = ROOT / "permuter"
PERMUTER = ROOT / "tools" / "decomp-permuter"
OBJDUMP = ROOT / (fdiff.BINUTILS + "objdump")
AS = ROOT / (fdiff.BINUTILS + "as")

_spec = importlib.util.spec_from_file_location("configure", ROOT / "configure.py")
configure = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(configure)


def die(msg):
    sys.exit("permute.py: " + msg)


def enable_attempt(text, func):
    """text with the `#if 0` around the definition of func turned into `#if 1`. Raises ValueError when there is
    no definition or it sits in the #else branch of an `#if 0`."""
    lines = text.split("\n")
    head = re.compile(r"^[A-Za-z_][\w\s*]*?\b%s\s*\(" % re.escape(func))
    defn = None
    for i, line in enumerate(lines):
        if head.match(line):
            rest = "\n".join(lines[i:i + 40])
            brace, semi = rest.find("{"), rest.find(";")
            if brace != -1 and (semi == -1 or brace < semi):
                defn = i
                break
    if defn is None:
        raise ValueError(f"no C definition of {func}")
    stack = []  # [line, directive, in #else]
    for i in range(defn):
        d = lines[i].strip()
        if not d.startswith("#"):
            continue
        d = d[1:].strip()
        if re.match(r"(if|ifdef|ifndef)\b", d):
            stack.append([i, d, False])
        elif re.match(r"(else|elif)\b", d) and stack:
            stack[-1][2] = True
        elif re.match(r"endif\b", d) and stack:
            stack.pop()
    for i, d, in_else in stack:
        if re.match(r"if\s+0\b", d):
            if in_else:
                raise ValueError(f"{func} is defined in the #else of the `#if 0` at line {i + 1}")
            lines[i] = re.sub(r"if\s+0\b", "if 1", lines[i], count=1)
    return "\n".join(lines)


def check_args(src, func, root=ROOT):
    """The C file as a path relative to root (src/...c), after checking that it is a C file under root/src and
    that func is a C identifier the file mentions (a definition, an attempt or an INCLUDE_ASM). Raises ValueError."""
    if not re.fullmatch(r"[A-Za-z_]\w*", func):
        raise ValueError(f"not a function name: {func!r}")
    root = Path(root).resolve()
    path = Path(src)
    path = (path if path.is_absolute() else root / path).resolve()
    if path.suffix != ".c" or not path.is_file() or not path.is_relative_to(root / "src"):
        raise ValueError(f"not a C file under src/: {src}")
    rel = path.relative_to(root).as_posix()
    if not re.search(r"\b%s\b" % re.escape(func), path.read_text(errors="replace")):
        raise ValueError(f"no {func} in {rel}")
    return rel


def default_jobs(env=os.environ):
    """The permuter's thread count when no -jN is given: $COOL_JOBS (a positive integer, set by scripts/cool.sh), else
    every core."""
    jobs = env.get("COOL_JOBS", "")
    return jobs if jobs.isdigit() and int(jobs) > 0 else str(os.cpu_count() or 1)


def settings_toml(func, objdump_sh):
    return f'func_name = "{func}"\ncompiler_type = "gcc"\nobjdump_command = "{objdump_sh}"\n'


def write_script(path, text):
    path.write_text(text)
    path.chmod(0o755)


def compile_sh(src):
    g = configure.g_flag(src)
    inc = shlex.quote(f"-I{ROOT / 'include'}")
    cc_flags = configure.CC_FLAGS.replace("-Iinclude", inc)
    as_flags = configure.CC_AS_FLAGS.replace("-Iinclude", inc)
    return f"""#!/bin/bash
# compile.sh in.c -o out.o  (generated by scripts/permute.py for {src}; same steps as the build's cc rule)
set -e
ASM="${{3%.o}}.s"
trap 'rm -f "$ASM"' EXIT
{shlex.quote(str(ROOT / configure.CC))} {cc_flags} {g} -w -S "$1" -o "$ASM"
{shlex.quote(str(AS))} {as_flags} {g} {shlex.quote(str(ROOT / 'include' / 'gcc_prelude.inc'))} "$ASM" -o "$3"
"""


def objdump_sh(func):
    # splat's jump-table labels (`jlabel .L0036FB80`) are symbols in target.o, the compiler's .L labels are not: a
    # `<.L...>:` line neither ends the function nor is printed, so both sides list the same lines (blank ones dropped).
    return f"""#!/bin/bash
# objdump.sh file.o: disassembly with relocations of {func} only, through its jump-table labels (.L*)
{shlex.quote(str(OBJDUMP))} -drz -m mips:5900 "$1" | awk '$2 == "<{func}>:" {{ p = 1; print; next }} /^[0-9a-f]+ <.*>:$/ {{ if ($2 !~ /^<\\.L/) p = 0; next }} NF == 0 {{ next }} p'
"""


def run(cmd):
    try:
        return subprocess.run([str(c) for c in cmd], cwd=ROOT, capture_output=True, text=True)
    except FileNotFoundError:
        die(f"cannot run {cmd[0]}: missing tool (docs/setup.md)")


def make_target(work, src, func):
    """Writes target.o; returns where it came from."""
    asm = Path("asm/nonmatchings") / Path(src).relative_to("src").with_suffix("") / f"{func}.s"
    if (ROOT / asm).exists():
        wrapper = work / "target.s"
        wrapper.write_text(f'.include "macro.inc"\n.section .text\n.set noat\n.set noreorder\n'
                           f'.include "{asm}"\n')
        r = run([AS, *configure.CC_AS_FLAGS.split(), configure.g_flag(src), "-I.", "include/gcc_prelude.inc",
                 wrapper, "-o", work / "target.o"])
        if r.returncode:
            die(f"assembling {asm} failed:\n{r.stderr}")
        return str(asm)
    r = run([sys.executable, ROOT / "scripts" / "fdiff.py", func])
    if r.returncode or not r.stdout.startswith("OK"):
        die(f"no {asm} and the C of {func} does not match yet (fdiff.py). Keep the attempt in `#if 0`, with "
            f"INCLUDE_ASM in its #else, and run configure.py --split so that splat writes the .s")
    r = run(["bash", work / "compile.sh", work / "base.c", "-o", work / "target.o"])
    if r.returncode:
        die(f"compiling base.c failed:\n{r.stdout}{r.stderr}")
    return "the C in the build (fdiff.py: OK)"


def dump(work, obj):
    return run(["bash", work / "objdump.sh", obj]).stdout.splitlines()[1:]


def parse_args(args, env=os.environ):
    """(the two positional arguments, --run given, the thread count, the permuter options after `--`) of a command
    line; the thread count is -jN or -j N, else default_jobs(env). Raises ValueError on anything else."""
    extra = []
    if "--" in args:
        args, extra = args[:args.index("--")], args[args.index("--") + 1:]
    pos, start, jobs = [], False, None
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--run":
            start = True
        elif a == "-j" and i + 1 < len(args) and re.fullmatch(r"[1-9]\d*", args[i + 1]):
            jobs = args[i + 1]
            i += 1
        elif re.fullmatch(r"-j[1-9]\d*", a):
            jobs = a[2:]
        elif a.startswith("-"):
            raise ValueError(f"unknown option {a}")
        else:
            pos.append(a)
        i += 1
    if len(pos) != 2:
        raise ValueError("expected <src/file.c> <Func>")
    return pos, start, jobs or default_jobs(env), extra


def main():
    try:
        pos, start, jobs, extra = parse_args(sys.argv[1:])
    except ValueError:
        sys.exit(__doc__)
    try:
        src = check_args(pos[0], pos[1])
        func = pos[1]
        text = enable_attempt((ROOT / src).read_text(), func)
    except ValueError as e:
        die(str(e))

    work = WORK / func
    if work.exists():
        for old in work.iterdir():
            if not old.name.startswith("output-"):
                shutil.rmtree(old) if old.is_dir() else old.unlink()
    work.mkdir(parents=True, exist_ok=True)
    write_script(work / "compile.sh", compile_sh(src))
    write_script(work / "objdump.sh", objdump_sh(func))
    (work / "settings.toml").write_text(settings_toml(func, work / "objdump.sh"))

    enabled = work / "enabled.c"
    enabled.write_text(text)
    r = run([ROOT / configure.CC, "-E", "-P", "-DPERMUTER", "-Iinclude", f"-I{Path(src).parent}", enabled,
             "-o", work / "base.c"])
    enabled.unlink()
    if r.returncode:
        die("preprocessing failed:\n" + r.stderr)

    origin = make_target(work, src, func)
    r = run(["bash", work / "compile.sh", work / "base.c", "-o", work / "base.o"])
    if r.returncode:
        die(f"base.c does not compile:\n{r.stdout}{r.stderr}")
    target, base = dump(work, work / "target.o"), dump(work, work / "base.o")
    (work / "base.o").unlink()
    if not target:
        die(f"{func} is not in target.o")
    differ = sum(a != b for a, b in zip(target, base)) + abs(len(target) - len(base))
    print(f"{func}: {work.relative_to(ROOT)}")
    print(f"  target: {origin} ({len(target)} lines of objdump)")
    print(f"  base.c: {'identical to the target' if not differ else f'{differ} objdump lines differ'}")

    if start:
        cmd = [sys.executable, str(PERMUTER / "permuter.py"), str(work), f"-j{jobs}", *extra]
        print("  running:", " ".join(cmd), flush=True)
        os.execv(sys.executable, cmd)


if __name__ == "__main__":
    main()
