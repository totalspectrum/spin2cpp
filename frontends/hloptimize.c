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

//
// check to see if a block contains a label (so somebody could
// jump into it
//
int
BlockContainsLabel(AST *body)
{
    if (!body) return 0;
    if (body->kind == AST_LABEL) {
        return 1;
    }
    return BlockContainsLabel(body->left) || BlockContainsLabel(body->right);
}

//
// remove dead code
//
void RemoveDeadCode(AST *body)
{
    if (!body) return;
    switch (body->kind) {
    case AST_IF:
        if (IsConstExpr(body->left)) {
            int32_t val = EvalConstExpr(body->left);
            AST *thenelse = GetEffectiveNode(body->right);
            AST *newbody, *deletebody;

            if (val) {
                newbody = thenelse->left;
                deletebody = thenelse->right;
            } else {
                newbody = thenelse->right;
                deletebody = thenelse->left;
            }
            if (!BlockContainsLabel(deletebody)) {
                if (newbody) {
                    *body = *newbody;
                    RemoveDeadCode(body);
                } else {
                    /* replace with a dummy */
                    thenelse->left = thenelse->right = NULL;
                }
                return;
            }
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
