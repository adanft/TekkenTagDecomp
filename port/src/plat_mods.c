/*
 * Mods (T49, docs/port/mods.md): replacement music and textures dropped in folders.
 *
 *   <mods>/<name>/NN.wav, NN.ogg, music/NN.wav ...   the music of BGM track NN (0-27, plat_bgm.c's track table)
 *   <mods>/<name>/textures/.../<texture>.png         replacement textures, PCSX2 names (gs_texpack.c, T39)
 *
 * <mods> is mods.dir (TTT_MODS), else mods/ in the player's data folder (plat_paths.c: beside the program when
 * portable, the repository's mods/ in a checkout, else $XDG_DATA_HOME/ttt/mods). Every folder in it is a mod. The
 * load order is mods.order (names, commas between them; mods not in it follow in name order), the disabled ones are in
 * mods.disabled; the first enabled mod that has a file wins a conflict, as the first replacement of a name wins in a
 * texture pack. The overlay's Mods page changes both (written to the settings file at once).
 *
 * Music: the game still plays its stream. The SPU2 model, the BGM engine (plat_bgm.c) and everything the game reads
 * back run as without mods; only the host's output changes. After each piece of VBlank the model mixed, plat_snd.c
 * hands its two buses (Spu2_MixBus: the whole mix and the part of core 0's voices 22-23) here: while the game plays
 * track n (engine state 4) and a mod has a file for n, the music part is taken out of the mix and the file's samples
 * put in instead, at the game's music volume and fades (option volume * fade, then core 0's master, core 1's AVOL and
 * master, as the stream voices go through), in stereo or the game's mono. Files are decoded whole (WAV PCM 8 / 16 /
 * 24 / 32-bit; Ogg Vorbis through mods_vorbis.cpp), resampled to 48 kHz by linear interpolation (integer steps of
 * rate / 48000, no drift), and loop: at a WAV `smpl` loop, the Vorbis LOOPSTART / LOOPLENGTH comments or a sidecar
 * NN.loop file ("start end" in the file's frames, or "once"), else over the whole file when the game's track loops.
 * With a window (`realtime`) a file is decoded by a background thread while the track plays on in silence, then joins
 * at the time the track has reached; headless runs (TTT_WAV) decode at once, so their output is the same each run.
 *
 * None of it is game state: nothing here is in a state (plat_state.c), so the state hash, the rollback and netplay
 * never see a mod. Mods only change what the player hears and sees; Mods_Summary says which are on, for the handshake
 * as information. Integer only, like the rest of the port's C. C library calls through the GOT (no new PLT entry).
 */
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "port_addr.h"
#include "port_host.h"
#include "plat_mods.h"
#include "plat_settings.h"
#include "spu2.h"

/* C library calls through the GOT: no new PLT entry (port_host.h) */
#define PORT_NOPLT(f) __typeof__(f) f __attribute__((noplt))
PORT_NOPLT(opendir);
PORT_NOPLT(readdir);
PORT_NOPLT(closedir);
PORT_NOPLT(stat);
PORT_NOPLT(strncmp);
PORT_NOPLT(strchr);
PORT_NOPLT(pthread_cond_signal);
#undef PORT_NOPLT

void Port_UserFolder(const char *leaf, char *out, int n); /* plat_paths.c */
uint32_t Port_VBlankCount(void);                           /* plat_hw.c */
int Port_BgmNow(int *gain, int *stereo, int *loops);       /* plat_bgm.c: read-only */
int Port_BgmTrackInfo(int i, int *type, int *count, uint32_t *bytes);
int Mods_DecodeVorbis(const uint8_t *data, int len, uint32_t maxFrames, int16_t **out, uint32_t *frames,
                      uint32_t *rate, uint32_t *loopStart, uint32_t *loopEnd); /* mods_vorbis.cpp */

#define OUT_RATE 48000u
#define MAX_FRAMES (30u * 1000u * 1000u) /* about 10 minutes at 48 kHz: 120 MB of samples */
#define MAX_FILE (256l << 20)
#define STAGE_TRACKS 0x004FB9C0u /* the game's stage -> BGM track table (s16, battle_mode.c), read from the disc's data */

/* ------------------------------------------------------------------------------------------------ the mod list */

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static char sRoot[1024];
static ModInfo sMods[MODS_MAX];
static int sN, sScanned, sGen = 1;
static char sTrackPath[MODS_TRACKS][1200]; /* the winning file of each track ("" none) */
static int sTrackMod[MODS_TRACKS];

static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* The track of a music file name: 1 or 2 digits (0-27) not followed by a digit, then anything, ending in .wav or
   .ogg (any case): "07.ogg", "7.wav", "21 - stage.ogg". -1 otherwise. */
