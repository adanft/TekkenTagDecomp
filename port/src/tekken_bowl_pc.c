/*
 * The Tekken Bowl drafts the PC build leaves off (port/tools/portsrc.py REPLACED) that are mostly FPU code: their m2c
 * drafts in src/tekken_bowl.c hold raw `sqrt.s` / `cvt.w.s` words and VU0 instructions on registers the C does not
 * hold. Written again from the asm, one statement per FPU instruction or call, in its order. This file is built like
 * the game's C (software float on the EE's FPU model, softfloat_ps2.c), so `+ - * /`, `sqrtf` and the comparisons are
 * the FPU's add.s / sub.s / mul.s / div.s / sqrt.s / c.cond.s; the VU0 code goes through vu0_pc.c (Port_Vu0Bowl*,
 * func_003E8FA0 ...), on the vector unit's arithmetic. Structures are read at the asm's offsets.
 */
#include <stdint.h>
#include <string.h>

typedef float f32;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint8_t u8;
typedef int8_t s8;

/* A .lit4 constant by its bits. */
static f32 W(uint32_t w) {
    f32 f;

    memcpy(&f, &w, 4);
    return f;
}
#define F32(p, o) (*(f32 *)((u8 *)(p) + (o)))
#define S32(p, o) (*(s32 *)((u8 *)(p) + (o)))
#define S16(p, o) (*(s16 *)((u8 *)(p) + (o)))
#define U16(p, o) (*(u16 *)((u8 *)(p) + (o)))
#define Q16(d, s) memcpy((d), (s), 16) /* lq / sq of a quadword */
#define VEC f32 __attribute__((aligned(16)))

/* vu0_pc.c */
void func_003E8FA0(f32 x);
void func_003E9038(f32 *d, const f32 *m, const f32 *rot);
void func_003E9120(const f32 *m, const f32 *rot, const f32 *pos);
void func_003E91D0(f32 *d, const f32 *m, const f32 *rot);
void func_003E92C0(f32 *d, const f32 *a, const f32 *b, f32 t);
void func_003ECAD8(void *b);
void Port_Vu0BowlLoadMtx(const f32 *m);
void Port_Vu0BowlRotX(void);
void Port_Vu0BowlRotY(void);
void Port_Vu0BowlRotZ(void);
void Port_Vu0BowlXform(f32 *out, const f32 *v);
void Port_Vu0BowlAdd13(f32 *out, const f32 *b);
/* plat_vu0.c (libvu0) */
void sceVu0SubVector(f32 *d, const f32 *a, const f32 *b);
void sceVu0ScaleVector(f32 *d, const f32 *a, f32 s);
void sceVu0CopyMatrix(f32 *d, const f32 *m);
void sceVu0MulMatrix(f32 *d, const f32 *a, const f32 *b);
/* the PS2's libm (third_party/newlib_libm) and libc */
f32 sinf(f32);
f32 cosf(f32);
f32 asinf(f32);
f32 acosf(f32);
f32 atan2f(f32, f32);
f32 sqrtf(f32);
int rand(void);
/* game code */
void func_003EC320(f32 *out, const f32 *in);
void func_003ECA38(void *b, f32 t);
void func_003ECA80(void *b, f32 t);
f32 func_003ECD00(void *b, s32 k, f32 t);
void func_003ECDE8(void *b, f32 *n, f32 *pt);
void func_003ECF70(void *b);
s32 func_003ED118(void *b, s32 k, f32 t);
f32 func_003ED268(void *b, s32 k, f32 t);
void func_003D10D0(s32 i, f32 *m, f32 scale);
void func_00353C88(s32 i, f32 *out);
void func_003F3108(void);
void func_003B7AC8(void);
void Snd_PlaySe(s32 id, s32 player);
void Snd_PlayVoice(s32 player, s32 set, u16 code);
void Bgm_SetVolumeFade(s32 volume, s32 time);
s32 func_003EAE08(void *f, s16 *out);
u16 func_00364088(s32 i);
void func_003EA868(s32 x, s32 y, s32 full);
void func_003E1B40(void);
void func_003E1BB0(void);
void func_003BB228(u8 side, u8 kind);
s32 func_003E96D8(void *f, s32 mv);
s32 Text_Printf(const char *fmt, ...);

