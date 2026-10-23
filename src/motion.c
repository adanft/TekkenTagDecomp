/* 0x3639F0-0x364448 (module motion, docs/game_overview.md section 4; docs/systems/animation.md): motion data. Motion_Sample
 * decodes one frame of a fighter motion (Motion_DecodeFrame: raw first and last frames, then per channel a delta /
 * run-length bit stream for each block of 32 frames) or reads it from a raw motion bank (Motion_LoadBanks and the clip
 * accessors). The range is a cut inside the fighter_setup range; its .rodata (0x4FC610-0x4FC630, the decoder's jump
 * table) is a subsegment, included in place while the decoder is assembly. Every NON_MATCHING draft (#if 0) compiles
 * when enabled. */
#include "common.h"
#include "fighter.h"

/* A clip of a motion bank (func_00364528 returns one): a frame count, the number of 3-channel groups per frame, then
 * frames x groups x 3 s16 values. */
typedef struct {
    /* 0x0 */ u16 unk0;                       /* frames */
    /* 0x2 */ u16 unk2;                       /* 3-channel groups per frame */
    /* 0x4 */ s16 unk4[1];
} MotionClip;

extern Fighter gFighters[4];
extern s32 D_005F5E30;
extern s32 D_005F5E34;
extern s32 D_005F5E38;
extern s32 D_005F5E3C;
extern s32 D_005F5E40;
extern s32 D_005F5E44;
extern void *D_005F6B74;
extern void *D_005F6B78;
extern void *D_005F6B7C;

void exit(s32 code) __attribute__((noreturn));
u8 func_003A9520(void);
s32 Motion_DecodeFrame(u8 *m, s16 *out, s32 frame, s32 n);
void func_003640B0(s16 *out, s32 frame, s32 i, s32 n);
void Motion_CopyFrame(s16 *out, s32 frame, s32 i, s32 n, u16 *p);
MotionClip *func_00364248(s32 k, s32 frame, void *bank);
void *func_00364528(s32 i, void *bank);
s32 func_00364550(void *bank);
void *func_003B8810(void);
u32 func_003B8818(void);
u32 func_0035CFD0(void *dst, s32 src, u32 cap);
MotionClip *func_003672D0(s32 i);
MotionClip *func_003672F0(s32 i);

/* One frame of a motion: kind 1 the 3 root channels, else all 57; from motion bank 0 while D_005F6254 is set, else
 * decoded. Returns 0. */
s32 Motion_Sample(u8 *m, s16 *out, s32 frame, s32 kind) {
    s32 n = kind == 1 ? 3 : 0x39;

    if (func_003A9520()) {
        func_003640B0(out, frame, 0, n);
        return 0;
    }
    Motion_DecodeFrame(m, out, frame, n);
    return 0;
}

/* Motion scale (0x1000 = 1.0) of the root channels: unk4FC, or the other fighter's while unkE0 < 0. */
s32 Motion_GetScale(Fighter *f) {
    if (f->unkE0 < 0) {
        if (f == &gFighters[0]) {
            return gFighters[1].unk4FC;
        }
        return gFighters[0].unk4FC;
    }
    return f->unk4FC;
}

/* The root channels of a motion frame, height and distance scaled by Motion_GetScale (the >> 13 branch is dead:
 * Motion_Sample returns 0). */
void Motion_SampleRoot(u8 *m, s16 *out, s32 frame, Fighter *f) {
    s32 scale = Motion_GetScale(f);
    s32 y;
    s32 y2;

    if (Motion_Sample(m, out, frame, 1)) {
        y = out[1] * scale / 0x2000;
        out[2] = out[2] * scale / 0x2000;
        out[1] = y;
        if ((s16)y < 0) {
            if ((s16)y >= -9) {
                out[1] = 0;
            }
        }
        if (out[2] < 0) {
            if (out[2] >= -9) {
                out[2] = 0;
            }
        }
    } else {
        y2 = out[1] * scale / 0x1000;
        out[2] = (out[2] * scale / 0x1000) & 0x7FFF;
        out[1] = y2;
    }
}

/* The whole pose of a motion frame. */
void Motion_SamplePose(u8 *m, s16 *out, s32 frame) {
    Motion_Sample(m, out, frame, 2);
}

