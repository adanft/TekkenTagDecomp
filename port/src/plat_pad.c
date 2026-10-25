/*
 * Pads on PC (T34): libpad and libmtap as the game uses them (src/pad.c), fed once per VBlank.
 *
 * Two DualShock 2 pads on ports 0 and 1 (slot 0), or with the multitap (input.multitap / TTT_MULTITAP, read at start)
 * four on port 0's slots 0-3 (1A-1D: the game's players 1-4, src/pad.c's D_00509D00; port 1 is then empty, as the
 * game reads no player there). Each one walks the game's set-up sequence of a real
 * DualShock 2 (Pad_Update): stable (state 6), analog mode (id 7, also as the extended id), pressure mode accepted at
 * once, 2 actuators, and from then on reads of kind 0x79: buttons (active low), the sticks (right x, right y, left x,
 * left y, 0x80 = centre) and 12 pressure bytes (0xFF when the button is down, 0 when up).
 *
 * The pads' state changes only at a VBlank (Port_PadVBlank, from plat_hw.c's VBlank, before the game's handler runs):
 * it is latched there from one source and every scePadRead until the next VBlank returns it. The source is, in order:
 *   TTT_INPUT_PLAY=<file>  a recorded stream (below), the same pad states at the same VBlanks on every run;
 *   the window             keyboard and SDL gamepads (plat_window.c), sampled at the VBlank.
 * TTT_INPUT_RECORD=<file> writes the latched states. With the headless clock the game's VBlanks are the same on every
 * run, so a recording made headless (TTT_FRAMES, or TTT_HEADLESS=1 with TTT_WINDOW=1 to play it) replays the same run.
 *
 * How many pads are connected (TTT_PADS=0..2, 0..4 with the multitap): 2 (4) in real time, 0 headless (the runs and
 * state hashes of the earlier tasks have no pad), the recording's count with TTT_INPUT_PLAY (and its multitap).
 *
 * Vibration: the game drives the two motors with scePadSetActDirect (src/modes.c func_003BA888, after its options' and
 * battle mode's vibration switch) through the alignment of scePadSetActAlign (src/pad.c: byte 0 the small motor, on or
 * off, byte 1 the large one, 0-255); plat_window.c plays them on the pad's SDL gamepad (input.vibration). Output only:
 * nothing of it reaches the game; the second pass of TTT_SYNCTEST and a TTT_INPUT_PLAY replay play nothing.
 *
 * Recording format (text): a header line `ttt-input 1 pads <n>` (`ttt-input 2 pads <n> multitap 1` with the multitap,
 * then four pads per line), then one line per VBlank where the state changes:
 *   <vblank> <buttons0> <analog0> <buttons1> <analog1> [<buttons2> <analog2> <buttons3> <analog3>]
 * in hex, buttons with 1 = pressed in libpad's order (0x8000 left, 0x4000 down, 0x2000 right, 0x1000 up, 0x0800 start,
 * 0x0400 R3, 0x0200 L3, 0x0100 select, 0x80 square, 0x40 cross, 0x20 circle, 0x10 triangle, 8 R1, 4 L1, 2 R2, 1 L2),
 * analog as 8 hex digits: right x, right y, left x, left y. A state holds until the next line; `#` starts a comment.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat_settings.h"
#include "plat_state.h"

void Port_Hit(const char *name);                            /* plat_stub.c */
int Port_Headless(void);                                    /* plat_hw.c */
uint32_t Port_VBlankCount(void);                            /* plat_hw.c */
int Port_WindowOpen(void);                                  /* plat_window.c */
void Port_WindowPads(uint16_t buttons[4], uint8_t analog[4][4]); /* plat_window.c */
void Port_WindowRumble(int pad, int small, int large);           /* plat_window.c */

#define PAD_MAX 2 /* the pads of pad.now, the hashed state (its layout since T34 b1) */
#define PAD_ALL 4 /* with the multitap */

