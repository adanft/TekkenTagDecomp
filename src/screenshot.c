/* 0x39BBF0-0x39C7B8 (module screenshot, docs/game_overview.md section 4): the end of the GTE-style helpers of
 * src/gte.c (geometry offset and screen distance, perspective transform, square root, normalise, cross product,
 * scale matrix) and the screenshot state machine (capture the frame into a JPEG and save it to the memory card). */
#include "common.h"
#include "gte.h"

/* D_008BF000: the screenshot state. */
typedef struct {
    /* 0x00 */ s16 state;
    /* 0x02 */ s16 timer;
    /* 0x04 */ s16 request;
    /* 0x06 */ s16 unk6;
    /* 0x08 */ s16 unk8;
    /* 0x0A */ s16 unkA;
    /* 0x0C */ s16 started;  /* Jpeg_EncodeBegin called */
    /* 0x0E */ s16 unkE;
    /* 0x10 */ s16 result;
    /* 0x12 */ s16 port;
    /* 0x14 */ s16 msgTimer;
} ScrnState;

/* A 2D point passed by value in one register. */
typedef struct {
    s16 x;
    s16 y;
} ScrnPoint;

typedef struct {
    u8 data[0x70];
} ScrnStoreImage;

extern GteMatrix gGteRt;
extern GteVector gGteTmpVector;
extern s32 D_005F6C74;
extern s32 D_005F6C78;
extern s32 D_005F6C7C;
extern u32 D_005F6C80;
extern s32 D_005F6C84;
extern s32 D_005F5CDC;
extern ScrnState D_008BF000;

void exit(s32 status) __attribute__((noreturn));
void FlushCache(s32 op);
GteVector *Gte_ApplyMatrix(GteMatrix *m, GteSVector *v, GteVector *out);
s32 func_003662D8(s32 y, s32 x);
s32 func_00398918(void);
s32 func_00398928(void);
void Gfx_SetPhase(u8 phase);
void func_003992C8(s32 value);
void func_003992D8(void);
s32 func_0035D698(u32 size);
s32 func_0035D6D0(void);
s32 func_0035E958(s32 a, s32 b, s32 c);
s32 func_0035E8D8(void);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 b, s32 color);
void func_003DEB80(s32 a);
s32 Text_Width(s32 n);
void Text_DrawId(s32 x, s32 y, s32 id, u32 color);
s32 Jpeg_EncodeBegin(s32 a, s32 b, s32 c);
s32 Jpeg_EncodeRow(void);
s32 Jpeg_EncodeEnd(void);
s32 Jpeg_EncodedSize(void);
void sceGsSetDefStoreImage(ScrnStoreImage *sp, s16 sbp, s16 sbw, s16 spsm, s16 x, s16 y, s16 w, s16 h);
s32 sceGsExecStoreImage(ScrnStoreImage *sp, void *dstaddr);
s32 sceGsSyncPath(s32 mode, u16 timeout);

void func_0039BBF0(s32 x, s32 y);
void func_0039BC00(s32 h);
s32 func_0039BC08(GteSVector *v, s16 *xy);
u32 func_0039BCF8(u32 n);
s32 func_0039BD68(s32 n);
s32 func_0039BD88(s32 n);
s32 func_0039BDA8(s32 a, s32 b, s32 c);
s32 func_0039BDF0(GteVector *v, GteVector *out);
void func_0039BEC8(GteVector *a, GteVector *b, GteVector *out);
GteMatrix *func_0039BFB0(GteMatrix *m, GteVector *v);
s32 func_0039C0E0(s32 x, s32 y);
void func_0039C110(void);
void Scrn_Capture(void);
void func_0039C2C0(void);
void func_0039C6E8(s32 a, s32 b);
s32 func_0039C718(void);
s32 func_0039C738(void);
s32 func_0039C768(void);
s32 func_0039C778(void);
void func_0039C788(void);

void func_0039BBF0(s32 x, s32 y) {
    D_005F6C78 = x;
    D_005F6C7C = y;
}

void func_0039BC00(s32 h) {
    D_005F6C74 = h;
}

s32 func_0039BC08(GteSVector *v, s16 *xy) {
    GteVector t;

    Gte_ApplyMatrix(&gGteRt, v, &t);
    t.vx += gGteRt.t[0];
    t.vy += gGteRt.t[1];
    t.vz += gGteRt.t[2];
    if (t.vz < 1000) {
        t.vz = 1000;
    }
    if (t.vz > 0) {
        s32 x = t.vx * D_005F6C74 / t.vz;
        s32 y = t.vy * D_005F6C74 / t.vz;

        xy[0] = D_005F6C78 + x;
        xy[1] = D_005F6C7C + y;
    } else {
        *(s32 *)xy = 0x0FFF0FFF;
    }
    return t.vz / 4;
}

