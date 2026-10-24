/*
 * The window (T30): the displayed picture (both read circuits merged, as the dump) in an SDL3 window.
 *
 * SDL3 is loaded at run time (dlopen of libSDL3.so.0, or TTT_SDL_LIB) through the small function table below: the
 * program does not link it, headless runs and the tests never need it, and without it the run goes on headless with a
 * message. The 32-bit program needs the 32-bit library (Arch: lib32-sdl3 from multilib).
 *
 * SDL runs on a thread of its own: the game's thread runs on the PS2 stack in EE RAM (plat_main.c), 200 KB that grow
 * into the game's .bss, too small and too precious for a video driver. Gs_VBlank (gs_core.c) hands each finished
 * picture over (Port_WindowPresent copies it); the window thread uploads it to a texture and presents it, with the
 * renderer's vsync (TTT_VSYNC=0: without). The game's pace is the platform clock's (plat_hw.c): 59.94 Hz in real
 * time, as fast as it goes headless. Closing the window or Escape ends the program (Port_WindowClosed).
 *
 * Input (T34): the window thread keeps the keyboard's and the first two SDL gamepads' state as two PS2 pads
 * (libpad's button bits, the sticks as bytes); plat_pad.c takes a copy at each VBlank (Port_WindowPads).
 *
 * Only pointer and integer arguments cross into SDL (no SDL_FRect: the port has no host floating point).
 *
 * Renderers (T30 batch 3, TTT_RENDERER=gl / sw): with `gl` (the default when the window opens) the window gets an
 * OpenGL 3.3 core context and the window thread itself is the GS's back end (gs_gl.c): it carries out the GS commands
 * the game's thread queues (Gs_GlPump) and presents each VBlank's picture; when the context or the back end cannot be
 * made, the window falls back to `sw`, the software rasteriser's pictures through an SDL renderer, as before.
 * TTT_WINDOW_HIDDEN=1 keeps the window hidden (tests, measurements); SDL_VIDEO_DRIVER=offscreen also works with GL
 * where SDL has EGL.
 *
 * Closing the window or Escape ends the program from the window thread (the GL / renderer objects are released there
 * first, then _exit): the game's thread may be anywhere, even waiting for the back end.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "port_host.h"
#include "gs.h"
#include "gs_int.h"

int Port_Headless(void); /* plat_hw.c */

/* SDL3's ABI as used here (values from SDL3's headers). */
#define SDL_INIT_VIDEO 0x20u
#define SDL_INIT_GAMEPAD 0x2000u
#define SDL_WINDOW_RESIZABLE 0x20ull
#define SDL_WINDOW_OPENGL 0x2ull
#define SDL_WINDOW_HIDDEN 0x8ull
#define SDL_GL_DOUBLEBUFFER 5
#define SDL_GL_DEPTH_SIZE 6
#define SDL_GL_CONTEXT_MAJOR_VERSION 17
#define SDL_GL_CONTEXT_MINOR_VERSION 18
#define SDL_GL_CONTEXT_PROFILE_MASK 20
#define SDL_GL_CONTEXT_PROFILE_CORE 1
#define SDL_PIXELFORMAT_RGB24 0x17101803u
#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_LOGICAL_PRESENTATION_LETTERBOX 2
#define SDL_SCALEMODE_LINEAR 1
#define SDL_EVENT_QUIT 0x100u
#define SDL_EVENT_WINDOW_CLOSE_REQUESTED 0x210u
#define SDL_EVENT_KEY_DOWN 0x300u
#define SDL_EVENT_KEY_UP 0x301u
#define SDL_EVENT_GAMEPAD_AXIS_MOTION 0x650u
#define SDL_EVENT_GAMEPAD_BUTTON_DOWN 0x651u
#define SDL_EVENT_GAMEPAD_BUTTON_UP 0x652u
#define SDL_EVENT_GAMEPAD_ADDED 0x653u
#define SDL_EVENT_GAMEPAD_REMOVED 0x654u
#define SDL_KEY_OFFSET 28      /* SDL_KeyboardEvent.key */
#define SDL_SCANCODE_OFFSET 24 /* SDL_KeyboardEvent.scancode */
#define SDL_KEY_DOWN_OFFSET 36 /* SDL_KeyboardEvent.down */
#define SDL_WHICH_OFFSET 16    /* SDL_Gamepad{Device,Button,Axis}Event.which */
#define SDL_BUTTON_OFFSET 20   /* SDL_GamepadButtonEvent.button, SDL_GamepadAxisEvent.axis */
#define SDL_BUTTON_DOWN_OFFSET 21
#define SDL_AXIS_VALUE_OFFSET 24
#define SDLK_ESCAPE 0x1Bu
#define SDL_SCANCODE_ESCAPE 41u

