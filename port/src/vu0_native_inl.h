/*
 * The game's VU0 skinning programs (src/vu0/skin_diffuse.vsm, skin_specular.vsm, skin_gloss.vsm, skin_envmap.vsm:
 * vu0_full.vsm's programs at 0x000, 0x04A, 0x0EA, 0x186; vu0_small.vsm's skin_diffuse_world and skin_hide) in C
 * (T31), for Vu0_RunNative. Included by
 * vu_fast.c only: the code runs on the SSE unit under the predecoded interpreter's float exception (toolchain.SIMD_FILES,
 * which names vu_fast.c), and uses its helpers.
 *
 * Each program is written out instruction by instruction, one line per instruction, in the order run_fast does it:
 * the step count, the I bit, the upper result (MAC or clipping flags queued), the lower half, then the upper result
 * written. Q, the clipping and the MAC flags are brought up to date exactly where the interpreter does it (an operand
 * Q, FCAND, FMAND, the end), so their timing needs no reasoning here and an entry state with results still in flight
 * (the environment-map program ends with one) comes out the same. What goes away is the decoding, the dispatch, the
 * dest-mask selects of the full-mask results, and the operand mapping of the add / multiply:
 *   - with the denormals-are-zero bit added to V4_MXCSR the SSE unit reads an exponent-0 operand as a zero of its
 *     sign, which is v4_op_in's mapping; the other half of that mapping (exponent 255 is the largest number) cannot be
 *     done by the host, but such an operand can only come from outside the program (results round toward zero and
 *     never reach exponent 255), so the registers the programs read and every quadword they load are tested for it
 *     and the rest of the program then uses v4_add / v4_mul (`slow`). No CPU without DAZ runs the fast path.
 * The differential checks (vu0_micro.c: TTT_VU0_CHECK=1 on the game, TTT_SELFTEST=vu0 on random states) compare the
 * results with the interpreter's byte for byte: registers, data memory, the whole pipeline state.
 */

typedef struct N0 {
    VuCore *u;
    V4 v[32], acc;      /* the registers, in the host's registers while the program runs (written back after) */
    uint32_t cycle, mt; /* the step, the MAC queue's next free entry (run_fast's mt / mc) */
    int mc, slow;
    /* the MAC flags' entries not yet in the queue (n0_mac / n0_flush): np of them, the last eight kept */
    V4 mres[8];
    uint32_t mcyc[8], mmsk[8];
    int np;
} N0;

#define N0_DAZ 0x0040u
#define N0_EXP V4_SET1(0x7F800000)
/* a dest mask (x = bit 3) as all-ones lanes, a constant once inlined */
#define N0_MASK(d) _mm_set_epi32(-((d) & 1), -(((d) >> 1) & 1), -(((d) >> 2) & 1), -(((d) >> 3) & 1))

static int n0_daz = -1; /* whether the CPU has the denormals-are-zero bit (FXSAVE's MXCSR_MASK) */

static inline __attribute__((always_inline)) int n0_bad(V4 v) { /* any lane with exponent 255 */
    return _mm_movemask_epi8(_mm_cmpeq_epi32(_mm_and_si128(v, N0_EXP), N0_EXP));
}
static inline __attribute__((always_inline)) uint32_t n0_lane(V4 v, int c) {
    return (uint32_t)_mm_cvtsi128_si32(c == 0 ? v : c == 1 ? _mm_shuffle_epi32(v, 0x55) : c == 2 ? _mm_shuffle_epi32(v, 0xAA)
                                                                                              : _mm_shuffle_epi32(v, 0xFF));
}
/* the exact arithmetic out of line: rarely run, it keeps the programs' code small */
static __attribute__((noinline)) V4 n0_mul_exact(V4 a, V4 b) { return v4_mul(a, b); }
static __attribute__((noinline)) V4 n0_add_exact(V4 a, V4 b) { return v4_add(a, b); }
static inline __attribute__((always_inline)) V4 n0_mul(const N0 *s, V4 a, V4 b) {
    if (__builtin_expect(s->slow, 0)) {
        return n0_mul_exact(a, b);
    }
    return _mm_castps_si128(_mm_mul_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b)));
}
static inline __attribute__((always_inline)) V4 n0_add(const N0 *s, V4 a, V4 b) {
    if (__builtin_expect(s->slow, 0)) {
        return n0_add_exact(a, b);
    }
    return _mm_castps_si128(_mm_add_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b)));
}
static inline __attribute__((always_inline)) V4 n0_sub(const N0 *s, V4 a, V4 b) { /* v4_sub: a + (b, sign flipped) */
    return n0_add(s, a, _mm_xor_si128(b, V4_SET1(0x80000000u)));
}
/* the upper result r over a register's old value under a dest mask */
static inline __attribute__((always_inline)) V4 n0_sel(V4 r, V4 old, uint32_t dest) {
    return dest == 15 ? r : v4_sel(N0_MASK(dest), r, old);
}
/* MACPUSH, kept aside until the queue is read or control flow joins (n0_flush): of the entries of a stretch of
   straight code only the last eight reach the queue (the others would be overwritten there), and their slots are
   constants the compiler keeps in registers. Every join of paths is preceded by a flush (np then 0 on all of them),
   so np is a constant at every push. */
