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

static AST *basic_print_float;
static AST *basic_print_string;
static AST *basic_print_integer;
static AST *basic_print_unsigned;
static AST *basic_print_char;
static AST *basic_print_nl;
static AST *basic_put;

static AST *float_add;
static AST *float_sub;
static AST *float_mul;
static AST *float_div;
static AST *float_cmp;
static AST *float_fromuns;
static AST *float_fromint;
static AST *float_toint;
static AST *float_abs;
static AST *float_neg;

static AST *string_cmp;
static AST *string_concat;
static AST *make_methodptr;
static AST *gc_alloc;
static AST *gc_free;

static int
IsBasicString(AST *typ)
{
    if (typ == NULL) return 0;
    if (typ == ast_type_string) return 1;
    if (typ->kind == AST_MODIFIER_CONST || typ->kind == AST_MODIFIER_VOLATILE) {
        return IsBasicString(typ->left);
    }
    return 0;
}

static AST *
addPrintCall(AST *seq, AST *func, AST *expr)
{
    AST *elem;
    AST *funccall = NewAST(AST_FUNCCALL, func,
                           expr ? NewAST(AST_EXPRLIST, expr, NULL) : NULL);
    elem = NewAST(AST_SEQUENCE, funccall, NULL);
    return AddToList(seq, elem);
}

static AST *
addPutCall(AST *seq, AST *func, AST *expr, int size)
{
    AST *elem;
    AST *params;
    AST *sizexpr = AstInteger(size);
    // take the address of expr (or of expr[0] for arrays)
    if (IsArrayType(ExprType(expr))) {
        expr = NewAST(AST_ARRAYREF, expr, AstInteger(0));
    }
    expr = NewAST(AST_ADDROF, expr, NULL);
    // pass it to the basic_put function
    params = NewAST(AST_EXPRLIST, expr, NULL);
    params = AddToList(params,
                       NewAST(AST_EXPRLIST, sizexpr, NULL));
    
    AST *funccall = NewAST(AST_FUNCCALL, func, params);
    elem = NewAST(AST_SEQUENCE, funccall, NULL);
    return AddToList(seq, elem);
}

static void
doBasicTransform(AST **astptr)
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
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    case AST_COUNTREPEAT:
        // convert repeat count into a for loop
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_RANGEREF:
        *astptr = ast = TransformRangeUse(ast);
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        {
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
    case AST_FUNCCALL:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        if (1)
        {
            // the parser treats a(x) as a function call (always), but in
            // fact it may be an array reference; change it to one if applicable
            AST *left = ast->left;
            AST *index = ast->right;
            AST *typ;
        
            typ = ExprType(left);
            if (typ && (IsPointerType(typ) || IsArrayType(typ))) {
                ast->kind = AST_ARRAYREF;
                if (!index || index->kind != AST_EXPRLIST) {
                    ERROR(ast, "Internal error: expected expression list in array subscript");
                    return;
                }
                // reduce a single item expression list, if necessary
                if (index->right != NULL) {
                    ERROR(index, "Multi-dimensional arrays are not supported\n");
                    return;
                }
                index = index->left;
                // and now we have to convert the array reference to
                // BASIC style (subtract one)
                index = AstOperator('-', index, AstInteger(1));
                ast->right = index;
            }
        }
        break;
    case AST_METHODREF:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        if (IsPointerType(ast->left)) {
            WARNING(ast, "Needs a pointer dereference");
        }
        break;
    case AST_PRINT:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        {
            // convert PRINT to a series of calls to basic_print_xxx
            AST *seq = NULL;
            AST *type;
            AST *exprlist = ast->left;
            AST *expr;
            while (exprlist) {
                if (exprlist->kind != AST_EXPRLIST) {
                    ERROR(exprlist, "internal error in print list");
                }
                expr = exprlist->left;
                exprlist = exprlist->right;
                // PUT gets translated to a PRINT with an AST_HERE node
                // this requests that we write out the literal bytes of
                // an expression
                if (expr->kind == AST_HERE) {
                    // request to put literal data
                    int size;
                    expr = expr->left;
                    size = TypeSize(ExprType(expr));
                    seq = addPutCall(seq, basic_put, expr, size);
                    continue;
                }
                if (expr->kind == AST_CHAR) {
                    expr = expr->left;
                    if (IsConstExpr(expr) && EvalConstExpr(expr) == 10) {
                        seq = addPrintCall(seq, basic_print_nl, NULL);
                    } else {
                        seq = addPrintCall(seq, basic_print_char, expr);
                    }
                    continue;
                }
                type = ExprType(expr);
                if (!type) {
                    ERROR(ast, "Unknown type in print");
                    continue;
                }
                if (IsFloatType(type)) {
                    seq = addPrintCall(seq, basic_print_float, expr);
                } else if (IsStringType(type)) {
                    seq = addPrintCall(seq, basic_print_string, expr);
                } else if (IsGenericType(type)) {
                    // create a hex call
                    AST *ast;
                    AST *params;

                    params = NewAST(AST_EXPRLIST, expr,
                                    NewAST(AST_EXPRLIST, AstInteger(16), NULL));
                    ast = NewAST(AST_FUNCCALL, basic_print_unsigned, params);
                    seq = AddToList(seq, NewAST(AST_SEQUENCE, ast, NULL));
                } else if (IsUnsignedType(type)) {
                    seq = addPrintCall(seq, basic_print_unsigned, expr);
                } else if (IsIntType(type)) {
                    seq = addPrintCall(seq, basic_print_integer, expr);
                } else {
                    ERROR(ast, "Unable to print expression of this type");
                }
            }
            *astptr = ast = seq;
        }
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
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        if (0 != (func = IsSpinCoginit(ast))) {
            func->cog_task = 1;
            func->force_static = 1;
        }
        break;
    default:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    }
}

