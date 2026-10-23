/* 0x340000-0x341B28 (module render_fighter_a; docs/systems/render.md section 12): the stage renderer, formerly
 * src/340000.c: the stage scene D_005F6780 (depth merge sort, level of detail), the stage draw func_00340E28 and the
 * per-frame scene update func_00341AE8 that Battle_UpdateFrame calls. One object [V: its .lit4 pool 0x5F5378-0x5F539C,
 * a subsegment; LIT4_WORD lines for the functions still in assembly]; the end is [I]. Every NON_MATCHING draft (#if 0)
 * compiles when enabled. */
#include "common.h"
#include "fighter.h"

/* A table of model files (render_fighter_b.c). */
typedef struct {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ void **models;
} ModelFile;

/* A key of the depth sort (func_00340BF0). */
typedef struct {
    /* 0x0 */ f32 key;
    /* 0x4 */ s32 index;
} SortKey;

/* The merge sort's indexes (file statics of the original [I]). */
extern s32 D_005F6F1C;
extern s32 D_005F6F20;
extern s32 D_005F6F24;
extern s32 D_005F6F28;

/* One drawn entry of the scene (0x150 bytes), as func_00341218 fills it and Model_Draw takes it. */
typedef struct {
    /* 0x000 */ f32 m[4][4];                  /* model-view-projection */
    /* 0x040 */ f32 m40[4][4];
    /* 0x080 */ u8 pad80[0x30];
    /* 0x0B0 */ u128 pos;                     /* world position of the cell */
    /* 0x0C0 */ f32 light[2][4][4];           /* the entry's own light set (unk2 != 0 stages); light[1][3][3] alpha */
    /* 0x140 */ s32 file;                     /* level of detail */
    /* 0x144 */ s32 model;
    /* 0x148 */ s32 flags;                    /* 1, 2: casts the shadow of fighter 1, 2 */
    /* 0x14C */ u8 pad14C[4];
} SceneEnt;

/* The scene behind D_005F6780: the camera position at 0x14, a vector at 0x40, the number of
 * sorted entries, the entries and the two key arrays of the depth sort [V offsets; I roles]. */
typedef struct {
    /* 0x00000 */ s16 unk0;
    /* 0x00002 */ s16 unk2;
    /* 0x00004 */ s16 levels;
    /* 0x00006 */ s16 unk6;
    /* 0x00008 */ s16 unk8;
    /* 0x0000A */ s16 col;                    /* grid cell of the camera */
    /* 0x0000C */ s16 row;
    /* 0x0000E */ s16 unkE;
    /* 0x00010 */ s16 unk10;                  /* second box set (raised floor) present */
    /* 0x00012 */ u8 pad12[2];
    /* 0x00014 */ f32 x;
    /* 0x00018 */ f32 y;
    /* 0x0001C */ f32 z;
    /* 0x00020 */ f32 unk20;
    /* 0x00024 */ f32 unk24;
    /* 0x00028 */ f32 unk28;
    /* 0x0002C */ f32 unk2C;
    /* 0x00030 */ f32 thr[4];
    /* 0x00040 */ u128 unk40;
    /* 0x00050 */ u128 unk50;
    /* 0x00060 */ f32 box[9][4];              /* corners of the stage floor and of the raised floor, a centre */
    /* 0x000F0 */ s16 grid[16][16];
    /* 0x002F0 */ void *spr;                  /* the scratchpad work area of func_00341218 */
    /* 0x002F4 */ s32 count;
    /* 0x002F8 */ u8 pad2F8[8];
    /* 0x00300 */ SceneEnt ent[256];
    /* 0x15300 */ f32 mats[256][4][4];
    /* 0x19300 */ SortKey keys[256];
    /* 0x19B00 */ SortKey tmp[256];
} Scene;

extern Scene *D_005F6780;

void func_00340BF0(SortKey *keys, SortKey *tmp, s32 lo, s32 hi);

