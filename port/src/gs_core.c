/*
 * The Graphics Synthesizer: GIF packets, GS registers, GS memory and a software reference rasteriser.
 *
 * The game's packets reach it from the DMA channels (plat_hw.c): GIF data on PATH3 (the GIF channel), on PATH2
 * (VIF1 DIRECT / DIRECTHL, gs_vif1.c) and, once VU1 runs (T29), on PATH1 (XGKICK). Each path keeps its own GIF tag
 * state. The registers are interpreted as the GS does, and the primitives are drawn into GS memory itself:
 *     GIF tag (PACKED / REGLIST / IMAGE)  ->  registers, vertex queue  ->  sprite / triangle / line / point  ->  pixels
 *
 * GS memory is the real 4 MB with its real layout (pages, blocks, columns per pixel format), because the game draws
 * into memory it later reads as a texture and uploads indexed textures and palettes with other formats than it
 * draws them with. The CLUT is the GS's own 1 KB buffer, loaded when TEX0 / TEX2 is written (CLD), as on the GS.
 *
 * Structure after the reference project's hardware-level GS core (port/src/gs/gs_core.c of Tenkaichi3Decomp): its
 * swizzle tables and per-format addressing, the GIF and register decoding, the per-pixel pipeline (scissor, alpha
 * test, depth test, blending, FBMSK) and the upload fast paths. Unlike that file everything here is integer:
 * coordinates stay in the GS's 12.4 fixed point, triangles use edge functions with 24-bit barycentric fractions,
 * and STQ texture coordinates are decoded from their float bits (no host floating point anywhere in the port).
 * Added: the CLUT buffer with CLD / CSA / CSM2, fog, PABE, COLCLAMP, DATE, FBA, local -> local transfers, SIGNAL /
 * FINISH / LABEL, and the frame dump.
 *
 * Not modelled: mip-mapping (level 0 only, nearest or bilinear by TEX1), dithering, anti-aliasing, GS -> host
 * transfers (counted), the exact rasterisation rules of the hardware beyond the top-left fill convention.
 *
 * TTT_DUMP=<dir> writes the displayed picture as <dir>/frame_<vblank>.png every TTT_DUMP_EVERY VBlanks (default 60):
 * the read circuit enabled in PMODE (circuit 1 first), its DISPFB / DISPLAY, interlaced frame mode doubled to the
 * TV's lines. TTT_SELFTEST=gs checks the memory layout, the GIF and the rasteriser and exits.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_addr.h"
#include "gs.h"

#define PRIV(off) (*(volatile uint64_t *)Port_Ptr(0x12000000u + (off)))

/* ------------------------------------------------------------------------------------------------ GS memory */

/* 4 MB. A buffer pointer counts 256-byte blocks, a buffer width 64 pixels. Pages of 8 KB hold 32 blocks in a
   format-specific order; inside a block, pixels are arranged in columns. The indexed formats' column tables are
   generated from their first column (odd columns swap the halves of every group of eight). */
