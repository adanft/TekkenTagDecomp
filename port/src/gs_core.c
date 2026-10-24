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
 * both read circuits (DISPFB / DISPLAY each) merged as PMODE asks (EN1 / EN2, MMOD / ALP, SLBG with BGCOLOR),
 * interlaced frame mode doubled to the TV's lines. TTT_SELFTEST=gs checks the memory layout, the GIF and the rasteriser and exits.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define memcpy __builtin_memcpy /* -fno-builtin: inline the small copies of the hot paths */
#include <time.h>
#include "port_addr.h"
#include "gs.h"
#include "gs_int.h"

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

/* Where the pixel (x, y) of a buffer lives: a word of GS memory and the field of it (value = (*w >> shift) & mask). */
typedef struct VLoc {
    uint32_t *w;
    uint32_t shift, mask;
} VLoc;

/* The pixel (x, y) of the buffer at bp (blocks), width bw (64 pixels), format psm. The depth formats (0x30..0x3A)
   are the colour formats with the blocks of a page in another order (XOR 0x18). */
static inline __attribute__((always_inline)) VLoc vram_loc(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y) {
    uint32_t a, zx = 0;
    VLoc l;

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
        l.w = &sVram[(a >> 1) & 0xFFFFF];
        l.shift = (a & 1) * 16;
        l.mask = 0xFFFF;
        return l;
    case 0x13: /* 8 bits */
        a = ((bp + ((y >> 1) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + sCol8[y & 15][x & 15];
        l.w = &sVram[(a >> 2) & 0xFFFFF];
        l.shift = (a & 3) * 8;
        l.mask = 0xFF;
        return l;
    case 0x14: /* 4 bits */
        a = ((bp + ((y >> 2) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock16[(y >> 4) & 7][(x >> 5) & 3]) << 9) + sCol4[y & 15][x & 31];
        l.w = &sVram[(a >> 3) & 0xFFFFF];
        l.shift = (a & 7) * 4;
        l.mask = 0xF;
        return l;
    default: /* 32 bits, and the formats that live inside a 32-bit pixel */
        a = ((bp + (y & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + (kBlock32[(y >> 3) & 3][(x >> 3) & 7] ^ zx)) << 6) + kCol32[y & 7][x & 7];
        l.w = &sVram[a & 0xFFFFF];
        switch (psm) {
        case 0x01: l.shift = 0; l.mask = 0xFFFFFF; break;
        case 0x1B: l.shift = 24; l.mask = 0xFF; break;
        case 0x24: l.shift = 24; l.mask = 0xF; break;
        case 0x2C: l.shift = 28; l.mask = 0xF; break;
        default: l.shift = 0; l.mask = 0xFFFFFFFFu; break;
        }
        return l;
    }
}

static inline uint32_t vloc_read(VLoc l) {
    return (*l.w >> l.shift) & l.mask;
}

static inline void vloc_write(VLoc l, uint32_t v) {
    *l.w = (*l.w & ~(l.mask << l.shift)) | (v & l.mask) << l.shift;
}

/* Reads (write = 0) or writes the pixel (x, y) of the buffer at bp, width bw, format psm. */
static uint32_t vram_rw(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, int write, uint32_t v) {
    VLoc l = vram_loc(bp, bw, psm, x, y);

    if (write) {
        vloc_write(l, v);
        return 0;
    }
    return vloc_read(l);
}

/* ------------------------------------------------------------------------------------------------ state */


/* The registers (the front end: GIF, register writes, the vertex queue; the calling thread). */
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
    uint32_t cbp[2], clutGen;
} gs;

/* TTT_GS_TRACE=1: per VBlank, the pixels written per frame buffer (FBP) and the last TEX0 / FRAME / TEST used. */
static int sTrace = -1;
static struct { uint32_t px, prims; uint64_t tex0, frame, test, alpha; } sTr[512];
static uint32_t sUploadGen; /* bumped by every write into GS memory through a transfer (CLD 4 / 5, below) */
static uint32_t sClutVer = 1; /* bumped with every CLUT load: the primitives after it carry the new number */
static struct { uint32_t tags, prims, pixels, imageQw, localCopies, readbacks, regs; } sStat;

/* GIF tag state of each path (1 = VU1, 2 = VIF1, 3 = GIF channel) */
static struct { uint64_t lo, hi; uint32_t loops, reg; } sPath[4];
static uint32_t sTracePrimVb, sCurVb;
static int sTraceX = -1, sTraceY = -1;
static uint32_t sPathTags[4], sTraceBad, sBlockQwc;
static const uint8_t *sBlock;

static void gs_sync(void);
static void raster_reset(void);

uint32_t Gs_VramRead(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y) {
    Gs_AsyncDrain();
    tables_init();
    gs_sync();
    return vram_rw(bp, bw, psm, x, y, 0, 0);
}

void Gs_VramWrite(uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y, uint32_t v) {
    Gs_AsyncDrain();
    tables_init();
    gs_sync();
    vram_rw(bp, bw, psm, x, y, 1, v);
}

void Gs_Reset(void) {
    Gs_AsyncDrain(); /* sceGsResetGraph: the GS thread has finished what was sent before (gs_async.c) */
    tables_init();
    gs_sync();
    memset(&gs, 0, sizeof(gs));
    raster_reset();
    memset(sPath, 0, sizeof(sPath));
    sClutVer++;
    gs.prmodecont = 1; /* the attributes come from PRIM */
    gs.colclamp = 1;
    gs.q = 0x3F800000u;
}

/* ------------------------------------------------------------------------------------------------ textures */

/* A stored colour of format psm as R | G << 8 | B << 16 | A << 24 (TEXA gives the alpha of 24 and 16 bits). */
static uint32_t expand(uint32_t c, uint32_t psm, uint64_t texa) {
    uint32_t ta0 = texa & 0xFF, ta1 = (texa >> 32) & 0xFF, aem = (texa >> 15) & 1;

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

/* Whether TEX0 / TEX2 loads the CLUT buffer (only for the indexed formats, as the GS). CLD: 0 keep, 1 load, 2 load and
   CBP0 = CBP, 3 load and CBP1 = CBP, 4 load if CBP != CBP0, then CBP0 = CBP, 5 the same with CBP1.
   [I] For 4 / 5 the buffer is also reloaded when GS memory was written since the last load (as PCSX2 does): a
   palette uploaded again to the same address is seen. The load itself (clut_fill) reads GS memory: it runs in the
   back end, in order with the primitives. */
static int clut_wanted(uint64_t t0) {
    uint32_t psm = (t0 >> 20) & 0x3F, cbp = (t0 >> 37) & 0x3FFF, cld = (t0 >> 61) & 7;
    int load;

    if (psm_bits(psm) > 8) {
        return 0;
    }
    switch (cld) {
    case 1: load = 1; break;
    case 2: load = 1; gs.cbp[0] = cbp; break;
    case 3: load = 1; gs.cbp[1] = cbp; break;
    case 4: load = gs.cbp[0] != cbp || gs.clutGen != sUploadGen; gs.cbp[0] = cbp; break;
    case 5: load = gs.cbp[1] != cbp || gs.clutGen != sUploadGen; gs.cbp[1] = cbp; break;
    default: load = 0; break;
    }
    if (load) {
        gs.clutGen = sUploadGen;
        sClutVer++;
    }
    return load;
}

static void clut_fill(uint16_t *clut, uint64_t t0, uint64_t texclut) {
    uint32_t psm = (t0 >> 20) & 0x3F, cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15, csm = (t0 >> 55) & 1;
    uint32_t csa = (t0 >> 56) & 31, bits = (uint32_t)psm_bits(psm), n, i;

    n = bits == 8 ? 256 : 16;
    for (i = 0; i < n; i++) {
        uint32_t x, y, c, bw = 1, e;
        if (csm) { /* CSM2: one row of TEXCLUT, 16-bit entries only */
            bw = (uint32_t)(texclut & 0x3F);
            x = (uint32_t)((texclut >> 6) & 0x3F) * 16 + i;
            y = (uint32_t)((texclut >> 12) & 0x3FF);
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
            clut[e & 255] = (uint16_t)c;
            clut[(e & 255) + 256] = (uint16_t)(c >> 16);
        } else {
            clut[e & 511] = (uint16_t)c;
        }
    }
}

static uint32_t clut_read(const uint16_t *clut, uint64_t t0, uint32_t index, int bits, uint64_t texa) {
    uint32_t cpsm = (t0 >> 51) & 15, csa = (t0 >> 56) & 31, e = bits == 8 ? index : csa * 16 + index;
    if (cpsm == 0 || cpsm == 1) {
        e &= 255;
        return expand(clut[e] | (uint32_t)clut[e + 256] << 16, 0, texa);
    }
    return expand(clut[e & 511], cpsm, texa);
}

/* ------------------------------------------------------------------------------------------------ primitive state */


static void draw_setup(Draw *d, int ctx) {
    uint64_t test = gs.test[ctx], al = gs.alpha[ctx], zb = gs.zbuf[ctx], sc = gs.scissor[ctx], fr = gs.frame[ctx];
    uint64_t t0 = gs.tex0[ctx], t1 = gs.tex1[ctx], cl = gs.clamp[ctx];
    uint32_t mmin;

    d->ctx = ctx;
    d->prim = gs.prim;
    d->tex0 = t0;
    d->texa = gs.texa;
    d->fogcol = gs.fogcol;
    d->clutVer = sClutVer;
    d->test = test;
    d->alpha = al;
    d->zbuf = zb;
    d->frame = fr;
    d->scx0 = (int)(sc & 0x7FF);
    d->scx1 = (int)((sc >> 16) & 0x7FF);
    d->scy0 = (int)((sc >> 32) & 0x7FF);
    d->scy1 = (int)((sc >> 48) & 0x7FF);
    d->fbp = (uint32_t)(fr & 0x1FF) << 5;
    d->fbw = (fr >> 16) & 0x3F;
    d->fpsm = (fr >> 24) & 0x3F;
    d->fbits = (uint32_t)psm_bits(d->fpsm);
    d->fbmsk = (uint32_t)(fr >> 32);
    d->zbp = (uint32_t)(zb & 0x1FF) << 5;
    d->zpsm = 0x30 | (uint32_t)((zb >> 24) & 15);
    d->zmax = psm_bits(d->zpsm) == 32 ? 0xFFFFFFFFu : psm_bits(d->zpsm) == 24 ? 0xFFFFFFu : 0xFFFFu;
    d->zwrite = !((zb >> 32) & 1);
    d->ztest = (int)((test >> 16) & 1);
    d->ztst = (test >> 17) & 3;
    d->atest = (int)(test & 1);
    d->atst = (test >> 1) & 7;
    d->aref = (test >> 4) & 0xFF;
    d->afail = (test >> 12) & 3;
    d->date = (int)((test >> 14) & 1);
    d->datm = (test >> 15) & 1;
    d->blend = (int)((gs.prim >> 6) & 1);
    d->pabe = (int)(gs.pabe & 1);
    d->colclamp = (int)(gs.colclamp & 1);
    d->fba = (int)(gs.fba[ctx] & 1);
    d->asel = al & 3;
    d->bsel = (al >> 2) & 3;
    d->csel = (al >> 4) & 3;
    d->dsel = (al >> 6) & 3;
    d->afix = (al >> 32) & 0xFF;
    d->tex = (int)((gs.prim >> 4) & 1);
    d->fst = (int)((gs.prim >> 8) & 1);
    d->fog = (int)((gs.prim >> 5) & 1);
    d->gouraud = (int)((gs.prim >> 3) & 1);
    d->lw = (int)((t0 >> 26) & 15);
    d->lh = (int)((t0 >> 30) & 15);
    if (!d->tex) {
        return;
    }
    d->tbp = t0 & 0x3FFF;
    d->tbw = (t0 >> 14) & 0x3F;
    d->tpsm = (t0 >> 20) & 0x3F;
    d->tbits = (uint32_t)psm_bits(d->tpsm);
    d->tfx = (t0 >> 35) & 3;
    d->tcc = (t0 >> 34) & 1;
    d->tw = 1 << ((t0 >> 26) & 15);
    d->th = 1 << ((t0 >> 30) & 15);
    if (d->tw > 1024) { d->tw = 1024; }
    if (d->th > 1024) { d->th = 1024; }
    d->wms = cl & 3;
    d->wmt = (cl >> 2) & 3;
    d->minu = (int)((cl >> 4) & 0x3FF);
    d->maxu = (int)((cl >> 14) & 0x3FF);
    d->minv = (int)((cl >> 24) & 0x3FF);
    d->maxv = (int)((cl >> 34) & 0x3FF);
    d->ta0 = gs.texa & 0xFF;
    d->ta1 = (gs.texa >> 32) & 0xFF;
    d->aem = (gs.texa >> 15) & 1;
    d->lodk = (int32_t)((t1 >> 32) & 0xFFF);
    if (d->lodk & 0x800) {
        d->lodk -= 0x1000;
    }
    d->lcm = (int)(t1 & 1);
    d->lodl = (int)((t1 >> 19) & 3);
    d->mmagLinear = (int)((t1 >> 5) & 1);
    mmin = (uint32_t)(t1 >> 6) & 7;
    d->mminLinear = mmin == 1 || mmin == 4 || mmin == 5; /* LINEAR, LINEAR_MIPMAP_NEAREST / _LINEAR: linear inside a level */
    d->filter = d->mmagLinear == d->mminLinear ? d->mmagLinear : 2;
}

/* ------------------------------------------------------------------------------------------------ back end */

/* A raster worker. With GS threads (TTT_GS_THREADS, below) every worker runs every primitive and draws the rows of its
   own 8-row bands ((y >> 3) % count == id); count 1 draws all of them. The bands keep every word of GS memory that a
   buffer's pixels share (8- and 4-bit formats pack pixels of a column's four rows; columns start every 4 rows) inside
   one band. 8 rows rather than 16 (T30 batch 2) share an object's rows out more evenly, and a 448-row frame has 56
   bands: 7 for each of 8 workers (16-row bands gave four of them 4 bands and the other four 3). */
typedef struct Raster {
    int id, count;
    /* the CLUT buffer (each worker loads its own, in order with its primitives): 512 halfwords; a 32-bit entry e keeps
       its low half at e and its high half at e + 256 */
    uint16_t clut[512];
    int64_t pixQ;  /* Q of the pixel being drawn as pixQ / 2^pixQSh (the level of detail of LCM 0 depends on it) */
    int pixQSh;
    uint32_t pixels;
    const uint32_t *pal;
    uint32_t palBuf[256];  /* the CLUT expanded for an indexed texture, kept while its inputs stay the same */
    struct { uint32_t ver, cpsm, csa, bits; uint64_t texa; int valid; } palKey;
} Raster;

static inline int row_mine(const Raster *r, int y) {
    return r->count == 1 || ((y >> 3) % r->count) == r->id;
}

/* Whether any of the rows y0..y1 - 1 is this worker's. */
static int rows_any_mine(const Raster *r, int y0, int y1) {
    int b;
    if (r->count == 1) {
        return y0 < y1;
    }
    for (b = y0 >> 3; b <= (y1 - 1) >> 3 && b < (y0 >> 3) + r->count; b++) {
        if (b % r->count == r->id) {
            return y0 < y1;
        }
    }
    return 0;
}

static const uint32_t *palette(Raster *r, const Draw *d) {
    uint64_t t0 = d->tex0;
    uint32_t cpsm = (t0 >> 51) & 15, csa = (t0 >> 56) & 31, i, bits = d->tbits;

    if (!r->palKey.valid || r->palKey.ver != d->clutVer || r->palKey.cpsm != cpsm || r->palKey.csa != csa || r->palKey.bits != bits ||
        r->palKey.texa != d->texa) {
        for (i = 0; i < (bits == 8 ? 256u : 16u); i++) {
            r->palBuf[i] = clut_read(r->clut, t0, i, (int)bits, d->texa);
        }
        r->palKey.valid = 1;
        r->palKey.ver = d->clutVer;
        r->palKey.cpsm = cpsm;
        r->palKey.csa = csa;
        r->palKey.bits = bits;
        r->palKey.texa = d->texa;
    }
    return r->palBuf;
}

/* ------------------------------------------------------------------------------------------------ texturing */

/* A stored colour of the texture's format (32, 24 or 16 bits) as R | G << 8 | B << 16 | A << 24: expand() on the
   decoded TEXA. */
static inline uint32_t texel_expand(const Draw *d, uint32_t c) {
    switch (d->tbits) {
    case 32:
        return c;
    case 24:
        c &= 0xFFFFFF;
        return c | ((d->aem && c == 0) ? 0 : d->ta0) << 24;
    default: {
        uint32_t r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        uint32_t a = (c & 0x8000) ? d->ta1 : (d->aem && (c & 0x7FFF) == 0 ? 0 : d->ta0);
        return r | g << 8 | b << 16 | a << 24;
    }
    }
}

/* The texel at (u, v) (whole texels) of the primitive's texture, wrapped or clamped by CLAMP. */
static inline uint32_t texel(const Raster *r, const Draw *d, int u, int v) {
    uint32_t c;

    if (d->wms == 0) { u &= d->tw - 1; } else if (d->wms == 1) { u = u < 0 ? 0 : u >= d->tw ? d->tw - 1 : u; }
    else if (d->wms == 2) { u = u < d->minu ? d->minu : u > d->maxu ? d->maxu : u; } else { u = (u & d->minu) | d->maxu; }
    if (d->wmt == 0) { v &= d->th - 1; } else if (d->wmt == 1) { v = v < 0 ? 0 : v >= d->th ? d->th - 1 : v; }
    else if (d->wmt == 2) { v = v < d->minv ? d->minv : v > d->maxv ? d->maxv : v; } else { v = (v & d->minv) | d->maxv; }
    c = vloc_read(vram_loc(d->tbp, d->tbw, d->tpsm, (uint32_t)u, (uint32_t)v));
    return d->tbits > 8 ? texel_expand(d, c) : r->pal[c];
}

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
   0 is read: no mip-maps (MXL), which the game does not use. The level of detail is only computed when MMAG and MMIN
   filter differently. */
static inline uint32_t sample(const Raster *r, const Draw *d, int32_t u16, int32_t v16) {
    uint32_t c00, c10, c01, c11, out = 0;
    int32_t fu, fv, u0, v0, i;
    int linear = d->filter;

    if (linear == 2) {
        int32_t lod;
        if (d->lcm) {
            lod = d->lodk;
        } else {
            int64_t q = r->pixQ < 0 ? -r->pixQ : r->pixQ;
            lod = ((r->pixQSh * 16 - log2_16(q != 0 ? q : 1)) << d->lodl) + d->lodk;
        }
        linear = lod <= 0 ? d->mmagLinear : d->mminLinear;
    }
    if (!linear) {
        return texel(r, d, u16 >> 4, v16 >> 4);
    }
    u16 -= 8;
    v16 -= 8;
    u0 = u16 >> 4;
    v0 = v16 >> 4;
    fu = u16 & 15;
    fv = v16 & 15;
    c00 = texel(r, d, u0, v0);
    if (fu == 0 && fv == 0) {
        return c00;
    }
    c10 = texel(r, d, u0 + 1, v0);
    c01 = texel(r, d, u0, v0 + 1);
    c11 = texel(r, d, u0 + 1, v0 + 1);
    {
        uint32_t w00 = (uint32_t)(16 - fu) * (uint32_t)(16 - fv), w10 = (uint32_t)fu * (uint32_t)(16 - fv);
        uint32_t w01 = (uint32_t)(16 - fu) * (uint32_t)fv, w11 = (uint32_t)fu * (uint32_t)fv;
        for (i = 0; i < 32; i += 8) {
            uint32_t a = (c00 >> i) & 0xFF, b = (c10 >> i) & 0xFF, c = (c01 >> i) & 0xFF, e = (c11 >> i) & 0xFF;
            out |= ((a * w00 + b * w10 + c * w01 + e * w11) >> 8) << i;
        }
    }
    return out;
}

/* Texture function (TEX0.TFX, TCC) on a vertex colour; u16 / v16 are texel coordinates in 1/16 texel. */
static inline uint32_t shade(const Raster *r, const Draw *d, uint32_t vc, int32_t u16, int32_t v16) {
    uint32_t tc, out = 0, tfx = d->tfx, tcc = d->tcc, av = vc >> 24, at, a;
    int i;

    tc = sample(r, d, u16, v16);
    at = tc >> 24;
    for (i = 0; i < 3; i++) {
        uint32_t cv = (vc >> (8 * i)) & 0xFF, ct = (tc >> (8 * i)) & 0xFF, c;
        c = tfx == 0 ? (ct * cv) >> 7 : tfx == 1 ? ct : ((ct * cv) >> 7) + av;
        out |= (c > 255 ? 255 : c) << (8 * i);
    }
    a = !tcc ? av : tfx == 0 ? (at * av) >> 7 : tfx == 2 ? at + av : at;
    if (tfx == 3 && !tcc) {
        a = av;
    }
    return out | (a > 255 ? 255 : a) << 24;
}

static uint32_t fogged(uint32_t c, uint32_t f, uint64_t fogcol) {
    uint32_t out = c & 0xFF000000u;
    int i;
    for (i = 0; i < 3; i++) {
        uint32_t cc = (c >> (8 * i)) & 0xFF, fc = (uint32_t)(fogcol >> (8 * i)) & 0xFF;
        out |= ((cc * f + fc * (255 - f)) >> 8) << (8 * i);
    }
    return out;
}

/* The colour of a pixel from its interpolated colour, texture coordinates and fog. */
static inline uint32_t colour(const Raster *r, const Draw *d, uint32_t c, int32_t u16, int32_t v16, uint32_t f) {
    if (d->tex) {
        c = shade(r, d, c, u16, v16);
    }
    if (d->fog) {
        c = fogged(c, f, d->fogcol);
    }
    return c;
}

/* ------------------------------------------------------------------------------------------------ pixels */

/* TTT_GS_TRACE_PRIMS with TTT_GS_TRACE_PIXEL: why a pixel at the traced position was not written. */
#define TRACE_REJECT(why)                                                                                              \
    do {                                                                                                               \
        if (sTracePrimVb != 0 && sCurVb == sTracePrimVb && x == sTraceX && y == sTraceY) {                             \
            printf("gs-reject %d,%d %s src %08X z %08X prim %03llX test %llX zbuf %llX\n", x, y, why, src, z,            \
                   (unsigned long long)d->prim, (unsigned long long)d->test, (unsigned long long)d->zbuf);               \
        }                                                                                                              \
    } while (0)
static void pixel(Raster *r, const Draw *d, int x, int y, uint32_t z, uint32_t src) {
    uint32_t dv, sa = src >> 24, out[3], fbits = d->fbits;
    int i, write_rgb = 1, write_a = 1, write_z = d->zwrite;
    VLoc fl, zl;

    if (x < d->scx0 || x > d->scx1 || y < d->scy0 || y > d->scy1 || x < 0 || y < 0) {
        TRACE_REJECT("scissor");
        return;
    }
    if (d->atest) { /* alpha test */
        uint32_t ref = d->aref;
        int pass;
        switch (d->atst) {
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
            switch (d->afail) {
            case 0: TRACE_REJECT("alpha test"); return; /* KEEP */
            case 1: write_z = 0; break;              /* FB_ONLY */
            case 2: write_rgb = write_a = 0; break;  /* ZB_ONLY */
            default: write_z = 0; write_a = 0; break; /* RGB_ONLY */
            }
        }
    }
    fl = vram_loc(d->fbp, d->fbw, d->fpsm, (uint32_t)x, (uint32_t)y);
    if (d->date) { /* destination alpha test: the frame's alpha bit (7 of 32-bit, 15 of 16-bit) */
        uint32_t dw = vloc_read(fl), bit;
        if (fbits == 32 || fbits == 16) {
            bit = fbits == 32 ? dw >> 31 : (dw >> 15) & 1;
            if (bit != d->datm) {
                TRACE_REJECT("destination alpha test");
                return;
            }
        }
    }
    /* the depth buffer lives in GS memory: ZBUF.ZBP, the frame's width, format 0x30 | ZBUF.PSM */
    if (z > d->zmax) {
        z = d->zmax;
    }
    if (d->ztest || write_z) {
        zl = vram_loc(d->zbp, d->fbw, d->zpsm, (uint32_t)x, (uint32_t)y);
    }
    if (d->ztest) { /* depth test */
        uint32_t zd = vloc_read(zl);
        switch (d->ztst) {
        case 0: TRACE_REJECT("depth test NEVER"); return;
        case 1: break;
        case 2: if (z < zd) { TRACE_REJECT("depth test GEQUAL"); return; } break;
        default: if (z <= zd) { TRACE_REJECT("depth test GREATER"); return; } break;
        }
    }
    if (write_rgb) {
        uint32_t c, m = d->fbmsk, old;
        dv = vloc_read(fl);
        old = dv;
        if (fbits == 16) {
            dv = (dv & 31) << 3 | ((dv >> 5) & 31) << 11 | ((dv >> 10) & 31) << 19 | ((dv & 0x8000) ? 0x80000000u : 0);
        } else if (fbits == 24) {
            dv |= 0x80000000u;
        }
        out[0] = src & 0xFF;
        out[1] = (src >> 8) & 0xFF;
        out[2] = (src >> 16) & 0xFF;
        if (d->blend && (!d->pabe || (sa & 0x80))) { /* blending: ((A - B) * C >> 7) + D */
            uint32_t s[3] = {out[0], out[1], out[2]};
            int cc = d->csel == 0 ? (int)sa : d->csel == 1 ? (int)(dv >> 24) : (int)d->afix;
            for (i = 0; i < 3; i++) {
                int dc = (int)((dv >> (8 * i)) & 0xFF);
                int a = d->asel == 0 ? (int)s[i] : d->asel == 1 ? dc : 0;
                int b = d->bsel == 0 ? (int)s[i] : d->bsel == 1 ? dc : 0;
                int dd = d->dsel == 0 ? (int)s[i] : d->dsel == 1 ? dc : 0;
                int v = (((a - b) * cc) >> 7) + dd;
                out[i] = d->colclamp ? (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v) : (uint32_t)v & 0xFF;
            }
        }
        if (d->fba) {
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
        vloc_write(fl, c);
        r->pixels++;
        if (sTracePrimVb != 0 && sCurVb == sTracePrimVb && x == sTraceX && y == sTraceY) {
            printf("gs-pixel %d,%d fbp %X src %08X old %08X new %08X prim %03llX tex0 %016llX alpha %llX test %llX\n", x, y, d->fbp, src, old, c,
                   (unsigned long long)d->prim, (unsigned long long)d->tex0, (unsigned long long)d->alpha, (unsigned long long)d->test);
        }
        if (sTrace > 0) {
            sTr[d->fbp >> 5].px++;
            sTr[d->fbp >> 5].tex0 = d->tex ? d->tex0 : 0;
            sTr[d->fbp >> 5].frame = d->frame;
            sTr[d->fbp >> 5].test = d->test;
            sTr[d->fbp >> 5].alpha = d->alpha;
        }
    }
    if (write_z) {
        vloc_write(zl, z);
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
static inline int32_t stq_texel(int64_t s20, int64_t q20, int log2size) {
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

static int ceil16(int32_t v) { return (int)((v + 15) >> 4); }

/* floor(n / d) for d > 0. */
static int64_t floordiv(int64_t n, int64_t d) {
    int64_t q = n / d;
    return (n % d != 0 && n < 0) ? q - 1 : q;
}

/* An exact running quotient: q = floor(N / d), r = N - q * d, while N grows by a fixed step (d > 0). */
typedef struct {
    int64_t q, r, dq, dr, d;
} Quot;

static void quot_start(Quot *s, int64_t n, int64_t step, int64_t d) {
    s->d = d;
    s->q = floordiv(n, d);
    s->r = n - s->q * d;
    s->dq = floordiv(step, d);
    s->dr = step - s->dq * d;
}

/* Whether the step carried a unit into q. */
static inline int quot_step(Quot *s) {
    s->q += s->dq;
    s->r += s->dr;
    if (s->r >= s->d) {
        s->r -= s->d;
        s->q++;
        return 1;
    }
    return 0;
}

static void draw_sprite(Raster *r, const Draw *d, const Vtx *a, const Vtx *b) {
    int32_t X0 = a->x, X1 = b->x, Y0 = a->y, Y1 = b->y;
    int32_t U0, U1, V0, V1;
    int xa, xb, ya, yb, x, y;
    uint32_t col = b->rgba;

    if (d->fst) {
        U0 = a->u; U1 = b->u; V0 = a->v; V1 = b->v;
        r->pixQ = fix(b->q, 20);
        r->pixQSh = 20;
    } else {
        uint32_t f[5];
        int sh;
        int64_t q;
        f[0] = a->s; f[1] = b->s; f[2] = a->t; f[3] = b->t; f[4] = b->q;
        sh = stq_shift(f, 5);
        q = fix(b->q, sh);
        r->pixQ = q;
        r->pixQSh = sh;
        U0 = stq_texel(fix(a->s, sh), q, d->lw); U1 = stq_texel(fix(b->s, sh), q, d->lw);
        V0 = stq_texel(fix(a->t, sh), q, d->lh); V1 = stq_texel(fix(b->t, sh), q, d->lh);
    }
    if (X0 > X1) { int32_t t = X0; X0 = X1; X1 = t; t = U0; U0 = U1; U1 = t; }
    if (Y0 > Y1) { int32_t t = Y0; Y0 = Y1; Y1 = t; t = V0; V0 = V1; V1 = t; }
    xa = ceil16(X0); xb = ceil16(X1); ya = ceil16(Y0); yb = ceil16(Y1);
    if (xa < d->scx0) { xa = d->scx0; }
    if (xb > d->scx1 + 1) { xb = d->scx1 + 1; }
    if (ya < d->scy0) { ya = d->scy0; }
    if (yb > d->scy1 + 1) { yb = d->scy1 + 1; }
    for (y = ya; y < yb; y++) {
        int32_t v16, du = U1 - U0, su = du < 0 ? -1 : 1;
        Quot qu;

        if (!row_mine(r, y)) {
            continue;
        }
        v16 = Y1 != Y0 ? V0 + (int32_t)((int64_t)(V1 - V0) * (y * 16 - Y0) / (Y1 - Y0)) : V0;
        /* u16 = U0 + (U1 - U0) * (x * 16 - X0) / (X1 - X0), cut toward zero: x * 16 >= X0 here, so the quotient is
           |U1 - U0| * (x * 16 - X0) / (X1 - X0) with the sign of U1 - U0, kept as a running quotient */
        if (xa < xb && d->tex && X1 != X0) {
            int64_t m = (int64_t)du * su;
            quot_start(&qu, m * ((int64_t)xa * 16 - X0), m * 16, (int64_t)X1 - X0);
        }
        for (x = xa; x < xb; x++) {
            int32_t u16 = U0;
            if (d->tex && X1 != X0) {
                u16 = U0 + su * (int32_t)qu.q;
                quot_step(&qu);
            }
            pixel(r, d, x, y, b->z, colour(r, d, col, u16, v16, b->f));
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

/* An attribute interpolated over a triangle: a0 + ((a1 - a0) * lb + (a2 - a0) * lc) >> 24, with p the sum of the
   products kept modulo 2^64 (as the products themselves wrap). */
typedef struct {
    int64_t a0;
    uint64_t d1, d2, p, base;
} Attr;

static void attr_add(Attr *at, int *n, int64_t a0, int64_t a1, int64_t a2) {
    at[*n].a0 = a0;
    at[*n].d1 = (uint64_t)(a1 - a0);
    at[*n].d2 = (uint64_t)(a2 - a0);
    (*n)++;
}

#define AT(i) (at[i].a0 + ((int64_t)at[i].p >> 24))

/* Triangles by edge functions with the top-left rule. Per row the pixels inside all three edges form one span,
   found exactly from the edge values at the row's first pixel and their step per pixel; along the span the
   barycentric fractions lb = (w1 << 24) / area, lc = (w2 << 24) / area are running exact quotients and each attribute's
   (a1 - a0) * lb + (a2 - a0) * lc a running sum: the same pixels and values as evaluating every pixel of the bounding
   box. Attribute slots: z, then R G B A and fog (Gouraud), then U V (UV) or S T Q (STQ). */
static void draw_triangle(Raster *r, const Draw *d, const Vtx *a, const Vtx *b, const Vtx *c) {
    int64_t area = edge(a, b, c->x, c->y);
    int sign = area < 0 ? -1 : 1, gouraud = d->gouraud, tex = d->tex, fst = d->fst;
    int bias[3], x, y, x0, x1, y0, y1, k;
    int32_t minx, maxx, miny, maxy;
    int64_t S[3] = {0, 0, 0}, T[3] = {0, 0, 0}, Q[3] = {0, 0, 0}, step[3];
    int qsh = 20;
    const Vtx *v[3];
    Attr at[9];
    int na, AZ = 0, AR = 1, AF = 5, AU, AV, AQ;

    if (area == 0) {
        return;
    }
    miny = a->y < b->y ? a->y : b->y;
    miny = c->y < miny ? c->y : miny;
    maxy = a->y > b->y ? a->y : b->y;
    maxy = c->y > maxy ? c->y : maxy;
    y0 = ceil16(miny); y1 = (int)(maxy >> 4) + 1;
    if (y0 < d->scy0) { y0 = d->scy0; }
    if (y1 > d->scy1 + 1) { y1 = d->scy1 + 1; }
    if (!rows_any_mine(r, y0, y1)) {
        return; /* only the rows of other workers (the per-pixel Q of LCM 0 is set again before it is read) */
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
        r->pixQ = fix(c->q, 20);
        r->pixQSh = 20;
    }
    bias[0] = top_left(b, c, sign) ? 0 : 1;
    bias[1] = top_left(c, a, sign) ? 0 : 1;
    bias[2] = top_left(a, b, sign) ? 0 : 1;
    /* edge k's value grows by step[k] per pixel to the right: edge(p, q, x, y) changes by -(q.y - p.y) * 16 */
    step[0] = -(int64_t)(c->y - b->y) * 16 * sign;
    step[1] = -(int64_t)(a->y - c->y) * 16 * sign;
    step[2] = -(int64_t)(b->y - a->y) * 16 * sign;
    minx = a->x; maxx = a->x; miny = a->y; maxy = a->y;
    for (x = 1; x < 3; x++) {
        if (v[x]->x < minx) { minx = v[x]->x; }
        if (v[x]->x > maxx) { maxx = v[x]->x; }
        if (v[x]->y < miny) { miny = v[x]->y; }
        if (v[x]->y > maxy) { maxy = v[x]->y; }
    }
    x0 = ceil16(minx); x1 = (int)(maxx >> 4) + 1; y0 = ceil16(miny); y1 = (int)(maxy >> 4) + 1;
    if (x0 < d->scx0) { x0 = d->scx0; }
    if (x1 > d->scx1 + 1) { x1 = d->scx1 + 1; }
    if (y0 < d->scy0) { y0 = d->scy0; }
    if (y1 > d->scy1 + 1) { y1 = d->scy1 + 1; }
    AU = gouraud ? 6 : 1;
    AV = AU + 1;
    AQ = AU + 2;
    na = 0;
    attr_add(at, &na, (int64_t)a->z, (int64_t)b->z, (int64_t)c->z);
    if (gouraud) {
        for (k = 0; k < 32; k += 8) {
            attr_add(at, &na, (int64_t)((a->rgba >> k) & 0xFF), (int64_t)((b->rgba >> k) & 0xFF), (int64_t)((c->rgba >> k) & 0xFF));
        }
        attr_add(at, &na, (int64_t)a->f, (int64_t)b->f, (int64_t)c->f);
    }
    if (tex && fst) {
        attr_add(at, &na, (int64_t)a->u, (int64_t)b->u, (int64_t)c->u);
        attr_add(at, &na, (int64_t)a->v, (int64_t)b->v, (int64_t)c->v);
    } else if (tex) {
        attr_add(at, &na, S[0], S[1], S[2]);
        attr_add(at, &na, T[0], T[1], T[2]);
        attr_add(at, &na, Q[0], Q[1], Q[2]);
    }
    for (y = y0; y < y1 && x0 < x1; y++) {
        int32_t px = x0 * 16, py = y * 16;
        int64_t w[3], lo = 0, hi = x1 - x0 - 1;
        Quot qb, qc;

        if (!row_mine(r, y)) {
            continue;
        }
        w[0] = edge(b, c, px, py) * sign;
        w[1] = edge(c, a, px, py) * sign;
        w[2] = edge(a, b, px, py) * sign;
        /* the pixels n = 0.. of the row with w[k] + step[k] * n >= bias[k] for every edge */
        for (k = 0; k < 3 && lo <= hi; k++) {
            if (step[k] == 0) {
                if (w[k] < bias[k]) {
                    hi = -1;
                }
            } else if (step[k] > 0) {
                if (w[k] < bias[k]) {
                    int64_t n = (bias[k] - w[k] + step[k] - 1) / step[k];
                    if (n > lo) { lo = n; }
                }
            } else {
                if (w[k] < bias[k]) {
                    hi = -1;
                } else {
                    int64_t n = (w[k] - bias[k]) / -step[k];
                    if (n < hi) { hi = n; }
                }
            }
        }
        if (lo > hi) {
            continue;
        }
        quot_start(&qb, (w[1] + step[1] * lo) * ((int64_t)1 << 24), step[1] * ((int64_t)1 << 24), area);
        quot_start(&qc, (w[2] + step[2] * lo) * ((int64_t)1 << 24), step[2] * ((int64_t)1 << 24), area);
        for (k = 0; k < na; k++) {
            at[k].p = at[k].d1 * (uint64_t)qb.q + at[k].d2 * (uint64_t)qc.q;
            at[k].base = at[k].d1 * (uint64_t)qb.dq + at[k].d2 * (uint64_t)qc.dq;
        }
        for (x = x0 + (int)lo; x <= x0 + (int)hi; x++) {
            int32_t u16 = 0, v16 = 0;
            uint32_t col, z, f;
            int cb, cc;

            z = (uint32_t)AT(AZ);
            if (gouraud) {
                col = 0;
                for (k = 0; k < 4; k++) {
                    int64_t ch = AT(AR + k);
                    col |= (uint32_t)(ch < 0 ? 0 : ch > 255 ? 255 : ch) << (8 * k);
                }
                f = (uint32_t)AT(AF);
            } else {
                col = c->rgba;
                f = c->f;
            }
            if (tex) {
                if (fst) {
                    u16 = (int32_t)AT(AU);
                    v16 = (int32_t)AT(AV);
                } else {
                    int64_t q = AT(AQ);
                    r->pixQ = q;
                    r->pixQSh = qsh;
                    u16 = stq_texel(AT(AU), q, d->lw);
                    v16 = stq_texel(AT(AV), q, d->lh);
                }
            }
            pixel(r, d, x, y, z, colour(r, d, col, u16, v16, f > 255 ? 255 : f));
            cb = quot_step(&qb);
            cc = quot_step(&qc);
            for (k = 0; k < na; k++) {
                at[k].p += at[k].base + (cb ? at[k].d1 : 0) + (cc ? at[k].d2 : 0);
            }
        }
    }
}

static void draw_line(Raster *r, const Draw *d, const Vtx *a, const Vtx *b) {
    int32_t dx = b->x - a->x, dy = b->y - a->y;
    int32_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int n = (int)((adx > ady ? adx : ady) >> 4), i;

    for (i = 0; i <= n; i++) {
        int32_t x = a->x + (n ? (int32_t)((int64_t)dx * i / n) : 0), y = a->y + (n ? (int32_t)((int64_t)dy * i / n) : 0);
        if (row_mine(r, (int)(y >> 4))) {
            pixel(r, d, (int)(x >> 4), (int)(y >> 4), b->z, d->fog ? fogged(b->rgba, b->f, d->fogcol) : b->rgba);
        }
    }
}

/* ------------------------------------------------------------------------------------------------ commands */

/* The front end turns primitives and the operations on GS memory into commands; the back end carries them out in
   order. Without GS threads a command runs at once; with them it goes into a ring that the raster workers read. */



static void exec_upload(const Cmd *c);
static void exec_copy(const Cmd *c);
static void exec_show(const Cmd *c);
static void exec_window(Raster *r, const Cmd *c);

static void exec_cmd(Raster *r, const Cmd *c) {
    const Draw *d = &c->u.prim.d;

    switch (c->type) {
    case CMD_POINT:
    case CMD_LINE:
    case CMD_TRIANGLE:
    case CMD_SPRITE:
        r->pal = d->tex && d->tbits <= 8 ? palette(r, d) : NULL;
        if (c->type == CMD_TRIANGLE) {
            draw_triangle(r, d, &c->u.prim.v[0], &c->u.prim.v[1], &c->u.prim.v[2]);
        } else if (c->type == CMD_SPRITE) {
            draw_sprite(r, d, &c->u.prim.v[0], &c->u.prim.v[1]);
        } else if (c->type == CMD_LINE) {
            draw_line(r, d, &c->u.prim.v[0], &c->u.prim.v[1]);
        } else {
            const Vtx *v = &c->u.prim.v[0];
            if (row_mine(r, (int)(v->y >> 4))) {
                pixel(r, d, (int)(v->x >> 4), (int)(v->y >> 4), v->z, d->fog ? fogged(v->rgba, v->f, d->fogcol) : v->rgba);
            }
        }
        break;
    case CMD_UPLOAD: exec_upload(c); break;
    case CMD_COPY: exec_copy(c); break;
    case CMD_CLUT: clut_fill(r->clut, c->u.clut.t0, c->u.clut.texclut); break;
    case CMD_SHOW:
        if (c->u.show.rgb != NULL) {
            exec_window(r, c);
        } else {
            exec_show(c);
        }
        break;
    default: break;
    }
}

/* ---- GS threads: TTT_GS_THREADS=n raster workers (0: none, the calling thread draws) ---- */

#include <unistd.h>
#include "port_host.h"

#define RING 8192u /* commands; a power of two */
#define GL_RING 65536u /* with the GL back end: more than a fight's VBlank, so the front end runs on while the window
                          thread presents */
static uint32_t sRingN = RING;
static int sGl; /* the GL back end (gs_gl.c) on the window thread is the one consumer of the ring */

static int sThreads = -1;   /* raster workers; 0 until started */
static Raster sSync = {0, 1};
static Raster sWorker[16];
static Cmd *sRing;
/* The ring's counters, each on a cache line of its own: every command moves one of them, and threads sharing a line
   would pass it back and forth at every command (T30 batch 2). */
typedef struct {
    uint32_t v;
    uint8_t pad[60];
} __attribute__((aligned(64))) Counter;
static Counter sHeadC;     /* commands published by the front end */
static Counter sTailC[16]; /* per worker: commands finished (the next one to read) */
static Counter sSleepC[2]; /* threads asleep on sWake (workers waiting for commands) / sIdle (for progress) */
#define sHead sHeadC.v
#define sTail(k) sTailC[k].v
#define sSleepWake sSleepC[0].v
#define sSleepIdle sSleepC[1].v
static uint32_t sMinTail; /* the front end's last look at min_tail(): the workers are at least that far */
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sWake = PTHREAD_COND_INITIALIZER, sIdle = PTHREAD_COND_INITIALIZER;

static uint32_t ld(const uint32_t *p) { return __atomic_load_n(p, __ATOMIC_ACQUIRE); }

static uint32_t min_tail(void) {
    uint32_t m = ld(&sTail(0));
    int k;
    for (k = 1; k < sThreads; k++) {
        uint32_t t = ld(&sTail(k));
        if ((int32_t)(t - m) < 0) {
            m = t;
        }
    }
    return m;
}

/* Waits until cond() holds: spins a little, then sleeps on cv (sWake: new commands, broadcast by the front end;
   sIdle: a finished command, broadcast by the workers). */
static void wait_until(int (*cond)(void *), void *arg, pthread_cond_t *cv) {
    int spin;
    uint32_t *sleepers = cv == &sWake ? &sSleepWake : &sSleepIdle;
    for (spin = 0; spin < 100; spin++) {
        if (cond(arg)) {
            return;
        }
        __builtin_ia32_pause();
    }
    pthread_mutex_lock(&sLock);
    __atomic_add_fetch(sleepers, 1, __ATOMIC_SEQ_CST);
    while (!cond(arg)) {
        pthread_cond_wait(cv, &sLock);
    }
    __atomic_sub_fetch(sleepers, 1, __ATOMIC_SEQ_CST);
    pthread_mutex_unlock(&sLock);
}

static void wake(pthread_cond_t *cv) {
    uint32_t *sleepers = cv == &sWake ? &sSleepWake : &sSleepIdle;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (__atomic_load_n(sleepers, __ATOMIC_SEQ_CST) != 0) {
        pthread_mutex_lock(&sLock);
        pthread_cond_broadcast(cv);
        pthread_mutex_unlock(&sLock);
    }
}

static int has_cmd(void *a) { return ld(&sHead) != *(const uint32_t *)a; }
static int all_reached(void *a) { return (int32_t)(min_tail() - *(const uint32_t *)a) >= 0; }
static int worker0_past(void *a) { return (int32_t)(ld(&sTail(0)) - *(const uint32_t *)a) > 0; }

static void *worker_main(void *arg) {
    Raster *r = arg;
    uint32_t i = 0, head = 0;

    for (;;) {
        const Cmd *c;
        int sync;
        if (i == head) { /* every command seen is done: look again, and before waiting tell the waiters on progress */
            head = ld(&sHead);
            if (i == head) {
                wake(&sIdle);
                wait_until(has_cmd, &i, &sWake);
                head = ld(&sHead);
            }
        }
        c = &sRing[i & (sRingN - 1)];
        sync = c->serial || c->barrier;
        if (sync) {
            /* everyone has finished the commands before this one */
            wake(&sIdle);
            wait_until(all_reached, &i, &sIdle);
        }
        if (c->serial) {
            if (r->id == 0) {
                r->count = 1; /* all rows */
                exec_cmd(r, c);
                r->count = sThreads;
            } else {
                wait_until(worker0_past, &i, &sIdle);
            }
        } else {
            exec_cmd(r, c);
        }
        i++;
        __atomic_store_n(&sTail(r->id), i, __ATOMIC_RELEASE);
        /* the waiters on progress are told after a serial command or a barrier, every 32 commands, and before this
           worker waits (above): none is left asleep, and the fence in wake() is not paid at every command */
        if (sync || (i & 31) == 0) {
            wake(&sIdle);
        }
    }
    return NULL;
}

static void threads_start(void) {
    const char *e = getenv("TTT_GS_THREADS");
    int n, k;

    if (sThreads >= 0) {
        return;
    }
    if (e != NULL) {
        n = atoi(e);
    } else { /* default: one per processor but the one the game runs on, up to 8 */
        long cpus = sysconf(_SC_NPROCESSORS_ONLN);
        n = cpus > 9 ? 8 : cpus > 1 ? (int)cpus - 1 : 0;
    }
    if (getenv("TTT_GS_TRACE") || getenv("TTT_GS_TRACE_PRIMS")) {
        n = 0; /* the traces print from the pixels, in order */
    } else if (Port_WindowGl()) { /* the window thread draws everything (Gs_GlPump): one consumer, no raster threads */
        sRing = calloc(GL_RING, sizeof(Cmd));
        if (sRing != NULL) {
            sRingN = GL_RING;
            sGl = 1;
            sThreads = 1;
            return;
        }
        fprintf(stderr, "ttt: gs: no memory for the GL back end's ring\n");
        exit(2);
    }
    if (n > 16) {
        n = 16;
    }
    if (n > 0) {
        sRing = calloc(RING, sizeof(Cmd));
        if (sRing == NULL) {
            n = 0;
        }
    }
    for (k = 0; k < n; k++) {
        pthread_t t;
        sWorker[k].id = k;
        sWorker[k].count = n;
        if (pthread_create(&t, NULL, worker_main, &sWorker[k]) != 0) {
            fprintf(stderr, "ttt: gs: cannot start raster thread %d: drawing on the main thread\n", k);
            n = 0; /* none started yet past this one cannot be undone: only reached at the first start */
            break;
        }
        pthread_detach(t);
    }
    sThreads = n;
}

static int all_done(void *a) { (void)a; return min_tail() == ld(&sHead); }

/* Waits until the back end has carried out every command (GS memory is then up to date). */
static void gs_sync(void) {
    if (sThreads > 0) {
        wake(&sWake);
        wait_until(all_done, NULL, &sIdle);
    }
}

/* The room the front end waits for when the ring is full: with the GL back end a quarter of the ring, so that the
   window thread wakes it once per quarter instead of every few commands (each wake a futex call on both sides). */
static uint32_t sRoomWant = 1;

static int has_room(void *a) {
    (void)a;
    sMinTail = min_tail();
    return sRingN - (sHead - sMinTail) >= sRoomWant;
}

/* Whether the front end, if it waits for room, has it with the back end at `tail`. */
static int room_at(uint32_t tail) { return sRingN - (ld(&sHead) - tail) >= __atomic_load_n(&sRoomWant, __ATOMIC_RELAXED); }

static uint32_t sGlShows; /* window pictures queued for the GL back end and not yet drawn */
static uint32_t sGlCpuMs; /* the window thread's CPU time in ms as of its last commands (TTT_GS_STATS); 32 bits: 64-bit
                             atomics would call libatomic through new PLT entries (gotcha 20) */
int gGsGlSkip;

/* The GL back end's side of the ring (the window thread, plat_window.c): runs the queued commands on gs_gl.c, waiting up
   to ms milliseconds when there are none (the window thread also reads the input events). A window picture is only
   shown when no newer one is queued: the back end catches up with the front end instead of presenting every one. */
int Gs_GlPump(int ms) {
    static uint32_t next;
    uint32_t head = ld(&sHead);
    int n = 0;

    if (next == head) {
        struct timespec ts;
        int spin;
        wake(&sIdle);
        for (spin = 0; spin < 100 && ld(&sHead) == next; spin++) {
            __builtin_ia32_pause();
        }
        if (ld(&sHead) == next) {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += (long)ms * 1000000;
            while (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_mutex_lock(&sLock);
            __atomic_add_fetch(&sSleepWake, 1, __ATOMIC_SEQ_CST);
            while (ld(&sHead) == next && pthread_cond_timedwait(&sWake, &sLock, &ts) == 0) {
            }
            __atomic_sub_fetch(&sSleepWake, 1, __ATOMIC_SEQ_CST);
            pthread_mutex_unlock(&sLock);
        }
        head = ld(&sHead);
    }
    while (next != head && sRing != NULL) {
        const Cmd *c = &sRing[next & (sRingN - 1)];
        if (c->type == CMD_SHOW && c->u.show.window) {
            gGsGlSkip = __atomic_sub_fetch(&sGlShows, 1, __ATOMIC_RELAXED) != 0;
        }
        Gl_Exec(c);
        next++;
        if ((next & 63) == 0) { /* the tail every 64 commands: its cache line is the front end's to read */
            __atomic_store_n(&sTail(0), next, __ATOMIC_RELEASE);
            if (room_at(next)) {
                wake(&sIdle);
            }
        }
        if (++n >= 4096) {
            break; /* back to the window's events now and then */
        }
        if (next == head) {
            head = ld(&sHead);
        }
    }
    __atomic_store_n(&sTail(0), next, __ATOMIC_RELEASE);
    if (next == ld(&sHead) || room_at(next)) {
        wake(&sIdle);
    }
    if (n > 0) {
        struct timespec ts;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        __atomic_store_n(&sGlCpuMs, (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000), __ATOMIC_RELAXED);
    }
    return n;
}

uint32_t Gs_GlCpuMs(void) { return __atomic_load_n(&sGlCpuMs, __ATOMIC_RELAXED); }

static int64_t sWaitNs; /* the front end's CPU time spent waiting for room in the ring (TTT_GS_STATS) */

static int64_t thread_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

/* Hands a command to the back end: at once, or through the ring. */
static uint32_t sCmdCount[3]; /* commands, serial ones, barriers (TTT_GS_THREADS report on stderr) */

static void submit(const Cmd *c) {
    sCmdCount[0]++;
    sCmdCount[1] += c->serial;
    sCmdCount[2] += c->barrier;
    if (sThreads < 0) {
        threads_start();
    }
    if (sThreads == 0) {
        exec_cmd(&sSync, c);
        return;
    }
    if (sHead - sMinTail >= sRingN && !has_room(NULL)) { /* full as far as last seen: look again, then wait */
        int64_t t0 = thread_ns();
        __atomic_store_n(&sRoomWant, sGl ? sRingN / 4 : 1, __ATOMIC_RELAXED);
        wake(&sWake);
        wait_until(has_room, NULL, &sIdle);
        __atomic_store_n(&sRoomWant, 1, __ATOMIC_RELAXED);
        sWaitNs += thread_ns() - t0;
    }
    sRing[sHead & (sRingN - 1)] = *c;
    if (sGl && c->type == CMD_SHOW && c->u.show.window) {
        __atomic_add_fetch(&sGlShows, 1, __ATOMIC_RELAXED);
    }
    __atomic_store_n(&sHead, sHead + 1, __ATOMIC_RELEASE);
    /* sleeping workers are woken for a batch of commands; at once for one the others will wait for (a worker
       waiting at a barrier is woken by the workers' progress) */
    if (sGl ? (sHead - ld(&sTail(0)) >= 2048 || c->type == CMD_SHOW) : (c->serial || c->barrier || (sHead & 15) == 0)) {
        wake(&sWake); /* the GL back end: for a good part of the ring or a picture (it also looks every 4 ms) */
    }
}

static void raster_reset(void) {
    int k;
    memset(sSync.clut, 0, sizeof sSync.clut);
    for (k = 0; k < 16; k++) {
        memset(sWorker[k].clut, 0, sizeof sWorker[k].clut);
    }
}

static uint32_t gs_pixels(void) {
    uint32_t n = sSync.pixels;
    int k;
    for (k = 0; k < sThreads; k++) {
        n += sWorker[k].pixels;
    }
    return n;
}

/* ---- hazards: when the workers' row split is not enough ----
   Workers run apart from each other between barriers: each draws its rows of every primitive. That is the serial
   result as long as, since the last barrier, a word of GS memory is only ever written through one buffer layout (it
   is then always the same worker's) and nothing is read by a worker that another may write: a texture or a CLUT load
   must not read pages the primitives since the last barrier wrote, and a primitive must not write pages they read.
   The front end tracks the 8 KB pages written since the last barrier (frame and depth apart), the pages read, and the
   frame / depth layouts:
   - a primitive whose frame, depth and texture pages overlap each other is serial (worker 0 alone);
   - a primitive with another frame or depth layout, or whose pages meet the others' as above, starts after a barrier;
   - a CLUT load whose pages were written starts after a barrier.
   Uploads, local copies and the VBlank picture are serial. */
static struct {
    int open, fset, zset;
    uint32_t fbp, fbw, fpsm, zbp, zpsm;
    uint32_t fpages[16], zpages[16], rpages[16];
} sEpoch;

/* The pages (8 KB, 512 of them) of the pixels x0..x1, y0..y1 of a buffer (as vram_loc addresses them). */
static void pages_of(uint32_t *set, uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1) {
    /* PSMT8H / 4HL / 4HH are fields of 32-bit pixels: their pages are the 32-bit layout's (T30 batch 3) */
    uint32_t bits = (uint32_t)psm_bits(psm == 0x1B || psm == 0x24 || psm == 0x2C ? 0 : psm), pw = bits >= 16 ? 64 : 128, ph = bits == 32 || bits == 24 ? 32 : bits == 4 ? 128 : 64;
    uint32_t bwp, lo, hi, p;

    if (bw == 0) {
        bw = 1;
    }
    bwp = bits >= 16 ? bw : bw >> 1;
    x0 &= 0x7FF; x1 &= 0x7FF; y0 &= 0x7FF; y1 &= 0x7FF;
    if (x1 < x0 || y1 < y0) {
        x0 = 0; x1 = 0x7FF; y0 = 0; y1 = 0x7FF;
    }
    lo = bp + (y0 / ph) * bwp * 32 + (x0 / pw) * 32;
    hi = bp + (y1 / ph) * bwp * 32 + (x1 / pw) * 32 + 32; /* blocks, exclusive */
    if (hi > 16384) { /* wraps around GS memory */
        memset(set, 0xFF, 64);
        return;
    }
    for (p = lo >> 5; p <= (hi - 1) >> 5;) { /* pages lo / 32 .. (hi - 1) / 32, a word of the set at a time */
        uint32_t last = (hi - 1) >> 5, end = (p | 31) < last ? (p | 31) : last, n = end - (p & ~31u) + 1;
        set[p >> 5] |= (n >= 32 ? 0xFFFFFFFFu : (1u << n) - 1) & (0xFFFFFFFFu << (p & 31));
        p = end + 1;
    }
}

static int pages_cross(const uint32_t *a, const uint32_t *b) {
    int i;
    for (i = 0; i < 16; i++) {
        if (a[i] & b[i]) {
            return 1;
        }
    }
    return 0;
}

/* The texel coordinates a wrap mode can give (texel(): REPEAT / CLAMP within the size, REGION_CLAMP between MIN and
   MAX, REGION_REPEAT (u & MSK) | FIX). */
static void tex_range(uint32_t wm, int size, int mn, int mx, uint32_t *lo, uint32_t *hi) {
    if (wm <= 1) {
        *lo = 0;
        *hi = (uint32_t)size - 1;
    } else if (wm == 2) {
        *lo = (uint32_t)(mn < mx ? mn : mx);
        *hi = (uint32_t)(mn < mx ? mx : mn);
    } else {
        *lo = (uint32_t)mx;
        *hi = (uint32_t)(mn | mx);
    }
}

static void epoch_open(void) {
    if (!sEpoch.open) {
        memset(&sEpoch, 0, sizeof sEpoch);
        sEpoch.open = 1;
    }
}

static void epoch_add(uint32_t *set, const uint32_t *pages) {
    int k;
    for (k = 0; k < 16; k++) {
        set[k] |= pages[k];
    }
}

/* A CLUT load reads GS memory: after a barrier when the primitives since the last one wrote its pages. */
static void classify_clut(Cmd *c) {
    uint64_t t0 = c->u.clut.t0, tc = c->u.clut.texclut;
    uint32_t cp[16] = {0}, cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15, csm = (t0 >> 55) & 1;

    if (sThreads <= 0 || sGl) {
        return;
    }
    if (csm) {
        pages_of(cp, cbp, (uint32_t)(tc & 0x3F), cpsm, 0, 0x7FF, (uint32_t)((tc >> 12) & 0x3FF), (uint32_t)((tc >> 12) & 0x3FF));
    } else {
        pages_of(cp, cbp, 1, cpsm, 0, 15, 0, 15);
    }
    if (sEpoch.open && (pages_cross(cp, sEpoch.fpages) || pages_cross(cp, sEpoch.zpages))) {
        c->barrier = 1;
        sEpoch.open = 0;
    }
    epoch_open();
    epoch_add(sEpoch.rpages, cp);
}

static void classify(Cmd *c) {
    const Draw *d = &c->u.prim.d;
    uint32_t fp[16] = {0}, zp[16] = {0}, tp[16] = {0};
    /* depth is read by the GEQUAL / GREATER tests only, written unless ZMSK */
    int zused = (d->ztest && d->ztst >= 2) || d->zwrite;
    int x0 = d->scx0, x1 = d->scx1, y0 = d->scy0, y1 = d->scy1;

    if (sThreads <= 0 || sGl) {
        return;
    }
    if (c->type == CMD_SPRITE) { /* a sprite's pixels lie inside its rectangle */
        const Vtx *a = &c->u.prim.v[0], *b = &c->u.prim.v[1];
        int bx0 = (a->x < b->x ? a->x : b->x) >> 4, bx1 = ((a->x > b->x ? a->x : b->x) >> 4) + 1;
        int by0 = (a->y < b->y ? a->y : b->y) >> 4, by1 = ((a->y > b->y ? a->y : b->y) >> 4) + 1;
        if (bx0 > x0) { x0 = bx0; }
        if (bx1 < x1) { x1 = bx1; }
        if (by0 > y0) { y0 = by0; }
        if (by1 < y1) { y1 = by1; }
        if (x1 < x0 || y1 < y0) { /* nothing drawn */
            x1 = x0;
            y1 = y0;
        }
    }
    pages_of(fp, d->fbp, d->fbw, d->fpsm, (uint32_t)x0, (uint32_t)x1, (uint32_t)y0, (uint32_t)y1);
    if (zused) {
        pages_of(zp, d->zbp, d->fbw, d->zpsm, (uint32_t)x0, (uint32_t)x1, (uint32_t)y0, (uint32_t)y1);
    }
    if (d->tex) {
        uint32_t u0, u1, v0, v1;
        tex_range(d->wms, d->tw, d->minu, d->maxu, &u0, &u1);
        tex_range(d->wmt, d->th, d->minv, d->maxv, &v0, &v1);
        if (c->type == CMD_SPRITE && d->fst) { /* UV sprites: the texels between the corners' UV (one more each side
                                                  for the bilinear filter), when the wrap mode does not move them */
            const Vtx *a = &c->u.prim.v[0], *b = &c->u.prim.v[1];
            int ua = ((a->u < b->u ? a->u : b->u) >> 4) - 1, ub = ((a->u > b->u ? a->u : b->u) >> 4) + 1;
            int va = ((a->v < b->v ? a->v : b->v) >> 4) - 1, vb = ((a->v > b->v ? a->v : b->v) >> 4) + 1;
            if (d->wms <= 1 && ua >= 0 && ub <= d->tw - 1) { u0 = (uint32_t)ua; u1 = (uint32_t)ub; }
            if (d->wmt <= 1 && va >= 0 && vb <= d->th - 1) { v0 = (uint32_t)va; v1 = (uint32_t)vb; }
        }
        pages_of(tp, d->tbp, d->tbw, d->tpsm, u0, u1, v0, v1);
    }
    if (pages_cross(fp, zp) || pages_cross(tp, fp) || pages_cross(tp, zp)) {
        c->serial = 1;
        sEpoch.open = 0;
        return;
    }
    if (sEpoch.open && ((sEpoch.fset && (sEpoch.fbp != d->fbp || sEpoch.fbw != d->fbw || sEpoch.fpsm != d->fpsm)) ||
                        (zused && sEpoch.zset && (sEpoch.zbp != d->zbp || sEpoch.zpsm != d->zpsm || sEpoch.fbw != d->fbw)) ||
                        pages_cross(tp, sEpoch.fpages) || pages_cross(tp, sEpoch.zpages) || pages_cross(fp, sEpoch.zpages) ||
                        pages_cross(fp, sEpoch.rpages) || pages_cross(zp, sEpoch.fpages) || pages_cross(zp, sEpoch.rpages))) {
        sEpoch.open = 0;
        c->barrier = 1;
    }
    epoch_open();
    if (!sEpoch.fset) {
        sEpoch.fset = 1;
        sEpoch.fbp = d->fbp;
        sEpoch.fbw = d->fbw;
        sEpoch.fpsm = d->fpsm;
    }
    if (zused && !sEpoch.zset) {
        sEpoch.zset = 1;
        sEpoch.zbp = d->zbp;
        sEpoch.zpsm = d->zpsm;
    }
    epoch_add(sEpoch.fpages, fp);
    epoch_add(sEpoch.zpages, zp);
    epoch_add(sEpoch.rpages, tp);
}

/* An operation on GS memory: worker 0 alone, after every earlier command; the next epoch starts after it. */
static void submit_serial(Cmd *c) {
    c->serial = 1;
    c->barrier = 0;
    sEpoch.open = 0;
    submit(c);
}

static void submit_prim(int type, int ctx, const Vtx *a, const Vtx *b, const Vtx *c3) {
    Cmd c;

    c.type = (uint8_t)type;
    c.serial = 0;
    c.barrier = 0;
    draw_setup(&c.u.prim.d, ctx);
    c.u.prim.v[0] = *a;
    if (b != NULL) {
        c.u.prim.v[1] = *b;
    }
    if (c3 != NULL) {
        c.u.prim.v[2] = *c3;
    }
    classify(&c);
    submit(&c);
}

/* ------------------------------------------------------------------------------------------------ vertices */

static int ctx_of(void) { return (int)((gs.prim >> 9) & 1); }

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
    if (sTracePrimVb != 0 && sCurVb == sTracePrimVb) { /* gs-adc: a vertex that only queues (XYZ3, ADC) */
        printf("%s t%d ctx%d prim %03llX xy %d,%d z %X uv %d,%d st %08X,%08X q %08X rgba %08X tex0 %016llX tex1 %llX clamp %llX frame %llX test %llX alpha %llX texa %llX zbuf %llX scissor %llX\n",
               kick ? "gs-prim" : "gs-adc", type, ctx, (unsigned long long)gs.prim, v.x >> 4, v.y >> 4, v.z, v.u >> 4, v.v >> 4, v.s, v.t, v.q, v.rgba,
               (unsigned long long)gs.tex0[ctx], (unsigned long long)gs.tex1[ctx], (unsigned long long)gs.clamp[ctx], (unsigned long long)gs.frame[ctx],
               (unsigned long long)gs.test[ctx], (unsigned long long)gs.alpha[ctx], (unsigned long long)gs.texa,
               (unsigned long long)gs.zbuf[ctx], (unsigned long long)gs.scissor[ctx]);
    }
    switch (type) {
    case 0:
        if (kick) {
            sStat.prims++;
            submit_prim(CMD_POINT, ctx, &v, NULL, NULL);
        }
        gs.vcount = 0;
        break;
    case 1:
    case 2:
        if (gs.vcount == 2) {
            if (kick) {
                sStat.prims++;
                submit_prim(CMD_LINE, ctx, &gs.vtx[0], &gs.vtx[1], NULL);
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
                submit_prim(CMD_TRIANGLE, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]);
            }
            gs.vcount = 0;
        }
        break;
    case 4: /* strip: the queue keeps sliding */
        if (gs.vcount == 3 && kick) {
            sStat.prims++;
            submit_prim(CMD_TRIANGLE, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]);
        }
        break;
    case 5: /* fan: the first vertex stays */
        if (gs.vcount == 3) {
            if (kick) {
                sStat.prims++;
                submit_prim(CMD_TRIANGLE, ctx, &gs.vtx[0], &gs.vtx[1], &gs.vtx[2]);
            }
            gs.vtx[1] = gs.vtx[2];
            gs.vcount = 2;
        }
        break;
    case 6:
        if (gs.vcount == 2) {
            if (kick) {
                sStat.prims++;
                submit_prim(CMD_SPRITE, ctx, &gs.vtx[0], &gs.vtx[1], NULL);
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
        Cmd c;
        c.type = CMD_COPY;
        c.u.copy.bitbltbuf = gs.bitbltbuf;
        c.u.copy.trxpos = gs.trxpos;
        c.u.copy.tx = gs.tx;
        c.u.copy.ty = gs.ty;
        c.u.copy.tw = gs.tw;
        c.u.copy.th = gs.th;
        submit_serial(&c);
        sStat.localCopies++;
        sUploadGen++;
    }
}

static void exec_copy(const Cmd *c) {
    uint64_t bb = c->u.copy.bitbltbuf, tp = c->u.copy.trxpos;
    uint32_t sbp = bb & 0x3FFF, sbw = (bb >> 16) & 0x3F, spsm = (bb >> 24) & 0x3F;
    uint32_t dbp = (bb >> 32) & 0x3FFF, dbw = (bb >> 48) & 0x3F, dpsm = (bb >> 56) & 0x3F;
    uint32_t sx = tp & 0x7FF, sy = (tp >> 16) & 0x7FF, x, y;
    for (y = 0; y < c->u.copy.th; y++) {
        for (x = 0; x < c->u.copy.tw; x++) {
            vram_rw(dbp, dbw, dpsm, c->u.copy.tx + x, c->u.copy.ty + y, 1, vram_rw(sbp, sbw, spsm, sx + x, sy + y, 0, 0));
        }
    }
}

/* Image data of a host -> GS transfer: whole pixels in the destination format, row after row. The front end keeps
   the position (tpos); the back end writes the pixels from a copy of the data. */
static void transfer_data(const uint8_t *p, uint32_t bytes) {
    uint32_t dpsm = (gs.bitbltbuf >> 56) & 0x3F, bits, total, n;
    Cmd c;

    if (sThreads < 0) {
        threads_start();
    }
    if (!gs.transfer || gs.tw == 0) {
        return;
    }
    sUploadGen++;
    bits = (uint32_t)psm_bits(dpsm);
    total = gs.tw * gs.th;
    c.type = CMD_UPLOAD;
    c.u.upload.bitbltbuf = gs.bitbltbuf;
    c.u.upload.tx = gs.tx;
    c.u.upload.ty = gs.ty;
    c.u.upload.tw = gs.tw;
    c.u.upload.th = gs.th;
    c.u.upload.tpos = gs.tpos;
    c.u.upload.bytes = bytes;
    c.u.upload.data = NULL;
    n = bytes * 8 / bits; /* whole pixels in the data */
    gs.tpos = gs.tpos >= total ? gs.tpos : total - gs.tpos < n ? total : gs.tpos + n;
    if (sThreads > 0) {
        c.u.upload.data = malloc(bytes ? bytes : 1);
        if (c.u.upload.data == NULL) {
            fprintf(stderr, "ttt: gs: out of memory for an upload\n");
            exit(2);
        }
        memcpy(c.u.upload.data, p, bytes);
        submit_serial(&c);
    } else {
        c.u.upload.data = (uint8_t *)p;
        exec_upload(&c);
    }
}

static void exec_upload(const Cmd *c) {
    uint64_t bb = c->u.upload.bitbltbuf;
    uint32_t dbp = (bb >> 32) & 0x3FFF, dbw = (bb >> 48) & 0x3F, dpsm = (bb >> 56) & 0x3F;
    uint32_t tx = c->u.upload.tx, ty = c->u.upload.ty, tw = c->u.upload.tw, th = c->u.upload.th, tpos = c->u.upload.tpos;
    uint32_t bytes = c->u.upload.bytes, bits, total, i = 0;
    const uint8_t *p = c->u.upload.data;

    bits = (uint32_t)psm_bits(dpsm);
    total = tw * th;
    /* the usual formats a row at a time through the page tables (the same writes as vram_rw) */
    if (dpsm == 0x00 || dpsm == 0x02 || dpsm == 0x0A || dpsm == 0x13 || dpsm == 0x14) {
        uint32_t bw = dbw ? dbw : 1, count = bytes * 8 / bits;
        uint8_t *vram8 = (uint8_t *)sVram;
        uint16_t *vram16 = (uint16_t *)sVram;
        while (i < count && tpos < total) {
            uint32_t col = tpos % tw, y = (ty + tpos / tw) & 0x7FF, x = tx + col, run = tw - col, j;
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
                    uint32_t cc = (p[k >> 1] >> ((k & 1) * 4)) & 15;
                    *b = (a & 1) ? (uint8_t)((*b & 0x0F) | cc << 4) : (uint8_t)((*b & 0xF0) | cc);
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
            tpos += run;
        }
    } else {
        for (; (i + 1) * bits <= bytes * 8 && tpos < total; tpos++, i++) {
            uint32_t x = tx + tpos % tw, y = ty + tpos / tw, cc;
            switch (bits) {
            case 32: cc = p[i * 4] | p[i * 4 + 1] << 8 | p[i * 4 + 2] << 16 | (uint32_t)p[i * 4 + 3] << 24; break;
            case 24: cc = p[i * 3] | p[i * 3 + 1] << 8 | (uint32_t)p[i * 3 + 2] << 16; break;
            case 16: cc = p[i * 2] | (uint32_t)p[i * 2 + 1] << 8; break;
            case 8: cc = p[i]; break;
            default: cc = (p[i / 2] >> ((i & 1) * 4)) & 15; break;
            }
            vram_rw(dbp, dbw, dpsm, x, y, 1, cc);
        }
    }
    if (sThreads > 0) {
        free(c->u.upload.data);
    }
}

/* ------------------------------------------------------------------------------------------------ registers */

/* PRMODECONT.AC = 1: the attributes are PRIM's own; AC = 0: PRMODE's, with PRIM's type. */
static void prim_update(void) {
    gs.prim = (gs.prmodecont & 1) ? gs.primRaw : (gs.primRaw & 7) | (gs.prmode & 0x7F8);
}

/* TEX0 / TEX2: the CLUT load, when CLD asks for one. */
static void clut_load(uint64_t t0) {
    if (clut_wanted(t0)) {
        Cmd c;
        c.type = CMD_CLUT;
        c.serial = 0;
        c.barrier = 0;
        c.u.clut.t0 = t0;
        c.u.clut.texclut = gs.texclut;
        classify_clut(&c);
        submit(&c);
    }
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
        if (Gs_AsyncRegWrite(1, m & 0xFFFFFFFFull, d)) {
            break; /* the GS thread: the game's thread writes the registers (gs_async.c) */
        }
        PRIV(GS_SIGLBLID) = (id & ~m) | (d & m & 0xFFFFFFFFull);
        PRIV(GS_CSR) |= 1;
        break;
    }
    case 0x61: /* FINISH */
        if (!Gs_AsyncRegWrite(2, 0, 0)) {
            PRIV(GS_CSR) |= 2;
        }
        break;
    case 0x62: { /* LABEL: SIGLBLID.LBLID under the mask */
        uint64_t id = PRIV(GS_SIGLBLID), m = (d >> 32) << 32;
        if (Gs_AsyncRegWrite(0, m, d << 32)) {
            break;
        }
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


static void circuit(const DispRegs *regs, int k, Circuit *c) {
    uint64_t fb = regs->dispfb[k], disp = regs->display[k];
    uint32_t magh = (uint32_t)(disp >> 23) & 15, magv = (uint32_t)(disp >> 27) & 3;

    c->fbp = (uint32_t)(fb & 0x1FF) << 5;
    c->fbw = (uint32_t)(fb >> 9) & 0x3F;
    c->psm = (uint32_t)(fb >> 15) & 0x1F;
    c->dbx = (uint32_t)(fb >> 32) & 0x7FF;
    c->dby = (uint32_t)(fb >> 43) & 0x7FF;
    c->x0 = ((uint32_t)disp & 0xFFF) / (magh + 1);
    c->y0 = ((uint32_t)(disp >> 12) & 0x7FF) / (magv + 1);
    c->width = ((uint32_t)(disp >> 32) & 0xFFF) / (magh + 1) + 1 - (magh != 0 && ((disp >> 32) & 0xFFF) % (magh + 1) == 0);
    c->width = (((uint32_t)(disp >> 32) & 0xFFF) + 1) / (magh + 1);
    c->lines = (((uint32_t)(disp >> 44) & 0x7FF) + 1) / (magv + 1);
    if (disp == 0 || c->width == 0 || c->lines == 0) { /* nothing set up yet: the usual 640 x 448 */
        c->width = 640;
        c->lines = 448;
    }
    if (c->width > 1024) { c->width = 1024; }
    if (c->lines > 1024) { c->lines = 1024; }
}

/* Circuit c's pixel at output column x, raster line r (dbl raster lines per buffer line) as 0xAABBGGRR; *in = 0 when
   its DISPLAY area does not cover that point. */
static uint32_t circuit_pixel(const Circuit *c, int32_t x, int32_t r, uint32_t dbl, int *in) {
    uint32_t v;

    if (x < 0 || r < 0 || (uint32_t)x >= c->width || (uint32_t)r >= c->lines) {
        *in = 0;
        return 0;
    }
    *in = 1;
    v = vram_rw(c->fbp, c->fbw, c->psm, c->dbx + (uint32_t)x, c->dby + (uint32_t)r / dbl, 0, 0);
    if ((int)x == sTraceX && (int)r / (int)dbl == sTraceY) {
        printf("gs-disp %d,%d fbp %X fbw %u psm %X dbx %u dby %u -> %08X\n", x, r / (int)dbl, c->fbp, c->fbw, c->psm, c->dbx, c->dby, v);
    }
    switch (psm_bits(c->psm)) {
    case 16: return (v & 31) << 3 | ((v >> 5) & 31) << 11 | ((v >> 10) & 31) << 19 | (v & 0x8000u ? 0x80000000u : 0);
    case 24: return v | 0x80000000u; /* [I] no alpha in the buffer: taken as 0x80 (1.0) for MMOD 0 */
    default: return v;
    }
}

/* The picture the display circuits show now, as RGB, merged as the PCRTC does (PMODE): circuit 2 (EN2, SLBG 0) or
   BGCOLOR (SLBG 1, or where circuit 2 shows nothing) is the background; circuit 1 (EN1) goes over it weighted by
   ALP (MMOD 1) or by its own alpha (MMOD 0, 0x80 = 1.0), out = c1 * a + bg * (255 - a), a in 0..255, rounded [I: the
   PCRTC's rounding]. The picture has the size and position of the first enabled circuit; each circuit sits at its own
   DISPLAY.DX / DY. Interlaced frame mode (SMODE2 INT and FFMD) shows each buffer line in both fields: doubled.
   *w / *h: the size. */
/* The picture: its size (*w, *h), and with rgb its pixels (the rows of worker rs, or all of them with rs NULL). */
static void display_into(const DispRegs *regs, uint8_t *rgb, uint32_t *w, uint32_t *h, const Raster *rs) {
    uint64_t pmode = regs->pmode, smode2 = regs->smode2, bg = regs->bgcolor;
    int en1 = (int)(pmode & 1), en2 = (int)(pmode >> 1) & 1, mmod = (int)(pmode >> 5) & 1, slbg = (int)(pmode >> 7) & 1;
    uint32_t alp = (uint32_t)(pmode >> 8) & 0xFF, dbl = (smode2 & 3) == 3 ? 2 : 1, x, r;
    Circuit c[2];
    const Circuit *main;

    circuit(regs, 0, &c[0]);
    circuit(regs, 1, &c[1]);
    main = !en1 && en2 ? &c[1] : &c[0];
    *w = main->width;
    *h = main->lines;
    for (r = 0; rgb != NULL && r < main->lines; r++) {
        if (rs != NULL && !row_mine(rs, (int)r)) {
            continue;
        }
        for (x = 0; x < main->width; x++) {
            int32_t ax = (int32_t)(x + main->x0), ar = (int32_t)(r + main->y0);
            uint32_t base = (uint32_t)bg & 0xFFFFFF, out, k;
            int in;

            if (en2 && !slbg) {
                uint32_t p = circuit_pixel(&c[1], ax - (int32_t)c[1].x0, ar - (int32_t)c[1].y0, dbl, &in);
                if (in) {
                    base = p;
                }
            }
            out = base;
            if (en1) {
                uint32_t p = circuit_pixel(&c[0], ax - (int32_t)c[0].x0, ar - (int32_t)c[0].y0, dbl, &in);
                if (in) {
                    uint32_t a = mmod ? alp : (p >> 24) * 2 > 255 ? 255 : (p >> 24) * 2;
                    out = 0;
                    for (k = 0; k < 24; k += 8) {
                        out |= ((((p >> k) & 0xFF) * a + ((base >> k) & 0xFF) * (255 - a) + 127) / 255) << k;
                    }
                }
            }
            rgb[((size_t)r * main->width + x) * 3 + 0] = (uint8_t)out;
            rgb[((size_t)r * main->width + x) * 3 + 1] = (uint8_t)(out >> 8);
            rgb[((size_t)r * main->width + x) * 3 + 2] = (uint8_t)(out >> 16);
        }
    }
}

static uint8_t *displayed(const DispRegs *regs, uint32_t *w, uint32_t *h) {
    uint8_t *rgb;

    display_into(regs, NULL, w, h, NULL);
    rgb = calloc((size_t)*w * *h, 3);
    if (rgb != NULL) {
        display_into(regs, rgb, w, h, NULL);
    }
    return rgb;
}

/* The display registers now. */
static void disp_regs(DispRegs *r) {
    r->pmode = PRIV(GS_PMODE);
    r->smode2 = PRIV(GS_SMODE2);
    r->bgcolor = PRIV(GS_BGCOLOR);
    r->dispfb[0] = PRIV(GS_DISPFB1);
    r->dispfb[1] = PRIV(GS_DISPFB2);
    r->display[0] = PRIV(GS_DISPLAY1);
    r->display[1] = PRIV(GS_DISPLAY2);
}

static const char *sDumpDir;
static uint32_t sDumpEvery;

/* The picture of a VBlank, in order with the drawing: the dump's PNG. */
static void exec_show(const Cmd *c) {
    char name[4096];
    uint32_t w, h;
    uint8_t *rgb = displayed(&c->u.show.regs, &w, &h);

    if (rgb == NULL) {
        return;
    }
    snprintf(name, sizeof name, "%s/frame_%05u.png", sDumpDir, (unsigned)c->u.show.vblank);
    if (!write_png(name, w, h, rgb)) {
        fprintf(stderr, "ttt: cannot write %s\n", name);
    }
    free(rgb);
}

/* The window's pictures: each VBlank the workers draw the picture into a free slot (each its rows, after everything
   queued before it); the newest finished one goes to the window. Three slots: the window lags the front end by up
   to two pictures and skips one when the back end is further behind. */
static struct { uint8_t *rgb; uint32_t w, h, idx; int busy; } sWin[3];

static void exec_window(Raster *r, const Cmd *c) {
    uint32_t w, h;
    display_into(&c->u.show.regs, c->u.show.rgb, &w, &h, r);
}

static int cmd_done(uint32_t idx) {
    return sThreads <= 0 || (int32_t)(min_tail() - (idx + 1)) >= 0;
}

static void window_frames(const DispRegs *regs) {
    int k, newest = -1;
    Cmd c;

    if (Port_WindowClosed()) {
        printf("ttt: window closed\n");
        fflush(stdout);
        exit(0);
    }
    if (sThreads < 0) {
        threads_start();
    }
    if (sGl) { /* the GL back end draws the picture in order with the primitives and shows it */
        c.type = CMD_SHOW;
        c.serial = 0;
        c.barrier = 1; /* wakes the window thread */
        c.u.show.regs = *regs;
        c.u.show.vblank = 0;
        c.u.show.rgb = NULL;
        c.u.show.window = 1;
        submit(&c);
        return;
    }
    for (k = 0; k < 3; k++) {
        if (sWin[k].busy && cmd_done(sWin[k].idx) && (newest < 0 || (int32_t)(sWin[k].idx - sWin[newest].idx) > 0)) {
            newest = k;
        }
    }
    if (newest >= 0) {
        Port_WindowPresent(sWin[newest].rgb, sWin[newest].w, sWin[newest].h);
        for (k = 0; k < 3; k++) {
            if (sWin[k].busy && (int32_t)(sWin[k].idx - sWin[newest].idx) <= 0) {
                sWin[k].busy = 0; /* shown, or older than the one shown */
            }
        }
    }
    for (k = 0; k < 3 && sWin[k].busy; k++) {
    }
    if (k == 3) {
        return; /* the back end is three pictures behind: this one is skipped */
    }
    if (sWin[k].rgb == NULL) {
        sWin[k].rgb = malloc(1024 * 1024 * 3);
        if (sWin[k].rgb == NULL) {
            return;
        }
    }
    tables_init();
    c.type = CMD_SHOW;
    c.serial = 0;
    c.barrier = 0;
    c.u.show.regs = *regs;
    c.u.show.vblank = 0;
    c.u.show.rgb = sWin[k].rgb;
    c.u.show.window = 0;
    display_into(&c.u.show.regs, NULL, &sWin[k].w, &sWin[k].h, NULL);
    if (sThreads > 0) { /* it reads what everything before it drew, and nothing after it may draw there before it read */
        c.barrier = 1;
        sEpoch.open = 0;
        epoch_open();
        memset(sEpoch.rpages, 0xFF, sizeof sEpoch.rpages);
    }
    sWin[k].idx = sHead;
    sWin[k].busy = 1;
    submit(&c);
}

/* At each VBLANK_START (plat_hw.c, the game's thread): the display registers now; the picture is made in order with the
   drawing, by the GS thread when there is one (gs_async.c). */
void Gs_VBlank(uint32_t vblank) {
    DispRegs regs;
    disp_regs(&regs);
    if (!Gs_AsyncVBlank(vblank, &regs)) {
        Gs_VBlankRun(vblank, &regs);
    }
}

void Gs_VBlankRun(uint32_t vblank, const DispRegs *regs) {
    static int init;

    if (!init) {
        const char *e = getenv("TTT_DUMP_EVERY");
        init = 1;
        sDumpDir = getenv("TTT_DUMP");
        sDumpEvery = e != NULL ? (uint32_t)strtoul(e, NULL, 0) : 60;
        if (sDumpEvery == 0) {
            sDumpEvery = 60;
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
            printf("gs-trace %u dispfb1 %llX dispfb2 %llX pmode %llX display1 %llX display2 %llX bgcolor %llX\n", (unsigned)vblank,
                   (unsigned long long)regs->dispfb[0], (unsigned long long)regs->dispfb[1], (unsigned long long)regs->pmode,
                   (unsigned long long)regs->display[0], (unsigned long long)regs->display[1], (unsigned long long)regs->bgcolor);
        }
    }
    { /* TTT_GS_TEXDUMP=<vblank>:<tex0>:<x>:<y>:<w>:<h> writes that texture region (through a CLUT loaded from TEX0) */
        const char *td = getenv("TTT_GS_TEXDUMP");
        if (td != NULL && sDumpDir != NULL && strtoul(td, NULL, 0) == vblank) {
            char *e, name[4096];
            unsigned long long t0;
            uint32_t x0, y0, tw, th, xx, yy;
            uint8_t *img;
            uint16_t saved[512];
            Raster *cur = sThreads > 0 ? &sWorker[0] : &sSync; /* every worker's CLUT is the same once they are idle */
            strtoul(td, &e, 0);
            t0 = strtoull(e + 1, &e, 16);
            x0 = (uint32_t)strtoul(e + 1, &e, 0);
            y0 = (uint32_t)strtoul(e + 1, &e, 0);
            tw = (uint32_t)strtoul(e + 1, &e, 0);
            th = (uint32_t)strtoul(e + 1, &e, 0);
            img = calloc((size_t)tw * th, 3);
            gs_sync();
            memcpy(saved, cur->clut, sizeof saved);
            {
                uint64_t lt0 = (t0 & ~(7ull << 61)) | (1ull << 61);
                if (clut_wanted(lt0)) {
                    clut_fill(cur->clut, lt0, gs.texclut);
                }
            }
            for (yy = 0; yy < th; yy++) {
                for (xx = 0; xx < tw; xx++) {
                    uint32_t psm = (uint32_t)(t0 >> 20) & 0x3F, c = vram_rw((uint32_t)t0 & 0x3FFF, (uint32_t)(t0 >> 14) & 0x3F, psm, x0 + xx, y0 + yy, 0, 0);
                    uint8_t *o = &img[(yy * tw + xx) * 3];
                    c = psm_bits(psm) > 8 ? expand(c, psm, gs.texa) : clut_read(cur->clut, t0, c, psm_bits(psm), gs.texa);
                    o[0] = (uint8_t)c; o[1] = (uint8_t)(c >> 8); o[2] = (uint8_t)(c >> 16);
                }
            }
            memcpy(cur->clut, saved, sizeof saved);
            sClutVer++;
            snprintf(name, sizeof name, "%s/tex_%05u.png", sDumpDir, (unsigned)vblank);
            write_png(name, tw, th, img);
            free(img);
        }
    }
    if (Port_WindowOpen()) {
        window_frames(regs);
    }
    if (sDumpDir == NULL || *sDumpDir == 0 || vblank % sDumpEvery != 0) {
        return;
    }
    tables_init();
    {
        Cmd c;
        c.type = CMD_SHOW;
        c.u.show.regs = *regs;
        c.u.show.vblank = vblank;
        c.u.show.rgb = NULL;
        c.u.show.window = 0;
        submit_serial(&c);
    }
}

void Gs_Finish(void) {
    Gs_AsyncDrain();
    gs_sync();
}

void Gs_Report(void) {
    Gs_AsyncDrain();
    gs_sync();
    if (sThreads > 0 && getenv("TTT_GS_STATS") != NULL) {
        struct timespec ts;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        fprintf(stderr, "gs-stats: %d raster threads; %u commands, %u serial, %u after a barrier; main thread %ld.%03ld s CPU, %ld ms of it "
                "waiting for the raster threads, %ld ms for the GS thread; GS thread %ld ms CPU; GIF thread %ld ms CPU; window thread %ld ms CPU\n", sThreads, sCmdCount[0],
                sCmdCount[1], sCmdCount[2], (long)ts.tv_sec, ts.tv_nsec / 1000000, (long)(sWaitNs / 1000000), (long)(Gs_AsyncWaitNs() / 1000000),
                (long)Gs_AsyncCpuMs(0), (long)Gs_AsyncCpuMs(1), (long)Gs_GlCpuMs());
    }
    sStat.pixels = gs_pixels();
    printf("ttt: gs: %u GIF tags, %u register writes, %u primitives, %u pixels, %u image qwords, %u local copies, "
           "%u readbacks (not modelled)\n",
           sStat.tags, sStat.regs, sStat.prims, sStat.pixels, sStat.imageQw, sStat.localCopies, sStat.readbacks);
    Gs_Vu1Report();
    Gl_Report();
    Port_WindowReport();
}

/* ------------------------------------------------------------------------------------------------ for gs_gl.c */

uint32_t *Gs_Vram(void) { return sVram; }

/* Whether the GS has raster threads or the GL back end (the GS thread of gs_async.c is then on by default). */
int Gs_ThreadsWanted(void) {
    if (sThreads < 0) {
        threads_start();
    }
    return sThreads > 0;
}
int Gs_PsmBits(uint32_t psm) { return psm_bits(psm); }

const uint16_t *Gs_PageTable(int k) {
    tables_init();
    switch (k) {
    case 0: return &sIn32[0][0];
    case 1: return &sIn16[0][0][0];
    case 2: return &sIn16[1][0][0];
    case 3: return &sIn8[0][0];
    default: return &sIn4[0][0];
    }
}

void Gs_PagesOf(uint32_t set[16], uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1) {
    pages_of(set, bp, bw, psm, x0, x1, y0, y1);
}

void Gs_TexRange(uint32_t wm, int size, int mn, int mx, uint32_t *lo, uint32_t *hi) { tex_range(wm, size, mn, mx, lo, hi); }
void Gs_CpuUpload(const Cmd *c) { exec_upload(c); }
void Gs_CpuCopy(const Cmd *c) { exec_copy(c); }
void Gs_ClutFill(uint16_t clut[512], uint64_t t0, uint64_t texclut) { clut_fill(clut, t0, texclut); }

void Gs_Palette(const uint16_t clut[512], const Draw *d, uint32_t out[256]) {
    uint32_t i;
    for (i = 0; i < (d->tbits == 8 ? 256u : 16u); i++) {
        out[i] = clut_read(clut, d->tex0, i, (int)d->tbits, d->texa);
    }
}

void Gs_Circuit(const DispRegs *regs, int k, Circuit *c) { circuit(regs, k, c); }
int Gs_WritePng(const char *path, uint32_t w, uint32_t h, const uint8_t *rgb) { return write_png(path, w, h, rgb); }
const char *Gs_DumpDir(void) { return sDumpDir != NULL && *sDumpDir != 0 ? sDumpDir : NULL; }

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

/* The read circuits' merge (PMODE): circuit 1 over circuit 2 or the background colour, by ALP or circuit 1's alpha,
   each circuit at its own DISPLAY position. Two 64 x 32 PSMCT32 buffers (FBW 1) at FBP 0x100 and 0x140. */
static void selftest_display(void) {
    uint32_t x, y, w, h;
    uint8_t *rgb;
    DispRegs dregs;
    uint64_t disp = 63ull << 32 | 31ull << 44; /* DW 63, DH 31, MAGH / MAGV 0 */

    for (y = 0; y < 32; y++) {
        for (x = 0; x < 64; x++) {
            vram_rw(0x100 << 5, 1, 0, x, y, 1, 0x40000000u | (x < 32 ? 0xC8u : 0x10u)); /* red 200 / 16, alpha 0x40 */
            vram_rw(0x140 << 5, 1, 0, x, y, 1, 0x80006400u);                            /* green 100 */
        }
    }
    PRIV(GS_SMODE2) = 0;
    PRIV(GS_DISPFB1) = 0x100 | 1ull << 9;
    PRIV(GS_DISPFB2) = 0x140 | 1ull << 9;
    PRIV(GS_DISPLAY1) = disp;
    PRIV(GS_DISPLAY2) = disp | 1ull << 12; /* DY 1: circuit 2 one line lower */
    PRIV(GS_BGCOLOR) = 0x00320000u;        /* blue 50 */
    /* both circuits, MMOD 1 (ALP 0x7F), SLBG 0: c1 * 127/255 + c2 * 128/255 */
    PRIV(GS_PMODE) = 3 | 1u << 5 | 0x7Fu << 8;
    disp_regs(&dregs);
    rgb = displayed(&dregs, &w, &h);
    expect(rgb != NULL && w == 64 && h == 32, "display: circuit 1's size");
    if (rgb != NULL) {
        expect(rgb[(1 * 64 + 0) * 3 + 0] == (200 * 127 + 127) / 255 && rgb[(1 * 64 + 0) * 3 + 1] == (100 * 128 + 127) / 255,
               "display: ALP blends circuit 1 over circuit 2");
        expect(rgb[(1 * 64 + 40) * 3 + 0] == (16 * 127 + 127) / 255, "display: ALP blend, second half");
        expect(rgb[0] == (200 * 127 + 127) / 255 && rgb[1] == 0 && rgb[2] == (50 * 128 + 127) / 255,
               "display: background where circuit 2 shows nothing (its DISPLAY.DY)");
        free(rgb);
    }
    /* MMOD 0: circuit 1's own alpha (0x40 = 0.5, 0x80 = 1.0) */
    PRIV(GS_PMODE) = 3;
    disp_regs(&dregs);
    rgb = displayed(&dregs, &w, &h);
    if (rgb != NULL) {
        expect(rgb[(1 * 64 + 0) * 3 + 0] == (200 * 128 + 127) / 255 && rgb[(1 * 64 + 0) * 3 + 1] == (100 * 127 + 127) / 255,
               "display: MMOD 0 takes circuit 1's alpha");
        free(rgb);
    }
    /* SLBG 1: the background colour instead of circuit 2 */
    PRIV(GS_PMODE) = 3 | 1u << 5 | 1u << 7 | 0x80u << 8;
    disp_regs(&dregs);
    rgb = displayed(&dregs, &w, &h);
    if (rgb != NULL) {
        expect(rgb[(5 * 64 + 0) * 3 + 1] == 0 && rgb[(5 * 64 + 0) * 3 + 2] == (50 * 127 + 127) / 255,
               "display: SLBG 1 blends with BGCOLOR");
        free(rgb);
    }
    /* circuit 2 alone: its own picture */
    PRIV(GS_PMODE) = 2;
    disp_regs(&dregs);
    rgb = displayed(&dregs, &w, &h);
    if (rgb != NULL) {
        expect(rgb[(5 * 64 + 3) * 3 + 0] == 0 && rgb[(5 * 64 + 3) * 3 + 1] == 100, "display: circuit 2 alone");
        free(rgb);
    }
    PRIV(GS_PMODE) = 0;
}

int Port_GsVifSelfTest(void); /* gs_vif1.c */

void Port_GsSelfTest(void) {
    if (sThreads < 0) {
        sThreads = 0; /* the checks read GS memory right after drawing */
    }
    tables_init();
    selftest_swizzle();
    selftest_draw();
    selftest_display();
    sFails += Port_GsVifSelfTest();
    memset(sVram, 0, sizeof sVram);
    Gs_Reset();
    if (sFails == 0) {
        printf("ttt: gs self-test OK\n");
    }
    exit(sFails != 0);
}
