//
// Pasm data output for spin2cpp
//
// Copyright 2016 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "spinc.h"
#include "ir.h"
#include "flexbuf.h"

typedef struct OperandList {
  struct OperandList *next;
  Operand *op;
} OperandList;

Operand *NewOperand(enum Operandkind, const char *name, int val);
Operand *CompileExpression(IRList *irl, AST *expr);

void OptimizeIR(IRList *irl);
void EmitGlobals(IRList *irl);
static void EmitMove(IRList *irl, Operand *dst, Operand *src);

struct GlobalVariable {
    Operand *op;
    int val;
};

static struct flexbuf gvars;

Operand *
GetGlobal(Operandkind kind, const char *name, int value)
{
  size_t siz = flexbuf_curlen(&gvars) / sizeof(struct GlobalVariable);
  size_t i;
  struct GlobalVariable tmp;
  struct GlobalVariable *g = (struct GlobalVariable *)flexbuf_peek(&gvars);
  for (i = 0; i < siz; i++) {
    if (strcmp(name, g[i].op->name) == 0) {
      return g[i].op;
    }
  }
  tmp.op = NewOperand(kind, strdup(name), value);
  tmp.val = value;
  flexbuf_addmem(&gvars, (const char *)&tmp, sizeof(tmp));
  return tmp.op;
}

void
OutputAsmCode(const char *fname, ParserState *P)
{
    FILE *f = NULL;
    ParserState *save;
    IRList irl;
    const char *asmcode;
    
    save = current;
    current = P;

    if (!CompileToIR(&irl, P)) {
        return;
    }
    OptimizeIR(&irl);
    EmitGlobals(&irl);
    asmcode = IRAssemble(&irl);
    
    current = save;

    f = fopen(fname, "wb");
    if (!f) {
        perror(fname);
        exit(1);
    }
    fwrite(asmcode, 1, strlen(asmcode), f);
    fclose(f);
}

IR *NewIR(enum IROpcode kind)
{
    IR *ir = malloc(sizeof(*ir));
    memset(ir, 0, sizeof(*ir));
    ir->opc = kind;
    return ir;
}

Operand *NewOperand(enum Operandkind k, const char *name, int value)
{
    Operand *R = malloc(sizeof(*R));
    memset(R, 0, sizeof(*R));
    R->kind = k;
    R->name = name;
    R->val = value;
    return R;
}

/*
 * append some IR to the end of a list
 */
void
AppendIR(IRList *irl, IR *ir)
{
    IR *last = irl->tail;
    if (!ir) return;
    if (!last) {
        irl->head = irl->tail = ir;
        return;
    }
    last->next = ir;
    ir->prev = last;
    while (ir->next) {
        ir = ir->next;
    }
    irl->tail = ir;
}

void
DeleteIR(IRList *irl, IR *ir)
{
  IR *prev = ir->prev;
  IR *next = ir->next;

  if (prev) {
    prev->next = next;
  } else {
    irl->head = ir;
  }
  if (next) {
    next->prev = prev;
  } else {
    irl->tail = prev;
  }
}

static void EmitOp0(IRList *irl, Operandkind code)
{
  IR *ir = NewIR(code);
  AppendIR(irl, ir);
}

static void EmitOp1(IRList *irl, Operandkind code, Operand *op)
{
  IR *ir = NewIR(code);
  ir->dst = op;
  AppendIR(irl, ir);
}

static Operand *EmitOp2(IRList *irl, Operandkind code, Operand *d, Operand *s)
{
  IR *ir = NewIR(code);
  ir->dst = d;
  ir->src = s;
  AppendIR(irl, ir);
  return d;
}

void EmitLabel(IRList *irl, Operand *op)
{
  EmitOp1(irl, OPC_LABEL, op);
}

void EmitLong(IRList *irl, int val)
{
  Operand *op;

  op = NewOperand(REG_IMM, "", val);
  EmitOp1(irl, OPC_LONG, op);
}

void EmitFunctionProlog(IRList *irl, Function *f)
{
  EmitLabel(irl, f->asmname);
}

void EmitFunctionEpilog(IRList *irl, Function *f)
{
    EmitLabel(irl, f->asmretname);
    EmitOp0(irl, OPC_RET);
}

Operand *
NewImmediate(int32_t val)
{
  char temp[64];
  if (val >= 0 && val < 512) {
    return NewOperand(REG_IMM, "", (int32_t)val);
  }
  sprintf(temp, "imm_%u_", (unsigned)val);
  return GetGlobal(REG_REG, temp, (int32_t)val);
}

