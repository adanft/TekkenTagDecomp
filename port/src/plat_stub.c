/*
 * Stand-ins of the PC build. A function the port does not provide yet reports its name and stops the program, so a
 * run shows what the game needs next. The Sony SDK, libc and libgcc names the game calls are generated the same way
 * by port/tools/link.py (build/port/stubs.txt lists them); they move here, or to their own platform file, as they
 * get real implementations (T24: kernel, SIF, CD, VBlank).
 */
#include <stdio.h>
#include <stdlib.h>

void Port_Missing(const char *name) {
    printf("ttt: not implemented yet: %s\n", name);
    exit(3);
}
