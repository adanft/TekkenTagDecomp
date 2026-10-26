/*
 * VU0 micro mode: the VIF0 side that loads the game's VU0 microcode (MPG) and the program start the EE makes with
 * vcallms (port/src/vu0_pc.c). The programs run on the micro-mode interpreter shared with VU1 (vu_fast.c); the
 * game's four skinning programs (src/vu0/skin_*.vsm) run unchanged on it.
 *
 * VU0 memory is the EE's view of it (plat_mem.c): micro memory at 0x11000000 (4 KB), data memory at 0x11004000
 * (4 KB). The registers are shared with macro mode: gPortVu0Vf (plat_ee.c), gPortVu0Vi, gPortVu0Acc, gPortVu0Q.
 *
 * T31: the skinning programs (vu0_full.vsm's four, vu0_small.vsm's two of their own) run as C instead (Vu0_RunNative,
 * vu0_native_inl.h in vu_fast.c), with the same registers, memory and pipeline state as the interpreter leaves, about
 * five times faster. A program runs so only while micro memory holds the game's words for it (an FNV-1a hash of its
 * slots, worked out again after any MPG word or state load); anything else runs on the interpreter.
 * TTT_VU0_NATIVE=0 runs everything on the interpreter; TTT_VU0_CHECK=1 runs each C program also on the interpreter
 * from a copy of the unit and compares the two byte for byte; TTT_SELFTEST=vu0 does that on random states.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define memcpy __builtin_memcpy /* -fno-builtin: inline the small copies of the hot paths */
#define memcmp __builtin_memcmp
#include "vu_micro.h"
#include "plat_state.h"
#include "port_addr.h"

extern float gPortVu0Vf[32][4]; /* plat_ee.c */
uint16_t gPortVu0Vi[16];         /* vi0..vi15 (ctc2 / cfc2 in macro mode) */
uint32_t gPortVu0Acc[4];         /* ACC, shared with macro mode (vu0_pc.c) */
uint32_t gPortVu0Q = 0x3F800000u; /* Q */

#define MICRO ((uint8_t *)0x11000000u)
#define DATA ((uint8_t *)0x11004000u)

/* ---------------------------------------------------------------------------------------------- VIF0 */

static uint32_t sMicroGen = 1; /* changes with micro memory (MPG words, state loads): the C programs check it again */

static struct {
    uint32_t pending;  /* data words still owed to the current code */
    uint32_t kind;     /* 0 none, 1 MPG, 2 skipped data */
    uint32_t mpgAddr;  /* byte address in micro memory of the next MPG word */
    uint32_t mask;
} vif;

/* The VIF0 codes and data of a DMA block, in order (plat_hw.c): STCYCL / STMOD / STMASK / MARK / flushes are
   accepted, MPG loads micro memory; anything with data the port does not use yet (STROW, STCOL, UNPACK) has its
   data skipped and is reported once. */
void Port_Vif0Feed(const uint32_t *w, uint32_t n) {
    static int reported;
    uint32_t k;

    for (k = 0; k < n; k++) {
        uint32_t code, cmd, num, imm;

        if (vif.pending) {
            if (vif.kind == 1) {
                memcpy(MICRO + (vif.mpgAddr & 0xFFC), &w[k], 4);
                vif.mpgAddr += 4;
                sMicroGen++;
            }
            vif.pending--;
            continue;
        }
        code = w[k];
        cmd = (code >> 24) & 0x7F;
        num = (code >> 16) & 0xFF;
        imm = code & 0xFFFF;
        if (cmd == 0x4A) { /* MPG: num instructions (0 = 256) at imm * 8 */
            vif.kind = 1;
            vif.mpgAddr = (imm * 8) & 0xFFF;
            vif.pending = (num ? num : 256) * 2;
        } else if (cmd == 0x20) { /* STMASK: one word */
            vif.kind = 2;
            vif.pending = 1;
        } else if (cmd == 0x30 || cmd == 0x31) { /* STROW / STCOL: four words */
            vif.kind = 2;
            vif.pending = 4;
        } else if (cmd >= 0x60) { /* UNPACK: not used by the game on VIF0 so far */
            uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, cnt = num ? num : 256;
            uint32_t bits = (32u >> vl) * (vn + 1);
            vif.kind = 2;
            vif.pending = (cnt * bits + 31) / 32;
            if (!reported++) {
                fprintf(stderr, "ttt: vif0: UNPACK 0x%08x skipped (not implemented)\n", code);
            }
        } else if (cmd == 0x14 || cmd == 0x15 || cmd == 0x17) { /* MSCAL / MSCALF / MSCNT */
            if (!reported++) {
                fprintf(stderr, "ttt: vif0: program start 0x%08x ignored (not implemented)\n", code);
            }
        }
        /* NOP, STCYCL, OFFSET, BASE, ITOP, STMOD, MSKPATH3, MARK, FLUSH*: nothing to do here */
    }
}

