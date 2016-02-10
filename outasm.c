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

Operand *NewOperand(enum Operandkind, const char *name, int val);

struct GlobalVariable {
    Operand *op;
    int val;
};

static struct flexbuf gvars;

Operand *
GetGlobal(const char *name, int value)
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
  tmp.op = NewOperand(REG_REG, name, value);
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

static void EmitOp2(IRList *irl, Operandkind code, Operand *d, Operand *s)
{
  IR *ir = NewIR(code);
  ir->dst = d;
  ir->src = s;
  AppendIR(irl, ir);
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
  return GetGlobal(strdup(temp), (int32_t)val);
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
  default:
    ERROR(expr, "Cannot handle expression yet");
    return NewOperand(REG_IMM, "", 0);
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
	    result = GetGlobal("result_", 0);
            EmitOp2(irl, OPC_MOVE, result, op);
	}
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
    /* now emit the global variables */
    EmitGlobals(irl);
    return true;
}