bool VerifyIntegerType(AST *astForError, AST *typ, const char *opname)
{
    if (!typ)
        return true;
    if (IsIntType(typ))
        return true;
    // for now, accept generic types too as if they were integer
    // perhaps this should give a warning?
    if (IsGenericType(typ))
        return true;
    ERROR(astForError, "Expected integer type for parameter of %s", opname);
    return false;
}

// create a call to function func with parameters ast->left, ast->right
// there is an optional 3rd argument too
static AST *
MakeOperatorCall(AST *func, AST *left, AST *right, AST *extraArg)
{
    AST *call;
    AST *params = NULL;

    if (!func) {
        ERROR(left, "Internal error, NULL parameter");
        return AstInteger(0);
    }
    if (left) {
        params = AddToList(params, NewAST(AST_EXPRLIST, left, NULL));
    }
    if (right) {
        params = AddToList(params, NewAST(AST_EXPRLIST, right, NULL));
    }
    if (extraArg) {
        params = AddToList(params, NewAST(AST_EXPRLIST, extraArg, NULL));
    }
    call = NewAST(AST_FUNCCALL, func, params);
    return call;
}

// do a promotion when we already know the size of the original type
static AST *dopromote(AST *expr, int numbytes, int operator)
{
    int shiftbits = numbytes * 8;
    AST *promote;
    if (shiftbits == 32) {
        return expr; // nothing to do
    }
    promote = AstOperator(operator, expr, AstInteger(shiftbits));
    return promote;
}
// force a promotion from a small integer type to a full 32 bits
static AST *forcepromote(AST *type, AST *expr)
{
    int tsize;
    int op;
    if (!type) {
        return expr;
    }
    if (!IsIntType(type)) {
        ERROR(expr, "internal error in forcecpromote");
    }
    tsize = TypeSize(type);
    op = IsUnsignedType(type) ? K_ZEROEXTEND : K_SIGNEXTEND;
    return dopromote(expr, tsize, op);
}

