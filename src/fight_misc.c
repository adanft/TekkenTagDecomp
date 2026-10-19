/* 0x376C30-0x3790C8 (module fight_misc, docs/game_overview.md section 4; docs/systems/fighter.md section 5). The hit
 * test of the fight simulation (func_003770E0: attack volumes against body spheres, reached from Battle_ResolveHits
 * through func_003772B0), its segment / sphere test (func_00376CF8) and the code around it. The start is where
 * sound ends (Snd_*, its .rodata 0x500780-0x500D50); the end is provisional [I]: func_003790C8 reads a .data byte
 * placed after the practice object's own data, so it opens the practice object. The object's .rodata
 * (0x506150-0x506260) is included in place with INCLUDE_RODATA except the jump tables of the functions that are C;
 * its .lit4 word (2 pi, 0x5F5170) comes from func_00378760. */
#include "common.h"
#include "fighter.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* D_008A7A90 / D_008A7AA0: one 0x10-byte record per side holding the fighter's position (func_003772F0) [I]. */
typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u8 pad2[0x2];
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 unk8;
    /* 0xC */ s32 unkC;
} Unk8A7A90;

typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ s16 unk4;
} Unk8A7AB0Sub;

/* D_008A7AB0: a position and three cleared halfwords handed to func_0035BF10 (func_00379090) [I]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ u8 padC[0x4];
    /* 0x10 */ Unk8A7AB0Sub unk10;
    /* 0x16 */ u8 pad16[0xA];
} Unk8A7AB0;

/* D_008A7AD0: one 0x50-byte record per side (Fighter.unk1E), copied to and from the attack volume at Fighter + 0x190
 * (func_003780C8, func_00378500) [I]. */
typedef struct {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s32 unk44;
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 unk4C;
} Unk8A7AD0;

typedef struct {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ s16 unk2;
    /* 0x4 */ s16 unk4;
} Unk8A7B70;

extern s32 D_005F6BF0[2];
extern Unk8A7A90 D_008A7A90;
extern Unk8A7A90 D_008A7AA0;
extern Unk8A7AB0 D_008A7AB0;
extern Unk8A7AD0 D_008A7AD0[2];
extern Unk8A7B70 D_008A7B70[2];
/* Per character: the radii of its 14 body spheres. */
extern s16 *D_0042F098[];

/* Read only through D_0042EFD8 in .data (pointers to them) [V]: the object's first .rodata. */
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506150);
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506160);
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506180);
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_005061A0);

s32 func_00374C60(s32 id, s32 player);
s32 abs(s32 x);
void *func_003462F0(s32 side);
s32 func_00346310(s32 side);
void func_0035BF10(s32 a, s32 b, Unk8A7AB0 *p, s32 d);
void func_00378078(Unk8A7AD0 *p);

void func_00376C30(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        D_005F6BF0[i] = 0;
    }
}

void func_00376C60(s32 player) {
    D_005F6BF0[player] = 5;
}

void func_00376C78(s32 player) {
    s32 *p = &D_005F6BF0[player];

    if (*p != 0) {
        if (*p == 5) {
            func_00374C60(0x1321, player);
        }
        if (*p == 1) {
            func_00374C60(0x1321, player);
        }
        *p = *p - 1;
    }
}

/* Does the attack volume v touch the body sphere s? The segment is first cut to the sphere's height band (y - r .. y + r),
 * then its closest point to the centre is found in the x / z plane; the hit holds when that distance squared is at most
 * the sphere's radius squared. Integer only (64-bit products for the projection). */
