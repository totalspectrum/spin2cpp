/*
 * Spin to C/C++ converter
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling expressions
 */

#include "spinc.h"
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* code to get an object pointer from an object symbol */
Module *
GetObjectPtr(Symbol *sym)
{
    AST *oval;
    if (sym->type != SYM_OBJECT) {
        fprintf(stderr, "internal error, not an object symbol\n");
        abort();
    }
    oval = (AST *)sym->val;
    if (oval->kind != AST_OBJECT) {
        fprintf(stderr, "internal error, not an object AST\n");
        abort();
    }
    return oval->d.ptr;
}

/* code to find a symbol */
Symbol *
LookupSymbolInTable(SymbolTable *table, const char *name)
{
    Symbol *sym = NULL;
    if (!table) return NULL;
    sym = FindSymbol(table, name);
    if (!sym) {
        if (table->next) {
	   return LookupSymbolInTable(table->next, name);
	}
    }
    return sym;
}

Symbol *
LookupSymbolInFunc(Function *func, const char *name)
{
    Symbol *sym = NULL;

    if (func) {
        sym = LookupSymbolInTable(&func->localsyms, name);
    } else {
        sym = LookupSymbolInTable(&current->objsyms, name);
    }
    return sym;
}

Symbol *
LookupSymbol(const char *name)
{
    return LookupSymbolInFunc(curfunc, name);
}

/*
 * look up an AST that should be a symbol; print an error
 * message if it is not found
 */
Symbol *
LookupAstSymbol(AST *ast, const char *msg)
{
    Symbol *sym;
    AST *id;

    if (ast->kind == AST_IDENTIFIER) {
        id = ast;
    } else if (ast->kind == AST_ARRAYREF) {
        id = ast->left;
    } else {
        ERROR(ast, "internal error, bad id passed to LookupAstSymbol");
        return NULL;
    }
    if (id->kind != AST_IDENTIFIER) {
        ERROR(id, "expected an identifier, got %d", id->kind);
        return NULL;
    }
    sym = LookupSymbol(id->d.string);
    if (!sym) {
        ERROR(id, "unknown identifier %s used in %s", id->d.string, msg);
    }
    return sym;
}

/*
 * look up an object method or constant
 * "expr" is the context (for error messages)
 */
Symbol *
LookupObjSymbol(AST *expr, Symbol *obj, const char *name)
{
    Symbol *sym;
    Module *objstate;

    if (obj->type != SYM_OBJECT) {
        ERROR(expr, "expected an object");
        return NULL;
    }
    objstate = GetObjectPtr(obj);
    sym = FindSymbol(&objstate->objsyms, name);
    if (!sym) {
        ERROR(expr, "unknown identifier %s in %s", name, obj->name);
    }
    return sym;
}

/*
 * look up the class name of an object
 */
const char *
ObjClassName(Symbol *obj)
{
    Module *objstate;

    if (obj->type != SYM_OBJECT) {
        ERROR(NULL, "expected an object");
        return NULL;
    }
    objstate = GetObjectPtr(obj);
    return objstate->classname;
}

/*
 * look up an object constant reference
 * sets *objsym to the object and *sym to the symbol
 */

int
GetObjConstant(AST *expr, Symbol **objsym_ptr, Symbol **sym_ptr)
{
    Symbol *objsym, *sym;
    objsym = LookupAstSymbol(expr->left, "object reference");
    if (!objsym) {
        // error already printed
        return 0;
    }
    if (objsym->type != SYM_OBJECT) {
        ERROR(expr, "%s is not an object", objsym->name);
        return 0;
    }
    if (expr->right->kind != AST_IDENTIFIER) {
        ERROR(expr, "expected identifier after '.'");
        return 0;
    }
    sym = LookupObjSymbol(expr, objsym, expr->right->d.string);
    if (!sym || (sym->type != SYM_CONSTANT && sym->type != SYM_FLOAT_CONSTANT)) {
        ERROR(expr, "%s is not a constant of object %s",
              expr->right->d.string, objsym->name);
        return 0;
    }

    if (objsym_ptr) *objsym_ptr = objsym;
    if (sym_ptr) *sym_ptr = sym;
    return 1;
}

