/*
 * The SPU2 model (T32): the sound chip the IOP driver RSPU2DRV drives, enough of it for the game's voices.
 *
 *  - 2 MB sound RAM; two cores of 24 voices; output 48 kHz stereo, 16-bit.
 *  - PS-ADPCM: 16-byte blocks of 28 samples (byte 0 shift | filter << 4, byte 1 flags, 14 bytes of nibbles, low first).
 *    Flags: bit 2 loop start (the loop address takes the block's address unless it was written), bit 0 end (ENDX is
 *    set and the voice goes on at the loop address), bit 1 repeat (without it an end block silences the voice:
 *    envelope 0, the voice off; libspu2 then reports key status 3).
 *  - Pitch: 0x1000 = one sample per output sample (48 kHz), at most 0x3FFF; linear interpolation between the last two
 *    decoded samples (the hardware interpolates 4 samples with a Gaussian table: a difference in tone only).
 *  - ADSR as on the PS1/PS2 SPU: ADSR1 = attack mode:1 rate:7 | decay shift:4 | sustain level:4, ADSR2 = sustain
 *    mode:1 direction:1 0 rate:7 | release mode:1 shift:5; the envelope steps by (7 - (rate & 3)) << (11 - shift) or
 *    (-8 + (rate & 3)) << ..., every 1 << (shift - 11) samples, x4 slower above 0x6000 in exponential attack, scaled
 *    by the level in exponential decrease.
 *  - Volumes: fixed mode only (bit 15 clear: bits 14-0 are the signed volume / 2); the sweep modes, noise and reverb
 *    are not modelled. Core 0's output (under its master volume) enters core 1's mix at core 1's AVOL, and core 1's
 *    master volume makes the output, as libspu2's SpuInit routes them.
 *
 * Everything is integer arithmetic, and the state changes only through the calls below: what is heard, and what the
 * game reads back (key status), depends on the commands and on the number of samples mixed, never on the host clock.
 */
#include <stdio.h>
#include <string.h>
#include "spu2.h"
#include "plat_state.h"

Spu2Core gSpu2[2];
static uint8_t sRam[SPU2_RAM_SIZE];

uint8_t *Spu2_Ram(void) { return sRam; }

void Spu2_Reset(void) {
    memset(gSpu2, 0, sizeof gSpu2);
}

static const int8_t kF0[5] = { 0, 60, 115, 98, 122 };
static const int8_t kF1[5] = { 0, 0, -52, -55, -60 };

int Spu2_DecodeBlock(const uint8_t *blk, int16_t *out, int16_t *h1, int16_t *h2) {
    int shift = blk[0] & 0xF, filter = (blk[0] >> 4) & 7, i;
    int32_t f0, f1, p1 = *h1, p2 = *h2;
    if (shift > 12) {
        shift = 9; /* shifts 13-15 behave as 9 */
    }
    if (filter > 4) {
        filter = 4;
    }
    f0 = kF0[filter];
    f1 = kF1[filter];
    for (i = 0; i < 28; i++) {
        int32_t nib = (blk[2 + i / 2] >> ((i & 1) * 4)) & 0xF;
        int32_t s = (int32_t)(int16_t)(uint16_t)(nib << 12) >> shift;
        s += (p1 * f0 + p2 * f1 + 32) >> 6;
        if (s > 32767) {
            s = 32767;
        } else if (s < -32768) {
            s = -32768;
        }
        out[i] = (int16_t)s;
        p2 = p1;
        p1 = s;
    }
    *h1 = (int16_t)p1;
    *h2 = (int16_t)p2;
    return blk[1];
}

/* Decodes the block at the voice's current address; the previous block's last sample stays in d[1]. */
static void load_block(Spu2Voice *v) {
    const uint8_t *b;
    v->nax &= (SPU2_RAM_SIZE - 1) & ~15u;
    b = sRam + v->nax;
    v->d[1] = v->d[29];
    v->flags = (uint8_t)Spu2_DecodeBlock(b, v->d + 2, &v->hist1, &v->hist2);
    if ((v->flags & 4) && !v->lsaSet) {
        v->lsa = v->nax;
    }
}

