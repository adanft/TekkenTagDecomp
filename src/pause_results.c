/* 0x3B4D50-0x3B90C0 (module pause_results, docs/game_overview.md section 4; docs/systems/modes.md). .rodata
 * 0x51E580-0x51E970 is its own (subsegment, every string and table included in place, the selectable-character sets at
 * 0x51E850 last); the start is [I], the end an object edge [V: modes' .rodata follows]. */
#include "common.h"
#include "fighter.h"

extern s16 gMode;
extern s16 gSubMode;
extern u16 gFrameCount;
extern s16 D_005F6DBC;
extern u16 D_005F6DE6;
extern s32 D_005F6DE0;
extern u8 *D_005F6DE8;
extern s32 D_005F6DEC;
extern s32 D_005F6DF0;
extern s32 D_005F6DF4;
extern s32 D_005F6DF8;
extern s32 D_005F6DFC;
extern s32 D_005F6E00;
extern s32 D_005F6E04;
extern s32 D_005F64F8;
extern s16 D_005F5CC8;
extern s16 D_005F5CCA;
extern u8 D_01859080[];
extern u8 D_176A80[];
extern u8 D_012C9BA0[];
extern s32 D_01FAB050[];
extern s32 D_01FAB068[];
extern s32 D_01FAB080[];
extern s32 D_01FAB098[];
extern s32 D_01FAB0B0[];

s32 func_00361B28(void);
extern u16 D_005F6DC0[4];
extern s32 D_005F5F08;
extern s32 D_005F5EE4;
extern s32 D_005F5EE0;
extern s32 D_005F64FC;
extern s32 D_005F6500;
extern u8 D_008D7F80[];
extern u8 D_00FCD2B0[];
extern u8 D_00F6E780[];
extern u8 D_00F705E0[];
extern u8 D_012BDBA0[];
extern u8 D_012B9B90[];
extern u8 D_013474C0[];
extern u8 D_01637DB0[];
extern u8 D_0164BDB0[];
extern u8 D_01834230[];
extern u8 D_012D1BA0[];
u32 func_003712C0(void);
s32 func_003F1570(void);
void Mode_Change(s32 mode);
s32 func_0035EA48(void);
void func_0039C6E8(s32 a, s32 b);
void Bgm_Stop(void);
void func_0035D090(void);
s32 File_WaitAll(void);
void func_003BAA90(void);
s32 File_Request(s32 id, u32 dest, s32 flags, s32 toIop);
void func_003B8A50(void);
void func_00346600(void);
void func_00343F88(void);
void func_003C4828(void);
void func_003992C8(s32 value);
void func_003B4D88(Fighter *f);
void func_003B7A58(void);
void func_003B89B8(void);
extern Fighter gFighters[4];
extern Fighter D_00884EE0; /* gFighters[1] */
extern u8 D_0043B7F8[];
extern s32 D_005F6504;
extern s32 D_005F6DC8;
extern s32 D_005F6DCC;
extern s16 D_005F6DD0;
extern s16 D_005F6DD2;
extern s16 D_005F6DD4;
extern s16 D_005F6DD6;
extern s16 D_005F5C74;
extern s16 D_005F62AC;
extern s32 D_008D7EA8[];
extern s32 D_008D7EE8[];
void Snd_PlaySe(s32 id, s32 player);
void func_003BAA38(void);
void func_003A9510(s32 a);
void func_003A7558(void);
void func_003A87E8(void);
s32 func_003A9698(void);
void func_0036C490(Fighter *f);
void func_00367FF0(void);
void File_WaitRelease(s32 *handle);
void func_003638F0(void);
void func_003B7998(s32 on);
s32 func_003B52F0(u8 *p);
void Options_Apply(void);
extern TeamMember D_00440940[2][2];
void func_00341AE8(void);
void func_003E9628(Fighter *a, Fighter *b, s32 c);
void func_00371B18(Fighter *a, Fighter *b);
void func_00372BA8(Fighter *a, Fighter *b);
void func_0035C238(void);
void func_00366ED0(void);
void func_003DB690(void);
s32 func_003ADF30(s32 side);
void func_0035C8D0(Fighter *f, s32 character, s32 costume);
void func_003ADB98(Fighter *f);
void func_0035C850(Fighter *f, s32 n);
void func_00368168(Fighter *f);
void func_00368398(Fighter *f, Fighter *o);
void func_003B4DC0(Fighter *a, Fighter *b);
typedef struct {
    s32 w[9];
} CamPos;
typedef struct {
    s32 m[8];
} GteMatrix;
extern CamPos D_00415808;
extern GteMatrix D_00436530;
extern s16 gSinTable[];
extern u16 D_005F6DDE;
void func_003DB578(void);
s32 func_0038DBC0(void);
void Tag_SetCounter(s32 side, s32 state);
s32 func_00367310(s32 slot);
void func_003AF540(Fighter *f, s32 a, s32 b);
void func_003AFDA8(s32 side, s32 slot, s32 r);
void func_003ADFB0(void);
void func_0035B4B0(s32 *c, GteMatrix *m);
void func_00341218(void);
void func_003D1040(void);
void Wind_Update(void);
void func_003B6218(void);
void func_003B5C10(Fighter *f, s32 side);
typedef struct {
    u8 unk0[4];
    u8 unk4;
} Unk415840;
extern Unk415840 *D_00415840[];
typedef struct {
    s32 x;
    s32 y;
} ResPos;
extern ResPos D_0043B798[][6];
extern u64 D_005F6738;
extern s32 D_005F6730;
extern s16 D_005F6DB8;
extern s16 D_005F6DC4;
typedef struct {
    s16 c0;
    s16 c1;
    s16 dx;
    s16 dz;
    s32 move;
} ResStart;
extern ResStart D_0043B5C0[];
extern s16 D_0043A180[];
typedef struct {
    s32 v[10];
} ResColors;
extern const ResColors D_0051E5C8;
extern const char D_0051E5F0[];
extern const char D_0051E600[];
extern const char D_0051E610[];
s32 Text_Printf(const char *fmt, ...);
s32 func_003B4F40(Fighter *a, Fighter *b, s16 *c);
void func_003B4E80(Fighter *a, Fighter *b, s16 *pos);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_SetBlend(s32 a, s32 b);
void func_003CF1C0(s32 a, s32 b, s32 x, s32 y);
void func_003D22E0(Fighter *a, Fighter *b, s32 n);
void func_003D2330(Fighter *a, Fighter *b, s32 c, s32 d);
extern s32 D_005F5CDC;
void func_003CD6E8(void);
void func_003C4AE0(void);
s32 Results_DrawStageTimes(s32 a);
void func_003B6228(s32 a, u8 *b);
void func_003CC078(s32 x, s32 y, s32 z);
void func_003CBFB8(s32 spr, s32 x, s32 y, s32 z);
typedef struct {
    u16 x;
    u16 y;
    u32 z;
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    u32 q;
} ResVtx;
/* Menu window quad: alpha limit, then four vertices (XYZ2, RGBAQ), as modes_b.c's MbQuad. */
typedef struct {
    u16 alpha;
    u16 pad2[3];
    ResVtx v[4];
} __attribute__((aligned(8))) ResQuad;
extern ResQuad D_0043FD10[];
void func_003BB680(ResQuad *q, s32 z, s32 unused);
typedef struct {
    s16 c[4];
    s16 w[4];
} PrRec;
typedef struct {
    u8 pad0[0x1A];
    u8 unk1A;
    u8 unk1B;
    u8 unk1C;
    u8 pad1D[0x23];
    u8 cnt;
    u8 pad41[0x3D];
    PrRec rec[15];
} PrSetup;
/* Drafts (NON_MATCHING) */
s32 func_003B5328(void);
s32 func_003B6C68(PrSetup *g);
void Pause_Update(void);
s32 func_003B5CA8(void);
s32 func_003B5DB0(Fighter *a, Fighter *b, s32 mode);
void func_003B6408(void);
void func_003B6808(s32 x, s32 y, s32 idx, s32 hl);
void func_003B77B8(void);
void func_003B7888(void);
void func_003637B0(void);
void func_003B8840(u8 *p);
void func_003B8910(void);

void func_003B4D50(Fighter *f) {
    if (f->unk2A == 15) {
        f->unk28 = D_005F6DC0[f->unk1E];
    }
}

