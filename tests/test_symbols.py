"""Consistency checks of the project's own symbol files (config/symbols/*.txt); no disc or build needed."""

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


fdiff = _load("fdiff", ROOT / "scripts" / "fdiff.py")
apply_names = _load("apply_names", ROOT / "scripts" / "apply_names.py")
progress = _load("progress", ROOT / "scripts" / "progress.py")
configure = _load("configure", ROOT / "configure.py")


class SymbolFiles(unittest.TestCase):
    def setUp(self):
        self.files = apply_names.load_symbols(ROOT)

    def test_every_file_is_listed_in_the_yaml(self):
        listed = configure.yaml_symbol_paths((ROOT / configure.YAML).read_text())
        configure.check_symbol_paths(listed, fdiff.symbol_files(ROOT))

    def test_ranges_do_not_overlap_and_every_entry_is_in_its_file(self):
        self.assertTrue(apply_names.file_ranges(self.files))
        self.assertEqual(apply_names.misplaced(self.files), [])

    def test_library_files_follow_library_ranges(self):
        labeled = progress.parse_labeled_ranges((ROOT / progress.RANGES).read_text())
        for label in {label for _, _, label in labeled}:
            path = f"{fdiff.SYMBOL_DIR}/{label}.txt"
            want = sorted((lo, hi) for lo, hi, l in labeled if l == label)
            self.assertEqual(sorted(fdiff.parse_file_ranges(self.files[path])), want, path)

    def test_names_and_addresses_are_unique(self):
        names, addrs = [], []
        for text in self.files.values():
            for m in map(fdiff.SYMBOL_LINE.match, text.splitlines()):
                if m:
                    names.append(m.group(1))
                    addrs.append(int(m.group(2), 16))
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(len(addrs), len(set(addrs)))


if __name__ == "__main__":
    unittest.main()
