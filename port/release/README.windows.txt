Tekken Tag Tournament - PC port {version} (Windows x86-64)
==============================================================================

*** UNTESTED ON WINDOWS ***
This build was cross-compiled on Linux and has not yet been run on Windows.
It may not start at all. The port has been tested on Arch Linux only.
Reports (with the output of "ttt.exe --version" and any crash report) are
welcome.

A native PC program that runs Tekken Tag Tournament (PlayStation 2, USA release
SLUS-20001). This archive contains no part of the game: you need your own disc.
The first run asks for it.


Requirements
------------
- Windows 10 or 11, 64-bit.
{sdl3_dll}
- A graphics driver with OpenGL 3.3.
- Your disc of Tekken Tag Tournament, USA v2.00 (SLUS-20001), in one of these forms:
  the disc in a drive, a folder with its files, or an image (.iso, or
  .bin + .cue).


First run
---------
1. Extract the archive to a folder (with SDL3.dll next to ttt.exe, see
   above) and run ttt.exe.
   It is a console program: a console window shows its messages.
2. The setup screen asks for your disc: type or paste its path (Ctrl+V), or
   press Browse... Then press "Check and install". Only the unmodified USA
   release is accepted; a data file is made from it (the disc is only read).
3. Later runs start the game directly. "ttt.exe --setup" opens the setup
   screen again.

From a console, the same in one command:
    ttt.exe --setup D:\
    (exit code 0 done, 2 no disc there or cannot write, 4 refused)


Where files go
--------------
- Data file (ttt.dat), memory cards (saves\), texture packs (textures\) and
  mods (mods\): %LOCALAPPDATA%\ttt\
- Settings (settings.ini) and crash reports (crash\): %APPDATA%\ttt\
- Portable mode: create an empty file named portable.txt next to ttt.exe and
  everything stays in this folder.
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
- Direct (rollback netplay over UDP, one side reachable; Windows Firewall may
  ask to allow ttt.exe):
      set TTT_NET_HOST=0.0.0.0:7000 & ttt.exe          (host, player 1)
      set TTT_NET_JOIN=<host address>:7000 & ttt.exe   (join, player 2)
  See docs\netplay.md.
- Matchmaking: no public server is configured, and nothing is contacted until
  you set one. Anyone can run ttt-server.exe; players enter its address in
  F1 > Netplay. See docs\matchmaking.md.
- ttt-sign.exe signs update notices for a server's operator; players never
  need it.


In this archive
---------------
  ttt.exe, ttt.exe.syms        the program, and its symbol names for crash reports
  ttt-server.exe, ttt-sign.exe matchmaking server and notice signer (optional)
  SDL3.dll                     SDL 3 (official releases; zlib licence)
  lang\                        translations (lang\<code>.json)
  docs\                        the documentation (install, settings, netplay...)
  LICENSES\                    licences of the code included from other projects
  VERSION.txt                  the build; CHANGELOG.txt: what changed


Help
----
- "ttt.exe --version" prints the build and the languages it found.
- After a crash a report is written to %APPDATA%\ttt\crash\: include it, and
  the output of "ttt.exe --version", when you report a problem.