static uint32_t sVram[1 << 20];
static const uint8_t kBlock32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static const uint8_t kBlock16[8][4] = {{0, 2, 8, 10}, {1, 3, 9, 11}, {4, 6, 12, 14}, {5, 7, 13, 15}, {16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};
static const uint8_t kBlock16S[8][4] = {{0, 2, 16, 18}, {1, 3, 17, 19}, {8, 10, 24, 26}, {9, 11, 25, 27}, {4, 6, 20, 22}, {5, 7, 21, 23}, {12, 14, 28, 30}, {13, 15, 29, 31}};
static const uint8_t kCol32[8][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
                                     {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
static const uint8_t kCol16[8][16] = {
    {0, 2, 8, 10, 16, 18, 24, 26, 1, 3, 9, 11, 17, 19, 25, 27}, {4, 6, 12, 14, 20, 22, 28, 30, 5, 7, 13, 15, 21, 23, 29, 31},
    {32, 34, 40, 42, 48, 50, 56, 58, 33, 35, 41, 43, 49, 51, 57, 59}, {36, 38, 44, 46, 52, 54, 60, 62, 37, 39, 45, 47, 53, 55, 61, 63},
    {64, 66, 72, 74, 80, 82, 88, 90, 65, 67, 73, 75, 81, 83, 89, 91}, {68, 70, 76, 78, 84, 86, 92, 94, 69, 71, 77, 79, 85, 87, 93, 95},
    {96, 98, 104, 106, 112, 114, 120, 122, 97, 99, 105, 107, 113, 115, 121, 123}, {100, 102, 108, 110, 116, 118, 124, 126, 101, 103, 109, 111, 117, 119, 125, 127}};
static const uint8_t kCol8First[4][16] = {{0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54}, {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
                                          {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23}, {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31}};
static const uint8_t kCol4First[4][32] = {
    {0, 8, 32, 40, 64, 72, 96, 104, 2, 10, 34, 42, 66, 74, 98, 106, 4, 12, 36, 44, 68, 76, 100, 108, 6, 14, 38, 46, 70, 78, 102, 110},
    {16, 24, 48, 56, 80, 88, 112, 120, 18, 26, 50, 58, 82, 90, 114, 122, 20, 28, 52, 60, 84, 92, 116, 124, 22, 30, 54, 62, 86, 94, 118, 126},
    {65, 73, 97, 105, 1, 9, 33, 41, 67, 75, 99, 107, 3, 11, 35, 43, 69, 77, 101, 109, 5, 13, 37, 45, 71, 79, 103, 111, 7, 15, 39, 47},
    {81, 89, 113, 121, 17, 25, 49, 57, 83, 91, 115, 123, 19, 27, 51, 59, 85, 93, 117, 125, 21, 29, 53, 61, 87, 95, 119, 127, 23, 31, 55, 63}};
static uint8_t sCol8[16][16];
static uint16_t sCol4[16][32];
/* For uploads: the position of pixel (x, y) inside its page, in pixels of the format's size (page = 64 x 32 at 32
   bits, 64 x 64 at 16, 128 x 64 at 8, 128 x 128 at 4). */
static uint16_t sIn32[32][64], sIn16[2][64][64], sIn8[64][128], sIn4[128][128];
static int sTables;

static void tables_init(void) {
    int y, x;
    if (sTables) {
        return;
    }
    sTables = 1;
    for (y = 0; y < 16; y++) {
        int col = y >> 2;
        for (x = 0; x < 16; x++) {
            sCol8[y][x] = (uint8_t)(kCol8First[y & 3][(col & 1) ? (x ^ 4) : x] + col * 64);
        }
        for (x = 0; x < 32; x++) {
            sCol4[y][x] = (uint16_t)(kCol4First[y & 3][(col & 1) ? (x ^ 4) : x] + col * 128);
        }
    }
    for (y = 0; y < 128; y++) {
        for (x = 0; x < 128; x++) {
            if (y < 32 && x < 64) {
                sIn32[y][x] = (uint16_t)((kBlock32[(y >> 3) & 3][(x >> 3) & 7] << 6) + kCol32[y & 7][x & 7]);
            }
            if (y < 64 && x < 64) {
                sIn16[0][y][x] = (uint16_t)((kBlock16[(y >> 3) & 7][(x >> 4) & 3] << 7) + kCol16[y & 7][x & 15]);
                sIn16[1][y][x] = (uint16_t)((kBlock16S[(y >> 3) & 7][(x >> 4) & 3] << 7) + kCol16[y & 7][x & 15]);
            }
            if (y < 64) {
                sIn8[y][x] = (uint16_t)((kBlock32[(y >> 4) & 3][(x >> 4) & 7] << 8) + sCol8[y & 15][x & 15]);
            }
            sIn4[y][x] = (uint16_t)((kBlock16[(y >> 4) & 7][(x >> 5) & 3] << 9) + sCol4[y & 15][x & 31]);
        }
    }
}

static int psm_bits(uint32_t psm) {
    if ((psm & 0x30) == 0x30) {
        psm &= 0x0F; /* depth formats: 32, 24, 16 bits like the colour formats */
    }
    switch (psm) {
    case 0x01: return 24;
    case 0x02: case 0x0A: return 16;
    case 0x13: case 0x1B: return 8;
    case 0x14: case 0x24: case 0x2C: return 4;
    default: return 32;
    }
}

/* Reads (write = 0) or writes the pixel (x, y) of the buffer at bp (blocks), width bw (64 pixels), format psm.
   The depth formats (0x30..0x3A) are the colour formats with the blocks of a page in another order (XOR 0x18). */
static uint32_t vram_rw(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, int write, uint32_t v) {
    uint32_t a, *w, zx = 0;

    if (bw == 0) {
        bw = 1;
    }
    if ((psm & 0x30) == 0x30) {
        zx = 0x18;
        psm &= 0x0F;
    }
    x &= 0x7FF;
    y &= 0x7FF;
    switch (psm) {
    case 0x02: case 0x0A: /* 16 bits */
        a = ((bp + ((y >> 1) & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + ((psm == 0x0A ? kBlock16S : kBlock16)[(y >> 3) & 7][(x >> 4) & 3] ^ zx)) << 7) +
            kCol16[y & 7][x & 15];
        w = &sVram[(a >> 1) & 0xFFFFF];
        if (write) {
            *w = (*w & ~(0xFFFFu << ((a & 1) * 16))) | (v & 0xFFFF) << ((a & 1) * 16);
            return 0;
        }
        return (*w >> ((a & 1) * 16)) & 0xFFFF;
    case 0x13: /* 8 bits */
        a = ((bp + ((y >> 1) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + sCol8[y & 15][x & 15];
        w = &sVram[(a >> 2) & 0xFFFFF];
        if (write) {
            *w = (*w & ~(0xFFu << ((a & 3) * 8))) | (v & 0xFF) << ((a & 3) * 8);
            return 0;
        }
        return (*w >> ((a & 3) * 8)) & 0xFF;
    case 0x14: /* 4 bits */
        a = ((bp + ((y >> 2) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock16[(y >> 4) & 7][(x >> 5) & 3]) << 9) + sCol4[y & 15][x & 31];
        w = &sVram[(a >> 3) & 0xFFFFF];
        if (write) {
            *w = (*w & ~(0xFu << ((a & 7) * 4))) | (v & 0xF) << ((a & 7) * 4);
            return 0;
        }
        return (*w >> ((a & 7) * 4)) & 0xF;
    default: /* 32 bits, and the formats that live inside a 32-bit pixel */
        a = ((bp + (y & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + (kBlock32[(y >> 3) & 3][(x >> 3) & 7] ^ zx)) << 6) + kCol32[y & 7][x & 7];
        w = &sVram[a & 0xFFFFF];
        switch (psm) {
        case 0x01: if (write) { *w = (*w & 0xFF000000u) | (v & 0xFFFFFF); return 0; } return *w & 0xFFFFFF;
        case 0x1B: if (write) { *w = (*w & 0x00FFFFFFu) | v << 24; return 0; } return *w >> 24;
        case 0x24: if (write) { *w = (*w & 0xF0FFFFFFu) | (v & 15) << 24; return 0; } return (*w >> 24) & 15;
        case 0x2C: if (write) { *w = (*w & 0x0FFFFFFFu) | (v & 15) << 28; return 0; } return *w >> 28;
        default: if (write) { *w = v; return 0; } return *w;
        }
    }
}

uint32_t Gs_VramRead(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y) {
    tables_init();
    return vram_rw(bp, bw, psm, x, y, 0, 0);
}

void Gs_VramWrite(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, uint32_t v) {
    tables_init();
    vram_rw(bp, bw, psm, x, y, 1, v);
}

/* ------------------------------------------------------------------------------------------------ state */

typedef struct Vtx {
    int32_t x, y;        /* window coordinates (XYOFFSET taken off), 12.4 */
    uint32_t z;
    uint32_t s, t, q;    /* float bits */
    int32_t u, v;        /* texel coordinates, 10.4 */
    uint32_t rgba;
    uint32_t f;          /* fog coefficient */
} Vtx;

static struct {
    uint64_t prim, primRaw, rgbaq, st, uv, texa, prmodecont, prmode, fogcol, colclamp, pabe, texclut;
    uint64_t tex0[2], tex1[2], clamp[2], xyoffset[2], scissor[2], alpha[2], test[2], frame[2], zbuf[2], fba[2];
    uint64_t bitbltbuf, trxpos, trxreg, trxdir;
    uint32_t q, fog;
    Vtx vtx[3];
    int vcount;
    /* host -> GS transfer in progress */
    int transfer;
    uint32_t tx, ty, tw, th, tpos;
    /* the CLUT buffer: 512 halfwords; a 32-bit entry e keeps its low half at e and its high half at e + 256 */
    uint16_t clut[512];
    uint32_t cbp[2], clutGen;
} gs;

/* TTT_GS_TRACE=1: per VBlank, the pixels written per frame buffer (FBP) and the last TEX0 / FRAME / TEST used. */
static int sTrace = -1;
static struct { uint32_t px, prims; uint64_t tex0, frame, test, alpha; } sTr[512];
static uint32_t sUploadGen; /* bumped by every write into GS memory through a transfer (CLD 4 / 5, below) */
static struct { uint32_t tags, prims, pixels, imageQw, localCopies, readbacks, regs; } sStat;

/* GIF tag state of each path (1 = VU1, 2 = VIF1, 3 = GIF channel) */
static struct { uint64_t lo, hi; uint32_t loops, reg; } sPath[4];
static uint32_t sTracePrimVb, sCurVb;
static int sTraceX = -1, sTraceY = -1;
static uint32_t sPathTags[4], sTraceBad, sBlockQwc;
static const uint8_t *sBlock;

void Gs_Reset(void) {
    tables_init();
    memset(&gs, 0, sizeof(gs));
    memset(sPath, 0, sizeof(sPath));
    gs.prmodecont = 1; /* the attributes come from PRIM */
    gs.colclamp = 1;
    gs.q = 0x3F800000u;
}

/* ------------------------------------------------------------------------------------------------ textures */

/* A stored colour of format psm as R | G << 8 | B << 16 | A << 24 (TEXA gives the alpha of 24 and 16 bits). */
static uint32_t expand(uint32_t c, uint32_t psm) {
    uint32_t ta0 = gs.texa & 0xFF, ta1 = (gs.texa >> 32) & 0xFF, aem = (gs.texa >> 15) & 1;

    switch (psm_bits(psm)) {
    case 32:
        return c;
    case 24:
        c &= 0xFFFFFF;
        return c | ((aem && c == 0) ? 0 : ta0) << 24;
    default: {
        uint32_t r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        uint32_t a = (c & 0x8000) ? ta1 : (aem && (c & 0x7FFF) == 0 ? 0 : ta0);
        return r | g << 8 | b << 16 | a << 24;
    }
    }
}

/* Loads the CLUT buffer for TEX0 / TEX2 (only for the indexed formats, as the GS). CLD: 0 keep, 1 load, 2 load and
   CBP0 = CBP, 3 load and CBP1 = CBP, 4 load if CBP != CBP0, then CBP0 = CBP, 5 the same with CBP1.
   [I] For 4 / 5 the buffer is also reloaded when GS memory was written since the last load (as PCSX2 does): a
   palette uploaded again to the same address is seen. */
static void clut_load(uint64_t t0) {
    uint32_t psm = (t0 >> 20) & 0x3F, cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15, csm = (t0 >> 55) & 1;
    uint32_t csa = (t0 >> 56) & 31, cld = (t0 >> 61) & 7, bits = (uint32_t)psm_bits(psm), n, i;
    int load;

    if (bits > 8) {
        return;
    }
    switch (cld) {
    case 1: load = 1; break;
    case 2: load = 1; gs.cbp[0] = cbp; break;
    case 3: load = 1; gs.cbp[1] = cbp; break;
    case 4: load = gs.cbp[0] != cbp || gs.clutGen != sUploadGen; gs.cbp[0] = cbp; break;
    case 5: load = gs.cbp[1] != cbp || gs.clutGen != sUploadGen; gs.cbp[1] = cbp; break;
    default: load = 0; break;
    }
    if (!load) {
        return;
    }
    gs.clutGen = sUploadGen;
    n = bits == 8 ? 256 : 16;
    for (i = 0; i < n; i++) {
        uint32_t x, y, c, bw = 1, e;
        if (csm) { /* CSM2: one row of TEXCLUT, 16-bit entries only */
            bw = (uint32_t)(gs.texclut & 0x3F);
            x = (uint32_t)((gs.texclut >> 6) & 0x3F) * 16 + i;
            y = (uint32_t)((gs.texclut >> 12) & 0x3FF);
        } else if (n == 256) { /* 16 x 16, index bits 3 and 4 exchanged */
            uint32_t k = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);
            x = k & 15;
            y = k >> 4;
        } else { /* 8 x 2 */
            x = i & 7;
            y = i >> 3;
        }
        c = vram_rw(cbp, bw, cpsm == 0 || cpsm == 1 ? 0 : cpsm, x, y, 0, 0);
        e = (n == 256 ? i : csa * 16 + i);
        if (cpsm == 0 || cpsm == 1) {
            gs.clut[e & 255] = (uint16_t)c;
            gs.clut[(e & 255) + 256] = (uint16_t)(c >> 16);
        } else {
            gs.clut[e & 511] = (uint16_t)c;
        }
    }
}

static uint32_t clut_read(uint64_t t0, uint32_t index, int bits) {
    uint32_t cpsm = (t0 >> 51) & 15, csa = (t0 >> 56) & 31, e = bits == 8 ? index : csa * 16 + index;
    if (cpsm == 0 || cpsm == 1) {
        e &= 255;
        return expand(gs.clut[e] | (uint32_t)gs.clut[e + 256] << 16, 0);
    }
    return expand(gs.clut[e & 511], cpsm);
}

/* The texel at (u, v) (whole texels) of the context's texture, wrapped or clamped by CLAMP. */
static uint32_t texel(int ctx, int u, int v) {
    uint64_t t0 = gs.tex0[ctx], cl = gs.clamp[ctx];
    uint32_t tbp = t0 & 0x3FFF, tbw = (t0 >> 14) & 0x3F, psm = (t0 >> 20) & 0x3F;
    int tw = 1 << ((t0 >> 26) & 15), th = 1 << ((t0 >> 30) & 15), bits;
    uint32_t wms = cl & 3, wmt = (cl >> 2) & 3, c;
    int minu = (int)((cl >> 4) & 0x3FF), maxu = (int)((cl >> 14) & 0x3FF), minv = (int)((cl >> 24) & 0x3FF), maxv = (int)((cl >> 34) & 0x3FF);

    if (tw > 1024) { tw = 1024; }
    if (th > 1024) { th = 1024; }
    if (wms == 0) { u &= tw - 1; } else if (wms == 1) { u = u < 0 ? 0 : u >= tw ? tw - 1 : u; }
    else if (wms == 2) { u = u < minu ? minu : u > maxu ? maxu : u; } else { u = (u & minu) | maxu; }
    if (wmt == 0) { v &= th - 1; } else if (wmt == 1) { v = v < 0 ? 0 : v >= th ? th - 1 : v; }
    else if (wmt == 2) { v = v < minv ? minv : v > maxv ? maxv : v; } else { v = (v & minv) | maxv; }
    c = vram_rw(tbp, tbw, psm, (uint32_t)u, (uint32_t)v, 0, 0);
    bits = psm_bits(psm);
    return bits > 8 ? expand(c, psm) : clut_read(t0, c, bits);
}

/* Q of the pixel being drawn as sPixQ / 2^sPixQSh (the level of detail of LCM 0 depends on it): set by the
   rasteriser. */
static int64_t sPixQ = (int64_t)1 << 20;
static int sPixQSh = 20;

/* log2(x) in 1/16 units for x > 0: the position of the highest bit, then the next four bits as the fraction
   [I: a linear fraction; the GS's own log2 of Q is not documented beyond "log2(1/|Q|)"]. */
static int32_t log2_16(int64_t x) {
    int p = 0;
    while (p < 62 && (x >> (p + 1)) != 0) {
        p++;
    }
    return p * 16 + (int32_t)((p >= 4 ? x >> (p - 4) : x << (4 - p)) & 15);
}

/* The texel colour at (u16, v16) (1/16 texel) with TEX1's filter: LOD = K (LCM 1) or (log2(1/|Q|) << L) + K (LCM 0),
   in 1/16; LOD <= 0 magnifies (MMAG), else minifies (MMIN). Linear (MMAG 1, MMIN 1 / 4 / 5) weighs the four texels
   around the point half a texel up and left with the coordinates' 4-bit fractions, colour and alpha alike. Only level
   0 is read: no mip-maps (MXL), which the game does not use. */
static uint32_t sample(int ctx, int32_t u16, int32_t v16) {
    uint64_t t1 = gs.tex1[ctx];
    int32_t k = (int32_t)((t1 >> 32) & 0xFFF), lod;
    int linear;
    uint32_t c00, c10, c01, c11, out = 0;
    int32_t fu, fv, u0, v0, i;

    if (k & 0x800) {
        k -= 0x1000;
    }
    if (t1 & 1) {
        lod = k;
    } else {
        int64_t q = sPixQ < 0 ? -sPixQ : sPixQ;
        lod = ((sPixQSh * 16 - log2_16(q != 0 ? q : 1)) << ((t1 >> 19) & 3)) + k;
    }
    if (lod <= 0) {
        linear = (int)((t1 >> 5) & 1);
    } else {
        uint32_t mmin = (uint32_t)(t1 >> 6) & 7;
        linear = mmin == 1 || mmin == 4 || mmin == 5; /* LINEAR, LINEAR_MIPMAP_NEAREST / _LINEAR: linear inside a level */
    }
    if (!linear) {
        return texel(ctx, u16 >> 4, v16 >> 4);
    }
    u16 -= 8;
    v16 -= 8;
    u0 = u16 >> 4;
    v0 = v16 >> 4;
    fu = u16 & 15;
    fv = v16 & 15;
    c00 = texel(ctx, u0, v0);
    if (fu == 0 && fv == 0) {
        return c00;
    }
    c10 = texel(ctx, u0 + 1, v0);
    c01 = texel(ctx, u0, v0 + 1);
    c11 = texel(ctx, u0 + 1, v0 + 1);
    for (i = 0; i < 32; i += 8) {
        uint32_t a = (c00 >> i) & 0xFF, b = (c10 >> i) & 0xFF, c = (c01 >> i) & 0xFF, d = (c11 >> i) & 0xFF;
        uint32_t v = (a * (uint32_t)(16 - fu) * (uint32_t)(16 - fv) + b * (uint32_t)fu * (uint32_t)(16 - fv) + c * (uint32_t)(16 - fu) * (uint32_t)fv +
                      d * (uint32_t)fu * (uint32_t)fv) >> 8;
        out |= v << i;
    }
    return out;
}

/* Texture function (TEX0.TFX, TCC) on a vertex colour; u16 / v16 are texel coordinates in 1/16 texel. */
static uint32_t shade(int ctx, uint32_t vc, int32_t u16, int32_t v16) {
    uint64_t t0 = gs.tex0[ctx];
    uint32_t tc, out = 0, tfx = (t0 >> 35) & 3, tcc = (t0 >> 34) & 1, av = vc >> 24, at, a;
    int i;

    tc = sample(ctx, u16, v16);
    at = tc >> 24;
    for (i = 0; i < 3; i++) {
        uint32_t cv = (vc >> (8 * i)) & 0xFF, ct = (tc >> (8 * i)) & 0xFF, c;
        c = tfx == 0 ? (ct * cv) >> 7 : tfx == 1 ? ct : ((ct * cv) >> 7) + av;
        out |= (c > 255 ? 255 : c) << (8 * i);
    }
    a = !tcc ? av : tfx == 0 ? (at * av) >> 7 : tfx == 2 ? at + av : tfx == 3 ? at : at;
    if (tfx == 3 && !tcc) {
        a = av;
    }
    return out | (a > 255 ? 255 : a) << 24;
}

static uint32_t fogged(uint32_t c, uint32_t f) {
    uint32_t out = c & 0xFF000000u;
    int i;
    for (i = 0; i < 3; i++) {
        uint32_t cc = (c >> (8 * i)) & 0xFF, fc = (uint32_t)(gs.fogcol >> (8 * i)) & 0xFF;
        out |= ((cc * f + fc * (255 - f)) >> 8) << (8 * i);
    }
    return out;
}

/* ------------------------------------------------------------------------------------------------ pixels */

static int tex_on(void);
static void pixel(int ctx, int x, int y, uint32_t z, uint32_t src) {
    uint64_t test = gs.test[ctx], al = gs.alpha[ctx], zb = gs.zbuf[ctx], pr = gs.prim, sc = gs.scissor[ctx], fr = gs.frame[ctx];
    uint32_t fbp = (uint32_t)(fr & 0x1FF) << 5, fbw = (fr >> 16) & 0x3F, fpsm = (fr >> 24) & 0x3F;
    uint32_t d, sa = src >> 24, out[3], zbp, zpsm, zmax, fbits = (uint32_t)psm_bits(fpsm);
    int i, write_rgb = 1, write_a = 1, write_z = !((zb >> 32) & 1);

    if (x < (int)(sc & 0x7FF) || x > (int)((sc >> 16) & 0x7FF) || y < (int)((sc >> 32) & 0x7FF) || y > (int)((sc >> 48) & 0x7FF) || x < 0 ||
        y < 0) {
        return;
    }
    if (test & 1) { /* alpha test */
        uint32_t ref = (test >> 4) & 0xFF;
        int pass;
        switch ((test >> 1) & 7) {
        case 0: pass = 0; break;
        case 1: pass = 1; break;
        case 2: pass = sa < ref; break;
        case 3: pass = sa <= ref; break;
        case 4: pass = sa == ref; break;
        case 5: pass = sa >= ref; break;
        case 6: pass = sa > ref; break;
        default: pass = sa != ref; break;
        }
        if (!pass) {
            switch ((test >> 12) & 3) {
            case 0: return;                          /* KEEP */
            case 1: write_z = 0; break;              /* FB_ONLY */
            case 2: write_rgb = write_a = 0; break;  /* ZB_ONLY */
            default: write_z = 0; write_a = 0; break; /* RGB_ONLY */
            }
        }
    }
    if ((test >> 14) & 1) { /* destination alpha test: the frame's alpha bit (7 of 32-bit, 15 of 16-bit) */
        uint32_t dv = vram_rw(fbp, fbw, fpsm, (uint32_t)x, (uint32_t)y, 0, 0), bit;
        if (fbits == 32 || fbits == 16) {
            bit = fbits == 32 ? dv >> 31 : (dv >> 15) & 1;
            if (bit != ((test >> 15) & 1)) {
                return;
            }
        }
    }
    /* the depth buffer lives in GS memory: ZBUF.ZBP, the frame's width, format 0x30 | ZBUF.PSM */
    zbp = (uint32_t)(zb & 0x1FF) << 5;
    zpsm = 0x30 | (uint32_t)((zb >> 24) & 15);
    zmax = psm_bits(zpsm) == 32 ? 0xFFFFFFFFu : psm_bits(zpsm) == 24 ? 0xFFFFFFu : 0xFFFFu;
    if (z > zmax) {
        z = zmax;
    }
    if ((test >> 16) & 1) { /* depth test */
        uint32_t zd = vram_rw(zbp, fbw, zpsm, (uint32_t)x, (uint32_t)y, 0, 0);
        switch ((test >> 17) & 3) {
        case 0: return;
        case 1: break;
        case 2: if (z < zd) { return; } break;
        default: if (z <= zd) { return; } break;
        }
    }
    if (write_rgb) {
        uint32_t c, m = (uint32_t)(fr >> 32), old;
        d = vram_rw(fbp, fbw, fpsm, (uint32_t)x, (uint32_t)y, 0, 0);
        old = d;
        if (fbits == 16) {
            d = (d & 31) << 3 | ((d >> 5) & 31) << 11 | ((d >> 10) & 31) << 19 | ((d & 0x8000) ? 0x80000000u : 0);
        } else if (fbits == 24) {
            d |= 0x80000000u;
        }
        out[0] = src & 0xFF;
        out[1] = (src >> 8) & 0xFF;
        out[2] = (src >> 16) & 0xFF;
        if (((pr >> 6) & 1) && (!(gs.pabe & 1) || (sa & 0x80))) { /* blending: ((A - B) * C >> 7) + D */
            uint32_t s[3] = {out[0], out[1], out[2]};
            int cc = ((al >> 4) & 3) == 0 ? (int)sa : ((al >> 4) & 3) == 1 ? (int)(d >> 24) : (int)((al >> 32) & 0xFF);
            for (i = 0; i < 3; i++) {
                int dc = (int)((d >> (8 * i)) & 0xFF);
                int a = (al & 3) == 0 ? (int)s[i] : (al & 3) == 1 ? dc : 0;
                int b = ((al >> 2) & 3) == 0 ? (int)s[i] : ((al >> 2) & 3) == 1 ? dc : 0;
                int dd = ((al >> 6) & 3) == 0 ? (int)s[i] : ((al >> 6) & 3) == 1 ? dc : 0;
                int v = (((a - b) * cc) >> 7) + dd;
                out[i] = (gs.colclamp & 1) ? (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v) : (uint32_t)v & 0xFF;
            }
        }
        if (gs.fba[ctx] & 1) {
            sa |= 0x80;
        }
        if (!write_a) {
            m |= 0xFF000000u;
        }
        c = out[0] | out[1] << 8 | out[2] << 16 | sa << 24;
        /* FRAME.FBMSK: the bits set are not written (given for 32-bit pixels, narrowed for 16-bit ones) */
        if (fbits == 16) {
            c = out[0] >> 3 | (out[1] >> 3) << 5 | (out[2] >> 3) << 10 | (sa & 0x80) << 8;
            m = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
        }
        if (m != 0) {
            c = (old & m) | (c & ~m);
        }
        vram_rw(fbp, fbw, fpsm, (uint32_t)x, (uint32_t)y, 1, c);
        sStat.pixels++;
        if (sTracePrimVb != 0 && sCurVb == sTracePrimVb && x == sTraceX && y == sTraceY) {
            printf("gs-pixel %d,%d fbp %X src %08X old %08X new %08X prim %03llX tex0 %016llX alpha %llX test %llX\n", x, y, fbp, src, old, c,
                   (unsigned long long)gs.prim, (unsigned long long)gs.tex0[ctx], (unsigned long long)al, (unsigned long long)test);
        }
        if (sTrace > 0) {
            sTr[fbp >> 5].px++;
            sTr[fbp >> 5].tex0 = tex_on() ? gs.tex0[ctx] : 0;
            sTr[fbp >> 5].frame = fr;
            sTr[fbp >> 5].test = test;
            sTr[fbp >> 5].alpha = al;
        }
    }
    if (write_z) {
        vram_rw(zbp, fbw, zpsm, (uint32_t)x, (uint32_t)y, 1, z);
    }
}

/* ------------------------------------------------------------------------------------------------ primitives */

/* A float's value times 2^shift, from its bits (EE floats: exponent 0 is zero), clamped to +/- 2^62. */
static int64_t fix(uint32_t bits, int shift) {
    int e = (int)((bits >> 23) & 0xFF), sh;
    int64_t m = (int64_t)((bits & 0x7FFFFF) | 0x800000), r;

    if (e == 0) {
        return 0;
    }
    sh = e - 150 + shift;
    if (sh > 38) {
        r = (int64_t)1 << 62;
    } else if (sh >= 0) {
        r = m << sh;
    } else if (sh > -32) {
        r = m >> -sh;
    } else {
        r = 0;
    }
    return (bits >> 31) ? -r : r;
}

/* The shift that puts the largest of n floats near 2^36 (S, T and Q of a primitive share it, so S / Q keeps its
   precision however small Q gets: far away Q is about 2^-14, which a fixed 2^20 scale left with 6 bits). */
static int stq_shift(const uint32_t *bits, int n) {
    int i, e = 0;
    for (i = 0; i < n; i++) {
        int ei = (int)((bits[i] >> 23) & 0xFF);
        if (ei > e) {
            e = ei;
        }
    }
    return e == 0 ? 20 : 36 - (e - 127) - 1;
}

/* S / Q times the texture's size, in 1/16 texel. */
static int32_t stq_texel(int64_t s20, int64_t q20, int log2size) {
    int64_t r;
    if (q20 == 0) {
        q20 = 1;
    }
    if (s20 > ((int64_t)1 << 47) || s20 < -((int64_t)1 << 47)) { /* keep s * size inside 64 bits */
        s20 >>= 14;
        q20 >>= 14;
        if (q20 == 0) {
            q20 = 1;
        }
    }
    r = s20 * ((int64_t)16 << log2size) / q20;
    return r > 0x3FFFFFFF ? 0x3FFFFFFF : r < -0x3FFFFFFF ? -0x3FFFFFFF : (int32_t)r;
}

static int ctx_of(void) { return (int)((gs.prim >> 9) & 1); }
static int tex_on(void) { return (int)((gs.prim >> 4) & 1); }
static int fst_on(void) { return (int)((gs.prim >> 8) & 1); }
static int fog_on(void) { return (int)((gs.prim >> 5) & 1); }

/* The colour of a pixel from its interpolated colour, texture coordinates and fog. */
static uint32_t colour(int ctx, uint32_t c, int32_t u16, int32_t v16, uint32_t f) {
    if (tex_on()) {
        c = shade(ctx, c, u16, v16);
    }
    if (fog_on()) {
        c = fogged(c, f);
    }
    return c;
}

/* Scissor rectangle of the context, as a pixel range [x0, x1) x [y0, y1). */
static void scissor_box(int ctx, int *x0, int *x1, int *y0, int *y1) {
    uint64_t sc = gs.scissor[ctx];
    *x0 = (int)(sc & 0x7FF);
    *x1 = (int)((sc >> 16) & 0x7FF) + 1;
    *y0 = (int)((sc >> 32) & 0x7FF);
    *y1 = (int)((sc >> 48) & 0x7FF) + 1;
}

static int ceil16(int32_t v) { return (int)((v + 15) >> 4); }

static void draw_sprite(int ctx, const Vtx *a, const Vtx *b) {
    int32_t X0 = a->x, X1 = b->x, Y0 = a->y, Y1 = b->y;
    int32_t U0, U1, V0, V1;
    int xa, xb, ya, yb, x, y, sx0, sx1, sy0, sy1;
    uint32_t col = b->rgba;

    if (fst_on()) {
        U0 = a->u; U1 = b->u; V0 = a->v; V1 = b->v;
        sPixQ = fix(b->q, 20);
        sPixQSh = 20;
    } else {
        uint64_t t0 = gs.tex0[ctx];
        int lw = (int)((t0 >> 26) & 15), lh = (int)((t0 >> 30) & 15);
        uint32_t f[5];
        int sh;
        int64_t q;
        f[0] = a->s; f[1] = b->s; f[2] = a->t; f[3] = b->t; f[4] = b->q;
        sh = stq_shift(f, 5);
        q = fix(b->q, sh);
        sPixQ = q;
        sPixQSh = sh;
        U0 = stq_texel(fix(a->s, sh), q, lw); U1 = stq_texel(fix(b->s, sh), q, lw);
        V0 = stq_texel(fix(a->t, sh), q, lh); V1 = stq_texel(fix(b->t, sh), q, lh);
    }
    if (X0 > X1) { int32_t t = X0; X0 = X1; X1 = t; t = U0; U0 = U1; U1 = t; }
    if (Y0 > Y1) { int32_t t = Y0; Y0 = Y1; Y1 = t; t = V0; V0 = V1; V1 = t; }
    xa = ceil16(X0); xb = ceil16(X1); ya = ceil16(Y0); yb = ceil16(Y1);
    scissor_box(ctx, &sx0, &sx1, &sy0, &sy1);
    if (xa < sx0) { xa = sx0; }
    if (xb > sx1) { xb = sx1; }
    if (ya < sy0) { ya = sy0; }
    if (yb > sy1) { yb = sy1; }
    for (y = ya; y < yb; y++) {
        int32_t v16 = Y1 != Y0 ? V0 + (int32_t)((int64_t)(V1 - V0) * (y * 16 - Y0) / (Y1 - Y0)) : V0;
        for (x = xa; x < xb; x++) {
            int32_t u16 = X1 != X0 ? U0 + (int32_t)((int64_t)(U1 - U0) * (x * 16 - X0) / (X1 - X0)) : U0;
            pixel(ctx, x, y, b->z, colour(ctx, col, u16, v16, b->f));
        }
    }
}

/* Edge function of a -> b at p (12.4 coordinates): positive on one side, in 1/256 square pixels. */
static int64_t edge(const Vtx *a, const Vtx *b, int32_t px, int32_t py) {
    return (int64_t)(b->x - a->x) * (py - a->y) - (int64_t)(b->y - a->y) * (px - a->x);
}

/* Whether the edge a -> b (inside on its positive side once `sign` is applied) owns the pixels on it: left and top
   edges do (the inside lies to +x, or for a horizontal edge to +y). */
static int top_left(const Vtx *a, const Vtx *b, int sign) {
    int64_t A = -(int64_t)(b->y - a->y) * sign, B = (int64_t)(b->x - a->x) * sign;
    return A > 0 || (A == 0 && B > 0);
}

#define LERP(a0, a1, a2) ((a0) + (((int64_t)((a1) - (a0)) * lb + (int64_t)((a2) - (a0)) * lc) >> 24))

static void draw_triangle(int ctx, const Vtx *a, const Vtx *b, const Vtx *c) {
    int64_t area = edge(a, b, c->x, c->y);
    int sign = area < 0 ? -1 : 1, gouraud = (int)((gs.prim >> 3) & 1), tex = tex_on(), fst = fst_on();
    int bias0, bias1, bias2, x, y, x0, x1, y0, y1, sx0, sx1, sy0, sy1;
    int32_t minx, maxx, miny, maxy;
    int64_t S[3] = {0, 0, 0}, T[3] = {0, 0, 0}, Q[3] = {0, 0, 0};
    int qsh = 20;
    int lw = (int)((gs.tex0[ctx] >> 26) & 15), lh = (int)((gs.tex0[ctx] >> 30) & 15);
    const Vtx *v[3];

    if (area == 0) {
        return;
    }
    area *= sign;
    v[0] = a; v[1] = b; v[2] = c;
    if (tex && !fst) {
        uint32_t f[9];
        for (x = 0; x < 3; x++) {
            f[x * 3] = v[x]->s;
            f[x * 3 + 1] = v[x]->t;
            f[x * 3 + 2] = v[x]->q;
        }
        qsh = stq_shift(f, 9);
        for (x = 0; x < 3; x++) {
            S[x] = fix(v[x]->s, qsh);
            T[x] = fix(v[x]->t, qsh);
            Q[x] = fix(v[x]->q, qsh);
        }
    }
    if (tex && fst) {
        sPixQ = fix(c->q, 20);
        sPixQSh = 20;
    }
    bias0 = top_left(b, c, sign) ? 0 : 1;
    bias1 = top_left(c, a, sign) ? 0 : 1;
    bias2 = top_left(a, b, sign) ? 0 : 1;
    minx = a->x; maxx = a->x; miny = a->y; maxy = a->y;
    for (x = 1; x < 3; x++) {
        if (v[x]->x < minx) { minx = v[x]->x; }
        if (v[x]->x > maxx) { maxx = v[x]->x; }
        if (v[x]->y < miny) { miny = v[x]->y; }
        if (v[x]->y > maxy) { maxy = v[x]->y; }
    }
    x0 = ceil16(minx); x1 = (int)(maxx >> 4) + 1; y0 = ceil16(miny); y1 = (int)(maxy >> 4) + 1;
    scissor_box(ctx, &sx0, &sx1, &sy0, &sy1);
    if (x0 < sx0) { x0 = sx0; }
    if (x1 > sx1) { x1 = sx1; }
    if (y0 < sy0) { y0 = sy0; }
    if (y1 > sy1) { y1 = sy1; }
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            int32_t px = x * 16, py = y * 16, u16 = 0, v16 = 0;
            int64_t w0 = edge(b, c, px, py) * sign, w1 = edge(c, a, px, py) * sign, w2 = edge(a, b, px, py) * sign, lb, lc;
            uint32_t col, z, f;

            if (w0 < bias0 || w1 < bias1 || w2 < bias2) {
                continue;
            }
            lb = (w1 << 24) / area;
            lc = (w2 << 24) / area;
            z = (uint32_t)LERP((int64_t)a->z, (int64_t)b->z, (int64_t)c->z);
            if (gouraud) {
                int k;
                col = 0;
                for (k = 0; k < 32; k += 8) {
                    int64_t ch = LERP((int64_t)((a->rgba >> k) & 0xFF), (int64_t)((b->rgba >> k) & 0xFF), (int64_t)((c->rgba >> k) & 0xFF));
                    col |= (uint32_t)(ch < 0 ? 0 : ch > 255 ? 255 : ch) << k;
                }
                f = (uint32_t)LERP((int64_t)a->f, (int64_t)b->f, (int64_t)c->f);
            } else {
                col = c->rgba;
                f = c->f;
            }
            if (tex) {
                if (fst) {
                    u16 = (int32_t)LERP((int64_t)a->u, (int64_t)b->u, (int64_t)c->u);
                    v16 = (int32_t)LERP((int64_t)a->v, (int64_t)b->v, (int64_t)c->v);
                } else {
                    int64_t q = LERP(Q[0], Q[1], Q[2]);
                    sPixQ = q;
                    sPixQSh = qsh;
                    u16 = stq_texel(LERP(S[0], S[1], S[2]), q, lw);
                    v16 = stq_texel(LERP(T[0], T[1], T[2]), q, lh);
                }
            }
            pixel(ctx, x, y, z, colour(ctx, col, u16, v16, f > 255 ? 255 : f));
        }
    }
}

static void draw_line(int ctx, const Vtx *a, const Vtx *b) {
    int32_t dx = b->x - a->x, dy = b->y - a->y;
    int32_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int n = (int)((adx > ady ? adx : ady) >> 4), i;

    for (i = 0; i <= n; i++) {
        int32_t x = a->x + (n ? (int32_t)((int64_t)dx * i / n) : 0), y = a->y + (n ? (int32_t)((int64_t)dy * i / n) : 0);
        pixel(ctx, (int)(x >> 4), (int)(y >> 4), b->z, fog_on() ? fogged(b->rgba, b->f) : b->rgba);
    }
}

/* A vertex arrives: XYZ2 / XYZF2 kick (draw when the primitive is complete), XYZ3 / XYZF3 only queue. */
static void vertex(uint32_t x, uint32_t y, uint32_t z, uint32_t f, int kick) {
    int ctx = ctx_of(), type = (int)(gs.prim & 7);
    Vtx v;

    v.x = (int32_t)x - (int32_t)(gs.xyoffset[ctx] & 0xFFFF);
    v.y = (int32_t)y - (int32_t)((gs.xyoffset[ctx] >> 32) & 0xFFFF);
    v.z = z;
    v.f = f;
    v.rgba = (uint32_t)gs.rgbaq;
    v.s = (uint32_t)gs.st;
    v.t = (uint32_t)(gs.st >> 32);
    v.q = gs.q;
    v.u = (int32_t)(gs.uv & 0x3FFF);
    v.v = (int32_t)((gs.uv >> 16) & 0x3FFF);
    if (gs.vcount < 3) {
        gs.vtx[gs.vcount++] = v;
    } else {
        gs.vtx[0] = gs.vtx[1];
        gs.vtx[1] = gs.vtx[2];
        gs.vtx[2] = v;
    }
    if (kick && sTracePrimVb != 0 && sCurVb == sTracePrimVb) {
        printf("gs-prim t%d ctx%d prim %03llX xy %d,%d z %X uv %d,%d st %08X,%08X q %08X rgba %08X tex0 %016llX tex1 %llX clamp %llX frame %llX test %llX alpha %llX texa %llX\n",
               type, ctx, (unsigned long long)gs.prim, v.x >> 4, v.y >> 4, v.z, v.u >> 4, v.v >> 4, v.s, v.t, v.q, v.rgba,
               (unsigned long long)gs.tex0[ctx], (unsigned long long)gs.tex1[ctx], (unsigned long long)gs.clamp[ctx], (unsigned long long)gs.frame[ctx],
               (unsigned long long)gs.test[ctx], (unsigned long long)gs.alpha[ctx], (unsigned long long)gs.texa);
    }
    switch (type) {
    case 0:
        if (kick) {
            sStat.prims++;
            pixel(ctx, (int)(v.x >> 4), (int)(v.y >> 4), v.z, fog_on() ? fogged(v.rgba, v.f) : v.rgba);
        }
        gs.vcount = 0;
        break;
    case 1:
    case 2:
        if (gs.vcount == 2) {
            if (kick) {
                sStat.prims++;
                draw_line(ctx, &gs.vtx[0], &gs.vtx[1]);
            }
            if (type == 2) {
                gs.vtx[0] = gs.vtx[1];
                gs.vcount = 1;
            } else {
                gs.vcount = 0;
            }
        }
        break;
    case 3:
        if (gs.vcount == 3) {
            if (kick) {
                sStat.prims++;
                draw_triangle(ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]);
            }
            gs.vcount = 0;
        }
        break;
    case 4: /* strip: the queue keeps sliding */
        if (gs.vcount == 3 && kick) {
            sStat.prims++;
            draw_triangle(ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]);
        }
        break;
    case 5: /* fan: the first vertex stays */
        if (gs.vcount == 3) {
            if (kick) {
                sStat.prims++;
                draw_triangle(ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]);
            }
            gs.vtx[1] = gs.vtx[2];
            gs.vcount = 2;
        }
        break;
    case 6:
        if (gs.vcount == 2) {
            if (kick) {
                sStat.prims++;
                draw_sprite(ctx, &gs.vtx[0], &gs.vtx[1]);
            }
            gs.vcount = 0;
        }
        break;
    default:
        gs.vcount = 0;
        break;
    }
}

/* ------------------------------------------------------------------------------------------------ transfers */

static void transfer_begin(void) {
    uint32_t dir = gs.trxdir & 3;
    gs.transfer = dir == 0; /* host -> GS */
    gs.tx = (gs.trxpos >> 32) & 0x7FF;
    gs.ty = (gs.trxpos >> 48) & 0x7FF;
    gs.tw = gs.trxreg & 0xFFF;
    gs.th = (gs.trxreg >> 32) & 0xFFF;
    gs.tpos = 0;
    if (sTrace > 0) {
        printf("gs-xfer %u dir %u bitbltbuf %016llX trxpos %llX trxreg %llX\n", sCurVb, dir, (unsigned long long)gs.bitbltbuf,
               (unsigned long long)gs.trxpos, (unsigned long long)gs.trxreg);
    }
    if (dir == 1) {
        sStat.readbacks++; /* GS -> host: not modelled */
    } else if (dir == 2) { /* local -> local, at once */
        uint32_t sbp = gs.bitbltbuf & 0x3FFF, sbw = (gs.bitbltbuf >> 16) & 0x3F, spsm = (gs.bitbltbuf >> 24) & 0x3F;
        uint32_t dbp = (gs.bitbltbuf >> 32) & 0x3FFF, dbw = (gs.bitbltbuf >> 48) & 0x3F, dpsm = (gs.bitbltbuf >> 56) & 0x3F;
        uint32_t sx = gs.trxpos & 0x7FF, sy = (gs.trxpos >> 16) & 0x7FF, x, y;
        for (y = 0; y < gs.th; y++) {
            for (x = 0; x < gs.tw; x++) {
                vram_rw(dbp, dbw, dpsm, gs.tx + x, gs.ty + y, 1, vram_rw(sbp, sbw, spsm, sx + x, sy + y, 0, 0));
            }
        }
        sStat.localCopies++;
        sUploadGen++;
    }
}

/* Image data of a host -> GS transfer: whole pixels in the destination format, row after row. */
static void transfer_data(const uint8_t *p, uint32_t bytes) {
    uint32_t dbp = (gs.bitbltbuf >> 32) & 0x3FFF, dbw = (gs.bitbltbuf >> 48) & 0x3F, dpsm = (gs.bitbltbuf >> 56) & 0x3F;
    uint32_t bits, total, i = 0;

    if (!gs.transfer || gs.tw == 0) {
        return;
    }
    sUploadGen++;
    bits = (uint32_t)psm_bits(dpsm);
    total = gs.tw * gs.th;
    /* the usual formats a row at a time through the page tables (the same writes as vram_rw) */
    if (dpsm == 0x00 || dpsm == 0x02 || dpsm == 0x0A || dpsm == 0x13 || dpsm == 0x14) {
        uint32_t bw = dbw ? dbw : 1, count = bytes * 8 / bits;
        uint8_t *vram8 = (uint8_t *)sVram;
        uint16_t *vram16 = (uint16_t *)sVram;
        while (i < count && gs.tpos < total) {
            uint32_t col = gs.tpos % gs.tw, y = (gs.ty + gs.tpos / gs.tw) & 0x7FF, x = gs.tx + col, run = gs.tw - col, j;
            if (run > count - i) {
                run = count - i;
            }
            if (dpsm == 0x13) {
                const uint16_t *in = sIn8[y & 63];
                uint32_t base = (dbp + (y >> 6) * 32 * (bw >> 1)) << 8;
                for (j = 0; j < run; j++, x++) {
                    vram8[(base + (((x & 0x7FF) >> 7) << 13) + in[x & 127]) & 0x3FFFFF] = p[i + j];
                }
            } else if (dpsm == 0x14) {
                const uint16_t *in = sIn4[y & 127];
                uint32_t base = (dbp + (y >> 7) * 32 * (bw >> 1)) << 9;
                for (j = 0; j < run; j++, x++) {
                    uint32_t a = base + (((x & 0x7FF) >> 7) << 14) + in[x & 127], k = i + j;
                    uint8_t *b = &vram8[(a >> 1) & 0x3FFFFF];
                    uint32_t c = (p[k >> 1] >> ((k & 1) * 4)) & 15;
                    *b = (a & 1) ? (uint8_t)((*b & 0x0F) | c << 4) : (uint8_t)((*b & 0xF0) | c);
                }
            } else if (dpsm == 0x00) {
                const uint16_t *in = sIn32[y & 31];
                uint32_t base = (dbp + (y >> 5) * 32 * bw) << 6;
                for (j = 0; j < run; j++, x++) {
                    memcpy(&sVram[(base + (((x & 0x7FF) >> 6) << 11) + in[x & 63]) & 0xFFFFF], &p[(i + j) * 4], 4);
                }
            } else {
                const uint16_t *in = sIn16[dpsm == 0x0A][y & 63];
                uint32_t base = (dbp + (y >> 6) * 32 * bw) << 7;
                for (j = 0; j < run; j++, x++) {
                    memcpy(&vram16[(base + (((x & 0x7FF) >> 6) << 12) + in[x & 63]) & 0x1FFFFF], &p[(i + j) * 2], 2);
                }
            }
            i += run;
            gs.tpos += run;
        }
        return;
    }
    for (; (i + 1) * bits <= bytes * 8 && gs.tpos < total; gs.tpos++, i++) {
        uint32_t x = gs.tx + gs.tpos % gs.tw, y = gs.ty + gs.tpos / gs.tw, c;
        switch (bits) {
        case 32: c = p[i * 4] | p[i * 4 + 1] << 8 | p[i * 4 + 2] << 16 | (uint32_t)p[i * 4 + 3] << 24; break;
        case 24: c = p[i * 3] | p[i * 3 + 1] << 8 | (uint32_t)p[i * 3 + 2] << 16; break;
        case 16: c = p[i * 2] | (uint32_t)p[i * 2 + 1] << 8; break;
        case 8: c = p[i]; break;
        default: c = (p[i / 2] >> ((i & 1) * 4)) & 15; break;
        }
        vram_rw(dbp, dbw, dpsm, x, y, 1, c);
    }
}

/* ------------------------------------------------------------------------------------------------ registers */

/* PRMODECONT.AC = 1: the attributes are PRIM's own; AC = 0: PRMODE's, with PRIM's type. */
static void prim_update(void) {
    gs.prim = (gs.prmodecont & 1) ? gs.primRaw : (gs.primRaw & 7) | (gs.prmode & 0x7F8);
}

static void reg_write(uint32_t addr, uint64_t d) {
    sStat.regs++;
    switch (addr) {
    case 0x00: gs.primRaw = d; prim_update(); gs.vcount = 0; break;
    case 0x01: gs.rgbaq = d; gs.q = (uint32_t)(d >> 32); break;
    case 0x02: gs.st = d; break;
    case 0x03: gs.uv = d; break;
    case 0x04: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)((d >> 32) & 0xFFFFFF), (uint32_t)(d >> 56), 1); break;
    case 0x05: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)(d >> 32), gs.fog, 1); break;
    case 0x06: case 0x07: gs.tex0[addr - 6] = d; clut_load(d); break;
    case 0x08: case 0x09: gs.clamp[addr - 8] = d; break;
    case 0x0A: gs.fog = (uint32_t)(d >> 56); break;
    case 0x0C: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)((d >> 32) & 0xFFFFFF), (uint32_t)(d >> 56), 0); break;
    case 0x0D: vertex(d & 0xFFFF, (d >> 16) & 0xFFFF, (uint32_t)(d >> 32), gs.fog, 0); break;
    case 0x14: case 0x15: gs.tex1[addr - 0x14] = d; break;
    case 0x16: case 0x17: /* TEX2: the format and CLUT fields of TEX0 */
        gs.tex0[addr - 0x16] = (gs.tex0[addr - 0x16] & ~0xFFFFFFE003F00000ull) | (d & 0xFFFFFFE003F00000ull);
        clut_load(gs.tex0[addr - 0x16]);
        break;
    case 0x18: case 0x19: gs.xyoffset[addr - 0x18] = d; break;
    case 0x1A: gs.prmodecont = d; prim_update(); break;
    case 0x1B: gs.prmode = d; prim_update(); break;
    case 0x1C: gs.texclut = d; break;
    case 0x3B: gs.texa = d; break;
    case 0x3D: gs.fogcol = d; break;
    case 0x40: case 0x41: gs.scissor[addr - 0x40] = d; break;
    case 0x42: case 0x43: gs.alpha[addr - 0x42] = d; break;
    case 0x46: gs.colclamp = d; break;
    case 0x47: case 0x48: gs.test[addr - 0x47] = d; break;
    case 0x49: gs.pabe = d; break;
    case 0x4A: case 0x4B: gs.fba[addr - 0x4A] = d & 1; break;
    case 0x4C: case 0x4D: gs.frame[addr - 0x4C] = d; break;
    case 0x4E: case 0x4F: gs.zbuf[addr - 0x4E] = d; break;
    case 0x50: gs.bitbltbuf = d; break;
    case 0x51: gs.trxpos = d; break;
    case 0x52: gs.trxreg = d; break;
    case 0x53: gs.trxdir = d; transfer_begin(); break;
    case 0x60: { /* SIGNAL: SIGLBLID.SIGID under the mask, CSR.SIGNAL */
        uint64_t id = PRIV(GS_SIGLBLID), m = d >> 32;
        PRIV(GS_SIGLBLID) = (id & ~m) | (d & m & 0xFFFFFFFFull);
        PRIV(GS_CSR) |= 1;
        break;
    }
    case 0x61: PRIV(GS_CSR) |= 2; break; /* FINISH */
    case 0x62: { /* LABEL: SIGLBLID.LBLID under the mask */
        uint64_t id = PRIV(GS_SIGLBLID), m = (d >> 32) << 32;
        PRIV(GS_SIGLBLID) = (id & ~m) | ((d << 32) & m);
        break;
    }
    default: break; /* TEX1, MIPTBP, DTHE, DIMX, SCANMSK, TEXFLUSH...: no effect here */
    }
}

