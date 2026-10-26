/*
 * The game's files on PC, and the RSPU2DRV commands that carry them.
 *
 * The game reads nothing through libcdvd: it asks the sound driver on the IOP (RSPU2DRV) to read sectors of
 * TEKKEN.BIN into EE memory (command 0x204E) or SPU2 sound RAM (0x2045) and polls the driver's status word (0x2030)
 * every frame (docs/systems/files.md, docs/systems/audio.md). Here sceSdRemote is a direct call: a read is done at
 * once from the user's disc and the status word shows it finished at the next poll, as a fast drive would.
 * The data is the archive as stored (scrambled): the game descrambles it itself.
 *
 * The disc: TTT_DISC (or the settings' game.disc, which the setup writes: plat_setup.c, T43) = a folder holding the
 * disc's files (a mounted disc, names in any case, or the copy scripts/setup_disc.py makes in iso/), or an image of the
 * disc: .iso (ISO 9660, 2048-byte sectors) or a raw .bin / .cue (2352-byte sectors, MODE2 or MODE1); TEKKEN.BIN is
 * found in its root directory. Without TTT_DISC, the repository's iso/ (build/port/ttt -> ../../iso), iso/ beside the
 * program, or iso/ in the working directory. Without a disc the first read stops the program (exit code 2). Nothing
 * of the disc is ever written.
 *
 * Every other RSPU2DRV command is sound (T32): libspu2's commands in plat_snd.c on the SPU2 model (spu2.c), the BGM
 * tracks, the sound queue and the status bits 4 / 6 in plat_bgm.c (docs/port/sound.md). The movie stream (0x2058)
 * only records its arguments: the port's movie player reads the video (plat_movie.c), the BGM engine the audio
 * (docs/port/movies.md).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_addr.h"
#include "port_host.h"
#include "plat_os.h"
#include "spu2.h"
#include "plat_state.h"

/* C library calls the T43 disc formats added, through the GOT (port_host.h: the PLT stays as it was) */
PORT_NOPLT(strchr);

int Port_SndCommand(int cmd, const uint32_t *a); /* plat_snd.c */
void Port_SndGet(int cmd, uint8_t *out);
void Port_SndDriverStart(void);
uint32_t Port_BgmStatus(void); /* plat_bgm.c: bits 4 and 6 */

#define SECTOR 2048
#define ARCHIVE_SECTOR0 0x4820 /* the table's sectors count from here (systems/files.md) */
#define SIZE_BIAS 0x350A

/* A disc image: 2048-byte sectors (.iso), or raw CD sectors (.bin of a .cue: 2352 bytes, the 2048 data bytes at 24 in
   MODE2 form 1, at 16 in MODE1). A disc folder's TEKKEN.BIN is read as an image of 2048-byte sectors from 0. */
typedef struct {
    FILE *f;
    int raw, at;
} Img;

static Img sImg;             /* the disc: the image, or the folder's TEKKEN.BIN */
static int sDir;             /* the disc is a folder */
static long long sArchive;   /* byte offset of TEKKEN.BIN in the image's data */
static long long sArchiveSize;
static int sDiscTried;
static char sDiscName[1024]; /* as given: a folder, an .iso, a .bin or a .cue */
static char sImage[1100];    /* the host file read: the image, the .cue's .bin, or the folder's TEKKEN.BIN */

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* `n` bytes at byte `off` of the image's data (its 2048-byte sectors end to end). */
static int img_read(const Img *im, long long off, void *dst, size_t n) {
    uint8_t *d = dst;
    if (im->raw == SECTOR) {
        return fseeko(im->f, off, SEEK_SET) == 0 && fread(dst, 1, n, im->f) == n;
    }
    while (n > 0) {
        long long lba = off / SECTOR;
        size_t in = (size_t)(off % SECTOR), k = SECTOR - in < n ? SECTOR - in : n;
        if (fseeko(im->f, lba * im->raw + im->at + (long long)in, SEEK_SET) != 0 || fread(d, 1, k, im->f) != k) {
            return 0;
        }
        d += k, off += (long long)k, n -= k;
    }
    return 1;
}

