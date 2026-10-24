/*
 * Movies on PC (T33): the game's three PSS movies decoded natively (docs/port/movies.md).
 *
 * On the PS2 the movie player of movie.c (Sony's libmpeg sample) reads the stream from the IOP into a ring, splits it
 * with sceMpegDemuxPssRing, feeds the IPU by DMA from a decoder thread and shows the pictures from a VBlank interrupt;
 * the IOP plays the interleaved ADPCM audio itself. The port has no IPU, threads or IOP. Its player (movie.c, under
 * PORT) keeps the game's own display side: the same picture buffer, the same GS packets (func_0039D580), the same
 * frame buffers and display switch; this file gives it the pictures:
 *  - Port_MovieOpen: the video bytes of the file that 0x2058 started (each chunk of the file is `audio` sectors of
 *    ADPCM, then `video` sectors of the PSS the EE gets; plat_file.c keeps 0x2058's arguments),
 *  - Port_MovieFrame: the next picture in display order, decoded by third_party/pl_mpeg2 (MPEG-2, integer only) and
 *    converted as the IPU's colour space conversion does to RGBA32, in the order sceMpegGetPicture writes it:
 *    16 x 16 macroblocks of 0x400 bytes, column by column (the game's upload packets place them so),
 *  - Port_MovieAudio: the audio track for code 0x5C (plat_bgm.c): the same file's ADPCM sectors, with the length and
 *    the volume scale of the driver's own per-movie table, read from the user's RSPU2DRV.IRX.
 * Everything runs on the game's thread, one picture per two VBlanks: the same pictures on every run.
 * TTT_MOVIES=0 skips the movies as before T33 (the player sees the stream never go idle).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_addr.h"
#include "../../third_party/pl_mpeg2/m2v.h"

int Port_DiscRead(uint32_t sector, uint32_t offset, void *dst, uint32_t bytes); /* plat_file.c */
long Port_DiscReadFile(const char *dirName, const char *name, void *dst, long max);
int Port_MovieStreamArgs(uint32_t *sector, uint32_t *movie, uint32_t *audio, uint32_t *video);
void Port_Hit(const char *what);

#define SECTOR 2048u
#define STAGE 0x20000u /* more than a whole PES packet (at most 65541 bytes) */
#define READ 0x4000u

static struct {
    M2v *dec;
    uint32_t sector, audioBytes, videoBytes; /* the file, its interleave */
    uint32_t total, pos;                     /* video bytes the EE would get, read so far */
    uint8_t stage[STAGE + READ];             /* PSS not yet split */
    uint32_t have, done;
    uint32_t written; /* elementary stream bytes given to the decoder */
    int frames;
} sM;

int Port_MoviesOn(void) {
    const char *e = getenv("TTT_MOVIES");
    return e == NULL || strcmp(e, "0") != 0;
}

/* movie.c func_0039C878 asks before the movie: with TTT_MOVIES=0 it is skipped as before T33. */
int Port_MoviePlayable(void) {
    if (!Port_MoviesOn()) {
        Port_Hit("movie (skipped)");
        return 0;
    }
    return 1;
}

static uint32_t be16(const uint8_t *p) { return (uint32_t)p[0] << 8 | p[1]; }

