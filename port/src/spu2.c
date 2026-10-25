/*
 * The SPU2 model (T32): the sound chip the IOP driver RSPU2DRV drives, enough of it for the game's voices.
 *
 *  - 2 MB sound RAM; two cores of 24 voices; output 48 kHz stereo, 16-bit.
 *  - PS-ADPCM: 16-byte blocks of 28 samples (byte 0 shift | filter << 4, byte 1 flags, 14 bytes of nibbles, low first).
 *    Flags: bit 2 loop start (the loop address takes the block's address unless it was written), bit 0 end (ENDX is
 *    set and the voice goes on at the loop address), bit 1 repeat (without it an end block silences the voice:
 *    envelope 0, the voice off; libspu2 then reports key status 3).
 *  - Pitch: 0x1000 = one sample per output sample (48 kHz), at most 0x3FFF. The output sample is the hardware's 4-point
 *    Gaussian interpolation of the 4 most recent decoded samples (oldest, older, old, new; new = the sample the pitch
 *    counter is on), bits 4-11 of the counter as the index i into the 512-entry table:
 *    (g[0xFF - i] * oldest >> 15) + (g[0x1FF - i] * older >> 15) + (g[0x100 + i] * old >> 15) + (g[i] * new >> 15).
 *    Each phase's four weights sum to 0x7F7F-0x7F81, so the result stays within 16 bits. The samples before a key on
 *    count as 0.
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
/* The previous block's samples 25 and 26 (d[27], d[28] before the next block's decode), per voice: with d[1] (its
   sample 27) they are the three samples the interpolation reaches back to at the start of a block. They
   change only what is heard, never anything the game reads back, so they are saved with the state (rollback) and
   left out of its hash (docs/port/sound.md). */
static int16_t sTail[2][SPU2_VOICES][2];

uint8_t *Spu2_Ram(void) { return sRam; }

void Spu2_Reset(void) {
    memset(gSpu2, 0, sizeof gSpu2);
    memset(sTail, 0, sizeof sTail);
}

/* The SPU's Gaussian interpolation table: hardware behaviour, the values as published in Martin Korth's "psx-spx"
   (nocash) SPU documentation, also used by PCSX2's SPU2 and DuckStation (the PS2's SPU2 cores keep the PS1 SPU's
   voice pipeline). tests/test_port_sound.py checks the table against the published one's SHA-1. */
