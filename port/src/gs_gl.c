/*
 * The GS's OpenGL 3.3 back end (T30 batch 3): the window's drawing on the GPU. The software rasteriser of gs_core.c
 * stays the reference (headless runs, dumps, self-tests); this back end runs on the window thread (plat_window.c,
 * which owns the GL context) when the window is open with TTT_RENDERER=gl (the default when windowed), and carries out
 * the same commands the front end queues for the raster workers, in order:
 *
 *   - GS memory: gs_core.c's 4 MB stay on the CPU for uploads, local copies and CLUT loads, and are mirrored in a
 *     1024 x 1024 RGBA8UI texture (one GS word per texel: word w at (w & 1023, w >> 10)). Every texture read decodes
 *     the GS's swizzled layout in the shader from that mirror (the page tables of gs_core.c as small textures), so any
 *     format, any CLUT trick and any reinterpretation reads what the GS would.
 *   - Render targets: FRAME / ZBUF buffers are linear GL textures (RGBA16F, DEPTH32F) the primitives draw into. A target
 *     is loaded from the mirror when it is created or grows (whole pages), and written back into the mirror (a
 *     "resolve": a draw that scatters its pixels to their GS words) only when something reads its pages: a texture, a
 *     CLUT load, the display, an upload or another target over the same memory. A 24-bit target owns only the low 3
 *     bytes of its words (the game keeps PSMT8H / 4HL / 4HH fonts in the top byte of its frame buffer).
 *   - Per page the back end knows which bytes the CPU wrote and the mirror has not seen yet (pushed before the next
 *     GPU use, with a colour mask when only some bytes changed) and which bytes the GPU wrote that the CPU copy lacks
 *     (read back before the CPU writes or reads those bytes: uploads and local copies into resolved pages; a CLUT load
 *     from them runs on the GPU instead, see clut()).
 *   - Primitives become vertices (integer attributes: 12.4 positions, Z, colour, S / T / Q float bits or U / V, fog)
 *     and a per-primitive state record (texture, CLUT row, wrap, filter, TFX / TCC, fog, alpha test, scissor, blend
 *     selects) in a texture buffer; a draw call covers every primitive until the target or a fixed-function setting
 *     changes (blend equation, depth test / write, colour mask, the alpha test's fail mode). Both buffers are rings
 *     written unsynchronised (T30 batch 4). An alpha test with the same outcome for every pixel of a primitive (its
 *     alpha range: alpha_range) is no test, and a repeated pass on the same vertices shares the depth write of the
 *     first (same_depth): most two-pass batches of batch 3 are gone.
 *   - The shader computes everything the GS does per pixel in integers (texel addressing, wrap modes, CLUT, TEXA,
 *     bilinear with 4-bit fractions, TFX / TCC, fog, alpha test, FBA); the blend ((A - B) * C >> 7) + D is fixed-function
 *     dual-source blending: the shader outputs Cs * |ks| and |kd| for the batch's equation (add, subtract or reverse
 *     subtract). The targets are half floats so that kd above 1 (C = As up to 0xFF: the stages' lighting passes,
 *     Cd * As) is not clamped; values are clamped to 0..255 when written back into GS memory. The alpha test's
 *     FB_ONLY / ZB_ONLY / RGB_ONLY fail modes draw the batch twice with other masks (passing pixels, then failing
 *     ones), so such a batch only holds primitives with the same state record (the GS's own batches): a multi-pass
 *     surface (base, light, add) keeps its order per pixel.
 *   - Repeated work is cached: the CLUT buffers a load gave (by its fields, GS memory's generation and the buffer
 *     before it), the expanded CLUT rows (by content), the pages a target drew since its last resolve (by rows).
 *   - The display: both read circuits merged as PMODE asks (gs_core.c's display_into, as a shader reading the
 *     mirror), into a picture of the display's size; the window shows it letterboxed, TTT_DUMP writes it as PNG.
 *
 *   - Video options (T37, docs/port/video.md): targets at N x N GL pixels per GS pixel (GS memory stays 1x: a resolve
 *     reads each pixel's first sample; a texture that is a colour target in the same format samples the target's
 *     snapshot at full resolution), widescreen margins on 640-wide targets (the full-width draws go on into them, the
 *     HUD's sprites stay 4:3), the game's flicker filter replaced by the frame it copies, low latency (a frame
 *     presented when the game has copied it).
 *
 * Not modelled here (counted, `ttt: gl:` line of the TTT_FRAMES report): the destination alpha test (DATE), C = Ad on
 * a 32 / 16-bit frame (taken as 0x80), partial FBMSK bytes, COLCLAMP 0 (wrap), PABE with a subtracting equation; 16-bit
 * frames keep more than 5 bits per channel and a blend result above 255 is only clamped when written back (the next
 * blend on that pixel sees it unclamped). Colours and depth go through the GPU's float units: pictures are close to
 * the software ones, not identical.
 *
 * No host floating point: everything here is integer; floats exist only inside the shaders, on the GPU. Nothing the
 * game reads comes from this file (the GS writes no game memory; SIGNAL / FINISH / LABEL stay on the game's thread).
 * GL is loaded through SDL_GL_GetProcAddress into the table below (no link against libGL).
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
#include "plat_settings.h"

/* ------------------------------------------------------------------------------------------------ GL */

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef intptr_t GLintptr, GLsizeiptr;
typedef unsigned char GLboolean;
typedef char GLchar;

#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_NEVER 0x0200
#define GL_GREATER 0x0204
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC1_ALPHA 0x8589
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_BLEND 0x0BE2
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_DITHER 0x0BD0
#define GL_SCISSOR_TEST 0x0C11
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_BUFFER 0x8C2A
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_RGBA16F 0x881A
#define GL_RGBA8UI 0x8D7C
#define GL_RGBA32UI 0x8D70
#define GL_R16UI 0x8234
#define GL_RGBA_INTEGER 0x8D99
#define GL_RED_INTEGER 0x8D94
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_COLOR 0x1800
#define GL_NONE 0
#define GL_BACK 0x0405
#define GL_ARRAY_BUFFER 0x8892
#define GL_STREAM_DRAW 0x88E0
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_MAX_TEXTURE_BUFFER_SIZE 0x8C2B
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020

