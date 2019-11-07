/*
 * Spin to C/C++ translator
 * Copyright 2011-2019 Total Spectrum Software Inc.
 * 
 * +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "common.h"
#include "preprocess.h"
#include "version.h"

AST *basic_get_float;
AST *basic_get_string;
AST *basic_get_integer;
AST *basic_read_line;

AST *basic_print_float;
AST *basic_print_string;
AST *basic_print_integer;
AST *basic_print_unsigned;
AST *basic_print_char;
AST *basic_print_nl;
AST *basic_put;

static AST *float_add;
static AST *float_sub;
static AST *float_mul;
static AST *float_div;
static AST *float_cmp;
static AST *float_fromuns;
static AST *float_fromint;
static AST *float_toint;
static AST *float_abs;
static AST *float_sqrt;
static AST *float_neg;

static AST *struct_copy;
static AST *string_cmp;
static AST *string_concat;
static AST *make_methodptr;
static AST *gc_alloc_managed;
static AST *gc_free;

static AST *funcptr_cmp;

static AST *BuildMethodPointer(AST *ast);

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
static AST *dopromote(AST *expr, int srcbytes, int destbytes, int operator)
{
    int shiftbits = srcbytes * 8;
    AST *promote;

    if (shiftbits == 32 && destbytes < 8) {
        return expr; // nothing to do
    }
    promote = AstOperator(operator, expr, AstInteger(shiftbits));
    if (destbytes == 8) {
        // at this point "promote" will contain a 4 byte value
        // now we need to convert it to an 8 byte value
        AST *highword;
        if (operator == K_ZEROEXTEND) {
            highword = AstInteger(0);
        } else {
            highword = AstOperator(K_SAR, promote, AstInteger(31));
        }
        promote = NewAST(AST_EXPRLIST,
                         promote,
                         NewAST(AST_EXPRLIST,
                                highword,
                                NULL));
    }
    return promote;
}
// do a narrowing operation to convert from A bytes to B bytes
// works by going A -> 32 bits -> B
static AST *donarrow(AST *expr, int A, int B, int isSigned)
{
    int shiftbits = (A - B) * 8;
    AST *promote;
    AST *narrow;

    if (A == 8 && B <= 4) {
        // have to narrow
        if (expr->kind != AST_EXPRLIST) {
            return expr;
        }
        expr = expr->left;
        A = 4;
    }
    shiftbits = (A - B) * 8;
    if (shiftbits == 0) {
        return expr; // nothing to do
    }
    promote = dopromote(expr, A, LONG_SIZE, isSigned ? K_ZEROEXTEND : K_SIGNEXTEND);
#if 0    
    if (shiftbits > 0) {
        int operator = isSigned ? K_SAR : K_SHR;
        narrow = AstOperator(K_SHL, promote, AstInteger(shiftbits));
        narrow = AstOperator(operator, narrow, AstInteger(shiftbits));
    } else {
        narrow = promote;
    }
#else
    narrow = promote;
#endif
    return narrow;
}

// force a promotion from a small integer type to a full 32 bits
static AST *forcepromote(AST *type, AST *expr)
{
    int tsize;
    int op;
    if (!type) {
        return expr;
    }
    if (!IsIntType(type) && !IsGenericType(type)) {
        ERROR(expr, "internal error in forcepromote");
    }
    tsize = TypeSize(type);
    op = IsUnsignedType(type) ? K_ZEROEXTEND : K_SIGNEXTEND;
    return dopromote(expr, tsize, LONG_SIZE, op);
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
    int finalsize = 4;
    
    force = force || (lsize != rsize);
    
    if (lsize < finalsize && force) {
        if (IsConstExpr(ast->right)) {
            return lefttype;
        }
        if (leftunsigned) {
            ast->left = dopromote(ast->left, lsize, finalsize, K_ZEROEXTEND);
            lefttype = ast_type_unsigned_long;
        } else {
            ast->left = dopromote(ast->left, lsize, finalsize, K_SIGNEXTEND);
            lefttype = ast_type_long;
        }
        rettype = righttype;
    }
    if (rsize < finalsize && force) {
        if (IsConstExpr(ast->left)) {
            return righttype;
        }
        if (rightunsigned) {
            ast->right = dopromote(ast->right, rsize, finalsize, K_ZEROEXTEND);
            righttype = ast_type_unsigned_long;
        } else {
            ast->right = dopromote(ast->right, rsize, finalsize, K_SIGNEXTEND);
            righttype = ast_type_long;
        }
        rettype = lefttype;
    }
    if (leftunsigned || rightunsigned) {
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
    if (IsConstExpr(ast)) {
        int x = EvalConstExpr(ast);
        // FIXME: assumes 32 bit floats only
        return AstFloat((float)x);
    }
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
    } else {
        // in C we need to promote both sides to  long
        if (curfunc && curfunc->language == LANG_C) {
            int operator;
            if (lefttype) {
                int leftsize = TypeSize(lefttype);
                if (leftsize < 4) {
                    operator = IsUnsignedType(lefttype) ? K_ZEROEXTEND : K_SIGNEXTEND;
                    ast->left = dopromote(ast->left, leftsize, LONG_SIZE, operator);
                    lefttype = ast_type_long;
                }
            }
            if (righttype) {
                int rightsize = TypeSize(righttype);
                if (rightsize < 4) {
                    operator = IsUnsignedType(righttype) ? K_ZEROEXTEND : K_SIGNEXTEND;
                    ast->right = dopromote(ast->right, rightsize, LONG_SIZE, operator);
                    righttype = ast_type_long;
                }
            }
        }
    }
            
    if (isfloat) {
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
    lefttype = MatchIntegerTypes(ast, lefttype, righttype, 0);
    if (IsUnsignedType(lefttype)) {
        if (op == K_MODULUS) {
            ast->d.ival = K_UNS_MOD;
        } else if (op == '/') {
            ast->d.ival = K_UNS_DIV;
        }
    }
    return lefttype;
}

static bool
IsSymbol(AST *expr)
{
    if (!expr) return false;
    if (IsIdentifier(expr))
        return true;
    return false;
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
        ast->left = domakefloat(lefttype, ast->left);
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

    if (IsPointerType(lefttype) || IsPointerType(righttype)) {
        /* FIXME: should handle floats and type promotion here!!! */
        leftUnsigned = rightUnsigned = 0;
        if ( (lefttype && IsFunctionType(lefttype->left)) || (righttype && IsFunctionType(righttype->left)) ) {
            ast->left = MakeOperatorCall(funcptr_cmp, ast->left, ast->right, NULL);
            ast->right = AstInteger(0);
            return;
        }
    } else {
        if (!MakeBothIntegers(ast, lefttype, righttype, "comparison")) {
            return;
        }
        // need to widen the types
        ast->left = forcepromote(lefttype, ast->left);
        ast->right = forcepromote(righttype, ast->right);
        leftUnsigned = IsUnsignedType(lefttype);
        rightUnsigned = IsUnsignedType(righttype);
    }
    
     //
    // handle unsigned/signed comparisons here
    //
    
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

