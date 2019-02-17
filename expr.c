/*
 * Spin to C/C++ converter
 * Copyright 2011-2019 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling expressions
 */

#include "spinc.h"
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* get an identifier name, or find the root name if an array reference */

const char *
GetVarNameForError(AST *expr)
{
    while (expr && expr->kind == AST_ARRAYREF) {
        expr = expr->left;
    }
    if (expr->kind == AST_IDENTIFIER) {
        return expr->d.string;
    }
    return "variable";
}
        
/* get the identifier name from an identifier */
const char *
GetIdentifierName(AST *expr)
{
    const char *name;
    if (expr->kind == AST_IDENTIFIER) {
        name = expr->d.string;
    } else {
        name = "expression";
    }
    return name;
}

/* get class from object type */
Module *
GetClassPtr(AST *objtype)
{
    objtype = BaseType(objtype);
    if (objtype->kind != AST_OBJECT) {
        ERROR(objtype, "internal error, not an object symbol");
        return NULL;
    }
    return (Module *)objtype->d.ptr;
}
/* get object type from module */
AST *
ClassType(Module *P)
{
    if (!P) {
        return NULL;
    }
    if (!P->type) {
        AST *ast;
        ast = NewAST(AST_OBJECT, NULL, NULL);
        ast->d.ptr = (void *)P;
        P->type = ast;
    }
    return P->type;
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
 * if there's no message pointer, do not print a message
 */
Symbol *
LookupAstSymbol(AST *ast, const char *msg)
{
    Symbol *sym = NULL;
    AST *id;
    const char *name = "";
    
    if (ast->kind == AST_SYMBOL) {
        return (Symbol *)ast->d.ptr;
    }
    if (ast->kind == AST_IDENTIFIER) {
        id = ast;
    } else if (ast->kind == AST_ARRAYREF) {
        id = ast->left;
    } else {
        //ERROR(ast, "internal error, bad id passed to LookupAstSymbol");
        return NULL;
    }
    if (id->kind == AST_IDENTIFIER || id->kind == AST_SYMBOL) {
        name = GetIdentifierName(id);
        sym = LookupSymbol(name);
    }
    if (!sym && msg) {
        ERROR(id, "unknown identifier %s used in %s", name, msg);
    }
    return sym;
}

/*
 * look up an object member given the object type
 */
Symbol *
LookupMemberSymbol(AST *expr, AST *objtype, const char *name, Module **Ptr)
{
    Module *P;
    Symbol *sym;
    objtype = BaseType(objtype);

    if (!objtype) {
        // if a constant ref, look for the object in the current modules list of
        // objects; this allows Spin to use constants before they are declared
        if (expr->kind == AST_CONSTREF) {
            AST *objident = expr->left;
            while (objident && objident->kind != AST_IDENTIFIER) {
                objident = objident->left;
            }
            if (objident) {
                AST *a;
                a = current->objblock;
                while (a) {
                    if (a->kind == AST_OBJECT) {
                        if (AstUses(a->left, objident)) {
                            objtype = a;
                            break;
                        }
                    }
                    a = a->right;
                }
            }
        }
    }
    if (!objtype || objtype->kind != AST_OBJECT) {
        ERROR(expr, "request for member %s in something not an object", name);
        return NULL;
    }
    P = (Module *)objtype->d.ptr;
    if (Ptr) {
        *Ptr = P;
    }
    sym = FindSymbol(&P->objsyms, name);
    if (!sym) {
        ERROR(expr, "unknown identifier %s in class %s", name, P->classname);
    }
    return sym;
}

/*
 * look up a symbol from a method reference
 * "expr" is the pointer to the method ref
 * if "Ptr" is nonzero it is used to return the
 * object referred to
 */
Symbol *
LookupMethodRef(AST *expr, Module **Ptr)
{
    AST *type = ExprType(expr->left);
    AST *ident = expr->right;
    const char *name;
    
    name = GetIdentifierName(ident);
    return LookupMemberSymbol(expr, type, name, Ptr);
    
}

/*
 * look up an object method or constant
 * "lhs" is the left hand side of the object reference
 */
Symbol *
LookupObjrefSymbol(AST *lhs, const char *name)
{
    AST *objtype = ExprType(lhs);
    if (!IsClassType(objtype)) {
        ERROR(lhs, "Expected class type for looking up %s", name);
        return NULL;
    }
    return LookupMemberSymbol(lhs, objtype, name, NULL);
}

/*
 * look up the class name of an object
 */
const char *
ObjClassName(AST *objtype)
{
    Module *P;
    if (!IsClassType(objtype)) {
        ERROR(objtype, "expected an object");
        return NULL;
    }
    P = GetClassPtr(objtype);
    return P->classname;
}

const char *
TypeName(AST *type)
{
    if (type->kind == AST_OBJECT) {
        Module *P = (Module *)type->d.ptr;
        return P->classname;
    }
    return "that type";
}

/* code to check if a coginit invocation is for a spin method */
/* if it is, returns a pointer to the method doing the invocation */
bool
IsSpinCoginit(AST *params, Function **methodptr)
{
    AST *exprlist, *func;
    Symbol *sym = NULL;

    if (methodptr) {
        *methodptr = NULL;
    }
    if (!params || !params->left || params->kind != AST_COGINIT) {
        return false;
    }
    exprlist = params->left;
    exprlist = exprlist->right; // skip over cog id
    if (exprlist->kind != AST_EXPRLIST || !exprlist->left) {
        ERROR(params, "coginit/cognew expected expression");
        return false;
    }
    func = exprlist->left;
    if (func->kind == AST_IDENTIFIER) {
        sym = LookupAstSymbol(func, "coginit/cognew");
        if (sym && sym->type == SYM_FUNCTION) {
            if (methodptr) *methodptr = (Function *)sym->val;
            return true;
        }
    }
    if (func->kind == AST_FUNCCALL) {
        /* FIXME? Spin requires that it be a local method; do we care? */
        sym = FindFuncSymbol(func, NULL, 1);
        if (sym) {
            if (sym->type == SYM_BUILTIN) {
                return false;
            }
            if (sym->type == SYM_FUNCTION) {
                if (methodptr) {
                    *methodptr = (Function *)sym->val;
                }
                return true;
            }
            // hmmm, interesting situation here; we've got an indirect
            // call via a variable
            if (sym->type == SYM_LABEL) {
                return false;
            }
            return true;
        }
    }
    return false;
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
    if (expr->kind == AST_IDENTIFIER || expr->kind == AST_SYMBOL)
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
    exprvar = AstTempLocalVariable(prefix, NULL);
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
    AST *hwreg = NULL;
    ASTReportInfo saveinfo;
    AST *result = NULL;
    
    AstReportAs(dst, &saveinfo);
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
                hwreg = dst->left;
                if (hwreg->kind == AST_HWREG) {
                    result = AstAssign(hwreg,
                                     AstOperator('^', hwreg, maskexpr));
                    AstReportDone(&saveinfo);
                    return result;
                } else {
                    ERROR(hwreg, "unable to handle reg pair");
                }
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
    result = AstAssign(dst->left,
                     AstOperator('^', dst->left, maskexpr));
    AstReportDone(&saveinfo);
    return result;
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
    AST *result;
    ASTReportInfo saveinfo;
    
    AstReportAs(dst, &saveinfo);
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
        result = AstAssign(dst->left,
                         AstOperator('|', dst->left, maskexpr));
    } else {
        maskexpr = AstOperator(K_BIT_NOT, NULL, maskexpr);
        result = AstAssign(dst->left,
                         AstOperator('&', dst->left, maskexpr));
    }
    AstReportDone(&saveinfo);
    return result;
}

