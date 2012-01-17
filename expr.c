#include "spinc.h"
#include <ctype.h>
#include <string.h>
#include <math.h>

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
LookupAstSymbol(AST *id, const char *msg)
{
    Symbol *sym;
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
    objstate = (ParserState *)obj->val;
    sym = FindSymbol(&objstate->objsyms, name);
    if (!sym) {
        ERROR(expr, "unknown identifier %s in %s", name, obj->name);
    }
    return sym;
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

    lab = (Label *)sym->val;
    fprintf(f, "(%s(", ref ? "" : "*");
    PrintType(f, lab->type);
    fprintf(f, " *)&dat[%d])", lab->offset);
}

/* code to print an integer */
void
PrintInteger(FILE *f, int32_t v)
{
    if (v == (int32_t)0x80000000)
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
    int32_t v;

    switch (sym->type) {
    case SYM_LABEL:
        PrintLabel(f, sym, 0);
        break;
    case SYM_CONSTANT:
        v = EvalConstExpr((AST *)sym->val);
        PrintInteger(f, v);
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
    case SYM_RESULT:
        if (curfunc && curfunc->result_in_parmarray) {
            fprintf(f, "%s[0]", curfunc->parmarray);
        } else {
            fprintf(f, "%s", sym->name);
        }
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
        fprintf(f, "Shr__(");
        PrintExpr(f, left);
        fprintf(f, ", ");
        PrintExpr(f, right);
        fprintf(f, ")");
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
    case '?':
        fprintf(f, "rand()");
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
    case T_ENCODE:
        fprintf(f, "(32 - __builtin_clz(");
        PrintExpr(f, right);
        fprintf(f, "))");
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
    }
    if (reverse) {
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

/*
 * print a range use
 */
void
PrintRangeUse(FILE *f, AST *src)
{
    int lo, hi;
    int reverse = 0;
    uint32_t mask;
    int nbits;

    if (src->left->kind != AST_HWREG) {
        ERROR(src, "range not applied to hardware register");
        return;
    }
    if (src->right->kind != AST_RANGE) {
        ERROR(src, "internal error: expecting range");
        return;
    }
    /* now handle the ordinary case */
    hi = EvalConstExpr(src->right->left);
    lo = EvalConstExpr(src->right->right);

    if (hi < lo) {
        int tmp;
        reverse = 1;
        tmp = lo; lo = hi; hi = tmp;
    }
    if (reverse) {
        ERROR(src, "cannot currently handle reversed range");
    }
    nbits = (hi - lo + 1);

    mask = ((1U<<nbits) - 1);


    fprintf(f, "((");
    PrintLHS(f, src->left, 1, 0);
    fprintf(f, " >> %d)", lo); 
    fprintf(f, " & 0x%x)", mask);
}


static void
PrintStringChar(FILE *f, int c)
{
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
        c = expr->d.string[0];
        if (isprint(c)) {
            fprintf(f, "'%c'", c);
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
        PrintPostfix(f, expr);
        break;
    case AST_ASSIGN:
        fprintf(f, "(");
        PrintAssign(f, expr->left, expr->right);
        fprintf(f, ")");
        break;
    case AST_FUNCCALL:
        if (expr->left && expr->left->kind == AST_METHODREF) {
            objsym = LookupAstSymbol(expr->left->left, "object reference");
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
            fprintf(f, "%s.", objsym->name);
        } else {
            sym = LookupAstSymbol(expr->left, "function call");
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
        PrintMacroExpr(f, "Lookup__", expr->left, expr->right);
        break;
    case AST_CONSTREF:
        if (!GetObjConstant(expr, &objsym, &sym))
            return;
        fprintf(f, "%s.%s", objsym->name, sym->name);
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
        op = expr->d.ival;
        switch (op) {
        case T_NOT:
            fprintf(f, "!(");
            PrintBoolExpr(f, expr->right);
            fprintf(f, ")");
            break;
        case T_OR:
        case T_AND:
            fprintf(f, "(");
            PrintBoolExpr(f, expr->left);
            fprintf(f, "%s", op == T_OR ? ") || (" : ") && (");
            PrintBoolExpr(f, expr->right);
            fprintf(f, ")");
            break;
        default:
            PrintOperator(f, op, expr->left, expr->right);
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
        current = objsym->val;
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