void func_003B4D88(Fighter *f) {
    s32 x = f->unk4;
    s32 z = f->unkC;
    f->unk51C = x;
    f->unk524 = z;
    f->unk708 = x;
    f->unk70C = -0x591;
    f->unk710 = z;
    f->unk6B8 = x;
    f->unk6BC = -0x41B;
    f->unk6C0 = z;
}

void func_003B4DC0(Fighter *a, Fighter *b) {
    s32 i;
    for (i = 0; i < 2; i++) {
        func_003B4D88(a);
        a = b;
    }
}

s32 func_003B4E00(u8 *p) {
    return ((s8 *)p)[0x401] == 0;
}

s32 func_003B4E10(Fighter *a, Fighter *b) {
    s32 i;
    for (i = 0; i < 2; i++) {
        if (gFighters[i].unk2A == 0x13) {
            s16 c = gFighters[i ^ 1].unk2E;
            if (c == 0x6A || c == 0x6C) {
                return 1;
            }
        }
    }
    return 0;
}

void func_003B4E80(Fighter *a, Fighter *b, s16 *pos) {
    a->unk51C = 0;
    a->unk4 = 0;
    a->unk524 = 0;
    a->unkC = 0;
    b->unk51C = 0;
    b->unk4 = 0;
    b->unk524 = 0;
    b->unkC = 0;
    a->unk1C = 0;
    a->unk18 = 0;
    a->unk3C = a->unk40 = a->unk1A = pos[5];
    b->unk1C = 0;
    b->unk18 = 0;
    b->unk3C = b->unk40 = b->unk1A = pos[11];
    func_00368398(a, b);
    func_00368398(b, a);
    a->unk4 = pos[3];
    a->unkC = pos[4];
    b->unk4 = pos[9];
    b->unkC = pos[10];
    func_003B4DC0(a, b);
}

s32 func_003B4F40(Fighter *a, Fighter *b, s16 *c) {
    s32 r = 0;
    if (c[1] != -1) {
        r = c[1] != (a->unk26 & 7);
    }
    if (c[7] != -1 && c[7] != (b->unk26 & 7)) {
        r = 1;
    }
    return r;
}

s32 func_003B4F88(Fighter *a, Fighter *b, s16 *tbl) {
    s32 found = 0;
    s16 *p = tbl;

    a->unkB12C = NULL;
    b->unkB12C = NULL;
    if (a->unk127 == 0 || b->unk127 == 0 || D_0043B7F8[0] == 8) {
        return 0;
    }
    while (p[0] != -1 && p[6] != -1) {
        if (p[0] == a->unk2A && p[6] == b->unk2A) {
            if (func_003B4F40(a, b, p) == 0) {
                a->unkB12C = (void *)p;
                b->unkB12C = (void *)(p + 6);
                if (tbl == D_0043A180) {
                    func_003B4E80(a, b, p);
                }
                found = 1;
                break;
            }
        } else if (p[0] == b->unk2A && p[6] == a->unk2A) {
            if (func_003B4F40(b, a, p) == 0) {
                b->unkB12C = (void *)p;
                a->unkB12C = (void *)(p + 6);
                if (tbl == D_0043A180) {
                    func_003B4E80(b, a, p);
                }
                found = 1;
                break;
            }
        }
        p += 14;
    }
    return found;
}

void func_003B5150(Fighter *a, Fighter *b, s32 flag) {
    a->unk4 = a->unk51C = -500;
    a->unk524 = 0;
    a->unkC = 0;
    a->unk1A = 0;
    a->unk40 = 0;
    a->unk3C = 0;
    a->unk18 = 0;
    a->unk1C = 0;
    b->unk524 = 0;
    b->unkC = 0;
    b->unk18 = 0;
    b->unk1C = 0;
    b->unk1A = 0;
    b->unk40 = 0;
    b->unk3C = 0;
    b->unk4 = b->unk51C = 500;
    func_00368398(a, b);
    func_00368398(b, a);
    func_003B4DC0(a, b);
    if (flag) {
        a->unkC += 0x5DC;
        a->unk710 += 0x5DC;
    } else {
        a->unkC -= 0x5DC;
        a->unk710 -= 0x5DC;
    }
    func_003D22E0(a, b, 0x5A);
    if (D_005F6DB8) {
        func_003D2330(a, b, 0, 0);
    } else {
        func_003D2330(b, a, 1, 0);
    }
}

void func_003B5270(Fighter *a, Fighter *b) {
    func_00341AE8();
    if (D_0043B7F8[0] == 8) {
        func_003E9628(a, b, 0);
    } else {
        func_00371B18(a, b);
    }
    func_00372BA8(a, b);
    func_0035C238();
    func_00366ED0();
    func_003DB690();
}


s32 func_003B52F0(u8 *p) {
    switch (*p) {
    case 0: case 1: case 2: case 5: case 8:
        return 1;
    case 3: case 4: case 6: case 7: case 9: case 10:
    default:
        return 0;
    }
}

extern s16 D_005F64D8;
extern s32 D_005F64DC;
extern s32 gRoundPhase;
extern s16 D_005F6DBA;
extern u16 D_005F5CB0;
void func_0034CB50(void);
void func_00373218(Fighter *f);
s32 func_003B4F88(Fighter *a, Fighter *b, s16 *tbl);
void func_003B5150(Fighter *a, Fighter *b, s32 flag);
s32 func_003B4E10(Fighter *a, Fighter *b);
void func_003465E0(void);
void func_00344318(void);
void Fx_LoadFighterTex(Fighter *f);
void func_003E9530(s32 a);
void func_003533E8(s32 a);
void func_0036DDD0(Fighter *a, Fighter *b);
void Battle_StepMoves(Fighter *a, Fighter *b);
void func_00372A10(Fighter *a, Fighter *b);
void func_00373390(Fighter *f, s32 n);
void func_00373038(Fighter *a, Fighter *b);
void func_003B4D50(Fighter *f);

INCLUDE_RODATA("asm/nonmatchings/pause_results", jtbl_0051E5B0);