Operand *
NewFunctionTempRegister()
{
  char buf[128];
  Function *f = curfunc;
  ParserState *P = f->parse;

  f->curtempreg++;
  if (f->curtempreg > f->maxtempreg) {
    f->maxtempreg = f->curtempreg;
  }
  sprintf(buf, "%s_%s_tmp%03d_", P->basename, f->name, f->curtempreg);
  return GetGlobal(REG_LOCAL, buf, 0);
}

Operand *
CompileIdentifierForFunc(IRList *irl, AST *expr, Function *func)
{
  ParserState *P = func->parse;
  char temp[128];
  snprintf(temp, sizeof(temp)-1, "%s_%s_%s_", P->basename, func->name, expr->d.string);
  return GetGlobal(REG_LOCAL, temp, 0);
}

Operand *
CompileIdentifier(IRList *irl, AST *expr)
{
  return CompileIdentifierForFunc(irl, expr, curfunc);
}

Operand *
CompileHWReg(IRList *irl, AST *expr)
{
  HwReg *hw = (HwReg *)expr->d.ptr;
  return GetGlobal(REG_HW, hw->cname, 0);
}

static int
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
  case T_SHL:
    return OPC_SHL;
  case T_SAR:
    return OPC_SAR;
  case T_SHR:
    return OPC_SHR;
  case T_NEGATE:
    return OPC_NEG;
  case T_ABS:
    return OPC_ABS;
  default:
    ERROR(NULL, "Unsupported operator %d", op);
    return OPC_UNKNOWN;
  }
}

Operand *
CompileOperator(IRList *irl, int op, AST *lhs, AST *rhs)
{
  Operand *left = CompileExpression(irl, lhs);
  Operand *right = CompileExpression(irl, rhs);
  Operand *temp = NewFunctionTempRegister();

  switch(op) {
  case '+':
  case '-':
  case '^':
  case '&':
  case '|':
  case T_SHL:
  case T_SHR:
  case T_SAR:
    EmitOp2(irl, OPC_MOVE, temp, left);
    return EmitOp2(irl, OpcFromOp(op), temp, right);
  case T_NEGATE:
  case T_ABS:
    return EmitOp2(irl, OpcFromOp(op), temp, right);
  default:
    ERROR(lhs, "Unsupported operator %d", op);
    return left;
  }
}

void
AppendOperand(OperandList **listptr, Operand *op)
{
  OperandList *next = malloc(sizeof(OperandList));
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

/* compiles a list of expressions; returns the last one
 * tolist is a list of where to store the expressions
 * (may be NULL if we don't care about saving them)
 */
OperandList *
CompileExprList(IRList *irl, AST *fromlist)
{
  AST *from;
  Operand *opfrom = NULL;
  Operand *opto = NULL;
  OperandList *ret = NULL;

  while (fromlist) {
    from = fromlist->left;
    fromlist = fromlist->right;

    opfrom = CompileExpression(irl, from);
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
  AST *astlist = func->params;
  AST *ast;
  Operand *op;
  Operand *dst;

  while (oplist != NULL && astlist != NULL) {
    op = oplist->op;
    ast = astlist->left;
    oplist = oplist->next;
    astlist = astlist->right;

    dst = CompileIdentifierForFunc(irl, ast, func);
    EmitMove(irl, dst, op);
  }
}

Operand *
CompileFunccall(IRList *irl, AST *expr)
{
  Operand *result;
  Symbol *sym;
  Function *func;
  AST *params;
  OperandList *temp;

  /* compile the function operands */
  sym = FindFuncSymbol(expr, NULL, NULL);
  if (!sym || sym->type != SYM_FUNCTION) {
    ERROR(expr, "expected function symbol");
    return NULL;
  }
  func = (Function *)sym->val;
  params = expr->right;
  temp = CompileExprList(irl, params);

  /* now copy the parameters into place (have to do this in case there are
     function calls within the parameters)
   */
  EmitParameterList(irl, temp, func);

  /* emit the call */
  EmitOp1(irl, OPC_CALL, func->asmname);

  /* now get the result */
  result = GetGlobal(REG_REG, "result_", 0);
  return result;
}

Operand *
CompileExpression(IRList *irl, AST *expr)
{
  Operand *r;

  while (expr && expr->kind == AST_COMMENTEDNODE) {
    expr = expr->left;
  }
  if (!expr) return NULL;

  switch (expr->kind) {
  case AST_INTEGER:
  case AST_FLOAT:
    r = NewImmediate((int32_t)expr->d.ival);
    return r;
  case AST_IDENTIFIER:
    return CompileIdentifier(irl, expr);
  case AST_HWREG:
    return CompileHWReg(irl, expr);
  case AST_OPERATOR:
    return CompileOperator(irl, expr->d.ival, expr->left, expr->right);
  case AST_FUNCCALL:
    return CompileFunccall(irl, expr);
  default:
    ERROR(expr, "Cannot handle expression yet");
    return NewOperand(REG_REG, "???", 0);
  }
}

static void EmitStatement(IRList *irl, AST *ast); /* forward declaration */

static void
EmitStatementList(IRList *irl, AST *ast)
{
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error: expected statement list, got %d",
                  ast->kind);
            return;
        }
        EmitStatement(irl, ast->left);
        ast = ast->right;
    }
}