#if 0
/* NON_MATCHING: x / z kept in t9 / t8 instead of s0 / s4, the dy copy and the order of the distance blocks differ (3 instructions short) */
s32 func_00376CF8(AttackVol *v, BodySphere *s) {
    AttackVol c;
    s32 x, y, z, r;
    s32 dx, dy, dz;
    s32 ax, az, ex, ez, t, len, d;
    s32 k, lo, hi;
    s32 hit = 0;

    x = s->unk0;
    dx = v->unkC - v->unk0;
    r = s->unkC;
    if (abs(dx) / 2 + r >= abs(x - (v->unkC + v->unk0) / 2)) {
        z = s->unk8;
        dz = v->unk14 - v->unk8;
        if (abs(dz) / 2 + r >= abs(z - (v->unk14 + v->unk8) / 2)) {
            y = s->unk4;
            dy = v->unk10 - v->unk4;
            if (abs(dy) / 2 + r >= abs(y - (v->unk10 + v->unk4) / 2)) {
                c.unkC = v->unkC;
                c.unk14 = v->unk14;
                c.unk0 = v->unk0;
                c.unk8 = v->unk8;
                if (v->unk4 != v->unk10) {
                    hi = y + r;
                    lo = y - r;
                    if (v->unk10 < v->unk4) {
                        if (hi < v->unk4) {
                            k = ((hi - v->unk4) << 12) / dy;
                            c.unk0 = v->unk0 + k * dx / 4096;
                            c.unk8 = v->unk8 + k * dz / 4096;
                        }
                        if (v->unk10 < lo) {
                            k = ((lo - v->unk4) << 12) / dy;
                            c.unkC = v->unk0 + k * dx / 4096;
                            c.unk14 = v->unk8 + k * dz / 4096;
                        }
                    } else {
                        if (hi < v->unk10) {
                            k = ((hi - v->unk4) << 12) / dy;
                            c.unkC = v->unk0 + k * dx / 4096;
                            c.unk14 = v->unk8 + k * dz / 4096;
                        }
                        if (v->unk4 < lo) {
                            k = ((lo - v->unk4) << 12) / dy;
                            c.unk0 = v->unk0 + k * dx / 4096;
                            c.unk8 = v->unk8 + k * dz / 4096;
                        }
                    }
                }
                ax = c.unk0 - x;
                az = c.unk8 - z;
                ex = c.unkC - c.unk0;
                ez = c.unk14 - c.unk8;
                t = -(ex * ax + ez * az);
                len = ex * ex + ez * ez;
                if (len == 0) {
                    d = ax * ax + az * az;
                } else if (t >= 0) {
                    if (len >= t) {
                        ax += (s32)((s64)ex * t / len);
                        az += (s32)((s64)ez * t / len);
                        d = ax * ax + az * az;
                    } else {
                        ax = c.unkC - s->unk0;
                        az = c.unk14 - s->unk8;
                        d = ax * ax + az * az;
                    }
                } else {
                    d = ax * ax + az * az;
                }
                hit = d <= s->unk10;
            }
        }
    }
    return hit;
}
#else
INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00376CF8);
#endif

/* The hit test of a frame: a's attack volumes against b's body spheres, then b's against a's. Bit 0 of the result is a
 * hit by a (the contact goes to b->unk39C, cb), bit 1 a hit by b (to ca). Moves of kind 0x19, and of kind 0x18 for
 * character 0x14, use the attack volumes of func_003462F0 instead of the fighter's own (unk13C of them, at most 4). A
 * side whose unk14E is set cannot be hit. */
s32 func_003770E0(Fighter *a, Fighter *b, HitContact *ca, HitContact *cb) {
    Fighter *att = a;
    Fighter *def = b;
    HitContact *out = cb;
    s32 result = 0;
    s32 bit = 1;
    s32 i, j, k, n;
    AttackVol *vol;
    BodySphere *sph;
    u8 kind;

    for (i = 0; i < 2; i++) {
        kind = att->unkC0->unk28;
        if ((kind == 0x18 && att->unk2A == 0x14) || kind == 0x19) {
            n = func_00346310(att->unk1E);
            vol = func_003462F0(att->unk1E);
        } else {
            n = att->unk13C;
            vol = (AttackVol *)&att->unk190;
            if (n > 4) {
                n = 4;
            }
        }
        for (k = 0; k < n && def->unk14E == 0; k++, vol++) {
            sph = def->unk1F0;
            for (j = 0; j < 14; j++, sph++) {
                if (func_00376CF8(vol, sph)) {
                    out->unk18 = j;
                    result |= bit;
                    out->unk0 = vol->unkC;
                    out->unk4 = vol->unk10;
                    out->unk8 = vol->unk14;
                    out->unk10 = vol->unkC - vol->unk0;
                    out->unk12 = vol->unk10 - vol->unk4;
                    out->unk14 = vol->unk14 - vol->unk8;
                    goto next;
                }
            }
        }
    next:
        att = b;
        def = a;
        out = ca;
        bit = 2;
    }
    return result;
}