static int track_of(const char *name) {
    const char *ext = strrchr(name, '.');
    int t = 0, k = 0;
    while (name[k] >= '0' && name[k] <= '9' && k < 3) {
        t = t * 10 + (name[k] - '0');
        k++;
    }
    if (k == 0 || k > 2 || t >= MODS_TRACKS || ext == NULL || strlen(ext) != 4) {
        return -1;
    }
    if ((lower(ext[1]) == 'w' && lower(ext[2]) == 'a' && lower(ext[3]) == 'v') ||
        (lower(ext[1]) == 'o' && lower(ext[2]) == 'g' && lower(ext[3]) == 'g')) {
        return t;
    }
    return -1;
}

/* The music files of one folder into `paths` (the smaller name wins when a track has several, so the choice does not
   depend on the directory's order); their tracks as a mask. */
static uint32_t scan_music(const char *dir, char (*paths)[1200]) {
    DIR *d = opendir(dir);
    struct dirent *e;
    uint32_t mask = 0;
    if (d == NULL) {
        return 0;
    }
    while ((e = readdir(d)) != NULL) {
        char p[1200];
        int t = e->d_name[0] == '.' ? -1 : track_of(e->d_name);
        if (t < 0) {
            continue;
        }
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        if (is_dir(p)) {
            continue;
        }
        if (!paths[t][0] || strcmp(p, paths[t]) < 0) {
            memcpy(paths[t], p, sizeof p);
        }
        mask |= 1u << t;
    }
    closedir(d);
    return mask;
}

/* Is `name` in the comma-separated `list` (spaces around a name ignored)? Its place, or -1. */
static int list_find(const char *list, const char *name) {
    size_t n = strlen(name);
    int place = 0;
    const char *p = list;
    while (*p) {
        const char *end = strchr(p, ','), *a = p, *b;
        if (end == NULL) {
            end = p + strlen(p);
        }
        b = end;
        while (a < b && *a == ' ') {
            a++;
        }
        while (b > a && b[-1] == ' ') {
            b--;
        }
        if ((size_t)(b - a) == n && strncmp(a, name, n) == 0) {
            return place;
        }
        place++;
        p = *end ? end + 1 : end;
    }
    return -1;
}

static void folder(void) {
    if (sRoot[0]) {
        return;
    }
    Settings_GetStr("mods.dir", sRoot, sizeof sRoot);
    if (!sRoot[0]) {
        Port_UserFolder("mods", sRoot, sizeof sRoot);
    }
}

/* The winners of every track, and each mod's shadowed tracks, from the list in order. With sLock held. */
static void resolve_locked(void) {
    static char paths[MODS_TRACKS][1200];
    int i, t;
    for (t = 0; t < MODS_TRACKS; t++) {
        sTrackPath[t][0] = 0;
        sTrackMod[t] = -1;
    }
    for (i = 0; i < sN; i++) {
        ModInfo *m = &sMods[i];
        char dir[1200];
        m->shadowed = 0;
        if (!m->enabled || m->music == 0) {
            continue;
        }
        memset(paths, 0, sizeof paths);
        snprintf(dir, sizeof dir, "%s/%s", sRoot, m->name);
        scan_music(dir, paths);
        snprintf(dir, sizeof dir, "%s/%s/music", sRoot, m->name);
        scan_music(dir, paths);
        for (t = 0; t < MODS_TRACKS; t++) {
            if (!paths[t][0]) {
                continue;
            }
            if (sTrackPath[t][0]) {
                m->shadowed |= 1u << t;
            } else {
                memcpy(sTrackPath[t], paths[t], sizeof sTrackPath[t]);
                sTrackMod[t] = i;
            }
        }
    }
    sGen++;
}

/* Reads the mods folder: every sub-folder, ordered by mods.order then by name, enabled unless in mods.disabled. */
static void scan_locked(void) {
    static char paths[MODS_TRACKS][1200];
    char order[1024], disabled[1024], dir[1200];
    DIR *d;
    struct dirent *e;
    int i, k;
    folder();
    Settings_GetStr("mods.order", order, sizeof order);
    Settings_GetStr("mods.disabled", disabled, sizeof disabled);
    sN = 0;
    d = opendir(sRoot);
    while (d != NULL && (e = readdir(d)) != NULL && sN < MODS_MAX) {
        ModInfo *m;
        if (e->d_name[0] == '.' || strchr(e->d_name, ',') != NULL || strlen(e->d_name) >= sizeof sMods[0].name) {
            continue; /* hidden, or a name the comma lists cannot hold */
        }
        snprintf(dir, sizeof dir, "%s/%s", sRoot, e->d_name);
        if (!is_dir(dir)) {
            continue;
        }
        m = &sMods[sN++];
        memset(m, 0, sizeof *m);
        snprintf(m->name, sizeof m->name, "%s", e->d_name);
        m->enabled = list_find(disabled, m->name) < 0;
        memset(paths, 0, sizeof paths);
        m->music = scan_music(dir, paths);
        snprintf(dir, sizeof dir, "%s/%s/music", sRoot, e->d_name);
        m->music |= scan_music(dir, paths);
        snprintf(dir, sizeof dir, "%s/%s/textures", sRoot, e->d_name);
        m->textures = is_dir(dir);
    }
    if (d != NULL) {
        closedir(d);
    }
    /* insertion sort: the place in mods.order first (listed before unlisted), then the name */
    for (i = 1; i < sN; i++) {
        ModInfo m = sMods[i];
        int pm = list_find(order, m.name);
        for (k = i - 1; k >= 0; k--) {
            int pk = list_find(order, sMods[k].name);
            int after = pk < 0 && pm < 0 ? strcmp(sMods[k].name, m.name) > 0
                        : pk < 0         ? 1
                        : pm < 0         ? 0
                                         : pk > pm;
            if (!after) {
                break;
            }
            sMods[k + 1] = sMods[k];
        }
        sMods[k + 1] = m;
    }
    sScanned = 1;
    resolve_locked();
}

