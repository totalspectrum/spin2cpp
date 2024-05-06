//
// Peephole optimizer for asm IR
//
// Copyright 2016-2024 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "spinc.h"
#include "outasm.h"

/////////////////////////////////////////////////////
// new peephole optimization scheme
/////////////////////////////////////////////////////
typedef struct PeepholePattern {
    int cond;
    int opc;
    int dst;
    int src1;
    unsigned flags;
} PeepholePattern;

#define PEEP_FLAGS_NONE 0x0000
#define PEEP_FLAGS_P2   0x0001
#define PEEP_FLAGS_WCZ_OK 0x0002
#define PEEP_FLAGS_MUST_WZ 0x0004
#define PEEP_FLAGS_MUST_WC 0x0008
#define PEEP_FLAGS_DONE 0xffff

#define OPERAND_ANY -1
#define COND_ANY    -1
#define OPC_ANY     -1

#define MAX_OPERANDS_IN_PATTERN 16

#define PEEP_OPNUM_MASK 0x00ffffff
#define PEEP_OP_MASK    0xff000000
#define PEEP_OP_SET     0x01000000
#define PEEP_OP_MATCH   0x02000000
#define PEEP_OP_IMM     0x03000000
#define PEEP_OP_SET_IMM 0x04000000
#define PEEP_OP_MATCH_DEAD 0x05000000  /* like PEEP_OP_MATCH, but operand is dead after this instr */
#define PEEP_OP_CLRBITS 0x06000000
#define PEEP_OP_MATCH_M1S 0x07000000
#define PEEP_OP_MATCH_M1U 0x08000000

#define PEEP_OP_CLRMASK(bits, shift) (PEEP_OP_CLRBITS|(bits<<6)|(shift))

static Operand *peep_ops[MAX_OPERANDS_IN_PATTERN];

static int PeepOperandMatch(int patrn_dst, Operand *dst, IR *ir)
{
    int opnum, opflag;

    if (patrn_dst != OPERAND_ANY) {
        opnum = patrn_dst & PEEP_OPNUM_MASK;
        opflag = patrn_dst & PEEP_OP_MASK;
        if (opflag == PEEP_OP_SET) {
            peep_ops[opnum] = dst;
        } else if (opflag == PEEP_OP_SET_IMM) {
            if (dst->kind != IMM_INT) {
                return 0;
            }
            peep_ops[opnum] = dst;
        } else if (opflag == PEEP_OP_MATCH) {
            if (!SameOperand(peep_ops[opnum], dst)) {
                return 0;
            }
        } else if (opflag == PEEP_OP_MATCH_DEAD) {
            if (!SameOperand(peep_ops[opnum], dst)) {
                return 0;
            }
            if (!IsDeadAfter(ir, dst)) {
                return 0;
            }
        } else if (opflag == PEEP_OP_MATCH_M1U) {
            // the new operand must have the same value as the original, minus 1
            // (used for compares)
            if (dst->kind != IMM_INT) return 0;
            if (peep_ops[opnum]->kind != IMM_INT) return 0;
            if ( (uint32_t)(peep_ops[opnum]->val) != 1+((uint32_t)dst->val) ) {
                return 0;
            }
            // disallow overflow case
            if ((uint32_t)dst->val == 0xFFFFFFFFU) {
                return 0;
            }
        } else if (opflag == PEEP_OP_MATCH_M1S) {
            // the new operand must have the same value as the original, minus 1
            // (signed version of above)
            if (dst->kind != IMM_INT) return 0;
            if (peep_ops[opnum]->kind != IMM_INT) return 0;
            if ( (int32_t)(peep_ops[opnum]->val) != 1+((int32_t)dst->val) ) {
                return 0;
            }
            // disallow overflow case
            if ((uint32_t)dst->val == 0x7fffffffU) {
                return 0;
            }
        } else if (opflag == PEEP_OP_IMM) {
            if (dst->kind != IMM_INT) {
                return 0;
            }
            if (dst->val != opnum) {
                return 0;
            }
        } else if (opflag == PEEP_OP_CLRBITS) {
            uint32_t mask, shift;
            if (dst->kind != IMM_INT) {
                return 0;
            }
            mask = (opnum>>6) & 0x3f;
            shift = (opnum & 0x3f);
            mask = ((1<<mask)-1)<<shift;
            mask = ~mask;
            if ( ((uint32_t)dst->val) != mask) {
                return 0;
            }
        }
    }
    return 1;
}

static int MatchPattern(PeepholePattern *patrn, IR *ir)
{
    int ircount = 0;
    unsigned flags;
    int cur_cond = COND_ANY;
    for(;;) {
        flags = patrn->flags;
        if (flags == PEEP_FLAGS_DONE) {
            break; // we've matched so far
        }
        if ( (flags & PEEP_FLAGS_P2) && !gl_p2 ) {
            return 0;
        }
        if (!ir) {
            return 0; // no match, ran out of instructions
        }
        if (patrn->cond == COND_ANY) {
            if (cur_cond == COND_ANY) {
                cur_cond = ir->cond;
            }
            if (cur_cond != ir->cond) {
                return 0;
            }
        }
        else if ((ir->cond != patrn->cond)) {
            return 0;
        }
        if (patrn->opc != ir->opc && patrn->opc != OPC_ANY) {
            return 0;
        }
        // not all patterns are compatible with wcz bits
        if (ir->flags & 0xff) {
            if (!(flags & PEEP_FLAGS_WCZ_OK)) {
                return 0;
            }
        }
        if ((flags & PEEP_FLAGS_MUST_WC) && !(ir->flags & FLAG_WC) && !(ir->flags & FLAG_WCZ)) return 0;
        if ((flags & PEEP_FLAGS_MUST_WZ) && !(ir->flags & FLAG_WZ) && !(ir->flags & FLAG_WCZ)) return 0;

        if (!PeepOperandMatch(patrn->dst, ir->dst, ir)) {
            return 0;
        }
        if (!PeepOperandMatch(patrn->src1, ir->src, ir)) {
            return 0;
        }
        patrn++;
        ir = NextIR(ir); //ir->next;
        ircount++;
    }
    return ircount;
}

