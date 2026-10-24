# PC port: sound

The game's sound runs on the IOP: the EE sends every sound command through librspu2's `sceSdRemote` to the driver
`RSPU2DRV.IRX`, which drives the SPU2 with libspu2 and streams the BGM. The port has no IOP. It answers those commands
in C and runs a software SPU2. The full protocol, with the driver's addresses, is in
[docs/systems/audio.md](../systems/audio.md). This page covers what the port implements, how, and what is missing.

Files:

| File | Role |
|---|---|
| `port/src/spu2.c` | The SPU2 model: 2 MB sound RAM, two cores of 24 voices, PS-ADPCM, pitch, ADSR, volumes, the mix. `TTT_SELFTEST=spu`. |
| `port/src/plat_snd.c` | libspu2's commands on the model, the per-VBlank mix, the WAV dump, the ring that feeds the window's audio. |
| `port/src/plat_bgm.c` | The driver's BGM stream engine: tracks, the sound queue, the state machine, the double buffers. |
| `port/src/plat_file.c` | Receives every `sceSdRemote` command, handles file reads (0x204E, 0x2045 into SPU2 RAM) and the status word, and passes the sound commands on. |
| `port/src/plat_window.c` | The SDL3 audio stream, on the window thread. |

## 1. The protocol the game uses

Source: the game's calls (`src/sound.c`, `src/sound_if.c`, `src/file.c`) and the driver's disassembly
(`scripts/irx.py dis iso/IRX/RSPU2DRV.IRX`).

| Command | What the game sends | What the port does |
|---|---|---|
| 0x0001 | SpuInit (at boot, from `sceSdRemoteInit`) | Every voice: volume 0, pitch 0x3FFF, ADSR 0, start 0x5000. Writes a block of 07 bytes and a silent block at 0x5000. Master volumes 0, core 1's AVOL 0x7FFF, core 0. |
| 0x0002 | SpuSetCore(core) before each libspu2 command | Selects the core. Returns the previous one. |
| 0x0005 | Key off `mask` (stop effects) | Core 1, key off. The core stays 1, as in the driver. |
| 0x7128 | SpuSetCommonAttr (boot, per core): master volume 0x3FFF, CD volume 0x7FFF | Master volume. The CD volume of core 0 lands on core 1's AVOL, which is core 0's output inside core 1's mix (libspu2 writes it to 0x790 + core * 0x28). |
| 0x7240 | SpuSetVoiceAttr (boot): all voices, mask 0x9F (volume 0x1FFF, pitch 0x1000, start 0x5010) | Volumes `& 0x7FFF` (volmode 0), pitch, start, loop address (0x10000), raw ADSR (0x20000 / 0x40000). |
| 0x7440 | SpuSetKeyOnWithAttr (`Snd_KeyOn`): one voice of core 1, mask 0x93 (volume L = R, pitch, start) | The same as 0x7240, then key on. |
| 0x6418 | SpuGetAllKeysStatus, every frame (`Snd_UpdateVoices`) | 24 bytes. Key shadow on and ENVX != 0: 1. Shadow on and ENVX 0: 3. Shadow off and ENVX != 0: 2. Both off: 0. The game frees a voice slot on 0 or 3. |
| 0x2000 | Driver start | Finds the disc, SpuInit again, core 0 master volume 0x3FFF, the stream voices set up, no stream active. |
| 0x205E | 28 BGM tracks at boot: index, type and play count, volume scale, sector, biased size, loop sector | Stored (`plat_bgm.c`). |
| 0x7F0F | The sound queue, a 0x30-byte copy ending in 0xFF | Read by the BGM engine (section 3). |
| 0x2030 | Status, every frame | Bit 0: a read is pending. Bit 2: disc error. Bit 4: the BGM engine is in state 3 (a track is buffered and held). Bit 6: no stream active. |
| 0x204E / 0x2045 / 0x2050 / 0x2051 | File reads to EE memory / SPU2 RAM, abort, semaphore tick | `plat_file.c`. A read to SPU2 RAM lands in the model's RAM. |
| 0x2058 / 0x2059 / 0x205A | Movie stream | Refused. There are no movies yet. |

### 1.1 Sound effects and voices

`Snd_PlaySeEx` looks up an 8-byte descriptor in the bank table: offset, pitch, volume, category. The sample address
is the bank base + offset + 0x40, which skips the VAG header. `Snd_KeyOn` keys the voice on with that address, pitch
and volume. The banks are files loaded into SPU2 RAM by 0x2045 reads, at 0x5010 and after.