static void ensure(void) {
    pthread_mutex_lock(&sLock);
    if (!sScanned) {
        scan_locked();
    }
    pthread_mutex_unlock(&sLock);
}

const char *Mods_Folder(void) {
    pthread_mutex_lock(&sLock);
    folder();
    pthread_mutex_unlock(&sLock);
    return sRoot;
}

int Mods_Count(void) {
    ensure();
    return sN;
}

int Mods_Get(int i, ModInfo *out) {
    int ok;
    ensure();
    pthread_mutex_lock(&sLock);
    ok = i >= 0 && i < sN;
    if (ok) {
        *out = sMods[i];
    }
    pthread_mutex_unlock(&sLock);
    return ok;
}

/* mods.order and mods.disabled from the list. With sLock held. */
static void save_locked(void) {
    char order[1024], disabled[1024];
    int i, a = 0, b = 0;
    order[0] = disabled[0] = 0;
    for (i = 0; i < sN; i++) {
        if (a < (int)sizeof order - 1) {
            a += snprintf(order + a, sizeof order - (size_t)a, "%s%s", a ? "," : "", sMods[i].name);
        }
        if (!sMods[i].enabled && b < (int)sizeof disabled - 1) {
            b += snprintf(disabled + b, sizeof disabled - (size_t)b, "%s%s", b ? "," : "", sMods[i].name);
        }
    }
    Settings_SetStr("mods.order", order);
    Settings_SetStr("mods.disabled", disabled);
}

void Mods_SetEnabled(int i, int on) {
    ensure();
    pthread_mutex_lock(&sLock);
    if (i >= 0 && i < sN && sMods[i].enabled != (on != 0)) {
        sMods[i].enabled = on != 0;
        resolve_locked();
        save_locked();
    }
    pthread_mutex_unlock(&sLock);
}

void Mods_Move(int i, int delta) {
    ensure();
    pthread_mutex_lock(&sLock);
    if (i >= 0 && i < sN && delta != 0) {
        int j = i + delta < 0 ? 0 : i + delta >= sN ? sN - 1 : i + delta;
        ModInfo m = sMods[i];
        if (j < i) {
            memmove(&sMods[j + 1], &sMods[j], (size_t)(i - j) * sizeof m);
        } else {
            memmove(&sMods[i], &sMods[i + 1], (size_t)(j - i) * sizeof m);
        }
        sMods[j] = m;
        resolve_locked();
        save_locked();
    }
    pthread_mutex_unlock(&sLock);
}

void Mods_Rescan(void) {
    pthread_mutex_lock(&sLock);
    scan_locked();
    pthread_mutex_unlock(&sLock);
}

/* ------------------------------------------------------------------------------------------------ textures */

int Mods_TexturesActive(void) {
    int i, on = 0;
    if (!Settings_GetInt("mods.textures")) {
        return 0;
    }
    ensure();
    pthread_mutex_lock(&sLock);
    for (i = 0; i < sN; i++) {
        on |= sMods[i].enabled && sMods[i].textures;
    }
    pthread_mutex_unlock(&sLock);
    return on;
}

int Mods_TextureDirs(char (*dirs)[1100], int *mod, int max) {
    int i, n = 0;
    if (!Settings_GetInt("mods.textures")) {
        return 0;
    }
    ensure();
    pthread_mutex_lock(&sLock);
    for (i = 0; i < sN && n < max; i++) {
        if (sMods[i].enabled && sMods[i].textures) {
            snprintf(dirs[n], sizeof dirs[n], "%s/%s/textures", sRoot, sMods[i].name);
            mod[n++] = i;
        }
    }
    pthread_mutex_unlock(&sLock);
    return n;
}

void Mods_NoteTextures(int mod, int files, int shadowed) {
    pthread_mutex_lock(&sLock);
    if (mod >= 0 && mod < sN) {
        sMods[mod].texFiles = files;
        sMods[mod].texShadowed = shadowed;
    }
    pthread_mutex_unlock(&sLock);
}

