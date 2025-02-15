/*
 * Spin to C/C++ converter
 * Copyright 2011-2025 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * various high level transformations that should take
 * place regardless of optimization
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

/*
 * fix up references
 */
static void
fixReferences(AST **astptr, int incdecop)
{
    AST *ast = *astptr;
    AST *typ;
    AST *deref;
    ASTReportInfo saveinfo;

    if (!ast) return;
    switch (ast->kind) {
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
        typ = ExprType(ast);
        if (typ && IsRefType(typ)) {
            if (incdecop == '@') return;
            AstReportAs(ast, &saveinfo);
            if (incdecop) {
                switch (incdecop) {
                case K_REF_POSTDEC:
                    ast = AstOperator(K_DECREMENT, ast, NULL); break;
                case K_REF_POSTINC:
                    ast = AstOperator(K_INCREMENT, ast, NULL); break;
                case K_REF_PREDEC:
                    ast = AstOperator(K_DECREMENT, NULL, ast); break;
                case K_REF_PREINC:
                    ast = AstOperator(K_INCREMENT, NULL, ast); break;
                default:
                    ERROR(ast, "Internal compiler error: unknown op\n"); break;
                }
            }
            deref = NewAST(AST_MEMREF, typ->left, ast);
            deref = NewAST(AST_ARRAYREF, deref, AstInteger(0));
            *astptr = deref;
            AstReportDone(&saveinfo);
        }
        return;
    case AST_ASSIGN_INIT:
        /* leave the LHS alone, if it's a reference we are assigning it */
        fixReferences(&ast->right, incdecop);
        return;
    case AST_OPERATOR:
        if (ast->d.ival == K_REF_PREINC || ast->d.ival == K_REF_PREDEC
            || ast->d.ival == K_REF_POSTINC || ast->d.ival == K_REF_POSTDEC) {
            bool onLeft = (ast->left != NULL);
            int op = ast->d.ival;
            AST *typ = onLeft ? ExprType(ast->left) : ExprType(ast->right);
            if (!IsRefType(typ)) {
                ERROR(ast, "Applying [++] or [--] to a non-pointer\n");
            } else {
                fixReferences(&ast->left, op);
                fixReferences(&ast->right, op);
                *astptr = (ast->left) ? ast->left : ast->right;
                return;
            }
        }
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        typ = ExprType(ast->left);
        if (typ && IsRefType(typ)) {
            fixReferences(&ast->left, '@');
            *astptr = ast->left;
            return;
        }
        break;
    default:
        break;
    }
    fixReferences(&ast->left, incdecop);
    fixReferences(&ast->right, incdecop);
}

/*
 * if we see something like M ^= N
 * we want to transform to M := M ^ N
 * but we cannot do that if M has side effects
 * so we have to pull side effects out of M
 * cases to watch for:
 *  M == X[(A, B, C)]  --> temp := (A, B, C), X[temp]
 *  M == X[A:=B] --> A:=B, X[A]
 *  M == X[func(A)] --> temp := func(A), X[temp]
 *
 * beware though of post effects:
 *   X[I++] := I  --> X[I]:=I, I++
 *   *X++ ^= N  --> *X ^= N, X++
 */

AST*
ExtractSideEffects(AST *expr, AST **preseq)
{
    AST *temp;
    AST *sideexpr = NULL;
    ASTReportInfo saveinfo;
    
    if (!expr) {
        return expr;
    }
    switch (expr->kind) {
    case AST_ARRAYREF:
    case AST_MEMREF:
        if (ExprHasSideEffects(expr->right)) {
            AstReportAs(expr, &saveinfo);
            temp = AstTempLocalVariable("_temp_", NULL);
            sideexpr = AstAssign(temp, expr->right);
            expr->right = temp;
            if (*preseq) {
                *preseq = NewAST(AST_SEQUENCE, *preseq, sideexpr);
            } else {
                *preseq = sideexpr;
            }
            AstReportDone(&saveinfo);
        }
        if (ExprHasSideEffects(expr->left) && expr->kind == AST_ARRAYREF && expr->left->kind != AST_MEMREF) {
            AstReportAs(expr, &saveinfo);
            AST *typ = ExprType(expr->left);
            temp = AstTempLocalVariable("_arr_", typ);
            sideexpr = AstAssign(temp, expr->left);
            expr->left = temp;
            if (*preseq) {
                *preseq = NewAST(AST_SEQUENCE, *preseq, sideexpr);
            } else {
                *preseq = sideexpr;
            }
            AstReportDone(&saveinfo);
        }
        expr->left = ExtractSideEffects(expr->left, preseq);
        break;
    default:
        /* do nothing */
        break;
    }
    return expr;
}