static inline __attribute__((always_inline)) void n0_mac(N0 *s, V4 r, uint32_t dest) {
    int k = s->np & 7;

    s->mres[k] = r;
    s->mcyc[k] = s->cycle + 4;
    s->mmsk[k] = dest;
    s->np++;
}
/* The entries kept aside into the queue as the pushes one by one would leave it. */
static inline __attribute__((always_inline)) void n0_flush(N0 *s) {
    int i, n = s->np;

    for (i = n > 8 ? n - 8 : 0; i < n; i++) {
        VuMacStep *st = &s->u->macQueue[(s->mt + (uint32_t)i) & 7];
        st->cycle = s->mcyc[i & 7];
        st->mask = s->mmsk[i & 7];
        V4_STORE(st->res, s->mres[i & 7]);
    }
    s->mt = (s->mt + (uint32_t)n) & 7;
    s->mc = s->mc + n < 8 ? s->mc + n : 8;
    s->np = 0;
}
/* macq_now with one copy into macNow (the last of the entries that arrived; each arrival replaces the one before) */
static inline __attribute__((always_inline)) void n0_macq_now(VuCore *u, uint32_t cycle) {
    int n = u->macCount, h = u->macHead, k = 0;

    while (k < n && u->macQueue[(h + k) & 7].cycle <= cycle) {
        k++;
    }
    if (k != 0) {
        u->macNow = u->macQueue[(h + k - 1) & 7];
        u->macLazy = 1;
        u->macHead = (h + k) & 7;
        u->macCount = n - k;
    }
}
/* MACSYNC: the queue brought up to the step (FMAND) */
static inline __attribute__((always_inline)) void n0_macsync(N0 *s) {
    n0_flush(s);
    s->u->macCount = s->mc;
    s->u->macHead = (int)((s->mt - (uint32_t)s->mc) & 7);
    n0_macq_now(s->u, s->cycle);
    s->mc = s->u->macCount;
}
/* lq.dest vf[t], off(vi[b]) (t != 0): the loaded lanes tested for exponent 255 */
static inline __attribute__((always_inline)) void n0_lq(N0 *s, int t, uint32_t dest, int32_t off, int b) {
    const uint8_t *m = s->u->data + (((uint32_t)((int32_t)s->u->vi[b] + off)) & s->u->dataMask) * 16;
    V4 v = V4_LOAD(m);

    if (dest == 15) {
        s->slow |= n0_bad(v);
        s->v[t] = v;
    } else {
        s->slow |= n0_bad(_mm_and_si128(v, N0_MASK(dest)));
        s->v[t] = v4_sel(N0_MASK(dest), v, s->v[t]);
    }
}
/* QSETF */
static inline __attribute__((always_inline)) void n0_qset(N0 *s, uint32_t v, uint32_t lat) {
    q_now(s->u, s->cycle);
    s->u->qPending = v;
    s->u->qReady = s->cycle + lat;
}
/* clipw.xyz vf[fs], vf[ft]w */
static inline __attribute__((always_inline)) void n0_clip(N0 *s, int fs, int ft) {
    V4 a = s->v[fs];
    int32_t w = sf_key(n0_lane(s->v[ft], 3) & 0x7FFFFFFFu), x = sf_key(n0_lane(a, 0)), y = sf_key(n0_lane(a, 1)),
            z = sf_key(n0_lane(a, 2));
    uint32_t fl = (x > w) | (x < -w) << 1 | (y > w) << 2 | (y < -w) << 3 | (z > w) << 4 | (z < -w) << 5;
    VuCore *u = s->u;

    u->clipLatest = ((u->clipLatest << 6) | fl) & 0xFFFFFF;
    queue(u->clipQueue, &u->clipHead, &u->clipCount, s->cycle, u->clipLatest);
}

/* The notation of the programs below: one line per instruction (its micro-memory slot in the comment). */
#define T s->cycle++
#define R(n) s->v[n]
#define BC(n, c) _mm_shuffle_epi32(s->v[n], (c) * 0x55)
#define QB (q_now(s->u, s->cycle), V4_SET1(*s->u->q))
#define IB V4_SET1(s->u->i)
#define ACC s->acc
#define MUL(a, b) n0_mul(s, a, b)
#define ADD(a, b) n0_add(s, a, b)
#define SUB(a, b) n0_sub(s, a, b)
#define MADD(a, b) n0_add(s, ACC, n0_mul(s, a, b))
#define MSUB(a, b) n0_sub(s, ACC, n0_mul(s, a, b))
#define MAC(d) n0_mac(s, r, d)
#define TO(n, d) (s->v[n] = n0_sel(r, s->v[n], d))
#define TOACC(d) (s->acc = n0_sel(r, s->acc, d))
#define LQ(t, d, off, b) n0_lq(s, t, d, off, b)
#define LOI(v) (s->u->i = (v))
#define IREG(n) s->u->vi[n]
#define ISET(n, v) (s->u->vi[n] = (uint16_t)(v))
#define MOVE(t, c, f) (s->v[t] = v4_sel(N0_MASK(8 >> (c)), s->v[f], s->v[t]))
#define DIV_Q(a, ac, b, bc) n0_qset(s, v1_div(n0_lane(s->v[a], ac), n0_lane(s->v[b], bc)), 7)
#define RSQRT_Q(a, ac, b, bc) n0_qset(s, v1_div(n0_lane(s->v[a], ac), v1_sqrt(n0_lane(s->v[b], bc))), 13)
#define FLUSH n0_flush(s)
#define FCAND(m) (clip_now(s->u, s->cycle), ISET(1, (s->u->clip & (m)) != 0))
#define FMAND(t, b) (n0_macsync(s), ISET(t, mac_now(s->u) & IREG(b)))
enum { X, Y, Z, W };
enum { MX = 8, MY = 4, MZ = 2, MW = 1, MXYZ = 14, MXYZW = 15, MYZ = 6, MXY = 12 };

/* The bone blend at the start of every program (slots 0x00-0x25 of each): normals by rows nr..nr+2 of each bone block
   into vf[nd], positions by rows pr..pr+3 into vf22 (vf21 the pending term). Stops after the last bone's slot. */
