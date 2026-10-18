/* 0x3745E8-0x376C30: sound effects on SPU2 core 1 voices 0-21 through libspu2 commands sent with sceSdRemote, and
 * the fighter voices (voice sets D_00501808, voice sequences D_00501A78). docs/systems/audio.md section 6. Its
 * .rodata (0x500780-0x500D50) is included in place. */
#include "common.h"
#include "sound.h"
#include "fighter.h"

extern u16 D_005F5C50;
extern u16 D_005F5CBC;
extern u16 D_005F5C78;
extern s32 D_005F5F38;
extern s32 D_005F5F3C;
extern s32 D_005F5F40;
extern SndBank gSndBanks[4];
extern SndVoice gSndVoices[SND_VOICE_COUNT];
extern u8 gSndKeyStatus[SND_VOICE_COUNT];
extern SndDelayed gSndDelayed[24];
extern SndBankDef gSndBankDefs[];
/* Tables read from the original executable. */
extern const s8 D_00500780[];
extern const SndDelayStep D_005007D8[3];
extern const u32 D_00500CB0[];
extern const u32 D_00500CB4[];
extern const u32 D_00500CB8[];
/* Fighter voice data, outside this object's .rodata. */
extern SndVoiceSet D_00501808[];

void Snd_Nop(void);
void Snd_InitSpu(void);
void Snd_InitBanks(void);
void func_00375488(s32 player);
s32 sceSdRemote(s32 arg, s32 command, ...);
s32 Snd_AllocVoice(s32 category, s32 player);
void Snd_SetVoiceSlot(s32 category, s32 player, s32 id, s32 voice, s32 volume);
s32 Snd_PlaySeEx(s32 id, s32 player, s32 mode, u32 scale);
void Snd_KeyOn(s32 voice, u16 volume, u16 pitch, u32 addr);
void Snd_StopCategory(s32 category);
void Snd_StopAllSe(void);
s32 func_003755C8(s32 id);
s32 Snd_QueueDelayed(s32 id, s32 player);
void func_00365BB0(s32 id, s32 player);
s32 func_003ABCC0(void);
s32 func_0035A530(void);
const SndDelayStep *Snd_GetDelaySteps(void);
void Snd_SetPlayerVolume(s32 id, u16 left, u16 right);
s32 func_003758F0(u16 left, u16 right);
s32 func_00375930(u16 left, u16 right);
void Snd_PlayVoice(s32 player, s32 set, u16 code);
void Snd_StopVoiceSeq(Fighter *f);
void Snd_StartVoiceSeq(Fighter *f, s32 seq, s32 time);
void func_00376578(Fighter *f, Fighter *other, s16 code);
void func_00376C60(s32 player);
s32 func_003800D8(s32 n);
s32 rand(void);
void func_00376908(Fighter *f, Fighter *other, s32 keep);

extern Fighter gFighters[4];
extern u16 gFrameCount;
extern s32 D_005F5C40[2];
extern s32 D_005F5F18[2];
extern s16 D_005F5F48[2];
extern s16 D_005F5F50[2];
extern const s16 D_00501800[];
extern const SndVoiceStep D_00501A78[];
extern const s16 D_00505C10[];
extern const s16 D_00500CE8[];
extern const s16 D_00500CF8[];
extern const s16 D_00500D00[];
extern const s16 D_00500D08[];
extern const s16 D_00500D10[];
extern const s16 D_00500D18[];
extern const s16 D_00500D20[];
extern const s16 D_00500D28[];
extern const s16 D_00500D30[];
extern const s16 D_00500D38[];
extern const s16 D_00500D40[];
extern const s16 D_00500D48[];

/* Boot: SPU2 set-up and the sound effect banks. */
void Snd_Init(void) {
    Snd_InitSpu();
    Snd_InitBanks();
}