//
// insert promotion code under AST for either the left or right type
// if "force" is nonzero then we will always promote small integers,
// otherwise we promote only if their sizes do not match
// return the final type
//
AST *MatchIntegerTypes(AST *ast, AST *lefttype, AST *righttype, int force) {
    int lsize = TypeSize(lefttype);
    int rsize = TypeSize(righttype);
    AST *rettype = lefttype;
    int leftunsigned = IsUnsignedType(lefttype);
    int rightunsigned = IsUnsignedType(righttype);
    
    force = force || (lsize != rsize);
    
    if (lsize < 4 && force) {
        if (IsConstExpr(ast->right)) {
            return lefttype;
        }
        if (leftunsigned) {
            ast->left = dopromote(ast->left, lsize, K_ZEROEXTEND);
            lefttype = ast_type_unsigned_long;
        } else {
            ast->left = dopromote(ast->left, lsize, K_SIGNEXTEND);
            lefttype = ast_type_long;
        }
        rettype = righttype;
    }
    if (rsize < 4 && force) {
        if (IsConstExpr(ast->left)) {
            return righttype;
        }
        if (rightunsigned) {
            ast->right = dopromote(ast->right, rsize, K_ZEROEXTEND);
            righttype = ast_type_unsigned_long;
        } else {
            ast->right = dopromote(ast->right, rsize, K_SIGNEXTEND);
            righttype = ast_type_long;
        }
        rettype = lefttype;
    }
    if (leftunsigned && rightunsigned) {
        return rettype;
    } else {
        return ast_type_long;
    }
}

static AST *
domakefloat(AST *typ, AST *ast)
{
    AST *ret;
    if (!ast) return ast;
    if (IsGenericType(typ)) return ast;
    if (gl_fixedreal) {
        ret = AstOperator(K_SHL, ast, AstInteger(G_FIXPOINT));
        return ret;
    }
    ast = forcepromote(typ, ast);
    if (IsUnsignedType(typ)) {
        ret = MakeOperatorCall(float_fromuns, ast, NULL, NULL);
    } else {
        ret = MakeOperatorCall(float_fromint, ast, NULL, NULL);
    }
    return ret;
}

static AST *
dofloatToInt(AST *ast)
{
    AST *ret;
    if (gl_fixedreal) {
        // FIXME: should we round here??
        ret = AstOperator(K_SAR, ast, AstInteger(G_FIXPOINT));
        return ret;
    }
    ast = MakeOperatorCall(float_toint, ast, NULL, NULL);
    return ast;
}

bool MakeBothIntegers(AST *ast, AST *ltyp, AST *rtyp, const char *opname)
{
    if (IsFloatType(ltyp)) {
        ast->left = dofloatToInt(ast->left);
        ltyp = ast_type_long;
    }
    if (IsFloatType(rtyp)) {
        ast->right = dofloatToInt(ast->right);
        rtyp = ast_type_long;
    }
    return VerifyIntegerType(ast, ltyp, opname) && VerifyIntegerType(ast, rtyp, opname);
}

static AST *
HandleTwoNumerics(int op, AST *ast, AST *lefttype, AST *righttype)
{
    int isfloat = 0;
    int isalreadyfixed = 0;
    AST *scale = NULL;
    
    if (IsFloatType(lefttype)) {
        isfloat = 1;
        if (!IsFloatType(righttype)) {
            if (gl_fixedreal && (op == '*' || op == '/')) {
                // no need for fixed point mul, just do regular mul
                isalreadyfixed = 1;
                if (op == '/') {
                    // fixed / int requires no scale
                    scale = AstInteger(0);
                }
            } else {
                ast->right = domakefloat(righttype, ast->right);
            }
        }
    } else if (IsFloatType(righttype)) {
        isfloat = 1;
        if (gl_fixedreal && (op == '*' || op == '/')) {
            // no need for fixed point mul, regular mul works
            isalreadyfixed = 1;
            if (op == '/') {
                // int / fixed requires additional scaling
                scale = AstInteger(2*G_FIXPOINT);
            }
        } else {
            ast->left = domakefloat(lefttype, ast->left);
        }
    }
    if (isfloat) {
        // FIXME need to call appropriate funcs here
        switch (op) {
        case '+':
            if (!gl_fixedreal) {
                *ast = *MakeOperatorCall(float_add, ast->left, ast->right, NULL);
            }
            break;
        case '-':
            if (!gl_fixedreal) {
                *ast = *MakeOperatorCall(float_sub, ast->left, ast->right, NULL);
            }
            break;
        case '*':
            if (!isalreadyfixed) {
                *ast = *MakeOperatorCall(float_mul, ast->left, ast->right, NULL);
            }
            break;
        case '/':
            if (gl_fixedreal) {
                if (!isalreadyfixed) {
                    scale = AstInteger(G_FIXPOINT);
                }
            }
            *ast = *MakeOperatorCall(float_div, ast->left, ast->right, scale);
            break;
        default:
            ERROR(ast, "internal error unhandled operator");
            break;
        }
        return ast_type_float;
    }
    if (!MakeBothIntegers(ast, lefttype, righttype, "operator"))
        return NULL;
    return MatchIntegerTypes(ast, lefttype, righttype, 0);
}

