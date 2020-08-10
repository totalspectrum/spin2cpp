/*
 * Spin to C/C++ converter
 * Copyright 2011-2020 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * various high level optimizations
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

AST *GetEffectiveNode(AST *ast)
{
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        ast = ast->left;
    }
    return ast;
}

void RemoveDeadCode(AST *body)
{
    if (!body) return;
    switch (body->kind) {
    case AST_IF:
        if (IsConstExpr(body->left)) {
            int32_t val = EvalConstExpr(body->left);
            AST *thenelse = GetEffectiveNode(body->right);
            AST *newbody = val ? thenelse->left : thenelse->right;
            if (newbody) {
                *body = *newbody;
                RemoveDeadCode(body);
                return;
            } else {
                /* replace with a dummy */
                thenelse->left = thenelse->right = NULL;
            }
            return;
        }
        break;
    default:
        break;
    }
    RemoveDeadCode(body->left);
    RemoveDeadCode(body->right);
}

void DoHighLevelOptimize(Module *Q)
{
    Module *savecur = current;
    Function *func;
    Function *savefunc = curfunc;
    
    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;
        if (func->optimize_flags & OPT_DEADCODE) {
            RemoveDeadCode(func->body);
        }
    }
    curfunc = savefunc;
    current = savecur;
}
