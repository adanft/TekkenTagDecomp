/* 0x3ADFA8-0x3B1478 (module menu_common, docs/game_overview.md section 4; docs/systems/modes.md). .rodata
 * 0x51DE30-0x51DEF0 is its own (subsegment, included in place); both edges are [I]. */
#include "common.h"

#include "fighter.h"

/* The camera block D_00415808: three angles, then the position (ints). */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
} McCam;

/* Rotation in 1/0x10000 turns (ints) and a translation (floats), func_003ADFF0's input. */
typedef struct {
    /* 0x00 */ s32 rot[3];
    /* 0x0C */ s32 padC;
    /* 0x10 */ f32 pos[3];
    /* 0x1C */ f32 w;
} McView;

typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s16 unk8;
    /* 0xA */ s16 unkA;
} McSideMove;

/* Per-port pad settings (D_008D7E68, 4 x 8 bytes). */
typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 unk6;
} McPort;

typedef struct {
    s16 s[3];
} McTriple;

/* A 0x50-byte record of the fighter at +0x630 (func_003AF9D8). */
typedef struct {
    /* 0x00 */ u8 pad0[0x18];
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ u8 pad24[0x2C];
} McPart;

/* A joint record of a model file (func_003AFE00): parent index, two vectors. */
typedef struct {
    /* 0x00 */ s32 parent;
    /* 0x04 */ f32 a[3];
    /* 0x10 */ f32 b[3];
} McJointSrc;

typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 count;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ McJointSrc *joint[1];
} McJointHdr;

typedef struct {
    /* 0x00 */ u8 pad0[0xB0];
    /* 0xB0 */ void *parent;
    /* 0xB4 */ u8 padB4[0xC];
} McJoint;

typedef struct {
    f32 v[3];
} McVec3;

/* The team battle game record (func_003AE440's argument). */
typedef struct {
    /* 0x00 */ u8 unk0[0x10];
    /* 0x10 */ u8 unk10[0xB];
    /* 0x1B */ u8 count;
    /* 0x1C */ u8 pad1C[2];
} McTeam;

typedef struct {
    /* 0x0 */ u8 pad0[6];
    /* 0x6 */ s16 side[2];
    /* 0xA */ s16 ch[2];
    /* 0xE */ u8 padE[2];
} McRound;

typedef struct {
    /* 0x00 */ u8 pad0[0x1F];
    /* 0x1F */ u8 unk1F;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u8 pad22[0x1E];
    /* 0x40 */ u8 unk40;
    /* 0x41 */ u8 pad41;
    /* 0x42 */ McTeam team[2];
    /* 0x7E */ u8 pad7E[2];
    /* 0x80 */ McRound round[1];
} McGame;

/* The fighter slot seen from func_003AF9D8 (the parts at +0x630, their pointers at +0x8730). */
typedef struct {
    /* 0x0000 */ u8 pad0[0x630];
    /* 0x0630 */ McPart part[412];
    /* 0x86F0 */ u8 pad86F0[0x40];
    /* 0x8730 */ McPart *slot[0x34];
    /* 0x8800 */ f32 jointPos[1][16];
} McFighter;

/* A loaded file handle per side and slot (D_004177E8, 4 x 3 x 0xC). */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 unk8;
} McFile;

typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s16 unk8;
    /* 0xA */ s16 padA;
} McRec;

typedef struct {
    /* 0x0 */ u8 pad0[8];
    /* 0x8 */ s16 unk8;
    /* 0xA */ u8 padA[6];
} McCharEntry;

typedef struct {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ s8 unk1;
    /* 0x2 */ u8 pad2[6];
} McSlot;

typedef struct {
    /* 0x00 */ u8 pad0[0x16];
    /* 0x16 */ u16 unk16;
    /* 0x18 */ u8 pad18[0x24];
    /* 0x3C */ u8 unk3C[2][2];
} McSetup;

typedef struct {
    u8 a;
    u8 b;
} McPair;

typedef struct {
    /* 0x00 */ u8 pad0[0x38];
    /* 0x38 */ McPair unk38[2];
} McSave;

extern Fighter gFighters[4];
extern u8 D_0043B7F8[];
extern u8 D_00439C60[];
extern McCam D_00415808;
extern McSlot D_004157F8[];
extern McSideMove D_00437E30[];
extern const McCharEntry D_004FF690[];
extern s16 D_00437DA0[];
extern McFile D_004177E8[4][3];
extern s32 D_005F5CD4;
extern McPort D_008D7E68[];
extern McTriple D_008D7E88[];
extern McRec D_00440C28[][8];
extern s32 D_00440D04[];
extern void *D_008D7E50[];
extern s16 D_005F62A8[4];
extern s16 gMode;
extern s16 gSubMode;
s32 Pad_IsMultitap(void);
s32 Pad_CountReady(void);
extern s16 D_005F5CC8[4];
extern u16 D_005F5CB0[2];
extern u16 D_005F5CB4;
extern s16 D_005F5C80[4];
void func_003ADBB0(Fighter *f);
void func_003AF7D0(Fighter *f, s32 b);
void func_00362B18(Fighter *f);
s32 func_003B01B0(s32 i);
void func_003B0298(s32 i);
void func_003B01D0(s32 a, s32 port, s32 b);
void func_003AE738(s32 a, s32 i);
void func_003BA520(s32 side);
void func_003AFFE0(s32 a, s32 port);
s16 func_003B0078(s32 a, s32 port);
void func_003CB8E0(s32 a, s32 b);
extern u32 D_00415718[];
extern u32 D_00415728[];
extern s32 D_00439B20[];
extern u32 D_00415748[];
extern u32 D_00415758[];
s32 Pad_GetPlayerMode(s32 player);
void func_003BAA38(void);
void func_003ADB70(s32 side, s32 member, s32 n);
void func_003B00A8(s32 a, s32 port, s32 v);
void func_003B00D8(s32 port, s32 v);
s32 func_003B00F0(s32 port);
s32 func_003B04A0(void);
void func_003B03F8(s32 port);
void func_003B0380(s32 side);
McVec3 *func_003B87F8(s32 id);
void func_003AFAA0(Fighter *f, s32 a);
void *func_00368218(Fighter *f, s16 move);
void func_00363068(Fighter *f, s32 n);
void Skel_Update(Fighter *f);
void Fighter_RunMoveEvents(Fighter *f);
s32 func_0035A670(void);
void Snd_StartMoveVoices(Fighter *f);
void func_00376220(Fighter *f, Fighter *other);
void func_00375368(s32 index, s32 player);
void func_003AE400(s32 side, s32 member, s32 n);
void func_0035A1E8(McGame *g);
typedef struct {
    f32 v[4];
} __attribute__((aligned(8))) McV4;
void func_003625B0(s32 angle, f32 (*m)[4]);
void sceVu0RotMatrixX(f32 (*m0)[4], f32 (*m1)[4], f32 rx);
void sceVu0RotMatrixZ(f32 (*m0)[4], f32 (*m1)[4], f32 rz);
void sceVu0ApplyMatrix(f32 *out, f32 (*m)[4], f32 *v);
void sceVu0CopyMatrix(f32 (*m0)[4], f32 (*m1)[4]);
void sceVu0MulMatrix(f32 (*m0)[4], f32 (*m1)[4], f32 (*m2)[4]);
extern f32 D_00433400[4][4];
extern f32 D_00433440[4][4];
extern f32 D_004334C0[4][4];
extern TeamMember D_00440940[2][2];
extern u8 D_005F5CCC;
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 D_005F5F08;
extern s16 D_005F62AC;
extern s32 D_005F5EFC;
extern s32 D_005F5F00;
extern s32 D_005F5EF8;
extern s16 D_005F5CB6;
void func_003645D8(s32 a);
void func_003B6218(void);
void func_003AE6E0(void);
void Tag_ResetCounters(void);
void func_003B8C08(void);
void func_00353D58(void);
void func_00353DB0(void);
void func_00353658(void);
void func_00353E08(void);
void func_003533E8(s32 a);
void func_00363618(void);
void func_00363630(s32 a);
void func_003B8AA0(void);
void func_00373218(Fighter *f);
void func_00371B78(Fighter *f);
void func_00373320(void);
void func_00373DE0(void);
void func_003760E0(void);
void func_003753D0(void);
void func_00375190(void);
s32 File_Release(s32 handle);
void Mode_Change(s32 mode);
void Snd_PlaySe(s32 id, s32 player);
s32 func_003BDF38(void);
s32 func_003B7A38(void);
s32 func_003B1200(s32 port);
s32 func_003B0458(void);
s32 File_CountQueued(void);
void func_0035C8D0(Fighter *f, s32 character, s32 costume);
s32 func_00367310(s32 slot);
void func_003AF540(Fighter *f, s32 a, s32 b);
void func_003AFDA8(s32 side, s32 slot, s32 b);
void func_003AEEF0(s32 i);
void func_003AEDA0(s32 i);
void func_00375388(s32 player);
void func_00375488(s32 player);
void func_00366B68(void *a, void *b);
void func_0034E640(s32 j, Fighter *f);
extern s32 D_008D7E40[];
extern s16 *D_00439A80[];
extern s16 D_005F6D7E;
extern s16 D_005F6D80;
extern s16 D_005F6D82;

