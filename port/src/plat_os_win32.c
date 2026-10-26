/*
 * The operating system under the port, Windows side (T42 b6 skeleton for T42 b7: mingw-w64, x86-64; plat_os.h,
 * docs/port/windows.md). Not built yet: on Linux this file is empty, and no mingw-w64 compiler has checked it (none was
 * installed when it was written). Each function names the Linux call it stands for (plat_os.h, plat_os_posix.c) and
 * the Win32 one that replaces it; the ones marked TODO need more than the call.
 *
 * Paths: the port builds them with '/', which every Win32 file call accepts; PlatOs_ExePath turns the module's '\' into
 * '/' so that the callers' strrchr(.., '/') keeps working. Names are UTF-8 in the port and UTF-16 for Win32 (the W
 * calls, through utf16()).
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "plat_os.h"

static int utf16(const char *s, wchar_t *out, int n) { return MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n) > 0; }
static int utf8(const wchar_t *s, char *out, int n) {
    return WideCharToMultiByte(CP_UTF8, 0, s, -1, out, n, NULL, NULL) > 0;
}

/* ---------------------------------------------------------------------------------------------- program, files */

/* /proc/self/exe -> GetModuleFileNameW(NULL); readlink's contract: the length, no terminator. */
long PlatOs_ExePath(char *buf, size_t size) {
    wchar_t w[MAX_PATH * 2];
    char u[MAX_PATH * 6];
    size_t n, i;
    DWORD k = GetModuleFileNameW(NULL, w, (DWORD)(sizeof w / sizeof w[0]));
    if (k == 0 || k >= sizeof w / sizeof w[0] || !utf8(w, u, (int)sizeof u)) {
        return -1;
    }
    n = strlen(u) < size ? strlen(u) : size;
    for (i = 0; i < n; i++) {
        buf[i] = u[i] == '\\' ? '/' : u[i];
    }
    return (long)n;
}

/* /proc/self/cmdline -> GetCommandLineW + CommandLineToArgvW (valid before main, as the constructor needs). */
int PlatOs_HasArg(const char *arg) {
    int argc, i, found = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc), want[256];
    if (argv == NULL || !utf16(arg, want, 256)) {
        return 0;
    }
    for (i = 1; i < argc && !found; i++) {
        found = wcscmp(argv[i], want) == 0;
    }
    LocalFree(argv);
    return found;
}

/* gettid == getpid -> the id of the thread the constructors ran on (the process's first) */
static DWORD sMainThread;
__attribute__((constructor)) static void main_thread(void) { sMainThread = GetCurrentThreadId(); }
int PlatOs_OnMainThread(void) { return GetCurrentThreadId() == sMainThread; }

int PlatOs_MkDir(const char *path, int mode) { /* mkdir(path, mode) -> CreateDirectoryW (no modes) */
    wchar_t w[1024];
    (void)mode;
    return utf16(path, w, 1024) && CreateDirectoryW(w, NULL) ? 0 : -1;
}

/* rename over an existing file -> MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH) (ReplaceFileW keeps attributes but
   fails when the target is missing) */
int PlatOs_Replace(const char *from, const char *to) {
    wchar_t a[1024], b[1024];
    return utf16(from, a, 1024) && utf16(to, b, 1024) &&
                   MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
               ? 0
               : -1;
}

int PlatOs_SyncFile(FILE *f) { return _commit(_fileno(f)); } /* fsync -> _commit (FlushFileBuffers) */
void PlatOs_SyncDir(const char *dir) { (void)dir; }          /* (NTFS: the rename is journaled; nothing to flush) */
int PlatOs_Exists(const char *path) { return _access(path, 0) == 0; }

