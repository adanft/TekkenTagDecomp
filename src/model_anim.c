/* 0x3D3288-0x3D33F0, a part of module model_anim (docs/game_overview.md section 4): the tag swap of one side. The
 * rest of the module stays in the split assembly. The edges are not object edges: this part emits no data of its own,
 * so it can be cut anywhere between two functions (docs/build.md). */
#include "common.h"
#include "fighter.h"

extern Fighter gFighters[4];
extern TeamMember D_00440940[2][2];
extern u8 D_0043B7F8[];

void func_003C1A78(s32 side);
void func_003D31F8(s32 side, s32 state);
void func_0035C8D0(Fighter *f, s32 character, s32 costume);
void func_0035C850(Fighter *f, s32 n);
void func_00368168(Fighter *f);
void func_00377F58(Fighter *f);
void func_003465F0(void);
void func_00344318(void);
void func_003A82C8(Fighter *f);
void func_00370E20(Fighter *f, s32 n);
void func_0038ADF8(s32 side);
void func_0037A970(Fighter *f);
void func_0037EA08(void);

/* The tag swap of a side (from Fighter_StepMove when a tag move starts, and from func_003F0008): the fighter's active
 * and partner member indices unk0 / unk2 flip, the life and red life come from the incoming member's record, then
 * the character and costume of that member are loaded (func_0035C8D0, func_0035C850) and its body set up
 * (func_00368168, func_00377F58 body spheres, ...); practice mode (game type 7) also resets its state. */
void Fighter_TagSwap(s32 side) {
    Fighter *f = &gFighters[side];
    s16 n = f->unk2;
    s16 m;

    m = f->unk0 ^= 1;
    f->unk2 ^= 1;
    f->unk3F4 = D_00440940[side][n].unkC;
    f->unk3F8 = D_00440940[side][n].unk10;
    func_003C1A78(side);
    func_003D31F8(side, 2);
    func_0035C8D0(f, D_00440940[side][m].unk0, D_00440940[side][m].unk4);
    func_0035C850(f, 1);
    func_00368168(f);
    func_00377F58(f);
    func_003465F0();
    func_00344318();
    func_003A82C8(f);
    func_00370E20(f, 1);
    func_0038ADF8(side);
    if (D_0043B7F8[0] == 7) {
        func_0037A970(f);
        func_0037EA08();
    }
}