/* code to check if a coginit invocation is for a spin method */
/* if it is, returns a pointer to the spin method */
Function *
IsSpinCoginit(AST *params)
{
    AST *exprlist, *func;
    Symbol *sym = NULL;
    if (!params || !params->left || params->kind != AST_COGINIT) {
        return 0;
    }
    exprlist = params->left;
    exprlist = exprlist->right; // skip over cog id
    if (exprlist->kind != AST_EXPRLIST || !exprlist->left) {
        ERROR(params, "coginit/cognew expected expression");
        return 0;
    }
    func = exprlist->left;
    if (func->kind == AST_IDENTIFIER) {
        sym = LookupAstSymbol(func, "coginit/cognew");
        if (sym && sym->type == SYM_FUNCTION) {
            return (Function *)sym->val;
        }
    }
    if (func->kind == AST_FUNCCALL) {
        /* FIXME? Spin requires that it be a local method; do we care? */
        sym = FindFuncSymbol(func, NULL, NULL);
        if (sym) {
            return (Function *)sym->val;
        }
    }
    return 0;
}

/*
 * reverse bits 0..N-1 of A
 */
static int32_t
ReverseBits(int32_t A, int32_t N)
{
    uint32_t x = A;

    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    x = (x >> 16) | (x << 16);

    return (x >> (32-N));
}

/*
 * special case an assignment like outa[2..1] ^= -1
 */
static AST *
RangeXor(AST *dst, AST *src)
{
    int nbits;
    uint32_t mask;
    int lo;

    if (dst->right->right == NULL) {
        AST *loexpr = dst->right->left;
        nbits = 1;
        /* special case: if src is -1 or 0
         * then we don't have to construct a mask,
         * a single bit is enough
         */
        if (IsConstExpr(src) && !IsConstExpr(loexpr)) {
            int32_t srcval = EvalConstExpr(src);
            AST *maskexpr;
            if (srcval == -1 || srcval == 0) {
                maskexpr = AstOperator(T_SHL, AstInteger(1), loexpr);
                return AstAssign('^', dst->left, maskexpr);
            }
        }
        lo = EvalConstExpr(loexpr);
    } else {
        int hi = EvalConstExpr(dst->right->left);
        lo = EvalConstExpr(dst->right->right);
        if (hi < lo) {
            int tmp;
            tmp = lo; lo = hi; hi = tmp;
        }
        nbits = (hi - lo + 1);
    }
    mask = ((1U<<nbits) - 1);
    mask = mask & EvalConstExpr(src);
    mask = (mask << lo) | (mask >> (32-lo));

    return AstAssign('^', dst->left, AstInteger(mask));
}

/*
 * special case setting or clearing a range of bits
 *   outa[x] := 1
 * becomes
 *   outa |= (1<<x)
 * and
 *   outa[x] := 0
 * becomes
 *   outa[x] &= ~(1<<x)
 */
/* WARNING: mask must be a contiguous set of bits that fill
 * the desired range
 */
static AST *
RangeBitSet(AST *dst, uint32_t mask, int bitset)
{
    AST *loexpr;
    AST *hiexpr;
    AST *maskexpr;
    
    if (dst->right->right == NULL) {
        loexpr = dst->right->left;
    } else {
        int hi, lo;
        loexpr = dst->right->right;
	hiexpr = dst->right->left;
	lo = EvalConstExpr(loexpr);
	hi = EvalConstExpr(hiexpr);
	if (hi < lo) {
	  AST *tmp = loexpr;
	  loexpr = hiexpr;
	  hiexpr = tmp;
	}

    }
    maskexpr = AstOperator(T_SHL, AstInteger(mask), loexpr);
    if (bitset) {
        return AstAssign('|', dst->left, maskexpr);
    } else {
        maskexpr = AstOperator(T_BIT_NOT, NULL, maskexpr);
        return AstAssign('&', dst->left, maskexpr);
    }
}

/*
 * special code for printing a range expression
 * src->left is the hardware register
 * src->right is an AST_RANGE with left being the start, right the end
 *
 * outa[2..1] := foo
 * should evaluate to:
 *   _OUTA = (_OUTA & ~(0x3<<1)) | (foo<<1);
 *
 * Note that we want to special case some common idioms:
 *  outa[2..1] := outa[2..1] ^ -1, for example
 *
 * toplevel is "1" if we are at top level and can emit if statements
 */
