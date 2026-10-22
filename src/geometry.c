/* 0x346398-0x347760 (module geometry, docs/game_overview.md section 4): the object pool of the effects (0x200 objects
 * of 0x160 bytes from func_003639D0: free and active lists, per-frame update and free, func_00346608 from the main
 * loop), the draw chains of the sprite objects by animation bank and frame and their two draw passes, a few animated
 * sprite callbacks and the effect kind table accessor. .lit4 0x5F51A0 (533.0f of func_003473F0). */
#include "common.h"

typedef f32 Vu0Matrix[4][4];

/* An object of the pool (0x200 records of 0x160 bytes from func_003639D0): list links, update callback, the chain of
 * its draw group (unkC), its ring slot, flags. The same memory as Effect (include/effect.h); 8-aligned here because
 * func_00346608 tests two flag bits with one `ld`. */
/* The drawing state at +0x20 (EffectSub in include/effect.h). */
typedef struct {
    /* 0x00 */ f32 unk0;
    /* 0x04 */ f32 unk4;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ void *unk10;       /* current animation frame record (ObjFrame) */
    /* 0x14 */ u16 unk14;         /* frame count */
    /* 0x16 */ u16 unk16;         /* frame */
    /* 0x18 */ u16 unk18;
    /* 0x1A */ u16 unk1A;         /* animation bank */
    /* 0x1C */ u16 unk1C;
    /* 0x1E */ u16 unk1E;         /* bank frame */
    /* 0x20 */ u16 unk20;
    /* 0x22 */ u16 unk22;         /* parameter record (func_00343F38) */
    /* 0x24 */ u8 pad24[0xC];
    /* 0x30 */ f32 unk30[4];      /* scale */
    /* 0x40 */ s32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ s32 unk4C;
    /* 0x50 */ u8 unk50;
    /* 0x51 */ u8 unk51;
    /* 0x52 */ u8 unk52;
    /* 0x53 */ u8 unk53;
    /* 0x54 */ f32 unk54;
    /* 0x58 */ f32 unk58;
    /* 0x5C */ u8 pad5C[4];
    /* 0x60 */ f32 unk60[4];
    /* 0x70 */ f32 unk70[4];
    /* 0x80 */ f32 unk80[4];
    /* 0x90 */ s32 unk90;
    /* 0x94 */ s16 unk94;
    /* 0x96 */ s16 unk96;
    /* 0x98 */ u8 pad98[0xA8];
} ObjSub; /* size 0x140 */

/* Animation frame record: offset (scaled by unk30), then two floats (unk54, unk58). */
typedef struct {
    /* 0x00 */ f32 unk0[3];
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 unk10;
} ObjFrame;

/* A parameter record of D_00436688 (func_00343F38): scale, one word, two floats. */
typedef struct {
    /* 0x00 */ f32 unk0[3];
    /* 0x0C */ s32 unkC;
    /* 0x10 */ f32 unk10;
    /* 0x14 */ f32 unk14;
} ObjParam;

typedef struct Obj Obj;
struct Obj {
    /* 0x00 */ Obj *next;
    /* 0x04 */ Obj *prev;
    /* 0x08 */ void (*update)(Obj *o);
    /* 0x0C */ Obj *unkC;
    /* 0x10 */ Obj **slot;
    /* 0x14 */ s32 flags;
    /* 0x18 */ s16 unk18;
    /* 0x1A */ s16 unk1A;
    /* 0x1C */ s32 unk1C;
    /* 0x20 */ ObjSub sub;
} __attribute__((aligned(8))); /* size 0x160 */

/* Per animation bank (26): the first and last object of each frame's draw chain and the chain lengths. */
typedef struct {
    /* 0x000 */ Obj *unk0[46];
    /* 0x0B8 */ Obj *unkB8[46];
    /* 0x170 */ s16 unk170[46];
} Unk8BF8D0; /* size 0x1CC */

typedef struct {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
    /* 0x4 */ s16 unk4;
} Unk8BF3A0;

typedef struct {
    /* 0x0 */ void *unk0;
    /* 0x4 */ s32 unk4;
} Unk5193B8;

