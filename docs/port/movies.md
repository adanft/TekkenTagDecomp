# PC port: movies

The game plays three full-motion movies. On the PS2 they go through Sony's libmpeg and the IPU; the port has
neither. It decodes the video natively, hands the pictures to the game's own display code, and plays the audio on
its SPU2 model. This page covers the format, what the game does on the PS2, and what the port does instead.

## 1. The files

The movies are files 14, 15 and 16 of `TEKKEN.BIN` (`File_GetMovieSector(n)` = the sector of file 14 + n).

| Movie | Where it plays | Pictures | Length |
|---|---|---|---|
| 0 | the intro, at boot (`Mode_Title`'s first pass), and in the title's attract cycle | 2034 | 67.8 s |
| 1 | the title's attract cycle | 848 | 28.3 s |
| 2 | the title's attract cycle; an ending (`loading_title.c`, character 0x21) | 1350 | 45.0 s |

The theater mode (`theater.c`) plays them on request too.

**Interleave.** Each file is a run of chunks of 0x87 sectors: **7 sectors of audio, then 0x80 sectors of video**.
These are the last two arguments of the stream start command `0x2058` (`Movie_StartStream`: sector, movie, 7, 0x80).
The IOP driver keeps the audio and sends the EE only the video: `D_00518EA8[n].size` bytes of it (movie 0:
0x4294004).

**Video.** The video bytes are a PSS (an MPEG-2 program stream) of 0x4000-byte packs, each with four PES packets of
stream 0xE0 (the first pack also has a system header). There is no audio or private stream in it. The elementary
stream is MPEG-2 main profile:
- 640 x 448, 4:2:0, frame rate code 5 (30 pictures a second), one sequence header per file. Sequence end codes
  appear mid-stream (12 in movie 0) and are ignored.
- **I, P and B pictures** (movie 0: 135 I, 578 P, 1321 B). So these movies are **not intra-only**, unlike the
  reference port's (Budokai Tenkaichi 3).
- Every picture: a frame picture (`picture_structure` 3), `frame_pred_frame_dct` 1, `progressive_frame` 1,
  `intra_dc_precision` 0, `q_scale_type` 0, **`intra_vlc_format` 1** (table B-15 for intra blocks),
  `alternate_scan` 0, no concealment vectors. The `f_code`s change per picture and per direction (1 to 4).

**Audio.** PS-ADPCM, stereo, 44.1 kHz (the stream voices' pitch 0xEB3). Each 2048-byte audio sector is 1024 bytes
of left and 1024 bytes of right, as the BGM tracks. The first block of each channel carries the loop-start flag.
The driver plays only part of the audio: its own table at `.rodata` 0x9EA0 of `RSPU2DRV.IRX` has, per movie, a
16-bit volume scale (0x3333 for all three), a 16-bit value of unknown use, the file size, and the number of ADPCM
bytes to play. After that many bytes, movies 0 and 2 continue with data that is not ADPCM.

## 2. On the PS2

`func_0039C7D8(n)` asks for movie n. `Movie_Update`, called every frame from `Frame_End`, then runs
`func_0039C878`, which blocks until the movie ends:

1. It places the buffers in the heap above `D_005F6504`: the frame queue (5 pictures of 0x118000 bytes and their
   0x85740-byte GS packets), the IPU input ring, the 0xF0000-byte read ring.
2. It removes the game's VBlank handler, sets up the display (`func_0039D320`: two 640 x 224 PSMCT32 frame buffers,
   FBP 0 and 70, the GS context of the drawing) and clears it (`func_0039D9F0`).
3. If the BGM stream goes idle (`Snd_WaitStreamIdle`), `Movie_Open` starts the read thread and the decoder thread
   (`func_0039EB78`), sends 0x2058, and installs the movie's VBlank handler `func_0039E268`.
4. `func_0039CAC8` reads 0x10000-byte chunks with 0x2059 into the read ring, demultiplexes them with
   `sceMpegDemuxPssRing` into the IPU ring (DMA channel 4), and polls the pads: from the 11th decoded picture on,
   Start (`Input_SkipPressed`) stops the movie and sets `D_005F6CC4`. Once the frame queue has a picture, it starts
   the audio (`Movie_PlayAudio`: sound queue code 0x5C n) and the display.
5. The decoder thread takes each picture with `sceMpegGetPicture(rgb32, 0x460)`: RGBA32 from the IPU's colour space
   conversion, in 16 x 16 macroblocks of 0x400 bytes, **column by column** (the upload packets place macroblock k
   at x = k / 28, y = k % 28). At the first picture it builds every slot's GS packets with `func_0039D580`: field 0
   uploads the picture as a texture at TBP 0x2300 and draws its even lines into FBP 0 (a sprite that halves the
   height, texel centres on the even rows); field 1 draws the odd lines from the same texture into FBP 70.
6. The VBlank handler shows FBP 0 on even fields and FBP 70 on odd fields and kicks the packets (`func_0039E200`):
   each picture shows for two fields, 29.97 pictures a second.
7. At the end: 0x205A(0), the threads and the handler removed, a white or black screen (`D_00518EA8[n].unk4`, unless
   skipped), 0x205A(1) until the driver is back in file mode, `Snd_WaitStreamIdle`, the game's VBlank handler back.

## 3. On PC

The port keeps steps 1-3 and 7, and the display side of 5 and 6. It replaces the threads, the IPU and the read path.

**The player** (`src/movie.c`, `Port_MoviePlay`, under `PORT` in the draft `func_0039C878`):
- `Movie_StartStream` sends 0x2058 as on the PS2. `plat_file.c` keeps its arguments.
- Each picture is written into the first slot of the frame queue by `plat_movie.c`.
- At the first picture, the game's `func_0039D580` builds the two packets, as the decoder thread does. The audio
  starts then (`Movie_PlayAudio`, `Snd_Flush`).
- Per picture: both packets are kicked (`func_0039E200`), then FBP 0 is shown for one VBlank and FBP 70 for the
  next (`func_0039D9B8`). After each VBlank, Start is checked as on the PS2.
- At the end, or when skipped: 0x205A(0), and back to `func_0039C878`.
- Everything runs on the game's thread. The pictures depend only on the disc and the VBlank count, so every run gives
  the same frames and sound.

**The pictures** (`port/src/plat_movie.c`):
- The video bytes are read from the disc, skipping each chunk's 7 audio sectors, up to `D_00518EA8[n].size`.
- The PSS is split by hand. The PES payloads of stream 0xE0 go to the decoder.
- `third_party/pl_mpeg2` decodes the MPEG-2 stream (section 4).
- Each picture is converted to RGBA32 and written in the IPU's macroblock order:
  - ITU-R BT.601 (Y 16-235, Cb / Cr 16-240) in 8-bit fixed point;
  - each chroma sample covers 2 x 2 pixels;
  - alpha 0x80.
  The IPU's exact rounding is not modelled [I].

**The audio** (`port/src/plat_bgm.c`, code 0x5C n). The movie's audio is a stream track of its own (type 4) on the
BGM engine's voices, buffers and state machine:
- Audio sector j of the track is file sector (j / 7) x 0x87 + j % 7.
- It plays once. The length and volume scale come from the driver's table, read from the user's own
  `IRX/RSPU2DRV.IRX` on the disc (directory or ISO). The entry is used only if its file size matches the game's file
  table.
- Without that table, the track plays every audio sector of the file at scale 0x3333.
- The volume is the BGM's: option volume x scale x fade.
- `Snd_WaitStreamIdle` after the movie sends `Bgm_Stop`. That stops a skipped movie's audio.
- The driver's movie-mode CD timing is not modelled. The audio starts after the engine's 9-tick prefill.

`docs/port/sound.md` still says code 0x5C is ignored; this page supersedes it.

**The GS kicks.** The port's `sceGsSyncPath` returns at once, and a DMA transfer is carried out at the next poll.
`func_0039D320` kicks its set-up packet, and `func_0039D9F0` kicks again before any poll, so the set-up was lost
and the movie's sprites drew nothing. `Port_GifSync` (`plat_hw.c`) now carries out a started GIF transfer at once,
without moving the clock. `func_0039C878` and the player call it after each kick ([README](README.md) gotcha 27).

**`TTT_MOVIES=0`** skips every movie as before T33: `Port_MoviePlayable` reports the "movie (skipped)" placeholder,
and the game behaves as when the stream never goes idle. The test helpers run with it unless a test asks for movies.

## 4. The decoder

`third_party/pl_mpeg2/m2v.c` is PL_MPEG's MPEG-1 video decoder (Dominic Szablewski, MIT) extended to the MPEG-2
subset above. Its license file, `third_party/pl_mpeg2/LICENSE`, is MIT, and so is everything added. libmpeg2 (GPL)
was avoided: the repository has no license file of its own.

From PL_MPEG:
- the bit reader;
- the VLC trees: macroblock address increment, macroblock types, coded block pattern, motion codes, DC sizes, and
  table B-14;
- the half-pel prediction;
- the AAN integer IDCT with its pre-multiplied scale factors.

Added for MPEG-2:
- the sequence, picture coding and quant matrix extensions;
- one `f_code` per vector component;
- the non-linear quantiser scale;
- MPEG-2 inverse quantisation: truncating division, saturation to -2048..2047, mismatch control on coefficient 63;
- table B-15, built into a tree when the decoder is created;
- the 6 + 12-bit escape.

It has no host float (`tests/test_port_float.py`), and builds with the port's flags at -O3 (`port/tools/undefined.py`).

It refuses streams outside that subset, and the movie then ends at once:
- field pictures, field prediction, frame DCT off;
- `intra_dc_precision` other than 0;
- alternate scan, concealment vectors;
- chroma formats other than 4:2:0.

**Checked against FFmpeg** (scratch only, nothing kept). All three movies decode to their full picture count (2034,
848, 1350). Against `ffmpeg`'s YUV output, the mean absolute difference per picture is 0.03-0.06, with a largest
difference of 7 levels. The gap is the IDCT's rounding, carried through the P and B pictures.

**In the game.** At VBlanks 499-502, each dump matches its reference picture's field, line-doubled. The fields go even,
odd, even, odd, and the mean difference is 1.0 level per channel (the colour conversion). The OpenGL back end's dumps
differ from the software rasteriser's by 0.5 on average. Decoding takes about 1.6 ms a picture on the test machine.

## 5. Tests

`tests/test_port_movie.py` (with the disc):
- **Intro determinism.** The boot reaches the intro. The frames at VBlanks 400 and 600 are pictures, not blank and
  not equal. The movie's audio sounds between them. A second run gives the same frames and WAV byte for byte.
- **Skip.** Start at VBlank 260 skips the intro: the skipped flag is set, the audio stops, and the title goes on to
  the main menu.
- **`TTT_MOVIES=0`.** The boot reaches the main menu at VBlank 183, as before.

No frame or sound of the game is kept; the tests write to temporary directories.

## 6. Open

- Movies 1 and 2 are checked through the decoder against FFmpeg, not yet through the game's own paths. A 14000-VBlank
  idle run with movies reaches neither: after the intro, the title runs the attract demo fight (mode 4), not a movie.
- The driver's movie mode: its CD timing and the exact moment its audio starts. The second 16-bit field of its table
  is unknown.
- The IPU's colour conversion rounding and its alpha thresholds (the port writes alpha 0x80).
- The PS2's 29.97 pictures a second against the audio's 44.1 kHz: the same drift as on the console, about 70 ms over
  movie 0.
