/*
 * The simulation's state (T27, docs/port/determinism.md): everything a run's future depends on, as one block that can
 * be hashed, saved and put back at a VBlank boundary. The base of rollback netplay (T45).
 *
 * What it is: the PS2's memory (EE RAM with the game's data, bss and stack, the scratchpad, the EE hardware registers,
 * VU0 / VU1 memory, the GS privileged registers) and the regions each module of the port registers from a constructor
 * (Port_StateAdd: timers, DMA / interrupt state, VU registers and pipelines, VIF, the GS front end and GS memory, the
 * SPU2 model and sound RAM, the BGM streamer, RSPU2DRV status, pads, the memory card's request, the kernel's tables).
 * The game's code runs natively on the EE stack, so the CPU's registers are the host's. The boundary itself is a stack
 * switch (Port_StateBoundary): it pushes the callee-saved registers on the EE stack (in EE RAM), keeps the stack
 * pointer and runs everything else on a service stack; a save copies the regions and that stack pointer, a load copies
 * them back and the switch then returns into the saved boundary. The switch writes the same bytes on the EE stack in
 * every run, whether a state is taken or not, and the port's waits on its threads run off the EE stack too
 * (Port_OffEe): the game reads stale stack words in places (func_003D2818's camera key), which must not depend on
 * the host's timing. A state file holds no host heap address, so it loads into a fresh process of the same build
 * (the build id is checked). Not saved (host side, drained or rebuilt instead): the GS threads' queues and the raster
 * workers (idle at the boundary: Gs_Finish), the predecoded VU programs (decoded again), the window, the audio ring,
 * files.
 *
 * The gameplay hash (Port_StateHash): XXH3-64 over the regions marked PS_HASH. The game keeps host addresses in its
 * memory (callbacks in its tables and object pools, gotcha 20, and values derived from them), and they move whenever
 * the code changes size. Mapping them to function names is not exact: game data (packed bytes, texture words) holds
 * about a hundred words per run that equal some function's address in one build and not in the next, and some stored
 * values are code addresses plus an offset. So every word inside a fixed window [HOST_LO, HOST_HI) that holds the
 * whole program image (code and variables) hashes as one constant: the same in every build, for real pointers and
 * for data that happens to fall there alike (about 0.3 % of the words). The EE stack is left out (return addresses,
 * frame layout), and so is the ELF header the loader maps in front of the game's data; so are the GS's memory, VU1
 * and the GS front end (the GS threads' side: the game never reads them back). TTT_SYNCTEST compares the raw states.
 *
 *   TTT_HASH_EVERY=<n>          print `ttt: hash <vblank> <hash>` at every n-th VBlank boundary
 *   TTT_SAVESTATE=<vblank>:<f>  save the state at that VBlank boundary to file f (and go on)
 *   TTT_LOADSTATE=<f>           load file f at the first poll and go on from there
 *   TTT_SYNCTEST=<n>            save, run n VBlanks, save, load, run the same n again, save, and compare
 *   TTT_SYNCTEST_FROM=<vblank>  ... from that VBlank on (default: from the first)
 *   TTT_SELFTEST=state          the hash's host-address rule and an in-process save / load
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#define memcpy __builtin_memcpy
#define XXH_INLINE_ALL
#define XXH_NO_STDLIB
#define XXH_FORCE_MEMORY_ACCESS 2 /* x86: unaligned loads are fine */
#include "../../third_party/xxhash/xxhash.h"
#include "plat_state.h"
#include "port_addr.h"
#include "port_host.h"
#include "gs.h"

uint32_t Port_VBlankCount(void); /* plat_hw.c */
int Port_Headless(void);

int gPortStateWant;
int gPortResim;
int gPortNoGs;
int gPortStateNoGs;

/* ---------------------------------------------------------------------------------------------- the regions */

enum { PS_EERAM = 0x100 };
#define MAX_REGIONS 96
#define MAX_HOOKS 16
static struct { const char *name; uint8_t *p; uint32_t size; int flags; } sReg[MAX_REGIONS];
static int sRegN;
static struct { const char *(*busy)(void); void (*before)(void); void (*after)(void); } sHook[MAX_HOOKS];
static int sHookN;

void Port_StateAdd(const char *name, void *p, uint32_t size, int flags) {
    if (sRegN == MAX_REGIONS) {
        fprintf(stderr, "ttt: state: too many regions (%s)\n", name);
        exit(2);
    }
    sReg[sRegN].name = name;
    sReg[sRegN].p = p;
    sReg[sRegN].size = size;
    sReg[sRegN++].flags = flags;
}

void Port_StateHooks(const char *(*busy)(void), void (*before)(void), void (*after_load)(void)) {
    if (sHookN == MAX_HOOKS) {
        fprintf(stderr, "ttt: state: too many hooks\n");
        exit(2);
    }
    sHook[sHookN].busy = busy;
    sHook[sHookN].before = before;
    sHook[sHookN++].after = after_load;
}

extern char __ttt_bss_end[]; /* image.ld: the end of the game's bss; the EE stack is above it */
#define STACK_LO (((uint32_t)(uintptr_t)__ttt_bss_end + 15u) & ~15u)
/* The loader maps the program's ELF header and program headers into the page in front of the game's data (the first
   LOAD segment starts at file offset 0): build-dependent bytes where the PS2 has the end of its code. */
extern char __ttt_data_start[];
#define ELFHDR_LO ((uint32_t)(uintptr_t)__ttt_data_start & ~0xFFFu)
#define ELFHDR_HI ((uint32_t)(uintptr_t)__ttt_data_start)

/* The PS2's memory, first of all (constructor 101, as the modules' registrations). The order of the regions is the
   order of the hash's terms, so this runs at its place among the constructors: the empty asm keeps clang from
   evaluating it at compile time (everything it calls is in this file), which put these regions first in its build and
   gave every run another hash (T42 b2, docs/port/README.md gotcha 32). */
__attribute__((constructor(101))) static void state_memory(void) {
    __asm__ volatile("");
    Port_StateAdd("ee.ram", Port_Ptr(0x20000000u), PORT_EE_RAM_SIZE, PS_HASH | PS_EERAM); /* the uncached view: all 32 MB */
    Port_StateAdd("ee.spr", Port_Ptr(PORT_SPR_ADDR), PORT_SPR_SIZE, PS_HASH);
    Port_StateAdd("ee.hw", Port_Ptr(0x10000000u), 0x10000u, PS_HASH);
    Port_StateAdd("vu0.mem", Port_Ptr(0x11000000u), 0x8000u, PS_HASH);
    Port_StateAdd("vu1.mem", Port_Ptr(0x11008000u), 0x8000u, PS_GSSIDE);
    Port_StateAdd("gs.priv", Port_Ptr(0x12000000u), 0x2000u, PS_HASH | PS_GSPRIV);
}

/* ---------------------------------------------------------------------------------------------- the program's symbols */

/* The host image (the program's loaded segments) and the game's data labels (to name a difference), from
   /proc/self/exe. */
static uint32_t sImgLo, sImgHi;
static uint32_t *sDataAddr, sDataN;
static const char **sDataName;
static uint8_t *sElf;
static int sSymsDone;

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }

/* Sorts the labels by address: Shell's sort (the host qsort is not in the program's PLT). */
static void sort_labels(void) {
    static const uint32_t gaps[] = { 13776, 4592, 1968, 861, 336, 112, 48, 21, 7, 3, 1 };
    uint32_t g, i, j;
    for (g = 0; g < sizeof gaps / sizeof gaps[0]; g++) {
        uint32_t gap = gaps[g];
        for (i = gap; i < sDataN; i++) {
            uint32_t ta = sDataAddr[i];
            const char *tc = sDataName[i];
            for (j = i; j >= gap && sDataAddr[j - gap] > ta; j -= gap) {
                sDataAddr[j] = sDataAddr[j - gap];
                sDataName[j] = sDataName[j - gap];
            }
            sDataAddr[j] = ta;
            sDataName[j] = tc;
        }
    }
}

