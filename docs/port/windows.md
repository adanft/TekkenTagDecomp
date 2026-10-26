# Windows: the operating system layer (T42 b6)

The port talks to the operating system through one header, `port/src/plat_os.h`. Linux is unchanged: the same
calls, made from the same files with the same flags, so the 2551-VBlank idle run and the scripted fight end in the
same state (`0x8ba37caeb2bacfa8` / `0xcce5eb554fa51395`) and the 32-bit program keeps its 48 PLT entries in the same
order. The Windows side, `plat_os_win32.c`, is a skeleton for T42 b7 (mingw-w64, x86-64): it names the Win32
replacement for every call and implements most of them, but no compiler has checked it yet.

## How it is split

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
| `mmap(MAP_SHARED, MAP_FIXED[_NOREPLACE])` | `plat_mem.c` | `PlatOs_MapView` | `MapViewOfFileEx` at the address. Mapping over memory already in use (`replace`, EE RAM over the program's own range) is not possible: see below |
| `mmap(MAP_ANONYMOUS, MAP_FIXED_NOREPLACE)` | `plat_mem.c` | `PlatOs_MapFixed` | `VirtualAlloc(addr, MEM_RESERVE \| MEM_COMMIT)` (addresses aligned to 64 KB) |
| `/proc/self/maps` | `plat_mem.c` | `PlatOs_RangeTaken` | `VirtualQuery` over the range |
| `mmap(PROT_NONE / rw)` anywhere | `plat_crash.c` | `PlatOs_MapPages` | `VirtualAlloc(PAGE_NOACCESS / PAGE_READWRITE)` |
| `sigaction` + `sigaltstack` + `ucontext` registers | `plat_crash.c` | `PlatOs_CrashHooks`, `PlatOsFault` | `AddVectoredExceptionHandler`, the `CONTEXT` registers, `SetThreadStackGuarantee`; the exception code is turned into a signal number |
| `process_vm_readv` | `plat_crash.c` | `PlatOs_ReadMemory` | `ReadProcessMemory(GetCurrentProcess())` |
| `syscall(SYS_gettid) == getpid()` | `plat_crash.c` | `PlatOs_OnMainThread` | `GetCurrentThreadId()`, compared with the id recorded by a constructor |
| `userfaultfd` + `PAGEMAP_SCAN` + `madvise` | `plat_state.c` | `PlatOs_Dirty*` | Not done yet: `GetWriteWatch` only covers `VirtualAlloc(MEM_WRITE_WATCH)` memory. Tracking is off, so each save makes a whole copy (as on Linux before 6.7) |
| `/proc/self/exe` (readlink) | `plat_paths.c`, `plat_file.c`, `plat_crash.c`, `ee_diff.c`, `plat_floattest.c` | `PlatOs_ExePath` | `GetModuleFileNameW`, with `\` turned into `/` |
| `/proc/self/exe` read as ELF | `plat_state.c`, `ee_diff.c` | `PlatOs_SelfImage` | The PE sections of `GetModuleHandleW(NULL)`. Symbols are still to do: the COFF symbol table in the file |
| `/proc/self/cmdline` | `plat_data.c` (runs before `main`) | `PlatOs_HasArg` | `CommandLineToArgvW(GetCommandLineW())` |
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
| `posix_spawnp` + `waitpid` | `plat_speech.c` | `PlatOs_RunWait` | `CreateProcessW` + `WaitForSingleObject` (to do; `spd-say` has no Windows counterpart: SAPI) |
| `sysconf(_SC_NPROCESSORS_ONLN)` | `gs_core.c` | `PlatOs_CpuCount` | `GetSystemInfo` |
| BSD socket headers, `poll`, `MSG_DONTWAIT`, a socket's `close` | `plat_net.c` | `PlatOs_NetInit`, `PlatOs_SockWait`, `PlatOs_SockClose`, `PlatOsSock`, `PLAT_SOCK_NONE`, `PLAT_MSG_DONTWAIT` | `WSAStartup`, `WSAPoll`, `closesocket`, `SOCKET`; the socket must be made non-blocking with `ioctlsocket(FIONBIO)` (to do) |
| `/dev/urandom` | `plat_net.c` (the matchmaking cookie) | `PlatOs_Random` | `BCryptGenRandom` (links `bcrypt`) |
| `execv("/proc/self/exe")` | `plat_net.c` (the overlay's restart into a session) | `PlatOs_Restart` | `_wexecv` of `GetModuleFileNameW`: a new process, then this one exits |
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
  `inet_ntop`). b7 still needs casts, because Winsock takes `char *` buffers and `int` lengths.

## What b7 has to solve

- **Fixed-address memory.** EE RAM (0x00010000-0x02000000) and its views at 0x20000000 and 0x30000000 must be free
  when `plat_mem.c` maps them. Linux maps EE RAM over the program's own range (`MAP_FIXED`); Windows cannot. Three
  things are needed: link the PE image above 0x02000000 (this is the riskiest part of the data at absolute addresses,
  T42 b0); keep the heap out of the way with an early `VirtualAlloc(MEM_RESERVE)` of the PS2 ranges; and release that
  reservation just before the views are made. Windows also cannot map the first 64 KB.
- **Crash reports for raised signals** (`abort`, `raise(SIGBUS)` in the test) need `signal()` as well, and
  `PlatOs_CrashHooks` must keep its own copy of the signal list.
- **Page tracking** with `GetWriteWatch` needs the tracked regions allocated with `MEM_WRITE_WATCH`. That rules out
  the file mapping views, so the likely design is a copy-on-save of EE RAM.
- **Symbols** of the program for the state hash's labels and the divergence check: read the COFF symbol table, or
  have `link.py` write a table as it already does `ttt.syms`.
- **Netplay.** Make the socket non-blocking (`ioctlsocket(FIONBIO)` after each `socket`). The matchmaking code (T46)
  goes through the layer like the rest; its restart into a session (`PlatOs_Restart`) is `_wexecv`, which joins the
  arguments with spaces unquoted: quote them as `PlatOs_RunWait` will.
