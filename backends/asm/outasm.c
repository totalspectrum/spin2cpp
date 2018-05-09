//
// Pasm data output for spin2cpp
//
// Copyright 2016-2018 Total Spectrum Software Inc.
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

#define SETJMP_BUF_SIZE (8*LONG_SIZE)

#define ENTRYNAME "entry"
#define COG_CODE (gl_outputflags & OUTFLAG_COG_CODE)
#define HUB_CODE (!COG_CODE)
#define COG_DATA (gl_outputflags & OUTFLAG_COG_DATA)
#define HUB_DATA (!COG_DATA)

#define IS_FAST_CALL(f) (FuncData(f)->convention == FAST_CALL)
#define IS_STACK_CALL(f) (FuncData(f)->convention == STACK_CALL)

/* lists of instructions in hub and cog */
static IRList cogcode;
static IRList hubcode;
static IRList cogdata;
static IRList cogbss;
static IRList hubdata;

Operand *mulfunc, *muldiva, *muldivb;
Operand *divfunc;

static Operand *putcogreg;

static Operand *abortfunc;
static Operand *catchfunc;
static Operand *arg1;
static Operand *arg2;
static Operand *result1;
static Operand *abortcalled;

static Operand *nextlabel;
static Operand *quitlabel;

static Operand *objbase;
static Operand *objlabel;

static Operand *stackptr;
static Operand *stacklabel;
static Operand *stacktop;  // indirect reference through stackptr
static Operand *frameptr;
static Operand *linkreg;

static Operand *hubexit;
static Operand *cogexit;

static Operand *kernelptr;

static Operand *CompileExpression(IRList *irl, AST *expr, Operand *dest);
static Operand* CompileMul(IRList *irl, AST *expr, int gethi, Operand *dest);
static Operand* CompileDiv(IRList *irl, AST *expr, int getmod, Operand *dest);
static Operand *Dereference(IRList *irl, Operand *op);
static Operand *CompileFunccall(IRList *irl, AST *expr);
static Operand *CompileIdentifierForFunc(IRList *irl, AST *expr, Function *func);

static Operand *GetAddressOf(IRList *irl, AST *expr);
static IR *EmitMove(IRList *irl, Operand *dst, Operand *src);
//static void EmitLea(IRList *irl, Operand *dst, Operand *src);
static Operand *GetLea(IRList *irl, Operand *src);
static void EmitBuiltins(IRList *irl);
static IR *EmitOp1(IRList *irl, IROpcode code, Operand *op);
static IR *EmitOp2(IRList *irl, IROpcode code, Operand *op, Operand *op2);
static void CompileConsts(IRList *irl, AST *consts);
static void EmitAddSub(IRList *irl, Operand *dst, int off);
static Operand *SizedHubMemRef(int size, Operand *addr, int offset);
static Operand *CogMemRef(Operand *addr, int offset);
static void EmitDebugComment(IRList *irl, AST *ast);

typedef struct AsmVariable {
    Operand *op;
    intptr_t val;
    size_t count; // number of reps of "value"
} AsmVariable;

// global variables in COG memory (really holds struct AsmVariables)
static struct flexbuf cogGlobalVars;

// global variables in hub memory (really holds struct AsmVariables)
static struct flexbuf hubGlobalVars;

// returns true if P is the top level module for this project
static int
IsTopLevel(Module *P)
{
    extern Module *allparse;
    return P == allparse;
}

static const char *
IdentifierLocalName(Function *func, const char *name)
{
    char temp[1024];
    Module *P = func->module;
    if (func->is_leaf) {
        // leaf functions can share a set of local variables
        Symbol *s = FindSymbol(&func->localsyms, name);
        int offset = -1;
        if (s) {
            if (s->type == SYM_RESULT) {
                offset = 0;
            } else if (s->type == SYM_PARAMETER) {
                offset = (LONG_SIZE + s->offset)/LONG_SIZE;
            } else if (s->type == SYM_LOCALVAR || s->type == SYM_TEMPVAR) {
                offset = (LONG_SIZE * (1+func->numparams) + s->offset) / LONG_SIZE;
            }
        }
        if (offset > 0) {
            snprintf(temp, sizeof(temp)-1, "_var_%02d", offset);
        } else {
            snprintf(temp, sizeof(temp)-1, "_var_%s", name);
        }
    } else if (IsTopLevel(P)) {
        snprintf(temp, sizeof(temp)-1, "_%s_%s", func->name, name);
    } else {
        snprintf(temp, sizeof(temp)-1, "_%s_%s_%s", P->classname, func->name, name);
    }
    return strdup(temp);
}

static const char *
IdentifierGlobalName(Module *P, const char *name)
{
    char temp[1024];
    if (IsTopLevel(P)) {
        // avoid conflict with built-in assembler names by prepending "_"
        snprintf(temp, sizeof(temp)-1, "_%s", name);
    } else {
        snprintf(temp, sizeof(temp)-1, "_%s_%s", P->classname, name);
    }
    return strdup(temp);
}

static void
ValidateObjbase(void)
{
    if (!objbase) {
        objlabel = NewOperand(IMM_HUB_LABEL, "objmem", 0);
        objbase = NewImmediatePtr("objptr", objlabel);
    }
}

static void
ValidateStackptr(void)
{
    if (!stackptr) {
        if (HUB_DATA || gl_p2) {
            stacklabel = NewOperand(IMM_HUB_LABEL, "stackspace", 0);
            if (gl_p2) {
                stackptr = GetOneGlobal(REG_HW, "ptra", 0);
            } else if (gl_optimize_flags & OPT_REMOVE_HUB_BSS) {
                stackptr = GetOneGlobal(REG_REG, "sp", current->varsize);
            } else {
                stackptr = NewImmediatePtr("sp", stacklabel);
            }
            stacktop = SizedHubMemRef(LONG_SIZE, stackptr, 0);
        } else {
            stacklabel = NewOperand(IMM_COG_LABEL, "stackspace", 0);
            stackptr = NewImmediatePtr("sp", stacklabel);
            stacktop = CogMemRef(stackptr, 0);            
        }
    }
}

static void
ValidateAbortFuncs(void)
{
    if (!abortfunc) {
        abortfunc = NewOperand(IMM_COG_LABEL, "__abort", 0);
        catchfunc = NewOperand(IMM_COG_LABEL, "__setjmp", 0);
        abortcalled = GetOneGlobal(REG_REG, "result2", 0);
        if (!result1) {
            result1 = GetOneGlobal(REG_REG, "result1", 0);
        }
        if (!arg1) {
            arg1 = GetOneGlobal(REG_ARG, "arg1", 0);
        }
        if (!frameptr) {
            frameptr = GetOneGlobal(REG_REG, "fp", 0);
        }
        ValidateStackptr();
        ValidateObjbase();
    }
}

static int IsMemRef(Operand *op)
{
    return op && (op->kind >= LONG_REF) && (op->kind <= COG_REF);
}

static Operand *
GetSizedVar(struct flexbuf *fb, Operandkind kind, const char *name, intptr_t value, int count)
{
  size_t siz = flexbuf_curlen(fb) / sizeof(AsmVariable);
  size_t i;
  AsmVariable tmp;
  AsmVariable *g = (AsmVariable *)flexbuf_peek(fb);
  for (i = 0; i < siz; i++) {
    if (strcmp(name, g[i].op->name) == 0) {
        if (g[i].val != value) {
            ERROR(NULL, "Internal error, redefining value of %s", name);
        }
        if (g[i].count < count) {
            g[i].count = count;
        }
      return g[i].op;
    }
  }
  tmp.op = NewOperand(kind, name, value);
  tmp.val = value;
  tmp.count = count;
  flexbuf_addmem(fb, (const char *)&tmp, sizeof(tmp));
  return tmp.op;
}

Operand *GetOneGlobal(Operandkind kind, const char *name, intptr_t value)
{
    return GetSizedVar(&cogGlobalVars, kind, name, value, 1);
}
Operand *GetSizedGlobal(Operandkind kind, const char *name, intptr_t value, int size)
{
    return GetSizedVar(&cogGlobalVars, kind, name, value, size);
}

Operand *GetOneHub(Operandkind kind, const char *name, intptr_t value)
{
    return GetSizedVar(&hubGlobalVars, kind, name, value, 1);
}

Instruction *
FindInstrForOpc(IROpcode kind)
{
    static Instruction **lookup_table;
    extern Instruction *instr; // in lexer.c
    Instruction *r;
    
    if ((unsigned int)kind >= OPC_GENERIC) {
        return NULL;
    }
    if (!lookup_table) {
        // set up the lookup table
        int i = 0;
        lookup_table = (Instruction **)calloc(1, sizeof(Instruction *) * (unsigned int)OPC_GENERIC);
        do {
            if ((unsigned int)instr[i].opc < OPC_GENERIC) {
                lookup_table[(unsigned int)instr[i].opc] = &instr[i];
            }
            i++;
        } while (instr[i].name != 0);
    }
    r = lookup_table[(unsigned int)kind];
    if (!r) {
        ERROR(NULL, "internal error: unknown instruction");
    }
    return r;
}

IR *NewIR(IROpcode kind)
{
    IR *ir = (IR *)malloc(sizeof(*ir));
    memset(ir, 0, sizeof(*ir));
    ir->opc = kind;
    ir->instr = FindInstrForOpc(kind);
    return ir;
}

IR *DupIR(IR *old)
{
    IR *ir = NewIR(OPC_DUMMY);
    memcpy(ir, old, sizeof(*ir));
    ir->prev = ir->next = NULL;
    return ir;
}

static void
ReplaceJumpTarget(IR *jmpir, Operand *dst)
{
    switch(jmpir->opc) {
    case OPC_JUMP:
        jmpir->dst = dst;
        break;
    case OPC_DJNZ:
        jmpir->src = dst;
        break;
    default:
        ERROR(NULL, "Unable to replace jump target");
    }
}

//
// replace the individual IR "ir" in list "irl" with a list of
// instructions from function f; used for e.g. expanding inline functions
//
void ReplaceIRWithInline(IRList *irl, IR *origir, Function *func)
{
    IR *newir;
    IR *dest = origir->prev;
    IRList *insert = FuncIRL(func);
    IR *ir;

    if (FuncData(func)->asmretname != FuncData(func)->asmreturnlabel) {
        ERROR(func->body, "Internal error, illegal inline");
    }
    DeleteIR(irl, origir);
    ir = insert->head;
    while (ir) {
        if (ir->opc == OPC_DEAD) {
            // remove all dead notes, etc. from the original inline
            ir = ir->next;
            continue;
        }
        newir = DupIR(ir);
        newir->flags |= FLAG_INSTR_NEW;
        if (newir->opc == OPC_LABEL) {
        // leave off the asm return name, if it's there
        // FIXME: this is probably an obsolete test now
            if (newir->dst == FuncData(func)->asmretname) {
                /* do nothing */
            } else {
                // make the label kind match the appropriate type
                // of label for this function
                if (curfunc->cog_code) {
                    newir->dst->kind = IMM_COG_LABEL;
                } else {
                    newir->dst->kind = IMM_HUB_LABEL;
                }
                InsertAfterIR(irl, dest, newir);
                dest = newir;
            }
        } else if (newir->opc == OPC_COMMENT) {
            /* leave out comments, they will be out of place */
        } else {
            InsertAfterIR(irl, dest, newir);
            dest = newir;
        }
        ir = ir->next;
    }

    // replace all labels in the original inline code
    for (ir = insert->head; ir; ir = ir->next) {
        if (ir->opc == OPC_LABEL) {
            if (ir->dst == FuncData(func)->asmretname) {
                // do nothing
            } else {
                IR *jmpir;
                ir->dst = NewCodeLabel();
                jmpir = (IR *)ir->aux;
                if (!jmpir) {
                    ERROR(NULL, "internal error: unable to find jump target");
                    return;
                }
                ReplaceJumpTarget(jmpir, ir->dst);
            }
        }
    }
}

Operand *NewOperand(enum Operandkind k, const char *name, intptr_t value)
{
    Operand *R = (Operand *)malloc(sizeof(*R));
    memset(R, 0, sizeof(*R));
    R->kind = k;
    R->name = name;
    R->val = value;
    if (k == IMM_COG_LABEL || k == IMM_HUB_LABEL) {
      R->used = 1;
    }
    return R;
}

void
AppendOperand(OperandList **listptr, Operand *op)
{
  OperandList *next = (OperandList *)malloc(sizeof(OperandList));
  OperandList *x;
  next->op = op;
  next->next = NULL;
  for(;;) {
    x = *listptr;
    if (!x) {
      *listptr = next;
      return;
    }
    listptr = &x->next;
  }
}

void
AppendOperandUnique(OperandList **listptr, Operand *op)
{
  OperandList *next;
  OperandList *x;
  for(;;) {
    x = *listptr;
    if (!x) {
        next = (OperandList *)malloc(sizeof(OperandList));
        next->op = op;
        next->next = NULL;
        *listptr = next;
        return;
    }
    if (x->op == op) {
        return;
    }
    listptr = &x->next;
  }
}

/*
 * append some IR to the end of a list
 */
void
AppendIR(IRList *irl, IR *ir)
{
    IR *last = irl->tail;
    InsertAfterIR(irl, last, ir);
}

/*
 * insert a new IR element after element orig
 * if orig == NULL then place orig in the
 * beginning of the list
 */

void
InsertAfterIR(IRList *irl, IR *orig, IR *ir)
{
    IR *o_next;

    if (!ir) return;
    
    if (!orig) {
        /* place at the beginning of the list */
        o_next = irl->head;
        irl->head = ir;
        ir->prev = NULL;
    } else {
        /* place it after orig */
        o_next = orig->next;
        orig->next = ir;
        ir->prev = orig;
    }
    /* now skip to the end of the ir list */
    while (ir->next) {
        ir = ir->next;
    }
    /* now fix up o_next to point back to ir */
    if (!o_next) {
        irl->tail = ir;
        ir->next = NULL;
    } else {
        ir->next = o_next;
        o_next->prev = ir;
    }
}

/*
 * Append a whole list
 */
void
AppendIRList(IRList *irl, IRList *sub)
{
  IR *last = irl->tail;
  if (!last) {
    irl->head = sub->head;
    irl->tail = sub->tail;
    return;
  }
  AppendIR(irl, sub->head);
}

/* Delete one IR from a list */
void
DeleteIR(IRList *irl, IR *ir)
{
  IR *prev = ir->prev;
  IR *next = ir->next;

  if (prev) {
    prev->next = next;
  } else {
    irl->head = next;
  }
  if (next) {
    next->prev = prev;
  } else {
    irl->tail = prev;
  }
}

// emit a machine instruction with no operands
static IR *EmitOp0(IRList *irl, IROpcode code)
{
  IR *ir = NewIR(code);
  AppendIR(irl, ir);
  return ir;
}

// emit a machine instruction with one operand
static IR *EmitOp1(IRList *irl, IROpcode code, Operand *op)
{
  IR *ir = NewIR(code);
  ir->dst = op;
  AppendIR(irl, ir);
  return ir;
}

// emit a machine instruction with two operands
static IR *EmitOp2(IRList *irl, IROpcode code, Operand *d, Operand *s)
{
  IR *ir = NewIR(code);
  ir->dst = d;
  ir->src = s;
  AppendIR(irl, ir);
  return ir;
}

// emit an assembler label
void EmitLabel(IRList *irl, Operand *op)
{
  EmitOp1(irl, OPC_LABEL, op);
}

void EmitNamedCogLabel(IRList *irl, const char *name)
{
    EmitLabel(irl, NewOperand(IMM_COG_LABEL, name, 0));
}

// create a new temporary label name
char *
NewTempLabelName()
{
    return NewTemporaryVariable("L_");
}

Operand *
NewHubLabel()
{
  Operand *label;
  label = NewOperand(IMM_HUB_LABEL, NewTempLabelName(), 0);
  label->used = 0;
  return label;
}

Operand *
NewCodeLabel()
{
  Operand *label;
  if (curfunc && !curfunc->cog_code) {
      label = NewOperand(IMM_HUB_LABEL, NewTempLabelName(), 0);
  } else {
      label = NewOperand(IMM_COG_LABEL, NewTempLabelName(), 0);
  }
  label->used = 0;
  return label;
}

Operand *
NewDataLabel()
{
  Operand *label;
  label = NewOperand(IMM_HUB_LABEL, NewTempLabelName(), 0);
  label->used = 0;
  return label;
}

void EmitLong(IRList *irl, int val)
{
  Operand *op;

  op = NewOperand(IMM_INT, "", val);
  EmitOp1(irl, OPC_LONG, op);
}