/* game data */
extern f32 D_01FC61A0[16]; /* the bowl's camera matrix */
extern f32 D_01FC6160[16];
extern u8 D_01FC51B0[0x30]; /* the camera target: position, +0x10/+0x14 angles, +0x20 distance, +0x24 state */
extern f32 D_01FC51C0[4];
extern f32 D_01FC51E8;
extern u8 D_01FC51E0[0x80]; /* the ball */
extern s32 D_01FC51D4;      /* D_01FC51B0 + 0x24 */
extern u8 D_01FC5260[];     /* the ten pins, 0x180 bytes each */
extern f32 D_00523570[4];
extern u8 D_00523960[16];
extern s16 D_01FC6340[6];
extern u8 D_005237B0[];
extern s32 D_00415808[8];
extern u8 D_01FC6350[0x14]; /* the bowler's move ids (+0xC ...) */
extern u8 D_01FC635C, D_01FC6360, D_01FC6362;
extern u8 D_005236A0[];
extern s32 D_005F6998[2]; /* the two style names */
extern s32 D_005F69A0[2]; /* their x positions */
extern s32 D_005F69A8;
extern s8 D_005F69B0[2];
extern u8 D_0043B7FA;
extern s32 D_005F5F08;
extern u16 gFrameCount;
extern s16 D_005F6988, D_005F698A, D_005F70A8, D_005F70AA, D_005F70AE, D_005F70B6, D_005F70BC, D_005F70BE,
    D_005F70C0, D_005F70CE, D_005F70D4;
extern u16 D_005F70AC;
extern s32 D_005F70DC, D_005F70E0;
extern const char D_005238E0[], D_005238F0[], D_00523908[];

#define PI W(0x40490FDBu)
#define TWO_PI W(0x40C90FDBu)

/* The fold into [-pi, pi] the bowl code repeats inline (D_005F5A40 ...: pi, 2 pi, -pi, 2 pi). */
static f32 wrap(f32 a) {
    if (PI < a) {
        a = a - TWO_PI;
    } else if (a < -PI) {
        a = a + TWO_PI;
    }
    return a;
}

/* func_003EC388 (0x3EC388): the body's angles a (x, y, z, w) turned by the rates b, through the matrix of the folded
 * angles on VU0 (vf4..vf7, from D_01FC61A0); a[3] gets the heading of its z axis. */
void func_003EC388(f32 *a, f32 *b) {
    VEC sp[4];
    f32 c, s, f20, f22, f23, f24, f25, f0;

    func_003EC320(sp, a);
    a[1] = a[1] + b[1];
    c = cosf(sp[1]);
    s = sqrtf(1.0f - c * c);
    if (sp[1] < 0.0f) {
        s = -s;
    }
    f20 = sinf(b[0]) * c;
    f25 = f20 - sinf(b[2]) * s;
    f20 = sinf(b[2]) * c;
    f23 = f20 + sinf(b[0]) * s;
    a[0] = a[0] + asinf(f25);
    Port_Vu0BowlLoadMtx(D_01FC61A0);
    func_003E8FA0(wrap(a[0]));
    Port_Vu0BowlRotX();
    func_003E8FA0(asinf(f23));
    Port_Vu0BowlRotZ();
    c = cosf(sp[0]);
    s = sqrtf(1.0f - c * c);
    if (sp[0] < 0.0f) {
        s = -s;
    }
    f24 = s * f23;
    f22 = c * f23;
    f0 = asinf(f25);
    f20 = wrap(sp[0] + f0);
    f0 = asinf(f24 * f22);
    a[0] = a[0] + f0;
    if (c != 0.0f && f23 != 0.0f) {
        f32 r;

        if (0.0f <= f20) {
            r = -atan2f(f22, sinf(f20));
        } else {
            r = atan2f(f22, -sinf(f20));
        }
        a[1] = a[1] + r;
        func_003E8FA0(-r);
        Port_Vu0BowlRotY();
    }
    func_003E8FA0(-wrap(a[0]));
    Port_Vu0BowlRotX();
    sp[3] = 0.0f;
    sp[2] = 1.0f;
    sp[1] = 0.0f;
    sp[0] = 0.0f;
    Port_Vu0BowlXform(sp, sp);
    if (sp[0] != 0.0f || sp[2] != 0.0f) {
        a[3] = a[3] + atan2f(sp[0], sp[2]);
    }
}

/* func_003ED338 (0x3ED338): one step of a pin (BowlBody): the first corner (or the edge, k = 4) to touch the floor
 * within the step, the step up to that contact, the bounce and friction, then the rest of the step. */