extern u16 D_005F6CD2;
extern u16 D_005F6CD0;
extern s32 D_005F6CCC;
extern s32 D_005F6CC8;
extern s32 D_005F5F10;
extern s32 D_005F5EE0;
extern s32 D_005F5EE4;
extern Obj D_008BF610;
extern Obj D_008BF770;
extern Unk8BF8D0 D_008BF8D0[26];
extern f32 D_00879C60[];
extern f32 D_00885500[];
extern const Unk5193B8 D_005193B8[];
extern Vu0Matrix D_00433400;
extern Vu0Matrix D_00433440;

Obj *func_003639D0(void);
void func_00346210(void);
void func_00345FC8(void);
void func_00346230(Obj *o);
void func_003A4370(void);
Unk8BF3A0 *func_00343F08(s32 i);
ObjParam *func_00343F38(s32 i);
s32 func_003A9698(void);
void FlushCache(s32 op);
u32 Spr_Setup(u32 count, s32 reserved);
void func_00366EA8(Vu0Matrix m);
u32 func_00342FB0(void);
void func_00344030(s32 i, s32 frame, u32 pk);
u32 Dma_Wait(volatile u32 *chan);
s32 func_003459D0(Obj *o, f32 far);
void sceVu0MulMatrix(Vu0Matrix m0, Vu0Matrix m1, Vu0Matrix m2);
void sceVu0CopyMatrix(Vu0Matrix m0, Vu0Matrix m1);

void func_003463A8(void);
void func_003463B8(void);
void func_00346430(void);
void func_00346580(Obj *o);
f32 func_00346808(void);

/* Freezes the object pool (no allocation, update or draw while set). */
void func_00346398(void) {
    D_005F6CD2 = 1;
}

void func_003463A8(void) {
    D_005F6CD2 = 0;
}

u16 func_003463B0(void) {
    return D_005F6CD2;
}

/* Empties the draw chains of the 26 animation banks. */
void func_003463B8(void) {
    s32 i;
    s32 j;
    Unk8BF8D0 *g;

    for (i = 0; i < 26; i++) {
        g = &D_008BF8D0[i];
        for (j = 0; j < 46; j++) {
            g->unk170[j] = 0;
            g->unk0[j] = 0;
            g->unkB8[j] = 0;
        }
    }
}

void func_00346430(void) {
    Obj *base = func_003639D0();
    Obj *p = base;
    Obj *nxt = base + 1;
    Obj *prev = base - 1;
    s32 i;
    s32 n = 0x200;

    for (i = 0; i < n; i++) {
        p->next = prev;
        p->prev = nxt;
        p->update = 0;
        p->unkC = 0;
        p->unk1A = 0;
        p->unk18 = 0;
        p->flags = 0;
        prev++;
        nxt++;
        p++;
    }
    p--;
    p->prev = &D_008BF770;
    base->next = &D_008BF770;
    D_008BF770.prev = base;
    D_008BF770.next = p;
    D_008BF610.next = &D_008BF610;
    D_005F6CCC = 0x200;
    D_008BF610.prev = &D_008BF610;
    D_005F6CC8 = 0;
    D_005F6CD0 = 0;
    func_003463A8();
    func_003463B8();
    func_00346210();
    func_00345FC8();
}

Obj *func_003464F8(void (*update)(Obj *o)) {
    Obj *head;
    s32 n;
    Obj *o;
    Obj *p;
    Obj *act;
    Obj *q;

    if (D_005F6CD2 == 0) {
        head = &D_008BF770;
        n = D_005F6CCC;
        o = head->prev;
        if (n > 0) {
            goto take;
        }
    }
    return 0;
take:
    p = o->prev;
    act = &D_008BF610;
    head->prev = p;
    p->next = head;
    q = act->prev;
    act->prev = o;
    o->next = act;
    o->prev = q;
    q->next = o;
    o->update = update;
    o->flags = 0;
    o->unk18 = 0;
    o->unk1A = 0;
    o->unk1C = 0;
    o->unkC = 0;
    o->slot = 0;
    D_005F6CCC = n - 1;
    D_005F6CC8++;
    return o;
}