/* NON_MATCHING: 90 lines, register allocation in the reload / swap cases, the next-phase select becomes movz (original beql) */
#if 0
s32 func_003B5328(void) {
    Fighter *a = &gFighters[0];
    Fighter *b = &D_00884EE0;
    s32 side, other, m, r, next;
    Fighter *f;
    s32 ret = 0;

    switch (D_005F64D8) {
    case 0:
        D_0043B7F8[2] = 0;
        D_005F6DC0[0] = a->unk28;
        D_005F6DC0[1] = b->unk28;
        D_005F6DBC = 0;
        if (D_005F5CB0) {
            D_005F6DB8 = 1;
        } else {
            D_005F6DB8 = 0;
        }
        D_005F6DBA = D_005F6DB8;
        a->unkB132 = 0;
        b->unkB132 = 0;
        side = D_005F6DBA;
        other = side ^ 1;
        a->unk127 = 1;
        b->unk127 = 1;
        D_005F6DBC = 1;
        m = gFighters[other].unk2;
        if (gFighters[other].unk402) {
            gFighters[side].unk127 = 0;
        }
        f = &gFighters[side];
        if (f->unk127) {
            func_0035C8D0(f, D_00440940[other][m].unk0, D_00440940[other][m].unk4);
            f->unk22 = m * 2 + other;
            func_003ADB98(f);
            func_0035C850(f, 0);
            func_00368168(f);
            Tag_SetCounter(side, 2);
            r = func_00367310(f->unk22);
            func_003AF540(f, 0, r);
            func_003AFDA8(side, f->unk22, r);
        }
        func_0034CB50();
        func_00373218(a);
        func_00373218(b);
        if (!func_003B4F88(a, b, D_0043A180)) {
            func_003B5150(a, b, side);
        } else {
            if (func_003B4E10(a, b)) {
                f->unk127 = 0;
            }
            func_003D22E0(a, b, 0x5A);
            if (D_005F6DB8) {
                func_003D2330(a, b, 0, 1);
            } else {
                func_003D2330(b, a, 1, 1);
            }
        }
        D_005F64DC = 0x5A;
        D_005F64D8 = 1;
        gRoundPhase = 2;
        func_003465E0();
        func_00344318();
        Fx_LoadFighterTex(a);
        Fx_LoadFighterTex(b);
        D_0043B7F8[2] = func_003B52F0(D_0043B7F8);
        if (D_0043B7F8[0] == 8) {
            func_003E9530(-0x5A);
        }
    case 1:
        func_003533E8(1);
        if (!D_005F5EE0) {
            D_005F64DC--;
        }
        if (D_005F64DC < 0) {
            func_003B5270(a, b);
            next = 2;
            if (D_0043B7F8[0] == 8 && D_0043B7F8[0x1A] == 1) {
                next = 4;
            }
            D_005F64D8 = next;
            D_0043B7F8[2] = 0;
            break;
        }
        func_0036DDD0(a, b);
        if (!D_005F5EE0) {
            Battle_StepMoves(a, b);
        }
        func_003B5270(a, b);
        if (func_003712C0() & 0x800) {
            func_003BAA38();
            D_0043B7F8[2] = 0;
            D_005F64D8 = 4;
            break;
        }
        a->unkC6 = a->unkC4;
        b->unkC6 = b->unkC4;
        func_00372A10(a, b);
        break;
    case 2:
        side = D_005F6DB8;
        f = &gFighters[side];
        m = f->unk0;
        func_0035C8D0(f, D_00440940[side][m].unk0, D_00440940[side][m].unk4);
        f->unk22 = m * 2 + side;
        func_003ADB98(f);
        func_003B4D50(f);
        func_00368168(f);
        Tag_SetCounter(side, 2);
        r = func_00367310(f->unk22);
        func_003AF540(f, 0, r);
        func_003AFDA8(side, f->unk22, r);
        a->unk127 = 1;
        b->unk127 = 1;
        D_005F6DB8 ^= 1;
        D_005F64DC = 0x5A;
        side = D_005F6DB8;
        other = side ^ 1;
        m = gFighters[other].unk2;
        if (gFighters[other].unk402) {
            gFighters[side].unk127 = 0;
        }
        f = &gFighters[side];
        if (f->unk127) {
            func_0035C8D0(f, D_00440940[other][m].unk0, D_00440940[other][m].unk4);
            f->unk22 = m * 2 + other;
            func_003ADB98(f);
            func_0035C850(f, 0);
            func_00368168(f);
            Tag_SetCounter(side, 2);
            r = func_00367310(f->unk22);
            func_003AF540(f, 0, r);
            func_003AFDA8(side, f->unk22, r);
        }
        func_0034CB50();
        func_00373218(a);
        func_00373218(b);
        if (!func_003B4F88(a, b, D_0043A180)) {
            func_003B5150(a, b, D_005F6DB8);
        } else {
            if (func_003B4E10(a, b)) {
                f->unk127 = 0;
            }
            func_003D22E0(a, b, 0x5A);
            if (D_005F6DB8) {
                func_003D2330(a, b, 0, 1);
            } else {
                func_003D2330(b, a, 1, 1);
            }
        }
        D_005F64DC = 0x5A;
        D_005F64D8 = 3;
        func_00373390(a, 8);
        func_00373390(b, 8);
        gRoundPhase = 2;
        D_0043B7F8[2] = func_003B52F0(D_0043B7F8);
        func_003465E0();
        func_00344318();
        Fx_LoadFighterTex(a);
        Fx_LoadFighterTex(b);
        if (D_0043B7F8[0] == 8) {
            func_003E9530(-0x5A);
        }
    case 3:
        func_003533E8(1);
        if (!D_005F5EE0) {
            D_005F64DC--;
        }
        if (D_005F64DC < 0) {
            func_003B5270(a, b);
            D_005F64D8 = 5;
            D_0043B7F8[2] = 0;
            break;
        }
        func_0036DDD0(a, b);
        if (!D_005F5EE0) {
            Battle_StepMoves(a, b);
        }
        func_003B5270(a, b);
        if (func_003712C0() & 0x800) {
            func_003BAA38();
            D_005F64D8 = 4;
            D_0043B7F8[2] = 0;
            break;
        }
        a->unkC6 = a->unkC4;
        b->unkC6 = b->unkC4;
        func_00372A10(a, b);
        break;
    case 4:
        side = D_005F6DBA;
        f = &gFighters[side];
        m = f->unk0;
        func_0035C8D0(f, D_00440940[side][m].unk0, D_00440940[side][m].unk4);
        func_003ADB98(f);
        func_003B4D50(f);
        func_00368168(f);
        Tag_SetCounter(side, 2);
        r = func_00367310(f->unk22);
        func_003AF540(f, 0, r);
        func_003AFDA8(side, f->unk22, r);
        D_005F64DC = 0x5A;
    case 5:
        side = (s16)(D_005F6DBA ^ 1);
        ret = 1;
        f = &gFighters[side];
        m = f->unk0;
        func_0035C8D0(f, D_00440940[side][m].unk0, D_00440940[side][m].unk4);
        func_003ADB98(f);
        func_003B4D50(f);
        func_00368168(f);
        Tag_SetCounter(side, 2);
        r = func_00367310(f->unk22);
        func_003AF540(f, 0, r);
        func_003AFDA8(side, f->unk22, r);
        func_0034CB50();
        D_005F6DBC = 0;
        a->unk127 = 1;
        b->unk127 = 1;
        a->unk4 = 0;
        a->unk51C = 0;
        a->unkC = 0;
        a->unk524 = 0;
        b->unk4 = 0;
        b->unk51C = 0;
        b->unkC = 0;
        b->unk524 = 0;
        func_00373038(a, b);
        func_003B4DC0(a, b);
        gRoundPhase = 0;
        func_00373390(a, 8);
        func_00373390(b, 8);
        func_003465E0();
        func_00344318();
        Fx_LoadFighterTex(a);
        Fx_LoadFighterTex(b);
        D_0043B7F8[2] = func_003B52F0(D_0043B7F8);
        break;
    }
    return ret;
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", func_003B5328);
#endif

void func_003B5C10(Fighter *f, s32 side) {
    s32 v = func_003ADF30(side);
    s16 m = v;
    f->unk0 = v;
    f->unk2 = m ^ 1;
    func_0035C8D0(f, D_00440940[side][m].unk0, D_00440940[side][m].unk4);
    func_003ADB98(f);
    func_0035C850(f, 0);
    func_00368168(f);
}

/* NON_MATCHING: 44 lines, the original keeps the %hi of gFighters / D_00884EE0 in s0 / s3 (six saved registers) */
#if 0
s32 func_003B5CA8(void) {
    Fighter *f0, *f1;
    s32 side, slot, r;

    f1 = &gFighters[1];
    f0 = &gFighters[0];
    func_003B6218();
    side = (s16)f0->unk20;
    func_003B5C10(f0, side);
    slot = gFighters[0].unk0;
    Tag_SetCounter(side, 2);
    slot = side + slot * 2;
    r = func_00367310(slot);
    func_003AF540(f0, 0, r);
    func_003AFDA8(side, slot, r);

    side = (s16)f1->unk20;
    func_003B5C10(f1, side);
    slot = gFighters[1].unk0;
    Tag_SetCounter(side, 2);
    slot = side + slot * 2;
    r = func_00367310(slot);
    func_003AF540(f1, 0, r);
    func_003AFDA8(side, slot, r);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", func_003B5CA8);
#endif

extern s16 D_0043AC38[];
extern s16 D_0043B140[];
void func_0034CB50(void);
void func_003533E8(s32 a);

/* NON_MATCHING: 2 lines, scheduling: the lh of the fighter's side goes one slot earlier */
#if 0
s32 func_003B5DB0(Fighter *a, Fighter *b, s32 mode) {
    Fighter *f;
    Fighter *g;
    Fighter *h;
    Fighter *p;
    TeamMember *m;
    s32 side, other, i, r, v, mi;

    if (mode == 0) {
        if (a->unkB4 > 0) {
            f = b;
        } else if (b->unkB4 > 0) {
            f = a;
        } else {
            return 1;
        }
    } else {
        if (a->unkB4 > 0) {
            f = a;
        } else if (b->unkB4 > 0) {
            f = b;
        } else {
            return 1;
        }
    }
    side = (s16)f->unk20;
    other = (s16)(f->unk20 ^ 1);
    D_005F6DC4 = other;
    g = &gFighters[other];
    mi = g->unk2;
    if (g->unk402 != 0) {
        gFighters[side].unk127 = 0;
    }
    D_005F6DBC = 1;
    if (gFighters[side].unk127 != 0) {
        func_0035C8D0(f, D_00440940[other][mi].unk0, D_00440940[other][mi].unk4);
        f->unk22 = (g->unk22 + 2) & 3;
        func_003ADB98(f);
        func_0035C850(f, 0);
        func_00368168(f);
        if (mode == 0) {
            func_003B4F88(a, b, D_0043AC38);
        } else if (mode == 1) {
            func_003B4F88(a, b, D_0043B140);
        }
        h = &gFighters[side];
        r = func_00367310(h->unk22);
        func_003AF540(h, 0, r);
        func_003AFDA8(side, h->unk22, r);
        Tag_SetCounter(side, 2);
    }
    gFighters[other].unkC = 0;
    gFighters[other].unk4 = 0;
    func_003ADB98(&gFighters[other]);
    if (mode == 0) {
        f->unkC = -0xC00;
        f->unk4 = 0x400;
    } else {
        f->unkC = 0;
        f->unk4 = 0x400;
    }
    a->unk8 = 0;
    b->unk8 = 0;
    g = &gFighters[other];
    if (g->unk402 != 0 || func_003B4E10(a, b) != 0 || func_003B4E00((u8 *)g) != 0) {
        f->unk127 = 0;
        b->unkC = 0;
        a->unkC = 0;
        b->unk4 = 0;
        a->unk4 = 0;
    }
    for (i = 0, p = gFighters; i < 2; i++, p++) {
        if (p->unk3F4 == 0) {
            p->unk3F4 = 100000;
        }
        p->unk3C = 0;
        p->unk1A = 0;
        p->unk150 = 0;
        p->unk152 = 0;
        if (mode == 1) {
            if (p->unk2A == 14) {
                v = 0x1000;
                if (p->unk1E == other) {
                    v = -0x2000;
                }
                p->unk3C = v;
                p->unk1A = v;
            }
            if (p->unk2A == 20) {
                if (p->unk1E == other) {
                    p->unk4 = -0x300;
                } else {
                    p->unk4 = 0x300;
                }
            }
        }
    }
    func_003B4DC0(a, b);
    func_0034CB50();
    func_003533E8(1);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", func_003B5DB0);
#endif

void func_003B60F0(Fighter *a, Fighter *b) {
    Fighter *g = &gFighters[D_005F6DC4];
    Fighter *f = &gFighters[(s16)(D_005F6DC4 ^ 1)];
    s16 c0 = g->unk28;
    s16 c1 = f->unk28;
    ResStart *e = D_0043B5C0;
    u32 i;

    if (a->unkB12C == 0 && b->unkB12C == 0) {
        for (i = 0; i < 39; i++, e++) {
            if (c0 == e->c0 && c1 == e->c1 && a->unk10C == e->move && b->unk10C == e->move) {
                f->unk4 += e->dx;
                f->unkC += e->dz;
                func_003B4D88(f);
            }
        }
    }
}

void func_003B6218(void) {
    D_005F6DBC = 0;
}

s32 func_003B6220(void) {
    return D_005F6DBC;
}

void func_003B6228(s32 a, u8 *b) {
    if (func_003F1570()) {
        Mode_Change(3);
    }
}

void Mode_Results(void) {
    u8 *s = D_0043B7F8;
    s32 r;

    switch (gSubMode) {
    case 0:
        Bgm_Stop();
        if (D_0043B7F8[0] == 3 || D_0043B7F8[0] == 5 || D_0043B7F8[0] == 8) {
            func_003CD6E8();
        }
        func_003C4AE0();
        gSubMode = 1;
        func_003BAA90();
    case 1:
        r = D_005F5CDC;
        switch (s[0]) {
        case 3:
        case 5:
            if (Results_DrawStageTimes(r) == 1) {
                gMode = 6;
            }
            break;
        case 8:
            func_003B6228(r, s);
            break;
        }
        break;
    case 2:
        Mode_Change(3);
        break;
    }
}

void Time_FramesToClock(u32 t, u32 *min, u32 *sec, u32 *cs) {
    if (t > 0x57E3F) {
        t = 0x57E3F;
    }
    *cs = t * 100 / 60;
    *sec = *cs / 100;
    *min = *sec / 60;
    *sec %= 60;
    *cs %= 100;
}

INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E5C8);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E5F0);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E600);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E610);

