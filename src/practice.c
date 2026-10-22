/* 0x3790C8-0x37BA88 (module practice, docs/game_overview.md section 4): practice mode, its menus ("FREESTYLE",
 * "VS CPU" ...) and the menu settings D_0042F268. The object continues in src/fighter_core.c (its tables at
 * 0x506938 / 0x5069F8 / 0x506BC0 are in that file's .rodata subsegment). */
#include "common.h"
#include "fighter.h"

/* D_0042F268: the practice menu state and settings. */
typedef struct {
    /* 0x00 */ u8 side;      /* the player's side (D_005F5CBE) */
    /* 0x01 */ u8 other;     /* the dummy's side */
    /* 0x02 */ u16 held;
    /* 0x04 */ u16 pressed;
    /* 0x06 */ u16 unk6;
    /* 0x08 */ u8 unk8;
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u16 page;
    /* 0x0C */ s16 unkC;
    /* 0x0E */ u8 pad0E[2];
    /* 0x10 */ u32 unk10[2];
    /* 0x18 */ u16 unk18;
    /* 0x1A */ u16 unk1A;
    /* 0x1C */ u16 unk1C;
    /* 0x1E */ u16 unk1E;
    /* 0x20 */ s8 unk20;
    /* 0x21 */ u8 unk21;
    /* 0x22 */ u16 unk22;
    /* 0x24 */ u16 unk24;
    /* 0x26 */ s16 unk26;
    /* 0x28 */ u16 unk28;
    /* 0x2A */ s16 unk2A;
    /* 0x2C */ s16 unk2C;
    /* 0x2E */ s16 unk2E;
    /* 0x30 */ u8 unk30[2];
    /* 0x32 */ u8 unk32;
    /* 0x33 */ u8 unk33;
    /* 0x34 */ s8 unk34;
    /* 0x35 */ u8 unk35;
    /* 0x36 */ u8 unk36;
    /* 0x37 */ u8 unk37;
    /* 0x38 */ u8 pad38[0x54];
    /* 0x8C */ u8 unk8C;
    /* 0x8D */ u8 unk8D;
    /* 0x8E */ s16 unk8E;
    /* 0x90 */ s16 unk90;
    /* 0x92 */ u8 unk92;
    /* 0x93 */ u8 unk93;
    /* 0x94 */ u8 unk94;
    /* 0x95 */ u8 unk95;
    /* 0x96 */ u8 pad96[2];
    /* 0x98 */ Fighter *unk98;
    /* 0x9C */ s8 unk9C;
    /* 0x9D */ s8 menu;      /* the current menu (0-3) */
    /* 0x9E */ s8 item1[8][4]; /* items 1-8, one per menu */
    /* 0xBE */ s8 item9[7];  /* items 9-15 */
    /* 0xC5 */ s8 fixed[4][16]; /* items not reset by func_003793C0 */
    /* 0x105 */ u8 pad105[3];
} PracMenu; /* size 0x108 */

/* The practice command lists (see src/fighter_core.c). */
typedef struct {
    /* 0x00 */ u16 unk0;
    /* 0x02 */ u16 unk2;
    /* 0x04 */ u16 unk4;
    /* 0x06 */ u16 unk6;
    /* 0x08 */ u16 unk8;
    /* 0x0A */ u16 unkA;
    /* 0x0C */ u16 unkC;
    /* 0x0E */ u16 unkE;
    /* 0x10 */ u16 unk10;
    /* 0x12 */ u16 unk12[8];
    /* 0x22 */ u16 unk22[16];
} CmdListEntry;

typedef struct {
    /* 0x0000 */ s32 unk0;
    /* 0x0004 */ s32 unk4;
    /* 0x0008 */ CmdListEntry unk8[0x7B];
} CmdList;

/* D_00506938: per item, the option names and their count. */
typedef struct {
    /* 0x0 */ const char **names;
    /* 0x4 */ s16 count;
    /* 0x6 */ s16 unk6;
    /* 0x8 */ s32 unk8;
} PracItemDef;

/* D_00506BC0: per menu, the item defaults and the items it shows. */
typedef struct {
    /* 0x0 */ const s32 *defaults;
    /* 0x4 */ const s32 *items;  /* the items of the menu, first-end */
    /* 0x8 */ s8 first;
    /* 0x9 */ s8 end;
} PracMenuDef;

/* D_0042F140: two practice flag pairs (0x0 / 0x4 and 0x8 / 0xC). */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
} Unk42F140;

extern PracMenu D_0042F268;
extern Unk42F140 D_0042F140[];
extern Unk42F140 D_0042F148[];
extern u8 D_0043B7F8[];
extern u8 D_005F5CBE;
extern Fighter gFighters[4];
extern const PracMenuDef D_00506BC0[];
extern const s16 D_0050693C[];
extern const PracItemDef D_00506938[];
extern const s32 D_005069F8[];
extern const char **D_00415840[];
extern const char D_005F5F68[];
extern const s32 D_005F5F70[];
extern const s32 D_005F5F78[];
extern const s32 D_005F5F80[];
extern const s32 D_005F5F88[];
extern const char D_005F5F90[];
extern const char D_00506408[];
extern s32 D_005F5C48[2];
extern s32 D_005F5F98;
extern s32 D_005F5F08;
extern s16 D_005F5C74;
extern s32 D_00415718[];
extern s32 D_00415728[];
extern s32 D_00415738[];
extern const char D_005062A0[];
extern const char D_005062B8[];
extern const char D_005062D0[];
extern const char D_005062F0[];
extern const char D_00506310[];
extern const char D_00506328[];
extern const char D_00506418[];
extern const char D_00506430[];
extern const char D_00506450[];
extern const char D_00506468[];
extern const char D_00506478[];
extern const char D_00506488[];
extern const char D_00506498[];
extern const char D_005064A8[];

