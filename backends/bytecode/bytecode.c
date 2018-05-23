//
// Bytecode data output for spin2cpp
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
#include "bytecode.h"

// compilation routines
static void
SetupForBytecode(Module *P)
{
    Function *f;
    Flexbuf *mem;
    
    if (!P->bedata) {
        P->bedata = calloc(sizeof(ByteCodeModData), 1);
        for (f = P->functions; f; f = f->next) {
            f->bedata = calloc(sizeof(ByteCodeFuncData), 1);
        }
        mem = (Flexbuf *)calloc(1, sizeof(*mem));
        flexbuf_init(mem, 32768);
        ModData(P)->mem = mem;
    }
}

StackIR *NewStackIR(StackOp opc)
{
    StackIR *ir = (StackIR*)calloc(1, sizeof(*ir));
    ir->opc = opc;
    return ir;
}

static void CompileStatement(StackIRList *irl, AST *ast); /* forward declaration */

static void CompileStatementList(StackIRList *irl, AST *ast)
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

static void CompileImmediate(StackIRList *irl, int32_t i)
{
    StackIR *ir = NewStackIR(SOP_PUSHIMM);
    ir->operand = AstInteger(i);
}

static void CompileExpression(StackIRList *irl, AST *ast)
{
    AST *expr = ast;
    while (expr && expr->kind == AST_COMMENTEDNODE) {
        expr = expr->left;
    }
    if (!expr) {
        ERROR(ast, "Expected a value");
    }
    if (IsConstExpr(expr)) {
        expr = FoldIfConst(expr);
    }
    switch (expr->kind) {
    case AST_INTEGER:
    case AST_FLOAT:
        CompileImmediate(irl, (int32_t)expr->d.ival);
        break;
    default:
        ERROR(ast, "Cannot handle expression in bytecode yet");
        break;
    }
}

static void CompileStatement(StackIRList *irl, AST *ast)
{
    AST *retval;
    StackIR *ir;
    
    if (!ast) return;
    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        CompileStatement(irl, ast->left);
        break;
    case AST_RETURN:
        retval = ast->left;
        if (retval) {
            CompileExpression(irl, retval);
        }
        ir = NewStackIR(SOP_RET);
        AppendStackIR(irl, ir);
        break;
    case AST_STMTLIST:
        CompileStatementList(irl, ast);
        break;
    default:
        ERROR(ast, "Cannot compile statement for bytecode yet");
        break;
    }
}

static void
CompileFunctions(Module *P)
{
    Function *f;

    for (f = P->functions; f; f = f->next) {
        StackIRList *irl = &FuncData(f)->code;
        CompileStatementList(irl, f->body);
    }
}

static void
DoFuncDecl(Module *P)
{
}

static void
EmitDatSection(Module *P)
{
  Flexbuf *fb;
  Flexbuf *relocs;

  fb = (Flexbuf *)calloc(1, sizeof(*fb));
  relocs = (Flexbuf *)calloc(1, sizeof(*relocs));
  flexbuf_init(fb, 16*1024);
  flexbuf_init(relocs, 512);
  PrintDataBlock(fb, P, NULL,relocs);

  ModData(P)->data = fb;
  ModData(P)->dataRelocs = relocs;
}

#define VISITFLAG_INIT          0x01240001
#define VISITFLAG_FUNCDECL      0x01240002
#define VISITFLAG_COMPILEFUNCS  0x01240003
#define VISITFLAG_EMITDAT       0x01240004

typedef void (*VisitorFunc)(Module *P);

static void
VisitRecursive(Module *P, VisitorFunc func, unsigned visitval)
{
    Module *Q;
    AST *subobj;
    Module *save = current;
    Function *savecurf = curfunc;
    
    if (P->visitflag == visitval)
        return;

    current = P;

    P->visitflag = visitval;
    (*func)(P);

    // compile intermediate code for submodules
    for (subobj = P->objblock; subobj; subobj = subobj->right) {
        if (subobj->kind != AST_OBJECT) {
            ERROR(subobj, "Internal Error: Expecting object AST");
            break;
        }
        Q = (Module *)subobj->d.ptr;
        VisitRecursive(Q, func, visitval);
    }
    current = save;
    curfunc = savecurf;
}

void
OutputBytecode(const char *fname, Module *P)
{
    FILE *f = NULL;

    f = fopen(fname, "w");
    if (!f) {
        fprintf(stderr, "Unable to open bytecode output: ");
        perror(fname);
        exit(1);
    }
    // write the bytecode here
    VisitRecursive(P, SetupForBytecode, VISITFLAG_INIT);
    VisitRecursive(P, DoFuncDecl, VISITFLAG_FUNCDECL);
    VisitRecursive(P, CompileFunctions, VISITFLAG_COMPILEFUNCS);
    VisitRecursive(P, EmitDatSection, VISITFLAG_EMITDAT);
    
    fclose(f);
}
