/* 0x397948-0x398510: file loader (requests served by the IOP) and IOP status. docs/game_overview.md section 7,
 * docs/systems/files.md, docs/systems/audio.md section 5. */
#include "common.h"
#include "file.h"

/* A BGM track of the IOP stream engine (RSPU2DRV command 0x205E). */
typedef struct {
    /* 0x0 */ u8 type;   /* 1 disc stream, 2, 3 streamed from EE memory */
    /* 0x1 */ s8 unk1;
    /* 0x2 */ u16 unk2;  /* 0x2000-0x3CCC: volume or pitch */
    /* 0x4 */ s16 unk4;
    /* 0x6 */ u16 fileId;
    /* 0x8 */ s16 loopSector;
} BgmTrack;

extern s32 gSifCmdPending;
extern u32 gEeStreamBase;
extern s32 D_005F61D8;
extern s32 D_005F61DC;
extern s32 gIopStatus;
extern s32 gFilePending;
extern s32 D_005F6C68;
extern s32 D_005F6C6C;
extern FileReq gFileReqs[FILE_REQ_COUNT];
extern FileQueue gFileQueue;
extern u8 D_008BCFF0[];
extern u32 gSifCmdBuffer[4];
extern u8 D_008BD090[];
extern FileEntry gFileTable[];

void exit(s32 status) __attribute__((noreturn));
void FlushCache(s32 op);
s32 sceSifInitRpc(s32 mode);
void func_00401F58(void *buf, s32 size);
s32 sceSifAddCmdHandler(s32 cid, void (*handler)(void *, void *), void *data);
u32 sceSifSendCmd(s32 cid, void *packet, s32 packetSize, void *srcExtra, void *destExtra, s32 sizeExtra);
s32 sceSdRemote(s32 arg, s32 command, ...);
s32 sceGsSyncV(s32 mode);
s32 File_Update(void);
s32 File_Release(s32 handle);
s32 File_CountQueued(void);

/* The 28 BGM tracks, read from the original executable (INCLUDE_RODATA). */
extern const BgmTrack D_0050CD00[28];

INCLUDE_RODATA("asm/nonmatchings/file", gFileTable);

INCLUDE_RODATA("asm/nonmatchings/file", D_0050CD00);

/* SIF command 0 from the IOP (movie chunk sent): nothing to do. */
void File_SifCmd0Handler(void *packet, void *data) {
    __asm__ volatile("sync.l");
    __asm__ volatile("ei");
    __asm__ volatile("ei");
}

/* Answers a pending SIF command 1 of the IOP: sends it a part of the BGM held in EE memory. */
void File_SendSifCmd(void) {
    if (gSifCmdPending != 0) {
        FlushCache(0);
        if (gEeStreamBase == 0) {
            exit(1);
        }
        sceSifSendCmd(0, D_008BD090, 0x10, (void *)(gEeStreamBase + gSifCmdBuffer[0] * 0x800), (void *)gSifCmdBuffer[1],
                      gSifCmdBuffer[2] * 0x800);
        gSifCmdPending = 0;
    }
}

/* SIF command 1 from the IOP (stream data wanted from EE memory): flags it for File_SendSifCmd. */
void File_SifCmdHandler(void *packet, void *data) {
    gSifCmdPending = 1;
    __asm__ volatile("sync.l");
    __asm__ volatile("ei");
    __asm__ volatile("ei");
}

/* Uncached address of the buffer the IOP writes its SIF command 1 arguments to. */
u32 File_SifCmdBufferAddr(void) {
    return ((u32)gSifCmdBuffer & 0x0FFFFFFF) | 0x20000000;
}

/* Gives the IOP stream engine its 28 BGM tracks (track 7 starts 0xCF7 sectors into its file). */
void File_RegisterIopFiles(void) {
    u32 i;
    u32 sector;
    s32 size;
    s32 skip = 0xCF7 * 0x800;

    for (i = 0; i < 28; i++) {
        if (i != 7) {
            sector = gFileTable[D_0050CD00[i].fileId].sector;
            size = gFileTable[D_0050CD00[i].fileId].size;
        } else {
            sector = gFileTable[D_0050CD00[i].fileId].sector + 0xCF7;
            size = gFileTable[D_0050CD00[i].fileId].size - skip;
        }
        sceSdRemote(1, 0x205E, i, D_0050CD00[i].type | (D_0050CD00[i].unk1 << 8),
                    D_0050CD00[i].unk2 | (D_0050CD00[i].unk4 << 16), sector, size, D_0050CD00[i].loopSector);
    }
}

