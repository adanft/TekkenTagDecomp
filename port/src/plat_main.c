/*
 * The game runs on a stack in EE RAM, where the PS2's kernel put it: just below the top of the 32 MB
 * (crt0's SetupThread asks for "stack -1": the end of memory). Game code takes addresses of its locals and hands
 * them to the DMA (func_0035D790 sends a quadword on its stack to the scratchpad): a DMA address is the game
 * address + 0x10000000 masked with 0x8FFFFFFF, which only works for addresses below 0x10000000. The host stack
 * (0xFFxxxxxx) would wrap.
 *
 * The link wraps `main` (port/tools/link.py: -Wl,--wrap=main): __wrap_main moves the stack pointer to the EE stack
 * and calls the game's main. The space between the end of .bss (0x01FCDEF0) and 0x02000000 is 200 KB; on the PS2
 * the same stack grows into the top of .bss as well (docs/game_overview.md section 1).
 */
#include <stdint.h>
#include <stdlib.h>

#define PORT_EE_STACK_TOP 0x01FFFF00u

int __real_main(int argc, char **argv);

int __wrap_main(int argc, char **argv) {
    __asm__ volatile("movl %0, %%esp\n\t"
                     "subl $8, %%esp\n\t" /* 16-byte aligned at the call */
                     "pushl %2\n\t"
                     "pushl %1\n\t"
                     "call __real_main\n\t"
                     "subl $12, %%esp\n\t"
                     "pushl %%eax\n\t"
                     "call exit"
                     :
                     : "r"(PORT_EE_STACK_TOP), "r"(argc), "r"(argv)
                     : "memory");
    __builtin_unreachable();
}
