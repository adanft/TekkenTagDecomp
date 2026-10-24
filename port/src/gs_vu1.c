/*
 * VU1: the vertex unit. The game uploads its VU1 programs once (the boot packet, src/vu1/*.vsm) and, per object
 * and per batch, matrices and vertices through VIF1 UNPACK (gs_vif1.c); MSCAL / MSCALF / MSCNT run a program,
 * which transforms, lights and clips the batch and sends the finished GIF packet to the GS with XGKICK (PATH1):
 *     VIF1 UNPACK / MPG / MSCAL  ->  VU1 memory and program  ->  vu_fast.c  ->  XGKICK  ->  GIF (gs_core.c)
 * The ORIGINAL programs run unchanged on the shared micro-mode interpreter (vu_fast.c, predecoded; vu_micro.c, the
 * reference) with the EE float model's vector-unit arithmetic. TTT_VU_CHECK=1 runs every program on both and compares.
 *
 * Structure after the reference project's VU1 unit (port/src/gs/gs_vu1.c of Tenkaichi3Decomp): the program runs to
 * its end at the VIF code that starts it, XGKICK hands the GIF one packet (its tags up to EOP, read across the end of
 * VU memory) at once.
 *
 * TTT_SELFTEST=vu1 runs small hand-assembled programs (scripts/vuasm.py) through VIF1 and checks their results
 * against the VU's rules: pairing, branches and their delay slots, the E bit, Q / P latency, the MAC and clipping
 * flags' delay, integer and EFU instructions, XTOP / XITOP with the double buffer, XGKICK.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define memcpy __builtin_memcpy /* -fno-builtin: inline the small copies of the hot paths */
#include "port_addr.h"
#include "gs.h"
#include "vu_micro.h"
#include "plat_state.h"

#define VU1_DATA ((uint8_t *)Port_Ptr(0x1100C000u))
#define VU1_MICRO ((uint8_t *)Port_Ptr(0x11008000u))

static uint32_t sVf[32][4] __attribute__((aligned(16)));
static uint16_t sVi[16];
static uint32_t sAcc[4], sQ;
static VuOp sOps[0x800]; /* the predecoded program cache (vu_fast.c) */
static VuCore sVu = { .ops = sOps }; /* (ops is not part of a state: set here for a process that starts from one) */
static uint32_t sRuns, sKicks, sEndless;
static uint32_t sEntry[0x800], sLastEntry; /* runs per entry address (MSCNT counts for the program it continues) */

static void kick(uint32_t qw);

void Gs_Vu1Reset(void) {
    memset(sVf, 0, sizeof sVf);
    memset(sVi, 0, sizeof sVi);
    memset(sAcc, 0, sizeof sAcc);
    sQ = 0;
    memset(&sVu, 0, sizeof sVu);
    sVf[0][3] = 0x3F800000u;
    sVu.vf = sVf;
    sVu.vi = sVi;
    sVu.acc = sAcc;
    sVu.q = &sQ;
    sVu.micro = VU1_MICRO;
    sVu.data = VU1_DATA;
    sVu.microMask = 0x7FF;
    sVu.dataMask = 0x3FF;
    sVu.vu1 = 1;
    sVu.kick = kick;
    sVu.settle = 1;
    sVu.ops = sOps;
}

/* XGKICK: the GIF packet at quadword `qw` of VU1 memory, tag by tag up to the one with EOP. */
static void kick(uint32_t qw) {
    static uint8_t local[0x4000];
    uint8_t *q = Gs_AsyncGifReserve(0x400), *buf = q != NULL ? q : local; /* the GIF thread's queue, written in place */
    uint32_t a = qw & 0x3FF, len = 0;

    sKicks++;
    for (;;) {
        uint64_t lo;
        uint32_t nloop, flg, nreg, n;
        memcpy(&lo, VU1_DATA + a * 16, 8);
        nloop = lo & 0x7FFF;
        flg = (lo >> 58) & 3;
        nreg = (lo >> 60) & 15;
        if (nreg == 0) {
            nreg = 16;
        }
        n = 1 + (flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop);
        if (len + n > 0x400) { /* longer than VU memory: a broken packet, send what there is */
            n = 0x400 - len;
        }
        for (; n != 0; n--, len++, a = (a + 1) & 0x3FF) {
            memcpy(buf + len * 16, VU1_DATA + a * 16, 16);
        }
        if (((lo >> 15) & 1) || len >= 0x400) {
            break;
        }
    }
    if (q != NULL) {
        Gs_AsyncGifCommit(1, len);
    } else {
        Gs_Gif(1, buf, len);
    }
}