AST *
TransformRangeAssign(AST *dst, AST *src, int toplevel)
{
    int reverse = 0;
    uint32_t mask;
    int nbits;
    AST *loexpr;

    if (dst->right->kind != AST_RANGE) {
        ERROR(dst, "internal error: expecting range");
        return 0;
    }
    AstReportAs(dst);  // set up error messages as if coming from "dst"
    
    /* special case logical operators */

    /* doing a NOT on the whole thing */
    if (src->kind == AST_OPERATOR && src->d.ival == T_BIT_NOT
        && AstMatch(dst, src->right))
    {
        return RangeXor(dst, AstInteger(0xffffffff));
    }
    /* now handle the ordinary case */
    /* if the "range" is just a single item it does not have to
       be constant, but for a real range we need constants on each end
    */
    if (dst->right->right == NULL) {
        nbits = 1;
        loexpr = dst->right->left;
        /* special case flipping a bit */
        if (src->kind == AST_OPERATOR && src->d.ival == '^'
            && AstMatch(dst, src->left)
            && IsConstExpr(src->right)
            && EvalConstExpr(src->right) == 1)
        {
            return RangeXor(dst, AstInteger(0xffffffff));
        }
    } else {
        int hi = EvalConstExpr(dst->right->left);
        int lo = EvalConstExpr(dst->right->right);

        if (hi < lo) {
            int tmp;
            reverse = 1;
            tmp = lo; lo = hi; hi = tmp;
        }
        nbits = (hi - lo + 1);
        if (reverse) {
            src = AstOperator(T_REV, src, AstInteger(nbits));
            if (IsConstExpr(src)) {
                src = AstInteger(EvalConstExpr(src));
            }
        }
        loexpr = AstInteger(lo);
    }
    mask = ((1U<<nbits) - 1);
    if (IsConstExpr(src)) {
        int bitset = EvalConstExpr(src);
        if (bitset == 0 || (bitset&mask) == mask) {
            return RangeBitSet(dst, mask, bitset);
        }
    }
    if (nbits >= 32) {
        return AstAssign(T_ASSIGN, dst->left, FoldIfConst(src));
    }

    /*
     * special case: if we have just one bit, then most code
     * generators actually do better on
     *  if (src & 1)
     *    outa |= mask
     *  else
     *    outa &= ~mask
     */
    if (nbits == 1 && toplevel) {
        AST *ifcond;
        AST *ifpart;
        AST *elsepart;
        AST *shift;
        AST *stmt;
        AST *ifstmt;
        AST *maskvar;
        AST *maskassign;

        maskvar = AstTempLocalVariable("_mask");
        shift = AstOperator(T_SHL, AstInteger(1), loexpr);
        shift = FoldIfConst(shift);
        maskassign = AstAssign(T_ASSIGN, maskvar, shift);
        maskassign = NewAST(AST_STMTLIST, maskassign, NULL);
        // insert the mask assignment at the beginning of the function
        maskassign->right = curfunc->body;
        curfunc->body = maskassign;
            
        ifcond = AstOperator('&', src, AstInteger(1));
        ifpart = AstOperator('|', dst->left, maskvar);
        ifpart = AstAssign(T_ASSIGN, dst->left, ifpart);
        ifpart = NewAST(AST_STMTLIST, ifpart, NULL);
        
        elsepart = AstOperator('&', dst->left,
                               AstOperator(T_BIT_NOT, NULL, maskvar));
        elsepart = AstAssign(T_ASSIGN, dst->left, elsepart);
        elsepart = NewAST(AST_STMTLIST, elsepart, NULL);
        
        stmt = NewAST(AST_THENELSE, ifpart, elsepart);
        ifstmt = NewAST(AST_IF, ifcond, stmt);
        ifstmt = NewAST(AST_STMTLIST, ifstmt, NULL);
        return ifstmt;
    }
                             
    /*
     * outa[lo] := src
     * can translate to:
     *   outa = (outa & ~(mask<<lo)) | ((src & mask) << lo)
     */
    {
        AST *andexpr;
        AST *orexpr;
        AST *maskexpr = AstInteger(mask);
        andexpr = AstOperator(T_SHL, maskexpr, loexpr);
        andexpr = AstOperator(T_BIT_NOT, NULL, andexpr);
        andexpr = FoldIfConst(andexpr);
        andexpr = AstOperator('&', dst->left, andexpr);

        orexpr = FoldIfConst(AstOperator('&', src, maskexpr));
        orexpr = AstOperator(T_SHL, orexpr, loexpr);
        orexpr = FoldIfConst(orexpr);
        orexpr = AstOperator('|', andexpr, orexpr);

        return AstAssign(T_ASSIGN, dst->left, orexpr);
    }
}

