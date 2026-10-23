/*
 * libpkt: the SDK's packet builders the game makes its DMA lists with (VIF1 packets with DMA tags, VIF codes and GIF
 * tags; GIF packets). They only write into the game's buffers and keep four pointers; the layout follows the
 * SDK's documented behaviour:
 *
 *  - a DMA tag takes a whole quadword; in a VIF1 packet its upper 64 bits carry the first two VIF codes (the
 *    channel runs with TTE), in a GIF packet they are padding;
 *  - Cnt / Ref / End close the previous tag (its QWC becomes the quadwords written after it) and open a new one;
 *  - Open*Code puts the VIF code in the last word of a quadword (DIRECT data must start on a quadword), padding
 *    with VIF NOPs, and Close*Code writes the quadword count into it; Open/CloseGifTag do the same for NLOOP;
 *  - Terminate pads to a quadword with NOPs and closes the last tag; Size is in quadwords.
 *
 * [I: field use beyond the four documented pointers, and the exact padding words, are not checked against the
 * library code; the GS path (T28) will check the lists it receives.]
 */
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t *pCurrent;
    uint32_t *pBase;
    uint32_t *pDmaTag;
    uint32_t *pVifCode; /* GIF packet: pGifTag */
    uint32_t *pGifTag;  /* VIF1 packet only (its 0x10 words) */
    uint32_t pad[3];
} Packet;

static uint32_t qw_index(const uint32_t *p) { return (uint32_t)(uintptr_t)p >> 2 & 3; }

/* Closes the open DMA tag: QWC = quadwords from the tag's end to the current position (rounded up). */
static void close_tag(Packet *pk) {
    uint32_t *t = pk->pDmaTag, words;
    if (t == NULL) {
        return;
    }
    if ((t[0] >> 28 & 7) == 3 || (t[0] >> 28 & 7) == 0) { /* ref / refe: the data is elsewhere */
        return;
    }
    words = (uint32_t)(pk->pCurrent - (t + 4));
    t[0] = (t[0] & 0xFFFF0000u) | ((words + 3) / 4 & 0xFFFF);
}

static void pad_to(Packet *pk, uint32_t index) {
    while (qw_index(pk->pCurrent) != index) {
        *pk->pCurrent++ = 0; /* VIF NOP */
    }
}

static uint32_t *new_tag(Packet *pk, uint32_t id, uint32_t addr, uint32_t qwc, int irq) {
    uint32_t *t;
    close_tag(pk);
    pad_to(pk, 0);
    t = pk->pCurrent;
    t[0] = (uint32_t)irq << 31 | id << 28 | (qwc & 0xFFFF);
    t[1] = addr;
    t[2] = 0;
    t[3] = 0;
    pk->pDmaTag = t;
    return t;
}

