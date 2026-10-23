/* The vector units' micro mode, for VU0 (vu0_micro.c) and VU1 (gs_vu1.c): the reference interpreter (vu_micro.c,
   integer arithmetic) and the predecoded one (vu_fast.c, the same results on the SSE unit). The arithmetic is the EE
   float model's vector unit (softfloat_ps2_inl.h). */
#ifndef PORT_VU_MICRO_H
#define PORT_VU_MICRO_H
#include <stdint.h>

typedef struct VuFlagStep {
    uint32_t cycle, value;
} VuFlagStep;

/* An upper instruction's result on its way to the MAC flags: the flags are worked out from it only when read. */
typedef struct VuMacStep {
    uint32_t cycle, mask, res[4];
} VuMacStep;

/* One micro-memory instruction decoded for the predecoded interpreter (vu_fast.c): the raw words it was decoded
   from (a slot whose micro memory changed is decoded again when it runs), what the upper and lower halves do and
   their operands. */
typedef struct VuOp {
    uint32_t lo, hi;
    uint8_t up, bk, tgt, dest; /* upper operation, its second operand (vector / broadcast / Q / I), result target */
    uint8_t fs, ft, fd, bc;
    uint8_t lw, ld, is, it;    /* lower operation, its dest mask and registers */
    uint8_t id, fsf, ftf, flags;
    uint32_t imm, aux;         /* immediate (or the unknown counter's index + 128); branch target */
} VuOp;

typedef struct VuCore {
    /* registers: VU0's are shared with macro mode (plat_ee.c, vu0_micro.c), so they are reached through pointers */
    uint32_t (*vf)[4];
    uint16_t *vi;
    uint32_t *acc, *q;
    uint32_t i, p;
    /* memory: micro memory (instructions), data memory (quadwords); sizes are powers of two */
    const uint8_t *micro;
    uint8_t *data;
    uint32_t microMask; /* instruction index mask: 0x1FF (VU0, 4 KB) or 0x7FF (VU1, 16 KB) */
    uint32_t dataMask;  /* quadword index mask: 0xFF (VU0) or 0x3FF (VU1) */
    /* VU1 only (vu1 != 0): XTOP / XITOP values latched by the VIF, XGKICK */
    int vu1;
    uint32_t top, itop;
    void (*kick)(uint32_t qw);
    /* pipeline state */
    uint32_t cycle, qPending, qReady, pPending, pReady;
    uint32_t clip, clipLatest, mac;
    VuFlagStep clipQueue[8];
    VuMacStep macQueue[8];
    VuMacStep macNow; /* the result the MAC flags show now (macLazy: mac is not worked out yet) */
    int macLazy;
    int clipHead, clipCount, macHead, macCount;
    int settle;  /* at the end of a program, finish Q / P / flags still in flight (time passes before the next) */
    uint32_t pc; /* where the last program stopped: MSCNT continues there */
    /* statistics */
    uint64_t instr;
    unsigned unknown[2][128];
    /* the fast interpreter's cache of decoded instructions, one per micro-memory slot (microMask + 1 of them); NULL:
       Vu_Run steps with the reference interpreter */
    VuOp *ops;
    int opsInit;
} VuCore;

/* Runs from instruction `pc` until the E bit and its delay slot; returns 0, or -1 if the program did not end. With
   u->ops the predecoded interpreter, else the reference; both leave the same registers, memory and pipeline state. */
int Vu_Run(VuCore *u, uint32_t pc);
/* The reference interpreter: one instruction decoded and stepped at a time, every flag and latency every step. */
int Vu_RunRef(VuCore *u, uint32_t pc);
/* Whether two units are in the same state: registers, data memory, pipeline (by content), counters. */
int Vu_SameState(VuCore *a, VuCore *b);
/* TTT_SELFTEST=vu1: random programs through both interpreters, the 4-lane arithmetic against the scalar model;
   prints a summary line and returns the number of differences. */
int Vu_FastSelfTest(void);
/* Instructions decoded but not implemented so far (each counted once per occurrence). */
unsigned Vu_Unknown(const VuCore *u);

#endif
