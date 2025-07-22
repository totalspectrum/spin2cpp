/*
 * Spin to C/C++ translator
 * Copyright 2011-2025 Total Spectrum Software Inc.
 * Copyright 2021-2024 Ada Gottensträter
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
#include <assert.h>
#include "common.h"
#include "becommon.h"

static AST *make_methodptr;
static AST *get_rawfuncaddr;

AST *
BuildMethodPointer(AST *ast, bool is_abs)
{
    Symbol *sym;
    AST *objast;
    AST *funcaddr;
    AST *result;
    AST *call;
    Function *func;
    AST *funcptrtype;
    
    sym = FindCalledFuncSymbol(ast, &objast, 0);
    if (!sym || sym->kind != SYM_FUNCTION) {
        if (sym) {
            ERROR(ast, "%s is not a function", sym->user_name);
        } else {
            ERROR(ast, "Internal error, unable to find function address");
        }
        return ast;
    }
    func = (Function *)sym->v.ptr;
    if (func->is_static) {
        objast = AstInteger(0);
    } else if (objast == NULL) {
        objast = NewAST(AST_SELF, NULL, NULL);
    } else {
        objast = NewAST(AST_ADDROF, objast, NULL);
    }
    funcptrtype = NewAST(AST_PTRTYPE, func->overalltype, NULL);
    AddIndirectFunctionCall(func, false);
    if (func->callSites == 0) {
        MarkUsed(func, "func pointer");
    }
    // save off the current @ node
    if (IndexedMethodPtrs()) {
        funcaddr = AstInteger(func->method_index);
    } else {
        funcaddr = NewAST(AST_ADDROF, ast->left, ast->right);
    }
    // create a call
    if (!make_methodptr) {
        make_methodptr = AstIdentifier("_make_methodptr");
    }
    if (!get_rawfuncaddr) {
        get_rawfuncaddr = AstIdentifier("_get_rawfuncaddr");
    }
    call = NewAST(AST_EXPRLIST, funcaddr, NULL);
    call = NewAST(AST_EXPRLIST, objast, call);
    if (is_abs) {
        result = NewAST(AST_FUNCCALL, get_rawfuncaddr, call);
    } else {
        result = NewAST(AST_FUNCCALL, make_methodptr, call);
    }
    // cast the result to the correct function pointer type
    result = NewAST(AST_CAST, funcptrtype, result);
    return result;
}

bool interp_can_multireturn() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return false;
    case INTERP_KIND_NUCODE: return true;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return false;
    }
}

bool interp_can_unsigned() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return false;
    case INTERP_KIND_NUCODE: return true;
    default:
        ERROR(NULL,"Unknown interpreter kind");
        return false;
    }
}

static bool interp_prefers_decode() {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM: return true;
    default:
        return false;
    }
}

// get number of parameters
// adjust for possible varargs (indicated by negative)
static int BCGetNumParams(Function *F) {
    int n = F->numparams;
    return n >= 0 ? n : (-n)+1;
}

static int DefaultGetNumParams(Function *F) {
    int n = F->numparams;
    return n >= 0 ? n : (-n);
}

// find base for result variables
static int resultOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        if (offset == 0) return 0;
        return 4 + (BCGetNumParams(F)*4) + (offset-4);
    case INTERP_KIND_NUCODE:
        return offset + (BCGetNumParams(F)+4)*LONG_SIZE ;
    default:
        return offset;
    }
}

// number of results for Spin1 bytecode purposes
static int BCGetNumResults(Function *F) {
    int n = F->numresults;
    return (n<=1) ? 1 : n;
}

// number of results for Spin2/Nu code bytecode purposes
static int DefaultGetNumResults(Function *F) {
    int n = F->numresults;
    return n;
}

// find base for parameter variables
static int paramOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        return 4 + offset; // always one result pushed onto stack
    case INTERP_KIND_NUCODE:
        return offset;
    default:
        return offset + DefaultGetNumResults(F)*4;
    }
}

// find base for local variables
static int localOffset(Function *F, int offset) {
    switch(gl_interp_kind) {
    case INTERP_KIND_P1ROM:
        return offset + (BCGetNumResults(F)+ BCGetNumParams(F))*4; // FIXME small variables
    case INTERP_KIND_NUCODE:
        return offset + (DefaultGetNumResults(F)+BCGetNumParams(F)+4)*4;
    default:
        return offset + (DefaultGetNumResults(F) + DefaultGetNumParams(F))*4;
    }
}

//
// normalize offsets for Spin compatibility
// In Spin, function variables must be laid out with
// results first, then parameters, then locals.
// This function resets all variable offsets to give the
// correct values.
//
static int resetOffsets(Symbol *sym, void *arg)
{
    Function *F = (Function *)arg;
    
    switch (sym->kind) {
    case SYM_RESULT:
        sym->offset = resultOffset(F, sym->offset);
        break;
    case SYM_PARAMETER:
        sym->offset = paramOffset(F, sym->offset);
        break;
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
        sym->offset = localOffset(F, sym->offset);
        break;
    default:
        /* nothing to do */
        return 1;
    }
    return 1;
}