/*
 * print a range use
 */
AST *
TransformRangeUse(AST *src)
{
    int reverse = 0;
    AST *mask;
    int nbits;
    AST *loexpr;
    AST *val;

    if (src->left->kind != AST_HWREG) {
        ERROR(src, "range not applied to hardware register");
        return src;
    }
    if (src->right->kind != AST_RANGE) {
        ERROR(src, "internal error: expecting range");
        return src;
    }
    /* now handle the ordinary case */
    if (src->right->right == NULL) {
        loexpr = src->right->left;
        nbits = 1;
    } else {
        int hi = EvalConstExpr(src->right->left);
        int lo = EvalConstExpr(src->right->right);

        if (hi < lo) {
            int tmp;
            reverse = 1;
            tmp = lo; lo = hi; hi = tmp;
        }
        nbits = (hi - lo + 1);
        loexpr = AstInteger(lo);
    }
    mask = AstInteger((1U<<nbits) - 1);

    /* we want to end up with:
       ((src->left >> lo) & mask)
    */
    val = AstOperator(T_SAR, src->left, loexpr);
    val = AstOperator('&', val, mask);
    if (reverse) {
        val = AstOperator(T_REV, val, AstInteger(nbits));
    }
    return val;
}

static float
EvalFloatOperator(int op, float lval, float rval, int *valid)
{
    
    switch (op) {
    case '+':
        return lval + rval;
    case '-':
        return lval - rval;
    case '/':
        return lval / rval;
    case '*':
        return lval * rval;
    case '|':
        return intAsFloat(floatAsInt(lval) | floatAsInt(rval));
    case '&':
        return intAsFloat(floatAsInt(lval) & floatAsInt(rval));
    case '^':
        return intAsFloat(floatAsInt(lval) ^ floatAsInt(rval));
    case T_HIGHMULT:
        return lval*rval / (float)(1LL<<32);
    case T_SHL:
        return intAsFloat(floatAsInt(lval) << floatAsInt(rval));
    case T_SHR:
        return intAsFloat(((uint32_t)floatAsInt(lval)) >> floatAsInt(rval));
    case T_SAR:
        return intAsFloat(((int32_t)floatAsInt(lval)) >> floatAsInt(rval));
    case '<':
        return intAsFloat(-(lval < rval));
    case '>':
        return intAsFloat(-(lval > rval));
    case T_LE:
        return intAsFloat(-(lval <= rval));
    case T_GE:
        return intAsFloat(-(lval >= rval));
    case T_NE:
        return intAsFloat(-(lval != rval));
    case T_EQ:
        return intAsFloat(-(lval == rval));
    case T_NEGATE:
        return -rval;
    case T_ABS:
        return (rval < 0) ? -rval : rval;
    case T_SQRT:
        return sqrtf(rval);
    default:
        if (valid)
            *valid = 0;
        else
            ERROR(NULL, "invalid floating point operator %d\n", op);
        return 0;
    }
}