// check for simple array references like:
// x.byte[N] := Y
// return a transformed expression like
// x.[N+7..N] := Y
// we may assume that ast is an arrayref

AST *
CheckSimpleArrayref(AST *ast)
{
    if (TraditionalBytecodeOutput()) return NULL; // This optimization is pointless in bytecode mode
    AST *newexpr = NULL;
    if (ast->left && ast->left->kind == AST_MEMREF && IsConstExpr(ast->right)) {
        AST *left = ast->left;
        int index = EvalConstExpr(ast->right);
        AST *typ = left->left;
        AST *id = left->right;
        int shift = 0;
        int bits = 0;
        ASTReportInfo saveinfo;

        if (typ && id && id->kind == AST_ADDROF) {
            AST *subtype;
            id = id->left;
            subtype = ExprType(id);
            if (IsArrayType(subtype)) {
                return NULL;
            }
            if (IsIdentifier(id) && IsLocalVariable(id) && TypeSize(subtype) == 4) {
                if (typ == ast_type_word && index < 2) {
                    shift = index * 16;
                    bits = 16;
                } else if (typ == ast_type_byte && index < 4) {
                    shift = index * 8;
                    bits = 8;
                }
                if (bits > 0) {
                    AstReportAs(ast, &saveinfo);
                    newexpr = id;
                    newexpr = NewAST(AST_RANGEREF, newexpr,
                                     NewAST(AST_RANGE,
                                            AstInteger(shift+bits-1),
                                            AstInteger(shift)));
                    AstReportDone(&saveinfo);
                    // OK, if we're on the LHS we have to transform this
                    // to an assignment, otherwise to a use
                    return newexpr;
                }
            }
        }
    }
    return NULL;
}

// look out for short-circuiting assignments
// flag ||= x should always evaluate x, at least in Spin

static int IsBoolOp(int op)
{
    return (op == K_BOOL_OR || op == K_BOOL_AND || op == K_BOOL_XOR);
}

// transform an assignment chain like a := b := expr
// into (tmp := expr, b := tmp, a := tmp)
// we do this recursively:
//    b := expr  -> (tmp := expr, b:=tmp) if expr is not an assign
//    a := (tmp := expr, b:=tmp) -> ( (tmp := expr, b:=tmp), a:=tmp)

static AST *
TransformAssignChainNoCasts(AST **astptr)
{
    AST *ast = *astptr;
    AST *tmp = NULL;
    AST *lhs, *rhs;
    AST *newseq;
    ASTReportInfo saveinfo;
    
    if (!ast) return tmp;
    if (ast->kind != AST_ASSIGN) return tmp;
    lhs = ast->left;
    rhs = ast->right;
    // watch out for Spin function calls masquerading as identifiers
    if (curfunc->language == LANG_SPIN_SPIN1 && rhs->kind == AST_IDENTIFIER) {
        AST *typ = ExprType(rhs);
        if (IsFunctionType(typ)) {
            rhs = ast->right = NewAST(AST_FUNCCALL, rhs, NULL);
        }
        //printf("typ->kind == %d\n", typ->kind);
    }
    if (rhs->kind != AST_ASSIGN) {
        switch (rhs->kind) {
        case AST_IDENTIFIER:
        case AST_LOCAL_IDENTIFIER:
        case AST_FLOAT:
        case AST_INTEGER:
            tmp = rhs;
            break;
        default:
            AstReportAs(rhs, &saveinfo);
            tmp = AstTempLocalVariable("_temp_", NULL);
            newseq = NewAST(AST_SEQUENCE, AstAssign(tmp, rhs), AstAssign(lhs, tmp));
            *astptr = newseq;
            AstReportDone(&saveinfo);
            break;
        }
    } else {
        tmp = TransformAssignChainNoCasts(&ast->right);
        if (!tmp) return tmp;
        AstReportAs(rhs, &saveinfo);
        newseq = NewAST(AST_SEQUENCE, ast->right, AstAssign(lhs, tmp));
        *astptr = newseq;
        AstReportDone(&saveinfo);
    }
    return tmp;
}

