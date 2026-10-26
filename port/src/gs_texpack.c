/*
 * Texture packs (T39, docs/port/textures.md): the GL back end (gs_gl.c) names each texture a primitive samples the
 * way PCSX2 names its dumps and replacements, writes new ones as PNG (dump mode) and draws a PNG of the same name
 * instead (replace mode).
 *
 *   <textures>/SLUS-20001/dumps/<name>.png          written here (dump mode)
 *   <textures>/SLUS-20001/replacements/**<name>.png  read here, any size, any sub-folder (replace mode)
 *
 * <textures> is video.texture_dir (TTT_TEXTURES), else the repository's textures/ when the program is build/port/ttt
 * of a checkout, else textures/ in the player's data folder (plat_paths.c, T43: beside the program when portable).
 * A name is PCSX2's (GSTextureReplacements.cpp, GSTextureCache.cpp HashTextureLevel / HashCacheKey::Create):
 * `<tex>-<bits>` or, for an indexed texture, `<tex>-<clut>-<bits>`, with <tex> and <clut> XXH3-64 in lower-case hex without leading zeros and <bits> eight hex digits:
 *
 *   - tex: XXH3-64 of the texture's data at level 0, TW x TH texels from TBP0 / TBW / PSM. PSMCT32, PSMT8 and PSMT4
 *     textures at least one block wide and high hash their 256-byte blocks of GS memory as stored, block rows top to
 *     bottom, left to right; the others (PSMCT24, PSMCT16 / 16S, PSMT8H / 4HL / 4HH, or smaller than a block) hash
 *     the texels expanded row by row: 4 bytes R G B A (16 / 24 bits through TEXA) or, indexed, one index byte.
 *   - clut: XXH3-64 of the 16 or 256 CLUT entries the texture uses, as 32-bit R G B A (16-bit entries through TEXA).
 *   - bits: PSM | TW << 6 | TH << 10 | (16 / 24-bit formats only) TA0 << 15 | AEM << 23 | TA1 << 24.
 *
 * A dump is the texture as 8-bit RGBA (indexed texels through the CLUT), alpha as the GS stores it (0x80 = 1.0); a
 * replacement is read the same way (its alpha 128 is opaque for the game's blending). Not named, so never dumped or
 * replaced: textures in a render target's pages (the GPU drew them), the depth formats, TW / TH above 1024, region
 * clamp / repeat narrower than the texture (PCSX2's "-r<w>x<h>" names), CLUTs the GPU loaded from a target.
 *
 * Threads: everything here runs on the window thread, except one worker (started on the first need) that scans the
 * replacements folder (after the enabled mods' textures/ folders, T49, plat_mods.c: the first file of a name wins, so
 * a mod higher in the load order wins over a lower one and over the pack), decodes every replacement it found
 * (stb_image, PNG only; a name already found earlier is not decoded) and compresses and writes the dumps
 * (stb_image_write), so a picture never waits for the disk. Nothing of the game reads any of it: the GS memory is
 * only read (gs_core.c's CPU copy, which the window thread owns in GL mode). Integer only, like the rest of the port.
 * All C library calls go through the GOT (port_host.h; toolchain.py NOPLT_FILES): no new PLT entry.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_host.h"
#include "plat_os.h"
#include "gs_int.h"
#include "plat_settings.h"
#include "plat_mods.h"

/* C library calls through the GOT: no new PLT entry (port_host.h) */
PORT_NOPLT(malloc);
PORT_NOPLT(calloc);
PORT_NOPLT(realloc);
PORT_NOPLT(free);
PORT_NOPLT(memcpy);
PORT_NOPLT(memmove);
PORT_NOPLT(memset);
PORT_NOPLT(memcmp);
PORT_NOPLT(strlen);
PORT_NOPLT(strcmp);
PORT_NOPLT(strrchr);
PORT_NOPLT(strchr);
PORT_NOPLT(strcat);
PORT_NOPLT(strstr);
PORT_NOPLT(abs);
PORT_NOPLT(snprintf);
PORT_NOPLT(printf);
PORT_NOPLT(fopen);
PORT_NOPLT(fread);
PORT_NOPLT(fwrite);
PORT_NOPLT(fclose);
PORT_NOPLT(fseek);
PORT_NOPLT(ftell);
PORT_NOPLT(fflush);
PORT_NOPLT(rename);
PORT_NOPLT(readlink);
PORT_NOPLT(getpid);
PORT_NOPLT(clock_gettime);

#define XXH_INLINE_ALL
#define XXH_NO_STDLIB
#define XXH_FORCE_MEMORY_ACCESS 2 /* x86: unaligned loads are fine */
#include "../../third_party/xxhash/xxhash.h"

/* stb (third_party/stb, public domain / MIT): PNG only, from memory, every function static (the unused ones, HDR and
   JPEG with their float code, are dropped by the compiler) */
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)
#define STBI_MAX_DIMENSIONS 8192
#include "../../third_party/stb/stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#include "../../third_party/stb/stb_image_write.h"

#define SERIAL "SLUS-20001"

