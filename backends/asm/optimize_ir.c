//
// IR optimizer
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

// forward declarations
static bool IsReorderBarrier(IR *ir);

// fcache size in longs; -1 means take a guess
int gl_fcache_size = -1;

//
// helper functions
//


// a kingdom for a std::vector....
struct dependency {
    struct dependency *link;
    Operand *reg;
};

static void DeleteDependencyList(struct dependency **list) {
    for (struct dependency *tmp=*list; tmp;) {
        struct dependency *tmp2 = tmp->link;
        free(tmp);
        tmp=tmp2;
    }
    *list = NULL;
}

static void PrependDependency(struct dependency **list,Operand *reg) {
    struct dependency *tmp = (struct dependency *)malloc(sizeof(struct dependency));
    tmp->link = *list;
    tmp->reg = reg;
    *list = tmp;
}

static bool CheckDependency(struct dependency **list, Operand *reg) {
    for (struct dependency *tmp=*list; tmp; tmp=tmp->link) {
        if (tmp->reg == reg) return true;
    }
    return false;
}

static void DeleteDependencies(struct dependency **list,Operand *reg) {
    struct dependency **prevlink = list;
    for (struct dependency *tmp=*list; tmp;) {
        struct dependency *next = tmp->link;
        if (tmp->reg == reg) {
            *prevlink = next;
            free(tmp);
        } else {
            prevlink = &tmp->link;
        }
        tmp=next;
    }
}

/* IR instructions that have no effect on the generated code */
bool IsDummy(IR *op)
{
    switch(op->opc) {
    case OPC_COMMENT:
    case OPC_LITERAL:
    case OPC_CONST:
    case OPC_DUMMY:
        return true;
    default:
        return op->cond == COND_FALSE;
    }
}

//
// check if immediate operand needs an AUGD or AUGS to fit
//
bool NeedsImmAug(Operand *op) {
    if (!gl_p2) return false; // P1 immediates are never augmented
    if (op && op->kind == IMM_INT) {
        uint32_t val = (uint32_t)op->val;
        if (val > 511) return true;
    }
    return false;
}

//
// replace an opcode
//
void
ReplaceOpcode(IR *ir, IROpcode op)
{
    ir->opc = op;
    ir->instr = FindInstrForOpc(op);
}

// Returns the one unique jump to a given label.
// NULL if multiple jumps (or none at all)
static IR *UniqJumpForLabel(IR *lbl) {
    if (lbl->opc != OPC_LABEL) ERROR(NULL,"internal error, UniqJumpForLabel called on wrong opc %d",lbl->opc);
    struct ir_lbljumps *list = (struct ir_lbljumps *)lbl->aux;
    if (list && !list->next) return list->jump;
    else return NULL;
}

static void AppendLblJump(IR *lbl,IR *jmp) {
    if (lbl->opc != OPC_LABEL) ERROR(NULL,"internal error, AppendLblJump called on wrong opc %d",lbl->opc);
    struct ir_lbljumps *entry = (struct ir_lbljumps *)malloc(sizeof(struct ir_lbljumps));
    entry->next = (struct ir_lbljumps *)lbl->aux;
    entry->jump = jmp;
    lbl->aux = entry;
}

//
// return TRUE if an instruction uses its destination
// (most do)
//
static bool
InstrReadsDst(IR *ir)
{
    switch (ir->opc) {
    case OPC_MOV:
    case OPC_NEG:
    case OPC_NEGC:
    case OPC_NEGNC:
    case OPC_NEGZ:
    case OPC_NEGNZ:
    case OPC_ABS:
    case OPC_RDBYTE:
    case OPC_RDWORD:
    case OPC_RDLONG:
    case OPC_GETQX:
    case OPC_GETQY:
    case OPC_GETRND:
    case OPC_GETCT:
    case OPC_GETNIB:
    case OPC_GETBYTE:
    case OPC_GETWORD:
    case OPC_WRC:
    case OPC_WRNC:
    case OPC_WRZ:
    case OPC_WRNZ:
    case OPC_DECOD:
    case OPC_ENCOD:
    case OPC_BMASK:
    case OPC_NOT:
    case OPC_ONES:
    case OPC_RDPIN:
        return false;
    case OPC_MUXC:
    case OPC_MUXNC:
    case OPC_MUXZ:
    case OPC_MUXNZ:
        if (ir->src && ir->src->kind == IMM_INT && (int32_t)ir->src->val == -1) {
            return false;
        } else {
            return true;
        }
    case OPC_SUBX:
    case OPC_SUBSX:
        return ir->src != ir->dst;
    default:
        break;
    }
    return true;
}

// recognize instructions that modify their
// destination
// (should only be called on real instructions and OPC_LIVE)

static bool
InstrSetsDst(IR *ir)
{
    switch (ir->opc) {
    case OPC_LABEL:
    case OPC_WRLONG:
    case OPC_WRWORD:
    case OPC_WRBYTE:
    case OPC_QDIV:
    case OPC_QFRAC:
    case OPC_QMUL:
    case OPC_QROTATE:
    case OPC_QSQRT:
    case OPC_QVECTOR:
    case OPC_QLOG:
    case OPC_QEXP:
    case OPC_DRVH:
    case OPC_DRVL:
    case OPC_DRVC:
    case OPC_DRVNC:
    case OPC_DRVZ:
    case OPC_DRVNZ:
    case OPC_SETQ:
    case OPC_SETQ2:
    case OPC_TESTB:
    case OPC_TESTBN:
    case OPC_LOCKTRY:
        return false;
    case OPC_LOCKREL:
        return (ir->flags & FLAG_WC) && ir->dst->kind != IMM_INT;
    case OPC_CMP:
    case OPC_CMPS:
    case OPC_TEST:
    case OPC_TESTN:
    case OPC_GENERIC_NR:
    case OPC_GENERIC_NR_NOFLAGS:
    case OPC_PUSH:
    case OPC_LOCKRET:
    case OPC_LOCKSET:
    case OPC_LOCKCLR:
        return (ir->flags & FLAG_WR) != 0;
    default:
        return (ir->flags & FLAG_NR) == 0;
    }
}

// recognizes branch instructions
// jumps and calls are both branches, but sometimes
// we treat them differently

static bool IsJump(IR *ir)
{
    switch (ir->opc) {
    case OPC_JUMP:
    case OPC_REPEAT:
    case OPC_DJNZ:
    case OPC_REPEAT_END:
    case OPC_JMPREL:
    case OPC_GENERIC_BRANCH:
    case OPC_GENERIC_BRCOND:
        return true;
    default:
        return false;
    }
}

bool IsBranch(IR *ir)
{
    return IsJump(ir) || ir->opc == OPC_CALL;
}

static bool IsWrite(IR *ir) {
    switch (ir->opc) {
    case OPC_WRLONG:
    case OPC_WRWORD:
    case OPC_WRBYTE:
    case OPC_GENERIC:
    case OPC_GENERIC_NOFLAGS:
        return true;
    default:
        return false;
    }
}

static bool IsRead(IR *ir) {
    switch (ir->opc) {
    case OPC_RDLONG:
    case OPC_RDWORD:
    case OPC_RDBYTE:
    case OPC_GENERIC:
    case OPC_GENERIC_NOFLAGS:
        return true;
    default:
        return false;
    }
}


static bool
IsReadWrite(IR *ir)
{
    if (!ir) {
        return false;
    }
    switch (ir->opc) {
    case OPC_RDBYTE:
    case OPC_RDWORD:
    case OPC_RDLONG:
    case OPC_WRBYTE:
    case OPC_WRWORD:
    case OPC_WRLONG:
        return true;
    default:
        return false;
    }
}

static bool
IsLabel(IR *ir)
{
    return ir->opc == OPC_LABEL;
}

static bool IsPrefixOpcode(IR *ir) {
    if (!ir) return false;
    switch (ir->opc) {
    case OPC_GENERIC_DELAY:
    case OPC_SETQ:
    case OPC_SETQ2:
        return true;
    default:
        return false;
    }
}
static bool IsCordicCommand(IR *ir) {
    if (!ir) return false;
    switch (ir->opc) {
    case OPC_QMUL:
    case OPC_QDIV:
    case OPC_QFRAC:
    case OPC_QROTATE:
    case OPC_QSQRT:
    case OPC_QVECTOR:
    case OPC_QLOG:
    case OPC_QEXP:
        return true;
    default:
        return false;
    }
}
static bool IsCordicGet(IR *ir) {
    if (!ir) return false;
    switch (ir->opc) {
    case OPC_GETQX:
    case OPC_GETQY:
        return true;
    default:
        return false;
    }
}

// return TRUE if an instruction modifies a register
bool
InstrModifies(IR *ir, Operand *reg)
{
    Operand *dst = ir->dst;
    Operand *src = ir->src;
    if (reg && reg->kind == REG_SUBREG) {
        reg = (Operand *)reg->name;
    }
    if (dst && dst->kind == REG_SUBREG) {
        dst = (Operand *)dst->name;
    }
    if (src && src->kind == REG_SUBREG) {
        src = (Operand *)src->name;
    }
    if (dst == reg) {
        return InstrSetsDst(ir);
    }
    if (ir->opc == OPC_MOVD && reg) {
        if (dst->kind == IMM_COG_LABEL && !strcmp(reg->name, dst->name)) {
            return true;
        }
    }
    if (src == reg && OPEFFECT_NONE != (ir->srceffect & 0xff)) {
        return true;
    }
    return false;
}

// return TRUE if a and b are the same register
bool
SameRegister(Operand *A, Operand *B)
{
    if (A == B) return true;
    if (A->kind != REG_SUBREG || B->kind != REG_SUBREG) {
        return false;
    }
    if (A->val != B->val) return false;
    return SameRegister((Operand *)A->name, (Operand *)B->name);
}

// return TRUE if an instruction uses a register
bool
InstrUses(IR *ir, Operand *reg)
{
    Operand *dst = ir->dst;
    Operand *src = ir->src;

    if (reg && reg->kind == REG_SUBREG) {
        reg = (Operand *)reg->name;
    }
    if (dst && dst->kind == REG_SUBREG) {
        dst = (Operand *)dst->name;
    }
    if (src && src->kind == REG_SUBREG) {
        src = (Operand *)src->name;
    }
    if (src == reg) {
        return true;
    }
    if (dst == reg && InstrReadsDst(ir)) {
        return true;
    }
    if (src && src->kind == IMM_COG_LABEL && reg) {
        if (!strcmp(reg->name, src->name)) {
            return true;
        }
    }
    return false;
}

// returns TRUE if an operand represents a local register or argument
static bool
IsArg(Operand *op)
{
    if (op->kind == REG_SUBREG) op = (Operand *)op->name;
    return op->kind == REG_ARG;
}

static bool
isResult(Operand *op)
{
    if (op->kind == REG_SUBREG) op = (Operand *)op->name;
    return op->kind == REG_RESULT;
}

bool
IsSubReg(Operand *reg)
{
    return reg && reg->kind == REG_SUBREG;
}

bool
IsHwReg(Operand *reg)
{
    return reg && reg->kind == REG_HW;
}

// returns TRUE if an operand represents a local register
bool
IsLocal(Operand *op)
{
    if (op->kind == REG_SUBREG) op = (Operand *)op->name;
    return op->kind == REG_LOCAL || op->kind == REG_TEMP;
}

// returns TRUE if an operand represents a local register or argument
bool
IsLocalOrArg(Operand *op)
{
    if (op->kind == REG_SUBREG) op = (Operand *)op->name;
    return op->kind == REG_LOCAL || op->kind == REG_ARG || op->kind == REG_TEMP;
}

static bool
IsDirectImmediate(Operand *op)
{
    return op && op->kind == IMM_INT;
}

static bool
IsImmediate(Operand *op)
{
    return op->kind == IMM_INT || op->kind == IMM_COG_LABEL;
}

static bool
IsImmediateVal(Operand *op, int val)
{
    return op->kind == IMM_INT && op->val == val;
}

bool
IsValidDstReg(Operand *op)
{
    switch(op->kind) {
    case REG_SUBREG:
    case REG_LOCAL:
    case REG_TEMP:
    case REG_ARG:
    case REG_REG:
    case IMM_COG_LABEL:  // for popping ret addresses and such
        return true;
    default:
        return false;
    }
}

static unsigned CanonizeFlags(unsigned f) {
    return ((f&FLAG_CSET)?FLAG_WC:0)|((f&FLAG_ZSET)?FLAG_WZ:0);
}

bool
InstrSetsFlags(IR *ir, unsigned flags)
{
    return ir->flags & CanonizeFlags(flags);
}

bool
InstrSetsAnyFlags(IR *ir)
{
    return InstrSetsFlags(ir, FLAG_CZSET);
}

static unsigned
FlagsUsedByCond(IRCond cond) {

    switch (cond) {
    case COND_TRUE:
    case COND_FALSE:
        return 0;
    case COND_NC:
    case COND_C:
        return FLAG_WC;
    case COND_NZ:
    case COND_Z:
        return FLAG_WZ;
    default:
        return FLAG_WC|FLAG_WZ;
    }
}

static bool
InstrUsesFlags_CondAside(IR *ir, unsigned flags)
{
    if ((ir->flags & (FLAG_ANDC|FLAG_XORC|FLAG_ORC)) && (flags & FLAG_WC)) return true;
    if ((ir->flags & (FLAG_ANDZ|FLAG_XORZ|FLAG_ORZ)) && (flags & FLAG_WZ)) return true;

    switch (ir->opc) {
    case OPC_GENERIC:
    case OPC_GENERIC_DELAY:
    case OPC_GENERIC_NR:
    case OPC_GENERIC_BRANCH:
    case OPC_GENERIC_BRCOND:
        /* it might use flags, we don't know (e.g. addx) */
        return true;
    case OPC_GENERIC_NOFLAGS:
    case OPC_GENERIC_NR_NOFLAGS:
        /* definitely does not use flags */
        return false;
    case OPC_WRC:
    case OPC_WRNC:
    case OPC_DRVC:
    case OPC_DRVNC:
    case OPC_MUXC:
    case OPC_MUXNC:
    case OPC_SUMC:
    case OPC_SUMNC:
    case OPC_NEGC:
    case OPC_NEGNC:
    case OPC_BITC:
    case OPC_BITNC:
    case OPC_RCL:
    case OPC_RCR:
    case OPC_ADDX:
    case OPC_ADDSX:
    case OPC_SUBX:
    case OPC_SUBSX:
        /* definitely uses the C flag */
        return (flags & FLAG_WC) != 0;
    case OPC_DRVZ:
    case OPC_DRVNZ:
    case OPC_MUXZ:
    case OPC_MUXNZ:
    case OPC_NEGZ:
    case OPC_NEGNZ:
    case OPC_SUMZ:
    case OPC_SUMNZ:
    case OPC_WRZ:
    case OPC_WRNZ:
        /* definitely uses the Z flag */
        return (flags & FLAG_WZ) != 0;
    default:
        return false;
    }
}

bool
InstrUsesFlags(IR *ir, unsigned flags) {
    if (FlagsUsedByCond(ir->cond) & flags) return true;
    return InstrUsesFlags_CondAside(ir,flags);
}

bool IsHubDest(Operand *dst)
{
    switch (dst->kind) {
    case IMM_HUB_LABEL:
    case REG_HUBPTR:
        return true;
    default:
        return false;
    }
}

bool MaybeHubDest(Operand *dst)
{
    switch (dst->kind) {
    case IMM_COG_LABEL:
        if (!strcmp(dst->name, "gosub_"))
            return true;
        return false;
    default:
        return true;
    }
}

Operand *
JumpDest(IR *jmp)
{

    switch (jmp->opc) {
    case OPC_DJNZ:
    case OPC_GENERIC_BRCOND:
        return jmp->src;
    default:
        return jmp->dst;
    }
}

/*
 * true if a branch target is after a given instruction
 * (or equal to that instruction)
 */
static bool
JumpIsAfterOrEqual(IR *ir, IR *jmp)
{
    // ptr to jump destination gets stored in aux
    if (jmp->aux) {
        IR *label = (IR *)jmp->aux;
        if (!label) {
            return false;
        }
        if (label->addr >= ir->addr) {
            return true;
        }
        return false;
    }
    if (curfunc && (
                JumpDest(jmp) == FuncData(curfunc)->asmretname
                || JumpDest(jmp) == FuncData(curfunc)->asmreturnlabel) )
        return true;
    return false;
}

static bool
IsForwardJump(IR *jmp)
{
    return JumpIsAfterOrEqual(jmp, jmp);
}

/* check to see if a jump is relatively close to its
 * destination; if this is false then (on P2) djnz may
 * exceed its relative distance limit
 */
static bool
IsCloseJump(IR *jmp)
{
    if (jmp->aux) {
        int delta;
        IR *label = (IR *)jmp->aux;
        delta = jmp->addr - label->addr;
        if (delta < 0) delta = -delta;
        if (delta < 200) return true;
    }
    return false;
}

/*
 * find next instruction following ir
 */
static IR *
NextInstruction(IR *ir)
{
    while (ir) {
        ir = ir->next;
        if (ir && !IsDummy(ir) && !IsLabel(ir)) {
            return ir;
        }
    }
    return NULL;
}

/* like NextInstruction, but follow unconditional jumps */
static IR *
NextInstructionFollowJumps(IR *ir)
{
    ir = NextInstruction(ir);
    if (!ir || (ir->opc != OPC_JUMP) || ir->cond != COND_TRUE)
        return ir;
    if (!ir->aux) {
        return ir;
    }
    ir = (IR *)ir->aux;
    return NextInstructionFollowJumps(ir);
}

#if 0
/*
 * return the next instruction after (or including) ir
 * that uses op, or NULL if we can't determine it
 */
static IR *
NextUseAfter(IR *ir, Operand *op)
{
    while (ir) {
        while (IsDummy(ir)) {
            ir = ir->next;
        }
        if (!ir) return NULL;
        if (IsJump(ir)) {
            // we could try to thread through the jumps,
            // but for now punt
            return NULL;
        }
        if (InstrUses(ir, op) || InstrModifies(ir, op)) {
            if (ir->cond != COND_TRUE) {
                return NULL;
            }
            return ir;
        }
        ir = ir->next;
    }
    return NULL;
}
#endif

//
// find the next IR after orig that uses dest
//
static IR* FindNextUse(IR *ir, Operand *dst)
{
    ir = ir->next;
    while (ir) {
        if (InstrUses(ir, dst)) {
            return ir;
        }
        if (InstrModifies(ir, dst)) {
            // modifies but does not use? abort
            return NULL;
        }
        if (IsJump(ir)) {
            return NULL;
        }
        if (ir->opc == OPC_LABEL) {
            return NULL;
        }
        ir = ir->next;
    }
    return NULL;
}

bool
IsSrcBitIndex(IR *ir)
{
    switch (ir->opc) {
    case OPC_SHL:
    case OPC_SHR:
    case OPC_SAR:
    case OPC_ROL:
    case OPC_ROR:
    case OPC_RCL:
    case OPC_RCR:
    case OPC_TESTB:
    case OPC_TESTBN:
    case OPC_ZEROX:
    case OPC_SIGNX:
        return true;
    default:
        return false;
    }
}

static bool
isConstMove(IR *ir, int32_t *valout) {
    if (!ir->src || !IsImmediate(ir->src)) return false;
    int32_t val = (int32_t)ir->src->val;
    switch (ir->opc) {
    case OPC_MOV:
        break;
    case OPC_NEG:
        val = -val;
        break;
    case OPC_ABS:
        if (val<0) val = -val;
        break;
    case OPC_DECOD:
        val = 1<<(val&31);
        break;
    case OPC_BMASK:
        val = (2<<(val&31))-1;
        break;
    case OPC_GETNIB:
        val = (val>>(ir->src2->val*4))&0xF;
        break;
    case OPC_GETBYTE:
        val = (val>>(ir->src2->val*8))&0xFF;
        break;
    case OPC_GETWORD:
        val = (val>>(ir->src2->val*16))&0xFFFF;
        break;
    default:
        return false;
    }
    if (valout) *valout = val;
    return true;
}

static uint32_t EvalP2BitMask(int x) {
    uint32_t val = (2<<((x>>5)&31))-1;
    val = val<<(x&31) | val>>((32-x)&31);
    return val;
}

static bool
isMaskingOp(IR *ir, int32_t *maskout) {
    if (!ir->src) return false;
    int32_t mask;
    if (IsImmediate(ir->src)) {
        mask = (int32_t)ir->src->val;
        switch (ir->opc) {
        case OPC_AND:
            break;
        case OPC_ANDN:
            mask = ~mask;
            break;
        case OPC_ZEROX:
            mask = (2<<(mask&31))-1;
            break;
        case OPC_BITL:
            mask = ~EvalP2BitMask(mask);
            break;
        default:
            return false;
        }
    } else if (ir->src == ir->dst) {
        switch (ir->opc) {
        case OPC_GETNIB:
            if (!IsImmediateVal(ir->src2,0)) return false;
            mask = 0xF;
            break;
        case OPC_GETBYTE:
            if (!IsImmediateVal(ir->src2,0)) return false;
            mask = 0xFF;
            break;
        case OPC_GETWORD:
            if (!IsImmediateVal(ir->src2,0)) return false;
            mask = 0xFFFF;
            break;
        default:
            return false;
        }
    } else {
        return false;
    }
    if (maskout) *maskout = mask;
    return true;
}

// Is this an op that moves data from S to D in some manner?
// For all of these InstrReadsDst is false
static bool
isMoveLikeOp(IR *ir) {
    switch (ir->opc) {
    case OPC_MOV:
    case OPC_NEG:
    case OPC_ABS:
    case OPC_NOT:
    case OPC_ENCOD:
    case OPC_DECOD:
    case OPC_ONES:
    case OPC_BMASK:
    case OPC_NEGC:
    case OPC_NEGNC:
    case OPC_NEGZ:
    case OPC_NEGNZ:
    case OPC_GETNIB:
    case OPC_GETBYTE:
    case OPC_GETWORD:
    // memory reads also count
    case OPC_RDBYTE:
    case OPC_RDWORD:
    case OPC_RDLONG:
        return true;
    default:
        return false;
    }
}

static inline bool CondIsSubset(IRCond super, IRCond sub) {
    return sub == (super|sub);
}

// Note: currently only valid for P2
static int InstrMinCycles(IR *ir) {
    if (IsDummy(ir)||IsLabel(ir)) return 0;
    int aug = 0;
    
    if (NeedsImmAug(ir->src)) aug += 2;
    if (NeedsImmAug(ir->dst)) aug += 2;

    switch (ir->opc) {
    case OPC_WRBYTE:
    case OPC_WRWORD:
    case OPC_WRLONG:
        return aug+3;
    case OPC_RDBYTE:
    case OPC_RDWORD:
    case OPC_RDLONG:
        return aug+9;
    default:
        return aug+2;
    }
}
#if 0
// Note: currently only valid for P2, and not used yet
static int InstrMaxCycles(IR *ir) {
    if (IsDummy(ir)||IsLabel(ir)) return 0;
    if (IsBranch(ir)) return 100000; // No good.

    int aug = 0;
    if (ir->src && ir->src->kind == IMM_INT && ir->src->val > 511) aug += 2;
    if (ir->dst && ir->dst->kind == IMM_INT && ir->dst->val > 511) aug += 2;

    switch (ir->opc) {
    case OPC_WRBYTE:
        return aug+26; // Can never be unaligned
    case OPC_WRWORD:
    case OPC_WRLONG:
        return aug+27;
    case OPC_RDBYTE:
        return aug+20; // Can never be unaligned
    case OPC_RDWORD:
    case OPC_RDLONG:
        return aug+21;
    case OPC_GENERIC:
    case OPC_GENERIC_NR:
    case OPC_GENERIC_DELAY:
    case OPC_GENERIC_BRANCH:
    case OPC_GENERIC_BRCOND:
    case OPC_GENERIC_NR_NOFLAGS:
    case OPC_GENERIC_NOFLAGS:
    case OPC_WAITX:
    case OPC_WAITCNT:
        return 100000; // No good;
    case OPC_QMUL:
    case OPC_QDIV:
    case OPC_QFRAC:
    case OPC_QSQRT:
    case OPC_QLOG:
    case OPC_QEXP:
        return aug+9;
    default:
        return aug+2;
    }
}
#endif

