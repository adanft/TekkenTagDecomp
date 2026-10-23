/*
 * newlib's rand / srand as SLUS_200.01 has them (0x40FBD0 / 0x40FBC0). The game's random numbers are simulation
 * state (docs/known_bugs.md, section 8), so they must be the PS2 C library's, not the host's: the 32-bit linear
 * congruential generator of newlib's reentrancy structure, next = next * 1103515245 + 12345, result next & 0x7FFFFFFF.
 * The state is the game's own word in EE RAM, `_rand_next` at offset 0x58 of the structure `_impure_ptr` (D_004F5A8C)
 * points to, so it is saved, hashed and compared with the rest of the game's memory.
 */
#include <stdint.h>

extern uint8_t *D_004F5A8C; /* _impure_ptr */

void srand(unsigned seed) { *(uint32_t *)(D_004F5A8C + 0x58) = seed; }

int rand(void) {
    uint32_t *next = (uint32_t *)(D_004F5A8C + 0x58);

    *next = *next * 1103515245u + 12345u;
    return (int)(*next & 0x7FFFFFFFu);
}
