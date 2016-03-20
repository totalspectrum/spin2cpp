//
// IR optimizer
//
// Copyright 2016 Total Spectrum Software Inc.
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

//
// helper functions
//

/* IR instructions that have no effect on the generated code */
bool IsDummy(IR *op)
{
  switch(op->opc) {
  case OPC_COMMENT:
  case OPC_DEAD:
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
    return false;
  default:
    break;
  }
  return true;
}

// recognize instructions that modify their
// destination
// (should only be called on real instructions)

static bool
InstrSetsDst(IR *ir)
{
  switch (ir->opc) {
  case OPC_LABEL:
      return false;
  case OPC_CMP:
  case OPC_CMPS:
  case OPC_TEST:
  case OPC_TESTN:
  case OPC_GENERIC_NR:
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
  case OPC_DJNZ:
    return true;
  default:
    return false;
  }
}

static bool IsBranch(IR *ir)
{
    return IsJump(ir) || ir->opc == OPC_CALL;
}

static bool
IsLabel(IR *ir)
{
  return ir->opc == OPC_LABEL;
}

// true if an instruction can change flags
static bool
SetsFlags(IR *ir)
{
  return 0 != (ir->flags & (FLAG_WC|FLAG_WZ));
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
  return op->kind == REG_LOCAL;
}

// returns TRUE if an operand represents a local register or argument
bool
IsLocalOrArg(Operand *op)
{
  return op->kind == REG_LOCAL || op->kind == REG_ARG;
}

static bool
IsImmediate(Operand *op)
{
    return op->kind == IMM_INT;
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
    case REG_ARG:
    case REG_REG:
    case IMM_COG_LABEL:  // for popping ret addresses and such
        return true;
    default:
        return false;
    }
}

static bool
InstrSetsFlags(IR *ir)
{
    if ( 0 != (ir->flags & (FLAG_WZ|FLAG_WC)) )
        return true;
    return false;
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
        /* it might use flags, we don't know (e.g. addx) */
        return true;
    case OPC_MUXC:
    case OPC_MUXNC:
        return (flags & FLAG_WC) != 0;
    case OPC_MUXZ:
    case OPC_MUXNZ:
        return (flags & FLAG_WZ) != 0;
    default:
        return false;
    }
}