u32 func_0039BCF8(u32 n) {
    u32 s;
    u32 t;

    if (n == 0) {
        return 0;
    }
    s = 1;
    t = n;
    while (s < t) {
        s <<= 1;
        t >>= 1;
    }
    do {
        t = s;
        s = (n / t + t) >> 1;
    } while (s < t);
    return t;
}

s32 func_0039BD68(s32 n) {
    return func_0039BCF8(n);
}

s32 func_0039BD88(s32 n) {
    return func_0039BCF8(n);
}

s32 func_0039BDA8(s32 a, s32 b, s32 c) {
    s32 ax = (s16)a;
    s32 ay = a >> 16;
    s32 bx = (s16)b;
    s32 by = b >> 16;
    s32 cx = (s16)c;
    s32 cy = c >> 16;

    return (by - ay) * (cx - ax) - (bx - ax) * (cy - ay);
}

s32 func_0039BDF0(GteVector *v, GteVector *out) {
    s32 sq;
    s32 len;

    sq = v->vx * v->vx + v->vy * v->vy + v->vz * v->vz;
    len = func_0039BD68(sq);
    if (len > 0) {
        len = 0x1000000 / len;
    }
    out->vx = v->vx * len / 4096;
    out->vy = v->vy * len / 4096;
    out->vz = v->vz * len / 4096;
    return sq;
}

void func_0039BEC8(GteVector *a, GteVector *b, GteVector *out) {
    gGteTmpVector.vx = a->vy * b->vz / 4096 - a->vz * b->vy / 4096;
    gGteTmpVector.vy = a->vz * b->vx / 4096 - a->vx * b->vz / 4096;
    gGteTmpVector.vz = a->vx * b->vy / 4096 - a->vy * b->vx / 4096;
    out->vx = gGteTmpVector.vx;
    out->vy = gGteTmpVector.vy;
    out->vz = gGteTmpVector.vz;
}

GteMatrix *func_0039BFB0(GteMatrix *m, GteVector *v) {
    m->m[0][0] = m->m[0][0] * v->vx / 4096;
    m->m[0][1] = m->m[0][1] * v->vy / 4096;
    m->m[0][2] = m->m[0][2] * v->vz / 4096;
    m->m[1][0] = m->m[1][0] * v->vx / 4096;
    m->m[1][1] = m->m[1][1] * v->vy / 4096;
    m->m[1][2] = m->m[1][2] * v->vz / 4096;
    m->m[2][0] = m->m[2][0] * v->vx / 4096;
    m->m[2][1] = m->m[2][1] * v->vy / 4096;
    m->m[2][2] = m->m[2][2] * v->vz / 4096;
    return m;
}

s32 func_0039C0E0(s32 x, s32 y) {
    s32 r = func_003662D8(y, x);

    return r < 0x800 ? r : r - 0x1000;
}

void func_0039C110(void) {
    func_0039BBF0(0x140, 0xF0);
}

