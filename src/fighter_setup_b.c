/* 0x364448-0x3682F8 (module fighter_setup, docs/game_overview.md section 4). */
#include "common.h"
#include "fighter.h"

/* Fighter fields this file reads (an overlay with the full slot size). */
typedef struct {
    /* 0x0000 */ u8 pad0[0x51C];
    /* 0x051C */ s32 unk51C[3];               /* position */
    /* 0x0528 */ u8 pad528[0xAB44];
    /* 0xB06C */ s32 unkB06C[2];
    /* 0xB074 */ s32 unkB074[2];
    /* 0xB07C */ f32 unkB07C[2];
    /* 0xB084 */ u8 padB084[0x4F4];
    /* 0xB578 */ u8 padB578[0x328];
} FighterFsb; /* size 0xB8A0 */

#define FSB(f) ((FighterFsb *)(f))

/* A fighter's state on one replay frame (0x38 bytes). */
typedef struct {
    /* 0x00 */ struct FighterMove *unk0;      /* unkC0 (and unkB8) */
    /* 0x04 */ struct FighterMove *unk4;      /* unkB888 */
    /* 0x08 */ struct FighterMove *unk8;      /* unkB88C */
    /* 0x0C */ u8 unkC;                       /* unkB890 */
    /* 0x0D */ u8 unkD;                       /* unkB891 */
    /* 0x0E */ u8 unkE;                       /* unkC4 */
    /* 0x0F */ u8 unkF;                       /* unkB878 */
    /* 0x10 */ u8 unk10;                      /* unkB5DC */
    /* 0x11 */ u8 unk11;                      /* unkB5E0 */
    /* 0x12 */ u8 pad12[0x2];
    /* 0x14 */ s32 unk14[3];                  /* position unk51C */
    /* 0x20 */ u16 unk20[3];
    /* 0x26 */ s16 unk26;
    /* 0x28 */ u8 unk28[2];
    /* 0x2A */ u8 unk2A[2];                   /* unkB07C * 128 */
    /* 0x2C */ u8 unk2C[2];                   /* unkB074 */
    /* 0x2E */ s16 unk2E[2];                  /* unkB06C */
    /* 0x32 */ u8 unk32;
    /* 0x33 */ u8 unk33;
    /* 0x34 */ u16 unk34;                     /* unk28 */
    /* 0x36 */ u8 pad36[0x2];
} ReplayFrame;

/* A replay: 0x78 frames of both fighters (0x3480 bytes). Slots 0-7 are saved replays, 8 the one being recorded. */
typedef struct {
    /* 0x0000 */ ReplayFrame unk0[2][0x78];
} ReplaySlot;

typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ u16 unk4;
} ReplayChr;

typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
} ReplayVoice;

typedef struct {
    s32 v[4];
} ReplayVec;

extern s32 D_0085A600[];                   /* game type of each slot (8 saved + the recording), 0x17: empty */
extern s32 D_0085A628[];                   /* last frame of each slot */
extern ReplayVec D_0085A650[];
extern s32 D_0085A6E0[];
extern s32 D_0085A708[];
extern u16 D_0085A730[][2];                /* both fighters' unkB4 */
extern ReplayChr D_0085A758[][9];          /* both fighters' unk26 / unk28 / unk2C per slot */
extern ReplaySlot D_0085A7D0[];
extern ReplayFrame D_00878050[];
extern ReplayVoice D_008780C0[][0x78];
extern Fighter gFighters[];

extern s32 D_005F5E48;
extern s32 D_005F6B80;
extern s32 D_005F6B84;
extern s32 D_005F6B88;
extern s32 D_005F6B8C;
extern s32 D_005F6B90;
extern s32 D_005F6B94;
extern s32 D_005F6B98;
extern s32 D_005F6B9C;
extern s32 D_005F6BA0;
extern s32 D_005F6BA4;
extern s32 D_005F6BA8;
extern s32 D_005F5F20;
extern u16 D_005F5CB0;
extern u16 D_005F5CB2;
extern u16 D_005F5CB6;
extern u8 D_005F5CCC;
extern s32 gRoundPhase;
extern s16 gMode;

void HitLog_Save(s16 slot);
void HitLog_Load(s16 slot, s16 back);
void HitLog_Rewind(s16 n);
void HitLog_Disable(void);
void func_003D46F0(void);
void func_003D45A8(s32 n);
void func_00373320(void);
void func_00396AB0(Fighter *a, Fighter *b, s32 c);
void func_003D0748(Fighter *a, Fighter *b);
void func_003D4728(s32 slot);
void func_003D0BD0(ReplayVec *out);
s32 func_003D0C00(s32 a);
s32 func_003D0C10(Fighter *a, Fighter *b);
s32 func_00374D90(s32 id, s32 player);
s32 func_00374E00(s32 id, s32 player);
void func_003D0660(Fighter *a, Fighter *b);
void func_003D0C08(s32 n);
void func_003D0C28(Fighter *a, Fighter *b, s32 v, ReplayVec *p);
void func_003D4638(s32 slot, s32 n);
void func_003ADB20(void);
void func_003ADD60(s32 a);
void func_003AFDA8(s32 side, s32 slot, s32 b);
void func_0036D010(s16 cpu1, s16 cpu2);
void func_0036D5D0(void);
void func_00373380(Fighter *f);
void func_00363150(Fighter *f);
void func_00362B18(Fighter *f);
void func_00368168(Fighter *f);

s32 Lz_Decompress(u8 *src, u8 *dst);
void *func_00364528(u32 i, void *bank);
s32 func_00364550(void *bank);
s32 func_00364558(u32 i, void *bank);
void func_00364568(s32 *p);
void func_003645A0(s32 *p);
void func_003645D8(s32 n);
void func_00364678(Fighter *f);
void func_00364698(void);
void func_003647E8(void);
s32 func_00364B50(void);
s32 func_00364EE8(void);
void func_00364EF8(void);
s32 func_00364F08(void);
s32 func_00364F18(void);
void func_00364F28(void);
void func_00364F38(Fighter *f);
void func_00365108(Fighter *f, u16 *r);
void func_00365248(Fighter *f);
void func_00365300(Fighter *f);
s32 func_00365520(Fighter *f, s32 v);
s32 func_00365610(Fighter *f, s32 v, s32 i);
void func_00365718(Fighter *f);
s32 func_003659B0(Fighter *f, s32 v);
s32 func_00365AC0(Fighter *f, s32 v);
void func_00365BB0(u16 id, u16 player);
void func_00365C30(void);
s32 func_00365C80(void);
s32 func_00365D00(void);
s32 func_00365D10(s32 slot, s32 *last);
void func_00366090(void);
s16 func_003660B8(s16 x);
s16 func_00366108(s32 dx, s32 dz);
s16 func_003662D8(s32 dx, s32 dz);
s32 func_00366360(s32 i);
void func_00366380(Fighter *f);
void func_003663C0(Fighter *f, s32 id);
s16 func_00366410(s32 id);
s32 func_00366460(s32 n);
s32 func_003664E0(Fighter *f, s32 i);
s32 func_00366518(Fighter *f);
s32 func_00366540(Fighter *f);
s32 func_00366568(Fighter *f);
s32 func_00366590(Fighter *f);
s32 func_003665B8(Fighter *f);
void func_00366EA8(void *m);
void func_00366EB8(void *m);
f32 func_00366EC8(void);
void func_003671D0(f32 a);
void func_003673B0(Fighter *f);
void func_003673F8(void);
void func_00367450(Fighter *f);
void func_00367488(Fighter *f, s32 flags);
void func_00367600(Fighter *f);
s32 abs(s32 x);
void File_WaitDone(s32 unused);
void sceVu0CopyMatrix(void *dst, void *src);
void sceVu0MulMatrix(void *m0, void *m1, void *m2);
void sceVu0RotMatrixY(void *m0, void *m1, f32 ry);
void Mtx_Unit(void *m);

/* A fighter slot's model data (0x2C bytes, D_008794C0[4]): the pointers of its model bank entries. */
typedef struct {
    void *unk0;
    void *unk4;
    void *unk8;
    void *unkC;
    void *unk10;
    void *unk14;
    void *unk18;
    void *unk1C;
    void *unk20;
    void *unk24;
    void *unk28;
} FsbModels;

extern const s16 D_004FC680[];
extern const s16 D_004FF690[];
extern const s8 D_00416A90[];
extern const s8 D_00416C28[];
extern const s8 D_00416BA0[];
extern const s8 D_00416CB0[];
extern const s8 D_00416D38[];
extern const s8 D_00416DC0[];
extern FsbModels D_008794C0[];
extern f32 D_005F5E78;
extern u128 D_00879420[4];
extern u128 D_00879460[4];
extern u128 D_00433400[4];
extern u128 D_00433480[4];