void Mods_Summary(char *out, int n) {
    int i, k, music = 0, tex = 0, len = 0;
    ensure();
    pthread_mutex_lock(&sLock);
    for (k = 0; k < MODS_TRACKS; k++) {
        music += sTrackPath[k][0] != 0;
    }
    music = Settings_GetInt("mods.music") ? music : 0;
    for (i = 0; i < sN; i++) {
        tex += sMods[i].enabled && sMods[i].textures;
    }
    tex = Settings_GetInt("mods.textures") ? tex : 0;
    len = snprintf(out, (size_t)n, "music %d track%s, textures %d mod%s", music, music == 1 ? "" : "s", tex,
                   tex == 1 ? "" : "s");
    for (i = 0, k = 0; i < sN && len < n - 1; i++) {
        if (sMods[i].enabled && ((music && sMods[i].music & ~sMods[i].shadowed) || (tex && sMods[i].textures))) {
            len += snprintf(out + len, (size_t)(n - len), "%s%s", k++ ? ", " : " (", sMods[i].name);
        }
    }
    if (k && len < n - 1) {
        snprintf(out + len, (size_t)(n - len), ")");
    }
    pthread_mutex_unlock(&sLock);
}

/* ------------------------------------------------------------------------------------------------ music files */

typedef struct {
    char path[1200];
    int16_t *s;            /* interleaved stereo at `rate` */
    uint32_t frames, rate;
    uint32_t loopStart, loopEnd; /* in frames; loopEnd 0: none given */
    int mode;              /* 0 the game's choice (whole file when its track loops), 1 the loop given, 2 once */
} Pcm;

static void pcm_free(Pcm *p) {
    if (p != NULL) {
        free(p->s);
        free(p);
    }
}

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }

/* One sample of a WAV's PCM data as 16 bits. */
static int32_t wav_sample(const uint8_t *p, int bits) {
    switch (bits) {
    case 8: return ((int32_t)p[0] - 128) << 8;
    case 16: return (int16_t)rd16(p);
    case 24: return (int16_t)rd16(p + 1);
    default: return (int16_t)rd16(p + 2);
    }
}

/* A RIFF WAVE file (PCM, or WAVE_FORMAT_EXTENSIBLE with PCM inside; 1 or more channels of 8 / 16 / 24 / 32-bit
   integers) into `p`, with its first `smpl` loop (start, end inclusive). 0 on success. */
static int decode_wav(const uint8_t *d, long len, Pcm *p) {
    const uint8_t *data = NULL;
    uint32_t dataLen = 0, ch = 0, bits = 0, align = 0, i;
    long at = 12;
    if (len < 44 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) {
        return -1;
    }
    while (at + 8 <= len) {
        uint32_t size = rd32(d + at + 4);
        const uint8_t *c = d + at + 8;
        long avail = len - at - 8;
        if ((long)size > avail) {
            size = (uint32_t)avail; /* a truncated last chunk: what is there */
        }
        if (memcmp(d + at, "fmt ", 4) == 0 && size >= 16) {
            uint32_t tag = rd16(c);
            if (tag == 0xFFFE && size >= 26) {
                tag = rd16(c + 24); /* the sub-format GUID's first two bytes */
            }
            if (tag != 1) {
                return -1; /* compressed or float WAVs: not read */
            }
            ch = rd16(c + 2);
            p->rate = rd32(c + 4);
            align = rd16(c + 12);
            bits = rd16(c + 14);
        } else if (memcmp(d + at, "data", 4) == 0) {
            data = c;
            dataLen = size;
        } else if (memcmp(d + at, "smpl", 4) == 0 && size >= 36 + 24 && rd32(c + 28) > 0) {
            p->loopStart = rd32(c + 36 + 8);
            p->loopEnd = rd32(c + 36 + 12) + 1;
            p->mode = 1;
        }
        at += 8 + (long)size + (size & 1);
    }
    if (data == NULL || ch == 0 || (bits != 8 && bits != 16 && bits != 24 && bits != 32) || align < ch * bits / 8 ||
        p->rate < 4000 || p->rate > 192000) {
        return -1;
    }
    p->frames = dataLen / align;
    if (p->frames > MAX_FRAMES) {
        p->frames = MAX_FRAMES;
    }
    if (p->frames == 0 || (p->s = malloc((size_t)p->frames * 4)) == NULL) {
        return -1;
    }
    for (i = 0; i < p->frames; i++) {
        const uint8_t *f = data + (size_t)i * align;
        p->s[2 * i] = (int16_t)wav_sample(f, (int)bits);
        p->s[2 * i + 1] = (int16_t)wav_sample(f + (ch > 1 ? bits / 8 : 0), (int)bits);
    }
    return 0;
}

