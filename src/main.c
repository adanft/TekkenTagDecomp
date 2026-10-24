/* 0x357358-0x358A38: boot, main loop, frame phases, mode dispatch and the per-player input state.
 * docs/game_overview.md sections 1-3. */
#include "common.h"
#include "fighter.h"
#include "trig.h"


extern Fighter gFighters[4];
/* The slots 1-3 of gFighters under their own names (Sys_Init addresses them directly). */
extern Fighter D_00884EE0;
extern Fighter D_00890780;
extern Fighter D_0089C020;
extern char *D_00415700[];
extern s32 D_005F5C10[];
extern u16 gFrameCount;
extern s16 gMode;
extern s16 gSubMode;
extern s32 D_005F5C00;
extern s32 D_005F5C04;
extern s32 gFrameT0;
extern s32 gFrameOverrun;
extern u16 D_005F5CB6;
extern s32 D_005F5CD0;
extern s16 D_005F5CBC;
extern u16 D_005F5CB0[2];
extern s16 D_005F5CB2;
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern s32 gDrawFighterCount;
extern s32 gDrawStagePass;
extern s32 gDrawModelPass;
extern s32 gDrawEffectPass;
extern u32 D_005F6B10;
extern u8 D_0043B7F8[];
extern u8 D_00433400[];
extern u8 D_00433440[];
extern u8 D_004334C0[];
extern s16 gSinTable[];
extern s32 D_005F5C40[2];
extern s32 D_005F5C48[2];
extern u32 D_00415718[];
extern u32 D_00415728[];
extern u32 D_00415738[];
extern u32 D_00415748[];
extern u32 D_00415758[];
extern u32 D_00415768[];
extern s8 gPlayerAnalog[];
extern u32 D_005F5CD8;
extern u32 D_005F5CDC;
extern u32 D_005F5CE0;
extern u32 D_005F5CE4;
extern u32 D_005F5CE8;
extern u32 D_005F5CEC;
extern u32 D_00439B40[][18];
extern u8 D_00439CA0[][10];
extern u8 D_00439C60[];
extern u32 D_0051DEF0[];
extern u32 D_0051DF28[];
extern u8 D_0051DF48[];
extern u32 D_005F7180[][18];

