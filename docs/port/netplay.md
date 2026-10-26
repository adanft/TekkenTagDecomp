# PC port: rollback netplay

T45. Two copies of the game, one per player, play one match over UDP. Each copy runs the whole game; the only thing
they exchange is the pads' states, one per VBlank, and a fingerprint of the state now and then to prove they still
agree. Built on the state's save / load at the VBlank boundary ([determinism.md](determinism.md)). Code:
`port/src/plat_net.c`; the hooks in `plat_state.c` (the boundary), `plat_pad.c` (the pads), `plat_hw.c` (the clock).
Tests: `tests/test_port_net.py`. Finding the other player through a server (room codes, quick match), NAT traversal,
the chat and the session in the overlay are T46: [matchmaking.md](matchmaking.md).

## Playing

Both players need the same build of the program and the game's data from the same (unmodified USA) disc. One hosts,
the other joins:

    TTT_NET_HOST=0.0.0.0:7000 build/port/ttt          # host, pad 1 (player 1), listening on every interface
    TTT_NET_JOIN=192.0.2.10:7000 build/port/ttt       # join, pad 2 (player 2)

`TTT_NET_HOST=7000` alone listens on 127.0.0.1 only (two copies on one machine): a port is opened to other machines
only when an address is given. The host waits up to `TTT_NET_WAIT` seconds (default 60) for someone to join. Each
player plays with the window's first pad (the keyboard's player 1 keys or the first gamepad); the game sees the host's
as pad 1 and the joining side's as pad 2. Both copies start from boot together (below) and go through the menus
together: player 1 picks the mode, a second player joins in as the game lets one (Start on pad 2: a challenger in
arcade, or VS mode).

| Variable | Effect |
|---|---|
| `TTT_NET_HOST=[<addr>:]<port>` | Host on that UDP port; the address defaults to 127.0.0.1 (`0.0.0.0` or `[::]` for every interface). Port 0: any free one, printed as `ttt: net: listening on <addr>:<port>`. |
| `TTT_NET_JOIN=<addr>:<port>` | Join a host (a name, an IPv4 address, or `[<IPv6>]:<port>`). |
| `TTT_NET_DELAY=<n>` | Input delay in VBlanks (default 2). The host's is used by both. |
| `TTT_NET_ROLLBACK=<n>` | The most VBlanks the game may run ahead of the peer's last state (default 6, at most 12; 0: lockstep). Each side's own. |
| `TTT_NET_CHECK=<n>` | A state fingerprint every n VBlanks (default 60; 0: none). The host's is used by both. |
| `TTT_NET_TIMEOUT=<s>` | A peer silent that long while it is waited for is gone (default 5): exit code 3. |
| `TTT_NET_DESYNC=continue` | Report a desync and go on (default: stop, exit code 5). |
| `TTT_INPUT_RECORD=<file>` | The session's final inputs, both pads (each VBlank once both sides' states are known): replayed offline with `TTT_INPUT_PLAY` and `TTT_MC0=none`, it gives the same run. |

For tests and measurements: `TTT_NET_SIM=delay:<ms>,jitter:<ms>,loss:<%>` holds back, reorders and drops this
side's packets on purpose (each side its own; the round trip is the sum); `TTT_NET_PACE_FROM=<vblank>` paces only
from that VBlank (0: never, both as fast as they can); `TTT_NET_DRAW=0` draws nothing (headless runs: only the state
counts); `TTT_NET_PERTURB=<vblank>` flips a bit of the game's state there (the desync test); `TTT_NET_TRACE=1` a line
per VBlank (time, pacing wait, frame advantage); `TTT_INPUT_PLAY=<file>` plays this side's pad from a recording (the
column of this side's pad: pad 1's for the host, pad 2's for the joining side). With `TTT_FRAMES` the window stays
closed unless `TTT_WINDOW=1`, and both sides wait for each other's last inputs before the report.

Exit codes: 2 the address or the socket, 3 nobody joined / no answer / the peer gone, 4 refused (the reason is
printed), 5 a desync, 6 a rollback found no state to go back to (a bug).

## What both sides must share

A session plays the same simulation on both sides, so everything that changes the game's state must be the same.
The joining side sends HELLO (every 100 ms) until the host answers WELCOME or REFUSE (both also carry the player's
name, `net.name`, for the overlay and the title, and the player's mods, `Mods_Summary`: shown to the peer and never
compared, [mods.md](mods.md)); each compares:

- the protocol version and the **build id** (the program's code hashed with the state's region list,
  `Port_StateBuildId`): another build of the program is refused;
- the **state hash at the first VBlank boundary** (`Port_StateHash` before the game reads a pad): the game's data
  loaded into EE RAM (`ttt.dat`, so another disc or data file differs) and whatever the boot set up;
- **game data marked modified** (`[modified game data]`, [data.md](data.md)) on either side: refused;
- the **settings that change the state**: `cheats.unlock_all` ([cheats.md](cheats.md)), `TTT_SEED`, `TTT_MC_POLLS`.
  Changing the cheat in the overlay during a session does nothing.

What a session fixes on both sides, whatever the settings say:

