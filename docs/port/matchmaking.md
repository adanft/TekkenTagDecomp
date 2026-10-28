# PC port: matchmaking

T46. Two players find each other through a small **rendezvous server that someone in the group runs**, instead of
typing IP addresses. The server pairs two games of the same build (by a room's code or a quick-match queue), tells
each where the other is so they can connect directly (UDP hole punching), and relays their packets when they cannot.
Then the rollback session of [netplay.md](netplay.md) runs over that path. No server is built into the game, none is
contacted by default, and the server keeps nothing once a pair has played.

Code: `port/server/ttt-server.c` (the server), `port/server/ttt_match_proto.h` (the protocol, shared with the game),
`port/server/ttt-sign.c` (the update manifest's signer), `port/src/plat_net.c` (the game's side), `overlay.cpp` (the
Netplay page). Tests: `tests/test_port_match.py` (loopback only).

## Quick path

1. Build the server (any Linux / BSD / macOS machine with a C compiler):

       python3 port/tools/build_server.py        # -> build/server/ttt-server, build/server/ttt-sign
       # or by hand: cc -O2 -o ttt-server port/server/ttt-server.c
       python3 port/tools/build_server.py --win64   # Windows (mingw-w64 cross compiler) -> build/server-win64/*.exe

   The Windows programs (T42 b7) cross-build with `-Werror` and need Windows 8 or later; they have not been run on
   Windows yet.

2. Run it where both players can reach it (a forwarded UDP port at home, a small VPS):

       build/server/ttt-server --listen 0.0.0.0:7100

   The default is `--listen 127.0.0.1:7100`: loopback only, for trying it on one machine.

3. Each player sets the server (F1, **Netplay** page, or the settings file / variable) and starts a session:

       TTT_NET_SERVER=203.0.113.5:7100 TTT_NET_CREATE=1 build/port/ttt      # a new room: prints its code
       TTT_NET_SERVER=203.0.113.5:7100 TTT_NET_ROOM=K7QX2M build/port/ttt   # join that room
       TTT_NET_SERVER=203.0.113.5:7100 TTT_NET_QUICK=1 build/port/ttt       # quick match: the next player waiting

   The Netplay page's **Create a room**, **Join this room** and **Quick match** buttons do the same: they start the
   program again from boot in that mode (a session always starts from boot on both sides, [netplay.md](netplay.md)).
   The room's code shows in the window's title and on the page while the room waits.

## Settings and variables

| Setting (`settings.ini`) | Variable | Default | Effect |
|---|---|---|---|
| `net.server` | `TTT_NET_SERVER` | empty | `<addr>:<port>` of the matchmaking server. Empty: no matchmaking. |
| `net.name` | `TTT_NET_NAME` | `Player` | The name the other player sees (31 bytes of UTF-8). |
| `net.punch` | `TTT_NET_PUNCH` | 1 | Try a direct path first; 0: always through the server's relay. |
| `net.update_key` | `TTT_NET_UPDATE_KEY` | empty | The public key (64 hex digits) update manifests must be signed with. Empty: no update check. |
| | `TTT_NET_CREATE=1` / `TTT_NET_ROOM=<code>` / `TTT_NET_QUICK=1` | | The session's mode (read at start). |
| | `TTT_NET_PORT=<n>` | any | The local UDP port (for a forwarded one). |
| | `TTT_NET_WAIT=<s>` | 600 (60 to join) | How long to wait for a player, then exit code 3. |
| | `TTT_NET_PUNCH_MS=<ms>` | 2000 | How long the joining side tries the direct path. |
| | `TTT_UPDATE_CHECK=1` | | Ask the server for its update manifest, report, exit. |
| | `TTT_NET_CHAT=<text>[\|<text>]` | | Lines sent when the session starts (tests). |
| | `TTT_NET_TEST_BUILD=<hex>` | | Another build id to the server (the version-mismatch test). |

Exit codes as in [netplay.md](netplay.md): 2 no server / bad address, 3 nobody came or no answer, 4 refused (the
server's reason is printed: no such room, another build, the server is full).

## How a session is found

```
game A (room)          server                 game B (join K7QX2M)
  CREATE  ------------>  room K7QX2M
  <------------  WAIT K7QX2M
                                <------------  JOIN K7QX2M (same build?)
  <------------  MATCH (host, B's addresses)  MATCH (joining, A's addresses) ------------>
  PUNCH -> B's addresses  ...  ...  PUNCH -> A's addresses
  PUNCH_ACK  --------------------------------------------->   direct path found
  <------------------------------------------------------   HELLO / WELCOME, the session
  (no PUNCH_ACK within 2 s: both send to the server, which relays)
```

- **Pairing.** The room's creator (or the game that waited longest in the queue) is the host, pad 1; the other joins,
  pad 2. The server makes a random 32-bit session id; the host accepts a HELLO only with that id.
- **Version check before pairing.** Each request carries the netplay protocol version and the build id
  (`Port_StateBuildId`). A join to a room of another build is refused (`MS_ERR_VERSION`), the quick match only pairs
  equal builds. The handshake still checks the rest (the game data, the settings that change the game).
- **Hole punching.** The server tells each game the other's address as the server saw it (its public endpoint behind
  a NAT) and the local address it reported (two players behind one NAT). Both send PUNCH to those addresses every
  50 ms, which opens each side's NAT mapping; the joining side takes the first address that answers PUNCH_ACK (a path
  that works both ways) and sends HELLO there. This works through most home NATs (endpoint-independent mapping); not
  through two symmetric NATs.
