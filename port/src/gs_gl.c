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
 *     (read back before the CPU writes or reads those bytes: CLUT loads and uploads into resolved pages).
 *   - Primitives become vertices (integer attributes: 12.4 positions, Z, colour, S / T / Q float bits or U / V, fog)
 *     and a per-primitive state record (texture, CLUT row, wrap, filter, TFX / TCC, fog, alpha test, scissor, blend
 *     selects) in a texture buffer; a draw call covers every primitive until the target or a fixed-function setting
 *     changes (blend equation, depth test / write, colour mask, the alpha test's fail mode).
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
#include "port_host.h"
#include "gs.h"
#include "gs_int.h"

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
#undef F
};

/* ------------------------------------------------------------------------------------------------ shaders */

/* Texture units */
enum { U_VRAM, U_TAB, U_INV, U_PAL, U_STATES, U_SRC, U_STAGE };

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
    "uniform ivec2 uSize;\n"
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
    "    vec2 p = (vec2(aXY) + 8.0) * 0.0625;\n" /* the GS's pixel (x, y) at the centre of GL's pixel (x, y) */
    "    float z = uZBits > 0 ? float(min(aZ, uZMax)) * exp2(float(1 - uZBits)) - 1.0 : 0.0;\n"
    "    gl_Position = vec4(p * 2.0 / vec2(uSize) - 1.0, z, 1.0);\n"
    "    vCol = vec4(float(aCol & 255u), float((aCol >> 8) & 255u), float((aCol >> 16) & 255u), float(aCol >> 24));\n"
    "    vColF = aCol;\n"
    "    vTex = (fl & 2u) != 0u ? vec3(float(aSTQ.x), float(aSTQ.y), 1.0) : vec3(ee_float(aSTQ.x), ee_float(aSTQ.y), ee_float(aSTQ.z));\n"
    "    vFog = float(aFog);\n"
    "    vFogF = aFog;\n"
    "    vState = aState;\n"
    "}\n";

static const char kDrawFs[] =
    "uniform usamplerBuffer uStates;\n"
    "uniform usampler2D uPal;\n"
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
    "uvec4 sample_tex(int u16, int v16, float q) {\n"
    "    uint fl = s0.x, filter = (fl >> 7) & 3u;\n"
    "    bool linear = filter == 1u;\n"
    "    if (filter == 2u) {\n"
    "        int lodk = int(s0.w >> 20); if (lodk >= 2048) lodk -= 4096;\n"
    "        int lod = (fl & (1u << 14)) != 0u ? lodk : (int(floor(-log2(max(abs(q), 1e-30)) * 16.0)) << ((fl >> 15) & 3u)) + lodk;\n"
    "        linear = lod <= 0 ? (fl & (1u << 17)) != 0u : (fl & (1u << 18)) != 0u;\n"
    "    }\n"
    "    if (!linear) return texel(u16 >> 4, v16 >> 4);\n"
    "    u16 -= 8; v16 -= 8;\n"
    "    int u0 = u16 >> 4, v0 = v16 >> 4;\n"
    "    uint fu = uint(u16 & 15), fv = uint(v16 & 15);\n"
    "    uvec4 c00 = texel(u0, v0);\n"
    "    if (fu == 0u && fv == 0u) return c00;\n"
    "    uvec4 c10 = texel(u0 + 1, v0), c01 = texel(u0, v0 + 1), c11 = texel(u0 + 1, v0 + 1);\n"
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
    "    ivec2 pix = ivec2(gl_FragCoord.xy);\n"
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
    "out uvec4 oWord;\n"
    "uint inv(uint row, uint e) { return texelFetch(uInv, ivec2(int(e & 63u), int(row + (e >> 6))), 0).r; }\n"
    "uint px32(ivec2 p, uint psm) {\n"
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
    "out vec4 oCol;\n"
    "void main() {\n"
    "    uvec2 p = uvec2(gl_FragCoord.xy);\n"
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

