/* Host library functions the port's later code calls (GS threads, the window), declared `noplt`: the calls go through
   the GOT, so the program keeps the PLT it had. The game stores host code addresses in its memory (.sbss holds game
   function pointers) and the TTT_FRAMES state hash covers them: a new PLT entry would move every game function and
   change the hash of an unchanged run. tests/test_port_platform.py checks that the PLT has none of these. */
#ifndef PORT_HOST_H
#define PORT_HOST_H
#include <pthread.h>
#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

/* PORT_NOPLT(f): calls of the host library function f go through the GOT. gcc: the `noplt` attribute. clang on 32-bit
   x86 knows neither the attribute nor a working -fno-plt: the calls go to the local thunk __ttt_got_<f> instead, a
   `jmp *f@GOT` that port/tools/link.py generates (toolchain.py, GOT_THUNK). Stays defined for the files that declare
   their own host calls. On Windows (T42 b6, plat_os.h; no PLT there) a declaration of nothing, so that a function
   Windows lacks may still be named.
   PORT_GOT_ALIAS(f): f_got, a second name of f whose calls go through the GOT while calls of f itself keep the PLT,
   for a file holding calls that came from files of either kind (plat_os_posix.c). */
#if defined(_WIN32)
#define PORT_NOPLT(f) extern int port_noplt_##f
#define PORT_GOT_ALIAS(f) extern __typeof__(f) f##_got __asm__(#f)
#elif defined(__clang__) && defined(__i386__)
#define PORT_NOPLT(f) __typeof__(f) f __asm__("__ttt_got_" #f)
#define PORT_GOT_ALIAS(f) extern __typeof__(f) f##_got __asm__("__ttt_got_" #f)
#else
#define PORT_NOPLT(f) __typeof__(f) f __attribute__((noplt))
#define PORT_GOT_ALIAS(f) extern __typeof__(f) f##_got __asm__(#f) __attribute__((noplt))
#endif
PORT_NOPLT(pthread_create);
PORT_NOPLT(pthread_detach);
PORT_NOPLT(pthread_mutex_lock);
PORT_NOPLT(pthread_mutex_unlock);
PORT_NOPLT(pthread_cond_wait);
PORT_NOPLT(pthread_cond_broadcast);
PORT_NOPLT(pthread_cond_timedwait);
#ifndef _WIN32
PORT_NOPLT(dlopen);
PORT_NOPLT(dlsym);
PORT_NOPLT(dlerror);
PORT_NOPLT(dlclose);
PORT_NOPLT(sysconf);
PORT_NOPLT(_exit);
#endif

#endif
