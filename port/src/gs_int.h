/* The GS's internals shared by its two back ends: the software reference rasteriser of gs_core.c (and its raster
   threads) and the OpenGL back end of gs_gl.c (T30 batch 3). The front end (GIF, registers, the vertex queue:
   gs_core.c, on the game's thread) turns primitives and operations on GS memory into commands; a back end carries them
   out in order. Integer only, as the rest of the GS path. */
#ifndef PORT_GS_INT_H
#define PORT_GS_INT_H
#include <stdint.h>

typedef struct Vtx {
    int32_t x, y;        /* window coordinates (XYOFFSET taken off), 12.4 */
    uint32_t z;
    uint32_t s, t, q;    /* float bits */
    int32_t u, v;        /* texel coordinates, 10.4 */
    uint32_t rgba;
    uint32_t f;          /* fog coefficient */
} Vtx;

/* What the pixels of one primitive read from the registers, decoded once when the primitive is queued (the front end
   may change the registers while the back end still draws it). The decoding is the one the per-pixel code did. */
typedef struct Draw {
    int ctx;
    uint64_t prim, tex0, texa, fogcol, test, alpha, zbuf, frame;
    uint32_t clutVer;
    int scx0, scx1, scy0, scy1;                      /* SCISSOR, inclusive */
    uint32_t fbp, fbw, fpsm, fbits, fbmsk;           /* FRAME */
    uint32_t zbp, zpsm, zmax;                        /* ZBUF */
    int zwrite, ztest;
    uint32_t ztst;
    int atest;                                       /* TEST: alpha test, destination alpha test */
    uint32_t atst, aref, afail, datm;
    int date;
    int blend, pabe, colclamp, fba;                  /* ALPHA, PABE, COLCLAMP, FBA */
    uint32_t asel, bsel, csel, dsel, afix;
    int tex, fst, fog, gouraud;                      /* PRIM */
    uint32_t tbp, tbw, tpsm, tbits, tfx, tcc;        /* TEX0 */
    int tw, th, lw, lh;
    uint32_t wms, wmt;                               /* CLAMP */
    int minu, maxu, minv, maxv;
    uint32_t ta0, ta1, aem;                          /* TEXA */
    int filter;                                      /* 0 nearest, 1 linear, 2 by the level of detail (TEX1) */
    int32_t lodk;
    int lcm, lodl, mmagLinear, mminLinear;
} Draw;

enum { CMD_POINT, CMD_LINE, CMD_TRIANGLE, CMD_SPRITE, CMD_UPLOAD, CMD_COPY, CMD_CLUT, CMD_SHOW, CMD_BARRIER, CMD_STATE };

/* The display registers at a VBlank (the picture the dump and the window show), and the field the game's flicker
   filter last made a display buffer for (GsField below): fieldOdd 1 when that copy took the frame's odd lines. */
typedef struct DispRegs {
    uint64_t pmode, smode2, bgcolor, dispfb[2], display[2];
    uint32_t fieldFbp, fieldOdd;
} DispRegs;

typedef struct Cmd {
    uint8_t type;
    uint8_t serial;   /* worker 0 alone carries it out, the others wait (operations on GS memory) */
    uint8_t barrier;  /* the workers first wait for each other (the command reads what an earlier one wrote) */
    union {
        struct { Draw d; Vtx v[3]; } prim;
        struct { uint64_t bitbltbuf; uint32_t tx, ty, tw, th, tpos, bytes; uint8_t *data; } upload;
        struct { uint64_t bitbltbuf, trxpos; uint32_t tx, ty, tw, th; } copy;
        struct { uint64_t t0, texclut; } clut;
        struct { DispRegs regs; uint32_t vblank; uint8_t *rgb; uint8_t window; uint64_t ns; } show; /* window: the GL back end's window picture */ /* rgb: the window's picture, else the dump */ /* ns: when the GL window picture was queued (host, monotonic) */
        /* the GL back end at a state save (load 0: GS memory and the CLUT buffer made whole on the CPU, the CLUT
           buffer into clut) or after a load (load 1: the loaded GS memory and CLUT buffer taken), F07 */
        struct { uint16_t *clut; uint8_t load; } state;
    } u;
} Cmd;

/* One read circuit (k = 0, 1): its DISPFB buffer and its DISPLAY area in output pixels / raster lines. */
typedef struct {
    uint32_t fbp, fbw, psm, dbx, dby, x0, y0, width, lines;
} Circuit;


/* ---- gs_core.c, for the GL back end (all integer; the GS memory is gs_core.c's 4 MB) ---- */
uint32_t *Gs_Vram(void);                                         /* the 4 MB of GS memory, 1M words */
int Gs_PsmBits(uint32_t psm);
/* The position of pixel (x, y) inside its page for each layout (tables_init's sIn32, sIn16 / 16S, sIn8, sIn4):
   k = 0: 64 x 32, 1: 64 x 64 (16), 2: 64 x 64 (16S), 3: 128 x 64, 4: 128 x 128. */