/* TTT_VU_CHECK=1: each program also runs on the reference interpreter, from a copy of the unit (registers, data
   memory, pipeline) whose XGKICKs only hash their packets; both must end in the same state with the same packets. */
static int sCheck = -1;
static uint32_t sCheckRuns, sCheckBad;
static uint32_t sRefVf[32][4] __attribute__((aligned(16)));
static uint16_t sRefVi[16];
static uint32_t sRefAcc[4], sRefQ;
static uint8_t sRefData[0x4000];
static uint64_t sHash[2];

static void hash_packet(int which, const uint8_t *data, uint32_t qw) {
    uint32_t a = qw & 0x3FF, n = 0;
    uint64_t h = sHash[which];
    for (;;) { /* the quadwords kick() sends */
        uint64_t lo, w[2];
        uint32_t nloop, flg, nreg, len;
        memcpy(&lo, data + a * 16, 8);
        nloop = lo & 0x7FFF;
        flg = (lo >> 58) & 3;
        nreg = (lo >> 60) & 15;
        nreg = nreg == 0 ? 16 : nreg;
        len = 1 + (flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop);
        len = n + len > 0x400 ? 0x400 - n : len;
        for (; len != 0; len--, n++, a = (a + 1) & 0x3FF) {
            memcpy(w, data + a * 16, 16);
            h = (h ^ w[0]) * 0x100000001B3ull ^ w[1];
        }
        if (((lo >> 15) & 1) || n >= 0x400) {
            break;
        }
    }
    sHash[which] = h * 31 + 1;
}
static void ref_kick(uint32_t qw) { hash_packet(0, sRefData, qw); }
static void fast_kick(uint32_t qw) {
    hash_packet(1, VU1_DATA, qw);
    kick(qw);
}

static int run_checked(uint32_t pc) {
    VuCore ref = sVu;
    int r0, r1;

    memcpy(sRefVf, sVf, sizeof sVf);
    memcpy(sRefVi, sVi, sizeof sVi);
    memcpy(sRefAcc, sAcc, sizeof sAcc);
    sRefQ = sQ;
    memcpy(sRefData, VU1_DATA, sizeof sRefData);
    ref.vf = sRefVf;
    ref.vi = sRefVi;
    ref.acc = sRefAcc;
    ref.q = &sRefQ;
    ref.data = sRefData;
    ref.kick = ref_kick;
    ref.ops = NULL;
    sHash[0] = sHash[1] = 0;
    r0 = Vu_RunRef(&ref, pc);
    sVu.kick = fast_kick;
    r1 = Vu_Run(&sVu, pc);
    sVu.kick = kick;
    sCheckRuns++;
    if (r0 != r1 || sHash[0] != sHash[1] || !Vu_SameState(&ref, &sVu)) {
        if (sCheckBad++ < 5) {
            fprintf(stderr, "ttt: vu1 check: run %u (entry 0x%03X) differs from the reference\n", sRuns, pc);
        }
    }
    return r1;
}

/* MSCAL / MSCALF (addr: the instruction address) or MSCNT (addr < 0: where the last program stopped), with TOP and
   ITOP as the VIF latched them. */
void Gs_Vu1Run(int addr, uint32_t top, uint32_t itop) {
    uint32_t pc;

    if (sVu.vf == NULL) {
        Gs_Vu1Reset();
    }
    sVu.top = top;
    sVu.itop = itop;
    sRuns++;
    if (addr >= 0) {
        sLastEntry = (uint32_t)addr & 0x7FF;
    }
    sEntry[sLastEntry]++;
    if (sCheck < 0) {
        sCheck = getenv("TTT_VU_CHECK") != NULL;
    }
    pc = addr >= 0 ? (uint32_t)addr : sVu.pc;
    if ((sCheck ? run_checked(pc) : Vu_Run(&sVu, pc)) != 0 && sEndless++ == 0) {
        fprintf(stderr, "ttt: vu1: program at 0x%03X did not end (stopped at 0x%03X)\n", addr, sVu.pc);
    }
}

