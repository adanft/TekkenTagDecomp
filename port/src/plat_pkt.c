/*
 * libpkt: the SDK's packet builders the game makes its DMA lists with (VIF1 packets with DMA tags, VIF codes and GIF
 * tags; GIF packets). Written after the library's own code (asm/B4320.s, 0x404500-0x404AA0; checked instruction by
 * instruction in T29 b4, TTT_SELFTEST=hw):
 *
 *  - the packet struct: pCurrent 0x0, pBase 0x4, pDmaTag 0x8, pVifCode 0xC, pGifTag 0x14 (VIF1 packets); Init and
 *    Reset write only the first three;
 *  - Terminate pads with zero words to a quadword, ADDS (end - tag) / 16 - 1 to the open DMA tag's QWC, clears
 *    pDmaTag and returns pCurrent; Cnt / Ref / End (and the GIF ones) terminate first;
 *  - Cnt writes the tag's lower half (flag | 0x10000000, 0) and leaves its upper half to the VIF codes; Ref writes a
 *    whole tag (flag | size | 0x30000000, addr & 0x9FFFFFFF, opt1, opt2) and leaves no tag open;
 *  - Open*Code aligns to the quadword's last word (zero words) and writes 0x50 / 0x51 << 24, bit 31 for any non-zero
 *    irq; Close*Code ADDS the whole quadwords written after the code (rounded down);
 *  - CloseGifTag ADDS ceil(n / NREG) to NLOOP (n: quadwords after the tag for PACKED, doublewords for REGLIST /
 *    IMAGE; NREG 0 is 16) and pads with zero words to a quadword: Gfx_PkAddImage presets NLOOP in an IMAGE tag and
 *    closes it with nothing after it, so NLOOP must stay;
 *  - Size is (pCurrent - pBase) / 16, rounded down.
 */
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t *pCurrent;
    uint32_t *pBase;
    uint32_t *pDmaTag;
    uint32_t *pVifCode; /* unused by the GIF packet functions */
    uint32_t unk10;
    uint32_t *pGifTag; /* VIF1 packets */
} Packet;

/* Zero words up to the next quadword. */
static uint32_t *pad_qw(uint32_t *p) {
    while ((uint32_t)(uintptr_t)p & 0xC) {
        *p++ = 0;
    }
    return p;
}

static uint32_t *terminate(Packet *pk) {
    uint32_t *p = pad_qw(pk->pCurrent);

    if (pk->pDmaTag != NULL) {
        pk->pDmaTag[0] += (uint32_t)(((int32_t)((uint8_t *)p - (uint8_t *)pk->pDmaTag) >> 4) - 1);
    }
    pk->pDmaTag = NULL;
    pk->pCurrent = p;
    return p;
}

/* Ref of either packet kind: a whole tag, no tag left open. */
static void ref(Packet *pk, void *addr, uint32_t size, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    uint32_t *t = terminate(pk);

    t[0] = flag | size | 0x30000000u;
    t[1] = (uint32_t)(uintptr_t)addr & 0x9FFFFFFFu;
    t[2] = opt1;
    t[3] = opt2;
    pk->pCurrent = t + 4;
}

