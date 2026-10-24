/* MPEG-2 video decoder: PL_MPEG's MPEG-1 video decoder (MIT, Dominic Szablewski; see LICENSE) extended to the
   MPEG-2 main profile subset the PS2's encoders wrote: 4:2:0, frame pictures with frame_pred_frame_dct = 1. Integer
   arithmetic only (no host float: the port forbids it, tests/test_port_float.py). */
#include <stdlib.h>
#include <string.h>
#include "m2v.h"
#include "m2v_tables.h"

enum { PIC_I = 1, PIC_P = 2, PIC_B = 3 };
enum { SC_PICTURE = 0x00, SC_SLICE_FIRST = 0x01, SC_SLICE_LAST = 0xAF, SC_USER = 0xB2, SC_SEQUENCE = 0xB3,
       SC_EXTENSION = 0xB5 };
#define IS_SLICE(c) ((c) >= SC_SLICE_FIRST && (c) <= SC_SLICE_LAST)
#define B15_EOB 0xFFFE
#define ESCAPE 0xFFFF

static const uint8_t ZIG_ZAG[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

static const uint8_t INTRA_QUANT[64] = {
    8,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37, 19, 22, 26, 27, 29, 34,
    34, 38, 22, 22, 26, 27, 29, 34, 37, 40, 22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32,
    35, 40, 48, 58, 26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83,
};

/* The scale factors of the IDCT below (PL_MPEG), folded into the coefficients. */
static const uint8_t PREMULTIPLIER[64] = {
    32, 44, 42, 38, 32, 25, 17, 9,  44, 62, 58, 52, 44, 35, 24, 12, 42, 58, 55, 49, 42, 33,
    23, 12, 38, 52, 49, 44, 38, 30, 20, 10, 32, 44, 42, 38, 32, 25, 17, 9,  25, 35, 33, 30,
    25, 20, 14, 7,  17, 24, 23, 20, 17, 14, 9,  5,  9,  12, 12, 10, 9,  7,  5,  2,
};

/* quantiser_scale for q_scale_type 1 (ISO/IEC 13818-2 table 7-6). */
static const uint8_t NONLINEAR_QSCALE[32] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  10, 12, 14, 16, 18, 20,  22,
    24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96, 104, 112,
};

typedef struct {
    uint8_t *y, *cb, *cr;
} Planes;

typedef struct {
    int h, v;        /* the motion vector predictor, half pels */
    int is_set;
    int r_h, r_v;    /* f_code - 1 per component */
} Motion;

#define B15_NODES 512

struct M2v {
    /* the elementary stream */
    uint8_t *bytes;
    size_t cap, len, bit;
    int ended;
    M2vLoad load;
    void *user;

    /* sequence */
    int has_sequence;
    int width, height, mb_width, mb_height, mb_size, luma_width, chroma_width;
    uint8_t *frames_data;
    Planes cur, fwd, bwd;
    uint8_t intra_quant[64], non_intra_quant[64];

    /* picture */
    int start_code;
    int picture_type;
    int intra_vlc_format, q_scale_type;
    Motion mf, mb;
    int has_reference;
    int unsupported;

    /* slice / macroblock */
    int quantizer_scale;
    int slice_begin;
    int mb_address, mb_row, mb_col;
    int mb_type, mb_intra;
    int dc_predictor[3];
    int block[64];

    M2vVlcU b15[B15_NODES * 2];
};

/* ---- bit reader (PL_MPEG's plm_buffer, append mode) ---- */

void m2v_write(M2v *d, const uint8_t *bytes, int n) {
    if (n <= 0) {
        return;
    }
    if (d->len + (size_t)n + 4 > d->cap) {
        size_t cap = d->cap ? d->cap : 1 << 16;
        while (d->len + (size_t)n + 4 > cap) {
            cap *= 2;
        }
        d->bytes = realloc(d->bytes, cap);
        d->cap = cap;
    }
    memcpy(d->bytes + d->len, bytes, (size_t)n);
    d->len += (size_t)n;
    memset(d->bytes + d->len, 0, 4); /* reads may look 4 bytes ahead */
}

