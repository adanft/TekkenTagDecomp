/*
 * The settings overlay (T36, docs/port/settings.md): Dear ImGui (third_party/imgui, MIT) over the window's picture.
 * F1 opens it; the close button or Escape asks "Quit?". Before the game starts it can be the setup screen instead (T43,
 * plat_setup.c, docs/port/install.md): the player's disc, picked by path, by the built-in browser or by the system's
 * file dialog, checked by the main thread while this screen shows the progress.
 *
 * Window thread only (plat_window.c calls every function here): ImGui computes in host floating point, which the rest of
 * the port never does, so nothing of it reaches the game. What the player chooses goes to the settings file
 * (plat_settings.c, integers and text) and to the window (Port_WindowApply: vsync, window size, fullscreen, volume,
 * keyboard bindings); what the game's thread sees of it is the window's pads, neutral while the overlay is open, and
 * whether it is paused (an integer read at the VBlank boundary, plat_hw.c). The build lets host float instructions
 * into this file's and ImGui's functions only (tests/test_port_float.py).
 *
 * Platform glue written here (SDL3 is loaded at run time, so ImGui's SDL3 backend, which links SDL, is not used): the
 * raw SDL events by offset, as plat_window.c reads them (keys, mouse, and gamepads for ImGui's gamepad navigation: the
 * d-pad or left stick moves, south activates, east goes back). Renderers: ImGui's OpenGL 3 backend (functions from
 * SDL_GL_GetProcAddress), or a small SDL_Renderer path for TTT_RENDERER=sw (SDL_RenderGeometryRaw).
 */
#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imgui.h"
#include "imgui_internal.h" /* the focused item for the screen reader (ImGui's test engine hooks) */
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_opengl3_loader.h"
#include "overlay.h"
#include "plat_settings.h"
#include "plat_lang.h"
#include "plat_net.h"
#include "plat_speech.h"
#include "plat_mods.h"

#ifndef TTT_GIT
#define TTT_GIT "unknown"
#endif

/* imgui_impl_opengl3.cpp is built with -Datexit=ttt_imgl3w_atexit (port/tools/toolchain.py): its libGL loader, which
   the port does not use (imgl3wInit2 below), would register a handler through the C library's atexit, a new PLT
   entry. */
extern "C" int ttt_imgl3w_atexit(void (*fn)(void)) {
    (void)fn;
    return 0;
}

extern "C" void Overlay_AssertFailed(const char *what, const char *file, int line) {
    static int reported;
    if (!reported) {
        reported = 1;
        fprintf(stderr, "ttt: overlay: ImGui check failed: %s (%s:%d)\n", what, file, line);
    }
}

/* SDL3 values (plat_window.c's style: no SDL headers) */
enum : uint32_t {
    EV_KEY_DOWN = 0x300, EV_KEY_UP = 0x301, EV_MOUSE_MOTION = 0x400, EV_MOUSE_DOWN = 0x401, EV_MOUSE_UP = 0x402,
    EV_MOUSE_WHEEL = 0x403, EV_MOUSE_LEAVE = 0x20D, EV_FOCUS_LOST = 0x20F, EV_PAD_AXIS = 0x650, EV_PAD_DOWN = 0x651,
    EV_PAD_UP = 0x652, EV_TEXT_INPUT = 0x303,
    PIXELFORMAT_RGBA32 = 0x16762004u, TEXTUREACCESS_STATIC = 0, BLENDMODE_BLEND = 1,
};
static const int kScancodeAt = 24, kKeyModAt = 32, kKeyDownAt = 36, kKeyRepeatAt = 37; /* SDL_KeyboardEvent */
static const int kMouseXAt = 28, kMouseYAt = 32, kButtonAt = 24, kButtonDownAt = 25;  /* SDL_Mouse*Event */
static const int kWheelXAt = 24, kWheelYAt = 28, kWheelDirAt = 32;
static const int kPadCodeAt = 20, kPadDownAt = 21, kPadAxisValueAt = 24; /* SDL_Gamepad{Button,Axis}Event */
static const int kTextAt = 20; /* SDL_TextInputEvent.text (a pointer: after windowID on i386) */

static int sMode; /* 0 none, 1 GL, 2 SDL renderer */
static OverlaySdlRenderer sSw;
static bool sSettings, sQuitAsk, sQuitYes;
static int sCapturePad = -1, sCaptureButton = -1;
static bool sCaptureGamepad; /* the binding being captured is a gamepad's (pad.*), not a key's */
static int sGamepadPage;     /* the Controls page's gamepad: pad 1-4 */
static int64_t sLastNs;
static int64_t sShown[64]; /* the meter: when the last pictures reached the window */
static unsigned sShownN;
static int sTest = -1, sTestFrames, sTestPages, sTestCapture, sTestGpCapture; /* TTT_OVERLAY_TEST */
static int sTestTab = -1;
static unsigned sDrawn;
static int sStyleScale = -1, sStyleContrast = -1; /* the ui.scale and ui.high_contrast the style has (T48) */
static unsigned sMarkDrawn; /* frames the settings window showed the [modified game data] mark (TTT_OVERLAY_TEST) */
static void (*sBindFramebuffer)(GLenum target, GLuint fb); /* the setup screen clears the window (GL) */

static float rd_f32(const uint8_t *p) {
    float f;
    memcpy(&f, p, 4);
    return f;
}

static uint32_t rd_u32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static void init_common(void) {
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    io.BackendPlatformName = "ttt_sdl3_raw";
    ImGuiPlatformIO &pio = ImGui::GetPlatformIO(); /* the clipboard: a path pasted into the setup screen */
    pio.Platform_GetClipboardTextFn = [](ImGuiContext *) -> const char * { return Port_WindowClipboard(); };
    pio.Platform_SetClipboardTextFn = [](ImGuiContext *, const char *t) { Port_WindowSetClipboard(t); };
    sStyleScale = sStyleContrast = -1; /* the style: apply_style, before each frame */
    const char *e = getenv("TTT_OVERLAY_TEST");
    sTest = e != nullptr && *e != 0 && *e != '0' ? 0 : -1;
}

extern "C" int Overlay_InitGl(void *(*getproc)(const char *name)) {
    if (sMode != 0) {
        return 1;
    }
    imgl3wInit2((GL3WGetProcAddressProc)getproc);
    sBindFramebuffer = (void (*)(GLenum, GLuint))getproc("glBindFramebuffer");
    if (glGetIntegerv == nullptr) {
        fprintf(stderr, "ttt: overlay: no OpenGL functions\n");
        return 0;
    }
    init_common();
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        ImGui::DestroyContext();
        fprintf(stderr, "ttt: overlay: ImGui's OpenGL 3 renderer did not start\n");
        return 0;
    }
    sMode = 1;
    return 1;
}

extern "C" int Overlay_InitSw(const OverlaySdlRenderer *r) {
    if (sMode != 0) {
        return 1;
    }
    if (r == nullptr || r->renderer == nullptr || r->CreateTexture == nullptr || r->UpdateTexture == nullptr ||
        r->SetTextureBlendMode == nullptr || r->DestroyTexture == nullptr || r->RenderGeometryRaw == nullptr ||
        r->SetRenderClipRect == nullptr) {
        return 0;
    }
    sSw = *r;
    init_common();
    ImGuiIO &io = ImGui::GetIO();
    io.BackendRendererName = "ttt_sdlrenderer3_raw";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
    sMode = 2;
    return 1;
}

extern "C" int Overlay_Ready(void) { return sMode != 0; }