/* ---- VIF1 packets ---- */
void sceVif1PkInit(Packet *pk, void *base) {
    pk->pCurrent = base;
    pk->pDmaTag = NULL;
    pk->pBase = base;
}
void sceVif1PkReset(Packet *pk) {
    pk->pDmaTag = NULL;
    pk->pCurrent = pk->pBase;
}
uint32_t *sceVif1PkTerminate(Packet *pk) { return terminate(pk); }
uint32_t sceVif1PkSize(Packet *pk) { return (uint32_t)((uint8_t *)pk->pCurrent - (uint8_t *)pk->pBase) >> 4; }
void sceVif1PkCnt(Packet *pk, uint32_t flag) {
    uint32_t *t = terminate(pk);

    pk->pDmaTag = t;
    t[0] = flag | 0x10000000u;
    pk->pVifCode = NULL;
    t[1] = 0;
    pk->pCurrent = t + 2;
}
void sceVif1PkRef(Packet *pk, void *addr, uint32_t size, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    ref(pk, addr, size, opt1, opt2, flag);
}
/* Not in the executable; the counterpart of sceVif1PkCnt with ID end. */
void sceVif1PkEnd(Packet *pk, uint32_t flag) {
    uint32_t *t = terminate(pk);

    pk->pDmaTag = t;
    t[0] = flag | 0x70000000u;
    pk->pVifCode = NULL;
    t[1] = 0;
    pk->pCurrent = t + 2;
}
uint32_t *sceVif1PkReserve(Packet *pk, uint32_t count) {
    uint32_t *p = pk->pCurrent;
    pk->pCurrent = p + count;
    return p;
}
/* Zero words up to the next word whose index within a 2^(bit + 2)-byte block is pos. */
void sceVif1PkAlign(Packet *pk, uint32_t bit, uint32_t pos) {
    uint32_t mask = 0xFFFFFFFFu >> (32 - ((bit + 2) & 31));
    uint32_t cur = (uint32_t)(uintptr_t)pk->pCurrent;
    uint32_t end = (cur & ~mask) + pos * 4;
    uint32_t *p = pk->pCurrent;

    if (end < cur) {
        end += mask + 1;
    }
    if (cur < end) {
        while ((uint32_t)(uintptr_t)p < end) {
            *p++ = 0;
        }
        pk->pCurrent = p;
    }
}
static void open_direct(Packet *pk, uint32_t code, int irq) {
    uint32_t *p;

    sceVif1PkAlign(pk, 2, 3);
    p = pk->pCurrent;
    pk->pCurrent = p + 1;
    pk->pVifCode = p;
    *p = irq != 0 ? code | 0x80000000u : code;
}
static void close_direct(Packet *pk) {
    uint32_t *c = pk->pVifCode;

    pk->pVifCode = NULL;
    *c += (uint32_t)((int32_t)((uint8_t *)(pk->pCurrent - 1) - (uint8_t *)c) >> 2) >> 2;
}
void sceVif1PkOpenDirectCode(Packet *pk, int irq) { open_direct(pk, 0x50000000u, irq); }
void sceVif1PkCloseDirectCode(Packet *pk) { close_direct(pk); }
void sceVif1PkOpenDirectHLCode(Packet *pk, int irq) { open_direct(pk, 0x51000000u, irq); }
void sceVif1PkCloseDirectHLCode(Packet *pk) { close_direct(pk); }
/* The GIF tag is passed by value (u128: four words on the stack). */
void sceVif1PkOpenGifTag(Packet *pk, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3) {
    uint32_t *p = pk->pCurrent;

    p[0] = t0;
    p[1] = t1;
    p[2] = t2;
    p[3] = t3;
    pk->pCurrent = p + 4;
    pk->pGifTag = p;
}
void sceVif1PkCloseGifTag(Packet *pk) {
    uint32_t *g = pk->pGifTag, *p = pk->pCurrent;
    uint64_t tag = g[0] | (uint64_t)g[1] << 32;
    uint32_t dw = (uint32_t)(((int32_t)((uint8_t *)p - (uint8_t *)g) >> 3) - 2);
    uint32_t nreg = (uint32_t)(tag >> 60) != 0 ? (uint32_t)(tag >> 60) : 16;
    uint32_t n = (tag >> 58) & 3 ? dw : dw >> 1;

    tag += (n + nreg - 1) / nreg;
    g[0] = (uint32_t)tag;
    g[1] = (uint32_t)(tag >> 32);
    pk->pGifTag = NULL;
    pk->pCurrent = pad_qw(p);
}
void sceVif1PkAddGsAD(Packet *pk, uint32_t addr, uint32_t lo, uint32_t hi) {
    uint32_t *p = pk->pCurrent;

    p[0] = lo;
    p[1] = hi;
    p[2] = addr;
    p[3] = 0;
    pk->pCurrent = p + 4;
}

/* ---- GIF packets ---- */
void sceGifPkInit(Packet *pk, void *base) {
    pk->pCurrent = base;
    pk->pDmaTag = NULL;
    pk->pBase = base;
}
/* Not in the executable: as sceVif1PkReset. */
void sceGifPkReset(Packet *pk) {
    pk->pDmaTag = NULL;
    pk->pCurrent = pk->pBase;
}
/* Not in the executable: as sceGifPkEnd with ID cnt. */
void sceGifPkCnt(Packet *pk, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    uint32_t *t = terminate(pk);

    pk->pDmaTag = t;
    t[0] = flag | 0x10000000u;
    t[1] = 0;
    t[2] = opt1;
    t[3] = opt2;
    pk->pCurrent = t + 4;
}
void sceGifPkRef(Packet *pk, void *addr, uint32_t size, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    ref(pk, addr, size, opt1, opt2, flag);
}
void sceGifPkEnd(Packet *pk, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    uint32_t *t = terminate(pk);

    pk->pDmaTag = t;
    t[0] = flag | 0x70000000u;
    t[1] = 0;
    t[2] = opt1;
    t[3] = opt2;
    pk->pCurrent = t + 4;
}
uint32_t *sceGifPkReserve(Packet *pk, uint32_t count) {
    uint32_t *p = pk->pCurrent;
    pk->pCurrent = p + count;
    return p;
}
uint32_t *sceGifPkTerminate(Packet *pk) { return terminate(pk); }
uint32_t sceGifPkSize(Packet *pk) { return (uint32_t)((uint8_t *)pk->pCurrent - (uint8_t *)pk->pBase) >> 4; }
