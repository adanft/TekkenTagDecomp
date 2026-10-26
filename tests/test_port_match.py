"""Matchmaking (T46, port/server/ttt-server.c, port/src/plat_net.c, docs/port/matchmaking.md), on the loopback
interface only: the server listens on 127.0.0.1 and nothing else is contacted.

- The server's protocol, from small Python clients (no game needed): rooms by code, the version check, the quick
  match, the relay, the manifest's anti-amplification rule.
- The manifest signer (port/server/ttt-sign.c) against PyNaCl (libsodium) when it is installed, and the game's
  Ed25519 / SHA-512 self-test (RFC 8032's first vector, TTT_SELFTEST=net).
- Two copies of the built game paired by the server: a room's code with a direct path and the chat both ways, the
  quick match forced through the relay, a joining side of another build refused, the update check.

The server and the signer are built here (port/tools/build_server.py, a few seconds). Test keys are made at run time
and never kept."""

import os
import re
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

from test_port_platform import EXE, ROOT, disc, run_ttt

SERVER_DIR = ROOT / "build/server"
SERVER, SIGN = SERVER_DIR / "ttt-server", SERVER_DIR / "ttt-sign"
MAGIC, VERSION = 0x534D5454, 1
CREATE, JOIN, QUICK, LEAVE, MANIFEST = 1, 2, 3, 4, 5
S_WAIT, S_MATCH, S_ERROR, S_MANIFEST = 0x10, 0x11, 0x12, 0x13
CODE_CHARS = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"

try:
    import nacl.signing  # PyNaCl (libsodium): the reference for the signatures
except ImportError:  # pragma: no cover
    nacl = None