/* ---- the SDL renderer path ---- */
static void sw_texture(ImTextureData *tex) {
    if (tex->Status == ImTextureStatus_WantCreate) {
        void *t = sSw.CreateTexture(sSw.renderer, PIXELFORMAT_RGBA32, TEXTUREACCESS_STATIC, tex->Width, tex->Height);
        if (t == nullptr) {
            tex->SetStatus(ImTextureStatus_Destroyed);
            return;
        }
        sSw.UpdateTexture(t, nullptr, tex->GetPixels(), tex->GetPitch());
        sSw.SetTextureBlendMode(t, BLENDMODE_BLEND);
        tex->SetTexID((ImTextureID)(intptr_t)t);
        tex->SetStatus(ImTextureStatus_OK);
    } else if (tex->Status == ImTextureStatus_WantUpdates) {
        void *t = (void *)(intptr_t)tex->TexID;
        for (ImTextureRect &r : tex->Updates) {
            int rect[4] = {r.x, r.y, r.w, r.h};
            sSw.UpdateTexture(t, rect, tex->GetPixelsAt(r.x, r.y), tex->GetPitch());
        }
        tex->SetStatus(ImTextureStatus_OK);
    } else if (tex->Status == ImTextureStatus_WantDestroy) {
        if (tex->TexID != ImTextureID_Invalid) {
            sSw.DestroyTexture((void *)(intptr_t)tex->TexID);
        }
        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

/* Vertex positions in pixels and SDL_FColor colours (plain buffers: a static object with a destructor would register
   it through __cxa_atexit). */
static float *sXy, *sCol;
static int sCap;

static void sw_render(ImDrawData *dd) {
    const ImVec2 sc = dd->FramebufferScale, off = dd->DisplayPos;
    const float fw = dd->DisplaySize.x * sc.x, fh = dd->DisplaySize.y * sc.y;
    if (fw <= 0.0f || fh <= 0.0f) {
        return;
    }
    if (dd->Textures != nullptr) {
        for (ImTextureData *t : *dd->Textures) {
            if (t->Status != ImTextureStatus_OK) {
                sw_texture(t);
            }
        }
    }
    for (const ImDrawList *dl : dd->CmdLists) {
        const ImDrawVert *v = dl->VtxBuffer.Data;
        const int nv = dl->VtxBuffer.Size;
        if (nv > sCap) {
            ImGui::MemFree(sXy);
            ImGui::MemFree(sCol);
            sCap = nv + nv / 2;
            sXy = (float *)ImGui::MemAlloc(sizeof(float) * 2 * (size_t)sCap);
            sCol = (float *)ImGui::MemAlloc(sizeof(float) * 4 * (size_t)sCap);
        }
        float *xy = sXy, *col = sCol;
        for (int i = 0; i < nv; i++) { /* positions in pixels, colours as SDL_FColor */
            xy[i * 2] = (v[i].pos.x - off.x) * sc.x;
            xy[i * 2 + 1] = (v[i].pos.y - off.y) * sc.y;
            ImU32 c = v[i].col;
            col[i * 4] = (float)(c >> IM_COL32_R_SHIFT & 0xFF) * (1.0f / 255.0f);
            col[i * 4 + 1] = (float)(c >> IM_COL32_G_SHIFT & 0xFF) * (1.0f / 255.0f);
            col[i * 4 + 2] = (float)(c >> IM_COL32_B_SHIFT & 0xFF) * (1.0f / 255.0f);
            col[i * 4 + 3] = (float)(c >> IM_COL32_A_SHIFT & 0xFF) * (1.0f / 255.0f);
        }
        for (int k = 0; k < dl->CmdBuffer.Size; k++) {
            const ImDrawCmd *cmd = &dl->CmdBuffer[k];
            if (cmd->UserCallback != nullptr) {
                if (cmd->UserCallback != ImGui::GetPlatformIO().DrawCallback_ResetRenderState) {
                    cmd->UserCallback(dl, cmd);
                }
                continue;
            }
            float x0 = (cmd->ClipRect.x - off.x) * sc.x, y0 = (cmd->ClipRect.y - off.y) * sc.y;
            float x1 = (cmd->ClipRect.z - off.x) * sc.x, y1 = (cmd->ClipRect.w - off.y) * sc.y;
            x0 = x0 < 0.0f ? 0.0f : x0;
            y0 = y0 < 0.0f ? 0.0f : y0;
            x1 = x1 > fw ? fw : x1;
            y1 = y1 > fh ? fh : y1;
            if (x1 <= x0 || y1 <= y0) {
                continue;
            }
            int rect[4] = {(int)x0, (int)y0, (int)(x1 - x0), (int)(y1 - y0)};
            sSw.SetRenderClipRect(sSw.renderer, rect);
            sSw.RenderGeometryRaw(sSw.renderer, (void *)(intptr_t)cmd->GetTexID(), xy + cmd->VtxOffset * 2,
                                  (int)(2 * sizeof(float)), col + cmd->VtxOffset * 4, (int)(4 * sizeof(float)),
                                  &v[cmd->VtxOffset].uv.x, (int)sizeof(ImDrawVert), nv - (int)cmd->VtxOffset,
                                  dl->IdxBuffer.Data + cmd->IdxOffset, (int)cmd->ElemCount, (int)sizeof(ImDrawIdx));
        }
    }
    sSw.SetRenderClipRect(sSw.renderer, nullptr);
}

extern "C" void Overlay_Shutdown(void) {
    if (sMode == 0) {
        return;
    }
    if (sMode == 1) {
        ImGui_ImplOpenGL3_Shutdown();
    } else {
        for (ImTextureData *t : ImGui::GetPlatformIO().Textures) {
            if (t->RefCount == 1 && t->TexID != ImTextureID_Invalid) {
                t->Status = ImTextureStatus_WantDestroy;
                sw_texture(t);
            }
        }
    }
    ImGui::MemFree(sXy);
    ImGui::MemFree(sCol);
    sXy = sCol = nullptr;
    sCap = 0;
    ImGui::DestroyContext();
    sMode = 0;
}

/* ---- input ---- */
static ImGuiKey imgui_key(uint32_t sc) {
    if (sc >= 4 && sc <= 29) {
        return (ImGuiKey)(ImGuiKey_A + (int)(sc - 4));
    }
    if (sc >= 30 && sc <= 38) {
        return (ImGuiKey)(ImGuiKey_1 + (int)(sc - 30));
    }
    switch (sc) {
    case 39: return ImGuiKey_0;
    case 40: return ImGuiKey_Enter;
    case 42: return ImGuiKey_Backspace;
    case 43: return ImGuiKey_Tab;
    case 44: return ImGuiKey_Space;
    case 74: return ImGuiKey_Home;
    case 75: return ImGuiKey_PageUp;
    case 76: return ImGuiKey_Delete;
    case 77: return ImGuiKey_End;
    case 78: return ImGuiKey_PageDown;
    case 79: return ImGuiKey_RightArrow;
    case 80: return ImGuiKey_LeftArrow;
    case 81: return ImGuiKey_DownArrow;
    case 82: return ImGuiKey_UpArrow;
    case 88: return ImGuiKey_KeypadEnter;
    case 224: return ImGuiKey_LeftCtrl;
    case 225: return ImGuiKey_LeftShift;
    case 226: return ImGuiKey_LeftAlt;
    case 228: return ImGuiKey_RightCtrl;
    case 229: return ImGuiKey_RightShift;
    case 230: return ImGuiKey_RightAlt;
    default: return ImGuiKey_None;
    }
}

static void bind_captured(uint32_t sc) {
    uint16_t b[2][SB_COUNT];
    Port_WindowGetBindings(b);
    if (Settings_Rebind(b, sCapturePad, sCaptureButton, sc)) {
        Port_WindowSetBindings(b);
        Settings_SaveBindings(b);
        sTestCapture += sTest >= 0;
    }
    sCapturePad = sCaptureButton = -1;
}

static void bind_captured_gamepad(unsigned input) {
    uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT];
    Port_WindowGetGamepad(g);
    if (Settings_RebindGamepad(g, sCapturePad, sCaptureButton, input)) {
        Port_WindowSetGamepad(g);
        Settings_SaveGamepad(g);
        sTestGpCapture += sTest >= 0;
    }
    sCapturePad = sCaptureButton = -1;
}

static int16_t rd_s16(const uint8_t *p) {
    int16_t v;
    memcpy(&v, p, 2);
    return v;
}

/* SDL gamepad buttons (SDL_GamepadButton order) as ImGui's gamepad keys */
static const ImGuiKey kPadKeys[15] = {
    ImGuiKey_GamepadFaceDown, ImGuiKey_GamepadFaceRight, ImGuiKey_GamepadFaceLeft, ImGuiKey_GamepadFaceUp,
    ImGuiKey_GamepadBack, ImGuiKey_None, ImGuiKey_GamepadStart, ImGuiKey_GamepadL3, ImGuiKey_GamepadR3,
    ImGuiKey_GamepadL1, ImGuiKey_GamepadR1, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown,
    ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight,
};

static void stick_keys(ImGuiKey neg, ImGuiKey pos, int16_t raw) {
    ImGuiIO &io = ImGui::GetIO();
    float v = (float)raw / 32767.0f;
    io.AddKeyAnalogEvent(neg, v < -0.3f, v < 0.0f ? -v : 0.0f);
    io.AddKeyAnalogEvent(pos, v > 0.3f, v > 0.0f ? v : 0.0f);
}

/* A gamepad event while a binding is captured: 1 when taken (a button or a trigger past half way binds, guide
   cancels). Releases and the sticks go on: ImGui sees the release of the button that started the capture. */
static int capture_gamepad_event(uint32_t type, const uint8_t *ev) {
    if (type == EV_PAD_DOWN) {
        unsigned b = ev[kPadCodeAt];
        if (b == SETTINGS_GP_GUIDE || !sCaptureGamepad) {
            sCapturePad = sCaptureButton = -1; /* cancelled (guide; or a key was asked for) */
        } else if (b < SETTINGS_GP_LTRIGGER) {
            bind_captured_gamepad(b);
        }
        return 1;
    }
    if (type == EV_PAD_AXIS && (ev[kPadCodeAt] == 4 || ev[kPadCodeAt] == 5)) {
        if (sCaptureGamepad && rd_s16(ev + kPadAxisValueAt) >= 16384) {
            bind_captured_gamepad(ev[kPadCodeAt] == 4 ? SETTINGS_GP_LTRIGGER : SETTINGS_GP_RTRIGGER);
        }
        return 1;
    }
    return 0;
}

extern "C" int Overlay_Event(uint32_t type, const uint8_t *ev) {
    if (sMode == 0) {
        return 0;
    }
    if ((type == EV_KEY_DOWN || type == EV_KEY_UP) && sCapturePad >= 0) {
        uint32_t sc = rd_u32(ev + kScancodeAt);
        if (type == EV_KEY_DOWN && ev[kKeyRepeatAt] == 0) {
            if (sc == SETTINGS_SC_ESCAPE || sc == SETTINGS_SC_OPEN || sCaptureGamepad) {
                sCapturePad = sCaptureButton = -1; /* cancelled (Escape, F1; any key for a gamepad's binding) */
            } else {
                bind_captured(sc);
            }
        }
        return 1;
    }
    if (sCapturePad >= 0 && capture_gamepad_event(type, ev)) {
        return 1;
    }
    if (!Overlay_Open()) {
        return 0;
    }
    ImGuiIO &io = ImGui::GetIO();
    switch (type) {
    case EV_MOUSE_MOTION:
        io.AddMousePosEvent(rd_f32(ev + kMouseXAt), rd_f32(ev + kMouseYAt));
        break;
    case EV_MOUSE_DOWN:
    case EV_MOUSE_UP: {
        int b = ev[kButtonAt] == 1 ? 0 : ev[kButtonAt] == 3 ? 1 : ev[kButtonAt] == 2 ? 2 : -1;
        io.AddMousePosEvent(rd_f32(ev + kMouseXAt), rd_f32(ev + kMouseYAt));
        if (b >= 0) {
            io.AddMouseButtonEvent(b, ev[kButtonDownAt] != 0);
        }
        break;
    }
    case EV_MOUSE_WHEEL: {
        float s = rd_u32(ev + kWheelDirAt) == 1 ? -1.0f : 1.0f; /* SDL_MOUSEWHEEL_FLIPPED */
        io.AddMouseWheelEvent(-rd_f32(ev + kWheelXAt) * s, rd_f32(ev + kWheelYAt) * s);
        break;
    }
    case EV_MOUSE_LEAVE:
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    case EV_FOCUS_LOST:
        io.AddFocusEvent(false);
        break;
    case EV_TEXT_INPUT: {
        const char *t;
        memcpy(&t, ev + kTextAt, sizeof t);
        if (t != nullptr) {
            io.AddInputCharactersUTF8(t);
        }
        break;
    }
    case EV_PAD_DOWN:
    case EV_PAD_UP: {
        unsigned b = ev[kPadCodeAt];
        if (b < 15 && kPadKeys[b] != ImGuiKey_None) {
            io.AddKeyEvent(kPadKeys[b], type == EV_PAD_DOWN && ev[kPadDownAt] != 0);
        }
        break;
    }
    case EV_PAD_AXIS: {
        int16_t v = rd_s16(ev + kPadAxisValueAt);
        switch (ev[kPadCodeAt]) {
        case 0: stick_keys(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, v); break;
        case 1: stick_keys(ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown, v); break;
        case 2: stick_keys(ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, v); break;
        case 3: stick_keys(ImGuiKey_GamepadRStickUp, ImGuiKey_GamepadRStickDown, v); break;
        case 4: io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, v >= 16384, v > 0 ? (float)v / 32767.0f : 0.0f); break;
        case 5: io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, v >= 16384, v > 0 ? (float)v / 32767.0f : 0.0f); break;
        default: break;
        }
        break;
    }
    case EV_KEY_DOWN:
    case EV_KEY_UP: {
        uint32_t sc = rd_u32(ev + kScancodeAt);
        uint16_t mod;
        memcpy(&mod, ev + kKeyModAt, 2);
        io.AddKeyEvent(ImGuiMod_Ctrl, (mod & 0x00C0) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (mod & 0x0003) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (mod & 0x0300) != 0);
        ImGuiKey k = imgui_key(sc);
        if (k != ImGuiKey_None) {
            io.AddKeyEvent(k, type == EV_KEY_DOWN && ev[kKeyDownAt] != 0);
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

extern "C" void Overlay_Toggle(void) {
    if (sMode == 0 || sQuitAsk || Port_SetupActive()) {
        return;
    }
    sSettings = !sSettings;
    sCapturePad = sCaptureButton = -1;
    ImGui::GetIO().ClearInputKeys(); /* keys held when it last closed */
    Port_WindowApply(); /* the pause follows the overlay */
}

extern "C" void Overlay_AskQuit(void) {
    if (sMode != 0) {
        if (!sSettings) {
            ImGui::GetIO().ClearInputKeys();
        }
        sQuitAsk = true;
        sCapturePad = sCaptureButton = -1;
        Port_WindowApply();
    }
}

extern "C" int Overlay_QuitAsked(void) { return sQuitAsk; }
extern "C" int Overlay_WantsQuit(void) { return sQuitYes; }
extern "C" int Overlay_Open(void) { return sMode != 0 && (sSettings || sQuitAsk || Port_SetupActive()); }

extern "C" int Overlay_Visible(void) {
    return sMode != 0 && (sSettings || sQuitAsk || sTest >= 0 || Port_SetupActive() ||
                          Settings_GetInt("video.fps_meter") != 0 || Port_NetChatRecent());
}

extern "C" void Overlay_FrameShown(int64_t now_ns) {
    sShown[sShownN++ % 64] = now_ns;
}

/* ---- the look (T48): the overlay's size (ui.scale) and the high-contrast colours (ui.high_contrast) ---- */
static float sUi = 1.0f; /* ui.scale / 100 */

static float S(float v) { return v * sUi; } /* a size in the overlay's units */

static void high_contrast(ImGuiStyle &st) {
    ImVec4 *c = st.Colors;
    const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f), white(1.0f, 1.0f, 1.0f, 1.0f), yellow(1.0f, 0.9f, 0.0f, 1.0f),
        dark(0.12f, 0.12f, 0.12f, 1.0f), grey(0.8f, 0.8f, 0.8f, 1.0f);
    ImGui::StyleColorsDark(&st);
    c[ImGuiCol_Text] = white;
    c[ImGuiCol_TextDisabled] = grey;
    c[ImGuiCol_WindowBg] = black;
    c[ImGuiCol_ChildBg] = black;
    c[ImGuiCol_PopupBg] = black;
    c[ImGuiCol_Border] = white;
    c[ImGuiCol_FrameBg] = dark;
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.3f, 0.3f, 0.0f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.45f, 0.4f, 0.0f, 1.0f);
    c[ImGuiCol_TitleBg] = black;
    c[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.45f, 1.0f);
    c[ImGuiCol_CheckMark] = yellow;
    c[ImGuiCol_SliderGrab] = yellow;
    c[ImGuiCol_SliderGrabActive] = white;
    c[ImGuiCol_Button] = dark;
    c[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.0f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.45f, 0.4f, 0.0f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.0f, 0.0f, 0.45f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.3f, 0.0f, 1.0f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.4f, 0.0f, 1.0f);
    c[ImGuiCol_Separator] = white;
    c[ImGuiCol_Tab] = dark;
    c[ImGuiCol_TabHovered] = ImVec4(0.3f, 0.3f, 0.0f, 1.0f);
    c[ImGuiCol_TabSelected] = ImVec4(0.0f, 0.0f, 0.45f, 1.0f);
    c[ImGuiCol_TabSelectedOverline] = yellow;
    c[ImGuiCol_TableBorderStrong] = white;
    c[ImGuiCol_TableBorderLight] = grey;
    c[ImGuiCol_TableRowBg] = black;
    c[ImGuiCol_TableRowBgAlt] = dark;
    c[ImGuiCol_NavCursor] = yellow;
    st.FrameBorderSize = 1.0f;
    st.WindowBorderSize = 2.0f;
    st.TabBorderSize = 1.0f;
}

/* The style again when ui.scale or ui.high_contrast changed (before a frame). */
static void apply_style(void) {
    int sc = Settings_GetInt("ui.scale"), hc = Settings_GetInt("ui.high_contrast");
    if (sc == sStyleScale && hc == sStyleContrast) {
        return;
    }
    sStyleScale = sc;
    sStyleContrast = hc;
    sUi = (float)sc / 100.0f;
    ImGuiStyle &st = ImGui::GetStyle();
    st = ImGuiStyle();
    if (hc) {
        high_contrast(st);
    } else {
        ImGui::StyleColorsDark(&st);
    }
    st.FontSizeBase = 18.0f * sUi;
    st.WindowRounding = 6.0f;
    st.FrameRounding = 4.0f;
    st.ScaleAllSizes(1.25f * sUi);
}

/* A label with a fixed ImGui id: the text may change with the language, the item stays the same one. */
static const char *L(const char *text, const char *id) {
    static char buf[8][320];
    static int k;
    k = (k + 1) & 7;
    snprintf(buf[k], sizeof buf[k], "%s###%s", text, id);
    return buf[k];
}

/* ---- the screen reader (T48): the item with the keyboard / gamepad focus, said when it changes ---- */
static bool sSr;                   /* ui.screen_reader */
static ImGuiID sSrId, sSrSaidId;   /* the focused item this frame, the one said last */
static char sSrLabel[320], sSrSaid[400], sSrValue[96], sSrHint[160], sSrWindow[160];
static ImGuiItemStatusFlags sSrFlags;
static unsigned sSrCount;          /* texts said (TTT_OVERLAY_TEST) */

static void sr_say(const char *text, int interrupt) {
    if (sSr && text != nullptr && *text) {
        Speech_Say(text, interrupt);
        sSrCount++;
    }
}

/* A label as said: up to its "##" id part. */
static void sr_text(char *out, size_t n, const char *label) {
    const char *h = strstr(label, "##");
    size_t k = h != nullptr ? (size_t)(h - label) : strlen(label);
    k = k < n - 1 ? k : n - 1;
    memcpy(out, label, k);
    out[k] = 0;
}

/* What an item says instead of its label (a button in a table: "Pad 1, up: W"); for the next item only. */
static void sr_hint(const char *text) { snprintf(sSrHint, sizeof sSrHint, "%s", text); }

/* The value an item shows (a slider's), when it has the focus. */
static void sr_value(const char *text) {
    if (sSr && ImGui::GetItemID() == GImGui->NavId) {
        snprintf(sSrValue, sizeof sSrValue, "%s", text);
    }
}

void ImGuiTestEngineHook_ItemAdd(ImGuiContext *, ImGuiID, const ImRect &, const ImGuiLastItemData *) {}
void ImGuiTestEngineHook_Log(ImGuiContext *, const char *, ...) {}
const char *ImGuiTestEngine_FindItemDebugLabel(ImGuiContext *, ImGuiID) { return nullptr; }
void ImGuiTestEngineHook_ItemInfo(ImGuiContext *ctx, ImGuiID id, const char *label, ImGuiItemStatusFlags flags) {
    if (id != 0 && id == ctx->NavId && label != nullptr && (ctx->CurrentWindow == nullptr || id != ctx->CurrentWindow->ID)) {
        sSrId = id;
        sSrFlags = flags;
        if (sSrHint[0]) {
            snprintf(sSrLabel, sizeof sSrLabel, "%s", sSrHint);
        } else {
            sr_text(sSrLabel, sizeof sSrLabel, label);
        }
    }
    sSrHint[0] = 0;
}

static void sr_frame_begin(void) {
    sSr = Settings_GetInt("ui.screen_reader") != 0;
    if (sSr != (Speech_Enabled() != 0)) {
        Speech_SetEnabled(sSr);
    }
    A11y_SetGame(sSr && Settings_GetInt("ui.announce_game"));
    GImGui->TestEngineHookItems = sSr;
    sSrId = 0;
    sSrValue[0] = sSrHint[0] = 0;
}

/* After the frame: the window that has the focus, then the item, when either changed (or the item's state). */
static void sr_frame_end(void) {
    char w[160], t[400];
    int interrupt = 1;
    if (!sSr) {
        sSrSaidId = 0;
        sSrWindow[0] = sSrSaid[0] = 0;
        return;
    }
    ImGuiWindow *nw = GImGui->NavWindow;
    sr_text(w, sizeof w, nw != nullptr ? nw->Name : "");
    if (strcmp(w, sSrWindow) != 0) {
        snprintf(sSrWindow, sizeof sSrWindow, "%s", w);
        sr_say(w, 1);
        interrupt = 0;
    }
    if (sSrId == 0) {
        return;
    }
    const char *state = "";
    if (sSrFlags & ImGuiItemStatusFlags_Checkable) {
        state = sSrFlags & ImGuiItemStatusFlags_Checked ? Tr("sr.checked", "checked") : Tr("sr.not_checked", "not checked");
    }
    snprintf(t, sizeof t, "%s%s%s%s%s", sSrLabel, *state ? ", " : "", state, sSrValue[0] ? ", " : "", sSrValue);
    if (sSrId != sSrSaidId || strcmp(t, sSrSaid) != 0) {
        sr_say(t, interrupt);
        sSrSaidId = sSrId;
        snprintf(sSrSaid, sizeof sSrSaid, "%s", t);
    }
}

/* ---- the windows ---- */
static void setting_note(const char *key) {
    const char *env = Settings_Env(key);
    if (Settings_FromEnv(key) && env != nullptr) {
        ImGui::SameLine();
        ImGui::TextDisabled(TrFmt("overlay.set_by_env", "(set by %s)"), env);
    } else if (Settings_NeedsRestart(key)) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", Tr("overlay.next_start", "(on the next start)"));
    }
}