void func_003ED338(u8 *p) {
    VEC sp0[4] = {0}, sp10[4] = {0}, sp20[4] = {0};
    f32 t = 1.0f, f0, f1, f2, f21 = 0.0f;
    s32 k = -1, i, hit = 0;

    for (i = 0; i < 4; i++) {
        f1 = F32(p, 0xC4 + i * 16) + D_00523570[i];
        f2 = F32(p, 0x104 + i * 16);
        if (0.0f <= f1) {
            f1 = 0.0f;
        } else {
            f1 = (sqrtf(f2 * f2 - f1 * W(0x411CCCCDu)) - f2) / W(0x409CCCCDu);
        }
        if (!(f1 < 1.0f)) {
            continue;
        }
        if (!(f1 <= t)) {
            continue;
        }
        t = f1;
        if (i == 2) {
            f32 f20 = F32(p, 0x60) + F32(p, 0x70) * t;

            f0 = asinf(W(0x3D6A0EA1u)) + W(0x3FC90FDBu) - W(0x3BA3D70Au);
            k = 4;
            if (f0 < f20) {
                continue;
            }
            f20 = F32(p, 0x60) + F32(p, 0x70) * t;
            f0 = -(asinf(W(0x3D6A0EA1u)) + W(0x3FC90FDBu)) + W(0x3BA3D70Au);
            if (f20 < f0) {
                continue;
            }
        }
        k = i;
    }
    if (k < 0) {
        f1 = F32(p, 0x4);
        goto rest;
    }
    if (k == 4) {
        func_003E92C0(sp0, (f32 *)(p + 0x20), (f32 *)(p + 0x30), t);
        Q16(sp10, p + 0x30);
        goto step;
    }
    func_003E92C0(sp0, (f32 *)(p + 0xC0 + k * 16), (f32 *)(p + 0x100 + k * 16), t);
    sp0[1] = sp0[1] + D_00523570[k];
    if (k == 3) {
        f32 f20 = W(0x459C4000u), f3, x, y, z, xx, zz;

        func_003E92C0(sp20, (f32 *)(p + 0x150), (f32 *)(p + 0x160), t);
        sceVu0SubVector(sp20, sp0, sp20);
        f3 = sp20[0] * sp20[0] + sp20[1] * sp20[1] + sp20[2] * sp20[2];
        if (f3 < f20) {
            sceVu0SubVector(sp20, (f32 *)(p + 0xC0), (f32 *)(p + 0x150));
            hit = 1;
            x = sp20[0];
            y = sp20[1];
            z = sp20[2];
            xx = x * x;
            zz = z * z;
            f21 = sqrtf(f20 / (xx + y * y + zz));
            f3 = xx + zz;
            if (f3 <= 0.0f) {
                Q16(sp0, p + 0x150);
            } else {
                f3 = sqrtf(f3);
                sp10[1] = f3;
                sp10[0] = -x * y / f3;
                sp10[2] = -z * y / f3;
                func_003E92C0(sp0, (f32 *)(p + 0x150), sp10, f21);
            }
            if (sp0[1] <= 0.0f) {
                sceVu0SubVector(sp20, (f32 *)(p + 0x80), (f32 *)(p + 0x140));
                x = sp20[0];
                z = sp20[2];
                f3 = x * x + z * z;
                if (f3 <= 0.0f) {
                    Q16(sp10, p + 0x140);
                } else {
                    f3 = sqrtf(f3);
                    y = sp20[1];
                    sp10[1] = f3;
                    sp10[0] = -x * y / f3;
                    sp10[2] = -z * y / f3;
                    func_003E92C0(sp10, (f32 *)(p + 0x140), sp10, f21);
                }
                sceVu0SubVector(sp10, sp10, sp0);
                f2 = sp10[1];
                t = (sqrtf(f2 * f2 - sp0[1] * W(0x411CCCCDu)) - f2) / W(0x409CCCCDu);
                if (1.0f <= t) {
                    f1 = F32(p, 0x4);
                    goto rest;
                }
                func_003E92C0(sp0, sp0, sp10, t);
            }
        }
    }
    Q16(sp10, p + 0x100 + k * 16);
step:
    func_003ECA38(p, t);
    f0 = t * W(0x401CCCCDu) * t;
    f1 = t * W(0x409CCCCDu);
    sp10[1] = sp10[1] + f1;
    sp0[1] = sp0[1] + f0;
    F32(p, 0x4) = F32(p, 0x4) + f0;
    F32(p, 0x14) = F32(p, 0x14) + f1;
    if (0.0f < sp10[1]) {
        if (k == 4) {
            sp10[1] = sp10[1] * W(0xC0DAE147u);
        } else {
            f0 = -func_003ECD00(p, k, t) * W(0x3FE66666u);
            sp10[1] = sp10[1] * f0;
        }
        sp10[0] = 0.0f;
        sp10[2] = 0.0f;
        func_003ECDE8(p, sp10, sp0);
    }
    t = 1.0f - t;
    if (!(0.0f < func_003ED268(p, k, t))) {
        goto last;
    }
    if (k == 4) {
        f0 = t * W(0xC194F5C3u) * W(0x3F4CCCCDu);
    } else {
        f0 = -func_003ECD00(p, k, 1.0f - t) * W(0x409CCCCDu) * t * W(0x3F4CCCCDu);
    }
    sp10[1] = f0;
    sp10[2] = 0.0f;
    sp10[0] = 0.0f;
    func_003ECDE8(p, sp10, sp0);
    F32(p, 0x14) = 0.0f;
    if (k == 4 || func_003ED118(p, k, t) != 0) {
        f32 x = F32(p, 0x10), z = F32(p, 0x18), r;

        r = 1.0f - t * (0.25f / sqrtf(x * x + z * z));
        if (r <= 0.0f) {
            F32(p, 0x10) = 0.0f;
            F32(p, 0x18) = 0.0f;
        } else {
            F32(p, 0x18) = z * r;
            F32(p, 0x10) = x * r;
        }
        r = 1.0f - t * (W(0x3A51B717u) / sqrtf(F32(p, 0x50) * F32(p, 0x50) + F32(p, 0x58) * F32(p, 0x58)));
        if (r <= 0.0f) {
            F32(p, 0x50) = 0.0f;
            F32(p, 0x58) = 0.0f;
            F32(p, 0x54) = 0.0f;
        } else {
            sceVu0ScaleVector((f32 *)(p + 0x50), (f32 *)(p + 0x50), r);
        }
    } else if (hit && F32(p, 0x40) < W(0x3BA3D70Au) && W(0xBBA3D70Au) < F32(p, 0x40)) {
        f32 x = F32(p, 0x10), z = F32(p, 0x18), r;

        r = 1.0f - t * (0.75f / sqrtf(x * x + z * z));
        if (r <= 0.0f) {
            F32(p, 0x10) = 0.0f;
            F32(p, 0x18) = 0.0f;
        } else {
            F32(p, 0x18) = z * r;
            F32(p, 0x10) = x * r;
        }
        r = 1.0f - t * (W(0x3A51B717u) / sqrtf(F32(p, 0x50) * F32(p, 0x50) + F32(p, 0x54) * F32(p, 0x54) +
                                               F32(p, 0x58) * F32(p, 0x58)));
        if (r <= 0.0f) {
            F32(p, 0x50) = 0.0f;
            F32(p, 0x40) = 0.0f;
            F32(p, 0x58) = 0.0f;
            F32(p, 0x54) = 0.0f;
        } else {
            sceVu0ScaleVector((f32 *)(p + 0x50), (f32 *)(p + 0x50), r);
        }
    }
last:
    func_003ECA80(p, t);
    f1 = t * W(0x401CCCCDu) * t;
    f2 = t * W(0x409CCCCDu);
    F32(p, 0x14) = F32(p, 0x14) + f2;
    F32(p, 0x4) = F32(p, 0x4) + f1;
    func_003ECAD8(p);
    func_003ECF70(p);
    if (!hit) {
        S32(p, 0x170) = 0;
    }
    return;
rest: /* no contact in this step: fall freely */
    F32(p, 0x4) = f1 + W(0x401CCCCDu);
    F32(p, 0x14) = F32(p, 0x14) + W(0x409CCCCDu);
    func_003ECAD8(p);
}