/* ---------------------------------------------------------------------------------------------- program start */

/* Runs the program at micro-memory byte address `addr` until its end (E bit and the instruction after it). Results
   still in flight at the end stay in flight into the next call (the EE starts the next vertex at once). */
static VuCore vu;
static VuOp ops[0x200]; /* the predecoded program cache (vu_fast.c) */

int Vu0_RunNative(VuCore *u, int prog, uint32_t slot); /* vu_fast.c (vu0_native_inl.h): 0 = ended, as Vu_Run */

/* The programs with a C version: where they start, their length (instructions), the C program and the FNV-1a hash of
   their micro-memory words (src/vu0/skin_*.vsm). The character select's packet (vu0_small.vsm) puts skin_diffuse's
   words at 0x04A too (skin_diffuse_world) and skin_hide at 0x0EA. */
static const struct {
    uint16_t slot, len;
    int prog;
    uint64_t hash;
} kNative[6] = {
    {0x000, 71, 0, 0x51ADB822507A564Full},  /* skin_diffuse */
    {0x04A, 148, 1, 0x719C09946490E785ull}, /* skin_specular */
    {0x04A, 71, 0, 0x51ADB822507A564Full},  /* skin_diffuse_world */
    {0x0EA, 118, 2, 0x5929915C9F76EC23ull}, /* skin_gloss */
    {0x0EA, 4, 4, 0x1564DDF3180173CBull},   /* skin_hide */
    {0x186, 81, 3, 0xB2F69E31948A1D3Full},  /* skin_envmap */
};
static int sNativeOn = -1, sCheck = -1; /* TTT_VU0_NATIVE (default on), TTT_VU0_CHECK */
static uint32_t sIdGen;                 /* sMicroGen when sId was worked out */
static int8_t sId[4];                   /* the kNative entry in micro memory at 0x000, 0x04A, 0x0EA, 0x186, or -1 */
static uint32_t sNativeRuns, sRuns, sCheckRuns, sCheckBad;

static uint64_t micro_hash(uint32_t slot, uint32_t len) {
    uint64_t h = 0xCBF29CE484222325ull;
    uint32_t k;

    for (k = slot * 8; k < (slot + len) * 8; k++) {
        h = (h ^ MICRO[k & 0xFFF]) * 0x100000001B3ull;
    }
    return h;
}

static void identify(void) {
    int k;

    sId[0] = sId[1] = sId[2] = sId[3] = -1;
    for (k = 5; k >= 0; k--) { /* (the longer program of a slot first) */
        int at = kNative[k].slot == 0x000 ? 0 : kNative[k].slot == 0x04A ? 1 : kNative[k].slot == 0x0EA ? 2 : 3;
        if (sId[at] < 0 && micro_hash(kNative[k].slot, kNative[k].len) == kNative[k].hash) {
            sId[at] = (int8_t)k;
        }
    }
    sIdGen = sMicroGen;
}

/* The kNative entry for a start slot, or -1. */
static int native_prog(uint32_t slot) {
    int at;

    if (sNativeOn <= 0) {
        if (sNativeOn == 0) {
            return -1;
        }
        {
            const char *e = getenv("TTT_VU0_NATIVE"), *c = getenv("TTT_VU0_CHECK");
            sNativeOn = !(e != NULL && e[0] == '0' && e[1] == 0);
            sCheck = c != NULL && c[0] != 0 && !(c[0] == '0' && c[1] == 0);
        }
        if (!sNativeOn) {
            return -1;
        }
    }
    switch (slot) {
    case 0x000: at = 0; break;
    case 0x04A: at = 1; break;
    case 0x0EA: at = 2; break;
    case 0x186: at = 3; break;
    default: return -1;
    }
    if (sIdGen != sMicroGen) {
        identify();
    }
    return sId[at];
}

