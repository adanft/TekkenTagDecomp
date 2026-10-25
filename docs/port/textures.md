# PC port: texture packs

T39. With the OpenGL renderer the port can write the game's textures out as PNG files (dump mode) and draw PNG
files of the same names in their place (replace mode), in PCSX2's folder layout and with PCSX2's file names. The code
is `port/src/gs_texpack.c` (names, files, a background thread) and the texture-pack part of `port/src/gs_gl.c`
(which textures get a name, the replacement in the shader).

Nothing here touches the game: the GS memory is only read, everything runs on the window thread (and the worker
thread for the files), and the state hash of a run is the same with dumping or replacing on (the GL fight replay:
`0x3c2cd520128fc04d` with dumps on and with a replacement for every dumped texture; `0xcce5eb554fa51395` since T42 b1,
[determinism.md](determinism.md) Values). The software renderer and headless
runs never name a texture.

## Switches

| Setting | Variable | Default | Effect |
|---|---|---|---|
| `video.texture_replace` | `TTT_TEXTURE_REPLACE` | 0 | Draw the replacements found in `replacements/`. |
| `video.texture_dump` | `TTT_TEXTURE_DUMP` | 0 | Write every new texture to `dumps/` (not those that have a replacement). |
| `video.texture_dir` | `TTT_TEXTURES` | (empty) | The textures folder; read at start. |

The overlay (F1, Video, "Texture packs") has the two switches and shows the folder. Both switches take effect at the
next picture; the replacements folder is read once a run, the first time replace mode is on, so new files are seen at
the next start.

## Folders

```
<textures>/SLUS-20001/dumps/<name>.png
<textures>/SLUS-20001/replacements/<name>.png       (any sub-folder, any depth up to 8)
```

`<textures>` is `video.texture_dir` / `TTT_TEXTURES`, else the repository's `textures/` when the program is
`build/port/ttt` of a checkout (git ignores it), else `textures/` beside the program. Dump mode creates `dumps/` and
an empty `replacements/`. This is PCSX2's layout below its textures folder (`textures/<serial>/{dumps,replacements}`),
so a PCSX2 pack folder for SLUS-20001 can be copied in as it is.

## Names

PCSX2's (checked against its sources, `pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp` and `GSTextureCache.cpp`:
`HashTextureLevel`, `HashCacheKey::Create`, `SourceRegion::Create`, `PaletteKeyHash`):

```
<tex>-<bits>.png                  16 / 24 / 32-bit textures
<tex>-<clut>-<bits>.png           indexed (PSMT8, PSMT4, PSMT8H, PSMT4HL, PSMT4HH)
<tex>[-<clut>]-r<w>x<h>-<bits>.png   a CLAMP region (see below)
```

`<tex>` and `<clut>` are XXH3-64 (seed 0) in lower-case hex without leading zeros (`%llx`), `<bits>` eight hex digits:

- **tex**, the texture's data at level 0, TW x TH texels at TBP0 / TBW / PSM of TEX0:
  - PSMCT32, PSMT8 and PSMT4 textures at least one block wide and high (8 x 8, 16 x 16, 32 x 16 texels), without an X
    region and with a Y region (if any) starting at 0: the 256-byte blocks of GS memory as they are stored, rows of
    blocks top to bottom, each row left to right (block numbers through the page layout and TBW).
  - All others (PSMCT24, PSMCT16 / 16S, PSMT8H / 4HL / 4HH, smaller textures, regions): the texels row by row, each 4
    bytes R G B A (16 and 24 bits expanded through TEXA: 5-bit channels shifted left by 3, alpha TA0 / TA1, AEM) or,
    indexed, one byte per index.
- **clut**: the 16 or 256 CLUT entries the texture uses (CSA applied) as 32-bit R G B A, 16-bit entries expanded
  through TEXA. Only in the names of indexed textures.
- **bits**: PSM | TW << 6 | TH << 10, and for 16 / 24-bit formats only TA0 << 15 | AEM << 23 | TA1 << 24.
- **region**: when CLAMP is region clamp narrower than TW / TH (or past it), or region repeat with MINU != 0 (the same
  for V), the name covers only those texels: U from MINU to MAXU (clamp) or from MAXU to MINU | MAXU (repeat), and
  `r<w>x<h>` gives their width and height, 0 for an axis without a region. The game draws its fighters and stages from
  such atlases (most of its textures have a region).