static const int16_t kGauss[512] = {
    -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001,
    -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003,
    0x0003, 0x0004, 0x0004, 0x0005, 0x0005, 0x0006, 0x0007, 0x0007,
    0x0008, 0x0009, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
    0x000F, 0x0010, 0x0011, 0x0012, 0x0013, 0x0015, 0x0016, 0x0018,
    0x0019, 0x001B, 0x001C, 0x001E, 0x0020, 0x0021, 0x0023, 0x0025,
    0x0027, 0x0029, 0x002C, 0x002E, 0x0030, 0x0033, 0x0035, 0x0038,
    0x003A, 0x003D, 0x0040, 0x0043, 0x0046, 0x0049, 0x004D, 0x0050,
    0x0054, 0x0057, 0x005B, 0x005F, 0x0063, 0x0067, 0x006B, 0x006F,
    0x0074, 0x0078, 0x007D, 0x0082, 0x0087, 0x008C, 0x0091, 0x0096,
    0x009C, 0x00A1, 0x00A7, 0x00AD, 0x00B3, 0x00BA, 0x00C0, 0x00C7,
    0x00CD, 0x00D4, 0x00DB, 0x00E3, 0x00EA, 0x00F2, 0x00FA, 0x0101,
    0x010A, 0x0112, 0x011B, 0x0123, 0x012C, 0x0135, 0x013F, 0x0148,
    0x0152, 0x015C, 0x0166, 0x0171, 0x017B, 0x0186, 0x0191, 0x019C,
    0x01A8, 0x01B4, 0x01C0, 0x01CC, 0x01D9, 0x01E5, 0x01F2, 0x0200,
    0x020D, 0x021B, 0x0229, 0x0237, 0x0246, 0x0255, 0x0264, 0x0273,
    0x0283, 0x0293, 0x02A3, 0x02B4, 0x02C4, 0x02D6, 0x02E7, 0x02F9,
    0x030B, 0x031D, 0x0330, 0x0343, 0x0356, 0x036A, 0x037E, 0x0392,
    0x03A7, 0x03BC, 0x03D1, 0x03E7, 0x03FC, 0x0413, 0x042A, 0x0441,
    0x0458, 0x0470, 0x0488, 0x04A0, 0x04B9, 0x04D2, 0x04EC, 0x0506,
    0x0520, 0x053B, 0x0556, 0x0572, 0x058E, 0x05AA, 0x05C7, 0x05E4,
    0x0601, 0x061F, 0x063E, 0x065C, 0x067C, 0x069B, 0x06BB, 0x06DC,
    0x06FD, 0x071E, 0x0740, 0x0762, 0x0784, 0x07A7, 0x07CB, 0x07EF,
    0x0813, 0x0838, 0x085D, 0x0883, 0x08A9, 0x08D0, 0x08F7, 0x091E,
    0x0946, 0x096F, 0x0998, 0x09C1, 0x09EB, 0x0A16, 0x0A40, 0x0A6C,
    0x0A98, 0x0AC4, 0x0AF1, 0x0B1E, 0x0B4C, 0x0B7A, 0x0BA9, 0x0BD8,
    0x0C07, 0x0C38, 0x0C68, 0x0C99, 0x0CCB, 0x0CFD, 0x0D30, 0x0D63,
    0x0D97, 0x0DCB, 0x0E00, 0x0E35, 0x0E6B, 0x0EA1, 0x0ED7, 0x0F0F,
    0x0F46, 0x0F7F, 0x0FB7, 0x0FF1, 0x102A, 0x1065, 0x109F, 0x10DB,
    0x1116, 0x1153, 0x118F, 0x11CD, 0x120B, 0x1249, 0x1288, 0x12C7,
    0x1307, 0x1347, 0x1388, 0x13C9, 0x140B, 0x144D, 0x1490, 0x14D4,
    0x1517, 0x155C, 0x15A0, 0x15E6, 0x162C, 0x1672, 0x16B9, 0x1700,
    0x1747, 0x1790, 0x17D8, 0x1821, 0x186B, 0x18B5, 0x1900, 0x194B,
    0x1996, 0x19E2, 0x1A2E, 0x1A7B, 0x1AC8, 0x1B16, 0x1B64, 0x1BB3,
    0x1C02, 0x1C51, 0x1CA1, 0x1CF1, 0x1D42, 0x1D93, 0x1DE5, 0x1E37,
    0x1E89, 0x1EDC, 0x1F2F, 0x1F82, 0x1FD6, 0x202A, 0x207F, 0x20D4,
    0x2129, 0x217F, 0x21D5, 0x222C, 0x2282, 0x22DA, 0x2331, 0x2389,
    0x23E1, 0x2439, 0x2492, 0x24EB, 0x2545, 0x259E, 0x25F8, 0x2653,
    0x26AD, 0x2708, 0x2763, 0x27BE, 0x281A, 0x2876, 0x28D2, 0x292E,
    0x298B, 0x29E7, 0x2A44, 0x2AA1, 0x2AFF, 0x2B5C, 0x2BBA, 0x2C18,
    0x2C76, 0x2CD4, 0x2D33, 0x2D91, 0x2DF0, 0x2E4F, 0x2EAE, 0x2F0D,
    0x2F6C, 0x2FCC, 0x302B, 0x308B, 0x30EA, 0x314A, 0x31AA, 0x3209,
    0x3269, 0x32C9, 0x3329, 0x3389, 0x33E9, 0x3449, 0x34A9, 0x3509,
    0x3569, 0x35C9, 0x3629, 0x3689, 0x36E8, 0x3748, 0x37A8, 0x3807,
    0x3867, 0x38C6, 0x3926, 0x3985, 0x39E4, 0x3A43, 0x3AA2, 0x3B00,
    0x3B5F, 0x3BBD, 0x3C1B, 0x3C79, 0x3CD7, 0x3D35, 0x3D92, 0x3DEF,
    0x3E4C, 0x3EA9, 0x3F05, 0x3F62, 0x3FBD, 0x4019, 0x4074, 0x40D0,
    0x412A, 0x4185, 0x41DF, 0x4239, 0x4292, 0x42EB, 0x4344, 0x439C,
    0x43F4, 0x444C, 0x44A3, 0x44FA, 0x4550, 0x45A6, 0x45FC, 0x4651,
    0x46A6, 0x46FA, 0x474E, 0x47A1, 0x47F4, 0x4846, 0x4898, 0x48E9,
    0x493A, 0x498A, 0x49D9, 0x4A29, 0x4A77, 0x4AC5, 0x4B13, 0x4B5F,
    0x4BAC, 0x4BF7, 0x4C42, 0x4C8D, 0x4CD7, 0x4D20, 0x4D68, 0x4DB0,
    0x4DF7, 0x4E3E, 0x4E84, 0x4EC9, 0x4F0E, 0x4F52, 0x4F95, 0x4FD7,
    0x5019, 0x505A, 0x509A, 0x50DA, 0x5118, 0x5156, 0x5194, 0x51D0,
    0x520C, 0x5247, 0x5281, 0x52BA, 0x52F3, 0x532A, 0x5361, 0x5397,
    0x53CC, 0x5401, 0x5434, 0x5467, 0x5499, 0x54CA, 0x54FA, 0x5529,
    0x5558, 0x5585, 0x55B2, 0x55DE, 0x5609, 0x5632, 0x565B, 0x5684,
    0x56AB, 0x56D1, 0x56F6, 0x571B, 0x573E, 0x5761, 0x5782, 0x57A3,
    0x57C3, 0x57E2, 0x57FF, 0x581C, 0x5838, 0x5853, 0x586D, 0x5886,
    0x589E, 0x58B5, 0x58CB, 0x58E0, 0x58F4, 0x5907, 0x5919, 0x592A,
    0x593A, 0x5949, 0x5958, 0x5965, 0x5971, 0x597C, 0x5986, 0x598F,
    0x5997, 0x599E, 0x59A4, 0x59A9, 0x59AD, 0x59B0, 0x59B2, 0x59B3,
};

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

