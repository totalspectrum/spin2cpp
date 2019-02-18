/*
 * Spin to C/C++ converter
 * Copyright 2011-2019 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for BASIC specific features
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

AST *basic_get_float;
AST *basic_get_string;
AST *basic_get_integer;
AST *basic_read_line;

AST *basic_print_float;
AST *basic_print_string;
AST *basic_print_integer;
AST *basic_print_unsigned;
AST *basic_print_char;
AST *basic_print_nl;
AST *basic_put;

static AST *float_add;
static AST *float_sub;
static AST *float_mul;
static AST *float_div;
static AST *float_cmp;
static AST *float_fromuns;
static AST *float_fromint;
static AST *float_toint;
static AST *float_abs;
static AST *float_sqrt;
static AST *float_neg;

static AST *string_cmp;
static AST *string_concat;
static AST *make_methodptr;
static AST *gc_alloc;
static AST *gc_free;

static AST *BuildMethodPointer(AST *ast);

static int
IsBasicString(AST *typ)
{
    if (typ == NULL) return 0;
    if (typ == ast_type_string) return 1;
    if (typ->kind == AST_MODIFIER_CONST || typ->kind == AST_MODIFIER_VOLATILE) {
        return IsBasicString(typ->left);
    }
    return 0;
}

AST *
addPrintCall(AST *seq, AST *handle, AST *func, AST *expr, AST *fmt)
{
    AST *elem;
    AST *params;
    ASTReportInfo saveinfo;

    AstReportAs(fmt, &saveinfo);
    if (fmt) {
        params = NewAST(AST_EXPRLIST, fmt, NULL);
    } else {
        params = NULL;
    }
    if (expr) {
        params = NewAST(AST_EXPRLIST, expr, params);
    }
    params = NewAST(AST_EXPRLIST, handle, params);
    AST *funccall = NewAST(AST_FUNCCALL, func, params);
    elem = NewAST(AST_SEQUENCE, funccall, NULL);
    AstReportDone(&saveinfo);
    return AddToList(seq, elem);
}

AST *
addReadCall(AST *seq, AST *lineptr, AST *func, AST *var)
{
    AST *elem;
    AST *params;
    AST *funccall;
    AST *results;
    ASTReportInfo saveinfo;

    AstReportAs(lineptr, &saveinfo);
    results = NewAST(AST_EXPRLIST,
                     var,
                     NewAST(AST_EXPRLIST, lineptr, NULL));
    params = NewAST(AST_EXPRLIST, lineptr, NULL);
    funccall = NewAST(AST_FUNCCALL, func, params);
    elem = NewAST(AST_SEQUENCE, AstAssign(results, funccall), NULL);
    AstReportDone(&saveinfo);
    return AddToList(seq, elem);
}

static AST *
addPutCall(AST *seq, AST *handle, AST *func, AST *expr, int size)
{
    AST *elem;
    AST *params;
    AST *sizexpr = AstInteger(size);
    // take the address of expr (or of expr[0] for arrays)
    if (IsArrayType(ExprType(expr))) {
        expr = NewAST(AST_ARRAYREF, expr, AstInteger(0));
    }
    expr = NewAST(AST_ADDROF, expr, NULL);
    // pass it to the basic_put function
    params = NewAST(AST_EXPRLIST, expr, NULL);
    params = AddToList(params,
                       NewAST(AST_EXPRLIST, sizexpr, NULL));
    params = NewAST(AST_EXPRLIST, handle, params);
    AST *funccall = NewAST(AST_FUNCCALL, func, params);
    elem = NewAST(AST_SEQUENCE, funccall, NULL);
    return AddToList(seq, elem);
}

static AST *
addPrintHex(AST *seq, AST *handle, AST *expr, AST *fmtAst)
{
    // create a hex call
    AST *ast;
    AST *params;

    params = NewAST(AST_EXPRLIST, handle,
                    NewAST(AST_EXPRLIST, expr,
                           NewAST(AST_EXPRLIST, fmtAst,
                                  NewAST(AST_EXPRLIST, AstInteger(16), NULL))));
    ast = NewAST(AST_FUNCCALL, basic_print_unsigned, params);
    ast = NewAST(AST_SEQUENCE, ast, NULL);
    return AddToList(seq, ast);
}

//
// transform the expressions in a "print using" clause into a form
// that's easier to print
// each field discovered in the string causes the corresponding parameter
// to be prefixed with an AST_USING with an AST_INTEGER left side "fmtparam"
// "fmtparam" has the following fields:
//   bit 0-7: width 0-255; 0 means "no particular width"
//   bit 8-9: 0=left justify, 1=right justify, 2=center
//   bits 10-15: reserved
//   bit 16-21: minimum number of digits to show
//   bits 22-23: sign field: 0 = print nothing for positive sign, 1 = print space, 2 = print +
//

#define FMTPARAM_WIDTH(w) (w)
#define FMTPARAM_MINDIGITS(x) ((x)<<16)
#define FMTPARAM_LEFTJUSTIFY (0)
#define FMTPARAM_RIGHTJUSTIFY (1<<8)
#define FMTPARAM_CENTER (2<<8)
#define FMTPARAM_SIGNSPACE (1<<22)
#define FMTPARAM_SIGNPLUS  (2<<22)

static AST *
harvest(AST *exprlist, Flexbuf *fb)
{
    const char *str;

    if (flexbuf_curlen(fb) > 0) {
        flexbuf_addchar(fb, 0);
        str = flexbuf_get(fb);
        exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, AstStringLiteral(strdup(str)), NULL));
    }
    return exprlist;
}

static AST *
NextParam(AST **params)
{
    AST *p = *params;
    AST *r = NULL;
    if (p) {
        r = p;
        p = p->right;
        r->right = NULL;
        *params = p;
    } else {
        return r;
    }
    return r->left;
}

static AST *
TransformUsing(const char *usestr, AST *params)
{
    AST *exprlist = NULL;
    AST *using = NULL;
    AST *lastFormat = AstInteger(0);
    Flexbuf fb;
    int c;
    int width;
    int minwidth;
    unsigned fmtparam;
    unsigned signchar = 0;
    ASTReportInfo saveinfo;

    AstReportAs(params, &saveinfo);
    
    // scan through the use str until we find a special character
    flexbuf_init(&fb, 80);
    for(;;) {
        c = *usestr++;
        if (!c) {
            break;
        }
        switch (c) {
        case '_':
            // next character is escaped
            c = *usestr++;
            if (c) {
                flexbuf_addchar(&fb, c);
            } else {
                --usestr;
            }
            signchar = 0;
            break;
        case '&':
            exprlist = harvest(exprlist, &fb);
            
            // reset format to default
            lastFormat = AstInteger(0);
            using = NewAST(AST_USING, lastFormat, NextParam(&params)); 
            exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, using, NULL));
            signchar = 0;
            break;
        case '!':
            exprlist = harvest(exprlist, &fb);
            // reset format to width 1
            lastFormat = AstInteger(1);
            using = NewAST(AST_USING, lastFormat, NextParam(&params));
            exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, using, NULL));
            break;

        case '+':
        case '-':
            exprlist = harvest(exprlist, &fb);
            signchar = (c == '-') ? FMTPARAM_SIGNSPACE : FMTPARAM_SIGNPLUS;
            width = 1;
            minwidth = 0;
            c = *usestr;
            if (c == '#' || c == '%') {
                goto handlenumeric;
            }
            ERROR(params, "+ or - in print using must be followed by numeric format");
            AstReportDone(&saveinfo);
            return exprlist;
        case '%':
        case '#':
            signchar = 0;
            exprlist = harvest(exprlist, &fb);
            width = minwidth = 1;
        handlenumeric:
            while (*usestr && *usestr == c) {
                usestr++;
                width++;
                if (c == '%') minwidth++;
            }
            fmtparam = FMTPARAM_WIDTH(width) | FMTPARAM_MINDIGITS(minwidth) | signchar | FMTPARAM_RIGHTJUSTIFY;
            lastFormat = AstInteger(fmtparam);
            using = NewAST(AST_USING, lastFormat, NextParam(&params));
            exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, using, NULL));
            break;
        case '\\':
            exprlist = harvest(exprlist, &fb);
            width = 1;
            fmtparam = 0;
            while (*usestr && *usestr != c) {
                if (*usestr == '<') {
                    fmtparam = FMTPARAM_LEFTJUSTIFY;
                } else if (*usestr == '>') {
                    fmtparam = FMTPARAM_RIGHTJUSTIFY;
                } else if (*usestr == '=') {
                    fmtparam = FMTPARAM_CENTER;
                }
                usestr++;
                width++;
            }
            if (*usestr == c) {
                width++;
                usestr++;
            }
            fmtparam |= FMTPARAM_WIDTH(width);
            lastFormat = AstInteger(fmtparam);
            using = NewAST(AST_USING, lastFormat, NextParam(&params));
            exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, using, NULL));
            break;
        case '*':
        case '^':
        case '$':
        case '.':
            ERROR(params, "Unimplemented print using character '%c'", c);
            AstReportDone(&saveinfo);
            return exprlist;
        default:
            flexbuf_addchar(&fb, c);
            break;
        }
    }
    exprlist = harvest(exprlist, &fb);

    if (params) {
        exprlist = AddToList(exprlist, params);
    }
    flexbuf_delete(&fb);
    AstReportDone(&saveinfo);
    return exprlist;
}

AST *
genPrintf(AST *ast)
{
    AST *args = ast->right;
    AST *str;
    const char *fmtstring = NULL;
    AST *exprlist;
    AST *seq = NULL;
    AST *Handle = AstInteger(0);
    AST *Zero = AstInteger(0);
    int c;
    Flexbuf fb;
    int fmt;
    int minwidth = 0;
    
    flexbuf_init(&fb, 80);
    if (!args) {
        ERROR(ast, "Empty printf");
        return ast;
    }
    if (args->kind != AST_EXPRLIST) {
        ERROR(ast, "Expected expression list for printf");
        return ast;
    }
    str = args->left;
    args = args->right;
    if (str && str->kind == AST_STRINGPTR) {
        str = str->left;
    }
    if (str && str->kind == AST_EXPRLIST && str->left && str->left->kind == AST_STRING && str->right == NULL)
    {
        fmtstring = str->left->d.string;
    }
    if (!fmtstring) {
        ERROR(ast, "__builtin_printf only works with a constant string");
        return ast;
    }
    while (*fmtstring) {
        c = *fmtstring++;
        if (!c) {
            break;
        }
        if (c == '%') {
            c = *fmtstring++;
            if (!c) {
                ERROR(ast, "bad format in __builtin_printf");
                break;
            }
            if (c == '%') {
                flexbuf_addchar(&fb, c);
            } else {
                AST *thisarg;

                exprlist = harvest(NULL, &fb);
                if (exprlist) {
                    seq = addPrintCall(seq, Handle, basic_print_string, exprlist->left, Zero);
                }
                if (!args || args->kind != AST_EXPRLIST) {
                    ERROR(ast, "not enough parameters for printf format string");
                    break;
                }
                fmt = 0;
                minwidth = 0;
                thisarg = args->left;
                args = args->right;
                while ( (c >= '0' && c <= '9') && *fmtstring) {
                     minwidth = minwidth * 10 + (c - '0');
                    c = *fmtstring++;
               }
                if (c == 'l' && *fmtstring) {
                    c = *fmtstring++;
                }
                if (minwidth) {
                    fmt = FMTPARAM_MINDIGITS(minwidth);
                }
                switch (c) {
                case 'd':
                    seq = addPrintCall(seq, Handle, basic_print_integer, thisarg, AstInteger(fmt));
                    break;
                case 'u':
                    seq = addPrintCall(seq, Handle, basic_print_unsigned, thisarg, AstInteger(fmt));
                    break;
                case 'x':
                    seq = addPrintHex(seq, Handle, thisarg, AstInteger(fmt));
                    break;
                case 's':
                    seq = addPrintCall(seq, Handle, basic_print_string, thisarg, AstInteger(fmt));
                    break;
                case 'c':
                    seq = addPrintCall(seq, Handle, basic_print_char, thisarg, AstInteger(fmt));
                    break;
                case 'f':
                    seq = addPrintCall(seq, Handle, basic_print_float, thisarg, AstInteger(fmt));
                    break;
                default:
                    ERROR(ast, "unknown printf format character `%c'", c);
                    break;
                }
            }
        } else {
            if (c == '\n') {
                flexbuf_addchar(&fb, '\r');
            }
            flexbuf_addchar(&fb, c);
        }
    }
    exprlist = harvest(NULL, &fb);
    if (exprlist) {
        seq = addPrintCall(seq, Handle, basic_print_string, exprlist->left, Zero);
    }
    return seq;
}

static void
doBasicTransform(AST **astptr)
{
    AST *ast = *astptr;
    Function *func;
    ASTReportInfo saveinfo;
    
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        astptr = &ast->left;
        ast = *astptr;
    }
    if (!ast) return;
    switch (ast->kind) {
    case AST_ASSIGN:
        if (ast->left) {
            AstReportAs(ast, &saveinfo); // any newly created AST nodes should reflect debug info from this one
            if (ast->left->kind == AST_RANGEREF) {
                *astptr = ast = TransformRangeAssign(ast->left, ast->right, 1);
            } else if (ast->left->kind == AST_IDENTIFIER && !strcmp(ast->left->d.string, curfunc->name)) {
                ast->left->kind = AST_RESULT;
            }
            AstReportDone(&saveinfo);
        }
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    case AST_CASE:
    {
        AST *list = ast->right;
        AST *var;
        AST *seq = NULL;
        AST *elseholder = NULL;
        AST *ifholder = NULL;
        doBasicTransform(&ast->left);
        AstReportAs(ast, &saveinfo);
        if (ast->left->kind == AST_IDENTIFIER) {
            var = ast->left;
        } else {
            var = AstTempLocalVariable("_tmp_", ExprType(ast->left));
            seq = AddToList(seq, NewAST(AST_STMTLIST, AstAssign(var, ast->left), NULL));
        }
        while (list) {
            AST *caseitem;
            AST *casetest;
            AST *casebody;
            if (list->kind != AST_LISTHOLDER) {
                ERROR(list, "internal error, expected list holder");
            }
            caseitem = list->left;
            doBasicTransform(&caseitem->left);
            doBasicTransform(&caseitem->right);
            casetest = caseitem->left;
            casebody = caseitem->right;
            if (casetest->kind == AST_RANGE) {
                casetest = AstOperator(K_BOOL_AND,
                                       AstOperator(K_GE, var, casetest->left),
                                       AstOperator(K_LE, var, casetest->right));
            } else if (casetest->kind == AST_OTHER) {
                casetest = AstInteger(1);
            } else {
                casetest = AstOperator(K_EQ, var, casetest);
            }
            ifholder = NewAST(AST_IF, casetest, NULL);
            if (!elseholder) {
                seq = AddToList(seq, NewAST(AST_STMTLIST, ifholder, NULL));
            } else {
                elseholder->right = ifholder;
            }
            ifholder->left = elseholder = NewAST(AST_THENELSE, casebody, NULL);
            list = list->right;
        }
        AstReportDone(&saveinfo);
        break;
    }        
    case AST_COUNTREPEAT:
        // convert repeat count into a for loop
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_RANGEREF:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        *astptr = ast = TransformRangeUse(ast);
        break;
    case AST_ALLOCA:
        doBasicTransform(&ast->right);
        curfunc->uses_alloca = 1;
        break;
    case AST_ADDROF:
    case AST_ABSADDROF:
        {
            doBasicTransform(&ast->left);
            doBasicTransform(&ast->right);
            if (IsLocalVariable(ast->left)) {
                curfunc->local_address_taken = 1;
            }
            // taking the address of a function may restrict how
            // we can call it (stack vs. register calling)
            Symbol *sym;
            Function *f = NULL;
            sym = FindCalledFuncSymbol(ast, NULL, 0);
            if (sym && sym->type == SYM_FUNCTION) {
                f = (Function *)sym->val;
            }
            if (f) {
                f->used_as_ptr = 1;
                f->callSites++;
            }
        }
        break;
    case AST_FUNCCALL:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        if (1)
        {
            // the parser treats a(x) as a function call (always), but in
            // fact it may be an array reference; change it to one if applicable
            AST *left = ast->left;
            AST *index = ast->right;
            AST *typ;
            
            typ = ExprType(left);
            if (typ && ( (IsPointerType(typ) && !IsFunctionType(typ))  || IsArrayType(typ))) {
                ast->kind = AST_ARRAYREF;
                if (!index || index->kind != AST_EXPRLIST) {
                    ERROR(ast, "Internal error: expected expression list in array subscript");
                    return;
                }
                // reduce a single item expression list, if necessary
                if (index->right != NULL) {
                    ERROR(index, "Multi-dimensional arrays are not supported\n");
                    return;
                }
                index = index->left;
                // later we will have to convert the array reference to
                // BASIC style (subtract one); that's done in the type checking
                ast->right = index;
            }
        }
        break;
    case AST_USING:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        if (ast->left && ast->left->kind == AST_STRING) {
            *ast = *TransformUsing(ast->left->d.string, ast->right);
        } else {
            WARNING(ast, "Unexpected using method");
        }
        break;
    case AST_METHODREF:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        if (IsPointerType(ast->left)) {
            WARNING(ast, "Needs a pointer dereference");
        }
        break;
    case AST_TRYENV:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        // keep local variables on stack, so they will be preserved
        // if an exception throws us back here without cleanup
        curfunc->local_address_taken = 1;
        break;
    case AST_READ:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        {
            // convert to a series of calls to _basic_getstr, etc.
            AST *exprlist = ast->left;
            AST *handle = ast->right;
            AST *var, *type;
            AST *seq = NULL;
            AST *lineptr = NULL;
            AST *read_lineptr;
            
            if (handle) {
                lineptr = AstTempLocalVariable("_tmp_", ast_type_string);
                read_lineptr = AstAssign(lineptr,
                                         NewAST(AST_FUNCCALL,
                                                basic_read_line,
                                                NewAST(AST_EXPRLIST,
                                                       handle,
                                                       NULL)));
                seq = NewAST(AST_SEQUENCE, read_lineptr, NULL);
            } else {
                lineptr = AstIdentifier("__basic_data_ptr");
                seq = NULL;
            }
            while (exprlist) {
                var = exprlist->left;
                exprlist = exprlist->right;
                type = ExprType(var);
                if (IsFloatType(type)) {
                    seq = addReadCall(seq, lineptr, basic_get_float, var);
                } else if (IsStringType(type)) {
                    seq = addReadCall(seq, lineptr, basic_get_string, var);
                } else if (IsIntType(type)) {
                    seq = addReadCall(seq, lineptr, basic_get_integer, var);
                } else {
                    const char *name = GetVarNameForError(var);
                    ERROR(ast, "Type of %s is %s", name, type ? "invalid for reading" : "unknown");
                }
            }
            *astptr = ast = seq;
        }
        break;
    case AST_PRINT:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        {
            // convert PRINT to a series of calls to basic_print_xxx
            AST *seq = NULL;
            AST *type;
            AST *exprlist = ast->left;
            AST *expr;
            AST *handle = ast->right;
            AST *defaultFmt = AstInteger(0);
            AST *fmtAst;
            if (!handle) {
                handle = AstInteger(0);
            }
            while (exprlist) {
                fmtAst = defaultFmt;
                if (exprlist->kind != AST_EXPRLIST) {
                    ERROR(exprlist, "internal error in print list");
                }
                expr = exprlist->left;
                exprlist = exprlist->right;
                if (expr->kind == AST_USING) {
                    fmtAst = expr->left;
                    expr = expr->right;
                }
                if (!expr) {
                    continue;
                }
                // PUT gets translated to a PRINT with an AST_HERE node
                // this requests that we write out the literal bytes of
                // an expression
                if (expr->kind == AST_HERE) {
                    // request to put literal data
                    int size;
                    expr = expr->left;
                    size = TypeSize(ExprType(expr));
                    seq = addPutCall(seq, handle, basic_put, expr, size);
                    continue;
                }
                if (expr->kind == AST_CHAR) {
                    expr = expr->left;
                    if (IsConstExpr(expr) && EvalConstExpr(expr) == 10) {
                        seq = addPrintCall(seq, handle, basic_print_nl, NULL, NULL);
                    } else {
                        seq = addPrintCall(seq, handle, basic_print_char, expr, NULL);
                    }
                    continue;
                }
                type = ExprType(expr);
                if (!type) {
                    ERROR(ast, "Unknown type in print");
                    continue;
                }
                if (IsFloatType(type)) {
                    seq = addPrintCall(seq, handle, basic_print_float, expr, fmtAst);
                } else if (IsStringType(type)) {
                    seq = addPrintCall(seq, handle, basic_print_string, expr, fmtAst);
                } else if (IsGenericType(type)) {
                    // create a hex call
                    seq = addPrintHex(seq, handle, expr, fmtAst);
                } else if (IsUnsignedType(type)) {
                    seq = addPrintCall(seq, handle, basic_print_unsigned, expr, fmtAst);
                } else if (IsIntType(type)) {
                    seq = addPrintCall(seq, handle, basic_print_integer, expr, fmtAst);
                } else {
                    ERROR(ast, "Unable to print expression of this type");
                }
            }
            *astptr = ast = seq;
        }
        break;
    case AST_LABEL:
        if (!ast->left || ast->left->kind != AST_IDENTIFIER) {
            ERROR(ast, "Label is not an identifier");
        } else {
            const char *name = ast->left->d.string;
            Symbol *sym = FindSymbol(&curfunc->localsyms, name);
            if (sym) {
                WARNING(ast, "Redefining %s as a label", name);
            }
            AddSymbol(&curfunc->localsyms, name, SYM_LOCALLABEL, 0);
        }
        break;
    case AST_COGINIT:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        if (IsSpinCoginit(ast, &func) && func) {
            func->cog_task = 1;
            func->force_static = 1;
        }
        break;
    case AST_IDENTIFIER:
    {
        AST *typ;
        if (IsLocalVariable(ast)) {
            typ = ExprType(ast);
            if (typ && TypeSize(typ) > LARGE_SIZE_THRESHOLD) {
                curfunc->large_local = 1;
            }
        }
        break;
    }
    default:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    }
}

bool VerifyIntegerType(AST *astForError, AST *typ, const char *opname)
{
    if (!typ)
        return true;
    if (IsIntType(typ))
        return true;
    // for now, accept generic types too as if they were integer
    // perhaps this should give a warning?
    if (IsGenericType(typ))
        return true;
    ERROR(astForError, "Expected integer type for parameter of %s", opname);
    return false;
}

// create a call to function func with parameters ast->left, ast->right
// there is an optional 3rd argument too
static AST *
MakeOperatorCall(AST *func, AST *left, AST *right, AST *extraArg)
{
    AST *call;
    AST *params = NULL;

    if (!func) {
        ERROR(left, "Internal error, NULL parameter");
        return AstInteger(0);
    }
    if (left) {
        params = AddToList(params, NewAST(AST_EXPRLIST, left, NULL));
    }
    if (right) {
        params = AddToList(params, NewAST(AST_EXPRLIST, right, NULL));
    }
    if (extraArg) {
        params = AddToList(params, NewAST(AST_EXPRLIST, extraArg, NULL));
    }
    call = NewAST(AST_FUNCCALL, func, params);
    return call;
}

// do a promotion when we already know the size of the original type
static AST *dopromote(AST *expr, int srcbytes, int destbytes, int operator)
{
    int shiftbits = srcbytes * 8;
    AST *promote;

    if (shiftbits == 32 && destbytes < 8) {
        return expr; // nothing to do
    }
    promote = AstOperator(operator, expr, AstInteger(shiftbits));
    if (destbytes == 8) {
        // at this point "promote" will contain a 4 byte value
        // now we need to convert it to an 8 byte value
        AST *highword;
        if (operator == K_ZEROEXTEND) {
            highword = AstInteger(0);
        } else {
            highword = AstOperator(K_SAR, promote, AstInteger(31));
        }
        promote = NewAST(AST_EXPRLIST,
                         promote,
                         NewAST(AST_EXPRLIST,
                                highword,
                                NULL));
    }
    return promote;
}
// do a narrowing operation to convert from A bytes to B bytes
// works by going A -> 32 bits -> B
static AST *donarrow(AST *expr, int A, int B, int isSigned)
{
    int shiftbits = (A - B) * 8;
    AST *promote;
    AST *narrow;

    if (A == 8 && B <= 4) {
        // have to narrow
        if (expr->kind != AST_EXPRLIST) {
            return expr;
        }
        expr = expr->left;
        A = 4;
    }
    shiftbits = (A - B) * 8;
    if (shiftbits == 0) {
        return expr; // nothing to do
    }
    promote = dopromote(expr, A, LONG_SIZE, isSigned ? K_ZEROEXTEND : K_SIGNEXTEND);
#if 0    
    if (shiftbits > 0) {
        int operator = isSigned ? K_SAR : K_SHR;
        narrow = AstOperator(K_SHL, promote, AstInteger(shiftbits));
        narrow = AstOperator(operator, narrow, AstInteger(shiftbits));
    } else {
        narrow = promote;
    }
#else
    narrow = promote;
#endif
    return narrow;
}

// force a promotion from a small integer type to a full 32 bits
static AST *forcepromote(AST *type, AST *expr)
{
    int tsize;
    int op;
    if (!type) {
        return expr;
    }
    if (!IsIntType(type) && !IsGenericType(type)) {
        ERROR(expr, "internal error in forcecpromote");
    }
    tsize = TypeSize(type);
    op = IsUnsignedType(type) ? K_ZEROEXTEND : K_SIGNEXTEND;
    return dopromote(expr, tsize, LONG_SIZE, op);
}

//
// insert promotion code under AST for either the left or right type
// if "force" is nonzero then we will always promote small integers,
// otherwise we promote only if their sizes do not match
// return the final type
//
AST *MatchIntegerTypes(AST *ast, AST *lefttype, AST *righttype, int force) {
    int lsize = TypeSize(lefttype);
    int rsize = TypeSize(righttype);
    AST *rettype = lefttype;
    int leftunsigned = IsUnsignedType(lefttype);
    int rightunsigned = IsUnsignedType(righttype);
    int finalsize = 4;
    
    force = force || (lsize != rsize);
    
    if (lsize < finalsize && force) {
        if (IsConstExpr(ast->right)) {
            return lefttype;
        }
        if (leftunsigned) {
            ast->left = dopromote(ast->left, lsize, finalsize, K_ZEROEXTEND);
            lefttype = ast_type_unsigned_long;
        } else {
            ast->left = dopromote(ast->left, lsize, finalsize, K_SIGNEXTEND);
            lefttype = ast_type_long;
        }
        rettype = righttype;
    }
    if (rsize < finalsize && force) {
        if (IsConstExpr(ast->left)) {
            return righttype;
        }
        if (rightunsigned) {
            ast->right = dopromote(ast->right, rsize, finalsize, K_ZEROEXTEND);
            righttype = ast_type_unsigned_long;
        } else {
            ast->right = dopromote(ast->right, rsize, finalsize, K_SIGNEXTEND);
            righttype = ast_type_long;
        }
        rettype = lefttype;
    }
    if (leftunsigned && rightunsigned) {
        return rettype;
    } else {
        return ast_type_long;
    }
}

static AST *
domakefloat(AST *typ, AST *ast)
{
    AST *ret;
    if (!ast) return ast;
    if (IsGenericType(typ)) return ast;
    if (gl_fixedreal) {
        ret = AstOperator(K_SHL, ast, AstInteger(G_FIXPOINT));
        return ret;
    }
    ast = forcepromote(typ, ast);
    if (IsUnsignedType(typ)) {
        ret = MakeOperatorCall(float_fromuns, ast, NULL, NULL);
    } else {
        ret = MakeOperatorCall(float_fromint, ast, NULL, NULL);
    }
    return ret;
}

static AST *
dofloatToInt(AST *ast)
{
    AST *ret;
    if (gl_fixedreal) {
        // FIXME: should we round here??
        ret = AstOperator(K_SAR, ast, AstInteger(G_FIXPOINT));
        return ret;
    }
    ast = MakeOperatorCall(float_toint, ast, NULL, NULL);
    return ast;
}

bool MakeBothIntegers(AST *ast, AST *ltyp, AST *rtyp, const char *opname)
{
    if (IsFloatType(ltyp)) {
        ast->left = dofloatToInt(ast->left);
        ltyp = ast_type_long;
    }
    if (IsFloatType(rtyp)) {
        ast->right = dofloatToInt(ast->right);
        rtyp = ast_type_long;
    }
    return VerifyIntegerType(ast, ltyp, opname) && VerifyIntegerType(ast, rtyp, opname);
}

static AST *
HandleTwoNumerics(int op, AST *ast, AST *lefttype, AST *righttype)
{
    int isfloat = 0;
    int isalreadyfixed = 0;
    AST *scale = NULL;
    
    if (IsFloatType(lefttype)) {
        isfloat = 1;
        if (!IsFloatType(righttype)) {
            if (gl_fixedreal && (op == '*' || op == '/')) {
                // no need for fixed point mul, just do regular mul
                isalreadyfixed = 1;
                if (op == '/') {
                    // fixed / int requires no scale
                    scale = AstInteger(0);
                }
            } else {
                ast->right = domakefloat(righttype, ast->right);
            }
        }
    } else if (IsFloatType(righttype)) {
        isfloat = 1;
        if (gl_fixedreal && (op == '*' || op == '/')) {
            // no need for fixed point mul, regular mul works
            isalreadyfixed = 1;
            if (op == '/') {
                // int / fixed requires additional scaling
                scale = AstInteger(2*G_FIXPOINT);
            }
        } else {
            ast->left = domakefloat(lefttype, ast->left);
        }
    }
    if (isfloat) {
        switch (op) {
        case '+':
            if (!gl_fixedreal) {
                *ast = *MakeOperatorCall(float_add, ast->left, ast->right, NULL);
            }
            break;
        case '-':
            if (!gl_fixedreal) {
                *ast = *MakeOperatorCall(float_sub, ast->left, ast->right, NULL);
            }
            break;
        case '*':
            if (!isalreadyfixed) {
                *ast = *MakeOperatorCall(float_mul, ast->left, ast->right, NULL);
            }
            break;
        case '/':
            if (gl_fixedreal) {
                if (!isalreadyfixed) {
                    scale = AstInteger(G_FIXPOINT);
                }
            }
            *ast = *MakeOperatorCall(float_div, ast->left, ast->right, scale);
            break;
        default:
            ERROR(ast, "internal error unhandled operator");
            break;
        }
        return ast_type_float;
    }
    if (!MakeBothIntegers(ast, lefttype, righttype, "operator"))
        return NULL;
    lefttype = MatchIntegerTypes(ast, lefttype, righttype, 0);
    if (IsUnsignedType(lefttype)) {
        if (op == K_MODULUS) {
            ast->d.ival = K_UNS_MOD;
        } else if (op == '/') {
            ast->d.ival = K_UNS_DIV;
        }
    }
    return lefttype;
}

static bool
IsSymbol(AST *expr)
{
    if (!expr) return false;
    if (expr->kind == AST_IDENTIFIER || expr->kind == AST_SYMBOL)
        return true;
    return false;
}

bool IsUnsignedConst(AST *ast)
{
    if (!IsConstExpr(ast)) {
        return false;
    }
    if (EvalConstExpr(ast) < 0) {
        return false;
    }
    return true;
}

void CompileComparison(int op, AST *ast, AST *lefttype, AST *righttype)
{
    int isfloat = 0;
    int leftUnsigned = 0;
    int rightUnsigned = 0;
    
    if (IsFloatType(lefttype)) {
        if (!IsFloatType(righttype)) {
            ast->right = domakefloat(righttype, ast->right);
        }
        isfloat = 1;
    } else if (IsFloatType(righttype)) {
        ast->left = domakefloat(lefttype, ast->left);
        isfloat = 1;
    }
    if (isfloat) {
        if (gl_fixedreal) {
            // we're good
        } else {
            ast->left = MakeOperatorCall(float_cmp, ast->left, ast->right, NULL);
            ast->right = AstInteger(0);
        }
        return;
    }
    // allow for string comparison
    if (IsBasicString(lefttype) || IsBasicString(righttype)) {
        if (!CompatibleTypes(lefttype, righttype)) {
            ERROR(ast, "illegal comparison with string");
            return;
        }
        ast->left = MakeOperatorCall(string_cmp, ast->left, ast->right, NULL);
        ast->right = AstInteger(0);
        return;
    }

    if (IsPointerType(lefttype) || IsPointerType(righttype)) {
        /* FIXME: should handle floats and type promotion here!!! */
        leftUnsigned = rightUnsigned = 0;
    } else {
        if (!MakeBothIntegers(ast, lefttype, righttype, "comparison")) {
            return;
        }
        // need to widen the types
        ast->left = forcepromote(lefttype, ast->left);
        ast->right = forcepromote(righttype, ast->right);
        leftUnsigned = IsUnsignedType(lefttype);
        rightUnsigned = IsUnsignedType(righttype);
    }
    
     //
    // handle unsigned/signed comparisons here
    //
    
    if (leftUnsigned || rightUnsigned) {
        if ( (leftUnsigned && (rightUnsigned || IsUnsignedConst(ast->right)))
             || (rightUnsigned && IsUnsignedConst(ast->left)) )
        {
            switch (op) {
            case '<':
                ast->d.ival = K_LTU;
                break;
            case '>':
                ast->d.ival = K_GTU;
                break;
            case K_LE:
                ast->d.ival = K_LEU;
                break;
            case K_GE:
                ast->d.ival = K_GEU;
                break;
            default:
                break;
            }
        } else {
            // cannot do unsigned comparison
            // signed comparison will work if the sizes are < 32 bits
            // if both are 32 bits, we need to do something else
            int lsize = TypeSize(lefttype);
            int rsize = TypeSize(righttype);
            if (lsize == 4 && rsize == 4) {
                WARNING(ast, "signed/unsigned comparison may not work properly");
            }
        }
    }

}