void func_0037D890(void);
void func_0037FA50(void);
void func_0037D870(void);
void func_0037C000(void);
void func_0037BC60(PracMenu *m);
void func_0037EA08(void);
void func_0037EF60(void);
void func_0037BEF0(u8 side);
void func_0037CC58(s32 player);
s32 func_0037C058(void);
s32 func_0037C080(void);
void func_0037DC98(Fighter *f, Fighter *o, s32 kind);
s32 func_0037D880(void);
s32 func_0037DBE8(void);
s32 func_0037DC10(void);
s32 func_0037E670(Fighter *f, Fighter *o);
void func_0037BB70(Fighter *f, s32 on);
void func_0037CAB8(s32 player);
void func_0037CAD8(s32 player);
void func_0037CAF8(s32 player);
void func_0037CB18(s32 player);
struct FighterMove *func_00368218(Fighter *f, s16 move);
s32 func_0037D4F8(Fighter *f);
void func_0037FB30(s32 side);
void Snd_PlaySe(s32 id, s32 player);
s32 func_003E08D0(s32 a);
s32 func_0037FB70(Fighter *f);
s32 func_0036CFC0(u16 buttons);
void func_003DF2F0(s32 x, s32 y, u32 k);
void func_003DEF98(s32 x, s32 y, s32 k);
s32 func_003DD580(s32 slot);
u16 func_0037D4C8(Fighter *f, s32 i);
void Prac_DrawDemoPrompt(s32 side, s32 k);
u16 func_0037CA78(s32 player);
void func_003DEC20(void);
void func_003DD158(s32 side);
void func_003DD188(s32 x, s32 y, s32 side, s32 a, s32 b);
s32 Text_Printf(const char *fmt, ...);
s32 func_0035A670(void);
CmdList *func_0037D460(s32 i);
u16 func_003DFF20(void);
u16 func_003DFF28(void);
void func_0037BAB0(s32 arg0, s32 arg1);
s32 func_0036BDD0(Fighter *f);
void func_0037C1B8(void);
void func_0037C0A8(void);
void func_0037EA58(void);
s32 func_003AFFB8(s32 i, s32 a);
s32 func_003BB938(void);
void func_0037FA78(PracMenu *m);
s32 func_0037EB00(PracMenu *m, Fighter *a, Fighter *b);
s32 func_0037BFB8(PracMenu *m, Fighter *a, Fighter *b);
void func_0037C018(void);
void func_0037C038(void);
void func_0037EFB0(void);
void func_003DD098(void);
void func_00375428(void);
s32 func_0037D710(Fighter *f, s32 i);
void func_0036C468(Fighter *f);
void func_0036C448(Fighter *f);
void func_0036C480(Fighter *f);
void func_0036C490(Fighter *f);
void Prac_ResetPositions(Fighter *f);
void func_0037BAA8(void);
s32 func_0037D798(Fighter *f, s32 i);
void func_00350578(Fighter *f, s32 side);
s32 Prac_IsPlaying(s32 player);
s32 func_0037DEE0(Fighter *f);
s32 func_0037C168(void);
void func_0037D848(void);
s32 func_0037D518(Fighter *f, u32 i);
s32 func_003800D8(s32 n);
void func_0037CA98(s32 player);
void func_0037D828(Fighter *f);
s32 func_0037DFC8(Fighter *f);
void func_00358A30(void);
void func_0035A518(void);
void func_0037BEF8(s32 side);
void func_0037BC98(PracMenu *m, Fighter *f, Fighter *o);
void func_0037BDD8(PracMenu *m);
s32 func_0037DC88(void);
s32 func_0036E160(void);
s32 func_0037C370(Fighter *f);
void *memcpy(void *dst, const void *src, u32 n);
char *strcat(char *dst, const char *src);
s32 sprintf(char *buf, const char *fmt, ...);

u8 func_003790C8(void);
void func_003790D8(void);
void func_00379338(PracMenu *m, s32 menu);
void func_003793C0(PracMenu *m, s32 menu);
s32 func_00379468(s32 item, s32 value);
s32 func_003794A0(s32 item);
s32 func_003794D8(PracMenu *m, s32 menu, u32 item, s32 value, s32 set);
void func_003795B8(void);
s32 func_003795D8(void);
s32 func_00379600(void);
s32 func_00379628(void);
u8 func_00379638(void);
s32 func_00379648(void);
void func_00379658(void);
void func_00379670(void);
void func_00379688(void);
void func_003796A0(void);
void func_003796B8(void);
void func_0037A258(void);
void func_0037A440(Fighter *f);
void func_0037A5C8(PracMenu *m, Fighter *a, Fighter *b);
void func_0037A6D0(PracMenu *m, Fighter *a, Fighter *b);
void func_0037A970(Fighter *f);
void func_0037AA68(Fighter *a, Fighter *b);
s32 func_0037AAF8(PracMenu *m);
void func_0037ABB8(PracMenu *m, s32 menu);
void func_0037ABF0(PracMenu *m, Fighter *a, Fighter *b);
void func_0037AE88(Fighter *f, Fighter *o, s32 kind);
void func_0037AF30(Fighter *f, Fighter *o, s32 kind);
void func_0037AF80(PracMenu *m, Fighter *a, Fighter *b);
void func_0037B478(PracMenu *m);
void func_0037B488(PracMenu *m);
void func_0037B4A8(PracMenu *m);
void func_0037B4F8(PracMenu *m, Fighter *a, Fighter *b);
void func_0037B6C8(void);
s32 func_0037B6D0(s32 x, s32 y, u16 buttons);
void func_0037B738(s32 x, s32 y, u16 buttons);
s32 func_0037B770(Fighter *f);
void func_0037B7A8(s32 n);
s32 func_0037B7B0(void);
void func_0037B7D8(Fighter *f);
void func_0037B870(Fighter *f, s32 flag);
void func_0037B8D8(Fighter *f, s32 a, s32 b);
void func_0037BA30(s32 side, s32 a, s32 b);

static inline s32 Prac_Wrap(s32 v, s32 lo, s32 hi) {
    if (v < lo) {
        v = hi;
    } else if (hi < v) {
        v = lo;
    }
    return v;
}

u8 func_003790C8(void) {
    return D_0042F268.unk8;
}