/* Opens `path` as an ISO 9660 image in any of the layouts above; 0 (nothing left open) when it is none. */
static int img_open(Img *im, const char *path) {
    static const int kLayouts[3][2] = {{2048, 0}, {2352, 24}, {2352, 16}};
    uint8_t pvd[8];
    int i;
    im->f = fopen(path, "rb");
    for (i = 0; im->f != NULL && i < 3; i++) {
        im->raw = kLayouts[i][0];
        im->at = kLayouts[i][1];
        if (img_read(im, 16LL * SECTOR, pvd, sizeof pvd) && memcmp(pvd + 1, "CD001", 5) == 0) {
            return 1;
        }
    }
    if (im->f != NULL) {
        fclose(im->f);
        im->f = NULL;
    }
    return 0;
}

/* `name` (or `name;1`) in the directory at sector `lba` (`size` bytes) of the image: its sector and size, or -1. */
static long img_find(const Img *im, uint32_t lba, uint32_t size, const char *name, uint32_t *outLba) {
    uint8_t dir[SECTOR];
    size_t nl = strlen(name);
    uint32_t off;
    for (off = 0; off < size; off += SECTOR) {
        uint32_t i = 0;
        if (!img_read(im, (long long)(lba + off / SECTOR) * SECTOR, dir, SECTOR)) {
            return -1;
        }
        while (i + 33 < SECTOR && dir[i] != 0) {
            uint8_t len = dir[i], n = dir[i + 32];
            if (n >= nl && i + 33 + n <= SECTOR && memcmp(dir + i + 33, name, nl) == 0 &&
                (n == nl || dir[i + 33 + nl] == ';')) {
                *outLba = le32(dir + i + 2);
                return (long)le32(dir + i + 10);
            }
            i += len;
        }
    }
    return -1;
}

/* `dir`/`name` (`name` in the root directory when dir is NULL) of the image: its sector and size, or -1. */
static long img_file(const Img *im, const char *dirName, const char *name, uint32_t *lba) {
    uint8_t pvd[SECTOR];
    long size;
    if (!img_read(im, 16LL * SECTOR, pvd, SECTOR)) {
        return -1;
    }
    *lba = le32(pvd + 156 + 2);
    size = (long)le32(pvd + 156 + 10);
    if (dirName != NULL && (size = img_find(im, *lba, (uint32_t)size, dirName, lba)) < 0) {
        return -1;
    }
    return img_find(im, *lba, (uint32_t)size, name, lba);
}

/* A file of a disc folder: `base`/`dir`/`name` as on the disc, in lower case (a disc mounted by Linux shows its names
   so), or with ISO 9660's ";1" (mounted with map=off). Opened, its path in `out`; NULL when none. */
static FILE *dir_open(const char *base, const char *dirName, const char *name, char *out, size_t n) {
    char lo[300];
    size_t i, k;
    int v;
    for (v = 0; v < 3; v++) {
        FILE *f;
        if (dirName != NULL) {
            snprintf(lo, sizeof lo, "%s/%s%s", dirName, name, v == 2 ? ";1" : "");
        } else {
            snprintf(lo, sizeof lo, "%s%s", name, v == 2 ? ";1" : "");
        }
        for (i = 0, k = strlen(lo); v == 1 && i < k; i++) {
            lo[i] = lo[i] >= 'A' && lo[i] <= 'Z' ? (char)(lo[i] + 32) : lo[i];
        }
        snprintf(out, n, "%s/%s", base, lo);
        if ((f = fopen(out, "rb")) != NULL) {
            return f;
        }
    }
    return NULL;
}

/* The .bin a .cue sheet names first (`FILE "disc.bin" BINARY`), beside the sheet: 1 and its path in `out`. */
static int cue_bin(const char *cue, char *out, size_t n) {
    char line[1200], *a, *b;
    const char *slash = strrchr(cue, '/');
    FILE *f = fopen(cue, "r");
    int ok = 0;
    while (f != NULL && !ok && fgets(line, sizeof line, f) != NULL) {
        for (a = line; *a == ' ' || *a == '\t'; a++) {
        }
        if (memcmp(a, "FILE ", 5) != 0) {
            continue;
        }
        a += 5;
        if (*a == '"') {
            b = strrchr(++a, '"');
        } else {
            b = a + strcspn(a, " \t\r\n");
        }
        if (b == NULL || b == a) {
            break;
        }
        *b = 0;
        if (*a == '/' || slash == NULL) {
            snprintf(out, n, "%s", a);
        } else {
            snprintf(out, n, "%.*s/%s", (int)(slash - cue), cue, a);
        }
        ok = 1;
    }
    if (f != NULL) {
        fclose(f);
    }
    return ok;
}

