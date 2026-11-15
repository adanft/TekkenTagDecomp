"""Snapshot reads of a target through a synthetic GL driver: no SDL, display, disc or game execution.

A primitive that samples the snapshot of the target it draws into (the frame bent by a mesh of triangles, at scale 2 and
above) neither resolves that target into GS memory nor copies whole rows of it: only the rectangle drawn since the last
copy. Two-pass alpha-tested primitives (FB_ONLY) share a batch when they cover different pixels, and a batch starts with
the masks the last one left, so depth writes are not switched twice per primitive."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

DRIVER = r'''
#include <assert.h>
static unsigned blit_log, draw_log;
#include "gs_gl.c"
int gPortStateNoGs;
void Gs_AsyncDrain(void) {}
int Port_WindowGl(void) { return 1; }
void Port_WindowGlSize(int *w, int *h) { *w = 320; *h = 240; }
void Port_WindowGlSwap(void) {}
void Port_StateAdd(const char *n, void *p, uint32_t z, int f) {}
void Port_StateHooks(const char *(*busy)(void), void (*before)(void), void (*after)(void)) {}
int Port_StateLoading(void) { return 0; }
int Settings_GetInt(const char *k) { return !strcmp(k, "video.resolution") ? 2 : !strcmp(k, "video.deflicker") || !strcmp(k, "video.smooth"); }
void Settings_GetStr(const char *k, char *out, int n) { snprintf(out, (size_t)n, "4:3"); }
int Mods_TexturesActive(void) { return 0; }
void Tp_Settings(int d, int r) {}
int Tp_Name(const uint32_t *v, const Draw *d, const uint32_t *p, TpName *n) { return 0; }
void Tp_Rect(const Draw *d, TpName *n) {}
void Tp_Dump(const uint32_t *v, const Draw *d, const uint32_t *p, const TpName *n) {}
int Tp_Find(const TpName *n) { return -1; }
int Tp_Count(void) { return 0; }
int Tp_Take(int i, uint8_t **p, int *w, int *h) { return -1; }
void Tp_Free(uint8_t *p) { free(p); }
uint64_t Tp_Hash(const void *p, uint32_t n) { return 1; }
void Tp_Report(uint32_t a, uint32_t b, uint32_t c) {}
@STUBS@
static void tri(Cmd *c, int x0, int y0, int x1, int y1, int x2, int y2) {
    c->u.prim.v[0].x = x0 * 16; c->u.prim.v[0].y = y0 * 16;
    c->u.prim.v[1].x = x1 * 16; c->u.prim.v[1].y = y1 * 16;
    c->u.prim.v[2].x = x2 * 16; c->u.prim.v[2].y = y2 * 16;
    Gl_Exec(c);
}
/* FB_ONLY alpha test with depth writes: the vertices' alpha straddles AREF, so both passes draw. */
static void alpha_tri(Cmd *c, uint32_t aref, int x0, int y0, int x1, int y1, int x2, int y2) {
    Draw *d = &c->u.prim.d;
    d->aref = aref;
    c->u.prim.v[0].rgba = 0x00808080; c->u.prim.v[1].rgba = 0x80808080; c->u.prim.v[2].rgba = 0xFF808080;
    tri(c, x0, y0, x1, y1, x2, y2);
}
static void alpha_batches(void) {
    Cmd c = {0};
    Draw *d = &c.u.prim.d;
    c.type = CMD_TRIANGLE;
    d->prim = 3; d->ctx = 1; d->fbp = 32; d->fbw = 1; d->fbits = 32; d->gouraud = 1;
    d->scx1 = 63; d->scy1 = 31; d->colclamp = 1; d->zmax = UINT32_MAX;
    d->zbp = 96; d->zpsm = 0x30; d->zwrite = 1; d->ztest = 1; d->ztst = 1;
    tri(&c, 0, 0, 64, 0, 0, 32);                  /* makes the colour and depth targets (their loads draw too) */
    flush();
    d->atest = 1; d->atst = 5; d->afail = 1;
    draw_log = 1;
    alpha_tri(&c, 0x80, 0, 0, 10, 0, 0, 10);      /* A */
    alpha_tri(&c, 0x81, 30, 0, 40, 0, 30, 10);    /* B: a new state record, apart from A: joins */
    alpha_tri(&c, 0x80, 40, 0, 30, 10, 40, 10);   /* C: shares B's edge only: joins */
    alpha_tri(&c, 0x81, 2, 2, 8, 2, 2, 8);        /* D: inside A: a batch of its own */
    alpha_tri(&c, 0x80, 31, 1, 33, 1, 31, 3);     /* E: on top of B (drawn already), apart from D: joins D */
    flush();
    draw_log = 0;
}
int main(int argc, char **argv) {
    char why[128];
    Cmd c = {0};
    Draw *d = &c.u.prim.d;
    assert(Gl_Init(getproc, why, sizeof why));
    if (argc > 1) {
        alpha_batches();
        return 0;
    }
    sScale = 2;
    c.type = CMD_TRIANGLE;
    d->prim = 3; d->ctx = 1; d->fbp = 32; d->fbw = 1; d->fbits = 32;
    d->scx1 = 63; d->scy1 = 31; d->colclamp = 1; d->zmax = UINT32_MAX;
    for (int k = 0; k < 3; k++) c.u.prim.v[k].rgba = 0x80808080;
    tri(&c, 0, 0, 64, 0, 0, 32);                  /* the whole 64 x 32 target */
    d->tex = d->fst = 1; d->tbp = 32; d->tbw = 1; d->tbits = 32; d->tw = 64; d->th = 32; d->wms = d->wmt = 1;
    blit_log = 1;
    tri(&c, 40, 20, 41, 20, 40, 21);              /* reads its own target: the first copy is whole */
    d->tex = 0;
    tri(&c, 40, 20, 47, 20, 40, 23);              /* drawn: x 40..47, y 20..23 */
    d->tex = 1;
    tri(&c, 40, 20, 47, 20, 40, 23);              /* reads it again: only that rectangle copied */
    blit_log = 0;
    printf("resolves %u dirty %d\n", sGlStat.resolves, sRt[0].dirty);
    memset(&c, 0, sizeof c);                      /* a copy out of the target's pages: GS memory is read now */
    c.type = CMD_COPY;
    c.u.copy.bitbltbuf = 32ull | 1ull << 16 | 64ull << 32 | 1ull << 48;
    c.u.copy.tw = c.u.copy.th = 1;
    Gl_Exec(&c);
    printf("resolves %u dirty %d\n", sGlStat.resolves, sRt[0].dirty);
    return 0;
}
'''


@unittest.skipUnless(shutil.which("clang"), "clang with guest-pointer support required")
class SnapshotReads(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.tmp.name) / "snapshot"
        text = (ROOT / "port/src/gs_gl.c").read_text()
        table = text.split("static struct {", 1)[1].split("} gl;", 1)[0]
        bodies = {"GetString": 'return (const unsigned char *)"synthetic";',
                  "GetShaderiv": "*a2 = 1;", "GetProgramiv": "*a2 = 1;",
                  "CreateShader": "return 1;", "CreateProgram": "return 1;",
                  "GetError": "return 0;", "GetUniformLocation": "return -1;",
                  "CheckFramebufferStatus": "return GL_FRAMEBUFFER_COMPLETE;",
                  "GetIntegerv": "*a1 = 1000000;",
                  "BlitFramebuffer": 'if (blit_log) printf("blit %d %d %d %d\\n", a0, a1, a2, a3);',
                  "ReadPixels": "memset(a6, 0, (size_t)a2 * a3 * 4);",
                  "DrawArrays": 'if (draw_log) printf("draw %d\\n", a2);',
                  "DepthMask": 'if (draw_log) printf("depth %d\\n", a0);',
                  "ColorMask": 'if (draw_log) printf("color %d%d%d%d\\n", a0, a1, a2, a3);'}
        for name in ("GenTextures", "GenFramebuffers", "GenBuffers", "GenVertexArrays"):
            bodies[name] = "static unsigned objects; for (int i=0; i<a0; i++) a1[i] = ++objects;"
        stubs, mapping = [], []
        for ret, name, args in re.findall(r"([\w *]+)\(\*(\w+)\)\(([^;]*)\);", table):
            params = [] if args == "void" else args.split(",")
            signature = ",".join(f"{p.strip()} a{i}" for i, p in enumerate(params)) or "void"
            body = bodies.get(name, "" if ret.strip() == "void" else "return 0;")
            stubs.append(f"static {ret.strip()} fake_{name}({signature}) {{ {body} }}")
            mapping.append(f'if (!strcmp(name, "gl{name}")) return (void *)fake_{name};')
        stubs.append("static void *getproc(const char *name) {\n" + "\n".join(mapping) + "\nreturn NULL;\n}")
        src = Path(cls.tmp.name) / "snapshot.c"
        src.write_text(DRIVER.replace("@STUBS@", "\n".join(stubs)))
        r = subprocess.run(["clang", "-std=gnu11", "-fms-extensions", "-ffunction-sections", "-fdata-sections",
                            "-I", str(ROOT / "port/src"), "-I", str(ROOT / "port/include"),
                            str(src), str(ROOT / "port/src/gs_core.c"),
                            "-Wl,--gc-sections", "-lm", "-pthread", "-o", str(cls.exe)],
                           capture_output=True, text=True, timeout=60)
        if r.returncode:
            raise AssertionError(r.stdout + r.stderr)
        p = subprocess.run([str(cls.exe)], capture_output=True, text=True, timeout=10)
        if p.returncode or p.stderr:
            raise AssertionError(p.stdout + p.stderr)
        cls.lines = p.stdout.splitlines()
        p = subprocess.run([str(cls.exe), "alpha"], capture_output=True, text=True, timeout=10)
        if p.returncode or p.stderr:
            raise AssertionError(p.stdout + p.stderr)
        log = p.stdout.splitlines()
        draws = [i for i, l in enumerate(log) if l.startswith("draw ")]
        cls.alpha = log[draws[0]:draws[-1] + 1] if draws else log

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_copies_only_the_rectangle_drawn_since_the_last_copy(self):
        blits = [tuple(map(int, l.split()[1:])) for l in self.lines if l.startswith("blit ")]
        # 2x: the first copy is whole (128 x 64); then the union of both triangles drawn after it, x 40..47 and
        # y 20..23, with one pixel more each side in x: x 39..48 -> 78..98, y 20..23 -> 40..48
        self.assertEqual(blits, [(0, 0, 128, 64), (78, 40, 98, 48)])

    def test_two_pass_primitives_on_different_pixels_share_a_batch(self):
        # A, B and C (apart, or sharing one edge) in one batch: passing pixels with depth writes, then failing ones
        # without. D overlaps A: a new batch, which E (apart from D) joins; it starts with the failing pixels, whose
        # masks the last batch left, so depth writes switch once, not twice
        self.assertEqual(self.alpha, ["draw 9", "depth 0", "draw 9", "draw 6", "depth 1", "draw 6"])

    def test_reading_its_own_snapshot_does_not_resolve_and_leaves_it_dirty(self):
        counts = [l for l in self.lines if l.startswith("resolves ")]
        self.assertEqual(counts[0], "resolves 0 dirty 1")
        # a later reader of GS memory still gets the target resolved
        self.assertEqual(counts[1], "resolves 1 dirty 0")


if __name__ == "__main__":
    unittest.main()
