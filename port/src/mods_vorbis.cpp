/*
 * Ogg Vorbis files of music mods (T49, docs/port/mods.md): stb_vorbis (third_party/stb/stb_vorbis.c, public domain or
 * MIT) decodes a whole file from memory into 16-bit samples, for plat_mods.c.
 *
 * Why C++: Vorbis decoding computes in floating point. The port's C is built in software float on the EE model
 * (toolchain.py PORT_CFLAGS), which would be slow here and is meant for the game; the build lets host float
 * instructions into the functions of port/src/*.cpp only (toolchain.host_float_objects(), tests/test_port_float.py),
 * as for the overlay. Nothing of the simulation reads what this computes: the samples only replace what the host
 * plays (plat_mods.c, plat_snd.c).
 *
 * No host maths library is linked (imconfig_ttt.h): stb_vorbis' few calls of floor, exp, log, pow, ldexp, sin and cos
 * (codebook and window set-up, never per sample) are given here in double precision from + - * / and integer steps.
 * Built with -fno-plt like the overlay: its C library calls (malloc, qsort...) go through the GOT, no new PLT entry.
 */
#define NDEBUG 1 /* stb_vorbis' assert() checks: off, no __assert_fail */
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- maths for stb_vorbis (set-up only; double precision) ---- */
static double ttt_floor(double x) {
    if (x >= 9007199254740992.0 || x <= -9007199254740992.0 || x != x) {
        return x;
    }
    double t = (double)(long long)x;
    return t > x ? t - 1.0 : t;
}

static double ttt_ldexp(double x, int e) {
    while (e > 0) {
        int k = e > 60 ? 60 : e;
        x *= (double)(1ull << k);
        e -= k;
    }
    while (e < 0) {
        int k = -e > 60 ? 60 : -e;
        x /= (double)(1ull << k);
        e += k;
    }
    return x;
}

static double ttt_exp(double x) {
    const double ln2 = 0.69314718055994530942;
    if (x > 709.0) {
        x = 709.0;
    }
    if (x < -745.0) {
        return 0.0;
    }
    int k = (int)ttt_floor(x / ln2 + 0.5);
    double r = x - k * ln2, term = 1.0, sum = 1.0; /* |r| <= ln2 / 2: the series converges fast */
    for (int i = 1; i < 30; i++) {
        term *= r / i;
        sum += term;
    }
    return ttt_ldexp(sum, k);
}

static double ttt_log(double x) {
    if (!(x > 0.0)) {
        return -1e300;
    }
    int e = 0;
    while (x >= 2.0) {
        x *= 0.5;
        e++;
    }
    while (x < 1.0) {
        x *= 2.0;
        e--;
    }
    if (x > 1.41421356237309504880) { /* m in [0.707, 1.414): z small */
        x *= 0.5;
        e++;
    }
    double z = (x - 1.0) / (x + 1.0), z2 = z * z, term = z, sum = 0.0;
    for (int i = 1; i < 60; i += 2) { /* log m = 2 atanh z */
        sum += term / i;
        term *= z2;
    }
    return 2.0 * sum + e * 0.69314718055994530942;
}

static double ttt_pow(double x, double y) {
    if (y == ttt_floor(y) && y >= 0.0 && y < 64.0) { /* stb_vorbis' only use: an integer power */
        double r = 1.0;
        for (int i = 0; i < (int)y; i++) {
            r *= x;
        }
        return r;
    }
    return x > 0.0 ? ttt_exp(y * ttt_log(x)) : 0.0;
}

static double ttt_sin(double x) {
    const double pi = 3.14159265358979323846, twopi = 6.28318530717958647692;
    x -= twopi * ttt_floor(x / twopi); /* [0, 2 pi) */
    if (x > pi) {
        x -= twopi;
    }
    if (x > pi * 0.5) { /* sin(pi - x) = sin(x): [-pi / 2, pi / 2] */
        x = pi - x;
    } else if (x < -pi * 0.5) {
        x = -pi - x;
    }
    double x2 = x * x, term = x, sum = x;
    for (int i = 1; i < 14; i++) {
        term *= -x2 / ((2 * i) * (2 * i + 1));
        sum += term;
    }
    return sum;
}

