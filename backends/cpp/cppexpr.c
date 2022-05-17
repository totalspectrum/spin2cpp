/*
 * Spin to C/C++ converter
 * Copyright 2011-2021 Total Spectrum Software Inc.
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

static void PrintStringLiteral(Flexbuf *f, const char *s);

static int
isBooleanOperator(AST *expr)
{
    int x;

    if (expr->kind != AST_OPERATOR)
        return 0;

    x = expr->d.ival;
    switch (x) {
    case K_BOOL_NOT:
    case K_BOOL_AND:
    case K_BOOL_OR:
    case K_LE:
    case K_LEU:
    case '<':
    case K_LTU:
    case K_GE:
    case K_GEU:
    case '>':
    case K_GTU:
    case K_EQ:
    case K_NE:
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
  if (expr->d.ival != K_NEGATE)
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
    int divBy4 = (flags & PRINTEXPR_GASIMM) != 0;
    Label *lab = (Label *)sym->val;

    if (current->pasmLabels && !(flags & PRINTEXPR_GASABS)) {
        if (divBy4) {
            flexbuf_printf(f, "(_lbl_(%s)/4)", sym->user_name);
        } else {
            flexbuf_printf(f, "_lbl_(%s)", sym->user_name);
        }
    } else if (current->gasPasm || (flags & PRINTEXPR_DEBUG)) {
        flexbuf_printf(f, "%s", sym->user_name);
    } else {
        flexbuf_printf(f, "(%s(", ref ? "" : "*");
        PrintType(f, lab->type, 0);
        flexbuf_printf(f, "*)&%s[%d])", current->datname, lab->hubval);
    }
}

static void
PrintHere(Flexbuf *f, AST *ast, int flags)
{
    int divBy4 = (flags & PRINTEXPR_GASIMM) != 0;
    Symbol *org = (Symbol *)ast->d.ptr;  // last origin value seen
    
    if (current->pasmLabels) {
        if (divBy4) {
            flexbuf_printf(f, "((. - %s)/4)", org->user_name);
        } else {
            flexbuf_printf(f, "(. - %s)", org->user_name);
        }
    } else if (current->gasPasm) {
        flexbuf_printf(f, ".");
    } else {
        ERROR(ast, "AST_HERE encountered in unexpected context");
    }
}

/* code to print an integer */
void
PrintInteger(Flexbuf *f, int32_t v, int flags)
{
    if (current->pasmLabels) {
        if ((flags & PRINTEXPR_GASOP) && !(flags & PRINTEXPR_GASIMM)) {
            v *= 4; // adjust for COG addressing
        }
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
PrintFloat(Flexbuf *f, int32_t v, int flags)
{
    bool printAsFloat;

    if (flags & PRINTEXPR_USEFLOATS) {
        printAsFloat = true;
    } else if (gl_fixedreal) {
        printAsFloat = false;
    } else {
        int language;
        if (curfunc) {
            language = curfunc->language;
        } else if (current) {
            language = current->curLanguage;
        } else {
            ERROR(NULL, "Unable to determine language");
            language = LANG_SPIN_SPIN1;
        }
        if ( IsSpinLang(language) ) {
            printAsFloat = false;
        } else {
            printAsFloat = true;
        }
    }
    if (printAsFloat) {
        if (gl_fixedreal) {
            flexbuf_printf(f, "%f", (float)(v)/(float)(1<<G_FIXPOINT));
        } else {
            flexbuf_printf(f, "%f", intAsFloat(v));
        }
        return;
    }
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

void
PrintObjConstName(Flexbuf *f, Module *P, const char* symname)
{
    if (gl_ccode || gl_gas_dat) {
        PrintUpper(f, P->classname);
        flexbuf_printf(f, "_");
        PrintUpper(f, symname);
    } else {
        flexbuf_printf(f, "%s::%s", P->classname, symname);
    }
}

void
CppPrintName(Flexbuf *f, const char *name, int flags)
{
    int c;
    if (flags & PRINTEXPR_INLINESYM) {
        flexbuf_printf(f, "%%[", name);
    }
    if (!strcmp(name, "clkfreq")) {
        if (gl_p2) {
            flexbuf_printf(f, "_clockfreq()");
        } else {
            flexbuf_printf(f, "_CLKFREQ");
        }
    } else {
        while (0 != (c = *name++)) {
            switch (c) {
            case '#':
                flexbuf_printf(f, "_R");
                break;
            case '%':
                flexbuf_printf(f, "_I");
                break;
            case '$':
                flexbuf_printf(f, "_S");
                break;
            default:
                flexbuf_addchar(f, c);
                break;
            }
        }
    }
    if (flags & PRINTEXPR_INLINESYM) {
        flexbuf_printf(f, "]");
    }

}

/* code to print a symbol to a file */
void
PrintSymbol(Flexbuf *f, Symbol *sym, int flags)
{
    switch (sym->kind) {
    case SYM_LABEL:
        PrintLabel(f, sym, flags);
        break;
    case SYM_CONSTANT:
    case SYM_FLOAT_CONSTANT:
        if (IsReservedWord(sym->user_name) && !(flags & PRINTEXPR_USECONST)) {
            int32_t v;
            v = EvalConstExpr((AST *)sym->val);
            PrintInteger(f, v, flags);
        } else if (gl_ccode || gl_gas_dat) {
            PrintObjConstName(f, current, sym->user_name);
        } else {
            CppPrintName(f, sym->user_name, flags);
        }
        break;
    case SYM_PARAMETER:
        if (curfunc && curfunc->parmarray && !(flags & PRINTEXPR_INLINESYM)) {
            flexbuf_printf(f, "%s[%d]", curfunc->parmarray, curfunc->result_in_parmarray+sym->offset/4);
        } else {
            CppPrintName(f, sym->user_name, flags);
        }
        break;              
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
        if (curfunc && curfunc->localarray && !(flags & PRINTEXPR_INLINESYM)) {
            flexbuf_printf(f, "%s[%d]", curfunc->localarray, sym->offset/4);
        } else {
            CppPrintName(f, sym->user_name, flags);
        }
        break;              
    case SYM_RESULT:
        if (curfunc && curfunc->result_in_parmarray && !(flags & PRINTEXPR_INLINESYM)) {
            flexbuf_printf(f, "%s[0]", curfunc->parmarray);
        } else {
            CppPrintName(f, sym->user_name, flags);
        }
        break;
    case SYM_VARIABLE:
        if (sym->flags & SYMF_GLOBAL) {
            if (!strcmp(sym->user_name, "__clkfreq_var")) {
                CppPrintName(f, "_clkfreq", flags);
                break;
            } else if (!strcmp(sym->user_name, "__clkmode_var")) {
                CppPrintName(f, "_clkmode", flags);
                break;
            }
        }
        if ( (gl_ccode || (curfunc && curfunc->force_static))
             && !(sym->flags & SYMF_GLOBAL) )
        {
            flexbuf_printf(f, "self->");
        }
        CppPrintName(f, sym->user_name, flags);
        break;
    case SYM_FUNCTION:
    default:
        CppPrintName(f, sym->user_name, 0);
        break;
    }
}

/* code to print a function call to a file */
/* localMethod is true for plain calls (like foo(x))
   and false for object references (like bar.foo(x))
*/
void
PrintFuncCall(Flexbuf *f, Symbol *sym, AST *params, AST *objtype, AST *objref)
{
    int is_static = 0;
    bool localMethod = false;
    
    Function *func = NULL;
    if (sym->kind == SYM_FUNCTION) {
        func = (Function *)sym->val;
        is_static = func->is_static;
        localMethod = (objtype == NULL) && (objref == NULL);
    }
    if (gl_ccode) {
        if (objref) {
            PrintExpr(f, objref, PRINTEXPR_TOPLEVEL);
        }
    } else {
        if (localMethod && curfunc && curfunc->force_static) {
            // need to call through an object
            flexbuf_printf(f, "self->");
        } else if (objref && objref->left) {
            PrintExpr(f, objref->left, PRINTEXPR_TOPLEVEL);
            flexbuf_printf(f, ".");
        }
    }
    /* check for object method call */
    flexbuf_printf(f, "%s(", sym->user_name);
    if ( (gl_ccode && !is_static)
         || (func && func->force_static)
        ) {
        if (objtype) {
            flexbuf_printf(f, "&");
            PrintExpr(f, objref->left, PRINTEXPR_DEFAULT);
        } else {
            flexbuf_printf(f, "self");
        }
        if (params)
            flexbuf_printf(f, ", ");
    }
    /* print the arguments */
    PrintExprList(f, params, PRINTEXPR_DEFAULT, func);
    flexbuf_printf(f, ")");
}

/* code to print coginit to a file */
void
PrintLabelCoginit(Flexbuf *f, AST *params)
{
    const char *funcname = gl_p2 ? "_cogstart_PASM" : "coginit";
    if (params->kind == AST_COGINIT) {
        params = params->left;
    } else {
        ERROR(params, "expected coginit");
        return;
    }

    if (params && params->left && IsConstExpr(params->left)) {
        int32_t cogid = EvalConstExpr(params->left);
        bool use_cognew = false;

        if (gl_p2) {
            use_cognew = (cogid == 16);
        } else {
            use_cognew = (cogid >= NUM_COGS || cogid < 0);
        }
        if (use_cognew) {
            params = params->right;
            funcname = gl_p2 ? "_cognew" : "cognew";
        }
    }
    flexbuf_printf(f, "%s(", funcname);
    PrintExprList(f, params, PRINTEXPR_DEFAULT, NULL);
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
    if (!IsIdentifier(stack->left)) {
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
    if (IsIdentifier(func)) {
        sym = LookupAstSymbol(func, "coginit/cognew");
    } else if (func->kind == AST_FUNCCALL) {
        sym = LookupAstSymbol(func->left, "coginit/cognew");
        params = func->right;
    }
 
    if (!sym || sym->kind != SYM_FUNCTION) {
        ERROR(body, "coginit expected spin method");
        return;
    }
    flexbuf_printf(f, "Coginit__(");
    PrintExpr(f, cogid, PRINTEXPR_TOPLEVEL);
    flexbuf_printf(f, ", (void *)");
    /* need to find stack size */
    PrintStackWithSize(f, stack);
    flexbuf_printf(f, ", (void *)");
    if (gl_ccode && sym && sym->kind == SYM_FUNCTION) {
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
            PrintTypedExpr(f, NULL, params->left, PRINTEXPR_TOPLEVEL);
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
    if (IsSpinCoginit(params, NULL))  {
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
        PrintTypedExpr(f, NULL, left, flags);
        flexbuf_printf(f, " %s ", op);
        PrintTypedExpr(f, NULL, right, flags);
    } else if (right) {
        flexbuf_printf(f, "%s", op);
        PrintTypedExpr(f, NULL, right, flags);
    } else {
        PrintTypedExpr(f, NULL, left, flags);
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
    PrintTypedExpr(f, NULL, left, flags);
    flexbuf_printf(f, ", ");
    PrintTypedExpr(f, NULL, right, flags);
    flexbuf_printf(f, ")");
}

void
PrintOperator(Flexbuf *f, int op, AST *left, AST *right, int flags)
{
    char opstring[4];
    AST *subexpr;
    
    switch (op) {
    case K_HIGHMULT:
        PrintMacroExpr(f, "Highmult__", left, right, flags);
        break;
    case K_LE:
        PrintInOp(f, "<=", left, right, flags);
        break;
    case K_GE:
        PrintInOp(f, ">=", left, right, flags);
        break;
    case K_LEU:
        PrintInOp(f, "<=", left, right, flags | PRINTEXPR_FORCE_UNS);
        break;
    case K_GEU:
        PrintInOp(f, ">=", left, right, flags | PRINTEXPR_FORCE_UNS);
        break;
    case K_LTU:
        PrintInOp(f, "<", left, right, flags | PRINTEXPR_FORCE_UNS);
        break;
    case K_GTU:
        PrintInOp(f, ">", left, right, flags | PRINTEXPR_FORCE_UNS);
        break;
    case K_EQ:
        PrintInOp(f, "==", left, right, flags);
        break;
    case K_NE:
        PrintInOp(f, "!=", left, right, flags);
        break;
    case K_SHL:
        PrintInOp(f, "<<", left, right, flags);
        break;
    case K_SAR:
        PrintInOp(f, ">>", left, right, flags);
        break;
    case K_SHR:
        if (current->pasmLabels
            || (IsConstExpr(left) && EvalConstExpr(left) >= 0))
        {
            PrintInOp(f, ">>", left, right, flags);
        } else {
            PrintMacroExpr(f, "Shr__", left, right, flags);
        }
        break;
    case K_REV:
        flexbuf_printf(f, "__builtin_propeller_rev(");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, ", 32 - ");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case K_MODULUS:
        PrintInOp(f, "%", left, right, flags);
        break;
    case K_INCREMENT:
    case K_DECREMENT:
    {
        // we don't go through the usual PrintInOp because we
        // have to control the casting
        const char *str = op == K_INCREMENT ? "++" : "--";
        const char *prefix;
        const char *suffix;
        AST *finalType;
        int needCast = 0;
        prefix = left ? "" : str;
        suffix = left ? str : "";
        finalType = left ? ExprType(left) : ExprType(right);
        if (finalType && IsPointerType(finalType)) {
            needCast = 0 != curfunc->parmarray;
        }
        if (needCast) {
            flexbuf_addstr(f, "((");
            PrintType(f, finalType, 0);
            flexbuf_addstr(f, ")");
        }
        flexbuf_printf(f, "%s", prefix);
        PrintExpr(f, left ? left : right, flags);
        flexbuf_printf(f, "%s", suffix);
        if (needCast) {
            flexbuf_addstr(f, ")");
        }
        break;
    }
    case K_NEGATE:
        /* watch out for a special case: boolean operators get a negation as well,
	   so optimize away the double negate */
        /* similarly for - - x */
        if (isBooleanOperator(right) || (isNegateOperator(right))) {
	    PrintOperator(f, right->d.ival, right->left, right->right, flags);
	} else {
            PrintInOp(f, "-", left, right, flags);
	}
        break;
    case K_BIT_NOT:
        PrintLogicOp(f, "~", left, right, flags);
        break;
    case K_BOOL_AND:
        PrintInOp(f, "&&", left, right, flags);
        break;
    case K_BOOL_OR:
        PrintInOp(f, "||", left, right, flags);
        break;
    case K_BOOL_NOT:
        PrintInOp(f, "!", left, right, flags);
        break;
    case K_LIMITMIN:
        PrintMacroExpr(f, "Max__", left, right, flags);
        break;
    case K_LIMITMAX:
        PrintMacroExpr(f, "Min__", left, right, flags);
        break;
    case K_ROTL:
        PrintMacroExpr(f, "Rotl__", left, right, flags);
        break;
    case K_ROTR:
        PrintMacroExpr(f, "Rotr__", left, right, flags);
        break;
    case K_ABS:
        flexbuf_printf(f, "abs(");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case K_SQRT:
        flexbuf_printf(f, "Sqrt__(");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case K_ZEROEXTEND:
        subexpr = SimpleOptimizeExpr(AstOperator('-', AstInteger(32), right));
        flexbuf_printf(f, "((uint32_t)");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, " << ");
        PrintExpr(f, subexpr, flags);
        flexbuf_printf(f, ") >> ");
        PrintExpr(f, subexpr, flags);
        break;
    case K_SIGNEXTEND:
        subexpr = SimpleOptimizeExpr(AstOperator('-', AstInteger(32), right));
        flexbuf_printf(f, "((int32_t)");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, " << ");
        PrintExpr(f, subexpr, flags);
        flexbuf_printf(f, ") >> ");
        PrintExpr(f, subexpr, flags);
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
    case K_ENCODE:
        flexbuf_printf(f, "BitEncode__(");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case K_DECODE:
        flexbuf_printf(f, "(1<<");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case K_FRAC64:
        flexbuf_printf(f, "( ((uint64_t)");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, ") << 32 ) / ((uint64_t)");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ")");
        break;
    case K_UNS_HIGHMULT:
        flexbuf_printf(f, "(int32_t)( ((uint64_t)");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, ") * ((uint64_t)");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ") >> 32 )");
        break;
    case K_SCAS:
        flexbuf_printf(f, "(int32_t)( ((int64_t)");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, ") * ((int64_t)");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ") >> 30 )");
        break;
    case K_UNS_DIV:
        flexbuf_printf(f, "(int32_t)( ((uint32_t)");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, ") / ((uint32_t)");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ") )");
        break;
    case K_UNS_MOD:
        flexbuf_printf(f, "(int32_t)( ((uint32_t)");
        PrintExpr(f, left, flags);
        flexbuf_printf(f, ") %% ((uint32_t)");
        PrintExpr(f, right, flags);
        flexbuf_printf(f, ") )");
        break;
    default:
        ERROR(NULL, "unsupported operator 0x%x", op);
        break;
    }
}