static AST *ScalePointer(AST *type, AST *val)
{
    int size;
    if (!IsPointerType(type)) {
        ERROR(val, "internal error: expected pointer type");
        return val;
    }
    size = TypeSize(BaseType(type));
    val = AstOperator('*', val, AstInteger(size));
    return val;
}

// return the address of an array
AST *ArrayAddress(AST *expr)
{
    return NewAST(AST_ABSADDROF,
                  NewAST(AST_ARRAYREF, expr, AstInteger(0)),
                  NULL);
}

AST *StructAddress(AST *expr)
{
    if (expr->kind == AST_MEMREF) {
        return expr->right;
    }
    return NewAST(AST_ABSADDROF, expr, NULL);
}

// return the address of a function
AST *FunctionAddress(AST *expr)
{
    if (IsSymbol(expr)) {
        expr = NewAST(AST_ABSADDROF, expr, NULL);
        expr = BuildMethodPointer(expr);
    }
    return expr;
}

AST *FunctionPointerType(AST *typ)
{
    return NewAST(AST_PTRTYPE, typ, NULL);
}

//
// cast an array to a pointer type;
//
AST *ArrayToPointerType(AST *type)
{
    AST *modifier;
    if (type->kind == AST_ARRAYTYPE) {
        type = NewAST(AST_PTRTYPE, type->left, NULL);
    } else {
        modifier = NewAST(type->kind, NULL, NULL);
        modifier->left = ArrayToPointerType(type->left);
        type = modifier;
    }
    return type;
}