/* convert offsets in local variables to their canonical values, depending on
 * how the interpreter wants them laid out
 */
void
NormalizeVarOffsets(Function *F)
{
    IterateOverSymbols(&F->localsyms, resetOffsets, (void *)F);
}

/*
 * evaluate any constant expressions within a string
 */
AST *
EvalStringConst(AST *expr)
{
    if (!expr) {
        return expr;
    }
    switch (expr->kind) {
    case AST_EXPRLIST:
        return NewAST(AST_EXPRLIST, EvalStringConst(expr->left), EvalStringConst(expr->right));
    case AST_STRING:
    case AST_INTEGER:
        return expr;
    default:
        if (IsConstExpr(expr)) {
            return AstInteger(EvalConstExpr(expr));
        } else {
            return expr;
        }
    }
}

/* create a zero terminated string, or count prepended string */
/* "lenVal" is the number of count bytes, 0 for a zero terminated string */
AST *
EvalTerminatedStringConst(AST *expr, int lenVal)
{
    unsigned stringLength = 0;
    unsigned encodeLength;
    unsigned byteMask;
    int i;

    expr = EvalStringConst(expr);
    if (expr && expr->kind != AST_EXPRLIST) {
        expr = NewAST(AST_EXPRLIST, expr, NULL);
    }
    if (lenVal <= 0) {
        // zero terminate the string
        expr = AddToList(expr, NewAST(AST_EXPRLIST, AstInteger(0), NULL));
    } else {
        // prefix the string with a length which is "lenVal" bytes long
        lenVal *= 8; // convert to bit count
        stringLength = AstStringLen(expr) - 1; // AstStringLen includes a trailing 0
        if (lenVal < LONG_SIZE*8) {
            byteMask = (1<<lenVal) - 1;
            encodeLength = stringLength & byteMask;
            if (encodeLength != stringLength) {
                ERROR(expr, "String length does not fit in %d bytes", lenVal);
            }
        } else {
            encodeLength = stringLength;
        }
        // construct little endian prefix
        for (i = lenVal-8; i >= 0; i -= 8) {
            expr = NewAST(AST_EXPRLIST, AstInteger( (encodeLength >> i) & 0xff), expr);
        }
    }
    return expr;
}

/* append a string */
static void StringAppend(Flexbuf *fb,AST *expr) {
    if(!expr) return;
    switch (expr->kind) {
    case AST_INTEGER: {
        int i = expr->d.ival;
        if (i < 0 || i>255) ERROR(expr,"Character out of range!");
        flexbuf_putc(i,fb);
    } break;
    case AST_STRING: {
        flexbuf_addstr(fb,expr->d.string);
    } break;
    case AST_EXPRLIST: {
        if (expr->left) StringAppend(fb,expr->left);
        if (expr->right) StringAppend(fb,expr->right);
    } break;
    default: {
        if (IsConstExpr(expr)) {
            int i = EvalConstExpr(expr);
            if (i < 0 || i>255) ERROR(expr,"Character out of range!");
            flexbuf_putc(i,fb);
        } else {
            ERROR(expr,"String expression is not constant");
        }
    } break;
    }
}

void StringBuildBuffer(Flexbuf *fb, AST *expr, int lenPrefix) {
    if (lenPrefix) {
        int actualLen = AstStringLen(expr) - 1; // AstStringLen includes a trailing 0
        int i;
        if (lenPrefix < LONG_SIZE) {
            int maxLen = (1<<(lenPrefix*8));
            if (maxLen <= actualLen) {
                ERROR(expr, "String length does not fit in %d bytes", lenPrefix);
            }
        }
        for (i = 0; i < lenPrefix; i++) {
            flexbuf_addchar(fb, actualLen & 0xff);
            actualLen = actualLen >> 8;
        }
    }   
    StringAppend(fb, expr);
    if (lenPrefix == 0) {
        // zero terminate
        flexbuf_addchar(fb, 0);
    }
}