typedef struct {
    uint32_t vblank;
    uint16_t buttons[PAD_MAX];
    uint8_t analog[PAD_MAX][4];
} PadFrame;

/* The rest of the latched state: pads 3 and 4 (the multitap) and each pad's actuator alignment. Saved, not hashed: what
   the game makes of it is in its own memory. */
typedef struct {
    uint16_t buttons[PAD_ALL - PAD_MAX];
    uint8_t analog[PAD_ALL - PAD_MAX][4];
    uint8_t align[PAD_ALL][6];
} PadMore;

/* All four pads' state at one VBlank: a line of a recording. */
typedef struct {
    uint32_t vblank;
    uint16_t buttons[PAD_ALL];
    uint8_t analog[PAD_ALL][4];
} PadInput;

static int sPads = -1; /* connected pads, -1 before the first use */
static int sMultitap;  /* the multitap on port 0 (fixed at start: the setting, or the recording's) */
static PadFrame sNow;  /* the state latched at the last VBlank */
static PadMore sMore;
static PadInput *sPlay;
static int sPlaying; /* TTT_INPUT_PLAY */
static uint32_t sPlayCount, sPlayNext;
static FILE *sRecord;
static PadInput sRecorded;
static int sRecordedAny;
static uint32_t sPressedVBlanks; /* VBlanks with a button down (the report) */

static void centre(PadInput *f) {
    memset(f, 0, sizeof *f);
    memset(f->analog, 0x80, sizeof f->analog);
}

/* The latched state as one PadInput, and back. */
static void now_get(PadInput *f) {
    int p;
    f->vblank = sNow.vblank;
    for (p = 0; p < PAD_ALL; p++) {
        f->buttons[p] = p < PAD_MAX ? sNow.buttons[p] : sMore.buttons[p - PAD_MAX];
        memcpy(f->analog[p], p < PAD_MAX ? sNow.analog[p] : sMore.analog[p - PAD_MAX], 4);
    }
}

static void now_put(const PadInput *f) {
    int p;
    sNow.vblank = f->vblank;
    for (p = 0; p < PAD_ALL; p++) {
        if (p < PAD_MAX) {
            sNow.buttons[p] = f->buttons[p];
            memcpy(sNow.analog[p], f->analog[p], 4);
        } else {
            sMore.buttons[p - PAD_MAX] = f->buttons[p];
            memcpy(sMore.analog[p - PAD_MAX], f->analog[p], 4);
        }
    }
}

/* Parses one line of a recording into `f` (two pads, or four: the ones a line leaves out are idle); 1 when it holds a
   state. */
/* (strtoul, not sscanf or realloc below: host calls already in the program's PLT, see port_host.h) */
static int parse_line(const char *line, PadInput *f) {
    unsigned long v[1 + 2 * PAD_ALL];
    char *end;
    int k, p, i;

    for (k = 0; k < 1 + 2 * PAD_ALL; k++) {
        while (*line == ' ' || *line == '\t') {
            line++;
        }
        v[k] = strtoul(line, &end, k == 0 ? 10 : 16);
        if (end == line) {
            if (k < 1 + 2 * PAD_MAX || (k & 1) == 0) { /* at least two pads, whole pads */
                return 0;
            }
            break;
        }
        line = end;
    }
    centre(f);
    f->vblank = (uint32_t)v[0];
    for (p = 0; p < PAD_ALL && 2 + 2 * p < k; p++) {
        f->buttons[p] = (uint16_t)v[1 + 2 * p];
        for (i = 0; i < 4; i++) {
            f->analog[p][i] = (uint8_t)(v[2 + 2 * p] >> (24 - 8 * i));
        }
    }
    return 1;
}