void Spu2_KeyOn(int core, uint32_t mask) {
    int i;
    for (i = 0; i < SPU2_VOICES; i++) {
        Spu2Voice *v = &gSpu2[core].v[i];
        if (!(mask & (1u << i))) {
            continue;
        }
        v->nax = v->ssa;
        v->counter = 0;
        v->envx = 0;
        v->envCount = 0;
        v->phase = SPU2_ATTACK;
        v->keyed = 1;
        v->hist1 = v->hist2 = 0;
        v->d[29] = 0;
        load_block(v);
        gSpu2[core].endx &= ~(1u << i);
    }
}

void Spu2_KeyOff(int core, uint32_t mask) {
    int i;
    for (i = 0; i < SPU2_VOICES; i++) {
        Spu2Voice *v = &gSpu2[core].v[i];
        if (mask & (1u << i)) {
            v->keyed = 0;
            if (v->phase != SPU2_OFF) {
                v->phase = SPU2_RELEASE;
                v->envCount = 0;
            }
        }
    }
}

int Spu2_KeyStatus(int core, int voice) {
    const Spu2Voice *v = &gSpu2[core].v[voice];
    /* the driver tests ENVX != 0; just after a key on ENVX is still 0 for a sample or two, which the IOP's command
       latency hides on the PS2: here the attack counts as sounding */
    int sounding = v->phase != SPU2_OFF && (v->envx != 0 || v->phase == SPU2_ATTACK);
    return v->keyed ? (sounding ? 1 : 3) : (sounding ? 2 : 0);
}

/* One envelope step of rate `rate` (shift << 2 | step), increasing or decreasing, linear or exponential. */
static void env_step(Spu2Voice *v, int rate, int increase, int expo) {
    int shift = rate >> 2;
    int32_t step = increase ? 7 - (rate & 3) : -8 + (rate & 3);
    uint32_t cycles = 1u << (shift > 11 ? shift - 11 : 0);
    step = shift < 11 ? step * (1 << (11 - shift)) : step;
    if (expo && increase && v->envx > 0x6000) {
        cycles <<= 2;
    }
    if (expo && !increase) {
        step = step * v->envx >> 15;
    }
    if (++v->envCount < cycles) {
        return;
    }
    v->envCount = 0;
    v->envx += step;
    if (v->envx > 0x7FFF) {
        v->envx = 0x7FFF;
    } else if (v->envx < 0) {
        v->envx = 0;
    }
}

static void envelope(Spu2Voice *v) {
    switch (v->phase) {
    case SPU2_ATTACK:
        env_step(v, (v->adsr1 >> 8) & 0x7F, 1, v->adsr1 >> 15);
        if (v->envx >= 0x7FFF) {
            v->phase = SPU2_DECAY;
            v->envCount = 0;
        }
        break;
    case SPU2_DECAY:
        env_step(v, ((v->adsr1 >> 4) & 0xF) << 2, 0, 1);
        if (v->envx <= (((v->adsr1 & 0xF) + 1) << 11)) {
            v->phase = SPU2_SUSTAIN;
            v->envCount = 0;
        }
        break;
    case SPU2_SUSTAIN:
        env_step(v, (v->adsr2 >> 6) & 0x7F, !((v->adsr2 >> 14) & 1), v->adsr2 >> 15);
        break;
    case SPU2_RELEASE:
        env_step(v, (v->adsr2 & 0x1F) << 2, 0, (v->adsr2 >> 5) & 1);
        if (v->envx <= 0) {
            v->phase = SPU2_OFF;
        }
        break;
    }
}

/* A volume register as a factor of 0x8000 (fixed mode; a sweep register counts as its bits 14-0, see above). */
static int32_t vol_factor(uint16_t r) {
    return (int32_t)(int16_t)(uint16_t)(r << 1);
}

