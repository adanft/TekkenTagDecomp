/*
 * The operating system under the port, Linux side (T42 b6, plat_os.h; Windows: plat_os_win32.c): the pieces of
 * plat_os.h too large to be inline. Each was moved here from the file named at it, unchanged, and makes its C library
 * calls as that file did: through the PLT (plat_mem.c, plat_state.c's image, plat_data.c) or through the GOT
 * (plat_crash.c, a -fno-plt file; plat_state.c's page tracking, plat_speech.c, plat_data.c's statx: PORT_NOPLT), with
 * PORT_GOT_ALIAS for a function called both ways, so the 32-bit program's PLT stays as it was (port_host.h).
 */
#ifndef _WIN32
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include "port_host.h"
#include "plat_os.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

/* the calls through the GOT (the C library functions not in the 32-bit program's PLT) */
PORT_NOPLT(statx);
PORT_NOPLT(sigaction);
PORT_NOPLT(sigaltstack);
PORT_NOPLT(sigemptyset);
PORT_NOPLT(__errno_location);
PORT_NOPLT(syscall);
PORT_NOPLT(raise);
PORT_NOPLT(ioctl);
PORT_NOPLT(madvise);
PORT_NOPLT(posix_spawnp);
PORT_NOPLT(waitpid);
/* functions of the PLT that plat_crash.c (-fno-plt) called through the GOT */
PORT_GOT_ALIAS(mmap);
PORT_GOT_ALIAS(getpid);

extern char **environ;

/* ---------------------------------------------------------------------------------------------- the command line */