/*
 * code to print out a type declaration
 */
static void
doPrintType(Flexbuf *f, AST *typedecl, int addspace, int flags)
{
    int size;
    char *space = addspace ? " " : "";
    
    if (!typedecl) {
        typedecl = ast_type_generic;
    }
    if (typedecl->kind == AST_MODIFIER_VOLATILE) {
        doPrintType(f, typedecl->left, addspace, flags | ISVOLATILE);
        return;
    }
    if (typedecl->kind == AST_MODIFIER_CONST) {
        doPrintType(f, typedecl->left, addspace, flags | ISCONST);
        return;
    }
    if (typedecl->kind != AST_PTRTYPE && (flags & ISVOLATILE)) {
        flexbuf_printf(f, "volatile ");
    }
    if (typedecl->kind != AST_PTRTYPE && (flags & ISCONST)) {
        flexbuf_printf(f, "const ");
    }
    switch (typedecl->kind) {
    case AST_GENERICTYPE:
    case AST_INTTYPE:
    case AST_UNSIGNEDTYPE:
        size = EvalConstExpr(typedecl->left);
        if (typedecl->kind == AST_UNSIGNEDTYPE) {
            if (size == 1) {
                flexbuf_printf(f, "char%s", space);
                return;
            } else {
                flexbuf_printf(f, "u");
            }
        }
        switch (size) {
        case 1:
            flexbuf_printf(f, "int8_t%s", space);
            break;
        case 2:
            flexbuf_printf(f, "int16_t%s", space);
            break;
        case 4:
            //flexbuf_printf(f, "int32_t");
            flexbuf_printf(f, "%s%s", gl_intstring, space);
            break;
        case 8:
            flexbuf_printf(f, "int64_t%s", space);
            break;
        default:
            ERROR(typedecl, "unsupported integer size %d", size);
            break;
        }
        break;
    case AST_FLOATTYPE:
        size = EvalConstExpr(typedecl->left);
        if ( (curfunc && IsSpinLang(curfunc->language)) || gl_fixedreal) {
            // eventually we will want to really support float operands
            // but for now, treat floats as ints
            if (size == 4) {
                flexbuf_printf(f, "int32_t%s", space);
            } else {
                ERROR(typedecl, "unsupported float size %d", size);
            }
        } else {
            if (size == 4) {
                flexbuf_printf(f, "float%s", space);
            } else if (size == 8) {
                flexbuf_printf(f, "long double%s", space);
            } else {
                ERROR(typedecl, "unsupported float size %d", size);
            }
        }
        break;
    case AST_COPYREFTYPE:
        doPrintType(f, typedecl->left, 1, 0);
        break;
    case AST_REFTYPE:
    case AST_PTRTYPE:
    case AST_ARRAYTYPE:
        doPrintType(f, typedecl->left, 1, 0);
        flexbuf_printf(f, "*");
        if (0 != (flags & ISVOLATILE)) {
            flexbuf_printf(f, "volatile ");
        }
        if (0 != (flags & ISCONST)) {
            flexbuf_printf(f, "const ");
        }
        break;
    case AST_TUPLE_TYPE:
        flexbuf_printf(f, "Tuple%d__%s", AstListLen(typedecl), space);
        break;
    case AST_VOIDTYPE:
        flexbuf_printf(f, "void%s", space);
        break;
    case AST_OBJECT:
        {
            Module *P = (Module *)typedecl->d.ptr;
            flexbuf_printf(f, "%s%s", P->classname, space);
        }
        break;
    case AST_FUNCTYPE:
        {
            flexbuf_printf(f, "void *");
        }
        break;
    default:
        ERROR(typedecl, "unknown type declaration %d", typedecl->kind);
        break;
    }
}

