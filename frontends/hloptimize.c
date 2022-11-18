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

static void
HandleSpecialFunction(AST *ast, const char *spfunc)
{
    AST *arglist;
    AST *pin_expr;
    AST *x_expr;

    arglist = ast->right;
    if (!strcmp(spfunc, "pinw")) {
        if (!arglist || !arglist->right) {
            return;
        }
        pin_expr = arglist->left;
        x_expr = arglist->right->left;
        if (!x_expr) {
            return;
        }
        if (IsConstExpr(pin_expr) && (64 > (unsigned)EvalConstExpr(pin_expr))) {
            // writing to a single pin, we can always use drvw

        } else if (IsConstExpr(x_expr)) {
            int x = EvalConstExpr(x_expr);
            if (x != 0 && x != 1) {
                return;
            }
        } else if (x_expr->kind == AST_OPERATOR) {
            int x = 9999;
            if (x_expr->d.ival == '&') {
                if (IsConstExpr(x_expr->left)) {
                    x = EvalConstExpr(x_expr->left);
                } else if (IsConstExpr(x_expr->right)) {
                    x = EvalConstExpr(x_expr->right);
                }
                if (x != 0 && x != 1) {
                    return;
                }
            } else if (x_expr->d.ival == K_SHR) {
                if (!IsConstExpr(x_expr->right)) {
                    return;
                }
                x = EvalConstExpr(x_expr->right);
                if (x != 31) {
                    return;
                }
            } else {
                return;
            }
        } else {
            return;
        }
        // if we get here, convert to _drvw
        ast->left = AstIdentifier("_drvw");
        return;
    } else if (!strcmp(spfunc, "pinr")) {
        arglist = ast->right;
        if (!arglist) {
            return;
        }
        pin_expr = arglist->left;
        if (IsConstExpr(pin_expr) && (64 > (unsigned)EvalConstExpr(pin_expr))) {
            /* yes, do the switch */
            ast->left = AstIdentifier("_pinr");
            return;
        }
    } else if (!strcmp(spfunc,"memset")) {
        arglist = ast->right;
        if (!arglist || !arglist->right || !arglist->right->right) {
            return;
        }
        //AST **setptr = &arglist->left;
        AST **setval = &arglist->right->left;
        AST **setlen = &arglist->right->right->left;
        bool const_val = IsConstExpr(*setval);
        bool const_len = IsConstExpr(*setlen);
        uint32_t tmp;
        // If on P2 and doing a long-sized memset, convert to faster function
        if (gl_p2 && const_len && ((tmp = (uint32_t)EvalConstExpr(*setlen)) & 3) == 0) {
            ASTReportInfo save;
            AstReportAs(ast,&save);
            *setlen = AstInteger(tmp>>2);
            if (const_val) {
                tmp = EvalConstExpr(*setval);
                if (tmp!=0) {
                    tmp &= 255;
                    *setval = AstInteger((tmp<<24)|(tmp<<16)|(tmp<<8)|(tmp<<0));
                }
            } else {
                *setval = NewAST(AST_FUNCCALL,AstIdentifier("__builtin_movbyts"),NewAST(AST_EXPRLIST,*setval,NewAST(AST_EXPRLIST,AstInteger(0),NULL)));
            }
            ast->left = AstIdentifier("__builtin_longset"); // Subject to further optimization in ASM backend after inlining
            AstReportDone(&save);
            return;
        }
    }
}

static void
HLOptimizePass(AST *body) {
    if (!body) return;
    if (body->kind == AST_FUNCCALL && IsIdentifier(body->left)) {
        Symbol *sym;
        HLOptimizePass(body->right);
        sym = LookupSymbol(GetIdentifierName(body->left));
        if (sym && sym->kind == SYM_FUNCTION) {
            Function *F = (Function *)sym->val;
            if (F->specialfunc && (curfunc->optimize_flags & OPT_SPECIAL_FUNCS)) {
                HandleSpecialFunction(body, F->specialfunc);
            }
        }
    } else {
        HLOptimizePass(body->right);
        HLOptimizePass(body->left);
    }
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
        HLOptimizePass(func->body);
    }
    curfunc = savefunc;
    current = savecur;
}
