"""The PC build's pads (port/src/plat_pad.c: libpad fed once per VBlank, TTT_INPUT_PLAY / TTT_INPUT_RECORD), checked on
the built program build/port/ttt (made from your own SLUS_200.01 by port/tools/build.py). Skipped when the program has
not been built; the replay runs are also skipped without disc data."""

import os
import re
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt
from test_port_window import png_rgb

IDLE = "80808080"


def stream(*presses, pads=2):
    """A recording: each press is (vblank, buttons of pad 1, VBlanks held)."""
    lines = [f"ttt-input 1 pads {pads}"]
    for v, b, n in presses:
        lines.append(f"{v} {b:04x} {IDLE} 0000 {IDLE}")
        lines.append(f"{v + n} 0000 {IDLE} 0000 {IDLE}")
    return "\n".join(lines) + "\n"


def state(r):
    m = re.search(r"ttt: state: .*", r.stdout)
    return m.group(0) if m else None


def peek(r, addr):
    """The words TTT_PEEK printed for addr (hex string as passed) at the end of the run."""
    m = re.search(rf"ttt: peek {int(addr, 16):08X}:((?: [0-9A-F]{{8}})+)", r.stdout)
    return [int(w, 16) for w in m.group(1).split()] if m else None


START, RIGHT, DOWN, SQUARE, TRIANGLE, CROSS = 0x0800, 0x2000, 0x4000, 0x0080, 0x0010, 0x0040
# Start at the main menu (ARCADE), then at the character select: right, down, square (Yoshimitsu), right, square
# (Nina, the tag partner). The stage loads and stage 1 is fought (mode 4, sub-state 8) by VBlank 830.
TO_THE_FIGHT = ((200, START, 6), (330, RIGHT, 4), (360, DOWN, 4), (400, SQUARE, 4), (440, RIGHT, 4),
                (470, SQUARE, 4))
