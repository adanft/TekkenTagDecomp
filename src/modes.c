/* 0x3B90C0-0x3BB310 (module modes, docs/game_overview.md section 4; docs/systems/modes.md): the mode change and the
 * first mode handlers (transition, boot) with their helpers. .rodata 0x51E970-0x51EA60 is its own (subsegment, every
 * table included in place); the start is a cut [I], the end an object edge [V: scene3d's .rodata follows]. */
#include "common.h"

#include "fighter.h"

/* One rumble step source (D_01FAB148): a pointer into a rumble script and its state. */
typedef struct {
    /* 0x0 */ u16 *script;
    /* 0x4 */ u8 on;
    /* 0x5 */ u8 level;
    /* 0x6 */ u16 time;
} RumbleTrack;

/* Rumble state of one port (D_01FAB148, 2 x 0x44 bytes). */
typedef struct {
    /* 0x00 */ RumbleTrack small[4];
    /* 0x20 */ RumbleTrack big[4];
    /* 0x40 */ s16 unk40;
    /* 0x42 */ s16 unk42;
} RumblePort;

/* Actuator state of one pad (D_0043BC10, 4 x 8 bytes). */
typedef struct {
    /* 0x0 */ u8 act[6];
    /* 0x6 */ u8 unk6;
    /* 0x7 */ u8 unk7;
} PadAct;

/* The game set-up block (D_0043B7F8 and the mode set-ups). */
typedef struct {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 unk1;
    /* 0x02 */ u8 unk2;
    /* 0x03 */ u8 unk3;
    /* 0x04 */ u8 unk4;
    /* 0x05 */ u8 unk5;
    /* 0x06 */ u8 unk6;
    /* 0x07 */ u8 unk7;
    /* 0x08 */ u8 unk8;
    /* 0x09 */ u8 unk9;
    /* 0x0A */ u8 unkA;
    /* 0x0B */ u8 unkB;
    /* 0x0C */ u8 unkC;
    /* 0x0D */ u8 unkD;
    /* 0x0E */ u8 unkE;
    /* 0x0F */ u8 unkF;
    /* 0x10 */ u8 unk10;
    /* 0x11 */ u8 unk11;
    /* 0x12 */ u8 unk12[2];
    /* 0x14 */ u16 unk14;
    /* 0x16 */ u16 unk16;
    /* 0x18 */ u8 unk18[4];
    /* 0x1C */ u8 unk1C;
    /* 0x1D */ u8 unk1D[2];
    /* 0x1F */ u8 unk1F;
    /* 0x20 */ u8 unk20;
    /* 0x21 */ u8 unk21[3];
    /* 0x24 */ s32 unk24;
    /* 0x28 */ s32 unk28;
    /* 0x2C */ s32 unk2C;
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ u8 unk38[8];
    /* 0x40 */ struct {
        s16 unk0;
        s16 unk2;
        s16 unk4;
    } unk40[2];
    /* 0x4C */ s16 unk4C;
    /* 0x4E */ s16 unk4E;
    /* 0x50 */ s32 unk50;
} GameSetup;

/* The mode change request (D_005F6508): Mode_Change stores the next and the previous mode here. */
typedef struct {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 next;
    /* 0x2 */ u8 prev;
    /* 0x3 */ u8 unk3;
    /* 0x4 */ u8 unk4;
    /* 0x5 */ u8 unk5;
    /* 0x6 */ u8 unk6;
    /* 0x7 */ u8 unk7;
} ModeReq;

extern ModeReq D_005F6508;
extern s32 D_005F6510;
extern s32 D_005F6E08;
extern s32 D_005F6E0C;
/* The boot file loads (D_01FAB110): `count` requests, then the stage file. */
typedef struct {
    /* 0x00 */ s32 count;
    /* 0x04 */ s32 handle[12];
    /* 0x34 */ s32 stage;
} BootLoads;

extern BootLoads D_01FAB110;
extern u32 D_01FAB0C0[8];
extern s32 D_01FAB0E8[7];
extern s16 gMode;
extern s16 gSubMode;
extern u16 gFrameCount;
extern Fighter gFighters[];
extern u8 D_005F6509;
extern volatile u8 D_005F650A; /* fake: volatile keeps its store out of the delay slot of the call that follows (Mode_Change) */
extern u8 D_005F5C58;
extern u16 D_005F5C60;
extern u16 D_005F5C64;
extern u16 D_005F5C66;
extern u16 D_005F5C68;
extern u16 D_005F5C6A;
extern u16 D_005F5C6E;
extern u16 D_005F5C7A;
extern u16 D_005F5C80[2];
extern u16 D_005F5CB0[2];
extern u16 D_005F5CBC;
extern u8 D_005F5CBE;
extern u8 D_005F5CBF;
extern s32 D_005F5CC0;
extern s32 D_005F5CC4;
extern s32 D_005F5EF8;
extern u16 D_005F6E10;
extern u16 D_005F6E12;
extern u16 D_005F6E14;
extern u8 D_005F6E16;
extern s16 D_005F6E32;
extern u32 D_00415728[];
extern GameSetup D_0043B7F8;
/* One team (D_00440940, 2 x 0x60): the overlay names of the two members' character and costume words. */
typedef struct {
    TeamMember m[2];
} TeamSide;
extern TeamSide D_00440940[];
extern u8 D_005F5CCC;
extern u8 D_005F5CCD;
extern u16 D_005F5CB4;
extern u8 D_0043BC00[];
extern PadAct D_0043BC10[];
extern u16 *D_0043BB20[][2];
extern RumblePort D_01FAB148[];
extern s16 D_01FAB1D0[][4];
extern const char D_0051E9F8[];
extern const u8 D_0051EA10[];
extern const u8 D_0051EA30[];