/* As Motion_SampleRoot for kind 3; decodes the frame twice. */
void func_00363BD8(u8 *m, s16 *out, s32 frame, Fighter *f) {
    s32 scale = Motion_GetScale(f);
    s32 y;
    s32 y2;

    Motion_Sample(m, out, frame, 3);
    if (Motion_Sample(m, out, frame, 3)) {
        y = out[1] * scale / 0x2000;
        out[2] = out[2] * scale / 0x2000;
        out[1] = y;
        if ((s16)y < 0) {
            if ((s16)y >= -9) {
                out[1] = 0;
            }
        }
        if (out[2] < 0) {
            if (out[2] >= -9) {
                out[2] = 0;
            }
        }
    } else {
        y2 = out[1] * scale / 0x1000;
        out[2] = (out[2] * scale / 0x1000) & 0x7FFF;
        out[1] = y2;
    }
}

INCLUDE_RODATA("asm/nonmatchings/motion", jtbl_004FC610);
/* NON_MATCHING: same instructions, 59 lines of register allocation (the decoder's state in other registers). */
/* Decodes frame `frame` of motion m into n channels: m[0] frames, raw first frame at +2, raw last frame at +0x74, block
 * offsets at +0xE6; a frame in between starts from the first frame and applies ((frame - 1) & 31) + 1 steps of its
 * 32-frame block: per channel a 5-bit stream length, then 4-bit codes (0 none, 1-6 biased deltas of 6/4/2 bits, 7 a raw
 * 12-bit value, +8 repeated) shifted left by 2 (channels 1, 2) or 4. */
