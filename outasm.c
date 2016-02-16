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

Operand *
CreateTempLabel(IRList *irl)
{
  Operand *label;
  static int lnum = 1;
  char temp[128];
  sprintf(temp, "L_%03d_", lnum);
  lnum++;
  label = NewOperand(REG_LABEL, strdup(temp), 0);
  return label;
}

void EmitLong(IRList *irl, int val)
{
  Operand *op;

  op = NewOperand(REG_IMM, "", val);
  EmitOp1(irl, OPC_LONG, op);
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
GetFunctionTempRegister(Function *f, int n)
{
  ParserState *P = f->parse;
  char buf[128];
  sprintf(buf, "%s_%s_tmp%03d_", P->basename, f->name, n);
  return GetGlobal(REG_LOCAL, buf, 0);
}

Operand *
NewFunctionTempRegister()
{
  Function *f = curfunc;

  f->curtempreg++;
  if (f->curtempreg > f->maxtempreg) {
    f->maxtempreg = f->curtempreg;
  }
  return GetFunctionTempRegister(f, f->curtempreg);
}

Operand *
CompileIdentifierForFunc(IRList *irl, AST *expr, Function *func)
{
  ParserState *P = func->parse;
  Symbol *sym = FindSymbol(&func->localsyms, expr->d.string);
  char temp[128];
  snprintf(temp, sizeof(temp)-1, "%s_%s_%s_", P->basename, func->name, expr->d.string);

  if (sym && sym->type == SYM_PARAMETER) {
    return GetGlobal(REG_ARG, temp, 0);
  }
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

static IRCond
CondFromExpr(int kind)
{
  switch (kind) {
  case T_NE:
    return COND_NE;
  case T_EQ:
    return COND_EQ;
  case T_GE:
    return COND_GE;
  case T_LE:
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
static IRCond
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

  if (expr->kind == AST_OPERATOR) {
    opkind = expr->d.ival;
  } else {
    opkind = -1;
  }
  switch(opkind) {
  case T_NE:
  case T_EQ:
  case '<':
  case '>':
  case T_LE:
  case T_GE:
    cond = CondFromExpr(opkind);
    lhs = CompileExpression(irl, expr->left);
    rhs = CompileExpression(irl, expr->right);
    break;
  default:
    cond = COND_NE;
    lhs = CompileExpression(irl, expr);
    rhs = NewOperand(REG_IMM, "", 0);
    break;
  }
  /* emit a compare operator */
  /* note that lhs cannot be a constant */
  if (lhs && lhs->kind == REG_IMM) {
    Operand *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
    cond = FlipSides(cond);
  }
  EmitOp2(irl, OPC_CMP, lhs, rhs);
  return cond;
}

void
CompileBoolBranches(IRList *irl, AST *expr, Operand *truedest, Operand *falsedest)
{
    IRCond cond;
    int opkind;
    Operand *dummylabel = NULL;
    
    if (expr->kind == AST_OPERATOR) {
        opkind = expr->d.ival;
    } else {
        opkind = -1;
    }

    switch (opkind) {
    case T_NOT:
        CompileBoolBranches(irl, expr->right, falsedest, truedest);
        break;
    case T_AND:
        if (!falsedest) {
            dummylabel = CreateTempLabel(irl);
            falsedest = dummylabel;
        }
        CompileBoolBranches(irl, expr->left, NULL, falsedest);
        CompileBoolBranches(irl, expr->right, truedest, falsedest);
        if (dummylabel) {
            EmitLabel(irl, dummylabel);
        }
        break;
    case T_OR:
        if (!truedest) {
            dummylabel = CreateTempLabel(irl);
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
  Operand *val;

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
  case AST_ASSIGN:
    r = CompileExpression(irl, expr->left);
    val = CompileExpression(irl, expr->right);
    EmitMove(irl, r, val);
    return r;
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
    Operand *botloop, *toploop;
    int starttempreg, endtempreg;

    if (!ast) return;

    starttempreg = curfunc->curtempreg;
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
	EmitJump(irl, COND_TRUE, curfunc->asmretname);
	break;
    case AST_WHILE:
        toploop = CreateTempLabel(irl);
	botloop = CreateTempLabel(irl);
	EmitLabel(irl, toploop);
        CompileBoolBranches(irl, ast->left, NULL, botloop);
        EmitStatementList(irl, ast->right);
	EmitJump(irl, COND_TRUE, toploop);
	EmitLabel(irl, botloop);
	break;
    case AST_DOWHILE:
        toploop = CreateTempLabel(irl);
	EmitLabel(irl, toploop);
        EmitStatementList(irl, ast->right);
        CompileBoolBranches(irl, ast->left, toploop, NULL);
	break;
    case AST_IF:
        toploop = CreateTempLabel(irl);
        CompileBoolBranches(irl, ast->left, NULL, toploop);
	ast = ast->right;
	if (ast->kind == AST_COMMENTEDNODE) {
	  ast = ast->left;
	}
	/* ast should be an AST_THENELSE */
	EmitStatementList(irl, ast->left);
	if (ast->right) {
	  botloop = CreateTempLabel(irl);
	  EmitJump(irl, COND_TRUE, botloop);
	  EmitLabel(irl, toploop);
	  EmitStatementList(irl, ast->right);
	  EmitLabel(irl, botloop);
	} else {
	  EmitLabel(irl, toploop);
	}
	break;
    case AST_YIELD:
	/* do nothing in assembly for YIELD */
        break;
    default:
        /* assume an expression */
        (void)CompileExpression(irl, ast);
        break;
    }
    /* release temporaries we used */
    endtempreg = curfunc->curtempreg;
    curfunc->curtempreg = starttempreg;

    /* and mark them as dead */
    while (endtempreg > starttempreg) {
      op = GetFunctionTempRegister(curfunc, endtempreg);
      EmitOp1(irl, OPC_DEAD, op);
      --endtempreg;
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

    // assign all function names so we can do forward calls
    for(f = P->functions; f; f = f->next) {
	char *fname;
        char *frname;
	frname = malloc(strlen(f->name) + strlen(P->basename) + 8);
	sprintf(frname, "%s_%s", P->basename, f->name);
	fname = strdup(frname);
	strcat(frname, "_ret");

        f->asmname = NewOperand(REG_LABEL, fname, 0);
        f->asmretname = NewOperand(REG_LABEL, frname, 0);
    }
    
    // now compile the functions
    for(f = P->functions; f; f = f->next) {
	if (newlineOp) {
  	    EmitNewline(irl);
	}
        EmitWholeFunction(irl, f);
	newlineOp = NewOperand(REG_STRING, "\n", 0);
    }
    return gl_errors == 0;
}

bool
InstrReadsDst(int opc)
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
IsLocalOrArg(Operand *op)
{
  return op->kind == REG_LOCAL || op->kind == REG_ARG;
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

static bool IsDummy(IR *op)
{
  switch(op->opc) {
  case OPC_COMMENT:
  case OPC_DEAD:
    return true;
  default:
    return false;
  }
}

/*
 * return TRUE if the operand's value does not need to be preserved
 * after instruction instr
 */
bool
IsDead(IR *instr, Operand *op)
{
  IR *ir;

  if (!IsLocalOrArg(op)) {
    return false;
  }
  if (instr->opc == OPC_DEAD && op == instr->dst) {
    return true;
  }
  for (ir = instr->next; ir; ir = ir->next) {
    if (ir->opc == OPC_DEAD && op == ir->dst) {
      return true;
    }
    if (IsDummy(ir)) continue;
    if (ir->opc == OPC_LABEL) {
      continue;
    }
    if (ir->opc == OPC_RET) {
      return true;
    }
    if (ir->opc == OPC_JUMP) {
      return false;
    }
    if (ir->opc == OPC_CALL && op->kind == REG_ARG) {
      return false;
    }
    if (ir->src == op) {
      return false;
    }
    if (ir->dst == op) {
      /* the value is unused for certain opcodes */
      if (InstrReadsDst(ir->opc)) {
	return false;
      }
    }
  }
  /* if we reach the end without seeing any use */
  return true;
}

bool
InstrSetsDst(int opc)
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
    if (IsDummy(ir)) {
      continue;
    }
    if (ir->opc == OPC_LABEL) {
      return false;
    }
    if (ir->dst == orig && InstrSetsDst(ir->opc) && !InstrReadsDst(ir->opc)) {
      return ir->cond == COND_TRUE;
    }
    if (ir->src == replace || ir->dst == replace) {
      return false;
    }
  }
  return false;
}

static bool
SafeToReplaceForward(IR *first_ir, Operand *orig, Operand *replace)
{
  IR *ir;
  for (ir = first_ir; ir; ir = ir->next) {
    if (IsDummy(ir)) {
	continue;
    }
    if (ir->opc == OPC_RET) {
      return IsLocalOrArg(orig);
    } else if (IsBranch(ir->opc)) {
      return false;
    }
    if (ir->opc == OPC_LABEL) {
      if (IsDead(ir->next, orig) && IsDead(ir->next, replace)) return true;
      return false;
    }
    if (ir->dst == replace) {
      // special case: if we have a "mov replace,orig" and orig is dead
      // then we are good to go
      if (ir->opc == OPC_MOVE && ir->src == orig && IsDead(ir->next, orig) && ir->cond == COND_TRUE) {
	return true;
      }
      return false;
    }
    if (ir->src == replace && ir != first_ir) {
      return false;
    }
  }
  return IsLocalOrArg(orig);
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
      if (InstrSetsDst(ir->opc) && !InstrReadsDst(ir->opc)) {
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
    if (IsDummy(ir)) {
      continue;
    }
    if (ir->opc == OPC_RET || ir->opc == OPC_LABEL) {
      return;
    } else if (IsBranch(ir->opc)) {
      return;
    }
    if (ir->src == orig) {
      ir->src = replace;
      if (ir->dst == replace && ir->opc == OPC_MOVE) {
	return;
      }
    }
    if (ir->dst == orig) {
      ir->dst = replace;
    }
  }
}

void
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

void EliminateDeadCode(IRList *irl)
{
  bool change;
  IR *ir, *ir_next;
  do {
    change = false;
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
	      change = true;
	    }
	    x = ir_next;
	  }
	}
	/* if the branch is to the next instruction, delete it */
	if (ir_next && ir_next->opc == OPC_LABEL && ir_next->dst == ir->dst) {
	  DeleteIR(irl, ir);
	  change = true;
	}
      } else if (!IsDummy(ir)) {
	if (ir_next && ir->dst && IsDead(ir, ir->dst)) {
	  DeleteIR(irl, ir);
	  change = true;
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
    if (IsDummy(ir)) {
      continue;
    }
    CheckOpUsage(ir->src);
    CheckOpUsage(ir->dst);
  }
}

/* checks for short forward (conditional) jumps
 * returns the number of instructions forward
 * or 0 if not a valid candidate for optimization
 */
#define MAX_JUMP_OVER 2
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
  return n;
}

void ConditionalizeInstructions(IR *ir, IRCond cond, int n)
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

static void
MarkLabelIfUnused(IRList *irl, Operand *label)
{
  IR *target = 0;
  IR *ir;
  for (ir = irl->head; ir; ir = ir->next) {
    if (!IsDummy(ir)) {
      if (ir->src == label) return;
      if (ir->dst == label) {
	if (ir->opc == OPC_LABEL) {
	  target = ir; // this is the label we may want to delete
	} else {
	  return;
	}
      }
    }
  }
  if (target) {
    // we can't actually delete because the caller may hold
    // a pointer to this label as ir_next
    // so just mark it as a dummy
    target->opc = OPC_DEAD;
  }
}

void OptimizeShortBranches(IRList *irl)
{
  IR *ir;
  IR *ir_next;
  int n;

  ir = irl->head;
  while (ir) {
    ir_next = ir->next;
    n = IsShortForwardJump(ir);
    if (n) {
      ConditionalizeInstructions(ir->next, InvertCond(ir->cond), n);
      DeleteIR(irl, ir);
      /* now see if the label is unused */
      MarkLabelIfUnused(irl, ir->dst);
    }
    ir = ir_next;
  }
}

void
OptimizeIR(IRList *irl)
{
  if (gl_optimize_flags & OPT_NO_ASM) return;
  EliminateDeadCode(irl);
  OptimizeMoves(irl);
  OptimizeShortBranches(irl);
  CheckUsage(irl);
}
