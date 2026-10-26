"""Rollback netplay (T45, port/src/plat_net.c, docs/port/netplay.md): two copies of the built program build/port/ttt
over UDP on the loopback interface (127.0.0.1 only: nothing else is contacted), each playing its own pad from a
script (TTT_INPUT_PLAY). Skipped when the program has not been built or no disc data is found."""

import os
import re
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

from test_port_input import IDLE, IN_THE_FIGHT, TO_THE_FIGHT
from test_port_platform import EXE, disc, run_ttt
from test_port_window import NO_SDL, SDL_LIBS

START, LEFT, SQUARE = 0x0800, 0x8000, 0x0080
# The joining side (pad 2) in round 1: back, a punch, then Start (a challenger: the game goes to the select).
SIDE2 = ((1250, LEFT, 20), (1300, SQUARE, 4), (1340, START, 6))
FRAMES = "1451"


def two_pads(pad1=(), pad2=()):
    """A recording with both pads: each press is (vblank, buttons, VBlanks held)."""
    ev = sorted([(v, 0, b) for v, b, n in pad1] + [(v + n, 0, 0) for v, b, n in pad1] +
                [(v, 1, b) for v, b, n in pad2] + [(v + n, 1, 0) for v, b, n in pad2])
    cur, lines = [0, 0], ["ttt-input 1 pads 2"]
    for v, p, b in ev:
        cur[p] = b
        lines.append(f"{v} {cur[0]:04x} {IDLE} {cur[1]:04x} {IDLE}")
    return "\n".join(lines) + "\n"


def state_hash(text):
    m = re.search(r"ttt: state: .* hash (0x[0-9a-f]+)", text)
    return m.group(1) if m else None


def net_stat(text, pattern):
    m = re.search(pattern, text)
    return int(m.group(1)) if m else None