/* NON_MATCHING: 62 lines, scheduling of the six divisions and the blink test */
#if 0
void func_003B6408(void) {
    u8 *s = D_0043B7F8;
    s32 col[3];
    s32 k;
    ResColors t = D_0051E5C8;
    s32 a = *(u16 *)(s + 0x40);
    s32 b = *(u16 *)(s + 0x46);
    s32 c = *(u16 *)(s + 0x4C);
    s32 ah = a / 100, al = a % 100;
    s32 bh = b / 100, bl = b % 100;
    s32 ch = c / 100, cl = c % 100;

    col[0] = t.v[ah % 10];
    col[1] = t.v[bh % 10];
    col[2] = t.v[ch % 10];
    if (*(s32 *)(s + 0x50) > 0) {
        (*(s32 *)(s + 0x50))--;
    }
    k = s[0x1F];
    if (k && (*(s32 *)(s + 0x50) & 4)) {
        col[k - 1] = 0xE;
    }
    Text_Printf(D_0051E5F0, 10, 6, col[0], 0xFD, 0x168, al);
    Text_Printf(D_0051E5F0, 10, 6, col[1], 0x151, 0x168, bl);
    Text_Printf(D_0051E600, 10, 6, 9, 0x138, 0x168);
    Text_Printf(D_0051E610, 10, 4, 0xE, 0x11D, 0x186);
    Text_Printf(D_0051E5F0, 10, 4, col[2], 0x12D, 0x19A, cl);
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", func_003B6408);
#endif

typedef struct {
    u8 chr[8];
    u8 sub[0x13];
    u8 n;
    u8 pad1C[2];
} PrTeam;
typedef struct {
    u8 pad0[0x42];
    PrTeam team[2];
} PrResult;
s32 func_003CBFA0(s32 a);
void Sprite_DrawScaled(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);

void func_003B6620(PrResult *r, s32 *a, s32 *b) {
    s32 side, j, k, id, c, spr;
    s32 x = 0xFA;
    s32 step = -0x22;
    s32 *list;
    PrTeam *t;
    u64 cc;

    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 1);
    D_005F6730 = 0xFFE40;
    for (side = 0, list = a; side < 2; side++) {
        t = &r->team[side];
        for (j = 0; j < t->n; j++) {
            id = t->chr[j] * 8 + t->sub[j];
            c = 0x80;
            for (k = 0; k < t->n; k++) {
                if (list[k] == id) {
                    c = 0x30;
                    break;
                }
            }
            spr = func_003CBFA0(D_00415840[id]->unk4);
            cc = c;
            D_005F6738 = cc | (cc << 8) | (cc << 16) | 0x80000000;
            Sprite_DrawScaled(1, spr, 0x84, x, 0x154, 0x20, 0x22);
            x += step;
        }
        step = 0x22;
        x = 0x168;
        list = b;
    }
}

/* NON_MATCHING: 2 lines, position of the x << 4 shift */
#if 0
void func_003B6808(s32 x, s32 y, s32 idx, s32 hl) {
    ResQuad q = D_0043FD10[idx];
    q.v[0].x += x << 4;
    q.v[1].x += x << 4;
    q.v[2].x += x << 4;
    q.v[3].x += x << 4;
    q.v[0].y += y << 4;
    q.v[1].y += y << 4;
    q.v[2].y += y << 4;
    q.v[3].y += y << 4;
    if (hl) {
        q.v[0].r = q.v[0].g = q.v[0].b = 0x20;
        q.v[1].r = q.v[1].g = q.v[1].b = 0x20;
        q.v[2].r = q.v[2].g = q.v[2].b = 0x20;
        q.v[3].r = q.v[3].g = q.v[3].b = 0x20;
        if (q.v[0].a == 0) {
            q.v[0].a = 0;
            q.v[1].a = 0x80;
            q.v[3].a = 0x80;
            q.v[2].a = 0;
        }
        if (q.v[1].a == 0) {
            q.v[1].a = 0;
            q.v[0].a = 0x80;
            q.v[2].a = 0x80;
            q.v[3].a = 0;
        }
    }
    func_003BB680(&q, 0xFFE00, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", func_003B6808);
#endif

void func_003B6968(s32 x, s32 y, s32 row, s32 col, s32 idx, s32 fl) {
    s32 a = 0x80;
    s32 spr, sx, sy;
    u64 c;

    if (idx >= 0) {
        if ((fl & 0x8000) && (fl & 0xFFF)) {
            a = 0x30;
        }
        spr = D_00415840[idx]->unk4;
        Sprite_SetFilter(1, 1);
        Sprite_SetBlend(1, 1);
        D_005F6730 = 0xFFE40;
        D_005F6738 = 0x80808080;
        sx = x + D_0043B798[row][col].x;
        sy = y + D_0043B798[row][col].y;
        func_003CC078(sx, sy, 0);
        c = a;
        D_005F6738 = c | (c << 8) | (c << 16) | 0x80000000;
        func_003CBFB8(spr, sx, sy, 0);
    }
}

void func_003B6AC0(s32 x, s32 y, s32 row, s32 col, s32 idx, s32 fl) {
    s32 a = 0x80;
    s32 spr, sx, sy;
    u64 c;

    if (idx >= 0) {
        if ((fl & 0x8000) && (fl & 0xFFF)) {
            a = 0x30;
        }
        spr = D_00415840[idx]->unk4;
        sx = x + D_0043B798[row][col + 3].x;
        sy = y + D_0043B798[row][col + 3].y;
        Sprite_SetFilter(1, 0);
        Sprite_SetBlend(1, 1);
        c = a;
        D_005F6738 = c | (c << 8) | (c << 16) | 0x80000000;
        D_005F6730 = 0xFFE40;
        func_003CF1C0(1, spr, sx, sy);
        D_005F6738 = 0x80808080;
    }
}

s32 func_003B6BD8(void) {
    return (func_003712C0() & 0x8F4) != 0;
}

void func_003B6C00(void) {
    s32 i;
    D_005F62AC = 0;
    D_005F6DC8 = 0;
    D_005F6DCC = 0;
    D_005F6DD0 = 0;
    D_005F6DD4 = 0;
    D_005F6DD6 = 0;
    D_005F6DD2 = 0;
    D_005F5C74 = 0;
    for (i = 0; i < 15; i++) {
        D_008D7EA8[i] = -0x78;
        D_008D7EE8[i] = 0xE0;
    }
}

INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E620);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E640);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E660);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E670);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E680);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E698);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E6B0);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E6C8);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E6E8);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E6F8);