s16 func_003772B0(Fighter *a, Fighter *b, HitContact *ca, HitContact *cb) {
    return func_003770E0(a, b, ca, cb);
}

void func_003772D8(void) {
    D_008A7A90.unk0 = 100;
    D_008A7AA0.unk0 = 100;
}

void func_003772F0(Fighter *a, Fighter *b) {
    Unk8A7A90 *pa = &D_008A7A90;
    Unk8A7A90 *pb = &D_008A7AA0;

    pa->unk4 = a->unk51C;
    pa->unk8 = a->unk520;
    pa->unkC = a->unk524;
    pb->unk4 = b->unk51C;
    pb->unk8 = b->unk520;
    pb->unkC = b->unk524;
}

INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00377338);

void func_00377338(Fighter *a, Fighter *b);

void func_00377AE0(Fighter *a, Fighter *b) {
    func_00377338(a, b);
}

void func_00377B58(Fighter *f, Fighter *o, s16 kind);

void func_00377AE8(Fighter *a, Fighter *b) {
    s32 ka = a->unkC0->unk30;
    s32 kb = b->unkC0->unk30;

    if (ka == 6) {
        kb = 6;
    }
    func_00377B58(a, b, ka);
    func_00377B58(b, a, kb);
}

INCLUDE_RODATA("asm/nonmatchings/fight_misc", jtbl_005061E0);
INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00377B58);









/* Sets the radii of the fighter's body spheres from its character's table. */
void func_00377F58(Fighter *f) {
    s16 *r = D_0042F098[f->unk2A];
    s32 i;

    for (i = 0; i < 14; i++) {
        s16 v = *r++;
        f->unk1F0[i].unkC = v;
        f->unk1F0[i].unk10 = v * v;
    }
}

/* func_00377F58 with every radius 10 larger. */
void func_00377FB8(Fighter *f) {
    s16 *r = D_0042F098[f->unk2A];
    s32 i;

    for (i = 0; i < 14; i++) {
        s32 v = *r++ + 10;
        f->unk1F0[i].unkC = v;
        f->unk1F0[i].unk10 = v * v;
    }
}

void func_00378068(Unk8A7AD0 *p);

void func_00378018(void) {
    Unk8A7AD0 *p = D_008A7AD0;
    s32 i;

    for (i = 0; i < 2; i++, p++) {
        func_00378068(p);
    }
}

void func_00378068(Unk8A7AD0 *p) {
    p->unk0 = 0;
    p->unk4 = 0;
    func_00378078(p);
}

void func_00378078(Unk8A7AD0 *p) {
    p->unk8 = 0;
    p->unkC = 0;
    p->unk10 = 0;
    p->unk14 = 0;
    p->unk18 = 0;
    p->unk1C = 0;
    p->unk20 = 0;
    p->unk24 = 0;
    p->unk28 = 0;
    p->unk2C = 0;
    p->unk30 = 0;
    p->unk34 = 0;
    p->unk38 = 0;
    p->unk3C = 0;
    p->unk40 = 0;
    p->unk44 = 0;
    p->unk48 = 0;
    p->unk4C = 0;
}

void func_003780C8(Fighter *f) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    s32 x, y, z;

    func_00378078(p);
    x = f->unk708;
    y = f->unk70C;
    z = f->unk710;
    f->unk190 = x;
    f->unk194 = y;
    f->unk198 = z;
    f->unk19C = x;
    f->unk1A0 = y;
    f->unk1A4 = z;
    p->unk38 = x;
    p->unk3C = y;
    p->unk40 = z;
    p->unk44 = x;
    p->unk48 = y;
    p->unk4C = z;
}

struct FighterUnkC0 *func_00368248(s32 id);
s32 Snd_PlaySe(s32 id, s32 player);
void func_003780C8(Fighter *f);
void func_00378810(Fighter *f);
void func_00378A28(Fighter *f);
void func_00378C48(Fighter *f);
void func_00378E70(Fighter *f);
void func_003A8C50(s32 side);

/* Character 0x1D: while its current move (a move with an attack kind 0x18 among unk28..unk2B) is inside its frame
 * window unk2C..unk2D, moves 0x104F, 0x1051 / 0x1053, 0x1A48 and 0x1054 start the special attack volume kinds 1-4
 * (sound effect 0x45), which then follow func_00378810 ... func_00378E70 each frame while `step` is set. */