// Printf that auto-allocates some space (and never frees it, lol)
char *auto_printf(size_t max,const char *format,...) {
    char *buffer = (char *)malloc(max);
    va_list args;
    va_start(args, format);
    vsnprintf(buffer,max,format,args);
    va_end(args);
    return buffer;
}

/* find the backend name for a symbol */
const char *BackendNameForSymbol(Symbol *sym) {
    Module *Q = (Module *)(sym->module ? sym->module : NULL);
    if (NuBytecodeOutput()) {
        return NuCodeSymbolName(sym);
    }
    return IdentifierModuleName(Q, sym->our_name);
}

/*
 * utility function for visiting modules and doing things to them (used by back ends primarily)
 */
int
VisitRecursive(void *vptr, Module *P, VisitorFunc func, unsigned visitval)
{
    Module *Q;
    AST *subobj;
    Module *save = current;
    Function *savecurf = curfunc;
    int change = 0;
    
    if (P->all_visitflags & visitval)
        return change;  // already visited this one

    current = P;

    P->all_visitflags |= visitval;
    P->visitFlag = visitval;
    change |= (*func)(vptr, P);

    // compile intermediate code for submodules
    for (subobj = P->objblock; subobj; subobj = subobj->right) {
        if (subobj->kind != AST_OBJECT) {
            ERROR(subobj, "Internal Error: Expecting object AST");
            break;
        }
        Q = (Module *)subobj->d.ptr;
        change |= VisitRecursive(vptr, Q, func, visitval);
    }
    // and for sub-submodules
    for (Q = P->subclasses; Q; Q = Q->subclasses) {
        change |= VisitRecursive(vptr, Q, func, visitval);
    }
    current = save;
    curfunc = savecurf;
    return change;
}

bool isBoolOperator(int optoken) {
    switch (optoken) {
    case K_BOOL_AND:
    case K_BOOL_NOT:
    case K_BOOL_OR:
    case K_BOOL_XOR:
    case K_LOGIC_AND:
    case K_LOGIC_OR:
    case K_LOGIC_XOR:
        return true;
    default:
        return false;
    }
}

static bool OptNestedAdd(AST **node, int32_t *outVal) {
    if ((*node)->kind != AST_OPERATOR) return false;
    if (IsConstExpr(*node)) return false;
    if ((*node)->d.ival == '+' && IsConstExpr((*node)->left)) {
        *outVal = EvalConstExpr((*node)->left);
        *node = (*node)->right;
        return true;
    } else if ((*node)->d.ival == '+' && IsConstExpr((*node)->right)) {
        *outVal = EvalConstExpr((*node)->right);
        *node = (*node)->left;
        return true;
    } else if ((*node)->d.ival == '-' && IsConstExpr((*node)->right)) {
        *outVal = 0-EvalConstExpr((*node)->right);
        *node = (*node)->left;
        return true;
    }
    return false;
}