/* Table of 0x30-byte records at 0x521700 (D_00521700), indexed by id. Only the fields used so far are named. */
typedef struct {
    /* 0x00 */ s16 unk0;
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s16 unk4;
    /* 0x06 */ s16 unk6;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;                      /* scale of the floor box */
    /* 0x10 */ f32 unk10;                     /* height of the raised floor (0: none) */
    /* 0x14 */ f32 unk14;
    /* 0x18 */ f32 unk18;
    /* 0x1C */ u8 pad1C[4];
    /* 0x20 */ u128 unk20;
} Unk521700;

extern Unk521700 D_00521700[];

/* The stage model bank's header as func_00340070 reads it (func_00356E50). */
typedef struct {
    /* 0x00 */ s16 unk0;                      /* level-of-detail count */
    /* 0x02 */ u8 pad2[6];
    /* 0x08 */ u32 unk8;                      /* grid columns */
    /* 0x0C */ u32 unkC;                      /* grid rows */
    /* 0x10 */ f32 unk10;                     /* stage width */
    /* 0x14 */ f32 unk14;                     /* stage depth */
} StageBank;

StageBank *func_00356E50(s32 i);
Scene *func_003639C8(void);
s32 func_003AA5A0(void);
void func_00340070(u32 id);
void sceVu0ApplyMatrix(f32 *out, f32 (*m)[4], f32 *v);
void sceVu0DivVectorXYZ(f32 *out, f32 *v, f32 d);
s32 func_00340528(f32 x, f32 a, f32 b);
s32 func_00340588(f32 (*v)[4]);

/* 1 when the projected point c (GS 12.4 pixels) lies off the 640 x 448 screen. The right and bottom bounds come in as
 * parameters so that the compiler keeps `li / slt` for them (fake: the original likely had them literal). */
static inline s32 Scene_IsOff(f32 *c, s32 hx, s32 hy) { /* fake */
    s32 x = c[0];
    s32 y = c[1];

    return x < 0x6C0 || x > hx || y < 0x720 || y > hy;
}

void func_00357258(f32 *values, s32 count);
float sqrtf(float x);
void func_0035B4B0(void *a, void *b);
void func_00366ED0(void);
void func_00341218(void);
void func_003D1040(void);
void Wind_Update(void);
extern s32 D_00415808[];
extern u8 D_00436530[];

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

LIT4_WORD(D_005F5378, 0x477FFF00);
LIT4_WORD(D_005F537C, 0x47435000);
LIT4_WORD(D_005F5380, 0x477FFF00);
LIT4_WORD(D_005F5384, 0x459C4000);
LIT4_WORD(D_005F5388, 0x461C4000);
LIT4_WORD(D_005F538C, 0x464B2000);
/* NON_MATCHING: 9 lines: the record's 0x8 / 0x14 floats take each other's registers and the D_00521700 lui is scheduled later */
#if 0
/* Sets one corner of the floor box (w = 1). */
static inline void Scene_SetCorner(f32 *v, f32 x, f32 y, f32 z) {
    v[0] = x;
    v[1] = y;
    v[2] = z;
    v[3] = 1.0f;
}

