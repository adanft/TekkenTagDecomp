/*
 * The game runs on a stack in EE RAM, where the PS2's kernel put it: just below the top of the 32 MB
 * (crt0's SetupThread asks for "stack -1": the end of memory). Game code takes addresses of its locals and hands
 * them to the DMA (func_0035D790 sends a quadword on its stack to the scratchpad): a DMA address is the game
 * address + 0x10000000 masked with 0x8FFFFFFF, which only works for addresses below 0x10000000. The host stack
 * (0xFFxxxxxx) would wrap.
 *
 * The link wraps `main` (port/tools/link.py: -Wl,--wrap=main): __wrap_main moves the stack pointer to the EE stack
 * and calls the game's main. The space between the end of .bss (0x01FCDEF0) and 0x02000000 is 200 KB; on the PS2
 * the same stack grows into the top of .bss as well (docs/game_overview.md section 1).
 */
#include <stdint.h>
#include <stdlib.h>

#define PORT_EE_STACK_TOP 0x01FFFF00u /* the 256 bytes above: PORT_EE_SPARE_QW (port_compat.h) */

int __real_main(int argc, char **argv);
int Port_WindowOpen(void); /* plat_window.c */
void Port_PadSelfTest(void); /* plat_pad.c: TTT_SELFTEST=pad */
void Port_WindowSelfTest(void); /* plat_window.c: TTT_SELFTEST=window */
void Settings_Load(void); /* plat_settings.c: the settings file, before anything reads the variables it sets */
void Settings_SelfTest(void); /* TTT_SELFTEST=settings */
void Lang_Init(void); /* plat_lang.c: the interface's language (T47) */
void Speech_Init(void); /* plat_speech.c: the screen reader's output (T48) */
void A11y_Init(void); /* plat_a11y.c: the game's announcements (T48, opt-in) */
void Port_CrashInstall(void); /* plat_crash.c: the crash report handlers, with their own signal stack */
void Port_SetupArgs(int argc, char **argv); /* plat_setup.c: `--setup <disc>` (exits) or `--setup` (the screen) */
void Port_SetupRun(void); /* plat_setup.c: the setup screen when the game's data is not loaded yet (T43) */

int __wrap_main(int argc, char **argv) {
    /* the settings and the window first, on the host stack: SDL's start-up is neither for the EE stack nor for the
       game's clock */
    Settings_Load();
    Lang_Init();
    Speech_Init();
    A11y_Init();
    Settings_SelfTest();
    Port_PadSelfTest();
    Port_WindowSelfTest();
    Port_SetupArgs(argc, argv);
    Port_WindowOpen();
    Port_SetupRun();
    Port_CrashInstall();
    /* The callee-saved registers are cleared first: they hold the C library's values here (__libc_start_main's), and
       the game's functions push them on the EE stack, which a state holds (plat_state.c). This never returns. */
#if defined(__x86_64__)
    /* x86-64 (TTT_CC=clang64): the same in the SysV convention, arguments in edi / rsi; rbx, rbp, r12-r15 cleared */
    __asm__ volatile("movq %0, %%rsp\n\t" /* 16-byte aligned at the call */
                     "xorl %%ebx, %%ebx\n\t"
                     "xorl %%ebp, %%ebp\n\t"
                     "xorl %%r12d, %%r12d\n\t"
                     "xorl %%r13d, %%r13d\n\t"
                     "xorl %%r14d, %%r14d\n\t"
                     "xorl %%r15d, %%r15d\n\t"
                     "call __real_main\n\t"
                     "movl %%eax, %%edi\n\t"
                     "call exit"
                     :
                     : "a"((uintptr_t)PORT_EE_STACK_TOP), "D"(argc), "S"(argv)
                     : "memory");
#else
    __asm__ volatile("movl %0, %%esp\n\t"
                     "subl $8, %%esp\n\t" /* 16-byte aligned at the call */
                     "pushl %2\n\t"
                     "pushl %1\n\t"
                     "xorl %%ebx, %%ebx\n\t"
                     "xorl %%esi, %%esi\n\t"
                     "xorl %%edi, %%edi\n\t"
                     "xorl %%ebp, %%ebp\n\t"
                     "call __real_main\n\t"
                     "subl $12, %%esp\n\t"
                     "pushl %%eax\n\t"
                     "call exit"
                     :
                     : "r"(PORT_EE_STACK_TOP), "r"(argc), "r"(argv)
                     : "memory");
#endif
    __builtin_unreachable();
}
