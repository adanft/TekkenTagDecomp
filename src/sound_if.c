/* 0x399FC8-0x39A2B8: the sound byte-code queue sent to RSPU2DRV (command 0x7F0F) for BGM and movie audio.
 * docs/systems/audio.md section 4. */
#include "common.h"

/* The 0x30 bytes of the queue the IOP reads (it stops at the first 0xFF). */
typedef struct {
    u64 w[6];
} SndQueue;

extern s32 gSndDriverReady;
extern s32 gSndQueueCount;
extern u8 *gSndQueuePtr;
extern SndQueue gSndQueue;
extern u16 D_005F5C76;
/* BGM volume per option level 0-10, read from the original executable. */
extern const u16 D_00518E58[11];

s32 sceSdRemote(s32 arg, s32 command, ...);
u32 File_SifCmdBufferAddr(void);
s32 Iop_BgmBusy(void);
void Iop_ClearEeStreamBase(void);
void Bgm_SetVolumeLevel(s32 level);

void Snd_Nop(void) {
}

void func_00399FD0(void) {
}

/* Boot: starts the IOP stream engine once (command 0x2000) and empties the queue. */
void Snd_InitDriver(void) {
    if (gSndDriverReady == 0) {
        gSndQueueCount = 0;
        gSndDriverReady = 1;
        sceSdRemote(1, 0x2000, 0x2237, File_SifCmdBufferAddr(), 0x446E);
    }
}

/* Sends the queued byte codes, ended by 0xFF, and empties the queue. */
void Snd_Flush(void) {
    SndQueue queue;

    if (gSndDriverReady != 0) {
        if (gSndQueueCount > 0) {
            *gSndQueuePtr = 0xFF;
            queue = gSndQueue;
            sceSdRemote(1, 0x7F0F, &queue);
        }
        gSndQueueCount = 0;
        gSndQueuePtr = (u8 *)&gSndQueue;
    }
}

/* Queues BGM track `track` (code 0x40) at the option volume. */
void Bgm_Play(s32 track) {
    Bgm_SetVolumeLevel(D_005F5C76);
    *gSndQueuePtr++ = 0x40;
    *gSndQueuePtr++ = track;
    gSndQueueCount += 2;
}

void Bgm_PlayFlag(s32 track) {
    Bgm_Play(track | 0x80);
}

s32 Bgm_IsPlaying(void) {
    return Iop_BgmBusy();
}

void Bgm_Stop(void) {
    *gSndQueuePtr++ = 0x43;
    gSndQueueCount += 1;
    Iop_ClearEeStreamBase();
}

/* Code 0x44: fades the BGM volume to `volume` over `time` * 4 ticks. */
void Bgm_SetVolumeFade(s32 volume, s32 time) {
    *gSndQueuePtr++ = 0x44;
    *gSndQueuePtr++ = volume;
    *gSndQueuePtr++ = time;
    gSndQueueCount += 3;
}

/* Code 0x45: BGM volume of option level `level` (0-10). */
void Bgm_SetVolumeLevel(s32 level) {
    if (level < 0) {
        level = 0;
    } else if (level > 10) {
        level = 10;
    }
    *gSndQueuePtr++ = 0x45;
    *gSndQueuePtr++ = D_00518E58[level];
    gSndQueueCount += 2;
}

/* Codes 0x50 / 0x51: stereo on / off. */
void Snd_SetStereo(s32 on) {
    if (on) {
        *gSndQueuePtr++ = 0x50;
    } else {
        *gSndQueuePtr++ = 0x51;
    }
    gSndQueueCount += 1;
}

/* Code 0x5C: plays the audio of movie `movie` at the option volume. */
s32 Movie_PlayAudio(s32 movie) {
    Bgm_SetVolumeLevel(D_005F5C76);
    *gSndQueuePtr++ = 0x5C;
    *gSndQueuePtr++ = movie;
    gSndQueueCount += 2;
    return 0;
}

s32 Bgm_Abort(void) {
    *gSndQueuePtr++ = 0x5D;
    gSndQueueCount += 1;
    return 0;
}
