/* Game addresses in the port's own code.

   The game keeps addresses in 4 bytes: its pointers, the DMA chains, the hardware registers it writes. In the
   32-bit build a game address IS the host address, so the conversions are plain casts. T22 maps the PS2 regions
   the game names by number (scratchpad 0x70000000, hardware registers 0x10000000-0x1100FFFF, VU memory, the heap at
   its PS2 addresses) into the process; until then a game address that is not a host object faults when used.

   Port_Ptr: a game address (a 4-byte value read from the game's memory) -> a pointer to use here.
   Port_Addr: a pointer to memory the game can reach -> the game address to hand it or to store in its memory.
   Both keep 0 / NULL. Pointers the game passes to a port function, and those a port function returns, need neither:
   they cross calls as ordinary pointers. */
#ifndef PORT_ADDR_H
#define PORT_ADDR_H
#include <stdint.h>

#define Port_Ptr(addr) ((void *)(uintptr_t)(uint32_t)(addr))
#define Port_Addr(p) ((uint32_t)(uintptr_t)(p))

/* The 32-bit build relies on these: a game pointer field and a host pointer have the same size. */
typedef char port_addr_pointer_is_4_bytes[sizeof(void *) == 4 ? 1 : -1];

#endif