/* An image record of the GS image packets (0x20 bytes, D_004176E0[4]). */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u32 unk4;                      /* VRAM address */
    /* 0x08 */ u32 unk8;                      /* width */
    /* 0x0C */ u32 unkC;                      /* format */
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
} FsbImage;

typedef struct {
    s32 w[8];
} FsbWords8;

typedef struct FsbNode {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ FsbWords8 unk4;
    /* 0x24 */ u8 pad24[0x24];
    /* 0x48 */ struct FsbNode *unk48;
    /* 0x4C */ struct FsbNode *unk4C;
} FsbNode;

typedef struct {
    f32 v[4];
} __attribute__((aligned(16))) FsbVec;

typedef struct {
    s32 unk0;
    s32 unk4;
    s32 unk8;
} FsbFile;

/* Per command slot (0x24 bytes, D_004177E0): three file requests (voice bank, model bank, end marker). */
typedef struct {
    FsbFile unk0[3];
} FsbFiles;

typedef struct {
    s32 unk0;
    s32 unk4;
    u8 pad8[0x58];
} FsbTeamMember;

typedef struct {
    s32 unk0;
    s32 unk4;
    s32 unk8;
    s32 unkC;
    s32 unk10;
    s32 unk14;
    s32 unk18;
    s32 unk1C;
} FsbCamPos;

typedef struct {
    s32 unk0;
    s32 unk4;
    s32 unk8;
    s32 unkC;
    s32 unk10;
    s32 unk14;
    s32 unk18;
    s32 unk1C;
} FsbCam;

extern u8 D_00416E50[];
extern FsbImage D_004176E0[];
extern const FsbWords8 D_00417760;
extern s32 D_00417780[];
extern FsbFiles D_004177E0[];
extern FsbTeamMember D_00440940[];
extern FsbCam D_00415808;
extern FsbCamPos D_00433520;
extern const s16 gSinTable[];
extern s32 D_005F6730;
extern s32 D_005F6BAC;
extern s32 D_005F6BB0;
extern s32 D_005F6BB4;
extern s32 D_005F6BB8;
extern s32 D_005F6BBC;
extern s32 D_005F6BC0;
extern s32 D_005F6BC4;
extern s32 D_005F6BC8;
extern s32 D_005F6BCC;
extern f32 D_008794A0[];
extern void *D_008795C8[];
extern void *D_008795E0[];
extern s32 *D_005F5E7C;
extern s32 D_005F5E80;
extern s32 D_005F5E84;
extern struct FighterMove D_00531EA0[];
extern u8 D_00482248[];
extern struct MoveMod D_00524690[];
extern u16 D_0052C448[];
extern struct FighterMove *D_00417900[][0x159C];
extern u16 D_0058F3D0[][0x159C];
extern struct FighterMove **D_00879600[];
extern s16 D_005E8608[];

u64 *func_003994C0(void);
void func_00399580(u64 *p);
void Sprite_SendImagePacket(u64 *pk, s32 qwc);
void Sprite_UploadBank(s32 bank, u64 *data, s32 size, s32 r);
void Sprite_SetBlend(s32 a, s32 b);
void Sprite_SetFilter(s32 a, s32 b);
void exit(s32 status) __attribute__((noreturn));
s32 func_003A0180(s32 a, s32 b);
s32 func_00393810(void);
s32 func_0036E158(void);
s32 func_003ABD78(void);
s32 func_003BB4D8(void);
f32 func_003D80E8(f32 a);
void sceVu0ApplyMatrix(f32 *out, void *m, f32 *v);
void sceVu0InversMatrix(void *m0, void *m1);
s32 File_Request(s32 id, u32 dest, s32 flags, s32 toIop);
u32 Snd_GetBankAddr(s32 slot);
s32 func_0035CED8(s32 id);
s32 func_0035CEF8(s32 n);
s32 func_003ABCC0(void);
s32 func_003B87E0(s32 i);
s32 File_Release(s32 handle);
void FlushCache(s32 op);
void Fx_LoadFighterTex(Fighter *f);
void Snd_SetBankOwner(s32 slot, s32 chara);
void func_00344EA8(Fighter *f);
void func_0034E130(void *p);
void func_0037D0F8(void *src, s32 slot);
void func_003AF540(Fighter *f, s32 a, s32 b);
s32 func_003B87B0(s32 i);
s32 func_003B87C8(s32 i);
void func_003DC870(s32 i);
void Fx_LoadStageLayers(s32 base);
void func_00342360(s32 i);
void func_00345070(void);
u32 func_00354368(void *rec);
void func_00356E20(s32 x, void *p, s32 a);
void func_00357160(s32 id, s32 slot, void *t, s32 a);
s32 func_0035CF20(u32 dest, s32 n, s32 max);
s32 func_0035CF78(u32 dest, s32 n, s32 max);
s32 func_0035D348(s32 n);
void func_00361D50(s32 i, u16 size, void *data);
void func_00361EA8(s32 *blocks, s32 *subBlocks);
u32 func_003B8788(void);
void func_003D0C68(s32 a, s32 b);
void func_003D0CC8(s32 scene);
void func_003D0E18(void);
void func_003DB340(void);
void func_003DB368(s32 stage, void *bank, s32 *arg);
void func_003DB578(void);
s32 func_003E7210(s32 a);
void func_003682F8(struct FighterMove *m);
u8 *func_003B8760(void);
u8 *func_003B8770(void);
void func_00362328(void);
s32 func_00363888(void);
s32 func_003638F0(void);
u8 *func_003639E8(void);
void func_003665E0(s32 x, s32 y, FsbImage *img, FsbImage *clut, s32 c);
s32 func_00366820(u64 *pk);
u64 *func_00366880(u64 *pk, FsbImage *img);
void func_00366A20(u64 *buf, s32 size);
void func_00366AB8(f32 a, f32 b);
void func_00366B68(FsbNode *parent, FsbNode *n);
void func_00366BD0(s32 angle, s32 r, s32 *x, s32 *z);
s32 func_00366C18(void);
void func_00366DB0(s32 n);
void func_00366DC8(void);
void func_00366E38(void);
void func_00366ED0(void);
void *func_00367250(s32 i);
void *func_00367270(s32 i);
void *func_00367290(s32 i);
void *func_003672B0(s32 i);
void *func_003672D0(s32 i);
void *func_003672F0(s32 i);
void *func_00367310(s32 i);
void *func_00367330(s32 i);
void *func_00367350(s32 i);
void *func_00367370(s32 i);
void *func_00367390(s32 i);
s32 func_00367850(void);
void *func_00367858(void);
void *func_00367868(void);
void *func_00367878(void);
void *func_00367888(void);
void *func_00367898(void);
void *func_003678A8(void);
void *func_003678B8(void);
s32 func_003678C8(void);
void func_003678D0(void);
void func_00367930(void);
void func_00367938(s32 stage);
void func_00367CA0(s16 *p);
s32 func_00367CC8(void);
u16 func_00367CD0(s32 i);
void *func_00367D00(s32 i);
void func_00367D28(u16 *data, s16 *out, s32 frame, s32 n);
void func_00367FE0(u16 *data, s16 *out, s32 frame);
void func_00367FE8(u16 *data, s16 *out, s32 frame);
void func_00367FF0(void);
void func_00368140(Fighter *a, Fighter *b);
struct FighterMove **func_00368200(s32 side);
struct FighterMove *func_00368218(Fighter *f, s16 move);
struct FighterMove *func_00368248(s32 id);
u16 func_00368260(s32 chr, s32 move);
s16 func_00368288(s16 n);
s32 func_003682A8(s32 a, s16 b);
void func_00366098(void);
void func_003660A8(void);

/* LZ decompression: a flag byte (bit 0 first, ended by the highest set bit) before each group of literal bytes and
 * 16-bit back references (11-bit distance, 0 = 0x800; 5-bit length, 0 = 32); a zero flag byte ends the data. Returns
 * the size written. */
