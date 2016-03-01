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

static void PrintObjectSym(FILE *f, Symbol *objsym, AST *expr);

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
            fprintf(f, "%s[%d]", curfunc->parmarray, curfunc->result_in_parmarray+sym->offset/4);
        } else {
            fprintf(f, "%s", sym->name);
        }
        break;              
    case SYM_LOCALVAR:
        if (curfunc && curfunc->localarray) {
            fprintf(f, "%s[%d]", curfunc->localarray, sym->offset/4);
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
        if (gl_ccode || (curfunc && curfunc->force_static) ) {
            fprintf(f, "self->%s", sym->name);
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
PrintFuncCall(FILE *f, Symbol *sym, AST *params, Symbol *objsym, AST *objref)
{
    int is_static = 0;
    if (sym->type == SYM_FUNCTION) {
        Function *func;

        func = (Function *)sym->val;
        is_static = func->is_static;
    }
    /* check for object method call */
    fprintf(f, "%s(", sym->name);
    if (gl_ccode && !is_static) {
        if (objsym) {
            PrintObjectSym(f, objsym, objref);
        } else {
            fprintf(f, "self");
        }
        if (params)
            fprintf(f, ", ");
    }
    PrintExprList(f, params);
    fprintf(f, ")");
}

/* code to print coginit to a file */
void
PrintLabelCoginit(FILE *f, AST *params)
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
    fprintf(f, "%s(", funcname);
    PrintExprList(f, params);
    fprintf(f, ")");
}

void
PrintTopOfStack(FILE *f, AST *origstack)
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
    stacksize = EvalConstExpr(stype->right);

    /* now change the array reference to use the top of stack */
    fprintf(f, "&%s[%d]", sym->name, stacksize);
}

void
PrintSpinCoginit(FILE *f, AST *body)
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
    fprintf(f, "Coginit__(");
    PrintExpr(f, cogid);
    fprintf(f, ", (void *)");
    /* need to find stack size */
    PrintTopOfStack(f, stack);
    fprintf(f, ", (void *)");
    PrintSymbol(f, sym);

    /* print parameters, and pad with 0's */
    while (params || n < 4) {
        if (params && params->kind != AST_EXPRLIST) {
            ERROR(params, "expected expression list");
            return;
        }
        fprintf(f, ", ");
        if (params) {
            PrintExpr(f, params->left);
            params = params->right;
        } else {
            fprintf(f, "0");
        }
        n++;
    }
    if (n > 4) {
        ERROR(body, "too many arguments to spin method in coginit/cognew");
    }
    fprintf(f, ")");
}

void
PrintCogInit(FILE *f, AST *params)
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
    if (left->kind == AST_INTEGER || left->kind == AST_FLOAT) {
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
        if (current->printLabelsVerbatim
            || (IsConstExpr(left) && EvalConstExpr(left) >= 0))
        {
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
        /* watch out for a special case: boolean operators get a negation as well,
	   so optimize away the double negate */
        /* similarly for - - x */
        if (isBooleanOperator(right) || (isNegateOperator(right))) {
	    PrintOperator(f, right->d.ival, right->left, right->right);
	} else {
            PrintInOp(f, "-", left, right);
	}
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
static void
doPrintType(FILE *f, AST *typedecl, int fromPtr)
{
    int size;

    switch (typedecl->kind) {
    case AST_GENERICTYPE:
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
        size = EvalConstExpr(typedecl->left);
        if (typedecl->kind == AST_UNSIGNEDTYPE) {
            if (size == 1 && fromPtr) {
                fprintf(f, "char");
                return;
            }
            fprintf(f, "u");
        }
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
            ERROR(typedecl, "unsupported integer size %d", size);
            break;
        }
        break;
    case AST_FLOATTYPE:
        size = EvalConstExpr(typedecl->left);
        if (size == 4) {
            fprintf(f, "float");
        } else if (size == 8) {
            fprintf(f, "long double");
        } else {
            ERROR(typedecl, "unsupported float size %d", size);
        }
        break;
    case AST_PTRTYPE:
        doPrintType(f, typedecl->left, 1);
        fprintf(f, " *");
        break;
    case AST_VOIDTYPE:
        fprintf(f, "void");
        break;
    default:
        ERROR(typedecl, "unknown type declaration %d", typedecl->kind);
        break;
    }
}

void
PrintType(FILE *f, AST *typedecl)
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
                        PrintFuncCall(f, sym, NULL, NULL, NULL);
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
            fprintf(f, "%s[%d + ", curfunc->localarray, sym->offset/4);
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

void
PrintRangeAssign(FILE *f, AST *dst, AST *src)
{
    AST *newast;
    AST *lhs, *rhs;
    int op;
    
    newast = TransformRangeAssign(dst, src);
    /* try to pretty print if we can */
    lhs = newast->left;
    rhs = newast->right;
    if (rhs->kind == AST_OPERATOR && AstMatch(lhs, rhs->left)) {
        op = rhs->d.ival;
        if (op == '&' || op == '|' || op == '^') {
            rhs = rhs->right;
            PrintLHS(f, lhs, 1, 0);
            fprintf(f, " %c= ", op);
            if (rhs->kind == AST_INTEGER) {
                fprintf(f, "0x%x", rhs->d.ival);
            } else {
                PrintExpr(f, rhs);
            }
            return;
        }
    }
    PrintAssign(f, newast->left, newast->right);
}

/*
 * print a range use
 */
void
PrintRangeUse(FILE *f, AST *src)
{
    AST *expr = TransformRangeUse(src);
    PrintExpr(f, expr);
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
    } else if (lhs->kind == AST_SPRREF) {
        fprintf(f, "cogmem_put__(");
        PrintExpr(f, lhs->left);
        fprintf(f, ", ");
        PrintExpr(f, rhs);
        fprintf(f, ")");
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
        /*
         * Normally we put parentheses around operators, but at
         * the top of an assignment we should not have to.
         */
        PrintExprToplevel(f, rhs);
    }
}