The game never sets ADSR. Every voice keeps SpuInit's ADSR1 = ADSR2 = 0:
- the attack is linear and the fastest,
- the decay goes down to 0x800,
- the sustain climbs back to the top at once,
- the release takes about 2 samples.

So an effect plays at full envelope until its ADPCM end flag (no repeat) silences the voice. The game then reads
key status 3 and frees the slot.

### 1.2 Mixing

There is no reverb (effects off, EVOL 0) and no noise or pitch modulation (NON = PMON = 0). Core 0 holds the BGM on
voices 22 and 23. Core 1 holds the effects on voices 0 to 21. Core 0's output enters core 1's mix at AVOL 0x7FFF.
Core 1's master volume gives the output.

## 2. The SPU2 model (`spu2.c`)

- **Format.** 48 kHz, 16-bit stereo. Integer arithmetic only: `tests/test_port_float.py` forbids host float code
  outside the VU interpreter.
- **PS-ADPCM.** 16-byte blocks of 28 samples. The filter pairs are (0, 0), (60, 0), (115, -52), (98, -55),
  (122, -60) in 64ths. Shifts 13 to 15 act as 9, and results are clamped to 16 bits.
- **Loop flags.**
  - Bit 2 (loop start): the loop address takes the block's address, unless the loop address was written.
  - Bit 0 (end): ENDX is set and the voice goes on at the loop address.
  - Bit 0 without bit 1 (repeat): the voice also falls silent (envelope 0, off).
- **Pitch.** 0x1000 is one source sample per output sample, capped at 0x3FFF. The model interpolates linearly between
  the last two samples. The chip uses a 4-tap Gaussian filter, so the tone differs slightly but the timing does not.
- **ADSR.** As on the SPU:
  - The step is `(7 - (rate & 3)) << (11 - shift)` going up, or `(-8 + (rate & 3)) << ...` going down.
  - It is applied every `1 << (shift - 11)` samples.
  - Exponential attack is 4 times slower above 0x6000. Exponential decrease is scaled by the level.
  - Phases: attack until 0x7FFF, decay down to `(SL + 1) * 0x800`, sustain until key off, release down to 0.
- **Volumes.** Fixed mode: bits 14-0 are the signed volume / 2. Sweep modes are not modelled; the game never uses
  them.
- **Key status.** The driver tests ENVX. The model counts a voice in its attack phase as sounding even while ENVX is
  still 0. On the PS2, the one or two samples before ENVX rises are hidden by the IOP's command latency.

## 3. BGM (`plat_bgm.c`)

The model follows the driver's stream engine. The details and IOP addresses are in audio.md section 5.2.

**Tracks.** Each track has a type:
- 1: read from disc.
- 2: SPU-resident. Track 0, the character select, plays 0x128040 / 0x18A290 directly.
- 3: streamed from EE memory at `gEeStreamBase`.

Each track also has:
- a play count (negative means it loops for ever),
- a volume scale (0x2000 to 0x3CCC),
- a loop sector. Type 3 tracks always loop to their start.

**Queue codes.**

| Code | Effect |
|---|---|
| `40 n` | Play track n. With bit 7 set, the track loads and holds; a later `40 n` for the same track releases it. |
| `43` | Stop. |
| `44 v t` | Fade to v over t * 4 ticks; with t = 0, set at once. |
| `45 v` | Option volume, 0 to 128. |
| `50` / `51` | Stereo / mono. |
| `5C n` | Movie audio. Ignored: there are no movies. |
| `5D` | Stop flag. |

**Timer.** The engine ticks every 66 H-lines, about 238.4 Hz or 3.98 ticks per VBlank. The ticks are spread evenly
over each VBlank's samples.

**States.**

| State | Meaning |
|---|---|
| 1 | Idle. A request starts the track and moves to 2. |
| 2 | Prefill. Lasts 9 ticks, the time a 24x CD needs for the driver's 64-sector first read. |
| 3 | Ready, and held while the hold flag is set. Status bit 4. |
| 4 | Playing. |
| 5 | Stopped. Goes back to 1. |