static inline __attribute__((always_inline)) void n0_blend(N0 *s, int nd, int nr, int pr) {
    V4 r;
    int t;

    T; r = MUL(R(0), BC(0, X)); MAC(MXYZW); LQ(12, MXYZW, nr, 2); TO(1, MXYZW);             /* 00 */
    T; LQ(13, MXYZW, nr + 1, 2);                                                            /* 01 */
    T; LQ(14, MXYZW, nr + 2, 2);                                                            /* 02 */
    T; r = MUL(R(0), BC(0, X)); MAC(MXYZW); LQ(15, MXYZW, pr, 2); TO(21, MXYZW);            /* 03 */
    T; r = MUL(R(12), BC(2, X)); MAC(MXYZW); LQ(16, MXYZW, pr + 1, 2); TOACC(MXYZW);         /* 04 */
    T; r = MADD(R(13), BC(2, Y)); MAC(MXYZW); LQ(17, MXYZW, pr + 2, 2); TOACC(MXYZW);        /* 05 */
    T; r = MADD(R(14), BC(2, Z)); MAC(MXYZW); LQ(18, MXYZW, pr + 3, 2); TO(nd, MXYZW);       /* 06 */
    T; r = MUL(R(15), BC(26, X)); MAC(MXYZW); LQ(12, MXYZW, nr, 3); TOACC(MXYZW);            /* 07 */
    T; r = MADD(R(16), BC(26, Y)); MAC(MXYZW); LQ(13, MXYZW, nr + 1, 3); TOACC(MXYZW);       /* 08 */
    T; r = MADD(R(17), BC(26, Z)); MAC(MXYZW); t = IREG(3) == IREG(0); TOACC(MXYZW);             /* 09 ibeq vi3 */
    T; r = MADD(R(18), BC(26, W)); MAC(MXYZW); LQ(14, MXYZW, nr + 2, 3); TO(22, MXYZW);      /* 0A */
    if (t) {
        FLUSH;
        return;
    }
    T; LQ(15, MXYZW, pr, 3);                                                                /* 0B */
    T; r = MUL(R(12), BC(3, X)); MAC(MXYZW); LQ(16, MXYZW, pr + 1, 3); TOACC(MXYZW);         /* 0C */
    T; r = MADD(R(13), BC(3, Y)); MAC(MXYZW); LQ(17, MXYZW, pr + 2, 3); TOACC(MXYZW);        /* 0D */
    T; r = MADD(R(14), BC(3, Z)); MAC(MXYZW); LQ(18, MXYZW, pr + 3, 3); TO(19, MXYZW);       /* 0E */
    T; r = MUL(R(15), BC(27, X)); MAC(MXYZW); LQ(12, MXYZW, nr, 4); TOACC(MXYZW);            /* 0F */
    T; r = MADD(R(16), BC(27, Y)); MAC(MXYZW); LQ(13, MXYZW, nr + 1, 4); TOACC(MXYZW);       /* 10 */
    T; r = MADD(R(17), BC(27, Z)); MAC(MXYZW); LQ(14, MXYZW, nr + 2, 4); TOACC(MXYZW);       /* 11 */
    T; r = ADD(R(nd), R(19)); MAC(MXYZW); t = IREG(4) == IREG(0); TO(nd, MXYZW);                 /* 12 ibeq vi4 */
    T; r = MADD(R(18), BC(27, W)); MAC(MXYZW); TO(21, MXYZW);                                /* 13 */
    if (t) {
        FLUSH;
        return;
    }
    T; r = MUL(R(12), BC(4, X)); MAC(MXYZW); LQ(15, MXYZW, pr, 4); TOACC(MXYZW);             /* 14 */
    T; r = MADD(R(13), BC(4, Y)); MAC(MXYZW); LQ(16, MXYZW, pr + 1, 4); TOACC(MXYZW);        /* 15 */
    T; r = MADD(R(14), BC(4, Z)); MAC(MXYZW); LQ(17, MXYZW, pr + 2, 4); TO(19, MXYZW);       /* 16 */
    T; r = ADD(R(22), R(21)); MAC(MXYZW); LQ(18, MXYZW, pr + 3, 4); TO(22, MXYZW);           /* 17 */
    T; r = MUL(R(15), BC(28, X)); MAC(MXYZW); LQ(12, MXYZW, nr, 5); TOACC(MXYZW);            /* 18 */
    T; r = MADD(R(16), BC(28, Y)); MAC(MXYZW); LQ(13, MXYZW, nr + 1, 5); TOACC(MXYZW);       /* 19 */
    T; r = MADD(R(17), BC(28, Z)); MAC(MXYZW); LQ(14, MXYZW, nr + 2, 5); TOACC(MXYZW);       /* 1A */
    T; r = ADD(R(nd), R(19)); MAC(MXYZW); t = IREG(5) == IREG(0); TO(nd, MXYZW);                 /* 1B ibeq vi5 */
    T; r = MADD(R(18), BC(28, W)); MAC(MXYZW); TO(21, MXYZW);                                /* 1C */
    if (t) {
        FLUSH;
        return;
    }
    T; r = MUL(R(12), BC(5, X)); MAC(MXYZW); LQ(15, MXYZW, pr, 5); TOACC(MXYZW);             /* 1D */
    T; r = MADD(R(13), BC(5, Y)); MAC(MXYZW); LQ(16, MXYZW, pr + 1, 5); TOACC(MXYZW);        /* 1E */
    T; r = MADD(R(14), BC(5, Z)); MAC(MXYZW); LQ(17, MXYZW, pr + 2, 5); TO(19, MXYZW);       /* 1F */
    T; r = ADD(R(22), R(21)); MAC(MXYZW); LQ(18, MXYZW, pr + 3, 5); TO(22, MXYZW);           /* 20 */
    T; r = MUL(R(15), BC(29, X)); MAC(MXYZW); TOACC(MXYZW);                                  /* 21 */
    T; r = MADD(R(16), BC(29, Y)); MAC(MXYZW); TOACC(MXYZW);                                 /* 22 */
    T; r = ADD(R(nd), R(19)); MAC(MXYZW); TO(nd, MXYZW);                                     /* 23 */
    T; r = MADD(R(17), BC(29, Z)); MAC(MXYZW); TOACC(MXYZW);                                 /* 24 */
    T; r = MADD(R(18), BC(29, W)); MAC(MXYZW); TO(21, MXYZW);                                /* 25 */
    FLUSH;
}