static struct {
    _Bool (*Init)(uint32_t flags);
    void (*Quit)(void);
    const char *(*GetError)(void);
    int (*GetVersion)(void);
    void *(*CreateWindow)(const char *title, int w, int h, uint64_t flags);
    void *(*CreateRenderer)(void *window, const char *name);
    const char *(*GetRendererName)(void *renderer);
    _Bool (*SetRenderVSync)(void *renderer, int vsync);
    _Bool (*SetRenderLogicalPresentation)(void *renderer, int w, int h, int mode);
    void *(*CreateTexture)(void *renderer, uint32_t format, int access, int w, int h);
    void (*DestroyTexture)(void *texture);
    _Bool (*SetTextureScaleMode)(void *texture, int mode);
    _Bool (*UpdateTexture)(void *texture, const void *rect, const void *pixels, int pitch);
    _Bool (*RenderClear)(void *renderer);
    _Bool (*RenderTexture)(void *renderer, void *texture, const void *src, const void *dst);
    _Bool (*RenderPresent)(void *renderer);
    _Bool (*PollEvent)(void *event);
    void *(*OpenGamepad)(uint32_t id);
    void (*CloseGamepad)(void *gamepad);
    const char *(*GetGamepadName)(void *gamepad);
    void (*DestroyWindow)(void *window);
    void (*DestroyRenderer)(void *renderer);
    /* OpenGL (optional: without them the window uses the SDL renderer) */
    _Bool (*GL_SetAttribute)(int attr, int value);
    void *(*GL_CreateContext)(void *window);
    _Bool (*GL_MakeCurrent)(void *window, void *context);
    void *(*GL_GetProcAddress)(const char *proc);
    _Bool (*GL_SetSwapInterval)(int interval);
    _Bool (*GL_SwapWindow)(void *window);
    _Bool (*GL_DestroyContext)(void *context);
    _Bool (*GetWindowSizeInPixels)(void *window, int *w, int *h);
} sdl;

static const struct {
    const char *name;
    size_t offset;
} kSdlFuncs[] = {
#define F(n) {"SDL_" #n, offsetof(__typeof__(sdl), n)}
    F(Init), F(Quit), F(GetError), F(GetVersion), F(CreateWindow), F(CreateRenderer), F(GetRendererName), F(SetRenderVSync),
    F(SetRenderLogicalPresentation), F(CreateTexture), F(DestroyTexture), F(SetTextureScaleMode), F(UpdateTexture), F(RenderClear),
    F(RenderTexture), F(RenderPresent), F(PollEvent), F(OpenGamepad), F(CloseGamepad), F(GetGamepadName), F(DestroyWindow),
    F(DestroyRenderer),
#undef F
};
#define SDL_FUNCS_REQUIRED (sizeof kSdlFuncs / sizeof kSdlFuncs[0])
static const struct {
    const char *name;
    size_t offset;
} kSdlGlFuncs[] = {
#define F(n) {"SDL_" #n, offsetof(__typeof__(sdl), n)}
    F(GL_SetAttribute), F(GL_CreateContext), F(GL_MakeCurrent), F(GL_GetProcAddress), F(GL_SetSwapInterval), F(GL_SwapWindow),
    F(GL_DestroyContext), F(GetWindowSizeInPixels),
#undef F
};
static int sHaveGlFuncs;

/* The frame handed over: the newest picture, taken by the window thread. */
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sCond = PTHREAD_COND_INITIALIZER;
static uint8_t *sFrame;
static uint32_t sW, sH, sSeq, sShown;
static int sState;  /* 0 not tried, 1 starting, 2 open, -1 not available */
static int sClosed; /* the user closed the window */
static int sGlMode;  /* the window draws with the GL back end (gs_gl.c) */
static int sQuitReq; /* the game's thread asks the window thread to release the window (end of a TTT_FRAMES run) */
static void *sWindow, *sRenderer, *sGlContext;
static char sWhy[512];

