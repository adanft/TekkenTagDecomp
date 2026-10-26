/*
 * Ed25519 signatures and SHA-512: the subset of TweetNaCl (crypto_sign, crypto_hash) needed to sign and verify an
 * update manifest (docs/port/matchmaking.md). Header only: every function is static, no C library call (no memcpy, no
 * allocation, no randomness: the caller hands in the 32-byte seed), integer arithmetic only.
 *
 * The field and group arithmetic follows TweetNaCl 20140427 (Daniel J. Bernstein, Bernard van Gastel, Wesley
 * Janssen, Tanja Lange, Peter Schwabe, Sjaak Smetsers; public domain, see LICENSE in this folder); SHA-512 is written
 * out plainly (FIPS 180-4). The constants (d, 2d, the base point, sqrt(-1), SHA-512's K and initial values) were
 * computed from their definitions rather than copied. Changes from TweetNaCl: detached signatures, an incremental
 * SHA-512, and verification refuses a signature whose S is not below the group order L (RFC 8032 section 5.1.7).
 * tests/test_port_match.py checks it against PyNaCl (libsodium) and OpenSSL when either is installed, and against
 * RFC 8032's first test vector.
 *
 * Signing is not constant time with respect to anything but what TweetNaCl already guarantees; it runs offline
 * (port/server/ttt-sign.c), never in the game.
 */
#ifndef TTT_ED25519_H
#define TTT_ED25519_H
#include <stdint.h>

typedef int64_t ed_gf[16];

/* ---------------------------------------------------------------------------------------------- SHA-512 */

typedef struct {
    uint64_t h[8];
    uint64_t len;  /* bytes hashed so far */
    uint8_t buf[128];
    uint32_t fill;
} ed_sha512_ctx;

static const uint64_t ed_sha512_k[80] = {
    0x428a2f98d728ae22ull, 0x7137449123ef65cdull, 0xb5c0fbcfec4d3b2full, 0xe9b5dba58189dbbcull,
    0x3956c25bf348b538ull, 0x59f111f1b605d019ull, 0x923f82a4af194f9bull, 0xab1c5ed5da6d8118ull,
    0xd807aa98a3030242ull, 0x12835b0145706fbeull, 0x243185be4ee4b28cull, 0x550c7dc3d5ffb4e2ull,
    0x72be5d74f27b896full, 0x80deb1fe3b1696b1ull, 0x9bdc06a725c71235ull, 0xc19bf174cf692694ull,
    0xe49b69c19ef14ad2ull, 0xefbe4786384f25e3ull, 0x0fc19dc68b8cd5b5ull, 0x240ca1cc77ac9c65ull,
    0x2de92c6f592b0275ull, 0x4a7484aa6ea6e483ull, 0x5cb0a9dcbd41fbd4ull, 0x76f988da831153b5ull,
    0x983e5152ee66dfabull, 0xa831c66d2db43210ull, 0xb00327c898fb213full, 0xbf597fc7beef0ee4ull,
    0xc6e00bf33da88fc2ull, 0xd5a79147930aa725ull, 0x06ca6351e003826full, 0x142929670a0e6e70ull,
    0x27b70a8546d22ffcull, 0x2e1b21385c26c926ull, 0x4d2c6dfc5ac42aedull, 0x53380d139d95b3dfull,
    0x650a73548baf63deull, 0x766a0abb3c77b2a8ull, 0x81c2c92e47edaee6ull, 0x92722c851482353bull,
    0xa2bfe8a14cf10364ull, 0xa81a664bbc423001ull, 0xc24b8b70d0f89791ull, 0xc76c51a30654be30ull,
    0xd192e819d6ef5218ull, 0xd69906245565a910ull, 0xf40e35855771202aull, 0x106aa07032bbd1b8ull,
    0x19a4c116b8d2d0c8ull, 0x1e376c085141ab53ull, 0x2748774cdf8eeb99ull, 0x34b0bcb5e19b48a8ull,
    0x391c0cb3c5c95a63ull, 0x4ed8aa4ae3418acbull, 0x5b9cca4f7763e373ull, 0x682e6ff3d6b2b8a3ull,
    0x748f82ee5defb2fcull, 0x78a5636f43172f60ull, 0x84c87814a1f0ab72ull, 0x8cc702081a6439ecull,
    0x90befffa23631e28ull, 0xa4506cebde82bde9ull, 0xbef9a3f7b2c67915ull, 0xc67178f2e372532bull,
    0xca273eceea26619cull, 0xd186b8c721c0c207ull, 0xeada7dd6cde0eb1eull, 0xf57d4f7fee6ed178ull,
    0x06f067aa72176fbaull, 0x0a637dc5a2c898a6ull, 0x113f9804bef90daeull, 0x1b710b35131c471bull,
    0x28db77f523047d84ull, 0x32caab7b40c72493ull, 0x3c9ebe0a15c9bebcull, 0x431d67c49c100d4cull,
    0x4cc5d4becb3e42b6ull, 0x597f299cfc657e2aull, 0x5fcb6fab3ad6faecull, 0x6c44198c4a475817ull,
};