- **The clock**: the deterministic one of headless runs (one Timer 0 tick per poll, `plat_hw.c`), paced by
  `plat_net.c` to 59.94 VBlanks a second. The real-time clock is never used: its polls per frame follow the host's
  timing and the game's spin generator with them, so two machines would part at once. `game.speed` and the
  fast-forward key have no effect, and the overlay's pause does not stop the game (the peer would not stop).
- **No memory card** on either port (`plat_mc.c`): both boot the same, nothing is read from or written to a player's
  saves. The game runs without a card as on a PS2 without one (no saving; unlocks only with the cheat, on both sides).
- **No movies** (`plat_movie.c`): the decoder is host memory no state holds, so no state could be saved while one
  plays.
- **Two pads**, no multitap.

Starting from boot (rather than exchanging a state at the connection) needs nothing sent but the handshake, cannot
pick up a player's private state, and a state file would be 40 MB; the price is that both go through the menus
together. A savestate exchange (both loading the same `TTT_LOADSTATE` file) works in principle but is not tested.

## How it runs

Both sides latch the pads at the same VBlanks as offline (`Port_PadVBlank`); a session's latch takes both states
from `plat_net.c` (`Port_NetPads`):

- **The local pad** is sampled at VBlank n (the first time the game passes it) and becomes the state of VBlank
  n + delay, sent to the peer at the next boundary. With delay 0 a press is seen as offline, with delay 2 two
  VBlanks (33 ms) later. The VBlanks up to delay + 1 are idle on both sides.
- **The peer's pad** is the state it sent for that VBlank or, when it has not arrived yet, a prediction: the last
  state that did arrive.