void FlushCache(s32 op);
s32 sceSifInitRpc(s32 mode);
s32 sceCdInit(s32 mode);
s32 sceCdMmode(s32 media);
s32 sceSifRebootIop(const char *img);
s32 sceSifSyncIop(void);
s32 sceSifLoadFileReset(void);
s32 sceSifLoadModule(const char *path, s32 argSize, s32 *args);
void sceDmaReset(s32 mode);
void sceVpu0Reset(void);
void sceGsResetPath(void);
s32 sceGsSyncPath(s32 mode, u16 timeout);
void func_0035D458(void);
void func_0035E400(void);
void Gfx_Init(void);
void sceSdRemoteInit(void);
void Mc_Init(void);
void func_00361B18(void);
void Snd_InitDriver(void);
void File_Init(void);
void func_0039C7B8(void);
void func_003B9058(void);
void func_003B8A50(void);
void func_00357600(void);
void func_003585E0(void);
void func_003BAA90(void);
void Bgm_Stop(void);
void Unlock_ApplyAll(void);
void func_0039C110(void);
void func_0039BBF0(s32 w, s32 h);
void func_00398540(s32 x);
void Rng_Init(void);
void func_00393898(void);
void func_003941F0(void);
void func_003673F8(void);
void func_00367930(void);
void func_003455D0(void);
void func_00346430(void);
void func_003A6BD0(void);
void func_003A7190(void);
void func_003C12D0(s32 a, s32 b);
void func_003B7A58(void);
void func_003A9688(s32 a);
void func_0037CFA8(void);
void func_003DE930(void);
void func_0035D078(void);
void Mode_Change(s32 mode);
void func_003B1478(void);
void func_0035CE38(void);
void func_003C8A38(void);
void Options_Apply(void);
void func_003B7AB8(void);
void Bowl_InitHighScores(void);
void Spr_Setup(s32 a, s32 b);
u16 *func_00342FB0(void);
void *func_003B8798(s32 a);
void func_003430D8(void *dst, void *ref);
void Dma_Wait(u32 channel);
void func_00343178(void *addr, s32 qwc);
s32 func_003ADFC0(void);
s32 func_003AE670(void);
s32 func_003ABC80(void);
void Mode_SetDrawFlags(u32 mode);
void Frame_Begin(void);
void func_00357BD0(void);
void func_00357A50(void);
void func_00357A78(void);
void func_00357B28(void);
void func_00357B50(void);
void Frame_PreUpdate(void);
void Mode_Dispatch(void);
void func_00346608(void);
s32 func_00356A20(u16 stage);
s32 func_00356A48(u16 stage);
void Frame_End(void);
void Sys_Init(void);
void func_0035DA70(void);
void func_0035E370(void);
void Pad_Update(void);
void Input_Update(void);
void func_003BAE40(void);
void Rng_Reseed(void);
void func_003ADFA8(void);
void Mode_Boot(void);
void Mode_Transition(void);
void Mode_Title(void);
void Mode_MainMenu(void);
void Mode_Battle(void);
void Mode_CharSelect(void);
void func_003C8A50(void);
void Mode_NameEntry(void);
void Mode_Ending(void);
void Mode_Options(void);
void Mode_Results(void);
void Mode_Loading(void);
void Mode_EnterTheater(void);
void Mode_Theater(void);
void Mode_EnterMusicPlayer(void);
void Mode_MusicPlayer(void);
void Mode_Gallery(void);
void func_00357C60(void);
void func_00357C88(void);
void func_00357D18(void);
void func_00357D20(void);
void func_00357D28(void);
void func_00357D30(void);
void func_00357D38(void);
void func_00357DB0(void);
void func_00357DB8(void);
void func_00357DD8(void);
void func_003567A8(void);
s32 Tag_StepCounters(s32 i, s32 a);
void func_00357640(s32 i);
void Fx_DrawFighterTex(s32 i);
void func_00343340(Fighter *f, s32 a);
void Vu0_LoadMicrocode(void);
void Gfx_SetPhase(u8 phase);
void func_0035D790(s32 a);
void Gfx_SetFrameReady(void);
void Gfx_WaitVSync(void);
void func_00398D20(void);
void Snd_UpdateVoices(void);
s32 File_Update(void);
void Snd_Flush(void);
void Mc_Update(void);
void func_0035DF38(void);
void Movie_Update(s32 a);
void func_0039C2C0(void);
void func_0035DB90(void);
void func_003DE6C0(u16 stage);
void Model_FlushDrawLists(s32 a);
void func_00340E28(s32 a);
void func_003DB758(void);
void func_003621F8(s32 a);
void Fx_DrawStageLayers(void);
void func_00341F68(void *a, void *b);
void func_003DB7B0(void);
void func_0035DC58(void);
void func_0035DDE0(void);
void func_003E7270(void *a);
void func_0035DE68(void);
void func_00399918(void);
void func_00346830(void);
void func_00350C40(Fighter *f);
void func_003469D8(void);
void Fx_DrawQuads(void);
void func_00399AF8(void);
void func_003A6C08(void);
s16 func_003660B8(s16 x);
void Input_BuildMapping(s32 player);
u32 Pad_AddStickBits(s32 player, u32 buttons);
u16 Pad_GetPlayerInput(s32 player);
void Input_MapButtons(s32 player);
void Input_MergePlayers(void);
s32 func_003AFFB8(s32 i, s32 a);
void Input_SetSide(s32 flag, s32 player, s32 slot);
void func_003B04F0(s32 *a, s32 *b);
void func_0035A4F0(s32 a);

INCLUDE_RODATA("asm/nonmatchings/main", D_004FB880);

INCLUDE_RODATA("asm/nonmatchings/main", D_004FB8A0);

INCLUDE_RODATA("asm/nonmatchings/main", D_004FB8B8);

INCLUDE_RODATA("asm/nonmatchings/main", D_004FB8D0);

INCLUDE_RODATA("asm/nonmatchings/main", D_004FB8E8);

INCLUDE_RODATA("asm/nonmatchings/main", D_004FB908);

/* Reboots the IOP with IOPRP165.IMG and loads the six IOP modules (each retried until it loads). */
void Sys_BootIop(void) {
    s32 i;

    sceSifInitRpc(0);
    sceCdInit(0);
    sceCdMmode(1);
    while (sceSifRebootIop("cdrom:\\IRX\\IOPRP165.IMG;1") == 0) {
    }
    while (sceSifSyncIop() == 0) {
    }
    sceSifInitRpc(0);
    sceCdInit(0);
    sceCdMmode(1);
    sceSifLoadFileReset();
    i = 0;
    do {
        while (sceSifLoadModule(D_00415700[i], 0, D_005F5C10) < 0) {
        }
        i++;
    } while ((u32)i < 6);
}

