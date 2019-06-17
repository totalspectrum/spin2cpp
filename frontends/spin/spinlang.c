/*
 * Spin to C/C++ converter
 * Copyright 2011-2019 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for Spin specific features
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

// Spin doesn't have pointers, so looking for arrays or
// pointers should find only arrays
#define IsArraySymbol(s) IsArrayOrPointerSymbol(s)

bool
IsLocalVariable(AST *ast) {
    Symbol *sym;

    if (ast->kind == AST_LOCAL_IDENTIFIER) {
        ast = ast->left;
    }
    switch (ast->kind) {
    case AST_IDENTIFIER:
        sym = LookupSymbol(ast->d.string);
        if (!sym) return false;
        switch (sym->kind) {
        case SYM_RESULT:
        case SYM_LOCALVAR:
        case SYM_PARAMETER:
            return true;
        default:
            return false;
        }
        break;
    case AST_ARRAYREF:
        if (IsLocalVariable(ast->left)) {
            // check for pointer dereference, which is not
            // actually going to cause us grief
            if (IsPointerType(ExprType(ast->left))) {
                return false;
            }
            return true;
        }
        return false;
    case AST_RESULT:
        return true;
    default:
        return false;
    }
}

/*
 * check for a variable having its address taken
 */
static bool
IsAddrRef(AST *body, Symbol *sym)
{
    if (body->kind == AST_ADDROF)
        return true;
    if (body->kind == AST_ARRAYREF && !IsArraySymbol(sym))
        return true;
    return false;
}

/*
 * change a small longmove call into a series of assignments
 * returns true if transform done
 */
#define LONGMOVE_THRESHOLD 4

static bool
TransformLongMove(AST **astptr, AST *ast)
{
    AST *dst;
    AST *src;
    AST *count;
    AST *sequence;
    AST *assign;
    Symbol *syms, *symd;
    int srcoff, dstoff;
    int n;
    SymbolTable *srctab, *dsttab;
    ASTReportInfo saveinfo;
    
    ast = ast->right; // ast->left is the longmove function
    dst = ast->left; ast = ast->right;
    if (!ast || !dst) return false;
    src = ast->left; ast = ast->right;
    if (!ast || !src) return false;
    count = ast->left; ast = ast->right;
    if (ast || !count) return false;
    if (!IsConstExpr(count)) return false;
    n = EvalConstExpr(count);
    if (n > LONGMOVE_THRESHOLD || n <= 0) return false;

    // check src and dst
    if (src->kind != AST_ADDROF && src->kind != AST_ABSADDROF) return false;
    src = src->left;
    if (!IsIdentifier(src)) return false;
    if (dst->kind != AST_ADDROF && dst->kind != AST_ABSADDROF) return false;
    dst = dst->left;
    if (!IsIdentifier(dst)) return false;

    // ok, we have the pattern we like
    syms = LookupAstSymbol(src, NULL);
    symd = LookupAstSymbol(dst, NULL);
    if (!syms || !symd) return false;

    switch(syms->kind) {
    case SYM_VARIABLE:
        srctab = &current->objsyms;
        break;
    case SYM_PARAMETER:
    case SYM_LOCALVAR:
        srctab = &curfunc->localsyms;
        break;
    default:
        return false;
    }
    switch(symd->kind) {
    case SYM_VARIABLE:
        dsttab = &current->objsyms;
        break;
    case SYM_PARAMETER:
    case SYM_LOCALVAR:
        dsttab = &curfunc->localsyms;
        break;
    default:
        return false;
    }
    AstReportAs(dst, &saveinfo);
    srcoff = syms->offset;
    dstoff = symd->offset;
    sequence = NULL;
    for(;;) {
        assign = AstAssign(AstIdentifier(symd->our_name),
                           AstIdentifier(syms->our_name));
        sequence = AddToList(sequence, NewAST(AST_SEQUENCE, assign, NULL));
        --n;
        if (n == 0) break;
        srcoff += 4;
        dstoff += 4;
        symd = FindSymbolByOffset(dsttab, dstoff);
        syms = FindSymbolByOffset(srctab, srcoff);
        if (!symd || !syms) {
            AstReportDone(&saveinfo);
            return false;
        }
    }
    *astptr = sequence;
    /* the longmove probably indicates a COG will be reading these
       variables */
    current->volatileVariables = 1;
    AstReportDone(&saveinfo);
    return true;
}

