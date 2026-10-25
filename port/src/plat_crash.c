/*
 * Crash reports (T41, docs/port/README.md "Crash reports").
 *
 * SIGSEGV, SIGBUS, SIGILL, SIGFPE and SIGABRT write a text report to `<user dir>/crash/ttt-crash-<UTC time>-<pid>.txt`
 * (<user dir>: the settings file's directory, plat_settings.c; without one, the working directory) and print its path
 * on stderr; then the signal takes its default course (the handler is reset first), so the exit status and any core
 * dump are as without the handler. The handler runs on a stack of its own (sigaltstack, mapped at start): the game
 * thread runs on the EE stack in EE RAM (plat_main.c), which may be the one that failed.
 *
 * In the handler only async-signal-safe calls (open, write, read, close, mkdir, clock_gettime, getpid, raise) and
 * plain memory reads: what needs the C library is written beforehand into static text (Port_CrashInstall at start,
 * Port_CrashRefresh after a settings change, into the buffer the handler does not read at the time). The report holds:
 * the signal and the fault address, the host registers, the function the instruction is in and code addresses found
 * on the stack (names from build/port/ttt.syms, which port/tools/link.py writes from the program's own symbols: the
 * game's functions are native code, so the EE pc is the PS2 address of the function's name, not tracked per
 * instruction), the last VBlank, the renderer, the build id, the settings and the state of the game data
 * (plat_data.c). No memory of the game beyond those few words.
 *
 * TTT_CRASH_TEST=<kind>[@<vblank>] (port only, for tests/test_port_crash.py): segv, stack (a fault with a bad stack
 * pointer: only an alternate stack can take it), ill, fpe, bus, abrt, at that VBlank (default 30).
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include "plat_settings.h"

#ifndef TTT_GIT
#define TTT_GIT "unknown"
#endif

uint32_t Port_VBlankCount(void);    /* plat_hw.c */
int Port_WindowOpen(void);          /* plat_window.c: 1 with a window (already opened) */
int Port_WindowGl(void);
int Port_DataState(void);           /* plat_data.c */
const char *Port_DataWhy(void);
extern uint8_t gPortOffStack[], gPortCtxStack[]; /* plat_state.c: the port's own stacks (262144 bytes each) */
extern uint32_t gPortOffEeEsp;                   /* plat_state.c: the EE stack pointer Port_OffEe left */
extern char __executable_start[], etext[];

#define ALT_STACK (64 * 1024)
#define REPORT_MAX 16384
#define STACK_ADDR 8                     /* code addresses taken from a stack */
#define MAX_ADDR (1 + 2 * STACK_ADDR)    /* the instruction's, the current stack's, the EE stack's */

static int sInstalled;
static char sDir[1100], sParent[1100]; /* <user dir>/crash and <user dir> */
static char sSyms[1100];               /* build/port/ttt.syms beside the program */
static char sRenderer[128];
static char sSettings[2][2048];        /* the settings line; the handler reads sSettings[sCur] */
static int sCur;
static uint32_t sMainStack; /* an address on the main thread's host stack, taken at start (the words above it are mapped) */
static int sTestKind, sTestAt = 30;
static char sReport[REPORT_MAX];
static int sLen;

static const struct { int sig; const char *name; } kSignals[] = {
    {SIGSEGV, "SIGSEGV"}, {SIGBUS, "SIGBUS"}, {SIGILL, "SIGILL"}, {SIGFPE, "SIGFPE"}, {SIGABRT, "SIGABRT"},
};
#define NSIGNALS ((int)(sizeof kSignals / sizeof kSignals[0]))

/* ---------------------------------------------------------------------------------------------- text, signal-safe */

static void put(const char *s) {
    while (*s && sLen < REPORT_MAX - 1) {
        sReport[sLen++] = *s++;
    }
}

static void put_hex(uint32_t v) {
    char b[11] = "0x";
    int i;
    for (i = 0; i < 8; i++) {
        b[2 + i] = "0123456789abcdef"[v >> (28 - 4 * i) & 15];
    }
    b[10] = 0;
    put(b);
}