static uint64_t ed_ror(uint64_t x, int n) { return x >> n | x << (64 - n); }

static void ed_sha512_block(ed_sha512_ctx *s, const uint8_t *p) {
    uint64_t w[80], a, b, c, d, e, f, g, h, t1, t2;
    int i, j;
    for (i = 0; i < 16; i++) {
        w[i] = 0;
        for (j = 0; j < 8; j++) {
            w[i] = w[i] << 8 | p[8 * i + j];
        }
    }
    for (i = 16; i < 80; i++) {
        uint64_t s0 = ed_ror(w[i - 15], 1) ^ ed_ror(w[i - 15], 8) ^ (w[i - 15] >> 7);
        uint64_t s1 = ed_ror(w[i - 2], 19) ^ ed_ror(w[i - 2], 61) ^ (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (i = 0; i < 80; i++) {
        t1 = h + (ed_ror(e, 14) ^ ed_ror(e, 18) ^ ed_ror(e, 41)) + ((e & f) ^ (~e & g)) + ed_sha512_k[i] + w[i];
        t2 = (ed_ror(a, 28) ^ ed_ror(a, 34) ^ ed_ror(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
    }
    s->h[0] += a, s->h[1] += b, s->h[2] += c, s->h[3] += d, s->h[4] += e, s->h[5] += f, s->h[6] += g, s->h[7] += h;
}

static void ed_sha512_init(ed_sha512_ctx *s) {
    static const uint64_t iv[8] = {0x6a09e667f3bcc908ull, 0xbb67ae8584caa73bull, 0x3c6ef372fe94f82bull,
                                   0xa54ff53a5f1d36f1ull, 0x510e527fade682d1ull, 0x9b05688c2b3e6c1full,
                                   0x1f83d9abfb41bd6bull, 0x5be0cd19137e2179ull};
    int i;
    for (i = 0; i < 8; i++) {
        s->h[i] = iv[i];
    }
    s->len = 0;
    s->fill = 0;
}

static void ed_sha512_update(ed_sha512_ctx *s, const uint8_t *p, uint64_t n) {
    s->len += n;
    while (n > 0) {
        s->buf[s->fill++] = *p++;
        n--;
        if (s->fill == 128) {
            ed_sha512_block(s, s->buf);
            s->fill = 0;
        }
    }
}

static void ed_sha512_final(ed_sha512_ctx *s, uint8_t out[64]) {
    uint64_t bits = s->len * 8;
    uint8_t one = 0x80, zero = 0, lenb[16];
    int i;
    ed_sha512_update(s, &one, 1);
    while (s->fill != 112) {
        ed_sha512_update(s, &zero, 1);
    }
    for (i = 0; i < 8; i++) {
        lenb[i] = 0; /* (messages under 2^61 bytes) */
        lenb[8 + i] = (uint8_t)(bits >> (56 - 8 * i));
    }
    ed_sha512_update(s, lenb, 16);
    for (i = 0; i < 64; i++) {
        out[i] = (uint8_t)(s->h[i / 8] >> (56 - 8 * (i % 8)));
    }
}

static void ed_sha512(uint8_t out[64], const uint8_t *p, uint64_t n) {
    ed_sha512_ctx s;
    ed_sha512_init(&s);
    ed_sha512_update(&s, p, n);
    ed_sha512_final(&s, out);
}

/* ---------------------------------------------------------------------------------------------- GF(2^255 - 19) */

static const ed_gf ed_gf0 = {0}, ed_gf1 = {1};
static const ed_gf ed_D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
                           0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};
static const ed_gf ed_D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
                            0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406};
static const ed_gf ed_X = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
                           0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169};
static const ed_gf ed_Y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                           0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666};
static const ed_gf ed_I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
                           0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};

