/*
 * The settings overlay (T36, docs/port/settings.md): Dear ImGui (third_party/imgui, MIT) over the window's picture.
 * F1 opens it; the close button or Escape asks "Quit?".
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
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_opengl3_loader.h"
#include "overlay.h"
#include "plat_settings.h"

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
    EV_PAD_UP = 0x652,
    PIXELFORMAT_RGBA32 = 0x16762004u, TEXTUREACCESS_STATIC = 0, BLENDMODE_BLEND = 1,
};
static const int kScancodeAt = 24, kKeyModAt = 32, kKeyDownAt = 36, kKeyRepeatAt = 37; /* SDL_KeyboardEvent */
static const int kMouseXAt = 28, kMouseYAt = 32, kButtonAt = 24, kButtonDownAt = 25;  /* SDL_Mouse*Event */
static const int kWheelXAt = 24, kWheelYAt = 28, kWheelDirAt = 32;
static const int kPadCodeAt = 20, kPadDownAt = 21, kPadAxisValueAt = 24; /* SDL_Gamepad{Button,Axis}Event */

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
static unsigned sMarkDrawn; /* frames the settings window showed the [modified game data] mark (TTT_OVERLAY_TEST) */

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
    ImGui::StyleColorsDark();
    ImGuiStyle &st = ImGui::GetStyle();
    st.FontSizeBase = 18.0f;
    st.WindowRounding = 6.0f;
    st.FrameRounding = 4.0f;
    st.ScaleAllSizes(1.25f);
    const char *e = getenv("TTT_OVERLAY_TEST");
    sTest = e != nullptr && *e != 0 && *e != '0' ? 0 : -1;
}

