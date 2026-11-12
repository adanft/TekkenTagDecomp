"""GPU preference policy on synthetic Linux DRM devices: no display, driver or game data required."""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from test_port_platform import EXE, disc, run_ttt

ROOT = Path(__file__).resolve().parents[1]
GPU_ENV = ("TTT_GPU", "TTT_RENDERER", "SDL_VIDEO_DRIVER", "DRI_PRIME", "__NV_PRIME_RENDER_OFFLOAD",
           "__NV_PRIME_RENDER_OFFLOAD_PROVIDER", "__GLX_VENDOR_LIBRARY_NAME", "__EGL_VENDOR_LIBRARY_FILENAMES",
           "__EGL_VENDOR_LIBRARY_DIRS", "MESA_LOADER_DRIVER_OVERRIDE", "LIBGL_ALWAYS_SOFTWARE")


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("cc"), "Linux and a host C compiler required")
class GpuPreference(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory()
        cls.exe = Path(cls.build.name) / "gpu-test"
        source = Path(cls.build.name) / "gpu-test.c"
        source.write_text(r'''
#include <stdarg.h>
#define ioctl test_ioctl
#include "plat_gpu.c"
#undef ioctl

int test_ioctl(int fd, unsigned long request, ...) {
    va_list args;
    GpuAmdQuery *query;
    GpuAmdInfo *info;
    char flag;
    va_start(args, request);
    query = va_arg(args, GpuAmdQuery *);
    va_end(args);
    if (request != _IOW('d', 0x45, GpuAmdQuery) || query->query != 0x16 || query->return_size != 144) return -1;
    if (read(fd, &flag, 1) != 1 || flag == '?') return -1;
    info = (GpuAmdInfo *)(uintptr_t)query->return_pointer;
    info->ids_flags = flag == 'A' ? 1 : 0;
    return 0;
}

int main(int argc, char **argv) {
    unsigned i;
    gpu_prefer(argv[1], argv[2]);
    if (argc > 3) Port_GpuFallback();
    for (i = 0; i < sizeof kGpuEnv / sizeof kGpuEnv[0]; i++) {
        const char *v = getenv(kGpuEnv[i]);
        printf("%s=%s\n", kGpuEnv[i], v != NULL ? v : "<unset>");
    }
    return 0;
}
''')
        subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "port/src"),
                        str(source), "-o", str(cls.exe)], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.sys = self.root / "drm"
        self.dev = self.root / "dev"
        self.sys.mkdir()
        self.dev.mkdir()

    def tearDown(self):
        self.tmp.cleanup()

    def gpu(self, index, pci="0000:01:00.0", vendor="0x10de", driver="nvidia", flag="D"):
        device = self.root / "devices" / pci
        device.mkdir(parents=True)
        (device / "vendor").write_text(vendor)
        (device / "driver").symlink_to(self.root / "drivers" / driver)
        card = self.sys / f"renderD{index}"
        card.mkdir()
        (card / "device").symlink_to(device)
        (self.dev / card.name).write_text(flag)

    def run_policy(self, fallback=False, **overrides):
        env = {k: v for k, v in os.environ.items() if k not in GPU_ENV}
        env.update(overrides)
        cmd = [str(self.exe), str(self.sys), str(self.dev)] + (["fallback"] if fallback else [])
        r = subprocess.run(cmd, env=env, capture_output=True, text=True, check=True)
        values = dict(line.split("=", 1) for line in r.stdout.splitlines() if not line.startswith("ttt:"))
        return values, r.stdout

    def test_nvidia_requests_offload_not_a_different_display_server(self):
        self.gpu(129)
        values, out = self.run_policy(SDL_VIDEO_DRIVER="wayland")
        self.assertEqual(values, {"DRI_PRIME": "<unset>", "__NV_PRIME_RENDER_OFFLOAD": "1",
                                  "__GLX_VENDOR_LIBRARY_NAME": "nvidia"})
        self.assertIn("requesting discrete NVIDIA (pci-0000_01_00_0)", out)

    def test_global_nvidia_glx_vendor_does_not_block_wayland_offload(self):
        self.gpu(129)
        values, _ = self.run_policy(__GLX_VENDOR_LIBRARY_NAME="nvidia")
        self.assertEqual(values["__NV_PRIME_RENDER_OFFLOAD"], "1")

    def test_amd_discrete_uses_exact_pci_address(self):
        self.gpu(128, pci="0000:03:00.0", vendor="0x1002", driver="amdgpu")
        values, out = self.run_policy()
        self.assertEqual(values["DRI_PRIME"], "pci-0000_03_00_0")
        self.assertEqual(values["__NV_PRIME_RENDER_OFFLOAD"], "<unset>")
        self.assertIn("requesting discrete AMD", out)

    def test_integrated_or_unrecognized_devices_keep_system_choice(self):
        for vendor, driver, flag in (("0x8086", "i915", "D"), ("0x1002", "amdgpu", "A"),
                                     ("0x1002", "amdgpu", "?"), ("0x10de", "vfio-pci", "D")):
            with self.subTest(vendor=vendor, driver=driver, flag=flag):
                self.gpu(128, vendor=vendor, driver=driver, flag=flag)
                values, out = self.run_policy()
                self.assertEqual(set(values.values()), {"<unset>"})
                self.assertNotIn("requesting discrete", out)
                shutil.rmtree(self.root / "devices")
                shutil.rmtree(self.sys / "renderD128")

    def test_no_device_or_missing_sysfs_keeps_system_choice(self):
        for missing in (False, True):
            if missing:
                self.sys.rmdir()
            values, _ = self.run_policy()
            self.assertEqual(set(values.values()), {"<unset>"})

    def test_unavailable_render_node_keeps_system_choice(self):
        self.gpu(128)
        (self.dev / "renderD128").unlink()
        values, _ = self.run_policy()
        self.assertEqual(set(values.values()), {"<unset>"})

    def test_multiple_discrete_cards_use_stable_pci_order(self):
        self.gpu(129, pci="0000:05:00.0")
        self.gpu(130, pci="0000:02:00.0", vendor="0x1002", driver="amdgpu")
        values, _ = self.run_policy()
        self.assertEqual(values["DRI_PRIME"], "pci-0000_02_00_0")

    def test_explicit_gpu_choices_are_never_overwritten(self):
        self.gpu(129)
        for name, value in (("DRI_PRIME", "0"), ("__NV_PRIME_RENDER_OFFLOAD", "0"),
                            ("__NV_PRIME_RENDER_OFFLOAD_PROVIDER", "NVIDIA-G0"),
                            ("__EGL_VENDOR_LIBRARY_FILENAMES", "/manual/vendor.json"),
                            ("__GLX_VENDOR_LIBRARY_NAME", "mesa"), ("LIBGL_ALWAYS_SOFTWARE", "1")):
            with self.subTest(name=name):
                values, out = self.run_policy(**{name: value})
                self.assertNotIn("requesting discrete", out)
                for key in values:
                    self.assertEqual(values[key], value if key == name else "<unset>")

    def test_system_mode_software_and_test_drivers_do_not_request_offload(self):
        self.gpu(129)
        for env in ({"TTT_GPU": "system"}, {"TTT_GPU": "unknown"}, {"TTT_RENDERER": "sw"},
                    {"SDL_VIDEO_DRIVER": "dummy"}, {"SDL_VIDEO_DRIVER": "offscreen"}):
            with self.subTest(env=env):
                values, out = self.run_policy(**env)
                self.assertEqual(set(values.values()), {"<unset>"})
                self.assertNotIn("requesting discrete", out)

    def test_fallback_removes_only_automatically_added_values(self):
        self.gpu(129)
        values, out = self.run_policy(fallback=True, __GLX_VENDOR_LIBRARY_NAME="nvidia")
        self.assertEqual(values, {"DRI_PRIME": "<unset>", "__NV_PRIME_RENDER_OFFLOAD": "<unset>",
                                  "__GLX_VENDOR_LIBRARY_NAME": "nvidia"})
        self.assertIn("retrying with the system GPU selection", out)

    def test_amd_fallback_removes_automatic_prime(self):
        self.gpu(128, vendor="0x1002", driver="amdgpu")
        values, out = self.run_policy(fallback=True)
        self.assertEqual(set(values.values()), {"<unset>"})
        self.assertIn("retrying with the system GPU selection", out)


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("cc"), "Linux and a host C compiler required")
class GpuCleanup(unittest.TestCase):
    def build_run(self, source, argument, stdout):
        with tempfile.TemporaryDirectory() as d:
            src, exe = Path(d) / "cleanup.c", Path(d) / "cleanup"
            src.write_text(source)
            build = subprocess.run(["cc", "-std=gnu11", "-ffunction-sections", "-fdata-sections",
                                    "-I", str(ROOT / "port/src"), "-I", str(ROOT / "port/include"),
                                    str(src), "-Wl,--gc-sections", "-o", str(exe)],
                                   capture_output=True, text=True, timeout=30)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            r = subprocess.run([str(exe), str(argument)], capture_output=True, text=True, timeout=10)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertEqual(r.stderr, "")
            self.assertEqual(r.stdout, stdout)

    def test_context_failure_cleanup_and_successful_release(self):
        self.build_run(r'''
#include "plat_window.c"
#include <assert.h>
static int mode, contexts, windows, inits;
const char *Port_DataMark(void) { return ""; }
const char *Tr(const char *key, const char *en) { (void)key; return en; }
void Settings_GetStr(const char *key, char *v, int n) { (void)key; snprintf(v, n, "4:3"); }
int Gl_AspectMargin(const char *a) { (void)a; return 0; }
int Gl_Init(void *(*p)(const char *), char *why, int n) {
    (void)p; inits++; snprintf(why, n, "injected failure"); return mode == 3;
}
int Overlay_InitGl(void *(*p)(const char *)) { (void)p; return 1; }
void Overlay_Shutdown(void) {}
void Port_SndRingEnable(int on) { (void)on; }
static _Bool attribute(int a, int b) { (void)a; (void)b; return 1; }
static void *window(const char *s, int w, int h, uint64_t f) {
    (void)s; (void)w; (void)h; (void)f; windows++; return (void *)1;
}
static void *context(void *w) { (void)w; if (!mode) return NULL; contexts++; return (void *)2; }
static _Bool current(void *w, void *c) { (void)w; return c == NULL || mode != 1; }
static const char *error(void) { return "injected"; }
static _Bool interval(int v) { (void)v; return 1; }
static _Bool destroy_context(void *c) { assert(c == (void *)2); contexts--; return 1; }
static void destroy_window(void *w) { assert(w == (void *)1 && contexts == 0); windows--; }
static void quit(void) { assert(!contexts && !windows); }
int main(void) {
    sHaveGlFuncs = 1;
    sdl.GL_SetAttribute = attribute; sdl.CreateWindow = window; sdl.GL_CreateContext = context;
    sdl.GL_MakeCurrent = current; sdl.GetError = error; sdl.GL_SetSwapInterval = interval;
    sdl.GL_DestroyContext = destroy_context; sdl.DestroyWindow = destroy_window; sdl.Quit = quit;
    for (mode = 0; mode < 3; mode++) {
        assert(!gl_open(0, 0, 1));
        assert(!contexts && !windows && !sGlContext && !sWindow);
        assert(inits == (mode == 2));
    }
    assert(gl_open(0, 0, 1));
    assert(contexts == 1 && windows == 1);
    release();
    assert(!contexts && !windows);
    return 0;
}
''', 0, "ttt: window: no OpenGL 3.3 context (injected): software renderer\n" * 2
             + "ttt: window: OpenGL back end: injected failure: software renderer\n"
             + "ttt: window: no settings overlay (F1)\n")

    def test_partial_allocations_and_setup_error_are_freed_before_retry(self):
        # Typed no-op driver entry points keep this a real Gl_Init call without a display.
        text = (ROOT / "port/src/gs_gl.c").read_text()
        table = text.split("static struct {", 1)[1].split("} gl;", 1)[0]
        stubs, mapping = [], []
        bodies = {
            "GetString": 'return (const unsigned char *)"synthetic";',
            "GetShaderiv": "*a2 = 1;", "GetProgramiv": "*a2 = 1;",
            "CreateShader": "return 1;", "CreateProgram": "return 1;",
            "GetError": "return setup_error;", "GetUniformLocation": "return -1;",
            "CheckFramebufferStatus": "return GL_FRAMEBUFFER_COMPLETE;",
            "GetIntegerv": "*a1 = 1000000;",
        }
        for ret, name, args in re.findall(r"([\w *]+)\(\*(\w+)\)\(([^;]*)\);", table):
            params = [] if args == "void" else args.split(",")
            signature = ",".join(f"{p.strip()} a{i}" for i, p in enumerate(params)) or "void"
            body = bodies.get(name, "" if ret.strip() == "void" else "return 0;")
            stubs.append(f"static {ret.strip()} fake_{name}({signature}) {{ {body} }}")
            mapping.append(f'if (!strcmp(name, "gl{name}")) return (void *)fake_{name};')
        source = r'''
#include <stdlib.h>
#include <assert.h>
static int calls, fail_at, live, setup_error;
static void *alloc(size_t n) { void *p; if (++calls == fail_at) return NULL; p = calloc(1, n); if (p) live++; return p; }
static void drop(void *p) { if (p) live--; free(p); }
#define malloc(n) alloc(n)
#define calloc(n,s) alloc((n)*(s))
#define free(p) drop(p)
#include "gs_gl.c"
#undef malloc
#undef calloc
#undef free
const uint16_t *Gs_PageTable(int k) { static uint16_t t[16384]; (void)k; return t; }
uint32_t *Gs_Vram(void) { static uint32_t v[1048576]; return v; }
''' + "\n".join(stubs) + '\nstatic void *getproc(const char *name) {\n' + "\n".join(mapping) + r'''
return NULL;
}
static void *missing(const char *name) { (void)name; return NULL; }
int main(int argc, char **argv) {
    char why[128]; (void)argc;
    fail_at = atoi(argv[1]); setup_error = fail_at == 7;
    assert(!Gl_Init(missing, why, sizeof why));
    assert(!strcmp(why, "no glGetError") && live == 0);
    assert(!Gl_Init(getproc, why, sizeof why));
    assert(!strcmp(why, setup_error ? "GL error while setting up" : "out of memory"));
    assert(live == 0);
    fail_at = setup_error = calls = 0;
    assert(Gl_Init(getproc, why, sizeof why));
    assert(live == 4);
    return 0;
}
'''
        for failure in range(1, 8):
            with self.subTest(failure=failure):
                self.build_run(source, failure, "ttt: gl: synthetic, synthetic\n")


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("cc") and EXE.exists() and disc() is not None
                     and os.environ.get("TTT_GPU_TESTS") == "1", "live GPU fallback test: set TTT_GPU_TESTS=1")