/* Master volume and CD input of both cores, then the default volume, pitch and address of every voice. */
void Snd_InitSpu(void) {
    SpuCommonAttr common;
    SpuVoiceAttr attr;
    s32 core;

    for (core = 0; core < 2; core++) {
        Snd_Nop();
        sceSdRemote(1, 2, core);
        common.mask = 0x3FCF;
        common.mvol.left = 0x3FFF;
        common.mvol.right = 0x3FFF;
        common.mvolmode.left = 0;
        common.mvolmode.right = 0;
        common.cd.volume.left = 0x7FFF;
        common.cd.volume.right = 0x7FFF;
        common.cd.reverb = 0;
        common.cd.mix = 0;
        common.ext.volume.left = 0;
        common.ext.volume.right = 0;
        common.ext.reverb = 0;
        common.ext.mix = 0;
        Snd_Nop();
        sceSdRemote(1, 0x7128, &common);
    }
    for (core = 0; core < 2; core++) {
        Snd_Nop();
        sceSdRemote(1, 2, core);
        attr.mask = 0x9F;
        attr.voice = 0xFFFFFF;
        attr.volume.left = 0x1FFF;
        attr.volume.right = 0x1FFF;
        attr.volmode.left = 0;
        attr.volmode.right = 0;
        attr.pitch = 0x1000;
        attr.addr = 0x5010;
        Snd_Nop();
        sceSdRemote(1, 0x7240, &attr);
    }
}

/* Places the four character banks after the common sound effects in SPU2 RAM. */
void Snd_InitBanks(void) {
    SndBank *bank = gSndBanks;
    s32 i;
    u32 addr = D_00500CB8[0] + 0x5010;

    for (i = 0; i < 4; i++) {
        bank->addr = addr;
        bank->owner = 0;
        addr += 0x1C900;
        bank++;
    }
}

u32 Snd_GetBankAddr(s32 slot) {
    return gSndBanks[slot].addr;
}

INCLUDE_RODATA("asm/nonmatchings/sound", D_00500780);

void Snd_SetBankOwner(s32 slot, s32 chara) {
    gSndBanks[slot].owner = D_00500780[chara];
}

void Snd_ClearVoices(void) {
    SndVoice *v = gSndVoices;
    s32 i;

    for (i = 0; i < SND_VOICE_COUNT; i++) {
        v->category = 0;
        v->id = 0;
        v->player = 0;
        v->volume = 0;
    }
}

INCLUDE_RODATA("asm/nonmatchings/sound", jtbl_005007B0);

/* A voice for a sound effect of `category`: the categories 3-9 first take a voice already playing that category,
 * the others a free voice first. Returns -1 when none is left. */
#if 0
/* NON_MATCHING: the original keeps the four identical case bodies (3, 4, 5, 6-9) apart; the compiler cross-jumps them
 * into one. */
s32 Snd_AllocVoice(s32 category, s32 player) {
    s32 voice = 0;

    player &= 1;
    if (category == 6 || category == 8) {
        category += player;
    }
    switch (category) {
        case 3:
            voice = Snd_FindCategoryVoice(category);
            if (voice < 0) {
                voice = Snd_FindFreeVoice();
                if (voice < 0) {
                    voice = Snd_FindReusableVoice();
                }
            }
            break;
        case 4:
            voice = Snd_FindCategoryVoice(category);
            if (voice < 0) {
                voice = Snd_FindFreeVoice();
                if (voice < 0) {
                    voice = Snd_FindReusableVoice();
                }
            }
            break;
        case 5:
            voice = Snd_FindCategoryVoice(category);
            if (voice < 0) {
                voice = Snd_FindFreeVoice();
                if (voice < 0) {
                    voice = Snd_FindReusableVoice();
                }
            }
            break;
        case 6:
        case 7:
        case 8:
        case 9:
            voice = Snd_FindCategoryVoice(category);
            if (voice < 0) {
                voice = Snd_FindFreeVoice();
                if (voice < 0) {
                    voice = Snd_FindReusableVoice();
                }
            }
            break;
        case 1:
        case 2:
        case 10:
            voice = Snd_FindFreeVoice();
            if (voice < 0) {
                voice = Snd_FindReusableVoice();
                if (voice < 0) {
                    voice = Snd_FindCategoryVoice(category);
                }
            }
            break;
    }
    return voice;
}
#else
INCLUDE_ASM("asm/nonmatchings/sound", Snd_AllocVoice);
#endif