void Gs_RegWrite(uint32_t addr, uint64_t d) {
    tables_init();
    reg_write(addr, d);
}

/* ------------------------------------------------------------------------------------------------ GIF */

/* `qwc` quadwords of GIF data on one path; a packet may continue in the next call. */
void Gs_Gif(int path, const uint8_t *p, uint32_t qwc) {
    uint64_t q[2];
    tables_init();
    path &= 3;
    sBlock = p;
    sBlockQwc = qwc;
    while (qwc != 0) {
        uint32_t flg, nreg;
        if (sPath[path].loops == 0) {
            memcpy(q, p, 16);
            p += 16;
            qwc--;
            sStat.tags++;
            sPathTags[path]++;
            if (sTrace > 0 && ((q[0] >> 16) & 0x3FFFFFFFull) != 0 && sTraceBad < 4) {
                sTraceBad++;
                printf("gs-trace bad tag path %d at %p (block %p qwc %u): %016llX %016llX, previous %016llX %016llX\n", path,
                       (const void *)(p - 16), (const void *)sBlock, sBlockQwc, (unsigned long long)q[0], (unsigned long long)q[1],
                       (unsigned long long)sPath[path].lo, (unsigned long long)sPath[path].hi);
            }
            sPath[path].lo = q[0];
            sPath[path].hi = q[1];
            sPath[path].loops = q[0] & 0x7FFF;
            sPath[path].reg = 0;
            if (sPath[path].loops != 0 && ((q[0] >> 46) & 1) && ((q[0] >> 58) & 3) == 0) {
                reg_write(0, (q[0] >> 47) & 0x7FF); /* PRE: PRIM from the tag */
            }
            continue;
        }
        flg = (sPath[path].lo >> 58) & 3;
        nreg = (sPath[path].lo >> 60) & 15;
        if (nreg == 0) {
            nreg = 16;
        }
        if (flg == 0) { /* PACKED: one quadword per register */
            uint32_t r = (uint32_t)(sPath[path].hi >> (4 * sPath[path].reg)) & 15;
            uint64_t lo, hi;
            memcpy(q, p, 16);
            lo = q[0];
            hi = q[1];
            switch (r) {
            case 0x0: reg_write(0, lo & 0x7FF); break;
            case 0x1: /* RGBAQ: Q from the last ST */
                reg_write(1, (lo & 0xFF) | ((lo >> 32) & 0xFF) << 8 | (hi & 0xFF) << 16 | ((hi >> 32) & 0xFF) << 24 | (uint64_t)gs.q << 32);
                break;
            case 0x2: gs.st = lo; gs.q = (uint32_t)hi; break;
            case 0x3: reg_write(3, (lo & 0x3FFF) | ((lo >> 32) & 0x3FFF) << 16); break;
            case 0x4: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)((hi >> 4) & 0xFFFFFF), (uint32_t)((hi >> 36) & 0xFF), !((hi >> 47) & 1)); break;
            case 0x5: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)hi, gs.fog, !((hi >> 47) & 1)); break;
            case 0xA: gs.fog = (uint32_t)((hi >> 36) & 0xFF); break;
            case 0xC: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)((hi >> 4) & 0xFFFFFF), (uint32_t)((hi >> 36) & 0xFF), 0); break;
            case 0xD: vertex(lo & 0xFFFF, (lo >> 32) & 0xFFFF, (uint32_t)hi, gs.fog, 0); break;
            case 0xE: reg_write((uint32_t)(hi & 0xFF), lo); break; /* A+D */
            case 0xF: break;                                        /* NOP */
            default: reg_write(r, lo); break;
            }
            p += 16;
            qwc--;
            if (++sPath[path].reg == nreg) {
                sPath[path].reg = 0;
                sPath[path].loops--;
            }
        } else if (flg == 1) { /* REGLIST: 64 bits per register */
            int half;
            memcpy(q, p, 16);
            for (half = 0; half < 2 && sPath[path].loops != 0; half++) {
                uint32_t r = (uint32_t)(sPath[path].hi >> (4 * sPath[path].reg)) & 15;
                if (r != 0xE && r != 0xF) {
                    reg_write(r, q[half]);
                }
                if (++sPath[path].reg == nreg) {
                    sPath[path].reg = 0;
                    sPath[path].loops--;
                }
            }
            p += 16;
            qwc--;
        } else { /* IMAGE (and the undefined 3, treated alike) */
            uint32_t n = sPath[path].loops < qwc ? sPath[path].loops : qwc;
            sStat.imageQw += n;
            transfer_data(p, n * 16);
            p += 16 * n;
            qwc -= n;
            sPath[path].loops -= n;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ frame dump */

static uint32_t crc_table[256];

static uint32_t crc32_update(uint32_t c, const uint8_t *p, size_t n) {
    size_t i;
    if (crc_table[1] == 0) {
        uint32_t k, j, v;
        for (k = 0; k < 256; k++) {
            for (v = k, j = 0; j < 8; j++) {
                v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            }
            crc_table[k] = v;
        }
    }
    for (i = 0; i < n; i++) {
        c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    }
    return c;
}

static void put32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16); b[2] = (uint8_t)(v >> 8); b[3] = (uint8_t)v;
}