void func_00340070(u32 id) {
    StageBank *b = func_00356E50(id);
    Scene *s = func_003639C8();
    Scene *sc;
    Unk521700 *r;
    u16 r0, r2, r4, r6;
    u16 lv;
    u128 *q;
    f32 f8, f14, f18;
    f32 w;
    f32 h;
    f32 k;
    f32 a;
    f32 c;
    f32 y;
    f32 z0;
    f32 *v;
    s32 i;
    s32 j;

    D_005F6780 = s;
    s->x = w = b->unk10;
    if (id >= 24) {
        id = 0;
    }
    h = b->unk14;
    s->unk0 = id;
    s->y = h;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            s->grid[i][j] = (i % b->unkC) * b->unk8 + j % b->unk8;
        }
    }
    sc = D_005F6780;
    sc->z = 600.0f;
    r = &D_00521700[sc->unk0];
    r0 = r->unk0;
    r2 = r->unk2;
    r4 = r->unk4;
    r6 = r->unk6;
    sc->unk6 = r0;
    sc->unk8 = r2;
    sc->unk2 = r4;
    sc->unkE = r6;
    f8 = r->unk8;
    f14 = r->unk14;
    f18 = r->unk18;
    lv = b->unk0;
    sc->levels = lv;
    sc->unk20 = f8;
    sc->unk24 = f14;
    sc->unk28 = f18;
    q = &sc->unk50; /* fake: a store the compiler cannot keep apart from D_005F6780 */
    *q = D_00521700[sc->unk0].unk20;
    k = D_00521700[D_005F6780->unk0].unkC;
    a = w * k;
    c = h * k;
    Scene_SetCorner(D_005F6780->box[0], a * 0.5f, 0.0f, c * 0.5f);
    Scene_SetCorner(D_005F6780->box[3], a * 0.5f, 0.0f, -c * 0.5f);
    Scene_SetCorner(D_005F6780->box[1], -a * 0.5f, 0.0f, c * 0.5f);
    v = D_005F6780->box[2];
    Scene_SetCorner(v, -a * 0.5f, 0.0f, -c * 0.5f);
    z0 = v[1];
    y = D_00521700[D_005F6780->unk0].unk10;
    if (y != z0) {
        D_005F6780->unk10 = 1;
        Scene_SetCorner(D_005F6780->box[4], w * 0.5f, y, h * 0.5f);
        Scene_SetCorner(D_005F6780->box[7], w * 0.5f, y, -h * 0.5f);
        Scene_SetCorner(D_005F6780->box[5], -w * 0.5f, y, h * 0.5f);
        Scene_SetCorner(D_005F6780->box[6], -w * 0.5f, y, -h * 0.5f);
        Scene_SetCorner(D_005F6780->box[8], z0, y, z0);
    } else {
        D_005F6780->unk10 = 0;
    }
    if (D_005F6780->unk0 == 0) {
        D_005F6780->unk2C = 65535.0f;
        D_005F6780->thr[0] = 8000.0f;
        D_005F6780->thr[1] = 8000.0f;
        D_005F6780->thr[2] = 8000.0f;
        D_005F6780->thr[3] = 8000.0f;
    } else if (func_003AA5A0() != 0) {
        D_005F6780->unk2C = 0.0f;
        D_005F6780->thr[0] = 0.0f;
        D_005F6780->thr[1] = 50000.0f;
        D_005F6780->thr[2] = 50000.0f;
        D_005F6780->thr[3] = 50000.0f;
    } else {
        D_005F6780->unk2C = 65535.0f;
        D_005F6780->thr[0] = 5000.0f;
        D_005F6780->thr[1] = 8000.0f;
        D_005F6780->thr[2] = 10000.0f;
        D_005F6780->thr[3] = 13000.0f;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_00340070);
#endif

PORT_RET(s16) func_00340398(s32 id) {
    return D_00521700[id].unk4;
}

PORT_RET(s16) func_003403B8(void) {
    return D_005F6780->unk0;
}

void func_003403C8(f32 *v) {
    Scene *s = D_005F6780;
    f32 x, y, z;

    x = s->x;
    z = s->z;
    y = s->y;
    v[0] = x;
    v[1] = z;
    v[2] = y;
    v[3] = 1.0f;
}

void func_003403F8(u128 *v) {
    *v = D_005F6780->unk40;
}

/* Level 1-5 of a distance: the mean of the positive w of four vectors (65535 when none), written to *out, against the
 * scene's thresholds thr[0..3] (only `levels` of them used). */
s32 func_00340408(f32 (*v)[4], f32 *out) {
    f32 n = 0.0f;
    f32 sum;
    f32 d = 65535.0f;
    s32 i;

    sum = n;
    for (i = 0; i < 4; i++) {
        if (0.0f < v[i][3]) {
            sum += v[i][3];
            n += 1.0f;
        }
    }
    if (n != 0.0f) {
        *out = sum / n;
        d = *out;
    }
    if (D_005F6780->levels >= 6 && D_005F6780->thr[3] < d) {
        return 5;
    }
    if (D_005F6780->levels >= 5 && D_005F6780->thr[2] < d) {
        return 4;
    }
    if (D_005F6780->levels >= 4 && D_005F6780->thr[1] < d) {
        return 3;
    }
    if (D_005F6780->levels >= 3 && D_005F6780->thr[0] < d) {
        return 2;
    }
    return 1;
}

