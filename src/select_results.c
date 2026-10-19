/* 0x3C1470-0x3C1A78, a part of module select_results (docs/game_overview.md section 4): the team members' life at
 * round start (Battle_InitTeamLife) and the per-frame life recovery of the partner (Battle_RecoverLife), with the two
 * functions between them still in assembly. The rest of the module stays in the split assembly. The edges are not
 * object edges: this part emits no data of its own, so it can be cut anywhere between two functions (docs/build.md). */
#include "common.h"
#include "fighter.h"

extern Fighter gFighters[4];
extern TeamMember D_00440940[2][2];
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;

s32 func_003AE420(s32 side, s32 member);
void func_003AE400(s32 side, s32 member, s32 n);
s32 func_003C1350(s32 character, s32 side);
s32 func_0036E158(void);
s32 func_003A9698(void);
s32 func_003AE3C0(Fighter *f);

/* Round start: a member that func_003AE420 marks (1) gets its maximum life from func_003C1350 (by character) and
 * full life; each member's statistic unk28 restarts at 100; the fighter of the side takes the life, red life and
 * maximum of its active member (unk0) and is standing again (unk40C = 1). */
void Battle_InitTeamLife(void) {
    s32 side;
    s32 m;
    Fighter *f;
    s32 life;

    for (side = 0; side < 2; side++) {
        f = &gFighters[side];
        for (m = 0; m < 2; m++) {
            if (func_003AE420(side, m) == 1) {
                func_003AE400(side, m, 0);
                life = func_003C1350(D_00440940[side][m].unk0, side);
                D_00440940[side][m].unk14 = life;
                D_00440940[side][m].unk10 = life;
                D_00440940[side][m].unkC = D_00440940[side][m].unk14;
                D_00440940[side][m].unk1C = 0;
                D_00440940[side][m].unk18 = 0;
            }
            f->unk40C = 1;
            D_00440940[side][m].unk28 = 100;
            f->unk3F4 = D_00440940[side][f->unk0].unkC;
            f->unk3F8 = D_00440940[side][f->unk0].unk10;
            f->unk3FC = D_00440940[side][f->unk0].unk14;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/select_results", func_003C1620);

INCLUDE_ASM("asm/nonmatchings/select_results", func_003C18B8);

/* Once per simulated frame while the round runs (func_0036E158() == 1, not paused, func_003A9698 clear): the active
 * fighter regains maximum / 6000 per frame up to its red life only when func_003AE3C0 is clear and unkD2 is 0; its
 * life and red life are copied to its member record, and the partner's record regains maximum / 6000 per frame up to
 * its red life. */
void Battle_RecoverLife(s32 side) {
    Fighter *f;
    Fighter *g;
    s16 m0;
    s16 m1;

    if (func_0036E158() != 1) {
        return;
    }
    if (D_005F5EE0 != 0) {
        return;
    }
    if (D_005F5EE4 != 0) {
        return;
    }
    if (func_003A9698() != 0) {
        return;
    }
    f = &gFighters[side];
    m0 = f->unk0;
    m1 = f->unk2;
    if (func_003AE3C0(f) == 0 && f->unkD2 == 0) {
        f->unk3F4 += f->unk3FC / 6000;
        if (f->unk3F8 < f->unk3F4) {
            f->unk3F4 = f->unk3F8;
        }
    }
    g = &gFighters[side];
    D_00440940[side][m0].unkC = g->unk3F4;
    D_00440940[side][m0].unk10 = g->unk3F8;
    D_00440940[side][m1].unkC += D_00440940[side][m1].unk14 / 6000;
    if (D_00440940[side][m1].unk10 < D_00440940[side][m1].unkC) {
        D_00440940[side][m1].unkC = D_00440940[side][m1].unk10;
    }
}