/*
 * transform an assignment chain
 *  A := B := C
 * => SEQ(T:=C, B:=T, A:=T)
 * this is necessary in cases where A, B, or C is a range expression
 * otherwise the returned value will incorrectly not be a range expression
 */
AST *
TransformAssignChain(AST *dst, AST *src)
{
    AST *seq = NULL;
    AST *final;
    AST *temp;
    ASTReportInfo saveinfo;

    if (!src) {
        ERROR(dst, "Internal error in assign chain");
        return src;
    }
    AstReportAs(src, &saveinfo);
    switch (src->kind) {
    case AST_ASSIGN:
        seq = TransformAssignChain(src->left, src->right);
        final = seq->left;
        if (final->kind == AST_ASSIGN) {
            final = final->left;
        }
        break;
    case AST_INTEGER:
        seq = NewAST(AST_SEQUENCE, src, NULL);
        final = src;
        break;
    default:
        temp = AstTempLocalVariable("_temp", ast_type_unsigned_long);
        seq = NewAST(AST_SEQUENCE, AstAssign(temp, src), NULL);
        final = temp;
        break;
    }
    seq = AddToList(seq, NewAST(AST_SEQUENCE, AstAssign(dst, final), NULL));
    AstReportDone(&saveinfo);
    return seq;
}

/*
 * special code for printing a range expression
 * dst->left is the hardware register; or, it may be an AST_REGPAIR
 *      holding two paired registers
 * dst->right is an AST_RANGE with left being the start, right the end
 *
 * outa[2..1] := foo
 * should evaluate to:
 *   _OUTA = (_OUTA & ~(0x3<<1)) | (foo<<1);
 *
 * Note that we want to special case some common idioms:
 *  outa[2..1] := outa[2..1] ^ -1, for example
 *
 * toplevel is "1" if we are at top level and can emit if statements
 *
 * if we have an AST_REGPAIR then we need to output two sets of assignments
 * and make them conditional
 */