static int32_t
EvalIntOperator(int op, int32_t lval, int32_t rval, int *valid)
{
    
    switch (op) {
    case '+':
        return lval + rval;
    case '-':
        return lval - rval;
    case '/':
        if (rval == 0)
	    return rval;
        return lval / rval;
    case T_MODULUS:
        if (rval == 0)
	    return rval;
        return lval % rval;
    case '*':
        return lval * rval;
    case '|':
        return lval | rval;
    case '^':
        return lval ^ rval;
    case '&':
        return lval & rval;
    case T_HIGHMULT:
        return (int)(((long long)lval * (long long)rval) >> 32LL);
    case T_SHL:
        return lval << rval;
    case T_SHR:
        return ((uint32_t)lval) >> rval;
    case T_SAR:
        return ((int32_t)lval) >> rval;
    case T_ROTL:
        return ((uint32_t)lval << rval) | ((uint32_t) lval) >> (32-rval);
    case T_ROTR:
        return ((uint32_t)lval >> rval) | ((uint32_t) lval) << (32-rval);
    case '<':
        return -(lval < rval);
    case '>':
        return -(lval > rval);
    case T_LE:
        return -(lval <= rval);
    case T_GE:
        return -(lval >= rval);
    case T_NE:
        return -(lval != rval);
    case T_EQ:
        return -(lval == rval);
    case T_NEGATE:
        return -rval;
    case T_BIT_NOT:
        return ~rval;
    case T_ABS:
        return (rval < 0) ? -rval : rval;
    case T_SQRT:
        return (unsigned int)sqrtf((float)(unsigned int)rval);
    case T_DECODE:
        return (1L << rval);
    case T_ENCODE:
        return 32 - __builtin_clz(rval);
    case T_LIMITMIN:
        return (lval < rval) ? rval : lval;
    case T_LIMITMAX:
        return (lval > rval) ? rval : lval;
    case T_REV:
        return ReverseBits(lval, rval);
    default:
        if (valid)
            *valid = 0;
        else
            ERROR(NULL, "unknown operator in constant expression %d", op);
        return 0;
    }
}

static ExprVal
EvalOperator(int op, ExprVal le, ExprVal re, int *valid)
{
    if (IsFloatType(le.type) || IsFloatType(re.type)) {
        return floatExpr(EvalFloatOperator(op, intAsFloat(le.val), intAsFloat(re.val), valid));
    }
    return intExpr(EvalIntOperator(op, le.val, re.val, valid));
}

#define PASM_FLAG 0x01

static ExprVal EvalExpr(AST *expr, unsigned flags, int *valid);

/*
 * evaluate an expression in a particular parser state
 */
static ExprVal
EvalExprInState(Module *P, AST *expr, unsigned flags, int *valid)
{
    Module *saveState;
    Function *saveFunc;
    ExprVal ret;

    saveState = current;
    saveFunc = curfunc;
    current = P;
    curfunc = 0;
    ret = EvalExpr(expr, flags, valid);
    current = saveState;
    curfunc = saveFunc;
    return ret;
}

/*
 * evaluate an expression
 * if unable to evaluate, return 0 and set "*valid" to 0
 */
