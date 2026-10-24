/*
 * newlib's rand / srand as SLUS_200.01 has them (0x40FBD0 / 0x40FBC0). The game's random numbers are simulation
 * state (docs/known_bugs.md, section 8), so they must be the PS2 C library's, not the host's: the 32-bit linear
 * congruential generator of newlib's reentrancy structure, next = next * 1103515245 + 12345, result next & 0x7FFFFFFF.
 * The state is the game's own word in EE RAM, `_rand_next` at offset 0x58 of the structure `_impure_ptr` (D_004F5A8C)
 * points to, so it is saved, hashed and compared with the rest of the game's memory.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "plat_state.h"

extern uint8_t *D_004F5A8C; /* _impure_ptr */

void srand(unsigned seed) { *(uint32_t *)(D_004F5A8C + 0x58) = seed; }

int rand(void) {
    uint32_t *next = (uint32_t *)(D_004F5A8C + 0x58);

    *next = *next * 1103515245u + 12345u;
    return (int)(*next & 0x7FFFFFFFu);
}

/* printf / sprintf / memcpy / memset (the game's and the port's, port/tools/link.py: --wrap) off the EE stack: the C
   library's frames would leave addresses of this process there (a state holds the EE stack; the game reads stale stack
   words in places, docs/port/determinism.md). vprintf / vsprintf through the GOT: no new PLT entry (port_host.h). */
__typeof__(vprintf) vprintf __attribute__((noplt));
__typeof__(vsprintf) vsprintf __attribute__((noplt));

typedef struct {
    char *buf;
    const char *fmt;
    va_list ap;
    int r;
} Fmt;

static void vprintf_off(void *p) {
    Fmt *f = p;
    f->r = vprintf(f->fmt, f->ap);
}

static void vsprintf_off(void *p) {
    Fmt *f = p;
    f->r = vsprintf(f->buf, f->fmt, f->ap);
}

int __wrap_printf(const char *fmt, ...) {
    Fmt f;
    f.buf = NULL;
    f.fmt = fmt;
    va_start(f.ap, fmt);
    Port_OffEe(vprintf_off, &f);
    va_end(f.ap);
    return f.r;
}

int __wrap_sprintf(char *buf, const char *fmt, ...) {
    Fmt f;
    f.buf = buf;
    f.fmt = fmt;
    va_start(f.ap, fmt);
    Port_OffEe(vsprintf_off, &f);
    va_end(f.ap);
    return f.r;
}

/* memcpy / memset on the EE stack: string instructions in place, no call into the C library (whose memcpy calls on
   inside itself); elsewhere the C library's. */
void *__real_memcpy(void *d, const void *s, size_t n);
void *__real_memset(void *d, int c, size_t n);

static inline int on_ee_stack(void) {
    uint32_t sp;
    __asm__("movl %%esp, %0" : "=r"(sp));
    return sp < 0x02000000u;
}

void *__wrap_memcpy(void *d, const void *s, size_t n) {
    void *dd = d;
    if (!on_ee_stack()) {
        return __real_memcpy(d, s, n);
    }
    __asm__ volatile("rep movsb" : "+D"(dd), "+S"(s), "+c"(n) : : "memory");
    return d;
}

void *__wrap_memset(void *d, int c, size_t n) {
    void *dd = d;
    if (!on_ee_stack()) {
        return __real_memset(d, c, n);
    }
    __asm__ volatile("rep stosb" : "+D"(dd), "+c"(n) : "a"(c) : "memory");
    return d;
}
