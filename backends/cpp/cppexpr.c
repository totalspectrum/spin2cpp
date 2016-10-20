/*
 * Spin to C/C++ converter
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling expressions
 */

#include "spinc.h"
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "outcpp.h"

#define gl_ccode (gl_output == OUTPUT_C)

static void PrintObjectSym(Flexbuf *f, Symbol *objsym, AST *expr, int flags);

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

static int
isNegateOperator(AST *expr)
{
  if (expr->kind != AST_OPERATOR)
    return 0;
  if (expr->d.ival != T_NEGATE)
    return 0;
  return 1;
}

/* code to print a label to a file
 * if "ref" is nonzero this is an array reference, so do not
 * dereference again
 */
static void
PrintLabel(Flexbuf *f, Symbol *sym, int flags)
{
    int ref = (flags & PRINTEXPR_ISREF) != 0;
    Label *lab = (Label *)sym->val;

    if (current->printLabelsVerbatim) {
        if (current->fixImmediate) {
            flexbuf_printf(f, "((%s-..start)/4)", sym->name);
        } else {
            flexbuf_printf(f, "%s", sym->name);
        }
    } else {
        flexbuf_printf(f, "(%s(", ref ? "" : "*");
        PrintType(f, lab->type);
        flexbuf_printf(f, " *)&%s[%d])", current->datname, lab->offset);
    }
}

/* code to print an integer */
void
PrintInteger(Flexbuf *f, int32_t v)
{
    if (current->printLabelsVerbatim) {
        if (v > -10 && v < 10) {
            flexbuf_printf(f, "%ld", (long)v);
        } else {
            flexbuf_printf(f, "$%lx", (long)(uint32_t)v);
        }
    }
    else if (v == (int32_t)0x80000000)
        flexbuf_printf(f, "(%s)0x%lxU", gl_intstring, (long)(uint32_t)v);
    else
        flexbuf_printf(f, "%ld", (long)v);
}

/* code to print a float */
void
PrintFloat(Flexbuf *f, int32_t v)
{
    if (v < 0)
        flexbuf_printf(f, "(%s)0x%lx", gl_intstring, (long)(uint32_t)v);
    else
        flexbuf_printf(f, "0x%lx", (long)v);
}

static void
PrintUpper(Flexbuf *f, const char *name)
{
    int c;
    while ((c = *name++) != 0) {
        flexbuf_addchar(f, toupper(c));
    }
}

static void
PrintObjConstName(Flexbuf *f, Module *P, Symbol *sym)
{
    if (gl_ccode) {
        PrintUpper(f, P->classname);
        flexbuf_printf(f, "_");
        PrintUpper(f, sym->name);
    } else {
        flexbuf_printf(f, "%s::%s", P->classname, sym->name);
    }
}

/* code to print a symbol to a file */
void
PrintSymbol(Flexbuf *f, Symbol *sym, int flags)
{
    switch (sym->type) {
    case SYM_LABEL:
        PrintLabel(f, sym, flags);
        break;
    case SYM_CONSTANT:
        if (IsReservedWord(sym->name)) {
            int32_t v;
            v = EvalConstExpr((AST *)sym->val);
            PrintInteger(f, v);
        } else if (gl_ccode) {
            PrintObjConstName(f, current, sym);
        } else {
            flexbuf_printf(f, "%s", sym->name);
        }
        break;
    case SYM_FLOAT_CONSTANT:
        PrintFloat(f, EvalConstExpr((AST*)sym->val));
        break;
    case SYM_PARAMETER:
        if (curfunc && curfunc->parmarray) {
            flexbuf_printf(f, "%s[%d]", curfunc->parmarray, curfunc->result_in_parmarray+sym->offset/4);
        } else {
            flexbuf_printf(f, "%s", sym->name);
        }
        break;              
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
        if (curfunc && curfunc->localarray) {
            flexbuf_printf(f, "%s[%d]", curfunc->localarray, sym->offset/4);
        } else {
            flexbuf_printf(f, "%s", sym->name);
        }
        break;              
    case SYM_RESULT:
        if (curfunc && curfunc->result_in_parmarray) {
            flexbuf_printf(f, "%s[0]", curfunc->parmarray);
        } else {
            flexbuf_printf(f, "%s", sym->name);
        }
        break;
    case SYM_VARIABLE:
        if ( (gl_ccode || (curfunc && curfunc->force_static))
             && !(sym->flags & SYMF_GLOBAL) )
        {
            flexbuf_printf(f, "self->%s", sym->name);
        } else {
            flexbuf_printf(f, "%s", sym->name);
        }
        break;
    case SYM_FUNCTION:
    default:
        flexbuf_printf(f, "%s", sym->name);
        break;
    }
}

/* code to print a function call to a file */
void
PrintFuncCall(Flexbuf *f, Symbol *sym, AST *params, Symbol *objsym, AST *objref)
{
    int is_static = 0;
    if (sym->type == SYM_FUNCTION) {
        Function *func;

        func = (Function *)sym->val;
        is_static = func->is_static;
    }
    /* check for object method call */
    flexbuf_printf(f, "%s(", sym->name);
    if (gl_ccode && !is_static) {
        if (objsym) {
            flexbuf_printf(f, "&");
            PrintObjectSym(f, objsym, objref, PRINTEXPR_DEFAULT);
        } else {
            flexbuf_printf(f, "self");
        }
        if (params)
            flexbuf_printf(f, ", ");
    }
    PrintExprList(f, params, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ")");
}