const uint16_t *Gs_PageTable(int k);
void Gs_PagesOf(uint32_t set[16], uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x0, uint32_t x1, uint32_t y0, uint32_t y1);
void Gs_TexRange(uint32_t wm, int size, int mn, int mx, uint32_t *lo, uint32_t *hi);
void Gs_CpuUpload(const Cmd *c);                                 /* a CMD_UPLOAD into GS memory (frees the data) */
void Gs_CpuCopy(const Cmd *c);                                   /* a CMD_COPY inside GS memory */
void Gs_ClutFill(uint16_t clut[512], uint64_t t0, uint64_t texclut);
void Gs_Palette(const uint16_t clut[512], const Draw *d, uint32_t out[256]); /* the CLUT expanded through TEXA */
void Gs_Circuit(const DispRegs *regs, int k, Circuit *c);
/* The field placement (docs/port/video.md, Field placement): the game's flicker filter copies its 448-line frame into
   a 224-line display buffer from the even lines in one field and the odd lines in the other, and a TV shows the odd
   field half a line lower. Gs_FieldSprite watches the copy's sprites (in drawing order), Gs_FieldRegs puts the last
   copy's field into a VBlank's registers (reset: the next copy starts afresh), Gs_FieldShift is 1 when circuit c of r
   is to be shown one raster line lower. */
typedef struct GsField { uint32_t fbp; int32_t phase; int fresh; } GsField;
void Gs_FieldSprite(GsField *f, const Draw *d, const Vtx *v);
void Gs_FieldRegs(GsField *f, DispRegs *r, int reset);
uint32_t Gs_FieldShift(const DispRegs *r, const Circuit *c);
int Gs_WritePng(const char *path, uint32_t w, uint32_t h, const uint8_t *rgb);
const char *Gs_DumpDir(void);
/* The GL back end's consumer (the window thread): carries out the commands queued so far, waiting up to `ms`
   milliseconds for the first one; returns how many it ran. */
int Gs_GlPump(int ms);
extern uint16_t gGsFrameAtVb[256]; /* the game's frame count at each VBlank (by its number & 255; window runs only) */
extern int gGsGlSkip; /* while a window picture runs: a newer one is queued (the GL back end draws it, shows only the newer) */

/* ---- the GS thread (gs_async.c) ---- */
int Gs_AsyncVBlank(uint32_t vblank, const DispRegs *regs);   /* queues a VBlank's picture (0: no GS thread) */
void Gs_VBlankRun(uint32_t vblank, const DispRegs *regs);    /* gs_core.c: a VBlank's picture, dumps and traces */
int Gs_AsyncRegWrite(uint64_t csrBits, uint64_t sigMask, uint64_t sigValue); /* SIGNAL / FINISH / LABEL kept (1) */
void Gs_AsyncRegs(void);
uint64_t Gs_AsyncWaitNs(void);
uint32_t Gs_AsyncCpuMs(int k); /* the GS (0) / GIF (1) thread's CPU time in ms (TTT_GS_STATS) */
uint32_t Gs_GlCpuMs(void);    /* the window thread's, with the GL back end */
int Gs_ThreadsWanted(void);

/* ---- gs_gl.c ---- */
int Gl_Init(void *(*getproc)(const char *name), char *why, int whylen); /* on the thread with the GL context */
int Gl_Redisplay(void); /* the last window picture again, presented (window thread; 0: none yet) */
int Gl_AspectMargin(const char *aspect); /* video.aspect ("4:3", "16:9", "21:9"): GS pixels added each side of 640 x 448 */
void Gl_Exec(const Cmd *c);                                     /* one command, in order */
void Gl_Report(void);
int Gl_SelfTest(void);                                         /* TTT_SELFTEST=gl: fails */

/* ---- gs_texpack.c: texture dumps and replacements (T39, docs/port/textures.md), for gs_gl.c (window thread) ---- */
typedef struct TpName {
    uint64_t tex, clut; /* XXH3 of the texture's data and of its CLUT (0: not indexed) */
    uint32_t bits;      /* PSM | TW << 6 | TH << 10 | TEXA (16 / 24 bits) */
    uint32_t rw, rh;    /* the CLAMP region's width / height (PCSX2's "-r<w>x<h>"), 0: the whole TW / TH */
    uint32_t x0, y0, w, h; /* not in the name: the texels named, [x0, x0 + w) x [y0, y0 + h) */
} TpName;
/* PCSX2's name of the texture `d` samples from GS memory `vram` (pal: its 16 / 256 CLUT entries as R G B A, NULL when
   the CPU does not know them); 0 when no pack can name it. */
int Tp_Name(const uint32_t *vram, const Draw *d, const uint32_t *pal, TpName *n);
void Tp_Rect(const Draw *d, TpName *n);  /* only the texels a name covers (x0 y0 w h, rw rh) */
void Tp_Settings(int dump, int replace);  /* video.texture_dump / video.texture_replace, at each picture */
void Tp_Dump(const uint32_t *vram, const Draw *d, const uint32_t *pal, const TpName *n); /* queued once per name */
int Tp_Find(const TpName *n);             /* the replacement of that name (index), -1 none, -2 not known yet */
int Tp_Count(void);                       /* replacements found (0 until the folder was read) */
/* Replacement i's pixels (RGBA8, w x h, the caller frees them with Tp_Free): 1 taken, 0 not decoded yet, -1 none. */
int Tp_Take(int i, uint8_t **px, int *w, int *h);
void Tp_Free(uint8_t *px);
void Tp_Report(uint32_t replacedDraws, uint32_t named, uint32_t gpuUploads); /* waits for the queued dumps */
uint64_t Tp_Hash(const void *p, uint32_t n); /* XXH3-64 */
int Tp_SelfTest(void);                    /* TTT_SELFTEST=texpack: fails */

/* ---- plat_window.c, for gs_gl.c ---- */
int Port_WindowGl(void);                                         /* the window draws with the GL back end */
void Port_WindowGlSize(int *w, int *h);                          /* the window's drawable size in pixels */
void Port_WindowGlSwap(void);                                    /* present the default framebuffer */

#endif
