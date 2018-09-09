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
static AST *basic_print_unsigned;
static AST *basic_print_char;
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
            if (typ && typ->kind == AST_ARRAYTYPE) {
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
                // and now we have to convert the array reference to
                // BASIC style (subtract one)
                index = AstOperator('-', index, AstInteger(1));
                ast->right = index;
            }
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
            while (exprlist) {
                expr = exprlist->left;
                exprlist = exprlist->right;
                if (expr->kind == AST_CHAR) {
                    expr = expr->left;
                    if (IsConstExpr(expr) && EvalConstExpr(expr) == 10) {
                        seq = addPrintCall(seq, basic_print_nl, NULL);
                    } else {
                        seq = addPrintCall(seq, basic_print_char, expr);
                    }
                    continue;
                }
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
                } else if (type->kind == AST_UNSIGNEDTYPE) {
                    seq = addPrintCall(seq, basic_print_unsigned, expr);
                } else {
                    ERROR(ast, "Unable to print expression of this type");
                }
            }
            *astptr = ast = seq;
        }
        break;
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
    basic_print_unsigned = getBasicPrimitive("_basic_print_unsigned");
    basic_print_string = getBasicPrimitive("_basic_print_string");
    basic_print_char = getBasicPrimitive("_basic_print_char");
    basic_print_nl = getBasicPrimitive("_basic_print_nl");
    
    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;

        doBasicTransform(&func->body);
    }
    curfunc = savefunc;
    current = savecur;
}