/* ---- input: two PS2 pads from the keyboard and the gamepads ---- */
/* libpad's button bits (1 = pressed) */
#define PB_L2 0x0001u
#define PB_R2 0x0002u
#define PB_L1 0x0004u
#define PB_R1 0x0008u
#define PB_TRIANGLE 0x0010u
#define PB_CIRCLE 0x0020u
#define PB_CROSS 0x0040u
#define PB_SQUARE 0x0080u
#define PB_SELECT 0x0100u
#define PB_L3 0x0200u
#define PB_R3 0x0400u
#define PB_START 0x0800u
#define PB_UP 0x1000u
#define PB_RIGHT 0x2000u
#define PB_DOWN 0x4000u
#define PB_LEFT 0x8000u

/* Keyboard (SDL scancodes, the key's place, whatever the layout): pad 1 on the letters, pad 2 on the arrows and the
   keypad. Tekken's default: square left punch, triangle right punch, cross left kick, circle right kick. */
static const struct {
    uint16_t scancode;
    uint8_t pad;
    uint16_t bit;
} kKeys[] = {
    {26, 0, PB_UP}, {4, 0, PB_LEFT}, {22, 0, PB_DOWN}, {7, 0, PB_RIGHT},          /* W A S D */
    {24, 0, PB_SQUARE}, {12, 0, PB_TRIANGLE}, {13, 0, PB_CROSS}, {14, 0, PB_CIRCLE}, /* U I J K */
    {28, 0, PB_L1}, {18, 0, PB_R1}, {11, 0, PB_L2}, {15, 0, PB_R2},                /* Y O H L */
    {40, 0, PB_START}, {42, 0, PB_SELECT},                                          /* Return, Backspace */
    {82, 1, PB_UP}, {80, 1, PB_LEFT}, {81, 1, PB_DOWN}, {79, 1, PB_RIGHT},          /* arrows */
    {92, 1, PB_SQUARE}, {93, 1, PB_TRIANGLE}, {89, 1, PB_CROSS}, {90, 1, PB_CIRCLE}, /* keypad 4 5 1 2 */
    {95, 1, PB_L1}, {94, 1, PB_R1}, {96, 1, PB_L2}, {97, 1, PB_R2},                /* keypad 7 6 8 9 */
    {88, 1, PB_START}, {87, 1, PB_SELECT},                                          /* keypad Enter, + */
};

/* SDL gamepad buttons (SDL_GamepadButton: south, east, west, north, back, guide, start, sticks, shoulders, d-pad) */
static const uint16_t kPadButtons[15] = {
    PB_CROSS, PB_CIRCLE, PB_SQUARE, PB_TRIANGLE, PB_SELECT, 0, PB_START, PB_L3, PB_R3, PB_L1, PB_R1,
    PB_UP, PB_DOWN, PB_LEFT, PB_RIGHT,
};

static pthread_mutex_t sPadLock = PTHREAD_MUTEX_INITIALIZER;
static uint16_t sKeyButtons[2], sPadButtons[2];
static uint16_t sLatched[2]; /* buttons pressed since the last VBlank's look: a tap between two VBlanks still counts */
static uint8_t sAnalog[2][4] = {{0x80, 0x80, 0x80, 0x80}, {0x80, 0x80, 0x80, 0x80}};
static void *sGamepad[2];  /* the gamepad of each pad, the first two connected */
static uint32_t sGamepadId[2];

static void pad_set(uint16_t *b, uint16_t bit, int down) {
    *b = (uint16_t)(down ? *b | bit : *b & ~bit);
}

static int gamepad_slot(uint32_t id) {
    int i;
    for (i = 0; i < 2; i++) {
        if (sGamepad[i] != NULL && sGamepadId[i] == id) {
            return i;
        }
    }
    return -1;
}

