/* Prefer a discrete GPU before SDL loads a graphics driver. This is a request, not a requirement: unknown or
   inaccessible hardware keeps the system default. Never assume that GPU 1 is discrete (it can be an integrated GPU
   on a desktop whose default is already discrete). No subprocess, Vulkan dependency or display-server override. */
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
/* The hybrid drivers inspect the executable's exports before main. Windows / driver application settings take
   precedence. These must be 32-bit values even in the 64-bit executable, and must not live in a DLL. */
__declspec(dllexport) uint32_t NvOptimusEnablement = 1;
__declspec(dllexport) uint32_t AmdPowerXpressRequestHighPerformance = 1;

void Port_GpuPrefer(void) {}
int Port_GpuFallback(void) { return 0; }
#else
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Linux DRM UAPI: AMDGPU_INFO_DEV_INFO (amdgpu_drm.h). Only request the stable prefix through ids_flags;
   return_size bounds the kernel's copy. Keeping this small avoids a build dependency on libdrm headers. */
typedef struct {
    uint64_t return_pointer;
    uint32_t return_size, query, data[4];
} GpuAmdQuery;
typedef struct {
    uint32_t identity[8];
    uint64_t clocks[2];
    uint32_t cu[22];
    uint64_t ids_flags;
} GpuAmdInfo;
_Static_assert(sizeof(GpuAmdQuery) == 32 && offsetof(GpuAmdInfo, ids_flags) == 136, "AMDGPU query ABI");

static int amd_discrete(const char *node) {
    GpuAmdInfo info = {0};
    GpuAmdQuery query = {(uint64_t)(uintptr_t)&info, sizeof info, 0x16, {0}};
    int fd = open(node, O_RDWR | O_CLOEXEC), ok;
    if (fd < 0) return 0;
    ok = ioctl(fd, _IOW('d', 0x45, GpuAmdQuery), &query) == 0;
    close(fd);
    return ok && !(info.ids_flags & 1u); /* AMDGPU_IDS_FLAGS_FUSION: an APU, not a discrete Radeon */
}

/* Return the driver kind (1 NVIDIA proprietary, 2 Mesa) and its PCI address. Stable PCI ordering, not readdir's
   order, breaks ties between several discrete cards. AMD's kernel flag distinguishes a Radeon from an AMD APU. */
static int find_gpu(const char *sysdir, const char *devdir, char pci[32]) {
    DIR *dir = opendir(sysdir);
    struct dirent *entry;
    int chosen = 0;
    pci[0] = 0;
    if (dir == NULL) return 0;
    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX], device[PATH_MAX], driver[PATH_MAX], node[PATH_MAX], address[32], *name;
        unsigned vendor, domain, bus, slot, function;
        long len;
        int kind = 0, used = 0;
        FILE *f;
        if (strncmp(entry->d_name, "renderD", 7) != 0 ||
            strspn(entry->d_name + 7, "0123456789") != strlen(entry->d_name + 7) || !entry->d_name[7]) continue;
        snprintf(path, sizeof path, "%s/%s/device", sysdir, entry->d_name);
        if (realpath(path, device) == NULL) continue;
        name = strrchr(device, '/');
        if (name == NULL || sscanf(name + 1, "%x:%x:%x.%x%n", &domain, &bus, &slot, &function, &used) != 4 ||
            name[1 + used] || domain > 0xffff || bus > 255 || slot > 31 || function > 7) continue;
        snprintf(path, sizeof path, "%s/%s/device/vendor", sysdir, entry->d_name);
        f = fopen(path, "r");
        if (f == NULL) continue;
        vendor = 0;
        (void)fscanf(f, "%x", &vendor);
        fclose(f);
        snprintf(path, sizeof path, "%s/%s/device/driver", sysdir, entry->d_name);
        len = readlink(path, driver, sizeof driver - 1);
        if (len <= 0) continue;
        driver[len] = 0;
        name = strrchr(driver, '/');
        if (name == NULL) continue;
        snprintf(node, sizeof node, "%s/%s", devdir, entry->d_name);
        if (vendor == 0x10de && strcmp(name + 1, "nvidia") == 0 && access(node, R_OK | W_OK) == 0) kind = 1;
        if (vendor == 0x1002 && strcmp(name + 1, "amdgpu") == 0 && amd_discrete(node)) kind = 2;
        if (!kind) continue;
        snprintf(address, sizeof address, "pci-%04x_%02x_%02x_%x", domain, bus, slot, function);
        if (!chosen || strcmp(address, pci) < 0) {
            strcpy(pci, address);
            chosen = kind;
        }
    }
    closedir(dir);
    return chosen;
}

