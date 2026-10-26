"""T47: the port's text catalogs (docs/port/translations.md). The source checks need no build:

- every Tr / TrFmt / Lang_Format / set_err call in port/src gives a stable key and its English text; port/lang/en.json
  holds exactly those keys with the same English text (a string in the code missing from en.json, or a key in en.json
  no code uses, fails);
- the overlay hands ImGui no English sentence that bypasses the catalog (a literal with words as a widget's label or
  text);
- every other catalog (port/lang/<code>.json) is a flat JSON object of strings, without repeated keys, with exactly
  en.json's keys (and "@name"), and each text has the English text's placeholders: {0}..{9} for Lang_Format texts, the
  same printf conversions in order for TrFmt texts (the formats ImGui gets).

With the program built, the settings self-test switches the language through the settings file (TTT_SELFTEST=settings,
tests/test_port_settings.py); the overlay in Spanish is tested in tests/test_port_a11y.py.

`python3 tests/test_port_lang.py --write-en` rewrites port/lang/en.json from the code (keys sorted)."""

import json
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "port/src"
LANG = ROOT / "port/lang"

KEY = r"[a-z][a-z0-9_]*(?:\.[a-z0-9_]+)+"
LIT = r'"(?:[^"\\\n]|\\.)*"'
# A keyed call: the function, then (after other arguments for Lang_Format) the key and the English text, which may be
# several literals in a row.
CALL = re.compile(r"\b(Tr|TrFmt|Lang_Format|set_err)\((?:[^\"();]|\([^()]*\))*?\"(" + KEY + r")\"\s*,\s*((?:" + LIT +
                  r"\s*)+)")
CONV = re.compile(r"%[-+ #0-9.lhzjt*']*[diouxXcspfFeEgGaA]")
PLACE = re.compile(r"\{[0-9]\}")


def c_string(literals):
    """The value of adjacent C string literals (UTF-8 source; \\x and octal escapes are bytes)."""
    out = b""
    for m in re.finditer(LIT, literals):
        body, i = m.group(0)[1:-1], 0
        while i < len(body):
            if body[i] != "\\":
                out += body[i].encode("utf-8")
                i += 1
                continue
            e = re.match(r"x[0-9A-Fa-f]{1,2}|[0-7]{1,3}|.", body[i + 1:]).group(0)
            if e[0] == "x":
                out += bytes([int(e[1:], 16)])
            elif e[0] in "01234567":
                out += bytes([int(e, 8)])
            else:
                out += {"n": "\n", "t": "\t", "r": "\r"}.get(e, e).encode("utf-8")
            i += 1 + len(e)
    return out.decode("utf-8")


def code_strings():
    """{key: (english, kind, where)} from the port's sources; a key given two different texts is an error."""
    found, errors = {}, []
    for f in sorted(list(SRC.glob("*.c")) + list(SRC.glob("*.cpp"))):
        if f.name == "plat_lang.c":  # the catalog code itself (its self-test's made-up keys)
            continue
        text = f.read_text(encoding="utf-8")
        for m in CALL.finditer(text):
            fn, key, en = m.group(1), m.group(2), c_string(m.group(3))
            kind = "printf" if fn == "TrFmt" else "plain"
            line = text.count("\n", 0, m.start()) + 1
            where = f"{f.relative_to(ROOT)}:{line}"
            if key in found and found[key][0] != en:
                errors.append(f"{where}: {key} has another English text than at {found[key][2]}")
            elif key in found and found[key][1] != kind:
                errors.append(f"{where}: {key} is used both as a printf format and as plain text")
            found.setdefault(key, (en, kind, where))
    return found, errors


def load_catalog(path):
    """A catalog: a flat object of strings (repeated keys refused)."""
    def pairs(items):
        d = {}
        for k, v in items:
            if k in d:
                raise ValueError(f"{path.name}: repeated key {k}")
            if not isinstance(v, str):
                raise ValueError(f"{path.name}: {k} is not a string")
            d[k] = v
        return d
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=pairs)


def placeholders(text):
    return sorted(set(PLACE.findall(text)))


def conversions(text):
    return CONV.findall(text.replace("%%", ""))