static ExprVal
EvalExpr(AST *expr, unsigned flags, int *valid)
{
    Symbol *sym, *objsym;
    ExprVal lval, rval;
    ExprVal aval;
    int reportError = (valid == NULL);
    ExprVal ret;
    int kind;
    
    if (!expr)
        return intExpr(0);

    kind = expr->kind;
    switch (kind) {
    case AST_INTEGER:
        return intExpr(expr->d.ival);

    case AST_FLOAT:
        return floatExpr(intAsFloat(expr->d.ival));

    case AST_STRING:
        return intExpr(expr->d.string[0]);

    case AST_TOFLOAT:
        lval = EvalExpr(expr->left, flags, valid);
        if ( !IsIntOrGenericType(lval.type)) {
            ERROR(expr, "applying float to a non integer expression");
        }
        return floatExpr((float)(lval.val));

    case AST_TRUNC:
        lval = EvalExpr(expr->left, flags, valid);
        if (!IsFloatType(lval.type)) {
            ERROR(expr, "applying trunc to a non float expression");
        }
        return intExpr((int)intAsFloat(lval.val));

    case AST_ROUND:
        lval = EvalExpr(expr->left, flags, valid);
        if (!IsFloatType(lval.type)) {
            ERROR(expr, "applying trunc to a non float expression");
        }
        return intExpr((int)roundf(intAsFloat(lval.val)));

    case AST_CONSTANT:
        return EvalExpr(expr->left, flags, valid);
    case AST_CONSTREF:
        if (!GetObjConstant(expr, &objsym, &sym)) {
            return intExpr(0);
        }
        /* while we're evaluating, use the object context */
        ret = EvalExprInState(GetObjectPtr(objsym), sym->val, flags, valid);
        return ret;
    case AST_RESULT:
        *valid = 0;
        return intExpr(0); 
    case AST_IDENTIFIER:
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            if (reportError)
                ERROR_UNKNOWN_SYMBOL(expr);
            else
                *valid = 0;
            return intExpr(0);
        } else {
            switch (sym->type) {
            case SYM_CONSTANT:
                return intExpr(EvalConstExpr((AST *)sym->val));
            case SYM_FLOAT_CONSTANT:
                return floatExpr(intAsFloat(EvalConstExpr((AST *)sym->val)));
            case SYM_LABEL:
                if (flags & PASM_FLAG) {
                    Label *lref = sym->val;
                    if (lref->asmval & 0x03) {
                        if (reportError)
                            ERROR(expr, "label %s not on longword boundary", sym->name);
                        else
                            *valid = 0;
                        return intExpr(0);
                    }
                    return intExpr(lref->asmval >> 2);
                }
                /* otherwise fall through */
            default:
                if (reportError)
                    ERROR(expr, "Symbol %s is not constant", expr->d.string);
                else
                    *valid = 0;
                return intExpr(0);
            }
        }
        break;
    case AST_OPERATOR:
        lval = EvalExpr(expr->left, flags, valid);
        if (expr->d.ival == T_OR && lval.val)
            return lval;
        if (expr->d.ival == T_AND && lval.val == 0)
            return lval;
        rval = EvalExpr(expr->right, flags, valid);
        return EvalOperator(expr->d.ival, lval, rval, valid);
    case AST_CONDRESULT:
        aval = EvalExpr(expr->left, flags, valid);
        if (!expr->right || expr->right->kind != AST_THENELSE)
            goto invalid_const_expr;
        if (aval.val) {
            return EvalExpr(expr->right->left, flags, valid);
        } else {
            return EvalExpr(expr->right->right, flags, valid);
        }
    case AST_ISBETWEEN:
        if (!expr->right || expr->right->kind != AST_RANGE) {
            goto invalid_const_expr;
        } else {
            ExprVal isge, isle;
            aval = EvalExpr(expr->left, flags, valid);
            lval = EvalExpr(expr->right->left, flags, valid);
            rval = EvalExpr(expr->right->right, flags, valid);
            isge = EvalOperator(T_LE, lval, aval, valid);
            isle = EvalOperator(T_LE, aval, rval, valid);
            return EvalOperator(T_AND, isge, isle, valid);
        }
    case AST_HWREG:
        if (flags & PASM_FLAG) {
            HwReg *hw = expr->d.ptr;
            return intExpr(hw->addr);
        }
        if (reportError)
            ERROR(expr, "Used hardware register where constant is expected");
        else if (valid)
            *valid = 0;
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        /* it's OK to take the address of a label; in that case, just
           send back the offset into the dat section
        */
        expr = expr->left;
        if (expr->kind != AST_IDENTIFIER) {
            if (reportError)
                ERROR(expr, "Only addresses of identifiers allowed");
            else
                *valid = 0;
            return intExpr(0);
        }
        sym = LookupSymbol(expr->d.string);
        if (!sym || sym->type != SYM_LABEL) {
            if (reportError)
                ERROR(expr, "Only addresses of labels allowed");
            else
                *valid = 0;
            return intExpr(0);
        } else {
            Label *lref = sym->val;
            if (kind == AST_ABSADDROF) {
                if (gl_dat_offset == -1) {
                    ERROR(expr, "offset for the @@@ operator is not known");
                } else {
                    return intExpr(lref->asmval + gl_dat_offset);
                }
            }
            return intExpr(lref->asmval);
        }
        break;
    default:
        break;
    }
invalid_const_expr:
    if (reportError)
        ERROR(expr, "Bad constant expression");
    else
        *valid = 0;
    return intExpr(0);
}

int32_t
EvalConstExpr(AST *expr)
{
    ExprVal e = EvalExpr(expr, 0, NULL);
    return e.val;
}

int32_t
EvalPasmExpr(AST *expr)
{
    ExprVal e = EvalExpr(expr, PASM_FLAG, NULL);
    return e.val;
}

int
IsConstExpr(AST *expr)
{
    int valid;
    valid = 1;
    EvalExpr(expr, 0, &valid);
    return valid;
}

int
IsFloatConst(AST *expr)
{
    int valid;
    ExprVal eval;
    valid = 1;
    eval = EvalExpr(expr, 0, &valid);
    if (valid && IsFloatType(eval.type))
        return 1;
    return 0;
}

/*
 * see if an AST refers to a parameter of this function, and return
 * an index into the list if it is
 * otherwise, return -1
 */

