/*
 * VIF1: the interface in front of VU1 and PATH2 of the GIF. The VIF1 DMA channel (plat_hw.c) hands it the words of
 * each transfer, the DMA tag's upper half first when the channel runs with TTE. Commands:
 *
 *  - UNPACK (0x60-0x7F): vectors into VU1 data memory (0x1100C000, 16 KB, the EE's view of it), every format
 *    (S / V2 / V3 / V4 of 32, 16, 8 bits and V4-5), signed or unsigned, with STCYCL skipping / filling writes,
 *    STMASK, STROW / STCOL and STMOD offset / difference modes, and the double buffer (BASE / OFFSET / TOPS, FLG);
 *  - MPG (0x4A): microcode into VU1 micro memory (0x11008000, 16 KB);
 *  - DIRECT / DIRECTHL (0x50 / 0x51): GIF data on PATH2 (gs_core.c);
 *  - MSCAL / MSCALF / MSCNT (0x14 / 0x15 / 0x17): TOP / ITOP latch, the double buffer swaps and the VU1 program
 *    runs to its end (gs_vu1.c), kicking its GIF packets on PATH1;
 *  - STCYCL, OFFSET, BASE, ITOP, STMOD, MSKPATH3, MARK, FLUSH*: state, or nothing to do here.
 *
 * A command's data may continue in the next transfer (the command word is often in a DMA tag and its data in the
 * block behind it): DIRECT data is passed on as it comes, the data of the other commands is collected first.
 *
 * Structure after the reference project's VIF1 and UNPACK code (port/src/gs/gs_core.c and gs_vu1.c of
 * Tenkaichi3Decomp).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define memcpy __builtin_memcpy /* -fno-builtin: inline the small copies of the hot paths */
#include "port_addr.h"
#include "gs.h"
#include "plat_state.h"

#define VU1_DATA ((uint8_t *)Port_Ptr(0x1100C000u))
#define VU1_MICRO ((uint8_t *)Port_Ptr(0x11008000u))

uint32_t gGsVu1Calls;
uint32_t gGsVu1Mpg;

static struct {
    uint32_t cl, wl, mode, mask, row[4], col[4];
    uint32_t base, ofst, tops, top, dbf, itop, itops;
    uint32_t direct;                      /* GIF quadwords still to come (DIRECT) */
    uint32_t pend, need, have, buf[2048]; /* a command waiting for `need` words of data */
} vif;

void Gs_Vif1Reset(void) {
    Gs_AsyncDrain(); /* the game's thread (sceGsResetGraph, self-tests): after what the GS thread still has */
    memset(&vif, 0, sizeof vif);
}

/* Words of data that follow an UNPACK with these parameters (a filling write takes fewer than it writes). */
static uint32_t unpack_words(uint32_t cmd, uint32_t num) {
    uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, n = num ? num : 256;
    uint32_t bits = vn == 3 && vl == 3 ? 16 : (32u >> vl) * (vn + 1);

    if (vif.wl > vif.cl && vif.wl != 0) {
        n = vif.cl * (n / vif.wl) + (n % vif.wl > vif.cl ? vif.cl : n % vif.wl);
    }
    return (n * bits + 31) / 32;
}