void Gs_Vu1Report(void) {
    unsigned k, n = Vu_Unknown(&sVu);
    printf("ttt: vu1: %u MPG, %u program runs, %llu instructions, %u kicks, %u not implemented", gGsVu1Mpg, sRuns,
           (unsigned long long)sVu.instr, sKicks, n);
    for (k = 0; k < 256 && n != 0; k++) {
        if (sVu.unknown[k >> 7][k & 127] != 0) {
            printf(" %s:0x%02X", k < 128 ? "upper" : "lower", k & 127);
        }
    }
    printf("\n");
    if (sCheck > 0) {
        printf("ttt: vu1 check: %u runs, %u differ\n", sCheckRuns, sCheckBad);
    }
    if (getenv("TTT_VU1_CALLS") != NULL) { /* which programs ran, by the entry the last MSCAL / MSCALF named */
        for (k = 0; k < 0x800; k++) {
            if (sEntry[k] != 0) {
                printf("ttt: vu1: entry 0x%03X: %u runs\n", k, sEntry[k]);
            }
        }
    }
}

/* ------------------------------------------------------------------------------------------------ self-test */

static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: vu1 self-test FAILED: %s\n", what);
        sFails++;
    }
}

static uint32_t *mem(uint32_t qw) { return (uint32_t *)(VU1_DATA + (qw & 0x3FF) * 16); }
static void put(uint32_t qw, uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    uint32_t v[4] = {x, y, z, w};
    memcpy(mem(qw), v, 16);
}
static int vec(uint32_t qw, uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    const uint32_t *v = mem(qw);
    if (v[0] == x && v[1] == y && v[2] == z && v[3] == w) {
        return 1;
    }
    printf("ttt: vu1 self-test: memory %u = %08X %08X %08X %08X, expected %08X %08X %08X %08X\n", qw, v[0], v[1], v[2],
           v[3], x, y, z, w);
    return 0;
}

/* The programs below are scripts/vuasm.py output (MPG code + instructions, lower word first); the listing of each is
   in the comment above it. */

/* .mpg 0x0: pairing (the lower half reads registers before the upper half writes them), I, ACC, MR32, OPMULA /
   OPMSUB, FTOI0 / ITOF0
       nop                        | lq.xyzw vf1, 0(vi0)
       nop                        | lq.xyzw vf2, 1(vi0)
       add.xyzw vf1, vf1, vf2     | sq.xyzw vf1, 2(vi0)
       nop                        | sq.xyzw vf1, 3(vi0)
       mulx.xyzw vf3, vf2, vf1x   | loi 0x40000000 [I]
       muli.xyzw vf4, vf3, i      | nop
       mulaw.xyzw acc, vf1, vf0w  | nop
       maddw.xyzw vf5, vf2, vf0w  | nop
       ftoi0.xyzw vf6, vf5        | mr32.xyzw vf7, vf2
       opmula.xyz acc, vf1, vf2   | sq.xyzw vf4, 4(vi0)
       opmsub.xyz vf8, vf2, vf1   | sq.xyzw vf5, 5(vi0)
       itof0.xyzw vf9, vf6        | sq.xyzw vf6, 6(vi0)
       nop                        | sq.xyzw vf7, 7(vi0)
       nop                        | sq.xyzw vf8, 8(vi0)
       nop                        | sq.xyzw vf9, 9(vi0) [E]
       nop                        | nop                                                                              */
static const uint32_t kProgA[] = {
    0x4A100000u, 0x01E10000u, 0x000002FFu, 0x01E20001u, 0x000002FFu, 0x03E00802u, 0x01E20868u, 0x03E00803u,
    0x000002FFu, 0x40000000u, 0x81E110D8u, 0x8000033Cu, 0x01E0191Eu, 0x8000033Cu, 0x01E009BFu, 0x8000033Cu,
    0x01E0114Bu, 0x81E7133Du, 0x01E6297Cu, 0x03E02004u, 0x01C20AFEu, 0x03E02805u, 0x01C1122Eu, 0x03E03006u,
    0x01E9313Cu, 0x03E03807u, 0x000002FFu, 0x03E04008u, 0x000002FFu, 0x03E04809u, 0x400002FFu, 0x8000033Cu,
    0x000002FFu};

