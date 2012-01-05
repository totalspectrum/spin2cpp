#include "spinc.h"
#include <ctype.h>
#include <string.h>

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

/* code to print a label to a file
 * if "ref" is nonzero this is an array reference, so do not
 * dereference again
 */
void
PrintLabel(FILE *f, Symbol *sym, int ref)
{
    Label *lab;

    lab = (Label *)sym->val;
    fprintf(f, "(%s(", ref ? "" : "*");
    PrintType(f, lab->type);
    fprintf(f, " *)&dat[%d])", lab->offset);
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
        fprintf(f, "%ld", (long)(intptr_t)sym->val);
        break;
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

/* code to print left operator right */
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
        fprintf(f, "(( (long long)");
        PrintExpr(f, left);
        fprintf(f, " * (long long)");
        PrintExpr(f, right);
        fprintf(f, ") >> 32)");
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
        fprintf(f, "((uint32_t)");
        PrintExpr(f, left);
        fprintf(f, ") >> ");
        PrintExpr(f, right);
        break;
    case T_REV:
        fprintf(f, "__builtin_propeller_reverse(");
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
        PrintInOp(f, "~", left, right);
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
    case '+':
    case '-':
    case '/':
    case '*':
    case '&':
    case '|':
    case '^':
    case '<':
    case '>':
        opstring[0] = op;
        opstring[1] = 0;
        PrintInOp(f, opstring, left, right);
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
        PrintLHS(f, expr->left, assignment, 1);
        fprintf(f, "[");
        PrintExpr(f, expr->right);
        fprintf(f, "]");
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
 */
void
PrintPostfix(FILE *f, AST *expr)
{
    const char *str;

    if (expr->d.ival == '~')
        str = "0";
    else if (expr->d.ival == T_DOUBLETILDE) {
        str = "-1";
    } else {
        ERROR(expr, "bad postfix operator %d", expr->d.ival);
        return;
    }
    fprintf(f, "__extension__({ int32_t _tmp_ = ");
    PrintLHS(f, expr->left, 0, 0);
    fprintf(f, "; ");
    PrintLHS(f, expr->left, 1, 0);
    fprintf(f, " = %s; _tmp_; })", str);
}

/*
 * special case an assignment like outa[2..1] ^= -1
 */
static void
RangeXor(FILE *f, AST *dst, AST *src)
{
    int lo, hi, nbits;
    uint32_t mask;

    /* now handle the ordinary case */
    hi = EvalConstExpr(dst->right->left);
    lo = EvalConstExpr(dst->right->right);
    if (hi < lo) {
        int tmp;
        tmp = lo; lo = hi; hi = tmp;
    }
    nbits = (hi - lo + 1);
    mask = ((1U<<nbits) - 1);
    mask = mask & EvalConstExpr(src);
    mask = (mask << lo) | (mask >> (32-lo));

    PrintLHS(f, dst->left, 1, 0);
    fprintf(f, " ^= 0x%x", mask);
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
    int lo, hi;
    int reverse = 0;
    uint32_t mask;
    int nbits;

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
    hi = EvalConstExpr(dst->right->left);
    lo = EvalConstExpr(dst->right->right);

    if (hi < lo) {
        int tmp;
        reverse = 1;
        tmp = lo; lo = hi; hi = tmp;
        ERROR(dst, "cannot currently handle reversed range");
    }
    nbits = (hi - lo + 1);

    if (nbits >= 32) {
        PrintLHS(f, dst->left, 1, 0);
        fprintf(f, " = ");
        PrintExpr(f, src);
        return;
    }
    mask = ((1U<<nbits) - 1);
    mask = (mask << lo) | (mask >> (32-lo));


    PrintLHS(f, dst->left, 1, 0);
    fprintf(f, " = (");
    PrintLHS(f, dst->left, 1, 0);
    fprintf(f, " & 0x%08x) | ((", ~mask);
    PrintExpr(f, src);
    fprintf(f, " << %d) & 0x%08x)", lo, mask); 
}

static void
PrintStringLiteral(FILE *f, const char *s)
{
    int c;
    fprintf(f, "\"");
    while ((c = *s++) != 0) {
        if (isprint(c) && c != '"') {
            fprintf(f, "%c", c);
        } else if (c == 10) {
            fprintf(f, "\\n");
        } else if (c == 13) {
            fprintf(f, "\\r");
        } else {
            fprintf(f, "\\%03o", c);
        }
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
        PrintLHS(f, lhs, 1, 0);
        fprintf(f, " = ");
        PrintExpr(f, rhs);
    }
}

/* code to print an expression to a file */
void
PrintExpr(FILE *f, AST *expr)
{
    Symbol *sym;
    int c;
    const char *name = "";

    if (!expr) {
        return;
    }
    switch (expr->kind) {
    case AST_INTEGER:
        fprintf(f, "%ld", (long)(int32_t)expr->d.ival);
        break;
    case AST_STRING:
        c = expr->d.string[0];
        if (isprint(c)) {
            fprintf(f, "'%c'", c);
        } else {
            fprintf(f, "%d", c);
        }
        break;
    case AST_ADDROF:
        fprintf(f, "(int32_t)(&");
        PrintLHS(f, expr->left, 0, 0);
        fprintf(f, ")");
        break;
    case AST_IDENTIFIER:
    case AST_HWREG:
    case AST_MEMREF:
    case AST_ARRAYREF:
        PrintLHS(f, expr, 0, 0);
        break;
    case AST_OPERATOR:
        fprintf(f, "(");
        PrintOperator(f, expr->d.ival, expr->left, expr->right);
        fprintf(f, ")");
        break;
    case AST_POSTEFFECT:
        PrintPostfix(f, expr);
        break;
    case AST_ASSIGN:
        fprintf(f, "(");
        PrintAssign(f, expr->left, expr->right);
        fprintf(f, ")");
        break;
    case AST_FUNCCALL:
        sym = NULL;
        if (expr->left) {
            if (expr->left->kind == AST_IDENTIFIER) {
                name = expr->left->d.string;
                sym = LookupSymbol(name);
            }
        }
        if (!sym) {
            ERROR(expr, "undefined identifier %s in function call", name);
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
        ERROR(expr, "cannot handle range in expression yet");
        break;
    case AST_LOOKUP:
        PrintMacroExpr(f, "Lookup__", expr->left, expr->right);
        break;
    default:
        ERROR(expr, "Internal error, bad expression");
        break;
    }
}

static long
EvalOperator(int op, int32_t lval, int32_t rval, int *valid)
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
    case '<':
        return lval < rval;
    case '>':
        return lval > rval;
    case T_LE:
        return lval <= rval;
    case T_GE:
        return lval >= rval;
    case T_NE:
        return lval != rval;
    case T_EQ:
        return lval == rval;
    case T_NEGATE:
        return -rval;
    case T_BIT_NOT:
        return ~rval;
    case T_ABS:
        return (rval < 0) ? -rval : rval;
    case T_DECODE:
        return (1L << rval);
    case T_ENCODE:
        return 32 - __builtin_clz(rval);
    default:
        if (valid)
            *valid = 0;
        else
            ERROR(NULL, "unknown operator %d\n", op);
        return 0;
    }
}

