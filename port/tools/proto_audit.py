#!/usr/bin/env python3
"""Audit the prototypes the PC port compiles for argument layouts that differ on i386 (T41 b0).

On the EE the drafts' prototypes can disagree and still work: integer arguments travel in $a0-$t3, float arguments in
$f12-$f19, so a missing trailing argument, an int/float order swap or a 4-byte/8-byte mix-up often reads the right
register anyway. On i386 every argument is a stack slot in order: the callee then reads a garbage slot, swaps two
arguments, or a variadic reader (`Text_Printf`) walks past what the caller pushed.

The audit preprocesses every game C file the port compiles (the drafts switched on, as port/tools/portsrc.py does)
and the port's own C (port/src), collects every function declaration (file scope and block scope) and definition, and
reduces each to its i386 slot layout:

    I  a 4-byte integer or pointer        F  a float (4 bytes; the build is soft-float, so a float result is in eax)
    D  a double (8 bytes)                 L  a 64-bit integer (8 bytes)
    Q  a 128-bit integer (16 bytes)       S  a structure by value (its name)
    V  `...`                              U  no prototype: `f()` (the arguments are promoted, floats to double)

Every declaration whose layout differs from the definition (or, with no C definition, from the other declarations)
is reported with its source line. A difference only in I/F at the same slot, or in the return type I/F, keeps the
stack layout and is listed apart as "class" differences (value reinterpretation, also wrong on the EE).

It also checks the calls of `Text_Printf` (the game's variadic printer: `%Z %H %V %F %S %c %s %d %x %D %C %h %v %f
%p %b` read one 4-byte argument each) whose format is a rodata string of the split (asm/**/D_XXXXXXXX.s `.asciz`):
the number of arguments against the format's.

Usage: python3 port/tools/proto_audit.py [-v] [name ...]
       (-v also lists the class differences; names limit the report). Needs asm/ (the matching build's split) for the
       format strings. Exit status 0; the report is the output."""
import collections
import pathlib
import re
import subprocess
import sys

from toolchain import PORT_CFLAGS, ROOT, cc, game_cflags
import portsrc

BASE_INT = {"char", "short", "int", "long", "unsigned", "signed", "_Bool", "enum"}
KEYWORDS = {"return", "else", "case", "goto", "do", "sizeof", "if", "while", "for", "switch", "typedef"}
QUALIFIERS = {"extern", "static", "const", "volatile", "inline", "__inline", "__inline__", "register", "struct",
              "union", "enum", "unsigned", "signed", "__attribute__", "__extension__"}
# Text_Printf (select_results_b.c): directives that read one argument.
TP_ARGS = set("ZHVFScsdxDChvfpb")


def preprocess(path, flags):
    r = subprocess.run([cc(), *flags, "-E", str(path)], cwd=ROOT, capture_output=True, text=True)
    return r.stdout


def lines_with_origin(text):
    """[(origin file, origin line, text)] from the preprocessor's line markers."""
    out, cur, n = [], "?", 0
    for l in text.split("\n"):
        m = re.match(r'# (\d+) "([^"]+)"', l)
        if m:
            n, cur = int(m.group(1)), m.group(2)
            continue
        out.append((cur, n, l))
        n += 1
    return out


def norm_origin(f):
    f = f.replace(str(ROOT) + "/", "")
    return re.sub(r"^build/port/gen/src/", "src/", f)