static struct {
    GLenum (*GetError)(void);
    const unsigned char *(*GetString)(GLenum);
    void (*Enable)(GLenum);
    void (*Disable)(GLenum);
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
    void (*ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
    void (*DepthMask)(GLboolean);
    void (*DepthFunc)(GLenum);
    void (*BlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum);
    void (*BlendEquationSeparate)(GLenum, GLenum);
    void (*ClearBufferfv)(GLenum, GLint, const void *);
    void (*GenTextures)(GLsizei, GLuint *);
    void (*DeleteTextures)(GLsizei, const GLuint *);
    void (*BindTexture)(GLenum, GLuint);
    void (*ActiveTexture)(GLenum);
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
    void (*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
    void (*TexParameteri)(GLenum, GLenum, GLint);
    void (*TexBuffer)(GLenum, GLenum, GLuint);
    void (*PixelStorei)(GLenum, GLint);
    void (*GenFramebuffers)(GLsizei, GLuint *);
    void (*DeleteFramebuffers)(GLsizei, const GLuint *);
    void (*BindFramebuffer)(GLenum, GLuint);
    void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    GLenum (*CheckFramebufferStatus)(GLenum);
    void (*DrawBuffer)(GLenum);
    void (*ReadBuffer)(GLenum);
    void (*BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
    void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
    void (*GenBuffers)(GLsizei, GLuint *);
    void (*BindBuffer)(GLenum, GLuint);
    void (*BufferData)(GLenum, GLsizeiptr, const void *, GLenum);
    void (*BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void *);
    void (*GenVertexArrays)(GLsizei, GLuint *);
    void (*BindVertexArray)(GLuint);
    void (*EnableVertexAttribArray)(GLuint);
    void (*VertexAttribIPointer)(GLuint, GLint, GLenum, GLsizei, const void *);
    GLuint (*CreateShader)(GLenum);
    void (*ShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
    void (*CompileShader)(GLuint);
    void (*GetShaderiv)(GLuint, GLenum, GLint *);
    void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    GLuint (*CreateProgram)(void);
    void (*AttachShader)(GLuint, GLuint);
    void (*LinkProgram)(GLuint);
    void (*GetProgramiv)(GLuint, GLenum, GLint *);
    void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void (*UseProgram)(GLuint);
    GLint (*GetUniformLocation)(GLuint, const GLchar *);
    void (*Uniform1i)(GLint, GLint);
    void (*Uniform2i)(GLint, GLint, GLint);
    void (*Uniform4i)(GLint, GLint, GLint, GLint, GLint);
    void (*Uniform1ui)(GLint, GLuint);
    void (*Uniform4iv)(GLint, GLsizei, const GLint *);
    void (*Uniform4uiv)(GLint, GLsizei, const GLuint *);
    void (*DrawArrays)(GLenum, GLint, GLsizei);
    void (*Finish)(void);
    void *(*MapBufferRange)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
    GLboolean (*UnmapBuffer)(GLenum);
    void (*GetIntegerv)(GLenum, GLint *);
} gl;

static const struct {
    const char *name;
    size_t offset;
} kGlFuncs[] = {
#define F(n) {"gl" #n, offsetof(__typeof__(gl), n)}
    F(GetError), F(GetString), F(Enable), F(Disable), F(Viewport), F(ColorMask), F(DepthMask), F(DepthFunc),
    F(BlendFuncSeparate), F(BlendEquationSeparate), F(ClearBufferfv), F(GenTextures), F(DeleteTextures), F(BindTexture),
    F(ActiveTexture), F(TexImage2D), F(TexSubImage2D), F(TexParameteri), F(TexBuffer), F(PixelStorei), F(GenFramebuffers),
    F(DeleteFramebuffers), F(BindFramebuffer), F(FramebufferTexture2D), F(CheckFramebufferStatus), F(DrawBuffer),
    F(ReadBuffer), F(BlitFramebuffer), F(ReadPixels), F(GenBuffers), F(BindBuffer), F(BufferData), F(BufferSubData),
    F(GenVertexArrays), F(BindVertexArray), F(EnableVertexAttribArray), F(VertexAttribIPointer), F(CreateShader),
    F(ShaderSource), F(CompileShader), F(GetShaderiv), F(GetShaderInfoLog), F(CreateProgram), F(AttachShader),
    F(LinkProgram), F(GetProgramiv), F(GetProgramInfoLog), F(UseProgram), F(GetUniformLocation), F(Uniform1i),
    F(Uniform2i), F(Uniform4i), F(Uniform1ui), F(Uniform4iv), F(Uniform4uiv), F(DrawArrays), F(Finish),
    F(MapBufferRange), F(UnmapBuffer), F(GetIntegerv),
#undef F
};

/* ------------------------------------------------------------------------------------------------ shaders */

/* Texture units */
enum { U_VRAM, U_TAB, U_INV, U_PAL, U_STATES, U_SRC, U_STAGE, U_CLUT, U_SNAP, U_DSP0, U_DSP1 };

/* GS memory in the shader: the mirror and the page tables (gs_core.c's sIn32 / sIn16 / sIn16S / sIn8 / sIn4 at rows
   0 / 32 / 96 / 160 / 224 of a 128-wide R16UI texture), gs_read() = gs_core.c's vram_loc + vloc_read. */
static const char kLib[] =
    "uniform usampler2D uVram;\n"
    "uniform usampler2D uTab;\n"
    "uint vword(uint w) {\n"
    "    uvec4 b = texelFetch(uVram, ivec2(int(w & 1023u), int((w >> 10) & 1023u)), 0);\n"
    "    return b.r | (b.g << 8) | (b.b << 16) | (b.a << 24);\n"
    "}\n"
    "uint tab(uint row, uint x, uint y) { return texelFetch(uTab, ivec2(int(x), int(row + y)), 0).r; }\n"
    "uint psm_bits(uint psm) {\n"
    "    if ((psm & 0x30u) == 0x30u) psm &= 0x0Fu;\n"
    "    if (psm == 0x01u) return 24u;\n"
    "    if (psm == 0x02u || psm == 0x0Au) return 16u;\n"
    "    if (psm == 0x13u || psm == 0x1Bu) return 8u;\n"
    "    if (psm == 0x14u || psm == 0x24u || psm == 0x2Cu) return 4u;\n"
    "    return 32u;\n"
    "}\n"
    "uint gs_read(uint bp, uint bw, uint psm, uint x, uint y) {\n"
    "    uint zx = 0u;\n"
    "    if (bw == 0u) bw = 1u;\n"
    "    if ((psm & 0x30u) == 0x30u) { zx = 0x18u; psm &= 0x0Fu; }\n"
    "    x &= 0x7FFu; y &= 0x7FFu;\n"
    "    if (psm == 0x02u || psm == 0x0Au) {\n"
    "        uint a = ((bp + (y >> 6) * 32u * bw + (x >> 6) * 32u) << 7) + (tab(psm == 0x0Au ? 96u : 32u, x & 63u, y & 63u) ^ (zx << 7));\n"
    "        return (vword((a >> 1) & 0xFFFFFu) >> ((a & 1u) * 16u)) & 0xFFFFu;\n"
    "    }\n"
    "    if (psm == 0x13u) {\n"
    "        uint a = ((bp + (y >> 6) * 32u * (bw >> 1) + (x >> 7) * 32u) << 8) + tab(160u, x & 127u, y & 63u);\n"
    "        return (vword((a >> 2) & 0xFFFFFu) >> ((a & 3u) * 8u)) & 0xFFu;\n"
    "    }\n"
    "    if (psm == 0x14u) {\n"
    "        uint a = ((bp + (y >> 7) * 32u * (bw >> 1) + (x >> 7) * 32u) << 9) + tab(224u, x & 127u, y & 127u);\n"
    "        return (vword((a >> 3) & 0xFFFFFu) >> ((a & 7u) * 4u)) & 0xFu;\n"
    "    }\n"
    "    uint w = vword((((bp + (y >> 5) * 32u * bw + (x >> 6) * 32u) << 6) + (tab(0u, x & 63u, y & 31u) ^ (zx << 6))) & 0xFFFFFu);\n"
    "    if (psm == 0x01u) return w & 0xFFFFFFu;\n"
    "    if (psm == 0x1Bu) return w >> 24;\n"
    "    if (psm == 0x24u) return (w >> 24) & 15u;\n"
    "    if (psm == 0x2Cu) return w >> 28;\n"
    "    return w;\n"
    "}\n"
    "uvec4 unpack32(uint c) { return uvec4(c & 255u, (c >> 8) & 255u, (c >> 16) & 255u, c >> 24); }\n"
    /* gs_core.c's expand(): a stored colour of 32, 24 or 16 bits with TEXA's alpha */
    "uvec4 expandc(uint c, uint bits, uint ta0, uint ta1, bool aem) {\n"
    "    if (bits == 32u) return unpack32(c);\n"
    "    if (bits == 24u) { c &= 0xFFFFFFu; return uvec4(unpack32(c).rgb, (aem && c == 0u) ? 0u : ta0); }\n"
    "    uint a = (c & 0x8000u) != 0u ? ta1 : ((aem && (c & 0x7FFFu) == 0u) ? 0u : ta0);\n"
    "    return uvec4((c & 31u) << 3, ((c >> 5) & 31u) << 3, ((c >> 10) & 31u) << 3, a);\n"
    "}\n";

/* The primitives. State record (3 uvec4 per primitive, see state_of()):
   s0.x flags: 0 tex, 1 fst, 2-3 tfx, 4 tcc, 5 gouraud, 6 fog, 7-8 filter, 9 fba, 10 atest, 11-13 atst, 14 lcm,
               15-16 lodl, 17 mmag linear, 18 mmin linear, 19 aem, 20 pabe, 21 blend, 22-29 A / B / C / D selects,
               30 the frame has no alpha (C = Ad reads 0x80)
   s0.y tbp | tbw << 14 | tpsm << 20 | lw << 26     s0.z lh | wms << 4 | wmt << 6 | CLUT row << 8 | aref << 24
   s0.w minu | maxu << 10 | lodk << 20               s1.x minv | maxv << 10 | afix << 20
   s1.y ta0 | ta1 << 8                               s1.z scissor x0 | x1 << 16   s1.w scissor y0 | y1 << 16
   s2.x fog colour */
static const char kDrawVs[] =
    "#version 330 core\n"
    "layout(location = 0) in ivec2 aXY;\n"
    "layout(location = 1) in uint aZ;\n"
    "layout(location = 2) in uint aCol;\n"
    "layout(location = 3) in uvec3 aSTQ;\n"
    "layout(location = 4) in uint aFog;\n"
    "layout(location = 5) in uint aState;\n"
    "uniform usamplerBuffer uStates;\n"
    "uniform ivec2 uSize;\n"  /* the target in GS pixels, margins included */
    "uniform int uScale;\n"   /* GL pixels per GS pixel (T37) */
    "uniform int uMarg;\n"    /* the target's left margin in GS pixels (widescreen, T37) */
    "uniform int uZBits;\n"
    "uniform uint uZMax;\n"
    "noperspective out vec4 vCol;\n"
    "flat out uint vColF;\n"
    "noperspective out vec3 vTex;\n"
    "noperspective out float vFog;\n"
    "flat out uint vFogF;\n"
    "flat out uint vState;\n"
    "float ee_float(uint b) {\n" /* EE floats: exponent 0 is zero, exponent 255 a number */
    "    if ((b & 0x7F800000u) == 0u) return 0.0;\n"
    "    if ((b & 0x7F800000u) == 0x7F800000u) b = (b & 0x80000000u) | 0x7F7FFFFFu;\n"
    "    return uintBitsToFloat(b);\n"
    "}\n"
    "void main() {\n"
    "    uint fl = texelFetch(uStates, int(aState) * 3).x;\n"
    /* the GS's pixel (x, y) at the centre of GL's pixel (x, y); scaled, at the centre of the first of its S x S pixels,
       so that the others sample inside it (a UV sprite never reaches the texel before its first: no seams) */
    "    vec2 p = vec2(aXY) * 0.0625 + vec2(float(uMarg), 0.0) + 0.5 / float(uScale);\n"
    "    float z = uZBits > 0 ? float(min(aZ, uZMax)) * exp2(float(1 - uZBits)) - 1.0 : 0.0;\n"
    "    gl_Position = vec4(p * 2.0 / vec2(uSize) - 1.0, z, 1.0);\n"
    "    vCol = vec4(float(aCol & 255u), float((aCol >> 8) & 255u), float((aCol >> 16) & 255u), float(aCol >> 24));\n"
    "    vColF = aCol;\n"
    "    vTex = (fl & 2u) != 0u ? vec3(float(int(aSTQ.x)), float(int(aSTQ.y)), 1.0) : vec3(ee_float(aSTQ.x), ee_float(aSTQ.y), ee_float(aSTQ.z));\n"
    "    vFog = float(aFog);\n"
    "    vFogF = aFog;\n"
    "    vState = aState;\n"
    "}\n";

static const char kDrawFs[] =
    "uniform usamplerBuffer uStates;\n"
    "uniform usampler2D uPal;\n"
    "uniform sampler2D uSnap;\n" /* the snapshot of the target the texture is (scaled / widescreen, T37) */
    "uniform int uScale;\n"
    "uniform int uPass;\n"
    "noperspective in vec4 vCol;\n"
    "flat in uint vColF;\n"
    "noperspective in vec3 vTex;\n"
    "noperspective in float vFog;\n"
    "flat in uint vFogF;\n"
    "flat in uint vState;\n"
    "layout(location = 0, index = 0) out vec4 oCol;\n"
    "layout(location = 0, index = 1) out vec4 oFac;\n"
    "uvec4 s0, s1, s2;\n"
    "int wrapc(int u, uint wm, int size, int mn, int mx) {\n"
    "    if (wm == 0u) return u & (size - 1);\n"
    "    if (wm == 1u) return u < 0 ? 0 : (u >= size ? size - 1 : u);\n"
    "    if (wm == 2u) return u < mn ? mn : (u > mx ? mx : u);\n"
    "    return (u & mn) | mx;\n"
    "}\n"
    "uvec4 texel(int u, int v) {\n"
    "    uint tpsm = (s0.y >> 20) & 63u, bits = psm_bits(tpsm), lw = (s0.y >> 26) & 15u, lh = s0.z & 15u;\n"
    "    int tw = 1 << min(lw, 10u), th = 1 << min(lh, 10u);\n"
    "    u = wrapc(u, (s0.z >> 4) & 3u, tw, int(s0.w & 1023u), int((s0.w >> 10) & 1023u));\n"
    "    v = wrapc(v, (s0.z >> 6) & 3u, th, int(s1.x & 1023u), int((s1.x >> 10) & 1023u));\n"
    "    uint c = gs_read(s0.y & 0x3FFFu, (s0.y >> 14) & 63u, tpsm, uint(u), uint(v));\n"
    "    if (bits <= 8u) return texelFetch(uPal, ivec2(int(c), int((s0.z >> 8) & 0xFFFFu)), 0);\n"
    "    return expandc(c, bits, s1.y & 255u, (s1.y >> 8) & 255u, (s0.x & (1u << 19)) != 0u);\n"
    "}\n"
    /* a texel of a target's snapshot at full resolution: (hu, hv) in its GL pixels, its margin in s2.y; the wrap mode
       on the GS texel, the left margin unwrapped; stored as the GS would (16 bits: 5 per channel), then TEXA */
    "int fdiv(int a, int b) { return a >= 0 ? a / b : -((b - 1 - a) / b); }\n"
    "uvec4 snap_texel(int hu, int hv) {\n"
    "    int S = uScale, m = int(s2.y), u = fdiv(hu, S), v = fdiv(hv, S), su = hu - u * S, sv = hv - v * S;\n"
    "    uint tpsm = (s0.y >> 20) & 63u, bits = psm_bits(tpsm), lw = (s0.y >> 26) & 15u, lh = s0.z & 15u;\n"
    "    int tw = 1 << min(lw, 10u), th = 1 << min(lh, 10u);\n"
    "    if (u >= 0 || u < -m) u = wrapc(u, (s0.z >> 4) & 3u, tw, int(s0.w & 1023u), int((s0.w >> 10) & 1023u));\n"
    "    v = wrapc(v, (s0.z >> 6) & 3u, th, int(s1.x & 1023u), int((s1.x >> 10) & 1023u));\n"
    "    ivec2 ts = textureSize(uSnap, 0);\n"
    "    vec4 f = texelFetch(uSnap, clamp(ivec2((u + m) * S + su, v * S + sv), ivec2(0), ts - 1), 0);\n"
    "    uvec4 b = uvec4(clamp(f, 0.0, 1.0) * 255.0 + 0.5);\n"
    "    uint ta0 = s1.y & 255u, ta1 = (s1.y >> 8) & 255u;\n"
    "    bool aem = (s0.x & (1u << 19)) != 0u;\n"
    "    if (bits == 16u) return expandc((b.r >> 3) | ((b.g >> 3) << 5) | ((b.b >> 3) << 10) | (b.a >= 128u ? 0x8000u : 0u), 16u, ta0, ta1, aem);\n"
    "    return expandc(b.r | (b.g << 8) | (b.b << 16) | (b.a << 24), bits, ta0, ta1, aem);\n"
    "}\n"
    "uvec4 tap(bool snap, int u, int v) { return snap ? snap_texel(u, v) : texel(u, v); }\n"
    "uvec4 sample_tex(int u16, int v16, float q) {\n"
    "    uint fl = s0.x, filter = (fl >> 7) & 3u;\n"
    "    bool linear = filter == 1u, snap = (fl & 0x80000000u) != 0u;\n"
    "    if (filter == 2u) {\n"
    "        int lodk = int(s0.w >> 20); if (lodk >= 2048) lodk -= 4096;\n"
    "        int lod = (fl & (1u << 14)) != 0u ? lodk : (int(floor(-log2(max(abs(q), 1e-30)) * 16.0)) << ((fl >> 15) & 3u)) + lodk;\n"
    "        linear = lod <= 0 ? (fl & (1u << 17)) != 0u : (fl & (1u << 18)) != 0u;\n"
    "    }\n"
    "    if (snap) { u16 = clamp(u16, -(1 << 24), 1 << 24) * uScale; v16 = clamp(v16, -(1 << 24), 1 << 24) * uScale; }\n" /* 1/16 of its GL pixels */
    "    if (!linear) return tap(snap, u16 >> 4, v16 >> 4);\n"
    "    u16 -= 8; v16 -= 8;\n"
    "    int u0 = u16 >> 4, v0 = v16 >> 4;\n"
    "    uint fu = uint(u16 & 15), fv = uint(v16 & 15);\n"
    "    uvec4 c00 = tap(snap, u0, v0);\n"
    "    if (fu == 0u && fv == 0u) return c00;\n"
    "    uvec4 c10 = tap(snap, u0 + 1, v0), c01 = tap(snap, u0, v0 + 1), c11 = tap(snap, u0 + 1, v0 + 1);\n"
    "    return (c00 * ((16u - fu) * (16u - fv)) + c10 * (fu * (16u - fv)) + c01 * ((16u - fu) * fv) + c11 * (fu * fv)) >> 8;\n"
    "}\n"
    "int stq(float s, float q, uint lsize) {\n"
    "    float r = trunc(s / (q == 0.0 ? 1e-30 : q) * float(16 << lsize));\n"
    "    return int(clamp(r, -1073741823.0, 1073741823.0));\n"
    "}\n"
    "void main() {\n"
    "    int st = int(vState) * 3;\n"
    "    s0 = texelFetch(uStates, st); s1 = texelFetch(uStates, st + 1); s2 = texelFetch(uStates, st + 2);\n"
    "    uint fl = s0.x;\n"
    "    ivec2 pix = ivec2(gl_FragCoord.xy) / uScale;\n" /* the GS pixel, margin included (so is the scissor) */
    "    if (pix.x < int(s1.z & 0xFFFFu) || pix.x > int(s1.z >> 16) || pix.y < int(s1.w & 0xFFFFu) || pix.y > int(s1.w >> 16)) discard;\n"
    "    bool gouraud = (fl & 32u) != 0u;\n"
    "    uvec4 c = gouraud ? uvec4(clamp(vCol, 0.0, 255.0)) : unpack32(vColF);\n"
    "    if ((fl & 1u) != 0u) {\n"
    "        int u16, v16;\n"
    "        if ((fl & 2u) != 0u) { u16 = int(floor(vTex.x)); v16 = int(floor(vTex.y)); }\n"
    "        else { u16 = stq(vTex.x, vTex.z, (s0.y >> 26) & 15u); v16 = stq(vTex.y, vTex.z, s0.z & 15u); }\n"
    "        uvec4 tc = sample_tex(u16, v16, vTex.z);\n"
    "        uint tfx = (fl >> 2) & 3u;\n"
    "        bool tcc = (fl & 16u) != 0u;\n"
    "        uvec3 rgb = tfx == 0u ? (tc.rgb * c.rgb) >> 7 : (tfx == 1u ? tc.rgb : ((tc.rgb * c.rgb) >> 7) + c.a);\n"
    "        uint a = !tcc ? c.a : (tfx == 0u ? (tc.a * c.a) >> 7 : (tfx == 2u ? tc.a + c.a : tc.a));\n"
    "        c = min(uvec4(rgb, a), uvec4(255u));\n"
    "    }\n"
    "    if ((fl & 64u) != 0u) {\n"
    "        uint f = gouraud ? uint(clamp(vFog, 0.0, 255.0)) : min(vFogF, 255u);\n"
    "        uvec3 fc = unpack32(s2.x).rgb;\n"
    "        c.rgb = (c.rgb * f + fc * (255u - f)) >> 8;\n"
    "    }\n"
    "    uint sa = c.a;\n"
    "    if ((fl & 1024u) != 0u && uPass != 0) {\n"
    "        uint atst = (fl >> 11) & 7u, aref = s0.z >> 24;\n"
    "        bool pass = atst == 1u || (atst == 2u && sa < aref) || (atst == 3u && sa <= aref) || (atst == 4u && sa == aref) ||\n"
    "                    (atst == 5u && sa >= aref) || (atst == 6u && sa > aref) || (atst == 7u && sa != aref);\n"
    "        if (pass == (uPass == 2)) discard;\n"
    "    }\n"
    "    float ks = 1.0, kd = 0.0;\n"
    "    if ((fl & (1u << 21)) != 0u && !((fl & (1u << 20)) != 0u && sa < 128u)) {\n"
    "        uint as_ = (fl >> 22) & 3u, bs = (fl >> 24) & 3u, cs = (fl >> 26) & 3u, ds = (fl >> 28) & 3u;\n"
    "        float cc = (cs == 0u ? float(sa) : (cs == 1u ? 128.0 : float((s1.x >> 20) & 255u))) / 128.0;\n"
    "        ks = abs((as_ == 0u ? cc : 0.0) - (bs == 0u ? cc : 0.0) + (ds == 0u ? 1.0 : 0.0));\n"
    "        kd = abs((as_ == 1u ? cc : 0.0) - (bs == 1u ? cc : 0.0) + (ds == 1u ? 1.0 : 0.0));\n"
    "    }\n"
    "    oCol = vec4(min(vec3(c.rgb) * ks, 255.0) / 255.0, float((fl & 512u) != 0u ? sa | 128u : sa) / 255.0);\n"
    "    oFac = vec4(0.0, 0.0, 0.0, kd);\n"
    "}\n";

/* A rectangle of a target, for the passes below: (x0, y0)-(x1, y1) in pixels of a uTarget-sized target. */
static const char kRectVs[] =
    "#version 330 core\n"
    "uniform ivec4 uRect;\n"
    "uniform ivec2 uTarget;\n"
    "void main() {\n"
    "    int i = gl_VertexID;\n"
    "    vec2 p = vec2((i & 1) != 0 ? uRect.z : uRect.x, i >= 2 ? uRect.w : uRect.y);\n"
    "    gl_Position = vec4(p / vec2(uTarget) * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

/* Resolve: a target's pixels into their GS words (drawn over the mirror's rows of the target's pages). The inverse page
   tables give the pixel of each word (32 bits) or half word (16 bits): rows 0 / 32 / 96 of a 64-wide R16UI texture,
   x | y << 8. */
static const char kResolveFs[] =
    "#version 330 core\n"
    "uniform sampler2D uSrc;\n"
    "uniform usampler2D uInv;\n"
    "uniform uvec4 uBuf;\n" /* bp (blocks, page aligned), bw, psm, 1 = depth */
    "uniform ivec2 uSM;\n"  /* the target's scale and margin: a GS pixel is its first GL pixel (as the GS samples it) */
    "out uvec4 oWord;\n"
    "uint inv(uint row, uint e) { return texelFetch(uInv, ivec2(int(e & 63u), int(row + (e >> 6))), 0).r; }\n"
    "uint px32(ivec2 p, uint psm) {\n"
    "    p = (p + ivec2(uSM.y, 0)) * uSM.x;\n"
    "    if (uBuf.w != 0u) {\n"
    "        float d = texelFetch(uSrc, p, 0).r;\n"
    "        if (psm == 0x00u) return uint(min(d * 4294967296.0, 4294967295.0));\n"
    "        if (psm == 0x01u) return uint(min(d * 16777216.0 + 0.5, 16777215.0));\n"
    "        return uint(min(d * 65536.0 + 0.5, 65535.0));\n"
    "    }\n"
    "    uvec4 b = uvec4(clamp(texelFetch(uSrc, p, 0), 0.0, 1.0) * 255.0 + 0.5);\n"
    "    if (psm == 0x02u || psm == 0x0Au) return (b.r >> 3) | ((b.g >> 3) << 5) | ((b.b >> 3) << 10) | (b.a >= 128u ? 0x8000u : 0u);\n"
    "    return b.r | (b.g << 8) | (b.b << 16) | (b.a << 24);\n"
    "}\n"
    "void main() {\n"
    "    uint w = uint(gl_FragCoord.y) * 1024u + uint(gl_FragCoord.x);\n"
    "    uint page = (w >> 11) - (uBuf.x >> 5), bw = max(uBuf.y, 1u);\n"
    "    uint ppx = page % bw, ppy = page / bw, o = w & 2047u, psm = uBuf.z & 0x0Fu, zx = uBuf.w != 0u ? 0x18u : 0u, word;\n"
    "    if (psm == 0x02u || psm == 0x0Au) {\n"
    "        uint row = psm == 0x0Au ? 96u : 32u;\n"
    "        uint e0 = inv(row, (o * 2u) ^ (zx << 7)), e1 = inv(row, (o * 2u + 1u) ^ (zx << 7));\n"
    "        word = px32(ivec2(int(ppx * 64u + (e0 & 255u)), int(ppy * 64u + (e0 >> 8))), psm) |\n"
    "               (px32(ivec2(int(ppx * 64u + (e1 & 255u)), int(ppy * 64u + (e1 >> 8))), psm) << 16);\n"
    "    } else {\n"
    "        uint e = inv(0u, o ^ (zx << 6));\n"
    "        word = px32(ivec2(int(ppx * 64u + (e & 255u)), int(ppy * 32u + (e >> 8))), psm);\n"
    "    }\n"
    "    oWord = uvec4(word & 255u, (word >> 8) & 255u, (word >> 16) & 255u, word >> 24);\n"
    "}\n";

/* Load: a target's pixels from GS memory (the mirror). */
static const char kLoadFs[] =
    "uniform uvec4 uBuf;\n" /* bp, bw, psm, depth bits (0: colour) */
    "uniform ivec2 uSM;\n"  /* scale, margin: the margins load black (depth 0) */
    "out vec4 oCol;\n"
    "void main() {\n"
    "    ivec2 q = ivec2(gl_FragCoord.xy) / uSM.x - ivec2(uSM.y, 0);\n"
    "    if (q.x < 0 || q.x >= int(max(uBuf.y, 1u)) * 64) { oCol = vec4(0.0); gl_FragDepth = 0.0; return; }\n"
    "    uvec2 p = uvec2(q);\n"
    "    uint v = gs_read(uBuf.x, uBuf.y, uBuf.z, p.x, p.y), bits = psm_bits(uBuf.z);\n"
    "    if (uBuf.w != 0u) { gl_FragDepth = float(v) * exp2(-float(uBuf.w)); oCol = vec4(0.0); return; }\n"
    "    uvec4 c = bits == 16u ? uvec4((v & 31u) << 3, ((v >> 5) & 31u) << 3, ((v >> 10) & 31u) << 3, (v & 0x8000u) != 0u ? 128u : 0u)\n"
    "                          : unpack32(bits == 24u ? v & 0xFFFFFFu : v);\n"
    "    oCol = vec4(c) / 255.0;\n"
    "    gl_FragDepth = gl_FragCoord.z;\n"
    "}\n";

/* Staged copy into the mirror (only some bytes of the words: the colour mask). */
static const char kStageFs[] =
    "#version 330 core\n"
    "uniform usampler2D uStage;\n"
    "out uvec4 oWord;\n"
    "void main() { oWord = texelFetch(uStage, ivec2(gl_FragCoord.xy), 0); }\n";

/* The CLUT buffer on the GPU, for CLUT loads from pages the GPU drew (T30 batch 4: no read back). A row of a 512-wide
   R16UI texture holds the whole buffer (gs_core.c's clut[512] halves); the halves the CPU copy knows come as uniforms,
   the others from the previous GPU row. */
static const char kClutLib[] =
    "uniform usampler2D uClut;\n"
    "uniform int uRow;\n"       /* the GPU row with the halves the CPU copy lacks (-1: none) */
    "uniform uvec4 uCpu[64];\n" /* the CPU copy: two halves per word */
    "uniform uvec4 uUnk[4];\n"  /* bit h: half h only on the GPU */
    "uint half_(uint h) {\n"
    "    if (uRow >= 0 && ((uUnk[h >> 7][(h >> 5) & 3u] >> (h & 31u)) & 1u) != 0u) return texelFetch(uClut, ivec2(int(h), uRow), 0).r;\n"
    "    uint w = uCpu[h >> 3][(h >> 1) & 3u];\n"
    "    return (h & 1u) != 0u ? w >> 16 : w & 0xFFFFu;\n"
    "}\n";

/* A CLUT load (gs_core.c's clut_fill) into a new row: the halves it writes from the mirror, the others kept. */
static const char kClutLoadFs[] =
    "uniform uvec4 uLd;\n" /* cbp, cpsm, entries | csa << 16 | csm << 31, TEXCLUT: cbw | cou << 6 | cov << 12 */
    "out uvec4 oHalf;\n"
    "void main() {\n"
    "    uint h = uint(gl_FragCoord.x), cpsm = uLd.y, n = uLd.z & 0xFFFFu, csa = (uLd.z >> 16) & 31u, i = 512u;\n"
    "    bool c32 = cpsm == 0u || cpsm == 1u;\n"
    "    uint base = n == 256u ? 0u : (c32 ? (csa * 16u) & 255u : csa * 16u), e = c32 ? h & 255u : h;\n"
    "    if (e >= base && e < base + n) i = e - base;\n"
    "    if (i == 512u) { oHalf = uvec4(half_(h), 0u, 0u, 0u); return; }\n"
    "    uint x, y, bw = 1u;\n"
    "    if ((uLd.z >> 31) != 0u) { bw = uLd.w & 63u; x = ((uLd.w >> 6) & 63u) * 16u + i; y = (uLd.w >> 12) & 1023u; }\n"
    "    else if (n == 256u) { uint k = (i & 0xE7u) | ((i & 8u) << 1) | ((i & 0x10u) >> 1); x = k & 15u; y = k >> 4; }\n"
    "    else { x = i & 7u; y = i >> 3; }\n"
    "    uint c = gs_read(uLd.x, bw, c32 ? 0u : cpsm, x, y);\n"
    "    oHalf = uvec4(c32 && h >= 256u ? c >> 16 : c & 0xFFFFu, 0u, 0u, 0u);\n"
    "}\n";

/* A CLUT row for the draws (pal_row) from the buffer: gs_core.c's clut_read through TEXA. */
static const char kClutExpandFs[] =
    "uniform uvec4 uEx;\n" /* cpsm, csa, entries, TEXA: ta0 | ta1 << 8 | aem << 16 */
    "out uvec4 oCol;\n"
    "void main() {\n"
    "    uint i = uint(gl_FragCoord.x), cpsm = uEx.x, e = uEx.z == 256u ? i : uEx.y * 16u + i;\n"
    "    if (i >= uEx.z) { oCol = uvec4(0u); return; }\n"
    "    if (cpsm == 0u || cpsm == 1u) { e &= 255u; oCol = unpack32(half_(e) | (half_(e + 256u) << 16)); return; }\n"
    "    oCol = expandc(half_(e & 511u), 16u, uEx.w & 255u, (uEx.w >> 8) & 255u, ((uEx.w >> 16) & 1u) != 0u);\n"
    "}\n";

/* The display: gs_core.c's display_into() on the mirror, or (T37) on a target or a target's snapshot at full resolution.
   The picture is (width + 2 margins) x lines GS pixels, uOut.x GL pixels each; the margins are black unless a circuit
   reads a target whose margins were drawn (widescreen). */
static const char kDisplayFs[] =
    "uniform uvec4 uCa[2];\n" /* per circuit: fbp, fbw, psm, dbx | dby << 16 */
    "uniform ivec4 uCb[2];\n" /* per circuit: x0, y0, width, lines */
    "uniform uvec4 uMode;\n"  /* PMODE, BGCOLOR, lines per buffer line, 0 */
    "uniform ivec4 uMain;\n"  /* the main circuit's x0, y0, width */
    "uniform ivec4 uTex[2];\n" /* per circuit: 1 = from uDsp0 / uDsp1 (a target), its margin, margins drawn, lines per buffer line */
    "uniform ivec2 uOut;\n"   /* scale, the picture's margin */
    "uniform sampler2D uDsp0;\n"
    "uniform sampler2D uDsp1;\n"
    "out vec4 oCol;\n"
    "uvec4 cpix(int k, int x, int r, ivec2 f, out bool inn) {\n"
    "    int m = uTex[k].x != 0 && uTex[k].z != 0 ? uTex[k].y : 0;\n"
    "    uint bits = psm_bits(uCa[k].z);\n"
    "    inn = !(x < -m || r < 0 || x >= uCb[k].z + m || r >= uCb[k].w);\n"
    "    if (!inn) return uvec4(0u);\n"
    "    if (uTex[k].x != 0) {\n"
    "        ivec2 p = ivec2((int(uCa[k].w & 0xFFFFu) + x + uTex[k].y) * uOut.x + f.x,\n"
    "                        int(uCa[k].w >> 16) * uOut.x + (r * uOut.x + f.y) / uTex[k].w);\n"
    "        uvec4 b = uvec4(clamp(k == 0 ? texelFetch(uDsp0, p, 0) : texelFetch(uDsp1, p, 0), 0.0, 1.0) * 255.0 + 0.5);\n"
    "        if (bits == 16u) return uvec4(b.rgb & 0xF8u, b.a >= 128u ? 128u : 0u);\n"
    "        return bits == 24u ? uvec4(b.rgb, 128u) : b;\n"
    "    }\n"
    "    uint v = gs_read(uCa[k].x, uCa[k].y, uCa[k].z, (uCa[k].w & 0xFFFFu) + uint(x), (uCa[k].w >> 16) + uint(r) / uMode.z);\n"
    "    if (bits == 16u) return uvec4((v & 31u) << 3, ((v >> 5) & 31u) << 3, ((v >> 10) & 31u) << 3, (v & 0x8000u) != 0u ? 128u : 0u);\n"
    "    if (bits == 24u) return uvec4(unpack32(v).rgb, 128u);\n"
    "    return unpack32(v);\n"
    "}\n"
    "void main() {\n"
    "    int S = uOut.x, X = int(gl_FragCoord.x), Y = int(gl_FragCoord.y), r = Y / S, x = X / S - uOut.y;\n"
    "    ivec2 f = ivec2(X - (X / S) * S, Y - r * S);\n"
    "    int ax = x + uMain.x, ar = r + uMain.y;\n"
    "    uint pm = uMode.x;\n"
    "    bool en1 = (pm & 1u) != 0u, en2 = (pm & 2u) != 0u, mmod = (pm & 32u) != 0u, slbg = (pm & 128u) != 0u, inn;\n"
    "    uvec3 base = x < 0 || x >= uMain.z ? uvec3(0u) : unpack32(uMode.y).rgb, o;\n"
    "    if (en2 && !slbg) { uvec4 p = cpix(1, ax - uCb[1].x, ar - uCb[1].y, f, inn); if (inn) base = p.rgb; }\n"
    "    o = base;\n"
    "    if (en1) {\n"
    "        uvec4 p = cpix(0, ax - uCb[0].x, ar - uCb[0].y, f, inn);\n"
    "        if (inn) { uint a = mmod ? (pm >> 8) & 255u : min(p.a * 2u, 255u); o = (p.rgb * a + base * (255u - a) + 127u) / 255u; }\n"
    "    }\n"
    "    oCol = vec4(vec3(o) / 255.0, 1.0);\n"
    "}\n";

/* ------------------------------------------------------------------------------------------------ state */

#define MAX_RT 16
#define RT_ROWS 1024 /* rows of a target at most (the GS addresses 2048; the game draws within 1024) */
#define PAL_ROWS 2048
#define MAX_VERTS (1 << 18)   /* per draw-call flush */
#define MAX_STATES 32768
#define VB_RING (1 << 21)     /* vertices in the vertex buffer: written in turn, given new storage when it wraps */
#define ST_RING (1 << 18)     /* state records in the state buffer, the same way */

typedef struct Rt {
    int used, depth;
    uint32_t bp, bw, psm;   /* bp in blocks (FRAME / ZBUF give whole pages); psm: FRAME's, or 0x30 | ZBUF's */
    uint32_t rows;          /* rows loaded: whole pages */
    int dirty;              /* drawn since its last resolve, in rows dy0..dy1 */
    int dy0, dy1;
    int dpValid;            /* dpages: the pages of rows dy0..dy1 */
    uint32_t dpages[16];
    uint32_t pages[16];     /* the pages of its rows */
    uint32_t bytes;         /* the bytes of a word it owns: 0x7 for 24 bits */
    GLuint tex;
    int w;
    /* T37 */
    int m;                  /* widescreen margin left and right (GS pixels; a 640-wide target only) */
    int tw, th;             /* the texture's size: (w + 2 m) x rows GS pixels, sScale GL pixels each */
    GLuint snap, snapFbo;   /* a copy for texture reads of it at full resolution (snap_update) */
    int snapDirty, sy0, sy1; /* rows drawn since that copy */
    uint32_t snapGen;       /* the copy's serial number */
    uint32_t mgen, snapMgen; /* the window picture its margins were last drawn in (and as of the copy) */
    int copyOf;             /* 1 + the target whose snapshot copyGen it holds, halved vertically (the game's flicker filter) */
    uint32_t copyGen;
} Rt;

typedef struct GlVtx {
    int32_t x, y;
    uint32_t z, rgba, s, t, q, fog, state;
} GlVtx;

/* What a draw call shares (a change ends the batch). */
typedef struct Key {
    int rt, zrt;            /* target, depth target (-1: none) */
    int src;                /* the target whose snapshot the texture is (-1: GS memory) */
    int kind;               /* 0 triangles, 1 lines, 2 points */
    int blend;              /* 0 off, 1 add, 2 subtract, 3 reverse subtract, 4 zero */
    int dfunc, dwrite;      /* depth (dfunc 0: no depth test) */
    int cmask;              /* colour mask r g b a = bits 0..3 */
    int pass;               /* 0 one pass, no alpha test; 1 one pass, failing pixels dropped; 2 two passes */
    int fmask, fdwrite;     /* the second pass's masks */
} Key;

static int sReady;
/* The video options (T37, docs/port/video.md), applied at the window's pictures. */
static int sScale = 1;     /* internal resolution: a GS pixel of a target is sScale x sScale GL pixels */
static int sMarg;          /* widescreen: GS pixels added left and right of 640-wide targets and of the picture */
static int sDeflicker = 1; /* 0: the frame the game blurs into its half-height display buffer is shown instead */
static int sSmooth = 1;    /* the picture scaled into the window bilinear (1) or nearest (0) */
static int sLowLat;        /* low input lag: present a frame when the game copies it, glFinish after the swap */
static uint32_t sFrame = 2; /* window pictures (margins drawn in this one or the last are shown) */
static uint32_t sSnapGen;
static int sCopyPend = -1; /* the game is copying its frame into this target (its display buffer) */
static int sCopyHits;      /* copy primitives that covered the display buffer's last pixel (the copy's last cell) */
static int sEarlyShown;    /* low latency: the picture of the next VBlank is on the screen already */
static DispRegs sLastRegs;
static int sLastRegsValid;
static uint64_t sShowNs[2]; /* when the last two window pictures' VBlanks were queued (host ns) */
static struct { uint32_t presents, early, skipped, n[2]; uint64_t latSum[2], latMax[2]; } sPres; /* [1]: early presents */
/* T38: frame pacing (docs/port/video.md), from the window picture of VBlank TTT_PACE_FROM (default 600) on: the
   intervals between presents (swap returned) and between the VBlank pictures' queueing on the game's side, as
   histograms of 20 us bins up to 100 ms (longer ones only counted and in the max). */
#define PACE_BIN_NS 20000u
#define PACE_BINS 5000u
#define VBLANK_NS 16683333u /* 1 / 59.94 Hz */
typedef struct { uint32_t bins[PACE_BINS], n, over, late; uint64_t sum, max, last; } Pace;
static Pace sPacePres, sPaceQueue;
static uint32_t sPaceFrom, sPaceDropped; /* 0: TTT_PACE_FROM not read yet */
static uint32_t sPaceShows, sPaceRepeats; /* VBlank pictures, those without a new game frame since the last */
static uint16_t sPaceLastFrame;
static int sPaceOn;
static uint64_t sSwapSum, sSwapMax;
static int sDispW, sDispH; /* the display texture's size */
static int sCause; /* what asks for a resolve / read back (TTT_FRAMES report): 0 texture, 1 CLUT, 2 upload, 3 display, 4 target */
static uint32_t sResBy[5], sRbBy[5];
static GLuint sVramTex, sVramFbo, sTabTex, sInvTex, sPalTex, sStateBuf, sStateTex, sStageTex, sDispTex, sDispFbo;
static GLuint sVbo, sVao, sEmptyVao;
static GLuint sProgDraw, sProgResolve, sProgLoad, sProgStage, sProgDisplay, sProgClutLoad, sProgClutExpand;
#define CLUT_ROWS 64 /* GPU CLUT buffers per texture; two textures, the rows taking turns (a pass reads the previous row) */
static GLuint sClutTex[2], sClutFbo[2], sPalFbo;
static struct { GLint rect, target, row, cpu, unk, arg; } uClutLoad, uClutExpand;
/* The CLUT buffer's halves only the GPU has (sClutTex row sClutGpu: serial number, row (n >> 1) % CLUT_ROWS of texture
   n & 1); sClutHash is then a serial number (with the top bit set) instead of the content's hash. */
static uint32_t sClutUnk[16], sClutGpu, sClutSerial;
static int sClutUnkAny;
static uint32_t sMirrorGen; /* bumped whenever the mirror changes (resolves, pushes) */
/* The last load run on the GPU: the same one again, on the buffer it left and the same mirror, changes nothing (the game
   sends TEX0 with CLD 1 before every batch) */
static struct { uint64_t key, ktc, after; uint32_t mirrorGen; } sLastGpuLoad;
static struct { GLint size, zbits, zmax, pass, scale, marg; } uDraw;
static struct { GLint rect, target, buf, sm; } uResolve, uLoad, uStage;
static struct { GLint rect, target, ca, cb, mode, main, tex, out; } uDisp;
static Rt sRt[MAX_RT];
static struct { int c, z; GLuint fbo; } sFbo[48];
static uint8_t sCpuDirty[512]; /* bytes of the page the CPU wrote that the mirror lacks */
static uint8_t sGpuNewer[512]; /* bytes of the page the GPU wrote (resolves) that the CPU copy lacks */
static int sGpuPages;          /* pages with sGpuNewer bytes */
static uint32_t sMemGen;       /* bumped whenever the CPU copy of GS memory changes (uploads, copies, read backs) */
static uint64_t sClutHash;
static uint32_t sDirtyEpoch, sCheckedEpoch; /* bumped when a target becomes dirty: sDirtyAny is then made again */
static int sChecked;
static uint32_t sDirtyAny[16];     /* the CLUT buffer's content (64-bit FNV-1a of its words) */
static uint16_t sClut[512];   /* the CLUT buffer, loaded in order with the primitives */
static uint32_t (*sPal)[256];  /* CLUT rows waiting for the next draw call */
static uint8_t sPalA[PAL_ROWS][2]; /* per CLUT row: its entries' smallest and largest alpha */
static uint32_t sPalNext, sPalUp;
static struct { uint64_t ver; uint32_t cpsm, csa, bits; uint64_t texa; int row; } sPalKey = {0, 0, 0, 0, 0, -1};
static GlVtx *sV;
static uint32_t sNv;
static uint32_t (*sS)[12];
static uint32_t sNs;
static uint32_t sVbBase, sStBase, sStRing; /* where the next flush writes in the rings (vertices, state records) */
/* A draw call: the primitives sharing the fixed-function state (a two-pass alpha test also needs the same state record,
   as the GS's own batches: its failing pixels are drawn after the passing ones of the whole batch). */
typedef struct Batch {
    Key k;
    uint32_t first, count;
} Batch;
#define MAX_BATCHES 32768
static Batch *sB;
static uint32_t sNb;
static struct { uint64_t tex0, clamp; uint32_t pages[16]; int valid; } sTexPages;
/* The geometry of a primitive for same_depth(): type, vertex positions and Z, scissor. */
typedef struct Geo {
    int type;
    int32_t xyz[3][3];
    int sc[4];
} Geo;
/* The last primitive that wrote depth wherever it passed its depth test (see same_depth()). */
static struct { int valid, zrt; Geo g; } sLastZ;
static struct { uint32_t batches, draws, resolves, loads, pushes, readbacks, rts, date, ad, fbmsk, colclamp, pabe, overlapZ, shows, clutGpu; } sGlStat;

/* ------------------------------------------------------------------------------------------------ helpers */

static GLuint shader(GLenum type, const char *a, const char *b, const char *c, char *why, int whylen) {
    const char *src[3] = {a, b, c};
    GLuint s = gl.CreateShader(type);
    GLint ok = 0;
    gl.ShaderSource(s, 3, src, NULL);
    gl.CompileShader(s);
    gl.GetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        log[0] = 0;
        gl.GetShaderInfoLog(s, sizeof log, NULL, log);
        snprintf(why, (size_t)whylen, "shader: %s", log);
        return 0;
    }
    return s;
}

static GLuint program(const char *vs, const char *fs, const char *fsLib, char *why, int whylen) {
    GLuint v = shader(GL_VERTEX_SHADER, vs, "", "", why, whylen), f, p;
    GLint ok = 0;
    if (v == 0) {
        return 0;
    }
    f = fsLib != NULL ? shader(GL_FRAGMENT_SHADER, "#version 330 core\n", fsLib, fs, why, whylen)
                      : shader(GL_FRAGMENT_SHADER, fs, "", "", why, whylen);
    if (f == 0) {
        return 0;
    }
    p = gl.CreateProgram();
    gl.AttachShader(p, v);
    gl.AttachShader(p, f);
    gl.LinkProgram(p);
    gl.GetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        log[0] = 0;
        gl.GetProgramInfoLog(p, sizeof log, NULL, log);
        snprintf(why, (size_t)whylen, "link: %s", log);
        return 0;
    }
    return p;
}

static void samplers(GLuint p) {
    static const struct { const char *name; int unit; } k[] = {
        {"uVram", U_VRAM}, {"uTab", U_TAB}, {"uInv", U_INV}, {"uPal", U_PAL}, {"uStates", U_STATES}, {"uSrc", U_SRC}, {"uStage", U_STAGE},
        {"uClut", U_CLUT}, {"uSnap", U_SNAP}, {"uDsp0", U_DSP0}, {"uDsp1", U_DSP1},
    };
    size_t i;
    gl.UseProgram(p);
    for (i = 0; i < sizeof k / sizeof k[0]; i++) {
        GLint l = gl.GetUniformLocation(p, k[i].name);
        if (l >= 0) {
            gl.Uniform1i(l, k[i].unit);
        }
    }
}

static GLuint texture(GLenum ifmt, int w, int h, GLenum fmt, GLenum type, const void *data) {
    GLuint t;
    gl.GenTextures(1, &t);
    gl.BindTexture(GL_TEXTURE_2D, t);
    gl.TexImage2D(GL_TEXTURE_2D, 0, (GLint)ifmt, w, h, 0, fmt, type, data);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return t;
}

static GLuint fbo_of(GLuint color, GLuint depth) {
    GLuint f;
    gl.GenFramebuffers(1, &f);
    gl.BindFramebuffer(GL_FRAMEBUFFER, f);
    if (color != 0) {
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        gl.DrawBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        gl.DrawBuffer(GL_NONE);
        gl.ReadBuffer(GL_NONE);
    }
    if (depth != 0) {
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    }
    if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "ttt: gl: incomplete framebuffer\n");
    }
    return f;
}