s32 func_00340528(f32 x, f32 a, f32 b) {
    if (b < a) {
        return x <= a && b <= x;
    }
    return a <= x && x <= b;
}

s32 func_00340588(f32 (*v)[4]) {
    s32 i;
    s32 j;
    f32 s;
    f32 x;

    for (i = 0; i < 4; i++) {
        j = i + 1;
        if (i == 3) {
            j = 0;
        }
        if (v[i][2] < 0.0f && v[j][2] < 0.0f) {
            continue;
        }
        if (v[j][1] == v[i][1]) {
            continue;
        }
        s = (v[j][0] - v[i][0]) / (v[j][1] - v[i][1]);
        if (func_00340528(1824.0f, v[i][1], v[j][1])) {
            x = (1824.0f - v[i][1]) * s + v[i][0];
            if (1728.0f <= x && x <= 2368.0f) {
                return 0;
            }
        }
        if (func_00340528(2272.0f, v[i][1], v[j][1])) {
            x = (2272.0f - v[i][1]) * s + v[i][0];
            if (1728.0f <= x && x <= 2368.0f) {
                return 0;
            }
        }
    }
    for (i = 0; i < 4; i++) {
        j = i + 1;
        if (i == 3) {
            j = 0;
        }
        if (v[i][2] < 0.0f && v[j][2] < 0.0f) {
            continue;
        }
        if (v[j][0] == v[i][0]) {
            continue;
        }
        s = (v[j][1] - v[i][1]) / (v[j][0] - v[i][0]);
        if (func_00340528(1728.0f, v[i][0], v[j][0])) {
            x = (1728.0f - v[i][0]) * s + v[i][1];
            if (1824.0f <= x && x <= 2272.0f) {
                return 0;
            }
        }
        if (func_00340528(2368.0f, v[i][0], v[j][0])) {
            x = (2368.0f - v[i][0]) * s + v[i][1];
            if (1824.0f <= x && x <= 2272.0f) {
                return 0;
            }
        }
    }
    return 1;
}

/* Projects the four floor corners with m into out (y mirrored for points behind the camera). 1 when all four are
 * behind; else, when the stage centre (and the raised floor's centre) are off screen, whether no box edge crosses the
 * screen (func_00340588); 0 when a centre is on screen. */
s32 func_003408B0(f32 (*m)[4], f32 (*out)[4]) {
    f32 c[4];
    f32 top[4][4];
    s32 i;
    s32 n;
    s32 off;
    f32 *p;

    n = 0;
    for (i = 0; i < 4; i++) {
        sceVu0ApplyMatrix(out[i], m, D_005F6780->box[i]);
        sceVu0DivVectorXYZ(out[i], out[i], out[i][3]);
        if (out[i][2] < 0.0f) {
            n++;
            out[i][1] = -out[i][1];
        }
    }
    if (n == 4) {
        return 1;
    }
    sceVu0DivVectorXYZ(c, m[3], m[3][3]);
    if (!Scene_IsOff(c, 0x940, 0x8E0)) {
        return 0;
    }
    if (D_005F6780->unk10 == 0) {
        return func_00340588(out);
    }
    sceVu0ApplyMatrix(c, m, D_005F6780->box[8]);
    sceVu0DivVectorXYZ(c, c, c[3]);
    if (!Scene_IsOff(c, 0x940, 0x8E0)) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        p = top[i];
        sceVu0ApplyMatrix(p, m, D_005F6780->box[4 + i]);
        sceVu0DivVectorXYZ(p, p, p[3]);
    }
    return func_00340588(out) && func_00340588(top);
}

/* 1 when the point (x1, y1) is within `radius` of (x0, y0). */
s32 func_00340AF0(f32 x0, f32 y0, f32 radius, f32 x1, f32 y1) {
    return sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0)) <= radius;
}

void func_00340B30(f32 *dir, f32 *p, f32 *out) {
    out[0] = p[0] + -p[1] * dir[0] / dir[1];
    out[1] = 0.0f;
    out[2] = p[2] + -p[1] * dir[2] / dir[1];
    out[3] = 1.0f;
}