/* One SDL event's effect on the pads (window thread). */
static void input_event(uint32_t type, const uint8_t *ev) {
    uint32_t id, sc;
    int i;

    pthread_mutex_lock(&sPadLock);
    if (type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP) {
        memcpy(&sc, ev + SDL_SCANCODE_OFFSET, 4);
        for (i = 0; i < (int)(sizeof kKeys / sizeof kKeys[0]); i++) {
            if (kKeys[i].scancode == sc) {
                pad_set(&sKeyButtons[kKeys[i].pad], kKeys[i].bit, ev[SDL_KEY_DOWN_OFFSET] != 0);
                if (ev[SDL_KEY_DOWN_OFFSET] != 0) {
                    sLatched[kKeys[i].pad] |= kKeys[i].bit;
                }
            }
        }
    } else if (type >= SDL_EVENT_GAMEPAD_AXIS_MOTION && type <= SDL_EVENT_GAMEPAD_REMOVED) {
        memcpy(&id, ev + SDL_WHICH_OFFSET, 4);
        i = gamepad_slot(id);
        if (type == SDL_EVENT_GAMEPAD_ADDED && i < 0) {
            i = sGamepad[0] == NULL ? 0 : sGamepad[1] == NULL ? 1 : -1;
            if (i >= 0 && (sGamepad[i] = sdl.OpenGamepad(id)) != NULL) {
                const char *name = sdl.GetGamepadName(sGamepad[i]);
                sGamepadId[i] = id;
                printf("ttt: input: pad %d: %s\n", i + 1, name != NULL ? name : "gamepad");
                fflush(stdout);
            }
        } else if (type == SDL_EVENT_GAMEPAD_REMOVED && i >= 0) {
            sdl.CloseGamepad(sGamepad[i]);
            sGamepad[i] = NULL;
            sPadButtons[i] = 0;
            memset(sAnalog[i], 0x80, 4);
        } else if (type == SDL_EVENT_GAMEPAD_AXIS_MOTION && i >= 0) {
            uint8_t axis = ev[SDL_BUTTON_OFFSET];
            int16_t v;
            memcpy(&v, ev + SDL_AXIS_VALUE_OFFSET, 2);
            if (axis < 4) { /* left x, left y, right x, right y -> libpad's right x, right y, left x, left y */
                sAnalog[i][axis ^ 2] = (uint8_t)((v + 32768) >> 8);
            } else if (axis < 6) { /* triggers: L2 / R2 past half way */
                pad_set(&sPadButtons[i], axis == 4 ? PB_L2 : PB_R2, v >= 16384);
            }
        } else if ((type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || type == SDL_EVENT_GAMEPAD_BUTTON_UP) && i >= 0) {
            uint8_t b = ev[SDL_BUTTON_OFFSET];
            if (b < 15) {
                pad_set(&sPadButtons[i], kPadButtons[b], ev[SDL_BUTTON_DOWN_OFFSET] != 0);
                if (ev[SDL_BUTTON_DOWN_OFFSET] != 0) {
                    sLatched[i] |= kPadButtons[b];
                }
            }
        }
    }
    pthread_mutex_unlock(&sPadLock);
}

/* The pads now (plat_pad.c, at a VBlank, on the game's thread): keyboard and gamepad buttons together. */
void Port_WindowPads(uint16_t buttons[2], uint8_t analog[2][4]) {
    int i;
    pthread_mutex_lock(&sPadLock);
    for (i = 0; i < 2; i++) {
        buttons[i] = (uint16_t)(sKeyButtons[i] | sPadButtons[i] | sLatched[i]);
        sLatched[i] = 0;
        memcpy(analog[i], sAnalog[i], 4);
    }
    pthread_mutex_unlock(&sPadLock);
}

/* Loads SDL3 and fills the table; 0 with the reason in sWhy when it cannot. */
static int sdl_load(void) {
    const char *lib = getenv("TTT_SDL_LIB");
    void *h;
    size_t i;

    if (lib == NULL || *lib == 0) {
        lib = "libSDL3.so.0";
    }
    h = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) {
        const char *e = dlerror();
        snprintf(sWhy, sizeof sWhy, "cannot load %s (%s)", lib, e != NULL ? e : "?");
        return 0;
    }
    for (i = 0; i < sizeof kSdlFuncs / sizeof kSdlFuncs[0]; i++) {
        void *f = dlsym(h, kSdlFuncs[i].name);
        if (f == NULL) {
            snprintf(sWhy, sizeof sWhy, "%s has no %s", lib, kSdlFuncs[i].name);
            dlclose(h);
            return 0;
        }
        memcpy((char *)&sdl + kSdlFuncs[i].offset, &f, sizeof f);
    }
    sHaveGlFuncs = 1;
    for (i = 0; i < sizeof kSdlGlFuncs / sizeof kSdlGlFuncs[0]; i++) {
        void *f = dlsym(h, kSdlGlFuncs[i].name);
        sHaveGlFuncs &= f != NULL;
        memcpy((char *)&sdl + kSdlGlFuncs[i].offset, &f, sizeof f);
    }
    return 1;
}