/* code to print coginit to a file */
void
PrintLabelCoginit(Flexbuf *f, AST *params)
{
    const char *funcname = "coginit";
    if (params->kind == AST_COGINIT) {
        params = params->left;
    } else {
        ERROR(params, "expected coginit");
        return;
    }

    if (params && params->left && IsConstExpr(params->left)) {
        int32_t cogid = EvalConstExpr(params->left);
        if (cogid >= NUM_COGS || cogid < 0) {
            params = params->right;
            funcname = "cognew";
        }
    }
    flexbuf_printf(f, "%s(", funcname);
    PrintExprList(f, params, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ")");
}

void
PrintStackWithSize(Flexbuf *f, AST *origstack)
{
    AST *stack = origstack;
    AST *stype;
    int stacksize;
    
    Symbol *sym;
    if (stack->kind != AST_ADDROF) {
        ERROR(stack, "non-address given for stack in coginit");
        return;
    }
    stack = stack->left;
    if (stack->kind != AST_ARRAYREF || !stack->left) {
        ERROR(stack, "coginit stack is not part of an array");
        return;
    }
    if (stack->left->kind != AST_IDENTIFIER) {
        ERROR(stack, "coginit stack too complicated");
        return;
    }
    sym = LookupAstSymbol(stack->left, "coginit/cognew");
    if (!sym) {
        return;
    }
    stype = (AST *)sym->val;
    if (!stype || stype->kind != AST_ARRAYTYPE) {
        ERROR(stack, "coginit stack is not array");
        return;
    }
    stacksize = EvalConstExpr(stype->right) * TypeSize(stype->left);

    /* now change the array reference to use the top of stack */
    PrintSymbol(f, sym, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ", %d", stacksize);
}

void
PrintSpinCoginit(Flexbuf *f, AST *body)
{
    AST *cogid;
    AST *func;
    AST *stack;
    AST *params = NULL;
    int n = 0;
    
    Symbol *sym = NULL;

    if (body->kind == AST_COGINIT) {
        body = body->left;
    } else {
        ERROR(params, "expected coginit");
        return;
    }
    if (!body || body->kind != AST_EXPRLIST) {
        ERROR(body, "Expected expression list");
        return;
    }
    cogid = body->left; body = body->right;
    if (!body || body->kind != AST_EXPRLIST) {
        ERROR(body, "Expected expression in coginit");
        return;
    }
    func = body->left; stack = body->right;
    if (!stack || !func) {
        ERROR(body, "coginit of spin method requires function and stack");
        return;
    }
    if (stack->kind != AST_EXPRLIST) {
        ERROR(stack, "coginit: expected stack expression");
        return;
    }
    if (stack->right != 0) {
        ERROR(stack, "coginit: extra parameters after stack");
        return;
    }
    stack = stack->left;
    if (func->kind == AST_IDENTIFIER) {
        sym = LookupAstSymbol(func, "coginit/cognew");
    } else if (func->kind == AST_FUNCCALL) {
        sym = LookupAstSymbol(func->left, "coginit/cognew");
        params = func->right;
    }
 
    if (!sym || sym->type != SYM_FUNCTION) {
        ERROR(body, "coginit expected spin method");
        return;
    }
    flexbuf_printf(f, "Coginit__(");
    PrintExpr(f, cogid, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ", (void *)");
    /* need to find stack size */
    PrintStackWithSize(f, stack);
    flexbuf_printf(f, ", (void *)");
    if (gl_ccode && sym && sym->type == SYM_FUNCTION) {
        flexbuf_printf(f, "%s_", current->classname);
    }
    PrintSymbol(f, sym, PRINTEXPR_DEFAULT);

    /* print parameters, and pad with 0's */
    while (params || n < 4) {
        if (params && params->kind != AST_EXPRLIST) {
            ERROR(params, "expected expression list");
            return;
        }
        flexbuf_printf(f, ", ");
        if (params) {
            PrintExpr(f, params->left, PRINTEXPR_DEFAULT);
            params = params->right;
        } else {
            flexbuf_printf(f, "0");
        }
        n++;
    }
    if (n > 4) {
        ERROR(body, "too many arguments to spin method in coginit/cognew");
    }
    flexbuf_printf(f, ")");
}

void
PrintCogInit(Flexbuf *f, AST *params)
{
    if (!params || !params->left) {
        ERROR(params, "coginit/cognew requires parameters");
        return;
    }
    if (IsSpinCoginit(params))  {
        PrintSpinCoginit(f, params);
    } else {
        PrintLabelCoginit(f, params);
    }
}

/* code to print left operator right
 */
static void
PrintInOp(Flexbuf *f, const char *op, AST *left, AST *right, int flags)
{
    if (left && right) {
        PrintExpr(f, left, flags);
        flexbuf_printf(f, " %s ", op);
        PrintExpr(f, right, flags);
    } else if (right) {
        flexbuf_printf(f, "%s", op);
        PrintExpr(f, right, flags);
    } else {
        PrintExpr(f, left, flags);
        flexbuf_printf(f, "%s", op);
    }
}

/* code to print left operator right where operator is a bit manipulation
 * operator
 * just like PrintInOp, but prints integer constants in hex
 */
