/*
 * The vector units' micro mode: one interpreter for VU0 (vu0_micro.c: the skinning programs of src/vu0/) and VU1
 * (gs_vu1.c: the vertex programs of src/vu1/). The caller hands it the unit's registers and memory (VuCore).
 *
 * Structure after the reference project's VU1 interpreter (port/src/gs/gs_vu1.c of Tenkaichi3Decomp): upper and
 * lower halves decoded per instruction, the upper result committed after the lower half read its operands,
 * branches and the end bit taking effect after the following instruction (the delay slot), the Q register's latency
 * (div / sqrt 7, rsqrt 13 steps), the P register's (the EFU: erleng 24, eatan 54...) and the four-step delay of the
 * clipping and MAC flags. Unlike that file the arithmetic is the EE float model's vector unit (softfloat_ps2_inl.h):
 * the results feed the game's DMA packets and the GS, and the port keeps one number model. Two interpreters run it:
 * Vu_RunRef, one step per instruction in integer arithmetic (the specification), and the predecoded one below
 * (Vu_Run with a VuOp cache, T30 batch 2), which gets the same model's results from the SSE unit (vu_simd_inl.h).
 * One step is one instruction: not modelled are stalls (the programs are hand-scheduled, and the units interlock on
 * the vector registers, so reading a result early only waits for it), the MAC flag's underflow / overflow bits and
 * the status flag (no program reads them). A branch reads the integer registers as the previous instruction left
 * them (the game's programs keep an instruction between an integer operation and the branch that tests it, except
 * after the flag instructions, whose result they mean to read). Instructions outside the set the game's programs use
 * are counted (Vu_Unknown) and skipped.
 */
#include <stdint.h>
#include <string.h>
#define memcpy __builtin_memcpy /* -fno-builtin: inline the small copies of the hot paths */
#include "vu_micro_inl.h"

unsigned Vu_Unknown(const VuCore *u) {
    unsigned k, n = 0;
    for (k = 0; k < 128; k++) {
        n += u->unknown[0][k] + u->unknown[1][k];
    }
    return n;
}

/* ---------------------------------------------------------------------------------------------- interpreter */

#define DEST(n) ((dest >> (3 - (n))) & 1)
#define LDEST(n) ((ld >> (3 - (n))) & 1)
#define VI(r) u->vi[(r) & 15]
#define VF(r) u->vf[(r)]
#define PSET(v, lat)                                                                                               \
    do {                                                                                                           \
        u->pPending = (v);                                                                                         \
        u->pReady = u->cycle + (lat);                                                                              \
    } while (0)

