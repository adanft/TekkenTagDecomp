/*
 * The game's menus and fights for the screen reader (T48, docs/port/accessibility.md): opt-in (ui.announce_game with
 * ui.screen_reader), read once a VBlank from the game's own memory, never written.
 *
 * plat_hw.c's VBlank calls Port_A11yVBlank off the EE stack, before the game's VBlank handler, on the game's thread:
 * the values read are the ones of a whole frame, and a run makes the same announcements every time. The reads change
 * nothing the game or the state hash sees (a VBlank with this on hashes as one with it off: tests/test_port_a11y.py).
 * The names said (menu items, characters) are the game's own strings, read from its memory at run time; nothing of
 * them is in this program.
 *
 * What is read (addresses: the game's fixed addresses; the evidence in the decompiled code):
 * - gMode, gSubMode (config/symbols/data.txt): the screen (docs/game_overview.md; 2 title, 3 main menu, 5 character
 *   select, 4 a fight...).
 * - the main menu: the cursor, s8 at 0x005F650E (src/modes_b.c:150, moved at :1318), an index into the menu's
 *   visible items: the 12 entries of the table at 0x0051EFE0 ({const char *name; u8 id, locked...}, src/modes_b.c:140)
 *   without the locked ones, but TEKKEN BOWL (id 8), THEATER (9) and GALLERY (10) once unlocked (func_003BD130,
 *   src/modes_b.c:1173; the unlock flags at 0x00439C82, 0x00439C70, 0x00439C83 of func_003BFEB0..., read as there).
 * - the character select: each side's record at 0x00440D08 + side * 0x1C (SelSide, src/select_results_b.c:93): the
 *   highlighted character at +0x10 (0x37 random, 0x36 none), its costume at +0x14; the character's name through the
 *   table at 0x00415840 (SelChar *[character * 8 + costume], {const char *name; ...}, src/select_results.c:125).
 * - a fight: the round's state, s32 at 0x005F5E88 (0 intro, 1 fighting, 2 the round ends, 3 its result:
 *   Battle_UpdateRound, docs/systems/fighter.md), the round's number gRoundNumber at 0x005F5F04 (counted at the
 *   round's start, src/battle_flow.c:3336), how it ended at 0x005F5CAC (1 time, 2 K.O., 8 perfect, 0x20 draw:
 *   Battle_SetRoundResult, src/battle_flow.c:4313); the two fighters on screen, gFighters at 0x00879640 (0xB8A0 each):
 *   the team member in play at +0x0, the round won (+0xB4 = 1), life and maximum life at +0x3F4 / +0x3FC
 *   (include/fighter.h:380); the team members' characters at 0x00440940 ([side][member] x 0x30, include/fighter.h:487).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "port_addr.h"
#include "plat_lang.h"
#include "plat_settings.h"
#include "plat_speech.h"

extern int16_t gMode, gSubMode; /* src/main.c */

#define A_MENU_CURSOR 0x005F650Eu
#define A_MENU_TABLE 0x0051EFE0u
#define A_UNLOCK_BOWL 0x00439C82u
#define A_UNLOCK_THEATER 0x00439C70u
#define A_UNLOCK_GALLERY 0x00439C83u
#define A_SEL_SIDE 0x00440D08u
#define A_CHAR_TABLE 0x00415840u
#define A_ROUND_STATE 0x005F5E88u
#define A_ROUND_NUMBER 0x005F5F04u
#define A_ROUND_RESULT 0x005F5CACu
#define A_FIGHTERS 0x00879640u
#define A_FIGHTER_SIZE 0xB8A0u
#define A_TEAM 0x00440940u

static volatile int sOn;
static unsigned sSaid;

/* A value that is said once it held for three VBlanks and differs from the one said last (no flicker said). */
typedef struct {
    int seen, held, said;
} Track;

static int settle(Track *t, int v) {
    if (v != t->seen) {
        t->seen = v;
        t->held = 0;
        return 0;
    }
    if (++t->held == 2 && v != t->said) {
        t->said = v;
        return 1;
    }
    return 0;
}

static void reset(Track *t) {
    t->seen = t->said = -1000;
    t->held = 0;
}

static Track sMode, sMenu, sSel[2], sRound;
static int sBucket[2], sMember[2];

