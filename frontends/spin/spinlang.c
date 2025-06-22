/*
 * Spin to C/C++ converter
 * Copyright 2011-2025 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for Spin specific features
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

extern AST *ParsePrintStatement(AST *ast); /* in basiclang.c */

bool
IsLocalVariableEx(AST *ast, Symbol **symout) {
    Symbol *sym;

    if (symout) *symout = NULL;
    if (!ast) return false;
    if (ast->kind == AST_STMTLIST || ast->kind == AST_SEQUENCE) {
        while (ast && ast->right) {
            ast = ast->right;
        }
        ast = ast->left;
    }
    if (!ast) return false;
    if (ast->kind == AST_LOCAL_IDENTIFIER) {
        ast = ast->left;
    }
    switch (ast->kind) {
    case AST_CAST:
        return IsLocalVariableEx(ast->right, symout);
    case AST_IDENTIFIER:
        sym = LookupSymbol(ast->d.string);
        if (!sym) return false;
        switch (sym->kind) {
        case SYM_RESULT:
        case SYM_LOCALVAR:
        case SYM_PARAMETER:
            if (symout) *symout = sym;
            return true;
        default:
            return false;
        }
        break;
    case AST_RANGEREF:
    case AST_METHODREF:
    case AST_ARRAYREF:
        if (IsLocalVariableEx(ast->left, symout)) {
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

bool
IsLocalVariable(AST *ast) {
    return IsLocalVariableEx(ast, NULL);
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
#define LONGMOVE_THRESHOLD 6
#define LONGMOVE_THRESHOLD_BYTECODE 1

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
    int i, n;
    SymbolTable *srctab, *dsttab;
    ASTReportInfo saveinfo;
    int srcarray = 0;
    int dstarray = 0;
    
    ast = ast->right; // ast->left is the longmove function
    dst = ast->left; ast = ast->right;
    if (!ast || !dst) return false;
    src = ast->left; ast = ast->right;
    if (!ast || !src) return false;
    count = ast->left; ast = ast->right;
    if (ast || !count) return false;
    if (!IsConstExpr(count)) return false;
    n = EvalConstExpr(count);
    if (n > ( (TraditionalBytecodeOutput()) ? LONGMOVE_THRESHOLD_BYTECODE : LONGMOVE_THRESHOLD) || n <= 0) return false;

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
    case SYM_LABEL:
        srcarray = true;
        // fall through
    case SYM_VARIABLE:
        srctab = &current->objsyms;
        break;
    case SYM_RESULT:
    case SYM_PARAMETER:
    case SYM_LOCALVAR:
        srctab = &curfunc->localsyms;
        break;
    default:
        return false;
    }
    switch(symd->kind) {
    case SYM_LABEL:
        dstarray = true;
        // fall through
    case SYM_VARIABLE:
        dsttab = &current->objsyms;
        break;
    case SYM_RESULT:
    case SYM_PARAMETER:
    case SYM_LOCALVAR:
        dsttab = &curfunc->localsyms;
        break;
    default:
        return false;
    }
    if (IsArrayType(ExprType(src))) {
        srcarray = TypeSize(ExprType(src)) / LONG_SIZE;
    }
    if (IsArrayType(ExprType(dst))) {
        dstarray = TypeSize(ExprType(dst)) / LONG_SIZE;
    }
    AstReportAs(dst, &saveinfo);
    srcoff = syms->offset;
    dstoff = symd->offset;
    sequence = NULL;
    i = 0;
    for(;;) {
        AST *src_id = AstIdentifier(syms->our_name);
        AST *dst_id = AstIdentifier(symd->our_name);
        AST *src_tmp, *dst_tmp;
        if (srcarray) {
            src_tmp = NewAST(AST_ARRAYREF, src_id, AstInteger(i));
        } else {
            src_tmp = src_id;
        }
        if (dstarray) {
            dst_tmp = NewAST(AST_ARRAYREF, dst_id, AstInteger(i));
        } else {
            dst_tmp = dst_id;
        }
        i++;
        assign = AstAssign(dst_tmp, src_tmp);
        sequence = AddToList(sequence, NewAST(AST_SEQUENCE, assign, NULL));
        --n;
        if (n == 0) break;
        srcoff += 4;
        dstoff += 4;
        if (!dstarray) {
            symd = FindSymbolByOffsetAndKind(dsttab, dstoff, symd->kind);
        }
        if (!srcarray) {
            syms = FindSymbolByOffsetAndKind(srctab, srcoff, syms->kind);
        }
        if (!symd || !syms) {
            AstReportDone(&saveinfo);
            return false;
        }
        srcoff = syms->offset;
        dstoff = symd->offset;
    }
    *astptr = sequence;
    /* the longmove probably indicates a COG will be reading these
       variables */
    current->volatileVariables = 1;
    AstReportDone(&saveinfo);
    return true;
}

//
// fix up any parameters passed by reference
// this applies both to calls into BASIC/C++ functions
// but not to ^Struct parameters, which are more like
// pointer types
//
static void
FixupSpinParameterTypes(AST **astptr, AST *ast)
{
    (void)FixupFunccallTypes(ast, false);
}

// special processing for various Spin functions
//   longmove(@x, @y, n) gets turned into direct moves
//   pinw(p, n) gets turned into _drvw(p, n) if we can prove
//     n is just 1 bit (not done here any more, it's language independent)
// returns true if anything changed
// ast is the FUNCCALL ast, with ast->left known to be an identifier
// *astptr should match ast
//

static bool
SpinFunctionSpecialCase(AST **astptr, AST *ast)
{
    const char *name = ast->left->d.string;
    if (!strcasecmp(name, "longmove")) {
        return TransformLongMove(astptr, ast);
    }
    return false;
}

// create a local array for 
static void
SetLocalArray(Function *fdef, Symbol *sym, AST *body)
{
    if (sym->kind == SYM_PARAMETER) {
        if (!fdef->parmarray) {
            fdef->parmarray = NewTemporaryVariable("_parm_", &fdef->local_var_counter);
        }
        fdef->localarray = fdef->parmarray;
    } else if (sym->kind == SYM_LOCALVAR && (!body || IsAddrRef(body, sym)) ) {
        if (!fdef->localarray) {
            fdef->localarray = NewTemporaryVariable("_local_", &fdef->local_var_counter);
        }
    }
}

/*
 * scan a function body for various special conditions
 * "expectType" marks a parameter type that is expected
 */
extern AST *FunctionAddress(AST *); // in types.c
extern AST *BuildMethodPointer(AST *); // in becommon.c

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
    case AST_METHODREF:
    case AST_CONSTREF:
        /* we can (and should) update the lhs though */
        ScanFunctionBody(fdef, body->left, body, NULL);
        /* and transform plain foo.bar into foo.bar() if it is a function */
        if (upper && upper->kind != AST_FUNCCALL && upper->kind != AST_ADDROF)
        {
            AST *typ = ExprType(body);
            if (typ && IsFunctionType(typ)) {
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
                LANGUAGE_WARNING(LANG_SPIN_SPIN2, body, "omitting () on function calls is a fastspin extension");
            }
        }
        return;
    case AST_ADDROF:
    case AST_ABSADDROF:
    case AST_ARRAYREF:
    case AST_FIELDADDR:
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
                } else if (sym && sym->kind == SYM_LABEL) {
                    Label *lab = (Label *)sym->v.ptr;
                    if (lab->type == ast_type_void) {
                        WARNING(body, "Applying @ to RES memory `%s' is not supported in standard Spin", GetIdentifierName(ast));
                    }
                    if (upper->kind == AST_MEMREF) {
                        int refalign = TypeAlign(upper->left);
                        int labalign = TypeAlign(lab->type);
                        if ( (refalign > labalign) && !gl_p2 ) {
                            lab->flags |= (LABEL_NEEDS_EXTRA_ALIGN|LABEL_USED_IN_SPIN);
                            WARNING(body, "Label is dereferenced with greater alignment than it was declared with");
                        }
                    }
                } else if (sym && sym->kind == SYM_FUNCTION) {
                    // insert an explicit function address
                    AST *getaddr = FunctionAddress(body->left);
                    fdef->is_leaf = 0;
                    if (upper->right == body) {
                        upper->right = getaddr;
                        return;
                    }
                    if (upper->left == body) {
                        upper->left = getaddr;
                        return;
                    }
                    WARNING(body, "Internal error, function address may not be computed correctly");
                }
            }
        } else if (ast->kind == AST_RESULT) {
            /* hack to make F32.spin work: it expects all variables
               to be arranged as result,parameters,locals
               so force all of those into the _parm_ array
            */
            if (!fdef->parmarray) {
                fdef->parmarray = NewTemporaryVariable("_parm_", &fdef->local_var_counter);
            }
            fdef->localarray = fdef->parmarray;
            if (!fdef->result_in_parmarray) {
                fdef->result_in_parmarray = 1;
                fdef->resultexpr = NewAST(AST_RESULT, NULL, NULL);
                fdef->result_used = 1;
            }
        } else if (ast->kind == AST_METHODREF) {
            // fix up @foo.bar
            AST *typ = ExprType(ast);
            if (IsFunctionType(typ)) {
                AST *getaddr = BuildMethodPointer(body);
                fdef->is_leaf = 0;
                if (upper->right == body) {
                    upper->right = getaddr;
                    return;
                }
                if (upper->left == body) {
                    upper->left = getaddr;
                    return;
                }
                WARNING(body, "Internal error, function address may not be computed correctly");
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
                Label *L = (Label *)sym->v.ptr;
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
            if (sym->kind == SYM_FUNCTION && upper && upper->kind != AST_FUNCCALL && upper->kind != AST_ADDROF) {
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
                LANGUAGE_WARNING(LANG_SPIN_SPIN2, body, "omitting () on function calls is a fastspin extension");
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
                Function *calledFunc = (Function *)calledSym->v.ptr;
                AST *calledParam = calledFunc->params;
                if (calledFunc->numparams < 0 && NoVarargsOutput()) {
                    // special case: varargs function with NoVarargsOutput()
                    // normally this is handled in CheckTypes(), but Spin doesn't call CheckTypes
                    ScanFunctionBody(fdef, body->left, body, NULL);
                    ScanFunctionBody(fdef, body->right, body, NULL);
                    CheckTypes(body);
                    return;
                }
                while (calledParam && actualParamList) {
                    AST *paramId = calledParam->left;

                    if (paramId->kind == AST_VARARGS) {
                        // for bytecode, we have to handle specially
                        if (NoVarargsOutput()) {
                            ERROR(body, "varargs calls in Spin need special handling");
                        }
                    }
                    actualParam = actualParamList->left;
                    paramType = NULL;
                    if (paramId && paramId->kind == AST_IDENTIFIER) {
                        Symbol *paramSym = FindSymbol(&calledFunc->localsyms, paramId->d.string);
                        if (paramSym && paramSym->kind == SYM_PARAMETER) {
                            // symbol value is its expected type
                            paramType = (AST *)paramSym->v.ptr; 
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
    case AST_BRKDEBUG:
        // FIXME: probably should properly scan expressions, but this is awkward because there
        // are pseudo-function calls like uhex(n) inside them
        return;
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
doSpinTransform(AST **astptr, int level, AST *parent)
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
    case AST_INLINEASM:
        return;
    case AST_EXPRLIST:
        doSpinTransform(&ast->left, level == 2 ? level : 0, ast);
        doSpinTransform(&ast->right, level == 2 ? level : 0, ast);
        break;
    case AST_THENELSE:
        doSpinTransform(&ast->left, level, ast);
        doSpinTransform(&ast->right, level, ast);
        break;
    case AST_RETURN:
    case AST_THROW:
        doSpinTransform(&ast->left, 0, ast);
        break;
    case AST_CATCH:
        doSpinTransform(&ast->left, level, ast);
        // Interpreter's exception system is too simplistic for this,
        // in that you can only set up a catch on a call
        // and can't differentiate a normal return from an abort.
        if (gl_output != OUTPUT_BYTECODE /*|| gl_interp_kind == INTERP_KIND_NUCODE */ ) {
            curfunc->force_locals_to_stack = 1; // if we do a catch we will want data on stack
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
        }
        break;
    case AST_IF:
    case AST_WHILE:
    case AST_DOWHILE:
        doSpinTransform(&ast->left, 0, ast);
        doSpinTransform(&ast->right, level, ast);
        break;
    case AST_COUNTREPEAT:
        ast = ast->right; // from value
        doSpinTransform(&ast->left, 0, ast);
        ast = ast->right; // to value
        doSpinTransform(&ast->left, 0, ast);
        ast = ast->right; // step value
        doSpinTransform(&ast->left, 0, ast);
        doSpinTransform(&ast->right, level, ast);

        /* now fix it up */
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_STMTLIST:
        doSpinTransform(&ast->left, 1, ast);
        doSpinTransform(&ast->right, 1, ast);
        break;
    case AST_CASE:
    case AST_CASETABLE:
    {
        AST *list = ast->right;
        const char *case_name = ast->kind == AST_CASETABLE ? "case_fast" : NULL;
        doSpinTransform(&ast->left, 0, ast);
        AstReportAs(ast, &saveinfo); // any newly created AST nodes should reflect debug info from this one
#ifdef NEVER // handled in CreateSwitch now        
        if (ast->left->kind != AST_IDENTIFIER && ast->left->kind != AST_ASSIGN) {
            AST *var = AstTempLocalVariable("_tmp_", NULL);
            ast->left = AstAssign(var, ast->left);
        }
#endif        
        while (list) {
            AST *caseitem;

            ASSERT_AST_KIND(list,AST_STMTLIST,);

            caseitem = list->left;
            doSpinTransform(&caseitem->left, level, ast);
            doSpinTransform(&caseitem->right, level, ast);
            list = list->right;
        }
        *ast = *CreateSwitch(ast, case_name);
        AstReportDone(&saveinfo);
        break;
    }
    case AST_COGINIT:
        if (IsSpinCoginit(ast, &func) && func) {
            func->cog_task = 1;
            func->force_static = 1;
        }
        doSpinTransform(&ast->left, 2, ast);
        doSpinTransform(&ast->right, 2, ast);
        return;
    case AST_TASKINIT:
        if (IsSpinCoginit(ast, &func) && func) {
            func->force_static = 1;
        } else {
            ERROR(ast, "TASKSPIN requires a function");
        }
        doSpinTransform(&ast->left, 2, ast);
        doSpinTransform(&ast->right, 2, ast);
        /* convert TASKSPIN(t, func(a, b), stack)
           into __builtin_taskstartl(t, stack, @func, 2, a, b) */
        {
            ASTReportInfo saveinfo;
            AST *taskid, *funccall, *args, *stack;
            AST *funcname, *funcptr;
            int nargs;
            AstReportAs(ast, &saveinfo);
            funcname = AstIdentifier("__builtin_taskstartl");
            args = ast->left;
            if (!args) { ERROR(ast, "bad TASKSPIN command"); goto taskdone; }
            taskid = args->left; args = args->right;
            if (!args) { ERROR(ast, "bad TASKSPIN command"); goto taskdone; }
            funccall = args->left; args = args->right;
            if (!args) { ERROR(ast, "bad TASKSPIN command"); goto taskdone; }
            stack = args->left; args = args->right;
            if (args) { ERROR(ast, "bad TASKSPIN command"); goto taskdone; }
            if (!funccall || funccall->kind != AST_FUNCCALL) {
                ERROR(ast, "expected method call in TASKSPIN");
                goto taskdone;
            }
            funcptr = NewAST(AST_ADDROF, funccall->left, NULL);
            args = funccall->right;
            nargs = AstListLen(args);
            args = NewAST(AST_EXPRLIST, taskid,
                          NewAST(AST_EXPRLIST, stack,
                                 NewAST(AST_EXPRLIST, funcptr,
                                        NewAST(AST_EXPRLIST, AstInteger(nargs),
                                               args))));
            funccall = NewAST(AST_FUNCCALL, funcname, args);
            *ast = *funccall;
        taskdone:
            AstReportDone(&saveinfo);
        }
        return;
    case AST_FUNCCALL:
        if (level == 0) {
            /* check for void functions here; if one is called,
               pretend it returned 0 */
            sym = FindFuncSymbol(ast, NULL, 0);
            if (sym && sym->kind == SYM_FUNCTION) {
                Function *f = (Function *)sym->v.ptr;
                if (GetFunctionReturnType(f) == ast_type_void) {
                    AST *seq = NewAST(AST_SEQUENCE, ast, AstInteger(0));
                    *astptr = seq;
                }
            }
        }
        /* check for longmove(@x, @y, n) where n is a small
           number */
        if (level == 1 && ast->left && ast->left->kind == AST_IDENTIFIER
            && SpinFunctionSpecialCase(astptr, ast) )
        {
            ast = *astptr;
        }
        doSpinTransform(&ast->left, 0, ast);
        doSpinTransform(&ast->right, 0, ast);

        /* fix up any implicit pointers in the parameter list */
        FixupSpinParameterTypes(astptr, ast);
        break;
    case AST_POSTSET:
    {
        /* x~ is the same as (tmp = x, x = 0, tmp) */
        /* x~~ is (tmp = x, x = -1, tmp) */
        /* x\y is (tmp = x, x = y, tmp) */
        AST *target;
        AST *tmp;
        AST *seq1, *seq2;

        AstReportAs(ast, &saveinfo);
        ast->right = target = DupAST(ast->right);
        if (IsConstExpr(ast->left)) {
            ERROR(ast, "left side of \\ may not be constant");
        }
        if (level == 1) {
            // at toplevel we can ignore the old result
            // Do this even if we could do it natively, 
            // since normal assignment is generally faster
            AST *typ = ExprType(ast->left);
            if (typ && IsRefType(typ)) {
                typ = typ->left;
            }
            int destSize = typ ? TypeSize(typ) : LONG_SIZE;
            if (destSize > LONG_SIZE) {
                // use memset or memcpy?
                if (IsConstExpr(target)) {
                    *astptr = ast = MakeOperatorCall(struct_memset,
                                                     NewAST(AST_ABSADDROF,
                                                            ast->left, NULL),
                                                     target,
                                                     AstInteger(destSize));
                } else if (TypeSize(target) != destSize) {
                    ERROR(ast, "Type size mismatch");
                } else {
                    // memcpy
                    *astptr = ast = MakeOperatorCall(struct_copy,
                                                     NewAST(AST_ABSADDROF,
                                                            ast->left, NULL),
                                                     NewAST(AST_ABSADDROF,
                                                            target, NULL),
                                                     AstInteger(destSize));
                }
            } else {
                *astptr = ast = AstAssign(ast->left, target);
            }
        } else if (TraditionalBytecodeOutput()) {
            // Do nothing except transform the children
            AstReportDone(&saveinfo);
            doSpinTransform(&ast->left, 0, ast);
            doSpinTransform(&ast->right, 0, ast);
            break; // Prevent infinite recursion
        } else {
            tmp = AstTempLocalVariable("_tmp_", NULL);

            seq1 = NewAST(AST_SEQUENCE,
                          AstAssign(tmp, DupAST(ast->left)),
                          AstAssign(ast->left, target));
            seq2 = NewAST(AST_SEQUENCE, seq1, tmp);
            *astptr = ast = seq2;
        }
        AstReportDone(&saveinfo);
        // if we did a transform,
        // we may have a range reference in here, so do the
        // transform on the result
        doSpinTransform(astptr, level, ast);
	break;
    }
    case AST_ASSIGN:
        if (ast->left && ast->left->kind == AST_FIELDREF) {
            // change field[p][i] := x to _set_field(p, i, x)
            AST *func = AstIdentifier("_set_field");
            AST *p = ast->left->left;
            AST *i = ast->left->right;
            AST *x = ast->right;
            AST *args = NewAST(AST_EXPRLIST, p,
                               NewAST(AST_EXPRLIST, i,
                                      NewAST(AST_EXPRLIST, x, NULL)));
            AstReportAs(ast, &saveinfo);
            *astptr = ast = NewAST(AST_FUNCCALL, func, args);
            AstReportDone(&saveinfo);
        } else if (ast->left && ast->left->kind == AST_RANGEREF) {
            *astptr = ast = TransformRangeAssign(ast->left, ast->right, ast->d.ival, level == 1);
        }
        if (ast->left && IsIdentifier(ast->left)) {
            const char *name = GetUserIdentifierName(ast->left);
            if (IsVoidType(ExprType(ast->left))) {
                WARNING(ast->left, "assigning value to RES memory");
            }
            if ( !strcasecmp("send", name) ) {
                curfunc->sets_send = 1;
            } else if (!strcasecmp("recv", name)) {
                curfunc->sets_recv = 1;
            }
        }
        doSpinTransform(&ast->left, 2, ast);  // the "2" inhibits some optimizations
        doSpinTransform(&ast->right, 0, ast);
        break;
    case AST_METHODREF:
    case AST_CONSTREF:
        doSpinTransform(&ast->left, 0, ast);
        doSpinTransform(&ast->right, 0, ast);
        // if we have v.x and v is an array, convert to v[0].x
        if (ast->left && ast->left->kind == AST_IDENTIFIER) {
            AST *objtype = ExprType(ast->left);
            if(IsArrayType(objtype)) {
                AST *lookup;
                AstReportAs(ast, &saveinfo);
                lookup = NewAST(AST_ARRAYREF, ast->left, AstInteger(0));
                *ast = *NewAST(ast->kind, lookup, ast->right);
                AstReportDone(&saveinfo);
            }
        }
        /* check for references to abstract objects (which will have NULL pointers) */
        if (ast->left && ast->left->kind == AST_ARRAYREF && ast->left->left && ast->left->left->kind == AST_MEMREF)
        {
            AST *ref = ast->left->left;
            AST *methodname = ast->right;
            AST *basetyp = ref->left;
            AST *baseobj = ref->right;
            if (baseobj && baseobj->kind == AST_INTEGER && baseobj->d.ival == 0) {
                Module *P = GetClassPtr(basetyp);
                if (P && IsIdentifier(methodname)) {
                    Symbol *sym = LookupSymbolInTable(&P->objsyms,
                                                      GetIdentifierName(methodname));
                    if (sym) {
                        switch (sym->kind) {
                        case SYM_CONSTANT:
                        case SYM_FLOAT_CONSTANT:
                        case SYM_LABEL:
                            break;
                        case SYM_FUNCTION:
                            /* FIXME: at least in principle we could allow
                               calls to static functions, but punt for now */
                            /* fall through */
                        default:
                            ERROR(ast, "attempt to dereference abstract object");
                            break;
                        }
                    }
                }
            }
        }
        break;
    case AST_FIELDREF:
    {
        // transform FIELD[p][i] into _get_field(p, i)
        AstReportAs(ast, &saveinfo);
        AST *func = AstIdentifier("_get_field");
        AST *args = NewAST(AST_EXPRLIST, ast->left,
                           NewAST(AST_EXPRLIST, ast->right, NULL));
        *ast = *NewAST(AST_FUNCCALL, func, args);
        AstReportDone(&saveinfo);
    } break;
    case AST_RANGEREF:
        *astptr = ast = TransformRangeUse(ast);
        break;
    case AST_ALLOCA:
        doSpinTransform(&ast->right, 0, ast);
        curfunc->uses_alloca = 1;
        break;
    case AST_FIELDADDR:
        if (ast->left && ast->left->kind != AST_RANGEREF) {
            doSpinTransform(&ast->left, 0, ast);
        }
        if (IsLocalVariableEx(ast->left, &sym)) {
            curfunc->local_address_taken = 1;
            if (sym) {
                sym->flags |= SYMF_ADDRESSABLE;
            } else {
                curfunc->force_locals_to_stack = 1;
            }
        }
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        doSpinTransform(&ast->left, 0, ast);
        if (IsLocalVariableEx(ast->left, &sym)) {
            curfunc->local_address_taken = 1;
            if (sym) {
                sym->flags |= SYMF_ADDRESSABLE;
            } else {
                curfunc->force_locals_to_stack = 1;
            }
        }
        break;
    case AST_MEMREF:
        // add memory dereferences if necessary
        doSpinTransform(&ast->left, 0, ast);
        doSpinTransform(&ast->right, 0, ast);
        if (parent->kind != AST_ARRAYREF) {
            AST *dsttyp = ast->left;
            AST *srctyp = ExprType(ast->right);            
            if (IsInterfaceType(dsttyp) && !IsInterfaceType(srctyp)) {
                CoerceAssignTypes(ast, AST_CAST, &ast->right, dsttyp, srctyp, "interface conversion");
                ast->kind = AST_CAST;
                break;
            }
            AstReportAs(ast, &saveinfo);
            AST *dup = NewAST(AST_MEMREF, ast->left, ast->right);
            AST *deref = NewAST(AST_ARRAYREF, dup, AstInteger(0));
            AstReportDone(&saveinfo);
            *ast = *deref;
        }
        break;
    case AST_ARRAYREF:
        // array references like T[x] may actually
        // be a memory lookup if T is a typename
        // (this code is probably obsolete due to parser changes...)
        if (ast->left && ast->left->kind == AST_IDENTIFIER) {
            Symbol *sym = LookupSymbol(ast->left->d.string);
            AST *typ;
            if (sym) {
                int isLocal = 0;
                switch (sym->kind) {
                case SYM_TYPEDEF:
                {
                    AstReportAs(ast, &saveinfo);
                    // change this into a pointer cast
                    //AST *ptrtype = NewAST(AST_PTRTYPE, (AST *)sym->v.ptr, NULL);
                    AST *ptrtype = (AST *)sym->v.ptr;
                    AST *ptrcast = NewAST(AST_MEMREF, ptrtype, ast->right);
                    AST *deref = NewAST(AST_ARRAYREF, ptrcast, AstInteger(0));
                    AstReportDone(&saveinfo);
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
                        AstReportAs(ast, &saveinfo);
                        AST *ptr = NewAST(AST_ADDROF, ast->left, NULL);
                        AST *memref = NewAST(AST_MEMREF, typ, ptr);
                        *ast = *NewAST(AST_ARRAYREF, memref, ast->right);
                        if (isLocal) {
                            SetLocalArray(curfunc, sym, NULL);
                            sym->flags |= SYMF_ADDRESSABLE;
                            curfunc->local_address_taken = 1;
                        }
                        AstReportDone(&saveinfo);
                    }
                    break;
                default:
                    break;
                } 
            }
        } else if (ast->left && ast->left->kind == AST_MEMREF && IsConstExpr(ast->right)) {
            switch (parent->kind) {
            default:
                /* default used to be to do nothing, and only the cases below
                   were accepted; not sure why, but revert if problems */
            case AST_EXPRLIST:
            case AST_ASSIGN:
            case AST_OPERATOR:
            case AST_RETURN:
            {
                AST *newexpr = (level < 2) ? CheckSimpleArrayref(ast) : 0;
                if (newexpr) {
                    newexpr = TransformRangeUse(newexpr);
                    *astptr = ast = newexpr;
                }
                break;
            }
            case AST_ARRAYREF:
                /* the default used to be to not apply the transformation */
                /* I suspect the transform was interfering with array optimizations */
                /* so for now suppress it in ARRAYREFs, but I'm not certain
                   even that is required */
                break;
            }
        }
        doSpinTransform(&ast->left, 0, ast);
        doSpinTransform(&ast->right, 0, ast);
        break;
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
    {
        AST *typ = ExprType(ast);
        if (curfunc && !curfunc->stack_local && IsLocalVariable(ast)) {
            if (typ) {
                if (TypeGoesOnStack(typ)) {
                    curfunc->stack_local = 1;
                }
            }
        }
#ifdef OLD_REFERENCES        
        if (typ && IsRefType(typ)) {
            AST *deref;
            AstReportAs(ast, &saveinfo);
            deref = NewAST(AST_MEMREF, typ->left, DupAST(ast));
            deref = NewAST(AST_ARRAYREF, deref, AstInteger(0));
            *ast = *deref;
            AstReportDone(&saveinfo);
        }
#endif        
        break;
    }
    case AST_PRINTDEBUG:
    {
        AST *debugprint = CreatePrintfDebug(ast->left, ast->right);
        *astptr = ast = debugprint;
        if (debugprint) {
            doSpinTransform(&debugprint->left, 0, ast);
            doSpinTransform(&debugprint->right, 0, ast);
        }
        break;
    }
    case AST_SIZEOF:
    {
        AST *typ = ExprType(ast->left);
        if (typ && IsRefType(typ)) {
            ast->left = typ->left;
        }
        break;
    }
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
                AstReportAs(ast, &saveinfo);
                if (TraditionalBytecodeOutput()) {
                    lhsast = AstAssign(ast->right, NULL);
                    lhsast->d.ival = ast->d.ival;
                    *astptr = ast = lhsast;
                } else if (ExprHasSideEffects(ast->right)) {
                    AST *preseq = NULL;
                    AST *expr;
                    expr = ExtractSideEffects(ast->right, &preseq);
                    lhsast = DupAST(expr);
                    preseq = NewAST(AST_SEQUENCE, preseq, AstAssign(lhsast, expr));
                    *astptr = ast = preseq;
                } else {
                    lhsast = DupAST(ast->right);
                    *astptr = ast = AstAssign(lhsast, ast);
                }
                doSpinTransform(astptr, level, parent);
                AstReportDone(&saveinfo);
                break;
            default:
                break;
            }
        } else {
            AST *lhsast;
            switch (ast->d.ival) {
            case K_DECODE:
                AstReportAs(ast, &saveinfo);
                lhsast = AstOperator(K_SHL, AstInteger(1), ast->right);
                lhsast->lineidx = ast->lineidx;
                lhsast->lexdata = ast->lexdata;
                *astptr = ast = lhsast;
                AstReportDone(&saveinfo);
                break;
            }
        }
        if (ast->d.ival == K_SGNCOMP) {
            AST *tmpx, *tmpy;
            AST *seq1 = NULL;
            AST *seq2 = NULL;
            AST *lhsast;
            AstReportAs(ast, &saveinfo);
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
            AstReportDone(&saveinfo);
        } else if (ast->d.ival == K_BOOL_XOR) {
            // transform a XOR b to (a<>0) ^ (b<>0)
            AstReportAs(ast, &saveinfo);
            ast = AstOperator('^',
                              AstOperator(K_NE, ast->left, AstInteger(0)),
                              AstOperator(K_NE, ast->right, AstInteger(0)));
            *astptr = ast;
            AstReportDone(&saveinfo);
        }
        /* fall through */
    default:
        doSpinTransform(&ast->left, 0, ast);
        doSpinTransform(&ast->right, 0, ast);
        break;
    }
}

void
SpinTransform(Function *func)
{
    InitGlobalFuncs();
    
    // simplify assignments: this is required for some of
    // the other passes to work
    DoHLTransforms(func);
    
    // spin specific stuff
    doSpinTransform(&func->body, 1, func->body);

    // ScanFunctionBody is left over from older code
    // it should probably be merged in with doSpinTransform
    /* check for special conditions */
    ScanFunctionBody(func, func->body, NULL, NULL);

    /* if we put the locals into an array, record the size of that array */
    if (func->localarray) {
        func->localarray_len += func->numlocals;
    }
}