/* A recording's header: `ttt-input <version> pads <n>`, version 2 adding ` multitap <0|1>`. 1 when it is one. */
static int parse_header(const char *line) {
    static const char kHeader[] = "ttt-input ";
    unsigned long ver, n;
    char *e;
    int mt = 0;

    if (memcmp(line, kHeader, sizeof kHeader - 1) != 0) {
        return 0;
    }
    ver = strtoul(line + sizeof kHeader - 1, &e, 10);
    if (memcmp(e, " pads ", 6) != 0) {
        return 0;
    }
    n = strtoul(e + 6, &e, 10);
    if (ver >= 2 && memcmp(e, " multitap ", 10) == 0) {
        mt = strtoul(e + 10, NULL, 10) != 0;
    }
    sMultitap = mt;
    sPads = (int)(n > (mt ? PAD_ALL : PAD_MAX) ? (mt ? PAD_ALL : PAD_MAX) : n);
    return 1;
}

static void load_play(const char *path) {
    FILE *fp = fopen(path, "r");
    char line[256];
    uint32_t cap = 0;

    if (fp == NULL) {
        printf("ttt: input: cannot read %s\n", path);
        exit(2);
    }
    while (fgets(line, sizeof line, fp) != NULL) {
        PadInput f;
        if (line[0] == '#' || parse_header(line)) {
            continue;
        }
        if (!parse_line(line, &f)) {
            continue;
        }
        if (sPlayCount == cap) {
            PadInput *grown;
            cap = cap ? cap * 2 : 64;
            grown = malloc(cap * sizeof *sPlay);
            if (grown == NULL) {
                printf("ttt: input: %s is too large\n", path);
                exit(2);
            }
            if (sPlay != NULL) {
                memcpy(grown, sPlay, sPlayCount * sizeof *sPlay);
                free(sPlay);
            }
            sPlay = grown;
        }
        sPlay[sPlayCount++] = f;
    }
    fclose(fp);
}

/* The setting, read off the EE stack: what the file holds would otherwise leave its trace in the game's memory. */
static void read_multitap(void *arg) {
    *(int *)arg = Settings_GetInt("input.multitap") != 0;
}

static void setup(void) {
    const char *play = getenv("TTT_INPUT_PLAY"), *rec = getenv("TTT_INPUT_RECORD"), *pads = getenv("TTT_PADS");

    PadInput idle;
    int p;

    Port_OffEe(read_multitap, &sMultitap);
    sPads = Port_Headless() ? 0 : sMultitap ? PAD_ALL : PAD_MAX;
    centre(&idle);
    now_put(&idle);
    for (p = 0; p < PAD_ALL; p++) {
        memset(sMore.align[p], 0xFF, 6); /* libpad's: no actuator until scePadSetActAlign */
    }
    if (play != NULL && *play != 0) {
        sMultitap = 0; /* a version 1 recording: no multitap */
        sPads = PAD_MAX;
        sPlaying = 1;
        load_play(play);
    }
    if (pads != NULL && *pads != 0) {
        int n = atoi(pads), max = sMultitap ? PAD_ALL : PAD_MAX;
        sPads = n < 0 ? 0 : n > max ? max : n;
    }
    if (rec != NULL && *rec != 0) {
        sRecord = fopen(rec, "w");
        if (sRecord == NULL) {
            printf("ttt: input: cannot write %s\n", rec);
            exit(2);
        }
        if (sMultitap) {
            fprintf(sRecord, "ttt-input 2 pads %d multitap 1\n# vblank buttons0 analog0 ... buttons3 analog3 "
                             "(port/src/plat_pad.c)\n", sPads);
        } else {
            fprintf(sRecord, "ttt-input 1 pads %d\n# vblank buttons0 analog0 buttons1 analog1 (port/src/plat_pad.c)\n",
                    sPads);
        }
        fflush(sRecord);
    }
}

static int pads(void) {
    if (sPads < 0) {
        setup();
    }
    return sPads;
}