AST *ClassToPointerType(AST *type)
{
    AST *modifier;
    if (type->kind == AST_OBJECT) {
        type = NewAST(AST_PTRTYPE, type, NULL);
    } else {
        modifier = NewAST(type->kind, NULL, NULL);
        modifier->left = ClassToPointerType(type->left);
        type = modifier;
    }
    return type;
}

AST *CoerceOperatorTypes(AST *ast, AST *lefttype, AST *righttype)
{
    AST *rettype = lefttype;
    int op;

    // hmmm, should we automatically convert arrays to pointers here?
    // for current languages yes, eventually maybe not if we want
    // to support array arithmetic
    if (IsArrayType(lefttype)) {
        ast->left = ArrayAddress(ast->left);
        lefttype = ArrayToPointerType(lefttype);
    }
    if (IsArrayType(righttype)) {
        ast->right = ArrayAddress(ast->right);
        righttype = ArrayToPointerType(righttype);
    }
    if (IsFunctionType(lefttype) && !IsPointerType(lefttype)) {
        ast->left = FunctionAddress(ast->left);
        lefttype = FunctionPointerType(lefttype);
    }
    if (IsFunctionType(righttype) && !IsPointerType(righttype)) {
        ast->right = FunctionAddress(ast->right);
        righttype = FunctionPointerType(righttype);
    }
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
        if (IsPointerType(lefttype) && IsIntType(righttype)) {
            ast->right = ScalePointer(lefttype, forcepromote(righttype, ast->right));
            return lefttype;
        } else if (IsPointerType(righttype) && IsIntType(lefttype)) {
            ast->left = ScalePointer(righttype, forcepromote(lefttype, ast->left));
            return righttype;
        } else {
            return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
        }
    case '-':
        if (IsPointerType(lefttype) && IsPointerType(righttype)) {
            // we actually want to compute (a - b) / sizeof(*a)
            if (!CompatibleTypes(lefttype, righttype)) {
                ERROR(lefttype, "- applied to incompatible pointer types");
            } else {
                AST *diff;
                diff = AstOperator('-', ast->left, ast->right);
                diff = AstOperator(K_UNS_DIV, diff, AstInteger(TypeSize(BaseType(righttype))));
                *ast = *diff;
            }
            return ast_type_unsigned_long;
        }
        if (IsPointerType(lefttype) && IsIntType(righttype)) {
            ast->right = ScalePointer(lefttype, forcepromote(righttype, ast->right));
            return lefttype;
        } else if (IsPointerType(righttype) && IsIntType(lefttype)) {
            ast->left = ScalePointer(righttype, forcepromote(lefttype, ast->left));
            return righttype;
        } else {
            return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
        }
    case '*':
    case '/':
    case K_MODULUS:
        return HandleTwoNumerics(op, ast, lefttype, righttype);
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
    case K_NEGATE:
    case K_ABS:
    case K_SQRT:
        if (IsFloatType(rettype)) {
            if (!gl_fixedreal) {
                if (op == K_ABS) {
                    *ast = *MakeOperatorCall(float_abs, ast->right, NULL, NULL);
                } else if (op == K_SQRT) {
                    *ast = *MakeOperatorCall(float_sqrt, ast->right, NULL, NULL);
                } else {
                    if (IsConstExpr(ast->right)) {
                        int x = EvalConstExpr(ast->right);
                        *ast = *NewAST(AST_FLOAT, NULL, NULL);
                        ast->d.ival = x ^ 0x80000000U;
                    } else {
                        *ast = *MakeOperatorCall(float_neg, ast->right, NULL, NULL);
                    }
                }
                return rettype;
            }
            if (gl_fixedreal && op == K_SQRT) {
                *ast = *AstOperator(K_SHL, AstOperator(op, ast->left, ast->right), AstInteger(G_FIXPOINT/2));
            }
            return rettype;
        } else {
            const char *name;
            if (op == K_ABS) {
                name = "abs";
            } else if (op == K_SQRT) {
                name = "sqrt";
            } else {
                name = "negate";
            }
            if (!VerifyIntegerType(ast, rettype, name))
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
    case K_BOOL_NOT:
    case K_BOOL_AND:
    case K_BOOL_OR:
        if (IsFloatType(lefttype)) {
            ast->left = MakeOperatorCall(float_cmp, ast->left, AstInteger(0), NULL);
            lefttype = ast_type_long;
        }
        if (IsFloatType(righttype)) {
            ast->right = MakeOperatorCall(float_cmp, ast->right, AstInteger(0), NULL);
            righttype = ast_type_long;
        }
        if (lefttype && !IsBoolCompatibleType(lefttype)) {
            ERROR(ast, "Expression not compatible with boolean operation");
        } else if (righttype && !IsBoolCompatibleType(righttype)) {
            ERROR(ast, "Expression not compatible with boolean operation");
        }
        return ast_type_long;
    case K_INCREMENT:
    case K_DECREMENT:
        if (lefttype && IsPointerType(lefttype)) {
            return lefttype;
        }
        if (righttype && IsPointerType(righttype)) {
            return righttype;
        }
        /* fall through */
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
// returns the new type (should normally be desttype)
//
AST *CoerceAssignTypes(AST *line, int kind, AST **astptr, AST *desttype, AST *srctype)
{
    ASTReportInfo saveinfo;
    AST *expr = *astptr;
    const char *msg;
    int lang = curfunc ? curfunc->language : (current ? current->mainLanguage : LANG_C);
    
    if (kind == AST_RETURN) {
        msg = "return";
    } else if (kind == AST_FUNCCALL) {
        msg = "parameter passing";
    } else if (kind == AST_ARRAYREF) {
        msg = "array indexing";
    } else {
        msg = "assignment";
    }

    if (expr && expr->kind == AST_INTEGER && expr->d.ival == 0) {
        if (curfunc && curfunc->language == LANG_C) {
            if (IsPointerType(desttype)) {
                return desttype;
            }
        }
    }
    
    if (!desttype || !srctype) {
        return desttype;
    }
    AstReportAs(expr, &saveinfo);
    if (IsFloatType(desttype)) {
        if (IsIntType(srctype)) {
            *astptr = domakefloat(srctype, expr);
            srctype = ast_type_float;
        }
    }
    // allow floats to be cast as ints
    if (IsIntType(desttype) && IsFloatType(srctype)) {
        expr = dofloatToInt(expr);
        *astptr = expr;
        AstReportDone(&saveinfo);
        return desttype;
    }

    // automatically cast arrays to pointers if necessary
    if (IsArrayType(srctype) && IsPointerType(desttype)) {
        srctype = ArrayToPointerType(srctype);
        expr = ArrayAddress(expr);
        *astptr = expr;
    }
    // similarly for classes in some languages
    if (IsClassType(srctype) && IsPointerType(desttype) && (lang == LANG_BASIC || lang == LANG_CIRCUS))
      {
          srctype = ClassToPointerType(srctype);
          expr = StructAddress(expr);
          *astptr = expr;
      }
    
    if (IsFunctionType(srctype) && IsPointerType(desttype) && !IsPointerType(srctype)) {
        srctype = FunctionPointerType(srctype);
        expr = FunctionAddress(expr);
        *astptr = expr;
    }
    if (!CompatibleTypes(desttype, srctype)) {
        if (IsPointerType(desttype) && IsPointerType(srctype)) {
            WARNING(line, "incompatible pointer types in %s", msg);
        } else {
            ERROR(line, "incompatible types in %s", msg);
            return desttype;
        }
    }
    if (IsConstType(desttype) && kind == AST_ASSIGN) {
        WARNING(line, "assignment to const object");
    }
    if (IsPointerType(srctype) && IsConstType(BaseType(srctype)) && !IsConstType(BaseType(desttype))) {
        if (desttype != ast_type_const_generic) {
            WARNING(line, "%s discards const attribute from pointer", msg);
        }
    }
    if (IsIntType(desttype) || IsGenericType(desttype)) {
        if (IsIntType(srctype) || IsGenericType(srctype)) {
            int lsize = TypeSize(desttype);
            int rsize = TypeSize(srctype);
            if (lsize > rsize) {
                if (IsUnsignedType(srctype)) {
                    *astptr = dopromote(expr, rsize, lsize, K_ZEROEXTEND);
                } else {
                    *astptr = dopromote(expr, rsize, lsize, K_SIGNEXTEND);
                }
            } else if (rsize == 8 && lsize < rsize) {
                // narrowing cast
                *astptr = donarrow(expr, rsize, lsize, IsUnsignedType(srctype));
            }
        }
    }
    AstReportDone(&saveinfo);
    return desttype;
}

/* change AST so that it casts src to desttype */
static AST *
doCast(AST *desttype, AST *srctype, AST *src)
{
    AST *expr = src;
    const char *name;
    ASTReportInfo saveinfo;
    
    if (IsVoidType(desttype)) {
        // (void)x ignores x
        return src;
    }
    if (!srctype || IsGenericType(srctype)) {
        return src;
    }
    AstReportAs(src, &saveinfo);
    if (src && IsIdentifier(src)) {
        name = GetUserIdentifierName(src);
    } else {
        name = "expression";
    }
    if (IsArrayType(srctype)) {
        src = ArrayAddress(src);
        srctype = ast_type_ptr_void;
    } else if (IsFunctionType(srctype) && !IsPointerType(srctype)) {
        // need to create a handle for it
        src = FunctionAddress(src);
        srctype = FunctionPointerType(srctype);
    }
    if (IsPointerType(desttype) || IsGenericType(desttype)) {
        if (IsFloatType(srctype)) {
            src = dofloatToInt(src);
            srctype = ast_type_long;
        }
        if (IsArrayType(srctype)) {
            return ArrayAddress(src);
        }
        if (IsPointerType(srctype)) {
            return src;
        }
        if (IsIntType(srctype)) {
            /* FIXME: should probably check size here */
            return src;
        }
        if (srctype->kind == AST_FUNCTYPE) {
            return NewAST(AST_ADDROF, src, NULL);
        }
        ERROR(src, "unable to convert %s to a pointer type", name);
        AstReportDone(&saveinfo);
        return NULL;
    }
    if (IsFloatType(desttype)) {
        if (IsFloatType(srctype)) {
            AstReportDone(&saveinfo);
            return src;
        }
        if (IsPointerType(srctype)) {
            srctype = ast_type_long;
        }
        if (IsIntType(srctype)) {
            AST *r = domakefloat(srctype, src);
            AstReportDone(&saveinfo);
            return r;
        }
        ERROR(src, "unable to convert %s to a float type", name);
        AstReportDone(&saveinfo);
        return NULL;
    }
    if (IsIntType(desttype)) {
        if (IsFloatType(srctype)) {
            src = dofloatToInt(src);
            srctype = ast_type_long;
        }
        if (IsPointerType(srctype)) {
            srctype = ast_type_long;
        }
        if (IsIntType(srctype)) {
            int lsize = TypeSize(desttype);
            int rsize = TypeSize(srctype);
            if (lsize > rsize) {
                int finalsize = (lsize < LONG_SIZE) ? LONG_SIZE : lsize;
                if (IsUnsignedType(srctype)) {
                    src = dopromote(src, rsize, finalsize, K_ZEROEXTEND);
                } else {
                    src = dopromote(expr, rsize, finalsize, K_SIGNEXTEND);
                }
            } else if (lsize < rsize) {
                src = donarrow(src, rsize, lsize, IsUnsignedType(srctype));
            }
            AstReportDone(&saveinfo);
            return src;
        }
    }
    AstReportDone(&saveinfo);
    ERROR(src, "bad cast of %s", name);
    return NULL;
}

static AST *
BuildMethodPointer(AST *ast)
{
    Symbol *sym;
    AST *objast;
    AST *funcaddr;
    AST *result;
    Function *func;
    
    sym = FindCalledFuncSymbol(ast, &objast, 0);
    if (!sym || sym->kind != SYM_FUNCTION) {
        ERROR(ast, "Internal error, unable to find function address");
        return ast;
    }
    func = (Function *)sym->val;
    if (func->is_static) {
        objast = AstInteger(0);
    } else if (objast == NULL) {
        objast = NewAST(AST_SELF, NULL, NULL);
    } else {
        objast = NewAST(AST_ADDROF, objast, NULL);
    }
    func->used_as_ptr = 1;
    // save off the current @ node
    funcaddr = NewAST(AST_ADDROF, ast->left, ast->right);
    // create a call
    result = MakeOperatorCall(make_methodptr, objast, funcaddr, NULL);
    return result;
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

    if (ast->kind == AST_CAST) {
        AST *cast;
        ltype = ast->left;
        rtype = CheckTypes(ast->right);
        cast = doCast(ltype, rtype, ast->right);
        if (cast) {
            ast->right = cast;
        }
        return ltype;
    }        
    ltype = CheckTypes(ast->left);
    if (ast->kind != AST_METHODREF) {
        rtype = CheckTypes(ast->right);
    }
    switch (ast->kind) {
    case AST_GOSUB:
        /* FIXME: should check here for top level function */
    case AST_GOTO:
        {
            AST *id = ast->left;
            if (!id || !IsIdentifier(id)) {
                ERROR(ast, "Expected identifier in goto/gosub");
            } else {
                Symbol *sym = FindSymbol(&curfunc->localsyms, GetIdentifierName(id));
                if (!sym || sym->kind != SYM_LOCALLABEL) {
                    ERROR(id, "%s is not a local label", GetUserIdentifierName(id));
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
        if (ltype && IsClassType(ltype)) {
            int siz = TypeSize(ltype);
            if (siz > 8) {
                // convert the assignment to a memcpy
                AST *lptr = StructAddress(ast->left);
                AST *rptr = StructAddress(ast->right);
                AST *copy = MakeOperatorCall(struct_copy, lptr, rptr, AstInteger(siz));
                *ast = *NewAST(AST_MEMREF, NULL, copy);
//                WARNING(ast, "Need to convert to memcpy");
            }
        }
        break;
    case AST_RETURN:
        if (ast->left) {
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
            AST *functype;

            functype = RemoveTypeModifiers(ExprType(ast->left));
            if (functype && functype->kind == AST_PTRTYPE) {
                functype = RemoveTypeModifiers(functype->left);
            }
            if (IsFunctionType(functype)) {
                calledParamList = functype->right;
                while (actualParamList) {
                    AST *paramId = calledParamList ? calledParamList->left : NULL;
                    AST *actualParam = actualParamList->left;
                    
                    expectType = NULL;
                    passedType = NULL;
                    if (!passedType) {
                        passedType = ExprType(actualParam);
                    }
                    if (paramId) {
                        // if the parameter has a type declaration, use it
                        if (paramId->kind == AST_DECLARE_VAR) {
                            expectType = ExprType(paramId);
                        }
                    }
                    if (!expectType) {
                        // we use const generic to avoid lots of warning
                        // messages about passing strings to printf
                        expectType = ast_type_const_generic;
                    }
                    CoerceAssignTypes(ast, AST_FUNCCALL, &actualParamList->left, expectType, passedType);
                    if (calledParamList) {
                        calledParamList = calledParamList->right;
                    }
                    actualParamList = actualParamList->right;
                }
                ltype = functype->left;
            } else {
                return NULL;
            }
        }
        break;
    case AST_RESULT:
        return GetFunctionReturnType(curfunc);
    case AST_FLOAT:
    case AST_TRUNC:
    case AST_ROUND:
        return ast_type_float;
    case AST_INTEGER:
        if (ast->d.ival == 0) {
            // let "0" stand for any NULL object
            return ast_type_generic;
        }
        if (ast->left) {
            return ast->left;
        }
        return  ast_type_long;
    case AST_ISBETWEEN:
    case AST_HWREG:
    case AST_CONSTREF:
        return ast_type_long;
    case AST_SIZEOF:
        return ast_type_unsigned_long;
    case AST_CATCHRESULT:
    case AST_BITVALUE:
        return ast_type_generic;
    case AST_SETJMP:
        return ast_type_long;
    case AST_STRING:
    case AST_STRINGPTR:
        if (curfunc && curfunc->language == LANG_BASIC) {
            return ast_type_string;
        }
        return ast_type_ptr_byte;
    case AST_ADDROF:
    case AST_ABSADDROF:
        if (IsFunctionType(ltype)) {
            *ast = *BuildMethodPointer(ast);
            return ltype;
        }
        return NewAST(AST_PTRTYPE, ltype, NULL);
    case AST_ARRAYREF:
        {
            AST *lefttype = ltype;
            AST *righttype;
            AST *basetype;

            righttype = ExprType(ast->right);
            if (IsFloatType(righttype)) {
                righttype = CoerceAssignTypes(ast, AST_ARRAYREF, &ast->right, ast_type_long, righttype);
            }
            if (!lefttype) {
                lefttype = ExprType(ast->left);
            }
            if (!lefttype) {
                return NULL;
            }
            basetype = BaseType(lefttype);
            if (IsPointerType(lefttype)) {
                // force this to have a memory dereference
                // and in BASIC, also force the appropriate number for the base
                AST *deref;
                if (curfunc && curfunc->language == LANG_BASIC) {
                    extern Symbol *GetCurArrayBase();
                    Symbol *sym = GetCurArrayBase();
                    if (sym && sym->kind == SYM_CONSTANT) {
                        ast->right = AstOperator('-', ast->right, (AST *)sym->val);
                    }
                }
                deref = NewAST(AST_MEMREF, basetype, ast->left);
                ast->left = deref;
            } else if (ast->left->kind == AST_MEMREF) {
                // the base type may be encoded here
                if (ast->left->left) {
                    basetype = ast->left->left;
                }
            } else if (IsArrayType(lefttype)) {
                // convert the array index to subtract base
                AST *base = GetArrayBase(lefttype);
                if (base) {
                    ast->right = AstOperator('-', ast->right, base);
                }
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
            *ast = *MakeOperatorCall(gc_alloc_managed, sizeExpr, NULL, NULL);
        }
        break;
    case AST_DELETE:
        *ast = *MakeOperatorCall(gc_free, ast->left, NULL, NULL);
        ltype = ast_type_void;
        break;
    case AST_CONDRESULT:
    {
        //AST *cond = ast->left; // not needed here
        AST *outputs = ast->right;
        if (!outputs) return NULL;
        ltype = ExprType(outputs->left);
        if (IsArrayType(ltype)) {
            outputs->left = ArrayAddress(outputs->left);
            ltype = ArrayToPointerType(ltype);
        }
        rtype = ExprType(outputs->right);
        if (IsArrayType(rtype)) {
            outputs->right = ArrayAddress(outputs->right);
            rtype = ArrayToPointerType(rtype);
        }
        if (IsGenericType(ltype)) {
            ltype = rtype;
        }
        if (!CompatibleTypes(ltype, rtype)) {
            WARNING(ast, "different types in arms of ?");
        }
        return ltype;
    }
    case AST_ALLOCA:
    {
        return ast->left ? ast->left : ast_type_ptr_void;
    }
    case AST_METHODREF:
        if (ltype && !IsClassType(ltype)) {
            ERROR(ast, "Method reference on non-class %s", GetIdentifierName(ast->left));
            return ltype;
        }
        return ExprType(ast);
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
    case AST_SYMBOL:
        // add super class lookups if necessary
        {
            Module *P;
            AST *supers = NULL;
            static AST *superref = NULL;
            Symbol *sym = LookupAstSymbol(ast, NULL);
            if (!sym) {
                return NULL;
            }
            ltype = ExprType(ast);
            // if this is a REFTYPE then dereference it
            if (ltype && IsRefType(ltype)) {
                AST *deref;
                AST *basetype = BaseType(ltype);
                deref = DupAST(ast);
                deref = NewAST(AST_MEMREF, basetype, deref);
                deref = NewAST(AST_ARRAYREF, deref, AstInteger(0));
                *ast = *deref;
                ltype = basetype;
            }
            if (sym->kind == SYM_FUNCTION) {
                Function *f = (Function *)sym->val;
                if (f->module == current || f->module == globalModule) {
                    return ltype;
                }
            }
            if (sym->kind == SYM_VARIABLE || sym->kind == SYM_FUNCTION) {
                const char *name = sym->our_name;
                P = current;
                while (P) {
                    sym = FindSymbol(&P->objsyms, name);
                    if (sym) {
                        break;
                    }
                    if (!superref) {
                        superref = AstIdentifier("__super");
                    }
                    if (supers) {
                        supers = NewAST(AST_METHODREF, supers, superref);
                    } else {
                        supers = superref;
                    }
                    supers = NewAST(AST_ARRAYREF,
                                    NewAST(AST_MEMREF,
                                           ClassType(P->superclass),
                                           supers),
                                    AstInteger(0));
                    P = P->superclass;
                }
                if (sym && supers) {
                    *ast = *NewAST(AST_METHODREF, supers, DupAST(ast));
                }
            }
            return ltype;
        }
    case AST_EXPRLIST:
    case AST_SEQUENCE:
    case AST_CONSTANT:
    case AST_VA_ARG:
        return ExprType(ast);
    case AST_SIMPLEFUNCPTR:
        return ast_type_generic;
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
InitGlobalFuncs(void)
{
    if (!basic_print_integer) {
        if (gl_fixedreal) {
            basic_print_float = getBasicPrimitive("_basic_print_fixed");
            float_mul = getBasicPrimitive("_fixed_mul");
            float_div = getBasicPrimitive("_fixed_div");
        } else {
            basic_print_float = getBasicPrimitive("_basic_print_float");
            basic_get_float = getBasicPrimitive("_basic_get_float");
            float_cmp = getBasicPrimitive("_float_cmp");
            float_add = getBasicPrimitive("_float_add");
            float_sub = getBasicPrimitive("_float_sub");
            float_mul = getBasicPrimitive("_float_mul");
            float_div = getBasicPrimitive("_float_div");
            float_fromuns = getBasicPrimitive("_float_fromuns");
            float_fromint = getBasicPrimitive("_float_fromint");
            float_toint = getBasicPrimitive("_float_trunc");
            float_abs = getBasicPrimitive("_float_abs");
            float_sqrt = getBasicPrimitive("_float_sqrt");
            float_neg = getBasicPrimitive("_float_negate");
        }
        basic_get_integer = getBasicPrimitive("_basic_get_integer");
        basic_get_string = getBasicPrimitive("_basic_get_string");
        basic_read_line = getBasicPrimitive("_basic_read_line");
        
        basic_print_integer = getBasicPrimitive("_basic_print_integer");
        basic_print_unsigned = getBasicPrimitive("_basic_print_unsigned");
        basic_print_string = getBasicPrimitive("_basic_print_string");
        basic_print_char = getBasicPrimitive("_basic_print_char");
        basic_print_nl = getBasicPrimitive("_basic_print_nl");
        basic_put = getBasicPrimitive("_basic_put");

        if (gl_p2) {
            struct_copy = getBasicPrimitive("longmove");
        } else {
            struct_copy = getBasicPrimitive("bytemove");
        }
        string_cmp = getBasicPrimitive("_string_cmp");
        string_concat = getBasicPrimitive("_string_concat");
        make_methodptr = getBasicPrimitive("_make_methodptr");
        gc_alloc_managed = getBasicPrimitive("_gc_alloc_managed");
        gc_free = getBasicPrimitive("_gc_free");

        funcptr_cmp = getBasicPrimitive("_funcptr_cmp");
    }
}

void FixupParameters(Function *func)
{
}

