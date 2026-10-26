/*
 * Where the port keeps the player's files (T43, docs/port/install.md).
 *
 * Three cases, decided once from the program's own place (PlatOs_ExePath: symbolic links resolved):
 *
 * - portable: a file `portable.txt` beside the program. Everything stays beside it: the data file (ttt.dat), the
 *   memory cards (saves/), the texture packs (textures/) and the settings file (settings.ini, plat_settings.c).
 * - a checkout: the program is build/port/ttt of this repository (../../port/tools/build.py exists beside it). The
 *   data file stays beside the program (build/port/ttt.dat, ignored by git), the cards and the texture packs are the
 *   repository's saves/ and textures/, the settings file is the per-user one: as before T43.
 * - installed (anything else): the per-user data folder $XDG_DATA_HOME/ttt, else ~/.local/share/ttt, holds ttt.dat,
 *   saves/ and textures/; the settings file is the per-user one ($XDG_CONFIG_HOME/ttt/settings.ini, else
 *   ~/.config/ttt/settings.ini). Windows (T42 b6, plat_os.h): %LOCALAPPDATA%\ttt and %APPDATA%\ttt\settings.ini.
 *
 * The variables still win over all three: TTT_DATA (the data file), TTT_SAVES, TTT_TEXTURES (video.texture_dir),
 * TTT_CONFIG. Host stack only; nothing here is game state.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat_os.h"

static char sExeDir[1024];
static int sKind = -1; /* 0 installed, 1 portable, 2 a checkout */

static void decide(void) {
    char probe[1100];
    long k;
    if (sKind >= 0) {
        return;
    }
    sKind = 0;
    k = PlatOs_ExePath(sExeDir, sizeof sExeDir - 1);
    if (k > 0) {
        char *slash;
        sExeDir[k] = 0;
        slash = strrchr(sExeDir, '/');
        if (slash != NULL) {
            *slash = 0;
        } else {
            sExeDir[0] = 0;
        }
    } else {
        sExeDir[0] = 0;
    }
    if (sExeDir[0]) {
        snprintf(probe, sizeof probe, "%s/portable.txt", sExeDir);
        if (PlatOs_Exists(probe)) {
            sKind = 1;
            return;
        }
        snprintf(probe, sizeof probe, "%s/../../port/tools/build.py", sExeDir);
        if (PlatOs_Exists(probe)) {
            sKind = 2;
        }
    }
}

/* The program's folder ("" when unknown). */
const char *Port_ExeDir(void) {
    decide();
    return sExeDir;
}

int Port_Portable(void) {
    decide();
    return sKind == 1;
}

int Port_Checkout(void) {
    decide();
    return sKind == 2;
}

/* The per-user folder of `kind` (0 data, 1 settings) with `leaf` in it into out[n]: 0 when there is none (no HOME).
   Linux: $XDG_DATA_HOME / $XDG_CONFIG_HOME when absolute, else ~/.local/share / ~/.config; Windows: %LOCALAPPDATA% /
   %APPDATA%. */
int Port_UserDir(int kind, const char *leaf, char *out, int n) {
#ifdef _WIN32
    const char *b = getenv(kind ? "APPDATA" : "LOCALAPPDATA");
    if (b != NULL && *b != 0) {
        snprintf(out, (size_t)n, "%s/%s", b, leaf);
        return 1;
    }
#else
    const char *x = getenv(kind ? "XDG_CONFIG_HOME" : "XDG_DATA_HOME"), *h = getenv("HOME");
    if (x != NULL && *x == '/') {
        snprintf(out, (size_t)n, "%s/%s", x, leaf);
        return 1;
    } else if (h != NULL && *h != 0) {
        snprintf(out, (size_t)n, "%s/%s/%s", h, kind ? ".config" : ".local/share", leaf);
        return 1;
    }
#endif
    return 0;
}

/* The folder the setup's browser starts in: the home folder when absolute, else the root. */
const char *Port_HomeDir(void) {
#ifdef _WIN32
    const char *h = getenv("USERPROFILE");
    return h != NULL && h[0] != 0 && h[1] == ':' ? h : "C:/";
#else
    const char *h = getenv("HOME");
    return h != NULL && *h == '/' ? h : "/";
#endif
}

/* The folder of the player's game files: beside the program (portable, a checkout), else the per-user data folder;
   "." when there is neither (no HOME, unknown program path). Not created here: Port_MakeDirs. */
const char *Port_DataDir(void) {
    static char dir[1100];
    if (dir[0]) {
        return dir;
    }
    decide();
    if (sKind != 0 && sExeDir[0]) {
        snprintf(dir, sizeof dir, "%s", sExeDir);
    } else if (!Port_UserDir(0, "ttt", dir, sizeof dir)) {
        snprintf(dir, sizeof dir, "%s", sExeDir[0] ? sExeDir : ".");
    }
    return dir;
}

/* The default folder of `leaf` (saves, textures): the repository's in a checkout, else in Port_DataDir(). */
void Port_UserFolder(const char *leaf, char *out, int n) {
    decide();
    if (sKind == 2) {
        snprintf(out, (size_t)n, "%s/../../%s", sExeDir, leaf);
    } else {
        snprintf(out, (size_t)n, "%s/%s", Port_DataDir(), leaf);
    }
}

/* Creates the folders above `path` (a file's path), like mkdir -p of its directory. */
void Port_MakeDirs(const char *path) {
    char p[1200];
    char *s;
    snprintf(p, sizeof p, "%s", path);
    for (s = p + 1; *s; s++) {
        if (*s == '/') {
            *s = 0;
            PlatOs_MkDir(p, 0755);
            *s = '/';
        }
    }
}