/* The next block, at the end of the current one (its flags decide). */
static void next_block(Spu2Core *c, int i) {
    Spu2Voice *v = &c->v[i];
    if (v->flags & 1) {
        c->endx |= 1u << i;
        v->nax = v->lsa;
        if (!(v->flags & 2)) {
            v->phase = SPU2_OFF;
            v->envx = 0;
            return;
        }
    } else {
        v->nax += 16;
    }
    load_block(v);
}

void Spu2_MixBus(int16_t *out, int32_t *bus, uint32_t musicVoices, int n) {
    int k, c, i;
    for (k = 0; k < n; k++) {
        int32_t sum[2] = { 0, 0 }, mus[2] = { 0, 0 };
        for (c = 0; c < 2; c++) { /* core 0, then core 1 with core 0's output at its AVOL */
            Spu2Core *core = &gSpu2[c];
            int32_t l = 0, r = 0, ml = 0, mr = 0;
            for (i = 0; i < SPU2_VOICES; i++) {
                Spu2Voice *v = &core->v[i];
                uint32_t idx, frac, pitch;
                int32_t s, sl, sr;
                if (v->phase == SPU2_OFF) {
                    continue;
                }
                idx = v->counter >> 12;
                frac = v->counter & 0xFFF;
                s = v->d[1 + idx] + (((v->d[2 + idx] - v->d[1 + idx]) * (int32_t)frac) >> 12);
                envelope(v);
                s = s * v->envx >> 15;
                sl = s * vol_factor(v->volL) >> 15;
                sr = s * vol_factor(v->volR) >> 15;
                l += sl;
                r += sr;
                if (c == 0 && (musicVoices >> i & 1u)) {
                    ml += sl;
                    mr += sr;
                }
                pitch = v->pitch > 0x3FFF ? 0x3FFF : v->pitch;
                v->counter += pitch;
                while (v->phase != SPU2_OFF && v->counter >= (28u << 12)) {
                    v->counter -= 28u << 12;
                    next_block(core, i);
                }
            }
            if (c == 1) {
                l += (int32_t)((int64_t)sum[0] * core->avolL >> 15);
                r += (int32_t)((int64_t)sum[1] * core->avolR >> 15);
                ml = (int32_t)((int64_t)mus[0] * core->avolL >> 15);
                mr = (int32_t)((int64_t)mus[1] * core->avolR >> 15);
            }
            sum[0] = (int32_t)((int64_t)l * vol_factor(core->mvolL) >> 15);
            sum[1] = (int32_t)((int64_t)r * vol_factor(core->mvolR) >> 15);
            mus[0] = (int32_t)((int64_t)ml * vol_factor(core->mvolL) >> 15);
            mus[1] = (int32_t)((int64_t)mr * vol_factor(core->mvolR) >> 15);
        }
        if (out != NULL) {
            for (c = 0; c < 2; c++) {
                out[2 * k + c] = (int16_t)(sum[c] > 32767 ? 32767 : sum[c] < -32768 ? -32768 : sum[c]);
            }
        }
        if (bus != NULL) {
            bus[4 * k] = sum[0];
            bus[4 * k + 1] = sum[1];
            bus[4 * k + 2] = mus[0];
            bus[4 * k + 3] = mus[1];
        }
    }
}

void Spu2_Mix(int16_t *out, int n) {
    Spu2_MixBus(out, NULL, 0, n);
}

/* ---- TTT_SELFTEST=spu ---- */
static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: spu self-test FAILED: %s\n", what);
        sFails++;
    }
}

/* A block of filter `filter`, shift `shift`, flags `flags` whose 28 nibbles are nib(i) = (i * 3 + seed) & 15. */
static void make_block(uint8_t *b, int shift, int filter, int flags, int seed) {
    int i;
    memset(b, 0, 16);
    b[0] = (uint8_t)(shift | filter << 4);
    b[1] = (uint8_t)flags;
    for (i = 0; i < 28; i++) {
        b[2 + i / 2] |= (uint8_t)(((i * 3 + seed) & 15) << ((i & 1) * 4));
    }
}

