/* Game addresses in the port's own code.

   The game keeps addresses in 4 bytes: its pointers, the DMA chains, the hardware registers it writes. The 32-bit
   build maps the PS2's address space at the PS2's addresses (plat_mem.c): EE RAM with the game's data and bss at
   0x00010000-0x02000000, its uncached views at 0x20000000 and 0x30000000, the hardware registers at 0x10000000,
   0x11000000 (VU memory) and 0x12000000 (GS), the scratchpad at 0x70000000. So a game address IS the host address
   and the conversions are plain casts.

   Port_Ptr: a game address (a 4-byte value read from the game's memory) -> a pointer to use here.
   Port_Addr: a pointer to memory the game can reach -> the game address to hand it or to store in its memory.
   Both keep 0 / NULL. Pointers the game passes to a port function, and those a port function returns, need neither:
   they cross calls as ordinary pointers.

   What is not identity-mapped: the first 64 KB of EE RAM (Linux refuses to map them; reached at 0x20000000 +
   addr) and KSEG0 / KSEG1 (0x80000000 / 0xA0000000, which the game does not use). Port_DmaPtr gives the memory a
   DMA channel addresses: bit 31 set selects the scratchpad, otherwise physical EE RAM. */
#ifndef PORT_ADDR_H
#define PORT_ADDR_H
#include <stdint.h>

#define Port_Ptr(addr) ((void *)(uintptr_t)(uint32_t)(addr))
#define Port_Addr(p) ((uint32_t)(uintptr_t)(p))

#define PORT_EE_RAM_SIZE 0x02000000u
#define PORT_SPR_ADDR 0x70000000u
#define PORT_SPR_SIZE 0x4000u

/* The physical EE RAM address of any of its views (0x0..., 0x2... uncached, 0x3... uncached accelerated). */
#define Port_EePhys(addr) ((uint32_t)(addr) & 0x0FFFFFFFu)
/* Whether a game address is EE RAM, through any of its views. */
#define Port_IsEeRam(addr)                                                                                          \
    ((((uint32_t)(addr) >> 28) == 0 || ((uint32_t)(addr) >> 28) == 2 || ((uint32_t)(addr) >> 28) == 3) &&          \
     Port_EePhys(addr) < PORT_EE_RAM_SIZE)
/* A DMA address (MADR / TADR / a tag's ADDR): the scratchpad when bit 31 is set, else EE RAM (the 64 KB the host
   cannot map at 0 through the uncached view). */
#define Port_DmaPtr(addr)                                                                                           \
    (((uint32_t)(addr) & 0x80000000u) ? Port_Ptr(PORT_SPR_ADDR + ((uint32_t)(addr) & (PORT_SPR_SIZE - 1)))           \
     : (((uint32_t)(addr) & 0x0FFFFFFFu) < 0x10000u) ? Port_Ptr(0x20000000u | ((uint32_t)(addr) & 0x0FFFFFFFu))   \
                                                     : Port_Ptr((uint32_t)(addr) & 0x0FFFFFFFu))

/* The 32-bit build relies on these: a game pointer field and a host pointer have the same size. The 64-bit build
   (TTT_CC=clang64, docs/port/README.md "64-bit") keeps the game's pointers 4 bytes wide by rewriting the game's code
   (port/tools/ptr32.py) and keeps everything the game addresses below 4 GB, so the casts above stay right; the port's
   own pointers are 8 bytes there, and a game structure the port declares must give its pointer fields the game's
   width: `T *PORT_GPTR field` (clang's `__ptr32 __uptr` there, -fms-extensions: a 4-byte pointer, zero-extended when
   read; nothing in the 32-bit build). */
#if defined(__x86_64__)
#define PORT_GPTR __ptr32 __uptr
#else
#define PORT_GPTR
#endif
#if defined(__x86_64__)
typedef char port_addr_host_is_64_bit[sizeof(void *) == 8 ? 1 : -1];
#else
typedef char port_addr_pointer_is_4_bytes[sizeof(void *) == 4 ? 1 : -1];
#endif

#endif