static void set_state(int s) {
    pthread_mutex_lock(&sLock);
    sState = s;
    pthread_cond_broadcast(&sCond);
    pthread_mutex_unlock(&sLock);
}

/* Whether an event ends the program: the window's close button, SDL's quit, Escape (by key or by its place). */
static int quit_event(uint32_t type, const uint8_t *ev) {
    uint32_t key, sc;
    memcpy(&key, ev + SDL_KEY_OFFSET, 4);
    memcpy(&sc, ev + SDL_SCANCODE_OFFSET, 4);
    return type == SDL_EVENT_QUIT || type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
           (type == SDL_EVENT_KEY_DOWN && (key == SDLK_ESCAPE || sc == SDL_SCANCODE_ESCAPE));
}

/* The window's objects, released on the window thread (GL contexts and renderers belong to it). */
static void release(void) {
    if (sGlContext != NULL) {
        sdl.GL_MakeCurrent(sWindow, NULL);
        sdl.GL_DestroyContext(sGlContext);
        sGlContext = NULL;
    }
    if (sRenderer != NULL) {
        sdl.DestroyRenderer(sRenderer);
        sRenderer = NULL;
    }
    if (sWindow != NULL) {
        sdl.DestroyWindow(sWindow);
        sWindow = NULL;
    }
    sdl.Quit();
}

/* The pending events: the pads' state, and the end of the program when the window is closed. */
static void events(void) {
    uint8_t ev[128];
    while (sdl.PollEvent(ev)) {
        uint32_t type;
        memcpy(&type, ev, 4);
        if (quit_event(type, ev)) {
            __atomic_store_n(&sClosed, 1, __ATOMIC_RELEASE);
            printf("ttt: window closed\n");
            fflush(stdout);
            release();
            fflush(stdout);
            _exit(0);
        }
        input_event(type, ev);
    }
}

/* The end of a TTT_FRAMES run with a window: the window thread releases the window, then says so (state 3). */
static int quit_requested(void) {
    if (!__atomic_load_n(&sQuitReq, __ATOMIC_ACQUIRE)) {
        return 0;
    }
    release();
    set_state(3);
    return 1;
}

/* An OpenGL 3.3 core window with the GS's GL back end (gs_gl.c) on this thread; 0 (and nothing left open) when any
   step fails. */
static int gl_open(uint64_t flags, int vsync) {
    char why[1024];

    if (!sHaveGlFuncs) {
        printf("ttt: window: this SDL has no OpenGL functions: software renderer\n");
        return 0;
    }
    sdl.GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    sdl.GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    sdl.GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    sdl.GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    sdl.GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    sWindow = sdl.CreateWindow("Tekken Tag Tournament", 1280, 896, flags | SDL_WINDOW_OPENGL);
    if (sWindow == NULL) {
        printf("ttt: window: no OpenGL window (%s): software renderer\n", sdl.GetError());
        return 0;
    }
    sGlContext = sdl.GL_CreateContext(sWindow);
    if (sGlContext == NULL || !sdl.GL_MakeCurrent(sWindow, sGlContext)) {
        printf("ttt: window: no OpenGL 3.3 context (%s): software renderer\n", sdl.GetError());
    } else {
        sdl.GL_SetSwapInterval(vsync);
        if (Gl_Init(sdl.GL_GetProcAddress, why, (int)sizeof why)) {
            return 1;
        }
        printf("ttt: window: OpenGL back end: %s: software renderer\n", why);
        sdl.GL_MakeCurrent(sWindow, NULL);
        sdl.GL_DestroyContext(sGlContext);
    }
    sGlContext = NULL;
    sdl.DestroyWindow(sWindow);
    sWindow = NULL;
    return 0;
}