- **Relay.** When no PUNCH_ACK came within `TTT_NET_PUNCH_MS` (or `net.punch` is 0), the joining side sends its HELLO to
  the server, which forwards every datagram of a paired game that is not its own protocol to the other game, as it is.
  A direct path that gets no WELCOME within 2 s falls back to the relay too. Each game then talks only to the address
  the handshake came from (`connect`), so a relayed session ignores everything else.
- **Chat.** CHAT / CHAT_ACK packets on the session's path: each line is sent every 200 ms until acknowledged, lines
  arrive in order and once. Lines typed before the peer is connected wait in a queue of 16 and go out at the start;
  the "lobby" is the game's own menus, which both sides go through together. The overlay's Netplay page shows the last
  32 lines and a field to write; a line that arrives shows for 10 s over the game (F1 to answer). Control characters
  become spaces, and a line is at most 159 bytes of UTF-8.
- **What the player sees.** The window's title: `Netplay: room K7QX2M, waiting for a player`, then
  `Netplay with Bea (direct): 23 ms, 4 rollbacks`. The Netplay page: the state, the room's code, the peer, direct or
  relayed, the round trip and input delay, rollbacks and frames run again, the waits, and the chat.

## The protocol

UDP, little-endian, every server message starting with the magic `TTMS` and a version byte. The layout, field by
field, is in `port/server/ttt_match_proto.h`; in short:

| Message | From | Content |
|---|---|---|
| `CREATE`, `JOIN`, `QUICK`, `LEAVE` | game | protocol version, build id, local address, room code (join), name; 160 bytes |
| `MANIFEST` | game | 1232 bytes of zeros after the header |
| `WAIT` | server | the room's code, the place in the queue, the game's public address |
| `MATCH` | server | role, relay on/off, session id, the peer's public and local addresses, name, the room's code |
| `ERROR` | server | a code (no room, another build, full, malformed, no manifest) and a reason |
| `MANIFEST` | server | the manifest file, as it is |
| anything else | paired game | relayed to the other game unchanged |

A game repeats its request every 500 ms until it hears an answer (requests are idempotent: the server keys a game by
its address and a random cookie). A game silent for `--wait-timeout` (10 s) loses its room or its place; a pair with
no traffic for `--pair-timeout` (30 s) is forgotten. `LEAVE` (sent at the end of a session) forgets it at once.

## Hosting a server

    ttt-server [--listen <addr>:<port>] [--manifest <file>] [--no-relay] [--relay-rate <bytes/s>]
               [--max-waiting <n>] [--max-pairs <n>] [--wait-timeout <s>] [--pair-timeout <s>] [--verbose]

- One process, one UDP socket, no threads, no files written. It needs no privileges: run it as an ordinary user (a
  systemd unit with `DynamicUser=yes` is plenty) on a port above 1024. Forward that UDP port if it runs at home.
- Memory: about 200 bytes per waiting game and 400 per pair (`--max-waiting`, `--max-pairs`, 256 each by default).
- The relay costs bandwidth: a session sends about 60 small packets a second each way (about 6-10 KB/s per player).
  `--relay-rate` caps each game (64 KB/s by default; past it packets are dropped, and the rollback copes with the
  loss). `--no-relay` turns it off: then pairs that cannot punch through fail (exit code 3 on the joining side).
- `--verbose` logs pairings, codes and addresses on stderr; without it the server prints only its start and stop.
- IPv6: `--listen [::]:7100` serves IPv6 games (both players must reach it the same way).