char *PlatOs_RealPath(const char *path, char *buf) { /* realpath -> GetFullPathNameW (TODO: links: GetFinalPathName) */
    wchar_t w[1024], full[1024];
    char *p;
    if (!utf16(path, w, 1024) || GetFullPathNameW(w, 1024, full, NULL) == 0 || !utf8(full, buf, PLAT_PATH_MAX) ||
        GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
        return NULL;
    }
    for (p = buf; *p; p++) {
        *p = *p == '\\' ? '/' : *p;
    }
    return buf;
}

int PlatOs_Stat(const char *path, PlatOsStat *st) { /* stat -> _wstat64 */
    wchar_t w[1024];
    struct _stat64 s;
    if (!utf16(path, w, 1024) || _wstat64(w, &s) != 0) {
        return -1;
    }
    st->dir = (s.st_mode & _S_IFMT) == _S_IFDIR;
    st->reg = (s.st_mode & _S_IFMT) == _S_IFREG;
    st->size = (long long)s.st_size;
    st->mtime = (long long)s.st_mtime;
    return 0;
}

/* statx -> GetFileInformationByHandle: volume serial number, file index, size, last write time (100 ns from 1601) */
int PlatOs_FileKey(const char *path, PlatOsFileKey *k) {
    wchar_t w[1024];
    BY_HANDLE_FILE_INFORMATION i;
    HANDLE h;
    unsigned long long t;
    if (!utf16(path, w, 1024)) {
        return -1;
    }
    h = CreateFileW(w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }
    if (!GetFileInformationByHandle(h, &i)) {
        CloseHandle(h);
        return -1;
    }
    CloseHandle(h);
    t = (unsigned long long)i.ftLastWriteTime.dwHighDateTime << 32 | i.ftLastWriteTime.dwLowDateTime;
    k->dev_major = i.dwVolumeSerialNumber;
    k->dev_minor = 0;
    k->ino = (unsigned long long)i.nFileIndexHigh << 32 | i.nFileIndexLow;
    k->size = (unsigned long long)i.nFileSizeHigh << 32 | i.nFileSizeLow;
    k->mtime_s = (long long)(t / 10000000ull) - 11644473600ll;
    k->mtime_ns = (unsigned)(t % 10000000ull) * 100u;
    return 0;
}

/* opendir / readdir -> FindFirstFileW / FindNextFileW ("." and ".." are listed too) */
struct PlatOsDir {
    HANDLE h;
    int first;
    WIN32_FIND_DATAW d;
    char name[MAX_PATH * 4];
};
PlatOsDir *PlatOs_DirOpen(const char *path) {
    wchar_t w[1100];
    char p[1100];
    PlatOsDir *d = calloc(1, sizeof *d);
    snprintf(p, sizeof p, "%s/*", path);
    if (d == NULL || !utf16(p, w, 1100) || (d->h = FindFirstFileW(w, &d->d)) == INVALID_HANDLE_VALUE) {
        free(d);
        return NULL;
    }
    d->first = 1;
    return d;
}
const char *PlatOs_DirNext(PlatOsDir *d) {
    if (!d->first && !FindNextFileW(d->h, &d->d)) {
        return NULL;
    }
    d->first = 0;
    return utf8(d->d.cFileName, d->name, (int)sizeof d->name) ? d->name : "";
}
void PlatOs_DirClose(PlatOsDir *d) {
    FindClose(d->h);
    free(d);
}

char *PlatOs_TempDir(char *tmpl) { /* mkdtemp -> _mktemp_s + _mkdir */
    return _mktemp_s(tmpl, strlen(tmpl) + 1) == 0 && _mkdir(tmpl) == 0 ? tmpl : NULL;
}

int PlatOs_SetEnv(const char *name, const char *value, int replace) { /* setenv -> _putenv_s */
    if (!replace && getenv(name) != NULL) {
        return 0;
    }
    return _putenv_s(name, value) == 0 ? 0 : -1;
}
/* unsetenv -> _putenv_s(name, "") (TODO: getenv then gives NULL, as unsetenv; an empty value cannot be set this way:
   SetEnvironmentVariableW keeps the C library's copy apart) */
