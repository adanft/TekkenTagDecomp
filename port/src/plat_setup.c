/*
 * The setup (T43, docs/port/install.md): the player's disc, checked, and the data file made from it, in the program
 * itself (no Python, no other tool).
 *
 * plat_data.c tries at start (a constructor) to load the data file or to make it from the disc it finds (TTT_DISC, the
 * settings' game.disc, iso/). When it cannot (no disc, a refused disc) or the setup was asked for, it leaves EE RAM
 * empty and the reason in Port_DataPending, and __wrap_main (plat_main.c) comes here before the game starts:
 *
 * - `ttt --setup <disc>` (scripts, no window): checks the disc (a folder, a mounted disc, an .iso, a .bin or a .cue),
 *   makes the data file, remembers the disc as game.disc in the settings file, prints what it did and exits: 0, 2 (no
 *   disc there, or the data file cannot be written), 4 (not the unmodified USA disc, the reason on stderr).
 * - a run in real time with a window (or `ttt --setup` alone, or TTT_SETUP_TEST): the setup screen (overlay.cpp, on the
 *   window thread over a blank picture). The player types or pastes a path, or picks it in the built-in browser (or
 *   the system's file dialog when SDL has one); this thread checks that disc and makes the data file (Port_DataInstall,
 *   about two seconds: the whole TEKKEN.BIN is hashed) while the screen shows the progress; a refused disc shows why
 *   and the player picks again. When it is done the game starts at once.
 * - otherwise (TTT_FRAMES runs, no window): the reason on stderr and its exit code, as before T43.
 *
 * TTT_SETUP_TEST=<disc> (tests, with TTT_WINDOW=1 TTT_WINDOW_HIDDEN=1): the setup screen drives itself from inside
 * (overlay.cpp): it lists the disc's folder in the browser, types the path and presses the button; no input from the
 * desktop. A refused disc ends the run with its exit code once the screen has shown the reason.
 *
 * The host stack only (before the game's stack switch), and the window thread through the functions below (a mutex).
 */
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "port_host.h"
#include "overlay.h"
#include "plat_settings.h"

/* C library calls the setup added, through the GOT (port_host.h: the PLT stays as it was) */
#define PORT_NOPLT(f) __typeof__(f) f __attribute__((noplt))
PORT_NOPLT(opendir);
PORT_NOPLT(readdir);
PORT_NOPLT(closedir);
PORT_NOPLT(stat);
PORT_NOPLT(qsort);
PORT_NOPLT(realpath);
PORT_NOPLT(strncmp);
#undef PORT_NOPLT

/* plat_data.c, plat_hw.c, plat_window.c */
int Port_DataPending(void);
const char *Port_DataError(void);
int Port_DataInstall(const char *disc);
void Port_DataProgress(int *permille, const char **stage);
const char *Port_DataPath(void);
int Port_Headless(void);
int Port_WindowOpen(void);
void Port_WindowPresent(const uint8_t *rgb, uint32_t w, uint32_t h);
void Port_WindowExit(int code);
int Port_WindowGl(void);

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sCond = PTHREAD_COND_INITIALIZER;
static int sActive, sBusy, sAsked;
static char sPath[1024], sError[700], sRequest[1024];
static int sTestFrames, sTestEntries, sTestErrorFrames; /* TTT_SETUP_TEST */

/* The disc's path made absolute (the settings file keeps it for runs from any folder). */
static void absolute(const char *in, char *out, int n) {
    char buf[PATH_MAX];
    if (realpath(in, buf) != NULL) {
        snprintf(out, (size_t)n, "%s", buf);
    } else {
        snprintf(out, (size_t)n, "%s", in);
    }
}

/* After a setup: game.disc in the settings file, so later runs find the disc. 1 when written. */
static int remember(const char *disc) {
    if (Settings_SetStr("game.disc", disc)) {
        return 1;
    }
    fprintf(stderr, "ttt: setup: the disc could not be remembered (%s): set TTT_DISC=%s\n", Settings_SaveError(),
            disc);
    return 0;
}