s32 func_0039C768(void);
void func_003ADFF0(McView *v);
void func_003AE170(McCam *c);
s32 func_003BFE58(void);
void func_003B0350(s16 v);
void func_0038DBC0(void);
s32 func_003AE608(s32 ch, s32 kind);
void func_003AFF40(s32 i);
void func_003B0120(s32 i);
void func_003AF9D8(Fighter *f, s32 a, s32 b);

/* func_003B87B0's model record: the part ids of each side at unk8 (see src/dma_gif.c). */
typedef struct {
    /* 0x00 */ u8 pad0[0x2C];
    /* 0x2C */ s32 part[2][2][5];
} McPartTbl;

typedef struct {
    /* 0x00 */ s32 unk0;                 /* set once the file below is relocated */
    /* 0x04 */ s32 unk4;                 /* joint count less 0x62 */
    /* 0x08 */ McPartTbl *unk8;          /* the model file */
    /* 0x0C */ void *unkC[1];            /* per joint: its 0x20-byte record in the file */
} McModel;

/* A model file as loaded (func_003AF540 relocates it in place). */
typedef struct {
    /* 0x00 */ u8 pad0[0x10];
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;                /* file offset, made a pointer */
    /* 0x18 */ u8 pad18[0x14];
    /* 0x2C */ s32 part[2][2][5];        /* file offsets of the parts, 0 = none */
    /* 0x7C */ u8 joint[1][0x20];
} McModelFile;

void sceGifPkInit(void *pk, void *base);
void sceGifPkEnd(void *pk, u32 a, u32 b, u32 c);
void sceGifPkTerminate(void *pk);
u32 sceGifPkSize(void *pk);
void FlushCache(s32 op);
u32 *func_003B8798(s32 id);
void *func_00367330(s32 id);
void *func_00367350(s32 id);
u32 func_00354290(void *pk, void *rec, u32 base);
void func_00343CC8(s32 part, u32 tbp);
void func_003D8498(Fighter *f, void *p);
extern const s8 D_0051DE50[];
extern s16 D_004396C0[];
extern s32 D_00437FF8[];
extern s32 D_00437E70[];
void func_0034FBA0(s32 side);

/* A start position (D_00437D40 / D_00437D80, 16 bytes per side). */
typedef struct {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 z;
    /* 0xC */ u16 dir;
    /* 0xE */ u16 padE;
} McStart;

extern McFile D_004177F8[][3];
extern McStart D_00437D40[];
extern McStart D_00437D80[];
extern s16 D_00437DE8[];
u32 func_003CBED8(s32 port);
void func_003A8848(u32 i);
void func_00367488(Fighter *f, s32 a);
s32 File_IsMarkerReached(s32 handle);
void func_00367600(Fighter *f);
void func_0035C850(Fighter *f, s32 n);
void func_00368168(Fighter *f);
void func_00373E18(Fighter *f);
void func_003AEA40(s32 side, s32 hold);
void func_003B0D50(s32 port, s32 side, s32 blink);

typedef struct {
    s32 x;
    s32 y;
} McXY;

/* Three cursor offsets (D_0051DE98). */
typedef struct {
    McXY p[3];
} McXY3;

extern const McXY3 D_0051DE98;
extern const char D_0051DEB0[];
extern const char D_0051DEC0[];
extern const char D_0051DED0[];
extern const char D_0051DEE0[];
extern s32 D_00439B10[];
extern s16 D_005F62B0;
s32 func_003AFF08(s32 i);
void func_003CCEA8(s32 x, s32 y, s32 port);
void func_003CCF40(s32 x0, s32 x1, s32 y, s32 port);
void exit(s32 code);
s32 Text_Printf(const char *fmt, ...);

typedef struct {
    /* 0x00 */ u8 pad0[8];
    /* 0x08 */ void *unk8;
} McModelEx;

McModel *func_003B87B0(s32 id);
void func_003AFE00(Fighter *f, s32 id, s32 a, McJoint *joints, McJointHdr *h);
McModelEx *func_003B87C8(s32 id);
void func_003568E0(s32 part, s32 a);
void func_00347760(Fighter *f);
void func_003564C8(Fighter *f, McModel *m);
void func_003D8490(void *p, s32 a);
void func_00351E10(Fighter *f);
void func_003519E0(Fighter *f, McModel *m);
void func_003504C0(Fighter *f);
void func_00353668(s32 i);
void func_00353678(s32 i);
extern s32 D_00437E60[];
extern s32 D_005F67C0;


void func_003ADFA8(void) {
    D_005F5CD4 = 0;
}

void func_003ADFB0(void) {
    D_005F5CD4 = 1;
}

s32 func_003ADFC0(void) {
    s32 r = 0;

    if (D_005F5CD4 != 0) {
        r = func_0039C768() == 0;
    }
    return r;
}