/* NON_MATCHING: 45 lines: the original keeps src and src + 1 in two registers (a0 / a3) through both loops; every loop form tried keeps one pointer */
#if 0
s32 Lz_Decompress(u8 *src, u8 *dst) {
    u8 *start = dst;
    u8 flags;
    s32 w;
    s32 dist;
    s32 len;
    s32 i;
    u8 *from;

    for (flags = *src++; flags != 0; flags = *src++) {
        for (; flags >= 2; flags >>= 1) {
            if (flags & 1) {
                *dst++ = *src++;
            } else {
                w = (src[0] << 8) + src[1];
                src += 2;
                dist = w & 0x7FF;
                if (dist == 0) {
                    dist = 0x800;
                }
                from = dst - dist;
                len = (w >> 11) & 0x1F;
                if (len == 0) {
                    len = 0x20;
                }
                for (i = 0; i < len; i++) {
                    *dst++ = *from++;
                }
            }
        }
    }
    return dst - start;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", Lz_Decompress);
#endif

void *func_00364528(u32 i, void *bank) {
    s32 *b = bank;
    s32 off = b[i * 2 + 1];

    if ((s32)i >= b[0]) {
        return NULL;
    }
    return (u8 *)b + off;
}

s32 func_00364550(void *bank) {
    return *(s32 *)bank;
}

s32 func_00364558(u32 i, void *bank) {
    return ((s32 *)bank)[i * 2 + 2];
}

/* Wraps a frame index into the 120-frame ring. */
void func_00364568(s32 *p) {
    if (*p < 0) {
        *p += 0x1E00;
    }
    *p %= 0x78;
}

void func_003645A0(s32 *p) {
    if (*p < 0) {
        *p += 0x1E00;
    }
    *p %= 0xF0;
}

void func_003645D8(s32 n) {
    s32 i;

    switch (n) {
    case 0:
        for (i = 0; i < 9; i++) {
            D_0085A600[i] = 0x17;
        }
        break;
    case 1:
        D_005F6B84 = 0;
        break;
    case 2:
        D_005F5E48 = 7;
        D_005F6B90 = -2;
        D_005F6B80 = 8;
        D_005F6B88 = 0;
        D_005F6B8C = 0;
        D_005F6B94 = 0;
        D_005F6B98 = 0;
        D_005F6BA0 = 0;
        break;
    }
    func_003D46F0();
}

void func_00364678(Fighter *f) {
    D_005F6B90 = (D_005F6B88 - f->unkC4 - 0x1E) * 2;
    func_003645A0(&D_005F6B90);
}

void func_00364698(void) {
    s32 i;
    s32 n;

    D_0085A600[8] = D_005F5CB6;
    for (i = 0; i < 2; i++) {
        D_0085A758[i][8].unk0 = gFighters[i].unk26;
        D_0085A758[i][8].unk2 = gFighters[i].unk28;
        D_0085A758[i][8].unk4 = gFighters[i].unk2C;
    }
    if (D_005F6B90 >= 0) {
        n = D_005F6B88 - (D_005F6B90 >> 1);
        func_00364568(&n);
        if (n < 0x96) {
            D_005F6B90 = (D_005F6B88 - 0x96) * 2;
        }
        func_003645A0(&D_005F6B90);
        D_005F6B90 = (D_005F6B88 - 0x14) * 2;
        func_003645A0(&D_005F6B90);
        D_005F5E48 = 2;
        D_005F5F20 = 0x14;
        n = D_005F6B88 - (D_005F6B90 >> 1);
        D_005F6B8C = D_005F6B90;
        func_00364568(&n);
        HitLog_Rewind(n);
        func_003D45A8(n);
    } else {
        D_005F5E48 = 5;
        D_005F6B90 = D_005F6B88 * 2 - 0x140;
        func_003645A0(&D_005F6B90);
    }
    HitLog_Disable();
}

/* Saves the recorded replay (slot 8) into the slot D_005F5CCC. */
/* NON_MATCHING: 122 lines: the || test of the two fighters is laid out differently, then register allocation of the 0x38-byte frame copy loop */
#if 0
void func_003647E8(void) {
    s32 i;
    s32 side;
    s32 j;
    s32 n;
    u8 slot;

    if (!((gFighters[0].unkB4 > 0 && D_005F5CB0 != 0) || (gFighters[1].unkB4 > 0 && D_005F5CB2 != 0))) {
        return;
    }
    if (D_005F6B90 >= 0) {
        D_005F6B84 = 1;
    } else if (D_005F6B84 != 0) {
        return;
    }
    slot = D_005F5CCC;
    D_0085A600[slot] = D_0085A600[8];
    n = D_005F6BA0;
    if (n > 0x78) {
        n = 0x78;
        D_005F6BA0 = 0x78;
    }
    D_0085A628[slot] = n - 1;
    HitLog_Save(D_005F5CCC);
    slot = D_005F5CCC;
    for (side = 0; side < 2; side++) {
        D_0085A758[side][slot] = D_0085A758[side][8];
    }
    j = 0;
    if (D_005F6BA0 >= 0x78) {
        j = D_005F6B88;
    }
    for (i = 0; i < 0x78; i++) {
        for (side = 0; side < 2; side++) {
            D_0085A7D0[slot].unk0[side][i] = D_0085A7D0[8].unk0[side][j];
        }
        D_008780C0[slot][i] = D_008780C0[8][j];
        j++;
        if (j >= 0x78) {
            j = 0;
        }
        func_003D0BD0(&D_0085A650[D_005F5CCC]);
        D_0085A708[D_005F5CCC] = func_003D0C10(&gFighters[0], &gFighters[1]);
        n = func_003D0C00(0);
        slot = D_005F5CCC;
        D_0085A730[slot][0] = gFighters[0].unkB4;
        D_0085A6E0[slot] = n;
        D_0085A730[slot][1] = gFighters[1].unkB4;
    }
    func_003D4728(slot);
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_003647E8);
#endif

/* The replay player's per-frame step (D_005F5E48: 1 playing, 2 starting, 3 first frame, 4 back to the fight, 5 off,
 * 6 replay of a saved slot, 7 recording, 8 stopped); returns the state, 0 when off. */
/* NON_MATCHING: 106 lines: register allocation and the hoisted side * 0x1A40 / 0xB8A0 / 0x38 products of the two position loops */
#if 0
s32 func_00364B50(void) {
    s32 end = D_005F6B88 * 2 - 2;
    s32 side;
    s32 k;
    s32 n;
    s32 x;
    s32 r;

    if (end <= -1) {
        end = 0xEE;
    }
    switch (D_005F5E48) {
    case 2:
        D_005F5E48 = 3;
        for (side = 0; side < 2; side++) {
            for (k = 0; k < 3; k++) {
                D_00878050[side].unk14[k] = FSB(&gFighters[side])->unk51C[k];
                FSB(&gFighters[side])->unk51C[k] = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk14[k];
            }
        }
        func_00396AB0(&gFighters[0], &gFighters[1], 0);
        func_00363150(&gFighters[0]);
        func_00363150(&gFighters[1]);
        break;
    case 3:
        D_005F6B98 = 0;
        D_005F5E48 = 1;
        break;
    case 1:
        D_005F6B8C += 2;
        func_003645A0(&D_005F6B8C);
        if (D_005F6B8C == end || D_005F6B98 != 0) {
            D_005F5E48 = 4;
        }
        func_00365C80();
        break;
    case 6:
        if (D_005F6BA4 >= 0 && D_005F6BA4 < 3) {
            D_005F6B8C += 2;
        } else {
            D_005F6B8C += 1;
        }
        func_003645A0(&D_005F6B8C);
        if (D_005F6BA4 != 3) {
            x = 0x46;
            end = D_005F6B88 * 2 - 0xAA;
        } else {
            x = 0xC6;
            end = D_005F6B88 * 2 - 0x2A;
        }
        if (end <= -1) {
            end = x;
        }
        if (D_005F6B8C == end) {
            D_005F6BA4++;
            if (D_005F6BA4 == 4) {
                D_005F6B9C = 0;
            } else {
                n = D_005F6B88 - 0x77;
                func_00364568(&n);
                HitLog_Load(D_005F6BA8, n);
                D_005F6B8C = (D_005F6B88 - 0x76) * 2;
                func_003645A0(&D_005F6B8C);
                for (side = 0; side < 2; side++) {
                    for (k = 0; k < 3; k++) {
                        FSB(&gFighters[side])->unk51C[k] = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk14[k];
                    }
                }
                gRoundPhase = 3;
                func_003D0748(&gFighters[0], &gFighters[1]);
            }
        }
        func_00365C80();
        break;
    case 4:
        D_005F5E48 = 5;
        func_00373320();
        break;
    case 5:
    case 8:
        break;
    case 7:
        D_005F6B88++;
        D_005F6BA0++;
        func_00364568(&D_005F6B88);
        func_00365C30();
        break;
    }
    r = D_005F5E48;
    if (r == 5 || r == 7 || r == 8) {
        r = 0;
    }
    return r;
}
#else
INCLUDE_RODATA("asm/nonmatchings/fighter_setup_b", jtbl_004FC630);
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_00364B50);
#endif

s32 func_00364EE8(void) {
    return D_005F6B9C == 0;
}

void func_00364EF8(void) {
    D_005F5E48 = 8;
}

s32 func_00364F08(void) {
    return D_005F5E48 == 8;
}

s32 func_00364F18(void) {
    return D_005F5E48 == 5;
}

void func_00364F28(void) {
    D_005F6B98 = 1;
}