static void *window_main(void *arg) {
    void *tex = NULL;
    uint32_t tw = 0, th = 0, seen = 0;
    uint8_t *pic = NULL;
    const char *vs = getenv("TTT_VSYNC"), *rs = getenv("TTT_RENDERER"), *hs = getenv("TTT_WINDOW_HIDDEN");
    int v, vsync = !(vs != NULL && atoi(vs) == 0);
    int64_t lastEvents = 0;
    uint64_t flags = SDL_WINDOW_RESIZABLE | (hs != NULL && atoi(hs) != 0 ? SDL_WINDOW_HIDDEN : 0);

    (void)arg;
    if (!sdl.Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) && !sdl.Init(SDL_INIT_VIDEO)) {
        snprintf(sWhy, sizeof sWhy, "SDL_Init: %s", sdl.GetError());
        set_state(-1);
        return NULL;
    }
    v = sdl.GetVersion();
    if (rs != NULL && strcmp(rs, "sw") != 0 && strcmp(rs, "gl") != 0) {
        printf("ttt: window: TTT_RENDERER=%s unknown (gl or sw): gl\n", rs);
    }
    if (rs == NULL || strcmp(rs, "sw") != 0) {
        sGlMode = gl_open(flags, vsync);
    }
    if (sGlMode) {
        printf("ttt: window: SDL %d.%d.%d, OpenGL renderer (TTT_RENDERER=gl)\n", v / 1000000, v / 1000 % 1000, v % 1000);
        fflush(stdout);
        set_state(2);
        for (;;) { /* the events every 4 ms or so: SDL's event pump is not free, the GS commands come first */
            struct timespec now;
            int64_t ms;
            clock_gettime(CLOCK_MONOTONIC, &now);
            ms = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
            if (ms - lastEvents >= 4 || ms < lastEvents) {
                lastEvents = ms;
                events();
                if (quit_requested()) {
                    return NULL;
                }
            }
            Gs_GlPump(4);
        }
    }
    sWindow = sdl.CreateWindow("Tekken Tag Tournament", 1280, 896, flags);
    sRenderer = sWindow != NULL ? sdl.CreateRenderer(sWindow, NULL) : NULL;
    if (sRenderer == NULL) {
        snprintf(sWhy, sizeof sWhy, "SDL window: %s", sdl.GetError());
        release();
        set_state(-1);
        return NULL;
    }
    sdl.SetRenderVSync(sRenderer, vsync);
    printf("ttt: window: SDL %d.%d.%d, renderer %s (TTT_RENDERER=sw)\n", v / 1000000, v / 1000 % 1000, v % 1000, sdl.GetRendererName(sRenderer));
    fflush(stdout);
    set_state(2);
    for (;;) {
        uint32_t w = 0, h = 0;
        int fresh = 0;

        events();
        if (quit_requested()) {
            return NULL;
        }
        pthread_mutex_lock(&sLock);
        if (sSeq == seen) { /* nothing new: wait for a frame, at most 10 ms (the events) */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 10000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            pthread_cond_timedwait(&sCond, &sLock, &ts);
        }
        if (sSeq != seen && sFrame != NULL) {
            seen = sSeq;
            w = sW;
            h = sH;
            if (w * h != tw * th || pic == NULL) {
                free(pic);
                pic = malloc((size_t)w * h * 3);
            }
            if (pic != NULL) {
                memcpy(pic, sFrame, (size_t)w * h * 3);
                fresh = 1;
            }
        }
        pthread_mutex_unlock(&sLock);
        if (!fresh) {
            continue;
        }
        if (tex == NULL || w != tw || h != th) {
            if (tex != NULL) {
                sdl.DestroyTexture(tex);
            }
            tex = sdl.CreateTexture(sRenderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, (int)w, (int)h);
            if (tex == NULL) {
                continue;
            }
            sdl.SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
            sdl.SetRenderLogicalPresentation(sRenderer, (int)w, (int)h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
            tw = w;
            th = h;
        }
        sdl.UpdateTexture(tex, NULL, pic, (int)w * 3);
        sdl.RenderClear(sRenderer);
        sdl.RenderTexture(sRenderer, tex, NULL, NULL);
        sdl.RenderPresent(sRenderer);
        __atomic_add_fetch(&sShown, 1, __ATOMIC_RELAXED);
    }
    return NULL;
}