#define COG_RESERVE 0
#define HUB_RESERVE 1

void EmitReserve(IRList *irl, int val, int isHub)
{
  Operand *op;

  op = NewOperand(IMM_INT, "", val);
  if (isHub) {
      EmitOp1(irl, OPC_RESERVEH, op);
  } else {
      EmitOp1(irl, OPC_RESERVE, op);
  }
}

static void EmitLongPtr(IRList *irl, Operand *op)
{
  EmitOp1(irl, OPC_LONG, op);
}

static void EmitStringNoTrailingZero(IRList *irl, AST *ast)
{
  Operand *op;
  switch (ast->kind) {
  case AST_STRING:
      op = NewOperand(IMM_STRING, ast->d.string, 0);
      EmitOp1(irl, OPC_STRING, op);
      break;
  case AST_INTEGER:
      op = NewOperand(IMM_INT, "", (int)ast->d.ival);
      EmitOp1(irl, OPC_BYTE, op);
      break;
  default:
      if (IsConstExpr(ast)) {
          op = NewOperand(IMM_INT, "", EvalConstExpr(ast));
          EmitOp1(irl, OPC_BYTE, op);
      } else {
          ERROR(ast, "Unable to emit string");
      }
      break;
  }
}

void EmitString(IRList *irl, AST *ast)
{
  while (ast && ast->kind == AST_EXPRLIST) {
      EmitStringNoTrailingZero(irl, ast->left);
      ast = ast->right;
  }
  if (ast) {
      EmitStringNoTrailingZero(irl, ast);
  }
  // add a trailing 0
  EmitOp1(irl, OPC_BYTE, NewImmediate(0));
}

void EmitJump(IRList *irl, IRCond cond, Operand *label)
{
  IR *ir;

  if (cond == COND_FALSE) return;
  ir = NewIR(OPC_JUMP);
  ir->dst = label;
  ir->cond = cond;
  AppendIR(irl, ir);
}

Operand *
NewImmediate(int32_t val)
{
  char temp[1024];
  if ( gl_p2 || (val >= 0 && val < 512) ) {
    return NewOperand(IMM_INT, "", (int32_t)val);
  }
  sprintf(temp, "imm_%u_", (unsigned)val);
  return GetOneGlobal(IMM_INT, strdup(temp), (int32_t)val);
}

Operand *
NewImmediatePtr(const char *name, Operand *val)
{
    char temp[1024];
    if (!name) {
        sprintf(temp, "ptr_%s_", val->name);
        name = strdup(temp);
    }
    if (val->kind == IMM_COG_LABEL) {
        return GetOneGlobal(REG_COGPTR, name, (intptr_t)val);
    } else {
        return GetOneGlobal(REG_HUBPTR, name, (intptr_t)val);
    }
}

static Operand *
GetFunctionTempRegister(Function *f, int n)
{
  Module *P = f->module;
  char buf[1024];
  if (f->is_leaf) {
      // leaf functions can share a set of temporary registers
      snprintf(buf, sizeof(buf)-1, "_tmp%03d_", n);
  } else if (IsTopLevel(P)) {
      snprintf(buf, sizeof(buf)-1, "%s_tmp%03d_", f->name, n);
  } else {
      snprintf(buf, sizeof(buf)-1, "%s_%s_tmp%03d_", P->classname, f->name, n);
  }
  return GetOneGlobal(REG_LOCAL, strdup(buf), 0);
}

Operand *
NewFunctionTempRegister()
{
    Function *f = curfunc;
    IRFuncData *fdata = FuncData(f);

    fdata->curtempreg++;
    if (fdata->curtempreg > fdata->maxtempreg) {
        fdata->maxtempreg = fdata->curtempreg;
    }
    return GetFunctionTempRegister(f, fdata->curtempreg);
}

static Operand *
SizedHubMemRef(int size, Operand *addr, int offset)
{
    Operand *temp;

    if (size == 1) {
        temp = NewOperand(BYTE_REF, (char *)addr, offset);
    } else if (size == 2) {
        temp = NewOperand(WORD_REF, (char *)addr, offset);
    } else {
        temp = NewOperand(LONG_REF, (char *)addr, offset);
        if (size != 4) {
            ERROR(NULL, "Illegal size for memory reference");
        }
    }
    return temp;
}

static Operand *
TypedHubMemRef(AST *type, Operand *addr, int offset)
{
    int size;
    while (type && (type->kind == AST_ARRAYTYPE || type->kind == AST_PTRTYPE)) {
        type = type->left;
    }
    if (!type) {
        size = 4;
    } else {
        size = EvalConstExpr(type->left);
    }
    return SizedHubMemRef(size, addr, offset);
}

static Operand *
CogMemRef(Operand *addr, int offset)
{
    return NewOperand(COG_REF, (char *)addr, offset);
}

static Operand *
FrameRef(int offset)
{
    if (!frameptr) {
        frameptr = GetOneGlobal(REG_REG, "fp", 0);
    }
    if (COG_DATA) {
        return CogMemRef(frameptr, offset / LONG_SIZE);
    }
    return SizedHubMemRef(LONG_SIZE, frameptr, offset);
}
static Operand *
StackRef(int offset)
{
    ValidateStackptr();
    if (COG_DATA) {
        return CogMemRef(stackptr, offset / LONG_SIZE);
    }
    return SizedHubMemRef(LONG_SIZE, stackptr, offset);
}

static Operand *
ValidateDatBase(Module *P)
{
    AsmModData *PD = ModData(P);
    if (!PD->datbase) {
        PD->datlabel = NewOperand(IMM_HUB_LABEL, IdentifierGlobalName(P, "dat_"), 0);
        PD->datbase = NewImmediatePtr(NULL, PD->datlabel);
    }
    return PD->datbase;
}

static Operand *
LabelRef(IRList *irl, Symbol *sym)
{
    Operand *temp;
    Label *lab = (Label *)sym->val;
    Module *P = current;
    Operand *datbase = ValidateDatBase(P);
    // FIXME: should we check the label's flags to see if it's actually
    // declared in COG memory??
    temp = TypedHubMemRef(lab->type, datbase, (int)lab->hubval);
    return temp;
}

static Operand *
CompileIdentifierForFunc(IRList *irl, AST *expr, Function *func)
{
  Module *P = func->module;
  Symbol *sym;
  const char *name;
  AST *fcall;
  int stype;
  int size;
  
  if (expr->kind == AST_RESULT) {
      if (func->resultexpr && func->resultexpr->kind == AST_IDENTIFIER) {
          name = func->resultexpr->d.string;
      } else {
          name = "result";
      }
  } else if (expr->kind == AST_IDENTIFIER) {
      name = expr->d.string;
  } else {
      ERROR(expr, "Internal error, unexpected expression type for identifier");
      return NewImmediate(0);
  }
  sym = LookupSymbolInFunc(func, name);
  if (sym) {
      stype = sym->type;
      switch (stype) {
      case SYM_VARIABLE:
          if (sym->flags & SYMF_GLOBAL) {
              Operand *addr = NewImmediate(sym->offset);
              return NewOperand(LONG_REF, (char *)addr, 0);
          }
          ValidateObjbase();
          if (COG_DATA) {
              // COG memory
              size = ArrayTypeSize((AST *)sym->val);
              return GetSizedGlobal(REG_REG, IdentifierGlobalName(P, sym->name), 0, size);
          } else {
              // HUB memory
              return TypedHubMemRef((AST *)sym->val, objbase, (int)sym->offset);
          }
      case SYM_FUNCTION:
          fcall = NewAST(AST_FUNCCALL, expr, NULL);
          return CompileFunccall(irl, fcall);
      default:
          if (sym->type == SYM_RESERVED && !strcmp(sym->name, "result")) {
              /* do nothing, this is OK */
              stype = SYM_RESULT;
          } else {
              ERROR(expr, "Symbol %s is of a type not handled by PASM output yet", name);
          }
          /* fall through */
      case SYM_LOCALVAR:
      case SYM_PARAMETER:
      case SYM_RESULT:
          if (IS_STACK_CALL(func)) {
              if (stype == SYM_RESULT) {
                  return FrameRef(0);
              }
              if (stype == SYM_PARAMETER) {
                  return FrameRef(LONG_SIZE + sym->offset);
              }
              if (stype == SYM_LOCALVAR) {
                  return FrameRef(LONG_SIZE * (1+func->numparams) + sym->offset);
              }
          }
          if (stype == SYM_RESULT) {
              size = LONG_SIZE;
          } else {
              size = ArrayTypeSize((AST *)sym->val);
          }
          return GetSizedGlobal(REG_LOCAL, IdentifierLocalName(func, name), 0, size);
      case SYM_TEMPVAR:
          size = ArrayTypeSize((AST *)sym->val);
          return GetSizedGlobal(REG_LOCAL, IdentifierLocalName(func, name), 0, size);
      case SYM_LABEL:
          return LabelRef(irl, sym);
      }
  } else {
      ERROR_UNKNOWN_SYMBOL(expr);
  }
  return GetOneGlobal(REG_LOCAL, IdentifierLocalName(func, name), 0);
}

//
// utility function: get a pointer to where we should put the n'th argument
// for function f when we call it
//
static Operand *GetFunctionParameterForCall(IRList *irl, Function *func, int n)
{
    if (IS_FAST_CALL(func)) {
        char temp[20];
        sprintf(temp, "arg%d", n+1);
        return GetOneGlobal(REG_ARG, strdup(temp), 0);
    } else {
        AST *astlist = func->params;
        AST *ast;

        while (astlist != NULL) {
            ast = astlist->left;
            astlist = astlist->right;
            if (n == 0) {
                Symbol *sym = FindSymbol(&func->localsyms, ast->d.string);
                if (!sym) {
                    ERROR(NULL, "Internal error: symbol %s not found", ast->d.string);
                    return NewImmediate(0);
                }
                // we have to leave space for:
                // return address
                // old frame pointer
                // result value
                return StackRef(3*LONG_SIZE + sym->offset);
            }
            --n;
        }
        ERROR(NULL, "Too many parameters to function %s", func->name);
        return GetOneGlobal(REG_ARG, "dummyArg", 0);
    }
}

static void EmitPush(IRList *irl, Operand *src)
{
    IR *ir;
    ValidateStackptr();
    ir = EmitMove(irl, stacktop, src);
    if (gl_p2) {
        ir->srceffect = OPEFFECT_POSTINC;
    } else if (COG_DATA) {
        EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(1));
    } else {
        EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(4));
    }
}
static void EmitPop(IRList *irl, Operand *src)
{
    ValidateStackptr();
    IR *ir;
    if (!gl_p2) {
        if (COG_DATA) {
            EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(1));
        } else {
            EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(4));
        }
    }
    ir = EmitMove(irl, src, stacktop);
    if (gl_p2) {
        ir->srceffect = OPEFFECT_PREDEC;
    }
}

//
// the size of local variables
//
static int LocalSize(Function *func)
{
    return LONG_SIZE * (func->numlocals + func->numparams + 1);
}

//
// the function Prolog and Epilog are always output, and do things
// like stack management and variable setup
//

static void EmitFunctionProlog(IRList *irl, Function *func)
{
    int n = 0;
    AST *astlist;
    AST *ast;
    Operand *src;
    Operand *dst;

    if (IS_FAST_CALL(func)) {
        //
        // move parameters into local registers
        //
        astlist = func->params;
        while (astlist != NULL) {
            ast = astlist->left;
            astlist = astlist->right;

            src = GetFunctionParameterForCall(irl, func, n++);
            dst = CompileIdentifierForFunc(irl, ast, func);
            EmitMove(irl, dst, src);
        }
    }
}

//
// pop a list of variables, in reverse order
// the lists are one way, so we make a local copy in revstack and
// pop backwards from there
//
#define MAX_LOCAL_STACK 1024
static void PopList(IRList *irl, OperandList *list, Function *func)
{
    Operand *revstack[MAX_LOCAL_STACK];
    Operand *dst;
    int n = 0;

    for (; list; list = list->next) {
        dst = list->op;
        revstack[n++] = dst;
        if (n == MAX_LOCAL_STACK) {
            ERROR(func->locals, "Stack needed for function %s is too big", func->name);
            return;
        }
    }
    // now pop in reverse order
    while (n > 0) {
        --n;
        dst = revstack[n];
        EmitPop(irl, dst);
    }
}

//
// WARNING: most of the stuff placed here will be skipped over
// by any return statement!!
//
static void EmitFunctionEpilog(IRList *irl, Function *func)
{
    FreeTempRegisters(irl, 0);
}

static void
GetLocalRegsUsed(OperandList **list, IRList *irl)
{
    IR *ir;
    for (ir = irl->head; ir; ir = ir->next) {
        if (IsDummy(ir)) continue;
        if (ir->dst && IsLocal(ir->dst)) {
            AppendOperandUnique(list, ir->dst);
        }
        if (ir->src && IsLocal(ir->src)) {
            AppendOperandUnique(list, ir->src);
        }
    }
}

static bool
NeedToSaveLocals(Function *func)
{
    return func->is_recursive;
}

//
// the header/footer are code that should only be emitted for
// non-inline function invocations, at the beginning and
// end of the function; they contain the labels and
// ret instruction, for example
//
static void EmitFunctionHeader(IRList *irl, Function *func)
{
    OperandList *oplist;

    // earlier we put the appropriate comments into func->irheader
    // copy them out now
    //EmitDebugComment(irl, func->decl);
    AppendIRList(irl, &FuncData(func)->irheader);

    if (gl_output == OUTPUT_COGSPIN && FuncData(func)->asmaltname) {
        // insert a dummy function header that just changes the return address
        EmitLabel(irl, FuncData(func)->asmaltname);
        if (!gl_p2) {
            EmitOp2(irl, OPC_MOVS, FuncData(func)->asmretname, linkreg);
        }
    }
    
    // now the function label
    EmitLabel(irl, FuncData(func)->asmname);

    // set up frame pointer for stack call functions
    if (IS_STACK_CALL(func)) {
        int localsize;
        //
        // adjust stack as necessary
        //
        if (!frameptr) {
            frameptr = GetOneGlobal(REG_REG, "fp", 0);
        }
        EmitPush(irl, frameptr);
        EmitMove(irl, frameptr, stackptr);
        localsize = LocalSize(func);
        if (COG_DATA) {
            EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(localsize / LONG_SIZE));
        } else {
            EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(localsize));
        }
    }

    //
    // if recursive function, push all local registers
    // we also have to push any local temporary registers that
    // are used in the body
    // FIXME: can probably optimize this to skip temporaries that
    // are used only after the first call
    //
    
    if (NeedToSaveLocals(func)) {
        GetLocalRegsUsed(&FuncData(func)->saveregs, FuncIRL(func));
        // push scratch registers that need preserving
        for (oplist = FuncData(func)->saveregs; oplist; oplist = oplist->next) {
            EmitPush(irl, oplist->op);
        }
        // push return address, if we are in cog mode
        if (func->cog_code && !gl_p2) {
            EmitPush(irl, FuncData(func)->asmretname);
        }
    }
}

static void EmitFunctionFooter(IRList *irl, Function *func)
{
    if (FuncData(func)->asmreturnlabel != FuncData(func)->asmretname) {
        EmitLabel(irl, FuncData(func)->asmreturnlabel);
    }
    if (NeedToSaveLocals(func)) {
        // pop return address
        // do this here to avoid a hardware pipeline hazard:
        // we need at least 1 instruction between the pop
        // and the actual return
        if (func->cog_code && !gl_p2) {
            EmitPop(irl, FuncData(func)->asmretname);
        }
        // pop off all local variables
        PopList(irl, FuncData(func)->saveregs, func);
    }
    if (IS_STACK_CALL(func)) {
        EmitMove(irl, stackptr, frameptr);
        EmitPop(irl, frameptr);
    }
    EmitLabel(irl, FuncData(func)->asmretname);
    EmitOp0(irl, OPC_RET);
}

Operand *
CompileIdentifier(IRList *irl, AST *expr)
{
    Symbol *sym;

    if (expr->kind == AST_IDENTIFIER) {
        sym = LookupSymbol(expr->d.string);
        if (sym && (sym->type == SYM_CONSTANT || sym->type == SYM_FLOAT_CONSTANT)) {
            AST *symexpr = (AST *)sym->val;
            int val = EvalConstExpr(symexpr);
            if (val >= 0 && val < 512) {
                // FIXME: it would be nice to use sym->name as a symbolic
                // name for the constant, but this causes problems
                // with visibility in subobjects
                return NewOperand(IMM_INT, "" /*sym->name*/, val);
            } else {
                return NewImmediate(val);
            }
        }
    }
    return CompileIdentifierForFunc(irl, expr, curfunc);
}

