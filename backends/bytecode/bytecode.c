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

// routine for printing a stack IRL
static char *AssembleStackIRL(StackIRList *irl);
void DumpStackIRL(StackIRList *irl);

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
    AppendStackIR(irl, ir);
}

static void CompileSymbol(StackIRList *irl, AST *expr)
{
    Symbol *sym;
    int stype;
    sym = LookupSymbol(expr->d.string);
    if (!sym) {
        ERROR_UNKNOWN_SYMBOL(expr);
        return;
    }
    stype = sym->type;
    switch(stype) {
    case SYM_CONSTANT:
    case SYM_FLOAT_CONSTANT:
    {
        AST *symexpr = (AST *)sym->val;
        int val = EvalConstExpr(symexpr);
        CompileImmediate(irl, val);
        return;
    }
    default:
        ERROR(expr, "Internal error, symbol not handled for bytecode");
        return;
    }
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
    case AST_IDENTIFIER:
        CompileSymbol(irl, expr);
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
    StackIR *ir;
    for (f = P->functions; f; f = f->next) {
        StackIRList *irl = &FuncData(f)->code;
        // emit function prolog
        ir = NewStackIR(SOP_LABEL);
        ir->operand = FuncData(f)->label;
        AppendStackIR(irl, ir);
        // compile function
        CompileStatementList(irl, f->body);
    }
}

static void
DoFuncDecl(Module *P)
{
    Function *f;
    for (f = P->functions; f; f = f->next) {
        const char *fname;
        fname = IdentifierGlobalName(P, f->name);
        FuncData(f)->label = AstIdentifier(fname);
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
    char *str;
    
    f = fopen(fname, "w");
    if (!f) {
        fprintf(stderr, "Unable to open bytecode output: ");
        perror(fname);
        exit(1);
    }
    // create the bytecode here
    VisitRecursive(P, SetupForBytecode, VISITFLAG_INIT);
    VisitRecursive(P, DoFuncDecl, VISITFLAG_FUNCDECL);
    VisitRecursive(P, CompileFunctions, VISITFLAG_COMPILEFUNCS);
    VisitRecursive(P, EmitDatSection, VISITFLAG_EMITDAT);

    // chain all the IR together
    {
        Function *pf;
        for (pf = P->functions; pf; pf = pf->next) {
            str = AssembleStackIRL(&FuncData(pf)->code);
            puts(str);
            free(str);
        }
    }
            
    fclose(f);
}

//
// debug routine
//
static const char *opname(StackOp op)
{
    switch(op) {
    case SOP_ADD: return "add";
    case SOP_AND: return "and";
    case SOP_SUB: return "sub";
    case SOP_OR:  return "or";
    case SOP_XOR: return "xor";
    case SOP_SHL: return "shl";
    case SOP_SHR: return "shr";
    case SOP_SAR: return "sar";

    case SOP_EQ:  return "eq";
    case SOP_NE:  return "ne";
    case SOP_LT:  return "lt";
    case SOP_LE:  return "le";
    case SOP_LTU:  return "ltu";
    case SOP_LEU:  return "leu";
        
    case SOP_RET: return "ret";
    case SOP_CALL: return "call";
    case SOP_CALLASM: return "callasm";
    case SOP_PUSHIMM: return "pushi";
    case SOP_PICK: return "pick";
    case SOP_PUT: return "put";
    case SOP_STOB: return "stob";
    case SOP_STOW: return "stow";
    case SOP_STOL: return "stol";
    case SOP_LODB: return "lodb";
    case SOP_LODW: return "lodw";
    case SOP_LODL: return "lodl";
    default:
        return "???";
    }
}

static char *
AssembleStackIRL(StackIRList *irl)
{
    StackIR *ir;
    Flexbuf fb_base;
    Flexbuf *f;
    char *ret;
    
    f = &fb_base;
    flexbuf_init(f, 512);
    
    ir = irl->head;
    while (ir) {
        switch(ir->opc) {
        case SOP_LABEL:
            flexbuf_printf(f, "%s:\n", ir->operand->d.string);
            break;
        case SOP_PUSHIMM:
        case SOP_CALL:
        case SOP_CALLASM:
        case SOP_JZ:
        case SOP_JNZ:
        case SOP_JMP:
            flexbuf_printf(f, "\t%s ", opname(ir->opc));
            PrintExpr(f, ir->operand, 0);
            flexbuf_printf(f, "\n");
            break;
        default:
            flexbuf_printf(f, "\t%s\n", opname(ir->opc));
            break;
        }
        ir = ir->next;
    }
    flexbuf_addchar(f, 0);
    ret = flexbuf_get(f);
    flexbuf_delete(f);
    return ret;
}

void DumpStackIRL(StackIRList *irl)
{
    char *ret = AssembleStackIRL(irl);
    puts(ret);
    free(ret);
}