void func_003790D8(void) {
    PracMenu *m;
    s32 i;
    s32 j;

    func_0037D890();
    m = &D_0042F268;
    m->side = D_005F5CBE;
    m->unk8C = 0;
    m->other = (D_005F5CBE + 1) & 1;
    m->held = 0;
    m->pressed = 0;
    m->unk6 = 0;
    m->unk8 = 0;
    m->unk9 = 0;
    m->unk9C = 0;
    m->page = 0;
    m->unkC = 0;
    for (i = 0; i < 2; i++) {
        m->unk10[i] = 0;
    }
    m->unk18 = 0;
    m->unk1A = 0;
    m->unk1C = 0;
    m->unk1E = 0;
    m->unk20 = 0;
    m->unk21 = 0;
    m->unk22 = 0;
    m->unk24 = 0;
    m->unk26 = 0;
    m->unk28 = 0;
    m->unk2A = 0;
    m->unk2C = 0;
    m->unk2E = 0;
    for (i = 0; i < 2; i++) {
        m->unk30[i] = 0;
    }
    m->unk32 = 0;
    m->unk33 = 0;
    m->unk34 = 0;
    m->unk35 = 0;
    m->unk36 = 0;
    m->unk37 = 0;
    m->unk98 = gFighters;
    m->unk8D = 0;
    m->unk8E = 0;
    m->unk90 = 0;
    m->unk92 = 0;
    m->unk93 = 0;
    m->unk94 = 0;
    m->unk95 = 0;
    func_0037FA50();
    for (i = 0; i < 4; i++) {
        func_00379338(m, i);
    }
    m->menu = 0;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 16; j++) {
            m->fixed[i][j] = 0;
        }
    }
    for (i = 0; i < 4; i++) {
        s32 end = D_00506BC0[i].end;
        const s32 *l;

        j = D_00506BC0[i].first;
        l = D_00506BC0[i].items;
        for (; j < end; j++) {
            m->fixed[i][l[j]] = 1;
        }
    }
    func_0037D870();
    func_003795B8();
    func_0037C000();
    func_0037BC60(m);
    func_0037EA08();
    func_0037EF60();
    func_0037BEF0(m->other);
}

void func_00379338(PracMenu *m, s32 menu) {
    const s32 *p = D_00506BC0[menu].defaults;
    s32 i;

    for (i = 0; i < 16; i++) {
        func_003794D8(m, menu, i, *p, 1);
        p++;
    }
}

void func_003793C0(PracMenu *m, s32 menu) {
    const s32 *p = D_00506BC0[menu].defaults;
    s8 *l = m->fixed[menu];
    s32 i;

    for (i = 0; i < 16; i++) {
        if (*l == 0) {
            func_003794D8(m, menu, i, *p, 1);
        }
        l++;
        p++;
    }
}

s32 func_00379468(s32 item, s32 value) {
    return func_003794D8(&D_0042F268, D_0042F268.menu, item, value, 1);
}

s32 func_003794A0(s32 item) {
    return func_003794D8(&D_0042F268, D_0042F268.menu, item, 0, 0);
}

s32 func_003794D8(PracMenu *m, s32 menu, u32 item, s32 value, s32 set) {
    s8 *p = NULL;

    switch (item) {
    case 0:
        p = &m->menu;
        break;
    case 1:
        p = &m->item1[0][menu];
        break;
    case 2:
        p = &m->item1[1][menu];
        break;
    case 3:
        p = &m->item1[2][menu];
        break;
    case 4:
        p = &m->item1[3][menu];
        break;
    case 5:
        p = &m->item1[4][menu];
        break;
    case 6:
        p = &m->item1[5][menu];
        break;
    case 7:
        p = &m->item1[6][menu];
        break;
    case 8:
        p = &m->item1[7][menu];
        break;
    case 9:
        p = &m->item9[0];
        break;
    case 10:
        p = &m->item9[1];
        break;
    case 11:
        p = &m->item9[2];
        break;
    case 12:
        p = &m->item9[3];
        break;
    case 13:
        p = &m->item9[4];
        break;
    case 14:
        p = &m->item9[5];
        break;
    case 15:
        p = &m->item9[6];
        break;
    }
    if (set == 1) {
        *p = value;
    }
    return *p;
}

void func_003795B8(void) {
    D_0042F140[0].unk0 = 0;
    D_0042F140[0].unk4 = 0;
    D_0042F140[1].unk0 = 0;
    D_0042F140[1].unk4 = 0;
}

s32 func_003795D8(void) {
    if (D_0043B7F8[0] == 7) {
        return D_0042F140[0].unk0;
    }
    return 0;
}

s32 func_00379600(void) {
    if (D_0043B7F8[0] == 7) {
        return D_0042F148[0].unk0;
    }
    return 0;
}

s32 func_00379628(void) {
    return D_0042F140[0].unk4;
}

u8 func_00379638(void) {
    return D_0042F268.side;
}

s32 func_00379648(void) {
    return D_0042F148[0].unk4;
}

void func_00379658(void) {
    D_0042F140[0].unk0 = 1;
    D_0042F140[0].unk4 = 1;
}

void func_00379670(void) {
    D_0042F148[0].unk0 = 1;
    D_0042F148[0].unk4 = 1;
}

void func_00379688(void) {
    D_0042F140[0].unk0 = 1;
    D_0042F140[0].unk4 = 0;
}

void func_003796A0(void) {
    D_0042F148[0].unk0 = 1;
    D_0042F148[0].unk4 = 0;
}


