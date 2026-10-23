/*
 * The vector units' predecoded interpreter (Vu_Run with u->ops; T30 batch 2): the same machine as Vu_RunRef
 * (vu_micro.c), with the work moved out of the per-instruction path.
 *   - Each micro-memory slot is decoded once into a VuOp (u->ops); the raw words are compared at each fetch and a
 *     slot whose words changed (MPG, or any other write to micro memory) is decoded again.
 *   - One dispatch per half: the upper on its operation and second operand together (the operations with MAC
 *     flags queue them in their case), the lower on its operation.
 *   - The upper instruction runs on four lanes at once (vu_simd_inl.h), the lanes outside the dest mask computed
 *     and dropped; add, multiply, divide, square root and the conversions on the SSE unit in round-toward-zero /
 *     flush-to-zero mode (V4_MXCSR, set for the program and put back around XGKICK and at its end), which computes
 *     the EE float model exactly.
 *   - Q, P, the clipping and the MAC flags are brought up to the current step only where an instruction reads or
 *     replaces them and at the end of the program, instead of at every step: each update depends only on the step
 *     number, so doing it late gives the values doing it at every step would. A flag queue that fills meanwhile
 *     drops its oldest entry, which has arrived already (at most four entries are on their way, sent by the last
 *     four instructions), and a later arrived entry replaces it anyway.
 *   - vf0 / vi0 are reset after each instruction as in Vu_RunRef; the next pc is pc + 1 without looking at the
 *     branch state unless the instruction or the one before branches or ends.
 * Vu_RunRef stays the specification: TTT_SELFTEST=vu1 (vu_fasttest.c) runs random programs through both, and
 * TTT_VU_CHECK=1 (gs_vu1.c) every program of the game.
 */
#include <stdint.h>
#include <string.h>
#define memcpy __builtin_memcpy /* -fno-builtin: inline the small copies of the hot paths */
#include "vu_micro_inl.h"
#include "vu_simd_inl.h"

enum { U_NOP, U_ADD, U_SUB, U_MUL, U_MADD, U_MSUB, U_MAX, U_MIN, U_ITOF, U_FTOI, U_ABS, U_CLIP, U_OPMULA, U_OPMSUB,
       U_UNKNOWN };
enum { B_VEC, B_BC, B_Q, B_I };
enum { T_NONE, T_FD, T_FT, T_ACC };
enum { F_IBIT = 1, F_EBIT = 2, F_CTL = 4 /* E bit or a branch: the next pc is not simply pc + 1 */ };
/* what the run loop dispatches on (VuOp.up after decoding): the operations alone, and each arithmetic operation with
   each kind of second operand */
enum { K_NOP, K_UNKNOWN, K_ITOF, K_FTOI, K_ABS, K_CLIP, K_OPMULA, K_OPMSUB, K_ARITH };
#define K_OF(op, bk) (K_ARITH + ((op) - U_ADD) * 4 + (bk))
enum { L_NONE, L_LQ, L_SQ, L_ILW, L_ISW, L_IADDIU, L_ISUBIU, L_FCEQ, L_FCSET, L_FCAND, L_FCOR, L_FMEQ, L_FMAND, L_FMOR,
       L_FCGET, L_B, L_BAL, L_JR, L_JALR, L_IBEQ, L_IBNE, L_IBLTZ, L_IBGTZ, L_IBLEZ, L_IBGEZ, L_IADD, L_ISUB, L_IADDI,
       L_IAND, L_IOR, L_MOVE, L_MR32, L_LQI, L_SQI, L_LQD, L_SQD, L_DIV, L_SQRT, L_RSQRT, L_WAITQ, L_MTIR, L_MFIR,
       L_ILWR, L_ISWR, L_MFP, L_XTOP, L_XITOP, L_XGKICK, L_ESADD, L_ERSADD, L_ELENG, L_ERLENG, L_EATANXY, L_EATANXZ,
       L_ESUM, L_ESQRT, L_ERSQRT, L_ERCPR, L_WAITP, L_EATAN, L_UNKNOWN };

