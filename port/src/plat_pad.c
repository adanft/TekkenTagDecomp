/*
 * Pads on PC (T34): libpad and libmtap as the game uses them (src/pad.c), fed once per VBlank.
 *
 * Two DualShock 2 pads on ports 0 and 1 (slot 0), no multitap. Each one walks the game's set-up sequence of a real
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
 * How many pads are connected (TTT_PADS=0..2): 2 in real time, 0 headless (the runs and state hashes of the earlier
 * tasks have no pad), the recording's count with TTT_INPUT_PLAY.
 *
 * Recording format (text): a header line `ttt-input 1 pads <n>`, then one line per VBlank where the state changes:
 *   <vblank> <buttons0> <analog0> <buttons1> <analog1>
 * in hex, buttons with 1 = pressed in libpad's order (0x8000 left, 0x4000 down, 0x2000 right, 0x1000 up, 0x0800 start,
 * 0x0400 R3, 0x0200 L3, 0x0100 select, 0x80 square, 0x40 cross, 0x20 circle, 0x10 triangle, 8 R1, 4 L1, 2 R2, 1 L2),
 * analog as 8 hex digits: right x, right y, left x, left y. A state holds until the next line; `#` starts a comment.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Port_Hit(const char *name);                            /* plat_stub.c */
int Port_Headless(void);                                    /* plat_hw.c */
int Port_WindowOpen(void);                                  /* plat_window.c */
void Port_WindowPads(uint16_t buttons[2], uint8_t analog[2][4]); /* plat_window.c */

#define PAD_MAX 2

typedef struct {
    uint32_t vblank;
    uint16_t buttons[PAD_MAX];
    uint8_t analog[PAD_MAX][4];
} PadFrame;

static int sPads = -1; /* connected pads, -1 before the first use */
static PadFrame sNow;  /* the state latched at the last VBlank */
static PadFrame *sPlay;
static int sPlaying; /* TTT_INPUT_PLAY */
static uint32_t sPlayCount, sPlayNext;
static FILE *sRecord;
static PadFrame sRecorded;
static int sRecordedAny;
static uint32_t sPressedVBlanks; /* VBlanks with a button down (the report) */

static void centre(PadFrame *f) {
    memset(f, 0, sizeof *f);
    memset(f->analog, 0x80, sizeof f->analog);
}