/* skin_diffuse (0x000; skin_diffuse_world, 0x04A of the character select's packet, is the same words: the programs
   use no absolute address), from blend_done (0x026). Returns where it stopped, from the program's first slot. */
static inline __attribute__((always_inline)) uint32_t n0_diffuse(N0 *s) {
    V4 r;
    int t;

    n0_blend(s, 20, 8, 0);
    T; LOI(0x43000000u); r = ADD(R(1), BC(0, W)); MAC(MXYZW); TO(1, MXYZW);                  /* 26 */
    T;                                                                                      /* 27 */
    T; r = v4_max(R(20), BC(0, X)); TO(20, MXYZ);                                           /* 28 */
    T; r = ADD(R(22), R(21)); MAC(MXYZW); LQ(31, MXYZW, 0, 0); TO(22, MXYZW);                /* 29 */
    T; r = MUL(R(0), IB); MAC(MW); TO(20, MW);                                              /* 2A */
    T;                                                                                      /* 2B */
    T; r = MUL(R(8), BC(20, X)); MAC(MXYZW); TOACC(MXYZW);                                   /* 2C */
    T; r = MADD(R(9), BC(20, Y)); MAC(MXYZW); DIV_Q(0, W, 22, W); TOACC(MXYZW);              /* 2D */
    T; r = MADD(R(10), BC(20, Z)); MAC(MXYZW); MOVE(7, Z, 22); TOACC(MXYZW);                 /* 2E */
    T; r = MADD(R(11), BC(0, W)); MAC(MXYZW); TO(19, MXYZW);                                 /* 2F */
    T; r = SUB(R(22), R(7)); MAC(MW); TO(22, MW);                                           /* 30 */
    T;                                                                                      /* 31 */
    T;                                                                                      /* 32 */
    T; r = v4_min(R(19), BC(31, W)); TO(20, MXYZ);                                          /* 33 */
    T; r = MUL(R(22), QB); MAC(MXYZ); DIV_Q(0, W, 22, W); TO(22, MXYZ);                     /* 34 */
    T;                                                                                      /* 35 */
    T;                                                                                      /* 36 */
    T; r = SUB(R(0), BC(0, W)); MAC(MXYZ); TOACC(MXYZ);                                     /* 37 */
    T; r = MADD(R(22), R(31)); MAC(MXYZ); TO(23, MXYZ);                                     /* 38 */
    T; r = v4_ftoi(R(20), 0); TO(20, MXYZW);                                                /* 39 */
    T; MOVE(22, W, 0);                                                                      /* 3A */
    T; r = MUL(R(7), QB); MAC(MZ); TO(22, MZ);                                              /* 3B */
    T; n0_clip(s, 23, 0);                                                                   /* 3C */
    T;                                                                                      /* 3D */
    T;                                                                                      /* 3E */
    T; r = v4_ftoi(R(22), 4); TO(22, MXYZW);                                                /* 3F */
    T; FCAND(0x3FFFF);                                                                      /* 40 */
    T; t = IREG(1) == IREG(0);                                                                  /* 41 ibeq vi1, done */
    T;                                                                                      /* 42 */
    if (!t) {
        T; LQ(22, MW, 1, 0);                                                                /* 43 */
        T;                                                                                  /* 44 */
    }
    T;                                                                                      /* 45 [E] */
    T;                                                                                      /* 46 */
    FLUSH;
    return 0x47;
}

