/* 0x3564C8-0x3567A8 (module theater, docs/game_overview.md section 4; docs/systems/modes.md). */
#include "common.h"

typedef f32 Vu0Matrix[4][4] __attribute__((aligned(16)));

/* One record of a fighter's motion list (arg1 of func_003564C8): the part id at +0x1C. */
typedef struct {
    /* 0x00 */ u8 pad0[0x1C];
    /* 0x1C */ s32 id;
} ThPart;

/* A pose: the part count at +4, the part pointers from +0x194. */
typedef struct {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 count;
    /* 0x008 */ s32 unk8;
    /* 0x00C */ ThPart *part[1];
} ThPose;

/* Six joint indices per fighter slot (D_0046B010, 12 bytes per slot). */
typedef struct {
    /* 0x0 */ s16 j[6];
} ThJoints;

extern ThJoints D_0046B010[];
extern f32 D_004334C0[4][4];

s32 func_003B87F8(s16 id);
void sceVu0CopyMatrix(void *dst, void *src);
void sceVu0MulMatrix(void *m0, void *m1, void *m2);
void func_00349D70(void *dst, void *src);
void func_00349C50(void *a, void *b, void *c);
void func_0034E148(u8 *f, s32 n, ThPose *p);

void func_003564C8(u8 *f, ThPose *p) {
    Vu0Matrix m[2];
    s32 r;
    s32 n;
    s32 i;
    u8 *a;
    u8 *b;

    r = func_003B87F8(*(s16 *)(f + 0x20));
    n = p->count;
    if (n >= 0x40) {
        n = 0x3F;
        p->count = 0x3F;
    }
    a = f + 0x5730;
    sceVu0CopyMatrix(f + 0x560, f + 0x8790);
    i = n;
    func_00349D70(f + 0x8910, (u8 *)r + 0x3C);
    func_00349D70(f + 0x8950, (u8 *)r + 0x48);
    func_00349C50(f + 0xDB0, f + 0x87D0, m);
    b = f + 0xA050;
    for (; i > 0; i--) {
        sceVu0MulMatrix(a, *(void **)(a + 0xB0), b);
        b += 0x40;
        sceVu0MulMatrix(a + 0x40, D_004334C0, a);
        a += 0xC0;
    }
    func_0034E148(f, n, p);
    if (*(u16 *)(f + 0xB130) & 2) {
        s32 j;

        for (j = 0; j < n; j++) {
            switch (p->part[j + 0x62]->id) {
            case 31:
                D_0046B010[*(s16 *)(f + 0x20)].j[0] = j;
                break;
            case 34:
                D_0046B010[*(s16 *)(f + 0x20)].j[1] = j;
                break;
            case 37:
                D_0046B010[*(s16 *)(f + 0x20)].j[2] = j;
                break;
            case 32:
                D_0046B010[*(s16 *)(f + 0x20)].j[3] = j;
                break;
            case 35:
                D_0046B010[*(s16 *)(f + 0x20)].j[4] = j;
                break;
            case 38:
                D_0046B010[*(s16 *)(f + 0x20)].j[5] = j;
                break;
            }
        }
    }
}

/* m = identity. The body is hand-written inline asm with its own return (a1 holds 1.0f, `sq $0` clears the rows);
 * the compiler's own `jr ra` / `nop` after it is what splat saw as func_00356738 [V: 0x356738 is that epilogue, the
 * next function starts 8-aligned]. */
#ifndef PORT /* hand-written EE / VU0 assembly: the PC build has its own (port/src/vu0_pc.c) */
void Mtx_Unit(u128 *m) {
    __asm__ volatile(".set noreorder\n\t"
                     "lui $5, 0x3F80\n\tsq $0, 0x0(%0)\n\tsq $0, 0x10(%0)\n\tsq $0, 0x20(%0)\n\tsq $0, 0x30(%0)\n\t"
                     "sw $5, 0x0(%0)\n\tsw $5, 0x14(%0)\n\tsw $5, 0x28(%0)\n\tjr $31\n\tsw $5, 0x3C(%0)\n\t"
                     ".set reorder"
                     : : "r"(m) : "$5", "memory");
}
#endif

/* m = 0, the same way; splat's func_00356754 (not 8-aligned) is the compiler's epilogue after the asm. */
#ifndef PORT /* hand-written EE / VU0 assembly: the PC build has its own (port/src/vu0_pc.c) */
void Mtx_Zero(u128 *m) {
    __asm__ volatile(".set noreorder\n\t"
                     "sq $0, 0x0(%0)\n\tsq $0, 0x10(%0)\n\tsq $0, 0x20(%0)\n\tjr $31\n\tsq $0, 0x30(%0)\n\t"
                     ".set reorder"
                     : : "r"(m) : "memory");
}
#endif

/* d.xyz = s.xyz */
void func_00356760(f32 *d, f32 *s) {
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

/* d.xyz = s.xyz * k */
void func_00356780(f32 *d, f32 *s, f32 k) {
    d[0] = s[0] * k;
    d[1] = s[1] * k;
    d[2] = s[2] * k;
}
