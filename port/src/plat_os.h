/*
 * The operating system under the port (T42 b6, docs/port/windows.md): every call the port makes that is not plain C
 * or one of the POSIX interfaces Windows' mingw-w64 also has goes through here.
 *
 * Linux (POSIX): the small calls are inline functions below, so each stays in the file that makes it, compiled with
 * that file's flags: a call that went through the PLT still does and one that went through the GOT (port_host.h
 * PORT_NOPLT, toolchain.py NOPLT_FILES) still does, and the 32-bit program's PLT stays as it was. The larger pieces
 * (fixed-address memory, the crash handlers, page tracking, the program's own image, the command line, child
 * processes) are in plat_os_posix.c. Windows: plat_os_win32.c, a skeleton for T42 b7 (mingw-w64, x86-64) that names
 * the Win32 equivalent of each.
 *
 * Left to the C library on both (mingw-w64 has them with the same meaning): stdio, getenv, getpid, unlink, rmdir,
 * open / read / write / close with PLAT_O_BINARY; threads and clocks are POSIX threads (winpthreads on Windows:
 * pthread_*, clock_gettime with CLOCK_MONOTONIC, CLOCK_REALTIME and CLOCK_THREAD_CPUTIME_ID, nanosleep); sockets are
 * BSD sockets (Winsock 2 on Windows, after PlatOs_NetInit).
 */
#ifndef PLAT_OS_H
#define PLAT_OS_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "port_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------------------------- shared declarations */

typedef struct {
    int dir, reg;        /* a folder; a regular file */
    long long size;      /* bytes */
    long long mtime;     /* seconds since 1970 */
} PlatOsStat;

/* What identifies a file's content cheaply (plat_data.c's check of TEKKEN.BIN): its device, file number, size and
   modification time. */
typedef struct {
    unsigned dev_major, dev_minor;
    unsigned long long ino, size;
    long long mtime_s;
    unsigned mtime_ns;
} PlatOsFileKey;

/* A fault the crash handler reports (plat_crash.c): sig is a POSIX signal number (Windows: the one its exception
   code stands for), code > 0 a fault at addr, code <= 0 a signal raised (no address). The registers of the thread
   that failed: x86-64's in full (r8-r15 too), i386's in the low halves (r unused). */
typedef struct {
    int sig, code;
    uintptr_t addr;
    uintptr_t pc, sp, fp, flags, ax, bx, cx, dx, si, di;
    uintptr_t r[8]; /* r8-r15 */
} PlatOsFault;
typedef void (*PlatOsFaultFn)(const PlatOsFault *f);

/* plat_os_posix.c / plat_os_win32.c */
int PlatOs_HasArg(const char *arg);        /* the command line holds `arg` after the program's name (before main) */
int PlatOs_OnMainThread(void);             /* the calling thread is the process's first (signal-safe) */
int PlatOs_FileKey(const char *path, PlatOsFileKey *k);       /* 0, -1 */
int PlatOs_RunWait(char *const argv[]);    /* runs argv[0] (found on PATH), waits: 1 when it exited with 0 */

/* Fixed-address memory (plat_mem.c). A memory object of `size` bytes (a handle, -1 when none); its views at fixed
   addresses (0: mapped; `replace`: over whatever the program has there, else refused when anything is); anonymous
   memory at a fixed address (refused when anything is there); memory anywhere (`rw` 0: no access; NULL: none). */
int PlatOs_MemCreate(const char *name, size_t size);
int PlatOs_MapView(int mem, void *addr, size_t off, size_t size, int replace);
int PlatOs_MapFixed(void *addr, size_t size);
void *PlatOs_MapPages(size_t size, int rw);
/* 1 when a mapping of the process meets [lo, hi) outside [keep_lo, keep_hi), its description (text ending in a
   newline) in what[n]; 0 otherwise or when the map cannot be read. */
int PlatOs_RangeTaken(uintptr_t lo, uintptr_t hi, uintptr_t keep_lo, uintptr_t keep_hi, char *what, size_t n);

/* Crash handling (plat_crash.c). Installs fn for the n signals sigs[] on an alternate stack of alt_stack bytes, each
   handler reset to the default at its first call (the default action follows: a fault happens again on return, a
   raised signal is raised again); fn runs with only async-signal-safe calls allowed. 0, or -1 when there is no
   alternate stack (the handlers are installed all the same). */