/* First free voice, or -1. */
s32 Snd_FindFreeVoice(void) {
    s32 i = 0;
    s32 found = -1;
    SndVoice *v = gSndVoices;

    for (; i < SND_VOICE_COUNT; i++, v++) {
        if (v->category == 0) {
            found = i;
            break;
        }
    }
    return found;
}

/* First voice that is free or of category 10, or -1. */
s32 Snd_FindReusableVoice(void) {
    s32 i = 0;
    s32 found = -1;
    SndVoice *v = gSndVoices;

    for (; i < SND_VOICE_COUNT; i++, v++) {
        if (v->category == 0 || v->category == 10) {
            found = i;
            break;
        }
    }
    return found;
}

/* First voice of `category`, or -1. */
s32 Snd_FindCategoryVoice(s32 category) {
    s32 i = 0;
    s32 found = -1;
    SndVoice *v = gSndVoices;

    for (; i < SND_VOICE_COUNT; i++, v++) {
        if (v->category == category) {
            found = i;
            break;
        }
    }
    return found;
}

void Snd_SetVoiceSlot(s32 category, s32 player, s32 id, s32 voice, s32 volume) {
    SndVoice *v = &gSndVoices[voice];

    v->category = category;
    v->player = player;
    v->id = id;
    v->volume = volume;
}

/* Every frame: frees the slots of voices that stopped (key state 0 or 3) and keeps the 22 key states. */
void Snd_UpdateVoices(void) {
    char status[64];
    SndVoice *v = gSndVoices;
    s32 i = 0;
    char *p;
    u8 *q;

    Snd_Nop();
    sceSdRemote(1, 2, 1);
    Snd_Nop();
    sceSdRemote(1, 0x6418, status);
    p = status;
    for (; i < SND_VOICE_COUNT; i++) {
        if (*p == 3 || *p == 0) {
            v->category = 0;
        }
        p++;
        v++;
    }
    p = status;
    q = gSndKeyStatus;
    for (i = 0; i < SND_VOICE_COUNT; i++) {
        *q++ = *p++;
    }
}

/* Bank of sound effect `id`, or -1. */
s32 Snd_FindBank(u32 id) {
    SndBankDef *def = gSndBankDefs;
    s32 i;
    s32 last;

    for (i = 0; i < 62; i++) {
        last = def->first + def->count - 1;
        if (id >= def->first && id <= last) {
            return i;
        }
        def++;
    }
    return -1;
}

SndSe *Snd_GetSe(s32 id) {
    s32 se = func_003755C8(id);
    SndBankDef *def;

    if (se == 0) {
        return NULL;
    }
    def = &gSndBankDefs[Snd_FindBank(se)];
    return &def->se[se - def->first];
}