static void syms_init(void) {
    FILE *f;
    long n;
    uint32_t phoff, shoff, k, cap = 0;
    uint16_t phnum, phentsize, shnum, shentsize;
    int e64;

    if (sSymsDone) {
        return;
    }
    sSymsDone = 1;
    f = fopen("/proc/self/exe", "rb");
    if (f == NULL) {
        return;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    sElf = malloc((size_t)n);
    if (sElf == NULL || fread(sElf, 1, (size_t)n, f) != (size_t)n || memcmp(sElf, "\177ELF", 4) != 0 ||
        sElf[5] != 1 || (sElf[4] != 1 && sElf[4] != 2)) {
        fclose(f);
        return;
    }
    fclose(f);
    /* ELF32, or ELF64 for the x86-64 build (TTT_CC=clang64): the same fields at other offsets and widths. Every
       address and offset used here is below 4 GB, so the low word of a 64-bit field is the value (little-endian). */
    e64 = sElf[4] == 2;
    phoff = rd32(sElf + (e64 ? 32 : 28));
    phentsize = rd16(sElf + (e64 ? 54 : 42));
    phnum = rd16(sElf + (e64 ? 56 : 44));
    shoff = rd32(sElf + (e64 ? 40 : 32));
    shentsize = rd16(sElf + (e64 ? 58 : 46));
    shnum = rd16(sElf + (e64 ? 60 : 48));
    sImgLo = 0xFFFFFFFFu;
    for (k = 0; k < phnum; k++) { /* PT_LOAD segments above the PS2's map: the host image */
        const uint8_t *ph = sElf + phoff + k * phentsize;
        uint32_t va = rd32(ph + (e64 ? 16 : 8)), memsz = rd32(ph + (e64 ? 40 : 20));
        if (rd32(ph) == 1 && va >= 0x02000000u && va < 0x40000000u) { /* (not the heap guard, link.py BRK_GUARD) */
            sImgLo = va < sImgLo ? va : sImgLo;
            sImgHi = va + memsz > sImgHi ? va + memsz : sImgHi;
        }
    }
    for (k = 0; k < shnum; k++) {
        const uint8_t *sh = sElf + shoff + k * shentsize;
        if (rd32(sh + 4) == 2) { /* SHT_SYMTAB */
            uint32_t link = rd32(sh + (e64 ? 40 : 24)), symsize = e64 ? 24 : 16;
            const char *str = (const char *)sElf + rd32(sElf + shoff + link * shentsize + (e64 ? 24 : 16));
            uint32_t off = rd32(sh + (e64 ? 24 : 16)), size = rd32(sh + (e64 ? 32 : 20)), j;
            for (j = 0; j + symsize <= size; j += symsize) {
                const uint8_t *sym = sElf + off + j;
                uint32_t v = rd32(sym + (e64 ? 8 : 4)), type = sym[e64 ? 4 : 12] & 15;
                const char *name = str + rd32(sym);
                if ((type == 0 || type == 1) && v >= 0x10000u && v < PORT_EE_RAM_SIZE && name[0] != 0 && name[0] != '.' &&
                    name[0] != '$') {
                    if (sDataN == cap) {
                        cap = cap ? cap * 2 : 8192;
                        sDataAddr = realloc(sDataAddr, cap * 4);
                        sDataName = realloc(sDataName, cap * sizeof *sDataName);
                    }
                    sDataAddr[sDataN] = v;
                    sDataName[sDataN++] = name;
                }
            }
        }
    }
    sort_labels();
}

/* A word as the hash sees it (see the top). The window is fixed, not the program's own range, so that a word's class
   does not depend on the size of the program. */
#define HOST_LO 0x08000000u
#define HOST_HI 0x09000000u
#define HOST_ID 0xC0DEC0DEu
static inline uint32_t norm_word(uint32_t v) { return v - HOST_LO < HOST_HI - HOST_LO ? HOST_ID : v; }

/* The name of the game's data label at or below EE address a (NULL: none). */
static const char *data_name(uint32_t a, uint32_t *base) {
    uint32_t lo = 0, hi = sDataN;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (sDataAddr[mid] <= a) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo == 0) {
        return NULL;
    }
    *base = sDataAddr[lo - 1];
    return sDataName[lo - 1];
}

/* ---------------------------------------------------------------------------------------------- the hash */

/* The GS privileged registers as the hash and the synctest see them (PS_GSPRIV): `chunk` holds bytes [base, base+n). */
static void gspriv_mask(uint8_t *chunk, uint32_t base, uint32_t n) {
    if (base <= 0x1000u && base + n >= 0x1008u) {
        uint32_t *csr = (uint32_t *)(chunk + 0x1000u - base);
        csr[0] &= 0x2008u; /* FIELD, VSINT */
        csr[1] = 0;
    }
    if (base <= 0x1080u && base + n >= 0x1088u) {
        memset(chunk + 0x1080u - base, 0, 8); /* SIGLBLID */
    }
}

static uint32_t sChunk[4096];

static void hash_bytes(XXH3_state_t *st, const uint8_t *p, uint32_t n, int flags) {
    uint32_t base = 0;
    while (n != 0) {
        uint32_t k = n > sizeof sChunk ? (uint32_t)sizeof sChunk : n, i, w = k / 4, any = 0;
        const uint32_t *src = (const uint32_t *)p;
        for (i = 0; i < w; i++) { /* most chunks hold no host address: hashed in place */
            any |= src[i] - HOST_LO < HOST_HI - HOST_LO;
        }
        if (any || (flags & PS_GSPRIV)) {
            memcpy(sChunk, p, k);
            for (i = 0; i < w; i++) {
                sChunk[i] = norm_word(sChunk[i]);
            }
            if (flags & PS_GSPRIV) {
                gspriv_mask((uint8_t *)sChunk, base, k);
            }
            XXH3_64bits_update(st, sChunk, k);
        } else {
            XXH3_64bits_update(st, p, k);
        }
        p += k;
        n -= k;
        base += k;
    }
}

static XXH3_state_t sXxh;
static int sTold;

static uint64_t region_hash(int r) {
    XXH3_64bits_reset(&sXxh);
    if (sReg[r].flags & PS_EERAM) {
        /* the program's ELF header and the EE stack (above the game's bss) left out */
        hash_bytes(&sXxh, sReg[r].p, ELFHDR_LO, sReg[r].flags);
        hash_bytes(&sXxh, sReg[r].p + ELFHDR_HI, STACK_LO - ELFHDR_HI, sReg[r].flags);
    } else {
        hash_bytes(&sXxh, sReg[r].p, sReg[r].size, sReg[r].flags);
    }
    return XXH3_64bits_digest(&sXxh);
}

uint64_t Port_StateHash(void) {
    uint64_t h[MAX_REGIONS];
    int r, n = 0;
    syms_init();
    if (sImgHi != 0 && (sImgLo < HOST_LO || sImgHi > HOST_HI) && !sTold) { /* the window must hold the program image */
        sTold = 1;
        fprintf(stderr, "ttt: state: the program image 0x%08X-0x%08X is not inside 0x%08X-0x%08X: the hash is not "
                "stable across builds\n", (unsigned)sImgLo, (unsigned)sImgHi, HOST_LO, HOST_HI);
    }
    for (r = 0; r < sRegN; r++) {
        if (sReg[r].flags & PS_HASH) {
            h[n++] = region_hash(r);
        }
    }
    return XXH3_64bits(h, (size_t)n * sizeof h[0]);
}

/* TTT_HASH_REGIONS=1: each hashed region's own hash after the total (to find where two runs part). */
static void hash_regions_print(void) {
    int r;
    for (r = 0; r < sRegN; r++) {
        if (sReg[r].flags & PS_HASH) {
            printf("ttt: hash-region %s %016llx\n", sReg[r].name, (unsigned long long)region_hash(r));
        }
    }
}

/* ---------------------------------------------------------------------------------------------- the stack switches */

/* Port_StateBoundary (plat_hw.c, on the EE stack): pushes the callee-saved registers, keeps the stack pointer in
   gPortCtxEsp and runs port_boundary_body on the service stack, then returns through whatever gPortCtxEsp is by then: the
   same frame, or after a load the saved one (the EE stack is part of EE RAM, so a saved frame comes back with the
   memory). */
#if defined(__x86_64__)
uintptr_t gPortCtxEsp; /* (x86-64: the self-tests also cross a boundary on the host stack, above 4 GB; a state holds
                          the low half, taken on the EE stack) */