/* Replay of the position: on playback frames (odd half frames average two recorded frames), recorded on record. */
void func_00364F38(Fighter *f) {
    s32 side = f->unk1E;
    s32 k;
    s32 next;

    if (D_005F5E48 == 1 || D_005F5E48 == 3 || D_005F5E48 == 6) {
        next = (D_005F6B8C >> 1) + 1;
        func_00364568(&next);
        for (k = 0; k < 3; k++) {
            if (D_005F6B8C & 1) {
                FSB(f)->unk51C[k] = (D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk14[k] +
                                     D_0085A7D0[D_005F6B80].unk0[side][next].unk14[k]) / 2;
            } else {
                FSB(f)->unk51C[k] = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk14[k];
            }
        }
    } else if (D_005F5E48 == 4) {
        for (k = 0; k < 3; k++) {
            FSB(f)->unk51C[k] = D_00878050[side].unk14[k];
        }
    }
    if (D_005F5E48 == 7) {
        for (k = 0; k < 3; k++) {
            D_0085A7D0[8].unk0[side][D_005F6B88].unk14[k] = FSB(f)->unk51C[k];
        }
    }
}

void func_00365108(Fighter *f, u16 *r) {
    s32 side = f->unk1E;
    ReplayFrame *fr;
    u16 *q;

    if (D_005F5E48 == 3) {
        D_00878050[side].unk20[0] = r[0];
        D_00878050[side].unk20[1] = r[1];
        D_00878050[side].unk20[2] = r[2];
    }
    if (D_005F5E48 == 1 || D_005F5E48 == 3 || D_005F5E48 == 6) {
        fr = (ReplayFrame *)((u8 *)D_0085A7D0 + ((D_005F6B8C >> 1) * 0x38 + side * 0x1A40 + D_005F6B80 * 0x3480));
        r[0] = fr->unk20[0];
        r[1] = fr->unk20[1];
        r[2] = fr->unk20[2];
    } else if (D_005F5E48 == 4) {
        r[0] = D_00878050[side].unk20[0];
        r[1] = D_00878050[side].unk20[1];
        r[2] = D_00878050[side].unk20[2];
    }
    if (D_005F5E48 == 7) {
        D_0085A7D0[8].unk0[side][D_005F6B88].unk20[0] = r[0];
        D_0085A7D0[8].unk0[side][D_005F6B88].unk20[1] = r[1];
        D_0085A7D0[8].unk0[side][D_005F6B88].unk20[2] = r[2];
    }
}

void func_00365248(Fighter *f) {
    s32 side = f->unk1E;

    if (D_005F5E48 == 7) {
        D_0085A7D0[8].unk0[side][D_005F6B88].unk0 = f->unkC0;
        D_0085A7D0[8].unk0[side][D_005F6B88].unk4 = f->unkB888;
        D_0085A7D0[8].unk0[side][D_005F6B88].unk8 = f->unkB88C;
        D_0085A7D0[8].unk0[side][D_005F6B88].unkC = f->unkB890;
        D_0085A7D0[8].unk0[side][D_005F6B88].unkD = f->unkB891;
        D_0085A7D0[8].unk0[side][D_005F6B88].unkE = f->unkC4;
        D_0085A7D0[8].unk0[side][D_005F6B88].unkF = f->unkB878;
        D_0085A7D0[8].unk0[side][D_005F6B88].unk10 = f->unkB5DC;
        D_0085A7D0[8].unk0[side][D_005F6B88].unk11 = f->unkB5E0;
        D_0085A7D0[8].unk0[side][D_005F6B88].unk34 = f->unk28;
    }
}

void func_00365300(Fighter *f) {
    s32 side = f->unk1E;

    if (D_005F5E48 == 3) {
        D_00878050[side].unk0 = f->unkC0;
        D_00878050[side].unk4 = f->unkB888;
        D_00878050[side].unk8 = f->unkB88C;
        D_00878050[side].unkC = f->unkB890;
        D_00878050[side].unkD = f->unkB891;
        D_00878050[side].unkE = f->unkC4;
        D_00878050[side].unkF = f->unkB878;
        D_00878050[side].unk10 = f->unkB5DC;
        D_00878050[side].unk11 = f->unkB5E0;
        D_00878050[side].unk34 = f->unk28;
    }
    if (D_005F5E48 == 1 || D_005F5E48 == 3 || D_005F5E48 == 6) {
        f->unkC0 = f->unkB8 = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk0;
        f->unkB888 = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk4;
        f->unkB88C = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk8;
        f->unkB890 = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unkC;
        f->unkB891 = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unkD;
        f->unkC4 = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unkE;
        f->unkB878 = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unkF;
        f->unkB5DC = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk10;
        f->unkB5E0 = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk11;
        if (f->unk28 != (s16)D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk34) {
            f->unk28 = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk34;
            func_00368168(f);
        }
    } else if (D_005F5E48 == 4) {
        f->unkC0 = f->unkB8 = D_00878050[side].unk0;
        f->unkB888 = D_00878050[side].unk4;
        f->unkB88C = D_00878050[side].unk8;
        f->unkB890 = D_00878050[side].unkC;
        f->unkB891 = D_00878050[side].unkD;
        f->unkC4 = D_00878050[side].unkE;
        f->unkB878 = D_00878050[side].unkF;
        f->unkB5DC = D_00878050[side].unk10;
        f->unkB5E0 = D_00878050[side].unk11;
        f->unk28 = D_00878050[side].unk34;
    }
}

s32 func_00365520(Fighter *f, s32 v) {
    s32 side = f->unk1E;

    if (D_005F5E48 == 3) {
        D_00878050[side].unk26 = v;
    }
    if (D_005F5E48 == 1 || D_005F5E48 == 3 || D_005F5E48 == 6) {
        v = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk26;
    } else if (D_005F5E48 == 4) {
        v = D_00878050[side].unk26;
    } else if (D_005F5E48 == 7) {
        D_0085A7D0[8].unk0[side][D_005F6B88].unk26 = v;
    }
    return v;
}

s32 func_00365610(Fighter *f, s32 v, s32 i) {
    s32 side = f->unk1E;

    if (D_005F5E48 == 3) {
        D_00878050[side].unk28[i] = v;
    }
    if (D_005F5E48 == 1 || D_005F5E48 == 3 || D_005F5E48 == 6) {
        v = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk28[i];
    } else if (D_005F5E48 == 4) {
        v = D_00878050[side].unk28[i];
    } else if (D_005F5E48 == 7) {
        D_0085A7D0[8].unk0[side][D_005F6B88].unk28[i] = v;
    }
    return v;
}

void func_00365718(Fighter *f) {
    s32 side = f->unk1E;
    s32 i;

    for (i = 0; i < 2; i++) {
        if (D_005F5E48 == 3) {
            D_00878050[side].unk2C[i] = FSB(f)->unkB074[i];
            D_00878050[side].unk2E[i] = FSB(f)->unkB06C[i];
            D_00878050[side].unk2A[i] = (s32)(FSB(f)->unkB07C[i] * 128.0f);
        }
        if (D_005F5E48 == 1 || D_005F5E48 == 3 || D_005F5E48 == 6) {
            FSB(f)->unkB074[i] = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk2C[i];
            FSB(f)->unkB06C[i] = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk2E[i];
            FSB(f)->unkB07C[i] = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk2A[i] * 0.0078125f;
        } else if (D_005F5E48 == 4) {
            FSB(f)->unkB074[i] = D_00878050[side].unk2C[i];
            FSB(f)->unkB06C[i] = D_00878050[side].unk2E[i];
            FSB(f)->unkB07C[i] = D_00878050[side].unk2A[i] * 0.0078125f;
        } else if (D_005F5E48 == 7) {
            D_0085A7D0[8].unk0[side][D_005F6B88].unk2C[i] = FSB(f)->unkB074[i];
            D_0085A7D0[8].unk0[side][D_005F6B88].unk2E[i] = FSB(f)->unkB06C[i];
            D_0085A7D0[8].unk0[side][D_005F6B88].unk2A[i] = (s32)(FSB(f)->unkB07C[i] * 128.0f);
        }
    }
}

s32 func_003659B0(Fighter *f, s32 v) {
    s32 side = f->unk1E;

    if (D_005F5E48 == 3) {
        D_00878050[side].unk33 = v / 2;
    }
    if (D_005F5E48 == 1 || D_005F5E48 == 3 || D_005F5E48 == 6) {
        v = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk33 * 2;
    } else if (D_005F5E48 == 4) {
        v = D_00878050[side].unk33 * 2;
    } else if (D_005F5E48 == 7) {
        D_0085A7D0[8].unk0[side][D_005F6B88].unk33 = v / 2;
    }
    return v;
}

s32 func_00365AC0(Fighter *f, s32 v) {
    s32 side = f->unk1E;

    if (D_005F5E48 == 3) {
        D_00878050[side].unk32 = v;
    }
    if (D_005F5E48 == 1 || D_005F5E48 == 3 || D_005F5E48 == 6) {
        v = D_0085A7D0[D_005F6B80].unk0[side][D_005F6B8C >> 1].unk32;
    } else if (D_005F5E48 == 4) {
        v = D_00878050[side].unk32;
    } else if (D_005F5E48 == 7) {
        D_0085A7D0[8].unk0[side][D_005F6B88].unk32 = v;
    }
    return v;
}

