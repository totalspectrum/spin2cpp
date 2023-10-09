/*
 * Spin to C/C++ translator
 * Copyright 2011-2023 Total Spectrum Software Inc.
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
#include "becommon.h"

AST *basic_get_float;
AST *basic_get_string;
AST *basic_get_integer;
AST *basic_read_line;

AST *basic_print_float;
AST *basic_print_string;
AST *basic_print_integer;
AST *basic_print_integer_2;
AST *basic_print_integer_3;
AST *basic_print_integer_4;
AST *basic_print_unsigned;
AST *basic_print_unsigned_2;
AST *basic_print_unsigned_3;
AST *basic_print_unsigned_4;
AST *basic_print_longinteger;
AST *basic_print_longunsigned;
AST *basic_print_char;
AST *basic_print_nl;
AST *basic_put;

AST *basic_lock_io;
AST *basic_unlock_io;

AST *varargs_ident;

static AST *float_add;
static AST *float_sub;
static AST *float_mul;
static AST *float_div;
static AST *float_cmp;
static AST *float_fromuns;
static AST *float_fromint;
static AST *float_fromuns64;
static AST *float_fromint64;
static AST *float_toint;
static AST *float_todouble;
static AST *float_abs;
static AST *float_sqrt;
static AST *float_neg;
static AST *float_pow_n;
static AST *float_powf;

static AST *double_add;
static AST *double_sub;
static AST *double_mul;
static AST *double_div;
static AST *double_cmp;
static AST *double_fromuns;
static AST *double_fromint;
static AST *double_fromuns64;
static AST *double_fromint64;
static AST *double_toint;
static AST *double_abs;
static AST *double_sqrt;
static AST *double_neg;
static AST *double_powf;

static AST *int64_add;
static AST *int64_sub;
static AST *int64_muls;
static AST *int64_mulu;
static AST *int64_divs, *int64_divu;
static AST *int64_mods, *int64_modu;
static AST *int64_neg, *int64_abs;
static AST *int64_cmpu, *int64_cmps;
static AST *int64_shl, *int64_shr, *int64_sar;
static AST *int64_and, *int64_or, *int64_xor;
static AST *int64_signx, *int64_zerox;

static AST *struct_copy;
static AST *struct_memset;
static AST *string_cmp;
static AST *string_concat;
static AST *string_length;

static AST *string_tointeger;
static AST *string_frominteger;

static AST *gc_alloc_managed;
static AST *gc_free;

static AST *funcptr_cmp;
static AST *make_interfaceptrs;

static AST * getBasicPrimitive(const char *name);

// check that "typ" is an integer type
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

/*
 * check the current boolean type
 */
AST *
CurBoolType()
{
    int language = GetCurrentLang();
    if (IsCLang(language)) {
        return ast_type_c_boolean;
    }
    return ast_type_basic_boolean;
}

/*
 * provide a result for unordered comparisons
 * like NaN = NaN
 */
static int
UnorderedResult(int op)
{
    switch (op) {
    case '>':
    case K_GE:
        return -1;
    case '<':
    case K_LE:
    case K_EQ:
    case K_NE:
    default:
        return 1;
    }
}

// create a call to function func with parameters ast->left, ast->right
// there is an optional 3rd argument too
static AST *
MakeOperatorCall(AST *func, AST *left, AST *right, AST *extraArg)
{
    AST *call;
    AST *params = NULL;
    ASTReportInfo saveinfo;

    if (!func) {
        ERROR(left, "Internal error, NULL parameter");
        return AstInteger(0);
    }
    if (left) {
        AstReportAs(left, &saveinfo);
    } else if (right) {
        AstReportAs(right, &saveinfo);
    } else {
        AstReportAs(func, &saveinfo);
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

    // do some optimizations on constant values
    if (func == string_concat && left && right && left->kind == AST_STRINGPTR && right->kind == AST_STRINGPTR)
    {
        call = AstMergeStrings(left, right);
    } else {
        call = NewAST(AST_FUNCCALL, func, params);
    }
    AstReportDone(&saveinfo);
    return call;
}

// do a promotion when we already know the size of the original type
static AST *dopromote(AST *expr, int srcbytes, int destbytes, int operatr)
{
    int shiftbits = srcbytes * 8;
    AST *promote;
    ASTReportInfo saveinfo;

    if (shiftbits == 32 && destbytes < 8) {
        return expr; // nothing to do
    }
    AstReportAs(expr, &saveinfo);
    promote = AstOperator(operatr, expr, AstInteger(shiftbits));
    if (destbytes == 8) {
        if ( IsConstExpr(promote)) {
            AST *typ = ExprType(expr);
            AST *val;
            int64_t result = EvalConstExpr(expr);
            if (IsUnsignedType(typ)) {
                typ = ast_type_unsigned_long64;
            } else {
                typ = ast_type_long64;
            }
            val = AstInteger(result);
            val->left = typ;
#if 1
            promote = NewAST(AST_EXPRLIST,
                             NewAST(AST_GETLOW, val, NULL),
                             NewAST(AST_EXPRLIST,
                                    NewAST(AST_GETHIGH, val, NULL),
                                    NULL));
#else
            promote = val;
#endif
        } else {
            // at this point "promote" will contain a 4 byte value
            // now we need to convert it to an 8 byte value
            AST *convfunc;

            if (operatr == K_ZEROEXTEND) {
                convfunc = int64_zerox;
            } else {
                convfunc = int64_signx;
            }
            promote = MakeOperatorCall(convfunc, promote, NULL, NULL);
        }
    }
    AstReportDone(&saveinfo);
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
        if (expr->kind == AST_EXPRLIST) {
            expr = expr->left;
        } else {
            expr = NewAST(AST_GETLOW, expr, NULL);
        }
        A = 4;
    }
    shiftbits = (A - B) * 8;
    if (shiftbits == 0) {
        return expr; // nothing to do
    }
    promote = dopromote(expr, A, LONG_SIZE, isSigned ? K_ZEROEXTEND : K_SIGNEXTEND);
#if 0
    if (shiftbits > 0) {
        int operatr = isSigned ? K_SAR : K_SHR;
        narrow = AstOperator(K_SHL, promote, AstInteger(shiftbits));
        narrow = AstOperator(operatr, narrow, AstInteger(shiftbits));
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
    if (tsize < LONG_SIZE) {
        return dopromote(expr, tsize, LONG_SIZE, op);
    }
    return expr;
}

// force a promotion from a small integer type to a full 64 bits
static AST *forcepromote64(AST *type, AST *expr)
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
    if (tsize < LONG64_SIZE) {
        return dopromote(expr, tsize, LONG64_SIZE, op);
    }
    return expr;
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
    int finalsize;
    AST *ulong_type, *long_type;

    force = force || (lsize != rsize);
    if (lsize > LONG_SIZE || rsize > LONG_SIZE) {
        finalsize = LONG64_SIZE;
        ulong_type = ast_type_unsigned_long64;
        long_type = ast_type_long64;
    } else {
        finalsize = LONG_SIZE;
        ulong_type = ast_type_unsigned_long;
        long_type = ast_type_long;
    }
    if (lsize < finalsize && force) {
        if (leftunsigned) {
            ast->left = dopromote(ast->left, lsize, finalsize, K_ZEROEXTEND);
            lefttype = ulong_type;
        } else {
            ast->left = dopromote(ast->left, lsize, finalsize, K_SIGNEXTEND);
            lefttype = long_type;
        }
        rettype = righttype;
    }
    if (rsize < finalsize && force) {
        if (rightunsigned) {
            ast->right = dopromote(ast->right, rsize, finalsize, K_ZEROEXTEND);
            righttype = ulong_type;
        } else {
            ast->right = dopromote(ast->right, rsize, finalsize, K_SIGNEXTEND);
            righttype = long_type;
        }
        rettype = lefttype;
    }
    if (leftunsigned || rightunsigned) {
        return rettype;
    } else {
        return long_type;
    }
}

static AST *
domakedouble(AST *typ, AST *ast)
{
    AST *ret;
    if (!ast) return ast;
    if (IsGenericType(typ)) return ast;
    if (gl_fixedreal) {
        ret = AstOperator(K_SHL, ast, AstInteger(G_FIXPOINT));
        return FoldIfConst(ret);
    }
    if (!IsIntOrGenericType(typ)) {
        ERROR(ast, "Unable to cast this type to float");
        return ast;
    }
    ast = forcepromote(typ, ast);
    int siz = TypeSize(typ);
    if (IsUnsignedType(typ)) {
        ret = MakeOperatorCall( siz == 8 ? double_fromuns64 : double_fromuns, ast, NULL, NULL);
    } else {
        ret = MakeOperatorCall( siz == 8 ? double_fromint64 : double_fromint, ast, NULL, NULL);
    }
    return ret;
}

