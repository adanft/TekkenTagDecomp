/*
 * The sound (T32): RSPU2DRV's libspu2 commands on the SPU2 model (spu2.c), and its output.
 *
 * plat_file.c receives every sceSdRemote command; the sound ones come here (docs/port/sound.md):
 *   0x0001 SpuInit, 0x0002 SpuSetCore, 0x0005 key off, 0x7128 SpuSetCommonAttr, 0x7240 SpuSetVoiceAttr,
 *   0x7440 SpuSetKeyOnWithAttr, 0x6418 SpuGetAllKeysStatus; the BGM engine's 0x2000 (with plat_file.c), 0x205E
 *   tracks and 0x7F0F sound queue go to plat_bgm.c.
 *
 * Time: the SPU2 advances only at VBlanks (Port_SndVBlank, from plat_hw.c on the game's thread): 48000 * 1001 / 60000
 * = 800.8 stereo frames per VBlank, exactly (800 or 801, by an integer accumulator). What the game reads back (key
 * status) is thus the same on every run, headless or in real time, whatever the host's audio does.
 *
 * Output: the frames go to
 *   - TTT_WAV=<file>: a 48 kHz 16-bit stereo WAV of the whole run (headless runs included);
 *   - the window's SDL3 audio stream (plat_window.c), through a ring the window thread drains: the game's thread only
 *     writes, the window thread only reads, with 32-bit atomic indices. TTT_AUDIO=0: no audio device.
 * Both get the host's volumes (T40: audio.volume master, audio.music for the music voices, audio.effects for the rest,
 * audio.mute), applied to the mixed output only: the SPU2 model and what the game reads back never see them. While the
 * game speed is not 1x (plat_hw.c) the window plays nothing; TTT_WAV still gets every frame, at the game's pace.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_addr.h"
#include "port_host.h"
#include "spu2.h"
#include "plat_state.h"
#include "plat_settings.h"

static int sCore; /* SpuSetCore */
static uint32_t sHalfLines; /* the BGM timer's phase in the field (half H-lines) */
__attribute__((constructor(101))) static void snd_state(void) {
    Port_StateAdd("snd.core", &sCore, sizeof sCore, PS_HASH);
    Port_StateAdd("snd.halfLines", &sHalfLines, sizeof sHalfLines, PS_HASH);
}

void Port_BgmInit(void); /* plat_bgm.c */
void Port_BgmTrack(const uint32_t *a);
void Port_BgmQueue(const uint8_t *q);
void Port_BgmTick(void);
void Port_BgmService(void);

/* ---- libspu2 on the model ---- */
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* libspu2 volume (`vol`, `mode` 0 = direct) as a register value: direct volumes are 15 bits. */
static uint16_t vol_reg(uint16_t vol, uint16_t mode) {
    return mode == 0 ? (uint16_t)(vol & 0x7FFF) : (uint16_t)(0x8000 | (vol & 0x7F));
}

/* SpuSetVoiceAttr: the SpuVoiceAttr at `a` (include/sound.h) on core sCore; with `keyOn`, the voices are keyed on. */
static void voice_attr(const uint8_t *a, int keyOn) {
    uint32_t voices = rd32(a) & 0xFFFFFF, mask = rd32(a + 4);
    int i;
    for (i = 0; i < SPU2_VOICES; i++) {
        Spu2Voice *v = &gSpu2[sCore].v[i];
        if (!(voices & (1u << i))) {
            continue;
        }
        if (mask & 0x01) {
            v->volL = vol_reg(rd16(a + 0x08), (mask & 0x04) ? rd16(a + 0x0C) : 0);
        }
        if (mask & 0x02) {
            v->volR = vol_reg(rd16(a + 0x0A), (mask & 0x08) ? rd16(a + 0x0E) : 0);
        }
        if (mask & 0x10) {
            v->pitch = rd16(a + 0x14);
        }
        if (mask & 0x80) {
            v->ssa = rd32(a + 0x1C) & (SPU2_RAM_SIZE - 1) & ~15u;
        }
        if (mask & 0x10000) {
            v->lsa = rd32(a + 0x20) & (SPU2_RAM_SIZE - 1) & ~15u;
            v->lsaSet = 1;
        }
        if (mask & 0x20000) {
            v->adsr1 = rd16(a + 0x3A);
        }
        if (mask & 0x40000) {
            v->adsr2 = rd16(a + 0x3C);
        }
    }
    if (keyOn) {
        Spu2_KeyOn(sCore, voices);
    }
}

