/*
 * The BGM stream engine of RSPU2DRV (T32), on the SPU2 model: core 0 voices 22 (left) and 23 (right).
 *
 * As the driver does it (docs/port/sound.md, IOP addresses in docs/systems/audio.md):
 *  - 28 tracks registered by 0x205E: type (1 disc, 2 SPU-resident, 3 EE memory), a play count (negative: for ever),
 *    a volume scale, the table sector, the biased size, the loop sector.
 *  - The sound queue (0x7F0F) sets requests: 0x40 n play (bit 7: load and hold), 0x43 stop, 0x44 v t fade,
 *    0x45 v option volume, 0x50 / 0x51 stereo / mono, 0x5C n movie audio, 0x5D stop flag.
 *  - Movie audio (0x5C n, T33): a stream track of its own (type 4) on the same voices, buffers and state machine: the
 *    ADPCM sectors of the movie file 0x2058 started (each chunk of the file is 7 audio sectors, then 0x80 of video),
 *    played once, with the length and the volume scale of the driver's per-movie table (plat_movie.c) [I: the
 *    driver's movie mode reads the same interleave; its own timing is not modelled].
 *  - A state machine stepped on the stream thread's timer, H-line / 66 (about 238.4 Hz, 3.98 ticks a VBlank):
 *    1 idle, 2 prefill, 3 ready (held while the hold flag is set), 4 playing, 5 stopped (back to 1). Status (0x2030)
 *    bit 4 = state 3, bit 6 = no stream active.
 *  - libspu2's SpuSt double buffers: voice 22 plays SPU RAM 0x1EE000, voice 23 0x1EE800, two halves of 1024 bytes
 *    each, at pitch 0xEB3 (44.1 kHz); every 2048-byte sector of a track is 1024 bytes of left ADPCM then 1024 of right.
 *    When a voice leaves a half, that half gets the next sector. The flags of three blocks of each half are rewritten:
 *    first half 6, 2, ..., 2; second half 2, 2, ..., 3 (end + repeat: back to the buffer start). The last chunk ends
 *    in a silent block looping on itself (00 07); reaching it keys the voices off and stops the track.
 *  - Volume every tick: (option volume * track scale * fade) >> 14, voice 22 = (v, 0), voice 23 = (0, v) in stereo,
 *    both v * 65 / 100 in mono. Fades: linear over t * 4 ticks.
 *
 * Differences: the driver's 512 KB IOP ring and its CD reads are not modelled; a sector is read from the disc (or
 * from EE memory for type 3) when its half is refilled. The prefill (64 sectors) takes 9 ticks, the time a 24x CD
 * needs for them. Everything advances only with the ticks and the samples mixed (plat_snd.c), never the host clock.
 */
#include <stdio.h>
#include <string.h>
#include "port_addr.h"
#include "spu2.h"
#include "plat_state.h"

int Port_DiscRead(uint32_t sector, uint32_t offset, void *dst, uint32_t bytes); /* plat_file.c */
int Port_MovieAudio(int movie, uint32_t *sector, uint32_t *audio, uint32_t *video, uint32_t *bytes,
                    uint32_t *scale); /* plat_movie.c */

#define BGM_L 22
#define BGM_R 23
#define BUF_L 0x1EE000u
#define BUF_R 0x1EE800u
#define HALF 1024u
#define TRACKS 28
#define MOVIE_TRACK TRACKS /* the track slot of code 0x5C */
#define SIZE_BIAS 0x350Au
#define PREFILL_TICKS 9
#define EE_STREAM_BASE 0x005F61D4u /* gEeStreamBase: type 3 tracks come from EE memory there */

typedef struct {
    uint8_t type;
    int8_t count;
    uint16_t scale;
    uint32_t sector, bytes, loop; /* loop: byte offset of the loop start */
    uint32_t audio, video;        /* type 4 (movie): sectors of audio, then of video, in each chunk of the file */
} Track;