/* `--setup <disc>`: the setup without a window; never returns. */
static void setup_cli(const char *arg) {
    char disc[1024];
    int code;
    absolute(arg, disc, sizeof disc);
    printf("ttt: setup: checking %s\n", disc);
    fflush(stdout);
    code = Port_DataInstall(disc);
    if (code != 0) {
        fflush(stdout);
        fprintf(stderr, "ttt: setup: %s\n", Port_DataError());
        exit(code);
    }
    remember(disc);
    printf("ttt: setup: done: Tekken Tag Tournament (USA, SLUS-20001) verified; data file %s; disc remembered in %s\n",
           Port_DataPath(), *Settings_Path() ? Settings_Path() : "(no settings file)");
    fflush(stdout);
    exit(0);
}

/* The command line: `--setup <disc>` runs the setup here and exits; `--setup` alone asks for the setup screen. */
void Port_SetupArgs(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--setup") == 0) {
            if (i + 1 < argc && strncmp(argv[i + 1], "--", 2) != 0) {
                setup_cli(argv[i + 1]);
            }
            sAsked = 1;
        }
    }
}

/* ---- the window thread's side (overlay.cpp) ---- */

int Port_SetupActive(void) { return __atomic_load_n(&sActive, __ATOMIC_ACQUIRE); }

/* What the setup screen shows: busy (a disc being checked) with its progress, the last error, the path to start from. */
void Port_SetupView(PortSetupView *v) {
    const char *stage;
    pthread_mutex_lock(&sLock);
    v->busy = sBusy;
    snprintf(v->error, sizeof v->error, "%s", sError);
    snprintf(v->path, sizeof v->path, "%s", sPath);
    pthread_mutex_unlock(&sLock);
    Port_DataProgress(&v->permille, &stage);
    snprintf(v->stage, sizeof v->stage, "%s", stage != NULL ? stage : "");
    v->dataPath = Port_DataPath();
}

/* The player pressed "Check and install" with `path`. */
void Port_SetupRequest(const char *path) {
    pthread_mutex_lock(&sLock);
    if (!sBusy && path != NULL && *path) {
        snprintf(sRequest, sizeof sRequest, "%s", path);
        sBusy = 1;
        sError[0] = 0;
        pthread_cond_broadcast(&sCond);
    }
    pthread_mutex_unlock(&sLock);
}

static int entry_cmp(const void *a, const void *b) {
    const PortSetupEntry *x = a, *y = b;
    if ((x->kind == 2) != (y->kind == 2)) {
        return x->kind == 2 ? 1 : -1; /* folders first */
    }
    return strcmp(x->name, y->name);
}

static int image_name(const char *n) {
    size_t k = strlen(n);
    char e[5];
    int i;
    if (k < 5 || n[k - 4] != '.') {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        char c = n[k - 4 + i];
        e[i] = c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
    }
    e[4] = 0;
    return strcmp(e, ".iso") == 0 || strcmp(e, ".bin") == 0 || strcmp(e, ".cue") == 0;
}

/* The built-in browser: folder `dir`'s sub-folders (kind 1; 3 when it holds TEKKEN.BIN, a disc's folder) and disc
   images (kind 2: .iso, .bin, .cue), by name, hidden entries left out. The count (at most `max`), -1 when it cannot be
   read. */
int Port_SetupList(const char *dir, PortSetupEntry *out, int max) {
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    if (d == NULL) {
        return -1;
    }
    while (n < max && (e = readdir(d)) != NULL) {
        char p[2300];
        struct stat st;
        if (e->d_name[0] == '.') {
            continue;
        }
        snprintf(p, sizeof p, "%s/%s", strcmp(dir, "/") == 0 ? "" : dir, e->d_name);
        if (stat(p, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            char t[2400];
            struct stat ts;
            snprintf(t, sizeof t, "%s/TEKKEN.BIN", p);
            out[n].kind = stat(t, &ts) == 0 ? 3 : (snprintf(t, sizeof t, "%s/tekken.bin", p), stat(t, &ts) == 0 ? 3 : 1);
        } else if (S_ISREG(st.st_mode) && image_name(e->d_name)) {
            out[n].kind = 2;
        } else {
            continue;
        }
        snprintf(out[n].name, sizeof out[n].name, "%s", e->d_name);
        n++;
    }
    closedir(d);
    qsort(out, (size_t)n, sizeof out[0], entry_cmp);
    return n;
}

/* TTT_SETUP_TEST: the screen's frames, the browser's entries and the frames that showed an error. */
void Port_SetupTestNote(int entries, int errorShown) {
    pthread_mutex_lock(&sLock);
    sTestFrames++;
    sTestEntries = entries; /* the browser as last drawn */
    sTestErrorFrames += errorShown != 0;
    pthread_cond_broadcast(&sCond);
    pthread_mutex_unlock(&sLock);
}

/* ---- the setup screen's run (host stack, before the game) ---- */

static void test_report(void) {
    pthread_mutex_lock(&sLock);
    printf("ttt: setup: screen test: %d frames drawn (%s), browser %d entries, error shown %d frames\n", sTestFrames,
           Port_WindowGl() ? "gl" : "sw", sTestEntries, sTestErrorFrames);
    pthread_mutex_unlock(&sLock);
    fflush(stdout);
}

static void wait_until(int64_t ns) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)(ns / 1000000000);
    ts.tv_nsec += (long)(ns % 1000000000);
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    pthread_cond_timedwait(&sCond, &sLock, &ts);
}