#else
uint32_t gPortCtxEsp;
#endif
#define SERVICE_STACK 262144
uint8_t gPortCtxStack[SERVICE_STACK] __attribute__((aligned(16)));
void port_boundary_body(void);
#if defined(__x86_64__)
/* x86-64 (TTT_CC=clang64): rbp, rbx, r12-r15 */
__asm__(".text\n"
        ".globl Port_StateBoundary\n"
        ".type Port_StateBoundary, @function\n"
        "Port_StateBoundary:\n"
        "\tpushq %rbp\n"
        "\tpushq %rbx\n"
        "\tpushq %r12\n"
        "\tpushq %r13\n"
        "\tpushq %r14\n"
        "\tpushq %r15\n"
        "\tmovq %rsp, gPortCtxEsp(%rip)\n"
        "\tleaq gPortCtxStack+262144(%rip), %rsp\n"
        "\tcall port_boundary_body\n"
        "\tmovq gPortCtxEsp(%rip), %rsp\n"
        "\tpopq %r15\n"
        "\tpopq %r14\n"
        "\tpopq %r13\n"
        "\tpopq %r12\n"
        "\tpopq %rbx\n"
        "\tpopq %rbp\n"
        "\tret\n"
        ".size Port_StateBoundary, .-Port_StateBoundary\n");
#else
__asm__(".text\n"
        ".globl Port_StateBoundary\n"
        ".type Port_StateBoundary, @function\n"
        "Port_StateBoundary:\n"
        "\tpushl %ebp\n"
        "\tpushl %ebx\n"
        "\tpushl %esi\n"
        "\tpushl %edi\n"
        "\tmovl %esp, gPortCtxEsp\n"
        "\tleal gPortCtxStack+262144, %esp\n"
        "\tcall port_boundary_body\n"
        "\tmovl gPortCtxEsp, %esp\n"
        "\tpopl %edi\n"
        "\tpopl %esi\n"
        "\tpopl %ebx\n"
        "\tpopl %ebp\n"
        "\tret\n"
        ".size Port_StateBoundary, .-Port_StateBoundary\n");
#endif

/* Port_OffEe: fn(arg) on a stack of its own when called on the EE stack (the game's thread), else where it is. The
   EE stack pointer it left is kept in gPortOffEeEsp (the crash report's view of the game's frames, plat_crash.c). */
#define OFF_STACK 262144
uint8_t gPortOffStack[OFF_STACK] __attribute__((aligned(16)));
uint32_t gPortOffEeEsp;
#if defined(__x86_64__)
__asm__(".text\n"
        ".globl Port_OffEe\n"
        ".type Port_OffEe, @function\n"
        "Port_OffEe:\n"
        "\tpushq %rbp\n"
        "\tmovq %rsp, %rbp\n"
        "\tcmpq $0x02000000, %rsp\n"
        "\tjae 1f\n"
        "\tmovl %esp, gPortOffEeEsp(%rip)\n"
        "\tleaq gPortOffStack+262144(%rip), %rsp\n"
        "1:\n"
        "\tandq $-16, %rsp\n"
        "\tmovq %rdi, %rax\n"
        "\tmovq %rsi, %rdi\n"
        "\tcall *%rax\n"
        "\tmovq %rbp, %rsp\n"
        "\tpopq %rbp\n"
        "\tret\n"
        ".size Port_OffEe, .-Port_OffEe\n");
#else
__asm__(".text\n"
        ".globl Port_OffEe\n"
        ".type Port_OffEe, @function\n"
        "Port_OffEe:\n"
        "\tpushl %ebp\n"
        "\tmovl %esp, %ebp\n"
        "\tmovl 8(%ebp), %eax\n"
        "\tmovl 12(%ebp), %edx\n"
        "\tcmpl $0x02000000, %esp\n"
        "\tjae 1f\n"
        "\tmovl %esp, gPortOffEeEsp\n"
        "\tleal gPortOffStack+262144, %esp\n"
        "1:\n"
        "\tandl $-16, %esp\n"
        "\tsubl $12, %esp\n"
        "\tpushl %edx\n"
        "\tcall *%eax\n"
        "\tmovl %ebp, %esp\n"
        "\tpopl %ebp\n"
        "\tret\n"
        ".size Port_OffEe, .-Port_OffEe\n");
#endif

/* ---------------------------------------------------------------------------------------------- save and load */

typedef struct {
    char magic[8];  /* "TTTSTATE" */
    uint32_t version, size;
    uint64_t build; /* the program's code and the region list */
    uint32_t esp, vblank;
    uint64_t token; /* page tracking (F07): the process and tracking session that saved it (0: none, as in files) */
    uint32_t epoch; /* ... and the tracking epoch the save was taken at */
    uint32_t pad;
} StateHeader;      /* 48 bytes: the regions start 16-aligned */
#define STATE_VERSION 1

static uint64_t sBuild;
static const char *sBusyWhy;
static int sInBoundary;

size_t Port_StateSize(void) {
    size_t n = sizeof(StateHeader);
    int r;
    for (r = 0; r < sRegN; r++) {
        n += sReg[r].flags & PS_HASHONLY ? 0 : (sReg[r].size + 15u) & ~15u;
    }
    return n;
}

static uint64_t build_id(void) {
    if (sBuild == 0) {
        extern char __executable_start[], etext[];
        int r;
        XXH3_64bits_reset(&sXxh);
        XXH3_64bits_update(&sXxh, __executable_start, (size_t)(etext - __executable_start));
        for (r = 0; r < sRegN; r++) {
            XXH3_64bits_update(&sXxh, sReg[r].name, strlen(sReg[r].name));
            XXH3_64bits_update(&sXxh, &sReg[r].size, 4);
        }
        sBuild = XXH3_64bits_digest(&sXxh);
    }
    return sBuild;
}

static uint64_t ns_now(void);
static uint64_t sPrepNs; /* the last save's or load's preparation (the GS drain, mostly) */
static int prepare(void) {
    int r;
    uint64_t t0 = ns_now();
    if (!sInBoundary) {
        sBusyWhy = "not at a VBlank boundary";
        return -1;
    }
    build_id();
    for (r = 0; r < sHookN; r++) {
        if (sHook[r].busy != NULL && (sBusyWhy = sHook[r].busy()) != NULL) {
            return -1;
        }
    }
    for (r = 0; r < sHookN; r++) {
        if (sHook[r].before != NULL) {
            sHook[r].before();
        }
    }
    sPrepNs = ns_now() - t0;
    return 0;
}

/* ---------------------------------------------------------------------------------------------- page tracking (F07) */

/* A save copies 40 MB, most of it EE RAM, GS memory and sound RAM, of which a VBlank writes a small part. With tracking
   on, the kernel write-protects the whole pages of every region of TRK_MIN bytes or more (EE RAM through all three of
   its views): userfaultfd in asynchronous write-protect mode, where a write to a protected page is let through by the
   kernel itself (no signal, nothing on any stack, writes by the kernel and by other threads included), and
   PAGEMAP_SCAN, which lists the pages written since the last scan and protects them again in one call. Each scan is
   an epoch; every tracked page keeps the epoch it was last seen written in. A buffer saved by this tracking session
   carries its epoch: compared with the memory now, only pages written in a later epoch can differ (a load's own
   copies are writes too), so a save into it copies those and a load from it copies those back. The parts of regions
   outside whole pages and the small regions are copied every time. */
#ifndef UFFD_USER_MODE_ONLY
#define UFFD_USER_MODE_ONLY 1
#endif
#define TRK_FEATURES ((1ull << 12) | (1ull << 13) | (1ull << 15)) /* UFFD_FEATURE_WP_HUGETLBFS_SHMEM, _UNPOPULATED, _ASYNC */
#define TRK_UFFDIO_API 0xC018AA3Fu      /* _IOWR(0xAA, 0x3F, struct uffdio_api) */
#define TRK_UFFDIO_REGISTER 0xC020AA00u /* _IOWR(0xAA, 0x00, struct uffdio_register) */
#define TRK_PAGEMAP_SCAN 0xC0606610u    /* _IOWR('f', 16, struct pm_scan_arg) */
#define TRK_PAGE 4096u
#define TRK_MIN 65536u
typedef struct { uint64_t api, features, ioctls; } TrkApi;
typedef struct { uint64_t start, len, mode, ioctls; } TrkReg;
typedef struct {
    uint64_t size, flags, start, end, walk_end, vec, vec_len, max_pages, category_inverted, category_mask,
        category_anyof_mask, return_mask;
} TrkScan;
typedef struct { uint64_t start, end, categories; } TrkRange;
/* through the GOT (port_host.h: gcc's noplt, the GOT thunks of clang's 32-bit variant) */
PORT_NOPLT(syscall);
PORT_NOPLT(ioctl);
PORT_NOPLT(open);
PORT_NOPLT(close);
PORT_NOPLT(madvise);