void m2v_end(M2v *d) {
    d->ended = 1;
}

static void discard_read_bytes(M2v *d) {
    size_t byte = d->bit >> 3;
    if (byte > 0 && d->bytes != NULL) {
        memmove(d->bytes, d->bytes + byte, d->len - byte + 4);
        d->len -= byte;
        d->bit -= byte << 3;
    }
}

static int has(M2v *d, size_t bits) {
    while ((d->len << 3) - d->bit < bits) {
        size_t before = d->len;
        if (d->ended) {
            return 0;
        }
        d->load(d, d->user);
        if (d->len == before && !d->ended) {
            d->ended = 1; /* a loader that gives nothing has nothing left */
        }
    }
    return 1;
}

/* Up to 24 bits; 0 (and the position at the end) when the stream has fewer. */
static int read_bits(M2v *d, int count) {
    const uint8_t *p;
    uint32_t w;
    if (!has(d, (size_t)count)) {
        d->bit = d->len << 3;
        return 0;
    }
    p = d->bytes + (d->bit >> 3);
    w = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    w <<= d->bit & 7;
    d->bit += (size_t)count;
    return (int)(w >> (32 - count));
}

static void skip_bits(M2v *d, int count) {
    if (has(d, (size_t)count)) {
        d->bit += (size_t)count;
    }
}

static void align(M2v *d) {
    d->bit = (d->bit + 7) & ~(size_t)7;
}

static int next_start_code(M2v *d) {
    align(d);
    while (has(d, 5 << 3)) {
        size_t i = d->bit >> 3;
        if (d->bytes[i] == 0 && d->bytes[i + 1] == 0 && d->bytes[i + 2] == 1) {
            d->bit = (i + 4) << 3;
            return d->bytes[i + 3];
        }
        d->bit += 8;
    }
    if (has(d, 4 << 3)) { /* a start code in the very last 4 bytes */
        size_t i = d->bit >> 3;
        if (d->bytes[i] == 0 && d->bytes[i + 1] == 0 && d->bytes[i + 2] == 1) {
            d->bit = (i + 4) << 3;
            return d->bytes[i + 3];
        }
    }
    d->bit = d->len << 3;
    return -1;
}

/* Whether a picture start code follows (loading as needed), without moving. */
static int has_picture_ahead(M2v *d) {
    size_t at = d->bit;
    int c;
    do {
        c = next_start_code(d);
    } while (c != -1 && c != SC_PICTURE);
    d->bit = at;
    return c == SC_PICTURE;
}

static int peek_non_zero(M2v *d, int count) {
    int v;
    if (!has(d, (size_t)count)) {
        return 0;
    }
    v = read_bits(d, count);
    d->bit -= (size_t)count;
    return v != 0;
}

static int read_vlc(M2v *d, const M2vVlc *table) {
    M2vVlc s = {0, 0};
    do {
        s = table[s.index + read_bits(d, 1)];
    } while (s.index > 0);
    return s.value;
}

static unsigned read_vlc_uint(M2v *d, const M2vVlcU *table) {
    return (uint16_t)read_vlc(d, (const M2vVlc *)table);
}

/* ---- set-up ---- */

static void build_b15(M2v *d) {
    int nodes = 1, i, b, extra;
    for (i = 0; i < B15_NODES * 2; i++) { /* invalid everywhere: index -1, value 0 (a zero level: corrupt) */
        d->b15[i].index = -1;
        d->b15[i].value = 0;
    }
    for (i = 0; i < (int)(sizeof(M2V_DCT_COEFF_B15) / sizeof(M2V_DCT_COEFF_B15[0])) + 2; i++) {
        unsigned code, len, value;
        int node = 0;
        if (i < (int)(sizeof(M2V_DCT_COEFF_B15) / sizeof(M2V_DCT_COEFF_B15[0]))) {
            code = M2V_DCT_COEFF_B15[i].code;
            len = M2V_DCT_COEFF_B15[i].len;
            value = (unsigned)M2V_DCT_COEFF_B15[i].run << 8 | M2V_DCT_COEFF_B15[i].level;
        } else if (i == (int)(sizeof(M2V_DCT_COEFF_B15) / sizeof(M2V_DCT_COEFF_B15[0]))) {
            code = 0x01, len = 6, value = ESCAPE;
        } else {
            code = 0x06, len = 4, value = B15_EOB;
        }
        for (b = (int)len - 1; b > 0; b--) {
            M2vVlcU *e = &d->b15[node * 2 + ((code >> b) & 1)];
            if (e->index <= 0) {
                e->index = (int16_t)(nodes * 2);
                e->value = 0;
                nodes++;
            }
            node = e->index / 2;
        }
        extra = node * 2 + (code & 1);
        d->b15[extra].index = 0;
        d->b15[extra].value = (uint16_t)value;
    }
}