/* func_003EEF00 (0x3EEF00): the draw matrices of the ball (models 0 and 2, while it rolls in front of the pins) and
 * of the standing pins (models 1 and 3), from their positions and folded angles. */
void func_003EEF00(void) {
    VEC m[16], m40[16], v80[4], v90[4];
    u8 *s1 = D_01FC51E0, *s2 = D_01FC5260;
    f32 f20, f12;
    s32 i;

    sceVu0CopyMatrix(m40, D_01FC61A0);
    func_00353C88(0, v80);
    S32(m40, 0x14) = 0;
    m40[4] = -v80[0] / v80[1];
    m40[6] = -v80[2] / v80[1];
    if (S32(s1, 0x74) != 0 && F32(s1, 0x8) < W(0x4644A400u)) {
        func_003EC320((f32 *)(s1 + 0x50), (f32 *)(s1 + 0x50));
        func_003E91D0(m, D_01FC61A0, (f32 *)(s1 + 0x50));
        m[13] = m[13] - F32(s1, 0x4);
        m[14] = m[14] - F32(s1, 0x8);
        m[12] = m[12] + F32(s1, 0x0);
        func_003D10D0(0, m, 1.0f);
        f12 = v80[0] * v80[0];
        f20 = sqrtf(f12 + v80[2] * v80[2]);
        f12 = sqrtf(f12 + v80[1] * v80[1] + v80[2] * v80[2]);
        v90[0] = -acosf(f20 / f12);
        v90[1] = acosf(v80[2] / f20);
        v90[2] = -F32(s1, 0x50);
        S32(v90, 0xC) = 0;
        func_003E9038(m, D_01FC61A0, v90);
        m[13] = m[13] - F32(s1, 0x4);
        m[14] = m[14] - F32(s1, 0x8);
        m[12] = m[12] + F32(s1, 0x0);
        sceVu0MulMatrix(m, m40, m);
        func_003D10D0(2, m, 1.0f);
    }
    for (i = 10; i > 0; i--, s2 += 0x180) {
        if (S32(s2, 0x174) == 0) {
            continue;
        }
        func_003EC320((f32 *)(s2 + 0x40), (f32 *)(s2 + 0x40));
        func_003E91D0(m, D_01FC6160, (f32 *)(s2 + 0x40));
        m[12] = m[12] + F32(s2, 0x0);
        m[13] = m[13] - F32(s2, 0x4);
        m[14] = m[14] - F32(s2, 0x8);
        func_003D10D0(1, m, 1.0f);
        sceVu0MulMatrix(m, m40, m);
        func_003D10D0(3, m, 1.0f);
    }
}