/* Unlinks o from the active list and gives it back to the free list. */
void func_00346580(Obj *o) {
    Obj *p = o->prev;
    Obj *n = o->next;
    Obj *old;

    p->next = n;
    n->prev = p;
    old = D_008BF770.next;
    D_008BF770.next = o;
    o->prev = &D_008BF770;
    old->prev = o;
    D_005F6CCC++;
    D_005F6CC8--;
}

/* Marks o to be freed at the end of the frame (func_00346608). */
void func_003465C8(Obj *o) {
    o->flags |= 0x10000;
}

/* Frees every object with flag 1 or 2 at the end of the frame. */
void func_003465E0(void) {
    D_005F6CD0 |= 3;
}

/* Frees every object with flag 2 (and resets the side rings) at the end of the frame. */
void func_003465F0(void) {
    D_005F6CD0 |= 2;
}

/* Resets the pool. */
void func_00346600(void) {
    func_00346430();
}

/* Per frame (main.c): frees the objects marked for it, runs every object's update and sorts the drawn ones into the
 * draw chains of their animation bank and frame. */
/* NON_MATCHING: 12 lines: s (o + 0x20) lands in a0 instead of a1 and fr * 4 is computed before fr * 2 */
#if 0
void func_00346608(void) {
    Obj *o;
    Obj *next;
    u16 id;
    u32 fr;
    Unk8BF3A0 *b;
    Unk8BF8D0 *g;
    Obj **tail;
    ObjSub *s;

    func_003A4370();
    if (D_005F6CD2 != 0) {
        return;
    }
    for (o = D_008BF610.next; o != &D_008BF610; o = o->next) {
        if ((o->flags & D_005F6CD0) || (o->flags & 0x10000)) {
            if (o->flags & 2) {
                if (o->slot != 0) {
                    *o->slot = 0;
                }
            }
            func_00346580(o);
            o->flags = 0;
        }
    }
    if (D_005F6CD0 & 2) {
        func_00345FC8();
    }
    D_005F6CD0 = 0;
    func_00346210();
    o = D_008BF610.next;
    while (o != &D_008BF610) {
        if (D_005F5F10 != 0) {
            o->update(o);
        }
        if (o->flags & 0x10) {
            func_00346230(o);
        }
        s = &o->sub;
        if ((o->flags & 0xF) && !(o->flags & 0x40) && !(o->flags & 0x10000)) {
            id = s->unk1A;
            fr = s->unk1E;
            if (id < 26) {
                b = func_00343F08(id);
                if (b->unk2 != 0) {
                    fr = fr % b->unk2;
                } else {
                    fr = 0;
                }
                o->unkC = 0;
                o->flags |= 0x40;
                g = &D_008BF8D0[id];
                g->unk170[fr]++;
                tail = g->unkB8;
                tail += fr;
                if (*tail == 0) {
                    g->unk0[fr] = o;
                } else {
                    (*tail)->unkC = o;
                }
                *tail = o;
            }
        }
        next = o->next;
        if (o == next) {
            func_00346430();
            return;
        }
        o = next;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/geometry", func_00346608);
#endif

/* The larger of two far distances (sprites beyond it are not drawn). */
f32 func_00346808(void) {
    f32 a = D_00879C60[0];
    f32 b = D_00885500[0];

    if (a < b) {
        a = b;
    }
    return a;
}

/* Draws the sprite objects of banks 6-11 by draw chain (bank texture upload, then each object with flag 4). */
void func_00346830(void) {
    s32 i;
    s32 j;
    f32 far;
    Obj *o;
    Unk8BF8D0 *g;
    u32 pk;

    if (D_005F6CD2 != 0) {
        return;
    }
    far = func_00346808();
    FlushCache(0);
    Spr_Setup(3, 0x230);
    func_00366EA8((void *)0x70000290);
    for (i = 6; i < 12; i++) {
        g = &D_008BF8D0[i];
        for (j = 0; j < 46; j++) {
            if (g->unk170[j] > 0 && g->unk0[j] != 0) {
                pk = func_00342FB0();
                *(u32 *)0x700002E0 = pk;
                func_00344030(i, j, pk);
                o = g->unk0[j];
                if (o != 0) do {
                    if (!(o->flags & 4)) {
                        Dma_Wait((volatile u32 *)0x1000D000);
                    } else {
                        *(u32 *)0x700002E0 = func_00342FB0();
                        if (func_003459D0(o, far) != 0) {
                            Dma_Wait((volatile u32 *)0x1000D000);
                        }
                        o->flags &= 0xFFFFFFBF;
                        if (o == o->unkC) {
                            break;
                        }
                    }
                    o = o->unkC;
                } while (o != 0);
            }
        }
    }
}

/* Draws every sorted sprite object (flag 0x40) by bank and frame, with the screen matrix chosen by flag 4, then
 * empties the chains. */
void func_003469D8(void) {
    s32 i;
    s32 j;
    Obj *o;
    Unk8BF8D0 *g;
    u32 pk;

    if (D_005F6CD2 == 0) {
        FlushCache(0);
        Spr_Setup(3, 0x230);
        func_00366EA8((void *)0x70000110);
        sceVu0MulMatrix((void *)0x70000150, D_00433440, D_00433400);
        for (i = 0; i < 26; i++) {
            g = &D_008BF8D0[i];
            for (j = 0; j < 46; j++) {
                if (g->unk170[j] > 0 && g->unk0[j] != 0) {
                    pk = func_00342FB0();
                    *(u32 *)0x700002E0 = pk;
                    func_00344030(i, j, pk);
                    for (o = g->unk0[j]; o != 0; o = o->unkC) {
                        if (!(o->flags & 0x40)) {
                            Dma_Wait((volatile u32 *)0x1000D000);
                        } else {
                            *(u32 *)0x700002E0 = func_00342FB0();
                            if (o->flags & 4) {
                                sceVu0CopyMatrix((void *)0x70000290, (void *)0x70000110);
                            } else {
                                sceVu0CopyMatrix((void *)0x70000290, (void *)0x70000150);
                            }
                            if (func_003459D0(o, 0.0f) != 0) {
                                Dma_Wait((volatile u32 *)0x1000D000);
                            }
                            o->flags &= 0xFFFFFFBF;
                            if (o == o->unkC) {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    func_003463B8();
}

/* Animated sprite with its parameters from record unk22: plays its frames once, the bank frame held at the last one;
 * frees itself at the end. */
void func_00346BB8(Obj *e) {
    ObjSub *s = &e->sub;
    ObjParam *prm = func_00343F38(s->unk22);
    ObjFrame *fr;
    Unk8BF3A0 *b;

    if (e->unk18 == 0) {
        s->unk90 = 0x80;
        s->unk4C = 0x96;
        s->unk96 = 0x1E;
        s->unk30[0] = prm->unk0[0];
        s->unk30[1] = prm->unk0[1];
        s->unk30[2] = prm->unk0[2];
        s->unk40 = prm->unkC;
        s->unk44 = prm->unk10;
        s->unk48 = prm->unk14;
        e->unk18++;
    } else {
        fr = s->unk10;
        s->unk60[0] = fr->unk0[0] * s->unk30[0];
        s->unk60[1] = fr->unk0[1] * s->unk30[1];
        s->unk60[2] = fr->unk0[2] * s->unk30[2];
        s->unk60[3] = 1.0f;
        s->unk54 = fr->unkC;
        s->unk58 = fr->unk10;
        if (s->unk1C != 0) {
            b = func_00343F08(s->unk1A);
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                if (s->unk58 < 1.0f) {
                    if (++s->unk1E >= b->unk2) {
                        s->unk1E = b->unk2 - 1;
                    }
                }
                fr++;
                s->unk10 = fr;
                s->unk16++;
            }
        } else {
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                fr++;
                s->unk10 = fr;
                s->unk16++;
            }
        }
        if (s->unk16 >= s->unk14) {
            func_003465C8(e);
        }
    }
}

/* As func_00346BB8 with the bank frame looping. */
void func_00346DA8(Obj *e) {
    ObjSub *s = &e->sub;
    ObjParam *prm = func_00343F38(s->unk22);
    ObjFrame *fr;
    Unk8BF3A0 *b;

    if (e->unk18 == 0) {
        s->unk90 = 0x80;
        s->unk4C = 0x96;
        s->unk96 = 0x1E;
        s->unk30[0] = prm->unk0[0];
        s->unk30[1] = prm->unk0[1];
        s->unk30[2] = prm->unk0[2];
        s->unk40 = prm->unkC;
        s->unk44 = prm->unk10;
        s->unk48 = prm->unk14;
        e->unk18++;
    } else {
        fr = s->unk10;
        s->unk60[0] = fr->unk0[0] * s->unk30[0];
        s->unk60[1] = fr->unk0[1] * s->unk30[1];
        s->unk60[2] = fr->unk0[2] * s->unk30[2];
        s->unk60[3] = 1.0f;
        s->unk54 = fr->unkC;
        s->unk58 = fr->unk10;
        if (s->unk1C != 0) {
            b = func_00343F08(s->unk1A);
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                if (s->unk58 < 1.0f) {
                    s->unk1E++;
                    if (b->unk2 != 0) {
                        s->unk1E %= b->unk2;
                    }
                }
                fr++;
                s->unk10 = fr;
                s->unk16++;
            }
        } else {
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                fr++;
                s->unk10 = fr;
                s->unk16++;
            }
        }
        if (s->unk16 >= s->unk14) {
            func_003465C8(e);
        }
    }
}

/* Animated sprite at scale 1.5 that publishes its bank frame (unk4 of the bank record). */
void func_00346FA0(Obj *e) {
    ObjSub *s = &e->sub;
    ObjFrame *fr;

    func_00343F38(0);
    if (e->unk18 == 0) {
        s->unk40 = 1;
        s->unk90 = 0x80;
        s->unk4C = 0x96;
        s->unk30[0] = 1.5f;
        s->unk30[1] = 1.5f;
        s->unk30[2] = 1.5f;
        s->unk96 = 0x1E;
        if (s->unk1C != 0) {
            s->unk48 = 2.0f * s->unk48;
        } else {
            s->unk48 = s->unk48 * 4.0f;
        }
        e->unk18++;
    } else {
        fr = s->unk10;
        s->unk60[0] = fr->unk0[0] * s->unk30[0];
        s->unk60[1] = fr->unk0[1] * s->unk30[1];
        s->unk60[2] = fr->unk0[2] * s->unk30[2];
        s->unk60[3] = 1.0f;
        s->unk54 = fr->unkC;
        s->unk58 = fr->unk10;
        if (s->unk1C != 0) {
            func_00343F08(s->unk1A)->unk4 = s->unk1E;
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                if (s->unk58 < 1.0f) {
                    s->unk1E++;
                }
                fr++;
                s->unk10 = fr;
                s->unk16++;
            }
        } else {
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                fr++;
                s->unk10 = fr;
                s->unk16++;
            }
        }
        if (s->unk16 >= s->unk14) {
            func_003465C8(e);
        }
    }
}

/* Animated sprite at scale 2, distance 300. */
void func_00347178(Obj *e) {
    ObjSub *s = &e->sub;
    ObjFrame *fr;

    if (e->unk18 == 0) {
        s->unk40 = 1;
        s->unk90 = 0x80;
        s->unk4C = 0x96;
        s->unk30[0] = 2.0f;
        s->unk30[1] = 2.0f;
        s->unk30[2] = 2.0f;
        s->unk48 = 300.0f;
        s->unk96 = 0x1E;
        e->unk18++;
    } else {
        fr = s->unk10;
        s->unk60[0] = fr->unk0[0] * s->unk30[0];
        s->unk60[1] = fr->unk0[1] * s->unk30[1];
        s->unk60[2] = fr->unk0[2] * s->unk30[2];
        s->unk60[3] = 1.0f;
        s->unk54 = fr->unkC;
        s->unk58 = fr->unk10;
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            fr++;
            s->unk10 = fr;
            s->unk16++;
        }
        if (s->unk16 >= s->unk14) {
            func_003465C8(e);
        }
    }
}

/* Animated sprite at scale 1.5. */
void func_003472B8(Obj *e) {
    ObjSub *s = &e->sub;
    ObjFrame *fr;

    if (e->unk18 == 0) {
        s->unk40 = 1;
        s->unk90 = 0x80;
        s->unk4C = 0x96;
        s->unk30[0] = 1.5f;
        s->unk30[1] = 1.5f;
        s->unk30[2] = 1.5f;
        s->unk96 = 0x1E;
        e->unk18++;
    } else {
        fr = s->unk10;
        s->unk60[0] = fr->unk0[0] * s->unk30[0];
        s->unk60[1] = fr->unk0[1] * s->unk30[1];
        s->unk60[2] = fr->unk0[2] * s->unk30[2];
        s->unk60[3] = 1.0f;
        s->unk54 = fr->unkC;
        s->unk58 = fr->unk10;
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            fr++;
            s->unk10 = fr;
            s->unk16++;
        }
        if (s->unk16 >= s->unk14) {
            func_003465C8(e);
        }
    }
}

/* Looping bank animation published to the bank record, distance 533. */
void func_003473F0(Obj *e) {
    ObjSub *s = &e->sub;
    Unk8BF3A0 *b = func_00343F08(s->unk1A);
    ObjFrame *fr;

    if (e->unk18 == 0) {
        s->unk96 = 0x1E;
        s->unk40 = 1;
        s->unk44 = 128.0f;
        s->unk90 = 0x80;
        s->unk48 = 533.0f;
        s->unk4C = 0x96;
        e->unk18++;
    } else {
        fr = s->unk10;
        s->unk60[0] = fr->unk0[0] * s->unk30[0];
        s->unk60[1] = fr->unk0[1] * s->unk30[1];
        s->unk60[2] = fr->unk0[2] * s->unk30[2];
        s->unk60[3] = 1.0f;
        s->unk54 = fr->unkC;
        s->unk58 = fr->unk10;
        b->unk4 = s->unk1E;
        if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
            if (++s->unk1E >= b->unk2) {
                s->unk1E = 0;
            }
            fr++;
            s->unk10 = fr;
            s->unk16++;
        }
        if (s->unk16 >= s->unk14) {
            func_003465C8(e);
        }
    }
}