/* The framebuffer of colour target c and depth target z (-1: none). */
static GLuint fbo(int c, int z) {
    size_t i, free_ = sizeof sFbo / sizeof sFbo[0];
    for (i = 0; i < sizeof sFbo / sizeof sFbo[0]; i++) {
        if (sFbo[i].fbo != 0 && sFbo[i].c == c && sFbo[i].z == z) {
            return sFbo[i].fbo;
        }
        if (sFbo[i].fbo == 0 && free_ == sizeof sFbo / sizeof sFbo[0]) {
            free_ = i;
        }
    }
    if (free_ == sizeof sFbo / sizeof sFbo[0]) { /* full: start again */
        for (i = 0; i < sizeof sFbo / sizeof sFbo[0]; i++) {
            gl.DeleteFramebuffers(1, &sFbo[i].fbo);
            sFbo[i].fbo = 0;
        }
        free_ = 0;
    }
    sFbo[free_].c = c;
    sFbo[free_].z = z;
    sFbo[free_].fbo = fbo_of(c >= 0 ? sRt[c].tex : 0, z >= 0 ? sRt[z].tex : 0);
    return sFbo[free_].fbo;
}

static void rect_pass(GLint uRect, GLint uTarget, int x0, int y0, int x1, int y1, int tw, int th) {
    gl.Uniform4i(uRect, x0, y0, x1, y1);
    gl.Uniform2i(uTarget, tw, th);
    gl.BindVertexArray(sEmptyVao);
    gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/* The GL state a batch left behind is not tracked: each pass sets what it needs. */
static void plain_state(void) {
    gl.Disable(GL_BLEND);
    gl.Disable(GL_DEPTH_TEST);
    gl.DepthMask(0);
    gl.ColorMask(1, 1, 1, 1);
}

static int set_overlaps(const uint32_t *a, const uint32_t *b) {
    int i;
    for (i = 0; i < 16; i++) {
        if (a[i] & b[i]) {
            return 1;
        }
    }
    return 0;
}

/* The bytes of a GS word a format writes (the formats inside the top byte of a 32-bit pixel, 24-bit colour / depth). */
static uint32_t psm_bytes(uint32_t psm) {
    switch (psm) {
    case 0x01: case 0x31: return 0x7;
    case 0x1B: case 0x24: case 0x2C: return 0x8;
    default: return 0xF;
    }
}

static uint32_t page_height(uint32_t psm) {
    uint32_t bits = (uint32_t)Gs_PsmBits(psm);
    return bits >= 24 ? 32 : bits == 4 ? 128 : 64;
}

/* ------------------------------------------------------------------------------------------------ GS memory */

/* The pages the CPU wrote into the mirror: whole words with a texture upload, some bytes with a staged masked copy. */
static void push(void) {
    uint32_t *vram = Gs_Vram();
    int p = 0, staged = 0;

    while (p < 512) {
        int q;
        uint8_t m = sCpuDirty[p];
        if (m == 0) {
            p++;
            continue;
        }
        for (q = p; q < 512 && sCpuDirty[q] == m; q++) {
        }
        if (m == 0xF) {
            gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
            gl.BindTexture(GL_TEXTURE_2D, sVramTex);
            gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, p * 2, 1024, (q - p) * 2, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, vram + p * 2048);
        } else {
            gl.ActiveTexture(GL_TEXTURE0 + U_STAGE);
            gl.BindTexture(GL_TEXTURE_2D, sStageTex);
            gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, p * 2, 1024, (q - p) * 2, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, vram + p * 2048);
            if (!staged) {
                staged = 1;
                gl.BindFramebuffer(GL_FRAMEBUFFER, sVramFbo);
                gl.Viewport(0, 0, 1024, 1024);
                gl.UseProgram(sProgStage);
                plain_state();
            }
            gl.ColorMask(m & 1, (m >> 1) & 1, (m >> 2) & 1, (m >> 3) & 1);
            rect_pass(uStage.rect, uStage.target, 0, p * 2, 1024, q * 2, 1024, 1024);
        }
        sGlStat.pushes++;
        sMirrorGen++;
        memset(sCpuDirty + p, 0, (size_t)(q - p));
        p = q;
    }
    gl.ColorMask(1, 1, 1, 1);
    gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
    gl.BindTexture(GL_TEXTURE_2D, sVramTex);
}

/* The CPU copy of the pages of `set` gets the bytes `mask` the GPU wrote there. */
static void readback(const uint32_t *set, uint32_t mask) {
    uint32_t *vram = Gs_Vram(), w;
    int any = 0;

    if (sGpuPages == 0) {
        return;
    }
    for (w = 0; w < 16; w++) {
        uint32_t bits = set[w];
        while (bits != 0) {
            int p = (int)(w * 32 + (uint32_t)__builtin_ctz(bits));
            bits &= bits - 1;
            if (!(sGpuNewer[p] & mask)) {
                continue;
            }
            if (!any) {
                any = 1;
                push(); /* the mirror then holds every byte: whole words come back */
                gl.BindFramebuffer(GL_READ_FRAMEBUFFER, sVramFbo);
                gl.ReadBuffer(GL_COLOR_ATTACHMENT0);
            }
            gl.ReadPixels(0, p * 2, 1024, 2, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, vram + p * 2048);
            sGpuNewer[p] = 0;
            sGpuPages--;
            sMemGen++;
            sGlStat.readbacks++;
            sRbBy[sCause]++;
        }
    }
}

