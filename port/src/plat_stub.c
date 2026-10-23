/*
 * Stand-ins of the PC build. A function the port does not provide yet reports its name and stops the program, so a
 * run shows what the game needs next. The Sony SDK, libc and libgcc names the game calls are generated the same way
 * by port/tools/link.py (build/port/stubs.txt lists them); they move here, or to their own platform file, as they
 * get real implementations.
 *
 * Port_Hit records the placeholders that were reached (the generated stubs under TTT_STUB_RETURN, and the
 * port's own "does nothing yet" functions of plat_lib.c); a headless run lists them at its end (Port_StubReport).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Port_HwSummary(void); /* plat_hw.c */

void Port_Missing(const char *name) {
    printf("ttt: not implemented yet: %s\n", name);
    Port_HwSummary();
    exit(3);
}

#define MAX_HITS 256
static const char *sHits[MAX_HITS];
static int sHitCount;

void Port_Hit(const char *name) {
    int i;
    for (i = 0; i < sHitCount; i++) {
        if (sHits[i] == name || strcmp(sHits[i], name) == 0) {
            return;
        }
    }
    if (sHitCount < MAX_HITS) {
        sHits[sHitCount++] = name;
    }
}

void Port_StubReport(void) {
    int i;
    printf("ttt: placeholders hit (%d):", sHitCount);
    for (i = 0; i < sHitCount; i++) {
        printf(" %s", sHits[i]);
    }
    printf("\n");
}