/* The display: gs_core.c's display_into() on the mirror. */
static const char kDisplayFs[] =
    "uniform uvec4 uCa[2];\n" /* per circuit: fbp, fbw, psm, dbx | dby << 16 */
    "uniform ivec4 uCb[2];\n" /* per circuit: x0, y0, width, lines */
    "uniform uvec4 uMode;\n"  /* PMODE, BGCOLOR, lines per buffer line, 0 */
    "uniform ivec4 uMain;\n"  /* the main circuit's x0, y0 */
    "out vec4 oCol;\n"
    "uvec4 cpix(int k, int x, int r, out bool inn) {\n"
    "    inn = !(x < 0 || r < 0 || x >= uCb[k].z || r >= uCb[k].w);\n"
    "    if (!inn) return uvec4(0u);\n"
    "    uint v = gs_read(uCa[k].x, uCa[k].y, uCa[k].z, (uCa[k].w & 0xFFFFu) + uint(x), (uCa[k].w >> 16) + uint(r) / uMode.z);\n"
    "    uint bits = psm_bits(uCa[k].z);\n"
    "    if (bits == 16u) return uvec4((v & 31u) << 3, ((v >> 5) & 31u) << 3, ((v >> 10) & 31u) << 3, (v & 0x8000u) != 0u ? 128u : 0u);\n"
    "    if (bits == 24u) return uvec4(unpack32(v).rgb, 128u);\n"
    "    return unpack32(v);\n"
    "}\n"
    "void main() {\n"
    "    int x = int(gl_FragCoord.x), r = int(gl_FragCoord.y), ax = x + uMain.x, ar = r + uMain.y;\n"
    "    uint pm = uMode.x;\n"
    "    bool en1 = (pm & 1u) != 0u, en2 = (pm & 2u) != 0u, mmod = (pm & 32u) != 0u, slbg = (pm & 128u) != 0u, inn;\n"
    "    uvec3 base = unpack32(uMode.y).rgb, o;\n"
    "    if (en2 && !slbg) { uvec4 p = cpix(1, ax - uCb[1].x, ar - uCb[1].y, inn); if (inn) base = p.rgb; }\n"
    "    o = base;\n"
    "    if (en1) {\n"
    "        uvec4 p = cpix(0, ax - uCb[0].x, ar - uCb[0].y, inn);\n"
    "        if (inn) { uint a = mmod ? (pm >> 8) & 255u : min(p.a * 2u, 255u); o = (p.rgb * a + base * (255u - a) + 127u) / 255u; }\n"
    "    }\n"
    "    oCol = vec4(vec3(o) / 255.0, 1.0);\n"
    "}\n";

/* ------------------------------------------------------------------------------------------------ state */

#define MAX_RT 16
#define RT_ROWS 1024 /* rows allocated per target (the GS addresses 2048; the game draws within 1024) */
#define PAL_ROWS 2048
#define MAX_VERTS (1 << 18)
#define MAX_STATES 32768

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
} Rt;

typedef struct GlVtx {
    int32_t x, y;
    uint32_t z, rgba, s, t, q, fog, state;
} GlVtx;

/* What a draw call shares (a change ends the batch). */
typedef struct Key {
    int rt, zrt;            /* target, depth target (-1: none) */
    int kind;               /* 0 triangles, 1 lines, 2 points */
    int blend;              /* 0 off, 1 add, 2 subtract, 3 reverse subtract, 4 zero */
    int dfunc, dwrite;      /* depth (dfunc 0: no depth test) */
    int cmask;              /* colour mask r g b a = bits 0..3 */
    int pass;               /* 0 one pass, no alpha test; 1 one pass, failing pixels dropped; 2 two passes */
    int fmask, fdwrite;     /* the second pass's masks */
} Key;