int PlatOs_UnsetEnv(const char *name) { return _putenv_s(name, "") == 0 ? 0 : -1; }

/* dlopen / dlsym / dlclose / dlerror -> LoadLibraryW / GetProcAddress / FreeLibrary / FormatMessage */
void *PlatOs_LibOpen(const char *name) {
    wchar_t w[512];
    return utf16(name, w, 512) ? (void *)LoadLibraryW(w) : NULL;
}
void *PlatOs_LibSym(void *lib, const char *name) { return (void *)GetProcAddress((HMODULE)lib, name); }
void PlatOs_LibClose(void *lib) { FreeLibrary((HMODULE)lib); }
const char *PlatOs_LibError(void) {
    static char msg[256];
    if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), 0, msg,
                       sizeof msg, NULL) == 0) {
        snprintf(msg, sizeof msg, "error %lu", (unsigned long)GetLastError());
    }
    return msg;
}

long PlatOs_CpuCount(void) { /* sysconf(_SC_NPROCESSORS_ONLN) -> GetSystemInfo */
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (long)si.dwNumberOfProcessors;
}

/* posix_spawnp + waitpid -> CreateProcessW + WaitForSingleObject + GetExitCodeProcess (TODO: quote the arguments into
   one command line; plat_speech.c's spd-say has no Windows counterpart: speech would be SAPI's ISpVoice) */
int PlatOs_RunWait(char *const argv[]) {
    (void)argv;
    return 0;
}

/* /dev/urandom -> BCryptGenRandom with the system's generator (links bcrypt) */
int PlatOs_Random(void *buf, size_t n) {
    return BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 ? 0 : -1;
}

/* execv("/proc/self/exe") -> _wexecv on the module's own path. Windows has no exec in place: the C library starts a new
   process and this one exits (a console sees it end). TODO: _wexecv joins the arguments with spaces, unquoted. */
void PlatOs_Restart(char *const argv[]) {
    static wchar_t self[MAX_PATH * 2], args[32][1024];
    const wchar_t *w[33];
    int i;
    DWORD k = GetModuleFileNameW(NULL, self, MAX_PATH * 2);
    if (k == 0 || k >= MAX_PATH * 2) {
        return;
    }
    for (i = 0; i < 32 && argv[i] != NULL; i++) {
        if (!utf16(argv[i], args[i], 1024)) {
            return;
        }
        w[i] = args[i];
    }
    w[i] = NULL;
    fflush(NULL);
    _wexecv(self, w);
}

/* ---------------------------------------------------------------------------------------------- memory */

/* memfd_create + ftruncate -> CreateFileMappingW(INVALID_HANDLE_VALUE, PAGE_READWRITE, size): a pagefile-backed
   section. The handle is returned as an int index into a small table (plat_mem.c keeps an int). */
static HANDLE sSections[4];
int PlatOs_MemCreate(const char *name, size_t size) {
    int i;
    (void)name;
    for (i = 0; i < 4 && sSections[i] != NULL; i++) {
    }
    if (i == 4 || (sSections[i] = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                                    (DWORD)((unsigned long long)size >> 32), (DWORD)size, NULL)) == NULL) {
        return -1;
    }
    return i;
}

/* mmap(MAP_SHARED | MAP_FIXED_NOREPLACE) -> MapViewOfFileEx at the address (fails when anything is there).
   TODO `replace` (MAP_FIXED over EE RAM): Windows cannot map over a reservation; the program must leave the range
   free instead (the PE image linked above 0x02000000, nothing in its bss there, the heap kept away with a
   VirtualAlloc(MEM_RESERVE) of the PS2's ranges as early as possible, released just before the views are made). */
int PlatOs_MapView(int mem, void *addr, size_t off, size_t size, int replace) {
    void *p;
    (void)replace;
    if (mem < 0 || mem >= 4 || sSections[mem] == NULL) {
        return -1;
    }
    p = MapViewOfFileEx(sSections[mem], FILE_MAP_ALL_ACCESS, (DWORD)((unsigned long long)off >> 32), (DWORD)off, size,
                        addr);
    return p == addr ? 0 : -1;
}