/* ------------------------------------------------------------------------------------------------ naming */

static int psm_ok(uint32_t psm) {
    switch (psm) {
    case 0x00: case 0x01: case 0x02: case 0x0A: case 0x13: case 0x14: case 0x1B: case 0x24: case 0x2C: return 1;
    default: return 0;
    }
}

/* gs_core.c's page tables (Gs_PageTable), fetched once */
static const uint16_t *sTab[5];

static const uint16_t *tab(int k) {
    if (sTab[k] == NULL) {
        sTab[k] = Gs_PageTable(k);
    }
    return sTab[k];
}

/* One texel as stored (gs_gl.c's gs_read for the colour formats): 32 / 24 / 16 bits, or an index. */
static inline uint32_t rd(const uint32_t *vram, uint32_t bp, uint32_t bw, uint32_t psm, uint32_t x, uint32_t y) {
    const uint16_t *t;
    uint32_t a, w;
    if (bw == 0) {
        bw = 1;
    }
    x &= 0x7FF;
    y &= 0x7FF;
    switch (psm) {
    case 0x02: case 0x0A:
        t = tab(psm == 0x0A ? 2 : 1);
        a = ((bp + (y >> 6) * 32 * bw + (x >> 6) * 32) << 7) + t[(y & 63) * 64 + (x & 63)];
        return (vram[(a >> 1) & 0xFFFFF] >> ((a & 1) * 16)) & 0xFFFF;
    case 0x13:
        t = tab(3);
        a = ((bp + (y >> 6) * 32 * (bw >> 1) + (x >> 7) * 32) << 8) + t[(y & 63) * 128 + (x & 127)];
        return (vram[(a >> 2) & 0xFFFFF] >> ((a & 3) * 8)) & 0xFF;
    case 0x14:
        t = tab(4);
        a = ((bp + (y >> 7) * 32 * (bw >> 1) + (x >> 7) * 32) << 9) + t[(y & 127) * 128 + (x & 127)];
        return (vram[(a >> 3) & 0xFFFFF] >> ((a & 7) * 4)) & 0xF;
    }
    t = tab(0);
    w = vram[(((bp + (y >> 5) * 32 * bw + (x >> 6) * 32) << 6) + t[(y & 31) * 64 + (x & 63)]) & 0xFFFFF];
    switch (psm) {
    case 0x1B: return w >> 24;
    case 0x24: return (w >> 24) & 15;
    case 0x2C: return w >> 28;
    default: return w;
    }
}

/* A stored colour of 32 / 24 / 16 bits as R G B A (gs_core.c's expand, PCSX2's Expand16 / ExpandBlock24). */
static uint32_t expand(uint32_t c, uint32_t psm, uint32_t ta0, uint32_t ta1, uint32_t aem) {
    uint32_t a;
    if (psm == 0x00) {
        return c;
    }
    if (psm == 0x01) {
        c &= 0xFFFFFF;
        return c | (aem && c == 0 ? 0 : ta0) << 24;
    }
    a = (c & 0x8000) ? ta1 : (aem && (c & 0x7FFF) == 0 ? 0 : ta0);
    return (c & 31) << 3 | ((c >> 5) & 31) << 11 | ((c >> 10) & 31) << 19 | a << 24;
}

static uint8_t *sScratch; /* window thread */
static size_t sScratchN;

static uint8_t *scratch(size_t n) {
    if (n > sScratchN) {
        uint8_t *p = realloc(sScratch, n);
        if (p == NULL) {
            return NULL;
        }
        sScratch = p;
        sScratchN = n;
    }
    return sScratch;
}

/* PCSX2's SourceRegion::Create on one axis: the texels [*lo, *hi) CLAMP confines the texture to, when PCSX2 names that
   region instead of the whole TW / TH (region clamp narrower than the size or past it; region repeat). 0: none. */
static int region(uint32_t wm, uint32_t mn, uint32_t mx, uint32_t size, uint32_t *lo, uint32_t *hi) {
    if (wm == 2 && mx >= mn && (mx - mn + 1 < size || mx >= size)) {
        *lo = mn;
        *hi = mx + 1;
        return 1;
    }
    if (wm == 3 && mn != 0) {
        uint32_t r = ((mn | mx) - mx) + 1;
        if (r < size || (mx != 0 && r <= size)) {
            *lo = mx;
            *hi = (mn | mx) + 1;
            return 1;
        }
    }
    return 0;
}

/* The texel (x, y) as 4 bytes R G B A (16 / 24 bits through TEXA), or its index (indexed: the format is). */
static inline uint32_t texel(const uint32_t *vram, const Draw *d, int indexed, uint32_t x, uint32_t y) {
    uint32_t c = rd(vram, d->tbp, d->tbw, d->tpsm, x, y);
    return indexed ? c : expand(c, d->tpsm, d->ta0 & 255, d->ta1 & 255, d->aem & 1);
}