static void screen(int pending) {
    static uint8_t black[640 * 448 * 3];
    const char *test = getenv("TTT_SETUP_TEST");
    char req[1024], disc[1024];
    int code, i;

    pthread_mutex_lock(&sLock);
    snprintf(sPath, sizeof sPath, "%s", test != NULL && *test ? "" : Port_DiscName());
    snprintf(sError, sizeof sError, "%s", pending > 0 ? Port_DataError() : "");
    pthread_mutex_unlock(&sLock);
    Port_WindowPresent(black, 640, 448); /* the software renderer's picture under the screen */
    __atomic_store_n(&sActive, 1, __ATOMIC_RELEASE);
    printf("ttt: setup: the setup screen is open%s%s\n", pending > 0 ? ": " : "", pending > 0 ? Port_DataError() : "");
    fflush(stdout);
    for (;;) {
        pthread_mutex_lock(&sLock);
        while (!sBusy) {
            wait_until(100000000);
        }
        snprintf(req, sizeof req, "%s", sRequest);
        pthread_mutex_unlock(&sLock);

        absolute(req, disc, sizeof disc);
        printf("ttt: setup: checking %s\n", disc);
        fflush(stdout);
        code = Port_DataInstall(disc);
        pthread_mutex_lock(&sLock);
        sBusy = 0;
        if (code == 0) {
            pthread_mutex_unlock(&sLock);
            break;
        }
        snprintf(sError, sizeof sError, "%s", Port_DataError());
        snprintf(sPath, sizeof sPath, "%s", req);
        pthread_mutex_unlock(&sLock);
        fflush(stdout);
        fprintf(stderr, "ttt: setup: %s\n", Port_DataError());
        if (test != NULL && *test) { /* the test ends here once the screen has shown the reason */
            pthread_mutex_lock(&sLock);
            for (i = 0; i < 100 && sTestErrorFrames < 3; i++) {
                wait_until(100000000);
            }
            pthread_mutex_unlock(&sLock);
            test_report();
            Port_WindowExit(code);
        }
    }
    __atomic_store_n(&sActive, 0, __ATOMIC_RELEASE);
    remember(disc);
    printf("ttt: setup: done: Tekken Tag Tournament (USA, SLUS-20001) verified; data file %s\n", Port_DataPath());
    if (test != NULL && *test) {
        test_report();
    }
    fflush(stdout);
}

/* After the window opened (plat_main.c): when the game's data is not in EE RAM yet, the setup screen, or the reason on
   stderr and its exit code. Returns once the data is loaded. */
void Port_SetupRun(void) {
    int pending = Port_DataPending();
    const char *test = getenv("TTT_SETUP_TEST");
    int asked = pending == -1 || sAsked || (test != NULL && *test);
    if (pending == 0 && !asked) {
        return;
    }
    if (!Port_WindowOpen() || !Overlay_Ready() || (Port_Headless() && !asked)) { /* no screen to show */
        fflush(stdout);
        if (pending == -1) {
            fprintf(stderr, "ttt: setup: the setup screen needs a window: run `ttt --setup <disc>` with your disc's "
                            "folder or image\n");
            exit(2);
        }
        fprintf(stderr, "ttt: %s\n", Port_DataError());
        exit(pending);
    }
    screen(pending);
}
