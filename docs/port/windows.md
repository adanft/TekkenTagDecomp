# Windows: the x86-64 program (T42 b6, b7)

`TTT_CC=win64 python3 port/tools/build.py` cross-builds the Windows program on Linux: `build/port-win64/ttt.exe`, the
same x86-64 program as the Linux default (clang64: 4-byte game pointers, software float, image at 0x08000000) for
64-bit Windows. It compiles (all 134 objects, the Linux build's list) and links, and the static checks below pass.
**It has not run yet**: neither Windows nor Wine was available. Everything about its behaviour at run time is
therefore unverified; the list at the end says what to check first.

## Quick path

1. Tools on the Linux machine: clang, llc and libclang's Python bindings (as for the Linux build), plus mingw-w64
   (`x86_64-w64-mingw32-gcc`, `-binutils`, the CRT and winpthreads; Arch: `mingw-w64-gcc`).
2. `TTT_CC=win64 python3 port/tools/build.py` -> `build/port-win64/ttt.exe` (+ `ttt.exe.syms`, `ttt.exe.map`).
3. `python3 -m unittest tests/test_port_win64.py` (static checks; `TTT_SLOW_TESTS=1` builds first). With `wine` on
   PATH the same test also runs the self-tests and the idle run's reference hash.
4. The matchmaking server: `python3 port/tools/build_server.py --win64` -> `build/server-win64/ttt-server.exe`,
   `ttt-sign.exe` ([matchmaking.md](matchmaking.md)).

## What a player needs on Windows

| File | Where it comes from |
|---|---|
| `ttt.exe` | this build (console program: its messages go to the terminal that started it) |
| `SDL3.dll` | SDL 3's own Windows release (x64 build of `SDL3-<version>-win32-x64.zip`, github.com/libsdl-org/SDL releases), placed next to `ttt.exe`. The program loads it when it opens its window (`PlatOs_LibOpen`), so the build does not need it and nothing in the repository ships it |
| the game's data | from the player's own disc, as on Linux ([data.md](data.md)); `ttt.dat`, `saves/`, `textures/` in `%LOCALAPPDATA%\ttt`, the settings in `%APPDATA%\ttt\settings.ini` (`plat_paths.c`; `portable.txt` beside the program keeps everything there) |

Windows 10 1903 or later: the program's manifest makes UTF-8 its code page (folders with non-ASCII names), and the C
library is the Universal C Runtime (part of Windows 10 and 11).

## How the build differs from Linux

| Part | Windows (`toolchain.py`, `undefined.py`, `link.py`) |
|---|---|
| Compilers | clang / clang++ `--target=x86_64-w64-mingw32` for everything; the game's files go through the same `ptr32.py` -> `clang -emit-llvm` -> `irfix.py` -> `llc` pipeline (the target comes with the IR). `-D__CRT__NO_INLINE` (the headers' inline maths uses long double, which software float forbids), `-D_FILE_OFFSET_BITS=64` (`fseeko` on a disc image over 2 GB), `-fno-auto-import` (a variable is reached directly, not through a `.refptr` pointer) |
| Calling convention | Win64 throughout (rcx, rdx, r8, r9, 32 bytes of shadow space, rdi / rsi / xmm6-xmm15 callee-saved): the game's code and the port's are compiled for it alike. Software float passes floats in integer registers, so a variadic double reaches `vprintf` as written (a Win64 `va_list` is a pointer into memory): `plat_libc.c` uses the plain path there, not the SysV per-conversion formatting |
| Binutils | `x86_64-w64-mingw32-nm` / `objcopy` (COFF objects); `windres` for the manifest |
| Link | `x86_64-w64-mingw32-gcc`: `--image-base=0x08000000`, `--disable-dynamicbase`, `--disable-high-entropy-va`, `--disable-reloc-section` (the 4-byte addresses in the game's code and data are final), an 8 MB main stack, `--wrap` as on Linux; `-static` (winpthreads, libgcc), `ws2_32`, `bcrypt`, `shell32`. No heap guard page (no brk). `__executable_start` is defined as the code's start (`image.ld`), which ld's PE script lacks. The game's data names are absolute symbols in EE RAM, as on Linux |
| Imports (checked) | kernel32, user32 (Dear ImGui's clipboard), shell32, ws2_32, bcrypt and the Universal C Runtime (`api-ms-win-crt-*`). No winpthreads or libgcc DLL |
| `#undef __ptr32` | mingw-w64's `_mingw.h` defines `__ptr32` away; `port_addr.h` undefines it so that `PORT_GPTR` stays a 4-byte pointer, and asserts that it is |
| `windows.h` names | `plat_os.h` takes only its core (`NOUSER`, `NOGDI`, `NOMINMAX`...) and drops `IN` / `OUT` / `OPTIONAL`; `overlay.cpp` drops the loader's `SB_LEFT` / `SB_RIGHT`; xxhash is kept scalar (`XXH_VECTOR 0`) as with `-mno-sse` on Linux |

## Memory at fixed addresses

The PS2's ranges must be free when `plat_mem.c` maps them: EE RAM (0x00010000-0x02000000, the first 64 KB Windows
never maps either), its views at 0x20000000 and 0x30000000, the registers at 0x10000000, 0x11000000 and 0x12000000 and
the scratchpad at 0x70000000. A Windows process starts with its first thread's stack, its heap and the code page tables
at the bottom of its address space, inside EE RAM, and Windows cannot map over memory in use. So
(`plat_os_win32.c`, `PlatOs_MemPrepare`, called first by `plat_mem.c`):

1. The ranges are checked. Free: nothing else happens.
2. Taken: the program starts itself again in a new process with bottom-up address randomisation and its high-entropy
   form forced on (a mitigation policy given to `CreateProcessW`). The system's own allocations then begin at a random
   place in a terabyte, while the program's image stays at 0x08000000 (not relocatable). The new process is made
   suspended; the parent reserves the ranges in it before it runs, resumes it, waits, and ends with its exit code.
   The variable `TTT_WIN_FIXED_RELAUNCH` marks the second process (it does not pass it on); `TTT_NO_RELAUNCH=1`
   turns the step off.
3. In the second process the parent's reservations count as free and each is released just before its range is
   mapped (`MapViewOfFileEx` for the EE RAM views of one pagefile-backed section, `VirtualAlloc` for the rest).

If a range is still taken, `plat_mem.c` stops with its usual message (`ttt: cannot map ...` / `EE RAM range is
taken`).

## The game's stack

The game's thread runs on the EE stack in EE RAM, as on Linux (`plat_main.c`). The Win64 versions of the four stack
switches (`__wrap_main`, `Port_StateBoundary`, `Port_OffEe`, `port_call_on_ee` / `port_call_on_stack` in
`plat_state.c`) keep the Win64 callee-saved registers and the shadow space; `__wrap_main` clears rbx, rbp, rdi, rsi,
r12-r15 and xmm6-xmm15 before the game's `main`. Windows checks a stack pointer against the thread's bounds in its TEB
(the system libraries' `__chkstk` probes from `StackLimit` down to a new frame; exception dispatch stops at a frame
outside them), so `PlatOs_StackWide` widens them to 0x00010000-4 GB just before the switch: every stack the game's
thread uses afterwards (EE stack, the service stacks in the program's bss) lies inside. mingw-w64's own
`___chkstk_ms`, which the compiled code calls, only touches the pages under the stack pointer. The switches have no
unwind data: nothing unwinds through them (no C++ exceptions, no `longjmp`).

## Crash reports

A vectored exception handler reports a fault in the program's own code first (the game's thread included, whose EE
stack Windows could not unwind), turning the exception code into the signal the report names; a fault elsewhere (a
system library may handle its own) is reported by the unhandled-exception filter if nobody handles it. `abort()` is
reported through `signal(SIGABRT)`. Not possible on Windows: `TTT_CRASH_TEST=stack` (a fault with an unusable stack
pointer ends the process before any handler) and `bus` (the C library has no SIGBUS: `raise` fails and returns). The
names come from `ttt.exe.syms` (link.py) and `PlatOs_SelfImage` reads the COFF symbol table GNU ld leaves in the file
(checked against `nm`: every value equal).

## The operating system layer (b6)

| Part | What |
|---|---|
| `plat_os.h`, Linux part | Small calls as `static inline` functions. Each call stays in the file that makes it, so a call that went through the PLT still does, and one that went through the GOT (`port_host.h` `PORT_NOPLT`, `toolchain.py` `NOPLT_FILES`) still does. The header declares `PORT_NOPLT` only for C library functions that are not in the PLT. |
| `plat_os_posix.c` | The larger pieces, moved without changes from the file named beside each one. Its calls match how the original file made them. `PORT_GOT_ALIAS(f)` (`port_host.h`) gives `f_got`, a second name for a function that is called both ways (`mmap`, `getpid`). On Windows the file is empty. |
| `plat_os.h`, Windows part, and `plat_os_win32.c` | Prototypes, plus the Win32 version of each function. On Linux the `.c` file is empty. |

`port_host.h` now builds on Windows. `PORT_NOPLT` expands to an empty declaration there, so a file can still name a
function that Windows lacks. `dlfcn.h` and `unistd.h` are included only on Linux. `tests/test_port_os.py` checks
three things: no Linux-only call appears outside the layer, every `PlatOs_` function has a Windows definition, and
each side's file is switched off on the other system.

## The calls

| Linux | Where it was | `plat_os.h` | Windows (`plat_os_win32.c`) |
|---|---|---|---|
| `memfd_create` + `ftruncate` | `plat_mem.c` | `PlatOs_MemCreate` | `CreateFileMappingW(INVALID_HANDLE_VALUE, PAGE_READWRITE)` |
| `mmap(MAP_SHARED, MAP_FIXED[_NOREPLACE])` | `plat_mem.c` | `PlatOs_MapView` | `MapViewOfFileEx` at the address, after `PlatOs_MemPrepare` made the ranges free (Memory at fixed addresses) |
| `mmap(MAP_ANONYMOUS, MAP_FIXED_NOREPLACE)` | `plat_mem.c` | `PlatOs_MapFixed` | `VirtualAlloc(addr, MEM_RESERVE \| MEM_COMMIT)` (addresses aligned to 64 KB) |
| `/proc/self/maps` | `plat_mem.c` | `PlatOs_RangeTaken` | `VirtualQuery` over the range (the parent's reservations count as free) |
| `mmap(PROT_NONE / rw)` anywhere | `plat_crash.c` | `PlatOs_MapPages` | `VirtualAlloc(PAGE_NOACCESS / PAGE_READWRITE)` |
| `sigaction` + `sigaltstack` + `ucontext` registers | `plat_crash.c` | `PlatOs_CrashHooks`, `PlatOsFault` | `AddVectoredExceptionHandler` + `SetUnhandledExceptionFilter`, the `CONTEXT` registers, `SetThreadStackGuarantee`, `signal(SIGABRT)`; the exception code is turned into a signal number (Crash reports) |
| `process_vm_readv` | `plat_crash.c` | `PlatOs_ReadMemory` | `ReadProcessMemory(GetCurrentProcess())` |
| `syscall(SYS_gettid) == getpid()` | `plat_crash.c` | `PlatOs_OnMainThread` | `GetCurrentThreadId()`, compared with the id recorded by a constructor |
| `userfaultfd` + `PAGEMAP_SCAN` + `madvise` | `plat_state.c` | `PlatOs_Dirty*` | None: `GetWriteWatch` only covers `VirtualAlloc(MEM_WRITE_WATCH)` memory, and the tracked regions are file mapping views and the program's bss. Tracking is off, so each save makes a whole copy (as on Linux before 6.7): the same states, a slower rollback |
| `/proc/self/exe` (readlink) | `plat_paths.c`, `plat_file.c`, `plat_crash.c`, `ee_diff.c`, `plat_floattest.c` | `PlatOs_ExePath` | `GetModuleFileNameW`, with `\` turned into `/` |
| `/proc/self/exe` read as ELF | `plat_state.c`, `ee_diff.c` | `PlatOs_SelfImage` | The PE sections of `GetModuleHandleW(NULL)` (not the discardable debugging ones) and the COFF symbol table of the file |
| `/proc/self/cmdline` | `plat_data.c` (runs before `main`) | `PlatOs_HasArg` | `CommandLineToArgvW(GetCommandLineW())` (links shell32) |
| `statx` | `plat_data.c` | `PlatOs_FileKey` | `GetFileInformationByHandle` (volume serial number, file index, size, last write time) |
| `stat` | `plat_mc.c`, `plat_setup.c`, `plat_file.c`, `gs_texpack.c`, `plat_mods.c` | `PlatOs_Stat`, `PlatOsStat` | `_wstat64` |
| `mkdir(path, mode)` | 7 files | `PlatOs_MkDir` | `CreateDirectoryW` (no mode) |
| `rename` over an existing file | `plat_data.c`, `plat_mc.c`, `plat_settings.c`, `gs_texpack.c` | `PlatOs_Replace` | `MoveFileExW(MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH)` |
| `fsync(fileno(f))`, a folder's `fsync` | same | `PlatOs_SyncFile`, `PlatOs_SyncDir` | `_commit`; nothing for a folder |
| `access(F_OK)` | `plat_paths.c`, `plat_settings.c`, `gs_texpack.c` | `PlatOs_Exists` | `_access` |
| `opendir` / `readdir` / `closedir` | `plat_mc.c`, `plat_setup.c`, `plat_lang.c`, `gs_texpack.c`, `plat_mods.c` | `PlatOs_Dir*` | `FindFirstFileW` / `FindNextFileW` |
| `realpath` | `plat_setup.c` | `PlatOs_RealPath` | `GetFullPathNameW` |
| `mkdtemp` | `plat_mc.c` (self-test) | `PlatOs_TempDir` | `_mktemp_s` + `_mkdir` |
| `setenv` / `unsetenv` | `plat_settings.c`, `plat_mc.c`, `plat_net.c` | `PlatOs_SetEnv`, `PlatOs_UnsetEnv` | `_putenv_s` |
| `dlopen` / `dlsym` / `dlclose` / `dlerror` | `plat_window.c`, `plat_speech.c` | `PlatOs_Lib*`, `PLAT_LIB_SDL3`, `PLAT_LIB_SPEECHD` | `LoadLibraryW` / `GetProcAddress` / `FreeLibrary` / `FormatMessage`; `SDL3.dll`; no speechd |
| `posix_spawnp` + `waitpid` | `plat_speech.c` | `PlatOs_RunWait` | `CreateProcessW` with the arguments quoted as `CommandLineToArgvW` reads them + `WaitForSingleObject` (`spd-say` has no Windows counterpart: speech would be SAPI) |
| `sysconf(_SC_NPROCESSORS_ONLN)` | `gs_core.c` | `PlatOs_CpuCount` | `GetSystemInfo` |
| BSD socket headers, `poll`, `MSG_DONTWAIT`, a socket's `close` | `plat_net.c` | `PlatOs_NetInit`, `PlatOs_Socket`, `PlatOs_SockWait`, `PlatOs_SockClose`, `PlatOsSock`, `PLAT_SOCK_NONE`, `PLAT_MSG_DONTWAIT` | `WSAStartup`, `WSAPoll`, `closesocket`, `SOCKET`; `PlatOs_Socket` makes each socket non-blocking (`ioctlsocket(FIONBIO)`) and turns `SIO_UDP_CONNRESET` off |
| `/dev/urandom` | `plat_net.c` (the matchmaking cookie) | `PlatOs_Random` | `BCryptGenRandom` (links `bcrypt`) |
| `execv("/proc/self/exe")` | `plat_net.c` (the overlay's restart into a session) | `PlatOs_Restart` | `CreateProcessW` of `GetModuleFileNameW` with the arguments quoted, then this one exits (no exec in place) |
| `$XDG_DATA_HOME`, `$XDG_CONFIG_HOME`, `$HOME` | `plat_paths.c`, `plat_settings.c`, `overlay.cpp` | `Port_UserDir`, `Port_HomeDir` (`plat_paths.c`) | `%LOCALAPPDATA%`, `%APPDATA%`, `%USERPROFILE%` |

## Left to the C library

These have the same meaning under mingw-w64, so they are not wrapped:

- stdio, `getenv`, `getpid`, `unlink`, `rmdir`, `_exit`.
- `open` / `read` / `write` / `close`. The crash report and the symbol file open theirs with `PLAT_O_BINARY`, so
  Windows does no newline translation.
- Threads and clocks: winpthreads (shipped with mingw-w64), not the Win32 thread calls. It provides `pthread_*`,
  `clock_gettime` with `CLOCK_MONOTONIC`, `CLOCK_REALTIME` (the timed waits) and `CLOCK_THREAD_CPUTIME_ID`, and
  `nanosleep`. Using it keeps the GS workers, the texture pack worker, the background check, the setup thread and
  speech as they are.
- Sockets: Winsock keeps the BSD names (`socket`, `bind`, `connect`, `sendto`, `recvfrom`, `getaddrinfo`,
  `inet_ntop`). Winsock's `char *` buffers and `int` lengths are accepted as they are (the port's flags turn those
  diagnostics into warnings).

## Checked, and not

| Checked (cross-built on Linux, `tests/test_port_win64.py`) | Not checked (needs Windows or Wine) |
|---|---|
| All 134 objects compile, the same 43 stubs as Linux; the link succeeds | That the program starts: the relaunch, the fixed maps, the EE stack switch with the TEB bounds |
| PE32+ console program, ImageBase 0x08000000, no dynamic base, no high-entropy VA, no relocations, code at 0x08001000, bss inside the state hash's window (below 0x09000000) | The self-tests (mem, hw, ee, gs, float, vu1, vu0, state) and the idle run's hash `0x61dc0b370ff7019b` (the test runs them when `wine` is on PATH) |
| Imports: system DLLs and the UCRT only | The window, GL, sound, gamepads (SDL3.dll), the overlay |
| No x87 or SSE float instruction in a game or port function but the SIMD files and the overlay (the float rule) | Crash reports, sockets / netplay, the matchmaking restart, `RunWait` |
| The data rewrite table's 615 entries hold the linked addresses; the COFF symbols read as `PlatOs_SelfImage` reads them equal `nm`'s | Real Windows' address space: whether the high-entropy relaunch keeps the low 32 MB free every time; Wine's (which may ignore the policy) |
| The server and signer cross-build with `-Werror` | Their behaviour on Windows |

## Next

Run it: install Wine and `python3 -m unittest tests/test_port_win64.py` (self-tests, then the 2551-VBlank idle hash
with `TTT_SEED=1`, 17 pictures), then a real Windows machine with `SDL3.dll` next to `ttt.exe`. If the low ranges are
taken under Wine, `TTT_NO_RELAUNCH=1` shows the first occupant in `plat_mem.c`'s message.
