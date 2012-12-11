#include "spinc.h"
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* code to get an object pointer from an object symbol */
ParserState *
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
LookupSymbol(const char *name)
{
    Symbol *sym = NULL;
    if (curfunc) {
        sym = FindSymbol(&curfunc->localsyms, name);
        if (!sym) {
            sym = FindSymbol(&curfunc->parse->objsyms, name);
        }
    } else {
        sym = FindSymbol(&current->objsyms, name);
    }
    if (!sym) {
        sym = FindSymbol(&reservedWords, name);
    }
    return sym;
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
    ParserState *objstate;

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
    ParserState *objstate;

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

/* code to print a label to a file
 * if "ref" is nonzero this is an array reference, so do not
 * dereference again
 */
void
PrintLabel(FILE *f, Symbol *sym, int ref)
{
    Label *lab;

    if (current->printLabelsVerbatim) {
        if (current->fixImmediate) {
            fprintf(f, "((%s-..start)/4)", sym->name);
        } else {
            fprintf(f, "%s", sym->name);
        }
    } else {
        lab = (Label *)sym->val;
        fprintf(f, "(%s(", ref ? "" : "*");
        PrintType(f, lab->type);
        fprintf(f, " *)&%s[%d])", current->datname, lab->offset);
    }
}

/* code to print an integer */
void
PrintInteger(FILE *f, int32_t v)
{
    if (current->printLabelsVerbatim) {
        if (v > -10 && v < 10) {
            fprintf(f, "%ld", (long)v);
        } else {
            fprintf(f, "$%lx", (long)(uint32_t)v);
        }
    }
    else if (v == (int32_t)0x80000000)
        fprintf(f, "(int32_t)0x%lxU", (long)(uint32_t)v);
    else
        fprintf(f, "%ld", (long)v);
}

/* code to print a float */
void
PrintFloat(FILE *f, int32_t v)
{
    if (v < 0)
        fprintf(f, "(int32_t)0x%lx", (long)(uint32_t)v);
    else
        fprintf(f, "0x%lx", (long)v);
}

/* code to print a symbol to a file */
void
PrintSymbol(FILE *f, Symbol *sym)
{
    switch (sym->type) {
    case SYM_LABEL:
        PrintLabel(f, sym, 0);
        break;
    case SYM_CONSTANT:
        if (IsReservedWord(sym->name)) {
            int32_t v;
            v = EvalConstExpr((AST *)sym->val);
            PrintInteger(f, v);
        } else {
            fprintf(f, "%s", sym->name);
        }
        break;
    case SYM_FLOAT_CONSTANT:
        PrintFloat(f, EvalConstExpr((AST*)sym->val));
        break;
    case SYM_PARAMETER:
        if (curfunc && curfunc->parmarray) {
            fprintf(f, "%s[%d]", curfunc->parmarray, curfunc->result_in_parmarray+sym->count);
        } else {
            fprintf(f, "%s", sym->name);
        }
        break;              
    case SYM_LOCALVAR:
        if (curfunc && curfunc->localarray) {
            fprintf(f, "%s[%d]", curfunc->localarray, sym->count);
        } else {
            fprintf(f, "%s", sym->name);
        }
        break;              
    case SYM_RESULT:
        if (curfunc && curfunc->result_in_parmarray) {
            fprintf(f, "%s[0]", curfunc->parmarray);
        } else {
            fprintf(f, "%s", sym->name);
        }
        break;
    case SYM_VARIABLE:
        if (gl_ccode) {
            fprintf(f, "thisobj.%s", sym->name);
        } else {
            fprintf(f, "%s", sym->name);
        }
        break;
    case SYM_FUNCTION:
    default:
        fprintf(f, "%s", sym->name);
        break;
    }
}

/* code to print a function call to a file */
void
PrintFuncCall(FILE *f, Symbol *sym, AST *params)
{
    fprintf(f, "%s(", sym->name);
    PrintExprList(f, params);
    fprintf(f, ")");
}

/* code to print left operator right
 */
static void
PrintInOp(FILE *f, const char *op, AST *left, AST *right)
{
    if (left && right) {
        PrintExpr(f, left);
        fprintf(f, " %s ", op);
        PrintExpr(f, right);
    } else if (right) {
        fprintf(f, "%s", op);
        PrintExpr(f, right);
    } else {
        PrintExpr(f, left);
        fprintf(f, "%s", op);
    }
}

/* code to print left operator right where operator is a bit manipulation
 * operator
 * just like PrintInOp, but prints integer constants in hex
 */
static void
PrintHexExpr(FILE *f, AST *left)
{
    if (IsConstExpr(left)) {
        fprintf(f, "0x%x", EvalConstExpr(left));
    } else {
        PrintExpr(f, left);
    }
}

static void
PrintLogicOp(FILE *f, const char *op, AST *left, AST *right)
{
    if (left && right) {
        PrintHexExpr(f, left);
        fprintf(f, " %s ", op);
        PrintHexExpr(f, right);
    } else if (right) {
        fprintf(f, "%s", op);
        PrintHexExpr(f, right);
    } else {
        PrintHexExpr(f, left);
        fprintf(f, "%s", op);
    }
}

static void
PrintMacroExpr(FILE *f, const char *name, AST *left, AST *right)
{
    fprintf(f, "%s(", name);
    PrintExpr(f, left);
    fprintf(f, ", ");
    PrintExpr(f, right);
    fprintf(f, ")");
}

void
PrintOperator(FILE *f, int op, AST *left, AST *right)
{
    char opstring[4];

    switch (op) {
    case T_HIGHMULT:
        PrintMacroExpr(f, "Highmult__", left, right);
        break;
    case T_LE:
        PrintInOp(f, "<=", left, right);
        break;
    case T_GE:
        PrintInOp(f, ">=", left, right);
        break;
    case T_EQ:
        PrintInOp(f, "==", left, right);
        break;
    case T_NE:
        PrintInOp(f, "!=", left, right);
        break;
    case T_SHL:
        PrintInOp(f, "<<", left, right);
        break;
    case T_SAR:
        PrintInOp(f, ">>", left, right);
        break;
    case T_SHR:
        if (current->printLabelsVerbatim) {
            PrintInOp(f, ">>", left, right);
        } else {
            PrintMacroExpr(f, "Shr__", left, right);
        }
        break;
    case T_REV:
        fprintf(f, "__builtin_propeller_rev(");
        PrintExpr(f, left);
        fprintf(f, ", 32 - ");
        PrintExpr(f, right);
        fprintf(f, ")");
        break;
    case T_MODULUS:
        PrintInOp(f, "%", left, right);
        break;
    case T_INCREMENT:
        PrintInOp(f, "++", left, right);
        break;
    case T_DECREMENT:
        PrintInOp(f, "--", left, right);
        break;
    case T_NEGATE:
        PrintInOp(f, "-", left, right);
        break;
    case T_BIT_NOT:
        PrintLogicOp(f, "~", left, right);
        break;
    case T_AND:
        PrintInOp(f, "&&", left, right);
        break;
    case T_OR:
        PrintInOp(f, "||", left, right);
        break;
    case T_NOT:
        PrintInOp(f, "!", left, right);
        break;
    case T_LIMITMIN:
        PrintMacroExpr(f, "Max__", left, right);
        break;
    case T_LIMITMAX:
        PrintMacroExpr(f, "Min__", left, right);
        break;
    case T_ROTL:
        PrintMacroExpr(f, "Rotl__", left, right);
        break;
    case T_ROTR:
        PrintMacroExpr(f, "Rotr__", left, right);
        break;
    case T_ABS:
        fprintf(f, "abs(");
        PrintExpr(f, right);
        fprintf(f, ")");
        break;
    case T_SQRT:
        fprintf(f, "Sqrt__(");
        PrintExpr(f, right);
        fprintf(f, ")");
        break;
    case '?':
        if (left) {
            fprintf(f, "RandForw__(");
            PrintExpr(f, left);
            fprintf(f, ")");
        } else {
            fprintf(f, "RandBack__(");
            PrintExpr(f, right);
            fprintf(f, ")");
        }
        break;
    case '&':
    case '|':
    case '^':
        opstring[0] = op;
        opstring[1] = 0;
        PrintLogicOp(f, opstring, left, right);
        break;
    case '+':
    case '-':
    case '/':
    case '*':
    case '<':
    case '>':
        opstring[0] = op;
        opstring[1] = 0;
        PrintInOp(f, opstring, left, right);
        break;
    case T_ENCODE:
        fprintf(f, "BitEncode__(");
        PrintExpr(f, right);
        fprintf(f, ")");
        break;
    case T_DECODE:
        fprintf(f, "(1<<");
        PrintExpr(f, right);
        fprintf(f, ")");
        break;
    default:
        ERROR(NULL, "unsupported operator %d", op);
        break;
    }
}

/*
 * code to print out a type declaration
 */
void
PrintType(FILE *f, AST *typedecl)
{
    int size;

    switch (typedecl->kind) {
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
        if (typedecl->kind == AST_UNSIGNEDTYPE) {
            fprintf(f, "u");
        }
        size = EvalConstExpr(typedecl->left);
        switch (size) {
        case 1:
            fprintf(f, "int8_t");
            break;
        case 2:
            fprintf(f, "int16_t");
            break;
        case 4:
            fprintf(f, "int32_t");
            break;
        default:
            ERROR(NULL, "unsupported integer size %d", size);
            break;
        }
        break;
    default:
        ERROR(typedecl, "unknown type declaration %d", typedecl->kind);
        break;
    }
}

/* code to print a source expression (could be an array reference or
 * range)
 * if "assignment" is true then we are in an assignment operator, so
 * only certain types of symbols are valid
 * if "ref" is true then we are planning to use the expression
 * as a reference, so no extra dereferencing should be added to
 * e.g. labels, and plain symbols and such should be cast appropriately
 */
void
PrintLHS(FILE *f, AST *expr, int assignment, int ref)
{
    Symbol *sym;
    HwReg *hw;

    switch (expr->kind) {
    case AST_RESULT:
        if (!curfunc) {
            ERROR(expr, "RESULT keyword outside of function");
        } else if (curfunc->result_in_parmarray) {
            fprintf(f, "%s[0]", curfunc->parmarray);
        } else {
            PrintLHS(f, curfunc->resultexpr, assignment, ref);
        }
        break;
    case AST_IDENTIFIER:
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            ERROR(expr, "Unknown symbol %s", expr->d.string);
        } else {
            if (sym->type == SYM_FUNCTION || sym->type == SYM_BUILTIN) {
                if (assignment) {
                    ERROR(expr, "symbol %s on left hand side of assignment", sym->name);
                } else {
                    if (sym->type == SYM_BUILTIN) {
                        Builtin *b = sym->val;
                        (*b->printit)(f, b, NULL);
                    } else {
                        if (gl_ccode) {
                            fprintf(f, "%s_", current->classname);
                        }
                        PrintFuncCall(f, sym, NULL);
                    }
                }
            } else if (sym->type == SYM_LABEL) {
                PrintLabel(f, sym, ref);
            } else {
                PrintSymbol(f, sym);
            }
        }
        break;
    case AST_ADDROF:
        if (!ref) {
            fprintf(f, "(int32_t)");
        }
        PrintLHS(f, expr->left, assignment, 1);
        break;
    case AST_ARRAYREF:
        if (expr->left && expr->left->kind == AST_IDENTIFIER) {
            sym = LookupSymbol(expr->left->d.string);
        } else {
            sym = NULL;
        }
        if (sym && sym->type == SYM_LOCALVAR && curfunc && curfunc->localarray) {
            fprintf(f, "%s[%d + ", curfunc->localarray, sym->count);
            PrintExpr(f, expr->right);
            fprintf(f, "]");
        } else {
            if (sym && !IsArraySymbol(sym)) {
//                ERROR(expr, "array dereference of bad symbol %s", sym->name);
                fprintf(f, "(&");
                PrintLHS(f, expr->left, assignment, 1);
                fprintf(f, ")");
            } else {
                PrintLHS(f, expr->left, assignment, 1);
            }
            fprintf(f, "[");
            PrintExpr(f, expr->right);
            fprintf(f, "]");
        }
        break;
    case AST_HWREG:
        hw = (HwReg *)expr->d.ptr;
        fprintf(f, "%s", hw->cname);
        break;
    case AST_MEMREF:
        fprintf(f, "((");
        PrintType(f, expr->left);
        fprintf(f, " *)");
        PrintExpr(f, expr->right);
        fprintf(f, ")");
        break;
    default:
        ERROR(expr, "bad target for assignment");
        break;
    }
}