/* skin_specular (0x04A), from blend_done (0x070). */
static inline __attribute__((always_inline)) uint32_t n0_specular(N0 *s) {
    V4 r;
    int t;

    n0_blend(s, 25, 4, 0);
    T; r = ADD(R(1), BC(0, W)); MAC(MXYZW); LQ(12, MXYZW, 8, 0); TO(1, MXYZW);               /* 70 */
    T; LQ(13, MXYZW, 9, 0);                                                                 /* 71 */
    T; r = MUL(R(25), R(25)); MAC(MYZ); LQ(14, MXYZW, 10, 0); TO(2, MYZ);                    /* 72 */
    T; LOI(0x43000000u); r = MUL(R(12), BC(25, X)); MAC(MXYZW); TOACC(MXYZW);                /* 73 */
    T; r = MADD(R(13), BC(25, Y)); MAC(MXYZW); TOACC(MXYZW);                                 /* 74 */
    T; r = MADD(R(14), BC(25, Z)); MAC(MXYZ); TO(20, MXYZ);                                  /* 75 */
    T;                                                                                      /* 76 */
    T; r = MUL(R(25), BC(25, X)); MAC(MX); LQ(26, MXYZW, 11, 0); TOACC(MX);                  /* 77 */
    T; r = MADD(R(1), BC(2, Y)); MAC(MX); ISET(8, IREG(0) + 0x80); TOACC(MX);                 /* 78 */
    T; r = MADD(R(1), BC(2, Z)); MAC(MX); FMAND(7, 8); TO(2, MX);                            /* 79 */
    T; r = ADD(R(22), R(21)); MAC(MXYZW); t = IREG(7) != IREG(0); TO(22, MXYZW);                 /* 7A ibne vi7, normal_only */
    T;                                                                                      /* 7B */
    if (t) {
        goto normal_only; /* (its only way in) */
    }
    T;                                                                                      /* 7C */
    T;                                                                                      /* 7D */
    T; RSQRT_Q(0, W, 2, X);                                                                 /* 7E */
    T; r = v4_max(R(20), BC(0, X)); TO(20, MXYZ);                                           /* 7F */
    T; r = MUL(R(0), IB); MAC(MW); LQ(31, MXYZW, 0, 0); TO(20, MW);                          /* 80 */
    T; ISET(9, IREG(0) + IREG(6));                                                             /* 81 */
    T;                                                                                      /* 82 */
    T; r = MUL(R(8), BC(20, X)); MAC(MXYZW); TOACC(MXYZW);                                   /* 83 */
    T; r = MADD(R(9), BC(20, Y)); MAC(MXYZW); TOACC(MXYZW);                                  /* 84 */
    T; r = MADD(R(10), BC(20, Z)); MAC(MXYZW); TOACC(MXYZW);                                 /* 85 */
    T; r = MADD(R(11), BC(0, W)); MAC(MXYZ); TO(20, MXYZ);                                   /* 86 */
    T;                                                                                      /* 87 */
    T;                                                                                      /* 88 */
    T;                                                                                      /* 89 */
    T; r = v4_min(R(20), BC(31, W)); TO(20, MXYZ);                                          /* 8A */
    T; r = MUL(R(25), QB); MAC(MXYZ); DIV_Q(0, W, 22, W); TO(25, MXYZ);                     /* 8B */
    T;                                                                                      /* 8C */
    T; r = SUB(R(22), R(7)); MAC(MW); TO(22, MW);                                           /* 8D */
    T; r = v4_ftoi(R(20), 0); MOVE(7, Z, 22); TO(20, MXYZW);                                /* 8E */
    T; r = MUL(R(25), R(26)); MAC(MYZ); TO(16, MYZ);                                        /* 8F */
    T;                                                                                      /* 90 */
    T; r = ADD(R(25), R(25)); MAC(MXYZ); TO(17, MXYZ);                                      /* 91 */
    T; r = MUL(R(25), BC(26, X)); MAC(MX); DIV_Q(0, W, 22, W); TOACC(MX);                    /* 92 */
    T; r = MADD(R(1), BC(16, Y)); MAC(MX); TOACC(MX);                                        /* 93 */
    T; r = MADD(R(1), BC(16, Z)); MAC(MX); TO(16, MX);                                       /* 94 */
    T; MOVE(22, W, 0);                                                                      /* 95 */
    T; r = MUL(R(22), QB); MAC(MXYZ); TO(22, MXYZ);                                         /* 96 */
    T; r = SUB(R(0), R(26)); MAC(MXYZ); TOACC(MXYZ);                                        /* 97 */
    T; r = MADD(R(17), BC(16, X)); MAC(MXYZ); TO(16, MXYZ);                                  /* 98 */
    T; r = SUB(R(0), BC(0, W)); MAC(MXYZ); TOACC(MXYZ);                                     /* 99 */
    T; r = MADD(R(22), R(31)); MAC(MXYZ); TO(23, MXYZ);                                     /* 9A */
    T; r = MUL(R(7), QB); MAC(MZ); TO(22, MZ);                                              /* 9B */
    T; r = MUL(R(12), BC(16, X)); MAC(MX); TOACC(MX);                                        /* 9C */
    T; r = MADD(R(13), BC(16, Y)); MAC(MX); ISET(7, IREG(0) + 0xFF); TOACC(MX);               /* 9D */
    T; r = MADD(R(14), BC(16, Z)); MAC(MX); ISET(9, IREG(6) & IREG(7)); TO(24, MX);             /* 9E */
    T; n0_clip(s, 23, 0); ISET(9, IREG(9) - 1);                                              /* 9F */
    T; r = MUL(R(12), BC(25, X)); MAC(MXYZW); TOACC(MXYZW);                                  /* A0 */
    T; r = MADD(R(13), BC(25, Y)); MAC(MXYZW); TOACC(MXYZW);                                 /* A1 */
    T; r = MADD(R(14), BC(25, Z)); MAC(MXYZ); FMAND(7, 8); TO(18, MXYZ);                     /* A2 */
    T; r = v4_ftoi(R(22), 4); FCAND(0x3FFFF); TO(22, MXYZW);                                /* A3 */
    T; r = MUL(R(24), R(24)); MAC(MX); t = IREG(1) != IREG(0); TO(24, MX);                      /* A4 ibne vi1, hide */
    T;                                                                                      /* A5 */
    FLUSH;
    if (t) {
        goto hide;
    }
    T; r = v4_max(R(18), BC(0, X)); t = IREG(7) != IREG(0); TO(18, MXYZ);                       /* A6 ibne vi7, done */
    T;                                                                                      /* A7 */
    FLUSH;
    if (t) {
        goto done;
    }
    T; t = IREG(9) == IREG(0);                                                                  /* A8 ibeq vi9, spec_add */
    T;                                                                                      /* A9 */
    FLUSH;
    if (!t) {
        do {
            T; r = MUL(R(24), R(24)); MAC(MX); ISET(9, IREG(9) - 1); TO(24, MX);             /* AA spec_pow */
            T;                                                                              /* AB */
            T; t = IREG(9) != IREG(0);                                                          /* AC ibne vi9, spec_pow */
            T;                                                                              /* AD */
            FLUSH;
        } while (t);
    }
    T; r = ADD(R(0), R(18)); MAC(MX); TOACC(MX);                                            /* AE spec_add */
    T; r = MADD(R(24), BC(6, X)); MAC(MX); TO(18, MX);                                       /* AF */
    T;                                                                                      /* B0 */
    T;                                                                                      /* B1 */
    T;                                                                                      /* B2 */
    T; r = MUL(R(8), BC(18, X)); MAC(MXYZW); TOACC(MXYZW);                                   /* B3 */
    T; r = MADD(R(9), BC(18, Y)); MAC(MXYZW); TOACC(MXYZW);                                  /* B4 */
    T; r = MADD(R(10), BC(18, Z)); MAC(MXYZW); TOACC(MXYZW);                                 /* B5 */
    T; r = MADD(R(11), BC(0, W)); MAC(MXYZW); TO(19, MXYZW);                                 /* B6 */
    T;                                                                                      /* B7 */
    T;                                                                                      /* B8 */
    T; r = MUL(R(0), IB); MAC(MW); TO(20, MW);                                              /* B9 */
    T; r = v4_min(R(19), BC(31, W)); TO(20, MXYZ);                                          /* BA */
    T;                                                                                      /* BB */
    T;                                                                                      /* BC */
    T;                                                                                      /* BD */
    T; r = v4_ftoi(R(20), 0); TO(20, MXYZW);                                                /* BE */
    T;                                                                                      /* BF */
    T;                                                                                      /* C0 [E] */
    T;                                                                                      /* C1 */
    FLUSH;
    return 0xC2 - 0x4A;
normal_only:
    T; r = v4_max(R(20), BC(0, X)); TO(20, MXYZ);                                           /* C2 */
    T; r = MUL(R(0), IB); MAC(MW); LQ(31, MXYZW, 0, 0); TO(20, MW);                          /* C3 */
    T; DIV_Q(0, W, 22, W);                                                                  /* C4 */
    T; MOVE(7, Z, 22);                                                                      /* C5 */
    T; r = SUB(R(22), R(7)); MAC(MW); TO(22, MW);                                           /* C6 */
    T; r = MUL(R(8), BC(20, X)); MAC(MXYZW); TOACC(MXYZW);                                   /* C7 */
    T; r = MADD(R(9), BC(20, Y)); MAC(MXYZW); TOACC(MXYZW);                                  /* C8 */
    T; r = MADD(R(10), BC(20, Z)); MAC(MXYZW); TOACC(MXYZW);                                 /* C9 */
    T; r = MADD(R(11), BC(0, W)); MAC(MXYZW); TO(19, MXYZW);                                 /* CA */
    T; r = MUL(R(22), QB); MAC(MXYZ); DIV_Q(0, W, 22, W); TO(22, MXYZ);                     /* CB */
    T;                                                                                      /* CC */
    T; r = SUB(R(0), BC(0, W)); MAC(MXYZ); TOACC(MXYZ);                                     /* CD */
    T; r = v4_min(R(19), BC(31, W)); MOVE(22, W, 0); TO(20, MXYZ);                          /* CE */
    T; r = MADD(R(22), R(31)); MAC(MXYZ); TO(23, MXYZ);                                     /* CF */
    T;                                                                                      /* D0 */
    T;                                                                                      /* D1 */
    T; r = MUL(R(7), QB); MAC(MZ); TO(22, MZ);                                              /* D2 */
    T; n0_clip(s, 23, 0);                                                                   /* D3 */
    T; r = v4_ftoi(R(22), 4); TO(22, MXYZW);                                                /* D4 */
    T; r = v4_ftoi(R(20), 0); TO(20, MXYZW);                                                /* D5 */
    T;                                                                                      /* D6 */
    T; FCAND(0x3FFFF);                                                                      /* D7 */
    T; t = IREG(1) == IREG(0);                                                                  /* D8 ibeq vi1, done */
    T;                                                                                      /* D9 */
    FLUSH;
    if (t) {
        goto done;
    }
hide:
    T; LQ(22, MW, 1, 0);                                                                    /* DA */
    T;                                                                                      /* DB */
done:
    T;                                                                                      /* DC [E] */
    T;                                                                                      /* DD */
    FLUSH;
    return 0xDE - 0x4A;
}

