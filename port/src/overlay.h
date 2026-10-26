/* The settings overlay (T36, port/src/overlay.cpp: Dear ImGui), for the window thread (plat_window.c). Integer and
   pointer arguments only: the C side has no host float. Nothing here runs on the game's thread. */
#ifndef OVERLAY_H
#define OVERLAY_H
#include <stdint.h>

#ifdef __cplusplus
#define OVL_BOOL bool
extern "C" {
#else
#define OVL_BOOL _Bool
#endif

/* The SDL renderer functions the software renderer's overlay draws with (SDL3's signatures; pointers stay opaque). */
typedef struct {
    void *renderer;
    void *(*CreateTexture)(void *renderer, uint32_t format, int access, int w, int h);
    OVL_BOOL (*UpdateTexture)(void *texture, const void *rect, const void *pixels, int pitch);
    OVL_BOOL (*SetTextureBlendMode)(void *texture, uint32_t mode);
    void (*DestroyTexture)(void *texture);
    OVL_BOOL (*RenderGeometryRaw)(void *renderer, void *texture, const float *xy, int xyStride, const void *color,
                               int colorStride, const float *uv, int uvStride, int vertices, const void *indices,
                               int nIndices, int indexSize);
    OVL_BOOL (*SetRenderClipRect)(void *renderer, const void *rect);
} OverlaySdlRenderer;

int Overlay_InitGl(void *(*getproc)(const char *name)); /* with the window's GL context current; 1 when ready */
int Overlay_InitSw(const OverlaySdlRenderer *r);       /* 1 when ready */
void Overlay_Shutdown(void);                            /* before the context / renderer goes */
int Overlay_Ready(void);

/* One SDL event (raw bytes, plat_window.c's offsets). 1: the overlay took it (a key or a gamepad input being bound),
   the window does nothing more with it. While the overlay is open its mouse, keys and gamepad buttons feed ImGui. */
int Overlay_Event(uint32_t type, const uint8_t *ev);
void Overlay_Toggle(void);    /* F1: the settings window */
void Overlay_AskQuit(void);   /* the close button or Escape: the "Quit?" question */
int Overlay_QuitAsked(void);  /* the question is up */
int Overlay_WantsQuit(void);  /* the player said yes */
int Overlay_Open(void);       /* the settings or the question: the game gets no input */
int Overlay_Visible(void);    /* anything to draw (also the frame-time meter) */
/* Draws the overlay over the window's picture (GL: the default framebuffer; sw: through the renderer), the window
   being win_w x win_h (SDL's window units) and pix_w x pix_h pixels. `now_ns`: the monotonic clock. */
void Overlay_Render(int win_w, int win_h, int pix_w, int pix_h, int64_t now_ns);
void Overlay_FrameShown(int64_t now_ns); /* a picture reached the window (the meter) */
/* TTT_OVERLAY_TEST=1 (tests): every page is drawn in turn, a key is bound through the capture path, the quit
   question (asked by the window's test) is answered no; the counts are reported (Overlay_Report). */
void Overlay_TestStep(void);
void Overlay_Report(void);

/* ---- plat_window.c, for the overlay (window thread) ---- */
void Port_WindowApply(void);                                   /* the live settings changed: apply them */
void Port_WindowGetBindings(uint16_t b[2][14]);
void Port_WindowSetBindings(const uint16_t b[2][14]);
void Port_WindowGetGamepad(uint8_t g[4][16]);                  /* the gamepads' bindings (plat_settings.h) */
void Port_WindowSetGamepad(const uint8_t g[4][16]);
const char *Port_WindowGamepadName(int pad);                   /* pad 0-3; NULL: none */
const char *Port_WindowRendererName(void);
const char *Port_DataPath(void);                               /* plat_data.c: the data file */
const char *Port_DiscName(void);                               /* plat_file.c */
int Port_DataState(void);   /* plat_data.c: 0 verified, 1 modified, 2 being checked, 3 not checked */
const char *Port_DataWhy(void); /* plat_data.c: the first difference, or why not checked */
const char *Tp_Folder(void);                                   /* gs_texpack.c: <textures>/SLUS-20001 */
/* the window's clipboard (paths pasted into the setup screen) and the system's file dialog, when SDL has them */
const char *Port_WindowClipboard(void);                        /* a copy, valid until the next call; NULL: none */
void Port_WindowSetClipboard(const char *text);
int Port_WindowDialogAvailable(void);
void Port_WindowDialog(int folder, const char *start);         /* opens it; the choice comes later: */
int Port_WindowDialogResult(char *out, int n);                 /* 1 once with the path chosen */

/* ---- plat_setup.c, for the setup screen (T43; window thread) ---- */
typedef struct {
    int busy;          /* a disc is being checked */
    int permille;      /* its progress */
    char stage[64];    /* what is being done */
    char error[700];   /* why the last disc was refused, or why the game could not start; "" none */
    char path[1024];   /* the path to start from */
    const char *dataPath; /* where the data file goes */
} PortSetupView;
typedef struct {
    char name[256];
    int kind; /* 1 a folder, 2 a disc image (.iso .bin .cue), 3 a folder holding TEKKEN.BIN */
} PortSetupEntry;
int Port_SetupActive(void);                     /* the setup screen is up (the game has not started) */
void Port_SetupView(PortSetupView *v);
void Port_SetupRequest(const char *path);       /* "Check and install" */
int Port_SetupList(const char *dir, PortSetupEntry *out, int max); /* the browser: -1 when unreadable */
const char *Port_HomeDir(void);                 /* plat_paths.c: the browser starts there */
void Port_SetupTestNote(int entries, int errorShown); /* TTT_SETUP_TEST: a frame drawn */

#ifdef __cplusplus
}
#endif
#endif