/* Whether two units are in the same state byte for byte: registers, data memory, I / P, the pipeline (with the
   queues' old entries), where they stopped and the counters. */
static int same_unit(const VuCore *a, const VuCore *b) {
    return memcmp(a->vf, b->vf, 32 * 16) == 0 && memcmp(a->vi, b->vi, 16 * 2) == 0 &&
           memcmp(a->acc, b->acc, 16) == 0 && *a->q == *b->q && memcmp(a->data, b->data, 4096) == 0 &&
           a->i == b->i && a->p == b->p &&
           memcmp(&a->cycle, &b->cycle, offsetof(VuCore, ops) - offsetof(VuCore, cycle)) == 0;
}

/* What differs between two units (the first differences), for the reports of the checks below. */
static void unit_diff(const VuCore *a, const VuCore *b) {
    const uint32_t *x = &a->cycle, *y = &b->cycle;
    int j, c, n = 0;

    for (j = 0; j < 32; j++) {
        for (c = 0; c < 4; c++) {
            if (a->vf[j][c] != b->vf[j][c] && n++ < 8) {
                fprintf(stderr, "  vf%d.%c %08X / %08X\n", j, "xyzw"[c], a->vf[j][c], b->vf[j][c]);
            }
        }
    }
    for (j = 0; j < 16; j++) {
        if (a->vi[j] != b->vi[j] && n++ < 8) {
            fprintf(stderr, "  vi%d %04X / %04X\n", j, a->vi[j], b->vi[j]);
        }
    }
    for (c = 0; c < 4; c++) {
        if (a->acc[c] != b->acc[c] && n++ < 8) {
            fprintf(stderr, "  acc.%c %08X / %08X\n", "xyzw"[c], a->acc[c], b->acc[c]);
        }
    }
    if ((*a->q != *b->q || a->i != b->i || a->p != b->p) && n++ < 8) {
        fprintf(stderr, "  q %08X / %08X, i %08X / %08X, p %08X / %08X\n", *a->q, *b->q, a->i, b->i, a->p, b->p);
    }
    for (j = 0; j < (int)((offsetof(VuCore, ops) - offsetof(VuCore, cycle)) / 4); j++) {
        if (x[j] != y[j] && n++ < 12) {
            fprintf(stderr, "  pipeline word %d %08X / %08X\n", j, x[j], y[j]);
        }
    }
}

/* TTT_VU0_CHECK=1: the interpreter from a copy of the unit, then the C program on the unit itself. */
static uint32_t sRefVf[32][4], sRefAcc[4], sRefQ;
static uint16_t sRefVi[16];
static uint8_t sRefData[4096];

static int run_checked(int k) {
    VuCore ref = vu;
    int r0, r1;

    memcpy(sRefVf, gPortVu0Vf, sizeof sRefVf);
    memcpy(sRefVi, gPortVu0Vi, sizeof sRefVi);
    memcpy(sRefAcc, gPortVu0Acc, sizeof sRefAcc);
    sRefQ = gPortVu0Q;
    memcpy(sRefData, DATA, sizeof sRefData);
    ref.vf = sRefVf;
    ref.vi = sRefVi;
    ref.acc = sRefAcc;
    ref.q = &sRefQ;
    ref.data = sRefData;
    r0 = Vu_Run(&ref, kNative[k].slot);
    r1 = Vu0_RunNative(&vu, kNative[k].prog, kNative[k].slot);
    sCheckRuns++;
    if (r0 != r1 || !same_unit(&ref, &vu)) {
        if (sCheckBad++ < 5) {
            fprintf(stderr, "ttt: vu0 check: run %u (program 0x%03X) differs from the interpreter (interpreter / C):\n",
                    sRuns, kNative[k].slot);
            unit_diff(&ref, &vu);
        }
    }
    return r1;
}