class Types:
    """Typedef names -> slot class, from the preprocessed text."""

    def __init__(self):
        self.td = {"f32": "F", "f64": "D", "s64": "L", "u64": "L", "s128": "Q", "u128": "Q"}
        self.td.update({n: "I" for n in ("s8", "u8", "s16", "u16", "s32", "u32")})

    def scan(self, text):
        for m in re.finditer(r"\btypedef\s+([^;{}]*?)\s*\b(\w+)\s*;", text):
            base, name = m.group(1), m.group(2)
            if name not in self.td:
                c = self.classify(base)
                if c:
                    self.td[name] = c
        for m in re.finditer(r"\btypedef\s+[^;{}()]*?\b(\w+)\s*\[[^;{}]*\]\s*;", text):
            self.td.setdefault(m.group(1), "I")  # an array type: a pointer as a parameter
        for m in re.finditer(r"\btypedef\s+(struct|union)\s*\w*\s*\{", text):
            # the name follows the matching brace
            i, depth = m.end(), 1
            while i < len(text) and depth:
                depth += {"{": 1, "}": -1}.get(text[i], 0)
                i += 1
            n = re.match(r"\s*(?:__attribute__\s*\(\(.*?\)\)\s*)?(\w+)", text[i:i + 200])
            if n and n.group(1) not in self.td:
                self.td[n.group(1)] = "S:" + n.group(1)
        for m in re.finditer(r"\btypedef\s+(struct|union)\s+(\w+)\s+(\w+)\s*;", text):
            self.td.setdefault(m.group(3), "S:" + m.group(3))

    def classify(self, t):
        """Slot class of a parameter or return type (name included or not), None when unknown."""
        t = re.sub(r"__attribute__\s*\(\(.*?\)\)", "", t).strip()
        if "(" in t or "*" in t or "[" in t:
            return "I"  # pointer, array or function pointer
        words = re.findall(r"\w+", t)
        if not words:
            return None
        if words[0] in ("struct", "union") and len(words) > 1:
            return "S:" + words[1]
        if "float" in words:
            return "F"
        if "double" in words:
            return "D"
        if words.count("long") >= 2:
            return "L"
        if any(w in BASE_INT for w in words):
            return "I"
        for w in words:
            if w in QUALIFIERS:
                continue
            if w == "void":
                return "void"
            if w in self.td:
                return self.td[w]
            return None
        return None


def split_params(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
            continue
        depth += ch in "([{"
        depth -= ch in ")]}"
        cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def layout(types, params):
    ps = params.strip()
    if ps == "":
        return ("U",)
    if ps == "void":
        return ()
    out = []
    for p in split_params(ps):
        if p == "...":
            out.append("V")
            continue
        c = types.classify(p)
        out.append(c or "?" + p)
    return tuple(out)


DECL = re.compile(r"(?:^|(?<=[;{}]))\s*((?:(?:extern|static|const|volatile|inline|__inline__|unsigned|signed|struct|union"
                  r"|enum|PORT_RET\(\w+\))\s+)*[A-Za-z_]\w*(?:\s*\*)*)\s*(\*?)\s*\b([A-Za-z_]\w*)\s*\(((?:[^;{}()]|\([^;{}()]*"
                  r"(?:\([^;{}()]*\))?[^;{}()]*\))*)\)\s*(?:__attribute__\s*\(\(.*?\)\)\s*)?([;{])", re.S)


class Decl:
    def __init__(self, name, ret, params, origin, line, is_def, text):
        self.name, self.ret, self.params, self.origin, self.line, self.is_def, self.text = (
            name, ret, params, origin, line, is_def, text)

    def where(self):
        return f"{self.origin}:{self.line}"


def collect(path, flags, types, decls, calls):
    text = preprocess(path, flags)
    if not text:
        print(f"proto_audit: cannot preprocess {path}", file=sys.stderr)
        return
    types.scan(text)
    rows = lines_with_origin(text)
    body = "\n".join(r[2] for r in rows)
    starts, pos = [], 0
    for r in rows:
        starts.append(pos)
        pos += len(r[2]) + 1

    def origin(off):
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= off:
                lo = mid
            else:
                hi = mid - 1
        return norm_origin(rows[lo][0]), rows[lo][1]

    for m in DECL.finditer(body):
        ret, star, name, params, end = m.groups()
        rw = ret.split()
        if not rw or rw[0] in KEYWORDS or name in KEYWORDS or rw[-1] in KEYWORDS:
            continue
        if re.match(r"PORT_RET\((\w+)\)", ret):
            ret = re.sub(r"PORT_RET\((\w+)\)", r"\1", ret)
        rc = types.classify(ret + star)
        if rc is None:
            continue  # not a type: an expression such as `a * f(b);`
        o, n = origin(m.start(3))
        key = name
        if re.search(r"\bstatic\b", ret):
            key = f"{name}@{norm_origin(str(path))}"  # one per file
        decls[key].append(Decl(name, rc, params, o, n, end == "{", m.group(0).strip()))
    # Text_Printf calls (any TU): (origin, line, args)
    for m in re.finditer(r"\bText_Printf\s*\(", body):
        i, depth = m.end(), 1
        while i < len(body) and depth:
            depth += {"(": 1, ")": -1}.get(body[i], 0)
            i += 1
        args = split_params(body[m.end():i - 1])
        pre = body[max(0, m.start() - 40):m.start()]
        if re.search(r"\b(?:s32|int|void|extern)\s*$", pre):
            continue  # a declaration
        o, n = origin(m.start())
        calls.append((o, n, args))


def implicit_calls(path, flags):
    """[(origin, line, name, argument count)] of the calls gcc sees without a declaration (implicit int, arguments
    promoted: a float travels as a double)."""
    fl = [f for f in flags if f != "-w"] + ["-fsyntax-only", "-Wimplicit-function-declaration"]
    r = subprocess.run([cc(), *fl, str(path)], cwd=ROOT, capture_output=True, text=True)
    out = []
    src = None
    for m in re.finditer(r"^([^:\n]+):(\d+):(\d+): (?:warning|error): implicit declaration of function .(\w+).",
                         r.stderr, re.M):
        f, line, col, name = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)
        try:
            text = (ROOT / f).read_text() if not f.startswith("/") else pathlib.Path(f).read_text()
        except OSError:
            continue
        lines = text.split("\n")
        rest = "\n".join(lines[line - 1:line + 20])[col - 1:]
        i = rest.find("(")
        depth, j = 1, i + 1
        while j < len(rest) and depth:
            depth += {"(": 1, ")": -1}.get(rest[j], 0)
            j += 1
        args = split_params(rest[i + 1:j - 1]) if i >= 0 else []
        out.append((norm_origin(f), line, name, len(args)))
    return out