/* Boot: IOP, DMA, VU0, GS path, graphics, sound, memory card, files, then the game's own initialisers; queues
 * mode 2 behind mode 0. */
void Sys_Init(void) {
    Sys_BootIop();
    sceDmaReset(1);
    sceVpu0Reset();
    sceGsResetPath();
    *(volatile u32 *)0x10003010 = 4;
    func_0035D458();
    func_0035E400();
    Gfx_Init();
    sceSdRemoteInit();
    Mc_Init();
    func_00361B18();
    Snd_InitDriver();
    File_Init();
    func_0039C7B8();
    gFighters[0].unk1E = 0;
    gFighters[0].unk20 = 0;
    D_00884EE0.unk1E = 1;
    D_00884EE0.unk20 = 1;
    D_00890780.unk1E = 0;
    D_00890780.unk20 = 2;
    D_0089C020.unk1E = 1;
    D_0089C020.unk20 = 3;
    func_003B9058();
    func_003B8A50();
    func_00357600();
    func_003585E0();
    func_003BAA90();
    Bgm_Stop();
    Unlock_ApplyAll();
    func_0039C110();
    func_0039BBF0(0x100, 0xF0);
    D_005F5CD0 = 0x352;
    func_00398540(0x352);
    D_005F5CBC = 1;
    D_005F5CB2 = 0;
    D_005F5CB0[0] = 0;
    Rng_Init();
    func_00393898();
    func_003941F0();
    func_003673F8();
    func_00367930();
    D_005F5EE0 = 0;
    D_005F5EE4 = 0;
    func_003455D0();
    func_00346430();
    func_003A6BD0();
    func_003A7190();
    func_003C12D0(1, -1);
    func_003B7A58();
    func_003A9688(0);
    func_0037CFA8();
    func_003DE930();
    func_0035D078();
    Mode_Change(2);
    gMode = 0;
    gSubMode = 0;
    gFrameCount = 0;
    FlushCache(0);
}

void func_00357600(void) {
    func_003B1478();
    func_0035CE38();
    func_003C8A38();
    Options_Apply();
    func_003B7AB8();
    Bowl_InitHighScores();
}

/* Sends the DMA packet of fighter `i` through the scratchpad. */
void func_00357640(s32 i) {
    u16 *pkt;
    Fighter *f;

    Spr_Setup(2, 0);
    pkt = func_00342FB0();
    f = &gFighters[i];
    func_003430D8(pkt, func_003B8798(f->unk22));
    Dma_Wait(0x1000D400);
    func_00343178(pkt + 8, pkt[0]);
}

/* Draw-pass control for the mode: fighters to draw (gDrawFighterCount) and three pass flags. */
void Mode_SetDrawFlags(u32 mode) {
    gDrawFighterCount = 0;
    gDrawStagePass = 0;
    gDrawModelPass = 0;
    gDrawEffectPass = 0;
    if (func_003ADFC0() == 0) {
        return;
    }
    switch (mode) {
    case 4:
    case 8:
    case 11:
        gDrawFighterCount = 2;
        break;
    case 5:
        gDrawFighterCount = 4;
        break;
    }
    switch (mode) {
    case 5:
        if (func_003AE670() == 0) {
            break;
        }
    case 4:
    case 6:
    case 7:
    case 8:
    case 11:
        if (func_003ABC80() == 0) {
            gDrawStagePass = 1;
        }
        break;
    case 10:
        if (D_0043B7F8[0] == 8) {
            gDrawModelPass = 1;
        }
        break;
    case 9:
        break;
    }
    switch (mode) {
    case 4:
    case 6:
    case 7:
    case 8:
    case 11:
        gDrawEffectPass = 1;
        break;
    }
}

/* Boot, then one iteration per VBlank: draw passes for the previous update, then the update. */
int main(void) {
    Sys_Init();
    for (;;) {
        Frame_Begin();
        func_00357BD0();
        func_00357A50();
        func_00357A78();
        func_00357B28();
        func_00357B50();
        Frame_PreUpdate();
        Mode_Dispatch();
        if (gDrawEffectPass) {
            func_00346608();
        }
        D_005F5C00 = func_00356A20(D_005F5CB6);
        D_005F5C04 = func_00356A48(D_005F5CB6);
        FlushCache(0);
        Frame_End();
    }
}