/* upper: operation, second operand and target of the opcodes below 0x3C and of the 0x3C-0x3F group (index sp) */
static void decode_upper(VuOp *o, uint32_t hi) {
    static const uint8_t kOp[0x30] = {
        U_ADD, U_ADD, U_ADD, U_ADD, U_SUB, U_SUB, U_SUB, U_SUB, U_MADD, U_MADD, U_MADD, U_MADD, U_MSUB, U_MSUB, U_MSUB,
        U_MSUB, U_MAX, U_MAX, U_MAX, U_MAX, U_MIN, U_MIN, U_MIN, U_MIN, U_MUL, U_MUL, U_MUL, U_MUL, U_MUL, U_MAX, U_MUL,
        U_MIN, U_ADD, U_MADD, U_ADD, U_MADD, U_SUB, U_MSUB, U_SUB, U_MSUB, U_ADD, U_MADD, U_MUL, U_MAX, U_SUB, U_MSUB,
        U_OPMSUB, U_MIN};
    uint32_t op = hi & 0x3F, sp = (((hi >> 6) & 31) << 2) | (hi & 3), k;

    o->tgt = T_NONE;
    if ((hi & 0x07FFFFFFu) == 0x000002FFu) {
        o->up = U_NOP;
        return;
    }
    k = op < 0x3C ? op : sp;
    if ((op < 0x3C && k >= 0x30) || (op >= 0x3C && (k >= 0x30 || k == 0x2B))) {
        o->up = U_UNKNOWN;
        o->bc = (uint8_t)(op < 0x3C ? op : 64 + (sp & 63));
        return;
    }
    if (op >= 0x3C && k == 0x2F) {
        o->up = U_NOP;
        return;
    }
    o->bk = B_BC;
    if (k >= 0x28) {
        o->bk = B_VEC;
    } else if (k >= 0x1C) { /* 0x1C q, 0x1D / 0x1E / 0x1F i; 0x20-0x27: even pairs q, odd pairs i */
        o->bk = k == 0x1C || (k >= 0x20 && ((k - 0x20) & 2) == 0) ? B_Q : B_I;
    }
    o->up = kOp[k];
    o->tgt = op < 0x3C ? T_FD : T_ACC;
    if (op >= 0x3C) {
        if (k >= 0x10 && k < 0x18) { /* ITOF / FTOI 0 / 4 / 12 / 15 */
            o->up = k < 0x14 ? U_ITOF : U_FTOI;
            o->bc = (uint8_t)((k & 3) == 0 ? 0 : (k & 3) == 1 ? 4 : (k & 3) == 2 ? 12 : 15);
            o->tgt = T_FT;
        } else if (k == 0x1D) {
            o->up = U_ABS;
            o->tgt = T_FT;
        } else if (k == 0x1F) {
            o->up = U_CLIP;
            o->tgt = T_NONE;
        } else if (k == 0x2E) {
            o->up = U_OPMULA;
        }
    }
    if (o->up == U_OPMULA || o->up == U_OPMSUB) {
        o->dest = 0xE;
        o->bk = B_VEC;
    }
    if ((o->tgt == T_FD && o->fd == 0) || (o->tgt == T_FT && o->ft == 0)) {
        o->tgt = T_NONE; /* vf0 is not written */
    }
}

static uint8_t dispatch_of(const VuOp *o) {
    switch (o->up) {
    case U_NOP: return K_NOP;
    case U_ITOF: return K_ITOF;
    case U_FTOI: return K_FTOI;
    case U_ABS: return K_ABS;
    case U_CLIP: return K_CLIP;
    case U_OPMULA: return K_OPMULA;
    case U_OPMSUB: return K_OPMSUB;
    case U_UNKNOWN: return K_UNKNOWN;
    default: return (uint8_t)K_OF(o->up, o->bk);
    }
}