/* .mpg 0x40: branches and their delay slots, BAL / JR, the E bit's delay slot, MSCNT continuing after it
   start:  nop | iaddiu vi1, vi0, 0x3          nop | isw.z vi5, 16(vi0) [E]
           nop | iaddiu vi2, vi0, 0x0          nop | iaddiu vi6, vi0, 0x77      ; E delay slot: runs
           nop | iaddiu vi3, vi0, 0x0          nop | isw.x vi6, 17(vi0)         ; MSCNT starts here
   loop:   nop | iaddi vi2, vi2, 2             nop | iaddiu vi6, vi0, 0x55
           nop | isubiu vi1, vi1, 0x1          nop | isw.y vi6, 17(vi0) [E]
           nop | nop                           nop | nop
           nop | ibne vi1, vi0, loop     sub:  nop | iadd vi5, vi4, vi4
           nop | iaddiu vi3, vi3, 0x1          nop | jr vi15
           nop | bal vi15, sub                 nop | nop
           nop | iaddiu vi4, vi0, 0x10
           nop | isw.x vi2, 16(vi0)
           nop | isw.y vi3, 16(vi0)                                                                                */
static const uint32_t kProgB[] = {
    0x4A150040u, 0x10010003u, 0x000002FFu, 0x10020000u, 0x000002FFu, 0x10030000u, 0x000002FFu, 0x800210B2u,
    0x000002FFu, 0x12010801u, 0x000002FFu, 0x8000033Cu, 0x000002FFu, 0x520107FCu, 0x000002FFu, 0x10031801u,
    0x000002FFu, 0x420F0009u, 0x000002FFu, 0x10040010u, 0x000002FFu, 0x0B020010u, 0x000002FFu, 0x0A830010u,
    0x000002FFu, 0x0A450010u, 0x400002FFu, 0x10060077u, 0x000002FFu, 0x0B060011u, 0x000002FFu, 0x10060055u,
    0x000002FFu, 0x0A860011u, 0x400002FFu, 0x8000033Cu, 0x000002FFu, 0x80042170u, 0x000002FFu, 0x48007800u,
    0x000002FFu, 0x8000033Cu, 0x000002FFu};

/* .mpg 0x80: Q latency (DIV, WAITQ), P (ERLENG, EATANxy, WAITP, MFP), the MAC flags' and the clipping flags' delay
       nop                        | lq.xyzw vf1, 20(vi0)
       nop                        | lq.xyzw vf2, 21(vi0)
       nop                        | div q, vf0w, vf0w
       nop                        | waitq
       nop                        | div q, vf1x, vf2w
       mulq.xyzw vf3, vf2, q      | nop                       ; Q not ready: the old Q
       nop                        | waitq
       mulq.xyzw vf4, vf2, q      | erleng.xyz p, vf1
       nop                        | waitp
       nop                        | mfp.xyzw vf5, p
       nop                        | eatanxy.xy p, vf2
       nop                        | mfp.y vf6, p              ; P not ready: the old P
       nop                        | waitp
       nop                        | mfp.x vf6, p
       nop                        | iaddiu vi7, vi0, 0xff
       add.xyzw vf0, vf0, vf0     | fcset 0x0                 ; MAC: x, y, z zero
       sub.xyzw vf0, vf1, vf2     | nop                       ; MAC: z, w negative
       nop                        | nop
       nop                        | nop
       nop                        | fmand vi2, vi7            ; four instructions after the add
       nop                        | fmand vi3, vi7            ; ... after the sub
       clipw.xyz vf1, vf2w        | nop
       clipw.xyz vf2, vf1w        | nop
       nop                        | nop
       nop                        | fcget vi4
       nop                        | fcget vi5
       nop                        | fcget vi6
       nop                        | fcand vi1, 0x100
       nop                        | sq.xyzw vf3, 22(vi0)
       ...                          sq vf4..vf6 to 23..25, isw vi2..vi5 to 26, vi6 / vi1 to 27.x / 27.y [E]      */
