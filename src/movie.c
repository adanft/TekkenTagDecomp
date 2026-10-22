/* 0x39C7B8-0x3A04B8 (module movie, docs/game_overview.md section 4): MPEG movie playback built on Sony's
 * libmpeg sample player (a read thread fills a ring buffer from the IOP stream, the demultiplexer feeds the IPU
 * through a DMA ring (vibuf), decoded pictures go through a frame queue (vobuf) to the GS on VBlank), then a few
 * fixed-point helpers at the end. */
#include "common.h"

/* A quadword of a DMA / GIF packet. */
typedef union {
    u128 q;
    u64 d[2];
    u32 w[4];
} MovieQw;

/* The IOP stream ring buffer (D_005F6CA0 points at one). */
typedef struct {
    /* 0x00000 */ u8 data[0xF0000];
    /* 0xF0000 */ s32 put;
    /* 0xF0004 */ s32 count;
    /* 0xF0008 */ s32 size;
} MovieReadBuf;

/* A time stamp of the IPU ring (pts / dts and the stream position they belong to). */
typedef struct {
    /* 0x00 */ s64 pts;
    /* 0x08 */ s64 dts;
    /* 0x10 */ s32 pos;
    /* 0x14 */ s32 len;
} MovieTs;

/* The IPU input ring fed by DMA channel 4 (toIPU), 2 KB blocks. */
typedef struct {
    /* 0x00 */ u8 *data;
    /* 0x04 */ u128 *tag;
    /* 0x08 */ s32 n;
    /* 0x0C */ s32 dmaStart;
    /* 0x10 */ s32 dmaN;
    /* 0x14 */ s32 readBytes;
    /* 0x18 */ s32 buffSize;
    /* 0x1C */ u32 d4madr;
    /* 0x20 */ u32 d4tadr;
    /* 0x24 */ u32 d4qwc;
    /* 0x28 */ u32 d4chcr;
    /* 0x2C */ u32 d3madr;
    /* 0x30 */ u32 d3qwc;
    /* 0x34 */ u32 d3chcr;
    /* 0x38 */ u32 ipubp;
    /* 0x3C */ u32 ipuctrl;
    /* 0x40 */ s32 sema;
    /* 0x44 */ s32 isActive;
    /* 0x48 */ s64 totalBytes;
    /* 0x50 */ MovieTs *ts;
    /* 0x54 */ s32 nTs;
    /* 0x58 */ s32 countTs;
    /* 0x5C */ s32 wtTs;
} MovieViBuf;

/* sceMpeg (libmpeg). */
typedef struct {
    /* 0x00 */ s32 width;
    /* 0x04 */ s32 height;
    /* 0x08 */ s32 frameCount;
    /* 0x0C */ s32 pad0C;
    /* 0x10 */ s64 pts;
    /* 0x18 */ s64 dts;
    /* 0x20 */ u64 flags;
    /* 0x28 */ s64 pts2nd;
    /* 0x30 */ s64 dts2nd;
    /* 0x38 */ u64 flags2nd;
    /* 0x40 */ void *sys;
    /* 0x44 */ s32 pad44;
} sceMpeg;

/* D_004365D0: the video decoder. */
typedef struct {
    /* 0x00 */ sceMpeg mpeg;
    /* 0x48 */ MovieViBuf vibuf;
    /* 0xA8 */ s32 state;
    /* 0xAC */ s32 padAC[2];
    /* 0xB4 */ s32 handlerId;
} MovieVideoDec;

/* D_00436550: the queue of decoded frames. */
typedef struct {
    /* 0x00 */ u8 *data;   /* 0x118000 bytes per frame */
    /* 0x04 */ u8 *tag;    /* 0x85740 bytes per frame; the first word is the status */
    /* 0x08 */ s32 write;
    /* 0x0C */ volatile s32 count;
    /* 0x10 */ s32 size;
    /* 0x14 */ s32 pad14;
} MovieVoBuf;

/* D_00436568: the playback state. */
typedef struct {
    /* 0x00 */ s8 unk0;
    /* 0x01 */ s8 unk1;
    /* 0x02 */ s8 unk2;
    /* 0x04 */ s32 sector;
    /* 0x08 */ s32 size;
    /* 0x0C */ s32 remain;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ volatile s32 vcount; /* VBlanks */
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
} MovieState;

/* D_00518EA8: per movie, its size and whether it ends on a white screen. */
typedef struct {
    /* 0x0 */ s32 size;
    /* 0x4 */ s8 unk4;
} MovieInfo;

/* sceGsDispfb / sceGsDisplay as libgraph declares them. */
typedef struct {
    u32 FBP : 9;
    u32 FBW : 6;
    u32 PSM : 5;
    u32 p0 : 12;
    u32 DBX : 11;
    u32 DBY : 11;
    u32 p1 : 10;
} MovieGsDispfb;

typedef struct {
    u32 DX : 12;
    u32 DY : 11;
    u32 MAGH : 4;
    u32 MAGV : 2;
    u32 p0 : 3;
    u32 DW : 12;
    u32 DH : 11;
    u32 p1 : 9;
} MovieGsDisplay;

typedef struct {
    /* 0x00 */ u32 pmode[2];
    /* 0x08 */ u32 smode2[2];
    /* 0x10 */ MovieGsDispfb dispfb;
    /* 0x18 */ MovieGsDisplay display;
    /* 0x20 */ u32 bgcolor[2];
} MovieGsDispEnv;

/* D_00436598: the movie's display: the frame buffer pages of the two fields and the picture size. */
typedef struct {
    /* 0x00 */ MovieGsDispEnv env;
    /* 0x28 */ s32 fbp[2];
    /* 0x30 */ s32 w;
    /* 0x34 */ s32 h;
} MovieDisp;

typedef struct {
    s32 x;
    s32 y;
    s32 w;
    s32 h;
} MovieRect;

/* sceMpegCbDataStr. */
typedef struct {
    /* 0x00 */ s32 type;
    /* 0x04 */ void *header;
    /* 0x08 */ u8 *data;
    /* 0x0C */ u32 len;
    /* 0x10 */ s64 pts;
    /* 0x18 */ s64 dts;
} MovieCbStr;

/* sceMpegCbDataTimeStamp. */
typedef struct {
    /* 0x00 */ s32 type;
    /* 0x04 */ s32 pad4;
    /* 0x08 */ s64 pts;
    /* 0x10 */ s64 dts;
} MovieCbTs;

/* The four bytes of the MPEG sequence end code. */
typedef struct {
    u8 b[4];
} MovieSeqEnd;

/* Movie types end. */

/* The SDK's ThreadParam. */
typedef struct {
    s32 status;
    void (*entry)(void *);
    void *stack;
    s32 stackSize;
    void *gpReg;
    s32 initPriority;
    s32 currentPriority;
    u32 attr;
    u32 option;
    s32 waitType;
    s32 waitId;
    s32 wakeupCount;
} MovieThreadParam;

/* The SDK's SemaParam. */
typedef struct {
    s32 currentCount;
    s32 maxCount;
    s32 initCount;
    s32 numWaitThreads;
    u32 attr;
    u32 option;
} MovieSemaParam;

extern MovieVoBuf D_00436550;
extern MovieState D_00436568;
extern MovieDisp D_00436598;
extern MovieVideoDec D_004365D0;
extern MovieViBuf D_00436618;
extern const MovieInfo D_00518EA8[];
/* Stores that the original keeps out of delay slots. */
#define VOL16(x) (*(volatile s16 *)&(x))

extern s16 D_005F6CC0;
extern s16 D_005F6CC2;
extern s16 D_005F6CC4;
extern s16 D_005F6CC6;
extern volatile s32 D_005F61F8;
extern volatile s32 D_005F61FC;
extern s32 D_005F6200;
extern s32 D_005F6204;
extern s32 D_005F6208;
extern s32 D_005F6504;
extern u32 D_005F6C88;
extern s32 D_005F6C8C;
extern s32 D_005F6C90;
extern s32 D_005F6C94;
extern s32 D_005F6C98;
extern s32 D_005F6C9C;
extern MovieReadBuf *D_005F6CA0;
extern s32 D_005F6CA4;
extern s32 D_005F6CA8;
extern s32 D_005F6CAC;
extern s32 D_005F6CB0;
extern s32 D_005F6CB4;
extern s32 D_005F6CB8;
extern u8 D_005FD0F0[];
extern u8 D_005F6210[];
extern s16 gSinTable[];
extern u128 D_008BF040[];
extern u128 D_008BF180[];

s32 sceSdRemote(s32 arg, s32 command, ...);
void FlushCache(s32 op);
s32 RotateThreadReadyQueue(s32 prio);
s32 CreateThread(MovieThreadParam *param);
s32 StartThread(s32 id, void *arg);
s32 TerminateThread(s32 id);
s32 DeleteThread(s32 id);
s32 CreateSema(MovieSemaParam *param);
s32 DeleteSema(s32 id);
s32 SignalSema(s32 id);
s32 WaitSema(s32 id);
s32 AddIntcHandler(s32 cause, s32 (*handler)(s32), s32 next);
s32 RemoveIntcHandler(s32 cause, s32 id);
s32 EnableIntc(s32 cause);
s32 DisableIntc(s32 cause);
void *memcpy(void *dst, const void *src, u32 n);
s32 sceGsSyncV(s32 mode);
s32 sceGsSyncPath(s32 mode, u16 timeout);
s32 sceMpegInit(void);
s32 sceMpegCreate(sceMpeg *mp, u8 *work, s32 workSize);
s32 sceMpegAddCallback(sceMpeg *mp, s32 type, s32 (*cb)(), void *anyData);
s32 sceMpegAddStrCallback(sceMpeg *mp, s32 strType, s32 ch, s32 (*cb)(), void *data);
s32 func_003F9240(MovieVideoDec *vd);