static int sTrack;                    /* 1 on, 0 off, -1 not offered by the kernel */
static int sUffd = -1, sPagemap = -1;
static uint32_t sTrkOff[MAX_REGIONS], sTrkEnd[MAX_REGIONS]; /* a region's whole pages, bytes [off, end) (end 0: none) */
static uint32_t sTrkBase[MAX_REGIONS]; /* index of its first page in sPageEp */
static uint32_t *sPageEp, sTrkPages;  /* per tracked page: the epoch it was last seen written in */
static uint32_t sEpoch;
static uint64_t sToken;
static TrkRange sVec[256];
static uint64_t sTrkScans, sTrkScanNs, sTrkWritten, sTrkPagesSaved, sTrkPagesLoaded, sTrkIncSaves, sTrkIncLoads;
static uint64_t sAllSaves, sAllLoads;
static const char *sTrkWhy = "off";
static uint64_t ns_now(void);

static int trk_register(uintptr_t a, uint32_t n) {
    TrkReg r;
    memset(&r, 0, sizeof r);
    r.start = a;
    r.len = n;
    r.mode = 2; /* UFFDIO_REGISTER_MODE_WP */
    return ioctl(sUffd, TRK_UFFDIO_REGISTER, &r);
}

/* The written pages of view `base` (region r's byte i at base + i) in [off, end): their epoch is now, and they are
   protected again. */
static int trk_scan_view(int r, uintptr_t base, uint32_t off, uint32_t end) {
    uint64_t at = base + off, stop = base + end;
    while (at < stop) {
        TrkScan a;
        int n, i;
        memset(&a, 0, sizeof a);
        a.size = sizeof a;
        a.flags = 1 | 2; /* PM_SCAN_WP_MATCHING | PM_SCAN_CHECK_WPASYNC */
        a.start = at;
        a.end = stop;
        a.vec = (uintptr_t)sVec;
        a.vec_len = sizeof sVec / sizeof sVec[0];
        a.category_mask = a.return_mask = 2; /* PAGE_IS_WRITTEN */
        n = ioctl(sPagemap, TRK_PAGEMAP_SCAN, &a);
        if (n < 0) {
            return -1;
        }
        for (i = 0; i < n; i++) {
            uint32_t p = (uint32_t)((sVec[i].start - base - sTrkOff[r]) / TRK_PAGE);
            uint32_t q = (uint32_t)((sVec[i].end - base - sTrkOff[r]) / TRK_PAGE);
            sTrkWritten += q - p;
            for (; p < q; p++) {
                sPageEp[sTrkBase[r] + p] = sEpoch;
            }
        }
        if (a.walk_end <= at) {
            return -1;
        }
        at = a.walk_end;
    }
    return 0;
}

static void trk_off(const char *why) {
    if (sUffd >= 0) {
        close(sUffd); /* (unregisters every range: the pages are writable again) */
    }
    if (sPagemap >= 0) {
        close(sPagemap);
    }
    sUffd = sPagemap = -1;
    sTrack = why != NULL ? -1 : 0;
    sTrkWhy = why != NULL ? why : "off";
    sToken = 0; /* the buffers saved so far are whole copies like any other */
}

/* A new epoch: the pages written since the last one. */
static void trk_scan(void) {
    uint64_t t0 = ns_now();
    int r;
    sEpoch++;
    for (r = 0; r < sRegN && sTrack > 0; r++) {
        if (sTrkEnd[r] == 0) {
            continue;
        }
        if (trk_scan_view(r, (uintptr_t)sReg[r].p, sTrkOff[r], sTrkEnd[r]) != 0 ||
            ((sReg[r].flags & PS_EERAM) && (trk_scan_view(r, 0x00000000u, 0x10000u, sTrkEnd[r]) != 0 ||
                                           trk_scan_view(r, 0x30000000u, sTrkOff[r], sTrkEnd[r]) != 0))) {
            trk_off("PAGEMAP_SCAN failed");
        }
    }
    sTrkScans++;
    sTrkScanNs += ns_now() - t0;
}

static int trk_on(void) {
    TrkApi api;
    uint32_t pages = 0;
    int r;
    if (sTrack != 0) {
        return sTrack > 0;
    }
    sUffd = (int)syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
    if (sUffd < 0) {
        trk_off("no userfaultfd");
        return 0;
    }
    memset(&api, 0, sizeof api);
    api.api = 0xAA; /* UFFD_API */
    api.features = TRK_FEATURES;
    sPagemap = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    if (ioctl(sUffd, TRK_UFFDIO_API, &api) != 0 || (api.features & TRK_FEATURES) != TRK_FEATURES || sPagemap < 0) {
        trk_off("the kernel has no asynchronous write protection (Linux 6.7)");
        return 0;
    }
    for (r = 0; r < sRegN; r++) {
        uintptr_t a = (uintptr_t)sReg[r].p, lo = (a + TRK_PAGE - 1) & ~(uintptr_t)(TRK_PAGE - 1);
        uintptr_t hi = (a + sReg[r].size) & ~(uintptr_t)(TRK_PAGE - 1);
        sTrkEnd[r] = 0;
        if ((sReg[r].flags & PS_HASHONLY) || sReg[r].size < TRK_MIN || hi <= lo) {
            continue;
        }
        sTrkOff[r] = (uint32_t)(lo - a);
        sTrkEnd[r] = (uint32_t)(hi - a);
        sTrkBase[r] = pages;
        pages += (uint32_t)(hi - lo) / TRK_PAGE;
        if (!(sReg[r].flags & PS_EERAM)) {
            madvise((void *)lo, hi - lo, MADV_NOHUGEPAGE); /* (pages of the program's bss: 4 KB, not 2 MB, at a time) */
        }
        if (trk_register(lo, (uint32_t)(hi - lo)) != 0 ||
            ((sReg[r].flags & PS_EERAM) && (trk_register(0x00010000u, (uint32_t)(hi - lo) - 0x10000u) != 0 ||
                                           trk_register(0x30000000u, (uint32_t)(hi - lo)) != 0))) {
            trk_off("userfaultfd refused a region");
            return 0;
        }
    }
    if (pages != sTrkPages) {
        free(sPageEp);
        sPageEp = malloc(pages * sizeof *sPageEp);
        sTrkPages = pages;
    }
    if (sPageEp == NULL) {
        trk_off("no memory");
        return 0;
    }
    sTrack = 1;
    sTrkWhy = "on";
    sToken = (ns_now() << 16) ^ (uint64_t)syscall(SYS_getpid) ^ 0x7474740000000000ull;
    sToken += sToken == 0;
    trk_scan(); /* every page counts as written once: no buffer holds anything of this session yet */
    return sTrack > 0;
}

int Port_StateTrack(int on) {
    if (!on) {
        if (sTrack > 0) {
            trk_off(NULL);
        }
        return 0;
    }
    return trk_on();
}

/* Copies the pages of region r written after epoch `since` (and the bytes outside its whole pages) from src to dst;
   how many pages. */
static uint32_t trk_copy(int r, uint8_t *dst, const uint8_t *src, uint32_t since) {
    const uint32_t *ep = sPageEp + sTrkBase[r];
    uint32_t off = sTrkOff[r], n = (sTrkEnd[r] - off) / TRK_PAGE, k = 0, copied = 0;
    memcpy(dst, src, off);
    memcpy(dst + sTrkEnd[r], src + sTrkEnd[r], sReg[r].size - sTrkEnd[r]);
    while (k < n) {
        uint32_t j;
        if (ep[k] <= since) {
            k++;
            continue;
        }
        for (j = k; j < n && ep[j] > since; j++) {
        }
        memcpy(dst + off + k * TRK_PAGE, src + off + k * TRK_PAGE, (j - k) * TRK_PAGE);
        copied += j - k;
        k = j;
    }
    return copied;
}

/* Whether buffer h holds a save of this tracking session (its pages that were not written since are the memory's). */
static int trk_valid(const StateHeader *h) {
    return sTrack > 0 && h->token == sToken && h->epoch != 0 && h->epoch <= sEpoch &&
           memcmp(h->magic, "TTTSTATE", 8) == 0 && h->build == sBuild && h->size == Port_StateSize();
}

/* TTT_STATE_TRACK_CHECK=1: after each save into or load from a tracked buffer, the whole of every region compared with
   the buffer (what a whole copy would have given): the pages a difference is in were written and not seen. */