static void EmitMove(IRList *irl, Operand *dst, Operand *src)
{
  EmitOp2(irl, OPC_MOVE, dst, src);
}

static void EmitStatement(IRList *irl, AST *ast)
{
    AST *retval;
    Operand *op;
    Operand *result;

    if (!ast) return;

    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        EmitStatement(irl, ast->left);
        break;
    case AST_RETURN:
        retval = ast->left;
        if (!retval) {
            retval = curfunc->resultexpr;
        }
	if (retval) {
	    op = CompileExpression(irl, retval);
	    result = GetGlobal(REG_REG, "result_", 0);
            EmitOp2(irl, OPC_MOVE, result, op);
	}
	EmitOp1(irl, OPC_JUMP, curfunc->asmretname);
	break;
    case AST_ASSIGN:
        result = CompileExpression(irl, ast->left);
	op = CompileExpression(irl, ast->right);
	EmitMove(irl, result, op);
	break;
    default:
        ERROR(ast, "Not yet able to handle this kind of statement");
	break;
    }
}

/*
 * compile a function to IR and put it at the end of the IRList
 */

void
EmitWholeFunction(IRList *irl, Function *f)
{
    curfunc = f;
    EmitFunctionProlog(irl, f);
    EmitStatementList(irl, f->body);
    EmitFunctionEpilog(irl, f);
}

Operand *newlineOp;
void EmitNewline(IRList *irl)
{
  IR *ir = NewIR(OPC_COMMENT);
  ir->dst = newlineOp;
  AppendIR(irl, ir);
}

static int gcmpfunc(const void *a, const void *b)
{
  const struct GlobalVariable *ga = (const struct GlobalVariable *)a;
  const struct GlobalVariable *gb = (const struct GlobalVariable *)b;
  return strcmp(ga->op->name, gb->op->name);
}

void EmitGlobals(IRList *irl)
{
    size_t siz = flexbuf_curlen(&gvars) / sizeof(struct GlobalVariable);
    size_t i;
    struct GlobalVariable *g = (struct GlobalVariable *)flexbuf_peek(&gvars);

    if (siz > 0) {
      EmitNewline(irl);
    }
    /* sort the global variables */
    qsort(g, siz, sizeof(*g), gcmpfunc);
    for (i = 0; i < siz; i++) {
      if (g[i].op->kind == REG_LOCAL && !g[i].op->used) {
	continue;
      }
      if (g[i].op->kind == REG_HW) {
	continue;
      }
      EmitLabel(irl, g[i].op);
      EmitLong(irl, g[i].val);
    }
}

bool
CompileToIR(IRList *irl, ParserState *P)
{
    Function *f;

    irl->head = NULL;
    irl->tail = NULL;

    for(f = P->functions; f; f = f->next) {
        char *frname;
	char *fname;
	frname = malloc(strlen(f->name) + strlen(P->basename) + 8);
	sprintf(frname, "%s_%s", P->basename, f->name);
	fname = strdup(frname);
	strcat(frname, "_ret");

        f->asmname = NewOperand(REG_LABEL, fname, 0);
        f->asmretname = NewOperand(REG_LABEL, frname, 0);
	if (newlineOp) {
  	    EmitNewline(irl);
	}
        EmitWholeFunction(irl, f);
	newlineOp = NewOperand(REG_STRING, "\n", 0);
    }
    return gl_errors == 0;
}

/*
 * return TRUE if the operand's value does not need to be preserved
 * after instruction instr
 */
bool
IsDead(IR *instr, Operand *op)
{
  IR *ir;

  if (op->kind != REG_LOCAL) {
    return false;
  }
  for (ir = instr->next; ir; ir = ir->next) {
    if (ir->opc == OPC_RET) {
      return true;
    }
    if (ir->src == op) {
      return false;
    }
    if (ir->dst == op) {
      /* the value is unused for certain opcodes */
      switch(ir->opc) {
      case OPC_NEG:
	break;
      default:
	return false;
      }
    }
  }
  return false;
}

bool
InstrReadsDest(int opc)
{
  switch (opc) {
  case OPC_MOVE:
  case OPC_NEG:
  case OPC_NOT:
  case OPC_ABS:
    return false;
  default:
    break;
  }
  return true;
}