/* As func_00346FA0 without the parameter record, the distance doubled when the bank frame is not used. */
void func_00347568(Obj *e) {
    ObjSub *s = &e->sub;
    ObjFrame *fr;

    func_00343F08(s->unk1A);
    if (e->unk18 == 0) {
        s->unk30[0] = 1.5f;
        s->unk30[1] = 1.5f;
        s->unk96 = 0x1E;
        s->unk40 = 1;
        s->unk90 = 0x80;
        s->unk30[2] = 1.5f;
        s->unk4C = 0x96;
        if (s->unk1C == 0) {
            s->unk48 = 2.0f * s->unk48;
        }
        e->unk18++;
    } else {
        fr = s->unk10;
        s->unk60[0] = fr->unk0[0] * s->unk30[0];
        s->unk60[1] = fr->unk0[1] * s->unk30[1];
        s->unk60[2] = fr->unk0[2] * s->unk30[2];
        s->unk60[3] = 1.0f;
        s->unk54 = fr->unkC;
        s->unk58 = fr->unk10;
        if (s->unk1C == 0) {
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                fr++;
                s->unk10 = fr;
                s->unk16++;
            }
        } else {
            func_00343F08(s->unk1A)->unk4 = s->unk1E;
            if (D_005F5EE0 == 0 && D_005F5EE4 == 0 && func_003A9698() == 0) {
                if (s->unk58 < 1.0f) {
                    s->unk1E++;
                }
                fr++;
                s->unk10 = fr;
                s->unk16++;
            }
        }
        if (s->unk16 >= s->unk14) {
            func_003465C8(e);
        }
    }
}

/* The effect kind id (36 entries of the handler table D_005193B8), or NULL. */
void *func_00347730(s16 id) {
    if ((u16)id >= 36) {
        return 0;
    }
    return (void *)&D_005193B8[id];
}