/* The sidecar NN.loop beside the file: "once" (or "none", "off"), or "start [end]" in the file's frames (end
   exclusive; 0 or absent: the end of the file). `#` starts a comment. */
static void read_sidecar(Pcm *p) {
    char path[1200], text[256];
    char *dot, *s;
    FILE *f;
    size_t n;
    uint32_t v[2] = {0, 0};
    int k = 0;
    snprintf(path, sizeof path, "%s", p->path);
    dot = strrchr(path, '.');
    if (dot == NULL || strlen(dot) + 1 > 6) {
        return;
    }
    memcpy(dot, ".loop", 6);
    if ((f = fopen(path, "rb")) == NULL) {
        return;
    }
    n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = 0;
    for (s = text; *s; s++) { /* comments to the end of their line become spaces */
        if (*s == '#') {
            while (*s && *s != '\n') {
                *s++ = ' ';
            }
            if (!*s) {
                break;
            }
        }
    }
    for (s = text; *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n'; s++) {
    }
    if (strncmp(s, "once", 4) == 0 || strncmp(s, "none", 4) == 0 || strncmp(s, "off", 3) == 0) {
        p->mode = 2;
        return;
    }
    while (*s && k < 2) {
        if (*s >= '0' && *s <= '9') {
            char *e;
            v[k++] = (uint32_t)strtoul(s, &e, 10);
            s = e;
        } else {
            s++;
        }
    }
    if (k > 0) {
        p->loopStart = v[0];
        p->loopEnd = k > 1 && v[1] > 0 ? v[1] : p->frames;
        p->mode = 1;
    }
}

/* A music file decoded whole, or NULL (with the reason printed). */
static Pcm *load(const char *path) {
    Pcm *p = calloc(1, sizeof *p);
    FILE *f;
    uint8_t *file = NULL;
    long len = 0;
    int bad = 1;
    const char *ext = strrchr(path, '.');
    if (p == NULL) {
        return NULL;
    }
    snprintf(p->path, sizeof p->path, "%s", path);
    f = fopen(path, "rb");
    if (f != NULL) {
        if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && len < MAX_FILE && fseek(f, 0, SEEK_SET) == 0 &&
            (file = malloc((size_t)len)) != NULL && fread(file, 1, (size_t)len, f) != (size_t)len) {
            free(file);
            file = NULL;
        }
        fclose(f);
    }
    if (file != NULL) {
        if (ext != NULL && lower(ext[1]) == 'o') {
            bad = Mods_DecodeVorbis(file, (int)len, MAX_FRAMES, &p->s, &p->frames, &p->rate, &p->loopStart,
                                    &p->loopEnd) != 0;
            p->mode = p->loopEnd != 0;
        } else {
            bad = decode_wav(file, len, p) != 0;
        }
        free(file);
    }
    if (bad) {
        printf("ttt: mods: %s: not read (WAV PCM 8-32 bit or Ogg Vorbis expected)\n", path);
        pcm_free(p);
        return NULL;
    }
    read_sidecar(p);
    if (p->mode == 1 && (p->loopEnd > p->frames || p->loopStart >= p->loopEnd)) {
        printf("ttt: mods: %s: loop %u-%u outside the file's %u frames, ignored\n", path, (unsigned)p->loopStart,
               (unsigned)p->loopEnd, (unsigned)p->frames);
        p->mode = 0;
    }
    return p;
}

/* ---- the loader thread (with a window: a file decodes while the game goes on) ---- */
static pthread_mutex_t sLoadLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sLoadCond = PTHREAD_COND_INITIALIZER;
static char sReqPath[1200];
static int sReqId, sDoneId, sLoader;
static Pcm *sDone;

static void *loader(void *arg) {
    int handled = 0;
    (void)arg;
    pthread_mutex_lock(&sLoadLock);
    for (;;) {
        if (sReqId != handled) {
            char path[1200];
            int id = sReqId;
            Pcm *p;
            memcpy(path, sReqPath, sizeof path);
            pthread_mutex_unlock(&sLoadLock);
            p = load(path);
            pthread_mutex_lock(&sLoadLock);
            pcm_free(sDone); /* a result nobody took */
            sDone = p;
            sDoneId = id;
            handled = id;
            continue;
        }
        pthread_cond_wait(&sLoadCond, &sLoadLock);
    }
    return NULL;
}

static int request(const char *path) {
    int id;
    pthread_mutex_lock(&sLoadLock);
    if (!sLoader) {
        pthread_t t;
        if (pthread_create(&t, NULL, loader, NULL) == 0) {
            pthread_detach(t);
            sLoader = 1;
        }
    }
    id = sLoader ? ++sReqId : 0;
    if (id) {
        snprintf(sReqPath, sizeof sReqPath, "%s", path);
        pthread_cond_signal(&sLoadCond);
    }
    pthread_mutex_unlock(&sLoadLock);
    return id;
}