static const uint32_t kProgC[] = {
    0x4A270080u, 0x01E10014u, 0x000002FFu, 0x01E20015u, 0x000002FFu, 0x81E003BCu, 0x000002FFu, 0x800003BFu,
    0x000002FFu, 0x81820BBCu, 0x000002FFu, 0x8000033Cu, 0x01E010DCu, 0x800003BFu, 0x000002FFu, 0x81C00F3Fu,
    0x01E0111Cu, 0x800007BFu, 0x000002FFu, 0x81E5067Cu, 0x000002FFu, 0x8180177Cu, 0x000002FFu, 0x8086067Cu,
    0x000002FFu, 0x800007BFu, 0x000002FFu, 0x8106067Cu, 0x000002FFu, 0x100700FFu, 0x000002FFu, 0x22000000u,
    0x01E00028u, 0x8000033Cu, 0x01E2082Cu, 0x8000033Cu, 0x000002FFu, 0x8000033Cu, 0x000002FFu, 0x34023800u,
    0x000002FFu, 0x34033800u, 0x000002FFu, 0x8000033Cu, 0x01C209FFu, 0x8000033Cu, 0x01C111FFu, 0x8000033Cu,
    0x000002FFu, 0x38040000u, 0x000002FFu, 0x38050000u, 0x000002FFu, 0x38060000u, 0x000002FFu, 0x24000100u,
    0x000002FFu, 0x03E01816u, 0x000002FFu, 0x03E02017u, 0x000002FFu, 0x03E02818u, 0x000002FFu, 0x03E03019u,
    0x000002FFu, 0x0B02001Au, 0x000002FFu, 0x0A83001Au, 0x000002FFu, 0x0A44001Au, 0x000002FFu, 0x0A25001Au,
    0x000002FFu, 0x0B06001Bu, 0x000002FFu, 0x0A81001Bu, 0x400002FFu, 0x8000033Cu, 0x000002FFu};

/* .mpg 0xC0: XTOP / XITOP, XGKICK of the batch at TOP
       nop | xtop vi1
       nop | xitop vi2
       nop | isw.x vi1, 28(vi0)
       nop | isw.y vi2, 28(vi0)
       nop | xgkick vi1 [E]
       nop | nop                                                                                                   */
static const uint32_t kProgD[] = {
    0x4A0600C0u, 0x800106BCu, 0x000002FFu, 0x800206BDu, 0x000002FFu, 0x0B01001Cu, 0x000002FFu, 0x0A82001Cu,
    0x000002FFu, 0x80000EFCu, 0x400002FFu, 0x8000033Cu, 0x000002FFu};

/* .mpg 0xD0: LQI / SQD, ILW, MTIR / MFIR, ABS, MAXx / MINIz, IAND / IOR / ISUB, FTOI4 / ITOF4, XGKICK across the
   end of VU memory
       nop                        | iaddiu vi1, vi0, 0x3ff
       nop                        | iaddiu vi3, vi0, 0x28
       nop                        | iaddiu vi4, vi0, 0x32
       nop                        | lqi.xyzw vf1, (vi3++)
       nop                        | sqd.xyzw vf1, (--vi4)
       nop                        | ilw.w vi5, 0(vi4)
       nop                        | mtir vi6, vf1y
       nop                        | mfir.xyzw vf2, vi6
       abs.xyzw vf3, vf2          | iand vi7, vi5, vi6
       maxx.xz vf4, vf1, vf0x     | ior vi8, vi5, vi6
       miniz.xz vf5, vf1, vf0z    | isub vi9, vi5, vi6
       ftoi4.xz vf6, vf1          | sq.xyzw vf3, 50(vi0)
       nop                        | sq.xyzw vf4, 51(vi0)
       itof4.xz vf7, vf6          | sq.xyzw vf5, 52(vi0)
       ...                          sq vf6 / vf7 to 53 / 54; isw vi3, vi4, vi7, vi8 to 55, vi9 / vi5 to 56.x / .y
       nop                        | xgkick vi1 [E]
       nop                        | nop                                                                              */
static const uint32_t kProgE[] = {
    0x4A1800D0u, 0x100103FFu, 0x000002FFu, 0x10030028u, 0x000002FFu, 0x10040032u, 0x000002FFu, 0x81E11B7Cu,
    0x000002FFu, 0x81E40B7Fu, 0x000002FFu, 0x08252000u, 0x000002FFu, 0x80260BFCu, 0x000002FFu, 0x81E233FDu,
    0x000002FFu, 0x800629F4u, 0x01E311FDu, 0x80062A35u, 0x01400910u, 0x80062A71u, 0x01400956u, 0x03E01832u,
    0x0146097Du, 0x03E02033u, 0x000002FFu, 0x03E02834u, 0x0147313Du, 0x03E03035u, 0x000002FFu, 0x03E03836u,
    0x000002FFu, 0x0B030037u, 0x000002FFu, 0x0A840037u, 0x000002FFu, 0x0A470037u, 0x000002FFu, 0x0A280037u,
    0x000002FFu, 0x0B090038u, 0x000002FFu, 0x0A850038u, 0x000002FFu, 0x80000EFCu, 0x400002FFu, 0x8000033Cu,
    0x000002FFu};

