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
        PrintExpr(f, expr->left);
        fprintf(f, "%c", expr->d.ival);
        PrintExpr(f, expr->right);
        fprintf(f, ")");
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
    case '*':
        return lval * rval;
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
