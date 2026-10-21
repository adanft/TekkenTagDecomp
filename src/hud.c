/* 0x3C1A78-0x3C3150 (module hud, docs/game_overview.md section 4; docs/systems/render.md section 5): the fight HUD.
 * Each frame Hud_Draw (from battle_flow's func_0036EA80) draws the round markers, two life bars per side (active
 * member in front, partner behind; they trade places over 30 frames on a tag) and the timer, through the sprite2d
 * primitives (640 x 448 screen pixels, GS context 2). Also the K.O. percentages, the per-round HUD state and the
 * partner counter of the team members (TeamMember.unk28). A cut inside the select_results range (its .rodata follows
 * that object's), not an object edge; the C emits no data of its own. Every NON_MATCHING draft (#if 0) compiles when
 * enabled. */
#include "common.h"
#include "fighter.h"

/* Per-side HUD state (D_01FAC8B8, 2 x 0x28 bytes). */
typedef struct {
    /* 0x00 */ s32 unk0;                      /* tag-in fade counter (Hud_Draw) */
    /* 0x04 */ s32 unk4[2];                   /* white damage trail per member (falls to the life) */
    /* 0x0C */ s32 unkC;                      /* win marker blink running */
    /* 0x10 */ s32 unk10;                     /* markers shown */
    /* 0x14 */ s32 unk14;                     /* blink frames left */
    /* 0x18 */ s32 unk18;                     /* blink half period */
    /* 0x1C */ s32 unk1C;                     /* blink frame counter */
    /* 0x20 */ s32 unk20;                     /* blink phase */
    /* 0x24 */ s32 unk24;                     /* 8 when the side has one member, else 0: lowers the bars */
} HudSide;

extern Fighter gFighters[4];
extern TeamMember D_00440940[2][2];
extern HudSide D_01FAC8B8[2];
extern s32 D_01FAC908[];
extern u8 D_0043B7F8[];
extern s32 D_005F6E64;
extern s32 D_005F6730;
extern u64 D_005F6738;
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 gRoundTimer;
extern u16 D_005F5C70;
extern s16 D_005F5CC8[2];
extern s32 D_005F6E58;
extern s32 D_005F6E5C;
extern s32 D_01FAC878[2][2];
extern s32 D_01FAC888[2][2];
extern s32 D_01FAC898[2][2];
extern s32 D_01FAC8A8[2][2];

s32 func_0036E158(void);
s32 func_003A9698(void);
s32 func_003CF278(u8 c0, u8 k0, u8 c1, u8 k1, s32 kind, s32 n);
void func_003CF608(s32 a);
void Sprite_SetFilter(s32 a, s32 b);
void Sprite_SetBlend(s32 a, s32 b);
s32 func_003CF230(s32 chr, s32 costume);
s32 func_0039C778(void);
void Sprite_DrawScaledBlended(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);
void func_003CF1C0(s32 a, s32 b, s32 x, s32 y);
extern s8 D_0051F678[];
extern const char D_0051F6A0[];
extern const char D_0051F6B0[];
s32 Text_Printf(const char *fmt, ...);
s32 func_003C0098(void);
s32 Tag_StepCounters(s32 side, s32 a);
void Hud_DrawMarkers(void);
void Hud_DrawLifeBar(s32 side, s32 member, s32 fade, s32 partner);
void func_003C6738(s32 a, s32 n, s32 x, s32 y);
void Sprite_Draw(s32 a, s32 b, s32 c, s32 x, s32 y);
void Sprite_DrawScaled(s32 a, s32 b, s32 c, s32 x, s32 y, s32 w, s32 h);

/* Blink of the newest marker: unk20 toggles every unk18 frames for unk14 frames, the period shrinking by one each
 * time the phase turns back to 0 (never below 2) [V]. */
#define HUD_BLINK(h)                                                                                                  \
    if ((h)->unkC == 1) {                                                                                             \
        if ((h)->unk14 > 0) {                                                                                         \
            (h)->unk14--;                                                                                             \
            (h)->unk1C++;                                                                                             \
            if ((h)->unk18 == (h)->unk1C) {                                                                           \
                (h)->unk1C = 0;                                                                                       \
                (h)->unk20 ^= 1;                                                                                      \
                (h)->unk18 -= (h)->unk20;                                                                             \
                if ((h)->unk18 == 1) {                                                                                \
                    (h)->unk18 = 2;                                                                                   \
                }                                                                                                     \
            }                                                                                                         \
        } else {                                                                                                      \
            (h)->unk20 = 1;                                                                                           \
            (h)->unkC = 0;                                                                                            \
        }                                                                                                             \
    }