#if 0
s32 Motion_DecodeFrame(u8 *m, s16 *out, s32 frame, s32 n) {
    s32 len = 0;
    s32 base = 0;
    s32 mask = 0;
    s32 code = 0;
    s16 *o = out;
    u16 *p;
    u16 *next;
    s32 c;
    s32 k;
    s32 steps;
    s32 bits;
    s32 word;
    s32 shift;
    s32 run;
    s32 i;
    s16 val;

    if (frame + 1 >= m[0]) {
        p = (u16 *)(m + 0x74);
        for (c = 0; c < n; c++) {
            *o++ = *p++;
        }
    } else {
        p = (u16 *)(m + 2);
        for (c = 0; c < n; c++) {
            *o++ = *p++;
        }
        if (frame != 0) {
            k = frame - 1;
            o = out;
            steps = (k & 0x1F) + 1;
            bits = 0;
            word = 0;
            c = 0;
            p = (u16 *)(m + ((u16 *)(m + 0xE6))[k / 32] * 2);
            for (c = 0; c < n; c++) {
                next = p;
                if (bits < 5) {
                    word = *p++;
                    bits = 16;
                }
                shift = (u32)(c - 1) <= 1 ? 2 : 4;
                next += word & 0x1F;
                word >>= 5;
                bits -= 5;
                val = *o;
                run = 0;
                for (i = 0; i < steps; i++) {
                    if (bits < 16) {
                        word += *p++ << bits;
                        bits += 16;
                    }
                    if (run == 0) {
                        code = word & 0xF;
                        bits -= 4;
                        word >>= 4;
                        if (code >= 8) {
                            run = (word + 1) & 0x1F;
                            bits -= 5;
                            word >>= 5;
                            code -= 8;
                        }
                    } else {
                        run--;
                    }
                    switch (code) {
                    case 0:
                        continue;
                    case 1:
                        base = -0x54;
                        goto six;
                    case 2:
                        base = -0x14;
                        goto four;
                    case 3:
                        base = -4;
                        goto two;
                    case 4:
                        base = 1;
                    two:
                        len = 2;
                        mask = 3;
                        break;
                    case 5:
                        base = 5;
                    four:
                        len = 4;
                        mask = 0xF;
                        break;
                    case 6:
                        base = 0x15;
                    six:
                        len = 6;
                        mask = 0x3F;
                        break;
                    case 7:
                        if (bits < 12) {
                            word += *p++ << bits;
                            bits += 16;
                        }
                        bits -= 12;
                        val = (word & 0xFFF) << shift;
                        word >>= 12;
                        continue;
                    }
                    bits -= len;
                    val += (base + (word & mask)) << shift;
                    word >>= len;
                }
                *o++ = val;
                p = next;
                bits = 0;
                word = 0;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/motion", Motion_DecodeFrame);
#endif

/* Decompresses the motion bank archive into its buffer (exit(0) if it does not fit or is empty) and takes banks 0-2. */
void Motion_LoadBanks(s32 src) {
    void *buf = func_003B8810();
    u32 cap = func_003B8818();
    u32 n = func_0035CFD0(buf, src, cap);

    if (n >= cap) {
        exit(0);
    }
    if (n == 0) {
        exit(0);
    }
    D_005F6B74 = func_00364528(0, buf);
    D_005F6B78 = func_00364528(1, buf);
    D_005F6B7C = func_00364528(2, buf);
    D_005F5E30 = func_00364550(D_005F6B74);
    D_005F5E34 = func_00364550(D_005F6B78);
    D_005F5E38 = func_00364550(D_005F6B7C);
    D_005F5E3C = 0;
}

void func_00364080(s32 i) {
    D_005F5E3C = i;
}

u16 func_00364088(s32 i) {
    return *(u16 *)func_00364528(i, D_005F6B74);
}

/* Frame `frame` of clip D_005F5E3C of bank 0, channels i..n. */
void func_003640B0(s16 *out, s32 frame, s32 i, s32 n) {
    if (D_005F5E3C >= D_005F5E30) {
        D_005F5E3C = D_005F5E30 - 1;
    }
    Motion_CopyFrame(out, frame, i, n, func_00364528(D_005F5E3C, D_005F6B74));
}

void func_00364130(void *bank) {
    s32 n = func_00364550(bank);

    D_005F5E40 = (s32)bank;
    D_005F5E44 = n;
}

s32 func_00364160(void) {
    return D_005F5E44;
}

PORT_RET(u16) func_00364168(s32 i) {
    return *(u16 *)func_00364528(i, (void *)D_005F5E40);
}

MotionClip *func_00364190(s32 k, s32 frame) {
    if (k >= D_005F5E44) {
        k = D_005F5E44 - 1;
    }
    return func_00364248(k, frame, (void *)D_005F5E40);
}

/* NON_MATCHING: 5 lines: the original copies the start channel into the loop counter after the multiply. */
/* Copies channels i..n of frame `frame` (the last frame past the end) of a raw clip. */
#if 0
void Motion_CopyFrame(s16 *out, s32 frame, s32 i, s32 n, u16 *p) {
    if (frame >= p[0]) {
        frame = p[0] - 1;
    }
    p = (u16 *)((u8 *)p + ((frame * p[1] + i) * 6 + 4));
    for (; i < n; i += 3) {
        *out++ = *p++;
        *out++ = *p++;
        *out++ = *p++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/motion", Motion_CopyFrame);
#endif

/* Address of frame `frame` of clip k of a bank. */
MotionClip *func_00364248(s32 k, s32 frame, void *bank) {
    MotionClip *c = func_00364528(k, bank);

    if (frame >= c->unk0) {
        frame = c->unk0 - 1;
    }
    return (MotionClip *)((u8 *)c + (frame * c->unk2 * 6 + 4));
}

/* Frame unkB084 of the clip unkB088 (0: the character's own, from 50: bank 1), or NULL. */
s16 *func_00364298(Fighter *f) {
    s32 k = f->unkB088;
    MotionClip *c;
    s32 frame;

    if (k == 0) {
        c = func_003672F0(f->unk22);
        frame = f->unkB084;
        if (c != NULL) {
            if (frame >= c->unk0) {
                frame = c->unk0 - 1;
            }
            return (s16 *)((u8 *)c + (frame * c->unk2 * 6 + 4));
        }
    } else if (k >= 0x32) {
        return (s16 *)func_00364248(k - 0x32, f->unkB084, D_005F6B78);
    }
    return NULL;
}

/* Frame count of the clip unkB088, or 0. */
u16 func_00364330(Fighter *f) {
    s32 k = f->unkB088;
    MotionClip *c;

    if (k == 0) {
        c = func_003672F0(f->unk22);
    } else {
        if (k < 0x32) {
            return 0;
        }
        c = func_00364528(k - 0x32, D_005F6B78);
    }
    if (c != NULL) {
        return c->unk0;
    }
    return 0;
}

/* Frame unkB06C[j - 1] of the clip unkB074[j - 1] (0: the character's own, from 50: bank 2), or NULL. */
s16 *func_00364390(Fighter *f, s32 j) {
    s32 k;
    s32 frame;
    MotionClip *c;
    s32 n;

    j--;
    k = f->unkB074[j];
    frame = f->unkB06C[j];
    if (k == 0) {
        c = func_003672D0(f->unk22);
        if (c != NULL) {
            if (frame >= c->unk0) {
                frame = c->unk0 - 1;
            }
            return (s16 *)((u8 *)c + (frame * c->unk2 * 6 + 4));
        }
    } else if (k >= 0x32) {
        n = j - 1;
        return (s16 *)func_00364248((k - 0x32) * 2 - n, frame, D_005F6B7C);
    }
    return NULL;
}