static double ttt_cos(double x) { return ttt_sin(x + 1.57079632679489661923); }

#define floor ttt_floor
#define ldexp ttt_ldexp
#define exp ttt_exp
#define log ttt_log
#define pow ttt_pow
#define sin ttt_sin
#define cos ttt_cos

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_MAX_CHANNELS 8
#include "../../third_party/stb/stb_vorbis.c"

#undef floor
#undef ldexp
#undef exp
#undef log
#undef pow
#undef sin
#undef cos

/* An unsigned decimal value of a comment "KEY=value" (case-insensitive key); 1 when found. */
static int comment_value(stb_vorbis_comment *c, const char *key, uint32_t *out) {
    size_t n = strlen(key);
    for (int i = 0; i < c->comment_list_length; i++) {
        const char *s = c->comment_list[i];
        size_t k = 0;
        while (k < n && s[k] != 0 && ((s[k] | 0x20) == (key[k] | 0x20))) {
            k++;
        }
        if (k == n && s[n] == '=' && s[n + 1] >= '0' && s[n + 1] <= '9') {
            uint32_t v = 0;
            for (s += n + 1; *s >= '0' && *s <= '9'; s++) {
                v = v * 10 + (uint32_t)(*s - '0');
            }
            *out = v;
            return 1;
        }
    }
    return 0;
}

/* A whole Ogg Vorbis file in memory -> malloc'd 16-bit interleaved stereo (mono doubled, beyond two channels the first
   two), its rate and frame count; the loop from the LOOPSTART / LOOPLENGTH (or LOOPEND) comments, in frames, when the
   file has them (*loopEnd 0 otherwise). `maxFrames` caps the length. 0 on success. */
extern "C" int Mods_DecodeVorbis(const uint8_t *data, int len, uint32_t maxFrames, int16_t **out, uint32_t *frames,
                                 uint32_t *rate, uint32_t *loopStart, uint32_t *loopEnd) {
    int err = 0;
    stb_vorbis *v = stb_vorbis_open_memory(data, len, &err, nullptr);
    if (v == nullptr) {
        return -1;
    }
    stb_vorbis_info info = stb_vorbis_get_info(v);
    uint32_t total = stb_vorbis_stream_length_in_samples(v);
    if (info.channels < 1 || info.sample_rate < 4000 || info.sample_rate > 192000) {
        stb_vorbis_close(v);
        return -1;
    }
    if (total == 0 || total > maxFrames) {
        total = maxFrames; /* unknown length (or too long): decoded up to the cap, then trimmed */
    }
    int16_t *buf = (int16_t *)malloc((size_t)total * 4);
    if (buf == nullptr) {
        stb_vorbis_close(v);
        return -1;
    }
    uint32_t got = 0;
    while (got < total) {
        int n = stb_vorbis_get_samples_short_interleaved(v, 2, buf + 2 * got, (int)(total - got) * 2);
        if (n <= 0) {
            break;
        }
        got += (uint32_t)n;
    }
    stb_vorbis_comment c = stb_vorbis_get_comment(v);
    uint32_t ls = 0, ll = 0, le = 0;
    *loopStart = *loopEnd = 0;
    if (comment_value(&c, "LOOPSTART", &ls)) {
        if (comment_value(&c, "LOOPLENGTH", &ll) && ll > 0) {
            le = ls + ll;
        } else if (!comment_value(&c, "LOOPEND", &le)) {
            le = got;
        }
        if (ls < le && le <= got) {
            *loopStart = ls;
            *loopEnd = le;
        }
    }
    stb_vorbis_close(v);
    if (got == 0) {
        free(buf);
        return -1;
    }
    *out = buf;
    *frames = got;
    *rate = info.sample_rate;
    return 0;
}
