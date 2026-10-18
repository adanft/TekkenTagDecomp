# TekkenTagDecomp

A decompilation of **Tekken Tag Tournament** for PlayStation 2, with the goal of a native PC port: the game's own
code compiled for PC, with a new renderer, sound and input layer underneath. It is not an emulator.

## How it works

1. **Matching decompilation.** The game's executable is rebuilt from C that compiles to the same bytes as the
   original. Matching is the proof that the C behaves exactly like the game.
2. **PC port.** The decompiled code is compiled for PC, and a platform layer replaces the PS2 hardware: graphics,
   sound, input, memory card and disc access.

## Requirements

You need your own ISO of Tekken Tag Tournament for PlayStation 2. This repository contains no game data.

## Project notes

- Work starts with a byte-identical rebuild of the executable; nothing is ported before its code matches.
- Fight logic is decompiled first, since errors there only show when two machines disagree; graphics and menus
  come after.
- The PS2's floating point is not IEEE-exact, so the port reproduces it in software to keep fights identical.

## Status

Early setup: toolchain and build of the original executable.