Operand *
CompileHWReg(IRList *irl, AST *expr)
{
  HwReg *hw = (HwReg *)expr->d.ptr;
  return GetOneGlobal(REG_HW, hw->cname, 0);
}

static int isPowerOf2(unsigned x)
{
    return (x & (x-1)) == 0;
}

//
// Decompose val into a sequence of a shift, add/sub
// returns 0 if failure, 1 if success
// sets shifts[0] to final shift
// shifts[1] to +1 for add, -1 for sub, 0 if done
// shifts[2] to initial shift
//
static int DecomposeBits(unsigned val, int *shifts)
{
    int shift = 0;
    
    while (val != 0) {
        if (val & 1) {
            break;
        }
        shift++;
        val = val >> 1;
    }
    shifts[0] = shift;
    if (val == 1) {
        // we had a power of 2
        shifts[1] = 0;
        return 1;
    }
    // OK, can the new val itself be decomposed?
    if (isPowerOf2(val-1)) {
        shifts[1] = +1;
        return DecomposeBits(val-1, &shifts[2]);
    } else if (isPowerOf2(val+1)) {
        shifts[1] = -1;
        return DecomposeBits(val+1, &shifts[2]);
    } else {
        return 0;
    }
}

static int g_NeedMulHi = 0;

static Operand *
CompileMul(IRList *irl, AST *expr, int gethi, Operand *dest)
{
    Operand *lhs = CompileExpression(irl, expr->left, NULL);
    Operand *rhs = CompileExpression(irl, expr->right, NULL);
    Operand *temp = dest ? dest : NewFunctionTempRegister();

    g_NeedMulHi |= gethi;
    
    // if lhs is constant, swap left and right
    if (lhs->kind == IMM_INT) {
        Operand *swap = lhs;
        lhs = rhs;
        rhs = swap;
    }
    // check for multiply by constants
    if (rhs->kind == IMM_INT && rhs->val >= 0 && gethi == 0) {
        int shifts[4];
        int val = rhs->val;

        if (val == 0) {
            EmitMove(irl, temp, rhs); // rhs == 0
            return temp;
        }
        if (val == 1) {
            EmitMove(irl, temp, lhs);
            return temp;
        }
        lhs = Dereference(irl, lhs);
        // see if we can emit a sequence of shift and add/sub
        if (DecomposeBits(val, shifts)) {
            EmitMove(irl, temp, lhs);
            if (shifts[1] == 0) {
                EmitOp2(irl, OPC_SHL, temp, NewImmediate(shifts[0]));
                return temp;
            } else {
                EmitOp2(irl, OPC_SHL, temp, NewImmediate(shifts[2]));
            }
            if (shifts[1] > 0) {              
                EmitOp2(irl, OPC_ADD, temp, lhs);
            } else {
                EmitOp2(irl, OPC_SUB, temp, lhs);
            }
            EmitOp2(irl, OPC_SHL, temp, NewImmediate(shifts[0]));
            return temp;
        }
    }
    if (!mulfunc) {
        mulfunc = NewOperand(IMM_COG_LABEL, "multiply_", 0);
        muldiva = GetOneGlobal(REG_ARG, "muldiva_", 0);
        muldivb = GetOneGlobal(REG_ARG, "muldivb_", 0);
    }
    EmitMove(irl, muldiva, lhs);
    EmitMove(irl, muldivb, rhs);
    EmitOp1(irl, OPC_CALL, mulfunc);
    if (gethi) {
        EmitMove(irl, temp, muldivb);
    } else {
        EmitMove(irl, temp, muldiva);
    }
    return temp;
}

#define FAST_IMMEDIATE_DIVIDES

static Operand *
CompileDiv(IRList *irl, AST *expr, int getmod, Operand *dest)
{
  Operand *lhs = CompileExpression(irl, expr->left, NULL);
  Operand *rhs = CompileExpression(irl, expr->right, NULL);
  Operand *temp = dest ? dest : NewFunctionTempRegister();

#ifdef FAST_IMMEDIATE_DIVIDES
  if (rhs->kind == IMM_INT && rhs->val > 0 && isPowerOf2(rhs->val)) {
      IR *ir;
      int val = rhs->val;
      
      if (val == 1) {
          EmitMove(irl, temp, lhs);
          return temp;
      }

      lhs = Dereference(irl, lhs);
      ir = EmitOp2(irl, OPC_ABS, temp, lhs);
      ir->flags |= FLAG_WC; // carry will have the original sign bit
      if (getmod) {
          EmitOp2(irl, OPC_AND, temp, NewImmediate(val-1));
      } else {
          int shift = 0;
          val = val >>1;
          while (val != 0) {
              shift++;
              val = val >> 1;
          }
          EmitOp2(irl, OPC_SHR, temp, NewImmediate(shift));
      }
      ir = EmitOp2(irl, OPC_NEG, temp, temp);
      ir->cond = COND_LT; // only do the negate if x < 0
      return temp;
  }
#endif
  
  if (!divfunc) {
    divfunc = NewOperand(IMM_COG_LABEL, "divide_", 0);
    muldiva = GetOneGlobal(REG_ARG, "muldiva_", 0);
    muldivb = GetOneGlobal(REG_ARG, "muldivb_", 0);
  }
  EmitMove(irl, muldiva, lhs);
  EmitMove(irl, muldivb, rhs);
  EmitOp1(irl, OPC_CALL, divfunc);
  if (getmod) {
      EmitMove(irl, temp, muldiva);
  } else {
      EmitMove(irl, temp, muldivb);
  }
  return temp;
}

static IRCond
CondFromExpr(int kind)
{
  switch (kind) {
  case K_NE:
    return COND_NE;
  case K_EQ:
    return COND_EQ;
  case K_GE:
    return COND_GE;
  case K_LE:
    return COND_LE;
  case '<':
    return COND_LT;
  case '>':
    return COND_GT;
  default:
    break;
  }
  ERROR(NULL, "Unknown boolean operator");
  return COND_FALSE;
}

/*
 * change condition for when the two sides of a comparison
 * are flipped (e.g. a<b becomes b>a)
 */
static IRCond
FlipSides(IRCond cond)
{
  switch (cond) {
  case COND_LT:
    return COND_GT;
  case COND_GT:
    return COND_LT;
  case COND_LE:
    return COND_GE;
  case COND_GE:
    return COND_LE;
  default:
    return cond;
  }
}
/*
 * invert a condition (e.g. EQ -> NE, LT -> GE
 * note that the conditions are declared in a
 * particular order to make this easier
 */
IRCond
InvertCond(IRCond v)
{
  v = (IRCond)(1 ^ (unsigned)v);
  return v;
}

static IRCond
CompileBasicBoolExpression(IRList *irl, AST *expr)
{
  IRCond cond;
  Operand *lhs;
  Operand *rhs;
  int opkind;
  IR *ir;
  int flags;

  if (expr->kind == AST_OPERATOR) {
    opkind = (int)expr->d.ival;
  } else {
    opkind = -1;
  }
  switch(opkind) {
  case K_NE:
  case K_EQ:
  case '<':
  case '>':
  case K_LE:
  case K_GE:
    cond = CondFromExpr(opkind);
    lhs = CompileExpression(irl, expr->left, NULL);
    rhs = CompileExpression(irl, expr->right, NULL);
    break;
  default:
    cond = COND_NE;
    lhs = CompileExpression(irl, expr, NULL);
    rhs = NewOperand(IMM_INT, "", 0);
    break;
  }
  /* emit a compare operator */
  /* note that lhs cannot be a constant */
  if (lhs && lhs->kind == IMM_INT) {
    Operand *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
    cond = FlipSides(cond);
  }
  switch (cond) {
  case COND_NE:
  case COND_EQ:
    flags = FLAG_WZ;
    break;
  default:
    flags = FLAG_WZ|FLAG_WC;
    break;
  }
  lhs = Dereference(irl, lhs);
  rhs = Dereference(irl, rhs);
  ir = EmitOp2(irl, OPC_CMPS, lhs, rhs);
  ir->flags |= flags;
  return cond;
}

void
CompileBoolBranches(IRList *irl, AST *expr, Operand *truedest, Operand *falsedest)
{
    IRCond cond;
    int opkind;
    Operand *dummylabel = NULL;
    IR *ir;
    
    if (expr->kind == AST_ISBETWEEN) {
        Operand *lo, *hi;
        Operand *val;
        Operand *tmplo, *tmphi;
        Operand *tmp2lo, *tmp2hi;
        val = CompileExpression(irl, expr->left, NULL);
        val = Dereference(irl, val);
        lo = CompileExpression(irl, expr->right->left, NULL);
        hi = CompileExpression(irl, expr->right->right, NULL);
        tmplo = NewFunctionTempRegister();
        tmphi = NewFunctionTempRegister();
        tmp2lo = NewFunctionTempRegister();
        tmp2hi = NewFunctionTempRegister();
        EmitMove(irl, tmplo, lo);
        EmitMove(irl, tmp2lo, tmplo);
        EmitMove(irl, tmphi, hi);
        EmitMove(irl, tmp2hi, tmphi);
        EmitOp2(irl, OPC_MAXS, tmplo, tmp2hi); // now tmplo really is the smallest
        EmitOp2(irl, OPC_MINS, tmphi, tmp2lo); // and tmphi really is highest
        ir = EmitOp2(irl, OPC_CMPS, tmplo, val);
        ir->flags |= FLAG_WZ|FLAG_WC;
        ir = EmitOp2(irl, OPC_CMPS, val, tmphi);
        ir->flags |= FLAG_WZ|FLAG_WC;
        ir->cond = COND_LE;
        if (truedest) {
            EmitJump(irl, COND_LE, truedest);
        }
        if (falsedest) {
            EmitJump(irl, COND_GT, falsedest);
        }
        return;
    }
    
    if (expr->kind == AST_OPERATOR) {
        opkind = (int)expr->d.ival;
    } else {
        opkind = -1;
    }

    switch (opkind) {
    case K_NOT:
        CompileBoolBranches(irl, expr->right, falsedest, truedest);
        break;
    case K_AND:
        if (!falsedest) {
            dummylabel = NewCodeLabel();
            falsedest = dummylabel;
        }
        CompileBoolBranches(irl, expr->left, NULL, falsedest);
        CompileBoolBranches(irl, expr->right, truedest, falsedest);
        if (dummylabel) {
            EmitLabel(irl, dummylabel);
        }
        break;
    case K_OR:
        if (!truedest) {
            dummylabel = NewCodeLabel();
            truedest = dummylabel;
        }
        CompileBoolBranches(irl, expr->left, truedest, NULL);
        CompileBoolBranches(irl, expr->right, truedest, falsedest);
        if (dummylabel) {
            EmitLabel(irl, dummylabel);
        }
        break;
    default:
        if (IsConstExpr(expr)) {
            cond = EvalConstExpr(expr) == 0 ? COND_FALSE : COND_TRUE;
        } else {
            cond = CompileBasicBoolExpression(irl, expr);
        }
        if (truedest) {
            EmitJump(irl, cond, truedest);
        }
        if (falsedest) {
            EmitJump(irl, InvertCond(cond), falsedest);
        }
        break;
    }
}

static IROpcode
OpcFromOp(int op)
{
  switch(op) {
  case '+':
    return OPC_ADD;
  case '-':
    return OPC_SUB;
  case '&':
    return OPC_AND;
  case '|':
    return OPC_OR;
  case '^':
    return OPC_XOR;
  case K_SHL:
    return OPC_SHL;
  case K_SAR:
    return OPC_SAR;
  case K_SHR:
    return OPC_SHR;
  case K_NEGATE:
    return OPC_NEG;
  case K_ABS:
    return OPC_ABS;
  case K_ROTL:
      return OPC_ROL;
  case K_ROTR:
      return OPC_ROR;
//  case K_REV:
//      return gl_p2 ? OPC_REV_P2 : OPC_REV_P1;
  case K_LIMITMIN:
      return OPC_MINS;
  case K_LIMITMAX:
      return OPC_MAXS;
  default:
    ERROR(NULL, "Unsupported operator %d", op);
    return OPC_UNKNOWN;
  }
}
static Operand *
Dereference(IRList *irl, Operand *op)
{
    if (IsMemRef(op)) {
        Operand *temp = NewFunctionTempRegister();
        EmitMove(irl, temp, op);
        return temp;
    }
    return op;
}

static Operand *
CompileBasicOperator(IRList *irl, AST *expr, Operand *dest)
{
  int op = (int)expr->d.ival;
  AST *lhs = expr->left;
  AST *rhs = expr->right;
  Operand *left;
  Operand *right;
  Operand *temp = NewFunctionTempRegister(); // FIXME? was dest ? dest : NewFunctionTempRegister(), but that fails for a = b - a
  IR *ir;

  switch(op) {
  case K_REV:
      // we actually want rev lhs, 32 - rhs
      rhs = AstOperator('-', AstInteger(32), rhs);
      if (gl_p2) {
          // reverse, then shift right
          left = CompileExpression(irl, lhs, NULL);
          right = CompileExpression(irl, rhs, NULL);
          EmitOp2(irl, OPC_REV_P2, temp, left); // reverse the bits
          EmitOp2(irl, OPC_SHR, temp, right);
          return temp;
      } else {
          left = CompileExpression(irl, lhs, temp);
          right = CompileExpression(irl, rhs, NULL);
          EmitMove(irl, temp, left);
          right = Dereference(irl, right);
          EmitOp2(irl, OPC_REV_P1, temp, right);
          return temp;
      }
  case '-':
  case K_SHL:
  case K_SHR:
  case K_SAR:
  case K_ROTL:
  case K_ROTR:
  case K_LIMITMIN:
  case K_LIMITMAX:
      left = CompileExpression(irl, lhs, temp);
      right = CompileExpression(irl, rhs, NULL);
      EmitMove(irl, temp, left);
      right = Dereference(irl, right);
      EmitOp2(irl, OpcFromOp(op), temp, right);
      return temp;
    // commutative ops 
  case '+':
  case '^':
  case '&':
  case '|':
      // there might be something different we could do about
      // commutative operations, but for now handle them the same
      left = CompileExpression(irl, lhs, temp);
      right = CompileExpression(irl, rhs, NULL);
      EmitMove(irl, temp, left);
      right = Dereference(irl, right);
      EmitOp2(irl, OpcFromOp(op), temp, right);
      return temp;
  case K_NEGATE:
  case K_ABS:
      right = CompileExpression(irl, rhs, temp);
      right = Dereference(irl, right);
      EmitOp2(irl, OpcFromOp(op), temp, right);
      return temp;
  case K_BIT_NOT:
      right = CompileExpression(irl, rhs, temp);
      EmitMove(irl, temp, right);
      left = NewImmediate(-1);
      EmitOp2(irl, OPC_XOR, temp, left);
      return temp;
  case K_ENCODE:
      right = CompileExpression(irl, rhs, temp);
      left = NewFunctionTempRegister();
      EmitMove(irl, left, right);
      EmitMove(irl, temp, NewImmediate(32));
      right = NewCodeLabel();
      EmitLabel(irl, right);
      ir = EmitOp2(irl, OPC_SHL, left, NewImmediate(1));
      ir->flags |= FLAG_WC;
      ir = EmitOp2(irl, OPC_DJNZ, temp, right);
      ir->cond = COND_NC;
      return temp;
  case K_DECODE:
      ERROR(rhs, "Internal error: decode operators should have been handled in spin transormations");
      return NewImmediate(0);

  case K_NOT:
  case K_AND:
  case K_OR:
  case K_EQ:
  case K_NE:
  case K_LE:
  case K_GE:
  case '<':
  case '>':
  {
      Operand *zero = NewImmediate(0);
      Operand *skiplabel = NewCodeLabel();
      EmitMove(irl, temp, zero);
      CompileBoolBranches(irl, expr, NULL, skiplabel);
      EmitOp2(irl, OPC_XOR, temp, NewImmediate(-1));
      EmitLabel(irl, skiplabel);
      return temp;
  }
  case K_SQRT:
  {
      AST *fcall;
      AstReportAs(expr);
      fcall = NewAST(AST_FUNCCALL, AstIdentifier("_sqrt"),
                     NewAST(AST_EXPRLIST, expr->right, NULL));
      return CompileFunccall(irl, fcall);
  }
  case '?':
  {
      AST *fcall;
      AST *var;
      const char *fname;
      AstReportAs(expr);
      if (expr->left) {
          fname = "_lfsr_forward";
          var = expr->left;
      } else {
          fname = "_lfsr_backward";
          var = expr->right;
      }
      fcall = NewAST(AST_FUNCCALL, AstIdentifier(fname),
                     NewAST(AST_EXPRLIST, var, NULL));
      fcall = AstAssign(var, fcall);
      return CompileExpression(irl, fcall, NULL);
  }
  default:
    ERROR(lhs, "Unsupported operator %d", op);
    return NewImmediate(0);
  }
}