static struct {
    Track tracks[TRACKS + 1];
    int req;                 /* requested track, -1 none */
    int movieReq;            /* the request is code 0x5C's (MOVIE_TRACK) */
    int hold, stopReq;
    int state, mode;         /* state machine; mode 1 idle, 2 BGM */
    int cur;                 /* current track */
    int prefill;             /* ticks left */
    int count;               /* plays left (negative: for ever) */
    uint32_t pos;            /* next byte of the track to send */
    int half;                /* the half voice 22 is playing */
    int final;               /* the last chunk is in the buffer */
    uint32_t finalAddr;      /* voice 22's address of its silent end block */
    int optVol, fade, stereo;
    int fadeFrom, fadeTo, fadeLen, fadeN;
} sB = { .req = -1, .cur = -1, .state = 1, .mode = 1, .stereo = 1, .fade = 128 };

static void set_voice_vol(int voice, int l, int r) {
    gSpu2[0].v[voice].volL = (uint16_t)(l & 0x7FFF);
    gSpu2[0].v[voice].volR = (uint16_t)(r & 0x7FFF);
}

/* The stream voices as command 0x2000 sets them up (mask 0xFF93: volume 0, pitch, start, ADSR). */
static void stream_voices(void) {
    int i;
    for (i = BGM_L; i <= BGM_R; i++) {
        Spu2Voice *v = &gSpu2[0].v[i];
        v->volL = v->volR = 0;
        v->pitch = 0x0EB3;
        v->ssa = i == BGM_L ? BUF_L : BUF_R;
        v->adsr1 = 0x000F;
        v->adsr2 = 0x0003;
    }
}

/* Command 0x2000 (after SpuInit): the stream voices keyed off and set up, no stream active. */
/* The SPU2 voices of the music (core 0), for the host's music volume (plat_snd.c, T40): the BGM and movie tracks. */
uint32_t Port_BgmVoices(void) { return 3u << BGM_L; }

void Port_BgmInit(void) {
    Spu2_KeyOff(0, 3u << BGM_L);
    stream_voices();
    sB.req = sB.cur = -1;
    sB.state = sB.mode = 1;
}

/* 0x205E: track a[0] = {type | count << 8, scale | x << 16, sector, biased size, loop sector}. */
void Port_BgmTrack(const uint32_t *a) {
    Track *t;
    uint32_t size = a[4] - SIZE_BIAS;
    if (a[0] >= TRACKS) {
        return;
    }
    t = &sB.tracks[a[0]];
    t->type = (uint8_t)a[1];
    t->count = (int8_t)(a[1] >> 8);
    t->scale = (uint16_t)a[2];
    t->sector = a[3];
    t->bytes = (int32_t)a[4] > (int32_t)SIZE_BIAS ? size : 0;
    t->loop = (int32_t)a[5] > 0 ? a[5] * 2048u : 0;
    if (t->type == 3) {
        t->loop = 0; /* the driver forces it: type 3 tracks loop to their start */
    }
}

/* 0x7F0F: the queue's byte codes, up to 0xFF (at most 48 bytes). */
void Port_BgmQueue(const uint8_t *q) {
    int i = 0;
    while (i < 48 && q[i] != 0xFF) {
        switch (q[i]) {
        case 0x40: {
            int n = q[i + 1];
            if (n & 0x80) {
                sB.req = n & 0x7F;
                sB.hold = 1;
            } else if ((sB.state == 2 || sB.state == 3) && sB.cur == n) {
                sB.hold = 0;
            } else {
                sB.req = n;
                sB.hold = 0;
            }
            i += 2;
            break;
        }
        case 0x43:
            sB.stopReq = 1;
            sB.req = -1;
            i += 1;
            break;
        case 0x44:
            if (q[i + 2] == 0) {
                sB.fade = q[i + 1] > 128 ? 128 : q[i + 1];
                sB.fadeLen = 0;
            } else {
                sB.fadeFrom = sB.fade;
                sB.fadeTo = q[i + 1] > 128 ? 128 : q[i + 1];
                sB.fadeLen = sB.fadeN = q[i + 2] * 4;
            }
            i += 3;
            break;
        case 0x45:
            sB.optVol = q[i + 1] > 128 ? 128 : q[i + 1];
            i += 2;
            break;
        case 0x50:
        case 0x51:
            sB.stereo = q[i] == 0x50;
            i += 1;
            break;
        case 0x5C: { /* the audio of movie n: a track of its own */
            Track *t = &sB.tracks[MOVIE_TRACK];
            uint32_t scale;
            if (Port_MovieAudio(q[i + 1], &t->sector, &t->audio, &t->video, &t->bytes, &scale)) {
                t->type = 4;
                t->count = 1;
                t->scale = (uint16_t)scale;
                t->loop = 0;
                sB.req = MOVIE_TRACK;
                sB.movieReq = 1;
                sB.hold = 0;
            }
            i += 2;
            break;
        }
        case 0x5D:
            sB.stopReq = 1;
            i += 1;
            break;
        default:
            return; /* the driver would hang on an unknown code: stop reading */
        }
    }
}