/* NON_MATCHING: 35 lines: gcc reuses the second unkE test (lhu) for the loop-top exit test, the original reloads it (lh) at the top; registers follow */
#if 0
void Scrn_Capture(void) {
    ScrnStoreImage img;
    ScrnState *s;
    u32 i;
    u32 j;
    u32 half;
    u32 next;
    u32 off;
    s32 buf;

    func_00398918();
    half = func_00398928() & 1;
    s = &D_008BF000;
    for (i = 0; i < 0x38; i++) {
        for (j = 0; j < 2; j++) {
            off = half * 0x3C00;
            buf = D_005F6C84;
            if (s->unkE == 0) {
                if (s->started == 0) {
                    Jpeg_EncodeBegin(buf, buf + 0x7800, 0x25800);
                    s->started = 1;
                }
                sceGsSetDefStoreImage(&img, 0, 10, 1, 0, (i * 2 + j) * 8, 0x280, 8);
                buf += off;
                next = half + 1;
                FlushCache(0);
                Gfx_SetPhase(2);
            } else {
                exit(1);
            }
            while (sceGsExecStoreImage(&img, (void *)buf) != 0) {
            }
            sceGsSyncPath(0, 0);
            *(volatile u32 *)0x10008000 = 0x40;
            *(volatile u32 *)0x10009000 = 0x41;
            *(volatile u32 *)0x1000D400 = 0x40;
            Gfx_SetPhase(3);
            if (s->unkE == 0 && j == 1) {
                Jpeg_EncodeRow();
            }
            half = next & 1;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/screenshot", Scrn_Capture);
#endif

void func_0039C2C0(void) {
    s32 i;
    s32 w;
    s32 t;
    s32 x;
    s32 y;
    s32 r;

    switch (D_008BF000.state) {
    case 1:
        if (D_008BF000.request != 0) {
            D_008BF000.result = -1;
            D_008BF000.state = 2;
            D_008BF000.request = 0;
            D_008BF000.timer = 2;
        }
        break;
    case 2:
        if (D_008BF000.port == -1) {
            D_008BF000.result = 1;
            D_008BF000.msgTimer = 60;
            D_008BF000.state = 10;
        } else if (--D_008BF000.timer <= 0) {
            D_008BF000.state = 3;
            D_008BF000.timer = 2;
        }
        break;
    case 3:
        D_008BF000.unk6 = 1;
        if (D_008BF000.unk8 == 1 && D_008BF000.unkA == 0) {
            func_003992C8(1);
        }
        if (--D_008BF000.timer == 0) {
            D_008BF000.timer = 4;
            D_008BF000.state = 4;
        }
        break;
    case 4:
        if (--D_008BF000.timer == 0) {
            D_008BF000.state = 5;
            D_008BF000.timer = 0x70;
            func_0035D698(0x2D000);
            D_005F6C84 = func_0035D6D0();
        }
        break;
    case 5:
        func_0035D698(0x2D000);
        if ((D_005F6C84 = func_0035D6D0()) != 0) {
            D_008BF000.state = 6;
        }
        break;
    case 6:
        func_0035D698(0x2D000);
        D_005F6C84 = func_0035D6D0();
        Scrn_Capture();
        if (D_008BF000.unkE == 0) {
            if (Jpeg_EncodeEnd() != 0 || (D_005F6C80 = Jpeg_EncodedSize()) > 0x257FF) {
                D_008BF000.result = 1;
                D_008BF000.state = 0;
                D_008BF000.unk6 = 0;
                D_008BF000.msgTimer = 60;
            } else {
                D_008BF000.state = 7;
            }
        }
        break;
    case 7:
        func_0035D698(0x2D000);
        D_005F6C84 = func_0035D6D0();
        if (func_0035E958(D_008BF000.port, D_005F6C84 + 0x7800, D_005F6C80) == 0) {
            D_008BF000.state = 8;
        }
        break;
    case 8:
        func_0035D698(0x2D000);
        D_005F6C84 = func_0035D6D0();
        r = func_0035E8D8();
        if (!(r & 0x8000)) {
            if (!(r & 0x4000)) {
                D_008BF000.unk6 = 0;
                D_008BF000.state = 11;
            } else {
                D_008BF000.result = 1;
                D_008BF000.msgTimer = 60;
                D_008BF000.unk6 = 0;
                D_008BF000.state = 0;
            }
        }
        break;
    case 0:
        func_003992D8();
        func_003992C8(0);
        D_008BF000.state = 10;
        break;
    case 10:
        if (D_008BF000.result == 1) {
            func_003DBCF0(0, 0, 0x280, 0x1C0, 0xFFFE, 0x60);
            w = 0;
            for (i = 0; i < 3; i++) {
                t = Text_Width(i + 0x29);
                if (w < t) {
                    w = t;
                }
            }
            x = (0x280 - w) / 2;
            func_003DEB80(0);
            for (i = 0, y = 0xB4; i < 3; i++, y += 0x32) {
                Text_DrawId(x, y, i + 0x29, 0x80404080);
            }
            if (D_008BF000.msgTimer > 0) {
                D_008BF000.msgTimer--;
            } else if (D_005F5CDC & 0x8FC) {
                D_008BF000.state = 13;
            }
        }
        break;
    case 11:
        func_003992D8();
        func_003992C8(0);
        D_008BF000.state = 12;
        break;
    case 12:
        D_008BF000.state = 13;
        break;
    case 13:
        func_003992C8(0);
        D_008BF000.state = 1;
        D_008BF000.result = -1;
        D_008BF000.unk6 = 0;
        break;
    case 9:
    default:
        exit(1);
    }
    D_008BF000.unkA = D_008BF000.unk8;
    D_008BF000.unk8 = D_008BF000.unk6;
}

void func_0039C6E8(s32 a, s32 b) {
    ScrnState *s = &D_008BF000;

    if (s->state == 1) {
        s->port = b;
        s->unkE = a;
        s->request = 1;
        s->started = 0;
    }
}

s32 func_0039C718(void) {
    return func_0039C738();
}

s32 func_0039C738(void) {
    s32 r = 0;

    if (D_008BF000.state != 1 || D_008BF000.request != 0) {
        r = 1;
    }
    return r;
}

s32 func_0039C768(void) {
    return D_008BF000.unk6;
}

s32 func_0039C778(void) {
    return D_008BF000.unk6;
}

void func_0039C788(void) {
    D_008BF000.unkA = 0;
    D_008BF000.unk8 = 0;
    D_008BF000.unk6 = 0;
    D_008BF000.request = 0;
    D_008BF000.state = 1;
    D_008BF000.result = -1;
}