static void check(const char *label, const char *key) {
    bool v = Settings_GetInt(key) != 0;
    ImGui::BeginDisabled(Settings_FromEnv(key) != 0);
    if (ImGui::Checkbox(L(label, key), &v)) {
        Settings_SetInt(key, v ? 1 : 0);
        Port_WindowApply();
    }
    ImGui::EndDisabled();
    setting_note(key);
}

static void slider(const char *label, const char *key, int lo, int hi, const char *fmt) {
    int v = Settings_GetInt(key);
    char val[96];
    ImGui::BeginDisabled(Settings_FromEnv(key) != 0);
    ImGui::SetNextItemWidth(S(220.0f));
    if (ImGui::SliderInt(L(label, key), &v, lo, hi, fmt)) {
        Settings_SetInt(key, v);
        Port_WindowApply();
    }
    snprintf(val, sizeof val, fmt, v);
    sr_value(val);
    ImGui::EndDisabled();
    setting_note(key);
}

static bool tab(const char *name, const char *id, int index) {
    ImGuiTabItemFlags f = sTestTab == index ? ImGuiTabItemFlags_SetSelected : 0;
    bool open = ImGui::BeginTabItem(L(name, id), nullptr, f);
    if (open && sTestTab == index) {
        sTestPages |= 1 << index;
    }
    return open;
}