bool IsUnsignedConst(AST *ast)
{
    if (!IsConstExpr(ast)) {
        return false;
    }
    if (EvalConstExpr(ast) < 0) {
        return false;
    }
    return true;
}

void CompileComparison(int op, AST *ast, AST *lefttype, AST *righttype)
{
    int isfloat = 0;
    int leftUnsigned = 0;
    int rightUnsigned = 0;
    
    if (IsFloatType(lefttype)) {
        if (!IsFloatType(righttype)) {
            ast->right = domakefloat(righttype, ast->right);
        }
        isfloat = 1;
    } else if (IsFloatType(righttype)) {
        ast->left = domakefloat(righttype, ast->left);
        isfloat = 1;
    }
    if (isfloat) {
        if (gl_fixedreal) {
            // we're good
        } else {
            ast->left = MakeOperatorCall(float_cmp, ast->left, ast->right, NULL);
            ast->right = AstInteger(0);
        }
        return;
    }
    // allow for string comparison
    if (IsBasicString(lefttype) || IsBasicString(righttype)) {
        if (!CompatibleTypes(lefttype, righttype)) {
            ERROR(ast, "illegal comparison with string");
            return;
        }
        ast->left = MakeOperatorCall(string_cmp, ast->left, ast->right, NULL);
        ast->right = AstInteger(0);
        return;
    }
    
    if (!MakeBothIntegers(ast, lefttype, righttype, "comparison")) {
        return;
    }
    // need to widen the types
    ast->left = forcepromote(lefttype, ast->left);
    ast->right = forcepromote(righttype, ast->right);
    
    //
    // handle unsigned/signed comparisons here
    //
    leftUnsigned = IsUnsignedType(lefttype);
    rightUnsigned = IsUnsignedType(righttype);
    
    if (leftUnsigned || rightUnsigned) {
        if ( (leftUnsigned && (rightUnsigned || IsUnsignedConst(ast->right)))
             || (rightUnsigned && IsUnsignedConst(ast->left)) )
        {
            switch (op) {
            case '<':
                ast->d.ival = K_LTU;
                break;
            case '>':
                ast->d.ival = K_GTU;
                break;
            case K_LE:
                ast->d.ival = K_LEU;
                break;
            case K_GE:
                ast->d.ival = K_GEU;
                break;
            default:
                break;
            }
        } else {
            // cannot do unsigned comparison
            // signed comparison will work if the sizes are < 32 bits
            // if both are 32 bits, we need to do something else
            int lsize = TypeSize(lefttype);
            int rsize = TypeSize(righttype);
            if (lsize == 4 && rsize == 4) {
                WARNING(ast, "signed/unsigned comparison may not work properly");
            }
        }
    }

}

