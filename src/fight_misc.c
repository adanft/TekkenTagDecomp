/* 0x376C30-0x3790C8 (module fight_misc, docs/game_overview.md section 4; docs/systems/fighter.md section 5). The hit
 * test of the fight simulation (func_003770E0: attack volumes against body spheres, reached from Battle_ResolveHits
 * through func_003772B0), its segment / sphere test (func_00376CF8) and the code around it. The start is where
 * sound ends (Snd_*, its .rodata 0x500780-0x500D50); the end is provisional [I]: func_003790C8 reads a .data byte
 * placed after the practice object's own data, so it opens the practice object. The object's .rodata
 * (0x506150-0x506260) is included in place with INCLUDE_RODATA except the jump tables of the functions that are C;
 * its .lit4 word (2 pi, 0x5F5170) comes from func_00378760. */
#include "common.h"
#include "fighter.h"
#include "trig.h"

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

extern const s16 gSinTable[];
s32 func_003985D8(s32 x);
s32 func_00366108(s32 dx, s32 dz);

/* Pushes the two fighters apart (from Battle_ApplyVelocity through func_00377AE0): the deepest overlap of their push
 * spheres (or 400 when both are inside a frame window, close and moving towards each other), split between them by
 * how far each moved since func_003772F0 saved their positions, along their facings; the result is written to the
 * velocities unk3E8-unk3F0 (halved unless the forced case) [V code; I meaning]. */
#if 0
/* NON_MATCHING: register allocation and scheduling all through (338 of 490 instructions differ; one instruction
 * more): the copy of the other fighter's spheres runs with other pointer registers, and the overlap test and the
 * facing test load their operands in another order. */