static void put_dec(uint64_t v, int width) {
    char b[24];
    int n = 0;
    do {
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v != 0 || n < width);
    while (n > 0) {
        char c[2] = {b[--n], 0};
        put(c);
    }
}

static void line(const char *key, const char *value) {
    put(key);
    put(value);
    put("\n");
}

/* ---------------------------------------------------------------------------------------------- names */

static uint32_t sAddr[MAX_ADDR], sSymAt[MAX_ADDR], sSymPs2[MAX_ADDR];
static char sSymName[MAX_ADDR][96];
static int sAddrN;

static uint32_t parse_hex(const char *p, const char *end) {
    uint32_t v = 0;
    for (; p < end; p++) {
        int d = *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : -1;
        if (d < 0) {
            break;
        }
        v = v << 4 | (uint32_t)d;
    }
    return v;
}

/* One line of ttt.syms: "<host address> <PS2 address or 00000000> <name>". */
static void sym_line(const char *l, int n) {
    uint32_t at, ps2;
    int i;
    if (n < 19 || l[8] != ' ' || l[17] != ' ') {
        return;
    }
    at = parse_hex(l, l + 8);
    ps2 = parse_hex(l + 9, l + 17);
    for (i = 0; i < sAddrN; i++) {
        if (at <= sAddr[i] && at >= sSymAt[i]) {
            int k = n - 18 < (int)sizeof sSymName[i] - 1 ? n - 18 : (int)sizeof sSymName[i] - 1;
            sSymAt[i] = at;
            sSymPs2[i] = ps2;
            memcpy(sSymName[i], l + 18, (size_t)k);
            sSymName[i][k] = 0;
        }
    }
}

/* The nearest symbol at or below each of sAddr, in one pass over ttt.syms (read and close: signal-safe). */
static void resolve_names(void) {
    static char buf[4096 + 256];
    int fd = sSyms[0] ? open(sSyms, O_RDONLY) : -1, have = 0, i;
    ssize_t n;
    for (i = 0; i < sAddrN; i++) {
        sSymAt[i] = 0;
        sSymName[i][0] = 0;
    }
    if (fd < 0) {
        return;
    }
    while ((n = read(fd, buf + have, sizeof buf - 1 - (size_t)have)) > 0) {
        int start = 0, end = have + (int)n;
        for (i = 0; i < end; i++) {
            if (buf[i] == '\n') {
                sym_line(buf + start, i - start);
                start = i + 1;
            }
        }
        have = end - start;
        if (have >= (int)sizeof buf - 1) {
            have = 0; /* a line longer than the buffer: not ours */
        }
        memmove(buf, buf + start, (size_t)have);
    }
    close(fd);
}

static int in_code(uint32_t a) { return a >= (uint32_t)(uintptr_t)__executable_start && a < (uint32_t)(uintptr_t)etext; }

static void put_name(int i) {
    if (!in_code(sAddr[i])) {
        put("(outside the program: a library)");
    } else if (sSymName[i][0] == 0) {
        put(sSyms[0] ? "(no symbol: build/port/ttt.syms missing or older)" : "(no symbol table)");
    } else {
        put(sSymName[i]);
        put("+");
        put_hex(sAddr[i] - sSymAt[i]);
        if (sSymPs2[i] != 0) {
            put(" (EE ");
            put_hex(sSymPs2[i]);
            put(")");
        }
    }
}

/* Code addresses among the words above the stack pointer (at most STACK_ADDR, the first 4 KB), when the stack is one
   the program knows to be mapped: the EE stack (EE RAM), the port's service stacks, the main thread's host stack. */
