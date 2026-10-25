# PC port: cheats

T40. The settings overlay's **Cheats** page (F1) has one cheat, **Unlock everything** (`cheats.unlock_all`,
`TTT_CHEAT_UNLOCK=1`). It is off by default. The page warns that it modifies the game's state. Code:
`port/src/plat_cheat.c`.

## What it does

It sets the flags the game itself sets after every unlock. Nothing else is patched.

The game keeps its options and unlocks in one 0x68-byte block, `D_00439C60`. The memory card save holds it
([docs/systems/modes.md](../systems/modes.md) section 4). The unlock fields:

| Offset | Type | Meaning | Who sets it in the game | The cheat |
|---|---|---|---|---|
| `+0x10` | u64 | Clear bits, one per character id: the THEATER entries (`func_003BFDD8`, `src/theater.c:239`). Any bit set shows THEATER in the main menu (`func_003BFED8`). | `Unlock_SetClearBit` from the ending (`src/loading_title.c:1182`, `src/select_results.c:958`) | ORs in the bit of every id in the theater character table `D_0051B678` (0x118 signed bytes, -1 for none, `config/symbols/data.txt:645`) |
| `+0x18` | u32 | The unlock step counter | `Unlock_Step` (`src/select_results.c:936`): a new clear bit, or 50 matches counted in `+0x20` (`func_003BFDF8`) | Raised to the last threshold of the unlock table |
| `+0x22` | u8 | TEKKEN BOWL in the main menu when not 0 (`func_003BFEB0`); then a bowl play counter, capped at 3 (`func_003BFCF8`, `src/battle_mode.c:491`) | Unlock kind 5 (`func_003BFB80`: 0 becomes 1) | 0 becomes 1 |
| `+0x23` | u8 | GALLERY in the main menu (`func_003BFEE8`) | Unlock kind 4 (`func_003BFBA0`) | 1 |
| `+0x37` | u8 | THEATER's extra option (`func_003BFDC8`, `src/theater.c:229`) | Unlock kind 6 (`func_003BFD98`) | 1 |

The unlock table `D_00440818` is an array of `{u16 threshold, u16 kind, u32 arg}` entries in the executable, ended
by threshold 0xFFFF. `Unlock_ApplyAll` (`src/select_results.c:900`) first clears `+0x23` and `+0x37` and resets the
character sets. It then applies every entry whose threshold the counter has reached:

- kinds 0-3 add the `arg` character to the selectable sets `D_0043B968`, `D_0043B990`, `D_0043B9B8` and `D_0043B9E0`
  (`func_0035CA00`, `func_0035C998`, `func_0035CA68`, `func_0035CAE0`);
- kinds 4-6 set the bytes in the table above;
- kind 7 does nothing.

The sets are rebuilt from the counter and never saved. The game calls `Unlock_ApplyAll` at boot (`src/main.c:287`),
at every game start (`Game_Start`, `src/modes.c:849`) and on the main menu's entries (`src/modes_b.c:1446-1459`).

The port reads both tables from the game's memory at run time. No game data is copied into the port. The cheat
never writes the character sets: the game's own `Unlock_ApplyAll` rebuilds them from the raised counter.

## When

At every VBlank while the cheat is on, so a save loaded from the card later is raised again. The write runs in
`plat_hw.c`'s VBlank work on the game's thread, off the EE stack, between game instructions at a hardware poll.
Values are only raised (OR / max), never lowered. The game's read-modify-writes of these fields stay correct:
`Unlock_Step`'s `++` only goes above the target, and `Unlock_SetClearBit` finds the bit already set.

- **Turning it off does not lock anything again.** The flags stay until the game's state resets them, for example a
  boot without the save.
- **The save keeps the unlocks.** The cheat does not set the save's dirty flag (`func_00361968`), but the next save
  the game writes, for whatever reason, contains the raised block.
- **New-entry blinking.** The main menu sees TEKKEN BOWL / THEATER / GALLERY as new (bits of `+0x24`) and moves
  its cursor there, as after a real unlock. A recorded input replay made without the cheat goes elsewhere with it.

## State hash

A run with the cheat has a different hash, on purpose. The headless idle run (300 VBlanks, `TTT_SEED=1`, empty
card) reports:

```
ttt: unlocks: counter 0, ..., 20 selectable characters; cheat off (raised at 0 VBlanks)   (default)
ttt: unlocks: counter 14, clear bits 0x0000003fffffffff, gallery 1, bowl 1, theater 1, 34 selectable characters; cheat on (raised at 1 VBlanks)
```

The 2551-VBlank idle run gives `0xba03f15fdc086048` with the cheat instead of `0xd36c82e9a145875a`. When it is off,
nothing is written, and every hash is unchanged. Every `TTT_FRAMES` report has the `ttt: unlocks:` line. Its
selectable count is the number of non-zero bytes of `D_0043B968`'s 0x22 bytes.

## Tests

`tests/test_port_state.py` `CheatUnlock` runs two headless 300-VBlank runs, one without the cheat and one with it.
With the cheat on:

- the counter and clear bits are set;
- the three menu bytes are 1;
- more characters are selectable;
- the hash differs.

`tests/test_port_input.py` `Theater` uses the cheat to reach THEATER (up and Start on the main menu, whose cursor
starts on GALLERY) and its music player.

## Open

- **THEATER (fixed in T41 b0).** Entering it crashed in `Mode_Theater`: the draft called `Text_Printf(D_00522AC0, x,
  y, 0xA)` through a 4-argument local prototype, and the format's `%s` read a stack word as a string. `Mode_Theater`
  and `Mode_MusicPlayer` are now written from the asm; `tests/test_port_input.py` `Theater` enters THEATER with
  the cheat, opens the music player, plays a track and comes back (with `TTT_SLOW_TESTS=1` also an ending, and
  every call compared with the original by `TTT_DIFF`). The defect class is audited by `port/tools/proto_audit.py`
  ([README](README.md), gotcha 29).
- Only "unlock everything". There are no per-item toggles and no other cheats (infinite health, timers): those
  would need fight-state addresses checked the same way.
- Not checked: the costume / tag partner rules beyond the four selectable sets, and TEKKEN BOWL levels past the
  menu entry.
