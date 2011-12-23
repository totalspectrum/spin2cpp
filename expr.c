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
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            ERROR("Unknown symbol %s", expr->d.string);
        } else {
            PrintSymbol(f, sym);
        }
        break;
    case AST_OPERATOR:
        fprintf(f, "(");
        PrintBinaryOperator(f, expr->d.ival, expr->left, expr->right);
        fprintf(f, ")");
        break;
    case AST_ASSIGN:
        PrintExpr(f, expr->left);
        fprintf(f, " = ");
        PrintExpr(f, expr->right);
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