def setUpModule():
    r = subprocess.run([sys.executable, str(ROOT / "port/tools/build_server.py")], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("build_server.py failed: " + r.stdout + r.stderr)


class Server:
    """ttt-server on 127.0.0.1, a free port; stopped (SIGTERM) on exit. `out` holds its stdout once stopped."""

    def __init__(self, *args):
        self.args, self.out = list(args), ""

    def __enter__(self):
        self.p = subprocess.Popen([str(SERVER), "--listen", "127.0.0.1:0", *self.args], stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True)
        line = self.p.stdout.readline()
        m = re.search(r"listening on 127\.0\.0\.1:(\d+)", line)
        if not m:
            self.p.kill()
            raise RuntimeError("the server did not start: " + line)
        self.port = int(m.group(1))
        self.lines = [line]
        self.reader = threading.Thread(target=lambda: self.lines.extend(self.p.stdout))
        self.reader.start()
        return self

    def __exit__(self, *exc):
        self.p.terminate()
        try:
            self.p.wait(10)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()
        self.reader.join()
        self.p.stdout.close()
        self.out = "".join(self.lines)


class Client:
    """A made-up game: one UDP socket on 127.0.0.1, its cookie."""

    def __init__(self, server, cookie, build=0x1234, name="Py"):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.bind(("127.0.0.1", 0))
        self.s.settimeout(2.0)
        self.server, self.cookie, self.build, self.name = ("127.0.0.1", server.port), cookie, build, name

    def close(self):
        self.s.close()

    def request(self, kind, code="", size=160):
        local = bytes([4, 0]) + struct.pack("<H", self.s.getsockname()[1]) + socket.inet_aton("127.0.0.1") + bytes(12)
        p = struct.pack("<IBBHI", MAGIC, kind, VERSION, 0, self.cookie) + struct.pack("<IQ", 1, self.build) + local
        p += code.encode().ljust(8, b"\0") + self.name.encode().ljust(32, b"\0")
        self.s.sendto(p.ljust(size, b"\0"), self.server)

    def recv(self):
        try:
            d, a = self.s.recvfrom(4096)
        except socket.timeout:
            return None, None
        return d, a

    def answer(self, kind=None):
        """The next server message (type, body), skipping other kinds when one is asked for."""
        end = time.time() + 2
        while time.time() < end:
            d, a = self.recv()
            if d is None:
                return None, b""
            if len(d) >= 12 and struct.unpack_from("<I", d)[0] == MAGIC and (kind is None or d[4] == kind):
                return d[4], d
        return None, b""


def ep(d, off):
    """An endpoint of the protocol: (family, port, IPv4 text)."""
    return d[off], struct.unpack_from("<H", d, off + 2)[0], socket.inet_ntoa(d[off + 4:off + 8])


def text(d, off, n):
    return d[off:off + n].split(b"\0")[0].decode()


@unittest.skipUnless(sys.platform.startswith("linux"), "the server and these tests run on Linux")
class Protocol(unittest.TestCase):
    def test_rooms_and_the_version_check(self):
        """A room gets a 6-character code; a game of another build joining it is refused (MS_ERR_VERSION) and the
        room stays open; a game of the same build joins (the code in lower case) and both get MATCH: the room's game
        is the host, each gets the other's address, local address and name, and the same session id."""
        with Server("--verbose") as srv:
            a, b, c = Client(srv, 11, name="Ana"), Client(srv, 22, name="Bea"), Client(srv, 33, build=0x9999)
            try:
                a.request(CREATE)
                kind, w = a.answer()
                self.assertEqual(kind, S_WAIT)
                code = text(w, 12, 8)
                self.assertRegex(code, f"^[{CODE_CHARS}]{{6}}$")
                self.assertEqual(ep(w, 24), (4, a.s.getsockname()[1], "127.0.0.1"))
                c.request(JOIN, code)
                kind, e = c.answer()
                self.assertEqual(kind, S_ERROR)
                self.assertEqual(struct.unpack_from("<I", e, 12)[0], 3)
                self.assertIn("another build", text(e, 16, 64))
                b.request(JOIN, code.lower())
                ka, ma = a.answer(S_MATCH)
                kb, mb = b.answer(S_MATCH)
                self.assertEqual((ka, kb), (S_MATCH, S_MATCH))
                self.assertEqual((ma[12], mb[12]), (0, 1))
                self.assertEqual(ma[16:20], mb[16:20])
                self.assertEqual(ep(ma, 20)[1], b.s.getsockname()[1])
                self.assertEqual(ep(mb, 20)[1], a.s.getsockname()[1])
                self.assertEqual((text(ma, 60, 32), text(mb, 60, 32)), ("Bea", "Ana"))
                self.assertEqual(text(mb, 92, 8), code)
                b.request(JOIN, code)  # (MATCH lost: asked again, sent again)
                self.assertEqual(b.answer(S_MATCH)[0], S_MATCH)
                b.request(JOIN, "ZZZZZ9")
                self.assertEqual(b.answer()[0], S_MATCH)  # (a paired game is answered with its pair)
                c.request(JOIN, "ZZZZZ9")
                kind, e = c.answer()
                self.assertEqual((kind, struct.unpack_from("<I", e, 12)[0]), (S_ERROR, 1))
            finally:
                for x in (a, b, c):
                    x.close()
        self.assertIn("1 pairs made", srv.out)

    def test_quick_match_and_the_relay(self):
        """The quick-match queue pairs games of the same build (the one of another build keeps waiting); then a
        datagram that is not the server's protocol, from one game of the pair, reaches the other one from the server's
        address, and one from an address the server does not know goes nowhere. LEAVE ends the pair."""
        with Server() as srv:
            a, b, x = Client(srv, 1, build=7), Client(srv, 2, build=8), Client(srv, 3, build=7)
            try:
                a.request(QUICK)
                self.assertEqual(a.answer()[0], S_WAIT)
                b.request(QUICK)
                kind, w = b.answer()
                self.assertEqual((kind, struct.unpack_from("<I", w, 20)[0]), (S_WAIT, 2))
                x.request(QUICK)
                kx, mx = x.answer(S_MATCH)
                ka, ma = a.answer(S_MATCH)
                self.assertEqual((kx, ka), (S_MATCH, S_MATCH))
                self.assertEqual((ma[12], mx[12], ma[13]), (0, 1, 1))  # the one that waited hosts; relay on
                a.s.sendto(b"TTN1 hello through the relay", x.server)
                d, src = x.recv()
                self.assertEqual(d, b"TTN1 hello through the relay")
                self.assertEqual(src, x.server)
                b.s.sendto(b"TTN1 not paired", b.server)
                self.assertEqual(a.recv()[0], None)
                self.assertEqual(x.recv()[0], None)
                a.request(LEAVE)
                time.sleep(0.2)
                x.s.sendto(b"TTN1 after leaving", x.server)
                self.assertEqual(a.recv()[0], None)
            finally:
                for c in (a, b, x):
                    c.close()
        self.assertRegex(srv.out, r"1 pairs made, \d+ bytes relayed")

    def test_no_relay(self):
        """--no-relay: MATCH says so, and nothing is forwarded."""
        with Server("--no-relay") as srv:
            a, b = Client(srv, 1), Client(srv, 2)
            try:
                a.request(QUICK)
                b.request(QUICK)
                kb, mb = b.answer(S_MATCH)
                self.assertEqual((kb, mb[13]), (S_MATCH, 0))
                b.s.sendto(b"TTN1 x", b.server)
                self.assertEqual(a.answer(S_MATCH)[0], S_MATCH)
                self.assertEqual(a.recv()[0], None)
            finally:
                a.close()
                b.close()

    def test_manifest_answers_are_never_larger_than_requests(self):
        """A manifest request shorter than MS_MANIFEST_REQ_SIZE gets no answer (a forged sender could not use the
        server to send others more than it sent); a full-size one gets the manifest, byte for byte, or MS_ERR_NO_MANIFEST
        when there is none."""
        with tempfile.TemporaryDirectory(prefix="ttt-match-") as d:
            m = Path(d) / "manifest.txt"
            m.write_bytes(b"ttt-update 1\nversion 1.0\nbuild 0000000000000001\n")
            with Server("--manifest", str(m)) as srv:
                a = Client(srv, 5)
                try:
                    a.request(MANIFEST, size=200)
                    self.assertEqual(a.answer()[0], None)
                    a.request(MANIFEST, size=1232)
                    kind, d = a.answer()
                    self.assertEqual(kind, S_MANIFEST)
                    self.assertLessEqual(len(d), 1232)
                    n = struct.unpack_from("<H", d, 12)[0]
                    self.assertEqual(d[14:14 + n], m.read_bytes())
                finally:
                    a.close()
        with Server() as srv:
            a = Client(srv, 6)
            try:
                a.request(MANIFEST, size=1232)
                kind, d = a.answer()
                self.assertEqual((kind, struct.unpack_from("<I", d, 12)[0]), (S_ERROR, 6))
            finally:
                a.close()


@unittest.skipUnless(sys.platform.startswith("linux"), "the signer is built and run on Linux here")
class Signatures(unittest.TestCase):
    def test_sign_and_verify(self):
        """ttt-sign: a key made, a manifest signed and verified; one changed byte, or another key, fails. With PyNaCl:
        ttt-sign's public key and signature are libsodium's for the same seed, and a libsodium signature verifies."""
        with tempfile.TemporaryDirectory(prefix="ttt-sign-") as d:
            key, src, out = Path(d) / "key", Path(d) / "m.txt", Path(d) / "m.signed"
            pk = subprocess.run([str(SIGN), "keygen", str(key)], capture_output=True, text=True, check=True).stdout.strip()
            self.assertRegex(pk, r"^[0-9a-f]{64}$")
            self.assertEqual(oct(key.stat().st_mode & 0o777), "0o600")
            self.assertNotEqual(subprocess.run([str(SIGN), "keygen", str(key)], capture_output=True).returncode, 0)
            src.write_text("ttt-update 1\nversion 2.0\nbuild 0123456789abcdef\nurl example.invalid/ttt\n")
            subprocess.run([str(SIGN), "sign", str(key), str(src), str(out)], check=True)
            ok = subprocess.run([str(SIGN), "verify", pk, str(out)], capture_output=True, text=True)
            self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
            bad = out.read_bytes().replace(b"2.0", b"2.1")
            (Path(d) / "bad").write_bytes(bad)
            self.assertEqual(subprocess.run([str(SIGN), "verify", pk, str(Path(d) / "bad")],
                                            capture_output=True).returncode, 1)
            other = "11" * 32
            self.assertEqual(subprocess.run([str(SIGN), "verify", other, str(out)], capture_output=True).returncode, 1)
            if nacl is None:
                self.skipTest("PyNaCl not installed: no cross-check against libsodium")
            seed = bytes.fromhex(key.read_text().strip())
            sk = nacl.signing.SigningKey(seed)
            self.assertEqual(sk.verify_key.encode().hex(), pk)
            body = src.read_bytes()
            sig = re.search(rb"signature ([0-9a-f]{128})", out.read_bytes()).group(1).decode()
            self.assertEqual(sk.sign(body).signature.hex(), sig)  # Ed25519 is deterministic
            for i in range(8):
                k2 = nacl.signing.SigningKey(os.urandom(32))
                msg = body + f"notes run {i}\n".encode()
                (Path(d) / "x").write_bytes(msg + b"signature " + k2.sign(msg).signature.hex().encode() + b"\n")
                r = subprocess.run([str(SIGN), "verify", k2.verify_key.encode().hex(), str(Path(d) / "x")],
                                   capture_output=True)
                self.assertEqual(r.returncode, 0)


def game_env(d, port, extra):
    e = {k: v for k, v in os.environ.items() if not k.startswith(("TTT_", "SDL_"))}
    e.update({"TTT_DISC": str(disc()), "TTT_MOVIES": "0", "TTT_LANG": "en", "TTT_SAVES": d,
              "TTT_CONFIG": d + "/settings.ini", "TTT_TEXTURES": d + "/textures", "TTT_MODS": d + "/mods",
              "XDG_DATA_HOME": d + "/data", "XDG_CONFIG_HOME": d + "/config", "TTT_NET_SERVER": f"127.0.0.1:{port}",
              "TTT_NET_WAIT": "60",
              "TTT_FRAMES": "600", "TTT_SEED": "1", "TTT_NET_DRAW": "0", "TTT_NET_PACE_FROM": "0"})
    e.update(extra)
    return e


def run_match(server, host, join, timeout=600, host_first=r"room ([A-Z0-9]{6}):|waiting in the quick-match queue"):
    """The first game (`host` variables) until the server has it waiting, then the second; both to the end.
    Returns ((rc, output) of the first, (rc, output) of the second). A joining side's TTT_NET_ROOM of "{code}" is
    the room's code."""
    with tempfile.TemporaryDirectory(prefix="ttt-match-h-") as dh, tempfile.TemporaryDirectory(prefix="ttt-match-j-") as dj:
        h = subprocess.Popen([str(EXE)], env=game_env(dh, server.port, host), stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
        out_h, m = [], None
        for line in h.stdout:
            out_h.append(line)
            m = re.search(host_first, line)
            if m or "ttt: net: stopped" in line:
                break
        if m is None:
            h.wait(timeout)
            h.stdout.close()
            return (h.returncode, "".join(out_h)), (None, "")
        rest = threading.Thread(target=lambda: out_h.extend(h.stdout))
        rest.start()
        try:
            join = {k: v.replace("{code}", m.group(1) or "") for k, v in join.items()}
            j = subprocess.run([str(EXE)], env=game_env(dj, server.port, join), capture_output=True, text=True,
                               timeout=timeout)
            h.wait(timeout)
        finally:
            if h.poll() is None:
                h.kill()
                h.wait()
        rest.join()
        h.stdout.close()
        return (h.returncode, "".join(out_h)), (j.returncode, j.stdout + j.stderr)


def state_hash(out):
    m = re.search(r"ttt: state: .* hash (0x[0-9a-f]+)", out)
    return m.group(1) if m else None


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class GameCrypto(unittest.TestCase):
    def test_selftest(self):
        """The game's SHA-512 and Ed25519 (third_party/tweetnacl/ed25519.h) on RFC 8032's first vector, a manifest
        signed and verified, then refused with one byte changed (TTT_SELFTEST=net)."""
        r = run_ttt(TTT_SELFTEST="net")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: net self-test OK", r.stdout)

    def test_update_check(self):
        """TTT_UPDATE_CHECK=1: the server's manifest, signed with the key in TTT_NET_UPDATE_KEY, names a newer build
        (exit 0, nothing downloaded); with another key it is refused (exit 4); without a manifest, exit 3."""
        with tempfile.TemporaryDirectory(prefix="ttt-update-") as d:
            key, src, signed = Path(d) / "key", Path(d) / "m.txt", Path(d) / "m.signed"
            pk = subprocess.run([str(SIGN), "keygen", str(key)], capture_output=True, text=True,
                                check=True).stdout.strip()
            src.write_text("ttt-update 1\nversion 9.8.7\nbuild 00000000000000ff\nurl example.invalid/ttt\n")
            subprocess.run([str(SIGN), "sign", str(key), str(src), str(signed)], check=True)
            with Server("--manifest", str(signed)) as srv:
                good = run_ttt(TTT_UPDATE_CHECK="1", TTT_NET_SERVER=f"127.0.0.1:{srv.port}", TTT_NET_UPDATE_KEY=pk)
                bad = run_ttt(TTT_UPDATE_CHECK="1", TTT_NET_SERVER=f"127.0.0.1:{srv.port}",
                              TTT_NET_UPDATE_KEY="ab" * 32)
            with Server() as srv:
                none = run_ttt(TTT_UPDATE_CHECK="1", TTT_NET_SERVER=f"127.0.0.1:{srv.port}", TTT_NET_UPDATE_KEY=pk)
        self.assertEqual(good.returncode, 0, good.stdout)
        self.assertIn("ttt: update: version 9.8.7 is available (build 00000000000000ff): example.invalid/ttt; "
                      "nothing is downloaded", good.stdout)
        self.assertEqual(bad.returncode, 4, bad.stdout)
        self.assertIn("not signed with net.update_key: ignored", bad.stdout)
        self.assertEqual(none.returncode, 3, none.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(sys.platform.startswith("linux"), "the server runs on Linux here")
class Sessions(unittest.TestCase):
    def assert_session(self, h, j):
        self.assertEqual(h[0], 0, h[1][-3000:])
        self.assertEqual(j[0], 0, j[1][-3000:])
        self.assertIn("ttt: net: playing as pad 1", h[1])
        self.assertIn("ttt: net: playing as pad 2", j[1])
        self.assertIsNotNone(state_hash(h[1]))
        self.assertEqual(state_hash(h[1]), state_hash(j[1]))
        for out in (h[1], j[1]):
            self.assertRegex(out, r" 0 desyncs")
            self.assertGreater(int(re.search(r"ttt: net: (\d+) checks", out).group(1)), 5)

    def test_room_code_direct_with_chat(self):
        """A room's code: the joining side reaches the host directly (hole punching on loopback), both play 600
        VBlanks to the same state, and the chat goes both ways (acknowledged; UTF-8 kept, control characters not)."""
        with Server("--verbose") as srv:
            h, j = run_match(srv, {"TTT_NET_CREATE": "1", "TTT_NET_NAME": "Ana", "TTT_NET_CHAT": "hi Bea"},
                             {"TTT_NET_ROOM": "{code}", "TTT_NET_NAME": "Bea", "TTT_NET_CHAT": "hola|¿qué\ttal?"})
        self.assert_session(h, j)
        self.assertIn("ttt: net: path: direct to 127.0.0.1:", j[1])
        self.assertNotIn("relayed", h[1])
        self.assertIn("ttt: net: paired with Bea (pad 2;", h[1])
        self.assertIn("ttt: net: chat from Bea: hola\n", h[1])
        self.assertIn("ttt: net: chat from Bea: ¿qué tal?\n", h[1])
        self.assertIn("ttt: net: chat from Ana: hi Bea\n", j[1])
        self.assertIn("chat 2 sent (2 acknowledged), 1 received", j[1])
        self.assertIn("chat 1 sent (1 acknowledged), 2 received", h[1])
        self.assertRegex(srv.out, r"relayed 0 / 0 bytes")

    def test_quick_match_forced_relay(self):
        """The quick match with hole punching off on both sides (TTT_NET_PUNCH=0): every packet goes through the
        server's relay, and the session ends in the same state on both sides."""
        with Server("--verbose") as srv:
            h, j = run_match(srv, {"TTT_NET_QUICK": "1", "TTT_NET_PUNCH": "0"},
                             {"TTT_NET_QUICK": "1", "TTT_NET_PUNCH": "0"})
        self.assert_session(h, j)
        self.assertIn("ttt: net: path: relayed by the server 127.0.0.1:", j[1])
        self.assertIn("joined (relayed by the server)", h[1])
        self.assertIn("path relayed by the server; peer Player", h[1])
        relayed = re.search(r"(\d+) bytes relayed", srv.out)
        self.assertGreater(int(relayed.group(1)), 10000, srv.out[-2000:])

    def test_another_build_is_refused(self):
        """A joining side whose build differs (TTT_NET_TEST_BUILD) is refused by the server before any packet goes
        to the host (exit code 4); the host keeps its room until TTT_NET_WAIT (exit code 3)."""
        with Server() as srv:
            h, j = run_match(srv, {"TTT_NET_CREATE": "1", "TTT_NET_WAIT": "8"},
                             {"TTT_NET_ROOM": "{code}", "TTT_NET_TEST_BUILD": "123456789abcdef0"}, timeout=120)
        self.assertEqual(j[0], 4, j[1][-2000:])
        self.assertIn("ttt: net: the server refused: the room's game is another build", j[1])
        self.assertEqual(h[0], 3, h[1][-2000:])
        self.assertIn("ttt: net: nobody came within 8 s", h[1])


if __name__ == "__main__":
    unittest.main()