void Fighter_Push(Fighter *a, Fighter *b) {
    PushSphere sb[8];
    PushSphere *p;
    PushSphere *q;
    s32 n;
    s16 i;
    s16 j;
    s16 pen;
    s16 d;
    s16 dx, dy, dz;
    u16 r;
    s32 hard;
    s32 ax, ay, az, bx, by, bz;
    s32 ia, ib;
    s32 sa, ca, sbn, cb;
    s32 ex, ez;
    u16 da, db;
    s16 anga, angb;
    s32 dir, diff, sum, pa, pb;

    pen = 0;
    a->unk3E8 = 0;
    a->unk3EC = 0;
    a->unk3F0 = 0;
    hard = 0;
    b->unk3E8 = 0;
    b->unk3EC = 0;
    b->unk3F0 = 0;
    p = b->unk308;
    q = sb;
    for (n = 0; n < 8; n++) {
        *q++ = *p++;
    }
    p = a->unk308;
    for (i = 0; i < 8; i++, p++) {
        if (p->unkC != 0) {
            q = sb;
            for (j = 0; j < 8; j++, q++) {
                if (q->unkC != 0) {
                    dx = q->unk0 - p->unk0;
                    dy = q->unk4 - p->unk4;
                    dz = q->unk8 - p->unk8;
                    r = p->unkC + q->unkC;
                    if (ABS(dx) < r && ABS(dy) < r && ABS(dz) < r) {
                        d = r - func_003985D8(dx * dx + dy * dy + dz * dz);
                        if (pen < d) {
                            pen = d;
                        }
                    }
                }
            }
        }
    }
    if (a->unkC0->unk2C != 0 && b->unkC0->unk2C != 0) {
        if (pen > 0 || ABS(a->unk520 - b->unk520) < 300) {
            ib = (0x400 - (s16)b->unk1A / 16) & 0xFFF;
            ia = (0x400 - (s16)a->unk1A / 16) & 0xFFF;
            ex = (b->unk51C + gSinTable[ib + 0x400] * 200 / 4096) - (a->unk51C + gSinTable[ia + 0x400] * 200 / 4096);
            ez = (b->unk524 + gSinTable[ib] * 200 / 4096) - (a->unk524 + gSinTable[ia] * 200 / 4096);
            if (ABS(ex) < 500 && ABS(ez) < 500 && (b->unk4 - a->unk4) * ex + (b->unkC - a->unkC) * ez < 0) {
                hard = 1;
                if (pen < 400) {
                    pen = 400;
                }
            }
        }
    }
    if (pen > 0) {
        ax = a->unk51C;
        ay = a->unk520;
        az = a->unk524;
        bx = b->unk51C;
        by = b->unk520;
        bz = b->unk524;
        dx = ax - D_008A7A90.unk4;
        dy = ay - D_008A7A90.unk8;
        dz = az - D_008A7A90.unkC;
        da = func_003985D8(dx * dx + dy * dy + dz * dz);
        dx = bx - D_008A7AA0.unk4;
        dy = by - D_008A7AA0.unk8;
        dz = bz - D_008A7AA0.unkC;
        db = func_003985D8(dx * dx + dy * dy + dz * dz);
        anga = 0x400 - (s16)a->unk1A / 16;
        dir = func_00366108(b->unk51C - a->unk51C, b->unk524 - a->unk524);
        diff = (anga - dir) & 0xFFF;
        if (diff >= 0x800) {
            diff = 0x1000 - diff;
        }
        if (diff > 0x400) {
            anga += 0x800;
        }
        angb = 0x400 - (s16)b->unk1A / 16;
        diff = (angb - (s16)(dir + 0x800)) & 0xFFF;
        if (diff >= 0x800) {
            diff = 0x1000 - diff;
        }
        if (diff > 0x400) {
            angb += 0x800;
        }
        ia = anga & 0xFFF;
        ib = angb & 0xFFF;
        sa = gSinTable[ia];
        ca = gSinTable[ia + 0x400];
        sbn = gSinTable[ib];
        cb = gSinTable[ib + 0x400];
        ax += cb * db / 4096;
        az += sbn * db / 4096;
        bx += ca * da / 4096;
        bz += sa * da / 4096;
        if ((da | db) == 0) {
            db = 1;
            da = 1;
        }
        sum = da + db;
        if (sum == 0) {
            sum = 1;
        }
        pa = pen * da / sum;
        pb = pen * db / sum;
        ax -= ca * pa / 4096;
        az -= sa * pa / 4096;
        bx -= cb * pb / 4096;
        bz -= sbn * pb / 4096;
        if (hard != 0) {
            a->unk3E8 = ax - a->unk51C;
            a->unk3EC = ay - a->unk520;
            a->unk3F0 = az - a->unk524;
            b->unk3E8 = bx - b->unk51C;
            b->unk3EC = by - b->unk520;
            b->unk3F0 = bz - b->unk524;
        } else {
            a->unk3E8 = (ax - a->unk51C) / 2;
            a->unk3EC = (ay - a->unk520) / 2;
            a->unk3F0 = (az - a->unk524) / 2;
            b->unk3E8 = (bx - b->unk51C) / 2;
            b->unk3EC = (by - b->unk520) / 2;
            b->unk3F0 = (bz - b->unk524) / 2;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/fight_misc", Fighter_Push);
#endif

void Fighter_Push(Fighter *a, Fighter *b);

void func_00377AE0(Fighter *a, Fighter *b) {
    Fighter_Push(a, b);
}

void Fighter_SetPushRadii(Fighter *f, Fighter *o, s16 kind);

void func_00377AE8(Fighter *a, Fighter *b) {
    s32 ka = a->unkC0->unk30;
    s32 kb = b->unkC0->unk30;

    if (ka == 6) {
        kb = 6;
    }
    Fighter_SetPushRadii(a, b, ka);
    Fighter_SetPushRadii(b, a, kb);
}

extern s16 *D_0042EFD8[];

/* Sets the radii of the eight push spheres (Fighter + 0x308) from the character's table D_0042EFD8 (radii of 0 or 1
 * turn a sphere off). kind & 0xF picks a rule: 1 / 5 / 6 keep spheres 0 and 3 only (5 / 6 at half size), 2 makes all
 * of them tiny, 3 halves them, 4 turns them off, 7 / 9 enlarge sphere 0 by 60 (7 also keeps 1-3), 8 enlarges all by
 * 100, 0 turns off the spheres whose bit (kind >> 4) is set while the move's window is open (bit 12) or always. Then,
 * by the current move: spheres 1 and 2 are off during a frame window, 6 and 7 for unk135 with unkF3, 4-7 for move
 * flag 0x400 [V code; I meaning]. */
void Fighter_SetPushRadii(Fighter *f, Fighter *o, s16 kind) {
    s16 *tbl = D_0042EFD8[f->unk2A];
    s32 i;
    s32 on = 0;

    if ((kind & 0xF) != 0) {
        switch (kind & 0xF) {
        case 1:
            for (i = 0; i < 8; i++) {
                if (i == 0) {
                    f->unk308[i].unkC = tbl[i];
                } else if (i == 3) {
                    f->unk308[i].unkC = tbl[i];
                } else {
                    f->unk308[i].unkC = 1;
                }
            }
            break;
        case 2:
            for (i = 0; i < 8; i++) {
                f->unk308[i].unkC = 1;
            }
            break;
        case 3:
            for (i = 0; i < 8; i++) {
                f->unk308[i].unkC = tbl[i] >> 1;
            }
            break;
        case 4:
            for (i = 0; i < 8; i++) {
                f->unk308[i].unkC = 0;
            }
            break;
        case 5:
        case 6:
            for (i = 0; i < 8; i++) {
                if (i == 0) {
                    f->unk308[i].unkC = tbl[i] >> 1;
                } else if (i == 3) {
                    f->unk308[i].unkC = tbl[i] >> 1;
                } else {
                    f->unk308[i].unkC = 1;
                }
            }
            break;
        case 7:
            for (i = 0; i < 8; i++) {
                if (i == 0) {
                    f->unk308[i].unkC = tbl[i] + 60;
                } else if (i == 6 || i == 7 || i == 4 || i == 5) {
                    f->unk308[i].unkC = 1;
                } else {
                    f->unk308[i].unkC = tbl[i];
                }
            }
            break;
        case 9:
            for (i = 0; i < 8; i++) {
                if (i == 0) {
                    f->unk308[i].unkC = tbl[i] + 60;
                } else {
                    f->unk308[i].unkC = 1;
                }
            }
            break;
        case 8:
            for (i = 0; i < 8; i++) {
                f->unk308[i].unkC = tbl[i] + 100;
            }
            break;
        default:
            for (i = 0; i < 8; i++) {
                f->unk308[i].unkC = tbl[i];
            }
            break;
        }
    } else {
        if (((u16)kind >> 12) & 1) {
            if (f->unkC0->unk2C != 0) {
                if (f->unkC0->unk2C - 8 < f->unkC4) {
                    on = f->unkC4 < f->unkC0->unk2D;
                }
            } else {
                on = 1;
            }
        } else {
            on = 1;
        }
        for (i = 0; i < 8; i++) {
            if (on && ((kind >> (i + 4)) & 1)) {
                f->unk308[i].unkC = 0;
            } else {
                f->unk308[i].unkC = tbl[i];
            }
        }
    }
    for (i = 0; i < 8; i++) {
        if (f->unkC0->unk2C != 0) {
            if (i == 1 || i == 2) {
                f->unk308[i].unkC = 0;
            }
        } else if (f->unk135 != 0 && f->unkF3 != 0) {
            if (i == 6 || i == 7) {
                f->unk308[i].unkC = 0;
            }
        } else if (f->unkC0->unk24 & 0x400) {
            if (i == 6 || i == 7 || i == 4 || i == 5) {
                f->unk308[i].unkC = 0;
            }
        }
    }
}









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

struct FighterMove *func_00368248(s32 id);
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
    struct FighterMove *m = f->unkC0;

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

extern u8 D_005F5F58;
extern u8 D_005F5F59;
extern u8 D_005F5F5A;
s32 func_003ABCD0(void);
void func_003A8D08(Fighter *f, s32 *a, s32 *b, s32 size);

/* Draws the special attack volume of character 0x1D (func_003A8D08 between its two points unk38 and unk44): its size
 * shrinks over the move's frame window unless func_003ABCD0 holds. The colour scaled into col is never used. */
void func_00378600(Fighter *f) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    u8 col[4];
    s32 t;
    s32 n;
    s32 size;

    if (p->unk0 == 0) {
        return;
    }
    if (func_003ABCD0() != 0) {
        t = 0x1000;
    } else {
        n = f->unkC0->unk2D - f->unkC0->unk2C + 0x1E;
        if (n <= 0) {
            n = 1;
        }
        t = ((n - (f->unkC4 - f->unkC0->unk2C)) << 12) / n;
        col[0] = D_005F5F58 * t / 4096;
        col[1] = D_005F5F59 * t / 4096;
        col[2] = D_005F5F5A * t / 4096;
    }
    switch (p->unkC) {
    case 0:
    default:
        size = t * 100 / 4096 + 50;
        break;
    case 1:
        size = t * 130 / 4096 + 70;
        break;
    }
    func_003A8D08(f, &p->unk38, &p->unk44, size);
}

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
void func_00378500(Fighter *f);
extern s16 D_004FF690[];
extern s16 D_00506248[];
extern s16 D_0050624C[];

/* Kind 1 of the special attack volume of character 0x1D: a velocity of 200 units along the facing (unk3C, or the
 * angle of func_00378760 while func_003ABCD0 holds), scaled by the frames since the move's window opened (or the
 * request's frame count), and two offsets from D_00506248 / D_0050624C. */
#if 0
/* NON_MATCHING: the store to unk14 is scheduled before the one to unk18 (the original stores unk18, unk14, unk1C);
 * every other instruction matches. */
void func_00378810(Fighter *f) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    Unk8A7B70 *q = &D_008A7B70[f->unk1E];
    s16 angle;
    s16 back;
    s32 sx, sy, sz, t;
    s16 *tbl;
    s32 x, b, a;
    s32 *pa; /* fake (decomp-permuter): the address of a changes the register order of a and the -1 */

    if (func_003ABCD0() == 0) {
        angle = f->unk3C;
    } else {
        angle = func_00378760(f);
    }
    sx = Math_Sin(angle) * 200 / 32768;
    sz = Math_Cos(angle) * 200 / 32768;
    sy = 0;
    if (func_003ABCD0() == 0) {
        t = f->unkC4 - f->unkC0->unk2C;
    } else {
        t = q->unk2;
    }
    t = t < 0 ? 0 : t;
    p->unk14 = t * sx;
    p->unk18 = t * sy;
    p->unk1C = t * sz;
    if (D_004FF690[f->unk26] != 0x6B) {
        tbl = D_00506248;
    } else {
        tbl = D_0050624C;
    }
    back = angle - 0x8000;
    a = tbl[0];
    b = tbl[1];
    x = a * Math_Sin(back) / 32768;
    a = a * Math_Cos(back);
    p->unk20 = x;
    p->unk24 = b;
    p->unk28 = a / 32768;
    pa = &a;
    tbl = D_00506248;
    a = tbl[0];
    b = tbl[1];
    x = *pa * Math_Sin(back) / 32768;
    a = a * Math_Cos(back);
    p->unk2C = x;
    p->unk30 = b;
    p->unk34 = a / 32768;
    func_00378500(f);
    p->unkC = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/fight_misc", func_00378810);
#endif

INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506250);
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506254);
extern s16 D_00506250[];
extern s16 D_00506254[];