#if 0
/* NON_MATCHING: branch layout: the original tests D_005F5C50 first with id == 0 in the delay slot. */
s32 func_00374C00(s32 id) {
    if (D_005F5C50 == 0 && D_005F5CBC != 0) {
        return 1;
    }
    return id == 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/sound", func_00374C00);
#endif

#if 0
/* NON_MATCHING: the original keeps the result in  and tests 0x1F-0x20 as a range; the compiler uses . */
s32 func_00374C28(s32 id) {
    s32 r = 0;

    if (D_005F5F38 != 0 && (id < 0x1F || id > 0x20) && id != 0x19) {
        r = id != 0x1A;
    }
    return r;
}
#else
INCLUDE_ASM("asm/nonmatchings/sound", func_00374C28);
#endif

s32 func_00374C60(s32 id, s32 player) {
    id &= 0xFFF;
    if (func_00374C00(id) == 0) {
        func_00365BB0(id, player & 0xFFFF);
        if (func_00374C28(id) != 0) {
            return Snd_QueueDelayed(id, player);
        } else {
            return Snd_PlaySeEx(id, player, 0, 0);
        }
    }
}

/* Plays sound effect `id` for `player`. */
void Snd_PlaySe(s32 id, s32 player) {
    id &= 0xFFF;
    if (func_00374C00(id) == 0) {
        Snd_PlaySeEx(id, player, 0, 0);
    }
}

void func_00374D30(s32 id, s32 player) {
    if (func_003ABCC0() == 0 && func_0035A530() != 0) {
        Snd_PlaySe(id, player);
    }
}

s32 func_00374D90(s32 id, s32 player) {
    id &= 0xFFF;
    if (func_00374C00(id) == 0) {
        if (func_00374C28(id) != 0) {
            return Snd_QueueDelayed(id, player);
        } else {
            return Snd_PlaySeEx(id, player, 0, 0);
        }
    }
}

s32 func_00374E00(s32 id, s32 player) {
    SndSe *se;

    id &= 0xFFF;
    if (func_00374C00(id) == 0) {
        se = Snd_GetSe(id);
        if (se != NULL && se->category != 6) {
            if (func_00374C28(id) != 0) {
                return Snd_QueueDelayed(id, player);
            } else {
                return Snd_PlaySeEx(id, player, 0, 0);
            }
        }
    }
}

/* Plays sound effect `id` for `player`: finds its bank and sample, picks a voice of its category and keys it on.
 * `mode` 1 skips the categories 6-7, 2 moves categories other than 8-9 to 10; `scale` (0-0x100) scales the volume. */
#if 0
/* NON_MATCHING: register allocation: the sample address and the category swap $s1 / $s2. */
s32 Snd_PlaySeEx(s32 id, s32 player, s32 mode, u32 scale) {
    s32 se;
    s32 bank;
    SndBankDef *def;
    SndSe *e;
    s32 category;
    u32 addr;
    s32 voice;
    u16 volume;
    s32 i;

    se = func_003755C8(id);
    if (se != 0) {
        bank = Snd_FindBank(se);
        if (bank >= 0) {
            def = &gSndBankDefs[bank];
            e = &def->se[se - def->first];
            category = e->category;
            if (category == 6 || category == 8) {
                category += player;
            }
            if (mode == 1) {
                if (category >= 6 && category <= 7) {
                    goto end;
                }
            } else if (mode == 2) {
                if (category < 8 || category > 9) {
                    category = 10;
                }
            }
            switch (def->kind) {
                default:
                case 0:
                    addr = e->offset + D_00500CB0[0] + 0x5010;
                    break;
                case 1:
                    addr = e->offset + D_00500CB4[0] + 0x5010;
                    break;
                case 2:
                    for (i = 0; i < 4; i++) {
                        if (gSndBanks[i].owner == bank) {
                            break;
                        }
                    }
                    addr = gSndBanks[i].addr + e->offset;
                    break;
            }
            voice = Snd_AllocVoice(category, player);
            addr += 0x40;
if (voice >= 0) {
                volume = e->volume << 8;
                if (volume != 0) {
                    volume--;
                }
                if (scale != 0) {
                    volume = (volume * scale) >> 8;
                }
                volume = (u32)(volume * D_005F5C78) / 10;
                Snd_SetVoiceSlot(category, player, se, voice, volume);
                Snd_KeyOn(voice, volume, e->pitch, addr);
            }
        }
    }
end:;
}
#else
INCLUDE_ASM("asm/nonmatchings/sound", Snd_PlaySeEx);
#endif

/* Starts `voice` (core 1) on the sample at `addr`. */
void Snd_KeyOn(s32 voice, u16 volume, u16 pitch, u32 addr) {
    SpuVoiceAttr attr;

    Snd_Nop();
    sceSdRemote(1, 2, 1);
    attr.voice = 1L << voice;
    attr.mask = 0x93;
    attr.volume.left = volume;
    attr.volume.right = volume;
    attr.pitch = pitch;
    attr.addr = addr;
    Snd_Nop();
    sceSdRemote(1, 0x7440, &attr);
}

void func_00375180(void) {
    D_005F5F38 = 1;
}

void func_00375190(void) {
    D_005F5F38 = 0;
}

s32 func_00375198(void) {
    return D_005F5F38;
}

void func_003751A0(s32 mode) {
    if (mode == 6) {
        D_005F5F38 = 1;
    } else {
        D_005F5F38 = 0;
    }
}

INCLUDE_RODATA("asm/nonmatchings/sound", D_005007D8);

const SndDelayStep *Snd_GetDelaySteps(void) {
    return D_005007D8;
}

void Snd_ClearDelayed(void) {
    SndDelayed *d = gSndDelayed;
    s32 i;

    for (i = 0; i < 24; i++) {
        d->delay = 0;
        d++;
    }
    D_005F5F3C = 0;
}

/* Queues sound effect `id` to be played after a delay. */
s32 Snd_QueueDelayed(s32 id, s32 player) {
    SndDelayed *d = &gSndDelayed[D_005F5F3C];
    const SndDelayStep *step = Snd_GetDelaySteps();

    d->delay = step->delay;
    d->id = id;
    d->player = player;
    D_005F5F3C++;
    D_005F5F3C = D_005F5F3C < 0 ? 23 : D_005F5F3C > 23 ? 0 : D_005F5F3C;
}

void Snd_UpdateDelayed(void) {
    SndDelayed *d = gSndDelayed;
    const SndDelayStep *step;
    s32 i;
    s32 j;
    s16 scale;

    for (i = 0; i < 24; i++, d++) {
        if (d->delay != 0 && d->unk0 == 0) {
            scale = 0;
            step = Snd_GetDelaySteps();
            for (j = 0; j < 3; j++, step++) {
                if (d->delay == step->delay) {
                    scale = step->scale;
                }
            }
            if (scale != 0) {
                Snd_PlaySeEx(d->id, d->player, 2, scale);
            }
            d->delay--;
        }
    }
}

void func_00375368(s32 index, s32 player) {
    D_005F5F40 = player;
    Snd_PlaySe((&D_00501808[index])->unkE, player);
}

void func_00375388(s32 player) {
    if (player == D_005F5F40) {
        Snd_StopCategory(5);
    }
}

void func_003753A8(void) {
    Snd_ClearDelayed();
    Snd_StopAllSe();
    Snd_ClearVoices();
}

void func_003753D0(void) {
    s32 i;

    Snd_ClearDelayed();
    for (i = 0; i < 11; i++) {
        if (i != 1) {
            Snd_StopCategory(i);
        }
    }
}

void func_00375420(void) {
    Snd_StopCategory(5);
}

void func_00375428(void) {
    Snd_StopCategory(8);
    Snd_StopCategory(9);
}

void func_00375448(void) {
    Snd_StopCategory(2);
    Snd_StopCategory(3);
}

void func_00375468(void) {
    func_00375488(0);
    func_00375488(1);
}

void func_00375488(s32 player) {
    if (player == 0) {
        Snd_StopCategory(6);
    } else {
        Snd_StopCategory(7);
    }
}

/* Keys off every voice of `category`. */
void Snd_StopCategory(s32 category) {
    SndVoice *v = gSndVoices;
    s32 mask = 0;
    s32 i;

    if (category != 0) {
        for (i = 0; i < SND_VOICE_COUNT; i++, v++) {
            if (v->category == category) {
                v->category = 0;
                mask |= 1L << i;
            }
        }
        if (mask != 0) {
            Snd_Nop();
            sceSdRemote(1, 2, 1);
            Snd_Nop();
            sceSdRemote(1, 5, 0, mask);
        }
    }
}

/* Keys off voices 0-21. */
void Snd_StopAllSe(void) {
    s32 mask = 0;
    s32 i = 0;

    for (i = 0; i < SND_VOICE_COUNT; i++) {
        mask |= 1L << i;
    }
    Snd_Nop();
    sceSdRemote(1, 2, 1);
    Snd_Nop();
    sceSdRemote(1, 5, 0, mask);
}

INCLUDE_RODATA("asm/nonmatchings/sound", D_005007F8);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500808);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500878);
INCLUDE_RODATA("asm/nonmatchings/sound", jtbl_00500900);