static int ed_vn32(const uint8_t *x, const uint8_t *y) { /* 0 when equal, -1 when not */
    uint32_t i, d = 0;
    for (i = 0; i < 32; i++) {
        d |= (uint32_t)(x[i] ^ y[i]);
    }
    return (int)(1 & ((d - 1) >> 8)) - 1;
}

static void ed_set(ed_gf r, const ed_gf a) {
    int i;
    for (i = 0; i < 16; i++) {
        r[i] = a[i];
    }
}

static void ed_car(ed_gf o) {
    int i;
    int64_t c;
    for (i = 0; i < 16; i++) {
        o[i] += (int64_t)1 << 16;
        c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c * ((int64_t)1 << 16);
    }
}

static void ed_sel(ed_gf p, ed_gf q, int b) {
    int64_t t, c = ~(int64_t)(b - 1);
    int i;
    for (i = 0; i < 16; i++) {
        t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void ed_pack25519(uint8_t *o, const ed_gf n) {
    int i, j, b;
    ed_gf m, t;
    ed_set(t, n);
    ed_car(t);
    ed_car(t);
    ed_car(t);
    for (j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        ed_sel(t, m, 1 - b);
    }
    for (i = 0; i < 16; i++) {
        o[2 * i] = (uint8_t)(t[i] & 0xff);
        o[2 * i + 1] = (uint8_t)(t[i] >> 8);
    }
}

static int ed_neq(const ed_gf a, const ed_gf b) {
    uint8_t c[32], d[32];
    ed_pack25519(c, a);
    ed_pack25519(d, b);
    return ed_vn32(c, d);
}

static uint8_t ed_par(const ed_gf a) {
    uint8_t d[32];
    ed_pack25519(d, a);
    return d[0] & 1;
}

static void ed_unpack25519(ed_gf o, const uint8_t *n) {
    int i;
    for (i = 0; i < 16; i++) {
        o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
    }
    o[15] &= 0x7fff;
}

static void ed_A(ed_gf o, const ed_gf a, const ed_gf b) {
    int i;
    for (i = 0; i < 16; i++) {
        o[i] = a[i] + b[i];
    }
}

static void ed_Z(ed_gf o, const ed_gf a, const ed_gf b) {
    int i;
    for (i = 0; i < 16; i++) {
        o[i] = a[i] - b[i];
    }
}

static void ed_M(ed_gf o, const ed_gf a, const ed_gf b) {
    int64_t t[31];
    int i, j;
    for (i = 0; i < 31; i++) {
        t[i] = 0;
    }
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            t[i + j] += a[i] * b[j];
        }
    }
    for (i = 0; i < 15; i++) {
        t[i] += 38 * t[i + 16];
    }
    for (i = 0; i < 16; i++) {
        o[i] = t[i];
    }
    ed_car(o);
    ed_car(o);
}

static void ed_S(ed_gf o, const ed_gf a) { ed_M(o, a, a); }

static void ed_inv(ed_gf o, const ed_gf i) {
    ed_gf c;
    int a;
    ed_set(c, i);
    for (a = 253; a >= 0; a--) {
        ed_S(c, c);
        if (a != 2 && a != 4) {
            ed_M(c, c, i);
        }
    }
    ed_set(o, c);
}