static int sReady;
static int sCause; /* what asks for a resolve / read back (TTT_FRAMES report): 0 texture, 1 CLUT, 2 upload, 3 display, 4 target */
static uint32_t sResBy[5], sRbBy[5];
static GLuint sVramTex, sVramFbo, sTabTex, sInvTex, sPalTex, sStateBuf, sStateTex, sStageTex, sDispTex, sDispFbo;
static GLuint sVbo, sVao, sEmptyVao;
static GLuint sProgDraw, sProgResolve, sProgLoad, sProgStage, sProgDisplay;
static struct { GLint size, zbits, zmax, pass; } uDraw;
static struct { GLint rect, target, buf; } uResolve, uLoad, uStage;
static struct { GLint rect, target, ca, cb, mode, main; } uDisp;
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
static uint32_t sPalNext, sPalUp;
static struct { uint64_t ver; uint32_t cpsm, csa, bits; uint64_t texa; int row; } sPalKey = {0, 0, 0, 0, 0, -1};
static GlVtx *sV;
static uint32_t sNv;
static uint32_t (*sS)[12];
static uint32_t sNs;
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
static struct { uint32_t batches, draws, resolves, loads, pushes, readbacks, rts, date, ad, fbmsk, colclamp, pabe, overlapZ, shows; } sGlStat;

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
    rect_pass(uResolve.rect, uResolve.target, 0, (int)p0 * 2, 1024, (int)(p0 + pages) * 2, 1024, 1024);
    gl.ColorMask(1, 1, 1, 1);
    gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
    for (p = p0; p < p0 + pages; p++) {
        sGpuPages += sGpuNewer[p] == 0;
        sGpuNewer[p] |= (uint8_t)r->bytes;
    }
    sGlStat.resolves++;
    sResBy[sCause]++;
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
    gl.Viewport(0, 0, r->w, RT_ROWS);
    gl.UseProgram(sProgLoad);
    plain_state();
    if (r->depth) {
        uint32_t bits = (uint32_t)Gs_PsmBits(r->psm);
        gl.Enable(GL_DEPTH_TEST);
        gl.DepthFunc(GL_ALWAYS);
        gl.DepthMask(1);
        gl.Uniform4uiv(uLoad.buf, 1, (const GLuint[]){r->bp, r->bw, r->psm, bits == 32 ? 32u : bits == 24 ? 24u : 16u});
    } else {
        gl.Uniform4uiv(uLoad.buf, 1, (const GLuint[]){r->bp, r->bw, r->psm, 0});
    }
    rect_pass(uLoad.rect, uLoad.target, 0, (int)y0, r->w, (int)y1, r->w, RT_ROWS);
    plain_state();
    sGlStat.loads++;
}