/* Frame counter, wait until 3.2 ms after VBlank start (Timer 0 >= 0x724), then input and the RNG reseed. */
void Frame_PreUpdate(void) {
    func_0035DA70();
    func_0035E370();
    gFrameCount++;
    while (*(volatile u32 *)0x10000000 < 0x724) {
        EE_HW_POLL();
    }
    Pad_Update();
    Input_Update();
    func_003BAE40();
    Rng_Reseed();
}

/* One update of the current mode. */
void Mode_Dispatch(void) {
    func_003ADFA8();
    switch (gMode) {
    case 0:
        Mode_Boot();
        break;
    case 1:
        Mode_Transition();
        break;
    case 2:
        Mode_Title();
        break;
    case 3:
#ifdef PORT
        PORT_DIFF(0x3BD2E0, Mode_MainMenu, "i", 0); /* TTT_DIFF: the mode against the original code */
#else
        Mode_MainMenu();
#endif
        break;
    case 4:
#ifdef PORT
        PORT_DIFF(0x35A6A0, Mode_Battle, "i", 0); /* TTT_DIFF: the mode against the original code */
#else
        Mode_Battle();
#endif
        break;
    case 5:
#ifdef PORT
        PORT_DIFF(0x3CB900, Mode_CharSelect, "i", 0); /* TTT_DIFF: the mode against the original code */
#else
        Mode_CharSelect();
#endif
        break;
    case 6:
        func_003C8A50();
        break;
    case 7:
        Mode_NameEntry();
        break;
    case 8:
        Mode_Ending();
        break;
    case 9:
#ifdef PORT
        PORT_DIFF(0x3B4B20, Mode_Options, "i", 0); /* TTT_DIFF: the mode against the original code */
#else
        Mode_Options();
#endif
        break;
    case 10:
        Mode_Results();
        break;
    case 11:
        Mode_Loading();
        break;
    case 12:
        Mode_EnterTheater();
        break;
    case 13:
        Mode_Theater();
        break;
    case 14:
        Mode_EnterMusicPlayer();
        break;
    case 15:
        Mode_MusicPlayer();
        break;
    case 16:
        Mode_Gallery();
        break;
    }
    Mode_SetDrawFlags(gMode);
}

void func_00357A50(void) {
    func_00357C60();
    func_00357C88();
    func_00357D18();
}

/* Draw pass for the fighters. */
void func_00357A78(void) {
    s32 i;

    func_00357D20();
    if (gDrawFighterCount != 0) {
        func_003567A8();
        for (i = 0; i < gDrawFighterCount; i++) {
            if (gFighters[i].unk127 != 0 && Tag_StepCounters(i, 0) == 0) {
                func_00357640(i);
                Fx_DrawFighterTex(i);
                func_00343340(&gFighters[i], 0);
            }
        }
    }
    func_00357D28();
}

void func_00357B28(void) {
    func_00357D30();
    func_00357D38();
    func_00357DB0();
}

void func_00357B50(void) {
    func_00357DB8();
    func_00357DD8();
}

/* Loads the mode's VU0 microcode, hands the frame to the VBlank handler and waits for it. */
void Frame_Begin(void) {
    u32 status;

    Vu0_LoadMicrocode();
    do {
        EE_DI();
        EE_SYNC_P();
        EE_MFC0_STATUS(status);
    } while (status & 0x10000);
    Gfx_SetPhase(0);
    func_0035D790(D_005F5C00);
    D_005F5C00 = 0;
    Gfx_SetFrameReady();
    EE_EI();
    Gfx_WaitVSync();
}

void func_00357BD0(void) {
    Gfx_SetPhase(1);
    func_00398D20();
}

/* Services (sound, files, memory card, movie) and the frame-time sample. */
void Frame_End(void) {
    s32 t;

    Snd_UpdateVoices();
    File_Update();
    Snd_Flush();
    Mc_Update();
    func_0035DF38();
    sceGsSyncPath(0, 0);
    t = *(volatile s32 *)0x10000000;
    gFrameT0 = t;
    gFrameOverrun = t >= 0x246E;
    Gfx_SetPhase(2);
    Movie_Update(0);
    func_0039C2C0();
}