#define PASM_FLAG 0x01

/*
 * evaluate an expression
 * if unable to evaluate, return 0 and set "*valid" to 0
 */
static int32_t
EvalExpr(AST *expr, unsigned flags, int *valid)
{
    Symbol *sym;
    int32_t lval, rval;
    int reportError = (valid == NULL);

    if (!expr)
        return 0;

    switch (expr->kind) {
    case AST_INTEGER:
        return expr->d.ival;

    case AST_STRING:
        return expr->d.string[0];

    case AST_IDENTIFIER:
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            if (reportError)
                ERROR(expr, "Unknown symbol %s", expr->d.string);
            else
                *valid = 0;
            return 0;
        } else {
            switch (sym->type) {
            case SYM_CONSTANT:
                return (int32_t)(intptr_t)sym->val;
            case SYM_LABEL:
                if (flags & PASM_FLAG) {
                    Label *lref = sym->val;
                    if (lref->asmval & 0x03) {
                        if (reportError)
                            ERROR(expr, "label %s not on longword boundary", sym->name);
                        else
                            *valid = 0;
                        return 0;
                    }
                    return lref->asmval >> 2;
                }
                /* otherwise fall through */
            default:
                if (reportError)
                    ERROR(expr, "Symbol %s is not constant", expr->d.string);
                else
                    *valid = 0;
                return 0;
            }
        }
        break;
    case AST_OPERATOR:
        lval = EvalExpr(expr->left, flags, valid);
        rval = EvalExpr(expr->right, flags, valid);
        return EvalOperator(expr->d.ival, lval, rval, valid);
    case AST_HWREG:
        if (flags & PASM_FLAG) {
            HwReg *hw = expr->d.ptr;
            return hw->addr;
        }
        if (reportError)
            ERROR(expr, "Used hardware register where constant is expected");
        else
            *valid = 0;
        break;
    default:
        if (reportError)
            ERROR(expr, "Bad constant expression");
        else
            *valid = 0;
        break;
    }
    return 0;
}