static void record(const PadInput *now) {
    int p;
    if (sRecordedAny && memcmp(sRecorded.buttons, now->buttons, sizeof now->buttons) == 0 &&
        memcmp(sRecorded.analog, now->analog, sizeof now->analog) == 0) {
        return;
    }
    sRecorded = *now;
    sRecordedAny = 1;
    fprintf(sRecord, "%u", (unsigned)now->vblank);
    for (p = 0; p < (sMultitap ? PAD_ALL : PAD_MAX); p++) {
        const uint8_t *a = now->analog[p];
        fprintf(sRecord, " %04x %02x%02x%02x%02x", now->buttons[p], a[0], a[1], a[2], a[3]);
    }
    fprintf(sRecord, "\n");
    fflush(sRecord);
}

/* At VBlank `n` (plat_hw.c): the pads' state for the game until the next one. */
void Port_PadVBlank(uint32_t n) {
    PadInput now;
    int p, any = 0;

    if (pads() == 0 && sRecord == NULL) {
        return;
    }
    now_get(&now);
    if (sPlaying) {
        while (sPlayNext < sPlayCount && sPlay[sPlayNext].vblank <= n) {
            now = sPlay[sPlayNext++];
        }
    } else if (Port_WindowOpen()) {
        Port_WindowPads(now.buttons, now.analog);
    }
    for (p = 0; p < PAD_ALL; p++) {
        if (p >= sPads) { /* not connected: nothing pressed */
            now.buttons[p] = 0;
            memset(now.analog[p], 0x80, 4);
        }
        any |= now.buttons[p];
    }
    now.vblank = n;
    now_put(&now);
    if (any != 0) {
        sPressedVBlanks++;
    }
    if (sRecord != NULL && !gPortResim) {
        record(&now);
    }
}

/* The TTT_FRAMES report line (only with pads). */
void Port_PadReport(void) {
    if (sPads > 0) {
        printf("ttt: input: %d pads%s, %u vblanks with a button down%s\n", sPads, sMultitap ? " (multitap)" : "",
               (unsigned)sPressedVBlanks, sPlaying ? " (played)" : "");
    }
}

/* libpad's read data of pad state `f` / pad `p`: kind 0x79 (DualShock 2, pressure mode). */
static void encode(const PadInput *f, int p, uint8_t rdata[32]) {
    /* the button bit of each pressure byte: right, left, up, down, triangle, circle, cross, square, L1, R1, L2, R2 */
    static const uint16_t kPressure[12] = { 0x2000, 0x8000, 0x1000, 0x4000, 0x10, 0x20, 0x40, 0x80, 4, 8, 1, 2 };
    uint16_t b = f->buttons[p];
    int i;

    memset(rdata, 0, 32);
    rdata[1] = 0x79;
    rdata[2] = (uint8_t)~(b >> 8);
    rdata[3] = (uint8_t)~b;
    memcpy(rdata + 4, f->analog[p], 4);
    for (i = 0; i < 12; i++) {
        rdata[8 + i] = (b & kPressure[i]) ? 0xFF : 0;
    }
}

/* The pad at libpad's port and slot: ports 0 and 1 without the multitap, port 0's slots 0-3 with it; -1 none. */
static int pad_at(int port, int slot) {
    int p = sMultitap ? (port == 0 ? slot : -1) : (slot == 0 ? port : -1);
    return p >= 0 && p < pads() ? p : -1;
}

static int connected(int port, int slot) {
    return pad_at(port, slot) >= 0;
}

/* ---- libmtap: the multitap on port 0 when input.multitap is on (and there are pads) ---- */
int sceMtapInit(void) { return 1; }
int sceMtapPortOpen(int port) { (void)port; return 1; }
int sceMtapPortClose(int port) { (void)port; return 1; }
int sceMtapGetConnection(int port) { return port == 0 && pads() > 0 && sMultitap ? 1 : 0; }