void func_00357C60(void) {
    func_0035DB90();
    Dma_Wait(0x10008000);
}

/* Stage draw pass. */
void func_00357C88(void) {
    if (gDrawStagePass) {
        func_003DE6C0(D_005F5CB6);
        Model_FlushDrawLists(0);
        func_00340E28(1);
        func_003DB758();
        func_003621F8(0);
        Fx_DrawStageLayers();
        func_00341F68(D_00433440, D_00433400);
        func_003DB7B0();
        if (D_005F5C04 == 0) {
            func_00340E28(0);
            Model_FlushDrawLists(1);
        }
    }
}

void func_00357D18(void) {
    func_0035DA70();
}

void func_00357D20(void) {
    func_0035DC58();
}

void func_00357D28(void) {
    func_0035DA70();
}

void func_00357D30(void) {
    func_0035DDE0();
}

void func_00357D38(void) {
    if (gDrawStagePass) {
        if (D_005F5C04) {
            func_00340E28(0);
            Model_FlushDrawLists(1);
        }
        if (D_005F5CB6 == 2 || D_005F5CB6 == 0x13 || D_005F5CB6 == 0x16) {
            func_003E7270(D_004334C0);
        }
    }
    if (gDrawModelPass) {
        Model_FlushDrawLists(1);
    }
}

void func_00357DB0(void) {
    func_0035DA70();
}

void func_00357DB8(void) {
    func_0035DE68();
    func_00399918();
}

void func_00357DD8(void) {
    s32 i;

    if (gDrawEffectPass) {
        func_00346830();
    }
    if (gDrawFighterCount) {
        func_003567A8();
        for (i = 0; i < gDrawFighterCount; i++) {
            if (gFighters[i].unk127 != 0 && Tag_StepCounters(i, 0) == 0) {
                func_00357640(i);
                Fx_DrawFighterTex(i);
                func_00343340(&gFighters[i], 1);
            }
        }
        func_003567A8();
        for (i = 0; i < gDrawFighterCount; i++) {
            if (gFighters[i].unk127 != 0 && Tag_StepCounters(i, 0) == 0) {
                func_00350C40(&gFighters[i]);
            }
        }
    }
    if (gDrawEffectPass) {
        func_003469D8();
        Fx_DrawQuads();
    }
    func_00399AF8();
    func_003A6C08();
}

/* Sine of a 16-bit angle from the table gSinTable, clamped to +-0xFFF, in 1.12 fixed point * 8. */
s16 Math_Sin(s32 angle) {
    s32 v = gSinTable[((u32)angle >> 4) & 0xFFF];

    if (v >= 0x1000) {
        v = 0xFFF;
    }
    if (v <= -0x1000) {
        v = -0xFFF;
    }
    return v << 3;
}

/* Cosine: the sine table a quarter turn on. */
s16 Math_Cos(s32 angle) {
    s32 v = gSinTable[(((u32)angle << 16) >> 20) + 0x400];

    if (v >= 0x1000) {
        v = 0xFFF;
    }
    if (v <= -0x1000) {
        v = -0xFFF;
    }
    return v << 3;
}

/* 16-bit angle of the vector (x, y). */
u16 Math_Atan2(s16 x, s16 y) {
    u16 ax = x;
    u16 ay = y;
    u16 a;

    if (x < 0) {
        ax = -ax;
    }
    if (y < 0) {
        ay = -ay;
    }
    if (ax == ay) {
        a = 0x200;
    } else if (ay < ax) {
        a = func_003660B8((u32)(ay << 12) / ax);
    } else {
        a = 0x400 - func_003660B8((u32)(ax << 12) / ay);
    }
    a <<= 4;
    if (x >= 0) {
        if (y >= 0) {
            return a;
        }
        return ~a;
    }
    if (y >= 0) {
        return 0x7FFF - a;
    }
    return a - 0x8000;
}

/* Integer square root (Newton). */
u32 Math_Isqrt(u32 x) {
    u32 a;
    u32 b;

    if (x == 0) {
        return 0;
    }
    a = 1;
    b = x;
    while (a < b) {
        a <<= 1;
        b >>= 1;
    }
    do {
        b = a;
        a = (x / b + b) >> 1;
    } while (a < b);
    return b;
}