void func_00340BA0(void) {
    s32 i;

    D_005F6780->count = 0;
    for (i = 0; i < 256; i++) {
        D_005F6780->keys[i].index = i;
        D_005F6780->ent[i].flags = 0;
    }
}

/* Merge sort of keys[lo..hi], largest key first, through tmp. */
void func_00340BF0(SortKey *keys, SortKey *tmp, s32 lo, s32 hi) {
    s32 mid;

    if (lo < hi) {
        mid = (lo + hi) >> 1;
        func_00340BF0(keys, tmp, lo, mid);
        func_00340BF0(keys, tmp, mid + 1, hi);
        D_005F6F28 = 0;
        for (D_005F6F1C = lo; D_005F6F1C <= mid; D_005F6F1C++) {
            tmp[D_005F6F28++] = keys[D_005F6F1C];
        }
        D_005F6F24 = lo;
        D_005F6F1C = mid + 1;
        D_005F6F20 = 0;
        while (D_005F6F1C <= hi && D_005F6F20 < D_005F6F28) {
            if (tmp[D_005F6F20].key >= keys[D_005F6F1C].key) {
                keys[D_005F6F24++] = tmp[D_005F6F20++];
            } else {
                keys[D_005F6F24++] = keys[D_005F6F1C++];
            }
        }
        while (D_005F6F20 < D_005F6F28) {
            keys[D_005F6F24++] = tmp[D_005F6F20++];
        }
    }
}

void func_00340DF8(void) {
    func_00340BF0(D_005F6780->keys, D_005F6780->tmp, 0, D_005F6780->count - 1);
}

s32 func_00356F48(s32 x);
s32 func_00356EE8(s32 side);
ModelFile **func_00356E38(s32 i);
void func_00353DC0(f32 (*out)[4][4]);
f32 func_00366EC8(void);
f32 sinf(f32 x);
f32 cosf(f32 x);
void func_00356840(void);
void sceVu0CopyMatrix(f32 (*m0)[4], f32 (*m1)[4]);
f32 func_00342FA0(void);
void func_00342F98(f32 v);
void func_003621F8(s32 a);
void Model_Draw(ModelFile **files, SceneEnt *o, void *a, u32 kind);
void func_00362318(void);
s32 Tag_StepCounters(s32 side, s32 n);
void func_00342818(Fighter *f);
void func_00342128(Fighter *f);
void func_003422C0(Fighter *f, ModelFile **files, SceneEnt *o);
void func_00342A38(Fighter *f);
void func_00342988(void);
extern u16 D_005F5CB6;
extern Fighter gFighters[];
extern Fighter D_00884EE0;

