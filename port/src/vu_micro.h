/* The vector units' micro mode (vu_micro.c): one interpreter for VU0 (vu0_micro.c) and VU1 (gs_vu1.c). Integer
   only: the arithmetic is the EE float model's vector unit (softfloat_ps2_inl.h). */
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
} VuCore;

/* Runs from instruction `pc` until the E bit and its delay slot; returns 0, or -1 if the program did not end. */
int Vu_Run(VuCore *u, uint32_t pc);
/* Instructions decoded but not implemented so far (each counted once per occurrence). */
unsigned Vu_Unknown(const VuCore *u);

#endif