INCLUDE_RODATA("asm/nonmatchings/practice", D_005062A0);
INCLUDE_RODATA("asm/nonmatchings/practice", D_005062B8);
INCLUDE_RODATA("asm/nonmatchings/practice", D_005062D0);
INCLUDE_RODATA("asm/nonmatchings/practice", D_005062F0);
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506310);
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506328);
/* NON_MATCHING: about 30 real lines (130 with relocations): the signed range test on the u16 page, the unk8E choice chain and register allocation */
#if 0
void func_003796B8(void) {
    PracMenu *m = &D_0042F268;
    Fighter *p;
    Fighter *d;
    Fighter *f;
    s32 k;
    s32 n;
    s32 t;
    s32 show;
    s32 busy;
    s32 slot;

    func_003795B8();
    func_0037C000();
    func_0037C1B8();
    n = m->page;
    if (n < 4 && n >= 0 && func_003794A0(0) == 2) {
        func_0037EA58();
    }
    k = func_003AFFB8(D_005F5CBE, gFighters[D_005F5CBE].unk0);
    p = &gFighters[m->side];
    d = &gFighters[m->other];
    p->unk3F4 = p->unk3F8 = p->unk3FC;
    d->unk3F4 = d->unk3F8 = d->unk3FC;
    d->unk400 = 0;
    m->held = D_00415718[k];
    m->pressed = D_00415728[k];
    m->unk6 = D_00415738[k];
    p->unk400 = 0;
    D_005F5C74 = 0;
    if (func_003BB938() != 0) {
        if (func_0036E160() != 0) {
            func_00379670();
        }
        return;
    }
    switch (m->page) {
    case 0:
        if (D_005F5F08 != 0) {
            m->page = 1;
            m->unkC = 1;
        } else {
            m->unk8 = 0;
        }
        func_0037FA78(m);
        switch (func_003794A0(0)) {
        case 0:
            Text_Printf(D_005062A0, 10, 4, 5, 16, 430);
            break;
        case 1:
            Text_Printf(D_005062B8, 10, 4, 5, 16, 430);
            break;
        case 2:
            Text_Printf(D_005062D0, 10, 4, 5, 16, 430);
            if (func_0037EB00(m, p, d) == 0) {
                func_00379658();
                m->page = 1;
            }
            break;
        case 3:
            Text_Printf(D_005062F0, 10, 4, 5, 16, 430);
            Text_Printf(D_00506310, 10, 4, 7, 0x114, 430);
            Text_Printf(D_00506328, 11, 6, 0x17E, 430);
            if ((m->pressed & 0x100) && func_0037BFB8(m, p, d) != 0) {
                func_00379688();
                if (func_003794A0(9) == 0) {
                    m->unk92 = 0;
                    func_0037C018();
                    m->unk98 = p;
                } else {
                    m->unk92 = 1;
                    func_0037C038();
                    m->unk98 = d;
                }
                m->unk90 = 120;
                m->page = 11;
            }
            break;
        }
        break;
    case 1:
        m->unk8 = 1;
        func_0037A5C8(m, p, d);
        func_0037C0A8();
        func_0037EFB0();
        if (func_003794A0(14) == 1) {
            func_00379468(14, 0);
            m->page = 0;
            m->unk8 = 0;
            func_00379688();
        }
        if (func_003794A0(11) == 1) {
            m->unk8 = 0;
            m->page = 3;
        } else {
            if (func_003794A0(15) == 1) {
                func_00379468(15, 0);
                func_0037EA08();
                m->page = 0;
                m->unk8 = 0;
                func_00379688();
            }
            if (m->pressed & 0x8B0) {
                func_00379688();
                m->page = 0;
                m->unkC = 0;
            }
        }
        break;
    case 3:
        func_00379670();
        func_003DD098();
        func_0037B7D8(p);
        if (m->pressed & 0x800) {
            func_00375428();
            func_00379468(11, 0);
            func_00379688();
            m->page = 0;
            m->unkC = 0;
        }
        if (m->pressed & 0xB0) {
            func_00375428();
            func_00379468(11, 0);
            m->page = 1;
        } else if ((m->pressed & 0x40) && func_0037D710(p, func_003DD580(p->unk24)) != 0) {
            func_0036C468(p);
            func_0037BC60(m);
            m->unk8D = 1;
            func_0037B488(m);
            func_0037B478(m);
            slot = func_003DD580(p->unk24);
            if (p->unk28 == 5 && d->unk28 == 2 && slot == 0x76) {
                m->unk8E = 0;
            } else if (p->unk28 == 5 && d->unk28 == 1 && slot == 0x76) {
                m->unk8E = 30;
            } else if ((u16)(d->unk28 - 1) >= 2 && (d->unk2A == 15 || d->unk2A == 33)) {
                m->unk8E = 10;
            } else if ((u16)(d->unk28 - 1) >= 2 && d->unk28 == 8) {
                m->unk8E = 30;
            } else {
                m->unk8E = 5;
            }
            m->page = 4;
        }
        break;
    case 4:
        func_003796A0();
        if (m->unk8D != 0) {
            m->unk8D = 0;
            Prac_ResetPositions(p);
        }
        func_0037BAA8();
        func_0037B870(p, 0);
        if (m->unk8E <= 0) {
            m->unk8E = 0;
            m->page = 5;
        } else {
            m->unk8E--;
        }
        break;
    case 5:
        func_00379670();
        func_0037BAA8();
        func_0037B870(p, 0);
        if (func_0037D798(p, func_003DD580(p->unk24)) != 0) {
            func_0036C468(p);
            func_00350578(p, p->unk1E);
            func_00350578(d, d->unk1E);
            func_003796A0();
            m->page = 6;
        } else {
            m->page = 3;
        }
        break;
    case 6:
        func_003796A0();
        func_0037B870(p, 1);
        if (Prac_IsPlaying(p->unk1E) == 0) {
            func_0036C480(p);
            m->page = 7;
        }
        break;
    case 7:
        func_003796A0();
        func_0037B870(p, 1);
        if (func_0037B770(p) != 0) {
            func_003796A0();
            m->unk8E = 30;
            m->page = 8;
        }
        break;
    case 8:
        func_003796A0();
        func_0037B870(p, 1);
        func_0036C480(p);
        if (m->unk8E <= 0) {
            m->unk8E = 0;
            func_0036C448(p);
            func_0037D870();
            func_0036C490(p);
            func_00379670();
            m->page = 3;
        } else {
            m->unk8E--;
        }
        break;
    case 9:
        func_00379670();
        func_0037B870(p, 1);
        if (m->pressed & 0x40) {
            func_003796A0();
            Prac_ResetPositions(p);
            m->unk8E = 30;
            m->page = 4;
        } else if (m->pressed & 0xB0) {
            func_0036C448(p);
            func_0036C490(p);
            func_0037D870();
            func_00379670();
            m->page = 3;
        }
        break;
    case 10:
        m->page = 3;
        break;
    case 11:
        func_003796A0();
        if (m->unk92 != 0) {
            func_0037C038();
        } else {
            func_0037C018();
        }
        if (func_0037DEE0(m->unk98) != 0) {
            m->page = 12;
        } else if (m->unk90 <= 0) {
            m->unk90 = 0;
            if (func_0037C168() == 0) {
                func_0037D848();
                Prac_ResetPositions(m->unk98);
                m->page = 12;
            }
        } else {
            m->unk90--;
        }
        break;
    case 12:
        func_003796A0();
        f = func_003794A0(9) == 0 ? p : d;
        n = func_0037D4F8(f);
        if (n != 0) {
            t = func_003794A0(10);
            m->unk95 = 0;
            if (t == n && t != 1) {
                k = func_0037D518(f, func_003800D8(t));
                m->unk95 = 1;
            } else {
                k = func_0037D518(f, t);
            }
            if (func_0037D798(f, k) != 0) {
                m->unk94 = k;
                m->unk98 = f;
                m->unk93 = f->unk1E;
                func_0036C468(p);
                func_003796A0();
                func_0037BAA8();
                m->page = 13;
                break;
            }
        }
        m->page = 0;
        break;
    case 13:
        func_003796A0();
        if (m->unk95 == 0) {
            func_0037B8D8(m->unk98, m->unk94, func_0037CA78(m->unk93));
        }
        if (m->pressed & 0x800) {
            func_0037CA98(m->unk98->unk1E);
            func_0037D828(m->unk98);
            func_0036C480(m->unk98);
            func_0036C448(m->unk98);
            func_0037D870();
            func_0036C490(m->unk98);
            func_003796A0();
            m->page = 0;
        } else if (Prac_IsPlaying(m->unk93) == 0 || func_0037DFC8(m->unk98) != 0) {
            func_0037CA98(m->unk98->unk1E);
            func_0036C480(m->unk98);
            m->page = 14;
        }
        break;
    case 14:
        func_003796A0();
        func_0036C448(m->unk98);
        func_0037D870();
        func_0036C490(m->unk98);
        m->page = 0;
        break;
    }
    if (m->unk8 != m->unk9) {
        if (m->unk8 != 0) {
            if (func_003794A0(0) != 2) {
                func_0037EA08();
            }
            func_0037CA98(m->unk98->unk1E);
            func_0037FB30(m->side);
        } else if (func_003794A0(0) == 0 && m->page == 0) {
            func_0037B4A8(m);
        }
        func_00375428();
    }
    m->unk9 = m->unk8;
    if (func_003794A0(12) == 1) {
        func_00379468(12, 0);
        func_00358A30();
    }
    if (func_003794A0(13) == 1) {
        func_00379468(13, 0);
        func_0035A518();
    }
    func_0037BEF8(m->other);
    if (m->unk8 == 0) {
        show = 0;
        if (func_003794A0(6) == 1) {
            busy = 0;
            switch (m->page) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
                show = 0;
                break;
            case 0:
            case 11:
            case 12:
            case 13:
            case 14:
                show = 1;
                break;
            }
            if (show != 0) {
                if (D_005F5F08 != 0 || (func_003795D8() != 0 && func_00379628() != 0) ||
                    (func_00379600() != 0 && func_00379648() != 0)) {
                    busy = 1;
                }
                if (busy == 0) {
                    func_0037BC98(m, p, d);
                }
                func_0037B4F8(m, p, d);
                func_0037BDD8(m);
            }
        }
        if (func_0037D880() != 0) {
            d->unk12E = func_0037DC88() != 0;
        } else {
            t = func_003794A0(5);
            if (t != 1) {
                d->unk12E = 0;
            } else {
                d->unk12E = t;
            }
        }
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/practice", jtbl_00506340);
INCLUDE_RODATA("asm/nonmatchings/practice", jtbl_00506380);
INCLUDE_ASM("asm/nonmatchings/practice", func_003796B8);
#endif

void func_0037A258(void) {
    PracMenu *m = &D_0042F268;
    Fighter *p = &gFighters[D_0042F268.side];
    Fighter *d = &gFighters[m->other];

    if (func_003794A0(1) == 1) {
        func_0037CC58(0);
        func_0037A440(p);
    }
    if (func_0037C058() != 0) {
        p->unk420 = 4;
        func_0037DC98(p, d, 14);
    } else if (func_0037D880() != 0) {
        p->unk420 = 4;
        func_0037DC98(p, d, func_0037DBE8());
    } else {
        func_0037AE88(p, d, func_003794A0(2));
    }
    if (func_0037C080() != 0) {
        d->unk420 = 4;
        func_0037DC98(d, p, 14);
    } else if (func_0037D880() != 0) {
        func_0037AF30(d, p, func_0037DC10());
    } else {
        func_0037AF30(d, p, func_003794A0(3));
    }
    m->unk30[m->side] = func_0037E670(p, d);
    m->unk30[m->other] = func_0037E670(d, p);
    if (func_003794A0(7) == 1) {
        func_0037BB70(p, m->unk30[m->side]);
        func_0037BB70(d, m->unk30[m->other]);
    }
    func_0037AF80(m, p, d);
}

void func_0037A440(Fighter *f) {
    PracMenu *m = &D_0042F268;
    s32 side = D_0042F268.side;
    struct FighterMove *mv;
    u16 id;
    s32 same;
    s32 on;
    s32 chg;

    if (D_005F5C48[side] & 0xF0F4) {
        m->unk34 = 20;
    } else {
        mv = f->unkC0;
        id = mv->unk10;
        if (f->unk10C == id) {
            same = 1;
        } else {
            same = 0;
        }
        if (f->unk188 != NULL) {
            if (f->unk188 == func_00368218(f, f->unk188->unk10)) {
                same = 1;
            }
        } else if (m->unk30[side] == 0) {
            if (mv->unk10 == func_00368218(f, id)->unk10) {
                same = 1;
            }
        }
        if (same) {
            if (--m->unk34 < 0) {
                m->unk34 = 0;
            }
        }
    }
    on = m->unk35 = m->unk34 != 0;
    chg = on ^ m->unk36;
    m->unk36 = on;
    m->unk37 = chg != 0;
    if (m->unk37 != 0 && on != 0) {
        func_0037CAB8(side);
        func_0037CAD8(side);
    }
    if (f->unkE0 > 0) {
        if (f->unk4C0.unk10 != 0) {
            func_0037CAF8(side);
            func_0037CAD8(side);
        }
    } else if (m->unk35 == 0) {
        func_0037CAD8(side);
    }
    func_0037CB18(side);
}

void func_0037A5C8(PracMenu *m, Fighter *a, Fighter *b) {
    s32 cur = m->unk9C;
    u16 pad = m->unk6;
    s32 lo = D_00506BC0[m->menu].first;
    s32 hi = (s8)(D_00506BC0[m->menu].end - 1);
    s32 v = cur;
    s32 r;

    if (pad & 0x1000) {
        v = (s8)(cur - 1);
    } else if (pad & 0x4000) {
        v = (s8)(cur + 1);
    }
    r = v < lo ? hi : (v > hi ? lo : v);
    if (r != cur) {
        m->unk9C = r;
        Snd_PlaySe(3, 0);
        func_003E08D0(-1);
    } else if (func_0037AAF8(m) == 0) {
        func_0037A6D0(m, a, b);
    }
}



/* NON_MATCHING: 2 lines: the wrap's movn picks the other operand (move a1, s0 / movn a1, zero in the original) */
#if 0
void func_0037A6D0(PracMenu *m, Fighter *a, Fighter *b) {
    u32 item = D_00506BC0[func_003794A0(0)].items[m->unk9C];
    s32 max = D_005069F8[item] - 1;
    s32 cur = func_003794A0(item);
    s32 v = cur;
    s32 k;

    switch (item) {
    case 0:
        if (m->pressed & 0x8000) {
            v = cur - 1;
        } else if (m->pressed & 0x2040) {
            v = cur + 1;
        }
        v = v < 0 ? max : (v > max ? 0 : v);
        break;
    case 9:
        if (m->unk6 & 0x8000) {
            v = cur - 1;
        } else if (m->pressed & 0x2040) {
            v = cur + 1;
        }
        v = v < 0 ? max : (v > max ? 0 : v);
        break;
    case 10:
        if (func_003794A0(9) == 0) {
            max = func_0037D4F8(a) - 1;
        } else {
            k = func_0037D4F8(b);
            max = 0;
            if (k != 1) {
                max = k;
            }
        }
        if (m->unk6 & 0x8000) {
            v = cur - 1;
        } else if (m->pressed & 0x2040) {
            v = cur + 1;
        }
        v = v < 0 ? max : (v > max ? 0 : v);
        break;
    case 2:
    case 3:
    case 4:
        if (m->unk6 & 0x8000) {
            v = cur - 1;
        } else if (m->pressed & 0x2040) {
            v = cur + 1;
        }
        v = v < 0 ? max : (v > max ? 0 : v);
        break;
    case 1:
    case 5:
    case 6:
    case 7:
    case 8:
        if (m->pressed & 0xA040) {
            v = (cur + 1) & 1;
        }
        v = v < 0 ? max : (v > max ? 0 : v);
        break;
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        if (m->pressed & 0x40) {
            v = (cur + 1) & 1;
        }
        v = v < 0 ? max : (v > max ? 0 : v);
        break;
    }
    if (m->held & 0x5000) {
        cur = func_003794A0(item);
        v = cur;
    }
    func_00379468(item, v);
    if (v != cur) {
        if (item == 0) {
            func_0037ABB8(m, func_003794A0(0));
        } else {
            if (item == 9) {
                func_0037AA68(a, b);
            }
            Snd_PlaySe(4, 0);
        }
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/practice", jtbl_005063C0);
INCLUDE_ASM("asm/nonmatchings/practice", func_0037A6D0);
#endif

void func_0037A970(Fighter *f) {
    PracMenu *m = &D_0042F268;
    s32 n;
    s32 v;
    s32 k;

    if (func_003794A0(9) == 0 && m->side == f->unk1E) {
        n = func_0037D4F8(f) - 1;
        v = func_003794A0(10);
        v = v < 0 ? n : (n < v ? 0 : v);
        func_00379468(10, v);
    }
    if (func_003794A0(9) == 1 && f->unk1E == m->other) {
        k = func_0037D4F8(f);
        n = 0;
        if (k != 1) {
            n = k;
        }
        v = func_003794A0(10);
        v = v < 0 ? n : (n < v ? 0 : v);
        func_00379468(10, v);
    }
}

/* NON_MATCHING: 15 lines: the original keeps the clamp's 0 in a saved register (s2) set at entry; gcc folds it to the constant */
#if 0
void func_0037AA68(Fighter *a, Fighter *b) {
    s32 lo = 0;
    s32 n;
    s32 v;
    s32 k;

    if (func_003794A0(9) == 0) {
        n = func_0037D4F8(a) - 1;
    } else {
        k = func_0037D4F8(b);
        n = 0;
        if (k != 1) {
            n = k;
        }
    }
    v = func_003794A0(10);
    if (v < lo) {
        v = n;
    } else if (n < v) {
        v = lo;
    }
    func_00379468(10, v);
}
#else
INCLUDE_ASM("asm/nonmatchings/practice", func_0037AA68);
#endif

s32 func_0037AAF8(PracMenu *m) {
    s32 cur = func_003794A0(0);
    s32 v = cur;
    s32 n;

    if ((m->held & 3) == 3) {
        if (m->pressed & 4) {
            v = cur - 1;
        } else if (m->pressed & 8) {
            v = cur + 1;
        }
    }
    if (v < 0) {
        n = D_0050693C[0] - 1;
    } else {
        n = 0;
        if (v <= D_0050693C[0] - 1) {
            n = v;
        }
    }
    func_00379468(0, n);
    if (n != cur) {
        func_0037ABB8(m, n);
        return 1;
    }
    return 0;
}

void func_0037ABB8(PracMenu *m, s32 menu) {
    m->unk9C = 0;
    func_003793C0(m, menu);
    Snd_PlaySe(0x7043, 0);
    func_0037FB30(m->side);
}




/* NON_MATCHING: about 60 lines: the short string copies (lw / lhu / lbu moves of strcpy literals in .sdata; memcpy gives lh / lb) and register allocation */
#if 0
void func_0037ABF0(PracMenu *m, Fighter *a, Fighter *b) {
    char name[0x40] = "";
    char num[0x40] = "";
    s32 row = 6;
    s32 i = 0;
    s32 item;
    s32 v;
    s32 k;
    Fighter *f;
    char *str;

    Text_Printf(D_005F5F68, 6, 1, m->unk9C + 6);
    for (; i < D_00506BC0[m->menu].end; ) {
        item = D_00506BC0[m->menu].items[i];
        switch (item) {
        default:
            str = (char *)D_00506938[item].names[func_003794A0(item)];
            break;
        case 9:
            if (func_003794A0(9) == 0) {
                f = a;
                memcpy(name, D_005F5F70, 7);
            } else {
                f = b;
                memcpy(name, D_005F5F78, 7);
            }
            strcat(name, *D_00415840[f->unk26]);
            str = name;
            break;
        case 10:
            v = func_0037D4F8(func_003794A0(9) != 0 ? b : a);
            if (v == 0) {
                str = num;
                memcpy(num, D_005F5F80, 4);
            } else {
                k = func_003794A0(10);
                str = num;
                if (k == v && k != 1) {
                    memcpy(num, D_005F5F88, 7);
                } else {
                    sprintf(num, D_005F5F90, k + 1);
                }
            }
            break;
        }
        i++;
        Text_Printf(D_00506408, 6, 2, row, str);
        row++;
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506400);
INCLUDE_ASM("asm/nonmatchings/practice", func_0037ABF0);
#endif
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506408);

void func_0037AE88(Fighter *f, Fighter *o, s32 kind) {
    switch (kind) {
    case 0:
        f->unk12C = 1;
        if (func_0037FB70(f) == 0) {
            f->unk420 = 1;
        }
        break;
    case 1:
        f->unk12C = 0;
        f->unk420 = 4;
        func_0037DC98(f, o, 0);
        break;
    case 2:
        f->unk420 = 2;
        f->unk12C = 0;
        break;
    }
}

void func_0037AF30(Fighter *f, Fighter *o, s32 kind) {
    if (kind == 11) {
        f->unk12C = 1;
        f->unk420 = 1;
    } else if (kind == 12) {
        f->unk12C = 0;
        f->unk420 = 2;
    } else {
        f->unk420 = 4;
        f->unk12C = 0;
        func_0037DC98(f, o, kind);
    }
}


void func_0037AF80(PracMenu *m, Fighter *a, Fighter *b) {
    u16 *c;
    s32 hit;
    s32 n;
    s32 r;

    if (b->unk144 != 0) {
        m->unk18 = b->unk144;
    }
    if (b->unk142 != 0) {
        m->unk1A = b->unk142;
    }
    m->unk10[a->unk1E] += a->unk144;
    if (m->unk10[a->unk1E] > 99999) {
        m->unk10[a->unk1E] = 99999;
    }
    m->unk10[b->unk1E] += b->unk144;
    if (m->unk10[b->unk1E] > 99999) {
        m->unk10[b->unk1E] = 99999;
    }
    if (a->unk134 != 0) {
        if (a->unkE0 > 0) {
            if (a->unkE0 == 1 || (a->unkE0 >= 2 && func_0036BDD0(a) == 0)) {
                m->unk21 = 1;
            }
        } else {
            m->unk21 = 0;
        }
    }
    if (a->unk132 != 0 || a->unkE0 > 0) {
        m->unk32 = 1;
    } else {
        struct FighterMove *mv = a->unkC0;
        s32 ok = 0;

        if (a->unk10C == mv->unk10) {
            ok = 1;
        }

        if (a->unk134 != 0) {
            switch (a->unkF8) {
            case 3:
                ok = 1;
                break;
            case 2:
            case 4:
                if (mv->unk2C == 0) {
                    ok = 1;
                }
                break;
            }
        }
        if (a->unk133 != 0) {
            ok = 1;
        }
        if (ok) {
            m->unk32 = 0;
        }
    }
    if (b->unk131 != 0 || b->unkE0 < 0) {
        m->unk33 = 1;
    } else {
        struct FighterMove *mv;
        s32 ok = 0;

        if (func_0036BDD0(b) == 0) {
            ok = 1;
        }
        mv = b->unkC0;
        if (b->unk10C == mv->unk10) {
            ok = 1;
        }
        if (b->unkF8 == 3 && b->unk134 != 0) {
            ok = 1;
        }
        c = (u16 *)mv->unkC;
        n = b->unkC4;
        while (*c != 0x8000) {
            c += 7;
        }
        if (n >= ((u8 *)c)[0xB] && ((u8 *)c)[0xB] >= n && n > 0) {
            struct FighterMove *r = func_00368218(b, mv->unk10);

            if (b->unkC0->unk10 == r->unk10) {
                ok = 1;
            }
        }
        if (ok) {
            m->unk33 = 0;
        }
    }
    hit = func_0037C370(a);
    if (hit == 1) {
        m->unk2A = 120;
        m->unk28 = m->unk1E;
        m->unk26 = ++m->unk20;
        m->unk21 = hit;
    }
    if (m->unk32 != 0 || m->unk33 != 0) {
        if (b->unk144 != 0 && b->unk130 == 0) {
            if (b->unkE0 < 0 || b->unkE2 < 0) {
                if (m->unk21 != 0) {
                    if (hit == 0) {
                        m->unk20++;
                    }
                    m->unk21 = 0;
                    m->unk1C = m->unk18;
                } else {
                    m->unk1C += m->unk18;
                }
                m->unk24 = 100;
                m->unk22 = m->unk1C;
            } else {
                m->unk20++;
                m->unk22 = m->unk18;
                if (m->unk1A != 0) {
                    r = m->unk18 * 100 / m->unk1A;
                } else {
                    r = 0;
                }
                m->unk24 = r;
            }
            m->unk2A = 120;
            m->unk1E += m->unk18;
            m->unk26 = m->unk20;
            m->unk28 = m->unk1E;
        }
    } else {
        m->unk1C = 0;
        m->unk20 = 0;
        m->unk1E = 0;
    }
    if (m->unk20 >= 100) {
        m->unk20 = 99;
    }
    if (m->unk26 >= 100) {
        m->unk26 = 99;
    }
    if (m->unk1E >= 1000) {
        m->unk1E = 999;
    }
    if (m->unk28 >= 1000) {
        m->unk28 = 999;
    }
    if (m->unk2A > 0) {
        if (m->unk20 < 2) {
            m->unk2A--;
        }
    } else {
        m->unk26 = 0;
        m->unk28 = 0;
    }
    if (b->unk12F != 0) {
        if (b->unk131 != 0 && m->unk20 < 2 && b->unk13A != 0) {
            m->unk2C = 120;
        } else {
            m->unk2C = 0;
        }
    }
    if (m->unk2C > 0) {
        m->unk2C--;
    }
    if (b->unk13B != 0 && b->unk131 != 0) {
        m->unk2E = 120;
    }
    if (m->unk2E > 0) {
        m->unk2E--;
    }
}

void func_0037B478(PracMenu *m) {
    m->unk22 = 0;
    m->unk24 = 0;
}

void func_0037B488(PracMenu *m) {
    m->unk1E = 0;
    m->unk20 = 0;
    m->unk26 = 0;
    m->unk28 = 0;
    m->unk2A = 0;
    m->unk2C = 0;
    m->unk2E = 0;
}

void func_0037B4A8(PracMenu *m) {
    if ((u32)((u16)m->unk2A - 1) < 59) {
        m->unk2A = 60;
    }
    if ((u32)((u16)m->unk2C - 1) < 59) {
        m->unk2C = 60;
    }
    if ((u32)((u16)m->unk2E - 1) < 59) {
        m->unk2E = 60;
    }
}



INCLUDE_RODATA("asm/nonmatchings/practice", D_00506418);
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506430);
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506450);
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506468);
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506478);
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506488);
INCLUDE_RODATA("asm/nonmatchings/practice", D_00506498);
INCLUDE_RODATA("asm/nonmatchings/practice", D_005064A8);
void func_0037B4F8(PracMenu *m, Fighter *a, Fighter *b) {
    Text_Printf(D_00506418, 0x1E, 0x2A, 10, 4, 0x11, m->unk10[m->other]);
    Text_Printf(D_00506418, 0x15E, 0x2A, 10, 4, 0x11, m->unk10[m->side]);
    Text_Printf(D_00506430, 0x1E, 0x3E, 10, 4, m->unk24 > 100 ? 7 : 0x13, m->unk22, m->unk24);
    if (m->unk2C > 0) {
        Text_Printf(D_00506450, 0x28, 0x5E, 10, 4, 12);
    }
    if (m->unk2E > 0) {
        Text_Printf(D_00506468, 0x28, 0x72, 10, 4, 12);
    }
    if (m->unk2A != 0 && m->unk26 >= 2) {
        Text_Printf(D_00506478, 10, 0x90, 10, 7, 0x13, m->unk26);
        Text_Printf(D_00506488, 0x50, 0x94, 10, 6, 11);
        Text_Printf(D_00506498, 0x16, 0xB5, 10, 6, 0x13, m->unk28);
        Text_Printf(D_005064A8, 0x70, 0xBC, 10, 4, 9);
    }
}