static int
AddSubVal(IR *ir)
{
    int val = ir->src->val;
    if (ir->opc == OPC_SUB) val = -val;
    return val;
}

extern Operand *mulfunc, *unsmulfunc, *divfunc, *unsdivfunc, *muldiva, *muldivb;

static bool isMulDivFunc(Operand *func) {
    return !!func && (func == mulfunc || func == unsmulfunc || func == divfunc || func == unsdivfunc);
}

// Set "actually" if you only care about the call actually using the current value
static bool FuncUsesArgEx(Operand *func, Operand *arg, bool actually)
{
    if (isMulDivFunc(func)) {
        return (arg == muldiva || arg == muldivb);
    } else if (arg == muldiva || arg == muldivb) {
        return !actually;
    } else if (func && func->val) {
        Function *funcObj = (Function *)func->val;
        if ((/*func->kind == IMM_COG_LABEL ||*/ func->kind == IMM_HUB_LABEL) && (actually || funcObj->is_leaf || FuncData(funcObj)->effectivelyLeaf)) {
            if (arg->kind != REG_ARG) return true; // subreg or smth
            if (arg->val < funcObj->numparams) return true; // Arg used;
            if (!actually && arg->val < FuncData(funcObj)->maxInlineArg) return true; // Arg clobbered
            if ( ((Function *)func->val)->numparams < 0 ) return true; // varargs
            return false; // Arg not used
        }
    }
    return true;
}
static bool FuncUsesArg(Operand *func, Operand *arg) {
    return FuncUsesArgEx(func,arg,false);
}


static bool IsCallThatUsesReg(IR *ir,Operand *op) {
    if (ir->opc != OPC_CALL) return false;
    if (IsLocal(op)) return false;
    if (IsArg(op) && !FuncUsesArg(ir->dst,op)) return false;
    if (isResult(op) && ir->dst && isMulDivFunc(ir->dst)) return false;
    return true;
}


static bool UsedInRange(IR *start,IR *end,Operand *reg) {
    if (!reg || !IsRegister(reg->kind)) return false;
    for (IR *ir=start; ir!=end->next; ir=ir->next) {
        if (InstrUses(ir,reg)||IsJump(ir)) return true;
        if (ir->opc == OPC_CALL) {
            if (IsArg(reg) && FuncUsesArgEx(ir->dst,reg,true)) return true;
            if (IsArg(reg)||isResult(reg)) return false; // Becomes dead
            if (!IsLocal(reg)) return true;
        }
        if (InstrModifies(ir,reg) && ir->cond==COND_TRUE) return false; // Has become dead
    }
    return false;
}

static bool ModifiedInRange(IR *start,IR *end,Operand *reg) {
    if (!reg || !IsRegister(reg->kind)) return false;
    int32_t offset = 0;
    for (IR *ir=start; ir!=end->next; ir=ir->next) {
        if (ir->cond == COND_TRUE && (ir->opc == OPC_ADD || ir->opc == OPC_SUB) && ir->dst == reg && ir->src->kind == IMM_INT) {
            offset += AddSubVal(ir);
        } else if (ir->opc == OPC_CALL) {
            if (!IsLocal(reg) && !(IsArg(reg) && !FuncUsesArg(ir->dst,reg)) && reg->kind != REG_HUBPTR && !(reg->kind == REG_REG && !strcmp(reg->name,"fp"))) return true;
        } else if (InstrModifies(ir,reg)||IsJump(ir)||IsLabel(ir)) return true;
    }
    return offset != 0;
}

static bool ReadWriteInRange(IR *start,IR *end) {
    for (IR *ir=start; ir!=end->next; ir=ir->next) {
        if (IsReadWrite(ir)) return true;
    }
    return false;
}
static bool WriteInRange(IR *start,IR *end) {
    for (IR *ir=start; ir!=end->next; ir=ir->next) {
        if (IsWrite(ir)) return true;
    }
    return false;
}

static bool FlagsChangeInRange(IR *start,IR *end,int flags) {
    for (IR *ir=start; ir!=end->next; ir=ir->next) {
        if (InstrSetsFlags(ir,flags)) return true;
    }
    return false;
}

static int MinCyclesInRange(IR *start,IR *end) {
    int cyc = 0;
    for (IR *ir=start; ir!=end->next; ir=ir->next) {
        cyc += InstrMinCycles(ir);
    }
    return cyc;
}


static void
DoReorderBlock(IRList *irl,IR *after,IR *top,IR *bottom) {
    IR *above = top->prev;
    IR *below = bottom->next;
    // Unlink block
    if (above) above->next = below;
    else irl->head = below;
    if (below) below->prev = above;
    else irl->tail = above;
    // Link block at new location
    top->prev = after;
    bottom->next = after->next;
    if (after->next) after->next->prev = bottom;
    else irl->tail = bottom;
    after->next = top;
}

// Return true for "normal" ALU ops
static bool
IsSafeALUOp(IROpcode opc) {
    switch(opc) {
    case OPC_MOV:
    case OPC_ADD:
    case OPC_SUB:
    case OPC_AND:
    case OPC_OR:
    case OPC_XOR:
    case OPC_TEST:
    case OPC_TESTN:
    case OPC_SHL:
    case OPC_SHR:
    case OPC_NEG:
    case OPC_ABS:
        return true;
    default:
        return false;
    }
}

/*
 * return TRUE if the operand's value does not need to be preserved
 * after instruction instr
 * follows branches, but gives up after MAX_FOLLOWED_JUMPS to prevent
 * infinite loops
 */
#define MAX_FOLLOWED_JUMPS 8

static bool
doIsDeadAfter(IR *instr, Operand *op, int level, IR **stack)
{
    IR *ir;
    int i;

    if (op->kind == REG_HW) {
        // hardware registers are never dead
        return false;
    }
#if 0
    if (op->kind == REG_SUBREG) {
        // cannot handle sub registers properly yet
        return false;
    }
#endif
    if (level >= MAX_FOLLOWED_JUMPS) {
        // give up!
        return false;
    }
    for (ir = instr->next; ir; ir = ir->next) {
        if (InstrUses(ir, op) && !IsDummy(ir)) {
            // value is used, so definitely not dead
            // well, unless the value is used only to update itself:
            // check for that here
            if (ir->dst != op) {
                return false;  // op used to modify something else
            }
            if (InstrSetsAnyFlags(ir)) {
                return false;  // flag setting matters, we are not dead
            }
            // be very cautious about whether op is dead if
            // any "unusal" opcodes (like waitpeq) are used
            // so just accept a safe subset
            if (!IsSafeALUOp(ir->opc)) {
                // assume live
                return false;
            }
#if 0
            if (op->kind == REG_SUBREG) {
                return false;
            }
            if (ir->dst && ir->dst->kind == REG_SUBREG) {
                return false;
            }
            if (ir->src && ir->src->kind == REG_SUBREG) {
                return false;
            }
#endif
        }
        for (i = 0; i < level; i++) {
            if (ir == stack[i]) {
                // we've come around a loop
                // registers may be considered dead, labels not
                return IsRegister(op->kind);
            }
        }
        if (ir->opc == OPC_LABEL) {
            // potential problem: if there is a branch before instr
            // that goes to LABEL then we might miss a set
            // so check
            if (ir->flags & FLAG_LABEL_NOJUMP) {
                // this label isn't a jump target, so don't worry about it
                continue;
            }
            if (!ir->next) {
                // last label in the function, again, no need to worry
                continue;
            }
            if (!ir->aux) {
                // we don't know what branches come here,
                // so for caution give up
                return false;
            }
            if (level != 0) {
                // Not sure what this check is for? But moved it out of the check loop
                continue;
            }
            for (struct ir_lbljumps *list = (struct ir_lbljumps *)ir->aux;list;list=list->next) {
                IR *comefrom = list->jump;
                if (comefrom->addr < instr->addr) {
                    // go back and see if there are any references before the
                    // jump that brought us here
                    // if so, abort
                    IR *backir = comefrom;
                    while (backir) {
                        if (backir->src == op || backir->dst == op) {
                            return false;
                        }
                        backir = backir->prev;
                    }
                }
            }
            continue;
        }
        if (InstrModifies(ir, op) && !InstrUses(ir,op) && !IsDummy(ir)) {
            // if the instruction modifies but does not use the op,
            // then we're setting it from another register and it's dead
            if (op->kind == REG_SUBREG) {
                return false;
            }
            if (ir->dst && ir->dst->kind == REG_SUBREG) {
                return false;
            }
            if (ir->cond == COND_TRUE) {
                return true;
            }
        }
        if (ir->opc == OPC_RET && ir->cond == COND_TRUE) {
            goto done;
        } else if (ir->opc == OPC_CALL && !IsDummy(ir)) {

            if (!IsLocal(op)) {
                // we know of some special cases where argN is not used
                if (IsArg(op) && !FuncUsesArg(ir->dst, op)) {
                    /* OK to continue */
                } else if (IsArg(op) && !FuncUsesArgEx(ir->dst,op,true)) {
                    return true; // Value not actually used, goes dead.
                } else if (isResult(op) && isMulDivFunc(ir->dst)) {
                    /* Result not affected by mul/div */
                } else if (isResult(op)) {
                    if (ir->cond == COND_TRUE) return true; // Results get set by functions
                } else {
                    return false;
                }
            }
        } else if (IsJump(ir) && !IsDummy(ir)) {
            // if the jump is to an unknown place give up
            stack[level] = ir; // remember where we were
            if (!ir->aux) {
                // jump to return is like running off the end
                if (ir->dst == FuncData(curfunc)->asmreturnlabel && ir->cond == COND_TRUE) {
                    goto done;
                }
                // See if this is a jump table
                if (ir->cond == COND_TRUE && ir->next && IsLabel(ir->next) && ir->next->next && (ir->next->next->flags & FLAG_JMPTABLE_INSTR)) {
                    for (IR *entry=ir->next->next; entry&&(entry->flags&FLAG_JMPTABLE_INSTR); entry=entry->next) {
                        if (!entry->aux) return false;
                        if (!doIsDeadAfter((IR *)entry->aux, op, level+1, stack)) return false;
                    }
                    return true; // All branches dead
                }
                return false;
            }
            if (!doIsDeadAfter((IR *)ir->aux, op, level+1, stack)) {
                return false;
            }
#if 1
            // be very careful about potential conditional jumps to
            // after this point, but we did check for that above
            if (ir->cond == COND_TRUE && ir->opc == OPC_JUMP) {
                return true;
            }
#endif
        }
    }
done:
    /* if we reach the end without seeing any use */
    if (isResult(op)) {
        if (op->kind != REG_RESULT) return false; // subreg or smth else
        if (op->val < curfunc->numresults) return false; // This is a defined result of this function
        return true;
    } else {
        return IsLocalOrArg(op);
    }
}

bool
IRIsDeadAfter(IR *instr, Operand *op)
{
    IR *stack[MAX_FOLLOWED_JUMPS];
    stack[0] = instr;
    return doIsDeadAfter(instr, op, 1, stack);
}

static IR*
SafeToReplaceBack(IR *instr, Operand *orig, Operand *replace)
{
    IR *last_ir = NULL;
    int usecount = 0;

    if (SrcOnlyHwReg(replace) || !IsRegister(replace->kind))
        return NULL;
    if (replace->kind == REG_SUBREG || (orig && orig->kind == REG_SUBREG) ) {
        return NULL;
    }

    struct dependency *up_labels = NULL; // Outstanding loop label list

    for (IR *ir = instr; ir; ir = ir->prev) {
        last_ir = ir;
        if (IsDummy(ir)) {
            continue;
        }
        if (ir->opc == OPC_LABEL) {
            if (ir->flags & FLAG_LABEL_NOJUMP) {
                continue;
            } else if (CheckDependency(&up_labels,ir->dst)) {
                DeleteDependencies(&up_labels,ir->dst);
                continue;
            }
            goto fail;
        }
        if (ir->opc == OPC_LIVE) {
            goto fail;
        }
        if (IsJump(ir)) {
            if (!(curfunc->optimize_flags & OPT_EXPERIMENTAL) || IsHwReg(replace)) {
                // Safety fail
                goto fail;
            } else if (IsForwardJump(ir)) {
                // Could probably also handle down labels in separate list...
                goto fail;
            } else {
                PrependDependency(&up_labels,JumpDest(ir));
            }
        }
        if (IsCallThatUsesReg(ir,orig) || IsCallThatUsesReg(ir,replace)) {
            goto fail;
        }
        // Note: InstrModifies does some weird stuff with subregisters, so check like this instead
        if (ir->dst == orig && InstrSetsDst(ir) && !InstrReadsDst(ir)) {
            if (InstrIsVolatile(ir)) goto fail;
            if (IsHwReg(replace) && ir->src && IsHwReg(ir->src) && ir->src != replace)
            {
                goto fail;
            }
            if (usecount > 0 && IsHwReg(replace) && ir->src != replace) {
                goto fail;
            }
            // this was perhaps overly cautious, but I can remember a lot of headaches
            // around optimization, so we may need to revert to it
            //return ir->cond == COND_TRUE;
            if (ir->cond == COND_TRUE && !up_labels) return ir;
        }
        if (InstrUses(ir, replace) || ir->dst == replace) {
            goto fail;
        }
        if (IsHwReg(replace) && (InstrUses(ir, orig) || InstrModifies(ir, orig))) {
            if (usecount > 0) {
                goto fail;
            }
            usecount++;
        }
    }
    // we've reached the start
    // is orig dead here? if so we can replace it
    // args are live, and so are globals
    if (!up_labels && IsLocal(orig)) return last_ir;
    // Fall into fail case
fail:
    DeleteDependencyList(&up_labels);
    return NULL;
}

//
// returns true if orig is a source only
// hardware register (CNT, INA, INB)
//
bool
SrcOnlyHwReg(Operand *orig)
{
    if (orig->kind != REG_HW)
        return false;
    if (orig->val != 0) {
        return true;
    }
    if (!strcasecmp(orig->name, "CNT")
            || !strcasecmp(orig->name, "PHSA") // Not actually source-only, but dest reads last written value
            || !strcasecmp(orig->name, "PHSB") // ^^
            || !strcasecmp(orig->name, "INA")
            || !strcasecmp(orig->name, "INB"))
    {
        return true;
    }
    return false;
}

//
// see if we can replace "orig" with "replace" in code after first_ir
// returns the IR where we should stop scanning for replacement,
// or NULL if replacement is unsafe
//
static IR*
SafeToReplaceForward(IR *first_ir, Operand *orig, Operand *replace, IRCond setterCond)
{
    IR *ir;
    IR *last_ir = NULL;
    bool assignments_are_safe = true;
    bool orig_modified = false;
    bool isCond = (setterCond != COND_TRUE);
    bool condition_safe = true;

    if (SrcOnlyHwReg(replace) || !IsRegister(replace->kind)) {
        return NULL;
    }
    if (replace->kind == REG_SUBREG) {
        return NULL;
    }
    if (orig && orig->kind == REG_SUBREG) {
        return NULL;
    }
    if (!first_ir) {
        return NULL;
    }
#if 1
    // special case: if orig is dead after this,
    // and if first_ir does not modify it, then it is safe to
    // replace
    if (!IsBranch(first_ir) && IsDeadAfter(first_ir, orig) && !InstrModifies(first_ir, orig) && !isCond) {
        return first_ir;
    }
#endif
    for (ir = first_ir; ir; ir = ir->next) {
        if (IsDummy(ir)) {
            continue;
        }
#if 1
        // paranoia: subregisters are more complicated than we realize,
        // so be careful if any are in use
        if (ir->src && ir->src->kind == REG_SUBREG) {
            assignments_are_safe = false;
        }
#endif
        if (ir->opc == OPC_LIVE && !strcmp(orig->name, ir->dst->name)) {
            return NULL;
        }
        if (ir->opc == OPC_LIVE && !strcmp(replace->name, ir->dst->name)) {
            return NULL;
        }
        if (ir->opc == OPC_RET) {
            return (IsLocalOrArg(orig) && !isCond) ? ir : NULL;
        } else if (ir->opc == OPC_CALL) {
            // it's OK to replace forward over a call as long
            // as orig is a local register (not an ARG!)
            if (IsArg(orig)) {
                if (FuncUsesArg(ir->dst, orig)) {
                    return NULL;
                }
            } else if (!IsLocal(orig)) {
                return NULL;
            }
            if (IsArg(replace)) {
                if (FuncUsesArg(ir->dst, replace)) {
                    // if there are any more references to orig then
                    // replacement will fail (since arg gets changed
                    // by the call)
                    return (assignments_are_safe && IsDeadAfter(ir, orig) && !isCond) ? ir : NULL;
                }
            } else if (!IsLocal(replace)) {
                return NULL;
            }
        } else if (IsJump(ir)) {
            // forward branches (or branches to code we've
            // already seen) are safe; others we should assume
            // will cause problems
            // Note though that if we do branch ahead then
            // we cannot assume that assignments are safe!
            if (ir->aux && IsDeadAfter((IR *)ir->aux,replace) && IsDeadAfter((IR *)ir->aux,orig)) {
                // both regs are dead after branch, so we don't care
            } else {
                if (!JumpIsAfterOrEqual(first_ir, ir)) {
                    return NULL;
                }
                if (assignments_are_safe && IsForwardJump(ir) && ir->aux) {
                    IR *jmpdst = (IR *)ir->aux;
                    assignments_are_safe = IsDeadAfter(jmpdst, orig);
                } else {
                    assignments_are_safe = false;
                }
            }
        }
        if (ir->opc == OPC_LABEL) {
            if (ir->flags & FLAG_LABEL_NOJUMP) {
                // this label is not a jump target, so ignore it
                continue;
            }
            // do we know who jumps to this label?
            if (ir->aux) {
                // if a jumper is before our first instruction, then
                // we don't know what may have happened before, so
                // replacement is dangerous
                // however, in the special case that the register is never
                // actually used again then it's safe
                bool jumpers_before = false;
                for(struct ir_lbljumps *list = (struct ir_lbljumps *)ir->aux;list;list=list->next) {
                    if (list->jump->addr < first_ir->addr) {
                        jumpers_before = true;
                    }
                }
                if (jumpers_before) {
                    if (assignments_are_safe && IsDeadAfter(ir, orig) && !isCond) {
                        return ir;
                    }
                    return NULL;
                }
                assignments_are_safe = false;
            } else {
                // unknown jumpers, assume the worst
                return NULL;
            }
        }
        if ((!condition_safe || !CondIsSubset(setterCond,ir->cond)) && InstrUses(ir,orig)) {
            return NULL;
        }
        if (InstrModifies(ir,replace)) {
            // special case: if we have a "mov replace,x" and orig is dead
            // then we are good to go; at that point we know it is safe to replace
            // orig with replace because:
            //  (a) orig is dead after this, so not used
            //  (b) whatever we did to replace up til now it doesn't matter, a fresh
            //      value is being put into it
            //  if "assignments_are_safe" is false then we don't know if another
            //  branch might still use "replace", so punt and give up
            if (ir->cond != COND_TRUE && (!condition_safe || !CondIsSubset(ir->cond,setterCond))) {
                return NULL;
            }
            if (ir->dst->kind == REG_SUBREG || replace->kind == REG_SUBREG) {
                // sub register usage is problematic
                return NULL;
            }
            if (!assignments_are_safe) {
                return NULL;
            }
            if (!InstrUses(ir, replace) && IsDeadAfter(ir, orig)) {
                return ir;
            }
            if (!orig_modified && last_ir && IsDeadAfter(last_ir, orig)) {
                // orig never actually got changed, and neither did replace (up
                // until now) so we can do the replacement
                return last_ir;
            }
            return NULL;
        }
        if (InstrModifies(ir, orig)) {
            if (ir->src == orig && ir->srceffect != OPEFFECT_NONE) {
                return NULL;
            }
            if (ir->dst == orig && ir->dsteffect != OPEFFECT_NONE) {
                return NULL;
            }
            if (ir->dst->kind == REG_SUBREG) {
                // sub registers are complicated, punt
                return NULL;
            }
            if (!condition_safe) {
                // Can't prove that ir->cond is equal / a subset anymore, thus skip below branch.
                return NULL;
            } else if (ir->cond != setterCond) {
                assignments_are_safe = false;
                if (!CondIsSubset(setterCond,ir->cond)) {
                    // Not a subset of the setter condition, can't replace
                    return NULL;
                }
            }
            if (!InstrUses(ir, orig) && assignments_are_safe) {
                // we are completely re-setting "orig" here, so we can just
                // leave now
                return last_ir;
            }
            // we do not want to end accidentally modifying "replace" if it is still live
            // note that IsDeadAfter(first_ir, replace) gives a more accurate
            // view than IsDeadAfter(ir, replace), because it can look back
            // further (and we've already verified that replace is not doing
            // anything interesting between first_ir and here)
            if (!IsDeadAfter(first_ir, replace)) {
                return NULL;
            }
            orig_modified = true;
        }
        if (isCond && InstrSetsFlags(ir,FlagsUsedByCond(setterCond))) {
            // Setters condition is no longer valid.
            // Technically, condition is still safe if it's logically AND-ed, i.e "if_z test x,y wz",
            // but we're not going there now.
            condition_safe = false;
        }
        last_ir = ir;
    }
    return IsLocalOrArg(orig) ? last_ir : NULL;
}


static void
ReplaceBack(IR *instr, Operand *orig, Operand *replace, IR *stop_ir)
{
    IR *ir;
    for (ir = instr; ir; ir = ir->prev) {
        if (ir->dst == orig) ir->dst = replace;
        if (ir == stop_ir) break;
        if (ir->src == orig) ir->src = replace;
    }
}

static void
ReplaceForward(IR *instr, Operand *orig, Operand *replace, IR *stop_ir)
{
    IR *ir;
    for (ir = instr; ir; ir = ir->next) {
        if (ir->src == orig) {
            ir->src = replace;
        }
        if (ir->dst == orig) {
            ir->dst = replace;
        }
        if (ir == stop_ir) break;
    }
}

