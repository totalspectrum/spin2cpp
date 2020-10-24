/*
 * Spin to C/C++ converter
 * Copyright 2011-2020 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for BASIC specific features
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

AST *
addToPrintSeq(AST *seq, AST *printCall)
{
    if (seq) {
        seq = AstOperator('+', seq, printCall);
    } else {
        seq = printCall;
    }
    return seq;
}

AST *
addFloatPrintCall(AST *seq, AST *handle, AST *func, AST *expr, AST *fmt, int ch)
{
    AST *elem;
    AST *params;
    ASTReportInfo saveinfo;

    AstReportAs(fmt, &saveinfo);
    if (ch) {
        params = NewAST(AST_EXPRLIST, AstInteger(ch), NULL);
    } else {
        params = NULL;
    }
    if (fmt) {
        params = NewAST(AST_EXPRLIST, fmt, params);
    }
    if (expr) {
        params = NewAST(AST_EXPRLIST, expr, params);
    }

    params = NewAST(AST_EXPRLIST, handle, params);
    AST *funccall = NewAST(AST_FUNCCALL, func, params);
    elem = addToPrintSeq(seq, funccall);
    AstReportDone(&saveinfo);
    return elem;
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
    elem = addToPrintSeq(seq, funccall);
    AstReportDone(&saveinfo);
    return elem;
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
    elem = addToPrintSeq(seq, funccall);
    return elem;
}

// create a hex print integer call
static AST *
addPrintHex(AST *seq, AST *handle, AST *func, AST *expr, AST *fmtAst)
{
    AST *ast;
    AST *params;

    params = NewAST(AST_EXPRLIST, handle,
                    NewAST(AST_EXPRLIST, expr,
                           NewAST(AST_EXPRLIST, fmtAst,
                                  NewAST(AST_EXPRLIST, AstInteger(16), NULL))));
    ast = NewAST(AST_FUNCCALL, func, params);
    ast = addToPrintSeq(seq, ast);
    return ast;
}

// create a decimal print integer call
static AST *
addPrintDec(AST *seq, AST *handle, AST *func, AST *expr, AST *fmtAst)
{
    AST *ast;
    AST *params;

    params = NewAST(AST_EXPRLIST, handle,
                    NewAST(AST_EXPRLIST, expr,
                           NewAST(AST_EXPRLIST, fmtAst,
                                  NewAST(AST_EXPRLIST, AstInteger(10), NULL))));
    ast = NewAST(AST_FUNCCALL, func, params);
    ast = addToPrintSeq(seq, ast);
    return ast;
}

//
// transform the expressions in a "print using" clause into a form
// that's easier to print
// each field discovered in the string causes the corresponding parameter
// to be prefixed with an AST_USING with an AST_INTEGER left side "fmtparam"
// "fmtparam" has the following fields:
//   bit 0-7: minimum width 0-255; 0 means "no particular width"
//   bit 8-15: maximum width 0-255: 0 means "no particular maximum"
//   bit 16-21: precision (digits after decimal point)
//   bit 22: 1=left justify, 2=right justify, 3=center
//   bits 10-15: reserved
//   bit 16-21: minimum number of digits to show
//   bits 22-23: sign field: 0 = print nothing for positive sign, 1 = print space, 2 = print +
//

#define MAXWIDTH_BIT (0)
#define MINWIDTH_BIT (8)
#define PREC_BIT     (16)
#define JUSTIFY_BIT  (22)
#define PADDING_BIT  (24)
#define SIGNCHAR_BIT (26)
#define ALT_BIT      (28)
#define UPCASE_BIT   (29)

#define FMTPARAM_MAXWIDTH(w) (w)
#define FMTPARAM_MINWIDTH(w) ((w)<<MINWIDTH_BIT)
#define FMTPARAM_ALLWIDTH(w) (FMTPARAM_MAXWIDTH(w)|FMTPARAM_MINWIDTH(w))
#define FMTPARAM_MINDIGITS(x) (((x)+1)<<PREC_BIT)
#define FMTPARAM_LEFTJUSTIFY (0)
#define FMTPARAM_RIGHTJUSTIFY (2<<JUSTIFY_BIT)
#define FMTPARAM_CENTER (3<<JUSTIFY_BIT)
#define FMTPARAM_SIGNPLUS  (1<<SIGNCHAR_BIT)
#define FMTPARAM_SIGNSPACE (2<<SIGNCHAR_BIT)

static AST *
AddExprToList(AST *list, AST *x)
{
    return AddToList(list, NewAST(AST_EXPRLIST, x, NULL));
}

static AST *
harvest(AST *exprlist, Flexbuf *fb)
{
    const char *str;

    if (flexbuf_curlen(fb) > 0) {
        flexbuf_addchar(fb, 0);
        str = flexbuf_get(fb);
        exprlist = AddToList(exprlist, NewAST(AST_EXPRLIST, AstStringPtr(strdup(str)), NULL));
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
            fmtparam = FMTPARAM_ALLWIDTH(width) | FMTPARAM_MINDIGITS(minwidth) | signchar | FMTPARAM_RIGHTJUSTIFY;
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
            fmtparam |= FMTPARAM_ALLWIDTH(width);
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
    int minwidth;
    int zeropad;
    int justify;
    ASTReportInfo saveinfo;
    
    if (gl_output == OUTPUT_CPP || gl_output == OUTPUT_C) {
        return NULL; // convert directly to C
    }
    flexbuf_init(&fb, 80);
    if (!args) {
        //ERROR(ast, "Empty printf");
        return NULL;
    }
    if (args->kind != AST_EXPRLIST) {
        //ERROR(ast, "Expected expression list for printf");
        return NULL;
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
        //ERROR(ast, "__builtin_printf only works with a constant string");
        return NULL;
    }
    AstReportAs(ast, &saveinfo);
    while (*fmtstring) {
        c = *fmtstring++;
        if (!c) {
            break;
        }
        if (c == '%') {
            c = *fmtstring++;
            if (!c) {
                //ERROR(ast, "bad format in __builtin_printf");
                AstReportDone(&saveinfo);
                return NULL;
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
                    WARNING(ast, "not enough parameters for printf format string");
                    break;
                }
                fmt = 0;
                minwidth = 0;
                zeropad = 0;
                justify = FMTPARAM_RIGHTJUSTIFY;
                thisarg = args->left;
                args = args->right;
                if (c == '-') {
                    justify = FMTPARAM_LEFTJUSTIFY;
                    c = *fmtstring++;
                    if (!c) {
                        return NULL;
                    }
                }
                if (c == '0') {
                    zeropad = 1;
                }
                while ( (c >= '0' && c <= '9') && *fmtstring) {
                     minwidth = minwidth * 10 + (c - '0');
                    c = *fmtstring++;
               }
                if (c == 'l' && *fmtstring) {
                    c = *fmtstring++;
                }
                if (minwidth) {
                    if (zeropad) {
                        fmt = FMTPARAM_MINDIGITS(minwidth);
                    } else {
                        fmt = FMTPARAM_MINWIDTH(minwidth) | justify;
                    }
                }
                switch (c) {
                case 'd':
                    seq = addPrintDec(seq, Handle, basic_print_integer, thisarg, AstInteger(fmt));
                    break;
                case 'u':
                    seq = addPrintDec(seq, Handle, basic_print_unsigned, thisarg, AstInteger(fmt));
                    break;
                case 'x':
                    seq = addPrintHex(seq, Handle, basic_print_unsigned, thisarg, AstInteger(fmt));
                    break;
                case 's':
                    seq = addPrintCall(seq, Handle, basic_print_string, thisarg, AstInteger(fmt));
                    break;
                case 'c':
                    seq = addPrintCall(seq, Handle, basic_print_char, thisarg, AstInteger(fmt));
                    break;
                case 'f':
                case 'g':
                case 'e':
                    seq = addFloatPrintCall(seq, Handle, basic_print_float, thisarg, AstInteger(fmt), c);
                    break;
                default:
                    //ERROR(ast, "unknown printf format character `%c'", c);
                    return NULL;
                }
            }
        } else if (c == '\n') {
            exprlist = harvest(NULL, &fb);
            if (exprlist) {
                seq = addPrintCall(seq, Handle, basic_print_string, exprlist->left, Zero);
            }
            seq = addPrintCall(seq, Handle, basic_print_nl, NULL, NULL);
        } else {
            flexbuf_addchar(&fb, c);
        }
    }
    exprlist = harvest(NULL, &fb);
    if (exprlist) {
        seq = addPrintCall(seq, Handle, basic_print_string, exprlist->left, Zero);
    }
    AstReportDone(&saveinfo);
    return seq;
}

static AST *
ArrayDeref(AST *base, AST *index)
{
    if (index->kind != AST_EXPRLIST) {
        return NewAST(AST_ARRAYREF, base, index);
    }
    // A[0] -> ARRAYREF(A, 0)
    // A[0, 1] -> ARRAYREF(ARRAYREF(A, 0), 1)
    while (index) {
        base = NewAST(AST_ARRAYREF, base, index->left);
        index = index->right;
    }
    return base;
}

// the parser treats a(x) as a function call (always), but in fact it may be an array reference;
// change it to one if applicable
static void
adjustFuncCall(AST *ast)
{
    AST *left = ast->left;
    AST *leftparent = NULL;
    AST *index = ast->right;
    AST *typ;
    AST *func = NULL;
    AST *templident;
    AST *methodref;
    AST *methodcall;
    Module *P = NULL;

    
    if (left->kind == AST_METHODREF) {
        methodcall = left;
        templident = left->right;
	methodref = left->left;
    } else {
        templident = left;
	methodref = NULL;
        methodcall = NULL;
        leftparent = ast;
    }
    /* check for template instantiation */
    if (templident->kind == AST_IDENTIFIER || templident->kind == AST_LOCAL_IDENTIFIER) {
        Symbol *sym;
	const char *name = GetIdentifierName(templident);
	if (methodref) {
            AST *modtyp = ExprType(methodref);
            if (IsRefType(modtyp)) {
                modtyp = modtyp->left;
            }
	    if (!IsClassType(modtyp)) {
	        ERROR(methodref, "Unable to determine class type");
	        return;
	    }
	    P = GetClassPtr(modtyp);
	}
	sym = P ? FindSymbol(&P->objsyms, name) : LookupSymbol(name);
        if (sym && sym->kind == SYM_TEMPLATE) {
	    func = InstantiateTemplateFunction(P ? P : current, (AST *)sym->val, ast);
            if (func) {
                if (methodcall) {
                    methodcall->right = func;
                } else {
                    ast->left = func;
                }
	    }
	}
    }
    if (!func) {
        typ = ExprType(left);
        if (typ && ( (IsPointerType(typ) && !IsFunctionType(typ))  || IsArrayType(typ)) && left) {
            if (index) {
                AST *arrayref = ArrayDeref(left, index);
                *ast = *arrayref;
            } else {
                if (IsArrayType(typ)) {
                    // allow "foo()" to mean "@foo(0)"
                    // actually we just replace "foo()" with "foo"
                    // and let array->pointer promotion do the rest
                    *ast = *left;
                }
            }
	} else {
            if (left->kind == AST_IDENTIFIER && leftparent && typ) {
                if (!strcmp(left->d.string, "_basic_open") && IsStringType(typ)) {
                    /* change to _basic_open_string */
                    AST *newleft = AstIdentifier("_basic_open_string");
                    leftparent->left = newleft;
                    /* append O_RDWR | O_CREAT */
                    leftparent->right = AddToList(leftparent->right, AstInteger(6));
                }
            }
        }
    }
}