static Operand *
CompileOperator(IRList *irl, AST *expr, Operand *dest)
{
    int op = (int)expr->d.ival;
    switch (op) {
    case K_INCREMENT:
    case K_DECREMENT:
    {
        AST *addone;
        Operand *lhs;
        Operand *temp = NULL;
        int opc = (op == K_INCREMENT) ? '+' : '-';
        if (expr->left) {  /* x++ */
            addone = AstAssign(expr->left, AstOperator(opc, expr->left, AstInteger(1)));
            temp = NewFunctionTempRegister();
            lhs = CompileExpression(irl, expr->left, NULL);
            EmitMove(irl, temp, lhs);
            CompileExpression(irl, addone, NULL);
            return temp;
        } else {
            addone = AstAssign(expr->right, AstOperator(opc, expr->right, AstInteger(1)));
        }
        return CompileExpression(irl, addone, NULL);
    }
    case '*':
        return CompileMul(irl, expr, 0, dest);
    case K_HIGHMULT:
        return CompileMul(irl, expr, 1, dest);
    case '/':
        return CompileDiv(irl, expr, 0, dest);
    case K_MODULUS:
        return CompileDiv(irl, expr, 1, dest);
    case '&':
        if (expr->right->kind == AST_OPERATOR && expr->right->d.ival == K_BIT_NOT) {
            Operand *temp = dest ? dest : NewFunctionTempRegister();
            Operand *lhs = CompileExpression(irl, expr->left, temp);
            Operand *rhs = CompileExpression(irl, expr->right->right, NULL);
            EmitMove(irl, temp, lhs);
            rhs = Dereference(irl, rhs);
            EmitOp2(irl, OPC_ANDN, temp, rhs);
            return temp;
        }
        return CompileBasicOperator(irl, expr, dest);
    default:
        return CompileBasicOperator(irl, expr, dest);
    }
}

static OperandList quitstack;

static void
PushQuitNext(Operand *q, Operand *n)
{
  OperandList *qholder = (OperandList *)malloc(sizeof(OperandList));
  OperandList *nholder = (OperandList *)malloc(sizeof(OperandList));
  qholder->op = quitlabel;
  nholder->op = nextlabel;
  qholder->next = nholder;
  nholder->next = quitstack.next;
  quitstack.next = qholder;

  quitlabel = q;
  nextlabel = n;
}

static void
PopQuitNext()
{
  OperandList *ql, *nl;

  ql = quitstack.next;
  if (!ql || !ql->next) {
    ERROR(NULL, "Internal error: empty loop stack");
    return;
  }
  nl = ql->next;
  quitstack.next = nl->next;
  quitlabel = ql->op;
  nextlabel = nl->op;
  free(nl);
  free(ql);
}

/* compiles a list of expressions; returns the last one
 * tolist is a list of where to store the expressions
 * (may be NULL if we don't care about saving them)
 */
static OperandList *
CompileExprList(IRList *irl, AST *fromlist)
{
  AST *from;
  Operand *opfrom = NULL;
  Operand *opto = NULL;
  OperandList *ret = NULL;

  while (fromlist) {
    from = fromlist->left;
    fromlist = fromlist->right;

    opfrom = CompileExpression(irl, from, NULL);
    opto = NewFunctionTempRegister();
    if (!opfrom) break;
    EmitMove(irl, opto, opfrom);
    AppendOperand(&ret, opto);
  }
  return ret;
}

static void
EmitParameterList(IRList *irl, OperandList *oplist, Function *func)
{
    Operand *op;
    Operand *dst;
    int n = 0;
  
    while (oplist != NULL) {
        op = oplist->op;
        oplist = oplist->next;

        dst = GetFunctionParameterForCall(irl, func, n++);
        EmitMove(irl, dst, op);
    }
}

static Operand *
CompileFunccall(IRList *irl, AST *expr)
{
  Operand *result;
  Symbol *sym;
  Function *func;
  AST *params;
  Symbol *objsym;
  OperandList *temp;
  Operand *reg;
  IR *ir;
  int n;

  /* compile the function operands */
  objsym = NULL;
  sym = FindFuncSymbol(expr, NULL, &objsym);
  if (!sym || sym->type != SYM_FUNCTION) {
    ERROR(expr, "expected function symbol");
    return NewImmediate(0);
  }
  func = (Function *)sym->val;
  params = expr->right;
  temp = CompileExprList(irl, params);

  /* now copy the parameters into place (have to do this in case there are
     function calls within the parameters)
   */
  EmitParameterList(irl, temp, func);

  /* emit the call */
  if (objsym) {
      Operand *offset = NULL;
      Operand *absaddr = NULL;
      Operand *saveobj = NULL;
      AST *call = expr->left;
      if (call->kind == AST_METHODREF) {
          call = call->left;
      } else {
          ERROR(call, "Internal error: expected method reference");
      }
      if (call->kind == AST_ARRAYREF) {
          if (IsArraySymbol(objsym)) {
              // add the index * sizeof(object) into the array offset
              Module *objModule = GetObjectPtr(objsym);
              AST *arrayderef = AstOperator('*', call->right, AstInteger(objModule->varsize));
              arrayderef = AstOperator('+', arrayderef, AstInteger(objsym->offset));
              offset = CompileExpression(irl, arrayderef, NULL);
          } else {
              absaddr = CompileExpression(irl, call->right, NULL);
          }
      } else {
          offset = NewImmediate(objsym->offset);
      }
      ValidateObjbase();
      if (offset) {
          EmitOp2(irl, OPC_ADD, objbase, offset);
      } else {
          saveobj = NewFunctionTempRegister();
          EmitMove(irl, saveobj, objbase);
          EmitMove(irl, objbase, absaddr);
      }
      ir = EmitOp1(irl, OPC_CALL, FuncData(func)->asmname);
      if (offset) {
          EmitOp2(irl, OPC_SUB, objbase, offset);
      } else {
          EmitMove(irl, objbase, saveobj);
      }
  } else {
      ir = EmitOp1(irl, OPC_CALL, FuncData(func)->asmname);
  }
  ir->aux = (void *)func; // remember the function for optimization purposes
  func->callSites |= 1;      // internal functions may not have been marked earlier
  if (IS_FAST_CALL(func)) {
      /* mark parameters as dead */
      n = AstListLen(params);
      while (n > 0) {
          --n;
          reg = GetFunctionParameterForCall(irl, func, n);
          EmitOp1(irl, OPC_DEAD, reg);
      }
  }

  /* now get the result */
  /* NOTE: we cannot assume this is unchanged over future calls,
     so save it in a temp register
  */
  result = GetOneGlobal(REG_REG, "result1", 0);
  reg = NewFunctionTempRegister();
  EmitMove(irl, reg, result);
  return reg;
}

static bool
IsCogMem(Operand *addr)
{
    switch (addr->kind) {
    case IMM_COG_LABEL:
    case COG_REF:
    case REG_HW:
    case REG_REG:
    case REG_LOCAL:
        return true;
    case IMM_HUB_LABEL:
    case IMM_STRING:
    case IMM_INT:
    case LONG_REF:
    case WORD_REF:
    case BYTE_REF:
        return false;
    default:
        return COG_DATA;
    }
}

static Operand *
CompileHubref(IRList *irl, AST *expr)
{
    Operand *addr = CompileExpression(irl, expr->right, NULL);
    AST *type = expr->left;

    addr = Dereference(irl, addr);
    return TypedHubMemRef(type, addr, 0);
}

static Operand *
ApplyArrayIndex(IRList *irl, Operand *base, Operand *offset)
{
    Operand *basereg;
    Operand *newbase;
    Operand *temp;
    int idx;
    int siz;
    int shift;

    // check for COG memory references
    if (!IsMemRef(base) && IsCogMem(base)) {
        Operand *addr;
        switch(base->kind) {
        case REG_REG:
        case REG_LOCAL:
        case REG_ARG:
        case REG_HW:
            base->used = 1;
            addr = NewOperand(IMM_COG_LABEL, base->name, 0);
            base = CogMemRef(addr, 0);
            break;
        default:
            break;
        }
    }
    if (!IsMemRef(base)) {
        ERROR(NULL, "Array does not reference memory");
        return base;
    }
    switch (base->kind) {
    case LONG_REF:
        siz = 4;
        shift = 2;
        break;
    case WORD_REF:
        siz = 2;
        shift = 1;
        break;
    case BYTE_REF:
        siz = 1;
        shift = 0;
        break;
    case COG_REF:
        siz = 1;
        shift = 0;
        break;
    default:
        ERROR(NULL, "Bad size in array reference");
        siz = 1;
	shift = 0;
        break;
    }
          
    basereg = (Operand *)base->name;
    if (offset->kind == IMM_INT) {
        idx = offset->val * siz;
        if (idx == 0) {
            return base;
        }
        newbase = NewOperand(base->kind, (char *)basereg, idx + base->val);
        return newbase;
    }
    temp = NewFunctionTempRegister();
    EmitMove(irl, temp, offset);
    if (shift) {
        EmitOp2(irl, OPC_SHL, temp, NewImmediate(shift));
    }
    newbase = GetLea(irl, base);
    EmitOp2(irl, OPC_ADD, temp, newbase);
    return NewOperand(base->kind, (char *)temp, 0);
}

static Operand *
CompileCondResult(IRList *irl, AST *expr)
{
    AST *cond = expr->left;
    AST *ifpart = expr->right->left;
    AST *elsepart = expr->right->right;
    Operand *r = NewFunctionTempRegister();
    Operand *tmp;
    Operand *label1 = NewCodeLabel();
    Operand *label2 = NewCodeLabel();

    CompileBoolBranches(irl, cond, NULL, label1);
    /* the default is the IF part */
    tmp = CompileExpression(irl, ifpart, NULL);
    EmitMove(irl, r, tmp);
    EmitJump(irl, COND_TRUE, label2);

    /* here is the ELSE part */
    EmitLabel(irl, label1);
    tmp = CompileExpression(irl, elsepart, NULL);
    EmitMove(irl, r, tmp);

    /* all done */
    EmitLabel(irl, label2);

    return r;
}

//
// compile a masked fetch:
//   (a & mask) | val
// here a is expr->left, mask = expr->right->left, val = expr->right->right
// note that this is marked volatile to prevent later optimization
// useful for maniuplating groups of bits in OUTA and DIRA
//
static Operand *
CompileMaskMove(IRList *irl, AST *expr)
{
    AST *destast = expr->left;
    AST *maskast = expr->right->left;
    AST *valast = expr->right->right;
    Operand *tmp = NewFunctionTempRegister();
    Operand *dest;
    Operand *val = CompileExpression(irl, valast, NULL);
    Operand *mask = CompileExpression(irl, maskast, NULL);
    IR *ir;

    switch(destast->kind) {
    case AST_IDENTIFIER:
        dest = CompileIdentifier(irl, destast);
        break;
    case AST_HWREG:
        dest = CompileHWReg(irl, destast);
        break;
    default:
        ERROR(expr, "Internal error, bad value  in MaskMove");
        return tmp;
    }
    ir = EmitMove(irl, tmp, dest);
    ir->flags |= FLAG_KEEP_INSTR;
    ir = EmitOp2(irl, OPC_AND, tmp, mask);
    ir->flags |= FLAG_KEEP_INSTR;
    ir = EmitOp2(irl, OPC_OR, tmp, val);
    ir->flags |= FLAG_KEEP_INSTR;
    return tmp;
}

//
// get the effective address of "src"
// this may require us to emit instructions
//
static Operand *
GetLea(IRList *irl, Operand *src)
{
    // check for COG memory references
    if (!IsMemRef(src) && IsCogMem(src)) {
        Operand *addr;
        switch(src->kind) {
        case IMM_COG_LABEL:
            return src;
        case REG_REG:
        case REG_LOCAL:
        case REG_ARG:
        case REG_HW:
            src->used = 1;
            addr = NewOperand(IMM_COG_LABEL, src->name, 0);
            return addr;
        default:
            break;
        }
    }
    if (IsMemRef(src)) {
        Operand *dst = NewFunctionTempRegister();
        int off = src->val;
        src = (Operand *)src->name;
        if (off) {
            EmitAddSub(irl, src, off);
        }
        EmitMove(irl, dst, src);
        if (off) {
            EmitAddSub(irl, src, -off);
        }
        return dst;
    } else {
        ERROR(NULL, "Load Effective Address on a non-memory reference");
        return NULL;
    }
}

//
// compile a coginit expression
// if we have a spin function call inside it then this
// is mildly complicated
//
static Operand *
CompileCoginit(IRList *irl, AST *expr)
{
    AST *funccall;
    AST *params = expr->left;
    Function *remote;
    
    if ( (remote = IsSpinCoginit(expr)) != NULL) {
        AST *exprlist;
        AST *func;
        AST *stack;
        AST *cogid;
        AST *kernel;
        Operand *newstackptr;
        Operand *newstacktop;
        Operand *const4;
        Operand *funcptr;
        OperandList *plist;

        if (!kernelptr) {
            kernelptr = NewImmediatePtr("entryptr__", NewOperand(IMM_HUB_LABEL, ENTRYNAME, 0));
        }
        kernel = NewAST(AST_OPERAND, NULL, NULL);
        kernel->d.ptr = (void *)kernelptr;
        
        if (!IS_STACK_CALL(remote)) {
            ERROR(expr, "Internal error: wrong calling convention for coginit");
            return NewImmediate(0);
        }
        if (gl_output == OUTPUT_COGSPIN) {
            ERROR(expr, "coginit/cognew of Spin objects is not permitted from COG code");
            return NewImmediate(0);
        }
        if (remote->cog_code) {
            ERROR(expr, "Coginit target must be in hub memory. Try compiling with --code=hub.");
            return NewImmediate(0);
        }
        if (remote->module != curfunc->module) {
            ERROR(expr, "Coginit/cognew across objects is not supported");
            return NewImmediate(0);
        }
        exprlist = expr->left;
        cogid = exprlist->left;
        exprlist = exprlist->right;
        func = exprlist->left;
        exprlist = exprlist->right;
        stack = exprlist->left;
        if (exprlist->right != NULL) {
            ERROR(expr, "Too many parameters to coginit/cognew");
        }
        
        // we have to build the call into the new stack
        if (func->kind == AST_IDENTIFIER) {
            params = NULL;
        } else if (func->kind == AST_FUNCCALL) {
            params = func->right;
        } else {
            ERROR(expr, "Internal error, expected function call");
            return NewImmediate(0);
        }
        ValidateObjbase();
        if (stack->kind != AST_ADDROF && stack->kind != AST_DATADDROF
            && stack->kind != AST_ABSADDROF)
        {
            WARNING(stack, "Normally the coginit stack parameter should be an address");
        }
        newstackptr = CompileExpression(irl, stack, NULL);
        if (COG_DATA) {
            newstacktop = CogMemRef(newstackptr, 0);
            const4 = NewImmediate(1);
        } else {
            newstacktop = SizedHubMemRef(LONG_SIZE, newstackptr, 0);
            const4 = NewImmediate(4);
        }
        EmitMove(irl, newstacktop, objbase);
        EmitOp2(irl, OPC_ADD, newstackptr, const4);
        // push the function to call
        funcptr = FuncData(remote)->asmname;
        EmitMove(irl, newstacktop, funcptr);
        EmitOp2(irl, OPC_ADD, newstackptr, const4);
        // provide space for frame pointer
        EmitMove(irl, newstacktop, NewImmediate(0));
        EmitOp2(irl, OPC_ADD, newstackptr, const4);
        // provide space for result
        EmitMove(irl, newstacktop, NewImmediate(0));
        EmitOp2(irl, OPC_ADD, newstackptr, const4);
        // now the parameters
        plist = CompileExprList(irl, params);
        while (plist) {
            EmitMove(irl, newstacktop, plist->op);
            EmitOp2(irl, OPC_ADD, newstackptr, const4);
            plist = plist->next;
        }
        // OK, new stack is all set up
        // now we need to call coginit(cogid, @entry, stack)
        params = NewAST( AST_EXPRLIST, cogid,
                         NewAST(AST_EXPRLIST, kernel,
                                NewAST(AST_EXPRLIST, stack, NULL)) );
    }
    funccall = AstIdentifier("_coginit");
    funccall = NewAST(AST_FUNCCALL, funccall, params);
    return CompileFunccall(irl, funccall);
}