static void flush(void);
static void rt_drop(int i);

/* Writes target i back into GS memory (the mirror) if it was drawn since its last resolve. */
static void resolve(int i) {
    Rt *r = &sRt[i];
    uint32_t ph = page_height(r->psm), p0, pages, p;

    if (!r->dirty) {
        return;
    }
    flush();
    push();
    r->dirty = 0;
    /* the page rows drawn since the last resolve */
    p0 = (r->bp >> 5) + (uint32_t)r->dy0 / ph * r->bw;
    pages = ((uint32_t)r->dy1 / ph - (uint32_t)r->dy0 / ph + 1) * r->bw;
    if (p0 + pages > 512) {
        pages = 512 - p0;
    }
    gl.BindFramebuffer(GL_FRAMEBUFFER, sVramFbo);
    gl.Viewport(0, 0, 1024, 1024);
    gl.UseProgram(sProgResolve);
    plain_state();
    gl.ColorMask(1, 1, 1, r->bytes == 0xF);
    gl.ActiveTexture(GL_TEXTURE0 + U_SRC);
    gl.BindTexture(GL_TEXTURE_2D, r->tex);
    gl.Uniform4uiv(uResolve.buf, 1, (const GLuint[]){r->bp, r->bw, r->psm, (GLuint)r->depth});
    gl.Uniform2i(uResolve.sm, sScale, r->m);
    rect_pass(uResolve.rect, uResolve.target, 0, (int)p0 * 2, 1024, (int)(p0 + pages) * 2, 1024, 1024);
    gl.ColorMask(1, 1, 1, 1);
    gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
    for (p = p0; p < p0 + pages; p++) {
        sGpuPages += sGpuNewer[p] == 0;
        sGpuNewer[p] |= (uint8_t)r->bytes;
    }
    sGlStat.resolves++;
    sResBy[sCause]++;
    sMirrorGen++;
}

/* Whether target i drew pages of `set` (bytes `mask`) since its last resolve. */
static int drew_over(int i, const uint32_t *set, uint32_t mask) {
    Rt *r = &sRt[i];
    if (!r->used || !r->dirty || !(r->bytes & mask) || !set_overlaps(r->pages, set)) {
        return 0;
    }
    if (!r->dpValid) {
        memset(r->dpages, 0, sizeof r->dpages);
        Gs_PagesOf(r->dpages, r->bp, r->bw, r->psm, 0, r->bw * 64 - 1, (uint32_t)r->dy0, (uint32_t)r->dy1);
        r->dpValid = 1;
    }
    return set_overlaps(r->dpages, set);
}

/* Resolves every target that drew pages of `set` with bytes `mask`. */
static void resolve_over(const uint32_t *set, uint32_t mask) {
    int i;
    for (i = 0; i < MAX_RT; i++) {
        if (drew_over(i, set, mask)) {
            resolve(i);
        }
    }
}

/* Loads rows y0..y1 - 1 of target i from GS memory. */
static void rt_load(int i, uint32_t y0, uint32_t y1) {
    Rt *r = &sRt[i];
    flush();
    push();
    gl.BindFramebuffer(GL_FRAMEBUFFER, fbo(r->depth ? -1 : i, r->depth ? i : -1));
    gl.Viewport(0, 0, r->tw, r->th);
    gl.UseProgram(sProgLoad);
    plain_state();
    gl.Uniform2i(uLoad.sm, sScale, r->m);
    if (r->depth) {
        uint32_t bits = (uint32_t)Gs_PsmBits(r->psm);
        gl.Enable(GL_DEPTH_TEST);
        gl.DepthFunc(GL_ALWAYS);
        gl.DepthMask(1);
        gl.Uniform4uiv(uLoad.buf, 1, (const GLuint[]){r->bp, r->bw, r->psm, bits == 32 ? 32u : bits == 24 ? 24u : 16u});
    } else {
        gl.Uniform4uiv(uLoad.buf, 1, (const GLuint[]){r->bp, r->bw, r->psm, 0});
    }
    rect_pass(uLoad.rect, uLoad.target, 0, (int)y0 * sScale, r->tw, (int)y1 * sScale, r->tw, r->th);
    plain_state();
    sGlStat.loads++;
    sLastZ.valid = 0;
}

static void rt_drop(int i) {
    size_t k;
    if (!sRt[i].used) {
        return;
    }
    sLastZ.valid = 0;
    resolve(i);
    flush();
    for (k = 0; k < sizeof sFbo / sizeof sFbo[0]; k++) {
        if (sFbo[k].fbo != 0 && (sFbo[k].c == i || sFbo[k].z == i)) {
            gl.DeleteFramebuffers(1, &sFbo[k].fbo);
            sFbo[k].fbo = 0;
        }
    }
    gl.DeleteTextures(1, &sRt[i].tex);
    if (sRt[i].snap != 0) {
        gl.DeleteFramebuffers(1, &sRt[i].snapFbo);
        gl.DeleteTextures(1, &sRt[i].snap);
    }
    if (sCopyPend == i) {
        sCopyPend = -1;
    }
    memset(&sRt[i], 0, sizeof sRt[i]);
    sTexPages.valid = 0;
}

/* Drops every target but `keep` over the pages of `set` with bytes in `mask` (resolved first). */
static void drop_over(const uint32_t *set, uint32_t mask, int keep) {
    int i;
    for (i = 0; i < MAX_RT; i++) {
        if (i != keep && sRt[i].used && (sRt[i].bytes & mask) && set_overlaps(sRt[i].pages, set)) {
            rt_drop(i);
        }
    }
}

/* The target for buffer (bp, bw, psm) with at least `rows` rows (created, grown and loaded as needed). */
static int rt_get(int depth, uint32_t bp, uint32_t bw, uint32_t psm, uint32_t rows) {
    uint32_t ph = page_height(psm), set[16];
    int i, slot = -1;

    if (bw == 0) {
        bw = 1;
    }
    rows = (rows + ph - 1) / ph * ph;
    if (rows > RT_ROWS) {
        rows = RT_ROWS;
    }
    for (i = 0; i < MAX_RT; i++) {
        Rt *r = &sRt[i];
        if (r->used && r->depth == depth && r->bp == bp && r->bw == bw && r->psm == psm) {
            if (rows <= r->rows) {
                return i;
            }
            /* grow: the texture is as high as the rows (T37: scaled targets), so the target is written back and
               made again, loaded from GS memory */
            rt_drop(i);
            break;
        }
    }
    memset(set, 0, sizeof set);
    Gs_PagesOf(set, bp, bw, psm, 0, bw * 64 - 1, 0, rows - 1);
    drop_over(set, 0xF, -1);
    for (i = 0; i < MAX_RT && slot < 0; i++) {
        if (!sRt[i].used) {
            slot = i;
        }
    }
    if (slot < 0) { /* full: the first one goes */
        rt_drop(0);
        slot = 0;
    }
    flush();
    {
        Rt *r = &sRt[slot];
        memset(r, 0, sizeof *r);
        r->used = 1;
        r->depth = depth;
        r->bp = bp;
        r->bw = bw;
        r->psm = psm;
        r->rows = rows;
        r->bytes = psm_bytes(psm);
        r->w = (int)bw * 64;
        r->m = r->w == 640 ? sMarg : 0;
        r->tw = (r->w + 2 * r->m) * sScale;
        r->th = (int)rows * sScale;
        memcpy(r->pages, set, sizeof set);
        gl.ActiveTexture(GL_TEXTURE0 + U_SRC); /* the unit the passes rebind: the others keep their textures */
        r->tex = depth ? texture(GL_DEPTH_COMPONENT32F, r->tw, r->th, GL_DEPTH_COMPONENT, GL_FLOAT, NULL)
                       : texture(GL_RGBA16F, r->tw, r->th, GL_RGBA, GL_FLOAT, NULL);
        gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
        rt_load(slot, 0, rows);
    }
    sGlStat.rts++;
    return slot;
}

/* ------------------------------------------------------------------------------------------------ draws */

static void blend_state(int b) {
    if (b == 0) {
        gl.Disable(GL_BLEND);
        return;
    }
    gl.Enable(GL_BLEND);
    if (b == 4) {
        gl.BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        gl.BlendFuncSeparate(GL_ZERO, GL_ZERO, GL_ONE, GL_ZERO);
        return;
    }
    gl.BlendEquationSeparate(b == 1 ? GL_FUNC_ADD : b == 2 ? GL_FUNC_SUBTRACT : GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
    gl.BlendFuncSeparate(GL_ONE, GL_SRC1_ALPHA, GL_ONE, GL_ZERO);
}

/* The colour and depth masks, each set only when it changes. */
static void set_masks(int cmask, int dwrite, int *curC, int *curD) {
    if (cmask != *curC) {
        gl.ColorMask(cmask & 1, (cmask >> 1) & 1, (cmask >> 2) & 1, (cmask >> 3) & 1);
        *curC = cmask;
    }
    if ((dwrite != 0) != *curD) {
        gl.DepthMask(dwrite != 0);
        *curD = dwrite != 0;
    }
}

/* The CLUT rows made on the CPU since the last upload. */
static void pal_upload(void) {
    if (sPalNext > sPalUp) {
        gl.ActiveTexture(GL_TEXTURE0 + U_PAL);
        gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)sPalUp, 256, (GLsizei)(sPalNext - sPalUp), GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, sPal[sPalUp]);
        gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
        sPalUp = sPalNext;
    }
}

/* The halves of the CLUT buffer that a load or a read with these fields writes / reads (gs_core.c's clut_fill /
   clut_read): bits of out[16]. */
static void clut_halves(uint32_t cpsm, uint32_t csa, uint32_t n, uint32_t out[16]) {
    int c32 = cpsm == 0 || cpsm == 1;
    memset(out, 0, 16 * sizeof *out);
    if (n == 256) { /* entries 0..255: every half (32 bits), the low 256 (16 bits) */
        memset(out, 0xFF, (c32 ? 16 : 8) * sizeof *out);
    } else { /* 16 entries from CSA: half a word of the set (and the same 256 halves on, 32 bits) */
        uint32_t e = c32 ? (csa * 16) & 255 : (csa * 16) & 511;
        out[e >> 5] |= 0xFFFFu << (e & 31);
        if (c32) {
            out[(e + 256) >> 5] |= 0xFFFFu << (e & 31);
        }
    }
}

/* The uniforms of kClutLib for the current buffer. */
static void clut_uniforms(GLint row, GLint cpu, GLint unk) {
    GLuint words[256];
    uint32_t i;
    for (i = 0; i < 256; i++) {
        words[i] = (GLuint)sClut[i * 2] | (GLuint)sClut[i * 2 + 1] << 16;
    }
    gl.Uniform4uiv(cpu, 64, words);
    gl.Uniform4uiv(unk, 4, sClutUnk);
    gl.ActiveTexture(GL_TEXTURE0 + U_CLUT);
    gl.BindTexture(GL_TEXTURE_2D, sClutTex[sClutGpu & 1]);
    gl.Uniform1i(row, sClutUnkAny ? (GLint)((sClutGpu >> 1) % CLUT_ROWS) : -1);
    gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
}

/* Writes `bytes` bytes at `at` of the buffer bound to `target`; `fresh`: the ring wrapped (new storage). */
static void ring_write(GLenum target, int fresh, GLintptr at, GLsizeiptr bytes, const void *data) {
    void *p = gl.MapBufferRange(target, at, bytes, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT |
                                                   (fresh ? GL_MAP_INVALIDATE_BUFFER_BIT : GL_MAP_INVALIDATE_RANGE_BIT));
    if (p == NULL) { /* not expected: the plain upload */
        gl.BufferSubData(target, at, bytes, data);
        return;
    }
    memcpy(p, data, (size_t)bytes);
    gl.UnmapBuffer(target);
}

/* Carries out the queued primitives: their vertices and state records go to the GPU in one upload, then one draw call
   per batch (two with an alpha test that writes on failure), setting only the GL state that changes between them. */
static void flush(void) {
    uint32_t i;
    int curFbo = -1, curRt = -1, curZ = -1, curDf = -1, curBlend = -1, curPass = -1, curCmask = -1, curDmask = -1, curSrc = -1;

    if (sNv == 0) {
        return;
    }
    push();
    pal_upload();
    /* Both go after what earlier flushes wrote, unsynchronised (the GPU still reads those parts, never these); a ring
       that wraps gets new storage (the driver keeps the old one until the GPU is done with it). Giving the buffers new
       storage at every flush cost the driver fresh pages each time (T30 batch 4: 700 page faults per fight VBlank). */
    gl.BindBuffer(GL_TEXTURE_BUFFER, sStateBuf);
    ring_write(GL_TEXTURE_BUFFER, sStBase == 0, (GLintptr)sStBase * 48, (GLsizeiptr)sNs * 48, sS);
    sStBase += sNs;
    if (sStBase + MAX_STATES > sStRing) {
        sStBase = 0; /* put() numbers the next records from there */
    }
    gl.BindBuffer(GL_ARRAY_BUFFER, sVbo);
    if (sVbBase + sNv > VB_RING) {
        sVbBase = 0;
    }
    ring_write(GL_ARRAY_BUFFER, sVbBase == 0, (GLintptr)sVbBase * (GLintptr)sizeof(GlVtx), (GLsizeiptr)sNv * (GLsizeiptr)sizeof(GlVtx), sV);
    gl.UseProgram(sProgDraw);
    gl.Uniform1i(uDraw.scale, sScale);
    gl.BindVertexArray(sVao);
    for (i = 0; i < sNb; i++) {
        const Key *k = &sB[i].k;
        GLenum mode = k->kind == 0 ? GL_TRIANGLES : k->kind == 1 ? GL_LINES : GL_POINTS;
        int fb = (int)fbo(k->rt, k->zrt), zbits = 0;
        if (fb != curFbo) {
            gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)fb);
            curFbo = fb;
        }
        if (k->rt != curRt) {
            const Rt *r = &sRt[k->rt];
            curRt = k->rt;
            gl.Viewport(0, 0, r->tw, r->th);
            gl.Uniform2i(uDraw.size, r->w + 2 * r->m, (GLint)r->rows);
            gl.Uniform1i(uDraw.marg, r->m);
        }
        if (k->src != curSrc && k->src >= 0) {
            gl.ActiveTexture(GL_TEXTURE0 + U_SNAP);
            gl.BindTexture(GL_TEXTURE_2D, sRt[k->src].snap);
            gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
            curSrc = k->src;
        }
        if (k->zrt >= 0) {
            uint32_t bits = (uint32_t)Gs_PsmBits(sRt[k->zrt].psm);
            zbits = bits == 32 ? 32 : bits == 24 ? 24 : 16;
        }
        if (zbits != curZ) {
            gl.Uniform1i(uDraw.zbits, zbits);
            gl.Uniform1ui(uDraw.zmax, zbits == 32 ? 0xFFFFFFFFu : zbits == 24 ? 0xFFFFFFu : 0xFFFFu);
            if (zbits != 0 && curZ <= 0) {
                gl.Enable(GL_DEPTH_TEST);
            } else if (zbits == 0) {
                gl.Disable(GL_DEPTH_TEST);
            }
            curZ = zbits;
            curDf = -1;
        }
        if (zbits != 0 && k->dfunc != curDf) {
            gl.DepthFunc((GLenum)k->dfunc);
            curDf = k->dfunc;
        }
        if (k->blend != curBlend) {
            blend_state(k->blend);
            curBlend = k->blend;
        }
        set_masks(k->cmask, k->dwrite, &curCmask, &curDmask);
        if ((k->pass != 0) != curPass) {
            curPass = k->pass != 0;
            gl.Uniform1i(uDraw.pass, curPass);
        }
        gl.DrawArrays(mode, (GLint)(sVbBase + sB[i].first), (GLsizei)sB[i].count);
        sGlStat.draws++;
        if (k->pass == 2) { /* the failing pixels: usually only the depth mask changes (FB_ONLY) */
            set_masks(k->fmask, k->fdwrite, &curCmask, &curDmask);
            gl.Uniform1i(uDraw.pass, 2);
            curPass = 2;
            gl.DrawArrays(mode, (GLint)(sVbBase + sB[i].first), (GLsizei)sB[i].count);
            sGlStat.draws++;
        }
    }
    sVbBase += sNv;
    plain_state();
    sGlStat.batches += sNb;
    sNv = 0;
    sNs = 0;
    sNb = 0;
}

/* The CLUT row of an indexed texture (expanded through TEXA), shared by the primitives that use the same one. */
static int pal_row(const Draw *d) {
    static struct { uint64_t ver, texa; uint32_t cpsm, csa, bits, hash, n; int row; } cache[32];
    static uint32_t cacheNext;
    uint32_t cpsm = (d->tex0 >> 51) & 15, csa = (d->tex0 >> 56) & 31, tmp[256], n = d->tbits == 8 ? 256 : 16, h = 2166136261u, i;

    if (sPalKey.row >= 0 && sPalKey.ver == sClutHash && sPalKey.cpsm == cpsm && sPalKey.csa == csa && sPalKey.bits == d->tbits &&
        sPalKey.texa == d->texa) {
        return sPalKey.row;
    }
    sPalKey.ver = sClutHash;
    sPalKey.cpsm = cpsm;
    sPalKey.csa = csa;
    sPalKey.bits = d->tbits;
    sPalKey.texa = d->texa;
    if (sClutUnkAny) { /* entries the CPU copy lacks: the row is made on the GPU (pal_gpu) */
        uint32_t need[16];
        int k, gpu = 0;
        clut_halves(cpsm, csa, n, need);
        for (k = 0; k < 16; k++) {
            gpu |= (need[k] & sClutUnk[k]) != 0;
        }
        if (gpu) {
            for (i = 0; i < 32; i++) {
                if (cache[i].row >= 0 && cache[i].row < (int)sPalNext && cache[i].ver == sClutHash && cache[i].cpsm == cpsm &&
                    cache[i].csa == csa && cache[i].bits == d->tbits && cache[i].texa == d->texa) {
                    sPalKey.row = cache[i].row;
                    return sPalKey.row;
                }
            }
            if (sPalNext == PAL_ROWS) {
                flush();
                sPalNext = sPalUp = 0;
                for (i = 0; i < 32; i++) {
                    cache[i].row = -1;
                }
            }
            pal_upload(); /* the CPU rows before it, so that a later upload does not cover it */
            gl.BindFramebuffer(GL_FRAMEBUFFER, sPalFbo);
            gl.Viewport(0, 0, 256, PAL_ROWS);
            gl.UseProgram(sProgClutExpand);
            plain_state();
            clut_uniforms(uClutExpand.row, uClutExpand.cpu, uClutExpand.unk);
            gl.Uniform4uiv(uClutExpand.arg, 1, (const GLuint[]){cpsm, csa, n, (GLuint)(d->texa & 0xFF) | (GLuint)((d->texa >> 32) & 0xFF) << 8 |
                                                                  (GLuint)((d->texa >> 15) & 1) << 16});
            rect_pass(uClutExpand.rect, uClutExpand.target, 0, (int)sPalNext, 256, (int)sPalNext + 1, 256, PAL_ROWS);
            sGlStat.clutGpu++;
            sPalA[sPalNext][0] = 0; /* not known here */
            sPalA[sPalNext][1] = 255;
            i = cacheNext++ & 31;
            cache[i].ver = sClutHash;
            cache[i].texa = d->texa;
            cache[i].cpsm = cpsm;
            cache[i].csa = csa;
            cache[i].bits = d->tbits;
            cache[i].hash = 0;
            cache[i].n = 0; /* never matched by content */
            cache[i].row = (int)sPalNext;
            sPalKey.row = (int)sPalNext++;
            sPalUp = sPalNext;
            return sPalKey.row;
        }
    }
    /* the same CLUT content, entry range and TEXA give the same row (the game alternates a few CLUTs) */
    for (i = 0; i < 32; i++) {
        if (cache[i].row >= 0 && cache[i].row < (int)sPalNext && cache[i].ver == sClutHash && cache[i].cpsm == cpsm && cache[i].csa == csa &&
            cache[i].bits == d->tbits && cache[i].texa == d->texa) {
            sPalKey.row = cache[i].row;
            return sPalKey.row;
        }
    }
    Gs_Palette(sClut, d, tmp);
    for (i = 0; i < n; i++) {
        h = (h ^ tmp[i]) * 16777619u;
    }
    for (i = 0; i < 32; i++) { /* another CLUT with the same colours */
        if (cache[i].row >= 0 && cache[i].row < (int)sPalNext && cache[i].hash == h && cache[i].n == n &&
            memcmp(sPal[cache[i].row], tmp, n * 4) == 0) {
            sPalKey.row = cache[i].row;
            return sPalKey.row;
        }
    }
    if (sPalNext == PAL_ROWS) {
        flush();
        sPalNext = sPalUp = 0;
        for (i = 0; i < 32; i++) {
            cache[i].row = -1;
        }
    }
    memcpy(sPal[sPalNext], tmp, n * 4);
    sPalA[sPalNext][0] = 255;
    sPalA[sPalNext][1] = 0;
    for (i = 0; i < n; i++) {
        uint8_t a = (uint8_t)(tmp[i] >> 24);
        sPalA[sPalNext][0] = a < sPalA[sPalNext][0] ? a : sPalA[sPalNext][0];
        sPalA[sPalNext][1] = a > sPalA[sPalNext][1] ? a : sPalA[sPalNext][1];
    }
    i = cacheNext++ & 31;
    cache[i].ver = sClutHash;
    cache[i].texa = d->texa;
    cache[i].cpsm = cpsm;
    cache[i].csa = csa;
    cache[i].bits = d->tbits;
    cache[i].hash = h;
    cache[i].n = n;
    cache[i].row = (int)sPalNext;
    sPalKey.row = (int)sPalNext++;
    return sPalKey.row;
}