static void chunk(FILE *f, const char *kind, const uint8_t *data, uint32_t n) {
    uint8_t b[4];
    uint32_t c = crc32_update(0xFFFFFFFFu, (const uint8_t *)kind, 4);
    c = crc32_update(c, data, n) ^ 0xFFFFFFFFu;
    put32(b, n);
    fwrite(b, 1, 4, f);
    fwrite(kind, 1, 4, f);
    fwrite(data, 1, n, f);
    put32(b, c);
    fwrite(b, 1, 4, f);
}

/* An 8-bit RGB PNG: rows with filter 0 in stored (uncompressed) deflate blocks. */
static int write_png(const char *path, uint32_t w, uint32_t h, const uint8_t *rgb) {
    size_t raw = (size_t)(w * 3 + 1) * h, blocks = (raw + 65534) / 65535, n = 2 + raw + blocks * 5 + 4, pos = 2, done = 0, y;
    uint8_t *z = malloc(n), ihdr[13];
    uint8_t *rows = malloc(raw);
    uint32_t s1 = 1, s2 = 0;
    FILE *f;

    if (z == NULL || rows == NULL) {
        free(z);
        free(rows);
        return 0;
    }
    for (y = 0; y < h; y++) {
        rows[y * (w * 3 + 1)] = 0;
        memcpy(&rows[y * (w * 3 + 1) + 1], &rgb[y * w * 3], w * 3);
    }
    z[0] = 0x78;
    z[1] = 0x01;
    while (done < raw) {
        size_t len = raw - done > 65535 ? 65535 : raw - done, i;
        z[pos] = done + len == raw ? 1 : 0;
        z[pos + 1] = (uint8_t)len; z[pos + 2] = (uint8_t)(len >> 8);
        z[pos + 3] = (uint8_t)~len; z[pos + 4] = (uint8_t)(~len >> 8);
        memcpy(&z[pos + 5], &rows[done], len);
        for (i = 0; i < len; i++) {
            s1 = (s1 + rows[done + i]) % 65521;
            s2 = (s2 + s1) % 65521;
        }
        pos += 5 + len;
        done += len;
    }
    put32(&z[pos], s2 << 16 | s1);
    pos += 4;
    f = fopen(path, "wb");
    if (f == NULL) {
        free(z);
        free(rows);
        return 0;
    }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    put32(ihdr, w);
    put32(ihdr + 4, h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, (uint32_t)pos);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(z);
    free(rows);
    return 1;
}

