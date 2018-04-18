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

int gl_fcache_size = 64; // 256 bytes of FCACHE

//
// helper functions
//

/* IR instructions that have no effect on the generated code */
bool IsDummy(IR *op)
{
  switch(op->opc) {
  case OPC_COMMENT:
  case OPC_LITERAL:
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
  case OPC_GENERIC_BRANCH:
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

// return TRUE if an instruction modifies a register
bool
InstrModifies(IR *ir, Operand *reg)
{
    Operand *dst = ir->dst;
    if (dst == reg) {
        return InstrSetsDst(ir);
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
    return InstrSetsFlags(ir, FLAG_WZ|FLAG_WC);
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

Operand *
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

/*
 * return TRUE if the operand's value does not need to be preserved
 * after instruction instr
 * follows branches, but gives up after MAX_FOLLOWED_JUMPS to prevent
 * inifinite loops
 */
#define MAX_FOLLOWED_JUMPS 8

static bool
doIsDeadAfter(IR *instr, Operand *op, int level, IR **stack)
{
  IR *ir;
  int i;
  
  if (instr->opc == OPC_DEAD && op == instr->dst) {
    return true;
  }
  if (level >= MAX_FOLLOWED_JUMPS) {
      // give up!
      return false;
  }
  if (op->kind == REG_HW) {
      // hardware registers are never dead
      return false;
  }
  for (ir = instr->next; ir; ir = ir->next) {
    if (ir->opc == OPC_DEAD && op == ir->dst) {
      return true;
    }
    for (i = 0; i < level; i++) {
        if (ir == stack[i]) {
            // we've come around a loop
            return IsLocalOrArg(op);
        }
    }
    if (IsDummy(ir)) continue;
    if (ir->opc == OPC_LABEL) {
        // potential problem: if there is a branch before instr
        // that goes to LABEL then we might miss a set
        // so check
        IR *comefrom = (IR *)ir->aux;
        if (!comefrom || (level == 0 && comefrom->addr < instr->addr)) {
            return false;
        }
        continue;
    }
    if (InstrUses(ir, op)) {
        // value is used, so definitely not dead
        return false;
    }
    if (InstrModifies(ir, op)) {
      if (ir->cond == COND_TRUE) {
	return true;
//      } else {
//        return false;
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
        // if the jump is to an unknown place give up
        if (!ir->aux) {
            return false;
        }
        stack[level] = ir; // remember where we were
        if (!doIsDeadAfter((IR *)ir->aux, op, level+1, stack)) {
            return false;
        }
        if (ir->cond == COND_TRUE && ir->opc == OPC_JUMP) {
            return true;
        }
    }
  }
  /* if we reach the end without seeing any use */
  return IsLocalOrArg(op);
}

static bool
IsDeadAfter(IR *instr, Operand *op)
{
    IR *stack[MAX_FOLLOWED_JUMPS];
    return doIsDeadAfter(instr, op, 0, stack);
}

static bool
SafeToReplaceBack(IR *instr, Operand *orig, Operand *replace)
{
  IR *ir;
  if (SrcOnlyHwReg(replace))
      return false;
  for (ir = instr; ir; ir = ir->prev) {
      if (IsDummy(ir)) {
          continue;
      }
      if (ir->opc == OPC_LABEL) {
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
          return ir->cond == COND_TRUE;
      }
      if (InstrUses(ir, replace) || ir->dst == replace) {
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
  bool orig_modified = false;
  
  if (SrcOnlyHwReg(replace)) {
      return NULL;
  }
  for (ir = first_ir; ir; ir = ir->next) {
    if (ir->opc == OPC_DEAD) {
        if (ir->dst == orig) {
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
    if (InstrModifies(ir,replace)) {
      // special case: if we have a "mov replace,x" and orig is dead
      // then we are good to go; at that point we know it is safe to replace
      // orig with replace because:
      //  (a) orig is dead after this, so not used
      //  (b) whatever we did to replace up til now it doesn't matter, a fresh
      //      value is being put into it
      //  if "assignments_are_safe" is false then we don't know if another
      //  branch might still use "replace", so punt and give up
      if (assignments_are_safe && !InstrUses(ir, replace) && IsDeadAfter(ir, orig) && ir->cond == COND_TRUE) {
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
        // we do not want to end up changing "replace" if it is still live
        // note that IsDeadAfter(first_ir, replace) gives a more accurate
        // view than IsDeadAfter(ir, replace), because it can look back
        // further (and we've already verified that replace is not doing
        // anything interesting between first_ir and here)
        if (!IsDeadAfter(first_ir, replace)) {
            return NULL;
        }
        orig_modified = true;
    }
    if (InstrUses(ir, replace) && ir != first_ir) {
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
        if (ir->opc == OPC_DEAD && ir->dst == replace) {
            ir->opc = OPC_DUMMY;
        }
        if (ir->opc == OPC_DEAD && ir->dst == orig) {
            ir->opc = OPC_DUMMY;
        }
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
    if (InstrSetsAnyFlags(ir))
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

  if (ir->src == NULL || ir->src->kind != IMM_INT || imm->kind != IMM_INT)
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
  if (InstrSetsAnyFlags(ir)) {
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
    if (ir->opc == OPC_CALL) {
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
            if (InstrIsVolatile(ir)) {
                /* do nothing */
            } else if (ir->opc == OPC_MOV && ir->src == ir->dst) {
                if (!InstrSetsAnyFlags(ir)) {
                    DeleteIR(irl, ir);
                    change = 1;
                }
            } else if (ir->opc == OPC_MOV && ir->cond == COND_TRUE) {
                if (ir->src == ir->dst && !InstrSetsAnyFlags(ir)) {
                    DeleteIR(irl, ir);
                    change = 1;
                } else if (IsImmediate(ir->src)) {
                    int sawchange;
                    change |= (sawchange =PropagateConstForward(ir_next, ir->dst, ir->src));
                    if (sawchange && !InstrSetsAnyFlags(ir) && IsDeadAfter(ir, ir->dst)) {
                        // we no longer need the original mov
                        DeleteIR(irl, ir);
                    }
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
HasSideEffects(IR *ir)
{
    if (ir->dst && !IsLocalOrArg(ir->dst) /*ir->dst->kind == REG_HW*/) {
        return true;
    }
    if (InstrSetsAnyFlags(ir)) {
        return true;
    }
    if (IsBranch(ir)) {
      return true;
    }
    if (InstrIsVolatile(ir)) {
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
    case OPC_ADDCT1:
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
              /* if the branch skips over things that already have the right
                 condition, delete it
              */
              if (ir->dst && ir_next && AllInstructionsConditional(InvertCond(ir->cond), ir_next, ir->dst))
              {
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
static int IsSafeShortForwardJump(IR *irbase)
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
      if (ir->opc == OPC_CALL) {
          // calls do not preserve condition codes
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
                    if (loopend->opc == OPC_JUMP && loopend->cond == COND_TRUE && loopend->aux && ir_prev == (IR *)loopend->aux) {
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
    if (name && (name[0] == 'L' && name[1] == '_' && name[2] == '_' && isdigit(name[3]))) {
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
                    } else {
                        DeleteIR(irl, ir);
                    }
                    changed = 1;
                    goto done;
                } else if (opc == OPC_OR && ((oldmask | newmask) == oldmask)) {
                    if (InstrSetsFlags(ir, FLAG_WZ)) {
                        ReplaceOpcode(ir, OPC_CMP);
                        ir->src->val = 0;
                    } else {
                        DeleteIR(irl, ir);
                    }
                    changed = 1;
                    goto done;
                }
            }
        }
        if (opc == OPC_AND && InstrSetsAnyFlags(ir)) {
            previr = FindPrevSetterForReplace(ir, ir->dst);
            if (previr && previr->opc == OPC_MOV && IsDeadAfter(ir, ir->dst) && !SrcOnlyHwReg(previr->src)) {
	        ReplaceOpcode(ir, OPC_TEST);
                ir->dst = previr->src;
                DeleteIR(irl, previr);
                changed = 1;
            } else if (IsDeadAfter(ir, ir->dst)) {
		ReplaceOpcode(ir, OPC_TEST);
                changed = 1;
            }
        } else if (opc == OPC_SHR && !InstrSetsFlags(ir, FLAG_WC)
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
                    ReplaceOpcode(previr, OPC_SHR);
                    previr->flags &= ~(FLAG_WZ|FLAG_WC);
                    previr->flags |= irflags;
                    for (testir = previr->next; testir && testir != lastir;
                         testir = testir->next)
                    {
                        ReplaceZWithNC(testir);
                    }
                    DeleteIR(irl, ir);
                    changed = 1;
                    goto done;
                }
            }
        }
        else if (opc == OPC_OR && ir->cond == COND_C && !InstrSetsAnyFlags(ir)
                 && ir_next->opc == OPC_ANDN && ir_next->cond == COND_NC
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
                 && ir_next->opc == OPC_SUB
                 && ir_next->cond == ir->cond
                 && ir_next->dst == ir->src
                 && !InstrSetsAnyFlags(ir_next)
                 && 0 != (previr = FindPrevSetterForReplace(ir, ir->src))
                 && previr->cond == ir->cond
                 && previr->opc == OPC_ADD
                 && previr->src == ir_next->src
                 && previr->dst == ir_next->dst
                 && !InstrSetsAnyFlags(ir_next)
            )
        {
            // add x, y   '' previr
            // mov a, x   '' ir
            // sub x, y   '' ir_next
            // => mov a, x
            //    add a, y
            ReplaceOpcode(ir_next, OPC_ADD);
            ir_next->dst = ir->dst;
            DeleteIR(irl, previr);
            changed = 1;
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
    done:
        ir = ir_next;
    }
    return changed;
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
        ir = ir_next;
    }
    return changed;
}

//
// find the next rdlong that uses src
// returns NULL if we spot anything that changes src, dest,
// or a branch
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
        if (ir->opc == OPC_RDLONG || ir->opc == OPC_WRLONG) {
            dst1 = ir->dst;
            base = ir->src;
            nextread = FindNextRead(ir, dst1, base);
            if (nextread) {
                nextread->src = dst1;
                ReplaceOpcode(nextread, OPC_MOV);
                change = 1;
                goto get_next;
            }
        }
#if 1
        // try to avoid having two read/write ops in a row
        if (IsReadWrite(ir) && IsReadWrite(next_ir) && IsNonReadWriteOpcode(prev_ir)) {
            if (CanSwap(ir, prev_ir)) {
                // want to swap prev_ir and ir here
                IR *tail = prev_ir->next;
                DeleteIR(irl, prev_ir);  // remove prev_ir from list
                prev_ir->next = NULL;
                InsertAfterIR(irl, ir, prev_ir); // move it to later
                ir = prev_ir;
                change = 1;
                // get rid of any .dead notes
                while (tail && tail != ir) {
                    if (tail->opc == OPC_DEAD) {
                        tail->opc = OPC_DUMMY;
                    }
                    tail = tail->next;
                }
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
        change |= OptimizeReadWrite(irl);
        change |= OptimizeMoves(irl);
        change |= OptimizeImmediates(irl);
        change |= OptimizeCompares(irl);
        change |= OptimizeShortBranches(irl);
        change |= OptimizeAddSub(irl);
        change |= OptimizePeepholes(irl);
        if (gl_p2) {
            change |= OptimizeP2(irl);
        }
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
    // check for fcache
    OptimizeFcache(irl);
    
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
    if (f->no_inline) return false;
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
    if (RemoveIfInlined(f)) {
        if (f->callSites == 1) {
            return true;
        } else if (f->callSites == 2) {
            return (n <= 2*INLINE_THRESHOLD);
        }
    }

    // otherwise only inline small functions
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

//
// convert loops to FCACHE when we can
//

static bool
LoopCanBeFcached(IRList *irl, IR *root)
{
    IR *endjmp;
    IR *endlabel;
    IR *ir = root;
    int size = 0;
    
    if (!IsHubDest(ir->dst)) {
        // this loop is not in HUB memory
        return false;
    }
    endjmp = ir->aux;
    if (!endjmp || !IsJump(endjmp)) {
        // we don't know who jumps here
        return false;
    }
    endlabel = endjmp;
    while (endlabel->next && endlabel->next->opc == OPC_LABEL) {
        endlabel = endlabel->next;
    }
    if (IsForwardJump(endjmp)) {
        return false;
    }
    {
        ir = ir->next;
        while (ir != endjmp) {
            if (ir->opc == OPC_CALL) {
                // no calls to hub memory!
                if (IsHubDest(ir->dst)) {
                    return false;
                }
            }
            if (IsJump(ir)) {
                if (!JumpIsAfterOrEqual(root, ir))
                    return false;
                if (JumpDest(ir) != endlabel->dst && JumpIsAfterOrEqual(endlabel, ir))
                    return false;
            }
            if (!IsDummy(ir) && ir->opc != OPC_LABEL) {
                size++;
                if (size >= gl_fcache_size) {
                    return false;
                }
            }
            ir = ir->next;
        }
    }
    return true;
}

void
OptimizeFcache(IRList *irl)
{
    IR *ir;

    if (gl_p2)
        return;
    
    ir = irl->head;
    while (ir) {
        if (IsLabel(ir)) {
            if (LoopCanBeFcached(irl, ir)) {
                Operand *src = ir->dst;
                Operand *dst = NewHubLabel();
                IR *endlabel = NewIR(OPC_LABEL);
                IR *fcache = NewIR(OPC_FCACHE);
                IR *jmp = (IR *)ir->aux;
                fcache->src = src;
                fcache->dst = dst;
                endlabel->dst = dst;
                InsertAfterIR(irl, jmp, endlabel);
                InsertAfterIR(irl, ir->prev, fcache);
                while (ir != endlabel) {
                    ir->fcache = src;
                    ir = ir->next;
                }
            }
        }
        ir = ir->next;
    }
}