static void vu_init(void) {
    vu.vf = (uint32_t (*)[4])gPortVu0Vf;
    vu.vi = gPortVu0Vi;
    vu.acc = gPortVu0Acc;
    vu.q = &gPortVu0Q;
    vu.micro = MICRO;
    vu.data = DATA;
    vu.microMask = 0x1FF;
    vu.dataMask = 0xFF;
    vu.ops = ops;
}

void Port_Vu0CallMs(uint32_t addr) {
    static int reported;
    static uint32_t calls;
    int k, rc;

    if (vu.ops == NULL || vu.vf == NULL) { /* (ops is not part of a state: a loaded state may have either unset) */
        vu_init();
    }
    sRuns++;
    k = native_prog((addr / 8) & 0x1FF);
    if (k >= 0) {
        sNativeRuns++;
        rc = sCheck ? run_checked(k) : Vu0_RunNative(&vu, kNative[k].prog, kNative[k].slot);
    } else {
        rc = Vu_Run(&vu, addr / 8);
    }
    if (rc != 0 && reported++ < 2) {
        fprintf(stderr, "ttt: vu0: program 0x%03X did not end\n", addr);
    }
    /* the unknown-instruction counters are summed now and then: the skinning programs run once per vertex */
    if ((calls++ & 1023) == 0 && reported == 0 && Vu_Unknown(&vu) != 0) {
        reported++;
        fprintf(stderr, "ttt: vu0: %u instructions not implemented (seen after program 0x%03X)\n", Vu_Unknown(&vu), addr);
    }
}

/* The end-of-run report (gs_core.c, after VU1's). */
void Port_Vu0Report(void) {
    printf("ttt: vu0: %u program runs, %u in C (%s), %llu instructions\n", sRuns, sNativeRuns,
           sNativeOn == 0 ? "off" : "on", (unsigned long long)vu.instr);
    if (sCheck > 0) {
        printf("ttt: vu0 check: %u runs, %u differ\n", sCheckRuns, sCheckBad);
    }
}

static void vu0_after_load(void) { sMicroGen++; }

/* The unit's own state (pipeline, flags, where the last program stopped), saved and restored around a TTT_DIFF check
   (ee_diff.c), which runs the original's programs on this unit too. */
void *Port_Vu0Core(uint32_t *size) {
    *size = (uint32_t)sizeof vu;
    sMicroGen++; /* (the check may put micro memory back too) */
    return &vu;
}

/* VU0's micro-mode side: its registers (shared with macro mode), VIF0, the unit's pipeline (not the predecoded program:
   each slot is checked against micro memory when it runs). */
__attribute__((constructor(101))) static void vu0_state(void) {
    Port_StateAdd("vu0.vi", gPortVu0Vi, sizeof gPortVu0Vi, PS_HASH);
    Port_StateAdd("vu0.acc", gPortVu0Acc, sizeof gPortVu0Acc, PS_HASH);
    Port_StateAdd("vu0.q", &gPortVu0Q, sizeof gPortVu0Q, PS_HASH);
    Port_StateAdd("vif0", &vif, sizeof vif, PS_HASH);
    Port_StateAdd("vu0.core", &vu, (uint32_t)offsetof(VuCore, ops), 0); /* (its pointers name this file's variables) */
    Port_StateAdd("vu0.pipe", &vu.cycle, (uint32_t)((uint8_t *)&vu.instr - (uint8_t *)&vu.cycle), PS_HASH | PS_HASHONLY);
    Port_StateAdd("vu0.ip", &vu.i, 2 * sizeof vu.i, PS_HASH | PS_HASHONLY);
    Port_StateHooks(NULL, NULL, vu0_after_load);
}

/* ---------------------------------------------------------------------------------------------- self-test */

/* TTT_SELFTEST=vu0 (T31): the C programs against the interpreter. The game's packets (D_004FA7E0, the character
   select's; D_004F5F00, the fight's) are sent through VIF0 from the game data in EE RAM, and each must be recognised
   program by program (skin_specular no more after one changed word). Then each program runs from
   random states on both: registers and data memory of ordinary floats, now and then edge operands (exponent 0 and 255,
   the largest), one to four bones, every branch (light 0 behind, the guard band, the highlight's exponent loop, 65535
   times included), the pipeline with results still in flight (Q, MAC and clipping queues at any position), end
   settling on or off, and vf0 / vi0 out of place; both must leave the same bytes (same_unit). */