//
// Apply a new condition code based on cval/zval being compared to 0
// returns true if successful (does not neccessarily indicate change occurence, use changeout)
//
bool
ApplyConditionAfter(IRList *irl, IR *instr, int cval, int zval, int *changeout)
{
    IR *ir;
    unsigned newcond;
    bool change = false;
    bool invalid_instr = false;
    int setz = instr->flags & FLAG_WZ;
    int setc = instr->flags & FLAG_WC;
    cval = cval  < 0 ? 2 : 0;
    zval = zval == 0 ? 1 : 0;

    for (ir = instr->next; ir; ir = ir->next) {
        if (IsDummy(ir)) continue;
        newcond = (unsigned)ir->cond;

        // Bit-magic on the condition values.
        // (Basically, take the half that's selected by the constant flag
        // and replicate it into the other half)
        if (setc) newcond = ((newcond >> cval)&0b0011)*0b0101;
        if (setz) newcond = ((newcond >> zval)&0b0101)*0b0011;

        if (ir->cond != (IRCond)newcond) {
            change = 1;
            ir->cond = (IRCond)newcond;
        }
        switch(ir->opc) {
        case OPC_NEGC :
            if (setc) {
                ReplaceOpcode(ir, cval ? OPC_NEG  : OPC_MOV );
                change=1;
            }
            break;
        case OPC_NEGNC:
            if (setc) {
                ReplaceOpcode(ir,!cval ? OPC_NEG  : OPC_MOV );
                change=1;
            }
            break;
        case OPC_NEGZ :
            if (setz) {
                ReplaceOpcode(ir, zval ? OPC_NEG  : OPC_MOV );
                change=1;
            }
            break;
        case OPC_NEGNZ:
            if (setz) {
                ReplaceOpcode(ir,!zval ? OPC_NEG  : OPC_MOV );
                change=1;
            }
            break;
        case OPC_SUMC :
            if (setc) {
                ReplaceOpcode(ir, cval ? OPC_SUB  : OPC_ADD );
                change=1;
            }
            break;
        case OPC_SUMNC:
            if (setc) {
                ReplaceOpcode(ir,!cval ? OPC_SUB  : OPC_ADD );
                change=1;
            }
            break;
        case OPC_SUMZ :
            if (setz) {
                ReplaceOpcode(ir, zval ? OPC_SUB  : OPC_ADD );
                change=1;
            }
            break;
        case OPC_SUMNZ:
            if (setz) {
                ReplaceOpcode(ir,!zval ? OPC_SUB  : OPC_ADD );
                change=1;
            }
            break;
        case OPC_MUXC :
            if (setc) {
                ReplaceOpcode(ir, cval ? OPC_OR   : OPC_ANDN);
                change=1;
            }
            break;
        case OPC_MUXNC:
            if (setc) {
                ReplaceOpcode(ir,!cval ? OPC_OR   : OPC_ANDN);
                change=1;
            }
            break;
        case OPC_MUXZ :
            if (setz) {
                ReplaceOpcode(ir, zval ? OPC_OR   : OPC_ANDN);
                change=1;
            }
            break;
        case OPC_MUXNZ:
            if (setz) {
                ReplaceOpcode(ir,!zval ? OPC_OR   : OPC_ANDN);
                change=1;
            }
            break;
        case OPC_DRVC :
            if (setc) {
                ReplaceOpcode(ir, cval ? OPC_DRVH : OPC_DRVL);
                change=1;
            }
            break;
        case OPC_DRVNC:
            if (setc) {
                ReplaceOpcode(ir,!cval ? OPC_DRVH : OPC_DRVL);
                change=1;
            }
            break;
        case OPC_DRVZ :
            if (setz) {
                ReplaceOpcode(ir, zval ? OPC_DRVH : OPC_DRVL);
                change=1;
            }
            break;
        case OPC_DRVNZ:
            if (setz) {
                ReplaceOpcode(ir,!zval ? OPC_DRVH : OPC_DRVL);
                change=1;
            }
            break;

        case OPC_WRC  :
            if (setc) {
                ReplaceOpcode(ir,OPC_MOV);
                ir->src=NewImmediate( cval?1:0);
                change = 1;
            }
            break;
        case OPC_WRNC :
            if (setc) {
                ReplaceOpcode(ir,OPC_MOV);
                ir->src=NewImmediate(!cval?1:0);
                change = 1;
            }
            break;
        case OPC_WRZ  :
            if (setz) {
                ReplaceOpcode(ir,OPC_MOV);
                ir->src=NewImmediate( zval?1:0);
                change = 1;
            }
            break;
        case OPC_WRNZ :
            if (setz) {
                ReplaceOpcode(ir,OPC_MOV);
                ir->src=NewImmediate(!zval?1:0);
                change = 1;
            }
            break;

        case OPC_RCL:
        case OPC_RCR:
            if (setc) {
                bool is_rcr = (ir->opc == OPC_RCR);
                if (cval) {
                    if (IsImmediate(ir->src) && !InstrSetsAnyFlags(ir) && !IsHwReg(ir->dst)) {
                        // generate new OR instruction if shift is constant (might turn to BITH on P2)
                        ReplaceOpcode(ir,is_rcr? OPC_SHR : OPC_SHL);
                        IR *orinst = NewIR(OPC_OR);
                        orinst->dst = ir->dst;
                        uint32_t mask = (1<<(ir->src->val & 31)) - 1;
                        if (is_rcr) mask <<= ((32 - ir->src->val) & 31);
                        orinst->src = NewImmediate(mask);
                        orinst->cond = ir->cond;
                        InsertAfterIR(irl,ir,orinst);
                        change = 1;
                    } else {
                        invalid_instr = 1;
                    }
                } else {
                    ReplaceOpcode(ir,is_rcr? OPC_SHR : OPC_SHL);
                    change = 1;
                }
            }
            break;
        case OPC_ADDSX:
        case OPC_SUBSX:
            if (setc) {
                // ADDS/SUBS don't have OPCs????
                invalid_instr = 1;
            }
            break;
        case OPC_ADDX:
        case OPC_SUBX:
            if (setc) {
                bool is_sub = (ir->opc == OPC_SUBX);
                if (ir->flags & FLAG_WZ) {
                    invalid_instr = 1;
                } else if (!cval) {
                    ReplaceOpcode(ir,is_sub ? OPC_SUB : OPC_ADD);
                    change = 1;
                } else if (cval && IsImmediate(ir->src)) {
                    if (IsImmediateVal(ir->src,UINT32_MAX)) {
                        // This instruction does nothing and can be deleted
                        ir->cond = COND_FALSE;
                    } else {
                        ReplaceOpcode(ir,is_sub ? OPC_SUB : OPC_ADD);
                        ir->src = NewImmediate(ir->src->val + 1);
                    }
                    change = 1;
                } else {
                    invalid_instr = 1;
                }
            }
            break;
        default:
            if (InstrUsesFlags(ir,setc|setz)) {
                invalid_instr = 1;
                WARNING(NULL,"Internal warning. Couldn't ApplyConditionAfter");
            }
            break;
        }

        if (ir->cond != COND_FALSE) {
            if(InstrSetsFlags(ir,FLAG_WC)) setc = 0;
            if(InstrSetsFlags(ir,FLAG_WZ)) setz = 0;
        }

        if (!setc && !setz) break;
    }
    if (changeout && !*changeout) *changeout = change;
    return !invalid_instr;
}

static bool
SameImmediate(Operand *a, Operand *b)
{
    if (a->kind != IMM_INT || b->kind != IMM_INT)
        return 0;
    return a->val == b->val;
}

bool
SameIROperand(Operand *a, Operand *b)
{
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return false;
    }
    return SameImmediate(a, b);
}

// Note: Intentionally doesn't check condition
static bool
SameIR(IR *a, IR *b) {
    if (!a || !b) {
        return false;
    }
    if (a == b) {
        return true;
    }
    if (a->opc >= OPC_GENERIC) return false;
    if (a->opc != b->opc) return false;
    if (a->flags != b->flags) return false;
    if (!SameOperand(a->src,b->src)) return false;
    if (!SameOperand(a->src2,b->src2)) return false;
    if (!SameOperand(a->dst,b->dst)) return false;
    if (a->srceffect != b->srceffect) return false;
    if (a->dsteffect != b->dsteffect) return false;

    return true;
}

// try to transform an operation with a destination that is
// known to be the constant "imm"
// if the src is also constant, convert it to a move immediate
// returns 1 if there is a change
static int
TransformConstDst(IRList *irl, IR *ir, Operand *imm)
{
    int32_t val1, val2, cval;
    int setsResult = 1;

    if (gl_p2 && (!InstrSetsDst(ir) || (ir->opc == OPC_LOCKREL && IsDeadAfter(ir,ir->dst)))) {
        // this may be a case where we can replace dst with src
        if (ir->instr) {
            switch (ir->instr->ops) {
            case P2_DST_CONST_OK:
            case P2_TWO_OPERANDS:
            case P2_RDWR_OPERANDS:
                ir->dst = imm;
                return 1;
            default:
                break;
            }
        }
    }
    if (ir->src == NULL) {
        return 0;
    }
    if (imm->kind == IMM_INT && imm->val == 0) {
        // transform add foo, bar into mov foo, bar, if foo is known to be 0
        if ((ir->opc == OPC_ADD || ir->opc == OPC_SUB || ir->opc == OPC_OR || ir->opc == OPC_XOR)
                && (ir->flags == FLAG_WZ || !InstrSetsAnyFlags(ir)))
        {
            if (ir->opc == OPC_SUB) {
                ReplaceOpcode(ir, OPC_NEG);
            } else {
                ReplaceOpcode(ir, OPC_MOV);
            }
            return 1;
        }
    }
    if (ir->src->kind != IMM_INT) {
        return 0;
    }
    if (imm->kind == IMM_COG_LABEL && ir->opc == OPC_ADD && ir->flags == 0) {
        Operand *newlabel = NewOperand(IMM_COG_LABEL, imm->name, imm->val + ir->src->val);
        ir->src = newlabel;
        ReplaceOpcode(ir, OPC_MOV);
        return 1;
    }
    if (imm->kind != IMM_INT) {
        return 0;
    }
    if (ir->flags & FLAG_WC) {
        // we don't know how to set the WC flag for anything other
        // than cmps/cmp/sub
        if (ir->opc != OPC_CMPS && ir->opc != OPC_CMP && ir->opc != OPC_SUB) {
            return 0;
        }
    }

    val1 = imm->val;
    val2 = ir->src->val;
    cval = 0;

    switch (ir->opc)
    {
    case OPC_ADD:
        val1 += val2;
        break;
    case OPC_TEST:
        setsResult = false;
    // fall through
    case OPC_AND:
        val1 &= val2;
        break;
    case OPC_OR:
        val1 |= val2;
        break;
    case OPC_XOR:
        val1 ^= val2;
        break;
    case OPC_TESTN:
        setsResult = false;
    // fall through
    case OPC_ANDN:
        val1 &= ~val2;
        break;
    case OPC_SHL:
        val1 = val1 << (val2&31);
        break;
    case OPC_SAR:
        val1 = val1 >> (val2&31);
        break;
    case OPC_SHR:
        val1 = ((uint32_t)val1) >> (val2&31);
        break;
    case OPC_ZEROX:
        val2 = 31-(val2&31);
        val1 = ((uint32_t)val1<<val2)>>val2;
        break;
    case OPC_SIGNX:
        val2 = 31-(val2&31);
        val1 = ((int32_t)val1<<val2)>>val2;
        break;
    case OPC_CMPS:
        val1 -= val2;
        cval = val1;
        setsResult = 0;
        break;
    case OPC_CMP:
        setsResult = 0;
    case OPC_SUB:
        if ((uint32_t)val1 < (uint32_t)val2) {
            cval = -1;
        } else if (val1 == val2) {
            cval = 0;
        } else {
            cval = 1;
        }
        val1 -= val2;
        break;
    case OPC_TESTBN:
        setsResult = false;
        // Z is set if bit is CLEAR
        val1 = val1 & (1<<(val2&31));
        break;
    case OPC_TESTB:
        setsResult = false;
        // Z is set if bit is SET
        val1 = ~val1 & (1<<(val2&31));
        break;
    case OPC_BITH:
        val1 |= EvalP2BitMask(val2);
        break;
    case OPC_BITL:
        val1 &= ~EvalP2BitMask(val2);
        break;
    case OPC_BITNOT:
        val1 ^= EvalP2BitMask(val2);
        break;
    default:
        return 0;
    }
    if (InstrSetsAnyFlags(ir)) {
        int condchange = 0;
        if (!ApplyConditionAfter(irl,ir, cval, val1, &condchange)) {
            return condchange; // Couldn't apply condition :(
        }
        ir->flags &= !FLAG_CZSET;
    }
    if (setsResult) {
        if (val1 < 0) {
            ReplaceOpcode(ir, OPC_NEG);
            ir->src = NewImmediate(-val1);
        } else {
            ReplaceOpcode(ir, OPC_MOV);
            ir->src = NewImmediate(val1);
        }
    } else {
        ir->cond = COND_FALSE;
    }
    return 1;
}

static bool
IsOnlySetterFor(IRList *irl, IR *orig_ir, Operand *orig)
{
    IR *ir;
    if (!IsLocal(orig)) {
        return false;
    }
    for (ir = irl->head; ir; ir = ir->next) {
        if (IsDummy(ir) || IsLabel(ir)) {
            continue;
        }
        if (ir != orig_ir && InstrModifies(ir, orig)) {
            return false;
        }
    }
    return true;
}

//
// if we see x:=2 then replace future uses of x with 2
// if orig_ir is the only setter for orig in the whole irl,
// then we can do this unconditionally; otherwise we have
// to beware of jumps and labels
//
static int
PropagateConstForward(IRList *irl, IR *orig_ir, Operand *orig, Operand *immval)
{
    IR *ir;
    int change = 0;
    bool unconditional;
    int32_t tmp;
    if (SrcOnlyHwReg(orig)) return 0;
    if (!isConstMove(orig_ir,&tmp)) ERROR(NULL,"isConstMove == false in PropagateConstForward");
    if (immval->val != tmp) {
        immval = NewImmediate(tmp);
    }

    unconditional = IsOnlySetterFor(irl, orig_ir, orig);
    for (ir = orig_ir->next; ir; ir = ir->next) {
        if (IsDummy(ir)) {
            continue;
        }
        if (IsLabel(ir) && !unconditional) {
            return change;
        }
        if (ir->opc == OPC_CALL) {
            if (!unconditional || !IsLocal(orig)) {
                return change;
            }
        }
        if (IsJump(ir) && !JumpIsAfterOrEqual(orig_ir, ir)) {
            if (unconditional) {
                continue;
            }
            return change;
        }
        if (!InstrIsVolatile(ir)) {
            if (ir->opc == OPC_MOV && !InstrSetsAnyFlags(ir) && SameImmediate(ir->src, immval)) {
                if ( ir->dst == orig ) {
                    // updating same register, so kill it
                    ir->opc = OPC_DUMMY;
                    change = 1;
                } else {
                    // it would be nice here to substitute forward the
                    // register "dst" with "orig", so as to eliminate some
                    // redundant register usage; but my original attempt to
                    // do this ran into infinite loops, so putting that on hold
                    // for now
                }
            } else if (ir->dst == orig) {
                // we can perhaps replace the operation with a mov
                change |= TransformConstDst(irl, ir, immval);
            } else if (ir->src == orig) {
                ir->src = immval;
                change = 1;
            }
        }
        if (InstrModifies(ir, orig)) {
            // our register has changed, so we must stop
            return change;
        }
    }
    return change;
}

static bool
DeleteMulDivSequence(IRList *irl, IR *ir, Operand *lastop, Operand *opa, Operand *opb, IR *lastir)
{
    IR *ir2, *ir3;

    // ir is pointing at the mov muldiva, opa instruction
    ir2 = ir->next;
    if (!ir2) return false;
    ir3 = ir2->next;
    if (!ir3) return false;
    if (ir2->opc != OPC_MOV) return false;
    if (ir2->dst != muldivb || !SameOperand(ir2->src, opb)) return false;
    if (ir3->opc != OPC_CALL) return false;
    if (ir3->dst != lastop) {
        // There is a case here where we might want to
        // merge "multiply" (getting low word) and "unsigned multiply"
        // (getting high word)
        // not sure how to do that yet...
        if (lastop == unsmulfunc && ir3->dst == mulfunc && lastir) {
            lastir->dst = ir3->dst;
        } else {
            return false;
        }
    }
    ir->opc = OPC_DUMMY;
    ir2->opc = OPC_DUMMY;
    ir3->opc = OPC_DUMMY;
    return true;
}

int
OptimizeMulDiv(IRList *irl)
{
    // known operands (NULL if not known)
    Operand *opa = NULL;   // first operand to multiply or divide
    Operand *opb = NULL;  // second operand to multiply or divide
    Operand *lastop = NULL;
    IR *ir;
    IR *lastir = NULL;
    int change = 0;
    int hiresult_used = 0;

    ir = irl->head;
    while (ir != 0) {
        if (IsLabel(ir)) {
            opa = opb = lastop = NULL;
            hiresult_used = 0;
        } else if (IsDummy(ir)) {
            // do nothing
        } else if (InstrModifies(ir, muldiva)) {
            if (ir->opc == OPC_MOV && ir->cond == COND_TRUE) {
                // this may be a particular sequence:
                //   mov muldiva, opa
                //   mov muldivb, opb
                //   call #function
                // if so, see if we have just done that sequence previously
                // so that the proper results are already in their
                // registers
                if (opa == ir->src && DeleteMulDivSequence(irl, ir, lastop, opa, opb, hiresult_used ? 0 : lastir)) {
                    change = 1;
                } else {
                    opa = ir->src;
                    opb = NULL;
                    lastop = NULL;
                    hiresult_used = 0;
                }
            } else {
                opa = opb = lastop = NULL;
            }
        } else if (InstrModifies(ir, muldivb)) {
            if (ir->opc == OPC_MOV) {
                opb = ir->src;
            } else if (InstrSetsDst(ir)) {
                opb = NULL;
                hiresult_used = 0;
            }
        } else if (InstrUses(ir, muldivb)) {
            hiresult_used = 1;
        } else if (opa && InstrModifies(ir, opa)) {
            opa = NULL;
        } else if (opb && InstrModifies(ir, opb)) {
            opb = NULL;
            hiresult_used = 0;
        } else if (ir->opc == OPC_CALL) {
            if (isMulDivFunc(ir->dst)) {
                lastir = ir;
                lastop = lastir->dst;
                hiresult_used = 0;
            } else {
                lastop = NULL;
                hiresult_used = 1;
            }
        }
        ir = ir->next;
    }
    return change;
}


/* return 1 if the instruction can have wz appended and produce a sensible
 * result (compares result to 0)
 */
static int
CanTestZero(int opc)
{
    switch (opc) {
    case OPC_ADD:
    case OPC_SUB:
    case OPC_AND:
    case OPC_ANDN:
    case OPC_OR:
    case OPC_MOV:
    case OPC_NEG:
    case OPC_NEGC:
    case OPC_NEGNC:
    case OPC_NEGZ:
    case OPC_NEGNZ:
    case OPC_SUMC:
    case OPC_SUMNC:
    case OPC_SUMZ:
    case OPC_SUMNZ:
    case OPC_MUXC:
    case OPC_MUXNC:
    case OPC_MUXZ:
    case OPC_MUXNZ:
    case OPC_RDLONG:
    case OPC_RDBYTE:
    case OPC_RDWORD:
    case OPC_XOR:
    case OPC_SAR:
    case OPC_SHR:
    case OPC_SHL:
    case OPC_SIGNX:
    case OPC_ZEROX:
    case OPC_MULU:
    case OPC_MULS:
        return 1;
    default:
        return 0;
    }
}

//
// find the previous instruction that sets a particular operand
// used for compares
// returns NULL if we cannot
//
static IR*
FindPrevSetterForCompare(IR *irl, Operand *dst)
{
    IR *ir;
    int orig_condition = irl->cond;

    if (SrcOnlyHwReg(dst))
        return NULL;
    for (ir = irl->prev; ir; ir = ir->prev) {
        if (IsDummy(ir)) {
            continue;
        }
        if (ir->opc == OPC_LABEL) {
            // we may have branched to here from somewhere
            // else that did the set
            return NULL;
        }
        if (ir->cond != orig_condition) {
            if (ir->dst == dst) {
                return NULL;
            }
        }
        if (InstrSetsAnyFlags(ir)) {
            // flags are messed up here, so we can't go back any further
            return NULL;
        }
        if (IsBranch(ir)) {
            return NULL;
        }
        if (ir->dst == dst && InstrSetsDst(ir)) {
            if (ir->cond != COND_TRUE) {
                // cannot be sure that we set the value here,
                // since the set is conditional
                return NULL;
            }
            return ir;
        }
    }
    return NULL;
}

//
// find the previous instruction that changes a particular operand
// also will return if it's in the dst field of a TEST
//
// used for peephole optimization
// returns NULL if we cannot find the instruction
//
static IR*
FindPrevSetterForReplace(IR *irorig, Operand *dst)
{
    IR *ir;
    IR *saveir;

#if 0
    // Sanity check: if the original instruction is conditional then
    // bail (the caller should have already checked for this, but
    // it shouldn't hurt to do it again).
    // FIXME? actually it does hurt, it blocks an optimization we want
    // (the if_b neg -> subx optimization
    if (irorig->cond != COND_TRUE) {
        return NULL;
    }
#endif
    if (SrcOnlyHwReg(dst))
        return NULL;
    for (ir = irorig->prev; ir; ir = ir->prev) {
        if (IsDummy(ir)) {
            continue;
        }
        if (ir->opc == OPC_LABEL) {
            // we may have branched to here from somewhere
            // else that did the set
            return NULL;
        }
        if (IsJump(ir)) {
            return NULL;
        }
        if (IsCallThatUsesReg(ir,dst)) {
            return NULL;
        }
        if (ir->dst == dst && (InstrSetsDst(ir) || ir->opc == OPC_TEST || ir->opc == OPC_TESTBN)) {
            if (ir->cond != COND_TRUE) {
                // cannot be sure that we set the value here,
                // since the set is conditional
                return NULL;
            }
            break;
        }
        if (ir->src == dst || ir->dst == dst) {
            // we cannot replace the setter, it's in use
            return NULL;
        }
    }
    if (!ir) {
        return NULL;
    }
    // OK, now go forward and make sure ir->src is not changed
    // until irorig
    saveir = ir;
    ir = ir->next;
    if (ModifiedInRange(ir, irorig->prev, saveir->src)) {
        return false;
    }
    return saveir;
}

//
// check for flags used between ir1 and ir2
//
static int
FlagsNotUsedBetween(IR *ir1, IR *ir2, unsigned flags)
{
    while (ir1 && ir1 != ir2) {
        if (InstrUsesFlags(ir1, flags)) {
            return 0;
        }
        ir1 = ir1->next;
    }
    if (!ir1) {
        return 0;
    }
    return 1;
}

//
// check for flags set between ir1 and ir2
//
static int
FlagsNotSetBetween(IR *ir1, IR *ir2, unsigned flags)
{
    while (ir1 && ir1 != ir2) {
        if (InstrSetsFlags(ir1, flags)) {
            return 0;
        }
        ir1 = ir1->next;
    }
    if (!ir1) {
        return 0;
    }
    return 1;
}