/* Records a voice for the frame being recorded. */
void func_00365BB0(u16 id, u16 player) {
    s32 n;

    if (D_005F5E48 == 7) {
        n = D_005F6B88 + D_005F6B94;
        func_00364568(&n);
        D_008780C0[8][n].unk0 = player;
        D_008780C0[8][n].unk2 = id;
        D_005F6B94++;
    }
}

void func_00365C30(void) {
    s32 n;

    if (D_005F6B94 > 0) {
        D_005F6B94--;
    }
    n = D_005F6B88 + D_005F6B94;
    func_00364568(&n);
    D_008780C0[8][n].unk2 = 0;
}

s32 func_00365C80(void) {
    if (!(D_005F6B8C & 1) && D_008780C0[D_005F6B80][D_005F6B8C >> 1].unk2 != 0) {
        if (D_005F5E48 == 6) {
            return func_00374E00(D_008780C0[D_005F6B80][D_005F6B8C >> 1].unk2, D_008780C0[D_005F6B80][D_005F6B8C >> 1].unk0);
        }
        return func_00374D90(D_008780C0[D_005F6B80][D_005F6B8C >> 1].unk2, D_008780C0[D_005F6B80][D_005F6B8C >> 1].unk0);
    }
}

s32 func_00365D00(void) {
    return (D_005F6B8C ^ 1) & 1;
}

/* Starts the replay of saved slot `slot` (returns 0 when the slot is empty). */
/* NON_MATCHING: 57 lines: register allocation in the set-up loops and the base of the gFighters[1] hit-contact copy (gFighters + 0x8000 in the original) */
#if 0
s32 func_00365D10(s32 slot, s32 *last) {
    s32 i;
    s32 k;
    s32 n;
    Fighter *f;
    s16 chr;

    D_005F5CB6 = D_0085A600[slot];
    if ((u16)D_0085A600[slot] == 0x17) {
        return 0;
    }
    func_003645D8(1);
    D_005F6BA8 = slot;
    func_003ADB20();
    func_003ADD60(1);
    for (i = 0; i < 2; i++) {
        gFighters[i].unk402 = 1;
        D_00440940[i].unk0 = D_0085A758[i][slot].unk0 >> 3;
        D_00440940[i].unk4 = D_0085A758[i][slot].unk0 & 7;
    }
    func_0036D010(0, 0);
    *last = D_005F6B88 = D_0085A628[slot] - 1;
    n = D_005F6B88 - 0x77;
    func_00364568(&n);
    HitLog_Load(slot, n);
    func_003D4638(slot, D_005F6B88 - 0x77);
    for (i = 0; i < 2; i++) {
        f = &gFighters[i];
        chr = f->unk22;
        func_003AFDA8(i, chr, (s32)func_00367310(chr));
        f->unk10 = 0;
        f->unk14 = 0;
    }
    func_0036D5D0();
    D_005F6B8C = (D_005F6B88 - 0x76) * 2;
    func_003645A0(&D_005F6B8C);
    gFighters[0].unkB4 = D_0085A730[slot][0];
    gFighters[1].unkB4 = D_0085A730[slot][1];
    D_005F6B80 = slot;
    D_005F5E48 = 6;
    D_005F6B9C = 1;
    func_00373380(&gFighters[0]);
    func_00373380(&gFighters[1]);
    for (i = 0; i < 2; i++) {
        for (k = 0; k < 3; k++) {
            FSB(&gFighters[i])->unk51C[k] = D_0085A7D0[D_005F6B80].unk0[i][D_005F6B8C >> 1].unk14[k];
        }
    }
    *(ReplayVec *)&gFighters[0].unk39C = D_0085A650[slot];
    *(ReplayVec *)&gFighters[1].unk39C = D_0085A650[slot];
    func_003D0C28(&gFighters[0], &gFighters[1], D_0085A708[slot], &D_0085A650[slot]);
    func_003D0660(&gFighters[0], &gFighters[1]);
    func_003D0C08(D_0085A6E0[slot]);
    func_003D0748(&gFighters[0], &gFighters[1]);
    D_005F6BA4 = 0;
    gRoundPhase = 3;
    func_00366E38();
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_00365D10);
#endif

void func_00366090(void) {
    D_005F6BA4 = 0;
}

void func_00366098(void) {
    D_005F5E48 = 7;
}

void func_003660A8(void) {
    D_005F5E48 = 5;
}

INCLUDE_RODATA("asm/nonmatchings/fighter_setup_b", D_004FC650);
INCLUDE_RODATA("asm/nonmatchings/fighter_setup_b", D_004FC668);
INCLUDE_RODATA("asm/nonmatchings/fighter_setup_b", D_004FC680);
s16 func_003660B8(s16 x) {
    s16 v = D_004FC680[abs(x / 4)];

    if (x < 0) {
        v = -v;
    }
    return v;
}

/* The angle (0-0xFFF) of the vector (dx, dz), from the arctangent table D_004FC680. */
s16 func_00366108(s32 dx, s32 dz) {
    s32 nz;

    if (dx == 0 && dz == 0) {
        return 0;
    }
    if (dx >= 0) {
        if (dz >= 0) {
            if (dx >= dz) {
                return D_004FC680[(dz << 10) / dx];
            }
            return 0x400 - D_004FC680[(dx << 10) / dz];
        }
        nz = -dz;
        if (dx >= nz) {
            return 0x1000 - D_004FC680[(nz << 10) / dx];
        }
        return D_004FC680[(dx << 10) / nz] + 0xC00;
    }
    if (dz >= 0) {
        dx = -dx;
        if (dx >= dz) {
            return 0x800 - D_004FC680[(dz << 10) / dx];
        }
        return D_004FC680[(dx << 10) / dz] + 0x400;
    }
    if (-dx >= -dz) {
        return D_004FC680[(dz << 10) / dx] + 0x800;
    }
    return 0xC00 - D_004FC680[(dx << 10) / dz];
}

s16 func_003662D8(s32 dx, s32 dz) {
    s32 ax = abs(dx);
    s32 az = abs(dz);

    while (ax > 0x1FFFFF || az > 0x1FFFFF) {
        ax >>= 1;
        az >>= 1;
    }
    return func_00366108(dx >= 0 ? ax : -ax, dz >= 0 ? az : -az);
}

INCLUDE_RODATA("asm/nonmatchings/fighter_setup_b", gSinTable);
INCLUDE_RODATA("asm/nonmatchings/fighter_setup_b", D_004FF690);
s32 func_00366360(s32 i) {
    s32 r;

    r = 0;
    if (D_004FF690[i] >= 0) {
        r = 1;
    }
    return r;
}

void func_00366380(Fighter *f) {
    if ((u16)f->unk26 < 0x110) {
        f->unk2E = D_004FF690[f->unk26];
    } else {
        f->unk2E = -1;
    }
}

/* NON_MATCHING: 7 lines: the compiler peels the first loop step (i known to be 1); the original increments i from 0 at run time */
#if 0
void func_003663C0(Fighter *f, s32 id) {
    s32 i = 0;

    f->unk26 = 0;
    if (id != D_004FF690[0]) {
        i++;
        while (i < 0x110) {
            if (id == D_004FF690[i]) {
                f->unk26 = i;
                break;
            }
            i++;
        }
    }
    func_00366380(f);
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_003663C0);
#endif

s16 func_00366410(s32 id) {
    s32 r = 0;
    s32 i = 0;

    if (id != D_004FF690[i]) {
        while (++i < 0x110) {
            if (id == D_004FF690[i]) {
                r = i;
                break;
            }
        }
    }
    return r;
}

s32 func_00366460(s32 n) {
    s32 i;
    s32 end;
    s32 c;
    s16 v;

    if (n >= 0x3B) {
    } else if (n >= 0x38) {
        return 8;
    }
    c = 0;
    for (i = n * 8, end = i + 8; i < end; i++) {
        v = D_004FF690[i];
        if (v != -1 && v != 0x77 && v != 0x45 && v != 0x78) {
            c++;
        }
    }
    return c;
}

s32 func_003664E0(Fighter *f, s32 i) {
    s32 n = f->unk2E;

    if ((u32)n < 0x86) {
        n = n * 2 - 1;
        return D_00416A90[i + n];
    }
    return 0;
}

s32 func_00366518(Fighter *f) {
    s32 n = f->unk2E;

    if ((u32)n < 0x86) {
        return D_00416C28[n];
    }
    return 0;
}

s32 func_00366540(Fighter *f) {
    s32 n = f->unk2E;

    if ((u32)n < 0x86) {
        return D_00416BA0[n];
    }
    return 0;
}

s32 func_00366568(Fighter *f) {
    s32 n = f->unk2E;

    if ((u32)n < 0x86) {
        return D_00416CB0[n];
    }
    return 0;
}

s32 func_00366590(Fighter *f) {
    s32 n = f->unk2E;

    if ((u32)n < 0x86) {
        return D_00416D38[n];
    }
    return 0;
}