void func_00378150(Fighter *f, s32 step) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    struct FighterUnkC0 *m = f->unkC0;

    if (f->unk2A != 0x1D || f->unk127 == 0) {
        p->unk0 = 0;
        return;
    }
    if (m->unk28 != 0x18 && m->unk29 != 0x18 && m->unk2A != 0x18 && m->unk2B != 0x18) {
        p->unk0 = 0;
        return;
    }
    switch (p->unk0) {
    case 0:
        if (f->unkC4 >= m->unk2C && f->unkC4 <= m->unk2D) {
            p->unk4 = 0;
            if (m == func_00368248(0x104F)) {
                p->unk4 = 1;
            } else if (m == func_00368248(0x1051)) {
                p->unk4 = 2;
            } else if (m == func_00368248(0x1053)) {
                p->unk4 = 2;
            } else if (m == func_00368248(0x1A48)) {
                p->unk4 = 3;
            } else if (m == func_00368248(0x1054)) {
                p->unk4 = 4;
            }
            if (p->unk4 != 0) {
                func_003780C8(f);
                p->unk0++;
                Snd_PlaySe(0x45, 0);
                func_003A8C50(f->unk1E);
            }
        }
        break;
    case 1:
        if (step != 0) {
            switch (p->unk4) {
            case 0:
                break;
            case 1:
                func_00378810(f);
                break;
            case 2:
                func_00378A28(f);
                break;
            case 3:
                func_00378C48(f);
                break;
            case 4:
                func_00378E70(f);
                break;
            }
        }
        break;
    }
    if (f->unkC4 < f->unkC0->unk2C || f->unkC4 > f->unkC0->unk2D) {
        p->unk0 = 0;
    }
}






void func_00378368(s32 side, s16 a, s16 b) {
    D_008A7B70[side & 1].unk0 = a;
    D_008A7B70[side & 1].unk4 = b;
    D_008A7B70[side & 1].unk2 = 0;
}

void func_00378810(Fighter *f);
void func_00378A28(Fighter *f);
void func_00378C48(Fighter *f);
void func_00378E70(Fighter *f);
void func_003A8C50(s32 side);
s32 func_003AA5E8(void);

/* Character 0x1D: a special attack volume driven by the request D_008A7B70[side] (kind 1-4 in unk0, frames in unk4):
 * state 0 takes the request and sets the volume up (func_003780C8), state 1 moves it each frame with one of
 * func_00378810 / func_00378A28 / func_00378C48 / func_00378E70 until unk2 reaches unk4 [I: a projectile]. */
void func_00378398(Fighter *f) {
    s16 side = f->unk1E;
    Unk8A7AD0 *p = &D_008A7AD0[side];
    Unk8A7B70 *q = &D_008A7B70[side];
    s32 kind;

    if (f->unk2A != 0x1D) {
        p->unk0 = 0;
        return;
    }
    switch (p->unk0) {
    case 0:
        kind = q->unk0;
        if (kind < 1 || kind > 4) {
            p->unk4 = 0;
        } else {
            p->unk4 = kind;
            func_003780C8(f);
            p->unk0++;
            func_003A8C50(side);
            q->unk0 = 0;
        }
        break;
    case 1:
        switch (p->unk4) {
        case 0:
            break;
        case 1:
            func_00378810(f);
            break;
        case 2:
            func_00378A28(f);
            break;
        case 3:
            func_00378C48(f);
            break;
        case 4:
            func_00378E70(f);
            break;
        }
        if (func_003AA5E8() == 0) {
            if (++q->unk2 >= q->unk4) {
                p->unk0 = 0;
                q->unk2 = 0;
                q->unk4 = 0;
            }
        }
        break;
    }
}






void func_00379090(s32 x, s32 y, s32 z);

