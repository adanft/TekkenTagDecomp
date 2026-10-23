/*
 * Ground truth for the game's own float maths library (newlib 1.10.0 in SLUS_200.01). This driver is linked at
 * 0x01000000 and packed (pack_libm.py) into one ELF together with your own SLUS_200.01, so it calls the ORIGINAL
 * routines at their addresses inside PCSX2 and prints the result bits on the EE serial port. Save the lines between
 * PS2LIBM BEGIN / END as pcsx2_libm.txt next to this file: TTT_SELFTEST=float then compares the port's library with
 * them. Columns: a b c sinf(a) cosf(a) atanf(a) atan2f(a, b) asinf(c) acosf(c) logf(|a|) powf(|a|, c) fmodf(a, b)
 * floorf(a). Build: build.sh. PENDING: not run yet (needs PCSX2 and a BIOS).
 */
typedef unsigned int u32;
typedef float f32;
#define FN1(addr) ((f32 (*)(f32))(addr))
#define FN2(addr) ((f32 (*)(f32, f32))(addr))
static void putc_(char c) { *(volatile char *)0x1000F180 = c; }
static void hex(u32 v) { int i; for (i = 28; i >= 0; i -= 4) putc_("0123456789abcdef"[(v >> i) & 15]); putc_(' '); }
static void str(const char *s) { while (*s) putc_(*s++); }
static u32 seed = 2024;
static u32 rnd(void) { seed = seed * 1664525u + 1013904223u; return seed; }
static f32 f(u32 u) { union { u32 u; f32 f; } x; x.u = u; return x.f; }
static u32 u(f32 v) { union { u32 u; f32 f; } x; x.f = v; return x.u; }

int main(void) {
    int i;

    str("\nPS2LIBM BEGIN\n");
    for (i = 0; i < 1500; i++) {
        u32 a = (rnd() & 0x807FFFFF) | ((112 + (rnd() >> 8) % 20) << 23); /* magnitudes 2^-15 .. 2^4 */
        u32 b = (rnd() & 0x807FFFFF) | ((112 + (rnd() >> 8) % 20) << 23);
        u32 c = (rnd() & 0x807FFFFF) | ((110 + (rnd() >> 8) % 17) << 23); /* below 1 */
        hex(a); hex(b); hex(c);
        hex(u(FN1(0x00407728)(f(a))));                     /* sinf */
        hex(u(FN1(0x00407640)(f(a))));                     /* cosf */
        hex(u(FN1(0x0040B2D0)(f(a))));                     /* atanf */
        hex(u(FN2(0x00407B28)(f(a), f(b))));               /* atan2f */
        hex(u(FN1(0x00407A28)(f(c))));                     /* asinf */
        hex(u(FN1(0x00407928)(f(c))));                     /* acosf */
        hex(u(FN1(0x00407D90)(f(a & 0x7FFFFFFF))));        /* logf */
        hex(u(FN2(0x00407ED8)(f(a & 0x7FFFFFFF), f(c))));  /* powf */
        hex(u(FN2(0x00407C50)(f(a), f(b))));               /* fmodf */
        hex(u(FN1(0x0040B5C0)(f(a))));                     /* floorf */
        putc_('\n');
    }
    str("PS2LIBM END\n");
    return 0;
}