int32_t
EvalConstExpr(AST *expr)
{
    return EvalExpr(expr, 0, NULL);
}

int32_t
EvalPasmExpr(AST *expr)
{
    return EvalExpr(expr, PASM_FLAG, NULL);
}

int
IsConstExpr(AST *expr)
{
    int valid;
    valid = 1;
    EvalExpr(expr, 0, &valid);
    return valid;
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

/* code to do memory fills */
void
memBuiltin(FILE *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != 3) {
        ERROR(params, "incorrect parameters to %s", b->name);
        return;
    }
    fprintf(f, "%s(", b->cname);
    PrintAsAddr(f, params->left);
    params = params->right;
    fprintf(f, ", ");
    if (!strcmp(b->cname, "memcpy")) {
        PrintAsAddr(f, params->left);
    } else {
        PrintExpr(f, params->left);
    }
    params = params->right;

    /* b->numparameters is overloaded to mean the size of memory we
       are working with
    */
    fprintf(f, ", %d*(", b->numparameters);
    PrintExpr(f, params->left);
    fprintf(f, "))");
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

/*
 * create a lookup expression
 * this actually has several parts:
 * (1) create a temporary identifier for the array
 * (2) add an entry for the array into the current parse state
 * (3) return an expression tree with the lookup in the array
 */

AST *
NewLookup(AST *expr, AST *table)
{
    AST *arrayident;
    AST *array;

    arrayident = AstTempVariable("_lookup_");
    AddSymbol(&current->objsyms, arrayident->d.string, SYM_NAME, NULL);
    array = NewAST(AST_ARRAYDECL, arrayident, table);
    current->arrays = AddToList(current->arrays, NewAST(AST_LISTHOLDER, array, NULL));
    return NewAST(AST_LOOKUP, expr, arrayident);
}

/*
 * print the array
 */
void
PrintLookupArray(FILE *f, AST *array)
{
    AST *name;
    AST *ast;
    AST *expr;
    int c, d;
    int i;

    if (!array || array->kind != AST_ARRAYDECL) {
        ERROR(array, "internal error bad array type");
        return;
    }
    name = array->left;
    if (!name || name->kind != AST_IDENTIFIER) {
        ERROR(name, "internal error: array name expected");
        return;
    }
    fprintf(f, "int32_t %s[] = {\n  ", name->d.string);
    ast = array->right;
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
            }
        } else {
            c = EvalConstExpr(expr);
            fprintf(f, "%d, ", c);
        }
    }
    fprintf(f, "\n};\n");
}
