/*
 * The game's files on PC, and the RSPU2DRV commands that carry them.
 *
 * The game reads nothing through libcdvd: it asks the sound driver on the IOP (RSPU2DRV) to read sectors of
 * TEKKEN.BIN into EE memory (command 0x204E) or SPU2 sound RAM (0x2045) and polls the driver's status word (0x2030)
 * every frame (docs/systems/files.md, docs/systems/audio.md). Here sceSdRemote is a direct call: a read is done at
 * once from the user's disc and the status word shows it finished at the next poll, as a fast drive would.
 * The data is the archive as stored (scrambled): the game descrambles it itself.
 *
 * The disc: TTT_DISC=<directory> holding TEKKEN.BIN (the copy scripts/setup_disc.py makes in iso/), or
 * TTT_DISC=<image.iso> (ISO 9660, 2048-byte sectors: TEKKEN.BIN is found in its root directory). Without TTT_DISC,
 * the repository's iso/ (build/port/ttt -> ../../iso), iso/ beside the program, or iso/ in the working directory.
 * Without a disc the first read stops the program (exit code 2). Nothing of the disc is ever written.
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
#include <unistd.h>
#include "port_addr.h"
#include "port_host.h"
#include "spu2.h"
#include "plat_state.h"

int Port_SndCommand(int cmd, const uint32_t *a); /* plat_snd.c */
void Port_SndGet(int cmd, uint8_t *out);
void Port_SndDriverStart(void);
uint32_t Port_BgmStatus(void); /* plat_bgm.c: bits 4 and 6 */

#define SECTOR 2048
#define ARCHIVE_SECTOR0 0x4820 /* the table's sectors count from here (systems/files.md) */
#define SIZE_BIAS 0x350A

static FILE *sDisc;          /* the ISO or TEKKEN.BIN */
static long long sArchive;   /* byte offset of TEKKEN.BIN in it */
static long long sArchiveSize;
static int sDiscTried;
static char sDiscName[1024];

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* TEKKEN.BIN in the root directory of an ISO 9660 image. */
static int find_in_iso(FILE *f) {
    uint8_t pvd[SECTOR], dir[SECTOR];
    uint32_t lba, size, off;
    if (fseeko(f, 16LL * SECTOR, SEEK_SET) != 0 || fread(pvd, 1, SECTOR, f) != SECTOR ||
        memcmp(pvd + 1, "CD001", 5) != 0) {
        return 0;
    }
    lba = le32(pvd + 156 + 2);
    size = le32(pvd + 156 + 10);
    for (off = 0; off < size; off += SECTOR) {
        uint32_t i = 0;
        if (fseeko(f, (long long)(lba + off / SECTOR) * SECTOR, SEEK_SET) != 0 || fread(dir, 1, SECTOR, f) != SECTOR) {
            return 0;
        }
        while (i < SECTOR && dir[i] != 0) {
            uint8_t len = dir[i], nlen = dir[i + 32];
            if (nlen >= 10 && memcmp(dir + i + 33, "TEKKEN.BIN", 10) == 0) {
                sArchive = (long long)le32(dir + i + 2) * SECTOR;
                sArchiveSize = le32(dir + i + 10);
                return 1;
            }
            i += len;
        }
    }
    return 0;
}

/* Opens `name` as the disc: a directory holding TEKKEN.BIN, or an ISO 9660 image. */
static int try_disc(const char *name) {
    char path[1200];
    snprintf(path, sizeof path, "%s/TEKKEN.BIN", name);
    sDisc = fopen(path, "rb");
    if (sDisc != NULL) {
        sArchive = 0;
        fseeko(sDisc, 0, SEEK_END);
        sArchiveSize = ftello(sDisc);
        return 1;
    }
    sDisc = fopen(name, "rb");
    if (sDisc != NULL && find_in_iso(sDisc)) {
        return 1;
    }
    if (sDisc != NULL) {
        fclose(sDisc);
        sDisc = NULL;
    }
    return 0;
}

/* Opens the disc once; 0 when there is none. Without TTT_DISC: the repository's iso/ (build/port/ttt -> ../../iso),
   then iso/ beside the program, then iso/ in the working directory. The program's own directory comes from
   /proc/self/exe (symbolic links resolved), so a copy of the program elsewhere finds only the last two. */