/* Kind 2 of the special attack volume of character 0x1D: as kind 1 (func_00378810) with a rising velocity, 102 units
 * forward and 110 up (204 / 220 while func_003ABCD0 holds), offsets from D_00506250 / D_00506254. */
void func_00378A28(Fighter *f) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    Unk8A7B70 *q = &D_008A7B70[f->unk1E];
    s16 angle;
    s16 back;
    s32 speed, up, sx, sy, sz, t;
    s16 *tbl;
    s32 x, b, a;
    s32 *pa; /* fake (decomp-permuter): the address of a changes the register order of a and the -1 */

    if (func_003ABCD0() == 0) {
        angle = f->unk3C;
        speed = 102;
        up = 110;
    } else {
        angle = func_00378760(f);
        speed = 204;
        up = 220;
    }
    sx = speed * Math_Sin(angle) / 32768;
    sz = speed * Math_Cos(angle) / 32768;
    sy = up;
    if (func_003ABCD0() == 0) {
        t = f->unkC4 - f->unkC0->unk2C;
    } else {
        t = q->unk2;
    }
    t = t < 0 ? 0 : t;
    p->unk14 = t * sx;
    p->unk18 = t * sy;
    p->unk1C = t * sz;
    if (D_004FF690[f->unk26] != 0x6B) {
        tbl = D_00506250;
    } else {
        tbl = D_00506254;
    }
    back = angle - 0x8000;
    a = tbl[0];
    b = tbl[1];
    x = a * Math_Sin(back) / 32768;
    a = a * Math_Cos(back);
    p->unk20 = x;
    p->unk24 = b;
    p->unk28 = a / 32768;
    pa = &a;
    tbl = D_00506250;
    a = tbl[0];
    b = tbl[1];
    x = *pa * Math_Sin(back) / 32768;
    a = a * Math_Cos(back);
    p->unk2C = x;
    p->unk30 = b;
    p->unk34 = a / 32768;
    func_00378500(f);
    p->unkC = 1;
}