static void decode_lower(VuOp *o, uint32_t lo, uint32_t pc, uint32_t mm) {
    uint32_t lop = lo >> 25, f = lo & 0x3F, sp = (((lo >> 6) & 31) << 2) | (lo & 3);
    int32_t imm11 = (int32_t)(lo << 21) >> 21;

    o->ld = (lo >> 21) & 15;
    o->is = (lo >> 11) & 31;
    o->it = (lo >> 16) & 31;
    o->id = (lo >> 6) & 31;
    o->fsf = (lo >> 21) & 3;
    o->ftf = (lo >> 23) & 3;
    o->imm = (uint32_t)imm11;
    o->aux = (pc + 1 + (uint32_t)imm11) & mm;
    switch (lop) {
    case 0x00: o->lw = L_LQ; return;
    case 0x01: o->lw = L_SQ; return;
    case 0x04: o->lw = L_ILW; return;
    case 0x05: o->lw = L_ISW; return;
    case 0x08: o->lw = L_IADDIU; o->imm = ((lo >> 10) & 0x7800) | (lo & 0x7FF); return;
    case 0x09: o->lw = L_ISUBIU; o->imm = ((lo >> 10) & 0x7800) | (lo & 0x7FF); return;
    case 0x10: o->lw = L_FCEQ; o->imm = lo & 0xFFFFFF; return;
    case 0x11: o->lw = L_FCSET; o->imm = lo & 0xFFFFFF; return;
    case 0x12: o->lw = L_FCAND; o->imm = lo & 0xFFFFFF; return;
    case 0x13: o->lw = L_FCOR; o->imm = lo & 0xFFFFFF; return;
    case 0x18: o->lw = L_FMEQ; return;
    case 0x1A: o->lw = L_FMAND; return;
    case 0x1B: o->lw = L_FMOR; return;
    case 0x1C: o->lw = L_FCGET; return;
    case 0x20: o->lw = L_B; return;
    case 0x21: o->lw = L_BAL; return;
    case 0x24: o->lw = L_JR; return;
    case 0x25: o->lw = L_JALR; return;
    case 0x28: o->lw = L_IBEQ; return;
    case 0x29: o->lw = L_IBNE; return;
    case 0x2C: o->lw = L_IBLTZ; return;
    case 0x2D: o->lw = L_IBGTZ; return;
    case 0x2E: o->lw = L_IBLEZ; return;
    case 0x2F: o->lw = L_IBGEZ; return;
    case 0x40: break;
    default: o->lw = L_UNKNOWN; o->imm = 128 + (lop & 63); return;
    }
    if (f < 0x3C) {
        switch (f) {
        case 0x30: o->lw = L_IADD; return;
        case 0x31: o->lw = L_ISUB; return;
        case 0x32: o->lw = L_IADDI; o->imm = (uint32_t)((int32_t)(lo << 21) >> 27); return;
        case 0x34: o->lw = L_IAND; return;
        case 0x35: o->lw = L_IOR; return;
        default: o->lw = L_UNKNOWN; o->imm = 128 + f; return;
        }
    }
    o->imm = 128 + 64 + (sp & 63); /* the unknown counter of XTOP / XITOP / XGKICK where the unit has none */
    switch (sp) {
    case 0x30: o->lw = L_MOVE; return;
    case 0x31: o->lw = L_MR32; return;
    case 0x34: o->lw = L_LQI; return;
    case 0x35: o->lw = L_SQI; return;
    case 0x36: o->lw = L_LQD; return;
    case 0x37: o->lw = L_SQD; return;
    case 0x38: o->lw = L_DIV; return;
    case 0x39: o->lw = L_SQRT; return;
    case 0x3A: o->lw = L_RSQRT; return;
    case 0x3B: o->lw = L_WAITQ; return;
    case 0x3C: o->lw = L_MTIR; return;
    case 0x3D: o->lw = L_MFIR; return;
    case 0x3E: o->lw = L_ILWR; return;
    case 0x3F: o->lw = L_ISWR; return;
    case 0x64: o->lw = L_MFP; return;
    case 0x68: o->lw = L_XTOP; return;
    case 0x69: o->lw = L_XITOP; return;
    case 0x6C: o->lw = L_XGKICK; return;
    case 0x70: o->lw = L_ESADD; return;
    case 0x71: o->lw = L_ERSADD; return;
    case 0x72: o->lw = L_ELENG; return;
    case 0x73: o->lw = L_ERLENG; return;
    case 0x74: o->lw = L_EATANXY; return;
    case 0x75: o->lw = L_EATANXZ; return;
    case 0x76: o->lw = L_ESUM; return;
    case 0x78: o->lw = L_ESQRT; return;
    case 0x79: o->lw = L_ERSQRT; return;
    case 0x7A: o->lw = L_ERCPR; return;
    case 0x7B: o->lw = L_WAITP; return;
    case 0x7D: o->lw = L_EATAN; return;
    default: o->lw = L_UNKNOWN; return;
    }
}

