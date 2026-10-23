/* 0x3567A8-0x356A70 (module theater, docs/game_overview.md section 4; docs/systems/modes.md). */
#include "common.h"

/* GIF packet of the SDK (sceGifPacket). */
typedef struct {
    /* 0x0 */ u32 *pCurrent;
    /* 0x4 */ u32 *pBase;
    /* 0x8 */ u32 *pDmaTag;
    /* 0xC */ u32 *pGifTag;
} GifPacket;

/* VIF1 packet of the SDK (sceVif1Packet). */
typedef struct {
    /* 0x0 */ u32 *pCurrent;
    /* 0x4 */ u32 *pBase;
    /* 0x8 */ u32 *pDmaTag;
    /* 0xC */ u32 *pVifCode;
    /* 0x10 */ u32 pad10[4];
} Vif1Packet;

/* An entry of the list func_00343CA8 returns (12 bytes): a DMA chain. */
typedef struct {
    /* 0x0 */ u32 *chain;
    /* 0x4 */ u32 unk4;
    /* 0x8 */ u32 unk8;
} ThChain;

extern u8 D_00521CE0[];
extern u8 D_00521D60[];
extern u8 D_00521D80[];
extern u8 D_00521E00[];
extern u8 D_00455D50[];
extern u8 D_00455D68[];

u32 Spr_Setup(u32 count, s32 reserved);
void func_00343178(u32 addr, s32 qwc);
void sceGifPkInit(GifPacket *pk, u32 base);
void sceGifPkRef(GifPacket *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceGifPkTerminate(GifPacket *pk);
s32 sceGifPkSize(GifPacket *pk);
void sceVif1PkInit(Vif1Packet *pk, u32 base);
void sceVif1PkRef(Vif1Packet *pk, void *addr, u32 size, u32 opt1, u32 opt2, u32 flag);
void sceVif1PkTerminate(Vif1Packet *pk);
s32 sceVif1PkSize(Vif1Packet *pk);
void func_00343CA8(s32 *n, ThChain **list, s32 id);
void func_003568E0(s32 id, u32 ofs);

RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/theater_d", D_00521CE0);
INCLUDE_RODATA("asm/nonmatchings/theater_d", D_00521D60);
/* Sends the two GIF blocks D_00521CE0 (8 qwords) and D_00521D60 (2) through the scratchpad. */
void func_003567A8(void) {
    GifPacket pk;
    u32 base;

    base = Spr_Setup(1, 0);
    sceGifPkInit(&pk, base);
    sceGifPkRef(&pk, D_00521CE0, 8, 0, 0, 0);
    sceGifPkRef(&pk, D_00521D60, 2, 0, 0, 0);
    sceGifPkTerminate(&pk);
    func_00343178(base, sceGifPkSize(&pk));
}

INCLUDE_RODATA("asm/nonmatchings/theater_d", D_00521D80);
INCLUDE_RODATA("asm/nonmatchings/theater_d", D_00521E00);
/* Sends the VIF1 blocks D_00521D80 (8 qwords, DIRECT) and D_00521E00 (2). */
void func_00356840(void) {
    Vif1Packet pk;
    u32 base;

    base = Spr_Setup(1, 0);
    sceVif1PkInit(&pk, base);
    sceVif1PkRef(&pk, D_00521D80, 8, 0x11000000, 0x50000008, 0);
    sceVif1PkRef(&pk, D_00521E00, 2, 0, 0x50000002, 0);
    sceVif1PkTerminate(&pk);
    func_00343178(base, sceVif1PkSize(&pk));
}

/* Walks the DMA chains of list `id` and adds `ofs` to the two 14-bit address fields of every tag-2 record. */
#if 0
/* NON_MATCHING: 13 lines: the loop's n-1 / list+12 temporaries (t3/t4) and the OR chain registers swapped */
void func_003568E0(s32 id, u32 ofs) {
    union {
        u128 q;
        u32 w[4];
    } tag;
    s32 cnt0;
    ThChain *list0;
    s32 n;
    ThChain *list;
    u32 *p;
    u32 *q;
    u32 end;
    u32 i;
    u32 cnt;
    u64 w;
    u64 w2;

    func_00343CA8(&cnt0, &list0, id);
    list = list0;
    n = cnt0;
    while (n != 0) {
        p = list->chain;
        do {
            tag.q = *(u128 *)p;
            switch (tag.w[3]) {
            case 1:
                end = (p[0] & 0x8000) >> 15;
                p += 12;
                break;
            case 0:
                end = 1;
                break;
            case 2:
                w = *(u64 *)(p + 6);
                q = p + 12;
                cnt = p[8] & 0x7FFF;
                end = (p[8] & 0x8000) >> 15;
                *(u64 *)(p + 4) = ((w + ((u64)ofs << 37)) & (0x3FFFULL << 37)) | ((w + ofs) & 0x3FFF) | (w & ~(0x3FFFULL | (0x3FFFULL << 37)));
                for (i = 0; i < cnt; i++) {
                    q = (u32 *)((u8 *)q + (((q[3] & 7) << 5) + 0x20));
                }
                p = q;
                break;
            }
        } while (end == 0);
        n--;
        list++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/theater_d", func_003568E0);
#endif

/* Stage tables of 23 bytes: D_00455D50[i], 0 past the end. */
PORT_RET(u8) func_00356A20(s32 i) {
    if (i < 23) {
        return D_00455D50[i];
    }
    return 0;
}

PORT_RET(u8) func_00356A48(s32 i) {
    if (i < 23) {
        return D_00455D68[i];
    }
    return 0;
}