// cmp + mov -> max/min
static PeepholePattern pat_maxs[] = {
    { COND_TRUE, OPC_CMPS, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_WCZ_OK },
    { COND_GT, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_maxu[] = {
    { COND_TRUE, OPC_CMP, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_WCZ_OK },
    { COND_GT, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_mins[] = {
    { COND_TRUE, OPC_CMPS, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_WCZ_OK },
    { COND_LT, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_minu[] = {
    { COND_TRUE, OPC_CMP, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_WCZ_OK },
    { COND_LT, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

static PeepholePattern pat_maxs_off[] = {
    { COND_TRUE, OPC_CMPS, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_WCZ_OK },
    { COND_GE, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH_M1S|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_maxu_off[] = {
    { COND_TRUE, OPC_CMP, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_WCZ_OK },
    { COND_GE, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH_M1U|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// zero/sign extend via x = (x<<N) >> N
static PeepholePattern pat_zeroex[] = {
    { COND_ANY, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_SET_IMM|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_SHR, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_signex[] = {
    { COND_ANY, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_SET_IMM|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_SAR, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// wrc x; cmp x, #0 wz
static PeepholePattern pat_wrc_cmp[] = {
    { COND_TRUE, OPC_WRC, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_CMP, PEEP_OP_MATCH|0, PEEP_OP_IMM|0, PEEP_FLAGS_P2|PEEP_FLAGS_WCZ_OK|PEEP_FLAGS_MUST_WZ },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// muxc x,#-1; cmp x, #0 wz
static PeepholePattern pat_muxc_cmp[] = {
    { COND_TRUE, OPC_MUXC, PEEP_OP_SET|0, PEEP_OP_CLRMASK(0,0), PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_CMP, PEEP_OP_MATCH|0, PEEP_OP_IMM|0, PEEP_FLAGS_WCZ_OK|PEEP_FLAGS_MUST_WZ },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// subx x,x; cmp x, #0 wz
static PeepholePattern pat_subx_cmp[] = {
    { COND_TRUE, OPC_SUBX, PEEP_OP_SET|0, PEEP_OP_MATCH|0, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_CMP, PEEP_OP_MATCH|0, PEEP_OP_IMM|0, PEEP_FLAGS_WCZ_OK|PEEP_FLAGS_MUST_WZ },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// muxc x,#-1 wz
static PeepholePattern pat_muxc_wz[] = {
    { COND_TRUE, OPC_MUXC, PEEP_OP_SET|0, PEEP_OP_CLRMASK(0,0), PEEP_FLAGS_WCZ_OK|PEEP_FLAGS_MUST_WZ },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// wrc x; and x, #1 -> the AND is redundant
static PeepholePattern pat_wrc_and[] = {
    { COND_TRUE, OPC_WRC, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|1, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// wrc x; test x, #1 wc -> the TEST is redundant
static PeepholePattern pat_wrc_test[] = {
    { COND_TRUE, OPC_WRC, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_TEST, PEEP_OP_MATCH|0, PEEP_OP_IMM|1, PEEP_FLAGS_P2 | PEEP_FLAGS_WCZ_OK },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

static PeepholePattern pat_clrc[] = {
    { COND_ANY, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_IMM|0, PEEP_FLAGS_NONE },
    { COND_ANY, OPC_TEST, PEEP_OP_MATCH_DEAD|0, PEEP_OP_IMM|1, PEEP_FLAGS_WCZ_OK },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_setc1[] = {
    { COND_ANY, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_IMM|1, PEEP_FLAGS_NONE },
    { COND_ANY, OPC_TEST, PEEP_OP_MATCH_DEAD|0, PEEP_OP_IMM|1, PEEP_FLAGS_WCZ_OK },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_setc2[] = {
    { COND_ANY, OPC_NEG, PEEP_OP_SET|0, PEEP_OP_IMM|1, PEEP_FLAGS_NONE },
    { COND_ANY, OPC_TEST, PEEP_OP_MATCH_DEAD|0, PEEP_OP_IMM|1, PEEP_FLAGS_WCZ_OK },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// XOR x,##-1 to NOT x,x
static PeepholePattern pat_not[] = {
    { COND_ANY, OPC_XOR, PEEP_OP_SET|0, PEEP_OP_CLRMASK(0,0), PEEP_FLAGS_P2},
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// cmps x,#0 wc; if_c neg x,x -> abs x,x wc
// (wz is also allowed)
static PeepholePattern pat_cmps_abs[] = {
    { COND_ANY, OPC_CMPS, PEEP_OP_SET|0, PEEP_OP_IMM|0, PEEP_FLAGS_WCZ_OK | PEEP_FLAGS_MUST_WC },
    { COND_C, OPC_NEG, PEEP_OP_MATCH|0, PEEP_OP_MATCH|0, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// remove redundant zero extends after rdbyte/rdword
static PeepholePattern pat_rdbyte1[] = {
    { COND_TRUE, OPC_RDBYTE, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_SHL, PEEP_OP_MATCH|0, PEEP_OP_IMM|24, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_SHR, PEEP_OP_MATCH|0, PEEP_OP_IMM|24, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_rdword1[] = {
    { COND_TRUE, OPC_RDWORD, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_SHL, PEEP_OP_MATCH|0, PEEP_OP_IMM|16, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_SHR, PEEP_OP_MATCH|0, PEEP_OP_IMM|16, PEEP_FLAGS_WCZ_OK },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_rdbyte2[] = {
    { COND_TRUE, OPC_RDBYTE, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_ZEROX, PEEP_OP_MATCH|0, PEEP_OP_IMM|7, PEEP_FLAGS_P2 | PEEP_FLAGS_WCZ_OK },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_rdword2[] = {
    { COND_TRUE, OPC_RDWORD, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_ZEROX, PEEP_OP_MATCH|0, PEEP_OP_IMM|15, PEEP_FLAGS_P2 | PEEP_FLAGS_WCZ_OK},
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// potentially eliminate a redundant mov+add sequence
static PeepholePattern pat_movadd[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_ADD, PEEP_OP_SET|2, PEEP_OP_MATCH|0, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// replace mov x, #2; shl x, y; sub x, #1  with bmask x, y
static PeepholePattern pat_bmask1[] = {
    { COND_ANY, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_IMM|2, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_SHL, PEEP_OP_MATCH|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_SUB, PEEP_OP_MATCH|0, PEEP_OP_IMM|1, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// replace add y, #1; decod x, y; sub x, #1  with bmask x, y
static PeepholePattern pat_bmask2[] = {
    { COND_ANY, OPC_ADD, PEEP_OP_SET|1, PEEP_OP_IMM|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_DECOD, PEEP_OP_SET|0, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_SUB, PEEP_OP_MATCH|0, PEEP_OP_IMM|1, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// replace mov a, x; waitx a  with waitx x
static PeepholePattern pat_waitx[] = {
    { COND_ANY, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_WAITX, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// replace if_c drvh / if_nc drvl with drvc
static PeepholePattern pat_drvc1[] = {
    { COND_C,  OPC_DRVH, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_NC, OPC_DRVL, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_drvc2[] = {
    { COND_NC, OPC_DRVL, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_C,  OPC_DRVH, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// replace if_c drvl / if_nc drvh with drvnc
static PeepholePattern pat_drvnc1[] = {
    { COND_C,  OPC_DRVL, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_NC, OPC_DRVH, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_drvnc2[] = {
    { COND_NC, OPC_DRVH, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_C,  OPC_DRVL, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// replace if_c bith / if_nc bitl with bitc
static PeepholePattern pat_bitc1[] = {
    { COND_C,  OPC_BITH, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_NC, OPC_BITL, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_bitc2[] = {
    { COND_NC, OPC_BITL, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_C,  OPC_BITH, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// replace if_c bitl / if_nc bith with bitnc
static PeepholePattern pat_bitnc1[] = {
    { COND_C,  OPC_BITL, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_NC, OPC_BITH, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_bitnc2[] = {
    { COND_NC, OPC_BITH, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_C,  OPC_BITL, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// replace if_z drvh / if_nz drvl with drvz
static PeepholePattern pat_drvz[] = {
    { COND_EQ, OPC_DRVH, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_NE, OPC_DRVL, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_drvnz1[] = {
    { COND_NE, OPC_DRVH, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_EQ, OPC_DRVL, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_drvnz2[] = {
    { COND_EQ, OPC_DRVL, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_NE, OPC_DRVH, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// replace if_c neg / if_nc mov and smiliar patterns with NEGcc
static PeepholePattern pat_negc1[] = {
    { COND_C,  OPC_NEG, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_NC, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_negc2[] = {
    { COND_NC, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_C,  OPC_NEG, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_negnc1[] = {
    { COND_NC, OPC_NEG, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_C,  OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_negnc2[] = {
    { COND_C,  OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_NC, OPC_NEG, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_negz1[] = {
    { COND_Z,  OPC_NEG, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_NZ, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_negz2[] = {
    { COND_NZ, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_Z,  OPC_NEG, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_negnz1[] = {
    { COND_NZ, OPC_NEG, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_Z,  OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_negnz2[] = {
    { COND_Z,  OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_NZ, OPC_NEG, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// replace if_c sub / if_nc add and smiliar patterns with SUMcc
static PeepholePattern pat_sumc1[] = {
    { COND_C,  OPC_SUB, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_NC, OPC_ADD, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sumc2[] = {
    { COND_NC, OPC_ADD, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_C,  OPC_SUB, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sumnc1[] = {
    { COND_NC, OPC_SUB, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_C,  OPC_ADD, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sumnc2[] = {
    { COND_C,  OPC_ADD, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_NC, OPC_SUB, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sumz1[] = {
    { COND_Z,  OPC_SUB, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_NZ, OPC_ADD, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sumz2[] = {
    { COND_NZ, OPC_ADD, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_Z,  OPC_SUB, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sumnz1[] = {
    { COND_NZ, OPC_SUB, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_Z,  OPC_ADD, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sumnz2[] = {
    { COND_Z,  OPC_ADD, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_NZ, OPC_SUB, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// replace mov x, #0 / cmp a, b wz / if_e mov x, #1
static PeepholePattern pat_seteq[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_IMM|0, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_CMP, OPERAND_ANY, OPERAND_ANY, PEEP_FLAGS_WCZ_OK },
    { COND_EQ,  OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_IMM|1, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_setne[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_IMM|0, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_CMP, OPERAND_ANY, OPERAND_ANY, PEEP_FLAGS_WCZ_OK },
    { COND_NE,  OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_IMM|1, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
#if 0
static PeepholePattern pat_sar24getbyte[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SAR, PEEP_OP_MATCH|0, PEEP_OP_IMM|24, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|255, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shr24getbyte[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SHR, PEEP_OP_MATCH|0, PEEP_OP_IMM|24, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|255, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sar16getbyte[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SAR, PEEP_OP_MATCH|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|255, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shr16getbyte[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SHR, PEEP_OP_MATCH|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|255, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sar8getbyte[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SAR, PEEP_OP_MATCH|0, PEEP_OP_IMM|8, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|255, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shr8getbyte[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SHR, PEEP_OP_MATCH|0, PEEP_OP_IMM|8, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|255, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_sar16getword[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SAR, PEEP_OP_MATCH|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_BITL, PEEP_OP_MATCH|0, PEEP_OP_IMM|(16+(15<<5)), PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shr16getword[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SAR, PEEP_OP_MATCH|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_BITL, PEEP_OP_MATCH|0, PEEP_OP_IMM|0xffff, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
#endif
static PeepholePattern pat_shl8setbyte[] = {
    { COND_TRUE, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_IMM|8, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_BITL, PEEP_OP_SET|1, PEEP_OP_IMM|(8+(7<<5)), PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_OR, PEEP_OP_MATCH|1, PEEP_OP_MATCH|0, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shl16setbyte[] = {
    { COND_TRUE, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_BITL, PEEP_OP_SET|1, PEEP_OP_IMM|(16+(7<<5)), PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_OR, PEEP_OP_MATCH|1, PEEP_OP_MATCH|0, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shl24setbyte[] = {
    { COND_TRUE, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_IMM|24, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_BITL, PEEP_OP_SET|1, PEEP_OP_IMM|(24+(7<<5)), PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_OR, PEEP_OP_MATCH|1, PEEP_OP_MATCH|0, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shl16setword[] = {
    { COND_TRUE, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_BITL, PEEP_OP_SET|1, PEEP_OP_IMM|(16+(15<<5)), PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_OR, PEEP_OP_MATCH|1, PEEP_OP_MATCH|0, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// mov x, y; and x, #1; add z, x => test y, #1 wz; if_nz add z, #1
static PeepholePattern pat_mov_and_add[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|1, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_ADD, PEEP_OP_SET|2, PEEP_OP_MATCH_DEAD|0, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// qmul x, y; getqx z; qmul x, y; getqy w -> qmul x, y; getqx z; getqy w
static PeepholePattern pat_qmul_qmul1[] = {
    { COND_TRUE, OPC_QMUL, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|2, OPERAND_ANY, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_QMUL, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
//    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|3, OPERAND_ANY, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// qmul x, y; getqy z; qmul x, y; getqx w -> qmul x, y; getqx z; getqy w
static PeepholePattern pat_qmul_qmul2[] = {
    { COND_TRUE, OPC_QMUL, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|2, OPERAND_ANY, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_QMUL, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
//    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|3, OPERAND_ANY, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// qdiv x, y; getqx z; qdiv x, y; getqy w -> qdiv x, y; getqx z; getqy w
static PeepholePattern pat_qdiv_qdiv1[] = {
    { COND_TRUE, OPC_QDIV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|2, OPERAND_ANY, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_QDIV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|3, OPERAND_ANY, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// qdiv x, y; getqx z; qdiv x, y; getqy w -> qdiv x, y; getqx z; getqy w
static PeepholePattern pat_qdiv_qdiv2[] = {
    { COND_TRUE, OPC_QDIV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|2, OPERAND_ANY, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_QDIV, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|3, OPERAND_ANY, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// Fuse signed quotient+remainder (constant divider)
static PeepholePattern pat_qdiv_qdiv_signed1[] = {
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|0,   PEEP_OP_SET|1,   PEEP_FLAGS_P2 | PEEP_FLAGS_MUST_WC | PEEP_FLAGS_WCZ_OK },
    { COND_TRUE, OPC_QDIV,  PEEP_OP_MATCH|0, PEEP_OP_SET|2,   PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|3,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_ANY,   OPERAND_ANY,     PEEP_OP_MATCH|3, PEEP_FLAGS_NONE }, // NEGC/NEGNC
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|4,   PEEP_OP_MATCH|1, PEEP_FLAGS_P2 | PEEP_FLAGS_MUST_WC | PEEP_FLAGS_WCZ_OK },
    { COND_TRUE, OPC_QDIV,  PEEP_OP_MATCH|4, PEEP_OP_MATCH|2, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|5,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    // NEGC/NEGNC here, but we don't really care if it's actually there
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// Fuse signed remainder+quotient (constant divider)
static PeepholePattern pat_qdiv_qdiv_signed2[] = {
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|0,   PEEP_OP_SET|1,   PEEP_FLAGS_P2 | PEEP_FLAGS_MUST_WC | PEEP_FLAGS_WCZ_OK },
    { COND_TRUE, OPC_QDIV,  PEEP_OP_MATCH|0, PEEP_OP_SET|2,   PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|3,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_ANY,   OPERAND_ANY,     PEEP_OP_MATCH|3, PEEP_FLAGS_NONE }, // NEGC/NEGNC
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|4,   PEEP_OP_MATCH|1, PEEP_FLAGS_P2 | PEEP_FLAGS_MUST_WC | PEEP_FLAGS_WCZ_OK },
    { COND_TRUE, OPC_QDIV,  PEEP_OP_MATCH|4, PEEP_OP_MATCH|2, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|5,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    // NEGC/NEGNC here, but we don't really care if it's actually there
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// Fuse signed quotient+remainder (constant dividend)
static PeepholePattern pat_qdiv_qdiv_signed3[] = {
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|0,   PEEP_OP_SET|1,   PEEP_FLAGS_P2 | PEEP_FLAGS_MUST_WC | PEEP_FLAGS_WCZ_OK },
    { COND_TRUE, OPC_QDIV,  PEEP_OP_SET|2,   PEEP_OP_MATCH|0,   PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|3,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_ANY,   OPERAND_ANY,     PEEP_OP_MATCH|3, PEEP_FLAGS_NONE }, // NEGC/NEGNC
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|4,   PEEP_OP_MATCH|1, PEEP_FLAGS_P2},
    { COND_TRUE, OPC_QDIV,  PEEP_OP_MATCH|2, PEEP_OP_MATCH|4, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|5,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    // There may be a NEG here
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// Fuse signed remainder+quotient (constant dividend, negative)
static PeepholePattern pat_qdiv_qdiv_signed4[] = {
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|0,   PEEP_OP_SET|1,   PEEP_FLAGS_P2},
    { COND_TRUE, OPC_QDIV,  PEEP_OP_SET|2,   PEEP_OP_MATCH|0,   PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|3,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_NEG,   OPERAND_ANY,     PEEP_OP_MATCH|3, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|4,   PEEP_OP_MATCH|1, PEEP_FLAGS_P2 | PEEP_FLAGS_MUST_WC | PEEP_FLAGS_WCZ_OK },
    { COND_TRUE, OPC_QDIV,  PEEP_OP_MATCH|2, PEEP_OP_MATCH|4, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|5,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    // NEGC/NEGNC here, but we don't really care if it's actually there
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
// Fuse signed remainder+quotient (constant dividend, positive)
static PeepholePattern pat_qdiv_qdiv_signed5[] = {
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|0,   PEEP_OP_SET|1,   PEEP_FLAGS_P2},
    { COND_TRUE, OPC_QDIV,  PEEP_OP_SET|2,   PEEP_OP_MATCH|0,   PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQY, PEEP_OP_SET|3,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_ABS,   PEEP_OP_SET|4,   PEEP_OP_MATCH|1, PEEP_FLAGS_P2 | PEEP_FLAGS_MUST_WC | PEEP_FLAGS_WCZ_OK },
    { COND_TRUE, OPC_QDIV,  PEEP_OP_MATCH|2, PEEP_OP_MATCH|4, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_GETQX, PEEP_OP_SET|5,   OPERAND_ANY,     PEEP_FLAGS_NONE },
    // There may be a NEG here
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// jmp conditional followed by jmp uncoditional to same place may be elided */
static PeepholePattern pat_jmp_jmp[] = {
    { COND_ANY, OPC_JUMP, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_JUMP, PEEP_OP_MATCH|0, OPERAND_ANY, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

/* delete "arg" instructions */
static int FixupDeleteInstr(int arg, IRList *irl, IR *ir) {
    IR *next_ir;
    while (ir && arg > 0) {
        next_ir = ir->next;
        DeleteIR(irl, ir);
        ir = next_ir;
        --arg;
    }
    return 1;
}


// convert mov x, #255; and x, y  to getbyte x, y, #0
static PeepholePattern pat_mov255_and[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_IMM|255, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

static int FixupMov255And(int arg, IRList *irl, IR *ir) {
    IR *next_ir = ir->next;
    DeleteIR(irl, ir);
    ir = next_ir;
    ReplaceOpcode(ir, OPC_GETBYTE);
    ir->src2 = NewImmediate(0);
    return 1;
}

static int ReplaceMaxMin(int arg, IRList *irl, IR *ir)
{
    IR *irnext;
    if (!InstrSetsFlags(ir, FLAG_WZ|FLAG_WC)) {
        return 0;
    }
    irnext = NextIR(ir);
    if (!IRFlagsDeadAfter(irl, irnext, FLAG_WZ|FLAG_WC)) {
        return 0;
    }
    ReplaceOpcode(ir, (IROpcode)arg);
    // note: one of the patterns checks for cmp with GT instead of GE, so
    // we have to use the second instruction's operand
    ir->src = irnext->src;
    DeleteIR(irl, irnext);
    return 1;
}
static int ReplaceExtend(int arg, IRList *irl, IR *ir)
{
    Operand *src = ir->src;
    int zbit;
    IR *irnext = NextIR(ir);
    if (src->kind != IMM_INT) {
        return 0;
    }
    zbit = 31 - src->val;
    ir->src = NewImmediate(zbit);
    ReplaceOpcode(ir, (IROpcode)arg);
    DeleteIR(irl, irnext);
    return 1;
}

// cmps x,#0 wc; if_c neg x,x -> abs x,x wc
// (wz also allowed)
static int ReplaceCmpsAbs(int arg, IRList *irl, IR *ir)
{
    IR *irnext = NextIR(ir);
    ReplaceOpcode(ir,OPC_ABS);
    ir->src = ir->dst;
    DeleteIR(irl,irnext);
    return 1;
}

//
// looks at the sequence (arg = 0)
//   wrc x (or muxc x,#-1 or subx x,x)
//   cmp x, #0 wz
// or (arg = 1)
//   muxc x,#-1 wz
// and if possible deletes it and replaces subsequent uses of C with NZ
//
static int ReplaceWrcCmp(int arg, IRList *irl, IR *ir)
{
    IR *ir0 = ir;
    IR *ir1 = NextIR(ir);
    IR *lastir = NULL;

    if (InstrIsVolatile(ir0) || !ir1 || InstrIsVolatile(ir1)) {
        return 0;
    }
    ir = arg == 0 ? NextIR(ir1) : NextIR(ir0);
    for(lastir = ir; lastir; lastir = NextIR(lastir)) {
        if (!lastir) {
            // End of function, safe.
            break;
        }
        if (InstrIsVolatile(lastir)) {
            return 0;
        }
        if (InstrUsesFlags(lastir, FLAG_WC)) {
            // C is explicitly used, so preserve it
            return 0;
        }
        if (IsBranch(lastir)) {
            // we assume flags do not have to be preserved across branches */
            break;
        }
        if (InstrSetsAnyFlags(lastir)) {
            // flags are to be replaced
            break;
        }
    }
    // OK, let's go ahead and change Z to NC
    while (ir != lastir) {
        ReplaceZWithNC(ir);
        ir = NextIR(ir);
    }
    if (lastir && IsBranch(lastir)) {
        ReplaceZWithNC(lastir);
    }
    if (arg == 0) {
        if (IsDeadAfter(ir1,ir0->dst)) DeleteIR(irl, ir0);
        DeleteIR(irl, ir1);
    } else {
        if (IsDeadAfter(ir0,ir0->dst)) DeleteIR(irl,ir0);
    }
    return 1;
}

static int ReplaceWrcTest(int arg, IRList *irl, IR *ir)
{
    IR *ir1 = NextIR(ir);

    // make sure the TEST only sets WC
    if (!ir1) return 0;
    if (ir1->flags & FLAG_WZ) return 0;
    if (!(ir1->flags & FLAG_WC)) return 0;
    if (InstrIsVolatile(ir1)) return 0;
    DeleteIR(irl, ir1);
    return 1;
}

//
// remove "arg" instructions following the current one
// and transfer their flag bits to the current one
//
static int RemoveNFlagged(int arg, IRList *irl, IR *ir)
{
    IR *irlast, *irnext;
    unsigned wcz_flags = 0;
    irnext = NextIR(ir);
    while (arg > 0 && irnext) {
        irlast = irnext;
        irnext = NextIR(irlast);
        --arg;
        if (arg == 0) {
            wcz_flags = irlast->flags & 0xff;
        }
        DeleteIR(irl, irlast);
    }
    ir->flags |= wcz_flags;
    return 1;
}

static int FixupMovAdd(int arg, IRList *irl, IR *ir)
{
    Operand *newsrc = ir->src;
    ir = NextIR(ir);
    if (ir->src != newsrc) {
        ir->src = newsrc;
        return 1;
    }
    return 0;
}

/* mov x, #0 ; test x, #1 wc => mov x, #0 wc */
static int FixupClrC(int arg, IRList *irl, IR *ir)
{
    IR *testir = NextIR(ir);
    int newflags;
    newflags = (testir->flags & (FLAG_WC|FLAG_WZ));
    ir->flags |= newflags;
    DeleteIR(irl, testir);

    /* an interesting other thing to check: if we have
     * a "drvc x" instruction following, replace it with
     * "drvl x"
     */
    IR *irnext = NextIR(ir);
    if (irnext && irnext->opc == OPC_DRVC) {
        ReplaceOpcode(irnext, OPC_DRVL);
    }
    return 1;
}
/* mov x, #1 ; test x, #1 wc => neg x, #1 wc */
static int FixupSetC(int arg, IRList *irl, IR *ir)
{
    IR *testir = NextIR(ir);
    int newflags;
    ReplaceOpcode(ir, OPC_NEG);
    newflags = (testir->flags & (FLAG_WC|FLAG_WZ));
    ir->flags |= newflags;
    DeleteIR(irl, testir);
    /* an interesting other thing to check: if we have
     * a "drvc x" instruction following, replace it with
     * "drvh x"
     */
    IR *irnext = NextIR(ir);
    if (irnext && irnext->opc == OPC_DRVC) {
        ReplaceOpcode(irnext, OPC_DRVH);
    }
    return 1;
}

// mov x, y; shr x, #N; and x, #255 => getbyte x, y, #N/8
// mov x, y; shr x, #N; and x, #65535 => getword x, y, #N/16
#if 0
static int FixupGetByteWord(int arg, IRList *irl, IR *ir0)
{
    IR *ir1 = NextIR(ir0);
    IR *ir2;
    int shift = 0;

    if (ir1->opc == OPC_SHR || ir1->opc == OPC_SAR) {
        ir2 = NextIR(ir1);
        shift = ir1->src->val;
    } else if (ir1->opc == OPC_AND) {
        ir2 = ir1;
        ir1 = NULL;
        shift = 0;
    } else {
        return 0;
    }

    ReplaceOpcode(ir0, (IROpcode)arg);
    if (arg == OPC_GETBYTE) {
        shift = shift/8;
    } else {
        shift = shift/16;
    }
    ir0->src2 = NewImmediate(shift);
    if (ir1) {
        DeleteIR(irl, ir1);
    }
    DeleteIR(irl, ir2);
    return 1;
}
#endif
// shl x, #N; and y, #MASK; or y, x => setbyte y, x, #N/8

static int FixupSetByteWord(int arg, IRList *irl, IR *ir0)
{
    IR *ir1 = NextIR(ir0);
    IR *ir2 = NextIR(ir1);
    int shift = 0;

    if (ir0->opc == OPC_SHL || ir0->opc == OPC_ROL) {
        shift = ir0->src->val;
    } else {
        return 0;
    }

    ReplaceOpcode(ir2, (IROpcode)arg);
    if (arg == OPC_SETBYTE) {
        shift = shift/8;
    } else {
        shift = shift/16;
    }
    ir2->src2 = NewImmediate(shift);
    if (IsDeadAfter(ir2, ir0->dst)) {
        DeleteIR(irl, ir0);
    }
    DeleteIR(irl, ir1);
    return 1;
}

static int FixupBmask(int arg, IRList *irl, IR *ir)
{
    IR *irnext, *irnext2;

    irnext = NextIR(ir);
    irnext2 = NextIR(irnext);

    ReplaceOpcode(irnext2, OPC_BMASK);
    irnext2->src = peep_ops[1];
    DeleteIR(irl, ir);
    DeleteIR(irl, irnext);
    return 1;
}

static int FixupWaitx(int arg, IRList *irl, IR *ir)
{
    IR *irnext;

    irnext = NextIR(ir);
    irnext->dst = peep_ops[1];
    DeleteIR(irl, ir);
    return 1;
}

// pattern is
//   mov x, #0
//   cmp arg01, arg02 wz
// if_e mov x, #1
//
// delete first move, and replace second with wrz x

static int FixupEq(int arg, IRList *irl, IR *ir)
{
    IR *irnext, *irnext2;

    irnext = NextIR(ir);
    irnext2 = NextIR(irnext);
    ReplaceOpcode(irnext2, (IROpcode)arg);
    irnext2->src = NULL;
    irnext2->cond = COND_TRUE;
    DeleteIR(irl, ir);
    return 1;
}

static int ReplaceDrvc(int arg, IRList *irl, IR *ir)
{
    ReplaceOpcode(ir, (IROpcode)arg);
    ir->cond = COND_TRUE;
    DeleteIR(irl, NextIR(ir));
    return 1;
}

static int ReplaceNot(int arg, IRList *irl, IR *ir) {
    ReplaceOpcode(ir,OPC_NOT);
    ir->src = ir->dst;
    return 1;
}

// pattern is
//   mov x, y
//   and x, #1
//   add z, x  '' x is dead
//
// change to
//    test y, #1 wz
// if_nz add z, #1
//
static int FixupAndAdd(int arg, IRList *irl, IR *ir0)
{
    IR *ir1, *ir2;

    ir1 = NextIR(ir0);
    ir2 = NextIR(ir1);

    ir1->dst = ir0->src;
    ReplaceOpcode(ir1, OPC_TEST);
    ir1->flags |= FLAG_WZ;

    ir2->src = NewImmediate(1);
    ir2->cond = COND_NE;

    DeleteIR(irl, ir0);
    return 1;
}

// pattern is
//   qmul x, y
//   getqx z
//   qmul x, y
//   ... other code ...
//
// check that z != x and z != y
// if so, delete second qmul
//

static int FixupQmuls(int arg, IRList *irl, IR *ir0)
{
    IR *ir1, *ir2;

    ir1 = NextIR(ir0);
    ir2 = NextIR(ir1);

    if (InstrModifies(ir1, ir0->src) || InstrModifies(ir2, ir0->dst)) {
        return 0;
    }
    DeleteIR(irl, ir2);
    return 1;
}

// pattern:
//  abs a, x wc
//  qdiv a,y
//  getq* b
//  neg* z,b
//  abs c, x wc
//  qdiv c,y
//  getq* d
//  neg* w,d
static int FixupQdivSigned(int arg, IRList *irl, IR *ir0)
{
    IR* ir1 = NextIR(ir0); // qdiv 1
    IR* ir2 = NextIR(ir1); // getq* 1
    IR* ir3 = NextIR(ir2); // neg* 1
    IR* ir4 = NextIR(ir3); // abs 2
    IR* ir5 = NextIR(ir4); // qdiv 2

    if (ir3->opc != OPC_NEGC && ir3->opc != OPC_NEGNC) return 0;

    if (InstrModifies(ir2,ir4->src)||InstrModifies(ir3,ir4->src)||
            InstrModifies(ir2,ir5->src)||InstrModifies(ir3,ir5->src) ) {
        return 0;
    }
    if (IsDeadAfter(ir5,ir5->dst)) DeleteIR(irl,ir4); // Delete second ABS unless register is somehow reused
    DeleteIR(irl,ir5); // Delete second QDIV
    return 1;
}

// pattern:
//  abs a, x
//  qdiv y,a
//  getq* b
//  neg* z,b
//  abs c, x wc
//  qdiv y,c
//  getq* d
//  neg w,d (maybe)
static int FixupQdivSigned2(int arg, IRList *irl, IR *ir0)
{
    IR* ir1 = NextIR(ir0); // qdiv 1
    IR* ir2 = NextIR(ir1); // getq* 1
    IR* ir3 = NextIR(ir2); // neg* 1
    IR* ir4 = NextIR(ir3); // abs 2
    IR* ir5 = NextIR(ir4); // qdiv 2

    if (ir3->opc != OPC_NEGC && ir3->opc != OPC_NEGNC && ir3->opc != OPC_NEG) return 0;

    if (InstrModifies(ir2,ir4->src)||InstrModifies(ir3,ir4->src)||
            InstrModifies(ir2,ir5->dst)||InstrModifies(ir3,ir5->dst) ) {
        return 0;
    }
    if (IsDeadAfter(ir5,ir5->src)) DeleteIR(irl,ir4); // Delete second ABS unless register is somehow reused
    DeleteIR(irl,ir5); // Delete second QDIV
    ir0->flags |= FLAG_WC;
    return 1;
}

// pattern:
//  abs a, x
//  qdiv y,a
//  getq* z
//  abs c, x wc
//  qdiv y,c
//  getq* d
//  neg* w,d
static int FixupQdivSigned3(int arg, IRList *irl, IR *ir0)
{
    IR* ir1 = NextIR(ir0); // qdiv 1
    IR* ir2 = NextIR(ir1); // getq* 1
    //IR* ir3 = NextIR(ir2); // neg* 1
    IR* ir4 = NextIR(ir2); // abs 2
    IR* ir5 = NextIR(ir4); // qdiv 2

    if (InstrModifies(ir2,ir4->src)||
            InstrModifies(ir2,ir5->dst)  ) {
        return 0;
    }
    if (IsDeadAfter(ir5,ir5->src)) DeleteIR(irl,ir4); // Delete second ABS unless register is somehow reused
    DeleteIR(irl,ir5); // Delete second QDIV
    ir0->flags |= FLAG_WC;
    return 1;
}

// bit mux
//   and val_2, mask_1
//   mov tmp_3, orig_0
//   andn tmp_3. mask_1
//   or   tmp_3, val_2
//   mov  orig_0, tmp_3
//
//   where "val" and "tmp" are dead after the sequence
//
// becomes
//   setq mask_1
//   muxq orig_0, val_2
//

static PeepholePattern pat_mux_qmux_1p[] = {
    { COND_ANY, OPC_AND, PEEP_OP_SET|2, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_MOV, PEEP_OP_SET|3, PEEP_OP_SET|0, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_ANDN, PEEP_OP_MATCH|3, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_OR, PEEP_OP_MATCH|3, PEEP_OP_MATCH_DEAD|2, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH_DEAD|3, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

static PeepholePattern pat_mux_qmux_2p[] = {
    { COND_ANY, OPC_MOV, PEEP_OP_SET|3, PEEP_OP_SET|0, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_AND, PEEP_OP_SET|2, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_ANDN, PEEP_OP_MATCH|3, PEEP_OP_MATCH|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_OR, PEEP_OP_MATCH|3, PEEP_OP_MATCH_DEAD|2, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_MOV, PEEP_OP_MATCH|0, PEEP_OP_MATCH_DEAD|3, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

// "arg" indicates which of the patterns above matched
static int FixupQMux(int arg, IRList *irl, IR *ir)
{
    IR *andir, *andnotir, *orir;
    IR *getir;
    IR *setir;
    Operand *mask_1, *orig_0, *val_2;
    IR *muxir;

    if (arg == 1) {
        andir = ir;
        getir = NextIR(andir);
        andnotir = NextIR(getir);
    } else if (arg == 2) {
        getir = ir;
        andir = NextIR(ir);
        andnotir = NextIR(andir);
    } else {
        ERROR(NULL, "internal peephole error");
        return 0;
    }
    orir = NextIR(andnotir);
    setir = NextIR(orir);

    val_2 = andir->dst;
    mask_1 = andir->src;
    orig_0 = getir->src;

    muxir = setir;

    DeleteIR(irl, getir);
    DeleteIR(irl, andnotir);
    DeleteIR(irl, orir);

    // change ir to be setq
    ReplaceOpcode(ir, OPC_SETQ);
    ir->dst = mask_1;
    // change setir
    ReplaceOpcode(muxir, OPC_MUXQ);
    muxir->dst = orig_0;
    muxir->src = val_2;

    //printf("qmux\n");

    return 1;
}

/*
 * sign extend followed by AND, GETBYTE, or GETWORD is sometimes redundant
 */
static PeepholePattern pat_shl_shr_and[] = {
    { COND_ANY, OPC_SHL,   PEEP_OP_SET|0, PEEP_OP_SET_IMM|1, PEEP_FLAGS_NONE },
    { COND_ANY, OPC_SAR,   PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { COND_ANY, OPC_AND,   PEEP_OP_MATCH|0, PEEP_OP_SET_IMM|2, PEEP_FLAGS_NONE },

    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_signx_and[] = {
    { COND_ANY, OPC_SIGNX, PEEP_OP_SET|0, PEEP_OP_SET_IMM|1, PEEP_FLAGS_P2 },
    { COND_ANY, OPC_AND,   PEEP_OP_MATCH|0, PEEP_OP_SET_IMM|2, PEEP_FLAGS_P2 },

    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

/* AND x, #255; wrbyte x, y : we can delete the AND if x is dead after */
static PeepholePattern pat_and_wrbyte[] = {
    { COND_ANY, OPC_AND,   PEEP_OP_SET|0, PEEP_OP_IMM|255, PEEP_FLAGS_NONE },
    { COND_ANY, OPC_WRBYTE,PEEP_OP_MATCH_DEAD|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
      
static int FixupShlShrAndImm(int arg, IRList *irl, IR *ir0)
{
    IR *ir1, *ir2;
    int signx_width;
    unsigned and_pat, signx_pat;
    
    ir1 = NextIR(ir0);  // this is the SHR
    ir2 = NextIR(ir1);  // and this is the AND
    signx_width = ir0->src->val & 0x1f;
    and_pat = (unsigned)ir2->src->val;
    signx_pat = (1U<<(signx_width)) - 1;
    if ( (and_pat & signx_pat) == and_pat ) {
        // we can delete the shl and shr
        DeleteIR(irl, ir1);
        DeleteIR(irl, ir0);
        return 1;
    }
    return 0;
}

static int FixupSignxAndImm(int arg, IRList *irl, IR *ir0)
{
    IR *ir1;
    int signx_width;
    unsigned and_pat, signx_pat;
    
    ir1 = NextIR(ir0);  // this is the AND
    signx_width = ir0->src->val & 0x1f;
    and_pat = (unsigned)ir1->src->val;
    signx_pat = (1U<<(signx_width+1)) - 1;
    if ( (and_pat & signx_pat) == and_pat ) {
        // we can delete the signx
        DeleteIR(irl, ir0);
        return 1;
    }
    return 0;
}

// add objptr, #N
// mov x, objptr
// sub objptr, #N
// This happens often enough that it's worth optimizing to
// mov x, objptr
// add x, #N
static PeepholePattern pat_lea_ptr[] = {
    { COND_TRUE, OPC_ADD, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|2, PEEP_OP_MATCH|0, PEEP_FLAGS_NONE },
    { COND_TRUE, OPC_SUB, PEEP_OP_MATCH|0, PEEP_OP_MATCH|1, PEEP_FLAGS_NONE },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

static int FixupLeaPtr(int arg, IRList *irl, IR *ir) {
    IR *next_ir = NextIR(ir);
    IR *last_ir = NextIR(next_ir);
    Operand *dest = next_ir->dst;

    ReplaceOpcode(last_ir, OPC_ADD);
    last_ir->dst = dest;
    DeleteIR(irl, ir);
    return 1;
}

/*
 * the actual list of peepholes
 */

struct Peepholes {
    PeepholePattern *check;
    int arg;
    int (*replace)(int arg, IRList *irl, IR *ir);
} peep2[] = {
    { pat_maxs, OPC_MAXS, ReplaceMaxMin },
    { pat_maxu, OPC_MAXU, ReplaceMaxMin },
    { pat_mins, OPC_MINS, ReplaceMaxMin },
    { pat_minu, OPC_MINU, ReplaceMaxMin },
    { pat_maxs_off, OPC_MAXS, ReplaceMaxMin },
    { pat_maxu_off, OPC_MAXU, ReplaceMaxMin },

    { pat_zeroex, OPC_ZEROX, ReplaceExtend },
    { pat_signex, OPC_SIGNX, ReplaceExtend },

    { pat_signx_and, 0, FixupSignxAndImm },
    { pat_shl_shr_and, 0, FixupShlShrAndImm },

    { pat_and_wrbyte, 1, FixupDeleteInstr },
    
    { pat_drvc1, OPC_DRVC, ReplaceDrvc },
    { pat_drvc2, OPC_DRVC, ReplaceDrvc },
    { pat_drvnc1, OPC_DRVNC, ReplaceDrvc },
    { pat_drvnc2, OPC_DRVNC, ReplaceDrvc },
    { pat_bitc1, OPC_BITC, ReplaceDrvc },
    { pat_bitc2, OPC_BITC, ReplaceDrvc },
    { pat_bitnc1, OPC_BITNC, ReplaceDrvc },
    { pat_bitnc2, OPC_BITNC, ReplaceDrvc },
    { pat_drvz, OPC_DRVZ, ReplaceDrvc },
    { pat_drvnz1, OPC_DRVNZ, ReplaceDrvc },
    { pat_drvnz2, OPC_DRVNZ, ReplaceDrvc },

    { pat_negc1, OPC_NEGC, ReplaceDrvc },
    { pat_negc2, OPC_NEGC, ReplaceDrvc },
    { pat_negnc1, OPC_NEGNC, ReplaceDrvc },
    { pat_negnc2, OPC_NEGNC, ReplaceDrvc },
    { pat_negz1, OPC_NEGZ, ReplaceDrvc },
    { pat_negz2, OPC_NEGZ, ReplaceDrvc },
    { pat_negnz1, OPC_NEGNZ, ReplaceDrvc },
    { pat_negnz2, OPC_NEGNZ, ReplaceDrvc },

    { pat_sumc1, OPC_SUMC, ReplaceDrvc },
    { pat_sumc2, OPC_SUMC, ReplaceDrvc },
    { pat_sumnc1, OPC_SUMNC, ReplaceDrvc },
    { pat_sumnc2, OPC_SUMNC, ReplaceDrvc },
    { pat_sumz1, OPC_SUMZ, ReplaceDrvc },
    { pat_sumz2, OPC_SUMZ, ReplaceDrvc },
    { pat_sumnz1, OPC_SUMNZ, ReplaceDrvc },
    { pat_sumnz2, OPC_SUMNZ, ReplaceDrvc },

    { pat_not, 0, ReplaceNot},

    { pat_cmps_abs, 0, ReplaceCmpsAbs},

    { pat_wrc_cmp, 0, ReplaceWrcCmp },
    { pat_wrc_and, 1, RemoveNFlagged },
    { pat_wrc_test, 0, ReplaceWrcTest },

    { pat_muxc_cmp, 0, ReplaceWrcCmp },
    { pat_subx_cmp, 0, ReplaceWrcCmp },
    { pat_muxc_wz, 1, ReplaceWrcCmp },

    { pat_rdbyte1, 2, RemoveNFlagged },
    { pat_rdword1, 2, RemoveNFlagged },
    { pat_rdbyte2, 1, RemoveNFlagged },
    { pat_rdword2, 1, RemoveNFlagged },

    { pat_movadd, 0, FixupMovAdd },

    { pat_bmask1, 0, FixupBmask },
    { pat_bmask2, 0, FixupBmask },

    { pat_waitx, 0, FixupWaitx },

    { pat_seteq, OPC_WRZ, FixupEq },
    { pat_setne, OPC_WRNZ, FixupEq },
#if 0
    { pat_sar24getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_shr24getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_sar16getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_shr16getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_sar8getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_shr8getbyte, OPC_GETBYTE, FixupGetByteWord },

    { pat_sar16getword, OPC_GETWORD, FixupGetByteWord },
    { pat_shr16getword, OPC_GETWORD, FixupGetByteWord },
#endif
    { pat_shl8setbyte, OPC_SETBYTE, FixupSetByteWord },
    { pat_shl16setbyte, OPC_SETBYTE, FixupSetByteWord },
    { pat_shl24setbyte, OPC_SETBYTE, FixupSetByteWord },

    { pat_shl16setword, OPC_SETWORD, FixupSetByteWord },

    { pat_clrc, 0, FixupClrC },
    { pat_setc1, 0, FixupSetC },
    { pat_setc2, 0, FixupSetC },

    { pat_mov_and_add, 0, FixupAndAdd },

    { pat_qmul_qmul1, 0, FixupQmuls },
    { pat_qmul_qmul2, 0, FixupQmuls },
    { pat_qdiv_qdiv1, 0, FixupQmuls },
    { pat_qdiv_qdiv2, 0, FixupQmuls },

    { pat_qdiv_qdiv_signed1, 0, FixupQdivSigned},
    { pat_qdiv_qdiv_signed2, 0, FixupQdivSigned},
    { pat_qdiv_qdiv_signed3, 0, FixupQdivSigned2},
    { pat_qdiv_qdiv_signed4, 0, FixupQdivSigned2},
    { pat_qdiv_qdiv_signed5, 0, FixupQdivSigned3},

    { pat_mux_qmux_1p, 1, FixupQMux },
    { pat_mux_qmux_2p, 2, FixupQMux },

    { pat_jmp_jmp, 1, FixupDeleteInstr },

    { pat_mov255_and, 1, FixupMov255And },

    { pat_lea_ptr, 0, FixupLeaPtr },
};


int OptimizePeephole2(IRList *irl)
{
    IR *ir;
    int change = 0;
    int i, r;

    ir = irl->head;
    for(;;) {
        while (ir && IsDummy(ir)) {
            ir = ir->next;
        }
        if (!ir) break;
        if (!InstrIsVolatile(ir)) {
            for (i = 0; i < sizeof(peep2) / sizeof(peep2[0]); i++) {
                r = MatchPattern(peep2[i].check, ir);
                if (r) {
                    r = (*peep2[i].replace)(peep2[i].arg, irl, ir);
                    if (r) {
                        change++;
                    }
                    break;
                }
            }
        }
        ir = NextIR(ir);
    }
    return change;
}