bool
InstrSetsDest(int opc)
{
  switch (opc) {
  case OPC_CMP:
  case OPC_LABEL:
    return false;
  default:
    break;
  }
  return true;
}

static bool
SafeToReplaceBack(IR *instr, Operand *orig, Operand *replace)
{
  IR *ir;
  for (ir = instr; ir; ir = ir->prev) {
    if (ir->opc == OPC_LABEL) {
      return false;
    }
    if (ir->src == replace || ir->dst == replace) {
      return false;
    }
    if (ir->dst == orig && InstrSetsDest(ir->opc) && !InstrReadsDest(ir->opc)) {
      return ir->cond == COND_TRUE;
    }
  }
  return false;
}

static bool IsBranch(int opc)
{
  switch (opc) {
  case OPC_JUMP:
  case OPC_DJNZ:
  case OPC_CALL:
    return true;
  default:
    return false;
  }
}

static bool
SafeToReplaceForward(IR *first_ir, Operand *orig, Operand *replace)
{
  IR *ir;
  for (ir = first_ir; ir; ir = ir->next) {
    if (ir->opc == OPC_RET) {
      return orig->kind == REG_LOCAL;
    } else if (IsBranch(ir->opc)) {
      return false;
    }
    if (ir->dst == replace) {
      return false;
    }
    if (ir->src == replace && ir != first_ir) {
      return false;
    }
  }
  return orig->kind == REG_LOCAL;
}


static void
ReplaceBack(IR *instr, Operand *orig, Operand *replace)
{
  IR *ir;
  for (ir = instr; ir; ir = ir->prev) {
    if (ir->opc == OPC_LABEL) {
      break;
    }
    if (ir->dst == orig) {
      ir->dst = replace;
      if (InstrSetsDest(ir->opc) && !InstrReadsDest(ir->opc)) {
	break;
      }
    }
    if (ir->src == orig) {
      ir->src = replace;
    }
  }
}

static void
ReplaceForward(IR *instr, Operand *orig, Operand *replace)
{
  IR *ir;
  for (ir = instr; ir; ir = ir->next) {
    if (ir->opc == OPC_RET) {
      return;
    } else if (IsBranch(ir->opc)) {
      return;
    }
    if (ir->src == orig) {
      ir->src = replace;
    }
    if (ir->dst == orig) {
      ir->dst = replace;
    }
  }
}

static void
OptimizeMoves(IRList *irl)
{
  IR *ir;
  IR *ir_next;
  bool change;

  do {
    change = false;
    ir = irl->head;
    while (ir != 0) {
      ir_next = ir->next;
      if (ir->opc == OPC_MOVE) {
	if (ir->src == ir->dst) {
	  DeleteIR(irl, ir);
	  change = true;
	} else if (IsDead(ir->next, ir->src) && SafeToReplaceBack(ir->prev, ir->src, ir->dst)) {
	  ReplaceBack(ir->prev, ir->src, ir->dst);
	  DeleteIR(irl, ir);
	  change = true;
	}
	else if (SafeToReplaceForward(ir->next, ir->dst, ir->src)) {
	  ReplaceForward(ir->next, ir->dst, ir->src);
	  DeleteIR(irl, ir);
	  change = true;
	}
      }
      ir = ir_next;
    }
  } while (change);
}

static void EliminateDeadCode(IRList *irl)
{
  bool change = false;
  IR *ir, *ir_next;
  do {
    ir = irl->head;
    while (ir) {
      ir_next = ir->next;
      if (ir->opc == OPC_JUMP) {
	if (ir->cond == COND_TRUE) {
	  // dead code from here to next label
	  IR *x = ir->next;
	  while (x && x->opc != OPC_LABEL) {
	    ir_next = x->next;
	    DeleteIR(irl, x);
	    x = ir_next;
	  }
	}
	/* if the branch is to the next instruction, delete it */
	if (ir_next && ir_next->opc == OPC_LABEL && ir_next->dst == ir->dst) {
	  DeleteIR(irl, ir);
	}
      }
      ir = ir_next;
    }
  } while (change);
}

static void CheckOpUsage(Operand *op)
{
  if (op) {
    op->used = 1;
  }
}

void CheckUsage(IRList *irl)
{
  IR *ir;
  for (ir = irl->head; ir; ir = ir->next) {
    CheckOpUsage(ir->src);
    CheckOpUsage(ir->dst);
  }
}

void
OptimizeIR(IRList *irl)
{
  if (gl_optimize_flags & OPT_NO_ASM) return;
  EliminateDeadCode(irl);
  OptimizeMoves(irl);
  CheckUsage(irl);
}