static void ed_pow2523(ed_gf o, const ed_gf i) {
    ed_gf c;
    int a;
    ed_set(c, i);
    for (a = 250; a >= 0; a--) {
        ed_S(c, c);
        if (a != 1) {
            ed_M(c, c, i);
        }
    }
    ed_set(o, c);
}

/* ---------------------------------------------------------------------------------------------- the group */

static void ed_add(ed_gf p[4], ed_gf q[4]) {
    ed_gf a, b, c, d, t, e, f, g, h;
    ed_Z(a, p[1], p[0]);
    ed_Z(t, q[1], q[0]);
    ed_M(a, a, t);
    ed_A(b, p[0], p[1]);
    ed_A(t, q[0], q[1]);
    ed_M(b, b, t);
    ed_M(c, p[3], q[3]);
    ed_M(c, c, ed_D2);
    ed_M(d, p[2], q[2]);
    ed_A(d, d, d);
    ed_Z(e, b, a);
    ed_Z(f, d, c);
    ed_A(g, d, c);
    ed_A(h, b, a);
    ed_M(p[0], e, f);
    ed_M(p[1], h, g);
    ed_M(p[2], g, f);
    ed_M(p[3], e, h);
}

static void ed_cswap(ed_gf p[4], ed_gf q[4], uint8_t b) {
    int i;
    for (i = 0; i < 4; i++) {
        ed_sel(p[i], q[i], b);
    }
}

static void ed_pack(uint8_t *r, ed_gf p[4]) {
    ed_gf tx, ty, zi;
    ed_inv(zi, p[2]);
    ed_M(tx, p[0], zi);
    ed_M(ty, p[1], zi);
    ed_pack25519(r, ty);
    r[31] ^= (uint8_t)(ed_par(tx) << 7);
}

static void ed_scalarmult(ed_gf p[4], ed_gf q[4], const uint8_t *s) {
    int i;
    ed_set(p[0], ed_gf0);
    ed_set(p[1], ed_gf1);
    ed_set(p[2], ed_gf1);
    ed_set(p[3], ed_gf0);
    for (i = 255; i >= 0; --i) {
        uint8_t b = (uint8_t)((s[i / 8] >> (i & 7)) & 1);
        ed_cswap(p, q, b);
        ed_add(q, p);
        ed_add(p, p);
        ed_cswap(p, q, b);
    }
}

static void ed_scalarbase(ed_gf p[4], const uint8_t *s) {
    ed_gf q[4];
    ed_set(q[0], ed_X);
    ed_set(q[1], ed_Y);
    ed_set(q[2], ed_gf1);
    ed_M(q[3], ed_X, ed_Y);
    ed_scalarmult(p, q, s);
}

/* the group order L, little-endian */
static const uint8_t ed_L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                                 0xa2, 0xde, 0xf9, 0xde, 0x14, 0,    0,    0,    0,    0,    0,
                                 0,    0,    0,    0,    0,    0,    0,    0,    0,    0x10};

