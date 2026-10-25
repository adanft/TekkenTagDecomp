/*
 * Cheats (T40, docs/port/cheats.md): "unlock everything", opt-in (cheats.unlock_all / TTT_CHEAT_UNLOCK, off by
 * default). It MODIFIES THE GAME'S STATE: the state hash of a run with it differs from one without, and the memory
 * card save the game writes afterwards keeps the unlocks.
 *
 * The game keeps its unlocks in the options / unlock save block D_00439C60 (0x68 bytes, saved to the memory card;
 * docs/systems/modes.md section 4, src/select_results.c):
 *   +0x10 u64  clear bits, one per character (Unlock_SetClearBit from the ending): the THEATER entries, and the menu
 *              item when any is set;
 *   +0x18 u32  the unlock step counter: Unlock_ApplyAll applies every entry of the unlock table D_00440818
 *              ({u16 threshold, u16 kind, u32 arg}, ended by 0xFFFF) whose threshold it reached: kinds 0-3 add
 *              characters to the selectable sets, 4 sets the GALLERY byte +0x23, 5 the TEKKEN BOWL byte +0x22 (0 -> 1),
 *              6 the THEATER option byte +0x37 (1).
 * This sets what the game itself would after every unlock: the counter to the table's last threshold, every clear bit
 * of the theater's character table D_0051B678 (0x118 bytes, -1 for none), and the three bytes those kinds set. Both
 * tables are read from the game's memory at run time. The selectable character sets are not written: the game rebuilds
 * them from the counter with its own Unlock_ApplyAll (at boot, at every game start, on the main menu's entries).
 *
 * When: at every VBlank while enabled (from plat_hw.c's VBlank work, off the EE stack, on the game's thread), so a save
 * loaded from the card later is raised again. Only ever raised (OR / max): turning the cheat off keeps what was set
 * until the game's own state resets it (a new boot without the save, or with a save written before). The game's dirty
 * flag for the save is not touched; whatever the game saves next carries the unlocks.
 */
#include <stdint.h>
#include <stdio.h>
#include "port_addr.h"
#include "plat_settings.h"

#define BLOCK 0x00439C60u         /* the options / unlock save block */
#define UNLOCK_TABLE 0x00440818u  /* {u16 threshold, u16 kind, u32 arg}, ended by threshold 0xFFFF */
#define THEATER_CHARS 0x0051B678u /* s8 character per theater entry, -1 none */
#define THEATER_CHARS_SIZE 0x118u
#define SET_PLAYABLE 0x0043B968u  /* the selectable set the character select reads (0x22 bytes) */
#define CHARS 0x22

static int sUnlock = -1; /* -1: not read yet; written by the window's thread (atomic) */
static uint32_t sApplied; /* VBlanks at which something had to be raised */

void Port_CheatSet(int unlockAll) {
    __atomic_store_n(&sUnlock, unlockAll != 0, __ATOMIC_RELEASE);
}

static int enabled(void) {
    int on = __atomic_load_n(&sUnlock, __ATOMIC_ACQUIRE);
    if (on < 0) {
        Port_CheatSet(Settings_GetInt("cheats.unlock_all"));
        on = __atomic_load_n(&sUnlock, __ATOMIC_ACQUIRE);
    }
    return on;
}

/* What every unlock gives: the counter (the last threshold) and the bytes kinds 4-6 set; the clear bits. */
static void targets(uint32_t *counter, int *gallery, int *bowl, int *theater, uint64_t *clear) {
    const uint8_t *t = Port_Ptr(UNLOCK_TABLE);
    const int8_t *c = Port_Ptr(THEATER_CHARS);
    uint32_t i;
    *counter = 0;
    *gallery = *bowl = *theater = 0;
    for (i = 0; i < 256; i++, t += 8) {
        uint16_t thr = (uint16_t)(t[0] | t[1] << 8), kind = (uint16_t)(t[2] | t[3] << 8);
        if (thr == 0xFFFF) {
            break;
        }
        *counter = thr;
        *gallery |= kind == 4;
        *bowl |= kind == 5;
        *theater |= kind == 6;
    }
    *clear = 0;
    for (i = 0; i < THEATER_CHARS_SIZE; i++) {
        if (c[i] >= 0 && c[i] < 64) {
            *clear |= (uint64_t)1 << c[i];
        }
    }
}

/* plat_hw.c, each VBlank (headless runs too: the cheat is part of the run's state). */
void Port_CheatVBlank(void) {
    uint8_t *b = Port_Ptr(BLOCK);
    uint32_t counter;
    uint64_t clear;
    int gallery, bowl, theater, raised = 0;
    if (!enabled()) {
        return;
    }
    targets(&counter, &gallery, &bowl, &theater, &clear);
    if (*(uint32_t *)(b + 0x18) < counter) {
        *(uint32_t *)(b + 0x18) = counter;
        raised = 1;
    }
    if ((*(uint64_t *)(b + 0x10) & clear) != clear) {
        *(uint64_t *)(b + 0x10) |= clear;
        raised = 1;
    }
    if (gallery && b[0x23] == 0) {
        b[0x23] = 1;
        raised = 1;
    }
    if (bowl && b[0x22] == 0) {
        b[0x22] = 1;
        raised = 1;
    }
    if (theater && b[0x37] == 0) {
        b[0x37] = 1;
        raised = 1;
    }
    sApplied += (uint32_t)raised;
}

/* plat_hw.c's end-of-run report (TTT_FRAMES): the unlock state as the game holds it, and whether the cheat is on. */
void Port_CheatReport(void) {
    const uint8_t *b = Port_Ptr(BLOCK), *set = Port_Ptr(SET_PLAYABLE);
    int i, n = 0;
    for (i = 0; i < CHARS; i++) {
        n += set[i] != 0;
    }
    printf("ttt: unlocks: counter %u, clear bits 0x%016llx, gallery %u, bowl %u, theater %u, %d selectable characters; "
           "cheat %s (raised at %u VBlanks)\n",
           (unsigned)*(const uint32_t *)(b + 0x18), (unsigned long long)*(const uint64_t *)(b + 0x10), b[0x23], b[0x22],
           b[0x37], n, enabled() ? "on" : "off", (unsigned)sApplied);
}
