/*
 * The operating system under the port, Windows side (T42 b6, completed in T42 b7: mingw-w64, x86-64, TTT_CC=win64;
 * plat_os.h, docs/port/windows.md). On Linux this file is empty. Each function names the Linux call it stands for
 * (plat_os.h, plat_os_posix.c) and the Win32 one that replaces it. Built and linked by a cross compiler; not run on
 * Windows yet (docs/port/windows.md lists what is checked).
 *
 * Paths: the port builds them with '/', which every Win32 file call accepts; PlatOs_ExePath turns the module's '\' into
 * '/' so that the callers' strrchr(.., '/') keeps working. Names are UTF-8 in the port and UTF-16 for Win32 (the W
 * calls, through utf16()).
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <mswsock.h>
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
/* unsetenv -> _putenv_s(name, ""), which removes the variable (getenv gives NULL) from the C library's copy and from
   the process's environment that a child inherits. (An empty value cannot be set this way; the port sets none.) */
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

/* argv as one Windows command line (UTF-16) into out[n]: each argument quoted the way CommandLineToArgvW and the C
   library read it back (backslashes doubled before a quote and at the end, quotes escaped). 0 when it does not fit. */
static int command_line(char *const argv[], wchar_t *out, size_t n) {
    size_t at = 0;
    int i;
    for (i = 0; argv[i] != NULL; i++) {
        wchar_t w[2048];
        const wchar_t *s;
        if (!utf16(argv[i], w, 2048) || at + 3 >= n) {
            return 0;
        }
        if (i > 0) {
            out[at++] = L' ';
        }
        out[at++] = L'"';
        for (s = w;; s++) {
            size_t bs = 0, k;
            while (*s == L'\\') {
                s++, bs++;
            }
            if (*s == 0 || *s == L'"') { /* backslashes before the closing quote or a quote: doubled */
                bs = bs * 2 + (*s == L'"');
            }
            if (at + bs + 3 >= n) {
                return 0;
            }
            for (k = 0; k < bs; k++) {
                out[at++] = L'\\';
            }
            if (*s == 0) {
                break;
            }
            out[at++] = *s;
        }
        out[at++] = L'"';
    }
    out[at] = 0;
    return 1;
}

/* A process from a command line (PATH searched for the program when app is NULL), sharing this one's standard
   handles; suspended when asked. 0, or -1. */
static int spawn(const wchar_t *app, wchar_t *cmd, DWORD flags, LPPROC_THREAD_ATTRIBUTE_LIST attr,
                 PROCESS_INFORMATION *pi) {
    STARTUPINFOEXW si;
    memset(&si, 0, sizeof si);
    si.StartupInfo.cb = attr != NULL ? sizeof si : sizeof si.StartupInfo;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.lpAttributeList = attr;
    fflush(NULL);
    return CreateProcessW(app, cmd, NULL, NULL, TRUE, flags | (attr != NULL ? EXTENDED_STARTUPINFO_PRESENT : 0), NULL,
                          NULL, &si.StartupInfo, pi)
               ? 0
               : -1;
}

/* posix_spawnp + waitpid -> CreateProcessW (the first argument found on PATH) + WaitForSingleObject +
   GetExitCodeProcess. (plat_speech.c's spd-say has no Windows counterpart, PLAT_LIB_SPEECHD is NULL there: speech
   would be SAPI's ISpVoice.) */