static void
SetLocalArray(Function *fdef, Symbol *sym, AST *body)
{
    if (sym->kind == SYM_PARAMETER) {
        if (!fdef->parmarray) {
            fdef->parmarray = NewTemporaryVariable("_parm_");
        }
        fdef->localarray = fdef->parmarray;
    } else if (sym->kind == SYM_LOCALVAR && (!body || IsAddrRef(body, sym)) ) {
        if (!fdef->localarray) {
            fdef->localarray = NewTemporaryVariable("_local_");
        }
    }
}

/*
 * scan a function body for various special conditions
 * "expectType" marks a parameter type that is expected
 */
static void
ScanFunctionBody(Function *fdef, AST *body, AST *upper, AST *expectType)
{
    AST *ast;
    Symbol *sym;

    if (!body)
        return;
    switch(body->kind) {
        // note that below we are making assumptions that we're still in the original function
        // these assumptions fail if we encounter a methodref or constref
    case AST_CONSTREF:
    case AST_METHODREF:
        /* we can (and should) update the lhs though */
        ScanFunctionBody(fdef, body->left, body, NULL);
        return;
    case AST_ADDROF:
    case AST_ABSADDROF:
    case AST_ARRAYREF:
        /* see if it's a parameter whose address is being taken */
        ast = body->left;
        if (IsIdentifier(ast)) {
            sym = FindSymbol(&fdef->localsyms, GetIdentifierName(ast));
            if (sym) {
                SetLocalArray(fdef, sym, body);
            } else {
                /* Taking the address of an object variable? That will make the object volatile. */
                sym = LookupSymbol(ast->d.string);
                if (sym && sym->kind == SYM_VARIABLE && IsAddrRef(body, sym)) {
                    current->volatileVariables = 1;
                } else if (sym && sym->kind == SYM_LABEL && upper->kind == AST_MEMREF) {
                    Label *lab = (Label *)sym->val;
                    int refalign = TypeAlignment(upper->left);
                    int labalign = TypeAlignment(lab->type);
                    if (refalign > labalign) {
                        lab->flags |= (LABEL_NEEDS_EXTRA_ALIGN|LABEL_USED_IN_SPIN);
                        WARNING(body, "Label is dereferenced with greater alignment than it was declared with");
                    }

                }
            }
        } else if (ast->kind == AST_RESULT) {
            /* hack to make F32.spin work: it expects all variables
               to be arranged as result,parameters,locals
               so force all of those into the _parm_ array
            */
            if (!fdef->parmarray) {
                fdef->parmarray = NewTemporaryVariable("_parm_");
            }
            fdef->localarray = fdef->parmarray;
            if (!fdef->result_in_parmarray) {
                fdef->result_in_parmarray = 1;
                fdef->resultexpr = NewAST(AST_RESULT, NULL, NULL);
                fdef->result_used = 1;
            }
        }
        // after an @, we probably cannot rely on any typing info??
        expectType = NULL;
        break;
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
        sym = FindSymbol(&fdef->localsyms,  GetIdentifierName(body));
        if (!sym) {
            sym = LookupSymbol(body->d.string);
        }
        if (sym) {
            if (sym->kind == SYM_LABEL) {
                Label *L = (Label *)sym->val;
                L->flags |= LABEL_USED_IN_SPIN;
            }
            // convert plain foo into foo[0] if foo is an array
            if (IsArraySymbol(sym) && (sym->kind == SYM_VARIABLE || sym->kind == SYM_LOCALVAR || sym->kind == SYM_LABEL))
            {
                if (upper && !(upper->kind == AST_ARRAYREF && upper->left == body)) {
                    AST *deref = NewAST(AST_ARRAYREF, body, AstInteger(0));
                    deref->lineidx = upper->lineidx;
                    deref->lexdata = upper->lexdata;
                    if (body == upper->left) {
                        upper->left = deref;
                    } else if (body == upper->right) {
                        upper->right = deref;
                    } else {
                        ERROR(body, "failed to dereference %s", body->d.string); 
                    }
                }
            }
            // convert plain IDENTIFIER into a FUNCCALL if it is a function
            // identifier
            if (sym->kind == SYM_FUNCTION && upper && upper->kind != AST_FUNCCALL) {
                AST *funccall;
                ASTReportInfo saveinfo;
                AstReportAs(body, &saveinfo);
                funccall = NewAST(AST_FUNCCALL, body, NULL);
                if (body == upper->left) {
                    upper->left = funccall;
                } else if (body == upper->right) {
                    upper->right = funccall;
                }
                AstReportDone(&saveinfo);
            }
        }
        break;
    case AST_STRINGPTR:
        return; // should be only constants inside, and we do not want to mess with them
        
    case AST_STRING:
        {
            // if we're passing to a string parameter, make sure
            // we're in a stringptr
            if (expectType && IsPointerType(expectType)) {
                if (upper->left == body) {
                    AST *newString;
                    ASTReportInfo saveinfo;
                    AstReportAs(body, &saveinfo);
                    newString = NewAST(AST_STRINGPTR,
                                       NewAST(AST_EXPRLIST, body, NULL),
                                       NULL);
                    AstReportDone(&saveinfo);
                    upper->left = newString;
                }
            }
        }
        return;
    case AST_FUNCCALL:
        {
            AST *actualParamList = body->right;
            AST *paramType;
            AST *actualParam;
            int n;
            
            // scan through parameters, adjusting for expected return types
            Symbol *calledSym = FindFuncSymbol(body, NULL, 1);
            if (calledSym && calledSym->kind == SYM_FUNCTION) {
                Function *calledFunc = (Function *)calledSym->val;
                AST *calledParam = calledFunc->params;
                while (calledParam && actualParamList) {
                    AST *paramId = calledParam->left;
                    
                    actualParam = actualParamList->left;
                    paramType = NULL;
                    if (paramId && paramId->kind == AST_IDENTIFIER) {
                        Symbol *paramSym = FindSymbol(&calledFunc->localsyms, paramId->d.string);
                        if (paramSym && paramSym->kind == SYM_PARAMETER) {
                            // symbol value is its expected type
                            paramType = (AST *)paramSym->val; 
                        }
                    }
                    ScanFunctionBody(fdef, actualParam, actualParamList, paramType);
                    // consume an appropriate number of parameters in the target
                    n = NumExprItemsOnStack(actualParam);
                    while (n > 0 && calledParam) {
                        calledParam = calledParam->right;
                        --n;
                    }
                    actualParamList = actualParamList->right;
                }
                // any leftovers? probably an error, but scan them anyway
                if (actualParamList) {
                    ScanFunctionBody(fdef, actualParamList->left, actualParamList, NULL);
                    ScanFunctionBody(fdef, actualParamList->right, actualParamList, NULL);
                }
                // now update the actual function call itself
                ScanFunctionBody(fdef, body->left, body, NULL);
                return;
            }
        }
        break;
    default:
        break;
    }
    ScanFunctionBody(fdef, body->left, body, expectType);
    ScanFunctionBody(fdef, body->right, body, expectType);
}