//
// compile a lookup/lookdown
//
static Operand *
CompileLookupDown(IRList *irl, AST *expr)
{
    AST *funccall;
    AST *idx, *base;
    AST *params;
    AST *ev, *table;
    AST *len;
    AST *arrid;
    Operand *value;
    
    int popsize = 0;
    if (expr->kind == AST_LOOKDOWN) {
        funccall = AstIdentifier("_lookdown");
    } else {
        funccall = AstIdentifier("_lookup");
    }
    ev = expr->left;
    if (ev->kind != AST_LOOKEXPR) {
        ERROR(ev, "Internal error in lookup");
        return NewImmediate(0);
    }
    base = ev->left;
    idx = ev->right;
    table = expr->right;

    if (table->kind == AST_TEMPARRAYUSE) {
        len = table->right;
        arrid = NewAST(AST_ADDROF, table->left, NULL);
    } else if (table->kind == AST_EXPRLIST) {
        // FIXME?? ASSUMES STACK GROWS UP!!!
        int n;
        Operand *basereg = NewFunctionTempRegister();
        Operand *finalidx;
        
        /* NOTE!
           we have to evaluate the index before evaluating array elements
        */
        finalidx = CompileExpression(irl, idx, NULL);
        idx = NewAST(AST_OPERAND, NULL, NULL);
        idx->d.ptr = (void *)finalidx;
        
        ValidateStackptr();
        arrid = NewAST(AST_OPERAND, NULL, NULL);
        arrid->d.ptr = (void *)basereg;
        EmitMove(irl, basereg, stackptr);
        n = 0;
        while (table) {
            Operand *src = CompileExpression(irl, table->left, NULL);
            EmitPush(irl, src);
            n++;
            table = table->right;
        }
        len = AstInteger(n);
        popsize = n*LONG_SIZE;
    } else {
        ERROR(table, "Internal error in lookup code");
        return NewImmediate(0);
    }

    params = NewAST(AST_EXPRLIST, idx,
                    NewAST(AST_EXPRLIST, base,
                           NewAST(AST_EXPRLIST, arrid,
                                  NewAST(AST_EXPRLIST, len, NULL))));
    funccall = NewAST(AST_FUNCCALL, funccall, params);
    value = CompileFunccall(irl, funccall);
    if (popsize) {
        EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(popsize));
    }
    return value;
}

//
// get the address of an expression
//
static Operand *
GetAddressOf(IRList *irl, AST *expr)
{
    Operand *res;
    Operand *tmp;
    switch (expr->kind) {
    case AST_IDENTIFIER:
        res = CompileExpression(irl, expr, NULL);
        tmp = GetLea(irl, res);
        return tmp;
    case AST_ARRAYREF:
    {
      Operand *base;
      Operand *offset;

      if (!expr->right) {
          ERROR(expr, "Array ref with no index?");
          return NewOperand(REG_REG, "???", 0);
      }
      base = CompileExpression(irl, expr->left, NULL);
      offset = CompileExpression(irl, expr->right, NULL);
      res = ApplyArrayIndex(irl, base, offset);
      tmp = GetLea(irl, res);
      return tmp;
    }    
    default:
        ERROR(expr, "Cannot take address of expression");
        break;
    }
    return NewImmediate(-1);
}

static Operand *
CompileExpression(IRList *irl, AST *expr, Operand *dest)
{
  Operand *r;
  Operand *val;

  while (expr && expr->kind == AST_COMMENTEDNODE) {
    expr = expr->left;
  }
  if (!expr) return NULL;
  if (IsConstExpr(expr)) {
      switch (expr->kind) {
      case AST_IDENTIFIER:
          // leave symbolic constants alone
      case AST_ADDROF:
      case AST_ABSADDROF:
          // similarly for address expressions
          break;
      default:
          expr = FoldIfConst(expr);
          break;
      }
  }
  switch (expr->kind) {
  case AST_CONDRESULT:
      return CompileCondResult(irl, expr);
  case AST_MASKMOVE:
      return CompileMaskMove(irl, expr);
  case AST_CATCH:
  {
      Operand *tmp;
      Operand *labelskip, *labelend;
      IR *ir;
      
      ValidateAbortFuncs();
      // save current state on the stack
      tmp = NewFunctionTempRegister();
      labelskip = NewCodeLabel();
      labelend = NewCodeLabel();
      EmitMove(irl, arg1, stackptr);
      EmitOp2(irl, OPC_ADD, stackptr, NewImmediate(SETJMP_BUF_SIZE));
      EmitOp1(irl, OPC_CALL, catchfunc);
      ir = EmitOp2(irl, OPC_CMPS, abortcalled, NewImmediate(0));
      ir->flags |= FLAG_WZ;
      // if (abortcalled) {
      //   result := result1
      // } else {
      //   result := compile(expr->left)
      // }
      EmitJump(irl, COND_EQ, labelskip);
      EmitMove(irl, tmp, result1);
      EmitJump(irl, COND_TRUE, labelend);
      EmitLabel(irl, labelskip);
      EmitMove(irl, tmp, CompileExpression(irl, expr->left, NULL));
      EmitLabel(irl, labelend);
      EmitOp2(irl, OPC_SUB, stackptr, NewImmediate(SETJMP_BUF_SIZE));
      return tmp;
      break;
  }
  case AST_ISBETWEEN:
  {
      Operand *zero = NewImmediate(0);
      Operand *skiplabel = NewCodeLabel();
      Operand *temp = NewFunctionTempRegister();
      
      EmitMove(irl, temp, zero);
      CompileBoolBranches(irl, expr, NULL, skiplabel);
      EmitOp2(irl, OPC_XOR, temp, NewImmediate(-1));
      EmitLabel(irl, skiplabel);
      return temp;
  }
  case AST_SEQUENCE:
      r = CompileExpression(irl, expr->left, NULL);
      r = CompileExpression(irl, expr->right, NULL);
      return r;
  case AST_INTEGER:
  case AST_FLOAT:
    r = NewImmediate((int32_t)expr->d.ival);
    return r;
  case AST_RESULT:
  case AST_IDENTIFIER:
    r = CompileIdentifier(irl, expr);
    if (dest) {
        EmitMove(irl, dest, r);
        r = dest;
    }
    return r;
  case AST_HWREG:
    return CompileHWReg(irl, expr);
  case AST_OPERATOR:
      return CompileOperator(irl, expr, dest);
  case AST_FUNCCALL:
      return CompileFunccall(irl, expr);
  case AST_ASSIGN:
      if (expr->d.ival != K_ASSIGN) {
          ERROR(expr, "Internal error: asm code cannot handle assignment");
      }
      r = CompileExpression(irl, expr->left, NULL);
      if (IsRegister(r->kind)) {
          val = CompileExpression(irl, expr->right, r);
      } else {
          val = CompileExpression(irl, expr->right, NULL);
      }
      EmitMove(irl, r, val);
      return r;
  case AST_RANGEREF:
      return CompileExpression(irl, TransformRangeUse(expr), dest);
  case AST_STRING:
      if (strlen(expr->d.string) > 1)  {
            ERROR(expr, "string too long, expected a single character");
      }
      return NewImmediate(expr->d.string[0]);
  case AST_STRINGPTR:
      r = GetOneHub(STRING_DEF, NewTempLabelName(), (intptr_t)(expr->left));
      return NewImmediatePtr(NULL, r);
  case AST_ARRAYREF:
  {
      Operand *base;
      Operand *offset;

      if (!expr->right) {
          ERROR(expr, "Array ref with no index?");
          return NewOperand(REG_REG, "???", 0);
      }
      base = CompileExpression(irl, expr->left, NULL);
      offset = CompileExpression(irl, expr->right, NULL);
      return ApplyArrayIndex(irl, base, offset);
  }
  case AST_SPRREF:
  {
      Operand *base;

      if (!expr->left) {
          ERROR(expr, "SPR ref with no index?");
          return NewOperand(REG_REG, "???", 0);
      }
      base = CompileExpression(irl, expr->left, NULL);
      return CogMemRef(base, 0);
  }
  case AST_MEMREF:
    return CompileHubref(irl, expr);
  case AST_COGINIT:
    return CompileCoginit(irl, expr);
  case AST_ADDROF:
  case AST_ABSADDROF:
      return GetAddressOf(irl, expr->left);
  case AST_DATADDROF:
      /* this requires that we add an offset to the value we get */
  {
      Operand *temp = dest ? dest : NewFunctionTempRegister();
      Operand *val = CompileExpression(irl, expr->left, dest);
      Operand *datbase = ValidateDatBase(current);
      
      EmitMove(irl, temp, val);
      EmitOp2(irl, OPC_ADD, temp, datbase);
      return temp;
  }
  case AST_LOOKUP:
  case AST_LOOKDOWN:
      return CompileLookupDown(irl, expr);
  case AST_OPERAND:
      /* stashed away by other code */
      return (Operand *)expr->d.ptr;
  default:
    ERROR(expr, "Cannot handle expression yet");
    return NewOperand(REG_REG, "???", 0);
  }
}

static void CompileStatement(IRList *irl, AST *ast); /* forward declaration */

static void
CompileStatementList(IRList *irl, AST *ast)
{
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error: expected statement list, got %d",
                  ast->kind);
            return;
        }
        CompileStatement(irl, ast->left);
        ast = ast->right;
    }
}
static void EmitAddSub(IRList *irl, Operand *dst, int off)
{
    IROpcode opc  = OPC_ADD;
    Operand *imm;
    
    if (off < 0) {
        off = -off;
        opc = OPC_SUB;
    }
    imm = NewImmediate(off);
    EmitOp2(irl, opc, dst, imm);
}

static IR *EmitCogread(IRList *irl, Operand *dst, Operand *src)
{
    Operand *dstimm;
    if (!putcogreg) {
        putcogreg = NewOperand(IMM_COG_LABEL, "wrcog", 0);
    }
    dstimm = GetLea(irl, dst);
    EmitOp2(irl, OPC_MOVS, putcogreg, src);
    EmitOp2(irl, OPC_MOVD, putcogreg, dstimm);
    return EmitOp1(irl, OPC_CALL, putcogreg);
}

static IR *EmitCogwrite(IRList *irl, Operand *src, Operand *dst)
{
    Operand *srcimm;
    if (!putcogreg) {
        putcogreg = NewOperand(IMM_COG_LABEL, "wrcog", 0);
    }
    src = Dereference(irl, src);
    srcimm = GetLea(irl, src);
    EmitOp2(irl, OPC_MOVS, putcogreg, srcimm);
    EmitOp2(irl, OPC_MOVD, putcogreg, dst);
    return EmitOp1(irl, OPC_CALL, putcogreg);
}

static IR *EmitMove(IRList *irl, Operand *origdst, Operand *origsrc)
{
    Operand *temp = NULL;
    Operand *temp2 = NULL;
    Operand *dst = origdst;
    Operand *src = origsrc;
    IR *ir = NULL;

    if (src->kind == IMM_HUB_LABEL) {
        src = NewImmediatePtr(NULL, src);
    }
    if (IsMemRef(src)) {
        int off = src->val;
        Operand *where;
        
        // if we are reading into a register, no need for
        // a temp
        if (IsValidDstReg(origdst)) {
            where = origdst;
        } else {
            where = temp = NewFunctionTempRegister();
        }
        src = (Operand *)src->name;
        if (off) {
            EmitAddSub(irl, src, off);
        }
        switch (origsrc->kind) {
        default:
            ERROR(NULL, "Illegal memory reference");
            break;
        case COG_REF:
            ir = EmitCogread(irl, where, src);
            break;
        case LONG_REF:
            ir = EmitOp2(irl, OPC_RDLONG, where, src);
            break;
        case WORD_REF:
            ir = EmitOp2(irl, OPC_RDWORD, where, src);
            break;
        case BYTE_REF:
            ir = EmitOp2(irl, OPC_RDBYTE, where, src);
            break;
        }
        if (off) {
            EmitAddSub(irl, src, -off);
        }
        if (where == origdst)
            return ir;
        src = temp;
    }
    if (IsMemRef(origdst)) {
        int off = dst->val;
        dst = (Operand *)dst->name;
        if (src->kind == IMM_INT || SrcOnlyHwReg(src)) {
            temp2 = NewFunctionTempRegister();
            EmitMove(irl, temp2, src);
            src = temp2;
        }
        if (off) {
            EmitAddSub(irl, dst, off);
        }
        switch (origdst->kind) {
        default:
            ERROR(NULL, "Illegal memory reference");
            break;
        case COG_REF:
            ir = EmitCogwrite(irl, src, dst);
            break;
        case LONG_REF:
            ir = EmitOp2(irl, OPC_WRLONG, src, dst);
            break;
        case WORD_REF:
            ir = EmitOp2(irl, OPC_WRWORD, src, dst);
            break;
        case BYTE_REF:
            ir = EmitOp2(irl, OPC_WRBYTE, src, dst);
            break;
        }
        if (off) {
            EmitAddSub(irl, dst, -off);
        }            
    } else {
        if (dst != src) {
            if (dst->kind == IMM_INT) {
                ERROR(NULL, "Internal Error: emitting move to immediate");
            }
            ir = EmitOp2(irl, OPC_MOV, dst, src);
        }
    }
    if (temp) {
        EmitOp1(irl, OPC_DEAD, temp);
    }
    if (temp2) {
        EmitOp1(irl, OPC_DEAD, temp2);
    }
    return ir;
}

void
FreeTempRegisters(IRList *irl, int starttempreg)
{
    int endtempreg;
    Operand *op;

    /* release temporaries we used */
    endtempreg = FuncData(curfunc)->curtempreg;
    FuncData(curfunc)->curtempreg = starttempreg;

    /* and mark them as dead */
    while (endtempreg > starttempreg) {
      op = GetFunctionTempRegister(curfunc, endtempreg);
      EmitOp1(irl, OPC_DEAD, op);
      --endtempreg;
    }
}

//
// emit debug directives
//
static LexStream *current_lex = NULL;

void
ResetDebugComment(Module *P)
{
    current_lex = &P->L;
    if (gl_debug) {
        current_lex->lineCounter = 0;
    }
}

static void
EmitOneSrcComment(IRList *irl, int line, LexStream *L)
{
    const char **srclines;
    const char *theline;
    int maxline;
    Operand *c;
    
    srclines = (const char **)flexbuf_peek(&L->lines);
    maxline = flexbuf_curlen(&L->lines) / sizeof(char **);
    theline = srclines[line];
    if (!theline) return;
    if (line < 0 || line >= maxline) return;
    
    // skip over preprocessor comments
    if (theline[0] == '{' && theline[1] == '#') {
        theline += 2;
        while (*theline && *theline != '}') theline++;
        if (*theline) theline++;
    }
    c = NewOperand(IMM_STRING, theline, 0);
    EmitOp1(irl, OPC_COMMENT, c);
}

void
EmitDebugComment(IRList *irl, AST *ast)
{
    LexStream *L;

    if (!gl_debug) return;
    if (!ast) return;
    if (!current) return;
    
    L = &current->L;
    if (!strcmp(L->fileName, ast->fileName)) {
        int line = ast->line;
        int i = L->lineCounter;
        while (i <= line) {
            EmitOneSrcComment(irl, i, L);
            i++;
        }
        L->lineCounter = i;
    }
}

//
// a for loop gets a pattern like
//
//   initial code
// Ltop:
//   if (!loopcond) goto Lexit
//   loop body
// Lnext:
//   updatestmt
//   goto Ltop
// Lexit
//
// if it must execute at least once, it looks like:
//   initial code
// Ltop:
//   loop body
// Lnext
//   updatestmt
//   if (loopcond) goto Ltop
// Lexit
//