static void
PrintHexExpr(Flexbuf *f, AST *left, int flags)
{
    if (left->kind == AST_INTEGER || left->kind == AST_FLOAT) {
        flexbuf_printf(f, "0x%x", EvalConstExpr(left));
    } else {
        PrintExpr(f, left, flags);
    }
}

static void
PrintLogicOp(Flexbuf *f, const char *op, AST *left, AST *right, int flags)
{
    if (left && right) {
        PrintHexExpr(f, left, flags);
        flexbuf_printf(f, " %s ", op);
        PrintHexExpr(f, right, flags);
    } else if (right) {
        flexbuf_printf(f, "%s", op);
        PrintHexExpr(f, right, flags);
    } else {
        PrintHexExpr(f, left, flags);
        flexbuf_printf(f, "%s", op);
    }
}

static void
PrintMacroExpr(Flexbuf *f, const char *name, AST *left, AST *right, int flags)
{
    flexbuf_printf(f, "%s(", name);
    PrintExpr(f, left, flags);
    flexbuf_printf(f, ", ");
    PrintExpr(f, right, flags);
    flexbuf_printf(f, ")");
}

void
PrintOperator(Flexbuf *f, int op, AST *left, AST *right, int flags)
{
    char opstring[4];

    switch (op) {
    case T_HIGHMULT:
        PrintMacroExpr(f, "Highmult__", left, right, flags);
        break;
    case T_LE:
        PrintInOp(f, "<=", left, right, flags);
        break;
    case T_GE:
        PrintInOp(f, ">=", left, right, flags);
        break;
    case T_EQ:
        PrintInOp(f, "==", left, right, flags);
        break;
    case T_NE:
        PrintInOp(f, "!=", left, right, flags);
        break;
    case T_SHL:
        PrintInOp(f, "<<", left, right, flags);
        break;
    case T_SAR:
        PrintInOp(f, ">>", left, right, flags);
        break;
    case T_SHR:
        if (current->printLabelsVerbatim
            || (IsConstExpr(left) && EvalConstExpr(left) >= 0))
        {
            PrintInOp(f, ">>", left, right, flags);
        } else {
            PrintMacroExpr(f, "Shr__", left, right, flags);
        }
        break;
    case T_REV:
        flexbuf_printf(f, "__builtin_propeller_rev(");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, ", 32 - ");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case T_MODULUS:
        PrintInOp(f, "%", left, right, flags);
        break;
    case T_INCREMENT:
        PrintInOp(f, "++", left, right, flags);
        break;
    case T_DECREMENT:
        PrintInOp(f, "--", left, right, flags);
        break;
    case T_NEGATE:
        /* watch out for a special case: boolean operators get a negation as well,
	   so optimize away the double negate */
        /* similarly for - - x */
        if (isBooleanOperator(right) || (isNegateOperator(right))) {
	    PrintOperator(f, right->d.ival, right->left, right->right, flags);
	} else {
            PrintInOp(f, "-", left, right, flags);
	}
        break;
    case T_BIT_NOT:
        PrintLogicOp(f, "~", left, right, flags);
        break;
    case T_AND:
        PrintInOp(f, "&&", left, right, flags);
        break;
    case T_OR:
        PrintInOp(f, "||", left, right, flags);
        break;
    case T_NOT:
        PrintInOp(f, "!", left, right, flags);
        break;
    case T_LIMITMIN:
        PrintMacroExpr(f, "Max__", left, right, flags);
        break;
    case T_LIMITMAX:
        PrintMacroExpr(f, "Min__", left, right, flags);
        break;
    case T_ROTL:
        PrintMacroExpr(f, "Rotl__", left, right, flags);
        break;
    case T_ROTR:
        PrintMacroExpr(f, "Rotr__", left, right, flags);
        break;
    case T_ABS:
        flexbuf_printf(f, "abs(");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case T_SQRT:
        flexbuf_printf(f, "Sqrt__(");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case '?':
        if (left) {
            flexbuf_printf(f, "RandForw__(");
            PrintExpr(f, left, flags);
            flexbuf_printf(f, ")");
        } else {
            flexbuf_printf(f, "RandBack__(");
            PrintExpr(f, right, flags);
            flexbuf_printf(f, ")");
        }
        break;
    case '&':
    case '|':
    case '^':
        opstring[0] = op;
        opstring[1] = 0;
        PrintLogicOp(f, opstring, left, right, flags);
        break;
    case '+':
    case '-':
    case '/':
    case '*':
    case '<':
    case '>':
        opstring[0] = op;
        opstring[1] = 0;
        PrintInOp(f, opstring, left, right, flags);
        break;
    case T_ENCODE:
        flexbuf_printf(f, "BitEncode__(");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case T_DECODE:
        flexbuf_printf(f, "(1<<");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    default:
        ERROR(NULL, "unsupported operator %d", op);
        break;
    }
}

/*
 * code to print out a type declaration
 */