// transform an assignment chain like a := b := expr
// into (b := expr, a := b)
// we do this recursively:
//    b := expr  -> (tmp := expr, b:=tmp) if expr is not an assign
//    a := (tmp := expr, b:=tmp) -> ( (tmp := expr, b:=tmp), a:=tmp)

static AST *
TransformAssignChainWithCasts(AST **astptr)
{
    AST *ast = *astptr;
    AST *tmp = NULL;
    AST *lhs, *rhs;
    AST *newseq;
    ASTReportInfo saveinfo;
    
    if (!ast) return tmp;
    if (ast->kind != AST_ASSIGN) return tmp;
    lhs = ast->left;
    rhs = ast->right;
    // watch out for Spin function calls masquerading as identifiers
    if (curfunc->language == LANG_SPIN_SPIN1 && rhs->kind == AST_IDENTIFIER) {
        AST *typ = ExprType(rhs);
        if (IsFunctionType(typ)) {
            rhs = ast->right = NewAST(AST_FUNCCALL, rhs, NULL);
        }
        //printf("typ->kind == %d\n", typ->kind);
    }
    if (rhs->kind != AST_ASSIGN) {
        tmp = lhs;
    } else {
        tmp = TransformAssignChainWithCasts(&ast->right);
        if (!tmp) return tmp;
        AstReportAs(ast, &saveinfo);
        newseq = NewAST(AST_SEQUENCE, ast->right, AstAssign(lhs, tmp));
        *astptr = newseq;
        tmp = lhs;
        AstReportDone(&saveinfo);
    }
    return tmp;
}

static AST *
AstTypedAssignStmt(AST *var, AST *val, AST *typ)
{
    AST *assign = AstAssign(var, NewAST(AST_CAST, typ, val));
    return NewAST(AST_STMTLIST, assign, NULL);
}

