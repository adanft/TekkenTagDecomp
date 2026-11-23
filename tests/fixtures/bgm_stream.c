/* Synthetic BGM sectors through the public stream/SPU interfaces; no game data. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spu2.h"
#include "plat_state.h"

void Port_BgmInit(void);
void Port_BgmTrack(const uint32_t *a);
void Port_BgmQueue(const uint8_t *q);
void Port_BgmTick(void);
void Port_BgmService(void);
uint32_t Port_BgmStatus(void);

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static const uint32_t base[2] = {0x1EE000, 0x1EE800};
static struct { void *p; uint32_t n; uint8_t *saved; } regions[8];
static int nr, marker, garbage, reads, piece = 128;
static uint32_t pcm = 2166136261u;
void Port_StateAdd(const char *name, void *p, uint32_t n, int flags) {
    (void)name; (void)flags;
    CHECK(nr < 8);
    regions[nr].p = p; regions[nr++].n = n;
}
int Port_MovieAudio(int m, uint32_t *s, uint32_t *a, uint32_t *v, uint32_t *b, uint32_t *g) {
    (void)m; (void)s; (void)a; (void)v; (void)b; (void)g;
    return 0;
}
int Port_DiscRead(uint32_t sector, uint32_t offset, void *dst, uint32_t bytes) {
    uint8_t data[2048];
    CHECK(sector == 123 && offset % 2048 == 0 && bytes == 2048);
    for (int c = 0; c < 2; c++) for (int b = 0; b < 64; b++) {
        uint8_t *p = data + c * 1024 + b * 16;
        p[0] = 12; p[1] = marker && offset == 0 && b == 49 ? 4 : 0;
        /* Issue 13: memory that is not ADPCM (an ending's fighter loaded over a type-3 track) has stray end (1) and
           end + repeat (3) flags in the middle of a half. */
        if (garbage && offset >= 2048 && b % 7 >= 5) p[1] = b % 7 == 5 ? 1 : 3;
        memset(p + 2, 0x11 * (1 + (offset / 2048 + b + c) % 7), 14);
    }
    memcpy(dst, data, bytes); reads++;
    return 0;
}
static void start(int sectors, int once, int hold) {
    uint32_t t[] = {11, 1u | ((once ? 1u : 255u) << 8), 0x2000, 123,
                    0x350A + (uint32_t)sectors * 2048, 0};
    uint8_t q[] = {0x45, 128, 0x40, (uint8_t)(11 | (hold ? 128 : 0)), 255};
    Port_BgmTrack(t); Port_BgmQueue(q);
    for (int i = 0; i < 11; i++) Port_BgmTick();
}
static void step(int i) {
    int16_t out[2];
    Spu2_Mix(out, 1);
    for (int c = 0; c < 2; c++) pcm = (pcm ^ (uint16_t)out[c]) * 16777619u;
    if (i % piece == 0) Port_BgmService();
    if (i % 201 == 0) Port_BgmTick();
}
static void traversal(int sectors, int finite) {
    uint32_t previous = base[0];
    int blocks = 0;
    for (int i = 0; i < 30000; i++) {
        step(i);
        uint32_t n = gSpu2[0].v[22].nax;
        CHECK(gSpu2[0].v[23].nax - base[1] == n - base[0]);
        CHECK(gSpu2[0].v[22].pitch == 0xEB3 && gSpu2[0].v[23].pitch == 0xEB3);
        if (n != previous) {
            blocks++;
            CHECK(n == base[0] + (uint32_t)(blocks % 128) * 16);
            previous = n;
        }
        if (!gSpu2[0].v[22].keyed) {
            CHECK(finite && blocks == sectors * 64 - 1);
            CHECK(!gSpu2[0].v[23].keyed);
            for (int j = 0; j < 300; j++) step(i + 1 + j);
            CHECK(Port_BgmStatus() == 0x40);
            return;
        }
    }
    CHECK(!finite && blocks > 700 && reads > 10);
}
static void resident(void) {
    uint32_t t[] = {0, 2u | (255u << 8), 0x2000, 0, 0, 0};
    uint8_t q[] = {0x40, 0, 255};
    const uint32_t addr[] = {0x128040, 0x18A290};
    for (int c = 0; c < 2; c++) {
        uint8_t *p = Spu2_Ram() + addr[c];
        memset(p, 0, 48); p[17] = 4; p[33] = 3;
    }
    Port_BgmTrack(t); Port_BgmQueue(q);
    for (int i = 0; i < 3; i++) Port_BgmTick();
    for (int i = 0; i < 1000; i++) {
        step(i);
        for (int c = 0; c < 2; c++) {
            Spu2Voice *v = &gSpu2[0].v[22 + c];
            CHECK(!v->lsaSet && v->nax >= addr[c] && v->nax < addr[c] + 48);
            if (i > 100) CHECK(v->lsa == addr[c] + 16);
        }
    }
}
static uint32_t state_hash(void) {
    uint32_t h = 2166136261u;
    for (int r = 0; r < nr; r++) for (uint32_t i = 0; i < regions[r].n; i++)
        h = (h ^ ((uint8_t *)regions[r].p)[i]) * 16777619u;
    return h;
}
int main(int argc, char **argv) {
    CHECK(argc >= 2 && nr == 4);
    const char *mode = argv[1];
    marker = !strcmp(mode, "loop") || !strcmp(mode, "restore") || !strcmp(mode, "finite");
    garbage = !strcmp(mode, "garbage");
    if (argc > 3) piece = atoi(argv[3]);
    CHECK(piece == 1 || piece == 128);
    Spu2_Reset(); Port_BgmInit();
    gSpu2[0].mvolL = gSpu2[0].mvolR = 0x3FFF;
    gSpu2[1].mvolL = gSpu2[1].mvolR = 0x3FFF;
    /* An independent core-1 effect must still honor its own end/no-repeat flag. */
    uint8_t *effect = Spu2_Ram() + 0x7000;
    memset(effect, 0x11, 32); effect[0] = effect[16] = 12; effect[1] = 4; effect[17] = 1;
    Spu2Voice *v = &gSpu2[1].v[3];
    v->ssa = 0x7000; v->pitch = 0x1000; v->volL = v->volR = 0x1000;
    Spu2_KeyOn(1, 1u << 3);
    if (!strcmp(mode, "finite") || !strcmp(mode, "resident")) {
        int n = !strcmp(mode, "resident") ? 3 : atoi(argv[2]);
        CHECK(n >= 1 && n <= 4);
        start(n, 1, 0); traversal(n, 1);
        if (!strcmp(mode, "resident")) resident();
    } else if (!strcmp(mode, "hold")) {
        start(8, 0, 1);
        CHECK(Port_BgmStatus() == 0x10 && !gSpu2[0].v[22].keyed && reads == 0);
        uint8_t release[] = {0x40, 11, 255};
        Port_BgmQueue(release); Port_BgmTick(); traversal(8, 0);
    } else if (!strcmp(mode, "stop") || !strcmp(mode, "fade")) {
        start(8, 0, 0);
        for (int i = 0; i < 2000; i++) step(i);
        uint8_t stop[] = {0x43, 255}, fade[] = {0x44, 0, 1, 255};
        Port_BgmQueue(!strcmp(mode, "stop") ? stop : fade);
        for (int i = 2000; i < 14000; i++) step(i);
        CHECK(Port_BgmStatus() == 0x40 && !gSpu2[0].v[22].keyed && !gSpu2[0].v[23].keyed);
        CHECK(reads <= 5);
    } else if (!strcmp(mode, "garbage")) {
        /* The voice keeps playing through the stray flags, the fade still stops the track, and the next one starts. */
        start(8, 0, 0);
        for (int i = 0; i < 4000; i++) {
            step(i);
            CHECK(gSpu2[0].v[22].phase != SPU2_OFF && gSpu2[0].v[22].nax - base[0] < 2048);
        }
        uint8_t fade[] = {0x44, 0, 1, 255};
        Port_BgmQueue(fade);
        for (int i = 4000; i < 16000; i++) step(i);
        CHECK(Port_BgmStatus() == 0x40 && !gSpu2[0].v[22].keyed && !gSpu2[0].v[23].keyed);
        garbage = 0; reads = 0;
        start(2, 1, 0); traversal(2, 1);
    } else if (!strcmp(mode, "restore")) {
        start(8, 0, 0);
        for (int i = 0; i < 4000; i++) step(i);
        for (int r = 0; r < nr; r++) {
            regions[r].saved = malloc(regions[r].n); CHECK(regions[r].saved);
            memcpy(regions[r].saved, regions[r].p, regions[r].n);
        }
        uint32_t saved_pcm = pcm;
        int saved_reads = reads;
        for (int i = 4000; i < 10000; i++) step(i);
        uint32_t expected = state_hash(), expected_pcm = pcm;
        int expected_reads = reads;
        for (int r = 0; r < nr; r++) {
            memcpy(regions[r].p, regions[r].saved, regions[r].n); free(regions[r].saved);
        }
        pcm = saved_pcm; reads = saved_reads;
        for (int i = 4000; i < 10000; i++) step(i);
        CHECK(state_hash() == expected && pcm == expected_pcm && reads == expected_reads);
    } else {
        CHECK(!strcmp(mode, "loop") || !strcmp(mode, "ordinary"));
        start(8, 0, 0);
        if (marker) for (int c = 0; c < 2; c++) CHECK(Spu2_Ram()[base[c] + 49 * 16 + 1] == 4);
        traversal(8, 0);
    }
    CHECK(gSpu2[1].v[3].phase == SPU2_OFF && gSpu2[1].v[3].lsa == 0x7000);
    CHECK(Spu2_KeyStatus(1, 3) == 3);
    printf("bgm fixture OK %s pcm=%08x reads=%d\n", mode, pcm, reads);
    return 0;
}
