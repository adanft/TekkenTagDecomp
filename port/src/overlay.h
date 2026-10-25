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

/* One SDL event (raw bytes, plat_window.c's offsets). 1: the overlay took it (a key being bound), the window does
   nothing more with it. While the overlay is open its mouse and keys feed ImGui. */
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
const char *Port_WindowGamepadName(int pad);                   /* NULL: none */
const char *Port_WindowRendererName(void);
const char *Port_DataPath(void);                               /* plat_data.c: the data file */
const char *Port_DiscName(void);                               /* plat_file.c */
extern int gPortModifiedData;                                  /* plat_data.c */
const char *Tp_Folder(void);                                   /* gs_texpack.c: <textures>/SLUS-20001 */

#ifdef __cplusplus
}
#endif
#endif