/* Boot: installs the SIF command handlers, frees every request slot and registers the BGM tracks. */
void File_Init(void) {
    FileReq *req = gFileReqs;
    s32 i;
    u32 status;

    gSifCmdPending = 0;
    sceSifInitRpc(0);
    do {
        __asm__ volatile("di");
        __asm__ volatile("sync.p");
        __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    } while (status & 0x10000);
    func_00401F58(D_008BCFF0, 0x10);
    sceSifAddCmdHandler(0, File_SifCmd0Handler, &D_005F6C68);
    sceSifAddCmdHandler(1, File_SifCmdHandler, &D_005F6C6C);
    __asm__ volatile("sync.l");
    __asm__ volatile("ei");
    __asm__ volatile("ei");
    for (i = 0; i < FILE_REQ_COUNT; i++) {
        req->handle = i;
        req->id = -1;
        req->state = 1;
        req->dest = req->size = req->sector = 0;
        req++;
    }
    for (i = 0; i < FILE_REQ_COUNT; i++) {
        gFileQueue.req[i] = NULL;
    }
    gFilePending = 0;
    gFileQueue.busy = 0;
    sceSdRemote(1, 0x2051);
    D_005F61DC = D_005F61D8 = -1;
    File_RegisterIopFiles();
}

/* Releases every queued request, waits until the IOP is idle and frees all slots. */
s32 File_WaitAll(void) {
    FileReq *req;
    s32 i;

    if (File_CountQueued() > 0) {
        for (i = FILE_REQ_COUNT - 1; i >= 0; i--) {
            if (gFileQueue.req[i] != NULL) {
                File_Release(gFileQueue.req[i]->handle);
            }
        }
        do {
            sceGsSyncV(0);
        } while (File_Update() != 0);
    }
    req = gFileReqs;
    for (i = 0; i < FILE_REQ_COUNT; i++) {
        req->handle = i;
        req->id = -1;
        req->dest = 0;
        req->sector = 0;
        req->size = 0;
        req->state = 1;
        req++;
        gFilePending = 0;
    }
    return 0;
}

/* First free request slot, or NULL. */
FileReq *File_AllocRequest(void) {
    FileReq *req = gFileReqs;
    s32 i;

    for (i = 0; i < FILE_REQ_COUNT; i++) {
        if (req->state == 1) {
            return req;
        }
        req++;
    }
    return NULL;
}

/* Drops the finished and freed requests from the queue, closes the gaps and returns how many are left. */
s32 File_CompactQueue(void) {
    s32 i;
    s32 j;
    s32 count;

    for (i = 0; i < FILE_REQ_COUNT; i++) {
        if (gFileQueue.req[i] != NULL && (gFileQueue.req[i]->state == 6 || gFileQueue.req[i]->state == 1)) {
            gFileQueue.req[i] = NULL;
        }
    }
    for (i = 0; i < FILE_REQ_COUNT; i++) {
        if (gFileQueue.req[i] == NULL) {
            for (j = i; j < FILE_REQ_COUNT - 1; j++) {
                gFileQueue.req[j] = gFileQueue.req[j + 1];
            }
        }
    }
    count = 0;
    for (i = 0; i < FILE_REQ_COUNT; i++) {
        if (gFileQueue.req[i] != NULL) {
            count++;
        }
    }
    return count;
}

/* Queues a read of file `id` to `dest` (16-byte aligned; an SPU2 address when `toIop`). Id 0xFFF queues a
 * marker that reads nothing. Returns the request handle, -1 when every slot is taken. */
#if 0
/* NON_MATCHING: one pair swapped: the queue address (addiu) is scheduled after the toIop store in the original. */
s32 File_Request(s32 id, u32 dest, s32 flags, s32 toIop) {
    FileReq *req;
    s32 i;

    if (dest & 0xF) {
        exit(1);
    }
    req = File_AllocRequest();
    if (req == NULL) {
        return -1;
    }
    for (i = 0; i < FILE_REQ_COUNT; i++) {
        if (gFileQueue.req[i] == NULL) {
            gFileQueue.req[i] = req;
            break;
        }
    }
    if (id != 0xFFF) {
        req->id = id;
        if (toIop == 0) {
            req->dest = (dest & 0x0FFFFFFF) | 0x20000000;
        } else {
            req->dest = dest;
        }
        req->size = -1;
        req->state = 2;
    } else {
        req->id = id;
        req->dest = dest;
        req->size = -1;
        req->state = 3;
    }
    req->flags = flags;
    req->toIop = toIop;
    gFileQueue.busy = 1;
    FlushCache(0);
    gFilePending++;
    return req->handle;
}
#else
INCLUDE_ASM("asm/nonmatchings/file", File_Request);
#endif

/* Frees request `handle`; a read the IOP is running is aborted. */
s32 File_Release(s32 handle) {
    FileReq *req = &gFileReqs[handle];
    s32 isHead = gFileQueue.req[0] == req;

    switch (req->state) {
    case 1:
        break;
    case 4:
        if (isHead) {
            sceSdRemote(1, 0x2050);
            req->state = 1;
        }
        gFilePending--;
        break;
    case 5:
        if (isHead) {
            req->state = 1;
        }
        gFilePending--;
        break;
    case 2:
    case 3:
    case 6:
        req->state = 1;
        gFilePending--;
        break;
    }
    File_CompactQueue();
    return 0;
}

