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

static Operand *mulfunc, *mula, *mulb;
static Operand *divfunc, *diva, *divb;

typedef struct OperandList {
  struct OperandList *next;
  Operand *op;
} OperandList;

Operand *NewOperand(enum Operandkind, const char *name, int val);
Operand *CompileExpression(IRList *irl, AST *expr);
static Operand* CompileMul(IRList *irl, AST *expr, int gethi);
static Operand* CompileDiv(IRList *irl, AST *expr, int getmod);

void OptimizeIRLocal(IRList *irl);
void OptimizeIRGlobal(IRList *irl);
void EmitGlobals(IRList *irl);
static void EmitMove(IRList *irl, Operand *dst, Operand *src);
static void EmitBuiltins(IRList *irl);
static void EmitOp1(IRList *irl, Operandkind code, Operand *op);
static void CompileConsts(IRList *irl, AST *consts);

struct GlobalVariable {
    Operand *op;
    int val;
};

static struct flexbuf gvars;

static int IsMemRef(Operand *op)
{
    return (op->kind >= LONG_REF) && (op->kind <= BYTE_REF);
}

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

    irl.head = NULL;
    irl.tail = NULL;

    CompileConsts(&irl, P->conblock);
    if (!CompileToIR(&irl, P)) {
        return;
    }
    OptimizeIRGlobal(&irl);
    EmitBuiltins(&irl);
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
  return GetGlobal(REG_IMM, temp, (int32_t)val);
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
  char temp[256];
  snprintf(temp, sizeof(temp)-1, "%s_%s_%s_", P->basename, func->name, expr->d.string);

  if (sym) {
      if (sym->type == SYM_PARAMETER) {
          return GetGlobal(REG_ARG, temp, 0);
      }
  }
  return GetGlobal(REG_LOCAL, temp, 0);
}

Operand *
CompileIdentifier(IRList *irl, AST *expr)
{
    Symbol *sym = LookupSymbol(expr->d.string);
    if (sym && sym->type == SYM_CONSTANT) {
          AST *symexpr = (AST *)sym->val;
          int val = EvalConstExpr(symexpr);
          if (val >= 0 && val < 512) {
              return NewOperand(REG_IMM, sym->name, val);
          } else {
              return NewImmediate(val);
          }
    }
    return CompileIdentifierForFunc(irl, expr, curfunc);
}

Operand *
CompileHWReg(IRList *irl, AST *expr)
{
  HwReg *hw = (HwReg *)expr->d.ptr;
  return GetGlobal(REG_HW, hw->cname, 0);
}

static Operand *
CompileMul(IRList *irl, AST *expr, int gethi)
{
  Operand *lhs = CompileExpression(irl, expr->left);
  Operand *rhs = CompileExpression(irl, expr->right);
  Operand *temp = NewFunctionTempRegister();

  if (!mulfunc) {
    mulfunc = NewOperand(REG_LABEL, "multiply_", 0);
    mula = GetGlobal(REG_ARG, "mula_", 0);
    mulb = GetGlobal(REG_ARG, "mulb_", 0);
  }
  EmitMove(irl, mula, lhs);
  EmitMove(irl, mulb, rhs);
  EmitOp1(irl, OPC_CALL, mulfunc);
  if (gethi) {
      EmitMove(irl, temp, mulb);
  } else {
      EmitMove(irl, temp, mula);
  }
  return temp;
}

static Operand *
CompileDiv(IRList *irl, AST *expr, int getmod)
{
  Operand *lhs = CompileExpression(irl, expr->left);
  Operand *rhs = CompileExpression(irl, expr->right);
  Operand *temp = NewFunctionTempRegister();

  if (!divfunc) {
    divfunc = NewOperand(REG_LABEL, "divide_", 0);
    diva = GetGlobal(REG_ARG, "mula_", 0);
    divb = GetGlobal(REG_ARG, "mulb_", 0);
  }
  EmitMove(irl, diva, lhs);
  EmitMove(irl, divb, rhs);
  EmitOp1(irl, OPC_CALL, divfunc);
  if (getmod) {
      EmitMove(irl, temp, divb);
  } else {
      EmitMove(irl, temp, diva);
  }
  return temp;
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
  EmitOp2(irl, OPC_CMPS, lhs, rhs);
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
  case T_BIT_NOT:
    return OPC_NOT;
  case T_ROTL:
      return OPC_ROL;
  case T_ROTR:
      return OPC_ROR;
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
CompileBasicOperator(IRList *irl, AST *expr)
{
  int op = expr->d.ival;
  AST *lhs = expr->left;
  AST *rhs = expr->right;
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
  case T_ROTL:
  case T_ROTR:
    EmitMove(irl, temp, left);
    right = Dereference(irl, right);
    EmitOp2(irl, OpcFromOp(op), temp, right);
    return temp;
  case T_NEGATE:
  case T_ABS:
  case T_BIT_NOT:
    right = Dereference(irl, right);
    EmitOp2(irl, OpcFromOp(op), temp, right);
    return temp;
  case T_NOT:
  case T_AND:
  case T_OR:
  case T_EQ:
  case T_NE:
  case T_LE:
  case T_GE:
  case '<':
  case '>':
  {
      Operand *zero = NewImmediate(0);
      Operand *skiplabel = CreateTempLabel(irl);
      EmitMove(irl, temp, zero);
      CompileBoolBranches(irl, expr, NULL, skiplabel);
      EmitOp2(irl, OPC_NOT, temp, temp);
      EmitLabel(irl, skiplabel);
      return temp;
  }
  
  default:
    ERROR(lhs, "Unsupported operator %d", op);
    return left;
  }
}