static AST *ScalePointer(AST *type, AST *val)
{
    int size;
    if (!IsPointerType(type)) {
        ERROR(val, "internal error: expected pointer type");
        return val;
    }
    size = TypeSize(BaseType(type));
    val = AstOperator('*', val, AstInteger(size));
    return val;
}

// return the address of an array
AST *ArrayAddress(AST *expr)
{
    return NewAST(AST_ABSADDROF,
                  NewAST(AST_ARRAYREF, expr, AstInteger(0)),
                  NULL);
}

// return the address of a function
AST *FunctionAddress(AST *expr)
{
    if (IsSymbol(expr)) {
        expr = NewAST(AST_ABSADDROF, expr, NULL);
        expr = BuildMethodPointer(expr);
    }
    return expr;
}

AST *FunctionPointerType(AST *typ)
{
    return NewAST(AST_PTRTYPE, typ, NULL);
}

//
// cast an array to a pointer type;
//
AST *ArrayToPointerType(AST *type)
{
    AST *modifier;
    if (type->kind == AST_ARRAYTYPE) {
        type = NewAST(AST_PTRTYPE, type->left, NULL);
    } else {
        modifier = NewAST(type->kind, NULL, NULL);
        modifier->left = ArrayToPointerType(type->left);
        type = modifier;
    }
    return type;
}

