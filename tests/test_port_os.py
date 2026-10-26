"""The operating system layer (T42 b6, port/src/plat_os.h, docs/port/windows.md): the port's Linux-only calls are made in
plat_os.h's POSIX part and plat_os_posix.c only, and each function of the layer has its Windows counterpart in
plat_os_win32.c. Reads the sources only."""
import re
import unittest
from pathlib import Path

SRC = Path(__file__).resolve().parents[1] / "port/src"
LAYER = {"plat_os.h", "plat_os_posix.c", "plat_os_win32.c"}

# Linux-only interfaces (or ones whose Windows form differs: mkdir's mode, rename over a file, dirent, dlopen, ...)
LINUX_ONLY = re.compile(
    r"/proc/|\bmemfd_create\b|\bMAP_FIXED|\bmmap\s*\(|\bsigaction\b|\bsigaltstack\b|\bprocess_vm_readv\b|\bSYS_\w+|"
    r"\bstatx\b|\buserfaultfd\s*\(|\bioctl\s*\(|\bmadvise\s*\(|\bdl(open|sym|close|error)\s*\(|\breadlink\s*\(|"
    r"\brealpath\s*\(|\b(open|read|close)dir\s*\(|(?<![\w.])mkdir\s*\(|\bposix_spawnp?\b|\bwaitpid\b|\bsysconf\s*\(|"
    r"\bucontext|\bfsync\s*\(|\b(un)?setenv\s*\(|\bmkdtemp\b|\bpoll\s*\(|"
    r"#\s*include\s*<(sys/|dirent\.h|dlfcn\.h|poll\.h|netdb\.h|spawn\.h|ucontext\.h|netinet/|arpa/)")


def code_lines(text):
    """(line number, text) of each line with the comments blanked out."""
    text = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return list(enumerate(text.splitlines(), 1))


def guarded_lines(text):
    """Line numbers inside `#ifndef _WIN32` / the `#else` of `#ifdef _WIN32` (one level): the layer's own POSIX parts
    in other files (port_host.h, plat_paths.c's per-user folders)."""
    out, stack = set(), []
    for n, line in code_lines(text):
        s = line.strip()
        if s.startswith("#if"):
            stack.append("posix" if s.startswith("#ifndef _WIN32") else "win" if s.startswith("#ifdef _WIN32") else "")
        elif s.startswith("#else") and stack:
            stack[-1] = {"win": "posix", "posix": "win"}.get(stack[-1], "")
        elif s.startswith("#endif") and stack:
            stack.pop()
        if "posix" in stack:
            out.add(n)
    return out


class OsLayer(unittest.TestCase):
    def test_no_linux_calls_outside_the_layer(self):
        found = []
        for f in sorted([*SRC.glob("*.c"), *SRC.glob("*.h"), *SRC.glob("*.cpp")]):
            if f.name in LAYER:
                continue
            text = f.read_text(errors="replace")
            guarded = guarded_lines(text)
            found += [f"{f.name}:{n}: {line.strip()}" for n, line in code_lines(text)
                      if n not in guarded and LINUX_ONLY.search(line)]
        self.assertEqual(found, [])

    def test_every_function_has_a_windows_side(self):
        header = (SRC / "plat_os.h").read_text()
        win = (SRC / "plat_os_win32.c").read_text()
        names = set(re.findall(r"\b(PlatOs_\w+)\s*\(", header))
        self.assertGreater(len(names), 30)
        missing = sorted(n for n in names if not re.search(r"^[\w ]+\*?\s*\*?%s\(" % n, win, re.M))
        self.assertEqual(missing, [])

    def test_posix_part_is_off_on_windows(self):
        self.assertTrue((SRC / "plat_os_posix.c").read_text().split("*/", 1)[1].lstrip().startswith("#ifndef _WIN32"))
        self.assertTrue((SRC / "plat_os_win32.c").read_text().split("*/", 1)[1].lstrip().startswith("#ifdef _WIN32"))


if __name__ == "__main__":
    unittest.main()
