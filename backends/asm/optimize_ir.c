//
// IR optimizer
//
// Copyright 2016-2020 Total Spectrum Software Inc.
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
static int OptimizePeephole2(IRList *irl);

// fcache size in longs; -1 means take a guess
int gl_fcache_size = -1;

//
// helper functions
//

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
// replace an opcode
//
static void
ReplaceOpcode(IR *ir, IROpcode op)
{
  ir->opc = op;
  ir->instr = FindInstrForOpc(op);
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
  case OPC_ABS:
  case OPC_RDBYTE:
  case OPC_RDWORD:
  case OPC_RDLONG:
  case OPC_GETQX:
  case OPC_GETQY:
  case OPC_GETRND:
  case OPC_GETCT:
  case OPC_GETBYTE:
  case OPC_GETWORD:
  case OPC_WRC:
  case OPC_WRNC:
  case OPC_WRZ:
  case OPC_WRNZ:
    return false;
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
      return false;
  case OPC_CMP:
  case OPC_CMPS:
  case OPC_TEST:
  case OPC_TESTN:
  case OPC_GENERIC_NR:
  case OPC_PUSH:
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

static bool IsBranch(IR *ir)
{
    return IsJump(ir) || ir->opc == OPC_CALL;
}

static bool IsWrite(IR *ir) {
    switch (ir->opc) {
    case OPC_WRLONG:
    case OPC_WRWORD:
    case OPC_WRBYTE:
    case OPC_GENERIC:
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

// return TRUE if an instruction modifies a register
bool
InstrModifies(IR *ir, Operand *reg)
{
    Operand *dst = ir->dst;
    if (dst == reg) {
        return InstrSetsDst(ir);
    }
    if (ir->opc == OPC_MOVD && reg) {
        if (dst->kind == IMM_COG_LABEL && !strcmp(reg->name, dst->name)) {
            return true;
        }
    }
    if (ir->src == reg && OPEFFECT_NONE != (ir->srceffect & 0xff)) {
        return true;
    }
    return false;
}

// return TRUE if an instruction uses a register
bool
InstrUses(IR *ir, Operand *reg)
{
    Operand *dst = ir->dst;
    Operand *src = ir->src;

    if (src == reg) {
        return true;
    }
    if (dst == reg && InstrReadsDst(ir)) {
        return true;
    }
    if (src && src->kind == IMM_COG_LABEL) {
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
    return op->kind == REG_ARG;
}

// returns TRUE if an operand represents a local register
bool
IsLocal(Operand *op)
{
    return op->kind == REG_LOCAL || op->kind == REG_TEMP;
}

// returns TRUE if an operand represents a local register or argument
bool
IsLocalOrArg(Operand *op)
{
    return op->kind == REG_LOCAL || op->kind == REG_ARG || op->kind == REG_TEMP;
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

static bool
InstrSetsFlags(IR *ir, int flags)
{
    if ( 0 != (ir->flags & flags) )
        return true;
    return false;
}

static bool
InstrSetsAnyFlags(IR *ir)
{
    return InstrSetsFlags(ir, FLAG_CZSET);
}

static bool
InstrIsVolatile(IR *ir)
{
    return 0 != (ir->flags & FLAG_KEEP_INSTR);
}

static bool
InstrUsesFlags(IR *ir, unsigned flags)
{
    if (ir->cond != COND_TRUE && ir->cond != COND_FALSE) {
        if ( (flags & FLAG_WC) ) {
            /* the E and NE flags do not require C */
            if (ir->cond != COND_EQ && ir->cond != COND_NE)
                return true;
        }
        if (flags & FLAG_WZ) {
            if (ir->cond != COND_C && ir->cond != COND_NC)
                return true;
        }
    }
    switch (ir->opc) {
    case OPC_GENERIC:
    case OPC_GENERIC_NR:
    case OPC_GENERIC_BRANCH:
    case OPC_GENERIC_BRCOND:
        /* it might use flags, we don't know (e.g. addx) */
        return true;
    case OPC_WRC:
    case OPC_WRNC:
    case OPC_DRVC:
    case OPC_DRVNC:
    case OPC_MUXC:
    case OPC_MUXNC:
        /* definitely uses the C flag */
        return (flags & FLAG_WC) != 0;
    case OPC_DRVZ:
    case OPC_DRVNZ:
    case OPC_MUXZ:
    case OPC_MUXNZ:
    case OPC_WRZ:
    case OPC_WRNZ:
        /* definitely uses the Z flag */
        return (flags & FLAG_WZ) != 0;
    default:
        return false;
    }
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

bool
IsMathInstr(IR *ir)
{
    switch (ir->opc) {
    case OPC_ADD:
    case OPC_SUB:
    case OPC_AND:
    case OPC_ANDN:
    case OPC_OR:
    case OPC_XOR:
    case OPC_SHL:
    case OPC_SHR:
    case OPC_SAR:
        return true;
    default:
        return false;
    }
}

extern Operand *mulfunc, *divfunc, *unsdivfunc, *muldiva, *muldivb;


static bool FuncUsesArg(Operand *func, Operand *arg)
{
    if (func == mulfunc || func == divfunc || func == unsdivfunc) {
        return (arg == muldiva) || (arg == muldivb);
    }
    return true;
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
  if (level >= MAX_FOLLOWED_JUMPS) {
      // give up!
      return false;
  }
  for (ir = instr->next; ir; ir = ir->next) {
    for (i = 0; i < level; i++) {
        if (ir == stack[i]) {
            // we've come around a loop
            // registers may be considered dead, labels not
            return IsRegister(op->kind);
        }
    }
    if (IsDummy(ir)) continue;
    if (ir->opc == OPC_LABEL) {
        // potential problem: if there is a branch before instr
        // that goes to LABEL then we might miss a set
        // so check
        IR *comefrom = (IR *)ir->aux;
        if (ir->flags & FLAG_LABEL_NOJUMP) {
            // this label isn't a jump target, so don't worry about it
            continue;
        }
        if (!comefrom) {
            // we don't know what branches come here,
            // so for caution give up
            return false;
        }
        if (level == 0 && comefrom->addr < instr->addr) {
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
        continue;
    }
    if (InstrUses(ir, op)) {
        // value is used, so definitely not dead
        // well, unless the value is used only to update itself:
        // check for that here
        if (ir->dst != op) {
            return false;  // op used to modify something else
        }
        if (InstrSetsAnyFlags(ir)) {
            return false;  // flag setting matters, we are not dead
        }
        switch(ir->opc) {
            // be very cautious about whether op is dead if
            // any "unusal" opcodes (like waitpeq) are used
            // so just accept
        case OPC_ADD:
        case OPC_SUB:
        case OPC_AND:
        case OPC_OR:
        case OPC_XOR:
            // not definitely alive or dead yet
            break;
        default:
            // assume live
            return false;
        }
    } else if (InstrModifies(ir, op)) {
        // if the instruction modifies but does not use the op,
        // then we're setting it from another register and it's dead
        if (ir->cond == COND_TRUE) {
            return true;
        }
    }
    if (ir->opc == OPC_RET && ir->cond == COND_TRUE) {
      return IsLocalOrArg(op);
    } else if (ir->opc == OPC_CALL) {
        if (!IsLocal(op)) {
            // we know of some special cases where argN is not used
            if (IsArg(op) && !FuncUsesArg(ir->dst, op)) {
                /* OK to continue */
            } else {
                return false;
            }
        }
    } else if (IsJump(ir)) {
        // if the jump is to an unknown place give up
        if (!ir->aux) {
            // jump to return is like running off the end
            if (ir->dst == FuncData(curfunc)->asmreturnlabel && ir->cond == COND_TRUE) {
                goto done;
            }
            return false;
        }
        stack[level] = ir; // remember where we were
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
  return IsLocalOrArg(op);
}

static bool
IsDeadAfter(IR *instr, Operand *op)
{
    IR *stack[MAX_FOLLOWED_JUMPS];
    stack[0] = instr;
    return doIsDeadAfter(instr, op, 1, stack);
}

static bool
SafeToReplaceBack(IR *instr, Operand *orig, Operand *replace)
{
  IR *ir;
  int usecount = 0;

  if (SrcOnlyHwReg(replace) || !IsRegister(replace->kind))
      return false;
  for (ir = instr; ir; ir = ir->prev) {
      if (IsDummy(ir)) {
          continue;
      }
      if (ir->opc == OPC_LABEL) {
          if (ir->flags & FLAG_LABEL_NOJUMP) {
              continue;
          }
          return false;
      }
      if (ir->opc == OPC_LIVE) {
          return false;
      }
      if (IsBranch(ir)) {
          return false;
      }
      if (InstrModifies(ir, orig) && !InstrReadsDst(ir)) {
          if (InstrIsVolatile(ir)) return false;
          if (IsHwReg(replace) && ir->src && IsHwReg(ir->src) && ir->src != replace)
          {
              return false;
          }
          if (usecount > 0 && IsHwReg(replace) && ir->src != replace) {
              return false;
          }
          return ir->cond == COND_TRUE;
      }
      if (InstrUses(ir, replace) || ir->dst == replace) {
          return false;
      }
      if ( (InstrUses(ir, orig) || InstrModifies(ir, orig)) && IsHwReg(replace)) {
          if (usecount > 0) {
              return false;
          }
          usecount++;
      }
  }
  // we've reached the start
  // is orig dead here? if so we can replace it
  // args are live, and so are globals
  return IsLocal(orig);
}

bool
IsHwReg(Operand *reg)
{
    return reg->kind == REG_HW;
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
SafeToReplaceForward(IR *first_ir, Operand *orig, Operand *replace)
{
  IR *ir;
  IR *last_ir = NULL;
  bool assignments_are_safe = true;
  bool orig_modified = false;
  
  if (SrcOnlyHwReg(replace) || !IsRegister(replace->kind)) {
      return NULL;
  }
  if (!first_ir) {
      return NULL;
  }

#if 1
  // special case: if orig is dead after this,
  // and if first_ir does not modify it, then it is safe to
  // replace
  if (!IsBranch(first_ir) && IsDeadAfter(first_ir, orig) && !InstrModifies(first_ir, orig)) {
      return first_ir;
  }
#endif  
  for (ir = first_ir; ir; ir = ir->next) {
    if (IsDummy(ir)) {
	continue;
    }
    if (ir->opc == OPC_LIVE && !strcmp(orig->name, ir->dst->name)) {
        return NULL;
    }
    if (ir->opc == OPC_LIVE && !strcmp(replace->name, ir->dst->name)) {
        return NULL;
    }
    if (ir->opc == OPC_RET) {
        return IsLocalOrArg(orig) ? ir : NULL;
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
                return (assignments_are_safe && IsDeadAfter(ir, orig)) ? ir : NULL;
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
    if (ir->opc == OPC_LABEL) {
        IR *comefrom;
        if (ir->flags & FLAG_LABEL_NOJUMP) {
            // this label is not a jump target, so ignore it
            continue;
        }
        // do we know who jumps to this label?
        comefrom = (IR *)ir->aux;
        if (comefrom) {
            // if the jumper is before our first instruction, then
            // we don't know what may have happened before, so
            // replacement is dangerous
            // however, in the special case that the register is never
            // actually used again then it's safe
            if (comefrom->addr < first_ir->addr) {
                if (assignments_are_safe && IsDeadAfter(ir, orig)) {
                    return ir;
                }
                return NULL;
            }
            assignments_are_safe = false;
        } else {
            // unknown jumper, assume the worst
            return NULL;
        }
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
      if (ir->cond != COND_TRUE) {
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
        if (ir->cond != COND_TRUE) {
            assignments_are_safe = false;
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
    last_ir = ir;
  }
  return IsLocalOrArg(orig) ? last_ir : NULL;
}


static void
ReplaceBack(IR *instr, Operand *orig, Operand *replace)
{
  IR *ir;
  for (ir = instr; ir; ir = ir->prev) {
    if (ir->opc == OPC_LABEL) {
        if (ir->flags & FLAG_LABEL_NOJUMP) {
            continue;
        }
        break;
    }
    if (ir->dst == orig) {
      ir->dst = replace;
      if (InstrSetsDst(ir) && !InstrReadsDst(ir) && ir->cond == COND_TRUE) {
	break;
      }
    }
    if (ir->src == orig) {
      ir->src = replace;
    }
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
// Apply a new condition code based on val being compared to 0
//
int
ApplyConditionAfter(IR *instr, int val)
{
  IR *ir;
  IRCond newcond;
  int change = 0;
  int setz = instr->flags & FLAG_WZ;
  int setc = instr->flags & FLAG_WC;
  
  for (ir = instr->next; ir; ir = ir->next) {
    if (IsDummy(ir)) continue;
    newcond = ir->cond;
    switch (newcond) {
    case COND_TRUE:
    case COND_FALSE:
        /* no change */
        break;
    case COND_EQ:
        if (setz) {
            newcond = (val == 0) ? COND_TRUE : COND_FALSE;
        }
        break;
    case COND_NE:
        if (setz) {
            newcond = (val != 0) ? COND_TRUE : COND_FALSE;
        }
        break;
    case COND_LT:
        if (setc) {
            newcond = (val < 0) ? COND_TRUE : COND_FALSE;
        }
        break;
    case COND_GT:
        if (setc && setz) {
            newcond = (val > 0) ? COND_TRUE : COND_FALSE;
        }
        break;
    case COND_LE:
        if (setc && setz) {
            newcond = (val <= 0) ? COND_TRUE : COND_FALSE;
        }
        break;
    case COND_GE:
        if (setc) {
            newcond = (val >= 0) ? COND_TRUE : COND_FALSE;
        }
        break;
    default:
        ERROR(NULL, "Internal error, unhandled condition");
        return 0;
    }
    if (ir->cond != newcond) {
        change = 1;
        ir->cond = newcond;
    }
    if (InstrSetsAnyFlags(ir)) {
        break;
    }
  }
  return change;
}

static bool
SameImmediate(Operand *a, Operand *b)
{
    if (a->kind != IMM_INT || b->kind != IMM_INT)
        return 0;
    return a->val == b->val;
}

static bool
SameOperand(Operand *a, Operand *b)
{
    if (a == b) {
        return true;
    }
    if (!a || !b) {
        return false;
    }
    return SameImmediate(a, b);
}

// try to transform an operation with a destination that is
// known to be the constant "imm"
// if the src is also constant, convert it to a move immediate
// returns 1 if there is a change
static int
TransformConstDst(IR *ir, Operand *imm)
{
  intptr_t val1, val2;
  int setsResult = 1;

  if (gl_p2 && !InstrSetsDst(ir)) {
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
      if ((ir->opc == OPC_ADD || ir->opc == OPC_SUB)
          && !InstrSetsAnyFlags(ir))
      {
          if (ir->opc == OPC_ADD) {
              ReplaceOpcode(ir, OPC_MOV);
          } else if (ir->opc == OPC_SUB) {
              ReplaceOpcode(ir, OPC_NEG);
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
    // than cmps
    if (ir->opc != OPC_CMPS && ir->opc != OPC_CMP) {
      return 0;
    }
  }

  val1 = imm->val;
  val2 = ir->src->val;

  switch (ir->opc)
    {
    case OPC_ADD:
      val1 += val2;
      break;
    case OPC_SUB:
      val1 -= val2;
      break;
    case OPC_AND:
      val1 &= val2;
      break;
    case OPC_OR:
      val1 |= val2;
      break;
    case OPC_XOR:
      val1 ^= val2;
      break;
    case OPC_ANDN:
      val1 &= ~val2;
      break;
    case OPC_SHL:
      val1 = val1 << val2;
      break;
    case OPC_SAR:
      val1 = val1 >> val2;
      break;
    case OPC_SHR:
      val1 = ((unsigned)val1) >> val2;
      break;
    case OPC_CMPS:
      val1 -= val2;
      setsResult = 0;
      break;
    case OPC_CMP:
      if ((unsigned)val1 < (unsigned)val2) {
          val1 = -1;
      } else if (val1 == val2) {
          val1 = 0;
      } else {
          val1 = 1;
      }
      setsResult = 0;
      break;
    default:
      return 0;
    }
  if (InstrSetsAnyFlags(ir)) {
      ApplyConditionAfter(ir, val1);
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

  if (orig_ir->opc == OPC_NEG) {
      immval = NewImmediate(-immval->val);
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
        change |= TransformConstDst(ir, immval);
    } else if (ir->src == orig) {
      ir->src = immval;
      change = 1;
    }
    if (InstrModifies(ir, orig)) {
      // our register has changed, so we must stop
      return change;
    }
  }
  return change;
}

static bool
DeleteMulDivSequence(IRList *irl, IR *ir, Operand *lastop, Operand *opa, Operand *opb)
{
    IR *ir2, *ir3;
    
    // ir is pointing at the mov muldiva, opa instruction
    ir2 = ir->next;
    if (!ir2) return false;
    ir3 = ir2->next;
    if (!ir3) return false;
    if (ir2->opc != OPC_MOV) return false;
    if (ir2->dst != muldivb || !SameOperand(ir2->src, opb)) return false;
    if (ir3->opc != OPC_CALL || ir3->dst != lastop) return false;
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
    int change = 0;
    
    ir = irl->head;
    while (ir != 0) {
        if (IsLabel(ir)) {
            opa = opb = lastop = NULL;
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
                if (opa == ir->src && DeleteMulDivSequence(irl, ir, lastop, opa, opb)) {
                    change = 1;
                } else {
                    opa = ir->src;
                    opb = NULL;
                    lastop = NULL;
                }
            } else {
                opa = opb = lastop = NULL;
            }
        } else if (InstrModifies(ir, muldivb)) {
            if (ir->opc == OPC_MOV) {
                opb = ir->src;
            } else if (InstrSetsDst(ir)) {
                opb = NULL;
            }
        } else if (opa && InstrModifies(ir, opa)) {
            opa = NULL;
        } else if (opb && InstrModifies(ir, opb)) {
            opb = NULL;
        } else if (ir->opc == OPC_CALL) {
            if (ir->dst == mulfunc || ir->dst == divfunc || ir->dst == unsdivfunc) {
                lastop = ir->dst;
            } else {
                lastop = NULL;
            }
        }
        ir = ir->next;
    }
    return change;
}

int
OptimizeMoves(IRList *irl)
{
    IR *ir;
    IR *ir_next;
    IR *stop_ir;
    int change;
    int everchange = 0;

    do {
        change = 0;
        ir = irl->head;
        while (ir != 0) {
            ir_next = ir->next;
            if (InstrIsVolatile(ir)) {
                /* do nothing */
            } else if (ir->opc == OPC_MOV && ir->src == ir->dst) {
                if (!InstrSetsAnyFlags(ir)) {
                    DeleteIR(irl, ir);
                    change = 1;
                }
            } else if ((ir->opc == OPC_MOV || ir->opc == OPC_NEG) && ir->cond == COND_TRUE && IsImmediate(ir->src)) {
                int sawchange;
                if (ir->flags == FLAG_WZ) {
                    // because this is a mov immediate, we know how
                    // WZ will be set
                    change |= ApplyConditionAfter(ir, ir->src->val);
                }
                change |= (sawchange = PropagateConstForward(irl, ir, ir->dst, ir->src));
                if (sawchange && !InstrSetsAnyFlags(ir) && IsDeadAfter(ir, ir->dst)) {
                    // we no longer need the original mov
                    DeleteIR(irl, ir);
                }
            } else if (ir->opc == OPC_MOV && ir->cond == COND_TRUE) {
                if (ir->src == ir->dst && !InstrSetsAnyFlags(ir)) {
                    DeleteIR(irl, ir);
                    change = 1;
                } else if (!InstrSetsAnyFlags(ir) && IsDeadAfter(ir, ir->src) && SafeToReplaceBack(ir->prev, ir->src, ir->dst)) {
                    ReplaceBack(ir->prev, ir->src, ir->dst);
                    DeleteIR(irl, ir);
                    change = 1;
                }
                else if ( !InstrSetsAnyFlags(ir) && 0 != (stop_ir = SafeToReplaceForward(ir->next, ir->dst, ir->src)) ) {
                    ReplaceForward(ir->next, ir->dst, ir->src, stop_ir);
                    DeleteIR(irl, ir);
                    change = 1;
                }
            }
            ir = ir_next;
        }
        everchange |= change;
    } while (change && 0);
    return everchange;
}

static bool
HasSideEffectsOtherThanReg(IR *ir)
{
    if ( (ir->dst && ir->dst->kind == REG_HW)
         || (ir->src && ir->src->kind == REG_HW) )
    {
        return true;
    }
    if (InstrSetsAnyFlags(ir)) {
        // if the flags might possibly be used, we have to assume there
        // are side effects
        IR *irnext;
        int flagsSet = ir->flags & (FLAG_WZ|FLAG_WC);
        for (irnext = ir->next; irnext; irnext = irnext->next) {
            if (InstrUsesFlags(irnext, flagsSet)) {
                return true;
            }
            if (InstrIsVolatile(irnext)) {
                return true;
            }
            if (IsBranch(irnext) && irnext->cond == COND_TRUE) {
                break;
            }
        }
    }
    if (IsBranch(ir)) {
        if (ir->opc == OPC_CALL &&
            (ir->dst == mulfunc || ir->dst == divfunc || ir->dst == unsdivfunc))
        {
            return false;
        }
        return true;
    }
    if (InstrIsVolatile(ir)) {
      return true;
    }
    switch (ir->opc) {
    case OPC_GENERIC:
    case OPC_GENERIC_NR:
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
    case OPC_DRVC:
    case OPC_DRVNC:
    case OPC_DRVL:
    case OPC_DRVH:
    case OPC_DRVNZ:
    case OPC_DRVZ:
    case OPC_PUSH:
    case OPC_POP:
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
    case OPC_OR:
        return (val == 0);
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
// eliminate code that is unnecessary
//
int EliminateDeadCode(IRList *irl)
{
    int change;
    IR *ir, *ir_next;

    change = 0;

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
    ir = irl->head;
    while (ir) {
      ir_next = ir->next;

      if (ir->opc == OPC_SETQ || ir->opc == OPC_SETQ2) {
          ir->flags |= FLAG_KEEP_INSTR;
          ir->next->flags |= FLAG_KEEP_INSTR;
      }
      if (InstrIsVolatile(ir)) {
          /* do nothing */
      } else if (ir->opc == OPC_JUMP) {
          if (ir->cond == COND_TRUE && !IsRegister(ir->dst->kind)) {
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
              /* is the branch to the next instruction? */
              if (ir_next && ir_next->opc == OPC_LABEL && ir_next->dst == ir->dst && !InstrIsVolatile(ir)) {
                  DeleteIR(irl, ir);
                  change = 1;
              }
          } else if (ir->cond == COND_FALSE && !InstrIsVolatile(ir)) {
              DeleteIR(irl, ir);
          } else {
              /* if the branch skips over things that already have the right
                 condition, delete it
              */
              if (ir->dst && !IsRegister(ir->dst->kind) && ir_next && AllInstructionsConditional(InvertCond(ir->cond), ir_next, ir->dst) && !InstrIsVolatile(ir))
              {
                  DeleteIR(irl, ir);
                  change = 1;
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
      ir = ir_next;
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
#define MAX_JUMP_OVER 3
static int IsSafeShortForwardJump(IR *irbase)
{
  int n = 0;
  Operand *target;
  IR *ir;

  if (irbase->opc != OPC_JUMP)
    return 0;
  if (InstrIsVolatile(irbase)) {
      return 0;
  }
  target = irbase->dst;
  if (IsRegister(target->kind)) {
      return 0;
  }
  ir = irbase->next;
  while (ir) {
    if (!IsDummy(ir)) {
      if (ir->cond != COND_TRUE) return 0;
      if (ir->opc == OPC_LABEL) {
	if (ir->dst == target) {
	  return n;
	}
	return 0;
      }
      if (ir->opc == OPC_CALL) {
          // calls do not preserve condition codes
          return 0;
      }
      if (ir->opc == OPC_DJNZ && !gl_p2) {
          // DJNZ does not work conditionally in LMM
          return 0;
      }
      n++;
      if (n > MAX_JUMP_OVER) return 0;
    }
    ir = ir->next;
  }
  // we reached the end... were we trying to jump to the end?
  if (curfunc && target == FuncData(curfunc)->asmreturnlabel)
      return n;
  return 0;
}

static void ConditionalizeInstructions(IR *ir, IRCond cond, int n)
{
  while (ir && n > 0) {
    if (!IsDummy(ir)) {
      if (ir->cond != COND_TRUE || ir->opc == OPC_LABEL) {
	ERROR(NULL, "Internal error bad conditionalize");
	return;
      }
      ir->cond = cond;
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

    if (irorig->cond != COND_TRUE) {
        return NULL;
    }
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
        if (IsBranch(ir)) {
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
    while (ir && ir != saveir) {
        if (IsDummy(ir)) {
            ir = ir->next;
            continue;
        }
        if (ir->dst == saveir->src && InstrSetsDst(ir)) {
            return NULL;
        }
        ir = ir->next;
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
        if ( (ir->opc == OPC_CMP||ir->opc == OPC_CMPS) && ir->cond == COND_TRUE
             && (FLAG_WZ == (ir->flags & (FLAG_WZ|FLAG_WC)))
             && IsImmediateVal(ir->src, 0)
             && !InstrIsVolatile(ir)
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
                && (ir_prev->flags == COND_TRUE)
                && CanTestZero(ir_prev->opc)
                && FlagsNotUsedBetween(ir_prev, ir, FLAG_WC|FLAG_WZ))
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
        if (!gl_p2 && (src->name == NULL || src->name[0] == 0)) {
            /* already a small immediate */
            continue;
        }
        val = src->val;
        if (ir->opc == OPC_MOV && val < 0 && val >= -511) {
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
AddSubVal(IR *ir)
{
    int val = ir->src->val;
    if (ir->opc == OPC_SUB) val = -val;
    return val;
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
    
    for (ir = irl->head; ir; ir = ir->next) {
        if (IsDummy(ir)) continue;
        if (IsJump(ir)) {
            dst = JumpDest(ir);
            if (dst == label) {
                ir->aux = irlabel; // record where the jump goes to
                if (irlabel->flags & FLAG_LABEL_USED) {
                    // the label has more than one use
                    irlabel->aux = NULL;
                } else {
                    irlabel->flags |= FLAG_LABEL_USED;
                    irlabel->aux = ir;
                }
            }
        } else if (ir != irlabel) {
            if (ir->src == label || ir->dst == label) {
                irlabel->flags |= FLAG_LABEL_USED;
                irlabel->aux = NULL;
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
static void
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
        ERROR(NULL, "Internal error, unexpected condition");
    }
    switch (ir->opc) {
    case OPC_MUXZ:
        ReplaceOpcode(ir, OPC_MUXNC);
        break;
    case OPC_MUXNZ:
        ReplaceOpcode(ir, OPC_MUXC);
        break;
    case OPC_DRVZ:
        ReplaceOpcode(ir, OPC_DRVNC);
        break;
    case OPC_DRVNZ:
        ReplaceOpcode(ir, OPC_DRVC);
        break;
    case OPC_MUXC:
    case OPC_MUXNC:
    case OPC_GENERIC:
    case OPC_GENERIC_NR:
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
        return true;
    default:
        return false;
    }
}

// check if x is of the form (A<<n)
// where A is a small integer that is all 1's
// in binary; if so, return a bit mask
// otherwise, return -1
static int P2CheckBitMask(unsigned int x)
{
    int rshift = 0;

    if (x == 0) return -1;
    while ((x & 1) == 0) {
        rshift++;
        x = x>>1;
    }
    switch (x) {
    case 1:
        return rshift;
    case 3:
        return rshift + (1<<5);
    case 7:
        return rshift + (2<<5);
    case 15:
        return rshift + (3<<5);
    case 31:
        return rshift + (4<<5);
    case 63:
        return rshift + (5<<5);
    case 127:
        return rshift + (6<<5);
    case 255:
        return rshift + (7<<5);
    default:
        return -1;
    }
}

static bool
FlagsDeadAfter(IRList *irl, IR *ir, unsigned flags)
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
    int opc;
    
    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        while (ir_next && IsDummy(ir_next)) {
            ir_next = ir_next->next;
        }
        if (InstrIsVolatile(ir)) {
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
            if (previr && previr->opc == OPC_AND && !InstrSetsAnyFlags(previr) && previr->cond == COND_TRUE )
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
        
        // check for add a,b ;; mov b,a ;; isdead a
        // becomes add b, a
        
        if (IsCommutativeMath(opc) && ir_next && ir_next->opc == OPC_MOV
            && ir->dst == ir_next->src
            && ir->src == ir_next->dst
            && !InstrSetsAnyFlags(ir)
            && !InstrSetsAnyFlags(ir_next)
            && ir->cond == ir_next->cond
            && IsDeadAfter(ir_next, ir->dst)
            )
        {
            ReplaceOpcode(ir_next, opc);
            DeleteIR(irl, ir);
            changed = 1;
            goto done;
        }

        // on P2, check for immediate operand with just one bit set
        if (gl_p2 && ir->src && ir->src->kind == IMM_INT && !InstrSetsAnyFlags(ir) && ((uint32_t)ir->src->val) > 511) {
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
            } else if (ir->opc == OPC_MOV) {
                uint32_t mask = ir->src->val;
                int bits = 0;
                while ( (mask & 1) ) {
                    bits++;
                    mask = mask >> 1;
                }
                if (bits > 0 && mask == 0) {
                    // could use BMASK instruction
                    ReplaceOpcode(ir, OPC_BMASK);
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
    done:
        ir = ir_next;
    }
    return changed;
}

/* special peephole for buf[i++] := n */
/* look for mov tmp, b; add b, #1; use tmp */
/* in this case push the add b, #1 as far forward as we can */
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
                if (IsBranch(stepir))
                    break;
                if (IsLabel(stepir))
                    break;
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
        if (ir_prev && ir_next
            && ir_prev->opc == OPC_MOV && ir_next->opc == OPC_MOV
            && ir_prev->dst == ir_next->src && ir_prev->src == ir_next->dst
            && ir->dst == ir_prev->dst
            && ir_prev->cond == ir->cond
            && ir_next->cond == ir->cond
            && IsMathInstr(ir)
            && !InstrIsVolatile(ir) && !InstrIsVolatile(ir_prev) && !InstrIsVolatile(ir_next)
            && !InstrSetsAnyFlags(ir)
            && !InstrSetsAnyFlags(ir_prev)
            && !InstrSetsAnyFlags(ir_next)
            && IsDeadAfter(ir_next, ir_next->src)
            )
        {
            ir->dst = ir_next->dst;
            ir = ir_next;
            ir_next = ir->next;
            change = 1;
            DeleteIR(irl, ir);
            if (IsDeadAfter(ir_prev, ir_prev->dst)) {
                DeleteIR(irl, ir_prev);
                ir_prev = NULL;
            }
        }
        ir_prev = ir;
        ir = ir_next;
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
        if (opc == OPC_ADDCT1 && IsImmediateVal(ir->src, 0)) {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_ADD && !InstrSetsAnyFlags(previr)) {
                // add foo, val / addct1 foo, #0 -> addct1 foo, val
                ir->src = previr->src;
                DeleteIR(irl, previr);
                changed = 1;
            }
        }
        if ( (opc == OPC_DJNZ || opc == OPC_JUMP) && ir->cond == COND_TRUE) {
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
                    } else if (IsBranch(pir)) {
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
                    labir->aux = repir;
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

//
// find the next rdlong that uses src
// returns NULL if we spot anything that changes src, dest,
// memory, or a branch
//
static IR* FindNextRead(IR *irorig, Operand *dest, Operand *src)
{
    IR *ir;
    for ( ir = irorig->next; ir; ir = ir->next) {
        if (IsDummy(ir)) continue;
        if (ir->opc == OPC_LABEL) {
            return NULL;
        }
        if (IsBranch(ir)) {
            return NULL;
        }
        if (ir->cond != irorig->cond) {
            return NULL;
        }
        if (ir->opc == OPC_RDLONG && ir->src == src) {
            return ir;
        }
        if (InstrModifies(ir, dest) || InstrModifies(ir, src)) {
            return NULL;
        }
        if (IsWrite(ir)) {
            return NULL;
        }
    }
    return NULL;
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
    if (InstrSetsAnyFlags(a)) return false;
    if (InstrIsVolatile(a) || InstrIsVolatile(b)) return false;
    if (IsBranch(a) || IsBranch(b)) return false;
    if (InstrModifies(a, b->src)) return false;
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
        if (ir->opc == OPC_RDLONG || ir->opc == OPC_WRLONG) {
            // don't mess with it if prev instr was OPC_SETQ
            if (prev_ir && (prev_ir->opc == OPC_SETQ || prev_ir->opc == OPC_SETQ2)) {
                prev_ir->flags |= FLAG_KEEP_INSTR;
                goto get_next;
            }
            dst1 = ir->dst;
            base = ir->src;
            nextread = FindNextRead(ir, dst1, base);
            if (nextread && nextread->cond == ir->cond) {
                // wrlong a, b ... rdlong c, b  -> mov c, a
                // rdlong a, b ... rdlong c, b  -> mov c, a
                nextread->src = dst1;
                ReplaceOpcode(nextread, OPC_MOV);
                change = 1;
                goto get_next;
            }
        } else if (ir->opc == OPC_RDBYTE) {
            dst1 = ir->dst;
            nextread = FindNextUse(ir, dst1);
            if (nextread
                && nextread->opc == OPC_AND
                && nextread->dst == dst1
                && IsImmediate(nextread->src)
                && !InstrSetsAnyFlags(nextread)
                && nextread->src->val == 255)
            {
                // don't need zero extend after rdbyte
                nextread->opc = OPC_DUMMY;
            }
        }
#if 1
        // try to avoid having two read/write ops in a row
        if (IsReadWrite(ir) && IsReadWrite(next_ir) && IsNonReadWriteOpcode(prev_ir)) {
            if (CanSwap(ir, prev_ir)) {
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
            jmpdest = NextInstruction(ir->aux);
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

// optimize an isolated piece of IRList
// (typically a function)
void
OptimizeIRLocal(IRList *irl, Function *f)
{
    int change;

    if (gl_errors > 0) return;
    if (!(gl_optimize_flags & OPT_BASIC_ASM)) return;
    if (!irl->head) return;

    // multiply divide optimization need only be performed once,
    // and should be done before other optimizations confuse things
    OptimizeMulDiv(irl);
again:
    do {
        change = 0;
        AssignTemporaryAddresses(irl);
        change |= CheckLabelUsage(irl);
        change |= OptimizeReadWrite(irl);
        change |= EliminateDeadCode(irl);
        change |= OptimizeCogWrites(irl);
        change |= OptimizeSimpleAssignments(irl);
        change |= OptimizeMoves(irl);
        change |= OptimizeImmediates(irl);
        change |= OptimizeCompares(irl);
        change |= OptimizeShortBranches(irl);
        change |= OptimizeAddSub(irl);
        change |= OptimizePeepholes(irl);
        change |= OptimizePeephole2(irl);
        change |= OptimizeIncDec(irl);
        change |= OptimizeJumps(irl);
        if (gl_p2) {
            change |= OptimizeP2(irl);
        }
    } while (change != 0);
    change = OptimizeTailCalls(irl, f);
    if (change) goto again;
    
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
        OptimizeFcache(irl);
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
    if (gl_exit_status && !strcmp(f->name, "main")) return true;
    return false;
}

//
// check a function to see if it should be inlined
//
#define INLINE_THRESHOLD_P1 2
#define INLINE_THRESHOLD_P2 4

bool
ShouldBeInlined(Function *f)
{
    IR *ir;
    int n = 0;
    int paramfactor;
    int threshold;

    if (gl_p2) {
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
        
        n++;
    }
    // a function called from only 1 place should be inlined
    // if it means that the function definition can be eliminated
    if (RemoveIfInlined(f) && (gl_optimize_flags & OPT_INLINE_SINGLEUSE)) {
        if (f->callSites == 1) {
            return true;
        } else if (f->callSites == 2) {
            return (n <= 2*threshold);
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
    return n <= (threshold + paramfactor);
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
    
    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        if (ir->opc == OPC_CALL) {
            f = (Function *)ir->aux;
            if (f && FuncData(f)->isInline) {
                ReplaceIRWithInline(irl, ir, f);
                FuncData(f)->actual_callsites--;
                change = 1;
            }
        }
        ir = ir_next;
    }
    return change;
}

//
// convert loops to FCACHE when we can
//

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
    
    if (!IsHubDest(ir->dst)) {
        // this loop is not in HUB memory
        return 0;
    }
    endjmp = ir->aux;
    if (!endjmp || !IsJump(endjmp)) {
        // we don't know who jumps here
        return 0;
    }
    endlabel = endjmp;
    while (endlabel->next && endlabel->next->opc == OPC_LABEL) {
        endlabel = endlabel->next;
    }
    if (IsForwardJump(endjmp)) {
        return 0;
    }

    ir = ir->next;
    while (ir != endjmp) {
        if (ir->fcache || InstrIsVolatile(ir)) {
            return 0;
        }
        if (ir->opc == OPC_CALL) {
            // no calls to hub memory!
            if (MaybeHubDest(ir->dst)) {
                return 0;
            }
        }
        if (IsJump(ir)) {
            if (!JumpIsAfterOrEqual(root, ir))
                return 0;
            if (JumpDest(ir) != endlabel->dst && JumpIsAfterOrEqual(endlabel, ir))
                return 0;
        }
        if (!IsDummy(ir) && ir->opc != OPC_LABEL) {
            --size_left;
            if (size_left <= 0) {
                return 0;
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
            } else if (!IsDummy(ir)) {
                --n;
                --size_left;
            }
            ir = ir->next;
        }
    }
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
#define PEEP_FLAGS_DONE 0xffff

#define OPERAND_ANY -1
#define COND_ANY    -1
#define OPC_ANY     -1
#define MAX_OPERANDS_IN_PATTERN 4

#define PEEP_OP_SET     0x01000000
#define PEEP_OP_MATCH   0x02000000
#define PEEP_OP_IMM     0x03000000
#define PEEP_OP_SET_IMM 0x04000000
#define PEEP_OP_MATCH_DEAD 0x05000000  /* like PEEP_OP_MATCH, but operand is dead after this instr */
#define PEEP_OP_CLRBITS 0x06000000
#define PEEP_OPNUM_MASK 0x00ffffff
#define PEEP_OP_MASK    0xff000000

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
        
        if (!PeepOperandMatch(patrn->dst, ir->dst, ir)) {
            return 0;
        }
        if (!PeepOperandMatch(patrn->src1, ir->src, ir)) {
            return 0;
        }
        patrn++;
        ir = ir->next;
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
static PeepholePattern pat_wrc[] = {
    { COND_TRUE, OPC_WRC, PEEP_OP_SET|0, OPERAND_ANY, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_CMP, PEEP_OP_MATCH|0, PEEP_OP_IMM|0, PEEP_FLAGS_P2|PEEP_FLAGS_WCZ_OK },
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
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|0xffff, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shr16getword[] = {
    { COND_TRUE, OPC_MOV, PEEP_OP_SET|0, PEEP_OP_SET|1, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_SAR, PEEP_OP_MATCH|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_MATCH|0, PEEP_OP_IMM|0xffff, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};

static PeepholePattern pat_shl8setbyte[] = {
    { COND_TRUE, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_IMM|8, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_SET|1, PEEP_OP_CLRMASK(8,8), PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_OR, PEEP_OP_MATCH|1, PEEP_OP_MATCH|0, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shl16setbyte[] = {
    { COND_TRUE, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_SET|1, PEEP_OP_CLRMASK(8,16), PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_OR, PEEP_OP_MATCH|1, PEEP_OP_MATCH|0, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shl24setbyte[] = {
    { COND_TRUE, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_IMM|24, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_SET|1, PEEP_OP_CLRMASK(8,24), PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_OR, PEEP_OP_MATCH|1, PEEP_OP_MATCH|0, PEEP_FLAGS_P2 },
    { 0, 0, 0, 0, PEEP_FLAGS_DONE }
};
static PeepholePattern pat_shl16setword[] = {
    { COND_TRUE, OPC_SHL, PEEP_OP_SET|0, PEEP_OP_IMM|16, PEEP_FLAGS_P2 },
    { COND_TRUE, OPC_AND, PEEP_OP_SET|1, PEEP_OP_CLRMASK(16,16), PEEP_FLAGS_P2 },
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

static int ReplaceMaxMin(int arg, IRList *irl, IR *ir)
{
    if (!InstrSetsFlags(ir, FLAG_WZ|FLAG_WC)) {
        return 0;
    }
    if (!FlagsDeadAfter(irl, ir->next, FLAG_WZ|FLAG_WC)) {
        return 0;
    }
    ReplaceOpcode(ir, arg);
    DeleteIR(irl, ir->next);
    return 1;
}
static int ReplaceExtend(int arg, IRList *irl, IR *ir)
{
    Operand *src = ir->src;
    int zbit;
    if (src->kind != IMM_INT) {
        return 0;
    }
    zbit = 31 - src->val;
    ir->src = NewImmediate(zbit);
    ReplaceOpcode(ir, arg);
    DeleteIR(irl, ir->next);
    return 1;
}
//
// looks at the sequence
//   wrc x
//   cmp x, #0 wz
// and if possible deletes it and replaces subsequent uses of C with NZ
//
static int ReplaceWrc(int arg, IRList *irl, IR *ir)
{
    IR *ir0 = ir;
    IR *ir1 = ir->next;
    IR *lastir = NULL;
    
    if (InstrIsVolatile(ir0) || !ir1 || InstrIsVolatile(ir1)) {
        return 0;
    }
    ir = ir1->next;
    for(lastir = ir; lastir; lastir = lastir->next) {
        if (!lastir || InstrIsVolatile(lastir)) {
            return 0;
        }
        if (IsBranch(lastir)) {
            // we assume flags do not have to be preserved across branches */
            break;
        }
        if (InstrUsesFlags(lastir, FLAG_WC)) {
            // C is explicitly used, so preserve it
            return 0;
        }
        if (InstrSetsAnyFlags(lastir)) {
            // flags are to be replaced
            break;
        }
    }
    if (!lastir) {
        return 0;
    }
    // OK, let's go ahead and change Z to NC
    while (ir != lastir) {
        ReplaceZWithNC(ir);
        ir = ir->next;
    }
    if (IsBranch(lastir)) {
        ReplaceZWithNC(lastir);
    }
    DeleteIR(irl, ir0);
    DeleteIR(irl, ir1);
    return 1;
}

static int RemoveNFlagged(int arg, IRList *irl, IR *ir)
{
    IR *irlast, *irnext;
    unsigned wcz_flags = 0;
    irnext = ir->next;
    while (arg > 0 && irnext) {
        irlast = irnext;
        irnext = irlast->next;
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
    ir = ir->next;
    if (ir->src != newsrc) {
        ir->src = newsrc;
        return 1;
    }
    return 0;
}

/* mov x, #0 ; test x, #1 wc => mov x, #0 wc */
static int FixupClrC(int arg, IRList *irl, IR *ir)
{
    IR *testir = ir->next;
    int newflags;
    newflags = (testir->flags & (FLAG_WC|FLAG_WZ));
    ir->flags |= newflags;
    DeleteIR(irl, testir);

    /* an interesting other thing to check: if we have
     * a "drvc x" instruction following, replace it with
     * "drvl x"
     */
    if (ir->next && ir->next->opc == OPC_DRVC) {
        ReplaceOpcode(ir->next, OPC_DRVL);
    }
    return 1;
}
/* mov x, #1 ; test x, #1 wc => neg x, #1 wc */
static int FixupSetC(int arg, IRList *irl, IR *ir)
{
    IR *testir = ir->next;
    int newflags;
    ReplaceOpcode(ir, OPC_NEG);
    newflags = (testir->flags & (FLAG_WC|FLAG_WZ));
    ir->flags |= newflags;
    DeleteIR(irl, testir);
    /* an interesting other thing to check: if we have
     * a "drvc x" instruction following, replace it with
     * "drvh x"
     */
    if (ir->next && ir->next->opc == OPC_DRVC) {
        ReplaceOpcode(ir->next, OPC_DRVH);
    }
    return 1;
}

// mov x, y; shr x, #N; and x, #255 => getbyte x, y, #N/8
// mov x, y; shr x, #N; and x, #65535 => getword x, y, #N/16

static int FixupGetByteWord(int arg, IRList *irl, IR *ir0)
{
    IR *ir1 = ir0->next;
    IR *ir2;
    int shift = 0;
    
    if (ir1->opc == OPC_SHR || ir1->opc == OPC_SAR) {
        ir2 = ir1->next;
        shift = ir1->src->val;
    } else if (ir1->opc == OPC_AND) {
        ir2 = ir1;
        ir1 = NULL;
        shift = 0;
    } else {
        return 0;
    }

    ReplaceOpcode(ir0, arg);
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
// shl x, #N; and y, #MASK; or y, x => setbyte y, x, #N/8

static int FixupSetByteWord(int arg, IRList *irl, IR *ir0)
{
    IR *ir1 = ir0->next;
    IR *ir2 = ir1->next;
    int shift = 0;
    
    if (ir0->opc == OPC_SHL || ir0->opc == OPC_ROL) {
        shift = ir0->src->val;
    } else {
        return 0;
    }

    ReplaceOpcode(ir2, arg);
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

    irnext = ir->next;
    irnext2 = irnext->next;
    
    ReplaceOpcode(irnext2, OPC_BMASK);
    irnext2->src = peep_ops[1];
    DeleteIR(irl, ir);
    DeleteIR(irl, irnext);
    return 1;
}

static int FixupWaitx(int arg, IRList *irl, IR *ir)
{
    IR *irnext;

    irnext = ir->next;    
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

    irnext = ir->next;
    irnext2 = irnext->next;
    ReplaceOpcode(irnext2, arg);
    irnext2->src = NULL;
    irnext2->cond = COND_TRUE;
    DeleteIR(irl, ir);
    return 1;
}

static int ReplaceDrvc(int arg, IRList *irl, IR *ir)
{
    ReplaceOpcode(ir, arg);
    ir->cond = COND_TRUE;
    DeleteIR(irl, ir->next);
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

    ir1 = ir0->next;
    ir2 = ir1->next;

    ir1->dst = ir0->src;
    ReplaceOpcode(ir1, OPC_TEST);
    ir1->flags |= FLAG_WZ;

    ir2->src = NewImmediate(1);
    ir2->cond = COND_NE;

    DeleteIR(irl, ir0);
    return 1;
}


struct Peepholes {
    PeepholePattern *check;
    int arg;
    int (*replace)(int arg, IRList *irl, IR *ir);
} peep2[] = {
    { pat_maxs, OPC_MAXS, ReplaceMaxMin },
    { pat_maxu, OPC_MAXU, ReplaceMaxMin },
    { pat_mins, OPC_MINS, ReplaceMaxMin },
    { pat_minu, OPC_MINU, ReplaceMaxMin },

    { pat_zeroex, OPC_ZEROX, ReplaceExtend },
    { pat_signex, OPC_SIGNX, ReplaceExtend },

    { pat_drvc1, OPC_DRVC, ReplaceDrvc },
    { pat_drvc2, OPC_DRVC, ReplaceDrvc },
    { pat_drvz, OPC_DRVZ, ReplaceDrvc },
    { pat_drvnz1, OPC_DRVNZ, ReplaceDrvc },
    { pat_drvnz2, OPC_DRVNZ, ReplaceDrvc },

    { pat_wrc, 0, ReplaceWrc },

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

    { pat_sar24getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_shr24getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_sar16getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_shr16getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_sar8getbyte, OPC_GETBYTE, FixupGetByteWord },
    { pat_shr8getbyte, OPC_GETBYTE, FixupGetByteWord },

    { pat_sar16getword, OPC_GETWORD, FixupGetByteWord },
    { pat_shr16getword, OPC_GETWORD, FixupGetByteWord },

    { pat_shl8setbyte, OPC_SETBYTE, FixupSetByteWord },
    { pat_shl16setbyte, OPC_SETBYTE, FixupSetByteWord },
    { pat_shl24setbyte, OPC_SETBYTE, FixupSetByteWord },
    
    { pat_shl16setword, OPC_SETWORD, FixupSetByteWord },
    
    { pat_clrc, 0, FixupClrC },
    { pat_setc1, 0, FixupSetC },
    { pat_setc2, 0, FixupSetC },

    { pat_mov_and_add, 0, FixupAndAdd },
};


static int OptimizePeephole2(IRList *irl)
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
        ir = ir->next;
    }
    return change;
}