/* Tag swap (called from Fighter_TagSwap) [V]: the new active member's (f->unk2) rage counter unk28 goes back to 100;
 * the new partner's (f->unk0), when at least 120, becomes 300 +- 60 or 0 by the pair rules of func_003CF278
 * (character / costume pairs) [I: TTT's partner rage]. */
void func_003C1A78(s32 side) {
    Fighter *f = &gFighters[side];
    s32 m = f->unk0;
    s32 p = f->unk2;

    D_00440940[side][p].unk28 = 100;
    if (D_00440940[side][m].unk28 >= 120) {
        D_00440940[side][m].unk28 += 100;
        D_00440940[side][m].unk28 = func_003CF278(D_00440940[side][m].unk0, D_00440940[side][m].unk4,
                                                  D_00440940[side][p].unk0, D_00440940[side][p].unk4, 300, 60);
    }
}

/* Per frame, outside pause / replay [V]: while the partner's counter unk28 is at least 120 it counts down and the
 * active fighter's unk400 (rage, extra damage [I]) is set; it is cancelled (counter back to 100) when the active member's
 * own counter is at least 120 or func_0036E158() != 1. */
/* The original object names the address D_00440968 (= D_00440940 + 0x28); the linked bytes are the same. */
void func_003C1B38(void) {
    Fighter *f = gFighters;
    s32 side;

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        for (side = 0; side < 2; side++, f++) {
            if (D_00440940[side][f->unk0].unk28 >= 120) {
                f->unk400 = 1;
                D_00440940[side][f->unk0].unk28--;
            } else {
                f->unk400 = 0;
            }
            if (D_00440940[side][f->unk2].unk28 >= 120 || func_0036E158() != 1) {
                f->unk400 = 0;
                D_00440940[side][f->unk0].unk28 = 100;
            }
        }
    }
}

/* Loads 2D sprite set 2 (func_003CF608) [I]. */
void func_003C1C90(void) {
    func_003CF608(2);
}

/* Round init [V]: clears the tag fade counters and copies each member's life to the damage trail (HudSide.unk4),
 * resets the refill counters unk18 / unk1C and saves life / red life in unk20 / unk24 (shown by game type 7). */
/* The original object names the address D_0044094C (= D_00440940 + 0xC); the linked bytes are the same. */
void Hud_InitRound(void) {
    s32 i;
    s32 j;

    for (i = 0; i < 2; i++) {
        D_01FAC8B8[i].unk0 = 0;
        for (j = 0; j < 2; j++) {
            D_01FAC8B8[i].unk4[j] = D_00440940[i][j].unkC;
            D_00440940[i][j].unk18 = 0;
            D_00440940[i][j].unk1C = 0;
            D_00440940[i][j].unk20 = D_00440940[i][j].unkC;
            D_00440940[i][j].unk24 = D_00440940[i][j].unk10;
        }
    }
}

/* Name plate of a member under its life bar [V]: background plate (sprite 0x39 / clut 0x3A, or 0x3B / 0x3C when
 * D_0051F678[character] != 0) 124 x 34 at (x, y) unless func_0039C778() != 0, then the character's name sprite
 * (func_003CF1C0, index from func_003CF230(character, costume)) at the same place. */
void Hud_DrawNamePlate(s32 side, s32 member, s32 x, s32 y) {
    s32 name = func_003CF230(D_00440940[side][member].unk0, D_00440940[side][member].unk4);

    Sprite_SetFilter(0, 1);
    Sprite_SetBlend(0, 4);
    if (func_0039C778() == 0) {
        if (D_0051F678[D_00440940[side][member].unk0] == 0) {
            Sprite_DrawScaledBlended(0, 0x39, 0x3A, x, y, 0x7C, 0x22);
        } else {
            Sprite_DrawScaledBlended(0, 0x3B, 0x3C, x, y, 0x7C, 0x22);
        }
    }
    Sprite_SetBlend(0, 1);
    func_003CF1C0(0, name, x, y);
    Sprite_SetFilter(0, 1);
}