static AST *
ConvertPrintToPrintf(AST *ast)
{
    AST *exprlist = ast->left;
    AST *handle = ast->right;
    AST *expr, *type;
    AST *printit = NewAST(AST_PRINT, NULL, NULL);
    AST *seq = NULL;    
//    AST *fmtAst = NULL;
    Flexbuf fbstr;
    char strbuf[8];

    flexbuf_init(&fbstr, 80);
    
    if (handle) {
        ERROR(ast, "Unable to convert print # to C");
        return AstInteger(0);
    }
    while (exprlist) {
        if (exprlist->kind != AST_EXPRLIST) {
            ERROR(exprlist, "internal error in print list");
            return NULL;
        }
        expr = exprlist->left;
        exprlist = exprlist->right;
        if (expr->kind == AST_USING) {
//            fmtAst = expr->left;
            expr = expr->right;
        }
        if (!expr) {
            continue;
        }
        if (expr->kind == AST_HERE) {
            // PUT expression
            ERROR(ast, "PUT not supported yet");
            return NULL;
        }
        if (expr->kind == AST_CHAR) {
            expr = expr->left;
            if (IsConstExpr(expr)) {
                int c = EvalConstExpr(expr);
                switch (c) {
                case '\n':
                    strcpy(strbuf, "\\n");
                    break;
                case '\r':
                    strcpy(strbuf, "\\r");
                    break;
                case '\t':
                    strcpy(strbuf, "\\t");
                    break;
                default:
                    strbuf[0] = c;
                    strbuf[1] = 0;
                    break;
                }
                flexbuf_addstr(&fbstr, strbuf);
            } else {
                flexbuf_addstr(&fbstr, "%c");
                seq = AddExprToList(seq, expr);
            }
            continue;
        }
        type = ExprType(expr);
        if (!type) {
            ERROR(ast, "Unknown type in print");
            continue;
        }
        if (IsFloatType(type)) {
            flexbuf_addstr(&fbstr, "%f");
            seq = AddExprToList(seq, expr);
        } else if (IsStringType(type)) {
            flexbuf_addstr(&fbstr, "%s");
            seq = AddExprToList(seq, expr);
        } else if (IsGenericType(type)) {
            flexbuf_addstr(&fbstr, "%x");
            seq = AddExprToList(seq, expr);
        } else if (IsUnsignedType(type)) {
            flexbuf_addstr(&fbstr, "%u");
            seq = AddExprToList(seq, expr);
        } else if (IsIntType(type)) {
            flexbuf_addstr(&fbstr, "%d");
            seq = AddExprToList(seq, expr);
        } else {
            ERROR(ast, "Unable to print expression of this type");
        }       
    }
    printit->left = AstIdentifier("printf");
    exprlist = harvest(NULL, &fbstr);
    exprlist = AddToList(exprlist, seq);
    printit->right = exprlist;
    return printit;
}