static void decode(VuOp *o, uint32_t pc, uint32_t mm, uint32_t lo, uint32_t hi) {
    o->lo = lo;
    o->hi = hi;
    o->dest = (hi >> 21) & 15;
    o->ft = (hi >> 16) & 31;
    o->fs = (hi >> 11) & 31;
    o->fd = (hi >> 6) & 31;
    o->bc = hi & 3;
    o->bk = B_VEC;
    o->flags = (uint8_t)(((hi >> 31) ? F_IBIT : 0) | ((hi & 0x40000000u) ? F_EBIT : 0));
    decode_upper(o, hi);
    if ((hi & 0x80000000u) || lo == 0x8000033Cu) {
        o->lw = L_NONE;
        o->aux = 0;
        o->imm = 0;
        o->ld = o->is = o->it = o->id = o->fsf = o->ftf = 0;
    } else {
        decode_lower(o, lo, pc, mm);
    }
    o->up = dispatch_of(o);
    if ((o->flags & F_EBIT) || (o->lw >= L_B && o->lw <= L_IBGEZ)) {
        o->flags |= F_CTL;
    }
}

/* Q / P / the flag queues brought up to step `cycle` (what Vu_RunRef does at the start of every step). */
static inline void q_now(VuCore *u, uint32_t cycle) {
    if (u->qReady != 0 && cycle >= u->qReady) {
        *u->q = u->qPending;
        u->qReady = 0;
    }
}
static inline void p_now(VuCore *u, uint32_t cycle) {
    if (u->pReady != 0 && cycle >= u->pReady) {
        u->p = u->pPending;
        u->pReady = 0;
    }
}
static inline void clip_now(VuCore *u, uint32_t cycle) {
    while (u->clipCount != 0 && u->clipQueue[u->clipHead].cycle <= cycle) {
        u->clip = u->clipQueue[u->clipHead].value;
        u->clipHead = (u->clipHead + 1) & 7;
        u->clipCount--;
    }
}
static inline void macq_now(VuCore *u, uint32_t cycle) {
    while (u->macCount != 0 && u->macQueue[u->macHead].cycle <= cycle) {
        u->macNow = u->macQueue[u->macHead];
        u->macLazy = 1;
        u->macHead = (u->macHead + 1) & 7;
        u->macCount--;
    }
}

#define VI(r) vi[(r) & 15]
#define VF(r) vf[(r)]
#define LANES(m) for (c = 0; c < 4; c++) if (((m) >> (3 - c)) & 1)
#define PSETF(v, lat)                                                                                                  \
    do {                                                                                                               \
        uint32_t pv_ = (v);                                                                                            \
        p_now(u, cycle);                                                                                               \
        u->pPending = pv_;                                                                                             \
        u->pReady = cycle + (lat);                                                                                     \
    } while (0)
#define QSETF(v, lat)                                                                                                  \
    do {                                                                                                               \
        uint32_t qv_ = (v);                                                                                            \
        q_now(u, cycle);                                                                                               \
        u->qPending = qv_;                                                                                             \
        u->qReady = cycle + (lat);                                                                                     \
    } while (0)
#define TAKE(t)                                                                                                        \
    do {                                                                                                               \
        branch = 2;                                                                                                    \
        target = (t);                                                                                                  \
    } while (0)

static uint32_t esum2(const uint32_t *v) { return vadd(vadd(vmul(v[0], v[0]), vmul(v[1], v[1])), vmul(v[2], v[2])); }