int
OptimizeMoves(IRList *irl)
{
    IR *ir;
    IR *ir_next;
    IR *stop_ir;
    int change;
    int everchange = 0;
    int32_t cval;

    do {
        change = 0;
        ir = irl->head;
        while (ir) {
            ir_next = ir->next;
            if (InstrIsVolatile(ir)) {
                /* do nothing */
            } else if (ir->opc == OPC_MOV && ir->src == ir->dst) {
                if (!InstrSetsAnyFlags(ir)) {
                    DeleteIR(irl, ir);
                    change = 1;
                } else if (ir->flags == FLAG_WZ) {
                    IR *prev_ir = FindPrevSetterForCompare(ir,ir->src);
                    if (prev_ir && CanTestZero(prev_ir->opc)
                            && (prev_ir->flags&(FLAG_ZSET&~FLAG_WZ)) == 0
                            && prev_ir->cond == COND_TRUE && ir->cond == COND_TRUE
                            && FlagsNotUsedBetween(prev_ir,ir,FLAG_WZ)
                            && FlagsNotSetBetween(prev_ir,ir,FLAG_WZ)) {
                        prev_ir->flags |= FLAG_WZ;
                        DeleteIR(irl, ir);
                        change = 1;
                    }
                }
            } else if (ir->cond == COND_TRUE && isConstMove(ir,&cval)) {
                int sawchange;
                if (ir->flags == FLAG_WZ && CanTestZero(ir->opc)) {
                    // because this is a mov immediate, we know how
                    // WZ will be set
                    if (ApplyConditionAfter(irl, ir, cval, cval, &change)) {
                        ir->flags &= ~FLAG_CZSET;
                        change |= 1;
                    }
                } else if (ir->flags != 0 && ir->opc == OPC_ABS) {
                    if (ApplyConditionAfter(irl, ir, ir->src->val, ir->src->val, &change)) {
                        ir->flags &= ~FLAG_CZSET;
                        change |= 1;
                    };
                }
                change |= (sawchange = PropagateConstForward(irl, ir, ir->dst, ir->src));
                if (sawchange && !InstrSetsAnyFlags(ir) && IsDeadAfter(ir, ir->dst)) {
                    // we no longer need the original mov
                    DeleteIR(irl, ir);
                }
            } else if (ir->opc == OPC_MOV) {
                if (ir->src == ir->dst && !InstrSetsAnyFlags(ir)) {
                    DeleteIR(irl, ir);
                    change = 1;
                } else if (!InstrSetsAnyFlags(ir) && ir->cond == COND_TRUE && IsDeadAfter(ir, ir->src) && (stop_ir = SafeToReplaceBack(ir->prev, ir->src, ir->dst))) {
                    ReplaceBack(ir->prev, ir->src, ir->dst, stop_ir);
                    DeleteIR(irl, ir);
                    change = 1;
                }
                else if ( !InstrSetsAnyFlags(ir) && 0 != (stop_ir = SafeToReplaceForward(ir->next, ir->dst, ir->src,ir->cond)) ) {
                    ReplaceForward(ir->next, ir->dst, ir->src, stop_ir);
                    DeleteIR(irl, ir);
                    change = 1;
                }
            } else if (isMoveLikeOp(ir) && (stop_ir = FindPrevSetterForReplace(ir,ir->src)) && stop_ir->opc == OPC_MOV
                       && !InstrIsVolatile(stop_ir) && !InstrSetsAnyFlags(stop_ir) && (ir->src==ir->dst||IsDeadAfter(ir,ir->src))) {
                ir->src = stop_ir->src;
                if (ir->cond == COND_TRUE) DeleteIR(irl,stop_ir);
                change = 1;
            }
            ir = ir_next;
        }
        everchange |= change;
    } while (change && 0);
    return everchange;
}

// Are given flag bits currently in use at this IR?
static unsigned
FlagsUsedAt(IR *ir,unsigned flags) {
    for (IR *irnext = ir; irnext; irnext = irnext->next) {
        if (InstrUsesFlags(irnext, flags))  return true;
        if (InstrIsVolatile(irnext)) return true;
        if (irnext->cond == COND_TRUE && IsBranch(irnext)) break;
        if (irnext->cond == COND_TRUE && InstrSetsFlags(irnext,flags)) break;
    }
    return false;
}

static bool
HasUsedFlags(IR *ir) {
    if (InstrSetsAnyFlags(ir)) {
        // if the flags might possibly be used, we have to assume there
        // are side effects
        return FlagsUsedAt(ir->next,CanonizeFlags(ir->flags));
    }
    return false;
}

static bool
HasSideEffectsOtherThanReg(IR *ir)
{
    if ( (ir->dst && ir->dst->kind == REG_HW)
            || (ir->src && ir->src->kind == REG_HW) )
    {
        return true;
    }
    if (ir->dsteffect != OPEFFECT_NONE || ir->srceffect != OPEFFECT_NONE) {
        return true;
    }
    if (IsBranch(ir)) {
        if (ir->opc == OPC_CALL && isMulDivFunc(ir->dst)) {
            return false;
        }
        return true;
    }
    if (HasUsedFlags(ir)) return true;
    if (InstrIsVolatile(ir)) {
        return true;
    }
    switch (ir->opc) {
    case OPC_GENERIC:
    case OPC_GENERIC_NR:
    case OPC_GENERIC_DELAY:
    case OPC_GENERIC_NOFLAGS:
    case OPC_GENERIC_NR_NOFLAGS:
    case OPC_LOCKCLR:
    case OPC_LOCKNEW:
    case OPC_LOCKRET:
    case OPC_LOCKSET:
    case OPC_LOCKTRY:
    case OPC_LOCKREL:
    case OPC_SETQ:
    case OPC_SETQ2:
    case OPC_WAITCNT:
    case OPC_WAITX:
    case OPC_WRBYTE:
    case OPC_WRLONG:
    case OPC_WRWORD:
    case OPC_COGSTOP:
    case OPC_COGID:
    case OPC_ADDCT1:
    case OPC_HUBSET:
    case OPC_QDIV:
    case OPC_QFRAC:
    case OPC_QMUL:
    case OPC_QROTATE:
    case OPC_QSQRT:
    case OPC_QVECTOR:
    case OPC_QLOG:
    case OPC_QEXP:
    case OPC_DRVC:
    case OPC_DRVNC:
    case OPC_DRVL:
    case OPC_DRVH:
    case OPC_DRVNZ:
    case OPC_DRVZ:
    case OPC_PUSH:
    case OPC_POP:
    case OPC_RDPIN:
        return true;
    default:
        return false;
    }
}

#if 0
static bool
HasSideEffects(IR *ir)
{
    if (ir->dst && !IsLocalOrArg(ir->dst) /*ir->dst->kind == REG_HW*/) {
        return true;
    }
    return HasSideEffectsOtherThanReg(ir);
}
#endif

static bool
MeaninglessMath(IR *ir)
{
    int val;
    if (0 != (ir->flags & (FLAG_WC|FLAG_WZ))) {
        return false;
    }
    if (!ir->src || ir->src->kind != IMM_INT) {
        return false;
    }
    val = ir->src->val;
    switch (ir->opc) {
    case OPC_ADD:
    case OPC_SUB:
    case OPC_SHL:
    case OPC_SHR:
    case OPC_SAR:
    case OPC_ROL:
    case OPC_ROR:
    case OPC_RCL:
    case OPC_RCR:
    case OPC_OR:
    case OPC_XOR:
    case OPC_ANDN:
    case OPC_MUXC:
    case OPC_MUXNC:
    case OPC_MUXZ:
    case OPC_MUXNZ:
    case OPC_SUMC:
    case OPC_SUMNC:
    case OPC_SUMZ:
    case OPC_SUMNZ:
    case OPC_MINU:
        return (val == 0);
    case OPC_ZEROX:
    case OPC_SIGNX:
        return (val == 31);
    case OPC_AND:
        return (val == -1);
    case OPC_MAXU:
        return (val == UINT32_MAX);
    case OPC_MINS:
        return (val == INT32_MIN);
    case OPC_MAXS:
        return (val == INT32_MAX);
    default:
        return false;
    }
}

//
// see if all instructions between ir and the label "lab" are
// conditional, and have condition "cond"
//
static bool
AllInstructionsConditional(IRCond cond, IR *ir, Operand *lab)
{
    while (ir) {
        if (ir->opc == OPC_LABEL) {
            if (ir->dst == lab) {
                return true;
            }
            return false; // some other jump may go in here
        } else if (!IsDummy(ir)) {
            if (InstrSetsAnyFlags(ir)) {
                return false;
            }
            if (ir->cond != cond) {
                return false;
            }
        }
        ir = ir->next;
    }
    return false;
}

//
// Check if jump goes to the next instruction
//
static bool
IsNoOpJump(IR *ir) {
    for (IR *ir2 = ir->next;ir2;ir2=ir2->next) {
        if (IsLabel(ir2)) {
            if (ir2->dst == ir->dst) {
                return true;
            }
        } else if (!IsDummy(ir2)) break;
    }
    return false;
}

//
// eliminate code that is unnecessary
//
int EliminateDeadCode(IRList *irl)
{
    int change;
    IR *ir, *ir_next;
    int remove_jumps;
    change = 0;

    if (curfunc) {
        remove_jumps = (curfunc->optimize_flags & OPT_DEADCODE) != 0;
    } else {
        remove_jumps = (gl_optimize_flags & OPT_DEADCODE) != 0;
    }
    // first case: a jump at the end to the ret label
    ir = irl->tail;
    while (ir && IsDummy(ir)) {
        ir = ir->prev;
    }
    if (ir && ir->opc == OPC_JUMP && curfunc && ir->dst == FuncData(curfunc)->asmreturnlabel && !InstrIsVolatile(ir)) {
        DeleteIR(irl, ir);
        change = 1;
    }
    // now look for other dead code
    for (ir = irl->head; ir; ir = ir_next) {
        ir_next = ir->next;

        if (ir->opc == OPC_SETQ || ir->opc == OPC_SETQ2 || ir->opc == OPC_GENERIC_DELAY) {
            ir->flags |= FLAG_KEEP_INSTR;
            ir->next->flags |= FLAG_KEEP_INSTR;
        }
        if (InstrIsVolatile(ir)) {
            /* do nothing */
        } else if (ir->opc == OPC_JUMP && remove_jumps) {

            if (IsNoOpJump(ir)) { /* is the branch to the next instruction? */
                DeleteIR(irl, ir);
                change = 1;
            } else if (ir->cond == COND_FALSE) {
                DeleteIR(irl, ir);
                change = 1;
            } else if (ir->dst && !IsRegister(ir->dst->kind) && ir_next && AllInstructionsConditional(InvertCond(ir->cond), ir_next, ir->dst)) {
                /* if the branch skips over things that already have the right
                    condition, delete it
                */
                DeleteIR(irl, ir);
                change = 1;
            } else if (ir->cond == COND_TRUE && !IsRegister(ir->dst->kind)) {
                // dead code from here to next label
                IR *x = ir->next;
                while (x && x->opc != OPC_LABEL) {
                    ir_next = x->next;
                    if (!IsDummy(x) && !InstrIsVolatile(x)) {
                        DeleteIR(irl, x);
                        change = 1;
                    }
                    x = ir_next;
                }
            }
        } else if (ir->cond == COND_FALSE) {
            DeleteIR(irl, ir);
            change = 1;
        } else if (!IsDummy(ir) && ir->dst && !HasSideEffectsOtherThanReg(ir) && IsDeadAfter(ir, ir->dst)) {
            DeleteIR(irl, ir);
            change = 1;
        } else if (MeaninglessMath(ir)) {
            DeleteIR(irl, ir);
            change = 1;
        }
    }
    return change;
}

static void CheckOpUsage(Operand *op)
{
    if (op) {
        op->used = 1;
    }
}

static void CheckUsage(IRList *irl)
{
    IR *ir;
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->opc == OPC_LABEL) {
            if (InstrIsVolatile(ir)) {
                ir->dst->used = 1;
            }
            continue;
        } else if (IsDummy(ir) || ir->opc == OPC_LABEL) {
            continue;
        }
        CheckOpUsage(ir->src);
        CheckOpUsage(ir->dst);
    }
    /* remove unused labels */
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->opc == OPC_LABEL && !InstrIsVolatile(ir)) {
            if (ir->dst->used == 0) {
                ir->opc = OPC_DUMMY;
            }
        }
    }
}

/* checks for short forward (conditional) jumps
 * returns the number of instructions forward
 * or 0 if not a valid candidate for optimization
 */
static int IsSafeShortForwardJump(IR *irbase) {
    int n = 0;
    Operand *target;
    IR *ir;
    int limit = (curfunc->optimize_flags & OPT_EXTRASMALL) ? 10 : (gl_p2 ? 5 : 3);
    unsigned dirty_flags = 0;

    if (irbase->opc != OPC_JUMP) return 0;
    if (InstrIsVolatile(irbase)) return 0;
    target = irbase->dst;
    if (IsRegister(target->kind)) return 0;
    ir = irbase->next;
    IRCond newcond = InvertCond(irbase->cond);
    while (ir) {
        if (!IsDummy(ir)) {
            //if (ir->cond != COND_TRUE) return 0;
            if (ir->opc == OPC_LABEL) {
                if (ir->dst == target) return n;
                else return 0;
            } else if (ir->opc == OPC_JUMP && ir->dst == target && ir->cond == irbase->cond) {
                // Found jump that goes where we want to go, anyways.
                return n;
            }
            // BRK instruction cannot be conditionalized??
            if (ir->opc == OPC_BREAK) {
                return 0;
            }
            unsigned problem_flags = FlagsUsedByCond(newcond) & dirty_flags;

            // If flags are dirty, we can only accept instructions whose
            // condition is already a subset of our new condition
            if (problem_flags != 0 && !CondIsSubset(newcond,ir->cond)) {
                return 0;
            }
            // If you're wondering about instrs with inherent flag use (NEGC etc),
            // Those are not an issue, since if they aren't already conditional,
            // they're either using the initial flag value or will be cought by the subset check

            if (ir->flags & FLAG_CSET) dirty_flags |= FLAG_WC;
            if (ir->flags & FLAG_ZSET) dirty_flags |= FLAG_WZ;
            if (ir->opc == OPC_CALL) {
                // calls do not preserve condition codes
                dirty_flags |= FLAG_WC|FLAG_WZ;
            }
            if (ir->opc == OPC_DJNZ && !gl_p2) {
                // DJNZ does not work conditionally in LMM
                return 0;
            }
            if (++n > limit) return 0;
        }
        ir = ir->next;
    }
    // we reached the end... were we trying to jump to the end?
    if (curfunc && target == FuncData(curfunc)->asmreturnlabel) return n;
    else return 0;
}

static void ConditionalizeInstructions(IR *ir, IRCond cond, int n)
{
    while (ir && n > 0) {
        if (!IsDummy(ir)) {
            if (ir->opc == OPC_LABEL) {
                ERROR(NULL, "Internal error bad conditionalize");
                return;
            }
            ir->cond = (IRCond)((unsigned)cond | (unsigned)ir->cond);
            --n;
        }
        ir = ir->next;
    }
    while (ir && IsDummy(ir)) {
        ir = ir->next;
    }
    if (ir && ir->opc == OPC_LABEL) {
        // this is the destination label
        // mark it to check for optimization
        ir->cond = cond;
    }
}

int OptimizeShortBranches(IRList *irl)
{
    IR *ir;
    IR *ir_next;
    int n;
    int change = 0;
    ir = irl->head;
    if (gl_compress) {
        return 0;
    }
    while (ir) {
        ir_next = ir->next;
        n = IsSafeShortForwardJump(ir);
        if (n) {
            ConditionalizeInstructions(ir->next, InvertCond(ir->cond), n);
            DeleteIR(irl, ir);
            change++;
        }
        ir = ir_next;
    }
    return change;
}

#if 0
static void DumpIR(IRList *irl,int suscnt,...) {
    struct flexbuf flex;
    flexbuf_init(&flex,0);
    for (IR *ir=irl->head; ir; ir=ir->next) {
        DoAssembleIR(&flex,ir,NULL);
        va_list argp;
        va_start(argp,suscnt);
        for (int i=0; i<suscnt; i++) if (ir==va_arg(argp,IR*)) flexbuf_printf(&flex,"/\\ sussy no %d\n",i);
        va_end(argp);
    }
    flexbuf_addchar(&flex,0);
    printf("%s\n",flexbuf_peek(&flex));
    flexbuf_delete(&flex);
}
#endif

// Check for spurious deleted IR.
// Should be fixed properly, but too lazy .
// EDIT: Is now fixed properly, but might be good to keep anyways?
static bool ValidIR(IRList *irl,IR* ir) {
    // Check upwards
    if (
        (ir->prev ? ir->prev->next : irl->head)!=ir
        || (ir->next ? ir->next->prev : irl->tail)!=ir
    ) {
        DEBUG(NULL,"Instr. validity check failed in %s",curfunc->user_name);
        return false;
    } else {
        return true;
    }
}