void func_00340E28(s32 pass) {
    f32 light[2][4][4];
    s32 shad;
    f32 save;
    f32 a;
    SortKey *k;
    SceneEnt *ent;
    s32 kind;
    ModelFile **files;
    s32 i;
    s32 idx;

    shad = 0;
    if (pass == 0) {
        shad = D_005F6780->unk2 == 1;
    }
    save = 1.0f;
    k = D_005F6780->keys;
    ent = D_005F6780->ent;
    kind = func_00356F48(D_005F6780->unk2);
    if (func_00356EE8(pass) == 0) {
        return;
    }
    files = func_00356E38(pass);
    func_00353DC0(light);
    a = -func_00366EC8();
    light[0][0][3] = sinf(a);
    a = -func_00366EC8();
    light[0][1][3] = cosf(a);
    func_00356840();
    if (pass == 0 && D_005F6780->unkE != 0) {
        for (i = 0; i < D_005F6780->count; i++) {
            sceVu0CopyMatrix(ent[i].m, D_005F6780->mats[i]);
        }
    }
    if (shad) {
        save = func_00342FA0();
    }
    if (D_005F6780->count != 0) {
        func_003621F8(pass == 0 ? 1 : 2);
    }
    for (i = 0; i < D_005F6780->count; i++, k++) {
        idx = k->index;
        if (D_005F6780->unk2 == 0) {
            Model_Draw(files, &ent[idx], light, kind);
        } else {
            if (shad) {
                func_00342F98(ent[idx].light[1][3][3]);
            }
            Model_Draw(files, &ent[idx], ent[idx].light, kind);
        }
    }
    if (shad) {
        func_00342F98(save);
    }
    if (D_005F5CB6 == 14) {
        if (pass != 0) {
            return;
        }
        func_00362318();
        if (gFighters[0].unk127 != 0 && Tag_StepCounters(0, 0) == 0) {
            func_00342A38(&gFighters[0]);
        }
        if (gFighters[1].unk127 != 0 && Tag_StepCounters(1, 0) == 0) {
            func_00342A38(&D_00884EE0);
        }
        func_00342988();
        return;
    }
    if (files[0]->models[0] == NULL) {
        return;
    }
    func_00362318();
    if (gFighters[0].unk127 != 0 && Tag_StepCounters(0, 0) == 0) {
        func_00342818(&gFighters[0]);
        func_00342128(&gFighters[0]);
        k = D_005F6780->keys;
        for (i = 0; i < D_005F6780->count; i++, k++) {
            idx = k->index;
            if (ent[idx].flags & 1) {
                func_003422C0(&gFighters[0], files, &ent[idx]);
            }
        }
    }
    if (gFighters[1].unk127 != 0 && Tag_StepCounters(1, 0) == 0) {
        func_00342818(&D_00884EE0);
        func_00342128(&D_00884EE0);
        k = D_005F6780->keys;
        for (i = 0; i < D_005F6780->count; i++, k++) {
            idx = k->index;
            if (ent[idx].flags & 2) {
                func_003422C0(&D_00884EE0, files, &ent[idx]);
            }
        }
    }
    func_00342988();
}

/* The scratchpad work area of the stage scene build (Spr_Setup(1, 0x1520)). */
typedef struct {
    /* 0x000 */ f32 view[4][4];
    /* 0x040 */ f32 view2[4][4];
    /* 0x080 */ f32 light[4][4];
    /* 0x0C0 */ f32 light2[4][4];
    /* 0x100 */ f32 world[4][4];
    /* 0x140 */ f32 mvp[4][4];
    /* 0x180 */ f32 mv2[4][4];
    /* 0x1C0 */ f32 local[4][4];
    /* 0x200 */ f32 base[2][4][4];
    /* 0x280 */ f32 proj[4][4];
    /* 0x2C0 */ f32 cam[4];
    /* 0x2D0 */ f32 pos[4];
    /* 0x2E0 */ f32 shadow[2][4];
    /* 0x300 */ f32 eye[4];
    /* 0x310 */ f32 grid[17][17][4];
} StageSpr;

typedef struct {
    f32 v[4];
} __attribute__((aligned(8))) Vec4;


u32 Spr_Setup(u32 count, s32 reserved);
void func_00340BA0(void);
void sceVu0MulMatrix(f32 (*m0)[4], f32 (*m1)[4], f32 (*m2)[4]);
void sceVu0TransMatrix(f32 (*m0)[4], f32 (*m1)[4], f32 *v);
void sceVu0AddVector(f32 *out, f32 *a, f32 *b);
void sceVu0SubVector(f32 *out, f32 *a, f32 *b);
void func_00366EB8(f32 (*m)[4]);
void Mtx_Unit(f32 (*m)[4]);
void func_00353C88(s32 i, f32 *out);
void func_00340B30(f32 *dir, f32 *p, f32 *out);
s32 func_003408B0(f32 (*m)[4], f32 (*out)[4]);
s32 func_00340408(f32 (*v)[4], f32 *out);
s32 func_00340AF0(f32 x0, f32 y0, f32 radius, f32 x1, f32 y1);
void func_003D43A8(f32 a, f32 b, f32 *v);
void func_003E13C8(f32 x);
void func_00340DF8(void);
extern f32 D_00433400[4][4];
extern f32 D_00433440[4][4];
extern f32 D_00433480[4][4];
extern f32 D_0046AF50[2][4][4];

