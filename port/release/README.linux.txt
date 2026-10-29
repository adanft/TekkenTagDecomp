Tekken Tag Tournament - PC port {version} (Linux x86-64)
==============================================================================

A native PC program that runs Tekken Tag Tournament (PlayStation 2, USA release
SLUS-20001). This archive contains no part of the game: you need your own disc.
The first run asks for it.

Tested on Arch Linux only. Other distributions with glibc 2.34 or newer are
expected to work but have not been tried; reports are welcome.


Requirements
------------
- Linux x86-64 with glibc 2.34 or newer (for example Ubuntu 22.04, Debian 12,
  Fedora 35, RHEL 9 or later).
- SDL 3 (libSDL3.so.0): your distribution's SDL 3 package, or SDL 3 built from
  https://libsdl.org. Without it the program runs without a window.
- A graphics driver with OpenGL 3.3.
- Your disc of Tekken Tag Tournament, USA (SLUS-20001), in one of these forms:
  the disc mounted, a folder with its files, or an image (.iso, or .bin + .cue).


First run
---------
1. Extract the archive anywhere and run ./ttt
2. The setup screen asks for your disc: type or paste its path (Ctrl+V), or
   press Browse... Then press "Check and install". Only the unmodified USA
   release is accepted; a data file is made from it (the disc is only read).
3. Later runs start the game directly. ./ttt --setup opens the setup screen
   again (to point the program at another copy of your disc).

Without a window, the same in one command:
    ./ttt --setup /path/to/disc-or-image
    (exit code 0 done, 2 no disc there or cannot write, 4 refused)


Where files go
--------------
- Data file (ttt.dat), memory cards (saves/), texture packs (textures/) and
  mods (mods/): ~/.local/share/ttt/   ($XDG_DATA_HOME/ttt/ when set)
- Settings (settings.ini) and crash reports (crash/): ~/.config/ttt/
  ($XDG_CONFIG_HOME/ttt/ when set)
- Portable mode: create an empty file named portable.txt next to ttt and
  everything stays in this folder (a USB stick, a folder you move around).
- The data file and your memory cards hold data from your disc: do not share
  them.


Controls
--------
- F1 (or a gamepad's guide button): settings - video, audio, controls, game,
  cheats, mods, netplay, accessibility (the language is chosen there), about.
- Tab, held: fast forward (the speed is set in F1 > Game).
- Esc, or closing the window: asks before quitting.
- Keyboard keys and gamepad buttons: F1 > Controls. Up to four gamepads.


Playing online
--------------
- Direct (rollback netplay over UDP, one side reachable):
      TTT_NET_HOST=0.0.0.0:7000 ./ttt            (host, player 1)
      TTT_NET_JOIN=<host address>:7000 ./ttt     (join, player 2)
  See docs/netplay.md.
- Matchmaking: no public server is configured, and nothing is contacted until
  you set one. Anyone can run ./ttt-server; players enter its address in
  F1 > Netplay (or TTT_NET_SERVER=<address>:<port>). See docs/matchmaking.md.
- ttt-sign signs update notices for a server's operator; players never need it.


In this archive
---------------
  ttt, ttt.syms          the program, and its symbol names for crash reports
  ttt-server, ttt-sign   matchmaking server and notice signer (optional)
  lang/                  translations (lang/<code>.json)
  docs/                  the documentation (install, settings, input, netplay...)
  LICENSES/              licences of the code included from other projects
  VERSION.txt            the build; CHANGELOG.txt: what changed


Help
----
- ./ttt --version prints the build and the languages it found.
- After a crash a report is written to ~/.config/ttt/crash/: include it, and
  the output of ./ttt --version, when you report a problem.