// Find common ops in branches and move them out
int OptimizeBranchCommonOps(IRList *irl) {
    int change = 0;
    //printf("%s pre:\n",curfunc->user_name);
    //DumpIR(irl,0);

    //IR *susjmp = 0,*suslbl = 0;

    for (IR *ir=irl->head; ir; ir=ir->next) {
        if (InstrIsVolatile(ir) || IsDummy(ir)) continue;
        if (ir->opc == OPC_JUMP && ir->cond != COND_TRUE && ir->aux) {
            // Check for common ops at top of branch
            IR *lbl = (IR *)ir->aux;
            if (lbl->opc == OPC_LABEL && UniqJumpForLabel(lbl) == ir && lbl->prev && lbl->prev->opc == OPC_JUMP && lbl->prev->cond == COND_TRUE && ValidIR(irl,lbl)) {
                for (;;) {
                    IR *next_stay = ir->next;
                    while (next_stay && IsDummy(next_stay)) next_stay = next_stay->next;
                    IR *next_jump = lbl->next;
                    while (next_jump && IsDummy(next_jump)) next_jump = next_jump->next;

                    if (SameIR(next_stay,next_jump) && next_stay->cond == next_jump->cond // SameIR doesn't check condition code
                            && !IsPrefixOpcode(next_stay) // TODO: pull entire prefix sequence
                            && !(InstrIsVolatile(next_stay)||InstrIsVolatile(next_jump))
                            && !InstrSetsFlags(next_stay,FlagsUsedByCond(ir->cond)) // Can't reorder flag-setting op if it changes the branch outcome
                            && next_stay->opc != OPC_CALL) // Calls can also set flags
                    {

                        //printf("Top delete %s\n",next_jump->instr->name);
                        DeleteIR(irl,next_jump);
                        DoReorderBlock(irl,ir->prev,next_stay,next_stay);
                        change++;
                    } else {
                        break;
                    }
                }
            }

        } else if (ir->opc == OPC_LABEL) {
            // check for common ops at bottom of branch
            IR *jump = UniqJumpForLabel(ir);
            if (jump && jump->opc == OPC_JUMP && jump->cond == COND_TRUE && jump->aux == ir && ValidIR(irl,jump)) {

                for (;;) {
                    IR *prev_stay = ir->prev;
                    while (prev_stay && IsDummy(prev_stay)) prev_stay = prev_stay->prev;
                    IR *prev_jump = jump->prev;
                    while (prev_jump && IsDummy(prev_jump)) prev_jump = prev_jump->prev;
                    if (prev_jump == ir || prev_jump == jump || prev_stay == ir || prev_stay == jump) break;

                    if (SameIR(prev_stay,prev_jump) && prev_stay->cond == prev_jump->cond
                            && !(IsPrefixOpcode(prev_stay->prev)||IsPrefixOpcode(prev_jump->prev))
                            && !(InstrIsVolatile(prev_stay)||InstrIsVolatile(prev_jump))) {

                        //printf("Bottom delete %s\n",prev_jump->instr->name);
                        DeleteIR(irl,prev_jump);
                        DoReorderBlock(irl,ir,prev_stay,prev_stay);
                        change++;
                        //suslbl = ir;susjmp = jump;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    //printf("%s post:\n",curfunc->user_name);
    //DumpIR(irl,2,susjmp,suslbl);
    return change;
}

//
// Optimize compares with 0 by changing a previous instruction to set
// flags instead
//

static int
OptimizeCompares(IRList *irl)
{
    IR *ir;
    IR *ir_next;
    IR *ir_prev;
    int change = 0;

    if (gl_compress) {
        return 0;
    }
    ir_prev = 0;
    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        while (ir && IsDummy(ir)) {
            ir = ir_next;
            if (ir) ir_next = ir->next;
        }
        if (!ir) break;
        // Convert pointless moves into CMPS S,#0
        if (ir->opc == OPC_MOV && !InstrIsVolatile(ir) && !IsHwReg(ir->src)
                && (ir->flags & (FLAG_WZ|FLAG_WC)) && (ir->src == ir->dst || (IsDeadAfter(ir,ir->dst) && IsRegister(ir->src->kind))) ) {
            ReplaceOpcode(ir,OPC_CMPS);
            ir->dst = ir->src;
            ir->src = NewImmediate(0);
            // Flags can stay as they are
            change |= 1;
            // Note that we do not break;
        }


        if ( (ir->opc == OPC_CMP||ir->opc == OPC_CMPS) && ir->cond == COND_TRUE
                && (ir->flags & (FLAG_WZ|FLAG_WC))
                && !InstrIsVolatile(ir)
                && ir->src == ir->dst)
        {
            // this compare always sets Z and clears C
            if (ApplyConditionAfter(irl, ir, 0, 0, &change)) {
                DeleteIR(irl, ir);
                change |= 1;
            }
        }
        else if (ir->cond == COND_TRUE
                 && ((ir->opc == OPC_CMP && IsImmediateVal(ir->src, 0)) || (ir->opc == OPC_CMPS && IsImmediateVal(ir->src, INT32_MIN)) )
                 && (FLAG_WC == (ir->flags & (FLAG_WZ|FLAG_WC)))
                 && !InstrIsVolatile(ir)
                )
        {
            // this compare always clears C
            if (ApplyConditionAfter(irl, ir, 1, 1, &change)) {
                DeleteIR(irl, ir);
                change |= 1;
            }
        }
        else if ( (ir->opc == OPC_CMP||ir->opc == OPC_CMPS) && ir->cond == COND_TRUE
                  && (FLAG_WZ == (ir->flags & (FLAG_WZ|FLAG_WC)))
                  && !InstrIsVolatile(ir)
                  && IsImmediateVal(ir->src, 0)
                )
        {
            ir_prev = FindPrevSetterForCompare(ir, ir->dst);
            if (!ir_prev) {
                // we can't find where that register was set? maybe it will be
                // set inside a loop
                IR *loopend;
                IR *jmpend;
                Operand *newlabel;
                IR *ircmp;
                IR *irlabel;
                ir_prev = ir->prev;
                while (ir_prev && IsDummy(ir_prev)) {
                    ir_prev = ir_prev->prev;
                }
                jmpend = ir_next;
                while (jmpend && IsDummy(jmpend)) {
                    jmpend = jmpend->next;
                }
                if (ir_prev && ir_prev->opc == OPC_LABEL
                        && jmpend && jmpend->opc == OPC_JUMP && jmpend->cond == COND_EQ && jmpend->aux)
                {
                    loopend = (IR *)jmpend->aux;
                    loopend = loopend->prev;
                    while (loopend && IsDummy(loopend)) {
                        loopend = loopend->prev;
                    }
                    if (loopend && loopend->opc == OPC_JUMP && loopend->cond == COND_TRUE && loopend->aux && ir_prev == (IR *)loopend->aux) {
                        // OK, we've found the jump to loop end
                        // insert a new label after jmpend, copy the compare, and
                        // change the loopend jmp to jump to the new label
                        newlabel = NewCodeLabel();
                        irlabel = NewIR(OPC_LABEL);
                        irlabel->dst = newlabel;
                        ircmp = NewIR(ir->opc);
                        ircmp->cond = COND_TRUE;
                        ircmp->flags = ir->flags;
                        ircmp->dst = ir->dst;
                        ircmp->src = ir->src;
                        InsertAfterIR(irl, jmpend, irlabel);
                        InsertAfterIR(irl, loopend->prev, ircmp);
                        loopend->aux = irlabel;
                        loopend->dst = newlabel;
                        loopend->cond = COND_NE;
                    }
                }
            }
            else if (ir_prev
                     && !InstrSetsAnyFlags(ir_prev)
                     && !InstrIsVolatile(ir_prev)
                     && !InstrIsVolatile(ir)
                     && ir_prev->cond == COND_TRUE
                     && (ir_prev->flags & (FLAG_ZSET&~FLAG_WZ)) == 0
                     && CanTestZero(ir_prev->opc)
                     && FlagsNotUsedBetween(ir_prev, ir, FLAG_WZ)
                     && FlagsNotSetBetween(ir_prev,ir,FLAG_WZ))
            {
                ir_prev->flags |= FLAG_WZ;
                DeleteIR(irl, ir);
                change = 1;
                /* now we may be able to do a further optimization,
                   if ir_prev is a sub and the next instruction is a jmp
                */
                if (ir_prev->opc == OPC_SUB
                        && ir_next->opc == OPC_JUMP
                        && ir_next->cond == COND_NE
                        && IsImmediateVal(ir_prev->src, 1)
                        && IsCloseJump(ir_next)
                        && !UsedInRange(ir_prev->next,ir_next->prev,ir_prev->dst)
                   )
                {
                    // replace jmp with djnz
                    ReplaceOpcode(ir_next, OPC_DJNZ);
                    ir_next->cond = COND_TRUE;
                    ir_next->src = ir_next->dst;
                    ir_next->dst = ir_prev->dst;
                    DeleteIR(irl, ir_prev);
                }
            }
        }
        ir_prev = ir;
        ir = ir_next;
    }
    return change;
}

static int
OptimizeImmediates(IRList *irl)
{
    IR *ir;
    Operand *src;
    int val;
    int change = 0;

    for (ir = irl->head; ir; ir = ir->next) {
        if (InstrIsVolatile(ir)) {
            continue;
        }
        src = ir->src;
        if (! (src && src->kind == IMM_INT) ) {
            continue;
        }
        val = src->val;
        if (val != (val&31) && IsSrcBitIndex(ir)) {
            // always cut unused bits when immediate is a bit index
            ir->src = NewImmediate(val&31);
            change++;
        } else if (!gl_p2 && (src->name == NULL || src->name[0] == 0)) {
            /* already a small immediate */
            continue;
        } else if (ir->opc == OPC_MOV && val < 0 && val >= -511) {
            ReplaceOpcode(ir, OPC_NEG);
            ir->src = NewImmediate(-val);
            change++;
        } else if (ir->opc == OPC_AND && val < 0 && val >= -512) {
            ReplaceOpcode(ir, OPC_ANDN);
            ir->src = NewImmediate( ~val ); /* note that is a tilde! */
            change++;
        } else if (ir->opc == OPC_ADD && val < 0 && val >= -511) {
            ReplaceOpcode(ir, OPC_SUB);
            ir->src = NewImmediate(-val);
            change++;
        } else if (ir->opc == OPC_SUB && val < 0 && val >= -511) {
            ReplaceOpcode(ir, OPC_ADD);
            ir->src = NewImmediate(-val);
            change++;
        }
    }
    return change;
}


static int
OptimizeAddSub(IRList *irl)
{
    int change = 0;
    IR *ir, *ir_next;
    IR *prev;

    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        while (ir_next && IsDummy(ir_next)) {
            ir_next = ir_next->next;
        }
        if (!ir_next) break;
        if ((ir->opc == OPC_ADD || ir->opc == OPC_SUB) && !InstrIsVolatile(ir)) {
            prev = FindPrevSetterForReplace(ir, ir->dst);
            if (prev && (prev->opc == OPC_ADD || prev->opc == OPC_SUB) ) {
                if (ir->src->kind == IMM_INT && prev->src->kind == IMM_INT
                        && ir->cond == prev->cond)
                {
                    int val = AddSubVal(ir) + AddSubVal(prev);
                    if (val < 0) {
                        val = -val;
                        ReplaceOpcode(ir, OPC_SUB);
                    } else {
                        ReplaceOpcode(ir, OPC_ADD);
                    }
                    ir->src = NewImmediate(val);
                    DeleteIR(irl, prev);
                    change = 1;
                }
            }
        }
        ir = ir_next;
    }
    return change;
}

//
// assign addresses to instructions
// these do not have to be exact, just close enough that they
// can help guide optimization, in particular whether jumps are
// forward or backward
//
static void
AssignTemporaryAddresses(IRList *irl)
{
    IR *ir;
    unsigned addr = 0;
    for (ir = irl->head; ir; ir = ir->next) {
        ir->flags &= ~FLAG_OPTIMIZER;
        ir->addr = addr;
        if (IsDummy(ir) || IsLabel(ir)) {
            // do not increment
        } else {
            addr++;
        }
        if (IsJump(ir) || IsLabel(ir)) {
            ir->aux = NULL;
        }
    }
}

//
// find out if a label is referenced (perhaps indirectly)
// if there is a unique jump to it, return a pointer to it
//
static void
MarkLabelUses(IRList *irl, IR *irlabel)
{
    IR *ir;
    Operand *label = irlabel->dst;
    Operand *dst;

    if (label->used >= 9999) {
        // GOSUB labels get flagged with a large used value so they do not get taken away
        irlabel->flags |= FLAG_LABEL_USED;
    }
    for (ir = irl->head; ir; ir = ir->next) {
        if (IsDummy(ir)) continue;
        if (IsJump(ir)) {
            dst = JumpDest(ir);
            if (dst == label) {
                ir->aux = irlabel; // record where the jump goes to
                // Append to list of label uses if not invalidated
                if (!(irlabel->flags & FLAG_LABEL_USED) || irlabel->aux) {
                    AppendLblJump(irlabel,ir);
                    irlabel->flags |= FLAG_LABEL_USED;
                }
            }
        } else if (ir != irlabel) {
            if (ir->src == label || ir->dst == label) {
                irlabel->flags |= FLAG_LABEL_USED;
                irlabel->aux = NULL; // invalidate use list
            }
        }
    }
}

static bool
IsTemporaryLabel(Operand *op)
{
    const char *name = op->name;
    if (name && (name[0] == 'L' && name[1] == '_' && name[2] == '_')) {
        return true;
    }
    return false;
}

//
// check label usage
//
static int
CheckLabelUsage(IRList *irl)
{
    IR *ir, *ir_next;
    ir = irl->head;
    int change = 0;

    while (ir) {
        ir_next = ir->next;
        if (ir->opc == OPC_LABEL) {
            MarkLabelUses(irl, ir);
            if ( IsTemporaryLabel(ir->dst) && !(ir->flags & (FLAG_LABEL_USED|FLAG_KEEP_INSTR))) {
                DeleteIR(irl, ir);
                change = 1;
            }
        }
        ir = ir_next;
    }

    return change;
}

//
// replace use of NZ with C and Z with NC
//
void
ReplaceZWithNC(IR *ir)
{
    switch (ir->cond) {
    case COND_EQ:
        ir->cond = COND_NC;
        break;
    case COND_NE:
        ir->cond = COND_C;
        break;
    case COND_TRUE:
    case COND_FALSE:
        break;
    default:
        ERROR(NULL, "Internal error, unexpected condition %d in ReplaceZWithNC",ir->cond);
    }
    switch (ir->opc) {
    case OPC_MUXZ:
        ReplaceOpcode(ir, OPC_MUXNC);
        break;
    case OPC_MUXNZ:
        ReplaceOpcode(ir, OPC_MUXC);
        break;
    case OPC_NEGZ:
        ReplaceOpcode(ir, OPC_NEGNC);
        break;
    case OPC_NEGNZ:
        ReplaceOpcode(ir, OPC_NEGC);
        break;
    case OPC_SUMZ:
        ReplaceOpcode(ir, OPC_SUMNC);
        break;
    case OPC_SUMNZ:
        ReplaceOpcode(ir, OPC_SUMC);
        break;
    case OPC_DRVZ:
        ReplaceOpcode(ir, OPC_DRVNC);
        break;
    case OPC_DRVNZ:
        ReplaceOpcode(ir, OPC_DRVC);
        break;
    case OPC_MUXC:
    case OPC_MUXNC:
    case OPC_NEGC:
    case OPC_NEGNC:
    case OPC_SUMC:
    case OPC_SUMNC:
    case OPC_GENERIC:
    case OPC_GENERIC_DELAY:
    case OPC_GENERIC_NR:
    case OPC_RCL:
    case OPC_RCR:
    case OPC_ADDX:
    case OPC_ADDSX:
    case OPC_SUBX:
    case OPC_SUBSX:
    // Are the below really needed here?
    case OPC_LOCKNEW:
    case OPC_LOCKSET:
    case OPC_LOCKCLR:
    case OPC_LOCKRET:
    case OPC_LOCKTRY:
    case OPC_LOCKREL:
    case OPC_SETQ:
    case OPC_SETQ2:
    case OPC_PUSH:
    case OPC_POP:
        ERROR(NULL, "Internal error, unexpected use of C");
        break;
    default:
        break;
    }
}

static bool
IsCommutativeMath(IROpcode opc)
{
    switch (opc) {
    case OPC_ADD:
    case OPC_OR:
    case OPC_AND:
    case OPC_XOR:
    case OPC_TEST:
    case OPC_MULU:
    case OPC_MULS:
        return true;
    case OPC_MAXS:
    case OPC_MAXU:
    case OPC_MINS:
    case OPC_MINU:
        // These have C flag that depends on operand order.
        // The only place this function is called currently doesn't allow flag sets, so it's fine,
        // But if that ever changes, these need to be distinguished.
        return true;
    default:
        return false;
    }
}

// check if x is of the form (A ROL n)
// where A is a small integer that is all 1's
// in binary; if so, return a bit mask
// otherwise, return -1
static int P2CheckBitMask(unsigned int x)
{
    if (x == 0 || x == 0xFFFFFFFF) return -1;

    int rshift = ctz32(x);
    if (rshift == 0 && (x>>31)) { // No trailing zero, but leading one -> try wraparound case
        rshift = 32-clz32(~x);
    }
    x = x>>rshift | x<<((32-rshift)&31);

    if(x&(x+1)) return -1; // x is not (2^n)-1
    int addbits = 31-clz32(x);
    if (addbits > 15) return -1; // Doesn't fit in 9 bits
    return rshift + (addbits<<5);
}

bool
IRFlagsDeadAfter(IRList *irl, IR *ir, unsigned flags)
{
    if (!ir) return true;
    ir = ir->next;
    while (ir && flags) {
        if (IsLabel(ir)) {
            return true;
        }
        if (ir->cond != COND_TRUE) {
            return false;
        }
        if (InstrUsesFlags(ir, flags)) {
            return false;
        }
        if (InstrSetsFlags(ir, flags)) {
            flags &= ~(ir->flags);
        }
        if (!flags) {
            return true;
        }
        ir = ir->next;
    }
    return true;
}

//
// basic peephole substitution
//
// mov tmp, x
// and tmp, y wz
//   becomes test x,y wz if tmp is dead
//
// test tmp, #1 wz
// shr  tmp, #1
//    becomes shr tmp, #1 wc if we can replace NZ with C
//
// similarly for test tmp, #$8000_0000 wz ; shl tmp, #1
//
// if_c  or a, b
// if_nc andn a, b
//    becomes muxc a, b
//
// and x, #0xf
// mov y, x
// and y #0xff
//
// can remove the last and, it is redundant
//

// add objptr, x
// mov tmp, objptr
// sub objptr, x
//   becomes mov tmp,objptr
//           add tmp,objptr
//

// mov a,b
// mov b,a
// we can delete the second mov

// add a,b
// mov b,a
//   becomes add b, a, if a is dead

// mov a, #1
// shl a, N
// becomes decod a, N on P2

// xor a, #(1<<x)
// becomes bitnot a, #x on P2

int
OptimizePeepholes(IRList *irl)
{
    IR *ir, *ir_next;
    IR *previr;
    int changed = 0;
    IROpcode opc;

    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        while (ir_next && IsDummy(ir_next)) {
            ir_next = ir_next->next;
        }
        if (InstrIsVolatile(ir) || ir->srceffect != OPEFFECT_NONE || ir->dsteffect != OPEFFECT_NONE) {
            goto done;
        }
        opc = ir->opc;
        if ( (opc == OPC_AND || opc == OPC_OR)
                && IsImmediate(ir->src)
                && !InstrSetsFlags(ir, FLAG_WC)
           )
        {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_MOV && !IsImmediate(previr->src)) {
                previr = FindPrevSetterForReplace(previr, previr->src);
            }
            if (previr && previr->opc == opc && IsImmediate(previr->src)) {
                unsigned oldmask = previr->src->val;
                unsigned newmask = ir->src->val;
                // check to see if ir is redundant
                if (opc == OPC_AND && ((oldmask & newmask) == oldmask)) {
                    if (InstrSetsFlags(ir, FLAG_WZ)) {
                        ReplaceOpcode(ir, OPC_CMP);
                        ir->src->val = 0;
                        changed = 1;
                    } else if (!InstrSetsAnyFlags(ir)) {
                        DeleteIR(irl, ir);
                        changed = 1;
                    }
                    goto done;
                } else if (opc == OPC_OR && ((oldmask | newmask) == oldmask)) {
                    if (InstrSetsFlags(ir, FLAG_WZ)) {
                        ReplaceOpcode(ir, OPC_CMP);
                        ir->src->val = 0;
                        changed = 1;
                    } else if (!InstrSetsAnyFlags(ir)) {
                        DeleteIR(irl, ir);
                        changed = 1;
                    }
                    goto done;
                }
            }
        }
        if (opc == OPC_AND && InstrSetsAnyFlags(ir)) {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_MOV && IsDeadAfter(ir, ir->dst) && !SrcOnlyHwReg(previr->src) && IsRegister(previr->src->kind)) {
                ReplaceOpcode(ir, OPC_TEST);
                ir->dst = previr->src;
                DeleteIR(irl, previr);
                changed = 1;
            } else if (IsDeadAfter(ir, ir->dst)) {
                ReplaceOpcode(ir, OPC_TEST);
                changed = 1;
            }
        } else if ( (opc == OPC_SHR || opc == OPC_SAR)
                    && !InstrSetsFlags(ir, FLAG_WC)
                    && !InstrUsesFlags(ir, FLAG_WC|FLAG_WZ)
                    && IsImmediateVal(ir->src, 1))
        {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_TEST
                    && IsImmediateVal(previr->src, 1))
            {
                /* maybe we can replace the test with the shr */
                /* we already know the result isn't used between previr
                   and ir; check the instructions in between for use
                   of C, and also verify that the flags aren't used
                   in subsequent instructions
                */
                int test_is_z = (previr->flags & (FLAG_WZ|FLAG_WC)) == FLAG_WZ;
                int test_is_c = (previr->flags & (FLAG_WZ|FLAG_WC)) == FLAG_WC;
                IR *testir;
                IR *lastir = NULL;
                bool sawir = false;
                bool changeok = test_is_z || test_is_c;
                int irflags = (ir->flags & FLAG_WZ) | FLAG_WC;
                for (testir = previr->next; testir && changeok; testir = testir->next) {
                    if (testir == ir) {
                        sawir = true;
                    }
                    if (IsBranch(testir)) {
                        /* we assume flags do not have to be preserved  across branches*/
                        lastir = testir;
                        break;
                    }
                    if (test_is_z && InstrUsesFlags(testir, FLAG_WC)) {
                        changeok = false;
                    } else if (InstrSetsAnyFlags(testir)) {
                        changeok = sawir;
                        lastir = testir;
                        break;
                    }
                }
                if (changeok) {
                    /* ok, let's go ahead and change it */
                    ReplaceOpcode(previr, opc);
                    previr->flags &= ~(FLAG_WZ|FLAG_WC);
                    previr->flags |= irflags;
                    if (test_is_z) {
                        for (testir = previr->next; testir && testir != lastir;
                                testir = testir->next)
                        {
                            ReplaceZWithNC(testir);
                        }
                        if (IsBranch(lastir)) {
                            ReplaceZWithNC(lastir);
                        }
                    }
                    DeleteIR(irl, ir);
                    changed = 1;
                    goto done;
                }
            }
        } else if ( (opc == OPC_SHL || opc == OPC_ROL)
                    && !InstrSetsFlags(ir, FLAG_WC)
                    && !InstrUsesFlags(ir, FLAG_WC|FLAG_WZ)
                    && IsImmediateVal(ir->src, 1))
        {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr &&
                    (    (previr->opc == OPC_TEST
                          && IsImmediateVal(previr->src, 0x80000000))
                         || (previr->opc == OPC_TESTBN
                             && IsImmediateVal(previr->src, 31))
                    )
                    && (previr->flags & (FLAG_WZ|FLAG_WC)) == FLAG_WZ)
            {
                /* maybe we can replace the test with the shl */
                /* we already know the result isn't used between previr
                   and ir; check the instructions in between for use
                   of C, and also verify that the flags aren't used
                   in subsequent instructions
                */
                IR *testir;
                IR *lastir = NULL;
                bool sawir = false;
                bool changeok = true;
                int irflags = (ir->flags & FLAG_WZ) | FLAG_WC;
                for (testir = previr->next; testir && changeok; testir = testir->next) {
                    if (testir == ir) {
                        sawir = true;
                    }
                    if (IsBranch(testir)) {
                        /* we assume flags do not have to be preserved  across branches*/
                        lastir = testir;
                        break;
                    }
                    if (InstrUsesFlags(testir, FLAG_WC)) {
                        changeok = false;
                    } else if (InstrSetsAnyFlags(testir)) {
                        changeok = sawir;
                        lastir = testir;
                        break;
                    }
                }
                if (changeok) {
                    /* ok, let's go ahead and change it */
                    ReplaceOpcode(previr, opc);
                    previr->src = NewImmediate(1); // change to shl #1 wc
                    previr->flags &= ~(FLAG_WZ|FLAG_WC);
                    previr->flags |= irflags;
                    for (testir = previr->next; testir && testir != lastir;
                            testir = testir->next)
                    {
                        ReplaceZWithNC(testir);
                    }
                    if (IsBranch(lastir)) {
                        ReplaceZWithNC(lastir);
                    }
                    DeleteIR(irl, ir);
                    changed = 1;
                    goto done;
                }
            }
        }
        else if (opc == OPC_OR && ir->cond == COND_C && !InstrSetsAnyFlags(ir)
                 && ir_next && ir_next->opc == OPC_ANDN && ir_next->cond == COND_NC
                 && !InstrSetsAnyFlags(ir_next)
                 && ir->src == ir_next->src
                 && ir->dst == ir_next->dst)
        {
            ir_next->cond = COND_TRUE;
            ReplaceOpcode(ir_next, OPC_MUXC);
            DeleteIR(irl, ir);
            changed = 1;
            goto done;
        }
        else if (opc == OPC_OR && ir->cond == COND_NE && !InstrSetsAnyFlags(ir)
                 && ir_next
                 && ir_next->opc == OPC_ANDN && ir_next->cond == COND_EQ
                 && !InstrSetsAnyFlags(ir_next)
                 && ir->src == ir_next->src
                 && ir->dst == ir_next->dst)
        {
            ir_next->cond = COND_TRUE;
            ReplaceOpcode(ir_next, OPC_MUXNZ);
            DeleteIR(irl, ir);
            changed = 1;
            goto done;
        }
        else if (opc == OPC_MOV && !InstrSetsAnyFlags(ir)
                 && ir_next
                 && !InstrSetsAnyFlags(ir_next)
                 && ir_next->cond == ir->cond
                 && ir_next->opc == OPC_SUB
                 && ir_next->dst == ir->src
                 && 0 != (previr = FindPrevSetterForReplace(ir, ir->src))
                 && previr->cond == ir->cond
                 && previr->opc == OPC_ADD
                 && previr->src == ir_next->src
                 && previr->dst == ir_next->dst
                 && !InstrSetsAnyFlags(ir_next)
                )
        {
            // add ptr, y   '' previr
            // mov a, ptr   '' ir
            // sub ptr, y   '' ir_next
            // => mov a, x
            //    add a, y
            ReplaceOpcode(ir_next, OPC_ADD);
            ir_next->dst = ir->dst;
            DeleteIR(irl, previr);
            changed = 1;
            goto done;
        }
        else if (gl_p2
                 && opc == OPC_MOV && !InstrSetsAnyFlags(ir)
                 && ir_next
                 && !InstrSetsAnyFlags(ir_next)
                 && ir_next->cond == ir->cond
                 && ir_next->opc == OPC_SHL
                 && ir->dst == ir_next->dst
                 && IsImmediateVal(ir->src, 1)
                )
        {
            // mov x, #1
            // shl x, y
            // -> decod x, y
            ReplaceOpcode(ir_next, OPC_DECOD);
            DeleteIR(irl, ir);
            goto done;
        }
        // check for mov a,b ;; mov b,a
        if (opc == OPC_MOV && ir_next && ir_next->opc == OPC_MOV
                && ir->dst == ir_next->src
                && ir->src == ir_next->dst
                && !InstrSetsAnyFlags(ir)
                && !InstrSetsAnyFlags(ir_next)
                && ir->cond == ir_next->cond)
        {
            ir_next->dst = ir->dst;
            ir_next->src = ir->src;
            DeleteIR(irl, ir);
            changed = 1;
            goto done;
        }
        // check for and a, x ;; test a, x wcz
        if (ir->opc == OPC_TEST && ir->cond == COND_TRUE)
        {
            IR *previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_AND && !InstrSetsAnyFlags(previr) && previr->cond == COND_TRUE && SameOperand(previr->src, ir->src))
            {
                if (IsDeadAfter(ir, ir->dst)) {
                    DeleteIR(irl, previr);
                } else {
                    previr->flags |= (ir->flags & (FLAG_WC|FLAG_WZ));
                    DeleteIR(irl, ir);
                }
                changed = 1;
                goto done;
            }
        }

        // check for consecutive math opcodes
        if (ir_next && ir_next->opc == opc
            && ir->dst == ir_next->dst
            && ir->cond == ir_next->cond
            && !InstrSetsAnyFlags(ir)
            && !InstrSetsAnyFlags(ir_next)
            && IsDirectImmediate(ir->src)
            && IsDirectImmediate(ir_next->src)
            && !IsHwReg(ir->dst)
            /* && !InstrIsVolatile(ir) already checked above */
            && !InstrIsVolatile(ir_next)
            )
        {
            int newval;
            bool merge = true;
            // check for mergeable immediate math operands
            switch (opc) {
/*
  ADD and SUB are already handled in OptimizeAddSub
            case OPC_ADD:
            case OPC_SUB:
                newval = ir_next->src->val + ir->src->val;
                break;
*/                
            case OPC_SHL:
            case OPC_SAR:
            case OPC_SHR:
                newval = ir_next->src->val + ir->src->val;
                if (newval > 0x1f) {
                    merge = false;
                }
                break;
            case OPC_OR:
                newval = ir_next->src->val | ir->src->val;
                break;
            case OPC_AND:
                newval = ir_next->src->val & ir->src->val;
                break;
            default:
                merge = false;
                break;
            }
            if (merge) {
                ir_next->src = NewImmediate(newval);
                DeleteIR(irl, ir);
                changed = 1;
                goto done;
            }
        }
        
        // check for add a,b ;; mov b,a ;; isdead a
        // becomes add b, a

        if ( (IsCommutativeMath(opc) || (gl_p2 && opc == OPC_SUB))
                && ir_next && ir_next->opc == OPC_MOV
                && ir->dst == ir_next->src
                && ir->src == ir_next->dst
                && !InstrSetsAnyFlags(ir)
                && !InstrSetsAnyFlags(ir_next)
                && ir->cond == ir_next->cond
                && IsDeadAfter(ir_next, ir->dst)
           )
        {
            ReplaceOpcode(ir_next, opc == OPC_SUB ? OPC_SUBR : opc);
            DeleteIR(irl, ir);
            changed = 1;
            goto done;
        }

        int32_t tmp;

        // transform
        //        mov x,#0
        //   if_c neg x,#1
        // to
        //        subx x,x
        if (ir->cond == COND_C && isConstMove(ir,&tmp) && tmp == -1 && !InstrSetsAnyFlags(ir) && !IsHwReg(ir->dst)) {
            previr = FindPrevSetterForReplace(ir,ir->dst);
            if (previr && isConstMove(previr,&tmp) && tmp == 0) {
                ReplaceOpcode(ir,OPC_SUBX);
                ir->src = ir->dst;
                ir->cond = COND_TRUE;
                DeleteIR(irl,previr); // For some reason deadcode doesn't always pick it up.
                changed = 1;
                goto done;
            }
        }

        // AND -> GET* optimization
        if (gl_p2 && isMaskingOp(ir,&tmp) && !InstrSetsAnyFlags(ir)) {
            IROpcode getopc;
            int shift;
            switch (tmp) {
            case 0xF:
                getopc = OPC_GETNIB;
                shift = 2;
                break;
            case 0xFF:
                getopc = OPC_GETBYTE;
                shift = 3;
                break;
            case 0xFFFF:
                getopc = OPC_GETWORD;
                shift = 4;
                break;
            default:
                goto no_getx;
            }
            IR *previr;
            int which = 0;
            previr = FindPrevSetterForReplace(ir,ir->dst);
            if (previr && (previr->opc == OPC_SHR || previr->opc == OPC_SAR)
                    && !InstrSetsAnyFlags(ir) && previr->src->kind == IMM_INT
                    && (previr->src->val & ((1<<shift)-1)) == 0 && ir->cond == COND_TRUE) {
                which = (previr->src->val&31)>>shift;
                DeleteIR(irl,previr);
                changed = 1;
            } else if (previr && previr->opc == getopc) {
                // No-op
                ir->cond = COND_FALSE;
                changed = 1;
                goto done;
            }
            if (ir->opc != getopc) {
                ReplaceOpcode(ir,getopc);
                changed = 1;
            }
            if (!ir->src2 || !IsImmediateVal(ir->src2,which)) {
                ir->src2 = NewImmediate(which);
                changed = 1;
            }
            ir->src = ir->dst;
            goto done;
        }