s32 func_003665B8(Fighter *f) {
    s32 n = f->unk2E;

    if ((u32)n < 0x86) {
        return D_00416DC0[n];
    }
    return 0;
}

/* Draws image `img` (a GS image record: address, width, format at 4 / 8 / 0xC, rectangle at 0x10-0x1C) at (x, y)
 * through its CLUT record `clut`, with grey level `c`. */
/* NON_MATCHING: 74 lines: the three GS constant terms of TEX0 are hoisted in t0-t2 in the original, folded here; register allocation of the parameters */
#if 0
void func_003665E0(s32 x, s32 y, FsbImage *img, FsbImage *clut, s32 c) {
    u64 *p = func_003994C0();

    if (p == NULL) {
        return;
    }
    *p++ = 0x8001 | (0x8400UL << 48);
    *p++ = 0xE5353710;
    *p++ = 0x316;
    *p++ = ((u64)c | (0x8000UL << 16)) | (((u64)c << 16) | ((u64)c << 8));
    *p++ = ((u64)img->unk4 | (0x8000UL << 46)) | ((u64)img->unkC << 20 | (0xA000UL << 18)) |
           (((u64)(u32)(clut->unk4 + clut->unk10) << 37 | (u64)img->unk8 << 14) | ((u64)clut->unkC << 51 | (0x8500UL << 19)));
    *p++ = (u32)((img->unk10 << 4) | 8) | ((u64)(u32)((img->unk14 << 4) | 8) << 16);
    *p++ = (((x << 4) + 0x6C00) | ((u64)((y << 4) + 0x7200) << 16)) | ((u64)((s64)D_005F6730 >> 4) << 32);
    *p++ = (u32)(((img->unk10 + img->unk18 - 1) << 4) | 8) | ((u64)(u32)(((img->unk14 + img->unk1C - 1) << 4) | 8) << 16);
    *p++ = ((u32)((x + img->unk18 + 0x6C0) << 4) | ((u64)(u32)((y + img->unk1C + 0x720) << 4) << 16)) | ((u64)((s64)D_005F6730 >> 4) << 32);
    *p++ = 0;
    func_00399580(p);
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_003665E0);
#endif

/* Size in qwords of a GS image packet: its header and the image data. */
s32 func_00366820(u64 *pk) {
    s32 n = (pk[0] & 0x7FFF) * (pk[0] >> 60) + 1;
    s32 m = (s32)(pk[n * 2] & 0x7FFF) + 1;

    return n + m;
}

u64 *func_00366880(u64 *pk, FsbImage *img) {
    s32 n = func_00366820(pk);
    u64 *q;

    if (img->unk0 != 0) {
        pk[2] = (u64)img->unkC << 24 | ((u64)img->unkC << 56 | ((u64)img->unk8 << 48 | (u64)img->unk4 << 32));
        img->unk10 = (pk[4] >> 32) & 0xFFF;
        img->unk14 = (pk[4] >> 48) & 0xFFF;
        img->unk18 = pk[6] & 0xFFF;
        img->unk1C = (pk[6] >> 32) & 0xFFF;
    } else {
        q = pk + 6;
        img->unk10 = (pk[4] >> 32) & 0xFFF;
        img->unk14 = (pk[4] >> 48) & 0xFFF;
        pk[4] = 0;
        img->unk18 = pk[6] & 0xFFF;
        img->unk1C = (pk[6] >> 32) & 0xFFF;
        if (img->unk18 == 0x10) {
            img->unk10 *= 4;
        } else if (img->unk18 != 8) {
            exit(1);
        }
        q -= 4;
        *q |= (u64)(u32)(img->unk4 + img->unk10) << 32;
        *q |= (u64)img->unk8 << 48;
        *q |= (u64)img->unkC << 56;
    }
    Sprite_SendImagePacket(pk, n);
    return pk + n * 2;
}

void func_00366A20(u64 *buf, s32 size) {
    s32 i;

    if (Lz_Decompress(D_00416E50, (u8 *)buf) >= size) {
        exit(0);
    }
    for (i = 0; i < 4; i++) {
        buf = func_00366880(buf, &D_004176E0[i]);
    }
    Sprite_UploadBank(10, buf, 0x80, 0x500);
}

void func_00366AB8(f32 a, f32 b) {
    FsbImage *img = D_004176E0;

    Sprite_SetFilter(1, 0);
    Sprite_SetBlend(1, 1);
    func_003665E0(0xF9, 0xB4, &img[0], &img[1], a * 128.0f);
    func_003665E0(0xD0, 0xC8, &img[2], &img[3], b * 128.0f);
    Sprite_SetFilter(1, 1);
}

void func_00366B68(FsbNode *parent, FsbNode *n) {
    n->unk4 = D_00417760;
    n->unk48 = parent;
    n->unk0 = 0;
    if ((u32)parent >= 2) {
        parent->unk4C = n;
    }
}

void func_00366BD0(s32 angle, s32 r, s32 *x, s32 *z) {
    angle = -angle & 0xFFF;
    *x = r * gSinTable[angle] >> 12;
    *z = r * gSinTable[angle + 0x400] >> 12;
}

/* NON_MATCHING: 9 lines: register allocation of the last (negative) division: the original holds the angle in a0 and 34 in a1 */
#if 0
s32 func_00366C18(void) {
    s32 a;
    s16 d;
    s16 t;
    s32 q;
    s32 p;

    D_005F6BB4 = D_005F6BBC;
    D_005F6BB8 = D_005F6BC0;
    a = func_003A0180(D_005F6BBC - D_00415808.unk14, D_005F6BC0 - D_00415808.unk1C);
    D_005F6BAC = D_005F6BB0 & 0xFFF;
    D_005F6BB0 = D_00415808.unk4 & 0xFFF;
    func_00366BD0(D_005F6BB0, 0x1000, &D_005F6BBC, &D_005F6BC0);
    D_005F6BBC += D_00415808.unk14;
    D_005F6BC0 += D_00415808.unk1C;
    if (func_00393810() != 0) {
        return 0;
    }
    if (func_0036E158() == 0 || D_00417780[D_005F6BC4] == 0) {
        return 0;
    }
    p = (s16)(D_005F6BAC << 4);
    t = (s16)(D_005F6BB0 << 4);
    d = p - t;
    if (d < 0) {
        d = -d;
    }
    if (d >= 0x11) {
        return 0;
    }
    p = (s16)(a << 4);
    d = p - t;
    if (d >= 0) {
        q = d / 34;
        if (q >= 0xAB) {
            q = 0xAA;
        }
        return q & 0xFFF;
    }
    q = (((D_005F6BB0 - a) << 4) & 0xFFFF) / 34;
    return -(q < 0xAB ? q : 0xAA) & 0xFFF;
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_00366C18);
#endif

void func_00366DB0(s32 n) {
    D_005F6BC4 = n;
    D_005F5E78 = 0;
    Mtx_Unit(D_00879420);
}

void func_00366DC8(void) {
    FighterFsb *f = FSB(gFighters);

    D_005F5E78 = 0;
    D_008794A0[0] = f[0].unk51C[0];
    D_008794A0[1] = f[0].unk51C[1];
    D_008794A0[2] = -(f32)f[0].unk51C[2];
    D_008794A0[4] = f[1].unk51C[0];
    D_008794A0[5] = f[1].unk51C[1];
    D_008794A0[6] = -(f32)f[1].unk51C[2];
}

void func_00366E38(void) {
    FighterFsb *f = FSB(gFighters);

    D_005F5E78 = 0;
    D_008794A0[0] = f[0].unk51C[0];
    D_008794A0[1] = f[0].unk51C[1];
    D_008794A0[2] = -(f32)f[0].unk51C[2];
    D_008794A0[4] = f[1].unk51C[0];
    D_008794A0[5] = f[1].unk51C[1];
    D_008794A0[6] = -(f32)f[1].unk51C[2];
}

void func_00366EA8(void *m) {
    sceVu0CopyMatrix(m, D_00879420);
}

void func_00366EB8(void *m) {
    sceVu0CopyMatrix(m, D_00879460);
}

f32 func_00366EC8(void) {
    return D_005F5E78;
}