/* XXH3 of the texture's data, PCSX2's HashTextureLevel (see the top): the texels [x0, x0 + w) x [y0, y0 + h). */
static int tex_hash(const uint32_t *vram, const Draw *d, const TpName *n, uint64_t *out) {
    uint32_t psm = d->tpsm, bsx, bsy, x, y, w = n->w, h = n->h, bits = (uint32_t)Gs_PsmBits(psm);
    uint8_t *buf;

    bsx = psm == 0x14 ? 32 : (psm == 0x13 || psm == 0x02 || psm == 0x0A) ? 16 : 8;
    bsy = psm == 0x13 || psm == 0x14 ? 16 : 8;
    if ((psm == 0x00 || psm == 0x13 || psm == 0x14) && w >= bsx && h >= bsy && n->rw == 0 && n->y0 == 0) {
        /* the blocks as stored, rows of blocks top to bottom (the last one whole), from the page tables' block numbers */
        uint32_t nbx = w / bsx, nby = (h + bsy - 1) / bsy, bw = d->tbw == 0 ? 1 : d->tbw, k = 0, tbp = d->tbp;
        const uint16_t *t = tab(psm == 0x00 ? 0 : psm == 0x13 ? 3 : 4);
        buf = scratch((size_t)nbx * nby * 256);
        if (buf == NULL) {
            return 0;
        }
        for (y = 0; y < nby; y++) {
            for (x = 0; x < nbx; x++, k++) {
                uint32_t blk;
                if (psm == 0x00) {
                    blk = tbp + ((y >> 2) * bw + (x >> 3)) * 32 + (t[((y * 8) & 31) * 64 + ((x * 8) & 63)] >> 6);
                } else if (psm == 0x13) {
                    blk = tbp + ((y >> 2) * (bw >> 1) + (x >> 3)) * 32 + (t[((y * 16) & 63) * 128 + ((x * 16) & 127)] >> 8);
                } else {
                    blk = tbp + ((y >> 3) * (bw >> 1) + (x >> 2)) * 32 + (t[((y * 16) & 127) * 128 + ((x * 32) & 127)] >> 9);
                }
                memcpy(buf + (size_t)k * 256, vram + (size_t)(blk & 0x3FFF) * 64, 256);
            }
        }
        *out = XXH3_64bits(buf, (size_t)nbx * nby * 256);
        return 1;
    }
    /* expanded: R G B A per texel, or one index byte */
    buf = scratch((size_t)w * h * 4);
    if (buf == NULL) {
        return 0;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint32_t c = texel(vram, d, bits <= 8, n->x0 + x, n->y0 + y);
            if (bits <= 8) {
                buf[y * w + x] = (uint8_t)c;
            } else {
                memcpy(buf + ((size_t)y * w + x) * 4, &c, 4);
            }
        }
    }
    *out = XXH3_64bits(buf, (size_t)w * h * (bits <= 8 ? 1 : 4));
    return 1;
}

void Tp_Rect(const Draw *d, TpName *n) {
    uint32_t lw = d->lw > 10 ? 10 : (uint32_t)d->lw, lh = d->lh > 10 ? 10 : (uint32_t)d->lh, lo, hi;
    memset(n, 0, sizeof *n);
    n->w = 1u << lw;
    n->h = 1u << lh;
    if (region(d->wms, (uint32_t)d->minu, (uint32_t)d->maxu, 1u << lw, &lo, &hi)) {
        n->x0 = lo;
        n->w = n->rw = hi - lo;
    }
    if (region(d->wmt, (uint32_t)d->minv, (uint32_t)d->maxv, 1u << lh, &lo, &hi)) {
        n->y0 = lo;
        n->h = n->rh = hi - lo;
    }
}

int Tp_Name(const uint32_t *vram, const Draw *d, const uint32_t *pal, TpName *n) {
    uint32_t psm = d->tpsm, lw = (uint32_t)d->lw, lh = (uint32_t)d->lh, bits = (uint32_t)Gs_PsmBits(psm);

    if (!psm_ok(psm) || lw > 10 || lh > 10 || (bits <= 8 && pal == NULL)) {
        return 0;
    }
    Tp_Rect(d, n);
    n->bits = psm | lw << 6 | lh << 10;
    if (bits == 16 || bits == 24) {
        n->bits |= (d->ta0 & 255) << 15 | (d->aem & 1) << 23 | (d->ta1 & 255) << 24;
    }
    n->clut = bits <= 8 ? XXH3_64bits(pal, (bits == 8 ? 256 : 16) * 4) : 0;
    return tex_hash(vram, d, n, &n->tex);
}

uint64_t Tp_Hash(const void *p, uint32_t n) { return XXH3_64bits(p, n); }

static void name_str(const TpName *n, char *out, size_t len) {
    char r[32] = "";
    if (n->rw != 0 || n->rh != 0) {
        snprintf(r, sizeof r, "r%ux%u-", (unsigned)n->rw, (unsigned)n->rh);
    }
    if (Gs_PsmBits(n->bits & 63) <= 8) {
        snprintf(out, len, "%llx-%llx-%s%08x", (unsigned long long)n->tex, (unsigned long long)n->clut, r, (unsigned)n->bits);
    } else {
        snprintf(out, len, "%llx-%s%08x", (unsigned long long)n->tex, r, (unsigned)n->bits);
    }
}