/* SpuSetCommonAttr: the master volume; libspu2 writes the CD input volume to 0x790 + core * 0x28, which for core 0
   is core 1's AVOL (core 0's output in core 1's mix) and for core 1 no register. External input and reverb: not
   modelled (the game sets them to 0). */
static void common_attr(const uint8_t *a) {
    uint32_t mask = rd32(a);
    if (sCore == 0 && (mask & 0x40)) {
        gSpu2[1].avolL = (int16_t)rd16(a + 0x10);
    }
    if (sCore == 0 && (mask & 0x80)) {
        gSpu2[1].avolR = (int16_t)rd16(a + 0x12);
    }
    if (mask & 1) {
        gSpu2[sCore].mvolL = vol_reg(rd16(a + 4), (mask & 4) ? rd16(a + 8) : 0);
    }
    if (mask & 2) {
        gSpu2[sCore].mvolR = vol_reg(rd16(a + 6), (mask & 8) ? rd16(a + 10) : 0);
    }
}

/* SpuInit (IOP 0x4020): every voice volume 0, pitch 0x3FFF, ADSR 0, start 0x5000 where a block of 07 bytes and a
   silent block are written; master volumes 0, core 1's AVOL 0x7FFF; all keys off; core 0. */
static void spu_init(void) {
    int c, i;
    Spu2_Reset();
    for (c = 0; c < 2; c++) {
        for (i = 0; i < SPU2_VOICES; i++) {
            gSpu2[c].v[i].pitch = 0x3FFF;
            gSpu2[c].v[i].ssa = 0x5000;
        }
    }
    memset(Spu2_Ram() + 0x5000, 0x07, 16);
    memset(Spu2_Ram() + 0x5010, 0, 16);
    gSpu2[1].avolL = gSpu2[1].avolR = 0x7FFF;
    sCore = 0;
}

/* Command 0x2000, the driver's start (plat_file.c finds the disc): SpuInit again, core 0's master volume 0x3FFF,
   the BGM stream voices set up. */
void Port_SndDriverStart(void) {
    spu_init();
    gSpu2[0].mvolL = gSpu2[0].mvolR = 0x3FFF;
    Port_BgmInit();
}

uint32_t Port_VBlankCount(void); /* plat_hw.c */

/* A sound command (anything that is not a file / movie command); its result. TTT_SND_TRACE=1 prints them. */
int Port_SndCommand(int cmd, const uint32_t *a) {
    static int trace = -1;
    if (trace < 0) {
        const char *t = getenv("TTT_SND_TRACE");
        trace = t != NULL && atoi(t) != 0;
    }
    if (trace && cmd != 0x0002) {
        const uint8_t *p = (cmd & 0xF000) == 0x7000 && a[0] != 0 ? Port_Ptr(a[0]) : NULL;
        printf("ttt: snd %u: %04x core %d %08x %08x %08x", (unsigned)Port_VBlankCount(), (unsigned)cmd, sCore,
               (unsigned)a[0], (unsigned)a[1], (unsigned)a[2]);
        if (p != NULL) {
            int i;
            printf(" |");
            for (i = 0; i < 0x40; i += 4) {
                printf(" %08x", (unsigned)rd32(p + i));
            }
        }
        printf("\n");
    }
    switch (cmd) {
    case 0x0001:
        spu_init();
        return 0;
    case 0x0002: {
        int prev = sCore;
        sCore = a[0] & 1;
        return prev;
    }
    case 0x0005: /* SpuSetCore(1), SpuSetKey(SPU_OFF, mask); the core stays 1 */
        sCore = 1;
        Spu2_KeyOff(sCore, a[1] & 0xFFFFFF);
        return 0;
    case 0x205E:
        Port_BgmTrack(a);
        return 0;
    case 0x7F0F:
        if (a[0] != 0) {
            Port_BgmQueue(Port_Ptr(a[0]));
        }
        return 0;
    case 0x7128:
        if (a[0] != 0) {
            common_attr(Port_Ptr(a[0]));
        }
        return 0;
    case 0x7240:
    case 0x7440:
        if (a[0] != 0) {
            voice_attr(Port_Ptr(a[0]), cmd == 0x7440);
        }
        return 0;
    default:
        return 0;
    }
}