static void unpack(uint32_t cmd, uint32_t num, uint32_t imm, const uint32_t *data) {
    uint32_t vn = (cmd >> 2) & 3, vl = cmd & 3, masked = (cmd >> 4) & 1, usn = (imm >> 14) & 1;
    uint32_t addr = (imm & 0x3FF) + (((imm >> 15) & 1) ? vif.tops : 0), n = num ? num : 256;
    uint32_t cl = vif.cl, wl = vif.wl, cyc = 0, bitpos = 0, k, c;
    const uint8_t *src = (const uint8_t *)data;
    uint8_t *mem = VU1_DATA;

    if (cl == 0 && wl == 0) {
        cl = wl = 1;
    }
    if (!masked && vif.mode == 0 && wl <= cl && (vl != 3 || vn == 3)) {
        /* the game's UNPACKs (T30 batch 2): every vector comes from the data and is written whole (the fields the
           format has not are 0, a scalar fills all four), so the loop below reduces to this */
        uint32_t size = 32u >> vl, bytes = vn == 3 && vl == 3 ? 2 : (vn + 1) * (size / 8), pos = 0;
        for (k = 0; k < n; k++) {
            uint32_t out[4] = {0, 0, 0, 0};
            if (vn == 3 && vl == 3) {
                uint32_t w = src[pos] | (uint32_t)src[pos + 1] << 8;
                out[0] = (w & 31) << 3;
                out[1] = ((w >> 5) & 31) << 3;
                out[2] = ((w >> 10) & 31) << 3;
                out[3] = ((w >> 15) & 1) << 7;
            } else if (size == 32) { /* constant sizes: inline copies */
                switch (vn) {
                case 0: memcpy(out, &src[pos], 4); break;
                case 1: memcpy(out, &src[pos], 8); break;
                case 2: memcpy(out, &src[pos], 12); break;
                default: memcpy(out, &src[pos], 16); break;
                }
            } else {
                for (c = 0; c <= vn; c++) {
                    if (size == 16) {
                        uint16_t h;
                        memcpy(&h, &src[pos + c * 2], 2);
                        out[c] = usn ? h : (uint32_t)(int32_t)(int16_t)h;
                    } else {
                        out[c] = usn ? src[pos + c] : (uint32_t)(int32_t)(int8_t)src[pos + c];
                    }
                }
            }
            if (vn == 0) {
                out[1] = out[2] = out[3] = out[0];
            }
            pos += bytes;
            memcpy(&mem[(addr & 0x3FF) * 16], out, 16);
            addr++;
            if (++cyc == wl) {
                addr += cl - wl; /* skipping write */
                cyc = 0;
            }
        }
        return;
    }
    for (k = 0; k < n; k++) {
        uint32_t v[4] = {0, 0, 0, 0}, out[4];
        int have = !(wl > cl && cyc >= cl); /* filling write: past CL the vector comes from the registers only */

        if (have) {
            if (vn == 3 && vl == 3) { /* V4-5: RGBA 5:5:5:1 */
                uint32_t w = src[bitpos / 8] | (uint32_t)src[bitpos / 8 + 1] << 8;
                v[0] = (w & 31) << 3;
                v[1] = ((w >> 5) & 31) << 3;
                v[2] = ((w >> 10) & 31) << 3;
                v[3] = ((w >> 15) & 1) << 7;
                bitpos += 16;
            } else {
                uint32_t size = 32u >> vl;
                for (c = 0; c <= vn; c++) {
                    uint32_t w = 0;
                    memcpy(&w, &src[bitpos / 8], size / 8);
                    if (!usn && size == 16) {
                        w = (uint32_t)(int32_t)(int16_t)w;
                    }
                    if (!usn && size == 8) {
                        w = (uint32_t)(int32_t)(int8_t)w;
                    }
                    v[c] = w;
                    bitpos += size;
                }
                if (vn == 0) { /* S: the scalar in every field */
                    v[1] = v[2] = v[3] = v[0];
                }
            }
        }
        memcpy(out, &mem[(addr & 0x3FF) * 16], 16);
        for (c = 0; c < 4; c++) {
            uint32_t m = masked ? (vif.mask >> (2 * ((cyc > 3 ? 3 : cyc) * 4 + c))) & 3 : 0;
            if (m == 0) {
                if (!have) {
                    continue; /* nothing to write from data: leave the field */
                }
                out[c] = v[c];
                if (vif.mode == 1) {
                    out[c] = v[c] + vif.row[c];
                } else if (vif.mode == 2) {
                    vif.row[c] += v[c];
                    out[c] = vif.row[c];
                }
            } else if (m == 1) {
                out[c] = vif.row[c];
            } else if (m == 2) {
                out[c] = vif.col[cyc > 3 ? 3 : cyc];
            } /* m == 3: write protected */
        }
        memcpy(&mem[(addr & 0x3FF) * 16], out, 16);
        addr++;
        if (++cyc == wl) {
            if (cl > wl) {
                addr += cl - wl; /* skipping write */
            }
            cyc = 0;
        }
    }
}

static void mpg(uint32_t addr, const uint32_t *words, uint32_t count) {
    uint32_t i;
    uint8_t *micro = VU1_MICRO;
    gGsVu1Mpg++;
    for (i = 0; i < count; i++) {
        memcpy(&micro[((addr * 8) + i * 4) & 0x3FFC], &words[i], 4);
    }
}

/* MSCAL / MSCALF / MSCNT: TOP and ITOP latch, the double buffer swaps, the program runs (gs_vu1.c). The VIF waits
   for the previous program to end before it starts one, so running it here, to its end, keeps the order of the
   UNPACKs, DIRECTs and kicks. */
static void call(int addr) {
    gGsVu1Calls++;
    vif.top = vif.tops;
    vif.itop = vif.itops;
    if (vif.dbf) {
        vif.tops = vif.base;
        vif.dbf = 0;
    } else {
        vif.tops = vif.base + vif.ofst;
        vif.dbf = 1;
    }
    Gs_Vu1Run(addr, vif.top, vif.itop);
}