static void CompileForLoop(IRList *irl, AST *ast, int atleastonce)
{
    AST *initstmt;
    AST *loopcond;
    AST *update;
    AST *body = 0;

    Operand *toplabel, *nextlabel, *exitlabel;
    initstmt = ast->left;
    ast = ast->right;
    if (!ast || ast->kind != AST_TO) {
        ERROR(ast, "Internal Error: expected AST_TO in for loop");
        return;
    }
    loopcond = ast->left;
    ast = ast->right;
    if (!ast || ast->kind != AST_STEP) {
        ERROR(ast, "Internal Error: expected AST_TO in for loop");
        return;
    }
    update = ast->left;
    ast = ast->right;
    body = ast;

    if (initstmt) {
        EmitDebugComment(irl, initstmt);
    } else if (loopcond) {
        EmitDebugComment(irl, loopcond);
    } else if (body) {
        EmitDebugComment(irl, body);
    }
    CompileExpression(irl, initstmt, NULL);
    
    toplabel = NewCodeLabel();
    nextlabel = NewCodeLabel();
    exitlabel = NewCodeLabel();
    PushQuitNext(exitlabel, nextlabel);

    EmitLabel(irl, toplabel);
    if (!atleastonce) {
        CompileBoolBranches(irl, loopcond, NULL, exitlabel);
    }
    CompileStatementList(irl, body);
    EmitLabel(irl, nextlabel);
    CompileStatement(irl, update);
    if (atleastonce) {
        CompileBoolBranches(irl, loopcond, toplabel, NULL);
    } else {
        EmitJump(irl, COND_TRUE, toplabel);
    }
    EmitLabel(irl, exitlabel);
    PopQuitNext();
}

static void CompileCaseStmt(IRList *irl, AST *ast)
{
    Operand *labeldone = NewCodeLabel();
    Operand *labelnext = NULL;
    AST *var;
    AST *item;
    AST *booltest;
    AST *stmts;

    var = ast->left;
    if (var->kind == AST_ASSIGN) {
        CompileExpression(irl, var, NULL);
        var = var->left;
    } else if (var->kind != AST_IDENTIFIER) {
        ERROR(var, "Internal error, expected identifier in case");
        return;
    }
    ast = ast->right;
    while (ast) {
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Internal error in case list");
            return;
        }
        item = ast->left;
        ast = ast->right;
        booltest = TransformCaseExprList(var, item->left);
        stmts = item->right;
        if (labelnext) {
            EmitJump(irl, COND_TRUE, labeldone);
            EmitLabel(irl, labelnext);
        }
        labelnext = NewCodeLabel();
        CompileBoolBranches(irl, booltest, NULL, labelnext);
        CompileStatementList(irl, stmts);
    }
    if (labelnext) {
        EmitLabel(irl, labelnext);
    }
    EmitLabel(irl, labeldone);
}

static void CompileStatement(IRList *irl, AST *ast)
{
    AST *retval;
    Operand *op;
    Operand *result;
    Operand *botloop, *toploop;
    Operand *exitloop;
    int starttempreg;

    if (!ast) return;

    starttempreg = FuncData(curfunc)->curtempreg;
    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        CompileStatement(irl, ast->left);
        break;
    case AST_RETURN:
        EmitDebugComment(irl, ast);
        retval = ast->left;
        if (!retval) {
            retval = curfunc->resultexpr;
        }
	if (retval) {
	    op = CompileExpression(irl, retval, NULL);
	    result = GetOneGlobal(REG_REG, "result1", 0);
            EmitMove(irl, result, op);
	}
	EmitJump(irl, COND_TRUE, FuncData(curfunc)->asmreturnlabel);
	break;
    case AST_ABORT:
        EmitDebugComment(irl, ast);
        retval = ast->left;
        if (!retval) {
            retval = curfunc->resultexpr;
        }
	if (!retval) {
            retval = AstInteger(0);
        }
        ValidateAbortFuncs();
        op = CompileExpression(irl, retval, NULL);
        EmitMove(irl, arg1, op);
        EmitOp1(irl, OPC_CALL, abortfunc);
        break;
    case AST_WHILE:
        EmitDebugComment(irl, ast->left);
        toploop = NewCodeLabel();
	botloop = NewCodeLabel();
	PushQuitNext(botloop, toploop);
	EmitLabel(irl, toploop);
        CompileBoolBranches(irl, ast->left, NULL, botloop);
	FreeTempRegisters(irl, starttempreg);
        CompileStatementList(irl, ast->right);
	EmitJump(irl, COND_TRUE, toploop);
	EmitLabel(irl, botloop);
	PopQuitNext();
	break;
    case AST_DOWHILE:
        toploop = NewCodeLabel();
	botloop = NewCodeLabel();
	exitloop = NewCodeLabel();
	PushQuitNext(exitloop, botloop);
	EmitLabel(irl, toploop);
        CompileStatementList(irl, ast->right);
	EmitLabel(irl, botloop);
        CompileBoolBranches(irl, ast->left, toploop, NULL);
	FreeTempRegisters(irl, starttempreg);
	EmitLabel(irl, exitloop);
	PopQuitNext();
	break;
    case AST_FORATLEASTONCE:
    case AST_FOR:
	CompileForLoop(irl, ast, ast->kind == AST_FORATLEASTONCE);
        break;
    case AST_INLINEASM:
        //EmitDebugComment(irl, ast);
        CompileInlineAsm(irl, ast->left);
        break;
    case AST_CASE:
        CompileCaseStmt(irl, ast);
        break;
    case AST_QUIT:
        EmitDebugComment(irl, ast);
        if (!quitlabel) {
	    ERROR(ast, "loop exit statement outside of loop");
	} else {
	    EmitJump(irl, COND_TRUE, quitlabel);
	}
	break;
    case AST_NEXT:
        EmitDebugComment(irl, ast);
        if (!nextlabel) {
	    ERROR(ast, "loop continue statement outside of loop");
	} else {
	    EmitJump(irl, COND_TRUE, nextlabel);
	}
	break;
    case AST_IF:
        EmitDebugComment(irl, ast->left);
        toploop = NewCodeLabel();
        CompileBoolBranches(irl, ast->left, NULL, toploop);
	FreeTempRegisters(irl, starttempreg);
	ast = ast->right;
	if (ast->kind == AST_COMMENTEDNODE) {
	  ast = ast->left;
	}
	/* ast should be an AST_THENELSE */
	CompileStatementList(irl, ast->left);
	if (ast->right) {
	  botloop = NewCodeLabel();
	  EmitJump(irl, COND_TRUE, botloop);
	  EmitLabel(irl, toploop);
	  CompileStatementList(irl, ast->right);
	  EmitLabel(irl, botloop);
	} else {
	  EmitLabel(irl, toploop);
	}
	break;
    case AST_YIELD:
	/* do nothing in assembly for YIELD */
        break;
    case AST_STMTLIST:
        CompileStatementList(irl, ast);
        break;
    case AST_ASSIGN:
        /* fall through */
    default:
        /* assume an expression */
        EmitDebugComment(irl, ast);
        (void)CompileExpression(irl, ast, NULL);
        break;
    }
    FreeTempRegisters(irl, starttempreg);
}

/*
 * compile just the body of a function, and put it
 * in an IRL for that function
 */
static void
CompileFunctionBody(Function *f)
{
    IRList *irl = FuncIRL(f);
    IRList *irheader = &FuncData(f)->irheader;
    
    nextlabel = quitlabel = NULL;
    EmitDebugComment(irheader, f->decl);
    
    EmitFunctionProlog(irl, f);
    // emit initializations if any required
    if (!f->result_in_parmarray && f->resultexpr && !IsConstExpr(f->resultexpr))
    {
        AST *resinit = AstAssign(f->resultexpr, AstInteger(0));
        CompileStatement(irl, resinit);
    }
    //
    CompileStatementList(irl, f->body);
    EmitFunctionEpilog(irl, f);
    OptimizeIRLocal(irl);
}

/*
 * compile a function to IR and put it at the end of the IRList
 */

static void
CompileWholeFunction(IRList *irl, Function *f)
{
    IRList *firl = FuncIRL(f);

    EmitFunctionHeader(irl, f);
    AppendIRList(irl, firl);
    EmitFunctionFooter(irl, f);
}

static Operand *newlineOp;
static void EmitNewline(IRList *irl)
{
  IR *ir = NewIR(OPC_LITERAL);
  ir->dst = newlineOp;
  AppendIR(irl, ir);
}

static int gcmpfunc(const void *a, const void *b)
{
  const AsmVariable *ga = (const AsmVariable *)a;
  const AsmVariable *gb = (const AsmVariable *)b;
  return strcmp(ga->op->name, gb->op->name);
}

#define SORT_ALPHABETICALLY 1
#define NO_SORT 0

static void EmitAsmVars(struct flexbuf *fb, IRList *datairl, IRList *bssirl, int flags)
{
    size_t siz = flexbuf_curlen(fb) / sizeof(AsmVariable);
    size_t i;
    AsmVariable *g = (AsmVariable *)flexbuf_peek(fb);
    int varsize;
    int alphaSort = flags & SORT_ALPHABETICALLY;

    if (siz > 0) {
      EmitNewline(datairl);
    }
    /* sort the global variables */
    if (alphaSort) {
        qsort(g, siz, sizeof(*g), gcmpfunc);
    }
    for (i = 0; i < siz; i++) {
      if (g[i].op->kind == REG_LOCAL && !g[i].op->used) {
	continue;
      }
      if (g[i].op->kind == IMM_INT && !g[i].op->used) {
	continue;
      }
      if (g[i].op->kind == REG_HW) {
	continue;
      }
      switch(g[i].op->kind) {
      case STRING_DEF:
          EmitLabel(datairl, g[i].op);
          EmitString(datairl, (AST *)g[i].val);
          break;
      case IMM_COG_LABEL:
      case IMM_HUB_LABEL:
      case REG_HUBPTR:
      case REG_COGPTR:
          EmitLabel(datairl, g[i].op);
          EmitLongPtr(datairl, (Operand *)g[i].val);
          break;
      case IMM_INT:
          EmitLabel(datairl, g[i].op);
          EmitLong(datairl, g[i].val);
          break;
      case REG_ARG:
      case REG_LOCAL:
	  if (bssirl != NULL) {
   	      EmitLabel(bssirl, g[i].op);
	      varsize = g[i].count / LONG_SIZE;
	      if (varsize == 0) varsize = 1;
	      EmitReserve(bssirl, varsize, COG_RESERVE);
	      break;
	  }
	  // otherwise fall through
      default:
          EmitLabel(datairl, g[i].op);
          varsize = g[i].count / LONG_SIZE;
          if (varsize <= 1) {
              EmitLong(datairl, g[i].val);
          } else {
              /* normally ir->src is NULL for OPC_LONG, but in this
                 case (an array definition) it is a count */
              EmitOp2(datairl, OPC_LONG, NewOperand(IMM_INT, "", 0),
                      NewOperand(IMM_INT, "", varsize));
          }
          break;
      }
    }
}
static void EmitGlobals(IRList *cogdata, IRList *cogbss, IRList *hubdata)
{
    EmitAsmVars(&cogGlobalVars, cogdata, cogbss, SORT_ALPHABETICALLY);
    EmitAsmVars(&hubGlobalVars, hubdata, NULL, NO_SORT);
}

#define VISITFLAG_COMPILEIR     0x01230001
#define VISITFLAG_FUNCNAMES     0x01230002
#define VISITFLAG_COMPILEFUNCS  0x01230003
#define VISITFLAG_EXPANDINLINE  0x01230004
#define VISITFLAG_EMITDAT       0x01230005

typedef void (*VisitorFunc)(IRList *irl, Module *P);

static void
VisitRecursive(IRList *irl, Module *P, VisitorFunc func, unsigned visitval)
{
    Module *Q;
    AST *subobj;
    Module *save = current;
    Function *savecurf = curfunc;
    
    if (P->visitflag == visitval)
        return;

    current = P;

    P->visitflag = visitval;
    (*func)(irl, P);

    // compile intermediate code for submodules
    for (subobj = P->objblock; subobj; subobj = subobj->right) {
        if (subobj->kind != AST_OBJECT) {
            ERROR(subobj, "Internal Error: Expecting object AST");
            break;
        }
        Q = (Module *)subobj->d.ptr;
        VisitRecursive(irl, Q, func, visitval);
    }
    current = save;
    curfunc = savecurf;
}

static bool
IsGlobalModule(Module *P)
{
    return P == globalModule;
}

//
// return true if a function should be removed
//
static bool
ShouldSkipFunction(Function *f)
{
    if (0 == (gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS))
        return false;
    if (gl_output == OUTPUT_COGSPIN)
        return false;
    if (0 != f->callSites)
        return false;
    if (f->cog_task)
        return false; // used in another cog
    // do not skip global functions, we handle those separately
    if (IsGlobalModule(f->module))
        return false;
    return true;
}

//
// return true if a function will be removed if inlined
//
bool
RemoveIfInlined(Function *f)
{
    if (f->cog_task)
        return false;
    if (ShouldSkipFunction(f))
        return true;
    if (!f->is_public || (gl_optimize_flags & OPT_REMOVE_UNUSED_FUNCS))
        return true;
    return false;
}

// assign all function names so we can do forward calls
// this is also where we can allocate the back end data
static void
AssignFuncNames(IRList *irl, Module *P)
{
    Function *f;
    Function *savecur = curfunc;
    
    (void)irl; // not used

    ResetDebugComment(P);
    if (!P->bedata) {
        P->bedata = calloc(sizeof(AsmModData), 1);
    }
    for(f = P->functions; f; f = f->next) {
	const char *fname;
        char *frname;
        char *faltname;
        
        if (ShouldSkipFunction(f))
            continue;
        curfunc = f;
        f->cog_code = (COG_CODE || (IsGlobalModule(P) && !gl_compressed)) ? 1 : 0;
        fname = IdentifierGlobalName(P, f->name);
	frname = (char *)malloc(strlen(fname) + 5);
	sprintf(frname, "%s_ret", fname);
        if (gl_output == OUTPUT_COGSPIN && f->cog_code && f->is_public) {
            faltname = (char *)malloc(strlen(fname) + 6);
            sprintf(faltname, "pasm%s", fname);
        } else {
            faltname = NULL;
        }
        f->bedata = calloc(1, sizeof(IRFuncData));

        // figure out calling convention
        if (f->local_address_taken || f->cog_task) {
            FuncData(f)->convention = STACK_CALL;
        } else if (f->cog_code) {
            FuncData(f)->convention = FAST_CALL;
        } else {
            FuncData(f)->convention = FAST_CALL; // default
        }

        if (f->cog_code) {
            FuncData(f)->asmname = NewOperand(IMM_COG_LABEL, fname, 0);
            FuncData(f)->asmretname = NewOperand(IMM_COG_LABEL, frname, 0);
        } else {
            FuncData(f)->asmname = NewOperand(IMM_HUB_LABEL, fname, 0);
            FuncData(f)->asmretname = NewOperand(IMM_HUB_LABEL, frname, 0);
        }
        if (f->is_recursive || IS_STACK_CALL(f)) {
            FuncData(f)->asmreturnlabel = NewCodeLabel();
            f->no_inline = 1; // cannot inline these, they need special setup/shutdown
        } else {
            FuncData(f)->asmreturnlabel = FuncData(f)->asmretname;
        }
        if (faltname) {
            FuncData(f)->asmaltname = NewOperand(IMM_COG_LABEL, faltname, 0);
        }
        // also fix up any extra declarations
        if (f->extradecl) {
            AST *ast = f->extradecl;
            AST *decl;
            AST *table;
            AST *name;
            Symbol *sym;
            int tablelen;
            Label *label;
            while (ast) {
                if (ast->kind != AST_LISTHOLDER) {
                    ERROR(ast, "Internal error: expected list holder");
                    break;
                }
                decl = ast->left;
                if (decl->kind != AST_TEMPARRAYDECL) {
                    ERROR(ast, "Internal error: expected temp array decl");
                    break;
                }
                name = decl->left;  // this is the array def
                tablelen = EvalConstExpr(name->right);
                name = name->left;
                if (name->kind != AST_IDENTIFIER) {
                    ERROR(ast, "Internal error: expected identifier");
                    break;
                }
                sym = FindSymbol(&P->objsyms, name->d.string);
                if (!sym || sym->type != SYM_TEMPVAR) {
                    ERROR(name, "Internal error: unable to find symbol");
                    break;
                }
                
                table = decl->right;
                if (table->kind != AST_EXPRLIST) {
                    ERROR(table, "Internal error: expected expression list");
                    break;
                }
                P->datsize = (P->datsize + 3) & ~3; // round up to long boundary
                label = (Label *)calloc(sizeof(*label), 1);
                sym->offset = label->hubval = P->datsize;
                label->type = ast_type_long;
                sym->type = SYM_LABEL;
                sym->val = (void *)label;
                table = NewAST(AST_LONGLIST, table, NULL);
                P->datblock = AddToList(P->datblock, table);
                P->datsize += tablelen * LONG_SIZE;
                ast = ast->right;
            }
        }
    }
    curfunc = savecur;
}