void
PrintExprToplevel(FILE *f, AST *expr)
{
    if (expr->kind == AST_ASSIGN) {
        PrintAssign(f, expr->left, expr->right);
    } else if (expr->kind == AST_OPERATOR && !isBooleanOperator(expr)) {
        PrintOperator(f, expr->d.ival, expr->left, expr->right);
    } else {
        PrintExpr(f, expr);
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

        fprintf(f, "__extension__({ ");
        fprintf(f, "int32_t %s = ", idxvar->d.string );
        PrintExpr(f, idx);
        fprintf(f, "; int32_t %s[] = {", arrid->d.string );
        len = PrintLookupArray(f, table);
        fprintf(f, "}; ");
        idx = idxvar;
    } else {
        len = EvalConstExpr(table->right);
        arrid = table->left;
    }
    fprintf(f, "%s(", name);
    PrintExpr(f, idx);
    fprintf(f, ", ");
    PrintExpr(f, base);
    fprintf(f, ", ");
    PrintExpr(f, arrid);
    fprintf(f, ", %u)", len);
    if (inExpr) {
        fprintf(f, "; })");
    }
}

/* print an object symbol */
static void
PrintObjectSym(FILE *f, Symbol *objsym, AST *expr)
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
        PrintLHS(f, expr, 0, 0);
    } else {
        if (gl_ccode || (curfunc && curfunc->force_static) )
            fprintf(f, "self->");
        fprintf(f, "%s", objsym->name);
        if (isArray) {
            fprintf(f, "[0]");
        }
    }
}

void
PrintGasExpr(FILE *f, AST *expr)
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
        PrintLHS(f, expr->left, 0, 0);
        break;
    case AST_ABSADDROF:
        fprintf(f, "_%s + (", current->datname);
        PrintLHS(f, expr->left, 0, 0);
        fprintf(f, " - ..start)");
        break;
    default:
        PrintExpr(f, expr);
        break;
    }
}