AST *CoerceOperatorTypes(AST *ast, AST *lefttype, AST *righttype)
{
    AST *rettype = lefttype;
    int op;
    
    //assert(ast->kind == AST_OPERATOR)
    if (!ast->left) {
        rettype = righttype;
    }
    op = ast->d.ival;
    switch(op) {
    case K_SAR:
    case K_SHL:
    case '&':
    case '|':
    case '^':
        if (lefttype && IsFloatType(lefttype)) {
            ast->left = dofloatToInt(ast->left);
            lefttype = ast_type_long;
        }
        if (righttype && IsFloatType(righttype)) {
            ast->right = dofloatToInt(ast->right);
            righttype = ast_type_long;
        }
        if (!MakeBothIntegers(ast, lefttype, righttype, "bit operation")) {
            return NULL;
        }
        if (ast->d.ival == K_SAR && lefttype && IsUnsignedType(lefttype)) {
            ast->d.ival = K_SHR;
        }
        return lefttype;
    case '+':
        if (IsStringType(lefttype) && IsStringType(righttype)) {
            *ast = *MakeOperatorCall(string_concat, ast->left, ast->right, NULL);
            return lefttype;
        }
    case '-':
    case '*':
    case '/':
        return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
    case K_SIGNEXTEND:
        VerifyIntegerType(ast, righttype, "sign extension");
        return ast_type_long;
    case K_ZEROEXTEND:
        VerifyIntegerType(ast, righttype, "zero extension");
        return ast_type_unsigned_long;
    case '<':
    case K_LE:
    case K_EQ:
    case K_NE:
    case K_GE:
    case '>':
        CompileComparison(ast->d.ival, ast, lefttype, righttype);
        return ast_type_long;
    case K_ABS:
    case K_NEGATE:
        if (IsFloatType(rettype)) {
            if (!gl_fixedreal) {
                if (op == K_ABS) {
                    *ast = *MakeOperatorCall(float_abs, ast->right, NULL, NULL);
                } else {
                    *ast = *MakeOperatorCall(float_neg, ast->right, NULL, NULL);
                }
                return rettype;
            }
            return rettype;
        } else {
            if (!VerifyIntegerType(ast, rettype, "abs"))
                return NULL;
            ast->right = forcepromote(rettype, ast->right);
            if (IsUnsignedType(rettype) && op == K_ABS) {
                *ast = *ast->right; // ignore the ABS
                return ast_type_unsigned_long;
            }
            return ast_type_long;
        }
    case K_ASC:
        if (!CompatibleTypes(righttype, ast_type_string)) {
            ERROR(ast, "expected string argument to ASC");
        } else {
            AST *newast;
            if (ast->right && ast->right->kind == AST_STRING) {
                // literal: fix it up here
                newast = AstInteger(ast->right->d.string[0]);
                *ast = *newast;
            } else {
                newast = NewAST(AST_MEMREF, ast_type_byte, ast->right);
                *ast = *newast;
            }
        }
        return ast_type_long;
    default:
        if (!MakeBothIntegers(ast, lefttype, righttype, "operator")) {
            return NULL;
        }
        return MatchIntegerTypes(ast, lefttype, righttype, 1);
    }
}

//
// modifies *astptr, originally of type srctype,
// to have type desttype by introducing any
// necessary casts
//
AST *CoerceAssignTypes(AST *line, int kind, AST **astptr, AST *desttype, AST *srctype)
{
    AST *expr = *astptr;
    const char *msg;
    
    if (kind == AST_RETURN) {
        msg = "return";
    } else if (kind == AST_FUNCCALL) {
        msg = "parameter passing";
    } else {
        msg = "assignment";
    }

    if (!desttype || !srctype || IsGenericType(desttype) || IsGenericType(srctype)) {
        return desttype;
    }
    if (IsFloatType(desttype)) {
        if (IsIntType(srctype)) {
            *astptr = domakefloat(srctype, expr);
            srctype = ast_type_float;
        }
    }
    // allow floats to be cast as ints
    if (IsIntType(desttype) && IsFloatType(srctype)) {
        if (IsUnsignedType(desttype)) {
            expr = MakeOperatorCall(float_fromuns, expr, NULL, NULL);
        } else {
            expr = MakeOperatorCall(float_fromint, expr, NULL, NULL);
        }
        *astptr = expr;
        return desttype;
    }
        
    // 
    if (!CompatibleTypes(desttype, srctype)) {
        ERROR(line, "incompatible types in %s", msg);
        return desttype;
    }
    if (IsConstType(desttype) && kind == AST_ASSIGN) {
        WARNING(line, "assignment to const object");
    }
    if (IsPointerType(srctype) && IsConstType(BaseType(srctype)) && !IsConstType(BaseType(desttype))) {
        WARNING(line, "%s discards const attribute from pointer", msg);
    }
    if (IsIntType(desttype)) {
        if (IsIntType(srctype)) {
            int lsize = TypeSize(desttype);
            int rsize = TypeSize(srctype);
            if (lsize > rsize) {
                if (IsUnsignedType(srctype)) {
                    *astptr = dopromote(expr, rsize, K_ZEROEXTEND);
                } else {
                    *astptr = dopromote(expr, rsize, K_SIGNEXTEND);
                }
            }
        }
        if (IsFloatType(srctype)) {
            ERROR(line, "cannot assign float to integer");
        }
    }
    return desttype;
}