static void
doPrintType(Flexbuf *f, AST *typedecl, int fromPtr)
{
    int size;

    switch (typedecl->kind) {
    case AST_GENERICTYPE:
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
        size = EvalConstExpr(typedecl->left);
        if (typedecl->kind == AST_UNSIGNEDTYPE) {
            if (size == 1 && fromPtr) {
                flexbuf_printf(f, "char");
                return;
            }
            flexbuf_printf(f, "u");
        }
        switch (size) {
        case 1:
            flexbuf_printf(f, "int8_t");
            break;
        case 2:
            flexbuf_printf(f, "int16_t");
            break;
        case 4:
            //flexbuf_printf(f, "int32_t");
            flexbuf_printf(f, "%s", gl_intstring);
            break;
        default:
            ERROR(typedecl, "unsupported integer size %d", size);
            break;
        }
        break;
    case AST_FLOATTYPE:
        size = EvalConstExpr(typedecl->left);
        if (size == 4) {
            flexbuf_printf(f, "float");
        } else if (size == 8) {
            flexbuf_printf(f, "long double");
        } else {
            ERROR(typedecl, "unsupported float size %d", size);
        }
        break;
    case AST_PTRTYPE:
        doPrintType(f, typedecl->left, 1);
        flexbuf_printf(f, " *");
        break;
    case AST_VOIDTYPE:
        flexbuf_printf(f, "void");
        break;
    default:
        ERROR(typedecl, "unknown type declaration %d", typedecl->kind);
        break;
    }
}

void
PrintType(Flexbuf *f, AST *typedecl)
{
    doPrintType(f, typedecl, 0);
}

/* code to print a source expression (could be an array reference or
 * range)
 * if "assignment" is true then we are in an assignment operator, so
 * only certain types of symbols are valid
 * if "ref" is true then we are planning to use the expression
 * as a reference, so no extra dereferencing should be added to
 * e.g. labels, and plain symbols and such should be cast appropriately
 */
static void
PrintLHS(Flexbuf *f, AST *expr, int flags)
{
    Symbol *sym;
    HwReg *hw;
    int assignment = (flags & PRINTEXPR_ASSIGNMENT) != 0;
    int ref = (flags & PRINTEXPR_ISREF) != 0;
    switch (expr->kind) {
    case AST_RESULT:
        if (!curfunc) {
            ERROR(expr, "RESULT keyword outside of function");
        } else if (curfunc->result_in_parmarray) {
            flexbuf_printf(f, "%s[0]", curfunc->parmarray);
        } else {
            PrintLHS(f, curfunc->resultexpr, flags);
        }
        break;
    case AST_IDENTIFIER:
        sym = LookupSymbol(expr->d.string);
        if (!sym) {
            ERROR_UNKNOWN_SYMBOL(expr);
        } else {
            if (sym->type == SYM_FUNCTION || sym->type == SYM_BUILTIN) {
                if (assignment) {
                    ERROR(expr, "symbol %s on left hand side of assignment", sym->name);
                } else {
                    if (sym->type == SYM_BUILTIN) {
                        Builtin *b = (Builtin *)sym->val;
                        (*b->printit)(f, b, NULL);
                    } else {
                        if (gl_ccode) {
                            flexbuf_printf(f, "%s_", current->classname);
                        }
                        PrintFuncCall(f, sym, NULL, NULL, NULL);
                    }
                }
            } else {
                PrintSymbol(f, sym, flags);
            }
        }
        break;
    case AST_ADDROF:
        if (!ref) {
            flexbuf_printf(f, "(%s)", gl_intstring);
        }
        PrintLHS(f, expr->left, flags | (PRINTEXPR_ISREF));
        break;
    case AST_ARRAYREF:
        if (expr->left && expr->left->kind == AST_IDENTIFIER) {
            sym = LookupSymbol(expr->left->d.string);
        } else {
            sym = NULL;
        }
        if (sym && sym->type == SYM_LOCALVAR && curfunc && curfunc->localarray) {
            flexbuf_printf(f, "%s[%d + ", curfunc->localarray, sym->offset/4);
            PrintExpr(f, expr->right, flags);
            flexbuf_printf(f, "]");
        } else {
            if (sym && !IsArraySymbol(sym)) {
//                ERROR(expr, "array dereference of bad symbol %s", sym->name);
                flexbuf_printf(f, "(&");
                PrintLHS(f, expr->left, flags | PRINTEXPR_ISREF);
                flexbuf_printf(f, ")");
            } else {
                PrintLHS(f, expr->left, flags | PRINTEXPR_ISREF);
            }
            flexbuf_printf(f, "[");
            PrintExpr(f, expr->right, flags);
            flexbuf_printf(f, "]");
        }
        break;
    case AST_HWREG:
        hw = (HwReg *)expr->d.ptr;
        flexbuf_printf(f, "%s", hw->cname);
        break;
    case AST_MEMREF:
        flexbuf_printf(f, "((");
        PrintType(f, expr->left);
        flexbuf_printf(f, " *)");
        PrintExpr(f, expr->right, flags);
        flexbuf_printf(f, ")");
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
PrintPostfix(Flexbuf *f, AST *expr, int toplevel, int flags)
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
        PrintAssign(f, expr->left, target, flags);
    } else {
        flexbuf_printf(f, "PostEffect__(");
        PrintExpr(f, expr->left, flags);
        flexbuf_printf(f, ", ");
        PrintExpr(f, target, flags);
        flexbuf_printf(f, ")");
    }
}

