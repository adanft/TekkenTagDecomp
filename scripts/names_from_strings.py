#!/usr/bin/env python3
"""Suggests function names from the strings the functions reference (prints only, changes nothing).

Usage: .venv/bin/python scripts/names_from_strings.py   (after configure.py has split the executable)

Debug and error strings often start with the name of the function that prints them ("sceGsSyncPath: DMA ...")
or name it with empty parentheses ("_sliceA0(): error"). A suggestion is printed in the symbol file format only
when it is one to one: the function (still a func_ placeholder) references strings naming exactly one name, and
no other placeholder function references a string with that name. Check each one before adding it to
config/symbols/ (scripts/apply_names.py): a string can also name a callee.
"""

import collections
import re

import fdiff

ROOT = fdiff.ROOT

LEADING = re.compile(r"^\s*([A-Za-z_]\w*)\s*:")
CALL = re.compile(r"\b([A-Za-z_]\w*)\(\)")
REF = re.compile(r"%(?:hi|lo|gp_rel)\((D_[0-9A-F]{8})\)")


def looks_like_code(name):
    """At least 4 characters, lower case, and an underscore or camel case: not a word of a message."""
    return len(name) >= 4 and re.search(r"[a-z]", name) is not None and \
        ("_" in name or re.search(r"[a-z][A-Z]", name) is not None)


def names_in_string(s):
    """Function names a string (as written in the assembly, escapes kept) names, in order."""
    found = [m.group(1) for m in LEADING.finditer(s)] + [m.group(1) for m in CALL.finditer(s)]
    return list(dict.fromkeys(n for n in found if looks_like_code(n)))


def strings_by_label(text):
    """label -> first string after each `dlabel` of a split data file."""
    out = {}
    label = None
    for line in text.splitlines():
        if m := re.match(r"dlabel (\w+)", line):
            label = m.group(1)
        elif label and label not in out and (m := re.search(r'\.asciz "(.*)"\s*$', line)):
            out[label] = m.group(1)
    return out


def refs_by_function(text):
    """function -> data labels it references (%hi/%lo/%gp_rel), from split code."""
    out = collections.defaultdict(set)
    func = None
    for line in text.splitlines():
        if m := re.match(r"glabel (\w+)", line):
            func = m.group(1)
        elif func:
            out[func].update(REF.findall(line))
    return dict(out)


def suggest(strings, refs):
    """[(name, address, string)] one-to-one suggestions for placeholder functions, by address."""
    names = {}
    for func, labels in refs.items():
        if fdiff.placeholder_addr(func) is None:
            continue
        found = {}
        for label in sorted(labels):
            for n in names_in_string(strings.get(label, "")):
                found.setdefault(n, strings[label])
        names[func] = found
    owners = collections.Counter(n for found in names.values() for n in found)
    out = []
    for func, found in names.items():
        if len(found) == 1:
            (name, s), = found.items()
            if owners[name] == 1:
                out.append((name, fdiff.placeholder_addr(func), s))
    return sorted(out, key=lambda x: x[1])


def main():
    strings = {}
    for path in sorted((ROOT / "asm" / "data").glob("*.s")):
        strings.update(strings_by_label(path.read_text(errors="replace")))
    refs = {}
    for path in sorted((ROOT / "asm").glob("*.s")) + sorted((ROOT / "asm" / "nonmatchings").rglob("*.s")):
        for func, labels in refs_by_function(path.read_text(errors="replace")).items():
            refs.setdefault(func, set()).update(labels)
    taken = set(fdiff.known_names())
    for name, addr, s in suggest(strings, refs):
        if name not in taken:
            print(f'{name} = 0x{addr:08X}; // type:func  from "{s[:60]}"')


if __name__ == "__main__":
    main()
