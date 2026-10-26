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
 * Input (T34): the window thread keeps the keyboard's state (pads 1 and 2) and the first four SDL gamepads' (pads 1-4,
 * in the order they connect) raw; plat_pad.c takes the pads at each VBlank (Port_WindowPads), made there into libpad's
 * button bits and stick bytes through the bindings (keys.*, pad.*), the sticks' dead zone (input.deadzone) and the left
 * stick as the d-pad (input.stick_dpad). The game's vibration (plat_pad.c, Port_WindowRumble) goes to the pad's
 * gamepad from this thread (SDL_RumbleGamepad, input.vibration); the guide button opens the settings, as F1 does.
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
 * first, then _exit): the game's thread may be anywhere, even waiting for the back end. Since T36 they first ask
 * "Quit?" in the overlay (Escape again or Yes quits; TTT_NO_QUIT_CONFIRM=1: at once, as before).
 *
 * The settings overlay (T36, overlay.cpp, docs/port/settings.md): F1 opens it over the picture, drawn on this thread
 * just before each present (GL: Port_WindowGlSwap; sw: the renderer loop). While it is open the pads see nothing
 * pressed (a key pressed then never reaches them; releases do) and, with game.pause, the game's clock waits at its next
 * poll (Port_WindowPaused, plat_hw.c): the simulation does not advance, and goes on from the same state. The live
 * settings (vsync, window size, fullscreen, volume, the keyboard's bindings) are applied here (Port_WindowApply).
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
#include "overlay.h"
#include "plat_net.h"
#include "plat_settings.h"

int Port_Headless(void); /* plat_hw.c */
int Port_NetWindow(void); /* plat_net.c: a netplay session shows a window (on the netplay clock) */

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
#define SDL_KEY_REPEAT_OFFSET 37 /* SDL_KeyboardEvent.repeat */
#define SDL_WINDOW_FULLSCREEN 0x1ull
#define SDL_LOGICAL_PRESENTATION_DISABLED 0
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
    /* audio (optional: without them the window is silent) */
    _Bool (*InitSubSystem)(uint32_t flags);
    void *(*OpenAudioDeviceStream)(uint32_t device, const void *spec, void *callback, void *userdata);
    _Bool (*ResumeAudioStreamDevice)(void *stream);
    _Bool (*PutAudioStreamData)(void *stream, const void *buf, int len);
    int (*GetAudioStreamQueued)(void *stream);
    void (*DestroyAudioStream)(void *stream);
    /* the overlay and the live settings (optional) */
    _Bool (*GetWindowSize)(void *window, int *w, int *h);
    _Bool (*SetWindowSize)(void *window, int w, int h);
    _Bool (*SetWindowFullscreen)(void *window, _Bool on);
    _Bool (*RenderGeometryRaw)(void *renderer, void *texture, const float *xy, int xyStride, const void *color,
                               int colorStride, const float *uv, int uvStride, int vertices, const void *indices,
                               int nIndices, int indexSize);
    _Bool (*SetRenderClipRect)(void *renderer, const void *rect);
    _Bool (*SetTextureBlendMode)(void *texture, uint32_t mode);
    _Bool (*SetWindowTitle)(void *window, const char *title); /* optional: the data mark (T41) */
    _Bool (*RumbleGamepad)(void *gamepad, uint16_t low, uint16_t high, uint32_t ms); /* optional: vibration (T34) */
    /* optional: the setup screen's text entry, clipboard and the system's file dialog (T43) */
    _Bool (*StartTextInput)(void *window);
    char *(*GetClipboardText)(void);
    _Bool (*SetClipboardText)(const char *text);
    void (*free)(void *p);
    void (*ShowOpenFileDialog)(void (*cb)(void *user, const char *const *files, int filter), void *user, void *window,
                               const void *filters, int nfilters, const char *start, _Bool many);
    void (*ShowOpenFolderDialog)(void (*cb)(void *user, const char *const *files, int filter), void *user,
                                 void *window, const char *start, _Bool many);
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
static const struct {
    const char *name;
    size_t offset;
} kSdlAudioFuncs[] = {
#define F(n) {"SDL_" #n, offsetof(__typeof__(sdl), n)}
    F(InitSubSystem), F(OpenAudioDeviceStream), F(ResumeAudioStreamDevice), F(PutAudioStreamData),
    F(GetAudioStreamQueued), F(DestroyAudioStream),
#undef F
};
static int sHaveAudioFuncs;
static const struct {
    const char *name;
    size_t offset;
} kSdlOverlayFuncs[] = {
#define F(n) {"SDL_" #n, offsetof(__typeof__(sdl), n)}
    F(GetWindowSize), F(SetWindowSize), F(SetWindowFullscreen), F(RenderGeometryRaw), F(SetRenderClipRect),
    F(SetTextureBlendMode),
#undef F
};
static int sHaveOverlayFuncs;

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

/* ---- input: PS2 pads from the keyboard and the gamepads ---- */
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
#define NGAMEPADS SETTINGS_PADS

/* Keyboard (SDL scancodes, the key's place, whatever the layout): [pad][SB_*] (plat_settings.h), from the settings
   file over the defaults (pad 1 on the letters, pad 2 on the arrows and the keypad; Tekken's default: square left
   punch, triangle right punch, cross left kick, circle right kick). Window thread only. */
static uint16_t sBind[2][SB_COUNT];
static const uint16_t kBindBits[SB_GAMEPAD_COUNT] = {PB_UP, PB_DOWN, PB_LEFT, PB_RIGHT, PB_SQUARE, PB_TRIANGLE,
                                                     PB_CROSS, PB_CIRCLE, PB_L1, PB_R1, PB_L2, PB_R2, PB_START,
                                                     PB_SELECT, PB_L3, PB_R3};

static pthread_mutex_t sPadLock = PTHREAD_MUTEX_INITIALIZER;
static uint16_t sKeyButtons[2];
static uint16_t sLatched[2]; /* keys pressed since the last VBlank's look: a tap between two VBlanks still counts */
/* The gamepads, raw (under sPadLock): SDL buttons down (bit = SDL_GamepadButton), pressed since the last look, axes
   (left x, left y, right x, right y, left trigger, right trigger). Bindings [pad][SB_* / SB_L3 / SB_R3]. */
static uint32_t sGpDown[NGAMEPADS], sGpLatched[NGAMEPADS];
static int16_t sGpAxis[NGAMEPADS][6];
static uint8_t sGpBind[NGAMEPADS][SB_GAMEPAD_COUNT];
static int sDeadzone = 20, sStickDpad = 1, sVibration = 1; /* input.* as last applied (under sPadLock) */
static void *sGamepad[NGAMEPADS]; /* the gamepad of each pad, the first four connected (window thread) */
static uint32_t sGamepadId[NGAMEPADS];
static char sGamepadName[NGAMEPADS][128];
static uint32_t sRumbleWant[NGAMEPADS]; /* the game's motors (atomic): large * 257 << 16 | small ? 0xFFFF : 0 */
static uint32_t sRumbleSent[NGAMEPADS];
static int64_t sRumbleAt[NGAMEPADS];
static int sNoRumble; /* TTT_WINDOW_HIDDEN: a hidden window (the tests) never rumbles a gamepad */
static int sInputBlocked; /* the overlay is open: the pads see nothing pressed (under sPadLock) */
static int sPaused;       /* the overlay is open and game.pause: the game's clock waits (atomic) */
static int sFastHeld;     /* Tab (SETTINGS_SC_FAST) is held: game.fast_forward instead of game.speed (window thread) */
static void apply_speed(void);

static void pad_set(uint16_t *b, uint16_t bit, int down) {
    *b = (uint16_t)(down ? *b | bit : *b & ~bit);
}

static int gamepad_slot(uint32_t id) {
    int i;
    for (i = 0; i < NGAMEPADS; i++) {
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
        for (i = 0; i < 2 * SB_COUNT; i++) {
            if (sBind[i / SB_COUNT][i % SB_COUNT] == sc) {
                pad_set(&sKeyButtons[i / SB_COUNT], kBindBits[i % SB_COUNT], ev[SDL_KEY_DOWN_OFFSET] != 0);
                if (ev[SDL_KEY_DOWN_OFFSET] != 0) {
                    sLatched[i / SB_COUNT] |= kBindBits[i % SB_COUNT];
                }
            }
        }
    } else if (type >= SDL_EVENT_GAMEPAD_AXIS_MOTION && type <= SDL_EVENT_GAMEPAD_REMOVED) {
        memcpy(&id, ev + SDL_WHICH_OFFSET, 4);
        i = gamepad_slot(id);
        if (type == SDL_EVENT_GAMEPAD_ADDED && i < 0) {
            for (i = 0; i < NGAMEPADS && sGamepad[i] != NULL; i++) {
            }
            if (i < NGAMEPADS && (sGamepad[i] = sdl.OpenGamepad(id)) != NULL) {
                const char *name = sdl.GetGamepadName(sGamepad[i]);
                sGamepadId[i] = id;
                snprintf(sGamepadName[i], sizeof sGamepadName[i], "%s", name != NULL ? name : "gamepad");
                sRumbleSent[i] = 0;
                printf("ttt: input: pad %d: %s\n", i + 1, name != NULL ? name : "gamepad");
                fflush(stdout);
            }
        } else if (type == SDL_EVENT_GAMEPAD_REMOVED && i >= 0) {
            sdl.CloseGamepad(sGamepad[i]);
            sGamepad[i] = NULL;
            sGamepadName[i][0] = 0;
            sGpDown[i] = sGpLatched[i] = 0;
            memset(sGpAxis[i], 0, sizeof sGpAxis[i]);
        } else if (type == SDL_EVENT_GAMEPAD_AXIS_MOTION && i >= 0) {
            uint8_t axis = ev[SDL_BUTTON_OFFSET];
            int16_t v;
            memcpy(&v, ev + SDL_AXIS_VALUE_OFFSET, 2);
            if (axis < 6) {
                sGpAxis[i][axis] = v;
            }
        } else if ((type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || type == SDL_EVENT_GAMEPAD_BUTTON_UP) && i >= 0) {
            uint8_t b = ev[SDL_BUTTON_OFFSET];
            if (b < 32) {
                sGpDown[i] = ev[SDL_BUTTON_DOWN_OFFSET] != 0 ? sGpDown[i] | 1u << b : sGpDown[i] & ~(1u << b);
                if (ev[SDL_BUTTON_DOWN_OFFSET] != 0) {
                    sGpLatched[i] |= 1u << b;
                }
            }
        }
    }
    pthread_mutex_unlock(&sPadLock);
}

/* One stick (SDL's -32768..32767, y down) as libpad's two bytes (0x80 centre, y down) past the dead zone (`dz` percent,
   round: inside it the stick is centred; outside, each axis is stretched to the full range again), and the d-pad
   directions it presses when `dpad` is set: past half way, in eight 45-degree sectors. Integers only. */
static void stick(int x, int y, int dz, int dpad, uint8_t out[2], uint16_t *buttons) {
    uint32_t ax = (uint32_t)(x < 0 ? -x : x), ay = (uint32_t)(y < 0 ? -y : y), d = (uint32_t)dz * 32767u / 100u;
    uint32_t sx, sy;

    out[0] = out[1] = 0x80;
    if (ax * ax + ay * ay < d * d) {
        return;
    }
    sx = ax <= d ? 0 : (ax - d) * 32767u / (32767u - d);
    sy = ay <= d ? 0 : (ay - d) * 32767u / (32767u - d);
    sx = sx > 32767 ? 32767 : sx;
    sy = sy > 32767 ? 32767 : sy;
    out[0] = (uint8_t)((x < 0 ? 32768 - sx : 32768 + sx) >> 8);
    out[1] = (uint8_t)((y < 0 ? 32768 - sy : 32768 + sy) >> 8);
    if (dpad && (sx >= 16384 || sy >= 16384)) {
        if (sy * 29 <= sx * 70) { /* within 67.5 degrees of the x axis (tan 67.5 = 2.414 ~ 70 / 29) */
            *buttons |= x < 0 ? PB_LEFT : PB_RIGHT;
        }
        if (sx * 29 <= sy * 70) {
            *buttons |= y < 0 ? PB_UP : PB_DOWN;
        }
    }
}

/* A gamepad's buttons through its pad's bindings (SDL buttons, the triggers past half way). Lock held. */
static uint16_t gamepad_buttons(int i) {
    uint32_t bits = sGpDown[i] | sGpLatched[i];
    uint16_t b = 0;
    int k;
    for (k = 0; k < SB_GAMEPAD_COUNT; k++) {
        unsigned in = sGpBind[i][k];
        int down = in < SETTINGS_GP_LTRIGGER ? (int)(bits >> in & 1)
                   : in == SETTINGS_GP_LTRIGGER  ? sGpAxis[i][4] >= 16384
                   : in == SETTINGS_GP_RTRIGGER  ? sGpAxis[i][5] >= 16384
                                                 : 0;
        if (down) {
            b |= kBindBits[k];
        }
    }
    return b;
}

/* The pads now (plat_pad.c, at a VBlank, on the game's thread): keyboard (pads 1 and 2) and gamepad buttons together,
   the sticks; nothing pressed while the overlay is open. */
void Port_WindowPads(uint16_t buttons[4], uint8_t analog[4][4]) {
    int i;
    pthread_mutex_lock(&sPadLock);
    for (i = 0; i < NGAMEPADS; i++) {
        uint16_t b = i < 2 ? (uint16_t)(sKeyButtons[i] | sLatched[i]) : 0;
        if (sGamepad[i] != NULL) {
            b |= gamepad_buttons(i);
            stick(sGpAxis[i][2], sGpAxis[i][3], sDeadzone, 0, analog[i], &b);
            stick(sGpAxis[i][0], sGpAxis[i][1], sDeadzone, sStickDpad, analog[i] + 2, &b);
        } else {
            memset(analog[i], 0x80, 4);
        }
        buttons[i] = sInputBlocked ? 0 : b;
        if (sInputBlocked) {
            memset(analog[i], 0x80, 4);
        }
        if (i < 2) {
            sLatched[i] = 0;
        }
        sGpLatched[i] = 0;
    }
    pthread_mutex_unlock(&sPadLock);
}

/* The game's motors for pad `pad` (plat_pad.c, game's thread): the small one on or off, the large one 0-255. */
void Port_WindowRumble(int pad, int small, int large) {
    if (pad >= 0 && pad < NGAMEPADS) {
        uint32_t v = (uint32_t)(large & 0xFF) * 257u << 16 | (small ? 0xFFFFu : 0u);
        __atomic_store_n(&sRumbleWant[pad], v, __ATOMIC_RELAXED);
    }
}

/* The motors to the gamepads (window thread, with the events): a change at once, a running motor again every half
   second for one second (the game may stop, or this thread, and SDL ends a rumble when its time runs out); off while
   the overlay is open, with input.vibration off, and in a hidden window. */
static void rumble_update(int64_t now) {
    int i;
    for (i = 0; i < NGAMEPADS; i++) {
        uint32_t want = __atomic_load_n(&sRumbleWant[i], __ATOMIC_RELAXED);
        if (sGamepad[i] == NULL || sdl.RumbleGamepad == NULL) {
            continue;
        }
        if (!sVibration || sNoRumble || Overlay_Open()) {
            want = 0;
        }
        if (want != sRumbleSent[i] || (want != 0 && now - sRumbleAt[i] > 500000000)) {
            sdl.RumbleGamepad(sGamepad[i], (uint16_t)(want >> 16), (uint16_t)want, want != 0 ? 1000 : 0);
            sRumbleSent[i] = want;
            sRumbleAt[i] = now;
        }
    }
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
    sHaveAudioFuncs = 1;
    for (i = 0; i < sizeof kSdlAudioFuncs / sizeof kSdlAudioFuncs[0]; i++) {
        void *f = dlsym(h, kSdlAudioFuncs[i].name);
        sHaveAudioFuncs &= f != NULL;
        memcpy((char *)&sdl + kSdlAudioFuncs[i].offset, &f, sizeof f);
    }
    sHaveOverlayFuncs = 1;
    for (i = 0; i < sizeof kSdlOverlayFuncs / sizeof kSdlOverlayFuncs[0]; i++) {
        void *f = dlsym(h, kSdlOverlayFuncs[i].name);
        sHaveOverlayFuncs &= f != NULL;
        memcpy((char *)&sdl + kSdlOverlayFuncs[i].offset, &f, sizeof f);
    }
    *(void **)&sdl.SetWindowTitle = dlsym(h, "SDL_SetWindowTitle");
    *(void **)&sdl.RumbleGamepad = dlsym(h, "SDL_RumbleGamepad");
    *(void **)&sdl.StartTextInput = dlsym(h, "SDL_StartTextInput");
    *(void **)&sdl.GetClipboardText = dlsym(h, "SDL_GetClipboardText");
    *(void **)&sdl.SetClipboardText = dlsym(h, "SDL_SetClipboardText");
    *(void **)&sdl.free = dlsym(h, "SDL_free");
    *(void **)&sdl.ShowOpenFileDialog = dlsym(h, "SDL_ShowOpenFileDialog");
    *(void **)&sdl.ShowOpenFolderDialog = dlsym(h, "SDL_ShowOpenFolderDialog");
    return 1;
}

static void set_state(int s) {
    pthread_mutex_lock(&sLock);
    sState = s;
    pthread_cond_broadcast(&sCond);
    pthread_mutex_unlock(&sLock);
}

/* Whether an event asks to end the program: the window's close button, SDL's quit, Escape (by key or by its place; not
   its key repeat). */
static int quit_event(uint32_t type, const uint8_t *ev) {
    uint32_t key, sc;
    memcpy(&key, ev + SDL_KEY_OFFSET, 4);
    memcpy(&sc, ev + SDL_SCANCODE_OFFSET, 4);
    return type == SDL_EVENT_QUIT || type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
           (type == SDL_EVENT_KEY_DOWN && ev[SDL_KEY_REPEAT_OFFSET] == 0 && (key == SDLK_ESCAPE || sc == SDL_SCANCODE_ESCAPE));
}

/* ---- sound (T32): the frames plat_snd.c mixes at each VBlank, from its ring into an SDL audio stream ---- */
#define SDL_INIT_AUDIO 0x10u
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK 0xFFFFFFFFu
#define SDL_AUDIO_S16LE 0x8010
#define AUDIO_MAX_QUEUED (48000 / 10 * 4) /* bytes queued in SDL: 0.1 s; more and the ring's backlog is dropped */
int Port_SndRingRead(int16_t *out, int max); /* plat_snd.c */
void Port_SndRingEnable(int on);
static void *sAudio;

/* Opens the default playback device (48 kHz, 16-bit stereo) unless TTT_AUDIO=0; without it the window is silent. */
static void audio_open(void) {
    const char *e = getenv("TTT_AUDIO");
    struct {
        int format, channels, freq;
    } spec = { SDL_AUDIO_S16LE, 2, 48000 };
    if ((e != NULL && atoi(e) == 0) || !sHaveAudioFuncs) {
        return;
    }
    if (!sdl.InitSubSystem(SDL_INIT_AUDIO) ||
        (sAudio = sdl.OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL)) == NULL) {
        printf("ttt: window: no sound (%s)\n", sdl.GetError());
        return;
    }
    sdl.ResumeAudioStreamDevice(sAudio);
    Port_SndRingEnable(1);
}

