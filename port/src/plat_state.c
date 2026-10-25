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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define memcpy __builtin_memcpy
#define XXH_INLINE_ALL
#define XXH_NO_STDLIB
#define XXH_FORCE_MEMORY_ACCESS 2 /* x86: unaligned loads are fine */
#include "../../third_party/xxhash/xxhash.h"
#include "plat_state.h"
#include "port_addr.h"
#include "gs.h"

uint32_t Port_VBlankCount(void); /* plat_hw.c */
int Port_Headless(void);

int gPortStateWant;
int gPortResim;

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

/* The PS2's memory, first of all (constructor 101, as the modules' registrations). */
__attribute__((constructor(101))) static void state_memory(void) {
    Port_StateAdd("ee.ram", Port_Ptr(0x20000000u), PORT_EE_RAM_SIZE, PS_HASH | PS_EERAM); /* the uncached view: all 32 MB */
    Port_StateAdd("ee.spr", Port_Ptr(PORT_SPR_ADDR), PORT_SPR_SIZE, PS_HASH);
    Port_StateAdd("ee.hw", Port_Ptr(0x10000000u), 0x10000u, PS_HASH);
    Port_StateAdd("vu0.mem", Port_Ptr(0x11000000u), 0x8000u, PS_HASH);
    Port_StateAdd("vu1.mem", Port_Ptr(0x11008000u), 0x8000u, 0);
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
    if (sElf == NULL || fread(sElf, 1, (size_t)n, f) != (size_t)n || memcmp(sElf, "\177ELF\001\001", 6) != 0) {
        fclose(f);
        return;
    }
    fclose(f);
    phoff = rd32(sElf + 28);
    phentsize = rd16(sElf + 42);
    phnum = rd16(sElf + 44);
    shoff = rd32(sElf + 32);
    shentsize = rd16(sElf + 46);
    shnum = rd16(sElf + 48);
    sImgLo = 0xFFFFFFFFu;
    for (k = 0; k < phnum; k++) { /* PT_LOAD segments above the PS2's map: the host image */
        const uint8_t *ph = sElf + phoff + k * phentsize;
        uint32_t va = rd32(ph + 8), memsz = rd32(ph + 20);
        if (rd32(ph) == 1 && va >= 0x02000000u) {
            sImgLo = va < sImgLo ? va : sImgLo;
            sImgHi = va + memsz > sImgHi ? va + memsz : sImgHi;
        }
    }
    for (k = 0; k < shnum; k++) {
        const uint8_t *sh = sElf + shoff + k * shentsize;
        if (rd32(sh + 4) == 2) { /* SHT_SYMTAB */
            const char *str = (const char *)sElf + rd32(sElf + shoff + rd32(sh + 24) * shentsize + 16);
            uint32_t off = rd32(sh + 16), size = rd32(sh + 20), j;
            for (j = 0; j + 16 <= size; j += 16) {
                const uint8_t *sym = sElf + off + j;
                uint32_t v = rd32(sym + 4), type = sym[12] & 15;
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
uint32_t gPortCtxEsp;
#define SERVICE_STACK 262144
uint8_t gPortCtxStack[SERVICE_STACK] __attribute__((aligned(16)));
void port_boundary_body(void);
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

/* Port_OffEe: fn(arg) on a stack of its own when called on the EE stack (the game's thread), else where it is. The
   EE stack pointer it left is kept in gPortOffEeEsp (the crash report's view of the game's frames, plat_crash.c). */
#define OFF_STACK 262144
uint8_t gPortOffStack[OFF_STACK] __attribute__((aligned(16)));
uint32_t gPortOffEeEsp;
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

/* ---------------------------------------------------------------------------------------------- save and load */

typedef struct {
    char magic[8];  /* "TTTSTATE" */
    uint32_t version, size;
    uint64_t build; /* the program's code and the region list */
    uint32_t esp, vblank;
    uint32_t pad[4];
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

int Port_StateSave(void *buf) {
    StateHeader *h = buf;
    uint8_t *d = (uint8_t *)buf + sizeof *h;
    int r;
    if (prepare() != 0) {
        return -1;
    }
    memset(h, 0, sizeof *h);
    memcpy(h->magic, "TTTSTATE", 8);
    h->version = STATE_VERSION;
    h->size = (uint32_t)Port_StateSize();
    h->build = sBuild;
    h->esp = gPortCtxEsp;
    h->vblank = Port_VBlankCount();
    for (r = 0; r < sRegN; r++) {
        if (!(sReg[r].flags & PS_HASHONLY)) {
            memcpy(d, sReg[r].p, sReg[r].size);
            d += (sReg[r].size + 15u) & ~15u;
        }
    }
    return 0;
}

int Port_StateLoad(const void *buf) {
    const StateHeader *h = buf;
    const uint8_t *d = (const uint8_t *)buf + sizeof *h;
    int r;
    if (memcmp(h->magic, "TTTSTATE", 8) != 0 || h->version != STATE_VERSION || h->size != Port_StateSize()) {
        sBusyWhy = "not a state of this version";
        return -1;
    }
    if (prepare() != 0) {
        return -1;
    }
    if (h->build != sBuild) {
        sBusyWhy = "saved by another build of the program";
        return -1;
    }
    for (r = 0; r < sRegN; r++) {
        if (!(sReg[r].flags & PS_HASHONLY)) {
            memcpy(sReg[r].p, d, sReg[r].size);
            d += (sReg[r].size + 15u) & ~15u;
        }
    }
    for (r = 0; r < sHookN; r++) {
        if (sHook[r].after != NULL) {
            sHook[r].after();
        }
    }
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
    sOn = sHashEvery || sSaveAt || sLoadFile || sSyncN;
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
   FINISH / SIGNAL / LABEL bits do not count). */
static int compare(const uint8_t *a, const uint8_t *b, uint32_t vb) {
    uint32_t off = sizeof(StateHeader), shown = 0;
    int r, bad = 0;
    for (r = 0; r < sRegN; r++) {
        const uint8_t *pa = a + off, *pb = b + off;
        uint32_t n = sReg[r].size, i, first = 0xFFFFFFFFu, count = 0;
        if (sReg[r].flags & PS_HASHONLY) {
            continue;
        }
        off += (n + 15u) & ~15u;
        if (memcmp(pa, pb, n) == 0) {
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

static void timed_save(uint8_t *buf) {
    uint64_t t0 = ns_now();
    Port_StateSave(buf);
    sSaveNs += ns_now() - t0 - sPrepNs;
    sDrainNs += sPrepNs;
    sSaves++;
}

/* At each boundary: a checkpoint (save), n VBlanks on (first pass), a save of where they led, the checkpoint loaded,
   the same n VBlanks again (second pass, gPortResim), a save, and the two compared; then the next checkpoint. */
static void synctest_step(uint32_t vb) {
    if (sPhase == 0) {
        if (vb < sSyncStart) {
            return;
        }
        uint64_t t0 = ns_now();
        if (Port_StateSave(sSlot) != 0) {
            sSkipped++;
            return;
        }
        sSaveNs += ns_now() - t0 - sPrepNs;
        sDrainNs += sPrepNs;
        sSaves++;
        sPhase = 1;
        sSyncFrom = vb;
        sSyncTo = vb + sSyncN;
        return;
    }
    if (vb != sSyncTo) {
        return;
    }
    if (sPhase == 1) {
        uint64_t t0;
        timed_save(sBufA);
        t0 = ns_now();
        if (Port_StateLoad(sSlot) != 0) {
            fprintf(stderr, "ttt: synctest: cannot load the checkpoint: %s\n", sBusyWhy);
            exit(2);
        }
        sLoadNs += ns_now() - t0 - sPrepNs;
        sDrainNs += sPrepNs;
        sLoads++;
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
    printf("ttt: state: %d regions, %u bytes; program image 0x%08X-0x%08X\n", sRegN, (unsigned)Port_StateSize(),
           (unsigned)sImgLo, (unsigned)sImgHi);
    if (sFails == 0) {
        printf("ttt: state self-test OK\n");
    }
    exit(sFails != 0);
}