void
PrintRangeAssign(Flexbuf *f, AST *dst, AST *src, int flags)
{
    AST *newast;
    AST *lhs, *rhs;
    int op;
    
    ERROR(dst, "Internal error: range assignment was left to backend");
    newast = TransformRangeAssign(dst, src, 0);
    /* try to pretty print if we can */
    lhs = newast->left;
    rhs = newast->right;
    if (rhs->kind == AST_OPERATOR && AstMatch(lhs, rhs->left)) {
        op = rhs->d.ival;
        if (op == '&' || op == '|' || op == '^') {
            rhs = rhs->right;
            PrintLHS(f, lhs, flags | PRINTEXPR_ASSIGNMENT);
            flexbuf_printf(f, " %c= ", op);
            if (rhs->kind == AST_INTEGER) {
                flexbuf_printf(f, "0x%x", rhs->d.ival);
            } else {
                PrintExpr(f, rhs, flags);
            }
            return;
        }
    }
    PrintAssign(f, newast->left, newast->right, flags);
}

/*
 * print a range use
 */
void
PrintRangeUse(Flexbuf *f, AST *src, int flags)
{
    AST *expr = TransformRangeUse(src);
    ERROR(src, "Internal error: range use was left to backend");
    PrintExpr(f, expr, flags);
}

static void
PrintStringChar(Flexbuf *f, int c)
{
    if (isprint(c)) {
        if (c == '\\' || c == '"' || c == '\'') {
            flexbuf_printf(f, "\\%c", c);
        } else {
            flexbuf_printf(f, "%c", c);
        }
    } else if (c == 10) {
        flexbuf_printf(f, "\\n");
    } else if (c == 13) {
        flexbuf_printf(f, "\\r");
    } else {
        flexbuf_printf(f, "\\%03o", c);
    }
}

static void
PrintStringList(Flexbuf *f, AST *ast)
{
    int c;
    const char *s;
    AST *elem;
    flexbuf_printf(f, "\"");
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
    flexbuf_printf(f, "\"");
}

static void
PrintStringLiteral(Flexbuf *f, const char *s)
{
    int c;
    flexbuf_printf(f, "\"");
    while ((c = *s++) != 0) {
        PrintStringChar(f, c);
    }
    flexbuf_printf(f, "\"");
}

/* code to print an object, treating it as an address */
void
PrintAsAddr(Flexbuf *f, AST *expr, int flags)
{
    if (!expr)
        return;
    switch(expr->kind) {
    case AST_STRING:
        PrintStringLiteral(f, expr->d.string);
        break;
    case AST_ADDROF:
        flexbuf_printf(f, "&");
        PrintLHS(f, expr->left, flags);
        break;
    case AST_IDENTIFIER:
    case AST_RESULT:
    case AST_HWREG:
    case AST_MEMREF:
    case AST_ARRAYREF:
        PrintLHS(f, expr, flags | PRINTEXPR_ISREF);
        break;
    case AST_ASSIGN:
    default:
        flexbuf_printf(f, "(void *)(");
        PrintExpr(f, expr, flags);
        flexbuf_printf(f, ")");
        break;
    }
}

/*
 * code to print an assignment
 */
void
PrintAssign(Flexbuf *f, AST *lhs, AST *rhs, int flags)
{
    if (lhs->kind == AST_RANGEREF) {
        PrintRangeAssign(f, lhs, rhs, flags);
    } else if (lhs->kind == AST_SPRREF) {
        flexbuf_printf(f, "cogmem_put__(");
        PrintExpr(f, lhs->left, flags);
        flexbuf_printf(f, ", ");
        PrintExpr(f, rhs, flags);
        flexbuf_printf(f, ")");
    } else {
        /* in Spin an expression like
             arr := 1
           where arr is an array is equivalent to
             arr[0] := 1
        */
        if (IsArray(lhs)) {
            lhs = NewAST(AST_ARRAYREF, lhs, AstInteger(0));
        }

        /* shortcut certain expressions */
        if (rhs->kind == AST_OPERATOR &&
            (rhs->d.ival == '|'
             || rhs->d.ival == '&'
             || rhs->d.ival == '^')
            && AstMatch(lhs, rhs->left))
        {
            PrintLHS(f, lhs, flags | PRINTEXPR_ASSIGNMENT);
            flexbuf_printf(f, " %c= ", rhs->d.ival);
            if (rhs->right->kind == AST_INTEGER) {
                flexbuf_printf(f, "0x%x", rhs->right->d.ival);
            } else {
                PrintExpr(f, rhs->right, flags);
            }
            return;
        }
        PrintLHS(f, lhs, flags | PRINTEXPR_ASSIGNMENT);
        flexbuf_printf(f, " = ");
        /*
         * Normally we put parentheses around operators, but at
         * the top of an assignment we should not have to.
         */
        PrintExprToplevel(f, rhs, flags);
    }
}

void
PrintExprToplevel(Flexbuf *f, AST *expr, int flags)
{
    if (!expr) return;
    if (expr->kind == AST_ASSIGN) {
        PrintAssign(f, expr->left, expr->right, flags);
    } else if (expr->kind == AST_OPERATOR && !isBooleanOperator(expr)) {
        PrintOperator(f, expr->d.ival, expr->left, expr->right, flags);
    } else {
        PrintExpr(f, expr, flags);
    }
}

/*
 * print a lookup array; returns the number of elements in it
 */