static void rt_drop(int i) {
    size_t k;
    if (!sRt[i].used) {
        return;
    }
    resolve(i);
    flush();
    for (k = 0; k < sizeof sFbo / sizeof sFbo[0]; k++) {
        if (sFbo[k].fbo != 0 && (sFbo[k].c == i || sFbo[k].z == i)) {
            gl.DeleteFramebuffers(1, &sFbo[k].fbo);
            sFbo[k].fbo = 0;
        }
    }
    gl.DeleteTextures(1, &sRt[i].tex);
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
            if (rows > r->rows) { /* grow: the new rows come from GS memory */
                memset(set, 0, sizeof set);
                Gs_PagesOf(set, bp, bw, psm, 0, bw * 64 - 1, r->rows, rows - 1);
                drop_over(set, 0xF, i);
                rt_load(i, r->rows, rows);
                sDirtyEpoch++;
                memset(r->pages, 0, sizeof r->pages);
                Gs_PagesOf(r->pages, bp, bw, psm, 0, bw * 64 - 1, 0, rows - 1);
                r->rows = rows;
            }
            return i;
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
        memcpy(r->pages, set, sizeof set);
        gl.ActiveTexture(GL_TEXTURE0 + U_SRC); /* the unit the passes rebind: the others keep their textures */
        r->tex = depth ? texture(GL_DEPTH_COMPONENT32F, r->w, RT_ROWS, GL_DEPTH_COMPONENT, GL_FLOAT, NULL)
                       : texture(GL_RGBA16F, r->w, RT_ROWS, GL_RGBA, GL_FLOAT, NULL);
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

/* Carries out the queued primitives: their vertices and state records go to the GPU in one upload, then one draw call
   per batch (two with an alpha test that writes on failure), setting only the GL state that changes between them. */
static void flush(void) {
    uint32_t i;
    int curFbo = -1, curW = -1, curZ = -1, curDf = -1, curBlend = -1, curPass = -1, curCmask = -1, curDmask = -1;

    if (sNv == 0) {
        return;
    }
    push();
    if (sPalNext > sPalUp) {
        gl.ActiveTexture(GL_TEXTURE0 + U_PAL);
        gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)sPalUp, 256, (GLsizei)(sPalNext - sPalUp), GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, sPal[sPalUp]);
        gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
        sPalUp = sPalNext;
    }
    gl.BindBuffer(GL_TEXTURE_BUFFER, sStateBuf);
    gl.BufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)MAX_STATES * 48, NULL, GL_STREAM_DRAW);
    gl.BufferSubData(GL_TEXTURE_BUFFER, 0, (GLsizeiptr)sNs * 48, sS);
    gl.BindBuffer(GL_ARRAY_BUFFER, sVbo);
    gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)MAX_VERTS * (GLsizeiptr)sizeof(GlVtx), NULL, GL_STREAM_DRAW);
    gl.BufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sNv * (GLsizeiptr)sizeof(GlVtx), sV);
    gl.UseProgram(sProgDraw);
    gl.BindVertexArray(sVao);
    for (i = 0; i < sNb; i++) {
        const Key *k = &sB[i].k;
        GLenum mode = k->kind == 0 ? GL_TRIANGLES : k->kind == 1 ? GL_LINES : GL_POINTS;
        int fb = (int)fbo(k->rt, k->zrt), zbits = 0;
        if (fb != curFbo) {
            gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)fb);
            curFbo = fb;
        }
        if (sRt[k->rt].w != curW) {
            curW = sRt[k->rt].w;
            gl.Viewport(0, 0, curW, RT_ROWS);
            gl.Uniform2i(uDraw.size, curW, RT_ROWS);
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
        gl.DrawArrays(mode, (GLint)sB[i].first, (GLsizei)sB[i].count);
        sGlStat.draws++;
        if (k->pass == 2) { /* the failing pixels: usually only the depth mask changes (FB_ONLY) */
            set_masks(k->fmask, k->fdwrite, &curCmask, &curDmask);
            gl.Uniform1i(uDraw.pass, 2);
            curPass = 2;
            gl.DrawArrays(mode, (GLint)sB[i].first, (GLsizei)sB[i].count);
            sGlStat.draws++;
        }
    }
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