static AST *
domakefloat(AST *typ, AST *ast)
{
    AST *ret;
    if (!ast) return ast;
    if (IsGenericType(typ)) return ast;
    if (gl_fixedreal) {
        ret = AstOperator(K_SHL, ast, AstInteger(G_FIXPOINT));
        return FoldIfConst(ret);
    }
    if (!IsIntOrGenericType(typ)) {
        ERROR(ast, "Unable to cast this type to float");
        return ast;
    }
    ast = forcepromote(typ, ast);
    if (IsConstExpr(ast)) {
        // FIXME: assumes 32 bit floats only
        int64_t x = EvalConstExpr(ast);
        float f;
        f = (IsUnsignedType(typ)) ? (float)(uint64_t)x : (float) x;
        return AstFloat(f);
    }
    int siz = TypeSize(typ);
    if (IsUnsignedType(typ)) {
        ret = MakeOperatorCall( (siz == 8) ? float_fromuns64 : float_fromuns, ast, NULL, NULL);
    } else {
        ret = MakeOperatorCall( (siz == 8) ? float_fromint64 : float_fromint, ast, NULL, NULL);
    }
    return ret;
}

static AST *
dofloatToInt(AST *ast, AST *srctyp, AST *dsttyp)
{
    AST *ret;

    if (IsBoolType(dsttyp)) {
        ret = AstOperator(K_NE, NULL, NULL);
        if (gl_fixedreal) {
            ret->left = ast;
        } else {
            ret->left = MakeOperatorCall(float_cmp, ast, AstInteger(0), AstInteger(UnorderedResult(K_NE)));
        }
        ret->right = AstInteger(0);
    } else if (gl_fixedreal) {
        // FIXME: should we round here??
        ret = AstOperator(K_SAR, ast, AstInteger(G_FIXPOINT));
    } else if (IsFloat64Type(srctyp)) {
        ret = MakeOperatorCall(double_toint, ast, NULL, NULL);
    } else if (IsConstExpr(ast)) {
        union f_or_i {
            float floatbits;
            int intbits;
        } g;
        g.intbits = EvalConstExpr(ast);
        ret = AstInteger((int)g.floatbits);
    } else {
        ret = MakeOperatorCall(float_toint, ast, NULL, NULL);
    }
    return ret;
}

static AST *
dofloatToDouble(AST *ast, AST *typ)
{
    AST *ret;

    if (gl_fixedreal) {
        // FIXME: should we round here??
        ret = AstOperator(K_SAR, ast, AstInteger(G_FIXPOINT));
        return ret;
    }
    if (IsFloat64Type(typ)) {
        return ast;
    }
    ast = MakeOperatorCall(float_todouble, ast, NULL, NULL);
    return ast;
}

bool MakeBothIntegers(AST *ast, AST *ltyp, AST *rtyp, const char *opname)
{
    if (IsFloatType(ltyp)) {
        ast->left = dofloatToInt(ast->left, ltyp, ast_type_long);
        ltyp = ast_type_long;
    }
    if (IsFloatType(rtyp)) {
        ast->right = dofloatToInt(ast->right, rtyp, ast_type_long);
        rtyp = ast_type_long;
    }
    return VerifyIntegerType(ast, ltyp, opname) && VerifyIntegerType(ast, rtyp, opname);
}

#define DEFAULT_FLOAT_TYPE ast_type_float