/* code to print an expression to a file */
void
PrintExpr(FILE *f, AST *expr)
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
    case AST_ABSADDROF:
        fprintf(f, "(int32_t)(&");
        PrintLHS(f, expr->left, 0, 0);
        fprintf(f, ")");
        break;
    case AST_DATADDROF:
        fprintf(f, "(int32_t)((");
        PrintLHS(f, expr->left, 0, 0);
        fprintf(f, ")+%s)", current->datname);
        break;
    case AST_CONDRESULT:
        fprintf(f, "(");
        PrintBoolExpr(f, expr->left);
        fprintf(f, ") ? ");
        PrintExprToplevel(f, expr->right->left);
        fprintf(f, " : ");
        PrintExprToplevel(f, expr->right->right);
        break;
    case AST_IDENTIFIER:
    case AST_HWREG:
    case AST_MEMREF:
    case AST_ARRAYREF:
    case AST_RESULT:
        PrintLHS(f, expr, 0, 0);
        break;
    case AST_SPRREF:
        fprintf(f, "cogmem_get__(");
        PrintExpr(f, expr->left);
        fprintf(f, ")");
        break;
    case AST_OPERATOR:
        if (isBooleanOperator(expr)) {
	    fprintf(f, "-");
	}
	fprintf(f, "(");
        PrintOperator(f, expr->d.ival, expr->left, expr->right);
        fprintf(f, ")");
        break;
    case AST_ISBETWEEN:
        fprintf(f, "-(");
        PrintBoolExpr(f, expr);
        fprintf(f, ")");
        break;
    case AST_POSTEFFECT:
        if (current) {
	    current->needsPosteffect = 1;
	}
        PrintPostfix(f, expr, 0);
        break;
    case AST_ASSIGN:
        fprintf(f, "(");
        PrintAssign(f, expr->left, expr->right);
        fprintf(f, ")");
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
                fprintf(f, "%s_", ObjClassName(objsym));
            } else {
                PrintObjectSym(f, objsym, objref);
                fprintf(f, ".");
            }
        } else {
            objsym = NULL;
            objref = NULL;
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
            PrintFuncCall(f, sym, expr->right, objsym, objref);
        } else {
            ERROR(expr, "%s is not a function", sym->name);
        }
        break;
    case AST_COGINIT:
        PrintCogInit(f, expr);
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
            AST *objast = objsym->val;
            Module *P = objast->d.ptr;
            fprintf(f, "%s::%s", P->classname, sym->name);
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
    case AST_SEQUENCE:
        fprintf(f, "( ");
        PrintExpr(f, expr->left);
        fprintf(f, ", ");
        PrintExpr(f, expr->right);
        fprintf(f, " )");
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
    } else if (expr->kind == AST_ISBETWEEN) {
        int onlyone = 0;
        if (IsConstExpr(expr->right->left) && IsConstExpr(expr->right->right)) {
            onlyone = 1;
        } else {
            fprintf(f, "(");
        }
        fprintf(f, "(");
        PrintExpr(f, expr->right->left);
        fprintf(f, " <= ");
        PrintExpr(f, expr->left);
        fprintf(f, ") && (");
        PrintExpr(f, expr->left);
        fprintf(f, " <= ");
        PrintExpr(f, expr->right->right);
        if (onlyone) {
            fprintf(f, ")");
        } else {
            fprintf(f, ")) || ((");
            PrintExpr(f, expr->right->left);
            fprintf(f, " >= ");
            PrintExpr(f, expr->left);
            fprintf(f, ") && (");
            PrintExpr(f, expr->left);
            fprintf(f, " >= ");
            PrintExpr(f, expr->right->right);
            fprintf(f, "))");
        }
    } else {
        PrintExpr(f, expr);
    }
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

    /* b->extradata is the size of memory we
       are working with
    */
    fprintf(f, ", %d*(", b->extradata);
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

    dst = params->left;
    params = params->right;
    src = params->left;
    params = params->right;
    count = params->left;
    /* b->extradata is the size of memory we
       are working with
    */
    typename = (b->extradata == 2) ? "uint16_t" : "int32_t";

    /* if the source is 0, use memset instead */
    if (IsConstExpr(src) && EvalConstExpr(src) == 0) {
        fprintf(f, "memset( (void *)");
        PrintAsAddr(f, dst);
        fprintf(f, ", 0, sizeof(%s)*", typename);
        PrintExpr(f, count);
        fprintf(f, ")");
        return;
    }
    idxname = NewTemporaryVariable("_fill_");
    valname = NewTemporaryVariable("_val_");
    ptrname = NewTemporaryVariable("_ptr_");
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
    fprintf(f, "%s((char *) ", b->cname);
    PrintExpr(f, params->left); params = params->right;
    fprintf(f, ")");
}

/* code to do strcomp(a, b) */
void
strcompBuiltin(FILE *f, Builtin *b, AST *params)
{
    fprintf(f, "-(0 == strcmp((char *)");
    PrintExpr(f, params->left); params = params->right;
    fprintf(f, ", (char *)");
    PrintExpr(f, params->left); params = params->right;
    fprintf(f, "))");
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