static int ends_with(const char *s, const char *tail) {
    size_t n = strlen(s), k = strlen(tail), i;
    if (n < k) {
        return 0;
    }
    for (i = 0; i < k; i++) {
        char c = s[n - k + i];
        if ((c >= 'A' && c <= 'Z' ? c + 32 : c) != tail[i]) {
            return 0;
        }
    }
    return 1;
}

/* Opens `name` as the disc: a folder holding TEKKEN.BIN, or an image whose root directory holds it (.iso, .bin, or the
   .bin of a .cue). */
static int try_disc(const char *name) {
    char bin[1100];
    uint32_t lba;
    long size;
    FILE *f = dir_open(name, NULL, "TEKKEN.BIN", sImage, sizeof sImage);
    sDir = 0;
    sArchive = sArchiveSize = 0;
    if (f != NULL) {
        sDir = 1;
        sImg.f = f;
        sImg.raw = SECTOR;
        sImg.at = 0;
        fseeko(f, 0, SEEK_END);
        sArchiveSize = ftello(f);
        return 1;
    }
    if (ends_with(name, ".cue")) {
        if (!cue_bin(name, bin, sizeof bin)) {
            return 0;
        }
    } else {
        snprintf(bin, sizeof bin, "%s", name);
    }
    snprintf(sImage, sizeof sImage, "%s", bin);
    if (!img_open(&sImg, bin)) {
        return 0;
    }
    if ((size = img_file(&sImg, NULL, "TEKKEN.BIN", &lba)) >= 0) {
        sArchive = (long long)lba * SECTOR;
        sArchiveSize = size;
        return 1;
    }
    fclose(sImg.f);
    sImg.f = NULL;
    return 0;
}

/* Opens the disc once; 0 when there is none. TTT_DISC (game.disc, which the setup writes: plat_setup.c), else the
   repository's iso/ (build/port/ttt -> ../../iso), then iso/ beside the program, then iso/ in the working directory.
   The program's own directory comes from PlatOs_ExePath (symbolic links resolved), so a copy of the program elsewhere
   finds only the last two. */
int Port_DiscOpen(void) {
    static const char *const kNear[] = { "/../../iso", "/iso" };
    const char *d;
    char exe[1024];
    long k;
    unsigned i;
    if (sDiscTried) {
        return sImg.f != NULL;
    }
    sDiscTried = 1;
    d = getenv("TTT_DISC");
    if (d != NULL && *d) {
        snprintf(sDiscName, sizeof sDiscName, "%s", d);
        return try_disc(sDiscName);
    }
    k = PlatOs_ExePath(exe, sizeof exe - 1);
    if (k > 0) {
        char *slash;
        exe[k] = 0;
        slash = strrchr(exe, '/');
        if (slash != NULL) {
            *slash = 0;
            for (i = 0; i < sizeof kNear / sizeof kNear[0]; i++) {
                snprintf(sDiscName, sizeof sDiscName, "%s%s", exe, kNear[i]);
                if (try_disc(sDiscName)) {
                    return 1;
                }
            }
        }
    }
    snprintf(sDiscName, sizeof sDiscName, "iso");
    return try_disc(sDiscName);
}

/* The setup (plat_setup.c, before the game starts): the disc is `name` from now on; 0 when it is none. */
int Port_DiscUse(const char *name) {
    if (sImg.f != NULL) {
        fclose(sImg.f);
        sImg.f = NULL;
    }
    sDiscTried = 1;
    snprintf(sDiscName, sizeof sDiscName, "%s", name);
    return try_disc(sDiscName);
}

/* The game cannot run without its data: on the PS2 a read error is retried for ever, here a missing disc would let
   the boot go on over memory the reads never filled (it crashed in func_00345410 when the program was copied away
   from the repository's iso/, with the message lost in stdout's buffer). Stop at the first read instead. */
static void no_disc(void) {
    fflush(stdout);
    fprintf(stderr,
            "ttt: no disc: run `ttt --setup <disc>` with your disc's folder or image (.iso, .bin, .cue), or set "
            "TTT_DISC to it (or put iso/ beside the program; last tried %s)\n",
            sDiscName);
    _exit(2);
}

/* `bytes` of the archive from table sector `sector` into `dst`; 0 on a failed read. */
static int read_archive(uint32_t sector, void *dst, uint32_t bytes) {
    long long off = ((long long)sector - ARCHIVE_SECTOR0) * SECTOR;
    if (!Port_DiscOpen() || off < 0 || off + bytes > sArchiveSize) {
        return 0;
    }
    return img_read(&sImg, sArchive + off, dst, bytes);
}

