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
#include <stdlib.h>
#include <string.h>
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

#if defined(__x86_64__)
/* x86-64 (TTT_CC=clang64): the game's code and the port's C are software float (toolchain.py), so a double passed to
   a variadic function travels in an integer register, and the C library's vprintf would read it from a vector register
   (SysV). So printf / sprintf format here, one conversion at a time through the C library's snprintf: the integer,
   string and pointer arguments as they are, a double's bits handed over in %xmm0 by port_snprintf_double (a move, no
   float instruction). Not handled (neither the game nor the port uses them): positional arguments (%1$d) and long
   double (%Lf). The port's other printf-family calls (fprintf, snprintf) have no floating conversions. */
int port_snprintf_double(char *buf, size_t n, const char *spec, uint64_t bits);
__asm__(".text\n"
        ".globl port_snprintf_double\n"
        ".type port_snprintf_double, @function\n"
        "port_snprintf_double:\n"
        "\tmovq %rcx, %xmm0\n"
        "\tmovl $1, %eax\n" /* (one vector register argument) */
        "\tjmp snprintf@PLT\n"
        ".size port_snprintf_double, .-port_snprintf_double\n");

typedef struct {
    char *dst; /* sprintf's buffer, or NULL for stdout */
    int n;     /* characters written so far */
} Out;

static void out_text(Out *o, const char *s, size_t len) {
    if (o->dst != NULL) {
        memcpy(o->dst + o->n, s, len);
    } else {
        fwrite(s, 1, len, stdout);
    }
    o->n += (int)len;
}

/* One conversion's argument, taken from the va_list once (so it can be formatted again into a larger buffer). */
typedef struct {
    int kind; /* 0 int, 1 long long, 2 pointer, 3 double (its bits) */
    int i;
    long long ll;
    void *ptr;
    uint64_t bits;
} Arg;

static int format_one(char *buf, size_t n, const char *spec, const Arg *a) {
    switch (a->kind) {
    case 0:
        return snprintf(buf, n, spec, a->i);
    case 1:
        return snprintf(buf, n, spec, a->ll);
    case 2:
        return snprintf(buf, n, spec, a->ptr);
    default:
        return port_snprintf_double(buf, n, spec, a->bits);
    }
}

/* The conversion at fmt ("%<flags><width>.<precision><length><conv>", a `*` taken from the arguments) written as one
   snprintf call; returns the length of its text in fmt. */
static int format(Out *o, const char *fmt, va_list *ap) {
    char spec[64], tmp[512], *text = tmp;
    const char *p = fmt + 1;
    int k = 0, r, longs = 0, half = 0, dot;
    Arg a = {0};
    spec[k++] = '%';
    for (; *p && strchr("-+ #0'", *p) != NULL && k < 16; p++) {
        spec[k++] = *p;
    }
    for (dot = 0; dot < 2; dot++) { /* the width, then '.' and the precision */
        if (dot == 1) {
            if (*p != '.') {
                break;
            }
            spec[k++] = *p++;
        }
        if (*p == '*') {
            k += snprintf(spec + k, 16, "%d", va_arg(*ap, int));
            p++;
        } else {
            for (; *p >= '0' && *p <= '9' && k < 40; p++) {
                spec[k++] = *p;
            }
        }
    }
    for (; *p && strchr("hlLqjzt", *p) != NULL && k < 48; p++) {
        longs += *p != 'h';
        half += *p == 'h';
        spec[k++] = *p;
    }
    if (*p == 0) { /* an unfinished conversion at the end: as written */
        out_text(o, fmt, (size_t)(p - fmt));
        return (int)(p - fmt);
    }
    spec[k++] = *p;
    spec[k] = 0;
    switch (*p) {
    case 'd': case 'i': case 'u': case 'o': case 'x': case 'X':
        if (longs) {
            a.kind = 1, a.ll = va_arg(*ap, long long);
        } else {
            a.kind = 0, a.i = va_arg(*ap, int);
        }
        break;
    case 'c':
        a.kind = 0, a.i = va_arg(*ap, int);
        break;
    case 's': case 'p':
        a.kind = 2, a.ptr = va_arg(*ap, void *);
        break;
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A':
        a.kind = 3, a.bits = va_arg(*ap, uint64_t);
        break;
    case 'n': {
        void *v = va_arg(*ap, void *);
        if (half >= 2) {
            *(signed char *)v = (signed char)o->n;
        } else if (half == 1) {
            *(short *)v = (short)o->n;
        } else if (longs) {
            *(long long *)v = o->n;
        } else {
            *(int *)v = o->n;
        }
        return (int)(p + 1 - fmt);
    }
    case '%':
        out_text(o, "%", 1);
        return (int)(p + 1 - fmt);
    default: /* not a conversion: as written */
        out_text(o, fmt, (size_t)(p + 1 - fmt));
        return (int)(p + 1 - fmt);
    }
    r = format_one(tmp, sizeof tmp, spec, &a);
    if (r >= (int)sizeof tmp && (text = malloc((size_t)r + 1)) != NULL) {
        format_one(text, (size_t)r + 1, spec, &a);
    }
    if (r > 0) { /* (cut to the buffer when a long text found no memory) */
        out_text(o, text != NULL ? text : tmp, text != NULL ? (size_t)r : sizeof tmp - 1);
    }
    if (text != tmp) {
        free(text);
    }
    return (int)(p + 1 - fmt);
}

static int port_vformat(char *dst, const char *fmt, va_list ap) {
    Out o = {dst, 0};
    va_list a;
    va_copy(a, ap);
    while (*fmt) {
        const char *pct = strchr(fmt, '%');
        if (pct == NULL) {
            out_text(&o, fmt, strlen(fmt));
            break;
        }
        out_text(&o, fmt, (size_t)(pct - fmt));
        fmt = pct + format(&o, pct, &a);
    }
    va_end(a);
    if (dst != NULL) {
        dst[o.n] = 0;
    }
    return o.n;
}

static void vprintf_off(void *p) {
    Fmt *f = p;
    f->r = port_vformat(NULL, f->fmt, f->ap);
}

static void vsprintf_off(void *p) {
    Fmt *f = p;
    f->r = port_vformat(f->buf, f->fmt, f->ap);
}
#else
static void vprintf_off(void *p) {
    Fmt *f = p;
    f->r = vprintf(f->fmt, f->ap);
}

static void vsprintf_off(void *p) {
    Fmt *f = p;
    f->r = vsprintf(f->buf, f->fmt, f->ap);
}
#endif

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