AST *
TransformRangeAssign(AST *dst, AST *src, int toplevel)
{
    AST *nbits;
    AST *loexpr;
    AST *revsrc;
    AST *maskexpr;
    AST *hwreg;
    AST *hwreg2 = NULL;
    ASTReportInfo saveinfo;
    AST *result;
    
    if (!dst) return dst;
    if (!dst->right || dst->right->kind != AST_RANGE) {
        ERROR(dst, "internal error: expecting range");
        return 0;
    }
    if (src && src->kind == AST_ASSIGN) {
        // watch out for assignment chains
        result = TransformAssignChain(dst, src);
        return result;
    }
    AstReportAs(dst, &saveinfo);  // set up error messages as if coming from "dst"

    hwreg = dst->left;
    if (hwreg->kind == AST_REGPAIR) {
        AST *assign, *assign2;
        AST *cond;
        hwreg2 = hwreg->right;
        hwreg = hwreg->left;

        hwreg = NewAST(AST_RANGEREF, hwreg, dst->right);
        hwreg2 = NewAST(AST_RANGEREF, hwreg2, dst->right);
        
        assign = TransformRangeAssign(hwreg, src, toplevel);
        assign2 = TransformRangeAssign(hwreg2, src, toplevel);
        if (dst->right->right && IsConstExpr(dst->right->right)) {
            cond = AstOperator(K_LTU, dst->right->right, AstInteger(32));
        } else {
            cond = AstOperator(K_LTU, dst->right->left, AstInteger(32));
        }
        if (IsConstExpr(cond)) {
            int x = EvalConstExpr(cond);
            AstReportDone(&saveinfo);
            if (x) return assign;
            return assign2;
        } 
        // here we don't know which set of pins to use, so
        // we have to generate an IF statement
        if (!toplevel) {
            ERROR(dst, "cannot handle unknown pins yet except at top level");
            AstReportDone(&saveinfo);
            return assign;
        }
        // wrap the sides of the IF in statement lists
        assign = NewAST(AST_STMTLIST, assign, NULL);
        assign2 = NewAST(AST_STMTLIST, assign2, NULL);
        assign = NewAST(AST_IF, cond, NewAST(AST_THENELSE, assign, assign2));
        assign = NewAST(AST_STMTLIST, assign, NULL);
        AstReportDone(&saveinfo);
        return assign;
    } else if (hwreg->kind == AST_HWREG) {
        // OK
    } else {
        ERROR(dst, "internal error in range assign");
        AstReportDone(&saveinfo);
        return NULL;
    }
    /* special case logical operators */

    /* doing a NOT on the whole thing */
    if (src->kind == AST_OPERATOR && src->d.ival == K_BIT_NOT
        && AstMatch(dst, src->right))
    {
        result = RangeXor(dst, AstInteger(0xffffffff));
        AstReportDone(&saveinfo);
        return result;
    }
    /* now handle the ordinary case */
    /* dst->right is the range of bits we're setting */
    if (dst->right->right == NULL) {
        /* just a single item in dst->right->left */
        nbits = AstInteger(1);
        loexpr = dst->right->left;
        /* special case flipping a bit */
        if (src->kind == AST_OPERATOR && src->d.ival == '^'
            && AstMatch(dst, src->left)
            && IsConstExpr(src->right)
            && EvalConstExpr(src->right) == 1)
        {
            result = RangeXor(dst, AstInteger(0xffffffff));
            AstReportDone(&saveinfo);
            return result;
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
            result = RangeBitSet(dst, mask, bitset);
            AstReportDone(&saveinfo);
            return result;
        }
    }
    if (IsConstExpr(nbits) && EvalConstExpr(nbits) >= 32) {
        result = AstAssign(dst->left, FoldIfConst(src));
        AstReportDone(&saveinfo);
        return result;
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

        maskvar = AstTempLocalVariable("_mask", ast_type_unsigned_long);
        shift = AstOperator(K_SHL, AstInteger(1), loexpr);
        maskassign = AstAssign(maskvar, shift);
        maskassign = NewAST(AST_STMTLIST, maskassign, NULL);
        // insert the mask assignment at the beginning of the function
        maskassign->right = curfunc->body;
        curfunc->body = maskassign;

        /* dst->left is the destination assignment */
        /* it may be an AST_LISTHOLDER, in which case it
           represents a pair of registers */
        
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
        AstReportDone(&saveinfo);
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

        andexpr = AstOperator(K_SHL, maskexpr, loexpr);
        andexpr = AstOperator(K_BIT_NOT, NULL, andexpr);
        andexpr = FoldIfConst(andexpr);

        orexpr = FoldIfConst(AstOperator('&', src, maskexpr));
        orexpr = AstOperator(K_SHL, orexpr, loexpr);
        orexpr = FoldIfConst(orexpr);
        
        orexpr = NewAST(AST_MASKMOVE, dst->left, AstOperator('|', andexpr, orexpr));
        result = AstAssign(dst->left, orexpr);
        AstReportDone(&saveinfo);
        return result;
    }
}