/* func_003EBC98 (0x3EBC98): the bowl camera's state machine (D_01FC51B0, state +0x24) on the bowler f, then the
 * camera position and target into D_00415808 (integers, negated as the game's camera keys). */
void func_003EBC98(u8 *f) {
    u8 *B = D_01FC51B0;
    VEC sp[4], sp10[4];
    f32 f0, f1, f20;

    switch ((uint32_t)S32(B, 0x24)) {
    case 0:
        S32(B, 0x24) = 7;
        S32(B, 0x0) = 0;
        F32(B, 0x4) = W(0xC50FC000u);
        F32(B, 0x8) = W(0xC63B8000u);
        F32(B, 0x20) = 2000.0f;
        S32(B, 0x14) = 0;
        break;
    case 1:
        F32(B, 0x4) = W(0xC512E000u);
        F32(B, 0x0) = (f32)S32(f, 0x4);
        F32(B, 0x8) = (f32)S32(f, 0xC);
        S32(B, 0x20) = 0;
        F32(B, 0x14) = (f32)S16(f, 0x1A) * PI * W(0x38000000u);
        break;
    case 2:
        f20 = (D_01FC51E8 - 4000.0f) - F32(B, 0x8);
        memcpy(sp10, D_00523960, 16);
        if (0.0f < f20) {
            sceVu0SubVector(sp, sp10, (f32 *)B);
            f20 = f20 / sp[2];
            if (f20 < 1.0f) {
                func_003E92C0((f32 *)B, (f32 *)B, sp, f20);
                f1 = 1.0f - f20;
                F32(B, 0x14) = F32(B, 0x14) * f1;
                F32(B, 0x20) = F32(B, 0x20) * f1;
            } else {
                S32(B, 0x24) = 7;
                Q16(B, sp10);
                F32(B, 0x20) = 0.0f;
                F32(B, 0x14) = 0.0f;
            }
        }
        break;
    case 3: {
        s32 a1, v1;
        f32 r, f3, f2;

        S32(B, 0x0) = 0;
        F32(B, 0x4) = W(0xC3910000u);
        F32(B, 0x8) = 8000.0f;
        F32(B, 0x20) = 4000.0f;
        S32(B, 0x24) = 7;
        r = (f32)(rand() & 0xFFFFFF);
        a1 = D_005F70B6;
        F32(B, 0x10) = ((f32)D_01FC6340[a1 % 3] + r * W(0x33800000u)) * W(0x3E860A92u) + W(0xBF61307Au);
        if (a1 < 3) {
            r = (f32)(rand() & 0xFFFFFF);
            v1 = D_005F70B6;
            f3 = W(0x3F060A92u);
            f2 = W(0xBF490FDBu);
            f1 = r * W(0x33800000u);
            f0 = (f32)D_01FC6340[v1 + 3];
        } else {
            r = (f32)(rand() & 0xFFFFFF);
            v1 = D_005F70B6 + (a1 < 6 ? 1 : 2);
            f3 = W(0x3F060A92u);
            f1 = r * W(0x33800000u);
            f2 = W(0xBF490FDBu);
            f0 = (f32)D_01FC6340[v1 % 3 + 3];
        }
        F32(B, 0x14) = (f0 + f1) * f3 + f2;
        break;
    }
    case 4:
        F32(B, 0x0) = F32(f, 0x590);
        F32(B, 0x4) = -F32(f, 0x594) - 500.0f;
        S32(B, 0x24) = S32(B, 0x24) + 1;
        F32(B, 0x14) = PI;
        F32(B, 0x20) = W(0x455AC000u);
        S32(B, 0x10) = 0;
        F32(B, 0x8) = -F32(f, 0x598);
        break;
    case 5: {
        s16 mv = S16(f, 0x2A);

        if (mv == 6 || (mv == 15 && D_005237B0[S16(f, 0x28)] == 6)) {
            f32 k = W(0x3F7851ECu), l = W(0x3CF5C28Fu);

            F32(B, 0x0) = F32(B, 0x0) * k + F32(f, 0x590) * l;
            F32(B, 0x4) = F32(B, 0x4) * k - (F32(f, 0x594) + 500.0f) * l;
            F32(B, 0x8) = F32(B, 0x8) * k - F32(f, 0x598) * l;
        } else {
            F32(B, 0x0) = (F32(B, 0x0) + F32(f, 0x590)) * 0.5f;
            F32(B, 0x4) = (F32(B, 0x4) - F32(f, 0x594) - 500.0f) * 0.5f;
            F32(B, 0x8) = (F32(B, 0x8) - F32(f, 0x598)) * 0.5f;
        }
        break;
    }
    default:
        break;
    }
    sp[0] = 0.0f;
    sp[3] = 0.0f;
    sp[2] = 0.0f;
    sp[1] = 0.0f;
    func_003E9120(D_01FC61A0, D_01FC51C0, sp);
    sp[2] = -F32(B, 0x20);
    Port_Vu0BowlXform(sp, sp);
    Port_Vu0BowlAdd13(sp, (f32 *)B);
    D_00415808[5] = (s32)sp[0];
    D_00415808[6] = (s32)sp[1];
    D_00415808[7] = (s32)sp[2];
    sceVu0ScaleVector(sp, D_01FC51C0, W(0x4422F983u));
    D_00415808[0] = -(s32)sp[0];
    D_00415808[1] = -(s32)sp[1];
    D_00415808[2] = -(s32)sp[2];
}

