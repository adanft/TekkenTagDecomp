/*
 * Where the port keeps the player's files (T43, docs/port/install.md).
 *
 * Three cases, decided once from the program's own place (/proc/self/exe, symbolic links resolved):
 *
 * - portable: a file `portable.txt` beside the program. Everything stays beside it: the data file (ttt.dat), the
 *   memory cards (saves/), the texture packs (textures/) and the settings file (settings.ini, plat_settings.c).
 * - a checkout: the program is build/port/ttt of this repository (../../port/tools/build.py exists beside it). The
 *   data file stays beside the program (build/port/ttt.dat, ignored by git), the cards and the texture packs are the
 *   repository's saves/ and textures/, the settings file is the per-user one: as before T43.
 * - installed (anything else): the per-user data folder $XDG_DATA_HOME/ttt, else ~/.local/share/ttt, holds ttt.dat,
 *   saves/ and textures/; the settings file is the per-user one ($XDG_CONFIG_HOME/ttt/settings.ini).
 *
 * The variables still win over all three: TTT_DATA (the data file), TTT_SAVES, TTT_TEXTURES (video.texture_dir),
 * TTT_CONFIG. Host stack only; nothing here is game state.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "port_host.h"

PORT_NOPLT(access);
PORT_NOPLT(mkdir);

static char sExeDir[1024];
static int sKind = -1; /* 0 installed, 1 portable, 2 a checkout */

static void decide(void) {
    char probe[1100];
    ssize_t k;
    if (sKind >= 0) {
        return;
    }
    sKind = 0;
    k = readlink("/proc/self/exe", sExeDir, sizeof sExeDir - 1);
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
        if (access(probe, F_OK) == 0) {
            sKind = 1;
            return;
        }
        snprintf(probe, sizeof probe, "%s/../../port/tools/build.py", sExeDir);
        if (access(probe, F_OK) == 0) {
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

/* The folder of the player's game files: beside the program (portable, a checkout), else the per-user data folder;
   "." when there is neither (no HOME, unknown program path). Not created here: Port_MakeDirs. */
const char *Port_DataDir(void) {
    static char dir[1100];
    const char *x, *h;
    if (dir[0]) {
        return dir;
    }
    decide();
    x = getenv("XDG_DATA_HOME");
    h = getenv("HOME");
    if (sKind != 0 && sExeDir[0]) {
        snprintf(dir, sizeof dir, "%s", sExeDir);
    } else if (x != NULL && *x == '/') {
        snprintf(dir, sizeof dir, "%s/ttt", x);
    } else if (h != NULL && *h != 0) {
        snprintf(dir, sizeof dir, "%s/.local/share/ttt", h);
    } else if (sExeDir[0]) {
        snprintf(dir, sizeof dir, "%s", sExeDir);
    } else {
        snprintf(dir, sizeof dir, ".");
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
            mkdir(p, 0755);
            *s = '/';
        }
    }
}
