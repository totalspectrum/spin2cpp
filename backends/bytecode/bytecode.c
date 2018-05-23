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

// some flexbuf utilities
static void flexbuf_putlong(Flexbuf *fb, uint32_t L)
{
    char *r;
    flexbuf_addchar(fb, L & 0xff);
    flexbuf_addchar(fb, (L>>8) & 0xff);
    flexbuf_addchar(fb, (L>>16) & 0xff);
    r = flexbuf_addchar(fb, (L>>24) & 0xff);
    if (!r) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
}
static void flexbuf_putword(Flexbuf *fb, uint32_t L)
{
    char *r;
    flexbuf_addchar(fb, L & 0xff);
    r = flexbuf_addchar(fb, (L>>8) & 0xff);
    if (!r) {
        fprintf(stderr, "out of memory\n");
        exit(2);
    }
}

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
            flexbuf_init(&FuncData(f)->code, 512);
        }
        mem = (Flexbuf *)calloc(1, sizeof(*mem));
        flexbuf_init(mem, 32768);
        ModData(P)->mem = mem;
    }
}

static inline void EmitBytecode(Flexbuf *fb, int i)
{
    flexbuf_addchar(fb, i);
}

static void CompileStatement(Flexbuf *fb, AST *ast); /* forward declaration */

static void CompileStatementList(Flexbuf *fb, AST *ast)
{
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error: expected statement list, got %d",
                  ast->kind);
            return;
        }
        CompileStatement(fb, ast->left);
        ast = ast->right;
    }
}

static bool PowOfTwo(int32_t ival, int *bval)
{
    int b;
    int32_t m = 2;
    for (b = 0; b < 31; b++) {
        if (ival == m) {
            *bval = b;
            return true;
        }
        if (ival == m-1) {
            *bval = b | 0x20;
            return true;
        }
        if (ival == ~m) {
            *bval = b | 0x40;
            return true;
        }
        if (ival == -m) {
            *bval = b | 0x60;
            return true;
        }
        m = m << 1;
    }
    return false;
}

static void CompileImmediate(Flexbuf *fb, int32_t i)
{
    int bval;
    if (i == -1) {
        EmitBytecode(fb, 0x34); // push#-1
    } else if (i == 0) {
        EmitBytecode(fb, 0x35); // push#0
    } else if (i == 1) {
        EmitBytecode(fb, 0x36); // push#1
    } else if ( 0 == (i & 0xffffff00) ) {
        EmitBytecode(fb, 0x38); // push#byte
        EmitBytecode(fb, i);
    } else if (PowOfTwo(i, &bval)) {
        EmitBytecode(fb, 0x37);
        EmitBytecode(fb, bval);
    } else if ( 0 == ( (~i) & 0xffffff00 ) ) {
        EmitBytecode(fb, 0x38);
        EmitBytecode(fb, ~i);
        EmitBytecode(fb, 0xe7); // bit_not
    } else if (0 == (i & 0xffff0000)) {
        EmitBytecode(fb, 0x39);
        EmitBytecode(fb, i >> 8);
        EmitBytecode(fb, i & 0xff);
    } else if (0 == (i & 0xff000000)) {
        EmitBytecode(fb, 0x3a);
        EmitBytecode(fb, (i >> 16) & 0xff);
        EmitBytecode(fb, (i >> 8) & 0xff);
        EmitBytecode(fb, i & 0xff);
    } else {
        EmitBytecode(fb, 0x3b);
        EmitBytecode(fb, (i >> 24) & 0xff);
        EmitBytecode(fb, (i >> 16) & 0xff);
        EmitBytecode(fb, (i >> 8) & 0xff);
        EmitBytecode(fb, i & 0xff);
    }
}

static void CompileExpression(Flexbuf *fb, AST *ast)
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
        CompileImmediate(fb, (int32_t)expr->d.ival);
        break;
    default:
        ERROR(ast, "Cannot handle expression in bytecode yet");
        break;
    }
}

static void CompileStatement(Flexbuf *fb, AST *ast)
{
    AST *retval;
    if (!ast) return;
    switch (ast->kind) {
    case AST_COMMENTEDNODE:
        CompileStatement(fb, ast->left);
        break;
    case AST_RETURN:
        retval = ast->left;
        if (!retval) {
            // just a plain return
            EmitBytecode(fb, 0x32);
        }
        CompileExpression(fb, retval);
        EmitBytecode(fb, 0x33);
        break;
    case AST_STMTLIST:
        CompileStatementList(fb, ast);
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
        Flexbuf *code = &FuncData(f)->code;
        CompileStatementList(code, f->body);
    }
}

static void
DoFuncDecl(Module *P)
{
    Flexbuf *mem = ModData(P)->mem;
    Function *f;
    flexbuf_putlong(mem, 0); // reserve space for link to next object
    for (f = P->functions; f; f = f->next) {
        // address of method, still to be determined
        flexbuf_putword(mem, 0);
        // stack growth for method
        flexbuf_putword(mem, 4*(1 + f->numparams + f->numlocals));
    }
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