/* ---- VIF1 packets ---- */
void sceVif1PkInit(Packet *pk, void *base) {
    pk->pBase = pk->pCurrent = base;
    pk->pDmaTag = NULL;
    pk->pVifCode = NULL;
    pk->pGifTag = NULL;
}
void sceVif1PkReset(Packet *pk) {
    pk->pCurrent = pk->pBase;
    pk->pDmaTag = NULL;
    pk->pVifCode = NULL;
    pk->pGifTag = NULL;
}
void sceVif1PkCnt(Packet *pk, uint32_t flag) {
    new_tag(pk, 1, 0, 0, flag & 1);
    pk->pCurrent = pk->pDmaTag + 2; /* the VIF codes in the tag's upper half */
}
void sceVif1PkRef(Packet *pk, void *addr, uint32_t size, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    uint32_t *t = new_tag(pk, 3, (uint32_t)(uintptr_t)addr, size, flag & 1);
    t[2] = opt1;
    t[3] = opt2;
    pk->pCurrent = t + 4;
}
void sceVif1PkEnd(Packet *pk, uint32_t flag) {
    new_tag(pk, 7, 0, 0, flag & 1);
    pk->pCurrent = pk->pDmaTag + 2;
}
uint32_t *sceVif1PkReserve(Packet *pk, uint32_t count) {
    uint32_t *p = pk->pCurrent;
    pk->pCurrent += count;
    return p;
}
void sceVif1PkAlign(Packet *pk, uint32_t bit, uint32_t pos) {
    uint32_t mask = (1u << bit) - 1;
    while ((((uint32_t)(uintptr_t)pk->pCurrent >> 2) & mask) != (pos & mask)) {
        *pk->pCurrent++ = 0;
    }
}
static void open_direct(Packet *pk, uint32_t cmd, int irq) {
    pad_to(pk, 3);
    pk->pVifCode = pk->pCurrent;
    *pk->pCurrent++ = (uint32_t)irq << 31 | cmd << 24;
}
static void close_direct(Packet *pk) {
    uint32_t words;
    if (pk->pVifCode == NULL) {
        return;
    }
    words = (uint32_t)(pk->pCurrent - (pk->pVifCode + 1));
    *pk->pVifCode = (*pk->pVifCode & 0xFFFF0000u) | ((words + 3) / 4 & 0xFFFF);
    pk->pVifCode = NULL;
}
void sceVif1PkOpenDirectCode(Packet *pk, int irq) { open_direct(pk, 0x50, irq & 1); }
void sceVif1PkCloseDirectCode(Packet *pk) { close_direct(pk); }
void sceVif1PkOpenDirectHLCode(Packet *pk, int irq) { open_direct(pk, 0x51, irq & 1); }
void sceVif1PkCloseDirectHLCode(Packet *pk) { close_direct(pk); }
/* The GIF tag is passed by value (u128: four words on the stack). */
void sceVif1PkOpenGifTag(Packet *pk, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3) {
    pk->pGifTag = pk->pCurrent;
    pk->pCurrent[0] = t0;
    pk->pCurrent[1] = t1;
    pk->pCurrent[2] = t2;
    pk->pCurrent[3] = t3;
    pk->pCurrent += 4;
}
void sceVif1PkCloseGifTag(Packet *pk) {
    uint32_t *g = pk->pGifTag, nreg, qws, flg, nloop;
    if (g == NULL) {
        return;
    }
    nreg = g[1] >> 28;
    flg = g[1] >> 26 & 3;
    if (nreg == 0) {
        nreg = 16;
    }
    qws = (uint32_t)(pk->pCurrent - (g + 4)) / 4;
    nloop = flg == 0 ? qws / nreg : qws; /* PACKED: a quadword per register; REGLIST / IMAGE: per quadword [I] */
    g[0] = (g[0] & ~0x7FFFu) | (nloop & 0x7FFF);
    pk->pGifTag = NULL;
}
void sceVif1PkAddGsAD(Packet *pk, uint32_t addr, uint32_t lo, uint32_t hi) {
    pk->pCurrent[0] = lo;
    pk->pCurrent[1] = hi;
    pk->pCurrent[2] = addr;
    pk->pCurrent[3] = 0;
    pk->pCurrent += 4;
}
void sceVif1PkTerminate(Packet *pk) {
    pad_to(pk, 0);
    close_tag(pk);
}
uint32_t sceVif1PkSize(Packet *pk) { return (uint32_t)(pk->pCurrent - pk->pBase + 3) / 4; }

/* ---- GIF packets ---- */
void sceGifPkInit(Packet *pk, void *base) {
    pk->pBase = pk->pCurrent = base;
    pk->pDmaTag = NULL;
    pk->pVifCode = NULL;
}
void sceGifPkReset(Packet *pk) {
    pk->pCurrent = pk->pBase;
    pk->pDmaTag = NULL;
    pk->pVifCode = NULL;
}
void sceGifPkCnt(Packet *pk, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    uint32_t *t = new_tag(pk, 1, 0, 0, flag & 1);
    t[2] = opt1;
    t[3] = opt2;
    pk->pCurrent = t + 4;
}
void sceGifPkRef(Packet *pk, void *addr, uint32_t size, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    sceVif1PkRef(pk, addr, size, opt1, opt2, flag);
}
void sceGifPkEnd(Packet *pk, uint32_t opt1, uint32_t opt2, uint32_t flag) {
    uint32_t *t = new_tag(pk, 7, 0, 0, flag & 1);
    t[2] = opt1;
    t[3] = opt2;
    pk->pCurrent = t + 4;
}
uint32_t *sceGifPkReserve(Packet *pk, uint32_t count) {
    uint32_t *p = pk->pCurrent;
    pk->pCurrent += count;
    return p;
}
void sceGifPkTerminate(Packet *pk) {
    pad_to(pk, 0);
    close_tag(pk);
}
uint32_t sceGifPkSize(Packet *pk) { return (uint32_t)(pk->pCurrent - pk->pBase + 3) / 4; }