extern uint32_t gVu0NativeExact; /* vu_fast.c */
uint32_t Vu0_NativeEnter(void);
void Vu0_NativeLeave(uint32_t mode);

static uint32_t sSeed = 0x2545F491u, sEdge;
static uint32_t rnd(void) {
    sSeed ^= sSeed << 13;
    sSeed ^= sSeed >> 17;
    sSeed ^= sSeed << 5;
    return sSeed;
}

/* a float near the game's (about 2^-7 .. 2^4), or with probability 1 / sEdge an edge one */
static uint32_t rfloat(void) {
    uint32_t r = rnd();
    if (sEdge != 0 && rnd() % sEdge == 0) {
        switch (rnd() & 3) {
        case 0: return r | 0x7F800000u;              /* exponent 255 */
        case 1: return r & 0x807FFFFFu;              /* exponent 0 */
        case 2: return (r & 0x807FFFFFu) | (254u << 23); /* the largest */
        default: return r & 0x80000000u;             /* zero */
        }
    }
    return (r & 0x807FFFFFu) | ((120 + rnd() % 12) << 23);
}

typedef struct TestUnit {
    uint32_t vf[32][4], acc[4], q;
    uint16_t vi[16];
    uint8_t data[4096];
    VuCore c;
} TestUnit;
static TestUnit sA, sB;
static VuOp sTestOps[0x200];

static void unit_bind(TestUnit *x) {
    x->c.vf = x->vf;
    x->c.vi = x->vi;
    x->c.acc = x->acc;
    x->c.q = &x->q;
    x->c.micro = MICRO;
    x->c.data = x->data;
    x->c.microMask = 0x1FF;
    x->c.dataMask = 0xFF;
    x->c.ops = sTestOps;
}

static void random_state(TestUnit *a) {
    uint32_t k, c, cyc, n, inband;

    for (k = 0; k < 32; k++) {
        for (c = 0; c < 4; c++) {
            a->vf[k][c] = rfloat();
        }
    }
    a->vf[0][0] = a->vf[0][1] = a->vf[0][2] = 0;
    a->vf[0][3] = 0x3F800000u;
    for (c = 0; c < 4; c++) {
        a->acc[c] = rfloat();
    }
    a->q = rfloat();
    for (k = 0; k < 16; k++) {
        a->vi[k] = (uint16_t)rnd();
    }
    a->vi[0] = 0;
    n = 1 + rnd() % 4; /* bones: vi2 .. vi(n + 1) not 0, the next 0 */
    for (k = 0; k < 4; k++) {
        if (k < n) {
            a->vi[2 + k] = (uint16_t)((rnd() & 1 ? 0 : rnd() & 0xFF00) | (1 + rnd() % 255));
        } else if (k == n) {
            a->vi[2 + k] = 0;
        }
    }
    k = rnd() % 1000;
    a->vi[6] = (uint16_t)((rnd() & 0xFF00) | (k == 0 ? 0 : k < 20 ? 100 + rnd() % 156 : rnd() % 6));
    if (rnd() % 64 == 0) { /* out of place: the interpreter runs it */
        if (rnd() & 1) {
            a->vi[0] = (uint16_t)rnd();
        } else {
            a->vf[0][rnd() & 3] = rfloat();
        }
    }
    for (k = 0; k < 256; k++) {
        for (c = 0; c < 4; c++) {
            uint32_t w = rfloat();
            memcpy(a->data + k * 16 + c * 4, &w, 4);
        }
    }
    memset(&a->c, 0, sizeof a->c);
    inband = rnd() & 1; /* half the vertices inside the guard band (no scale), the two before too */
    if (inband) {
        memset(a->data, 0, 12);
    }
    a->c.i = rnd();
    a->c.p = rnd();
    cyc = (rnd() & 0x0FFFFFFFu) | 0x100;
    a->c.cycle = cyc;
    a->c.qPending = rfloat();
    a->c.qReady = rnd() & 1 ? cyc + 1 + rnd() % 14 : 0;
    a->c.pPending = rfloat();
    a->c.pReady = rnd() & 1 ? cyc + 1 + rnd() % 30 : 0;
    a->c.clip = inband ? 0 : rnd() & 0xFFFFFF;
    a->c.clipLatest = inband ? 0 : rnd() & 0xFFFFFF;
    a->c.mac = rnd() & 0xFF;
    for (k = 0; k < 8; k++) { /* old entries */
        a->c.clipQueue[k].cycle = rnd();
        a->c.clipQueue[k].value = rnd() & 0xFFFFFF;
        a->c.macQueue[k].cycle = rnd();
        a->c.macQueue[k].mask = rnd() & 15;
        for (c = 0; c < 4; c++) {
            a->c.macQueue[k].res[c] = rfloat();
        }
    }
    a->c.macNow = a->c.macQueue[rnd() & 7];
    a->c.macLazy = (int)(rnd() & 1);
    a->c.clipHead = (int)(rnd() & 7);
    a->c.clipCount = inband ? 0 : (int)(rnd() % 4);
    a->c.macHead = (int)(rnd() & 7);
    a->c.macCount = (int)(rnd() % 4);
    for (k = 0; k < (uint32_t)a->c.clipCount; k++) { /* in flight: in order, arriving in the next steps */
        a->c.clipQueue[(a->c.clipHead + (int)k) & 7].cycle = cyc + 1 + k;
    }
    for (k = 0; k < (uint32_t)a->c.macCount; k++) {
        a->c.macQueue[(a->c.macHead + (int)k) & 7].cycle = cyc + 1 + k;
    }
    a->c.settle = rnd() % 8 == 0;
    a->c.pc = rnd() & 0x1FF;
    a->c.instr = rnd();
    unit_bind(a);
}