AST *CoerceOperatorTypes(AST *ast, AST *lefttype, AST *righttype)
{
    AST *rettype = lefttype;
    int op;

    // hmmm, should we automatically convert arrays to pointers here?
    // for current languages yes, eventually maybe not if we want
    // to support array arithmetic
    if (IsArrayType(lefttype)) {
        ast->left = ArrayAddress(ast->left);
        lefttype = ArrayToPointerType(lefttype);
    }
    if (IsArrayType(righttype)) {
        ast->right = ArrayAddress(ast->right);
        righttype = ArrayToPointerType(righttype);
    }
    if (IsFunctionType(lefttype) && !IsPointerType(lefttype)) {
        ast->left = FunctionAddress(ast->left);
        lefttype = FunctionPointerType(lefttype);
    }
    if (IsFunctionType(righttype) && !IsPointerType(righttype)) {
        ast->right = FunctionAddress(ast->right);
        righttype = FunctionPointerType(righttype);
    }
    //assert(ast->kind == AST_OPERATOR)
    if (!ast->left) {
        rettype = righttype;
    }
    op = ast->d.ival;
    switch(op) {
    case K_SAR:
    case K_SHL:
    case '&':
    case '|':
    case '^':
        if (lefttype && IsFloatType(lefttype)) {
            ast->left = dofloatToInt(ast->left);
            lefttype = ast_type_long;
        }
        if (righttype && IsFloatType(righttype)) {
            ast->right = dofloatToInt(ast->right);
            righttype = ast_type_long;
        }
        if (!MakeBothIntegers(ast, lefttype, righttype, "bit operation")) {
            return NULL;
        }
        if (ast->d.ival == K_SAR && lefttype && IsUnsignedType(lefttype)) {
            ast->d.ival = K_SHR;
        }
        return lefttype;
    case '+':
        if (IsStringType(lefttype) && IsStringType(righttype)) {
            *ast = *MakeOperatorCall(string_concat, ast->left, ast->right, NULL);
            return lefttype;
        }
        if (IsPointerType(lefttype) && IsIntType(righttype)) {
            ast->right = ScalePointer(lefttype, ast->right);
            return lefttype;
        } else if (IsPointerType(righttype) && IsIntType(lefttype)) {
            ast->left = ScalePointer(righttype, ast->left);
            return righttype;
        } else {
            return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
        }
    case '-':
        if (IsPointerType(lefttype) && IsPointerType(righttype)) {
            // we actually want to compute (a - b) / sizeof(*a)
            if (!CompatibleTypes(lefttype, righttype)) {
                ERROR(lefttype, "- applied to incompatible pointer types");
            } else {
                AST *diff;
                diff = AstOperator('-', ast->left, ast->right);
                diff = AstOperator(K_UNS_DIV, diff, AstInteger(TypeSize(BaseType(righttype))));
                *ast = *diff;
            }
            return ast_type_unsigned_long;
        }
        if (IsPointerType(lefttype) && IsIntType(righttype)) {
            ast->right = ScalePointer(lefttype, ast->right);
            return lefttype;
        } else if (IsPointerType(righttype) && IsIntType(lefttype)) {
            ast->left = ScalePointer(righttype, ast->left);
            return righttype;
        } else {
            return HandleTwoNumerics(ast->d.ival, ast, lefttype, righttype);
        }
    case '*':
    case '/':
    case K_MODULUS:
        return HandleTwoNumerics(op, ast, lefttype, righttype);
    case K_SIGNEXTEND:
        VerifyIntegerType(ast, righttype, "sign extension");
        return ast_type_long;
    case K_ZEROEXTEND:
        VerifyIntegerType(ast, righttype, "zero extension");
        return ast_type_unsigned_long;
    case '<':
    case K_LE:
    case K_EQ:
    case K_NE:
    case K_GE:
    case '>':
        CompileComparison(ast->d.ival, ast, lefttype, righttype);
        return ast_type_long;
    case K_ABS:
    case K_NEGATE:
    case K_SQRT:
        if (IsFloatType(rettype)) {
            if (!gl_fixedreal) {
                if (op == K_ABS) {
                    *ast = *MakeOperatorCall(float_abs, ast->right, NULL, NULL);
                } else if (op == K_SQRT) {
                    *ast = *MakeOperatorCall(float_sqrt, ast->right, NULL, NULL);
                } else {
                    *ast = *MakeOperatorCall(float_neg, ast->right, NULL, NULL);
                }
                return rettype;
            }
            if (gl_fixedreal && op == K_SQRT) {
                *ast = *AstOperator(K_SHL, AstOperator(op, ast->left, ast->right), AstInteger(G_FIXPOINT/2));
            }
            return rettype;
        } else {
            const char *name;
            if (op == K_ABS) {
                name = "abs";
            } else if (op == K_SQRT) {
                name = "sqrt";
            } else {
                name = "negate";
            }
            if (!VerifyIntegerType(ast, rettype, name))
                return NULL;
            ast->right = forcepromote(rettype, ast->right);
            if (IsUnsignedType(rettype) && op == K_ABS) {
                *ast = *ast->right; // ignore the ABS
                return ast_type_unsigned_long;
            }
            return ast_type_long;
        }
    case K_ASC:
        if (!CompatibleTypes(righttype, ast_type_string)) {
            ERROR(ast, "expected string argument to ASC");
        } else {
            AST *newast;
            if (ast->right && ast->right->kind == AST_STRING) {
                // literal: fix it up here
                newast = AstInteger(ast->right->d.string[0]);
                *ast = *newast;
            } else {
                newast = NewAST(AST_MEMREF, ast_type_byte, ast->right);
                *ast = *newast;
            }
        }
        return ast_type_long;
    case K_BOOL_NOT:
    case K_BOOL_AND:
    case K_BOOL_OR:
        if (lefttype && !IsBoolCompatibleType(lefttype)) {
            ERROR(ast, "Expression not compatible with boolean operation");
        } else if (righttype && !IsBoolCompatibleType(righttype)) {
            ERROR(ast, "Expression not compatible with boolean operation");
        }
        return ast_type_long;
    case K_INCREMENT:
    case K_DECREMENT:
        if (lefttype && IsPointerType(lefttype)) {
            return lefttype;
        }
        if (righttype && IsPointerType(righttype)) {
            return righttype;
        }
        /* fall through */
    default:
        if (!MakeBothIntegers(ast, lefttype, righttype, "operator")) {
            return NULL;
        }
        return MatchIntegerTypes(ast, lefttype, righttype, 1);
    }
}