static void
CompileFunc_internal(IRList *irl, Module *P)
{
    Function *savecurf = curfunc;
    
    Function *f;
    (void)irl; // not used
    for(f = P->functions; f; f = f->next) {
      if (ShouldSkipFunction(f))
          continue;
      curfunc = f;
      CompileFunctionBody(f);
      FuncData(f)->isInline = ShouldBeInlined(f);
    }
    curfunc = savecurf;
}

static void
ExpandInline_internal(IRList *irl, Module *P)
{
    Function *f;
    int change;
    
    for (f = P->functions; f; f = f->next) {
        IRList *firl = FuncIRL(f);
        if (ShouldSkipFunction(f))
            continue;
        curfunc = f;
        for(;;) {
            change = ExpandInlines(firl);
            if (!change) break;
            // may be new opportunities for optimization
            OptimizeIRLocal(firl);
        }
    }
}

void
CompileIntermediate(Module *P)
{
    if (!newlineOp)
      newlineOp = NewOperand(IMM_STRING, "\n", 0);

    VisitRecursive(NULL, P, AssignFuncNames, VISITFLAG_FUNCNAMES);
    VisitRecursive(NULL, P, CompileFunc_internal, VISITFLAG_COMPILEFUNCS);
    VisitRecursive(NULL, P, ExpandInline_internal, VISITFLAG_EXPANDINLINE);
}

static void
CompileToIR_internal(IRList *irl, Module *P)
{
    Function *f;
    Function *save = curfunc;

    // emit output for P
    for(f = P->functions; f; f = f->next) {
        // if the function was private and has
        // been inlined, skip it
        if (ShouldSkipFunction(f)) {
            continue;
        }
        if (RemoveIfInlined(f)) {
            // system functions were not skipped in ShouldSkipFunction,
            // so skip them here if they are not used
            // also skip inlined private and single-use functions
            if (FuncData(f)->isInline) {
                continue;
            }
            if (!f->callSites) {
                continue;
            }
        }
        curfunc = f;
	EmitNewline(irl);
        CompileWholeFunction(irl, f);
    }
    curfunc = save;
}

bool
CompileToIR(IRList *irl, Module *P)
{
    // generate code for inlining
    CompileIntermediate(P);
    // and generate real output
    VisitRecursive(irl, P, CompileToIR_internal, VISITFLAG_COMPILEIR);
    
    return gl_errors == 0;
}
/*
 * emit builtin functions like mul and div
 */
static const char *builtin_mul_p1 =
"\nmultiply_\n"
"\tmov\titmp2_, muldiva_\n"
"\txor\titmp2_, muldivb_\n"
"\tabs\tmuldiva_, muldiva_\n"
"\tabs\tmuldivb_, muldivb_\n"
"\tmov\tresult1, #0\n"
"\tmov\titmp1_, #32\n"
"\tshr\tmuldiva_, #1 wc\n"
"mul_lp_\n"
" if_c\tadd\tresult1, muldivb_ wc\n"
"\trcr\tresult1, #1 wc\n"
"\trcr\tmuldiva_, #1 wc\n"
"\tdjnz\titmp1_, #mul_lp_\n"

"\tshr\titmp2_, #31 wz\n"
" if_nz\tneg\tresult1, result1\n"
" if_nz\tneg\tmuldiva_, muldiva_ wz\n"
" if_nz\tsub\tresult1, #1\n"
"\tmov\tmuldivb_, result1\n"
"multiply__ret\n"
"\tret\n"
;

static const char *builtin_mul_p1_fast =
"\nmultiply_\n"
"       mov    itmp2_, muldiva_\n"
"       xor    itmp2_, muldivb_\n"
"       abs    muldiva_, muldiva_\n"
"       abs    muldivb_, muldivb_\n"
"	mov    result1, #0\n"
"mul_lp_\n"
"	shr    muldivb_, #1 wc,wz\n"
" if_c	add    result1, muldiva_\n"
"	shl    muldiva_, #1\n"
" if_ne	jmp    #mul_lp_\n"
"       shr    itmp2_, #31 wz\n"
" if_nz neg    result1, result1\n"
"	mov    muldiva_, result1\n"
"multiply__ret\n"
"	ret\n"
;

static const char *builtin_mul_p2 =
"\nmultiply_\n"
"\tmov\titmp2_, muldiva_\n"
"\txor\titmp2_, muldivb_\n"
"\tabs\tmuldiva_, muldiva_\n"
"\tabs\tmuldivb_, muldivb_\n"
"\tqmul\tmuldiva_, muldivb_\n"
"\tgetqx\tmuldiva_\n"
"\tgetqy\tmuldivb_\n"
"\tshr\titmp2_, #31 wz\n"
" if_nz\tneg\tmuldivb_, muldivb_\n"
" if_nz\tneg\tmuldiva_, muldiva_ wz\n"
" if_nz\tsub\tmuldivb_, #1\n"
"\treta\n"
;

/*
 * signed divide, taken from spin interpreter
 * calculates muldiva_ / muldivb_
 * quotient in muldivb_, remainder in muldiva_
 */

static const char *builtin_div_p1 =
"' code originally from spin interpreter, modified slightly\n"
"\ndivide_\n"
"       abs     muldiva_,muldiva_     wc       'abs(x)\n"
"       muxc    itmp2_,#%11                    'store sign of x\n"
"       abs     muldivb_,muldivb_     wc,wz    'abs(y)\n"
" if_c  xor     itmp2_,#%10                    'store sign of y\n"

"        mov     itmp1_,#0                    'unsigned divide\n"
"        mov     DIVCNT,#32\n"
"mdiv__\n"
"        shr     muldivb_,#1        wc,wz\n"
"        rcr     itmp1_,#1\n"
" if_nz   djnz    DIVCNT,#mdiv__\n"
"mdiv2__\n"
"        cmpsub  muldiva_,itmp1_        wc\n"
"        rcl     muldivb_,#1\n"
"        shr     itmp1_,#1\n"
"        djnz    DIVCNT,#mdiv2__\n"

"        test    itmp2_,#1        wc       'restore sign, remainder\n"
"        negc    muldiva_,muldiva_ \n"
"        test    itmp2_,#%10      wc       'restore sign, division result\n"
"        negc    muldivb_,muldivb_\n"

"divide__ret\n"
    "\tret\n"
"DIVCNT\n"
"\tlong\t0\n"
;
static const char *builtin_div_p2 =
"\ndivide_\n"
"       abs     muldiva_,muldiva_     wc       'abs(x)\n"
"       muxc    itmp2_,#%11                    'store sign of x\n"
"       abs     muldivb_,muldivb_     wcz      'abs(y)\n"
" if_c  xor     itmp2_,#%10                    'store sign of y\n"
"       setq    #0\n"
"       qdiv    muldiva_, muldivb_\n"
"       getqx   muldivb_\n"  // get quotient
"       getqy   muldiva_\n"  // get remainder
"       test    itmp2_,#1        wc       'restore sign, remainder\n"
"       negc    muldiva_,muldiva_ \n"
"       test    itmp2_,#%10      wc       'restore sign, division result\n"
"       negc    muldivb_,muldivb_\n"
"       reta\n"
;

static const char *builtin_lmm_p1 =
    "LMM_LOOP\n"
    "    rdlong LMM_i1, pc\n"
    "    add    pc, #4\n"
    "LMM_i1\n"
    "    nop\n"
    "    rdlong LMM_i2, pc\n"
    "    add    pc, #4\n"
    "LMM_i2\n"
    "    nop\n"
    "    rdlong LMM_i3, pc\n"
    "    add    pc, #4\n"
    "LMM_i3\n"
    "    nop\n"
    "    rdlong LMM_i4, pc\n"
    "    add    pc, #4\n"
    "LMM_i4\n"
    "    nop\n"
    "LMM_jmptop\n"
    "    jmp    #LMM_LOOP\n"
    "pc\n"
    "    long @@@hubentry\n"
    "lr\n"
    "    long 0\n"
    "hubretptr\n"
    "    long @@@hub_ret_to_cog\n"
    "LMM_CALL\n"
    "    mov    lr, pc\n"
    "    add    lr, #4\n"
    "    wrlong lr, sp\n"
    "    add    sp, #4\n"
    "    ' fall through\n"
    "LMM_JUMP\n"
    "    rdlong pc, pc\n"
    "    jmp    #LMM_LOOP\n"
    "LMM_RET\n"
    "    sub    sp, #4\n"
    "    rdlong pc, sp\n"
    "    jmp    #LMM_LOOP\n"
    "LMM_CALL_FROM_COG\n"
    "    wrlong  hubretptr, sp\n"
    "    add     sp, #4\n"
    "    jmp  #LMM_LOOP\n"
    "LMM_CALL_FROM_COG_ret\n"
    "    ret\n"
    "LMM_FCACHE_LOAD\n"
    "    rdlong COUNT, pc\n"
    "    add    pc, #4\n"
    "    mov    ADDR, pc\n"
    "    sub    LMM_ADDR, pc\n"
    "    tjz    LMM_ADDR, #a_fcachegoaddpc\n"
    "    movd   a_fcacheldlp, #LMM_FCACHE_START\n"
    "    shr    COUNT, #2\n"
    "a_fcacheldlp\n"
    "    rdlong 0-0, pc\n"
    "    add    pc, #4\n"
    "    add    a_fcacheldlp,inc_dest1\n"
    "    djnz   COUNT,#a_fcacheldlp\n"
    // add in a JMP back out of LMM
    "    ror    a_fcacheldlp, #9\n"
    "    movd   a_fcachecopyjmp, a_fcacheldlp\n"
    "    rol    a_fcacheldlp, #9\n"
    "a_fcachecopyjmp\n"
    "    mov    0-0, LMM_jmptop\n"
    "a_fcachego\n"
    "    mov    LMM_ADDR, ADDR\n"
    "    jmpret LMM_RETREG,#LMM_FCACHE_START\n"
    "a_fcachegoaddpc\n"
    "    add    pc, COUNT\n"
    "    jmp    #a_fcachego\n"
    "LMM_FCACHE_LOAD_ret\n"
    "    ret\n"
    "inc_dest1\n"
    "    long (1<<9)\n"
    "LMM_LEAVE_CODE\n"
    "    jmp LMM_RETREG\n"
    "LMM_ADDR\n"
    "    long 0\n"
    "ADDR\n"
    "    long 0\n"
    "COUNT\n"
    "    long 0\n"
    ;

static const char *builtin_lmm_p2 =
    "\tmov ptra, ##@stackspace\n"
    "\tjmp #@hubentry\n"
    ;

/* WARNING: make sure to increase SETJMP_BUF_SIZE if you add
 * more things to be saved in abort/catch
 */
static const char *builtin_abortcode_p1 =
    "abortchain long 0\n"
    "__setjmp\n"
    "    wrlong abortchain, arg1\n"
    "    mov abortchain, arg1\n"
    "    add arg1, #4\n"
    "    wrlong pc, arg1\n"
    "    add arg1, #4\n"
    "    wrlong sp, arg1\n"
    "    add arg1, #4\n"
    "    wrlong fp, arg1\n"
    "    add arg1, #4\n"
    "    wrlong objptr, arg1\n"
    "    add arg1, #4\n"
    "    wrlong __setjmp_ret, arg1\n"
    "    mov result2, #0\n"
    "__setjmp_ret\n"
    "    ret\n"
    "__abort\n"
    "    mov  result1, arg1\n"
    "    mov  arg1, abortchain\n"
    "    rdlong abortchain, arg1\n"
    "    add arg1, #4\n"
    "    rdlong pc, arg1\n"
    "    add arg1, #4\n"
    "    rdlong sp, arg1\n"
    "    add arg1, #4\n"
    "    rdlong fp, arg1\n"
    "    add arg1, #4\n"
    "    rdlong objptr, arg1\n"
    "    add arg1, #4\n"
    "    rdlong __abort_ret, arg1\n"
    "    mov result2, #1\n"
    "__abort_ret\n"
    "    ret\n"
    ;

static const char *builtin_abortcode_p2 =
    "abortchain long 0\n"
    "__setjmp\n"
    "    wrlong abortchain, arg1\n"
    "    mov abortchain, arg1\n"
    "    add arg1, #4\n"
    "    wrlong pc, arg1\n"
    "    add arg1, #4\n"
    "    wrlong ptra, arg1\n"
    "    add arg1, #4\n"
    "    wrlong fp, arg1\n"
    "    add arg1, #4\n"
    "    wrlong objptr, arg1\n"
    "    add arg1, #4\n"
    "    wrlong __setjmp_ret, arg1\n"
    "    mov result2, #0\n"
    "__setjmp_ret\n"
    "    ret\n"
    "__abort\n"
    "    mov  result1, arg1\n"
    "    mov  arg1, abortchain\n"
    "    rdlong abortchain, arg1\n"
    "    add arg1, #4\n"
    "    rdlong pc, arg1\n"
    "    add arg1, #4\n"
    "    rdlong ptra, arg1\n"
    "    add arg1, #4\n"
    "    rdlong fp, arg1\n"
    "    add arg1, #4\n"
    "    rdlong objptr, arg1\n"
    "    add arg1, #4\n"
    "    rdlong __abort_ret, arg1\n"
    "    mov result2, #1\n"
    "__abort_ret\n"
    "    ret\n"
    ;

const char *builtin_wrcog =
    "wrcog\n"
    "    mov    0-0, 0-0\n"
    "wrcog_ret\n"
    "    ret\n"
    ;

static void
EmitBuiltins(IRList *irl)
{
    if (HUB_CODE) {
        const char *builtin_lmm = gl_p2 ? builtin_lmm_p2 : builtin_lmm_p1;
        Operand *loop = NewOperand(IMM_STRING, builtin_lmm, 0);
        EmitOp1(irl, OPC_LITERAL, loop);
    }
    if (mulfunc) {
        Operand *loop;

        if (gl_p2) {
            loop = NewOperand(IMM_STRING, builtin_mul_p2, 0);
        } else if (g_NeedMulHi) {
            loop = NewOperand(IMM_STRING, builtin_mul_p1, 0);
        } else {
            loop = NewOperand(IMM_STRING, builtin_mul_p1_fast, 0);
        }
        EmitOp1(irl, OPC_LITERAL, loop);
        (void)GetOneGlobal(REG_REG, "itmp1_", 0);
        (void)GetOneGlobal(REG_REG, "itmp2_", 0);
        (void)GetOneGlobal(REG_REG, "result1", 0);
    }
    if (divfunc) {
        Operand *loop;

        if (gl_p2) {
            loop = NewOperand(IMM_STRING, builtin_div_p2, 0);
        } else {
            loop = NewOperand(IMM_STRING, builtin_div_p1, 0);
        }
        EmitOp1(irl, OPC_LITERAL, loop);
        (void)GetOneGlobal(REG_REG, "itmp1_", 0);
        (void)GetOneGlobal(REG_REG, "itmp2_", 0);
        (void)GetOneGlobal(REG_REG, "result1", 0);
    }
    if (abortfunc) {
        Operand *loop;

        if (gl_p2) {
            loop = NewOperand(IMM_STRING, builtin_abortcode_p2, 0);
        } else {
            loop = NewOperand(IMM_STRING, builtin_abortcode_p1, 0);
        }
        EmitOp1(irl, OPC_LITERAL, loop);
        if (COG_CODE) {
            // abort saves the pc, which we don't have in COG mode, so
            // add a dummy pc register
            (void)GetOneGlobal(REG_REG, "pc", 0);
        }
    }
    if (putcogreg) {
        Operand *func = NewOperand(IMM_STRING, builtin_wrcog, 0);
        EmitOp1(irl, OPC_LITERAL, func);
    }
}

static void
CompileConstant(IRList *irl, AST *ast)
{
    const char *name = ast->d.string;
    Symbol *sym;
    AST *expr;
    int32_t val;
    Operand *op1, *op2;
    
    if (!IsConstExpr(ast)) {
        ERROR(ast, "%s is not constant", name);
        return;
    }
    sym = LookupSymbol(name);
    if (!sym) {
        ERROR(ast, "constant symbol %s not declared??", name);
        return;
    }
    expr = (AST *)sym->val;
    val = EvalConstExpr(expr);
    op1 = NewOperand(IMM_STRING, name, val);
    op2 = NewImmediate(val);
    EmitOp2(irl, OPC_CONST, op1, op2);
}