M2v *m2v_create(M2vLoad load, void *user) {
    M2v *d = calloc(1, sizeof(M2v));
    if (d == NULL) {
        return NULL;
    }
    d->load = load;
    d->user = user;
    d->start_code = -1;
    build_b15(d);
    return d;
}

void m2v_destroy(M2v *d) {
    if (d != NULL) {
        free(d->frames_data);
        free(d->bytes);
        free(d);
    }
}

static void load_matrix(M2v *d, uint8_t *m) {
    int i;
    for (i = 0; i < 64; i++) {
        m[ZIG_ZAG[i]] = (uint8_t)read_bits(d, 8);
    }
}

static int decode_sequence_header(M2v *d) {
    int w, h, i;
    size_t luma, chroma;
    if (!has(d, 64 + 2 * 64 * 8)) {
        return 0;
    }
    w = read_bits(d, 12);
    h = read_bits(d, 12);
    skip_bits(d, 4 + 4 + 18 + 1 + 10 + 1); /* aspect, frame rate, bit rate, marker, vbv, constrained */
    if (read_bits(d, 1)) {
        load_matrix(d, d->intra_quant);
    } else {
        memcpy(d->intra_quant, INTRA_QUANT, 64);
    }
    if (read_bits(d, 1)) {
        load_matrix(d, d->non_intra_quant);
    } else {
        for (i = 0; i < 64; i++) {
            d->non_intra_quant[i] = 16;
        }
    }
    if (w <= 0 || h <= 0) {
        return 0;
    }
    if (d->has_sequence) {
        return w == d->width && h == d->height; /* a repeated header: the same picture size */
    }
    d->width = w;
    d->height = h;
    d->mb_width = (w + 15) >> 4;
    d->mb_height = (h + 15) >> 4;
    d->mb_size = d->mb_width * d->mb_height;
    d->luma_width = d->mb_width << 4;
    d->chroma_width = d->mb_width << 3;
    luma = (size_t)d->luma_width * (size_t)(d->mb_height << 4);
    chroma = (size_t)d->chroma_width * (size_t)(d->mb_height << 3);
    d->frames_data = calloc(3, luma + 2 * chroma);
    if (d->frames_data == NULL) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        Planes *p = i == 0 ? &d->cur : i == 1 ? &d->fwd : &d->bwd;
        p->y = d->frames_data + (luma + 2 * chroma) * (size_t)i;
        p->cb = p->y + luma;
        p->cr = p->cb + chroma;
    }
    d->has_sequence = 1;
    return 1;
}

/* An extension after a sequence header or a picture header (its start code read). */
static void decode_extension(M2v *d) {
    int id = read_bits(d, 4);
    if (id == 1) { /* sequence extension: 4:2:0 only, no size extension */
        int chroma;
        skip_bits(d, 8 + 1);
        chroma = read_bits(d, 2);
        if (chroma != 1 || read_bits(d, 2) != 0 || read_bits(d, 2) != 0) {
            d->unsupported = 1;
        }
    } else if (id == 3) { /* quant matrix extension */
        if (read_bits(d, 1)) {
            load_matrix(d, d->intra_quant);
        }
        if (read_bits(d, 1)) {
            load_matrix(d, d->non_intra_quant);
        }
    } else if (id == 8) { /* picture coding extension */
        int dc_precision, structure, frame_pred_frame_dct, concealment, alternate_scan;
        d->mf.r_h = read_bits(d, 4) - 1;
        d->mf.r_v = read_bits(d, 4) - 1;
        d->mb.r_h = read_bits(d, 4) - 1;
        d->mb.r_v = read_bits(d, 4) - 1;
        dc_precision = read_bits(d, 2);
        structure = read_bits(d, 2);
        skip_bits(d, 1); /* top_field_first */
        frame_pred_frame_dct = read_bits(d, 1);
        concealment = read_bits(d, 1);
        d->q_scale_type = read_bits(d, 1);
        d->intra_vlc_format = read_bits(d, 1);
        alternate_scan = read_bits(d, 1);
        if (dc_precision != 0 || structure != 3 || !frame_pred_frame_dct || concealment || alternate_scan) {
            d->unsupported = 1;
        }
    }
}