/*
 * SpinTransform
 * transform AST to reflect some oddities of the Spin language:
 * (1) It's legal to call a void function, just substitute 0 for the result
 * (2) Certain operators used at top level are changed into assignments
 * (3) Validate parameters to some builtins
 * (4) Turn AST_COUNTREPEAT into AST_FOR
 * (5) make sure expression in a case statement is a variable
 */
/* if level is 0, we are inside an expression
 * level == 1 at top level
 * level == 2 inside a coginit
 */
static void
doSpinTransform(AST **astptr, int level)
{
    AST *ast = *astptr;
    Symbol *sym;
    Function *func;
    ASTReportInfo saveinfo;
    
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        astptr = &ast->left;
        ast = *astptr;
    }
    if (!ast) return;

    switch (ast->kind) {
    case AST_EXPRLIST:
    case AST_THENELSE:
        doSpinTransform(&ast->left, level);
        doSpinTransform(&ast->right, level);
        break;
    case AST_RETURN:
    case AST_THROW:
        doSpinTransform(&ast->left, 0);
        break;
    case AST_CATCH:
        doSpinTransform(&ast->left, level);
        curfunc->local_address_taken = 1; // if we do a catch we will want data on stack
        AstReportAs(ast, &saveinfo); // any newly created AST nodes should reflect debug info from this one
        *astptr = ast = NewAST(AST_TRYENV,
                               NewAST(AST_CONDRESULT,
                                      AstOperator(K_EQ,
                                                  NewAST(AST_SETJMP, NULL, NULL),
                                                  AstInteger(0)),
                                      NewAST(AST_THENELSE,
                                             ast->left,
                                             NewAST(AST_CATCHRESULT, NULL, NULL))),
                               NULL);
        AstReportDone(&saveinfo);
        break;
    case AST_IF:
    case AST_WHILE:
    case AST_DOWHILE:
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, level);
        break;
    case AST_COUNTREPEAT:
        ast = ast->right; // from value
        doSpinTransform(&ast->left, 0);
        ast = ast->right; // to value
        doSpinTransform(&ast->left, 0);
        ast = ast->right; // step value
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, level);

        /* now fix it up */
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_STMTLIST:
        doSpinTransform(&ast->left, level);
        doSpinTransform(&ast->right, level);
        break;
    case AST_CASE:
    case AST_CASETABLE:
    {
        AST *list = ast->right;
        const char *case_name = ast->kind == AST_CASETABLE ? "case_fast" : NULL;
        doSpinTransform(&ast->left, level);
        AstReportAs(ast, &saveinfo); // any newly created AST nodes should reflect debug info from this one
        if (ast->left->kind != AST_IDENTIFIER && ast->left->kind != AST_ASSIGN) {
            AST *var = AstTempLocalVariable("_tmp_", NULL);
            ast->left = AstAssign(var, ast->left);
        }
        while (list) {
            AST *caseitem;
            if (list->kind != AST_STMTLIST) {
                ERROR(list, "internal error, expected list holder");
            }
            caseitem = list->left;
            doSpinTransform(&caseitem->left, 0);
            doSpinTransform(&caseitem->right, level);
            list = list->right;
        }
        *ast = *CreateSwitch(ast->left, ast->right, case_name);
        AstReportDone(&saveinfo);
        break;
    }
    case AST_COGINIT:
        if (IsSpinCoginit(ast, &func) && func) {
            func->cog_task = 1;
            func->force_static = 1;
        }
        doSpinTransform(&ast->left, 2);
        doSpinTransform(&ast->right, 2);
        break;
    case AST_FUNCCALL:
        if (level == 0) {
            /* check for void functions here; if one is called,
               pretend it returned 0 */
            sym = FindFuncSymbol(ast, NULL, 0);
            if (sym && sym->kind == SYM_FUNCTION) {
                Function *f = (Function *)sym->val;
                if (GetFunctionReturnType(f) == ast_type_void) {
                    AST *seq = NewAST(AST_SEQUENCE, ast, AstInteger(0));
                    *astptr = seq;
                }
            }
        }
        /* check for longmove(@x, @y, n) where n is a small
           number */
        if (level == 1 && ast->left && ast->left->kind == AST_IDENTIFIER
            && !strcasecmp(ast->left->d.string, "longmove"))
        {
            TransformLongMove(astptr, ast);
            ast = *astptr;
        }
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        break;
    case AST_POSTSET:
    {
        /* x~ is the same as (tmp = x, x = 0, tmp) */
        /* x~~ is (tmp = x, x = -1, tmp) */
        /* x\y is (tmp = x, x = y, tmp) */
        AST *target;
        AST *tmp;
        AST *seq1, *seq2;
        target = ast->right;
        if (level == 1) {
            // at toplevel we can ignore the old result
            *astptr = AstAssign(ast->left, target);
        } else {
            tmp = AstTempLocalVariable("_tmp_", NULL);

            seq1 = NewAST(AST_SEQUENCE,
                          AstAssign(tmp, ast->left),
                          AstAssign(ast->left, target));
            seq2 = NewAST(AST_SEQUENCE, seq1, tmp);
            *astptr = seq2;
        }
        // we may have a range reference in here, so do the
        // transform on the result
        doSpinTransform(astptr, level);
	break;
    }
    case AST_ASSIGN:
        if (ast->left && ast->left->kind == AST_RANGEREF) {
            *astptr = ast = TransformRangeAssign(ast->left, ast->right, level == 1);
        }
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        break;
    case AST_METHODREF:
    case AST_CONSTREF:
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        // if we have v.x and v is an array, convert to v[0].x
        if (ast->left && ast->left->kind == AST_IDENTIFIER) {
            AST *objtype = ExprType(ast->left);
            if(IsArrayType(objtype)) {
                AST *lookup;
                lookup = NewAST(AST_ARRAYREF, ast->left, AstInteger(0));
                *ast = *NewAST(ast->kind, lookup, ast->right);
            }
        }
        break;
    case AST_RANGEREF:
        *astptr = ast = TransformRangeUse(ast);
        break;
    case AST_ALLOCA:
        doSpinTransform(&ast->right, 0);
        curfunc->uses_alloca = 1;
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        doSpinTransform(&ast->left, 0);
        if (IsLocalVariable(ast->left)) {
            curfunc->local_address_taken = 1;
        }
        break;
    case AST_ARRAYREF:
        // array references like T[x] may actually
        // be a memory lookup if T is a typename
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        if (ast->left && ast->left->kind == AST_IDENTIFIER) {
            Symbol *sym = LookupSymbol(ast->left->d.string);
            AST *typ;
            if (sym) {
                int isLocal = 0;
                switch (sym->kind) {
                case SYM_TYPEDEF:
                {
                    // change this into a pointer cast
                    //AST *ptrtype = NewAST(AST_PTRTYPE, (AST *)sym->val, NULL);
                    AST *ptrtype = (AST *)sym->val;
                    AST *ptrcast = NewAST(AST_MEMREF, ptrtype, ast->right);
                    AST *deref = NewAST(AST_ARRAYREF, ptrcast, AstInteger(0));
                    *ast = *deref;
                    break;
                }                
                case SYM_LOCALVAR:
                case SYM_PARAMETER:
                    isLocal = 1;
                    /* fall through */
                case SYM_VARIABLE:
                    /* check for x[i] where x is not an array */
                    /* if found, convert to long[@x][i] */
                    typ = ExprType(ast->left);
                    if (!typ) typ = ast_type_long;
                    if (!IsArrayType(typ)) {
                        AST *ptr = NewAST(AST_ADDROF, ast->left, NULL);
                        AST *memref = NewAST(AST_MEMREF, typ, ptr);
                        *ast = *NewAST(AST_ARRAYREF, memref, ast->right);
                        if (isLocal) {
                            SetLocalArray(curfunc, sym, NULL);
                            curfunc->local_address_taken = 1;
                        }
                    }
                    break;
                default:
                    break;
                } 
            }
        }
        break;
        
    case AST_OPERATOR:
        if (level == 1) {
            AST *lhsast;
            switch (ast->d.ival) {
            case K_NEGATE:
            case K_ABS:
            case K_SQRT:
            case K_BIT_NOT:
            case K_BOOL_NOT:
            case K_DECODE:
            case K_ENCODE:
                lhsast = DupAST(ast->right);
                *astptr = ast = AstAssign(lhsast, ast);
                doSpinTransform(astptr, level);
                break;
            }
        } else {
            AST *lhsast;
            switch (ast->d.ival) {
            case K_DECODE:
                lhsast = AstOperator(K_SHL, AstInteger(1), ast->right);
                lhsast->lineidx = ast->lineidx;
                lhsast->lexdata = ast->lexdata;
                *astptr = ast = lhsast;
                break;
            }
        }
        if (ast->d.ival == K_SGNCOMP) {
            AST *tmpx, *tmpy;
            AST *seq1 = NULL;
            AST *seq2 = NULL;
            AST *lhsast;
            if (ExprHasSideEffects(ast->left)) {
                tmpx = AstTempLocalVariable("_temp_", NULL);
                seq1 = NewAST(AST_SEQUENCE, AstAssign(tmpx, ast->left), NULL);
            } else {
                tmpx = ast->left;
            }
            if (ExprHasSideEffects(ast->right)) {
                tmpy = AstTempLocalVariable("_temp_", NULL);
                seq2 = NewAST(AST_SEQUENCE, AstAssign(tmpy, ast->right), NULL);
            } else {
                tmpy = ast->right;
            }
            if (!tmpx || !tmpy) {
                ERROR(ast, "Internal error in <=>");
            }
            if (seq1) {
                if (seq2) {
                    seq1->right = seq2;
                }
            } else {
                seq1 = seq2;
            }
            // x <=> y means (x < y) ? -1 : (x == y) ? 0 : 1
            // or equivalently (x > y) ? 1 : (x < y)
            lhsast = NewAST(AST_CONDRESULT, AstOperator('>', tmpx, tmpy),
                            NewAST(AST_THENELSE, AstInteger(1), AstOperator('<', tmpx, tmpy)));
            if (seq1) {
                lhsast = NewAST(AST_SEQUENCE, seq1, lhsast);
            }
            *astptr = ast = lhsast;
        } else if (ast->d.ival == K_BOOL_XOR) {
            // transform a XOR b to (a<>0) ^ (b<>0)
            ast = AstOperator('^',
                              AstOperator(K_NE, ast->left, AstInteger(0)),
                              AstOperator(K_NE, ast->right, AstInteger(0)));
            *astptr = ast;
        }
        /* fall through */
    default:
        doSpinTransform(&ast->left, 0);
        doSpinTransform(&ast->right, 0);
        break;
    }
}

void
SpinTransform(Function *func)
{
    // simplify assignments: this is required for some of
    // the other passes to work
    SimplifyAssignments(&func->body);
        
    // spin specific stuff
    doSpinTransform(&func->body, 1);

    // ScanFunctionBody is left over from older code
    // it should probably be merged in with doSpinTransform
    /* check for special conditions */
    ScanFunctionBody(func, func->body, NULL, NULL);

    /* if we put the locals into an array, record the size of that array */
    if (func->localarray) {
        func->localarray_len += func->numlocals;
    }
}