/*
 * print a range use
 * src->right should be an AST_RANGE
 * src->left is a hardware register
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
    AST *range;
    ASTReportInfo saveinfo;
    
    if (!curfunc) {
        ERROR(src, "Internal error, could not find function");
        return AstInteger(0);
    }
    if (!src) return src;
    if (src->kind != AST_RANGEREF) {
        ERROR(src, "internal error in rangeref");
        return src;
    }
    range = src->right;
    if (range->kind != AST_RANGE) {
        ERROR(src, "internal error: expecting range");
        return src;
    }
    if (src->left->kind == AST_REGPAIR) {
        AST *rega, *regb;
        AST *cond;
        // create new range references like outa[range], outb[range]
        rega = NewAST(AST_RANGEREF, src->left->left, src->right);
        regb = NewAST(AST_RANGEREF, src->left->right, src->right);
        // and dereference them
        rega = TransformRangeUse(rega);
        regb = TransformRangeUse(regb);
        if (range->right && IsConstExpr(range->right)) {
            cond = AstOperator(K_LTU, range->right, AstInteger(32));
        } else {
            cond = AstOperator(K_LTU, range->left, AstInteger(32));
        }
        if (IsConstExpr(cond)) {
            int x = EvalConstExpr(cond);
            if (x) return rega;
            return regb;
        }
        return NewAST(AST_CONDRESULT,
                      cond,
                      NewAST(AST_THENELSE, rega, regb));
    }
    if (src->left->kind != AST_HWREG) {
        ERROR(src, "range not applied to hardware register");
        return AstInteger(0);
    }
    AstReportAs(src, &saveinfo);
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
    AstReportDone(&saveinfo);
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
    case K_LTU:
        return intAsFloat(-(lval < rval));
    case '>':
    case K_GTU:
        return intAsFloat(-(lval > rval));
    case K_LE:
    case K_LEU:
        return intAsFloat(-(lval <= rval));
    case K_GE:
    case K_GEU:
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
EvalFixedOperator(int op, int32_t lval, int32_t rval, int *valid)
{
    
    switch (op) {
    case '+':
        return lval + rval;
    case '-':
        return lval - rval;
    case '/':
        return (int32_t)( (1<<G_FIXPOINT) * (float)lval / (float)rval);
    case '*':
        return (lval * (int64_t)rval) >> G_FIXPOINT;
    case '|':
        return lval | rval;
    case '&':
        return lval & rval;
    case '^':
        return lval ^ rval;
    case K_HIGHMULT:
        return (lval*(int64_t)rval) >> (G_FIXPOINT + 32);
    case K_SHL:
        return lval << (rval >> G_FIXPOINT);
    case K_SHR:
        return ((uint32_t)lval) >> (rval >> G_FIXPOINT);
    case K_SAR:
        return lval >> (rval >> G_FIXPOINT);
    case '<':
        return (-(lval < rval));
    case '>':
        return (-(lval > rval));
    case K_LE:
        return (-(lval <= rval));
    case K_GE:
        return (-(lval >= rval));
    case K_LTU:
        return (-((uint32_t)lval < (uint32_t)rval));
    case K_GTU:
        return (-((uint32_t)lval > (uint32_t)rval));
    case K_LEU:
        return (-((uint32_t)lval <= (uint32_t)rval));
    case K_GEU:
        return (-((uint32_t)lval >= (uint32_t)rval));
    case K_NE:
        return (-(lval != rval));
    case K_EQ:
        return (-(lval == rval));
    case K_NEGATE:
        return -rval;
    case K_ABS:
        return (rval < 0) ? -rval : rval;
    case K_SQRT:
        rval = ((1<<G_FIXPOINT)*sqrtf((float)rval / (1<<G_FIXPOINT)));
        return rval;
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
        return lval << (rval & 0x1f);
    case K_SHR:
        return ((uint32_t)lval) >> (rval & 0x1f);
    case K_SAR:
        return ((int32_t)lval) >> (rval & 0x1f);
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
    case K_LTU:
        return (-((uint32_t)lval < (uint32_t)rval));
    case K_GTU:
        return (-((uint32_t)lval > (uint32_t)rval));
    case K_LEU:
        return (-((uint32_t)lval <= (uint32_t)rval));
    case K_GEU:
        return (-((uint32_t)lval >= (uint32_t)rval));
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
convToFloat(ExprVal intval)
{
    if (gl_fixedreal) {
        return fixedExpr((intval.val)<<G_FIXPOINT);
    } else {
        return floatExpr((float)intval.val);
    }
}

static ExprVal
EvalOperator(int op, ExprVal left, ExprVal right, int *valid)
{
    ExprVal le = left;
    ExprVal re = right;
    int isFloat = 0;

    if (IsStringType(le.type)) {
        *valid = 0;
        return intExpr(0);
    }
    if (IsFloatType(le.type)) {
        if (!IsFloatType(re.type)) {
            re = convToFloat(re);
        }
        isFloat = 1;
    } else if (IsFloatType(re.type)) {
        le = convToFloat(le);
        isFloat = 1;
    }
    if (isFloat) {
        if (gl_fixedreal) {
            return fixedExpr(EvalFixedOperator(op, le.val, re.val, valid));
        } else {
            return floatExpr(EvalFloatOperator(op, intAsFloat(le.val), intAsFloat(re.val), valid));
        }
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
    Symbol *sym;
    ExprVal lval, rval;
    ExprVal aval;
    int reportError = (valid == NULL);
    ExprVal ret;
    int kind;
    const char *name;
    AST *offsetExpr = NULL;
    unsigned long offset;
    
    if (!expr)
        return intExpr(0);
    if (depth > MAX_DEPTH) {
        ERROR(expr, "Expression too complicated or symbol definition loop");
        return intExpr(0);
    }

    kind = expr->kind;
    switch (kind) {
    case AST_INTEGER:
    case AST_BITVALUE:
        return intExpr(expr->d.ival);
    case AST_SIZEOF:
        return intExpr(TypeSize(ExprType(expr->left)));
    case AST_FLOAT:
        if (gl_fixedreal) {
            return fixedExpr(expr->d.ival);
        } else {
            return floatExpr(intAsFloat(expr->d.ival));
        }

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
        return convToFloat(lval);
    case AST_TRUNC:
        lval = EvalExpr(expr->left, flags, valid, depth+1);
        if (!IsFloatType(lval.type)) {
            ERROR(expr, "applying trunc to a non float expression");
        }
        if (gl_fixedreal) {
            return intExpr(lval.val >> G_FIXPOINT);
        } else {
            return intExpr((int)intAsFloat(lval.val));
        }
    case AST_ROUND:
        lval = EvalExpr(expr->left, flags, valid, depth+1);
        if (!IsFloatType(lval.type)) {
            ERROR(expr, "applying round to a non float expression");
        }
        if (gl_fixedreal) {
            return intExpr( (lval.val + (1<<(G_FIXPOINT-1))) >> G_FIXPOINT);
        } else {
            return intExpr((int)roundf(intAsFloat(lval.val)));
        }
    case AST_CONSTANT:
        return EvalExpr(expr->left, flags, valid, depth+1);
    case AST_CONSTREF:
    case AST_METHODREF:
    {
        Module *P;
        sym = LookupMethodRef(expr, &P);
        if (!sym) {
            return intExpr(0);
        }
        if ((sym->type != SYM_CONSTANT && sym->type != SYM_FLOAT_CONSTANT)) {
            if (valid) {
                *valid = 0;
            } else {
                ERROR(expr, "%s is not a constant of %s", GetIdentifierName(expr->right), P->classname);
            }
            return intExpr(0);
        }
        /* while we're evaluating, use the object context */
        ret = EvalExprInState(P, (AST *)sym->val, flags, valid, depth+1);
        return ret;
    }
    case AST_RESULT:
        *valid = 0;
        return intExpr(0);
    case AST_SYMBOL:
    case AST_IDENTIFIER:
        if (expr->kind == AST_SYMBOL) {
            sym = (Symbol *)expr->d.ptr;
            name = sym->name;
        } else {
            name = expr->d.string;
            sym = LookupSymbol(name);
        }
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
                if (gl_fixedreal) {
                    return fixedExpr(e.val);
                } else {
                    return floatExpr(intAsFloat(e.val));
                }
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
                    ERROR(expr, "Symbol %s is not constant", name);
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
        offset = 0;
        if (expr->kind == AST_ARRAYREF) {
            offsetExpr = expr->right;
            rval = EvalExpr(offsetExpr, flags, valid, depth+1);
            if (valid && !*valid)  {
                return rval;
            }
            expr = expr->left;
            offset = rval.val;
        }
        if (expr->kind != AST_IDENTIFIER && expr->kind != AST_SYMBOL) {
            if (reportError)
                ERROR(expr, "Only addresses of identifiers allowed");
            else
                *valid = 0;
            return intExpr(0);
        }
        if (expr->kind == AST_SYMBOL) {
            sym = (Symbol *)expr->d.ptr;
        } else {
            sym = LookupSymbol(expr->d.string);
        }
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
            if (offset) {
                offset *= TypeSize(BaseType(lref->type));
            }
            if (gl_p2) {
                return intExpr(lref->hubval + offset);
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
                    offset += gl_dat_offset;
                    return intExpr(lref->hubval + offset);
                }
            }
            return intExpr(lref->hubval + offset);
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