/* Splits the staged PSS: the payload of every complete video PES packet goes to the decoder. */
static void split(void) {
    for (;;) {
        const uint8_t *p = sM.stage + sM.done;
        uint32_t left = sM.have - sM.done, len;
        if (left < 4) {
            break;
        }
        if (p[0] != 0 || p[1] != 0 || p[2] != 1) {
            sM.done++; /* not at a start code: skip (the 4 bytes past the last pack) */
            continue;
        }
        if (p[3] == 0xBA) { /* pack header: MPEG-2 (14 bytes + stuffing) or MPEG-1 (12) */
            if (left < 14) {
                break;
            }
            len = (p[4] & 0xC0) == 0x40 ? 14u + (p[13] & 7u) : 12u;
        } else if (p[3] == 0xB9) { /* program end */
            len = 4;
        } else if (p[3] >= 0xBB) { /* system header, padding, PES */
            if (left < 6) {
                break;
            }
            len = 6 + be16(p + 4);
            if (left < len) {
                break;
            }
            if ((p[3] & 0xF0) == 0xE0 && len >= 9 && (p[6] & 0xC0) == 0x80 && 9u + p[8] <= len) {
                m2v_write(sM.dec, p + 9 + p[8], (int)(len - 9 - p[8]));
                sM.written += len - 9 - p[8];
            }
        } else {
            len = 1;
        }
        if (left < len) {
            break;
        }
        sM.done += len;
    }
    if (sM.done > 0) {
        memmove(sM.stage, sM.stage + sM.done, sM.have - sM.done);
        sM.have -= sM.done;
        sM.done = 0;
    }
}

/* The decoder wants more: the next piece of the video stream, split. */
static void load(M2v *d, void *user) {
    uint32_t chunk = sM.videoBytes, written = sM.written, n, in, off;
    (void)user;
    while (sM.pos < sM.total && sM.written == written) {
        in = sM.pos % chunk;
        n = chunk - in < READ ? chunk - in : READ;
        if (n > sM.total - sM.pos) {
            n = sM.total - sM.pos;
        }
        if (sM.have + n > sizeof sM.stage) {
            sM.have = 0; /* a packet longer than a PES can be: drop the garbage */
        }
        off = (sM.pos / chunk) * (chunk + sM.audioBytes) + sM.audioBytes + in;
        if (!Port_DiscRead(sM.sector, off, sM.stage + sM.have, n)) {
            sM.pos = sM.total; /* past the file: the end */
            break;
        }
        sM.have += n;
        sM.pos += n;
        split();
    }
    if (sM.pos >= sM.total && sM.written == written) {
        m2v_end(d);
    }
}

void Port_MovieClose(void) {
    m2v_destroy(sM.dec);
    sM.dec = NULL;
}

/* The video of the stream 0x2058 started; `bytes`: its length as the game counts it (D_00518EA8). */
int Port_MovieOpen(int movie, uint32_t bytes) {
    uint32_t sector, m, audio, video;
    (void)movie;
    Port_MovieClose();
    if (!Port_MovieStreamArgs(&sector, &m, &audio, &video) || video == 0) {
        return 0;
    }
    memset(&sM, 0, sizeof sM);
    sM.sector = sector;
    sM.audioBytes = audio * SECTOR;
    sM.videoBytes = video * SECTOR;
    sM.total = bytes;
    sM.dec = m2v_create(load, NULL);
    return sM.dec != NULL;
}