int PlatOs_RunWait(char *const argv[]) {
    static wchar_t cmd[32768];
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    if (argv == NULL || argv[0] == NULL || !command_line(argv, cmd, sizeof cmd / sizeof cmd[0]) ||
        spawn(NULL, cmd, 0, NULL, &pi) != 0) {
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

/* /dev/urandom -> BCryptGenRandom with the system's generator (links bcrypt) */
int PlatOs_Random(void *buf, size_t n) {
    return BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 ? 0 : -1;
}

/* execv("/proc/self/exe") -> CreateProcessW of the module's own path with argv quoted into one command line
   (command_line; _wexecv would join the arguments unquoted), then this process ends: Windows has no exec in place, so
   a console sees it end while the new one goes on in the same console. Returns only when the new process could not be
   made. */
void PlatOs_Restart(char *const argv[]) {
    static wchar_t self[MAX_PATH * 2], cmd[32768];
    PROCESS_INFORMATION pi;
    DWORD k = GetModuleFileNameW(NULL, self, MAX_PATH * 2);
    if (k == 0 || k >= MAX_PATH * 2 || argv == NULL || argv[0] == NULL ||
        !command_line(argv, cmd, sizeof cmd / sizeof cmd[0]) || spawn(self, cmd, 0, NULL, &pi) != 0) {
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ExitProcess(0);
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

/* The PS2's fixed ranges plat_mem.c maps (64 KB granules, Windows' allocation granularity): EE RAM above the first
   64 KB (which Windows never maps), the hardware registers, VU memory, the GS registers, the two views of EE RAM and
   the scratchpad. */
static const uintptr_t kFixed[][2] = {
    {0x00010000u, 0x02000000u}, {0x10000000u, 0x10010000u}, {0x11000000u, 0x11010000u}, {0x12000000u, 0x12010000u},
    {0x20000000u, 0x22000000u}, {0x30000000u, 0x32000000u}, {0x70000000u, 0x70010000u},
};
#define NFIXED (int)(sizeof kFixed / sizeof kFixed[0])
#define RELAUNCH_ENV L"TTT_WIN_FIXED_RELAUNCH"
#ifndef PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY
#define PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY 0x00020007
#endif
#ifndef PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON
#define PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON (0x00000001ull << 16)
#endif
#ifndef PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON
#define PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON (0x00000001ull << 20)
#endif

/* Whether m is a reservation the parent made for this process (relaunch): exactly one of the fixed ranges, reserved
   with no access. It counts as free (PlatOs_RangeTaken) and is released just before its range is mapped
   (release_ours), so nothing else takes the range in between. */
static int is_ours(const MEMORY_BASIC_INFORMATION *m) {
    int i;
    if (m->State != MEM_RESERVE || m->Type != MEM_PRIVATE || m->AllocationProtect != PAGE_NOACCESS ||
        m->BaseAddress != m->AllocationBase) {
        return 0;
    }
    for (i = 0; i < NFIXED; i++) {
        if ((uintptr_t)m->BaseAddress == kFixed[i][0] && (uintptr_t)m->BaseAddress + m->RegionSize == kFixed[i][1]) {
            return 1;
        }
    }
    return 0;
}
static void release_ours(void *addr) {
    MEMORY_BASIC_INFORMATION m;
    if (VirtualQuery(addr, &m, sizeof m) != 0 && is_ours(&m)) {
        VirtualFree(addr, 0, MEM_RELEASE);
    }
}

/* The program again, in a process made with bottom-up address randomisation (high entropy) forced on: the system's
   own allocations of a new process (its first thread's stack, the heap, the code page tables) then start at a random
   place in a terabyte instead of the bottom of the address space, where EE RAM goes; the program's image keeps its
   fixed address (it has no relocations and is not marked relocatable). The process is made suspended, the fixed
   ranges still free in it are reserved for it (is_ours: released as each is mapped), and this one waits for it and
   ends with its exit code. Returns only when it could not be made. */
static void relaunch(void) {
    static wchar_t self[MAX_PATH * 2];
    DWORD64 policy = PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON |
                     PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON;
    LPPROC_THREAD_ATTRIBUTE_LIST attr = NULL;
    SIZE_T sz = 0;
    PROCESS_INFORMATION pi;
    DWORD k = GetModuleFileNameW(NULL, self, MAX_PATH * 2), code = 2;
    wchar_t *cmd = _wcsdup(GetCommandLineW());
    int i;
    if (k == 0 || k >= MAX_PATH * 2 || cmd == NULL) {
        return;
    }
    SetEnvironmentVariableW(RELAUNCH_ENV, L"1");
    InitializeProcThreadAttributeList(NULL, 1, 0, &sz);
    attr = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(sz);
    if (attr == NULL || !InitializeProcThreadAttributeList(attr, 1, 0, &sz) ||
        !UpdateProcThreadAttribute(attr, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &policy, sizeof policy, NULL,
                                   NULL)) {
        free(attr);
        attr = NULL;
    }
    if (spawn(self, cmd, CREATE_SUSPENDED, attr, &pi) != 0 &&
        (attr == NULL || spawn(self, cmd, CREATE_SUSPENDED, NULL, &pi) != 0)) { /* (a system without the policy) */
        SetEnvironmentVariableW(RELAUNCH_ENV, NULL);
        return;
    }
    for (i = 0; i < NFIXED; i++) {
        VirtualAllocEx(pi.hProcess, (void *)kFixed[i][0], kFixed[i][1] - kFixed[i][0], MEM_RESERVE, PAGE_NOACCESS);
    }
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    ExitProcess(code);
}

/* Windows cannot map a view over memory in use (Linux's MAP_FIXED over EE RAM, `replace` below), and a process starts
   with its first stack, its heap and the code page tables at the bottom of its address space, inside EE RAM. So,
   before the first map, the fixed ranges are checked (a reservation the parent made counts as free); when anything
   else is there, the program runs again in a process that keeps the bottom free (relaunch, once: the variable
   RELAUNCH_ENV marks the second; TTT_NO_RELAUNCH=1 never). If even that one finds a range taken, plat_mem.c's
   check or map fails with its message. */
void PlatOs_MemPrepare(void) {
    static int done;
    char what[256];
    int i, taken = 0, again = GetEnvironmentVariableW(RELAUNCH_ENV, NULL, 0) != 0;
    if (done++) {
        return;
    }
    for (i = 0; i < NFIXED; i++) {
        taken |= PlatOs_RangeTaken(kFixed[i][0], kFixed[i][1], 0, 0, what, sizeof what);
    }
    if (again) {
        SetEnvironmentVariableW(RELAUNCH_ENV, NULL); /* (not passed on to what this program starts) */
    }
    if (taken && !again && getenv("TTT_NO_RELAUNCH") == NULL) {
        relaunch();
    }
}

/* mmap(MAP_SHARED | MAP_FIXED[_NOREPLACE]) -> MapViewOfFileEx at the address, which fails when anything is there:
   `replace` (Linux maps EE RAM over whatever the program had in the range) is honoured by PlatOs_MemPrepare having
   made the ranges free beforehand. The address and the offset are multiples of 64 KB. */
int PlatOs_MapView(int mem, void *addr, size_t off, size_t size, int replace) {
    void *p;
    (void)replace;
    if (mem < 0 || mem >= 4 || sSections[mem] == NULL) {
        return -1;
    }
    release_ours(addr);
    p = MapViewOfFileEx(sSections[mem], FILE_MAP_ALL_ACCESS, (DWORD)((unsigned long long)off >> 32), (DWORD)off, size,
                        addr);
    return p == addr ? 0 : -1;
}

/* mmap(MAP_ANONYMOUS | MAP_FIXED_NOREPLACE) -> VirtualAlloc(addr, MEM_RESERVE | MEM_COMMIT) (fails when taken; the
   address must be 64 KB aligned: the allocation granularity) */
int PlatOs_MapFixed(void *addr, size_t size) {
    release_ours(addr);
    return VirtualAlloc(addr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == addr ? 0 : -1;
}

void *PlatOs_MapPages(size_t size, int rw) {
    return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, rw ? PAGE_READWRITE : PAGE_NOACCESS);
}

/* /proc/self/maps -> VirtualQuery over [lo, hi): any region not MEM_FREE outside [keep_lo, keep_hi), but for a fixed
   range's reservation of ours (is_ours) */
int PlatOs_RangeTaken(uintptr_t lo, uintptr_t hi, uintptr_t keep_lo, uintptr_t keep_hi, char *what, size_t n) {
    uintptr_t a = lo;
    while (a < hi) {
        MEMORY_BASIC_INFORMATION m;
        uintptr_t b;
        if (VirtualQuery((void *)a, &m, sizeof m) == 0) {
            return 0;
        }
        b = (uintptr_t)m.BaseAddress + m.RegionSize;
        if (m.State != MEM_FREE && ((uintptr_t)m.BaseAddress < keep_lo || b > keep_hi) && !is_ours(&m)) {
            snprintf(what, n, "%p-%p state 0x%lx type 0x%lx\n", m.BaseAddress, (void *)b, (unsigned long)m.State,
                     (unsigned long)m.Type);
            return 1;
        }
        a = b;
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------------- crash handling */

/* sigaction + sigaltstack -> AddVectoredExceptionHandler. No alternate stack on Windows: the handler runs on the
   failing thread's stack (EE RAM for the game's thread), and after a stack overflow (EXCEPTION_STACK_OVERFLOW) on the
   reserve SetThreadStackGuarantee(alt_stack) keeps (the calling thread's). The exception code becomes the POSIX signal
   the report names; SA_RESETHAND: the handler reports once and returns EXCEPTION_CONTINUE_SEARCH (the default
   follows: the process ends). A fault whose stack pointer itself is unusable (TTT_CRASH_TEST=stack) cannot be
   reported: Windows ends the process before any handler runs.
   Raised signals: abort() raises SIGABRT, which the C library delivers to signal()'s handler (on_raised), reset to the
   default before it runs; abort then ends the process (exit code 3). The C library has no SIGBUS (raise(SIGBUS) fails
   and returns), and SIGSEGV / SIGILL / SIGFPE raised by raise() are not hooked: mingw-w64's start-up code would call
   the same handlers for faults the vectored handler has reported already. */
#define MAX_SIGS 16
static PlatOsFaultFn sFaultFn;
static int sSigs[MAX_SIGS];
static int sSigN;
static volatile LONG sFired;

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

/* Whether pc lies in the program's own image (its code: the game's and the port's). */
static int in_image(uintptr_t pc) {
    const uint8_t *base = (const uint8_t *)GetModuleHandleW(NULL);
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    return pc - (uintptr_t)base < nt->OptionalHeader.SizeOfImage;
}

static LONG report(EXCEPTION_POINTERS *e) {
    const CONTEXT *c = e->ContextRecord;
    PlatOsFault f;
    int sig = win_sig(e->ExceptionRecord->ExceptionCode), i, ours = 0;
    for (i = 0; i < sSigN; i++) {
        ours |= sSigs[i] == sig;
    }
    if (!ours || InterlockedExchange(&sFired, 1) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
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

/* First chance, before any frame's handler: a fault in the program's own code. That is the game's thread on the EE
   stack too, where no frame-based handler would be found (Windows unwinds only through stacks it knows). A fault in a
   system library is left to its own handlers first (a graphics driver may handle one it expects) and reported by
   on_unhandled when nobody does. */
static LONG CALLBACK on_exception(EXCEPTION_POINTERS *e) {
    return in_image((uintptr_t)e->ExceptionRecord->ExceptionAddress) ? report(e) : EXCEPTION_CONTINUE_SEARCH;
}
static LPTOP_LEVEL_EXCEPTION_FILTER sPrevFilter;
static LONG WINAPI on_unhandled(EXCEPTION_POINTERS *e) {
    report(e);
    return sPrevFilter != NULL ? sPrevFilter(e) : EXCEPTION_CONTINUE_SEARCH;
}

/* signal()'s handler of a raised signal: the registers are those of this handler (the raise point's are not kept),
   no address. */
static void on_raised(int sig) {
    CONTEXT c;
    PlatOsFault f;
    if (InterlockedExchange(&sFired, 1) != 0) {
        return;
    }
    RtlCaptureContext(&c);
    memset(&f, 0, sizeof f);
    f.sig = sig;
    f.code = 0;
    f.pc = (uintptr_t)c.Rip, f.sp = (uintptr_t)c.Rsp, f.fp = (uintptr_t)c.Rbp, f.flags = c.EFlags;
    f.ax = c.Rax, f.bx = c.Rbx, f.cx = c.Rcx, f.dx = c.Rdx, f.si = c.Rsi, f.di = c.Rdi;
    f.r[0] = c.R8, f.r[1] = c.R9, f.r[2] = c.R10, f.r[3] = c.R11;
    f.r[4] = c.R12, f.r[5] = c.R13, f.r[6] = c.R14, f.r[7] = c.R15;
    sFaultFn(&f);
}

int PlatOs_CrashHooks(const int *sigs, int n, PlatOsFaultFn fn, size_t alt_stack) {
    ULONG guarantee = (ULONG)alt_stack;
    int i;
    sFaultFn = fn;
    sSigN = n < MAX_SIGS ? n : MAX_SIGS;
    for (i = 0; i < sSigN; i++) { /* (a copy: the caller's array may be on its stack) */
        sSigs[i] = sigs[i];
        if (sigs[i] == SIGABRT) {
            signal(SIGABRT, on_raised);
        }
    }
    AddVectoredExceptionHandler(1, on_exception);
    sPrevFilter = SetUnhandledExceptionFilter(on_unhandled);
    return SetThreadStackGuarantee(&guarantee) ? 0 : -1;
}

/* process_vm_readv -> ReadProcessMemory(GetCurrentProcess()), which fails instead of faulting */
int PlatOs_ReadMemory(void *dst, uintptr_t src, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (const void *)src, dst, n, &got) && got == n;
}

/* ---------------------------------------------------------------------------------------------- page tracking */

/* userfaultfd + PAGEMAP_SCAN -> none: GetWriteWatch / ResetWriteWatch work only on memory allocated with
   VirtualAlloc(MEM_WRITE_WATCH), and the regions a state holds are file mapping views (EE RAM, aliased at 0x20000000
   and 0x30000000) and the program's own bss, which cannot be allocated that way. So tracking is off and each save is
   a whole copy (plat_state.c's fallback, as on a Linux older than 6.7): the same states, rollback slower. */
const char *PlatOs_DirtyOpen(void) { return "no write tracking on Windows (whole copies)"; }
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

/* /proc/self/exe as ELF -> the PE image in memory (GetModuleHandleW(NULL)): its sections as the segments; and the COFF
   symbol table GNU ld keeps in the file unless it is stripped (FileHeader.PointerToSymbolTable, 18-byte records, the
   string table after them), read once from the program's file and kept (the names stay valid). A symbol's value is
   its offset in its section (1-based number; the image base and the section's address are added) or, for section
   -1, an absolute address (the game's data names in EE RAM, link.py). The crash report's names come from ttt.exe.syms,
   which link.py writes as on Linux. */
static uint8_t *sPe;
static size_t sPeSize;
static char *sPeNames; /* the short (8-byte, unterminated) names, each copied with a terminator */
static int sPeState;   /* 0 not read, 1 read, -1 unreadable */

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t rd16(const uint8_t *p) { return p[0] | p[1] << 8; }

static int pe_read(void) {
    wchar_t w[MAX_PATH * 2];
    FILE *f;
    long n;
    DWORD k;
    if (sPeState != 0) {
        return sPeState;
    }
    sPeState = -1;
    k = GetModuleFileNameW(NULL, w, MAX_PATH * 2);
    if (k == 0 || k >= MAX_PATH * 2 || (f = _wfopen(w, L"rb")) == NULL) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    sPe = n > 0x40 ? malloc((size_t)n) : NULL;
    if (sPe == NULL || fread(sPe, 1, (size_t)n, f) != (size_t)n || sPe[0] != 'M' || sPe[1] != 'Z' ||
        rd32(sPe + 0x3C) + 24u > (uint32_t)n || memcmp(sPe + rd32(sPe + 0x3C), "PE\0\0", 4) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    sPeSize = (size_t)n;
    sPeState = 1;
    return 1;
}

int PlatOs_SelfImage(void (*seg)(void *ctx, uint32_t va, uint32_t size),
                     void (*sym)(void *ctx, const char *name, uint32_t value, int type), void *ctx) {
    const uint8_t *base = (const uint8_t *)GetModuleHandleW(NULL);
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)(base + ((const IMAGE_DOS_HEADER *)base)->e_lfanew);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    const uint8_t *fh, *tab;
    const char *str;
    uint32_t nsec, nsym, symoff, i;
    char *names;
    for (i = 0; i < nt->FileHeader.NumberOfSections && seg != NULL; i++) {
        if (!(s[i].Characteristics & IMAGE_SCN_MEM_DISCARDABLE)) { /* (not the debugging sections after the program) */
            seg(ctx, (uint32_t)(uintptr_t)(base + s[i].VirtualAddress), s[i].Misc.VirtualSize);
        }
    }
    if (sym == NULL) {
        return 0;
    }
    if (pe_read() < 0) {
        return -1;
    }
    fh = sPe + rd32(sPe + 0x3C) + 4; /* IMAGE_FILE_HEADER */
    nsec = rd16(fh + 2);
    symoff = rd32(fh + 8);
    nsym = rd32(fh + 12);
    if (symoff == 0 || nsym == 0 || symoff + (uint64_t)nsym * 18 + 4 > sPeSize) {
        return 0; /* stripped */
    }
    tab = sPe + symoff;
    str = (const char *)tab + nsym * 18;
    if (sPeNames == NULL && (sPeNames = calloc(nsym, 9)) == NULL) {
        return -1;
    }
    names = sPeNames;
    for (i = 0; i < nsym; i += 1 + tab[i * 18 + 17]) {
        const uint8_t *r = tab + i * 18;
        int16_t secn = (int16_t)rd16(r + 12);
        uint32_t v = rd32(r + 8), type = rd16(r + 14);
        const char *name;
        if (r[16] != IMAGE_SYM_CLASS_EXTERNAL && r[16] != IMAGE_SYM_CLASS_STATIC) {
            continue;
        }
        if (rd32(r) == 0) {
            name = str + rd32(r + 4);
        } else {
            memcpy(names + i * 9, r, 8);
            name = names + i * 9;
        }
        if (secn > 0 && (uint32_t)secn <= nsec) {
            v += (uint32_t)(uintptr_t)base + s[secn - 1].VirtualAddress;
        } else if (secn != IMAGE_SYM_ABSOLUTE) {
            continue; /* undefined or debugging */
        }
        sym(ctx, name, v, (type >> 4) == IMAGE_SYM_DTYPE_FUNCTION ? PLAT_SYM_FUNC : PLAT_SYM_NOTYPE);
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

/* socket -> socket, made non-blocking (ioctlsocket FIONBIO: Winsock has no MSG_DONTWAIT, PLAT_MSG_DONTWAIT is 0), and
   a datagram socket keeps receiving after an ICMP "port unreachable" (SIO_UDP_CONNRESET off; Windows would fail the
   next receive with WSAECONNRESET). */
PlatOsSock PlatOs_Socket(int family, int type) {
    SOCKET s = socket(family, type, 0);
    u_long on = 1;
    BOOL off = FALSE;
    DWORD got = 0;
    if (s == INVALID_SOCKET) {
        return s;
    }
    if (ioctlsocket(s, FIONBIO, &on) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    if (type == SOCK_DGRAM) {
        WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof off, NULL, 0, &got, NULL, NULL);
    }
    return s;
}

void PlatOs_SockClose(PlatOsSock s) { closesocket(s); } /* close -> closesocket */

void PlatOs_SockWait(PlatOsSock s, int ms) { /* poll -> WSAPoll */
    WSAPOLLFD pf;
    pf.fd = s;
    pf.events = POLLRDNORM;
    pf.revents = 0;
    WSAPoll(&pf, 1, ms);
}
/* ---------------------------------------------------------------------------------------------- the game's stack */

/* The thread's stack bounds (NT_TIB StackBase / StackLimit in its TEB) widened to cover every stack the game's thread
   uses from here on: the EE stack in EE RAM (0x01FFFF00 down), the service stacks in the program's bss (plat_state.c)
   and whatever it had. Windows' code compares the stack pointer with them: the system libraries' __chkstk walks
   down page by page from StackLimit to a new frame below it (from the host stack to EE RAM: through unmapped memory),
   and exception dispatch stops at a frame outside them. (mingw-w64's own ___chkstk_ms touches pages from the stack
   pointer and needs nothing.) Called once on the main thread just before it moves to the EE stack for good. */
void PlatOs_StackWide(void) {
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    if ((uintptr_t)tib->StackLimit > 0x00010000u) {
        tib->StackLimit = (PVOID)(uintptr_t)0x00010000u;
    }
    if ((uintptr_t)tib->StackBase < 0x100000000ull) {
        tib->StackBase = (PVOID)(uintptr_t)0x100000000ull;
    }
}
#else
typedef int plat_os_win32_unused; /* (Linux: plat_os_posix.c) */
#endif