/* draw_setup() leaves the texture fields of an untextured primitive unset: only the fields that apply are read, each
   cut to its width. */
static void state_of(const Draw *d, int palRow, uint32_t s[12]) {
    uint32_t fl = 0;
    memset(s, 0, 12 * sizeof *s);
    fl |= (uint32_t)(d->fst & 1) << 1 | (uint32_t)(d->gouraud & 1) << 5 | (uint32_t)(d->fog & 1) << 6 | (uint32_t)(d->fba & 1) << 9;
    fl |= (uint32_t)(d->atest & 1) << 10 | (d->atst & 7) << 11 | (uint32_t)(d->pabe & 1) << 20 | (uint32_t)(d->blend & 1) << 21;
    fl |= (d->asel & 3) << 22 | (d->bsel & 3) << 24 | (d->csel & 3) << 26 | (d->dsel & 3) << 28 | (uint32_t)(d->fbits != 32) << 30;
    if (d->tex) {
        fl |= 1u | (d->tfx & 3) << 2 | (d->tcc & 1) << 4 | (uint32_t)(d->filter & 3) << 7 | (uint32_t)(d->lcm & 1) << 14;
        fl |= (uint32_t)(d->lodl & 3) << 15 | (uint32_t)(d->mmagLinear & 1) << 17 | (uint32_t)(d->mminLinear & 1) << 18 | (d->aem & 1) << 19;
        s[1] = (d->tbp & 0x3FFF) | (d->tbw & 63) << 14 | (d->tpsm & 63) << 20 | (uint32_t)(d->lw & 15) << 26;
        s[2] = (uint32_t)(d->lh & 15) | (d->wms & 3) << 4 | (d->wmt & 3) << 6 | (uint32_t)(palRow & 0xFFFF) << 8;
        s[3] = (uint32_t)(d->minu & 1023) | (uint32_t)(d->maxu & 1023) << 10 | ((uint32_t)d->lodk & 0xFFF) << 20;
        s[4] = (uint32_t)(d->minv & 1023) | (uint32_t)(d->maxv & 1023) << 10;
        s[5] = (d->ta0 & 255) | (d->ta1 & 255) << 8;
    }
    s[0] = fl;
    s[2] |= (d->aref & 255) << 24;
    s[4] |= (d->afix & 255) << 20;
    s[6] = (uint32_t)d->scx0 | (uint32_t)d->scx1 << 16;
    s[7] = (uint32_t)d->scy0 | (uint32_t)d->scy1 << 16;
    s[8] = (uint32_t)d->fogcol & 0xFFFFFF;
}

/* The alpha the shader tests, as a range over the primitive: the vertices' alpha (one less for Gouraud shading, where
   the GPU's interpolation of equal values may land just below them), the texels' (a CLUT row's entries, TEXA for 24 /
   16 bits; bilinear filtering stays between its texels' alpha) and TFX / TCC. */
static void alpha_range(const Draw *d, const Vtx *v, int type, int palRow, uint32_t *lo, uint32_t *hi) {
    uint32_t cl, ch, tl, th;
    if (type == CMD_TRIANGLE && d->gouraud) {
        uint32_t a0 = v[0].rgba >> 24, a1 = v[1].rgba >> 24, a2 = v[2].rgba >> 24;
        cl = a0 < a1 ? a0 : a1;
        cl = a2 < cl ? a2 : cl;
        ch = a0 > a1 ? a0 : a1;
        ch = a2 > ch ? a2 : ch;
    } else { /* sprites and lines take the last vertex's colour, points their own */
        cl = ch = v[type == CMD_POINT ? 0 : 1].rgba >> 24;
    }
    if (d->gouraud && cl > 0) {
        cl--;
    }
    if (!d->tex || !d->tcc) {
        *lo = cl;
        *hi = ch;
        return;
    }
    if (d->tbits <= 8) {
        tl = sPalA[palRow][0];
        th = sPalA[palRow][1];
    } else if (d->tbits == 24) {
        tl = d->aem ? 0 : d->ta0;
        th = d->ta0;
    } else if (d->tbits == 16) {
        tl = d->ta0 < d->ta1 ? d->ta0 : d->ta1;
        th = d->ta0 > d->ta1 ? d->ta0 : d->ta1;
        tl = d->aem ? 0 : tl;
    } else {
        tl = 0;
        th = 255;
    }
    switch (d->tfx) {
    case 0: tl = (tl * cl) >> 7; th = (th * ch) >> 7; break;
    case 2: tl += cl; th += ch; break;
    default: break;
    }
    *lo = tl > 255 ? 255 : tl;
    *hi = th > 255 ? 255 : th;
}

/* The alpha test's outcome over alpha range lo..hi: 1 every pixel passes, -1 every pixel fails, 0 it depends. */
static int alpha_outcome(uint32_t atst, uint32_t aref, uint32_t lo, uint32_t hi) {
    switch (atst & 7) {
    case 0: return -1;
    case 1: return 1;
    case 2: return hi < aref ? 1 : lo >= aref ? -1 : 0;
    case 3: return hi <= aref ? 1 : lo > aref ? -1 : 0;
    case 4: return lo == aref && hi == aref ? 1 : aref < lo || aref > hi ? -1 : 0;
    case 5: return lo >= aref ? 1 : hi < aref ? -1 : 0;
    case 6: return lo > aref ? 1 : hi <= aref ? -1 : 0;
    default: return aref < lo || aref > hi ? 1 : lo == aref && hi == aref ? -1 : 0;
    }
}

/* The fixed-function part of a primitive. `outcome`: the alpha test's (alpha_outcome) when known for every pixel: such
   a primitive needs one pass with the passing or the failing masks and no test in the shader, and joins the batches
   of any state record (most of the fighters' and the stage's FB_ONLY surfaces: CLUT alpha all 0x80 against
   GEQUAL 0x80). */
static void key_of(const Draw *d, int rt, int zrt, int kind, int outcome, Key *k) {
    static const int kZ[4] = {GL_NEVER, GL_ALWAYS, GL_GEQUAL, GL_GREATER};
    uint32_t m = d->fbmsk;
    int cmask = 0, i, fmask, fdw;

    memset(k, 0, sizeof *k);
    k->rt = rt;
    k->zrt = zrt;
    k->kind = kind;
    k->blend = 1; /* without ABE: the shader's weights are 1 and 0, so opaque and blended primitives share a batch */
    if (d->blend) {
        int ksNeg = d->bsel == 0 && d->asel != 0 && d->dsel != 0, kdNeg = d->bsel == 1 && d->asel != 1 && d->dsel != 1;
        k->blend = ksNeg && kdNeg ? 4 : ksNeg ? 3 : kdNeg ? 2 : 1;
        if (d->csel == 1 && d->fbits != 24) {
            sGlStat.ad++;
        }
        if (d->pabe && k->blend == 3) {
            sGlStat.pabe++;
        }
        if (!d->colclamp) {
            sGlStat.colclamp++;
        }
    }
    for (i = 0; i < 4; i++) {
        uint32_t b = (m >> (8 * i)) & 0xFF;
        if (b != 0xFF) {
            cmask |= 1 << i;
        }
        if (b != 0 && b != 0xFF) {
            sGlStat.fbmsk++;
        }
    }
    if (d->date) {
        sGlStat.date++;
    }
    k->dfunc = zrt >= 0 ? (d->ztest ? kZ[d->ztst & 3] : GL_ALWAYS) : 0;
    k->dwrite = zrt >= 0 && d->zwrite;
    k->cmask = cmask;
    if (!d->atest || outcome > 0) {
        return;
    }
    switch (d->afail) {
    case 0: fmask = 0; fdw = 0; break;                 /* KEEP */
    case 1: fmask = cmask; fdw = 0; break;             /* FB_ONLY */
    case 2: fmask = 0; fdw = k->dwrite; break;         /* ZB_ONLY */
    default: fmask = cmask & 7; fdw = 0; break;        /* RGB_ONLY */
    }
    if (d->fbits == 24) { /* no alpha in the frame */
        fmask &= 7;
        if ((cmask & 7) == fmask) {
            fmask = cmask;
        }
    }
    if (outcome < 0) { /* every pixel fails: one pass with the failing masks */
        k->cmask = fmask;
        k->dwrite = fdw;
        k->pass = 0;
    } else if (fmask == cmask && fdw == k->dwrite) {
        k->pass = 0; /* the test changes nothing */
    } else if (fmask == 0 && fdw == 0) {
        k->pass = 1;
    } else {
        k->pass = 2;
        k->fmask = fmask;
        k->fdwrite = fdw;
    }
}

/* A primitive that should not write depth may when the write changes nothing: the fighters' and the stage's
   multi-pass surfaces draw a strip several times on the same vertices, so the later passes share the batch of the
   first instead of alternating the depth mask. P may write depth when its vertices (positions and Z, in order: the GPU
   makes the same fragments and depths) are those of Q, the last primitive that wrote depth wherever it passed its
   depth test (no per-pixel alpha test; ALWAYS, GEQUAL or GREATER), P's scissor lies inside Q's and P's test is GEQUAL
   or GREATER: wherever P passes, the depth is already its own (Q wrote it, or Q failed only on an equal one). */
static void geo_of(const Cmd *c, Geo *g) {
    const Vtx *v = c->u.prim.v;
    const Draw *d = &c->u.prim.d;
    int n = c->type == CMD_TRIANGLE ? 3 : c->type == CMD_POINT ? 1 : 2, i;
    memset(g, 0, sizeof *g);
    g->type = c->type;
    for (i = 0; i < n; i++) {
        g->xyz[i][0] = v[i].x;
        g->xyz[i][1] = v[i].y;
        g->xyz[i][2] = (int32_t)v[i].z;
    }
    g->sc[0] = d->scx0;
    g->sc[1] = d->scx1;
    g->sc[2] = d->scy0;
    g->sc[3] = d->scy1;
}

static void same_depth(const Cmd *c, Key *k) {
    Geo g;
    if (k->zrt < 0) {
        return;
    }
    if (!k->dwrite) {
        if (sLastZ.valid && sLastZ.zrt == k->zrt && k->pass != 2 && (k->dfunc == GL_GEQUAL || k->dfunc == GL_GREATER)) {
            geo_of(c, &g);
            if (g.type == sLastZ.g.type && memcmp(g.xyz, sLastZ.g.xyz, sizeof g.xyz) == 0 && g.sc[0] >= sLastZ.g.sc[0] &&
                g.sc[1] <= sLastZ.g.sc[1] && g.sc[2] >= sLastZ.g.sc[2] && g.sc[3] <= sLastZ.g.sc[3]) {
                k->dwrite = 1;
            }
        }
        return;
    }
    if (k->pass == 0 && (k->dfunc == GL_ALWAYS || k->dfunc == GL_GEQUAL || k->dfunc == GL_GREATER)) {
        sLastZ.valid = 1;
        sLastZ.zrt = k->zrt;
        geo_of(c, &sLastZ.g);
    } else if (sLastZ.zrt == k->zrt) {
        sLastZ.valid = 0;
    }
}

/* The texels a UV primitive reads along one axis, from its vertices' coordinates cmin..cmax (10.4) as the shader
   samples them: texel c >> 4 (nearest), or (c - 8) >> 4 and the next one (bilinear); an edge the primitive does not
   cover (a sprite's right / bottom one: `open`) is never sampled. Left as the wrap mode's whole range when REPEAT
   would move them. */
static void uv_span(uint32_t wm, int size, int mn, int mx, int cmin, int cmax, int open, int linear, uint32_t *lo, uint32_t *hi) {
    int a, b;
    Gs_TexRange(wm, size, mn, mx, lo, hi);
    if (wm > 1) {
        return;
    }
    if (open && cmax > cmin) {
        cmax--;
    }
    a = linear ? (cmin - 8) >> 4 : cmin >> 4;
    b = linear ? ((cmax - 8) >> 4) + 1 : cmax >> 4;
    if (wm == 1) { /* CLAMP */
        a = a < 0 ? 0 : a > size - 1 ? size - 1 : a;
        b = b < 0 ? 0 : b > size - 1 ? size - 1 : b;
    } else if (a < 0 || b > size - 1) {
        return;
    }
    *lo = (uint32_t)a;
    *hi = (uint32_t)b;
}

/* Texture reads of pages a target drew: that target goes back to GS memory first. A UV sprite or triangle reads only
   the texels its vertices' UV reach (uv_span), as gs_core.c's classify() for sprites: a copy of one buffer into
   another in strips then resolves its source once, not per strip, and glyphs from a font in a frame buffer's top byte
   do not resolve the buffers next to it. */
static void texture_hazard(const Draw *d, const Cmd *c) {
    uint64_t t0 = d->tex0 & 0x3FFFFFFFFull;
    uint64_t cl = (uint64_t)d->wms | (uint64_t)d->wmt << 2 | (uint64_t)d->minu << 4 | (uint64_t)d->maxu << 14 | (uint64_t)d->minv << 24 |
                  (uint64_t)d->maxv << 34;
    uint32_t mask = psm_bytes(d->tpsm);
    int i;

    if ((c->type == CMD_SPRITE || c->type == CMD_TRIANGLE) && d->fst) {
        const Vtx *v = c->u.prim.v;
        int n = c->type == CMD_SPRITE ? 2 : 3, k, iu0 = 0, iu1 = 0, iv0 = 0, iv1 = 0, linear = d->filter != 0;
        int openU = 0;
        uint32_t u0, u1, v0, v1;
        for (k = 1; k < n; k++) {
            iu0 = v[k].u < v[iu0].u ? k : iu0;
            iu1 = v[k].u > v[iu1].u ? k : iu1;
            iv0 = v[k].v < v[iv0].v ? k : iv0;
            iv1 = v[k].v > v[iv1].v ? k : iv1;
        }
        if (n == 2) { /* the corner with the largest U on the sprite's right edge: not drawn (GL's fill rule leaves out
                         right edges; which of the top and bottom edges it leaves out is the driver's: V keeps both) */
            openU = iu1 != iu0 && v[iu1].x > v[iu0].x;
        }
        uv_span(d->wms, d->tw, d->minu, d->maxu, v[iu0].u, v[iu1].u, openU, linear, &u0, &u1);
        uv_span(d->wmt, d->th, d->minv, d->maxv, v[iv0].v, v[iv1].v, 0, linear, &v0, &v1);
        memset(sTexPages.pages, 0, sizeof sTexPages.pages);
        Gs_PagesOf(sTexPages.pages, d->tbp, d->tbw, d->tpsm, u0, u1, v0, v1);
        sTexPages.valid = 0;
    } else if (!sTexPages.valid || sTexPages.tex0 != t0 || sTexPages.clamp != cl) {
        uint32_t u0, u1, v0, v1;
        Gs_TexRange(d->wms, d->tw, d->minu, d->maxu, &u0, &u1);
        Gs_TexRange(d->wmt, d->th, d->minv, d->maxv, &v0, &v1);
        memset(sTexPages.pages, 0, sizeof sTexPages.pages);
        Gs_PagesOf(sTexPages.pages, d->tbp, d->tbw, d->tpsm, u0, u1, v0, v1);
        sTexPages.tex0 = t0;
        sTexPages.clamp = cl;
        sTexPages.valid = 1;
    }
    sCause = 0;
    if (sDirtyEpoch != sCheckedEpoch || !sChecked) { /* the targets drew elsewhere since: the union of their pages */
        memset(sDirtyAny, 0, sizeof sDirtyAny);
        for (i = 0; i < MAX_RT; i++) {
            if (sRt[i].used && sRt[i].dirty) {
                int w;
                for (w = 0; w < 16; w++) {
                    sDirtyAny[w] |= sRt[i].pages[w];
                }
            }
        }
        sCheckedEpoch = sDirtyEpoch;
        sChecked = 1;
    }
    if (!set_overlaps(sDirtyAny, sTexPages.pages)) {
        return;
    }
    for (i = 0; i < MAX_RT; i++) {
        if (drew_over(i, sTexPages.pages, mask)) {
            resolve(i);
        }
    }
}

/* Target i is drawn in rows y0..y1. */
static void mark(int i, int y0, int y1) {
    Rt *r = &sRt[i];
    if (!r->snapDirty) {
        r->snapDirty = 1;
        r->sy0 = y0;
        r->sy1 = y1;
    } else {
        r->sy0 = y0 < r->sy0 ? y0 : r->sy0;
        r->sy1 = y1 > r->sy1 ? y1 : r->sy1;
    }
    if (!r->dirty) {
        r->dirty = 1;
        r->dy0 = y0;
        r->dy1 = y1;
        r->dpValid = 0;
        sDirtyEpoch++;
        return;
    }
    if (y0 < r->dy0 || y1 > r->dy1) {
        r->dy0 = y0 < r->dy0 ? y0 : r->dy0;
        r->dy1 = y1 > r->dy1 ? y1 : r->dy1;
        r->dpValid = 0;
    }
}

/* ---- T37: scaled and widescreen targets ---- */

/* Whether texture reads of targets go to their snapshots (full resolution, margins) instead of GS memory: at 1x 4:3
   with the game's flicker filter both are the same, and GS memory stays the path (the pictures of T30). Low latency
   needs them to see the game's copy of its frame. */
static int snapping(void) { return sScale > 1 || sMarg > 0 || !sDeflicker || sLowLat; }

/* Margins drawn in the window picture `gen` are shown up to the next one: later the target's margins are stale (a menu
   drawn over a fight's frame) and show black. */
static int fresh(uint32_t gen) { return gen != 0 && gen + 2 > sFrame; }