/* Input of side `side`: player `player`'s held and pressed buttons, or nothing unless `force`. */
void Input_SetSide(s32 force, s32 side, s32 player) {
    if (D_005F5CB0[side] != 0 || force != 0) {
        D_005F5C40[side] = D_00415748[player];
        D_005F5C48[side] = D_00415758[player];
    } else {
        D_005F5C40[side] = 0;
        D_005F5C48[side] = 0;
    }
}

/* Builds player `player`'s button mapping from its configuration bytes. */
#if 0
/* NON_MATCHING: 38 lines: the loads of the last two configuration bytes are scheduled above the second copy loop here. */
void Input_BuildMapping(s32 player) {
    u8 *src = D_00439CA0[player];
    u32 *dst = D_00439B40[player];
    s32 i;

    for (i = 0; i < 8; i++) {
        *dst++ = D_0051DEF0[*src++];
    }
    for (i = 0; i < 8; i++) {
        *dst++ = D_0051DF28[i];
    }
    D_00439B40[player][10] = 0x400;
    D_00439B40[player][17] = D_0051DEF0[src[1]];
    D_00439B40[player][16] = D_0051DEF0[src[0]];
}
#else
INCLUDE_ASM("asm/nonmatchings/main", Input_BuildMapping);
#endif

/* Default button configuration for player `player`. */
/* The per-player bytes at D_00439C60 + 0x3C of the save block (McSave in menu_common.c) [V offset; I meaning]. */
typedef struct {
    u8 pad[0x3C];
    u8 unk3C[2];
} InputSaveFlags;

void Input_ResetMapping(s32 player) {
    u8 *dst = D_00439CA0[player];
    s32 i;

    for (i = 0; i < 10; i++) {
        *dst++ = D_0051DF48[i];
    }
    ((InputSaveFlags *)D_00439C60)->unk3C[player] = 0;
    Input_BuildMapping(player);
}

