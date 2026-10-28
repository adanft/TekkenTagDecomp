"""The player release (T44, port/tools/release.py, docs/port/release.md): no game data in any archive, the files a
player needs present, and the extracted Linux program running from its own folder (its version, a self-test, its
translations found beside it, the refusal without a disc).

The content checker itself is tested on made-up archives every run. The archives are tested when there are some:
with TTT_SLOW_TESTS=1 the programs already built are packaged into a temporary folder first (release.py --no-build
--allow-dirty: the release's own build is release.py's job), otherwise the current version's archives in dist/ are
inspected, if any. With TTT_SLOW_TESTS=1 and the disc on this machine, the extracted program also sets up from the
disc and plays the idle run to the reference hash."""

import hashlib
import io
import os
import re
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "port/tools"))
import release  # noqa: E402

SLOW = os.environ.get("TTT_SLOW_TESTS") == "1"
IDLE_HASH = "0x8ba37caeb2bacfa8"  # tests/test_port_data.py
ELF_SHA1 = "c7ad9a022bdc836827a0cc093f91b4281836f2bb"

# What an archive may hold below its top folder (independent of release.py's own list).
LINUX_ALLOWED = re.compile(r"(ttt|ttt\.syms|ttt-server|ttt-sign|README\.txt|VERSION\.txt|CHANGELOG\.txt|"
                           r"lang/[a-z]{2}(_[A-Z]{2})?\.json|LICENSES/[\w.-]+\.txt|docs/[\w-]+\.md)")
WINDOWS_ALLOWED = re.compile(r"(ttt\.exe|ttt\.exe\.syms|ttt-server\.exe|ttt-sign\.exe|README\.txt|VERSION\.txt|"
                             r"CHANGELOG\.txt|lang/[a-z]{2}(_[A-Z]{2})?\.json|LICENSES/[\w.-]+\.txt|docs/[\w-]+\.md)")
LINUX_REQUIRED = {"ttt", "ttt.syms", "ttt-server", "ttt-sign", "README.txt", "VERSION.txt", "CHANGELOG.txt",
                  "lang/en.json", "lang/es.json", "LICENSES/README.txt", "LICENSES/imgui.txt", "LICENSES/pl_mpeg2.txt",
                  "LICENSES/stb.txt", "LICENSES/xxhash.txt", "LICENSES/tweetnacl.txt", "LICENSES/newlib.txt",
                  "docs/install.md", "docs/netplay.md", "docs/matchmaking.md"}
WINDOWS_REQUIRED = ({n for n in LINUX_REQUIRED if n not in ("ttt", "ttt.syms", "ttt-server", "ttt-sign")} |
                    {"ttt.exe", "ttt.exe.syms", "ttt-server.exe", "ttt-sign.exe", "LICENSES/mingw-w64-runtime.txt"})
# Never in an archive, in any folder (case ignored).
FORBIDDEN = re.compile(r"(^|/)(iso|saves|textures|mods|crash|frames)/|\.(iso|bin|cue|img|elf|irx|dat|disc|mcd|ps2|"
                       r"key|wav|ogg|png|ppm|dll|so)$|(^|/)(slus_|sces_|ttt\.dat|system\.cnf|settings\.ini|portable\.txt)",
                       re.I)


def entries(path):
    """{name below the top folder: (bytes, mode or None)} and the top folders."""
    out, tops = {}, set()
    if path.suffix == ".zip":
        with zipfile.ZipFile(path) as z:
            for i in z.infolist():
                top, _, rel = i.filename.partition("/")
                tops.add(top)
                if rel and not i.is_dir():
                    out[rel] = (z.read(i), i.external_attr >> 16)
    else:
        with tarfile.open(path, "r:gz") as t:
            for i in t.getmembers():
                top, _, rel = i.name.partition("/")
                tops.add(top)
                if rel and not i.isdir():
                    out[rel] = (t.extractfile(i).read() if i.isfile() else None, i.mode)
    return out, tops


def make_tar(path, files, links=()):
    """A .tar.gz with `files` {name: bytes} and symbolic links [(name, target)]."""
    with tarfile.open(path, "w:gz") as t:
        for name, data in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            info.mode = 0o755
            t.addfile(info, io.BytesIO(data))
        for name, target in links:
            info = tarfile.TarInfo(name)
            info.type = tarfile.SYMTYPE
            info.linkname = target
            t.addfile(info)