s32 sceMpegDelete(sceMpeg *mp);
s32 sceMpegGetPicture(sceMpeg *mp, void *rgb32, s32 mbcount);
s32 sceMpegDemuxPssRing(sceMpeg *mp, u8 *buff, s32 size, u8 *buffTop, s32 buffSize);
void Bgm_Stop(void);
void Snd_Flush(void);
s32 Snd_PlaySe(s32 id, s32 player);
u32 File_GetMovieSector(s32 n);
s32 Movie_PlayAudio(s32 movie);
void Pad_Update(void);
s32 Input_SkipPressed(void);
void func_003B89F8(void);
void Gfx_RemoveVSyncHandler(void);
void Gfx_InstallVSyncHandler(s32 mode);
void Gfx_GetDisplayOffset(s32 *dx, s32 *dy);
void Gfx_SetDispEnv(MovieGsDispEnv *disp, s32 psm, s32 w, s32 h, s32 dx, s32 dy, s32 second);
void sceGsPutDispEnv(MovieGsDispEnv *disp);
void func_0035D588(void);
void func_0035D608(void);
void func_003992D8(void);
void func_003F91F8(void);
s32 func_003F9230(MovieVideoDec *vd);
void func_003F9250(MovieVideoDec *vd);
s32 func_00366108(s32 dx, s32 dz);
s32 func_0039BDF0(s32 *v, s32 *out);
void func_0039BEC8(s32 *a, s32 *b, s32 *out);
void func_0039D580(MovieDisp *d, u8 *tag, u8 *data, s32 field, s32 x, s32 y, s32 w, s32 h2, s32 w2, s32 h);

MovieQw *func_0039DC48(MovieQw *p, u32 spr, u32 addr, u32 irq, u32 id, u32 pce, u32 qwc);
MovieQw *func_0039DCA0(MovieQw *p, s64 regs, u32 nreg, u32 flg, u32 prim, u32 pre, u32 eop, u32 nloop);
MovieQw *func_0039DCF8(MovieQw *p, u32 addr, s64 data);
MovieQw *func_0039DD28(MovieQw *p);
MovieQw *func_0039DD48(MovieQw *p, u32 lcm, u32 mxl, u32 mmag, u32 mmin, u32 mtba, u32 l, u32 k);
MovieQw *func_0039DDB0(MovieQw *p, u32 tbp, u32 tbw, u32 psm, u32 tw, u32 th, u32 tcc, u32 tfx, u32 cbp, u32 cpsm, u32 csm, u32 csa, u32 cld);
MovieQw *func_0039DE50(MovieQw *p, u32 prim, u32 iip, u32 tme, u32 fge, u32 abe, u32 aa1, u32 fst, u32 ctxt, u32 fix);
MovieQw *func_0039DED8(MovieQw *p, u32 u, u32 v);
MovieQw *func_0039DF08(MovieQw *p, u32 r, u32 g, u32 b, u32 a, u32 q);
MovieQw *func_0039DF58(MovieQw *p, u32 x, u32 y, u32 z);
MovieQw *func_0039DF90(MovieQw *p, u32 fbp, u32 fbw, u32 psm, u32 fbmsk);
MovieQw *func_0039DFD8(MovieQw *p, u32 ate, u32 atst, u32 aref, u32 afail, u32 date, u32 datm, u32 zte, u32 ztst);
MovieQw *func_0039E050(MovieQw *p, u32 x0, u32 x1, u32 y0, u32 y1);
MovieQw *func_0039E090(MovieQw *p, u32 ofx, u32 ofy);
MovieQw *func_0039E0C0(MovieQw *p, u32 ac);
MovieQw *func_0039E0E8(MovieQw *p, u32 wms, u32 wmt, u32 minu, u32 maxu, u32 minv, u32 maxv);
MovieQw *func_0039E148(MovieQw *p, u32 dbp, u32 dbw, u32 dpsm);
MovieQw *func_0039E178(MovieQw *p, u32 dir, u32 dsax, u32 dsay);
MovieQw *func_0039E1A8(MovieQw *p, u32 rrw, u32 rrh);
MovieQw *func_0039E1D8(MovieQw *p, u32 xdir);
void func_0039E200(u32 addr);
void func_0039D320(MovieDisp *d, s32 w, s32 h);
void func_0039D9B8(MovieDisp *d, s32 field);
void func_0039D9F0(MovieDisp *d, u32 color);
MovieQw *func_0039DAD8(MovieQw *p, MovieRect *xy, MovieRect *uv);
MovieQw *func_0039DB98(MovieQw *p, MovieRect *xy, u32 color);
u32 func_0039EFF0(MovieViBuf *f, u32 d4madr);
void func_0039F030(u32 chcr);
void func_0039F0A0(u32 chcr);
void func_0039F110(u64 *tag, u32 addr, u32 id, u32 qwc);
s32 func_0039F138(MovieViBuf *f, u8 *data, u128 *tag, s32 n, MovieTs *ts, s32 nTs);
void func_0039F408(MovieViBuf *f, s32 size);
s32 func_0039FA58(MovieViBuf *f);
s32 func_0039FAB0(MovieViBuf *f);
s32 func_0039FAF0(MovieViBuf *f);
void func_0039FB40(MovieViBuf *f);
s32 func_0039F1B0(MovieViBuf *f);
void func_0039F318(MovieViBuf *f, u8 **ptr1, s32 *size1, u8 **ptr2, s32 *size2);
s32 func_0039F638(MovieViBuf *f);
s32 func_0039F458(MovieViBuf *f);
s32 func_0039F738(MovieViBuf *f);
s32 func_0039FB90(MovieViBuf *f, MovieTs *nts);
s32 func_0039FCE8(MovieViBuf *f, MovieTs *ts);
s32 func_0039FDF8(MovieViBuf *f, MovieTs *ts);
s32 func_0039E4B8(sceMpeg *mp, MovieCbStr *str, MovieReadBuf *rb);
s32 func_0039E5C0(u8 *pd0, s32 d0, u8 *pd1, s32 d1, u8 *ps0, s32 s0, u8 *ps1, s32 s1);
s32 func_0039E808(MovieVideoDec *vd, u8 *work, s32 workSize, u8 *data, u128 *tag, s32 tagSize, MovieTs *ts, s32 nTs);
s32 func_0039E928(MovieVideoDec *vd, s32 type, s32 ch, s32 (*cb)(sceMpeg *, MovieCbStr *, MovieReadBuf *), MovieReadBuf *data);
void func_0039E948(MovieVideoDec *vd, u8 **pd0, s32 *d0, u8 **pd1, s32 *d1);
void func_0039E950(MovieVideoDec *vd, s32 size);
void func_0039E9C8(MovieVideoDec *vd, s64 pts, s64 dts, u8 *start, s32 len);
s32 func_0039EA28(MovieVideoDec *vd);
s32 func_0039EA60(MovieVideoDec *vd);
s32 func_0039EB30(MovieVideoDec *vd);
void func_0039EB78(MovieVideoDec *vd);
s32 func_0039EE70(sceMpeg *mp, MovieCbTs *cb, void *data);
s32 func_0039EEB8(u8 *pd0, s32 d0, u8 *pd1, s32 d1, u8 *ps0, s32 s0, u8 *ps1, s32 s1);
s32 func_0039C878(void);
s32 func_0039CAC8(MovieVideoDec *vd, MovieReadBuf *rb);
s32 Movie_Open(s32 movie);
void Movie_Close(void);
s32 Movie_ReadChunk(u8 *dst, s32 size);
void func_0039E920(void);
s32 func_0039E268(s32 cause);
void func_003A0020(MovieVoBuf *f);
s32 func_003A0180(s32 x, s32 z);
void func_003A01A8(s32 x, s32 y, s32 z, s16 *out);
s32 func_003A0218(s32 x, s32 z, s32 ang);
void func_003A02C0(s32 unused, u8 *f, u16 *base, s16 *delta, s16 *out);
s16 func_003A0340(u8 *f, s16 v);
void func_003A0388(s16 *in, s16 *out);
/* Movie prototypes end. */
void func_0039C7B8(void);
void func_0039C7D8(u32 movie);
s32 Movie_Update(void);
s32 func_0039C868(void);
s32 func_0039C878(void);
s32 func_0039CAC8(MovieVideoDec *vd, MovieReadBuf *rb);
void func_0039CDB0(void);
s32 func_0039CDB8(void);
s32 Movie_Open(s32 movie);
void Movie_Close(void);
void func_0039D040(void *arg);
s32 Movie_StartStream(s32 movie);
void Movie_WaitRead(void);
s32 Snd_WaitStreamIdle(void);
void func_0039E6F8(MovieReadBuf *b);
void func_0039E710(MovieReadBuf *b);
s32 func_0039E718(MovieReadBuf *b, u8 **ptr);
s32 func_0039E748(MovieReadBuf *b, s32 size);
s32 func_0039E790(MovieReadBuf *b, u8 **ptr);
s32 func_0039E7E0(MovieReadBuf *b, s32 size);
void func_0039E470(s32 field);
void func_0039E4A8(void);
s32 func_0039E958(MovieVideoDec *vd);
s32 func_0039E968(MovieVideoDec *vd);
s32 func_0039E9A0(MovieVideoDec *vd);
s32 func_0039E9B0(MovieVideoDec *vd);
s32 func_0039E9B8(MovieVideoDec *vd, s32 state);
s32 func_0039EA08(MovieVideoDec *vd);
s32 func_0039EDD0(void);
s32 func_0039EDE8(void);
s32 func_0039EE18(void);
s32 func_0039EE40(void);
s32 func_0039EE68(void);
s32 func_0039F458(MovieViBuf *f);
s32 func_0039F638(MovieViBuf *f);
s32 func_0039F738(MovieViBuf *f);
s32 func_0039FA58(MovieViBuf *f);
s32 func_0039FAF0(MovieViBuf *f);
void func_0039FFA0(MovieVoBuf *f, u8 *data, u8 *tag, s32 size);
void func_0039FFF0(MovieVoBuf *f);
void func_0039FFF8(MovieVoBuf *f);
s32 func_003A0008(MovieVoBuf *f);
void func_003A0020(MovieVoBuf *f);
u8 *func_003A00A0(MovieVoBuf *f);
s32 func_003A00E8(MovieVoBuf *f);
u8 *func_003A00F8(MovieVoBuf *f);
void func_003A0160(MovieVoBuf *f);