//
// modifies *astptr, originally of type srctype,
// to have type desttype by introducing any
// necessary casts
// returns the new type (should normally be desttype)
//
AST *CoerceAssignTypes(AST *line, int kind, AST **astptr, AST *desttype, AST *srctype)
{
    ASTReportInfo saveinfo;
    AST *expr = *astptr;
    const char *msg;

    if (kind == AST_RETURN) {
        msg = "return";
    } else if (kind == AST_FUNCCALL) {
        msg = "parameter passing";
    } else if (kind == AST_ARRAYREF) {
        msg = "array indexing";
    } else {
        msg = "assignment";
    }

    if (expr && expr->kind == AST_INTEGER && expr->d.ival == 0) {
        if (curfunc && curfunc->language == LANG_C) {
            if (IsPointerType(desttype)) {
                return desttype;
            }
        }
    }
    
    if (!desttype || !srctype) {
        return desttype;
    }
    AstReportAs(expr, &saveinfo);
    if (IsFloatType(desttype)) {
        if (IsIntType(srctype)) {
            *astptr = domakefloat(srctype, expr);
            srctype = ast_type_float;
        }
    }
    // allow floats to be cast as ints
    if (IsIntType(desttype) && IsFloatType(srctype)) {
        expr = dofloatToInt(expr);
        *astptr = expr;
        AstReportDone(&saveinfo);
        return desttype;
    }

    // automatically cast arrays to pointers if necessary
    if (IsArrayType(srctype) && IsPointerType(desttype) && IsSymbol(expr)) {
        srctype = ArrayToPointerType(srctype);
        expr = ArrayAddress(expr);
        *astptr = expr;
    }
    if (IsPointerType(desttype) && srctype) {
        if (srctype->kind == AST_FUNCTYPE) {
            srctype = FunctionPointerType(srctype);
            expr = FunctionAddress(expr);
            *astptr = expr;
        } else if (IsArrayType(srctype)) {
            // automatically cast arrays to pointers
            expr = ArrayAddress(expr);
            *astptr = expr;
            srctype = ArrayToPointerType(srctype);
        }
    }
    if (!CompatibleTypes(desttype, srctype)) {
        if (IsPointerType(desttype) && IsPointerType(srctype)) {
            WARNING(line, "incompatible pointer types in %s", msg);
        } else {
            ERROR(line, "incompatible types in %s", msg);
            return desttype;
        }
    }
    if (IsConstType(desttype) && kind == AST_ASSIGN) {
        WARNING(line, "assignment to const object");
    }
    if (IsPointerType(srctype) && IsConstType(BaseType(srctype)) && !IsConstType(BaseType(desttype))) {
        if (desttype != ast_type_const_generic) {
            WARNING(line, "%s discards const attribute from pointer", msg);
        }
    }
    if (IsIntType(desttype) || IsGenericType(desttype)) {
        if (IsIntType(srctype)) {
            int lsize = TypeSize(desttype);
            int rsize = TypeSize(srctype);
            if (lsize > rsize) {
                if (IsUnsignedType(srctype)) {
                    *astptr = dopromote(expr, rsize, lsize, K_ZEROEXTEND);
                } else {
                    *astptr = dopromote(expr, rsize, lsize, K_SIGNEXTEND);
                }
            } else if (rsize == 8 && lsize < rsize) {
                // narrowing cast
                *astptr = donarrow(expr, rsize, lsize, IsUnsignedType(srctype));
            }
        }
    }
    AstReportDone(&saveinfo);
    return desttype;
}