LIT4_WORD(D_005F5394, 0x40555555);
LIT4_WORD(D_005F5398, 0x39AEC33E);
/* NON_MATCHING: 268 lines: register allocation (spr / pos / light2 pointers, s5 / s6), the reload of D_005F6780 and the stage scheduling around the camera cell and the distance test differ */
#if 0
void func_00341218(void) {
    f32 m[4][4];
    f32 dir[4];
    Vec4 p;
    Vec4 t;
    f32 depth;
    s32 col0;
    s32 lvl;
    s32 cols;
    s32 rows;
    f32 *at;
    s16 *cells;
    f32 (*view2)[4];
    f32 (*light)[4];
    f32 (*light2)[4];
    f32 (*world)[4];
    f32 (*mvp)[4];
    f32 (*mv2)[4];
    f32 (*local)[4];
    f32 (*base)[4][4];
    f32 (*proj)[4];
    f32 *cam;
    f32 *eye;
    f32 (*shadow)[4];
    f32 (*grid)[17][4];
    StageSpr *spr;
    f32 *pos;
    f32 w;
    f32 d;
    f32 z0;
    f32 x0;
    f32 hx;
    f32 hz;
    f32 alpha;
    f32 dist;
    s32 cx;
    s32 cz;
    s32 ix;
    s32 iz;
    s32 row;
    s32 col;
    s32 i;
    s32 j;
    s32 n;
    s32 fl;
    s32 in0;
    s32 in1;
    f32 a;
    SceneEnt *e;

    cols = D_005F6780->unk6;
    rows = D_005F6780->unk8;
    w = D_005F6780->x;
    d = D_005F6780->y;
    alpha = 0.0f;
    hx = cols * 0.5f;
    hz = rows * 0.5f;
    x0 = -w * hx;
    z0 = -d * hz;
    spr = (StageSpr *)Spr_Setup(1, 0x1520);
    grid = spr->grid;
    at = spr->local[3];
    view2 = spr->view2;
    light = spr->light;
    pos = spr->pos;
    light2 = spr->light2;
    world = spr->world;
    mvp = spr->mvp;
    mv2 = spr->mv2;
    local = spr->local;
    base = spr->base;
    proj = spr->proj;
    cam = spr->cam;
    shadow = spr->shadow;
    eye = spr->eye;
    D_005F6780->spr = grid;
    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = 0.0f;
    pos[3] = 0.0f;
    func_00340BA0();
    sceVu0MulMatrix(spr->view, D_00433480, D_00433400);
    if (D_005F6780->unkE != 0) {
        sceVu0MulMatrix(view2, D_00433440, D_00433400);
    }
    func_00366EB8(m);
    cx = m[3][0];
    cz = m[3][2];
    ix = (s32)(cx + w * 0.5f) / w;
    if (cx + w * 0.5f < 0.0f) {
        ix--;
    }
    iz = (s32)(cz + d * 0.5f) / d;
    if (cz + d * 0.5f < 0.0f) {
        iz--;
    }
    cam[0] = w * ix;
    cam[1] = 0.0f;
    cam[2] = d * iz;
    cam[3] = 1.0f;
    D_005F6780->unk40 = *(u128 *)cam;
    col0 = ix % cols;
    row = iz % rows;
    if (row < 0) {
        row += rows;
    }
    if (col0 < 0) {
        col0 += cols;
    }
    D_005F6780->row = row;
    D_005F6780->col = col0;
    Mtx_Unit(m);
    sceVu0TransMatrix(m, m, cam);
    sceVu0MulMatrix(spr->view, spr->view, m);
    if (D_005F6780->unkE != 0) {
        sceVu0MulMatrix(view2, view2, m);
    }
    for (i = 0; i < 2; i++) {
        t.v[3] = 1.0f;
        t.v[0] = gFighters[i].unk51C;
        t.v[1] = gFighters[i].unk520;
        t.v[2] = -(f32)gFighters[i].unk524;
        p = t;
        func_00353C88(i, dir);
        dir[0] = -dir[0];
        dir[2] = -dir[2];
        func_00340B30(dir, p.v, shadow[i]);
    }
    if (D_005F6780->unk2 == 1) {
        alpha = 3.3333333f;
        func_003D43A8(120.0f, 200.0f, cam);
        func_003E13C8(D_005F6780->unk24);
        sceVu0CopyMatrix(base[0], D_0046AF50[0]);
        sceVu0CopyMatrix(base[1], D_0046AF50[1]);
        *(u128 *)light2[2] = *(u128 *)D_0046AF50[0][2];
        *(u128 *)light2[3] = *(u128 *)D_0046AF50[0][3];
        *(u128 *)light[2] = *(u128 *)D_0046AF50[1][2];
        *(u128 *)light[3] = *(u128 *)D_0046AF50[1][3];
    } else {
        sceVu0CopyMatrix(light, D_00433400);
        eye[0] = (f32)D_00415808[5] - cam[0];
        eye[1] = -((f32)D_00415808[6] + cam[1]);
        eye[3] = 1.0f;
        eye[2] = -((f32)D_00415808[7] + cam[2]);
    }
    Mtx_Unit(world);
    Mtx_Unit(local);
    for (i = 0; i < rows; i++) {
        cells = D_005F6780->grid[row];
        col = col0;
        for (j = 0; j < cols; j++) {
            pos[2] = z0 + d * i;
            pos[0] = x0 + w * j;
            sceVu0AddVector(local[3], cam, pos);
            dist = sqrtf((cx - at[0]) * (cx - at[0]) + (cz - at[2]) * (cz - at[2]));
            if (D_005F6780->unk20 < dist
                || (*(u64 *)world[3] = *(u64 *)pos, ((s32 *)world[3])[2] = ((s32 *)pos)[2], /* fake: the original copies 12 bytes with ld + lw */
                    sceVu0MulMatrix(mvp, spr->view, world),
                    func_003408B0(mvp, proj) != 0)) {
                col++;
                if (col >= cols) {
                    col = 0;
                }
                continue;
            }
            if (D_005F6780->unkE != 0) {
                sceVu0MulMatrix(mv2, view2, world);
            }
            lvl = func_00340408(proj, &depth);
            if (D_005F6780->unk2 == 0) {
                sceVu0SubVector(light[3], eye, pos);
            } else {
                sceVu0AddVector(light2[0], base[0][0], grid[i + 1][j + 1]);
                sceVu0AddVector(light2[1], base[0][1], grid[i + 1][j]);
                sceVu0AddVector(light[0], base[1][0], grid[i][j + 1]);
                sceVu0AddVector(light[1], base[1][1], grid[i][j]);
                light2[3][3] = a = alpha - dist * 0.00033333333f;
                if (a < 0.0f) {
                    light2[3][3] = 0.0f;
                } else if (a > 1.0f) {
                    light2[3][3] = 1.0f;
                }
            }
            in0 = func_00340AF0(shadow[0][0], shadow[0][2], D_005F6780->unk28, at[0], at[2]);
            in1 = func_00340AF0(shadow[1][0], shadow[1][2], D_005F6780->unk28, at[0], at[2]);
            n = D_005F6780->count;
            fl = in0 ? 1 : 0;
            if (in1) {
                fl |= 2;
            }
            e = &D_005F6780->ent[n];
            e->file = lvl;
            e->model = cells[col];
            e->flags = fl;
            sceVu0CopyMatrix(e->m, mvp);
            e->pos = *(u128 *)local[3];
            sceVu0CopyMatrix(e->m40, light);
            sceVu0CopyMatrix(D_005F6780->mats[n], mv2);
            D_005F6780->keys[n].key = depth;
            if (D_005F6780->unk2 == 1) {
                sceVu0CopyMatrix(e->light[1], light2);
            }
            col++;
            if (col >= cols) {
                col = 0;
            }
            D_005F6780->count++;
        }
        row++;
        if (row >= rows) {
            row = 0;
        }
    }
    func_00340DF8();
}
#else
INCLUDE_ASM("asm/nonmatchings/render_fighter_a", func_00341218);
#endif

void func_00341AE8(void) {
    func_0035B4B0(D_00415808, D_00436530);
    func_00366ED0();
    func_00341218();
    func_003D1040();
    Wind_Update();
}
