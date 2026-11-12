"""GPU preference policy on synthetic Linux DRM devices: no display, driver or game data required."""
import os
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


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("cc") and EXE.exists() and disc() is not None
                     and os.environ.get("TTT_GPU_TESTS") == "1", "live GPU fallback test: set TTT_GPU_TESTS=1")
class GpuWindowFallback(unittest.TestCase):
    def test_failed_offload_reinitializes_sdl_and_keeps_hardware_rendering(self):
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
            self.assertNotIn("TTT_RENDERER=sw", r.stdout)
