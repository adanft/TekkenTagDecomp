# T19 inventory: game functions not in C

T19 needs every game function in C with the SHA-1 intact. This page lists what was left at the start of T19 and what
is left after batch 1. Regenerate the tables after a build:

```sh
.venv/bin/python scripts/inventory.py          # summary tables
.venv/bin/python scripts/inventory.py --list   # plus one row per function
```

`scripts/inventory.py` (tests: `tests/test_inventory.py`) takes the functions that still carry splat's `.NON_MATCHING`
marker in `build/SLUS_200.01.elf` (as `scripts/progress.py` does) and sorts each one into a category:

- **asm-only, no C file**: the function sits in an assembly chunk of `config/SLUS_200.01.yaml`. Its module comes from
  the `// range:` lines of `config/symbols/game_<module>.txt`.
- **asm-only, not attempted**: a C file includes the function with `INCLUDE_ASM` but has no draft for it.
- **draft**: the `INCLUDE_ASM` sits in the `#else` of an `#if 0` block holding the closest attempt.

For a draft, the cause comes from keywords in its `NON_MATCHING:` note. This is a rough grouping, good enough to plan
cleanup batches; read the note itself before working on a function.

## At the start of T19 (1996 / 2813 functions in C)

817 functions were not in C: 386 asm-only and 431 drafts.

| Asm-only | Functions | Where |
|---|---:|---|
| No C file | 381 | `geometry` 53, `fighter_setup` 140, `practice` 44, `zsort` 13, `screenshot` 20, `movie` 111 |
| Not attempted, in a C file | 5 | `dma_gif` (four hand-written VU0 routines), `memcard` (`Mc_Update`) |

The brief also named `main`, `rng_misc`, `pad`, `file` and `gfx`. These had no asm-only function: everything left in
them is a draft.

| Draft cause (431) | Functions |
|---|---:|
| register allocation / scheduling | 250 |
| m2c rough draft | 67 |
| store order | 48 |
| block layout / alignment (cross-jumped or misplaced blocks, alignment nops) | 41 |
| other | 12 |
| data layout / `.lit4` pool order (pool out of text order, 128-bit save mode, `.sdata` shifts) | 9 |
| gas hazard | 4 |

## After T19 batch 1 (2312 / 2813 in C)

Every game function now has C. The 386 asm-only functions became 316 matches and 70 drafts, so 501 drafts remain.

| Draft cause (501) | Functions | New in batch 1 |
|---|---:|---:|
| register allocation / scheduling | 295 | 45 |
| m2c rough draft | 74 | 7 |
| store order | 54 | 6 |
| block layout / alignment | 44 | 3 |
| other | 12 | 0 |
| data layout / `.lit4` pool order | 11 | 2 |
| hand-written assembly (VU0 routines with internal entry points) | 7 | 7 |
| gas hazard | 4 | 0 |

Drafts per module:

| Module (C file) | Drafts | Module | Drafts | Module | Drafts |
|---|---:|---|---:|---|---:|
| `render_fighter_a` | 4 | `rng_misc` | 2 | `options` | 11 |
| `render_fighter_b` | 2 | `dma_spr` | 10 | `pause_results` | 9 |
| `vu1_packet` | 4 | `memcard` | 1 | `modes` | 7 |
| `dma_gif` | 9 | `font_tex` | 4 | `modes_b` | 6 |
| `geometry` | 2 | `fighter_setup` | 5 | `select_results` | 12 |
| `geometry_b` | 17 | `motion` | 2 | `hud` | 2 |
| `geometry_c` | 2 | `fighter_setup_b` | 10 | `select_results_b` | 35 |
| `scene3d` | 7 | `battle_flow` | 28 | `sprite2d` | 13 |
| `skeleton` | 3 | `sound` | 6 | `model_anim` | 3 |
| `scene3d_c` | 6 | `fight_misc` | 3 | `model_anim_b` | 19 |
| `scene3d_d` | 3 | `practice` | 6 | `demo` | 17 |
| `theater` | 3 | `fighter_core` | 57 | `menu_bg` | 6 |
| `theater_b` | 1 | `stage` | 46 | `gallery_jpeg` | 8 |
| `theater_d` | 1 | `pad` | 1 | `tekken_bowl` | 38 |
| `draw_list` | 3 | `gfx` | 4 | `effects` | 3 |
| `main` | 6 | `zsort` | 3 | `effects_draw` | 4 |
| `battle_mode` | 4 | `screenshot` | 1 | `loading_title` | 9 |
| | | `movie` | 23 | `menu_common` | 10 |

## Per-cause notes for the next batches

- **Register allocation / scheduling** (295): most are a few lines off. They are candidates for decomp-permuter (one run
  at a time) and for sweeps over declaration and statement order.
- **m2c rough draft** (74): the C compiles but has not been tuned. Most are large: the theater and music player modes,
  the Bowl physics, the fight camera, `Mc_Update` (1064 instructions), `func_003478F0` / `func_00347E28` (skeleton
  frames) and the camera collision tests `func_0034B4E8` / `func_0034BDD8`.
- **Store order** (54): sweep the orders of the store runs. The scheduler emits the last store of a run first.
- **Block layout / alignment** (44): cross-jumped `return` blocks, peeled loop entries, and the `.p2align` nop after a
  mid-function `b` / `jr` that this compiler emits and the original lacks.
- **Data layout / `.lit4` pool** (11): fixing these needs a change to the data layout, not the code: `.lit4` words out
  of text order (`vu1_packet`, `func_0034A570`, `loading_title`), the 128-bit save mode of `scene3d_c`, and
  `func_0037ABF0` copying `.sdata` strings.
- **Hand-written assembly** (7): the VU0 routines `func_00349CF8`-`func_0034A184` of `geometry_b` jump into each
  other's labels, and `func_0034A180` begins in the delay slot of `func_0034A148`'s return. They need one asm block per
  group of routines, not one per function.
- **Gas hazard** (4): `nop`s that modern gas inserts and the original assembler did not (`tekken_bowl`'s `cfc2`, and
  others). Each needs a new `--fix-asm` rule.
