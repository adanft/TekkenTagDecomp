# Documentation

## Labels

Claims about the game are tagged with one of two labels (in tables, a Status column holds the same words):

- **Verified** (**[V]**): read directly in the binary or the user's disc: the instruction sequence, a string, a
  version tag, a syscall number, a hardware register, a byte comparison, or matching C.
- **Inferred** (**[I]**): taken from strings, callers, the call graph, link order or a library's known API; not
  proven. A symbol name with this status carries `// guess` (see [decomp_guide.md](decomp_guide.md), "Naming").

Some notes also use **[PCSX2]** for what needs a check in an emulator. Setup and build pages describe the
project's own tools and carry no labels.

## Index

| Page | What it covers |
|---|---|
| [setup.md](setup.md) | Installing the toolchain inside the project and extracting your own disc. |
| [build.md](build.md) | How `configure.py` and ninja rebuild the executable byte for byte, and why the whole file is split. |
| [decomp_guide.md](decomp_guide.md) | The matching loop, the identified compiler and flags, matching notes, and the naming rules. |
| [text_map.md](text_map.md) | `.text` split into game code and library code (Sony SDK, newlib, libgcc), with evidence per range. |
| [game_overview.md](game_overview.md) | Boot, main loop, modes, the provisional module map, globals, random numbers, file loading. |
| [systems/files.md](systems/files.md) | `TEKKEN.BIN`: the file table, scrambling, contents, and the extractor. |
| [systems/audio.md](systems/audio.md) | The RSPU2DRV sound and file driver and the EE-IOP command protocol the game uses. |
| [systems/iop.md](systems/iop.md) | What each IOP module does and what the port replaces it with. |
| [systems/vu1/README.md](systems/vu1/README.md) | The VU0 and VU1 microcode, its listings and how they are assembled. |