no_getx:

        // Check for unneccessary sign/zero extends before MULU/MULS
        if (ir->opc == OPC_MULU || ir->opc == OPC_MULS) {
            IR *previr;
            int32_t tmp;
            // Check dst
            previr = FindPrevSetterForReplace(ir,ir->dst);
            if (previr && previr->opc == OPC_SIGNX && previr->src->kind == IMM_INT && (previr->src->val&31) >= 15) {
                DeleteIR(irl,previr);
                changed = 1;
            } else if (previr && isMaskingOp(previr,&tmp) && (tmp&0xFFFF)==0xFFFF) {
                DeleteIR(irl,previr);
                changed = 1;
            } else if (previr && previr->opc == OPC_GETWORD) {
                ReplaceOpcode(previr,OPC_MOV);
                previr->src2 = NULL;
                changed = 1;
            }
            // Check src
            if (IsDeadAfter(ir,ir->src)) {
                previr = FindPrevSetterForReplace(ir,ir->src);
                if (previr && previr->opc == OPC_SIGNX && previr->src->kind == IMM_INT && (previr->src->val&31) >= 15) {
                    DeleteIR(irl,previr);
                    changed = 1;
                } else if (previr && isMaskingOp(previr,&tmp) && (tmp&0xFFFF)==0xFFFF) {
                    DeleteIR(irl,previr);
                    changed = 1;
                } else if (previr && previr->opc == OPC_GETWORD) {
                    ReplaceOpcode(previr,OPC_MOV);
                    changed = 1;
                }
            }


        }


        if (ir->opc == OPC_NEGC || ir->opc == OPC_NEGNC || ir->opc == OPC_NEGZ || ir->opc == OPC_NEGNZ) {
            IR *nextir;
            // Check for no-op
            if (ir->src->kind == IMM_INT && ir->src->val == 0) {
                ReplaceOpcode(ir,OPC_MOV);
                changed = 1;
                goto done;
            }
            // Check for NEGxx;ADD/SUB sequences and transform to single SUMxx
            if ((nextir = FindNextUse(ir,ir->dst)) && nextir->src == ir->dst && nextir->cond == ir->cond
                    && (nextir->opc == OPC_ADD || nextir->opc == OPC_SUB)
                    && !InstrSetsAnyFlags(ir) && !InstrSetsFlags(nextir,FLAG_WC)
                    && !ModifiedInRange(ir->next,nextir->prev,ir->src)
                    && !FlagsChangeInRange(ir->next,nextir->prev,FlagsUsedByCond(ir->cond) | ((ir->opc == OPC_NEGC || ir->opc == OPC_NEGNC) ? FLAG_WC : FLAG_WZ))
                    && IsDeadAfter(nextir,ir->dst)) {
                bool isSub = (nextir->opc == OPC_SUB);
                IROpcode newOpc;
                switch (ir->opc) {
                case OPC_NEGC:
                    newOpc = isSub ? OPC_SUMNC : OPC_SUMC;
                    break;
                case OPC_NEGNC:
                    newOpc = isSub ? OPC_SUMC : OPC_SUMNC;
                    break;
                case OPC_NEGZ:
                    newOpc = isSub ? OPC_SUMNZ : OPC_SUMZ;
                    break;
                case OPC_NEGNZ:
                    newOpc = isSub ? OPC_SUMZ : OPC_SUMNZ;
                    break;
                default:
                    // unreachable
                    goto done;
                }
                nextir->src = ir->src;
                ReplaceOpcode(nextir,newOpc);
                DeleteIR(irl,ir);
                changed = 1;
                goto done;
            }
        }

        // on P2, check for immediate operand with just one bit set
        if (gl_p2 && ir->src && ir->src->kind == IMM_INT && !InstrSetsAnyFlags(ir) && ((uint32_t)ir->src->val) > 511) {
            if (ir->opc == OPC_AND) {
                int invmask = P2CheckBitMask(~(ir->src->val));
                if (invmask != -1) {
                    ReplaceOpcode(ir, OPC_BITL);
                    ir->src = NewImmediate(invmask);
                    changed = 1;
                    goto done;
                }
            }
            int mask = P2CheckBitMask(ir->src->val);
            if (mask != -1) {
                if (ir->opc == OPC_ANDN) {
                    ReplaceOpcode(ir, OPC_BITL);
                    ir->src = NewImmediate(mask);
                    changed = 1;
                    goto done;
                }
                if (ir->opc == OPC_OR) {
                    ReplaceOpcode(ir, OPC_BITH);
                    ir->src = NewImmediate(mask);
                    changed = 1;
                    goto done;
                }
                if (ir->opc == OPC_XOR) {
                    ReplaceOpcode(ir, OPC_BITNOT);
                    ir->src = NewImmediate(mask);
                    changed = 1;
                    goto done;
                }
                if (ir->opc == OPC_MOV && mask < 32) {
                    ReplaceOpcode(ir, OPC_DECOD);
                    ir->src = NewImmediate(mask);
                    changed = 1;
                    goto done;
                }
            }
            if (ir->opc == OPC_MOV || ir->opc == OPC_AND || ir->opc == OPC_ANDN) {
                uint32_t mask = ir->src->val;
                if (ir->opc == OPC_ANDN) mask = ~mask;
                int bits = 0;
                while ( (mask & 1) ) {
                    bits++;
                    mask = mask >> 1;
                }
                if (bits > 0 && mask == 0) {
                    // could use BMASK/ZEROX instruction
                    ReplaceOpcode(ir, ir->opc == OPC_MOV ? OPC_BMASK : OPC_ZEROX);
                    ir->src = NewImmediate(bits-1);
                    changed = 1;
                    goto done;
                }
            }
        }
        else if (gl_p2 && ir->opc == OPC_TEST && ir->flags == FLAG_WZ)
        {
            // this code isn't finished yet
            int mask = P2CheckBitMask(ir->src->val);
            if (mask==(mask & 0x1f)    // just one bit set
                    && ((unsigned)ir->src->val) > 511) // and a large constant
            {
                ReplaceOpcode(ir, OPC_TESTBN);
                ir->src = NewImmediate(mask);
                changed = 1;
                goto done;
            }
        }

        //   shr x,#N
        //   test x,#M
        // to
        //   test x,#M<<N
        if (ir->opc == OPC_TEST && ir->src->kind == IMM_INT) {
            uint32_t testmask = (uint32_t)ir->src->val;
            IR *prev_ir = FindPrevSetterForReplace(ir,ir->dst);
            if (prev_ir && prev_ir->opc == OPC_SHR 
                    && !InstrSetsAnyFlags(prev_ir) && !InstrIsVolatile(prev_ir)
                    && CondIsSubset(prev_ir->cond,ir->cond)
                    && prev_ir->src->kind == IMM_INT && IsDeadAfter(ir,ir->dst)) {
                int shiftval = prev_ir->src->val&31;

                if (shiftval == prev_ir->src->val)
                {
                    // Note: No need to check if shifted mask looses bits, since those would always be zero, anyways
                    ir->src = NewImmediate(testmask<<shiftval);
                    DeleteIR(irl,prev_ir);
                    changed = 1;
                    goto done;
                }
            }
        }

done:
        ir = ir_next;
    }
    return changed;
}

/* special peephole for buf[i++] := n */
/* look for mov tmp, b; add b, #1; use tmp */
/* in this case push the add b, #1 as far forward as we can */
/* but not past any CORDIC instructions... */
static int
OptimizeIncDec(IRList *irl)
{
    IR *ir, *ir_next;
    ir = irl->head;
    int change = 0;
    while (ir) {
        ir_next = ir->next;
        while (ir_next && IsDummy(ir_next)) {
            ir_next = ir_next->next;
        }
        if (ir->opc == OPC_MOV
                && IsLocal(ir->dst)
                && ir->cond == COND_TRUE
                && ir_next
                && (ir_next->opc == OPC_ADD || ir_next->opc == OPC_SUB)
                && ir_next->dst == ir->src
                && IsImmediate(ir_next->src)
                && !InstrIsVolatile(ir)
                && !InstrIsVolatile(ir_next)
                && !InstrSetsAnyFlags(ir_next)
           )
        {
            // push the add forward in the instruction stream as far as we can
            IR *placeir = NULL;
            IR *stepir = ir_next->next;
            Operand *changedOp = ir_next->dst;
            while (stepir) {
                if (IsJump(stepir) || IsReorderBarrier(stepir))
                    break;
                if (IsCallThatUsesReg(stepir,changedOp))
                    break;
                if (IsLabel(stepir))
                    break;
                if (IsCordicGet(stepir)) {
                    break;
                }
                if (stepir->dst == changedOp || stepir->src == changedOp) {
                    // cannot push any further
                    break;
                }
                if (InstrModifies(stepir, changedOp)) {
                    // also cannot push further
                    break;
                }
                // technically we could push past conditionals, but
                // the code may look ugly if we do
                if (stepir->cond != COND_TRUE)
                    break;
                placeir = stepir;
                stepir = stepir->next;
            }
            if (placeir) {
                DeleteIR(irl, ir_next);
                ir_next->next = NULL;
                InsertAfterIR(irl, placeir, ir_next);
                change = 1;
            }
        }
        ir = ir->next;
    }
    return change;
}

/* helper function: see if we can find any registers in this IRL that
 * use the name "name". This is needed because some indirect operations
 * create COG labels that don't point back to the original Operand; to
 * de-reference those and convert back to the original we need to search
 * for it
 */
static Operand *
FindNamedOperand(IRList *irl, const char *name, int val)
{
    IR *ir;
    for (ir = irl->head; ir; ir = ir->next) {
        if (IsDummy(ir) || IsLabel(ir)) continue;
        if (ir->dst && IsRegister(ir->dst->kind) && ir->dst->val == val) {
            if (!strcmp(ir->dst->name, name)) {
                return ir->dst;
            }
        }
        if (ir->src && IsRegister(ir->src->kind) && ir->src->val == val) {
            if (!strcmp(ir->src->name, name)) {
                return ir->src;
            }
        }
    }
    // if we get here, punt
    return NewOperand(REG_HW, name, val);
}

/* optimizer for COG memory accesses
 * if we see something like:
 *   movs wrcog, #x
 *   movd wrcog, #y
 *   call #wrcog
 * we can replace it with
 *   mov x, y
 */
extern Operand *putcogreg;

static int
IsMovIndirect(IR *ir, IR *ir_prev, IR *ir_next)
{
    if (!ir_prev
            || ir_prev->opc != OPC_LIVE)
    {
        return 0;
    }
    if (!ir_next) {
        return 0;
    }
    if (ir->opc == OPC_MOVS
            && ir->dst == putcogreg
            && ir->src->kind == IMM_COG_LABEL
            && ir_next
            && ir_next->opc == OPC_MOVD
            && ir_next->dst == putcogreg
            && ir_next->src->kind == IMM_COG_LABEL
            && ir_next->next
            && ir_next->next->opc == OPC_CALL
            && ir_next->next->dst == putcogreg
       )
    {
        return 1;
    }
    if (0 && gl_p2
            && ir->opc == OPC_MOV
            && ir->dst == ir_next->dst
            && ir->src->kind == IMM_COG_LABEL
            && ir_next->opc == OPC_ALTS
            && ir_next->src->kind == IMM_INT
            && ir_next->src->val == 0
            && ir_next->next
            && ir_next->next->opc == OPC_MOV
            && ir_next->next->src == ir_next->dst
       )
    {
        return 2;
    }
    return 0;
}

static int
OptimizeCogWrites(IRList *irl)
{
    IR *ir, *ir_next, *ir_prev;
    int change = 0;
    int x;
    ir = irl->head;
    ir_prev = NULL;
    while (ir) {
        ir_next = ir->next;
        x = IsMovIndirect(ir, ir_prev, ir_next);
        if (x)
        {
            Operand *src = ir->src;
            Operand *dst = ir_next->src;

            if (!strcmp(src->name, ir_prev->dst->name)) {
                src = ir_prev->dst;
            }
            if (!strcmp(dst->name, ir_prev->dst->name)) {
                dst = ir_prev->dst;
            }
            if (dst->kind == IMM_COG_LABEL) {
                dst = FindNamedOperand(irl, dst->name, dst->val);
            }
            if (src->kind == IMM_COG_LABEL) {
                src = FindNamedOperand(irl, src->name, src->val);
            }
            if (1) {
                ReplaceOpcode(ir, OPC_MOV);
                ir->dst = dst;
                ir->src = src;
                ir_prev->opc = OPC_DUMMY;
                ir_next->opc = OPC_DUMMY;
                ir_next->next->opc = OPC_DUMMY;
                change = 1;
            }
        }
        ir_prev = ir;
        ir = ir_next;
    }
    return change;
}

/* the code generator produces some obviously silly sequences
 * like:  mov T, A; op T, B; mov A, T
 * replace those with op A, B... but ONLY if T is dead after the 3rd one
 */
static int
OptimizeSimpleAssignments(IRList *irl)
{
    int change = 0;
    IR *ir, *ir_next, *ir_prev;
    ir_prev = ir_next = NULL;
    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        ir_prev = ir->prev;
        if (ir_prev && ir_next
                && ir_prev->opc == OPC_MOV && ir_next->opc == OPC_MOV
                && ir_prev->dst == ir_next->src && ir_prev->src == ir_next->dst
                && ir->dst == ir_prev->dst
                && ir_prev->cond == ir_next->cond
                && CondIsSubset(ir_prev->cond,ir->cond)
                && !IsBranch(ir)
                && !IsDummy(ir)
                && !IsLabel(ir)
                && InstrSetsDst(ir)
                && !InstrIsVolatile(ir) && !InstrIsVolatile(ir_prev) && !InstrIsVolatile(ir_next)
                && !InstrSetsAnyFlags(ir_prev)
                && !InstrSetsAnyFlags(ir_next)
                && IsDeadAfter(ir_next, ir_next->src) // This check also rejects HW regs
           )
        {
            ir->dst = ir_next->dst;
            change = 1;
            DeleteIR(irl, ir_next);
            if (IsDeadAfter(ir_prev, ir_prev->dst)) {
                DeleteIR(irl, ir_prev);
                ir_prev = NULL;
            }
        }
        ir = ir->next;
    }
    return change;
}
/* perform P2 specific optimizations */
int
OptimizeP2(IRList *irl)
{
    IR *ir, *ir_next;
    IR *previr;
    int changed = 0;
    int opc;

    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        while (ir_next && IsDummy(ir_next)) {
            ir_next = ir_next->next;
        }
        opc = ir->opc;
        if (opc == OPC_ADDCT1 && IsImmediateVal(ir->src, 0) && ir->cond == COND_TRUE && !InstrIsVolatile(ir)) {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_ADD && !InstrSetsAnyFlags(previr)) {
                // add foo, val / addct1 foo, #0 -> addct1 foo, val
                ir->src = previr->src;
                DeleteIR(irl, previr);
                changed = 1;
            }
        }
        if ( (opc == OPC_DJNZ || opc == OPC_JUMP) && ir->cond == COND_TRUE && !InstrIsVolatile(ir)) {
            // see if we can change the loop to use "repeat"
            Operand *var;
            Operand *dst;
            IR *labir = NULL;
            IR *pir = NULL;
            IR *repir = NULL;
            bool canRepeat = true;
            bool didAnything = false;

            if (opc == OPC_DJNZ) {
                var = ir->dst;
                dst = ir->src;
            } else {
                var = NULL;
                dst = ir->dst;
            }
            if (var == NULL || IsDeadAfter(ir, var)) {
                for (pir = ir->prev; pir; pir = pir->prev) {
                    if (pir->opc == OPC_LABEL) {
                        if (pir->dst != dst) {
                            canRepeat = false;
                        }
                        break;
                    } else if (IsBranch(pir) || pir->opc == OPC_BREAK) {
                        canRepeat = false;
                        break;
                    } else if (var && (pir->src == var || pir->dst == var)) {
                        canRepeat = false;
                        break;
                    } else if (!IsDummy(pir)) {
                        didAnything = true;
                    }
                }
                if (pir && canRepeat && didAnything) {
                    pir = pir->prev;   // WARNING: could be NULL, but InsertAfterIR will handle that
                    labir = NewIR(OPC_LABEL);
                    labir->dst = NewCodeLabel();
                    repir = NewIR(OPC_REPEAT);
                    repir->dst = labir->dst;
                    repir->src = var ? var : NewImmediate(0);
                    repir->aux = labir;
                    AppendLblJump(labir,repir);
                    InsertAfterIR(irl, pir, repir);
                    InsertAfterIR(irl, ir, labir);
                    ir->dst = dst;
                    ir->src = NULL;
                    ir->opc = OPC_REPEAT_END;
                    ir->instr = NULL;
                    changed = 1;
                }
            }
        }
        ir = ir_next;
    }
    return changed;
}

static int
MemoryOpSize(IR *ir) {
    if (!ir) {
        return 0;
    }
    switch (ir->opc) {
    case OPC_RDBYTE:
    case OPC_WRBYTE:
        return 1;
    case OPC_RDWORD:
    case OPC_WRWORD:
        return 2;
    case OPC_RDLONG:
    case OPC_WRLONG:
        return 4;
    default:
        return 0;
    }
}

static bool IsMemoryOrderSafe(Operand *op) {
    if (!(curfunc->optimize_flags & OPT_AGGRESSIVE_MEM)) {
        return false;
    }
    // maybe more conditions later?
    return true;
}

//
// find the next rdlong that uses src
// returns NULL if we spot anything that changes src, dest,
// memory, or a branch
//
static IR* FindNextRead(IR *irorig, Operand *dest, Operand *src, bool branch_ok)
{
    IR *ir;
    int32_t offset = 0;
    for ( ir = irorig->next; ir; ir = ir->next) {
        if (IsDummy(ir)) continue;
        if (ir->opc == OPC_LABEL) {
            return NULL;
        }
        if (IsBranch(ir)) {
            if (ir->opc == OPC_CALL && isMulDivFunc(ir->dst) && !IsCallThatUsesReg(ir, src) && !IsCallThatUsesReg(ir, dest)) {
                // Do nothing
            } else if (branch_ok && IsJump(ir) && ir->cond != COND_TRUE) {
                // Also do nothing
            } else {
                return NULL;
            }
        }
        #if 0
        if (ir->cond != irorig->cond) {
            return NULL;
        }
        #endif
        if (ir->src == src && offset == 0 && (ir->opc == OPC_RDLONG||ir->opc == OPC_RDWORD||ir->opc == OPC_RDBYTE)) {
            return ir;
        }
        if ((ir->opc == OPC_ADD || ir->opc == OPC_SUB) && ir->dst == src && ir->src && ir->src->kind == IMM_INT) {
            if (ir->cond == irorig->cond) {
                offset += AddSubVal(ir);
            } else {
                return NULL;
            }
        } else if (InstrModifies(ir, dest) || InstrModifies(ir, src)) {
            return NULL;
        }
        if (IsWrite(ir)) {
            if (ir->src == src && IsMemoryOrderSafe(src) && (ir->opc == OPC_WRLONG || ir->opc == OPC_WRWORD || ir->opc == OPC_WRBYTE)
                    && (offset >= MemoryOpSize(irorig) || offset <= -MemoryOpSize(ir))) {
                /* write does not conflict, should be okay */
            } else {
                return NULL;
            }
        }
    }
    return NULL;
}

// true if ir represents a real instruction which does not
// read/write memory
static bool
IsNonReadWriteOpcode(IR *ir)
{
    int opc;

    if (!ir) {
        return false;
    }
    opc = (int)ir->opc;
    if (opc < OPC_GENERIC && !IsReadWrite(ir)) {
        return true;
    }
    return false;
}

//
// return true if it's OK to swap IR a and b
// if a changes b's src, then no
//
static bool
CanSwap(IR *a, IR *b)
{
    if (InstrSetsAnyFlags(a) || InstrSetsAnyFlags(b)) return false;
    if (a->cond != b->cond) return false;
    if (InstrIsVolatile(a) || InstrIsVolatile(b)) return false;
    if (IsBranch(a) || IsBranch(b)) return false;
    if (InstrModifies(a, b->src)) return false;
    if (InstrModifies(a, b->dst)) return false;
    if (InstrUses(b, a->dst)) return false;
    if (InstrModifies(b, a->src)) return false;
    if (InstrUses(a, b->dst)) return false;
    return true;
}

//
// optimize read/write calls
//   rdlong a,b
//   rdlong c,b
// can become
//   rdlong a, b
//   mov    c, a
// also in:
//   rdbyte a, b
//   and a, #255
// we can skip the "and"