/* `bytes` from `offset` of the file at table sector `sector` (the BGM streams, plat_bgm.c); 0 on a failed read. */
int Port_DiscRead(uint32_t sector, uint32_t offset, void *dst, uint32_t bytes) {
    long long off = ((long long)sector - ARCHIVE_SECTOR0) * SECTOR + offset;
    if (!Port_DiscOpen() || off < 0 || off + bytes > sArchiveSize) {
        return 0;
    }
    return img_read(&sImg, sArchive + off, dst, bytes);
}

/* A file of the disc outside TEKKEN.BIN (`dir`/`name`, e.g. IRX/RSPU2DRV.IRX, or `name` in the root directory when
   dir is NULL, e.g. SLUS_200.01) into `dst`: its size, or -1. The disc folder, or the image's root directory and one
   subdirectory. */
static long read_file(const Img *im, int isDir, const char *base, const char *dirName, const char *name, void *dst,
                      long max) {
    char path[1300];
    FILE *f;
    long n;
    uint32_t lba;
    if (isDir) {
        if ((f = dir_open(base, dirName, name, path, sizeof path)) == NULL) {
            return -1;
        }
        n = (long)fread(dst, 1, (size_t)max, f);
        fclose(f);
        return n;
    }
    if ((n = img_file(im, dirName, name, &lba)) < 0) {
        return -1;
    }
    n = n < max ? n : max;
    return img_read(im, (long long)lba * SECTOR, dst, (size_t)n) ? n : -1;
}

long Port_DiscReadFile(const char *dirName, const char *name, void *dst, long max) {
    if (!Port_DiscOpen()) {
        return -1;
    }
    return read_file(&sImg, sDir, sDiscName, dirName, name, dst, max);
}

/* The program a disc boots (SYSTEM.CNF's BOOT2, e.g. "SLUS_200.01") into `out`, for the setup's messages about a disc
   that is not the one expected (plat_data.c): a folder or an image, opened on its own. 0 when there is none. */
int Port_DiscBootName(const char *name, char *out, int n) {
    char cnf[512], bin[1100], *p, *e;
    Img im = {NULL, SECTOR, 0};
    long k;
    PlatOsStat st;
    out[0] = 0;
    if (PlatOs_Stat(name, &st) == 0 && st.dir) {
        k = read_file(&im, 1, name, NULL, "SYSTEM.CNF", cnf, sizeof cnf - 1);
    } else {
        snprintf(bin, sizeof bin, "%s", name);
        if ((ends_with(name, ".cue") && !cue_bin(name, bin, sizeof bin)) || !img_open(&im, bin)) {
            return 0;
        }
        k = read_file(&im, 0, NULL, NULL, "SYSTEM.CNF", cnf, sizeof cnf - 1);
        fclose(im.f);
    }
    if (k <= 0) {
        return 0;
    }
    cnf[k] = 0;
    if ((p = strstr(cnf, "BOOT2")) == NULL || (p = strchr(p, '\\')) == NULL) {
        return 0;
    }
    p++;
    e = p + strcspn(p, ";\r\n \t");
    snprintf(out, (size_t)n, "%.*s", (int)(e - p), p);
    return out[0] != 0;
}

/* TEKKEN.BIN's size in bytes (0: no disc), and the name the disc was found under (plat_data.c's messages). */
long long Port_DiscArchiveSize(void) { return Port_DiscOpen() ? sArchiveSize : 0; }
const char *Port_DiscName(void) { return sDiscName; }

/* The host file that holds TEKKEN.BIN (the disc folder's TEKKEN.BIN, or the image) and the archive's offset in the
   image's data, for plat_data.c's check on a thread of its own (with its own handle: sImg's position belongs to the
   game's reads, Port_DiscArchiveReadAt); NULL without a disc. */
const char *Port_DiscArchiveFile(long long *offset) {
    if (!Port_DiscOpen()) {
        return NULL;
    }
    *offset = sArchive;
    return sImage;
}

/* `n` bytes from byte `off` of TEKKEN.BIN through `f`, a handle of Port_DiscArchiveFile's own (the image's layout). */
int Port_DiscArchiveReadAt(FILE *f, long long off, void *dst, size_t n) {
    Img im = sImg;
    im.f = f;
    return Port_DiscOpen() && off >= 0 && off + (long long)n <= sArchiveSize && img_read(&im, sArchive + off, dst, n);
}

/* ---- RSPU2DRV ---- */
static uint32_t sMovieStream[5]; /* 0x2058's arguments (sector, movie, audio sectors, video sectors), active */