static int sTrkCheck;
static uint64_t sTrkChecks, sTrkCheckBad;
static void trk_check(const uint8_t *buf, const char *what) {
    const uint8_t *d = buf + sizeof(StateHeader);
    int r, bad = 0;
    for (r = 0; r < sRegN; r++) {
        if (sReg[r].flags & PS_HASHONLY) {
            continue;
        }
        if ((sReg[r].flags & PS_GSSIDE) && (gPortStateNoGs || (((const StateHeader *)buf)->pad & 1))) {
            d += (sReg[r].size + 15u) & ~15u; /* (not saved or loaded; the GS threads may be writing it) */
            continue;
        }
        if (memcmp(sReg[r].p, d, sReg[r].size) != 0) {
            uint32_t i = 0;
            while (sReg[r].p[i] == d[i]) {
                i++;
            }
            if (sTrkCheckBad == 0) {
                printf("ttt: state-track: %s at vblank %u: %s +0x%X differs from a whole copy\n", what,
                       (unsigned)Port_VBlankCount(), sReg[r].name, (unsigned)i);
            }
            bad = 1;
        }
        d += (sReg[r].size + 15u) & ~15u;
    }
    sTrkChecks++;
    sTrkCheckBad += bad;
}

static int sLoading;
int Port_StateLoading(void) { return sLoading; }

int Port_StateSave(void *buf) {
    StateHeader *h = buf;
    uint8_t *d = (uint8_t *)buf + sizeof *h;
    uint32_t since = 0;
    int r, inc, hadGs, noGs = gPortStateNoGs;
    if (prepare() != 0) {
        return -1;
    }
    if (sTrack > 0) {
        trk_scan();
    }
    inc = trk_valid(h);
    since = h->epoch;
    hadGs = !(h->pad & 1);
    memset(h, 0, sizeof *h);
    memcpy(h->magic, "TTTSTATE", 8);
    h->version = STATE_VERSION;
    h->size = (uint32_t)Port_StateSize();
    h->build = sBuild;
    h->esp = gPortCtxEsp;
    h->vblank = Port_VBlankCount();
    h->pad = (uint32_t)noGs; /* bit 0: the GS's side is not in it */
    for (r = 0; r < sRegN; r++) {
        if (!(sReg[r].flags & PS_HASHONLY)) {
            int gs = (sReg[r].flags & PS_GSSIDE) != 0;
            if (noGs && gs) {
                /* left out */
            } else if (inc && sTrkEnd[r] != 0 && (hadGs || !gs)) {
                sTrkPagesSaved += trk_copy(r, d, sReg[r].p, since);
            } else {
                memcpy(d, sReg[r].p, sReg[r].size);
            }
            d += (sReg[r].size + 15u) & ~15u;
        }
    }
    sTrkIncSaves += inc;
    sAllSaves++;
    if (inc && sTrkCheck) {
        trk_check(buf, "save");
    }
    if (sTrack > 0) {
        h->token = sToken;
        h->epoch = sEpoch;
    }
    return 0;
}

int Port_StateLoad(const void *buf) {
    const StateHeader *h = buf;
    const uint8_t *d = (const uint8_t *)buf + sizeof *h;
    int r, inc, noGs = gPortStateNoGs;
    if (memcmp(h->magic, "TTTSTATE", 8) != 0 || h->version != STATE_VERSION || h->size != Port_StateSize()) {
        sBusyWhy = "not a state of this version";
        return -1;
    }
    gPortStateNoGs = noGs | (int)(h->pad & 1); /* (a state without the GS's side leaves that side as it is) */
    sLoading = 1;
    r = prepare();
    sLoading = 0;
    if (r != 0 || h->build != sBuild) {
        sBusyWhy = r != 0 ? sBusyWhy : "saved by another build of the program";
        gPortStateNoGs = noGs;
        return -1;
    }
    if (sTrack > 0) {
        trk_scan(); /* (the pages written since the last save or load) */
    }
    inc = trk_valid(h);
    for (r = 0; r < sRegN; r++) {
        if (!(sReg[r].flags & PS_HASHONLY)) {
            if (gPortStateNoGs && (sReg[r].flags & PS_GSSIDE)) {
                /* left as it is */
            } else if (inc && sTrkEnd[r] != 0) {
                sTrkPagesLoaded += trk_copy(r, sReg[r].p, d, h->epoch);
            } else {
                memcpy(sReg[r].p, d, sReg[r].size);
            }
            d += (sReg[r].size + 15u) & ~15u;
        }
    }
    sTrkIncLoads += inc;
    sAllLoads++;
    if (inc && sTrkCheck) {
        trk_check(buf, "load");
    }
    for (r = 0; r < sHookN; r++) {
        if (sHook[r].after != NULL) {
            sHook[r].after();
        }
    }
    gPortStateNoGs = noGs;
    gPortCtxEsp = h->esp; /* the boundary returns into the saved one */
    return 0;
}

/* ---------------------------------------------------------------------------------------------- the switches */

static uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int sInit, sOn;
static uint32_t sHashEvery, sSaveAt, sSyncN, sSyncStart;
static const char *sSaveFile, *sLoadFile;
static uint8_t *sSlot, *sBufA, *sBufB;
static int sPhase; /* synctest: 0 none, 1 first pass, 2 second pass */
static uint32_t sSyncFrom, sSyncTo, sChecks, sDiverged, sSkipped;
static uint64_t sSaveNs, sLoadNs, sDrainNs, sSaves, sLoads;
static void (*sTestHook)(void); /* TTT_SELFTEST=state */
/* TTT_ROLLBACK=<n>: the rollback test (below) */
#define RB_MAX 16
static uint32_t sRbN, sRbFrom, sRbTarget, sRbChecks, sRbDiverged, sRbSkipped, sRbOver, sRbFrames, sRbRolls;
static uint8_t *sRbSlot[RB_MAX + 1], *sRbCheck;
static int sRbNoDraw;
static uint64_t sRbStart, sRbLast, sRbSum, sRbMax, sRbWorkSum, sRbWorkMax, sRbCpu0, sRbCpuSum;
static uint64_t cpu_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint8_t *must_alloc(size_t n) {
    uint8_t *p = malloc(n);
    if (p == NULL) {
        fprintf(stderr, "ttt: state: no memory for %u bytes\n", (unsigned)n);
        exit(2);
    }
    return p;
}

static void init(void) {
    const char *e;
    sInit = 1;
    if ((e = getenv("TTT_HASH_EVERY")) != NULL && *e) {
        sHashEvery = (uint32_t)strtoul(e, NULL, 0);
    }
    if ((e = getenv("TTT_SAVESTATE")) != NULL && *e) {
        char *end;
        sSaveAt = (uint32_t)strtoul(e, &end, 0);
        sSaveFile = *end == ':' ? end + 1 : NULL;
        if (sSaveFile == NULL || *sSaveFile == 0) {
            fprintf(stderr, "ttt: TTT_SAVESTATE=<vblank>:<file>\n");
            exit(2);
        }
    }
    if ((e = getenv("TTT_LOADSTATE")) != NULL && *e) {
        sLoadFile = e;
    }
    if ((e = getenv("TTT_SYNCTEST")) != NULL && *e) {
        sSyncN = (uint32_t)strtoul(e, NULL, 0);
        if ((e = getenv("TTT_SYNCTEST_FROM")) != NULL && *e) {
            sSyncStart = (uint32_t)strtoul(e, NULL, 0);
        }
        if (sSyncN != 0) {
            size_t n = Port_StateSize();
            sSlot = must_alloc(n);
            sBufA = must_alloc(n);
            sBufB = must_alloc(n);
        }
    }
    if ((e = getenv("TTT_ROLLBACK")) != NULL && *e && sSyncN == 0) {
        uint32_t k;
        sRbN = (uint32_t)strtoul(e, NULL, 0);
        if ((e = getenv("TTT_ROLLBACK_FROM")) != NULL && *e) {
            sRbFrom = (uint32_t)strtoul(e, NULL, 0);
        }
        if (sRbN > RB_MAX) {
            sRbN = RB_MAX;
        }
        for (k = 0; sRbN != 0 && k <= sRbN; k++) {
            sRbSlot[k] = must_alloc(Port_StateSize());
            memset(sRbSlot[k], 0, sizeof(StateHeader));
        }
        e = getenv("TTT_ROLLBACK_DRAW");
        sRbNoDraw = e != NULL && strcmp(e, "0") == 0;
        gPortStateNoGs = sRbNoDraw; /* undrawn frames: the GS's side is neither saved nor loaded (it never goes back) */
        if (sRbN != 0 && getenv("TTT_ROLLBACK_CHECK") != NULL) {
            sRbCheck = must_alloc(Port_StateSize());
            memset(sRbCheck, 0, sizeof(StateHeader));
        }
    }
    sOn = sHashEvery || sSaveAt || sLoadFile || sSyncN || sRbN;
    /* page tracking: on for the synctest and the rollback test unless TTT_STATE_TRACK=0, on for anything with 1 */
    e = getenv("TTT_STATE_TRACK");
    sTrkCheck = getenv("TTT_STATE_TRACK_CHECK") != NULL;
    if (e != NULL && *e ? strcmp(e, "0") != 0 : sSyncN != 0 || sRbN != 0) {
        Port_StateTrack(1);
    }
}