static int
OptimizeReadWrite(IRList *irl)
{
    Operand *base;
    Operand *dst1;
    IR *ir;
    IR *nextread;
    IR *next_ir, *prev_ir;
    int change = 0;

restart_check:
    prev_ir = next_ir = NULL;
    ir = irl->head;
    while (ir) {
        next_ir = ir->next;
        while (next_ir && IsDummy(next_ir)) {
            next_ir = next_ir->next;
        }
        if (InstrIsVolatile(ir)) {
            goto get_next;
        }
        if (ir->srceffect != OPEFFECT_NONE || ir->dsteffect != OPEFFECT_NONE) {
            goto get_next;
        }
        if (ir->opc == OPC_RDLONG || ir->opc == OPC_WRLONG
                ||  ir->opc == OPC_RDWORD || ir->opc == OPC_WRWORD
                ||  ir->opc == OPC_RDBYTE || ir->opc == OPC_WRBYTE) {
            // don't mess with it if prev instr was OPC_SETQ
            if (prev_ir && (prev_ir->opc == OPC_SETQ || prev_ir->opc == OPC_SETQ2)) {
                prev_ir->flags |= FLAG_KEEP_INSTR;
                goto get_next;
            }
            dst1 = ir->dst;
            base = ir->src;
            int size = MemoryOpSize(ir);
            bool write = IsWrite(ir);
            // don't mess with it if src==dst
            if (!write && ir->src == ir->dst) goto get_next;
            nextread = FindNextRead(ir, dst1, base, (curfunc->optimize_flags & OPT_EXPERIMENTAL) && IsMemoryOrderSafe(ir->src));
            int nextsize = MemoryOpSize(nextread);
            if (nextread && CondIsSubset(ir->cond,nextread->cond)) {
                // wrlong a, b ... rdlong c, b  -> mov c, a
                // rdlong a, b ... rdlong c, b  -> mov c, a
                if(size == nextsize && (!write || size==4 || gl_p2) 
                        && ((gl_p2 && size==4) || !InstrSetsFlags(nextread,FLAG_WC))
                        && (!write || size==4 || !InstrSetsFlags(nextread,FLAG_WZ))) {
                    nextread->src = dst1;
                    if (!write || size == 4) {
                        ReplaceOpcode(nextread, OPC_MOV);
                    } else if (size == 2) {
                        ReplaceOpcode(nextread,OPC_GETWORD);
                        nextread->src2 = NewImmediate(0);
                    } else if (size == 1) {
                        ReplaceOpcode(nextread,OPC_GETBYTE);
                        nextread->src2 = NewImmediate(0);
                    }
                    change = 1;
                    goto get_next;
                }
            }
#if 1
            // bit of a hacky optimization for some tests
            // rdlong a, b + mov c, a -> rdlong c, b if a is dead
            else if (IsRead(ir) && next_ir && next_ir->opc == OPC_MOV
                    && !InstrIsVolatile(next_ir) && !InstrSetsAnyFlags(next_ir)
                    && SameRegister(next_ir->src, ir->dst)
                    && IsLocalOrArg(next_ir->src) && ir->cond == next_ir->cond
                    && IsDeadAfter(next_ir, ir->dst)) {
                Operand *tmp = ir->dst;
                ir->dst = next_ir->dst;
                next_ir->dst = tmp;
                ReplaceOpcode(next_ir, OPC_MOV);
                change = 1;
                goto get_next;
            }
#endif
        }
        if (ir->opc == OPC_RDBYTE || ir->opc == OPC_RDWORD) {
            int32_t mval;
            dst1 = ir->dst;
            int mask = ir->opc == OPC_RDBYTE ? 0xFF : 0xFFFF;
            nextread = FindNextUse(ir, dst1);
            if (nextread
                    && nextread->cond == ir->cond
                    && !FlagsChangeInRange(ir->next,nextread->prev,FlagsUsedByCond(ir->cond))
                    && nextread->dst == dst1
                    && (nextread->flags & FLAG_WZ) == nextread->flags
                    && isMaskingOp(nextread,&mval)
                    && mval == mask)
            {
                // don't need zero extend after rdbyte (change to MOV, gets eliminated next pass)
                ReplaceOpcode(nextread,OPC_MOV);
                nextread->src = nextread->dst;
                change = 1;
            }
        }
        // cut unneccessary bits for immediate write values
        if (gl_p2 && (ir->opc == OPC_WRBYTE || ir->opc == OPC_WRWORD) && ir->dst && ir->dst->kind == IMM_INT) {
            int mask = ir->opc == OPC_WRBYTE ? 0xFF : 0xFFFF;
            if ((ir->dst->val & mask) != ir->dst->val) {
                ir->dst = NewImmediate(ir->dst->val & mask);
                change = 1;
            }
        }
#if 1
        // try to avoid having two read/write ops in a row
        if (IsReadWrite(ir) && IsReadWrite(next_ir) && IsNonReadWriteOpcode(prev_ir)) {
            if (CanSwap(ir, prev_ir) && !IsCordicCommand(prev_ir)) {
                // want to swap prev_ir and ir here
                DeleteIR(irl, prev_ir);  // remove prev_ir from list
                prev_ir->next = NULL;
                InsertAfterIR(irl, ir, prev_ir); // move it to later
                ir = prev_ir;
                change = 1;
                goto restart_check;
            }
        }
#endif
get_next:
        prev_ir = ir;
        ir = next_ir;
    }
    return change;
}

// Pull matching add/sub instructions out of loops
static int
OptimizeLoopPtrOffset(IRList *irl) {
    int change = 0;

    // Find loops
    for (IR *ir=irl->head; ir; ir=ir->next) {
        IR *end;
        if (IsLabel(ir) && ir->prev && (end = UniqJumpForLabel(ir)) && IsJump(end) && !IsForwardJump(end)) {
            // Find top add/sub
            IR *nexttop;
            for(IR *top=ir->next; top&&top!=end; top=nexttop) {
                nexttop = top->next;
                if (!IsDummy(top)
                        && (top->opc == OPC_ADD || top->opc == OPC_SUB)
                        && top->cond == COND_TRUE && top->src->kind == IMM_INT
                        && !HasSideEffectsOtherThanReg(top)) {
                    // Try to find matching sub/add that's safe to move out of the loop
                    for(IR *bot=end->prev; bot&&bot!=top; bot=bot->prev) {
                        if (!IsDummy(bot) && 1
                                && (bot->opc == OPC_ADD || bot->opc == OPC_SUB)
                                && bot->dst == top->dst && bot->src->kind == IMM_INT
                                && bot->cond == COND_TRUE
                                && !HasSideEffectsOtherThanReg(bot)
                                && AddSubVal(bot) == 0-AddSubVal(top)
                                && !UsedInRange(ir->next,top->prev,top->dst) && !UsedInRange(bot->next,end->prev,top->dst)
                                && !ModifiedInRange(ir->next,top->prev,top->dst) && !ModifiedInRange(bot->next,end->prev,top->dst)
                                && !ModifiedInRange(top->next,bot->prev,top->dst)) {
                            DoReorderBlock(irl,ir->prev,top,top);
                            DoReorderBlock(irl,end,bot,bot);
                            change++;
                            break;
                        }
                    }
                }
            }
        }
    }

    return change;
}

//
// optimize for tail calls
static int
OptimizeTailCalls(IRList *irl, Function *f)
{
    IR *ir = irl->head;
    IR *irnext;
    int change = 0;

    if (f->local_address_taken) {
        // &local_var; do not try to optimize
        return change;
    }
    while (ir) {
        if (ir->opc == OPC_CALL && ir->dst == FuncData(f)->asmname) {
            irnext = NextInstructionFollowJumps(ir);
            if (!irnext)
            {
                ReplaceOpcode(ir, OPC_JUMP);
                ir->dst = FuncData(f)->asmentername;
                change = 1;
            }
        }
        ir = ir->next;
    }
    return change;
}

// optimize jumps: jmp #A where the instruction after A is jmp #B gets
// turned into jmp #B
static int
OptimizeJumps(IRList *irl)
{
    IR *ir = irl->head;
    IR *jmpdest;
    int change = 0;

    while (ir) {
        // internal jumptables are marked volatile (to avoid removal) but
        // we should allow jmp to jmp, so they're also marked with
        // FLAG_JMPTABLE_INSTR
        if (ir->opc == OPC_JUMP && (!InstrIsVolatile(ir) || ir->flags & FLAG_JMPTABLE_INSTR)) {
            // ptr to jump destination (if known) is in aux; see if it's also a jump
            jmpdest = NextInstruction((IR *)ir->aux);
            if (jmpdest && jmpdest->opc == OPC_JUMP && jmpdest->cond == COND_TRUE && jmpdest->aux && jmpdest != ir && jmpdest->dst != ir->dst) {
                ir->dst = jmpdest->dst;
                ir->aux = jmpdest->aux;
                change++;
            }
        }
        ir = ir->next;
    }
    return change;
}

static bool IsReorderBarrier(IR *ir) {
    if (!ir||InstrIsVolatile(ir)||IsBranch(ir)||IsLabel(ir)) return true;
    if (ir->dst&&IsHwReg(ir->dst)) return true;
    if (ir->src&&IsHwReg(ir->src)) return true;
    switch (ir->opc) {
    case OPC_GENERIC:
    case OPC_GENERIC_NR:
    case OPC_GENERIC_NR_NOFLAGS:
    case OPC_GENERIC_DELAY:
    case OPC_GENERIC_NOFLAGS:

    case OPC_LOCKCLR:
    case OPC_LOCKNEW:
    case OPC_LOCKRET:
    case OPC_LOCKSET:
    case OPC_LOCKTRY:
    case OPC_LOCKREL:
    case OPC_WAITCNT:
    case OPC_WAITX:
    case OPC_COGSTOP:
    case OPC_ADDCT1:
    case OPC_HUBSET:

    case OPC_QDIV: // TODO
    case OPC_QFRAC:
    case OPC_QMUL:
    case OPC_QSQRT:
    case OPC_QROTATE:
    case OPC_QVECTOR:
    case OPC_QLOG:
    case OPC_QEXP:

    case OPC_GETQX: // TODO
    case OPC_GETQY:

    case OPC_DRVC:
    case OPC_DRVNC:
    case OPC_DRVL:
    case OPC_DRVH:
    case OPC_DRVNZ:
    case OPC_DRVZ:
    case OPC_PUSH:
    case OPC_POP:
    case OPC_FCACHE:
    case OPC_MUXQ:
        return true;
    default:
        return false;
    }
}

struct reorder_block {
    unsigned count; // If zero, everything else is invalid
    IR *top,*bottom;
};

static struct reorder_block
FindBlockForReorderingDownward(IR *after) {
    IR *bottom = after;
    DEBUG(NULL,"Looking for a block to move down... (in %s)",curfunc->name);
    // Are the flags used at after?
    bool volatileC = FlagsUsedAt(after,FLAG_WC);
    bool volatileZ = FlagsUsedAt(after,FLAG_WZ);
    // Encountered a flag write on the way up?
    // (if a block uses a flag, but we didn't find a flag write inbetween,
    // that is the same flag state as after)
    bool foundC = InstrSetsFlags(after,FLAG_WC), foundZ = InstrSetsFlags(after,FLAG_WZ);
    // Encountered a flag read on the way up?
    bool dependC = InstrUsesFlags(after,FLAG_WC),dependZ = InstrUsesFlags(after,FLAG_WZ);
    struct dependency *depends = NULL;
    // Hunt for the start of a potential reordering block
    for (;;) {
        bottom = bottom->prev;
        if (!bottom || IsReorderBarrier(bottom)) return (struct reorder_block) {
            0
        };
        IR *top;
        // Need closure on a flag? (In this case, a setter)
        bool needC = false, needZ = false;
        unsigned count = 0;

        for (top=bottom; top; top=top->prev) {
            count++;
            if (IsReorderBarrier(top)) break;
            if (InstrSetsFlags(top,FLAG_WC)) {
                needC = false;
                if (volatileC && foundC) break;
                if (dependC) break;
            }
            if (InstrSetsFlags(top,FLAG_WZ)) {
                needZ = false;
                if (volatileZ && foundZ) break;
                if (dependZ) break;
            }
            if (foundC && InstrUsesFlags(top,FLAG_WC)) needC = true;
            if (foundZ && InstrUsesFlags(top,FLAG_WZ)) needZ = true;
            // Can only reorder reads with reads
            if (IsWrite(top) && ReadWriteInRange(bottom->next,after)) break;
            if (IsRead(top) && WriteInRange(bottom->next,after)) break;

            if (InstrSetsDst(top)) {
                // Can't reorder over dependent code
                if (UsedInRange(bottom->next,after,top->dst)) break;
                // Can't reorder over code that sets a non-dead value
                if (ModifiedInRange(bottom->next,after,top->dst) && !IsDeadAfter(after,top->dst)) break;
                // Check if this instruction meets some dependencies
                DeleteDependencies(&depends,top->dst);
            }
            // Does _this_ depend on anything that we can't take through reorder?
            if (ModifiedInRange(bottom->next,after,top->src)) {
                PrependDependency(&depends,top->src);
            }
            if (InstrReadsDst(top) && ModifiedInRange(bottom->next,after,top->dst)) {
                PrependDependency(&depends,top->dst);
            }
            // Ok, if there are no unmet dependencies, the block is good.
            if (!needC && !needZ && !depends && (!top->prev || !IsPrefixOpcode(top->prev))) {
                return (struct reorder_block) {
                    .count=count,.top=top,.bottom=bottom
                };
            }
        }
        // broke out of loop, this block is invalid
        if (InstrSetsFlags(bottom,FLAG_WC)) {
            foundC = true;
            dependC = false;
        }
        if (InstrSetsFlags(bottom,FLAG_WZ)) {
            foundZ = true;
            dependZ = false;
        }
        if (InstrUsesFlags(bottom,FLAG_WC)) dependC = true;
        if (InstrUsesFlags(bottom,FLAG_WZ)) dependZ = true;
        DeleteDependencyList(&depends);
    }
}

static struct reorder_block
FindBlockForReorderingUpward(IR *before) {
    IR *top = before;
    DEBUG(NULL,"Looking for a block to move up... (in %s)",curfunc->name);
    // Are the flags used at before?
    bool volatileC = FlagsUsedAt(before,FLAG_WC);
    bool volatileZ = FlagsUsedAt(before,FLAG_WZ);
    //DEBUG(NULL,"Volatile C: %s , Volatile Z: %s",volatileC?"Y":"n",volatileZ?"Y":"n");
    // Encountered a flag write on the way down?
    bool foundC = InstrSetsFlags(before,FLAG_WC), foundZ = InstrSetsFlags(before,FLAG_WZ);
    struct dependency *depends = NULL;
    // Hunt for the start of a potential reordering block
    for (;;) {
        top = top->next;
        if (!top || IsReorderBarrier(top)) return (struct reorder_block) {
            0
        };
        IR *bottom;
        // Need closure on a flag? (In this case, the last user)
        bool needC = false, needZ = false;
        // Have a flag setter in the block?
        bool foundClocal = false, foundZlocal = false;
        unsigned count = 0;
        for (bottom=top; bottom; bottom=bottom->next) {
            count++;
            if (IsReorderBarrier(bottom)) break;
            // if we use a flag that isn't set inside the block but we want to reorder over another flag write, abort
            if (foundC && !foundClocal && InstrUsesFlags(bottom,FLAG_WC)) break;
            if (foundZ && !foundZlocal && InstrUsesFlags(bottom,FLAG_WZ)) break;
            // If we write a flag, but there's another write we want to move over
            // we need to pull instructions until the flag is dead
            if (InstrSetsFlags(bottom,FLAG_WC)) {
                if (volatileC) break;
                if (foundC) needC = true;
                foundClocal = true;
            }
            if (InstrSetsFlags(bottom,FLAG_WZ)) {
                if (volatileZ) break;
                if (foundZ)  needZ = true;
                foundZlocal = true;
            }
            // Flags dead?
            if (needC && !FlagsUsedAt(bottom->next,FLAG_WC)) needC = false;
            if (needZ && !FlagsUsedAt(bottom->next,FLAG_WZ)) needZ = false;
            // Can only reorder reads with reads
            if (IsWrite(bottom) && ReadWriteInRange(before,bottom->prev)) break;
            if (IsRead(bottom) && WriteInRange(before,bottom->prev)) break;
            // Can't reorder over code depending on another value for dst
            if (InstrSetsDst(bottom) && UsedInRange(before,top->prev,bottom->dst)) break;
            // Can't reorder over code this depends on
            if (ModifiedInRange(before,top->prev,bottom->src) && UsedInRange(top,bottom,bottom->src)) break;
            if (InstrReadsDst(bottom)&&ModifiedInRange(before,top->prev,bottom->dst) && UsedInRange(top,bottom,bottom->dst)) break;
            // if this result is modified by code we reorder over, it must become dead at the end of the block
            if (InstrSetsDst(bottom) && ModifiedInRange(before,top->prev,bottom->dst)) {
                PrependDependency(&depends,bottom->dst);
            }
            // Delete dependencies that go dead
            if (bottom->src && IsDeadAfter(bottom,bottom->src)) DeleteDependencies(&depends,bottom->src);
            if (InstrReadsDst(bottom) && IsDeadAfter(bottom,bottom->dst)) DeleteDependencies(&depends,bottom->dst);

            // Ok, if there are no unmet dependencies, the block is good.
            if (!needC && !needZ && !depends && !IsPrefixOpcode(bottom)) {
                return (struct reorder_block) {
                    .count=count,.top=top,.bottom=bottom
                };
            }
        }
        // broke out of loop, this block is invalid
        if (InstrSetsFlags(top,FLAG_WC)) foundC = true;
        if (InstrSetsFlags(top,FLAG_WZ)) foundZ = true;
        DeleteDependencyList(&depends);
    }
}

#define CORDIC_PIPE_LENGTH 56

// Try to move code between QMUL/QDIV and GETQ* to amortize CORDIC latency
static bool
OptimizeCORDIC(IRList *irl) {
    // Search for QMUL/QDIV
    bool change = false;
    for (IR *ir=irl->tail; ir; ir=ir->prev) {
        if (!IsCordicCommand(ir) || InstrIsVolatile(ir)) continue;
        if(IsHwReg(ir->dst)||IsHwReg(ir->src)) continue;
        if(IsSubReg(ir->dst)||IsSubReg(ir->src)) continue;

        int cycles = 0;
        // Count min-cycles already inbetween command and get
        for (IR *ir2=ir->next; ir2; ir2=ir2->next) {
            if (IsCordicGet(ir2)) break;
            cycles += InstrMinCycles(ir2);
        }
        // Try pulling down blocks
        // (The cycle limit only applies in pathological cases)
        while (cycles<CORDIC_PIPE_LENGTH) {
            struct reorder_block blk = FindBlockForReorderingDownward(ir);
            if (blk.count == 0) break; // No block found
            DEBUG(NULL,"Reordering block of %d instructions down",blk.count);
            DoReorderBlock(irl,ir,blk.top,blk.bottom);
            cycles += MinCyclesInRange(blk.top,blk.bottom);
            change = true;
        }
    }
    // Search for GETQ*
    for (IR *ir=irl->head; ir; ir=ir->next) {
        if(!IsCordicGet(ir)) continue;
        if(InstrIsVolatile(ir)) continue;
        if(IsHwReg(ir->dst)) continue;
        if(IsSubReg(ir->dst)) continue;
        int cycles = 0;
        // Count min-cycles already inbetween command and get
        for (IR *ir2=ir->prev; ir2; ir2=ir2->prev) {
            if (IsCordicCommand(ir2)) break;
            cycles += InstrMinCycles(ir2);
        }
        // Try pulling up blocks
        // (The cycle limit only applies in pathological cases)
        while (cycles<CORDIC_PIPE_LENGTH) {
            struct reorder_block blk = FindBlockForReorderingUpward(ir);
            if (blk.count == 0) break; // No block found
            DEBUG(NULL,"Reordering block of %d instructions up",blk.count);
            DoReorderBlock(irl,ir->prev,blk.top,blk.bottom);
            cycles += MinCyclesInRange(blk.top,blk.bottom);
            change = true;
        }
    }
    return change;
}

//
static bool
CORDICconstPropagate(IRList *irl) {
    bool constantCommand=false,change=false,foundX=false,foundY=false;
    int32_t const_x=0,const_y=0;
    uint64_t setq1=0;
    bool setq1_valid = true;

    for(IR *ir=irl->head; ir; ir=ir->next) {
        if (IsCordicCommand(ir) && !InstrIsVolatile(ir) && !IsPrefixOpcode(ir->prev)
                && ir->dst && ir->dst->kind == IMM_INT
                && ir->src && ir->src->kind == IMM_INT) {
            constantCommand = true;
            foundX = false;
            foundY = false;
            switch(ir->opc) {
            case OPC_QMUL: {
                DEBUG(NULL,"Got const QMUL %ld,%ld",(long)ir->dst->val,(long)ir->src->val);
                uint64_t tmp = (uint64_t)((uint32_t)ir->dst->val) * (uint32_t)ir->src->val;
                const_x = (int32_t)tmp;
                const_y = (int32_t)(tmp>>32);
            }
            break;
            case OPC_QDIV: {
                uint64_t fullval;
                if (setq1_valid) {
                    fullval = (setq1 << 32) | (uint32_t)ir->dst->val;
                    if (ir->src->val == 0) {
                        const_x = 0xFFFFFFFF;
                        const_y = (int32_t)fullval;
                    } else {
                        const_x = (uint32_t)(fullval / (uint32_t)ir->src->val);
                        const_y = (uint32_t)(fullval % (uint32_t)ir->src->val);
                    }
                } else {
                    constantCommand = false;
                }
            }
            break;
            default: // other ops make brain hurt, owie
                constantCommand = false;
                break;
            }
            if (constantCommand) {
                DeleteIR(irl,ir);
                change = true;
            }
        } else if (constantCommand&&(ir->opc==OPC_GETQX || ir->opc==OPC_GETQY)) {
            if (ir->opc==OPC_GETQX) {
                if (foundX) WARNING(NULL,"Internal warning, found two GETQX during constant propagate??");
                foundX = true;
                ir->src = NewImmediate(const_x);
            } else {
                if (foundY) WARNING(NULL,"Internal warning, found two GETQY during constant propagate??");
                foundY = true;
                ir->src = NewImmediate(const_y);
            }
            ReplaceOpcode(ir,OPC_MOV);
        } else if (constantCommand&&(IsBranch(ir)||IsLabel(ir)||IsCordicCommand(ir))) {
            if (!foundX && !foundY) WARNING(NULL,"Internal warning, unused CORDIC constant??");
            constantCommand = false;
            setq1_valid = false;
        } else if (ir->opc == OPC_SETQ) {
            if (ir->dst->kind == IMM_INT) {
                setq1_valid = true;
                setq1 = (uint32_t)ir->dst->val;
            } else {
                setq1_valid = false;
            }
        }
    }
    if (constantCommand && !foundX && !foundY) WARNING(NULL,"Internal warning, unused CORDIC constant at end of function??");

    return change;
}

// Optimization may have created lone CORDIC commands
// which will lead to strange results.
// Thus, we shall remove these.
static bool
FixupLoneCORDIC(IRList *irl) {
    bool seenCommand = true, change = false;
    for(IR *ir=irl->tail; ir; ir=ir->prev) {
        if (IsCordicCommand(ir)) {
            if (seenCommand && !InstrIsVolatile(ir)) {
                if (IsPrefixOpcode(ir->prev)) DeleteIR(irl,ir->prev);
                if (ir->flags & FLAG_USER_INSTR) {
                    WARNING(ir->line, "Deleting apparently unused cordic instruction %s", ir->instr->name);
                }
                DeleteIR(irl,ir);
                change = true;
            } else seenCommand = true;
        } else if (IsCordicGet(ir)) {
            seenCommand = false;
        } else if (IsBranch(ir)||IsLabel(ir)) {
            seenCommand = true;
        }
    }
    return change;
}

