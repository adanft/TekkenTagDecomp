/*
 * The player's settings (T36, docs/port/settings.md).
 *
 * A text file of `key = value` lines (`#` / `;` comments, blank lines, `[section]` lines and keys this program does not
 * know are kept as they are when the file is written again). Where: TTT_CONFIG, else $XDG_CONFIG_HOME/ttt/settings.ini,
 * else ~/.config/ttt/settings.ini. Written at once when the overlay (overlay.cpp) changes a value: a temporary file
 * beside it, flushed to the disk, renamed over it, so a crash leaves the old file or the new one.
 *
 * Settings that existed as environment variables keep them, and the variables win: at start a file value becomes its
 * variable (setenv) only when the variable is not set, so the code that reads the variables (TTT_RENDERER, TTT_VSYNC,
 * TTT_MOVIES...) reads the file's choice without knowing about the file. The overlay shows which values the
 * environment holds.
 *
 * Nothing here is game state and nothing of the game reads it: the window thread applies the choices to the window,
 * the sound device and the keyboard (plat_window.c). Integer and text values only (the port has no host float).
 * Runs on the host stack: at start in __wrap_main (plat_main.c), later on the window thread; a mutex covers the lines.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "port_host.h"
#include "plat_settings.h"

/* C library calls through the GOT: no new PLT entry (port_host.h) */
#define PORT_NOPLT(f) __typeof__(f) f __attribute__((noplt))
PORT_NOPLT(setenv);
PORT_NOPLT(unsetenv);
PORT_NOPLT(mkdir);
PORT_NOPLT(strncmp);
PORT_NOPLT(strchr);
PORT_NOPLT(strerror);
PORT_NOPLT(access);
#undef PORT_NOPLT

typedef struct {
    const char *key, *env;
    int lo, hi, def;
    const char *sdef; /* text settings: the default (and lo / hi unused) */
    int restart;
} Def;

static const Def kDefs[] = {
    {"video.renderer", "TTT_RENDERER", 0, 0, 0, "gl", 1},
    {"video.vsync", "TTT_VSYNC", 0, 1, 1, NULL, 0},
    {"video.scale", "TTT_SCALE", 1, 6, 2, NULL, 0},
    {"video.fullscreen", "TTT_FULLSCREEN", 0, 1, 0, NULL, 0},
    {"video.fps_meter", "TTT_FPS_METER", 0, 1, 0, NULL, 0},
    {"video.resolution", "TTT_RESOLUTION", 1, 8, 1, NULL, 0},
    {"video.aspect", "TTT_ASPECT", 0, 0, 0, "4:3", 0},
    {"video.deflicker", "TTT_DEFLICKER", 0, 1, 1, NULL, 0},
    {"video.smooth", "TTT_SMOOTH", 0, 1, 1, NULL, 0},
    {"video.low_latency", "TTT_LOW_LATENCY", 0, 1, 0, NULL, 0},
    {"video.texture_dump", "TTT_TEXTURE_DUMP", 0, 1, 0, NULL, 0},
    {"video.texture_replace", "TTT_TEXTURE_REPLACE", 0, 1, 0, NULL, 0},
    {"video.texture_dir", "TTT_TEXTURES", 0, 0, 0, "", 1},
    {"audio.volume", "TTT_VOLUME", 0, 100, 100, NULL, 0},
    {"audio.mute", "TTT_MUTE", 0, 1, 0, NULL, 0},
    {"game.movies", "TTT_MOVIES", 0, 1, 1, NULL, 1},
    {"game.pause", "TTT_PAUSE_IN_MENU", 0, 1, 1, NULL, 0},
};
#define NDEFS ((int)(sizeof kDefs / sizeof kDefs[0]))

#define MAX_LINES 1024
#define LINE_MAX 1024
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static char *sLines[MAX_LINES];
static int sN;
static char sPath[1100], sLoadErr[256], sSaveErr[1400];
static char sEnvVal[NDEFS][1024]; /* the environment's value at start, "" none (a folder for video.texture_dir) */
static int sEnvSet[NDEFS], sSetByUs[NDEFS];