# In round 1: forward for 100 VBlanks, then left punch, right punch, left kick.
IN_THE_FIGHT = ((1150, RIGHT, 100), (1300, SQUARE, 4), (1340, TRIANGLE, 4), (1380, CROSS, 4))
SEL_SIDE0 = "440D08"  # SelState side 0 (D_00440CE8 + 0x20): state, cursor cell, ..., character at +0x10
MODE = "5F5C20"  # gMode (s16), gSubMode (s16)
FIGHTER0 = "879644"  # gFighters[0].unk4 / unk8 / unkC: the position
FIGHTER1 = "884EE4"  # gFighters[1], the same


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
class PadSelfTest(unittest.TestCase):
    def test_pad(self):
        """The recording's line format, libpad's read data (kind 0x79, buttons active low, sticks, pressure bytes) and
        the answers that take the game's set-up sequence to a ready DualShock 2 on ports 0 and 1."""
        r = run_ttt(TTT_SELFTEST="pad")
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("ttt: pad self-test OK", r.stdout)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Replay(unittest.TestCase):
    FRAMES = "320"
    START = 0x0800

    def play(self, text, **env):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(text)
            rec = Path(d) / "rec.txt"
            r = run_ttt(TTT_FRAMES=self.FRAMES, TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_INPUT_RECORD=str(rec), **env)
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            return r, rec.read_text()

    def test_headless_runs_have_no_pad(self):
        """Without TTT_INPUT_PLAY a headless run has no pad, as before (same state hashes as earlier tasks)."""
        r = run_ttt(TTT_FRAMES="200", TTT_SEED="1")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:])
        self.assertNotIn("ttt: input:", r.stdout)
        self.assertRegex(r.stdout, r"placeholders hit .*scePadGetState")

    def test_start_at_the_main_menu_replays_identically(self):
        """Start pressed at the main menu (VBlank 200) leaves it for the next mode; two replays end in the same state,
        which differs from the run with the same pads and no button."""
        idle, _ = self.play(stream())
        a, rec_a = self.play(stream((200, self.START, 6)))
        b, rec_b = self.play(stream((200, self.START, 6)))
        self.assertIn("ttt: input: 2 pads, 0 vblanks with a button down (played)", idle.stdout)
        self.assertIn("ttt: input: 2 pads, 6 vblanks with a button down (played)", a.stdout)
        modes_idle = re.search(r"ttt: modes: (.*)", idle.stdout).group(1)
        modes = re.search(r"ttt: modes: (.*)", a.stdout).group(1)
        self.assertNotRegex(modes_idle, r"\b4@")
        self.assertRegex(modes, r"\b3@\d+ 4@2\d\d")  # main menu, then the mode Start chose
        self.assertIsNotNone(state(a))
        self.assertEqual(state(a), state(b))
        self.assertNotEqual(state(a), state(idle))
        self.assertEqual(rec_a, rec_b)

    def test_character_select_runs(self):
        """Start at the main menu reaches the character select (mode 5), which no headless run reached before: it
        used to stop on two drafts of select_results_b.c (a 4-argument call of the 7-argument func_003C9AC0, and
        func_003C68F0 stepping its 0x20-byte font table by sizeof(SelState))."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream((200, self.START, 6)))
            r = run_ttt(TTT_FRAMES="700", TTT_SEED="1", TTT_INPUT_PLAY=str(p))
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertRegex(re.search(r"ttt: modes: (.*)", r.stdout).group(1), r"\b5@")

    def test_a_recording_replays_the_run(self):
        """TTT_INPUT_RECORD writes the latched states: playing that file back gives the same run."""
        a, rec = self.play(stream((200, self.START, 6)))
        self.assertIn("200 0800 80808080 0000 80808080", rec)
        self.assertIn("206 0000 80808080 0000 80808080", rec)
        b, rec2 = self.play(rec)
        self.assertEqual(state(a), state(b))
        self.assertEqual(rec, rec2)

    def test_multitap_recording(self):
        """A version 2 recording (`multitap 1`, four pads a line): the game finds the multitap on port 0 (gMultitap)
        and four ready DualShock 2 pads on slots 1A-1D (gPadPorts' connectors 0, 2, 3, 4 in phase 0x30; connector 1,
        port 2, empty); Start of pad 1 leaves the main menu as with two pads, and the run replays identically."""
        idle4 = " ".join(["0000 " + IDLE] * 4)
        text = (f"ttt-input 2 pads 4 multitap 1\n200 0800 {IDLE} 0000 {IDLE} 0000 {IDLE} 0040 {IDLE}\n"
                f"206 {idle4}\n")
        a, rec_a = self.play(text, TTT_PEEK="5F6C5C,8BCC80:45")
        b, rec_b = self.play(text, TTT_PEEK="5F6C5C,8BCC80:45")
        self.assertIn("ttt: input: 4 pads (multitap), 6 vblanks with a button down (played)", a.stdout)
        self.assertEqual(peek(a, "5F6C5C"), [1])
        phases = [w & 0xFFFF for w in peek(a, "8BCC80")[0::9]]
        self.assertEqual(phases, [0x30, 0, 0x30, 0x30, 0x30])
        self.assertRegex(re.search(r"ttt: modes: (.*)", a.stdout).group(1), r"\b3@\d+ 4@2\d\d")
        self.assertTrue(rec_a.startswith("ttt-input 2 pads 4 multitap 1\n"))
        self.assertIn(f"200 0800 {IDLE} 0000 {IDLE} 0000 {IDLE} 0040 {IDLE}\n", rec_a)
        self.assertEqual(state(a), state(b))
        self.assertEqual(rec_a, rec_b)


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class CharacterSelect(unittest.TestCase):
    def run_select(self, *presses):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*presses))
            r = run_ttt(TTT_FRAMES="345", TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_PEEK=f"{SEL_SIDE0}:5")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return peek(r, SEL_SIDE0)

    def test_the_cursor_moves(self):
        """Right on the d-pad moves side 1's cursor to the next cell and the side keeps choosing (state 1). Before T34
        b2 the m2c draft of func_003CA470 read the pad words as bytes: the cursor stayed and a press picked at once."""
        idle = self.run_select((200, START, 6))
        moved = self.run_select((200, START, 6), (330, RIGHT, 4))
        self.assertEqual(idle[0], 1)
        self.assertEqual(moved[0], 1)
        self.assertEqual(moved[1], idle[1] + 1)
        self.assertNotEqual(moved[4], idle[4])  # the character under the cursor


UP, LEFT, SELECT = 0x1000, 0x8000, 0x0100
# With the unlock cheat (TTT_CHEAT_UNLOCK, docs/port/cheats.md) the main menu's cursor starts on GALLERY: up and Start
# open THEATER (modes 12, 13). Left moves the theater's cursor from the movies tile to the music player tile; cross
# opens it (modes 14, 15); down and cross play the second track; select returns to the theater.
TO_THE_THEATER = ((200, UP, 4), (240, START, 6))
TO_THE_MUSIC_PLAYER = ((300, LEFT, 4), (340, CROSS, 4), (390, DOWN, 4), (430, CROSS, 4), (540, SELECT, 4))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Theater(unittest.TestCase):
    """T41 b0: THEATER and its music player run. Before, the m2c drafts of Mode_Theater and Mode_MusicPlayer called
    Text_Printf through 4-argument local prototypes: their formats ("%H%V%F%S%c%s") read the dropped arguments, and
    the %s took a stack word as a string (a crash 5 VBlanks into the theater). Both drafts are now written from the
    asm."""

    def play(self, frames, *presses, **env):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*presses))
            r = run_ttt(timeout=900, TTT_FRAMES=frames, TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_CHEAT_UNLOCK="1",
                        TTT_AUDIO="0", TTT_PEEK=MODE, **env)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return r

    def test_theater_and_music_player(self):
        """The theater (VBlank 245), the music player (341-540, a track played) and back, 640 VBlanks."""
        r = self.play("640", *TO_THE_THEATER, *TO_THE_MUSIC_PLAYER)
        modes = re.search(r"ttt: modes: (.*)", r.stdout).group(1)
        self.assertRegex(modes, r"\b3@\d+ 12@\d+ 13@\d+ 14@\d+ 15@\d+ 13@5\d\d$")
        self.assertEqual(peek(r, MODE), [0x0000000D])

    @unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (about a minute): TTT_SLOW_TESTS=1")
    def test_the_theater_drafts_agree_with_the_original(self):
        """A movie of the first character's row (an ending, mode 8, left with cross), then the music player: every
        call of Mode_Theater and Mode_MusicPlayer from VBlank 245 agrees with the executable's code (TTT_DIFF)."""
        r = self.play("1150", *TO_THE_THEATER, (360, RIGHT, 4), (400, SQUARE, 4), (600, CROSS, 4),
                      (760, LEFT, 4), (800, LEFT, 4), (840, CROSS, 4), (890, DOWN, 4), (930, CROSS, 4),
                      (1040, SELECT, 4), TTT_DIFF="245:2000", TTT_DIFF_ONLY="Mode_Theater,Mode_MusicPlayer")
        modes = re.search(r"ttt: modes: (.*)", r.stdout).group(1)
        self.assertRegex(modes, r"\b13@\d+ 8@\d+ 13@\d+ 14@\d+ 15@\d+ 13@\d+$")
        diffs = re.findall(r"ee-diff (Mode_Theater|Mode_MusicPlayer) \(VBlank \d+\): (\w+)", r.stdout)
        self.assertGreater(len(diffs), 500)
        self.assertEqual({d[1] for d in diffs}, {"same"}, [l for l in r.stdout.splitlines() if "DIFFERS" in l][:5])


