/* The GS path of the PC build: DMA (plat_hw.c) -> VIF1 (gs_vif1.c) -> GIF -> GS registers, GS memory and the
   software reference rasteriser (gs_core.c). Integer only: nothing here uses the host's float unit. */
#ifndef PORT_GS_H
#define PORT_GS_H
#include <stdint.h>

/* GIF paths: 1 = VU1 XGKICK, 2 = VIF1 DIRECT / DIRECTHL, 3 = the GIF DMA channel. Each keeps its own tag. */
void Gs_Gif(int path, const uint8_t *p, uint32_t qwc);
void Gs_RegWrite(uint32_t addr, uint64_t d);           /* one GS register (A+D addresses) */
uint32_t Gs_VramRead(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y);
void Gs_VramWrite(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, uint32_t v);
void Gs_Reset(void);                                   /* sceGsResetGraph: drawing state back to power-on */
void Gs_VBlank(uint32_t vblank);                       /* plat_hw.c, at each VBLANK_START: the frame dump */
void Gs_Report(void);                                  /* TTT_FRAMES report line */

/* VIF1 (gs_vif1.c): `count` words of a VIF1 transfer. VU1 memory is the EE's view of it: micro memory at 0x11008000,
   data memory at 0x1100C000 (16 KB each). */
void Gs_Vif1(const uint32_t *w, uint32_t count);
void Gs_Vif1Reset(void);
extern uint32_t gGsVu1Calls; /* MSCAL / MSCALF / MSCNT seen */
extern uint32_t gGsVu1Mpg;   /* MPG uploads */

/* VU1 (gs_vu1.c): run the program at instruction `addr` (MSCAL / MSCALF), or where the last one stopped (addr < 0,
   MSCNT), with TOP / ITOP as the VIF latched them; XGKICK feeds PATH1. */
void Gs_Vu1Run(int addr, uint32_t top, uint32_t itop);
void Gs_Vu1Reset(void);
void Gs_Vu1Report(void);

#define GS_PRIV(off) (*(volatile uint64_t *)(uintptr_t)(0x12000000u + (off)))
#define GS_PMODE 0x0000u
#define GS_SMODE2 0x0020u
#define GS_DISPFB1 0x0070u
#define GS_DISPLAY1 0x0080u
#define GS_DISPFB2 0x0090u
#define GS_DISPLAY2 0x00A0u
#define GS_BGCOLOR 0x00E0u
#define GS_CSR 0x1000u
#define GS_SIGLBLID 0x1080u

#endif
