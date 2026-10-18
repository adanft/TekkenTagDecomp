#ifndef FILE_H
#define FILE_H

#include "types.h"

/* One file of TEKKEN.BIN (gFileTable, 748 entries): see docs/systems/files.md. */
typedef struct {
    /* 0x0 */ u32 sector;  /* archive sector + 0x4820 */
    /* 0x4 */ s32 size;    /* size in bytes + FILE_SIZE_BIAS */
    /* 0x8 */ u32 check;
    /* 0xC */ u32 id;      /* id of an EE-loaded file, 0xFFFF for IOP data */
} FileEntry;

#define FILE_SIZE_BIAS 0x350A

/* A read request (gFileReqs[24]). `state`: 1 free, 2 queued, 3 queued marker (id 0xFFF), 4 reading,
 * 5 marker reached, 6 done. */
typedef struct {
    /* 0x00 */ s32 id;
    /* 0x04 */ u32 dest;    /* uncached EE address, or SPU2 address when toIop */
    /* 0x08 */ u32 sector;
    /* 0x0C */ s32 size;
    /* 0x10 */ s32 handle;  /* index of the request, returned by File_Request */
    /* 0x14 */ u16 state;
    /* 0x16 */ u8 flags;
    /* 0x17 */ u8 toIop;
} FileReq;

#define FILE_REQ_COUNT 24

/* Requests in issue order (the head is served first); `busy` is set while File_Request adds one. */
typedef struct {
    /* 0x00 */ FileReq *req[FILE_REQ_COUNT];
    /* 0x60 */ s32 busy;
} FileQueue;

#endif