static const char *kButtons[SB_COUNT] = {"up", "down", "left", "right", "square", "triangle", "cross", "circle",
                                         "l1", "r1", "l2", "r2", "start", "select"};
static const uint16_t kDefaultKeys[2][SB_COUNT] = {
    {26, 22, 4, 7, 24, 12, 13, 14, 28, 18, 11, 15, 40, 42},  /* W S A D, U I J K, Y O H L, Return, Backspace */
    {82, 81, 80, 79, 92, 93, 89, 90, 95, 94, 96, 97, 88, 87}, /* arrows, keypad 4 5 1 2, 7 6 8 9, Enter, + */
};

static int def_index(const char *key) {
    int i;
    for (i = 0; i < NDEFS; i++) {
        if (strcmp(kDefs[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

/* A line's key and value (trimmed, into the buffers): 0 for comments, blank and section lines. */
static int parse_line(const char *l, char *key, int kn, char *val, int vn) {
    const char *eq, *a, *b;
    int n;
    while (*l == ' ' || *l == '\t') {
        l++;
    }
    if (*l == 0 || *l == '#' || *l == ';' || *l == '[' || (eq = strchr(l, '=')) == NULL) {
        return 0;
    }
    for (b = eq; b > l && (b[-1] == ' ' || b[-1] == '\t'); b--) {
    }
    n = (int)(b - l);
    if (n <= 0 || n >= kn) {
        return 0;
    }
    memcpy(key, l, (size_t)n);
    key[n] = 0;
    for (a = eq + 1; *a == ' ' || *a == '\t'; a++) {
    }
    for (b = a + strlen(a); b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r' || b[-1] == '\n'); b--) {
    }
    n = (int)(b - a);
    if (n >= vn) {
        n = vn - 1;
    }
    memcpy(val, a, (size_t)n);
    val[n] = 0;
    return 1;
}

/* The file's value of a key (the last line that sets it): 1 when there is one. Lock held. */
static int file_get(const char *key, char *out, int n) {
    char k[128], v[LINE_MAX];
    int i, found = 0;
    for (i = 0; i < sN; i++) {
        if (parse_line(sLines[i], k, sizeof k, v, sizeof v) && strcmp(k, key) == 0) {
            snprintf(out, (size_t)n, "%s", v);
            found = 1;
        }
    }
    return found;
}

/* Sets a key's line (the last one that sets it), or adds one. Lock held. */
static int file_set(const char *key, const char *val) {
    char k[128], v[LINE_MAX], *line;
    int i, at = -1;
    size_t len = strlen(key) + strlen(val) + 4;
    for (i = 0; i < sN; i++) {
        if (parse_line(sLines[i], k, sizeof k, v, sizeof v) && strcmp(k, key) == 0) {
            at = i;
        }
    }
    if ((line = malloc(len)) == NULL) {
        return 0;
    }
    snprintf(line, len, "%s = %s", key, val);
    if (at >= 0) {
        free(sLines[at]);
        sLines[at] = line;
    } else if (sN < MAX_LINES) {
        sLines[sN++] = line;
    } else {
        free(line);
        return 0;
    }
    return 1;
}

static void resolve_path(void) {
    const char *e = getenv("TTT_CONFIG"), *x = getenv("XDG_CONFIG_HOME"), *h = getenv("HOME");
    if (e != NULL && *e != 0) {
        snprintf(sPath, sizeof sPath, "%s", e);
    } else if (x != NULL && *x == '/') {
        snprintf(sPath, sizeof sPath, "%s/ttt/settings.ini", x);
    } else if (h != NULL && *h != 0) {
        snprintf(sPath, sizeof sPath, "%s/.config/ttt/settings.ini", h);
    } else {
        sPath[0] = 0;
    }
}

void Settings_Load(void) {
    FILE *f;
    char line[LINE_MAX], v[LINE_MAX];
    int i;

    pthread_mutex_lock(&sLock);
    for (i = 0; i < NDEFS; i++) { /* a second load (the self-test) starts from the environment it found */
        if (sSetByUs[i]) {
            unsetenv(kDefs[i].env);
            sSetByUs[i] = 0;
        }
    }
    for (i = 0; i < sN; i++) {
        free(sLines[i]);
    }
    sN = 0;
    sLoadErr[0] = 0;
    resolve_path();
    f = sPath[0] != 0 ? fopen(sPath, "r") : NULL;
    if (f == NULL && sPath[0] != 0 && errno != ENOENT) {
        snprintf(sLoadErr, sizeof sLoadErr, "%s", strerror(errno));
    }
    while (f != NULL && sN < MAX_LINES && fgets(line, sizeof line, f) != NULL) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) {
            line[--n] = 0;
        }
        if ((sLines[sN] = malloc(n + 1)) == NULL) {
            break;
        }
        memcpy(sLines[sN++], line, n + 1);
    }
    if (f != NULL) {
        fclose(f);
    }
    for (i = 0; i < NDEFS; i++) {
        const char *e = getenv(kDefs[i].env);
        sEnvSet[i] = e != NULL;
        snprintf(sEnvVal[i], sizeof sEnvVal[i], "%s", e != NULL ? e : "");
        if (e == NULL && file_get(kDefs[i].key, v, sizeof v)) {
            setenv(kDefs[i].env, v, 0);
            sSetByUs[i] = 1;
        }
    }
    pthread_mutex_unlock(&sLock);
}

const char *Settings_Path(void) { return sPath; }
const char *Settings_LoadError(void) { return sLoadErr; }
const char *Settings_SaveError(void) { return sSaveErr; }

static int clamp_def(int i, int v) {
    return v < kDefs[i].lo ? kDefs[i].lo : v > kDefs[i].hi ? kDefs[i].hi : v;
}

int Settings_GetInt(const char *key) {
    char v[LINE_MAX];
    int i = def_index(key), r;
    if (i < 0 || kDefs[i].sdef != NULL) {
        return 0;
    }
    pthread_mutex_lock(&sLock);
    if (sEnvSet[i]) {
        r = clamp_def(i, atoi(sEnvVal[i]));
    } else if (file_get(key, v, sizeof v)) {
        r = clamp_def(i, atoi(v));
    } else {
        r = kDefs[i].def;
    }
    pthread_mutex_unlock(&sLock);
    return r;
}

void Settings_GetStr(const char *key, char *out, int n) {
    int i = def_index(key);
    pthread_mutex_lock(&sLock);
    if (i >= 0 && sEnvSet[i]) {
        snprintf(out, (size_t)n, "%s", sEnvVal[i]);
    } else if (!file_get(key, out, n)) {
        snprintf(out, (size_t)n, "%s", i >= 0 && kDefs[i].sdef != NULL ? kDefs[i].sdef : "");
    }
    pthread_mutex_unlock(&sLock);
}

int Settings_FromEnv(const char *key) {
    int i = def_index(key);
    return i >= 0 && sEnvSet[i];
}

int Settings_NeedsRestart(const char *key) {
    int i = def_index(key);
    return i >= 0 && kDefs[i].restart;
}

const char *Settings_Env(const char *key) {
    int i = def_index(key);
    return i >= 0 ? kDefs[i].env : NULL;
}

/* mkdir -p of the file's directory */
static void make_dirs(const char *path) {
    char d[1100];
    char *p;
    snprintf(d, sizeof d, "%s", path);
    for (p = d + 1; *p != 0; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(d, 0755);
            *p = '/';
        }
    }
}

/* The lines to the file: a temporary file beside it, flushed and synced, renamed over it. Lock held. */
static int save_locked(void) {
    char tmp[1200];
    FILE *f;
    int i, ok;

    sSaveErr[0] = 0;
    if (sPath[0] == 0) {
        snprintf(sSaveErr, sizeof sSaveErr, "no settings file (set TTT_CONFIG or HOME)");
        return 0;
    }
    make_dirs(sPath);
    snprintf(tmp, sizeof tmp, "%s.tmp", sPath);
    if ((f = fopen(tmp, "w")) == NULL) {
        snprintf(sSaveErr, sizeof sSaveErr, "%s: %s", tmp, strerror(errno));
        return 0;
    }
    ok = 1;
    for (i = 0; i < sN; i++) {
        ok &= fprintf(f, "%s\n", sLines[i]) >= 0;
    }
    ok &= fflush(f) == 0;
    ok &= fsync(fileno(f)) == 0;
    ok &= fclose(f) == 0;
    if (!ok || rename(tmp, sPath) != 0) {
        snprintf(sSaveErr, sizeof sSaveErr, "%s: %s", sPath, strerror(errno));
        unlink(tmp);
        return 0;
    }
    return 1;
}

int Settings_SetStr(const char *key, const char *v) {
    int ok;
    pthread_mutex_lock(&sLock);
    ok = file_set(key, v) && save_locked();
    pthread_mutex_unlock(&sLock);
    return ok;
}

int Settings_SetInt(const char *key, int v) {
    char b[16];
    int i = def_index(key);
    snprintf(b, sizeof b, "%d", i >= 0 ? clamp_def(i, v) : v);
    return Settings_SetStr(key, b);
}

/* ---- key names (USB HID usages, which SDL's scancodes are) ---- */
static const struct {
    uint16_t sc;
    const char *name;
} kKeyNames[] = {
    {40, "Return"}, {41, "Escape"}, {42, "Backspace"}, {43, "Tab"}, {44, "Space"}, {45, "-"}, {46, "="}, {47, "["},
    {48, "]"}, {49, "\\"}, {51, ";"}, {52, "'"}, {53, "`"}, {54, ","}, {55, "."}, {56, "/"}, {57, "Caps Lock"},
    {70, "Print Screen"}, {71, "Scroll Lock"}, {72, "Pause"}, {73, "Insert"}, {74, "Home"}, {75, "Page Up"},
    {76, "Delete"}, {77, "End"}, {78, "Page Down"}, {79, "Right"}, {80, "Left"}, {81, "Down"}, {82, "Up"},
    {83, "Num Lock"}, {84, "Keypad /"}, {85, "Keypad *"}, {86, "Keypad -"}, {87, "Keypad +"}, {88, "Keypad Enter"},
    {98, "Keypad 0"}, {99, "Keypad ."}, {101, "Menu"}, {224, "Left Ctrl"}, {225, "Left Shift"}, {226, "Left Alt"},
    {227, "Left GUI"}, {228, "Right Ctrl"}, {229, "Right Shift"}, {230, "Right Alt"}, {231, "Right GUI"},
};
#define NKEYNAMES ((int)(sizeof kKeyNames / sizeof kKeyNames[0]))

void Settings_KeyName(uint32_t sc, char *out, int n) {
    int i;
    if (sc >= 4 && sc <= 29) {
        snprintf(out, (size_t)n, "%c", (char)('A' + sc - 4));
    } else if (sc >= 30 && sc <= 39) {
        snprintf(out, (size_t)n, "%c", sc == 39 ? '0' : (char)('1' + sc - 30));
    } else if (sc >= 58 && sc <= 69) {
        snprintf(out, (size_t)n, "F%u", sc - 57);
    } else if (sc >= 89 && sc <= 97) {
        snprintf(out, (size_t)n, "Keypad %u", sc - 88);
    } else {
        for (i = 0; i < NKEYNAMES; i++) {
            if (kKeyNames[i].sc == sc) {
                snprintf(out, (size_t)n, "%s", kKeyNames[i].name);
                return;
            }
        }
        snprintf(out, (size_t)n, "Scancode %u", sc);
    }
}

static int same_name(const char *a, const char *b) {
    for (; *a != 0 && *b != 0; a++, b++) {
        char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a, y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;
        if (x != y) {
            return 0;
        }
    }
    return *a == 0 && *b == 0;
}

uint32_t Settings_KeyFromName(const char *name) {
    char buf[32];
    uint32_t sc;
    const char *p = name;
    int prefixed;
    if (name == NULL || *name == 0) {
        return 0;
    }
    prefixed = strncmp(name, "Scancode ", 9) == 0 || strncmp(name, "scancode ", 9) == 0;
    if (prefixed) {
        p = name + 9;
    }
    if (prefixed || (p[0] >= '0' && p[0] <= '9' && p[1] != 0)) { /* a number; a lone digit is that key, below */
        char *end;
        long v = strtol(p, &end, 10);
        if (end != p && *end == 0) {
            return v > 0 && v < 512 ? (uint32_t)v : 0;
        }
    }
    for (sc = 4; sc < 512; sc++) {
        Settings_KeyName(sc, buf, sizeof buf);
        if (same_name(buf, name)) {
            return sc;
        }
    }
    return 0;
}

/* ---- bindings ---- */
const char *Settings_ButtonName(int button) {
    return button >= 0 && button < SB_COUNT ? kButtons[button] : "?";
}

void Settings_DefaultBindings(uint16_t b[2][SB_COUNT]) {
    memcpy(b, kDefaultKeys, sizeof kDefaultKeys);
}

void Settings_Bindings(uint16_t b[2][SB_COUNT]) {
    char key[64], v[LINE_MAX];
    int p, k;
    Settings_DefaultBindings(b);
    pthread_mutex_lock(&sLock);
    for (p = 0; p < 2; p++) {
        for (k = 0; k < SB_COUNT; k++) {
            uint32_t sc;
            snprintf(key, sizeof key, "keys.p%d.%s", p + 1, kButtons[k]);
            if (file_get(key, v, sizeof v) && (sc = Settings_KeyFromName(v)) != 0) {
                Settings_Rebind(b, p, k, sc); /* the same rule as the overlay: one button per key */
            }
        }
    }
    pthread_mutex_unlock(&sLock);
}

int Settings_Rebind(uint16_t b[2][SB_COUNT], int pad, int button, uint32_t sc) {
    int p, k;
    uint16_t old;
    if (pad < 0 || pad > 1 || button < 0 || button >= SB_COUNT || sc == 0 || sc >= 512 || sc == SETTINGS_SC_OPEN ||
        sc == SETTINGS_SC_ESCAPE) {
        return 0;
    }
    old = b[pad][button];
    for (p = 0; p < 2; p++) {
        for (k = 0; k < SB_COUNT; k++) {
            if (b[p][k] == sc && (p != pad || k != button)) {
                b[p][k] = old;
            }
        }
    }
    b[pad][button] = (uint16_t)sc;
    return 1;
}

int Settings_SaveBindings(const uint16_t b[2][SB_COUNT]) {
    char key[64], name[32];
    int p, k, ok = 1;
    pthread_mutex_lock(&sLock);
    for (p = 0; p < 2; p++) {
        for (k = 0; k < SB_COUNT; k++) {
            snprintf(key, sizeof key, "keys.p%d.%s", p + 1, kButtons[k]);
            Settings_KeyName(b[p][k], name, sizeof name);
            ok &= file_set(key, name);
        }
    }
    ok = ok && save_locked();
    pthread_mutex_unlock(&sLock);
    return ok;
}

/* ---- TTT_SELFTEST=settings: the file's round trip, the environment's precedence, the bindings, the key names ---- */
static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("settings: FAIL %s\n", what);
        sFails++;
    }
}