static AST *
HandleTwoNumerics(int op, AST *ast, AST *lefttype, AST *righttype)
{
    int isfloat = 0;
    int isfloat64 = 0;
    int isalreadyfixed = 0;
    AST *scale = NULL;
    ASTReportInfo saveinfo;

    AstReportAs(ast, &saveinfo);
    if (op == K_MODULUS) {
        // MOD operator converts float operands to integer
        if (IsFloatType(lefttype)) {
            ast->left = dofloatToInt(ast->left, lefttype, ast_type_long);
            lefttype = ast_type_long;
        }
        if (IsFloatType(righttype)) {
            ast->right = dofloatToInt(ast->right, righttype, ast_type_long);
            righttype = ast_type_long;
        }
    }
    if (IsFloatType(lefttype)) {
        isfloat = 1;
        isfloat64 = IsFloat64Type(lefttype);
        if (!IsFloatType(righttype)) {
            if (gl_fixedreal && (op == '*' || op == '/')) {
                // no need for fixed point mul, just do regular mul
                isalreadyfixed = 1;
                if (op == '/') {
                    // fixed / int requires no scale
                    scale = AstInteger(0);
                }
                righttype = DEFAULT_FLOAT_TYPE;
            } else {
                if (isfloat64) {
                    ast->right = domakedouble(righttype, ast->right);
                } else {
                    ast->right = domakefloat(righttype, ast->right);
                }
                righttype = ExprType(ast->right);
            }
        } else if (isfloat64) {
            // promote left to float64
            ast->left = dofloatToDouble(ast->left, lefttype);
            lefttype = ast_type_float64;
            isfloat64 = 1;
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
            lefttype = DEFAULT_FLOAT_TYPE;
        } else {
            if (IsFloat64Type(righttype)) {
                isfloat64 = 1;
                ast->left = domakedouble(lefttype, ast->left);
            } else {
                ast->left = domakefloat(lefttype, ast->left);
            }
            lefttype = ExprType(ast->left);
        }
    } else {
        // for exponentiation both sides need to be floats
        if (op == K_POWER) {
            isfloat = 1;
            ast->left = domakefloat(lefttype, ast->left);
            ast->right = domakefloat(righttype, ast->right);
            lefttype = ExprType(ast->left);
            righttype = ExprType(ast->right);
        }
        // in C we need to promote both sides to  long
        else if (curfunc && IsCLang(curfunc->language)) {
            int operatr;
            if (lefttype) {
                int leftsize = TypeSize(lefttype);
                if (leftsize < 4) {
                    operatr = IsUnsignedType(lefttype) ? K_ZEROEXTEND : K_SIGNEXTEND;
                    ast->left = dopromote(ast->left, leftsize, LONG_SIZE, operatr);
                    lefttype = ast_type_long;
                }
            }
            if (righttype) {
                int rightsize = TypeSize(righttype);
                if (rightsize < 4) {
                    operatr = IsUnsignedType(righttype) ? K_ZEROEXTEND : K_SIGNEXTEND;
                    ast->right = dopromote(ast->right, rightsize, LONG_SIZE, operatr);
                    righttype = ast_type_long;
                }
            }
        }
    }

    if (IsConstExpr(ast)) {
        AST *finaltype = NULL;
        if (lefttype == righttype) {
            finaltype = lefttype;
        } else if (IsIntType(lefttype) && IsIntType(righttype)) {
            int lsiz  = TypeSize(lefttype);
            int rsiz = TypeSize(righttype);
            bool isUnsigned = IsUnsignedType(lefttype) || IsUnsignedType(righttype);
            if (lsiz > LONG_SIZE || rsiz > LONG_SIZE) {
                finaltype = isUnsigned ? ast_type_unsigned_long64 : ast_type_long64;
            } else {
                finaltype = isUnsigned ? ast_type_unsigned_long : ast_type_long;
            }
        }
        if (finaltype) {
            AST *newast = FoldIfConst(ast);
            *ast = *newast;
            if (IsFloatType(finaltype)) {
                ast->kind = AST_FLOAT;
            }
            finaltype = ExprType(ast);
            AstReportDone(&saveinfo);
            return finaltype;
        }
    }
    if (isfloat) {
        switch (op) {
        case '+':
            if (!gl_fixedreal) {
                *ast = *MakeOperatorCall( isfloat64 ? double_add : float_add, ast->left, ast->right, NULL);
            }
            break;
        case '-':
            if (!gl_fixedreal) {
                *ast = *MakeOperatorCall( isfloat64 ? double_sub : float_sub, ast->left, ast->right, NULL);
            }
            break;
        case '*':
            if (!isalreadyfixed) {
                *ast = *MakeOperatorCall( isfloat64 ? double_mul : float_mul, ast->left, ast->right, NULL);
            }
            break;
        case '/':
            if (gl_fixedreal) {
                if (!isalreadyfixed) {
                    scale = AstInteger(G_FIXPOINT);
                }
            }
            *ast = *MakeOperatorCall( isfloat64 ? double_div : float_div, ast->left, ast->right, scale);
            break;
        case K_POWER:
            *ast = *MakeOperatorCall( isfloat64 ? double_powf : float_powf, ast->left, ast->right, NULL);
            break;
        default:
            ERROR(ast, "internal error unhandled operator");
            break;
        }
        AstReportDone(&saveinfo);
        return ast_type_float;
    }
    if (!MakeBothIntegers(ast, lefttype, righttype, "operator")) {
        AstReportDone(&saveinfo);
        return NULL;
    }
    lefttype = MatchIntegerTypes(ast, lefttype, righttype, 0);
    if (IsUnsignedType(lefttype)) {
        if (op == K_MODULUS) {
            ast->d.ival = op = K_UNS_MOD;
        } else if (op == '/') {
            ast->d.ival = op = K_UNS_DIV;
        }
    }
    if (IsInt64Type(lefttype) && !IsConstExpr(ast)) {
        switch(op) {
        case '+':
            *ast = *MakeOperatorCall(int64_add, ast->left, ast->right, NULL);
            break;
        case '-':
            *ast = *MakeOperatorCall(int64_sub, ast->left, ast->right, NULL);
            break;
        case '*':
            *ast = *MakeOperatorCall(int64_muls, ast->left, ast->right, NULL);
            break;
        case '/':
            *ast = *MakeOperatorCall(int64_divs, ast->left, ast->right, NULL);
            break;
        case K_UNS_DIV:
            *ast = *MakeOperatorCall(int64_divu, ast->left, ast->right, NULL);
            break;
        case K_MODULUS:
            *ast = *MakeOperatorCall(int64_mods, ast->left, ast->right, NULL);
            break;
        case K_UNS_MOD:
            *ast = *MakeOperatorCall(int64_modu, ast->left, ast->right, NULL);
            break;
        case '&':
            *ast = *MakeOperatorCall(int64_and, ast->left, ast->right, NULL);
            break;
        case '|':
            *ast = *MakeOperatorCall(int64_or, ast->left, ast->right, NULL);
            break;
        case '^':
            *ast = *MakeOperatorCall(int64_xor, ast->left, ast->right, NULL);
            break;
        case K_SAR:
            *ast = *MakeOperatorCall(int64_sar, ast->left, ast->right, NULL);
            break;
        case K_SHR:
        {
            if (IsConstExpr(ast->right) && EvalConstExpr(ast->right) == 32) {
                AST *high = AstInteger(0);
                AST *low = NewAST(AST_GETHIGH, ast->left, NULL);
                AST *val = NewAST(AST_EXPRLIST, low, NewAST(AST_EXPRLIST, high, NULL));
                *ast = *val;
            } else {
                *ast = *MakeOperatorCall(int64_shr, ast->left, ast->right, NULL);
            }
            break;
        }
        case K_SHL:
        {
            if (IsConstExpr(ast->right) && EvalConstExpr(ast->right) == 32) {
                AST *low = AstInteger(0);
                AST *high = NewAST(AST_GETLOW, ast->left, NULL);
                AST *val = NewAST(AST_EXPRLIST, low, NewAST(AST_EXPRLIST, high, NULL));
                *ast = *val;
            } else {
                *ast = *MakeOperatorCall(int64_shl, ast->left, ast->right, NULL);
            }
            break;
        }
        default:
            ERROR(ast, "Compiler is incomplete: unable to handle this 64 bit expression");
            break;
        }
    }
    AstReportDone(&saveinfo);
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
    int isint64 = 0;

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
            ast->left = MakeOperatorCall(float_cmp, ast->left, ast->right, AstInteger(UnorderedResult(op)));
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
        isint64 = IsInt64Type(lefttype) || IsInt64Type(righttype);
        if (isint64) {
            ast->left = forcepromote64(lefttype, ast->left);
            ast->right = forcepromote64(righttype, ast->right);
        } else {
            ast->left = forcepromote(lefttype, ast->left);
            ast->right = forcepromote(righttype, ast->right);
        }
        leftUnsigned = IsUnsignedType(lefttype);
        rightUnsigned = IsUnsignedType(righttype);
    }

    //
    // handle unsigned/signed comparisons here
    //

    if (isint64) {
        if (leftUnsigned || rightUnsigned) {
            ast->left = MakeOperatorCall(int64_cmpu, ast->left, ast->right, NULL);
        } else {
            ast->left = MakeOperatorCall(int64_cmps, ast->left, ast->right, NULL);
        }
        ast->right = AstInteger(0);
    }
    else if (leftUnsigned || rightUnsigned) {
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
            if (lsize == 4 && rsize == 4 && op != K_EQ && op != K_NE) {
                if (!IsBoolType(lefttype) && !IsBoolType(righttype)) {
                    WARNING(ast, "signed/unsigned comparison may not work properly");
                }
            }
        }
    }

    if (!CompatibleTypes(lefttype, righttype)) {
        WARNING(ast, "incompatible types in comparison");
    }
}

static AST *ScalePointer(AST *type, AST *val)
{
    int size;
    if (!IsPointerType(type)) {
        ERROR(val, "Internal error, expected pointer type");
        return val;
    }
    size = TypeSize(BaseType(type));
    val = AstOperator('*', val, AstInteger(size));
    return val;
}

// return the address of an array
AST *ArrayAddress(AST *expr)
{
    if (curfunc && IsLocalVariable(expr)) {
        curfunc->local_address_taken = 1;
    }
    return NewAST(AST_ABSADDROF,
                  NewAST(AST_ARRAYREF, expr, AstInteger(0)),
                  NULL);
}

AST *StructAddress(AST *expr)
{
    if (expr->kind == AST_MEMREF) {
        return expr->right;
    }
    if (expr->kind == AST_FUNCCALL) {
        return expr;
    }
    if (expr->kind == AST_INTEGER) {
        ERROR(expr, "taking address of integer");
    }
    return NewAST(AST_ABSADDROF, expr, NULL);
}