/* change AST so that it casts src to desttype */
static AST *
doCast(AST *desttype, AST *srctype, AST *src)
{
    AST *expr = src;
    const char *name;
    ASTReportInfo saveinfo;
    
    if (IsVoidType(desttype)) {
        // (void)x ignores x
        return src;
    }
    if (!srctype || IsGenericType(srctype)) {
        return src;
    }
    AstReportAs(src, &saveinfo);
    if (src && src->kind == AST_IDENTIFIER) {
        name = src->d.string;
    } else {
        name = "expression";
    }
    if (IsArrayType(srctype)) {
        src = ArrayAddress(src);
        srctype = ast_type_ptr_void;
    }
    if (IsPointerType(desttype)) {
        if (IsFloatType(srctype)) {
            src = dofloatToInt(src);
            srctype = ast_type_long;
        }
        if (IsArrayType(srctype)) {
            return ArrayAddress(src);
        }
        if (IsPointerType(srctype)) {
            return src;
        }
        if (IsIntType(srctype)) {
            /* FIXME: should probably check size here */
            return src;
        }
        if (srctype->kind == AST_FUNCTYPE) {
            return NewAST(AST_ADDROF, src, NULL);
        }
        ERROR(src, "unable to convert %s to a pointer type", name);
        AstReportDone(&saveinfo);
        return NULL;
    }
    if (IsFloatType(desttype)) {
        if (IsFloatType(srctype)) {
            AstReportDone(&saveinfo);
            return src;
        }
        if (IsPointerType(srctype)) {
            srctype = ast_type_long;
        }
        if (IsIntType(srctype)) {
            AST *r = domakefloat(srctype, src);
            AstReportDone(&saveinfo);
            return r;
        }
        ERROR(src, "unable to convert %s to a float type", name);
        AstReportDone(&saveinfo);
        return NULL;
    }
    if (IsIntType(desttype)) {
        if (IsFloatType(srctype)) {
            src = dofloatToInt(src);
            srctype = ast_type_long;
        }
        if (IsPointerType(srctype)) {
            srctype = ast_type_long;
        }
        if (IsIntType(srctype)) {
            int lsize = TypeSize(desttype);
            int rsize = TypeSize(srctype);
            if (lsize > rsize) {
                int finalsize = (lsize < LONG_SIZE) ? LONG_SIZE : lsize;
                if (IsUnsignedType(srctype)) {
                    src = dopromote(src, rsize, finalsize, K_ZEROEXTEND);
                } else {
                    src = dopromote(expr, rsize, finalsize, K_SIGNEXTEND);
                }
            } else if (lsize < rsize) {
                src = donarrow(src, rsize, lsize, IsUnsignedType(srctype));
            }
            AstReportDone(&saveinfo);
            return src;
        }
    }
    AstReportDone(&saveinfo);
    ERROR(src, "bad cast of %s", name);
    return NULL;
}