int PlatOs_CrashHooks(const int *sigs, int n, PlatOsFaultFn fn, size_t alt_stack);
/* Copies n bytes at address src of this process into dst without faulting: 1 when all were readable. */
int PlatOs_ReadMemory(void *dst, uintptr_t src, size_t n);

/* Page tracking for the saves of rollback (plat_state.c): write protection that lets every write through and
   remembers the pages written. Open: NULL when available, else why not. Add: tracks [a, a + n) (whole pages); 0. Scan:
   fn(ctx, start, end) for each run of pages of [at, stop) written since the last scan, which protects them again;
   0, or -1 when the scan failed. NoHuge: [a, a + n) in small pages (a scan sees 4 KB at a time). */
const char *PlatOs_DirtyOpen(void);
void PlatOs_DirtyClose(void);
int PlatOs_DirtyAdd(uintptr_t a, size_t n);
int PlatOs_DirtyScan(uintptr_t at, uintptr_t stop, void (*fn)(void *ctx, uint64_t start, uint64_t end), void *ctx);
void PlatOs_DirtyNoHuge(void *a, size_t n);
void PlatOs_KernelWrite(void *p); /* the self-test: 16 bytes at p written by the system, not by the program */

/* The program's own image (plat_state.c, ee_diff.c), read once and kept: seg(ctx, address, size) for each loaded
   segment, then sym(ctx, name, value, type) for each symbol (type: PLAT_SYM_*; the names stay valid). Either may be
   NULL. 0, or -1 when the image cannot be read. */
enum { PLAT_SYM_NOTYPE = 0, PLAT_SYM_OBJECT = 1, PLAT_SYM_FUNC = 2 };
int PlatOs_SelfImage(void (*seg)(void *ctx, uint32_t va, uint32_t size),
                     void (*sym)(void *ctx, const char *name, uint32_t value, int type), void *ctx);

#ifndef _WIN32
/* ---------------------------------------------------------------------------------------------- POSIX (Linux) */
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* the C library calls below that are not in the 32-bit program's PLT: through the GOT in every file */
PORT_NOPLT(mkdir);
PORT_NOPLT(access);
PORT_NOPLT(realpath);
PORT_NOPLT(stat);
PORT_NOPLT(opendir);
PORT_NOPLT(readdir);
PORT_NOPLT(closedir);
PORT_NOPLT(open);
PORT_NOPLT(close);
PORT_NOPLT(setenv);
PORT_NOPLT(unsetenv);
PORT_NOPLT(poll);
PORT_NOPLT(mkdtemp);

#define PLAT_PATH_MAX 4096 /* (PATH_MAX; <limits.h> would bring LINE_MAX, which plat_settings.c defines) */
#define PLAT_O_BINARY 0
#define PLAT_LIB_SDL3 "libSDL3.so.0"
#define PLAT_LIB_SPEECHD "libspeechd.so.2"

/* The program's own path (symbolic links resolved) into buf, without a terminating 0: its length, <= 0 unknown. */
static inline long PlatOs_ExePath(char *buf, size_t size) { return (long)readlink("/proc/self/exe", buf, size); }
static inline int PlatOs_MkDir(const char *path, int mode) { return mkdir(path, (mode_t)mode); }
/* `from` renamed to `to`, replacing it at once: the old file or the new one, never neither. 0. */
static inline int PlatOs_Replace(const char *from, const char *to) { return rename(from, to); }
/* f's data on the disk (after fflush); 0. */
static inline int PlatOs_SyncFile(FILE *f) { return fsync(fileno(f)); }
/* the folder's entries on the disk (a rename in it) */
static inline void PlatOs_SyncDir(const char *dir) {
    int h = open(dir, O_RDONLY | O_DIRECTORY);
    if (h >= 0) {
        fsync(h);
        close(h);
    }
}
static inline int PlatOs_Exists(const char *path) { return access(path, F_OK) == 0; }
/* path made absolute, symbolic links resolved, into buf[PLAT_PATH_MAX]: buf, or NULL */
static inline char *PlatOs_RealPath(const char *path, char *buf) { return realpath(path, buf); }
static inline int PlatOs_Stat(const char *path, PlatOsStat *st) {
    struct stat s;
    if (stat(path, &s) != 0) {
        return -1;
    }
    st->dir = S_ISDIR(s.st_mode);
    st->reg = S_ISREG(s.st_mode);
    st->size = (long long)s.st_size;
    st->mtime = (long long)s.st_mtime;
    return 0;
}