static const char *const kGpuEnv[] = {"DRI_PRIME", "__NV_PRIME_RENDER_OFFLOAD", "__GLX_VENDOR_LIBRARY_NAME"};
static unsigned sGpuEnv; /* only the variables we added; never remove a launcher's or user's value */

int Port_GpuFallback(void) {
    unsigned i;
    if (!sGpuEnv) return 0;
    for (i = 0; i < sizeof kGpuEnv / sizeof kGpuEnv[0]; i++) {
        if (sGpuEnv & (1u << i)) unsetenv(kGpuEnv[i]);
    }
    sGpuEnv = 0;
    printf("ttt: gpu: retrying with the system GPU selection\n");
    return 1;
}

static void gpu_prefer(const char *sysdir, const char *devdir) {
    const char *mode = getenv("TTT_GPU"), *driver = getenv("SDL_VIDEO_DRIVER"), *renderer = getenv("TTT_RENDERER");
    const char *vendor = getenv("__GLX_VENDOR_LIBRARY_NAME");
    char pci[32];
    int kind;
    unsigned i;
    if (mode != NULL && strcmp(mode, "system") == 0) return;
    if (mode != NULL && strcmp(mode, "dedicated") != 0) {
        printf("ttt: gpu: unknown TTT_GPU=%s (dedicated or system): keeping system selection\n", mode);
        return;
    }
    if ((renderer != NULL && strcmp(renderer, "sw") == 0) ||
        (driver != NULL && (strcmp(driver, "dummy") == 0 || strcmp(driver, "offscreen") == 0))) return;
    /* Honour explicit GPU choices, including DRI_PRIME=0 and NVIDIA offload=0. A global GLX vendor of NVIDIA alone
       is not an EGL/Wayland GPU selection, so it must not prevent requesting NVIDIA offload there. */
    if (getenv("DRI_PRIME") || getenv("__NV_PRIME_RENDER_OFFLOAD") || getenv("__NV_PRIME_RENDER_OFFLOAD_PROVIDER") ||
        getenv("__EGL_VENDOR_LIBRARY_FILENAMES") || getenv("__EGL_VENDOR_LIBRARY_DIRS") ||
        getenv("MESA_LOADER_DRIVER_OVERRIDE") || getenv("LIBGL_ALWAYS_SOFTWARE") ||
        (vendor != NULL && strcmp(vendor, "nvidia") != 0)) return;
    kind = find_gpu(sysdir, devdir, pci);
    if (!kind || (kind == 2 && vendor != NULL)) return;
    for (i = 0; i < sizeof kGpuEnv / sizeof kGpuEnv[0]; i++) {
        const char *value = kind == 1 ? (i == 1 ? "1" : "nvidia") : pci;
        if ((kind == 1 && i == 0) || (kind == 2 && i != 0) || getenv(kGpuEnv[i]) != NULL) continue;
        if (setenv(kGpuEnv[i], value, 0) != 0) {
            (void)Port_GpuFallback();
            return;
        }
        sGpuEnv |= 1u << i;
    }
    printf("ttt: gpu: requesting discrete %s (%s)\n", kind == 1 ? "NVIDIA" : "AMD", pci);
}

void Port_GpuPrefer(void) { gpu_prefer("/sys/class/drm", "/dev/dri"); }
#endif