void func_00378500(Fighter *f) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    s32 x = f->unk708;
    s32 y = f->unk70C;
    s32 z = f->unk710;
    s32 px = x + p->unk20;
    s32 py = y - p->unk24;
    s32 pz = z + p->unk28;

    p->unk38 = px;
    p->unk3C = py;
    p->unk40 = pz;
    p->unk44 = px + p->unk14;
    p->unk48 = py + p->unk18;
    p->unk4C = pz + p->unk1C;
    f->unk190 = x = x + p->unk2C;
    f->unk194 = y = y - p->unk30;
    f->unk198 = z = z + p->unk34;
    f->unk19C = x + p->unk14;
    f->unk1A0 = y + p->unk18;
    f->unk1A4 = z + p->unk1C;
    if (p->unk48 >= 0) {
        p->unk48 = 0;
        f->unk1A0 = 0;
        if ((p->unk10++ & 7) == 0) {
            func_00379090(p->unk44, p->unk48 + 10, p->unk4C);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00378600);

typedef f32 FVec4[4] __attribute__((aligned(16)));

extern FVec4 D_0042F120[2];

void sceVu0ApplyMatrix(f32 *out, f32 (*m)[4], f32 *v);
f32 atan2f(f32 y, f32 x);

/* The angle (0x10000 per turn) of the segment between the two points of D_0042F120 moved by the matrix at
 * Fighter + 0x1BF0, in the x / z plane. Float math (atan2f). */
s16 func_00378760(Fighter *f) {
    FVec4 a, b;
    f32 dx, dz, t, turn;

    sceVu0ApplyMatrix(a, f->unk1BF0, D_0042F120[0]);
    sceVu0ApplyMatrix(b, f->unk1BF0, D_0042F120[1]);
    dx = b[0] - a[0];
    dz = b[2] - a[2];
    t = atan2f(dz, dx);
    turn = 6.2831855f;
    if (t >= turn) {
        t -= turn;
    }
    return t * 65536.0f / turn;
}

/* 8-aligned after the jump table of func_00378398 (0x506244). */
RODATA_ALIGN8();
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506248);
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_0050624C);
INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00378810);

INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506250);
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506254);
INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00378A28);

INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00378C48);

INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506258);
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_0050625C);
s16 Math_Sin(s32 angle);
s16 Math_Cos(s32 angle);
void func_00378500(Fighter *f);
extern s16 D_004FF690[];
extern s16 D_00506258[];
extern s16 D_0050625C[];

/* Kind 4 of the special attack volume of character 0x1D: a velocity of 300 units tilted by -0xAAA along the facing
 * unk3C, scaled by the frames since the move's window opened, and two offsets from D_00506258 / D_0050625C. */
#if 0
/* NON_MATCHING: register allocation (the tilt length, the offset and the -1 rounding constant in s2 / s1 / s0
 * instead of s0 / s2 / s1) */
void func_00378E70(Fighter *f) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    s32 r, sx, sy, sz, t;
    s16 *tbl;
    s32 a, b, x;

    r = Math_Cos(-0xAAA) * 300 / 32768;
    sx = r * Math_Sin(f->unk3C) / 32768;
    sz = r * Math_Cos(f->unk3C) / 32768;
    sy = Math_Sin(-0xAAA) * 300 / 32768;
    t = f->unkC4 - f->unkC0->unk2C;
    t = t < 0 ? 0 : t;
    p->unk14 = t * sx;
    p->unk18 = t * sy;
    p->unk1C = t * sz;
    if (D_004FF690[f->unk26] != 0x6B) {
        tbl = D_00506258;
    } else {
        tbl = D_0050625C;
    }
    a = tbl[0];
    b = tbl[1];
    x = a * Math_Sin((s16)(f->unk3C - 0x8000)) / 32768;
    a = a * Math_Cos((s16)(f->unk3C - 0x8000));
    p->unk20 = x;
    p->unk24 = b;
    p->unk28 = a / 32768;
    tbl = D_00506258;
    a = tbl[0];
    b = tbl[1];
    x = a * Math_Sin((s16)(f->unk3C - 0x8000)) / 32768;
    a = a * Math_Cos((s16)(f->unk3C - 0x8000));
    p->unk2C = x;
    p->unk30 = b;
    p->unk34 = a / 32768;
    func_00378500(f);
    p->unkC = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00378E70);
#endif

void func_00379090(s32 x, s32 y, s32 z) {
    Unk8A7AB0Sub *q;

    D_008A7AB0.unk0 = x;
    D_008A7AB0.unk4 = y;
    D_008A7AB0.unk8 = z;
    q = &D_008A7AB0.unk10;
    q->unk0 = 0;
    q->unk2 = 0;
    q->unk4 = 0;
    func_0035BF10(2, 3, &D_008A7AB0, 0);
}