ExprVal fixedExpr(int32_t f)
{
    ExprVal e;
    e.type = ast_type_float;
    e.val = f;
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
AST *
RemoveTypeModifiers(AST *ast)
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
    ast = RemoveTypeModifiers(ast);
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
    case AST_FUNCTYPE:
    case AST_OBJECT:
    case AST_TUPLETYPE:
        return 0;
    default:
        ERROR(ast, "Internal error: unknown type %d passed to IsArrayType",
              ast->kind);
    }
    return 0;
}

/* find the base of an array, or NULL for default (0) */
AST *
GetArrayBase(AST *arraytype)
{
    arraytype = RemoveTypeModifiers(arraytype);
    if (arraytype->kind == AST_ARRAYTYPE) {
        return arraytype->d.ptr;
    }
    return NULL;
}

/* find minimum alignment for a type */
int TypeAlign(AST *typ)
{
    typ = RemoveTypeModifiers(typ);
    while (typ && typ->kind == AST_ARRAYTYPE) {
        typ = RemoveTypeModifiers(typ->left);
    }
    if (!typ) {
        return 4;
    }
    switch (typ->kind) {
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
        return EvalConstExpr(typ->left);
    case AST_PTRTYPE:
    case AST_FUNCTYPE:
    default:
        return 4; // all pointers are 4 bytes
    case AST_OBJECT:
        {
            Module *P = (Module *)typ->d.ptr;
            if (P->varsize < 3) {
                return P->varsize;
            }
            return 4;
        }
    }
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
        if (!IsConstExpr(typ->right)) {
            ERROR(typ, "Unable to determine size of array");
            size = 1;
        } else {
            size = EvalConstExpr(typ->right);
        }
        return size * TypeSize(typ->left);
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
        return EvalConstExpr(typ->left);
    case AST_PTRTYPE:
    case AST_FUNCTYPE:
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
    case AST_VOIDTYPE:
    {
        return 1;
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
    typ = RemoveTypeModifiers(typ);
    if (!typ) return typ;
    switch (typ->kind) {
    case AST_ARRAYTYPE:
        return typ->left;
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
    case SYM_LABEL:
        return 1;
    default:
        return 0;
    }
    return IsArrayType(type) || IsPointerType(type);
}

/* find function symbol in a function call */
/* if errflag is nonzero, print errors for identifiers not found */
Symbol *
FindFuncSymbol(AST *ast, AST **objrefPtr, int errflag)
{
    AST *objref = NULL;
    Symbol *sym = NULL;
    AST *expr = ast;
    
    if (expr->kind != AST_METHODREF) {       
        if (expr->kind != AST_FUNCCALL && expr->kind != AST_ADDROF && expr->kind != AST_ABSADDROF) {
            ERROR(expr, "Internal error expecting function call");
            return NULL;
        }
        expr = expr->left;
    }
    while (expr && expr->kind == AST_ARRAYREF) {
        expr = expr->left;
    }
    if (expr && expr->kind == AST_MEMREF) {
        expr = expr->right;
    }
    if (expr && expr->kind == AST_METHODREF) {
        const char *thename;
        Function *f;
        AST *objtype;
        objref = expr->left;
        objtype = ExprType(objref);
        thename = GetIdentifierName(expr->right);
        if (!thename) {
            return NULL;
        }
        if (!IsClassType(objtype)) {
            if (errflag) {
                ERROR(ast, "request for member %s in something that is not an object", thename);
            }
            return NULL;
        }
        sym = LookupMemberSymbol(objref, objtype, thename, NULL);
        if (sym && sym->type == SYM_FUNCTION) {
            f = (Function *)sym->val;
            if (!f->is_public) {
                ERROR(ast, "%s is a private method", thename);
            }
        }
    } else {
        sym = LookupAstSymbol(expr, errflag ? "function call" : NULL);
    }
    if (objrefPtr) *objrefPtr = objref;
    return sym;
}

/* find called function symbol in a function call */
/* if errflag is nonzero, print errors for identifiers not found */
Symbol *
FindCalledFuncSymbol(AST *ast, AST **objrefPtr, int errflag)
{
#if 1
    return FindFuncSymbol(ast, objrefPtr, errflag);
#else    
    AST *objref = NULL;
    AST *objtype = NULL;
    Symbol *sym = NULL;
    AST *expr = ast;
    
    if (expr->kind != AST_METHODREF) {       
        if (expr->kind != AST_FUNCCALL && expr->kind != AST_ADDROF) {
            ERROR(expr, "Internal error expecting function call");
            return NULL;
        }
        expr = expr->left;
    }
        
    if (expr && expr->kind == AST_METHODREF) {
        const char *thename;
        Function *f;
        Module *P;
        
        objref = expr->left;
        objtype = ExprType(objref);

        if (!objtype || BaseType(objtype)->kind != AST_OBJECT) {
            if (errflag)
                ERROR(ast, "member dereference to non-object");
            return NULL;
        }
        P = GetClassPtr(objtype);
        thename = expr->right->d.string;
        sym = FindSymbol(&P->objsyms, thename);
        if (!sym || sym->type != SYM_FUNCTION) {
            if (errflag)
                ERROR(ast, "%s is not a method of %s", thename, P->classname);
            return NULL;
        }
        f = (Function *)sym->val;
        if (!f->is_public) {
            ERROR(ast, "%s is a private method of %s", thename, P->classname);
        }
    } else {
        sym = LookupAstSymbol(expr, errflag ? "function call" : NULL);
    }
    if (objrefPtr) *objrefPtr = objref;
    return sym;
#endif    
}

int
IsFloatType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_FLOATTYPE)
        return 1;
    return 0;
}