static const char *data_mark(void) { return Tr("data.mark", "[modified game data]"); }

static void page_video(void) {
    char r[16];
    Settings_GetStr("video.renderer", r, sizeof r);
    int gl = strcmp(r, "sw") != 0;
    ImGui::Text(TrFmt("overlay.video.renderer_now", "Renderer now: %s"), Port_WindowRendererName());
    ImGui::BeginDisabled(Settings_FromEnv("video.renderer") != 0);
    if (ImGui::RadioButton(L("OpenGL", "r_gl"), gl)) {
        Settings_SetStr("video.renderer", "gl");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(L(Tr("overlay.video.software", "Software"), "r_sw"), !gl)) {
        Settings_SetStr("video.renderer", "sw");
    }
    ImGui::EndDisabled();
    setting_note("video.renderer");
    check(Tr("overlay.video.vsync", "V-sync"), "video.vsync");
    slider(Tr("overlay.video.scale", "Window scale"), "video.scale", 1, 6, TrFmt("overlay.video.scale_fmt", "%dx (640x448 each)"));
    check(Tr("overlay.video.fullscreen", "Fullscreen"), "video.fullscreen");
    check(Tr("overlay.video.fps_meter", "Frame-time meter"), "video.fps_meter");
    /* T37: the GL back end's options (gs_gl.c reads them at each picture; docs/port/video.md) */
    ImGui::SeparatorText("OpenGL");
    ImGui::BeginDisabled(!gl);
    slider(Tr("overlay.video.resolution", "Internal resolution"), "video.resolution", 1, 8, "%dx");
    {
        static const char *const kAspect[3] = {"4:3", "16:9", "21:9"};
        char a[16];
        Settings_GetStr("video.aspect", a, sizeof a);
        ImGui::BeginDisabled(Settings_FromEnv("video.aspect") != 0);
        ImGui::TextUnformatted(Tr("overlay.video.aspect", "Aspect"));
        for (int i = 0; i < 3; i++) {
            ImGui::SameLine();
            if (ImGui::RadioButton(kAspect[i], strcmp(a, kAspect[i]) == 0 || (i == 0 && strcmp(a, "16:9") != 0 && strcmp(a, "21:9") != 0))) {
                Settings_SetStr("video.aspect", kAspect[i]);
                Port_WindowApply();
            }
        }
        ImGui::EndDisabled();
        setting_note("video.aspect");
    }
    check(Tr("overlay.video.deflicker", "Game's flicker filter (half height, blurred)"), "video.deflicker");
    check(Tr("overlay.video.smooth", "Smooth scaling to the window"), "video.smooth");
    check(Tr("overlay.video.low_latency", "Low input lag"), "video.low_latency");
    /* T39: texture packs (gs_texpack.c, docs/port/textures.md), PCSX2's folder layout and names */
    ImGui::SeparatorText(Tr("overlay.video.texture_packs", "Texture packs"));
    check(Tr("overlay.video.texture_replace", "Use replacement textures"), "video.texture_replace");
    check(Tr("overlay.video.texture_dump", "Dump textures (new ones as PNG)"), "video.texture_dump");
    ImGui::TextDisabled(TrFmt("overlay.video.texture_folder", "Folder: %s"), Tp_Folder());
    ImGui::TextDisabled("%s", Tr("overlay.video.texture_note", "(replacements/ is read once a run; video.texture_dir or "
                                 "TTT_TEXTURES moves it, on the next start)"));
    ImGui::EndDisabled();
}

static void page_audio(void) {
    slider(Tr("overlay.audio.volume", "Master volume"), "audio.volume", 0, 100, "%d%%");
    slider(Tr("overlay.audio.music", "Music"), "audio.music", 0, 100, "%d%%");
    slider(Tr("overlay.audio.effects", "Effects"), "audio.effects", 0, 100, "%d%%");
    check(Tr("overlay.audio.mute", "Mute"), "audio.mute");
    ImGui::TextWrapped("%s", Tr("overlay.audio.note", "The volumes scale the port's output (the window and TTT_WAV); the "
                                "game's own sound state is mixed as before. Music: the BGM and movie voices; effects: "
                                "every other voice. Nothing is played while the game speed is not 1x."));
}

/* T49: the mods (plat_mods.c, docs/port/mods.md): the folder's mods in load order, each on or off, moved up or down;
   what each replaces and what an earlier one hides. */
static void track_list(char *out, size_t n, uint32_t mask) {
    size_t k = 0;
    out[0] = 0;
    for (int t = 0; t < MODS_TRACKS && k + 4 < n; t++) {
        if (mask >> t & 1u) {
            k += (size_t)snprintf(out + k, n - k, "%s%d", k ? ", " : "", t);
        }
    }
}

