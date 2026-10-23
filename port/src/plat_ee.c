/*
 * EE instructions the game writes as inline assembly (include/ee_asm.h) and the VU0 register file that the
 * hand-written VU0 code works on (port/src/vu0_pc.c).
 */
#include <stdint.h>
#include <string.h>

/* COP0 Status bit 16 (EIE): interrupts enabled. di / ei only change it; the game spins until a read shows it clear. */
static uint32_t sStatus = 0x10000;

void Port_Di(void) { sStatus &= ~0x10000u; }
void Port_Ei(void) { sStatus |= 0x10000u; }
uint32_t Port_Mfc0Status(void) { return sStatus; }

/* sqrt.s on the bits, rounded toward zero (the EE does not round to nearest); a negative operand uses its absolute
 * value and zero / denormals give zero, as on the EE. Checked against the EE model in T23. */
float Port_SqrtS(float x) {
    union { float f; uint32_t u; } v = { x };
    uint32_t e = (v.u >> 23) & 0xFF, m, r, b;
    uint64_t rem, mant;
    if (e == 0) {
        v.u &= 0x80000000u;
        v.u = 0;
        return v.f;
    }
    m = (v.u & 0x7FFFFF) | 0x800000;
    e -= 127;
    if (e & 1) { /* odd exponent: take one bit into the mantissa */
        m <<= 1;
    }
    e = (uint32_t)((int32_t)e >> 1);
    mant = (uint64_t)m << 23; /* sqrt of mant has 24 significant bits */
    r = 0;
    rem = 0;
    for (b = 1u << 23; b; b >>= 1) {
        uint64_t t = (uint64_t)(r | b);
        if (t * t <= mant) {
            r |= b;
        }
    }
    (void)rem;
    v.u = ((e + 127) << 23) | (r & 0x7FFFFF);
    return v.f;
}

/* VU0 floating-point registers vf0..vf31, four floats each; vf0 is (0, 0, 0, 1). */
float gPortVu0Vf[32][4] __attribute__((aligned(16))) = { { 0.0f, 0.0f, 0.0f, 1.0f } };

/* lqc2 of n consecutive quadwords into vf<first>.. */
void Port_Vu0LoadVf(int first, const void *src, int n) {
    if (first > 0 && first + n <= 32) {
        memcpy(gPortVu0Vf[first], src, (size_t)n * 16);
    }
}