/* The original calls __fixunssfsi for a float to u32 conversion; this compiler emits fptoui (decomp_guide.md, T17 b4). */
#ifndef PORT
__asm__(".set fptoui, __fixunssfsi"); /* fake */
#endif

/* callees */
void func_003992C8(s32 value);
void func_003753D0(void);
void Bgm_Stop(void);
s32 func_003BDEE0(void);
s32 func_003BDEB8(void);
void func_0035CCE0(s32 i);
void func_003CCFF8(void);
void func_003CD678(void);
void func_003CD288(void);
void func_003BFA48(void);
void func_003B02C8(s32 side);
s32 func_003AFFB8(s32 i, s32 a);
void Snd_PlaySe(s32 id, s32 player);
void func_003AE738(s32 a, s32 b);
s32 Text_Printf(const char *fmt, ...);
s32 Pad_IsMultitap(void);
s32 func_003B6220(void);
s32 scePadSetActDirect(s32 port, s32 slot, const u8 *data);
void *memset(void *p, s32 c, s32 n);
s32 func_003B0078(s32 a, s32 b);
s32 func_003B79E0(void);
s32 File_Request(s32 id, u32 dest, s32 flags, s32 toIop);

void func_003B9078(s32 a);
void func_00361AA8(void);
s32 func_00361978(void);
void func_003C4868(void);
void func_003C4990(s32 r, s32 g, s32 b, s32 a);

void func_003B7AB8(void);
void func_003B07F8(void);
void Fighter_ClearLife(s32 side);
void func_003B7A58(void);
void func_003645D8(s32 n);
void Unlock_ApplyAll(void);
void func_003BF810(void);
void func_003B6218(void);

