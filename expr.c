/*
 * Spin to C/C++ converter
 * Copyright 2011-2018 Total Spectrum Software Inc.
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
    return (Module *)oval->d.ptr;
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
        ERROR(expr, "expected identifier after '#'");
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
            if (sym->type == SYM_BUILTIN) {
                return NULL;
            }
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
 * return true if "subexpr" appears inside "expr"
 */
bool
ExprContainsSubexpr(AST *expr, AST *subexpr)
{
    if (expr == NULL) {
        return false;
    }
    if (AstMatch(expr, subexpr))
        return true;
    return ExprContainsSubexpr(expr->left, subexpr) || ExprContainsSubexpr(expr->right, subexpr);
}

/*
 * Replace a complicated AST with a variable that's initialized at the
 * start of the current function.
 * checks the initialized variables at the start of the function to
 * see if we already have such a variable; if so, use it
 *
 */
AST *
ReplaceExprWithVariable(const char *prefix, AST *expr)
{
    AST *ast;
    AST *exprvar;
    AST *list;
    AST *exprinit;
    AST **lastptr;
    Symbol *sym;
    if (expr->kind == AST_IDENTIFIER)
        return expr; // already an identifier!

    // FIXME: need to make sure "expr" is invariant in the function,
    // otherwise we may get the wrong value
    // until then, punt
    if (1)
        return expr;
    
    // check initialization at start of curfunc
    lastptr = &curfunc->body;

    for (list = *lastptr; list; (lastptr = &list->right),(list = *lastptr)) {
        ast = list->left;
        if (ast->kind != AST_ASSIGN) {
            break;
        }
        exprvar = ast->left;
        if (exprvar->kind != AST_IDENTIFIER) break;
        // make sure this is an internal variable
        // (user variables may be modified before we use them)
        sym = LookupSymbol(exprvar->d.string);
        if (!sym || sym->type != SYM_TEMPVAR) break;
        if (AstMatch(ast->right, expr)) {
            // use this variable, no need for a new one
            return exprvar;
        }
    }
    // didn't find a variable, create a new one
    exprvar = AstTempLocalVariable(prefix);
    exprinit = NewAST(AST_STMTLIST,
                      AstAssign(exprvar, expr),
                      NULL);
    // insert it in the initialization sequence
    exprinit->right = *lastptr;
    *lastptr = exprinit;
    return exprvar;
}

/*
 * special case an assignment like outa[2..1] ^= -1
 */