static int16_t *tail_of(const Spu2Voice *v) {
    int c = v >= gSpu2[1].v && v < gSpu2[1].v + SPU2_VOICES;
    return sTail[c][v - gSpu2[c].v];
}

/* Decodes the block at the voice's current address; the previous block's last three samples stay in the tail and d[1]. */
static void load_block(Spu2Voice *v) {
    const uint8_t *b;
    int16_t *t = tail_of(v);
    v->nax &= (SPU2_RAM_SIZE - 1) & ~15u;
    b = sRam + v->nax;
    t[0] = v->d[27];
    t[1] = v->d[28];
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
        tail_of(v)[0] = tail_of(v)[1] = 0; /* nothing before the key on */
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

/* The 4-point Gaussian interpolation of four consecutive samples, oldest first, at index i (bits 4-11 of the pitch
   counter). */
static int32_t gauss4(int32_t oldest, int32_t older, int32_t old, int32_t cur, uint32_t i) {
    return (kGauss[0xFF - i] * oldest >> 15) + (kGauss[0x1FF - i] * older >> 15) + (kGauss[0x100 + i] * old >> 15) +
           (kGauss[i] * cur >> 15);
}

int32_t Spu2_Gauss(const int16_t *w, uint32_t i) {
    return gauss4(w[0], w[1], w[2], w[3], i & 0xFF);
}

/* The voice's output sample before its envelope: the current sample d[2 + idx] (new) and the three before it, from
   the tail at the start of a block. */
static int32_t voice_sample(const Spu2Voice *v, const int16_t *tail) {
    uint32_t idx = v->counter >> 12, i = (v->counter >> 4) & 0xFF;
    int32_t oldest = idx >= 2 ? v->d[idx - 1] : tail[idx], older = idx >= 1 ? v->d[idx] : tail[1];
    return gauss4(oldest, older, v->d[idx + 1], v->d[idx + 2], i);
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
                uint32_t pitch;
                int32_t s, sl, sr;
                if (v->phase == SPU2_OFF) {
                    continue;
                }
                s = voice_sample(v, sTail[c][i]);
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
    static const int16_t kWin[6][4] = { { 0, 32767, 0, 0 }, { 0, 0, 32767, 0 }, { 1000, -2000, 3000, -4000 },
                                        { 32767, 32767, 32767, 32767 }, { -32768, 12345, -777, 30000 },
                                        { 100, 200, 300, 400 } };
    static const uint32_t kPhase[6] = { 0x00, 0x00, 0x80, 0xFF, 0x3C, 0xA5 };
    static const int32_t kWant[6] = { 22962, 4870, 452, 32637, 5890, 262 }; /* worked out apart from this code */

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
    /* Gaussian interpolation: reference values, and a digest of all 256 phases of those windows (printed for
       tests/test_port_sound.py, which recomputes it from the published table) */
    {
        uint32_t h = 2166136261u, ph;
        for (i = 0; i < 6; i++) {
            expect(Spu2_Gauss(kWin[i], kPhase[i]) == kWant[i], "Gaussian interpolation: reference values");
            for (ph = 0; ph < 256; ph++) {
                h = (h ^ (uint32_t)Spu2_Gauss(kWin[i], ph)) * 16777619u;
            }
        }
        printf("ttt: spu gauss digest 0x%08x\n", h);
    }
    /* a voice's output follows the decoded stream through the interpolation, across block boundaries (the tail), with
       zeros before the key on: three blocks at pitch 0x0C35, envelope held at the top */
    {
        int16_t st[3 + 84], hh1 = 0, hh2 = 0, frame[2];
        int k, bad = 0;
        Spu2_Reset();
        make_block(Spu2_Ram() + 0x40000, 2, 1, 0x4, 4);
        make_block(Spu2_Ram() + 0x40010, 3, 3, 0x0, 13);
        make_block(Spu2_Ram() + 0x40020, 1, 2, 0x1, 7);
        memset(st, 0, sizeof st);
        for (i = 0; i < 3; i++) {
            Spu2_DecodeBlock(Spu2_Ram() + 0x40000 + 16 * i, st + 3 + 28 * i, &hh1, &hh2);
        }
        voice_setup(1, 7, 0x40000, 0x0C35);
        gSpu2[1].v[7].adsr2 = 0x1FC0; /* sustain: linear increase, rate 0x7F (stays) */
        Spu2_KeyOn(1, 1u << 7);
        gSpu2[1].v[7].phase = SPU2_SUSTAIN;
        gSpu2[1].v[7].envx = 0x7FFF;
        for (k = 0; (uint32_t)k * 0x0C35 >> 12 < 84; k++) {
            uint32_t p = (uint32_t)k * 0x0C35, n = p >> 12;
            int32_t g = Spu2_Gauss(st + n, (p >> 4) & 0xFF);
            int32_t want = (int32_t)((int64_t)((g * 0x7FFF >> 15) * 0x7FFE >> 15) * 0x7FFE >> 15);
            Spu2_Mix(frame, 1);
            bad += frame[0] != want || frame[1] != want;
        }
        expect(bad == 0, "the voice's samples are the Gaussian interpolation of the decoded stream");
        expect(Spu2_KeyStatus(1, 7) == 3, "the three blocks played, then the end");
    }
    /* SpuSt's stream buffer (plat_bgm.c): two halves, flags 6 2 .. 2 then 2 2 .. 3: the voice goes back to the
       buffer's start after the second half, ENDX set, still playing */
    Spu2_Reset();
    for (i = 0; i < 8; i++) {
        static const uint8_t kStFlags[8] = { 6, 2, 0, 2, 2, 2, 0, 3 };
        make_block(Spu2_Ram() + 0x40100 + 16 * i, 12, 0, kStFlags[i], i);
    }
    voice_setup(0, 22, 0x40100, 0x1000);
    Spu2_KeyOn(0, 1u << 22);
    Spu2_Mix(NULL, 8 * 28);
    expect(gSpu2[0].v[22].nax == 0x40100 && gSpu2[0].v[22].lsa == 0x40100 && (gSpu2[0].endx >> 22 & 1) &&
               Spu2_KeyStatus(0, 22) == 1, "stream buffer: back to the start after the second half");
    Spu2_Mix(NULL, 2 * 28);
    expect(gSpu2[0].v[22].nax == 0x40120, "stream buffer: plays on from the start");

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
    Port_StateAdd("spu2.tail", sTail, sizeof sTail, 0); /* what is heard only: saved, not hashed */
}