static void decode_slice(M2v *d, int slice);
static void decode_macroblock(M2v *d);
static void decode_block(M2v *d, int block);
static void predict_macroblock(M2v *d);
static void idct(int *block);

static int map_qscale(M2v *d, int code) {
    return d->q_scale_type ? NONLINEAR_QSCALE[code] : code << 1;
}

static void reset_dc(M2v *d) {
    d->dc_predictor[0] = d->dc_predictor[1] = d->dc_predictor[2] = 128;
}

static void decode_picture(M2v *d) {
    Planes tmp;
    int seen_coding_ext = 0;
    skip_bits(d, 10); /* temporal_reference */
    d->picture_type = read_bits(d, 3);
    skip_bits(d, 16); /* vbv_delay */
    if (d->picture_type < PIC_I || d->picture_type > PIC_B) {
        d->unsupported = 1;
        return;
    }
    if (d->picture_type >= PIC_P) {
        skip_bits(d, 4); /* full_pel_forward_vector, forward_f_code: 0 / 7 in MPEG-2 */
    }
    if (d->picture_type == PIC_B) {
        skip_bits(d, 4);
    }
    while (read_bits(d, 1)) { /* extra_information_picture */
        skip_bits(d, 8);
    }
    do {
        d->start_code = next_start_code(d);
        if (d->start_code == SC_EXTENSION) {
            size_t at = d->bit;
            if (read_bits(d, 4) == 8) {
                seen_coding_ext = 1;
            }
            d->bit = at;
            decode_extension(d);
        }
    } while (d->start_code == SC_EXTENSION || d->start_code == SC_USER);
    if (!seen_coding_ext) {
        d->unsupported = 1; /* MPEG-1 */
    }
    if (d->unsupported) {
        return;
    }

    tmp = d->fwd;
    if (d->picture_type != PIC_B) {
        d->fwd = d->bwd;
    }
    while (IS_SLICE(d->start_code)) {
        decode_slice(d, d->start_code);
        if (d->mb_address >= d->mb_size - 1) {
            break;
        }
        d->start_code = next_start_code(d);
    }
    if (d->picture_type != PIC_B) {
        d->bwd = d->cur;
        d->cur = tmp;
    }
}

int m2v_decode(M2v *d, M2vFrame *out) {
    const Planes *shown = NULL;
    int shown_type = 0;
    for (;;) {
        if (d->unsupported) {
            return -1;
        }
        while (d->start_code != SC_PICTURE) {
            d->start_code = next_start_code(d);
            if (d->start_code == SC_SEQUENCE) {
                if (!decode_sequence_header(d)) {
                    return -1;
                }
            } else if (d->start_code == SC_EXTENSION) {
                decode_extension(d);
            } else if (d->start_code == -1) {
                /* the end: the last reference picture is still to be shown */
                if (d->has_reference) {
                    d->has_reference = 0;
                    shown = &d->bwd;
                    shown_type = PIC_P;
                    goto out;
                }
                return 0;
            }
        }
        if (!d->has_sequence) {
            return -1;
        }
        /* the whole picture in the buffer: up to the next picture start code, or the end of the stream */
        has_picture_ahead(d);
        discard_read_bytes(d);
        decode_picture(d);
        if (d->unsupported) {
            return -1;
        }
        if (d->picture_type == PIC_B) {
            shown = &d->cur;
            shown_type = PIC_B;
        } else if (d->has_reference) {
            shown = &d->fwd;
            shown_type = d->picture_type;
        } else {
            d->has_reference = 1;
        }
        if (shown != NULL) {
            break;
        }
    }
out:
    out->width = d->width;
    out->height = d->height;
    out->stride = d->luma_width;
    out->cstride = d->chroma_width;
    out->y = shown->y;
    out->cb = shown->cb;
    out->cr = shown->cr;
    out->type = shown_type;
    return 1;
}

