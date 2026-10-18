# .text map: game code and library code

`.text` is 0x340000-0x41567C (the last function's padding runs to 0x415700, where `.data` starts): 3574
functions, 0xD3ADC bytes of function code. Read from the split assembly and the build's symbol table; nothing here
was confirmed by running the game.

Library code is not decompiled. It stays as split assembly and will be matched or linked as library objects (the
same Sony SDK, newlib and libgcc builds), so `scripts/progress.py` counts it apart from the game, using
`config/library_ranges.txt`. Function names are in `config/symbols/<library>.txt` (`// guess` marks inferred ones).

**Verified** means read directly from the binary: a version tag, a string the function prints or passes, a
syscall number, a hardware register, or the shape of handwritten code. **Inferred** means taken from link order,
the call graph or the library's known API.

## Summary

| Class | Functions | Bytes | Named |
|---|---:|---:|---:|
| Game (Namco) | 2815 | 0xB0FB0 | 61 |
| Sony SDK (libvu0, crt0, libcdvd, libdma, libgraph, libmpeg, libipu, libkernl, libpkt, librspu2, libpad, libmtap, libmc) | 612 | 0x14E68 | 357 |
| newlib libm | 31 | 0x41E0 | 31 |
| libgcc | 41 | 0x3434 | 41 |
| newlib libc | 75 | 0x66B0 | 75 |
| **Total** | **3574** | **0xD3ADC** | **565** |

The game is three blocks, 0x340000-0x3F3320 minus libvu0 and crt0, which the linker placed between game objects.
Everything from 0x3F3320 to the end of `.text` is library code.

## Range table

End addresses are excluded. "Named" counts the functions with a name in `config/symbols/`.

| Start | End | Funcs | Named | What | Evidence | Status |
|---|---|---:|---:|---|---|---|
| 0x340000 | 0x34A7A8 | 137 | 0 | Game | Uses `$gp` (first at 0x340BA0); calls into the game blocks below. | Verified |
| 0x34A7A8 | 0x34B4E8 | 40 | 40 | Sony libvu0 | Each function is the VU0 macro-code pattern of one `sceVu0*` call, in the library's order: ApplyMatrix (`vmulax/vmadday/vmaddaz/vmaddw`), OuterProduct (`vopmula/vopmsub`), DivVector (`vdiv Q`), FTOI4/FTOI0/ITOF4/ITOF0 (`vftoi4` ...), UnitMatrix (`vmr32` chain), ClampVector (`vmaxx/vminix`), ClipScreen (clip flags `$vi16`) ... sceVpu0Reset (VIF0 registers 0x10003810-30, FBRST). No `$gp`. Two entries (Normalize at 0x34A868, the `ecossin` helper at 0x34AAF0) hold VU0 opcodes the disassembler cannot decode and stay data labels. | Verified (names: Inferred for the matrix builders, marked guess) |
| 0x34B4E8 | 0x3572A0 | 132 | 0 | Game | `$gp` users; 0x34B4E8 itself uses `$gp` and calls libvu0. | Verified |
| 0x3572A0 | 0x357358 | 3 | 3 | Sony crt0 (`_start`, `_exit`, `_root`) | ELF entry point 0x3572A0: clears `.sbss`/`.bss`, inline syscalls 0x3C/0x3D (SetupThread, SetupHeap) with gp 0x5FD0F0, FlushCache, `main(argc, argv)` (0x3577E8), then jumps to Exit; `_exit` jumps to Exit, `_root` is syscall 0x23 (ExitThread). | Verified |
| 0x357358 | 0x3F3320 | 2546 | 1 | Game | `$gp` users up to 0x3F3118; game strings ("THE KING OF IRONBOWL" at 0x3F3118, `cdrom:\\IRX\\IOPRP165.IMG;1` at 0x357358). | Verified |
| 0x3F3320 | 0x3F5D58 | 44 | 14 | Sony libcdvd (tag `PsIIlibcdvd 1650`) | Debug strings naming the function: "Libcdvd bind err CdSearchFile", "call CdReadChain", "call cdread cmd", "N cmd wait" / "S cmd wait" (sync), "CD_Init", "DiskReady", "status called", "Clock read", "sceCdStRead/StPause/StResume/StStat call". First function follows the last game function 0x3F3118. | Verified |
| 0x3F5D58 | 0x3F6878 | 21 | 17 | Sony libdma (`PsIIlibdma  1620`) | "libdma: sync timeout" in the send/receive/sync functions; "DSendM/DSyncN/DLastSyncTime/DCallback: not installed yet"; sceDmaReset writes D_CTRL (0x1000E000), sceDmaPutStallAddr D_STADR (0x1000E060). | Verified (Send/Recv variants: Inferred) |
| 0x3F6878 | 0x3F8088 | 15 | 12 | Sony libgraph (`PsIIlibgraph1620`) | "sceGsDefDispEnv:", "sceGsSyncPath:", "sceGsExecStoreImage:", "sceGsPutDrawEnv:" strings; GS privileged registers (0x12000000 PMODE ..., 0x12001000 CSR); sceGsResetGraph calls SetGsCrt, GsPutIMR, RemoveIntcHandler. | Verified |
| 0x3F8088 | 0x3FF928 | 137 | 12 | Sony libmpeg (`PsIIlibmpeg 1620`) | "[MPEG ERROR]%s", "The size of work area is too small" (sceMpegCreate), "_sliceA0(): error happens", "_sequenceScalableExtension() is not implemented", MPEG-2 header/macroblock errors, "pack_header_field_flag needs to be '0' in PS" (PSS demux); IPU registers 0x10002000-30. Start: 0x3F8088 is the stream-id table lookup of the PSS callbacks (table 0x4F4B10). | Verified (start ±1 function) |
| 0x3FF928 | 0x3FFDC0 | 4 | 4 | Sony libipu (`PsIIlibipu  1620`) | The four functions that save, restore, wait for and reset the IPU and its DMA channels (0x10002010 IPU_CTRL, 0x1000B000 / 0x1000B400); called from libmpeg wrappers; matches the tag order (mpeg, ipu, kernl). | Inferred (boundary) |
| 0x3FFDC0 | 0x404508 | 256 | 196 | Sony libkernl (`PsIIlibkernl1620`) | 137 syscall stubs 0x3FFDC0-0x400640 (`addiu $v1, $zero, N; syscall`, N = 0x0 ... 0x7F); one-stub C wrappers (EnableIntc ...); newlib system call glue (TTY for fds 1/2, sbrk on EndOfHeap); "TTY: ..." DECI2 handler; Deci2Call wrappers with request numbers 1-9 and 0x10; SIF command/RPC (sceSifInitCmd: AddDmacHandler + sceSifSetDChain + SIF registers; sceSifBindRpc/CallRpc used by every IOP library); sceSifRebootIop ("rom0:UDNL "), sceSifLoadModule (the game loads its six `cdrom:\IRX\*.IRX` modules with it). | Verified |
| 0x404508 | 0x404AD8 | 21 | 21 | Sony libpkt (`PsIIlibpkt  1620`) | Packet builders: DMA tags (cnt 0x10000000, ref 0x30000000, end 0x70000000), VIF DIRECT/DIRECTHL codes (0x50/0x51), GIF tag open/close, qword alignment; about 150 call sites in the game. Sits right after libkernl, as in the tag order. | Verified (block); names Inferred |
| 0x404AD8 | 0x405008 | 6 | 2 | Sony librspu2 (`PsIIlibrspu21620`) | Binds RPC 0x80000601 (the RSPU2DRV module), "rSpuSetMultiVoiceAttr" in the RPC call function called from 31 places in the game's sound code; callback thread serving RPC 0x80000603. Only the init and the call function are named (`sceSdRemoteInit`, `sceSdRemote`, guesses from the API shape). | Verified (block) |
| 0x405008 | 0x406098 | 30 | 19 | Sony libpad (`PsIIlibpad  1630`) | "libpad: sceSifSetDma faild", "libpad: Module version mismatch" (scePadInit), "libpad: buffer addr is not 64 byte align" (scePadPortOpen); called from the game's pad code. | Verified (most names: Inferred) |
| 0x406098 | 0x406560 | 7 | 6 | Sony libmtap (`PsIIlibmtap 1620`) | "libmtap: bind failed", "sceMtapPortOpen/PortClose/GetConnection/GetSlotNumber/ChangeSlot: rpc error"; 0x406098 is its empty debug-print stub. | Verified |
| 0x406560 | 0x407640 | 22 | 5 | Sony libmc (`PsIIlibmc   1650`) | "bind error libmc", "libmc: too old release of mcserv.irx / mcman.irx" (sceMcInit); the rest are RPC calls made from the game's memory-card code. | Verified (block); names Inferred |
| 0x407640 | 0x40B860 | 31 | 31 | newlib libm (float fdlibm) | Wrappers pass their name to matherr ("sqrt", "acosf", "asinf", "atan2f", "fmodf", "logf", "powf"); fdlibm call graph (wrapper -> `__ieee754_*`, rem_pio2f -> `__kernel_rem_pio2f` -> scalbnf/floorf; cosf/sinf -> kernels); files in alphabetical order (sf_cos, sf_sin, w_sqrt, wf_* ; e_sqrt, ef_* , kf_* , s_* , sf_*). | Verified |
| 0x40B860 | 0x40ECE0 | 41 | 41 | libgcc | libgcc2: `__do_global_dtors/ctors`, `__main` (called by `main`), `exit` (dtors, `_cleanup`, `_exit`), 64-bit `__divdi3/__moddi3/__udivdi3/__umoddi3/__muldi3`, `__fixunsdfdi`, `__floatdidf`; then fp-bit double and float routines (pack/unpack, add/sub/mul/div, compare, conversions). Doubles are passed in single 64-bit registers. | Verified (compare function name: Inferred) |
| 0x40ECE0 | 0x415410 | 75 | 75 | newlib libc | `_impure_ptr` (0x4F5A8C) users: stdio (findfp, fwalk, stdio, vfprintf, fflush, fvwrite, makebuf, wsetup), malloc (mallocr, freer), printf/sprintf, rand (0x41C64E6D, 0x3039), string functions, vfprintf ("Inf"), `_dtoa_r` and mprec (`_Balloc` ... `_mprec_log10`), reentrant system calls. Three link passes, each in alphabetical file order (errno ... wsetup; closer ... readr; callocr, fstatr). | Verified (a few names marked guess) |
| 0x415410 | 0x415700 | 6 | 6 | Sony libkernl, late part | Data-cache loops over 64-byte lines with `cache 0x14` (index writeback-invalidate) and `cache 0x16` (index invalidate), with and without `di`; SyncDCache is called by libpad. Linked last, after libc. | Verified (code); names Inferred |

## How the split was made

Verified:

- **Version tags.** `.data` holds `PsIIlibcdvd 1650`, `libdma 1620`, `libgraph1620`, `libmpeg 1620`, `libipu 1620`,
  `libkernl1620`, `libpkt 1620`, `librspu21620`, `libpad 1630`, `libmtap 1620`, `libmc 1650`, in this order. The
  library blocks in `.text` follow the same order from 0x3F3320; each block has named functions (strings,
  registers or syscalls) on both sides of its boundaries, except the libmpeg/libipu edge.
- **`$gp`.** 1181 split functions use `%gp_rel`, all in the game blocks; none from 0x3F3320 on, none in libvu0 or crt0.
  The game was built with `-G8`; Sony's libraries were not.
- **No VU code in `.text`.** `.vutext` is empty (size 0); the VU microcode is data in `.vudata`
  ([systems/vu1/README.md](systems/vu1/README.md)).
- **Entry point.** The ELF entry 0x3572A0 is crt0, so crt0.o was linked after the game objects that precede it.
- **Syscall stubs.** 137 stubs at 0x3FFDC0-0x400640, each `addiu $v1, $zero, N; syscall; jr $ra; nop`, named
  after EE kernel call N (negative N: interrupt-context variant). N = 0x10 and 0x12 have two stubs; the second is
  `AddIntcHandler2` / `AddDmacHandler2`. The other `syscall`s are inline: crt0 (0x3C, 0x3D, 0x23) and a libkernl thread
  helper (0x404288, three times -0x2F).

Inferred:

- **Names inside a block that are marked `// guess`**: from the library's public API and what the caller passes
  (for example `sceCdMmode(1)` right after `sceCdInit`, `scePadInfoMode` called three times by the pad state
  machine). Expect some to be swapped with a neighbour of the same shape.
- **libvu0 inside the game.** It has no version tag; it is identified by its code only (above).
- **gp-free game code.** About 20 runs of 0x800 to 0x3800 bytes inside the game blocks do not use `$gp` (the
  largest: 0x35D1C8-0x3608B8, the game's DMA/scratchpad helpers and memory-card wrappers with
  "BASLUS-20001TekkenTT" strings). They call or are called by game code, so they count as game; their modules
  are in [game_overview.md](game_overview.md) section 4.