static void voice_setup(int core, int voice, uint32_t ssa, uint16_t pitch) {
    Spu2Voice *v = &gSpu2[core].v[voice];
    v->ssa = ssa;
    v->pitch = pitch;
    v->volL = v->volR = 0x3FFF;
    v->adsr1 = 0x000F; /* attack linear rate 0, decay 0, sustain level 0xF (no decay) */
    v->adsr2 = 0x0000; /* sustain linear increase rate 0, release linear shift 0 */
    (void)core;
    gSpu2[0].mvolL = gSpu2[0].mvolR = gSpu2[1].mvolL = gSpu2[1].mvolR = 0x3FFF;
    gSpu2[1].avolL = gSpu2[1].avolR = 0x7FFF;
}

int Spu2_SelfTest(void) {
    uint8_t b[16];
    int16_t out[28], h1 = 0, h2 = 0, pcm[2 * 200];
    uint32_t hash[2];
    int i, run;

    /* decoder: filter 0, shift 12: a sample is its nibble, sign-extended */
    make_block(b, 12, 0, 0x3, 5);
    expect(Spu2_DecodeBlock(b, out, &h1, &h2) == 3, "flags returned");
    for (i = 0; i < 28; i++) {
        int n = (i * 3 + 5) & 15;
        if (out[i] != (n >= 8 ? n - 16 : n)) {
            expect(0, "filter 0 shift 12 decodes the nibbles");
            break;
        }
    }
    expect(h1 == out[27] && h2 == out[26], "history keeps the last two samples");
    /* shift 0: nibble << 12; filter 1 adds 60/64 of the previous sample (rounded) */
    make_block(b, 0, 1, 0, 1); /* nibbles 1, 4, 7, 10 (-6), ... */
    h1 = 1000;
    h2 = 0;
    Spu2_DecodeBlock(b, out, &h1, &h2);
    expect(out[0] == 4096 + ((1000 * 60 + 32) >> 6), "filter 1, first sample");
    expect(out[1] == 16384 + ((out[0] * 60 + 32) >> 6), "filter 1, second sample");
    expect(out[3] == -24576 + ((out[2] * 60 + 32) >> 6), "filter 1, negative nibble");
    /* filter 2 and clamping */
    make_block(b, 0, 2, 0, 7); /* nibble 7: +28672 */
    h1 = 30000;
    h2 = -30000;
    Spu2_DecodeBlock(b, out, &h1, &h2);
    expect(out[0] == 32767, "clamped to +32767");
    /* shift 13-15 act as 9 */
    make_block(b, 15, 0, 0, 1);
    h1 = h2 = 0;
    Spu2_DecodeBlock(b, out, &h1, &h2);
    expect(out[0] == 8, "shift 15 behaves as 9 (4096 >> 9)");

    /* voices: a one-block sample with end and no repeat: 28 samples at pitch 0x1000, then off, status 3, ENDX */
    Spu2_Reset();
    make_block(Spu2_Ram() + 0x5010, 12, 0, 0, 3);
    make_block(Spu2_Ram() + 0x5020, 12, 0, 0x1, 3);
    voice_setup(1, 0, 0x5010, 0x1000);
    Spu2_KeyOn(1, 1);
    expect(Spu2_KeyStatus(1, 0) == 1, "key on: status 1");
    Spu2_Mix(NULL, 55);
    expect(Spu2_KeyStatus(1, 0) == 1 && !(gSpu2[1].endx & 1), "still playing after 55 samples");
    Spu2_Mix(NULL, 1);
    expect(Spu2_KeyStatus(1, 0) == 3 && (gSpu2[1].endx & 1), "end block without repeat: off, status 3, ENDX");
    Spu2_KeyOff(1, 1);
    expect(Spu2_KeyStatus(1, 0) == 0, "key off after the end: status 0");
    /* pitch 0x2000 goes through the two blocks in 28 samples */
    Spu2_KeyOn(1, 1);
    gSpu2[1].v[0].pitch = 0x2000;
    Spu2_Mix(NULL, 27);
    expect(Spu2_KeyStatus(1, 0) == 1, "pitch 0x2000: playing after 27 samples");
    Spu2_Mix(NULL, 1);
    expect(Spu2_KeyStatus(1, 0) == 3, "pitch 0x2000: ended after 28 samples");
    /* a loop: loop start on the second block, end + repeat: plays on, loop address = 0x5030 */
    make_block(Spu2_Ram() + 0x5030, 12, 0, 0x4, 9);
    make_block(Spu2_Ram() + 0x5040, 12, 0, 0x3, 9);
    voice_setup(1, 1, 0x5030, 0x1000);
    Spu2_KeyOn(1, 2);
    Spu2_Mix(NULL, 28 * 7);
    expect(Spu2_KeyStatus(1, 1) == 1 && gSpu2[1].v[1].lsa == 0x5030 && (gSpu2[1].endx & 2),
           "loop flags: repeats from the loop-start block");
    /* ADSR: attack linear rate 0 is full in 3 samples; release linear shift 0 (-0x4000 a sample) empties it in 2 */
    expect(gSpu2[1].v[1].envx == 0x7FFF, "attack reached the top");
    Spu2_KeyOff(1, 2);
    expect(Spu2_KeyStatus(1, 1) == 2, "key off: releasing, status 2");
    Spu2_Mix(NULL, 1);
    expect(Spu2_KeyStatus(1, 1) == 2 && gSpu2[1].v[1].envx == 0x3FFF, "release steps of 0x4000");
    Spu2_Mix(NULL, 1);
    expect(Spu2_KeyStatus(1, 1) == 0, "release finished: status 0");
    /* exponential decay to the sustain level */
    Spu2_Reset();
    make_block(Spu2_Ram() + 0x6000, 12, 0, 0x6, 2);
    voice_setup(0, 5, 0x6000, 0x1000);
    gSpu2[0].v[5].adsr1 = 0x00A3; /* decay shift 10, sustain level 3 (0x2000) */
    gSpu2[0].v[5].adsr2 = 0x1FC0; /* sustain: linear increase, rate 0x7F (stays) */
    Spu2_KeyOn(0, 1u << 5);
    Spu2_Mix(NULL, 48000);
    expect(gSpu2[0].v[5].phase == SPU2_SUSTAIN && gSpu2[0].v[5].envx <= 0x2000 && gSpu2[0].v[5].envx > 0x2000 - 16,
           "exponential decay stops at the sustain level");

    /* mixing: a looping sample on both cores is heard, the same twice */
    for (run = 0; run < 2; run++) {
        uint32_t h = 2166136261u;
        int nz = 0;
        Spu2_Reset();
        make_block(Spu2_Ram() + 0x7000, 4, 1, 0x4, 11);
        make_block(Spu2_Ram() + 0x7010, 4, 2, 0x3, 6);
        voice_setup(0, 22, 0x7000, 0x0EB3);
        voice_setup(1, 3, 0x7000, 0x1800);
        gSpu2[1].v[3].volR = 0x1000;
        Spu2_KeyOn(0, 1u << 22);
        Spu2_KeyOn(1, 1u << 3);
        Spu2_Mix(pcm, 200);
        for (i = 0; i < 400; i++) {
            nz += pcm[i] != 0;
            h = (h ^ (uint16_t)pcm[i]) * 16777619u;
        }
        expect(nz > 300, "the mix is not silent");
        hash[run] = h;
    }
    expect(hash[0] == hash[1], "the mix is deterministic");
    Spu2_Reset();
    printf(sFails ? "ttt: spu self-test FAILED (%d)\n" : "ttt: spu self-test OK\n", sFails);
    return sFails;
}

__attribute__((constructor(101))) static void spu2_state(void) {
    Port_StateAdd("spu2.cores", gSpu2, sizeof gSpu2, PS_HASH);
    Port_StateAdd("spu2.ram", sRam, sizeof sRam, PS_HASH);
}