bool OptimizeOperator(int *optoken, AST **left,AST **right) {
    ASTReportInfo save;
    int32_t addValue;

    // Try special cases first
    if (*optoken == K_SHL && left && IsConstEqual(*left,1) && interp_prefers_decode()) {
        // 1<<x can be |<x
        *left = NULL;
        *optoken = K_DECODE;
        return true;
    }
    if (*optoken == K_ZEROEXTEND && right && IsConstExpr(*right)) {
        int32_t x = EvalConstExpr(*right);
        if (x >= 32) {
            x = -1; // TODO: Is this actually correct?
        } else {
            x = (1<<(x))-1;
        }
        *right = AstInteger(x);
        *optoken = '&';
    }
    if (*optoken == K_LIMITMIN && right && IsConstZero(*right) && left && CanUseEitherSignedOrUnsigned(*left)) {
        // Remove pointless limitmin
        *optoken = '+';
        *right = AstInteger(0);
    }
    // Handle nested add/sub
    try_addopt_again:
    if (*optoken == '+' && right && IsConstExpr(*right) && left && OptNestedAdd(left,&addValue)) {
        *right = AstInteger(EvalConstExpr(*right)+addValue);
        goto try_addopt_again;
    }
    if (*optoken == '+' && left && IsConstExpr(*left) && right && OptNestedAdd(right,&addValue)) {
        *left = AstInteger(EvalConstExpr(*left)+addValue);
        goto try_addopt_again;
    }
    if (*optoken == '-' && right && IsConstExpr(*right) && left && OptNestedAdd(left,&addValue)) {
        // (x+2) - 3 can be x - 1
        *right = AstInteger(EvalConstExpr(*right)-addValue);
        goto try_addopt_again;
    }
    if (*optoken == '-'  && left && IsConstExpr(*left) && right && OptNestedAdd(right,&addValue)) {
        // 3 - (x+2) can be 1 - x
        *left = AstInteger(EvalConstExpr(*left)-addValue);
        goto try_addopt_again;
    }
    // Handle x != 0 in boolean operators
    if (isBoolOperator(*optoken) && left && *left && (*left)->kind == AST_OPERATOR && (*left)->d.ival == K_NE) {
        if (IsConstZero((*left)->left)) *left = (*left)->right;
        else if (IsConstZero((*left)->right)) *left = (*left)->left;
    }
    if (isBoolOperator(*optoken) && right && *right && (*right)->kind == AST_OPERATOR && (*right)->d.ival == K_NE) {
        if (IsConstZero((*right)->left)) *right = (*right)->right;
        else if (IsConstZero((*right)->right)) *right = (*right)->left;
    }

    int shiftOptOp = 0;
    bool canNopOpt = false;
    int32_t nopOptVal = 0;
    bool canZeroOpt = false;
    int32_t zeroOptVal;
    bool canCommute = false;
    int commuteOp = 0;
    int negateOp = 0;
    int unsignedToSignedOp = 0;

    switch (*optoken) {
    case '*':
        canCommute = true;
        shiftOptOp = K_SHL;
        canNopOpt = true;
        nopOptVal = 1;
        canZeroOpt = true;
        zeroOptVal = 0;
        break;
    case '/':
        // Replacing with SAR doesn't work...
        canNopOpt = true;
        nopOptVal = 1;
        break;
    case K_UNS_DIV:
        shiftOptOp = K_SHR;
        unsignedToSignedOp = '/';
        break;
    case K_SHL:
    case K_SHR:
    case K_SAR:
    case K_ROTL:
    case K_ROTR:
        canNopOpt = true;
        nopOptVal = 0;
        break;
    case K_LIMITMAX_UNS:
        unsignedToSignedOp = K_LIMITMAX;
        break;
    case K_LIMITMIN_UNS:
        canNopOpt = true;
        nopOptVal = 0;
        unsignedToSignedOp = K_LIMITMIN;
        break;
    case K_LTU: unsignedToSignedOp = '<'; commuteOp = K_GTU; break;
    case K_GTU: unsignedToSignedOp = '>'; commuteOp = K_LTU; break;
    case K_LEU: unsignedToSignedOp = K_LE; commuteOp = K_GEU; break;
    case K_GEU: unsignedToSignedOp = K_GE; commuteOp = K_LEU; break;
    case '-':
        canNopOpt = true;
        nopOptVal = 0;
        negateOp = '+';
        break;
    case '+':
        canCommute = true;
        negateOp = '-';
        break;
    case '&':
        canCommute = true;
        canNopOpt = true;
        nopOptVal = -1;
        canZeroOpt = true;
        zeroOptVal = 0;
        break;
    case '|':
    case '^':
        canCommute = true;
        canNopOpt = true;
        nopOptVal = 0;
        break;
    case K_EQ:
    case K_NE:
        canCommute = true;
        break;
    default: return false;
    }

    if (commuteOp) canCommute = true;
    else if (canCommute && !commuteOp) commuteOp = *optoken;
    // Try commuting constant to the right (mostly just makes the code simpler)
    if (canCommute && left && right && IsConstExpr(*left) && !IsConstExpr(*right)) {
        int uncommutedOp = *optoken;
        AST *swap = *left;
        *left = *right;
        *right = swap;
        *optoken = commuteOp;
        if (OptimizeOperator(optoken,left,right)) return true;
        // Undo swap if we couldn't do anything (fixes "waitcnt(381+cnt)")
        // TODO: Can we only do this when actually neccessary?
        //       Because having the constant on the right is better for IR-level optimization (write+read contract)
        swap = *left;
        *left = *right;
        *right = swap;
        *optoken = uncommutedOp;
    }

    if (right && *right && IsConstExpr(*right)) {
        int32_t rightVal = EvalConstExpr(*right);

        if (left && canZeroOpt && rightVal == zeroOptVal && *left && !ExprHasSideEffects(*left)) {
            AstReportAs(*right,&save);
            *optoken = '+';
            *left = AstInteger(0);
            *right = AstInteger(0);
            AstReportDone(&save);
            return true;
        } else if (canNopOpt && rightVal == nopOptVal) {
            if (left && *left && (*left)->kind == AST_OPERATOR) {
                *optoken = (*left)->d.ival;
                *right = (*left)->right;
                *left = (*left)->left;
                OptimizeOperator(optoken,left,right);
                return true;
            } else {
                AstReportAs(*right,&save);
                *optoken = '+';
                *right = AstInteger(0);
                AstReportDone(&save);
                return true;
            }
        } else if (shiftOptOp && isPowerOf2(rightVal)) {
            AstReportAs(*right,&save);
            *optoken = shiftOptOp;
            *right = AstInteger(31-clz32(rightVal));
            AstReportDone(&save);
            return true;
        } else if (negateOp && rightVal < 0 && rightVal != INT32_MIN) {
            AstReportAs(*right,&save);
            *optoken = negateOp;
            *right = AstInteger(-rightVal);
            AstReportDone(&save);
            return true;
        } else if (*optoken == K_GTU && rightVal == 0) {
            *optoken = K_NE;
            return true;
        } else if (*optoken == K_EQ && rightVal == 0) {
            *optoken = K_BOOL_NOT;
            *right = *left;
            *left = NULL;
            return true;
        }
    } else if (right) {
        if (negateOp && (*right)->kind == AST_OPERATOR && (*right)->d.ival == K_NEGATE) {
            // L + (-R) can be L - R
            *right = (*right)->right;
            *optoken = negateOp;
            OptimizeOperator(optoken,left,right);
            return true;
        }
    }

    if (unsignedToSignedOp && !interp_can_unsigned() && left && right && CanUseEitherSignedOrUnsigned(*left) && CanUseEitherSignedOrUnsigned(*right)) {
        *optoken = unsignedToSignedOp;
        return true;
    }

    return false;
}