/* Hex digits (any case) up to `stop`: their number, 0 when there are none, too many or another character. */
static int hex(const char *s, char stop, int max, uint64_t *v) {
    int k = 0;
    *v = 0;
    for (; s[k] != stop; k++) {
        char c = s[k];
        uint32_t d = c >= '0' && c <= '9' ? (uint32_t)(c - '0') : c >= 'a' && c <= 'f' ? (uint32_t)(c - 'a' + 10) :
                     c >= 'A' && c <= 'F' ? (uint32_t)(c - 'A' + 10) : 99;
        if (d == 99 || k == max) {
            return 0;
        }
        *v = *v << 4 | d;
    }
    return k;
}

/* Decimal digits up to `stop`, the same way. */
static int dec(const char *s, char stop, uint32_t *v) {
    int k = 0;
    *v = 0;
    for (; s[k] != stop; k++) {
        if (s[k] < '0' || s[k] > '9' || k == 5) {
            return 0;
        }
        *v = *v * 10 + (uint32_t)(s[k] - '0');
    }
    return k;
}

/* PCSX2's current name forms (`<tex>[-<clut>][-r<w>x<h>]-<bits>.png`, extension in any case); 0 for anything else
   (older region names, mip levels, DDS). The CLUT part must be there exactly for the indexed formats. */
static int parse_name(const char *f, TpName *n) {
    uint64_t v[3];
    const char *s = f, *end;
    size_t len = strlen(f);
    int parts = 0, k;

    if (len < 5) {
        return 0;
    }
    end = f + len - 4;
    if (end[0] != '.' || (end[1] | 32) != 'p' || (end[2] | 32) != 'n' || (end[3] | 32) != 'g') {
        return 0;
    }
    memset(n, 0, sizeof *n);
    for (;;) {
        if (*s == 'r' && parts > 0) { /* r<w>x<h>- */
            uint32_t a, b;
            k = dec(s + 1, 'x', &a);
            if (k == 0 || (k = dec(s + 1 + k + 1, '-', &b), k == 0)) {
                return 0;
            }
            s = strchr(s, '-') + 1;
            n->rw = a;
            n->rh = b;
            continue;
        }
        k = hex(s, s + 8 == end ? '.' : '-', 16, &v[parts]);
        if (k == 0) {
            return 0;
        }
        if (s + k == end) {
            if (k != 8 || parts == 0 || parts > 2) {
                return 0;
            }
            n->bits = (uint32_t)v[parts];
            n->tex = v[0];
            n->clut = parts == 2 ? v[1] : 0;
            return (parts == 2) == (Gs_PsmBits(n->bits & 63) <= 8);
        }
        if (parts == 2) {
            return 0;
        }
        parts++;
        s += k + 1;
    }
}

/* ------------------------------------------------------------------------------------------------ folders */

static char sRoot[1024], sGame[1100];

void Port_UserFolder(const char *leaf, char *out, int n); /* plat_paths.c */

static void folders(void) {
    if (sGame[0]) {
        return;
    }
    Settings_GetStr("video.texture_dir", sRoot, sizeof sRoot);
    if (!sRoot[0]) {
        Port_UserFolder("textures", sRoot, sizeof sRoot);
    }
    snprintf(sGame, sizeof sGame, "%s/" SERIAL, sRoot);
}

const char *Tp_Folder(void) {
    folders();
    return sGame;
}

/* ------------------------------------------------------------------------------------------------ the worker */

typedef struct Repl {
    TpName n;
    char *path;
    uint8_t *px;     /* decoded RGBA8 until the GL back end took it */
    int w, h;
    int state;       /* 0 waiting, 1 decoded, 2 failed, 3 taken (or hidden by an earlier file of the name) */
    int src;         /* where it was found: mod folder 0.. in load order, then the texture pack */
} Repl;

typedef struct Job {
    struct Job *next;
    char path[1200];
    int w, h;
    uint8_t *rgba;
} Job;

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sCond = PTHREAD_COND_INITIALIZER;
static int sWorker;            /* 1 started */
static int sScanWanted, sScanned; /* the replacements folder: asked for / read (sRepl stays as it is then) */
static Job *sHead, *sTail;
static int sBusy;              /* the worker is writing a dump */
static Repl *sRepl;            /* sorted by name; the window thread reads it once sScanned */
static int sNRepl, sCapRepl, sDecoded, sFailed, sSkippedFiles;
static uint32_t sDumped, sDumpFailed, sDumpExisting;
static int sScanSrc, sScanMods;  /* the folder being read (as Repl.src), the mod folders before the pack's */
static int sScanPack = 1;        /* the pack's replacements/ too (video.texture_replace), not only the mods' */