void
PrintType(Flexbuf *f, AST *typedecl, int flags)
{
    doPrintType(f, typedecl, 1, flags);
}
void
PrintCastType(Flexbuf *f, AST *typedecl)
{
    doPrintType(f, typedecl, 0, 0);
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
PrintLHS(Flexbuf *f, AST *expr, int flags)
{
    Symbol *sym;
    HwReg *hw;
    int assignment = (flags & PRINTEXPR_ASSIGNMENT) != 0;
    int ref = (flags & PRINTEXPR_ISREF) != 0;

    //flags &= ~PRINTEXPR_ISREF;
    switch (expr->kind) {
    case AST_RESULT:
        if (flags & PRINTEXPR_DEBUG) {
            flexbuf_addstr(f, "RESULT");
        } else if (!curfunc) {
            ERROR(expr, "RESULT keyword outside of function");
        } else if (curfunc->result_in_parmarray) {
            flexbuf_printf(f, "%s[0]", curfunc->parmarray);
        } else {
            PrintLHS(f, curfunc->resultexpr, flags);
        }
        break;
    case AST_IDENTIFIER:
    case AST_LOCAL_IDENTIFIER:
        sym = LookupAstSymbol(expr, NULL);
        if (flags & PRINTEXPR_DEBUG) {
            flexbuf_addstr(f, GetUserIdentifierName(expr));
        } else if (!sym) {
            ERROR_UNKNOWN_SYMBOL(expr);
        } else {
            if (sym->kind == SYM_FUNCTION || sym->kind == SYM_BUILTIN) {
                if (assignment) {
                    ERROR(expr, "symbol %s on left hand side of assignment", sym->user_name);
                } else {
                    if (sym->kind == SYM_BUILTIN) {
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
    case AST_DATADDROF:
    case AST_ABSADDROF:
        if (!ref) {
            flexbuf_printf(f, "(%s)", gl_intstring);
        }
        PrintLHS(f, expr->left, flags | PRINTEXPR_ISREF);
        break;
    case AST_ARRAYREF:
        flags &= ~PRINTEXPR_ASSIGNMENT;
        if (expr->left && IsIdentifier(expr->left)) {
            sym = LookupAstSymbol(expr->left, NULL);
        } else {
            sym = NULL;
        }
        if (flags & PRINTEXPR_DEBUG) {
            PrintLHS(f, expr->left, flags);
        } else if (sym && sym->kind == SYM_LOCALVAR && curfunc && curfunc->localarray) {
            flexbuf_printf(f, "%s[%d + ", curfunc->localarray, sym->offset/4);
            PrintExpr(f, expr->right, flags);
            flexbuf_printf(f, "]");
        } else {
            if (sym && (!IsArrayOrPointerSymbol(sym))) {
//                ERROR(expr, "array dereference of bad symbol %s", sym->user_name);
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
        if (flags & (PRINTEXPR_GAS|PRINTEXPR_DEBUG)) {
            flexbuf_printf(f, "%s", hw->name);
        } else {
            flexbuf_printf(f, "%s", hw->cname);
        }
        break;
    case AST_MEMREF:
        flags &= ~(PRINTEXPR_ISREF|PRINTEXPR_ASSIGNMENT);
#if 0
        flexbuf_printf(f, "((");
        PrintType(f, expr->left);
        flexbuf_printf(f, "*)");
        PrintExpr(f, expr->right, flags & ~PRINTEXPR_ISREF);
        flexbuf_printf(f, ")");
#else
	PrintTypedExpr(f, NewAST(AST_PTRTYPE, expr->left, NULL), expr->right, flags & ~PRINTEXPR_ISREF);
#endif
        break;
    case AST_STRING:
        PrintStringLiteral(f, expr->d.string);
        break;
    case AST_METHODREF:
        PrintExpr(f, expr->left, flags);
        flexbuf_addstr(f, ".");
        {
            const char *thename = GetIdentifierName(expr->right);
            if (!thename) {
                ERROR(expr, "Cannot find name %s", thename);
                break;
            }
            flexbuf_addstr(f, thename);
        }
        break;
    case AST_CAST:
        PrintTypedExpr(f, expr->left, expr->right, flags);
        break;
    default:
        ERROR(expr, "bad target for assignment");
        break;
    }
}

void
PrintRangeAssign(Flexbuf *f, AST *dst, AST *src, int flags)
{
    AST *newast;
    AST *lhs, *rhs;
    int op;
    
    ERROR(dst, "Internal error, range assignment was left to backend");
    newast = TransformRangeAssign(dst, src, 0, 0);
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
    ERROR(src, "Internal error, range use was left to backend");
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
    } else if (c == 9) {
        flexbuf_printf(f, "\\t");
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
        if (!elem && ast->kind == AST_STRING) {
            elem = ast;
        }
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
    case AST_LOCAL_IDENTIFIER:
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
    AST *desttype;
    if (lhs->kind == AST_EXPRLIST) {
        ERROR(lhs, "Possible compiler problem, should not see multiple assignments in cppexpr.c");
        // multiple assignment
        int n = AstListLen(lhs);
        flexbuf_printf(f, "({ Tuple%d__ tmp__ = ", n);
        if (rhs && rhs->kind == AST_EXPRLIST) {
            flexbuf_printf(f, "MakeTuple%d__(", n, n);
            PrintExprList(f, rhs, PRINTEXPR_DEFAULT, NULL);
            flexbuf_printf(f, ")");
        } else {
            PrintExpr(f, rhs, PRINTEXPR_DEFAULT);
        }
        flexbuf_printf(f, "; ");
        n = 0;
        while (lhs) {
            if (lhs->left && lhs->left->kind != AST_EMPTY) {
                PrintLHS(f, lhs->left, PRINTEXPR_ASSIGNMENT);
                flexbuf_printf(f, " = tmp__.v%d; ", n);
            }
            n++;
            lhs = lhs->right;
        }
        flexbuf_printf(f, " })");
    } else if (lhs->kind == AST_RANGEREF) {
        PrintRangeAssign(f, lhs, rhs, flags);
    } else if (lhs->kind == AST_SPRREF) {
        flexbuf_printf(f, "cogmem_put__(");
        PrintExpr(f, lhs->left, flags);
        flexbuf_printf(f, ", ");
        PrintTypedExpr(f, ast_type_long, rhs, flags);
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
        desttype = ExprType(lhs);
        flexbuf_printf(f, " = ");
        /*
         * Normally we put parentheses around operators, but at
         * the top of an assignment we should not have to.
         */
        PrintTypedExpr(f, desttype, rhs, flags | PRINTEXPR_TOPLEVEL);
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

void
PrintGasExpr(Flexbuf *f, AST *expr, bool useFloat)
{
    int flags = PRINTEXPR_GAS;
    
    if (!expr) {
        return;
    }
    if (expr->kind == AST_COMMENTEDNODE) {
        expr = expr->left;
        if (!expr) return;
    }

    if (useFloat) {
        flags |= PRINTEXPR_USEFLOATS;
    } else if (IsFloatConst(expr)) {
        int32_t v = EvalConstExpr(expr);
        PrintInteger(f, v, flags);
        return;
    }
    switch (expr->kind) {
    case AST_ADDROF:
        PrintLHS(f, expr->left, PRINTEXPR_GAS);
        break;
    case AST_ABSADDROF:
        PrintLHS(f, expr->left, PRINTEXPR_GAS|PRINTEXPR_GASABS);
        break;
    default:
        PrintExpr(f, expr, flags);
        break;
    }
}

/* code to print an expression to a file */
void
PrintExpr(Flexbuf *f, AST *expr, int flags)
{
    Symbol *sym;
    AST *objref;
    int c;

    if (!expr) {
        return;
    }
    if (expr->kind == AST_COMMENTEDNODE) {
        expr = expr->left;
        if (!expr) return;
    }
    if (expr->kind == AST_COMMENT) {
        return;
    }
    if (flags & PRINTEXPR_TOPLEVEL) {
        flags &= ~PRINTEXPR_TOPLEVEL;
        if (expr->kind == AST_ASSIGN) {
            PrintAssign(f, expr->left, expr->right, flags);
            return;
        } else if (expr->kind == AST_OPERATOR && !isBooleanOperator(expr)) {
            PrintOperator(f, expr->d.ival, expr->left, expr->right, flags);
            return;
        }
        /* otherwise fall through */
    }
    objref = NULL;
    sym = NULL;
    switch (expr->kind) {
    case AST_UNSIGNEDTYPE:
    case AST_INTTYPE:
    case AST_FLOATTYPE:
    case AST_PTRTYPE:
        PrintType(f, expr, flags);
        break;
    case AST_INTEGER:
        PrintInteger(f, (int32_t)expr->d.ival, flags);
        break;
    case AST_FLOAT:
        PrintFloat(f, (int32_t)expr->d.ival, flags);
        break;
    case AST_HERE:
        PrintHere(f, expr, flags);
        break;
    case AST_STRING:
        if (IsSpinLang(curfunc->language)) {
            if (strlen(expr->d.string) > 1)  {
                ERROR(expr, "string too long, expected a single character");
            }
            c = expr->d.string[0];
        
            if (isprint(c)) {
                flexbuf_putc('\'', f);
                PrintStringChar(f, c);
                flexbuf_putc('\'', f);
            } else {
                flexbuf_printf(f, "%d", c);
            }
        } else {
            PrintStringLiteral(f, expr->d.string);
        }
        break;
    case AST_STRINGPTR:
        //flexbuf_printf(f, "(char *)");
        PrintStringList(f, expr->left);
        break;
    case AST_CONSTANT:
        flexbuf_printf(f, "0x%x", EvalConstExpr(expr->left));
        break;      
    case AST_ADDROF:
    case AST_ABSADDROF:
    {
        AST *addr = expr->left;
        // special case array dereferences
        if (addr && addr->kind == AST_ARRAYREF
            && addr->right && addr->right->kind == AST_INTEGER
            && addr->right->d.ival == 0)
        {
            addr = addr->left;
            flexbuf_printf(f, "(");
            PrintLHS(f, addr, flags | PRINTEXPR_ISREF);
        } else {
            flexbuf_printf(f, "(&");
            PrintLHS(f, addr, flags & ~PRINTEXPR_ISREF);
        }
        flexbuf_printf(f, ")");
        break;
    }
    case AST_DATADDROF:
        flexbuf_printf(f, "(%s)((", gl_intstring);
        PrintLHS(f, expr->left, flags);
        flexbuf_printf(f, ")+%s)", current->datname);
        break;
    case AST_CONDRESULT:
        flexbuf_printf(f, "((");
        PrintBoolExpr(f, expr->left, flags);
        flexbuf_printf(f, ") ? ");
        PrintExpr(f, expr->right->left, flags | PRINTEXPR_TOPLEVEL);
        flexbuf_printf(f, " : ");
        PrintExpr(f, expr->right->right, flags | PRINTEXPR_TOPLEVEL);
        flexbuf_printf(f, ")");
        break;
    case AST_MASKMOVE:
        flexbuf_printf(f, "(");
        PrintExpr(f, expr->left, flags | PRINTEXPR_TOPLEVEL);
        flexbuf_printf(f, " & ");
        PrintHexExpr(f, expr->right->left, flags);
        flexbuf_printf(f, ") | ");
        PrintHexExpr(f, expr->right->right, flags);
        break;
    case AST_LOCAL_IDENTIFIER:
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
    case AST_METHODREF:
        {
            const char *thename;
            AST *objtype;
            
            thename = GetIdentifierName(expr->right);
            if (!thename) {
                return;
            }
            objref = expr->left;
            objtype = ExprType(objref);
            if (!IsClassType(objtype)) {
                ERROR(expr, "request for %s in something that is not a class", thename);
                return;
            }
            sym = LookupMemberSymbol(expr, objtype, thename, NULL, NULL);
            if (!sym) {
                ERROR(expr, "%s is not a member", thename);
                return;
            }
            if (sym->kind == SYM_FUNCTION && gl_ccode) {
                flexbuf_printf(f, "%s_", ObjClassName(objtype));
            } else {
                PrintExpr(f, objref, flags | PRINTEXPR_TOPLEVEL);
                flexbuf_printf(f, ".%s", thename);
            }
        }
        break;
    case AST_FUNCCALL:
    {
        const char *thename;
        AST *objtype;
        
        if (expr->left && expr->left->kind == AST_METHODREF) {
            objref = expr->left;
            objtype = ExprType(objref->left);
            thename = GetIdentifierName(objref->right);
            if (!thename) {
                return;
            }
            if (!IsClassType(objtype)) {
                ERROR(expr, "request for %s in something that is not a class", thename);
                return;
            }
            sym = LookupMemberSymbol(expr, objtype, thename, NULL, NULL);
            if (!sym) {
                ERROR(expr, "%s is not a member", thename);
            }
        } else {
            objtype = NULL;
            objref = NULL;
            sym = LookupAstSymbol(expr->left, NULL);
            if (gl_ccode && sym && sym->kind == SYM_FUNCTION)
                flexbuf_printf(f, "%s_", current->classname);
        }
        if (!sym) {
            /* if we can't find a symbol, punt and stick whatever identifier
               we can find in the output */
            const char *name = GetIdentifierName(expr->left);
            if (name) {
                flexbuf_printf(f, "%s(", name);
                PrintExprList(f, expr->right, flags, NULL);
                flexbuf_printf(f, ")");
            }
        } else if (sym->kind == SYM_BUILTIN) {
            Builtin *b = (Builtin *)sym->val;
            (*b->printit)(f, b, expr->right);
        } else if (sym->kind == SYM_FUNCTION) {
            PrintFuncCall(f, sym, expr->right, objtype, objref);
        } else {
            ERROR(expr, "%s is not a function", sym->user_name);
        }
        break;
    }
    case AST_PRINT:
        flexbuf_printf(f, "printf(");
        PrintExprList(f, expr->right, PRINTEXPR_DEFAULT, NULL);
        flexbuf_printf(f, ")");
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
    {
        Module *P;
        sym = LookupMethodRef(expr, &P, NULL);
        
        PrintObjConstName(f, P, sym->user_name);
        break;
    }
    case AST_TRYENV:
        flexbuf_printf(f, "__extension__({ AbortHook__ *stack__ = abortChain__, here__; ");
        flexbuf_printf(f, "%s tmp__; abortChain__ = &here__; tmp__ = ", gl_intstring);
        PrintExpr(f, expr->left, PRINTEXPR_DEFAULT);
        flexbuf_printf(f, "; abortChain__ = stack__; tmp__; })");
        break;
    case AST_SETJMP:
        flexbuf_printf(f, "setjmp(abortChain__->jmp)");
        break;
    case AST_CATCHRESULT:
        flexbuf_printf(f, "abortChain__->val");
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
    case AST_CAST:
        PrintTypedExpr(f, expr->left, expr->right, flags);
        break;
    case AST_SIZEOF:
        flexbuf_printf(f, "sizeof(");
        PrintExpr(f, expr->left, flags);
        flexbuf_printf(f, ")");
        break;
    case AST_VA_ARG:
        flexbuf_printf(f, "va_arg(");
        PrintExpr(f, expr->left, flags);
        flexbuf_printf(f, ",");
        PrintExpr(f, expr->right, flags);
        flexbuf_printf(f, ")");
        break;
    case AST_STMTLIST:
        flexbuf_printf(f, "({");
        PrintStatementList(f, expr, 0);
        flexbuf_printf(f, "})");
        break;
    case AST_SELF:
        flexbuf_printf(f, "this");
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
        case K_BOOL_NOT:
            flexbuf_printf(f, "!(");
            PrintBoolExpr(f, rhs, flags);
            flexbuf_printf(f, ")");
            break;
        case K_BOOL_OR:
        case K_BOOL_AND:
            flexbuf_printf(f, "(");
            PrintBoolExpr(f, lhs, flags);
            flexbuf_printf(f, "%s", op == K_BOOL_OR ? ") || (" : ") && (");
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

/*
 * print an expression list
 * if this is in a function call,
 * "func" will be the called function,
 * otherwise "func" will be NULL
 */
void
PrintExprList(Flexbuf *f, AST *list, int flags, Function *func)
{
    int needcomma = 0;
    AST *paramlist = NULL;
    AST *paramtype = NULL;
    
    if (func) {
        paramlist = func->params;
    }
    while (list) {
        if (list->kind != AST_EXPRLIST) {
            ERROR(list, "expected expression list");
            return;
        }
        if (needcomma) {
            flexbuf_printf(f, ", ");
        }
        paramtype = NULL;
        if (paramlist) {
            AST *paramid = paramlist->left;
            Symbol *sym;
            paramlist = paramlist->right;
            if (paramid && IsIdentifier(paramid)) {
                if (paramid->kind == AST_LOCAL_IDENTIFIER) {
                    paramid = paramid->left;
                }
                sym = FindSymbol(&func->localsyms, paramid->d.string);
                if (sym && sym->kind == SYM_PARAMETER) {
                    paramtype = (AST *)sym->val;
                }
            }
        }
        PrintTypedExpr(f, paramtype, list->left, flags | PRINTEXPR_TOPLEVEL);
        needcomma = 1;
        list = list->right;
    }
}


/*
 * Print a typed expression, including cast if necessary
 */
void
PrintTypedExpr(Flexbuf *f, AST *casttype, AST *expr, int flags)
{
    AST *et;
    bool addZero = false;
    bool needCloseParen = false;
    bool needCast = false;
    
    et = ExprType(expr);
    if (flags & PRINTEXPR_GAS) {
        PrintExpr(f, expr, flags);
        return;
    }
    if (et && et->kind == AST_VOIDTYPE) {
        addZero = true;
        et = NULL;
    }

    if ( (flags & PRINTEXPR_FORCE_UNS) ) {
        if (!casttype) {
            casttype = ast_type_unsigned_long;
        }
        if (!et || !IsUnsignedType(et)) {
            needCast = true;
        }
    }

    if (!CompatibleTypes(et, casttype) || needCast) {
        needCloseParen = !(flags & PRINTEXPR_TOPLEVEL);
	if (needCloseParen) {
	    flexbuf_printf(f, "(");
	}
        flexbuf_printf(f, "(");
        PrintCastType(f, casttype);
        flexbuf_printf(f, ")");
    }
    if (addZero) {
        flexbuf_printf(f, "(");
    }
    PrintExpr(f, expr, flags);
    if (addZero) {
        flexbuf_printf(f, ", 0)");
    }
    if (needCloseParen) {
        flexbuf_printf(f, ")");
    }
}