def run_pair(common=None, host=None, join=None, timeout=600):
    """A host (TTT_NET_HOST=0: a free port on 127.0.0.1) and a side joining it, each with run_ttt's private folders.
    Returns ((returncode, output) of the host, (returncode, output) of the joining side)."""
    def env(d, extra):
        e = {k: v for k, v in os.environ.items() if not k.startswith(("TTT_", "SDL_"))}
        e.update({"TTT_DISC": str(disc()), "TTT_MOVIES": "0", "TTT_SAVES": d, "TTT_CONFIG": d + "/settings.ini",
                  "TTT_TEXTURES": d + "/textures", "TTT_MODS": d + "/mods", "XDG_DATA_HOME": d + "/data",
                  "XDG_CONFIG_HOME": d + "/config", "TTT_NET_WAIT": "30"})
        e.update(common or {})
        e.update(extra)
        return e

    with tempfile.TemporaryDirectory(prefix="ttt-net-h-") as dh, tempfile.TemporaryDirectory(prefix="ttt-net-j-") as dj:
        h = subprocess.Popen([str(EXE)], env=env(dh, {"TTT_NET_HOST": "0", **(host or {})}), stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
        out_h, port = [], None
        for line in h.stdout:
            out_h.append(line)
            m = re.search(r"ttt: net: listening on 127\.0\.0\.1:(\d+)", line)
            if m:
                port = m.group(1)
                break
        if port is None:
            h.wait(timeout)
            h.stdout.close()
            return (h.returncode, "".join(out_h)), (None, "")
        rest = threading.Thread(target=lambda: out_h.extend(h.stdout))
        rest.start()
        try:
            j = subprocess.run([str(EXE)], env=env(dj, {"TTT_NET_JOIN": f"127.0.0.1:{port}", **(join or {})}),
                               capture_output=True, text=True, timeout=timeout)
            h.wait(timeout)
        finally:
            if h.poll() is None:
                h.kill()
        rest.join()
        h.stdout.close()
        return (h.returncode, "".join(out_h)), (j.returncode, j.stdout + j.stderr)


def scripts(d):
    """Each side's script: pad 1 from the main menu to round 1 and in it (the host), pad 2 in round 1 (joining)."""
    p1, p2 = Path(d) / "pad1.txt", Path(d) / "pad2.txt"
    p1.write_text(two_pads(TO_THE_FIGHT + IN_THE_FIGHT, ()))
    p2.write_text(two_pads((), SIDE2))
    return str(p1), str(p2)


class Session(unittest.TestCase):
    def assert_session(self, h, j):
        self.assertEqual(h[0], 0, h[1][-3000:])
        self.assertEqual(j[0], 0, j[1][-3000:])
        self.assertIn("ttt: net: playing as pad 1", h[1])
        self.assertIn("ttt: net: playing as pad 2", j[1])
        self.assertEqual(state_hash(h[1]), state_hash(j[1]))
        self.assertEqual(net_stat(h[1], r"(\d+) desyncs"), 0)
        self.assertEqual(net_stat(j[1], r"(\d+) desyncs"), 0)
        self.assertGreater(net_stat(h[1], r"ttt: net: (\d+) checks"), 15)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Netplay(Session):
    def test_refused_when_the_settings_differ(self):
        """The handshake compares what changes the game: a joining side with the unlock cheat on is refused by a host
        without it (exit code 4), and the host keeps waiting for another (exit code 3 at TTT_NET_WAIT)."""
        h, j = run_pair(common={"TTT_FRAMES": "100", "TTT_NET_DRAW": "0", "TTT_NET_WAIT": "5"},
                        join={"TTT_CHEAT_UNLOCK": "1"}, timeout=120)
        self.assertEqual(j[0], 4, j[1][-2000:])
        self.assertIn("ttt: net: the host refused: other settings", j[1])
        self.assertIn("ttt: net: refused a peer", h[1])
        self.assertEqual(h[0], 3, h[1][-2000:])

    def test_mods_are_shown_never_compared(self):
        """T49: each side's mods (Mods_Summary) travel in HELLO / WELCOME as information: a joining side with a texture
        mod plays with a host without one, and each prints the other's."""
        with tempfile.TemporaryDirectory(prefix="ttt-net-mods-") as m:
            (Path(m) / "shiny" / "textures").mkdir(parents=True)
            h, j = run_pair(common={"TTT_FRAMES": "100", "TTT_NET_DRAW": "0"}, join={"TTT_MODS": m}, timeout=180)
        self.assertEqual(h[0], 0, h[1][-2000:])
        self.assertEqual(j[0], 0, j[1][-2000:])
        self.assertIn("ttt: net: mods: music 0 tracks, textures 1 mod (shiny)", j[1])
        self.assertIn("ttt: net: the peer's mods: music 0 tracks, textures 1 mod (shiny)", h[1])
        self.assertIn("ttt: net: the peer's mods: music 0 tracks, textures 0 mods", j[1])

    def test_sessions_match_each_other_and_the_offline_replay(self):
        """Both sides play their own pad (pad 1 from the main menu to round 1 and in it, pad 2 in round 1) over
        loopback, undrawn and as fast as both go: once clean, once with 30 +- 10 ms of delay and 5 % loss each way and
        the last 150 VBlanks paced at 1x. Both sides of both sessions end in the same state, the second after rollbacks
        on both sides, every fingerprint check agrees, and the joining side's recording of the final inputs
        (TTT_INPUT_RECORD) replayed offline ends in that state too. About 2 minutes."""
        with tempfile.TemporaryDirectory(prefix="ttt-net-") as d:
            p1, p2 = scripts(d)
            rec = str(Path(d) / "rec.txt")
            common = {"TTT_FRAMES": FRAMES, "TTT_SEED": "1", "TTT_NET_DRAW": "0", "TTT_NET_PACE_FROM": "0"}
            clean = run_pair(common, {"TTT_INPUT_PLAY": p1}, {"TTT_INPUT_PLAY": p2, "TTT_INPUT_RECORD": rec})
            self.assert_session(*clean)
            lossy = run_pair({**common, "TTT_NET_SIM": "delay:30,jitter:10,loss:5", "TTT_NET_PACE_FROM": "1300"},
                             {"TTT_INPUT_PLAY": p1}, {"TTT_INPUT_PLAY": p2})
            self.assert_session(*lossy)
            self.assertEqual(state_hash(lossy[0][1]), state_hash(clean[0][1]))
            self.assertGreater(net_stat(lossy[0][1], r"ttt: net: rollbacks (\d+)"), 0)
            self.assertGreater(net_stat(lossy[1][1], r"ttt: net: rollbacks (\d+)"), 0)
            self.assertGreater(net_stat(lossy[0][1], r"(\d+) dropped on purpose"), 0)
            offline = run_ttt(timeout=900, TTT_FRAMES=FRAMES, TTT_SEED="1", TTT_MC0="none", TTT_INPUT_PLAY=rec)
        self.assertEqual(offline.returncode, 0, offline.stdout[-2000:])
        self.assertEqual(state_hash(offline.stdout), state_hash(clean[0][1]))

    def test_a_desync_is_reported(self):
        """TTT_NET_PERTURB flips a bit of the joining side's state at VBlank 1200: the next fingerprint check (1200)
        differs, both sides report it with the VBlank and the region (EE RAM) and stop (exit code 5)."""
        with tempfile.TemporaryDirectory(prefix="ttt-net-") as d:
            p1, p2 = scripts(d)
            h, j = run_pair({"TTT_FRAMES": FRAMES, "TTT_SEED": "1", "TTT_NET_DRAW": "0", "TTT_NET_PACE_FROM": "0",
                             "TTT_NET_SIM": "delay:10,loss:3"},
                            {"TTT_INPUT_PLAY": p1}, {"TTT_INPUT_PLAY": p2, "TTT_NET_PERTURB": "1200"})
        for rc, out in (h, j):
            self.assertEqual(rc, 5, out[-3000:])
            self.assertIn("ttt: net: desync at vblank 1200: state hash", out)
            self.assertIn("ttt: net: stopped: the two sides' states differ (vblank 1200", out)
        self.assertRegex(h[1] + j[1], r"desync at vblank 1200: region ee\.ram differs")


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(any(p.exists() for p in SDL_LIBS), NO_SDL)
class NetplayWindow(Session):
    def test_drawn_in_hidden_windows(self):
        """The same session drawn with GL in two hidden windows (SDL's offscreen driver, no sound), with 20 ms of
        delay each way, round 1 paced at 1x: the frames run again stay undrawn and both sides end in the state of the
        undrawn sessions. About a minute."""
        with tempfile.TemporaryDirectory(prefix="ttt-net-") as d:
            p1, p2 = scripts(d)
            win = {"TTT_FRAMES": FRAMES, "TTT_SEED": "1", "TTT_NET_PACE_FROM": "1101", "TTT_NET_SIM": "delay:20",
                   "TTT_WINDOW": "1", "TTT_WINDOW_HIDDEN": "1", "TTT_RENDERER": "gl", "TTT_AUDIO": "0", "TTT_VSYNC": "0",
                   "SDL_VIDEO_DRIVER": "offscreen"}
            h, j = run_pair(win, {"TTT_INPUT_PLAY": p1}, {"TTT_INPUT_PLAY": p2})
            if "OpenGL renderer" not in h[1]:
                self.skipTest("no OpenGL 3.3 context here: " + h[1][-500:])
            undrawn = run_pair({"TTT_FRAMES": FRAMES, "TTT_SEED": "1", "TTT_NET_DRAW": "0", "TTT_NET_PACE_FROM": "0"},
                               {"TTT_INPUT_PLAY": p1}, {"TTT_INPUT_PLAY": p2})
        self.assert_session(h, j)
        self.assertEqual(state_hash(h[1]), state_hash(undrawn[0][1]))
        self.assertGreater(net_stat(j[1], r"ttt: net: rollbacks (\d+)"), 0)


if __name__ == "__main__":
    unittest.main()
