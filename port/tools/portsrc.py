"""The game sources of the PC build and their preparation; shared by undefined.py and build.py.

The matching build keeps the C of a function that does not match yet inside `#if 0`, next to an INCLUDE_ASM line in
the `#else` branch. The PC build wants every function from C: prepare() writes a copy of such a source to
build/port/gen/src with those `#if 0` turned into `#if 1`. Nothing else changes; sources without INCLUDE_ASM are
compiled as they are. A function whose INCLUDE_ASM has no C anywhere in its file is reported as missing."""
import re

from toolchain import GEN, ROOT, YAML

# Drafts the PC build leaves off: their C is an m2c draft around hand-written EE / VU0 assembly that names registers
# the C does not hold, so it cannot run on a PC as it is. port/src/vu0_pc.c supplies these names instead (stubs that
# report themselves until they are written by hand: docs/port/README.md, "Hand replacements").
REPLACED = {"func_003EA978", "func_003EAF78", "func_003EBC98", "func_003EC388", "func_003ECAD8", "func_003ED338",
            "func_003EEF00"}


def c_names(yaml_text):
    """Names of the C subsegments (`- [offset, c, name]`) of the config, in link order."""
    return re.findall(r"^\s+- \[0x[0-9A-Fa-f]+, c, (\w+)\]", yaml_text, re.M)


def sources():
    """Paths of every game C file the matching build links."""
    return [ROOT / "src" / f"{n}.c" for n in c_names(YAML.read_text())]


def enable_attempts(text, replaced=REPLACED):
    """Returns (text with the drafts switched on, names that have C, names with no C found). Names in `replaced`
    are left off and counted in neither list."""
    lines = text.split("\n")
    done, missing = [], []
    for name in re.findall(r"^INCLUDE_ASM\([^,]+, (\w+)\);", text, re.M):
        if name in replaced:
            continue
        pat = re.compile(r"^[A-Za-z_][\w \*]*\b" + name + r"\(.*[,{(]\s*$|^[A-Za-z_][\w \*]*\b" + name + r"\(.*\) \{")
        defs = [i for i, l in enumerate(lines) if pat.match(l) and not l.rstrip().endswith(";")]
        if not defs:
            missing.append(name)
            continue
        depth, opener = 0, None
        for i in range(defs[-1], -1, -1):  # the innermost conditional that encloses the definition
            l = lines[i]
            if l.startswith("#endif"):
                depth += 1
            elif l.startswith("#if"):
                if depth == 0:
                    opener = i
                    break
                depth -= 1
        if opener is not None and re.match(r"#if 0\b", lines[opener]):
            lines[opener] = "#if 1" + lines[opener][5:]  # keep what follows: a note may start on this line
        done.append(name)  # no `#if 0` around it: compiled already (ASM_STUB form)
    return "\n".join(lines), done, missing


def prepare(f):
    """Path to compile for game source f, plus the names switched on and the names without C."""
    text = f.read_text()
    if "INCLUDE_ASM(" not in text:
        return f, [], []
    new, done, missing = enable_attempts(text)
    out = GEN / "src" / f.relative_to(ROOT / "src")
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.read_text() != new:
        out.write_text(new)
    return out, done, missing