- **Rollback**: at each VBlank boundary (`Port_NetBoundary`, on the boundary's service stack) the state is saved into
  a ring of `rollback` + 1 buffers (`Port_StateSave` with page tracking: the pages written since that buffer's last
  save) and the packets that arrived are read. When a peer state differs from the prediction the game used, the latest
  saved state before that VBlank is loaded and the VBlanks since run again with the right states (`gPortResim`), each
  saving its state again, and the game goes on. The frames run again are not drawn and not heard (`gPortNoGs`, the
  sound's `gPortResim`): every VBlank's picture and sound go out once, the first time, with the prediction. The saves
  leave out the GS's side of the state (`gPortStateNoGs`), so nothing waits for the GS threads.
- **Waiting**: the game does not run a VBlank more than `rollback` VBlanks past the peer's last state; it waits for
  it (a stall), sending its own every 8 ms. It also waits for all of the peer's states when the boundary's state could
  not be saved (nothing to roll back to: lockstep), and before the last VBlank of a `TTT_FRAMES` run.
- **Packets** (UDP, little-endian, one per VBlank and every 8 ms while waiting): the sender's VBlank, the last VBlank
  of the receiver's states it has (an acknowledgement), its frame advantage, its last final fingerprint, timestamps
  for the round trip, and every local state the peer has not acknowledged (up to 64): a lost packet is covered by the
  next one, reordered ones are sorted by VBlank.
- **Time sync**: each side estimates how far ahead of the peer it runs (its VBlank against the peer's last one plus
  half the round trip, smoothed) and sends it. Every 30 VBlanks the side ahead slows down by half the difference,
  at most 2 ms per VBlank. A side more than two VBlanks late (a long load, the host stalled) drops that time instead
  of rushing.

### Sound during rollback

The SPU2 model and the BGM streamer are part of the state, so after a rollback the sound goes on from the corrected
state; the VBlanks run again produce no output (as the synctest's second pass), so the audio stream gets exactly one
VBlank of sound per VBlank of wall time and nothing is heard twice. What a misprediction costs is what it changed:
a sound the predicted VBlanks started has been heard, and one that only the corrected VBlanks start is heard from
where they had got to (its first VBlanks are lost). Vibration follows the first pass too, and only the local pad's
(`plat_pad.c`).

### Pictures during rollback

The GS sees each VBlank's work once. The game reads nothing back from the GS, so its state does not depend on what
was drawn (the GL sessions end in the state of the undrawn ones); but a texture or a VU1 program the game uploaded
only in a VBlank that was run again (its DMA dropped) and then counts as resident would be missing from the pictures
until it is uploaded again. The pictures after rollbacks have not been compared with an offline run's (Open).

## Desync detection

Every `check` VBlanks (60 by default), at the boundary, each side takes a **fingerprint** of the state
(`Port_StateFingerprint`): every hashed region of [determinism.md](determinism.md), with EE RAM, GS memory and sound
RAM hashed page by page and each page's hash kept until page tracking sees that page written, so it costs the pages
written since the last one (about 1-2 ms instead of the 11-21 ms of `Port_StateHash`). It depends only on the state.
A fingerprint is tentative until every peer state up to its VBlank has arrived and matched what was used (a rollback
through that VBlank takes it again); then it is final and goes out with the inputs. When the two sides' fingerprints
of a VBlank differ, each prints

    ttt: net: desync at vblank 1200: state hash 1c9adcb175b31f81 here, 750043b35916b8ec at the peer
    ttt: net: desync at vblank 1200: region ee.ram differs (ea0d391ea41d8750 here, 0dbe502a9f063829 at the peer)

(the sides exchange their regions' fingerprints), and stops with exit code 5 (`TTT_NET_DESYNC=continue`: goes on).
Nothing resyncs: a desync means the two copies are not the same simulation (a build, data or setting the handshake
missed, or a determinism bug), and copying a 40 MB state across would hide it.

## Report

At the end of a `TTT_FRAMES` run (and before any exit on an error) each side prints:

    ttt: net: host, pad 1, delay 2, rollback 6; vblank 1451, peer inputs to 1452; ... packets sent, ... received, ...
    ttt: net: rollbacks 15, frames run again 68, max depth 6, depths 1-12: 2 1 0 2 4 6 0 0 0 0 0 0; rollback mean ...
    ttt: net: per vblank busy mean ... us, max ... us, ... over 16683 us; waits: ... stalls (... ms), pacing ... ms, ...
    ttt: net: 24 checks (fingerprint mean 1278 us), 0 desyncs, 0 saves refused; rtt min ... us, mean ... us, max ...
    ttt: net: paced from vblank 1101: 350 vblanks, busy mean ... us, p50 ... us, p95 ... us, max ... us, ...

"Busy" is a VBlank's wall time without the waits: the game's frame, the save, any rollback and the fingerprint.

## Measurements

Two processes on one machine over loopback, the scripted session of the tests (pad 1 from the main menu to round 1
and in it, pad 2 in round 1, ending at a challenger's select: 1451 VBlanks), paced at 1x from VBlank 1101 (round 1),
defaults (delay 2, rollback 6, a check every 60), 3 runs of each. Every run of both sides ended in the same state
(`0xb1022e42526da933`, the offline replay's), with no desync. "Busy" per paced VBlank, host / joining side. The
machine ran two other agents' test suites in the same CPU quota (3 CPUs) meanwhile: the times are pessimistic and
vary from run to run (the clean runs' means 6-11 ms against 4.4 ms in the quietest lossy run).

| | Busy mean / p50 / p95 (ms) | Paced rollbacks (frames, max depth) | Whole run: rollbacks, frames, max depth | Round trip min / mean |
|---|---|---|---|---|
| Clean, undrawn | 6.0-10.7 / 3.8-7.3 / 13.8-26.8 | 0-3 (0-11, up to 6) | 0-7, 0-24, up to 6 | <0.1 / 7-14 ms |
| 30 +- 10 ms each way, 5 % loss, undrawn | 4.4-10.2 / 3.8-5.3 / 8.8-26.8 | 2-7 (5-15, 2-6) | 2-19, 5-56, up to 6 | 41-42 / 73-91 ms |
| The same, GL in hidden windows, drawn | 15.7-22.4 / 5.8-8.3 / 69-100 | 2-5 (9-24, 5-6) | 2-13, 10-62, up to 6 | 41-43 / 127-154 ms |

- The fingerprint check costs 1.2-2.8 ms (mean) every 60 VBlanks; `Port_StateHash` cost 11-21 ms.
- A rollback of 4-6 undrawn VBlanks took 23-107 ms of wall time here (about 4 ms per VBlank on a quiet machine,
  [determinism.md](determinism.md)); the mean round trip includes the VBlanks spent loading (no packet goes out
  during a 0.1-0.6 s load), the minimum is the link's.
- Drawn with GL, the presents' intervals over round 1 were p50 20-32 ms, far from the 16.7 ms of an offline run on
  a quiet machine: with the CPU quota shared, the drawn sessions did not keep 1x (stalls 0.6-2 s per run). Not yet
  measured on a quiet machine (Open).

## Open

- **Spectators** and a lobby with a list of rooms (T46 did room codes, the quick match, hole punching, the relay,
  the version check before pairing, the overlay's page, the title and the chat: [matchmaking.md](matchmaking.md)).
  `TTT_NET_HOST` / `TTT_NET_JOIN` still need a reachable UDP port (LAN, a forwarded port, or a VPN).
- **Sessions from a menu** rather than from boot (both loading one state sent at the connection: 40 MB, or the
  pages that differ from a common boot state).
- **The memory card**: both sides' saves (unlocks, records) could be exchanged at the connection and kept read-only.
- **Rollback budget**: a fight VBlank run again undrawn costs about 4 ms, so about 3 rollback frames fit beside a
  drawn VBlank on this machine (see Measurements); a long round trip with jitter needs more input delay.
- **Pictures**: resident textures or VU1 programs uploaded only in a VBlank that was run again (see above) are not
  checked: compare the pictures of a session with rollbacks against its offline replay (`TTT_INPUT_RECORD`).
- **Disconnects**: a lost peer ends the session (exit code 3); there is no reconnect, no pause, no rematch.
- **IPv6** hosts need an explicit address (`[::]:<port>`).
- **Pacing measured on a quiet machine**, drawn with GL: the measurements above ran beside other heavy jobs.
- **Memory**: the ring holds rollback + 1 states of 40 MB (280 MB by default) per process.
