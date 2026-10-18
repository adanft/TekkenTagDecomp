# Audio and the RSPU2DRV protocol

The game has no sound code of its own on the IOP side except `RSPU2DRV.IRX`, and no disc code on the EE side:
every sound command, every file read and the movie stream go through one EE call, librspu2's
`sceSdRemote(mode, command, ...)`, to RPC server 0x80000601 in `RSPU2DRV.IRX`. A PC port that drops the IOP must
implement the 18 commands listed below. Other modules: [iop.md](iop.md). Archive format: [files.md](files.md).

Tags: **[V]** read in the disassembly (EE: `asm/`, IOP: `scripts/irx.py dis iso/IRX/RSPU2DRV.IRX`), **[I]**
inferred. IOP addresses are module-relative (`.text` loads at 0); `IOP 0x2DFC` means offset 0x2DFC in the module.

## 1. RSPU2DRV.IRX

- `.iopmod` name `ZsRspu2Driver`, version 1.01; banner "rspu2 driver module version 1.2.0 (C)SCEI" [V]. Sony's
  remote-SPU2 sample driver, extended by Namco ("TEKKEN TAG TOURNAMENT", "NAMCO LIMITED", "2000, 11, 02(US)",
  `\TEKKEN.BIN;1` in `.rodata`) [V strings; I: who changed what]. libspu2 and libsnd2 are linked in statically
  (`PsIIlibspu2 1600`, `PsIIlibsnd2 1600`); it imports no `libsd` [V].
- Imports (`irx.py info`): cdvdman (`sceCdInit`, `sceCdRead`, `sceCdGetError`, `sceCdSearchFile`, `sceCdSync`,
  `sceCdDiskReady`, `sceCdStatus`, `sceCdMmode`), sifcmd, sifman (`sceSifDmaStat`), thbase, thsemap, timrman,
  intrman, loadcore, sysmem, stdio. No `sceSifBindRpc`/`sceSifCallRpc`: the IOP never calls the EE's callback
  RPC 0x80000603 [V].
- Start (IOP 0x0): registers SIF command 0 handler IOP 0x2F48 (sets "EE data arrived" flag `0xA0B4`) and starts
  the RPC thread IOP 0x808 (priority 0x22, stack 0x800), which registers RPC **0x80000601** with server function
  IOP 0xD0, buffer IOP 0xA8E0, then `sceSifRpcLoop` [V].
- Dispatcher IOP 0xD0 `(command, data)`: a binary search on the command number (38 cases, section 3); `data`
  word 0 is junk, words 1..6 are the arguments. Unknown numbers print "SPU driver error: unknown command %x %d" [V].
  The server returns a pointer to the 16-byte result block at IOP 0xA0A4 (word 0 = result), or to a struct for
  the 0x6xxx "get" commands [V].
- Stream thread IOP 0xB1C (priority 0x52, stack 0xA00), started by command 0x2000: hardware timer from
  `AllocHardTimer(4, 16, 1)`, compare 66, mode 0x158; the timer interrupt (IOP 0xF70) wakes the thread, which
  takes semaphore `0xA7F0`, runs the BGM voice machine (IOP 0x182C), then the file/BGM-disc service (IOP 0x1EC0)
  or the movie service (IOP 0x2A68) and the EE-memory BGM service (IOP 0x24A0), and sleeps [V]. Timer source 4 =
  H-line, so about 15734 / 66 = 238 ticks per second [I].
- Semaphore `0xA7F0` serializes every 0x20xx command with the stream thread [V].
- IOP memory: CD read buffer 0x8B160, two banks of 0x48000 bytes; BGM ring buffer 0x80000 bytes at 64-byte
  aligned 0xAF80 [V]; state block 0x11B600-0x11B700; BGM track table 0x11B700 (28 x 20 bytes) [V].

## 2. Transport: `sceSdRemote` (EE 0x404BB8)

`int sceSdRemote(int mode, int command, a0, a1, a2, a3, a4, a5)` [V]:

- Packet `D_01FCAEC0` (0x40 bytes): word 0 = its own address, words 1..6 = the low 32 bits of the six
  arguments. Float argument registers are saved but not sent [V].
- `mode` 1 = blocking `sceSifCallRpc`; 0 = `SIF_RPC_M_NOWAIT` with end function `D_004F56B0` (set by
  `sceSdRemoteSetEndFunc` 0x404BA8). The game always passes 1 [V: every call site].
