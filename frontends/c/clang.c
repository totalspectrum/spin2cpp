/*
 * Spin to C/C++ converter
 * Copyright 2011-2022 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for C specific features
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

// genPrintf is in basic/basiclang.c
extern AST *genPrintf(AST *);

// "cflags" is reserved for future use

static void
doCTransform(AST **astptr, unsigned cflags)
{
    AST *ast = *astptr;
    Function *func = NULL;
    
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        astptr = &ast->left;
        ast = *astptr;
    }
    if (!ast) return;
    switch (ast->kind) {
    case AST_ASSIGN:
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
#if 1
        // this seems pretty hacky but is needed to make some
        // bitfield tests work
        if (ast->left && ast->left->kind == AST_CAST && ast->left->right && ast->left->right->kind == AST_RANGEREF) {
            ast->left = ast->left->right;
        }
#endif        
        if (ast->left && ast->left->kind == AST_RANGEREF) {
            *astptr = ast = TransformRangeAssign(ast->left, ast->right, ast->d.ival, 1);
        }

        break;
    case AST_COUNTREPEAT:
        // convert repeat count into a for loop
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_RANGEREF:
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
        *astptr = ast = TransformRangeUse(ast);
        break;
    case AST_ALLOCA:
        doCTransform(&ast->right, cflags);
        if (curfunc) {
            curfunc->uses_alloca = 1;
        } else {
            ERROR(ast, "use of alloca outside a function");
        }
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        {
            doCTransform(&ast->left, cflags);
            doCTransform(&ast->right, cflags);
            if (curfunc && IsLocalVariable(ast->left)) {
                curfunc->local_address_taken = 1;
            }
            // taking the address of a function may restrict how
            // we can call it (stack vs. register calling)
            Symbol *sym;
            Function *f = NULL;
            sym = FindCalledFuncSymbol(ast, NULL, 0);
            if (sym && sym->kind == SYM_FUNCTION) {
                f = (Function *)sym->v.ptr;
            }
            if (f) {
                f->used_as_ptr = 1;
                if (f->callSites == 0) {
                    MarkUsed(f, "func pointer");
                }
            }
        }
        break;
    case AST_METHODREF:
    {
        AST *typ;
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
        typ = ExprType(ast);
        if (typ && typ->kind == AST_USING) {
            AST *replace = DupAST(typ->left);
            if (replace && replace->right && replace->right->kind == AST_RANGEREF) {
                ast->right = replace->right->left;
                replace->right->left = DupAST(ast);
            }
            *ast = *replace;
        }
        break;
    }
    case AST_TRYENV:
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
        // keep local variables on stack, so they will be preserved
        // if an exception throws us back here without cleanup
        if (curfunc) {
            curfunc->local_address_taken = 1;
        } else {
            ERROR(ast, "setjmp/longjmp outside of a function");
        }
        break;
    case AST_LABEL:
        AddSymbolForLabel(ast);
        break;
    case AST_COGINIT:
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
        if (IsSpinCoginit(ast, &func) && func) {
            func->cog_task = 1;
            func->force_static = 1;
        }
        break;
    case AST_FUNCCALL:
        // handle __builtin_printf(x, ...) specially
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
        if (ast->left && ast->left->kind == AST_PRINT) {
            AST *newprintf = genPrintf(ast);
            if (newprintf) {
                *ast = *newprintf;
            } else {
                ast->left = AstIdentifier("printf");
            }
        }
        break;
    case AST_CASE:
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
        *ast = *CreateSwitch(ast, NULL);
        break;
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
    {
        if (curfunc && IsLocalVariable(ast)) {
            AST *typ = ExprType(ast);
            if (typ) {
                if (TypeGoesOnStack(typ)) {
                    curfunc->stack_local = 1;
                }
            }
        }
        break;
    }
    case AST_INLINEASM:
        /* don't mess with RANGEREFs in here */
        break;
    default:
        doCTransform(&ast->left, cflags);
        doCTransform(&ast->right, cflags);
        break;
    }
}

void
CTransform(Function *func)
{
    InitGlobalFuncs();

    DoHLTransforms(func);
    doCTransform(&func->body, 0);
    CheckTypes(func->body);
}