/* The movie stream 0x2058 started (plat_movie.c, plat_bgm.c): 1 and its arguments while it runs. */
int Port_MovieStreamArgs(uint32_t *sector, uint32_t *movie, uint32_t *audio, uint32_t *video) {
    *sector = sMovieStream[0];
    *movie = sMovieStream[1];
    *audio = sMovieStream[2];
    *video = sMovieStream[3];
    return (int)sMovieStream[4];
}

static uint32_t sStatus;                 /* 0x2030: bit 0 read pending, bit 2 disc error (bits 4, 6: plat_bgm.c) */
uint32_t gPortFileReads, gPortFileBytes;

/* A read of `size + 0x350A` (the table's biased size) from `sector` into EE memory or SPU2 RAM. The IOP moves the
   data in quadwords: the copy is rounded up to 16 bytes [I: the IOP's last chunk size is not known]. */
static int file_read(uint32_t sector, uint32_t dest, uint32_t biased, int toSpu) {
    uint32_t size = biased - SIZE_BIAS;
    void *dst;
    if ((int32_t)size <= 0) {
        return 0;
    }
    if (toSpu) {
        size = (size + 63) & ~63u;
        if (dest >= SPU2_RAM_SIZE || size > SPU2_RAM_SIZE - dest) {
            sStatus |= 4;
            return 0;
        }
        dst = Spu2_Ram() + dest; /* SPU2 sound RAM (spu2.c) */
    } else {
        size = (size + 15) & ~15u;
        if (!Port_IsEeRam(dest) || Port_EePhys(dest) + size > PORT_EE_RAM_SIZE) {
            sStatus |= 4;
            return 0;
        }
        dst = Port_DmaPtr(dest);
    }
    if (!Port_DiscOpen()) {
        no_disc();
    }
    if (!read_archive(sector, dst, size)) {
        sStatus |= 4; /* the driver's disc error flag */
        printf("ttt: disc read failed: sector 0x%x, 0x%x bytes\n", (unsigned)sector, (unsigned)size);
        return 0;
    }
    gPortFileReads++;
    gPortFileBytes += size;
    sStatus |= 1; /* pending until the next status poll: the data is there, the game sees it a frame later */
    return 0;
}

static int sStatusPolls;
/* One RSPU2DRV command. `a` are the six argument words; `reply` gets 0x40 bytes for the get commands (0x6xxx). */
static int rspu2(int cmd, const uint32_t *a) {
    switch (cmd) {
    case 0x2030: { /* status */
        uint32_t s = (sStatus & ~0x50u) | Port_BgmStatus();
        sStatus &= ~1u; /* a read shown pending once is finished at the next poll */
        sStatusPolls++;
        return (int)s;
    }
    case 0x204E: /* read to EE memory */
        return file_read(a[0], a[1], a[2], 0);
    case 0x2045: /* read to SPU2 RAM */
        return file_read(a[0], a[1], a[2], 1);
    case 0x2050: /* abort the read: it has finished already */
        sStatus &= ~1u;
        return 0;
    case 0x2000: /* driver start (finds TEKKEN.BIN on the disc; the SPU2 and the BGM engine: plat_snd.c) */
        Port_DiscOpen();
        Port_SndDriverStart();
        return 0;
    case 0x2058: /* movie stream start: file sector, movie, audio and video sectors of each interleave chunk (7,
                    0x80). The port's player (plat_movie.c) reads the video itself; the audio is code 0x5C's. */
        sMovieStream[0] = a[0];
        sMovieStream[1] = a[1];
        sMovieStream[2] = a[2];
        sMovieStream[3] = a[3];
        sMovieStream[4] = 1;
        return 0;
    case 0x2059: /* next movie chunk: the port's player never asks (it reads the disc directly) */
        return -1;
    case 0x205A: /* 0: stop the movie stream; 1: back to file mode, non-zero once there (func_0039C878 waits on it) */
        sMovieStream[4] = 0;
        return a[0] == 1 ? 1 : 0;
    default:
        /* 1 SpuInit, 2 SpuSetCore, 5 key off, 0x7xxx attribute sets, 0x7F0F sound queue, 0x205E BGM tracks
           (plat_snd.c, plat_bgm.c); 0x2051 semaphore tick: nothing to do. */
        return Port_SndCommand(cmd, a);
    }
}