static void page_mods(void) {
    char now[1200], list[160];
    check(Tr("overlay.mods.music", "Music mods"), "mods.music");
    check(Tr("overlay.mods.textures", "Texture mods"), "mods.textures");
    ImGui::TextDisabled(TrFmt("overlay.mods.folder", "Folder: %s"), Mods_Folder());
    if (ImGui::Button(L(Tr("overlay.mods.rescan", "Read the folder again"), "mods_rescan"))) {
        Mods_Rescan();
    }
    int t = Mods_MusicNow(now, sizeof now);
    const char *base = strrchr(now, '/');
    if (t >= 0) {
        ImGui::Text(TrFmt("overlay.mods.music_now", "Music now: track %d, %s"), t,
                    now[0] ? (base != nullptr ? base + 1 : now) : Tr("overlay.mods.music_game", "the game's own"));
    } else {
        ImGui::TextDisabled("%s", Tr("overlay.mods.music_none", "Music now: none"));
    }
    int n = Mods_Count();
    if (n == 0) {
        ImGui::TextWrapped("%s", Tr("overlay.mods.none", "No mods found. Each folder in the mods folder is a mod: music "
                                    "files named by track number (07.ogg, 21.wav) and a textures/ folder of replacement "
                                    "textures."));
    } else if (ImGui::BeginTable("mods", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn(Tr("overlay.mods.col_on", "On"));
        ImGui::TableSetupColumn(Tr("overlay.mods.col_mod", "Mod"), ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn(Tr("overlay.mods.col_music", "Music tracks"), ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableSetupColumn(Tr("overlay.mods.col_textures", "Textures"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn(Tr("overlay.mods.col_order", "Order"));
        ImGui::TableHeadersRow();
        for (int i = 0; i < n; i++) {
            ModInfo m;
            char id[200]; /* (the mod's name: up to 127 bytes, then the id) */
            if (!Mods_Get(i, &m)) {
                break;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool on = m.enabled != 0;
            snprintf(id, sizeof id, "%s###on%d", m.name, i); /* (the screen reader says the mod) */
            if (ImGui::Checkbox(id, &on)) {
                Mods_SetEnabled(i, on ? 1 : 0);
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(m.name);
            ImGui::TableNextColumn();
            track_list(list, sizeof list, m.music);
            ImGui::TextUnformatted(m.music ? list : "-");
            if (m.enabled && m.shadowed) {
                track_list(list, sizeof list, m.shadowed);
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), TrFmt("overlay.mods.music_hidden", "hidden by an earlier mod: %s"), list);
            }
            ImGui::TableNextColumn();
            if (!m.textures) {
                ImGui::TextUnformatted("-");
            } else if (m.texFiles > 0) {
                ImGui::Text(TrFmt("overlay.mods.tex_files", "%d files"), m.texFiles);
                if (m.texShadowed > 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), TrFmt("overlay.mods.tex_hidden", "%d hidden"), m.texShadowed);
                }
            } else {
                ImGui::TextUnformatted(Tr("overlay.mods.tex_yes", "yes"));
            }
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(i == 0);
            snprintf(id, sizeof id, "%s###up%d", Tr("overlay.mods.up", "Up"), i);
            if (ImGui::SmallButton(id)) {
                Mods_Move(i, -1);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(i == n - 1);
            snprintf(id, sizeof id, "%s###dn%d", Tr("overlay.mods.down", "Down"), i);
            if (ImGui::SmallButton(id)) {
                Mods_Move(i, 1);
            }
            ImGui::EndDisabled();
        }
        ImGui::EndTable();
    }
    ImGui::TextWrapped("%s", Tr("overlay.mods.note", "A mod higher in the list wins when two replace the same track or "
                                "texture. Music changes at once; textures on the next start. Mods change only what you "
                                "hear and see: the game runs the same with or without them, and netplay only shows the "
                                "other player which mods you use."));
}

/* T40: the game speed presets (percent), the real-time clock's rate (plat_hw.c). */
static void speed_choice(const char *label, const char *key, int lo) {
    static const int kSpeeds[] = {25, 50, 75, 100, 150, 200, 300, 400};
    int v = Settings_GetInt(key);
    ImGui::BeginDisabled(Settings_FromEnv(key) != 0);
    ImGui::TextUnformatted(label);
    for (int i = 0; i < (int)(sizeof kSpeeds / sizeof kSpeeds[0]); i++) {
        if (kSpeeds[i] < lo) {
            continue;
        }
        char name[16];
        snprintf(name, sizeof name, "%g##%s", kSpeeds[i] / 100.0, key);
        ImGui::SameLine();
        if (ImGui::RadioButton(name, v == kSpeeds[i])) {
            Settings_SetInt(key, kSpeeds[i]);
            Port_WindowApply();
        }
    }
    ImGui::EndDisabled();
    setting_note(key);
}

/* A pad button's name as shown (the file keeps its own names: Settings_ButtonName). */
static const char *button_label(int k) {
    switch (k) {
    case SB_UP: return Tr("button.up", "Up");
    case SB_DOWN: return Tr("button.down", "Down");
    case SB_LEFT: return Tr("button.left", "Left");
    case SB_RIGHT: return Tr("button.right", "Right");
    case SB_SQUARE: return Tr("button.square", "Square");
    case SB_TRIANGLE: return Tr("button.triangle", "Triangle");
    case SB_CROSS: return Tr("button.cross", "Cross");
    case SB_CIRCLE: return Tr("button.circle", "Circle");
    case SB_START: return Tr("button.start", "Start");
    case SB_SELECT: return Tr("button.select", "Select");
    default: return Settings_ButtonName(k); /* L1 ... R3: the same everywhere */
    }
}

/* A gamepad input's name as shown (the file keeps "south", "dpad_up"...). */
static const char *gamepad_label(unsigned in) {
    switch (in) {
    case 0: return Tr("gamepad.south", "South");
    case 1: return Tr("gamepad.east", "East");
    case 2: return Tr("gamepad.west", "West");
    case 3: return Tr("gamepad.north", "North");
    case 4: return Tr("gamepad.back", "Back");
    case 5: return Tr("gamepad.guide", "Guide");
    case 6: return Tr("gamepad.start", "Start");
    case 7: return Tr("gamepad.left_stick", "Left stick");
    case 8: return Tr("gamepad.right_stick", "Right stick");
    case 9: return Tr("gamepad.left_shoulder", "Left shoulder");
    case 10: return Tr("gamepad.right_shoulder", "Right shoulder");
    case 11: return Tr("gamepad.dpad_up", "D-pad up");
    case 12: return Tr("gamepad.dpad_down", "D-pad down");
    case 13: return Tr("gamepad.dpad_left", "D-pad left");
    case 14: return Tr("gamepad.dpad_right", "D-pad right");
    case 26: return Tr("gamepad.left_trigger", "Left trigger");
    case 27: return Tr("gamepad.right_trigger", "Right trigger");
    case SETTINGS_GP_NONE: return Tr("gamepad.none", "None");
    default: return Settings_GamepadInputName(in); /* misc, paddles, touchpad */
    }
}

static void page_controls(void) {
    uint16_t b[2][SB_COUNT];
    uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT];
    char name[32], id[200], hint[160], pn[8];
    Port_WindowGetBindings(b);
    Port_WindowGetGamepad(g);
    ImGui::SeparatorText(Tr("overlay.controls.keyboard", "Keyboard"));
    ImGui::TextWrapped("%s", Tr("overlay.controls.keyboard_help", "Click a key, then press the new key (Escape cancels). "
                                "F1, Escape and Tab (fast-forward) are kept for the overlay."));
    if (ImGui::BeginTable("keys", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn(Tr("overlay.controls.button", "Button"));
        ImGui::TableSetupColumn(Tr("overlay.controls.pad1", "Pad 1"), ImGuiTableColumnFlags_WidthFixed, S(160.0f));
        ImGui::TableSetupColumn(Tr("overlay.controls.pad2", "Pad 2"), ImGuiTableColumnFlags_WidthFixed, S(160.0f));
        ImGui::TableHeadersRow();
        for (int k = 0; k < SB_COUNT; k++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(button_label(k));
            for (int p = 0; p < 2; p++) {
                ImGui::TableNextColumn();
                bool waiting = !sCaptureGamepad && sCapturePad == p && sCaptureButton == k;
                Settings_KeyName(b[p][k], name, sizeof name);
                snprintf(id, sizeof id, "%s##k%d_%d", waiting ? Tr("overlay.controls.press_key", "press a key...") : name, p, k);
                snprintf(pn, sizeof pn, "%d", p + 1);
                Lang_Format(hint, sizeof hint, "sr.binding", "Pad {0}, {1}: {2}", pn, button_label(k), name, NULL);
                sr_hint(hint);
                if (ImGui::Button(id, ImVec2(S(150.0f), 0.0f))) {
                    sCapturePad = p;
                    sCaptureButton = k;
                    sCaptureGamepad = false;
                }
            }
        }
        ImGui::EndTable();
    }
    if (ImGui::Button(L(Tr("overlay.controls.default_keys", "Default keys"), "defkeys"))) {
        Settings_DefaultBindings(b);
        Port_WindowSetBindings(b);
        Settings_SaveBindings(b);
    }

    ImGui::SeparatorText(Tr("overlay.controls.gamepads", "Gamepads"));
    for (int p = 0; p < SETTINGS_PADS; p++) {
        const char *n = Port_WindowGamepadName(p);
        ImGui::Text(TrFmt("overlay.controls.pad_line", "Pad %d: %s"), p + 1,
                    n != nullptr ? n : p < 2 ? Tr("overlay.controls.none_keyboard", "(none: keyboard only)")
                                             : Tr("overlay.controls.none", "(none)"));
    }
    ImGui::TextDisabled("%s", Tr("overlay.controls.gamepads_note", "The first four gamepads SDL finds are pads 1-4, in "
                                 "the order they connect. Guide opens this window."));
    for (int p = 0; p < SETTINGS_PADS; p++) {
        snprintf(id, sizeof id, TrFmt("overlay.controls.pad_n", "Pad %d"), p + 1);
        snprintf(id + strlen(id), sizeof id - strlen(id), "###gp%d", p);
        if (p > 0) {
            ImGui::SameLine();
        }
        if (ImGui::RadioButton(id, sGamepadPage == p)) {
            sGamepadPage = p;
        }
    }
    ImGui::TextWrapped("%s", Tr("overlay.controls.gamepad_help", "Click a button, then press the gamepad's button or "
                                "trigger (guide or any key cancels)."));
    const int gp = sGamepadPage;
    if (ImGui::BeginTable("gamepad", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn(Tr("overlay.controls.button", "Button"));
        ImGui::TableSetupColumn(Tr("overlay.controls.gamepad", "Gamepad"), ImGuiTableColumnFlags_WidthFixed, S(200.0f));
        ImGui::TableHeadersRow();
        for (int k = 0; k < SB_GAMEPAD_COUNT; k++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(button_label(k));
            ImGui::TableNextColumn();
            bool waiting = sCaptureGamepad && sCapturePad == gp && sCaptureButton == k;
            snprintf(id, sizeof id, "%s##g%d_%d", waiting ? Tr("overlay.controls.press_button", "press a button...")
                                                          : gamepad_label(g[gp][k]), gp, k);
            snprintf(pn, sizeof pn, "%d", gp + 1);
            Lang_Format(hint, sizeof hint, "sr.binding", "Pad {0}, {1}: {2}", pn, button_label(k), gamepad_label(g[gp][k]), NULL);
            sr_hint(hint);
            if (ImGui::Button(id, ImVec2(S(190.0f), 0.0f))) {
                sCapturePad = gp;
                sCaptureButton = k;
                sCaptureGamepad = true;
            }
        }
        ImGui::EndTable();
    }
    snprintf(id, sizeof id, TrFmt("overlay.controls.default_buttons", "Default buttons (pad %d)"), gp + 1);
    snprintf(id + strlen(id), sizeof id - strlen(id), "###defgp");
    if (ImGui::Button(id)) {
        uint8_t d[SETTINGS_PADS][SB_GAMEPAD_COUNT];
        Settings_DefaultGamepad(d);
        memcpy(g[gp], d[gp], sizeof g[gp]);
        Port_WindowSetGamepad(g);
        Settings_SaveGamepad(g);
    }

    ImGui::SeparatorText(Tr("overlay.controls.sticks", "Sticks, vibration, multitap"));
    slider(Tr("overlay.controls.deadzone", "Stick dead zone"), "input.deadzone", 0, 90, "%d%%");
    check(Tr("overlay.controls.stick_dpad", "Left stick works the d-pad"), "input.stick_dpad");
    ImGui::TextDisabled("%s", Tr("overlay.controls.stick_note", "The game itself reads the d-pad only; the sticks also reach it as they are."));
    check(Tr("overlay.controls.vibration", "Vibration"), "input.vibration");
    check(Tr("overlay.controls.multitap", "Multitap: four players (pads 1-4 on port 1A-1D)"), "input.multitap");
    ImGui::TextDisabled("%s", Tr("overlay.controls.multitap_note", "With the multitap, pad 2 is slot 1B and port 2 is "
                                 "empty; recordings keep their own setting."));
}

static void page_game(void) {
    check(Tr("overlay.game.movies", "Movies"), "game.movies");
    check(Tr("overlay.game.pause", "Pause the game while this window is open"), "game.pause");
    ImGui::SeparatorText(Tr("overlay.game.speed_title", "Game speed"));
    speed_choice(Tr("overlay.game.speed", "Speed (x)"), "game.speed", 25);
    speed_choice(Tr("overlay.game.fast_forward", "Hold Tab (x)"), "game.fast_forward", 100);
    ImGui::TextDisabled("%s", Tr("overlay.game.speed_note", "Real time only: the game runs the same frames faster or "
                                 "slower; no sound off 1x."));
    ImGui::SeparatorText(Tr("overlay.game.data", "Data"));
    const char *disc = Port_DiscName(), *dat = Port_DataPath();
    ImGui::TextWrapped(TrFmt("overlay.game.disc", "Disc: %s"), disc != nullptr && *disc ? disc : Tr("overlay.game.not_opened", "(not opened)"));
    ImGui::TextWrapped(TrFmt("overlay.game.data_file", "Data file: %s"), dat != nullptr && *dat ? dat : Tr("overlay.controls.none", "(none)"));
    const char *kData[4] = {Tr("overlay.game.data_verified", "USA release (verified)"), data_mark(),
                            Tr("overlay.game.data_checking", "USA release so far (TEKKEN.BIN being checked)"),
                            Tr("overlay.game.data_unchecked", "not checked")};
    int st = Port_DataState();
    ImGui::TextWrapped(TrFmt("overlay.game.data_state", "Game data: %s"), st >= 0 && st < 4 ? kData[st] : "?");
    if ((st == 1 || st == 3) && *Port_DataWhy()) {
        ImGui::TextWrapped("(%s)", Port_DataWhy());
    }
    ImGui::SeparatorText(Tr("overlay.game.settings_file", "Settings file"));
    const char *p = Settings_Path();
    ImGui::TextWrapped("%s", *p ? p : Tr("overlay.game.no_settings_file", "(none: set TTT_CONFIG or HOME)"));
    if (*Settings_SaveError()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), TrFmt("overlay.game.not_saved", "Not saved: %s"), Settings_SaveError());
    }
}

/* T40: cheats (plat_cheat.c, docs/port/cheats.md). They change the game's own state. */
static void page_cheats(void) {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", Tr("overlay.cheats.warning", "These modify the game's state."));
    ImGui::TextWrapped("%s", Tr("overlay.cheats.note", "Unlocking sets the game's own unlock flags: every character, "
                                "TEKKEN BOWL, THEATER (every ending) and GALLERY. Your next memory card save keeps them, "
                                "and turning this off does not lock them again. The character select picks the new set "
                                "at the next game start."));
    check(Tr("overlay.cheats.unlock_all", "Unlock everything"), "cheats.unlock_all");
}

/* ---- the language (T47): the settings' ui.language, switched at once ---- */
static LangInfo sLangs[32];
static int sLangN = -1;

static const char *lang_name(const char *code) {
    for (int i = 0; i < sLangN; i++) {
        if (strcmp(sLangs[i].code, code) == 0) {
            return sLangs[i].name;
        }
    }
    return code;
}

static void language_picker(void) {
    char cur[32], autoName[128];
    if (sLangN < 0) {
        sLangN = Lang_List(sLangs, (int)(sizeof sLangs / sizeof sLangs[0]));
    }
    Settings_GetStr("ui.language", cur, sizeof cur);
    bool automatic = cur[0] == 0 || strcmp(cur, "auto") == 0;
    Lang_Format(autoName, sizeof autoName, "overlay.ui.language_auto", "Automatic ({0})", lang_name(Lang_Current()), NULL);
    ImGui::BeginDisabled(Settings_FromEnv("ui.language") != 0);
    ImGui::SetNextItemWidth(S(260.0f));
    if (ImGui::BeginCombo(L(Tr("overlay.ui.language", "Language"), "language"), automatic ? autoName : lang_name(cur))) {
        if (ImGui::Selectable(L(autoName, "auto"), automatic)) {
            Settings_SetStr("ui.language", "");
            Lang_Set("");
        }
        for (int i = 0; i < sLangN; i++) {
            if (ImGui::Selectable(L(sLangs[i].name, sLangs[i].code), !automatic && strcmp(cur, sLangs[i].code) == 0)) {
                Settings_SetStr("ui.language", sLangs[i].code);
                Lang_Set(sLangs[i].code);
            }
        }
        ImGui::EndCombo();
    }
    sr_value(automatic ? autoName : lang_name(cur));
    ImGui::EndDisabled();
    setting_note("ui.language");
}

/* T48: the interface's size and colours, the screen reader (docs/port/accessibility.md) */
static void page_access(void) {
    language_picker();
    ImGui::SeparatorText(Tr("overlay.ui.look", "Look"));
    slider(Tr("overlay.ui.scale", "Interface size"), "ui.scale", 50, 300, "%d%%");
    check(Tr("overlay.ui.high_contrast", "High contrast"), "ui.high_contrast");
    ImGui::SeparatorText(Tr("overlay.ui.screen_reader_title", "Screen reader"));
    check(Tr("overlay.ui.screen_reader", "Read the focused item aloud"), "ui.screen_reader");
    ImGui::BeginDisabled(Settings_GetInt("ui.screen_reader") == 0);
    check(Tr("overlay.ui.announce_game", "Announce the game's menus and fights"), "ui.announce_game");
    ImGui::EndDisabled();
    ImGui::TextDisabled(TrFmt("overlay.ui.output", "Output: %s"), Speech_Output());
    ImGui::TextWrapped("%s", Tr("overlay.ui.screen_reader_note", "Speech goes through speech-dispatcher when it is "
                                "installed. Arrow keys or the d-pad move between the items, Enter or the south button "
                                "activates. The game's announcements read its memory only: the main menu's item, the "
                                "characters picked, each round, the life bars by quarters."));
}

static void page_about(void) {
    ImGui::TextUnformatted(Tr("overlay.about.title", "Tekken Tag Tournament, native PC port"));
    ImGui::Text(TrFmt("overlay.about.build", "Build %s"), TTT_GIT);
    if (Port_DataState() == 1) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", data_mark());
    }
    ImGui::Text(TrFmt("overlay.about.imgui", "Dear ImGui %s (MIT)"), IMGUI_VERSION);
    ImGui::Separator();
    ImGui::TextWrapped("%s", Tr("overlay.about.keys", "F1: this window. Hold Tab: fast-forward. Escape or the close "
                                "button: quit (asks first). The game's data comes from your own disc; nothing of the "
                                "game is in this program."));
}

/* ---- netplay (T46, plat_net.c, docs/port/matchmaking.md): the server, the session, the chat ---- */
static char sNetServer[256], sNetName[64], sNetCode[16], sNetChat[160];
static bool sNetInit;

/* A text setting edited in a field: saved when Return is pressed or the field loses the focus. */
static void text_setting(const char *label, const char *key, char *buf, int n) {
    ImGui::BeginDisabled(Settings_FromEnv(key) != 0);
    ImGui::SetNextItemWidth(S(260.0f));
    ImGui::InputText(L(label, key), buf, (size_t)n);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Settings_SetStr(key, buf);
    }
    ImGui::EndDisabled();
    setting_note(key);
}

static void chat_lines(float height) {
    ImGui::BeginChild("##chatlog", ImVec2(0.0f, height), ImGuiChildFlags_Borders);
    int n = Port_NetChatCount();
    for (int i = 0; i < n; i++) {
        char t[160];
        int mine = 0;
        if (Port_NetChatLine(i, &mine, t, sizeof t)) {
            PortNetInfo ni;
            Port_NetInfo(&ni);
            ImGui::TextWrapped("%s: %s", mine ? Tr("overlay.net.you", "You") : (ni.peer[0] ? ni.peer : "?"), t);
        }
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

static void page_netplay(void) {
    PortNetInfo ni;
    Port_NetInfo(&ni);
    if (!sNetInit) {
        sNetInit = true;
        Settings_GetStr("net.server", sNetServer, sizeof sNetServer);
        Settings_GetStr("net.name", sNetName, sizeof sNetName);
    }
    ImGui::SeparatorText(Tr("overlay.net.server_title", "Matchmaking server"));
    text_setting(Tr("overlay.net.server", "Server (address:port)"), "net.server", sNetServer, sizeof sNetServer);
    text_setting(Tr("overlay.net.name", "Your name"), "net.name", sNetName, sizeof sNetName);
    check(Tr("overlay.net.punch", "Try a direct connection first"), "net.punch");
    if (sNetServer[0] == 0) {
        ImGui::TextWrapped("%s", Tr("overlay.net.no_server", "No server is built in: run your own (ttt-server) or ask "
                                    "a friend for theirs. See docs/port/matchmaking.md."));
    }
    if (ni.phase == NET_PHASE_OFF) {
        ImGui::SeparatorText(Tr("overlay.net.play_title", "Play online"));
        bool can = Port_NetCanRestart() && sNetServer[0] != 0;
        ImGui::TextWrapped("%s", Tr("overlay.net.restart_note", "A session starts the game again from the beginning on "
                                    "both sides."));
        ImGui::BeginDisabled(!can);
        if (ImGui::Button(L(Tr("overlay.net.create", "Create a room"), "create"))) {
            Port_NetRestart(1, "");
        }
        ImGui::SameLine();
        if (ImGui::Button(L(Tr("overlay.net.quick", "Quick match"), "quick"))) {
            Port_NetRestart(3, "");
        }
        ImGui::SetNextItemWidth(S(120.0f));
        ImGui::InputText("##roomcode", sNetCode, sizeof sNetCode, ImGuiInputTextFlags_CharsUppercase);
        ImGui::SameLine();
        if (ImGui::Button(L(Tr("overlay.net.join", "Join this room"), "join")) && sNetCode[0] != 0) {
            Port_NetRestart(2, sNetCode);
        }
        ImGui::EndDisabled();
    } else {
        const char *phase = ni.phase == NET_PHASE_SERVER ? Tr("overlay.net.phase_server", "Contacting the server") :
                            ni.phase == NET_PHASE_WAITING ? Tr("overlay.net.phase_waiting", "Waiting for a player") :
                            ni.phase == NET_PHASE_CONNECTING ? Tr("overlay.net.phase_connecting", "Connecting") :
                            Tr("overlay.net.phase_playing", "Playing");
        ImGui::SeparatorText(Tr("overlay.net.session_title", "Session"));
        ImGui::Text(TrFmt("overlay.net.phase", "State: %s"), phase);
        if (ni.code[0]) {
            ImGui::Text(TrFmt("overlay.net.code", "Room code: %s"), ni.code);
        }
        if (ni.peer[0]) {
            ImGui::Text(TrFmt("overlay.net.peer", "Playing with: %s (pad %d)"), ni.peer, 2 - ni.side);
        }
        if (ni.peerMods[0]) { /* T49: information only (mods change no game state; the handshake never compares them) */
            ImGui::TextWrapped(TrFmt("overlay.net.peer_mods", "Their mods: %s"), ni.peerMods);
            ImGui::TextWrapped(TrFmt("overlay.net.my_mods", "Your mods: %s"), ni.mods);
        }
        if (ni.phase == NET_PHASE_PLAYING) {
            ImGui::Text(TrFmt("overlay.net.path", "Connection: %s"),
                        ni.path == NET_PATH_RELAY ? Tr("net.path.relay", "relayed") : Tr("net.path.direct", "direct"));
            ImGui::Text(TrFmt("overlay.net.rtt", "Round trip: %u ms; input delay %u frames"), ni.rttMs, ni.delay);
            ImGui::Text(TrFmt("overlay.net.rollbacks", "Rollbacks: %u (%u frames run again); waits: %u"), ni.rollbacks,
                        ni.framesAgain, ni.stalls);
            ImGui::SeparatorText(Tr("overlay.net.chat_title", "Chat"));
            chat_lines(S(110.0f));
            ImGui::SetNextItemWidth(S(360.0f));
            bool send = ImGui::InputText("##chat", sNetChat, sizeof sNetChat, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            send |= ImGui::Button(L(Tr("overlay.net.send", "Send"), "send"));
            if (send && Port_NetChatSend(sNetChat)) {
                sNetChat[0] = 0;
                ImGui::SetKeyboardFocusHere(-1);
            }
        }
    }
    if (ni.update == NET_UPDATE_NEWER) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), TrFmt("overlay.net.update_newer", "Version %s is available "
                           "(nothing is downloaded: get it from a source you trust)."), ni.updateVersion);
    } else if (ni.update == NET_UPDATE_BAD_SIGNATURE || ni.update == NET_UPDATE_MALFORMED) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", Tr("overlay.net.update_bad", "The server's update "
                           "notice is not correctly signed: ignored."));
    }
}

