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

/*
 * types useful for checking local variable states
 */
typedef enum VarUse {
    VAR_UNUSED, VAR_SET, VAR_USED
} VarUse;
    
typedef struct LocalsInitCheckS {
    Function *F;
} LocalsInitCheckStruct;

/* see if a variable is used or set within an AST block */
static VarUse
doCheckLocalVar(Symbol *sym, AST *body, VarUse identifierUse, AST **where)
{
    VarUse left;
    VarUse right;
    const char *name;

again:
    while (body && body->kind == AST_COMMENTEDNODE) {
        body = body->left;
    }
    if (!body) return VAR_UNUSED;
    
    switch (body->kind) {
    case AST_CONSTREF:
        return VAR_UNUSED;
    case AST_METHODREF:
        left = doCheckLocalVar(sym, body->left, identifierUse, where);
        return left; // do not go down right, that's the member reference
    case AST_INLINEASM:
        left = doCheckLocalVar(sym, body->left, VAR_SET, where);
        if (left != VAR_UNUSED) return left;
        return doCheckLocalVar(sym, body->right, VAR_SET, where);
    case AST_RESULT:
        if (curfunc->resultexpr && curfunc->resultexpr->kind != AST_RESULT) {
            return doCheckLocalVar(sym, curfunc->resultexpr, identifierUse, where);
        }
        if (!strcmp(sym->user_name, "result")) return identifierUse;
        return VAR_UNUSED;
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
        name = GetIdentifierName(body);
        if (name && !strcmp(name, sym->our_name)) {
            *where = body;
            return identifierUse;
        }
        return VAR_UNUSED;
    case AST_ADDROF:
    case AST_ABSADDROF:
        // not, strictly speaking, accurate, but it makes things
        // like memset(foo, 0, sizeof(foo)) work
        return doCheckLocalVar(sym, body->left, VAR_SET, where);
    case AST_ASSIGN:
    case AST_VA_START:
        right = doCheckLocalVar(sym, body->right, VAR_USED, where);
        if (right != VAR_UNUSED) return right;
        left = doCheckLocalVar(sym, body->left, VAR_SET, where);
        return left;
    case AST_ARRAYREF:
        right = doCheckLocalVar(sym, body->right, VAR_USED, where);
        if (right != VAR_UNUSED) return right;
        left = doCheckLocalVar(sym, body->left, identifierUse, where);
        return left;
    case AST_SEQUENCE:
    case AST_STMTLIST:
        left = doCheckLocalVar(sym, body->left, VAR_USED, where);
        if (left != VAR_UNUSED) return left;
        body = body->right;
        goto again;
    case AST_FORATLEASTONCE:
    case AST_FOR:
        left = doCheckLocalVar(sym, body->left, VAR_SET, where);
        if (left != VAR_UNUSED) return left;
        return doCheckLocalVar(sym, body->right, VAR_USED, where);
    case AST_COUNTREPEAT:
        left = doCheckLocalVar(sym, body->left, VAR_SET, where);
        if (left != VAR_UNUSED) return left;
        return doCheckLocalVar(sym, body->right, VAR_USED, where);
    case AST_DOWHILE:
        right = doCheckLocalVar(sym, body->right, identifierUse, where);
        if (right != VAR_UNUSED) return right;
        left = doCheckLocalVar(sym, body->left, identifierUse, where);
        return left;
    case AST_THENELSE:
        left = doCheckLocalVar(sym, body->left, identifierUse, where);
        right = doCheckLocalVar(sym, body->right, identifierUse, where);
        if (left == VAR_SET && right == VAR_SET) return VAR_SET;
        if (left == VAR_USED || right == VAR_USED) return VAR_USED;
        return VAR_UNUSED;
    default:
        left = doCheckLocalVar(sym, body->left, identifierUse, where);
        if (left == VAR_UNUSED) {
            right = doCheckLocalVar(sym, body->right, identifierUse, where);
            return right;
        }
        return left;
    }
}

static int CheckSymbolUsage(Symbol *sym, void *arg) {
    LocalsInitCheckStruct *A = (LocalsInitCheckStruct *)arg;
    VarUse v;
    Function *F = A->F;
    AST *where;
    bool needInit;
    bool needWarn;
    if (sym->kind == SYM_RESULT || sym->kind == SYM_LOCALVAR) {
        where = sym->def;
        needInit = needWarn = false;
        v = doCheckLocalVar(sym, F->body, VAR_USED, &where);
        if (v == VAR_USED) {
            needWarn = true;
            // for RESULT variables in Spin, just automatically add initialization
            if (IsSpinLang(F->language)) {
                needWarn = F->language != LANG_SPIN_SPIN2;
                needInit = !needWarn;
                if (sym->kind == SYM_RESULT) {
                    needWarn = false;
                    // bytecode output doesn't need the initialization of the first output
                    if (TraditionalBytecodeOutput() && sym->offset == 0) {
                        needInit = false;
                    } else {
                        needInit = true;
                    }
                }
            }
        }
        if (needInit) {
            AST *init = AstIdentifier(sym->our_name);
            init = AstAssign(init, AstInteger(0));
            init = NewAST(AST_STMTLIST, init, F->body);
            F->body = init;
        }
        if (needWarn) {
            WARNING(where, "variable %s may be used before it is set in function %s", sym->user_name, F->name);
        }
    }
    return 1;
}

void DoHighLevelOptimize(Module *Q)
{
    LocalsInitCheckStruct A;
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
        /* check for locals that need initializing */
        A.F = func;
        IterateOverSymbols(&func->localsyms, CheckSymbolUsage, &A);
    }

    curfunc = savefunc;
    current = savecur;
}