static void add_repl(const char *path, const TpName *n) {
    if (sNRepl == sCapRepl) {
        int cap = sCapRepl ? sCapRepl * 2 : 64;
        Repl *r = realloc(sRepl, (size_t)cap * sizeof *r);
        if (r == NULL) {
            return;
        }
        sRepl = r;
        sCapRepl = cap;
    }
    memset(&sRepl[sNRepl], 0, sizeof sRepl[0]);
    sRepl[sNRepl].n = *n;
    sRepl[sNRepl].src = sScanSrc;
    sRepl[sNRepl].path = malloc(strlen(path) + 1);
    if (sRepl[sNRepl].path == NULL) {
        return;
    }
    memcpy(sRepl[sNRepl].path, path, strlen(path) + 1);
    sNRepl++;
}

static void scan(const char *dir, int depth) {
    PlatOsDir *d = PlatOs_DirOpen(dir);
    const char *e;
    char path[1200];
    if (d == NULL) {
        return;
    }
    while ((e = PlatOs_DirNext(d)) != NULL) {
        PlatOsStat st;
        TpName n;
        if (e[0] == '.' || (sScanSrc < sScanMods && strcmp(e, "dumps") == 0)) {
            continue; /* a mod's textures/ may hold a whole pack: its dumps are not replacements */
        }
        snprintf(path, sizeof path, "%s/%s", dir, e);
        if (PlatOs_Stat(path, &st) != 0) {
            continue;
        }
        if (st.dir) {
            if (depth < 8) {
                scan(path, depth + 1);
            }
        } else if (parse_name(e, &n)) {
            add_repl(path, &n);
        } else {
            sSkippedFiles++;
        }
    }
    PlatOs_DirClose(d);
}

static int name_cmp(const TpName *a, const TpName *b) {
    if (a->tex != b->tex) {
        return a->tex < b->tex ? -1 : 1;
    }
    if (a->clut != b->clut) {
        return a->clut < b->clut ? -1 : 1;
    }
    if (a->bits != b->bits) {
        return a->bits < b->bits ? -1 : 1;
    }
    if (a->rw != b->rw) {
        return a->rw < b->rw ? -1 : 1;
    }
    return a->rh < b->rh ? -1 : a->rh > b->rh;
}

static uint8_t *read_file(const char *path, long *len) {
    FILE *f = fopen(path, "rb");
    uint8_t *b = NULL;
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) == 0 && (*len = ftell(f)) > 0 && *len < (64l << 20) && fseek(f, 0, SEEK_SET) == 0 &&
        (b = malloc((size_t)*len)) != NULL && fread(b, 1, (size_t)*len, f) != (size_t)*len) {
        free(b);
        b = NULL;
    }
    fclose(f);
    return b;
}

static void write_dump(Job *j) {
    int len = 0;
    unsigned char *png = stbi_write_png_to_mem(j->rgba, j->w * 4, j->w, j->h, 4, &len);
    char tmp[1300];
    FILE *f;
    int ok = 0;
    if (png != NULL) {
        snprintf(tmp, sizeof tmp, "%s.tmp%d", j->path, (int)getpid());
        f = fopen(tmp, "wb");
        if (f != NULL) {
            ok = fwrite(png, 1, (size_t)len, f) == (size_t)len;
            ok &= fclose(f) == 0;
            ok = ok && PlatOs_Replace(tmp, j->path) == 0; /* a dump is whole or absent */
        }
        STBIW_FREE(png);
    }
    pthread_mutex_lock(&sLock);
    if (ok) {
        sDumped++;
    } else {
        sDumpFailed++;
    }
    pthread_mutex_unlock(&sLock);
}

