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
#include "port_host.h"

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
PORT_NOPLT(vprintf);
PORT_NOPLT(vsprintf);

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
#if defined(__x86_64__)
    uintptr_t sp; /* (x86-64: the host stacks lie above 4 GB, whose low half could look like EE RAM) */
    __asm__("movq %%rsp, %0" : "=r"(sp));
#else
    uint32_t sp;
    __asm__("movl %%esp, %0" : "=r"(sp));
#endif
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

/* acosf (0x407928), the one float maths wrapper of the executable that third_party/newlib_libm/wrappers.c lacks (only
   the Tekken Bowl's func_003EEF00 calls it): newlib's wf_acos.c built without _IEEE_LIBM, the same code as asinf
   there. Out of [-1, 1] (and not a NaN pattern, unless __fdlib_version is _IEEE_) errno becomes EDOM (33) in both
   the POSIX and the SVID / X/Open cases (newlib's default matherr returns 0) and the result is 0. */
extern int D_005F4848; /* __fdlib_version */
float __ieee754_acosf(float x);

float acosf(float x) {
    float z = __ieee754_acosf(x), a;
    uint32_t ix;

    __builtin_memcpy(&ix, &x, 4);
    if (D_005F4848 == -1 || (uint32_t)(0x7F800000 - (ix & 0x7FFFFFFF)) >> 31) {
        return z;
    }
    ix &= 0x7FFFFFFF; /* fabsf */
    __builtin_memcpy(&a, &ix, 4);
    if (a > 1.0f) {
        *(int32_t *)D_004F5A8C = 33;
        return 0.0f;
    }
    return z;
}