INCLUDE_ASM("asm/nonmatchings/sound", func_003755C8);

INCLUDE_RODATA("asm/nonmatchings/sound", jtbl_00500920);

INCLUDE_ASM("asm/nonmatchings/sound", func_003757B8);

INCLUDE_RODATA("asm/nonmatchings/sound", D_00500CB0);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500CB4);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500CB8);

s32 func_00375868(s32 mode) {
    if (func_003ABCC0() == 0) {
        if (mode == 0) {
            Snd_PlaySe(0x323, 0);
            return func_003758F0(0, 0);
        }
        if (mode == 9 || mode == 11 || mode == 18) {
            Snd_PlaySe(0x324, 0);
            return func_00375930(0, 0);
        }
    }
}

s32 func_003758F0(u16 left, u16 right) {
    Snd_SetPlayerVolume(func_003755C8(0x323), left, right);
}

s32 func_00375930(u16 left, u16 right) {
    Snd_SetPlayerVolume(func_003755C8(0x324), left, right);
}

/* Sets the left and right volume (0-0x1000) of every voice playing sound effect `id`. */
void Snd_SetPlayerVolume(s32 id, u16 left, u16 right) {
    SpuVoiceAttr attr;
    SndVoice *v = gSndVoices;
    s32 i = 0;
    u16 l;
    u16 r;

    if (left <= 0x1000 && right <= 0x1000) {
        for (; i < SND_VOICE_COUNT; i++, v++) {
            if (v->id == id && v->category != 0) {
                l = v->volume * left / 4096;
                r = v->volume * right / 4096;
                Snd_Nop();
                sceSdRemote(1, 2, 1);
                attr.mask = 3;
                attr.voice = 1L << i;
                attr.volume.left = l;
                attr.volume.right = r;
                Snd_Nop();
                sceSdRemote(1, 0x7240, &attr);
            }
        }
    }
}