/* ---- libpad ---- */
int scePadInit(int mode) { (void)mode; return 1; }
int scePadEnd(void) { return 1; }
int scePadPortOpen(int port, int slot, void *addr) { (void)port; (void)slot; (void)addr; return 1; }
int scePadPortClose(int port, int slot) { (void)port; (void)slot; return 1; }

/* 6 stable (pad connected), 0 disconnected. */
int scePadGetState(int port, int slot) {
    if (pads() == 0) {
        Port_Hit("scePadGetState"); /* the report keeps listing "no pad" runs */
    }
    return connected(port, slot) ? 6 : 0;
}

/* The last request (mode, pressure mode, actuator alignment) is complete at once. */
int scePadGetReqState(int port, int slot) { (void)port; (void)slot; return 0; }

int scePadRead(int port, int slot, uint8_t *rdata) {
    PadInput now;
    int p = pad_at(port, slot);
    if (p < 0) {
        return 0;
    }
    now_get(&now);
    encode(&now, p, rdata);
    return 32;
}

/* Current id / extended id: 7 (analog). Number of ids or the id at `offs`: one mode, analog. Actuator data. */
int scePadInfoMode(int port, int slot, int term, int offs) {
    if (!connected(port, slot)) {
        return 0;
    }
    switch (term) {
    case 1: /* InfoModeCurID */
    case 2: /* InfoModeCurExID */
        return 7;
    case 3: /* InfoModeCurExOffs */
        return 0;
    case 4: /* InfoModeIdTable */
        return offs == -1 ? 1 : offs == 0 ? 7 : 0;
    default:
        return 0;
    }
}

/* Two actuators: the small motor (on/off) and the large one. */
int scePadInfoAct(int port, int slot, int actno, int term) {
    (void)term;
    if (!connected(port, slot)) {
        return 0;
    }
    return actno == -1 ? 2 : 0;
}

int scePadInfoPressMode(int port, int slot) { return connected(port, slot) ? 1 : 0; }
int scePadEnterPressMode(int port, int slot) { return connected(port, slot) ? 1 : 0; }
int scePadSetMainMode(int port, int slot, int offs, int lock) { (void)offs; (void)lock; return connected(port, slot) ? 1 : 0; }
/* The actuator of each byte of scePadSetActDirect's data (0 the small motor, 1 the large one, 0xFF none). */
int scePadSetActAlign(int port, int slot, const uint8_t *data) {
    int p = pad_at(port, slot);
    if (p < 0) {
        return 0;
    }
    memcpy(sMore.align[p], data, 6);
    return 1;
}

/* The motors now, through the alignment: the small one on or off, the large one's speed 0-255 (plat_window.c). */
int scePadSetActDirect(int port, int slot, const uint8_t *data) {
    int p = pad_at(port, slot), i, small = 0, large = 0;
    if (p < 0) {
        return 0;
    }
    for (i = 0; i < 6; i++) {
        if (sMore.align[p][i] == 0) {
            small = data[i] != 0;
        } else if (sMore.align[p][i] == 1) {
            large = data[i];
        }
    }
    if (!gPortResim && !sPlaying) { /* a replay's motors are not the gamepads' to play */
        Port_WindowRumble(p, small, large);
    }
    return 1;
}

/* ---- self-test (TTT_SELFTEST=pad) ---- */
static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: pad self-test FAILED: %s\n", what);
        sFails++;
    }
}

/* plat_window.c's input path on made-up SDL events (no SDL, no window, nothing from the desktop, no real gamepad) */
void Port_WindowTestInput(int deadzone, int stickDpad, int vibration);
void Port_WindowTestEvent(uint32_t type, uint32_t which, int code, int value);
int Port_WindowTestRumble(int pad, uint16_t *low, uint16_t *high);
#define EV_AXIS 0x650u
#define EV_DOWN 0x651u
#define EV_UP 0x652u
#define EV_ADDED 0x653u
#define EV_REMOVED 0x654u