/* NON_MATCHING: 21 lines, the original keeps a second copy of the matrix pointer in s2 (frame 0xA0) for the Z rotation */
#if 0
void func_003ADFF0(McView *v) {
    McV4 p = { { -v->pos[0], -v->pos[1], -v->pos[2], 1.0f } };
    McV4 q;
    f32 m[4][4] __attribute__((aligned(16)));
    s32 a;
    f32 r;

    func_003625B0(-v->rot[1], m);
    a = (0x8000 - v->rot[0]) & 0xFFFF;
    if (a <= 0x7FFF) {
        r = 9.58738e-05f * (f32)a;
    } else {
        r = 9.58738e-05f * (f32)(a - 0x10000);
    }
    sceVu0RotMatrixX(m, m, r);
    a = -v->rot[2] & 0xFFFF;
    if (a <= 0x7FFF) {
        r = 9.58738e-05f * (f32)a;
    } else {
        r = 9.58738e-05f * (f32)(a - 0x10000);
    }
    sceVu0RotMatrixZ(m, m, r);
    sceVu0ApplyMatrix(q.v, m, p.v);
    m[3][0] = q.v[0];
    m[3][1] = q.v[1];
    m[3][2] = q.v[2];
    sceVu0CopyMatrix(D_00433400, m);
    sceVu0MulMatrix(D_004334C0, D_00433440, D_00433400);
}
#else
LIT4_WORD(D_005F5340, 0x38C90FDB);
LIT4_WORD(D_005F5344, 0x38C90FDB);
LIT4_WORD(D_005F5348, 0x38C90FDB);
LIT4_WORD(D_005F534C, 0x38C90FDB);
INCLUDE_ASM("asm/nonmatchings/menu_common", func_003ADFF0);
#endif

void func_003AE170(McCam *c) {
    McView v;

    v.pos[0] = (f32)c->unk14;
    v.pos[1] = -(f32)c->unk18;
    v.pos[2] = -(f32)c->unk1C;
    v.rot[0] = -(c->unk0 << 4);
    v.rot[1] = -(c->unk4 << 4);
    v.rot[2] = -(c->unk8 << 4);
    func_003ADFF0(&v);
}

void func_003AE1E8(void) {
    gFighters[0].unk0 = 0;
    gFighters[0].unk2 = 1;
    gFighters[1].unk0 = 0;
    gFighters[1].unk2 = 1;
}

void func_003AE210(u8 *g) {
    s32 i;
    s32 side;
    Fighter *f;

    f = gFighters;
    f[0].unk401 = -1;
    f[1].unk401 = -1;
    if (g[0x1A] == 1) {
        u8 x = D_005F5CCC;

        for (i = 0; i < 2; i++) {
            if (D_00440940[g[0x1D]][i].unk0 == 0x21) {
                if ((D_0043B7F8[0] == 3 || D_0043B7F8[0] == 0) && x == 7) {
                    gFighters[g[0x1D]].unk401 = 0;
                    gFighters[g[0x1D]].unk402 = 1;
                } else {
                    gFighters[g[0x1D]].unk401 = 1;
                }
            }
        }
        for (i = 0; i < 2; i++) {
            if (D_00440940[g[0x1C]][i].unk0 == 0x21) {
                gFighters[g[0x1C]].unk401 = 2;
            }
        }
        return;
    }
    for (i = 0; i < 2; i++) {
        for (side = 0; side < 2; side++) {
            if (D_00440940[i][side].unk0 == 0x21) {
                f->unk401 = 2;
            }
        }
        f++;
    }
}

s32 func_003AE3C0(Fighter *f) {
    if (f->unk2A != 0x21) {
        return -1;
    }
    return f->unk401;
}

void func_003AE3E0(s32 a) {
    gFighters[0].unk402 = a;
    gFighters[1].unk402 = a;
}

void func_003AE400(s32 side, s32 member, s32 n) {
    ((McSetup *)D_0043B7F8)->unk3C[side][member] = n;
}

s32 func_003AE420(s32 side, s32 member) {
    return ((McSetup *)D_0043B7F8)->unk3C[side][member];
}

void func_003AE440(McGame *g) {
    s32 i;
    s32 j;
    s32 n;
    s32 k = 0;
    s32 side;
    Fighter *f;

    if (D_0043B7F8[0] != 2) {
        return;
    }
    n = g->unk1F == 3 ? 2 : 1;
    side = g->unk21;
    for (i = 0; i < n; i++, side ^= 1) {
        f = &gFighters[side];
        for (j = 0; j < g->team[side].count; j++) {
            if (g->team[side].unk0[j] == f->unk2A) {
                g->team[side].unk10[j] = 2;
                func_003AE400(side, f->unk0, 1);
                g->round[g->unk40 - 1].side[k] = side;
                g->round[g->unk40 - 1].ch[k] = f->unk0;
                k++;
            }
        }
    }
    func_0035A1E8(g);
}