/* Target i's snapshot made current: the rows drawn since the last one copied (all of them the first time). */
static void snap_update(int i) {
    Rt *r = &sRt[i];
    int y0 = 0, y1 = r->th;
    if (r->snap != 0 && !r->snapDirty) {
        return;
    }
    flush();
    if (r->snap == 0) {
        gl.ActiveTexture(GL_TEXTURE0 + U_SRC);
        r->snap = texture(GL_RGBA16F, r->tw, r->th, GL_RGBA, GL_FLOAT, NULL);
        gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
        r->snapFbo = fbo_of(r->snap, 0);
    } else {
        y0 = r->sy0 * sScale;
        y1 = (r->sy1 + 1) * sScale < r->th ? (r->sy1 + 1) * sScale : r->th;
    }
    {
        GLuint src = fbo(i, -1);
        gl.BindFramebuffer(GL_READ_FRAMEBUFFER, src);
        gl.ReadBuffer(GL_COLOR_ATTACHMENT0);
        gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, r->snapFbo);
        gl.BlitFramebuffer(0, y0, r->tw, y1, 0, y0, r->tw, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    r->snapDirty = 0;
    r->snapGen = ++sSnapGen;
    r->snapMgen = r->mgen;
}

/* The colour target a texture is, in a format that reads the same values (32 / 24 bits over 32, 16 over 16): its
   snapshot is then sampled instead of GS memory. -1: none (other formats, CLUT textures, reinterpretations read GS
   memory as the GS would, at 1x). */
static int snap_source(const Draw *d) {
    int i;
    if (!snapping() || d->tbits < 16) {
        return -1;
    }
    for (i = 0; i < MAX_RT; i++) {
        const Rt *r = &sRt[i];
        if (r->used && !r->depth && r->bp == d->tbp && r->bw == (d->tbw != 0 ? d->tbw : 1) &&
            (r->psm == d->tpsm || (r->psm == 0x00 && d->tpsm == 0x01))) {
            snap_update(i);
            return i;
        }
    }
    return -1;
}

/* A sprite that covers the first or the last pixel of a widescreen target's row (w wide, margins m) goes on to the
   margin's edge; with UV (a copy of a widescreen target) U goes on at the same rate (negative U: the snapshot's left
   margin). 1 when it changed (o: the two vertices). */
static int extend_sprite(const Vtx *v, Vtx *o, int w, int m, int uv) {
    int a = v[0].x <= v[1].x ? 0 : 1, b = 1 - a, ch = 0;
    int64_t dx = (int64_t)v[b].x - v[a].x, du = (int64_t)v[b].u - v[a].u;
    o[0] = v[0];
    o[1] = v[1];
    if (dx <= 0) {
        return 0;
    }
    if (v[a].x <= 0) {
        int32_t nx = -m * 16;
        if (uv) {
            o[a].u = v[a].u - (int32_t)(du * (v[a].x - nx) / dx);
        }
        o[a].x = nx;
        ch = 1;
    }
    if (v[b].x > (w - 1) * 16) { /* it covers the last pixel (the game's copies end at w - 0.5) */
        int32_t nx = (w + m) * 16;
        if (uv) {
            o[b].u = v[b].u + (int32_t)(du * (nx - v[b].x) / dx);
        }
        o[b].x = nx;
        ch = 1;
    }
    return ch;
}

/* A sprite's edges on whole pixels, UV moved with them (scaled targets): it covers the GS pixels it covers at 1x and
   no part of a pixel more or less, so that sprites side by side (the game's copies in 64-pixel strips from x.0 to
   x+63.5) leave no gap between them at a finer scale. One axis: positions p (12.4), texels t (10.4). */
static void align_axis(int32_t *p0, int32_t *p1, int32_t *t0, int32_t *t1, int uv) {
    int32_t *a = *p0 <= *p1 ? p0 : p1, *b = a == p0 ? p1 : p0, *ta = a == p0 ? t0 : t1, *tb = a == p0 ? t1 : t0;
    int64_t d = (int64_t)*b - *a, dt = (int64_t)*tb - *ta;
    int32_t na = (*a + 15) & ~15, nb = (*b + 15) & ~15;
    if (d <= 0) {
        return;
    }
    if (uv) {
        *ta += (int32_t)(dt * (na - *a) / d);
        *tb += (int32_t)(dt * (nb - *b) / d);
    }
    *a = na;
    *b = nb;
}

/* The margin each side of a 640 x 448 picture for a display aspect ratio, in GS pixels as the window shows them
   (square): "16:9" 78, "21:9" 203, else 0 (4:3: the game's picture as before T37). */
int Gl_AspectMargin(const char *aspect) {
    int num = 0, den = 1;
    if (strcmp(aspect, "16:9") == 0) {
        num = 16;
        den = 9;
    } else if (strcmp(aspect, "21:9") == 0) {
        num = 21;
        den = 9;
    }
    return num == 0 ? 0 : (448 * num / den - 640 + 1) / 2;
}

static void put(const Vtx *v, uint32_t z, uint32_t rgba, uint32_t s, uint32_t t, uint32_t q, uint32_t fog, int32_t x, int32_t y, int fst) {
    GlVtx *o = &sV[sNv++];
    o->x = x;
    o->y = y;
    o->z = z;
    o->rgba = rgba;
    if (fst) {
        o->s = (uint32_t)s;
        o->t = (uint32_t)t;
        o->q = 0;
    } else {
        o->s = s;
        o->t = t;
        o->q = q;
    }
    o->fog = fog;
    o->state = sStBase + sNs - 1;
    (void)v;
}

static void early_present(int t);

static void prim(const Cmd *c) {
    const Draw *d = &c->u.prim.d;
    const Vtx *v = c->u.prim.v;
    uint32_t rows = (uint32_t)d->scy1 + 1, st[12];
    int kind = c->type == CMD_LINE ? 1 : c->type == CMD_POINT ? 2 : 0;
    int outcome = 0, newState, zneed = (d->ztest && d->ztst != 1) || d->zwrite, rt, zrt = -1, palRow = 0, need = c->type == CMD_SPRITE ? 6 : 3;
    int src = -1, wide = 0, present = 0;
    Key k;
    Vtx ext[2];
    uint32_t fpsm = d->fpsm;

    if (fpsm != 0x00 && fpsm != 0x01 && fpsm != 0x02 && fpsm != 0x0A) {
        fpsm = 0x00; /* the formats inside a 32-bit pixel: drawn as 32 bits */
    }
    sCause = 4;
    rt = rt_get(0, d->fbp, d->fbw, fpsm, rows);
    if (zneed) {
        zrt = rt_get(1, d->zbp, d->fbw, d->zpsm, rows);
        if (!sRt[rt].used || sRt[rt].bp != d->fbp || sRt[rt].depth) { /* the depth buffer lies over the frame */
            sGlStat.overlapZ++;
            rt_drop(zrt);
            zrt = -1;
            rt = rt_get(0, d->fbp, d->fbw, fpsm, rows);
        }
    }
    if (sCopyPend >= 0 && rt != sCopyPend) { /* the game copied its frame into the display buffer: the next frame begins */
        int t = sCopyPend;
        sCopyPend = -1;
        if (sLowLat && !sEarlyShown) {
            early_present(t);
        }
    }
    if (d->tex) {
        texture_hazard(d, c);
        if (d->tbits <= 8) {
            palRow = pal_row(d);
        }
        src = snap_source(d);
    }
    /* The game's flicker filter: its frame (448 lines) copied into the display buffer (224) by UV sprites, two lines
       into one. The display buffer remembers whose snapshot it holds (video.deflicker 0 shows that one instead). */
    if (src >= 0 && src != rt && c->type == CMD_SPRITE && d->fst) {
        int dy = v[1].y - v[0].y, dv = v[1].v - v[0].v;
        dy = dy < 0 ? -dy : dy;
        dv = dv < 0 ? -dv : dv;
        if (dy > 0 && dv - 2 * dy <= 16 && 2 * dy - dv <= 16) {
            sRt[rt].copyOf = src + 1;
            sRt[rt].copyGen = sRt[src].snapGen;
            if (sCopyPend != rt) { /* a copy begins: the frame is finished */
                sCopyPend = rt;
                sCopyHits = 0;
                sEarlyShown = 0;
                if (sLowLat && !sDeflicker) {
                    early_present(rt); /* the frame itself is shown: present it now */
                }
            }
            /* with the filter, the copy's last cell (two passes over the last pixel) finishes the picture */
            if ((v[0].x > (sRt[rt].w - 1) * 16 || v[1].x > (sRt[rt].w - 1) * 16) &&
                (v[0].y > ((int)sRt[rt].rows - 1) * 16 || v[1].y > ((int)sRt[rt].rows - 1) * 16) && ++sCopyHits == 2) {
                present = sLowLat && !sEarlyShown;
            }
        } else {
            sRt[rt].copyOf = 0;
        }
    } else {
        sRt[rt].copyOf = 0;
    }
    /* Widescreen: what the game draws over the whole width also goes into the margins. Triangles, lines and points
       already lie there (the VU1 programs clip at the GS's coordinate range, not the screen): the scissor opens. A
       sprite at an edge goes on (untextured: clears, fades; a copy of a widescreen frame: its margins), other sprites
       (the HUD, menus' pictures) stay in the 4:3 middle. */
    if (sRt[rt].m > 0 && d->scx0 == 0 && d->scx1 >= sRt[rt].w - 1) {
        if (c->type != CMD_SPRITE) {
            wide = 1;
        } else if (!d->tex || (src >= 0 && d->fst && fresh(sRt[src].snapMgen))) {
            wide = extend_sprite(v, ext, sRt[rt].w, sRt[rt].m, d->tex);
        }
    }
    if (c->type == CMD_SPRITE && (wide || sScale > 1)) {
        if (!wide) {
            ext[0] = v[0];
            ext[1] = v[1];
        }
        if (sScale > 1) {
            align_axis(&ext[0].x, &ext[1].x, &ext[0].u, &ext[1].u, d->tex && d->fst);
            align_axis(&ext[0].y, &ext[1].y, &ext[0].v, &ext[1].v, d->tex && d->fst);
        }
        v = ext;
    }
    if (wide) { /* drawn into the margins: they are shown with this picture */
        int n = c->type == CMD_TRIANGLE ? 3 : c->type == CMD_POINT ? 1 : 2, i;
        for (i = 0; i < n; i++) {
            if (v[i].x < 0 || v[i].x > sRt[rt].w * 16) {
                sRt[rt].mgen = sFrame;
                break;
            }
        }
    }
    if (d->atest) {
        uint32_t lo, hi;
        alpha_range(d, v, c->type, palRow, &lo, &hi);
        outcome = alpha_outcome(d->atst, d->aref, lo, hi);
    }
    key_of(d, rt, zrt, kind, outcome, &k);
    k.src = src;
    if (k.cmask == 0 && !k.dwrite) {
        return; /* writes nothing (every pixel fails, KEEP) */
    }
    if (sNv + (uint32_t)need > MAX_VERTS || sNs >= MAX_STATES || sNb >= MAX_BATCHES) {
        flush();
    }
    same_depth(c, &k);
    state_of(d, palRow, st);
    if (outcome != 0) {
        st[0] &= ~(1u << 10); /* no test in the shader */
    }
    { /* the scissor in the target's pixels, margin included */
        int m = sRt[rt].m, x0 = wide ? -m : d->scx0, x1 = wide ? sRt[rt].w - 1 + m : d->scx1;
        st[6] = (uint32_t)(x0 + m) | (uint32_t)(x1 + m) << 16;
    }
    if (src >= 0) {
        st[0] |= 1u << 31; /* from the snapshot of target src */
        st[9] = (uint32_t)sRt[src].m;
    }
    newState = sNs == 0 || memcmp(st, sS[sNs - 1], sizeof st) != 0;
    if (sNb == 0 || memcmp(&k, &sB[sNb - 1].k, sizeof k) != 0 || (k.pass == 2 && newState)) {
        sB[sNb].k = k;
        sB[sNb].first = sNv;
        sB[sNb].count = 0;
        sNb++;
    }
    sB[sNb - 1].count += (uint32_t)need;
    if (newState) {
        memcpy(sS[sNs++], st, sizeof st);
    }
    {
        int n = c->type == CMD_TRIANGLE ? 3 : c->type == CMD_POINT ? 1 : 2, i, y0 = v[0].y >> 4, y1 = (v[0].y + 15) >> 4;
        for (i = 1; i < n; i++) {
            y0 = (v[i].y >> 4) < y0 ? v[i].y >> 4 : y0;
            y1 = ((v[i].y + 15) >> 4) > y1 ? (v[i].y + 15) >> 4 : y1;
        }
        y0 = y0 < d->scy0 ? d->scy0 : y0;
        y1 = y1 > d->scy1 ? d->scy1 : y1;
        y1 = y1 >= (int)sRt[rt].rows ? (int)sRt[rt].rows - 1 : y1;
        if (y0 <= y1) {
            mark(rt, y0, y1);
            if (zrt >= 0 && d->zwrite) {
                mark(zrt, y0, y1);
            }
        }
    }
    switch (c->type) {
    case CMD_TRIANGLE: {
        int i;
        for (i = 0; i < 3; i++) {
            put(&v[i], v[i].z, v[i].rgba, d->fst ? (uint32_t)v[i].u : v[i].s, d->fst ? (uint32_t)v[i].v : v[i].t, v[i].q, v[i].f, v[i].x, v[i].y, d->fst);
        }
        break;
    }
    case CMD_SPRITE: { /* two triangles; colour, Z, fog and Q of the second vertex */
        const Vtx *a = &v[0], *b = &v[1];
        uint32_t sa = d->fst ? (uint32_t)a->u : a->s, ta = d->fst ? (uint32_t)a->v : a->t;
        uint32_t sb = d->fst ? (uint32_t)b->u : b->s, tb = d->fst ? (uint32_t)b->v : b->t;
        put(a, b->z, b->rgba, sa, ta, b->q, b->f, a->x, a->y, d->fst);
        put(a, b->z, b->rgba, sb, ta, b->q, b->f, b->x, a->y, d->fst);
        put(a, b->z, b->rgba, sa, tb, b->q, b->f, a->x, b->y, d->fst);
        put(a, b->z, b->rgba, sb, ta, b->q, b->f, b->x, a->y, d->fst);
        put(a, b->z, b->rgba, sa, tb, b->q, b->f, a->x, b->y, d->fst);
        put(a, b->z, b->rgba, sb, tb, b->q, b->f, b->x, b->y, d->fst);
        break;
    }
    case CMD_LINE:
        put(&v[0], v[1].z, v[1].rgba, v[0].s, v[0].t, v[0].q, v[1].f, v[0].x, v[0].y, d->fst);
        put(&v[1], v[1].z, v[1].rgba, v[1].s, v[1].t, v[1].q, v[1].f, v[1].x, v[1].y, d->fst);
        break;
    default:
        put(&v[0], v[0].z, v[0].rgba, v[0].s, v[0].t, v[0].q, v[0].f, v[0].x, v[0].y, d->fst);
        break;
    }
    if (present) {
        early_present(rt);
    }
}

/* ------------------------------------------------------------------------------------------------ memory operations */

static void upload(const Cmd *c) {
    uint64_t bb = c->u.upload.bitbltbuf;
    uint32_t dbp = (bb >> 32) & 0x3FFF, dbw = (bb >> 48) & 0x3F, dpsm = (bb >> 56) & 0x3F, set[16] = {0}, mask = psm_bytes(dpsm);
    int p;

    flush();
    if (c->u.upload.tw == 0 || c->u.upload.th == 0) {
        Gs_CpuUpload(c);
        return;
    }
    Gs_PagesOf(set, dbp, dbw, dpsm, c->u.upload.tx, c->u.upload.tx + c->u.upload.tw - 1, c->u.upload.ty, c->u.upload.ty + c->u.upload.th - 1);
    sCause = 2;
    drop_over(set, mask, -1);
    readback(set, mask);
    Gs_CpuUpload(c);
    sMemGen++;
    for (p = 0; p < 512; p++) {
        if ((set[p >> 5] >> (p & 31)) & 1) {
            sCpuDirty[p] |= (uint8_t)mask;
        }
    }
    sTexPages.valid = 0;
}

static void copy(const Cmd *c) {
    uint64_t bb = c->u.copy.bitbltbuf, tp = c->u.copy.trxpos;
    uint32_t sbp = bb & 0x3FFF, sbw = (bb >> 16) & 0x3F, spsm = (bb >> 24) & 0x3F;
    uint32_t dbp = (bb >> 32) & 0x3FFF, dbw = (bb >> 48) & 0x3F, dpsm = (bb >> 56) & 0x3F;
    uint32_t sx = tp & 0x7FF, sy = (tp >> 16) & 0x7FF, src[16] = {0}, dst[16] = {0}, mask = psm_bytes(dpsm);
    int p;

    flush();
    if (c->u.copy.tw == 0 || c->u.copy.th == 0) {
        return;
    }
    Gs_PagesOf(src, sbp, sbw, spsm, sx, sx + c->u.copy.tw - 1, sy, sy + c->u.copy.th - 1);
    Gs_PagesOf(dst, dbp, dbw, dpsm, c->u.copy.tx, c->u.copy.tx + c->u.copy.tw - 1, c->u.copy.ty, c->u.copy.ty + c->u.copy.th - 1);
    resolve_over(src, 0xF);
    readback(src, 0xF);
    drop_over(dst, mask, -1);
    readback(dst, mask);
    Gs_CpuCopy(c);
    sMemGen++;
    for (p = 0; p < 512; p++) {
        if ((dst[p >> 5] >> (p & 31)) & 1) {
            sCpuDirty[p] |= (uint8_t)mask;
        }
    }
    sTexPages.valid = 0;
}

static uint64_t clut_hash(void) {
    uint64_t h = 14695981039346656037ull;
    uint32_t i, w;
    for (i = 0; i < 512; i += 2) {
        w = (uint32_t)sClut[i] | (uint32_t)sClut[i + 1] << 16;
        h = (h ^ w) * 1099511628211ull;
    }
    return h & 0x7FFFFFFFFFFFFFFFull; /* the top bit: a buffer the GPU holds part of */
}

static void clut(const Cmd *c) {
    uint64_t t0 = c->u.clut.t0, tc = c->u.clut.texclut;
    uint32_t set[16] = {0}, cbp = (t0 >> 37) & 0x3FFF, cpsm = (t0 >> 51) & 15, csm = (t0 >> 55) & 1;

    if (csm) {
        Gs_PagesOf(set, cbp, (uint32_t)(tc & 0x3F), cpsm, 0, 0x7FF, (uint32_t)((tc >> 12) & 0x3FF), (uint32_t)((tc >> 12) & 0x3FF));
    } else {
        Gs_PagesOf(set, cbp, 1, cpsm, 0, 15, 0, 15);
    }
    sCause = 1;
    resolve_over(set, 0xF);
    {
        uint32_t w, gpu = 0, n = Gs_PsmBits((uint32_t)(t0 >> 20) & 0x3F) == 8 ? 256 : 16, halves[16];
        for (w = 0; w < 16 && !gpu; w++) {
            uint32_t bits = set[w];
            while (bits != 0 && !gpu) {
                gpu = sGpuNewer[w * 32 + (uint32_t)__builtin_ctz(bits)] != 0;
                bits &= bits - 1;
            }
        }
        clut_halves(cpsm, (uint32_t)(t0 >> 56) & 31, n, halves);
        if (gpu) { /* pages the GPU drew: the load runs on the GPU, from the mirror, instead of reading them back */
            uint64_t key = t0 & 0xFFFFFFE003F00000ull, ktc = csm ? tc : 0;
            uint32_t r;
            push();
            if (sLastGpuLoad.after == sClutHash && sLastGpuLoad.key == key && sLastGpuLoad.ktc == ktc && sLastGpuLoad.mirrorGen == sMirrorGen) {
                return;
            }
            r = ++sClutSerial;
            gl.BindFramebuffer(GL_FRAMEBUFFER, sClutFbo[r & 1]);
            gl.Viewport(0, 0, 512, CLUT_ROWS);
            gl.UseProgram(sProgClutLoad);
            plain_state();
            clut_uniforms(uClutLoad.row, uClutLoad.cpu, uClutLoad.unk);
            gl.Uniform4uiv(uClutLoad.arg, 1, (const GLuint[]){cbp, cpsm, n | (uint32_t)((t0 >> 56) & 31) << 16 | csm << 31,
                                                               (GLuint)(tc & 0x3FFFFF)});
            rect_pass(uClutLoad.rect, uClutLoad.target, 0, (int)((r >> 1) % CLUT_ROWS), 512, (int)((r >> 1) % CLUT_ROWS) + 1, 512, CLUT_ROWS);
            sClutGpu = r;
            sClutUnkAny = 1;
            for (w = 0; w < 16; w++) {
                sClutUnk[w] |= halves[w];
            }
            sClutHash = 0x8000000000000000ull | r;
            sGlStat.clutGpu++;
            sLastGpuLoad.key = key;
            sLastGpuLoad.ktc = ktc;
            sLastGpuLoad.after = sClutHash;
            sLastGpuLoad.mirrorGen = sMirrorGen;
            return;
        }
        if (sClutUnkAny) { /* a load from the CPU copy: its halves are known again */
            sClutUnkAny = 0;
            for (w = 0; w < 16; w++) {
                sClutUnk[w] &= ~halves[w];
                sClutUnkAny |= sClutUnk[w] != 0;
            }
            Gs_ClutFill(sClut, t0, tc);
            sClutHash = sClutUnkAny ? 0x8000000000000000ull | ++sClutSerial : clut_hash();
            return;
        }
    }
    /* The game sends TEX0 with CLD 1 before every batch, alternating a few CLUTs: a load is a function of its fields,
       of GS memory and of the buffer before it (4-bit loads fill part of it), so the buffers it gave are kept. */
    {
        static struct { uint64_t key, ktc, before, after; uint32_t gen; int valid; uint16_t buf[512]; } lru[8];
        static uint32_t next;
        uint64_t key = t0 & 0xFFFFFFE003F00000ull, ktc = csm ? tc : 0, before = sClutHash;
        int k;
        for (k = 0; k < 8; k++) {
            if (lru[k].valid && lru[k].key == key && lru[k].ktc == ktc && lru[k].gen == sMemGen && lru[k].before == before) {
                if (lru[k].after != sClutHash) {
                    memcpy(sClut, lru[k].buf, sizeof sClut);
                    sClutHash = lru[k].after;
                }
                return;
            }
        }
        Gs_ClutFill(sClut, t0, tc);
        sClutHash = clut_hash();
        k = (int)(next++ & 7);
        lru[k].valid = 1;
        lru[k].key = key;
        lru[k].ktc = ktc;
        lru[k].gen = sMemGen;
        lru[k].before = before;
        lru[k].after = sClutHash;
        memcpy(lru[k].buf, sClut, sizeof sClut);
    }
}

/* ------------------------------------------------------------------------------------------------ display */

static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}

