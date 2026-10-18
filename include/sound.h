#ifndef SOUND_H
#define SOUND_H

#include "types.h"

/* libspu2 structures, as the RSPU2DRV commands take them (docs/systems/audio.md section 3). */
typedef struct {
    s16 left;
    s16 right;
} SpuVolume;

typedef struct {
    /* 0x0 */ SpuVolume volume;
    /* 0x4 */ s32 reverb;
    /* 0x8 */ s32 mix;
} SpuExtAttr;

/* SpuSetCommonAttr (command 0x7128). */
typedef struct {
    /* 0x00 */ u32 mask;
    /* 0x04 */ SpuVolume mvol;
    /* 0x08 */ SpuVolume mvolmode;
    /* 0x0C */ SpuVolume mvolx;
    /* 0x10 */ SpuExtAttr cd;
    /* 0x1C */ SpuExtAttr ext;
} SpuCommonAttr; /* size 0x28 */

/* SpuSetVoiceAttr / SpuSetKeyOnWithAttr (commands 0x7240, 0x7440). */
typedef struct {
    /* 0x00 */ u32 voice;      /* bit per voice */
    /* 0x04 */ u32 mask;       /* fields to set */
    /* 0x08 */ SpuVolume volume;
    /* 0x0C */ SpuVolume volmode;
    /* 0x10 */ SpuVolume volumex;
    /* 0x14 */ u16 pitch;      /* 0x1000 = 48 kHz */
    /* 0x16 */ u16 note;
    /* 0x18 */ u16 sampleNote;
    /* 0x1A */ s16 envx;
    /* 0x1C */ u32 addr;       /* SPU2 RAM address of the sample */
    /* 0x20 */ u32 loopAddr;
    /* 0x24 */ s32 aMode;
    /* 0x28 */ s32 sMode;
    /* 0x2C */ s32 rMode;
    /* 0x30 */ u16 ar;
    /* 0x32 */ u16 dr;
    /* 0x34 */ u16 sr;
    /* 0x36 */ u16 rr;
    /* 0x38 */ u16 sl;
    /* 0x3A */ u16 adsr1;
    /* 0x3C */ u16 adsr2;
} SpuVoiceAttr; /* size 0x40 */

/* One sound effect of a bank: `offset` of its VAG from the bank start. */
typedef struct {
    /* 0x0 */ u32 offset;
    /* 0x4 */ u16 pitch;
    /* 0x6 */ u8 volume;
    /* 0x7 */ u8 category;
} SndSe;

/* An entry of the SE bank table gSndBankDefs (62 banks): ids first..first+count-1. */
typedef struct {
    /* 0x0 */ SndSe *se;
    /* 0x4 */ u32 unk4;
    /* 0x8 */ u16 kind;   /* 0, 1: fixed SPU2 address; 2: a character bank (Snd_GetBankAddr) */
    /* 0xA */ u16 first;
    /* 0xC */ u16 count;
    /* 0xE */ u16 unkE;
} SndBankDef;

/* A sound effect voice (gSndVoices, voices 0-21 of SPU2 core 1). */
typedef struct {
    /* 0x0 */ u16 category;  /* 0 = free */
    /* 0x2 */ u16 id;
    /* 0x4 */ s16 player;
    /* 0x6 */ u16 volume;
} SndVoice;

#define SND_VOICE_COUNT 22

/* A character bank in SPU2 RAM (4 slots of 0x1C900 bytes). */
typedef struct {
    /* 0x0 */ s32 owner;
    /* 0x4 */ u32 addr;
} SndBank;

/* A delayed sound effect (24 slots at gSndDelayed). */
typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 delay;  /* frames left; 0 = free */
    /* 0x4 */ u16 id;
    /* 0x6 */ u16 player;
} SndDelayed;

/* A step of the delayed sound effect table D_005007D8: the effect plays again at `scale` when `delay` is reached. */
typedef struct {
    /* 0x0 */ s16 delay;
    /* 0x2 */ s16 scale;
} SndDelayStep;

/* A fighter's voice set (D_00501808, 16 bytes per set). `seq` holds four counts, then the voice ids of each
 * kind; `unk4` / `unk8`: a count, then ids. */
typedef struct {
    /* 0x0 */ s16 *seq;
    /* 0x4 */ s16 *unk4;
    /* 0x8 */ s16 *unk8;
    /* 0xC */ s16 unkC;
    /* 0xE */ s16 unkE;
} SndVoiceSet;

/* A step of a fighter voice sequence (D_00501A78): at frame `time` run `op` with `arg`; time 0 ends it. */
typedef struct {
    /* 0x0 */ u8 time;
    /* 0x1 */ u8 op;
    /* 0x2 */ u16 arg;
} SndVoiceStep;

#endif
