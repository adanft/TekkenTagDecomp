/*
 * Screen reader output (T48, docs/port/accessibility.md; plat_speech.h).
 *
 * The overlay (overlay.cpp: the item that has the keyboard or gamepad focus, the setup screen's messages) and the
 * game's announcements (plat_a11y.c) hand texts here. Off unless ui.screen_reader (TTT_SCREEN_READER) is on. Outputs,
 * the first that works:
 *
 * - TTT_SR_LOG=<file>: each text appended as a line, at once, nothing spoken (the tests; or a FIFO another program
 *   reads, a braille display's bridge...). An interrupt is a line "--".
 * - speech-dispatcher through libspeechd (loaded at run time with dlopen: no link dependency; a 32-bit program needs
 *   the 32-bit library), in the interface's language.
 * - speech-dispatcher's spd-say command (any bitness), started for each text.
 * - nothing (said once on stderr).
 *
 * A thread of its own speaks (a speech server may be slow to answer); the callers only queue. Nothing here is game
 * state: the game's thread calls Speech_Say off the EE stack (plat_hw.c's VBlank) and reads nothing back.
 */
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "port_host.h"
#include "plat_lang.h"
#include "plat_settings.h"
#include "plat_speech.h"

#define PORT_NOPLT(f) __typeof__(f) f __attribute__((noplt))
PORT_NOPLT(posix_spawnp);
PORT_NOPLT(waitpid);
PORT_NOPLT(fputs);
PORT_NOPLT(fopen);
PORT_NOPLT(fflush);
#undef PORT_NOPLT

extern char **environ;

#define QUEUE 16
#define TEXT 512

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sCond = PTHREAD_COND_INITIALIZER;
static int sOn, sInit, sThread, sOutput = -1; /* -1 not chosen, 0 log, 1 libspeechd, 2 spd-say, 3 none */
static FILE *sLog;
static char sQueue[QUEUE][TEXT];
static int sHead, sCount, sCancel;
static unsigned sSaid;

/* libspeechd (speech-dispatcher's libspeechd.h: SPD_MODE_SINGLE 0, SPD_TEXT 3) */
static void *(*spd_open)(const char *client, const char *conn, const char *user, int mode);
static int (*spd_say)(void *conn, int priority, const char *text);
static int (*spd_cancel)(void *conn);
static int (*spd_set_language)(void *conn, const char *lang);
static void *sSpd;
static char sSpdLang[16];

void Speech_Init(void) {
    const char *log = getenv("TTT_SR_LOG");
    pthread_mutex_lock(&sLock);
    sInit = 1;
    sOn = Settings_GetInt("ui.screen_reader") != 0;
    if (log != NULL && *log && sLog == NULL) {
        sLog = fopen(log, "a");
        if (sLog == NULL) {
            fprintf(stderr, "ttt: screen reader: cannot write %s\n", log);
        } else {
            sOutput = 0;
        }
    }
    pthread_mutex_unlock(&sLock);
}

void Speech_SetEnabled(int on) {
    pthread_mutex_lock(&sLock);
    sOn = on != 0;
    pthread_mutex_unlock(&sLock);
}

int Speech_Enabled(void) { return __atomic_load_n(&sOn, __ATOMIC_RELAXED); }

const char *Speech_Output(void) {
    static const char *const kNames[4] = {"log", "libspeechd", "spd-say", "none"};
    return !sOn ? "off" : sOutput >= 0 ? kNames[sOutput] : "none";
}

/* ---- the speaking thread ---- */
static int open_libspeechd(void) {
    void *h = dlopen("libspeechd.so.2", RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) {
        return 0;
    }
    *(void **)&spd_open = dlsym(h, "spd_open");
    *(void **)&spd_say = dlsym(h, "spd_say");
    *(void **)&spd_cancel = dlsym(h, "spd_cancel");
    *(void **)&spd_set_language = dlsym(h, "spd_set_language");
    if (spd_open == NULL || spd_say == NULL || spd_cancel == NULL ||
        (sSpd = spd_open("ttt", "main", NULL, 0)) == NULL) {
        dlclose(h);
        return 0;
    }
    return 1;
}

static int run(char *const argv[]) {
    pid_t pid;
    int st;
    if (posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ) != 0) {
        return 0;
    }
    return waitpid(pid, &st, 0) == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0;
}

static int spd_say_works(void) {
    char *argv[] = {"spd-say", "-C", NULL};
    return run(argv);
}

static void speak(const char *text, int cancel) {
    const char *lang = Lang_Current();
    if (sOutput == 1) {
        if (spd_set_language != NULL && strcmp(sSpdLang, lang) != 0) {
            snprintf(sSpdLang, sizeof sSpdLang, "%s", lang);
            spd_set_language(sSpd, lang);
        }
        if (cancel) {
            spd_cancel(sSpd);
        }
        spd_say(sSpd, 3 /* SPD_TEXT */, text);
    } else if (sOutput == 2) {
        char l[16];
        char *cancelv[] = {"spd-say", "-C", NULL};
        char *argv[] = {"spd-say", "-l", l, "--", (char *)text, NULL};
        snprintf(l, sizeof l, "%s", lang);
        if (cancel) {
            run(cancelv);
        }
        run(argv);
    }
}

static void *speaker(void *arg) {
    char text[TEXT];
    (void)arg;
    if (open_libspeechd()) {
        sOutput = 1;
    } else if (spd_say_works()) {
        sOutput = 2;
    } else {
        sOutput = 3;
        fprintf(stderr, "ttt: screen reader: no speech-dispatcher (libspeechd.so.2 or spd-say): nothing is spoken; "
                        "TTT_SR_LOG=<file> writes the texts to a file\n");
    }
    for (;;) {
        int cancel;
        pthread_mutex_lock(&sLock);
        while (sCount == 0) {
            pthread_cond_wait(&sCond, &sLock);
        }
        memcpy(text, sQueue[sHead], TEXT);
        sHead = (sHead + 1) % QUEUE;
        sCount--;
        cancel = sCancel;
        sCancel = 0;
        pthread_mutex_unlock(&sLock);
        if (sOutput != 3) {
            speak(text, cancel);
        }
    }
    return NULL;
}

void Speech_Say(const char *text, int interrupt) {
    if (!__atomic_load_n(&sOn, __ATOMIC_RELAXED) || text == NULL || *text == 0) {
        return;
    }
    pthread_mutex_lock(&sLock);
    sSaid++;
    if (sLog != NULL) { /* the log: at once, in order */
        if (interrupt) {
            fputs("--\n", sLog);
        }
        fprintf(sLog, "%s\n", text);
        fflush(sLog);
        pthread_mutex_unlock(&sLock);
        return;
    }
    if (!sThread) {
        pthread_t t;
        sThread = 1;
        if (pthread_create(&t, NULL, speaker, NULL) == 0) {
            pthread_detach(t);
        } else {
            sOutput = 3;
        }
    }
    if (interrupt) { /* what was waiting is dropped too */
        sCount = 0;
        sCancel = 1;
    }
    if (sCount == QUEUE) { /* full: the oldest goes */
        sHead = (sHead + 1) % QUEUE;
        sCount--;
    }
    snprintf(sQueue[(sHead + sCount) % QUEUE], TEXT, "%s", text);
    sCount++;
    pthread_cond_broadcast(&sCond);
    pthread_mutex_unlock(&sLock);
}

unsigned Port_SpeechSaid(void) { return sSaid; }