extern s16 D_005F5F5C;
extern u16 D_005F5F5E;
extern s16 D_005F5F60;
s32 rand(void);

/* Kind 3 of the special attack volume of character 0x1D: state 0 resets the sweep, state 1 drops the volume (unk18
 * down by 800 a frame to -6400) for 8 frames, state 2 sweeps it left and right (D_005F5F5C by 0x200 a frame between
 * -0x2FFF and 0x2FFF) with a random length (rand() & 0x3FF); the offset is 140 units behind and 160 up [I]. */
void func_00378C48(Fighter *f) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    s32 a, b, x, z;

    switch (p->unk8) {
    case 0:
        D_005F5F5C = 0;
        D_005F5F5E = 0;
        D_005F5F60 = 0;
        p->unk8 = 1;
    case 1:
        p->unk14 = 0;
        p->unk18 -= 800;
        p->unk1C = 0;
        if (p->unk18 < -6400) {
            p->unk18 = -6400;
        }
        if (D_005F5F60++ >= 8) {
            p->unk8++;
        }
        break;
    case 2:
        if (D_005F5F5E == 0) {
            D_005F5F5C += 0x200;
        } else {
            D_005F5F5C -= 0x200;
        }
        if (D_005F5F5C > 0x2FFF || D_005F5F5C < -0x2FFF) {
            D_005F5F5E = (D_005F5F5E + 1) & 1;
        }
        p->unk14 = (rand() & 0x3FF) * Math_Sin((s16)(f->unk3C + D_005F5F5C)) / 32768;
        p->unk1C = (rand() & 0x3FF) * Math_Cos((s16)(f->unk3C + D_005F5F5C)) / 32768;
        break;
    }
    a = 140;
    b = 160;
    x = Math_Sin((s16)(f->unk3C - 0x8000)) * a / 32768;
    z = Math_Cos((s16)(f->unk3C - 0x8000)) * a / 32768;
    p->unk20 = x;
    p->unk24 = b;
    p->unk28 = z;
    func_00378500(f);
    p->unkC = 1;
}

INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_00506258);
INCLUDE_RODATA("asm/nonmatchings/fight_misc", D_0050625C);
extern s16 D_00506258[];
extern s16 D_0050625C[];

/* Kind 4 of the special attack volume of character 0x1D: a velocity of 300 units tilted by -0xAAA along the facing
 * unk3C, scaled by the frames since the move's window opened, and two offsets from D_00506258 / D_0050625C. */
void func_00378E70(Fighter *f) {
    Unk8A7AD0 *p = &D_008A7AD0[f->unk1E];
    s32 r, sx, sy, sz, t;
    s16 *tbl;
    s32 a, b, x;
    s32 *pa; /* fake (decomp-permuter): the address of a changes the register order of a and the -1 */

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
    pa = &a;
    tbl = D_00506258;
    a = tbl[0];
    b = tbl[1];
    x = *pa * Math_Sin((s16)(f->unk3C - 0x8000)) / 32768;
    a = a * Math_Cos((s16)(f->unk3C - 0x8000));
    p->unk2C = x;
    p->unk30 = b;
    p->unk34 = a / 32768;
    func_00378500(f);
    p->unkC = 0;
}

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
