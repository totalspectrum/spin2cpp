#include "spinc.h"

/* code to find a symbol */
Symbol *
LookupSymbol(const char *name)
{
    return FindSymbol(&current->objsyms, name);
}

/* code to print a symbol to a file */
void
PrintSymbol(FILE *f, Symbol *sym)
{
    fprintf(f, "%s", sym->name);
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
 */
void
PrintLHS(FILE *f, AST *expr)
{
    Symbol *sym;
    HwReg *hw;

    switch (expr->kind) {
    case AST_IDENTIFIER:
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            ERROR("Unknown symbol %s", expr->d.string);
        } else {
            PrintSymbol(f, sym);
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
        PrintLHS(f, src->left);
        fprintf(f, " = ");
        PrintExpr(f, dst);
        return;
    }
    mask = ~((1U<<nbits) - 1);
    mask = (mask << lo) | (mask >> (32-lo));

    PrintLHS(f, src->left);
    fprintf(f, " = (");
    PrintLHS(f, src->left);
    fprintf(f, " & 0x%08x) | (", mask);
    PrintExpr(f, dst);
    fprintf(f, " << %d)", lo); 
}

/* code to print an expression to a file */
void
PrintExpr(FILE *f, AST *expr)
{
    switch (expr->kind) {
    case AST_INTEGER:
        fprintf(f, "%lu", (unsigned long)expr->d.ival);
        break;
    case AST_IDENTIFIER:
    case AST_HWREG:
        PrintLHS(f, expr);
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
            PrintLHS(f, expr->left);
            fprintf(f, " = ");
            PrintExpr(f, expr->right);
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
    default:
        ERROR("unknown operator %d\n", op);
        return 0;
    }
}

long
EvalConstExpr(AST *expr)
{
    Symbol *sym;
    long lval, rval;

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
        lval = EvalConstExpr(expr->left);
        rval = EvalConstExpr(expr->right);
        return EvalOperator(expr->d.ival, lval, rval);
    default:
        ERROR("Bad constant expression");
        break;
    }
    return 0;
}