static void finish(uint32_t code, const uint32_t *b, uint32_t words) {
    uint32_t c = (code >> 24) & 0x7F;
    if (c == 0x20) {
        vif.mask = b[0];
    } else if (c == 0x30) {
        memcpy(vif.row, b, 16);
    } else if (c == 0x31) {
        memcpy(vif.col, b, 16);
    } else if (c == 0x4A) {
        mpg(code & 0xFFFF, b, words);
    } else {
        unpack(c, (code >> 16) & 0xFF, code & 0xFFFF, b);
    }
}

void Gs_Vif1(const uint32_t *w, uint32_t count) {
    uint32_t i = 0;

    while (i < count) {
        uint32_t code, cmd, num, imm;

        if (vif.direct != 0) {
            uint32_t qwc = (count - i) / 4 < vif.direct ? (count - i) / 4 : vif.direct;
            if (qwc == 0) {
                break; /* not a whole quadword left: GIF data is quadword aligned */
            }
            Gs_AsyncGif(2, (const uint8_t *)&w[i], qwc);
            i += qwc * 4;
            vif.direct -= qwc;
            continue;
        }
        if (vif.need != 0 && vif.have == 0 && count - i >= vif.need && vif.need <= 2048) {
            finish(vif.pend, &w[i], vif.need); /* all of it here: used where it lies */
            i += vif.need;
            vif.need = 0;
            continue;
        }
        if (vif.need != 0) {
            uint32_t n = count - i < vif.need - vif.have ? count - i : vif.need - vif.have;
            if (vif.have + n <= 2048) {
                memcpy(&vif.buf[vif.have], &w[i], n * 4);
            }
            vif.have += n;
            i += n;
            if (vif.have == vif.need) {
                if (vif.need <= 2048) {
                    finish(vif.pend, vif.buf, vif.need);
                }
                vif.need = vif.have = 0;
            }
            continue;
        }
        code = w[i++];
        cmd = (code >> 24) & 0x7F;
        num = (code >> 16) & 0xFF;
        imm = code & 0xFFFF;
        vif.pend = code;
        switch (cmd) {
        case 0x01: vif.cl = imm & 0xFF; vif.wl = (imm >> 8) & 0xFF; break; /* STCYCL */
        case 0x02: vif.ofst = imm & 0x3FF; vif.dbf = 0; vif.tops = vif.base; break; /* OFFSET */
        case 0x03: vif.base = imm & 0x3FF; break;                         /* BASE */
        case 0x04: vif.itops = imm & 0x3FF; break;                        /* ITOP */
        case 0x05: vif.mode = imm & 3; break;                             /* STMOD */
        case 0x14: case 0x15: call((int)imm); break;                      /* MSCAL, MSCALF */
        case 0x17: call(-1); break;                                       /* MSCNT */
        case 0x20: vif.need = 1; break;                                   /* STMASK */
        case 0x30: case 0x31: vif.need = 4; break;                        /* STROW, STCOL */
        case 0x4A: vif.need = (num ? num : 256) * 2; break;               /* MPG */
        case 0x50: case 0x51: vif.direct = imm ? imm : 65536; break;      /* DIRECT, DIRECTHL */
        default:
            if (cmd >= 0x60) {
                vif.need = unpack_words(cmd, num); /* UNPACK */
            }
            break; /* NOP, MSKPATH3, MARK, FLUSHE, FLUSH, FLUSHA */
        }
    }
}

/* ------------------------------------------------------------------------------------------------ self-test */

static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: gs self-test FAILED: %s\n", what);
        sFails++;
    }
}

static const uint32_t *vu(uint32_t qw) { return (const uint32_t *)&VU1_DATA[(qw & 0x3FF) * 16]; }
static int vec(uint32_t qw, uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
    const uint32_t *v = vu(qw);
    return v[0] == x && v[1] == y && v[2] == z && v[3] == w;
}