/* The angle of a fighter's s16 heading in radians as the bowl code converts it (0x8000 and above as negative). */
static f32 heading(u8 *f, uint32_t k1, uint32_t k2) {
    u16 v = U16(f, 0x1A);

    if (v <= 0x7FFF) {
        return W(k1) * (f32)v;
    }
    return (f32)(s32)(v + 0xFFFF0000u) * W(k2);
}

/* The bowler's next move id when the current one has run out (D_005F70D4 frames past its length). */
static int move_done(s16 mv) {
    s16 n = D_005F70D4;

    return (s32)func_00364088(mv) < n;
}

/* func_003EAF78 (0x3EAF78): the bowler's throw, one frame: aim (left / right on pad bits 0x2000 / 0x8000), the power
 * meter, the throw (the ball's start velocity and spin into D_01FC51E0) and the style menu; st[0] is the state. */
void func_003EAF78(u8 *f, s16 *st, int pad_held, int pad_new) {
    s16 held = (s16)pad_held, now = (s16)pad_new;
    u8 *H = D_01FC6350;
    s32 a2 = D_005F69A8;
    u8 t0;

    if (a2 > 0) {
        a2 = a2 - 1;
        D_005F69A8 = a2;
    }
    switch ((u16)st[0]) {
    case 0:
        t0 = D_01FC635C;
        D_005F70D4 = 1;
        D_005F6988 = t0;
        st[1] = 1;
        st[0] = (u16)st[0] + 1;
        a2 = 2;
        D_005F69A8 = 2;
        /* fallthrough */
    case 1:
        D_01FC51D4 = 0;
        if (a2 <= 0) {
            if (now & 0x800) {
                func_003F3108();
                D_0043B7FA = 0;
                D_005F5F08 = 1 << D_005F70AC;
                func_003B7AC8();
                D_005F70A8 = 9;
                goto quit;
            }
            if (now & 0x10) {
                D_005F70AE = 1;
                D_005F70A8 = 9;
            quit:
                Snd_PlaySe(4, 0);
                Bgm_SetVolumeFade(0x40, 0x78);
                return;
            }
        }
        if ((now & 0x100) && func_003EAE08(f, st) != 0) {
            return;
        }
        if (held & 0x2000) {
            S32(f, 0x4) = S32(f, 0x4) + 8;
            if (S32(f, 0x4) >= 0x3E9) {
                S32(f, 0x4) = 0x3E8;
            }
        }
        if (held & 0x8000) {
            S32(f, 0x4) = S32(f, 0x4) - 8;
            if (S32(f, 0x4) < -0x3E8) {
                S32(f, 0x4) = -0x3E8;
            }
        }
        if (held & 0x2000) {
            s16 mv = D_005F6988;

            if (mv == H[0xC] || (mv == H[0xD] && move_done(mv))) {
                D_005F70D4 = 1;
                D_005F6988 = H[0xE];
            }
        } else if (held & 0x8000) {
            s16 mv = D_005F6988;

            if (mv == H[0xC] || (mv == H[0xE] && move_done(mv))) {
                D_005F70D4 = 1;
                D_005F6988 = H[0xD];
            }
        } else {
            if ((D_005F6988 == H[0xE] && move_done(H[0xD])) || (D_005F6988 == H[0xD] && move_done(D_005F6988))) {
                D_005F70D4 = 1;
                D_005F6988 = H[0xC];
            }
        }
        func_003EA868(0x14, 0x14, 0);
        if (now & 0x40) {
            st[1] = 2;
            st[0] = (u16)st[0] + 1;
            func_003E1B40();
        }
        break;
    case 2: {
        f32 f1, f2, lim;

        D_01FC51D4 = 1;
        D_005F6988 = H[0xF];
        D_005F70D4 = 0x6A;
        if (held & 0x2000) {
            S16(f, 0x1A) = (u16)S16(f, 0x1A) + 0x10;
            if (S16(f, 0x1A) >= 0x801) {
                S16(f, 0x1A) = 0x800;
            }
            S16(f, 0x3C) = S16(f, 0x1A);
        }
        if (held & 0x8000) {
            S16(f, 0x1A) = (u16)S16(f, 0x1A) - 0x10;
            if (S16(f, 0x1A) < -0x800) {
                S16(f, 0x1A) = -0x800;
            }
            S16(f, 0x3C) = S16(f, 0x1A);
        }
        f2 = F32(st, 0x8);
        f1 = F32(st, 0x4) + f2;
        F32(st, 0x4) = f1;
        if (S16(f, 0x28) == 0x10 || S16(f, 0x28) == 0x13) {
            if (W(0x3F99999Au) < f1) {
                goto bounce;
            }
            lim = W(0xBF99999Au);
        } else if (H[0x12] != 0) {
            if (2.0f < f1) {
                goto bounce;
            }
            lim = -1.0f;
        } else {
            if (1.0f < f1) {
                goto bounce;
            }
            lim = -2.0f;
        }
        if (f1 < lim) {
        bounce:
            F32(st, 0x8) = -f2;
        }
        if (now & 0x20) {
            D_005F6988 = H[0xC];
            S32(st, 0x4) = 0;
            st[1] = 1;
            st[0] = (u16)st[0] - 1;
            func_003E1BB0();
        } else if (now & 0x40) {
            st[1] = 3;
            st[0] = (u16)st[0] + 1;
            func_003E1BB0();
        }
        break;
    }
    case 3:
        D_01FC51D4 = 7;
        D_005F6988 = H[0xF];
        if (F32(st, 0xC) < 100.0f) {
            F32(st, 0xC) = F32(st, 0xC) + F32(st, 0x10);
        } else {
            S32(st, 0xC) = 0;
        }
        if (now & 0x40) {
            int go = 0.0f < F32(st, 0xC);

            if (!go) {
                f32 r = (f32)(rand() & 0xFFFFFF) * W(0x33800000u) * 2000.0f;

                go = F32(st, 0x14) < (f32)(s32)r;
            }
            if (go) {
                st[0] = 4;
                D_005F70D4 = 1;
                D_005F6988 = H[0xF];
            } else {
                D_005F698A = 0;
                D_005F70CE = 0x3C;
                st[0] = 5;
            }
        }
        break;
    case 4: {
        u8 *ball = D_01FC51E0;
        f32 aim, pow, c, s, f0, f1, f2, f3;
        s16 player, set;
        int r;

        S32(D_01FC51B0, 0x24) = 7;
        if (held & 0x2000) {
            S32(f, 0x4) = S32(f, 0x4) + 0x500 / (s32)F32(st, 0x14);
            if (S32(f, 0x4) >= 0x3E9) {
                S32(f, 0x4) = 0x3E8;
            }
        } else if (held & 0x8000) {
            S32(f, 0x4) = S32(f, 0x4) - 0x500 / (s32)F32(st, 0x14);
            if (S32(f, 0x4) < -0x3E8) {
                S32(f, 0x4) = -0x3E8;
            }
        }
        if (D_005F70D4 != 0x6A) {
            break;
        }
        f0 = (f32)(rand() & 0xFFFFFF) * W(0x33800000u);
        f0 = f0 + f0;
        f0 = f0 - 1.0f;
        aim = F32(st, 0x4) + F32(st, 0x8) * f0;
        f0 = (f32)(rand() & 0xFFFFFF) * W(0x33800000u);
        f0 = f0 + f0;
        f0 = f0 - 1.0f;
        pow = F32(st, 0xC) + F32(st, 0x10) * f0;
        pow = F32(st, 0x14) * (pow + 100.0f) / 200.0f;
        c = cosf(heading(f, 0x38C90FDBu, 0x38C90FDBu));
        if (S16(f, 0x1A) >= 0) {
            s = sqrtf(1.0f - c * c);
        } else {
            s = -sqrtf(1.0f - c * c);
        }
        f0 = -aim;
        f3 = pow * s;
        f1 = pow * c;
        f0 = f0 * s;
        f2 = aim * c;
        F32(ball, 0x10) = f3;
        F32(ball, 0x18) = f1;
        F32(ball, 0x28) = f0;
        F32(ball, 0x20) = f2;
        S32(ball, 0x70) = 1;
        F32(ball, 0x60) = pow * -0.25f / 200.0f;
        F32(ball, 0x68) = aim * 0.5f;
        F32(ball, 0x54) = (f32)S16(f, 0x1A) * PI * W(0x38000000u);
        player = S16(f, 0x1E);
        set = S16(f, 0x2C);
        r = rand();
        Snd_PlayVoice(player, set, U16(D_005236A0, ((r >> 15) & 2) + S16(f, 0x2A) * 8));
        S32(D_01FC51B0, 0x24) = 2;
        st[1] = 4;
        st[0] = 7;
        if (D_005F70BC == 0) {
            func_003BB228((u8)D_005F70AC, 5);
        }
        break;
    }
    case 5:
        if (D_005F70CE > 0) {
            s32 i = D_005F698A;

            D_005F6988 = D_01FC635C;
            D_005F70CE = (u16)D_005F70CE - 1;
            Text_Printf(D_005238E0, D_005F69A0[i], 0xF0, 0xA, 8, (gFrameCount & 2) ? 0xC : 0x18, D_005F6998[i]);
        } else {
            D_005F70DC = S32(f, 0x4);
            D_005F70E0 = S32(f, 0xC);
            D_005F6988 = D_01FC6360;
            D_005F70D4 = 1;
            D_005F70C0 = 1;
            st[0] = 6;
            D_01FC51D4 = 2;
        }
        break;
    case 6: {
        s32 n = D_005F70D4 - 0x19, v1;
        f32 tn;

        D_005F6988 = H[0x10];
        if (n == 0) {
            Snd_PlayVoice(S16(f, 0x1E), S16(f, 0x2C), U16(D_005236A0, S16(f, 0x2A) * 8 + 4));
        }
        if (n > 0) {
            f32 sn = sinf(heading(f, 0x38C90FDBu, 0x38C90FDBu));

            tn = sn / cosf(heading(f, 0x38C90FDBu, 0x38C90FDBu));
            v1 = n << 8;
            if (n >= 0x19) {
                if (n < 0x32) {
                    v1 = (n << 8) - (n - 0x19) * (n - 0x19);
                } else if (n < 0x4B) {
                    s32 d = n - 0x32;

                    v1 = d * 0xCE - (d * (d * 4) - 0x2F8F);
                } else {
                    v1 = 0x39E9;
                }
            }
            S32(f, 0xC) = D_005F70E0 + v1;
            S32(f, 0x4) = D_005F70DC + (s32)((f32)v1 * tn);
        }
        if (move_done(D_005F6988)) {
            D_005F70D4 = 1;
            D_005F6988 = H[0x11];
            st[0] = 7;
        }
        break;
    }
    case 7:
        if (D_005F6988 == H[0xF] && move_done(D_005F6988)) {
            D_005F70D4 = 1;
            D_005F6988 = H[0xC];
            st[0] = 7;
        }
        break;
    default:
        break;
    }
    if (now != 0 && D_005F70AA >= 0x259) {
        D_005F70AA = 0x258;
    }
    if (st[0] < 4 && D_005F70AA >= 0x4651) {
        if (st[0] == 2) {
            func_003E1BB0();
        }
        D_005F70CE = 0x3C;
        D_005F698A = 1;
        st[0] = 5;
    }
    func_003E96D8(f, D_005F6988);
    if (D_005F70BE > 0) {
        u8 sp20[2];

        D_005F70BE = (u16)D_005F70BE - 1;
        sp20[0] = (u8)D_005F69B0[0];
        sp20[1] = (u8)D_005F69B0[1];
        Text_Printf(D_005238F0, 0xEF, 0x16C, 0xA, 4, 0xF, (s32)D_005F69B0[1]);
        Text_Printf(D_00523908, 0xAE, 0x180, 0xA, 5, 0xF, (s32)sp20[D_01FC6362]);
    }
    D_005F70AA = (u16)D_005F70AA + 1;
}