/* The last chat lines over the game for 10 s after one arrives, the overlay closed. */
static void chat_toast(void) {
    ImGuiIO &io = ImGui::GetIO();
    int n = Port_NetChatCount();
    ImGui::SetNextWindowPos(ImVec2(8.0f, io.DisplaySize.y - 8.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("##chattoast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                                                 ImGuiWindowFlags_NoFocusOnAppearing)) {
        PortNetInfo ni;
        Port_NetInfo(&ni);
        for (int i = n > 4 ? n - 4 : 0; i < n; i++) {
            char t[160];
            int mine = 0;
            if (Port_NetChatLine(i, &mine, t, sizeof t)) {
                ImGui::Text("%s: %s", mine ? Tr("overlay.net.you", "You") : (ni.peer[0] ? ni.peer : "?"), t);
            }
        }
        ImGui::TextDisabled("%s", Tr("overlay.net.chat_hint", "F1: reply"));
    }
    ImGui::End();
}

static void meter(void) {
    unsigned n = sShownN < 64 ? sShownN : 64;
    if (n < 2) {
        return;
    }
    int64_t last = sShown[(sShownN - 1) % 64], first = sShown[(sShownN - n) % 64], worst = 0;
    for (unsigned i = 1; i < n; i++) {
        int64_t d = sShown[(sShownN - i) % 64] - sShown[(sShownN - i - 1) % 64];
        worst = d > worst ? d : worst;
    }
    float avg = (float)(last - first) / (float)(n - 1) * 1e-6f;
    ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f));
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("##meter", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                                             ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::Text(TrFmt("overlay.meter", "%.1f FPS  %.2f ms  (worst %.2f ms)"), avg > 0.0f ? 1000.0f / avg : 0.0f, avg,
                    (float)worst * 1e-6f);
    }
    ImGui::End();
}