int Vu_RunRef(VuCore *u, uint32_t start) {
    int ending = 0, branch = 0;
    uint32_t target = 0, mm = u->microMask, pc = start & mm, guard, c;

    for (guard = 0; guard < (1u << 22); guard++) {
        uint32_t lo, hi, dest, ft, fs, fd, op, bc, next = (pc + 1) & mm;
        uint32_t res[4], wmask = 0, *wr = NULL, bcv = 0;
        int wacc = 0, lower = 1, fmac = 0;

        memcpy(&lo, u->micro + pc * 8, 4);
        memcpy(&hi, u->micro + pc * 8 + 4, 4);
        u->cycle++;
        u->instr++;
        if (u->qReady != 0 && u->cycle >= u->qReady) {
            *u->q = u->qPending;
            u->qReady = 0;
        }
        if (u->pReady != 0 && u->cycle >= u->pReady) {
            u->p = u->pPending;
            u->pReady = 0;
        }
        while (u->clipCount != 0 && u->clipQueue[u->clipHead].cycle <= u->cycle) {
            u->clip = u->clipQueue[u->clipHead].value;
            u->clipHead = (u->clipHead + 1) & 7;
            u->clipCount--;
        }
        while (u->macCount != 0 && u->macQueue[u->macHead].cycle <= u->cycle) {
            u->macNow = u->macQueue[u->macHead];
            u->macLazy = 1;
            u->macHead = (u->macHead + 1) & 7;
            u->macCount--;
        }
        dest = (hi >> 21) & 15;
        ft = (hi >> 16) & 31;
        fs = (hi >> 11) & 31;
        fd = (hi >> 6) & 31;
        op = hi & 0x3F;
        bc = hi & 3;
        bcv = VF(ft)[bc];
        if (hi & 0x80000000u) { /* I bit: the lower word is the I register's value */
            u->i = lo;
            lower = 0;
        }
#define A(c) VF(fs)[c]
#define B(c) VF(ft)[c]
#define ACC(c) u->acc[c]
#define EACH for (c = 0; c < 4; c++) if (DEST(c))
#define TO_FD wr = VF(fd); wmask = dest; fmac = 1
#define TO_ACC wacc = 1; wmask = dest; fmac = 1
        /* ---- upper instruction: result into res, committed after the lower one has read its operands ---- */
        if ((hi & 0x07FFFFFFu) == 0x000002FFu) {
            goto upper_done; /* NOP */
        }
        memcpy(res, op >= 0x3C ? u->acc : VF(fd), 16);
        if (op < 0x3C) {
            switch (op) {
            case 0x00: case 0x01: case 0x02: case 0x03: EACH res[c] = vadd(A(c), bcv); TO_FD; break;
            case 0x04: case 0x05: case 0x06: case 0x07: EACH res[c] = vsub(A(c), bcv); TO_FD; break;
            case 0x08: case 0x09: case 0x0A: case 0x0B: EACH res[c] = vadd(ACC(c), vmul(A(c), bcv)); TO_FD; break;
            case 0x0C: case 0x0D: case 0x0E: case 0x0F: EACH res[c] = vsub(ACC(c), vmul(A(c), bcv)); TO_FD; break;
            case 0x10: case 0x11: case 0x12: case 0x13: EACH res[c] = vmax(A(c), bcv); wr = VF(fd); wmask = dest; break;
            case 0x14: case 0x15: case 0x16: case 0x17: EACH res[c] = vmin(A(c), bcv); wr = VF(fd); wmask = dest; break;
            case 0x18: case 0x19: case 0x1A: case 0x1B: EACH res[c] = vmul(A(c), bcv); TO_FD; break;
            case 0x1C: EACH res[c] = vmul(A(c), *u->q); TO_FD; break;
            case 0x1D: EACH res[c] = vmax(A(c), u->i); wr = VF(fd); wmask = dest; break;
            case 0x1E: EACH res[c] = vmul(A(c), u->i); TO_FD; break;
            case 0x1F: EACH res[c] = vmin(A(c), u->i); wr = VF(fd); wmask = dest; break;
            case 0x20: EACH res[c] = vadd(A(c), *u->q); TO_FD; break;
            case 0x21: EACH res[c] = vadd(ACC(c), vmul(A(c), *u->q)); TO_FD; break;
            case 0x22: EACH res[c] = vadd(A(c), u->i); TO_FD; break;
            case 0x23: EACH res[c] = vadd(ACC(c), vmul(A(c), u->i)); TO_FD; break;
            case 0x24: EACH res[c] = vsub(A(c), *u->q); TO_FD; break;
            case 0x25: EACH res[c] = vsub(ACC(c), vmul(A(c), *u->q)); TO_FD; break;
            case 0x26: EACH res[c] = vsub(A(c), u->i); TO_FD; break;
            case 0x27: EACH res[c] = vsub(ACC(c), vmul(A(c), u->i)); TO_FD; break;
            case 0x28: EACH res[c] = vadd(A(c), B(c)); TO_FD; break;
            case 0x29: EACH res[c] = vadd(ACC(c), vmul(A(c), B(c))); TO_FD; break;
            case 0x2A: EACH res[c] = vmul(A(c), B(c)); TO_FD; break;
            case 0x2B: EACH res[c] = vmax(A(c), B(c)); wr = VF(fd); wmask = dest; break;
            case 0x2C: EACH res[c] = vsub(A(c), B(c)); TO_FD; break;
            case 0x2D: EACH res[c] = vsub(ACC(c), vmul(A(c), B(c))); TO_FD; break;
            case 0x2E: /* OPMSUB */
                res[0] = vsub(ACC(0), vmul(A(1), B(2)));
                res[1] = vsub(ACC(1), vmul(A(2), B(0)));
                res[2] = vsub(ACC(2), vmul(A(0), B(1)));
                wr = VF(fd); wmask = 0xE; fmac = 1;
                break;
            case 0x2F: EACH res[c] = vmin(A(c), B(c)); wr = VF(fd); wmask = dest; break;
            default: u->unknown[0][op]++; break;
            }
        } else {
            uint32_t sp = (fd << 2) | bc;
            switch (sp) {
            case 0x00: case 0x01: case 0x02: case 0x03: EACH res[c] = vadd(A(c), bcv); TO_ACC; break;
            case 0x04: case 0x05: case 0x06: case 0x07: EACH res[c] = vsub(A(c), bcv); TO_ACC; break;
            case 0x08: case 0x09: case 0x0A: case 0x0B: EACH res[c] = vadd(ACC(c), vmul(A(c), bcv)); TO_ACC; break;
            case 0x0C: case 0x0D: case 0x0E: case 0x0F: EACH res[c] = vsub(ACC(c), vmul(A(c), bcv)); TO_ACC; break;
            case 0x10: case 0x11: case 0x12: case 0x13: /* ITOF0 / 4 / 12 / 15 */
                memcpy(res, VF(ft), 16);
                EACH res[c] = itof(A(c), sp == 0x10 ? 0 : sp == 0x11 ? 4 : sp == 0x12 ? 12 : 15);
                wr = VF(ft); wmask = dest;
                break;
            case 0x14: case 0x15: case 0x16: case 0x17: /* FTOI0 / 4 / 12 / 15 */
                memcpy(res, VF(ft), 16);
                EACH res[c] = ftoi(A(c), sp == 0x14 ? 0 : sp == 0x15 ? 4 : sp == 0x16 ? 12 : 15);
                wr = VF(ft); wmask = dest;
                break;
            case 0x18: case 0x19: case 0x1A: case 0x1B: EACH res[c] = vmul(A(c), bcv); TO_ACC; break;
            case 0x1C: EACH res[c] = vmul(A(c), *u->q); TO_ACC; break;
            case 0x1D: memcpy(res, VF(ft), 16); EACH res[c] = A(c) & 0x7FFFFFFFu; wr = VF(ft); wmask = dest; break;
            case 0x1E: EACH res[c] = vmul(A(c), u->i); TO_ACC; break;
            case 0x1F: { /* CLIP: fs.xyz against +/-|ft.w| */
                int32_t w = sf_key(B(3) & 0x7FFFFFFFu);
                uint32_t f = (sf_key(A(0)) > w) | (sf_key(A(0)) < -w) << 1 | (sf_key(A(1)) > w) << 2 |
                             (sf_key(A(1)) < -w) << 3 | (sf_key(A(2)) > w) << 4 | (sf_key(A(2)) < -w) << 5;
                u->clipLatest = ((u->clipLatest << 6) | f) & 0xFFFFFF;
                queue(u->clipQueue, &u->clipHead, &u->clipCount, u->cycle, u->clipLatest);
                break;
            }
            case 0x20: EACH res[c] = vadd(A(c), *u->q); TO_ACC; break;
            case 0x21: EACH res[c] = vadd(ACC(c), vmul(A(c), *u->q)); TO_ACC; break;
            case 0x22: EACH res[c] = vadd(A(c), u->i); TO_ACC; break;
            case 0x23: EACH res[c] = vadd(ACC(c), vmul(A(c), u->i)); TO_ACC; break;
            case 0x24: EACH res[c] = vsub(A(c), *u->q); TO_ACC; break;
            case 0x25: EACH res[c] = vsub(ACC(c), vmul(A(c), *u->q)); TO_ACC; break;
            case 0x26: EACH res[c] = vsub(A(c), u->i); TO_ACC; break;
            case 0x27: EACH res[c] = vsub(ACC(c), vmul(A(c), u->i)); TO_ACC; break;
            case 0x28: EACH res[c] = vadd(A(c), B(c)); TO_ACC; break;
            case 0x29: EACH res[c] = vadd(ACC(c), vmul(A(c), B(c))); TO_ACC; break;
            case 0x2A: EACH res[c] = vmul(A(c), B(c)); TO_ACC; break;
            case 0x2C: EACH res[c] = vsub(A(c), B(c)); TO_ACC; break;
            case 0x2D: EACH res[c] = vsub(ACC(c), vmul(A(c), B(c))); TO_ACC; break;
            case 0x2E: /* OPMULA */
                res[0] = vmul(A(1), B(2));
                res[1] = vmul(A(2), B(0));
                res[2] = vmul(A(0), B(1));
                wacc = 1; wmask = 0xE; fmac = 1;
                break;
            case 0x2F: break; /* NOP */
            default: u->unknown[0][64 + (sp & 63)]++; break;
            }
        }
        if (fmac) { /* the MAC flags of this result, four instructions later (as queue()) */
            VuMacStep *st = &u->macQueue[(u->macHead + u->macCount) & 7];
            st->cycle = u->cycle + 4;
            st->mask = wmask;
            memcpy(st->res, res, 16);
            if (u->macCount < 8) {
                u->macCount++;
            } else {
                u->macHead = (u->macHead + 1) & 7;
            }
        }
upper_done:
        /* ---- lower instruction ---- */
        if (lower && lo != 0x8000033Cu) { /* 0x8000033C: the lower NOP (move vf0, vf0) */
            uint32_t lop = lo >> 25, it = (lo >> 16) & 31, is = (lo >> 11) & 31, id = (lo >> 6) & 31, ld = (lo >> 21) & 15;
            int32_t imm11 = (int32_t)(lo << 21) >> 21;
            uint32_t imm15 = ((lo >> 10) & 0x7800) | (lo & 0x7FF);
            uint32_t boff = (pc + 1 + (uint32_t)imm11) & mm;

            switch (lop) {
            case 0x00: { /* LQ */
                const uint8_t *m = MEMQ((int32_t)VI(is) + imm11);
                for (c = 0; c < 4; c++) if (LDEST(c) && it) memcpy(&VF(it)[c], m + c * 4, 4);
                break;
            }
            case 0x01: { /* SQ */
                uint8_t *m = MEMQ((int32_t)VI(it) + imm11);
                for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &VF(is)[c], 4);
                break;
            }
            case 0x04: { /* ILW */
                const uint8_t *m = MEMQ((int32_t)VI(is) + imm11);
                for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w; memcpy(&w, m + c * 4, 4); SETVI(it, w); }
                break;
            }
            case 0x05: { /* ISW */
                uint8_t *m = MEMQ((int32_t)VI(is) + imm11);
                for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w = VI(it); memcpy(m + c * 4, &w, 4); }
                break;
            }
            case 0x08: SETVI(it, VI(is) + imm15); break; /* IADDIU */
            case 0x09: SETVI(it, VI(is) - imm15); break; /* ISUBIU */
            case 0x10: SETVI(1, (u->clip & 0xFFFFFF) == (lo & 0xFFFFFF)); break; /* FCEQ */
            case 0x11: u->clip = u->clipLatest = lo & 0xFFFFFF; u->clipCount = 0; break; /* FCSET */
            case 0x12: SETVI(1, (u->clip & lo & 0xFFFFFF) != 0); break; /* FCAND */
            case 0x13: SETVI(1, ((u->clip | lo) & 0xFFFFFF) == 0xFFFFFF); break; /* FCOR */
            case 0x18: SETVI(it, (mac_now(u) & 0xFFFF) == (VI(is) & 0xFFFF)); break; /* FMEQ */
            case 0x1A: SETVI(it, mac_now(u) & VI(is)); break; /* FMAND */
            case 0x1B: SETVI(it, mac_now(u) | VI(is)); break; /* FMOR */
            case 0x1C: SETVI(it, u->clip & 0xFFF); break; /* FCGET */
            case 0x20: branch = 2; target = boff; break; /* B */
            case 0x21: SETVI(it, (pc + 2) & mm); branch = 2; target = boff; break; /* BAL */
            case 0x24: branch = 2; target = VI(is) & mm; break; /* JR: the register holds an instruction number */
            case 0x25: { uint32_t t = VI(is) & mm; SETVI(it, (pc + 2) & mm); branch = 2; target = t; break; } /* JALR */
            case 0x28: if (VI(it) == VI(is)) { branch = 2; target = boff; } break; /* IBEQ */
            case 0x29: if (VI(it) != VI(is)) { branch = 2; target = boff; } break; /* IBNE */
            case 0x2C: if ((int16_t)VI(is) < 0) { branch = 2; target = boff; } break; /* IBLTZ */
            case 0x2D: if ((int16_t)VI(is) > 0) { branch = 2; target = boff; } break; /* IBGTZ */
            case 0x2E: if ((int16_t)VI(is) <= 0) { branch = 2; target = boff; } break; /* IBLEZ */
            case 0x2F: if ((int16_t)VI(is) >= 0) { branch = 2; target = boff; } break; /* IBGEZ */
            case 0x40: {
                uint32_t f = lo & 0x3F;
                if (f < 0x3C) {
                    switch (f) {
                    case 0x30: SETVI(id, VI(is) + VI(it)); break; /* IADD */
                    case 0x31: SETVI(id, VI(is) - VI(it)); break; /* ISUB */
                    case 0x32: SETVI(it, VI(is) + (uint32_t)((int32_t)(lo << 21) >> 27)); break; /* IADDI */
                    case 0x34: SETVI(id, VI(is) & VI(it)); break; /* IAND */
                    case 0x35: SETVI(id, VI(is) | VI(it)); break; /* IOR */
                    default: u->unknown[1][f]++; break;
                    }
                } else {
                    uint32_t sp = (id << 2) | (lo & 3), fsf = (lo >> 21) & 3, ftf = (lo >> 23) & 3;
                    switch (sp) {
                    case 0x30: if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) VF(it)[c] = VF(is)[c]; } break; /* MOVE */
                    case 0x31: if (it) { uint32_t s[4]; memcpy(s, VF(is), 16); /* MR32 */
                                         for (c = 0; c < 4; c++) if (LDEST(c)) VF(it)[c] = s[(c + 1) & 3]; } break;
                    case 0x34: { const uint8_t *m = MEMQ(VI(is)); /* LQI */
                                 if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(&VF(it)[c], m + c * 4, 4); }
                                 SETVI(is, VI(is) + 1); break; }
                    case 0x35: { uint8_t *m = MEMQ(VI(it)); /* SQI */
                                 for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &VF(is)[c], 4);
                                 SETVI(it, VI(it) + 1); break; }
                    case 0x36: { const uint8_t *m; SETVI(is, VI(is) - 1); m = MEMQ(VI(is)); /* LQD */
                                 if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(&VF(it)[c], m + c * 4, 4); } break; }
                    case 0x37: { uint8_t *m; SETVI(it, VI(it) - 1); m = MEMQ(VI(it)); /* SQD */
                                 for (c = 0; c < 4; c++) if (LDEST(c)) memcpy(m + c * 4, &VF(is)[c], 4); break; }
                    case 0x38: u->qPending = vdiv(VF(is)[fsf], VF(it)[ftf]); u->qReady = u->cycle + 7; break; /* DIV */
                    case 0x39: u->qPending = vsqrt(VF(it)[ftf]); u->qReady = u->cycle + 7; break; /* SQRT */
                    case 0x3A: u->qPending = vdiv(VF(is)[fsf], vsqrt(VF(it)[ftf])); /* RSQRT */
                               u->qReady = u->cycle + 13; break;
                    case 0x3B: if (u->qReady != 0) { *u->q = u->qPending; u->qReady = 0; } break; /* WAITQ */
                    case 0x3C: SETVI(it, VF(is)[fsf]); break; /* MTIR */
                    case 0x3D: if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) VF(it)[c] = (uint32_t)(int32_t)(int16_t)VI(is); } break; /* MFIR */
                    case 0x3E: { const uint8_t *m = MEMQ(VI(is)); /* ILWR */
                                 for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w; memcpy(&w, m + c * 4, 4); SETVI(it, w); } break; }
                    case 0x3F: { uint8_t *m = MEMQ(VI(is)); /* ISWR */
                                 for (c = 0; c < 4; c++) if (LDEST(c)) { uint32_t w = VI(it); memcpy(m + c * 4, &w, 4); } break; }
                    case 0x64: if (it) { for (c = 0; c < 4; c++) if (LDEST(c)) VF(it)[c] = u->p; } break; /* MFP */
                    case 0x68: if (u->vu1) { SETVI(it, u->top); } else { u->unknown[1][64 + (sp & 63)]++; } break; /* XTOP */
                    case 0x69: if (u->vu1) { SETVI(it, u->itop); } else { u->unknown[1][64 + (sp & 63)]++; } break; /* XITOP */
                    case 0x6C: if (u->kick != NULL) { u->kick(VI(is)); } else { u->unknown[1][64 + (sp & 63)]++; } break; /* XGKICK */
                    case 0x70: { const uint32_t *v = VF(is); /* ESADD */
                                 PSET(vadd(vadd(vmul(v[0], v[0]), vmul(v[1], v[1])), vmul(v[2], v[2])), 11); break; }
                    case 0x71: { const uint32_t *v = VF(is); /* ERSADD */
                                 PSET(vdiv(0x3F800000u, vadd(vadd(vmul(v[0], v[0]), vmul(v[1], v[1])), vmul(v[2], v[2]))), 18); break; }
                    case 0x72: { const uint32_t *v = VF(is); /* ELENG */
                                 PSET(vsqrt(vadd(vadd(vmul(v[0], v[0]), vmul(v[1], v[1])), vmul(v[2], v[2]))), 18); break; }
                    case 0x73: { const uint32_t *v = VF(is); /* ERLENG */
                                 PSET(vdiv(0x3F800000u, vsqrt(vadd(vadd(vmul(v[0], v[0]), vmul(v[1], v[1])), vmul(v[2], v[2])))), 24); break; }
                    case 0x74: case 0x75: { const uint32_t *v = VF(is); /* EATANxy / EATANxz: atan(y / x), atan(z / x) */
                                 uint32_t x = v[0], y = v[sp == 0x74 ? 1 : 2];
                                 PSET(eatan_t(vdiv(vsub(y, x), vadd(y, x))), 54); break; }
                    case 0x76: { const uint32_t *v = VF(is); /* ESUM */
                                 PSET(vadd(vadd(vadd(v[0], v[1]), v[2]), v[3]), 12); break; }
                    case 0x78: PSET(vsqrt(VF(is)[fsf]), 12); break; /* ESQRT */
                    case 0x79: PSET(vdiv(0x3F800000u, vsqrt(VF(is)[fsf])), 18); break; /* ERSQRT */
                    case 0x7A: PSET(vdiv(0x3F800000u, VF(is)[fsf]), 12); break; /* ERCPR */
                    case 0x7B: if (u->pReady != 0) { u->p = u->pPending; u->pReady = 0; } break; /* WAITP */
                    case 0x7D: { uint32_t x = VF(is)[fsf]; /* EATAN */
                                 PSET(eatan_t(vdiv(vsub(x, 0x3F800000u), vadd(x, 0x3F800000u))), 54); break; }
                    default: u->unknown[1][64 + (sp & 63)]++; break;
                    }
                }
                break;
            }
            default: u->unknown[1][lop & 63]++; break;
            }
        }
        /* ---- commit the upper instruction's result ---- */
        if (wr != NULL && wr != VF(0)) {
            for (c = 0; c < 4; c++) if ((wmask >> (3 - c)) & 1) wr[c] = res[c];
        } else if (wacc) {
            for (c = 0; c < 4; c++) if ((wmask >> (3 - c)) & 1) u->acc[c] = res[c];
        }
        VF(0)[0] = VF(0)[1] = VF(0)[2] = 0;
        VF(0)[3] = 0x3F800000u;
        u->vi[0] = 0;
        /* ---- next instruction: branches take effect after one more instruction; so does the end ---- */
        if (ending) {
            u->pc = next;
            if (u->settle) {
                settle(u);
            }
            return 0;
        }
        if (hi & 0x40000000u) {
            ending = 1;
        }
        if (branch == 1) {
            pc = target;
            branch = 0;
        } else {
            if (branch == 2) {
                branch = 1;
            }
            pc = next;
        }
    }
    u->pc = pc;
    return -1;
}
/* ---------------------------------------------------------------------------------------------- checking */

