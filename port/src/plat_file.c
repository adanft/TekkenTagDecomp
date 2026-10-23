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
 * the repository's iso/ next to the program (build/port/ttt -> ../../iso). Nothing of the disc is ever written.
 *
 * Every other RSPU2DRV command (sound effects, BGM, the sound queue, movies) is answered harmlessly for now: keys
 * report off, the stream engine idle, the movie stream refuses to start. The real sound comes in T35+.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "port_addr.h"

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

static void default_disc(char *out, size_t n) {
    char exe[1024];
    ssize_t k = readlink("/proc/self/exe", exe, sizeof exe - 1);
    char *slash;
    out[0] = 0;
    if (k <= 0) {
        return;
    }
    exe[k] = 0;
    slash = strrchr(exe, '/');
    if (slash != NULL) {
        *slash = 0;
        snprintf(out, n, "%s/../../iso", exe); /* build/port/ttt -> iso/ */
    }
}

/* Opens the disc once; 0 when there is none (reads then fail and the game waits, as with an empty drive). */
int Port_DiscOpen(void) {
    const char *d;
    char path[1200];
    if (sDiscTried) {
        return sDisc != NULL;
    }
    sDiscTried = 1;
    d = getenv("TTT_DISC");
    if (d != NULL && *d) {
        snprintf(sDiscName, sizeof sDiscName, "%s", d);
    } else {
        default_disc(sDiscName, sizeof sDiscName);
    }
    snprintf(path, sizeof path, "%s/TEKKEN.BIN", sDiscName);
    sDisc = fopen(path, "rb");
    if (sDisc != NULL) {
        sArchive = 0;
        fseeko(sDisc, 0, SEEK_END);
        sArchiveSize = ftello(sDisc);
        return 1;
    }
    sDisc = fopen(sDiscName, "rb");
    if (sDisc != NULL && find_in_iso(sDisc)) {
        return 1;
    }
    if (sDisc != NULL) {
        fclose(sDisc);
        sDisc = NULL;
    }
    printf("ttt: no disc: set TTT_DISC to a directory with TEKKEN.BIN or to the game's .iso (tried %s)\n",
           sDiscName);
    return 0;
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

/* ---- RSPU2DRV ---- */
static uint8_t sSpuRam[2 * 1024 * 1024]; /* SPU2 sound RAM: 0x2045 reads land here (the sound of T35+ plays it) */
static uint32_t sStatus = 0x40;          /* 0x2030: bit 0 read pending, bit 2 disc error, bit 6 stream idle */
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
        if (dest >= sizeof sSpuRam || size > sizeof sSpuRam - dest) {
            sStatus |= 4;
            return 0;
        }
        dst = sSpuRam + dest;
    } else {
        size = (size + 15) & ~15u;
        if (!Port_IsEeRam(dest) || Port_EePhys(dest) + size > PORT_EE_RAM_SIZE) {
            sStatus |= 4;
            return 0;
        }
        dst = Port_DmaPtr(dest);
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
        uint32_t s = sStatus;
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
    case 0x2000: /* driver start (finds TEKKEN.BIN on the disc) */
        Port_DiscOpen();
        return 0;
    case 0x2058: /* movie stream start: no movies yet, refused (the player gives up on -1) [T: movie task] */
        return -1;
    case 0x2059: /* next movie chunk */
        return -1;
    case 0x205A: /* 0: stop the movie stream; 1: back to file mode, non-zero once there (func_0039C878 waits on it) */
        return a[0] == 1 ? 1 : 0;
    default:
        /* 1 SpuInit, 2 SpuSetCore, 5 key off, 0x7xxx attribute sets, 0x7F0F sound queue, 0x2051 semaphore tick,
           0x205E BGM track table: accepted, nothing to do yet. */
        return 0;
    }
}

/* sceSdRemote(mode, command, six argument words): the game's only door to the IOP (librspu2). */
int sceSdRemote(int mode, int cmd, int a0, int a1, int a2, int a3, int a4, int a5) {
    uint32_t a[6] = { (uint32_t)a0, (uint32_t)a1, (uint32_t)a2, (uint32_t)a3, (uint32_t)a4, (uint32_t)a5 };
    (void)mode;
    if ((cmd & 0xF000) == 0x6000) { /* get: 0x40 bytes into the buffer a0 points to; keys off, attributes zero */
        if (a0 != 0) {
            memset(Port_Ptr(a0), 0, 0x40);
        }
        return 0;
    }
    if (cmd >= 0x8100 && cmd <= 0x8600) { /* EE callbacks: never called (the game polls) */
        return 0;
    }
    return rspu2(cmd, a);
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