void func_00366ED0(void) {
    FsbVec a;
    FsbVec b;
    FsbVec c;
    FsbVec m[4];
    u128 mm[4];
    u128 m2[4];
    s32 t;
    f32 k;

    if ((gMode == 4 && func_00393810() == 0) || func_003ABD78() != 0) {
        if (func_003ABD78() == 0 && ((t = func_0036E158()) >= 6 || t < 3)) {
            {
                D_008794A0[3] = 1.0f;
                D_008794A0[7] = 1.0f;
                D_008794A0[0] = D_00433520.unk0;
                D_008794A0[1] = D_00433520.unk4;
                D_008794A0[2] = -(f32)D_00433520.unk8;
                D_008794A0[4] = D_00433520.unk10;
                D_008794A0[5] = D_00433520.unk14;
                D_008794A0[6] = -(f32)D_00433520.unk18;
            }
        }
        sceVu0ApplyMatrix(a.v, D_00433400, D_008794A0);
        sceVu0ApplyMatrix(b.v, D_00433400, &D_008794A0[4]);
        sceVu0InversMatrix(m, D_00433400);
        if (func_003ABD78() != 0) {
            c.v[1] = 0;
            c.v[3] = 1.0f;
            c.v[0] = (a.v[0] + b.v[0]) * 0.5f;
            c.v[2] = (a.v[2] + b.v[2]) * 0.5f;
        } else {
            c.v[0] = 0;
            c.v[3] = 1.0f;
            c.v[2] = (a.v[2] + b.v[2]) * 0.5f;
            c.v[1] = 0;
        }
        sceVu0ApplyMatrix(c.v, m, c.v);
        t = func_00366C18();
        k = 4095.0f;
        D_005F5E78 += 2.0f * (t / k) * 3.1415927f;
        D_005F5E78 = func_003D80E8(D_005F5E78);
        Mtx_Unit(m);
        sceVu0RotMatrixY(m, m, D_005F5E78);
        m[3].v[0] = c.v[0];
        m[3].v[2] = c.v[2];
        m[3].v[1] = 0;
        sceVu0CopyMatrix(D_00879460, m);
        sceVu0MulMatrix(mm, D_00433480, D_00433400);
        sceVu0MulMatrix(D_00879420, mm, m);
    } else if (func_003BB4D8() != 0) {
        Mtx_Unit(D_00879460);
        sceVu0RotMatrixY(D_00879460, D_00879460, 3.1415927f);
        sceVu0MulMatrix(m2, D_00433480, D_00433400);
        sceVu0MulMatrix(D_00879420, m2, D_00879460);
    } else {
        Mtx_Unit(D_00879460);
        sceVu0MulMatrix(D_00879420, D_00433480, D_00433400);
    }
}

void func_003671D0(f32 a) {
    D_005F5E78 = a;
    Mtx_Unit(D_00879460);
    sceVu0RotMatrixY(D_00879460, D_00879460, a);
    sceVu0MulMatrix(D_00879420, D_00433480, D_00433400);
    sceVu0MulMatrix(D_00879420, D_00879420, D_00879460);
}

void *func_00367250(s32 i) {
    return D_008794C0[i].unk0;
}

void *func_00367270(s32 i) {
    return D_008794C0[i].unk4;
}

void *func_00367290(s32 i) {
    return D_008794C0[i].unk8;
}

void *func_003672B0(s32 i) {
    return D_008794C0[i].unkC;
}

void *func_003672D0(s32 i) {
    return D_008794C0[i].unk10;
}

void *func_003672F0(s32 i) {
    return D_008794C0[i].unk14;
}

void *func_00367310(s32 i) {
    return D_008794C0[i].unk18;
}

void *func_00367330(s32 i) {
    return D_008794C0[i].unk1C;
}

void *func_00367350(s32 i) {
    return D_008794C0[i].unk20;
}

void *func_00367370(s32 i) {
    return D_008794C0[i].unk24;
}

void *func_00367390(s32 i) {
    return D_008794C0[i].unk28;
}

void func_003673B0(Fighter *f) {
    void **p = (void **)&D_008794C0[f->unk22];
    u32 i;

    for (i = 0; i < 11; i++) {
        p[i] = NULL;
    }
}

void func_003673F8(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        func_003673B0(&gFighters[i]);
    }
}

void func_00367450(Fighter *f) {
    func_00367488(f, 0);
    File_WaitDone(1);
    func_00367600(f);
}

void func_00367488(Fighter *f, s32 flags) {
    s32 slot = f->unk22;
    s32 chr = f->unk2E;
    s32 base;
    s32 size;

    func_003673B0(f);
    base = func_003B87E0(f->unk22);
    size = func_0035CED8(chr * 3 + 0x158);
    if (size != 0 && func_003ABCC0() == 0 && gMode != 0xB) {
        D_004177E0[slot].unk0[0].unk8 = File_Request(chr * 3 + 0x158, Snd_GetBankAddr(slot), flags, 1);
        D_004177E0[slot].unk0[0].unk4 = size;
    } else {
        D_004177E0[slot].unk0[0].unk8 = -1;
        D_004177E0[slot].unk0[0].unk4 = -1;
    }
    D_004177E0[slot].unk0[1].unk0 = base;
    D_004177E0[slot].unk0[1].unk8 = File_Request(chr * 3 + 0x159, base, flags, 0);
    func_0035CEF8(chr);
    if (flags != 0) {
        D_004177E0[slot].unk0[2].unk8 = File_Request(0xFFF, 0, flags, 0);
    } else {
        D_004177E0[slot].unk0[2].unk8 = -1;
    }
}