/* skin_gloss (0x0EA), from blend_done (0x110). */
static inline __attribute__((always_inline)) uint32_t n0_gloss(N0 *s) {
    V4 r;
    int t;

    n0_blend(s, 25, 4, 4);
    T; r = ADD(R(1), BC(0, W)); MAC(MXYZW); LQ(12, MXYZW, 8, 0); TO(1, MXYZW);               /* 110 */
    T; r = MUL(R(0), BC(0, X)); MAC(MW); LQ(13, MXYZW, 9, 0); TO(3, MW);                     /* 111 */
    T; r = MUL(R(25), R(25)); MAC(MYZ); LQ(14, MXYZW, 10, 0); TO(2, MYZ);                    /* 112 */
    T; r = ADD(R(22), R(21)); MAC(MXYZW); LQ(15, MXYZW, 4, 0); TO(24, MXYZW);                /* 113 */
    T; r = MUL(R(12), BC(25, X)); MAC(MXYZW); LQ(16, MXYZW, 5, 0); TOACC(MXYZW);             /* 114 */
    T; r = MADD(R(13), BC(25, Y)); MAC(MXYZW); LQ(17, MXYZW, 6, 0); TOACC(MXYZW);            /* 115 */
    T; r = MADD(R(14), BC(25, Z)); MAC(MXYZ); LQ(18, MXYZW, 7, 0); TO(20, MXYZ);             /* 116 */
    T; r = MUL(R(25), BC(25, X)); MAC(MX); LQ(26, MXYZW, 11, 0); TOACC(MX);                  /* 117 */
    T; LOI(0x43000000u); r = MADD(R(1), BC(2, Y)); MAC(MX); TOACC(MX);                       /* 118 */
    T; r = MADD(R(1), BC(2, Z)); MAC(MX); ISET(8, IREG(0) + 0x80); TO(2, MX);                 /* 119 */
    T; r = MUL(R(15), BC(24, X)); MAC(MXYZW); FMAND(7, 8); TOACC(MXYZW);                     /* 11A */
    T; r = MADD(R(16), BC(24, Y)); MAC(MXYZW); t = IREG(7) != IREG(0); TOACC(MXYZW);             /* 11B ibne vi7, facing_away */
    T; r = MADD(R(17), BC(24, Z)); MAC(MXYZW); TOACC(MXYZW);                                 /* 11C */
    if (t) {
        T; n0_clip(s, 15, 0);                                                               /* 15B facing_away */
        FLUSH;
        goto hide;
    }
    T; r = MADD(R(18), BC(0, W)); MAC(MXYZW); TO(22, MXYZW);                                 /* 11D */
    T;                                                                                      /* 11E */
    T; RSQRT_Q(0, W, 2, X);                                                                 /* 11F */
    T;                                                                                      /* 120 */
    T; LQ(31, MXYZW, 0, 0);                                                                 /* 121 */
    T; ISET(9, IREG(0) + IREG(6));                                                             /* 122 */
    T; r = MUL(R(0), BC(0, X)); MAC(MXYZW); TO(20, MXYZW);                                   /* 123 */
    T;                                                                                      /* 124 */
    T;                                                                                      /* 125 */
    T;                                                                                      /* 126 */
    T; r = v4_ftoi(R(20), 0); TO(20, MXYZW);                                                /* 127 */
    T;                                                                                      /* 128 */
    T;                                                                                      /* 129 */
    T;                                                                                      /* 12A */
    T;                                                                                      /* 12B */
    T; r = MUL(R(25), QB); MAC(MXYZ); DIV_Q(0, W, 22, W); TO(25, MXYZ);                     /* 12C */
    T;                                                                                      /* 12D */
    T; r = SUB(R(22), R(7)); MAC(MW); TO(22, MW);                                           /* 12E */
    T; MOVE(7, Z, 22);                                                                      /* 12F */
    T; r = MUL(R(25), R(26)); MAC(MYZ); TO(16, MYZ);                                        /* 130 */
    T;                                                                                      /* 131 */
    T; r = ADD(R(25), R(25)); MAC(MXYZ); TO(17, MXYZ);                                      /* 132 */
    T; r = MUL(R(25), BC(26, X)); MAC(MX); DIV_Q(0, W, 22, W); TOACC(MX);                    /* 133 */
    T; r = MADD(R(1), BC(16, Y)); MAC(MX); TOACC(MX);                                        /* 134 */
    T; r = MADD(R(1), BC(16, Z)); MAC(MX); TO(16, MX);                                       /* 135 */
    T; MOVE(22, W, 0);                                                                      /* 136 */
    T; r = MUL(R(22), QB); MAC(MXYZ); TO(22, MXYZ);                                         /* 137 */
    T; r = SUB(R(0), R(26)); MAC(MXYZ); TOACC(MXYZ);                                        /* 138 */
    T; r = MADD(R(17), BC(16, X)); MAC(MXYZ); TO(16, MXYZ);                                  /* 139 */
    T; r = SUB(R(0), BC(0, W)); MAC(MXYZ); TOACC(MXYZ);                                     /* 13A */
    T; r = MADD(R(22), R(31)); MAC(MXYZ); TO(23, MXYZ);                                     /* 13B */
    T; r = MUL(R(7), QB); MAC(MZ); TO(22, MZ);                                              /* 13C */
    T; r = MUL(R(12), BC(16, X)); MAC(MX); TOACC(MX);                                        /* 13D */
    T; r = MADD(R(13), BC(16, Y)); MAC(MX); ISET(7, IREG(0) + 0xFF); TOACC(MX);               /* 13E */
    T; r = MADD(R(14), BC(16, Z)); MAC(MX); ISET(9, IREG(6) & IREG(7)); TO(24, MX);             /* 13F */
    T; n0_clip(s, 23, 0); ISET(9, IREG(9) - 1);                                              /* 140 */
    T; r = v4_ftoi(R(22), 4); TO(22, MXYZW);                                                /* 141 */
    T;                                                                                      /* 142 */
    T; r = MUL(R(24), R(24)); MAC(MX); FMAND(7, 8); TO(24, MX);                             /* 143 */
    T; FCAND(0x3FFFF);                                                                      /* 144 */
    T; t = IREG(1) != IREG(0);                                                                  /* 145 ibne vi1, hide */
    T;                                                                                      /* 146 */
    FLUSH;
    if (t) {
        goto hide;
    }
    T; t = IREG(7) != IREG(0);                                                                  /* 147 ibne vi7, done */
    T; r = MUL(R(8), BC(6, X)); MAC(MXYZW); TO(2, MXYZW);                                    /* 148 */
    FLUSH;
    if (t) {
        goto done;
    }
    T; t = IREG(9) == IREG(0);                                                                  /* 149 ibeq vi9, spec_colour */
    T;                                                                                      /* 14A */
    FLUSH;
    if (!t) {
        do {
            T; r = MUL(R(24), R(24)); MAC(MX); ISET(9, IREG(9) - 1); TO(24, MX);             /* 14B spec_pow */
            T;                                                                              /* 14C */
            T; t = IREG(9) != IREG(0);                                                          /* 14D ibne vi9, spec_pow */
            T;                                                                              /* 14E */
            FLUSH;
        } while (t);
    }
    T; r = MUL(R(2), BC(24, X)); MAC(MXYZ); TO(19, MXYZ);                                    /* 14F spec_colour */
    T;                                                                                      /* 150 */
    T;                                                                                      /* 151 */
    T; r = MUL(R(0), IB); MAC(MW); TO(20, MW);                                              /* 152 */
    T; r = v4_min(R(19), BC(31, W)); TO(20, MXYZ);                                          /* 153 */
    T;                                                                                      /* 154 */
    T;                                                                                      /* 155 */
    T;                                                                                      /* 156 */
    T; r = v4_ftoi(R(20), 0); TO(20, MXYZW);                                                /* 157 */
    T;                                                                                      /* 158 */
    T;                                                                                      /* 159 [E] */
    T;                                                                                      /* 15A */
    FLUSH;
    return 0x15B - 0xEA;
hide:
    T; LQ(22, MW, 1, 0);                                                                    /* 15C */
    T;                                                                                      /* 15D */
done:
    T;                                                                                      /* 15E [E] */
    T;                                                                                      /* 15F */
    FLUSH;
    return 0x160 - 0xEA;
}