/* mmap(MAP_ANONYMOUS | MAP_FIXED_NOREPLACE) -> VirtualAlloc(addr, MEM_RESERVE | MEM_COMMIT) (fails when taken; the
   address must be 64 KB aligned: the allocation granularity) */
int PlatOs_MapFixed(void *addr, size_t size) {
    return VirtualAlloc(addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == addr ? 0 : -1;
}

void *PlatOs_MapPages(size_t size, int rw) {
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, rw ? PAGE_READWRITE : PAGE_NOACCESS);
}

/* /proc/self/maps -> VirtualQuery over [lo, hi): any region not MEM_FREE outside [keep_lo, keep_hi) */
int PlatOs_RangeTaken(uintptr_t lo, uintptr_t hi, uintptr_t keep_lo, uintptr_t keep_hi, char *what, size_t n) {
    uintptr_t a = lo;
    while (a < hi) {
        MEMORY_BASIC_INFORMATION m;
        uintptr_t b;
        if (VirtualQuery((void *)a, &m, sizeof m) == 0) {
            return 0;
        }
        b = (uintptr_t)m.BaseAddress + m.RegionSize;
        if (m.State != MEM_FREE && ((uintptr_t)m.BaseAddress < keep_lo || b > keep_hi)) {
            snprintf(what, n, "%p-%p state 0x%lx type 0x%lx\n", m.BaseAddress, (void *)b, (unsigned long)m.State,
                     (unsigned long)m.Type);
            return 1;
        }
        a = b;
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------- crash handling */

/* sigaction + sigaltstack -> AddVectoredExceptionHandler. No alternate stack on Windows: a stack overflow
   (EXCEPTION_STACK_OVERFLOW) still runs the handler on the guard page's reserve (SetThreadStackGuarantee(alt_stack)
   on each thread raises it). The exception code becomes the POSIX signal the report names; SA_RESETHAND: the handler
   reports once and returns EXCEPTION_CONTINUE_SEARCH (the default: the process ends). Raised signals (abort, raise):
   signal(SIGABRT, ...) TODO. */
static PlatOsFaultFn sFaultFn;
static const int *sSigs;
static int sSigN, sFired;

static int win_sig(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_GUARD_PAGE:
        return SIGSEGV;
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return SIGBUS;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
        return SIGILL;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
        return SIGFPE;
    default:
        return 0;
    }
}

static LONG CALLBACK on_exception(EXCEPTION_POINTERS *e) {
    const CONTEXT *c = e->ContextRecord;
    PlatOsFault f;
    int sig = win_sig(e->ExceptionRecord->ExceptionCode), i, ours = 0;
    for (i = 0; i < sSigN; i++) {
        ours |= sSigs[i] == sig;
    }
    if (!ours || sFired) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    sFired = 1;
    memset(&f, 0, sizeof f);
    f.sig = sig;
    f.code = 1;
    f.addr = e->ExceptionRecord->NumberParameters >= 2 ? (uintptr_t)e->ExceptionRecord->ExceptionInformation[1]
                                                       : (uintptr_t)e->ExceptionRecord->ExceptionAddress;
    f.pc = (uintptr_t)c->Rip, f.sp = (uintptr_t)c->Rsp, f.fp = (uintptr_t)c->Rbp, f.flags = c->EFlags;
    f.ax = c->Rax, f.bx = c->Rbx, f.cx = c->Rcx, f.dx = c->Rdx, f.si = c->Rsi, f.di = c->Rdi;
    f.r[0] = c->R8, f.r[1] = c->R9, f.r[2] = c->R10, f.r[3] = c->R11;
    f.r[4] = c->R12, f.r[5] = c->R13, f.r[6] = c->R14, f.r[7] = c->R15;
    sFaultFn(&f);
    return EXCEPTION_CONTINUE_SEARCH;
}

int PlatOs_CrashHooks(const int *sigs, int n, PlatOsFaultFn fn, size_t alt_stack) {
    ULONG guarantee = (ULONG)alt_stack;
    sFaultFn = fn;
    sSigs = sigs; /* (the caller's array: TODO copy it) */
    sSigN = n;
    AddVectoredExceptionHandler(1, on_exception);
    return SetThreadStackGuarantee(&guarantee) ? 0 : -1;
}

/* process_vm_readv -> ReadProcessMemory(GetCurrentProcess()), which fails instead of faulting */
int PlatOs_ReadMemory(void *dst, uintptr_t src, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)src, dst, n, &got) && got == n;
}