__attribute__((noinline, force_align_arg_pointer)) static int run_fast(VuCore *u, uint32_t start) {
    uint32_t (*const vf)[4] = u->vf;
    uint16_t *const vi = u->vi;
    const uint8_t *const micro = u->micro;
    VuOp *const ops = u->ops;
    const uint32_t mm = u->microMask;
    uint32_t pc = start & mm, cycle = u->cycle, n, c, target = 0;
    int ending = 0, branch = 0, rc = -1, mc = u->macCount;
    uint32_t mt = (uint32_t)(u->macHead + u->macCount) & 7; /* the MAC queue's next free entry and its length, kept
                                                               here and written back where it is read */
    const V4 one0 = _mm_set_epi32(0x3F800000, 0, 0, 0);
    const uint32_t csr = _mm_getcsr();

    if (!u->opsInit) {
        uint32_t k;
        for (k = 0; k <= mm; k++) {
            uint32_t lo, hi;
            memcpy(&lo, micro + k * 8, 4);
            memcpy(&hi, micro + k * 8 + 4, 4);
            decode(&ops[k], k, mm, lo, hi);
        }
        u->opsInit = 1;
    }
    _mm_setcsr(V4_MXCSR);
#define MACSYNC()                                                                                                      \
    do {                                                                                                               \
        u->macCount = mc;                                                                                              \
        u->macHead = (int)((mt - (uint32_t)mc) & 7);                                                                   \
        macq_now(u, cycle);                                                                                            \
        mc = u->macCount;                                                                                              \
    } while (0)
    for (n = 0; n < (1u << 22); n++) {
        VuOp *o = &ops[pc];
        uint32_t lo, hi, next = (pc + 1) & mm;
        V4 res = _mm_setzero_si128();

        memcpy(&lo, micro + pc * 8, 4);
        memcpy(&hi, micro + pc * 8 + 4, 4);
        if (lo != o->lo || hi != o->hi) {
            decode(o, pc, mm, lo, hi);
        }
        cycle++;
        if (o->flags & F_IBIT) {
            u->i = lo;
        }
        /* ---- upper: one dispatch on operation and second operand; the MAC flags' entry where it has one ---- */
#define MACPUSH()                                                                                                      \
    do {                                                                                                               \
        VuMacStep *st_ = &u->macQueue[mt];                                                                             \
        st_->cycle = cycle + 4;                                                                                        \
        st_->mask = o->dest;                                                                                           \
        V4_STORE(st_->res, res);                                                                                       \
        mt = (mt + 1) & 7;                                                                                             \
        mc += mc < 8;                                                                                                  \
    } while (0)
#define UCASE(OP, BK, B, EXPR, MAC)                                                                                    \
    case K_OF(OP, BK): {                                                                                               \
        V4 a = V4_LOAD(VF(o->fs)), b = B;                                                                              \
        res = EXPR;                                                                                                    \
        MAC;                                                                                                           \
        break;                                                                                                         \
    }
#define UCASES(OP, EXPR, MAC)                                                                                          \
    UCASE(OP, B_VEC, V4_LOAD(VF(o->ft)), EXPR, MAC)                                                                    \
    UCASE(OP, B_BC, V4_SET1(VF(o->ft)[o->bc]), EXPR, MAC)                                                              \
    UCASE(OP, B_Q, (q_now(u, cycle), V4_SET1(*u->q)), EXPR, MAC)                                                       \
    UCASE(OP, B_I, V4_SET1(u->i), EXPR, MAC)
        switch (o->up) {
        case K_NOP: break;
        UCASES(U_ADD, v4_add(a, b), MACPUSH())
        UCASES(U_SUB, v4_sub(a, b), MACPUSH())
        UCASES(U_MUL, v4_mul(a, b), MACPUSH())
        UCASES(U_MADD, v4_add(V4_LOAD(u->acc), v4_mul(a, b)), MACPUSH())
        UCASES(U_MSUB, v4_sub(V4_LOAD(u->acc), v4_mul(a, b)), MACPUSH())
        UCASES(U_MAX, v4_max(a, b), (void)0)
        UCASES(U_MIN, v4_min(a, b), (void)0)
        case K_OPMULA:
            res = v4_mul(_mm_shuffle_epi32(V4_LOAD(VF(o->fs)), _MM_SHUFFLE(3, 0, 2, 1)),
                         _mm_shuffle_epi32(V4_LOAD(VF(o->ft)), _MM_SHUFFLE(3, 1, 0, 2)));
            MACPUSH();
            break;
        case K_OPMSUB:
            res = v4_sub(V4_LOAD(u->acc), v4_mul(_mm_shuffle_epi32(V4_LOAD(VF(o->fs)), _MM_SHUFFLE(3, 0, 2, 1)),
                                                 _mm_shuffle_epi32(V4_LOAD(VF(o->ft)), _MM_SHUFFLE(3, 1, 0, 2))));
            MACPUSH();
            break;
        case K_ABS: res = _mm_and_si128(V4_LOAD(VF(o->fs)), V4_SET1(0x7FFFFFFF)); break;
        case K_ITOF: res = v4_itof(V4_LOAD(VF(o->fs)), o->bc); break;
        case K_FTOI: res = v4_ftoi(V4_LOAD(VF(o->fs)), o->bc); break;
        case K_CLIP: {
            const uint32_t *fa = VF(o->fs);
            int32_t w = sf_key(VF(o->ft)[3] & 0x7FFFFFFFu);
            uint32_t fl = (sf_key(fa[0]) > w) | (sf_key(fa[0]) < -w) << 1 | (sf_key(fa[1]) > w) << 2 |
                          (sf_key(fa[1]) < -w) << 3 | (sf_key(fa[2]) > w) << 4 | (sf_key(fa[2]) < -w) << 5;
            u->clipLatest = ((u->clipLatest << 6) | fl) & 0xFFFFFF;
            queue(u->clipQueue, &u->clipHead, &u->clipCount, cycle, u->clipLatest);
            break;
        }
        default: u->unknown[0][o->bc]++; break;
        }
#undef UCASES
#undef UCASE
        /* ---- lower ---- */
        switch (o->lw) {
        case L_NONE: break;
        case L_LQ:
            if (o->it) {
                const uint8_t *m = MEMQ((int32_t)VI(o->is) + (int32_t)o->imm);
                if (o->ld == 15) {
                    memcpy(VF(o->it), m, 16);
                } else {
                    LANES(o->ld) memcpy(&VF(o->it)[c], m + c * 4, 4);
                }
            }
            break;
        case L_SQ: {
            uint8_t *m = MEMQ((int32_t)VI(o->it) + (int32_t)o->imm);
            if (o->ld == 15) {
                memcpy(m, VF(o->is), 16);
            } else {
                LANES(o->ld) memcpy(m + c * 4, &VF(o->is)[c], 4);
            }
            break;
        }
        case L_ILW: {
            const uint8_t *m = MEMQ((int32_t)VI(o->is) + (int32_t)o->imm);
            LANES(o->ld) { uint32_t w; memcpy(&w, m + c * 4, 4); SETVI(o->it, w); }
            break;
        }
        case L_ISW: {
            uint8_t *m = MEMQ((int32_t)VI(o->is) + (int32_t)o->imm);
            LANES(o->ld) { uint32_t w = VI(o->it); memcpy(m + c * 4, &w, 4); }
            break;
        }
        case L_IADDIU: SETVI(o->it, VI(o->is) + o->imm); break;
        case L_ISUBIU: SETVI(o->it, VI(o->is) - o->imm); break;
        case L_FCEQ: clip_now(u, cycle); SETVI(1, (u->clip & 0xFFFFFF) == o->imm); break;
        case L_FCSET: u->clip = u->clipLatest = o->imm; u->clipCount = 0; break;
        case L_FCAND: clip_now(u, cycle); SETVI(1, (u->clip & o->imm) != 0); break;
        case L_FCOR: clip_now(u, cycle); SETVI(1, ((u->clip | o->imm) & 0xFFFFFF) == 0xFFFFFF); break;
        case L_FMEQ: MACSYNC(); SETVI(o->it, (mac_now(u) & 0xFFFF) == (VI(o->is) & 0xFFFF)); break;
        case L_FMAND: MACSYNC(); SETVI(o->it, mac_now(u) & VI(o->is)); break;
        case L_FMOR: MACSYNC(); SETVI(o->it, mac_now(u) | VI(o->is)); break;
        case L_FCGET: clip_now(u, cycle); SETVI(o->it, u->clip & 0xFFF); break;
        case L_B: TAKE(o->aux); break;
        case L_BAL: SETVI(o->it, (pc + 2) & mm); TAKE(o->aux); break;
        case L_JR: TAKE(VI(o->is) & mm); break;
        case L_JALR: { uint32_t t = VI(o->is) & mm; SETVI(o->it, (pc + 2) & mm); TAKE(t); break; }
        case L_IBEQ: if (VI(o->it) == VI(o->is)) TAKE(o->aux); break;
        case L_IBNE: if (VI(o->it) != VI(o->is)) TAKE(o->aux); break;
        case L_IBLTZ: if ((int16_t)VI(o->is) < 0) TAKE(o->aux); break;
        case L_IBGTZ: if ((int16_t)VI(o->is) > 0) TAKE(o->aux); break;
        case L_IBLEZ: if ((int16_t)VI(o->is) <= 0) TAKE(o->aux); break;
        case L_IBGEZ: if ((int16_t)VI(o->is) >= 0) TAKE(o->aux); break;
        case L_IADD: SETVI(o->id, VI(o->is) + VI(o->it)); break;
        case L_ISUB: SETVI(o->id, VI(o->is) - VI(o->it)); break;
        case L_IADDI: SETVI(o->it, VI(o->is) + o->imm); break;
        case L_IAND: SETVI(o->id, VI(o->is) & VI(o->it)); break;
        case L_IOR: SETVI(o->id, VI(o->is) | VI(o->it)); break;
        case L_MOVE: if (o->it) { LANES(o->ld) VF(o->it)[c] = VF(o->is)[c]; } break;
        case L_MR32:
            if (o->it) {
                uint32_t s[4];
                memcpy(s, VF(o->is), 16);
                LANES(o->ld) VF(o->it)[c] = s[(c + 1) & 3];
            }
            break;
        case L_LQI: {
            const uint8_t *m = MEMQ(VI(o->is));
            if (o->it) { LANES(o->ld) memcpy(&VF(o->it)[c], m + c * 4, 4); }
            SETVI(o->is, VI(o->is) + 1);
            break;
        }
        case L_SQI: {
            uint8_t *m = MEMQ(VI(o->it));
            LANES(o->ld) memcpy(m + c * 4, &VF(o->is)[c], 4);
            SETVI(o->it, VI(o->it) + 1);
            break;
        }
        case L_LQD: {
            const uint8_t *m;
            SETVI(o->is, VI(o->is) - 1);
            m = MEMQ(VI(o->is));
            if (o->it) { LANES(o->ld) memcpy(&VF(o->it)[c], m + c * 4, 4); }
            break;
        }
        case L_SQD: {
            uint8_t *m;
            SETVI(o->it, VI(o->it) - 1);
            m = MEMQ(VI(o->it));
            LANES(o->ld) memcpy(m + c * 4, &VF(o->is)[c], 4);
            break;
        }
        case L_DIV: QSETF(v1_div(VF(o->is)[o->fsf], VF(o->it)[o->ftf]), 7); break;
        case L_SQRT: QSETF(v1_sqrt(VF(o->it)[o->ftf]), 7); break;
        case L_RSQRT: QSETF(v1_div(VF(o->is)[o->fsf], v1_sqrt(VF(o->it)[o->ftf])), 13); break;
        case L_WAITQ: if (u->qReady != 0) { *u->q = u->qPending; u->qReady = 0; } break;
        case L_MTIR: SETVI(o->it, VF(o->is)[o->fsf]); break;
        case L_MFIR: if (o->it) { LANES(o->ld) VF(o->it)[c] = (uint32_t)(int32_t)(int16_t)VI(o->is); } break;
        case L_ILWR: {
            const uint8_t *m = MEMQ(VI(o->is));
            LANES(o->ld) { uint32_t w; memcpy(&w, m + c * 4, 4); SETVI(o->it, w); }
            break;
        }
        case L_ISWR: {
            uint8_t *m = MEMQ(VI(o->is));
            LANES(o->ld) { uint32_t w = VI(o->it); memcpy(m + c * 4, &w, 4); }
            break;
        }
        case L_MFP: p_now(u, cycle); if (o->it) { LANES(o->ld) VF(o->it)[c] = u->p; } break;
        case L_XTOP: if (u->vu1) { SETVI(o->it, u->top); } else { u->unknown[1][o->imm - 128]++; } break;
        case L_XITOP: if (u->vu1) { SETVI(o->it, u->itop); } else { u->unknown[1][o->imm - 128]++; } break;
        case L_XGKICK:
            if (u->kick != NULL) {
                _mm_setcsr(csr); /* the host's own mode outside the program */
                u->kick(VI(o->is));
                _mm_setcsr(V4_MXCSR);
            } else {
                u->unknown[1][o->imm - 128]++;
            }
            break;
        case L_ESADD: PSETF(esum2(VF(o->is)), 11); break;
        case L_ERSADD: PSETF(v1_div(0x3F800000u, esum2(VF(o->is))), 18); break;
        case L_ELENG: PSETF(v1_sqrt(esum2(VF(o->is))), 18); break;
        case L_ERLENG: PSETF(v1_div(0x3F800000u, v1_sqrt(esum2(VF(o->is)))), 24); break;
        case L_EATANXY: case L_EATANXZ: {
            const uint32_t *v = VF(o->is);
            uint32_t x = v[0], y = v[o->lw == L_EATANXY ? 1 : 2];
            PSETF(eatan_t(v1_div(vsub(y, x), vadd(y, x))), 54);
            break;
        }
        case L_ESUM: { const uint32_t *v = VF(o->is); PSETF(vadd(vadd(vadd(v[0], v[1]), v[2]), v[3]), 12); break; }
        case L_ESQRT: PSETF(v1_sqrt(VF(o->is)[o->fsf]), 12); break;
        case L_ERSQRT: PSETF(v1_div(0x3F800000u, v1_sqrt(VF(o->is)[o->fsf])), 18); break;
        case L_ERCPR: PSETF(v1_div(0x3F800000u, VF(o->is)[o->fsf]), 12); break;
        case L_WAITP: if (u->pReady != 0) { u->p = u->pPending; u->pReady = 0; } break;
        case L_EATAN: {
            uint32_t x = VF(o->is)[o->fsf];
            PSETF(eatan_t(v1_div(vsub(x, 0x3F800000u), vadd(x, 0x3F800000u))), 54);
            break;
        }
        default: u->unknown[1][o->imm - 128]++; break;
        }
        /* ---- the upper result, after the lower half read its operands ---- */
        if (o->tgt != T_NONE) {
            uint32_t *w = o->tgt == T_ACC ? u->acc : o->tgt == T_FD ? VF(o->fd) : VF(o->ft);
            if (o->dest == 15) {
                V4_STORE(w, res);
            } else {
                V4_STORE(w, v4_sel(v4_mask(o->dest), res, V4_LOAD(w)));
            }
        }
        V4_STORE(VF(0), one0);
        vi[0] = 0;
        if (((o->flags & F_CTL) | ending | branch) == 0) { /* no branch, no end on its way */
            pc = next;
            continue;
        }
        if (ending) {
            u->pc = next;
            rc = 0;
            n++;
            break;
        }
        if (o->flags & F_EBIT) {
            ending = 1;
        }
        if (branch == 1) {
            pc = target;
            branch = 0;
        } else {
            if (branch == 2) {
                branch = 1;
            }
            pc = next;
        }
    }
    _mm_setcsr(csr);
    u->macCount = mc;
    u->macHead = (int)((mt - (uint32_t)mc) & 7);
    if (rc != 0) {
        u->pc = pc;
    }
    u->cycle = cycle;
    u->instr += n;
    q_now(u, cycle);
    p_now(u, cycle);
    clip_now(u, cycle);
    macq_now(u, cycle);
    if (rc == 0 && u->settle) {
        settle(u);
    }
    return rc;
}

int Vu_Run(VuCore *u, uint32_t start) {
    return u->ops != NULL ? run_fast(u, start) : Vu_RunRef(u, start);
}