// return the address of a function
AST *FunctionAddress(AST *expr)
{
    if (expr && expr->kind == AST_METHODREF) {
        if (IsSymbol(expr->right)) {
            expr = NewAST(AST_ABSADDROF, expr, NULL);
            expr = BuildMethodPointer(expr);
            return expr;
        }
    }
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

AST *CoerceOperatorTypes(AST *ast, AST *lefttype, AST *righttype)
{
    AST *rettype = lefttype;
    int op;
    int isfloat64 = 0;

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
    case '&':
        if (IsBasicLang(GetCurrentLang()) && (IsStringType(lefttype) || IsStringType(righttype))) {
            AST *lhs = ast->left;
            AST *rhs = ast->right;
            if (IsStringType(lefttype)) {
                /* convert rhs to a string */
                if (IsStringType(righttype)) {
                    /* nothing to do */
                } else if (IsIntType(righttype)) {
                    /* convert to string */
                    rhs = MakeOperatorCall(string_frominteger, rhs, NULL, NULL);
                } else {
                    ERROR(rhs, "Unable to convert right side of + to a string");
                    rhs = lhs;
                }                
                *ast = *MakeOperatorCall(string_concat, lhs, rhs, NULL);
            } else if (IsIntType(lefttype)) {
                // x + "0" case
                rhs = MakeOperatorCall(string_tointeger, rhs, NULL, NULL);
                ast->right = rhs;
            }
            return lefttype;
        }
        /* fall through */
    case '|':
    case '^':
    case K_SAR:
    case K_SHL:
        if (lefttype && IsFloatType(lefttype)) {
            ast->left = dofloatToInt(ast->left, lefttype, ast_type_long);
            lefttype = ExprType(ast->left);
        }
        if (righttype && IsFloatType(righttype)) {
            ast->right = dofloatToInt(ast->right, righttype, ast_type_long);
            righttype = ExprType(ast->right);
        }
        if (ast->d.ival == K_SAR && lefttype && IsUnsignedType(lefttype)) {
            ast->d.ival = K_SHR;
        }
        return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
    case '+':
        if (IsBasicLang(GetCurrentLang()) && (IsStringType(lefttype) || IsStringType(righttype))) {
            AST *lhs = ast->left;
            AST *rhs = ast->right;
            if (IsStringType(lefttype)) {
                /* convert rhs to a string */
                if (IsStringType(righttype)) {
                    /* nothing to do */
                } else if (IsIntType(righttype)) {
                    /* convert to string */
                    rhs = MakeOperatorCall(string_frominteger, rhs, NULL, NULL);
                } else {
                    ERROR(rhs, "Unable to convert right side of + to a string");
                    rhs = lhs;
                }                
                *ast = *MakeOperatorCall(string_concat, lhs, rhs, NULL);
            } else if (IsIntType(lefttype)) {
                // x + "0" case
                rhs = MakeOperatorCall(string_tointeger, rhs, NULL, NULL);
                ast->right = rhs;
            }
            return lefttype;
        }
        if (IsPointerType(lefttype) && IsIntOrGenericType(righttype)) {
            ast->right = ScalePointer(lefttype, forcepromote(righttype, ast->right));
            return lefttype;
        } else if (IsPointerType(righttype) && IsIntOrGenericType(lefttype)) {
            ast->left = ScalePointer(righttype, forcepromote(lefttype, ast->left));
            return righttype;
        } else {
            return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
        }
    case '-':
        if (IsStringType(lefttype) || IsStringType(righttype)) {
            ERROR(ast, "- cannot be applied to strings");
            return ast_type_generic;
        }
        if (IsPointerType(lefttype) && IsPointerType(righttype)) {
            // we actually want to compute (a - b) / sizeof(*a)
            if (!CompatibleTypes(lefttype, righttype)) {
                ERROR(ast, "- applied to incompatible pointer types");
            } else {
                AST *diff;
                diff = AstOperator('-', ast->left, ast->right);
                diff = AstOperator(K_UNS_DIV, diff, AstInteger(TypeSize(BaseType(righttype))));
                *ast = *diff;
            }
            return ast_type_unsigned_long;
        }
        if (IsPointerType(lefttype) && IsIntOrGenericType(righttype)) {
            ast->right = ScalePointer(lefttype, forcepromote(righttype, ast->right));
            return lefttype;
        } else if (IsPointerType(righttype) && IsIntOrGenericType(lefttype)) {
            ast->left = ScalePointer(righttype, forcepromote(lefttype, ast->left));
            return righttype;
        } else {
            return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
        }
    case '*':
    case '/':
    case K_MODULUS:
    case K_POWER:
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
    case K_LTU:
    case K_LEU:
    case K_GTU:
    case K_GEU:
        CompileComparison(ast->d.ival, ast, lefttype, righttype);
        return CurBoolType();
    case K_NEGATE:
    case K_ABS:
    case K_SQRT:
        if (IsFloatType(rettype)) {
            isfloat64 = IsFloat64Type(rettype);
            if (!gl_fixedreal) {
                if (op == K_ABS) {
                    *ast = *MakeOperatorCall(isfloat64 ? double_abs : float_abs, ast->right, NULL, NULL);
                } else if (op == K_SQRT) {
                    *ast = *MakeOperatorCall(isfloat64 ? double_sqrt : float_sqrt, ast->right, NULL, NULL);
                } else {
                    if (IsConstExpr(ast->right) && !isfloat64) {
                        int x = EvalConstExpr(ast->right);
                        *ast = *NewAST(AST_FLOAT, NULL, NULL);
                        ast->d.ival = x ^ 0x80000000U;
                    } else {
                        *ast = *MakeOperatorCall(isfloat64 ? double_neg : float_neg, ast->right, NULL, NULL);
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
            int tsize;
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
            tsize = TypeSize(rettype);
            if (IsUnsignedType(rettype) && op == K_ABS) {
                *ast = *ast->right; // ignore the ABS
                return (tsize <= LONG_SIZE) ? ast_type_unsigned_long : ast_type_unsigned_long64;
            }
            if (tsize > LONG_SIZE) {
                switch (op) {
                case K_NEGATE:
                    *ast = *MakeOperatorCall(int64_neg, ast->right, NULL, NULL);
                    break;
                case K_ABS:
                    *ast = *MakeOperatorCall(int64_abs, ast->right, NULL, NULL);
                    break;
                default:
                    ERROR(ast, "operator not supported for 64 bits");
                }
                return rettype;
            }
            return rettype; // was ast_type_long
        }
    case K_ASC:
        if (!CompatibleTypes(righttype, ast_type_string)) {
            ERROR(ast, "expected string argument to ASC");
        } else {
            AST *newast;
            AST *sexpr = ast->right;
            if (sexpr && sexpr->kind == AST_STRINGPTR) {
                sexpr = sexpr->left;
                if (sexpr && sexpr->kind == AST_EXPRLIST) {
                    sexpr = sexpr->left;
                }
            }
            if (sexpr && sexpr->kind == AST_STRING) {
                // literal: fix it up here
                newast = AstInteger(sexpr->d.string[0]);
                *ast = *newast;
            } else if (sexpr && sexpr->kind == AST_INTEGER) {
                newast = AstInteger(sexpr->d.ival);
                *ast = *newast;
            } else {
                newast = NewAST(AST_MEMREF, ast_type_byte, ast->right);
                *ast = *newast;
            }
        }
        return ast_type_long;
    case K_STRLEN:
        if (!CompatibleTypes(righttype, ast_type_string)) {
            if (curfunc && IsBasicLang(curfunc->language)) {
                ERROR(ast, "expected string argument to LEN");
            } else {
                ERROR(ast, "expected string argument to __builtin_strlen");
            }
        } else {
            AST *newast;
            AST *sexpr = ast->right;
            
            if (sexpr && sexpr->kind == AST_STRINGPTR) {
                /* the -1 is to remove the trailing 0 */
                newast = AstInteger(AstStringLen(sexpr) - 1);
            } else {
                newast = MakeOperatorCall(string_length, sexpr, NULL, NULL);
            }
            *ast = *newast;
        }
        return ast_type_long;
    case K_BOOL_NOT:
    case K_BOOL_AND:
    case K_BOOL_OR:
        if (IsFloatType(lefttype)) {
            isfloat64 = IsFloat64Type(lefttype);
            ast->left = MakeOperatorCall(isfloat64 ? double_cmp : float_cmp, ast->left, AstInteger(0), AstInteger(1));
            lefttype = CurBoolType();
        }
        if (IsFloatType(righttype)) {
            isfloat64 = IsFloat64Type(righttype);
            ast->right = MakeOperatorCall(isfloat64 ? double_cmp : float_cmp, ast->right, AstInteger(0), AstInteger(1));
            righttype = CurBoolType();
        }
        if (lefttype && !IsBoolCompatibleType(lefttype)) {
            ERROR(ast, "Expression not compatible with boolean operation");
        } else if (righttype && !IsBoolCompatibleType(righttype)) {
            ERROR(ast, "Expression not compatible with boolean operation");
        }
        return CurBoolType();
    case K_INCREMENT:
    case K_DECREMENT:
        if ( (lefttype && IsConstType(lefttype) )
                || (righttype && IsConstType(righttype)) )
        {
            const char *name = NULL;
            if (ast->left && IsIdentifier(ast->left)) {
                name = GetUserIdentifierName(ast->left);
            } else if (ast->right && IsIdentifier(ast->right)) {
                name = GetUserIdentifierName(ast->right);
            }
            if (name) {
                WARNING(ast, "increment/decrement of const variable `%s'", name);
            } else {
                WARNING(ast, "increment/decrement of const item");
            }
        }
        if (lefttype && (IsPointerType(lefttype) || IsIntOrGenericType(lefttype))) {
            return lefttype;
        }
        if (righttype && (IsPointerType(righttype) || IsIntOrGenericType(righttype))) {
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
// NOTE: if **astptr is NULL, then we cannot do coercion
//
AST *CoerceAssignTypes(AST *line, int kind, AST **astptr, AST *desttype, AST *srctype, const char *msg)
{
    ASTReportInfo saveinfo;
    AST *expr = astptr ? *astptr : NULL;
    int lang = curfunc ? curfunc->language : (current ? current->mainLanguage : LANG_CFAMILY_C);

    if (expr && expr->kind == AST_INTEGER && expr->d.ival == 0) {
        // handle literal '0' specially for C
        if (curfunc && IsCLang(curfunc->language)) {
            if (IsPointerType(desttype)) {
                return desttype;
            }
        }
    }
    AstReportAs(expr, &saveinfo);
    if (IsRefType(desttype) && (kind == AST_FUNCCALL || kind == AST_RETURN)) {
        // passing or returning reference parameter
        if (!astptr) {
            ERROR(line, "Unable to pass or return multiple function result to reference parameter");
            AstReportDone(&saveinfo);
            return NULL;
        }
        if (desttype->kind == AST_COPYREFTYPE) {
            // need to create a temporary duplicate
            AST *sizeExpr = AstInteger(TypeSize(srctype));
            AST *lptr = MakeOperatorCall(gc_alloc_managed, sizeExpr, NULL, NULL);;
            AST *rptr = StructAddress(expr);
            AST *copy = MakeOperatorCall(struct_copy, lptr, rptr, sizeExpr);
            *astptr = copy;
        } else {
            *astptr = NewAST(AST_ADDROF, expr, NULL);
            if (curfunc && IsLocalVariable(expr)) {
                curfunc->local_address_taken = 1;
            }
        }
        srctype = NewAST(AST_REFTYPE, srctype, NULL);
    }    
    if (!desttype || !srctype) {
        AstReportDone(&saveinfo);
        return desttype;
    }

    if (IsRefType(srctype) && !IsRefType(desttype) && CompatibleTypes(desttype, srctype->left)) {
        return desttype;
    }
    if (IsFloatType(desttype)) {
        if (IsIntType(srctype)) {
            if (!astptr) {
                ERROR(line, "Unable to convert multiple function result to float");
                AstReportDone(&saveinfo);
                return ast_type_float;
            }
            *astptr = domakefloat(srctype, expr);
            srctype = ast_type_float;
        }
    }
    // allow floats to be cast as ints
    if (IsIntType(desttype) && IsFloatType(srctype)) {
        if (!astptr) {
            ERROR(line, "Unable to convert float function result to integer");
        } else {
            expr = dofloatToInt(expr, srctype, desttype);
            *astptr = expr;
        }
        AstReportDone(&saveinfo);
        return desttype;
    }

    // automatically cast arrays to pointers if necessary
    if (IsArrayType(srctype) && (IsPointerType(desttype) || !desttype)) {
        srctype = ArrayToPointerType(srctype);
        if (!astptr) {
            ERROR(line, "Unable to convert array function result to pointer");
        } else {
            expr = ArrayAddress(expr);
            *astptr = expr;
        }
    }
    // similarly for classes in some languages
    if (IsClassType(srctype) && (IsPointerType(desttype) || !desttype) && ( IsBasicLang(lang) || IsPythonLang(lang) ))
    {
        srctype = ClassToPointerType(srctype);
        if (!astptr) {
            ERROR(line, "Unable to convert class function result to pointer");
        } else {
            expr = StructAddress(expr);
            *astptr = expr;
        }
    }

    if (IsFunctionType(srctype) && IsPointerType(desttype) && !IsPointerType(srctype)) {
        srctype = FunctionPointerType(srctype);
        if (astptr) {
            expr = FunctionAddress(expr);
            *astptr = expr;
        } else {
            ERROR(line, "Unable to convert function result to pointer");
        }
    }
    // allow pair of 32 bit untyped to fill in a 64 bit
    if (IsInt64Type(desttype) && srctype && srctype->kind == AST_TUPLE_TYPE) {
        if (srctype->left == NULL && srctype->right && srctype->right->kind == AST_TUPLE_TYPE && srctype->right->right == NULL && srctype->right->left == NULL) {
            srctype = desttype;
        }
    }
    if (!CompatibleTypes(desttype, srctype)) {
        const char *desttype_name, *srctype_name;
        // special case: pointers can be passed as parameters to generic types
        if (kind == AST_FUNCCALL && IsGenericType(desttype) && IsPointerType(srctype)) {
            AstReportDone(&saveinfo);
            return desttype;
        }
        // check for interface conversion
        if (IsInterfaceType(BaseType(desttype))) {
            if (!IsPointerType(desttype)) {
                ERROR(expr, "interfaces are accessible only as pointers or references");
                AstReportDone(&saveinfo);
                return desttype;
            }
            AST *cvt = ConvertInterface(BaseType(desttype), BaseType(srctype), expr);
            if (!cvt) {
                desttype_name = TypeName(BaseType(desttype));
                srctype_name = TypeName(BaseType(srctype));
                ERROR(expr, "Unable to convert %s to interface %s", srctype_name, desttype_name);
                AstReportDone(&saveinfo);
                return desttype;
            }
            // fix up pointer stuff here
            *astptr = cvt;
            //
            return desttype;
        }
            
        //
        desttype_name = TypeName(desttype);
        srctype_name = TypeName(srctype);
        if (IsPointerType(desttype) && IsPointerType(srctype)) {
            if (curfunc && IsBasicLang(curfunc->language) && IsRefType(desttype) && IsArrayType(desttype->left) && TypeSize(desttype->left) == 0) {
                /* OK, parameter declared as foo() so can accept any array */
            } else {
                WARNING(line, "incompatible pointer types in %s: expected %s but got %s", msg, desttype_name, srctype_name);
            }
        } else {
            // in C, we can coerce int to pointer and vice-versa
            bool onlyWarn = false;
            if (curfunc && IsCLang(curfunc->language)) {
                if (IsPointerType(srctype) && IsIntType(desttype) && TypeSize(desttype) >= LONG_SIZE) {
                    onlyWarn = true;
                } else if (IsPointerType(desttype) && IsIntType(srctype) && TypeSize(srctype) == LONG_SIZE) {
                    onlyWarn = true;
                }
            }
            if (onlyWarn) {
                WARNING(line, "mixing pointer and integer types in %s: expected %s but got %s", msg, desttype_name, srctype_name);
            } else {
                ERROR(line, "incompatible types in %s: expected %s but got %s", msg, desttype_name, srctype_name);
            }
            AstReportDone(&saveinfo);
            return desttype;
        }
    }
    if (IsConstType(desttype) && kind == AST_ASSIGN) {
        // see if we can find an exact name
        if (line && line->kind == AST_ASSIGN && IsIdentifier(line->left)) {
            WARNING(line, "assignment to const variable `%s'", GetUserIdentifierName(line->left));
        } else {
            WARNING(line, "assignment to const item");
        }
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
                if (astptr) {
                    if (IsUnsignedType(srctype)) {
                        *astptr = dopromote(expr, rsize, lsize, K_ZEROEXTEND);
                    } else {
                        *astptr = dopromote(expr, rsize, lsize, K_SIGNEXTEND);
                    }
                } else {
                    WARNING(line, "Unable to widen function result");
                }
            } else if (rsize == 8) {
                if (lsize < rsize) {
                    // narrowing cast
                    if (astptr) {
                        *astptr = donarrow(expr, rsize, lsize, IsUnsignedType(srctype));
                    } else {
                        ERROR(line, "Unable to narrow parameter");
                    }
                } else if (lsize == 8 && expr->kind == AST_INTEGER) {
                    *astptr = NewAST(AST_EXPRLIST,
                                     NewAST(AST_GETLOW, expr, NULL),
                                     NewAST(AST_EXPRLIST,
                                            NewAST(AST_GETHIGH, expr, NULL),
                                            NULL));
                }
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
    if (IsFunctionType(desttype) && !IsPointerType(desttype)) {
        desttype = FunctionPointerType(desttype);
    }
    if (IsPointerType(desttype) || IsGenericType(desttype)) {
        if (IsFloatType(srctype)) {
            src = dofloatToInt(src, srctype, ast_type_long);
            srctype = ast_type_long;
        }
        if (IsArrayType(srctype)) {
            AstReportDone(&saveinfo);
            return ArrayAddress(src);
        }
        if (IsFunctionType(srctype) && IsFunctionType(desttype)) {
            int n1 = NumArgsForType(srctype);
            int n2 = NumArgsForType(desttype);
            if (n1 != n2 && NuBytecodeOutput()) {
                WARNING(src, "Casting function with %d arguments to one with %d arguments may not work", n1, n2);
            }
        }
        if (IsPointerType(srctype)) {
            if (IsPointerType(desttype)) {
                /* check for casting away "const" */
                AST *srcbase, *dstbase;
                srcbase = BaseType(srctype);
                dstbase = BaseType(desttype);
                if (IsConstType(srcbase) && !IsConstType(dstbase) && !(curfunc && IsCLang(curfunc->language)) ) {
                    WARNING(src, "cast removes const from pointer type");
                }
            }
            AstReportDone(&saveinfo);
            return src;
        }
        if (IsIntType(srctype)) {
            /* FIXME: should probably check size here */
            AstReportDone(&saveinfo);
            return src;
        }
        if (srctype->kind == AST_FUNCTYPE) {
            AstReportDone(&saveinfo);
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
            src = dofloatToInt(src, srctype, ast_type_long);
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


static int PushSize(AST *list)
{
    int size = 0;
    AST *typ;

    while (list) {
        typ = ExprType(list->left);
        size += TypeSize(typ);
        list = list->right;
    }
    return size;
}

//
// function for doing type checking and various kinds of
// type related manipulations. for example:
//
// signed/unsigned shift: x >> y  => signed shift if x is signed,
//                                   unsigned otherwise
// returns the most recent type signature
//
static AST *doCheckTypes(AST *ast)
{
    AST *ltype, *rtype;
    if (!ast) return NULL;
    if (ast->kind == AST_INLINEASM) return NULL;

    if (ast->kind == AST_CAST) {
        AST *cast;
        ltype = DerefType(ast->left);
        rtype = doCheckTypes(ast->right);
        cast = doCast(ltype, rtype, ast->right);
        if (cast) {
            ast->right = cast;
        }
        return ltype;
    }
    if (IsIdentifier(ast)) {
        ltype = NULL;
    } else {
        ltype = doCheckTypes(ast->left);
    }
    if (ast->kind != AST_METHODREF) {
        rtype = doCheckTypes(ast->right);
    } else {
        rtype = NULL;
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
    case AST_THROW:
        if (!IsIntOrGenericType(ltype)) {
            WARNING(ast, "Throwing non-integral types is not supported");
        }
        return NULL;
    case AST_COGINIT:
        ltype = ast_type_long;
        {
            bool isCog = IsSpinCoginit(ast, NULL);

            // promote types of parameters if necessary
            AST *paramlist = ast->left;
            AST *paramtype;
            AST *cogid, *funcall, *stack;
            while (paramlist) {
                stack = paramlist->left;
                paramtype = ExprType(stack);
                if (IsArrayType(paramtype)) {
                    paramlist->left = ArrayAddress(paramlist->left);
                }
                paramlist = paramlist->right;
            }
            paramlist = ast->left;
            if (!paramlist) {
                ERROR(ast, "Missing cog parameter for coginit/cognew");
                return NULL;
            }
            cogid = paramlist->left;
            paramlist = paramlist->right;
            if (!paramlist) {
                ERROR(ast, "Missing function call in coginit/cognew");
                return NULL;
            }
            funcall = paramlist->left;
            paramlist = paramlist->right;
            if (!paramlist) {
                ERROR(ast, "Missing stack parameter for coginit/cognew");
                return NULL;
            }
            stack = paramlist->left;
            if (paramlist->right) {
                ERROR(ast, "Too many parameters to coginit/cognew");
                return NULL;
            }
            paramtype = ExprType(cogid);
            if (paramtype && !IsIntType(paramtype)) {
                ERROR(ast, "Expected integer type for COG id");
                return NULL;
            }
            if (!isCog) {
                paramtype = ExprType(funcall);
                if (paramtype && !IsPointerType(paramtype)) {
                    WARNING(ast, "Expected pointer to instructions for coginit/cognew");
                }
            }
            paramtype = ExprType(stack);
            if (paramtype && !IsPointerType(paramtype) && isCog) {
                // this is only an issue if the first parameter is a pointer to a high level function
                ERROR(ast, "Expected pointer to stack as last parameter to coginit/cogid");
            }
        }
        break;
    case AST_OPERATOR:
        ltype = CoerceOperatorTypes(ast, ltype, rtype);
        break;
    case AST_ASSIGN:
        if (rtype) {
            AST *lhs = ast->left;
            if (!IsAstTempVariable(lhs)) {
                if (ast->right && ast->right->kind == AST_CAST && IsRefType(ast->right->left)) {
                    // undo dereference of lhs, if it happened
                    if (lhs->kind == AST_ARRAYREF && lhs->left->kind == AST_MEMREF && IsIdentifier(lhs->left->right)) {
                        lhs = lhs->left->right;
                        ast->left = lhs;
                        ltype = ExprType(lhs);
                    }
                }
                ltype = CoerceAssignTypes(ast, AST_ASSIGN, &ast->right, ltype, rtype, "assignment");
            }
        }
        if (ltype && IsClassType(ltype)) {
            int siz = TypeSize(ltype);
            if (TypeGoesOnStack(ltype)) {
                AST *lhs = ast->left;
                AST *rhs = ast->right;
                // convert the assignment to a memcpy (or memset in some cases)
                AST *lptr = StructAddress(lhs);
                if (!rhs) rhs = AstInteger(0);
                if (rhs->kind == AST_INTEGER) {
                    AST *zero = MakeOperatorCall(struct_memset, lptr, rhs, AstInteger(siz));
                    *ast = *NewAST(AST_MEMREF, NULL, zero);
                    if (rhs->d.ival != 0) {
                        WARNING(ast, "filling memory with non-zero value may not work properly");
                    }
                } else {
                    AST *rptr = StructAddress(rhs);
                    AST *copy = MakeOperatorCall(struct_copy, lptr, rptr, AstInteger(siz));
                    *ast = *NewAST(AST_MEMREF, NULL, copy);
//                WARNING(ast, "Need to convert to memcpy");
                }
            }
        }
        break;
    case AST_RETURN:
        if (ast->left) {
            rtype = ltype; // type of actual expression
            ltype = GetFunctionReturnType(curfunc);
            ltype = CoerceAssignTypes(ast, AST_RETURN, &ast->left, ltype, rtype, "return");
        }
        break;
    case AST_FUNCCALL:
    {
        AST *actualParamList = ast->right;
        AST *actualParamListPrev = ast;
        AST *calledParamList;
        AST *expectType, *passedType;
        AST *functype;
        AST *tupleType = NULL;
        AST **varArgsPlace = NULL;
        AST *varArgs = NULL;
        AST *varArgsList = NULL;
        int varargsOffset = 0;
        int tupleCount = 0;

        functype = RemoveTypeModifiers(ExprType(ast->left));
        if (functype && functype->kind == AST_PTRTYPE) {
            functype = RemoveTypeModifiers(functype->left);
        }
        if (!functype || IsFunctionType(functype)) {
            calledParamList = functype ? functype->right : NULL;
            while (actualParamList) {
                AST *paramId = calledParamList ? calledParamList->left : NULL;
                AST *actualParam = actualParamList->left;

                expectType = NULL;
                if (tupleType) {
                    passedType = tupleType->left;
                } else {
                    passedType = ExprType(actualParam);
                    if (passedType && passedType->kind == AST_TUPLE_TYPE) {
                        // a tuple substitutes for multiple parameters
                        tupleType = passedType;
                        passedType = passedType->left;
                        tupleCount = TypeSize(tupleType)/LONG_SIZE;
                    } else {
                        tupleCount = 0;
                    }
                }
                if (paramId) {
                    // if the parameter has a type declaration, use it
                    if (paramId->kind == AST_DECLARE_VAR) {
                        expectType = ExprType(paramId);
                    } else if (paramId->kind == AST_VARARGS) {
                        /* anything */
                        if (!varArgsPlace) {
                            if (NoVarargsOutput()) {
                                int bytes = PushSize(actualParamList);
                                AST *funcall = NewAST(AST_FUNCCALL, gc_alloc_managed,
                                                      NewAST(AST_EXPRLIST, AstInteger(bytes), NULL));
                                varArgs = AstTempLocalVariable("_varargs_", NULL);

                                varArgsPlace = &actualParamListPrev->right;
                                varArgsList = NewAST(AST_SEQUENCE, AstAssign(varArgs, funcall), NULL);
                            }
                        }
                    }
                }
                if (!expectType) {
                    // pass arrays as pointers
                    if (IsArrayType(passedType)) {
                        expectType = ArrayToPointerType(passedType);
                    } else if (TypeGoesOnStack(passedType)) {
                        // need to emit a copy
                        expectType = NewAST(AST_COPYREFTYPE, passedType, NULL);
                    } else if (TypeSize(passedType) > LONG_SIZE) {
                        expectType = passedType;
                    } else {
                        // we use const generic to avoid lots of warning
                        // messages about passing strings to printf
                        expectType = ast_type_const_generic;
                    }
                }
                if (tupleType) {
                    // cannot coerce function arguments, really
                    CoerceAssignTypes(ast, AST_FUNCCALL, NULL, expectType, passedType, "parameter passing");
                    tupleType = tupleType->right;
                } else {
                    CoerceAssignTypes(ast, AST_FUNCCALL, &actualParamList->left, expectType, passedType, "parameter passing");
                }
                if (!tupleType) {
                    if (varArgsList) {
                        AST *memref = NewAST(AST_MEMREF, expectType, AstOperator('+', varArgs, AstInteger(varargsOffset)));
                        AST *assignref = NULL;

                        if (tupleCount) {
                            int tupleOffset;
                            for (tupleOffset = tupleCount-1; tupleOffset >= 0; tupleOffset--) {
                                assignref = NewAST(AST_EXPRLIST,
                                                   NewAST(AST_ARRAYREF, memref, AstInteger(tupleOffset)),
                                                   assignref);
                            }
                        } else {
                            assignref = NewAST(AST_ARRAYREF, memref, AstInteger(0));
                        }
                        AST *assign = AstAssign(assignref,
                                                actualParamList->left);
                        varArgsList->right = assign;
                        varArgsList = NewAST(AST_SEQUENCE, varArgsList, NULL);
                        varargsOffset += TypeSize(expectType);
                    }
                    actualParamListPrev = actualParamList;
                    actualParamList = actualParamList->right;
                }
                if (calledParamList) {
                    calledParamList = calledParamList->right;
                }
            }
            ltype = functype ? functype->left : NULL;
            if (varArgsPlace) {
                varArgsList = NewAST(AST_SEQUENCE, varArgsList, varArgs);
                *varArgsPlace = NewAST(AST_EXPRLIST, varArgsList, NULL);
            }
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
        return ExprType(ast); //ast_type_long;
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
    case AST_FUNC_NAME:
        *ast = *AstStringPtr(curfunc->name);
    /* fall through */
    case AST_STRING:
    case AST_STRINGPTR:
        if (curfunc && IsBasicLang(curfunc->language)) {
            return ast_type_string;
        }
        return ast_type_ptr_byte;
    case AST_ADDROF:
    case AST_ABSADDROF:
        if (IsFunctionType(ltype) && !IsPointerType(ltype)) {
            *ast = *BuildMethodPointer(ast);
            return ltype;
        }
        return NewAST(AST_PTRTYPE, ltype, NULL);
    case AST_ARRAYREF:
    {
        AST *lefttype = ltype;
        AST *righttype = rtype ? rtype : ExprType(ast->right);
        AST *basetype;

        righttype = CoerceAssignTypes(ast, AST_ARRAYREF, &ast->right, ast_type_long, righttype, "array indexing");
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
            if (curfunc && IsBasicLang(curfunc->language)) {
                extern Symbol *GetCurArrayBase();
                Symbol *sym = GetCurArrayBase();
                if (sym && sym->kind == SYM_CONSTANT) {
                    ast->right = AstOperator('-', ast->right, (AST *)sym->v.ptr);
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
            // try to figure out a name for this
            const char *name = GetExprString(ast->left);
            ERROR(ast, "Array dereferences on non-array %s", name);
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
        if ( IsFloatType(ltype) && IsIntType(rtype) ) {
            // promote right side to float
            if (IsFloat64Type(ltype)) {
                outputs->right = domakedouble(rtype, outputs->right);
            } else {
                outputs->right = domakefloat(rtype, outputs->right);
            }
            rtype = ltype;
        } else if ( IsFloatType(rtype) && IsIntType(ltype) ) {
            // promote left side to float
            if (IsFloat64Type(rtype)) {
                outputs->left = domakedouble(ltype, outputs->left);
            } else {
                outputs->left = domakefloat(ltype, outputs->left);
            }
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
    {
        const char *thename = GetIdentifierName(ast->right);
        if (IsRefType(ltype)) {
            AST *deref;
            AST *basetype = DerefType(BaseType(ltype));
            deref = DupAST(ast->left);
            deref = NewAST(AST_MEMREF, basetype, deref);
            deref = NewAST(AST_ARRAYREF, deref, AstInteger(0));
            deref = NewAST(AST_METHODREF, deref, ast->right);
            *ast = *deref;
            ltype = basetype;
        }
        if (ltype && !IsClassType(ltype)) {
            ERROR(ast, "Method reference on non-class %s", GetIdentifierName(ast->left));
            return ltype;
        }
        if (!thename) {
            ERROR(ast, "expected identifier after `.'");
            return NULL;
        }
        return DerefType(ExprType(ast));
    }
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
    case AST_SYMBOL:
        // add super class lookups if necessary
    {
        Module *P;
        AST *supers = NULL;
        static AST *superref = NULL;
        ASTReportInfo saveinfo;
        Symbol *sym = LookupAstSymbol(ast, NULL);
        if (!sym) {
            return NULL;
        }
        AstReportAs(ast, &saveinfo);
        ltype = DerefType(ExprType(ast));
        if (!ltype && sym->kind == SYM_HWREG) {
            ltype = ast_type_unsigned_long;
        }
        // if this is a REFTYPE then dereference it
        if (ltype && IsRefType(ltype)) {
            AST *deref;
            AST *basetype = DerefType(BaseType(ltype));
            deref = DupAST(ast);
            deref = NewAST(AST_MEMREF, basetype, deref);
            deref = NewAST(AST_ARRAYREF, deref, AstInteger(0));
            *ast = *deref;
            ltype = basetype;
        }
        if (sym->kind == SYM_FUNCTION) {
            Function *f = (Function *)sym->v.ptr;
            if (f->module == current || IsSystemModule(f->module)) {
                AstReportDone(&saveinfo);
                return ltype;
            }
        }
        if (sym->kind == SYM_VARIABLE || sym->kind == SYM_FUNCTION) {
            const char *name = sym->our_name;
            int supersValid = 1;
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
                if (P && !FindSymbol(&P->objsyms, "__super")) {
                    supersValid = 0;
                }
            }
            if (sym && supers) {
                if (supersValid) {
                    *ast = *NewAST(AST_METHODREF, supers, DupAST(ast));
                } else if (P) {
                    // produce a warning if P is not the top level class
                    if (!IsTopLevel(P)) {
                        ERROR(ast, "Cannot handle reference to method of enclosing class");
                    }
                }
            }
        }
        AstReportDone(&saveinfo);
        return ltype;
    }
    case AST_EXPRLIST:
        if (ast->right) {
            return NewAST(AST_TUPLE_TYPE, ltype, rtype);
        }
        return ltype;
    case AST_SEQUENCE:
    case AST_STMTLIST:
    case AST_CONSTANT:
        return DerefType(ExprType(ast));
    case AST_SIMPLEFUNCPTR:
        return ast_type_generic;
    case AST_VA_START:
        if (NoVarargsOutput()) {
            ast->kind = AST_ASSIGN;
            ast->d.ival = K_ASSIGN;
            ast->right = varargs_ident;
        }
        break;
    case AST_VA_ARG:
        if (NoVarargsOutput()) {
            AST *typ = ExprType(ast->left);
            int siz = TypeSize(typ);
            AST *args = ast->right;
            AST *fetch;
            AST *incr;

            if (siz > 8) {
                ERROR(ast, "large varargs not supported with this output type");
                siz = 8;
            }
            incr = AstOperator('+', args, AstInteger(siz));
            fetch = NewAST(AST_MEMREF, typ,
                           NewAST(AST_POSTSET, args, incr));
            ast->kind = AST_ARRAYREF;
            ast->left = fetch;
            ast->right = AstInteger(0);
            return typ;
        }
        return DerefType(ExprType(ast));
    default:
        break;
    }
    if (IsFloatType(ltype)) {
        ActivateFeature(FEATURE_FLOAT_USED);
    }
    return DerefType(ltype);
}

AST *CheckTypes(AST *ast)
{
    return doCheckTypes(ast);
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
        basic_print_float = getBasicPrimitive("_basic_print_float");
        float_pow_n = getBasicPrimitive("_float_pow_n");
        float_powf = getBasicPrimitive("__builtin_powf");
        basic_get_float = getBasicPrimitive("_basic_get_float");
        if (gl_fixedreal) {
            float_mul = getBasicPrimitive("_fixed_mul");
            float_div = getBasicPrimitive("_fixed_div");
        } else {
            float_cmp = getBasicPrimitive("_float_cmp");
            float_add = getBasicPrimitive("_float_add");
            float_sub = getBasicPrimitive("_float_sub");
            float_mul = getBasicPrimitive("_float_mul");
            float_div = getBasicPrimitive("_float_div");
            float_fromuns = getBasicPrimitive("_float_fromuns");
            float_fromint = getBasicPrimitive("_float_fromint");
            float_fromuns64 = getBasicPrimitive("_float_fromuns64");
            float_fromint64 = getBasicPrimitive("_float_fromint64");
            float_toint = getBasicPrimitive("_float_trunc");
            float_todouble = getBasicPrimitive("_double_fromfloat");
            float_abs = getBasicPrimitive("_float_abs");
            float_sqrt = getBasicPrimitive("_float_sqrt");
            float_neg = getBasicPrimitive("_float_negate");
        }
        int64_add = getBasicPrimitive("_int64_add");
        int64_sub = getBasicPrimitive("_int64_sub");
        int64_muls = getBasicPrimitive("_int64_muls");
        int64_mulu = getBasicPrimitive("_int64_mulu");
        int64_divs = getBasicPrimitive("_int64_divs");
        int64_divu = getBasicPrimitive("_int64_divu");
        int64_mods = getBasicPrimitive("_int64_mods");
        int64_modu = getBasicPrimitive("_int64_modu");
        int64_neg = getBasicPrimitive("_int64_neg");
        int64_abs = getBasicPrimitive("_int64_abs");
        int64_cmps = getBasicPrimitive("_int64_cmps");
        int64_cmpu = getBasicPrimitive("_int64_cmpu");
        int64_shl = getBasicPrimitive("_int64_shl");
        int64_shr = getBasicPrimitive("_int64_shr");
        int64_sar = getBasicPrimitive("_int64_sar");
        int64_and = getBasicPrimitive("_int64_and");
        int64_or = getBasicPrimitive("_int64_or");
        int64_xor = getBasicPrimitive("_int64_xor");
        int64_signx = getBasicPrimitive("_int64_signx");
        int64_zerox = getBasicPrimitive("_int64_zerox");

        double_add = getBasicPrimitive("_double_add");
        double_sub = getBasicPrimitive("_double_sub");
        double_mul = getBasicPrimitive("_double_mul");
        double_div = getBasicPrimitive("_double_div");
        double_neg = getBasicPrimitive("_double_neg");
        double_sqrt = getBasicPrimitive("_double_sqrt");
        double_powf = getBasicPrimitive("_double_pow");
        double_cmp = getBasicPrimitive("_double_cmp");

        basic_get_integer = getBasicPrimitive("_basic_get_integer");
        basic_get_string = getBasicPrimitive("_basic_get_string");
        basic_read_line = getBasicPrimitive("_basic_read_line");

        basic_print_integer = getBasicPrimitive("_basic_print_integer");
        basic_print_integer_2 = getBasicPrimitive("_basic_print_integer_2");
        basic_print_integer_3 = getBasicPrimitive("_basic_print_integer_3");
        basic_print_integer_4 = getBasicPrimitive("_basic_print_integer_4");
        basic_print_unsigned = getBasicPrimitive("_basic_print_unsigned");
        basic_print_unsigned_2 = getBasicPrimitive("_basic_print_unsigned_2");
        basic_print_unsigned_3 = getBasicPrimitive("_basic_print_unsigned_3");
        basic_print_unsigned_4 = getBasicPrimitive("_basic_print_unsigned_4");
        basic_print_longinteger = getBasicPrimitive("_basic_print_longinteger");
        basic_print_longunsigned = getBasicPrimitive("_basic_print_longunsigned");
        basic_print_string = getBasicPrimitive("_basic_print_string");
        basic_print_char = getBasicPrimitive("_basic_print_char");
        basic_print_nl = getBasicPrimitive("_basic_print_nl");
        basic_put = getBasicPrimitive("_basic_put");
        basic_lock_io = getBasicPrimitive("__lockio");
        basic_unlock_io = getBasicPrimitive("__unlockio");

        struct_copy = getBasicPrimitive("bytemove");
        struct_memset = getBasicPrimitive("__builtin_memset");

        string_cmp = getBasicPrimitive("_string_cmp");
        string_concat = getBasicPrimitive("_string_concat");
        string_length = getBasicPrimitive("__builtin_strlen");
        string_tointeger = getBasicPrimitive("__builtin_atoi");
        string_frominteger = getBasicPrimitive("strint$");
        gc_alloc_managed = getBasicPrimitive("_gc_alloc_managed");
        gc_free = getBasicPrimitive("_gc_free");

        funcptr_cmp = getBasicPrimitive("_funcptr_cmp");

        make_interfaceptrs = getBasicPrimitive("_make_interfaceptrs");
        varargs_ident = AstIdentifier(VARARGS_PARAM_NAME);
    }
}

//
// create an interface skeleton for converting module P to interface I
//
Symbol *
GetInterfaceSkeleton(Module *P, Module *I, int *n_ptr, AST *line)
{
    // figure out how many functions we need in the interface
    int n = I->varsize / LONG_SIZE;
    Symbol *sym;
    *n_ptr = n;

    char *skelName = strdupcat("_skel_", I->classname);
    sym = LookupSymbolInTable(&P->objsyms, skelName);
    if (sym) {
        return sym;
    }

    // build a table of functions for the interface
    Function *pf;
    AST *initlist = NULL;
    AST *elem;
    AST *skelIdent = AstIdentifier(skelName);
    bool useDefaultImpl;
    
    for (pf = I->functions; pf; pf = pf->next) {
        if (!pf->is_public) {
            continue;
        }
        useDefaultImpl = false;
        // check for corresponding function in P
        Symbol *funcSym = LookupSymbolInTable(&P->objsyms, pf->name);
        if (funcSym) {
            if (funcSym->kind != SYM_FUNCTION) {
                ERROR(line, "Symbol %s in class %s is not a function but is needed by interface %s", pf->name, P->classname, I->classname);
                return NULL;
            }
            AST *ifaceType = pf->overalltype;
            Function *moduleFunc = (Function *)funcSym->v.ptr;
            AST *modType = moduleFunc->overalltype;
            if (!CompatibleTypes(ifaceType, modType)) {
                ERROR(line, "Incompatible types for function %s: interface expects %s but class %s has type %s", pf->name, TypeName(ifaceType), P->classname, TypeName(modType));
            }
        } else if (!pf->body) {
            ERROR(line, "Module %s does not implement interface function %s", P->classname, pf->name);
            return NULL;
        } else {
            // use the default implementation
            useDefaultImpl = true;
            funcSym = LookupSymbolInTable(&I->objsyms, pf->name);            
        }        
        elem = AstSymbol((void *)funcSym);
        elem = NewAST(AST_SIMPLEFUNCPTR, elem, NULL);
        if (useDefaultImpl) {
            AST *flag;
            if (ComplexMethodPtrs()) {
                flag = AstInteger(0x80000000);
            } else {
                flag = AstInteger(1);
            }
            elem->right = flag;
        }
        elem = NewAST(AST_EXPRLIST, elem, NULL);
        initlist = AddToList(initlist, elem);
    }
    elem = AstAssign(skelIdent, initlist);
    AST *typ = NewAST(AST_ARRAYTYPE, ast_type_generic_funcptr, AstInteger(n));

    DeclareOneGlobalVar(P, elem, typ, 1);
    DeclareModuleLabels(P);
    
    return LookupSymbolInTable(&P->objsyms, skelName);
}

//
// code for converting a class type to an interface type
//
AST *ConvertInterface(AST *ifaceType, AST *classType, AST *expr)
{
    Module *P;  // original class
    Module *I;  // interface type
    Symbol *skelSym;
    AST *newExpr;
    int n;
    
    if (!IsClassType(classType)) {
        return NULL;
    }
    P = GetClassPtr(classType);
    I = GetClassPtr(ifaceType);
    if (!I->isInterface) {
        ERROR(expr, "Internal error, expected interface type");
        return NULL;
    }
    // see if we've already built an interface skeleton
    // if not, build it
    skelSym = GetInterfaceSkeleton(P, I, &n, expr);
    if (!skelSym) {
        return NULL;
    }

    AST *skelMethod;

    skelMethod = AstSymbol((void *)skelSym);

    if (!IsPointerType(ExprType(expr))) {
        expr = NewAST(AST_ABSADDROF, expr, NULL);
    }
    // now build an interface from the skeleton and the
    // instance
    newExpr = MakeOperatorCall(make_interfaceptrs, expr, NewAST(AST_ABSADDROF, skelMethod, NULL), AstInteger(n));
    return newExpr;
}