static Operand *
JumpDest(IR *jmp)
{
    switch (jmp->opc) {
    case OPC_DJNZ:
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
        return (label->addr >= ir->addr);
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

/*
 * return TRUE if the operand's value does not need to be preserved
 * after instruction instr
 */
static bool
IsDeadAfter(IR *instr, Operand *op)
{
  IR *ir;
  bool replaceMeansDead = true;
  
  if (instr->opc == OPC_DEAD && op == instr->dst) {
    return true;
  }
  for (ir = instr->next; ir; ir = ir->next) {
    if (ir->opc == OPC_DEAD && op == ir->dst) {
      return true;
    }
    if (IsDummy(ir)) continue;
    if (ir->opc == OPC_LABEL) {
        // potential problem: if there is a branch before instr
        // that goes to LABEL then we might miss a set
        // so check
        IR *comefrom = (IR *)ir->aux;
        if (!comefrom || comefrom->addr < instr->addr) {
            return false;
        }
        replaceMeansDead = false;
        continue;
    }
    if (ir->src == op) {
        // value is used, so definitely not dead
        return false;
    }
    if (ir->dst == op) {
      /* the value is unused for certain opcodes */
      if (InstrReadsDst(ir)) {
	return false;
      }
      if (ir->cond == COND_TRUE) {
	return replaceMeansDead;
      } else {
        return false;
      }
    }
    if (ir->opc == OPC_RET && ir->cond == COND_TRUE) {
      return IsLocalOrArg(op);
    } else if (ir->opc == OPC_CALL) {
        if (!IsLocal(op)) {
            return false;
        }
    } else if (IsJump(ir)) {
        if (IsForwardJump(ir)) {
            // FIXME
            // special case: sometimes we add .dead notes right after a branch
            // so look here in case that happened
            IR *irdead;
            irdead = ir->next;
            while (irdead && irdead->opc == OPC_DEAD) {
                if (irdead->dst == op) {
                    return true;
                }
                irdead = irdead->next;
            }
        }
        // otherwise, is the jump backwards to before instr?
        // if it is, then we don't know if the opcode is dead
        if (!JumpIsAfterOrEqual(instr, ir)) {
            return false;
        }
        // forward jumps are a problem... we can have sets in two
        // different branches and that won't kill the variable
        // so avoid returning "dead" if we see a set
        replaceMeansDead = false;
    }
  }
  /* if we reach the end without seeing any use */
  return IsLocalOrArg(op);
}

static bool
SafeToReplaceBack(IR *instr, Operand *orig, Operand *replace)
{
  IR *ir;
  for (ir = instr; ir; ir = ir->prev) {
#if 0
      if (ir->opc == OPC_DEAD && (ir->dst == orig || ir->dst == replace)) {
          return false;
      }
#endif
      if (IsDummy(ir)) {
          continue;
      }
      if (ir->opc == OPC_LABEL) {
          return false;
      }
      if (IsBranch(ir)) {
          return false;
      }
      if (ir->dst == orig && InstrSetsDst(ir) && !InstrReadsDst(ir)) {
          return ir->cond == COND_TRUE;
      }
      if (ir->src == replace || ir->dst == replace) {
          return false;
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
    if (!strcasecmp(orig->name, "CNT")
        || !strcasecmp(orig->name, "INA")
        || !strcasecmp(orig->name, "INB"))
    {
        return true;
    }
    return false;
}

//
// returns the IR where we should stop scanning for replacement,
// or NULL if replacement is unsafe
//
static IR*
SafeToReplaceForward(IR *first_ir, Operand *orig, Operand *replace)
{
  IR *ir;
  IR *last_ir = NULL;
  bool assignments_are_safe = true;

  if (SrcOnlyHwReg(replace)) {
      return NULL;
  }
  for (ir = first_ir; ir; ir = ir->next) {
    if (ir->opc == OPC_DEAD) {
        if (ir->dst == orig) {
            // FIXME: why is this necessary
            //assignments_are_safe = false;
            return ir;
        }
    }
    if (IsDummy(ir)) {
	continue;
    }
    if (ir->opc == OPC_RET) {
        return IsLocalOrArg(orig) ? ir : NULL;
    } else if (ir->opc == OPC_CALL) {
        // it's OK to replace forward over a call as long
        // as orig is a local register (not an ARG!)
        if (!IsLocal(orig)) return NULL;
        if (IsArg(replace)) {
            // if there are any more references to orig then
            // replacement will fail (since arg gets changed
            // by the call)
            return IsDeadAfter(ir, orig) ? ir : NULL;
        } else if (!IsLocal(replace)) {
            return NULL;
        }
    } else if (IsBranch(ir)) {
        // forward branches (or branches to code we've
        // already seen) are safe; others we should assume
        // will cause problems
        // Note though that if we do branch ahead then
        // we cannot assume that assignments are safe!
        assignments_are_safe = false;
        if (!JumpIsAfterOrEqual(first_ir, ir)) {
            return NULL;
        }
    }
    if (ir->opc == OPC_LABEL) {
        IR *comefrom;
        // do we know who jumps to this label?
        comefrom = (IR *)ir->aux;
        if (comefrom) {
            // if the jumper is before our first instruction, that
            // is bad
            if (comefrom->addr < first_ir->addr) {
                return NULL;
            }
            assignments_are_safe = false;
        } else {
            // unknown jumper, assume the worst
            return NULL;
        }
    }
    if (ir->dst == replace) {
      // special case: if we have a "mov replace,x" and orig is dead
      // then we are good to go; at that point we know it is safe to replace
      // orig with replace because:
      //  (a) orig is dead after this, so not used
      //  (b) whatever we did to replace up til now it doesn't matter, a fresh
      //      value is being put into it
      //  if "assignments_are_safe" is false then we don't know if another
      //  branch might still use "replace", so punt and give up
      if (assignments_are_safe && ir->opc == OPC_MOV && IsDeadAfter(ir, orig) && ir->cond == COND_TRUE) {
	return ir;
      }
      return NULL;
    }
    if (ir->dst == orig && InstrSetsDst(ir)) {
        // we do not want to end up changing "replace" if it is still live
        // note that IsDeadAfter(first_ir, replace) gives a more accurate
        // view than IsDeadAfter(ir, replace), because it can look back
        // further (and we've already verified that replace is not doing
        // anything interesting between first_ir and here)
        if (!IsDeadAfter(first_ir, replace)) {
            return NULL;
        }
    }
    if (ir->src == replace && ir != first_ir) {
      return NULL;
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
    if (IsDummy(ir)) {
      continue;
    }
    if (ir->opc == OPC_LABEL) {
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
    if (IsDummy(ir)) {
        if (ir->opc == OPC_DEAD && ir->dst == replace) {
            ir->opc = OPC_DUMMY;
        }
        if (ir->opc == OPC_DEAD && ir->dst == orig) {
            ir->opc = OPC_DUMMY;
        }
    } else {
        if (ir->src == orig) {
            ir->src = replace;
        }
        if (ir->dst == orig) {
            ir->dst = replace;
        }
    }
    if (ir == stop_ir) break;
  }
}

//
//
//
void
ApplyConditionAfter(IR *instr, int val)
{
  IR *ir;
  IRCond newcond;

  for (ir = instr->next; ir; ir = ir->next) {
    if (IsDummy(ir)) continue;
    switch (ir->cond) {
    case COND_TRUE:
      newcond = COND_TRUE;
      break;
    case COND_FALSE:
      newcond = COND_FALSE;
      break;
    case COND_EQ:
      newcond = (val == 0) ? COND_TRUE : COND_FALSE;
      break;
    case COND_NE:
      newcond = (val != 0) ? COND_TRUE : COND_FALSE;
      break;
    case COND_LT:
      newcond = (val < 0) ? COND_TRUE : COND_FALSE;
      break;
    case COND_GT:
      newcond = (val > 0) ? COND_TRUE : COND_FALSE;
      break;
    case COND_LE:
      newcond = (val <= 0) ? COND_TRUE : COND_FALSE;
      break;
    case COND_GE:
      newcond = (val >= 0) ? COND_TRUE : COND_FALSE;
      break;
    default:
      ERROR(NULL, "Internal error, unhandled condition");
      return;
    }
    ir->cond = newcond;
    if (SetsFlags(ir))
      return;
  }
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

  if (ir->src->kind != IMM_INT || imm->kind != IMM_INT)
    {
      return 0;
    }

  if (ir->flags & FLAG_WC) {
    // we don't know how to set the WC flag for anything other
    // than cmps
    if (ir->opc != OPC_CMPS) {
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
    case OPC_CMPS:
      val1 -= val2;
      setsResult = 0;
      break;
    default:
      return 0;
    }
  if (SetsFlags(ir)) {
    ApplyConditionAfter(ir, val1);
  }
  if (setsResult) {
    ReplaceOpcode(ir, OPC_MOV);
    ir->src = NewImmediate(val1);
  } else {
    ir->cond = COND_FALSE;
  }
  return 1;
}

//
// if we see x:=2 then replace future uses of x with 2
//
static int
PropagateConstForward(IR *instr, Operand *orig, Operand *imm)
{
  IR *ir;
  int change = 0;
  for (ir = instr; ir; ir = ir->next) {
    if (IsDummy(ir)) {
        continue;
    }
    if (IsLabel(ir)) {
        return change;
    }
    if (IsBranch(ir) && !JumpIsAfterOrEqual(instr, ir)) {
      return change;
    }
    if (ir->dst == orig) {
      // we can perhaps replace the operation with a mov
      change |= TransformConstDst(ir, imm);
      // but our register has changed, so we must stop
      return change;
    } else if (ir->src == orig) {
      ir->src = imm;
      change = 1;
    }
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
            if (ir->opc == OPC_MOV && ir->cond == COND_TRUE) {
	      if (ir->src == ir->dst && !SetsFlags(ir)) {
                    DeleteIR(irl, ir);
                    change = 1;
		} else if (IsImmediate(ir->src)) {
                    change |= PropagateConstForward(ir_next, ir->dst, ir->src);
	      } else if (!SetsFlags(ir) && IsDeadAfter(ir, ir->src) && SafeToReplaceBack(ir->prev, ir->src, ir->dst)) {
                    ReplaceBack(ir->prev, ir->src, ir->dst);
                    DeleteIR(irl, ir);
                    change = 1;
                }
	      else if ( !SetsFlags(ir) && 0 != (stop_ir = SafeToReplaceForward(ir->next, ir->dst, ir->src)) ) {
                    ReplaceForward(ir->next, ir->dst, ir->src, stop_ir);
                    DeleteIR(irl, ir);
                    change = 1;
                }
            }
            ir = ir_next;
        }
        everchange |= change;
    } while (change);
    return everchange;
}

static bool
HasSideEffects(IR *ir)
{
    if (ir->dst && ir->dst->kind == REG_HW) {
        return true;
    }
    if (InstrSetsFlags(ir)) {
        return true;
    }
    if (IsBranch(ir)) {
      return true;
    }
    switch (ir->opc) {
    case OPC_GENERIC:
    case OPC_GENERIC_NR:
    case OPC_WAITCNT:
    case OPC_WRBYTE:
    case OPC_WRLONG:
    case OPC_WRWORD:
    case OPC_COGSTOP:
    case OPC_COGID:
        return true;
    default:
        return false;
    }
}

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
    if (ir && ir->opc == OPC_JUMP && curfunc && ir->dst == FuncData(curfunc)->asmreturnlabel) {
      DeleteIR(irl, ir);
      change = 1;
    }
    // now look for other dead code
    ir = irl->head;
    while (ir) {
      ir_next = ir->next;
      if (ir->opc == OPC_JUMP) {
          if (ir->cond == COND_TRUE) {
              // dead code from here to next label
              IR *x = ir->next;
              while (x && x->opc != OPC_LABEL) {
                  ir_next = x->next;
                  if (!IsDummy(x)) {
                      DeleteIR(irl, x);
                      change = 1;
                  }
                  x = ir_next;
              }
              /* is the branch to the next instruction? */
              if (ir_next && ir_next->opc == OPC_LABEL && ir_next->dst == ir->dst) {
                  DeleteIR(irl, ir);
                  change = 1;
              }
          } else if (ir->cond == COND_FALSE) {
              DeleteIR(irl, ir);
          } else {
              /* if the branch is to the next instruction, delete it */
              if (ir_next && ir_next->opc == OPC_LABEL && ir_next->dst == ir->dst) {
                  DeleteIR(irl, ir);
                  change = 1;
              }
          }
      } else if (ir->cond == COND_FALSE) {
	  DeleteIR(irl, ir);
	  change = 1;
      } else if (!IsDummy(ir) && ir->dst && IsDeadAfter(ir, ir->dst) && !HasSideEffects(ir)) {
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
    if (IsDummy(ir) || ir->opc == OPC_LABEL) {
      continue;
    }
    CheckOpUsage(ir->src);
    CheckOpUsage(ir->dst);
  }
  /* remove unused labels */
  for (ir = irl->head; ir; ir = ir->next) {
    if (ir->opc == OPC_LABEL) {
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
static int IsShortForwardJump(IR *irbase)
{
  int n = 0;
  Operand *target;
  IR *ir;

  if (irbase->opc != OPC_JUMP)
    return 0;
  target = irbase->dst;
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
    while (ir) {
        ir_next = ir->next;
        n = IsShortForwardJump(ir);
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

    for (ir = irl->prev; ir; ir = ir->prev) {
        if (IsDummy(ir)) {
            continue;
        }
        if (ir->opc == OPC_LABEL) {
            // we may have branched to here from somewhere
            // else that did the set
            return NULL;
        }
        if (InstrSetsFlags(ir)) {
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
        if (ir->dst == dst && (InstrSetsDst(ir) || ir->opc == OPC_TEST)) {
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
// Optimize compares with 0by changing a previous instruction to set
// flags instead
//

static int
OptimizeCompares(IRList *irl)
{
    IR *ir;
    IR *ir_next;
    IR *ir_prev;
    int change = 0;
    
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
            )
        {
            ir_prev = FindPrevSetterForCompare(ir, ir->dst);
            if (ir_prev
                && !InstrSetsFlags(ir_prev)
                && CanTestZero(ir_prev->opc))
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
                    && IsImmediateVal(ir_prev->src, 1) )
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
    
    for (ir = irl->head; ir; ir = ir->next) {
        src = ir->src;
        if (! (src && src->kind == IMM_INT) ) {
            continue;
        }
        if (src->name == NULL || src->name[0] == 0) {
            /* already a small immediate */
            continue;
        }
        val = src->val;
        if (ir->opc == OPC_MOV && val < 0 && val >= -511) {
	    ReplaceOpcode(ir, OPC_NEG);
            ir->src = NewImmediate(-val);
        } else if (ir->opc == OPC_AND && val < 0 && val >= -512) {
	    ReplaceOpcode(ir, OPC_ANDN);
            ir->src = NewImmediate(~val);
        } else if (ir->opc == OPC_ADD && val < 0 && val >= -511) {
	    ReplaceOpcode(ir, OPC_SUB);
            ir->src = NewImmediate(-val);
        } else if (ir->opc == OPC_SUB && val < 0 && val >= -511) {
	    ReplaceOpcode(ir, OPC_ADD);
            ir->src = NewImmediate(-val);
	}
    }
    return 0; /* no rescan necessary */
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
        if (ir->opc == OPC_ADD || ir->opc == OPC_SUB) {
            prev = FindPrevSetterForReplace(ir, ir->dst);
            if (prev && (prev->opc == OPC_ADD || prev->opc == OPC_SUB) ) {
                if (ir->src->kind == IMM_INT && prev->src->kind == IMM_INT)
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
        ir->addr = addr++;
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
            if (ir->opc == OPC_DJNZ) {
                dst = ir->src;
            } else {
                dst = ir->dst;
            }
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
    if (name && (name[0] == 'L' && name[1] == '_' && isdigit(name[2]))) {
        while (name[0] && name[1] != '_') {
            name++;
        }
        return name[0]  != 0 && name[1] == '_';
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
            if (IsTemporaryLabel(ir->dst) && !(ir->flags & FLAG_LABEL_USED)) {
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
    case OPC_MUXC:
    case OPC_MUXNC:
    case OPC_GENERIC:
    case OPC_GENERIC_NR:
        ERROR(NULL, "Internal error, unexpected use of C");
        break;
    default:
        break;
    }
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
// if_c  or a, b
// if_nc andn a, b
//    becomes muxc a, b
//

int
OptimizePeepholes(IRList *irl)
{
    IR *ir, *ir_next;
    IR *previr;
    int changed = 0;
    
    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        while (ir_next && IsDummy(ir_next))
            ir_next = ir_next->next;
        
        if (ir->opc == OPC_AND && InstrSetsFlags(ir)) {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_MOV && IsDeadAfter(ir, ir->dst)) {
	        ReplaceOpcode(ir, OPC_TEST);
                ir->dst = previr->src;
                DeleteIR(irl, previr);
                changed = 1;
            } else if (IsDeadAfter(ir, ir->dst)) {
		ReplaceOpcode(ir, OPC_TEST);
            }
        } else if (ir->opc == OPC_SHR && !InstrSetsFlags(ir)
                   && !InstrUsesFlags(ir, FLAG_WC|FLAG_WZ)
                   && IsImmediateVal(ir->src, 1))
        {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_TEST
                && IsImmediateVal(previr->src, 1)
                && (previr->flags & (FLAG_WZ|FLAG_WC)) == FLAG_WZ)
            {
                /* maybe we can replace the test with the shr */
                /* we already know the result isn't used between previr
                   and ir; check the instructions in between for use
                   of C, and also verify that the flags aren't used
                   in subsequent instructions
                */
                IR *testir;
                IR *lastir = NULL;
                bool sawir = false;
                bool changeok = true;
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
                    } else if (InstrSetsFlags(testir)) {
                        changeok = sawir;
                        lastir = testir;
                        break;
                    }
                }
                if (changeok) {
                    /* ok, let's go ahead and change it */
                    ReplaceOpcode(previr, OPC_SHR);
                    previr->flags &= ~FLAG_WZ;
                    previr->flags |= FLAG_WC;
                    for (testir = previr->next; testir && testir != lastir;
                         testir = testir->next)
                    {
                        ReplaceZWithNC(testir);
                    }
                    DeleteIR(irl, ir);
                    changed = 1;
                }
            }
        }
        else if (ir->opc == OPC_OR && ir->cond == COND_C && !InstrSetsFlags(ir)
                 && ir_next->opc == OPC_ANDN && ir_next->cond == COND_NC
                 && !InstrSetsFlags(ir_next)
                 && ir->src == ir_next->src
                 && ir->dst == ir_next->dst)
        {
            ir_next->cond = COND_TRUE;
            ReplaceOpcode(ir_next, OPC_MUXC);
            DeleteIR(irl, ir);
            changed = 1;
        }
        else if (ir->opc == OPC_OR && ir->cond == COND_NE && !InstrSetsFlags(ir)
                 && ir_next->opc == OPC_ANDN && ir_next->cond == COND_EQ
                 && !InstrSetsFlags(ir_next)
                 && ir->src == ir_next->src
                 && ir->dst == ir_next->dst)
        {
            ir_next->cond = COND_TRUE;
            ReplaceOpcode(ir_next, OPC_MUXNZ);
            DeleteIR(irl, ir);
            changed = 1;
        }
        ir = ir_next;
    }
    return changed;
}

//
// optimize
//

// optimize an isolated piece of IRList
// (typically a function)
void
OptimizeIRLocal(IRList *irl)
{
    int change;
    
    if (gl_optimize_flags & OPT_NO_ASM) return;
    if (!irl->head) return;
    do {
        change = 0;
        AssignTemporaryAddresses(irl);
        change |= CheckLabelUsage(irl);
        change |= EliminateDeadCode(irl);
        change |= OptimizeMoves(irl);
        change |= OptimizeImmediates(irl);
        change |= OptimizeShortBranches(irl);
        change |= OptimizeAddSub(irl);
        change |= OptimizeCompares(irl);
        change |= OptimizePeepholes(irl);
    } while (change != 0);
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
        if (ir->opc == OPC_DEAD || ir->opc == OPC_DUMMY) {
            DeleteIR(irl, ir);
        }
        ir = ir_next;
    }
    // check for usage
    CheckUsage(irl);
}


//
// check a function to see if it should be inlined
//
#define INLINE_THRESHOLD 3

bool
ShouldBeInlined(Function *f)
{
    IR *ir;
    int n = 0;

    if (gl_optimize_flags & OPT_NO_ASM) return false;
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
    return (n <= INLINE_THRESHOLD);
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
                change = 1;
            }
        }
        ir = ir_next;
    }
    return change;
}