static void ed_modL(uint8_t *r, int64_t x[64]) {
    int64_t carry;
    int i, j;
    for (i = 63; i >= 32; --i) {
        carry = 0;
        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * (int64_t)ed_L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry * 256;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (j = 0; j < 32; j++) {
        x[j] += carry - (x[31] >> 4) * (int64_t)ed_L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (j = 0; j < 32; j++) {
        x[j] -= carry * (int64_t)ed_L[j];
    }
    for (i = 0; i < 32; i++) {
        x[i + 1] += x[i] >> 8;
        r[i] = (uint8_t)(x[i] & 255);
    }
}

static void ed_reduce(uint8_t *r) { /* 64 bytes -> 32 bytes mod L */
    int64_t x[64];
    int i;
    for (i = 0; i < 64; i++) {
        x[i] = (int64_t)r[i];
    }
    for (i = 0; i < 64; i++) {
        r[i] = 0;
    }
    ed_modL(r, x);
}

static int ed_unpackneg(ed_gf r[4], const uint8_t p[32]) {
    ed_gf t, chk, num, den, den2, den4, den6;
    ed_set(r[2], ed_gf1);
    ed_unpack25519(r[1], p);
    ed_S(num, r[1]);
    ed_M(den, num, ed_D);
    ed_Z(num, num, r[2]);
    ed_A(den, r[2], den);
    ed_S(den2, den);
    ed_S(den4, den2);
    ed_M(den6, den4, den2);
    ed_M(t, den6, num);
    ed_M(t, t, den);
    ed_pow2523(t, t);
    ed_M(t, t, num);
    ed_M(t, t, den);
    ed_M(t, t, den);
    ed_M(r[0], t, den);
    ed_S(chk, r[0]);
    ed_M(chk, chk, den);
    if (ed_neq(chk, num)) {
        ed_M(r[0], r[0], ed_I);
    }
    ed_S(chk, r[0]);
    ed_M(chk, chk, den);
    if (ed_neq(chk, num)) {
        return -1;
    }
    if (ed_par(r[0]) == (p[31] >> 7)) {
        ed_Z(r[0], ed_gf0, r[0]);
    }
    ed_M(r[3], r[0], r[1]);
    return 0;
}

/* ---------------------------------------------------------------------------------------------- the interface */

/* The public key of a 32-byte secret seed. */
static void ed25519_public_key(uint8_t pk[32], const uint8_t seed[32]) {
    uint8_t d[64];
    ed_gf p[4];
    ed_sha512(d, seed, 32);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;
    ed_scalarbase(p, d);
    ed_pack(pk, p);
}

/* A detached signature (R, S) of m with the seed's key pair (RFC 8032 Ed25519). */
static void ed25519_sign(uint8_t sig[64], const uint8_t *m, uint64_t n, const uint8_t seed[32]) {
    uint8_t d[64], h[64], r[64], pk[32];
    int64_t x[64];
    ed_gf p[4];
    ed_sha512_ctx s;
    int i, j;
    ed25519_public_key(pk, seed);
    ed_sha512(d, seed, 32);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;
    ed_sha512_init(&s);
    ed_sha512_update(&s, d + 32, 32);
    ed_sha512_update(&s, m, n);
    ed_sha512_final(&s, r);
    ed_reduce(r);
    ed_scalarbase(p, r);
    ed_pack(sig, p);
    ed_sha512_init(&s);
    ed_sha512_update(&s, sig, 32);
    ed_sha512_update(&s, pk, 32);
    ed_sha512_update(&s, m, n);
    ed_sha512_final(&s, h);
    ed_reduce(h);
    for (i = 0; i < 64; i++) {
        x[i] = 0;
    }
    for (i = 0; i < 32; i++) {
        x[i] = (int64_t)r[i];
    }
    for (i = 0; i < 32; i++) {
        for (j = 0; j < 32; j++) {
            x[i + j] += (int64_t)h[i] * (int64_t)d[j];
        }
    }
    ed_modL(sig + 32, x);
}

/* 0 when sig is pk's signature of m, -1 when it is not (or pk is not a point, or S is not below L). */
static int ed25519_verify(const uint8_t sig[64], const uint8_t *m, uint64_t n, const uint8_t pk[32]) {
    uint8_t t[32], h[64];
    ed_gf p[4], q[4];
    ed_sha512_ctx s;
    int i;
    for (i = 31; i >= 0; i--) { /* S < L, compared from the top byte */
        if (sig[32 + i] < ed_L[i]) {
            break;
        }
        if (sig[32 + i] > ed_L[i] || i == 0) {
            return -1;
        }
    }
    if (ed_unpackneg(q, pk) != 0) {
        return -1;
    }
    ed_sha512_init(&s);
    ed_sha512_update(&s, sig, 32);
    ed_sha512_update(&s, pk, 32);
    ed_sha512_update(&s, m, n);
    ed_sha512_final(&s, h);
    ed_reduce(h);
    ed_scalarmult(p, q, h);
    ed_scalarbase(q, sig + 32);
    ed_add(p, q);
    ed_pack(t, p);
    return ed_vn32(sig, t);
}

#endif