void func_0037B6C8(void) {
}

s32 func_0037B6D0(s32 x, s32 y, u16 buttons) {
    s32 k = func_0036CFC0(buttons);

    if (k == 5) {
        return 0;
    }
    if (k >= 6) {
        k -= 2;
    } else {
        k -= 1;
    }
    func_003DF2F0(x, y, k);
    return 1;
}

void func_0037B738(s32 x, s32 y, u16 buttons) {
    s32 k = 0;

    if (buttons & 0x80) {
        k = 1;
    }
    if (buttons & 0x10) {
        k |= 2;
    }
    if (buttons & 0x40) {
        k |= 4;
    }
    if (buttons & 0x20) {
        k |= 8;
    }
    func_003DEF98(x, y, k);
}

s32 func_0037B770(Fighter *f) {
    if (f->unk10C == f->unkC0->unk10 || (f->unkC0->unk24 & 0xC0)) {
        return 1;
    }
    return 0;
}

void func_0037B7A8(s32 n) {
    D_005F5F98 = n;
}

s32 func_0037B7B0(void) {
    s32 n;

    if (D_005F5F98 > 0) {
        n = D_005F5F98 - 1;
        D_005F5F98 = n;
    } else {
        D_005F5F98 = 0;
        n = 0;
    }
    return n == 0;
}