static void *worker(void *arg) {
    (void)arg;
    pthread_mutex_lock(&sLock);
    for (;;) {
        if (sScanWanted && !sScanned) {
            static char mods[MODS_MAX][1100];
            static int modIndex[MODS_MAX], modFiles[MODS_MAX + 1], modHidden[MODS_MAX + 1];
            char dir[1200];
            int i, n, m;
            pthread_mutex_unlock(&sLock);
            /* sRepl is the worker's until sScanned: the mods' folders first (T49), then the pack's */
            sScanMods = Mods_TextureDirs(mods, modIndex, MODS_MAX);
            for (m = 0; m < sScanMods; m++) {
                sScanSrc = m;
                scan(mods[m], 0);
            }
            sScanSrc = sScanMods;
            snprintf(dir, sizeof dir, "%s/replacements", sGame);
            if (sScanPack) {
                scan(dir, 0);
            }
            /* sorted by name, so the window thread finds one by binary search; equal names: the first stays */
            for (i = 1; i < sNRepl; i++) {
                Repl r = sRepl[i];
                int k = i - 1;
                while (k >= 0 && name_cmp(&sRepl[k].n, &r.n) > 0) {
                    sRepl[k + 1] = sRepl[k];
                    k--;
                }
                sRepl[k + 1] = r;
            }
            /* a name found again further down the order is hidden: never decoded, counted for the Mods page */
            memset(modFiles, 0, sizeof modFiles);
            memset(modHidden, 0, sizeof modHidden);
            for (i = 0; i < sNRepl; i++) {
                modFiles[sRepl[i].src]++;
                if (i > 0 && name_cmp(&sRepl[i - 1].n, &sRepl[i].n) == 0) {
                    sRepl[i].state = 3;
                    modHidden[sRepl[i].src]++;
                }
            }
            for (m = 0, n = 0; m <= sScanMods; m++) {
                n += modHidden[m];
                if (m < sScanMods) {
                    Mods_NoteTextures(modIndex[m], modFiles[m], modHidden[m]);
                }
            }
            if (sScanMods > 0) {
                printf("ttt: textures: mods: %d mod folder%s, %d replacements from mods, %d names hidden by an earlier "
                       "file\n", sScanMods, sScanMods == 1 ? "" : "s", sNRepl - modFiles[sScanMods], n);
            }
            pthread_mutex_lock(&sLock);
            sScanned = 1;
            n = sNRepl;
            pthread_mutex_unlock(&sLock);
            /* every replacement decoded now, in the background: ready when the game first draws its texture */
            for (i = 0; i < n; i++) {
                long len = 0;
                uint8_t *file;
                if (sRepl[i].state == 3) {
                    continue;
                }
                file = read_file(sRepl[i].path, &len);
                int w = 0, h = 0, c = 0;
                uint8_t *px = file != NULL ? stbi_load_from_memory(file, (int)len, &w, &h, &c, 4) : NULL;
                free(file);
                pthread_mutex_lock(&sLock);
                sRepl[i].px = px;
                sRepl[i].w = w;
                sRepl[i].h = h;
                sRepl[i].state = px != NULL ? 1 : 2;
                sDecoded += px != NULL;
                sFailed += px == NULL;
                pthread_mutex_unlock(&sLock);
            }
            pthread_mutex_lock(&sLock);
            continue;
        }
        if (sHead != NULL) {
            Job *j = sHead;
            sHead = j->next;
            if (sHead == NULL) {
                sTail = NULL;
            }
            sBusy = 1;
            pthread_mutex_unlock(&sLock);
            write_dump(j);
            free(j->rgba);
            free(j);
            pthread_mutex_lock(&sLock);
            sBusy = 0;
            pthread_cond_broadcast(&sCond);
            continue;
        }
        pthread_cond_wait(&sCond, &sLock);
    }
    return NULL;
}

/* With sLock held. */
static void start_worker(void) {
    pthread_t t;
    if (!sWorker && pthread_create(&t, NULL, worker, NULL) == 0) {
        pthread_detach(t);
        sWorker = 1;
    }
}

/* ------------------------------------------------------------------------------------------------ the GL back end's side */

static int sDump, sReplace;
/* names dumped or found on the disk in this run: open addressing on (tex ^ clut ^ bits), grown at half full */
static TpName *sSeen;
static uint32_t sSeenCap, sSeenN;

static int seen_add(const TpName *n) {
    uint32_t i, m;
    if (sSeenN * 2 >= sSeenCap) {
        uint32_t cap = sSeenCap ? sSeenCap * 2 : 1024, k;
        TpName *t = calloc(cap, sizeof *t);
        if (t == NULL) {
            return 0;
        }
        for (k = 0; k < sSeenCap; k++) {
            if (sSeen[k].bits != 0 || sSeen[k].tex != 0) {
                TpName *o = &sSeen[k];
                for (i = (uint32_t)(o->tex ^ o->clut ^ o->bits) & (cap - 1); t[i].bits != 0 || t[i].tex != 0; i = (i + 1) & (cap - 1)) {
                }
                t[i] = *o;
            }
        }
        free(sSeen);
        sSeen = t;
        sSeenCap = cap;
    }
    m = sSeenCap - 1;
    for (i = (uint32_t)(n->tex ^ n->clut ^ n->bits) & m; sSeen[i].bits != 0 || sSeen[i].tex != 0; i = (i + 1) & m) {
        if (name_cmp(&sSeen[i], n) == 0) {
            return 0;
        }
    }
    sSeen[i] = *n;
    sSeenN++;
    return 1;
}

/* replace: 1 the pack and the mods' textures, 2 the mods' only (video.texture_replace off, T49). */
void Tp_Settings(int dump, int replace) {
    sDump = dump;
    sReplace = replace != 0;
    if (!dump && !replace) {
        return;
    }
    folders();
    pthread_mutex_lock(&sLock);
    if (replace && !sScanWanted) {
        sScanPack = replace == 1;
        sScanWanted = 1; /* read once a run: new files are seen at the next start */
        start_worker();
        pthread_cond_broadcast(&sCond);
    }
    pthread_mutex_unlock(&sLock);
    if (dump) {
        static int made;
        if (!made) {
            char dir[1200];
            made = 1;
            PlatOs_MkDir(sRoot, 0755);
            PlatOs_MkDir(sGame, 0755);
            snprintf(dir, sizeof dir, "%s/dumps", sGame);
            PlatOs_MkDir(dir, 0755);
            snprintf(dir, sizeof dir, "%s/replacements", sGame);
            PlatOs_MkDir(dir, 0755);
        }
    }
}