/* Resets the marker blink state of both sides [V]. */
void func_003C1EC0(void) {
    s32 i;
    HudSide *h;

    for (i = 0; i < 2; i++) {
        h = &D_01FAC8B8[i];
        h->unkC = 0;
        h->unk14 = 0;
        h->unk18 = 0;
        h->unk1C = 0;
        h->unk20 = 1;
    }
}

static inline s32 Hud_IsBlinkOn(HudSide *h) {
    return h->unk20 != 0;
}

/* NON_MATCHING: 1 line: in `if (k == n) h->unk10 = n;` the original stores n's register (s3), this C k's (s2); every
 * other instruction matches with the first loop on `ix` and the blink test through an inline (decomp-permuter, the
 * inline marked fake). D_0043B855 / D_0043B84A are D_0043B7F8 + 0x5D / + 0x52. */
/* Round / team markers [V]: game type 2 (D_0043B7F8[0] == 2, team battle [I]) draws one square per team member
 * (sprite 0x49 / clut 0x4A, 20 x 25 at y 60, mirrored on side 1), grey for members still standing (option byte
 * D_0043B7F8[0x52 + side * 0x1E + i] != 2) and red for the lost ones, the newest loss blinking. Then for each side the
 * extra icon (sprite 0x47 / clut 0x48 at x 13 / 595, y 400, Z 0xFFF00) when D_005F5CC8[side] != 0, and, when more than
 * one round is needed (D_005F5C70 >= 2), the round-win markers (sprite 0x43 / clut 0x44 won, 0x45 / 0x46 empty) every
 * 22 px from x 250 leftwards (side 0) or 364 rightwards (side 1) at y 60 - HudSide.unk24, the newest win blinking. */
