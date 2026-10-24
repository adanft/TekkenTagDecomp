/* The SPU2 model (port/src/spu2.c): 2 MB of sound RAM, two cores of 24 voices, PS-ADPCM with loop flags, pitch,
   ADSR, fixed volumes, 48 kHz stereo output. Integer arithmetic only; the state advances only through Spu2_Mix, so it
   is a function of the commands and of the number of samples mixed (docs/port/sound.md). */
#ifndef SPU2_H
#define SPU2_H
#include <stdint.h>

#define SPU2_RAM_SIZE (2u * 1024u * 1024u)
#define SPU2_VOICES 24
#define SPU2_RATE 48000

/* Envelope phases */
enum { SPU2_OFF, SPU2_ATTACK, SPU2_DECAY, SPU2_SUSTAIN, SPU2_RELEASE };

typedef struct {
    uint32_t ssa, lsa, nax; /* start, loop and current block addresses (bytes, multiples of 16) */
    uint32_t counter;       /* pitch counter: sample index in the block << 12 | fraction */
    uint16_t pitch;         /* 0x1000 = 48 kHz */
    uint16_t adsr1, adsr2;
    uint16_t volL, volR;    /* the VOLL / VOLR registers (fixed volume: bit 15 clear, 15-bit signed volume / 2) */
    int32_t envx;           /* 0..0x7FFF */
    uint32_t envCount;
    uint8_t phase;
    uint8_t flags;          /* the current block's loop flags (bit 0 end, 1 repeat, 2 loop start) */
    uint8_t keyed;          /* libspu2's key shadow: 1 after key on, 0 after key off */
    uint8_t lsaSet;         /* the loop address was written: loop-start flags no longer move it */
    int16_t hist1, hist2;   /* ADPCM predictor history */
    int16_t d[30];          /* d[1]: the previous block's last sample, d[2..29]: the current block */
} Spu2Voice;

typedef struct {
    Spu2Voice v[SPU2_VOICES];
    uint16_t mvolL, mvolR;  /* master volume registers (same format as the voices') */
    int16_t avolL, avolR;   /* core 1 only: the volume of core 0's output in core 1's mix (0x7FFF = 1) */
    uint32_t endx;          /* voices that passed a block with the end flag since their key on */
} Spu2Core;

extern Spu2Core gSpu2[2];
uint8_t *Spu2_Ram(void);
void Spu2_Reset(void);
void Spu2_KeyOn(int core, uint32_t mask);
void Spu2_KeyOff(int core, uint32_t mask);
/* libspu2's key status of one voice: 0 off, 1 on, 2 off with the envelope still sounding, 3 on with the envelope at
   zero (the sample ended or the release finished while the key shadow is on). */
int Spu2_KeyStatus(int core, int voice);
/* Mixes `n` stereo frames (interleaved L, R) into `out` (NULL: advance the voices only). */
void Spu2_Mix(int16_t *out, int n);
/* Decodes one 16-byte PS-ADPCM block into 28 samples with the predictor history; returns the block's flags byte. */
int Spu2_DecodeBlock(const uint8_t *blk, int16_t *out, int16_t *h1, int16_t *h2);
/* TTT_SELFTEST=spu: synthetic ADPCM blocks through the decoder and the voices; 0 when every check passes. */
int Spu2_SelfTest(void);

#endif