/* The result of request `id` when it is there: 1 and *out (NULL: the file could not be read). */
static int collect(int id, Pcm **out) {
    int done = 0;
    pthread_mutex_lock(&sLoadLock);
    if (sDoneId == id) {
        *out = sDone;
        sDone = NULL;
        sDoneId = 0;
        done = 1;
    }
    pthread_mutex_unlock(&sLoadLock);
    return done;
}

/* ------------------------------------------------------------------------------------------------ music playback */

static struct {
    int track;       /* the game's track at the last VBlank seen (-1 none) */
    int gen;         /* sGen when the file was chosen */
    int active;      /* replacing the music: a file for this track (loaded, loading or failed) */
    int pending;     /* loader request id, 0 none */
    Pcm *pcm, *cache;
    uint32_t pos, frac; /* source frame, and the fraction in 1/48000 of a frame */
    uint64_t elapsed;   /* output frames since the track started */
    int ended;
    char path[1200];
} sM = { .track = -1 };
static int sTrace = -1, sListed;

/* Position at `elapsed` output frames of the track (a file that arrived late joins at the track's time). */
static void seek(Pcm *p, int loops) {
    uint64_t src = sM.elapsed * p->rate;
    uint32_t start = p->mode == 1 ? p->loopStart : 0, end = p->mode == 1 ? p->loopEnd : p->frames;
    int looping = p->mode == 1 || (p->mode == 0 && loops);
    sM.frac = (uint32_t)(src % OUT_RATE);
    src /= OUT_RATE;
    sM.ended = 0;
    if (src >= end) {
        if (looping && end > start) {
            src = start + (src - end) % (end - start);
        } else {
            sM.ended = 1;
        }
    }
    sM.pos = (uint32_t)src;
}

/* One frame of the file at the position (linear interpolation), then the position steps by rate / 48000. */
static void next_frame(Pcm *p, int loops, int32_t *l, int32_t *r) {
    uint32_t start = p->mode == 1 ? p->loopStart : 0, end = p->mode == 1 ? p->loopEnd : p->frames, nx;
    int looping = p->mode == 1 || (p->mode == 0 && loops);
    if (sM.ended || p->frames == 0) {
        *l = *r = 0;
        return;
    }
    nx = sM.pos + 1 < end ? sM.pos + 1 : looping ? start : sM.pos;
    *l = ((int32_t)p->s[2 * sM.pos] * (int32_t)(OUT_RATE - sM.frac) + (int32_t)p->s[2 * nx] * (int32_t)sM.frac) /
         (int32_t)OUT_RATE;
    *r = ((int32_t)p->s[2 * sM.pos + 1] * (int32_t)(OUT_RATE - sM.frac) +
          (int32_t)p->s[2 * nx + 1] * (int32_t)sM.frac) / (int32_t)OUT_RATE;
    sM.frac += p->rate;
    while (sM.frac >= OUT_RATE) {
        sM.frac -= OUT_RATE;
        if (++sM.pos >= end) {
            if (looping) {
                sM.pos = start;
            } else {
                sM.pos = end - 1;
                sM.ended = 1;
                break;
            }
        }
    }
}

static void drop_current(void) {
    if (sM.pcm != NULL) {
        pcm_free(sM.cache);
        sM.cache = sM.pcm; /* kept: the same track again (a round, a retry) starts without loading */
        sM.pcm = NULL;
    }
}

static void release(void) {
    drop_current();
    sM.active = 0;
    sM.pending = 0;
    sM.path[0] = 0;
    sM.gen = -1; /* chosen again when music mods are on and a track plays */
}

/* The game's track changed (or the list did): which file replaces it now. */
static void choose(int track, int realtime, int restart, int loops) {
    char path[1200];
    path[0] = 0;
    ensure();
    pthread_mutex_lock(&sLock);
    if (track >= 0 && track < MODS_TRACKS) {
        memcpy(path, sTrackPath[track], sizeof path);
    }
    sM.gen = sGen;
    pthread_mutex_unlock(&sLock);
    if (restart) {
        sM.elapsed = 0;
    }
    if (sM.active && strcmp(path, sM.path) == 0) {
        if (restart && sM.pcm != NULL) {
            seek(sM.pcm, loops);
        }
        return;
    }
    drop_current();
    sM.pending = 0;
    sM.active = path[0] != 0;
    memcpy(sM.path, path, sizeof path);
    if (!sM.active) {
        return;
    }
    printf("ttt: mods: track %d: %s (VBlank %u)\n", track, path, (unsigned)Port_VBlankCount());
    if (sM.cache != NULL && strcmp(sM.cache->path, path) == 0) {
        sM.pcm = sM.cache;
        sM.cache = NULL;
    } else if (realtime && (sM.pending = request(path)) != 0) {
        return; /* silence until it is decoded */
    } else {
        sM.pcm = load(path);
    }
    if (sM.pcm != NULL) {
        seek(sM.pcm, loops); /* the start, or the track's time when the list changed while it plays */
    }
}