def rodata_strings():
    out = {}
    for f in (ROOT / "asm").rglob("D_*.s"):
        t = f.read_text(errors="replace")
        m = re.search(r'\.asciz\s+"((?:[^"\\]|\\.)*)"', t)
        if m:
            out[f.stem] = m.group(1)
    return out


def tp_count(fmt):
    """Arguments Text_Printf reads for fmt, None when the format stops early (an unknown directive returns 0)."""
    n, state = 0, 0
    for ch in fmt:
        if ch == "%" and state != 1:
            state = 1
            continue
        if state != 1:
            continue
        state = 0
        if ch in "012345678":
            state = 1
        elif ch in TP_ARGS:
            n += 1
        elif ch in "TP%":
            pass
        else:
            return None
    return n


def stack_bytes(lay):
    size = {"I": 4, "F": 4, "D": 8, "L": 8, "Q": 16}
    return sum(size.get(c, 4) for c in lay if c not in ("V", "U"))


def compare(ref, d):
    """None when d agrees with ref on i386, else (kind, text)."""
    a, b = layout_of(ref), layout_of(d)
    if a == b and ref.ret == d.ret:
        return None
    if a == b:
        if {ref.ret, d.ret} <= {"I", "F"}:
            return ("class", f"return {d.ret} against {ref.ret}")
        if "void" in (ref.ret, d.ret):
            return ("return", f"return {d.ret} against {ref.ret}")
        return ("return", f"return {d.ret} against {ref.ret}")
    if "U" in b or "U" in a:
        return ("unprototyped", f"{fmt_lay(b)} against {fmt_lay(a)}")
    va, vb = "V" in a, "V" in b
    fa, fb = [c for c in a if c != "V"], [c for c in b if c != "V"]
    if va != vb:
        kind = "variadic"
        return (kind, f"{fmt_lay(b)} against {fmt_lay(a)}")
    if len(fa) == len(fb) and stack_bytes(fa) == stack_bytes(fb):
        unknown = any(x.startswith("?") or y.startswith("?") for x, y in zip(fa, fb))
        if all(x == y or {x, y} <= {"I", "F"} for x, y in zip(fa, fb)) or unknown:
            if sorted(fa) == sorted(fb) and fa != fb and not unknown:
                return ("swap", f"{fmt_lay(b)} against {fmt_lay(a)} (int/float order)")
            return ("class", f"{fmt_lay(b)} against {fmt_lay(a)}")
        if all(x == y or (x.startswith("S:") and y.startswith("S:")) or (x.startswith("S:") or y.startswith("S:"))
               for x, y in zip(fa, fb)):
            return ("class", f"{fmt_lay(b)} against {fmt_lay(a)} (structure types)")
    if set(fa) <= {"I", "L", "Q"} and set(fb) <= {"I", "L", "Q"} and stack_bytes(fa) == stack_bytes(fb):
        return ("class", f"{fmt_lay(b)} against {fmt_lay(a)} (64/128-bit values as 32-bit halves)")
    if len(fb) < len(fa) or stack_bytes(fb) < stack_bytes(fa):
        return ("fewer", f"{fmt_lay(b)} against {fmt_lay(a)}")
    if len(fb) > len(fa) and fb[:len(fa)] == fa:
        return ("extra", f"{fmt_lay(b)} against {fmt_lay(a)}")
    return ("layout", f"{fmt_lay(b)} against {fmt_lay(a)}")