/* A get command (0x6xxx): 0x40 bytes into `out`. */
void Port_SndGet(int cmd, uint8_t *out) {
    int i;
    memset(out, 0, 0x40);
    if (cmd == 0x6418) { /* SpuGetAllKeysStatus */
        for (i = 0; i < SPU2_VOICES; i++) {
            out[i] = (uint8_t)Spu2_KeyStatus(sCore, i);
        }
    }
}

/* ---- output ---- */
#define RING_FRAMES 16384u /* a power of two: about 0.34 s */
static int16_t sRing[RING_FRAMES * 2];
static uint32_t sRingW, sRingR; /* frames written / read (wrapping): written by the game's thread / the window's */
static int sRingOn;              /* the window opened an audio stream */
static FILE *sWav;
static uint32_t sWavFrames;
static int sWavTried;

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* The 44-byte header of a 48 kHz 16-bit stereo PCM WAV holding sWavFrames frames. */
static void wav_header(void) {
    uint8_t h[44];
    uint32_t data = sWavFrames * 4;
    memcpy(h, "RIFF", 4);
    put32(h + 4, data + 36);
    memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16);                    /* fmt chunk size */
    put32(h + 20, 1u | 2u << 16);         /* PCM, 2 channels */
    put32(h + 24, SPU2_RATE);
    put32(h + 28, SPU2_RATE * 4);         /* bytes per second */
    put32(h + 32, 4u | 16u << 16);        /* 4 bytes per frame, 16 bits */
    memcpy(h + 36, "data", 4);
    put32(h + 40, data);
    fseek(sWav, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, sWav);
    fseek(sWav, 0, SEEK_END);
}

static void wav_write(const int16_t *pcm, int n) {
    if (!sWavTried) {
        const char *p = getenv("TTT_WAV");
        sWavTried = 1;
        if (p != NULL && *p) {
            sWav = fopen(p, "wb");
            if (sWav == NULL) {
                printf("ttt: TTT_WAV: cannot write %s\n", p);
            } else {
                wav_header();
            }
        }
    }
    if (sWav == NULL) {
        return;
    }
    fwrite(pcm, 4, (size_t)n, sWav); /* the host is little-endian, like the WAV format */
    sWavFrames += (uint32_t)n;
}

/* The end of the run (plat_hw.c's TTT_FRAMES report): the WAV's sizes. */
void Port_SndEnd(void) {
    if (sWav != NULL) {
        wav_header();
        fclose(sWav);
        sWav = NULL;
        printf("ttt: sound: %u frames written to TTT_WAV\n", (unsigned)sWavFrames);
    }
}

static void ring_write(const int16_t *pcm, int n) {
    uint32_t w = sRingW, r = __atomic_load_n(&sRingR, __ATOMIC_ACQUIRE);
    int k;
    if (!__atomic_load_n(&sRingOn, __ATOMIC_ACQUIRE)) {
        return;
    }
    for (k = 0; k < n && w - r < RING_FRAMES; k++, w++) { /* full: the rest is dropped (the device is behind) */
        sRing[(w & (RING_FRAMES - 1)) * 2] = pcm[2 * k];
        sRing[(w & (RING_FRAMES - 1)) * 2 + 1] = pcm[2 * k + 1];
    }
    __atomic_store_n(&sRingW, w, __ATOMIC_RELEASE);
}