Operand *
CompileOperator(IRList *irl, AST *expr)
{
    int op = expr->d.ival;
    switch (op) {
    case '*':
        return CompileMul(irl, expr, 0);
    case T_HIGHMULT:
        return CompileMul(irl, expr, 1);
    case '/':
        return CompileDiv(irl, expr, 0);
    case T_MODULUS:
        return CompileDiv(irl, expr, 1);
    case '&':
        if (expr->right->kind == AST_OPERATOR && expr->right->d.ival == T_BIT_NOT) {
	  Operand *lhs = CompileExpression(irl, expr->left);
	  Operand *rhs = CompileExpression(irl, expr->right->right);
	  Operand *temp = NewFunctionTempRegister();
	  EmitMove(irl, temp, lhs);
	  EmitOp2(irl, OPC_ANDN, temp, rhs);
	  return temp;
        }
    default:
        return CompileBasicOperator(irl, expr);
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

static Operand *
CompileMemref(IRList *irl, AST *expr)
{
  Operand *addr = CompileExpression(irl, expr->right);
  Operand *temp;
  AST *type = expr->left;
  int size = EvalConstExpr(type->left);

  if (size == 1) {
      temp = NewOperand(BYTE_REF, (char *)addr, 0);
  } else if (size == 2) {
      temp = NewOperand(WORD_REF, (char *)addr, 0);
  } else {
      temp = NewOperand(LONG_REF, (char *)addr, 0);
      if (size != 4) {
          ERROR(expr, "Illegal size for memory reference");
      }
  }
  return temp;
}

static Operand *
ApplyArrayIndex(IRList *irl, Operand *base, Operand *offset)
{
    Operand *basereg;
    Operand *newbase;
    int idx;
    int siz;
    int shift;
    
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
    default:
        ERROR(NULL, "Bad size in array reference");
        siz = 1;
        break;
    }
          
    basereg = (Operand *)base->name;
    if (offset->kind == REG_IMM) {
        idx = offset->val * siz;
        if (idx == 0) {
            return base;
        }
        newbase = NewOperand(base->kind, (char *)basereg, idx + base->val);
        return newbase;
    }
    newbase = NewFunctionTempRegister();
    EmitMove(irl, newbase, offset);
    if (shift) {
        EmitOp2(irl, OPC_SHL, newbase, NewImmediate(shift));
    }
    EmitOp2(irl, OPC_ADD, newbase, basereg);
    return NewOperand(base->kind, (char *)newbase, 0);
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
  if (IsConstExpr(expr)) {
      if (expr->kind == AST_IDENTIFIER) {
          // leave symbolic constants alone
      } else {
          expr = FoldIfConst(expr);
      }
  }
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
    return CompileOperator(irl, expr);
  case AST_FUNCCALL:
    return CompileFunccall(irl, expr);
  case AST_ASSIGN:
    r = CompileExpression(irl, expr->left);
    val = CompileExpression(irl, expr->right);
    EmitMove(irl, r, val);
    return r;
  case AST_RANGEREF:
    return CompileExpression(irl, TransformRangeUse(expr));
  case AST_ARRAYREF:
  {
      Operand *base;
      Operand *offset;

      if (!expr->right) {
          ERROR(expr, "Array ref with no index?");
          return NewOperand(REG_REG, "???", 0);
      }
      base = CompileExpression(irl, expr->left);
      offset = CompileExpression(irl, expr->right);
      return ApplyArrayIndex(irl, base, offset);
  }
  case AST_MEMREF:
    return CompileMemref(irl, expr);
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
static void EmitAddSub(IRList *irl, Operand *dst, int off)
{
    int opc  = OPC_ADD;
    Operand *imm;
    
    if (off < 0) {
        off = -off;
        opc = OPC_SUB;
    }
    imm = NewImmediate(off);
    EmitOp2(irl, opc, dst, imm);
}

static void EmitMove(IRList *irl, Operand *origdst, Operand *origsrc)
{
    Operand *temp;
    Operand *dst = origdst;
    Operand *src = origsrc;
    
    if (IsMemRef(src)) {
        int off = src->val;
        temp = NewFunctionTempRegister();
        src = (Operand *)src->name;
        switch (origsrc->kind) {
        default:
            ERROR(NULL, "Illegal memory reference");
            break;
        case LONG_REF:
            if (off) {
                EmitAddSub(irl, src, off);
            }
            EmitOp2(irl, OPC_RDLONG, temp, src);
            if (off) {
                EmitAddSub(irl, src, -off);
            }
            break;
        case WORD_REF:
            if (off) {
                EmitAddSub(irl, src, off);
            }
            EmitOp2(irl, OPC_RDWORD, temp, src);
            if (off) {
                EmitAddSub(irl, src, -off);
            }
            break;
        case BYTE_REF:
            if (off) {
                EmitAddSub(irl, src, off);
            }
            EmitOp2(irl, OPC_RDBYTE, temp, src);
            if (off) {
                EmitAddSub(irl, src, -off);
            }
            break;
        }
        src = temp;
    }
    if (IsMemRef(origdst)) {
        int off = dst->val;
        dst = (Operand *)dst->name;
        switch (origdst->kind) {
        default:
            ERROR(NULL, "Illegal memory reference");
            break;
        case LONG_REF:
            if (off) {
                EmitAddSub(irl, dst, off);
            }
            EmitOp2(irl, OPC_WRLONG, src, dst);
            if (off) {
                EmitAddSub(irl, dst, -off);
            }            
            break;
        case WORD_REF:
            if (off) {
                EmitAddSub(irl, dst, off);
            }
            EmitOp2(irl, OPC_WRWORD, src, dst);
            if (off) {
                EmitAddSub(irl, dst, -off);
            }
            break;
        case BYTE_REF:
            if (off) {
                EmitAddSub(irl, dst, off);
            }
            EmitOp2(irl, OPC_WRBYTE, src, dst);
            if (off) {
                EmitAddSub(irl, dst, -off);
            }
            break;
        }
    } else {
        EmitOp2(irl, OPC_MOVE, dst, src);
    }
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
            EmitMove(irl, result, op);
	}
	EmitJump(irl, COND_TRUE, curfunc->asmretname);
	break;
    case AST_WAITCNT:
        retval = ast->left;
        if (!retval) {
            ERROR(ast, "No expression for waitcnt");
            return;
        }
        op = CompileExpression(irl, retval);
        EmitOp2(irl, OPC_WAITCNT, op, NewImmediate(0));
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
    case AST_ASSIGN:
        if (ast->left && ast->left->kind == AST_RANGEREF) {
            ast = TransformRangeAssign(ast->left, ast->right);
        }
        /* fall through */
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
      if (g[i].op->kind == REG_IMM && !g[i].op->used) {
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
    IRList funcirl;

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
        funcirl.head = NULL;
	funcirl.tail = NULL;
        EmitWholeFunction(&funcirl, f);
	OptimizeIRLocal(&funcirl);
	if (newlineOp) {
  	    EmitNewline(irl);
	} else {
	  newlineOp = NewOperand(REG_STRING, "\n", 0);
	}
	AppendIRList(irl, &funcirl);
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
  case OPC_RDBYTE:
  case OPC_RDWORD:
  case OPC_RDLONG:
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

/* IR instructions that have no effect on the generated code */
static bool IsDummy(IR *op)
{
  switch(op->opc) {
  case OPC_COMMENT:
  case OPC_DEAD:
  case OPC_CONST:
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
  case OPC_CMPS:
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
    if (IsBranch(ir->opc)) {
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

static bool
HasSideEffects(IR *ir)
{
    if (ir->dst && ir->dst->kind == REG_HW) {
        return true;
    }
    switch (ir->opc) {
    case OPC_CMP:
    case OPC_CMPS:
    case OPC_WAITCNT:
    case OPC_WRBYTE:
    case OPC_WRLONG:
    case OPC_WRWORD:
        return true;
    default:
        return false;
    }
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
	if (ir_next && ir->dst && IsDead(ir, ir->dst) && !HasSideEffects(ir)) {
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

static void
OptimizeImmediates(IRList *irl)
{
    IR *ir;
    Operand *src;
    int val;
    
    for (ir = irl->head; ir; ir = ir->next) {
        src = ir->src;
        if (! (src && src->kind == REG_IMM) ) {
            continue;
        }
        if (src->name == NULL || src->name[0] == 0) {
            /* already a small immediate */
            continue;
        }
        val = src->val;
        if (ir->opc == OPC_MOVE && val < 0 && val >= -511) {
            ir->opc = OPC_NEG;
            ir->src = NewImmediate(-val);
        } else if (ir->opc == OPC_AND && val < 0 && val >= -512) {
            ir->opc = OPC_ANDN;
            ir->src = NewImmediate(~val);
	}
    }  
}

static int
AddSubVal(IR *ir)
{
    int val = ir->src->val;
    if (ir->opc == OPC_SUB) val = -val;
    return val;
}

static void
OptimizeAddSub(IRList *irl)
{
    IR *ir, *ir_next;
    ir = irl->head;
    while (ir) {
        ir_next = ir->next;
        while (ir_next && IsDummy(ir_next)) {
            ir_next = ir_next->next;
        }
        if (!ir_next) break;
        if (ir->opc == OPC_ADD || ir->opc == OPC_SUB) {
            if (ir_next->opc == OPC_ADD || ir_next->opc == OPC_SUB) {
                if (ir->dst == ir_next->dst && ir->src->kind == REG_IMM && ir_next->src->kind == REG_IMM)
                {
                    int val = AddSubVal(ir) + AddSubVal(ir_next);
                    if (val < 0) {
                        val = -val;
                        ir_next->opc = OPC_SUB;
                    } else {
                        ir_next->opc = OPC_ADD;
                    }
                    ir_next->src = NewImmediate(val);
                    DeleteIR(irl, ir);
                }
            }
        }
        ir = ir_next;
    }
}

void
OptimizeIRLocal(IRList *irl)
{
  if (gl_optimize_flags & OPT_NO_ASM) return;
  if (!irl->head) return;
  EliminateDeadCode(irl);
  OptimizeMoves(irl);
  OptimizeImmediates(irl);
  OptimizeShortBranches(irl);
  OptimizeAddSub(irl);
}
void
OptimizeIRGlobal(IRList *irl)
{
  CheckUsage(irl);
}

/*
 * emit builtin functions like mul and div
 */
static const char *builtin_mul =
"\nmultiply_\n"
"\tmov\titmp2_, mula_\n"
"\txor\titmp2_, mulb_\n"
"\tabs\tmula_, mula_\n"
"\tabs\tmulb_, mulb_\n"
"\tmov\tresult_, #0\n"
"\tmov\titmp1_, #32\n"
"\tshr\tmula_, #1 wc\n"
"mul_lp_\n"
" if_c\tadd\tresult_, mulb_ wc\n"
"\trcr\tresult_, #1 wc\n"
"\trcr\tmula_, #1 wc\n"
"\tdjnz\titmp1_, #mul_lp_\n"

"\tshr\titmp2_, #31 wz\n"
" if_nz\tneg\tresult_, result_\n"
" if_nz\tneg\tmula_, mula_ wz\n"
" if_nz\tsub\tresult_, #1\n"
"\tmov\tmulb_, result_\n"
"multiply__ret\n"
"\tret\n"
;

static const char *builtin_div =
"\ndivide_\n"
    "\tmov\tresult_, #0\n"
    "\tmov\titmp2_, mula_\n"
    "\txor\titmp2_, mulb_\n"
    "\tabs\tmula_, mula_\n"
    "\tabs\tmulb_, mulb_ wz,wc\n"
    " if_z\tjmp\t#divexit_\n"
    "\tmuxc\titmp2_, #1\n"
    "\tmov\titmp1_, #32\n"

"divlp1_\n"
    "\tshr\tmulb_,#1 wc,wz\n"
    "\trcr\tresult_,#1\n"
    " if_nz\tdjnz\titmp1_,#divlp1_\n"

"divlp2_\n"
    "\tcmpsub\tmula_,result_ wc\n"
    "\trcl\tmulb_,#1\n"
    "\tshr\tresult_,#1\n"
    "\tdjnz\titmp1_,#divlp2_\n"

    "\tcmps\titmp2_, #0 wc,wz\n"
    " if_b\tneg\tmula_, mula_\n"
    "\ttest\titmp2, #1\n"
    " if_nz\tneg\tmulb_, mulb_\n"
"divide__ret\n"
    "\tret\n"
;

static void
EmitBuiltins(IRList *irl)
{
    if (mulfunc) {
        Operand *loop = NewOperand(REG_STRING, builtin_mul, 0);
        EmitOp1(irl, OPC_COMMENT, loop);
        (void)GetGlobal(REG_REG, "itmp1_", 0);
        (void)GetGlobal(REG_REG, "itmp2_", 0);
        (void)GetGlobal(REG_REG, "result_", 0);
    }
    if (divfunc) {
        Operand *loop = NewOperand(REG_STRING, builtin_div, 0);
        EmitOp1(irl, OPC_COMMENT, loop);
        (void)GetGlobal(REG_REG, "itmp1_", 0);
        (void)GetGlobal(REG_REG, "itmp2_", 0);
        (void)GetGlobal(REG_REG, "result_", 0);
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
    op1 = NewOperand(REG_STRING, name, val);
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
