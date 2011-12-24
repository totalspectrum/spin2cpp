#include "spinc.h"

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
    return sym;
}

/* code to print a symbol to a file */
void
PrintSymbol(FILE *f, Symbol *sym)
{
    Label *ref;
    switch (sym->type) {
    case SYM_LABEL:
        ref = (Label *)sym->val;
        fprintf(f, "(*(int32_t *)&dat[%d])", ref->offset);
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
    PrintExpr(f, left);
    fprintf(f, " %s ", op);
    PrintExpr(f, right);
}

void
PrintBinaryOperator(FILE *f, int op, AST *left, AST *right)
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
    case T_MODULUS:
        PrintInOp(f, "%", left, right);
        break;
    default:
        opstring[0] = op;
        opstring[1] = 0;
        PrintInOp(f, opstring, left, right);
        break;
    }
}

/* code to print a source expression (could be an array reference or
 * range)
 * if "assignment" is true then we are in an assignment operator, so
 * only certain types of symbols are valid
 */
void
PrintLHS(FILE *f, AST *expr, int assignment)
{
    Symbol *sym;
    HwReg *hw;

    switch (expr->kind) {
    case AST_IDENTIFIER:
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            ERROR("Unknown symbol %s", expr->d.string);
        } else {
            if (sym->type == SYM_FUNCTION) {
                if (assignment) {
                    ERROR("symbol %s on left hand side of assignment", sym->name);
                } else {
                    PrintFuncCall(f, sym, NULL);
                }
            } else {
                PrintSymbol(f, sym);
            }
        }
        break;
    case AST_HWREG:
        hw = (HwReg *)expr->d.ptr;
        fprintf(f, "%s", hw->cname);
        break;
    default:
        ERROR("bad target for assignment");
        break;
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
 */
void
PrintRangeAssign(FILE *f, AST *src, AST *dst)
{
    int lo, hi;
    int reverse = 0;
    unsigned int mask;
    int nbits;

    if (src->right->kind != AST_RANGE) {
        ERROR("internal error: expecting range");
        return;
    }
    hi = EvalConstExpr(src->right->left);
    lo = EvalConstExpr(src->right->right);
    if (hi < lo) {
        int tmp;
        reverse = 1;
        tmp = lo; lo = hi; hi = tmp;
    }
    nbits = (hi - lo + 1);
    if (nbits >= 32) {
        PrintLHS(f, src->left, 1);
        fprintf(f, " = ");
        PrintExpr(f, dst);
        return;
    }
    mask = ((1U<<nbits) - 1);
    mask = (mask << lo) | (mask >> (32-lo));

    PrintLHS(f, src->left, 1);
    fprintf(f, " = (");
    PrintLHS(f, src->left, 1);
    fprintf(f, " & 0x%08x) | ((", ~mask);
    PrintExpr(f, dst);
    fprintf(f, " << %d) & 0x%08x)", lo, mask); 
}

/* code to print an expression to a file */
void
PrintExpr(FILE *f, AST *expr)
{
    Symbol *sym;

    switch (expr->kind) {
    case AST_INTEGER:
        fprintf(f, "%lu", (unsigned long)expr->d.ival);
        break;
    case AST_IDENTIFIER:
    case AST_HWREG:
        PrintLHS(f, expr, 0);
        break;
    case AST_OPERATOR:
        fprintf(f, "(");
        PrintBinaryOperator(f, expr->d.ival, expr->left, expr->right);
        fprintf(f, ")");
        break;
    case AST_ASSIGN:
        if (expr->left->kind == AST_RANGEREF) {
            PrintRangeAssign(f, expr->left, expr->right);
        } else {
            PrintLHS(f, expr->left, 1);
            fprintf(f, " = ");
            PrintExpr(f, expr->right);
        }
        break;
    case AST_FUNCCALL:
        sym = NULL;
        if (expr->left) {
            if (expr->left->kind == AST_IDENTIFIER)
                sym = LookupSymbol(expr->left->d.string);
        }
        if (!sym || sym->type != SYM_FUNCTION) {
            ERROR("not a function");
        } else {
            PrintFuncCall(f, sym, expr->right);
        }
        break;
    default:
        ERROR("Internal error, bad expression");
        break;
    }
}

static long
EvalOperator(int op, long lval, long rval)
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
        return -lval;
    case T_BIT_NOT:
        return ~lval;
    case T_ABS:
        return (lval < 0) ? -lval : lval;
    case T_DECODE:
        return (1L << lval);
    case T_ENCODE:
        return 32 - __builtin_clz(lval);
    default:
        ERROR("unknown operator %d\n", op);
        return 0;
    }
}

static long
EvalExpr(AST *expr, unsigned flags)
{
    Symbol *sym;
    long lval, rval;

    if (!expr)
        return 0;

    switch (expr->kind) {
    case AST_INTEGER:
        return expr->d.ival;
        break;
    case AST_IDENTIFIER:
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            ERROR("Unknown symbol %s", expr->d.string);
        } else {
            switch (sym->type) {
            case SYM_CONSTANT:
                return (long)sym->val;
            default:
                ERROR("Symbol %s is not constant", expr->d.string);
                break;
            }
        }
        break;
    case AST_OPERATOR:
        lval = EvalExpr(expr->left, flags);
        rval = EvalExpr(expr->right, flags);
        return EvalOperator(expr->d.ival, lval, rval);
    default:
        ERROR("Bad constant expression");
        break;
    }
    return 0;
}

long
EvalConstExpr(AST *expr)
{
    return EvalExpr(expr, 0);
}

void
PrintExprList(FILE *f, AST *list)
{
    int needcomma = 0;
    while (list) {
        if (list->kind != AST_EXPRLIST) {
            ERROR("expected expression list");
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