/* The window's pads latched at a VBlank, as Port_PadVBlank does, into `r` (pad at `port` / `slot`); 0: none there. */
static int window_read(int port, int slot, uint8_t r[32]) {
    PadInput now;
    now_get(&now);
    Port_WindowPads(now.buttons, now.analog);
    now_put(&now);
    memset(r, 0, 32);
    return scePadRead(port, slot, r);
}

static uint16_t rbuttons(const uint8_t r[32]) {
    return (uint16_t)~((r[2] << 8) | r[3]);
}

/* The multitap, the gamepads into the read data (bindings, sticks, dead zone, stick as d-pad, triggers) and the
   vibration, on made-up events. */
static void self_test_gamepads(void) {
    static const uint8_t kAlign[6] = {0, 1, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t r[32], act[6] = {0};
    uint16_t lo, hi;
    PadInput f;
    int i, calls;

    expect(parse_line("5 0040 80808080 0000 80808080 4000 80808080 0010 112233ff", &f) && f.buttons[2] == 0x4000 &&
               f.buttons[3] == 0x10 && f.analog[3][3] == 0xFF,
           "a four-pad line (multitap recording)");
    expect(!parse_line("5 0040 80808080 0000 80808080 4000", &f), "half a pad is not a state");
    expect(parse_header("ttt-input 2 pads 4 multitap 1\n") && sMultitap && sPads == 4, "version 2 header");
    expect(parse_header("ttt-input 1 pads 2\n") && !sMultitap && sPads == 2, "version 1 header: no multitap");
    sMultitap = 1;
    sPads = 4;
    expect(sceMtapGetConnection(0) == 1 && sceMtapGetConnection(1) == 0, "the multitap on port 0");
    for (i = 0; i < 4; i++) {
        expect(scePadGetState(0, i) == 6, "slots 1A-1D stable");
    }
    expect(scePadGetState(1, 0) == 0, "port 1 empty with the multitap");

    Port_WindowTestInput(20, 1, 1);
    for (i = 0; i < 4; i++) {
        Port_WindowTestEvent(EV_ADDED, 100 + i, 0, 0);
    }
    Port_WindowTestEvent(EV_DOWN, 102, 0, 1); /* gamepad 3's south: pad 3's cross */
    expect(window_read(0, 2, r) == 32 && rbuttons(r) == 0x40 && r[14] == 0xFF, "pad 3 (1C): cross from south");
    window_read(0, 0, r);
    expect(rbuttons(r) == 0, "pad 1 sees nothing of it");
    Port_WindowTestEvent(EV_UP, 102, 0, 0);
    Port_WindowTestEvent(EV_DOWN, 103, 2, 1);
    Port_WindowTestEvent(EV_UP, 103, 2, 0);
    expect(window_read(0, 3, r) == 32 && rbuttons(r) == 0x80, "a west tap between two VBlanks: pad 4's square once");
    window_read(0, 3, r);
    expect(rbuttons(r) == 0, "the tap counts once");

    Port_WindowTestEvent(EV_AXIS, 100, 0, 32767); /* left stick right */
    Port_WindowTestEvent(EV_AXIS, 100, 1, 3000);  /* a little down: inside the d-pad's angle of right */
    window_read(0, 0, r);
    expect(r[6] == 0xFF && rbuttons(r) == 0x2000, "left stick right: left x 0xFF, the d-pad's right");
    Port_WindowTestEvent(EV_AXIS, 100, 0, 5000);
    Port_WindowTestEvent(EV_AXIS, 100, 1, -3000);
    window_read(0, 0, r);
    expect(r[6] == 0x80 && r[7] == 0x80 && rbuttons(r) == 0, "inside the 20% dead zone: centred, no d-pad");
    Port_WindowTestEvent(EV_AXIS, 100, 0, 23000);
    Port_WindowTestEvent(EV_AXIS, 100, 1, -23000);
    window_read(0, 0, r);
    expect(rbuttons(r) == (0x2000 | 0x1000) && r[6] > 0xC0 && r[7] < 0x40, "a diagonal: right and up");
    Port_WindowTestEvent(EV_AXIS, 100, 1, -6000);
    window_read(0, 0, r);
    expect(rbuttons(r) == 0x2000, "mostly right: right only");
    Port_WindowTestEvent(EV_AXIS, 100, 2, -32768); /* right stick left: libpad's right x */
    window_read(0, 0, r);
    expect(r[4] == 0x00 && r[5] == 0x80, "the right stick, no d-pad from it");
    Port_WindowTestInput(20, 0, 1); /* stick as d-pad off: the stick only */
    window_read(0, 0, r);
    expect(r[6] > 0xC0 && rbuttons(r) == 0, "stick as d-pad off");
    Port_WindowTestInput(0, 1, 1);
    Port_WindowTestEvent(EV_AXIS, 100, 0, 0);
    Port_WindowTestEvent(EV_AXIS, 100, 1, 0);
    Port_WindowTestEvent(EV_AXIS, 100, 2, 0);
    Port_WindowTestEvent(EV_AXIS, 100, 4, 30000); /* the left trigger past half way: L2 */
    window_read(0, 0, r);
    expect(rbuttons(r) == 0x0001 && r[4] == 0x80 && r[6] == 0x80, "left trigger: L2, sticks centred");
    Port_WindowTestEvent(EV_AXIS, 100, 4, 0);
    Port_WindowTestEvent(EV_DOWN, 101, 5, 1); /* guide: never a pad button */
    window_read(0, 1, r);
    expect(rbuttons(r) == 0, "guide presses nothing");
    Port_WindowTestEvent(EV_UP, 101, 5, 0);
    Port_WindowTestEvent(EV_DOWN, 101, 9, 1);
    Port_WindowTestEvent(EV_REMOVED, 101, 0, 0);
    window_read(0, 1, r);
    expect(rbuttons(r) == 0, "a gamepad removed: its buttons released");
    Port_WindowTestEvent(EV_ADDED, 101, 0, 0);

    /* vibration through the game's alignment: byte 0 the small motor, byte 1 the large one */
    expect(scePadSetActAlign(0, 1, kAlign) == 1, "actuator alignment");
    act[0] = 1;
    act[1] = 200;
    expect(scePadSetActDirect(0, 1, act) == 1, "the motors set");
    calls = Port_WindowTestRumble(1, &lo, &hi);
    expect(calls == 1 && lo == 200 * 257 && hi == 0xFFFF, "pad 2's gamepad: large 200 -> low 51400, small on -> high");
    expect(Port_WindowTestRumble(0, &lo, &hi) == 0, "no other gamepad rumbles");
    expect(Port_WindowTestRumble(1, &lo, &hi) == 1, "unchanged: not sent again at once");
    act[0] = 0;
    act[1] = 0;
    scePadSetActDirect(0, 1, act);
    expect(Port_WindowTestRumble(1, &lo, &hi) == 2 && lo == 0 && hi == 0, "the motors off");
    Port_WindowTestInput(0, 1, 0); /* input.vibration off */
    act[1] = 255;
    scePadSetActDirect(0, 1, act);
    expect(Port_WindowTestRumble(1, &lo, &hi) == 2 && lo == 0 && hi == 0, "vibration off: nothing");
    expect(scePadSetActDirect(1, 0, act) == 0, "port 1: no pad with the multitap");
    sMultitap = 0;
    sPads = 2;
    expect(sceMtapGetConnection(0) == 0 && scePadGetState(1, 0) == 6 && scePadGetState(0, 1) == 0,
           "without the multitap: ports 0 and 1");
}

/* The recording's line format, the read data, and the answers of the game's set-up sequence (Pad_Update). Called
   by __wrap_main (plat_main.c) before the game starts. Not a constructor: those go to .text.startup, which the linker
   puts before the game's code, and would move every game function (the state hash covers some of their addresses). */
void Port_PadSelfTest(void) {
    const char *t = getenv("TTT_SELFTEST");
    PadInput f;
    uint8_t r[32];
    int i, press = 0;

    if (t == NULL || strcmp(t, "pad") != 0) {
        return;
    }
    expect(parse_line("120 0840 11223344 0000 80808080\n", &f), "a state line parses");
    expect(f.vblank == 120 && f.buttons[0] == 0x0840 && f.buttons[1] == 0, "vblank and buttons");
    expect(f.analog[0][0] == 0x11 && f.analog[0][3] == 0x44 && f.analog[1][2] == 0x80, "analog bytes in order");
    expect(f.buttons[2] == 0 && f.analog[3][0] == 0x80, "a two-pad line: pads 3 and 4 idle");
    expect(!parse_line("# comment\n", &f) && !parse_line("ttt-input 1 pads 2\n", &f), "comment and header skipped");
    parse_line("1 0840 11223344 0000 80808080", &f);
    encode(&f, 0, r);
    expect(r[0] == 0 && r[1] == 0x79, "kind 0x79: DualShock 2 in pressure mode");
    expect((uint16_t)~((r[2] << 8) | r[3]) == 0x0840, "buttons active low, as Pad_ReadPort reads them");
    expect(memcmp(r + 4, "\x11\x22\x33\x44", 4) == 0, "sticks");
    for (i = 0; i < 12; i++) {
        press += r[8 + i] != 0;
    }
    expect(r[14] == 0xFF && press == 1, "only cross's pressure byte (start has none)");
    pads();
    sMultitap = 0;
    sPads = 2;
    now_put(&f);
    expect(scePadGetState(0, 0) == 6 && scePadGetState(1, 0) == 6, "ports 0 and 1 stable");
    expect(scePadGetState(0, 1) == 0 && scePadGetState(2, 0) == 0, "no multitap slots");
    expect(sceMtapGetConnection(0) == 0, "no multitap");
    expect(scePadInfoMode(0, 0, 1, 0) == 7 && scePadInfoMode(1, 0, 2, 0) == 7, "analog id and extended id");
    expect(scePadInfoPressMode(0, 0) == 1 && scePadEnterPressMode(0, 0) == 1 && scePadGetReqState(0, 0) == 0,
           "pressure mode entered");
    expect(scePadInfoAct(0, 0, -1, 0) == 2 && scePadSetActAlign(0, 0, r) == 1, "two actuators");
    expect(scePadRead(0, 0, r) == 32 && r[1] == 0x79 && scePadRead(0, 1, r) == 0, "read");
    sPads = 1;
    expect(scePadGetState(1, 0) == 0 && scePadRead(1, 0, r) == 0, "TTT_PADS=1: port 1 empty");
    self_test_gamepads();
    printf(sFails ? "ttt: pad self-test FAILED (%d)\n" : "ttt: pad self-test OK\n", sFails);
    exit(sFails != 0);
}

/* The state: the pads as latched at the last VBlank (the replay's position follows the VBlank count). The set-up comes
   first, so that a later one does not undo a load. */
static void pad_before(void) { pads(); }
static void pad_after_load(void) {
    uint32_t n = Port_VBlankCount();
    sPlayNext = 0;
    while (sPlayNext < sPlayCount && sPlay[sPlayNext].vblank <= n) {
        sPlayNext++;
    }
}
__attribute__((constructor(101))) static void pad_state(void) {
    Port_StateAdd("pad.now", &sNow, sizeof sNow, PS_HASH);
    Port_StateAdd("pad.more", &sMore, sizeof sMore, 0);
    Port_StateAdd("pad.pressedVBlanks", &sPressedVBlanks, sizeof sPressedVBlanks, 0);
    Port_StateHooks(NULL, pad_before, pad_after_load);
}