int Port_DiscOpen(void) {
    static const char *const kNear[] = { "/../../iso", "/iso" };
    const char *d;
    char exe[1024];
    ssize_t k;
    unsigned i;
    if (sDiscTried) {
        return sDisc != NULL;
    }
    sDiscTried = 1;
    d = getenv("TTT_DISC");
    if (d != NULL && *d) {
        snprintf(sDiscName, sizeof sDiscName, "%s", d);
        return try_disc(sDiscName);
    }
    k = readlink("/proc/self/exe", exe, sizeof exe - 1);
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

/* The game cannot run without its data: on the PS2 a read error is retried for ever, here a missing disc would let
   the boot go on over memory the reads never filled (it crashed in func_00345410 when the program was copied away
   from the repository's iso/, with the message lost in stdout's buffer). Stop at the first read instead. */
static void no_disc(void) {
    fflush(stdout);
    fprintf(stderr,
            "ttt: no disc: set TTT_DISC to a directory with TEKKEN.BIN or to the game's .iso "
            "(or put iso/ beside the program; last tried %s)\n",
            sDiscName);
    _exit(2);
}

/* `bytes` of the archive from table sector `sector` into `dst`; 0 on a failed read. */
static int read_archive(uint32_t sector, void *dst, uint32_t bytes) {
    long long off = ((long long)sector - ARCHIVE_SECTOR0) * SECTOR;
    if (!Port_DiscOpen() || off < 0 || off + bytes > sArchiveSize) {
        return 0;
    }
    if (fseeko(sDisc, sArchive + off, SEEK_SET) != 0) {
        return 0;
    }
    return fread(dst, 1, bytes, sDisc) == bytes;
}

/* `bytes` from `offset` of the file at table sector `sector` (the BGM streams, plat_bgm.c); 0 on a failed read. */
int Port_DiscRead(uint32_t sector, uint32_t offset, void *dst, uint32_t bytes) {
    long long off = ((long long)sector - ARCHIVE_SECTOR0) * SECTOR + offset;
    if (!Port_DiscOpen() || off < 0 || off + bytes > sArchiveSize || fseeko(sDisc, sArchive + off, SEEK_SET) != 0) {
        return 0;
    }
    return fread(dst, 1, bytes, sDisc) == bytes;
}

/* A file of the disc outside TEKKEN.BIN (`dir`/`name`, e.g. IRX/RSPU2DRV.IRX) into `dst`: its size, or -1. The
   disc directory, or the ISO image's root directory and one subdirectory. */
static long find_iso_entry(uint32_t lba, uint32_t size, const char *name, uint32_t *outLba) {
    uint8_t dir[SECTOR];
    size_t nl = strlen(name);
    uint32_t off;
    for (off = 0; off < size; off += SECTOR) {
        uint32_t i = 0;
        if (fseeko(sDisc, (long long)(lba + off / SECTOR) * SECTOR, SEEK_SET) != 0 || fread(dir, 1, SECTOR, sDisc) != SECTOR) {
            return -1;
        }
        while (i < SECTOR && dir[i] != 0) {
            uint8_t len = dir[i], n = dir[i + 32];
            if (n >= nl && memcmp(dir + i + 33, name, nl) == 0 && (n == nl || dir[i + 33 + nl] == ';')) {
                *outLba = le32(dir + i + 2);
                return (long)le32(dir + i + 10);
            }
            i += len;
        }
    }
    return -1;
}

long Port_DiscReadFile(const char *dirName, const char *name, void *dst, long max) {
    char path[1300];
    FILE *f;
    long n;
    if (!Port_DiscOpen()) {
        return -1;
    }
    if (sArchive == 0) { /* a directory */
        snprintf(path, sizeof path, "%s/%s/%s", sDiscName, dirName, name);
        f = fopen(path, "rb");
        if (f == NULL) {
            return -1;
        }
        n = (long)fread(dst, 1, (size_t)max, f);
        fclose(f);
        return n;
    } else { /* an ISO image */
        uint8_t pvd[SECTOR];
        uint32_t lba;
        long size;
        if (fseeko(sDisc, 16LL * SECTOR, SEEK_SET) != 0 || fread(pvd, 1, SECTOR, sDisc) != SECTOR) {
            return -1;
        }
        size = find_iso_entry(le32(pvd + 156 + 2), le32(pvd + 156 + 10), dirName, &lba);
        if (size < 0 || (size = find_iso_entry(lba, (uint32_t)size, name, &lba)) < 0) {
            return -1;
        }
        n = size < max ? size : max;
        if (fseeko(sDisc, (long long)lba * SECTOR, SEEK_SET) != 0 || fread(dst, 1, (size_t)n, sDisc) != (size_t)n) {
            return -1;
        }
        return n;
    }
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
    SdCall c = { cmd, 0, { (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3, (uint32_t)a4, (uint32_t)a5 } };
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