/* Functions in address order. */

void func_0039C7B8(void) {
    D_005F6CC0 = 1;
    D_005F6CC2 = -1;
    VOL16(D_005F6CC4) = 0;
}

/* NON_MATCHING: 8 lines: the original if-converts the clamp as move zero / movn after sltiu; every form tried gives movz or the swapped order */
#if 0
void func_0039C7D8(u32 movie) {
    s32 n = movie;

    if (movie >= 3) {
        n = 0;
    }
    Bgm_Stop();
    Snd_Flush();
    D_005F6CC0 = 2;
    D_00436568.unk1C = 0;
    D_00436568.unk20 = 0;
    D_00436568.unk24 = 0;
    D_005F6CC2 = n;
    D_00436568.unk28 = 0;
    D_005F6CC4 = 0;
    D_005F6CC6 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039C7D8);
#endif

/* NON_MATCHING: 2 lines: the original places ld ra after move v0, zero */
#if 0
s32 Movie_Update(void) {
    if (D_005F6CC0 != 1) {
        func_0039C878();
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", Movie_Update);
#endif

/* NON_MATCHING: 2 lines: the original keeps the lh out of the jr delay slot (a volatile s16 read becomes lhu + sll / sra instead) */
#if 0
s32 func_0039C868(void) {
    return D_005F6CC4;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039C868);
#endif

/* NON_MATCHING: 46 lines: register allocation and scheduling of the buffer address stores and the state clears */
#if 0
s32 func_0039C878(void) {
    s32 base;
    s32 white;

    func_003B89F8();
    base = D_005F6504;
    D_005F6C90 = base + 0x578000;
    D_005F6C94 = base + 0x813440;
    D_005F6C98 = base + 0x814480;
    D_005F6C9C = base + 0x94FC80;
    D_005F6CA0 = (MovieReadBuf *)(base + 0x950480);
    D_005F6CA8 = base + 0xA404C0;
    D_005F6CAC = base + 0xAC04C0;
    D_005F6CB0 = base + 0xAC44C0;
    D_005F6C8C = base;
    D_005F6CA4 = base + 0xA404C0;
    Gfx_RemoveVSyncHandler();
    func_0035D588();
    D_00436568.vcount = 0;
    D_00436568.unk1C = 0;
    D_00436568.unk20 = 0;
    D_00436568.unk24 = 0;
    D_00436568.unk1 = 1;
    D_00436568.unk28 = 0;
    D_00436568.unk0 = 0;
    D_00436568.unk2 = 0;
    D_00436568.unk14 = 0;
    D_00436568.unk2C = 1;
    D_00436568.unk10 = 0;
    func_0039D320(&D_00436598, 0x280, 0xE0);
    func_0039D9F0(&D_00436598, 0);
    if (Snd_WaitStreamIdle() == 0) {
        Movie_Open(D_005F6CC2);
        func_0039CAC8(&D_004365D0, D_005F6CA0);
        Movie_Close();
    } else {
        D_005F6CC4 = 1;
    }
    sceGsSyncPath(0, 0);
    if (D_005F6CC6 != 0) {
        D_005F6CC4 = 1;
    }
    white = 0;
    if (D_005F6CC4 == 0) {
        white = 1;
        if (D_00518EA8[D_005F6CC2].unk4 == 0) {
            white = 0;
        }
    }
    func_0039D9F0(&D_00436598, white != 0 ? 0xFFFFFF : 0);
    func_0039D578(&D_00436598);
    FlushCache(0);
    func_0035D608();
    Gfx_InstallVSyncHandler(white);
    if (D_005F6CC4 != 0) {
        Snd_PlaySe(4, 0);
    }
    while (sceSdRemote(1, 0x205A, 1) == 0) {
        sceGsSyncV(0);
    }
    Snd_WaitStreamIdle();
    func_003992D8();
    D_00436568.unk2C = 1;
    D_005F6CC0 = 1;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039C878);
#endif

/* NON_MATCHING: 108 lines: loop layout and register allocation of the read / demultiplex loop (the original tests the exit conditions at the loop bottom) */
#if 0
s32 func_0039CAC8(MovieVideoDec *vd, MovieReadBuf *rb) {
    s32 stop = 0;
    s32 remain = D_00436568.remain;
    s32 toRead = remain;
    s32 done = remain < 5;
    u8 *put;
    u8 *get;
    volatile s32 audio = 0;
    s32 n;
    u32 end;

    D_005F6C88 = 0;
    while (!done && func_0039E9B0(vd) != 3 && D_00436568.unk28 == 0 && D_005F6CC6 == 0) {
        if (D_005F6C88 != D_00436568.vcount) {
            Pad_Update();
            if ((vd->mpeg.frameCount >= 11 && Input_SkipPressed() != 0) || D_005F6CC6 != 0) {
                stop = 1;
                D_005F6CC4 = 1;
            }
        }
        if (D_00436568.unk1C >= 60 || D_00436568.unk24 >= 181 || D_00436568.unk28 != 0) {
            stop = 1;
        }
        if (stop && vd->mpeg.frameCount >= 11) {
            func_0039E9A0(&D_004365D0);
        }
        n = func_0039E718(rb, &put);
        if (toRead > 0 && n > 0xFFFF && D_005F6C88 != D_00436568.vcount) {
            n = Movie_ReadChunk(put, 0x10000);
            D_005F6C88 = D_00436568.vcount;
            if (n > 0) {
                func_0039E748(rb, n);
                toRead -= n;
                D_00436568.unk14 += n;
            }
        }
        func_0039CDB0();
        n = func_0039E790(rb, &get);
        if (n > 0) {
            n = sceMpegDemuxPssRing(&vd->mpeg, get, n, rb->data, rb->size);
            remain -= n;
            done = remain < 5;
            func_0039E7E0(rb, n);
        }
        if (!audio && func_003A0008(&D_00436550) != 0 && func_0039CDB8() != 0) {
            audio = 1;
            func_0039E470(1);
            Movie_PlayAudio(D_005F6CC2);
            Snd_Flush();
        }
    }
    sceSdRemote(1, 0x205A, 0);
    while (func_0039EA60(vd) == 0) {
        func_0039CDB0();
    }
    end = D_00436568.vcount + 600;
    while (func_0039EB30(vd) == 0 && func_0039E9B0(vd) != 3) {
        func_0039CDB0();
        if (end < (u32)D_00436568.vcount) {
            break;
        }
    }
    func_0039E4A8();
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039CAC8);
#endif

void func_0039CDB0(void) {
    RotateThreadReadyQueue(0);
}

s32 func_0039CDB8(void) {
    return 1;
}

/* NON_MATCHING: 17 lines: s0 / s1 swapped between &D_004365D0 and the gp value, two alignment nops */
#if 0
s32 Movie_Open(s32 movie) {
    MovieThreadParam th;
    u32 status;

    *(volatile u32 *)0x1000E000 |= 3;
    *(volatile u32 *)0x1000E010 = 4;
    FlushCache(0);
    func_0039E6F8(D_005F6CA0);
    sceMpegInit();
    func_0039E808(&D_004365D0, (u8 *)D_005F6C98, 0x13B800, (u8 *)D_005F6CA8, (u128 *)D_005F6C94, 0x100, (MovieTs *)D_005F6CB0, 0x200);
    func_0039E928(&D_004365D0, 0, 0, func_0039E4B8, D_005F6CA0);
    func_0039FFA0(&D_00436550, (u8 *)(((u32)D_005F6C8C & 0x0FFFFFFF) | 0x20000000), (u8 *)D_005F6C90, 5);
    th.entry = func_0039D040;
    th.stack = (void *)D_005F6C9C;
    th.stackSize = 0x800;
    th.initPriority = 0;
    th.gpReg = D_005FD0F0;
    th.option = 0;
    D_005F6CB8 = CreateThread(&th);
    StartThread(D_005F6CB8, NULL);
    th.entry = (void (*)(void *))func_0039EB78;
    th.stack = (void *)D_005F6CAC;
    th.stackSize = 0x4000;
    th.gpReg = D_005FD0F0;
    th.initPriority = 0;
    th.option = 0;
    D_005F6CB4 = CreateThread(&th);
    StartThread(D_005F6CB4, &D_004365D0);
    Movie_StartStream(movie);
    do {
        __asm__ volatile("di");
        __asm__ volatile("sync.p");
        __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    } while (status & 0x10000);
    D_004365D0.handlerId = AddIntcHandler(2, func_0039E268, 0);
    EnableIntc(2);
    __asm__ volatile("ei");
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", Movie_Open);
#endif

void Movie_Close(void) {
    u32 status;

    func_0039E710(D_005F6CA0);
    func_0039FFF0(&D_00436550);
    TerminateThread(D_005F6CB4);
    DeleteThread(D_005F6CB4);
    TerminateThread(D_005F6CB8);
    DeleteThread(D_005F6CB8);
    do {
        __asm__ volatile("di");
        __asm__ volatile("sync.p");
        __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    } while (status & 0x10000);
    DisableIntc(2);
    RemoveIntcHandler(2, D_004365D0.handlerId);
    __asm__ volatile("ei");
    func_0039E968(&D_004365D0);
    *(volatile u32 *)0x10002010 = 0x40000000;
}

void func_0039D040(void *arg) {
    for (;;) {
        func_0039CDB0();
    }
}

s32 Movie_StartStream(s32 movie) {
    u32 sector;

    D_00436568.size = D_00518EA8[movie].size;
    sector = File_GetMovieSector(movie);
    D_00436568.sector = sector;
    D_00436568.remain = D_00436568.size;
    sceSdRemote(1, 0x2058, sector, movie, 7, 0x80);
    return 0;
}

/* NON_MATCHING: 7 lines: the original's early exit on bit 4 jumps into the epilogue (ld ra / ld s2 in the delay slots), ours to the loop end */
#if 0
void Movie_WaitRead(void) {
    s32 v = D_00436568.vcount;
    s32 r;

    for (;;) {
        r = sceSdRemote(1, 0x2030);
        if (r & 4) {
            D_005F6CC6 = 1;
            return;
        }
        while (v == D_00436568.vcount) {
        }
        if (!(r & 1)) {
            return;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", Movie_WaitRead);
#endif

s32 Snd_WaitStreamIdle(void) {
    s32 n = 900;
    s32 r = sceSdRemote(1, 0x2030) & 0x40;

    while (r == 0) {
        n--;
        sceGsSyncV(0);
        Bgm_Stop();
        Snd_Flush();
        r = sceSdRemote(1, 0x2030) & 0x40;
        if (n <= 0) {
            break;
        }
    }
    r = -1;
    if (n >= 1) {
        r = 0;
    }
    return r;
}

/* NON_MATCHING: 34 lines: loop layout of the IOP read wait (tries counter and the bit 4 / bit 0x20 tests) */
#if 0
s32 Movie_ReadChunk(u8 *dst, s32 size) {
    u32 tries = 0;
    s32 r;
    s32 n;
    s32 v;

    Movie_WaitRead();
    if (D_005F6CC6 != 0) {
        return 0;
    }
    sceSdRemote(1, 0x2059, dst, 0x20);
    r = sceSdRemote(1, 0x2030);
    while (!(r & 0x20)) {
        v = D_00436568.vcount;
        while (D_00436568.vcount == v) {
        }
        if (++tries >= 480) {
            D_005F6CC6 = 1;
            break;
        }
        r = sceSdRemote(1, 0x2030);
        if (r & 4) {
            D_005F6CC6 = 1;
            break;
        }
    }
    n = 0;
    if (D_005F6CC6 == 0) {
        FlushCache(0);
        n = D_00436568.remain;
        if (n > 0x10000) {
            n = 0x10000;
        }
        D_00436568.sector += 0x20;
        D_00436568.remain -= n;
    }
    return n;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", Movie_ReadChunk);
#endif

/* NON_MATCHING: 12 lines: scheduling of the prologue stores and the D_005F6208 store around the fbp mult */
#if 0
void func_0039D320(MovieDisp *d, s32 w, s32 h) {
    s32 dx;
    s32 dy;
    u32 p;
    MovieQw *q;

    D_005F61F8 = 0;
    D_005F61FC = 0;
    D_005F6208 = 0;
    D_005F6200 = 0;
    D_005F6204 = 0;
    d->fbp[1] = (w + 63) / 64 * (((h + 31) / 32) << 11) >> 11;
    d->w = w;
    d->h = h;
    d->fbp[0] = 0;
    Gfx_SetDispEnv(&d->env, 1, 0x280, 0xE0, 0, 0, 0);
    Gfx_GetDisplayOffset(&dx, &dy);
    d->env.display.DX = dx;
    d->env.display.DY = dy;
    d->env.dispfb.FBW = 10;
    sceGsPutDispEnv(&d->env);
    sceGsSyncPath(0, 0);
    p = (u32)D_008BF040 & 0x0FFFFFFF;
    q = func_0039DCA0((MovieQw *)(p | 0x20000000), 0xE, 1, 0, 0, 0, 1, 6);
    q = func_0039E0C0(q, 1);
    q = func_0039DF90(q, 0, (d->w + 63) / 64, 1, 0);
    q = func_0039DFD8(q, 0, 0, 0, 0, 0, 0, 0, 0);
    q = func_0039E050(q, 0, d->w - 1, 0, ((d->h + 31) / 32) << 6);
    q = func_0039E090(q, 0, 0);
    func_0039E0E8(q, 1, 1, 0, 0, 0, 0);
    *(volatile u32 *)0x1000A010 = p;
    *(volatile u32 *)0x1000A020 = 7;
    *(volatile u32 *)0x1000A000 = 0x101;
    sceGsSyncPath(0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039D320);
#endif

void func_0039D578(void *disp) {
}

/* NON_MATCHING: 95 lines: packet builder draft; argument evaluation order and registers of the GIF / DMA tag chain */
#if 0
void func_0039D580(MovieDisp *d, u8 *tag, u8 *data, s32 field, s32 x, s32 y, s32 w, s32 h2, s32 w2, s32 h) {
    s32 mbx = h >> 4;
    s32 mby = w2 >> 4;
    MovieQw *p = (MovieQw *)(((u32)tag & 0x0FFFFFFF) | 0x20000000);
    u32 src = (u32)data;
    MovieRect xy;
    MovieRect uv;
    MovieRect *puv;
    s32 i;
    s32 j;

    if (field == 0) {
        p = func_0039DC48(p, 0, 0, 0, 1, 0, 3);
        p = func_0039DCA0(p, 0xE, 1, 0, 0, 0, 0, 2);
        p = func_0039E148(p, 0x2300, (d->w + 63) / 64, 0);
        p = func_0039E1A8(p, 0x10, 0x10);
        for (i = 0; i < mby; i++) {
            for (j = 0; j < mbx; j++) {
                p = func_0039DC48(p, 0, 0, 0, 1, 0, 4);
                p = func_0039DCA0(p, 0xE, 1, 0, 0, 0, 0, 2);
                p = func_0039E178(p, 0, i * 16, j * 16);
                p = func_0039E1D8(p, 0);
                p = func_0039DCA0(p, 0, 0, 2, 0, 0, 0, 0x40);
                p = func_0039DC48(p, 0, src & 0x0FFFFFFF, 0, 3, 0, 0x40);
                src += 0x400;
            }
        }
        xy.x = x * 16;
        xy.y = y * 16;
        xy.w = w * 16;
        xy.h = h2 * 16;
        uv.x = 8;
        uv.y = 8;
        uv.w = w2 * 16;
        uv.h = h * 16;
        puv = &uv;
    } else {
        xy.x = x * 16;
        xy.w = w * 16;
        xy.h = h2 * 16;
        xy.y = (y + ((d->h + 31) / 32 << 5)) * 16;
        uv.x = 8;
        puv = &uv;
        uv.y = 0x18;
        uv.w = w2 * 16;
        uv.h = h * 16;
    }
    p = func_0039DC48(p, 0, 0, 0, 7, 0, 0xB);
    p = func_0039DCA0(p, 0xE, 1, 0, 0, 0, 1, 0xA);
    p = func_0039DD28(p);
    p = func_0039DD48(p, 0, 0, 0, 0, 0, 0, 0);
    p = func_0039DDB0(p, 0x2300, (d->w + 63) / 64, 0, 10, 10, 0, 1, 0, 0, 0, 0, 0);
    p = func_0039DD28(p);
    p = func_0039DD48(p, 0, 0, 1, 1, 0, 0, 0);
    func_0039DAD8(p, &xy, puv);
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039D580);
#endif

void func_0039D9B8(MovieDisp *d, s32 field) {
    s32 fbp;

    if (field != 0) {
        fbp = d->fbp[1];
    } else {
        fbp = d->fbp[0];
    }
    d->env.dispfb.FBP = fbp;
    sceGsPutDispEnv(&d->env);
}

void func_0039D9F0(MovieDisp *d, u32 color) {
    MovieRect r;
    u32 p;

    p = (u32)D_008BF180 & 0x0FFFFFFF;
    r.x = 0;
    r.y = 0;
    r.w = d->w << 4;
    r.h = ((d->h + 31) / 32) << 10;
    func_0039DB98(func_0039DCA0((MovieQw *)(p | 0x20000000), 0xE, 1, 0, 0, 0, 1, 4), &r, color);
    *(volatile u32 *)0x1000A010 = p;
    *(volatile u32 *)0x1000A020 = 5;
    *(volatile u32 *)0x1000A000 = 0x101;
    sceGsSyncPath(0, 0);
}

MovieQw *func_0039DAD8(MovieQw *p, MovieRect *xy, MovieRect *uv) {
    p = func_0039DE50(p, 6, 0, 1, 0, 0, 0, 1, 0, 0);
    p = func_0039DED8(p, uv->x, uv->y);
    p = func_0039DF58(p, xy->x, xy->y, 0);
    p = func_0039DED8(p, uv->x + uv->w, uv->y + uv->h);
    return func_0039DF58(p, xy->x + xy->w, xy->y + xy->h, 0);
}

MovieQw *func_0039DB98(MovieQw *p, MovieRect *xy, u32 color) {
    p = func_0039DE50(p, 6, 0, 0, 0, 0, 0, 1, 0, 0);
    p = func_0039DF58(p, xy->x, xy->y, 0);
    p = func_0039DF08(p, color & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF, color >> 24, 0);
    return func_0039DF58(p, xy->x + xy->w, xy->y + xy->h, 0);
}

MovieQw *func_0039DC48(MovieQw *p, u32 spr, u32 addr, u32 irq, u32 id, u32 pce, u32 qwc) {
    p->d[0] = (u64)spr << 63 | (u64)(addr & 0xFFFFFFF0) << 32 | (u64)irq << 31 | (u64)id << 28 | (u64)pce << 26 | (u64)qwc;
    return p + 1;
}

MovieQw *func_0039DCA0(MovieQw *p, s64 regs, u32 nreg, u32 flg, u32 prim, u32 pre, u32 eop, u32 nloop) {
    p->w[0] = eop << 15 | nloop;
    p->w[1] = pre << 14 | prim << 15 | flg << 26 | nreg << 28;
    p->w[2] = regs & 0xFFFFFFFF;
    p->w[3] = regs >> 32;
    return p + 1;
}

MovieQw *func_0039DCF8(MovieQw *p, u32 addr, s64 data) {
    p->w[3] = 0;
    p->w[0] = data & 0xFFFFFFFF;
    p->w[2] = addr;
    p->w[1] = data >> 32;
    return p + 1;
}

MovieQw *func_0039DD28(MovieQw *p) {
    return func_0039DCF8(p, 0x3F, 0);
}

MovieQw *func_0039DD48(MovieQw *p, u32 lcm, u32 mxl, u32 mmag, u32 mmin, u32 mtba, u32 l, u32 k) {
    return func_0039DCF8(p, 0x14, (u64)lcm | (u64)mxl << 2 | (u64)mmag << 5 | (u64)mmin << 6 | (u64)mtba << 9 | (u64)l << 19 | (u64)k << 32);
}

MovieQw *func_0039DDB0(MovieQw *p, u32 tbp, u32 tbw, u32 psm, u32 tw, u32 th, u32 tcc, u32 tfx, u32 cbp, u32 cpsm, u32 csm, u32 csa, u32 cld) {
    return func_0039DCF8(p, 6, (u64)tbp | (u64)tbw << 14 | (u64)psm << 20 | (u64)tw << 26 | (u64)th << 30 | (u64)tcc << 34 | (u64)tfx << 35 | (u64)cbp << 37 | (u64)cpsm << 51 | (u64)csm << 55 | (u64)csa << 56 | (u64)cld << 61);
}

MovieQw *func_0039DE50(MovieQw *p, u32 prim, u32 iip, u32 tme, u32 fge, u32 abe, u32 aa1, u32 fst, u32 ctxt, u32 fix) {
    return func_0039DCF8(p, 0, (u64)prim | (u64)iip << 3 | (u64)tme << 4 | (u64)fge << 5 | (u64)abe << 6 | (u64)aa1 << 7 | (u64)fst << 8 | (u64)ctxt << 9 | (u64)fix << 10);
}

MovieQw *func_0039DED8(MovieQw *p, u32 u, u32 v) {
    return func_0039DCF8(p, 3, (u64)u | (u64)v << 16);
}

MovieQw *func_0039DF08(MovieQw *p, u32 r, u32 g, u32 b, u32 a, u32 q) {
    return func_0039DCF8(p, 1, (u64)r | (u64)g << 8 | (u64)b << 16 | (u64)a << 24 | (u64)q << 32);
}

MovieQw *func_0039DF58(MovieQw *p, u32 x, u32 y, u32 z) {
    return func_0039DCF8(p, 5, (u64)x | (u64)y << 16 | (u64)z << 32);
}

MovieQw *func_0039DF90(MovieQw *p, u32 fbp, u32 fbw, u32 psm, u32 fbmsk) {
    return func_0039DCF8(p, 0x4C, (u64)fbp | (u64)fbw << 16 | (u64)psm << 24 | (u64)fbmsk << 32);
}

MovieQw *func_0039DFD8(MovieQw *p, u32 ate, u32 atst, u32 aref, u32 afail, u32 date, u32 datm, u32 zte, u32 ztst) {
    return func_0039DCF8(p, 0x47, (u64)ate | (u64)atst << 1 | (u64)aref << 4 | (u64)afail << 12 | (u64)date << 14 | (u64)datm << 15 | (u64)zte << 16 | (u64)ztst << 17);
}

MovieQw *func_0039E050(MovieQw *p, u32 x0, u32 x1, u32 y0, u32 y1) {
    return func_0039DCF8(p, 0x40, (u64)x0 | (u64)x1 << 16 | (u64)y0 << 32 | (u64)y1 << 48);
}

MovieQw *func_0039E090(MovieQw *p, u32 ofx, u32 ofy) {
    return func_0039DCF8(p, 0x18, (u64)ofx | (u64)ofy << 32);
}

MovieQw *func_0039E0C0(MovieQw *p, u32 ac) {
    return func_0039DCF8(p, 0x1A, (u64)ac);
}

MovieQw *func_0039E0E8(MovieQw *p, u32 wms, u32 wmt, u32 minu, u32 maxu, u32 minv, u32 maxv) {
    return func_0039DCF8(p, 8, (u64)wms | (u64)wmt << 2 | (u64)minu << 4 | (u64)maxu << 14 | (u64)minv << 24 | (u64)maxv << 34);
}

MovieQw *func_0039E148(MovieQw *p, u32 dbp, u32 dbw, u32 dpsm) {
    return func_0039DCF8(p, 0x50, ((u64)dpsm << 56 | (u64)dbw << 48) | (u64)dbp << 32);
}

MovieQw *func_0039E178(MovieQw *p, u32 dir, u32 dsax, u32 dsay) {
    return func_0039DCF8(p, 0x51, ((u64)dir << 59 | (u64)dsay << 48) | (u64)dsax << 32);
}

MovieQw *func_0039E1A8(MovieQw *p, u32 rrw, u32 rrh) {
    return func_0039DCF8(p, 0x52, (u64)rrh << 32 | (u64)rrw);
}

MovieQw *func_0039E1D8(MovieQw *p, u32 xdir) {
    return func_0039DCF8(p, 0x53, (u64)xdir);
}

void func_0039E200(u32 addr) {
    sceGsSyncPath(0, 0);
    *(volatile u32 *)0x1000A030 = addr & 0x0FFFFFFF;
    *(volatile u32 *)0x1000A020 = 0;
    *(volatile u32 *)0x1000A000 = 0x105;
    sceGsSyncPath(0, 0);
}

/* NON_MATCHING: 89 lines: block layout of the VBlank handler (shared counting tail and the two sync / ei exits) */
#if 0
s32 func_0039E268(s32 cause) {
    u8 *tag;
    s32 st;

    D_005F6204 = (*(volatile u64 *)0x12001000 >> 13) & 1;
    D_00436568.vcount++;
    if (D_005F61F8 != 0) {
        D_005F61FC++;
        if ((D_005F6208 = sceGsSyncPath(1, 0)) != 0) {
            goto count;
        }
        tag = func_003A00F8(&D_00436550);
        if (tag == NULL) {
            if (D_00436568.unk10 == 1) {
                D_00436568.unk20++;
                D_00436568.unk24++;
                if (D_00436568.unk20 >= D_00436568.unk1C) {
                    D_00436568.unk1C = D_00436568.unk20;
                }
            }
            __asm__ volatile("sync");
            __asm__ volatile("ei");
            return 0;
        }
        if (D_005F6204 == 0 && *(s32 *)tag == 2) {
            D_00436568.unk20 = 0;
            func_0039D9B8(&D_00436598, 0);
            if (D_00436568.unk10 != 0) {
                func_0039E200((u32)(tag + 0x1AB40));
            }
            *(s32 *)tag = 1;
        } else if (D_005F6204 != 0 && (st = *(s32 *)tag) == 1) {
            D_00436568.unk20 = 0;
            func_0039D9B8(&D_00436598, 1);
            func_0039E200((u32)(tag + 0x40));
            D_00436568.unk10 = st;
            *(s32 *)tag = 0;
            func_003A0160(&D_00436550);
        } else {
        count:
            if (D_00436568.unk10 == 1) {
                D_00436568.unk20++;
                D_00436568.unk24++;
                if (D_00436568.unk20 >= D_00436568.unk1C) {
                    D_00436568.unk1C = D_00436568.unk20;
                }
            }
        }
    }
    __asm__ volatile("sync");
    __asm__ volatile("ei");
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039E268);
#endif

void func_0039E470(s32 field) {
    while (sceGsSyncV(0) == field) {
    }
    D_005F61F8 = 1;
    D_005F61FC = 0;
}

void func_0039E4A8(void) {
    D_005F61F8 = 0;
}

s32 func_0039E4B8(sceMpeg *mp, MovieCbStr *str, MovieReadBuf *rb) {
    u8 *pd0;
    s32 d0;
    u8 *pd1;
    s32 d1;
    u8 *ps0 = str->data;
    u32 len = str->len;
    u32 s0 = rb->data + rb->size - ps0;
    s32 s1;
    s32 ret;

    if (len < s0) {
        s0 = len;
    }
    s1 = len - s0;
    func_0039E948(&D_004365D0, &pd0, &d0, &pd1, &d1);
    ret = func_0039E5C0((u8 *)(((u32)pd0 & 0x0FFFFFFF) | 0x20000000), d0, (u8 *)(((u32)pd1 & 0x0FFFFFFF) | 0x20000000), d1, ps0, s0, rb->data, s1);
    if (ret > 0) {
        func_0039E9C8(&D_004365D0, str->pts, str->dts, pd0, ret);
    }
    func_0039E950(&D_004365D0, ret);
    return ret > 0;
}

s32 func_0039E5C0(u8 *pd0, s32 d0, u8 *pd1, s32 d1, u8 *ps0, s32 s0, u8 *ps1, s32 s1) {
    if (d0 + d1 < s0 + s1) {
        return 0;
    }
    if (s0 >= d0) {
        memcpy(pd0, ps0, d0);
        memcpy(pd1, ps0 + d0, s0 - d0);
        memcpy(pd1 + s0 - d0, ps1, s1);
    } else if (s1 >= d0 - s0) {
        memcpy(pd0, ps0, s0);
        memcpy(pd0 + s0, ps1, d0 - s0);
        memcpy(pd1, ps1 + d0 - s0, s1 - (d0 - s0));
    } else {
        memcpy(pd0, ps0, s0);
        memcpy(pd0 + s0, ps1, s1);
    }
    return s0 + s1;
}

void func_0039E6F8(MovieReadBuf *b) {
    b->size = sizeof(b->data);
    b->put = 0;
    b->count = 0;
}

void func_0039E710(MovieReadBuf *b) {
}

s32 func_0039E718(MovieReadBuf *b, u8 **ptr) {
    s32 size = b->size - b->count;

    if (size != 0) {
        *ptr = b->data + b->put;
    }
    return size;
}

s32 func_0039E748(MovieReadBuf *b, s32 size) {
    s32 ok = b->size - b->count;

    if (size < ok) {
        ok = size;
    }
    b->put = (b->put + ok) % b->size;
    b->count += ok;
    return ok;
}

s32 func_0039E790(MovieReadBuf *b, u8 **ptr) {
    if (b->count != 0) {
        *ptr = b->data + (b->put - b->count + b->size) % b->size;
    }
    return b->count;
}

s32 func_0039E7E0(MovieReadBuf *b, s32 size) {
    s32 ok = b->count;

    if (size < ok) {
        ok = size;
    }
    b->count -= ok;
    return ok;
}

s32 func_0039E808(MovieVideoDec *vd, u8 *work, s32 workSize, u8 *data, u128 *tag, s32 tagSize, MovieTs *ts, s32 nTs) {
    sceMpegCreate(&vd->mpeg, work, workSize);
    sceMpegAddCallback(&vd->mpeg, 0, func_0039EDD0, NULL);
    sceMpegAddCallback(&vd->mpeg, 1, func_0039EDE8, NULL);
    sceMpegAddCallback(&vd->mpeg, 2, func_0039EE18, NULL);
    sceMpegAddCallback(&vd->mpeg, 3, func_0039EE40, NULL);
    sceMpegAddCallback(&vd->mpeg, 4, func_0039EE68, NULL);
    sceMpegAddCallback(&vd->mpeg, 5, func_0039EE70, NULL);
    func_0039E958(vd);
    func_0039F138(&vd->vibuf, data, tag, tagSize, ts, nTs);
    return 1;
}

void func_0039E920(void) {
    func_003F91F8();
}

s32 func_0039E928(MovieVideoDec *vd, s32 type, s32 ch, s32 (*cb)(sceMpeg *, MovieCbStr *, MovieReadBuf *), MovieReadBuf *data) {
    sceMpegAddStrCallback(&vd->mpeg, type, ch, cb, data);
    return 1;
}

void func_0039E948(MovieVideoDec *vd, u8 **pd0, s32 *d0, u8 **pd1, s32 *d1) {
    func_0039F318(&vd->vibuf, pd0, d0, pd1, d1);
}

void func_0039E950(MovieVideoDec *vd, s32 size) {
    func_0039F408(&vd->vibuf, size);
}

s32 func_0039E958(MovieVideoDec *vd) {
    vd->state = 0;
    return 0;
}

s32 func_0039E968(MovieVideoDec *vd) {
    func_0039FA58(&vd->vibuf);
    sceMpegDelete(&vd->mpeg);
    return 1;
}

s32 func_0039E9A0(MovieVideoDec *vd) {
    vd->state = 1;
    return 0;
}

s32 func_0039E9B0(MovieVideoDec *vd) {
    return vd->state;
}

s32 func_0039E9B8(MovieVideoDec *vd, s32 state) {
    s32 old = vd->state;

    vd->state = state;
    return old;
}

void func_0039E9C8(MovieVideoDec *vd, s64 pts, s64 dts, u8 *start, s32 len) {
    MovieTs ts;

    ts.pts = pts;
    ts.dts = dts;
    ts.pos = start - vd->vibuf.data;
    ts.len = len;
    func_0039FCE8(&D_00436618, &ts);
}

s32 func_0039EA08(MovieVideoDec *vd) {
    return func_0039FAF0(&vd->vibuf);
}

s32 func_0039EA28(MovieVideoDec *vd) {
    u8 *pd0;
    s32 d0;
    u8 *pd1;
    s32 d1;

    func_0039E948(vd, &pd0, &d0, &pd1, &d1);
    return d0 + d1;
}

s32 func_0039EA60(MovieVideoDec *vd) {
    MovieSeqEnd code = *(MovieSeqEnd *)D_005F6210;
    s32 n;
    u8 *pd0;
    s32 d0;
    u8 *pd1;
    s32 d1;

    func_0039E948(vd, &pd0, &d0, &pd1, &d1);
    if (d0 + d1 < 4) {
        return 0;
    }
    n = func_0039EEB8((u8 *)(((u32)pd0 & 0x0FFFFFFF) | 0x20000000), d0, (u8 *)(((u32)pd1 & 0x0FFFFFFF) | 0x20000000), d1, code.b, 4, NULL, 0);
    func_0039E950(&D_004365D0, n);
    func_0039FB40(&vd->vibuf);
    if (vd->state == 0) {
        vd->state = 2;
    }
    return 1;
}

s32 func_0039EB30(MovieVideoDec *vd) {
    s32 r = 0;

    if (func_0039EA08(vd) == 0) {
        r = func_003F9240(vd) != 0;
    }
    return r;
}

void func_0039EB78(MovieVideoDec *vd) {
    func_0039F1B0(&vd->vibuf);
    func_0039FFF8(&D_00436550);
    func_0039EBE8(vd);
    while (D_00436550.count != 0) {
    }
    func_0039E9B8(vd, 3);
}

/* NON_MATCHING: 74 lines: the original keeps vd and the result in stack slots and reloads D_00436550 with lui each time; loop layout differs */
#if 0
s32 func_0039EBE8(MovieVideoDec *vd) {
    MovieVideoDec *volatile v = vd;
    volatile s32 r = 1;
    u8 *pic;
    s32 i;
    s32 w;
    s32 h;

    while (func_003F9230(v) == 0) {
        if (func_0039E9B0(v) == 1) {
            r = -1;
            break;
        }
        while ((pic = func_003A00A0(&D_00436550)) == NULL) {
            func_0039CDB0();
        }
        sceMpegGetPicture(&v->mpeg, pic, 0x460);
        if (v->mpeg.frameCount == 0) {
            w = v->mpeg.width;
            h = v->mpeg.height;
            for (i = 0; i < D_00436550.size; i++) {
                func_0039D580(&D_00436598, D_00436550.tag + i * 0x85740 + 0x40, D_00436550.data + i * 0x118000, 0, 0, 0, w, h >> 1, w, h);
                func_0039D580(&D_00436598, D_00436550.tag + i * 0x85740 + 0x1AB40, D_00436550.data + i * 0x118000, 1, 0, 0, w, h >> 1, w, h);
            }
        }
        func_003A0020(&D_00436550);
        func_0039CDB0();
    }
    func_003F9250(v);
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039EBE8);
#endif

s32 func_0039EDD0(void) {
    ((volatile MovieState *)&D_00436568)->unk28 = 1;
    return 1;
}

s32 func_0039EDE8(void) {
    func_0039CDB0();
    func_0039F458(&D_00436618);
    return 1;
}

s32 func_0039EE18(void) {
    func_0039F638(&D_00436618);
    return 1;
}

s32 func_0039EE40(void) {
    func_0039F738(&D_00436618);
    return 1;
}

s32 func_0039EE68(void) {
    return 1;
}

s32 func_0039EE70(sceMpeg *mp, MovieCbTs *cb, void *data) {
    MovieTs ts;

    func_0039FDF8(&D_00436618, &ts);
    cb->pts = ts.pts;
    cb->dts = ts.dts;
    return 1;
}

s32 func_0039EEB8(u8 *pd0, s32 d0, u8 *pd1, s32 d1, u8 *ps0, s32 s0, u8 *ps1, s32 s1) {
    if (d0 + d1 < s0 + s1) {
        return 0;
    }
    if (s0 >= d0) {
        memcpy(pd0, ps0, d0);
        memcpy(pd1, ps0 + d0, s0 - d0);
        memcpy(pd1 + s0 - d0, ps1, s1);
    } else if (s1 >= d0 - s0) {
        memcpy(pd0, ps0, s0);
        memcpy(pd0 + s0, ps1, d0 - s0);
        memcpy(pd1, ps1 + d0 - s0, s1 - (d0 - s0));
    } else {
        memcpy(pd0, ps0, s0);
        memcpy(pd0 + s0, ps1, s1);
    }
    return s0 + s1;
}

u32 func_0039EFF0(MovieViBuf *f, u32 d4madr) {
    if (d4madr == ((u32)(f->tag + (f->n + 1)) & 0x0FFFFFFF)) {
        return 0;
    }
    return (d4madr - (u32)f->data) >> 11;
}

void func_0039F030(u32 chcr) {
    u32 status;

    do {
        __asm__ volatile("di");
        __asm__ volatile("sync.p");
        __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    } while (status & 0x10000);
    *(volatile u32 *)0x1000F590 = *(volatile u32 *)0x1000F520 | 0x10000;
    *(volatile u32 *)0x1000B000 = chcr;
    *(volatile u32 *)0x1000F590 = *(volatile u32 *)0x1000F520 & 0xFFFEFFFF;
    __asm__ volatile("ei");
}

void func_0039F0A0(u32 chcr) {
    u32 status;

    do {
        __asm__ volatile("di");
        __asm__ volatile("sync.p");
        __asm__ volatile("mfc0 %0, $12" : "=r"(status));
    } while (status & 0x10000);
    *(volatile u32 *)0x1000F590 = *(volatile u32 *)0x1000F520 | 0x10000;
    *(volatile u32 *)0x1000B400 = chcr;
    *(volatile u32 *)0x1000F590 = *(volatile u32 *)0x1000F520 & 0xFFFEFFFF;
    __asm__ volatile("ei");
}

void func_0039F110(u64 *tag, u32 addr, u32 id, u32 qwc) {
    *tag = (u64)addr << 32 | (u64)id << 28 | (u64)qwc;
}

s32 func_0039F138(MovieViBuf *f, u8 *data, u128 *tag, s32 n, MovieTs *ts, s32 nTs) {
    MovieSemaParam param;

    f->data = data;
    f->tag = (u128 *)(((u32)tag & 0x0FFFFFFF) | 0x20000000);
    f->buffSize = n << 11;
    f->ts = ts;
    f->nTs = nTs;
    param.initCount = 1;
    param.maxCount = 1;
    f->n = n;
    f->sema = CreateSema(&param);
    func_0039F1B0(f);
    f->totalBytes = 0;
    return 1;
}

s32 func_0039F1B0(MovieViBuf *f) {
    s32 i;

    f->isActive = 1;
    f->dmaStart = 0;
    f->dmaN = 0;
    f->readBytes = 0;
    f->countTs = 0;
    f->wtTs = 0;
    for (i = 0; i < f->nTs; i++) {
        f->ts[i].pts = -1;
        f->ts[i].dts = -1;
        f->ts[i].pos = 0;
        f->ts[i].len = 0;
    }
    for (i = 0; i < f->n; i++) {
        func_0039F110((u64 *)(f->tag + i), ((u32)f->data + (i << 11)) & 0x0FFFFFFF, 3, 0x80);
    }
    func_0039F110((u64 *)(f->tag + i), (u32)f->tag & 0x0FFFFFFF, 2, 0);
    *(volatile u32 *)0x1000B420 = 0;
    *(volatile u32 *)0x1000B410 = (u32)f->data & 0x0FFFFFFF;
    *(volatile u32 *)0x1000B430 = (u32)f->tag & 0x0FFFFFFF;
    func_0039F0A0(5);
    return 1;
}

/* NON_MATCHING: 24 lines: register allocation and order of the ring position / size computation (dmaN + 2 computed first in the original) */
#if 0
void func_0039F318(MovieViBuf *f, u8 **ptr1, s32 *size1, u8 **ptr2, s32 *size2) {
    s32 pos;
    s32 size;

    WaitSema(f->sema);
    {
        s32 dmaN = f->dmaN;
        s32 rd = f->readBytes;

        pos = (((f->dmaStart + dmaN) << 11) + rd) % f->buffSize;
        size = ((f->n - (dmaN + 2)) << 11) - rd;
    }
    if (f->buffSize - pos >= size) {
        *ptr1 = f->data + pos;
        *size1 = size;
        *ptr2 = NULL;
        *size2 = 0;
    } else {
        *ptr1 = f->data + pos;
        *size1 = f->buffSize - pos;
        *ptr2 = f->data;
        *size2 = size - (f->buffSize - pos);
    }
    SignalSema(f->sema);
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039F318);
#endif

void func_0039F408(MovieViBuf *f, s32 size) {
    WaitSema(f->sema);
    f->readBytes += size;
    f->totalBytes += size;
    SignalSema(f->sema);
}

/* NON_MATCHING: 26 lines: register allocation around the dmaN / dmaStart updates and the tag loop arguments */
#if 0
s32 func_0039F458(MovieViBuf *f) {
    s32 consume;
    s32 readStart;
    s32 readN;
    s32 last;
    s32 id;
    s32 i;
    s32 isNew = 0;
    u32 chcr;

    WaitSema(f->sema);
    if (f->isActive == 0) {
        return 0;
    }
    func_0039F0A0(5);
    chcr = *(volatile u32 *)0x1000B400;
    consume = ((s32)func_0039EFF0(f, *(volatile u32 *)0x1000B410) + f->n - f->dmaStart) % f->n;
    f->dmaN -= consume;
    f->dmaStart = (f->dmaStart + consume) % f->n;
    readStart = (f->dmaStart + f->dmaN) % f->n;
    readN = f->readBytes / 2048;
    f->readBytes -= readN * 2048;
    if (readN > 0) {
        last = (readStart + (f->n - 1)) % f->n;
        isNew = 1;
        func_0039F110((u64 *)(f->tag + last), (u32)f->data + (last << 11), 3, 0x80);
    }
    id = readStart;
    for (i = 0; i < readN; i++) {
        func_0039F110((u64 *)(f->tag + id), (u32)f->data + (id << 11), i == readN - 1 ? 0 : 3, 0x80);
        id = (id + 1) % f->n;
    }
    f->dmaN += readN;
    if (f->dmaN != 0) {
        if (isNew) {
            chcr = (chcr & 0x0FFFFFFF) | 0x30000000;
        }
        func_0039F0A0(chcr | 0x100);
    }
    SignalSema(f->sema);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039F458);
#endif

s32 func_0039F638(MovieViBuf *f) {
    WaitSema(f->sema);
    f->isActive = 0;
    func_0039F0A0(5);
    f->d4madr = *(volatile u32 *)0x1000B410;
    f->d4tadr = *(volatile u32 *)0x1000B430;
    f->d4qwc = *(volatile u32 *)0x1000B420;
    f->d4chcr = *(volatile u32 *)0x1000B400;
    while (*(volatile u32 *)0x10002010 & 0xF0) {
    }
    func_0039F030(0);
    f->d3madr = *(volatile u32 *)0x1000B010;
    f->d3qwc = *(volatile u32 *)0x1000B020;
    f->d3chcr = *(volatile u32 *)0x1000B000;
    f->ipubp = *(volatile u32 *)0x10002020;
    f->ipuctrl = *(volatile u32 *)0x10002010;
    SignalSema(f->sema);
    return 1;
}

/* NON_MATCHING: 150 lines: m2c-based draft of viBufRestartDMA; control flow and registers differ */
#if 0
s32 func_0039F738(MovieViBuf *f) {
    u32 bp = f->ipubp;
    s32 fifo = ((bp >> 16) & 3) + ((bp >> 8) & 0xF);
    u32 d4madr = f->d4madr - fifo * 16;
    u32 d4tadr = f->d4tadr;
    u32 d4qwc = f->d4qwc + fifo;
    u32 d4chcr = f->d4chcr | 0x100;
    u32 data;
    s32 size;
    s32 id;
    s32 r;
    s32 bid;
    s32 fid;

    WaitSema(f->sema);
    data = (u32)f->data;
    if (d4madr < data) {
        size = f->n << 11;
        d4qwc = (data - d4madr) >> 4;
        d4tadr = (u32)f->tag & 0x0FFFFFFF;
        d4madr += size;
        id = 0;
        if (f->d4madr != data) {
            id = f->d4madr == data + size ? 0 : 3;
        }
        r = (f->n - f->dmaStart) % f->n;
        d4chcr = (f->d4chcr & 0x0FFFFFFF) | (id << 28) | 0x100;
        if (r < 0 || r >= f->dmaN) {
            f->dmaStart = f->n - 1;
            f->dmaN++;
        }
    } else {
        bid = func_0039EFF0(f, f->d4madr);
        fid = func_0039EFF0(f, d4madr);
        if (bid != fid) {
            d4qwc = (data + (bid << 11) - d4madr) >> 4;
            d4tadr = (u32)(f->tag + bid) & 0x0FFFFFFF;
            id = data + (f->d4madr - data) % (u32)(f->n << 11) == data + (((f->dmaStart + f->dmaN) % f->n) << 11) ? 0 : 3;
            d4chcr = (f->d4chcr & 0x0FFFFFFF) | (id << 28) | 0x100;
            r = (fid + f->n - f->dmaStart) % f->n;
            if (r < 0 || r >= f->dmaN) {
                f->dmaStart = fid;
                f->dmaN++;
            }
        }
    }
    if (f->d3madr != 0 && f->d3qwc != 0) {
        *(volatile u32 *)0x1000B010 = f->d3madr;
        *(volatile u32 *)0x1000B020 = f->d3qwc;
        func_0039F030(f->d3chcr | 0x100);
    }
    if (f->dmaN != 0) {
        while (*(volatile s32 *)0x10002010 < 0) {
        }
        *(volatile u32 *)0x10002000 = bp & 0x7F;
        while (*(volatile s32 *)0x10002010 < 0) {
        }
    }
    *(volatile u32 *)0x1000B410 = d4madr;
    *(volatile u32 *)0x1000B430 = d4tadr;
    *(volatile u32 *)0x1000B420 = d4qwc;
    if (f->dmaN != 0) {
        func_0039F0A0(d4chcr);
    }
    *(volatile u32 *)0x10002010 = f->ipuctrl;
    f->isActive = 1;
    SignalSema(f->sema);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039F738);
#endif

s32 func_0039FA58(MovieViBuf *f) {
    func_0039F0A0(5);
    *(volatile u32 *)0x1000B420 = 0;
    *(volatile u32 *)0x1000B410 = 0;
    *(volatile u32 *)0x1000B430 = 0;
    DeleteSema(f->sema);
    return 1;
}

s32 func_0039FAB0(MovieViBuf *f) {
    s32 r;

    WaitSema(f->sema);
    r = f->isActive;
    SignalSema(f->sema);
    return r;
}

s32 func_0039FAF0(MovieViBuf *f) {
    s32 r;

    WaitSema(f->sema);
    r = (f->dmaN << 11) + f->readBytes;
    SignalSema(f->sema);
    return r;
}

void func_0039FB40(MovieViBuf *f) {
    WaitSema(f->sema);
    f->readBytes = (f->readBytes + 2047) / 2048 * 2048;
    SignalSema(f->sema);
}

/* NON_MATCHING: 1 line: the first division trap compares with zero directly in the original, with the hoisted zero register (t7) here */
#if 0
s32 func_0039FB90(MovieViBuf *f, MovieTs *nts) {
    s32 loop = 1;
    s32 rd = (f->wtTs - f->countTs + f->nTs) % f->nTs;
    s32 size = f->n << 11;
    s32 len;
    s32 c;
    MovieTs *ts;

    if (f->countTs > 0) {
        while (loop) {
            ts = f->ts + rd;
            if (ts->len == 0 || nts->len == 0) {
                break;
            }
            if ((ts->pos + size - nts->pos) % size < nts->len) {
                len = nts->pos + nts->len - ts->pos;
                if (ts->len < len) {
                    len = ts->len;
                }
                ts->pos = (ts->pos + len) % size;
                ts->len -= len;
                if (ts->len == 0) {
                    if (ts->pts >= 0) {
                        ts->pts = -1;
                        ts->dts = -1;
                        ts->pos = 0;
                        ts->len = 0;
                    }
                    c = f->countTs - 1;
                    if (c < 0) {
                        c = 0;
                    }
                    f->countTs = c;
                }
            } else {
                loop = 0;
            }
            rd = (rd + 1) % f->nTs;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039FB90);
#endif

s32 func_0039FCE8(MovieViBuf *f, MovieTs *ts) {
    s32 r = 0;

    WaitSema(f->sema);
    if (f->countTs < f->nTs) {
        func_0039FB90(f, ts);
        if (ts->pts >= 0 || ts->dts >= 0) {
            f->ts[f->wtTs].pts = ts->pts;
            f->ts[f->wtTs].dts = ts->dts;
            f->ts[f->wtTs].pos = ts->pos;
            f->ts[f->wtTs].len = ts->len;
            f->countTs++;
            f->wtTs = (f->wtTs + 1) % f->nTs;
        }
        r = 1;
    }
    SignalSema(f->sema);
    return r;
}

/* NON_MATCHING: 28 lines: the original reloads f->ts for each store and keeps the index expression; loop entry differs */
#if 0
s32 func_0039FDF8(MovieViBuf *f, MovieTs *ts) {
    s32 isEnd = 0;
    u32 madr = *(volatile u32 *)0x1000B410;
    u32 bp = *(volatile u32 *)0x10002020;
    s32 last = f->ipubp & 0x7F;
    u32 size = f->n << 11;
    s32 count;
    s32 i;
    u32 pos;
    MovieTs *t;

    madr -= (((bp >> 16) & 3) + ((bp >> 8) & 0xF)) * 16;
    WaitSema(f->sema);
    ts->dts = -1;
    ts->pts = -1;
    count = f->countTs;
    pos = (madr + (last >> 3) + size - (u32)f->data) % size;
    for (i = 0; i < count && isEnd == 0; i++) {
        t = f->ts + (f->wtTs - count + f->nTs + i) % f->nTs;
        if ((s32)(pos + size - t->pos) % (s32)size < t->len) {
            isEnd = 1;
            ts->pts = t->pts;
            ts->dts = t->dts;
            t->pts = -1;
            t->dts = -1;
            f->countTs -= f->countTs < 2 ? f->countTs : 1;
        }
    }
    SignalSema(f->sema);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039FDF8);
#endif

/* NON_MATCHING: 14 lines: order of the field stores against the loop counter (best store order gives 12) */
#if 0
void func_0039FFA0(MovieVoBuf *f, u8 *data, u8 *tag, s32 size) {
    s32 i;

    f->count = 0;
    f->data = data;
    f->tag = tag;
    f->size = size;
    f->write = 0;
    for (i = 0; i < size; i++) {
        *(s32 *)(f->tag + i * 0x85740) = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_0039FFA0);
#endif

void func_0039FFF0(MovieVoBuf *f) {
}

void func_0039FFF8(MovieVoBuf *f) {
    f->write = 0;
    f->count = 0;
}

s32 func_003A0008(MovieVoBuf *f) {
    return f->count == f->size;
}

/* NON_MATCHING: 19 lines: the original reloads write after the count store; register allocation of the DI section */
#if 0
void func_003A0020(MovieVoBuf *f) {
    u8 *tag = f->tag;
    u32 status;

    do {
        __asm__ volatile("di");
        __asm__ volatile("sync.p");
        __asm__ volatile("mfc0 %0, $12" : "=r"(status) : : "memory");
    } while (status & 0x10000);
    *(s32 *)(tag + f->write * 0x85740) = 2;
    f->count++;
    f->write = (f->write + 1) % f->size;
    __asm__ volatile("ei");
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_003A0020);
#endif

u8 *func_003A00A0(MovieVoBuf *f) {
    return func_003A0008(f) ? NULL : f->data + f->write * 0x118000;
}

s32 func_003A00E8(MovieVoBuf *f) {
    return f->count == 0;
}

/* NON_MATCHING: 4 lines: the volatile count load is scheduled before the lui of 0x85740 here, after it in the original */
#if 0
u8 *func_003A00F8(MovieVoBuf *f) {
    return func_003A00E8(f) ? NULL : f->tag + (f->write - f->count + f->size) % f->size * 0x85740;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_003A00F8);
#endif

void func_003A0160(MovieVoBuf *f) {
    if (f->count > 0) {
        f->count--;
    }
}

s32 func_003A0180(s32 x, s32 z) {
    return (func_00366108(x, z) - 0x400) & 0xFFF;
}

void func_003A01A8(s32 x, s32 y, s32 z, s16 *out) {
    s32 ry = func_003A0180(x, z);

    out[1] = ry;
    out[0] = func_003A0180(y, func_003A0218(x, z, ry));
}

/* NON_MATCHING: 42 lines: the original computes both absolute values with movn before the test and loads the sine in each arm */
#if 0
s32 func_003A0218(s32 x, s32 z, s32 ang) {
    s32 az = z > -1 ? z : -z;
    s32 ax = x > -1 ? x : -x;
    s32 num;
    s32 d;
    s32 q;
    s32 r;

    if (az < ax) {
        d = gSinTable[ang & 0xFFF];
        q = 1;
        num = x << 16;
    } else {
        d = gSinTable[(ang & 0xFFF) + 0x400];
        q = 1;
        num = z << 16;
    }
    d = (s16)(d << 3);
    if (d != 0) {
        q = d;
    }
    r = num / q >> 1;
    return r > -1 ? r : -r;
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_003A0218);
#endif

/* NON_MATCHING: 20 lines: pointer and counter registers of the 9-element loop */
#if 0
void func_003A02C0(s32 unused, u8 *f, u16 *base, s16 *delta, s16 *out) {
    s16 tmp[9];
    s16 *t = tmp;
    s32 k = *(s16 *)((s32)f + 0xB884) * 2;
    s32 i;

    for (i = 0; i < 9; i++) {
        *t++ = *base++ + *delta++ * k / 8192;
    }
    func_003A0388(tmp, out);
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_003A02C0);
#endif

s16 func_003A0340(u8 *f, s16 v) {
    return v + *(s16 *)((s32)f + 0xB872) * (s16)(*(s16 *)((s32)f + 0xB884) * 2) / 8192;
}

/* NON_MATCHING: 23 lines: load and store order of the two normalised columns */
#if 0
void func_003A0388(s16 *in, s16 *out) {
    s32 a[4];
    s32 b[4];
    s32 v[4];
    s32 d;

    v[0] = in[0];
    v[1] = in[3];
    v[2] = in[6];
    func_0039BDF0(v, a);
    out[0] = a[0];
    out[6] = a[2];
    out[3] = a[1];
    d = (in[1] * a[0] + in[4] * a[1] + in[7] * a[2]) >> 12;
    v[0] = in[1] - (d * a[0] >> 12);
    v[2] = in[7] - (d * a[2] >> 12);
    v[1] = in[4] - (d * a[1] >> 12);
    func_0039BDF0(v, b);
    out[1] = b[0];
    out[4] = b[1];
    out[7] = b[2];
    func_0039BEC8(a, b, v);
    out[2] = v[0];
    out[8] = v[2];
    out[5] = v[1];
}
#else
INCLUDE_ASM("asm/nonmatchings/movie", func_003A0388);
#endif