void func_0037B7D8(Fighter *f) {
    PracMenu *m = &D_0042F268;
    u16 k = func_0037D4C8(f, func_003DD580(f->unk24));

    if (m->menu >= 0) {
        if (m->menu < 2) {
            if (k != 0) {
                Prac_DrawDemoPrompt(f->unk1E, 4);
            } else {
                Prac_DrawDemoPrompt(f->unk1E, 5);
            }
        }
    }
}

void func_0037B870(Fighter *f, s32 flag) {
    s32 c = 0;
    s32 k = func_003DD580(f->unk22);

    if (flag) {
        c = func_0037CA78(f->unk1E);
    }
    func_0037B8D8(f, k, c);
}








void func_0037B8D8(Fighter *f, s32 idx, s32 c) {
    s32 r = 0;
    s32 y = 100;
    s32 side;
    s32 i;
    s32 cur;
    s32 k;
    u16 *p;
    u16 n;
    CmdListEntry *e;

    i = func_0035A670();
    if (i != 0) {
        return;
    }
    side = f->unk1E;
    func_003DEC20();
    cur = c;
    i = r;
    func_003DD158(side);
    e = &func_0037D460(f->unk22)->unk8[idx];
    p = e->unk12;
    n = *p;
    if (n != 0) {
        do {
            func_003DD188(0x140, y, side, n - 1, cur);
            i++;
            y += 0x3C;
            k = func_003DFF20();
            p++;
            if (k != 0) {
                r = k;
            }
            cur = func_003DFF28();
        } while (i < 8 && (n = *p) != 0);
    }
    func_003DD188(0x140, y, side, idx, cur);
    k = func_003DFF20();
    func_0037BAB0(c, k != 0 ? k : r);
}

void func_0037BA30(s32 side, s32 a, s32 b) {
    func_003DEC20();
    func_003DD158(side);
    func_003DD188(0x140, 0x64, side, a, b);
}