/* TTT_MODS_TRACKS=1: the game's own track table and stage table, as registered at boot (the list in mods.md). */
static void list_tracks(void) {
    int i, type, count;
    uint32_t bytes;
    static const char *const kType[5] = {"-", "disc", "SPU RAM", "EE memory", "movie"};
    for (i = 0; i < MODS_TRACKS; i++) {
        if (!Port_BgmTrackInfo(i, &type, &count, &bytes)) {
            continue;
        }
        /* a 2048-byte sector holds 1792 samples a channel at 44.1 kHz */
        printf("ttt: mods: table track %2d: %s, %s, %u s\n", i, type >= 0 && type < 5 ? kType[type] : "?",
               count < 0 ? "loops" : count == 1 ? "once" : "plays n times",
               type == 2 ? 0u : (unsigned)(bytes / 2048u * 1792u / 44100u));
    }
    for (i = 0; i < 26; i++) { /* func_00358F80: stage n < 0x1A plays D_004FB9C0[n] */
        printf("ttt: mods: table stage %2d: track %d\n", i, ((const int16_t *)Port_Ptr(STAGE_TRACKS))[i]);
    }
}

int Mods_MusicMix(int32_t *bus, int n, int realtime) {
    int gain = 0, stereo = 1, loops = 0, track, k, type, count, on;
    uint32_t bytes;
    int32_t c0l, c0r, al, ar, c1l, c1r;
    Pcm *p;
    if (sTrace < 0) {
        const char *t = getenv("TTT_MODS_TRACKS");
        sTrace = t != NULL && atoi(t) != 0;
    }
    track = Port_BgmNow(&gain, &stereo, &loops);
    if (sTrace && !sListed && Port_BgmTrackInfo(MODS_TRACKS - 1, &type, &count, &bytes)) {
        sListed = 1;
        list_tracks();
    }
    on = Settings_GetInt("mods.music");
    if (track != sM.track) {
        if (sTrace && track >= 0) {
            printf("ttt: mods: track %d starts (VBlank %u)\n", track, (unsigned)Port_VBlankCount());
        }
        sM.track = track;
        sM.elapsed = 0;
        if (track >= 0 && on) {
            choose(track, realtime, 1, loops);
        } else {
            release();
        }
    } else if (track >= 0 && !on) {
        release(); /* switched off: nothing kept but the cache; switched on again, it joins at the track's time */
    } else if (track >= 0 && sM.gen != __atomic_load_n(&sGen, __ATOMIC_ACQUIRE)) {
        choose(track, realtime, 0, loops); /* the list changed (or music mods came on): joins at the track's time */
    }
    if (track < 0 || !on || !sM.active) {
        sM.elapsed += (uint64_t)n;
        return 0;
    }
    if (sM.pending && collect(sM.pending, &sM.pcm)) {
        sM.pending = 0;
        if (sM.pcm != NULL) {
            seek(sM.pcm, loops);
        }
    }
    if (!sM.pending && sM.pcm == NULL) {
        sM.elapsed += (uint64_t)n;
        return 0; /* the file could not be read: the game's own music */
    }
    p = sM.pcm;
    /* the stream voices' path after the voice volume: core 0's master, core 1's AVOL, core 1's master (Spu2_MixBus) */
    c0l = (int16_t)(uint16_t)(gSpu2[0].mvolL << 1);
    c0r = (int16_t)(uint16_t)(gSpu2[0].mvolR << 1);
    al = gSpu2[1].avolL;
    ar = gSpu2[1].avolR;
    c1l = (int16_t)(uint16_t)(gSpu2[1].mvolL << 1);
    c1r = (int16_t)(uint16_t)(gSpu2[1].mvolR << 1);
    for (k = 0; k < n; k++) {
        int32_t l = 0, r = 0;
        int64_t ml, mr;
        if (p != NULL) {
            next_frame(p, loops, &l, &r);
        }
        if (!stereo) {
            l = r = (l + r) * 65 / 100;
        }
        ml = (int64_t)l * gain >> 14;
        mr = (int64_t)r * gain >> 14;
        ml = ((ml * c0l >> 15) * al >> 15) * c1l >> 15;
        mr = ((mr * c0r >> 15) * ar >> 15) * c1r >> 15;
        bus[4 * k] += (int32_t)ml - bus[4 * k + 2];
        bus[4 * k + 1] += (int32_t)mr - bus[4 * k + 3];
        bus[4 * k + 2] = (int32_t)ml;
        bus[4 * k + 3] = (int32_t)mr;
    }
    sM.elapsed += (uint64_t)n;
    return 1;
}

int Mods_MusicNow(char *file, int n) {
    /* the game's thread writes these; a torn read only shows a stale name for one frame */
    int t = sM.track;
    snprintf(file, (size_t)n, "%s", t >= 0 && sM.active ? sM.path : "");
    return t;
}