class GpuWindowFallback(unittest.TestCase):
    def test_failed_offload_reinitializes_sdl_and_keeps_opengl_backend(self):
        """Reject only the offloaded context through an SDL shim, then let the real driver create the default one."""
        with tempfile.TemporaryDirectory() as d:
            src, lib = Path(d) / "fail-offload.c", Path(d) / "fail-offload.so"
            src.write_text(r'''
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
void *SDL_GL_CreateContext(void *window) {
    void *(*create)(void *) = dlsym(RTLD_NEXT, "SDL_GL_CreateContext");
    if (getenv("__NV_PRIME_RENDER_OFFLOAD") || getenv("DRI_PRIME")) return NULL;
    return create(window);
}
''')
            subprocess.run(["cc", "-shared", "-fPIC", str(src), "-Wl,--no-as-needed", "-l:libSDL3.so.0", "-ldl",
                            "-o", str(lib)], check=True, capture_output=True, text=True)
            r = run_ttt(TTT_SDL_LIB=str(lib), TTT_FRAMES="120", TTT_WINDOW="1", TTT_WINDOW_HIDDEN="1",
                        TTT_RENDERER="gl", TTT_AUDIO="0", TTT_VSYNC="0")
            self.assertEqual(r.returncode, 0, r.stdout[-3000:] + r.stderr[-3000:])
            if "requesting discrete" not in r.stdout:
                self.skipTest("no automatic discrete GPU request on this host (or an explicit GPU override)")
            self.assertIn("retrying with the system GPU selection", r.stdout)
            self.assertIn("OpenGL renderer", r.stdout)
            renderer = re.search(r"^ttt: gl: ([^\n]+), ([^\n]+)$", r.stdout, re.MULTILINE)
            self.assertIsNotNone(renderer, "missing GL version and actual renderer identification")
            self.assertNotEqual(renderer.group(2).strip(), "?")
            # OpenGL can be Mesa llvmpipe/softpipe: this proves backend retry, not hardware acceleration.
            self.assertNotIn("TTT_RENDERER=sw", r.stdout)