# With the cheat, up twice and Start open TEKKEN BOWL (its character select is mode 5, the lane mode 4); square,
# right, square pick the bowler and the partner. On the lane cross three times: the aim, the spin and the power
# (VBlanks 760-880); the ball rolls into the pins and the first frame's second ball is set up.
TO_THE_BOWL = ((200, UP, 4), (240, UP, 4), (280, START, 6), (400, SQUARE, 4), (440, RIGHT, 4), (470, SQUARE, 4))
A_THROW = ((760, CROSS, 4), (820, CROSS, 4), (880, CROSS, 4))


@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
class Bowl(unittest.TestCase):
    """T41 b1: TEKKEN BOWL runs. Before, it stopped at VBlank 679 on the placeholder of func_003ECAD8 (the bowl's VU0
    code, now port/src/vu0_pc.c and tekken_bowl_pc.c), and the drafts func_003EFB00 / func_003EA4C8 crashed after the
    first ball (BowlBody pointers stepped by 0x180 elements). A throw and its roll, 1500 VBlanks; the lane (mode 4,
    sub-state 8) at the end."""

    def test_a_throw(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_BOWL, *A_THROW))
            r = run_ttt(timeout=900, TTT_FRAMES="1500", TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_CHEAT_UNLOCK="1",
                        TTT_AUDIO="0", TTT_PEEK=MODE)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertNotIn("not implemented yet", r.stdout)
        modes = re.search(r"ttt: modes: (.*)", r.stdout).group(1)
        self.assertRegex(modes, r"\b3@\d+ 4@\d+ 5@\d+ 4@\d+$")
        self.assertEqual(peek(r, MODE), [0x00080004])

    def test_the_lane_and_the_pause_windows_draw(self):
        """T41 b2: the lane (VBlank 780, the aim) has its wooden floor, and the pause window that Start opens at VBlank
        790 has its labels (CANCEL, RESET, TOP BOWLERS...). Both were black: the m2c draft of the stage load
        func_00367938 had 4 words for the sub-image blocks func_00361EA8 writes (16 in the original's frame, in front
        of the five image blocks), so the bowl's bank wrote over blocks[0] and its images went into VRAM over the
        lane's textures and the font. Then down, down and cross open TOP BOWLERS (VBlank 880: Bowl_DrawTop5, the
        records' rows of func_003F2AC0) and triangle turns to the score sheet (VBlank 960: the frames' table, rank and
        summary), the T41 b0 drafts."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_BOWL, (790, START, 4), (820, DOWN, 4), (840, DOWN, 4), (860, CROSS, 4),
                                (940, TRIANGLE, 4)))
            r = run_ttt(timeout=900, TTT_FRAMES="960", TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_CHEAT_UNLOCK="1",
                        TTT_AUDIO="0", TTT_DUMP=d, TTT_DUMP_EVERY="20", TTT_PEEK="5F5F08")
            self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
            lane, pause, top5, sheet = (png_rgb(Path(d) / f"frame_{v:05d}.png") for v in (780, 800, 880, 960))
        self.assertNotIn("not implemented yet", r.stdout)
        self.assertEqual(peek(r, "5F5F08"), [1])  # D_005F5F08: paused, by player 1

        def region(pic, x0, y0, x1, y1):
            """Mean level and white pixels of a region of a 640 x 448 picture."""
            w, _, px = pic
            sel = [px[(y * w + x) * 3:(y * w + x) * 3 + 3] for y in range(y0, y1) for x in range(x0, x1)]
            return sum(map(sum, sel)) / (3 * len(sel)), sum(1 for c in sel if min(c) > 160)

        self.assertGreater(region(lane, 160, 260, 480, 400)[0], 25)  # the floor in front of the bowler: about 46
        self.assertGreater(region(pause, 250, 120, 440, 220)[1], 1000)  # the labels: about 2300 white pixels, 0 before
        self.assertGreater(region(top5, 300, 60, 620, 420)[1], 8000)  # five rows of points and names: about 13600
        self.assertGreater(region(sheet, 40, 120, 620, 170)[1], 3000)  # the frames' table: about 6400 (top 5: 1100)

    @unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (about a minute): TTT_SLOW_TESTS=1")
    def test_the_draw_passes_run_on_the_original_code(self):
        """TTT_DIFF on main's five draw passes at VBlank 900 (the aim): the original's code runs to its end (COP0 ei /
        di / Status, the hardware registers, vcallms on the port's VU0; before T41 b2 it stopped on mfc0 in
        func_00357A50 and spun in func_00357A78). The fighters', the first and the last pass agree with the C."""
        passes = ("func_00357BD0", "func_00357A50", "func_00357A78", "func_00357B28", "func_00357B50")
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_BOWL, *A_THROW))
            r = run_ttt(timeout=900, TTT_FRAMES="901", TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_CHEAT_UNLOCK="1",
                        TTT_AUDIO="0", TTT_DIFF="900", TTT_DIFF_SECONDS="120", TTT_DIFF_ONLY=",".join(passes))
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertNotIn("original not run to its end", r.stdout)
        got = dict(re.findall(r"ee-diff (func_\w+) \(VBlank 900\): (\w+)", r.stdout))
        self.assertEqual(sorted(got), sorted(passes))
        self.assertEqual([got[n] for n in ("func_00357BD0", "func_00357A78", "func_00357B50")], ["same"] * 3)

    @unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (about seven minutes): TTT_SLOW_TESTS=1")
    def test_a_whole_game(self):
        """Cross every 40 VBlanks from the lane on: ten frames of two balls each (the bowler and the partner take
        turns), the game's end (func_003F1570) and the results (mode 10) at VBlank 10912."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_BOWL, *((v, CROSS, 4) for v in range(720, 10968, 40))))
            r = run_ttt(timeout=1800, TTT_FRAMES="10968", TTT_SEED="1", TTT_INPUT_PLAY=str(p), TTT_CHEAT_UNLOCK="1",
                        TTT_AUDIO="0")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertNotIn("not implemented yet", r.stdout)
        self.assertRegex(re.search(r"ttt: modes: (.*)", r.stdout).group(1), r"\b5@\d+ 4@\d+ 10@\d+$")

@unittest.skipUnless(EXE.exists(), "build/port/ttt not built (port/tools/build.py)")
@unittest.skipUnless(disc() is not None, "no disc data (TTT_DISC or iso/TEKKEN.BIN)")
@unittest.skipUnless(os.environ.get("TTT_SLOW_TESTS") == "1", "slow (about three minutes): TTT_SLOW_TESTS=1")
class Fight(unittest.TestCase):
    FRAMES = "1450"

    def play(self, *presses):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*presses))
            r = run_ttt(timeout=900, TTT_FRAMES=self.FRAMES, TTT_SEED="1", TTT_INPUT_PLAY=str(p),
                        TTT_PEEK=f"{MODE},{FIGHTER0}:3,{FIGHTER1}:3")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        return r

    def test_arcade_to_the_fight(self):
        """The pads take the game from the main menu through the character select (two members) and the stage load
        to round 1 of the fight (mode 4, sub-state 8); the fighters' moves follow the pad (forward, punches, a kick),
        and the same inputs replay the same fight."""
        a = self.play(*TO_THE_FIGHT, *IN_THE_FIGHT)
        b = self.play(*TO_THE_FIGHT, *IN_THE_FIGHT)
        idle = self.play(*TO_THE_FIGHT)
        modes = re.search(r"ttt: modes: (.*)", a.stdout).group(1)
        self.assertRegex(modes, r"\b3@\d+ 4@\d+ 5@\d+ 4@\d+$")  # main menu, fight set-up, select, fight
        self.assertEqual(peek(a, MODE), [0x00080004])
        self.assertEqual(peek(idle, MODE), [0x00080004])
        self.assertEqual(state(a), state(b))
        self.assertEqual(peek(a, FIGHTER0), peek(b, FIGHTER0))
        self.assertNotEqual(peek(a, FIGHTER0), peek(idle, FIGHTER0))
        self.assertNotEqual(state(a), state(idle))

    def test_stages_of_other_seeds(self):
        """The boot seed picks the stage: seed 3 the lanes stage (func_003D5B58 hung in its placement loop, then
        func_003D5E80 crashed in Model_QueueDraw), 9 the stage whose func_003D43A8 got its pointer as a float, 32 a
        stage with func_003E6DA0's particles (func_003E6B10 wrote at EE address 0). Each reaches round 1."""
        for seed in ("3", "9", "32"):
            with self.subTest(seed=seed), tempfile.TemporaryDirectory() as d:
                p = Path(d) / "in.txt"
                p.write_text(stream(*TO_THE_FIGHT, *IN_THE_FIGHT))
                r = run_ttt(timeout=900, TTT_FRAMES=self.FRAMES, TTT_SEED=seed, TTT_INPUT_PLAY=str(p),
                            TTT_PEEK=MODE)
                self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
                self.assertEqual(peek(r, MODE), [0x00080004])

    def test_snow_stage_path2_stays_in_step(self):
        """Seed 32 fights on stage 2, the snowy forest, whose snow particles (func_003E7270) upload their texture every
        frame through func_00344150 -> Gfx_PkAddImage, the game's only caller of libpkt's sceVif1PkOpenGifTag (a u128
        GIF tag) and sceVif1PkAddGsAD (a u64). The port took both as 32-bit words, right on i386 only: the x86-64
        build wrote a broken GIF tag and A+D data, PATH2 then read the following VIF1 packets as GIF data (`bad tag`,
        a tag of 27648 loops) and drew garbage for hundreds of VBlanks, about 1 VBlank/s with GL at scale 6 (runtime
        bug R1). PATH2 must see no bad tag and end each VBlank between packets."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "in.txt"
            p.write_text(stream(*TO_THE_FIGHT))
            r = run_ttt(timeout=900, TTT_FRAMES="1000", TTT_SEED="32", TTT_INPUT_PLAY=str(p), TTT_GS_TRACE="1",
                        TTT_PEEK=f"{MODE},5F5CB4")
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertEqual(peek(r, MODE), [0x00080004])
        self.assertEqual(peek(r, "5F5CB4")[0] >> 16, 2)  # D_005F5CB6, the stage
        self.assertNotIn("gs-trace bad tag", r.stdout)
        path2 = re.findall(r"gs-trace (\d+) path 2 tags \d+ loops (\d+)", r.stdout)
        self.assertGreater(len(path2), 900)
        self.assertEqual([v for v, loops in path2 if loops != "0"], [])


if __name__ == "__main__":
    unittest.main()