void A11y_Init(void) {
    reset(&sMode);
    reset(&sMenu);
    reset(&sSel[0]);
    reset(&sSel[1]);
    reset(&sRound);
    A11y_SetGame(Settings_GetInt("ui.screen_reader") && Settings_GetInt("ui.announce_game"));
}

void A11y_SetGame(int on) { sOn = on != 0; }

static int ram(uint32_t a, uint32_t n) { return a >= 0x00010000u && a + n <= 0x02000000u && a + n > a; }
static uint8_t rd8(uint32_t a) { return ram(a, 1) ? *(volatile uint8_t *)Port_Ptr(a) : 0; }
static int16_t rd16(uint32_t a) { return ram(a, 2) ? *(volatile int16_t *)Port_Ptr(a) : 0; }
static uint32_t rd32(uint32_t a) { return ram(a, 4) ? *(volatile uint32_t *)Port_Ptr(a) : 0; }

/* A string of the game's at `a` (printable ASCII; anything else ends it): 1 when there is one. */
static int game_text(uint32_t a, char *out, int n) {
    int k = 0;
    while (k < n - 1 && ram(a + (uint32_t)k, 1)) {
        uint8_t c = rd8(a + (uint32_t)k);
        if (c < 0x20 || c > 0x7E) {
            break;
        }
        out[k++] = (char)c;
    }
    out[k] = 0;
    while (k > 0 && out[k - 1] == ' ') {
        out[--k] = 0;
    }
    return k > 0;
}

static void say(const char *text, int interrupt) {
    sSaid++;
    Speech_Say(text, interrupt);
}

/* ---- the screens ---- */
static void screen_name(int mode) {
    const char *t;
    switch (mode) {
    case 2: t = Tr("a11y.mode.title", "Title screen"); break;
    case 3: t = Tr("a11y.mode.main_menu", "Main menu"); break;
    case 5: t = Tr("a11y.mode.select", "Character select"); break;
    case 6: case 7: t = Tr("a11y.mode.name_entry", "Name entry"); break;
    case 8: t = Tr("a11y.mode.ending", "Ending"); break;
    case 9: t = Tr("a11y.mode.options", "Options"); break;
    case 10: t = Tr("a11y.mode.results", "Results"); break;
    case 12: case 13: t = Tr("a11y.mode.theater", "Theater"); break;
    case 14: case 15: t = Tr("a11y.mode.music", "Music player"); break;
    case 16: t = Tr("a11y.mode.gallery", "Gallery"); break;
    default: return; /* boot, loading, a fight (its rounds are said) */
    }
    say(t, 1);
}

/* The main menu's item `cursor` (the visible ones, as func_003BD130 lists them): its name. */
static int menu_item(int cursor, char *out, int n) {
    int i, k = 0;
    for (i = 0; i < 12; i++) {
        uint32_t e = A_MENU_TABLE + (uint32_t)i * 8;
        uint8_t id = rd8(e + 4), locked = rd8(e + 5);
        int ok = !locked || (id == 8 && rd8(A_UNLOCK_BOWL) != 0) ||
                 (id == 9 && (rd32(A_UNLOCK_THEATER) | rd32(A_UNLOCK_THEATER + 4)) != 0) ||
                 (id == 10 && rd8(A_UNLOCK_GALLERY) != 0);
        if (ok && k++ == cursor) {
            return game_text(rd32(e), out, n);
        }
    }
    return 0;
}

/* A character's name (character * 8 + costume into the select's table). */
static int char_name(int chr, int costume, char *out, int n) {
    uint32_t idx, rec;
    if (chr < 0 || costume < 0 || costume > 7) {
        return 0;
    }
    idx = (uint32_t)chr * 8 + (uint32_t)costume;
    if (idx >= 0x118) {
        return 0;
    }
    rec = rd32(A_CHAR_TABLE + idx * 4);
    return rec != 0 && game_text(rd32(rec), out, n);
}

static void player_text(int side, const char *name, int interrupt) {
    char p[8], t[160];
    snprintf(p, sizeof p, "%d", side + 1);
    Lang_Format(t, sizeof t, "a11y.player_is", "Player {0}: {1}", p, name, NULL);
    say(t, interrupt);
}

