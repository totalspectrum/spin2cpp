/*
 * Spin to C/C++ converter
 * Copyright 2011-2018 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for BASIC specific features
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

static void
doCTransform(AST **astptr)
{
    AST *ast = *astptr;
    Function *func;
    
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        astptr = &ast->left;
        ast = *astptr;
    }
    if (!ast) return;
    AstReportAs(ast); // any newly created AST nodes should reflect debug info from this one
    switch (ast->kind) {
    case AST_ASSIGN:
        if (ast->left && ast->left->kind == AST_RANGEREF) {
            *astptr = ast = TransformRangeAssign(ast->left, ast->right, 1);
        }
        doCTransform(&ast->left);
        doCTransform(&ast->right);
        break;
    case AST_COUNTREPEAT:
        // convert repeat count into a for loop
        doCTransform(&ast->left);
        doCTransform(&ast->right);
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_RANGEREF:
        doCTransform(&ast->left);
        doCTransform(&ast->right);
        *astptr = ast = TransformRangeUse(ast);
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        {
            doCTransform(&ast->left);
            doCTransform(&ast->right);
            if (IsLocalVariable(ast->left)) {
                curfunc->local_address_taken = 1;
            }
            // taking the address of a function may restrict how
            // we can call it (stack vs. register calling)
            Symbol *sym;
            Function *f = NULL;
            sym = FindCalledFuncSymbol(ast, NULL, 0);
            if (sym && sym->type == SYM_FUNCTION) {
                f = (Function *)sym->val;
            }
            if (f) {
                f->used_as_ptr = 1;
                f->callSites++;
            }
        }
        break;
    case AST_METHODREF:
        doCTransform(&ast->left);
        doCTransform(&ast->right);
        if (IsPointerType(ast->left)) {
            WARNING(ast, "Needs a pointer dereference");
        }
        break;
    case AST_TRYENV:
        doCTransform(&ast->left);
        doCTransform(&ast->right);
        // keep local variables on stack, so they will be preserved
        // if an exception throws us back here without cleanup
        curfunc->local_address_taken = 1;
        break;
    case AST_PRINT:
        doCTransform(&ast->left);
        doCTransform(&ast->right);
        ERROR(ast, "Unexpected PRINT in C code");
        break;
    case AST_LABEL:
        if (!ast->left || ast->left->kind != AST_IDENTIFIER) {
            ERROR(ast, "Label is not an identifier");
        } else {
            const char *name = ast->left->d.string;
            Symbol *sym = FindSymbol(&curfunc->localsyms, name);
            if (sym) {
                WARNING(ast, "Redefining %s as a label", name);
            }
            AddSymbol(&curfunc->localsyms, name, SYM_LOCALLABEL, 0);
        }
        break;
    case AST_COGINIT:
        doCTransform(&ast->left);
        doCTransform(&ast->right);
        if (0 != (func = IsSpinCoginit(ast))) {
            func->cog_task = 1;
            func->force_static = 1;
        }
        break;
    default:
        doCTransform(&ast->left);
        doCTransform(&ast->right);
        break;
    }
}

void
CTransform(Module *Q)
{
    Module *savecur = current;
    Function *func;
    Function *savefunc = curfunc;

    InitGlobalFuncs();
    
    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;

        SimplifyAssignments(&func->body);
        doCTransform(&func->body);
        CheckTypes(func->body);
    }
    curfunc = savefunc;
    current = savecur;
}