static void scan_stack(uint32_t esp) {
    uint32_t hi = 0, a;
    uint32_t off = (uint32_t)(uintptr_t)gPortOffStack, ctx = (uint32_t)(uintptr_t)gPortCtxStack;
    uint32_t main = sMainStack;
    int found = 0;
    if (esp >= 0x00100000u && esp < 0x02000000u) {
        hi = 0x02000000u;
    } else if (esp >= off && esp < off + 262144u) {
        hi = off + 262144u;
    } else if (esp >= ctx && esp < ctx + 262144u) {
        hi = ctx + 262144u;
    } else if (esp < main && main - esp < (8u << 20)) {
        hi = main;
    }
    for (a = esp & ~3u; hi != 0 && a + 4 <= hi && a < esp + 4096 && found < STACK_ADDR; a += 4) {
        uint32_t w = *(const uint32_t *)(uintptr_t)a;
        if (in_code(w)) {
            sAddr[sAddrN++] = w;
            found++;
        }
    }
}

static int is_ee_stack(uint32_t esp) { return esp >= 0x00100000u && esp < 0x02000000u; }

/* ---------------------------------------------------------------------------------------------- the handler */

/* Days since 1970-01-01 to a civil date (UTC). */
static void civil(int64_t days, int *y, int *m, int *d) {
    int64_t z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097, yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

/* The time now (UTC), "YYYYMMDD-HHMMSS" (sep) or "YYYYMMDDTHHMMSS", into out[16]. */
static void stamp(char *out, int sep) {
    struct timespec ts;
    int y, m, d, i, v[6];
    int64_t s;
    char *p = out;
    clock_gettime(CLOCK_REALTIME, &ts);
    s = (int64_t)ts.tv_sec;
    civil(s / 86400, &y, &m, &d);
    v[0] = y, v[1] = m, v[2] = d, v[3] = (int)(s % 86400 / 3600), v[4] = (int)(s % 3600 / 60), v[5] = (int)(s % 60);
    for (i = 0; i < 6; i++) {
        if (i == 0) {
            *p++ = (char)('0' + v[0] / 1000 % 10), *p++ = (char)('0' + v[0] / 100 % 10);
        }
        if (i == 3) {
            *p++ = sep ? '-' : 'T';
        }
        *p++ = (char)('0' + v[i] / 10 % 10), *p++ = (char)('0' + v[i] % 10);
    }
    *p = 0;
}

static void write_all(int fd, const char *p, size_t n) {
    while (n > 0) {
        ssize_t k = write(fd, p, n);
        if (k <= 0) {
            if (k < 0 && errno == EINTR) {
                continue;
            }
            return;
        }
        p += k, n -= (size_t)k;
    }
}

static void handler(int sig, siginfo_t *si, void *ctx) {
    static const char *const kData[] = {"USA release (verified)", "[modified game data]", "being checked",
                                        "not checked"};
    ucontext_t *uc = ctx;
    const greg_t *g = uc->uc_mcontext.gregs;
    char when[20], path[1300];
    int saved = errno, fd, i, state, nHere;
    uint32_t fault, esp;
    const char *name = "?";

    for (i = 0; i < NSIGNALS; i++) {
        if (kSignals[i].sig == sig) {
            name = kSignals[i].name;
        }
    }
    sAddrN = 0;
    sAddr[sAddrN++] = (uint32_t)g[REG_EIP];
    fault = (uint32_t)(uintptr_t)si->si_addr;
    esp = (uint32_t)g[REG_ESP];
    if (!(sig == SIGSEGV && si->si_code > 0 && fault + 65536u >= esp && fault < esp + 4096u)) {
        scan_stack(esp); /* not when the stack itself is what failed (an overflow into its guard page) */
    }
    nHere = sAddrN;
    /* off the EE stack on the game's thread (Port_OffEe, the port's VBlank and library work): the game's frames are
       above the EE stack pointer it left */
    if (!is_ee_stack((uint32_t)g[REG_ESP]) && is_ee_stack(gPortOffEeEsp) && syscall(SYS_gettid) == getpid()) {
        scan_stack(gPortOffEeEsp);
    }
    resolve_names();

    stamp(when, 1);
    sLen = 0;
    put(sDir);
    put("/ttt-crash-");
    put(when);
    put("-");
    put_dec((uint64_t)getpid(), 1);
    put(".txt");
    memcpy(path, sReport, (size_t)sLen + 1 < sizeof path ? (size_t)sLen : sizeof path - 1);
    path[sLen < (int)sizeof path ? sLen : (int)sizeof path - 1] = 0;

    sLen = 0;
    put("Tekken Tag Tournament PC port: crash report\n");
    stamp(when, 0);
    line("time (UTC): ", when);
    line("build: ", TTT_GIT);
    put("signal: ");
    put(name);
    put(" (");
    put_dec((uint64_t)sig, 1);
    put("), code ");
    if (si->si_code < 0) {
        put("-");
    }
    put_dec((uint64_t)(si->si_code < 0 ? -si->si_code : si->si_code), 1);
    put("\nfault address: ");
    if (si->si_code > 0) {
        put_hex((uint32_t)(uintptr_t)si->si_addr);
    } else {
        put("(none: the signal was raised, not a fault)");
    }
    put("\nthread: ");
    put(syscall(SYS_gettid) == getpid() ? "main (the game)" : "other (window, GS or audio)");
    put("\nregisters: eip ");
    put_hex((uint32_t)g[REG_EIP]);
    put(" esp ");
    put_hex((uint32_t)g[REG_ESP]);
    put(" ebp ");
    put_hex((uint32_t)g[REG_EBP]);
    put(" eflags ");
    put_hex((uint32_t)g[REG_EFL]);
    put("\n           eax ");
    put_hex((uint32_t)g[REG_EAX]);
    put(" ebx ");
    put_hex((uint32_t)g[REG_EBX]);
    put(" ecx ");
    put_hex((uint32_t)g[REG_ECX]);
    put(" edx ");
    put_hex((uint32_t)g[REG_EDX]);
    put(" esi ");
    put_hex((uint32_t)g[REG_ESI]);
    put(" edi ");
    put_hex((uint32_t)g[REG_EDI]);
    put("\nstack: ");
    put(is_ee_stack(esp) ? "EE stack (EE RAM)" : "host");
    put("\nfunction: ");
    put_name(0);
    put("\n");
    for (i = 1; i < sAddrN; i++) {
        if (i == nHere) {
            put("EE stack pointer left by Port_OffEe: ");
            put_hex(gPortOffEeEsp);
            put("\n");
        }
        put(i < nHere ? "stack code address: " : "EE stack code address: ");
        put_hex(sAddr[i]);
        put(" ");
        put_name(i);
        put("\n");
    }
    put("last vblank: ");
    put_dec(Port_VBlankCount(), 1);
    put("\n");
    line("renderer: ", sRenderer);
    line("settings: ", sSettings[__atomic_load_n(&sCur, __ATOMIC_ACQUIRE)]);
    state = Port_DataState();
    put("game data: ");
    put(state >= 0 && state < 4 ? kData[state] : "?");
    if (state != 0 && Port_DataWhy()[0]) {
        put(" (");
        put(Port_DataWhy());
        put(")");
    }
    put("\n");

    mkdir(sParent, 0755);
    mkdir(sDir, 0755);
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd >= 0) {
        write_all(fd, sReport, (size_t)sLen);
        close(fd);
    }
    sLen = 0;
    put("ttt: crash: ");
    put(name);
    put(fd >= 0 ? "; report written to " : "; no report (cannot write ");
    put(path);
    put(fd >= 0 ? "\n" : ")\n");
    write_all(2, sReport, (size_t)sLen);
    errno = saved;
    /* SA_RESETHAND gave the signal back its default action: a fault happens again on return, a raised signal is
       raised again here (delivered when the handler returns) */
    if (si->si_code <= 0) {
        raise(sig);
    }
}