void func_003BFF88(void);
void func_003BB8B0(s32 a);
s32 func_003637D0(void);
u64 *func_003639E0(void);
void func_00366A20(u64 *p, s32 n);
void func_003616F8(void);
s32 func_00361710(void);
void func_003BFF38(void);
void Options_Apply(void);
s32 func_0035CED8(s32 id);
void *func_003453F0(void);
u32 func_003B8760(void);
s32 func_00345400(void);
void exit(s32 status) __attribute__((noreturn));
void func_00366AB8(f32 a, f32 b);
void File_WaitRelease(s32 *handle);
s32 File_Release(s32 handle);
u8 *func_003B8778(void);
void Sprite_UploadBank(s32 bank, u64 *data, s32 size, s32 r);
void func_003638F0(void);
void func_00345410(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
void Snd_Init(void);
void func_003C4828(void);

/* this file */
void Mode_Change(s32 mode);
void Mode_Transition(void);
void func_003B94E8(u32 *dest);
void Mode_Boot(void);
void func_003B9AE0(GameSetup *s, s32 kind);
void func_003B9B90(GameSetup *s, s32 kind);
void func_003B9C80(GameSetup *s, s32 kind);
void func_003B9D70(GameSetup *s, s32 kind);
void func_003B9E08(GameSetup *s, s32 kind);
void func_003B9EA8(GameSetup *s, s32 kind);
void func_003B9F50(GameSetup *s, s32 kind);
void func_003B9FE8(GameSetup *s, s32 kind);
void func_003BA080(GameSetup *s, s32 kind);
void func_003BA118(GameSetup *s, s32 kind);
void Game_Start(u32 kind, s32 mask);
s32 func_003BA450(s32 side);
s32 func_003BA480(void);
void func_003BA4B8(s32 side);
void func_003BA4D8(s32 side);
void func_003BA520(s32 v);
void func_003BA528(s32 side);
void func_003BA548(void);
s32 Game_UpdateContinue(s32 side);
s32 func_003BA750(s16 i);
void func_003BA770(s32 port, s32 *slot, s32 *portOut);
void func_003BA818(s32 i, s32 v);
void func_003BA830(s16 i, s32 v);
s32 func_003BA870(s32 i);
s32 func_003BA888(s32 i, s32 a, s32 b);
void func_003BA938(s32 a, s32 b);
void func_003BA990(s32 port, s32 v, s32 i);
void func_003BAA20(s32 port, s32 i);
void func_003BAA38(void);
void func_003BAA90(void);
void func_003BAB38(RumbleTrack *t, u16 *script);
void func_003BAB58(s32 big, RumbleTrack *t);
s32 func_003BAC08(RumbleTrack *t);
void func_003BAC38(RumblePort *p);
void func_003BAC88(u16 *req, RumblePort *p);
void func_003BAD98(s32 i);
void func_003BAE40(void);
void func_003BAEF0(void);
void func_003BB1F8(s32 i);
void func_003BB218(s32 i, s32 port);
void func_003BB228(u8 side, u8 kind);
s32 func_003BB308(void);


void Mode_Change(s32 mode) {
    if (mode != 3 && gMode != 3 && gMode != 6 && gMode != 7) {
        func_003992C8(0);
    }
    func_003753D0();
    Bgm_Stop();
    D_005F6509 = mode;
    D_005F650A = gMode;
    if (func_003BDEE0() != 0 || func_003BDEB8() != 0) {
        if (mode == 3) {
            gMode = mode;
            goto end;
        }
    }
    if (gMode == 9 || gMode == 0x10) {
        gMode = mode;
    } else {
        gMode = 1;
    }
end:
    gSubMode = 0;
}
void Mode_Transition(void) {
    ModeReq *r = &D_005F6508;
    u32 fade = 0;
    s32 n;

    switch (gSubMode) {
    case 0:
        func_003B9078(1);
        D_005F6E0C = 0;
        D_005F5CBC = 1;
        D_005F6E08 = 0;
        func_00361AA8();
        D_005F6510 = gFrameCount;
        Bgm_Stop();
        if (r->next != 2) {
            if (r->prev == 13) {
                func_003C4868();
                gSubMode = 0;
                gMode = r->next;
                return;
            }
            gSubMode = 10;
            break;
        }
        gSubMode = 2;
        func_003992C8(0);
    case 2:
    {
            f32 t;
            f32 d;

            n = func_00361978();
            t = (gFrameCount - D_005F6510) & 0xFFFF;
            d = 30.0f;
            if (t < d) {
                fade = t / d * 128.0f;
                break;
            }
            fade = 0x80;
            if (n == 0) {
                D_005F6510 = gFrameCount;
                gSubMode = 9;
            }
            break;
    }
    case 9:
        gSubMode = 13;
        fade = 0x80;
        break;
    case 13:
        gSubMode = 3;
        fade = 0x80;
        break;
    case 3:
        D_005F6E08 = 1;
        fade = 0x80;
        if (r->unk5 == 0) {
            func_003992C8(1);
            gSubMode = 0;
            gMode = r->next;
            break;
        }
        gSubMode = 4;
    case 4:
    {
            f32 t;

            t = (gFrameCount - D_005F6510) & 0xFFFF;
            fade = 0x80;
            if (120.0f < t) {
                D_005F6510 = gFrameCount;
                gSubMode = 5;
            }
            break;
    }
    case 5:
    {
            f32 t;
            f32 d;

            d = 30.0f;
            t = (gFrameCount - D_005F6510) & 0xFFFF;
            if (t > d) {
                D_005F6510 = 0;
                gSubMode = 6;
                break;
            }
            fade = (1.0f - t / d) * 128.0f;
            break;
    }
    case 6:
        gMode = r->next;
        gSubMode = 0;
        if (r->prev == 13) {
            func_003C4868();
            return;
        }
        break;
    case 10:
        n = func_00361978();
        if (n == 0) {
            gSubMode = 0;
            gMode = r->next;
        } else if (n == 2) {
            func_003992C8(0);
            gSubMode = 11;
        }
        break;
    case 11:
        if (func_00361978() == 0) {
            gSubMode = 12;
            D_005F6510 = gFrameCount;
        }
        break;
    case 12:
    {
            f32 t;

            t = (gFrameCount - D_005F6510) & 0xFFFF;
            if (6.0f < t) {
                gSubMode = 0;
                gMode = r->next;
            }
            break;
    }
    }
    if ((D_00415728[0] | D_00415728[1] | D_00415728[2] | D_00415728[3]) & 0x800) {
        D_005F6E0C = 1;
    }
    if (D_005F6E08 != 0 && D_005F6E0C != 0) {
        Mode_Change(3);
        Snd_PlaySe(4, 0);
    }
    if (gSubMode != 0 && gSubMode != 10 && gSubMode != 11 && gSubMode != 12) {
        func_003C4990(fade, fade, fade, 0x80);
    }
}
void func_003B94E8(u32 *dest) {
    BootLoads *l = &D_01FAB110;

    l->stage = -1;
    l->handle[0] = File_Request(2, 0x128000, 0, 1);
    l->handle[1] = File_Request(1, 0x5010, 0, 1);
    l->handle[2] = File_Request(3, dest[0], 0, 0);
    l->handle[3] = File_Request(4, dest[1], 0, 0);
    l->handle[4] = File_Request(5, dest[2], 0, 0);
    l->handle[5] = File_Request(6, dest[3], 0, 0);
    l->handle[6] = File_Request(7, dest[4], 0, 0);
    l->handle[7] = File_Request(8, dest[5], 0, 0);
    l->handle[8] = File_Request(9, dest[6], 0, 0);
    l->handle[9] = File_Request(10, dest[7], 0, 0);
    D_01FAB110.count = 9;
}
RODATA_ALIGN16();
INCLUDE_RODATA("asm/nonmatchings/modes", jtbl_0051E9B0);
/* NON_MATCHING: 18 lines, FPU registers of the case 3 and case 5 fades (the original keeps the divisor in f12 / the result in f13) */
#if 0
/* NON_MATCHING: 18 lines, FPU registers of the case 3 and case 5 fades (the original keeps the divisor in f12 / the result in f13) */
void Mode_Boot(void) {
    ModeReq *r = &D_005F6508;
    s32 i;
    s32 dt;
    u32 end;

    switch (gSubMode) {
    case 0:
        func_003B9078(0);
        func_003BFF88();
        func_003BB8B0(0x80);
        i = func_003637D0();
        func_00366A20(func_003639E0(), i);
        func_003616F8();
        gSubMode = 1;
        break;
    case 1:
        if (func_00361710() != 0) {
            break;
        }
        func_003BFF38();
        Options_Apply();
        D_005F5CBC = 1;
        r->next = 2;
        r->prev = 4;
        D_005F6510 = gFrameCount;
        gSubMode = 2;
        D_01FAB0E8[0] = func_0035CED8(3);
        D_01FAB0E8[1] = func_0035CED8(4);
        D_01FAB0E8[2] = func_0035CED8(5);
        D_01FAB0E8[3] = func_0035CED8(6);
        D_01FAB0E8[4] = func_0035CED8(7);
        D_01FAB0E8[5] = func_0035CED8(8);
        D_01FAB0E8[6] = func_0035CED8(9);
        D_01FAB0C0[0] = (u32)func_003639E0();
        D_01FAB0C0[1] = (u32)func_003453F0();
        D_01FAB0C0[2] = (D_01FAB0C0[1] + D_01FAB0E8[1] + 15) & 0xFFFFFFF0;
        D_01FAB0C0[3] = (D_01FAB0C0[2] + D_01FAB0E8[2] + 15) & 0xFFFFFFF0;
        D_01FAB0C0[4] = (D_01FAB0C0[3] + D_01FAB0E8[3] + 15) & 0xFFFFFFF0;
        D_01FAB0C0[5] = (D_01FAB0C0[4] + D_01FAB0E8[4] + 15) & 0xFFFFFFF0;
        D_01FAB0C0[6] = (D_01FAB0C0[5] + D_01FAB0E8[5] + 15) & 0xFFFFFFF0;
        D_01FAB0C0[7] = func_003B8760();
        end = (D_01FAB0C0[6] + D_01FAB0E8[6] + 15) & 0xFFFFFFF0;
        if (D_01FAB0C0[1] + func_00345400() < end) {
            exit(1);
        }
        func_003B94E8(D_01FAB0C0);
    case 2:
        {
            f32 t;
            f32 d;

            t = (gFrameCount - D_005F6510) & 0xFFFF;
            d = 30.0f;
            if (d < t) {
                D_005F6510 = gFrameCount;
                gSubMode = 3;
                t = 1.0f;
            } else {
                t = t / d;
            }
            func_00366AB8(t, 0.0f);
            return;
        }
    case 3:
        {
            f32 t;
            f32 d;
            f32 v;

            t = (gFrameCount - D_005F6510) & 0xFFFF;
            d = 90.0f;
            if (d < t) {
                D_005F6510 = gFrameCount;
                gSubMode = 4;
                func_003992C8(1);
                v = 1.0f;
            } else {
                v = t / d;
            }
            func_00366AB8(1.0f, v);
            return;
        }
    case 4:
        File_WaitRelease(&D_01FAB110.handle[D_01FAB110.count]);
        for (i = D_01FAB110.count - 1; i >= 0; i--) {
            File_Release(D_01FAB110.handle[i]);
        }
        func_003992C8(0);
        D_01FAB110.stage = File_Request(12, (u32)func_003B8778(), 0, 0);
        Sprite_UploadBank(0, (u64 *)D_01FAB0C0[0], 0x48481, D_01FAB0E8[0]);
        func_003638F0();
        func_00345410(D_01FAB0C0[1], D_01FAB0C0[2], D_01FAB0C0[3], D_01FAB0C0[4], D_01FAB0C0[5], D_01FAB0C0[6]);
        Snd_Init();
        gSubMode = 5;
        D_005F6510 = gFrameCount;
        func_00366AB8(1.0f, 1.0f);
        return;
    case 5:
        {
            f32 t;
            f32 d;
            f32 v;

            t = (gFrameCount - D_005F6510) & 0xFFFF;
            d = 30.0f;
            if (d < t) {
                D_005F6510 = gFrameCount;
                gSubMode = 6;
                v = 0.0f;
            } else {
                v = 1.0f - t / d;
            }
            func_00366AB8(v, v);
            return;
        }
    case 6:
        if (D_01FAB110.stage >= 0) {
            File_WaitRelease(&D_01FAB110.stage);
        }
        func_003C4828();
        gSubMode = 0;
        gMode = r->next;
        D_005F6510 = 0;
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/modes", Mode_Boot);
#endif
void func_003B9AE0(GameSetup *s, s32 kind) {
    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unk1 = 1;
    s->unk3 = D_005F5C6A;
    s->unk2 = 1;
    s->unk4 = 1;
    s->unk5 = 0;
    s->unkC = D_005F5C60;
    s->unk9 = D_005F5C68;
    s->unkB = D_005F5C66;
    s->unkD = D_005F5C58;
    s->unkA = D_005F5C64;
    s->unk6 = 0;
    s->unkE = D_005F5C7A;
    s->unk7 = 0;
    s->unk8 = D_005F5C6E;
    func_003CCFF8();
    func_003CD678();
    func_003CD288();
}
void func_003B9B90(GameSetup *s, s32 kind) {
    s32 i;

    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unk2 = 0;
    s->unk1 = 1;
    s->unk3 = 0;
    s->unk4 = 1;
    s->unk5 = 1;
    s->unkC = D_005F5C60;
    s->unk9 = 0;
    s->unkB = D_005F5C66;
    s->unkD = 1;
    s->unk8 = 1;
    s->unkE = 0;
    s->unk6 = 0;
    s->unk7 = 0;
    s->unkA = D_005F5C64;
    func_003CCFF8();
    for (i = 0; i < 2; i++) {
        s->unk40[i].unk0 = 0;
        s->unk40[i].unk2 = 100;
        s->unk40[i].unk4 = 0x180;
    }
    s->unk4C = 0;
    func_003BFA48();
    s->unk1F = 0;
    s->unk50 = 0xF0;
    func_003CD288();
}
void func_003B9C80(GameSetup *s, s32 kind) {
    s32 i;

    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unk2 = 0;
    s->unk1 = 1;
    s->unk3 = 0;
    s->unk4 = 1;
    s->unk5 = 1;
    s->unkC = D_005F5C60;
    s->unk9 = 0;
    s->unkB = D_005F5C66;
    s->unkD = 1;
    s->unk8 = 1;
    s->unkE = 0;
    s->unk6 = 0;
    s->unk7 = 0;
    s->unkA = D_005F5C64;
    func_003CCFF8();
    for (i = 0; i < 2; i++) {
        s->unk40[i].unk0 = 0;
        s->unk40[i].unk2 = 100;
        s->unk40[i].unk4 = 0x180;
    }
    s->unk4C = 0;
    func_003BFA48();
    s->unk1F = 0;
    s->unk50 = 0xF0;
    func_003CD288();
}
void func_003B9D70(GameSetup *s, s32 kind) {
    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unk1 = 1;
    s->unk2 = 0;
    s->unk3 = 0;
    s->unk4 = 0;
    s->unk5 = 0;
    s->unkC = 0;
    s->unk9 = 0;
    s->unkE = 0;
    s->unkA = D_005F5C64;
    s->unk6 = 0;
    s->unk7 = 0;
    s->unkB = 5;
    s->unkD = 1;
    s->unk8 = D_005F5C6E;
    func_003CCFF8();
    func_003CD288();
}
void func_003B9E08(GameSetup *s, s32 kind) {
    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unkB = 2;
    s->unkD = 1;
    s->unk1 = 0;
    s->unk2 = 0;
    s->unk3 = 0;
    s->unk4 = 0;
    s->unk5 = 0;
    s->unk9 = 0;
    s->unkE = 0;
    s->unkA = D_005F5C64;
    s->unkC = 0;
    s->unk6 = 0;
    s->unk7 = 0;
    s->unk8 = D_005F5C6E;
    func_003CCFF8();
    func_003CD678();
    func_003CD288();
}
void func_003B9EA8(GameSetup *s, s32 kind) {
    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unk1 = 1;
    s->unk2 = 1;
    s->unk3 = D_005F5C6A;
    s->unk5 = 0;
    s->unk4 = 1;
    s->unkC = D_005F5C60;
    s->unk9 = D_005F5C68;
    s->unkB = D_005F5C66;
    s->unkD = D_005F5C58;
    s->unkE = 0;
    s->unkA = D_005F5C64;
    s->unk6 = 0;
    s->unk7 = 0;
    s->unk8 = D_005F5C6E;
    func_003CCFF8();
    func_003CD288();
}
void func_003B9F50(GameSetup *s, s32 kind) {
    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unk1 = 1;
    s->unk2 = 1;
    s->unk3 = D_005F5C6A;
    s->unk4 = 1;
    s->unk5 = 0;
    s->unkC = 0;
    s->unk9 = 0;
    s->unkB = 2;
    s->unkD = 1;
    s->unkE = 0;
    s->unkA = D_005F5C64;
    s->unk6 = 0;
    s->unk7 = 0;
    s->unk8 = 1;
    func_003CCFF8();
    func_003CD288();
}
void func_003B9FE8(GameSetup *s, s32 kind) {
    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unk1 = 0;
    s->unk2 = 0;
    s->unk3 = 0;
    s->unk4 = 1;
    s->unk5 = 0;
    s->unkE = 0;
    s->unkA = D_005F5C64;
    s->unkD = 1;
    s->unkC = 0;
    s->unkB = 2;
    s->unk6 = 1;
    s->unk9 = 0;
    s->unk7 = 0;
    s->unk8 = D_005F5C6E;
    func_003CCFF8();
    func_003CD288();
}
void func_003BA080(GameSetup *s, s32 kind) {
    func_0035CCE0(0);
    if (kind == 3) {
        func_0035CCE0(2);
    }
    s->unk4 = 0;
    s->unk1 = 1;
    s->unk2 = 1;
    s->unk3 = 1;
    s->unk5 = 1;
    s->unkC = D_005F5C60;
    s->unkB = D_005F5C66;
    s->unkD = D_005F5C58;
    s->unkE = 0;
    s->unkA = 0;
    s->unk9 = 0;
    s->unk7 = 0;
    s->unk6 = 1;
    s->unk8 = 1;
    func_003CCFF8();
    func_003CD288();
}
void func_003BA118(GameSetup *s, s32 kind) {
    s->unk7 = 1;
    s->unkB = 2;
    s->unk1 = 0;
    s->unk2 = 0;
    s->unk3 = 0;
    s->unk4 = 0;
    s->unk5 = 0;
    s->unk6 = 0;
    s->unk8 = 0;
    s->unkC = 0;
    s->unk9 = 0;
    s->unkD = D_005F5C58;
    s->unkE = 0;
    s->unkA = 0;
    D_005F5CBC = 1;
}
INCLUDE_RODATA("asm/nonmatchings/modes", jtbl_0051E9D0);
/* NON_MATCHING: 8 lines, the 0x130-byte clear loop takes the counter and pointer in swapped registers and is not strength-reduced as the original */
#if 0
/* NON_MATCHING: 8 lines, the 0x130-byte clear loop takes the counter and pointer in swapped registers and is not strength-reduced as the original */
void Game_Start(u32 kind, s32 mask) {
    GameSetup *s = &D_0043B7F8;
    s32 i;
    s32 j;
    u8 *p;

    Bgm_Stop();
    func_003B7AB8();
    func_003B07F8();
    D_0043B7F8.unk0 = kind;
    s->unkF = 0;
    s->unk11 = 0;
    s->unk10 = 0;
    s->unk14 = 0;
    s->unk16 = 0;
    for (i = 0; i < 2; i++) {
        Fighter_ClearLife(i);
        D_005F5C80[i] = 0;
        D_005F5CB0[i] = 0;
        D_00440940[i].m[0].unk0 = -1;
        D_00440940[i].m[0].unk4 = -1;
        D_00440940[i].m[1].unk0 = -1;
        D_00440940[i].m[1].unk4 = -1;
    }
    func_003B7A58();
    func_003645D8(0);
    s->unk24 = 0;
    s->unk28 = 0;
    s->unk2C = 0;
    s->unk30 = 0;
    s->unk34 = 0;
    Unlock_ApplyAll();
    func_003BF810();
    p = (u8 *)&D_0043B7F8.unk40;
    for (j = 0; j < 0x130; j++) {
        p[j] = 0;
    }
    D_005F5CCD = 0;
    D_005F5CCC = s->unk24;
    D_005F5CB4 = 0;
    D_005F5CBC = 0;
    for (i = 0; i < 2; i++) {
        if ((mask >> i) & 1) {
            func_003BA4B8(i);
            D_005F5CB0[i] = 1;
        }
    }
    switch (kind) {
    case 0:
        func_003B9AE0(s, mask);
        break;
    case 1:
        func_003B9B90(s, mask);
        break;
    case 3:
        func_003B9E08(s, mask);
        break;
    case 7:
        func_003B9D70(s, mask);
        break;
    case 5:
        func_003B9EA8(s, mask);
        break;
    case 6:
        func_003B9C80(s, mask);
        break;
    case 8:
        func_003B9F50(s, mask);
        break;
    case 4:
        func_003B9FE8(s, mask);
        break;
    case 2:
        func_003BA080(s, mask);
        break;
    case 9:
        func_003BA118(s, mask);
        break;
    }
    for (i = 0; i < 2; i++) {
        func_003AE738(0, i);
    }
    func_003B6218();
    gSubMode = 0;
    gMode = 4;
}
#else
INCLUDE_ASM("asm/nonmatchings/modes", Game_Start);
#endif
s32 func_003BA450(s32 side) {
    s32 a = D_005F5CB0[side] != 0;
    s32 b = D_005F5C80[side] != 0;
    return (a << 1) | b;
}
s32 func_003BA480(void) {
    s32 three = 3; /* fake (decomp-permuter): with the loop below, keeps the base in a1 and the result in v0 */
    u8 k = D_0043B7F8.unk1F;

    do { /* fake */
        return k != three && k != 0 && D_0043B7F8.unk20 == D_0043B7F8.unk1C;
    } while (0);
}
void func_003BA4B8(s32 side) {
    D_005F5CC0 = 0;
    D_005F5CBE = side;
    D_005F5CC4 = 0;
    D_005F5CBF = (side + 1) & 1;
}
void func_003BA4D8(s32 side) {
    if (side == D_005F5CBE) {
        D_005F5CC0++;
        if (D_005F5CC0 >= 10000) {
            D_005F5CC0 = 9999;
        }
    } else {
        D_005F5CC0 = 1;
        D_005F5CBF = (side + 1) & 1;
        D_005F5CBE = side;
    }
}
void func_003BA520(s32 v) {
    D_005F5CBF = v;
}
void func_003BA528(s32 side) {
    D_005F5CB0[side] = 0;
    D_005F5C80[side] = 0;
    func_003B02C8(side);
}
void func_003BA548(void) {
    D_005F6E14 = 0x382;
    D_005F6E10 = 9;
    D_005F6E12 = 0;
}
INCLUDE_RODATA("asm/nonmatchings/modes", D_0051E9F8);
s32 Game_UpdateContinue(s32 side) {
    u32 btn = 0;
    s32 i;

    for (i = 0; i < 2; i++) {
        btn |= D_00415728[func_003AFFB8(side, i)];
    }
    if (btn & 0x800) {
        D_005F5CB0[side] = 1;
        D_0043B7F8.unk14++;
        Snd_PlaySe(5, 0);
        func_003AE738(1, side);
        if (D_0043B7F8.unk9 == 0) {
            D_005F5C80[side] = 1;
            return 1;
        }
        D_005F5C80[side] = 0;
        return 2;
    }
    if (((u16)D_00415728[0] | (u16)D_00415728[1] | (u16)D_00415728[2] | (u16)D_00415728[3]) & 0xF4) {
        if (D_005F6E14 < 100) {
            D_005F6E14 = 0;
        } else {
            D_005F6E14 -= 100;
        }
    }
    if (D_005F6E14 == 0) {
        return -1;
    }
    D_005F6E14--;
    D_005F6E10 = (D_005F6E14 + 100) / 100;
    D_005F6E12 = (D_005F6E14 + 100) % 100;
    if (D_005F6E10 < 9) {
        if (D_005F6E10 == 0) {
            if (D_005F6E12 == 99) {
                Snd_PlaySe(0x1056, 0);
            }
        } else if (D_005F6E12 == 99) {
            Snd_PlaySe(D_005F6E10 + 0x1047, 0);
        }
    }
    Text_Printf(D_0051E9F8, 0x56, 0x168, 0xA, D_005F6E10);
    return 0;
}
s32 func_003BA750(s16 i) {
    return D_0051EA30[i];
}
/* NON_MATCHING: 18 lines, case 0 store is cross-jumped into the case 1 tail; the original keeps two `b` + `sw zero` blocks */
#if 0
/* NON_MATCHING: 18 lines, case 0 store is cross-jumped into the case 1 tail; the original keeps two `b` + `sw zero` blocks */
void func_003BA770(s32 port, s32 *slot, s32 *portOut) {
    *slot = 0;
    *portOut = 0;
    switch (port) {
    case 0:
        *slot = 0;
        return;
    case 1:
        if (Pad_IsMultitap() == 0) {
            *portOut = port;
            *slot = 0;
        } else {
            *portOut = 0;
            *slot = port;
        }
        break;
    case 2:
        *portOut = 0;
        *slot = port;
        break;
    case 3:
        *portOut = 0;
        *slot = port;
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/modes", func_003BA770);
#endif
void func_003BA818(s32 i, s32 v) {
    PadAct *p = D_0043BC10;
    p[i].unk6 = v;
}
void func_003BA830(s16 i, s32 v) {
    D_0043BC10[func_003BA750(i)].unk7 = v;
}
s32 func_003BA870(s32 i) {
    return D_0043BC10[i].unk7;
}
s32 func_003BA888(s32 i, s32 a, s32 b) {
    s32 slot[2];
    PadAct *p = &D_0043BC10[i];

    slot[1] = 0;
    slot[0] = 0;
    if (p->unk6 == 0 || func_003B6220() != 0) {
        a = 0;
        b = 0;
    }
    p->act[0] = a;
    p->act[1] = b;
    if (func_003BA870(i) != 0) {
        func_003BA770(i, &slot[0], &slot[1]);
        scePadSetActDirect(slot[1], slot[0], p->act);
    }
    return 0;
}
void func_003BA938(s32 a, s32 b) {
    u8 act[6];

    memset(act, 0, 6);
    if ((a ^ b) & b) {
        scePadSetActDirect(1, 0, act);
    }
}
void func_003BA990(s32 port, s32 v, s32 i) {
    s32 j;

    if (port >= 2) {
        for (j = 0; j < 2; j++) {
            func_003BA990(j, v, i);
        }
    } else if (i < 4) {
        D_01FAB1D0[port][i] = v;
    }
}
INCLUDE_RODATA("asm/nonmatchings/modes", D_0051EA10);
INCLUDE_RODATA("asm/nonmatchings/modes", D_0051EA30);
void func_003BAA20(s32 port, s32 i) {
    func_003BA990(port, i, D_0051EA10[i]);
}
void func_003BAA38(void) {
    s32 i, j;
    s16 *p;

    for (i = 0; i < 2; i++) {
        p = D_01FAB1D0[i];
        for (j = 0; j < 4; j++) {
            *p++ = 0;
        }
    }
    D_005F6E16 = 0;
}
void func_003BAA90(void) {
    s32 i, j;
    s16 *p;
    s32 k;

    for (i = 0; i < 2; i++) {
        p = D_01FAB1D0[i];
        for (j = 0; j < 4; j++) {
            *p++ = 0;
        }
    }
    for (k = 0; k < 4; k++) {
        func_003BA818(k, 0);
        func_003BA888(k, 0, 1);
    }
    D_005F6E16 = 0;
}
void func_003BAB38(RumbleTrack *t, u16 *script) {
    if (*script & 0xF000) {
        t->script = script;
        t->time = 0;
    }
}
/* NON_MATCHING: 60 lines, register allocation of the script word fields (the original precomputes len / op in t2 / t0 and copies the track pointer to a3) */
#if 0
/* NON_MATCHING: 60 lines, register allocation of the script word fields (the original precomputes len / op in t2 / t0 and copies the track pointer to a3) */
void func_003BAB58(s32 big, RumbleTrack *t) {
    u16 *p = t->script;
    u16 w;
    u32 lvl;
    u32 len;
    u32 op;
    s32 mask;
    s32 on;

    t->on = 0;
    if (p != NULL) {
        w = *p;
        lvl = (w & 0xF00) >> 8;
        len = w & 0xFF;
        op = w & 0xF000;
        if (big) {
            t->level = lvl * 16 + 15;
        } else {
            mask = 1 << (gFrameCount & 7);
            on = 1;
            if (!(D_0043BC00[lvl] & mask)) {
                on = 0;
            }
            t->level = on;
        }
        t->on = 1;
        if (++t->time >= len) {
            t->time = 0;
            if (op == 0x1000) {
                t->script = p + 1;
                return;
            }
            t->script = NULL;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/modes", func_003BAB58);
#endif
s32 func_003BAC08(RumbleTrack *t) {
    s32 level = 0;
    s32 i;
    for (i = 0; i < 4; i++, t++) {
        if (t->on != 0) {
            level = t->level;
        }
    }
    return level;
}
void func_003BAC38(RumblePort *p) {
    s32 i, j;
    RumbleTrack *t;

    for (i = 0; i < 2; i++) {
        t = (RumbleTrack *)((u8 *)p + i * 32);
        for (j = 0; j < 4; j++, t++) {
            t->script = NULL;
            t->time = 0;
            t->on = 0;
            t->level = 0;
        }
    }
}
void func_003BAC88(u16 *req, RumblePort *p) {
    s32 i;
    s16 id;

    for (i = 0; i < 4; i++, req++) {
        id = *(volatile u16 *)req; /* fake: lhu, then the sign extension */
        if (id != 0) {
            func_003BAB38(&p->small[i], D_0043BB20[id][0]);
            func_003BAB38(&p->big[i], D_0043BB20[id][1]);
            *req = 0;
        }
    }
    for (i = 0; i < 4; i++) {
        func_003BAB58(0, &p->small[i]);
        func_003BAB58(1, &p->big[i]);
    }
    p->unk40 = func_003BAC08(p->small);
    p->unk42 = func_003BAC08(p->big);
}
void func_003BAD98(s32 i) {
    s32 side = func_003B0078(1, i);
    s32 own = i == func_003AFFB8(side, gFighters[side].unk0);
    s16 a;
    s16 b;

    if (D_005F5CB0[side] != 0 && own) {
        a = D_01FAB148[side].unk40;
        b = D_01FAB148[side].unk42;
    } else {
        a = 0;
        b = 0;
    }
    func_003BA888(i, a, b);
}
void func_003BAE40(void) {
    s32 i;

    if (gMode != 9 && func_003B79E0() == 0) {
        if (D_005F6E16 == 0) {
            func_003BAC38(&D_01FAB148[0]);
            func_003BAC38(&D_01FAB148[1]);
            D_005F6E16 = 1;
        }
        func_003BAC88((u16 *)D_01FAB1D0[0], &D_01FAB148[0]);
        func_003BAC88((u16 *)D_01FAB1D0[1], &D_01FAB148[1]);
        for (i = 0; i < 4; i++) {
            func_003BAD98(i);
        }
    }
}
void func_003BAEF0(void) {
    u32 flags[3];
    s32 i;
    s32 other;
    s32 to;
    Fighter *f;
    Fighter *g;

    memset(flags, 0, 12);
    if (D_005F5EF8 != 0) {
        return;
    }
    for (i = 0; i < 2; i++) {
        other = i ^ 1;
        f = &gFighters[i];
        to = -1;
        if (f->unk130 != 0) {
            func_003BAA20(i, 0x10);
            flags[other] |= 1;
        }
        if (f->unk131 != 0) {
            to = other;
            if (f->unk146 < 0x15) {
                func_003BAA20(i, 0x11);
                flags[to] |= 2;
            } else {
                func_003BAA20(i, 0x12);
                flags[to] |= 4;
            }
            g = &gFighters[i];
            if (g->unk13B != 0) {
                func_003BAA20(i, 0x14);
                flags[to] |= 8;
            }
            if (g->unk13A != 0) {
                func_003BAA20(i, 0x15);
                flags[to] |= 0x10;
            }
        }
        f = &gFighters[i];
        if (f->unk142 != 0) {
            func_003BAA20(i, 0x13);
            to = other;
            flags[to] |= 0x20;
        }
        if (f->unk137 != 0) {
            func_003BAA20(i, 0x18);
        }
        if (f->unk138 != 0) {
            func_003BAA20(i, 0x16);
            flags[to] |= 0x80;
        }
        if (f->unkC0->unk24 & 0x200) {
            if (f->unk134 != 0) {
                func_003BAA20(i, 0xE);
                flags[((s16)f->unk20 + 1) & 1] |= 0x40;
            }
        }
    }
    for (i = 0; i < 2; i++) {
        if (flags[i] & 0x40) {
            func_003BAA20(i, 0x1A);
        }
        if (flags[i] & 1) {
            func_003BAA20(i, 4);
        }
        if (flags[i] & 2) {
            func_003BAA20(i, 5);
        }
        if (flags[i] & 4) {
            func_003BAA20(i, 6);
        }
        if (flags[i] & 8) {
            func_003BAA20(i, 8);
        }
        if (flags[i] & 0x10) {
            func_003BAA20(i, 9);
        }
        if (flags[i] & 0x20) {
            func_003BAA20(i, 7);
        }
        if (flags[i] & 0x80) {
            func_003BAA20(i, 0xB);
        }
    }
}
void func_003BB1F8(s32 i) {
    if (D_005F5EF8 == 0) {
        func_003BAA20(2, i + 1);
    }
}
void func_003BB218(s32 i, s32 port) {
    func_003BAA20(port, i + 1);
}
void func_003BB228(u8 side, u8 kind) {
    s32 other;

    if (D_005F5EF8 != 0) {
        return;
    }
    other = side ^ 1;
    switch (kind) {
    case 0:
        func_003BAA20(side, 0x17);
        func_003BAA20(other, 0xA);
        break;
    case 1:
        func_003BAA20(side, 0xF);
        func_003BAA20(other, 0x1B);
        break;
    case 2:
        func_003BAA20(side, 0x19);
        break;
    case 3:
        func_003BAA20(side, 0xC);
        break;
    case 4:
        func_003BAA20(side, 0xD);
        break;
    case 5:
        func_003BAA20(side, 0x5);
        break;
    }
}
s32 func_003BB308(void) {
    return D_005F6E32;
}
