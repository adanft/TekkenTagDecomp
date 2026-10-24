/*
 * Dear ImGui's configuration for the port's overlay (T36), named by IMGUI_USER_CONFIG in port/tools/toolchain.py.
 *
 * ImGui runs only on the window thread (port/src/overlay.cpp) and uses host floating point there; nothing of the
 * simulation reads what it computes (docs/port/settings.md). The program links no host maths library: the game's
 * sinf / sqrtf... are the PS2's newlib in software float (third_party/newlib_libm), which return their results in eax,
 * where code built with host floats expects them on the x87 stack. So ImGui's maths functions are given here, built
 * from SSE instructions and integer steps, and no ImGui file may call the C library's maths functions (the build's
 * check: tests/test_port_float.py). No files (the settings file is the port's own), no shell, no demo windows.
 */
#pragma once

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_FILE_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
#define IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
#define IMGUI_IMPL_OPENGL_LOADER_IMGL3W /* functions from SDL_GL_GetProcAddress (imgl3wInit2), no libGL linked */

/* A failed ImGui check is reported once on stderr, and the overlay goes on (it never stops the game). */
extern "C" void Overlay_AssertFailed(const char *what, const char *file, int line);
#define IM_ASSERT(_EXPR) ((_EXPR) ? (void)0 : Overlay_AssertFailed(#_EXPR, __FILE__, __LINE__))

/* ---- maths (window thread only; accuracy for drawing, not for the game) ---- */
static inline float ImTttSqrtf(float x) { return x > 0.0f ? __builtin_sqrtf(x) : 0.0f; } /* sqrtss: -fno-math-errno */
static inline float ImTttFabs(float x) { return x < 0.0f ? -x : x; }
static inline float ImTttTrunc(float x) { return (x > 8388608.0f || x < -8388608.0f) ? x : (float)(int)x; }
static inline float ImTttFloor(float x) { float t = ImTttTrunc(x); return t > x ? t - 1.0f : t; }
static inline float ImTttCeil(float x) { float t = ImTttTrunc(x); return t < x ? t + 1.0f : t; }
static inline float ImTttFmod(float x, float y) { return y == 0.0f ? 0.0f : x - ImTttTrunc(x / y) * y; }
/* sine on [-pi, pi] after reduction: an odd polynomial (max error about 1e-6) */
static inline float ImTttSin(float x) {
    const float pi = 3.14159265f, twopi = 6.28318531f;
    float x2;
    x = ImTttFmod(x, twopi);
    if (x > pi) x -= twopi;
    if (x < -pi) x += twopi;
    if (x > pi * 0.5f) x = pi - x;      /* sin(pi - x) = sin(x) */
    if (x < -pi * 0.5f) x = -pi - x;
    x2 = x * x;
    return x * (1.0f + x2 * (-1.6666667e-1f + x2 * (8.3333310e-3f + x2 * (-1.9840874e-4f + x2 * 2.7525562e-6f))));
}
static inline float ImTttCos(float x) { return ImTttSin(x + 1.57079633f); }
/* atan on [-1, 1] (max error about 1e-5), then the quadrants */
static inline float ImTttAtanUnit(float z) {
    float z2 = z * z;
    return z * (0.99997726f + z2 * (-0.33262347f + z2 * (0.19354346f + z2 * (-0.11643287f + z2 * (0.05265332f + z2 * -0.01172120f)))));
}
static inline float ImTttAtan2(float y, float x) {
    const float pi = 3.14159265f;
    float ax = ImTttFabs(x), ay = ImTttFabs(y), r;
    if (ax == 0.0f && ay == 0.0f) return 0.0f;
    r = ay <= ax ? ImTttAtanUnit(ay / ax) : pi * 0.5f - ImTttAtanUnit(ax / ay);
    if (x < 0.0f) r = pi - r;
    return y < 0.0f ? -r : r;
}
static inline float ImTttAcos(float x) {
    if (x >= 1.0f) return 0.0f;
    if (x <= -1.0f) return 3.14159265f;
    return ImTttAtan2(ImTttSqrtf(1.0f - x * x), x);
}
/* log2 / exp2 from the float's fields and a polynomial for the mantissa */
static inline float ImTttLog2(float x) {
    union { float f; int i; } u;
    float m;
    int e;
    if (!(x > 0.0f)) return -128.0f;
    u.f = x;
    e = ((u.i >> 23) & 255) - 127;
    u.i = (u.i & 0x007FFFFF) | 0x3F800000;
    m = u.f - 1.0f; /* [0, 1) */
    return (float)e + m * (1.4426950f + m * (-0.7213475f + m * (0.4808983f + m * (-0.3606737f + m * (0.2885390f + m * -0.1442695f)))));
}
static inline float ImTttExp2(float x) {
    union { float f; int i; } u;
    float fl, f;
    if (x > 127.0f) x = 127.0f;
    if (x < -126.0f) return 0.0f;
    fl = ImTttFloor(x);
    f = x - fl;
    u.i = ((int)fl + 127) << 23;
    return u.f * (1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f)))));
}
static inline float ImTttPow(float x, float y) {
    if (x == 0.0f) return y == 0.0f ? 1.0f : 0.0f;
    if (x < 0.0f) return 0.0f;
    return ImTttExp2(y * ImTttLog2(x));
}
/* decimal text to double: sign, digits, point, exponent (what ImGui's input fields hand it) */
static inline double ImTttAtof(const char *s) {
    double v = 0.0, scale = 1.0;
    int neg = 0, e = 0, eneg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    while (*s >= '0' && *s <= '9') v = v * 10.0 + (*s++ - '0');
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') { v = v * 10.0 + (*s++ - '0'); scale *= 10.0; }
    }
    if (*s == 'e' || *s == 'E') {
        s++;
        if (*s == '-' || *s == '+') eneg = *s++ == '-';
        while (*s >= '0' && *s <= '9' && e < 300) e = e * 10 + (*s++ - '0');
    }
    while (e-- > 0) { if (eneg) scale *= 10.0; else v *= 10.0; }
    v /= scale;
    return neg ? -v : v;
}

#define ImFabs(X) ImTttFabs(X)
#define ImSqrt(X) ImTttSqrtf(X)
#define ImFmod(X, Y) ImTttFmod((X), (Y))
#define ImCos(X) ImTttCos(X)
#define ImSin(X) ImTttSin(X)
#define ImAcos(X) ImTttAcos(X)
#define ImAtan2(Y, X) ImTttAtan2((Y), (X))
#define ImAtof(STR) ImTttAtof(STR)
#define ImCeil(X) ImTttCeil(X)
static inline float ImPow(float x, float y) { return ImTttPow(x, y); }
static inline double ImPow(double x, double y) { return ImTttPow((float)x, (float)y); }
static inline float ImLog(float x) { return ImTttLog2(x) * 0.69314718f; }
static inline double ImLog(double x) { return ImTttLog2((float)x) * 0.69314718f; }
static inline int ImAbs(int x) { return x < 0 ? -x : x; }
static inline float ImAbs(float x) { return x < 0.0f ? -x : x; }
static inline double ImAbs(double x) { return x < 0.0 ? -x : x; }
static inline float ImSign(float x) { return (x < 0.0f) ? -1.0f : (x > 0.0f) ? 1.0f : 0.0f; }
static inline double ImSign(double x) { return (x < 0.0) ? -1.0 : (x > 0.0) ? 1.0 : 0.0; }
static inline float ImRsqrt(float x) { return 1.0f / ImTttSqrtf(x); }
static inline double ImRsqrt(double x) { return 1.0 / ImTttSqrtf((float)x); }
