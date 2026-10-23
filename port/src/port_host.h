/* Host library functions the port's later code calls (GS threads, the window), declared `noplt`: the calls go through
   the GOT, so the program keeps the PLT it had. The game stores host code addresses in its memory (.sbss holds game
   function pointers) and the TTT_FRAMES state hash covers them: a new PLT entry would move every game function and
   change the hash of an unchanged run. tests/test_port_platform.py checks that the PLT has none of these. */
#ifndef PORT_HOST_H
#define PORT_HOST_H
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

#define PORT_NOPLT(f) __typeof__(f) f __attribute__((noplt))
PORT_NOPLT(pthread_create);
PORT_NOPLT(pthread_detach);
PORT_NOPLT(pthread_mutex_lock);
PORT_NOPLT(pthread_mutex_unlock);
PORT_NOPLT(pthread_cond_wait);
PORT_NOPLT(pthread_cond_broadcast);
PORT_NOPLT(pthread_cond_timedwait);
PORT_NOPLT(dlopen);
PORT_NOPLT(dlsym);
PORT_NOPLT(dlerror);
PORT_NOPLT(dlclose);
PORT_NOPLT(sysconf);
#undef PORT_NOPLT

#endif