/* ---- the setup screen (T43) ---- */
static char sSetupPath[1024], sBrowseDir[1024];
static bool sSetupInit, sBrowse, sSetupAsked;
static PortSetupEntry sEntries[512];
static int sEntryN = -1;
static int sSetupTest = -1; /* TTT_SETUP_TEST: the screen's frames */
static char sSetupSaid[700];  /* the error or stage said last (the screen reader) */

static void browse_to(const char *dir) {
    snprintf(sBrowseDir, sizeof sBrowseDir, "%s", *dir ? dir : "/");
    size_t n = strlen(sBrowseDir);
    while (n > 1 && sBrowseDir[n - 1] == '/') {
        sBrowseDir[--n] = 0;
    }
    sEntryN = Port_SetupList(sBrowseDir, sEntries, (int)(sizeof sEntries / sizeof sEntries[0]));
}

static void parent_of(const char *path, char *out, size_t n) {
    snprintf(out, n, "%s", path);
    char *s = strrchr(out, '/');
    if (s == out) {
        out[1] = 0;
    } else if (s != nullptr) {
        *s = 0;
    } else {
        snprintf(out, n, "/");
    }
}

static void browse_up(void) {
    char up[1024];
    parent_of(sBrowseDir, up, sizeof up);
    browse_to(up);
}

static void setup_request(void) {
    Port_SetupRequest(sSetupPath);
    sSetupAsked = true;
}

static void setup_test_step(const PortSetupView &v) {
    const char *t = getenv("TTT_SETUP_TEST");
    if (t == nullptr || *t == 0) {
        return;
    }
    int n = ++sSetupTest;
    if (n == 1) { /* the browser on the disc's folder (or the folder holding the image) */
        char up[1024];
        parent_of(t, up, sizeof up);
        sBrowse = true;
        browse_to(up);
    } else if (n == 5) { /* the path typed */
        snprintf(sSetupPath, sizeof sSetupPath, "%s", t);
    } else if (n == 8) { /* "Check and install" */
        setup_request();
    }
    Port_SetupTestNote(sEntryN, sSetupAsked && !v.busy && v.error[0] != 0);
}