static void
CompileConsts(IRList *irl, AST *conblock)
{
    AST *ast, *upper;

    for (upper = conblock; upper; upper = upper->right) {
        ast = upper->left;
        if (ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        switch (ast->kind) {
        case AST_ENUMSKIP:
            CompileConstant(irl, ast->left);
            break;
        case AST_IDENTIFIER:
            CompileConstant(irl, ast);
            break;
        case AST_ASSIGN:
            CompileConstant(irl, ast->left);
            break;
        default:
            /* do nothing */
            break;
        }
    }
}

void
EmitDatSection(IRList *irl, Module *P)
{
  Flexbuf *fb;
  Flexbuf *relocs;
  Operand *op;

  if (!ModData(P)->datbase)
      return;
  fb = (Flexbuf *)calloc(1, sizeof(*fb));
  relocs = (Flexbuf *)calloc(1, sizeof(*relocs));
  flexbuf_init(fb, 32768);
  flexbuf_init(relocs, 512);
  PrintDataBlock(fb, P, NULL,relocs);
  op = NewOperand(IMM_BINARY, (const char *)fb, (intptr_t)relocs);
  EmitOp2(irl, OPC_LABELED_BLOB, ModData(P)->datlabel, op);
}

void
EmitVarSection(IRList *irl, Module *P)
{
    int len;
    if (!objlabel)
        return;
    len = P->varsize;
    len = (len+3) & ~3; // round up to long boundary
    EmitLabel(irl, objlabel);
    if (!len)
        len = 1;
    if (HUB_DATA && (gl_optimize_flags & OPT_REMOVE_HUB_BSS)) {
        EmitReserve(irl, 1, HUB_RESERVE);
    } else {
        EmitReserve(irl, len / 4, HUB_DATA ? HUB_RESERVE : COG_RESERVE);
    }
}

extern Module *globalModule;

/*
 * emit a small main program
 * it looks something like:
 *
 * entry:
 *         mov arg1, par wz
 *   if_nz jmp spininit
 *         call P->firstfunc using stackcall
 * exit:
 *         cogstop(cogid)
 * spininit:
 *         mov sp, arg1
 *         mov objbase, (sp)
 *         add sp, #4
 *         mov pc, sp
 *         add sp #4
 *         call result1 using stackcall
 *         jmp  #exit
 */
static void
EmitMain_P1(IRList *irl, Module *P)
{
    IR *ir;
    Function *firstfunc;
    const char *firstfuncname;
    Operand *spinlabel;
    Operand *const4 = NewImmediate(4);
    Operand *arg1;
    Operand *pc = NewOperand(REG_REG, "pc", 0);
    Operand *objptr = NewOperand(REG_REG, "objptr", 0);
    
    arg1 = GetOneGlobal(REG_ARG, "arg1", 0);
    firstfunc = P->functions;
    if (!firstfunc) {
        return;  // no functions at all
    }
    firstfunc->no_inline = 1; // make sure it is never inlined or removed
    firstfuncname = IdentifierGlobalName(P, firstfunc->name);
    
    spinlabel = NewOperand(IMM_COG_LABEL, "spininit", 0);
    cogexit = NewOperand(IMM_COG_LABEL, "cogexit", 0);
    hubexit = NewOperand(IMM_HUB_LABEL, "hubexit", 0);

    ir = EmitMove(irl, arg1, GetOneGlobal(REG_HW, "par", 0));
    ir->flags |= FLAG_WZ;

    if (HUB_CODE) {
        ValidateStackptr();
        ValidateObjbase();
        EmitJump(irl, COND_NE, spinlabel);
        if ( (gl_optimize_flags & OPT_REMOVE_HUB_BSS) ) {
            EmitOp2(irl, OPC_ADD, stackptr, objbase);
        }
    }

    if (firstfunc->cog_code || COG_CODE) {
        EmitOp1(irl, OPC_CALL, NewOperand(IMM_COG_LABEL, firstfuncname, 0));
    } else {
        EmitOp1(irl, OPC_CALL, NewOperand(IMM_HUB_LABEL, firstfuncname, 0));
    }
    EmitLabel(irl, cogexit);
    EmitOp1(irl, OPC_COGID, arg1);
    EmitOp1(irl, OPC_COGSTOP, arg1);

    if (HUB_CODE) {
        if (COG_DATA) {
            // the stack code below will fail to compile if --data=cog
            ERROR(NULL, "The combination --code=hub --data=cog is not supported");
            return;
        }
        EmitLabel(irl, spinlabel);
        EmitMove(irl, stackptr, arg1);
        EmitMove(irl, objptr, stacktop);
        EmitOp2(irl, OPC_ADD, stackptr, const4);
        EmitMove(irl, pc, stacktop);      // get address of function
        EmitMove(irl, stacktop, hubexit); // set return address
        EmitOp2(irl, OPC_ADD, stackptr, const4);
        EmitJump(irl, COND_TRUE, NewOperand(IMM_COG_LABEL, "LMM_LOOP", 0));
    }
}

static void
EmitMain_P2(IRList *irl, Module *P)
{
    Function *firstfunc;
    const char *firstfuncname;
    
    arg1 = GetOneGlobal(REG_ARG, "arg1", 0);
    firstfunc = P->functions;
    if (!firstfunc) {
        return;  // no functions at all
    }
    firstfunc->no_inline = 1; // make sure it is never inlined or removed
    firstfuncname = IdentifierGlobalName(P, firstfunc->name);
    
    ValidateStackptr();
    ValidateObjbase();
    cogexit = NewOperand(IMM_COG_LABEL, "cogexit", 0);
    hubexit = NewOperand(IMM_HUB_LABEL, "hubexit", 0);

    EmitMove(irl, stackptr, stacklabel);
    EmitOp1(irl, OPC_HUBSET, NewImmediate(255));
    if (firstfunc->cog_code || COG_CODE) {
        EmitOp1(irl, OPC_CALL, NewOperand(IMM_COG_LABEL, firstfuncname, 0));
    } else {
        EmitOp1(irl, OPC_CALL, NewOperand(IMM_HUB_LABEL, firstfuncname, 0));
    }
    EmitLabel(irl, cogexit);
    EmitOp1(irl, OPC_COGID, arg1);
    EmitOp1(irl, OPC_COGSTOP, arg1);
}

/*
 * .cog.spin main program looks like:
 * 
 */
#define MAX_COGSPIN_ARGS 16

void
EmitMain_CogSpin(IRList *irl, Module *p, int maxArgs)
{
    IR *ir;
    Operand *arg[MAX_COGSPIN_ARGS];
    Operand *par = GetOneGlobal(REG_HW, "par", 0);
    Operand *const4 = NewImmediate(4);
    Operand *mboxptr = GetOneGlobal(REG_REG, "mboxptr", 0);
    Operand *mboxcmd = GetOneGlobal(REG_REG, "mboxcmd", 0);
    Operand *waitloop;
    Operand *pasm__init;
    
    int i;
    if (maxArgs > MAX_COGSPIN_ARGS) {
        ERROR(NULL, ".cog.spin functions may not have more than %d arguments", MAX_COGSPIN_ARGS);
        maxArgs = MAX_COGSPIN_ARGS;
    }
    for (i = 0; i < maxArgs; i++) {
        char temp[20];
        sprintf(temp, "arg%d", i+1);
        arg[i] = GetOneGlobal(REG_ARG, strdup(temp), 0);
    }
    // always have at least 2 arguments
    arg1 = arg[0];
    arg2 = arg[1];
    result1 = GetOneGlobal(REG_REG, "result1", 0);

    if (gl_p2) {
        stackptr = GetOneGlobal(REG_HW, "ptra", 0);
        objbase = GetOneGlobal(REG_REG, "objptr", 0);
    } else {
        stackptr = GetOneGlobal(REG_REG, "sp", 0);
        objbase = GetOneGlobal(REG_REG, "objptr", 0);
        linkreg = GetOneGlobal(REG_REG, "linkreg", 0);
    }
    stacktop = SizedHubMemRef(LONG_SIZE, stackptr, 0);

    // mov mboxptr, par
    EmitMove(irl, mboxptr, par);
    // add mboxptr, #4 ' skip over lock
    EmitOp2(irl, OPC_ADD, mboxptr, const4);

    waitloop = NewOperand(IMM_COG_LABEL, "waitloop", 0);
    EmitLabel(irl, waitloop);
    ir = EmitOp2(irl, OPC_RDLONG, mboxcmd, mboxptr);
    ir->flags |= FLAG_WZ;
    EmitJump(irl, COND_EQ, waitloop);
    for (i = 0; i < maxArgs; i++) {
        EmitOp2(irl, OPC_ADD, mboxptr, const4);
        EmitOp2(irl, OPC_RDLONG, arg[i], mboxptr);
    }
    if (maxArgs > 1) {
        EmitOp2(irl, OPC_SUB, mboxptr, NewImmediate((maxArgs-1)*4));
    } else if (maxArgs == 0) {
        EmitOp2(irl, OPC_ADD, mboxptr, const4);
    }
    // divide address by 4 to convert from cog to hub
    EmitOp2(irl, OPC_SHR, mboxcmd, NewImmediate(2));
    // call command
    if (gl_p2) {
        EmitOp1(irl, OPC_CALL, mboxcmd);
    } else {
        //EmitJump(irl, COND_TRUE, mboxcmd);
        EmitOp2(irl, OPC_JMPRET, linkreg, mboxcmd);
    }
    // write back the result
    EmitOp2(irl, OPC_WRLONG, result1, mboxptr);
    EmitOp2(irl, OPC_SUB, mboxptr, const4);
    EmitMove(irl, arg1, NewImmediate(0));
    EmitOp2(irl, OPC_WRLONG, arg1, mboxptr);
    EmitJump(irl, COND_TRUE, waitloop);

    pasm__init = NewOperand(IMM_COG_LABEL, "pasm__init", 0);
    EmitLabel(irl, pasm__init);
    EmitMove(irl, objbase, arg1);
    EmitMove(irl, stackptr, arg2);
    if (gl_p2) {
        EmitOp0(irl, OPC_RET);
    } else {
        EmitJump(irl, COND_TRUE, linkreg);
    }
}

void
OutputAsmCode(const char *fname, Module *P, int outputMain)
{
    FILE *f = NULL;
    Module *save;
    IR *orgh;
    Operand *entrylabel = NewOperand(IMM_COG_LABEL, ENTRYNAME, 0);
    Operand *cog_bss_start = NewOperand(IMM_COG_LABEL, "COG_BSS_START", 0);

    unsigned int clkfreq, clkreg;
    const char *asmcode;
    int maxargs = 2; // initialization code wants 2 arguments
    
    save = current;
    current = P;

    memset(&cogcode, 0, sizeof(cogcode));
    memset(&hubcode, 0, sizeof(hubcode));
    memset(&cogdata, 0, sizeof(cogdata));
    memset(&hubdata, 0, sizeof(hubdata));
    memset(&cogbss, 0, sizeof(cogbss));
    
    if (gl_output == OUTPUT_COGSPIN) {
        // count max. number of arguments in any public function
        // also try to guess at stack size required
        Function *func;
        Symbol *stackSym;
        int mboxSize;
        int stackSize = 1;
        int maxLeafSize = 0;
        func = P->functions;
#define STACK_SIZE_NAME "__STACK_SIZE"
        stackSym = FindSymbol(&P->objsyms, STACK_SIZE_NAME);
        while (func) {
            if (func->is_public) {
                if (func->numparams > maxargs) {
                    maxargs = func->numparams;
                }
            }
            if (func->local_address_taken || func->is_recursive || func->cog_task) {
                int savesize = LocalSize(func) / LONG_SIZE;
                if (func->is_recursive) {
                    savesize *= 16;
                }
                if (func->is_leaf) {
                    if (maxLeafSize < savesize) maxLeafSize = savesize;
                } else {
                    stackSize += savesize;
                }
                stackSize += maxLeafSize;
            }

            func = func->next;            
        }
        mboxSize = maxargs + 3;
        EmitOp2(&cogcode, OPC_CONST, NewOperand(IMM_STRING, "__MBOX_SIZE", mboxSize), NewImmediate(mboxSize));

        if (!stackSym) {
            EmitOp2(&cogcode, OPC_CONST, NewOperand(IMM_STRING, STACK_SIZE_NAME, stackSize), NewImmediate(stackSize));
        }
    }
    EmitOp1(&cogbss, OPC_ORG, cog_bss_start);
    CompileConsts(&cogcode, P->conblock);

    // output the main stub
    EmitLabel(&cogcode, entrylabel);
    if (gl_output == OUTPUT_COGSPIN) {
        EmitMain_CogSpin(&cogcode, P, maxargs);
    } else if (outputMain) {
        if (gl_p2) {
            EmitMain_P2(&cogcode, P);
        } else {
            EmitMain_P1(&cogcode, P);
        }
    }
    // compile COG functions
    if (COG_CODE) {
        if (!CompileToIR(&cogcode, P)) {
            return;
        }
    }
    if (HUB_CODE) {
        ValidateStackptr();
        if (!gl_p2) {
            EmitLabel(&hubcode, NewOperand(IMM_HUB_LABEL, "hub_ret_to_cog", 0));
            EmitJump(&hubcode, COND_TRUE,
                     NewOperand(IMM_COG_LABEL, "LMM_CALL_FROM_COG_ret", 0));
        }
        EmitLabel(&hubcode, NewOperand(IMM_HUB_LABEL, "hubentry", 0));
        if (!CompileToIR(&hubcode, P)) {
            return;
        }
    }
    if (hubexit) {
        EmitLabel(&hubcode, hubexit);
        EmitJump(&hubcode, COND_TRUE, cogexit);
    }
    EmitBuiltins(&cogcode);
    // we compiled builtin functions into IR form earlier, now
    // output them
#if 0
    // these always go in COG memory
    CompileToIR_internal(&cogcode, globalModule);
#else
    if (HUB_CODE && gl_compressed) {
        CompileToIR_internal(&hubcode, globalModule);
    } else {
        CompileToIR_internal(&cogcode, globalModule);
    }
#endif
    // now copy the hub code into place
    orgh = EmitOp0(&cogcode, OPC_HUBMODE);
    if (gl_p2) {
        // on P2, make room for CLKFREQ and CLKMODE
        if (!GetClkFreq(P, &clkfreq, &clkreg)) {
            clkfreq = 80000000;
            clkreg = 0x64;
        }
        EmitLong(&cogcode, clkfreq);
        EmitLong(&cogcode, clkreg);
    }

    AppendIR(&cogcode, hubcode.head);

    // we have to optimize all code before emitting any variables
    OptimizeIRGlobal(&cogcode);

    // cog data
    EmitGlobals(&cogdata, &cogbss, &hubdata);

    // COG bss
    // FCACHE space
    if (HUB_CODE) {
        EmitNamedCogLabel(&cogbss, "LMM_RETREG");
        EmitReserve(&cogbss, 1, COG_RESERVE);
        EmitNamedCogLabel(&cogbss, "LMM_FCACHE_START");
        EmitReserve(&cogbss, gl_fcache_size+1, COG_RESERVE);
    }
              
    // we need to emit all dat sections
    VisitRecursive(&hubdata, P, EmitDatSection, VISITFLAG_EMITDAT);

    // only the top level variable space is needed
    EmitVarSection(&hubdata, P);
    
    // finally the stack, if we need it
    if (stacklabel) {
        if (HUB_DATA) {
            EmitLabel(&hubdata, stacklabel);
            EmitReserve(&hubdata, 1, HUB_RESERVE);
        } else {
            EmitLabel(&cogbss, stacklabel);
            EmitReserve(&cogbss, 1, COG_RESERVE);
        }
    }

    // now insert the cog data after the cog code, before the orgh
    EmitLabel(&cogdata, cog_bss_start);
    EmitOp0(&cogdata, OPC_FIT);
    
    EmitOp0(&cogbss, OPC_FIT);
    InsertAfterIR(&cogcode, orgh->prev, cogdata.head);
    
    // and the hub data at the end
    AppendIR(&cogcode, hubdata.head);
    // now the cog bss (which doesn't need any actual space
    AppendIR(&cogcode, cogbss.head);

    // and assemble the result
    asmcode = IRAssemble(&cogcode, P);
    
    current = save;

    f = fopen(fname, "w");
    if (!f) {
        fprintf(stderr, "Unable to open pasm output: ");
        perror(fname);
        exit(1);
    }
    // write a header if appropriate
    if (gl_output == OUTPUT_COGSPIN && gl_header1) {
        fprintf(f, "'' %s", gl_header1);
        fprintf(f, "'' %s", gl_header2);
    }
    fwrite(asmcode, 1, strlen(asmcode), f);
    fclose(f);
}