static void
BuildMethodPointer(AST *ast)
{
    Symbol *sym;
    AST *objast;
    AST *funcaddr;
    
    sym = FindCalledFuncSymbol(ast, &objast, 0);
    if (!sym || sym->type != SYM_FUNCTION) {
        ERROR(ast, "Internal error, unable to find function address");
        return;
    }
    if (objast == NULL) {
        objast = NewAST(AST_SELF, NULL, NULL);
    } else {
        objast = NewAST(AST_ADDROF, objast, NULL);
    }
    // save off the current @ node
    funcaddr = NewAST(AST_ADDROF, ast->left, ast->right);
    // create a call
    *ast = *MakeOperatorCall(make_methodptr, objast, funcaddr, NULL);
}

//
// function for doing type checking and various kinds of
// type related manipulations. for example:
//
// signed/unsigned shift: x >> y  => signed shift if x is signed,
//                                   unsigned otherwise
// returns the most recent type signature
//
AST *CheckTypes(AST *ast)
{
    AST *ltype, *rtype;
    if (!ast) return NULL;

    ltype = CheckTypes(ast->left);
    rtype = CheckTypes(ast->right);
    switch (ast->kind) {
    case AST_GOTO:
        {
            AST *id = ast->left;
            if (!id || id->kind != AST_IDENTIFIER) {
                ERROR(ast, "Expected identifier in goto");
            } else {
                Symbol *sym = FindSymbol(&curfunc->localsyms, id->d.string);
                if (!sym || sym->type != SYM_LOCALLABEL) {
                    ERROR(id, "%s is not a local label", id->d.string);
                }
            }
        }
        return NULL;
    case AST_COGINIT:
        ltype = ast_type_long;
        break;
    case AST_OPERATOR:
        ltype = CoerceOperatorTypes(ast, ltype, rtype);
        break;
    case AST_ASSIGN:
        if (rtype) {
            ltype = CoerceAssignTypes(ast, AST_ASSIGN, &ast->right, ltype, rtype);
        }
        break;
    case AST_RETURN:
        {
            rtype = ltype; // type of actual expression
            ltype = GetFunctionReturnType(curfunc);
            ltype = CoerceAssignTypes(ast, AST_RETURN, &ast->left, ltype, rtype);
        }
        break;
    case AST_FUNCCALL:
        {
            AST *actualParamList = ast->right;
            AST *calledParamList;
            AST *expectType, *passedType;
            Function *func;
            Symbol *calledSym = FindCalledFuncSymbol(ast, NULL, 1);
            if (calledSym && calledSym->type == SYM_FUNCTION) {
                func = (Function *)calledSym->val;
                calledParamList = func->params;
                while (calledParamList && actualParamList) {
                    AST *paramId = calledParamList->left;
                    AST *actualParam = actualParamList->left;
                    expectType = NULL;
                    passedType = ExprType(actualParam);
                    if (paramId) {
                        if (paramId->kind == AST_IDENTIFIER) {
                            Symbol *paramSym = FindSymbol(&func->localsyms, paramId->d.string);
                            if (paramSym && paramSym->type == SYM_PARAMETER) {
                                expectType = (AST *)paramSym->val;
                            }
                        } else if (paramId->kind == AST_DECLARE_VAR) {
                            expectType = paramId->left;
                        }
                    }
                    if (expectType) {
                        CoerceAssignTypes(ast, AST_FUNCCALL, &actualParamList->left, expectType, passedType);
                    }
                    calledParamList = calledParamList->right;
                    actualParamList = actualParamList->right;
                }
                ltype = GetFunctionReturnType(func);
            } else {
                return NULL;
            }
        }
        break;
    case AST_FLOAT:
    case AST_TRUNC:
    case AST_ROUND:
        return ast_type_float;
    case AST_ISBETWEEN:
    case AST_INTEGER:
    case AST_HWREG:
    case AST_CONSTREF:
        return ast_type_long;
    case AST_BITVALUE:
        return ast_type_generic;
    case AST_STRING:
    case AST_STRINGPTR:
        return ast_type_string;
    case AST_ADDROF:
    case AST_ABSADDROF:
        if (IsFunctionType(ltype)) {
            BuildMethodPointer(ast);
            return ltype;
        }
        return NewAST(AST_PTRTYPE, ltype, NULL);
    case AST_ARRAYREF:
        {
            AST *lefttype = ExprType(ast->left);
            AST *basetype;
            if (!lefttype) {
                return NULL;
            }
            basetype = BaseType(lefttype);
            if (IsPointerType(lefttype)) {
                // force this to have a memory dereference
                AST *deref = NewAST(AST_MEMREF, basetype, ast->left);
                ast->left = deref;
            } else if (ast->kind == AST_MEMREF) {
                // nothing to do
            } else if (IsArrayType(lefttype)) {
                // nothing to do here
            } else {
                ERROR(ast, "Array dereferences a non-array object");
                return NULL;
            }
            return basetype;
        }
        break;
    case AST_NEW:
        // turn this into an alloc
        {
            AST *sizeExpr;
            AST *basetype;
            int baseSize;
            ltype = ast->left;
            basetype = BaseType(ltype);
            baseSize = TypeSize(basetype);
            if (IsConstExpr(ast->right)) {
                baseSize *= EvalConstExpr(ast->right);
                sizeExpr = AstInteger(baseSize);
            } else {
                sizeExpr = AstOperator('*', ast->right, AstInteger(baseSize));
            }
            *ast = *MakeOperatorCall(gc_alloc, sizeExpr, NULL, NULL);
        }
        break;
    case AST_DELETE:
        *ast = *MakeOperatorCall(gc_free, ast->left, NULL, NULL);
        ltype = ast_type_void;
        break;
    case AST_EXPRLIST:
    case AST_SEQUENCE:
    case AST_METHODREF:
    case AST_IDENTIFIER:
    case AST_CONSTANT:
        return ExprType(ast);
    default:
        break;
    }
    return ltype;
}