static uint8_t clamp8(int v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

/* The next picture into `dst` (EE address): 1, or 0 at the end (or on an unsupported stream). RGBA32 with alpha
   0x80, in 16 x 16 macroblocks of 0x400 bytes, column by column, as sceMpegGetPicture leaves it for the game's
   upload packets. Colour space conversion: ITU-R BT.601 (Y 16-235, Cb / Cr 16-240), 8-bit fixed point as the IPU's
   CSC [I: its exact rounding is not modelled], each chroma sample over 2 x 2 pixels. */
int Port_MovieFrame(uint32_t dst, int *width, int *height) {
    M2vFrame f;
    uint8_t *out;
    int mx, my, x, y, mbw, mbh;
    if (sM.dec == NULL || m2v_decode(sM.dec, &f) != 1) {
        return 0;
    }
    mbw = (f.width + 15) >> 4;
    mbh = (f.height + 15) >> 4;
    out = Port_Ptr(dst);
    for (mx = 0; mx < mbw; mx++) {
        for (my = 0; my < mbh; my++) {
            for (y = 0; y < 16; y++) {
                const uint8_t *py = f.y + (size_t)(my * 16 + y) * (size_t)f.stride + (size_t)mx * 16;
                const uint8_t *pb = f.cb + (size_t)(my * 8 + y / 2) * (size_t)f.cstride + (size_t)mx * 8;
                const uint8_t *pr = f.cr + (size_t)(my * 8 + y / 2) * (size_t)f.cstride + (size_t)mx * 8;
                for (x = 0; x < 16; x++) {
                    int yy = (py[x] - 16) * 298, cb = pb[x / 2] - 128, cr = pr[x / 2] - 128;
                    out[0] = clamp8((yy + 409 * cr + 128) >> 8);
                    out[1] = clamp8((yy - 100 * cb - 208 * cr + 128) >> 8);
                    out[2] = clamp8((yy + 516 * cb + 128) >> 8);
                    out[3] = 0x80;
                    out += 4;
                }
            }
        }
    }
    sM.frames++;
    *width = f.width;
    *height = f.height;
    return 1;
}

/* ---- the audio (code 0x5C, plat_bgm.c) ---- */

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

#define DRV_MOVIE_TABLE 0x9EA0u /* RSPU2DRV.IRX .rodata: 12 bytes per movie {u16 scale, u16, file size, ADPCM bytes} */
#define FILE_TABLE 0x00509E40u  /* gFileTable: {sector, size + 0x350A, check, id}; movies are files 14-16 */

/* The driver's entry for `movie` from the disc's RSPU2DRV.IRX, checked against the game's file table; 0 if it is
   not there. */
static int driver_entry(int movie, uint32_t *scale, uint32_t *bytes) {
    static uint8_t irx[0x10000];
    static long n = -2;
    uint32_t shoff, shnum, shentsize, i, fileSize;
    if (n == -2) {
        n = Port_DiscReadFile("IRX", "RSPU2DRV.IRX", irx, sizeof irx);
    }
    if (n < 0x34 || memcmp(irx, "\x7F" "ELF", 4) != 0 || movie < 0 || movie > 2) {
        return 0;
    }
    shoff = le32(irx + 0x20);
    shentsize = irx[0x2E] | irx[0x2F] << 8;
    shnum = irx[0x30] | irx[0x31] << 8;
    fileSize = ((const uint32_t *)Port_Ptr(FILE_TABLE))[(14 + movie) * 4 + 1] - 0x350Au;
    for (i = 0; i < shnum && shoff + (i + 1) * shentsize <= (uint32_t)n; i++) {
        const uint8_t *sh = irx + shoff + i * shentsize;
        uint32_t addr = le32(sh + 12), off = le32(sh + 16), size = le32(sh + 20);
        uint32_t at = DRV_MOVIE_TABLE + (uint32_t)movie * 12;
        if (le32(sh + 4) == 1 && addr <= at && at + 12 <= addr + size && off + (at - addr) + 12 <= (uint32_t)n) {
            const uint8_t *e = irx + off + (at - addr);
            if (le32(e + 4) != fileSize) {
                return 0; /* another driver version */
            }
            *scale = e[0] | e[1] << 8;
            *bytes = le32(e + 8);
            return 1;
        }
    }
    return 0;
}

/* Code 0x5C n: the movie's audio as a stream track: the file's sector, its interleave, the ADPCM bytes to play and
   the volume scale. Without the driver's table: all the file's audio sectors at scale 0x3333 (the driver's default). */
int Port_MovieAudio(int movie, uint32_t *sector, uint32_t *audio, uint32_t *video, uint32_t *bytes, uint32_t *scale) {
    uint32_t m, fileSize, chunks;
    if (!Port_MovieStreamArgs(sector, &m, audio, video) || *audio == 0 || movie < 0 || movie > 2) {
        return 0;
    }
    if (!driver_entry(movie, scale, bytes)) {
        fileSize = ((const uint32_t *)Port_Ptr(FILE_TABLE))[(14 + movie) * 4 + 1] - 0x350Au;
        chunks = (fileSize + (*audio + *video) * SECTOR - 1) / ((*audio + *video) * SECTOR);
        *bytes = chunks * *audio * SECTOR;
        *scale = 0x3333;
    }
    return 1;
}