TYPES = Types()
DRAFTS = set()  # functions whose C is a NON_MATCHING draft the port switches on


def layout_of(d):
    return layout(TYPES, d.params)


def fmt_lay(lay):
    return "(" + ",".join(c[:1] if not c.startswith("S:") else c for c in lay) + ")" if lay else "(void)"


SEVERITY = ["fewer", "swap", "layout", "unprototyped", "variadic", "return", "extra", "class"]


def main():
    verbose = "-v" in sys.argv
    only = {a for a in sys.argv[1:] if not a.startswith("-")}
    decls, calls = collections.defaultdict(list), []
    game = []
    for f in portsrc.sources():
        out, done, _ = portsrc.prepare(f)
        game.append(out)
        DRAFTS.update(done)
    gflags = [f for f in game_cflags() if f not in ("-g",)]
    implicit = []
    for f in game:
        collect(f, gflags, TYPES, decls, calls)
        implicit += implicit_calls(f, gflags)
    for f in sorted((ROOT / "port/src").glob("*.c")):
        collect(f, PORT_CFLAGS + ["-DPORT"], TYPES, decls, calls)

    findings = collections.defaultdict(list)
    for name, ds in sorted(decls.items()):
        if only and name not in only:
            continue
        uniq = {}
        for d in ds:
            uniq.setdefault((d.where(), d.is_def), d)  # headers repeat in every file
        ds = list(uniq.values())
        defs = [d for d in ds if d.is_def]
        if defs:
            ref, base = defs[0], "definition"
        else:
            counts = collections.Counter((layout_of(d), d.ret) for d in ds)
            key = counts.most_common(1)[0][0]
            ref = next(d for d in ds if (layout_of(d), d.ret) == key)
            base = "most declarations (no C definition)"
        for d in ds:
            if d is ref:
                continue
            r = compare(ref, d)
            if r:
                findings[r[0]].append((name, d, ref, base, r[1]))

    strings = rodata_strings()
    fmt_bad = []
    for o, n, args in calls:
        if not args:
            continue
        m = re.search(r"\b(D_[0-9A-F]{8})\b", args[0])
        if not m or m.group(1) not in strings:
            continue
        want = tp_count(strings[m.group(1)])
        if want is not None and want != len(args) - 1:
            fmt_bad.append((o, n, m.group(1), strings[m.group(1)], len(args) - 1, want))

    total = 0
    for kind in SEVERITY:
        rows = findings.get(kind, [])
        if not rows or (kind == "class" and not verbose):
            continue
        print(f"== {kind}: {len(rows)}")
        for name, d, ref, base, why in rows:
            tag = " [draft]" if ref.is_def and ref.name in DRAFTS else ""
            print(f"  {d.where()}: {name} {why}; {base}{tag} at {ref.where()}")
        total += len(rows)
    if not verbose and findings.get("class"):
        print(f"== class: {len(findings['class'])} (same stack layout, int/float or structure type differs; -v lists)")
    seen = set()
    fmt_rows = [r for r in fmt_bad if not (r[0], r[1]) in seen and not seen.add((r[0], r[1]))]
    print(f"== Text_Printf format/argument count: {len(fmt_rows)}")
    for o, n, sym, s, got, want in fmt_rows:
        print(f'  {o}:{n}: {sym} "{s}" reads {want}, the call passes {got}')
    imp_rows = []
    for o, n, name, argc in implicit:
        if only and name not in only:
            continue
        defs = [d for d in decls.get(name, []) if d.is_def]
        if not defs:
            imp_rows.append((o, n, name, argc, "no C definition"))
            continue
        lay = layout_of(defs[0])
        want = len([c for c in lay if c not in ("V", "U")])
        wide = [c for c in lay if c in ("F", "D", "L", "Q") or c.startswith("S:")]
        if argc != want or wide:
            tag = " [draft]" if defs[0].name in DRAFTS else ""
            imp_rows.append((o, n, name, argc, f"definition{tag} {fmt_lay(lay)} at {defs[0].where()}"))
        elif verbose:
            imp_rows.append((o, n, name, argc, "agrees"))
    print(f"== implicit declarations (calls with no prototype in scope): {len(imp_rows)}")
    for o, n, name, argc, why in imp_rows:
        print(f"  {o}:{n}: {name} called with {argc} arguments; {why}")
    print(f"proto_audit: {sum(len(v) for v in decls.values())} declarations of {len(decls)} names; "
          f"{total} layout differences, {len(fmt_rows)} format mismatches")


if __name__ == "__main__":
    main()
