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

static void PrintObjectSym(FILE *f, Symbol *objsym, AST *expr);

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

/* code to check if a coginit invocation is for a spin method */
/* if it is, returns a pointer to the spin method */
Function *
IsSpinCoginit(AST *params)
{
    AST *exprlist, *func;
    Symbol *sym = NULL;
    if (!params || !params->left || params->kind != AST_COGINIT) {
        return 0;
    }
    exprlist = params->left;
    exprlist = exprlist->right; // skip over cog id
    if (exprlist->kind != AST_EXPRLIST || !exprlist->left) {
        ERROR(params, "coginit/cognew expected expression");
        return 0;
    }
    func = exprlist->left;
    if (func->kind == AST_IDENTIFIER) {
        sym = LookupAstSymbol(func, "coginit/cognew");
        if (sym && sym->type == SYM_FUNCTION) {
            return (Function *)sym->val;
        }
    }
    if (func->kind == AST_FUNCCALL) {
        /* FIXME? Spin requires that it be a local method; do we care? */
        sym = FindFuncSymbol(func, NULL, NULL);
        if (sym) {
            return (Function *)sym->val;
        }
    }
    return 0;
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
    AST *hiexpr;

    if (dst->right->right == NULL) {
        loexpr = dst->right->left;
    } else {
        int hi, lo;
        loexpr = dst->right->right;
	hiexpr = dst->right->left;
	lo = EvalConstExpr(loexpr);
	hi = EvalConstExpr(hiexpr);
	if (hi < lo) {
	  AST *tmp = loexpr;
	  loexpr = hiexpr;
	  hiexpr = tmp;
	}

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
        nbits = (hi - lo + 1);
        loexpr = AstInteger(lo);
    }
    mask = AstInteger((1U<<nbits) - 1);

    /* we want to end up with:
       ((src->left >> lo) & mask)
    */
    val = AstOperator(T_SAR, src->left, loexpr);
    val = AstOperator('&', val, mask);
    if (reverse) {
        val = AstOperator(T_REV, val, AstInteger(nbits));
    }
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
        if (rhs->kind == AST_OPERATOR && !isBooleanOperator(rhs)) {
            PrintOperator(f, rhs->d.ival, rhs->left, rhs->right);
        } else {
            PrintExpr(f, rhs);
        }
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
            ParserState *P = objast->d.ptr;
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
        if (rval == 0)
	    return rval;
        return lval / rval;
    case T_MODULUS:
        if (rval == 0)
	    return rval;
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
    if (IsFloatType(le.type) || IsFloatType(re.type)) {
        return floatExpr(EvalFloatOperator(op, intAsFloat(le.val), intAsFloat(re.val), valid));
    }
    return intExpr(EvalIntOperator(op, le.val, re.val, valid));
}

#define PASM_FLAG 0x01

static ExprVal EvalExpr(AST *expr, unsigned flags, int *valid);

/*
 * evaluate an expression in a particular parser state
 */
static ExprVal
EvalExprInState(ParserState *P, AST *expr, unsigned flags, int *valid)
{
    ParserState *saveState;
    Function *saveFunc;
    ExprVal ret;

    saveState = current;
    saveFunc = curfunc;
    current = P;
    curfunc = 0;
    ret = EvalExpr(expr, flags, valid);
    current = saveState;
    curfunc = saveFunc;
    return ret;
}

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
    int kind;
    
    if (!expr)
        return intExpr(0);

    kind = expr->kind;
    switch (kind) {
    case AST_INTEGER:
        return intExpr(expr->d.ival);

    case AST_FLOAT:
        return floatExpr(intAsFloat(expr->d.ival));

    case AST_STRING:
        return intExpr(expr->d.string[0]);

    case AST_TOFLOAT:
        lval = EvalExpr(expr->left, flags, valid);
        if ( !IsIntOrGenericType(lval.type)) {
            ERROR(expr, "applying float to a non integer expression");
        }
        return floatExpr((float)(lval.val));

    case AST_TRUNC:
        lval = EvalExpr(expr->left, flags, valid);
        if (!IsFloatType(lval.type)) {
            ERROR(expr, "applying trunc to a non float expression");
        }
        return intExpr((int)intAsFloat(lval.val));

    case AST_ROUND:
        lval = EvalExpr(expr->left, flags, valid);
        if (!IsFloatType(lval.type)) {
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
        ret = EvalExprInState(GetObjectPtr(objsym), sym->val, flags, valid);
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
    case AST_ABSADDROF:
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
            if (kind == AST_ABSADDROF) {
                ERROR(expr, "@@@ operator requires the --gas directive");
            }
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
    if (valid && IsFloatType(eval.type))
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
    e.type = ast_type_long;
    e.val = x;
    return e;
}

ExprVal floatExpr(float f)
{
    ExprVal e;
    e.type = ast_type_float;
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
    case AST_GENERICTYPE:
    case AST_FLOATTYPE:
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
        type = (AST *)sym->val;
        return type->left && type->left->kind == AST_ARRAYDECL;
    case SYM_LABEL:
        return 1;
    default:
        return 0;
    }
    return IsArrayType(type);
}

/* find function symbol in a function call */
Symbol *
FindFuncSymbol(AST *expr, AST **objrefPtr, Symbol **objsymPtr)
{
    AST *objref = NULL;
    Symbol *objsym = NULL;
    Symbol *sym = NULL;
    
    if (expr->left && expr->left->kind == AST_METHODREF) {
        const char *thename;
        objref = expr->left->left;
        objsym = LookupAstSymbol(objref, "object reference");
        if (!objsym) return NULL;
        if (objsym->type != SYM_OBJECT) {
            ERROR(expr, "%s is not an object", objsym->name);
            return NULL;
        }
        thename = expr->left->right->d.string;
        sym = LookupObjSymbol(expr, objsym, thename);
        if (!sym || sym->type != SYM_FUNCTION) {
            ERROR(expr, "%s is not a method of %s", thename, objsym->name);
            return NULL;
        }
    } else {
        sym = LookupAstSymbol(expr->left, "function call");
    }
    if (objsymPtr) *objsymPtr = objsym;
    if (objrefPtr) *objrefPtr = objref;
    return sym;
}

int
IsFloatType(AST *type)
{
    if (type->kind == AST_FLOATTYPE)
        return 1;
    return 0;
}

int
IsGenericType(AST *type)
{
    return type->kind == AST_GENERICTYPE;
}

int
IsIntType(AST *type)
{
    if (type->kind == AST_INTTYPE || type->kind == AST_UNSIGNEDTYPE)
        return 1;
    return 0;
}

/*
 * figure out an expression's type
 */
AST *
ExprType(AST *expr)
{
    return ast_type_generic;
}