/* ------------------------------------------------------------------------------------------------ self-test */

static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: mods self-test: FAILED: %s\n", what);
        sFails++;
    }
}

static void print_list(const char *when) {
    int i;
    for (i = 0; i < sN; i++) {
        printf("ttt: mods: %s %d %s %s music 0x%07x shadowed 0x%07x textures %d\n", when, i, sMods[i].name,
               sMods[i].enabled ? "on" : "off", (unsigned)sMods[i].music, (unsigned)sMods[i].shadowed,
               sMods[i].textures);
    }
}

/* TTT_SELFTEST=mods (with TTT_CONFIG and TTT_MODS from the test): the folder's list in its load order and each winning
   track file decoded, then the resampler on made-up samples; TTT_MODS_EDIT=1 also moves the last mod to the top and
   disables the second, through the overlay's calls (written to the settings file). */
void Mods_SelfTest(void) {
    const char *t = getenv("TTT_SELFTEST"), *edit = getenv("TTT_MODS_EDIT");
    char sum[256];
    int i, k;
    if (t == NULL || strcmp(t, "mods") != 0) {
        return;
    }
    ensure();
    printf("ttt: mods: folder %s, %d mods\n", Mods_Folder(), sN);
    print_list("list");
    for (k = 0; k < MODS_TRACKS; k++) {
        if (sTrackPath[k][0]) {
            Pcm *p = load(sTrackPath[k]);
            const char *name = strrchr(sTrackPath[k], '/');
            if (p == NULL) {
                printf("ttt: mods: file track %d %s unreadable\n", k, name + 1);
                continue;
            }
            printf("ttt: mods: file track %d mod %s %s: %u frames at %u Hz, loop %u-%u mode %d, first %d %d\n", k,
                   sMods[sTrackMod[k]].name, name + 1, (unsigned)p->frames, (unsigned)p->rate,
                   (unsigned)p->loopStart, (unsigned)p->loopEnd, p->mode, p->s[0], p->s[1]);
            pcm_free(p);
        }
    }
    { /* the resampler: a ramp at 24 kHz reads as the ramp and its midpoints, looping over frames 2-5 */
        static int16_t ramp[16];
        Pcm p;
        int32_t l, r, want[12] = {0, 50, 100, 150, 200, 250, 300, 350, 400, 450, 500, 350};
        memset(&p, 0, sizeof p);
        for (i = 0; i < 8; i++) {
            ramp[2 * i] = (int16_t)(i * 100);
            ramp[2 * i + 1] = (int16_t)(-i * 100);
        }
        p.s = ramp;
        p.frames = 8;
        p.rate = 24000;
        p.loopStart = 2;
        p.loopEnd = 6;
        p.mode = 1;
        sM.pos = sM.frac = 0;
        sM.ended = 0;
        for (i = 0; i < 12; i++) {
            next_frame(&p, 0, &l, &r);
            expect(l == want[i] && r == -want[i], "24 kHz ramp resampled to 48 kHz, looping");
            if (l != want[i]) {
                printf("ttt: mods self-test: frame %d: %d, want %d\n", i, (int)l, (int)want[i]);
            }
        }
        p.mode = 2; /* once: holds silence after the end */
        sM.pos = 7;
        sM.frac = 0;
        sM.ended = 0;
        next_frame(&p, 1, &l, &r);
        next_frame(&p, 1, &l, &r);
        next_frame(&p, 1, &l, &r);
        expect(sM.ended && l == 0, "a file played once ends in silence");
        sM.elapsed = 48000 * 3; /* 3 s into a 1 s file at 8 kHz looping whole: the start again */
        p.frames = 8000;
        p.rate = 8000;
        p.mode = 0;
        p.s = NULL;
        seek(&p, 1);
        expect(sM.pos == 0 && !sM.ended, "a late file joins at the track's time");
        sM.elapsed = 0;
    }
    expect(list_find("a, b ,c", "b") == 1 && list_find("a,b", "c") < 0 && list_find("", "a") < 0, "list parsing");
    expect(track_of("07.ogg") == 7 && track_of("7.WAV") == 7 && track_of("21 - stage.ogg") == 21 &&
               track_of("28.wav") < 0 && track_of("123.wav") < 0 && track_of("07.mp3") < 0 && track_of("x.wav") < 0,
           "track file names");
    if (edit != NULL && atoi(edit) != 0 && sN >= 2) {
        Mods_Move(sN - 1, -(sN - 1));
        Mods_SetEnabled(1, 0);
        print_list("edited");
    }
    Mods_Summary(sum, sizeof sum);
    printf("ttt: mods: summary: %s\n", sum);
    printf(sFails ? "ttt: mods self-test FAILED (%d)\n" : "ttt: mods self-test OK\n", sFails);
    exit(sFails != 0);
}
