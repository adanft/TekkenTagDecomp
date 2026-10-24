/*
 * EE instructions the game writes as inline assembly (include/ee_asm.h) and the VU0 register file that the
 * hand-written VU0 code works on (port/src/vu0_pc.c).
 */
#include <stdint.h>
#include <string.h>
#include "plat_state.h"

/* COP0 Status bit 16 (EIE): interrupts enabled. di / ei only change it; the game spins until a read shows it clear. */
static uint32_t sStatus = 0x10000;

void Port_HwDeliver(void); /* plat_hw.c: interrupts raised while they were off */
void Port_Di(void) { sStatus &= ~0x10000u; }
void Port_Ei(void) {
    sStatus |= 0x10000u;
    Port_HwDeliver();
}
uint32_t Port_Mfc0Status(void) { return sStatus; }
void Port_SetStatus(uint32_t s) { sStatus = s; }

/* sqrt.s: the EE FPU's square root of |x|, rounded to nearest (softfloat_ps2.c: zero and denormals give +0). */
uint32_t Port_FpuSqrt(uint32_t a);
uint32_t Port_SqrtS(uint32_t x) { return Port_FpuSqrt(x); }

/* VU0 floating-point registers vf0..vf31, four floats each; vf0 is (0, 0, 0, 1). */
float gPortVu0Vf[32][4] __attribute__((aligned(16))) = { { 0.0f, 0.0f, 0.0f, 1.0f } };

/* lqc2 of n consecutive quadwords into vf<first>.. */
void Port_Vu0LoadVf(int first, const void *src, int n) {
    if (first > 0 && first + n <= 32) {
        memcpy(gPortVu0Vf[first], src, (size_t)n * 16);
    }
}

__attribute__((constructor(101))) static void ee_state(void) {
    Port_StateAdd("ee.status", &sStatus, sizeof sStatus, PS_HASH);
    Port_StateAdd("vu0.vf", gPortVu0Vf, sizeof gPortVu0Vf, PS_HASH);
}
