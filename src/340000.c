/* 0x340000-0x340B30: the first functions of the game code. What they belong to is not known yet. */
#include "common.h"

/* Table of 0x30-byte records at 0x521700 (D_00521700), indexed by id. Only the fields used so far are named. */
typedef struct {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ u8 pad6[0xE];
    /* 0x14 */ f32 unk14;
    /* 0x18 */ u8 pad18[0x18];
} Unk521700;

extern Unk521700 D_00521700[];

void func_00357258(f32 *values, s32 count);
float sqrtf(float x);

/* When record `id` has unk4 == 1, passes 27 copies of its unk14 to func_00357258. */
void func_00340000(s32 id) {
    f32 values[27];
    s32 i;
    Unk521700 *rec = &D_00521700[id];

    if (rec->unk4 == 1) {
        for (i = 0; i < 27; i++) {
            values[i] = rec->unk14;
        }
        func_00357258(values, 27);
    }
}

INCLUDE_ASM("asm/nonmatchings/340000", func_00340070);

INCLUDE_ASM("asm/nonmatchings/340000", func_00340398);

INCLUDE_ASM("asm/nonmatchings/340000", func_003403B8);

INCLUDE_ASM("asm/nonmatchings/340000", func_003403C8);

INCLUDE_ASM("asm/nonmatchings/340000", func_003403F8);

INCLUDE_ASM("asm/nonmatchings/340000", func_00340408);

INCLUDE_ASM("asm/nonmatchings/340000", func_00340528);

INCLUDE_ASM("asm/nonmatchings/340000", func_00340588);

INCLUDE_ASM("asm/nonmatchings/340000", func_003408B0);

/* 1 when the point (x1, y1) is within `radius` of (x0, y0). */
s32 func_00340AF0(f32 x0, f32 y0, f32 radius, f32 x1, f32 y1) {
    return sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) <= radius;
}