/* Plays voice `code` of voice set `set` for `player`: kind (2-5) in bits 12-15, index in bits 0-11. */
void Snd_PlayVoice(s32 player, s32 set, u16 code) {
    s16 *seq;
    u16 kind;
    s32 i;
    s32 skip;
    s32 index;

    player &= 1;
    if (gFighters[player].unkB6 == 0) {
        index = code & 0xFFF;
        seq = D_00501808[set].seq;
        kind = (code >> 12) - 2;
        if (kind < 4 && index <= seq[kind] - 1) {
            skip = 0;
            for (i = 0; i < kind; i++) {
                skip += seq[i];
            }
            func_00374C60(seq[index + skip + 4], player);
        }
    }
}

INCLUDE_RODATA("asm/nonmatchings/sound", jtbl_00500CC0);

INCLUDE_ASM("asm/nonmatchings/sound", func_00375B60);

void func_003760E0(void) {
    Fighter *f = gFighters;
    s32 i;

    for (i = 0; i < 4; i++) {
        Snd_StopVoiceSeq(f);
        f++;
    }
}

void Snd_StopVoiceSeq(Fighter *f) {
    f->unkDC = -1;
    f->unkDE = 0;
    f->unk166 = 0;
    f->unk168 = 0;
}

/* Starts the voice sequences (kind 8) the current move of `f` lists, at most three. */
void Snd_StartMoveVoices(Fighter *f) {
    u16 *p = f->unkC0->unk1C;
    s32 i = 0;
    s32 seq;
    s32 mask = 0xFFF;

    if (p != NULL && *p != 0xFFFF) {
        do {
            seq = *p & mask;
            if (*p >> 12 == 8 && seq != 0 && f->unkDE != seq) {
                Snd_StartVoiceSeq(f, seq, f->unkC4 - 1);
            }
            i++;
        } while (i < 3 && *p != 0xFFFF);
    }
}

void Snd_StartVoiceSeq(Fighter *f, s32 seq, s32 time) {
    f->unkDE = seq;
    f->unkDC = seq;
    f->unk166 = 0;
    f->unk168 = time;
}

/* Runs the voice sequence of `f` (D_00501A78 steps from D_00505C10[f->unkDC]) up to the current frame. */
#if 0
/* NON_MATCHING: loop layout: the original tests the step time twice (time < frame, then frame != time) in the body and
 * keeps the step index in a register. */