**Double buffers.** Voice 22 plays 0x1EE000 and voice 23 plays 0x1EE800, each as two halves of 1024 bytes, at pitch
0xEB3 (44.1 kHz). The voices use ADSR1 0x000F and ADSR2 0x0003. Each 2048-byte sector of a track is 1024 bytes of
left ADPCM followed by 1024 bytes of right.
- When voice 22 leaves a half, both buffers' halves get the next sector.
- As libspu2's SpuSt does, the flags of the first, second and last block of each half are rewritten: 6 / 2 / 2 in the
  first half, 2 / 2 / 3 in the second.
- At the end of the track, or on a stop, the last chunk ends in a silent block that loops on itself (`00 07`).
  Reaching it keys the voices off.

**Volume.** Every tick, `v = option * scale * fade >> 14`. In stereo, voice 22 gets (v, 0) and voice 23 gets (0, v).
In mono, both get v * 65 / 100 on both sides.

**Not modelled.**
- The driver's 512 KB IOP ring and its CD timing. A sector is read from the disc (or from EE memory for type 3) when
  its half is refilled.
- The SIF command 1 exchange for type 3 tracks. The engine reads EE memory directly.
- Movie audio (code 0x5C) is handled as a one-shot track; see [movies.md](movies.md).

## 4. Time and determinism

Mixing is driven by VBlanks only. `Port_SndVBlank`, called from `plat_hw.c` on the game's thread, mixes
48000 * 1001 / 60000 = 800.8 frames per VBlank, as exact integer counts of 800 or 801. Between pieces of at most 128
frames the BGM engine refills its buffers. Everything the game reads back (key status, status bits 4 and 6) is
therefore a function of the commands and the VBlank count: the same on every run, headless or in real time.

Output:
- `TTT_WAV=<file>` writes the whole run as a 48 kHz 16-bit stereo WAV. Headless runs work too. The header is
  finalised at the `TTT_FRAMES` report.
- With a window, the frames go into a 16384-frame ring. The game's thread writes it and the window thread drains it
  into an SDL3 audio stream. The ring's indices are 32-bit atomics. The window thread keeps at most 0.1 s queued in
  SDL and drops any excess. `TTT_AUDIO=0` opens no audio device.
- `SDL_AUDIO_DRIVER=dummy` or `disk` keep the speakers out of tests.
- `TTT_SND_TRACE=1` prints every sound command with its VBlank and, for 0x7xxx, the struct sent.

## 5. Effect on the game's timing

Before T32 the stub never reported status bit 4. The attract cycle's loading mode starts track 1 with the hold flag
at VBlank 736, then waits up to 300 frames for bit 4 (`loading_title.c`, `Bgm_IsPlaying`). The stub always let that
timeout run out, and the music started at VBlank 1083.

The engine now reports the track ready after its prefill, and the game starts it at VBlank 786, as on the console.
The 2551-VBlank idle run keeps its frames up to VBlank 600. From the frame at VBlank 750 on, the attract cycle runs
about 300 VBlanks earlier. Its report ends with `sbss-hash 0x4a16b393`.

Proof that bit 4 is the only cause: with bit 4 masked out, the same build gives the old 17 frames. Only three words
of the hashed `.sbss` differ:
- `gIopStatus` 0x5F6C60 reads 0 instead of 0x40, because bit 6 is now clear while track 1 plays.
- 0x5F6FD0 and 0x5F6FD4 hold the stored addresses of `func_003D9588` and `func_003DA138`, which moved by 0x40 bytes
  of new host code (gotcha 20).

## 6. Tests

- `TTT_SELFTEST=spu` uses synthetic ADPCM blocks. It checks the decoder, loop flags, ENDX, key status 1 / 2 / 3 / 0,
  pitch, attack / decay / release, and a deterministic, non-silent mix of both cores.
- `tests/test_port_sound.py` runs the replay through the title, the main menu and the character select headless with
  `TTT_WAV`, twice. It checks that the two WAVs are identical, that the boot is silent, that the title's effect
  sounds, and that the select screen's stereo BGM plays. The WAVs are written to a temporary directory and deleted.
  No audio from the game's data is ever committed.

## 7. Open

- Movie audio (with the movies).
- Reverb: the game leaves the effect unit off (ATTR bit 7 clear, EVOL 0), so the console plays none either; a model
  would only matter for other software.
- Gaussian interpolation, if exact tone matters.
- Volume sweeps, which the game does not use.
- Whether some track blocks other than the three rewritten per half carry loop flags.
- Measuring the prefill time (9 ticks) and the effect latency against the console.