/* The window thread: up to `max` frames from the ring into `out`; how many. */
int Port_SndRingRead(int16_t *out, int max) {
    uint32_t r = sRingR, w = __atomic_load_n(&sRingW, __ATOMIC_ACQUIRE);
    int k = 0;
    for (; k < max && r != w; k++, r++) {
        out[2 * k] = sRing[(r & (RING_FRAMES - 1)) * 2];
        out[2 * k + 1] = sRing[(r & (RING_FRAMES - 1)) * 2 + 1];
    }
    __atomic_store_n(&sRingR, r, __ATOMIC_RELEASE);
    return k;
}

void Port_SndRingEnable(int on) {
    __atomic_store_n(&sRingOn, on, __ATOMIC_RELEASE);
}

/* The host's volumes (percent; mute is a master of 0): set by the window (Port_SndSetVolumes, its thread) or read from
   the settings at the first VBlank (headless runs). Read by the game's thread at each VBlank, one 32-bit word. */
static int sVolumes = -1; /* master | music << 8 | effects << 16 */
void Port_SndSetVolumes(int master, int music, int effects) {
    __atomic_store_n(&sVolumes, (master & 0xFF) | (music & 0xFF) << 8 | (effects & 0xFF) << 16, __ATOMIC_RELEASE);
}
static int volumes(void) {
    int v = __atomic_load_n(&sVolumes, __ATOMIC_ACQUIRE);
    if (v < 0) {
        Port_SndSetVolumes(Settings_GetInt("audio.mute") ? 0 : Settings_GetInt("audio.volume"),
                           Settings_GetInt("audio.music"), Settings_GetInt("audio.effects"));
        v = __atomic_load_n(&sVolumes, __ATOMIC_ACQUIRE);
    }
    return v;
}

/* The mixed buses (Spu2_MixBus) at the volumes, into 16-bit frames: (effects * fx + music * mu) * master. */
static void apply_volumes(int16_t *pcm, const int32_t *bus, int n, int v) {
    int64_t master = v & 0xFF, mu = (v >> 8) & 0xFF, fx = (v >> 16) & 0xFF;
    int k;
    for (k = 0; k < 2 * n; k++) {
        int32_t all = bus[2 * k - (k & 1)], mus = bus[2 * k - (k & 1) + 2];
        int64_t s = ((int64_t)(all - mus) * fx + (int64_t)mus * mu) * master / 10000;
        pcm[k] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
}

uint32_t Port_BgmVoices(void); /* plat_bgm.c */
int Port_Speed(void);          /* plat_hw.c: the game speed in percent (100 headless) */

/* VBlank `n` (1, 2, ...): the frames of the field that ends here, 800.8 a VBlank. The BGM engine's timer ticks every
   66 H-lines (262.5 a field: 3 or 4 ticks), spread evenly over the field's frames; between pieces of at most 128
   frames the engine refills its buffers. */
void Port_SndVBlank(uint32_t n) {
    static int16_t pcm[2 * 801];
    static int32_t bus[4 * 801];
    int frames = (int)((uint64_t)n * 4004u / 5u - (uint64_t)(n - 1) * 4004u / 5u);
    int ticks = 0, t, done = 0, v = volumes();
    for (sHalfLines += 525; sHalfLines >= 132; sHalfLines -= 132) {
        ticks++;
    }
    for (t = 0; t < ticks; t++) {
        int end = frames * (t + 1) / ticks;
        Port_BgmTick();
        while (done < end) {
            int k = end - done > 128 ? 128 : end - done;
            Spu2_MixBus(pcm + 2 * done, bus + 4 * done, Port_BgmVoices(), k);
            done += k;
            Port_BgmService();
        }
    }
    if (!gPortResim) { /* frames run again (TTT_SYNCTEST) were heard already */
        if (v != (100 | 100 << 8 | 100 << 16)) {
            apply_volumes(pcm, bus, frames, v);
        }
        wav_write(pcm, frames);
        if (Port_Speed() == 100) {
            ring_write(pcm, frames);
        }
    }
}