/* NON_MATCHING: 11 lines: register allocation only: the loop index and the bank address swapped in s0 / s1 */
#if 0
void func_00367600(Fighter *f) {
    s32 slot = f->unk22;
    FsbModels *m = &D_008794C0[slot];
    s32 base;
    u32 i;
    s32 k;
    s32 *p;
    s32 *q;

    FlushCache(0);
    func_003673B0(f);
    if (gMode != 0xB && func_003ABCC0() == 0) {
        Snd_SetBankOwner(slot, f->unk2C);
    }
    if (func_0035CEF8(f->unk2E) > 0) {
        base = func_003B87E0(slot);
        for (i = 0; i < 11; i++) {
            if (func_00364558(i, (void *)base) != 0) {
                ((void **)m)[i] = func_00364528(i, (void *)base);
            }
        }
        if (m->unk28 == NULL) {
            m->unk28 = (void *)(base + ((func_0035CEF8(f->unk2E) + 0xF) & 0xFFFFFFF0));
            func_0034E130(m->unk28);
        }
    }
    if (m->unk0 != NULL) {
        func_00344EA8(f);
    }
    if (m->unk8 != NULL && func_003BB4D8() == 0) {
        func_003DC870(f->unk22);
    }
    if (func_003BB4D8() == 0) {
        func_0037D0F8(func_003672B0(slot), slot);
    }
    if (m->unk10 != NULL) {
        func_00363150(f);
    }
    if (m->unk14 != NULL) {
        func_00362B18(f);
    }
    if (m->unk18 != NULL && m->unk1C != NULL) {
        p = (s32 *)func_003B87B0(slot);
        q = (s32 *)func_003B87C8(slot);
        *p = 0;
        *q = 0;
        func_003AF540(f, (s32)m->unk1C, (s32)m->unk18);
        Fx_LoadFighterTex(f);
    }
    for (k = 0; k < 3; k++) {
        if (D_004177E0[slot].unk0[k].unk8 >= 0) {
            File_Release(D_004177E0[slot].unk0[k].unk8);
            D_004177E0[slot].unk0[k].unk8 = -1;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_00367600);
#endif

s32 func_00367850(void) {
    return D_005F6BCC;
}

void *func_00367858(void) {
    return D_008795E0[0];
}

void *func_00367868(void) {
    return D_008795E0[1];
}

void *func_00367878(void) {
    return D_008795E0[2];
}

void *func_00367888(void) {
    return D_008795E0[3];
}

void *func_00367898(void) {
    return D_008795E0[4];
}

void *func_003678A8(void) {
    return D_008795E0[5];
}

void *func_003678B8(void) {
    return D_008795E0[6];
}

s32 func_003678C8(void) {
    return D_005F6BC8;
}

void func_003678D0(void) {
    u32 i;

    for (i = 0; i < 5; i++) {
        D_008795C8[i] = NULL;
    }
    for (i = 0; i < 7; i++) {
        D_008795E0[i] = NULL;
    }
    D_005F6BC8 = 0;
}

void func_00367930(void) {
    func_003678D0();
}

/* NON_MATCHING: 96 lines: stack frame (0xF0 against 0xD0: the original has more stack locals) and register allocation of the bank pointers */
#if 0
void func_00367938(s32 stage) {
    s32 sizes[4];
    s32 blocks[5];
    u8 *bank;
    u8 *bank2;
    u32 dst;
    u32 dst2;
    u32 i;
    s32 sum;
    s32 k;

    func_003DB340();
    func_003D0CC8(stage);
    func_003678D0();
    func_00363888();
    bank = func_003639E8();
    bank2 = (u8 *)func_003B8788();
    dst = (u32)func_003639E8();
    dst2 = func_003B8788();
    if (func_003ABCC0() == 0) {
        func_0035D348(stage);
    }
    func_0035CF78(dst, stage, 0x177000);
    dst2 += (func_0035CF20(dst2, stage, 0x2F08F0) + 0xF) & 0xFFFFFF0;
    for (i = 0; i < 5; i++) {
        if (func_00364558(i, bank) != 0) {
            D_008795C8[i] = func_00364528(i, bank);
        }
    }
    for (i = 0; i < 7; i++) {
        if (func_00364558(i, bank2) != 0) {
            D_008795E0[i] = func_00364528(i, bank2);
        }
    }
    if (D_005F5CB6 == 2 || D_005F5CB6 == 0x13 || D_005F5CB6 == 0x16) {
        func_003E7210(dst2);
        D_005F6BC8 = dst2;
    }
    for (k = 0; k < 5; k++) {
        blocks[k] = 0;
    }
    func_00362328();
    func_00361D50(0, func_00354368(D_008795C8[2]), D_008795C8[2]);
    func_00361D50(3, func_00354368(D_008795C8[1]), D_008795C8[1]);
    func_00361D50(1, func_00354368(D_008795C8[3]), D_008795C8[3]);
    func_00361D50(2, func_00354368(D_008795C8[4]), D_008795C8[4]);
    sum = 0;
    if (D_008795C8[0] != NULL) {
        for (k = 0; k < func_00364550(D_008795C8[0]); k++) {
            if (func_00364558(k, D_008795C8[0]) != 0) {
                sum += func_00354368(func_00364528(k, D_008795C8[0]));
            }
        }
        func_00361D50(4, sum, D_008795C8[0]);
    }
    func_00361EA8(blocks, sizes);
    for (i = 0; i < 5; i++) {
        D_008795C8[i] = NULL;
    }
    func_003638F0();
    func_00345070();
    if (D_008795E0[3] != NULL) {
        func_003D0C68((s32)D_008795E0[3], blocks[3]);
        func_003D0E18();
    }
    if (D_008795E0[4] != NULL) {
        func_00356E20(stage, D_008795E0[4], blocks[0]);
    }
    Fx_LoadStageLayers(blocks[0]);
    D_005F6BCC = blocks[0];
    if (D_008795E0[5] != NULL) {
        func_00357160(stage, 0, D_008795E0[5], blocks[1]);
    }
    if (D_008795E0[6] != NULL) {
        func_00357160(stage, 1, D_008795E0[6], blocks[2]);
    }
    if (D_008795E0[0] != NULL) {
        func_003DB368(stage, D_008795E0[0], sizes);
        func_003DB578();
    }
    func_00342360(stage);
}
#else
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_00367938);
#endif

void func_00367CA0(s16 *p) {
    s16 n = p[0];

    D_005F5E7C = (s32 *)p;
    D_005F5E80 = n / 8;
}

s32 func_00367CC8(void) {
    return D_005F5E80;
}

u16 func_00367CD0(s32 i) {
    s32 n = D_005F5E80 - 1;
    s32 *p = D_005F5E7C;

    if (i < D_005F5E80) {
        n = i;
    }
    return *(u16 *)((u8 *)p + p[n * 2]);
}

void *func_00367D00(s32 i) {
    s32 n = D_005F5E80 - 1;
    s32 *p = D_005F5E7C;

    if (i < D_005F5E80) {
        n = i;
    }
    return (u8 *)p + p[n * 2];
}

/* Decodes frame `frame` of a packed channel block into n values: frame 0 / the last frame are stored, the others are
 * deltas from the previous key in 4-bit codes (with repeat counts) read from 16-bit words. */
/* NON_MATCHING: 57 lines: register allocation only (decoder state in t5-t9, n in s1 in the original) */
#if 0
void func_00367D28(u16 *data, s16 *out, s32 frame, s32 n) {
    s32 bits = 0;
    s32 base = 0;
    s32 code = 0;
    s32 mask = 0;
    s32 i;
    s32 k;
    s32 cnt;
    s32 nb;
    s32 w;
    s32 shift;
    s32 rep;
    s16 v;
    u16 *p;
    u16 *next;
    s16 *q = out;

    if (frame + 1 >= data[0]) {
        p = data + 0x3A;
        for (i = 0; i < n; i++) {
            *q++ = *p++;
        }
        return;
    }
    p = data + 1;
    for (i = 0; i < n; i++) {
        *q++ = *p++;
    }
    if (frame == 0) {
        return;
    }
    q = out;
    cnt = ((frame - 1) & 0xF) + 1;
    p = data + data[0x73 + (frame - 1) / 16];
    nb = 0;
    w = 0;
    for (i = 0; i < n; i++) {
        next = p;
        if (nb < 4) {
            w = *p++;
            nb = 0x10;
        }
        shift = (u32)(i - 1) <= 1 ? 2 : 4;
        next += w & 0xF;
        w >>= 4;
        nb -= 4;
        v = *q;
        rep = 0;
        for (k = 0; k < cnt; k++) {
            if (nb < 0x10) {
                w += *p++ << nb;
                nb += 0x10;
            }
            if (rep == 0) {
                code = w & 0xF;
                nb -= 4;
                w >>= 4;
                if (code >= 8) {
                    nb -= 4;
                    rep = (w & 0xF) + 1;
                    w >>= 4;
                    code -= 8;
                }
            } else {
                rep--;
            }
            switch (code) {
            case 0:
                continue;
            case 1:
                base = -0x54;
                goto b6;
            case 2:
                base = -0x14;
                goto b4;
            case 3:
                base = -4;
                goto b2;
            case 4:
                base = 1;
            b2:
                bits = 2;
                mask = 3;
                break;
            case 5:
                base = 5;
            b4:
                bits = 4;
                mask = 0xF;
                break;
            case 6:
                base = 0x15;
            b6:
                bits = 6;
                mask = 0x3F;
                break;
            case 7:
                if (nb < 0xC) {
                    w += *p++ << nb;
                    nb += 0x10;
                }
                nb -= 0xC;
                v = (w & 0xFFF) << shift;
                w >>= 12;
                continue;
            }
            nb -= bits;
            v += (base + (w & mask)) << shift;
            w >>= bits;
        }
        *q++ = v;
        p = next;
        nb = 0;
        w = 0;
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/fighter_setup_b", jtbl_004FF8B0);
INCLUDE_ASM("asm/nonmatchings/fighter_setup_b", func_00367D28);
#endif

void func_00367FE0(u16 *data, s16 *out, s32 frame) {
    func_00367D28(data, out, frame, 3);
}

void func_00367FE8(u16 *data, s16 *out, s32 frame) {
    func_00367D28(data, out, frame, 0x39);
}

/* Relocates the move records D_00531EA0 once: their motion offsets, command lists, damage changes and lists. */
void func_00367FF0(void) {
    u8 *bank2 = func_003B8760();
    u8 *bank = func_003B8770();
    struct FighterMove *moves;
    struct FighterMove *m;
    u8 *cmds;
    s32 i;
    s32 v;

    if (D_005F5E84 == 0) {
        for (i = 0; i < 0x1CB7; i++) {
            moves = D_00531EA0;
            m = &moves[i];
            v = ((s32 *)bank)[m->unk0 * 2];
            if (v & 0x4000000) {
                v = v + (s32)bank2 - 0x4000000;
            } else {
                v += (s32)bank;
            }
            m->unk0 = v;
            cmds = D_00482248;
            m->unk18 = *(u8 *)v;
            m->unkC = cmds + (s32)m->unkC * 0xE;
            if ((s32)m->unk20 == -1) {
                m->unk20 = NULL;
            } else {
                m->unk20 = &D_00524690[(s32)m->unk20];
                func_003682F8(m);
            }
            if ((s32)m->unk1C == -1) {
                m->unk1C = NULL;
            } else {
                m->unk1C = &D_0052C448[(s32)m->unk1C];
            }
        }
        D_005F5E84 = 1;
    }
}

void func_00368140(Fighter *a, Fighter *b) {
    func_00368168(a);
    func_00368168(b);
}

void func_00368168(Fighter *f) {
    s32 i;
    struct FighterMove **dst = D_00417900[(s16)f->unk20];
    u16 *ids = D_0058F3D0[f->unk28];

    for (i = 0; i < 0x159C; i++) {
        *dst++ = &D_00531EA0[*ids++];
    }
    D_00879600[(s16)f->unk20] = D_00417900[(s16)f->unk20];
}

struct FighterMove **func_00368200(s32 side) {
    return D_00879600[side];
}

struct FighterMove *func_00368218(Fighter *f, s16 move) {
    return D_00879600[(s16)f->unk20][move];
}

struct FighterMove *func_00368248(s32 id) {
    return &D_00531EA0[id];
}

u16 func_00368260(s32 chr, s32 move) {
    return D_0058F3D0[chr][move];
}

s16 func_00368288(s16 n) {
    return n > 0x20 ? 0x20 : n;
}

s32 func_003682A8(s32 a, s16 b) {
    s16 *p = &D_005E8608[b];
    s32 r = 0;
    s16 id;

    while ((id = *p) != -1) {
        p++;
        if (a == D_00531EA0[id].unk0) {
            r = 1;
            break;
        }
    }
    return r;
}