/* skin_envmap (0x186), from blend_done (0x1AC). */
static inline __attribute__((always_inline)) uint32_t n0_envmap(N0 *s) {
    V4 r;
    int t;

    n0_blend(s, 25, 4, 0);
    T; LOI(0x43000000u); r = ADD(R(1), BC(0, W)); MAC(MXYZW); TO(1, MXYZW);                  /* 1AC */
    T; LQ(26, MXYZW, 11, 0);                                                                /* 1AD */
    T; r = MUL(R(25), R(25)); MAC(MYZ); TO(12, MYZ);                                        /* 1AE */
    T; r = ADD(R(22), R(21)); MAC(MXYZW); LQ(31, MXYZW, 0, 0); TO(22, MXYZW);                /* 1AF */
    T; r = MUL(R(1), IB); MAC(MXYZW); TO(20, MXYZW);                                        /* 1B0 */
    T; r = MUL(R(25), BC(25, X)); MAC(MX); TOACC(MX);                                        /* 1B1 */
    T; r = MADD(R(1), BC(12, Y)); MAC(MX); TOACC(MX);                                        /* 1B2 */
    T; r = MADD(R(1), BC(12, Z)); MAC(MX); DIV_Q(0, W, 22, W); TO(12, MX);                   /* 1B3 */
    T; r = v4_ftoi(R(20), 0); LQ(15, MXYZW, 252, 0); TO(20, MXYZW);                         /* 1B4 */
    T; r = SUB(R(22), R(7)); MAC(MW); LQ(16, MXYZW, 253, 0); TO(19, MW);                    /* 1B5 */
    T; r = ADD(R(0), R(22)); MAC(MZ); LQ(17, MXYZW, 254, 0); TO(7, MZ);                     /* 1B6 */
    T; LQ(18, MXYZW, 255, 0);                                                               /* 1B7 */
    T; LQ(13, MZ, 1, 0);                                                                    /* 1B8 */
    T;                                                                                      /* 1B9 */
    T; r = MUL(R(22), QB); MAC(MXYZ); RSQRT_Q(0, W, 12, X); TO(22, MXYZ);                   /* 1BA */
    T; r = MUL(R(1), QB); MAC(MW); TO(13, MW);                                              /* 1BB */
    T;                                                                                      /* 1BC */
    T;                                                                                      /* 1BD */
    T; r = MUL(R(22), R(31)); MAC(MXYZ); TOACC(MXYZ);                                       /* 1BE */
    T; r = MSUB(R(1), BC(1, W)); MAC(MXYZ); TO(23, MXYZ);                                   /* 1BF */
    T; MOVE(22, W, 0);                                                                      /* 1C0 */
    T;                                                                                      /* 1C1 */
    T;                                                                                      /* 1C2 */
    T; n0_clip(s, 23, 0);                                                                   /* 1C3 */
    T; r = v4_ftoi(R(22), 4); TO(22, MW);                                                   /* 1C4 */
    T;                                                                                      /* 1C5 */
    T; r = ADD(R(0), R(26)); MAC(MXYZ); TOACC(MXYZ);                                        /* 1C6 */
    T; r = MADD(R(25), QB); MAC(MXYZ); DIV_Q(0, W, 19, W); TO(25, MXYZ);                    /* 1C7 */
    T; r = MUL(R(13), BC(13, W)); MAC(MZ); FCAND(0x3FFFF); TO(13, MZ);                      /* 1C8 */
    T; r = MUL(R(1), BC(13, W)); MAC(MZ); TO(24, MZ);                                       /* 1C9 */
    T; r = ADD(R(0), BC(1, W)); MAC(MXYZW); TOACC(MXYZW);                                   /* 1CA */
    T; r = MADD(R(15), BC(25, X)); MAC(MXYZW); TOACC(MXYZW);                                 /* 1CB */
    T; r = MADD(R(16), BC(25, Y)); MAC(MXYZW); TOACC(MXYZW);                                 /* 1CC */
    T; r = MADD(R(17), BC(25, Z)); MAC(MXY); TO(24, MXY);                                   /* 1CD */
    T; r = MUL(R(7), QB); MAC(MZ); TO(22, MZ);                                              /* 1CE */
    T;                                                                                      /* 1CF */
    T;                                                                                      /* 1D0 */
    T; r = MUL(R(24), BC(13, Z)); MAC(MXY); t = IREG(1) == IREG(0); TO(24, MXY);                /* 1D1 ibeq vi1, done */
    T; r = v4_ftoi(R(22), 4); TO(22, MXYZ);                                                 /* 1D2 */
    if (!t) {
        T; LQ(22, MW, 1, 0);                                                                /* 1D3 */
        T;                                                                                  /* 1D4 */
    }
    T;                                                                                      /* 1D5 [E] */
    T;                                                                                      /* 1D6 */
    FLUSH;
    return 0x1D7 - 0x186;
}

/* skin_hide (0x0EA of the character select's packet): XYZF.w = the ADC word. */
static inline __attribute__((always_inline)) uint32_t n0_hide(N0 *s) {
    T; LQ(22, MW, 1, 0);                                                                    /* EA */
    T;                                                                                      /* EB */
    T;                                                                                      /* EC [E] */
    T;                                                                                      /* ED */
    FLUSH;
    return 4;
}

#undef T
#undef R
#undef BC
#undef QB
#undef IB
#undef ACC
#undef MUL
#undef ADD
#undef SUB
#undef MADD
#undef MSUB
#undef MAC
#undef TO
#undef TOACC
#undef LQ
#undef LOI
#undef IREG
#undef ISET
#undef MOVE
#undef DIV_Q
#undef RSQRT_Q
#undef FCAND
#undef FMAND
#undef FLUSH