static AST *
BuildMethodPointer(AST *ast)
{
    Symbol *sym;
    AST *objast;
    AST *funcaddr;
    AST *result;
    Function *func;
    
    sym = FindCalledFuncSymbol(ast, &objast, 0);
    if (!sym || sym->type != SYM_FUNCTION) {
        ERROR(ast, "Internal error, unable to find function address");
        return ast;
    }
    func = (Function *)sym->val;
    if (func->is_static) {
        objast = AstInteger(0);
    } else if (objast == NULL) {
        objast = NewAST(AST_SELF, NULL, NULL);
    } else {
        objast = NewAST(AST_ADDROF, objast, NULL);
    }
    func->used_as_ptr = 1;
    // save off the current @ node
    funcaddr = NewAST(AST_ADDROF, ast->left, ast->right);
    // create a call
    result = MakeOperatorCall(make_methodptr, objast, funcaddr, NULL);
    return result;
}

//
// function for doing type checking and various kinds of
// type related manipulations. for example:
//
// signed/unsigned shift: x >> y  => signed shift if x is signed,
//                                   unsigned otherwise
// returns the most recent type signature
//
AST *CheckTypes(AST *ast)
{
    AST *ltype, *rtype;
    if (!ast) return NULL;

    if (ast->kind == AST_CAST) {
        AST *cast;
        ltype = ast->left;
        rtype = CheckTypes(ast->right);
        cast = doCast(ltype, rtype, ast->right);
        if (cast) {
            ast->right = cast;
        }
        return ltype;
    }        
    ltype = CheckTypes(ast->left);
    if (ast->kind != AST_METHODREF) {
        rtype = CheckTypes(ast->right);
    }
    switch (ast->kind) {
    case AST_GOSUB:
        /* FIXME: should check here for top level function */
    case AST_GOTO:
        {
            AST *id = ast->left;
            if (!id || id->kind != AST_IDENTIFIER) {
                ERROR(ast, "Expected identifier in goto/gosub");
            } else {
                Symbol *sym = FindSymbol(&curfunc->localsyms, id->d.string);
                if (!sym || sym->type != SYM_LOCALLABEL) {
                    ERROR(id, "%s is not a local label", id->d.string);
                }
            }
        }
        return NULL;
    case AST_COGINIT:
        ltype = ast_type_long;
        break;
    case AST_OPERATOR:
        ltype = CoerceOperatorTypes(ast, ltype, rtype);
        break;
    case AST_ASSIGN:
        if (rtype) {
            ltype = CoerceAssignTypes(ast, AST_ASSIGN, &ast->right, ltype, rtype);
        }
        break;
    case AST_RETURN:
        if (ast->left) {
            rtype = ltype; // type of actual expression
            ltype = GetFunctionReturnType(curfunc);
            ltype = CoerceAssignTypes(ast, AST_RETURN, &ast->left, ltype, rtype);
        }
        break;
    case AST_FUNCCALL:
        {
            AST *actualParamList = ast->right;
            AST *calledParamList;
            AST *expectType, *passedType;
            AST *functype;

            functype = RemoveTypeModifiers(ExprType(ast->left));
            if (functype && functype->kind == AST_PTRTYPE) {
                functype = RemoveTypeModifiers(functype->left);
            }
            if (IsFunctionType(functype)) {
                calledParamList = functype->right;
                while (actualParamList) {
                    AST *paramId = calledParamList ? calledParamList->left : NULL;
                    AST *actualParam = actualParamList->left;
                    
                    expectType = NULL;
                    passedType = NULL;
                    if (!passedType) {
                        passedType = ExprType(actualParam);
                    }
                    if (paramId) {
                        // if the parameter has a type declaration, use it
                        if (paramId->kind == AST_DECLARE_VAR) {
                            expectType = ExprType(paramId);
                        }
                    }
                    if (!expectType) {
                        // we use const generic to avoid lots of warning
                        // messages about passing strings to printf
                        expectType = ast_type_const_generic;
                    }
                    CoerceAssignTypes(ast, AST_FUNCCALL, &actualParamList->left, expectType, passedType);
                    if (calledParamList) {
                        calledParamList = calledParamList->right;
                    }
                    actualParamList = actualParamList->right;
                }
                ltype = functype->left;
            } else {
                return NULL;
            }
        }
        break;
    case AST_RESULT:
        return GetFunctionReturnType(curfunc);
    case AST_FLOAT:
    case AST_TRUNC:
    case AST_ROUND:
        return ast_type_float;
    case AST_ISBETWEEN:
    case AST_INTEGER:
    case AST_HWREG:
    case AST_CONSTREF:
        return ast_type_long;
    case AST_SIZEOF:
        return ast_type_unsigned_long;
    case AST_CATCHRESULT:
    case AST_BITVALUE:
        return ast_type_generic;
    case AST_SETJMP:
        return ast_type_long;
    case AST_STRING:
    case AST_STRINGPTR:
        if (curfunc->language == LANG_BASIC) {
            return ast_type_string;
        }
        return ast_type_ptr_byte;
    case AST_ADDROF:
    case AST_ABSADDROF:
        if (IsFunctionType(ltype)) {
            *ast = *BuildMethodPointer(ast);
            return ltype;
        }
        return NewAST(AST_PTRTYPE, ltype, NULL);
    case AST_ARRAYREF:
        {
            AST *lefttype = ltype;
            AST *righttype;
            AST *basetype;

            righttype = ExprType(ast->right);
            if (IsFloatType(righttype)) {
                righttype = CoerceAssignTypes(ast, AST_ARRAYREF, &ast->right, ast_type_long, righttype);
            }
            if (!lefttype) {
                lefttype = ExprType(ast->left);
            }
            if (!lefttype) {
                return NULL;
            }
            basetype = BaseType(lefttype);
            if (IsPointerType(lefttype)) {
                // force this to have a memory dereference
                // and in BASIC, also force 1 for the base
                AST *deref;
                if (curfunc->language == LANG_BASIC) {
                    extern Symbol *GetCurArrayBase();
                    Symbol *sym = GetCurArrayBase();
                    if (sym && sym->type == SYM_CONSTANT) {
                        ast->right = AstOperator('-', ast->right, (AST *)sym->val);
                    }
                }
                deref = NewAST(AST_MEMREF, basetype, ast->left);
                ast->left = deref;
            } else if (ast->left->kind == AST_MEMREF) {
                // nothing to do
            } else if (IsArrayType(lefttype)) {
                // convert the array index to subtract base
                AST *base = GetArrayBase(lefttype);
                if (base) {
                    ast->right = AstOperator('-', ast->right, base);
                }
            } else {
                ERROR(ast, "Array dereferences a non-array object");
                return NULL;
            }
            return basetype;
        }
        break;
    case AST_NEW:
        // turn this into an alloc
        {
            AST *sizeExpr;
            AST *basetype;
            int baseSize;
            ltype = ast->left;
            basetype = BaseType(ltype);
            baseSize = TypeSize(basetype);
            if (IsConstExpr(ast->right)) {
                baseSize *= EvalConstExpr(ast->right);
                sizeExpr = AstInteger(baseSize);
            } else {
                sizeExpr = AstOperator('*', ast->right, AstInteger(baseSize));
            }
            *ast = *MakeOperatorCall(gc_alloc, sizeExpr, NULL, NULL);
        }
        break;
    case AST_DELETE:
        *ast = *MakeOperatorCall(gc_free, ast->left, NULL, NULL);
        ltype = ast_type_void;
        break;
    case AST_CONDRESULT:
    {
        //AST *cond = ast->left; // not needed here
        AST *outputs = ast->right;
        if (!outputs) return NULL;
        ltype = ExprType(outputs->left);
        rtype = ExprType(outputs->right);
        if (!CompatibleTypes(ltype, rtype)) {
            WARNING(ast, "different types in arms of ?");
        }
        return ltype;
    }
    case AST_ALLOCA:
    {
        return ast->left ? ast->left : ast_type_ptr_void;
    }
    case AST_METHODREF:
        if (!IsClassType(ltype)) {
            ERROR(ast, "Method reference on non-class %s", GetIdentifierName(ast->left));
            return ltype;
        }
        return ExprType(ast);
    case AST_IDENTIFIER:
    case AST_SYMBOL:
        // add super class lookups if necessary
        {
            Module *P;
            AST *supers = NULL;
            static AST *superref = NULL;
            Symbol *sym = LookupAstSymbol(ast, NULL);
            if (!sym) {
                return NULL;
            }
            ltype = ExprType(ast);
            if (sym->type == SYM_FUNCTION) {
                Function *f = (Function *)sym->val;
                if (f->module == current || f->module == globalModule) {
                    return ltype;
                }
            }
            if (sym->type == SYM_VARIABLE || sym->type == SYM_FUNCTION) {
                const char *name = sym->name;
                P = current;
                while (P) {
                    sym = FindSymbol(&P->objsyms, name);
                    if (sym) {
                        break;
                    }
                    if (!superref) {
                        superref = AstIdentifier("__super");
                    }
                    if (supers) {
                        supers = NewAST(AST_METHODREF, supers, superref);
                    } else {
                        supers = superref;
                    }
                    supers = NewAST(AST_ARRAYREF,
                                    NewAST(AST_MEMREF,
                                           ClassType(P->superclass),
                                           supers),
                                    AstInteger(0));
                    P = P->superclass;
                }
                if (sym && supers) {
                    *ast = *NewAST(AST_METHODREF, supers, DupAST(ast));
                }
            }
            return ltype;
        }
    case AST_EXPRLIST:
    case AST_SEQUENCE:
    case AST_CONSTANT:
    case AST_VA_ARG:
        return ExprType(ast);
    default:
        break;
    }
    return ltype;
}