static int sd_remote(int cmd, const uint32_t *a) {
    if ((cmd & 0xF000) == 0x6000) { /* get: 0x40 bytes into the buffer a0 points to (plat_snd.c) */
        if (a[0] != 0) {
            Port_SndGet(cmd, Port_Ptr(a[0]));
        }
        return 0;
    }
    if (cmd >= 0x8100 && cmd <= 0x8600) { /* EE callbacks: never called (the game polls) */
        return 0;
    }
    return rspu2(cmd, a);
}

typedef struct { int cmd, r; uint32_t a[6]; } SdCall;
static void sd_remote_off(void *p) {
    SdCall *c = p;
    c->r = sd_remote(c->cmd, c->a);
}

/* sceSdRemote(mode, command, six argument words): the game's only door to the IOP (librspu2). Off the EE stack
   (Port_OffEe): the disc reads go through the C library, whose frames hold addresses of this process. */
int sceSdRemote(int mode, int cmd, int a0, int a1, int a2, int a3, int a4, int a5) {
#if defined(__x86_64__)
    /* x86-64: the game calls it as variadic with only the words a command takes (sceSdRemote(1, 0x2030)), so the rest
       are whatever the caller's registers and stack slots held, host values among them; copied into a frame on the
       EE stack they stayed there (the synctest saw them differ between passes, T42 b4). The call's words go to a
       buffer of the port's instead. (Not reentrant; nothing calls it from inside a call.) */
    static SdCall c;
    c.cmd = cmd, c.r = 0;
    c.a[0] = (uint32_t)a0, c.a[1] = (uint32_t)a1, c.a[2] = (uint32_t)a2, c.a[3] = (uint32_t)a3;
    c.a[4] = (uint32_t)a4, c.a[5] = (uint32_t)a5;
#else
    SdCall c = { cmd, 0, { (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3, (uint32_t)a4, (uint32_t)a5 } };
#endif
    (void)mode;
    Port_OffEe(sd_remote_off, &c);
    return c.r;
}

int sceSdRemoteInit(void) { return 0; }
int sceSdRemoteCallbackInit(int prio) { (void)prio; return 0; }

/* The RSPU2DRV server for Port_SifCall (plat_sys.c): word 0 of the packet is the command, words 1-6 the arguments;
   the reply's word 0 is the result. */
int Port_Rspu2Serve(int fn, void *send, int ssize, void *recv, int rsize) {
    uint32_t *p = send;
    int r;
    (void)fn;
    if (p == NULL || ssize < 28) {
        return 0;
    }
    r = sceSdRemote(1, (int)p[0], (int)p[1], (int)p[2], (int)p[3], (int)p[4], (int)p[5], (int)p[6]);
    if (recv != NULL && rsize >= 4) {
        *(int32_t *)recv = r;
    }
    return rsize;
}

/* Self-test part (plat_sys.c, TTT_SELFTEST=rpc): a 0x204E read lands where the table says, and the status shows it
   pending once. Passes without a disc (nothing to read). */
int Port_FileSelfTest(void) {
    extern uint32_t gFileTable[]; /* the file table: {sector, size + 0x350A, check, id} */
    uint8_t want[64];
    uint8_t *dst = Port_Ptr(0x00180000u);
    uint32_t sector = gFileTable[3 * 4], biased = gFileTable[3 * 4 + 1];
    if (!Port_DiscOpen()) {
        printf("ttt: (no disc: reads not checked)\n");
        return 1;
    }
    if (!read_archive(sector, want, sizeof want)) {
        return 0;
    }
    memset(dst, 0, 64);
    if (sceSdRemote(1, 0x204E, (int)sector, 0x20180000, (int)biased, 0, 0, 0) != 0) {
        return 0;
    }
    if ((sceSdRemote(1, 0x2030, 0, 0, 0, 0, 0, 0) & 1) != 1 || (sceSdRemote(1, 0x2030, 0, 0, 0, 0, 0, 0) & 1) != 0) {
        return 0;
    }
    return memcmp(dst, want, sizeof want) == 0;
}

__attribute__((constructor(101))) static void file_state(void) {
    Port_StateAdd("file.status", &sStatus, sizeof sStatus, PS_HASH);
    Port_StateAdd("file.statusPolls", &sStatusPolls, sizeof sStatusPolls, PS_HASH);
    Port_StateAdd("file.movieStream", sMovieStream, sizeof sMovieStream, PS_HASH);
    Port_StateAdd("file.reads", &gPortFileReads, sizeof gPortFileReads, 0);
    Port_StateAdd("file.bytes", &gPortFileBytes, sizeof gPortFileBytes, 0);
}
