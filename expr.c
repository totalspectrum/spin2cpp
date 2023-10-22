/*
 * Spin to C/C++ converter
 * Copyright 2011-2023 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling expressions
 */

#include "spinc.h"
#include "backends/bytecode/bc_bedata.h"
#include "becommon.h" // for PrintExpr
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>

#ifdef NEED_BUILTINS
uint16_t bswap16(uint16_t x) {
    return (x>>8) | ((x&0xff)<<8);
}
uint32_t bswap32(uint32_t x) {
    uint8_t a, b, c, d;
    a = x;
    b = (x>>8);
    c = (x>>16);
    d = (x>>24);
    return d | (((uint32_t)c)<<8) | (((uint32_t)b)<<16) | (((uint32_t)a)<<24);
}
int clz32(uint32_t x) {
    unsigned r = 0;
    for (r = 0; r < 32; r++) {
        if ( ((int32_t)x) < 0) break;
        x = x<<1;
    }
    return r;
}

int ctz32(uint32_t x) {
    unsigned r = 0;
    for (r = 0; r < 32; r++) {
        if ( x & 1 ) break;
        x = x>>1;
    }
    return r;
}

int popcount(uint64_t x) {
    int n = 0;
    for (unsigned i = 0; i < 64; i++) {
        if (x & (1ULL<<i)) n++;
    }
    return n;
}
#endif

/* get the internal identifier name from an identifier */
const char *
GetIdentifierName(AST *expr)
{
    const char *name;
    if (expr->kind == AST_IDENTIFIER) {
        name = expr->d.string;
    }  else if (expr->kind == AST_LOCAL_IDENTIFIER) {
        name = expr->left->d.string;
    } else {
        name = "expression";
    }
    return name;
}
const char *
GetUserIdentifierName(AST *expr)
{
    const char *name;
    if (expr->kind == AST_IDENTIFIER) {
        name = expr->d.string;
    }  else if (expr->kind == AST_LOCAL_IDENTIFIER) {
        name = expr->right->d.string;
    } else {
        name = "expression";
    }
    return name;
}

/* get a user-printable string for an expression */
const char *GetExprString(AST *expr) {
    if (IsIdentifier(expr)) {
        return GetUserIdentifierName(expr);
    } else {
        Flexbuf f;
        flexbuf_init(&f, 256);
        PrintExpr(&f, expr, PRINTEXPR_DEBUG);
        flexbuf_addchar(&f, 0);
        return flexbuf_get(&f);
    }
}

/* get an identifier name, or find the root name if an array reference */
const char *
GetVarNameForError(AST *expr)
{
    while (expr && expr->kind == AST_ARRAYREF) {
        expr = expr->left;
    }
    if (IsIdentifier(expr)) {
        return GetUserIdentifierName(expr);
    }
    return "variable";
}