/* TTT_LOADSTATE asks for the first poll (plat_hw.c checks gPortStateWant). */
__attribute__((constructor(103))) static void state_want(void) {
    const char *e = getenv("TTT_LOADSTATE");
    gPortStateWant = e != NULL && *e != 0 && getenv("TTT_SELFTEST") == NULL;
}

static void load_file(void) {
    FILE *f = fopen(sLoadFile, "rb");
    size_t n = Port_StateSize();
    uint8_t *buf = must_alloc(n);
    uint64_t t0;
    if (f == NULL || fread(buf, 1, n, f) != n) {
        fprintf(stderr, "ttt: TTT_LOADSTATE: cannot read a state from %s\n", sLoadFile);
        exit(2);
    }
    fclose(f);
    t0 = ns_now();
    if (Port_StateLoad(buf) != 0) {
        fprintf(stderr, "ttt: TTT_LOADSTATE: %s: %s\n", sLoadFile, sBusyWhy);
        exit(2);
    }
    printf("ttt: loadstate: vblank %u from %s (%u us)\n", (unsigned)Port_VBlankCount(), sLoadFile,
           (unsigned)((ns_now() - t0) / 1000));
    free(buf);
}

static void save_file(uint32_t vb) {
    size_t n = Port_StateSize();
    uint8_t *buf = must_alloc(n);
    uint64_t t0, dt;
    FILE *f;
    memset(buf, 0, n); /* (the pages first, so that the time is the copy's) */
    t0 = ns_now();
    if (Port_StateSave(buf) != 0) {
        printf("ttt: savestate: cannot save at vblank %u: %s\n", (unsigned)vb, sBusyWhy);
        free(buf);
        return;
    }
    dt = ns_now() - t0;
    ((StateHeader *)buf)->token = 0; /* (a file is a whole copy for whoever loads it) */
    ((StateHeader *)buf)->epoch = 0;
    f = fopen(sSaveFile, "wb");
    if (f == NULL || fwrite(buf, 1, n, f) != n || fclose(f) != 0) {
        fprintf(stderr, "ttt: TTT_SAVESTATE: cannot write %s\n", sSaveFile);
        exit(2);
    }
    printf("ttt: savestate: vblank %u to %s (%u bytes; %u us, of which %u us waiting for the GS threads)\n",
           (unsigned)vb, sSaveFile, (unsigned)n, (unsigned)(dt / 1000), (unsigned)(sPrepNs / 1000));
    free(buf);
}

/* ---------------------------------------------------------------------------------------------- the synctest */

/* Two states of the same VBlank, region by region: the first difference of each region that differs (the GS's
   FINISH / SIGNAL / LABEL bits do not count; nor does the GS's side when a state lacks it). */
static int compare(const uint8_t *a, const uint8_t *b, uint32_t vb) {
    uint32_t off = sizeof(StateHeader), shown = 0;
    int r, bad = 0, noGs = (((const StateHeader *)a)->pad | ((const StateHeader *)b)->pad) & 1;
    for (r = 0; r < sRegN; r++) {
        const uint8_t *pa = a + off, *pb = b + off;
        uint32_t n = sReg[r].size, i, first = 0xFFFFFFFFu, count = 0;
        if (sReg[r].flags & PS_HASHONLY) {
            continue;
        }
        off += (n + 15u) & ~15u;
        if ((noGs && (sReg[r].flags & PS_GSSIDE)) || memcmp(pa, pb, n) == 0) {
            continue;
        }
        for (i = 0; i + 4 <= n; i += 4) {
            uint32_t wa, wb;
            memcpy(&wa, pa + i, 4);
            memcpy(&wb, pb + i, 4);
            if (sReg[r].flags & PS_GSPRIV) {
                if (i == 0x1000u) {
                    wa &= 0x2008u;
                    wb &= 0x2008u;
                } else if (i == 0x1004u || i == 0x1080u || i == 0x1084u) {
                    continue;
                }
            }
            if (wa != wb) {
                first = first == 0xFFFFFFFFu ? i : first;
                count++;
            }
        }
        if (count == 0) {
            continue;
        }
        bad = 1;
        if (sDiverged == 0 && shown++ < 12) {
            uint32_t wa, wb, base = 0;
            const char *name = NULL;
            memcpy(&wa, pa + first, 4);
            memcpy(&wb, pb + first, 4);
            if (sReg[r].flags & PS_EERAM) {
                syms_init();
                name = data_name(first, &base);
            }
            printf("ttt: synctest: vblank %u: %s +0x%X", (unsigned)vb, sReg[r].name, (unsigned)first);
            if (name != NULL) {
                printf(" (%s+0x%X)", name, (unsigned)(first - base));
            }
            printf(": %08X then %08X, %u words differ\n", (unsigned)wa, (unsigned)wb, (unsigned)count);
        }
    }
    return bad;
}

/* A save or a load of the synctest or the rollback test: its time, apart from the wait for the GS threads. */
static int timed_save(uint8_t *buf) {
    uint64_t t0 = ns_now();
    if (Port_StateSave(buf) != 0) {
        return -1;
    }
    sSaveNs += ns_now() - t0 - sPrepNs;
    sDrainNs += sPrepNs;
    sSaves++;
    return 0;
}

static void timed_load(const uint8_t *buf, const char *what) {
    uint64_t t0 = ns_now();
    if (Port_StateLoad(buf) != 0) {
        fprintf(stderr, "ttt: %s: cannot load: %s\n", what, sBusyWhy);
        exit(2);
    }
    sLoadNs += ns_now() - t0 - sPrepNs;
    sDrainNs += sPrepNs;
    sLoads++;
}

/* At each boundary: a checkpoint (save), n VBlanks on (first pass), a save of where they led, the checkpoint loaded,
   the same n VBlanks again (second pass, gPortResim), a save, and the two compared; then the next checkpoint. */
static void synctest_step(uint32_t vb) {
    if (sPhase == 0) {
        if (vb < sSyncStart) {
            return;
        }
        if (timed_save(sSlot) != 0) {
            sSkipped++;
            return;
        }
        sPhase = 1;
        sSyncFrom = vb;
        sSyncTo = vb + sSyncN;
        return;
    }
    if (vb != sSyncTo) {
        return;
    }
    if (sPhase == 1) {
        timed_save(sBufA);
        timed_load(sSlot, "synctest");
        sPhase = 2;
        gPortResim = 1;
        return; /* the game goes on at the checkpoint */
    }
    timed_save(sBufB);
    gPortResim = 0;
    sChecks++;
    if (compare(sBufA, sBufB, vb)) {
        if (sDiverged == 0) {
            printf("ttt: synctest: first divergence between vblanks %u and %u\n", (unsigned)sSyncFrom, (unsigned)vb);
        }
        sDiverged++;
    }
    sPhase = 0;
    synctest_step(vb);
}

/* TTT_ROLLBACK=<n> (F07, ahead of T45): rollback netplay's steady worst case. At every VBlank from TTT_ROLLBACK_FROM + n
   on: a save, the state of n VBlanks before loaded and those n VBlanks run again (gPortResim) with a save at each, as
   when a peer's inputs for all of them came late; then the game goes on. The report gives the wall time per VBlank
   (all of it work: a headless run does not wait) against the 16.68 ms of a VBlank. TTT_ROLLBACK_CHECK=1 also
   compares the state each rollback comes back to with the one saved the first time. */