static long file_text(const char *path, char *out, long max) {
    FILE *f = fopen(path, "r");
    long n;
    if (f == NULL) {
        return -1;
    }
    n = (long)fread(out, 1, (size_t)max - 1, f);
    fclose(f);
    out[n < 0 ? 0 : n] = 0;
    return n;
}

void Settings_SelfTest(void) {
    const char *t = getenv("TTT_SELFTEST"), *cfg = getenv("TTT_CONFIG");
    char text[8192], s[64], tmp[1200];
    uint16_t b[2][SB_COUNT], d[2][SB_COUNT], c[2][SB_COUNT];
    FILE *f;
    uint32_t sc;
    int p, k;

    if (t == NULL || strcmp(t, "settings") != 0) {
        return;
    }
    if (cfg == NULL || *cfg == 0) {
        printf("ttt: settings self-test FAILED (set TTT_CONFIG to a file the test may write)\n");
        exit(1);
    }
    for (k = 0; k < NDEFS; k++) { /* the test's own environment: only TTT_SCALE set */
        unsetenv(kDefs[k].env);
    }
    setenv("TTT_SCALE", "4", 1);
    f = fopen(cfg, "w");
    expect(f != NULL, "the test file can be written");
    if (f == NULL) {
        exit(1);
    }
    fprintf(f, "# my settings\n[video]\nvideo.vsync = 0\nvideo.scale=3\naudio.volume = 150\nfuture.option = keep me\n"
               "keys.p1.square = space\n\n; end\n");
    fclose(f);
    Settings_Load();
    expect(strcmp(Settings_Path(), cfg) == 0, "TTT_CONFIG names the file");
    expect(Settings_GetInt("video.vsync") == 0, "a file value is read");
    expect(getenv("TTT_VSYNC") != NULL && strcmp(getenv("TTT_VSYNC"), "0") == 0, "a file value becomes its variable");
    expect(Settings_GetInt("audio.volume") == 100, "a value past its range is clamped");
    expect(Settings_GetInt("video.scale") == 4 && Settings_FromEnv("video.scale"), "the environment wins over the file");
    expect(strcmp(getenv("TTT_SCALE"), "4") == 0, "the environment's variable is left alone");
    expect(Settings_GetInt("video.fps_meter") == 0 && Settings_GetInt("game.movies") == 1, "defaults");
    Settings_GetStr("video.renderer", s, sizeof s);
    expect(strcmp(s, "gl") == 0 && getenv("TTT_RENDERER") == NULL, "a text default, no variable set for it");
    expect(Settings_NeedsRestart("video.renderer") && !Settings_NeedsRestart("audio.volume"), "restart flags");
    Settings_Bindings(b);
    expect(b[0][SB_SQUARE] == 44 && b[0][SB_UP] == 26, "keys.p1.square = space binds Space");

    expect(Settings_SetInt("audio.volume", 40), "a change is written");
    snprintf(tmp, sizeof tmp, "%s.tmp", cfg);
    expect(access(tmp, F_OK) != 0, "no temporary file is left");
    expect(file_text(cfg, text, sizeof text) > 0, "the file is there");
    expect(strstr(text, "# my settings\n[video]\n") == text, "comments and sections are kept, in place");
    expect(strstr(text, "future.option = keep me\n") != NULL, "unknown keys are kept");
    expect(strstr(text, "audio.volume = 40\n") != NULL && strstr(text, "150") == NULL, "the line is replaced");
    expect(strstr(text, "video.scale=3\n") != NULL, "the environment's value is not written to the file");
    expect(Settings_SetStr("video.renderer", "sw") && Settings_SetInt("audio.mute", 1), "new keys are added");
    Settings_Load();
    expect(Settings_GetInt("audio.volume") == 40 && Settings_GetInt("audio.mute") == 1, "values read back");
    Settings_GetStr("video.renderer", s, sizeof s);
    expect(strcmp(s, "sw") == 0 && strcmp(getenv("TTT_RENDERER"), "sw") == 0, "a text value read back");
    file_text(cfg, text, sizeof text);
    expect(strstr(text, "future.option = keep me\n") != NULL && strstr(text, "; end\n") != NULL, "kept after a reload");

    /* the video options (T37) */
    expect(Settings_GetInt("video.resolution") == 1 && Settings_GetInt("video.deflicker") == 1 && Settings_GetInt("video.smooth") == 1 &&
               Settings_GetInt("video.low_latency") == 0,
           "video defaults");
    Settings_GetStr("video.aspect", s, sizeof s);
    expect(strcmp(s, "4:3") == 0, "aspect default 4:3");
    expect(Settings_SetInt("video.resolution", 12) && Settings_SetStr("video.aspect", "21:9") && Settings_SetInt("video.deflicker", 0) &&
               Settings_SetInt("video.low_latency", 1),
           "video options written");
    Settings_Load();
    Settings_GetStr("video.aspect", s, sizeof s);
    expect(Settings_GetInt("video.resolution") == 8 && strcmp(s, "21:9") == 0 && Settings_GetInt("video.deflicker") == 0 &&
               Settings_GetInt("video.low_latency") == 1,
           "video options read back (resolution clamped to 8)");
    file_text(cfg, text, sizeof text);
    expect(strstr(text, "video.resolution = 8\n") != NULL && strstr(text, "video.aspect = 21:9\n") != NULL, "video lines in the file");

    /* rebinding: one button per key (the two swap), F1 / Escape refused */
    Settings_DefaultBindings(d);
    memcpy(c, d, sizeof d);
    expect(Settings_Rebind(c, 0, SB_SQUARE, 26) && c[0][SB_SQUARE] == 26 && c[0][SB_UP] == 24,
           "W on p1 square: p1 up gets U");
    expect(Settings_Rebind(c, 0, SB_UP, 82) && c[0][SB_UP] == 82 && c[1][SB_UP] == 24, "a key of pad 2 swaps across pads");
    expect(!Settings_Rebind(c, 0, SB_START, SETTINGS_SC_OPEN) && c[0][SB_START] == 40, "F1 is refused");
    expect(!Settings_Rebind(c, 1, SB_START, SETTINGS_SC_ESCAPE), "Escape is refused");
    expect(!Settings_Rebind(c, 2, SB_START, 30) && !Settings_Rebind(c, 0, SB_COUNT, 30), "no such pad or button");
    expect(Settings_Rebind(c, 1, SB_CROSS, c[1][SB_CROSS]), "the same key again is fine");
    {
        int seen[512] = {0}, dup = 0;
        for (p = 0; p < 2; p++) {
            for (k = 0; k < SB_COUNT; k++) {
                dup |= seen[c[p][k]]++;
            }
        }
        expect(!dup, "no key presses two buttons");
    }
    expect(Settings_SaveBindings(c), "bindings written");
    Settings_Load();
    Settings_Bindings(b);
    expect(memcmp(b, c, sizeof b) == 0, "bindings read back");
    file_text(cfg, text, sizeof text);
    expect(strstr(text, "keys.p1.up = Up\n") != NULL && strstr(text, "keys.p2.up = U\n") != NULL, "bindings by name");

    for (sc = 1; sc < 300; sc++) {
        char n[32];
        Settings_KeyName(sc, n, sizeof n);
        if (Settings_KeyFromName(n) != sc) {
            printf("settings: FAIL key name %u '%s' -> %u\n", sc, n, Settings_KeyFromName(n));
            sFails++;
        }
    }
    expect(Settings_KeyFromName("keypad 4") == 92 && Settings_KeyFromName("left shift") == 225, "names in any case");
    expect(Settings_KeyFromName("1") == 30 && Settings_KeyFromName("123") == 123, "digits are keys, numbers scancodes");
    expect(Settings_KeyFromName("nonsense") == 0 && Settings_KeyFromName("") == 0, "unknown names");

    setenv("TTT_CONFIG", "/nonexistent-dir-ttt/x/settings.ini", 1);
    Settings_Load();
    expect(Settings_GetInt("audio.volume") == 100 && Settings_LoadError()[0] == 0, "a missing file is the defaults");
    unsetenv("TTT_SCALE");
    printf(sFails ? "ttt: settings self-test FAILED (%d)\n" : "ttt: settings self-test OK\n", sFails);
    fflush(stdout);
    exit(sFails != 0);
}