typedef struct {
    s32 v[8];
} PrList;
extern const PrList D_0051E620;
extern const char D_0051E640[];
extern const char D_0051E660[];
extern const char D_0051E670[];
extern const char D_0051E680[];
extern const char D_0051E698[];
extern const char D_0051E6B0[];
extern const char D_0051E6C8[];
extern const char D_0051E6E8[];
extern const char D_0051E6F8[];
extern u32 D_00415718[];
extern u32 D_00415728[];
extern s16 D_005F6DD8;
extern s16 D_005F6DDA;
extern s16 D_005F6DDC;
s32 func_003B00F0(s32 port);
void func_003BF180(u8 *p);
void func_003C9358(s32 a);
void func_003DBAA8(u32 x0, u32 x1, u32 y0, u32 y1);
s32 func_0035A1E8(u8 *o);
void func_003B6808(s32 x, s32 y, s32 id, s32 on);

/* NON_MATCHING: 200 lines, register allocation (i / row base swapped), func_003B6C00 is called with g in the original, flag-test scheduling */
#if 0
s32 func_003B6C68(PrSetup *g) {
    PrList l0 = D_0051E620;
    PrList l1 = D_0051E620;
    s32 *p1;
    s32 trig, held, i, rows, v, step, sel, x, y, n, k, col, res;
    s32 cnt[2];
    s32 val[2];
    s32 flag[2];
    s32 hit[2];
    PrRec *r;

    if (!D_005F6DCC) {
        D_005F6DD2 += 2;
    }
    D_005F6DDC = 0;
    D_005F6DDA = 0;
    D_005F6DD8 = 0;
    trig = 0;
    held = 0;
    for (i = 0; i < 4; i++) {
        if (func_003B00F0(i)) {
            held |= D_00415718[i];
            trig |= D_00415728[i];
        }
    }
    if (trig & 0x100) {
        func_003B6C00();
        D_005F62AC = 0x40;
        return 0;
    }
    D_005F6730 = 0xFFB00;
    Sprite_SetBlend(1, 0);
    func_003BF180((u8 *)g);
    Text_Printf(D_0051E640, 10, 5, 0xF, 0x87, 0x28);
    func_003C9358(2);
    if (D_005F6DCC && g->cnt >= 3) {
        if (D_005F6DD6 == 0) {
            D_005F6DD6 = (((held >> 14) & 1) - ((held >> 12) & 1)) * 0x50;
        }
        v = D_005F6DD6;
        if (v > 0) {
            step = -8;
        } else if (v < 0) {
            step = 8;
        } else {
            step = 0;
        }
        D_005F6DD6 = v + step;
        D_005F6DD4 += step;
        if (D_005F6DD4 < 0) {
            D_005F6DD4 = 0;
        } else if ((g->cnt - 3) * 0x50 < D_005F6DD4) {
            D_005F6DD4 = (g->cnt - 3) * 0x50;
        }
    }
    rows = D_005F6DD2 / 0x50;
    if (g->cnt >= 3) {
        if (g->cnt < rows) {
            rows = g->cnt;
        }
    } else if (rows >= 4) {
        rows = 3;
    }
    func_003DBAA8(0, 0x27F, 0x40, 0x128);
    cnt[1] = 0;
    cnt[0] = 0;
    p1 = l1.v;
    for (i = 0; i < rows; i++) {
        D_008D7EA8[i] += 0xC;
        sel = 0;
        if (D_008D7EA8[i] >= 0x96) {
            if (i < g->cnt && D_008D7EE8[i] == 0xE0) {
                Snd_PlaySe(i == g->cnt - 1 ? 5 : 4, 0);
            }
            if (!D_005F6DCC) {
                D_008D7EE8[i] -= 2;
            }
            if (g->cnt >= 3) {
                if (i == g->cnt - 1) {
                    D_005F6DCC = 1;
                }
            } else if (i >= 2) {
                D_005F6DCC = 1;
            }
            D_008D7EA8[i] = 0x96;
            sel = 0x8000;
        }
        if (i >= g->cnt) {
            continue;
        }
        y = D_008D7EE8[i] + D_005F6DD4;
        x = D_008D7EA8[i] - 0x78;
        r = &g->rec[i];
        n = 0;
        val[0] = -1;
        hit[1] = 0;
        hit[0] = 0;
        flag[1] = 0;
        flag[0] = 0;
        val[1] = -1;
        if (r->c[0] >= 0) {
            if ((r->w[0] == 0 && r->w[2] == 0) || (r->w[1] == 0 && r->w[3] == 0)) {
                flag[n] = 1;
            }
            val[n] = r->c[0];
            n = 1;
        }
        if (r->c[1] >= 0) {
            if ((r->w[0] == 0 && r->w[2] == 1) || (r->w[1] == 0 && r->w[3] == 1)) {
                flag[n] = 1;
            }
            val[n] = r->c[1];
            n++;
        }
        if (sel && (flag[0] || flag[1])) {
            hit[0] = 1;
            if (flag[0]) {
                l0.v[cnt[0]] = val[0];
            } else {
                l0.v[cnt[0]] = val[1];
            }
            cnt[0]++;
        }
        func_003B6808(x, y, 0x17, sel && (flag[0] || flag[1]));
        if (n < 2) {
            func_003B6968(x, y, 0, 2, val[0], sel | flag[0]);
            func_003B6AC0(x, y, 0, 2, val[0], sel | flag[0]);
        } else {
            func_003B6968(x, y, 0, 0, val[0], sel | flag[0]);
            func_003B6AC0(x, y, 0, 0, val[0], sel | flag[0]);
            func_003B6968(x, y, 0, 1, val[1], sel | flag[1]);
            func_003B6AC0(x, y, 0, 1, val[1], sel | flag[1]);
        }
        x = 0x208 - D_008D7EA8[i];
        n = 0;
        flag[1] = 0;
        flag[0] = 0;
        val[0] = -1;
        val[1] = -1;
        if (r->c[2] >= 0) {
            if ((r->w[0] == 1 && r->w[2] == 0) || (r->w[1] == 1 && r->w[3] == 0)) {
                flag[n] = 1;
            }
            val[n] = r->c[2];
            n = 1;
        }
        if (r->c[3] >= 0) {
            if ((r->w[0] == 1 && r->w[2] == 1) || (r->w[1] == 1 && r->w[3] == 1)) {
                flag[n] = 1;
            }
            val[n] = r->c[3];
            n++;
        }
        if (sel && (flag[0] || flag[1])) {
            hit[1] = 1;
            if (flag[0]) {
                p1[cnt[1]] = val[0];
            } else {
                p1[cnt[1]] = val[1];
            }
            cnt[1]++;
        }
        func_003B6808(x, y, 0x18, sel && (flag[0] || flag[1]));
        if (n < 2) {
            func_003B6968(x, y, 1, 2, val[0], sel | flag[0]);
            func_003B6AC0(x, y, 1, 2, val[0], sel | flag[0]);
        } else {
            func_003B6968(x, y, 1, 0, val[0], sel | flag[0]);
            func_003B6AC0(x, y, 1, 0, val[0], sel | flag[0]);
            func_003B6968(x, y, 1, 1, val[1], sel | flag[1]);
            func_003B6AC0(x, y, 1, 1, val[1], sel | flag[1]);
        }
        Text_Printf(D_0051E660, 0x118, y + (0x96 - D_008D7EA8[i]) / 4 + 0x28, 6);
        Text_Printf(D_0051E670, i > 8 ? 0x12A : 0x136, y + (0x96 - D_008D7EA8[i]) / 4 + 0x3C, 10, 5, 2, i + 1);
        if (hit[0] && hit[1]) {
            D_005F6DDC++;
        } else if (hit[1]) {
            D_005F6DD8++;
        } else if (hit[0]) {
            D_005F6DDA++;
        }
    }
    func_003DBAA8(0, 0x27F, 0, 0x1BF);
    if (D_005F6DCC) {
        col = 0x13;
        if (gFrameCount & 4) {
            col = 2;
        }
        res = func_0035A1E8((u8 *)g);
        if (res == 3) {
            Text_Printf(D_0051E610, 10, 10, 2, 0xE8, 0x190);
        } else if (g->unk1A == 2) {
            Text_Printf(D_0051E680, 10, 10, col, 0x86, 0x190, res != 2 ? g->unk1A : 1);
        } else if (g->unk1C + 1 == res) {
            Text_Printf(D_0051E698, 10, 10, col, 0x99, 0x190);
        } else {
            Text_Printf(D_0051E6B0, 10, 10, col, 0xB3, 0x190);
        }
    }
    func_003B6620((PrResult *)g, l0.v, p1);
    Text_Printf(D_0051E6C8, 10, 6, 0x112, 0x14C, 2, D_005F6DD8, 0x137, 9, 0x152, 2, D_005F6DDA);
    Text_Printf(D_0051E6E8, 10, 4, 0x11C, 0x168, 0xE);
    Text_Printf(D_0051E6F8, 10, 4, 0x134, 0x17C, 2, D_005F6DDC);
    return func_003B6BD8() != 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", func_003B6C68);
#endif

/* NON_MATCHING: 35 lines, store order of the camera and fighter resets */
#if 0
void func_003B77B8(void) {
    func_003DB578();
    func_0038DBC0();
    func_003A9510(0);
    gFighters[0].unk127 = 0;
    gFighters[1].unk127 = 0;
    D_005F6DDE = 0xC00;
    D_00415808.w[5] = gSinTable[0x1000] * 5000 / 4096;
    D_00415808.w[6] = -0x578;
    D_00415808.w[7] = gSinTable[0xC00] * 5000 / 4096;
    D_00415808.w[0] = 0;
    D_00415808.w[2] = 0;
    gFighters[0].unk51C = 0;
    gFighters[0].unk520 = 0;
    gFighters[0].unk524 = 0;
    gFighters[1].unk51C = 0;
    gFighters[1].unk520 = 0;
    gFighters[1].unk524 = 0;
    D_00415808.w[1] = 0;
    D_005F5EE0 = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", func_003B77B8);
#endif

/* NON_MATCHING: 41 lines, registers of the camera pointer and the sine index */
#if 0
void func_003B7888(void) {
    u16 a;

    func_003ADFB0();
    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        D_005F6DDE -= 3;
        D_00415808.w[1] -= 3;
    }
    a = D_005F6DDE & 0xFFF;
    D_005F6DDE = a;
    D_00415808.w[5] = gSinTable[a + 0x400] * 5000 / 4096;
    D_00415808.w[7] = gSinTable[a] * 5000 / 4096;
    func_0035B4B0(D_00415808.w, &D_00436530);
    func_00366ED0();
    func_00341218();
    func_003D1040();
    Wind_Update();
    func_003DB690();
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", func_003B7888);
#endif

void func_003B7998(s32 on) {
    if (on) {
        D_005F6DE6 |= 1;
        func_0039C6E8(0, func_0035EA48());
    } else {
        D_005F6DE6 &= ~1;
    }
}

s32 func_003B79E0(void) {
    s32 r = 0;
    if (D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
        u32 t = D_005F6DE6 & 0x10; r = t != 0;
    }
    return r;
}

s32 func_003B7A38(void) {
    return D_005F6DE6 & 1;
}

s32 func_003B7A48(void) {
    return D_005F6DE6 & 0x20;
}

void func_003B7A58(void) {
    D_005F5CC8 = 0;
    D_005F5CCA = 0;
    __asm__ volatile(""); /* fake */
}

void func_003B7A68(void) {
    Bgm_Stop();
    func_0035D090();
    File_WaitAll();
    Mode_Change(3);
    func_003B7A58();
    D_005F5F08 = 0;
    D_005F5EE4 = 0;
    D_005F5EE0 = 0;
    func_003BAA90();
}

void func_003B7AB0(void) {
    D_005F6DE6 = 0;
}

void func_003B7AB8(void) {
    D_005F6DE0 = 0;
    D_005F6DE6 = 0;
}

void func_003B7AC8(void) {
    D_005F6DE6 = 0;
    func_003B7998(0);
    Snd_PlaySe(4, 0);
    func_003BAA38();
    D_0043B7F8[2] = 0;
    if (D_005F6DE0 == 1) {
        D_005F6DE0 = 0;
    }
}

/* fake: the u64 return keeps the original jal + epilogue instead of a tail call */
u64 func_003B7B18(void) {
    return func_00361B28();
}

s32 func_003B7B38(void) {
    return D_005F6DE6;
}

void func_003B7B40(void) {
    D_0043B7F8[2] = func_003B52F0(D_0043B7F8);
    D_005F6DE6 = 0;
    D_005F5F08 = 0;
    D_005F5EE4 = 0;
    D_005F5EE0 = 0;
    func_0036C490(&gFighters[0]);
    func_0036C490(&gFighters[1]);
    Options_Apply();
}

INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E710);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E728);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E740);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E750);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E760);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E770);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E780);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E790);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E7A8);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E7C8);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E7D8);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E7E8);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E800);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E818);
typedef struct {
    s8 c[4];
} PsKeys;
typedef struct {
    const char *s[8];
} PsLabels;
extern PsKeys D_005F64E0;
extern const PsLabels D_0051E7A8;
extern const char D_0051E710[];
extern const char D_0051E728[];
extern const char D_0051E7C8[];
extern const char D_0051E7D8[];
extern const char D_0051E7E8[];
extern const char D_0051E800[];
extern const char D_0051E818[];
extern u32 D_00415728[];
extern u32 D_00415738[];
extern s16 D_005F6DE4;
s32 func_0039C738(void);
s32 func_003BB938(void);
s32 func_003AFFB8(s32 i, s32 a);
s32 func_003BFEF8(void);
void func_003DC908(void);
void func_003B4930(void);
void func_003B2D78(void);
void func_003DD098(void);
void Ui_DrawWindow(s32 x, s32 y, s32 w, s32 h, s32 z, u32 c0, u32 c1);
s32 Pad_IsMultitap(void);
s32 func_003B0340(void);
s32 func_003B1468(void);
void func_003DEB80(s32 a);
void func_003DBCF0(s32 a, s32 y, s32 w, s32 h, s32 z, u32 color);
void func_003E5268(s32 x, s32 y, s32 id, u32 color);
void func_0035A518(void);
void func_003B4938(s32 pad, s32 trig);
s32 Options_ControllerSetup(void);
s32 func_003F3118(s16 trig);
s32 func_003F1CB8(s16 held, s16 trig);
void func_003BB890(s32 a, s32 x, s32 y, s32 b);
void *memset(void *p, s32 c, u32 n);