/* A VIF0 DMA chain of cnt / end tags (TTE: each tag's upper half goes first), as plat_hw.c sends it. */
static int feed_packet(uint32_t addr) {
    const uint32_t *p = (const uint32_t *)Port_DmaPtr(addr);
    int n;

    for (n = 0; n < 16; n++) {
        uint32_t qwc = p[0] & 0xFFFF, id = (p[0] >> 28) & 7;
        if ((id != 1 && id != 7) || qwc > 0x200) {
            return -1;
        }
        Port_Vif0Feed(&p[2], 2);
        Port_Vif0Feed(&p[4], qwc * 4);
        if (id == 7) {
            return 0;
        }
        p += 4 + qwc * 4;
    }
    return -1;
}

/* The kNative entries native_prog finds at the four start slots, one hex digit each (f: none), 0x000's first. */
static uint32_t recognised(void) {
    return (uint32_t)(native_prog(0x000) & 15) << 12 | (uint32_t)(native_prog(0x04A) & 15) << 8 |
           (uint32_t)(native_prog(0x0EA) & 15) << 4 | (uint32_t)(native_prog(0x186) & 15);
}

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

/* Random runs of kNative entry k on both, from states made by random_state; returns the differences. ends: how often
   it stopped at its first end (the highlight's, for skin_specular and skin_gloss) and elsewhere. */
static uint32_t trials(int k, uint32_t n, uint32_t ends[2]) {
    static const uint16_t kFirstEnd[5] = {0x47, 0xC2 - 0x4A, 0x15B - 0xEA, 0x51, 4};
    uint32_t i, bad = 0;

    for (i = 0; i < n; i++) {
        int r0, r1;
        sEdge = i % 4 == 0 ? 8 + rnd() % 200 : 0;
        random_state(&sA);
        sB = sA;
        unit_bind(&sB);
        r0 = Vu_Run(&sA.c, kNative[k].slot);
        r1 = Vu0_RunNative(&sB.c, kNative[k].prog, kNative[k].slot);
        if (r0 != r1 || !same_unit(&sA.c, &sB.c)) {
            if (bad++ < 3) {
                printf("ttt: vu0 self-test: program 0x%03X (entry %d, trial %u) differs from the interpreter "
                       "(interpreter / C):\n", kNative[k].slot, k, i);
                fflush(stdout);
                unit_diff(&sA.c, &sB.c);
            }
        }
        ends[sB.c.pc != ((kNative[k].slot + kFirstEnd[kNative[k].prog]) & 0x1FF)]++;
    }
    return bad;
}