/* The picture the display circuits show now, as RGB: the circuit PMODE enables (1 first), its DISPFB and DISPLAY.
   Interlaced frame mode (SMODE2 INT and FFMD) shows each buffer line in both fields: doubled. *w / *h: the size. */
static uint8_t *displayed(uint32_t *w, uint32_t *h) {
    uint64_t pmode = PRIV(GS_PMODE), smode2 = PRIV(GS_SMODE2), fb, disp;
    uint32_t fbp, fbw, psm, dbx, dby, magh, magv, dw, dh, width, lines, buflines, x, y, dbl;
    uint8_t *rgb;
    int second = !(pmode & 1) && (pmode & 2);

    fb = PRIV(second ? GS_DISPFB2 : GS_DISPFB1);
    disp = PRIV(second ? GS_DISPLAY2 : GS_DISPLAY1);
    fbp = (uint32_t)(fb & 0x1FF) << 5;
    fbw = (uint32_t)(fb >> 9) & 0x3F;
    psm = (uint32_t)(fb >> 15) & 0x1F;
    dbx = (uint32_t)(fb >> 32) & 0x7FF;
    dby = (uint32_t)(fb >> 43) & 0x7FF;
    magh = (uint32_t)(disp >> 23) & 15;
    magv = (uint32_t)(disp >> 27) & 3;
    dw = (uint32_t)(disp >> 32) & 0xFFF;
    dh = (uint32_t)(disp >> 44) & 0x7FF;
    width = (dw + 1) / (magh + 1);
    lines = (dh + 1) / (magv + 1);
    if (disp == 0 || width == 0 || lines == 0) { /* nothing set up yet: the usual 640 x 448 */
        width = 640;
        lines = 448;
    }
    if (width > 1024) { width = 1024; }
    if (lines > 1024) { lines = 1024; }
    dbl = (smode2 & 3) == 3 ? 2 : 1;
    buflines = lines / dbl;
    rgb = calloc((size_t)width * buflines * dbl, 3);
    if (rgb == NULL) {
        return NULL;
    }
    for (y = 0; y < buflines; y++) {
        for (x = 0; x < width; x++) {
            uint32_t c = vram_rw(fbp, fbw, psm, dbx + x, dby + y, 0, 0), k;
            if ((int)x == sTraceX && (int)y == sTraceY) { printf("gs-disp %d,%d fbp %X fbw %u psm %X dbx %u dby %u -> %08X\n", x, y, fbp, fbw, psm, dbx, dby, c); }
            if (psm_bits(psm) == 16) {
                c = (c & 31) << 3 | ((c >> 5) & 31) << 11 | ((c >> 10) & 31) << 19;
            }
            for (k = 0; k < dbl; k++) {
                uint8_t *o = &rgb[((size_t)(y * dbl + k) * width + x) * 3];
                o[0] = (uint8_t)c; o[1] = (uint8_t)(c >> 8); o[2] = (uint8_t)(c >> 16);
            }
        }
    }
    *w = width;
    *h = buflines * dbl;
    return rgb;
}