typedef struct {
    s32 move;
    s32 ok;
} PsSnd;

INCLUDE_RODATA("asm/nonmatchings/pause_results", jtbl_0051E830);

/* NON_MATCHING: 285 lines, register allocation (trig / delta in other saved registers), branch layout of the cursor skip loops */
#if 0
void Pause_Update(void) {
    PsSnd snd = { 0 };
    PsKeys k;
    PsLabels lab;
    ResQuad q;
    s32 held;
    s32 opt = 0;
    const char **p;
    s32 side, trig, d, i, y, col, w, h, cancel;

    if (func_0039C738()) {
        return;
    }
    if (func_003BB938()) {
        return;
    }
    if (D_005F5F08) {
        side = (D_005F5F08 & 1) ^ 1;
        trig = D_00415728[func_003AFFB8(side, gFighters[side].unk0)];
        held = D_00415738[func_003AFFB8(side, gFighters[side].unk0)];
        d = ((trig >> 14) & 1) - ((trig >> 12) & 1);
        if (D_005F6DE6) {
            d = 0;
        }
        if (d) {
            snd.move |= 1;
        }
        D_005F6DE0 += d;
        if (D_0043B7F8[0] == 8) {
            while ((u32)(D_005F6DE0 - 2) < 2) {
                D_005F6DE0 += d;
            }
        } else {
            while ((u32)(D_005F6DE0 - 4) < 2) {
                D_005F6DE0 += d;
            }
        }
        if (D_005F6DE0 == 3) {
            if (d == 0 && !func_003B7B18() && !(D_005F6DE6 & 8)) {
                d = 1;
                snd.move |= 1;
            }
            if (D_005F6DE0 == 3 && (!func_003BFEF8() || !func_003B7B18())) {
                do {
                    D_005F6DE0 += d;
                } while ((u32)(D_005F6DE0 - 4) < 2);
            }
        }
        if (D_005F6DE0 < 0) {
            D_005F6DE0 = 7;
        } else if (D_005F6DE0 >= 8) {
            D_005F6DE0 = 0;
        }
        if (d == 0 && (trig & 0xFC) && !D_005F6DE6) {
            switch (D_005F6DE0) {
            case 0:
                snd.move |= 1;
                func_003B7B40();
                break;
            case 1:
                D_005F6DE6 |= 8;
                snd.move |= 1;
                trig &= ~0xFC;
                D_005F6DE4 = 0;
                break;
            case 2:
                D_005F6DE6 |= 2;
                snd.ok |= 1;
                func_003DC908();
                break;
            case 3:
                D_005F6DE6 |= 8;
                snd.ok |= 1;
                trig &= ~0xFC;
                D_005F6DE4 = 0;
                break;
            case 4:
                D_005F6DE6 |= 0x20;
                snd.ok |= 1;
                trig &= ~0xFC;
                break;
            case 5:
                D_005F6DE6 |= 0x40;
                snd.ok |= 1;
                trig &= ~0xFC;
                break;
            case 6:
                D_005F6DE6 |= 4;
                snd.ok |= 1;
                trig &= ~0xFC;
                func_003B4930();
                break;
            case 7:
                opt = 1;
                trig &= ~0xFC;
                func_003B2D78();
                D_005F6DE6 |= 0x10;
                snd.ok |= 1;
                break;
            }
        }
        if (D_005F6DE6 & 1) {
            func_003B7998(0);
            D_005F5C74 = 1;
            D_005F6DE6 &= ~8;
        } else if (D_005F6DE6 & 2) {
            func_003DD098();
            if (!snd.ok && (trig & 0x8FC)) {
                D_005F6DE0 = 2;
                D_005F6DE6 &= ~2;
                snd.move |= 1;
            }
        } else if (D_005F6DE6 & 4) {
            if (trig & 0x800) {
                D_005F6DE6 &= ~4;
                snd.move |= 1;
            }
        } else if (!D_005F6DE6) {
            h = 0x96;
            if (!func_003BFEF8()) {
                h = D_0043B7F8[0] != 8 ? 0x82 : 0x96;
            }
            Ui_DrawWindow(0xA0, 0x82, 0x140, h, 0xFFEE, 0x4040, 0x4040);
            if ((gFrameCount & 0x30) || D_005F5EE0) {
                k = D_005F64E0;
                if (Pad_IsMultitap()) {
                    Text_Printf(D_0051E710, 10, 5, 0xE1, 0x96, 0, 0x18B,
                                k.c[func_003AFFB8(side, gFighters[side].unk0)]);
                } else if (func_003B0340()) {
                    Text_Printf(D_0051E728, 10, 5, 0xE9, 0x96, 0, func_003AFFB8(side, gFighters[side].unk0) + 1);
                } else {
                    Text_Printf(D_0051E728, 10, 5, 0xE9, 0x96, 0, side + 1);
                }
            }
            lab = D_0051E7A8;
            if (func_003B1468()) {
                func_003DEB80(0);
                func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
                func_003E5268(0x1E, 0x186, D_005F6DE0 + 0x1F, 0x60606080);
            }
            y = 0xB4;
            p = lab.s;
            for (i = 0; i < 8; i++) {
                if (i == 3 && !func_003BFEF8()) {
                    continue;
                }
                if ((u32)(D_0043B7F8[0] == 8 ? i - 2 : i - 4) < 2) {
                    continue;
                }
                col = 5;
                if (D_005F6DE0 == i) {
                    w = Text_Printf(D_0051E7C8, 10, 4, p[i]);
                    q = D_0043FD10[21];
                    col = 0xB;
                    q.v[0].y += y << 4;
                    q.v[1].y += y << 4;
                    q.v[2].y += y << 4;
                    q.v[3].y += y << 4;
                    q.v[3].x = ((w + 0xDC) << 4) + 0x6C00;
                    q.v[0].x = 0x7740;
                    q.v[2].x = 0x7740;
                    q.v[1].x = ((w + 0xDC) << 4) + 0x6C00;
                    func_003BB680(&q, 0xFFEF0, 1);
                }
                if (i == 3 && !func_003B7B18()) {
                    col = 0x16;
                }
                Text_Printf(D_0051E7D8, 10, 4, 0xB4, y, col, p[i]);
                y += 0x14;
            }
        }
        if (D_005F6DE6 & 8) {
            cancel = 0;
            if (trig & 0xA000) {
                cancel = 1;
                snd.move |= 1;
                D_005F6DE4 ^= 1;
            }
            if (D_005F6DE0 == 3 && !func_003B7B18()) {
                D_005F6DE0 = 6;
                D_005F6DE6 &= ~8;
                snd.move |= 1;
            } else if ((trig & 0xFC) && !cancel) {
                if (D_005F6DE4) {
                    snd.ok |= 1;
                    if (D_005F6DE0 == 3) {
                        D_005F5C74 = 0;
                        func_003B7998(1);
                    } else {
                        func_0035A518();
                        Snd_PlaySe(4, 0);
                        return;
                    }
                } else {
                    D_005F6DE6 &= ~8;
                    snd.move |= 1;
                }
            }
            if (!(D_005F6DE6 & 1)) {
                Ui_DrawWindow(0xF0, 0xCE, 0xA0, 0x28, 0xFFEE, 0x4040, 0x4040);
                if (D_005F6DE0 == 3) {
                    Text_Printf(D_0051E7E8, 10, 4, 0xFB, 0xE6, 0x13);
                    if (func_003B1468()) {
                        func_003DEB80(0);
                        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
                        func_003E5268(0x1E, 0x186, 0x27, 0x60606080);
                    }
                } else {
                    Text_Printf(D_0051E800, 10, 4, 0x10E, 0xE6, 0x13);
                    if (func_003B1468()) {
                        func_003DEB80(0);
                        func_003DBCF0(0, 0x185, 0x280, 0x30, 0xFFFE, 0x18181860);
                        func_003E5268(0x1E, 0x186, 0x28, 0x60606080);
                    }
                }
                Text_Printf(D_0051E818, 0x102, 0xFA, 10, 4, D_005F6DE4 == 1 ? 0xB : 0x16, D_005F6DE4 == 0 ? 0xB : 0x16);
                if (D_005F6DE4) {
                    func_003BB890(0, 0x140, 0xFA, 5);
                } else {
                    func_003BB890(1, 0x140, 0xFA, 5);
                }
            }
            if (trig & 0x800) {
                D_005F6DE6 &= ~8;
                snd.move |= 1;
            }
        }
        if (D_005F6DE6 & 4) {
            func_003B4938(held, trig);
            if (trig & 0xEC) {
                D_005F6DE6 &= ~4;
            }
            if (!(D_005F6DE6 & 4)) {
                snd.move |= 1;
            }
        }
        if ((D_005F6DE6 & 0x10) && !opt) {
            if (Options_ControllerSetup()) {
                D_005F6DE6 &= ~0x10;
                snd.move |= 1;
            }
        }
        if (D_005F6DE6 & 0x20) {
            if (func_003F3118(trig)) {
                snd.move |= 1;
                D_005F6DE6 &= ~0x20;
            }
        } else if (D_005F6DE6 & 0x40) {
            if (func_003F1CB8(held, trig)) {
                snd.move |= 1;
                D_005F6DE6 &= ~0x40;
            }
        }
    }
    if (snd.ok) {
        Snd_PlaySe(5, 0);
    } else if (snd.move) {
        Snd_PlaySe(3, 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/pause_results", Pause_Update);
#endif

INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E850);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E878);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E8A0);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E8F0);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E918);
INCLUDE_RODATA("asm/nonmatchings/pause_results", D_0051E940);