/* Parses one line of a recording into `f`; 1 when it holds a state. */
/* (strtoul, not sscanf or realloc below: host calls already in the program's PLT, see port_host.h) */
static int parse_line(const char *line, PadFrame *f) {
    unsigned long v[1 + 2 * PAD_MAX];
    char *end;
    int k, p, i;

    for (k = 0; k < 1 + 2 * PAD_MAX; k++) {
        while (*line == ' ' || *line == '\t') {
            line++;
        }
        v[k] = strtoul(line, &end, k == 0 ? 10 : 16);
        if (end == line) {
            return 0;
        }
        line = end;
    }
    f->vblank = (uint32_t)v[0];
    for (p = 0; p < PAD_MAX; p++) {
        f->buttons[p] = (uint16_t)v[1 + 2 * p];
        for (i = 0; i < 4; i++) {
            f->analog[p][i] = (uint8_t)(v[2 + 2 * p] >> (24 - 8 * i));
        }
    }
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
        static const char kHeader[] = "ttt-input 1 pads ";
        PadFrame f;
        if (line[0] == '#') {
            continue;
        }
        if (memcmp(line, kHeader, sizeof kHeader - 1) == 0) {
            int n = atoi(line + sizeof kHeader - 1);
            sPads = n < 0 ? 0 : n > PAD_MAX ? PAD_MAX : n;
            continue;
        }
        if (!parse_line(line, &f)) {
            continue;
        }
        if (sPlayCount == cap) {
            PadFrame *grown;
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

static void setup(void) {
    const char *play = getenv("TTT_INPUT_PLAY"), *rec = getenv("TTT_INPUT_RECORD"), *pads = getenv("TTT_PADS");

    sPads = Port_Headless() ? 0 : PAD_MAX;
    centre(&sNow);
    if (play != NULL && *play != 0) {
        sPads = PAD_MAX;
        sPlaying = 1;
        load_play(play);
    }
    if (pads != NULL && *pads != 0) {
        int n = atoi(pads);
        sPads = n < 0 ? 0 : n > PAD_MAX ? PAD_MAX : n;
    }
    if (rec != NULL && *rec != 0) {
        sRecord = fopen(rec, "w");
        if (sRecord == NULL) {
            printf("ttt: input: cannot write %s\n", rec);
            exit(2);
        }
        fprintf(sRecord, "ttt-input 1 pads %d\n# vblank buttons0 analog0 buttons1 analog1 (port/src/plat_pad.c)\n", sPads);
        fflush(sRecord);
    }
}

static int pads(void) {
    if (sPads < 0) {
        setup();
    }
    return sPads;
}

static void record(void) {
    int p;
    if (sRecordedAny && memcmp(sRecorded.buttons, sNow.buttons, sizeof sNow.buttons) == 0 &&
        memcmp(sRecorded.analog, sNow.analog, sizeof sNow.analog) == 0) {
        return;
    }
    sRecorded = sNow;
    sRecordedAny = 1;
    fprintf(sRecord, "%u", (unsigned)sNow.vblank);
    for (p = 0; p < PAD_MAX; p++) {
        const uint8_t *a = sNow.analog[p];
        fprintf(sRecord, " %04x %02x%02x%02x%02x", sNow.buttons[p], a[0], a[1], a[2], a[3]);
    }
    fprintf(sRecord, "\n");
    fflush(sRecord);
}

/* At VBlank `n` (plat_hw.c): the pads' state for the game until the next one. */
void Port_PadVBlank(uint32_t n) {
    int p;

    if (pads() == 0 && sRecord == NULL) {
        return;
    }
    if (sPlaying) {
        while (sPlayNext < sPlayCount && sPlay[sPlayNext].vblank <= n) {
            sNow = sPlay[sPlayNext++];
        }
    } else if (Port_WindowOpen()) {
        Port_WindowPads(sNow.buttons, sNow.analog);
    }
    for (p = sPads; p < PAD_MAX; p++) { /* not connected: nothing pressed */
        sNow.buttons[p] = 0;
        memset(sNow.analog[p], 0x80, 4);
    }
    sNow.vblank = n;
    if ((sNow.buttons[0] | sNow.buttons[1]) != 0) {
        sPressedVBlanks++;
    }
    if (sRecord != NULL) {
        record();
    }
}

/* The TTT_FRAMES report line (only with pads). */
void Port_PadReport(void) {
    if (sPads > 0) {
        printf("ttt: input: %d pads, %u vblanks with a button down%s\n", sPads, (unsigned)sPressedVBlanks,
               sPlaying ? " (played)" : "");
    }
}

/* libpad's read data of pad state `f` / pad `p`: kind 0x79 (DualShock 2, pressure mode). */
static void encode(const PadFrame *f, int p, uint8_t rdata[32]) {
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

static int connected(int port, int slot) {
    return slot == 0 && port >= 0 && port < pads();
}

/* ---- libmtap: no multitap ---- */
int sceMtapInit(void) { return 1; }
int sceMtapPortOpen(int port) { (void)port; return 1; }
int sceMtapPortClose(int port) { (void)port; return 1; }
int sceMtapGetConnection(int port) { (void)port; return 0; }

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
    if (!connected(port, slot)) {
        return 0;
    }
    encode(&sNow, port, rdata);
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
int scePadSetActAlign(int port, int slot, const uint8_t *data) { (void)data; return connected(port, slot) ? 1 : 0; }
/* Vibration is not played yet (T34 later batch). */
int scePadSetActDirect(int port, int slot, const uint8_t *data) { (void)data; return connected(port, slot) ? 1 : 0; }

/* ---- self-test (TTT_SELFTEST=pad) ---- */
static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: pad self-test FAILED: %s\n", what);
        sFails++;
    }
}

/* The recording's line format, the read data, and the answers of the game's set-up sequence (Pad_Update). Called
   by __wrap_main (plat_main.c) before the game starts. Not a constructor: those go to .text.startup, which the linker
   puts before the game's code, and would move every game function (the state hash covers some of their addresses). */
void Port_PadSelfTest(void) {
    const char *t = getenv("TTT_SELFTEST");
    PadFrame f;
    uint8_t r[32];
    int i, press = 0;

    if (t == NULL || strcmp(t, "pad") != 0) {
        return;
    }
    expect(parse_line("120 0840 11223344 0000 80808080\n", &f), "a state line parses");
    expect(f.vblank == 120 && f.buttons[0] == 0x0840 && f.buttons[1] == 0, "vblank and buttons");
    expect(f.analog[0][0] == 0x11 && f.analog[0][3] == 0x44 && f.analog[1][2] == 0x80, "analog bytes in order");
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
    sPads = 2;
    sNow = f;
    expect(scePadGetState(0, 0) == 6 && scePadGetState(1, 0) == 6, "ports 0 and 1 stable");
    expect(scePadGetState(0, 1) == 0 && scePadGetState(2, 0) == 0, "no multitap slots");
    expect(scePadInfoMode(0, 0, 1, 0) == 7 && scePadInfoMode(1, 0, 2, 0) == 7, "analog id and extended id");
    expect(scePadInfoPressMode(0, 0) == 1 && scePadEnterPressMode(0, 0) == 1 && scePadGetReqState(0, 0) == 0,
           "pressure mode entered");
    expect(scePadInfoAct(0, 0, -1, 0) == 2 && scePadSetActAlign(0, 0, r) == 1, "two actuators");
    expect(scePadRead(0, 0, r) == 32 && r[1] == 0x79 && scePadRead(0, 1, r) == 0, "read");
    sPads = 1;
    expect(scePadGetState(1, 0) == 0 && scePadRead(1, 0, r) == 0, "TTT_PADS=1: port 1 empty");
    printf(sFails ? "ttt: pad self-test FAILED (%d)\n" : "ttt: pad self-test OK\n", sFails);
    exit(sFails != 0);
}
