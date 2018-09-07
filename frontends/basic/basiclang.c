/*
 * Spin to C/C++ converter
 * Copyright 2011-2018 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for BASIC specific features
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

static AST *basic_print_float;
static AST *basic_print_string;
static AST *basic_print_integer;
static AST *basic_print_nl;

static AST *
addPrintCall(AST *seq, AST *func, AST *expr)
{
    AST *elem;
    AST *funccall = NewAST(AST_FUNCCALL, func,
                           expr ? NewAST(AST_EXPRLIST, expr, NULL) : NULL);
    elem = NewAST(AST_SEQUENCE, funccall, NULL);
    return AddToList(seq, elem);
}

static void
doBasicTransform(AST **astptr)
{
    AST *ast = *astptr;
    
    while (ast && ast->kind == AST_COMMENTEDNODE) {
        astptr = &ast->left;
        ast = *astptr;
    }
    if (!ast) return;
    AstReportAs(ast); // any newly created AST nodes should reflect debug info from this one
    switch (ast->kind) {
    case AST_ASSIGN:
        if (ast->left && ast->left->kind == AST_RANGEREF) {
            *astptr = ast = TransformRangeAssign(ast->left, ast->right, 1);
        }
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    case AST_COUNTREPEAT:
        // convert repeat count into a for loop
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        *astptr = TransformCountRepeat(*astptr);
        break;
    case AST_RANGEREF:
        *astptr = ast = TransformRangeUse(ast);
        break;
    case AST_PRINT:
    {
        // convert PRINT to a series of calls to basic_print_xxx
        AST *seq = NULL;
        AST *type;
        AST *exprlist = ast->left;
        AST *expr;
        while (exprlist) {
            expr = exprlist->left;
            exprlist = exprlist->right;
            type = ExprType(expr);
            if (!type) {
                ERROR(ast, "Unknown type in print");
                continue;
            }
            if (type == ast_type_float) {
                seq = addPrintCall(seq, basic_print_float, expr);
            } else if (type == ast_type_string) {
                seq = addPrintCall(seq, basic_print_string, expr);
            } else if (type->kind == AST_INTTYPE) {
                seq = addPrintCall(seq, basic_print_integer, expr);
            } else {
                ERROR(ast, "Unable to print expression of this type");
            }
        }
        seq = addPrintCall(seq, basic_print_nl, NULL);
        *astptr = ast = seq;
        break;
    }
    default:
        doBasicTransform(&ast->left);
        doBasicTransform(&ast->right);
        break;
    }
}

static AST *
getBasicPrimitive(const char *name)
{
    AST *ast;

    ast = AstIdentifier(name);
    return ast;
}

void
BasicTransform(Module *Q)
{
    Module *savecur = current;
    Function *func;
    Function *savefunc = curfunc;

    basic_print_float = getBasicPrimitive("_basic_print_float");
    basic_print_integer = getBasicPrimitive("_basic_print_integer");
    basic_print_string = getBasicPrimitive("_basic_print_string");
    basic_print_nl = getBasicPrimitive("_basic_print_nl");
    
    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;

        doBasicTransform(&func->body);
    }
    curfunc = savefunc;
    current = savecur;
}
