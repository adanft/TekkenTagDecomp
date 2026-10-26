/*
 * Memory of the PC build: the PS2's address space, at the PS2's addresses.
 *
 * The game keeps addresses in 4-byte fields, writes hardware registers by number, keeps fixed buffers in EE RAM
 * outside its program (0x100000-0x33F000) and makes uncached views of its own variables (`addr | 0x20000000`,
 * `| 0x30000000`). So the 32-bit build gives it the PS2's map (docs/port/README.md, "Memory map"):
 *
 *   0x00010000..0x02000000  EE RAM (32 MB), one shared memory object. The game's data is copied into it at its
 *                           PS2 addresses from the user's disc (plat_data.c; the program carries none of it), its
 *                           bss is the object's zeroes; the first 64 KB cannot be mapped (vm.mmap_min_addr) and are
 *                           reached only through the aliases.
 *   0x20000000, 0x30000000  the uncached and uncached-accelerated views of EE RAM: the same memory object mapped
 *                           again, so a write through one view is seen through the others, as on the PS2.
 *   0x10000000 (64 KB)      EE hardware registers (timers, IPU, GIF, VIF, DMA, INTC, SIF);
 *   0x11000000 (64 KB)      VU0 / VU1 micro and data memory;
 *   0x12000000 (8 KB)       GS privileged registers: plain memory here, so a read gives what was last written
 *                           (0 at start: "transfer finished"). T24 / T28 put behaviour behind them.
 *   0x70000000 (16 KB)      the scratchpad.
 *
 * The program's own code, the C library and the host stack stay where Linux puts them (all above 0x08000000 and
 * below 4 GB in a 32-bit process). KSEG0 / KSEG1 (0x80000000 / 0xA0000000) are not mapped: the game does not use
 * them. Everything is set up by a constructor, before the game's main() and before any game data is touched.
 */
#include "port_addr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat_os.h"

#define EE_FIRST 0x00010000u /* the lowest address Linux lets a program map (vm.mmap_min_addr) */

/* Bounds of the game's data at its PS2 addresses (image.ld: absolute symbols, nothing is linked there). */
extern char __ttt_data_start[], __ttt_data_end[], __ttt_bss_start[], __ttt_bss_end[];

static int sEeRam = -1;
static int sDataLoaded;

void Port_LoadGameData(void); /* plat_data.c */

static void fail(const char *what, uint32_t addr) {
    fprintf(stderr, "ttt: cannot map %s at 0x%08X\n", what, addr);
    exit(2);
}

/* Anonymous memory at a fixed address; refused (not replaced) if anything is there already. */
static void map_plain(uint32_t addr, uint32_t size, const char *what) {
    if (PlatOs_MapFixed(Port_Ptr(addr), size) != 0) {
        fail(what, addr);
    }
}

/* A view of EE RAM (offset `off`, `size` bytes) at `addr`. */
static void map_ee(uint32_t addr, uint32_t off, uint32_t size, int replace, const char *what) {
    if (PlatOs_MapView(sEeRam, Port_Ptr(addr), off, size, replace) != 0) {
        fail(what, addr);
    }
}

/* EE RAM must hold nothing before it is mapped (the program has no segment there any more; a build that links
   something at those addresses is allowed its own range): anything else there (the host's code or heap) would be
   overwritten without a word. */
static void check_ee_range(uint32_t lo, uint32_t hi) {
    char what[512];
    uint32_t img_lo = Port_Addr(__ttt_data_start) & ~0xFFFu, img_hi = (Port_Addr(__ttt_bss_end) + 0xFFFu) & ~0xFFFu;

    if (PlatOs_RangeTaken(lo, hi, img_lo, img_hi, what, sizeof what)) {
        fprintf(stderr, "ttt: EE RAM range is taken: %s", what);
        exit(2);
    }
}

static void ee_ram(void) {
    sEeRam = PlatOs_MemCreate("ttt-ee-ram", PORT_EE_RAM_SIZE);
    if (sEeRam < 0) {
        fail("EE RAM (no memory object)", 0);
    }
    check_ee_range(EE_FIRST, PORT_EE_RAM_SIZE);
    map_ee(EE_FIRST, EE_FIRST, PORT_EE_RAM_SIZE - EE_FIRST, 1, "EE RAM");
    map_ee(0x20000000u, 0, PORT_EE_RAM_SIZE, 0, "the uncached view of EE RAM");
    map_ee(0x30000000u, 0, PORT_EE_RAM_SIZE, 0, "the uncached-accelerated view of EE RAM");
}

/* ---- Self-test (TTT_SELFTEST=mem): the map above, on the running program. Prints and exits. */
static int sFailed;

static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: memory self-test FAILED: %s\n", what);
        sFailed = 1;
    }
}

static int all_zero(const uint8_t *p, uint32_t n) {
    while (n-- != 0) {
        if (*p++ != 0) {
            return 0;
        }
    }
    return 1;
}