/* ---- slices and macroblocks ---- */

static void decode_slice(M2v *d, int slice) {
    d->slice_begin = 1;
    d->mb_address = (slice - 1) * d->mb_width - 1;
    d->mb.h = d->mf.h = 0;
    d->mb.v = d->mf.v = 0;
    reset_dc(d);
    d->quantizer_scale = map_qscale(d, read_bits(d, 5));
    while (read_bits(d, 1)) { /* intra_slice_flag and its byte, extra_information_slice */
        skip_bits(d, 8);
    }
    do {
        decode_macroblock(d);
    } while (d->mb_address < d->mb_size - 1 && peek_non_zero(d, 23));
}

static int decode_motion_vector(M2v *d, int r_size, int motion) {
    int fscale = 1 << r_size;
    int m_code = read_vlc(d, M2V_MOTION);
    int r, delta;
    if (m_code != 0 && fscale != 1) {
        r = read_bits(d, r_size);
        delta = ((abs(m_code) - 1) << r_size) + r + 1;
        if (m_code < 0) {
            delta = -delta;
        }
    } else {
        delta = m_code;
    }
    motion += delta;
    if (motion > (fscale << 4) - 1) {
        motion -= fscale << 5;
    } else if (motion < -(fscale << 4)) {
        motion += fscale << 5;
    }
    return motion;
}

static void decode_motion_vectors(M2v *d) {
    if (d->mf.is_set) {
        d->mf.h = decode_motion_vector(d, d->mf.r_h, d->mf.h);
        d->mf.v = decode_motion_vector(d, d->mf.r_v, d->mf.v);
    } else if (d->picture_type == PIC_P) {
        d->mf.h = d->mf.v = 0; /* no motion in a P picture: zero vector, predictors reset */
    }
    if (d->mb.is_set) {
        d->mb.h = decode_motion_vector(d, d->mb.r_h, d->mb.h);
        d->mb.v = decode_motion_vector(d, d->mb.r_v, d->mb.v);
    }
}

static void decode_macroblock(M2v *d) {
    int increment = 0, t, cbp, block, mask;
    const M2vVlc *table;

    t = read_vlc(d, M2V_MACROBLOCK_ADDRESS_INCREMENT);
    while (t == 34) { /* macroblock_stuffing (MPEG-1 only, harmless) */
        t = read_vlc(d, M2V_MACROBLOCK_ADDRESS_INCREMENT);
    }
    while (t == 35) { /* macroblock_escape */
        increment += 33;
        t = read_vlc(d, M2V_MACROBLOCK_ADDRESS_INCREMENT);
    }
    increment += t;

    if (d->slice_begin) {
        d->slice_begin = 0;
        d->mb_address += increment;
    } else {
        if (d->mb_address + increment >= d->mb_size) {
            return; /* corrupt */
        }
        if (increment > 1) {
            reset_dc(d);
            if (d->picture_type == PIC_P) {
                d->mf.h = d->mf.v = 0;
            }
        }
        while (increment > 1) { /* skipped macroblocks */
            d->mb_address++;
            d->mb_row = d->mb_address / d->mb_width;
            d->mb_col = d->mb_address % d->mb_width;
            predict_macroblock(d);
            increment--;
        }
        d->mb_address++;
    }
    d->mb_row = d->mb_address / d->mb_width;
    d->mb_col = d->mb_address % d->mb_width;
    if (d->mb_col >= d->mb_width || d->mb_row >= d->mb_height) {
        return; /* corrupt */
    }

    table = M2V_MACROBLOCK_TYPE[d->picture_type];
    d->mb_type = read_vlc(d, table);
    d->mb_intra = d->mb_type & 0x01;
    d->mf.is_set = d->mb_type & 0x08;
    d->mb.is_set = d->mb_type & 0x04;
    if (d->mb_type & 0x10) {
        d->quantizer_scale = map_qscale(d, read_bits(d, 5));
    }
    if (d->mb_intra) {
        d->mb.h = d->mf.h = 0;
        d->mb.v = d->mf.v = 0;
    } else {
        reset_dc(d);
        decode_motion_vectors(d);
        predict_macroblock(d);
    }

    cbp = (d->mb_type & 0x02) ? read_vlc(d, M2V_CODE_BLOCK_PATTERN) : (d->mb_intra ? 0x3F : 0);
    for (block = 0, mask = 0x20; block < 6; block++, mask >>= 1) {
        if (cbp & mask) {
            decode_block(d, block);
        }
    }
}