/* get class from object type */
Module *
GetClassPtr(AST *objtype)
{
    objtype = BaseType(objtype);
    if (objtype->kind != AST_OBJECT) {
        ERROR(objtype, "Internal error, not an object symbol");
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
LookupGlobalSymbol(const char *name)
{
    int lang = LANG_DEFAULT;
    if (curfunc) {
        lang = curfunc->language;
    } else if (current) {
        lang = current->mainLanguage;
    }
    if (!IsSpinLang(lang)) {
        Module *top = GetTopLevelModule();
        if (top) {
            Symbol *s = LookupSymbolInTable(&top->objsyms, name);
            if (s) {
                return s;
            }
        }
    }
    return NULL;
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
    if (!sym) {
        sym = LookupGlobalSymbol(name);
    }
    return sym;
}

Symbol *
LookupSymbol(const char *name)
{
    Symbol *s;
    s = LookupSymbolInFunc(curfunc, name);
    if (!s) {
        // maybe it's a global symbol?
        // only do this for non-Spin languages
        s = LookupGlobalSymbol(name);
    }
    return s;
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
    const char *username = "";
    const char *ourname;

    if (ast->kind == AST_SYMBOL) {
        return (Symbol *)ast->d.ptr;
    }
    if (ast->kind == AST_IDENTIFIER || ast->kind == AST_LOCAL_IDENTIFIER) {
        id = ast;
    } else if (ast->kind == AST_ARRAYREF) {
        id = ast->left;
        if (id && id->kind == AST_MEMREF) {
            id = id->right;
        }
    } else {
        //ERROR(ast, "Internal error, bad id passed to LookupAstSymbol");
        return NULL;
    }
    if (id->kind == AST_LOCAL_IDENTIFIER) {
        username = GetUserIdentifierName(id);
        ourname = GetIdentifierName(id);
    } else if (id->kind == AST_IDENTIFIER || id->kind == AST_SYMBOL) {
        username = ourname = GetIdentifierName(id);
    } else {
        ERROR(ast, "expected identifier");
        return NULL;
    }
    sym = LookupSymbol(ourname);
    if (!sym && msg) {
        ERROR(id, "unknown identifier %s used in %s", username, msg);
    }
    return sym;
}

/*
 * look up an object member given the object type
 */
Symbol *
LookupMemberSymbol(AST *expr, AST *objtype, const char *name, Module **Ptr, int *valid)
{
    Module *P;
    Symbol *sym;
    AST *objident = NULL;

    objtype = BaseType(objtype);

    if (!objtype) {
        // if a constant ref, look for the object in the current modules list of
        // objects; this allows Spin to use constants before they are declared
        if (expr->kind == AST_CONSTREF) {
            objident = expr->left;
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
        } else if (expr->kind == AST_METHODREF && expr->left && expr->left->kind == AST_IDENTIFIER) {
            // if a method reference, it may be in a pending variable
            objident = expr->left;
            objtype = NULL;
            if (objident) {
                AST *a = current->pendingvarblock;
                while (a) {
                    AST *b = a->left;
                    a = a->right;
                    if (b->kind == AST_DECLARE_VAR) {
                        if (b->left && b->left->kind == AST_OBJECT
                                && AstUses(b->right, objident))
                        {
                            objtype = b->left;
                            break;
                        }
                    }
                }
            }
        }
    }
    if (!objtype || objtype->kind != AST_OBJECT) {
        if (valid) {
            *valid = 0;
        } else {
            ERROR(expr, "request for member %s in something not an object", name);
        }
        return NULL;
    }
    P = (Module *)objtype->d.ptr;
    if (Ptr) {
        *Ptr = P;
    }
    sym = FindSymbol(&P->objsyms, name);
    if (!sym) {
        ERROR(expr, "unknown identifier %s in class %s", name, P->classname);
    } else if (sym->kind == SYM_WEAK_ALIAS) {
        sym = FindSymbol(&P->objsyms, (const char *)sym->v.ptr);
    } else if (sym->kind == SYM_ALIAS) {
        AST *newexpr = (AST *)sym->v.ptr;
        if (newexpr && newexpr->kind == AST_METHODREF) {
            const char *basename = GetIdentifierName(newexpr->left);
            const char *subname = GetIdentifierName(newexpr->right);
            Symbol *basesym;
            Symbol *finalsym;
            AST *Qtype;
            basesym = FindSymbol(&P->objsyms, basename);
            if (!basesym || basesym->kind != SYM_VARIABLE) {
                ERROR(expr, "unknown anonymous class %s in class %s", basename, P->classname);
                return NULL;
            }
            Qtype = (AST *)basesym->v.ptr;
            if (!IsClassType(Qtype)) {
                ERROR(expr, "unknown anonymous class %s in class %s", basename, P->classname);
                return NULL;
            }
            finalsym = LookupMemberSymbol(newexpr, Qtype, subname, NULL, valid);
            if (!finalsym) return NULL;
            //WARNING(expr, "transforming anon struct ref: base off=%d plus %d", basesym->offset, finalsym->offset);
            sym->kind = finalsym->kind;
            sym->offset = finalsym->offset + basesym->offset;
            sym->v.ptr = finalsym->v.ptr; // preserve the type
        }
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
LookupMethodRef(AST *expr, Module **Ptr, int *valid)
{
    AST *type = ExprType(expr->left);
    AST *ident = expr->right;
    const char *name;

    name = GetUserIdentifierName(ident);
    return LookupMemberSymbol(expr, type, name, Ptr, valid);

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
    return LookupMemberSymbol(lhs, objtype, name, NULL, NULL);
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
    if (!exprlist) {
        ERROR(params, "no function given in coginit/cognew");
        return false;
    }
    if (exprlist->kind != AST_EXPRLIST || !exprlist->left) {
        ERROR(params, "coginit/cognew expected expression");
        return false;
    }
    func = exprlist->left;
    if (func->kind == AST_IDENTIFIER || func->kind == AST_LOCAL_IDENTIFIER) {
        sym = LookupAstSymbol(func, "coginit/cognew");
        if (sym && sym->kind == SYM_FUNCTION) {
            if (methodptr) *methodptr = (Function *)sym->v.ptr;
            return true;
        }
    }
    if (func->kind == AST_FUNCCALL) {
        /* FIXME? Spin requires that it be a local method; do we care? */
        sym = FindFuncSymbol(func, NULL, 1);
        if (sym) {
            if (sym->kind == SYM_BUILTIN) {
                return false;
            }
            if (sym->kind == SYM_FUNCTION) {
                if (methodptr) {
                    *methodptr = (Function *)sym->v.ptr;
                }
                return true;
            }
            // hmmm, interesting situation here; we've got an indirect
            // call via a variable
            if (sym->kind == SYM_LABEL) {
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
 * start of the current expression
 * FIXME: this does not work yet, needs some way to insert code at the beginning of
 * the whole expression!!!
 */
AST *
ReplaceExprWithVariable(const char *prefix, AST *expr, AST **inits)
{
    AST *exprvar;
    AST *initexpr;
    if (expr->kind == AST_IDENTIFIER || expr->kind == AST_SYMBOL || expr->kind == AST_LOCAL_IDENTIFIER)
        return expr; // already an identifier!

    // Need to make sure there are no side effects
    // otherwise we may get the wrong value
    if (ExprHasSideEffects(expr)) {
        return expr;
    }

    exprvar = AstTempLocalVariable(prefix, NULL);
    initexpr = AstAssign(exprvar, expr);
    *inits = AddToList(*inits, NewAST(AST_SEQUENCE, initexpr, NULL));
    return exprvar;
}

/* mask off a bit number */
static AST *
BitNumber(AST *x)
{
    if (curfunc && IsSpinLang(curfunc->language)) return x;
    if (!IsConstExpr(x)) return x;

    // make sure we don't turn 1<<n into a big 64 bit number
    return AstOperator('&', x, AstInteger(31));
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
        loexpr = FoldIfConst(BitNumber(dst->right->left));
        nbits = AstInteger(1);
        /* special case: if src is -1 or 0
         * then we don't have to construct a mask,
         * a single bit is enough
         */
        if (IsConstExpr(src) && !IsConstExpr(loexpr)) {
            ExprInt srcval = EvalConstExpr(src);
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
        hiexpr = FoldIfConst(BitNumber(dst->right->left));
        loexpr = FoldIfConst(BitNumber(dst->right->right));
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
        loexpr = BitNumber(dst->right->left);
    } else {
        AST *hiexpr;
        loexpr = BitNumber(dst->right->right);
        hiexpr = BitNumber(dst->right->left);
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
 * change x.[A addbits B] into x.[(B+A)..A]
 * Note: A addbits B is encoded as
 *    A | (B<<5)
 * We pass in the AST_RANGE item, so dst->kind == AST_RANGE
 *
 * Sigh... it's harder than that, the Spin2 compiler allows you to use
 * a variable in there so we have to extract the bits at run time :(
 */
static void
CheckAddBits(AST *expr)
{
    AST *sub;
    // don't do this on Spin1, legacy code will only assume 1 bit anyway
    int lang = LANG_DEFAULT;
    if (curfunc) {
        lang = curfunc->language;
    } else if (current) {
        lang = current->curLanguage;
    }
    if (lang != LANG_SPIN_SPIN2) {
        // all this headache is Spin2 only
        return;
    }

    if (expr->right != NULL) {
        // we've already got an explicit range
        return;
    }
    if (expr->left == NULL) {
        // what the heck? we'll throw an error, somewhere
        return;
    }

    sub = expr->left;
    if (IsConstExpr(sub)) {
        uint32_t x = (uint32_t)EvalConstExpr(sub);
        uint32_t firstval = x & 0x1f;
        uint32_t rangeval = (x >> 5) & 0x1f;
        uint32_t lastval = firstval + rangeval;

        if (lastval != firstval) {
            expr->left = AstInteger(lastval);
            expr->right = AstInteger(firstval);
        }
        return;
    }

    // check for explicit A addbits B, in which
    // case we extract and use A and B as is
    if ( sub->kind == AST_OPERATOR
            && sub->d.ival == '|'
       )
    {
        if (sub->right
                && sub->right->kind == AST_OPERATOR
                && sub->right->d.ival == K_SHL
                && sub->right->right->kind == AST_INTEGER
                && sub->right->right->d.ival == 5)
        {
            AST *firstpin = expr->left->left;
            AST *lastpin = sub->right->left;
            lastpin = AstOperator('+', firstpin, lastpin);
            expr->left = lastpin;
            expr->right = firstpin;
        }
    }

    // otherwise:
    // we painfully have to extract the bits to create the range

    // if we can prove somehow that the expression is < 32, we can also skip
    // a lot of nonsense
    if (sub->kind == AST_OPERATOR && sub->d.ival == '&' && sub->right && IsConstExpr(sub->right)) {
        uint32_t mask = (uint32_t)EvalConstExpr(sub->right);
        if (mask < 32) {
            return;
        }
    }

    AST *firstpin = AstOperator('&', sub, AstInteger(0x1f));
    AST *lastpin = AstOperator(K_SHR, sub, AstInteger(5));
    lastpin = AstOperator('+', firstpin, lastpin);

    expr->left = lastpin;
    expr->right = firstpin;
}

// create a new expression that puts the inits first, then the actual expression
static AST *
FixupInits(AST *result, AST *inits)
{
    AST *seq;
    if (!inits) return result;
    seq = NewAST(AST_SEQUENCE, result, NULL);
    seq = AddToList(inits, seq);
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
TransformRangeAssign(AST *dst, AST *src, int optoken, int toplevel)
{
    AST *nbits;
    AST *loexpr;
    AST *revsrc;
    AST *maskexpr;
    AST *hwreg;
    AST *hwreg2 = NULL;
    ASTReportInfo saveinfo;
    AST *result;
    AST *inits = NULL;

    if (!dst) return dst;
    if (!dst->right || dst->right->kind != AST_RANGE) {
        ERROR(dst, "Internal error, expecting range");
        return 0;
    }
    if (src && src->kind == AST_ASSIGN) {
        // watch out for assignment chains
        result = TransformAssignChain(dst, src);
        return result;
    }
    AstReportAs(dst, &saveinfo);  // set up error messages as if coming from "dst"
    /* change x.[a addbits b] to x.[(b+a)..a] */
    CheckAddBits(dst->right);

    hwreg = dst->left;
    if (hwreg->kind == AST_REGPAIR) {
        AST *assign, *assign2;
        AST *cond;
        hwreg2 = hwreg->right;
        hwreg = hwreg->left;

        hwreg = NewAST(AST_RANGEREF, hwreg, dst->right);
        hwreg2 = NewAST(AST_RANGEREF, hwreg2, dst->right);

        assign = TransformRangeAssign(hwreg, src, optoken, toplevel);
        assign2 = TransformRangeAssign(hwreg2, src, optoken, toplevel);
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
    }

    // Bytecode can do it natively
    if (TraditionalBytecodeOutput() && dst->left->kind == AST_HWREG) {
        AST *assign_again = AstAssign(dst,src);
        assign_again->d.ival = optoken;
        AstReportDone(&saveinfo);
        return assign_again;
    }

    if (optoken != K_ASSIGN) ERROR(dst,"Assign operator %03X unhandled in TransformRangeAssign",optoken);

    /* special case logical operators */

    /* doing a NOT on the whole thing */
    if (src->kind == AST_OPERATOR && src->d.ival == K_BIT_NOT
            && AstMatch(dst, src->right))
    {
        result = RangeXor(dst, AstInteger(-1));
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
            result = RangeXor(dst, AstInteger(-1));
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
            nbits = ReplaceExprWithVariable("_nbits", nbits, &inits);
        }
        needrev = FoldIfConst(AstOperator('<', hiexpr, loexpr));
        loexpr = AstOperator(K_LIMITMAX, loexpr, hiexpr);
        if (IsConstExpr(loexpr)) {
            loexpr = FoldIfConst(loexpr);
        } else {
            loexpr = ReplaceExprWithVariable("_lo", loexpr, &inits);
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
            return FixupInits(result, inits);
        }
    }
    if (IsConstExpr(nbits) && EvalConstExpr(nbits) >= 32) {
        result = AstAssign(dst->left, FoldIfConst(src));
        AstReportDone(&saveinfo);
        return FixupInits(result, inits);
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
        if (IsConstExpr(loexpr)) {
            loexpr = FoldIfConst(AstOperator('&', loexpr, AstInteger(0x1f)));
        }
        shift = AstOperator(K_SHL, AstInteger(1), loexpr);
        maskassign = AstAssign(maskvar, shift);
        maskassign = NewAST(AST_STMTLIST, maskassign, NULL);
        if (inits) {
            maskassign = AddToList(maskassign,
                                   NewAST(AST_STMTLIST, inits, NULL));
            inits = NULL;
        }
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
        ifstmt = AddToList(maskassign, ifstmt);
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
        if (IsConstExpr(loexpr)) {
            loexpr = FoldIfConst(AstOperator('&', loexpr, AstInteger(0x1f)));
        } else if (loexpr->kind != AST_IDENTIFIER && loexpr->kind != AST_LOCAL_IDENTIFIER) {
            loexpr = ReplaceExprWithVariable("lo_", loexpr, &inits);
        }
        if (!IsConstExpr(maskexpr)) {
            maskexpr = ReplaceExprWithVariable("mask_", maskexpr, &inits);
        }
            
        andexpr = AstOperator(K_SHL, maskexpr, loexpr);
        andexpr = AstOperator(K_BIT_NOT, NULL, andexpr);
        andexpr = AstOperator('&', andexpr, AstInteger(0xffffffff));
        andexpr = FoldIfConst(andexpr);

        orexpr = FoldIfConst(AstOperator('&', src, maskexpr));
        orexpr = AstOperator(K_SHL, orexpr, loexpr);
        orexpr = FoldIfConst(orexpr);

        //orexpr = NewAST(AST_MASKMOVE, dst->left,
        //                AstOperator('|', andexpr, orexpr));
        orexpr = NewAST(AST_MASKMOVE, dst->left,
                        NewAST(AST_SEQUENCE, andexpr, orexpr));
        result = AstAssign(dst->left, orexpr);
        AstReportDone(&saveinfo);
        return FixupInits(result, inits);
    }
}

/*
 * print a range use
 * src->right should be an AST_RANGE
 * src->left is a hardware register or simple expression
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
    AST *inits = NULL;

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
        ERROR(src, "Internal error, expecting range");
        return src;
    }
    AstReportAs(src, &saveinfo);
    CheckAddBits(range);
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
            AstReportDone(&saveinfo);
            if (x) return rega;
            return regb;
        }
        cond = NewAST(AST_CONDRESULT,
                      cond,
                      NewAST(AST_THENELSE, rega, regb));
        AstReportDone(&saveinfo);
        return cond;
    }

    // Bytecode can do it natively
    if (TraditionalBytecodeOutput() && src->left->kind == AST_HWREG) {
        AstReportDone(&saveinfo);
        return src;
    }

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
            nbits = ReplaceExprWithVariable("_bits", nbits, &inits);
        }
        lo = NewAST(AST_CONDRESULT,
                    test,
                    NewAST(AST_THENELSE, hi, lo));
        if (IsConstExpr(lo)) {
            lo = AstInteger(EvalConstExpr(lo));
        } else {
            lo = ReplaceExprWithVariable("_lo_", lo, &inits);
        }
    }
    //mask = AstInteger((1U<<nbits) - 1);
    mask = AstOperator(K_SHL, AstInteger(1), nbits);
    mask = AstOperator('-', mask, AstInteger(1));
    if (IsConstExpr(mask)) {
        mask = FoldIfConst(mask);
    } else {
        mask = ReplaceExprWithVariable("_mask_", mask, &inits);
    }

    /* we want to end up with:
       ((src->left >> lo) & mask)
    */
    if (IsConstExpr(lo) && IsConstExpr(mask)) {
        unsigned maskval = EvalConstExpr(mask);
        unsigned loval = EvalConstExpr(lo);
        // optimize a common case: if the shift leaves fewer
        // bits than the mask will take out, then
        // just do the shift
        loval = 0xffffffffU >> loval;
        if ( (loval & maskval) == loval ) {
            mask = NULL; // no need for the mask at all
        }
    }
    val = FoldIfConst(AstOperator(K_SHR, src->left, lo));
    if (mask) {
        val = FoldIfConst(AstOperator('&', val, mask));
    }
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
    return FixupInits(val, inits);
}

static ExprFloat
EvalFloatOperator(int op, ExprFloat lval, ExprFloat rval, int *valid)
{

    switch (op) {
    case '+':
    case K_FADD:
        return lval + rval;
    case '-':
    case K_FSUB:
        return lval - rval;
    case '/':
    case K_FDIV:
        return lval / rval;
    case '*':
    case K_FMUL:
        return lval * rval;
    case '|':
        return intAsFloat(floatAsInt(lval) | floatAsInt(rval));
    case '&':
        return intAsFloat(floatAsInt(lval) & floatAsInt(rval));
    case '^':
        return intAsFloat(floatAsInt(lval) ^ floatAsInt(rval));
    case K_HIGHMULT:
    case K_UNS_HIGHMULT:
        return lval*rval / (float)(1LL<<32);
    case K_SHL:
        return intAsFloat(floatAsInt(lval) << floatAsInt(rval));
    case K_SHR:
        return intAsFloat(((UExprInt)floatAsInt(lval)) >> floatAsInt(rval));
    case K_SAR:
        return intAsFloat(((ExprInt)floatAsInt(lval)) >> floatAsInt(rval));
    case '<':
    case K_LTU:
    case K_FLT:
        return intAsFloat(-(lval < rval));
    case '>':
    case K_GTU:
    case K_FGT:
        return intAsFloat(-(lval > rval));
    case K_LE:
    case K_LEU:
    case K_FLE:
        return intAsFloat(-(lval <= rval));
    case K_GE:
    case K_GEU:
    case K_FGE:
        return intAsFloat(-(lval >= rval));
    case K_NE:
    case K_FNE:
        return intAsFloat(-(lval != rval));
    case K_EQ:
    case K_FEQ:
        return intAsFloat(-(lval == rval));
    case K_NEGATE:
    case K_FNEGATE:
        return -rval;
    case K_ABS:
    case K_FABS:
        return (rval < 0) ? -rval : rval;
    case K_SQRT:
    case K_FSQRT:
        return sqrtf(rval);
    case K_POWER:
        return powf(lval, rval);
    default:
        if (valid)
            *valid = 0;
        else
            ERROR(NULL, "invalid floating point operator %d\n", op);
        return 0;
    }
}

static ExprInt
EvalFixedOperator(int op, ExprInt lval, ExprInt rval, int *valid)
{

    switch (op) {
    case '+':
        return lval + rval;
    case '-':
        return lval - rval;
    case '/':
        return (int32_t)( (1<<G_FIXPOINT) * (ExprFloat)lval / (ExprFloat)rval);
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
    case K_UNS_HIGHMULT:
        return ((uint64_t)lval*(uint64_t)rval) >> (G_FIXPOINT + 32);
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

static ExprInt
BoolValue(int v)
{
    if (!v) return 0;
    if (!curfunc) return -1;
    if (LangBoolIsOne(curfunc->language)) {
        return 1;
    } else {
        return -1;
    }
}

static ExprInt
EvalIntOperator(int op, ExprInt lval, ExprInt rval, int *valid, bool truncMath)
{

    switch (op) {
    case '+':
        return lval + rval;
    case '-':
        return lval - rval;
    case '/':
        if (truncMath) {
            lval = (ExprInt)(int32_t)lval;
            rval = (ExprInt)(int32_t)rval;
        }
        if (rval == 0) return rval;
        return lval / rval;
    case K_MODULUS:
        if (truncMath) {
            lval = (ExprInt)(int32_t)lval;
            rval = (ExprInt)(int32_t)rval;
        }
        if (rval == 0) return rval;
        return lval % rval;
    case K_UNS_DIV:
        if (truncMath) {
            lval = (ExprInt)(uint32_t)lval;
            rval = (ExprInt)(uint32_t)rval;
        }
        if (rval == 0) return rval;
        return (ExprInt)((UExprInt) lval / (UExprInt) rval);
    case K_UNS_MOD:
        if (truncMath) {
            lval = (ExprInt)(uint32_t)lval;
            rval = (ExprInt)(uint32_t)rval;
        }
        if (rval == 0) return rval;
        return (ExprInt)((UExprInt) lval % (UExprInt) rval);
    case K_FRAC64:
        if (rval == 0) return rval;
        return (ExprInt)( (((uint64_t)lval)<<32) / (UExprInt)rval );
    case '*':
        return lval * rval;
    case '|':
        return lval | rval;
    case '^':
        return lval ^ rval;
    case '&':
        return lval & rval;
    case K_HIGHMULT:
        return (int32_t)(((int_fast64_t)lval * (int_fast64_t)rval) >> 32LL);
    case K_UNS_HIGHMULT:
        return (int32_t)(((uint_fast64_t)lval * (uint_fast64_t)rval) >> 32ULL);
    case K_SCAS:
        return (int32_t)(((int_fast64_t)lval * (int_fast64_t)rval) >> 30LL);
    case K_SHL:
        return lval << (rval & 0x3f);
    case K_SHR:
        return ((UExprInt)lval) >> (rval & 0x3f);
    case K_SAR:
        return ((ExprInt)lval) >> (rval & 0x3f);
    case K_ROTL:
        return ((uint32_t)lval << rval) | ((uint32_t) lval) >> (32-rval);
    case K_ROTR:
        return ((uint32_t)lval >> rval) | ((uint32_t) lval) << (32-rval);
    case '<':
        return BoolValue(lval < rval);
    case '>':
        return BoolValue(lval > rval);
    case K_LE:
        return BoolValue(lval <= rval);
    case K_GE:
        return BoolValue(lval >= rval);
    case K_LTU:
        return BoolValue(((UExprInt)lval < (UExprInt)rval));
    case K_GTU:
        return BoolValue(((UExprInt)lval > (UExprInt)rval));
    case K_LEU:
        return BoolValue(((UExprInt)lval <= (UExprInt)rval));
    case K_GEU:
        return BoolValue(((UExprInt)lval >= (UExprInt)rval));
    case K_NE:
        return BoolValue(lval != rval);
    case K_EQ:
        return BoolValue(lval == rval);
    case K_BOOL_OR:
        return BoolValue(lval || rval);
    case K_BOOL_XOR:
        return BoolValue((lval != 0) ^ (rval != 0));
    case K_BOOL_AND:
        return BoolValue(lval && rval);
    case K_BOOL_NOT:
        return BoolValue(!rval);
    case K_LOGIC_OR:
        return BoolValue(lval) | BoolValue(rval);
    case K_LOGIC_XOR:
        return BoolValue(lval) ^ BoolValue(rval);
    case K_LOGIC_AND:
        return BoolValue(lval) & BoolValue(rval);
    case K_NEGATE:
        return -rval;
    case K_BIT_NOT:
        return ~rval;
    case K_ABS:
        return (rval < 0) ? -rval : rval;
    case K_SQRT:
        return (UExprInt)sqrt((ExprFloat)(uint32_t)rval);
    case K_DECODE:
        return 1L << (rval&0x1f);
    case K_ENCODE:
        return 32 - clz32(rval);
    case K_ENCODE2:
        return (rval == 0) ? 0 : 31-clz32(rval);
    case K_LIMITMIN:
        return (lval < rval) ? rval : lval;
    case K_LIMITMAX:
        return (lval > rval) ? rval : lval;
    case K_LIMITMIN_UNS:
        return ((UExprInt)lval < (UExprInt)rval) ? rval : lval;
    case K_LIMITMAX_UNS:
        return ((UExprInt)lval > (UExprInt)rval) ? rval : lval;
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
    case K_DECREMENT:
    case K_INCREMENT:
        if (valid) {
            *valid = 0;
        } else {
            ERROR(NULL, "unexpected operator %s in constant expression", (op == K_INCREMENT) ? "++" : "--");
        }
        return 0;
    case K_ONES_COUNT:
        return popcount(rval);
    case K_QLOG:
    {
        double e = log2((double)(uint32_t)rval);
        uint32_t i = (int)e;
        return (i << 27) | (uint32_t)round((e - (double)i)*(1<<27));
    }
    break;
    case K_QEXP:
    {
        double e = ((uint32_t)rval) >> 27;
        double f = ((double)(rval & ((1<<27)-1))) / (1<<27);
        e = pow(2.0, e+f);
        return (uint32_t)round(e);
    }
    break;
    case K_POWER:
    {
        double a = (double)lval;
        double b = (double)rval;
        a = pow(a, b);
        return (uint32_t)round(a);
    }
    case K_FADD:
        return floatAsInt(intAsFloat(lval) + intAsFloat(rval));
    case K_FSUB:
        return floatAsInt(intAsFloat(lval) - intAsFloat(rval));
    case K_FMUL:
        return floatAsInt(intAsFloat(lval) * intAsFloat(rval));
    case K_FDIV:
        return floatAsInt(intAsFloat(lval) / intAsFloat(rval));
    case K_FABS:
        return floatAsInt(fabsf(intAsFloat(rval)));
    case K_FSQRT:
        return floatAsInt(sqrtf(intAsFloat(rval)));
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
    bool truncMath = false; // if true, use only 32 bits for math

    if (curfunc) {
        truncMath = IsSpinLang(curfunc->language);
    } else if (current) {
        truncMath = IsSpinLang(current->curLanguage);
    }

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
    if (IsUnsignedType(le.type) || IsUnsignedType(re.type)) {
        switch (op) {
        case '/': op = K_UNS_DIV; break;
        case K_MODULUS: op = K_UNS_MOD; break;
        case '<':  op = K_LTU; break;
        case K_LE: op = K_LEU; break;
        case '>':  op = K_GTU; break;
        case K_GE: op = K_GEU; break;
        default:
            break;
        }
        if (!truncMath) {
            // may need to truncate the operation to preserve semantics
            if (TypeSize(le.type) <= 4 && TypeSize(re.type) <= 4) {
                truncMath = true;
            }
        }
    }
    return intExpr(EvalIntOperator(op, le.val, re.val, valid, truncMath));
}

#define PASM_FLAG 0x01
#define CHECK_DEFINED_FLAG 0x02
#define NO_DATBASE_NEEDED_FLAG 0x04

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
#define MAX_DEPTH 80

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
    AST *typ = NULL;
    
    if (!expr)
        return intExpr(0);
    if (depth > MAX_DEPTH) {
        ERROR(expr, "Expression too complicated or symbol definition loop");
        return intExpr(0);
    }

    kind = expr->kind;
    switch (kind) {
    case AST_EXPRLIST:
        // some 64 bit expressions come back as low/high pairs
        if (!expr->left || !expr->right || expr->right->right) {
            goto invalid_const_expr;
        }
        if (expr->left->kind != AST_GETLOW || expr->right->left->kind != AST_GETHIGH) {
            goto invalid_const_expr;
        }
        lval = EvalExpr(expr->left->left, flags, valid, depth+1);
        if (valid && !*valid) return intExpr(0);
        rval = EvalExpr(expr->right->left->left, flags, valid, depth+1);
        if (valid && !*valid) return intExpr(1);
        if (!CompatibleTypes(lval.type, rval.type)) goto invalid_const_expr;
        if (lval.val != rval.val) {
            goto invalid_const_expr;
        }
        return lval;
    case AST_HERE_IMM:
    case AST_INTEGER:
    case AST_BITVALUE:
        return intExpr(expr->d.ival);
    case AST_SIZEOF:
    {
        AST *typ = ExprType(expr->left);
        if (!typ) {
            ERROR(expr, "Unknown type for sizeof");
        }
        return intExpr(TypeSize(typ));
    }
    case AST_FLOAT:
        if (gl_fixedreal) {
            return fixedExpr(expr->d.ival);
        } else {
            return floatExpr(intAsFloat(expr->d.ival));
        }
    case AST_HASMETHOD:
    {
        Symbol *sym;
        AST *T = ExprType(expr->left);
        if (!T || !IsClassType(T)) {
            return intExpr(0);
        }
        Module *Q = GetClassPtr(T);
        if (!Q) return intExpr(0);
        sym = LookupSymbolInTable(&Q->objsyms, GetUserIdentifierName(expr->right));
        if (!sym || sym->kind != SYM_FUNCTION) {
            return intExpr(0);
        }
        return intExpr(1);
    }
    case AST_SAMETYPES:
    {
        return intExpr(AstMatch(expr->left, expr->right));
    }
    case AST_STRING:
    {
        const char *s = expr->d.string;
        int val = s[0] & 0xff;
        int ok = 1;
        size_t n = strlen(s);
        if (n > 1 && (gl_run_charset == CHARSET_UTF8)) {
            /* is this a UTF-8 sequence denoting a single code point? */
            /* if so, return that code point */
            wchar_t w;
            size_t cnt;
            ok = 0;
            cnt = from_utf8(&w, s, n);
            if (cnt == n) {
                val = w;
                ok = 1;
            }
            if (reportError && !ok) {
                WARNING(expr, "only first element of string is used");
            }
        }
        return intExpr(val);
    }
    case AST_GETLOW:
    case AST_GETHIGH:
    {
        AST *subexpr = expr->left;
        int64_t xval;
        if (!subexpr || subexpr->kind != AST_INTEGER) {
            goto invalid_const_expr;
        }
        xval = subexpr->d.ival;
        if (expr->kind == AST_GETHIGH) {
            xval = xval >> 32;
        }
        return intExpr(xval);
    }
    case AST_TOFLOAT:
        lval = EvalExpr(expr->left, flags, valid, depth+1);
        if ( !IsIntOrGenericType(lval.type)) {
            ERROR(expr, "applying float to a non integer expression");
        }
        return convToFloat(lval);
    case AST_TRUNC:
        lval = EvalExpr(expr->left, flags, valid, depth+1);
        if (valid && !*valid) {
            return intExpr(0);
        }
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
        if (valid && !*valid) {
            return intExpr(0);
        }
        if (!IsFloatType(lval.type)) {
            ERROR(expr, "applying round to a non float expression");
        }
        if (gl_fixedreal) {
            return intExpr( (lval.val + (1<<(G_FIXPOINT-1))) >> G_FIXPOINT);
        } else {
            return intExpr((int)roundf(intAsFloat(lval.val)));
        }
    case AST_CONSTANT:
        return EvalExpr(expr->left, flags|PASM_FLAG, valid, depth+1);
    case AST_CONSTREF:
    case AST_METHODREF:
    {
        Module *P;

        sym = LookupMethodRef(expr, &P, valid);
        if (!sym) {
            return intExpr(0);
        }
#if 0
        if ((sym->kind != SYM_CONSTANT && sym->kind != SYM_FLOAT_CONSTANT)) {
            if (valid) {
                *valid = 0;
            } else {
                ERROR(expr, "%s is not a constant of %s", GetIdentifierName(expr->right), P->classname);
            }
            return intExpr(0);
        }
#endif
        /* while we're evaluating, use the object context */
        ret = EvalExprInState(P, expr->right, flags, valid, depth+1);
        return ret;
    }
    case AST_RESULT:
        *valid = 0;
        return intExpr(0);
    case AST_LOCAL_IDENTIFIER:
    case AST_SYMBOL:
    case AST_IDENTIFIER:
        if (expr->kind == AST_SYMBOL) {
            sym = (Symbol *)expr->d.ptr;
            name = sym->user_name;
        } else if (expr->kind == AST_LOCAL_IDENTIFIER) {
            name = expr->right->d.string;
            sym = LookupSymbol(expr->left->d.string);
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
            switch (sym->kind) {
            case SYM_CONSTANT:
            {
                ExprVal e = EvalExpr((AST *)sym->v.ptr, 0, valid, depth+1);
                return intExpr(e.val);
            }
            case SYM_FLOAT_CONSTANT:
            {
                ExprVal e = EvalExpr((AST *)sym->v.ptr, 0, valid, depth+1);
                if (gl_fixedreal) {
                    return fixedExpr(e.val);
                } else {
                    return floatExpr(intAsFloat(e.val));
                }
            }
            case SYM_PARAMETER:
            case SYM_RESULT:
            case SYM_LOCALVAR:
            case SYM_TEMPVAR:
            {
                if ((flags & PASM_FLAG) && curfunc) {
                    // we must be in inline assembly
                    offset = (sym->offset + 3) / 4;
                    if (offset > PASM_MAX_ASM_VAR_OFFSET) {
                        ERROR(expr, "Only the first 16 variables in a function may be accessed in inline assembly");
                    }
                    offset += PASM_INLINE_ASM_VAR_BASE;
                    return intExpr(offset);
                }
                if (reportError) {
                    ERROR(expr, "Symbol %s may not be accessed in inline assembly", sym->user_name);
                } else {
                    *valid = 0;
                }
                return intExpr(0);
            }
            break;
            case SYM_LABEL:
                if (flags & PASM_FLAG) {
                    Label *lref = (Label *)sym->v.ptr;
                    if (lref->flags & LABEL_IN_HUB) {
                        return intExpr(lref->hubval);
                    }
                    if (lref->cogval & 0x03) {
                        if (reportError) {
                            ERROR(expr, "label %s in COG memory not on longword boundary", sym->user_name);
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
        /* special case hack: '-' allows evaluation of @
           even if both sides are relative addresses
        */
        if (expr->d.ival == '-' && expr->left->kind == AST_ADDROF && expr->right->kind == AST_ADDROF) {
            flags |= (PASM_FLAG|NO_DATBASE_NEEDED_FLAG);
        }
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
        } else if (expr->left && IsIdentifier(expr->left) && IsConstExpr(expr->right) && (flags & PASM_FLAG)) {
            ExprVal lval;
            lval = EvalExpr(expr->left, flags, valid, depth+1);
            if (IsIntType(lval.type)) {
                ExprInt base = lval.val;
                ExprInt offset = EvalConstExpr(expr->right);
                return intExpr(base + offset);
            }
        }
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
    case AST_FIELDADDR:
    {
        /* it's OK to take the address of a label; in that case, just
           send back the offset into the dat section
        */
        bool isRegister = false;
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
            typ = rval.type;
        }
        if (expr->kind != AST_IDENTIFIER && expr->kind != AST_SYMBOL && expr->kind != AST_LOCAL_IDENTIFIER) {
            if (reportError)
                ERROR(expr, "Only addresses of identifiers allowed");
            else
                *valid = 0;
            return intExpr(0);
        }
        if (expr->kind == AST_SYMBOL) {
            sym = (Symbol *)expr->d.ptr;
            name = sym->user_name;
        } else if (expr->kind == AST_LOCAL_IDENTIFIER) {
            sym = LookupSymbol(expr->left->d.string);
            name = expr->right->d.string;
        } else {
            name = expr->d.string;
            sym = LookupSymbol(name);
        }
        if (sym && sym->kind == SYM_LABEL) {
            Label *lref = (Label *)sym->v.ptr;
            if (0 == (lref->flags & LABEL_IN_HUB)) {
                isRegister = true;
            }
            if (offset) {
                typ = BaseType(lref->type);
                offset *= TypeSize(BaseType(typ));
            }
            if ( (gl_dat_offset == -1 && kind == AST_ABSADDROF && gl_output != OUTPUT_BYTECODE) || 0 == (flags & PASM_FLAG)) {
                if (reportError) {
                    ERROR(expr, "offset for address operator is not known");
                } else {
                    *valid = 0;
                }
            }

            if (gl_output == OUTPUT_BYTECODE && gl_interp_kind != INTERP_KIND_NUCODE) {
                int datoffset;

                if (flags & NO_DATBASE_NEEDED_FLAG) {
                    datoffset = 0;
                } else {
                    datoffset = BCgetDAToffset(current,kind == AST_ABSADDROF,expr,reportError);
                }
                if (datoffset < 0) {
                    if (valid) *valid = 0;   // Error (BCgetDAToffset will have already printed it if needed)
                }
                else offset += datoffset;
            } else if (kind == AST_ABSADDROF || kind == AST_FIELDADDR) {
                offset += gl_dat_offset > 0 ? gl_dat_offset : 0;
            }
            if (kind == AST_FIELDADDR) {
                int x = TypeSize(typ);
                switch (x) {
                case 1:  offset |= 0x4e000000; break;
                case 2:  offset |= 0x9e000000; break;
                default:
                case 4:
                    if (isRegister) {
                        offset |= 0x3e000000;
                    } else {
                        offset |= 0xfe000000;
                    }
                    break;
                }
            }
            return intExpr(lref->hubval + offset);
        } else if (sym && sym->kind == SYM_VARIABLE && (sym->flags & SYMF_GLOBAL)) {
            return intExpr(sym->offset);
        } else {
            if (reportError) {
                if (!sym) {
                    ERROR(expr, "Unknown symbol %s", name);
                } else {
                    ERROR(expr, "Only addresses of labels allowed");
                }
            } else {
                *valid = 0;
            }
            return intExpr(0);
        }
        break;
    }
    case AST_CAST:
        rval = EvalExpr(expr->right, flags, valid, depth+1);
        if (IsGenericType(expr->left)) {
            return rval;
        }
        /* avoid redundant casts */
        if (IsFloatType(expr->left) && IsFloatType(rval.type)) {
            return rval;
        }
        if (!IsFloatType(expr->left) && !IsFloatType(rval.type)) {
            rval.type = expr->left; // preserve unsigned casts
            return rval;
        }
        if (IsFloatType(expr->left)) {
            return convToFloat(rval);
        } else if (IsFloatType(rval.type)) {
            rval.val = (int)intAsFloat(rval.val);
        }
        rval.type = expr->left;
        return rval;
    case AST_EXPECT: return EvalExpr(expr->left,flags,valid,depth+1);
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

ExprInt
EvalConstExpr(AST *expr)
{
    ExprVal e = EvalExpr(expr, 0, NULL, 0);
    return e.val;
}

ExprInt
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

bool
IsDefinedExpr(AST *expr)
{
    int valid;
    valid = 1;
    EvalExpr(expr, PASM_FLAG | CHECK_DEFINED_FLAG, &valid, 0);
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
ExprVal intExpr(ExprInt x)
{
    ExprVal e;
    e.type = ast_type_long;
    e.val = x;
    return e;
}

ExprVal floatExpr(ExprFloat f)
{
    ExprVal e;
    e.type = ast_type_float;
    e.val = floatAsInt(f);
    return e;
}

ExprVal fixedExpr(ExprInt f)
{
    ExprVal e;
    e.type = ast_type_float;
    e.val = f;
    return e;
}

ExprInt  floatAsInt(ExprFloat f)
{
    union float_or_int v;

    v.f = f;
    return v.i;
}

ExprFloat intAsFloat(ExprInt i)
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
    if (expr && expr->kind == AST_LOCAL_IDENTIFIER) {
        expr = expr->left;
    }
    if (!expr || expr->kind != AST_IDENTIFIER)
        return 0;
    sym = LookupSymbol(expr->d.string);
    if (!sym)
        return 0;
    if (sym->kind == SYM_LABEL)
        return 1;
    if (sym->kind != SYM_VARIABLE && sym->kind != SYM_LOCALVAR)
        return 0;
    type = (AST *)sym->v.ptr;
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
    while(ast && (ast->kind == AST_MODIFIER_CONST || ast->kind == AST_MODIFIER_VOLATILE || ast->kind == AST_MODIFIER_SEND_ARGS || ast->kind == AST_TYPEDEF || ast->kind == AST_REGISTER || ast->kind == AST_ANNOTATION)) {
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
    case AST_STATIC:
    case AST_ANNOTATION:
        return IsArrayType(ast->left);
    case AST_ARRAYTYPE:
        return 1;
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
    case AST_PTRTYPE:
    case AST_REFTYPE:
    case AST_COPYREFTYPE:
    case AST_VOIDTYPE:
    case AST_FUNCTYPE:
    case AST_OBJECT:
    case AST_TUPLE_TYPE:
    case AST_BITFIELD:
    case AST_SIGNED_BOOLTYPE:
    case AST_UNS_BOOLTYPE:
        return 0;
    case AST_TYPEOF:
        return IsArrayType(ExprType(ast->left));
    default:
        ERROR(ast, "Internal error, unknown type %d passed to IsArrayType",
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
        return (AST *)arraytype->d.ptr;
    }
    return NULL;
}

/* find minimum alignment for a type */
int TypeAlign(AST *typ)
{
    int r;
    typ = RemoveTypeModifiers(typ);
    while (typ && typ->kind == AST_ARRAYTYPE) {
        typ = RemoveTypeModifiers(typ->left);
    }
    if (!typ) {
        return LONG_SIZE;
    }
    switch (typ->kind) {
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
        r = EvalConstExpr(typ->left);
        if (r <= LONG_SIZE) return r;
        return LONG_SIZE;
    case AST_PTRTYPE:
    case AST_REFTYPE:
    case AST_COPYREFTYPE:
    case AST_FUNCTYPE:
    default:
        return 4; // all pointers are 4 bytes
    case AST_OBJECT:
    {
        Module *P = (Module *)typ->d.ptr;
        if (P->varsize < 3 && P->varsize > 0) {
            return P->varsize;
        }
        return LONG_SIZE;
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
    case AST_MODIFIER_SEND_ARGS:
    case AST_ANNOTATION:
    case AST_STATIC:
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
    case AST_UNS_BOOLTYPE:
    case AST_SIGNED_BOOLTYPE:
        return EvalConstExpr(typ->left);
    case AST_PTRTYPE:
    case AST_REFTYPE:
    case AST_COPYREFTYPE:
    case AST_FUNCTYPE:
        return 4; // all pointers are 4 bytes
    case AST_OBJECT:
    {
        Module *P = (Module *)typ->d.ptr;
        if (P->pendingvarblock) {
            ERROR(typ, "Internal error, Taking size of an object with pending variables\n");
        }
        return P->varsize;
    }
    case AST_TUPLE_TYPE:
    {
        int siz = TypeSize(typ->left);
        if (typ->right) {
            siz += TypeSize(typ->right);
        }
        return siz;
    }
    case AST_VOIDTYPE:
    {
        return 1;
    }
    case AST_TYPEOF:
        return TypeSize(ExprType(typ->left));
    default:
        ERROR(typ, "Internal error, unknown type %d passed to TypeSize",
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
    case AST_REFTYPE:
    case AST_COPYREFTYPE:
        return typ->left;
    default:
        return typ;
    }
}

/* find size for a type that's a member of an object */
/* generally like TypeSize, but pads arrays out to
 * long boundaries
 */
int PaddedTypeAlign(AST *typ)
{
    int siz;
    int align;
    if (!typ) return LONG_SIZE;
    align = TypeAlign(typ);
    siz = TypeSize(typ);
    if (siz >= LONG_SIZE) {
        align = (align+3)&~3;
    }
    return align;
}

int
IsArraySymbol(Symbol *sym)
{
    AST *type = NULL;
    if (!sym) return 0;
    switch (sym->kind) {
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
    case SYM_VARIABLE:
    case SYM_PARAMETER:
    case SYM_RESULT:
        type = (AST *)sym->v.ptr;
        break;
    case SYM_LABEL:
        return 1;
    default:
        return 0;
    }
    return IsArrayType(type);
}

int
IsArrayOrPointerSymbol(Symbol *sym)
{
    AST *type = NULL;
    if (!sym) return 0;
    switch (sym->kind) {
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
    case SYM_VARIABLE:
    case SYM_PARAMETER:
    case SYM_RESULT:
        type = (AST *)sym->v.ptr;
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
    
    if (IsIdentifier(expr)) {
        sym = LookupAstSymbol(expr, errflag ? "function call" : NULL);
        return sym;
    }
    if (expr->kind != AST_METHODREF) {
        if (expr->kind != AST_FUNCCALL && expr->kind != AST_ADDROF && expr->kind != AST_ABSADDROF) {
            //ERROR(expr, "Internal error expecting function call");
            // actually this can happen in some cases (e.g. in BASIC while some array refs still look
            // like function calls
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
        AST *objtype;
        objref = expr->left;
        objtype = ExprType(objref);
        while (objtype && IsRefType(objtype)) {
            objtype = objtype->left;
        }
        thename = GetUserIdentifierName(expr->right);
        if (!thename) {
            return NULL;
        }
        if (!IsClassType(objtype)) {
            if (errflag) {
                ERROR(ast, "request for member %s in something that is not an object", thename);
            }
            return NULL;
        }
        sym = LookupMemberSymbol(objref, objtype, thename, NULL, NULL);
        if (sym) {
            if (sym->flags & SYMF_PRIVATE) {
                ERROR(ast, "attempt to access private member symbol %s", thename);
                sym->flags &= ~SYMF_PRIVATE; // prevent future errors
            }
        }
    } else {
        if (expr) {
            sym = LookupAstSymbol(expr, errflag ? "function call" : NULL);
        } else if (errflag) {
            ERROR(ast, "NULL pointer in function call");
        }
        if (sym && sym->kind == SYM_ALIAS) {
            AST *newexpr = (AST *)sym->v.ptr;
            return FindFuncSymbol(newexpr, objrefPtr, errflag);
        }
    }
    if (objrefPtr) *objrefPtr = objref;
    if (sym && sym->kind == SYM_FUNCTION) {
        Function *pf = (Function *)sym->v.ptr;
        if (pf && pf->sym_funcptr) {
            sym = (Symbol *)pf->sym_funcptr;
        }
    }
    return sym;
}

/* find called function symbol in a function call */
/* if errflag is nonzero, print errors for identifiers not found */
Symbol *
FindCalledFuncSymbol(AST *ast, AST **objrefPtr, int errflag)
{
    return FindFuncSymbol(ast, objrefPtr, errflag);
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
IsFloat64Type(AST *type)
{
    int size;
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind != AST_FLOATTYPE)
        return 0;
    size = EvalConstExpr(type->left);
    if (size == LONG64_SIZE) {
        return 1;
    }
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
IsInterfaceType(AST *type)
{
    Module *P;
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind != AST_OBJECT) {
        return 0;
    }
    P = GetClassPtr(type);
    if (P && P->isInterface) {
        return 1;
    }
    return 0;
}

int
IsPointerType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;

    if (type->kind == AST_PTRTYPE || type->kind == AST_REFTYPE || type->kind == AST_COPYREFTYPE)
        return 1;
    return 0;
}

int
IsRefType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;

    if (type->kind == AST_REFTYPE || type->kind == AST_COPYREFTYPE)
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
    switch (type->kind) {
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_UNS_BOOLTYPE:
    case AST_SIGNED_BOOLTYPE:
        return 1;
    default:
        return 0;
    }
}

int
IsUnsignedType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_UNSIGNEDTYPE || type->kind == AST_UNS_BOOLTYPE)
        return 1;
    return 0;
}

int
IsBoolType(AST *type)
{
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (type->kind == AST_UNS_BOOLTYPE || type->kind == AST_SIGNED_BOOLTYPE)
        return 1;
    return 0;
}

int
IsInt64Type(AST *type)
{
    int size;
    type = RemoveTypeModifiers(type);
    if (!type) return 0;
    if (! (type->kind == AST_INTTYPE || type->kind == AST_UNSIGNEDTYPE) )
        return 0;
    size = EvalConstExpr(type->left);
    if (size == LONG64_SIZE) {
        return 1;
    }
    return 0;
}

int
NumArgsForType(AST *functype) {
    functype = BaseType(functype);
    return AstListLen(functype->right);
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
    case AST_GENERICTYPE:
    case AST_UNS_BOOLTYPE:
    case AST_SIGNED_BOOLTYPE:
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
    if (type->kind == AST_PTRTYPE || type->kind == AST_REFTYPE || type->kind == AST_COPYREFTYPE) {
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

int
IsConstArrayType(AST *type)
{
    if (IsArrayType(type)) {
        return IsConstType(BaseType(type));
    }
    return 0;
}

int
IsConstInitializer(AST *ident)
{
    AST *expr;
    if (ident->kind == AST_ASSIGN) {
        expr = ident->right;
    } else {
        expr = ident;
    }
    if (expr && expr->kind == AST_EXPRLIST) {
        AST *item;
        while (expr) {
            item = expr->left;
            expr = expr->right;
            if (!IsConstExpr(item)) {
                return 0;
            }
        }
        return 1;
    }    
    return IsConstExpr(expr);
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
    lsize = TypeSize(left);
    rsize = TypeSize(right);
    if (IsBoolType(left)) {
        lsize = 0;  // bool gets demoted
    }
    if (IsBoolType(right)) {
        rsize = 0;  // bool gets demoted
    }
    if (!left && rsize >= LONG_SIZE) return right;
    if (!right && lsize >= LONG_SIZE) return left;
    if (left && right && left->kind != right->kind) {
        if (lsize > LONG_SIZE || rsize > LONG_SIZE) {
            if (IsUnsignedType(left) && IsUnsignedType(right)) {
                return ast_type_unsigned_long64;
            }
            return ast_type_long64;
        }
        if (IsUnsignedType(left) && IsUnsignedType(right)) {
            return ast_type_unsigned_long;
        }
        return ast_type_long;
    }
    if (lsize < rsize) return right;
    return left;
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
    int lang = GetCurrentLang();
    
    if (!expr) return NULL;
    if (!P) {
        P = current;
    }
    switch (expr->kind) {
    case AST_TYPEOF:
        return ExprTypeRelative(table, expr->left, P);
    case AST_INTEGER:
        if ( IsCLang(lang) && expr->d.ival == 0) {
            return ast_type_generic;
        }
        if (expr->left) {  /* explicit cast */
            return expr->left;
        }
        /* go based on the size */
        {
            int64_t x = expr->d.ival;
            if (x >= INT32_MIN && x <= INT32_MAX)
                return ast_type_long;
            if (x >= 0 && x <= UINT32_MAX)
                return ast_type_unsigned_long;
            return ast_type_long64;
        }
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
    case AST_FUNC_NAME:
    case AST_STRING:
        // in Spin, a string is always dereferenced
        // so "abc" is the same as "a" is the same as 0x65
        // (actually no -- "abc" is the same as "a", "b", "c")
        if (IsSpinLang(lang)) {
            return ast_type_long;
        }
    /* otherwise fall through */
    case AST_STRINGPTR:
        if ( lang == LANG_CFAMILY_C && curfunc && !(curfunc->warn_flags & WARN_C_CONST_STRING) ) {
            return ast_type_ptr_byte;
        }
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
    case AST_LOCAL_IDENTIFIER:
    {
        Symbol *sym;
        Label *lab;
        AST *typ;
        const char *intern_name, *user_name;

        if (expr->kind == AST_LOCAL_IDENTIFIER) {
            intern_name = expr->left->d.string;
            user_name = expr->right->d.string;
        } else {
            intern_name = user_name = expr->d.string;
        }
        sym = LookupSymbolInTable(table, intern_name);
        if (!sym) return NULL;
        switch (sym->kind) {
        case SYM_CONSTANT:
        case SYM_HW_REG:
            return ast_type_long;
        case SYM_LABEL:
            lab = (Label *)sym->v.ptr;
            typ = lab->type;
            if (IsSpinLang(lang) && typ && typ->kind != AST_ARRAYTYPE) {
                if (typ == ast_type_void) {
                    return typ;
                }
                return NewAST(AST_ARRAYTYPE, typ, AstInteger(1));
            }
            return typ;
        case SYM_FLOAT_CONSTANT:
            return ast_type_float;
        case SYM_VARIABLE:
        case SYM_LOCALVAR:
        case SYM_TEMPVAR:
        case SYM_PARAMETER:
        case SYM_CLOSURE:
        case SYM_TYPEDEF:
            return (AST *)sym->v.ptr;
        case SYM_FUNCTION:
            return ((Function *)sym->v.ptr)->overalltype;
        case SYM_ALIAS:
            // an alias for an expression
            return ExprTypeRelative(table, (AST *)sym->v.ptr, P);
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
        if (!(sub->kind == AST_PTRTYPE || sub->kind == AST_ARRAYTYPE || sub->kind == AST_REFTYPE || sub->kind == AST_COPYREFTYPE)) return NULL;
        return sub->left;
    case AST_FUNCCALL:
    {
        Symbol *sym = FindFuncSymbol(expr, NULL, 0);
        AST *typexpr = NULL;
        if (expr->left) {
            if (expr->left->kind == AST_ARRAYREF) {
                sub = expr->left->left;
                if (sub && sub->kind == AST_MEMREF && sub->left) {
                    typexpr = sub->left;
                }
            } else if (expr->left->kind == AST_CAST) {
                typexpr = expr->left->left;
            }
        }
        if (typexpr) {
            if (typexpr->kind == AST_FUNCTYPE) {
                return typexpr->left;
            } else {
                // don't error here, let the high level handle it
                //ERROR(expr, "function call on non-function type");
                return NULL;
            }
        }
        if (sym) {
            switch (sym->kind) {
            case SYM_FUNCTION:
                return GetFunctionReturnType(((Function *)sym->v.ptr));
            case SYM_VARIABLE:
            case SYM_PARAMETER:
            case SYM_LOCALVAR:
            case SYM_TEMPVAR:
                typexpr = (AST *)sym->v.ptr;
                AST *old_typexpr = typexpr;
                if (typexpr && (typexpr->kind == AST_PTRTYPE || typexpr->kind == AST_REFTYPE || typexpr->kind == AST_COPYREFTYPE)) {
                    typexpr = RemoveTypeModifiers(typexpr->left);
                }
                if (typexpr && typexpr->kind == AST_FUNCTYPE) {
                    return typexpr->left;
                }
                if ( (IsArrayType(old_typexpr) || IsPointerType(old_typexpr)) && IsBasicLang(lang)) {
                    // in BASIC we may not have converted x(i) into an array reference yet
                    return BaseType(old_typexpr);
                }
                if (IsSpinLang(lang)) {
                    if (old_typexpr == NULL) {
                        return NULL;
                    }
                }
                ERROR(expr, "%s is not a function but is called like one", sym->user_name);
                break;
            default:
                return NULL;
            }
        } else if (expr && expr->left && expr->left->kind == AST_CAST) {
            typexpr = expr->left->left;
        }
        if (typexpr && (typexpr->kind == AST_PTRTYPE || typexpr->kind == AST_REFTYPE || typexpr->kind == AST_COPYREFTYPE)) {
            typexpr = RemoveTypeModifiers(typexpr->left);
        }
        if (typexpr && typexpr->kind == AST_FUNCTYPE) {
            return typexpr->left;
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
        AST *typexpr = NULL;
        Symbol *sym = NULL;
        const char *methodname;
        Function *func;
        Module *subclass;

        if (expr->right->kind != AST_IDENTIFIER && expr->right->kind != AST_LOCAL_IDENTIFIER) {
            ERROR(expr, "Expecting identifier after '.'");
            return NULL;
        }
        methodname = GetUserIdentifierName(expr->right);
        objtype = BaseType(ExprTypeRelative(table, objref, P));
        if (!objtype) return NULL;
        if (!IsClassType(objtype)) {
            ERROR(expr, "Expecting object for dereference of %s", methodname);
            return NULL;
        }
        sym = LookupMemberSymbol(expr, objtype, methodname, NULL, NULL);
        if (!sym) {
            ERROR(expr, "%s is not a member of %s", methodname, TypeName(objtype));
            return NULL;
        }
        if (sym->flags & SYMF_PRIVATE) {
            ERROR(expr, "attempt to access private member symbol %s", sym->user_name);
            sym->flags &= ~SYMF_PRIVATE; // prevent future errors
        }
        subclass = GetClassPtr(objtype);
        switch (sym->kind) {
        case SYM_FUNCTION:
            func = (Function *)sym->v.ptr;
            return func->overalltype;
        case SYM_VARIABLE:
            return (AST *)sym->v.ptr;
        case SYM_CONSTANT:
        case SYM_FLOAT_CONSTANT:
            return ExprTypeRelative(&subclass->objsyms, (AST *)sym->v.ptr, P);
        case SYM_ALIAS:
            typexpr = (AST *)sym->v.ptr;
            if (typexpr->kind == AST_CAST) {
                typexpr = typexpr->right;
            }
            if (typexpr && typexpr->kind == AST_RANGEREF) {
                AST *typ = NewAST(AST_USING, (AST *)sym->v.ptr, NULL);
                return typ;
            }
            return ExprTypeRelative(&subclass->objsyms, (AST *)sym->v.ptr, P);
        case SYM_LABEL:
        {
            Label *lref = (Label *)sym->v.ptr;
            if (lref && lref->type) {
                return lref->type;
            }
            /* fall through to default */
        }
        default:
            ERROR(expr, "Unable to handle member %s", methodname);
            return NULL;
        }
    }
    case AST_OPERATOR:
    {
        AST *ltype, *rtype;
        switch (expr->d.ival) {
        case K_POWER:
            return ast_type_float;
        case '-':
        case '+':
        case K_INCREMENT:
        case K_DECREMENT:
            ltype = ExprTypeRelative(table, expr->left, P);
            rtype = ExprTypeRelative(table, expr->right, P);
            if (IsFloatType(ltype) || IsFloatType(rtype)) {
                return ast_type_float;
            }
            if (expr->d.ival == '+') {
                if (IsStringType(ltype)) {
                    return ltype;
                } else if (IsStringType(rtype)) {
                    return ltype;
                }
            }
            if (IsSpinLang(lang)) {
                if (ltype) {
                    //if (IsIntOrGenericType(ltype)) return ltype;
                    if (IsPointerType(ltype)) {
                        if (PointerTypeIncrement(ltype) == 1) {
                            return ltype;
                        }
                        return ast_type_generic;
                    }
                }
                return WidestType(ltype, rtype);
            }
            if (IsArrayType(ltype)) {
                ltype = ArrayToPointerType(ltype);
            }
            if (IsArrayType(rtype)) {
                rtype = ArrayToPointerType(rtype);
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
            if (TypeSize(ExprType(expr->left)) > LONG_SIZE) {
                return ast_type_unsigned_long64;
            }
            return ast_type_unsigned_long;
        case '&':
            ltype = ExprTypeRelative(table, expr->left, P);
            rtype = ExprTypeRelative(table, expr->right, P);
            if (IsBasicLang(lang)) {
                if (IsStringType(ltype)) {
                    return ltype;
                } else if (IsStringType(rtype)) {
                    return ltype;
                }
            }
            if ( TypeSize(ltype) > LONG_SIZE ) {
                return ast_type_long64;
            }
            if ( TypeSize(rtype) > LONG_SIZE ) {
                return ast_type_long64;
            }
            return ast_type_long;
        case '<':
        case '>':
        case K_EQ:
        case K_NE:
        case K_GE:
        case K_LE:
        case K_GEU:
        case K_LEU:
        case K_GTU:
        case K_LTU:
        case K_BOOL_AND:
        case K_BOOL_OR:
        case K_BOOL_NOT:
        {
            if (IsBasicLang(lang)) {
                return ast_type_basic_boolean;
            }
        }
        default:
            ltype = ExprTypeRelative(table, expr->left, P);
            rtype = ExprTypeRelative(table, expr->right, P);
            if ( TypeSize(ltype) > LONG_SIZE ) {
                return ast_type_long64;
            }
            if ( TypeSize(rtype) > LONG_SIZE ) {
                return ast_type_long64;
            }
            return ast_type_long;
        }
    }
    case AST_EXPRLIST:
    {
        AST *typ = NewAST(AST_TUPLE_TYPE, NULL, NULL);
        typ->left = ExprTypeRelative(table, expr->left, P);
        if (expr->right) {
            typ->right = ExprTypeRelative(table, expr->right, P);
        }
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
    case AST_STMTLIST:
    {
        // find the last item in the statement list
        while (expr && expr->kind == AST_STMTLIST) {
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
    case AST_REFTYPE:
    case AST_COPYREFTYPE:
    case AST_ARRAYTYPE:
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
    case AST_FLOATTYPE:
    case AST_MODIFIER_CONST:
    case AST_MODIFIER_VOLATILE:
    case AST_OBJECT:
        return expr;
    case AST_CONDRESULT:
    {
        AST *sub = expr->right;
        AST *left, *right;
        if (!sub || sub->kind != AST_THENELSE) {
            ERROR(expr, "Internal error in ?: parsing");
            return NULL;
        }
        left = ExprTypeRelative(table, sub->left, P);
        right = ExprTypeRelative(table, sub->right, P);
        if (CompatibleTypes(left, right)) {
            return left;
        }
        return NULL;
    }
    case AST_ASSIGN:
        return ExprTypeRelative(table, expr->left, P);
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
    if (A->kind == AST_OBJECT) {
        return A->d.ptr == B->d.ptr;
    }
    return AstMatch(A->left, B->left) && SameTypes(A->right, B->right);
}

/* check to see if array type A is compatible with tuple type T */
static int ArrayCompatibleWithTuple(AST *A, AST *T) {
    int n = 0; // length of tuple
    AST *arrayTyp = A->left;
    AST *arrayLen = A->right;
    AST *Qtyp;
    AST *Q;
    Q = T;
    while (Q) {
        Qtyp = (Q->kind == AST_TUPLE_TYPE) ? Q->left : Q;
        if (!arrayTyp) {
            arrayTyp = Qtyp;
        }
        if (!SameTypes(arrayTyp, Qtyp)) {
            return 0;
        }
        n++;
        if (Q->kind == AST_TUPLE_TYPE) {
            Q = Q->right;
        } else {
            Q = NULL;
        }
    }
    // check for lengths compatible
    if (arrayLen) {
        int expectLen = EvalConstExpr(arrayLen);
        if (expectLen != n) {
            return 0;
        }
    }
    return 1;
}

/* check for compatibility of types */
/* NOTE: this is not symmetric; we allow A to be of stricter type
 * than B (so for example passing type B to type A is OK if B
 * is a "char *" and A is a "const char *", but not vice-versa)
 */
int
CompatibleTypes(AST *A, AST *B)
{
    bool isSpin = (curfunc != NULL && IsSpinLang(curfunc->language));
    bool skipfloats = isSpin;

    A = RemoveTypeModifiers(A);
    B = RemoveTypeModifiers(B);

    if (!A || (skipfloats && A->kind == AST_FLOATTYPE)) {
        A = ast_type_generic;
    }
    if (!B || (skipfloats && B->kind == AST_FLOATTYPE)) {
        B = ast_type_generic;
    }

    if (A == B) return 1;

    if (A && A->kind == AST_TUPLE_TYPE) {
        if (B && B->kind == AST_TUPLE_TYPE) {
            return CompatibleTypes(A->left, B->left) && CompatibleTypes(A->right, B->right);
        } else if (B && B->kind == AST_OBJECT) {
            /* special hack: tuple types should be compatible with
             * any object of the same size
             */
            return TypeSize(A) == TypeSize(B);
        } else if (B && B->kind == AST_ARRAYTYPE) {
            // arrays and tuples are compatible if all tuple components are
            // the same, and the lengths match
            return ArrayCompatibleWithTuple(B, A);
        } else if (A->right) {
            return 0;
        } else {
            return CompatibleTypes(A->left, B);
        }
    } else if (B && B->kind == AST_TUPLE_TYPE) {
        if (A && A->kind == AST_OBJECT) {
            return TypeSize(A) == TypeSize(B);
        } else if (A && A->kind == AST_ARRAYTYPE) {
            // arrays and tuples are compatible if all tuple components are
            // the same, and the lengths match
            return ArrayCompatibleWithTuple(A, B);
        }
        if (B->right) {
            return 0;
        }
        return CompatibleTypes(B->left, A);
    }

    if (A->kind == AST_INTTYPE || A->kind == AST_UNSIGNEDTYPE || A->kind == AST_GENERICTYPE || A->kind == AST_UNS_BOOLTYPE || A->kind == AST_SIGNED_BOOLTYPE) {
        if (B->kind == AST_INTTYPE || B->kind == AST_UNSIGNEDTYPE || B->kind == AST_GENERICTYPE || B->kind == AST_UNS_BOOLTYPE || B->kind == AST_SIGNED_BOOLTYPE) {
            return true;
        }
    }
    if ( !isSpin && (A->kind == AST_GENERICTYPE || B->kind == AST_GENERICTYPE)) {
        return 1;
    }
    if (A->kind != B->kind) {
        if (A->kind == AST_COPYREFTYPE && B->kind == AST_REFTYPE) {
            /* ok */
        } else if (A->kind == AST_REFTYPE && B->kind == AST_COPYREFTYPE) {
            /* ok */
        } else {
            return 0;
        }
    }
    if (IsRefType(A) && IsRefType(B)) {
        return CompatibleTypes(A->left, B->left);
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
    if ( A->kind == AST_PTRTYPE && B->kind == AST_PTRTYPE) {
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
            if (rhs && rhs->kind == AST_OPERATOR && rhs->d.ival == K_NEGATE) {
                return rhs->right;
            }
        } else if (op == '+' || op == '-') {
            int oppositeOp = (op == '+') ? '-' : '+';
            // (A+N)-N => A, similarly for (A-N)+N
            if (lhs && lhs->kind == AST_OPERATOR && lhs->d.ival == oppositeOp) {
                if (IsConstExpr(rhs) && AstMatch(lhs->right, rhs)) {
                    return lhs->left;
                }
            } else if (rhs && rhs->kind == AST_INTEGER && rhs->d.ival == 0) {
                return lhs;
            }
        }
        // optimize integer expressions
        if (rhs && rhs->kind == AST_INTEGER) {
            if ( (rhs->d.ival == 0) && ((op == '+') || (op == '-')) ) {
                return lhs;
            }
            if (lhs && lhs->kind == AST_INTEGER) {
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
    case AST_LOCAL_IDENTIFIER:
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
    functype = RemoveTypeModifiers(functype);
    while (functype && functype->kind == AST_PTRTYPE) {
        functype = functype->left;
    }
    if (!IsFunctionType(functype)) {
        // if this is Spin2, assume it is a void type (there would be
        // an explicit :1 after it otherwise)
        if (GetCurrentLang() == LANG_SPIN_SPIN2) {
            return 0;
        }
        // otherwise punt and assume just 1 return type; this works for
        // many Spin1 functions, and C and BASIC should have proper function
        // types anyway
        return 1; // unknown return type
    }
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
    if (functype && functype->kind == AST_TUPLE_TYPE) {
        AST *subtype;
        int n = 0;
        while (functype && functype->kind == AST_TUPLE_TYPE) {
            subtype = functype->left;
            functype = functype->right;
            n += (TypeSize(subtype)+3)/4;
        }
        return n;
    }
    return (TypeSize(functype) + 3) / 4;
}

int
FuncLongResults(AST *functype)
{
    return FuncNumResults(functype);
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

int
FuncLongParams(AST *functype)
{
    AST *arglist = functype->right;
    AST *arg;
    int n = 0;
    while (arglist) {
        arg = arglist->left;
        arglist = arglist->right;
        if (arg->kind == AST_VARARGS) {
            n += 1;
            break;
        }
        n += (TypeSize(ExprType(arg)) + 3) / 4;
    }
    return n;
}

//
// create an array type
//
AST *
MakeArrayType(AST *basetype, AST *exprlist)
{
    if (!exprlist) {
        return basetype;
    }
    if (exprlist->kind == AST_EXPRLIST) {
        basetype = MakeArrayType(basetype, exprlist->right);
        exprlist = exprlist->left;
    }
    AST *siz = exprlist;
    AST *base = NULL;
    if (siz->kind == AST_RANGE) {
        base = siz->left;
        siz = siz->right;
        siz = AstOperator('-', siz, AstOperator('-', base, AstInteger(1)));
    }
    AST *typ = NewAST(AST_ARRAYTYPE, basetype, siz);
    typ->d.ptr = (void *)base;
    return typ;
}

//
// clean up a type definition so it has no symbols in it, only
// constants
// this is required because we may need a subtype from an object,
// but constants should be evaluated in the context of the object itself
//
AST *
CleanupType(AST *typ)
{
    AST *idx;
    int n;

    if (!typ) return typ;
    switch (typ->kind) {
    case AST_ARRAYTYPE:
        idx = typ->right;
        if (idx && idx->kind != AST_INTEGER) {
            if (IsConstExpr(idx)) {
                n = EvalConstExpr(idx);
                typ->right = AstInteger(n);
            }
        }
        break;
    case AST_FUNCTYPE:
        typ->left = CleanupType(typ->left);
        break;
    default:
        break;
    }
    return typ;
}

//
// build an AST_EXPRLIST containing method references for each method
// within an object
// "typ" is the object type
//
AST *
BuildExprlistFromObject(AST *origexpr, AST *typ)
{
    AST *exprlist = NULL;
    AST *temp;
    Module *P;
    Symbol *sym;
    ASTReportInfo saveinfo;
    AST *expr = origexpr;
    AST **exprptr = NULL;

    int i;
    int n;

    /* chase down any expression statements */
    while (expr->kind == AST_STMTLIST || expr->kind == AST_SEQUENCE) {
        if (expr->right) {
            exprptr = &expr->right;
        } else {
            exprptr = &expr->left;
        }
        expr = *exprptr;
    }
    /* discard casts */
    while (expr && expr->kind == AST_CAST) {
        exprptr = &expr->right;
        expr = expr->right;
    }
    if (!expr || expr->kind == AST_EXPRLIST || expr->kind == AST_FUNCCALL) {
        /* already an expression list */
        return origexpr;
    }
    if (expr->kind == AST_ASSIGN) {
        exprlist = NewAST(AST_SEQUENCE, expr, expr->left);
        return BuildExprlistFromObject(exprlist, typ);
    }
    exprlist = NULL;
    if (IsScalar64Type(typ)) {
        exprlist = NewAST(AST_EXPRLIST, NewAST(AST_GETLOW, expr, NULL), NULL);
        temp = NewAST(AST_EXPRLIST, NewAST(AST_GETHIGH, expr, NULL), NULL);
        exprlist = AddToList(exprlist, temp);
        if (exprptr) {
            *exprptr = exprlist;
            return origexpr;
        }
        return exprlist;
    }
    if (!IsClassType(typ)) {
        return expr;
    }
    P = GetClassPtr(typ);
    n = TypeSize(typ);
    i = 0;
    AstReportAs(expr, &saveinfo);
    while (i < n) {
        sym = FindSymbolByOffsetAndKind(&P->objsyms, i, SYM_VARIABLE);
        if (!sym || sym->kind != SYM_VARIABLE) {
            ERROR(expr, "Unable to find symbol at offset %d", i);
            break;
        }
        int siz = TypeSize((AST *)sym->v.ptr);
        if (siz == 0) siz = LONG_SIZE;
        temp = NewAST(AST_METHODREF, expr, AstIdentifier(sym->our_name));
        temp = NewAST(AST_EXPRLIST, temp, NULL);
        exprlist = AddToList(exprlist, temp);
        i += siz;
    }
    AstReportDone(&saveinfo);
    if (exprptr) {
        *exprptr = exprlist;
        return origexpr;
    }
    return exprlist;
}

/* pull a single element of type "type" out of a list, and update that list */
/* needs to be able to handle nested arrays, so if "type" is an array type then
 * call recursively
 */
AST *PullElement(AST *type, AST **rawlist_ptr)
{
    AST *item = NULL;
    AST *rawlist = *rawlist_ptr;

    if (!rawlist) return NULL;
    if (IsArrayType(type)) {
        int numelems;
        int elemsize;
        int i;
        int typesize = TypeSize(type);
        type = RemoveTypeModifiers(BaseType(type));
        elemsize = TypeSize(type);
        numelems = typesize / elemsize;
        if (!numelems) {
            return NULL;
        }
        for (i = 0; i < numelems; i++) {
            item = AddToList(item, PullElement(type, rawlist_ptr));
        }
        return item;
    } else if (IsClassType(type)) {
        AST *varlist;
        AST *elem;
        AST *subtype;
        int sawBitfield = 0;
        Module *P = (Module *)type->d.ptr;
        if (P->pendingvarblock) {
            ERROR(type, "Internal error, pending variables on object");
            return NULL;
        }
        varlist = P->finalvarblock;
        while (varlist) {
            elem = varlist->left;
            varlist = varlist->right;
            if (elem->kind == AST_DECLARE_BITFIELD) {
                sawBitfield = 1;
                subtype = ast_type_long;
                item = AddToList(item, PullElement(subtype, rawlist_ptr));
            } else if (sawBitfield) {
                sawBitfield = 0;
            } else {
                subtype = ExprType(elem);
                item = AddToList(item, PullElement(subtype, rawlist_ptr));
            }
            if (P->isUnion) {
                break;
            }
        }
        return item;
    }
    if (rawlist) {
        item = rawlist->left;
        rawlist = rawlist->right;
    } else {
        item = AstInteger(0);
    }
    item = NewAST(AST_EXPRLIST, item, NULL);
    *rawlist_ptr = rawlist;
    return item;
}

/* find identifier "ident" in variable list "list" */
/* if curptr != NULL, set it to the index */
AST *FindMethodInList(AST *list, AST *ident, int *curptr)
{
    int curelem = 0;
    if (!IsIdentifier(ident)) {
        ERROR(ident, "Expected identifier");
        return NULL;
    }
    if (ident->kind == AST_LOCAL_IDENTIFIER) {
        // look for the "raw" form
        ident = ident->right;
    }
    while (list) {
        if (AstUses(list->left, ident)) {
            break;
        }
        list = list->right;
        curelem++;
    }
    if (!list) return NULL;
    if (curptr) *curptr = curelem;
    return list;
}

/* fix up an initializer list of a given type */
/* creates an array containing the initializer expressions;
 * each of these may in turn be an array of initializers
 * returns an EXPRLIST containing the items
 * returns NULL if list is empty
 */
AST *
FixupInitList(AST *type, AST *initval)
{
    int numelems;
    AST **astarr = 0;
    int curelem;
    AST *origtype = type;

    if (!initval) {
        return initval;
    }
    type = RemoveTypeModifiers(type);
    //typealign = TypeAlign(type);
    numelems  = 1;

    if (initval->kind == AST_STRINGPTR) {
        return initval;
    }
    if (initval->kind != AST_EXPRLIST) {
        initval = NewAST(AST_EXPRLIST, initval, NULL);
    }

    type = RemoveTypeModifiers(type);
    switch(type->kind) {
    case AST_ARRAYTYPE:
    {
        type = RemoveTypeModifiers(BaseType(type));
        /* if the first element is not an initializer list, then
           assume we've got a flat initializer like
             int a[2][3] = { 1, 2, 3, 4, 5, 6 }
           we need to convert this to { {1, 2, 3}, {4, 5, 6} }
        */
        if ( (IsArrayType(type) || IsClassType(type) ) && initval->left && initval->left->kind != AST_EXPRLIST
                && !(initval->left->kind == AST_STRINGPTR && IsArrayType(type)) ) {
            AST *rawlist = initval;
            AST *item;
            initval = NULL;
            numelems = 0;
            for (;;) {
                item = PullElement(type, &rawlist);
                if (!item) break;
                item = NewAST(AST_EXPRLIST, item, NULL);
                initval = AddToList(initval, item);
                numelems++;
            }
        } else {
            if (origtype->right) {
                numelems = EvalConstExpr(origtype->right);
            } else {
                numelems = AstListLen(initval);
            }
        }
        astarr = (AST **) calloc( numelems, sizeof(AST *) );
        if (!astarr) {
            ERROR(NULL, "out of memory");
            return 0;
        }
        curelem = 0;
        while (initval) {
            AST *val = initval;
            initval = initval->right;
            val->right = NULL;
            if (val->left->kind == AST_INITMODIFIER) {
                AST *root = val->left;
                AST *fixup = root->left;
                AST *newval = root->right;
                if (!fixup || fixup->kind != AST_ARRAYREF) {
                    ERROR(fixup, "initialization designator for array is not an array element");
                    val->left = AstInteger(0);
                } else if (fixup->left != NULL) {
                    ERROR(fixup, "Internal error, cannot handle nested designators");
                    val->left = AstInteger(0);
                } else if (!IsConstExpr(fixup->right)) {
                    ERROR(fixup, "initialization designator for array is not constant");
                    val->left = AstInteger(0);
                } else {
                    curelem = EvalConstExpr(fixup->right);
                    val->left = newval;
                }
            }
            if (curelem < numelems) {
                if (astarr[curelem]) {
                    // C99 says the last definition applies, so this probably isn't
                    // an error
                    WARNING(val, "Duplicate definition for element %d of array", curelem);
                }
                astarr[curelem] = val;
                curelem++;
            } else {
                WARNING(val, "Extra initializers for array");
            }
        }
        break;
    }
    case AST_OBJECT:
    {
        Module *P = GetClassPtr(type);
        int is_union = P->isUnion;
        AST *varlist = P->finalvarblock;
        if (!varlist) {
            return NULL;
        }
        numelems = AggregateCount(type);
        astarr = (AST **) calloc( numelems, sizeof(AST *) );
        if (!astarr) {
            ERROR(NULL, "out of memory");
            return 0;
        }
        /* now pull out the initializers */
        curelem = 0;
        while (initval) {
            AST *val = initval;
            initval = initval->right;
            val->right = NULL;
            if (val->left->kind == AST_INITMODIFIER) {
                AST *root = val->left;
                AST *fixup = root->left;
                AST *newval = root->right;
                if (!fixup || fixup->kind != AST_METHODREF) {
                    ERROR(fixup, "initialization designator for struct is not a method reference");
                    newval = AstInteger(0);
                } else if (fixup->left != NULL) {
                    ERROR(fixup, "Internal error, cannot handle nested designators");
                    newval = AstInteger(0);
                } else {
                    varlist = FindMethodInList(P->finalvarblock, fixup->right, &curelem);
                    if (!varlist) {
                        ERROR(fixup, "%s not found in struct", GetUserIdentifierName(fixup->right));
                        break;
                    }
                }
                val->left = newval;
            }
            if (is_union) {
                curelem = 0;
            }
            if (is_union && !(val->left && val->left->kind == AST_CAST)) {
                AST *subtype = ExprType(varlist->left);
                val->left = NewAST(AST_CAST, subtype, val->left);
            }
            if (curelem < numelems) {
                astarr[curelem] = val;
                curelem++;
                varlist = varlist->right;
            } else {
                WARNING(val, "ignoring excess element in initializer");
            }
        }
        break;
    }
    default:
        return initval;
    }

    for (curelem = 0; curelem < numelems; curelem++) {
        if (!astarr[curelem]) {
            astarr[curelem] = NewAST(AST_EXPRLIST, AstInteger(0), NULL);
        }
    }
    initval = astarr[0];
    for (curelem = 0; curelem < numelems-1; curelem++) {
        astarr[curelem]->right = astarr[curelem+1];
    }
    free(astarr);
    return initval;
}

/* get number of elements in an array or class */
int
AggregateCount(AST *typ)
{
    int size;
    int sawBitfield = 0;
    if (IsArrayType(typ)) {
        if (!IsConstExpr(typ->right)) {
            ERROR(typ, "Unable to determine size of array");
            size = 1;
        } else {
            size = EvalConstExpr(typ->right);
        }
        return size;
    }
    if (IsClassType(typ)) {
        Module *P = (Module *)typ->d.ptr;
        AST *list, *elem;
        if (P->isUnion) {
            return 1;
        }
        if (P->pendingvarblock) {
            ERROR(typ, "Internal error, Taking size of an object with pending variables\n");
        }
        list = P->finalvarblock;
        size = 0;
        while (list) {
            elem = list->left;
            if (elem->kind == AST_DECLARE_BITFIELD) {
                sawBitfield = 1;
                size++;
            } else {
                if (!sawBitfield) {
                    size++;
                }
                sawBitfield = 0;
            }
            list = list->right;
        }
        return size;
    }
    ERROR(typ, "Internal error, expected aggregate type in AggregateCount");
    return 1;
}

/* utility: construct a name for a type */
const char *TypeName(AST *typ)
{
    static char buf[1024];
    int size;
    int lang;
    int isUnsigned = 0;
    AST *nexttyp;
    int needMore = 0;

    buf[0] = 0;
    if (!typ) {
        return "generic type";
    }
    if (curfunc) {
        lang = curfunc->language;
    } else if (current) {
        lang = current->curLanguage;
    } else {
        lang = LANG_DEFAULT;
    }
    while (typ) {
        if (typ == ast_type_string) {
            strcat(buf, "string ");
            nexttyp = NULL;
        }
        if (typ == ast_type_void) {
            strcat(buf, "void ");
            nexttyp = NULL;
        }
        nexttyp = typ->left;
        needMore = 0;
        switch (typ->kind) {
        case AST_MODIFIER_SEND_ARGS:
            break;
        case AST_MODIFIER_CONST:
            strcat(buf, "const ");
            break;
        case AST_MODIFIER_VOLATILE:
            strcat(buf, "volatile ");
            break;
        case AST_FUNCTYPE:
            size = NumArgsForType(typ);
            sprintf(buf, "function of %d args returning ", size);
            needMore = 1;
            break;
        case AST_ARRAYTYPE:
            strcat(buf, "array of ");
            needMore = 1;
            break;
        case AST_PTRTYPE:
            strcat(buf, "pointer to ");
            needMore = 1;
            break;
        case AST_REFTYPE:
            strcat(buf, "reference to ");
            needMore = 1;
            break;
        case AST_GENERICTYPE:
            strcat(buf, "any ");
            break;
        case AST_UNSIGNEDTYPE:
            isUnsigned = 1;
        /* fall through */
        case AST_INTTYPE:
            size = EvalPasmExpr(typ->left);
            switch (size) {
            case 1:
                if (!isUnsigned) {
                    strcat(buf, "signed ");
                }
                if (IsCLang(lang)) {
                    strcat(buf, "char");
                } else {
                    strcat(buf, "byte");
                }
                break;
            case 2:
                if (IsSpinLang(lang)) {
                    if (!isUnsigned) {
                        strcat(buf, "signed ");
                    } else {
                        if (isUnsigned) {
                            strcat(buf, "unsigned ");
                        }
                    }
                }
                if (IsCLang(lang)) {
                    strcat(buf, "short ");
                } else {
                    strcat(buf, "word ");
                }
                break;
            case 8:
                if (isUnsigned) {
                    strcat(buf, "unsigned ");
                }
                strcat(buf, "64 bit integer ");
                break;
            default:
                if (isUnsigned) {
                    strcat(buf, "unsigned ");
                }
                if (IsCLang(lang)) {
                    strcat(buf, "int ");
                } else if (IsSpinLang(lang)) {
                    strcat(buf, "long ");
                } else {
                    strcat(buf, "integer ");
                }
                break;
            }
            nexttyp = NULL;
            break;
        case AST_FLOATTYPE:
            size = EvalPasmExpr(typ->left);
            switch (size) {
            case 8:
                strcat(buf, "double precision float ");
                break;
            default:
                strcat(buf, "single precision float ");
                break;
            }
            nexttyp = NULL;
            break;
        case AST_TUPLE_TYPE:
            sprintf(buf, "list of %d values ", AstListLen(typ));
            nexttyp = NULL;
            break;
        case AST_OBJECT:
        {
            Module *P = GetClassPtr(typ);
            strcat(buf, P->classname);
            nexttyp = NULL;
            break;
        }
        default:
            strcat(buf, "unknown type ");
            nexttyp = NULL;
            break;
        }
        typ = nexttyp;
    }
    if (needMore) {
        strcat(buf, "any");
    }
    return strdup(buf);
}

bool IsConstZero(AST *ast) {
    return IsConstEqual(ast,0);
}

bool IsConstEqual(AST *ast, ExprInt val) {
    return IsConstExpr(ast) && EvalConstExpr(ast) == val;
}

// dereference any AST_TYPEOF
AST *DerefType(AST *typ) {
    if (typ && typ->kind == AST_TYPEOF) {
        return ExprType(typ->left);
    }
    return typ;
}

int32_t const_or_default(Module *M, const char *name, int32_t defaultval) {
    Symbol *sym = FindSymbol(&M->objsyms,name);
    if (sym && sym->kind == SYM_CONSTANT) {
        return EvalConstExpr((AST *)sym->v.ptr);
    } else {
        return defaultval;
    }
}