void
doSimplifyAssignments(AST **astptr, int insertCasts, int atTopLevel)
{
    AST *ast = *astptr;
    AST *preseq = NULL;
    AST *lhs, *rhs;
    AST *typ;
    AST *chain;
    int siz;
    int lhsTopLevel, rhsTopLevel;
    ASTReportInfo saveinfo;
    
    if (!ast) return;

    lhsTopLevel = rhsTopLevel = atTopLevel;
    switch (ast->kind) {
    case AST_VA_ARG:
        lhsTopLevel = rhsTopLevel = 0;
        // transform va_arg(a, typ) to
        //  { tmp = *(typ *)a; a += sizeof(typ); tmp; }
        typ = ast->left;
        lhs = ast->right;
        siz = TypeSize(typ);
        AstReportAs(ast, &saveinfo);
        rhs = AstTempLocalVariable("_arg_", typ);
        chain = AstAssign(lhs, AstOperator('+', lhs, AstInteger(siz)));
        chain = NewAST(AST_SEQUENCE, chain, rhs);
        chain = NewAST(AST_SEQUENCE,
                       AstAssign(rhs,
                                 NewAST(AST_ARRAYREF,
                                        NewAST(AST_CAST, NewAST(AST_PTRTYPE, typ, NULL), lhs),
                                        AstInteger(0))),
                       chain);
        *astptr = ast = chain;
        AstReportDone(&saveinfo);
        break;
    case AST_SEQUENCE:
    case AST_STMTLIST:
    case AST_OTHER:
        lhsTopLevel = rhsTopLevel = 1;
        break;
    case AST_ASSIGN:
        lhsTopLevel = rhsTopLevel = 0;
        break;
    case AST_EXPRLIST:
        lhsTopLevel = 0;
        break;
    case AST_CASEITEM:
    case AST_WHILE:
    case AST_DOWHILE:
        lhsTopLevel = 0;
        rhsTopLevel = 1;
        break;
    case AST_COMMENT:
    case AST_COMMENTEDNODE:
        // no change to level status
        break;
    case AST_CONDRESULT: {
        AST *typ = ExprType(ast);
        if (typ && TypeSize(typ) > LONG_SIZE) {
            AstReportAs(ast, &saveinfo);
            if (IsArrayType(typ)) {
                typ = ArrayToPointerType(typ);
            }
            AST *cond = ast->left;
            AST *ifcond = ast->right->left;
            AST *elsecond = ast->right->right;
            AST *tempvar = AstTempLocalVariable("_temp_", typ);
            AST *newif =
                NewAST(AST_IF, cond,
                       NewAST(AST_THENELSE,
                              AstTypedAssignStmt(tempvar, ifcond, typ),
                              AstTypedAssignStmt(tempvar, elsecond, typ)));

            AST *stmt = NewAST(AST_STMTLIST,
                               newif,
                               NewAST(AST_STMTLIST, tempvar, NULL));
            *astptr = ast = stmt;
            AstReportDone(&saveinfo);
        }
        lhsTopLevel = rhsTopLevel = 1;
        break;
    }
    default:
        lhsTopLevel = rhsTopLevel = 0;
        break;
    }

    doSimplifyAssignments(&ast->left, insertCasts, lhsTopLevel);
    doSimplifyAssignments(&ast->right, insertCasts, rhsTopLevel);

    if (ast->kind == AST_ASSIGN_INIT) {
        AST *typ = ExprType(ast->left);
        if (IsRefType(typ)) {
            if (ast->right && (ast->right->kind != AST_CAST || !IsRefType(ast->right->left))) {
                AST *base = ast->right;
                ast->right = NewAST(AST_CAST, typ, NewAST(AST_ADDROF, base, NULL));
            }
        }
        ast->kind = AST_ASSIGN;
    }
    if (ast->kind == AST_ASSIGN) {
        int op = ast->d.ival;
        int size;
        lhs = ast->left;
        rhs = ast->right;
        size = TypeSize(ExprType(lhs));
        if (IsConstExpr(lhs)) {
            if (IsIdentifier(lhs)) {
                // check for CON := x
                ERROR(ast, "assignment to constant `%s'", GetUserIdentifierName(lhs));
            } else {
                ERROR(ast, "assignment to constant value");
            }
        }
        if (lhs->kind == AST_EXPRLIST) {
            // multiple assignment; verify it's a simple assignment
            // note that we cannot check the number of assignments
            // here because we have not yet done type inference, so
            // the count for functions is unknown
            if (op != K_ASSIGN) {
                ERROR(ast, "Multiple assignment with modification not permitted");
                return;
            }
        }
        else if (op && (op != K_ASSIGN || size > LONG_SIZE) )
        {
            bool change = false;
            AstReportAs(ast, &saveinfo);
            // if B has side effects,
            // transform A op= B
            // into tmp = B, A op= tmp
            // then if A has side effects, further decompose
            // those so eventually we can get A = A' op tmp
            if (rhs && ExprHasSideEffects(rhs) && !TraditionalBytecodeOutput()) {
                AST *oldrhs = rhs;
                rhs = ExtractSideEffects(rhs, &preseq);
                if (! (rhs == oldrhs && preseq == NULL) ) {
                    change = true;
                }
            }
            if ((ExprHasSideEffects(lhs) || IsBoolOp(op)) && !TraditionalBytecodeOutput()) {
                if (curfunc && IsSpinLang(curfunc->language) && rhs && !IsConstExpr(rhs)) {
                    // Spin must maintain a strict evaluation order
                    AST *temp = AstTempLocalVariable("_temp_", NULL);
                    AST *p2;
                    p2 = AstAssign(temp, rhs);
                    if (preseq) {
                        preseq = NewAST(AST_SEQUENCE, preseq, p2);
                    } else {
                        preseq = p2;
                    }
                    rhs = temp;
                    change = true;
                }
                AST *oldlhs = lhs;
                lhs = ExtractSideEffects(lhs, &preseq);
                if (lhs != oldlhs) {
                    change = true;
                }
            }
            if (op == 0 || op == K_ASSIGN) {
                if (change) {
                    ast = AstAssign(lhs, rhs);
                }
            } else {
                if (rhs) {
                    ast = AstAssign(lhs, AstOperator(op, DupAST(lhs), rhs));
                } else {
                    ast = AstAssign(lhs, AstOperator(op, NULL, DupAST(lhs)));
                }
            }
            if (preseq) {
                ast = NewAST(AST_SEQUENCE, preseq, ast);
            }
            AstReportDone(&saveinfo);
            *astptr = ast;
            lhs = ast->left;
            rhs = ast->right;
        }
        // check for special cases like local.byte[N] := X where N is a constant
        if (lhs && lhs->kind == AST_ARRAYREF) {
            AST *newexpr = CheckSimpleArrayref(lhs);
            if (newexpr) {
                ast->left = lhs = newexpr;
            }
        }
        if (!atTopLevel) {
            // some special cases
            AST *typ = ExprType(*astptr);
            bool needTransform = false;
            if (TypeSize(typ) > LONG_SIZE) {
                needTransform = true;
            } else {
                needTransform = TraditionalBytecodeOutput() ? insertCasts : !insertCasts;
            }
            if (needTransform) {
                AST *tmp = insertCasts ? TransformAssignChainWithCasts(astptr) : TransformAssignChainNoCasts(astptr);
                if (tmp) {
                    // we probably have something like a := b := expr
                    // which got transformed to (tmp := expr, b := tmp, a := tmp);
                    // make sure to return tmp as the final result
                    AST *ast = *astptr;
                    ast = NewAST(AST_SEQUENCE, ast, tmp);
                    *astptr = ast;
                }
            }
        }
    }
    /* optimize K_LOGIC_AND and K_LOGIC_OR (which do not short-circuit)
       to K_BOOL_AND/K_BOOL_OR (which do short-circuit) if we can
    */
    if (ast->kind == AST_OPERATOR) {
        int op;
        op = ast->d.ival;
        switch (op) {
        case K_LOGIC_AND:
        case K_LOGIC_OR:
        case K_LOGIC_XOR:
            if (ExprHasSideEffects(ast->right)) {
                if ( gl_output != OUTPUT_BYTECODE || (op == K_LOGIC_XOR || gl_interp_kind == INTERP_KIND_NUCODE) ) {
                    // bytecode has native support for logic AND/OR,
                    // but transform them for other backends
                    AstReportAs(ast, &saveinfo);
                    ast->left = AstOperator(K_NE, ast->left, AstInteger(0));
                    ast->right = AstOperator(K_NE, ast->right, AstInteger(0));
                    if (op == K_LOGIC_XOR) {
                        ast->d.ival = '^';
                    } else if (op == K_LOGIC_AND) {
                        ast->d.ival = '&';
                    } else {
                        ast->d.ival = '|';
                    }
                    AstReportDone(&saveinfo);
                }
            } else {
                if (op == K_LOGIC_XOR) {
                    ast->d.ival = K_BOOL_XOR;
                } else if (op == K_LOGIC_AND) {
                    ast->d.ival = K_BOOL_AND;
                } else {
                    ast->d.ival = K_BOOL_OR;
                }
            }
            break;
        case K_INCREMENT:
        case K_DECREMENT:
        {
            int newop = (op == K_DECREMENT) ? '-' : '+';
            /* for ++ and --, handle floats and 64 bit integers by turning into i = i+1 */
            /* specifically: i++ -> (tmp = i, i = i+1, tmp)
               ++i -> (i = i+1, i) */
            if (ast->left) {
                /* i++ case */
                AST *typ = ExprType(ast->left);
                if (typ) {
                    if (IsFloatType(typ) || IsInt64Type(typ) || IsBoolType(typ)) {
                        AstReportAs(ast, &saveinfo);
                        AST *temp = AstTempLocalVariable("_temp_", typ);
                        AST *save = AstAssign(temp, ast->left);
                        AST *update = AstAssign(ast->left, AstOperator(newop, ast->left, AstInteger(1)));

                        ast = *astptr = NewAST(AST_SEQUENCE,
                                               NewAST(AST_SEQUENCE, save, update),
                                               temp);
                        AstReportDone(&saveinfo);
                    }
                }
            } else {
                AST *ident = ast->right;
                AST *typ = ExprType(ident);
                if (typ) {
                    if (IsFloatType(typ) || IsInt64Type(typ) || IsBoolType(typ)) {
                        ast->kind = AST_ASSIGN;
                        ast->d.ival = K_ASSIGN;
                        ast->left = ident;
                        ast->right = AstOperator(newop, ident, AstInteger(1));
                    }
                }
            }
        }
        default:
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////
// high level transforms common to most languages
/////////////////////////////////////////////////////////////////////////
void
DoHLTransforms(Function *F)
{
    Function *savecurrent = curfunc;
    curfunc = F;
    // correct the function's numresults
    AST *functype;
    functype = F->overalltype;
    if (functype) {
        int n = FuncNumResults(functype);
        if (F->numresults != n) {
            F->numresults = n;
        }
    }
    // fix up references
    fixReferences(&F->body, 0);
    // simplify assignments within the function
    int insertCasts = !IsSpinLang(F->language);
    doSimplifyAssignments(&F->body, insertCasts, 1);
    curfunc = savecurrent;
}
