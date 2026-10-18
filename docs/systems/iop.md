# IOP modules

The EE reboots the IOP with `cdrom:\IRX\IOPRP165.IMG;1`, then loads six modules from `cdrom:\IRX\` in this order
(`Sys_BootIop`, [game_overview.md](../game_overview.md) section 2): SIO2MAN, MTAPMAN, PADMAN, MCMAN, MCSERV,
RSPU2DRV. A PC port replaces all of them: the five Sony modules through the EE libraries that call them, and
RSPU2DRV through `sceSdRemote` ([audio.md](audio.md)).

Read with `.venv/bin/python scripts/irx.py info|strings|dis iso/IRX/NAME.IRX` (module header, import/export
tables, strings, annotated R3000 disassembly). Import names come from the standard export order of each IOP
library [I for ordinals the script does not name]. Tags: **[V]** read in the files, **[I]** inferred.

## IOPRP165.IMG

A ROMDIR image (`RESET`, `ROMDIR`, `EXTINFO`, then four modules) replacing the boot ROM's versions of SIFCMD,
FILEIO, CDVDMAN and CDVDFSV, all tagged `PsII... 1650` [V]. CDVDFSV serves the EE's libcdvd [I: standard role], which
the game uses only for `sceCdInit` and `sceCdMmode` during boot [V: no other libcdvd call].

## Modules

| File | `.iopmod` name, version | Library tag | Exports | RPC served | EE side | Tag |
|---|---|---|---|---|---|---|
| SIO2MAN.IRX | `sio2man` 2.01 | `PsIIsio2man 1600` | `sio2man` 0x102, 59 entries | - | none (used by PADMAN, MTAPMAN, MCMAN) | V |
| MTAPMAN.IRX | `multitap_manager` 2.02 | `PsIImtapman 1600` | `mtapman` 0x102, 8 entries | 0x80000901-0x80000903, 0x800009FE, 0x800009FF | libmtap 1620 (EE 0x406098-0x406560) | V |
| PADMAN.IRX | `padman` 2.06 | `PsIIpadman  1630` | `padman` 0x102, 19 entries | 0x80000100, 0x80000101 | libpad 1630 (EE 0x405008-0x406098) | V |
| MCMAN.IRX | `mcman_tool` 2.0b | `PsIImcman   1650` | `mcman` 0x203, 54 entries | - (ioman driver) | through MCSERV | V |
| MCSERV.IRX | `mcserv` 2.08 | `PsIImcserv  1650` | `mcserv` 0x101, 8 entries | 0x80000400 | libmc 1650 (EE 0x406560-0x407640) | V |
| RSPU2DRV.IRX | `ZsRspu2Driver` 1.01 | statically linked `PsIIlibspu2 1600`, `PsIIlibsnd2 1600` | none | 0x80000601 | librspu2 1620 (EE 0x404AD8-0x405008) | V |

The EE side binds the same numbers: 0x80000100/0x80000101 (libpad), 0x80000400 (libmc), 0x80000601 and
0x80000603 (librspu2; 0x80000603 is an EE-side server nobody starts), 0x80000901-0x80000903 (libmtap) [V:
constants in `asm/`].

### Imports

- SIO2MAN: loadcore, intrman, dmacman (28, 32-35), thbase, thevent [V].
- MTAPMAN: loadcore, stdio, thbase, thevent, sifman, sifcmd, sio2man (48-54) [V].
- PADMAN: sysmem, loadcore, intrman, stdio, sio2man (11, 23, 25, 50, 55, 56, 58), thbase, thevent, sifman,
  sifcmd, vblank (8, 9) [V].
- MCMAN: loadcore, intrman, sio2man (24-26, 55), sysclib, thbase, thsemap, timrman, modload (13), ioman
  (`AddDrv`, `DelDrv`), secrman (4-6), cdvdman (51) [V]. It registers the `mc` device with ioman [I].
- MCSERV: loadcore, intrman, sysclib, sifman, sifcmd (including `sceSifGetOtherData`), thbase, mcman (21
  functions) [V].
- RSPU2DRV: [audio.md](audio.md) section 1.

### Notes

- MTAPMAN and PADMAN print "mtapman doesn't work", "PADMAN: *** VBLANK OVERLAP ***", "padman: invalid port number
  [%d]"; MCMAN carries "Sony PS2 Memory Card Format 1.2.0.0" [V]. Nothing in these four modules is specific to
  the game: they are Sony's SDK 1.6.x-era releases [I: version tags].
- Only RSPU2DRV contains game code (CD reads of `TEKKEN.BIN`, BGM and movie streaming, the sound queue).

## What the port does

| IOP piece | EE library calls to replace | Port side |
|---|---|---|
| SIO2MAN, PADMAN, MTAPMAN | `scePad*`, `sceMtap*` (`Pad_*` in `config/symbol_addrs.txt`) | host gamepad input; report no multitap or emulate it |
| MCMAN, MCSERV | `sceMc*` (`Mc_*`) | save files on the host |
| CDVDMAN/CDVDFSV (IOPRP) | `sceCdInit`, `sceCdMmode` at boot | no-ops |
| RSPU2DRV | `sceSdRemote` (18 commands) | [audio.md](audio.md) section 8 |
| SIF RPC, `sceSifLoadModule`, IOP reboot | `Sys_BootIop` | no-ops returning success |