#if 0
void Hud_DrawMarkers(void) {
    s32 p;
    s32 i;
    s32 j;
    u8 n;
    s32 k;
    s32 x;
    s32 dx;
    s32 ix;
    s32 save;
    HudSide *h;

    Sprite_SetFilter(0, 1);
    Sprite_SetBlend(0, 0);
    if (D_0043B7F8[0] == 2) {
        x = 0xFC;
        for (ix = 0; ix < 2; ix++) {
            n = (D_0043B7F8 + 0x5D)[ix * 0x1E];
            h = &D_01FAC8B8[ix];
            if (ix == 0) {
                x += 20;
                x -= n * 16;
            } else {
                x -= 16;
                x += n * 16;
            }
            k = 0;
            for (j = 0; j < n; j++) {
                if ((D_0043B7F8 + 0x52)[ix * 0x1E + j] != 2) {
                    k++;
                }
            }
            if (k == n) {
                h->unk10 = n;
            } else if (k < h->unk10) {
                h->unkC = 1;
                h->unk10 = k;
                h->unk14 = 180;
                h->unk18 = 11;
                h->unk1C = 0;
                h->unk20 = 1;
            }
            HUD_BLINK(h);
            for (i = 0; i < n; i++) {
                if (i < k) {
                    D_005F6738 = 0x80808080;
                } else if (h->unkC != 1 || k != i || Hud_IsBlinkOn(h)) { /* fake (decomp-permuter): an inline call */
                    D_005F6738 = 0x801E1E64;
                } else {
                    D_005F6738 = 0x80808080;
                }
                if (ix == 0) {
                    Sprite_DrawScaledBlended(0, 0x49, 0x4A, x, 0x3C, 0x14, 0x19);
                    x += 16;
                } else {
                    Sprite_DrawScaledBlended(0, 0x49, 0x4A, x, 0x3C, -0x14, 0x19);
                    x -= 16;
                }
            }
            x = 0x17E;
        }
    }
    D_005F6738 = 0x80808080;
    for (p = 0; p < 2; p++) {
        h = &D_01FAC8B8[p];
        if (p == 0) {
            k = 60 - h->unk24;
            x = 0xFA;
            dx = -0x16;
            ix = 0xD;
        } else {
            k = 60 - h->unk24;
            x = 0x16C;
            dx = 0x16;
            ix = 0x253;
        }
        if (D_005F5CC8[p] != 0) {
            save = D_005F6730;
            D_005F6730 = 0xFFF00;
            Sprite_SetFilter(1, 1);
            Sprite_SetBlend(1, 1);
            Sprite_Draw(1, 0x47, 0x48, ix, 400);
            D_005F6730 = save;
        }
        if (D_005F5C70 >= 2) {
            if (gFighters[p].unk5A == 0) {
                h->unk10 = 0;
            } else if (h->unk10 < gFighters[p].unk5A) {
                h->unkC = 1;
                h->unk10 = gFighters[p].unk5A;
                h->unk14 = 180;
                h->unk18 = 11;
                h->unk1C = 0;
                h->unk20 = 1;
            }
            HUD_BLINK(h);
            for (ix = 0; ix < D_005F5C70; ix++) {
                if (ix < gFighters[p].unk5A) {
                    if (h->unkC == 1 && h->unk10 - 1 == ix && h->unk20 == 0) {
                        Sprite_Draw(0, 0x45, 0x46, x, k);
                    } else {
                        Sprite_Draw(0, 0x43, 0x44, x, k);
                    }
                } else {
                    Sprite_Draw(0, 0x45, 0x46, x, k);
                }
                x += dx;
            }
        }
    }
    Sprite_SetFilter(0, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/hud", Hud_DrawMarkers);
#endif

/* NON_MATCHING: 235 lines: register allocation and stack slots (frame 0xF0 against 0xE0), every block in place */
/* Draws one life bar (side, member, fade = offset of a tag-in slide, partner = 1 for the second bar of the side) [V]:
 * y = 30 - fade; side 0 bar from x 29 - fade to 269 - fade (right-anchored, depletes towards the right end), side 1
 * from 370 + fade to 610 + fade; 240 px wide, 28 high, with 9 px end caps. Lengths are 240 * (value / 100) /
 * (maximum / 100) for the life (refilling by maximum / 800 per frame when it rises), the red life (or the frozen red
 * value blinking during a juggle, unk12A == 1) and the white damage trail HudSide.unk4[member] (falls by
 * maximum / 400 per frame). A member whose unk28 >= 120 uses the flashing sprite set (0x3D-0x42, brighter grey 0x70).
 * The partner bar also draws the name plate (Hud_DrawNamePlate) below it and is drawn at grey 0x80. Game type 7 draws the
 * values TeamMember.unk20 / unk24 instead of the life and red life. */
#if 0
void Hud_DrawLifeBar(s32 side, s32 member, s32 fade, s32 partner) {
    HudSide *hs = &D_01FAC8B8[side];
    s32 alpha;
    s32 tLife;
    s32 tTrail;
    s32 tBg;
    s32 tCapL;
    s32 tCapR;
    s32 tRed;
    s32 bgL;
    s32 bgR;
    s32 capL;
    s32 capR;
    s32 start;
    s32 flash;
    s32 y;
    s32 v;
    s32 len;
    s32 lifeL;
    s32 barR;
    s32 redL;
    s32 redR;
    s32 trL;
    s32 trR;
    s32 end;

    if (D_0043B7F8[0] == 7) {
        D_005F6E58 = D_00440940[side][member].unkC;
        D_005F6E5C = D_00440940[side][member].unk10;
        D_00440940[side][member].unkC = D_00440940[side][member].unk20;
        D_00440940[side][member].unk10 = D_00440940[side][member].unk24;
    }
    if (D_00440940[side][member].unk8 == 0) {
        return;
    }
    if (D_00440940[side][member ^ 1].unk8 == 0) {
        hs->unk24 = fade = 8;
    } else {
        hs->unk24 = 0;
    }
    if (gFighters[side].unk12A == 1 || D_01FAC8A8[side][member] != 0) {
        if (D_01FAC8A8[side][member] == 0) {
            D_01FAC8A8[side][member] = 60;
        }
        if (D_01FAC8A8[side][member] % 3 >= 2) {
            D_01FAC898[side][member] = D_01FAC888[side][member];
        } else {
            D_01FAC898[side][member] = 0;
        }
        D_01FAC8A8[side][member]--;
    } else {
        D_01FAC898[side][member] = 0;
        D_01FAC8A8[side][member] = 0;
        D_01FAC888[side][member] = D_00440940[side][member].unk10;
    }
    partner *= 40;
    if (D_00440940[side][member].unk28 >= 120) {
        if (D_01FAC878[side][member] == 0) {
            D_01FAC878[side][member] = 8;
        }
        flash = 0;
        if (D_01FAC878[side][member] >= 4) {
            if (partner != 40) {
                partner = 20;
            }
            flash = 1;
        }
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            D_01FAC878[side][member]--;
        }
    } else {
        D_01FAC878[side][member] = 0;
        flash = 0;
    }
    alpha = 0x50;
    if (flash == 0) {
        tLife = 0x2E;
        tTrail = 0x30;
        tBg = 0x32;
        tCapL = 0x34;
        tCapR = 0x36;
        tRed = 0x38;
    } else {
        tLife = 0x3D;
        tTrail = 0x3E;
        tBg = 0x3F;
        tCapL = 0x40;
        tCapR = 0x41;
        tRed = 0x42;
        alpha = 0x70;
    }
    y = 30 - fade;

    if (D_00440940[side][member].unk18 < D_00440940[side][member].unkC && D_00440940[side][member].unk18 != 0) {
        v = D_00440940[side][member].unk18 += D_00440940[side][member].unk14 / 800;
    } else {
        D_00440940[side][member].unk18 = 0;
        v = D_00440940[side][member].unkC;
    }
    len = 240 * (v / 100) / (D_00440940[side][member].unk14 / 100);
    if (side == 0) {
        end = 0x10D - fade;
        start = end - 240;
        capL = end - 249;
        lifeL = end - len;
        barR = end;
        capR = end;
        if (partner == 40) {
            Hud_DrawNamePlate(0, member, capL, y + 0x1C);
            alpha = 0x80;
        }
    } else {
        lifeL = fade + 0x172;
        barR = fade + 0x262;
        capL = fade + 0x169;
        end = lifeL + len;
        start = lifeL;
        capR = barR;
        if (partner == 40) {
            Hud_DrawNamePlate(side, member, fade + 0x1EA, y + 0x1C);
            alpha = 0x80;
        }
    }

    if (D_00440940[side][member].unk1C < D_00440940[side][member].unk10 && D_00440940[side][member].unk1C != 0) {
        v = D_00440940[side][member].unk1C += D_00440940[side][member].unk14 / 800;
    } else {
        D_00440940[side][member].unk1C = 0;
        v = D_00440940[side][member].unk10;
    }
    if (D_01FAC898[side][member] == 0) {
        len = 240 * (v / 100);
    } else {
        len = 240 * (D_01FAC898[side][member] / 100);
    }
    len = len / (D_00440940[side][member].unk14 / 100);
    if (side == 0) {
        redL = end - len;
        redR = lifeL;
    } else {
        redR = lifeL + len;
        redL = end;
    }
    if (side == 0) {
        bgR = redL;
        bgL = start;
    } else {
        bgR = barR;
        bgL = redR;
    }

    if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
        if (D_00440940[side][member].unkC < hs->unk4[member]) {
            hs->unk4[member] -= D_00440940[side][member].unk14 / 400;
        }
    }
    if (hs->unk4[member] < D_00440940[side][member].unkC) {
        hs->unk4[member] = D_00440940[side][member].unkC;
    }
    if (D_00440940[side][member].unkC == 0) {
        hs->unk4[member] = 0;
        D_00440940[side][member].unk10 = 0;
    }
    len = 240 * ((hs->unk4[member] - D_00440940[side][member].unkC) / 100) / (D_00440940[side][member].unk14 / 100);
    trL = end;
    if (side == 0) {
        trR = lifeL;
        trL = trR - len;
    } else {
        trR = trL + len;
    }

    Sprite_SetFilter(0, 0);
    Sprite_SetBlend(0, 1);
    D_005F6738 = (u64)alpha | ((u64)0x8000 << 16) | ((alpha << 16) | (alpha << 8));
    Sprite_DrawScaled(0, 0x33, tCapL, capL, y, 9, 0x1C);
    Sprite_DrawScaled(0, 0x35, tCapR, capR, y, 9, 0x1C);
    Sprite_DrawScaled(0, 0x31, tBg, bgL, y, bgR - bgL, 0x1C);
    Sprite_DrawScaled(0, 0x37, tRed, redL, y, redR - redL, 0x1C);
    Sprite_DrawScaled(0, 0x2D, tLife, lifeL, y, end - lifeL, 0x1C);
    Sprite_DrawScaled(0, 0x2F, tTrail, trL, y, trR - trL, 0x1C);
    D_005F6738 = 0x80808080;
    Sprite_SetFilter(0, 1);
    if (D_0043B7F8[0] == 7) {
        D_00440940[side][member].unkC = D_005F6E58;
        D_00440940[side][member].unk10 = D_005F6E5C;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/hud", Hud_DrawLifeBar);
#endif

/* K.O. life percentages [V]: makes the shown lives (unk408, 20.12) of two fighters with the same whole percentage
 * differ (the leader gets +10 - d, the other -d, d = 10 only at 100 %), then prints "%d%%" of side 0 right-aligned at
 * x 228 (width measured with D_0051F6A0) and side 1 at x 412, y 100, the higher one in colour 11, the lower in 12. */
void Hud_DrawKoPercent(Fighter *a, Fighter *b) {
    u16 lb = b->unk408;
    u16 la = a->unk408;
    s32 up = 0;
    s32 down = 0;
    s32 ca;
    s32 cb;
    s32 w;
    s32 pa = la * 100 / 4096;

    if (pa == lb * 100 / 4096) {
        up = 10;
        if (pa == 100) {
            down = 10;
        }
    }
    if (a->unk408 > b->unk408) {
        ca = 11;
        cb = 12;
        a->unk408 = la + (up - down);
        b->unk408 -= down;
    } else if (a->unk408 < b->unk408) {
        ca = 12;
        cb = 11;
        b->unk408 = lb + (up - down);
        a->unk408 -= down;
    } else {
        ca = 0;
        cb = 0;
    }
    w = Text_Printf(D_0051F6A0, 10, 5, (u32)(a->unk408 * 100 / 4096));
    Text_Printf(D_0051F6B0, 0xE4 - w, 100, 10, 5, ca, (u32)(a->unk408 * 100 / 4096));
    Text_Printf(D_0051F6B0, 0x19C, 100, 10, 5, cb, (u32)(b->unk408 * 100 / 4096));
}

/* HUD of game type 9 [V]: filtering and blend preset 1 on buffer 1, neutral colour, Z 0xFFFF0. */
void func_003C2EF8(void) {
    Sprite_SetFilter(1, 1);
    Sprite_SetBlend(1, 1);
    D_005F6738 = 0x80808080;
    D_005F6730 = 0xFFFF0;
}

/* Per-frame fight HUD [V]: Z from func_003C0098, markers (Hud_DrawMarkers), then per side the two life bars. When a tag
 * starts (Tag_StepCounters(side, 0) in 1..15) a 30-frame counter slides the bars: fade = counter / 2 for the incoming
 * bar, 15 - fade for the other; the active member (the one that is not gFighters[side].unk0) is drawn first with
 * partner 0. Last the timer: (gRoundTimer * 6 / 7 - 1) / 60 as two digits at x 320, y 6 (func_003C6738). */
void Hud_Draw(void) {
    s32 side;
    s32 a;
    s32 b;
    s32 t;
    HudSide *h;

    D_005F6730 = func_003C0098() << 4;
    Hud_DrawMarkers();
    for (side = 0; side < 2; side++) {
        h = &D_01FAC8B8[side];
        b = 0;
        a = 15;
        if (h->unk0 == 0 && Tag_StepCounters(side, 0) != 0 && Tag_StepCounters(side, 0) < 16) {
            h->unk0 = 30;
        }
        if (func_0036E158() != 1 || D_005F5EE0 != 0 || D_005F5EE4 != 0 || func_003A9698() != 0) {
            h->unk0 = 0;
        }
        if (h->unk0 != 0) {
            h->unk0--;
            b = h->unk0 / 2;
            a = 15 - b;
        }
        if (gFighters[side].unk0 == 0) {
            Hud_DrawLifeBar(side, 1, a, 0);
            Hud_DrawLifeBar(side, 0, b, 1);
        } else {
            Hud_DrawLifeBar(side, 0, a, 0);
            Hud_DrawLifeBar(side, 1, b, 1);
        }
    }
    t = (gRoundTimer * 6 / 7 - 1) / 60;
    Sprite_SetBlend(0, 1);
    func_003C6738(0, t, 320, 6);
}

/* Fight HUD init [V]: clears D_01FAC908 and sets D_005F6E64 for game type 8. */
void Hud_Init(void) {
    D_01FAC908[0] = 0;
    if (D_0043B7F8[0] == 8) {
        D_005F6E64 = 1;
    } else {
        D_005F6E64 = 0;
    }
}