/* Read-only views for the host's music mods (plat_mods.c, T49), which replace what the host plays and never the
   engine: the table track playing now (state 4; -1 none, the movie track included), with its volume (option volume *
   fade, 0x4000 = 1), stereo, and whether it loops for ever. */
int Port_BgmNow(int *gain, int *stereo, int *loops) {
    if (sB.state != 4 || sB.cur < 0 || sB.cur >= TRACKS) {
        return -1;
    }
    *gain = sB.optVol * sB.fade;
    *stereo = sB.stereo;
    *loops = sB.count < 0;
    return sB.cur;
}

/* Track i as 0x205E registered it: 0 when it is not registered (yet). */
int Port_BgmTrackInfo(int i, int *type, int *count, uint32_t *bytes) {
    if (i < 0 || i >= TRACKS || sB.tracks[i].type == 0) {
        return 0;
    }
    *type = sB.tracks[i].type;
    *count = sB.tracks[i].count;
    *bytes = sB.tracks[i].bytes;
    return 1;
}

uint32_t Port_BgmStatus(void) {
    return (sB.state == 3 ? 0x10u : 0) | (sB.mode == 1 ? 0x40u : 0);
}

/* The next 2048-byte sector of the current track into `sec`; marks the chunk final at the track's end. */
static void next_sector(uint8_t *sec) {
    const Track *t = &sB.tracks[sB.cur];
    memset(sec, 0, 2048);
    if (t->type == 3) {
        uint32_t base = *(uint32_t *)Port_Ptr(EE_STREAM_BASE);
        if (base != 0 && Port_IsEeRam(base + sB.pos)) {
            memcpy(sec, Port_Ptr(base + sB.pos), 2048);
        }
    } else if (t->type == 4) { /* movie: audio sector j of the interleaved file */
        uint32_t j = sB.pos / 2048;
        Port_DiscRead(t->sector, ((j / t->audio) * (t->audio + t->video) + j % t->audio) * 2048, sec, 2048);
    } else if (sB.pos < t->bytes) {
        Port_DiscRead(t->sector, sB.pos, sec, t->bytes - sB.pos < 2048 ? t->bytes - sB.pos : 2048);
    }
    sB.pos += 2048;
    if (sB.pos >= t->bytes) {
        if (sB.count > 0) {
            sB.count--;
        }
        if (sB.count == 0) {
            sB.final = 1;
        }
        sB.pos = t->loop < t->bytes ? t->loop : 0;
    }
    if (sB.stopReq) {
        sB.final = 1;
    }
}

/* Half `h` of both buffers gets the next sector, with SpuSt's flags; the last chunk ends in a silent loop block. */
static void fill_half(int h) {
    uint8_t sec[2048];
    int c;
    next_sector(sec);
    for (c = 0; c < 2; c++) {
        uint8_t *d = Spu2_Ram() + (c == 0 ? BUF_L : BUF_R) + (uint32_t)h * HALF;
        memcpy(d, sec + c * HALF, HALF);
        d[1] = h == 0 ? 6 : 2;
        d[16 + 1] = 2;
        d[HALF - 16 + 1] = h == 0 ? 2 : 3;
        if (sB.final) {
            memset(d + HALF - 16, 0, 16);
            d[HALF - 16 + 1] = 7;
        }
    }
    if (sB.final) {
        sB.finalAddr = BUF_L + (uint32_t)h * HALF + HALF - 16;
    }
}