/* Moves the mixed frames to the device; keeps at most AUDIO_MAX_QUEUED bytes waiting there (the game's pace is the
   platform clock's, the device's a little different: the excess is dropped rather than heard later and later). */
static void audio_pump(void) {
    static int16_t buf[2 * 1024];
    int n;
    if (sAudio == NULL) {
        return;
    }
    while ((n = Port_SndRingRead(buf, 1024)) > 0) {
        if (sdl.GetAudioStreamQueued(sAudio) < AUDIO_MAX_QUEUED) {
            sdl.PutAudioStreamData(sAudio, buf, n * 4);
        }
    }
}

/* The window's objects, released on the window thread (GL contexts and renderers belong to it). */
static void release(void) {
    Overlay_Shutdown();
    if (sAudio != NULL) {
        Port_SndRingEnable(0);
        sdl.DestroyAudioStream(sAudio);
        sAudio = NULL;
    }
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

/* The end of the program, from this thread (the window closed and confirmed). */
static void quit_now(void) {
    __atomic_store_n(&sClosed, 1, __ATOMIC_RELEASE);
    printf("ttt: window closed\n");
    fflush(stdout);
    release();
    fflush(stdout);
    _exit(0);
}

/* Asks before quitting unless TTT_NO_QUIT_CONFIRM=1 or the overlay is not there; the second ask quits. */
static void quit_asked(void) {
    static int noConfirm = -1;
    if (noConfirm < 0) {
        const char *e = getenv("TTT_NO_QUIT_CONFIRM");
        noConfirm = e != NULL && atoi(e) != 0;
    }
    if (noConfirm || !Overlay_Ready() || Overlay_QuitAsked()) {
        quit_now();
    }
    Overlay_AskQuit();
}

/* The pending events: the overlay's (F1, a key being bound, its mouse and keys while open), the end of the program,
   the pads' state (presses do not reach the pads while the overlay is open; releases do). */
static void handle_event(const uint8_t *ev) {
    uint32_t type, sc;
    memcpy(&type, ev, 4);
    memcpy(&sc, ev + SDL_SCANCODE_OFFSET, 4);
    if ((type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_KEY_UP) && sc == SETTINGS_SC_FAST) {
        /* hold to fast-forward (T40): never a pad button; pressed while the overlay is open it does nothing, a release
           always counts */
        if (type == SDL_EVENT_KEY_UP || !Overlay_Open()) {
            sFastHeld = type == SDL_EVENT_KEY_DOWN && ev[SDL_KEY_DOWN_OFFSET] != 0;
            apply_speed();
        }
        return;
    }
    if (Overlay_Event(type, ev)) {
        return;
    }
    if (type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && ev[SDL_BUTTON_OFFSET] == SETTINGS_GP_GUIDE) {
        Overlay_Toggle(); /* the guide button: F1 for a gamepad */
        return;
    }
    if (type == SDL_EVENT_KEY_DOWN && sc == SETTINGS_SC_OPEN) {
        if (ev[SDL_KEY_REPEAT_OFFSET] == 0) {
            Overlay_Toggle();
        }
        return;
    }
    if (quit_event(type, ev)) {
        quit_asked();
        return;
    }
    if (Overlay_Open() && (type == SDL_EVENT_KEY_DOWN || type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)) {
        return;
    }
    input_event(type, ev);
}

/* The window's title: "[modified game data]" after it when the game data in use is not the verified USA release
   (plat_data.c; the background check of TEKKEN.BIN may find that after the window is open). */
const char *Port_DataMark(void); /* plat_data.c */
const char *Tr(const char *key, const char *en); /* plat_lang.c */
extern int gPortNet; /* plat_net.c */
static int64_t now_ns(void);
static char sTitle[320], sNetTitle[200];
static const char *window_title(void) {
    const char *m = *Port_DataMark() ? Tr("data.mark", "[modified game data]") : ""; /* T47: translated */
    snprintf(sTitle, sizeof sTitle, "Tekken Tag Tournament%s%s%s%s", *m ? " " : "", m, sNetTitle[0] ? " - " : "",
             sNetTitle);
    return sTitle;
}

static void title_update(void) {
    static int marked;
    static int64_t netAt;
    int changed = 0;
    if (!marked && *Port_DataMark() && sWindow != NULL && sdl.SetWindowTitle != NULL) {
        marked = 1;
        changed = 1;
    }
    if (gPortNet && now_ns() - netAt > 500000000) { /* the netplay session (T46): peer, round trip, rollbacks */
        netAt = now_ns();
        changed |= Port_NetTitle(sNetTitle, sizeof sNetTitle);
    }
    if (changed && sWindow != NULL && sdl.SetWindowTitle != NULL) {
        sdl.SetWindowTitle(sWindow, window_title());
    }
}

static void events(void) {
    static int textOn;
    uint8_t ev[128];
    title_update();
    if (!textOn && (Port_SetupActive() || Overlay_Open()) && sdl.StartTextInput != NULL) {
        /* the setup screen's path entry (T43); the overlay's text fields (the netplay page, T46) */
        textOn = 1;
        sdl.StartTextInput(sWindow);
    }
    while (sdl.PollEvent(ev)) {
        handle_event(ev);
    }
    rumble_update(now_ns());
    if (Overlay_WantsQuit()) {
        quit_now();
    }
}

/* TTT_OVERLAY_TEST=1 (tests, with TTT_FRAMES): made-up key events through the window's own handling, at the overlay's
   frames (nothing reaches the desktop): F1 opens the settings, Return pressed while they are open does not reach the
   pads, Escape asks "Quit?" instead of quitting. overlay.cpp's own steps visit every page and bind a key. */
static int sTestFrame = -1, sTestF1, sTestBlocked, sTestAsked;
static void test_key(int down, uint32_t sc, uint32_t key) {
    uint8_t ev[128] = {0};
    uint32_t type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    memcpy(ev, &type, 4);
    memcpy(ev + SDL_SCANCODE_OFFSET, &sc, 4);
    memcpy(ev + SDL_KEY_OFFSET, &key, 4);
    ev[SDL_KEY_DOWN_OFFSET] = (uint8_t)down;
    handle_event(ev);
}

static void test_step(void) {
    if (sTestFrame < 0) {
        const char *e = getenv("TTT_OVERLAY_TEST");
        if (e == NULL || *e == 0 || *e == '0') {
            return;
        }
    }
    switch (++sTestFrame) {
    case 1:
        test_key(1, SETTINGS_SC_OPEN, 0x4000003Au);
        test_key(0, SETTINGS_SC_OPEN, 0x4000003Au);
        sTestF1 = Overlay_Open() && __atomic_load_n(&sInputBlocked, __ATOMIC_RELAXED);
        break;
    case 20:
        test_key(1, 24, 'u'); /* U: pad 1's square, not while the overlay is open */
        pthread_mutex_lock(&sPadLock);
        sTestBlocked = (sKeyButtons[0] & PB_SQUARE) == 0 && (sLatched[0] & PB_SQUARE) == 0;
        pthread_mutex_unlock(&sPadLock);
        test_key(0, 24, 'u');
        break;
    case 30:
        test_key(1, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
        test_key(0, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
        sTestAsked = Overlay_QuitAsked() && !__atomic_load_n(&sClosed, __ATOMIC_RELAXED);
        break;
    default:
        break;
    }
}

static int64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}

/* ---- the live settings (window thread) ---- */
static int sApplied[4] = {-1, -1, -1, -1}; /* vsync, scale, fullscreen, widescreen margin as last applied */

/* The window's width for a scale: 640 x 448 pictures, widened by the GL back end's widescreen margins (T37,
   video.aspect; the software renderer shows 4:3 only). */
static int win_margin(int gl) {
    char a[16];
    Settings_GetStr("video.aspect", a, sizeof a);
    return gl ? Gl_AspectMargin(a) : 0;
}

void Port_SetSpeed(int pct);                             /* plat_hw.c */
int Port_Speed(void);
void Port_SndSetVolumes(int master, int music, int effects); /* plat_snd.c */
void Port_CheatSet(int unlockAll);                       /* plat_cheat.c */

/* The game speed (T40): game.fast_forward while Tab is held, else game.speed (plat_hw.c scales the real-time clock). */
static void apply_speed(void) {
    Port_SetSpeed(Settings_GetInt(sFastHeld ? "game.fast_forward" : "game.speed"));
}

void Port_WindowApply(void) {
    int vsync = Settings_GetInt("video.vsync"), scale = Settings_GetInt("video.scale");
    int fs = Settings_GetInt("video.fullscreen"), marg = win_margin(sGlMode);
    int dz = Settings_GetInt("input.deadzone"), dpad = Settings_GetInt("input.stick_dpad");
    pthread_mutex_lock(&sPadLock);
    sInputBlocked = Overlay_Open();
    sDeadzone = dz;
    sStickDpad = dpad;
    pthread_mutex_unlock(&sPadLock);
    sVibration = Settings_GetInt("input.vibration");
    __atomic_store_n(&sPaused, Overlay_Open() && Settings_GetInt("game.pause") != 0, __ATOMIC_RELEASE);
    Port_SndSetVolumes(Settings_GetInt("audio.mute") ? 0 : Settings_GetInt("audio.volume"), Settings_GetInt("audio.music"),
                       Settings_GetInt("audio.effects"));
    apply_speed();
    Port_CheatSet(Settings_GetInt("cheats.unlock_all"));
    if (sWindow == NULL) {
        return;
    }
    if (vsync != sApplied[0]) {
        if (sGlContext != NULL) {
            sdl.GL_SetSwapInterval(vsync);
        } else if (sRenderer != NULL) {
            sdl.SetRenderVSync(sRenderer, vsync);
        }
    }
    if (sHaveOverlayFuncs && fs != sApplied[2]) {
        sdl.SetWindowFullscreen(sWindow, fs != 0);
    }
    if (sHaveOverlayFuncs && (scale != sApplied[1] || marg != sApplied[3]) && sApplied[1] >= 0 && !fs) {
        sdl.SetWindowSize(sWindow, (640 + 2 * marg) * scale, 448 * scale);
    }
    sApplied[0] = vsync;
    sApplied[1] = scale;
    sApplied[2] = fs;
    sApplied[3] = marg;
}

void Port_WindowGetBindings(uint16_t b[2][14]) {
    memcpy(b, sBind, sizeof sBind);
}

void Port_WindowSetBindings(const uint16_t b[2][14]) {
    memcpy(sBind, b, sizeof sBind); /* window thread: input_event runs here too */
}

void Port_WindowGetGamepad(uint8_t g[4][16]) {
    pthread_mutex_lock(&sPadLock);
    memcpy(g, sGpBind, sizeof sGpBind);
    pthread_mutex_unlock(&sPadLock);
}

void Port_WindowSetGamepad(const uint8_t g[4][16]) {
    pthread_mutex_lock(&sPadLock); /* the game's thread maps the gamepads at its VBlanks */
    memcpy(sGpBind, g, sizeof sGpBind);
    pthread_mutex_unlock(&sPadLock);
}

const char *Port_WindowGamepadName(int pad) {
    return pad >= 0 && pad < NGAMEPADS && sGamepad[pad] != NULL ? sGamepadName[pad] : NULL;
}

const char *Port_WindowRendererName(void) {
    static char name[96];
    if (sGlMode) {
        return "OpenGL 3.3";
    }
    snprintf(name, sizeof name, "software (SDL renderer %s)", sRenderer != NULL ? sdl.GetRendererName(sRenderer) : "?");
    return name;
}

/* The game's clock waits while this is 1 (plat_hw.c, at a poll in real time): the overlay is open with game.pause. */
int Port_WindowPaused(void) {
    return __atomic_load_n(&sPaused, __ATOMIC_ACQUIRE);
}

/* The overlay over the picture about to be presented (GL: the default framebuffer is bound). */
static int64_t sLastPresent;
static void overlay_draw(void) {
    int ww = 0, wh = 0, pw = 0, ph = 0;
    test_step();
    if (!Overlay_Visible()) {
        return;
    }
    Overlay_TestStep();
    if (sHaveOverlayFuncs) {
        sdl.GetWindowSize(sWindow, &ww, &wh);
    }
    if (!sdl.GetWindowSizeInPixels || !sdl.GetWindowSizeInPixels(sWindow, &pw, &ph)) {
        pw = ww;
        ph = wh;
    }
    Overlay_Render(ww, wh, pw, ph, now_ns());
}

/* The end of a TTT_FRAMES run with a window: the window thread releases the window, then says so (state 3). */
static int quit_requested(void) {
    if (!__atomic_load_n(&sQuitReq, __ATOMIC_ACQUIRE)) {
        return 0;
    }
    title_update();
    release();
    set_state(3);
    return 1;
}

/* An OpenGL 3.3 core window with the GS's GL back end (gs_gl.c) on this thread; 0 (and nothing left open) when any
   step fails. */
static int gl_open(uint64_t flags, int vsync, int scale) {
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
    sWindow = sdl.CreateWindow(window_title(), (640 + 2 * win_margin(1)) * scale, 448 * scale, flags | SDL_WINDOW_OPENGL);
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
            if (!sHaveOverlayFuncs || !Overlay_InitGl(sdl.GL_GetProcAddress)) {
                printf("ttt: window: no settings overlay (F1)\n");
            }
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
    const char *rs = getenv("TTT_RENDERER"), *hs = getenv("TTT_WINDOW_HIDDEN");
    int v, vsync = Settings_GetInt("video.vsync"), scale = Settings_GetInt("video.scale"); /* TTT_VSYNC / TTT_SCALE */
    int64_t lastEvents = 0;
    uint64_t flags = SDL_WINDOW_RESIZABLE | (hs != NULL && atoi(hs) != 0 ? SDL_WINDOW_HIDDEN : 0);

    (void)arg;
    sNoRumble = hs != NULL && atoi(hs) != 0;
    if (!sdl.Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) && !sdl.Init(SDL_INIT_VIDEO)) {
        snprintf(sWhy, sizeof sWhy, "SDL_Init: %s", sdl.GetError());
        set_state(-1);
        return NULL;
    }
    v = sdl.GetVersion();
    Settings_Bindings(sBind);
    Settings_Gamepad(sGpBind);
    audio_open();
    if (rs != NULL && strcmp(rs, "sw") != 0 && strcmp(rs, "gl") != 0) {
        printf("ttt: window: TTT_RENDERER=%s unknown (gl or sw): gl\n", rs);
    }
    if (rs == NULL || strcmp(rs, "sw") != 0) {
        sGlMode = gl_open(flags, vsync, scale);
    }
    if (sGlMode) {
        printf("ttt: window: SDL %d.%d.%d, OpenGL renderer (TTT_RENDERER=gl)\n", v / 1000000, v / 1000 % 1000, v % 1000);
        fflush(stdout);
        sApplied[0] = vsync;
        Port_WindowApply();
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
                audio_pump();
            }
            Gs_GlPump(4);
            if (Overlay_Open() && now_ns() - sLastPresent > 16000000) { /* the game shows nothing new (paused) */
                if (!Gl_Redisplay() && Port_SetupActive()) {
                    Port_WindowGlSwap(); /* the setup screen before the game's first picture (overlay.cpp clears) */
                }
            }
        }
    }
    sWindow = sdl.CreateWindow(window_title(), 640 * scale, 448 * scale, flags);
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
    if (sHaveOverlayFuncs) {
        OverlaySdlRenderer r;
        r.renderer = sRenderer;
        r.CreateTexture = sdl.CreateTexture;
        r.UpdateTexture = sdl.UpdateTexture;
        r.SetTextureBlendMode = sdl.SetTextureBlendMode;
        r.DestroyTexture = sdl.DestroyTexture;
        r.RenderGeometryRaw = sdl.RenderGeometryRaw;
        r.SetRenderClipRect = sdl.SetRenderClipRect;
        if (!Overlay_InitSw(&r)) {
            printf("ttt: window: no settings overlay (F1)\n");
        }
    }
    sApplied[0] = vsync;
    Port_WindowApply();
    set_state(2);
    for (;;) {
        uint32_t w = tw, h = th;
        int fresh = 0;

        events();
        if (quit_requested()) {
            return NULL;
        }
        audio_pump();
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
        if (!fresh && !(tex != NULL && Overlay_Open() && now_ns() - sLastPresent > 16000000)) {
            continue; /* nothing new; with the overlay open the last picture is drawn again (the game may be paused) */
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
        if (fresh) {
            sdl.UpdateTexture(tex, NULL, pic, (int)w * 3);
        }
        sdl.RenderClear(sRenderer);
        sdl.RenderTexture(sRenderer, tex, NULL, NULL);
        if (Overlay_Visible()) { /* the overlay in window pixels, outside the picture's letterbox */
            sdl.SetRenderLogicalPresentation(sRenderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            overlay_draw();
            sdl.SetRenderLogicalPresentation(sRenderer, (int)tw, (int)th, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        }
        sdl.RenderPresent(sRenderer);
        sLastPresent = now_ns();
        Overlay_FrameShown(sLastPresent);
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
    if (e != NULL ? atoi(e) == 0 : (Port_Headless() && !Port_NetWindow()) || getenv("TTT_SELFTEST") != NULL) {
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
   program exits (GL drivers do not like a context still current on another thread at exit). The release takes
   0.03 s at 1x but at 8x 0.6 s after 300 VBlanks, 1.9 s after 760 and more than 2 s after the fight (the driver frees
   the large targets): the wait allows 30 s (it was 2 s, and at 8x the program then exited while SDL_Quit was
   unloading the GL driver on the window thread, see Port_WindowExit). */
void Port_WindowReport(void) {
    if (sState != 2) {
        return;
    }
    if (sGlMode) {
        printf("ttt: window: OpenGL, %u pictures shown\n", __atomic_load_n(&sShown, __ATOMIC_RELAXED));
    } else {
        printf("ttt: window: %u pictures handed over, %u shown\n", sSeq, __atomic_load_n(&sShown, __ATOMIC_RELAXED));
    }
    Overlay_Report();
    if (sTestFrame >= 0) {
        printf("ttt: window: overlay test: F1 opens %d, presses blocked %d, Escape asks %d\n", sTestF1, sTestBlocked,
               sTestAsked);
    }
    fflush(stdout);
    __atomic_store_n(&sQuitReq, 1, __ATOMIC_RELEASE);
    pthread_mutex_lock(&sLock);
    pthread_cond_broadcast(&sCond);
    while (sState == 2) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 30;
        if (pthread_cond_timedwait(&sCond, &sLock, &ts) != 0) {
            break; /* the window thread is stuck in the driver: exit anyway */
        }
    }
    pthread_mutex_unlock(&sLock);
    printf("ttt: window: title \"%s\"\n", sTitle); /* as last set (title_update before the release) */
}

/* The end of a TTT_FRAMES run, after the report (plat_hw.c). With a window, _exit after flushing: exit() runs the
   exit handlers of the libraries SDL loaded, and at 8x (SDL_Quit still running on the window thread) one run in two
   ended in a jump into unmapped memory after a complete report (T38 b0). The port registers no exit handlers. */
void Port_WindowExit(int code) {
    fflush(NULL);
    if (sState >= 2) {
        _exit(code);
    }
    exit(code);
}

/* ---- the setup screen's helpers (T43, overlay.cpp; window thread) ---- */

/* The clipboard's text (a copy, valid until the next call), NULL when there is none or SDL has no clipboard. */
const char *Port_WindowClipboard(void) {
    static char *copy;
    char *t;
    free(copy);
    copy = NULL;
    if (sdl.GetClipboardText == NULL || (t = sdl.GetClipboardText()) == NULL) {
        return NULL;
    }
    copy = malloc(strlen(t) + 1);
    if (copy != NULL) {
        memcpy(copy, t, strlen(t) + 1);
    }
    if (sdl.free != NULL) {
        sdl.free(t);
    }
    return copy;
}

void Port_WindowSetClipboard(const char *text) {
    if (sdl.SetClipboardText != NULL) {
        sdl.SetClipboardText(text);
    }
}

/* The system's file dialog (SDL 3.2's, through the desktop's portal or zenity): asynchronous, its answer may come on
   another thread, kept here until the setup screen takes it. */
static pthread_mutex_t sDialogLock = PTHREAD_MUTEX_INITIALIZER;
static char sDialogPath[1024];
static int sDialogDone;

static void dialog_done(void *user, const char *const *files, int filter) {
    (void)user, (void)filter;
    if (files != NULL && files[0] != NULL) {
        pthread_mutex_lock(&sDialogLock);
        snprintf(sDialogPath, sizeof sDialogPath, "%s", files[0]);
        sDialogDone = 1;
        pthread_mutex_unlock(&sDialogLock);
    }
}

int Port_WindowDialogAvailable(void) { return sdl.ShowOpenFileDialog != NULL && sdl.ShowOpenFolderDialog != NULL; }

void Port_WindowDialog(int folder, const char *start) {
    static const char *const kFilter[2] = {"Disc images (iso, bin, cue)", "iso;bin;cue"};
    if (!Port_WindowDialogAvailable()) {
        return;
    }
    if (folder) {
        sdl.ShowOpenFolderDialog(dialog_done, NULL, sWindow, start != NULL && *start ? start : NULL, 0);
    } else {
        sdl.ShowOpenFileDialog(dialog_done, NULL, sWindow, kFilter, 1, start != NULL && *start ? start : NULL, 0);
    }
}

int Port_WindowDialogResult(char *out, int n) {
    int done;
    pthread_mutex_lock(&sDialogLock);
    done = sDialogDone;
    if (done) {
        snprintf(out, (size_t)n, "%s", sDialogPath);
        sDialogDone = 0;
    }
    pthread_mutex_unlock(&sDialogLock);
    return done;
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
    overlay_draw();
    sdl.GL_SwapWindow(sWindow);
    sLastPresent = now_ns();
    Overlay_FrameShown(sLastPresent);
    __atomic_add_fetch(&sShown, 1, __ATOMIC_RELAXED);
}

/* ---- TTT_SELFTEST=pad (plat_pad.c): the gamepads' path on made-up events. SDL is not loaded: its gamepad functions
   are stand-ins here (made-up handles, a recorder for the rumble), so no real gamepad is opened or rumbled. ---- */
static uint32_t sTestRumbles[NGAMEPADS];
static uint16_t sTestRumble[NGAMEPADS][2];
static void *test_open(uint32_t id) { return (void *)(uintptr_t)(0x1000u + id); }
static void test_close(void *g) { (void)g; }
static const char *test_name(void *g) { (void)g; return "test gamepad"; }
static _Bool test_rumble(void *g, uint16_t low, uint16_t high, uint32_t ms) {
    int i;
    (void)ms;
    for (i = 0; i < NGAMEPADS; i++) {
        if (sGamepad[i] == g) {
            sTestRumbles[i]++;
            sTestRumble[i][0] = low;
            sTestRumble[i][1] = high;
        }
    }
    return 1;
}

/* The stand-ins (only while SDL is not loaded), the default gamepad bindings and the input options. */
void Port_WindowTestInput(int deadzone, int stickDpad, int vibration) {
    if (sdl.OpenGamepad == NULL || sdl.OpenGamepad == test_open) {
        sdl.OpenGamepad = test_open;
        sdl.CloseGamepad = test_close;
        sdl.GetGamepadName = test_name;
        sdl.RumbleGamepad = test_rumble;
    }
    Settings_DefaultGamepad(sGpBind);
    sDeadzone = deadzone;
    sStickDpad = stickDpad;
    sVibration = vibration;
}

/* A made-up gamepad event (SDL's layout) through the window's own handling. */
void Port_WindowTestEvent(uint32_t type, uint32_t which, int code, int value) {
    uint8_t ev[128] = {0};
    int16_t v = (int16_t)value;
    if (sdl.OpenGamepad != test_open) {
        return;
    }
    memcpy(ev, &type, 4);
    memcpy(ev + SDL_WHICH_OFFSET, &which, 4);
    ev[SDL_BUTTON_OFFSET] = (uint8_t)code;
    ev[SDL_BUTTON_DOWN_OFFSET] = (uint8_t)(value != 0);
    memcpy(ev + SDL_AXIS_VALUE_OFFSET, &v, 2);
    handle_event(ev);
}

/* The motors sent now (a fixed clock: no refresh), and what gamepad `pad` has been sent: the calls so far. */
int Port_WindowTestRumble(int pad, uint16_t *low, uint16_t *high) {
    if (sdl.RumbleGamepad != test_rumble) {
        return -1;
    }
    rumble_update(0);
    *low = sTestRumble[pad][0];
    *high = sTestRumble[pad][1];
    return (int)sTestRumbles[pad];
}

/* TTT_SELFTEST=window: the keyboard's default mapping and the window's quit keys, on made-up SDL events (no SDL, no
   window, nothing sent to the desktop): a tap of Return between two VBlanks is pad 1's Start for one look, a held one
   until released, keypad Enter is pad 2's Start, W A S D the d-pad, Escape (by key code or by place, not its repeat) and
   the close button ask to end the program, other keys do not. */
void Port_WindowSelfTest(void) {
    const char *t = getenv("TTT_SELFTEST");
    uint16_t b[4];
    uint8_t a[4][4];
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
    Settings_DefaultBindings(sBind);
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
        ev[SDL_KEY_REPEAT_OFFSET] = 1;
        EXPECT(!quit_event(ty, ev), "Escape's key repeat does not answer the quit question");
        ev[SDL_KEY_REPEAT_OFFSET] = 0;
        sc = 40;
        ky = 0x0D;
        memcpy(ev + SDL_SCANCODE_OFFSET, &sc, 4);
        memcpy(ev + SDL_KEY_OFFSET, &ky, 4);
        EXPECT(!quit_event(ty, ev), "Return does not end the program");
        EXPECT(quit_event(SDL_EVENT_WINDOW_CLOSE_REQUESTED, ev), "the close button ends the program");
        EXPECT(quit_event(SDL_EVENT_QUIT, ev), "SDL's quit ends the program");
    }
    { /* T40: Tab held is game.fast_forward (default 3x), released game.speed; never a pad button */
        uint8_t ev[128] = {0};
        uint32_t ty = SDL_EVENT_KEY_DOWN, sc = SETTINGS_SC_FAST;
        memcpy(ev, &ty, 4);
        memcpy(ev + SDL_SCANCODE_OFFSET, &sc, 4);
        ev[SDL_KEY_DOWN_OFFSET] = 1;
        handle_event(ev);
        Port_WindowPads(b, a);
        EXPECT(Port_Speed() == Settings_GetInt("game.fast_forward") && b[0] == 0 && b[1] == 0, "Tab held: fast-forward");
        ty = SDL_EVENT_KEY_UP;
        memcpy(ev, &ty, 4);
        ev[SDL_KEY_DOWN_OFFSET] = 0;
        handle_event(ev);
        EXPECT(Port_Speed() == Settings_GetInt("game.speed"), "Tab released: the chosen speed");
    }
#undef KEY
#undef EXPECT
    printf(fails ? "ttt: window self-test FAILED (%d)\n" : "ttt: window self-test OK\n", fails);
    fflush(stdout);
    exit(fails != 0);
}