int
IsClassType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_OBJECT) {
        return 1;
    }
    return 0;
}

int
IsPointerType(AST *type)
{
    type = RemoveTypeModifiers(type);
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
IsVoidType(AST *type)
{
    return type && type->kind == AST_VOIDTYPE;
}

int
IsGenericType(AST *type)
{
    type = RemoveTypeModifiers(type);
    return type && type->kind == AST_GENERICTYPE;
}

int
IsIntType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_INTTYPE || type->kind == AST_UNSIGNEDTYPE)
        return 1;
    return 0;
}

int
IsUnsignedType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_UNSIGNEDTYPE)
        return 1;
    return 0;
}

int
IsBoolCompatibleType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 1;
    switch (type->kind) {
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_PTRTYPE:
        return 1;
    default:
        return 0;
    }
}

// also returns TRUE for pointers to functions,
// which may be implicitly dereferenced
int
IsFunctionType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_PTRTYPE) {
        type = RemoveTypeModifiers(type->left);
    }
    if (type && type->kind == AST_FUNCTYPE)
        return 1;
    return 0;
}

int
IsStringType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type == ast_type_string)
        return 1;
    return 0;
}

int
IsNumericType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_INTTYPE || type->kind == AST_UNSIGNEDTYPE || type->kind == AST_FLOATTYPE)
        return 1;
    return 0;
}

int
IsConstType(AST *type)
{
    return type && type->kind == AST_MODIFIER_CONST;
}

//
// find the wider of the numeric types "left" and "right"
// if one type is unsigned and the other is signed, return ast_type_long
//
AST *
WidestType(AST *left, AST *right)
{
    int lsize, rsize;
    left = RemoveTypeModifiers(left);
    right = RemoveTypeModifiers(right);
    if (!left) return right;
    if (!right) return left;
    if (left->kind != right->kind) {
        return ast_type_long;
    }
    lsize = TypeSize(left);
    rsize = TypeSize(right);
    if (lsize < rsize) return right;
    return left;
}

// for historical reasons SetFunctionReturnType is in functions.c
AST *
GetFunctionReturnType(Function *f)
{
    if (!f->overalltype)
        return NULL;
    return f->overalltype->left;
}

AST *
ExprType(AST *expr)
{
    SymbolTable *table;
    Module *P;

    if (!expr) return NULL;
    if (curfunc) {
        table = &curfunc->localsyms;
        P = curfunc->module;
    } else {
        P = current;
        table = &current->objsyms;
    }
    return ExprTypeRelative(table, expr, P);
}

/*
 * figure out an expression's type
 * returns NULL if we can't deduce it
 */