void Tp_Dump(const uint32_t *vram, const Draw *d, const uint32_t *pal, const TpName *n) {
    uint32_t x, y;
    int indexed = Gs_PsmBits(d->tpsm) <= 8;
    char name[80];
    Job *j;

    if (!sDump || Tp_Find(n) >= 0 || !seen_add(n)) {
        return; /* replaced or dumped already */
    }
    j = malloc(sizeof *j);
    if (j == NULL) {
        return;
    }
    name_str(n, name, sizeof name);
    snprintf(j->path, sizeof j->path, "%s/dumps/%s.png", sGame, name);
    if (PlatOs_Exists(j->path)) {
        free(j);
        sDumpExisting++;
        return;
    }
    j->w = (int)n->w;
    j->h = (int)n->h;
    j->next = NULL;
    j->rgba = malloc((size_t)n->w * n->h * 4);
    if (j->rgba == NULL) {
        free(j);
        return;
    }
    for (y = 0; y < n->h; y++) {
        for (x = 0; x < n->w; x++) {
            uint32_t c = texel(vram, d, indexed, n->x0 + x, n->y0 + y);
            c = indexed ? pal[c] : c;
            memcpy(j->rgba + ((size_t)y * n->w + x) * 4, &c, 4);
        }
    }
    pthread_mutex_lock(&sLock);
    if (sTail != NULL) {
        sTail->next = j;
    } else {
        sHead = j;
    }
    sTail = j;
    start_worker();
    pthread_cond_broadcast(&sCond);
    pthread_mutex_unlock(&sLock);
}

int Tp_Find(const TpName *n) {
    int lo = 0, hi, scanned;
    pthread_mutex_lock(&sLock);
    scanned = sScanned;
    pthread_mutex_unlock(&sLock);
    if (!sReplace) {
        return -1;
    }
    if (!scanned) {
        return -2; /* the folder is still being read */
    }
    hi = sNRepl - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = name_cmp(&sRepl[mid].n, n);
        if (c == 0) {
            while (mid > 0 && name_cmp(&sRepl[mid - 1].n, n) == 0) {
                mid--;
            }
            return mid;
        }
        if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1;
}

int Tp_Count(void) {
    int n;
    pthread_mutex_lock(&sLock);
    n = sScanned ? sNRepl : 0;
    pthread_mutex_unlock(&sLock);
    return n;
}

int Tp_Take(int i, uint8_t **px, int *w, int *h) {
    int st;
    pthread_mutex_lock(&sLock);
    st = i >= 0 && i < sNRepl ? sRepl[i].state : 2;
    if (st == 1) {
        *px = sRepl[i].px;
        *w = sRepl[i].w;
        *h = sRepl[i].h;
        sRepl[i].px = NULL;
        sRepl[i].state = 3;
    }
    pthread_mutex_unlock(&sLock);
    return st == 1 ? 1 : st == 0 ? 0 : -1;
}

void Tp_Free(uint8_t *px) { stbi_image_free(px); }

void Tp_Report(uint32_t replacedDraws, uint32_t named, uint32_t gpuUploads) {
    struct timespec t0, t;
    if (!sDump && !sReplace && sNRepl == 0) {
        return;
    }
    /* the dumps queued so far reach the disk (up to 30 s) */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    pthread_mutex_lock(&sLock);
    while (sHead != NULL || sBusy) {
        struct timespec until;
        clock_gettime(CLOCK_MONOTONIC, &t);
        if (t.tv_sec - t0.tv_sec > 30) {
            break;
        }
        clock_gettime(CLOCK_REALTIME, &until);
        until.tv_nsec += 20000000;
        if (until.tv_nsec >= 1000000000) {
            until.tv_sec++;
            until.tv_nsec -= 1000000000;
        }
        pthread_cond_timedwait(&sCond, &sLock, &until);
    }
    printf("ttt: textures: %s; dump %s, replace %s; %u textures named, %u dumped (%u on the disk already, %u failed); "
           "%d replacements (%d decoded, %d failed, %d other files), %u uploaded, %u primitives drawn with one\n",
           sGame, sDump ? "on" : "off", sReplace ? "on" : "off", named, sDumped, sDumpExisting, sDumpFailed, sNRepl, sDecoded, sFailed,
           sSkippedFiles, gpuUploads, replacedDraws);
    pthread_mutex_unlock(&sLock);
    fflush(stdout);
}

/* ------------------------------------------------------------------------------------------------ self-test */

static int sFails;

static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: texpack self-test: FAILED: %s\n", what);
        sFails++;
    }
}