def clean_env(home, **extra):
    """A player's environment with nothing of this machine's: no display (no window opens), sound through SDL's
    dummy driver, the per-user folders in `home`."""
    e = {"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "HOME": str(home), "LANG": "C",
         "XDG_DATA_HOME": str(home / "data"), "XDG_CONFIG_HOME": str(home / "config"), "SDL_AUDIO_DRIVER": "dummy"}
    e.update(extra)
    return e


class TestChecker(unittest.TestCase):
    """release.check on made-up archives: what it must refuse, and that a clean one passes."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="ttt-release-test-")
        self.dir = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_layout_names_the_games_files(self):
        files = release.layout_files()
        self.assertIn(("SLUS_200.01", 2873728, ELF_SHA1), files)
        self.assertTrue(any(w == "ttt.dat payload" for w, _, _ in files))

    def test_clean_archive_passes(self):
        p = self.dir / "ttt-x-linux-x86_64.tar.gz"
        make_tar(p, {"ttt-x/ttt": b"\x7fELF program", "ttt-x/README.txt": b"hello", "ttt-x/lang/es.json": b"{}",
                     "ttt-x/LICENSES/imgui.txt": b"MIT", "ttt-x/docs/install.md": b"# install"})
        self.assertEqual(release.check(p, sampled={}), [])

    def test_forbidden_names_refused(self):
        p = self.dir / "ttt-x-linux-x86_64.tar.gz"
        bad = ["ttt-x/ttt.dat", "ttt-x/ttt.dat.disc", "ttt-x/SLUS_200.01", "ttt-x/iso/TEKKEN.BIN", "ttt-x/game.iso",
               "ttt-x/saves/card0.ps2", "ttt-x/textures/a.png", "ttt-x/ttt-update.key", "ttt-x/SDL3.dll",
               "ttt-x/docs/notes.txt", "ttt-x/lang/sub/es.json"]
        make_tar(p, {n: b"x" for n in bad}, links=[("ttt-x/ttt", "/usr/bin/true")])
        problems = "\n".join(release.check(p, sampled={}))
        for n in bad:
            self.assertIn(n, problems)
        self.assertIn("ttt-x/ttt: not a regular file", problems)

    def test_two_top_folders_refused(self):
        p = self.dir / "ttt-x-linux-x86_64.tar.gz"
        make_tar(p, {"a/README.txt": b"1", "b/README.txt": b"2"})
        self.assertTrue(any("top folder" in s for s in release.check(p, sampled={})))

    def test_checkout_path_refused(self):
        p = self.dir / "ttt-x-windows-x86_64.zip"
        with zipfile.ZipFile(p, "w") as z:
            z.writestr("ttt-x/ttt.exe", b"MZ..." + str(ROOT).encode() + b"/third_party/imgui/imgui.cpp\0")
        self.assertTrue(any("checkout's path" in s for s in release.check(p, sampled={})))

    def test_samples_skip_library_tables(self):
        """The powers of ten every C library's number conversion holds are no evidence of the game's data."""
        f = self.dir / "data"
        varied = [tb for tb in release.vendored_tables() if len(set(tb[:release.SAMPLE])) >= release.DISTINCT]
        self.assertNotEqual(varied, [])  # else the test proves nothing
        for tb in varied:
            f.write_bytes(tb[:release.SAMPLE])
            self.assertEqual(release.samples(f), [])
        f.write_bytes(bytes(range(release.SAMPLE)))  # a varied window of no library: sampled
        self.assertEqual(len(release.samples(f)), 1)

    @unittest.skipUnless(release.game_files(), "the game's files are not on this machine (TTT_DISC, iso/, build/)")
    def test_game_bytes_found_inside_a_program(self):
        """A program that embeds 16 KB of the game's executable (or data) is refused, whatever its name."""
        gf = next((p for p in release.game_files() if p.stat().st_size < (64 << 20)), release.game_files()[0])
        with open(gf, "rb") as f:
            f.seek(min(0x10000, gf.stat().st_size // 2))
            chunk = f.read(16384)
        p = self.dir / "ttt-x-linux-x86_64.tar.gz"
        make_tar(p, {"ttt-x/ttt": b"\x7fELF" + b"\0" * 1000 + chunk + b"\0" * 1000})
        problems = release.check(p, sampled={gf: release.samples(gf)})
        self.assertTrue(any("samples of" in s for s in problems), problems)

    @unittest.skipUnless((ROOT / "iso/SLUS_200.01").is_file(), "no iso/SLUS_200.01")
    def test_game_executable_refused_by_hash(self):
        p = self.dir / "ttt-x-windows-x86_64.zip"
        with zipfile.ZipFile(p, "w") as z:
            z.writestr("ttt-x/README.txt", (ROOT / "iso/SLUS_200.01").read_bytes())
        self.assertTrue(any("is a file of the game" in s for s in release.check(p, sampled={})))


def find_archives():
    """{"linux": path, "windows": path} to test: packaged now (TTT_SLOW_TESTS=1) or the current version's in dist/."""
    if SLOW and (ROOT / "build/port/ttt").exists():
        out = Path(tempfile.mkdtemp(prefix="ttt-release-dist-"))
        targets = ["linux"] + (["windows"] if (ROOT / "build/port-win64/ttt.exe").exists() else [])
        r = subprocess.run([sys.executable, str(ROOT / "port/tools/release.py"), "--no-build", "--allow-dirty",
                            "--targets", ",".join(targets), "--out", str(out)], cwd=ROOT, capture_output=True,
                           text=True, timeout=600)
        if r.returncode != 0:
            raise AssertionError("release.py failed:\n" + (r.stdout + r.stderr)[-4000:])
        dist = out
    else:
        dist = ROOT / "dist"
    ver = release.version()
    found = {}
    for target, suffix in (("linux", "linux-x86_64.tar.gz"), ("windows", "windows-x86_64.zip")):
        p = dist / f"ttt-{ver}-{suffix}"
        if p.exists():
            found[target] = p
    return found


ARCHIVES = {}


def setUpModule():
    ARCHIVES.update(find_archives())


class TestArchives(unittest.TestCase):
    def archive(self, target):
        if target not in ARCHIVES:
            self.skipTest(f"no {target} archive of {release.version()} (port/tools/release.py, or TTT_SLOW_TESTS=1)")
        return ARCHIVES[target]

    def check_names(self, target, allowed, required):
        path = self.archive(target)
        files, tops = entries(path)
        self.assertEqual(tops, {path.name.rsplit(".tar.gz", 1)[0].rsplit(".zip", 1)[0]})
        for name in files:
            self.assertRegex(name, f"^{allowed.pattern}$", f"{path.name}: {name} is not an allowed file")
            self.assertNotRegex(name, FORBIDDEN, f"{path.name}: {name} is forbidden")
        self.assertEqual(required - set(files), set(), f"{path.name}: missing files")
        for name, (data, _) in files.items():
            self.assertIsNotNone(data, f"{name}: not a regular file")
            self.assertNotEqual(hashlib.sha1(data).hexdigest(), ELF_SHA1, name)
            self.assertNotEqual(len(data), 2873728, f"{name}: the size of the game's executable")
        self.assertEqual(release.check(path), [])
        return files

    def test_linux_contents(self):
        files = self.check_names("linux", LINUX_ALLOWED, LINUX_REQUIRED)
        for name in ("ttt", "ttt-server", "ttt-sign"):
            self.assertEqual(files[name][1] & 0o777, 0o755, name)
            self.assertTrue(files[name][0].startswith(b"\x7fELF"), name)
        self.assertIn(b"your own disc", files["README.txt"][0])
        self.assertIn(b"glibc 2.34", files["README.txt"][0])

    def test_windows_contents(self):
        files = self.check_names("windows", WINDOWS_ALLOWED, WINDOWS_REQUIRED)
        readme = files["README.txt"][0]
        self.assertIn(b"UNTESTED ON WINDOWS", readme)
        self.assertIn(b"SDL3.dll - NOT included", readme)
        self.assertIn(b"\r\n", readme)
        for name in ("ttt.exe", "ttt-server.exe", "ttt-sign.exe"):
            self.assertTrue(files[name][0].startswith(b"MZ"), name)

    def test_checksums(self):
        for path in ARCHIVES.values():
            sums = path.parent / "SHA256SUMS"
            self.assertTrue(sums.exists(), sums)
            want = hashlib.sha256(path.read_bytes()).hexdigest()
            self.assertIn(f"{want}  {path.name}", sums.read_text())

    def test_linux_portability(self):
        """The programs need glibc 2.34 at most and no library besides glibc's."""
        files, _ = entries(self.archive("linux"))
        with tempfile.TemporaryDirectory(prefix="ttt-release-elf-") as d:
            paths = []
            for name in ("ttt", "ttt-server", "ttt-sign"):
                (Path(d) / name).write_bytes(files[name][0])
                paths.append(Path(d) / name)
            lines, problems = release.portability(paths)
        self.assertEqual(problems, [], "\n".join(lines))


class TestExtractedLinux(unittest.TestCase):
    """The Linux archive extracted into a folder of its own, run as a player would (nothing of the checkout)."""

    @classmethod
    def setUpClass(cls):
        if "linux" not in ARCHIVES:
            raise unittest.SkipTest(f"no linux archive of {release.version()}")
        cls.tmp = tempfile.TemporaryDirectory(prefix="ttt-release-run-")
        base = Path(cls.tmp.name)
        with tarfile.open(ARCHIVES["linux"], "r:gz") as t:
            t.extractall(base / "x", filter="data")
        cls.dir = next((base / "x").iterdir())
        cls.exe = cls.dir / "ttt"

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        self.home = Path(tempfile.mkdtemp(prefix="home-", dir=self.tmp.name))

    def run_ttt(self, *args, timeout=60, **env):
        return subprocess.run([str(self.exe), *args], cwd=self.dir, env=clean_env(self.home, **env),
                              capture_output=True, text=True, timeout=timeout)

    def test_version_and_catalogs(self):
        """--version: the build's version and the languages found in lang/ beside the program; nothing written."""
        r = self.run_ttt("--version")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn(f"ttt {release.version()} (Linux x86-64)", r.stdout)
        langs = re.search(r"^languages: (.*)$", r.stdout, re.M)
        self.assertIsNotNone(langs, r.stdout)
        self.assertEqual(langs.group(1).split(), ["en", "es"])
        self.assertEqual(list(self.home.rglob("*")), [])

    def test_catalogs_only_from_beside_the_program(self):
        """Without lang/ beside it, only English: the catalogs above came from the release's folder."""
        lang = self.dir / "lang"
        hidden = self.dir / "lang.off"
        lang.rename(hidden)
        try:
            r = self.run_ttt("--version")
        finally:
            hidden.rename(lang)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("languages: en\n", r.stdout)

    def test_hw_selftest(self):
        r = self.run_ttt(TTT_SELFTEST="hw")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("hw self-test OK", r.stdout)

    def test_first_run_without_disc(self):
        """No disc, no data, no window: the refusal on stderr and exit code 2, not a crash."""
        r = self.run_ttt(TTT_WINDOW="0", timeout=120)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn("ttt: no disc: run `ttt --setup <disc>`", r.stderr)
        self.assertFalse((self.home / "data/ttt/ttt.dat").exists())

    def test_setup_without_a_disc(self):
        empty = self.home / "empty"
        empty.mkdir()
        r = self.run_ttt("--setup", str(empty))
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn("no Tekken Tag Tournament disc at", r.stderr)

    @unittest.skipUnless(SLOW and (ROOT / "iso/TEKKEN.BIN").is_file(), "slow, and needs the disc in iso/")
    def test_setup_then_idle_run(self):
        """The player's way: `ttt --setup <disc>`, then the idle run ends in the reference hash."""
        r = self.run_ttt("--setup", str(ROOT / "iso"), timeout=300)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertTrue((self.home / "data/ttt/ttt.dat").exists())
        dumps = self.home / "dumps"
        dumps.mkdir()
        r = self.run_ttt(timeout=1200, TTT_FRAMES="2551", TTT_SEED="1", TTT_DUMP=str(dumps), TTT_DUMP_EVERY="150",
                         TTT_MOVIES="0", TTT_LANG="en")
        self.assertEqual(r.returncode, 0, (r.stdout + r.stderr)[-3000:])
        self.assertIn(f"hash {IDLE_HASH} ", r.stdout)
        self.assertEqual(len(list(dumps.iterdir())), 17)


if __name__ == "__main__":
    unittest.main()