/* One more event at host time t for a pacing histogram: the interval since the last one. */
static void pace_add(Pace *p, uint64_t t) {
    if (p->last != 0 && t > p->last) {
        uint64_t d = t - p->last;
        p->n++;
        p->sum += d;
        p->max = d > p->max ? d : p->max;
        p->late += d * 2 > (uint64_t)VBLANK_NS * 3; /* more than 1.5 VBlanks */
        if (d / PACE_BIN_NS < PACE_BINS) {
            p->bins[d / PACE_BIN_NS]++;
        } else {
            p->over++;
        }
    }
    p->last = t;
}

/* The interval at percentile q of a pacing histogram (the middle of its bin; the max beyond the bins), in ns. */
static uint64_t pace_pct(const Pace *p, unsigned q) {
    uint32_t want = (uint32_t)(((uint64_t)p->n * q + 99) / 100), seen = 0, i;
    for (i = 0; i < PACE_BINS && want > 0; i++) {
        seen += p->bins[i];
        if (seen >= want) {
            return (uint64_t)i * PACE_BIN_NS + PACE_BIN_NS / 2;
        }
    }
    return p->max;
}

/* The display picture (sDispFbo, w x h) letterboxed into the window's back buffer, then presented (with the overlay,
   plat_window.c); low latency waits until the GPU is done (the driver queues no frame). `since`: the queue time of the
   VBlank after which the shown frame was complete (0: unknown), for the report's latency. */
static int sShownW, sShownH; /* the last picture shown in the window */
static void window_blit(int w, int h, uint64_t since) {
    int ww = 0, wh = 0, dw, dh, x0, y0;
    Port_WindowGlSize(&ww, &wh);
    if (ww <= 0 || wh <= 0) {
        return;
    }
    if ((int64_t)ww * h > (int64_t)wh * w) {
        dh = wh;
        dw = (int)((int64_t)wh * w / h);
    } else {
        dw = ww;
        dh = (int)((int64_t)ww * h / w);
    }
    x0 = (ww - dw) / 2;
    y0 = (wh - dh) / 2;
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.DrawBuffer(GL_BACK);
    gl.Viewport(0, 0, ww, wh);
    {
        static const uint32_t zero[4] = {0, 0, 0, 0};
        gl.ClearBufferfv(GL_COLOR, 0, zero);
    }
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, sDispFbo);
    gl.ReadBuffer(GL_COLOR_ATTACHMENT0);
    gl.BlitFramebuffer(0, 0, w, h, x0, y0 + dh, x0 + dw, y0, GL_COLOR_BUFFER_BIT, sSmooth ? GL_LINEAR : GL_NEAREST); /* line 0 at the top */
    {
        uint64_t t0 = now_ns(), t1;
        Port_WindowGlSwap();
        if (sLowLat) {
            gl.Finish();
        }
        t1 = now_ns();
        if (sPaceOn && since != 0) {
            pace_add(&sPacePres, t1);
            sSwapSum += t1 - t0;
            sSwapMax = t1 - t0 > sSwapMax ? t1 - t0 : sSwapMax;
        }
    }
    sPres.presents += since != 0;
    if (since != 0 && sFrame > 600) { /* from the first fight's VBlanks on (not the boot's loading) */
        uint64_t t = now_ns(), lat = t > since ? t - since : 0;
        int e = sEarlyShown == 2;
        sPres.n[e]++;
        sPres.latSum[e] += lat;
        sPres.latMax[e] = lat > sPres.latMax[e] ? lat : sPres.latMax[e];
    }
}

/* The last picture again (window thread, between GS commands): the overlay is open while the game shows nothing new
   (paused). 0 when there is no picture yet. */
int Gl_Redisplay(void) {
    if (sShownW <= 0 || sShownH <= 0) {
        return 0;
    }
    window_blit(sShownW, sShownH, 0);
    return 1;
}

/* The colour target a read circuit shows (its buffer's address, width and format), or -1. */
static int disp_rt(const Circuit *c) {
    int i;
    for (i = 0; i < MAX_RT; i++) {
        if (sRt[i].used && !sRt[i].depth && sRt[i].bp == c->fbp && sRt[i].bw == (c->fbw != 0 ? c->fbw : 1) && sRt[i].psm == c->psm) {
            return i;
        }
    }
    return -1;
}

/* Draws the picture of these display registers into sDispFbo: (width + 2 margins) x lines GS pixels, sScale GL pixels
   each (*pw x *ph). Scaled or widescreen, a circuit that shows a target reads the target (or, without the flicker
   filter, the snapshot of the frame the display buffer is a copy of); otherwise GS memory as the GS would. */
static void display(const DispRegs *regs, int *pw, int *ph) {
    uint64_t pmode = regs->pmode;
    int en1 = (int)(pmode & 1), en2 = (int)(pmode >> 1) & 1, k;
    Circuit ci[2];
    const Circuit *main;
    GLuint ca[8];
    GLint cb[8], ut[8] = {0, 0, 0, 1, 0, 0, 0, 1};
    int w, h, W, H, z = (regs->smode2 & 3) == 3 ? 2 : 1;

    flush();
    Gs_Circuit(regs, 0, &ci[0]);
    Gs_Circuit(regs, 1, &ci[1]);
    main = !en1 && en2 ? &ci[1] : &ci[0];
    w = (int)main->width;
    h = (int)main->lines;
    for (k = 0; k < 2; k++) {
        uint32_t set[16] = {0};
        if (k == 0 ? !en1 : !en2) {
            continue;
        }
        Gs_PagesOf(set, ci[k].fbp, ci[k].fbw, ci[k].psm, ci[k].dbx, ci[k].dbx + ci[k].width - 1, ci[k].dby, ci[k].dby + ci[k].lines / (uint32_t)z);
        sCause = 3;
        resolve_over(set, 0xF);
    }
    push();
    for (k = 0; k < 2; k++) {
        int t = snapping() ? disp_rt(&ci[k]) : -1;
        ca[k * 4 + 0] = ci[k].fbp;
        ca[k * 4 + 1] = ci[k].fbw;
        ca[k * 4 + 2] = ci[k].psm;
        ca[k * 4 + 3] = ci[k].dbx | ci[k].dby << 16;
        cb[k * 4 + 0] = (GLint)ci[k].x0;
        cb[k * 4 + 1] = (GLint)ci[k].y0;
        cb[k * 4 + 2] = (GLint)ci[k].width;
        cb[k * 4 + 3] = (GLint)ci[k].lines;
        ut[k * 4 + 3] = z;
        if (t >= 0) {
            const Rt *r = &sRt[t], *f = r->copyOf > 0 ? &sRt[r->copyOf - 1] : NULL;
            GLuint tex = r->tex;
            int whole = ci[k].dbx == 0 && (int)ci[k].width == r->w;
            ut[k * 4 + 0] = 1;
            ut[k * 4 + 1] = r->m;
            ut[k * 4 + 2] = whole && fresh(r->mgen);
            if (!sDeflicker && f != NULL && f->snap != 0 && f->snapGen == r->copyGen && z == 2 && ci[k].dby == 0) {
                tex = f->snap; /* the frame before the game's filter: its lines one to one */
                ca[k * 4 + 2] = f->psm;
                ut[k * 4 + 1] = f->m;
                ut[k * 4 + 2] = whole && fresh(f->snapMgen);
                ut[k * 4 + 3] = 1;
            }
            gl.ActiveTexture(GL_TEXTURE0 + U_DSP0 + (GLenum)k);
            gl.BindTexture(GL_TEXTURE_2D, tex);
        }
    }
    gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
    W = (w + 2 * sMarg) * sScale;
    H = h * sScale;
    if (W > sDispW || H > sDispH) { /* the picture grew (scale, aspect): a texture for it */
        gl.DeleteFramebuffers(1, &sDispFbo);
        gl.DeleteTextures(1, &sDispTex);
        sDispW = W > sDispW ? W : sDispW;
        sDispH = H > sDispH ? H : sDispH;
        gl.ActiveTexture(GL_TEXTURE0 + U_SRC);
        sDispTex = texture(GL_RGBA8, sDispW, sDispH, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
        sDispFbo = fbo_of(sDispTex, 0);
    }
    gl.BindFramebuffer(GL_FRAMEBUFFER, sDispFbo);
    gl.Viewport(0, 0, sDispW, sDispH);
    gl.UseProgram(sProgDisplay);
    plain_state();
    gl.Uniform4uiv(uDisp.ca, 2, ca);
    gl.Uniform4iv(uDisp.cb, 2, cb);
    gl.Uniform4iv(uDisp.tex, 2, ut);
    gl.Uniform2i(uDisp.out, sScale, sMarg);
    gl.Uniform4uiv(uDisp.mode, 1, (const GLuint[]){(GLuint)pmode, (GLuint)regs->bgcolor & 0xFFFFFF, (GLuint)z, 0});
    gl.Uniform4iv(uDisp.main, 1, (const GLint[]){(GLint)main->x0, (GLint)main->y0, w, 0});
    rect_pass(uDisp.rect, uDisp.target, 0, 0, W, H, sDispW, sDispH);
    sGlStat.shows++;
    *pw = W;
    *ph = H;
}

/* The video settings (plat_settings.c, the overlay's Video page), read at each window picture. A new scale or aspect
   writes every target back into GS memory and drops it: the next draws make them again at the new size. */
static void apply_settings(void) {
    char a[16];
    int scale = Settings_GetInt("video.resolution"), marg, i;
    Settings_GetStr("video.aspect", a, sizeof a);
    marg = Gl_AspectMargin(a);
    sDeflicker = Settings_GetInt("video.deflicker");
    sSmooth = Settings_GetInt("video.smooth");
    sLowLat = Settings_GetInt("video.low_latency");
    if (scale != sScale || marg != sMarg) {
        for (i = 0; i < MAX_RT; i++) {
            rt_drop(i);
        }
        sScale = scale;
        sMarg = marg;
    }
}

/* Low latency: the game copied its finished frame into the display buffer (target t) and goes on with the next one:
   present it now instead of at the next VBlank (the display registers of the last one). */
static void early_present(int t) {
    Circuit c0;
    int w, h;
    if (!sLastRegsValid || gGsGlSkip) {
        return;
    }
    Gs_Circuit(&sLastRegs, 0, &c0);
    if (disp_rt(&c0) != t) {
        return;
    }
    display(&sLastRegs, &w, &h);
    sShownW = w;
    sShownH = h;
    sEarlyShown = 2;
    window_blit(w, h, sShowNs[0]);
    sEarlyShown = 1;
    sPres.early++;
}

static void show(const Cmd *c) {
    const DispRegs *regs = &c->u.show.regs;
    int w, h;

    if (c->u.show.window) {
        apply_settings();
        if (sPaceFrom == 0) {
            const char *e = getenv("TTT_PACE_FROM");
            sPaceFrom = e != NULL && strtoul(e, NULL, 0) != 0 ? (uint32_t)strtoul(e, NULL, 0) : 600;
        }
        sPaceOn |= c->u.show.vblank >= sPaceFrom;
        if (sPaceOn) {
            uint16_t f = gGsFrameAtVb[c->u.show.vblank & 255];
            pace_add(&sPaceQueue, c->u.show.ns);
            sPaceDropped += gGsGlSkip != 0;
            sPaceRepeats += sPaceShows != 0 && f == sPaceLastFrame;
            sPaceLastFrame = f;
            sPaceShows++;
        }
        if (sEarlyShown && sLastRegsValid && memcmp(regs, &sLastRegs, sizeof *regs) == 0) {
            /* low latency: this picture went out when the game copied it */
            sEarlyShown = 0;
            sPres.skipped++;
            sShowNs[1] = sShowNs[0];
            sShowNs[0] = c->u.show.ns;
            sFrame++;
            return;
        }
        sEarlyShown = 0;
        display(regs, &w, &h);
        sLastRegs = *regs;
        sLastRegsValid = 1;
        sShowNs[1] = sShowNs[0];
        sShowNs[0] = c->u.show.ns;
        sFrame++;
        if (gGsGlSkip) {
            return; /* a newer picture is queued */
        }
        sShownW = w;
        sShownH = h;
        window_blit(w, h, sShowNs[1]);
        return;
    }
    display(regs, &w, &h);
    if (Gs_DumpDir() != NULL) {
        uint8_t *rgba = malloc((size_t)w * (size_t)h * 4), *rgb = malloc((size_t)w * (size_t)h * 3);
        char name[4096];
        size_t i;
        if (rgba != NULL && rgb != NULL) {
            gl.BindFramebuffer(GL_READ_FRAMEBUFFER, sDispFbo);
            gl.ReadBuffer(GL_COLOR_ATTACHMENT0);
            gl.ReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
            for (i = 0; i < (size_t)w * (size_t)h; i++) {
                rgb[i * 3] = rgba[i * 4];
                rgb[i * 3 + 1] = rgba[i * 4 + 1];
                rgb[i * 3 + 2] = rgba[i * 4 + 2];
            }
            snprintf(name, sizeof name, "%s/frame_%05u.png", Gs_DumpDir(), (unsigned)c->u.show.vblank);
            if (!Gs_WritePng(name, (uint32_t)w, (uint32_t)h, rgb)) {
                fprintf(stderr, "ttt: cannot write %s\n", name);
            }
        }
        free(rgba);
        free(rgb);
    }
}

/* ------------------------------------------------------------------------------------------------ entry points */

void Gl_Exec(const Cmd *c) {
    switch (c->type) {
    case CMD_POINT:
    case CMD_LINE:
    case CMD_TRIANGLE:
    case CMD_SPRITE:
        prim(c);
        return;
    case CMD_UPLOAD: upload(c); break;
    case CMD_COPY: copy(c); break;
    case CMD_CLUT: clut(c); break;
    case CMD_SHOW: show(c); break;
    default: break;
    }
}

int Gl_Init(void *(*getproc)(const char *name), char *why, int whylen) {
    size_t i;
    const unsigned char *ver, *ren;
    uint16_t *tab, *inv;
    int k;

    for (i = 0; i < sizeof kGlFuncs / sizeof kGlFuncs[0]; i++) {
        void *f = getproc(kGlFuncs[i].name);
        if (f == NULL) {
            snprintf(why, (size_t)whylen, "no %s", kGlFuncs[i].name);
            return 0;
        }
        memcpy((char *)&gl + kGlFuncs[i].offset, &f, sizeof f);
    }
    ver = gl.GetString(GL_VERSION);
    ren = gl.GetString(GL_RENDERER);
    sProgDraw = program(kDrawVs, kDrawFs, kLib, why, whylen);
    sProgResolve = sProgDraw ? program(kRectVs, kResolveFs, NULL, why, whylen) : 0;
    sProgLoad = sProgResolve ? program(kRectVs, kLoadFs, kLib, why, whylen) : 0;
    sProgStage = sProgLoad ? program(kRectVs, kStageFs, NULL, why, whylen) : 0;
    sProgDisplay = sProgStage ? program(kRectVs, kDisplayFs, kLib, why, whylen) : 0;
    {
        static char loadFs[sizeof kClutLib + sizeof kClutLoadFs], expandFs[sizeof kClutLib + sizeof kClutExpandFs];
        snprintf(loadFs, sizeof loadFs, "%s%s", kClutLib, kClutLoadFs);
        snprintf(expandFs, sizeof expandFs, "%s%s", kClutLib, kClutExpandFs);
        sProgClutLoad = sProgDisplay ? program(kRectVs, loadFs, kLib, why, whylen) : 0;
        sProgClutExpand = sProgClutLoad ? program(kRectVs, expandFs, kLib, why, whylen) : 0;
    }
    if (sProgClutExpand == 0) {
        return 0;
    }
    samplers(sProgClutLoad);
    samplers(sProgClutExpand);
    {
        static const char *const names[6] = {"uRect", "uTarget", "uRow", "uCpu", "uUnk", NULL};
        GLint *l = &uClutLoad.rect, *x = &uClutExpand.rect;
        int k;
        for (k = 0; k < 5; k++) {
            l[k] = gl.GetUniformLocation(sProgClutLoad, names[k]);
            x[k] = gl.GetUniformLocation(sProgClutExpand, names[k]);
        }
        uClutLoad.arg = gl.GetUniformLocation(sProgClutLoad, "uLd");
        uClutExpand.arg = gl.GetUniformLocation(sProgClutExpand, "uEx");
    }
    samplers(sProgDraw);
    samplers(sProgResolve);
    samplers(sProgLoad);
    samplers(sProgStage);
    samplers(sProgDisplay);
    uDraw.size = gl.GetUniformLocation(sProgDraw, "uSize");
    uDraw.zbits = gl.GetUniformLocation(sProgDraw, "uZBits");
    uDraw.zmax = gl.GetUniformLocation(sProgDraw, "uZMax");
    uDraw.pass = gl.GetUniformLocation(sProgDraw, "uPass");
    uDraw.scale = gl.GetUniformLocation(sProgDraw, "uScale");
    uDraw.marg = gl.GetUniformLocation(sProgDraw, "uMarg");
    uResolve.sm = gl.GetUniformLocation(sProgResolve, "uSM");
    uLoad.sm = gl.GetUniformLocation(sProgLoad, "uSM");
    uDisp.tex = gl.GetUniformLocation(sProgDisplay, "uTex");
    uDisp.out = gl.GetUniformLocation(sProgDisplay, "uOut");
    uResolve.rect = gl.GetUniformLocation(sProgResolve, "uRect");
    uResolve.target = gl.GetUniformLocation(sProgResolve, "uTarget");
    uResolve.buf = gl.GetUniformLocation(sProgResolve, "uBuf");
    uLoad.rect = gl.GetUniformLocation(sProgLoad, "uRect");
    uLoad.target = gl.GetUniformLocation(sProgLoad, "uTarget");
    uLoad.buf = gl.GetUniformLocation(sProgLoad, "uBuf");
    uStage.rect = gl.GetUniformLocation(sProgStage, "uRect");
    uStage.target = gl.GetUniformLocation(sProgStage, "uTarget");
    uDisp.rect = gl.GetUniformLocation(sProgDisplay, "uRect");
    uDisp.target = gl.GetUniformLocation(sProgDisplay, "uTarget");
    uDisp.ca = gl.GetUniformLocation(sProgDisplay, "uCa");
    uDisp.cb = gl.GetUniformLocation(sProgDisplay, "uCb");
    uDisp.mode = gl.GetUniformLocation(sProgDisplay, "uMode");
    uDisp.main = gl.GetUniformLocation(sProgDisplay, "uMain");

    /* the page tables, forward (128 wide: 32 / 16 / 16S / 8 / 4 bits at rows 0 / 32 / 96 / 160 / 224) and inverse
       (64 wide: the pixel x | y << 8 of each word of a 32-bit page at rows 0..31, of each half word of a 16 / 16S
       page at rows 32..95 / 96..159) */
    tab = calloc(128 * 352, 2);
    inv = calloc(64 * 160, 2);
    sPal = calloc(PAL_ROWS, sizeof *sPal);
    sV = malloc((size_t)MAX_VERTS * sizeof *sV);
    sS = malloc((size_t)MAX_STATES * sizeof *sS);
    sB = malloc((size_t)MAX_BATCHES * sizeof *sB);
    if (tab == NULL || inv == NULL || sPal == NULL || sV == NULL || sS == NULL || sB == NULL) {
        snprintf(why, (size_t)whylen, "out of memory");
        return 0;
    }
    {
        static const int kW[5] = {64, 64, 64, 128, 128}, kH[5] = {32, 64, 64, 64, 128}, kRow[5] = {0, 32, 96, 160, 224};
        for (k = 0; k < 5; k++) {
            const uint16_t *t = Gs_PageTable(k);
            int x, y;
            for (y = 0; y < kH[k]; y++) {
                for (x = 0; x < kW[k]; x++) {
                    uint16_t o = t[y * kW[k] + x];
                    tab[(kRow[k] + y) * 128 + x] = o;
                    if (k == 0) {
                        inv[o] = (uint16_t)(x | y << 8);
                    } else if (k <= 2) {
                        inv[(k == 1 ? 32 : 96) * 64 + o] = (uint16_t)(x | y << 8);
                    }
                }
            }
        }
    }
    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
    gl.ActiveTexture(GL_TEXTURE0 + U_TAB);
    sTabTex = texture(GL_R16UI, 128, 352, GL_RED_INTEGER, GL_UNSIGNED_SHORT, tab);
    gl.ActiveTexture(GL_TEXTURE0 + U_INV);
    sInvTex = texture(GL_R16UI, 64, 160, GL_RED_INTEGER, GL_UNSIGNED_SHORT, inv);
    free(tab);
    free(inv);
    gl.ActiveTexture(GL_TEXTURE0 + U_PAL);
    sPalTex = texture(GL_RGBA8UI, 256, PAL_ROWS, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);
    sPalFbo = fbo_of(sPalTex, 0);
    gl.ActiveTexture(GL_TEXTURE0 + U_CLUT);
    for (k = 0; k < 2; k++) {
        sClutTex[k] = texture(GL_R16UI, 512, CLUT_ROWS, GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
        sClutFbo[k] = fbo_of(sClutTex[k], 0);
    }
    gl.ActiveTexture(GL_TEXTURE0 + U_STAGE);
    sStageTex = texture(GL_RGBA8UI, 1024, 1024, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);
    gl.ActiveTexture(GL_TEXTURE0 + U_STATES);
    {
        GLint maxTexels = 0;
        gl.GetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTexels);
        sStRing = ST_RING;
        while (sStRing > MAX_STATES && (int64_t)sStRing * 3 > (int64_t)maxTexels) {
            sStRing >>= 1;
        }
    }
    gl.GenBuffers(1, &sStateBuf);
    gl.BindBuffer(GL_TEXTURE_BUFFER, sStateBuf);
    gl.BufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)sStRing * 48, NULL, GL_STREAM_DRAW);
    gl.GenTextures(1, &sStateTex);
    gl.BindTexture(GL_TEXTURE_BUFFER, sStateTex);
    gl.TexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, sStateBuf);
    gl.ActiveTexture(GL_TEXTURE0 + U_SRC);
    sDispTex = texture(GL_RGBA8, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    sDispFbo = fbo_of(sDispTex, 0);
    sDispW = sDispH = 1024;
    gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
    sVramTex = texture(GL_RGBA8UI, 1024, 1024, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, Gs_Vram());
    sVramFbo = fbo_of(sVramTex, 0);

    gl.GenVertexArrays(1, &sEmptyVao);
    gl.GenVertexArrays(1, &sVao);
    gl.BindVertexArray(sVao);
    gl.GenBuffers(1, &sVbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, sVbo);
    gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)VB_RING * (GLsizeiptr)sizeof(GlVtx), NULL, GL_STREAM_DRAW);
    for (k = 0; k < 6; k++) {
        gl.EnableVertexAttribArray((GLuint)k);
    }
    gl.VertexAttribIPointer(0, 2, GL_INT, sizeof(GlVtx), (const void *)offsetof(GlVtx, x));
    gl.VertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(GlVtx), (const void *)offsetof(GlVtx, z));
    gl.VertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(GlVtx), (const void *)offsetof(GlVtx, rgba));
    gl.VertexAttribIPointer(3, 3, GL_UNSIGNED_INT, sizeof(GlVtx), (const void *)offsetof(GlVtx, s));
    gl.VertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(GlVtx), (const void *)offsetof(GlVtx, fog));
    gl.VertexAttribIPointer(5, 1, GL_UNSIGNED_INT, sizeof(GlVtx), (const void *)offsetof(GlVtx, state));
    gl.BindVertexArray(sEmptyVao);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_DITHER);
    gl.Disable(GL_SCISSOR_TEST);
    plain_state();
    if (gl.GetError() != 0) {
        snprintf(why, (size_t)whylen, "GL error while setting up");
        return 0;
    }
    printf("ttt: gl: %s, %s\n", ver != NULL ? (const char *)ver : "?", ren != NULL ? (const char *)ren : "?");
    fflush(stdout);
    sReady = 1;
    return 1;
}