AST *
ParsePrintStatement(AST *ast)
{
    // convert PRINT to a series of calls to basic_print_xxx
    AST *seq = NULL;
    AST *type;
    AST *exprlist = ast->left;
    AST *expr;
    AST *handle = ast->right;
    AST *defaultFmt = AstInteger(0);
    AST *fmtAst;

    if (gl_output == OUTPUT_CPP || gl_output == OUTPUT_C) {
        return ConvertPrintToPrintf(ast);
    }
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
            seq = addFloatPrintCall(seq, handle, basic_print_float, expr, fmtAst, '#');
        } else if (IsStringType(type)) {
            seq = addPrintCall(seq, handle, basic_print_string, expr, fmtAst);
        } else if (IsGenericType(type)) {
            // create a hex call
            seq = addPrintHex(seq, handle, basic_print_unsigned, expr, fmtAst);
        } else if (IsUnsignedType(type)) {
            seq = addPrintDec(seq, handle, basic_print_unsigned, expr, fmtAst);
        } else if (IsIntType(type)) {
            seq = addPrintDec(seq, handle, basic_print_integer, expr, fmtAst);
        } else {
            ERROR(ast, "Unable to print expression of this type");
        }
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
            } else if (IsIdentifier(ast->left) && !strcmp(GetUserIdentifierName(ast->left), curfunc->name)) {
                ast->left = NewAST(AST_RESULT, NULL, NULL);
            }
            AstReportDone(&saveinfo);
        }
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    case AST_CASE:
    case AST_CASETABLE:
    {
        AST *list = ast->right;
        const char *case_name = (ast->kind == AST_CASETABLE) ? "ON GOTO" : NULL;
        doBasicTransform(&ast->left);
        AstReportAs(ast, &saveinfo);
        while (list) {
            AST *caseitem;
            if (list->kind != AST_STMTLIST) {
                ERROR(list, "internal error, expected list holder");
            }
            caseitem = list->left;
            doBasicTransform(&caseitem->left);
            doBasicTransform(&caseitem->right);
            list = list->right;
        }
        *ast = *CreateSwitch(ast->left, ast->right, case_name);
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
            if (sym && sym->kind == SYM_FUNCTION) {
                f = (Function *)sym->val;
            }
            if (f) {
                f->used_as_ptr = 1;
                if (f->callSites == 0) {
                    MarkUsed(f, "func pointer");
                }
            }
        }
        break;
    case AST_FUNCCALL:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        {
            // the parser treats a(x) as a function call (always), but in
            // fact it may be an array reference; change it to one if applicable
 	    adjustFuncCall(ast);
        }
        break;
    case AST_METHODREF:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        {
            AST *typ = ExprType(ast->left);
            if (IsPointerType(typ) && !IsRefType(typ)) {
                // WARNING(ast, "Needs a pointer dereference");
                ast->left = NewAST(AST_ARRAYREF, ast->left, AstInteger(0));
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
        *astptr = ast = ParsePrintStatement(ast);
        break;
    case AST_LABEL:
        if (!ast->left || !IsIdentifier(ast->left)) {
            ERROR(ast, "Label is not an identifier");
        } else {
            const char *name = GetIdentifierName(ast->left);
            Symbol *sym = FindSymbol(&curfunc->localsyms, name);
            if (sym) {
                WARNING(ast, "Redefining %s as a label", name);
            }
            AddSymbol(&curfunc->localsyms, name, SYM_LOCALLABEL, 0, NULL);
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
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
    {
        AST *typ;
        if (IsLocalVariable(ast)) {
            typ = ExprType(ast);
            if (typ) {
                if (TypeGoesOnStack(typ)) {
                    curfunc->stack_local = 1;
                }
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

void
BasicTransform(Function *func)
{
    InitGlobalFuncs();

    SimplifyAssignments(&func->body);
    doBasicTransform(&func->body);
    CheckTypes(func->body);
}