A dump is the texture (or region) as 8-bit RGBA, indexed texels through the CLUT, with the alpha as the GS keeps it:
0x80 is 1.0, so most dumps look half transparent in an image viewer. A replacement is read the same way (alpha 128 is
opaque for the game's blending, 255 doubles it). A replacement may have any size; it is mip-mapped and sampled with
bilinear filtering where the game filters the texture (TEX1), its own nearest texel where the game does not.

The names were checked against PCSX2's code, not against files dumped by PCSX2 (no PCSX2 run here). Points where a
PCSX2 pack may not match:

- PCSX2 names only what its texture cache sees as a plain texture. Here a texture is not named (never dumped nor
  replaced) when its texels lie in pages the GPU has drawn since the CPU last wrote them (render-to-texture, the frame
  buffer, the fonts the game keeps in the frame buffer's top byte), when its CLUT was loaded from such pages, in the
  depth formats, or with TW / TH above 1024. The game declares some 2D pictures (the select portraits, the versus
  screen) as 1024 x 1024 views over GS memory that reach into the frame buffer: their name would change with every
  frame (in PCSX2 too), so they are not named.
- Not read: the older region names (`-r<64-bit>-`), mip levels (`-mip<n>`), DDS files, `<serial>.zip`. The game uses
  no mip-maps.
- PCSX2 options that change names (dumping only palette or only direct textures, "dump with FMV active", the
  replacement pre-cache) have no counterpart: everything is dumped, and every replacement is decoded in the
  background as soon as the folder has been read.

## How it works

- `gs_gl.c` `tp_lookup`, at each textured primitive read from GS memory: a slot per texture (TEX0's texture fields,
  CLAMP, TEXA for 16 / 24 bits, the CLUT row's XXH3 for indexed ones; 4096 slots) keeps the name until a page under it
  changes (every upload, local copy, read back and resolve stamps its pages). A new or changed texture is named
  (`Tp_Name`, from GS memory's CPU copy), dumped when new (`Tp_Dump`: the RGBA is made on the window thread, the
  worker compresses and writes it, as `<name>.png.tmp<pid>` renamed into place) and looked up among the replacements
  (binary search).
- A primitive with a replacement gets its GL texture in its draw call (a texture unit, `uRepl`) and bit 0 of the state
  record's `s2.z` (with the region's origin and size in `s2.z` / `s2.w`); the fragment shader then samples the
  replacement instead of decoding GS memory, at the texel coordinate the GS would use, CLAMP applied. A primitive with
  a replacement and TCC tests its alpha per pixel (the replacement's alpha range is not known).
- The worker (one thread, started on the first need) reads the replacements folder, sorts the names, then decodes
  every replacement (stb_image, PNG); the window thread uploads one the first time it is drawn after decoding. Until
  then the original texture is drawn. At the end of a run the report waits up to 30 s for the queued dumps.
- Cost: in the fight replay the window thread names about 50 textures per VBlank (pages the game rewrites each frame),
  about 0.9 ms of its CPU time per VBlank with dumping on.
- PNG I/O: `third_party/stb` (stb_image v2.30, stb_image_write v1.16; public domain or MIT, `third_party/stb/LICENSE`),
  built inside `gs_texpack.c` with every function static (the unused HDR / JPEG code and its float maths are dropped);
  its C library calls, like all of `gs_texpack.c`'s, go through the GOT (`-fno-plt`, `NOPLT_FILES`), so the program's
  PLT does not grow.

The report line, with either switch on:

```
ttt: gl: texture packs: <n> namings, not named: <n> in drawn pages, <n> CLUT from the GPU, <n> other (format, size, region)
ttt: textures: <folder>; dump on, replace off; <n> textures named, <n> dumped (<n> on the disk already, <n> failed); <n> replacements (<n> decoded, <n> failed, <n> other files), <n> uploaded, <n> primitives drawn with one
```

## Tests

- `TTT_SELFTEST=texpack`: names on a made-up GS memory (the block order of a PSMCT32 texture, the expanded path of a
  small one, TEXA in the bits, the CLUT hash, regions), names read back from file names, malformed names refused.
- `tests/test_port_gl.py` `TexturePacks`: the scripted start of the game to the versus screen with the GL renderer in
  a hidden window: two dump runs write the same files under PCSX2-shaped names and change neither the pictures nor the
  state; a one-colour replacement twice the size for every dumped name (made at test time) turns the fighter on the
  character select magenta, with the same state.

Dumps and replacements are made from your own game: they are never committed (`textures/` is in `.gitignore`).