- Routing by command [V]:

| Command | Sent | Received | Return |
|---|---|---|---|
| 0x6xxx (get) | packet, 0x40 bytes | 0x40 bytes into the buffer `a0` points to | junk |
| 0x7600 | `a1 * 64` bytes from `a0` | nothing | junk; prints "rSpuSetMultiVoiceAttr" |
| other 0x7xxx (set) | 0x40 bytes from `a0` (a struct) | nothing | junk (caller's `$s3`) |
| 0x8100-0x8600 | stores `a0` as an EE callback (0x8100, 0x8200, 0x8600 are also sent) | | 0 |
| anything else | packet, 0x40 bytes | 0x10 bytes into the packet | word 0 = IOP result |

- `sceSdRemoteInit` 0x404AD8 binds 0x80000601 (retrying) and sends command 1. `sceSdRemoteCallbackInit`
  0x404E50 (no callers) would start a thread serving RPC 0x80000603 with six callback slots [V].

## 3. Commands

"Sites" counts `sceSdRemote` call sites in the game (a loop counts once). Unused cases are in the driver's table
but no game call reaches them [V: no other command constant reaches `sceSdRemote`]. libspu2 names are
[I: struct sizes, arguments and the call pattern]; all IOP handler addresses are [V].

### 3.1 Used by the game

| Cmd | Args (from the EE callers) | IOP action | Returns | Sites | When | Tag |
|---|---|---|---|---|---|---|
| 0x0001 | - | `SpuInit` (IOP 0x4020) | stale | 1 `sceSdRemoteInit` | boot | V/I |
| 0x0002 | core 0/1 | `SpuSetCore` (IOP 0x4058): sets the core later libspu2 calls use | previous core | 7 | before every SPU command | V |
| 0x0005 | 0, voice mask | `SpuSetKey(SPU_OFF, mask)` (IOP 0x46C0), on core 1 | stale | 2 | SE stop | I |
| 0x6418 | buffer (>= 0x40 bytes) | `SpuGetAllKeysStatus` (IOP 0x5708), core 1: 24 status bytes | struct at IOP 0xAF20 | 1 `Snd_UpdateVoices` | every frame | I |
| 0x7128 | `SpuCommonAttr*` (0x28 bytes) | `SpuSetCommonAttr` (IOP 0x4E50) | - | 1 (per core) | boot | I |
| 0x7240 | `SpuVoiceAttr*` (0x40 bytes) | `SpuSetVoiceAttr` (IOP 0x6B3C) | - | 2 | boot, SE volume | I |
| 0x7440 | `SpuVoiceAttr*` | `SpuSetKeyOnWithAttr` (IOP 0x4970), core 1 | - | 1 `Snd_KeyOn` | SE play | I |
| 0x7F0F | pointer to the 0x30-byte sound queue copy | byte-code interpreter IOP 0xC28 (section 4) | 0 | 1 `Snd_Flush` | frames with queued BGM commands | V |
| 0x2000 | 0x2237, EE address (uncached `D_008BD080`), 0x446E | once (flag `0xA0A0`), magic checked: IOP 0x890: `SpuInit`, `sceCdInit` until ready, `sceCdMmode(1)`, `sceCdSearchFile("\TEKKEN.BIN;1")` until found (LSN kept at 0x11B608), start the stream thread, key off voices 22-23, master volume 0x3FFF, set up the stream voices; 2nd arg kept at 0x11B604; 3rd arg unused | 0, -1 bad magic | 1 `Snd_InitDriver` | boot | V |
| 0x2030 | - | status word, IOP 0x2ED0 (bits below) | status | 6 | every frame (`File_Update`), movie waits | V |
| 0x204E | sector, EE dest, size + 0x350A, flags | queue a read to EE RAM (IOP 0x2DFC) | 0, -1 busy | 1 `File_Update` | file loads | V |
| 0x2045 | sector, SPU address, size + 0x350A, flags | queue a read into SPU2 RAM, size rounded up to 64 | 0, -1 busy | 1 `File_Update` | sample bank loads | V |
| 0x2050 | - | abort the running EE read (only state 2) | 0 | 1 `File_Release` | release of a busy slot | V |
| 0x2051 | - | take and release the semaphore (waits for the stream thread's tick) | 0 | 1 `File_Init` | boot | V |
| 0x205E | index 0-27, type/vol, 2 halves, sector, size word, loop sector | store BGM track `index` (IOP 0x3110, section 5.2) | 0 | 1 (x28) `File_RegisterIopFiles` | boot | V |
| 0x2058 | sector of file 14 + n, movie n (0-2), 7, 0x80 | start the movie stream (IOP 0x2610, section 5.3) | 0, -1 | 1 `Movie_StartStream` | movie start | V |
| 0x2059 | EE dest, 0x20 sectors | request the next movie chunk (IOP 0x2F58) | 0, -1 busy | 1 `Movie_ReadChunk` | movie, per chunk | V |
| 0x205A | 0 or 1 | 0: stop the movie stream; 1: wait for the drive, then return to file mode (IOP 0x2728) | 0 | 2 | movie end | V code / I meaning |

Read `flags` (4th word of 0x204E/0x2045) is stored at 0x11B6A0 and never read again [I: no load of it found].

Status word (0x2030) [V code, I meaning]: bit 0 = a file read is pending (`File_Update` waits for it to clear);
bit 2 = disc/stream error flag (0x11B60E, set when `sceCdStatus` reports a problem); bit 4 = BGM playing
(`Iop_BgmBusy`); bit 5 = movie mode and a chunk pending; bit 6 = stream mode idle (`Iop_StreamBusy` returns
`!bit6`).

### 3.2 In the driver, not used by the game

0x0006 (IOP 0x4590), 0x0008 (0x4084, reverb start address), 0x000A (0x7240, two halfwords), 0x000B (0x4660),
0x000E (0x4C40, `SpuSetTransferMode`), 0x0010 (0x4C10, `SpuSetTransferStartAddr`), 0x0011 (0x49A0, `SpuWrite`
from an IOP address), 0x0012 (0x4AA0), 0x0013 (0x4D10), 0x0017 (0x40DC), 0x0018 (0x4C80), 0x0020 (0x4110),
0x1010 (0x42C0), 0x1011 (0x41D0), 0x101C (0x48C0), 0x1031 (0x3F00), 0x4008 (0x3190, a no-op), 0x6128 (0x52C0,
`SpuGetCommonAttr`), 0x6314 (0x4610, a 0x14-byte get), 0x7508 (0x4250, an 8-byte set). Names given are [I].
The EE library also knows 0x6240, 0x7600 and 0xE621, which the driver rejects as unknown [V].

## 4. The sound queue (command 0x7F0F)

The EE writes byte codes to `gSndQueue` 0x436500 (write pointer `gSndQueuePtr`, count `gSndQueueCount`); `Snd_Flush`, called
from `Frame_End` and before movie waits, appends 0xFF and sends the first 0x30 bytes when the count is non-zero
[V]. The IOP walks at most 48 bytes until 0xFF; unknown codes (0x41-0x42, 0x46-0x4F, 0x52-0x5B) hang the loop
[V]. All codes act on the BGM / stream engine:

| Code | Bytes | EE writer | IOP effect | Tag |
|---|---|---|---|---|
| 0x40 n | 2 | `Bgm_Play(n)` (after a 0x45 with the option volume); `Bgm_PlayFlag` sets bit 7 of n | request BGM track n (0x11B65A); bit 7 sets 0x11B65C | V code, I meaning |
| 0x43 | 1 | `Bgm_Stop` (also clears the EE stream base) | stop request (0x11B65E = 1, track -1) | V/I |
| 0x44 v t | 3 | `Bgm_SetVolumeFade(v, t)` | t > 0: fade BGM volume to v over t * 4 ticks; t = 0: set at once | I |
| 0x45 v | 2 | `Bgm_SetVolumeLevel(level)`: v = byte table `D_00518E58` for option level 0-10 (0, 32, 40 ... 112, 128) | BGM volume, clamped 0-128 | V |
| 0x50 / 0x51 | 1 | `Snd_SetStereo(on)` (options screen) | stereo flag 0x11B610 = 1 / 0 | I |
| 0x5C n | 2 | `Movie_PlayAudio(n)` (with a 0x45) | start the audio of movie n on core 0; length from the driver's own per-movie table (`.rodata` 0x9EA0, 12 bytes each) | I |
| 0x5D | 1 | `Bgm_Abort` (no callers) | stop flag only | V |

## 5. Data paths

### 5.1 File reads (0x204E, 0x2045)

`File_Update` issues one read at a time and polls status bit 0 every frame [V, game_overview.md section 7].
On the IOP the stream thread, in normal (non-movie) mode [V, IOP 0x1EC0]:

1. LSN = `TEKKEN.BIN` LSN + sector - 0x4820; sectors = ceil((size word - 0x350A) / 2048).
2. `sceCdRead` of up to 128 sectors (the whole file when it is at most 256 sectors) into a 0x48000-byte bank of
   the buffer at 0x8B160, alternating banks; `sceCdSync`, `sceCdGetError` (CD errors set status bit 2).
3. EE read (state 2): `sceSifSendCmd(0, header, 16, buffer, EE dest, bytes)` DMAs the chunk to EE RAM; the EE's
   command 0 handler does nothing (0x397948, `ei` only). SPU read (state 3): `SpuSetCore`, `SpuSetTransferMode(0)`,
   `SpuSetTransferCallback(IOP 0x2FF0)`, `SpuSetTransferStartAddr(dest)`, `SpuWrite(buffer, bytes)`, next chunk
   when the callback fires.
4. Advance LSN, destination, remaining size; state back to idle (bit 0 clear) when done.

So `File_Request(..., toIop = 1)` means "to SPU2 RAM", not IOP RAM [V]: `dest` is an SPU2 byte address.

### 5.2 BGM (core 0, voices 22 and 23)

- `File_RegisterIopFiles` sends 28 tracks from the 10-byte records at `D_0050CD00` (file id at +6) and the file
  table [V]. Driver entry (20 bytes): type (record byte 0: 1 = disc stream on 26 tracks, 2 on one, 3 on one),
  a signed byte, two halfwords (record +2: 0x2000-0x3CCC on all tracks, possibly a per-track volume or pitch [I]),
  sector, size word, loop sector offset (negative = 0) [V layout].
- Track 7 starts 0xCF7 sectors into its file (the EE passes sector + 0xCF7, size - 0x67B800) [V].
- Start (IOP 0x16A4, when the requested track is >= 0 and the engine is idle): LSN and loop LSN from the entry,
  state 2 then 3 (playing, status bit 4) [V]. Type 3 tracks are not read from disc: the IOP sends SIF command 1 to
  the EE (`{sector offset, IOP dest, 64 sectors}` into `D_008BD080`), and `File_SendSifCmd` answers with
  `sceSifSendCmd(0, ..., D_005F61D4 + offset * 0x800, dest, 64 * 0x800)` from EE RAM [V]. `D_005F61D4` is set by
  `Iop_SetEeStreamBase` (from `func_003AD240`, which also starts the BGM) and cleared by `Bgm_Stop` [V]; the game
  exits if a request arrives while it is 0 [V].
- Playback: 2048-byte chunks go from the 512 KB IOP ring to SPU2 RAM; the engine rewrites voice 22/23 addresses
  every chunk (left and right 0x400 bytes apart) and loops at the loop sector [I: IOP 0xFD0].
- Units: volume 0-128; fades in 4-tick steps [I].

### 5.3 Movies (3 files: ids 14-16)

`Movie_StartStream(n)` sends 0x2058 with the file's sector; the driver switches to movie mode (0x11B612 = 3),
which suspends normal file reads [V]. `Movie_ReadChunk(dest)` sends 0x2059 (0x20 sectors), polls status bits 5
and 2, and waits for the EE-side DMA counter at `D_00436568 + 0x18` [V]. The IOP sends each chunk with SIF
command 0 (IOP 0x2B64) [V]. `Movie_PlayAudio(n)` (queue code 0x5C) plays the movie's ADPCM on core 0 voices
22/23 [I]. After the movie: 0x205A(1), then `Snd_WaitStreamIdle` (up to 900 frames for status bit 6, sending
`Bgm_Stop` each frame) [V].

## 6. Sound effects (core 1, voices 0-21)

- Boot (`Snd_Init` from `Mode_Boot`): for core 0 and 1, `SpuSetCommonAttr` (mask 0x3FCF, master 0x3FFF,
  CD input 0x7FFF, rest 0), then `SpuSetVoiceAttr` on all 24 voices (mask 0x9F: volume 0x1FFF, pitch 0x1000,
  start 0x5010) [V]. `Snd_InitBanks` places four 0x1C900-byte character banks after `D_00500CB8` + 0x5010 [V].
- SPU2 RAM: user area from 0x5010 [V: every address adds 0x5010]. Loads with `File_LoadSyncIop`: file 1 at 0x5010, file
  0x2D or 0xBB + 7n at 0x82D90, 0x158 + 3n at a caller-given address; character banks by `File_Request` to
  `Snd_GetBankAddr(slot)` [V].
- `Snd_PlaySe(id, player)` -> `Snd_PlaySeEx`: the bank table `gSndBankDefs` 0x42EBD8 (16 bytes per bank: descriptor array,
  kind at +8, first id at +0xA) gives an 8-byte descriptor `{u32 offset, u16 pitch, u8 volume, u8 category}`;
  address = bank base + offset + 0x40 (skips the VAG header) [V layout, I: header]. Volume = `((vol << 8) - 1) *
  scale >> 8 * SE option (0-10) / 10` [V]. `Snd_AllocVoice(category, player)` picks a voice from 0-21 (per-category jump table `jtbl_005007B0`;
  policy not read in detail);
  `Snd_KeyOn(voice, vol, pitch, addr)` sends `SpuSetCore(1)` + 0x7440 with mask 0x93 (volume L = R, pitch, start
  address) [V].
- Units [I: libspu2]: pitch 0x1000 = 48 kHz (sample rate = 48000 * pitch / 0x1000); voice volume 0-0x3FFF per
  side; pan = different L/R volumes (only `Snd_SetPlayerVolume` sets them apart, scales 0-0x1000).
- Voice slots `gSndVoices` 0x8A7920 (22 x 8 bytes: category, id, player, volume; `SndVoice` in include/sound.h) [V]. `Snd_UpdateVoices` (every frame)
  reads all key states with 0x6418 and frees a slot when its voice reports 0 (off) or 3 (released, envelope still
  sounding) [V; I: state names]. The 22 states are copied to `gSndKeyStatus`.
- `Snd_StopCategory(c)` keys off every voice of category c; `Snd_StopAllSe` keys off voices 0-21 (command 5) [V].
- Delayed effects (`Snd_QueueDelayed`, 24 slots `gSndDelayed`): an effect of `func_00374C28`'s set is queued and
  replayed by `Snd_UpdateDelayed` at the frames and scales of the 3-step table `D_005007D8` (mode 2 of
  `Snd_PlaySeEx`) [V code, I: echo].
- Fighter voices: `Snd_PlayVoice(player, set, code)` picks a voice id from the fighter's voice set (`D_00501808`, 16
  bytes per set: counts of four kinds, then the ids); `func_00376220` runs a voice sequence (`D_00501A78`, 4-byte
  steps {frame, op, arg}) started by `Snd_StartVoiceSeq` from the move data (kind 8 entries) [V layout, I: names].
- Every libspu2 command is preceded by a call to the empty stub `Snd_Nop` 0x399FC8 [V].

## 7. Callbacks and polling

No EE callback is registered with the driver: the game polls [V]. The EE handles two SIF commands: 0 (data
arrived; empty handler) and 1 (`File_SifCmdHandler`: the IOP asks for EE-memory BGM data, answered on the next
`File_Update`) [V].

## 8. What the port implements

- `sceSdRemote` as a direct call into C. Commands 1, 2, 5, 0x7128, 0x7240, 0x7440, 0x6418: a software SPU2 (24
  voices per core, 2 MB RAM, ADPCM decoder, ADSR) or a mixer with the same voice model; 0x6418 must report key
  states 0/1/3 as the hardware does so voice slots free on the same frame.
- 0x204E/0x2045/0x2050/0x2030: reads from `TEKKEN.BIN` into EE memory or the SPU RAM image, completed on a later
  frame to keep the original timing (status bit 0).
- 0x205E/0x7F0F codes/0x2000: a BGM streamer for the 28 tracks (ADPCM from disc or from EE RAM for type 3) with
  loop sector, volume and fades.
- 0x2058/0x2059/0x205A and code 0x5C: the movie stream feeding the EE's MPEG decoder plus its ADPCM audio.
- Ignore the 20 unused commands; 0x2051 is a no-op.

## Open questions

- Exact meaning of BGM record bytes 1-5 and of the 0x80 track flag (0x11B65C, written only).
- ADPCM interleave of the BGM and movie streams, and the role of the 25 raw ADPCM files [check against
  `assets/files`].
- Sample rate of the stream-thread timer (238 Hz if source 4 is H-line) [PCSX2].