////////////////////////////////////////////////////////////////
static AST *
getBasicPrimitive(const char *name)
{
    AST *ast;

    ast = AstIdentifier(name);
    return ast;
}

void
InitGlobalFuncs(void)
{
    if (!basic_print_integer) {
        if (gl_fixedreal) {
            basic_print_float = getBasicPrimitive("_basic_print_fixed");
            float_mul = getBasicPrimitive("_fixed_mul");
            float_div = getBasicPrimitive("_fixed_div");
        } else {
            basic_print_float = getBasicPrimitive("_basic_print_float");
            basic_get_float = getBasicPrimitive("_basic_get_float");
            float_cmp = getBasicPrimitive("_float_cmp");
            float_add = getBasicPrimitive("_float_add");
            float_sub = getBasicPrimitive("_float_sub");
            float_mul = getBasicPrimitive("_float_mul");
            float_div = getBasicPrimitive("_float_div");
            float_fromuns = getBasicPrimitive("_float_fromuns");
            float_fromint = getBasicPrimitive("_float_fromint");
            float_toint = getBasicPrimitive("_float_trunc");
            float_abs = getBasicPrimitive("_float_abs");
            float_sqrt = getBasicPrimitive("_float_sqrt");
            float_neg = getBasicPrimitive("_float_negate");
        }
        basic_get_integer = getBasicPrimitive("_basic_get_integer");
        basic_get_string = getBasicPrimitive("_basic_get_string");
        basic_read_line = getBasicPrimitive("_basic_read_line");
        
        basic_print_integer = getBasicPrimitive("_basic_print_integer");
        basic_print_unsigned = getBasicPrimitive("_basic_print_unsigned");
        basic_print_string = getBasicPrimitive("_basic_print_string");
        basic_print_char = getBasicPrimitive("_basic_print_char");
        basic_print_nl = getBasicPrimitive("_basic_print_nl");
        basic_put = getBasicPrimitive("_basic_put");

        string_cmp = getBasicPrimitive("_string_cmp");
        string_concat = getBasicPrimitive("_string_concat");
        make_methodptr = getBasicPrimitive("_make_methodptr");
        gc_alloc = getBasicPrimitive("_gc_alloc");
        gc_free = getBasicPrimitive("_gc_free");
    }
}

void
BasicTransform(Function *func)
{
    InitGlobalFuncs();

    doBasicTransform(&func->body);
    CheckTypes(func->body);
}