void Gs_VBlank(uint32_t vblank) {
    static int init;
    static const char *dir;
    static uint32_t every;
    char name[4096];
    uint32_t w, h;
    uint8_t *rgb;

    if (!init) {
        const char *e = getenv("TTT_DUMP_EVERY");
        init = 1;
        dir = getenv("TTT_DUMP");
        every = e != NULL ? (uint32_t)strtoul(e, NULL, 0) : 60;
        if (every == 0) {
            every = 60;
        }
    }
    sCurVb = vblank + 1;
    if (sTrace < 0) {
        const char *t = getenv("TTT_GS_TRACE"), *pv = getenv("TTT_GS_TRACE_PRIMS");
        sTrace = t != NULL && *t != 0 && *t != '0';
        sTracePrimVb = pv != NULL ? (uint32_t)strtoul(pv, NULL, 0) : 0;
        if (getenv("TTT_GS_TRACE_PIXEL")) {
            char *e;
            sTraceX = (int)strtol(getenv("TTT_GS_TRACE_PIXEL"), &e, 0);
            sTraceY = (int)strtol(e + 1, NULL, 0);
        }
    }
    if (sTrace > 0) {
        uint32_t i;
        for (i = 0; i < 512; i++) {
            if (sTr[i].px != 0) {
                printf("gs-trace %u fbp %03X px %u frame %016llX tex0 %016llX test %llX alpha %llX\n", (unsigned)vblank, (unsigned)i,
                       (unsigned)sTr[i].px, (unsigned long long)sTr[i].frame, (unsigned long long)sTr[i].tex0,
                       (unsigned long long)sTr[i].test, (unsigned long long)sTr[i].alpha);
            }
        }
        memset(sTr, 0, sizeof sTr);
        { /* each path's tags this VBlank and its tag state (a path left inside a packet swallows what follows) */
            int k;
            for (k = 1; k < 4; k++) {
                printf("gs-trace %u path %d tags %u loops %u tag %016llX %016llX\n", (unsigned)vblank, k, sPathTags[k], sPath[k].loops,
                       (unsigned long long)sPath[k].lo, (unsigned long long)sPath[k].hi);
                sPathTags[k] = 0;
            }
            printf("gs-trace %u dispfb1 %llX dispfb2 %llX pmode %llX\n", (unsigned)vblank, (unsigned long long)PRIV(GS_DISPFB1),
                   (unsigned long long)PRIV(GS_DISPFB2), (unsigned long long)PRIV(GS_PMODE));
        }
    }
    { /* TTT_GS_TEXDUMP=<vblank>:<tex0>:<x>:<y>:<w>:<h> writes that texture region (through a CLUT loaded from TEX0) */
        const char *td = getenv("TTT_GS_TEXDUMP");
        if (td != NULL && dir != NULL && strtoul(td, NULL, 0) == vblank) {
            char *e;
            unsigned long long t0;
            uint32_t x0, y0, tw, th, xx, yy;
            uint8_t *img;
            uint16_t saved[512];
            strtoul(td, &e, 0);
            t0 = strtoull(e + 1, &e, 16);
            x0 = (uint32_t)strtoul(e + 1, &e, 0);
            y0 = (uint32_t)strtoul(e + 1, &e, 0);
            tw = (uint32_t)strtoul(e + 1, &e, 0);
            th = (uint32_t)strtoul(e + 1, &e, 0);
            img = calloc((size_t)tw * th, 3);
            memcpy(saved, gs.clut, sizeof saved);
            clut_load((t0 & ~(7ull << 61)) | (1ull << 61));
            for (yy = 0; yy < th; yy++) {
                for (xx = 0; xx < tw; xx++) {
                    uint32_t psm = (uint32_t)(t0 >> 20) & 0x3F, c = vram_rw((uint32_t)t0 & 0x3FFF, (uint32_t)(t0 >> 14) & 0x3F, psm, x0 + xx, y0 + yy, 0, 0);
                    uint8_t *o = &img[(yy * tw + xx) * 3];
                    c = psm_bits(psm) > 8 ? expand(c, psm) : clut_read(t0, c, psm_bits(psm));
                    o[0] = (uint8_t)c; o[1] = (uint8_t)(c >> 8); o[2] = (uint8_t)(c >> 16);
                }
            }
            memcpy(gs.clut, saved, sizeof saved);
            snprintf(name, sizeof name, "%s/tex_%05u.png", dir, (unsigned)vblank);
            write_png(name, tw, th, img);
            free(img);
        }
    }
    if (dir == NULL || *dir == 0 || vblank % every != 0) {
        return;
    }
    tables_init();
    rgb = displayed(&w, &h);
    if (rgb == NULL) {
        return;
    }
    snprintf(name, sizeof name, "%s/frame_%05u.png", dir, (unsigned)vblank);
    if (!write_png(name, w, h, rgb)) {
        fprintf(stderr, "ttt: cannot write %s\n", name);
    }
    free(rgb);
}