/* ---------------------------------------------------------------------------------------------- set-up */

/* The settings line the report shows; after each change too (plat_settings.c), into the buffer not in use. */
void Port_CrashRefresh(void) {
    int next;
    if (!sInstalled) {
        return;
    }
    next = sCur ^ 1;
    Settings_Summary(sSettings[next], (int)sizeof sSettings[next]);
    __atomic_store_n(&sCur, next, __ATOMIC_RELEASE);
}

static void test_switch(void) {
    static const char *const kKinds[] = {"segv", "stack", "ill", "fpe", "bus", "abrt"};
    const char *e = getenv("TTT_CRASH_TEST"), *at;
    unsigned i;
    if (e == NULL || !*e) {
        return;
    }
    at = strchr(e, '@');
    for (i = 0; i < sizeof kKinds / sizeof kKinds[0]; i++) {
        size_t n = strlen(kKinds[i]);
        if (strncmp(e, kKinds[i], n) == 0 && (e[n] == 0 || e[n] == '@')) {
            sTestKind = (int)i + 1;
        }
    }
    if (at != NULL) {
        sTestAt = atoi(at + 1);
    }
    if (sTestKind == 0) {
        fprintf(stderr, "ttt: TTT_CRASH_TEST=segv|stack|ill|fpe|bus|abrt[@<vblank>]\n");
    }
}