/* NON_MATCHING: 10 lines, r=0 register and the s16 test's slti/xori */
#if 0
s32 func_003AE608(s32 ch, s32 kind) {
    s32 r = 0;

    if (kind == 4) {
        if (ch == 0x1A) {
            r = func_003BFE58() != 0;
        } else if (D_004FF690[ch].unk8 < 0) {
            r = 0;
        } else {
            r = 1;
        }
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_common", func_003AE608);
#endif

void func_003AE668(void) {
    func_003B0350(0);
}

s32 func_003AE670(void) {
    s32 r = 0;

    if (D_0043B7F8[0xF] != 0) {
        r = D_0043B7F8[8] != 0;
    }
    return r;
}

s32 func_003AE698(void) {
    s32 r = 0;

    if (D_0043B7F8[0xF] == 0) {
        r = D_0043B7F8[8] != 0;
    }
    return r;
}

s32 func_003AE6C0(s16 ch) {
    return D_00437DA0[ch];
}

void func_003AE6E0(void) {
    func_0038DBC0();
    D_00415808.unk0 = -0x80;
    D_00415808.unk4 = -0x800;
    D_00415808.unk8 = 0;
    D_00415808.unk14 = 0;
    D_00415808.unk18 = -0x480;
    D_00415808.unk1C = 0x1200;
}

void func_003AE730(void) {
}

void func_003AE738(s32 a, s32 i) {
    s32 c;

    if (D_0043B7F8[0xA] != 0 && D_0043B7F8[0] != 2) {
        D_004157F8[i].unk0 = ((McSave *)D_00439C60)->unk38[i].a;
        D_004157F8[i].unk1 = ((McSave *)D_00439C60)->unk38[i].b;
        return;
    }
    c = 0x1B;
    if (i == 0) {
        c = 0x12;
    }
    D_004157F8[i].unk0 = c;
    D_004157F8[i].unk1 = -1;
}

void func_003AE7C8(void) {
    s32 i;
    s32 k;
    s32 j;
    Fighter *f;

    D_005F5EE0 = 0;
    D_005F5EE4 = 0;
    D_005F5F08 = 0;
    D_005F62AC = 0;
    func_003645D8(1);
    func_003645D8(2);
    D_005F5EFC = 0;
    D_005F5F00 = 0;
    D_005F5EF8 = 0;
    func_003B6218();
    if (D_0043B7F8[8] == 0) {
        D_005F5CB6 = 0x18;
        func_003AE6E0();
        Tag_ResetCounters();
        func_003B8C08();
        func_00353D58();
        func_00353DB0();
        func_00353658();
        func_00353E08();
        func_003533E8(0);
        func_00363618();
        func_00363630(0x16);
    } else {
        func_003B8AA0();
    }
    f = gFighters;
    for (i = 0; i < 4; i++) {
        func_00373218(f);
        f->unk10C = 0;
        D_00437E30[i].unk2 = 0;
        D_00437E30[i].unkA = 0;
        func_00371B78(f);
        f->unkE0 = 0;
        f->unk127 = 0;
        gFighters[i].unk26 = -1;
        f++;
    }
    func_00373320();
    func_00373DE0();
    func_003760E0();
    func_003753D0();
    func_00375190();
    for (i = 0; i < 4; i++) {
        D_008D7E40[i] = 0;
    }
    for (j = 0; j < 4; j++) {
        for (k = 0; k < 3; k++) {
            D_004177E8[j][k].unk0 = -1;
        }
    }
}

void func_003AE9D8(s32 i) {
    Fighter *f = &gFighters[i];

    if (D_0043B7F8[8] == 0) {
        f->unk10C = 3;
        D_00437E30[i].unkA = 3;
        D_00437E30[i].unk0 = 0;
    }
}

void func_003AEA30(void) {
    func_003AE170(&D_00415808);
}

void func_003AEA40(s32 side, s32 hold) {
    Fighter *f = &gFighters[side];
    struct FighterMove *mv;

    f->unkB8 = f->unkC0 = func_00368218(f, D_00437E30[side].unkA);
    if (D_00437E30[side].unk0 >= ((struct FighterMove *)f->unkB8)->unk18 && hold == 0) {
        if (D_00437E30[side].unkA == 3) {
            D_00437E30[side].unkA = 0;
            f->unk10C = 0;
            mv = func_00368218(f, 0);
            f->unkC0 = mv;
            f->unkB8 = mv;
        }
        func_00363068(f, 5);
        D_00437E30[side].unk0 = 0;
        f->unk11D = 0;
    }
    f->unkBC = f->unkC4 = D_00437E30[side].unk0 + 1;
    f->unkB132 = 0;
    f->unk127 = 1;
    Skel_Update(f);
    Fighter_RunMoveEvents(f);
    f->unkC6 = f->unkC4;
    if (func_0035A670() == 0) {
        if (D_008D7E40[f->unk22] != 0) {
            s16 s = f->unk1E;

            f->unk1E = (f->unk22 >> 1) & 1;
            Snd_StartMoveVoices(f);
            if (f->unkDC != -1) {
                func_00376220(f, f);
            }
            f->unk1E = s;
        } else {
            Snd_StartMoveVoices(f);
            if (f->unkDC != -1) {
                func_00376220(f, f);
            }
        }
        if (D_008D7E40[f->unk22] == 0 && f->unk10C == 3 && f->unkC4 == 3) {
            func_00375368(f->unk2C, side & 1);
        }
    }
    if (hold == 0) {
        D_00437E30[side].unk0++;
    }
}

void func_003AEC58(s32 i, s32 a, s32 b, s32 c) {
    if (i < 4) {
        D_00437E30[i].unk2 = 1;
        D_00437E30[i].unk4 = a;
        D_00437E30[i].unk6 = b;
        D_00437E30[i].unk8 = c;
    }
}

void func_003AECA8(s32 side, s32 ch, s32 costume, s32 c) {
    Fighter *f = &gFighters[side];
    s32 m;

    if (D_0043B7F8[8] == 0) {
        D_00437E30[side].unk4 = ch;
        D_00437E30[side].unk6 = costume;
        D_00437E30[side].unk8 = c;
        func_0035C8D0(f, ch, costume);
        m = func_00367310(f->unk22);
        func_003AF540(f, 0, m);
        func_003AFDA8(side, f->unk22, m);
        func_00362B18(f);
        D_00437E30[side].unk2 = 5;
        D_008D7E40[f->unk22] = 1;
    }
}

void func_003AEDA0(s32 i) {
    D_00437E30[i].unkA = 0;
    gFighters[i].unk10C = 0;
}

void func_003AEDD8(s32 i) {
    if (i < 4) {
        func_003AEEF0(i);
        D_00437E30[i].unk2 = 0;
        D_00437E30[i].unk0 = 0;
        func_003AEDA0(i);
        gFighters[i].unk127 = 0;
        if (i >= 2) {
            s32 j = i - 2;

            if (D_00437E30[j].unkA == 3 && gFighters[j].unk10C == 3) {
                D_00437E30[j].unk0 = 0;
            }
            D_00437E30[j].unkA = 0;
            gFighters[j].unk10C = 0;
        }
        i &= 1;
        func_00375388(i);
        func_00375488(i);
    }
}

void func_003AEEF0(s32 i) {
    s32 k;

    if (i < 4) {
        for (k = 0; k < 3; k++) {
            if (D_004177E8[i][k].unk0 >= 0) {
                File_Release(D_004177E8[i][k].unk0);
                D_004177E8[i][k].unk0 = -1;
            }
        }
    }
}


void func_003AEF88(s32 side, s32 b, s32 c, s32 hold) {
    Fighter *f = &gFighters[side];
    McSideMove *sm = &D_00437E30[side];
    s32 mv = D_00437E30[side].unk4;
    McStart *st;
    s16 *p;
    s32 x, y, z, dir;

    if (D_0043B7F8[0] == 5) {
        st = &D_00437D80[side];
    } else {
        st = &D_00437D40[side];
    }
    switch (D_00437E30[side].unk2) {
    case 0:
        break;
    case 1:
        f->unk127 = 0;
        p = &D_005F62A8[side & 1];
        if (*p < 2 && (func_003CBED8(side & 1) & 0xA000)) {
            if (side & 2) {
                break;
            }
            if (*p == 0) {
                break;
            }
        }
        func_0035C8D0(f, mv, D_00437E30[side].unk6);
        func_003ADAA0(f);
        func_003A8848(f->unk22);
        D_00437E30[side].unk2 = 2;
        func_003AEEF0(side);
        func_00367488(f, 1);
        break;
    case 2:
        if (((McFile *)((u8 *)D_004177F8 + side * 0x24))->unk8 >= 0) {
            if (File_IsMarkerReached(((McFile *)((u8 *)D_004177F8 + side * 0x24))->unk8)) {
                sm->unk2 = 3;
            }
        }
        break;
    case 3:
        func_00367600(f);
        if (D_0043B7F8[8] != 0) {
            D_00437E30[side].unk2 = 0;
            break;
        }
        D_00437E30[side].unk2 = 4;
    case 4:
        func_003AFDA8(side, f->unk22, func_00367310(side));
        func_00362B18(f);
        D_00437E30[side].unk2 = 5;
    case 5:
        D_00437E30[side].unk2 = 6;
        func_00363068(f, 5);
        x = st->x;
        f->unk4 = x;
        y = st->y;
        f->unk8 = y;
        y -= D_00437DE8[f->unk2A];
        z = st->z;
        f->unk18 = 0;
        f->unkC = z;
        f->unk1C = 0;
        dir = st->dir;
        f->unk3C = dir;
        f->unk8 = y;
        D_00437E30[side].unk0 = 0;
        f->unk1A = dir;
        f->unk51C = x;
        f->unk520 = y;
        f->unk524 = z;
        f->unk11D = 0;
        func_0035C850(f, 0);
        func_00368168(f);
        f->unk11D = 0;
        func_00373E18(f);
        break;
    case 6:
        func_003AEA40(side, hold);
        break;
    }
}

s32 func_003AF2E0(s32 a, s32 b) {
    return D_00440C28[a][b].unk0;
}

s32 func_003AF308(s32 a, s32 b) {
    return D_00440C28[a][b].unk4;
}

s32 func_003AF330(s32 a, s32 b) {
    return D_00440C28[a][b].unk8;
}

s32 func_003AF358(void) {
    s32 r = 0;

    if (D_00440D04[0] != 0) {
        r = gMode == 5;
    }
    return r;
}

void func_003AF380(void) {
    s32 i;
    s32 k;

    for (i = 0; i < 4; i++) {
        for (k = 0; k < 3; k++) {
            if (D_004177E8[i][k].unk0 >= 0) {
                File_Release(D_004177E8[i][k].unk0);
                D_004177E8[i][k].unk0 = -1;
            }
        }
    }
}

s32 func_003AF448(void) {
    s32 i;
    s32 k;
    s32 n;

    if (File_CountQueued() != 0) {
        return 0;
    }
    n = 0;
    for (i = 0; i < 4; i++) {
        for (k = 0; k < 3; k++) {
            if (D_004177E8[i][k].unk0 >= 0) {
                File_Release(D_004177E8[i][k].unk0);
                n++;
                D_004177E8[i][k].unk0 = -1;
            }
        }
    }
    return n == 0;
}

void *func_003AF528(s32 i) {
    return D_008D7E50[i];
}

/* NON_MATCHING: 41 lines, the relocation loops get other temporaries (a0/a1 swapped) and its j loop a likely branch; invariants hoisted only partly */
#if 0
static inline void McModel_Relocate(McModel *m, McModelFile *file, s32 ch) {
    s32 x, y, z;
    s32 n;
    void **tbl = m->unkC;
    s32 *pn = &m->unk4;
    u8 *jp = file->joint[0];

    m->unk0 = 1;
    *pn = file->unk10;
    file->unk14 += (s32)file;
    for (x = 0; x < 2; x++) {
        for (y = 0; y < 2; y++) {
            for (z = 0; z < 5; z++) {
                if (file->part[x][y][z] != 0) {
                    file->part[x][y][z] += (s32)file;
                }
            }
        }
    }
    n = *pn + 0x62;
    for (x = 0; x < n; x++) {
        tbl[x] = jp;
        jp += 0x20;
    }
    D_008D7E50[ch] = jp;
}

void func_003AF540(Fighter *f, s32 a, s32 b) {
    McModelFile *file = (McModelFile *)b;
    s32 ch = (s16)f->unk22;
    s32 id = (s16)f->unk20;
    McModel *m;
    u32 *buf;
    u128 pk;
    s32 i, j, k;

    m = func_003B87B0(ch);
    buf = func_003B8798(ch);
    sceGifPkInit(&pk, buf);
    sceGifPkEnd(&pk, 0, 0, 0);
    func_00354290(&pk, func_00367330(ch), D_00437E60[id]);
    sceGifPkTerminate(&pk);
    buf[0] |= sceGifPkSize(&pk) - 1;
    FlushCache(0);
    m->unk8 = (McPartTbl *)file;
    if (m->unk0 == 0) {
        McModel_Relocate(m, file, ch);
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 2; j++) {
                for (k = 0; k < 5; k++) {
                    if (m->unk8->part[i][j][k] != 0) {
                        func_00343CC8(m->unk8->part[i][j][k], D_00437E60[id]);
                    }
                }
            }
        }
    }
    func_003D8498(f, func_00367350(ch));
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_common", func_003AF540);
#endif

void func_003AF7D0(Fighter *f, s32 b) {
    s32 ch = (s16)f->unk22;
    s32 id;
    McModelEx *mx;
    McJoint *j0;
    McJoint *joints;
    McModel *m;
    f32 d;
    s32 *p;
    s32 i, j, k;
    s32 v;

    j0 = (McJoint *)((u8 *)f + 0x560);
    joints = (McJoint *)((u8 *)f + 0xDB0);
    id = (s16)f->unk20;
    m = func_003B87B0(ch);
    mx = func_003B87C8(ch);
    d = 100.0f;
    v = (s16)(*(f32 *)(b + 0xC) * 4096.0f / d);
    *(s32 *)((u8 *)f + 0x500) = v;
    f->unk4FC = v;
    p = &D_00437E60[id];
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            for (k = 0; k < 5; k++) {
                if (m->unk8->part[i][j][k] != 0) {
                    func_003568E0(m->unk8->part[i][j][k], *p);
                }
            }
        }
    }
    j0->parent = 0;
    joints->parent = j0;
    func_003AFE00(f, id, ch, joints, (McJointHdr *)m);
    func_00347760(f);
    func_003564C8(f, m);
    func_003D8490(mx->unk8, (u16)D_005F5CB6 == 0xE ? 2 : 1);
    func_00351E10(f);
    func_003519E0(f, m);
    func_003504C0(f);
    func_00353668(f->unk1E);
    func_00353678(f->unk1E);
    if (gMode == 5) {
        D_005F67C0 = 1;
    } else {
        D_005F67C0 = 0;
    }
}