/* First key of the descrambling stream of file `id`. */
s64 File_KeySeed(s64 id) {
    return (id + 0xF3C) * 218319 + (id - 0x348F) * 0x1234567;
}

/* Next key of the descrambling stream. */
s64 File_KeyNext(s64 key) {
    return key * 0x71C3AB9 + 0x3941;
}

/* Once per frame: reads the IOP status, answers its SIF requests and steps the head request. Returns the number
 * of requests still queued. */
#if 0
/* NON_MATCHING: one pair swapped in state 2: the original computes both table addresses before loading the sector. */
s32 File_Update(void) {
    FileReq *req;
    u32 *p;
    s64 key;
    s32 size;
    u32 count;
    u32 i;
    s32 ret;

    gIopStatus = sceSdRemote(1, 0x2030);
    File_SendSifCmd();
    req = gFileQueue.req[0];
    if (req != NULL) {
        switch (req->state) {
        case 1:
        case 5:
        case 6:
            break;
        case 2:
            if (gIopStatus & 1) {
                break;
            }
            req->sector = gFileTable[req->id].sector;
            req->size = gFileTable[req->id].size;
            if (req->toIop == 0) {
                ret = sceSdRemote(1, 0x204E, req->sector, req->dest, req->size, req->flags);
            } else {
                ret = sceSdRemote(1, 0x2045, req->sector, req->dest, req->size, req->flags);
            }
            if (ret != 0) {
                break;
            }
            req->state = 4;
            break;
        case 3:
            req->state = 5;
            break;
        case 4:
            if (gIopStatus & 1) {
                break;
            }
            if (req->toIop == 0) {
                p = (u32 *)req->dest;
                key = File_KeySeed(req->id);
                size = 0x800;
                if (req->size < 0x800) {
                    size = req->size;
                }
                if (req->id != 0x2C) {
                    count = (u32)size >> 2;
                    for (i = 0; i < count; i++) {
                        *p ^= (s32)key;
                        key = File_KeyNext(key);
                        p++;
                    }
                }
            }
            FlushCache(0);
            req->state = 6;
            break;
        }
    }
    gFileQueue.busy = 0;
    return File_CompactQueue();
}
#else
RODATA_ALIGN16();
INCLUDE_ASM("asm/nonmatchings/file", File_Update);
#endif

/* Runs File_Update once per VSync until nothing is queued. */
void File_WaitDone(s32 unused) {
    do {
        sceGsSyncV(0);
    } while (File_Update() != 0);
}

/* 1 while request `handle` is not done. */
s32 File_IsBusy(s32 handle) {
    if (handle < 0) {
        return 0;
    }
    return gFileReqs[handle].state != 6;
}

s32 File_CountQueued(void) {
    return File_CompactQueue();
}

/* Sector of movie `n` (files 14-16). */
u32 File_GetMovieSector(s32 n) {
    return gFileTable[n + 14].sector;
}

s32 File_Exists(s32 id) {
    return gFileTable[id].size - FILE_SIZE_BIAS > 0;
}

s32 File_GetSize(s32 id) {
    return gFileTable[id].size - FILE_SIZE_BIAS;
}

/* Reads file `id` to `dest` and waits for it; returns its size. */
s32 File_LoadSync(s32 id, u32 dest) {
    s32 handle = File_Request(id, dest, 0, 0);

    File_WaitDone(1);
    FlushCache(0);
    File_Release(handle);
    return gFileTable[id].size - FILE_SIZE_BIAS;
}

/* Reads file `id` to SPU2 RAM address `dest` and waits for it; returns its size. */
s32 File_LoadSyncIop(s32 id, u32 dest) {
    s32 handle = File_Request(id, dest, 0, 1);

    File_WaitDone(1);
    File_Release(handle);
    return gFileTable[id].size - FILE_SIZE_BIAS;
}

/* 1 when request `handle` is the head of the queue and has reached its marker. */
s32 File_IsMarkerReached(s32 handle) {
    s32 ret = 0;

    if (gFileQueue.req[0] != NULL && gFileQueue.req[0]->handle == handle) {
        ret = gFileQueue.req[0]->state == 5;
    }
    return ret;
}

/* 1 while a BGM track plays (IOP status bit 4). */
s32 Iop_BgmBusy(void) {
    return (gIopStatus >> 4) & 1;
}

s32 Iop_GetStatus(void) {
    return gIopStatus;
}

/* Waits for the queue, then frees the request in `*handle` and sets it to -1. */
void File_WaitRelease(s32 *handle) {
    File_WaitDone(1);
    File_Release(*handle);
    FlushCache(0);
    *handle = -1;
}

/* 1 while the IOP stream engine is not idle (IOP status bit 6 clear). */
s32 Iop_StreamBusy(void) {
    return ((gIopStatus >> 6) ^ 1) & 1;
}

void Iop_SetEeStreamBase(u32 base) {
    gEeStreamBase = base;
}

void Iop_ClearEeStreamBase(void) {
    gEeStreamBase = 0;
}