/* Whether the run shows a window: TTT_WINDOW=1 / 0, else in real time (not headless, not a self-test). Opens it the
   first time (plat_main.c, before the game starts); when SDL3 is missing or fails, says why once and the run goes on
   headless. */
int Port_WindowOpen(void) {
    const char *e;
    pthread_t t;

    if (sState != 0) {
        return sState == 2;
    }
    e = getenv("TTT_WINDOW");
    if (e != NULL ? atoi(e) == 0 : Port_Headless() || getenv("TTT_SELFTEST") != NULL) {
        sState = -1;
        return 0;
    }
    sState = 1;
    if (!sdl_load()) {
        sState = -1;
    } else if (pthread_create(&t, NULL, window_main, NULL) != 0) {
        snprintf(sWhy, sizeof sWhy, "cannot start the window thread");
        sState = -1;
    } else {
        pthread_detach(t);
        pthread_mutex_lock(&sLock);
        while (sState == 1) {
            pthread_cond_wait(&sCond, &sLock);
        }
        pthread_mutex_unlock(&sLock);
    }
    if (sState != 2) {
        printf("ttt: window: %s: running headless\n", sWhy);
        fflush(stdout);
    }
    return sState == 2;
}

/* A picture for the window (RGB, w x h): copied, the window thread shows the newest one. */
void Port_WindowPresent(const uint8_t *rgb, uint32_t w, uint32_t h) {
    size_t n = (size_t)w * h * 3;

    pthread_mutex_lock(&sLock);
    if (sW * sH != w * h || sFrame == NULL) {
        free(sFrame);
        sFrame = malloc(n);
    }
    if (sFrame != NULL) {
        memcpy(sFrame, rgb, n);
        sW = w;
        sH = h;
        sSeq++;
        pthread_cond_broadcast(&sCond);
    }
    pthread_mutex_unlock(&sLock);
}

int Port_WindowClosed(void) {
    return __atomic_load_n(&sClosed, __ATOMIC_ACQUIRE);
}

/* The TTT_FRAMES report line (only when a window is open); then the window thread releases the window before the
   program exits (GL drivers do not like a context still current on another thread at exit). */
void Port_WindowReport(void) {
    if (sState != 2) {
        return;
    }
    if (sGlMode) {
        printf("ttt: window: OpenGL, %u pictures shown\n", __atomic_load_n(&sShown, __ATOMIC_RELAXED));
    } else {
        printf("ttt: window: %u pictures handed over, %u shown\n", sSeq, __atomic_load_n(&sShown, __ATOMIC_RELAXED));
    }
    fflush(stdout);
    __atomic_store_n(&sQuitReq, 1, __ATOMIC_RELEASE);
    pthread_mutex_lock(&sLock);
    pthread_cond_broadcast(&sCond);
    while (sState == 2) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        if (pthread_cond_timedwait(&sCond, &sLock, &ts) != 0) {
            break; /* the window thread is stuck in the driver: exit anyway */
        }
    }
    pthread_mutex_unlock(&sLock);
}

/* ---- the GL back end's window (gs_gl.c, on the window thread) ---- */
int Port_WindowGl(void) {
    return sState == 2 && sGlMode;
}

void Port_WindowGlSize(int *w, int *h) {
    if (sWindow == NULL || !sdl.GetWindowSizeInPixels(sWindow, w, h)) {
        *w = *h = 0;
    }
}

void Port_WindowGlSwap(void) {
    sdl.GL_SwapWindow(sWindow);
    __atomic_add_fetch(&sShown, 1, __ATOMIC_RELAXED);
}

/* TTT_SELFTEST=window: the keyboard's mapping and the window's quit keys, on made-up SDL events (no SDL, no window,
   nothing sent to the desktop): a tap of Return between two VBlanks is pad 1's Start for one look, a held one until
   released, keypad Enter is pad 2's Start, W A S D the d-pad, Escape (by key code or by place) and the close button end
   the program, other keys do not. */