int
PrintLookupArray(Flexbuf *f, AST *array, int flags)
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
                flexbuf_printf(f, "%d, ", i);
                sz++;
            }
        } else if (expr->kind == AST_STRING) {
            const char *s = expr->d.string;
            while (*s) {
                flexbuf_printf(f, "%d, ", *s);
                s++;
            }
        } else {
            PrintExpr(f, expr, flags);
            flexbuf_printf(f, ", ");
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
PrintLookExpr(Flexbuf *f, const char *name, AST *ev, AST *table)
{
    int len;
    AST *idx, *base;
    AST *arrid;
    int inExpr = 0;

    if (ev->kind != AST_LOOKEXPR
        || (table->kind != AST_EXPRLIST && table->kind != AST_TEMPARRAYUSE))
    {
        ERROR(ev, "Internal error in lookup");
        return;
    }
    base = ev->left;
    idx = ev->right;
    if (table->kind == AST_EXPRLIST) {
        // need to convert it to a temporary array here
        // also need to evaluate and save the index expression,
        // in case it has side effects and is used in the array
        // creation
        AST *idxvar;

        inExpr = 1;
        idxvar = AstTempVariable("i_");
        arrid = AstTempVariable("look_");

        flexbuf_printf(f, "__extension__({ ");
        flexbuf_printf(f, "%s %s = ", gl_intstring, idxvar->d.string );
        PrintExpr(f, idx, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, "; %s %s[] = {", gl_intstring, arrid->d.string );
        len = PrintLookupArray(f, table, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, "}; ");
        idx = idxvar;
    } else {
        len = EvalConstExpr(table->right);
        arrid = table->left;
    }
    flexbuf_printf(f, "%s(", name);
    PrintExpr(f, idx, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ", ");
    PrintExpr(f, base, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ", ");
    PrintExpr(f, arrid, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ", %u)", len);
    if (inExpr) {
        flexbuf_printf(f, "; })");
    }
}

/* print an object symbol */
static void
PrintObjectSym(Flexbuf *f, Symbol *objsym, AST *expr, int flags)
{
    int isArray = IsArraySymbol(objsym);

    if (objsym->type != SYM_OBJECT) {
        ERROR(expr, "Internal error, expecting an object symbol\n");
        return;
    }

    if (expr->kind == AST_ARRAYREF) {
        if (!isArray) {
            ERROR(expr, "%s is not an array of objects", objsym->name);
        }
        PrintLHS(f, expr, flags);
    } else {
        if (gl_ccode || (curfunc && curfunc->force_static) )
            flexbuf_printf(f, "self->");
        flexbuf_printf(f, "%s", objsym->name);
        if (isArray) {
            flexbuf_printf(f, "[0]");
        }
    }
}

void
PrintGasExpr(Flexbuf *f, AST *expr)
{
    if (!expr) {
        return;
    }
    if (expr->kind == AST_COMMENTEDNODE) {
        expr = expr->left;
        if (!expr) return;
    }

    switch (expr->kind) {
    case AST_ADDROF:
        PrintLHS(f, expr->left, PRINTEXPR_GAS);
        break;
    case AST_ABSADDROF:
        flexbuf_printf(f, "_%s + (", current->datname);
        PrintLHS(f, expr->left, PRINTEXPR_GAS);
        flexbuf_printf(f, " - ..start)");
        break;
    default:
        PrintExpr(f, expr, PRINTEXPR_GAS);
        break;
    }
}

/* code to print an expression to a file */
void
PrintExpr(Flexbuf *f, AST *expr, int flags)
{
    Symbol *sym, *objsym;
    AST *objref;
    int c;

    if (!expr) {
        return;
    }
    if (expr->kind == AST_COMMENTEDNODE) {
        expr = expr->left;
        if (!expr) return;
    }
    objref = NULL;
    objsym = sym = NULL;
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
            flexbuf_putc('\'', f);
            PrintStringChar(f, c);
            flexbuf_putc('\'', f);
        } else {
            flexbuf_printf(f, "%d", c);
        }
        break;
    case AST_STRINGPTR:
        flexbuf_printf(f, "(%s)", gl_intstring);
        PrintStringList(f, expr->left);
        break;
    case AST_CONSTANT:
        flexbuf_printf(f, "0x%x", EvalConstExpr(expr->left));
        break;      
    case AST_ADDROF:
    case AST_ABSADDROF:
        flexbuf_printf(f, "(%s)(&", gl_intstring);
        PrintLHS(f, expr->left, flags);
        flexbuf_printf(f, ")");
        break;
    case AST_DATADDROF:
        flexbuf_printf(f, "(%s)((", gl_intstring);
        PrintLHS(f, expr->left, flags);
        flexbuf_printf(f, ")+%s)", current->datname);
        break;
    case AST_CONDRESULT:
        flexbuf_printf(f, "((");
        PrintBoolExpr(f, expr->left, flags);
        flexbuf_printf(f, ") ? ");
        PrintExprToplevel(f, expr->right->left, flags);
        flexbuf_printf(f, " : ");
        PrintExprToplevel(f, expr->right->right, flags);
        flexbuf_printf(f, ")");
        break;
    case AST_MASKMOVE:
        flexbuf_printf(f, "(");
        PrintExprToplevel(f, expr->left, flags);
        flexbuf_printf(f, " & ");
        PrintHexExpr(f, expr->right->left, flags);
        flexbuf_printf(f, ") | ");
        PrintHexExpr(f, expr->right->right, flags);
        break;
    case AST_IDENTIFIER:
    case AST_HWREG:
    case AST_MEMREF:
    case AST_ARRAYREF:
    case AST_RESULT:
        PrintLHS(f, expr, flags);
        break;
    case AST_SPRREF:
        flexbuf_printf(f, "cogmem_get__(");
        PrintExpr(f, expr->left, flags);
        flexbuf_printf(f, ")");
        break;
    case AST_OPERATOR:
        if (isBooleanOperator(expr)) {
	    flexbuf_printf(f, "-");
	}
	flexbuf_printf(f, "(");
        PrintOperator(f, expr->d.ival, expr->left, expr->right, flags);
        flexbuf_printf(f, ")");
        break;
    case AST_ISBETWEEN:
        flexbuf_printf(f, "-(");
        PrintBoolExpr(f, expr, flags);
        flexbuf_printf(f, ")");
        break;
    case AST_ASSIGN:
        flexbuf_printf(f, "(");
        PrintAssign(f, expr->left, expr->right, flags);
        flexbuf_printf(f, ")");
        break;
    case AST_FUNCCALL:
        if (expr->left && expr->left->kind == AST_METHODREF) {
            const char *thename;
            objref = expr->left->left;
            objsym = LookupAstSymbol(objref, "object reference");
            if (!objsym) return;
            if (objsym->type != SYM_OBJECT) {
                ERROR(expr, "%s is not an object", objsym->name);
                return;
            }
            thename = expr->left->right->d.string;
            sym = LookupObjSymbol(expr, objsym, thename);
            if (!sym || sym->type != SYM_FUNCTION) {
                ERROR(expr, "%s is not a method of %s", thename, objsym->name);
                return;
            }
            if (gl_ccode) {
                flexbuf_printf(f, "%s_", ObjClassName(objsym));
            } else {
                PrintObjectSym(f, objsym, objref, flags);
                flexbuf_printf(f, ".");
            }
        } else {
            objsym = NULL;
            objref = NULL;
            sym = LookupAstSymbol(expr->left, "function call");
            if (gl_ccode && sym && sym->type == SYM_FUNCTION)
                flexbuf_printf(f, "%s_", current->classname);
        }
        if (!sym) {
            ; /* do nothing, printed error already */
        } else if (sym->type == SYM_BUILTIN) {
            Builtin *b = (Builtin *)sym->val;
            (*b->printit)(f, b, expr->right);
        } else if (sym->type == SYM_FUNCTION) {
            PrintFuncCall(f, sym, expr->right, objsym, objref);
        } else {
            ERROR(expr, "%s is not a function", sym->name);
        }
        break;
    case AST_COGINIT:
        PrintCogInit(f, expr);
        break;
    case AST_RANGEREF:
        PrintRangeUse(f, expr, flags);
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
        {
            AST *objast = (AST *)objsym->val;
            Module *P = (Module *)objast->d.ptr;
            
            PrintObjConstName(f, P, sym);
        }
        break;
    case AST_CATCH:
        flexbuf_printf(f, "__extension__({ AbortHook__ *stack__ = abortChain__, here__; ");
        flexbuf_printf(f, "%s tmp__; abortChain__ = &here__; ", gl_intstring);
        flexbuf_printf(f, "if (setjmp(here__.jmp) == 0) tmp__ = ");
        PrintExpr(f, expr->left, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, "; else tmp__ = here__.val; abortChain__ = stack__; tmp__; })");
        break;
    case AST_TRUNC:
    case AST_ROUND:
        if (!IsConstExpr(expr->left)) {
            ERROR(expr, "argument to trunc/float is not constant");
            break;
        }
        flexbuf_printf(f, "%d", EvalConstExpr(expr));
        break;
    case AST_SEQUENCE:
        flexbuf_printf(f, "( ");
        PrintExpr(f, expr->left, flags);
        if (expr->right) {
            flexbuf_printf(f, ", ");
            PrintExpr(f, expr->right, flags);
        }
        flexbuf_printf(f, " )");
        break;
    default:
        ERROR(expr, "Internal error, bad expression");
        break;
    }
}