AST *
ExprTypeRelative(SymbolTable *table, AST *expr, Module *P)
{
    AST *sub;

    if (!expr) return NULL;
    if (!P) {
        P = current;
    }
    switch (expr->kind) {
    case AST_INTEGER:
    case AST_CONSTANT:
    case AST_CONSTREF:
    case AST_HWREG:
    case AST_ISBETWEEN:
    case AST_COGINIT:
        return ast_type_long;
    case AST_ALLOCA:
        return expr->left;
    case AST_BITVALUE:
    case AST_CATCHRESULT:
        return ast_type_generic;
    case AST_FLOAT:
    case AST_TRUNC:
    case AST_ROUND:
        return ast_type_float;
    case AST_SELF:
        return NewAST(AST_PTRTYPE, ClassType(P), NULL);
    case AST_SUPER:
        return NewAST(AST_PTRTYPE, ClassType(P->superclass), NULL);        
    case AST_STRING:
        // in Spin, a string is always dereferenced
        // so "abc" is the same as "a" is the same as 0x65
        // (actually no -- "abc" is the same as "a", "b", "c")
        if (curfunc->language == LANG_SPIN) {
            return ast_type_long;
        }
        /* otherwise fall through */
    case AST_STRINGPTR:
        return ast_type_string;
    case AST_MEMREF:
        return expr->left; 
    case AST_ADDROF:
    case AST_ABSADDROF:
        /* if expr->right is not NULL, then it's the final type */
        if (expr->right) {
            AST *type = expr->right;
            if (type->kind == AST_FUNCTYPE) {
                type = NewAST(AST_PTRTYPE, type, NULL);
            }
            return type;
        } else {
            sub = ExprTypeRelative(table, expr->left, P);
        }
        if (!sub) sub= ast_type_generic;
        return NewAST(AST_PTRTYPE, sub, NULL);
    case AST_IDENTIFIER:
    {
        Symbol *sym = LookupSymbolInTable(table, expr->d.string);
        Label *lab;
        AST *typ;
        if (!sym) return NULL;
        switch (sym->type) {
        case SYM_CONSTANT:
        case SYM_HW_REG:
            return ast_type_long;
        case SYM_LABEL:
            lab = (Label *)sym->val;
            typ = lab->type;
            if (curfunc && curfunc->language == LANG_SPIN && typ && typ->kind != AST_ARRAYTYPE) {
                return NewAST(AST_ARRAYTYPE, typ, AstInteger(1));
            }
            return typ;
        case SYM_FLOAT_CONSTANT:
            return ast_type_float;
        case SYM_VARIABLE:
        case SYM_LOCALVAR:
        case SYM_PARAMETER:
        case SYM_CLOSURE:
        case SYM_TYPEDEF:
            return (AST *)sym->val;
        case SYM_FUNCTION:
            return ((Function *)sym->val)->overalltype;
        default:
            return NULL;
        }            
    }
    case AST_ARRAYREF:
        sub = ExprTypeRelative(table, expr->left, P);
        if (!sub) return NULL;
        if (sub->kind == AST_OBJECT) {
            // HACK for now: object arrays are declared funny
            return sub;
        }
        if (expr->left->kind == AST_MEMREF) {
            return sub;
        }
        if (!(sub->kind == AST_PTRTYPE || sub->kind == AST_ARRAYTYPE)) return NULL;
        return sub->left;
    case AST_FUNCCALL:
    {
        Symbol *sym = FindFuncSymbol(expr, NULL, 0);
        AST *typexpr;
        if (sym) {
            switch (sym->type) {
            case SYM_FUNCTION:
                return GetFunctionReturnType(((Function *)sym->val));
            case SYM_VARIABLE:
            case SYM_PARAMETER:
            case SYM_LOCALVAR:
            case SYM_TEMPVAR:
                typexpr = sym->val;
                if (typexpr && typexpr->kind == AST_PTRTYPE) {
                    typexpr = RemoveTypeModifiers(typexpr->left);
                }
                if (typexpr && typexpr->kind == AST_FUNCTYPE) {
                    return typexpr->left;
                }
                ERROR(expr, "Object called is not a function");
                break;
            default:
                return NULL;
            }
        }
        return NULL;
    }
    case AST_DECLARE_VAR:
    case AST_DECLARE_VAR_WEAK:
        return expr->left;
    case AST_LAMBDA:
//        return expr->left;
        return NewAST(AST_PTRTYPE, expr->left, NULL);
    case AST_METHODREF:
    {
        AST *objref = expr->left;
        AST *objtype = NULL;
        Symbol *sym = NULL;
        const char *methodname;
        Function *func;
        
        if (expr->right->kind != AST_IDENTIFIER) {
            ERROR(expr, "Expecting identifier after '.'");
            return NULL;
        }
        methodname = expr->right->d.string;
        objtype = BaseType(ExprTypeRelative(table, objref, P));
        if (!objtype) return NULL;
        if (!IsClassType(objtype)) {
            ERROR(expr, "Expecting object for dereference of %s", methodname);
            return NULL;
        }
        sym = LookupMemberSymbol(expr, objtype, methodname, NULL);
        if (!sym) {
            ERROR(expr, "%s is not a member of %s", methodname, TypeName(objtype));
            return NULL;
        }
        switch (sym->type) {
        case SYM_FUNCTION:
            func = (Function *)sym->val;
            return func->overalltype;
        case SYM_VARIABLE:
            return (AST *)sym->val;
        case SYM_CONSTANT:
            return ExprTypeRelative(table, (AST *)sym->val, P);
        default:
            ERROR(expr, "Unable to handle member %s", methodname);
            return NULL;
        }
    }
    case AST_OPERATOR:
    {
        AST *ltype, *rtype;
        switch (expr->d.ival) {
        case '-':
        case '+':
        case K_INCREMENT:
        case K_DECREMENT:
            ltype = ExprTypeRelative(table, expr->left, P);
            rtype = ExprTypeRelative(table, expr->right, P);
            if (IsFloatType(ltype) || IsFloatType(rtype)) {
                return ast_type_float;
            }
            if (expr->d.ival == '+' && IsStringType(ltype)) {
                return ltype;
            }
            if (curfunc && curfunc->language == LANG_SPIN) {
                if (!ltype) ltype = rtype;
                if (ltype) {
                    if (IsIntOrGenericType(ltype)) return ltype;
                    if (IsPointerType(ltype)) {
                        if (PointerTypeIncrement(ltype) == 1) {
                            return ltype;
                        }
                        return ast_type_generic;
                    }
                }
                return WidestType(ltype, rtype);
            }
            if (IsPointerType(ltype) && IsPointerType(rtype)) {
                if (expr->d.ival == '-') {
                    return ast_type_long;
                }
                return ltype;
            } else if (IsPointerType(ltype) && (rtype == NULL || IsIntType(rtype))) {
                return ltype;
            } else if (IsPointerType(rtype) && (ltype == NULL || IsIntType(ltype))) {
                return rtype;
            }
            return WidestType(ltype, rtype);
        case K_NEGATE:
        case '*':
        case '/':
        case K_ABS:
        case K_SQRT:
            ltype = ExprTypeRelative(table, expr->left, P);
            rtype = ExprTypeRelative(table, expr->right, P);
            if (IsFloatType(ltype) || IsFloatType(rtype)) {
                return ast_type_float;
            }
            return WidestType(ltype, rtype);
        case K_ZEROEXTEND:
            return ast_type_unsigned_long;
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
        return ExprTypeRelative(table, expr, P);
    }
    case AST_NEW:
        return expr->left;
    case AST_SIZEOF:
        return ast_type_unsigned_long;
    case AST_CAST:
    case AST_VA_ARG:
        return expr->left;
    case AST_PTRTYPE:
    case AST_ARRAYTYPE:
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_MODIFIER_CONST:
    case AST_MODIFIER_VOLATILE:
    case AST_OBJECT:
        return expr;
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
/* NOTE: this is not symmetric; we allow A to be of stricter type
 * than B (so for example passing type B to type A is OK if B
 * is a "char *" and A is a "const char *", but not vice-versa)
 */
int
CompatibleTypes(AST *A, AST *B)
{
    bool typesOK = (curfunc != NULL && curfunc->language != LANG_SPIN);
    bool skipfloats = !typesOK;
    
    A = RemoveTypeModifiers(A);
    B = RemoveTypeModifiers(B);
    
    if (!A || (skipfloats && A->kind == AST_FLOATTYPE)) {
        A = ast_type_generic;
    }
    if (!B || (skipfloats && B->kind == AST_FLOATTYPE)) {
        B = ast_type_generic;
    }

    if (A == B) return 1;
    if (A->kind == AST_INTTYPE || A->kind == AST_UNSIGNEDTYPE || A->kind == AST_GENERICTYPE) {
        if (B->kind == AST_INTTYPE || B->kind == AST_UNSIGNEDTYPE || B->kind == AST_GENERICTYPE) {
            return true;
        }
    }
    if (typesOK && (A->kind == AST_GENERICTYPE || B->kind == AST_GENERICTYPE)) {
        return 1;
    }
    if (A->kind != B->kind) {
        return 0;
    }
    if (A->kind == AST_FUNCTYPE) {
        if (FuncNumResults(A) != FuncNumResults(B)) {
            return 0;
        }
        // the return types have to be compatible
        if (!CompatibleTypes(A->left, B->left)) {
            return 0;
        }
        // and the parameters have to be compatible
        if (AstListLen(A->right) != AstListLen(B->right)) {
            // different number of parameters, bad
            return 0;
        }
        // FIXME: deal with actual parameter checks later
        return 1;
    }
    if (A->kind == AST_PTRTYPE && B->kind == AST_PTRTYPE) {
        // if one side is a void * then they are compatible
        AST *typeA = A->left;
        AST *rawtypeA = RemoveTypeModifiers(typeA);
        AST *typeB = B->left;
        AST *rawtypeB;
        if (IsVoidType(rawtypeA))
            return 1;
        if (rawtypeA && typeB && CompatibleTypes(rawtypeA, typeB)
            && TypeSize(rawtypeA) == TypeSize(typeB))
        {
            return 1;
        }
        rawtypeB = RemoveTypeModifiers(typeB);
        if (IsVoidType(rawtypeB)) {
            return 1;
        }
    }
    // both A and B are pointers (or perhaps arrays)
    // they are compatible if they are both pointers to the same thing
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
    AST *lhs;
    AST *rhs;

    if (!expr) return expr;
    lhs = SimpleOptimizeExpr(expr->left);
    rhs = SimpleOptimizeExpr(expr->right);
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
        if (rhs->kind == AST_INTEGER) {
            if ( (rhs->d.ival == 0) && ((op == '+') || (op == '-')) ) {
                return lhs;
            }
            if (lhs->kind == AST_INTEGER) {
                return AstInteger(EvalConstExpr(expr));
            }
        }
    }
    expr->left = lhs;
    expr->right = rhs;
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

int
FuncNumResults(AST *functype)
{
    functype = functype->left;
    if (functype == ast_type_void) {
        return 0;
    }
    if (!functype) {
//        WARNING(functype, "Internal error, NULL ptr in FuncNumResults");
        // assume 1 return value
        // we will get this with short anonymous functions
        return 1;
    }
    return (TypeSize(functype) + 3) / 4;
}

int
FuncNumParams(AST *functype)
{
    AST *arglist = functype->right;
    AST *arg;
    int n = 0;
    while (arglist) {
        arg = arglist->left;
        arglist = arglist->right;
        if (arg->kind == AST_VARARGS) {
            n = -n;
            break;
        }
        n++;
    }
    return n;
}