/* ---- prediction (PL_MPEG) ---- */

#define BLOCK_SET(DEST, DEST_INDEX, DEST_WIDTH, SOURCE_INDEX, SOURCE_WIDTH, BLOCK_SIZE, OP)                          \
    do {                                                                                                         \
        int dest_scan = DEST_WIDTH - BLOCK_SIZE, source_scan = SOURCE_WIDTH - BLOCK_SIZE, by, bx;                \
        for (by = 0; by < BLOCK_SIZE; by++) {                                                                    \
            for (bx = 0; bx < BLOCK_SIZE; bx++) {                                                                \
                DEST[DEST_INDEX] = OP;                                                                           \
                SOURCE_INDEX++;                                                                                  \
                DEST_INDEX++;                                                                                    \
            }                                                                                                    \
            SOURCE_INDEX += source_scan;                                                                         \
            DEST_INDEX += dest_scan;                                                                             \
        }                                                                                                        \
    } while (0)

static uint8_t clamp8(int n) {
    return (uint8_t)(n > 255 ? 255 : n < 0 ? 0 : n);
}

static void process_macroblock(M2v *d, const uint8_t *s, uint8_t *dst, int motion_h, int motion_v, int bs,
                               int interpolate) {
    int dw = d->mb_width * bs;
    int hp = motion_h >> 1, vp = motion_v >> 1;
    int odd_h = motion_h & 1, odd_v = motion_v & 1;
    unsigned si = (unsigned)(((d->mb_row * bs) + vp) * dw + (d->mb_col * bs) + hp);
    unsigned di = (unsigned)((d->mb_row * dw + d->mb_col) * bs);
    unsigned max_address = (unsigned)(dw * (d->mb_height * bs - bs + 1) - bs);
    if (si > max_address || di > max_address) {
        return; /* a vector out of the picture: corrupt */
    }
    switch (interpolate << 2 | odd_h << 1 | odd_v) {
    case 0: BLOCK_SET(dst, di, dw, si, dw, bs, s[si]); break;
    case 1: BLOCK_SET(dst, di, dw, si, dw, bs, (s[si] + s[si + dw] + 1) >> 1); break;
    case 2: BLOCK_SET(dst, di, dw, si, dw, bs, (s[si] + s[si + 1] + 1) >> 1); break;
    case 3: BLOCK_SET(dst, di, dw, si, dw, bs, (s[si] + s[si + 1] + s[si + dw] + s[si + dw + 1] + 2) >> 2); break;
    case 4: BLOCK_SET(dst, di, dw, si, dw, bs, (dst[di] + s[si] + 1) >> 1); break;
    case 5: BLOCK_SET(dst, di, dw, si, dw, bs, (dst[di] + ((s[si] + s[si + dw] + 1) >> 1) + 1) >> 1); break;
    case 6: BLOCK_SET(dst, di, dw, si, dw, bs, (dst[di] + ((s[si] + s[si + 1] + 1) >> 1) + 1) >> 1); break;
    case 7:
        BLOCK_SET(dst, di, dw, si, dw, bs,
                  (dst[di] + ((s[si] + s[si + 1] + s[si + dw] + s[si + dw + 1] + 2) >> 2) + 1) >> 1);
        break;
    }
}

