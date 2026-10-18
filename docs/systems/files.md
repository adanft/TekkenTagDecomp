# Files: TEKKEN.BIN

All game data except the executable and the IOP modules is in one archive, `TEKKEN.BIN` (307,849 sectors,
630,474,752 bytes). It has no directory: the game asks for files by id (0-747) and finds them through a table in
`SLUS_200.01`. `scripts/extract_disc.py` reads that table from your own executable and writes every file out,
descrambled, as the game sees it after a load.

Tags as in [game_overview.md](../game_overview.md): **[V]** read in the disassembly or checked on the user's disc
by the extractor, **[I]** inferred, not proven.

## Extracting

```
.venv/bin/python -I scripts/extract_disc.py [--verify]
```

Inputs `iso/SLUS_200.01` and `iso/TEKKEN.BIN` (from `scripts/setup_disc.py`); output `assets/files/NNN.bin`
(NNN = id, three digits; missing files are written empty) and `assets/files/manifest.tsv`: id, byte offset in
`TEKKEN.BIN`, size (-1 or 0 for no file), scrambled (1 if descrambled), table sector, kind guess, first 4 bytes.
`--verify` re-scrambles every output and compares it with `TEKKEN.BIN`. The repository holds only the table's
address, count and format, never its contents. `assets/` is ignored by git.

## File table

`D_00509E40`, 748 entries of 16 bytes, indexed by id [V]:

| Word | Meaning |
|---|---|
| 0 `sector` | Start sector: `TEKKEN.BIN` byte offset = `(sector - 0x4820) * 2048` [V] |
| 1 `size` | Byte size + 0x350A [V] |
| 2 `check` | `id * 0x40000 + 0x11F3C + ceil(size / 2048)` (32-bit) [V on all 748; nothing on the load path reads it] |
| 3 `ee_index` | The id itself for files loaded into EE memory, 0xFFFF for files only the IOP uses [I] |

- **Sector** [V]: the IOP read handler of RSPU2DRV (command 0x204E/0x2045, `.text` 0x2DFC) computes the disc LBA as
  `sector + lsn - 0x4820`, where `lsn` is `TEKKEN.BIN`'s start found once with
  `sceCdSearchFile("\TEKKEN.BIN;1")` (`.text` 0xED8, stored in `.bss`). So the table is independent of where the
  archive sits on the disc. On the user's disc the last file ends exactly at the end of `TEKKEN.BIN`, and no two
  files overlap.
- **Size** [V]: the EE helpers `File_GetSize` 0x398378 (`size - 0x350A`), `File_Exists` 0x398358
  (`size - 0x350A > 0`), `File_LoadSync` 0x398398 and `File_LoadSyncIop` 0x398400 (load, wait, return the size)
  all subtract the bias. The EE passes the biased word to the IOP, which subtracts 0x350A itself (`.text`
  0x2E18) and reads `ceil(size / 2048)` sectors. 140 entries have size 0 and 4 have size -1 (word 0x3509), both
  "no file" for `File_Exists`. Entries without data repeat the previous entry's sector (the 144 repeats).
- **Check word** [V]: matches the formula on every entry; `extract_disc.py` refuses a table that does not. No
  reader was found on the EE load path or in the IOP read handler [I: a leftover from the mastering tool].
- **EE index** [I]: the id itself on 558 entries, 0xFFFF on 190. Every file the EE loads with `toIop = 0` in
  `func_003B94E8` (ids 3-10) and `Mode_Boot` (id 12) has it; ids 1 and 2, loaded with `toIop = 1`, have 0xFFFF.
  On the disc, all 418 non-empty entries with it are scrambled and all 186 without it are not. Id 0x2C is the
  exception the descrambler hard-codes, and its word is 0xFFFF too.

## Scrambling

After a read into EE memory (not `toIop`) `File_Update` descrambles the first `min(size, 0x800)` bytes [V] (the
biased size is used, so in practice always 0x800 bytes of the sector buffer):

```
key = (id + 0xF3C) * 218319 + (id - 0x348F) * 0x1234567       ; File_KeySeed, 64-bit
for each 32-bit word w in the first 0x800 bytes:
    w ^= key & 0xFFFFFFFF
    key = key * 0x71C3AB9 + 0x3941                             ; File_KeyNext, 64-bit
```

Id 0x2C is skipped [V]. The extractor descrambles a file when its EE index equals its id and the id is not 0x2C
[I, see above]. Checks on the user's disc [V]: no scrambled file is recognisable as stored, and after
descrambling 230 parse as packs and 50 as GS image uploads; `--verify` re-scrambles all 748 back to the
archive bytes.

## Compression

None: files are stored as the game uses them [V for the 230 packs: every offset/size pair lies inside the file
and the furthest one ends exactly at the file size; I for the leaves]. The load helpers call no decompressor
[V], and a scan of the game code for byte-copy loops with flag bits found one [I: a heuristic scan]:
`Lz_Decompress` 0x364448 [V], called once, by `func_00366A20` from `Mode_Boot`, on data inside the executable
(0x416E50: 0x885 bytes unpack to 0x19D0, a GS image upload):

```
loop: flags = *src++; if flags == 0: end           ; returns the unpacked size
      while flags >= 2:                            ; bit 0 first; the highest set bit is a stop marker
          if flags & 1: *dst++ = *src++            ; literal
          else: v = src[0] << 8 | src[1]; src += 2 ; big-endian
                dist = v & 0x7FF or 0x800; len = (v >> 11) & 0x1F or 32
                copy len bytes from dst - dist (byte by byte, may overlap)
          flags >>= 1
```

## Contents

Kind guesses by `extract_disc.py` on the user's disc (first 64 KB of each file):

| Kind | Files | What it is |
|---|---:|---|
| pack | 230 | `u32 count`, then `count` pairs `{u32 offset, u32 size}` from the file start; nested packs are common [V for the layout] |
| vag | 156 | Sony `VAGp` sound (IOP side) |
| empty | 144 | size 0 or -1 |
| unknown | 140 | Mostly `u32 count` + fixed-size records (floats); file 0 is "NAMCO TEKKEN TAG TOURNAMENT" + 48 MB of noise [I: filler] |
| gs-image | 50 | GIF tag (NLOOP 1, 4 x A+D: BITBLTBUF, TRXPOS, TRXREG, TRXDIR) then an IMAGE tag and pixels: a texture upload |
| adpcm | 25 | Headerless SPU2 ADPCM blocks (IOP side, streamed or uploaded) |
| mpeg-ps | 3 | 0x3800 bytes of ADPCM, then MPEG-2 program stream packs: movies with interleaved sound [I] |

## Related loader code

`File_Request` / `File_Update` and the IOP-side uploads are in [game_overview.md](../game_overview.md) section 7.
`File_RegisterIopFiles` registers 28 streamed files with the IOP (one of them 0xCF7 sectors into its file).
A `toIop = 1` read (command 0x2045) goes into SPU2 sound RAM, not IOP memory: `dest` is an SPU2 byte address [V];
the 64-byte round-up is probably the SPU2 transfer block size [I]. The request flags are stored by the IOP and never
read [V].
See [audio.md](audio.md).

## Open questions

- Files 0 and 747 (no structure found) and whether anything reads them.
- Formats inside the packs and the record files: later tasks (models, motions, sound).