int Port_GsVifSelfTest(void) {
    uint32_t s[64];
    uint32_t calls = gGsVu1Calls;

    Gs_Vif1Reset();
    memset(VU1_DATA, 0, 0x4000);
    memset(VU1_MICRO, 0, 0x4000);
    memcpy(VU1_MICRO, "\x3C\x03\x00\x80\xFF\x02\x00\x40", 8); /* the MSCALs below run nop | nop [E] */
    /* STCYCL 1,1; UNPACK V4-32, 2 vectors at 0 */
    s[0] = 0x01000101u;
    s[1] = 0x6C020000u;
    s[2] = 1; s[3] = 2; s[4] = 3; s[5] = 4; s[6] = 5; s[7] = 6; s[8] = 7; s[9] = 8;
    Gs_Vif1(s, 10);
    expect(vec(0, 1, 2, 3, 4) && vec(1, 5, 6, 7, 8), "UNPACK V4-32");
    /* V3-16 signed at 4, V2-8 unsigned at 5, S-32 at 6, V4-5 at 7 */
    s[0] = 0x69010004u; s[1] = 0x0002FFFFu; s[2] = 0x00000003u;
    s[3] = 0x66014005u; s[4] = 0x000001FFu;
    s[5] = 0x60010006u; s[6] = 0xDEADBEEFu;
    s[7] = 0x6F010007u; s[8] = 0x0000801Fu | (2u << 5) | (31u << 10);
    Gs_Vif1(s, 9);
    expect(vu(4)[0] == 0xFFFFFFFFu && vu(4)[1] == 2 && vu(4)[2] == 3, "UNPACK V3-16 signed");
    expect(vu(5)[0] == 0xFF && vu(5)[1] == 1, "UNPACK V2-8 unsigned");
    expect(vec(6, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu), "UNPACK S-32 broadcast");
    expect(vec(7, 0xF8, 0x10, 0xF8, 0x80), "UNPACK V4-5");
    /* skipping write: STCYCL CL 2, WL 1, two vectors at 10 -> 10 and 12 */
    s[0] = 0x01000102u;
    s[1] = 0x6C02000Au; s[2] = 9; s[3] = 9; s[4] = 9; s[5] = 9; s[6] = 7; s[7] = 7; s[8] = 7; s[9] = 7;
    Gs_Vif1(s, 10);
    expect(vec(10, 9, 9, 9, 9) && vec(11, 0, 0, 0, 0) && vec(12, 7, 7, 7, 7), "STCYCL skipping write");
    /* filling write with a mask: CL 1, WL 2; the second write of each cycle comes from ROW (mask 1 in row 1) */
    s[0] = 0x01000201u;
    s[1] = 0x20000000u; s[2] = 0x5500u;                           /* STMASK */
    s[3] = 0x30000000u; s[4] = 0x11; s[5] = 0x22; s[6] = 0x33; s[7] = 0x44; /* STROW */
    s[8] = 0x7C040014u;                                           /* UNPACK V4-32 masked, 4 writes at 20 */
    s[9] = 1; s[10] = 2; s[11] = 3; s[12] = 4; s[13] = 5; s[14] = 6; s[15] = 7; s[16] = 8;
    Gs_Vif1(s, 17);
    expect(vec(20, 1, 2, 3, 4) && vec(21, 0x11, 0x22, 0x33, 0x44) && vec(22, 5, 6, 7, 8) && vec(23, 0x11, 0x22, 0x33, 0x44),
           "STCYCL filling write with STMASK / STROW");
    /* STMOD offset: data + ROW */
    s[0] = 0x01000101u; s[1] = 0x05000001u;
    s[2] = 0x6C01001Eu; s[3] = 1; s[4] = 1; s[5] = 1; s[6] = 1;
    s[7] = 0x05000000u;
    Gs_Vif1(s, 8);
    expect(vec(30, 0x12, 0x23, 0x34, 0x45), "STMOD offset mode");
    /* the double buffer: BASE 0x100, OFFSET 0x80; an UNPACK with FLG lands at TOPS, MSCAL swaps it */
    s[0] = 0x03000100u; s[1] = 0x02000080u;
    s[2] = 0x6C018000u; s[3] = 0xA; s[4] = 0xB; s[5] = 0xC; s[6] = 0xD;
    s[7] = 0x14000000u;
    s[8] = 0x6C018000u; s[9] = 0xE; s[10] = 0xF; s[11] = 0x10; s[12] = 0x11;
    Gs_Vif1(s, 13);
    expect(vec(0x100, 0xA, 0xB, 0xC, 0xD) && vec(0x180, 0xE, 0xF, 0x10, 0x11), "UNPACK FLG through the double buffer");
    expect(gGsVu1Calls == calls + 1, "MSCAL recorded");
    /* a command whose data arrives in the next transfer, and MPG into micro memory */
    s[0] = 0x6C010040u;
    Gs_Vif1(s, 1);
    s[0] = 0x21; s[1] = 0x22; s[2] = 0x23; s[3] = 0x24;
    Gs_Vif1(s, 4);
    expect(vec(0x40, 0x21, 0x22, 0x23, 0x24), "UNPACK data split across transfers");
    s[0] = 0x4A020004u; s[1] = 0x11111111u; s[2] = 0x22222222u; s[3] = 0x33333333u; s[4] = 0x44444444u;
    Gs_Vif1(s, 5);
    expect(memcmp(&VU1_MICRO[32], &s[1], 16) == 0, "MPG into VU1 micro memory");
    Gs_Vif1Reset();
    memset(VU1_DATA, 0, 0x4000);
    memset(VU1_MICRO, 0, 0x4000);
    return sFails;
}

__attribute__((constructor(101))) static void vif1_state(void) { Port_StateAdd("vif1", &vif, sizeof vif, PS_GSSIDE); }