/* A folder's entries, "." and ".." included: the next name, NULL at the end. */
typedef DIR PlatOsDir;
static inline PlatOsDir *PlatOs_DirOpen(const char *path) { return opendir(path); }
static inline const char *PlatOs_DirNext(PlatOsDir *d) {
    struct dirent *e = readdir(d);
    return e != NULL ? e->d_name : NULL;
}
static inline void PlatOs_DirClose(PlatOsDir *d) { closedir(d); }

/* a new folder from tmpl, whose last six characters "XXXXXX" become a unique name: tmpl, or NULL */
static inline char *PlatOs_TempDir(char *tmpl) { return mkdtemp(tmpl); }
static inline int PlatOs_SetEnv(const char *name, const char *value, int replace) {
    return setenv(name, value, replace);
}
static inline int PlatOs_UnsetEnv(const char *name) { return unsetenv(name); }

/* Shared libraries: dlopen (resolved at once, symbols kept local). */
static inline void *PlatOs_LibOpen(const char *name) { return dlopen(name, RTLD_NOW | RTLD_LOCAL); }
static inline void *PlatOs_LibSym(void *lib, const char *name) { return dlsym(lib, name); }
static inline void PlatOs_LibClose(void *lib) { dlclose(lib); }
static inline const char *PlatOs_LibError(void) { return dlerror(); }

static inline long PlatOs_CpuCount(void) { return sysconf(_SC_NPROCESSORS_ONLN); }

/* UDP sockets (plat_net.c): BSD sockets. NetInit: 0 when sockets can be made. SockWait: up to ms milliseconds for s to
   have something to read. */
typedef int PlatOsSock;
#define PLAT_SOCK_NONE (-1)
#define PLAT_MSG_DONTWAIT MSG_DONTWAIT
static inline int PlatOs_NetInit(void) { return 0; }
static inline void PlatOs_SockWait(PlatOsSock s, int ms) {
    struct pollfd pf;
    pf.fd = s;
    pf.events = POLLIN;
    pf.revents = 0;
    poll(&pf, 1, ms);
}

#else
/* ---------------------------------------------------------------------------------------------- Windows (T42 b7) */
#include <direct.h>
#include <io.h>
#include <process.h>
#include <signal.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef SIGBUS
#define SIGBUS 10 /* (not raised on Windows; the crash report's table and its test name it) */
#endif
#define PLAT_PATH_MAX 1024
#define PLAT_O_BINARY O_BINARY
#define PLAT_LIB_SDL3 "SDL3.dll"
#define PLAT_LIB_SPEECHD NULL /* (no libspeechd; speech would be SAPI) */
#ifndef F_OK
#define F_OK 0
#endif

long PlatOs_ExePath(char *buf, size_t size);
int PlatOs_MkDir(const char *path, int mode);
int PlatOs_Replace(const char *from, const char *to);
int PlatOs_SyncFile(FILE *f);
void PlatOs_SyncDir(const char *dir);
int PlatOs_Exists(const char *path);
char *PlatOs_RealPath(const char *path, char *buf);
int PlatOs_Stat(const char *path, PlatOsStat *st);
typedef struct PlatOsDir PlatOsDir;
PlatOsDir *PlatOs_DirOpen(const char *path);
const char *PlatOs_DirNext(PlatOsDir *d);
void PlatOs_DirClose(PlatOsDir *d);
char *PlatOs_TempDir(char *tmpl);
int PlatOs_SetEnv(const char *name, const char *value, int replace);
int PlatOs_UnsetEnv(const char *name);
void *PlatOs_LibOpen(const char *name);
void *PlatOs_LibSym(void *lib, const char *name);
void PlatOs_LibClose(void *lib);
const char *PlatOs_LibError(void);
long PlatOs_CpuCount(void);
typedef SOCKET PlatOsSock;
#define PLAT_SOCK_NONE INVALID_SOCKET
#define PLAT_MSG_DONTWAIT 0 /* (the socket is made non-blocking instead: ioctlsocket FIONBIO) */
int PlatOs_NetInit(void);
void PlatOs_SockWait(PlatOsSock s, int ms);
#endif

#ifdef __cplusplus
}
#endif
#endif
