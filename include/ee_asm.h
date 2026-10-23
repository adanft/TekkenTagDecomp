#ifndef EE_ASM_H
#define EE_ASM_H

/* Single EE instructions the game code writes as inline assembly. The matching build gets exactly the statement the
 * C files had before these names existed (the same tokens, so the same code); the PC build (PORT) gets their effect
 * from the platform layer (port/src/plat_ee.c) or nothing where the instruction only orders memory or the pipeline. */
#ifndef PORT

#define EE_DI() __asm__ volatile("di")
#define EE_EI() __asm__ volatile("ei")
#define EE_SYNC() __asm__ volatile("sync")
#define EE_SYNC_L() __asm__ volatile("sync.l")
#define EE_SYNC_P() __asm__ volatile("sync.p")
#define EE_NOP() __asm__ volatile("nop")
/* COP0 Status into v (the interrupt-enable loop: `do { di; sync.p; mfc0 } while (status & 0x10000)`). */
#define EE_MFC0_STATUS(v) __asm__ volatile("mfc0 %0, $12" : "=r"(v))
#define EE_MFC0_STATUS_MEM(v) __asm__ volatile("mfc0 %0, $12" : "=r"(v) : : "memory")
/* r = sqrt(x) with the two nops the original carries in front of sqrt.s. */
#define EE_SQRT_S(r, x) __asm__ volatile("nop\n\tnop\n\tsqrt.s %0, %1" : "=f"(r) : "f"(x))
/* Waits for the previous toSPR transfer (BC0F loop). */
#define EE_WAIT_BC0F() __asm__ volatile("1:\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tbc0f 1b")

#else

void Port_Di(void);
void Port_Ei(void);
unsigned int Port_Mfc0Status(void);
float Port_SqrtS(float x);
void Port_Vu0LoadVf(int first, const void *src, int n); /* lqc2 into VU0 vf<first>.. */
#define EE_DI() Port_Di()
#define EE_EI() Port_Ei()
#define EE_SYNC() __asm__ volatile("" : : : "memory")
#define EE_SYNC_L() __asm__ volatile("" : : : "memory")
#define EE_SYNC_P() __asm__ volatile("" : : : "memory")
#define EE_NOP() ((void)0)
#define EE_MFC0_STATUS(v) ((v) = Port_Mfc0Status())
#define EE_MFC0_STATUS_MEM(v) ((v) = Port_Mfc0Status())
#define EE_SQRT_S(r, x) ((r) = Port_SqrtS(x))
#define EE_WAIT_BC0F() ((void)0)

#endif

#endif /* EE_ASM_H */