static unsigned tenths(uint64_t ns) { return (unsigned)(ns / 100000); } /* in tenths of a millisecond */

static unsigned hundredths(uint64_t ns) { return (unsigned)(ns / 10000); } /* in hundredths of a millisecond */

/* T38: one pacing histogram's line (ms): intervals, mean, percentiles, max, intervals over 1.5 VBlanks. */
static void pace_report(const char *what, const Pace *p) {
    uint64_t v[5];
    int i;
    v[0] = p->n ? p->sum / p->n : 0;
    v[1] = pace_pct(p, 50);
    v[2] = pace_pct(p, 95);
    v[3] = pace_pct(p, 99);
    v[4] = p->max;
    printf("ttt: gl: pacing of %s from VBlank %u: %u intervals,", what, (unsigned)sPaceFrom, p->n);
    for (i = 0; i < 5; i++) {
        static const char *const names[5] = {"mean", "p50", "p95", "p99", "max"};
        printf(" %s %u.%02u", names[i], hundredths(v[i]) / 100, hundredths(v[i]) % 100);
    }
    printf(" ms, %u over 1.5 VBlanks\n", p->late);
}

void Gl_Report(void) {
    if (sReady) {
        printf("ttt: gl: %u draw calls in %u batches, %u targets made, %u loads, %u resolves, %u pushes, %u read back, %u CLUT passes, %u pictures; "
               "approximated: %u DATE, %u C=Ad, %u FBMSK bytes, %u COLCLAMP 0, %u PABE, %u depth over frame\n",
               sGlStat.draws, sGlStat.batches, sGlStat.rts, sGlStat.loads, sGlStat.resolves, sGlStat.pushes, sGlStat.readbacks,
               sGlStat.clutGpu, sGlStat.shows, sGlStat.date, sGlStat.ad, sGlStat.fbmsk, sGlStat.colclamp, sGlStat.pabe, sGlStat.overlapZ);
        printf("ttt: gl: resolves by texture %u clut %u upload %u show %u target %u; read back by texture %u clut %u upload %u show %u target %u\n",
               sResBy[0], sResBy[1], sResBy[2], sResBy[3], sResBy[4], sRbBy[0], sRbBy[1], sRbBy[2], sRbBy[3], sRbBy[4]);
        /* T37: the presents, and the time from the VBlank after which each shown frame was complete (the one at which
           the game copies it into its display buffer) to its present (swap returned; with low latency glFinish too),
           at VBlank pictures and early ones (low latency), from window picture 600 on */
        printf("ttt: gl: video %dx, margin %d, flicker filter %s, low latency %s; %u presents (%u early, %u VBlanks shown already); "
               "frame to present: at VBlanks %u, mean %u.%u ms, max %u.%u ms; early %u, mean %u.%u ms, max %u.%u ms\n",
               sScale, sMarg, sDeflicker ? "on" : "off", sLowLat ? "on" : "off", sPres.presents, sPres.early, sPres.skipped,
               sPres.n[0], tenths(sPres.n[0] ? sPres.latSum[0] / sPres.n[0] : 0) / 10, tenths(sPres.n[0] ? sPres.latSum[0] / sPres.n[0] : 0) % 10,
               tenths(sPres.latMax[0]) / 10, tenths(sPres.latMax[0]) % 10,
               sPres.n[1], tenths(sPres.n[1] ? sPres.latSum[1] / sPres.n[1] : 0) / 10, tenths(sPres.n[1] ? sPres.latSum[1] / sPres.n[1] : 0) % 10,
               tenths(sPres.latMax[1]) / 10, tenths(sPres.latMax[1]) % 10);
        pace_report("presents", &sPacePres);
        pace_report("VBlank pictures queued", &sPaceQueue);
        printf("ttt: gl: pacing: %u VBlank pictures, %u without a new game frame, %u not shown (a newer one queued), swap mean %u.%02u ms, max %u.%02u ms\n", sPaceShows, sPaceRepeats, sPaceDropped,
               hundredths(sPacePres.n ? sSwapSum / sPacePres.n : 0) / 100, hundredths(sPacePres.n ? sSwapSum / sPacePres.n : 0) % 100,
               hundredths(sSwapMax) / 100, hundredths(sSwapMax) % 100);
    }
}

/* ------------------------------------------------------------------------------------------------ self-test */

/* TTT_SELFTEST=gl: the rules above on made-up primitives (no GL context): the alpha test's outcome over a primitive's
   alpha range, the depth write a repeated pass may share, the texels a UV primitive reads, the CLUT halves of a load. */
static int sGlFails;
static void gl_expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: gl self-test FAILED: %s\n", what);
        sGlFails++;
    }
}

int Gl_SelfTest(void) {
    static Cmd q, p;
    Key kq, kp;
    uint32_t lo = 0, hi = 0, a = 0, b = 0, set[16], ref[16], cpsm, csa, n, i;
    static const uint32_t kCpsm[4] = {0, 1, 2, 10};
    int k;

    gl_expect(alpha_outcome(5, 0x80, 0x80, 0x80) == 1 && alpha_outcome(5, 0x80, 0, 0x7F) == -1 && alpha_outcome(5, 0x80, 0x40, 0x80) == 0,
              "GEQUAL over a range");
    gl_expect(alpha_outcome(2, 0x80, 0, 0x7F) == 1 && alpha_outcome(2, 0x80, 0x80, 0xFF) == -1 && alpha_outcome(6, 0x80, 0x80, 0x90) == 0,
              "LESS / GREATER over a range");
    gl_expect(alpha_outcome(4, 0x80, 0x80, 0x80) == 1 && alpha_outcome(4, 0x80, 0, 0x7F) == -1 && alpha_outcome(7, 0x80, 0x80, 0x80) == -1 &&
              alpha_outcome(7, 0x80, 0x81, 0xFF) == 1 && alpha_outcome(0, 0, 0, 255) == -1 && alpha_outcome(1, 0, 0, 255) == 1,
              "EQUAL / NOTEQUAL / NEVER / ALWAYS");

    /* a 4-bit CLUT texture, DECAL, alpha from the texture: the CLUT row's alpha range */
    q.type = CMD_TRIANGLE;
    q.u.prim.d.tex = 1;
    q.u.prim.d.tcc = 1;
    q.u.prim.d.tfx = 1;
    q.u.prim.d.tbits = 4;
    for (k = 0; k < 3; k++) {
        q.u.prim.v[k].rgba = 0x10808080u;
    }
    sPalA[1][0] = sPalA[1][1] = 0x80;
    alpha_range(&q.u.prim.d, q.u.prim.v, CMD_TRIANGLE, 1, &lo, &hi);
    gl_expect(lo == 0x80 && hi == 0x80, "DECAL: the CLUT row's alpha");
    q.u.prim.d.tfx = 0; /* MODULATE: (texel * vertex) >> 7; Gouraud: one below the vertices */
    for (k = 0; k < 3; k++) {
        q.u.prim.v[k].rgba = 0x80808080u;
    }
    alpha_range(&q.u.prim.d, q.u.prim.v, CMD_TRIANGLE, 1, &lo, &hi);
    gl_expect(lo == 0x80 && hi == 0x80, "MODULATE, flat");
    q.u.prim.d.gouraud = 1;
    alpha_range(&q.u.prim.d, q.u.prim.v, CMD_TRIANGLE, 1, &lo, &hi);
    gl_expect(lo == 0x7F && hi == 0x80, "MODULATE, Gouraud");
    q.u.prim.d.tbits = 32;
    alpha_range(&q.u.prim.d, q.u.prim.v, CMD_TRIANGLE, 1, &lo, &hi);
    gl_expect(lo == 0 && hi == 0xFF, "32-bit texture: any alpha");
    q.u.prim.d.tcc = 0;
    alpha_range(&q.u.prim.d, q.u.prim.v, CMD_TRIANGLE, 1, &lo, &hi);
    gl_expect(lo == 0x7F && hi == 0x80, "TCC 0: the vertices' alpha");

    /* FB_ONLY failing everywhere: one pass, colour only */
    q.u.prim.d.gouraud = 0;
    q.u.prim.d.atest = 1;
    q.u.prim.d.afail = 1;
    q.u.prim.d.fbits = 32;
    q.u.prim.d.ztest = 1;
    q.u.prim.d.ztst = 2;
    q.u.prim.d.zwrite = 1;
    q.u.prim.d.scx1 = 639;
    q.u.prim.d.scy1 = 447;
    key_of(&q.u.prim.d, 0, 1, 0, -1, &kq);
    gl_expect(kq.cmask == 0xF && !kq.dwrite && kq.pass == 0, "FB_ONLY, every pixel fails");
    key_of(&q.u.prim.d, 0, 1, 0, 0, &kq);
    gl_expect(kq.pass == 2 && kq.fmask == 0xF && !kq.fdwrite, "FB_ONLY, it depends: two passes");

    /* the depth write of a pass on the vertices of the last full depth writer */
    for (k = 0; k < 3; k++) {
        q.u.prim.v[k].x = 100 * 16 + k * 160;
        q.u.prim.v[k].y = 50 * 16 + (k & 1) * 320;
        q.u.prim.v[k].z = 0x1000u + (uint32_t)k;
    }
    p = q;
    p.u.prim.d.zwrite = 0;
    sLastZ.valid = 0;
    key_of(&q.u.prim.d, 0, 1, 0, 1, &kq);
    same_depth(&q, &kq);
    key_of(&p.u.prim.d, 0, 1, 0, 1, &kp);
    gl_expect(!kp.dwrite, "no depth write of its own");
    same_depth(&p, &kp);
    gl_expect(kp.dwrite && memcmp(&kp, &kq, sizeof kp) == 0, "the same vertices: shares the batch's depth write");
    p.u.prim.d.scx1 = 700; /* a wider scissor: pixels Q did not write */
    key_of(&p.u.prim.d, 0, 1, 0, 1, &kp);
    same_depth(&p, &kp);
    gl_expect(!kp.dwrite, "a wider scissor keeps its own");
    p.u.prim.d.scx1 = 639;
    p.u.prim.v[2].z++;
    key_of(&p.u.prim.d, 0, 1, 0, 1, &kp);
    same_depth(&p, &kp);
    gl_expect(!kp.dwrite, "another depth keeps its own");
    sLastZ.valid = 0;

    /* the texels of one axis */
    uv_span(1, 512, 0, 0, 0, 64 * 16, 1, 0, &a, &b);
    gl_expect(a == 0 && b == 63, "nearest, the sprite's right edge left out");
    uv_span(1, 512, 0, 0, 0, 64 * 16, 0, 0, &a, &b);
    gl_expect(a == 0 && b == 64, "nearest, both edges");
    uv_span(1, 512, 0, 0, 16, 64 * 16, 0, 1, &a, &b);
    gl_expect(a == 0 && b == 64, "bilinear: half a texel down, one more up");
    uv_span(1, 512, 0, 0, 0, 64 * 16, 0, 1, &a, &b);
    gl_expect(a == 0 && b == 64, "bilinear under CLAMP: from 0");
    uv_span(0, 512, 0, 0, 0, 64 * 16, 0, 1, &a, &b);
    gl_expect(a == 0 && b == 511, "bilinear under REPEAT at 0: the whole texture");
    uv_span(2, 512, 8, 40, 0, 64 * 16, 0, 0, &a, &b);
    gl_expect(a == 8 && b == 40, "REGION_CLAMP: its range");

    /* the CLUT halves: the same as one by one */
    for (k = 0; k < 4; k++) {
        cpsm = kCpsm[k];
        for (n = 16; n <= 256; n += 240) {
            for (csa = 0; csa < 32; csa++) {
                memset(ref, 0, sizeof ref);
                for (i = 0; i < n; i++) {
                    uint32_t e = n == 256 ? i : csa * 16 + i;
                    if (cpsm <= 1) {
                        e &= 255;
                        ref[e >> 5] |= 1u << (e & 31);
                        ref[(e + 256) >> 5] |= 1u << (e & 31);
                    } else {
                        e &= 511;
                        ref[e >> 5] |= 1u << (e & 31);
                    }
                }
                clut_halves(cpsm, csa, n, set);
                if (memcmp(set, ref, sizeof set) != 0) {
                    gl_expect(0, "CLUT halves");
                    csa = 32;
                    k = 4;
                    n = 512;
                }
            }
        }
    }
    /* T37: the widescreen margins, a sprite going on into them, sprite edges on whole pixels at a finer scale */
    gl_expect(Gl_AspectMargin("4:3") == 0 && Gl_AspectMargin("16:9") == 78 && Gl_AspectMargin("21:9") == 203 && Gl_AspectMargin("x") == 0,
              "aspect margins (796 and 1046 x 448)");
    {
        Vtx sv[2], o[2];
        int32_t x0 = 0, x1 = 1016, u0 = 8, u1 = 1024;
        memset(sv, 0, sizeof sv);
        sv[0].x = 0; /* the game's copy strips: x 0 .. 63.5, U 0.5 .. 64 (one to one) */
        sv[1].x = 1016;
        sv[0].u = 8;
        sv[1].u = 1024;
        gl_expect(extend_sprite(sv, o, 640, 78, 1) && o[0].x == -78 * 16 && o[0].u == 8 - 78 * 16 && o[1].x == 1016 && o[1].u == 1024,
                  "the left strip goes on into the margin, U with it");
        sv[0].x = 576 * 16;
        sv[1].x = 640 * 16 - 8;
        gl_expect(extend_sprite(sv, o, 640, 78, 0) && o[1].x == 718 * 16 && o[1].u == 1024 && o[0].x == 576 * 16,
                  "the right strip (to 639.5) goes on; untextured: U unchanged");
        sv[0].x = 100 * 16;
        sv[1].x = 200 * 16;
        gl_expect(!extend_sprite(sv, o, 640, 78, 1), "a sprite inside the picture stays");
        align_axis(&x0, &x1, &u0, &u1, 1);
        gl_expect(x0 == 0 && x1 == 1024 && u0 == 8 && u1 == 1032, "x 0 .. 63.5 covers pixels 0 .. 63: edge at 64, U at the same rate");
        x0 = 1024 - 8;
        x1 = 2048 - 8;
        u0 = 0;
        u1 = 1024;
        align_axis(&x1, &x0, &u1, &u0, 1);
        gl_expect(x0 == 1024 && x1 == 2048 && u0 == 8 && u1 == 1032, "both edges up to whole pixels, vertices in either order");
    }
    printf(sGlFails ? "ttt: gl self-test FAILED (%d)\n" : "ttt: gl self-test OK\n", sGlFails);
    return sGlFails;
}