## Privacy and security

- **Nothing is contacted unless the player configured a server.** `net.server` is empty by default and no address is
  built into the program. A server is a choice of the group of players, like a voice chat server.
- **What a server learns**: each player's public IP address and port, local address, chosen name, build id, and (when
  relayed) the session's packets, which are pads' states and state fingerprints, never anything of the player's files
  or saves. It keeps that only while the room or the pair exists, in memory; nothing is written to disk.
- **What the other player learns**: your name, and your public and local addresses: the server's MATCH carries them
  even when `net.punch = 0` (the relay then carries the session, but does not hide the addresses; Open).
- **No accounts, no authentication**: a room's code is the only secret (6 characters of 32: about a billion codes, and
  a room lasts as long as its creator waits). The session id the server makes keeps strangers out of a session.
- **Abuse limits**: answers are never larger than the requests they answer (a manifest request is padded to the
  manifest's size), so the server cannot amplify traffic toward a forged address; only a paired address's packets
  are relayed, only to its pair, within `--relay-rate`; datagrams over 1400 bytes are dropped. The server parses
  only its own fixed-size messages and copies names and codes with bounds.
- **Not encrypted**: the session's packets (and the chat) travel in the clear, as with most fighting-game netplay.
  Do not type anything private in the chat.

## Update notices (signed, never automatic)

The program never downloads or runs anything. A server may hand out a small **manifest** naming the newest build; the
game checks its Ed25519 signature against the public key in `net.update_key` and only reports what it says:

    ttt: update: this build is 1c9adcb175b31f81
    ttt: update: version 1.2 is available (build 750043b35916b8ec): https://example.org/ttt; nothing is downloaded: get it from a source you trust

The check runs at the start of a matchmaking session when `net.update_key` is set (the Netplay page then shows the
notice), or alone with `TTT_UPDATE_CHECK=1` (exit 0 checked, 3 no manifest / no answer, 4 bad signature). A manifest
whose signature does not verify is ignored, with a note.

Making and signing one (on a machine that is not the server; the secret key never goes there):

    build/server/ttt-sign keygen ttt-update.key          # prints the public key: publish it, players put it in net.update_key
    printf 'ttt-update 1\nversion 1.2\nbuild 750043b35916b8ec\nurl https://example.org/ttt\n' > manifest.txt
    build/server/ttt-sign sign ttt-update.key manifest.txt manifest.signed
    build/server/ttt-sign verify <public key> manifest.signed
    ttt-server --manifest manifest.signed ...

The manifest is text: `ttt-update 1`, `version`, `build` (16 hex digits, as `TTT_UPDATE_CHECK=1` prints), optional
`url` and `notes`, then `signature <128 hex digits>` over every byte before that line. `ttt-update*.key` is in
`.gitignore`. The Ed25519 and SHA-512 code (`third_party/tweetnacl/ed25519.h`) follows TweetNaCl (public domain) and is
checked against RFC 8032's first test vector (`TTT_SELFTEST=net`) and against libsodium through PyNaCl in the tests.
There are no official builds or keys yet: whoever distributes builds would publish their key next to them.

## Tests

`tests/test_port_match.py`, all on 127.0.0.1 (the server is built first, about 2 s):

- the protocol from Python clients: rooms, codes in any case, a join of another build refused, MATCH repeated,
  the quick match by build, the relay (and nothing relayed for unpaired addresses or after LEAVE), `--no-relay`,
  manifest requests smaller than the answer ignored;
- `ttt-sign` against PyNaCl, and the game's crypto self-test;
- two games: a room's code with a direct path and the chat both ways; the quick match with punching off (all
  relayed); a joining side of another build refused (exit 4) while the host keeps waiting; the update check with a
  good key, another key and no manifest. Each session plays 600 undrawn VBlanks and both sides end in the same state.

## Open

- **Lobby and spectators**: no list of open rooms (codes only), no third game watching a session.
- **NAT types**: two symmetric NATs always fall back to the relay; no port prediction, no IPv4/IPv6 mixing, no UPnP.
- **Windows**: the server and the socket code are POSIX; the overlay's restart uses `/proc/self/exe` (Linux).
- **Restart from the overlay** is not covered by a test (it replaces the process; the tests start sessions with the
  variables).
- **Encryption** of the session and the chat; a reconnect after a lost path (a lost peer still ends the session).
- **Relay-only privacy**: a mode where the server withholds the addresses from MATCH (both sides relayed).
