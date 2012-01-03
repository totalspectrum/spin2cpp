#include "spinc.h"
#include <ctype.h>

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

/* code to print a builtin function call to a file */
void
defaultBuiltin(FILE *f, Builtin *b, AST *params)
{
    fprintf(f, "%s(", b->cname);
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

void
PrintOperator(FILE *f, int op, AST *left, AST *right)
{
    char opstring[4];

    switch (op) {
    case '@':
        fprintf(f, "&");
        PrintExpr(f, right);
        break;
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
    default:
        opstring[0] = op;
        opstring[1] = 0;
        PrintInOp(f, opstring, left, right);
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
            ERROR("unsupported integer size %d", size);
            break;
        }
        break;
    default:
        ERROR("unknown type declaration %d", typedecl->kind);
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
            if (sym->type == SYM_FUNCTION || sym->type == SYM_BUILTIN) {
                if (assignment) {
                    ERROR("symbol %s on left hand side of assignment", sym->name);
                } else {
                    if (sym->type == SYM_BUILTIN) {
                        Builtin *b = sym->val;
                        (*b->printit)(f, b, NULL);
                    } else {
                        PrintFuncCall(f, sym, NULL);
                    }
                }
            } else {
                PrintSymbol(f, sym);
            }
        }
        break;
    case AST_ARRAYREF:
        PrintLHS(f, expr->left, assignment);
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
        ERROR("bad target for assignment");
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
        ERROR("bad postfix operator %d", expr->d.ival);
        return;
    }
    fprintf(f, "__extension__({ int32_t _tmp_ = ");
    PrintLHS(f, expr->left, 0);
    fprintf(f, "; ");
    PrintLHS(f, expr->left, 1);
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

    PrintLHS(f, dst->left, 1);
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
        ERROR("internal error: expecting range");
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
        ERROR("cannot currently handle reversed range");
    }
    nbits = (hi - lo + 1);

    if (nbits >= 32) {
        PrintLHS(f, dst->left, 1);
        fprintf(f, " = ");
        PrintExpr(f, src);
        return;
    }
    mask = ((1U<<nbits) - 1);
    mask = (mask << lo) | (mask >> (32-lo));


    PrintLHS(f, dst->left, 1);
    fprintf(f, " = (");
    PrintLHS(f, dst->left, 1);
    fprintf(f, " & 0x%08x) | ((", ~mask);
    PrintExpr(f, src);
    fprintf(f, " << %d) & 0x%08x)", lo, mask); 
}

/* code to print an expression to a file */
void
PrintExpr(FILE *f, AST *expr)
{
    Symbol *sym;
    int c;
    const char *name = "";

    switch (expr->kind) {
    case AST_INTEGER:
        fprintf(f, "%lu", (unsigned long)expr->d.ival);
        break;
    case AST_STRING:
        c = expr->d.string[0];
        if (isprint(c)) {
            fprintf(f, "'%c'", c);
        } else {
            fprintf(f, "%d", c);
        }
        break;
    case AST_IDENTIFIER:
    case AST_HWREG:
    case AST_MEMREF:
    case AST_ARRAYREF:
        PrintLHS(f, expr, 0);
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
            if (expr->left->kind == AST_IDENTIFIER) {
                name = expr->left->d.string;
                sym = LookupSymbol(name);
            }
        }
        if (!sym) {
            ERROR("undefined identifier %s in function call", name);
        } else if (sym->type == SYM_BUILTIN) {
            Builtin *b = sym->val;
            (*b->printit)(f, b, expr->right);
        } else if (sym->type == SYM_FUNCTION) {
            PrintFuncCall(f, sym, expr->right);
        } else {
            ERROR("not a function");
        }
        break;
    case AST_RANGEREF:
        ERROR("cannot handle range in expression yet");
        break;
    default:
        ERROR("Internal error, bad expression");
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
            ERROR("unknown operator %d\n", op);
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
                ERROR("Unknown symbol %s", expr->d.string);
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
                            ERROR("label %s not on longword boundary", sym->name);
                        else
                            *valid = 0;
                        return 0;
                    }
                    return lref->asmval >> 2;
                }
                /* otherwise fall through */
            default:
                if (reportError)
                    ERROR("Symbol %s is not constant", expr->d.string);
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
            ERROR("Used hardware register where constant is expected");
        else
            *valid = 0;
        break;
    default:
        if (reportError)
            ERROR("Bad constant expression");
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
