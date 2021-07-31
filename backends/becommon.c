/*
 * Spin to C/C++ translator
 * Copyright 2011-2021 Total Spectrum Software Inc.
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
    
    sym = FindCalledFuncSymbol(ast, &objast, 0);
    if (!sym || sym->kind != SYM_FUNCTION) {
        if (sym) {
            ERROR(ast, "%s is not a function", sym->user_name);
        } else {
            ERROR(ast, "Internal error, unable to find function address");
        }
        return ast;
    }
    func = (Function *)sym->val;
    if (func->is_static) {
        objast = AstInteger(0);
    } else if (objast == NULL) {
        objast = NewAST(AST_SELF, NULL, NULL);
    } else {
        objast = NewAST(AST_ADDROF, objast, NULL);
    }
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
    return result;
}

// find base for result variables
static int resultOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        if (offset == 0) return 0;
        return 4+F->numparams*4 + (offset-1)*4;
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
    return (F->result_declared) ? n : 0;
}

// find base for parameter variables
static int paramOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        return 4 + offset; // always one result pushed onto stack
    default:
        return offset + DefaultGetNumResults(F)*4;
    }
}

// find base for local variables
static int localOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        return offset + (BCGetNumResults(F)+F->numparams)*4; // FIXME small variables
    default:
        return offset + (DefaultGetNumResults(F) + F->numparams)*4;
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

void
NormalizeVarOffsets(Function *F)
{
    IterateOverSymbols(&F->localsyms, resetOffsets, (void *)F);
}