void
PrintBoolExpr(Flexbuf *f, AST *expr, int flags)
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
            flexbuf_printf(f, "!(");
            PrintBoolExpr(f, rhs, flags);
            flexbuf_printf(f, ")");
            break;
        case T_OR:
        case T_AND:
            flexbuf_printf(f, "(");
            PrintBoolExpr(f, lhs, flags);
            flexbuf_printf(f, "%s", op == T_OR ? ") || (" : ") && (");
            PrintBoolExpr(f, rhs, flags);
            flexbuf_printf(f, ")");
            break;
        default:
            PrintOperator(f, op, lhs, rhs, flags);
            break;
        }
    } else if (expr->kind == AST_ISBETWEEN) {
        int onlyone = 0;
        if (IsConstExpr(expr->right->left) && IsConstExpr(expr->right->right)) {
            onlyone = 1;
        } else {
            flexbuf_printf(f, "(");
        }
        flexbuf_printf(f, "(");
        PrintExpr(f, expr->right->left, flags);
        flexbuf_printf(f, " <= ");
        PrintExpr(f, expr->left, flags);
        flexbuf_printf(f, ") && (");
        PrintExpr(f, expr->left, flags);
        flexbuf_printf(f, " <= ");
        PrintExpr(f, expr->right->right, flags);
        if (onlyone) {
            flexbuf_printf(f, ")");
        } else {
            flexbuf_printf(f, ")) || ((");
            PrintExpr(f, expr->right->left, flags);
            flexbuf_printf(f, " >= ");
            PrintExpr(f, expr->left, flags);
            flexbuf_printf(f, ") && (");
            PrintExpr(f, expr->left, flags);
            flexbuf_printf(f, " >= ");
            PrintExpr(f, expr->right->right, flags);
            flexbuf_printf(f, "))");
        }
    } else {
        PrintExpr(f, expr, flags);
    }
}