void Gs_Report(void) {
    printf("ttt: gs: %u GIF tags, %u register writes, %u primitives, %u pixels, %u image qwords, %u local copies, "
           "%u readbacks (not modelled)\n",
           sStat.tags, sStat.regs, sStat.prims, sStat.pixels, sStat.imageQw, sStat.localCopies, sStat.readbacks);
    Gs_Vu1Report();
}

/* ------------------------------------------------------------------------------------------------ self-test */

static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: gs self-test FAILED: %s\n", what);
        sFails++;
    }
}

/* Where pixel (x, y) of a buffer at bp 0, width bw lands: the index of the 32-bit word (and the sub-unit). */
static int lands(uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, uint32_t word, uint32_t mask) {
    memset(sVram, 0, sizeof sVram);
    vram_rw(0, bw, psm, x, y, 1, 0xFFFFFFFFu);
    return sVram[word] == mask;
}

static void selftest_swizzle(void) {
    static const uint32_t kPsm[] = {0x00, 0x01, 0x02, 0x0A, 0x13, 0x14, 0x30, 0x31, 0x32, 0x3A};
    uint32_t i, x, y;

    /* PSMCT32: blocks of 8 x 8 in the page order 0 1 4 5 16 17 20 21 / 2 3 6 7..., columns 0 1 4 5 8 9 12 13 / 2 3... */
    expect(lands(1, 0x00, 0, 0, 0, 0xFFFFFFFFu) && lands(1, 0x00, 1, 0, 1, 0xFFFFFFFFu) && lands(1, 0x00, 2, 0, 4, 0xFFFFFFFFu) &&
           lands(1, 0x00, 0, 1, 2, 0xFFFFFFFFu) && lands(1, 0x00, 0, 2, 16, 0xFFFFFFFFu), "PSMCT32 columns");
    expect(lands(1, 0x00, 8, 0, 64, 0xFFFFFFFFu) && lands(1, 0x00, 0, 8, 128, 0xFFFFFFFFu) && lands(1, 0x00, 16, 0, 256, 0xFFFFFFFFu) &&
           lands(1, 0x00, 63, 31, 2047, 0xFFFFFFFFu), "PSMCT32 blocks in the page");
    expect(lands(1, 0x00, 0, 32, 2048, 0xFFFFFFFFu) && lands(2, 0x00, 64, 0, 2048, 0xFFFFFFFFu) && lands(2, 0x00, 0, 32, 4096, 0xFFFFFFFFu),
           "PSMCT32 pages across the buffer width");
    expect(lands(1, 0x01, 1, 0, 1, 0x00FFFFFFu) && lands(1, 0x1B, 1, 0, 1, 0xFF000000u) && lands(1, 0x24, 1, 0, 1, 0x0F000000u) &&
           lands(1, 0x2C, 1, 0, 1, 0xF0000000u), "PSMCT24, PSMT8H, PSMT4HL, PSMT4HH inside a 32-bit pixel");
    /* PSMCT16: 16 x 8 blocks, columns 0 2 8 10 16 18 24 26 1 3 9 11...; PSMCT16S: another block order */
    expect(lands(1, 0x02, 0, 0, 0, 0xFFFFu) && lands(1, 0x02, 1, 0, 1, 0xFFFFu) && lands(1, 0x02, 8, 0, 0, 0xFFFF0000u) &&
           lands(1, 0x02, 16, 0, 128, 0xFFFFu) && lands(1, 0x02, 0, 32, 1024, 0xFFFFu), "PSMCT16");
    expect(lands(1, 0x0A, 16, 0, 128, 0xFFFFu) && lands(1, 0x0A, 0, 32, 256, 0xFFFFu) && lands(1, 0x0A, 32, 0, 1024, 0xFFFFu), "PSMCT16S");
    /* PSMT8: 16 x 16 blocks; PSMT4: 32 x 16 blocks; both in pages of 128 pixels */
    expect(lands(2, 0x13, 0, 0, 0, 0xFFu) && lands(2, 0x13, 1, 0, 1, 0xFFu) && lands(2, 0x13, 8, 0, 0, 0xFF0000u) &&
           lands(2, 0x13, 0, 2, 8, 0xFF00u) && lands(2, 0x13, 16, 0, 64, 0xFFu) && lands(2, 0x13, 128, 0, 2048, 0xFFu), "PSMT8");
    expect(lands(2, 0x14, 0, 0, 0, 0xFu) && lands(2, 0x14, 1, 0, 1, 0xFu) && lands(2, 0x14, 8, 0, 0, 0xF00u) && lands(2, 0x14, 32, 0, 128, 0xFu) &&
           lands(2, 0x14, 128, 0, 2048, 0xFu), "PSMT4");
    /* the Z formats: the colour formats with the page's blocks XOR 24 */
    expect(lands(1, 0x30, 0, 0, 24 * 64, 0xFFFFFFFFu) && lands(1, 0x31, 8, 0, 25 * 64, 0x00FFFFFFu) && lands(1, 0x32, 0, 0, 24 * 64, 0xFFFFu) &&
           lands(1, 0x3A, 16, 0, 26 * 64, 0xFFFFu), "PSMZ32 / Z24 / Z16 / Z16S");
    /* the upload paths (row tables, generic) agree with the per-pixel addressing for every format */
    for (i = 0; i < sizeof kPsm / sizeof kPsm[0]; i++) {
        static uint8_t img[100 * 70 * 4];
        uint32_t psm = kPsm[i], bits = (uint32_t)psm_bits(psm), ok = 1, seed = 12345 + i;
        uint8_t tag[16];
        uint64_t t[2];
        memset(sVram, 0, sizeof sVram);
        for (x = 0; x < sizeof img; x++) {
            seed = seed * 1103515245u + 12345u;
            img[x] = (uint8_t)(seed >> 16);
        }
        Gs_RegWrite(0x50, (uint64_t)0x100 << 32 | (uint64_t)2 << 48 | (uint64_t)psm << 56);
        Gs_RegWrite(0x51, (uint64_t)5 << 32 | (uint64_t)3 << 48);
        Gs_RegWrite(0x52, 100 | (uint64_t)70 << 32);
        Gs_RegWrite(0x53, 0);
        t[0] = (100 * 70 * bits / 8 + 15) / 16 | 1ull << 15 | 2ull << 58;
        t[1] = 0;
        memcpy(tag, t, 16);
        Gs_Gif(3, tag, 1);
        Gs_Gif(3, img, (uint32_t)(t[0] & 0x7FFF));
        for (y = 0; y < 70 && ok; y++) {
            for (x = 0; x < 100 && ok; x++) {
                uint32_t k = y * 100 + x, want;
                switch (bits) {
                case 32: memcpy(&want, &img[k * 4], 4); break;
                case 24: want = img[k * 3] | img[k * 3 + 1] << 8 | (uint32_t)img[k * 3 + 2] << 16; break;
                case 16: want = img[k * 2] | (uint32_t)img[k * 2 + 1] << 8; break;
                case 8: want = img[k]; break;
                default: want = (img[k / 2] >> ((k & 1) * 4)) & 15; break;
                }
                ok = vram_rw(0x100, 2, psm, 5 + x, 3 + y, 0, 0) == want;
            }
        }
        if (!ok) {
            printf("ttt: gs self-test: upload of format 0x%02x differs at %u,%u\n", psm, x, y);
        }
        expect(ok, "IMAGE upload read back through the per-pixel addressing");
    }
}

/* A GIF packet builder for the test: A+D writes and IMAGE data under their own tags, sent on the GIF DMA channel. */
#define PK_BASE 0x00100000u
static uint64_t *sPk, *sPkTag;
static void pk_begin(void) {
    sPk = (uint64_t *)Port_Ptr(PK_BASE);
    sPkTag = NULL;
}
static void pk_close(void) {
    if (sPkTag != NULL) {
        sPkTag[0] = (uint64_t)((sPk - sPkTag) / 2 - 1) | 1ull << 60; /* NLOOP, PACKED, one register: A+D */
        sPkTag[1] = 0xE;
        sPkTag = NULL;
    }
}
static void pk_ad(uint32_t reg, uint64_t v) {
    if (sPkTag == NULL) {
        sPkTag = sPk;
        sPk += 2;
    }
    *sPk++ = v;
    *sPk++ = reg;
}
static void pk_image(const void *data, uint32_t qwc) {
    pk_close();
    sPk[0] = qwc | 2ull << 58;
    sPk[1] = 0;
    memcpy(sPk + 2, data, qwc * 16);
    sPk += 2 + 2 * qwc;
}
void Port_HwPoll(void);
static void pk_send(void) {
    pk_close();
    *(volatile uint32_t *)Port_Ptr(0x1000A010u) = PK_BASE;                                              /* D2_MADR */
    *(volatile uint32_t *)Port_Ptr(0x1000A020u) = (uint32_t)(sPk - (uint64_t *)Port_Ptr(PK_BASE)) / 2; /* D2_QWC */
    *(volatile uint32_t *)Port_Ptr(0x1000A000u) = 0x101; /* D2_CHCR: from memory, normal, STR */
    Port_HwPoll();
}
static uint64_t xyz(int x, int y, uint32_t z) { return (uint64_t)(x * 16) | (uint64_t)(y * 16) << 16 | (uint64_t)z << 32; }
static uint32_t at(int x, int y) { return vram_rw(0x80, 4, 0, (uint32_t)x, (uint32_t)y, 0, 0); }

