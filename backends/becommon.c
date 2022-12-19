/*
 * Spin to C/C++ translator
 * Copyright 2011-2022 Total Spectrum Software Inc.
 * 
 * +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "common.h"
#include "becommon.h"

static AST *make_methodptr;

AST *
BuildMethodPointer(AST *ast)
{
    Symbol *sym;
    AST *objast;
    AST *funcaddr;
    AST *result;
    AST *call;
    Function *func;
    AST *funcptrtype;
    
    sym = FindCalledFuncSymbol(ast, &objast, 0);
    if (!sym || sym->kind != SYM_FUNCTION) {
        if (sym) {
            ERROR(ast, "%s is not a function", sym->user_name);
        } else {
            ERROR(ast, "Internal error, unable to find function address");
        }
        return ast;
    }
    func = (Function *)sym->v.ptr;
    if (func->is_static) {
        objast = AstInteger(0);
    } else if (objast == NULL) {
        objast = NewAST(AST_SELF, NULL, NULL);
    } else {
        objast = NewAST(AST_ADDROF, objast, NULL);
    }
    funcptrtype = NewAST(AST_PTRTYPE, func->overalltype, NULL);
    func->used_as_ptr = 1;
    if (func->callSites == 0) {
        MarkUsed(func, "func pointer");
    }
    // save off the current @ node
    funcaddr = NewAST(AST_ADDROF, ast->left, ast->right);
    // create a call
    if (!make_methodptr) {
        make_methodptr = AstIdentifier("_make_methodptr");
    }
    call = NewAST(AST_EXPRLIST, funcaddr, NULL);
    call = NewAST(AST_EXPRLIST, objast, call);
    result = NewAST(AST_FUNCCALL, make_methodptr, call);
    // cast the result to the correct function pointer type
    result = NewAST(AST_CAST, funcptrtype, result);
    return result;
}

// get number of parameters
// adjust for possible varargs (indicated by negative)
static int BCGetNumParams(Function *F) {
    int n = F->numparams;
    return n >= 0 ? n : (-n)+1;
}

// find base for result variables
static int resultOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        if (offset == 0) return 0;
        return 4 + (BCGetNumParams(F)*4) + (offset-4);
    case INTERP_KIND_NUCODE:
        return offset + (BCGetNumParams(F)+4)*LONG_SIZE ;
    default:
        return offset;
    }
}

// number of results for Spin1 bytecode purposes
static int BCGetNumResults(Function *F) {
    int n = F->numresults;
    return (n<=1) ? 1 : n;
}

// number of results for Spin2/Nu code bytecode purposes
static int DefaultGetNumResults(Function *F) {
    int n = F->numresults;
    return n;
}

// find base for parameter variables
static int paramOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        return 4 + offset; // always one result pushed onto stack
    case INTERP_KIND_NUCODE:
        return offset;
    default:
        return offset + DefaultGetNumResults(F)*4;
    }
}

// find base for local variables
static int localOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        return offset + (BCGetNumResults(F)+ BCGetNumParams(F))*4; // FIXME small variables
    case INTERP_KIND_NUCODE:
        return offset + (DefaultGetNumResults(F)+BCGetNumParams(F)+4)*4;
    default:
        return offset + (DefaultGetNumResults(F) + BCGetNumParams(F))*4;
    }
}

//
// normalize offsets for Spin compatibility
// In Spin, function variables must be laid out with
// results first, then parameters, then locals.
// This function resets all variable offsets to give the
// correct values.
//
static int resetOffsets(Symbol *sym, void *arg)
{
    Function *F = (Function *)arg;
    
    switch (sym->kind) {
    case SYM_RESULT:
        sym->offset = resultOffset(F, sym->offset);
        break;
    case SYM_PARAMETER:
        sym->offset = paramOffset(F, sym->offset);
        break;
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
        sym->offset = localOffset(F, sym->offset);
        break;
    default:
        /* nothing to do */
        break;
    }
    return 1;
}

/* convert offsets in local variables to their canonical values, depending on
 * how the interpreter wants them laid out
 */
void
NormalizeVarOffsets(Function *F)
{
    IterateOverSymbols(&F->localsyms, resetOffsets, (void *)F);
}

/*
 * evaluate any constant expressions within a string
 */
AST *
EvalStringConst(AST *expr)
{
    if (!expr) {
        return expr;
    }
    switch (expr->kind) {
    case AST_EXPRLIST:
        return NewAST(AST_EXPRLIST, EvalStringConst(expr->left), EvalStringConst(expr->right));
    case AST_STRING:
    case AST_INTEGER:
        return expr;
    default:
        if (IsConstExpr(expr)) {
            return AstInteger(EvalConstExpr(expr));
        } else {
            return expr;
        }
    }
}

static void StringAppend(Flexbuf *fb,AST *expr) {
    if(!expr) return;
    switch (expr->kind) {
    case AST_INTEGER: {
        int i = expr->d.ival;
        if (i < 0 || i>255) ERROR(expr,"Character out of range!");
        flexbuf_putc(i,fb);
    } break;
    case AST_STRING: {
        flexbuf_addstr(fb,expr->d.string);
    } break;
    case AST_EXPRLIST: {
        if (expr->left) StringAppend(fb,expr->left);
        if (expr->right) StringAppend(fb,expr->right);
    } break;
    default: {
        if (IsConstExpr(expr)) {
            int i = EvalConstExpr(expr);
            if (i < 0 || i>255) ERROR(expr,"Character out of range!");
            flexbuf_putc(i,fb);
        } else {
            ERROR(expr,"String expression is not constant");
        }
    } break;
    }
}

void StringBuildBuffer(Flexbuf *fb, AST *expr) {
    StringAppend(fb, expr);
    flexbuf_addchar(fb, 0);
}

// Printf that auto-allocates some space (and never frees it, lol)
char *auto_printf(size_t max,const char *format,...) {
    char *buffer = malloc(max);
    va_list args;
    va_start(args, format);
    vsnprintf(buffer,max,format,args);
    va_end(args);
    return buffer;
}

/* find the backend name for a symbol */
const char *BackendNameForSymbol(Symbol *sym) {
    Module *Q = sym->module ? sym->module : NULL;
    if (NuBytecodeOutput()) {
        return NuCodeSymbolName(sym);
    }
    return IdentifierModuleName(Q, sym->our_name);
}

/*
 * utility function for visiting modules and doing things to them (used by back ends primarily)
 */
int
VisitRecursive(void *vptr, Module *P, VisitorFunc func, unsigned visitval)
{
    Module *Q;
    AST *subobj;
    Module *save = current;
    Function *savecurf = curfunc;
    int change = 0;
    
    if (P->all_visitflags & visitval)
        return change;  // already visited this one

    current = P;

    P->all_visitflags |= visitval;
    P->visitFlag = visitval;
    change |= (*func)(vptr, P);

    // compile intermediate code for submodules
    for (subobj = P->objblock; subobj; subobj = subobj->right) {
        if (subobj->kind != AST_OBJECT) {
            ERROR(subobj, "Internal Error: Expecting object AST");
            break;
        }
        Q = (Module *)subobj->d.ptr;
        change |= VisitRecursive(vptr, Q, func, visitval);
    }
    // and for sub-submodules
    for (Q = P->subclasses; Q; Q = Q->subclasses) {
        change |= VisitRecursive(vptr, Q, func, visitval);
    }
    current = save;
    curfunc = savecurf;
    return change;
}