static void copy_from(M2v *d, const Planes *s, int mh, int mv, int interpolate) {
    /* 4:2:0 chroma vectors: the luma vector / 2, truncated toward zero */
    process_macroblock(d, s->y, d->cur.y, mh, mv, 16, interpolate);
    process_macroblock(d, s->cr, d->cur.cr, mh / 2, mv / 2, 8, interpolate);
    process_macroblock(d, s->cb, d->cur.cb, mh / 2, mv / 2, 8, interpolate);
}

static void predict_macroblock(M2v *d) {
    if (d->picture_type == PIC_B) {
        if (d->mf.is_set) {
            copy_from(d, &d->fwd, d->mf.h, d->mf.v, 0);
            if (d->mb.is_set) {
                copy_from(d, &d->bwd, d->mb.h, d->mb.v, 1);
            }
        } else {
            copy_from(d, &d->bwd, d->mb.h, d->mb.v, 0);
        }
    } else {
        copy_from(d, &d->fwd, d->mf.h, d->mf.v, 0);
    }
}

/* ---- blocks: MPEG-2 inverse quantisation (7.4), then PL_MPEG's IDCT ---- */

static void decode_block(M2v *d, int block) {
    int *F = d->block;
    const uint8_t *W;
    int n = 0, sum = 0, dc_only, i, intra = d->mb_intra, b15 = intra && d->intra_vlc_format;
    int k = intra ? 0 : 1;
    uint8_t *dst;
    int dw, di, si = 0;

    if (intra) {
        int plane = block > 3 ? block - 3 : 0;
        int size = read_vlc(d, M2V_DCT_SIZE[plane]);
        int dc = d->dc_predictor[plane];
        if (size > 0) {
            int diff = read_bits(d, size);
            dc += (diff & (1 << (size - 1))) ? diff : (-(1 << size) | (diff + 1));
        }
        d->dc_predictor[plane] = dc;
        F[0] = dc << 3; /* intra_dc_mult 8 (intra_dc_precision 0) */
        sum = F[0];
        W = d->intra_quant;
        n = 1;
    } else {
        W = d->non_intra_quant;
    }

    for (;;) {
        int run, level, idx, v;
        unsigned coeff;
        if (b15) {
            coeff = read_vlc_uint(d, d->b15);
            if (coeff == B15_EOB) {
                break;
            }
        } else {
            coeff = read_vlc_uint(d, M2V_DCT_COEFF);
            if (coeff == 0x0001 && n > 0 && read_bits(d, 1) == 0) {
                break; /* end_of_block '10' */
            }
        }
        if (coeff == ESCAPE) { /* MPEG-2: 6-bit run, 12-bit signed level */
            run = read_bits(d, 6);
            level = read_bits(d, 12);
            if (level & 0x800) {
                level -= 0x1000;
            }
        } else {
            run = (int)(coeff >> 8);
            level = (int)(coeff & 0xFF);
            if (read_bits(d, 1)) {
                level = -level;
            }
        }
        n += run;
        if (n < 0 || n >= 64 || level == 0) {
            break; /* corrupt */
        }
        idx = ZIG_ZAG[n++];
        v = ((2 * abs(level) + k) * W[idx] * d->quantizer_scale) / 32;
        if (level < 0) {
            v = -v;
        }
        if (v > 2047) {
            v = 2047;
        } else if (v < -2048) {
            v = -2048;
        }
        F[idx] = v;
        sum += v;
    }
    if ((sum & 1) == 0) { /* mismatch control: the last coefficient's lowest bit toggled */
        F[63] ^= 1;
    }
    dc_only = 1;
    for (i = 0; i < 64; i++) {
        if (F[i] != 0) {
            F[i] *= PREMULTIPLIER[i];
            if (i != 0) {
                dc_only = 0;
            }
        }
    }

    if (block < 4) {
        dst = d->cur.y;
        dw = d->luma_width;
        di = (d->mb_row * d->luma_width + d->mb_col) << 4;
        if (block & 1) {
            di += 8;
        }
        if (block & 2) {
            di += d->luma_width << 3;
        }
    } else {
        dst = block == 4 ? d->cur.cb : d->cur.cr;
        dw = d->chroma_width;
        di = ((d->mb_row * d->luma_width) << 2) + (d->mb_col << 3);
    }
    if (dc_only) {
        int value = (F[0] + 128) >> 8;
        if (intra) {
            BLOCK_SET(dst, di, dw, si, 8, 8, clamp8(value));
        } else {
            BLOCK_SET(dst, di, dw, si, 8, 8, clamp8(dst[di] + value));
        }
    } else {
        idct(F);
        if (intra) {
            BLOCK_SET(dst, di, dw, si, 8, 8, clamp8(F[si]));
        } else {
            BLOCK_SET(dst, di, dw, si, 8, 8, clamp8(dst[di] + F[si]));
        }
    }
    memset(d->block, 0, sizeof(d->block));
}