void func_00376220(Fighter *f, Fighter *other) {
    const SndVoiceStep *seq;
    const SndVoiceStep *step;

    f->unk168++;
    seq = &D_00501A78[D_00505C10[f->unkDC]];
    while ((step = &seq[(s16)f->unk166])->time != 0) {
        if (f->unk168 < step->time) {
            return;
        }
        if (f->unk168 == step->time) {
            switch (step->op) {
                case 0:
                    func_00374C60(step->arg, 0);
                    break;
                case 1:
                    if ((step->arg >> 12) >= 2 && (step->arg >> 12) <= 5) {
                        Snd_PlayVoice(f->unk1E, f->unk2C, step->arg);
                    } else if ((step->arg & 0xFFF) == 0x13) {
                        func_00376C60(f->unk1E);
                    } else {
                        func_00374C60(step->arg, f->unk1E);
                    }
                    break;
                case 2:
                    if ((step->arg >> 12) >= 2 && (step->arg >> 12) <= 5) {
                        Snd_PlayVoice(other->unk1E, other->unk2C, step->arg);
                    } else if ((step->arg & 0xFFF) == 0x13) {
                        func_00376C60(other->unk1E);
                    } else {
                        func_00374C60(step->arg, other->unk1E);
                    }
                    break;
                case 3:
                    func_00376578(f, other, step->arg & 0xFFF);
                    break;
            }
        }
        f->unk166++;
    }
    Snd_StopVoiceSeq(f);
}
#else
INCLUDE_ASM("asm/nonmatchings/sound", func_00376220);
#endif

s32 func_00376410(Fighter *f, Fighter *other) {
    s16 *t;
    s16 n;
    s32 base;
    u16 code;
    s32 r = 0;

    if (gFrameCount & 3) {
        t = D_00501808[f->unk2C].unk8;
        if (other->unkCA < 20) {
            n = t[0];
            base = 2;
        } else {
            n = t[1];
            base = t[0] + 2;
        }
        if (n != 0) {
            code = t[base + (((rand() & 0xFF) * n) >> 8)];
            Snd_PlayVoice(f->unk1E, f->unk2C, code);
            r = 1;
        }
    }
    return r;
}

s32 func_003764D8(Fighter *f) {
    s16 *t;
    u16 code;
    s32 r = 0;

    if (rand() & 1) {
        t = D_00501808[f->unk2C].unk4;
        code = t[((t[0] * (rand() & 0xFF)) >> 8) + 1];
        Snd_PlayVoice(f->unk1E, f->unk2C, code);
        r = D_00501808[f->unk2C].unkC;
    }
    return r;
}

void func_00376578(Fighter *f, Fighter *other, s16 code) {
    switch (code) {
        case 0:
            D_005F5F50[f->unk1E] = 1;
            break;
        case 1:
            if (other->unkDC != -1) {
                Snd_PlayVoice(f->unk1E, f->unk2C, 0x2000);
                Snd_StopVoiceSeq(other);
            }
            break;
        case 2:
            D_005F5F48[f->unk1E] = 1;
            break;
        case 3:
            func_00374C60(D_00501800[((rand() & 0xFF) * 3) >> 8], other->unk1E);
            break;
    }
}

INCLUDE_RODATA("asm/nonmatchings/sound", D_00500CE8);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500CF8);

void func_00376680(Fighter *f) {
    s16 id;

    if ((f->unk2A == 5 || f->unk2A == 7 || f->unk2A == 10 || f->unk2A == 18) && f->unkC4 < 9) {
        id = D_00500CE8[((u32)rand() >> 6) & 3];
    } else {
        id = D_00500CF8[(((u32)rand() & 0xFF) * 3) >> 8];
    }
    func_00374C60(id, f->unk1E);
}