/* NON_MATCHING: 15 lines, the +0x8730 slot base is computed once where the original builds it twice */
#if 0
void func_003AF9D8(Fighter *f, s32 a, s32 b) {
    s16 *t = D_00439A80[f->unk2A];

    ((McFighter *)f)->slot[b] = &((McFighter *)f)->part[b];
    func_00366B68(((McFighter *)f)->slot[a], &((McFighter *)f)->part[b]);
    ((McFighter *)f)->part[b].unk18 = t[b * 4 + 0];
    ((McFighter *)f)->part[b].unk1C = t[b * 4 + 1];
    ((McFighter *)f)->part[b].unk20 = t[b * 4 + 2];
    func_0034E640(b, f);
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_common", func_003AF9D8);
#endif

RODATA_ALIGN16();
INCLUDE_RODATA("asm/nonmatchings/menu_common", D_0051DE50);

void func_003AFAA0(Fighter *f, s32 a) {
    McFighter *mf = (McFighter *)f;
    s16 *t = D_00439A80[f->unk2A];
    McModel *m = func_003B87B0(a);
    s32 i;
    s32 k;
    s32 n;
    s32 s;
    s16 chr; /* fake: the s16 copy keeps the switch value and the &unkB050 base in the original's registers */
    s32 par;

    if (f->unk2E == 0x6B) {
        t = D_004396C0;
    }
    for (i = 0; i < 0x12; i++) {
        McPart **sp = &mf->slot[i];

        par = *(s32 *)m->unkC[D_00437FF8[D_0051DE50[i]]];
        if (par < 0) {
            k = par;
        } else {
            k = D_00437E70[par];
            for (n = 0; n < 0x12; n++) {
                if (D_0051DE50[n] == k) {
                    k = n;
                    break;
                }
            }
        }
        s = D_0051DE50[i];
        *sp = &mf->part[s];
        if (k >= 0) {
            func_00366B68(mf->slot[k], &mf->part[s]);
        } else if (i >= 0x12) {
            *sp = NULL;
        } else {
            func_00366B68((u8 *)f + 0x504, &mf->part[s]);
        }
        mf->part[s].unk18 = t[s * 4 + 0];
        mf->part[s].unk1C = t[s * 4 + 1];
        mf->part[s].unk20 = t[s * 4 + 2];
    }
    chr = f->unk2A;
    s = chr;
    switch (s) {
    case 0x11:
        if (gMode == 5) {
            break;
        }
        func_003AF9D8(f, 2, 0x12);
        func_003AF9D8(f, 0x12, 0x13);
        func_003AF9D8(f, 0x13, 0x14);
        f->unkB050 = 0x15;
        break;
    case 0x14:
        func_003AF9D8(f, 0xC, 0x12);
        func_003AF9D8(f, 0x12, 0x13);
        f->unkB050 = s;
        if (gMode != 5) {
            func_003AF9D8(f, 2, 0x14);
            f->unkB050++;
        }
        func_0034FBA0((s16)f->unk20);
        break;
    default:
        f->unkB050 = 0x12;
        break;
    }
}

void func_003AFDA8(s32 side, s32 slot, s32 b) {
    Fighter *f = &gFighters[side];

    func_003ADBB0(f);
    func_003AF7D0(f, b);
    func_00362B18(f);
}

void func_003AFE00(Fighter *f, s32 id, s32 a, McJoint *joints, McJointHdr *h) {
    s32 n = h->count;
    McVec3 *out = func_003B87F8(id);
    s32 i;

    for (i = 0; i < n + 0x62; i++) {
        McJointSrc *s = h->joint[i];
        void *p;
        f32 *d;

        if (s->parent >= 0) {
            p = &joints[s->parent];
        } else {
            p = (u8 *)f + 0x560;
        }
        joints[i].parent = p;
        d = (f32 *)(i * 64 + (s32)f + 0x8800);
        d[0] = s->a[0];
        d[1] = s->a[1];
        d[2] = s->a[2];
        out[i].v[0] = s->b[0];
        out[i].v[1] = s->b[1];
        out[i].v[2] = s->b[2];
    }
    func_003AFAA0(f, a);
}

s32 func_003AFF08(s32 i) {
    s32 k = D_005F62A8[i];

    return D_008D7E88[i].s[k & 1];
}

/* NON_MATCHING: 9 lines, the port table's unk6 base is built as %lo + 6 in the original */
#if 0
void func_003AFF40(s32 i) {
    s32 k = D_005F62A8[i];
    s16 *p = &D_008D7E68[D_008D7E88[i].s[k & 1]].unk6;

    if (++*p >= 3) {
        *p = 2;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_common", func_003AFF40);
#endif

s32 func_003AFFB8(s32 i, s32 a) {
    return D_008D7E88[i].s[a];
}

void func_003AFFE0(s32 a, s32 port) {
    s16 v;
    s16 n;

    if (a != 0) {
        v = D_008D7E68[port].unk0;
    } else {
        v = D_008D7E68[port].unk2;
    }
    D_008D7E68[port].unk0 = v;
    n = ++D_008D7E88[v].s[2];
    D_008D7E88[v].s[n] = port;
    if (D_008D7E88[v].s[2] == 0) {
        D_008D7E88[v].s[n + 1] = port;
    }
}

s16 func_003B0078(s32 a, s32 port) {
    return a != 0 ? D_008D7E68[port].unk0 : D_008D7E68[port].unk2;
}

void func_003B00A8(s32 a, s32 port, s32 v) {
    if (a != 0) {
        D_008D7E68[port].unk0 = v;
    } else {
        D_008D7E68[port].unk2 = v;
    }
}

void func_003B00D8(s32 port, s32 v) {
    D_008D7E68[port].unk4 = v;
}

s32 func_003B00F0(s32 port) {
    return D_008D7E68[port].unk4;
}

s32 func_003B0108(s32 port) {
    return D_008D7E68[port].unk6;
}

/* NON_MATCHING: 8 lines, the port table's unk6 base is built as %lo + 6 in the original */
#if 0
void func_003B0120(s32 i) {
    s32 k;

    for (k = func_003B01B0(i); k >= 0; k--) {
        s16 *p = &D_008D7E68[D_008D7E88[i].s[k]].unk6;

        if (*p > 0) {
            (*p)--;
            return;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_common", func_003B0120);
#endif

s32 func_003B01B0(s32 i) {
    return D_008D7E88[i].s[2];
}

/* NON_MATCHING: 19 lines, saved registers (s3 holds the table's %hi) and scheduling */
#if 0
void func_003B01D0(s32 a, s32 port, s32 b) {
    s16 v = -1;

    if (a != 0) {
        v = b;
    }
    D_008D7E68[port].unk0 = v;
    if (Pad_CountReady() < 3) {
        D_008D7E68[port].unk2 = port & 1;
    } else {
        D_008D7E68[port].unk2 = (port & 2) >> 1;
    }
    D_008D7E68[port].unk4 = 0;
    D_008D7E68[port].unk6 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_common", func_003B01D0);
#endif

void func_003B0260(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_008D7E68[i].unk6 = 0;
    }
}

void func_003B0298(s32 i) {
    D_008D7E88[i].s[0] = -1;
    D_008D7E88[i].s[1] = -1;
    D_008D7E88[i].s[2] = -1;
}

void func_003B02C8(s32 x) {
    s32 i;

    func_003B0298(x);
    for (i = 0; i < 4; i++) {
        if (D_008D7E68[i].unk0 == x) {
            func_003B01D0(0, i, x);
        }
    }
}

s32 func_003B0340(void) {
    return D_005F6D82;
}

s16 func_003B0348(void) {
    return D_005F6D7E;
}

void func_003B0350(s16 v) {
    D_005F6D7E = v;
}

void func_003B0358(void) {
    D_005F6D7E ^= 1;
}

void func_003B0368(void) {
    D_005F6D82 = D_005F6D7E;
}

void func_003B0378(s16 v) {
    D_005F6D82 = v;
}

void func_003B0380(s32 side) {
    func_003AE738(1, side);
    D_005F5CC8[side] = 0;
    D_005F5CB0[side] = 1;
    D_005F5C80[side] = 0;
    func_003BA520(side);
    D_005F62A8[side] = 0;
}

void func_003B03F8(s32 port) {
    s32 v;

    func_003AFFE0(0, port);
    v = func_003B0078(1, port);
    if (func_003B01B0(v) == 0) {
        func_003CB8E0(v, 0);
    }
}

s32 func_003B0458(void) {
    s16 i;
    s16 n = 0;

    for (i = 0; i < 4; i++) {
        n += D_008D7E68[i].unk4;
    }
    return n;
}

s32 func_003B04A0(void) {
    s16 i;
    s16 n = 0;

    for (i = 0; i < 4; i++) {
        if (D_008D7E68[i].unk0 >= 0) {
            n++;
        }
    }
    return n;
}

s32 func_003B04F0(u32 *held, u32 *pressed) {
    s32 i;
    s32 mask = 0;

    *held = 0;
    *pressed = 0;
    for (i = 0; i < 4; i++) {
        if (Pad_GetPlayerMode(i) == -1) {
            continue;
        }
        if (func_003B00F0(i) != 0) {
            continue;
        }
        mask |= 1 << i;
        *held |= D_00415748[i];
        *pressed |= D_00415758[i];
    }
    if ((*held & 0x5000) == 0x5000) {
        *held &= ~0x5000;
    }
    if ((*held & 0xA000) == 0xA000) {
        *held &= ~0xA000;
    }
    if ((*pressed & 0x5000) == 0x5000) {
        *pressed &= ~0x5000;
    }
    if ((*pressed & 0xA000) == 0xA000) {
        *pressed &= ~0xA000;
    }
    return mask;
}

void func_003B0660(s16 i) {
    if (func_003B0458() == 4) {
        if (D_008D7E88[i].s[0] == D_008D7E88[i].s[1]) {
            D_008D7E88[i].s[1] = -1;
        }
    }
}

void func_003B06C8(void) {
    s32 i;
    s32 k;
    s32 found;
    s32 v;
    s32 w;

    func_003B0458();
    func_003B04A0();
    if (func_003B0458() == func_003B04A0()) {
        return;
    }
    for (i = 0; i < 4; i++) {
        if (func_003B00F0(i) == 0) {
            continue;
        }
        found = 0;
        if (func_003B0078(1, i) >= 0) {
            continue;
        }
        for (k = 0; k < 2; k++) {
            v = (func_003B0078(0, i) + k) & 1;
            if (func_003B01B0(v) <= 0) {
                func_003B00A8(0, i, v);
                found = 1;
                break;
            }
        }
        if (found) {
            func_003B03F8(i);
            w = func_003B0078(1, i);
            if (D_005F5CB0[w] == 0) {
                func_003B0380(w);
            }
        }
    }
}

void func_003B07F8(void) {
    D_005F6D80 = Pad_IsMultitap();
}

s32 func_003B0818(void) {
    return D_005F6D80;
}

typedef struct {
    /* 0x0 */ u8 pad0[8];
    /* 0x8 */ s32 unk8;
} McJoinArg;

typedef struct {
    s32 any;
    s32 pair;
    s32 limit;
    s32 maxn;
} McJoinRule;

extern s16 D_008D7E6E[];
void *memset(void *p, s32 c, s32 n);
void func_003B06C8(void);
void func_003B0660(s16 i);
s32 func_003B0108(s32 port);
s32 func_003B0340(void);
void func_003B0820(McJoinArg *arg);

/* NON_MATCHING: 154 lines, the spilled rule locals (sp+0x10..0x1C) get other constant registers and cross-jumped switch tails; s-register allocation */
#if 0
void func_003B0820(McJoinArg *arg) {
    s32 se[2];
    s32 any;
    s32 pair;
    s32 limit;
    s32 maxn;
    s32 n;
    s32 i;
    s32 k;
    s32 v;
    s32 done;
    s32 found;
    s32 keys;
    s32 pad;

    memset(se, 0, sizeof(se));
    any = 0;
    pair = 0;
    limit = 2;
    maxn = 2;
    if (arg->unk8 == 0) {
        func_003B06C8();
        return;
    }
    if (D_005F5CB4 == 1) {
        return;
    }
    switch (D_0043B7F8[0]) {
    case 2:
        limit = 2;
        any = 1;
        maxn = 4;
        break;
    case 5:
    case 6:
        limit = 2;
        any = 1;
        maxn = 2;
        break;
    case 1:
        if (func_003B0340() != 0) {
            limit = 4;
        } else {
            limit = 2;
            any = 1;
        }
        maxn = 4;
        break;
    case 0:
    case 8:
        if (func_003B0340() != 0) {
            limit = 4;
        } else {
            any = 1;
            limit = 2;
        }
        maxn = 4;
        break;
    case 3:
    case 4:
    case 7:
        if (func_003B0340() != 0) {
            pair = 1;
            limit = 2;
        } else {
            limit = 1;
        }
        maxn = 2;
        break;
    case 9:
        break;
    }
    n = 0;
    for (i = 0; i < 4; i++) {
        if (func_003B00F0(i) != 0) {
            n++;
            v = func_003B0108(i);
            if (v != 0) {
                n += v - 1;
            }
        }
    }
    for (i = 0; i < 4; i++) {
        pad = D_00415728[i];
        if (func_003B00F0(i) == 0 && n < maxn) {
            v = func_003B0458();
            if (pad & 0x800) {
                if (v >= limit) {
                    continue;
                }
                func_003B00D8(i, 1);
                n++;
                se[0] = 1;
                pad &= ~0x800;
                if (func_003B0458() == 4) {
                    for (k = 0; k < 4; k++) {
                        if (D_008D7E6E[k * 4] > 0) {
                            func_003B0660(func_003B0078(1, k));
                        }
                    }
                }
            }
        }
        if (func_003B00F0(i) != 1) {
            continue;
        }
        done = 0;
        if (func_003B0078(1, i) >= 0) {
            continue;
        }
        if (any) {
            for (k = 0; k < 2; k++) {
                if (D_005F5CB0[k] == 0) {
                    func_003B00A8(0, i, k);
                    done = 1;
                    break;
                }
            }
        } else if (pair && (D_005F5CB0[0] != 0 || D_005F5CB0[1] != 0)) {
            for (k = 0; k < 2; k++) {
                if (D_005F5CB0[k] != 0) {
                    func_003B00A8(0, i, k);
                    done = 1;
                    break;
                }
            }
        } else {
            for (k = 0; k < 2; k++) {
                if ((D_005F62A8[k] >= 2 && D_005F5CB0[k] != 0) || func_003B01B0(k) > 0) {
                    if (func_003B0458() - func_003B04A0() == 1) {
                        func_003B00A8(0, i, k ^ 1);
                        done = 1;
                        break;
                    }
                }
            }
        }
        if (!done) {
            found = 0;
            keys = pad & 0x8FC;
            for (k = 0; k < 2; k++) {
                if ((D_005F62A8[k] >= 2 && D_005F5CB0[k] != 0) || func_003B01B0(k) > 0) {
                    func_003B00A8(0, i, k ^ 1);
                    found = 1;
                    break;
                }
            }
            if (!found && (pad & 0xA000)) {
                found = (pad >> 13) & 1;
                if (found != func_003B0078(0, i)) {
                    Snd_PlaySe(3, 0);
                }
                func_003B00A8(0, i, found);
            }
            if (keys) {
                done = 1;
            }
            if (!done) {
                continue;
            }
        }
        func_003B03F8(i);
        se[1] = 1;
        v = func_003B0078(1, i);
        if (D_005F5CB0[v] == 0) {
            func_003B0380(v);
            break;
        }
    }
    if (se[0]) {
        Snd_PlaySe(0x41, 0);
        Snd_PlaySe(0x42, 0);
    } else if (se[1]) {
        Snd_PlaySe(5, 0);
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/menu_common", jtbl_0051DE70);

INCLUDE_ASM("asm/nonmatchings/menu_common", func_003B0820);
#endif

INCLUDE_RODATA("asm/nonmatchings/menu_common", D_0051DE98);
INCLUDE_RODATA("asm/nonmatchings/menu_common", D_0051DEB0);
INCLUDE_RODATA("asm/nonmatchings/menu_common", D_0051DEC0);
INCLUDE_RODATA("asm/nonmatchings/menu_common", D_0051DED0);
INCLUDE_RODATA("asm/nonmatchings/menu_common", D_0051DEE0);

/* NON_MATCHING: 16 lines, side and the slot counter k take s3/s4 swapped (and the port+0x41 temporary with them) */
#if 0
void func_003B0D50(s32 port, s32 side, s32 blink) {
    McXY3 pos;
    s32 x = 0;
    s32 y = 0x1A4;
    s32 k = 0;
    s32 sel = 0;
    s32 multi = 0;
    s32 n;
    s32 i;
    s32 s;
    s32 me;
    s32 col;
    s32 show;
    s32 off;
    s32 w;
    s32 c;

    pos = D_0051DE98;
    if (func_003B00F0(port) == 0) {
        return;
    }
    s = func_003B01B0(side);
    if (func_003B0078(1, port) < 0) {
        n = 0;
        for (i = 0; i < 4; i++) {
            if (func_003B00F0(i) == 0) {
                continue;
            }
            if (func_003B0078(1, i) >= 0) {
                continue;
            }
            if (func_003B0078(0, i) != side) {
                continue;
            }
            n++;
            k += i < port;
        }
        if (n == 3) {
            x = pos.p[k].x;
        }
        y = pos.p[k].y + 0x1A4;
        s = s + 1;
        multi = 1;
    } else if (s == 1) {
        if (D_008D7E88[side].s[0] == port) {
            s = 0;
        }
    }
    if (s < 0) {
        exit(1);
    }
    x += D_00439B10[side * 2 + s];
    me = func_003B0078(1, port);
    blink += port * 4;
    if (port == func_003AFF08(side) && D_005F62A8[side] < 2) {
        if (blink & 0x10) {
            func_003CCEA8(x, y, port);
        }
        sel = 1;
    } else if (me >= 0) {
        func_003CCEA8(x, y, port);
    }
    y += 10;
    col = 0;
    if (sel == 1 && (blink & 4)) {
        col = D_005F62A8[side] > 1 ? 0 : 9;
    }
    show = 0;
    side = Pad_GetPlayerMode(port) >= 0; /* fake: reuses side */
    if (!side) {
        col = 0x16;
    }
    if (me >= 0 || (blink & 0x10)) {
        show = 1;
    }
    if (D_005F6D80 != 0 || Pad_IsMultitap() != 0) {
        if (Pad_IsMultitap() == 0) {
            D_005F62B0++;
        } else {
            D_005F62B0 = 0;
            D_005F6D80 = 1;
        }
        off = -0x1C;
        c = port + 0x41;
        if (show) {
            Text_Printf(D_0051DEB0, x - 0x1C, y, 10, 4, col);
            Text_Printf(D_0051DEC0, x + 2, y - 4, 10, 5, col, c);
        }
        w = 0x39;
    } else {
        off = -0x13;
        if (show) {
            Text_Printf(D_0051DED0, x - 0x13, y, 10, 4, col);
            Text_Printf(D_0051DEE0, x - 2, y - 4, 10, 5, col, port + 1);
        }
        w = 0x27;
    }
    if (me < 0 && multi == 1 && side && (blink & 0x10)) {
        func_003CCF40(x + off - 0xC, x + off + w, y - 0x14, port);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/menu_common", func_003B0D50);
#endif

s32 func_003B1118(void) {
    if (gMode == 3) {
        return 0;
    }
    if (!((D_00415728[0] | D_00415728[1] | D_00415728[2] | D_00415728[3]) & 0x800)) {
        return 0;
    }
    if (func_003BDF38() != 0) {
        return 0;
    }
    Mode_Change(3);
    Snd_PlaySe(4, 0);
    return 1;
}

s32 func_003B1198(void) {
    if (gMode == 3) {
        return 0;
    }
    if (!((D_00415728[0] | D_00415728[1] | D_00415728[2] | D_00415728[3]) & 0x800)) {
        return 0;
    }
    Snd_PlaySe(4, 0);
    return 1;
}

s32 func_003B1200(s32 port) {
    s32 s;
    s32 o;

    if (func_003B00F0(port) == 0 && (D_00415728[port] & 0x800)) {
        if (D_005F5CB4 != 0) {
            return 0;
        }
        if (D_005F5CB0[0] != 0 && D_005F5CB0[1] != 0) {
            return 0;
        }
        s = D_005F5CB0[0] != 0;
        o = s ^ 1;
        ((McSetup *)D_0043B7F8)->unk16++;
        func_003B01D0(1, port, s);
        func_003B00D8(port, 1);
        func_003B00A8(0, port, s);
        func_003AFFE0(0, port);
        func_003B0380(s);
        func_003BAA38();
        func_003ADB70(o, 0, 1);
        func_003ADB70(o, 1, 1);
        func_003ADB70(s, 0, 0);
        func_003ADB70(s, 1, 0);
        return 1;
    }
    return 0;
}

s32 func_003B1330(void) {
    s32 i;
    s32 r = 0;

    for (i = 0; i < 4; i++) {
        r |= func_003B1200(i);
    }
    return r;
}

s32 func_003B1378(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        u16 h = D_00415718[i] & 0xFFF3;
        u16 p = D_00415728[i] & 0xFFF3;
        if (h == 0x900 && p == 0x100) {
            if (func_003B7A38() != 0) {
                return 0;
            }
            if (func_003BDF38() != 0) {
                return 0;
            }
            Snd_PlaySe(4, 0);
            return 1;
        }
    }
    return 0;
}

s32 func_003B1418(s32 i) {
    if (Pad_IsMultitap() != 0) {
        return D_00439B20[i];
    }
    return D_00439B20[i + 4];
}

s32 func_003B1468(void) {
    return D_00439C60[0x36];
}