/* Called by __wrap_main (plat_main.c) after the settings and the window, before the game: on the host stack. */
void Port_CrashInstall(void) {
    struct sigaction sa;
    stack_t ss;
    const char *cfg = Settings_Path();
    char exe[1024];
    ssize_t k;
    int i;

    sMainStack = (uint32_t)(uintptr_t)&ss;
    if (*cfg) {
        const char *slash = strrchr(cfg, '/');
        snprintf(sParent, sizeof sParent, "%.*s", slash != NULL ? (int)(slash - cfg) : 1, slash != NULL ? cfg : ".");
        if (sParent[0] == 0) {
            snprintf(sParent, sizeof sParent, "/");
        }
    } else {
        snprintf(sParent, sizeof sParent, ".");
    }
    snprintf(sDir, sizeof sDir, "%s/crash", sParent);
    k = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (k > 0) {
        exe[k] = 0;
        snprintf(sSyms, sizeof sSyms, "%s.syms", exe);
    }
    if (Port_WindowOpen()) {
        snprintf(sRenderer, sizeof sRenderer, "%s window", Port_WindowGl() ? "OpenGL 3.3" : "software (SDL renderer)");
    } else {
        char r[32];
        Settings_GetStr("video.renderer", r, sizeof r);
        snprintf(sRenderer, sizeof sRenderer, "no window (video.renderer %s)", r);
    }
    sInstalled = 1;
    Port_CrashRefresh();

    ss.ss_sp = mmap(NULL, ALT_STACK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ss.ss_size = ALT_STACK;
    ss.ss_flags = 0;
    if (ss.ss_sp == MAP_FAILED || sigaltstack(&ss, NULL) != 0) {
        fprintf(stderr, "ttt: crash reports: no alternate signal stack\n");
    }
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (i = 0; i < NSIGNALS; i++) {
        sigaction(kSignals[i].sig, &sa, NULL);
    }
    test_switch();
}

/* TTT_CRASH_TEST: the chosen fault at the chosen VBlank (plat_hw.c's VBlank, off the EE stack). */
void Port_CrashVBlank(uint32_t n) {
    void *page;
    if (sTestKind == 0 || n != (uint32_t)sTestAt) {
        return;
    }
    fflush(stdout);
    page = mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    switch (sTestKind) {
    case 1: /* segv: a write to a page with no access */
        *(volatile int *)page = 1;
        break;
    case 2: /* stack: the stack pointer itself on that page */
        __asm__ volatile("movl %0, %%esp\n\tpushl $0" : : "r"((char *)page + 2048) : "memory");
        break;
    case 3:
        __asm__ volatile("ud2");
        break;
    case 4: /* an integer division by zero (as asm: the compiler turns 1 / x into a comparison) */
        __asm__ volatile("xorl %%ecx, %%ecx\n\tmovl $1, %%eax\n\tcltd\n\tidivl %%ecx" : : : "eax", "ecx", "edx");
        break;
    case 5:
        raise(SIGBUS);
        break;
    case 6:
        abort();
        break;
    }
}