u8 *func_003B8760(void) {
    return D_01859080;
}

u8 *func_003B8770(void) {
    return D_005F6DE8;
}

u8 *func_003B8778(void) {
    return D_176A80;
}

s32 func_003B8788(void) {
    return D_005F6DEC;
}

s32 func_003B8790(void) {
    return D_005F6DF0;
}

s32 func_003B8798(s32 i) {
    return D_01FAB050[i];
}

s32 func_003B87B0(s32 i) {
    return D_01FAB068[i];
}

s32 func_003B87C8(s32 i) {
    return D_01FAB080[i];
}

s32 func_003B87E0(s32 i) {
    return D_01FAB098[i];
}

s32 func_003B87F8(s32 i) {
    return D_01FAB0B0[i];
}

s32 func_003B8810(void) {
    return D_005F6DF8;
}

s32 func_003B8818(void) {
    return D_005F6DFC;
}

s32 func_003B8820(void) {
    return D_005F6E00;
}

s32 func_003B8828(void) {
    return D_005F6DF4;
}

s32 func_003B8830(void) {
    return D_005F64F8;
}

s32 func_003B8838(void) {
    return D_005F6E04;
}

void func_003B8840(u8 *p) {
    if (D_005F6DE8 != p) {
        D_005F6DE8 = p;
        D_005F64FC = File_Request(0x85, (u32)p, 0, 0);
        D_005F6500 = 0;
        func_00367FF0();
    } else if (D_005F6500 != 0) {
        if (D_005F64FC >= 0) {
            File_WaitRelease(&D_005F64FC);
        }
        D_005F6500 = 0;
        func_00367FF0();
    }
}