void Port_WindowSelfTest(void) {
    const char *t = getenv("TTT_SELFTEST");
    uint16_t b[2];
    uint8_t a[2][4];
    int fails = 0;

    if (t == NULL || strcmp(t, "window") != 0) {
        return;
    }
#define KEY(down_, sc_, key_)                                                                                          \
    do {                                                                                                               \
        uint8_t ev[128] = {0};                                                                                         \
        uint32_t ty = (down_) ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP, sc = (sc_), ky = (key_);                       \
        memcpy(ev, &ty, 4);                                                                                            \
        memcpy(ev + SDL_SCANCODE_OFFSET, &sc, 4);                                                                      \
        memcpy(ev + SDL_KEY_OFFSET, &ky, 4);                                                                           \
        ev[SDL_KEY_DOWN_OFFSET] = (uint8_t)(down_);                                                                    \
        input_event(ty, ev);                                                                                           \
    } while (0)
#define EXPECT(ok, what)                                                                                               \
    do {                                                                                                               \
        if (!(ok)) {                                                                                                   \
            printf("window: FAIL %s\n", what);                                                                        \
            fails++;                                                                                                   \
        }                                                                                                              \
    } while (0)
    Port_WindowPads(b, a); /* clear */
    KEY(1, 40, 0x0D);
    KEY(0, 40, 0x0D);
    Port_WindowPads(b, a);
    EXPECT(b[0] == PB_START && b[1] == 0, "a Return tap between two VBlanks is pad 1's Start");
    Port_WindowPads(b, a);
    EXPECT(b[0] == 0, "the tap counts once");
    KEY(1, 40, 0x0D);
    Port_WindowPads(b, a);
    EXPECT(b[0] == PB_START, "Return held: Start");
    Port_WindowPads(b, a);
    EXPECT(b[0] == PB_START, "Return still held: Start");
    KEY(0, 40, 0x0D);
    Port_WindowPads(b, a);
    EXPECT(b[0] == 0, "Return released");
    KEY(1, 88, 0x4000010Eu);
    Port_WindowPads(b, a);
    EXPECT(b[1] == PB_START && b[0] == 0, "keypad Enter is pad 2's Start");
    KEY(0, 88, 0x4000010Eu);
    KEY(1, 26, 'w');
    KEY(1, 4, 'a');
    KEY(1, 22, 's');
    KEY(1, 7, 'd');
    KEY(1, 24, 'u');
    Port_WindowPads(b, a);
    EXPECT(b[0] == (PB_UP | PB_LEFT | PB_DOWN | PB_RIGHT | PB_SQUARE), "W A S D U: the d-pad and square");
    KEY(0, 26, 'w');
    KEY(0, 4, 'a');
    KEY(0, 22, 's');
    KEY(0, 7, 'd');
    KEY(0, 24, 'u');
    Port_WindowPads(b, a);
    EXPECT(b[0] == 0 && b[1] == 0, "all released");
    {
        uint8_t ev[128] = {0};
        uint32_t ty = SDL_EVENT_KEY_DOWN, sc = SDL_SCANCODE_ESCAPE, ky = SDLK_ESCAPE;
        memcpy(ev + SDL_SCANCODE_OFFSET, &sc, 4);
        memcpy(ev + SDL_KEY_OFFSET, &ky, 4);
        EXPECT(quit_event(ty, ev), "Escape ends the program");
        ky = 0x1000u; /* another key code at Escape's place (a remapped layout) */
        memcpy(ev + SDL_KEY_OFFSET, &ky, 4);
        EXPECT(quit_event(ty, ev), "the key at Escape's place ends the program");
        EXPECT(!quit_event(SDL_EVENT_KEY_UP, ev), "releasing Escape does nothing more");
        sc = 40;
        ky = 0x0D;
        memcpy(ev + SDL_SCANCODE_OFFSET, &sc, 4);
        memcpy(ev + SDL_KEY_OFFSET, &ky, 4);
        EXPECT(!quit_event(ty, ev), "Return does not end the program");
        EXPECT(quit_event(SDL_EVENT_WINDOW_CLOSE_REQUESTED, ev), "the close button ends the program");
        EXPECT(quit_event(SDL_EVENT_QUIT, ev), "SDL's quit ends the program");
    }
#undef KEY
#undef EXPECT
    printf(fails ? "ttt: window self-test FAILED (%d)\n" : "ttt: window self-test OK\n", fails);
    fflush(stdout);
    exit(fails != 0);
}