static void idct(int *block) {
    int b1, b3, b4, b6, b7, tmp1, tmp2, m0, x0, x1, x2, x3, x4, y3, y4, y5, y6, y7, i;

    for (i = 0; i < 8; ++i) { /* columns */
        b1 = block[4 * 8 + i];
        b3 = block[2 * 8 + i] + block[6 * 8 + i];
        b4 = block[5 * 8 + i] - block[3 * 8 + i];
        tmp1 = block[1 * 8 + i] + block[7 * 8 + i];
        tmp2 = block[3 * 8 + i] + block[5 * 8 + i];
        b6 = block[1 * 8 + i] - block[7 * 8 + i];
        b7 = tmp1 + tmp2;
        m0 = block[0 * 8 + i];
        x4 = ((b6 * 473 - b4 * 196 + 128) >> 8) - b7;
        x0 = x4 - (((tmp1 - tmp2) * 362 + 128) >> 8);
        x1 = m0 - b1;
        x2 = (((block[2 * 8 + i] - block[6 * 8 + i]) * 362 + 128) >> 8) - b3;
        x3 = m0 + b1;
        y3 = x1 + x2;
        y4 = x3 + b3;
        y5 = x1 - x2;
        y6 = x3 - b3;
        y7 = -x0 - ((b4 * 473 + b6 * 196 + 128) >> 8);
        block[0 * 8 + i] = b7 + y4;
        block[1 * 8 + i] = x4 + y3;
        block[2 * 8 + i] = y5 - x0;
        block[3 * 8 + i] = y6 - y7;
        block[4 * 8 + i] = y6 + y7;
        block[5 * 8 + i] = x0 + y5;
        block[6 * 8 + i] = y3 - x4;
        block[7 * 8 + i] = y4 - b7;
    }
    for (i = 0; i < 64; i += 8) { /* rows */
        b1 = block[4 + i];
        b3 = block[2 + i] + block[6 + i];
        b4 = block[5 + i] - block[3 + i];
        tmp1 = block[1 + i] + block[7 + i];
        tmp2 = block[3 + i] + block[5 + i];
        b6 = block[1 + i] - block[7 + i];
        b7 = tmp1 + tmp2;
        m0 = block[0 + i];
        x4 = ((b6 * 473 - b4 * 196 + 128) >> 8) - b7;
        x0 = x4 - (((tmp1 - tmp2) * 362 + 128) >> 8);
        x1 = m0 - b1;
        x2 = (((block[2 + i] - block[6 + i]) * 362 + 128) >> 8) - b3;
        x3 = m0 + b1;
        y3 = x1 + x2;
        y4 = x3 + b3;
        y5 = x1 - x2;
        y6 = x3 - b3;
        y7 = -x0 - ((b4 * 473 + b6 * 196 + 128) >> 8);
        block[0 + i] = (b7 + y4 + 128) >> 8;
        block[1 + i] = (x4 + y3 + 128) >> 8;
        block[2 + i] = (y5 - x0 + 128) >> 8;
        block[3 + i] = (y6 - y7 + 128) >> 8;
        block[4 + i] = (y6 + y7 + 128) >> 8;
        block[5 + i] = (x0 + y5 + 128) >> 8;
        block[6 + i] = (y3 - x4 + 128) >> 8;
        block[7 + i] = (y4 - b7 + 128) >> 8;
    }
}