void func_00376728(Fighter *f, Fighter *other) {
    if (f->unkCA < 7) {
        Snd_PlayVoice(other->unk1E, other->unk2C, 0x5001);
    } else if (other->unkCA < 16) {
        Snd_PlayVoice(other->unk1E, other->unk2C, 0x5001);
    } else if (gFrameCount & 3) {
        Snd_PlayVoice(other->unk1E, other->unk2C, 0x5003);
    } else {
        Snd_PlayVoice(other->unk1E, other->unk2C, 0x5004);
    }
}

INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D00);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D08);

void func_00376790(Fighter *f, Fighter *other) {
    u16 code = 0;
    s32 n = func_003800D8(4);

    if (other->unk2C == 16) {
        code = D_00500D00[n];
    } else if (other->unk2C == 17) {
        code = D_00500D08[n];
    }
    Snd_PlayVoice(other->unk1E, other->unk2C, code);
}

void func_00376818(Fighter *f, Fighter *other) {
    if (other->unk2C == 16) {
        if (func_003800D8(2) != 0) {
            func_00374C60(0x20D, other->unk1E);
        } else {
            func_00374C60(0x20E, other->unk1E);
        }
    } else if (other->unk2C == 17) {
        if (func_003800D8(2) != 0) {
            func_00374C60(0x214, other->unk1E);
        } else {
            func_00374C60(0x215, other->unk1E);
        }
    }
}

void func_003768C0(Fighter *f, Fighter *other, s32 keep) {
    func_00376908(f, other, keep);
    func_00376908(other, f, keep);
}

void func_00376908(Fighter *f, Fighter *other, s32 keep) {
    s32 side = f->unk1E;
    s32 buttons = D_005F5C40[side];

    if (buttons & 0x4000) {
        if (f->unk418 == 0) {
            D_005F5F18[side] = 1;
        }
        if (other->unk418 != 0) {
            D_005F5F18[(side + 1) & 1] = 1;
        }
    } else if (buttons & 0x1000) {
        if (f->unk418 == 0) {
            D_005F5F18[side] = 0;
        }
        if (other->unk418 != 0) {
            D_005F5F18[(side + 1) & 1] = 0;
        }
    } else if (keep == 0) {
        D_005F5F18[side] = 0;
    }
}

INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D10);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D18);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D20);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D28);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D30);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D38);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D40);
INCLUDE_RODATA("asm/nonmatchings/sound", D_00500D48);

/* A random voice for fighter `f` from the tables at D_00500D10, by its state and the value at +0xCA. The tables
 * of one entry still call rand(). */
void func_003769D0(Fighter *f) {
    if (f->unkC0->unk28 >= 12) {
        if (f->unkCA < 13) {
            func_00374C60(0x7009, f->unk1E);
        } else if (f->unkCA < 23) {
            func_00374C60(0x700C, f->unk1E);
        } else if (f->unkCA < 33) {
            func_00374C60(0x700B, f->unk1E);
        } else {
            func_00374C60(0x700D, f->unk1E);
        }
    } else if (f->unk2A == 5 || f->unk2A == 7 || f->unk2A == 10 || f->unk2A == 18) {
        if (f->unkCA < 13) {
            func_00374C60(D_00500D10[((rand() & 0xFF) * 2) >> 8], f->unk1E);
        } else if (f->unkCA < 23) {
            func_00374C60(D_00500D18[((rand() & 0xFF) * 1) >> 8], f->unk1E);
        } else if (f->unkCA < 33) {
            func_00374C60(D_00500D20[rand() % 1], f->unk1E);
        } else {
            func_00374C60(D_00500D28[((rand() & 0xFF) * 1) >> 8], f->unk1E);
        }
    } else {
        if (f->unkCA < 13) {
            func_00374C60(D_00500D30[((rand() & 0xFF) * 2) >> 8], f->unk1E);
        } else if (f->unkCA < 23) {
            func_00374C60(D_00500D38[((rand() & 0xFF) * 1) >> 8], f->unk1E);
        } else if (f->unkCA < 33) {
            func_00374C60(D_00500D40[((rand() & 0xFF) * 1) >> 8], f->unk1E);
        } else {
            func_00374C60(D_00500D48[((rand() & 0xFF) * 1) >> 8], f->unk1E);
        }
    }
}