/* plat_data.c (before main: the arguments from /proc/self/cmdline) */
int PlatOs_HasArg(const char *arg) {
    char cmd[4096];
    FILE *f;
    size_t n, i;
    if ((f = fopen("/proc/self/cmdline", "rb")) == NULL) {
        return 0;
    }
    n = fread(cmd, 1, sizeof cmd - 1, f);
    fclose(f);
    cmd[n] = 0;
    for (i = strlen(cmd) + 1; i < n; i += strlen(cmd + i) + 1) { /* the arguments after the program's name */
        if (strcmp(cmd + i, arg) == 0) {
            return 1;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------- files, processes */

/* plat_data.c: statx, not stat: -malign-double (PORT_CFLAGS) would move struct stat's 64-bit fields */
int PlatOs_FileKey(const char *path, PlatOsFileKey *k) {
    struct statx st;
    if (statx(AT_FDCWD, path, 0, STATX_INO | STATX_SIZE | STATX_MTIME, &st) != 0) {
        return -1;
    }
    k->dev_major = st.stx_dev_major;
    k->dev_minor = st.stx_dev_minor;
    k->ino = (unsigned long long)st.stx_ino;
    k->size = (unsigned long long)st.stx_size;
    k->mtime_s = (long long)st.stx_mtime.tv_sec;
    k->mtime_ns = st.stx_mtime.tv_nsec;
    return 0;
}

/* plat_speech.c */
int PlatOs_RunWait(char *const argv[]) {
    pid_t pid;
    int st;
    if (posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ) != 0) {
        return 0;
    }
    return waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

/* plat_crash.c */
int PlatOs_OnMainThread(void) { return syscall(SYS_gettid) == getpid_got(); }

/* ---------------------------------------------------------------------------------------------- fixed-address memory */

/* plat_mem.c: a memfd */
int PlatOs_MemCreate(const char *name, size_t size) {
    int fd = memfd_create(name, MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, (off_t)size) != 0) {
        return -1;
    }
    return fd;
}

int PlatOs_MapView(int mem, void *addr, size_t off, size_t size, int replace) {
    void *p = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_SHARED | (replace ? MAP_FIXED : MAP_FIXED_NOREPLACE), mem,
                   (off_t)off);
    return p == addr ? 0 : -1;
}

int PlatOs_MapFixed(void *addr, size_t size) {
    void *p = mmap(addr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    return p == addr ? 0 : -1;
}

/* plat_crash.c */
void *PlatOs_MapPages(size_t size, int rw) {
    void *p = mmap_got(NULL, size, rw ? PROT_READ | PROT_WRITE : PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p != MAP_FAILED ? p : NULL;
}

/* plat_mem.c: /proc/self/maps */
int PlatOs_RangeTaken(uintptr_t lo, uintptr_t hi, uintptr_t keep_lo, uintptr_t keep_hi, char *what, size_t n) {
    FILE *fp = fopen("/proc/self/maps", "r");
    char line[512];

    if (fp == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long a, b;
        if (sscanf(line, "%lx-%lx", &a, &b) != 2 || b <= lo || a >= hi) {
            continue;
        }
        if (a < keep_lo || b > keep_hi) {
            snprintf(what, n, "%s", line);
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

/* ---------------------------------------------------------------------------------------------- crash handlers */

static PlatOsFaultFn sFaultFn;

/* The registers of the report: the 32-bit build's, or on x86-64 (TTT_CC=clang64) the 64-bit ones, r8-r15 too. */
static void on_signal(int sig, siginfo_t *si, void *ctx) {
    const greg_t *g = ((ucontext_t *)ctx)->uc_mcontext.gregs;
    int saved = errno;
    PlatOsFault f;

    memset(&f, 0, sizeof f);
    f.sig = sig;
    f.code = si->si_code;
    f.addr = (uintptr_t)si->si_addr;
#if defined(__x86_64__)
    f.pc = (uintptr_t)g[REG_RIP], f.sp = (uintptr_t)g[REG_RSP], f.fp = (uintptr_t)g[REG_RBP];
    f.ax = (uintptr_t)g[REG_RAX], f.bx = (uintptr_t)g[REG_RBX], f.cx = (uintptr_t)g[REG_RCX];
    f.dx = (uintptr_t)g[REG_RDX], f.si = (uintptr_t)g[REG_RSI], f.di = (uintptr_t)g[REG_RDI];
    f.r[0] = (uintptr_t)g[REG_R8], f.r[1] = (uintptr_t)g[REG_R9], f.r[2] = (uintptr_t)g[REG_R10];
    f.r[3] = (uintptr_t)g[REG_R11], f.r[4] = (uintptr_t)g[REG_R12], f.r[5] = (uintptr_t)g[REG_R13];
    f.r[6] = (uintptr_t)g[REG_R14], f.r[7] = (uintptr_t)g[REG_R15];
#else
    f.pc = (uintptr_t)g[REG_EIP], f.sp = (uintptr_t)g[REG_ESP], f.fp = (uintptr_t)g[REG_EBP];
    f.ax = (uintptr_t)g[REG_EAX], f.bx = (uintptr_t)g[REG_EBX], f.cx = (uintptr_t)g[REG_ECX];
    f.dx = (uintptr_t)g[REG_EDX], f.si = (uintptr_t)g[REG_ESI], f.di = (uintptr_t)g[REG_EDI];
#endif
    f.flags = (uintptr_t)g[REG_EFL];
    sFaultFn(&f);
    errno = saved;
    /* SA_RESETHAND gave the signal back its default action: a fault happens again on return, a raised signal is
       raised again here (delivered when the handler returns) */
    if (si->si_code <= 0) {
        raise(sig);
    }
}

int PlatOs_CrashHooks(const int *sigs, int n, PlatOsFaultFn fn, size_t alt_stack) {
    struct sigaction sa;
    stack_t ss;
    int i, r = 0;

    sFaultFn = fn;
    ss.ss_sp = mmap_got(NULL, alt_stack, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ss.ss_size = alt_stack;
    ss.ss_flags = 0;
    if (ss.ss_sp == MAP_FAILED || sigaltstack(&ss, NULL) != 0) {
        r = -1;
    }
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_signal;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (i = 0; i < n; i++) {
        sigaction(sigs[i], &sa, NULL);
    }
    return r;
}

/* process_vm_readv: a system call that reports an unmapped page instead of faulting */
int PlatOs_ReadMemory(void *dst, uintptr_t src, size_t n) {
    struct iovec local = {dst, n}, remote = {(void *)src, n};
    return syscall(SYS_process_vm_readv, getpid_got(), &local, 1UL, &remote, 1UL, 0UL) == (long)n;
}

/* ---------------------------------------------------------------------------------------------- page tracking */

/* plat_state.c (F07): userfaultfd in asynchronous write-protect mode, where a write to a protected page is let through
   by the kernel itself (no signal, nothing on any stack, writes by the kernel and by other threads included), and
   PAGEMAP_SCAN, which lists the pages written since the last scan and protects them again in one call. */
#ifndef UFFD_USER_MODE_ONLY
#define UFFD_USER_MODE_ONLY 1
#endif
#define TRK_FEATURES ((1ull << 12) | (1ull << 13) | (1ull << 15)) /* UFFD_FEATURE_WP_HUGETLBFS_SHMEM, _UNPOPULATED, _ASYNC */
#define TRK_UFFDIO_API 0xC018AA3Fu      /* _IOWR(0xAA, 0x3F, struct uffdio_api) */
#define TRK_UFFDIO_REGISTER 0xC020AA00u /* _IOWR(0xAA, 0x00, struct uffdio_register) */
#define TRK_PAGEMAP_SCAN 0xC0606610u    /* _IOWR('f', 16, struct pm_scan_arg) */
typedef struct { uint64_t api, features, ioctls; } TrkApi;
typedef struct { uint64_t start, len, mode, ioctls; } TrkReg;
typedef struct {
    uint64_t size, flags, start, end, walk_end, vec, vec_len, max_pages, category_inverted, category_mask,
        category_anyof_mask, return_mask;
} TrkScan;
typedef struct { uint64_t start, end, categories; } TrkRange;

static int sUffd = -1, sPagemap = -1;
static TrkRange sVec[256];

const char *PlatOs_DirtyOpen(void) {
    TrkApi api;
    sUffd = (int)syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
    if (sUffd < 0) {
        return "no userfaultfd";
    }
    memset(&api, 0, sizeof api);
    api.api = 0xAA; /* UFFD_API */
    api.features = TRK_FEATURES;
    sPagemap = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    if (ioctl(sUffd, TRK_UFFDIO_API, &api) != 0 || (api.features & TRK_FEATURES) != TRK_FEATURES || sPagemap < 0) {
        return "the kernel has no asynchronous write protection (Linux 6.7)";
    }
    return NULL;
}

void PlatOs_DirtyClose(void) {
    if (sUffd >= 0) {
        close(sUffd); /* (unregisters every range: the pages are writable again) */
    }
    if (sPagemap >= 0) {
        close(sPagemap);
    }
    sUffd = sPagemap = -1;
}

int PlatOs_DirtyAdd(uintptr_t a, size_t n) {
    TrkReg r;
    memset(&r, 0, sizeof r);
    r.start = a;
    r.len = n;
    r.mode = 2; /* UFFDIO_REGISTER_MODE_WP */
    return ioctl(sUffd, TRK_UFFDIO_REGISTER, &r);
}

int PlatOs_DirtyScan(uintptr_t at_, uintptr_t stop_, void (*fn)(void *ctx, uint64_t start, uint64_t end), void *ctx) {
    uint64_t at = at_, stop = stop_;
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
            fn(ctx, sVec[i].start, sVec[i].end);
        }
        if (a.walk_end <= at) {
            return -1;
        }
        at = a.walk_end;
    }
    return 0;
}

void PlatOs_DirtyNoHuge(void *a, size_t n) { madvise(a, n, MADV_NOHUGEPAGE); }

void PlatOs_KernelWrite(void *p) { syscall(SYS_clock_gettime, 1, p); }

/* ---------------------------------------------------------------------------------------------- the program's image */

/* plat_state.c / ee_diff.c: /proc/self/exe, ELF32, or ELF64 for the x86-64 build (TTT_CC=clang64): the same fields at
   other offsets and widths. Every address and offset used here is below 4 GB, so the low word of a 64-bit field is
   the value (little-endian). */
static uint8_t *sElf;
static int sElfState; /* 0 not read, 1 read, -1 unreadable */

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }

static int self_read(void) {
    FILE *f;
    long n;
    if (sElfState != 0) {
        return sElfState;
    }
    sElfState = -1;
    f = fopen("/proc/self/exe", "rb");
    if (f == NULL) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    sElf = malloc((size_t)n);
    if (sElf == NULL || fread(sElf, 1, (size_t)n, f) != (size_t)n || memcmp(sElf, "\177ELF", 4) != 0 ||
        sElf[5] != 1 || (sElf[4] != 1 && sElf[4] != 2)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    sElfState = 1;
    return 1;
}

int PlatOs_SelfImage(void (*seg)(void *ctx, uint32_t va, uint32_t size),
                     void (*sym)(void *ctx, const char *name, uint32_t value, int type), void *ctx) {
    uint32_t phoff, shoff, k;
    uint16_t phnum, phentsize, shnum, shentsize;
    int e64;

    if (self_read() < 0) {
        return -1;
    }
    e64 = sElf[4] == 2;
    phoff = rd32(sElf + (e64 ? 32 : 28));
    phentsize = rd16(sElf + (e64 ? 54 : 42));
    phnum = rd16(sElf + (e64 ? 56 : 44));
    shoff = rd32(sElf + (e64 ? 40 : 32));
    shentsize = rd16(sElf + (e64 ? 58 : 46));
    shnum = rd16(sElf + (e64 ? 60 : 48));
    for (k = 0; k < phnum && seg != NULL; k++) { /* PT_LOAD segments */
        const uint8_t *ph = sElf + phoff + k * phentsize;
        if (rd32(ph) == 1) {
            seg(ctx, rd32(ph + (e64 ? 16 : 8)), rd32(ph + (e64 ? 40 : 20)));
        }
    }
    for (k = 0; k < shnum && sym != NULL; k++) {
        const uint8_t *sh = sElf + shoff + k * shentsize;
        if (rd32(sh + 4) == 2) { /* SHT_SYMTAB */
            uint32_t link = rd32(sh + (e64 ? 40 : 24)), symsize = e64 ? 24 : 16;
            const char *str = (const char *)sElf + rd32(sElf + shoff + link * shentsize + (e64 ? 24 : 16));
            uint32_t off = rd32(sh + (e64 ? 24 : 16)), size = rd32(sh + (e64 ? 32 : 20)), j;
            for (j = 0; j + symsize <= size; j += symsize) {
                const uint8_t *s = sElf + off + j;
                sym(ctx, str + rd32(s), rd32(s + (e64 ? 8 : 4)), s[e64 ? 4 : 12] & 15);
            }
        }
    }
    return 0;
}
#else
typedef int plat_os_posix_unused; /* (Windows: plat_os_win32.c) */
#endif
