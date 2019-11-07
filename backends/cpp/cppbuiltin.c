/*
 * Spin to C/C++ converter
 * Copyright 2011-2019 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling builtin expressions
 */

#include "spinc.h"
#include "backends/cpp/outcpp.h"
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define gl_ccode (gl_output == OUTPUT_C)

/* code to print a builtin function call to a file */
void
defaultBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != b->numparameters) {
        ERROR(params, "wrong number of parameters to %s", b->name);
    }
    if (gl_p2 && b->p2_cname) {
        flexbuf_printf(f, "%s(", b->p2_cname);
    } else if (gl_gas_dat && b->gas_cname) {
        flexbuf_printf(f, "%s(", b->gas_cname);
    } else {
        flexbuf_printf(f, "%s(", b->p1_cname);
    }
    PrintExprList(f, params, PRINTEXPR_TOPLEVEL, NULL);
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
    if (gl_p2 && b->p2_cname) {
        flexbuf_printf(f, "%s(", b->p2_cname);
    } else if (gl_gas_dat && b->gas_cname) {
        flexbuf_printf(f, "%s(", b->gas_cname);
    } else {
        flexbuf_printf(f, "%s(", b->p1_cname);
    }
    PrintTypedExpr(f, ast_type_long, a1, PRINTEXPR_TOPLEVEL);
    flexbuf_printf(f, ", ");
    PrintTypedExpr(f, ast_type_long, a2, PRINTEXPR_TOPLEVEL);
    flexbuf_printf(f, ")");
}

/* code to print a builtin variable reference call to a file */
void
defaultVariable(Flexbuf *f, Builtin *b, AST *params)
{
    if (gl_p2 && b->p2_cname) {
        flexbuf_printf(f, "%s", b->p2_cname);
    } else if (gl_gas_dat && b->gas_cname) {
        flexbuf_printf(f, "%s", b->gas_cname);
    } else {
        flexbuf_printf(f, "%s", b->p1_cname);
    }
}

/* code to do memory copies; also does 1 byte fills */
void
memBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    int ismemcpy = !strcmp(b->p1_cname, "memcpy") || !strcmp(b->p1_cname, "memmove");
    AST *dst, *src, *count;

    dst = params->left;
    params = params->right;
    src = params->left;
    params = params->right;
    count = params->left;

    flexbuf_printf(f, "%s( (void *)", b->p1_cname);
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
    flexbuf_printf(f, "%s(", b->p1_cname);
    PrintTypedExpr(f, ast_type_string, params->left, PRINTEXPR_TOPLEVEL);
    params = params->right;
    flexbuf_printf(f, ")");
}

/* code to do strcomp(a, b) */
void
strcompBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    flexbuf_printf(f, "-(0 == strcmp(");
    PrintTypedExpr(f, ast_type_string, params->left, PRINTEXPR_TOPLEVEL);
    params = params->right;
    flexbuf_printf(f, ", ");
    PrintTypedExpr(f, ast_type_string, params->left, PRINTEXPR_TOPLEVEL);
    params = params->right;
    flexbuf_printf(f, "))");
}

/* code to do reboot() -> clkset(0x80, 0) */
void
rebootBuiltin(Flexbuf *f, Builtin *b, AST *params)
{
    if (AstListLen(params) != b->numparameters) {
        ERROR(params, "wrong number of parameters to %s", b->name);
    }
    if (gl_gas_dat) {
        flexbuf_printf(f, "__builtin_propeller_clkset(0x80, 0)");
    } else {
        flexbuf_printf(f, "clkset(0x80, 0)");
    }
}