static AST *
RangeXor(AST *dst, AST *src)
{
    AST *nbits;
    AST *maskexpr;
    AST *loexpr;
    AST *hiexpr = NULL;
    
    AstReportAs(dst);
    if (dst->right->right == NULL) {
        loexpr = FoldIfConst(dst->right->left);
        nbits = AstInteger(1);
        /* special case: if src is -1 or 0
         * then we don't have to construct a mask,
         * a single bit is enough
         */
        if (IsConstExpr(src) && !IsConstExpr(loexpr)) {
            int32_t srcval = EvalConstExpr(src);
            AST *maskexpr;
            if (srcval == -1 || srcval == 0) {
                maskexpr = AstOperator(K_SHL, AstInteger(1), loexpr);
                return AstAssign(dst->left,
                                 AstOperator('^', dst->left, maskexpr));
            }
        }
    } else {
        hiexpr = FoldIfConst(dst->right->left);
        loexpr = FoldIfConst(dst->right->right);
        //nbits = (hi - lo + 1);
        nbits = AstOperator('+',
                            AstOperator(K_ABS, NULL,
                                        AstOperator('-', hiexpr, loexpr)),
                            AstInteger(1));
        nbits = FoldIfConst(nbits);
        loexpr = AstOperator(K_LIMITMAX, loexpr, hiexpr);
    }
    //mask = ((1U<<nbits) - 1);
    //mask = mask & EvalConstExpr(src);
    //mask = (mask << lo) | (mask >> (32-lo));
    maskexpr = AstOperator('-',
                           AstOperator(K_SHL, AstInteger(1), nbits),
                           AstInteger(1));
    maskexpr = FoldIfConst(maskexpr);
    maskexpr = AstOperator('&', maskexpr, src);
    maskexpr = AstOperator(K_ROTL, maskexpr, loexpr);
    maskexpr = FoldIfConst(maskexpr);
    return AstAssign(dst->left,
                     AstOperator('^', dst->left, maskexpr));
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
    AST *maskexpr;
    
    AstReportAs(dst);
    if (dst->right->right == NULL) {
        loexpr = dst->right->left;
    } else {
        AST *hiexpr;
        loexpr = dst->right->right;
	hiexpr = dst->right->left;
        loexpr = AstOperator(K_LIMITMAX, loexpr, hiexpr);
        loexpr = FoldIfConst(loexpr);
    }
    maskexpr = AstOperator(K_SHL, AstInteger(mask), loexpr);
    if (bitset) {
        return AstAssign(dst->left,
                         AstOperator('|', dst->left, maskexpr));
    } else {
        maskexpr = AstOperator(K_BIT_NOT, NULL, maskexpr);
        return AstAssign(dst->left,
                         AstOperator('&', dst->left, maskexpr));
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
    AST *nbits;
    AST *loexpr;
    AST *revsrc;
    AST *maskexpr;

    if (dst->right->kind != AST_RANGE) {
        ERROR(dst, "internal error: expecting range");
        return 0;
    }
    AstReportAs(dst);  // set up error messages as if coming from "dst"
    
    /* special case logical operators */

    /* doing a NOT on the whole thing */
    if (src->kind == AST_OPERATOR && src->d.ival == K_BIT_NOT
        && AstMatch(dst, src->right))
    {
        return RangeXor(dst, AstInteger(0xffffffff));
    }
    /* now handle the ordinary case */
    /* if the "range" is just a single item it does not have to
       be constant, but for a real range we need constants on each end
    */
    if (dst->right->right == NULL) {
        nbits = AstInteger(1);
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
        AST *hiexpr;
        AST *needrev;
        
        hiexpr = FoldIfConst(dst->right->left);
        loexpr = FoldIfConst(dst->right->right);

        //nbits = (hi - lo + 1);
        nbits = AstOperator('+',
                            AstOperator(K_ABS, NULL,
                                        AstOperator('-', hiexpr, loexpr)),
                            AstInteger(1));
        if (IsConstExpr(nbits)) {
            nbits = FoldIfConst(nbits);
        } else {
            nbits = ReplaceExprWithVariable("_nbits", nbits);
        }
        needrev = FoldIfConst(AstOperator('<', hiexpr, loexpr));
        if (IsConstExpr(loexpr)) {
            loexpr = FoldIfConst(AstOperator(K_LIMITMAX, loexpr, hiexpr));
        } else if (loexpr->kind != AST_IDENTIFIER) {
            loexpr = ReplaceExprWithVariable("_lo", loexpr);
        }
        revsrc = AstOperator(K_REV, src, nbits);
        if (IsConstExpr(needrev)) {
            if (EvalConstExpr(needrev)) {
                src = revsrc;
            }
        } else {
            src = NewAST(AST_CONDRESULT,
                         needrev,
                         NewAST(AST_THENELSE, revsrc, src));
        }
        src = FoldIfConst(src);
    }
    //mask = ((1U<<nbits) - 1);
    maskexpr = AstOperator('-',
                           AstOperator(K_SHL, AstInteger(1), nbits),
                           AstInteger(1));
    maskexpr = FoldIfConst(maskexpr);
    if (IsConstExpr(src) && IsConstExpr(maskexpr)) {
        int bitset = EvalConstExpr(src);
        int mask = EvalConstExpr(maskexpr);
        if (bitset == 0 || (bitset&mask) == mask) {
            return RangeBitSet(dst, mask, bitset);
        }
    }
    if (IsConstExpr(nbits) && EvalConstExpr(nbits) >= 32) {
        return AstAssign(dst->left, FoldIfConst(src));
    }

    /*
     * special case: if we have just one bit, then most code
     * generators actually do better on
     *  if (src & 1)
     *    outa |= mask
     *  else
     *    outa &= ~mask
     */
    if (toplevel && IsConstExpr(nbits) && EvalConstExpr(nbits) == 1) {
        AST *ifcond;
        AST *ifpart;
        AST *elsepart;
        AST *stmt;
        AST *shift;
        AST *ifstmt;
        AST *maskvar;
        AST *maskassign;

        maskvar = AstTempLocalVariable("_mask");
        shift = AstOperator(K_SHL, AstInteger(1), loexpr);
        maskassign = AstAssign(maskvar, shift);
        maskassign = NewAST(AST_STMTLIST, maskassign, NULL);
        // insert the mask assignment at the beginning of the function
        maskassign->right = curfunc->body;
        curfunc->body = maskassign;
            
        ifcond = AstOperator('&', src, AstInteger(1));
        ifpart = AstOperator('|', dst->left, maskvar);
        ifpart = AstAssign(dst->left, ifpart);
        ifpart = NewAST(AST_STMTLIST, ifpart, NULL);
        
        elsepart = AstOperator('&', dst->left,
                               AstOperator(K_BIT_NOT, NULL, maskvar));
        elsepart = AstAssign(dst->left, elsepart);
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
        if (!IsConstExpr(loexpr) && loexpr->kind != AST_IDENTIFIER) {
            loexpr = ReplaceExprWithVariable("lo_", loexpr);
        }
        if (!IsConstExpr(maskexpr)) {
            maskexpr = ReplaceExprWithVariable("mask_", maskexpr);
        }

#if 0
        andexpr = AstOperator(K_SHL, maskexpr, loexpr);
        andexpr = AstOperator(K_BIT_NOT, NULL, andexpr);
        andexpr = FoldIfConst(andexpr);
        andexpr = AstOperator('&', dst->left, andexpr);

        orexpr = FoldIfConst(AstOperator('&', src, maskexpr));
        orexpr = AstOperator(K_SHL, orexpr, loexpr);
        orexpr = FoldIfConst(orexpr);
        orexpr = AstOperator('|', andexpr, orexpr);

#else
        andexpr = AstOperator(K_SHL, maskexpr, loexpr);
        andexpr = AstOperator(K_BIT_NOT, NULL, andexpr);
        andexpr = FoldIfConst(andexpr);

        orexpr = FoldIfConst(AstOperator('&', src, maskexpr));
        orexpr = AstOperator(K_SHL, orexpr, loexpr);
        orexpr = FoldIfConst(orexpr);
        
        orexpr = NewAST(AST_MASKMOVE, dst->left, AstOperator('|', andexpr, orexpr));
      
#endif        
        return AstAssign(dst->left, orexpr);
    }
}

/*
 * print a range use
 */
AST *
TransformRangeUse(AST *src)
{
    AST *mask;
    AST *nbits;
    AST *val, *revval;
    AST *test;
    AST *hi;
    AST *lo;

    if (!curfunc) {
        ERROR(src, "Internal error, could not find function");
        return AstInteger(0);
    }
    if (src->left->kind != AST_HWREG) {
        ERROR(src, "range not applied to hardware register");
        return AstInteger(0);
    }
    if (src->right->kind != AST_RANGE) {
        ERROR(src, "internal error: expecting range");
        return src;
    }
    AstReportAs(src);
    /* now handle the ordinary case */
    if (src->right->right == NULL) {
        hi = lo = src->right->left;
        nbits = AstInteger(1);
        test = AstInteger(0); // hi < lo is always false
    } else {
        hi = src->right->left;
        lo = src->right->right;
        test = FoldIfConst(AstOperator('<', hi, lo));
        /*
        if (hi < lo) {
            int tmp;
            reverse = 1;
            tmp = lo; lo = hi; hi = tmp;
        }
        */
        //nbits = (hi - lo + 1);
        nbits = AstOperator('+', AstInteger(1),
                            AstOperator(K_ABS, NULL,
                                        AstOperator('-', hi, lo)));
        if (IsConstExpr(nbits)) {
            nbits = FoldIfConst(nbits);
        } else {
            nbits = ReplaceExprWithVariable("_bits", nbits);
        }
        lo = NewAST(AST_CONDRESULT,
                        test,
                        NewAST(AST_THENELSE, hi, lo));
        if (IsConstExpr(lo)) {
            lo = AstInteger(EvalConstExpr(lo));
        } else {
            lo = ReplaceExprWithVariable("_lo_", lo);
        }
    }
    //mask = AstInteger((1U<<nbits) - 1);
    mask = AstOperator(K_SHL, AstInteger(1), nbits);
    mask = AstOperator('-', mask, AstInteger(1));
    if (IsConstExpr(mask)) {
        mask = FoldIfConst(mask);
    } else {
        mask = ReplaceExprWithVariable("_mask_", mask);
    }
    
    /* we want to end up with:
       ((src->left >> lo) & mask)
    */
    val = FoldIfConst(AstOperator(K_SAR, src->left, lo));
    val = FoldIfConst(AstOperator('&', val, mask));
    revval = FoldIfConst(AstOperator(K_REV, val, nbits));
    
    if (IsConstExpr(test)) {
        int tval = EvalConstExpr(test);
        val = tval ? revval : val;
    } else {
        val = NewAST(AST_CONDRESULT,
                     test,
                     NewAST(AST_THENELSE,
                            revval,
                            val));
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
    case K_HIGHMULT:
        return lval*rval / (float)(1LL<<32);
    case K_SHL:
        return intAsFloat(floatAsInt(lval) << floatAsInt(rval));
    case K_SHR:
        return intAsFloat(((uint32_t)floatAsInt(lval)) >> floatAsInt(rval));
    case K_SAR:
        return intAsFloat(((int32_t)floatAsInt(lval)) >> floatAsInt(rval));
    case '<':
        return intAsFloat(-(lval < rval));
    case '>':
        return intAsFloat(-(lval > rval));
    case K_LE:
        return intAsFloat(-(lval <= rval));
    case K_GE:
        return intAsFloat(-(lval >= rval));
    case K_NE:
        return intAsFloat(-(lval != rval));
    case K_EQ:
        return intAsFloat(-(lval == rval));
    case K_NEGATE:
        return -rval;
    case K_ABS:
        return (rval < 0) ? -rval : rval;
    case K_SQRT:
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
    case K_MODULUS:
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
    case K_HIGHMULT:
        return (int)(((long long)lval * (long long)rval) >> 32LL);
    case K_SHL:
        return lval << rval;
    case K_SHR:
        return ((uint32_t)lval) >> rval;
    case K_SAR:
        return ((int32_t)lval) >> rval;
    case K_ROTL:
        return ((uint32_t)lval << rval) | ((uint32_t) lval) >> (32-rval);
    case K_ROTR:
        return ((uint32_t)lval >> rval) | ((uint32_t) lval) << (32-rval);
    case '<':
        return -(lval < rval);
    case '>':
        return -(lval > rval);
    case K_LE:
        return -(lval <= rval);
    case K_GE:
        return -(lval >= rval);
    case K_NE:
        return -(lval != rval);
    case K_EQ:
        return -(lval == rval);
    case K_NEGATE:
        return -rval;
    case K_BIT_NOT:
        return ~rval;
    case K_ABS:
        return (rval < 0) ? -rval : rval;
    case K_SQRT:
        return (unsigned int)sqrtf((float)(unsigned int)rval);
    case K_DECODE:
        return (1L << rval);
    case K_ENCODE:
        return 32 - __builtin_clz(rval);
    case K_LIMITMIN:
        return (lval < rval) ? rval : lval;
    case K_LIMITMAX:
        return (lval > rval) ? rval : lval;
    case K_REV:
        return ReverseBits(lval, rval);
    case K_ZEROEXTEND:
        {
            int shift = 32 - rval;
            return ((uint32_t)lval << shift) >> shift;
        }
        break;
    case K_SIGNEXTEND:
        {
            int shift = 32 - rval;
            return ((int32_t)lval << shift) >> shift;
        }
        break;
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

static ExprVal EvalExpr(AST *expr, unsigned flags, int *valid, int depth);

/*
 * evaluate an expression in a particular parser state
 */
static ExprVal
EvalExprInState(Module *P, AST *expr, unsigned flags, int *valid, int depth)
{
    Module *saveState;
    Function *saveFunc;
    ExprVal ret;

    saveState = current;
    saveFunc = curfunc;
    current = P;
    curfunc = 0;
    ret = EvalExpr(expr, flags, valid, depth);
    current = saveState;
    curfunc = saveFunc;
    return ret;
}

/*
 * evaluate an expression
 * if unable to evaluate, return 0 and set "*valid" to 0
 */
#define MAX_DEPTH 50

static ExprVal
EvalExpr(AST *expr, unsigned flags, int *valid, int depth)
{
    Symbol *sym, *objsym;
    ExprVal lval, rval;
    ExprVal aval;
    int reportError = (valid == NULL);
    ExprVal ret;
    int kind;

    if (!expr)
        return intExpr(0);
    if (depth > MAX_DEPTH) {
        ERROR(expr, "Expression too complicated or symbol definition loop");
        return intExpr(0);
    }

    kind = expr->kind;
    switch (kind) {
    case AST_INTEGER:
        return intExpr(expr->d.ival);

    case AST_FLOAT:
        return floatExpr(intAsFloat(expr->d.ival));

    case AST_STRING:
    {
        const char *s = expr->d.string;
        if (reportError && strlen(s) > 1) {
            WARNING(expr, "only first element of string is used");
        }
        return intExpr(expr->d.string[0]);
    }
    case AST_TOFLOAT:
        lval = EvalExpr(expr->left, flags, valid, depth+1);
        if ( !IsIntOrGenericType(lval.type)) {
            ERROR(expr, "applying float to a non integer expression");
        }
        return floatExpr((float)(lval.val));

    case AST_TRUNC:
        lval = EvalExpr(expr->left, flags, valid, depth+1);
        if (!IsFloatType(lval.type)) {
            ERROR(expr, "applying trunc to a non float expression");
        }
        return intExpr((int)intAsFloat(lval.val));

    case AST_ROUND:
        lval = EvalExpr(expr->left, flags, valid, depth+1);
        if (!IsFloatType(lval.type)) {
            ERROR(expr, "applying round to a non float expression");
        }
        return intExpr((int)roundf(intAsFloat(lval.val)));

    case AST_CONSTANT:
        return EvalExpr(expr->left, flags, valid, depth+1);
    case AST_CONSTREF:
        if (!GetObjConstant(expr, &objsym, &sym)) {
            return intExpr(0);
        }
        /* while we're evaluating, use the object context */
        ret = EvalExprInState(GetObjectPtr(objsym), (AST *)sym->val, flags, valid, depth+1);
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
            {
                ExprVal e = EvalExpr((AST *)sym->val, 0, NULL, depth+1);
                return intExpr(e.val);
            }
            case SYM_FLOAT_CONSTANT:
            {
                ExprVal e = EvalExpr((AST *)sym->val, 0, NULL, depth+1);
                return floatExpr(intAsFloat(e.val));
            }
            case SYM_LABEL:
                if (flags & PASM_FLAG) {
                    Label *lref = (Label *)sym->val;
                    if (lref->flags & LABEL_IN_HUB) {
                        return intExpr(lref->hubval);
                    }
                    if (lref->cogval & 0x03) {
                        if (reportError) {
                            ERROR(expr, "label %s in COG memory not on longword boundary", sym->name);
                        } else {
                            *valid = 0;
                        }
                        return intExpr(0);
                    }
                    return intExpr(lref->cogval >> 2);
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
        lval = EvalExpr(expr->left, flags, valid, depth+1);
        if (expr->d.ival == K_BOOL_OR && lval.val)
            return lval;
        if (expr->d.ival == K_BOOL_AND && lval.val == 0)
            return lval;
        rval = EvalExpr(expr->right, flags, valid, depth+1);
        return EvalOperator(expr->d.ival, lval, rval, valid);
    case AST_CONDRESULT:
        aval = EvalExpr(expr->left, flags, valid, depth+1);
        if (!expr->right || expr->right->kind != AST_THENELSE)
            goto invalid_const_expr;
        if (aval.val) {
            return EvalExpr(expr->right->left, flags, valid, depth+1);
        } else {
            return EvalExpr(expr->right->right, flags, valid, depth+1);
        }
    case AST_ISBETWEEN:
        if (!expr->right || expr->right->kind != AST_RANGE) {
            goto invalid_const_expr;
        } else {
            ExprVal isge, isle;
            aval = EvalExpr(expr->left, flags, valid, depth+1);
            lval = EvalExpr(expr->right->left, flags, valid, depth+1);
            rval = EvalExpr(expr->right->right, flags, valid, depth+1);
            isge = EvalOperator(K_LE, lval, aval, valid);
            isle = EvalOperator(K_LE, aval, rval, valid);
            return EvalOperator(K_BOOL_AND, isge, isle, valid);
        }
    case AST_HWREG:
        if (flags & PASM_FLAG) {
            HwReg *hw = (HwReg *)expr->d.ptr;
            return intExpr(hw->addr);
        }
        if (reportError)
            ERROR(expr, "Used hardware register where constant is expected");
        else if (valid)
            *valid = 0;
        break;
    case AST_ARRAYREF:
        if (expr->left && expr->left->kind == AST_STRING) {
            const char *str = expr->left->d.string;
            rval = EvalExpr(expr->right, flags, valid, depth+1);
            if ((!valid || *valid) && IsIntOrGenericType(rval.type)
                && rval.val >= 0 && rval.val <= strlen(str))
            {
                return intExpr(str[rval.val]);
            }
        }
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
            if (reportError) {
                if (!sym) {
                    ERROR(expr, "Unknown symbol %s", expr->d.string);
                } else {
                    ERROR(expr, "Only addresses of labels allowed");
                }
            } else {
                *valid = 0;
            }
            return intExpr(0);
        } else {
            Label *lref = (Label *)sym->val;
            if (gl_p2) {
                return intExpr(lref->hubval);
            }
            if (kind == AST_ABSADDROF) {
                if (gl_dat_offset == -1) {
                    if (reportError) {
                        ERROR(expr, "offset for the @@@ operator is not known");
                    } else {
                        *valid = 0;
                    }
                    return intExpr(0);
                } else {
                    return intExpr(lref->hubval + gl_dat_offset);
                }
            }
            return intExpr(lref->hubval);
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
    ExprVal e = EvalExpr(expr, 0, NULL, 0);
    return e.val;
}

int32_t
EvalPasmExpr(AST *expr)
{
    ExprVal e = EvalExpr(expr, PASM_FLAG, NULL, 0);
    return e.val;
}

int
IsConstExpr(AST *expr)
{
    int valid;
    valid = 1;
    EvalExpr(expr, 0, &valid, 0);
    return valid;
}

int
IsFloatConst(AST *expr)
{
    int valid;
    ExprVal eval;
    valid = 1;
    eval = EvalExpr(expr, 0, &valid, 0);
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
    if (type && type->kind == AST_ARRAYTYPE)
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

// remove modifiers from a type
static AST *
removeModifiers(AST *ast)
{
    while(ast && (ast->kind == AST_MODIFIER_CONST || ast->kind == AST_MODIFIER_VOLATILE)) {
        ast = ast->left;
    }
    return ast;
}

/*
 * check a type declaration to see if it is an array type
 */
int
IsArrayType(AST *ast)
{
    ast = removeModifiers(ast);
    if (!ast) return 0;
    switch (ast->kind) {
    case AST_ARRAYTYPE:
        return 1;
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
    case AST_PTRTYPE:
    case AST_VOIDTYPE:
        return 0;
    default:
        ERROR(ast, "Internal error: unknown type %d passed to IsArrayType",
              ast->kind);
    }
    return 0;
}

/* find size of a type */
/* for arrays, returns the whole size of the array */
/* returns size of 1 item for non-array types */
int TypeSize(AST *typ)
{
    int size;
    if (!typ) {
        return 4;
    }
    switch (typ->kind) {
    case AST_MODIFIER_CONST:
    case AST_MODIFIER_VOLATILE:
        return TypeSize(typ->left);
    case AST_ARRAYTYPE:
        size = EvalConstExpr(typ->right);
        return size * TypeSize(typ->left);
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
        return EvalConstExpr(typ->left);
    case AST_PTRTYPE:
        return 4; // all pointers are 4 bytes
    case AST_OBJECT:
    {
        Module *P = (Module *)typ->d.ptr;
        return P->varsize;
    }
    case AST_TUPLETYPE:
    {
        return typ->d.ival * 4;
    }
    default:
        ERROR(typ, "Internal error: unknown type %d passed to TypeSize",
              typ->kind);
        return 1;
    }
}

/* find base type for an array or pointer */
AST *BaseType(AST *typ)
{
    typ = removeModifiers(typ);
    if (!typ) return typ;
    switch (typ->kind) {
    case AST_ARRAYTYPE:
        return typ->right;
    case AST_PTRTYPE:
        return typ->left;
    default:
        return typ;
    }
}

/* find alignment for a type */
/* returns size of 1 item for non-array types */
int TypeAlignment(AST *typ)
{
    if (!typ) return 4;
    switch (typ->kind) {
    case AST_MODIFIER_CONST:
    case AST_MODIFIER_VOLATILE:
        return TypeAlignment(typ->left);
    case AST_ARRAYTYPE:
    case AST_PTRTYPE:
        return TypeAlignment(typ->left);
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
        return EvalConstExpr(typ->left);
    default:
        ERROR(typ, "Internal error: unknown type %d passed to TypeAlignment",
              typ->kind);
        return 1;
    }
}

int
IsArrayOrPointerSymbol(Symbol *sym)
{
    AST *type = NULL;
    if (!sym) return 0;
    switch (sym->type) {
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
    case SYM_VARIABLE:
    case SYM_PARAMETER:
    case SYM_RESULT:
        type = (AST *)sym->val;
        break;
    case SYM_OBJECT:
        type = (AST *)sym->val;
        return type->left && type->left->kind == AST_ARRAYDECL;
    case SYM_LABEL:
        return 1;
    default:
        return 0;
    }
    return IsArrayType(type) || IsPointerType(type);
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
    type = removeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_FLOATTYPE)
        return 1;
    return 0;
}

int
IsPointerType(AST *type)
{
    type = removeModifiers(type);
    if (!type) return 0;
    
    if (type->kind == AST_PTRTYPE)
        return 1;
    return 0;
}

int
PointerTypeIncrement(AST *type)
{
    if (!IsPointerType(type)) {
        return 1;
    }
    return TypeSize(type->left);
}

int
IsGenericType(AST *type)
{
    return type && type->kind == AST_GENERICTYPE;
}

int
IsIntType(AST *type)
{
    type = removeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_INTTYPE || type->kind == AST_UNSIGNEDTYPE)
        return 1;
    return 0;
}

int
IsNumericType(AST *type)
{
    type = removeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_INTTYPE || type->kind == AST_UNSIGNEDTYPE || type->kind == AST_FLOATTYPE)
        return 1;
    return 0;
}


/*
 * figure out an expression's type
 * returns NULL if we can't deduce it
 */
AST *
ExprType(AST *expr)
{
    AST *sub;

    if (!expr) return NULL;
    switch (expr->kind) {
    case AST_INTEGER:
    case AST_CONSTANT:
    case AST_CONSTREF:
    case AST_HWREG:
    case AST_ISBETWEEN:
        return ast_type_long;
    case AST_FLOAT:
    case AST_TRUNC:
    case AST_ROUND:
        return ast_type_float;
    case AST_STRING:
        // in Spin, a string is always dereferenced
        // so "abc" is the same as "a" is the same as 0x65
        // (actually no -- "abc" is the same as "a", "b", "c")
        if (current->language == LANG_SPIN) {
            return ast_type_long;
        }
        /* otherwise fall through */
    case AST_STRINGPTR:
        return ast_type_string;
    case AST_MEMREF:
        return expr->left; 
    case AST_ADDROF:
    case AST_ABSADDROF:
        sub = ExprType(expr->left);
        if (!sub) sub= ast_type_generic;
        return NewAST(AST_PTRTYPE, sub, NULL);
    case AST_IDENTIFIER:
    {
        Symbol *sym = LookupSymbol(expr->d.string);
        Label *lab;
        if (!sym) return NULL;
        switch (sym->type) {
        case SYM_CONSTANT:
        case SYM_HW_REG:
            return ast_type_long;
        case SYM_LABEL:
            lab = (Label *)sym->val;
            return NewAST(AST_ARRAYTYPE, lab->type, AstInteger(1));
        case SYM_FLOAT_CONSTANT:
            return ast_type_float;
        case SYM_VARIABLE:
        case SYM_LOCALVAR:
        case SYM_PARAMETER:
            return (AST *)sym->val;
        case SYM_FUNCTION:
            return ((Function *)sym->val)->rettype;
        default:
            return NULL;
        }            
    }
    case AST_ARRAYREF:
        sub = ExprType(expr->left);
        if (!sub) return NULL;
        if (!(sub->kind == AST_PTRTYPE || sub->kind == AST_ARRAYTYPE)) return NULL;
        return sub->left;
    case AST_FUNCCALL:
    case AST_METHODREF:
    {
        Symbol *sym = FindFuncSymbol(expr, NULL, NULL);
        if (sym) {
            if (sym->type == SYM_FUNCTION) {
                return ((Function *)sym->val)->rettype;
            }
        }
        return NULL;
    }
    case AST_OPERATOR:
    {
        AST *ltype, *rtype;
        switch (expr->d.ival) {
        case '+':
        case '-':
        case K_INCREMENT:
        case K_DECREMENT:
            ltype = ExprType(expr->left);
            rtype = ExprType(expr->right);
            if (IsFloatType(ltype) || IsFloatType(rtype)) {
                return ast_type_float;
            }
            if (!ltype) ltype = rtype;
            if (ltype) {
                if (IsIntOrGenericType(ltype)) return ltype;
                if (IsPointerType(ltype) && PointerTypeIncrement(ltype) == 1) {
                    return ltype;
                }
                return ast_type_generic;
            }
            return NULL;
        case K_NEGATE:
        case '*':
        case '/':
        case K_ABS:
        case K_SQRT:
            ltype = ExprType(expr->left);
            rtype = ExprType(expr->right);
            if (IsFloatType(ltype) || IsFloatType(rtype)) {
                return ast_type_float;
            }
            return ast_type_long;
        default:
            return ast_type_long;
        }
    }
    case AST_EXPRLIST:
    {
        AST *typ = NewAST(AST_TUPLETYPE, NULL, NULL);
        typ->d.ival = AstListLen(expr);
        return typ; 
    }
    case AST_SEQUENCE:
    {
        // find the last item in the sequence
        while (expr && expr->kind == AST_SEQUENCE) {
            if (expr->right) {
                expr = expr->right;
            } else {
                expr = expr->left;
            }
        }
        return ExprType(expr);
    }
    default:
        return NULL;
    }
}

/* check for two types the same */
int
SameTypes(AST *A, AST *B)
{
    if (A == B) return 1;
    // NULL is the same as ast_type_generic */
    if (!A) return (B == ast_type_generic);
    if (!B) return (A == ast_type_generic);
    if (A->kind != B->kind) return 0;
    return AstMatch(A->left, B->left) && SameTypes(A->right, B->right);
}

/* check for compatibility of types */
int
CompatibleTypes(AST *A, AST *B)
{
    bool typesOK = (current != NULL && current->language != LANG_SPIN);
    bool skipfloats = !typesOK;
    
    // FIXME: eventually float types should be
    // fully supported, but for now treat them
    // as generic
    if (!A || (skipfloats && A->kind == AST_FLOATTYPE)) {
        A = ast_type_generic;
    }
    if (!B || (skipfloats && B->kind == AST_FLOATTYPE)) {
        B = ast_type_generic;
    }

    if (A == B) return 1;
    if (A->kind == AST_INTTYPE || A->kind == AST_UNSIGNEDTYPE || A->kind == AST_GENERICTYPE) {
        return (B->kind == AST_INTTYPE || B->kind == AST_UNSIGNEDTYPE || B->kind == AST_GENERICTYPE);
    }
    if (typesOK && (A->kind == AST_GENERICTYPE || B->kind == AST_GENERICTYPE)) {
        return 1;
    }
    if (A->kind != B->kind) return 0;
    return SameTypes(A->left, B->left);
}

//
// Do some simple optimizations on an expr
// this is mainly used to tidy up internally generated
// expressions
//   (A+N)-N => A
// etc.
// The transformations done here are deliberately restricted
// because they can be applied to source code
//
AST *SimpleOptimizeExpr(AST *expr)
{
    AST *lhs = expr->left;
    AST *rhs = expr->right;
    if (expr->kind == AST_OPERATOR) {
        int op = expr->d.ival;
        if (op == K_NEGATE) {
            //   -(-x) => x
            if (rhs->kind == AST_OPERATOR && rhs->d.ival == K_NEGATE) {
                return rhs->right;
            }
        } else if (op == '+' || op == '-') {
            int oppositeOp = (op == '+') ? '-' : '+';
            // (A+N)-N => A, similarly for (A-N)+N
            if (lhs->kind == AST_OPERATOR && lhs->d.ival == oppositeOp) {
                if (IsConstExpr(rhs) && AstMatch(lhs->right, rhs)) {
                    return lhs->left;
                }
            } else if (rhs->kind == AST_INTEGER && rhs->d.ival == 0) {
                return lhs;
            }
        }
        // optimize integer expressions
        if (lhs->kind == AST_INTEGER && rhs->kind == AST_INTEGER) {
            return AstInteger(EvalConstExpr(expr));
        }
    }
    return expr;
}

/*
 * check an expression for side effects
 */
int
ExprHasSideEffects(AST *expr)
{
    if (!expr) return 0;
    switch (expr->kind) {
    case AST_ASSIGN:
    case AST_FUNCCALL:
    case AST_POSTSET:
        return 1;
    case AST_OPERATOR:
        switch(expr->d.ival) {
        case K_INCREMENT:
        case K_DECREMENT:
            return 1;
        default:
            break;
        }
        break;
    case AST_IDENTIFIER:
        return 0;
    case AST_INTEGER:
    case AST_STRING:
    case AST_FLOAT:
        return 0;
    default:
        break;
    }
    return ExprHasSideEffects(expr->left) || ExprHasSideEffects(expr->right);
}

bool
IsStringConst(AST *expr)
{
    AST *val;
    if (!expr) return 1;
    if (expr->kind == AST_STRINGPTR) {
        expr = expr->left;
        if (expr->kind == AST_STRING) {
            return true;
        } else if (expr->kind == AST_EXPRLIST) {
            // walk the string list
            while (expr) {
                val = expr->left;
                if (!val) return false;
                if (val->kind != AST_STRING
                    && !IsConstExpr(val))
                {
                    return false;
                }
                expr = expr->right;
            }
            return true;
        }
    } else if (expr->kind == AST_STRING) {
        /* hmmm, funny thing: is "A" a string or the integer 65? */
        /* we'll go with the standard Spin interpretation of the latter */
        return false;
    }
    return false;
}