static void selftest_draw(void) {
    uint32_t i, x, y, ok;

    memset(sVram, 0, sizeof sVram);
    Gs_Reset();
    /* a flat sprite on a 256-wide PSMCT32 buffer at block 0x80; depth at block 0x400 */
    pk_begin();
    pk_ad(0x4C, 4 | (uint64_t)4 << 16);            /* FRAME_1: FBP 4 (block 0x80), 256 wide, PSMCT32 */
    pk_ad(0x4E, 0x20 | 1ull << 32);                /* ZBUF_1: block 0x400, Z32, masked */
    pk_ad(0x40, 255ull << 16 | 255ull << 48);      /* SCISSOR_1 */
    pk_ad(0x18, 0);                                /* XYOFFSET_1 */
    pk_ad(0x47, 0);                                /* TEST_1 */
    pk_ad(0x00, 6);                                /* PRIM: sprite */
    pk_ad(0x01, 0x80112233u);
    pk_ad(0x05, xyz(10, 20, 0));
    pk_ad(0x05, xyz(30, 40, 0));
    pk_send();
    expect(at(10, 20) == 0x80112233u && at(29, 39) == 0x80112233u, "GIF PACKED sprite drawn on the GIF channel");
    expect(at(30, 39) == 0 && at(29, 40) == 0 && at(9, 20) == 0 && at(10, 19) == 0, "sprite edges exclusive at the far corner");
    /* scissor */
    pk_begin();
    pk_ad(0x40, 15ull << 16 | 255ull << 48);
    pk_ad(0x00, 6);
    pk_ad(0x01, 0x80445566u);
    pk_ad(0x05, xyz(10, 50, 0));
    pk_ad(0x05, xyz(30, 52, 0));
    pk_ad(0x40, 255ull << 16 | 255ull << 48);
    pk_send();
    expect(at(15, 50) == 0x80445566u && at(16, 50) == 0, "scissor");
    /* alpha blend (Cs - Cd) * As / 128 + Cd, As = 0x40, over black */
    pk_begin();
    pk_ad(0x42, 0 | 1 << 2 | 0 << 4 | 1 << 6);     /* ALPHA_1: A Cs, B Cd, C As, D Cd */
    pk_ad(0x00, 6 | 1 << 6);
    pk_ad(0x01, 0x400000FFu);
    pk_ad(0x05, xyz(40, 10, 0));
    pk_ad(0x05, xyz(44, 14, 0));
    pk_send();
    expect((at(41, 11) & 0xFFFFFF) == 0x00007F, "alpha blending");
    /* depth test GEQUAL with depth written: a nearer sprite stays, a farther one is rejected */
    pk_begin();
    pk_ad(0x4E, 0x20);                             /* Z32, written */
    pk_ad(0x47, 1ull << 16 | 2ull << 17);          /* ZTE, ZTST GEQUAL */
    pk_ad(0x00, 6);
    pk_ad(0x01, 0x80000011u);
    pk_ad(0x05, xyz(60, 10, 100));
    pk_ad(0x05, xyz(64, 14, 100));
    pk_ad(0x01, 0x80000022u);
    pk_ad(0x05, xyz(60, 10, 50));
    pk_ad(0x05, xyz(64, 14, 50));
    pk_send();
    expect(at(61, 11) == 0x80000011u && vram_rw(0x400, 4, 0x30, 61, 11, 0, 0) == 100, "depth test rejects the farther sprite");
    pk_begin();
    pk_ad(0x01, 0x80000033u);
    pk_ad(0x05, xyz(60, 10, 200));
    pk_ad(0x05, xyz(64, 14, 200));
    pk_ad(0x47, 0);
    pk_send();
    expect(at(61, 11) == 0x80000033u, "depth test passes the nearer sprite");
    /* textured sprite: PSMT8 texture (16 x 16, texel = x + 16 y) through a 256-colour CT32 palette (CSM1) */
    {
        static uint8_t tex[256];
        static uint32_t pal[256];
        for (i = 0; i < 256; i++) { /* the palette in CSM1 order: entry k at position k with bits 3 and 4 exchanged */
            tex[i] = (uint8_t)i;
            pal[(i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1)] = i * 0x010101u | 0x80000000u;
        }
        pk_begin();
        pk_ad(0x50, (uint64_t)0x200 << 32 | 2ull << 48 | 0x13ull << 56);
        pk_ad(0x51, 0);
        pk_ad(0x52, 16 | 16ull << 32);
        pk_ad(0x53, 0);
        pk_image(tex, 16);
        pk_ad(0x50, (uint64_t)0x300 << 32 | 1ull << 48);
        pk_ad(0x51, 0);
        pk_ad(0x52, 16 | 16ull << 32);
        pk_ad(0x53, 0);
        pk_image(pal, 64);
        pk_ad(0x06, 0x200 | 2ull << 14 | 0x13ull << 20 | 4ull << 26 | 4ull << 30 | 1ull << 34 | 1ull << 35 | (uint64_t)0x300 << 37 | 1ull << 61);
        pk_ad(0x08, 0);
        pk_ad(0x00, 6 | 1 << 4 | 1 << 8); /* sprite, textured, UV */
        pk_ad(0x01, 0x80808080u);
        pk_ad(0x03, 0);
        pk_ad(0x05, xyz(100, 100, 0));
        pk_ad(0x03, (16 * 16) | (uint64_t)(16 * 16) << 16);
        pk_ad(0x05, xyz(116, 116, 0));
        pk_send();
    }
    ok = 1;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            ok &= at(100 + (int)x, 100 + (int)y) == ((x + 16 * y) * 0x010101u | 0x80000000u);
        }
    }
    expect(ok, "PSMT8 texture through the CT32 CLUT (CSM1), decal");
    /* CLD 0 keeps the loaded palette even with another CBP */
    pk_begin();
    pk_ad(0x06, 0x200 | 2ull << 14 | 0x13ull << 20 | 4ull << 26 | 4ull << 30 | 1ull << 34 | 1ull << 35 | (uint64_t)0x3000 << 37);
    pk_ad(0x00, 6 | 1 << 4 | 1 << 8);
    pk_ad(0x03, 0);
    pk_ad(0x05, xyz(130, 100, 0));
    pk_ad(0x03, (16 * 16) | (uint64_t)(16 * 16) << 16);
    pk_ad(0x05, xyz(146, 116, 0));
    pk_send();
    expect(at(130 + 9, 100 + 3) == ((9 + 48) * 0x010101u | 0x80000000u), "CLD 0 keeps the CLUT buffer");
    /* bilinear magnification (TEX1 MMAG = 1, LCM = 1, K = 0: LOD 0): a 2 x 2 PSMCT32 texture, black | red, stretched
       over 32 x 4 pixels; the GS samples half a texel back and weighs the four texels with the 4-bit fractions */
    {
        static uint32_t tex2[4] = {0x80000000u, 0x800000FFu, 0x80000000u, 0x800000FFu};
        pk_begin();
        pk_ad(0x50, (uint64_t)0x380 << 32 | 1ull << 48);
        pk_ad(0x51, 0);
        pk_ad(0x52, 2 | 2ull << 32);
        pk_ad(0x53, 0);
        pk_image(tex2, 1);
        pk_ad(0x06, 0x380 | 1ull << 14 | 1ull << 26 | 1ull << 30 | 1ull << 34 | 1ull << 35); /* 2 x 2, CT32, TCC, DECAL */
        pk_ad(0x08, 1 | 1 << 2);                                                       /* CLAMP_1: clamp */
        pk_ad(0x14, 1 | 1 << 5);                                                       /* TEX1_1: LCM, MMAG linear */
        pk_ad(0x00, 6 | 1 << 4 | 1 << 8);
        pk_ad(0x03, 0);
        pk_ad(0x05, xyz(160, 100, 0));
        pk_ad(0x03, 32 | 32ull << 16);
        pk_ad(0x05, xyz(192, 104, 0));
        pk_send();
        expect((at(160 + 8, 101) & 0xFF) == 0 && (at(160 + 24, 101) & 0xFF) == 0xFF && (at(160 + 16, 101) & 0xFF) >= 0x7E &&
                   (at(160 + 16, 101) & 0xFF) <= 0x80 && (at(160 + 12, 101) & 0xFF) >= 0x3E && (at(160 + 12, 101) & 0xFF) <= 0x40,
               "bilinear magnification (TEX1 MMAG)");
        pk_begin();
        pk_ad(0x14, 1);                                                                /* TEX1_1: nearest */
        pk_ad(0x00, 6 | 1 << 4 | 1 << 8);
        pk_ad(0x03, 0);
        pk_ad(0x05, xyz(160, 110, 0));
        pk_ad(0x03, 32 | 32ull << 16);
        pk_ad(0x05, xyz(192, 114, 0));
        pk_ad(0x08, 0);
        pk_send();
        expect((at(160 + 15, 111) & 0xFF) == 0 && (at(160 + 16, 111) & 0xFF) == 0xFF, "nearest texel with MMAG 0");
    }
    /* STQ with a small Q (a far vertex, Q = 2^-14): S / Q = 0.3 on a 1024-wide texture is texel 307.2 */
    for (x = 300; x < 316; x++) {
        vram_rw(0x2000, 16, 0, x, 0, 1, 0x80000000u | (x - 299));
    }
    pk_begin();
    pk_ad(0x06, 0x2000 | 16ull << 14 | 10ull << 26 | 1ull << 34 | 1ull << 35); /* 1024 x 1, CT32, TCC, DECAL */
    pk_ad(0x14, 1);
    pk_ad(0x00, 6 | 1 << 4);                                                   /* sprite, textured, STQ */
    pk_ad(0x02, 0x3799999Au);                                                  /* S = 0.3 * 2^-14, T = 0 */
    pk_ad(0x01, 0x80808080u | 0x38800000ull << 32);                            /* Q = 2^-14 */
    pk_ad(0x05, xyz(200, 120, 0));
    pk_ad(0x05, xyz(201, 121, 0));
    pk_send();
    expect((at(200, 120) & 0xFF) == 307 - 299, "S / Q keeps its precision with a small Q");
    /* a Gouraud triangle covers its inside and owns only its top-left edges */
    pk_begin();
    pk_ad(0x00, 3 | 1 << 3);
    pk_ad(0x01, 0x800000FFu);
    pk_ad(0x05, xyz(150, 150, 0));
    pk_ad(0x01, 0x8000FF00u);
    pk_ad(0x05, xyz(170, 150, 0));
    pk_ad(0x01, 0x80FF0000u);
    pk_ad(0x05, xyz(150, 170, 0));
    pk_send();
    expect(at(150, 150) == 0x800000FFu && at(151, 151) != 0 && at(160, 159) != 0 && at(170, 150) == 0 && at(150, 170) == 0 && at(165, 165) == 0,
           "triangle coverage, Gouraud start colour");
    /* VIF1 DIRECT on the VIF1 channel: GIF data in the VIF stream draws, FINISH sets CSR.FINISH */
    {
        uint32_t *w = (uint32_t *)Port_Ptr(0x00100000u);
        uint64_t *g = (uint64_t *)&w[4];
        PRIV(GS_CSR) = 0;
        w[0] = 0; w[1] = 0; w[2] = 0; w[3] = 0x50000000u | 6; /* NOP x3, DIRECT 6 */
        g[0] = 5 | 1ull << 15 | 1ull << 60; g[1] = 0xE; /* PRIM, RGBAQ, XYZ2 x2, FINISH */
        g[2] = 6; g[3] = 0x00;
        g[4] = 0x80ABCDEFu; g[5] = 0x01;
        g[6] = xyz(200, 200, 0); g[7] = 0x05;
        g[8] = xyz(202, 202, 0); g[9] = 0x05;
        g[10] = 0; g[11] = 0x61; /* (a fifth A+D: FINISH, counted below) */
        g[0] = 5 | 1ull << 15 | 1ull << 60;
        *(volatile uint32_t *)Port_Ptr(0x10009010u) = 0x00100000u;
        *(volatile uint32_t *)Port_Ptr(0x10009020u) = 1 + 6;
        *(volatile uint32_t *)Port_Ptr(0x10009000u) = 0x101;
        Port_HwPoll();
        expect(at(201, 201) == 0x80ABCDEFu, "VIF1 DIRECT carries GIF data to the GS");
        expect((PRIV(GS_CSR) & 2) != 0, "FINISH sets CSR.FINISH");
    }
}

int Port_GsVifSelfTest(void); /* gs_vif1.c */

void Port_GsSelfTest(void) {
    tables_init();
    selftest_swizzle();
    selftest_draw();
    sFails += Port_GsVifSelfTest();
    memset(sVram, 0, sizeof sVram);
    Gs_Reset();
    if (sFails == 0) {
        printf("ttt: gs self-test OK\n");
    }
    exit(sFails != 0);
}