#define VBLANK_NS 16683333u
static void rollback_step(uint32_t vb) {
    uint64_t now = ns_now();
    uint8_t *slot = sRbSlot[vb % (sRbN + 1)];
    if (!gPortResim) {
        if (sRbLast != 0 && vb > sRbFrom + sRbN) { /* the last VBlank, its rollback included */
            uint64_t d = now - sRbLast;
            sRbSum += d;
            sRbMax = d > sRbMax ? d : sRbMax;
            sRbOver += d > VBLANK_NS;
            sRbFrames++;
        }
        sRbLast = now;
        if (vb < sRbFrom) {
            return;
        }
        if (timed_save(slot) != 0) {
            sRbSkipped++;
            return;
        }
        if (vb >= sRbFrom + sRbN) {
            sRbTarget = vb;
            sRbStart = now;
            sRbCpu0 = cpu_now();
            timed_load(sRbSlot[(vb - sRbN) % (sRbN + 1)], "rollback");
            gPortResim = 1;
            gPortNoGs = sRbNoDraw;
        }
        return;
    }
    if (vb < sRbTarget) {
        timed_save(slot);
        return;
    }
    now -= sRbStart; /* back where the rollback started: its time */
    sRbCpuSum += cpu_now() - sRbCpu0;
    sRbRolls++;
    sRbWorkSum += now;
    sRbWorkMax = now > sRbWorkMax ? now : sRbWorkMax;
    gPortResim = 0;
    gPortNoGs = 0;
    if (sRbCheck != NULL && Port_StateSave(sRbCheck) == 0) {
        sRbChecks++;
        if (compare(slot, sRbCheck, vb)) {
            if (sDiverged == 0) {
                printf("ttt: rollback: first divergence at vblank %u\n", (unsigned)vb);
            }
            sDiverged++;
            sRbDiverged++;
        }
    }
}

/* ---------------------------------------------------------------------------------------------- the boundary */

void port_boundary_body(void) {
    uint32_t vb;
    if (!sInit) {
        init();
    }
    if (!sOn && sTestHook == NULL) {
        return;
    }
    sInBoundary = 1;
    if (sTestHook != NULL) {
        sTestHook();
    }
    if (gPortStateWant) {
        gPortStateWant = 0;
        load_file();
        sInBoundary = 0;
        return;
    }
    vb = Port_VBlankCount();
    if (sHashEvery != 0 && vb % sHashEvery == 0 && !gPortResim) {
        Gs_Finish(); /* (nothing hashed is the GS threads', but the CSR bits they keep are written back at a drain) */
        printf("ttt: hash %u %016llx\n", (unsigned)vb, (unsigned long long)Port_StateHash());
    }
    if (sSaveAt != 0 && vb == sSaveAt && !gPortResim) {
        save_file(vb);
    }
    if (sSyncN != 0) {
        synctest_step(vb);
    }
    if (sRbN != 0) {
        rollback_step(vb);
    }
    sInBoundary = 0;
}

/* The TTT_FRAMES report's lines (plat_hw.c). */
void Port_StateReport(void) {
    if (getenv("TTT_HASH_REGIONS") != NULL) {
        hash_regions_print();
    }
    if (getenv("TTT_HASH_DUMP_SPR") != NULL) { /* the scratchpad, then the EE hardware registers, as hashed */
        FILE *f = fopen(getenv("TTT_HASH_DUMP_SPR"), "wb");
        const uint32_t *s = Port_Ptr(PORT_SPR_ADDR), *hw = Port_Ptr(0x10000000u);
        uint32_t i, v;
        for (i = 0; f != NULL && i < (PORT_SPR_SIZE + 0x10000u) / 4; i++) {
            v = norm_word(i < PORT_SPR_SIZE / 4 ? s[i] : hw[i - PORT_SPR_SIZE / 4]);
            fwrite(&v, 4, 1, f);
        }
        if (f != NULL) {
            fclose(f);
        }
    }
    if (getenv("TTT_HASH_DUMP") != NULL) { /* EE RAM as the hash sees it, to compare two builds word by word */
        FILE *f = fopen(getenv("TTT_HASH_DUMP"), "wb");
        const uint32_t *w = Port_Ptr(0x20000000u);
        uint32_t i, v;
        for (i = 0; f != NULL && i < STACK_LO / 4; i++) {
            v = i * 4 >= ELFHDR_LO && i * 4 < ELFHDR_HI ? 0 : norm_word(w[i]);
            fwrite(&v, 4, 1, f);
        }
        if (f != NULL) {
            fclose(f);
        }
    }
    if (getenv("TTT_STATE_STACK") != NULL) { /* EE stack words that look like this process's heap or libraries */
        const uint32_t *w = Port_Ptr(STACK_LO);
        uint32_t i, n = 0;
        for (i = 0; i < (PORT_EE_RAM_SIZE - STACK_LO) / 4; i++) {
            if (((w[i] >= HOST_HI && w[i] < 0x10000000u) || (w[i] >> 25) == 0x7Bu) && n++ < 32) {
                printf("ttt: state-stack %08X: %08X\n", (unsigned)(STACK_LO + i * 4), (unsigned)w[i]);
            }
        }
        printf("ttt: state-stack: %u words\n", (unsigned)n);
    }
    if (sSyncN != 0) {
        printf("ttt: synctest: %u checks of %u vblanks, %u diverged, %u skipped; state %u bytes, save %u us, load %u us"
               " (mean, besides %u us waiting for the GS threads)\n",
               (unsigned)sChecks, (unsigned)sSyncN, (unsigned)sDiverged, (unsigned)sSkipped, (unsigned)Port_StateSize(),
               (unsigned)(sSaves ? sSaveNs / sSaves / 1000 : 0), (unsigned)(sLoads ? sLoadNs / sLoads / 1000 : 0),
               (unsigned)(sSaves + sLoads ? sDrainNs / (sSaves + sLoads) / 1000 : 0));
    }
    if (sRbN != 0) {
        printf("ttt: rollback: %u rollbacks of %u vblanks%s, %u checked, %u diverged, %u skipped; per vblank mean %u us, "
               "max %u us, %u over %u us; the rollback itself mean %u us (game thread CPU %u us), max %u us; save %u us, "
               "load %u us (mean, besides %u us waiting for the GS threads)\n",
               (unsigned)sRbRolls, (unsigned)sRbN, sRbNoDraw ? " undrawn" : "", (unsigned)sRbChecks,
               (unsigned)sRbDiverged, (unsigned)sRbSkipped, (unsigned)(sRbFrames ? sRbSum / sRbFrames / 1000 : 0),
               (unsigned)(sRbMax / 1000), (unsigned)sRbOver, VBLANK_NS / 1000,
               (unsigned)(sRbRolls ? sRbWorkSum / sRbRolls / 1000 : 0), (unsigned)(sRbRolls ? sRbCpuSum / sRbRolls / 1000 : 0),
               (unsigned)(sRbWorkMax / 1000),
               (unsigned)(sSaves ? sSaveNs / sSaves / 1000 : 0), (unsigned)(sLoads ? sLoadNs / sLoads / 1000 : 0),
               (unsigned)(sSaves + sLoads ? sDrainNs / (sSaves + sLoads) / 1000 : 0));
    }
    if (sTrack != 0 || sTrkScans != 0) {
        printf("ttt: state-track: %s; %u pages tracked, %llu scans (mean %u us, %u pages written), %llu of %llu saves and "
               "%llu of %llu loads by page (mean %u / %u pages copied); %llu checked, %llu wrong\n",
               sTrkWhy, (unsigned)sTrkPages, (unsigned long long)sTrkScans,
               (unsigned)(sTrkScans ? sTrkScanNs / sTrkScans / 1000 : 0), (unsigned)(sTrkScans ? sTrkWritten / sTrkScans : 0),
               (unsigned long long)sTrkIncSaves, (unsigned long long)sAllSaves, (unsigned long long)sTrkIncLoads,
               (unsigned long long)sAllLoads, (unsigned)(sTrkIncSaves ? sTrkPagesSaved / sTrkIncSaves : 0),
               (unsigned)(sTrkIncLoads ? sTrkPagesLoaded / sTrkIncLoads : 0), (unsigned long long)sTrkChecks,
               (unsigned long long)sTrkCheckBad);
    }
}

/* ---------------------------------------------------------------------------------------------- TTT_SELFTEST=state */

void func_003D9588(void); /* the two game functions .sbss points at (0x5F6FD0 / 0x5F6FD4, gotcha 20) */
void func_003DA138(void);
static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: state self-test FAILED: %s\n", what);
        sFails++;
    }
}

static uint64_t buf_hash(const void *p, uint32_t n) {
    XXH3_64bits_reset(&sXxh);
    hash_bytes(&sXxh, p, n, 0);
    return XXH3_64bits_digest(&sXxh);
}

/* The round trip runs on the EE stack, as the game does: a state holds that stack (not the host's). */
void port_call_on_ee(void (*fn)(void));
#if defined(__x86_64__)
__asm__(".text\n"
        ".globl port_call_on_ee\n"
        ".type port_call_on_ee, @function\n"
        "port_call_on_ee:\n"
        "\tpushq %rbp\n"
        "\tmovq %rsp, %rbp\n"
        "\tmovl $0x01FFFF00, %esp\n"
        "\tcall *%rdi\n"
        "\tmovq %rbp, %rsp\n"
        "\tpopq %rbp\n"
        "\tret\n"
        ".size port_call_on_ee, .-port_call_on_ee\n");