/* OR of every player's input words into D_005F5CD8..D_005F5CEC; opposite directions cancel. */
#if 0
/* NON_MATCHING: the globals D_005F5CDC and D_005F5CE0 get swapped registers (t5/t6) inside the loop. */
void Input_MergePlayers(void) {
    s32 i;

    D_005F5CD8 = 0;
    D_005F5CDC = 0;
    D_005F5CE0 = 0;
    D_005F5CE4 = 0;
    D_005F5CE8 = 0;
    D_005F5CEC = 0;
    for (i = 0; i < 4; i++) {
        D_005F5CD8 |= D_00415718[i];
        D_005F5CDC |= D_00415728[i];
        D_005F5CE0 |= D_00415738[i];
        D_005F5CE4 |= D_00415748[i];
        D_005F5CE8 |= D_00415758[i];
        D_005F5CEC |= D_00415768[i];
    }
    if ((D_005F5CD8 & 0x5000) == 0x5000) {
        D_005F5CD8 &= ~0x5000;
        D_005F5CDC &= ~0x5000;
        D_005F5CE0 &= ~0x5000;
    }
    if ((D_005F5CD8 & 0xA000) == 0xA000) {
        D_005F5CD8 &= ~0xA000;
        D_005F5CDC &= ~0xA000;
        D_005F5CE0 &= ~0xA000;
    }
    if ((D_005F5CE4 & 0x5000) == 0x5000) {
        D_005F5CE4 &= ~0x5000;
        D_005F5CE8 &= ~0x5000;
        D_005F5CEC &= ~0x5000;
    }
    if ((D_005F5CE4 & 0xA000) == 0xA000) {
        D_005F5CE4 &= ~0xA000;
        D_005F5CE8 &= ~0xA000;
        D_005F5CEC &= ~0xA000;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/main", Input_MergePlayers);
#endif

/* Maps player `player`'s raw held and repeat bits through its button configuration. */
#if 0
/* NON_MATCHING: first draft; register allocation and scheduling of the two mapping loops differ. */
void Input_MapButtons(s32 player) {
    u32 old = D_00415748[player];
    s32 bits = D_00415718[player];
    u32 out = 0;
    s32 i;

    for (i = 0; i < 18; i++) {
        if (bits & 1) {
            out |= D_00439B40[player][i];
        }
        bits >>= 1;
    }
    if ((out & 0x5000) == 0x5000) {
        out &= ~0x5000;
    }
    if ((out & 0xA000) == 0xA000) {
        out &= ~0xA000;
    }
    D_00415758[player] = (old ^ out) & out;
    D_00415748[player] = out;
    bits = D_00415738[player];
    out = 0;
    for (i = 0; i < 18; i++) {
        if (bits & 1) {
            out |= D_00439B40[player][i];
        }
        bits >>= 1;
    }
    if ((out & 0x5000) == 0x5000) {
        out &= ~0x5000;
    }
    if ((out & 0xA000) == 0xA000) {
        out &= ~0xA000;
    }
    D_00415768[player] = out;
}
#else
INCLUDE_ASM("asm/nonmatchings/main", Input_MapButtons);
#endif

void func_003585E0(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_005F5C40[i & 1] = 0;
        D_005F5C48[i & 1] = 0;
        D_00415718[i] = 0;
        D_00415728[i] = 0;
        D_00415738[i] = 0;
        Input_BuildMapping(i);
    }
}

/* Adds bit 16 / 17 to `buttons` when either axis of the left / right stick of `player` is below 25 in magnitude. */
s32 abs(s32 x);

u32 Pad_AddStickBits(s32 player, u32 buttons) {
    s32 i;
    s32 v;
    s32 q;

    player *= 4;
    q = player + 1;
    for (i = 0; i < 2; i++) {
        v = gPlayerAnalog[i * 2 + player];
        if (abs(v) < 25) {
            goto set;
        }
        v = gPlayerAnalog[i * 2 + q];
        if (abs(v) < 25) {
        set:
            buttons |= 0x10000 << i;
        }
    }
    return buttons;
}

/* Extra buttons of a pad of kind 2 (bytes 5-7 of its data past 0x60). */
u16 Pad_GetExtraButtons(s32 port, u8 *rdata) {
    s32 v = rdata[5] < 0x61 ? 0 : 0x40;

    if (rdata[6] >= 0x61) {
        v |= 0x80;
    }
    if (rdata[7] >= 0x61) {
        v |= 4;
    }
    return v;
}

/* Once per frame: held, pressed and auto-repeat bits of the four players, then the two fight sides. */
#if 0
/* NON_MATCHING: the held/pressed bookkeeping is scheduled differently and the repeat loop registers differ. */
void Input_Update(void) {
    s32 i;
    s32 j;
    s32 cur;
    s32 old;
    s32 pressed;
    s32 rep;
    u32 n;

    for (i = 0; i < 4; i++) {
        cur = Pad_AddStickBits(i, Pad_GetPlayerInput(i));
        if ((cur & 0x5000) == 0x5000) {
            cur &= ~0x5000;
        }
        if ((cur & 0xA000) == 0xA000) {
            cur &= ~0xA000;
        }
        old = D_00415718[i];
        D_00415718[i] = cur;
        pressed = (old ^ cur) & cur;
        D_00415728[i] = pressed;
        rep = 0;
        for (j = 0; j < 18; j++) {
            rep <<= 1;
            if (!(cur & 0x20000)) {
                D_005F7180[i][j] = 0;
            } else if (pressed & 0x20000) {
                rep |= 1;
                D_005F7180[i][j] = 1;
            } else {
                n = ++D_005F7180[i][j];
                if (n >= 20 && (n & 3) == 0) {
                    rep |= 1;
                }
            }
            cur <<= 1;
            pressed <<= 1;
        }
        D_00415738[i] = rep;
        Input_MapButtons(i);
    }
    Input_MergePlayers();
    if (gMode == 4) {
        for (i = 0; i < 2; i++) {
            Input_SetSide(0, i, func_003AFFB8(i, gFighters[i].unk0));
            if (D_0043B7F8[0] == 7 && D_005F5CB0[i] == 0) {
                func_003B04F0(&D_005F5C40[i], &D_005F5C48[i]);
            }
        }
    } else {
        for (i = 0; i < 2; i++) {
            Input_SetSide(1, i, i);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/main", Input_Update);
#endif

/* 1 when any player has just pressed one of the buttons that skip (start; more in mode 13). */
s32 Input_SkipPressed(void) {
    u32 mask = gMode == 13 ? 0x8FC : 0x800;
    u32 all = 0;
    u32 old;
    s32 i;

    for (i = 0; i < 4; i++) {
        all |= Pad_GetPlayerInput(i);
    }
    old = D_005F6B10;
    D_005F6B10 = all;
    return ((old ^ all) & all & mask) != 0;
}

void func_00358A30(void) {
    func_0035A4F0(0x18);
}