int
FuncParameterNum(Function *func, AST *var)
{
    AST *parm, *list;
    int idx = 0;

    if (var->kind != AST_IDENTIFIER)
        return -1;
    for (list = func->params; list; list = list->right) {
        if (list->kind != AST_LISTHOLDER) {
            ERROR(list, "bad internal parameter list");
            return -1;
        }
        parm = list->left;
        if (parm->kind != AST_IDENTIFIER)
            continue;
        if (!strcasecmp(var->d.string, parm->d.string))
            return idx;
        idx++;
    }
    return -1;
}

/* expression utility functions */
ExprVal intExpr(int32_t x)
{
    ExprVal e;
    e.type = ast_type_long;
    e.val = x;
    return e;
}

ExprVal floatExpr(float f)
{
    ExprVal e;
    e.type = ast_type_float;
    e.val = floatAsInt(f);
    return e;
}

int32_t  floatAsInt(float f)
{
    union float_or_int v;

    v.f = f;
    return v.i;
}

float intAsFloat(int32_t i)
{
    union float_or_int v;

    v.i = i;
    return v.f;
}

int
IsArray(AST *expr)
{
    Symbol *sym;
    AST *type;
    if (!expr || expr->kind != AST_IDENTIFIER)
        return 0;
    sym = LookupSymbol(expr->d.string);
    if (!sym)
        return 0;
    if (sym->type == SYM_LABEL)
        return 1;
    if (sym->type != SYM_VARIABLE && sym->type != SYM_LOCALVAR)
        return 0;
    type = (AST *)sym->val;
    if (type->kind == AST_ARRAYTYPE)
        return 1;
    return 0;
}

AST *
FoldIfConst(AST *expr)
{
    if (IsConstExpr(expr)) {
        expr = AstInteger(EvalConstExpr(expr));
    }
    return expr;
}

/*
 * check a type declaration to see if it is an array type
 */
int
IsArrayType(AST *ast)
{
    switch (ast->kind) {
    case AST_ARRAYTYPE:
        return 1;
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
        return 0;
    default:
        ERROR(ast, "Internal error: unknown type %d passed to IsArrayType",
              ast->kind);
    }
    return 0;
}

int
IsArraySymbol(Symbol *sym)
{
    AST *type = NULL;
    if (!sym) return 0;
    switch (sym->type) {
    case SYM_LOCALVAR:
    case SYM_VARIABLE:
        type = sym->val;
        break;
    case SYM_OBJECT:
        type = (AST *)sym->val;
        return type->left && type->left->kind == AST_ARRAYDECL;
    case SYM_LABEL:
        return 1;
    default:
        return 0;
    }
    return IsArrayType(type);
}

/* find function symbol in a function call */
Symbol *
FindFuncSymbol(AST *expr, AST **objrefPtr, Symbol **objsymPtr)
{
    AST *objref = NULL;
    Symbol *objsym = NULL;
    Symbol *sym = NULL;
    
    if (expr->left && expr->left->kind == AST_METHODREF) {
        const char *thename;
        objref = expr->left->left;
        objsym = LookupAstSymbol(objref, "object reference");
        if (!objsym) return NULL;
        if (objsym->type != SYM_OBJECT) {
            ERROR(expr, "%s is not an object", objsym->name);
            return NULL;
        }
        thename = expr->left->right->d.string;
        sym = LookupObjSymbol(expr, objsym, thename);
        if (!sym || sym->type != SYM_FUNCTION) {
            ERROR(expr, "%s is not a method of %s", thename, objsym->name);
            return NULL;
        }
    } else {
        sym = LookupAstSymbol(expr->left, "function call");
    }
    if (objsymPtr) *objsymPtr = objsym;
    if (objrefPtr) *objrefPtr = objref;
    return sym;
}

int
IsFloatType(AST *type)
{
    if (type->kind == AST_FLOATTYPE)
        return 1;
    return 0;
}

int
IsGenericType(AST *type)
{
    return type->kind == AST_GENERICTYPE;
}

int
IsIntType(AST *type)
{
    if (type->kind == AST_INTTYPE || type->kind == AST_UNSIGNEDTYPE)
        return 1;
    return 0;
}

/*
 * figure out an expression's type
 */
AST *
ExprType(AST *expr)
{
    return ast_type_generic;
}