/* TTT_SELFTEST=texpack: the names on a made-up GS memory, no files and no GL. */
int Tp_SelfTest(void) {
    uint32_t *vram = calloc(1u << 20, 4), i;
    uint8_t *ref = malloc(64 * 256);
    TpName n, p;
    char s[64];
    Draw d;
    uint64_t h;

    if (vram == NULL || ref == NULL) {
        return 1;
    }
    for (i = 0; i < (1u << 20); i++) {
        vram[i] = i * 2654435761u;
    }
    /* a 64 x 32 PSMCT32 texture at block 0, width 1: one page, its 32 blocks in block-grid order */
    memset(&d, 0, sizeof d);
    d.tpsm = 0x00;
    d.tbp = 0;
    d.tbw = 1;
    d.lw = 6;
    d.lh = 5;
    expect(Tp_Name(vram, &d, NULL, &n), "PSMCT32 is named");
    {
        static const uint8_t grid[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23},
                                           {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
        int bx, by, k = 0;
        for (by = 0; by < 4; by++) {
            for (bx = 0; bx < 8; bx++, k++) {
                memcpy(ref + k * 256, vram + grid[by][bx] * 64, 256);
            }
        }
        h = XXH3_64bits(ref, 32 * 256);
    }
    expect(n.tex == h, "PSMCT32: XXH3 of the blocks in grid order");
    expect(n.clut == 0 && n.bits == (0x00u | 6u << 6 | 5u << 10), "PSMCT32: no CLUT, bits");
    name_str(&n, s, sizeof s);
    expect(parse_name(strcat(s, ".png"), &p) && name_cmp(&p, &n) == 0, "a direct name reads back");
    /* a 4 x 4 PSMCT32 texture: smaller than a block, the texels row by row */
    d.lw = 2;
    d.lh = 2;
    expect(Tp_Name(vram, &d, NULL, &n), "4 x 4 is named");
    {
        uint32_t px[16];
        int x, y;
        for (y = 0; y < 4; y++) {
            for (x = 0; x < 4; x++) {
                px[y * 4 + x] = rd(vram, 0, 1, 0, (uint32_t)x, (uint32_t)y);
            }
        }
        expect(n.tex == XXH3_64bits(px, sizeof px), "4 x 4: the texels expanded");
    }
    /* PSMCT16 with TEXA in the bits; PSMT8 with its CLUT */
    d.tpsm = 0x02;
    d.lw = d.lh = 4;
    d.ta0 = 0x12;
    d.ta1 = 0x34;
    d.aem = 1;
    expect(Tp_Name(vram, &d, NULL, &n) && n.bits == (0x02u | 4u << 6 | 4u << 10 | 0x12u << 15 | 1u << 23 | 0x34u << 24), "PSMCT16: TEXA in the bits");
    {
        uint32_t pal[256];
        for (i = 0; i < 256; i++) {
            pal[i] = i * 0x01010101u;
        }
        d.tpsm = 0x13;
        d.tbw = 2;
        expect(!Tp_Name(vram, &d, NULL, &n), "PSMT8 without a CPU CLUT: not named");
        expect(Tp_Name(vram, &d, pal, &n) && n.clut == XXH3_64bits(pal, 1024) && n.bits == (0x13u | 4u << 6 | 4u << 10), "PSMT8: CLUT hash, no TEXA");
        name_str(&n, s, sizeof s);
        expect(parse_name(strcat(s, ".PNG"), &p) && name_cmp(&p, &n) == 0, "an indexed name reads back");
    }
    expect(parse_name("abc-r16x0-00001234.png", &p) && p.rw == 16 && p.rh == 0 && p.tex == 0xabc && p.bits == 0x1234, "a region name reads");
    expect(!parse_name("abc-r16-00001234.png", &p) && !parse_name("abc-00001234-mip1.png", &p) && !parse_name("abc-00001234.dds", &p) &&
           !parse_name("abc-def-00001234.png", &p) && !parse_name("abc-00001213.png", &p), "malformed, mip levels, DDS, CLUT part not as the format says");
    /* region clamp narrower than the texture (the game's atlases): the region's texels, expanded, and "-r<w>x<h>" */
    d.tpsm = 0x00;
    d.tbw = 1;
    d.lw = d.lh = 5;
    d.wms = 2;
    d.minu = 8;
    d.maxu = 23;
    expect(Tp_Name(vram, &d, NULL, &n) && n.rw == 16 && n.rh == 0 && n.x0 == 8 && n.w == 16 && n.h == 32, "region clamp: 16 x 32 from x 8");
    {
        uint32_t px[16 * 32];
        int x, y;
        for (y = 0; y < 32; y++) {
            for (x = 0; x < 16; x++) {
                px[y * 16 + x] = rd(vram, 0, 1, 0, (uint32_t)(8 + x), (uint32_t)y);
            }
        }
        expect(n.tex == XXH3_64bits(px, sizeof px), "region clamp: its texels expanded");
    }
    name_str(&n, s, sizeof s);
    expect(strstr(s, "-r16x0-") != NULL && parse_name(strcat(s, ".png"), &p) && name_cmp(&p, &n) == 0, "a region name reads back");
    d.wms = 3; /* region repeat: [MAXU, (MINU | MAXU) + 1) */
    d.minu = 15;
    d.maxu = 16;
    expect(Tp_Name(vram, &d, NULL, &n) && n.x0 == 16 && n.rw == 16, "region repeat");
    free(vram);
    free(ref);
    if (sFails == 0) {
        printf("ttt: texpack self-test OK\n");
    }
    return sFails;
}
