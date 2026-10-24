/* A small MPEG-2 video decoder (main profile, 4:2:0, frame pictures with frame prediction and frame DCT, which is
   what the PlayStation 2's movie encoders wrote), derived from PL_MPEG's MPEG-1 decoder (MIT, see LICENSE). Integer
   arithmetic only. Field pictures, field prediction and dual prime are refused (m2v_decode returns -1).

   Use: m2v_create(load, user); load(d, user) is called whenever the decoder needs more of the elementary stream and
   answers with m2v_write (any amount) or m2v_end. m2v_decode gives the pictures in display order. */
#ifndef M2V_H
#define M2V_H
#include <stdint.h>

typedef struct M2v M2v;
typedef void (*M2vLoad)(M2v *d, void *user);

typedef struct {
    int width, height;  /* from the sequence header */
    int stride, cstride; /* bytes per row of the luma / chroma planes (macroblock-aligned) */
    const uint8_t *y, *cb, *cr;
    int type;            /* picture_coding_type: 1 I, 2 P, 3 B */
} M2vFrame;

M2v *m2v_create(M2vLoad load, void *user);
void m2v_destroy(M2v *d);
void m2v_write(M2v *d, const uint8_t *bytes, int n);
void m2v_end(M2v *d);
/* The next picture in display order: 1 (its planes stay valid until the next call), 0 at the end of the stream,
   -1 for a stream this decoder does not support. */
int m2v_decode(M2v *d, M2vFrame *out);

#endif