// FIXME: less clunky name
// Basically returns true if the value will be the same when interpreted signed or unsigned
bool CanUseEitherSignedOrUnsigned(AST *node) {
    if (!node) return false;
    if (IsConstExpr(node)) return EvalConstExpr(node) >= 0;
    AST *type = ExprType(node);
    if (type) {
        if (type->kind == AST_UNSIGNEDTYPE && type->left->d.ival < LONG_SIZE) return true;
    }
    if (node->kind == AST_OPERATOR) {
        int32_t constVal;
        bool constRight = false;
        if (IsConstExpr(node->left)) constVal = EvalConstExpr(node->left);
        else if (IsConstExpr(node->right)) constVal = EvalConstExpr(node->right), constRight = true;
        else goto no_constop;

        if (node->d.ival == '&' && constVal >= 0) return true;
        if (node->d.ival == K_SHR && constRight && (constVal & 31)) return true;

    }
    no_constop:
    return false;
}

//
// Decompose val into a sequence of a shift, add/sub
// returns 0 if failure, 1 if success
// sets shifts[0] to final shift
// shifts[1] to +1 for add, -1 for sub, 0 if done
// shifts[2] to initial shift
//
int DecomposeBits(unsigned val, int *shifts)
{
    int shift = 0;
    int temp[3];
    int r;
    
    while (val != 0) {
        if (val & 1) {
            break;
        }
        shift++;
        val = val >> 1;
    }
    shifts[0] = shift;
    if (val == 1) {
        // we had a power of 2
        shifts[1] = 0;
        return 1;
    }
    // OK, can the new val itself be decomposed?
    if (isPowerOf2(val-1)) {
        shifts[1] = +1;
        r = DecomposeBits(val-1, temp);
        shifts[2] = temp[0];
    } else if (isPowerOf2(val+1)) {
        shifts[1] = -1;
        r = DecomposeBits(val+1, temp);
        shifts[2] = temp[0];
    } else {
        return 0;
    }
    return r;
}

FunctionList *
NewFunctionList(Function *f)
{
    FunctionList *entry = (FunctionList *)calloc(1, sizeof(FunctionList));
    entry->func = f;
    return entry;
}