static void self_test(void) {
    volatile uint32_t *ram = Port_Ptr(0x00100000u), *unc = Port_Ptr(0x20100000u), *acc = Port_Ptr(0x30100000u);
    uint32_t data = Port_Addr(__ttt_data_start), dlen = Port_Addr(__ttt_data_end) - data;
    uint32_t bss = Port_Addr(__ttt_bss_start), blen = Port_Addr(__ttt_bss_end) - bss;
    volatile uint8_t *spr = Port_Ptr(0x70000000u);

    *ram = 0x11223344u;
    expect(*unc == 0x11223344u && *acc == 0x11223344u, "a write to EE RAM is seen through 0x20000000 / 0x30000000");
    *acc = 0x55667788u;
    expect(*ram == 0x55667788u && *unc == 0x55667788u, "a write through 0x30000000 is seen in EE RAM");
    *ram = 0;
    *(volatile uint32_t *)Port_Ptr(0x20000000u) = 0xA5A5A5A5u; /* the first 64 KB: aliases only */
    expect(*(volatile uint32_t *)Port_Ptr(0x30000000u) == 0xA5A5A5A5u, "the first 64 KB through both aliases");
    *(volatile uint32_t *)Port_Ptr(0x20000000u) = 0;

    expect(data >= EE_FIRST && data + dlen <= bss && bss + blen <= PORT_EE_RAM_SIZE,
           "the game's data and bss lie inside EE RAM, data first");
    expect(!sDataLoaded || (dlen != 0 && !all_zero(Port_Ptr(data), dlen)), "the game's data was copied into EE RAM");
    expect(memcmp(Port_Ptr(data), Port_Ptr(data | 0x20000000u), dlen) == 0, "the game's data through 0x20000000");
    expect(all_zero(Port_Ptr(bss | 0x30000000u), blen), "the bss is zero at start");
    *(volatile uint32_t *)Port_Ptr(bss) = 0xCAFEF00Du;
    expect(*(volatile uint32_t *)Port_Ptr(bss | 0x20000000u) == 0xCAFEF00Du, "a bss variable through 0x20000000");
    *(volatile uint32_t *)Port_Ptr(bss) = 0;
    *(volatile uint32_t *)Port_Ptr(0x01FFFFFCu) = 1; /* the PS2's stack, at the top of EE RAM */
    expect(*(volatile uint32_t *)Port_Ptr(0x31FFFFFCu) == 1, "the last word of EE RAM");
    *(volatile uint32_t *)Port_Ptr(0x01FFFFFCu) = 0;

    spr[0] = 1;
    spr[0x3FFF] = 2;
    expect(spr[0] == 1 && spr[0x3FFF] == 2, "the 16 KB scratchpad at 0x70000000");
    expect(Port_DmaPtr(0x80000010u) == (void *)(spr + 0x10), "a DMA address with the scratchpad bit");
    expect(Port_DmaPtr(0x00100000u) == (void *)ram, "a DMA address in EE RAM");
    spr[0] = spr[0x3FFF] = 0;

    *(volatile uint32_t *)Port_Ptr(0x1000D400u) = 0;
    expect(*(volatile uint32_t *)Port_Ptr(0x1000D400u) == 0, "the DMA registers (0x10000000)");
    *(volatile uint32_t *)Port_Ptr(0x1000FFFCu) = 7;
    expect(*(volatile uint32_t *)Port_Ptr(0x1000FFFCu) == 7, "the last EE register word");
    *(volatile uint32_t *)Port_Ptr(0x1100FFFCu) = 8;
    expect(*(volatile uint32_t *)Port_Ptr(0x1100FFFCu) == 8, "VU memory (0x11000000)");
    *(volatile uint64_t *)Port_Ptr(0x12001000u) = 9;
    expect(*(volatile uint64_t *)Port_Ptr(0x12001000u) == 9, "the GS privileged registers (0x12000000)");

    expect(Port_EePhys(0x30415700u) == 0x00415700u && Port_EePhys(0x20415700u) == 0x00415700u,
           "Port_EePhys drops the view bits");
    expect(Port_IsEeRam(0x20415700u) && Port_IsEeRam(0x01FFFFFFu) && !Port_IsEeRam(0x02000000u) &&
               !Port_IsEeRam(0x10000000u) && !Port_IsEeRam(0x70000000u),
           "Port_IsEeRam");
    expect(Port_Addr(Port_Ptr(0x00415700u)) == 0x00415700u && Port_Ptr(0) == NULL, "Port_Ptr / Port_Addr");
    if (!sFailed) {
        printf("ttt: memory self-test OK (EE RAM 0x%08X-0x%08X, data 0x%08X+0x%X, bss 0x%08X+0x%X)\n", EE_FIRST,
               PORT_EE_RAM_SIZE, data, dlen, bss, blen);
    }
    exit(sFailed);
}

__attribute__((constructor(101))) static void Port_MapMemory(void) {
    const char *t = getenv("TTT_SELFTEST");

    ee_ram();
    Port_LoadGameData(); /* the game's data from the user's disc into EE RAM (plat_data.c) */
    sDataLoaded = !all_zero(Port_Ptr(Port_Addr(__ttt_data_start)), 4096);
    map_plain(0x10000000u, 0x10000, "the EE hardware registers");
    map_plain(0x11000000u, 0x10000, "the VU memory");
    map_plain(0x12000000u, 0x2000, "the GS privileged registers");
    map_plain(0x70000000u, 0x4000, "the scratchpad");
    if (t != NULL && strcmp(t, "mem") == 0) {
        self_test();
    }
}