static void key_on(void) {
    const Track *t = &sB.tracks[sB.cur];
    stream_voices();
    if (t->type == 2) { /* SPU-resident (loaded by the game), looping by its own flags */
        gSpu2[0].v[BGM_L].ssa = 0x128040;
        gSpu2[0].v[BGM_R].ssa = 0x18A290;
    } else {
        fill_half(0);
        if (!sB.final) {
            fill_half(1);
        }
        sB.half = 0;
    }
    Spu2_KeyOn(0, 3u << BGM_L);
}

static void stop_now(void) {
    Spu2_KeyOff(0, 3u << BGM_L);
    sB.state = 5;
}

/* One tick of the stream thread: the state machine, the fade, the volumes. */
void Port_BgmTick(void) {
    switch (sB.state) {
    case 1:
        if ((sB.req >= 0 && sB.req < TRACKS) || (sB.req == MOVIE_TRACK && sB.movieReq)) {
            sB.movieReq = 0;
            const Track *t = &sB.tracks[sB.req];
            sB.cur = sB.req;
            sB.req = -1;
            sB.stopReq = 0;
            sB.final = 0;
            sB.count = t->count;
            sB.pos = 0;
            sB.mode = 2;
            sB.prefill = t->type == 2 ? 0 : PREFILL_TICKS;
            sB.state = 2;
        }
        break;
    case 2:
        if (sB.stopReq) {
            sB.state = 5;
        } else if (--sB.prefill <= 0) {
            sB.state = 3;
        }
        break;
    case 3:
        if (sB.stopReq) {
            sB.state = 5;
        } else if (!sB.hold) {
            sB.fade = 128;
            sB.fadeLen = 0;
            key_on();
            sB.state = 4;
        }
        break;
    case 4:
        if (sB.stopReq && sB.tracks[sB.cur].type == 2) {
            stop_now();
        }
        break;
    case 5:
        sB.mode = 1;
        sB.cur = -1;
        sB.state = 1;
        break;
    }
    if (sB.fadeLen > 0) {
        sB.fadeN--;
        sB.fade = (sB.fadeFrom * sB.fadeN + sB.fadeTo * (sB.fadeLen - sB.fadeN)) / sB.fadeLen;
        if (sB.fadeN <= 0) {
            sB.fadeLen = 0;
        }
        if (sB.fadeTo < sB.fadeFrom && sB.fade < 5 && sB.state == 4) {
            sB.stopReq = 1;
        }
    }
    if (sB.state == 4) {
        int v = (sB.optVol * sB.tracks[sB.cur].scale * sB.fade) >> 14;
        if (sB.stereo) {
            set_voice_vol(BGM_L, v, 0);
            set_voice_vol(BGM_R, 0, v);
        } else {
            set_voice_vol(BGM_L, v * 65 / 100, v * 65 / 100);
            set_voice_vol(BGM_R, v * 65 / 100, v * 65 / 100);
        }
    }
}

/* Between mixed pieces: refills the half voice 22 has left, stops at the silent end block. */
void Port_BgmService(void) {
    const Spu2Voice *v = &gSpu2[0].v[BGM_L];
    int h;
    if (sB.state != 4 || sB.tracks[sB.cur].type == 2) {
        return;
    }
    if (sB.final && v->nax == sB.finalAddr) {
        stop_now();
        return;
    }
    if (v->nax < BUF_L || v->nax >= BUF_L + 2 * HALF) {
        return;
    }
    h = v->nax >= BUF_L + HALF;
    if (h != sB.half) {
        if (!sB.final) {
            fill_half(sB.half);
        }
        sB.half = h;
    }
}

__attribute__((constructor(101))) static void bgm_state(void) {
    Port_StateAdd("bgm", &sB, sizeof sB, PS_HASH);
}