int Port_Vu0SelfTest(void) {
    static const int kFull[4] = {0, 1, 3, 5}, kSmall[2] = {2, 4};
    uint32_t runs = 0, bad = 0, ends[6][2] = {{0}}, exact0 = gVu0NativeExact, id[4], t[4][2];
    uint32_t mpg[4];
    int fails = 0, k;

    sNativeOn = 1;
    if (feed_packet(0x4FA7E0) != 0) {
        printf("ttt: vu0 self-test: no VU0 packets in EE RAM (the programs come from the game data)\n");
        return 1;
    }
    id[0] = recognised();
    for (k = 0; k < 2; k++) { /* the character select's two programs of their own */
        runs += 5000;
        bad += trials(kSmall[k], 5000, ends[kSmall[k]]);
    }
    feed_packet(0x4F5F00);
    id[1] = recognised();
    mpg[0] = 0x4A010000u | 0x50; /* MPG of one instruction at slot 0x50 (in skin_specular): a different word */
    memcpy(&mpg[1], MICRO + 0x50 * 8, 8);
    mpg[1] ^= 1;
    mpg[2] = mpg[3] = 0;
    Port_Vif0Feed(mpg, 3);
    id[2] = recognised();
    feed_packet(0x4F5F00);
    id[3] = recognised();
    if (id[0] != 0x0245 || id[1] != 0x0135 || id[2] != 0x0F35 || id[3] != 0x0135) {
        printf("ttt: vu0 self-test: programs recognised %04X / %04X / %04X / %04X (want 0245 0135 0F35 0135)\n", id[0],
               id[1], id[2], id[3]);
        fails++;
    }
    for (k = 0; k < 4; k++) {
        runs += 25000;
        bad += trials(kFull[k], 25000, ends[kFull[k]]);
    }
    for (k = 1; k <= 3; k += 2) { /* both ends of skin_specular and skin_gloss seen */
        if (ends[k][0] == 0 || ends[k][1] == 0) {
            printf("ttt: vu0 self-test: program 0x%03X stopped %u times at the highlight's end, %u elsewhere\n",
                   kNative[k].slot, ends[k][0], ends[k][1]);
            fails++;
        }
    }
    if (gVu0NativeExact == exact0) {
        printf("ttt: vu0 self-test: no run met an operand of exponent 255\n");
        fails++;
    }
    /* CPU time per run, interpreter / C, on a state without edge operands, in the skinning loop's float mode
       (informative) */
    sEdge = 0;
    for (k = 0; k < 4; k++) {
        uint32_t mode = Vu0_NativeEnter();
        int m, n;
        random_state(&sA);
        sA.vi[0] = 0;
        sA.vf[0][0] = sA.vf[0][1] = sA.vf[0][2] = 0;
        sA.vf[0][3] = 0x3F800000u;
        sA.vi[6] = 2;
        sB = sA;
        unit_bind(&sB);
        for (m = 0; m < 2; m++) {
            uint64_t t0 = now_ns();
            for (n = 0; n < 20000; n++) {
                if (m == 0) {
                    Vu_Run(&sA.c, kNative[kFull[k]].slot);
                } else {
                    Vu0_RunNative(&sB.c, kNative[kFull[k]].prog, kNative[kFull[k]].slot);
                }
            }
            t[k][m] = (uint32_t)((now_ns() - t0) / 20000);
        }
        Vu0_NativeLeave(mode);
    }
    printf("ttt: vu0 native: %u random runs, %u with exponent-255 operands, %u differ; ns per run interpreter / C: "
           "diffuse %u / %u, specular %u / %u, gloss %u / %u, envmap %u / %u\n",
           runs, gVu0NativeExact - exact0, bad, t[0][0], t[0][1], t[1][0], t[1][1], t[2][0], t[2][1], t[3][0], t[3][1]);
    return fails + (int)bad;
}