#else
__asm__(".text\n"
        ".globl port_call_on_ee\n"
        ".type port_call_on_ee, @function\n"
        "port_call_on_ee:\n"
        "\tpushl %ebp\n"
        "\tmovl %esp, %ebp\n"
        "\tmovl 8(%ebp), %eax\n"
        "\tmovl $0x01FFFF00, %esp\n"
        "\tcall *%eax\n"
        "\tmovl %ebp, %esp\n"
        "\tpopl %ebp\n"
        "\tret\n"
        ".size port_call_on_ee, .-port_call_on_ee\n");
#endif

#if defined(__x86_64__)
#define LOW_STACK (1u << 20)
static uint8_t sLowStack[LOW_STACK] __attribute__((aligned(16)));
void port_call_on_stack(void (*fn)(void), void *top);
__asm__(".text\n"
        ".type port_call_on_stack, @function\n"
        "port_call_on_stack:\n"
        "\tpushq %rbp\n"
        "\tmovq %rsp, %rbp\n"
        "\tmovq %rsi, %rsp\n"
        "\tcall *%rdi\n"
        "\tmovq %rbp, %rsp\n"
        "\tpopq %rbp\n"
        "\tret\n"
        ".size port_call_on_stack, .-port_call_on_stack\n");
void Port_OnLowStack(void (*fn)(void)) {
    if ((uint64_t)(uintptr_t)__builtin_frame_address(0) >> 32) {
        port_call_on_stack(fn, sLowStack + LOW_STACK);
    } else {
        fn();
    }
}
#else
void Port_OnLowStack(void (*fn)(void)) { fn(); }
#endif

static uint8_t *sTestBuf;
static volatile int sTestPass;
static uint64_t sTestHash;
static void test_save(void) { expect(Port_StateSave(sTestBuf) == 0, "save at a boundary"); }
static void test_load(void) { expect(Port_StateLoad(sTestBuf) == 0, "load at a boundary"); }

/* A boundary that saves, a change to EE RAM and the scratchpad, a boundary that loads: the game goes on after the
   first boundary again, with the saved words. */
static void round_trip(void) {
    volatile uint32_t *ee = Port_Ptr(0x00100000u), *spr = Port_Ptr(PORT_SPR_ADDR + 0x100u);

    sTestBuf = must_alloc(Port_StateSize());
    ee[0] = 0x11111111u;
    *spr = 0x22222222u;
    sTestPass = 0;
    sTestHook = test_save;
    Port_StateBoundary();
    sTestPass++;
    if (sTestPass == 1) {
        sTestHash = Port_StateHash(); /* (the save's hooks set some modules up first: the pads' rest state) */
        ee[0] = 0x33333333u;
        *spr = 0x44444444u;
        expect(Port_StateHash() != sTestHash, "the hash sees the change");
        sTestHook = test_load;
        Port_StateBoundary();
        expect(0, "the load came back to its own boundary");
        printf("ttt: state: load refused: %s\n", sBusyWhy);
    } else {
        expect(sTestPass == 2, "back after the saving boundary (after the load)");
        expect(ee[0] == 0x11111111u && *spr == 0x22222222u, "EE RAM and scratchpad back as saved");
        expect(Port_StateHash() == sTestHash, "the hash back as saved");
    }
    sTestHook = NULL;
    expect(Port_StateSave(sTestBuf) != 0, "no save outside a boundary");
}

/* Page tracking (F07): a save into and a load from a tracked buffer give what whole copies give (trk_check), after
   writes through each view of EE RAM, by the kernel (a system call's result) and into GS memory and sound RAM, and
   copy only about the pages written. */
static uint8_t *region_ptr(const char *name) {
    int r;
    for (r = 0; r < sRegN; r++) {
        if (strcmp(sReg[r].name, name) == 0) {
            return sReg[r].p + sReg[r].size / 2;
        }
    }
    return NULL;
}

static void track_test(void) {
    uint8_t *buf, *vram = region_ptr("gs.vram"), *snd = region_ptr("spu2.ram");
    uint64_t saved, loaded, checks;
    if (!Port_StateTrack(1)) {
        printf("ttt: state: page tracking not offered here (%s): not tested\n", sTrkWhy);
        return;
    }
    buf = must_alloc(Port_StateSize());
    memset(buf, 0, sizeof(StateHeader));
    sInBoundary = 1;
    sTrkCheck = 1;
    expect(Port_StateSave(buf) == 0 && ((StateHeader *)buf)->token == sToken, "a save with tracking on is stamped");
    *(volatile uint32_t *)Port_Ptr(0x00100000u) = 0x5A5A0001u;
    *(volatile uint32_t *)Port_Ptr(0x20200004u) = 0x5A5A0002u;
    *(volatile uint32_t *)Port_Ptr(0x30300008u) = 0x5A5A0003u;
    syscall(SYS_clock_gettime, 1, Port_Ptr(0x00180000u)); /* written by the kernel */
    if (vram != NULL && snd != NULL) {
        *vram ^= 0x55;
        *snd ^= 0x55;
    }
    saved = sTrkPagesSaved;
    checks = sTrkChecks;
    expect(Port_StateSave(buf) == 0 && sTrkChecks == checks + 1, "a save into a tracked buffer is by page");
    expect(sTrkPagesSaved - saved >= 6 && sTrkPagesSaved - saved < 64, "it copies about the pages written");
    *(volatile uint32_t *)Port_Ptr(0x00100000u) = 0x77777777u;
    *(volatile uint32_t *)Port_Ptr(0x00180000u) = 0x77777777u;
    if (vram != NULL) {
        *vram ^= 0xFF;
    }
    loaded = sTrkPagesLoaded;
    expect(Port_StateLoad(buf) == 0 && sTrkChecks == checks + 2, "a load from a tracked buffer is by page");
    expect(sTrkPagesLoaded - loaded >= 2 && sTrkPagesLoaded - loaded < 64, "it copies about the pages written");
    expect(*(volatile uint32_t *)Port_Ptr(0x20100000u) == 0x5A5A0001u, "the loaded word back");
    expect(sTrkCheckBad == 0, "tracked saves and loads equal whole copies");
    sTrkCheck = 0;
    sInBoundary = 0;
    *(volatile uint32_t *)Port_Ptr(0x00100000u) = 0;
    *(volatile uint32_t *)Port_Ptr(0x20200004u) = 0;
    *(volatile uint32_t *)Port_Ptr(0x30300008u) = 0;
    Port_StateTrack(0);
    printf("ttt: state: page tracking: %u pages tracked\n", (unsigned)sTrkPages);
    free(buf);
}

void Port_StateSelfTest(void) {
    uint32_t a = (uint32_t)(uintptr_t)func_003D9588, b = (uint32_t)(uintptr_t)func_003DA138;
    uint32_t w1[4], w2[4], w3[4];

    syms_init();
    expect(norm_word(a) == HOST_ID && norm_word(b) == HOST_ID, "the two .sbss callbacks -> the host constant");
    expect(norm_word(0x00123456u) == 0x00123456u && norm_word(0x3F800000u) == 0x3F800000u, "other words unchanged");
    expect(norm_word(a + 0x57) == HOST_ID && norm_word((uint32_t)(uintptr_t)&sRegN) == HOST_ID, "any host address");
    expect(norm_word(0x09000000u) == 0x09000000u && norm_word(0x07FFFFFFu) == 0x07FFFFFFu, "outside the window");
    /* the slots hold other host addresses in another build (the functions moved): the hash is the same */
    w1[0] = a; w1[1] = b; w1[2] = 0x1234; w1[3] = 0;
    w2[0] = a + 0x1000; w2[1] = b - 0x2340; w2[2] = 0x1234; w2[3] = 0;
    w3[0] = a; w3[1] = b; w3[2] = 0x1235; w3[3] = 0;
    expect(buf_hash(w1, 16) == buf_hash(w2, 16), "moved callbacks: the same hash");
    expect(buf_hash(w1, 16) != buf_hash(w3, 16), "other data: another hash");
    port_call_on_ee(round_trip);
    track_test();
    printf("ttt: state: %d regions, %u bytes; program image 0x%08X-0x%08X\n", sRegN, (unsigned)Port_StateSize(),
           (unsigned)sImgLo, (unsigned)sImgHi);
    if (sFails == 0) {
        printf("ttt: state self-test OK\n");
    }
    exit(sFails != 0);
}