/* The fixed-function part of a primitive. */
static void key_of(const Draw *d, int rt, int zrt, int kind, Key *k) {
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
    if (!d->atest) {
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
    if (fmask == cmask && fdw == k->dwrite) {
        k->pass = 0; /* the test changes nothing */
    } else if (fmask == 0 && fdw == 0) {
        k->pass = 1;
    } else {
        k->pass = 2;
        k->fmask = fmask;
        k->fdwrite = fdw;
    }
}

/* Texture reads of pages a target drew: that target goes back to GS memory first. A UV sprite or triangle reads only
   the texels between its vertices' UV (one more each side for the bilinear filter) when the wrap mode does not move
   them, as gs_core.c's classify() for sprites: a copy of one buffer into another in strips then resolves its source
   once, not per strip, and glyphs from a font in a frame buffer's top byte do not resolve the buffers next to it. */
static void texture_hazard(const Draw *d, const Cmd *c) {
    uint64_t t0 = d->tex0 & 0x3FFFFFFFFull;
    uint64_t cl = (uint64_t)d->wms | (uint64_t)d->wmt << 2 | (uint64_t)d->minu << 4 | (uint64_t)d->maxu << 14 | (uint64_t)d->minv << 24 |
                  (uint64_t)d->maxv << 34;
    uint32_t mask = psm_bytes(d->tpsm);
    int i;

    if ((c->type == CMD_SPRITE || c->type == CMD_TRIANGLE) && d->fst) {
        const Vtx *v = c->u.prim.v;
        int n = c->type == CMD_SPRITE ? 2 : 3, k, umin = v[0].u, umax = v[0].u, vmin = v[0].v, vmax = v[0].v;
        int ua, ub, va, vb;
        uint32_t u0, u1, v0, v1;
        for (k = 1; k < n; k++) {
            umin = v[k].u < umin ? v[k].u : umin;
            umax = v[k].u > umax ? v[k].u : umax;
            vmin = v[k].v < vmin ? v[k].v : vmin;
            vmax = v[k].v > vmax ? v[k].v : vmax;
        }
        ua = (umin >> 4) - 1;
        ub = (umax >> 4) + 1;
        va = (vmin >> 4) - 1;
        vb = (vmax >> 4) + 1;
        Gs_TexRange(d->wms, d->tw, d->minu, d->maxu, &u0, &u1);
        Gs_TexRange(d->wmt, d->th, d->minv, d->maxv, &v0, &v1);
        if (d->wms <= 1 && ua >= 0 && ub <= d->tw - 1) { u0 = (uint32_t)ua; u1 = (uint32_t)ub; }
        if (d->wmt <= 1 && va >= 0 && vb <= d->th - 1) { v0 = (uint32_t)va; v1 = (uint32_t)vb; }
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
    o->state = sNs - 1;
    (void)v;
}

static void prim(const Cmd *c) {
    const Draw *d = &c->u.prim.d;
    const Vtx *v = c->u.prim.v;
    uint32_t rows = (uint32_t)d->scy1 + 1, st[12];
    int kind = c->type == CMD_LINE ? 1 : c->type == CMD_POINT ? 2 : 0;
    int newState, zneed = (d->ztest && d->ztst != 1) || d->zwrite, rt, zrt = -1, palRow = 0, need = c->type == CMD_SPRITE ? 6 : 3;
    Key k;
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
    if (d->tex) {
        texture_hazard(d, c);
        if (d->tbits <= 8) {
            palRow = pal_row(d);
        }
    }
    key_of(d, rt, zrt, kind, &k);
    if (sNv + (uint32_t)need > MAX_VERTS || sNs >= MAX_STATES || sNb >= MAX_BATCHES) {
        flush();
    }
    state_of(d, palRow, st);
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
    return h;
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
    readback(set, 0xF);
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

static void show(const Cmd *c) {
    const DispRegs *regs = &c->u.show.regs;
    uint64_t pmode = regs->pmode;
    int en1 = (int)(pmode & 1), en2 = (int)(pmode >> 1) & 1, k;
    Circuit ci[2];
    const Circuit *main;
    GLuint ca[8];
    GLint cb[8];
    int w, h;

    flush();
    Gs_Circuit(regs, 0, &ci[0]);
    Gs_Circuit(regs, 1, &ci[1]);
    main = !en1 && en2 ? &ci[1] : &ci[0];
    w = (int)main->width;
    h = (int)main->lines;
    for (k = 0; k < 2; k++) {
        uint32_t set[16] = {0}, dbl = (regs->smode2 & 3) == 3 ? 2 : 1;
        if (k == 0 ? !en1 : !en2) {
            continue;
        }
        Gs_PagesOf(set, ci[k].fbp, ci[k].fbw, ci[k].psm, ci[k].dbx, ci[k].dbx + ci[k].width - 1, ci[k].dby, ci[k].dby + ci[k].lines / dbl);
        sCause = 3;
        resolve_over(set, 0xF);
    }
    push();
    for (k = 0; k < 2; k++) {
        ca[k * 4 + 0] = ci[k].fbp;
        ca[k * 4 + 1] = ci[k].fbw;
        ca[k * 4 + 2] = ci[k].psm;
        ca[k * 4 + 3] = ci[k].dbx | ci[k].dby << 16;
        cb[k * 4 + 0] = (GLint)ci[k].x0;
        cb[k * 4 + 1] = (GLint)ci[k].y0;
        cb[k * 4 + 2] = (GLint)ci[k].width;
        cb[k * 4 + 3] = (GLint)ci[k].lines;
    }
    gl.BindFramebuffer(GL_FRAMEBUFFER, sDispFbo);
    gl.Viewport(0, 0, 1024, 1024);
    gl.UseProgram(sProgDisplay);
    plain_state();
    gl.Uniform4uiv(uDisp.ca, 2, ca);
    gl.Uniform4iv(uDisp.cb, 2, cb);
    gl.Uniform4uiv(uDisp.mode, 1, (const GLuint[]){(GLuint)pmode, (GLuint)regs->bgcolor & 0xFFFFFF, (regs->smode2 & 3) == 3 ? 2u : 1u, 0});
    gl.Uniform4iv(uDisp.main, 1, (const GLint[]){(GLint)main->x0, (GLint)main->y0, 0, 0});
    rect_pass(uDisp.rect, uDisp.target, 0, 0, w, h, 1024, 1024);
    sGlStat.shows++;
    if (c->u.show.window) {
        int ww = 0, wh = 0, dw, dh, x0, y0;
        if (gGsGlSkip) {
            return; /* a newer picture is queued */
        }
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
        gl.BlitFramebuffer(0, 0, w, h, x0, y0 + dh, x0 + dw, y0, GL_COLOR_BUFFER_BIT, GL_LINEAR); /* line 0 at the top */
        Port_WindowGlSwap();
    } else if (Gs_DumpDir() != NULL) {
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
    if (sProgDisplay == 0) {
        return 0;
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
    gl.ActiveTexture(GL_TEXTURE0 + U_STAGE);
    sStageTex = texture(GL_RGBA8UI, 1024, 1024, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);
    gl.ActiveTexture(GL_TEXTURE0 + U_STATES);
    gl.GenBuffers(1, &sStateBuf);
    gl.BindBuffer(GL_TEXTURE_BUFFER, sStateBuf);
    gl.BufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)MAX_STATES * 48, NULL, GL_STREAM_DRAW);
    gl.GenTextures(1, &sStateTex);
    gl.BindTexture(GL_TEXTURE_BUFFER, sStateTex);
    gl.TexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32UI, sStateBuf);
    gl.ActiveTexture(GL_TEXTURE0 + U_SRC);
    sDispTex = texture(GL_RGBA8, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    sDispFbo = fbo_of(sDispTex, 0);
    gl.ActiveTexture(GL_TEXTURE0 + U_VRAM);
    sVramTex = texture(GL_RGBA8UI, 1024, 1024, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, Gs_Vram());
    sVramFbo = fbo_of(sVramTex, 0);

    gl.GenVertexArrays(1, &sEmptyVao);
    gl.GenVertexArrays(1, &sVao);
    gl.BindVertexArray(sVao);
    gl.GenBuffers(1, &sVbo);
    gl.BindBuffer(GL_ARRAY_BUFFER, sVbo);
    gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)MAX_VERTS * (GLsizeiptr)sizeof(GlVtx), NULL, GL_STREAM_DRAW);
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

void Gl_Report(void) {
    if (sReady) {
        printf("ttt: gl: %u draw calls in %u batches, %u targets made, %u loads, %u resolves, %u pushes, %u read back, %u pictures; "
               "approximated: %u DATE, %u C=Ad, %u FBMSK bytes, %u COLCLAMP 0, %u PABE, %u depth over frame\n",
               sGlStat.draws, sGlStat.batches, sGlStat.rts, sGlStat.loads, sGlStat.resolves, sGlStat.pushes, sGlStat.readbacks,
               sGlStat.shows, sGlStat.date, sGlStat.ad, sGlStat.fbmsk, sGlStat.colclamp, sGlStat.pabe, sGlStat.overlapZ);
        printf("ttt: gl: resolves by texture %u clut %u upload %u show %u target %u; read back by texture %u clut %u upload %u show %u target %u\n",
               sResBy[0], sResBy[1], sResBy[2], sResBy[3], sResBy[4], sRbBy[0], sRbBy[1], sRbBy[2], sRbBy[3], sRbBy[4]);
    }
}