def write_en():
    found, errors = code_strings()
    if errors:
        sys.exit("\n".join(errors))
    old = load_catalog(LANG / "en.json") if (LANG / "en.json").exists() else {}
    meta = {k: v for k, v in old.items() if k.startswith("@")} or {"@name": "English"}
    out = dict(sorted(meta.items()))
    out.update({k: found[k][0] for k in sorted(found)})
    (LANG / "en.json").write_text(json.dumps(out, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"port/lang/en.json: {len(found)} keys")


# Widgets whose first argument is shown: a literal with words there bypasses the catalog.
WIDGET = re.compile(r"ImGui::(Text|TextWrapped|TextDisabled|TextUnformatted|TextColored|Button|SmallButton|Checkbox|"
                    r"RadioButton|SeparatorText|BeginTabItem|Begin|Selectable|SliderInt|BeginCombo|TableSetupColumn|"
                    r"ProgressBar)\((?:ImVec4\([^)]*\),\s*)?(" + LIT + r")")


class Catalogs(unittest.TestCase):
    def test_code_and_english_catalog_agree(self):
        found, errors = code_strings()
        self.assertEqual(errors, [])
        self.assertGreater(len(found), 150)
        en = load_catalog(LANG / "en.json")
        self.assertEqual(en.get("@name"), "English")
        keys = {k for k in en if not k.startswith("@")}
        self.assertEqual(sorted(set(found) - keys), [], "strings in the code missing from port/lang/en.json "
                         "(python3 tests/test_port_lang.py --write-en)")
        self.assertEqual(sorted(keys - set(found)), [], "keys in port/lang/en.json no code uses")
        self.assertEqual([k for k in found if en[k] != found[k][0]], [], "English texts differ from the code's")

    def test_no_untranslated_widget_text(self):
        bad = []
        for f in sorted(SRC.glob("*.cpp")):
            text = f.read_text(encoding="utf-8")
            for m in WIDGET.finditer(text):
                s = c_string(m.group(2))
                if s.startswith("#") or s in ("OpenGL",):
                    continue
                if re.search(r"[A-Za-z]{2,}", CONV.sub("", s.split("##")[0])):
                    bad.append(f"{f.name}:{text.count(chr(10), 0, m.start()) + 1}: {s!r}")
        self.assertEqual(bad, [])

    def test_translations_complete_and_well_formed(self):
        found, _ = code_strings()
        en = load_catalog(LANG / "en.json")
        others = sorted(p for p in LANG.glob("*.json") if p.name != "en.json")
        self.assertTrue(any(p.name == "es.json" for p in others))
        for path in others:
            with self.subTest(catalog=path.name):
                cat = load_catalog(path)
                self.assertTrue(cat.get("@name"), "no @name")
                want = {k for k in en if not k.startswith("@")}
                have = {k for k in cat if not k.startswith("@")}
                self.assertEqual(sorted(want - have), [], "missing keys")
                self.assertEqual(sorted(have - want), [], "extra keys")
                for k in sorted(want & have):
                    self.assertTrue(cat[k].strip(), f"{k}: empty")
                    self.assertEqual(placeholders(cat[k]), placeholders(en[k]), f"{k}: placeholders")
                    if found.get(k, ("", "plain"))[1] == "printf":
                        self.assertEqual(conversions(cat[k]), conversions(en[k]), f"{k}: printf conversions")
                    else:
                        self.assertEqual([c for c in conversions(cat[k]) if c != "%s"], [], f"{k}: a stray % format")

    def test_checker_catches_broken_catalogs(self):
        """The checks themselves: a broken placeholder or conversion is seen."""
        self.assertNotEqual(placeholders("{0} y {1}"), placeholders("{0} and {2}"))
        self.assertNotEqual(conversions("%s: %d"), conversions("%d: %s"))
        self.assertEqual(conversions("100%% %d"), ["%d"])
        self.assertEqual(c_string('"a\\"b" "c\\xC3\\xA9"'), 'a"bc\u00e9')
        with self.assertRaises(ValueError):
            json.loads('{"a": "x", "a": "y"}', object_pairs_hook=lambda items: (_ for _ in ()).throw(ValueError())
                       if len({k for k, _ in items}) != len(items) else dict(items))


if __name__ == "__main__":
    if "--write-en" in sys.argv:
        write_en()
    else:
        unittest.main()