////////////////////////////////////////////////////////////////
static AST *
getBasicPrimitive(const char *name)
{
    AST *ast;

    ast = AstIdentifier(name);
    return ast;
}

void
BasicTransform(Module *Q)
{
    Module *savecur = current;
    Function *func;
    Function *savefunc = curfunc;

    if (!basic_print_integer) {
        if (gl_fixedreal) {
            basic_print_float = getBasicPrimitive("_basic_print_fixed");
            float_mul = getBasicPrimitive("_fixed_mul");
            float_div = getBasicPrimitive("_fixed_div");
        } else {
            basic_print_float = getBasicPrimitive("_basic_print_float");
            float_cmp = getBasicPrimitive("_float_cmp");
            float_add = getBasicPrimitive("_float_add");
            float_sub = getBasicPrimitive("_float_sub");
            float_mul = getBasicPrimitive("_float_mul");
            float_div = getBasicPrimitive("_float_div");
            float_fromuns = getBasicPrimitive("_float_fromuns");
            float_fromint = getBasicPrimitive("_float_fromint");
            float_toint = getBasicPrimitive("_float_trunc");
            float_abs = getBasicPrimitive("_float_abs");
            float_neg = getBasicPrimitive("_float_negate");
        }
        basic_print_integer = getBasicPrimitive("_basic_print_integer");
        basic_print_unsigned = getBasicPrimitive("_basic_print_unsigned");
        basic_print_string = getBasicPrimitive("_basic_print_string");
        basic_print_char = getBasicPrimitive("_basic_print_char");
        basic_print_nl = getBasicPrimitive("_basic_print_nl");
        basic_put = getBasicPrimitive("_basic_put");

        string_cmp = getBasicPrimitive("_string_cmp");
        string_concat = getBasicPrimitive("_string_concat");
        make_methodptr = getBasicPrimitive("_make_methodptr");
        gc_alloc = getBasicPrimitive("_gc_alloc");
        gc_free = getBasicPrimitive("_gc_free");
    }
    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;

        doBasicTransform(&func->body);
        CheckTypes(func->body);
    }
    curfunc = savefunc;
    current = savecur;
}
