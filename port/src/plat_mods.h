/* Mods (T49, docs/port/mods.md): folders of replacement music and textures under the mods folder, in a load order the
   player chooses. Host side only: nothing here is game state, nothing of the game reads it (plat_mods.c). */
#ifndef PLAT_MODS_H
#define PLAT_MODS_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODS_MAX 64
#define MODS_TRACKS 28 /* the BGM tracks the game registers (0x205E, plat_bgm.c) */

typedef struct {
    char name[128];
    int enabled;
    uint32_t music;    /* the tracks it has a file for (bit n: track n) */
    uint32_t shadowed; /* of those, the tracks an earlier enabled mod also replaces (that one plays) */
    int textures;      /* it has a textures/ folder */
    int texShadowed;   /* replacement textures of it that an earlier enabled mod (or the texture pack) also has */
    int texFiles;      /* replacement textures found in it (after the texture worker's scan) */
} ModInfo;

const char *Mods_Folder(void);       /* mods.dir (TTT_MODS), else mods/ in the player's data folder */
int Mods_Count(void);                /* the mods found, in load order (the first wins a conflict) */
int Mods_Get(int i, ModInfo *out);   /* 1 when i is a mod */
/* The player's choices, written to the settings file at once (mods.order, mods.disabled). Music follows them at once
   (the track playing switches); textures at the next start. */
void Mods_SetEnabled(int i, int on);
void Mods_Move(int i, int delta);
void Mods_Rescan(void);              /* reads the mods folder again */

/* Textures (gs_texpack.c): the enabled mods' textures/ folders, highest priority first, when mods.textures is on. */
int Mods_TexturesActive(void);
int Mods_TextureDirs(char (*dirs)[1100], int *mod, int max);
void Mods_NoteTextures(int mod, int files, int shadowed); /* the worker's counts, for the list */

/* Music (plat_snd.c, the game's thread, after the SPU2 model mixed `n` frames): the music bus of `bus` (four values a
   frame: whole L, R, music L, R; Spu2_MixBus) replaced by the mod's file for the track the game plays, if any. 1 when
   it changed `bus`. `realtime`: the window plays the output (files then load in the background). */
int Mods_MusicMix(int32_t *bus, int n, int realtime);
/* The track the game plays now (-1 none) and the mod file replacing it ("" none), for the overlay. */
int Mods_MusicNow(char *file, int n);
/* One line for netplay's handshake and the logs: which mods are active (information only: mods are cosmetic). */
void Mods_Summary(char *out, int n);

void Mods_SelfTest(void); /* TTT_SELFTEST=mods */

#ifdef __cplusplus
}
#endif
#endif