/* The same machine state: registers, data memory, the pipeline (what is on its way and when it arrives, not where
   in its ring it sits), counters. The MAC flags are compared by value (worked out on both if still lazy). */
int Vu_SameState(VuCore *a, VuCore *b) {
    int k, c;

    if (memcmp(a->vf, b->vf, 32 * 16) != 0 || memcmp(a->vi, b->vi, 16 * 2) != 0 || memcmp(a->acc, b->acc, 16) != 0 ||
        *a->q != *b->q || memcmp(a->data, b->data, (a->dataMask + 1) * 16) != 0) {
        return 0;
    }
    if (a->i != b->i || a->p != b->p || a->pc != b->pc || a->cycle != b->cycle || a->instr != b->instr ||
        a->qReady != b->qReady || (a->qReady != 0 && a->qPending != b->qPending) || a->pReady != b->pReady ||
        (a->pReady != 0 && a->pPending != b->pPending) || a->clip != b->clip || a->clipLatest != b->clipLatest ||
        mac_now(a) != mac_now(b) || a->clipCount != b->clipCount || a->macCount != b->macCount ||
        memcmp(a->unknown, b->unknown, sizeof a->unknown) != 0) {
        return 0;
    }
    for (k = 0; k < a->clipCount; k++) {
        const VuFlagStep *x = &a->clipQueue[(a->clipHead + k) & 7], *y = &b->clipQueue[(b->clipHead + k) & 7];
        if (x->cycle != y->cycle || x->value != y->value) {
            return 0;
        }
    }
    for (k = 0; k < a->macCount; k++) {
        const VuMacStep *x = &a->macQueue[(a->macHead + k) & 7], *y = &b->macQueue[(b->macHead + k) & 7];
        if (x->cycle != y->cycle || x->mask != y->mask) {
            return 0;
        }
        for (c = 0; c < 4; c++) {
            if (((x->mask >> (3 - c)) & 1) && x->res[c] != y->res[c]) {
                return 0;
            }
        }
    }
    return 1;
}