static void vif(uint32_t code) { Gs_Vif1(&code, 1); }
static void regs_clear(void) {
    memset(sVf, 0, sizeof sVf);
    memset(sVi, 0, sizeof sVi);
    memset(sAcc, 0, sizeof sAcc);
    sVf[0][3] = 0x3F800000u;
}
static uint32_t label(void) { return (uint32_t)(GS_PRIV(GS_SIGLBLID) >> 32); }

/* A GIF packet at quadword qw: one A+D quadword writing LABEL = id (SIGLBLID.LBLID, all bits). */
static void label_packet(uint32_t qw, uint32_t id) {
    put(qw, 0x00008001u, 0x10000000u, 0xEu, 0); /* NLOOP 1, EOP, PACKED, NREG 1, A+D */
    put((qw + 1) & 0x3FF, id, 0xFFFFFFFFu, 0x62u, 0);
}

int Port_Vu1SelfTest(void) {
    uint32_t s[16];

    sFails = 0;
    Gs_Vif1Reset();
    Gs_Vu1Reset();
    memset(VU1_DATA, 0, 0x4000);
    memset(VU1_MICRO, 0, 0x4000);
    Gs_Vif1(kProgA, sizeof kProgA / 4);
    Gs_Vif1(kProgB, sizeof kProgB / 4);
    Gs_Vif1(kProgC, sizeof kProgC / 4);
    Gs_Vif1(kProgD, sizeof kProgD / 4);
    Gs_Vif1(kProgE, sizeof kProgE / 4);

    /* A */
    put(0, 0x3F800000u, 0x40000000u, 0x40400000u, 0x40800000u); /* 1 2 3 4 */
    put(1, 0x40A00000u, 0x40C00000u, 0x40E00000u, 0x41000000u); /* 5 6 7 8 */
    vif(0x14000000u);                                            /* MSCAL 0x0 */
    expect(vec(2, 0x3F800000u, 0x40000000u, 0x40400000u, 0x40800000u), "pairing: SQ reads vf1 before ADD writes it");
    expect(vec(3, 0x40C00000u, 0x41000000u, 0x41200000u, 0x41400000u), "ADD");
    expect(vec(4, 0x42700000u, 0x42900000u, 0x42A80000u, 0x42C00000u), "MULx, LOI / MULi");
    expect(vec(5, 0x41300000u, 0x41600000u, 0x41880000u, 0x41A00000u), "MULAw / MADDw");
    expect(vec(6, 11, 14, 17, 20), "FTOI0");
    expect(vec(7, 0x40C00000u, 0x40E00000u, 0x41000000u, 0x40A00000u), "MR32");
    expect(vec(8, 0xC0800000u, 0x41000000u, 0xC0800000u, 0), "OPMULA / OPMSUB (cross product)");
    expect(vec(9, 0x41300000u, 0x41600000u, 0x41880000u, 0x41A00000u), "ITOF0");

    /* B */
    vif(0x14000040u); /* MSCAL 0x40 */
    expect(vec(16, 6, 3, 0x20, 0), "IBNE loop with its delay slot, BAL / JR");
    expect(vec(17, 0, 0, 0, 0), "the program stops after the E bit's delay slot");
    vif(0x17000000u); /* MSCNT */
    expect(vec(17, 0x77, 0x55, 0, 0), "E bit delay slot ran; MSCNT continues after it");

    /* C */
    regs_clear();
    put(20, 0x40400000u, 0x40800000u, 0, 0x3F800000u); /* 3 4 0 1 */
    put(21, 0x3F800000u, 0x3F800000u, 0x3F800000u, 0x40000000u); /* 1 1 1 2 */
    vif(0x14000080u);
    expect(vec(22, 0x3F800000u, 0x3F800000u, 0x3F800000u, 0x40000000u), "Q latency: MULq right after DIV");
    expect(vec(23, 0x3FC00000u, 0x3FC00000u, 0x3FC00000u, 0x40400000u), "DIV / WAITQ / MULq");
    expect(vec(24, 0x3E4CCCCCu, 0x3E4CCCCCu, 0x3E4CCCCCu, 0x3E4CCCCCu), "ERLENG / WAITP / MFP");
    expect(vec(25, 0x3F490FDBu, 0x3E4CCCCCu, 0, 0), "EATANxy; P latency");
    expect(vec(26, 0x0E, 0x30, 0, 0x05), "MAC flag delay (FMAND), clipping flag delay (FCGET)");
    expect(vec(27, 0x140, 1, 0, 0), "CLIP history, FCAND");

    /* D: TOP / ITOP through the VIF's double buffer, XGKICK */
    s[0] = 0x01000101u; /* STCYCL 1, 1 */
    s[1] = 0x03000100u; /* BASE 0x100 */
    s[2] = 0x02000080u; /* OFFSET 0x80 */
    s[3] = 0x0400002Au; /* ITOP 0x2A */
    s[4] = 0x6C028000u; /* UNPACK V4-32, 2 quadwords at TOPS + 0 */
    s[5] = 0x00008001u; s[6] = 0x10000000u; s[7] = 0xEu; s[8] = 0;
    s[9] = 0x12345678u; s[10] = 0xFFFFFFFFu; s[11] = 0x62u; s[12] = 0;
    s[13] = 0x140000C0u; /* MSCAL 0xC0 */
    Gs_Vif1(s, 14);
    expect(vec(28, 0x100, 0x2A, 0, 0), "XTOP / XITOP");
    expect(label() == 0x12345678u, "XGKICK of the batch at TOP (LABEL)");
    s[4] = 0x6C028000u;
    s[9] = 0x9ABCDEF0u;
    Gs_Vif1(&s[4], 10);
    expect(vec(28, 0x180, 0x2A, 0, 0), "XTOP after the double buffer swapped");
    expect(label() == 0x9ABCDEF0u, "XGKICK of the second batch");

    /* E */
    regs_clear();
    put(40, 0x40200000u, 0x00018005u, 0xBFC00000u, 0x00000F0Fu); /* 2.5, (int), -1.5, (int) */
    label_packet(0x3FF, 0xCAFEBABEu);
    vif(0x140000D0u);
    expect(vec(49, 0x40200000u, 0x00018005u, 0xBFC00000u, 0x00000F0Fu), "LQI / SQD");
    expect(vec(50, 0x7FFF8005u, 0x7FFF8005u, 0x7FFF8005u, 0x7FFF8005u), "MTIR / MFIR (sign-extended) / ABS");
    expect(vec(51, 0x40200000u, 0, 0, 0), "MAXx");
    expect(vec(52, 0, 0, 0xBFC00000u, 0), "MINIz");
    expect(vec(53, 0x28, 0, 0xFFFFFFE8u, 0), "FTOI4");
    expect(vec(54, 0x40200000u, 0, 0xBFC00000u, 0), "ITOF4");
    expect(vec(55, 41, 49, 5, 0x8F0F), "LQI / SQD address steps, IAND, IOR");
    expect(vec(56, 0x8F0A, 0x0F0F, 0, 0), "ISUB, ILW");
    expect(label() == 0xCAFEBABEu, "XGKICK across the end of VU memory");
    expect(Vu_Unknown(&sVu) == 0, "no instruction left unimplemented");

    Gs_Vif1Reset();
    Gs_Vu1Reset();
    memset(VU1_DATA, 0, 0x4000);
    memset(VU1_MICRO, 0, 0x4000);
    sFails += Vu_FastSelfTest();
    return sFails;
}

/* VU1's registers and pipeline (the unit's pointers name this file's variables and VU memory: the same in every process
   of the build). Not the predecoded program (ops, opsInit): each slot is checked against micro memory when it runs. */
__attribute__((constructor(101))) static void vu1_state(void) {
    Port_StateAdd("vu1.vf", sVf, sizeof sVf, 0);
    Port_StateAdd("vu1.vi", sVi, sizeof sVi, 0);
    Port_StateAdd("vu1.acc", sAcc, sizeof sAcc, 0);
    Port_StateAdd("vu1.q", &sQ, sizeof sQ, 0);
    Port_StateAdd("vu1.core", &sVu, (uint32_t)offsetof(VuCore, ops), 0);
}
