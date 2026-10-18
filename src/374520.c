/* 0x374520-0x3745E8: the last functions of the object before the sound code (battle_flow, still assembly). Its
 * .rodata ends with D_00500758, so the sound object starts at Snd_Init with its .rodata at 0x500780 (the 16-byte
 * alignment of its jump tables). */
#include "common.h"

extern s32 D_005F5ED4;
extern s32 D_005F5ED8;
extern s32 D_005F5EDC;
extern u32 D_00415718[];
extern u32 D_00415728[];
/* 20 values, read from the original executable. */
extern const s16 D_00500758[20];

void func_00374520(void) {
    D_005F5ED4 = 0;
    D_005F5ED8 = 0;
    D_005F5EDC = 0;
}

s32 func_00374530(void) {
    s32 v = D_005F5ED4;

    D_005F5ED4 = 0;
    return v;
}

s32 func_00374540(void) {
    return D_005F5EDC;
}

void func_00374548(void) {
    u16 held = D_00415718[0] | D_00415718[1] | D_00415718[2] | D_00415718[3];
    u16 trig = D_00415728[0] | D_00415728[1] | D_00415728[2] | D_00415728[3];

    if ((held & 1) && (trig & 2)) {
        D_005F5ED4 = 1;
        D_005F5ED8++;
        if (D_005F5ED8 >= 20) {
            D_005F5ED8 = 0;
        }
        D_005F5EDC = D_00500758[D_005F5ED8];
    }
}