extern "C" int Overlay_InitGl(void *(*getproc)(const char *name)) {
    if (sMode != 0) {
        return 1;
    }
    imgl3wInit2((GL3WGetProcAddressProc)getproc);
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
    if (sMode == 0 || sQuitAsk) {
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
extern "C" int Overlay_Open(void) { return sMode != 0 && (sSettings || sQuitAsk); }

extern "C" int Overlay_Visible(void) {
    return sMode != 0 && (sSettings || sQuitAsk || sTest >= 0 || Settings_GetInt("video.fps_meter") != 0);
}

extern "C" void Overlay_FrameShown(int64_t now_ns) {
    sShown[sShownN++ % 64] = now_ns;
}

/* ---- the windows ---- */
static void setting_note(const char *key) {
    const char *env = Settings_Env(key);
    if (Settings_FromEnv(key) && env != nullptr) {
        ImGui::SameLine();
        ImGui::TextDisabled("(set by %s)", env);
    } else if (Settings_NeedsRestart(key)) {
        ImGui::SameLine();
        ImGui::TextDisabled("(on the next start)");
    }
}

static void check(const char *label, const char *key) {
    bool v = Settings_GetInt(key) != 0;
    ImGui::BeginDisabled(Settings_FromEnv(key) != 0);
    if (ImGui::Checkbox(label, &v)) {
        Settings_SetInt(key, v ? 1 : 0);
        Port_WindowApply();
    }
    ImGui::EndDisabled();
    setting_note(key);
}

static void slider(const char *label, const char *key, int lo, int hi, const char *fmt) {
    int v = Settings_GetInt(key);
    ImGui::BeginDisabled(Settings_FromEnv(key) != 0);
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::SliderInt(label, &v, lo, hi, fmt)) {
        Settings_SetInt(key, v);
        Port_WindowApply();
    }
    ImGui::EndDisabled();
    setting_note(key);
}

static bool tab(const char *name, int index) {
    ImGuiTabItemFlags f = sTestTab == index ? ImGuiTabItemFlags_SetSelected : 0;
    bool open = ImGui::BeginTabItem(name, nullptr, f);
    if (open && sTestTab == index) {
        sTestPages |= 1 << index;
    }
    return open;
}

static void page_video(void) {
    char r[16];
    Settings_GetStr("video.renderer", r, sizeof r);
    int gl = strcmp(r, "sw") != 0;
    ImGui::Text("Renderer now: %s", Port_WindowRendererName());
    ImGui::BeginDisabled(Settings_FromEnv("video.renderer") != 0);
    if (ImGui::RadioButton("OpenGL", gl)) {
        Settings_SetStr("video.renderer", "gl");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Software", !gl)) {
        Settings_SetStr("video.renderer", "sw");
    }
    ImGui::EndDisabled();
    setting_note("video.renderer");
    check("V-sync", "video.vsync");
    slider("Window scale", "video.scale", 1, 6, "%dx (640x448 each)");
    check("Fullscreen", "video.fullscreen");
    check("Frame-time meter", "video.fps_meter");
    /* T37: the GL back end's options (gs_gl.c reads them at each picture; docs/port/video.md) */
    ImGui::SeparatorText("OpenGL");
    ImGui::BeginDisabled(!gl);
    slider("Internal resolution", "video.resolution", 1, 8, "%dx");
    {
        static const char *const kAspect[3] = {"4:3", "16:9", "21:9"};
        char a[16];
        Settings_GetStr("video.aspect", a, sizeof a);
        ImGui::BeginDisabled(Settings_FromEnv("video.aspect") != 0);
        ImGui::TextUnformatted("Aspect");
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
    check("Game's flicker filter (half height, blurred)", "video.deflicker");
    check("Smooth scaling to the window", "video.smooth");
    check("Low input lag", "video.low_latency");
    /* T39: texture packs (gs_texpack.c, docs/port/textures.md), PCSX2's folder layout and names */
    ImGui::SeparatorText("Texture packs");
    check("Use replacement textures", "video.texture_replace");
    check("Dump textures (new ones as PNG)", "video.texture_dump");
    ImGui::TextDisabled("Folder: %s", Tp_Folder());
    ImGui::TextDisabled("(replacements/ is read once a run; video.texture_dir or TTT_TEXTURES moves it, on the next start)");
    ImGui::EndDisabled();
}

static void page_audio(void) {
    slider("Master volume", "audio.volume", 0, 100, "%d%%");
    slider("Music", "audio.music", 0, 100, "%d%%");
    slider("Effects", "audio.effects", 0, 100, "%d%%");
    check("Mute", "audio.mute");
    ImGui::TextWrapped("The volumes scale the port's output (the window and TTT_WAV); the game's own sound state is "
                       "mixed as before. Music: the BGM and movie voices; effects: every other voice. Nothing is played "
                       "while the game speed is not 1x.");
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

static void page_controls(void) {
    uint16_t b[2][SB_COUNT];
    uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT];
    char name[32], id[48];
    Port_WindowGetBindings(b);
    Port_WindowGetGamepad(g);
    ImGui::SeparatorText("Keyboard");
    ImGui::TextWrapped("Click a key, then press the new key (Escape cancels). F1, Escape and Tab (fast-forward) are kept for the overlay.");
    if (ImGui::BeginTable("keys", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Button");
        ImGui::TableSetupColumn("Pad 1", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Pad 2", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableHeadersRow();
        for (int k = 0; k < SB_COUNT; k++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(Settings_ButtonName(k));
            for (int p = 0; p < 2; p++) {
                ImGui::TableNextColumn();
                bool waiting = !sCaptureGamepad && sCapturePad == p && sCaptureButton == k;
                Settings_KeyName(b[p][k], name, sizeof name);
                snprintf(id, sizeof id, "%s##k%d_%d", waiting ? "press a key..." : name, p, k);
                if (ImGui::Button(id, ImVec2(150.0f, 0.0f))) {
                    sCapturePad = p;
                    sCaptureButton = k;
                    sCaptureGamepad = false;
                }
            }
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Default keys")) {
        Settings_DefaultBindings(b);
        Port_WindowSetBindings(b);
        Settings_SaveBindings(b);
    }

    ImGui::SeparatorText("Gamepads");
    for (int p = 0; p < SETTINGS_PADS; p++) {
        const char *n = Port_WindowGamepadName(p);
        ImGui::Text("Pad %d: %s", p + 1, n != nullptr ? n : p < 2 ? "(none: keyboard only)" : "(none)");
    }
    ImGui::TextDisabled("The first four gamepads SDL finds are pads 1-4, in the order they connect. Guide opens this window.");
    for (int p = 0; p < SETTINGS_PADS; p++) {
        snprintf(id, sizeof id, "Pad %d##gp", p + 1);
        if (p > 0) {
            ImGui::SameLine();
        }
        if (ImGui::RadioButton(id, sGamepadPage == p)) {
            sGamepadPage = p;
        }
    }
    ImGui::TextWrapped("Click a button, then press the gamepad's button or trigger (guide or any key cancels).");
    const int gp = sGamepadPage;
    if (ImGui::BeginTable("gamepad", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Button");
        ImGui::TableSetupColumn("Gamepad", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableHeadersRow();
        for (int k = 0; k < SB_GAMEPAD_COUNT; k++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(Settings_ButtonName(k));
            ImGui::TableNextColumn();
            bool waiting = sCaptureGamepad && sCapturePad == gp && sCaptureButton == k;
            snprintf(id, sizeof id, "%s##g%d_%d", waiting ? "press a button..." : Settings_GamepadInputName(g[gp][k]), gp, k);
            if (ImGui::Button(id, ImVec2(190.0f, 0.0f))) {
                sCapturePad = gp;
                sCaptureButton = k;
                sCaptureGamepad = true;
            }
        }
        ImGui::EndTable();
    }
    snprintf(id, sizeof id, "Default buttons (pad %d)", gp + 1);
    if (ImGui::Button(id)) {
        uint8_t d[SETTINGS_PADS][SB_GAMEPAD_COUNT];
        Settings_DefaultGamepad(d);
        memcpy(g[gp], d[gp], sizeof g[gp]);
        Port_WindowSetGamepad(g);
        Settings_SaveGamepad(g);
    }

    ImGui::SeparatorText("Sticks, vibration, multitap");
    slider("Stick dead zone", "input.deadzone", 0, 90, "%d%%");
    check("Left stick works the d-pad", "input.stick_dpad");
    ImGui::TextDisabled("The game itself reads the d-pad only; the sticks also reach it as they are.");
    check("Vibration", "input.vibration");
    check("Multitap: four players (pads 1-4 on port 1A-1D)", "input.multitap");
    ImGui::TextDisabled("With the multitap, pad 2 is slot 1B and port 2 is empty; recordings keep their own setting.");
}

static void page_game(void) {
    check("Movies", "game.movies");
    check("Pause the game while this window is open", "game.pause");
    ImGui::SeparatorText("Game speed");
    speed_choice("Speed (x)", "game.speed", 25);
    speed_choice("Hold Tab (x)", "game.fast_forward", 100);
    ImGui::TextDisabled("Real time only: the game runs the same frames faster or slower; no sound off 1x.");
    ImGui::SeparatorText("Data");
    const char *disc = Port_DiscName(), *dat = Port_DataPath();
    ImGui::TextWrapped("Disc: %s", disc != nullptr && *disc ? disc : "(not opened)");
    ImGui::TextWrapped("Data file: %s", dat != nullptr && *dat ? dat : "(none)");
    static const char *const kData[] = {"USA release (verified)", "[modified game data]",
                                        "USA release so far (TEKKEN.BIN being checked)", "not checked"};
    int st = Port_DataState();
    ImGui::TextWrapped("Game data: %s", st >= 0 && st < 4 ? kData[st] : "?");
    if ((st == 1 || st == 3) && *Port_DataWhy()) {
        ImGui::TextWrapped("(%s)", Port_DataWhy());
    }
    ImGui::SeparatorText("Settings file");
    const char *p = Settings_Path();
    ImGui::TextWrapped("%s", *p ? p : "(none: set TTT_CONFIG or HOME)");
    if (*Settings_SaveError()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "Not saved: %s", Settings_SaveError());
    }
}

/* T40: cheats (plat_cheat.c, docs/port/cheats.md). They change the game's own state. */
static void page_cheats(void) {
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "These modify the game's state.");
    ImGui::TextWrapped("Unlocking sets the game's own unlock flags: every character, TEKKEN BOWL, THEATER (every "
                       "ending) and GALLERY. Your next memory card save keeps them, and turning this off does not lock "
                       "them again. The character select picks the new set at the next game start.");
    check("Unlock everything", "cheats.unlock_all");
}

static void page_about(void) {
    ImGui::Text("Tekken Tag Tournament, native PC port");
    ImGui::Text("Build %s", TTT_GIT);
    if (Port_DataState() == 1) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[modified game data]");
    }
    ImGui::Text("Dear ImGui %s (MIT)", IMGUI_VERSION);
    ImGui::Separator();
    ImGui::TextWrapped("F1: this window. Hold Tab: fast-forward. Escape or the close button: quit (asks first). The game's data comes from your "
                       "own disc; nothing of the game is in this program.");
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
        ImGui::Text("%.1f FPS  %.2f ms  (worst %.2f ms)", avg > 0.0f ? 1000.0f / avg : 0.0f, avg, (float)worst * 1e-6f);
    }
    ImGui::End();
}

static void ui(void) {
    ImGuiIO &io = ImGui::GetIO();
    if (Settings_GetInt("video.fps_meter")) {
        meter();
    }
    if (sSettings) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(640.0f, 520.0f), ImGuiCond_Appearing);
        bool open = true;
        if (ImGui::Begin("Settings (F1)", &open, ImGuiWindowFlags_NoCollapse)) {
            if (Port_DataState() == 1) { /* on every page: the game data in use is not the verified USA release */
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[modified game data]");
                sMarkDrawn++;
            }
            if (ImGui::BeginTabBar("pages")) {
                if (tab("Video", 0)) { page_video(); ImGui::EndTabItem(); }
                if (tab("Audio", 1)) { page_audio(); ImGui::EndTabItem(); }
                if (tab("Controls", 2)) { page_controls(); ImGui::EndTabItem(); }
                if (tab("Game", 3)) { page_game(); ImGui::EndTabItem(); }
                if (tab("Cheats", 4)) { page_cheats(); ImGui::EndTabItem(); }
                if (tab("About", 5)) { page_about(); ImGui::EndTabItem(); }
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
        if (ImGui::Begin("Quit?", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                                               ImGuiWindowFlags_NoMove)) {
            ImGui::Text("Quit Tekken Tag Tournament?");
            ImGui::TextDisabled("Escape again also quits.");
            if (ImGui::Button("Yes, quit", ImVec2(140.0f, 0.0f))) {
                sQuitYes = true;
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(140.0f, 0.0f))) {
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
    if (sMode == 1) {
        ImGui_ImplOpenGL3_NewFrame();
    }
    ImGui::NewFrame();
    ui();
    ImGui::Render();
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
extern "C" void Overlay_TestStep(void) {
    if (sTest < 0 || sMode == 0) {
        return;
    }
    int n = sTest++; /* the window's test (plat_window.c) opens the settings with F1 at its first frame */
    sTestTab = n >= 2 && n < 32 ? (n - 2) / 5 : -1; /* each page for five frames */
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
    for (int i = 0; i < 6; i++) {
        n += sTestPages >> i & 1;
    }
    return n;
}

extern "C" void Overlay_Report(void) {
    if (sTest >= 0) {
        uint16_t b[2][SB_COUNT];
        char name[32];
        Port_WindowGetBindings(b);
        Settings_KeyName(b[0][SB_SQUARE], name, sizeof name);
        printf("ttt: overlay: %d frames drawn (%s), pages %d/6, key capture %d (pad 1 square: %s)\n", sTestFrames,
               sMode == 1 ? "gl" : "sw", pages(), sTestCapture, name);
        printf("ttt: overlay: data mark drawn %u times\n", sMarkDrawn);
        uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT];
        Port_WindowGetGamepad(g);
        printf("ttt: overlay: gamepad capture %d (pad 2 cross: %s)\n", sTestGpCapture,
               Settings_GamepadInputName(g[1][SB_CROSS]));
        fflush(stdout);
    }
}