void func_003B88B8(void) {
    if (D_005F6DE8 != D_012C9BA0) {
        D_005F6DE8 = D_012C9BA0;
        D_005F64FC = File_Request(0x85, (u32)D_012C9BA0, 0, 0);
        D_005F6500 = 1;
    }
}

void func_003B8900(void) {
    func_003B8840(D_012C9BA0);
}

void func_003B8910(void) {
    s32 i;
    D_005F6DEC = 0;
    D_005F6DF4 = 0;
    D_005F6E04 = 0;
    D_005F6DF0 = 0;
    for (i = 0; i < 6; i++) {
        D_01FAB050[i] = 0;
        D_01FAB068[i] = 0;
        D_01FAB080[i] = 0;
        D_01FAB098[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        D_01FAB0B0[i] = 0;
    }
    D_005F6DF8 = 0;
    D_005F6E00 = 0;
    D_005F6504 = 0;
    D_005F64F8 = 0;
}

void func_003B89B8(void) {
}

void func_003B89C0(void) {
    func_003B8910();
    D_005F6DF4 = (s32)D_008D7F80;
    D_005F6DEC = (s32)D_00FCD2B0;
    func_003B89B8();
}

void func_003B89F8(void) {
    func_003B8910();
    D_005F6504 = (s32)D_008D7F80;
    func_003638F0();
    func_003B89B8();
    D_005F6DE8 = 0;
    func_003A9510(0);
    func_003A7558();
    func_003A87E8();
    D_005F6DFC = 0;
}

void func_003B8A50(void) {
    func_003B8910();
    D_005F6E04 = (s32)D_008D7F80;
    func_003638F0();
    func_003B89B8();
    func_003A9510(0);
    func_003A7558();
    func_003A87E8();
    D_005F6DFC = 0;
}

void func_003B8AA0(void) {
    s32 i;
    func_003B8910();
    for (i = 0; i < 4; i++) {
        D_01FAB050[i] = (s32)&D_008D7F80[i * 0xA0];
        D_01FAB068[i] = (s32)&D_008D7F80[0x280 + i * 0x294];
        D_01FAB080[i] = (s32)&D_008D7F80[0xCD0 + i * 0x6C];
        D_01FAB098[i] = (s32)&D_008D7F80[0xE80 + i * 0x1A5660];
    }
    for (i = 0; i < 2; i++) {
        D_01FAB0B0[i] = (s32)&D_00F6E780[i * 0x798];
    }
    D_005F6DEC = (s32)D_00FCD2B0;
    D_005F6DF0 = (s32)D_012BDBA0;
    func_003638F0();
    func_003B8840(D_012C9BA0);
    func_003B89B8();
    func_003A9510(0);
    func_003A7558();
    func_003A87E8();
    D_005F6DFC = 0;
}

void func_003B8C08(void) {
    s32 i;
    func_003B8910();
    for (i = 0; i < 4; i++) {
        D_01FAB050[i] = (s32)&D_008D7F80[i * 0xA0];
        D_01FAB068[i] = (s32)&D_008D7F80[0x280 + i * 0x294];
        D_01FAB080[i] = (s32)&D_008D7F80[0xCD0 + i * 0x6C];
        D_01FAB098[i] = (s32)&D_008D7F80[0xE80 + i * 0x1A5660];
    }
    for (i = 0; i < 4; i++) {
        D_01FAB0B0[i] = (s32)&D_00F6E780[i * 0x798];
    }
    D_005F64F8 = (s32)D_00F705E0;
    func_003638F0();
    func_003B8840(D_012C9BA0);
    func_003B89B8();
    func_003A9510(0);
    func_003A7558();
    func_003A87E8();
    D_005F6DFC = 0;
}

void func_003B8D60(void) {
    s32 i;
    func_003B8910();
    for (i = 0; i < 6; i++) {
        D_01FAB050[i] = (s32)&D_008D7F80[i * 0xA0];
        D_01FAB068[i] = (s32)&D_008D7F80[0x3C0 + i * 0x294];
        D_01FAB080[i] = (s32)&D_008D7F80[0x1340 + i * 0x6C];
        D_01FAB098[i] = (s32)&D_008D7F80[0x15D0 + i * 0x1A5660];
    }
    for (i = 0; i < 2; i++) {
        D_01FAB0B0[i] = (s32)&D_012B9B90[i * 0x798];
    }
    D_005F6DEC = (s32)D_013474C0;
    D_005F6DF0 = (s32)D_01637DB0;
    D_005F6DF8 = (s32)D_0164BDB0;
    D_005F6E00 = (s32)D_01834230;
    func_003638F0();
    D_005F6DE8 = 0;
    func_003B89B8();
    func_003A9510(1);
    func_003A7558();
    func_003A87E8();
    D_005F6DFC = 0x1E8480;
}

void func_003B8ED8(void) {
    s32 i;
    func_003B8910();
    for (i = 0; i < 4; i++) {
        D_01FAB050[i] = (s32)&D_008D7F80[i * 0xA0];
        D_01FAB068[i] = (s32)&D_008D7F80[0x280 + i * 0x294];
        D_01FAB080[i] = (s32)&D_008D7F80[0xCD0 + i * 0x6C];
        D_01FAB098[i] = (s32)&D_008D7F80[0xE80 + i * 0x1A5660];
    }
    for (i = 0; i < 2; i++) {
        D_01FAB0B0[i] = (s32)&D_00F6E780[i * 0x798];
    }
    D_005F6DEC = (s32)D_00FCD2B0;
    D_005F6DF0 = (s32)D_012BDBA0;
    D_005F6DF8 = (s32)D_012D1BA0;
    func_003638F0();
    D_005F6DE8 = 0;
    func_003B89B8();
    func_003A9510(1);
    if (D_0043B7F8[0] != 8) {
        func_003A7558();
        func_003A87E8();
    }
    D_005F6DFC = 0x2625A0;
}

void func_003B9058(void) {
    func_003B8910();
    D_005F6DE8 = 0;
    func_003637B0();
}

void func_003B9078(s32 a) {
    func_003B8A50();
    func_00346600();
    func_00343F88();
    if (a) {
        func_003C4828();
    }
    func_003992C8(0);
}