/*
 * code to print a postfix operator like x~ (which returns x then sets x to 0)
 * if "toplevel" is 1 then we can skip the saving of the original value
 */
void
PrintPostfix(FILE *f, AST *expr, int toplevel)
{
    AST *target;

    if (expr->d.ival == '~')
        target = AstInteger(0);
    else if (expr->d.ival == T_DOUBLETILDE) {
        target = AstInteger(-1);
    } else {
        ERROR(expr, "bad postfix operator %d", expr->d.ival);
        return;
    }
    if (toplevel) {
        PrintAssign(f, expr->left, target);
    } else {
        fprintf(f, "PostEffect__(");
        PrintExpr(f, expr->left);
        fprintf(f, ", ");
        PrintExpr(f, target);
        fprintf(f, ")");
    }
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
static void
RangeXor(FILE *f, AST *dst, AST *src)
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
            if (srcval == -1 || srcval == 0) {
                srcval = srcval & 1;
                PrintLHS(f, dst->left, 1, 0);
                fprintf(f, " ^= (1<<");
                PrintExpr(f, loexpr);
                fprintf(f, ")");
                return;
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

    PrintLHS(f, dst->left, 1, 0);
    fprintf(f, " ^= 0x%x", mask);
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
static void
RangeBitSet(FILE *f, AST *dst, uint32_t mask, int bitset)
{
    AST *loexpr;

    if (dst->right->right) {
        loexpr = dst->right->right;
    } else {
        loexpr = dst->right->left;
    }
    PrintLHS(f, dst->left, 1, 0);
    if (bitset) {
        fprintf(f, " |= (%u<<", mask);
    } else {
        fprintf(f, " &= ~(%u<<", mask);
    }
    PrintExpr(f, loexpr);
    fprintf(f, ")");
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
 */
void
PrintRangeAssign(FILE *f, AST *dst, AST *src)
{
    int reverse = 0;
    uint32_t mask;
    int nbits;
    AST *loexpr;

    if (dst->right->kind != AST_RANGE) {
        ERROR(dst, "internal error: expecting range");
        return;
    }
    /* special case logical operators */
    if (src->kind == AST_OPERATOR && src->d.ival == T_BIT_NOT
        && AstMatch(dst, src->right))
    {
        RangeXor(f, dst, AstInteger(0xffffffff));
        return;
    }
    /* now handle the ordinary case */
    /* if the "range" is just a single item it does not have to
       be constant, but for a real range we need constants on each end
    */
    if (dst->right->right == NULL) {
        nbits = 1;
        loexpr = dst->right->left;
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
        if (bitset == 0 || bitset == mask) {
            RangeBitSet(f, dst, mask, bitset);
            return;
        }
    }
    if (nbits >= 32) {
        PrintLHS(f, dst->left, 1, 0);
        fprintf(f, " = ");
        PrintExpr(f, FoldIfConst(src));
        return;
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

        PrintLHS(f, dst->left, 1, 0);
        fprintf(f, " = ");
        PrintExpr(f, orexpr);
    }
}

/*
 * print a range use
 */
void
PrintRangeUse(FILE *f, AST *src)
{
    int reverse = 0;
    AST *mask;
    int nbits;
    AST *loexpr;
    AST *val;

    if (src->left->kind != AST_HWREG) {
        ERROR(src, "range not applied to hardware register");
        return;
    }
    if (src->right->kind != AST_RANGE) {
        ERROR(src, "internal error: expecting range");
        return;
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
        if (reverse) {
            ERROR(src, "cannot currently handle reversed range");
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
    PrintExpr(f, val);
}


static void
PrintStringChar(FILE *f, int c)
{
    if (isprint(c)) {
        if (c == '\\' || c == '"' || c == '\'') {
            fprintf(f, "\\%c", c);
        } else {
            fprintf(f, "%c", c);
        }
    } else if (c == 10) {
        fprintf(f, "\\n");
    } else if (c == 13) {
        fprintf(f, "\\r");
    } else {
        fprintf(f, "\\%03o", c);
    }
}

static void
PrintStringList(FILE *f, AST *ast)
{
    int c;
    const char *s;
    AST *elem;
    fprintf(f, "\"");
    while (ast) {
        elem = ast->left;
        if (!elem) {
            ERROR(ast, "internal error in string list");
            break;
        }
        if (elem->kind == AST_STRING) {
            s = elem->d.string;
            while (s && *s) {
                c = *s++;
                PrintStringChar(f, c);
            }
        } else {
            c = EvalConstExpr(elem);
            PrintStringChar(f, c);
        }
        ast = ast->right;
    }
    fprintf(f, "\"");
}

static void
PrintStringLiteral(FILE *f, const char *s)
{
    int c;
    fprintf(f, "\"");
    while ((c = *s++) != 0) {
        PrintStringChar(f, c);
    }
    fprintf(f, "\"");
}

/* code to print an object, treating it as an address */
void
PrintAsAddr(FILE *f, AST *expr)
{
    if (!expr)
        return;
    switch(expr->kind) {
    case AST_STRING:
        PrintStringLiteral(f, expr->d.string);
        break;
    case AST_ADDROF:
        fprintf(f, "&");
        PrintLHS(f, expr->left, 0, 0);
        break;
    case AST_IDENTIFIER:
    case AST_RESULT:
    case AST_HWREG:
    case AST_MEMREF:
    case AST_ARRAYREF:
        PrintLHS(f, expr, 0, 1);
        break;
    case AST_ASSIGN:
    default:
        fprintf(f, "(void *)(");
        PrintExpr(f, expr);
        fprintf(f, ")");
        break;
    }
}

/*
 * code to print an assignment
 */
void
PrintAssign(FILE *f, AST *lhs, AST *rhs)
{
    if (lhs->kind == AST_RANGEREF) {
        PrintRangeAssign(f, lhs, rhs);
    } else {
        /* in Spin an expression like
             arr := 1
           where arr is an array is equivalent to
             arr[0] := 1
        */
        if (IsArray(lhs)) {
            lhs = NewAST(AST_ARRAYREF, lhs, AstInteger(0));
        }
        PrintLHS(f, lhs, 1, 0);
        fprintf(f, " = ");
        PrintExpr(f, rhs);
    }
}

static int
isBooleanOperator(AST *expr)
{
    int x;

    if (expr->kind != AST_OPERATOR)
        return 0;

    x = expr->d.ival;
    switch (x) {
    case T_NOT:
    case T_AND:
    case T_OR:
    case T_LE:
    case '<':
    case T_GE:
    case '>':
    case T_EQ:
    case T_NE:
        return 1;
    default:
        return 0;
    }
}


/*
 * print a lookup array; returns the number of elements in it
 */
int
PrintLookupArray(FILE *f, AST *array)
{
    AST *ast;
    AST *expr;
    int c, d;
    int i;
    int sz = 0;

    ast = array;
    while (ast) {
        expr = ast->left;
        ast = ast->right;

        if (expr->kind == AST_RANGE) {
            c = EvalConstExpr(expr->left);
            d = EvalConstExpr(expr->right);
            if (c > d) {
                int tmp = d; d = c; c = tmp;
            }
            for (i = c; i <= d; i++) {
                fprintf(f, "%d, ", i);
                sz++;
            }
        } else {
            PrintExpr(f, expr);
            fprintf(f, ", ");
            sz++;
        }
    }
    return sz;
}

//
// lookup/lookdown macros look like
// Lookup__(idx, base, ((int32_t[]){1, 2, 3...}), N)
// where idx is the indexing expression,
// base is 0 or 1 (array index base)
// N is the length of the array
//

static void
PrintLookExpr(FILE *f, const char *name, AST *ev, AST *table)
{
    int len;
    AST *idx, *base;
    if (ev->kind != AST_LOOKEXPR
        || (table->kind != AST_EXPRLIST && table->kind != AST_TEMPARRAYUSE))
    {
        ERROR(ev, "Internal error in lookup");
        return;
    }
    base = ev->left;
    idx = ev->right;
    fprintf(f, "%s(", name);
    PrintExpr(f, idx);
    fprintf(f, ", ");
    PrintExpr(f, base);
    fprintf(f, ", ");
    if (table->kind == AST_TEMPARRAYUSE) {
        PrintExpr(f, table->left);
        len = EvalConstExpr(table->right);
    } else {
        fprintf(f, "((int32_t[]){");
        len = PrintLookupArray(f, table);
        fprintf(f, "})");
    }
    fprintf(f, ", %u)", len);
}

/* print an object symbol */
static void
PrintObjectSym(FILE *f, Symbol *objsym, AST *expr)
{
    AST *decl;
    int isArray = 0;

    if (objsym->type != SYM_OBJECT) {
        ERROR(expr, "Internal error, expecting an object symbol\n");
        return;
    }
    decl = (AST *)objsym->val;
    if (decl->kind != AST_OBJECT) {
        ERROR(expr, "Internal error, symbol has no AST_OBJECT kind");
        return;
    }
    isArray = (decl->left && decl->left->kind == AST_ARRAYDECL);

    if (expr->kind == AST_ARRAYREF) {
        if (!isArray) {
            ERROR(expr, "%s is not an array of objects", objsym->name);
        }
        PrintLHS(f, expr, 0, 0);
    } else {
        fprintf(f, "%s", objsym->name);
        if (isArray) {
            fprintf(f, "[0]");
        }
    }
}

/* code to print an expression to a file */
void
PrintExpr(FILE *f, AST *expr)
{
    Symbol *sym, *objsym;
    int c;

    if (!expr) {
        return;
    }
    switch (expr->kind) {
    case AST_INTEGER:
        PrintInteger(f, (int32_t)expr->d.ival);
        break;
    case AST_FLOAT:
        PrintFloat(f, (int32_t)expr->d.ival);
        break;
    case AST_STRING:
        if (strlen(expr->d.string) > 1) 
            ERROR(expr, "string too long, expected a single character");

        c = expr->d.string[0];
        
        if (isprint(c)) {
            fputc('\'', f);
            PrintStringChar(f, c);
            fputc('\'', f);
        } else {
            fprintf(f, "%d", c);
        }
        break;
    case AST_STRINGPTR:
        fprintf(f, "(int32_t)");
        PrintStringList(f, expr->left);
        break;
    case AST_CONSTANT:
        fprintf(f, "0x%x", EvalConstExpr(expr->left));
        break;      
    case AST_ADDROF:
        fprintf(f, "(int32_t)(&");
        PrintLHS(f, expr->left, 0, 0);
        fprintf(f, ")");
        break;
    case AST_DATADDROF:
        fprintf(f, "(int32_t)((");
        PrintLHS(f, expr->left, 0, 0);
        fprintf(f, ")+%s)", current->datname);
        break;
    case AST_IDENTIFIER:
    case AST_HWREG:
    case AST_MEMREF:
    case AST_ARRAYREF:
    case AST_RESULT:
        PrintLHS(f, expr, 0, 0);
        break;
    case AST_OPERATOR:
        fprintf(f, "%s(", isBooleanOperator(expr) ? "-" : "");
        PrintOperator(f, expr->d.ival, expr->left, expr->right);
        fprintf(f, ")");
        break;
    case AST_POSTEFFECT:
        PrintPostfix(f, expr, 0);
        break;
    case AST_ASSIGN:
        fprintf(f, "(");
        PrintAssign(f, expr->left, expr->right);
        fprintf(f, ")");
        break;
    case AST_FUNCCALL:
        if (expr->left && expr->left->kind == AST_METHODREF) {
            AST *objref = expr->left->left;
            objsym = LookupAstSymbol(objref, "object reference");
            if (!objsym) return;
            if (objsym->type != SYM_OBJECT) {
                ERROR(expr, "%s is not an object", objsym->name);
                return;
            }
            sym = LookupObjSymbol(expr, objsym, expr->left->right->d.string);
            if (!sym || sym->type != SYM_FUNCTION) {
                ERROR(expr, "%s is not a method of %s", sym->name, objsym->name);
                return;
            }
            if (gl_ccode) {
                fprintf(f, "%s_", ObjClassName(objsym));
            } else {
                PrintObjectSym(f, objsym, objref);
                fprintf(f, ".");
            }
        } else {
            sym = LookupAstSymbol(expr->left, "function call");
            if (gl_ccode && sym && sym->type == SYM_FUNCTION)
                fprintf(f, "%s_", current->classname);
        }
        if (!sym) {
            ; /* do nothing, printed error already */
        } else if (sym->type == SYM_BUILTIN) {
            Builtin *b = sym->val;
            (*b->printit)(f, b, expr->right);
        } else if (sym->type == SYM_FUNCTION) {
            PrintFuncCall(f, sym, expr->right);
        } else {
            ERROR(expr, "%s is not a function", sym->name);
        }
        break;
    case AST_RANGEREF:
        PrintRangeUse(f, expr);
        break;
    case AST_LOOKUP:
        PrintLookExpr(f, "Lookup__", expr->left, expr->right);
        break;
    case AST_LOOKDOWN:
        PrintLookExpr(f, "Lookdown__", expr->left, expr->right);
        break;
    case AST_CONSTREF:
        if (!GetObjConstant(expr, &objsym, &sym))
            return;
        if (gl_ccode) {
            fprintf(f, "%s", sym->name);
        } else {
            fprintf(f, "%s.%s", objsym->name, sym->name);
        }
        break;
    case AST_CATCH:
        fprintf(f, "__extension__({ AbortHook__ *stack__ = abortChain__, here__; ");
        fprintf(f, "int32_t tmp__; abortChain__ = &here__; ");
        fprintf(f, "if (setjmp(here__.jmp) == 0) tmp__ = ");
        PrintExpr(f, expr->left);
        fprintf(f, "; else tmp__ = here__.val; abortChain__ = stack__; tmp__; })");
        break;
    case AST_TRUNC:
        if (!IsConstExpr(expr->left)) {
            ERROR(expr, "argument to trunc is not constant");
            break;
        }
        fprintf(f, "%d", EvalConstExpr(expr->left));
        break;
    default:
        ERROR(expr, "Internal error, bad expression");
        break;
    }
}

void
PrintBoolExpr(FILE *f, AST *expr)
{
    int op;
    if (expr->kind == AST_OPERATOR) {
        AST *lhs = expr->left;
        AST *rhs = expr->right;
        if (IsArray(lhs)) {
            lhs = NewAST(AST_ARRAYREF, lhs, AstInteger(0));
        }
        if (IsArray(rhs)) {
            rhs = NewAST(AST_ARRAYREF, rhs, AstInteger(0));
        }
        op = expr->d.ival;
        switch (op) {
        case T_NOT:
            fprintf(f, "!(");
            PrintBoolExpr(f, rhs);
            fprintf(f, ")");
            break;
        case T_OR:
        case T_AND:
            fprintf(f, "(");
            PrintBoolExpr(f, lhs);
            fprintf(f, "%s", op == T_OR ? ") || (" : ") && (");
            PrintBoolExpr(f, rhs);
            fprintf(f, ")");
            break;
        default:
            PrintOperator(f, op, lhs, rhs);
            break;
        }
    } else {
        PrintExpr(f, expr);
    }
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
        return lval / rval;
    case T_MODULUS:
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
    if (le.type == FLOAT_EXPR || re.type == FLOAT_EXPR) {
        return floatExpr(EvalFloatOperator(op, intAsFloat(le.val), intAsFloat(re.val), valid));
    }
    return intExpr(EvalIntOperator(op, le.val, re.val, valid));
}

#define PASM_FLAG 0x01

/*
 * evaluate an expression
 * if unable to evaluate, return 0 and set "*valid" to 0
 */
static ExprVal
EvalExpr(AST *expr, unsigned flags, int *valid)
{
    Symbol *sym, *objsym;
    ExprVal lval, rval;
    int reportError = (valid == NULL);
    ExprVal ret;
    ParserState *pushed;

    if (!expr)
        return intExpr(0);

    switch (expr->kind) {
    case AST_INTEGER:
        return intExpr(expr->d.ival);

    case AST_FLOAT:
        return floatExpr(intAsFloat(expr->d.ival));

    case AST_STRING:
        return intExpr(expr->d.string[0]);

    case AST_TOFLOAT:
        lval = EvalExpr(expr->left, flags, valid);
        if (lval.type != INT_EXPR) {
            ERROR(expr, "applying float to a non integer expression");
        }
        return floatExpr((float)(lval.val));

    case AST_TRUNC:
        lval = EvalExpr(expr->left, flags, valid);
        if (lval.type != FLOAT_EXPR) {
            ERROR(expr, "applying trunc to a non float expression");
        }
        return intExpr((int)intAsFloat(lval.val));

    case AST_ROUND:
        lval = EvalExpr(expr->left, flags, valid);
        if (lval.type != FLOAT_EXPR) {
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
        pushed = current;
        current = GetObjectPtr(objsym);
        ret = EvalExpr(sym->val, flags, valid);
        current = pushed;
        return ret;
    case AST_IDENTIFIER:
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            if (reportError)
                ERROR(expr, "Unknown symbol %s", expr->d.string);
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
    case AST_HWREG:
        if (flags & PASM_FLAG) {
            HwReg *hw = expr->d.ptr;
            return intExpr(hw->addr);
        }
        if (reportError)
            ERROR(expr, "Used hardware register where constant is expected");
        else
            *valid = 0;
        break;
    case AST_ADDROF:
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
            return intExpr(lref->asmval);
        }
        break;
    default:
        if (reportError)
            ERROR(expr, "Bad constant expression");
        else
            *valid = 0;
        break;
    }
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
    if (valid && eval.type == FLOAT_EXPR)
        return 1;
    return 0;
}

void
PrintExprList(FILE *f, AST *list)
{
    int needcomma = 0;
    while (list) {
        if (list->kind != AST_EXPRLIST) {
            ERROR(list, "expected expression list");
            return;
        }
        if (needcomma) {
            fprintf(f, ", ");
        }
        PrintExpr(f, list->left);
        needcomma = 1;
        list = list->right;
    }
}


/* code to print a builtin function call to a file */
void
defaultBuiltin(FILE *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != b->numparameters) {
        ERROR(params, "wrong number of parameters to %s", b->name);
    }
    fprintf(f, "%s(", b->cname);
    PrintExprList(f, params);
    fprintf(f, ")");
}

/* code to print a builtin variable reference call to a file */
void
defaultVariable(FILE *f, Builtin *b, AST *params)
{
    fprintf(f, "%s", b->cname);
}

/* code to do memory copies; also does 1 byte fills */
void
memBuiltin(FILE *f, Builtin *b, AST *params)
{
    int ismemcpy = !strcmp(b->cname, "memcpy") || !strcmp(b->cname, "memmove");
    AST *dst, *src, *count;

    if (AstListLen(params) != 3) {
        ERROR(params, "incorrect parameters to %s", b->name);
        return;
    }

    dst = params->left;
    params = params->right;
    src = params->left;
    params = params->right;
    count = params->left;

    fprintf(f, "%s( (void *)", b->cname);
    PrintAsAddr(f, dst);
    fprintf(f, ", ");
    if (ismemcpy) {
        fprintf(f, "(void *)");
        PrintAsAddr(f, src);
    } else {
        PrintExpr(f, src);
    }

    /* b->numparameters is overloaded to mean the size of memory we
       are working with
    */
    fprintf(f, ", %d*(", b->numparameters);
    PrintExpr(f, count);
    fprintf(f, "))");
}

/* code to do 2 and 4 byte memory fills */
void
memFillBuiltin(FILE *f, Builtin *b, AST *params)
{
    const char *idxname;
    const char *valname;
    const char *typename;
    const char *ptrname;
    AST *dst, *src, *count;

    if (AstListLen(params) != 3) {
        ERROR(params, "incorrect parameters to %s", b->name);
        return;
    }

    dst = params->left;
    params = params->right;
    src = params->left;
    params = params->right;
    count = params->left;

    idxname = NewTemporaryVariable("_fill_");
    valname = NewTemporaryVariable("_val_");
    ptrname = NewTemporaryVariable("_ptr_");
    /* b->numparameters is overloaded to mean the size of memory we
       are working with
    */
    typename = (b->numparameters == 2) ? "uint16_t" : "int32_t";
    fprintf(f, "{ int32_t %s; ", idxname);
    fprintf(f, "%s *%s = (%s *)", typename, ptrname, typename);
    PrintAsAddr(f, dst);
    fprintf(f, "; %s %s = ", typename, valname);
    PrintExpr(f, src);
    fprintf(f, "; for (%s = ", idxname);
    PrintExpr(f, count);
    fprintf(f, "; %s > 0; --%s) { ", idxname, idxname);
    fprintf(f, " *%s++ = %s; } }", ptrname, valname);
}

/* code to do strsize */
void
str1Builtin(FILE *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != 1) {
        ERROR(params, "incorrect parameters to %s", b->name);
        return;
    }
    fprintf(f, "%s((char *) ", b->cname);
    PrintExpr(f, params->left); params = params->right;
    fprintf(f, ")");
}

/* code to do strcomp(a, b) */
void
strcompBuiltin(FILE *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != 2) {
        ERROR(params, "incorrect parameters to %s", b->name);
        return;
    }
    fprintf(f, "-(0 == strcmp((char *)");
    PrintExpr(f, params->left); params = params->right;
    fprintf(f, ", (char *)");
    PrintExpr(f, params->left); params = params->right;
    fprintf(f, "))");
}

/* code for waitpne and waitpeq */
/* the last parameter must be a constant 0 */

void
waitBuiltin(FILE *f, Builtin *b, AST *params)
{
    AST *arg1, *arg2, *arg3;
    if (AstListLen(params) != b->numparameters) {
        ERROR(params, "wrong number of parameters to %s", b->name);
    }
    arg1 = params->left; params = params->right;
    arg2 = params->left; params = params->right;
    arg3 = params->left; params = params->right;

    if (EvalConstExpr(arg3) != 0) {
        ERROR(arg3, "Final parameter to %s must be 0", b->name);
    }
    fprintf(f, "%s(", b->cname);
    PrintExpr(f, arg1);
    fprintf(f, ", ");
    PrintExpr(f, arg2);
    fprintf(f, ")");
}

/* code to do reboot() -> clkset(0x80, 0) */
void
rebootBuiltin(FILE *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != b->numparameters) {
        ERROR(params, "wrong number of parameters to %s", b->name);
    }
    fprintf(f, "clkset(0x80, 0)");
}

/*
 * see if an AST refers to a parameter of this function, and return
 * an index into the list if it is
 * otherwise, return -1
 */

int
funcParameterNum(Function *func, AST *var)
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
    e.type = INT_EXPR;
    e.val = x;
    return e;
}

ExprVal floatExpr(float f)
{
    ExprVal e;
    e.type = FLOAT_EXPR;
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
        /* accept all objects as arrays, even if they are not */
        return 1;
    case SYM_LABEL:
        return 1;
    default:
        return 0;
    }
    return IsArrayType(type);
}