void
PrintExprList(Flexbuf *f, AST *list, int flags)
{
    int needcomma = 0;
    while (list) {
        if (list->kind != AST_EXPRLIST) {
            ERROR(list, "expected expression list");
            return;
        }
        if (needcomma) {
            flexbuf_printf(f, ", ");
        }
        PrintExpr(f, list->left, flags);
        needcomma = 1;
        list = list->right;
    }
}


/* code to print a builtin function call to a file */
void
defaultBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != b->numparameters) {
        ERROR(params, "wrong number of parameters to %s", b->name);
    }
    flexbuf_printf(f, "%s(", b->cname);
    PrintExprList(f, params, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ")");
}

/* code to print a waitpeq or waitpne function call to a file */
void
waitpeqBuiltin(Flexbuf *f, Builtin *b, AST *origparams)
{
    AST *a1;
    AST *a2;
    AST *a3;
    AST *params = origparams;
    if (AstListLen(params) != 3) {
        ERROR(params, "wrong number of parameters to %s", b->name);
    }
    a1 = params->left; params = params->right;
    a2 = params->left; params = params->right;
    a3 = params->left; params = params->right;
    if (!IsConstExpr(a3) || EvalConstExpr(a3) != 0) {
        ERROR(params, "Third parameter to %s must be 0", b->name);
        return;
    }
    flexbuf_printf(f, "%s(", b->cname);
    PrintExpr(f, a1, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ", ");
    PrintExpr(f, a2, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ")");
}

/* code to print a builtin variable reference call to a file */
void
defaultVariable(Flexbuf *f, Builtin *b, AST *params)
{
    flexbuf_printf(f, "%s", b->cname);
}

/* code to do memory copies; also does 1 byte fills */
void
memBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    int ismemcpy = !strcmp(b->cname, "memcpy") || !strcmp(b->cname, "memmove");
    AST *dst, *src, *count;

    dst = params->left;
    params = params->right;
    src = params->left;
    params = params->right;
    count = params->left;

    flexbuf_printf(f, "%s( (void *)", b->cname);
    PrintAsAddr(f, dst, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, ", ");
    if (ismemcpy) {
        flexbuf_printf(f, "(void *)");
        PrintAsAddr(f, src, PRINTEXPR_DEFAULT);
    } else {
        PrintExpr(f, src, PRINTEXPR_DEFAULT);
    }

    /* b->extradata is the size of memory we
       are working with
    */
    flexbuf_printf(f, ", %d*(", b->extradata);
    PrintExpr(f, count, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, "))");
}

/* code to do 2 and 4 byte memory fills */
void
memFillBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    const char *idxname;
    const char *valname;
    const char *type_name;
    const char *ptrname;
    AST *dst, *src, *count;

    dst = params->left;
    params = params->right;
    src = params->left;
    params = params->right;
    count = params->left;
    /* b->extradata is the size of memory we
       are working with
    */
    type_name = (b->extradata == 2) ? "uint16_t" : gl_intstring;

    /* if the source is 0, use memset instead */
    if (IsConstExpr(src) && EvalConstExpr(src) == 0) {
        flexbuf_printf(f, "memset( (void *)");
        PrintExpr(f, dst, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, ", 0, sizeof(%s)*", type_name);
        PrintExpr(f, count, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, ")");
        return;
    }
    idxname = NewTemporaryVariable("_fill_");
    valname = NewTemporaryVariable("_val_");
    ptrname = NewTemporaryVariable("_ptr_");
    flexbuf_printf(f, "{ %s %s; ", gl_intstring, idxname);
    flexbuf_printf(f, "%s *%s = (%s *)", type_name, ptrname, type_name);
    PrintAsAddr(f, dst, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, "; %s %s = ", type_name, valname);
    PrintExpr(f, src, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, "; for (%s = ", idxname);
    PrintExpr(f, count, PRINTEXPR_DEFAULT);
    flexbuf_printf(f, "; %s > 0; --%s) { ", idxname, idxname);
    flexbuf_printf(f, " *%s++ = %s; } }", ptrname, valname);
}

/* code to do strsize */
void
str1Builtin(Flexbuf *f, Builtin *b, AST *params)
{
    flexbuf_printf(f, "%s((char *) ", b->cname);
    PrintExpr(f, params->left, PRINTEXPR_DEFAULT); params = params->right;
    flexbuf_printf(f, ")");
}

/* code to do strcomp(a, b) */
void
strcompBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    flexbuf_printf(f, "-(0 == strcmp((char *)");
    PrintExpr(f, params->left, PRINTEXPR_DEFAULT); params = params->right;
    flexbuf_printf(f, ", (char *)");
    PrintExpr(f, params->left, PRINTEXPR_DEFAULT); params = params->right;
    flexbuf_printf(f, "))");
}

/* code to do reboot() -> clkset(0x80, 0) */
void
rebootBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != b->numparameters) {
        ERROR(params, "wrong number of parameters to %s", b->name);
    }
    flexbuf_printf(f, "clkset(0x80, 0)");
}