static void select_vblank(void) {
    int side;
    for (side = 0; side < 2; side++) {
        uint32_t s = A_SEL_SIDE + (uint32_t)side * 0x1C;
        int chr = (int)rd32(s + 0x10), costume = (int)rd32(s + 0x14);
        char name[48];
        if (!settle(&sSel[side], chr) || chr == 0x36) {
            continue;
        }
        if (chr == 0x37) {
            player_text(side, Tr("a11y.random", "Random"), 0);
        } else if (char_name(chr, costume, name, sizeof name)) {
            player_text(side, name, 0);
        }
    }
}

static uint32_t fighter(int side) { return A_FIGHTERS + (uint32_t)side * A_FIGHTER_SIZE; }

static int life_percent(int side) {
    int32_t life = (int32_t)rd32(fighter(side) + 0x3F4), max = (int32_t)rd32(fighter(side) + 0x3FC);
    if (max <= 0) {
        return -1;
    }
    life = life < 0 ? 0 : life > max ? max : life;
    return (int)((int64_t)life * 100 / max);
}

static void fight_vblank(void) {
    int state = (int)rd32(A_ROUND_STATE), side;
    char t[160], a[16], b[16];
    if (settle(&sRound, state)) {
        uint32_t r = rd32(A_ROUND_RESULT);
        if (state == 1) {
            snprintf(a, sizeof a, "%d", (int)rd32(A_ROUND_NUMBER));
            Lang_Format(t, sizeof t, "a11y.round_fight", "Round {0}. Fight!", a, NULL);
            say(t, 1);
            for (side = 0; side < 2; side++) {
                sBucket[side] = 4;
                sMember[side] = rd16(fighter(side));
            }
        } else if (state == 2) {
            say(r & 0x20 ? Tr("a11y.draw", "Draw") : r & 2 ? Tr("a11y.ko", "K.O.") :
                r & 1 ? Tr("a11y.time_up", "Time up") : Tr("a11y.round_over", "The round is over"), 1);
        } else if (state == 3) {
            for (side = 0; side < 2; side++) {
                if (rd16(fighter(side) + 0xB4) == 1) {
                    snprintf(a, sizeof a, "%d", side + 1);
                    if (r & 8) {
                        Lang_Format(t, sizeof t, "a11y.wins_perfect", "Player {0} wins the round. Perfect!", a, NULL);
                    } else {
                        Lang_Format(t, sizeof t, "a11y.wins", "Player {0} wins the round", a, NULL);
                    }
                    say(t, 0);
                }
            }
        }
    }
    if (state != 1 || sRound.said != 1) {
        return;
    }
    for (side = 0; side < 2; side++) {
        int pct = life_percent(side), m = rd16(fighter(side));
        if (pct >= 0 && pct / 25 < sBucket[side]) { /* each quarter of the life bar lost: its percentage */
            sBucket[side] = pct / 25;
            snprintf(a, sizeof a, "%d", side + 1);
            snprintf(b, sizeof b, "%d", pct);
            Lang_Format(t, sizeof t, "a11y.life", "Player {0}: {1} percent", a, b, NULL);
            say(t, 0);
        }
        if ((m == 0 || m == 1) && m != sMember[side]) { /* a tag: the partner comes in */
            uint32_t tm = A_TEAM + (uint32_t)(side * 2 + m) * 0x30;
            char name[48];
            sMember[side] = m;
            sBucket[side] = 4;
            if (char_name((int)rd32(tm), (int)rd32(tm + 4), name, sizeof name)) {
                snprintf(a, sizeof a, "%d", side + 1);
                Lang_Format(t, sizeof t, "a11y.tag", "Player {0} tags in {1}", a, name, NULL);
                say(t, 0);
            }
        }
    }
}

void Port_A11yVBlank(void) {
    int mode;
    char name[48];
    if (!sOn) {
        return;
    }
    mode = gMode;
    if (settle(&sMode, mode)) {
        screen_name(mode);
        reset(&sMenu);
        reset(&sSel[0]);
        reset(&sSel[1]);
        reset(&sRound);
    }
    if (sMode.said != mode) {
        return; /* a screen being changed */
    }
    if (mode == 3) {
        int c = (int8_t)rd8(A_MENU_CURSOR);
        if (settle(&sMenu, c) && menu_item(c, name, sizeof name)) {
            say(name, 0);
        }
    } else if (mode == 5) {
        select_vblank();
    } else if (mode == 4) {
        fight_vblank();
    }
}

void Port_A11yReport(void) {
    if (sOn) {
        printf("ttt: screen reader: game announcements %u, output %s\n", sSaid, Speech_Output());
    }
}