/* ---------------------------------------------------------------------------------------------- page tracking */

/* userfaultfd + PAGEMAP_SCAN -> GetWriteWatch / ResetWriteWatch, which only work on memory allocated with
   VirtualAlloc(MEM_WRITE_WATCH): not on file mapping views (EE RAM's three views) nor on the program's bss. TODO b7:
   allocate the tracked regions that way where possible; until then tracking is off and every save is a whole copy
   (plat_state.c's fallback, as on a Linux older than 6.7). */
const char *PlatOs_DirtyOpen(void) { return "no write tracking on Windows yet (GetWriteWatch: T42 b7)"; }
void PlatOs_DirtyClose(void) {}
int PlatOs_DirtyAdd(uintptr_t a, size_t n) {
    (void)a, (void)n;
    return -1;
}
int PlatOs_DirtyScan(uintptr_t at, uintptr_t stop, void (*fn)(void *ctx, uint64_t start, uint64_t end), void *ctx) {
    (void)at, (void)stop, (void)fn, (void)ctx;
    return -1;
}
void PlatOs_DirtyNoHuge(void *a, size_t n) { (void)a, (void)n; }
void PlatOs_KernelWrite(void *p) { /* clock_gettime by the kernel -> ReadFile into p would be the kernel's write */
    GetSystemTimeAsFileTime((FILETIME *)p);
}

/* ---------------------------------------------------------------------------------------------- the program's image */

/* /proc/self/exe as ELF -> the PE image in memory (GetModuleHandleW(NULL)): its sections as the segments, and the COFF
   symbol table mingw's ld keeps unless stripped (PointerToSymbolTable in the file: read the file through
   PlatOs_ExePath). TODO: the symbols; the crash report's names come from ttt.syms, which link.py writes either way. */
int PlatOs_SelfImage(void (*seg)(void *ctx, uint32_t va, uint32_t size),
                     void (*sym)(void *ctx, const char *name, uint32_t value, int type), void *ctx) {
    const uint8_t *base = (const uint8_t *)GetModuleHandleW(NULL);
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    unsigned i;
    (void)sym;
    for (i = 0; i < nt->FileHeader.NumberOfSections && seg != NULL; i++) {
        seg(ctx, (uint32_t)(uintptr_t)(base + s[i].VirtualAddress), s[i].Misc.VirtualSize);
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------- sockets */

int PlatOs_NetInit(void) { /* WSAStartup once */
    static int done;
    WSADATA w;
    if (!done) {
        done = WSAStartup(MAKEWORD(2, 2), &w) == 0 ? 1 : -1;
    }
    return done > 0 ? 0 : -1;
}

void PlatOs_SockClose(PlatOsSock s) { closesocket(s); } /* close -> closesocket */

void PlatOs_SockWait(PlatOsSock s, int ms) { /* poll -> WSAPoll */
    WSAPOLLFD pf;
    pf.fd = s;
    pf.events = POLLRDNORM;
    pf.revents = 0;
    WSAPoll(&pf, 1, ms);
}
#else
typedef int plat_os_win32_unused; /* (Linux: plat_os_posix.c) */
#endif