static void setup_screen(void) {
    ImGuiIO &io = ImGui::GetIO();
    PortSetupView v;
    Port_SetupView(&v);
    if (!sSetupInit) {
        sSetupInit = true;
        snprintf(sSetupPath, sizeof sSetupPath, "%s", v.path);
        browse_to(Port_HomeDir());
    }
    setup_test_step(v);
    char got[1024];
    if (Port_WindowDialogResult(got, sizeof got)) {
        snprintf(sSetupPath, sizeof sSetupPath, "%s", got);
    }
    { /* the screen reader: a new reason, or the stage of the check */
        const char *now = v.busy ? v.stage : v.error;
        if (strcmp(now, sSetupSaid) != 0) {
            snprintf(sSetupSaid, sizeof sSetupSaid, "%s", now);
            sr_say(now, 0);
        }
    }
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin(L(Tr("setup.window", "Setup"), "setup"), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        ImGui::SeparatorText(Tr("setup.title", "Tekken Tag Tournament: setup"));
        language_picker();
        ImGui::TextWrapped("%s", Tr("setup.intro", "The game's data comes from your own disc of Tekken Tag Tournament "
                                    "(USA, SLUS-20001); nothing of the game is in this program. Pick the disc: the "
                                    "folder of a mounted disc or of its files, or an image of it (.iso, .bin or .cue). It "
                                    "is checked against the unmodified release, then the game's data file is made and "
                                    "the game starts."));
        if (v.error[0] != 0 && !v.busy) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", v.error);
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        ImGui::BeginDisabled(v.busy != 0);
        ImGui::TextUnformatted(Tr("setup.disc", "Disc"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        sr_hint(Tr("setup.disc_field", "Disc path"));
        if (ImGui::InputText("##disc", sSetupPath, sizeof sSetupPath, ImGuiInputTextFlags_EnterReturnsTrue)) {
            setup_request();
        }
        sr_value(sSetupPath);
        if (ImGui::Button(L(Tr("setup.install", "Check and install"), "install"))) {
            setup_request();
        }
        ImGui::SameLine();
        if (ImGui::Button(L(sBrowse ? Tr("setup.hide_browser", "Hide the browser") : Tr("setup.browse", "Browse..."), "browse"))) {
            sBrowse = !sBrowse;
        }
        if (Port_WindowDialogAvailable()) {
            ImGui::SameLine();
            if (ImGui::Button(L(Tr("setup.image_dialog", "Image file (system dialog)..."), "dlgfile"))) {
                Port_WindowDialog(0, sBrowseDir);
            }
            ImGui::SameLine();
            if (ImGui::Button(L(Tr("setup.folder_dialog", "Folder (system dialog)..."), "dlgdir"))) {
                Port_WindowDialog(1, sBrowseDir);
            }
        }
        ImGui::EndDisabled();
        if (v.busy) {
            ImGui::ProgressBar((float)v.permille / 1000.0f, ImVec2(-1.0f, 0.0f), v.stage);
        }
        float footer = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetTextLineHeightWithSpacing();
        if (sBrowse && !v.busy) {
            ImGui::SeparatorText(Tr("setup.browse_title", "Browse"));
            if (ImGui::Button(L(Tr("setup.up", "Up"), "up"))) {
                browse_up();
            }
            ImGui::SameLine();
            if (ImGui::Button(L(Tr("setup.home", "Home"), "home"))) {
                browse_to(Port_HomeDir());
            }
            ImGui::SameLine();
            if (ImGui::Button(L(Tr("setup.use_folder", "Use this folder"), "usedir"))) {
                snprintf(sSetupPath, sizeof sSetupPath, "%s", sBrowseDir);
            }
            ImGui::SameLine();
            ImGui::TextWrapped("%s", sBrowseDir);
            if (ImGui::BeginChild("##entries", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders)) {
                if (sEntryN < 0) {
                    ImGui::TextDisabled("%s", Tr("setup.unreadable", "This folder cannot be read."));
                } else if (sEntryN == 0) {
                    ImGui::TextDisabled("%s", Tr("setup.empty", "No folders or disc images (.iso, .bin, .cue) here."));
                }
                char go[1024] = "";
                const char *kDisc = Tr("setup.tag_disc", "[disc]"), *kImage = Tr("setup.tag_image", "[image]");
                for (int i = 0; i < sEntryN; i++) {
                    const PortSetupEntry &e = sEntries[i];
                    char label[400];
                    snprintf(label, sizeof label, "%s%s%s%s##e%d", e.kind == 3 ? kDisc : e.kind == 2 ? kImage : "",
                             e.kind == 3 || e.kind == 2 ? " " : "", e.name, e.kind != 2 ? "/" : "", i);
                    char full[1300];
                    snprintf(full, sizeof full, "%s/%s", strcmp(sBrowseDir, "/") == 0 ? "" : sBrowseDir, e.name);
                    if (ImGui::Selectable(label, strcmp(full, sSetupPath) == 0, ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (e.kind == 1 || (e.kind == 3 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))) {
                            snprintf(go, sizeof go, "%s", full); /* into the folder */
                        } else {
                            snprintf(sSetupPath, sizeof sSetupPath, "%s", full); /* a disc folder or an image */
                        }
                    }
                }
                if (go[0]) {
                    browse_to(go);
                }
            }
            ImGui::EndChild();
        }
        ImGui::Separator();
        ImGui::TextDisabled(TrFmt("setup.data_goes", "The data file goes to %s"), v.dataPath != nullptr && *v.dataPath ? v.dataPath : "?");
        if (ImGui::Button(L(Tr("setup.quit", "Quit"), "quit"))) {
            Overlay_AskQuit();
        }
    }
    ImGui::End();
}

static void ui(void) {
    ImGuiIO &io = ImGui::GetIO();
    if (Settings_GetInt("video.fps_meter") && !Port_SetupActive()) {
        meter();
    }
    if (!sSettings && Port_NetChatRecent()) {
        chat_toast();
    }
    if (Port_SetupActive()) {
        setup_screen();
    }
    if (sSettings && !Port_SetupActive()) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(S(640.0f), S(520.0f)), ImGuiCond_Appearing);
        bool open = true;
        if (ImGui::Begin(L(Tr("overlay.window", "Settings (F1)"), "settings"), &open, ImGuiWindowFlags_NoCollapse)) {
            if (Port_DataState() == 1) { /* on every page: the game data in use is not the verified USA release */
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", data_mark());
                sMarkDrawn++;
            }
            if (ImGui::BeginTabBar("pages")) {
                if (tab(Tr("overlay.tab.video", "Video"), "video", 0)) { page_video(); ImGui::EndTabItem(); }
                if (tab(Tr("overlay.tab.audio", "Audio"), "audio", 1)) { page_audio(); ImGui::EndTabItem(); }
                if (tab(Tr("overlay.tab.controls", "Controls"), "controls", 2)) { page_controls(); ImGui::EndTabItem(); }
                if (tab(Tr("overlay.tab.game", "Game"), "game", 3)) { page_game(); ImGui::EndTabItem(); }
                if (tab(Tr("overlay.tab.cheats", "Cheats"), "cheats", 4)) { page_cheats(); ImGui::EndTabItem(); }
                if (tab(Tr("overlay.tab.mods", "Mods"), "mods", 5)) { page_mods(); ImGui::EndTabItem(); }
                if (tab(Tr("overlay.tab.access", "Accessibility"), "access", 6)) { page_access(); ImGui::EndTabItem(); }
                if (tab(Tr("overlay.tab.about", "About"), "about", 7)) { page_about(); ImGui::EndTabItem(); }
                if (tab(Tr("overlay.tab.netplay", "Netplay"), "netplay", 8)) { page_netplay(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        if (!open) {
            Overlay_Toggle();
        }
    }
    if (sQuitAsk) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowFocus();
        if (ImGui::Begin(L(Tr("overlay.quit.window", "Quit?"), "quit"), nullptr, ImGuiWindowFlags_NoCollapse |
                                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::TextUnformatted(Tr("overlay.quit.question", "Quit Tekken Tag Tournament?"));
            ImGui::TextDisabled("%s", Tr("overlay.quit.escape", "Escape again also quits."));
            if (ImGui::Button(L(Tr("overlay.quit.yes", "Yes, quit"), "yes"), ImVec2(S(140.0f), 0.0f))) {
                sQuitYes = true;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button(L(Tr("overlay.quit.no", "No"), "no"), ImVec2(S(140.0f), 0.0f))) {
                sQuitAsk = false;
                Port_WindowApply();
            }
        }
        ImGui::End();
    }
}

extern "C" void Overlay_Render(int win_w, int win_h, int pix_w, int pix_h, int64_t now_ns) {
    if (sMode == 0 || win_w <= 0 || win_h <= 0) {
        return;
    }
    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)win_w, (float)win_h);
    io.DisplayFramebufferScale = ImVec2((float)pix_w / (float)win_w, (float)pix_h / (float)win_h);
    float dt = sLastNs != 0 ? (float)(now_ns - sLastNs) * 1e-9f : 1.0f / 60.0f;
    io.DeltaTime = dt > 0.0001f ? dt : 0.0001f;
    sLastNs = now_ns;
    apply_style();
    if (sMode == 1) {
        if (Port_SetupActive()) { /* no picture of the game under the setup screen: a plain background */
            if (sBindFramebuffer != nullptr) {
                sBindFramebuffer(0x8D40 /* GL_FRAMEBUFFER */, 0);
            }
            glViewport(0, 0, pix_w, pix_h);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        ImGui_ImplOpenGL3_NewFrame();
    }
    sr_frame_begin();
    ImGui::NewFrame();
    ui();
    ImGui::Render();
    sr_frame_end();
    if (sMode == 1) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    } else {
        sw_render(ImGui::GetDrawData());
    }
    sDrawn++;
    if (sTest >= 0) {
        sTestFrames++;
    }
}

/* ---- TTT_OVERLAY_TEST: the overlay driven from inside, no input from the desktop ---- */
#define TEST_PAGES 9

extern "C" void Overlay_TestStep(void) {
    if (sTest < 0 || sMode == 0) {
        return;
    }
    int n = sTest++; /* the window's test (plat_window.c) opens the settings with F1 at its first frame */
    sTestTab = n >= 2 && n < 2 + 4 * TEST_PAGES ? (n - 2) / 4 : -1; /* each page for four frames */
    if (sQuitAsk) { /* the "Quit?" question is up (asked at the window's frame 30): the arrow keys move the focus
                       between its buttons, as a keyboard player would (the screen reader says each): right pressed
                       and released, then left */
        static int q;
        int k = q++;
        if (k >= 2 && k <= 5) {
            ImGui::GetIO().AddKeyEvent(k <= 3 ? ImGuiKey_RightArrow : ImGuiKey_LeftArrow, (k & 1) == 0);
        }
    }
    if (n == 28) { /* bind pad 1's square through the capture path: a made-up key event, as SDL would hand it */
        uint8_t ev[128] = {0};
        uint32_t type = EV_KEY_DOWN, sc = 44; /* Space */
        sCapturePad = 0;
        sCaptureButton = SB_SQUARE;
        memcpy(ev, &type, 4);
        memcpy(ev + kScancodeAt, &sc, 4);
        ev[kKeyDownAt] = 1;
        Overlay_Event(type, ev);
    }
    if (n == 29) { /* bind pad 2's cross to north through the gamepad capture path: a made-up gamepad event */
        uint8_t ev[128] = {0};
        uint32_t type = EV_PAD_DOWN;
        sCapturePad = 1;
        sCaptureButton = SB_CROSS;
        sCaptureGamepad = true;
        memcpy(ev, &type, 4);
        ev[kPadCodeAt] = 3; /* north */
        ev[kPadDownAt] = 1;
        Overlay_Event(type, ev);
    }
    if (n == 40) { /* the window's test asked "Quit?" with Escape at its frame 30: answer no */
        sQuitAsk = false;
        Port_WindowApply();
    }
}

static int pages(void) {
    int n = 0;
    for (int i = 0; i < TEST_PAGES; i++) {
        n += sTestPages >> i & 1;
    }
    return n;
}

/* T47: the accented letters of the catalogs (Latin-1 and Latin Extended-A) in the font in use */
static int glyphs(int *of) {
    static const ImWchar kTest[] = {0xE1, 0xE9, 0xED, 0xF3, 0xFA, 0xF1, 0xFC, 0xC1, 0xC9, 0xCD, 0xD3, 0xDA, 0xD1, 0xBF,
                                    0xA1, 0xE7, 0xE0, 0xE8, 0xF6, 0xE4, 0xDF};
    ImFont *f = ImGui::GetIO().Fonts->Fonts.Size > 0 ? ImGui::GetIO().Fonts->Fonts[0] : nullptr;
    int n = 0;
    *of = (int)(sizeof kTest / sizeof kTest[0]);
    for (int i = 0; f != nullptr && i < *of; i++) {
        n += f->IsGlyphInFont(kTest[i]);
    }
    return n;
}

extern "C" void Overlay_Report(void) {
    if (sTest >= 0) {
        uint16_t b[2][SB_COUNT];
        char name[32];
        int of, has = glyphs(&of);
        Port_WindowGetBindings(b);
        Settings_KeyName(b[0][SB_SQUARE], name, sizeof name);
        printf("ttt: overlay: %d frames drawn (%s), pages %d/%d, key capture %d (pad 1 square: %s)\n", sTestFrames,
               sMode == 1 ? "gl" : "sw", pages(), TEST_PAGES, sTestCapture, name);
        printf("ttt: overlay: data mark drawn %u times\n", sMarkDrawn);
        uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT];
        Port_WindowGetGamepad(g);
        printf("ttt: overlay: gamepad capture %d (pad 2 cross: %s)\n", sTestGpCapture,
               Settings_GamepadInputName(g[1][SB_CROSS]));
        printf("ttt: overlay: language %s, accented glyphs %d/%d, screen reader %u texts (%s)\n", Lang_Current(), has, of,
               sSrCount, Speech_Output());
        fflush(stdout);
    }
}