static void addKnownReg(struct dependency **list, Operand *op, bool arg) {
    if (op && op->kind != REG_SUBREG && (arg?IsArg(op)||isResult(op):IsLocal(op)) && !CheckDependency(list,op)) PrependDependency(list,op);
}

static bool
ReuseLocalRegisters(IRList *irl) {
    struct dependency *known_regs = NULL;
    bool change = false;
    IR *stop_ir;

     for(IR *ir=irl->head; ir; ir=ir->next) {
        // Find all the arg/result regs first
        addKnownReg(&known_regs,ir->src,true);
        addKnownReg(&known_regs,ir->dst,true);
    }

    for (IR *ir=irl->head; ir; ir=ir->next) {
        // Start of new dependency chain
        if (ir->dst && ir->dst != ir->src && IsLocal(ir->dst) && ir->dst->kind != REG_SUBREG && InstrModifies(ir,ir->dst) && !InstrUses(ir,ir->dst) && !InstrIsVolatile(ir) && !CheckDependency(&known_regs,ir->dst)) {
            for (struct dependency *tmp=known_regs; tmp; tmp=tmp->link) {
                if (tmp->reg!=ir->dst && IsDeadAfter(ir,tmp->reg) && (stop_ir = SafeToReplaceForward(ir->next,ir->dst,tmp->reg,ir->cond))) {
                    //DEBUG(NULL,"Using %s instead of %s",tmp->reg->name,ir->dst->name);
                    ReplaceForward(ir->next,ir->dst,tmp->reg,stop_ir);
                    ir->dst = tmp->reg;
                    change = true;
                    break;
                }
            }
        }
        // Collect known registers
        addKnownReg(&known_regs,ir->src,false);
        addKnownReg(&known_regs,ir->dst,false);
    }
    return change;
}


// Change calls to builtin_longfill_ on P2 to SETQ+WRLONG
int
OptimizeLongfill(IRList *irl) {
    int change = 0;
    if (!gl_p2) return 0;
    for (IR *ir=irl->head; ir; ir=ir->next) {
        IR *prevset;
        int32_t setval;
        if (IsDummy(ir)) continue;
        if (ir->opc == OPC_CALL && !CondIsSubset(COND_C,ir->cond) && !strcmp(ir->dst->name,"builtin_longfill_")
                && (prevset = FindPrevSetterForReplace(ir,GetArgReg(1)))
                && isConstMove(prevset,&setval)) {
            int addr = ir->addr; // Some opts require addresses to be sorta-correct;
            // Since we replace a funccall, we can clobber flags and args
            IR *sub = NewIR(OPC_SUB);
            sub->dst = GetArgReg(2);
            sub->src = NewImmediate(1);
            sub->flags = FLAG_WC;
            sub->addr = addr;
            sub->cond = ir->cond;
            IR *setq = NewIR(OPC_SETQ);
            setq->cond = COND_NC;
            setq->dst = GetArgReg(2);
            setq->cond = (IRCond)(COND_NC | ir->cond);
            setq->addr = addr;
            IR *wrlong = NewIR(OPC_WRLONG);
            wrlong->cond = (IRCond)(COND_NC | ir->cond);
            wrlong->dst = NewImmediate(setval);
            wrlong->src = GetArgReg(0);
            wrlong->addr = addr;

            InsertAfterIR(irl,ir,wrlong);
            InsertAfterIR(irl,ir,setq);
            InsertAfterIR(irl,ir,sub);
            DeleteIR(irl,ir);
            change++;
        }
    }
    return change;
}


static void append_disasm(Flexbuf *fb,IRList *irl) {
    char *buf = IRAssemble(irl,NULL);
    flexbuf_addstr(fb,buf);
    free(buf);
}

#define OPT_PASS(call) { \
    int passchg = call;\
    if (hooked) {\
        DEBUG(NULL,"%s Opt pass " #call " change: %d",FuncData(curfunc)->asmname->name,passchg);\
        flexbuf_addstr(&hookfb,"Pass " #call ":\n");\
        if (passchg) {\
            append_disasm(&hookfb,irl);\
        } else {\
            flexbuf_addstr(&hookfb,"NO CHANGE!\n");\
        }\
        flexbuf_addstr(&hookfb,"\n");\
    }\
    change |= passchg;\
}

// optimize an isolated piece of IRList
// (typically a function)
void
OptimizeIRLocal(IRList *irl, Function *f)
{
    int change;
    int flags = f->optimize_flags;
    if (gl_errors > 0) return;
    if (!irl->head) return;
    
    Flexbuf hookfb;
    bool hooked = false;//!strcmp(FuncData(curfunc)->asmname->name,"_libc_a_fopen");
    if (hooked) {
        flexbuf_init(&hookfb,512);
        flexbuf_addstr(&hookfb,"OPTIMIZE_IR DEBUG HOOK LOG for");
        flexbuf_addstr(&hookfb,FuncData(curfunc)->asmname->name);
        flexbuf_addstr(&hookfb,"\n\nInital:\n");
        append_disasm(&hookfb,irl);
    }

    // multiply divide optimization need only be performed once,
    // and should be done before other optimizations confuse things
    OptimizeMulDiv(irl);
    // Similarily for longfill (opt can only become available from inlining)
    OptimizeLongfill(irl);
again:
    do {
        change = 0;
        AssignTemporaryAddresses(irl);
        if (flags & OPT_BASIC_REGS) {
            OPT_PASS(CheckLabelUsage(irl));
            OPT_PASS(OptimizeReadWrite(irl));
            OPT_PASS(EliminateDeadCode(irl));
            OPT_PASS(OptimizeCogWrites(irl));
            OPT_PASS(OptimizeSimpleAssignments(irl));
            OPT_PASS(OptimizeMoves(irl));
        }
        if (flags & OPT_CONST_PROPAGATE) {
            OPT_PASS(OptimizeImmediates(irl));
        }
        if (flags & OPT_BASIC_REGS) {
            OPT_PASS(OptimizeCompares(irl));
            OPT_PASS(OptimizeAddSub(irl));
            OPT_PASS(OptimizeLoopPtrOffset(irl));
        }
        if (flags & OPT_PEEPHOLE) {
            OPT_PASS(OptimizePeepholes(irl));
            OPT_PASS(OptimizePeephole2(irl));
        }
        if (flags & OPT_BRANCHES) {
            if (flags & OPT_EXPERIMENTAL) {
                OPT_PASS(OptimizeBranchCommonOps(irl));
            }
            OPT_PASS(OptimizeShortBranches(irl));
        }
        if (flags & OPT_BASIC_REGS) {
            OPT_PASS(OptimizeIncDec(irl));
            OPT_PASS(OptimizeJumps(irl));
        }
        if (gl_p2 && (flags & OPT_BASIC_REGS)) {
            OPT_PASS(OptimizeP2(irl));
        }
        if (gl_p2) {
            OPT_PASS(FixupLoneCORDIC(irl));
            if (flags & OPT_CONST_PROPAGATE) {
                OPT_PASS(CORDICconstPropagate(irl));
            }
        }
    } while (change != 0);
    if (flags & OPT_TAIL_CALLS) {
        OPT_PASS(OptimizeTailCalls(irl, f));
    }
    if (change) goto again;
    if (gl_p2 && (flags & OPT_CORDIC_REORDER)) {
        OPT_PASS(OptimizeCORDIC(irl));
    }
    if (change) goto again;
    if (flags & OPT_LOCAL_REUSE) {
        OPT_PASS(ReuseLocalRegisters(irl));
    }
    if (change) goto again;

    if (hooked) {
        char fbuf[256];
        static int hookcnt = 0;
        snprintf(fbuf,255,"opthook_%d_%s.txt",++hookcnt,FuncData(curfunc)->asmname->name);
        FILE *logf = fopen(fbuf,"w");
        fputs(flexbuf_peek(&hookfb),logf);
        fclose(logf);
        flexbuf_delete(&hookfb);
    }
}

//
// optimize the whole program
//
void
OptimizeIRGlobal(IRList *irl)
{
    IR *ir, *ir_next;
    // remove dummy and dead notes
    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        if (ir->opc == OPC_DUMMY) {
            DeleteIR(irl, ir);
        }
        ir = ir_next;
    }
    if (gl_lmm_kind == LMM_KIND_ORIG) {
        // check for fcache
        if (gl_optimize_flags & OPT_AUTO_FCACHE) {
            OptimizeFcache(irl);
        }
    }
    // check for usage
    CheckUsage(irl);
}

static bool
NeverInline(Function *f)
{
    if (f->no_inline) return true;
    if (f->uses_alloca) return true;
    if (f->stack_local) return true;
    if (f->closure) return true;
    if (gl_cenv_flags && !strcmp(f->name, "main")) return true;
    return false;
}

//
// check a function to see if it should be inlined
// and return true if new inlining opportunites were unlocked
//
#define INLINE_THRESHOLD_P1 2
#define INLINE_THRESHOLD_P2 4

bool
AnalyzeInlineEligibility(Function *f)
{
    IR *ir;
    int n = 0;
    int paramfactor;
    int threshold;
    bool pure = true;

    unsigned prev_flags = FuncData(f)->inliningFlags;
    FuncData(f)->inliningFlags = 0;

    if (f->prefer_inline) {
        threshold = 100;
    } else if (gl_p2) {
        threshold = INLINE_THRESHOLD_P2;
    } else {
        threshold = INLINE_THRESHOLD_P1;
    }

    if (!(gl_optimize_flags & (OPT_INLINE_SMALLFUNCS|OPT_INLINE_SINGLEUSE))) {
        return false;
    }
    if (NeverInline(f)) {
        return false;
    }
    for (ir = FuncIRL(f)->head; ir; ir = ir->next) {
        if (IsDummy(ir)) continue;
        // we have to re-label any labels and branches
        if (IsLabel(ir)) {
            if (!ir->aux) {
                // cannot find a unique jump going to this label
                return false;
            }
            if (!IsTemporaryLabel(ir->dst)) {
                return false;
            }
            continue; // do not count labels against the cost
        } else if (IsJump(ir)) {
            if (!ir->aux) {
                // cannot find where this jump goes
                return false;
            }
            if (!IsTemporaryLabel( ((IR *)ir->aux)->dst )) {
                return false;
            }
        }

        // Check for impurity here. This check is very conservative.
        if (pure) {
            if (!IsLabel(ir) && !IsSafeALUOp(ir->opc)) pure = false;
            if (ir->dst && !(IsLocalOrArg(ir->dst) || isResult(ir->dst))) pure = false;
            if (ir->src && !(IsImmediate(ir->src) || IsLocalOrArg(ir->src) || isResult(ir->src))) pure = false;
        }

        n++;
    }

    if (pure) {
        FuncData(f)->inliningFlags |= ASM_INLINE_PURE_FLAG;
    }

    // a function called from only 1 place should be inlined
    // if it means that the function definition can be eliminated
    if (RemoveIfInlined(f) && (gl_optimize_flags & OPT_INLINE_SINGLEUSE)) {
        if (f->callSites <= 1) {
            FuncData(f)->inliningFlags |= ASM_INLINE_SINGLE_FLAG;
        } else if (f->callSites == 2 && (n <= 2*threshold)) {
            FuncData(f)->inliningFlags |= ASM_INLINE_SINGLE_FLAG;
        }
    }

    // otherwise only inline small functions
    // also note that we should consider the cost of moving instructions
    // into argument registers when considering this
    paramfactor = f->numparams;
    if (paramfactor < 2) {
        paramfactor = 2;
    } else if (paramfactor > 4) {
        paramfactor = 4;
    } else {
        paramfactor = f->numparams;
    }
    if (n <= (threshold + paramfactor)) {
            FuncData(f)->inliningFlags |= ASM_INLINE_SMALL_FLAG;
    }

    return FuncData(f)->inliningFlags & ~prev_flags;
}

static inline void updateMax(int *dst, int src) {
    if (*dst < src) *dst = src;
}

static bool
ShouldExpandPureFunction(IR *ir) {
    Function *f = (Function *)ir->aux;
    if (!(FuncData(f)->inliningFlags & ASM_INLINE_PURE_FLAG)) return false;
    if (!(gl_optimize_flags & OPT_EXPERIMENTAL)) return false;
    if (!(gl_optimize_flags & OPT_CONST_PROPAGATE)) return false;
    if (f->numparams <= 0) return false;

    // Make sure all args are constants
    for (int i = 0; i < f->numparams; i++) {
        IR *prev_ir = FindPrevSetterForReplace(ir,GetArgReg(i));
        if (!prev_ir || !isConstMove(prev_ir,NULL)) return false;
    }

    return true;
}

//
// expand function calls inline if appropriate
// returns 1 if anything was expanded
int
ExpandInlines(IRList *irl)
{
    Function *f;
    IR *ir, *ir_next;
    int change = 0;
    int non_inline_calls = 0;

    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        if (ir->opc == OPC_CALL) {
            f = (Function *)ir->aux;
            if (f && ((FuncData(f)->inliningFlags & (ASM_INLINE_SMALL_FLAG|ASM_INLINE_SINGLE_FLAG)) || ShouldExpandPureFunction(ir))) {
                ReplaceIRWithInline(irl, ir, f);
                FuncData(f)->actual_callsites--;
                FuncData(f)->got_inlined = true;
                updateMax(&FuncData(curfunc)->maxInlineArg,f->numparams);
                updateMax(&FuncData(curfunc)->maxInlineArg,FuncData(f)->maxInlineArg);
                if (!f->is_leaf && !FuncData(f)->effectivelyLeaf) non_inline_calls++; // Non-leaf inline may contain call
                change = 1;
            } else {
                non_inline_calls++;
                if (f && FuncData(f)->actual_callsites == 0) {
                    FuncData(f)->actual_callsites = 1;
                }
            }
        }
        ir = ir_next;
    }
    // If inlining (or previous dead-code optimization...) removed all external calls, mark as leaf.
    if (non_inline_calls==0 && !curfunc->is_leaf && !FuncData(curfunc)->effectivelyLeaf) {
        FuncData(curfunc)->effectivelyLeaf = true;
        change = true;
    }
    return change;
}

//
// convert loops to FCACHE when we can
//

static bool IsWaitLoopInstr(IR *ir) {
    switch(ir->opc) {
    case OPC_JUMP:
    case OPC_RDBYTE:
    case OPC_RDWORD:
    case OPC_RDLONG:
    case OPC_LOCKSET:
    case OPC_LOCKTRY:
    case OPC_CMP:
    case OPC_CMPS:
    case OPC_TEST:
    case OPC_TESTN:
    case OPC_TESTB:
    case OPC_TESTBN:
        return true;
    default:
        return false;
    }
}

// see if a loop can be cached
// "root" is a label
// returns NULL if no fcache, otherwise
// a new label which can point to the end
// of the loop
static IR *
LoopCanBeFcached(IRList *irl, IR *root, int size_left)
{
    IR *endjmp;
    IR *endlabel;
    IR *newlabel;
    IR *ir = root;

    bool non_wait = false;
    int loopsize = 0;

    if (!IsHubDest(ir->dst)) {
        // this loop is not in HUB memory
        return 0;
    }
    endjmp = UniqJumpForLabel(ir);
    if (!endjmp || !IsJump(endjmp)) {
        // we don't know who jumps here
        return 0;
    }
    endlabel = endjmp;
    // addr != 0 check is to avoid picking up return labels
    while (endlabel->next && endlabel->next->opc == OPC_LABEL && endlabel->next->addr != 0) {
        endlabel = endlabel->next;
    }
    if (IsForwardJump(endjmp)) {
        return 0;
    }

    size_left -= 5; // leave room for return and for errors in calculation
    ir = ir->next;
    while (ir != endjmp) {
        if (ir->fcache || InstrIsVolatile(ir)) {
            return 0;
        }
        if (ir->opc == OPC_CALL) {
            // no calls to hub memory
            if (MaybeHubDest(ir->dst)) {
                return 0;
            }
            // mul/div functions are definitely OK
            if (isMulDivFunc(ir->dst)) {
                goto call_ok;
            }
            // otherwise we assume the function may call other
            // things which will end up using FCACHE, so bad
            // FIXME: if we did this at a higher level we might
            // be able to detect leaf functions
            return 0;
call_ok:
            ;
        }
        if (ir->opc == OPC_FCACHE) {
            return 0;
        }
        if (IsJump(ir)) {
            if (!JumpIsAfterOrEqual(root, ir))
                return 0;
            if (JumpDest(ir) != endlabel->dst && JumpIsAfterOrEqual(endlabel, ir))
                return 0;
        }
        if (!IsDummy(ir) && ir->opc != OPC_LABEL) {
            // check for needing AUGD or AUGS
            if (NeedsImmAug(ir->dst))
                --size_left;
            if (NeedsImmAug(ir->src))
                --size_left;
            if (--size_left <= 0) {
                return 0;
            }
            if (++loopsize > 3 || !IsWaitLoopInstr(ir)) {
                non_wait = true;
            }
        }
        ir = ir->next;
    }

    //
    // OK, if we got here then the stuff from "root" to "endjmp"
    // is eligible for fcache
    // if there's another loop just after this one, it may be worth
    // trying to fit it in as well
    //
    if (ir == endjmp && size_left > gl_fcache_size/2) {
        // peek ahead a few instructions
        // if there's a loop there we can stick into fcache as well,
        // combine them both into one big block
        int n = 4;
        while (n > 0 && ir && size_left > 0) {
            if (IsLabel(ir)) {
                IR *newend = LoopCanBeFcached(irl, ir, size_left);
                if (newend) {
                    return newend;
                } else {
                    break;
                }
            } else if (IsBranch(ir) && MaybeHubDest(JumpDest(ir)) ) {
                break;
            } else if (ir->opc == OPC_FCACHE) {
                break;
            } else if (!IsDummy(ir)) {
                --n;
                if (NeedsImmAug(ir->dst)) --size_left;
                if (NeedsImmAug(ir->src)) --size_left;
                --size_left;
            }
            ir = ir->next;
        }
    }

    // Don't fcache if we got a wait loop
    if (!non_wait) return 0;

    Operand *dst = NewHubLabel();
    newlabel = NewIR(OPC_LABEL);
    newlabel->dst = dst;
    InsertAfterIR(irl, endjmp, newlabel);

    return newlabel;
}

void
OptimizeFcache(IRList *irl)
{
    IR *ir;
    IR *endlabel;

    ir = irl->head;
    while (ir) {
        if (IsLabel(ir)) {
            endlabel = LoopCanBeFcached(irl, ir, gl_fcache_size);
            if (endlabel) {
                Operand *src = ir->dst;
                Operand *dst = endlabel->dst;
                IR *startlabel = ir;
                IR *fcache = NewIR(OPC_FCACHE);
                IR *loopstart = ir->prev;
                fcache->src = src;
                fcache->dst = dst;
                while (loopstart && IsDummy(loopstart)) {
                    loopstart = loopstart->prev;
                }
                if (loopstart && loopstart->opc == OPC_REPEAT) {
                    loopstart = loopstart->prev;
                    startlabel = NewIR(OPC_LABEL);
                    fcache->src = startlabel->dst = NewHubLabel();
                    InsertAfterIR(irl, loopstart, startlabel);
                    ir = startlabel;
                }
                InsertAfterIR(irl, loopstart, fcache);
                while (ir != endlabel) {
                    ir->fcache = src;
                    ir = ir->next;
                }
            }
        }
        ir = ir->next;
    }
}

static void
HashOperand(SHA256_CTX *ctx, Operand *op, Function *f)
{
    if (!op) return;
    sha256_update(ctx, (unsigned char *)&op->kind, sizeof(op->kind));
    sha256_update(ctx, (unsigned char *)&op->val, sizeof(op->val));
    if (op->origsym) {
        Symbol *sym = (Symbol *)op->origsym;
        int offset = sym->offset;
        int kind = (int) sym->kind;
        sha256_update(ctx, (unsigned char *)&kind, sizeof(kind));
        sha256_update(ctx, (unsigned char *)&offset, sizeof(offset));
    } else {
        sha256_update(ctx, (unsigned char *)op->name, strlen(op->name));
    }
}

static void
HashIR(SHA256_CTX *ctx, IR *ir, Function *f)
{
    Operand *dst, *src, *src2;
    int addr;
    
    if (IsDummy(ir)) {
        return;
    }
    sha256_update(ctx, (unsigned char *)&ir->opc, sizeof(ir->opc));
    sha256_update(ctx, (unsigned char *)&ir->cond, sizeof(ir->cond));
    sha256_update(ctx, (unsigned char *)&ir->flags, sizeof(ir->flags));
    sha256_update(ctx, (unsigned char *)&ir->srceffect, sizeof(ir->srceffect));
    sha256_update(ctx, (unsigned char *)&ir->dsteffect, sizeof(ir->dsteffect));

    dst = ir->dst;
    src = ir->src;
    src2 = ir->src2;

    // handle some special cases here
    switch (ir->opc) {
    case OPC_LABEL:
        addr = ir->addr;
        sha256_update(ctx, (unsigned char *)&addr, sizeof(addr));
        dst = 0;
        break;
    case OPC_JUMP:
        if (ir->aux) {
            IR *dest = (IR *)ir->aux;
            addr = dest->addr;
            sha256_update(ctx, (unsigned char *)&addr, sizeof(addr));
            dst = 0;
        }
        break;
    case OPC_DJNZ:
        if (ir->aux) {
            IR *dest = (IR *)ir->aux;
            addr = dest->addr;
            sha256_update(ctx, (unsigned char *)&addr, sizeof(addr));
            src = 0;
        }
        break;
    default:
        /* do nothing */
        break;
    }
    
    HashOperand(ctx, dst, f);
    HashOperand(ctx, src, f);
    HashOperand(ctx, src2, f);
}

static FunctionList *funchash[256];

void
HashFuncIRL(Function *f)
{
    IR *ir;
    IRList *irl = FuncIRL(f);
    IRFuncData *fdata = FuncData(f);
    SHA256_CTX ctx;
    FunctionList *fl;
    unsigned char *hash = &fdata->firl_hash[0];
    
    static int32_t unique_func_count = 0;
    
    if (!(f->optimize_flags & OPT_MERGE_DUPLICATES)) {
        ++unique_func_count;
        memset(hash, 0xff, 16);
        memcpy(hash, (unsigned char *)&unique_func_count, sizeof(unique_func_count));
        return;
    }
    
    sha256_init(&ctx);
    unsigned char flags[2];
    flags[0] = f->code_placement;
    flags[1] = f->is_leaf;
    
    sha256_update(&ctx, flags, sizeof(flags));
    for (ir = irl->head; ir; ir = ir->next) {
        HashIR(&ctx, ir, f);
    }
    sha256_final(&ctx, hash);
    fl = funchash[hash[0]];
    if (!fl) {
        funchash[hash[0]] = NewFunctionList(f);
        return;
    }
    while (fl) {
        Function *f2 = fl->func;
        IRFuncData *f2data = f2 ? FuncData(f2) : 0;
        if (f2data) {
            if (memcmp(f2data->firl_hash, hash, SHA256_BLOCK_SIZE) == 0) {
                /* same implementation for both functions */
                /* make sure f2 does not get removed if f is needed */
                f2->used_as_ptr |= f->used_as_ptr;
                f2->cog_task |= f->cog_task;
                f2->callSites += f->callSites;
                
                FunctionList *entry = NewFunctionList(f);
                entry->next = f2data->funcdups;
                f2data->funcdups = entry;
                fdata->firl_done = true;
                return;
            }
        }
        if (!fl->next) {
            fl->next = NewFunctionList(f);
            break;
        }
        fl = fl->next;
    }
}
