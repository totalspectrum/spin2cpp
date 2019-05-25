/*
 * Spin to C/C++ translator
 * Copyright 2011-2019 Total Spectrum Software Inc.
 * 
 * +--------------------------------------------------------------------
 * ¦  TERMS OF USE: MIT License
 * +--------------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * +--------------------------------------------------------------------
 */
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "common.h"
#include "util/flexbuf.h"

void
AddSymbolForLabel(AST *ast)
{
    if (!ast->left || ast->left->kind != AST_IDENTIFIER) {
        ERROR(ast, "Label is not an identifier");
    } else {
        const char *name = ast->left->d.string;
        Symbol *sym = FindSymbol(&curfunc->localsyms, name);
        if (sym) {
            WARNING(ast, "Redefining %s as a label", name);
        }
        AddSymbol(&curfunc->localsyms, name, SYM_LOCALLABEL, NULL, NULL);
    }
}

/*
 * returns a list of if x goto y; statments where x is a case condition and
 * y is the case label
 */
static AST *
CreateGotos(AST *tmpvar, AST *switchstmt, AST *stmt, AST **defaultlabel, AST *endswitch)
{
    AST *labelid;
    AST *label;
    ASTReportInfo saveinfo;
again:    
    if (!stmt) {
        return switchstmt;
    }
    switch (stmt->kind) {
    case AST_CASE:
        ERROR(stmt, "Internal error, case not transformed");
        return switchstmt;
    case AST_CASEITEM:
    {
        AST *ifgoto;
        AST *ifcond;
        
        AstReportAs(stmt, &saveinfo);
        labelid = AstTempIdentifier("_case_");
        label = NewAST(AST_LABEL, labelid, NULL);
        AddSymbolForLabel(label);
        ifcond = AstOperator(K_EQ, tmpvar, stmt->left);
        *stmt = *NewAST(AST_STMTLIST, label,
                        NewAST(AST_STMTLIST, stmt->right, NULL));
        ifgoto = NewAST(AST_GOTO, labelid, NULL);
        ifgoto = NewAST(AST_STMTLIST, ifgoto, NULL);
        ifgoto = NewAST(AST_THENELSE, ifgoto, NULL);
        ifgoto = NewAST(AST_IF, ifcond, ifgoto);
        switchstmt = AddToList(switchstmt, NewAST(AST_STMTLIST, ifgoto, NULL));
        AstReportDone(&saveinfo);
        goto again;
    }
    case AST_OTHER:
        AstReportAs(stmt, &saveinfo);
        if (defaultlabel) {
            if (*defaultlabel) {
                ERROR(stmt, "Switch already has a default label");
            }
            labelid = AstTempIdentifier("_case_");
            *defaultlabel = labelid;
            label = NewAST(AST_LABEL, labelid, NULL);
            AddSymbolForLabel(label);
            *stmt = *NewAST(AST_STMTLIST, label,
                            NewAST(AST_STMTLIST, stmt->left, NULL));
        } else {
            ERROR(stmt, "Internal error, got default when not expecting it");
            return switchstmt;
        }
        AstReportDone(&saveinfo);
        goto again;
    case AST_QUIT:
        AstReportAs(stmt, &saveinfo);
        if (endswitch) {
            *stmt = *NewAST(AST_GOTO, endswitch, NULL);
        }
        AstReportDone(&saveinfo);
        return switchstmt;
    // for loops, "break" cannot have special meaning any more
    case AST_WHILE:
    case AST_DOWHILE:
    case AST_FOR:
    case AST_FORATLEASTONCE:
        endswitch = NULL;
        // fall through
    case AST_STMTLIST:
    case AST_IF:
    case AST_SEQUENCE:
    case AST_COMMENTEDNODE:
    case AST_THENELSE:
        switchstmt = CreateGotos(tmpvar, switchstmt, stmt->left, defaultlabel, endswitch);
        return CreateGotos(tmpvar, switchstmt, stmt->right, defaultlabel, endswitch);
    default:
        return switchstmt;
    }
    return switchstmt;
}

// struct that holds case values and labels
typedef struct {
    int32_t val;
    AST *label;
} CaseHolder;

static int caseCmp(const void *av, const void *bv)
{
    const CaseHolder *a = (const CaseHolder *)av;
    const CaseHolder *b = (const CaseHolder *)bv;
    return a->val - b->val;
}

static int AddCases(Flexbuf *fb, AST *ident, AST *expr, AST *label, const char *force_reason)
{
    CaseHolder temp;
    if (expr->kind != AST_OPERATOR) {
        if (force_reason) {
            ERROR(expr, "%s: case expression is not valid", force_reason);
        }
        return 0;
    }
    if (expr->d.ival == K_EQ) {
        if (!AstMatch(ident, expr->left)) {
            if (force_reason) {
                ERROR(expr, "%s: case expression is not valid", force_reason);
            }
            return 0;
        }
        if (!IsConstExpr(expr->right)) {
            if (force_reason) {
                ERROR(expr, "%s: case expression is not constant", force_reason);
            }
            return 0;
        }
        temp.val = EvalConstExpr(expr->right);
        temp.label = label;
        flexbuf_addmem(fb, (char *)&temp, sizeof(temp));
    } else if (expr->d.ival == K_BOOL_OR) {
        return AddCases(fb, ident, expr->left, label, force_reason) && AddCases(fb, ident, expr->right, label, force_reason);
    }
    return 1;
}

//
// check a list of if x goto y statements to see if they can be turned into a jump
// table
//
AST *CreateJumpTable(AST *switchstmt, AST *defaultlabel, const char *force_reason)
{
    AST *top, *ast;
    AST *assign, *ident;
    AST *expr;
    AST *label;
    AST *exprtype;
    Flexbuf fb;
    CaseHolder *cases;
    CaseHolder *curcase;
    size_t siz;
    int i;
    int range, minval;
    int maxrange = 255;
    int minrange = 3;
    int lastval;
    
    assign = switchstmt->left;
    switchstmt = switchstmt->right;
    if (assign->kind != AST_ASSIGN) {
        ERROR(NULL, "internal error in switch: assignment not found");
        return NULL;
    }
    ident = assign->left;
    exprtype = ExprType(ident);
    if (exprtype && !IsIntOrGenericType(exprtype)) {
        return NULL;
    }
    flexbuf_init(&fb, 0);
    for (top = switchstmt; top; top = top->right) {
        ast = top->left;
        if (ast->kind != AST_IF) {
            ERROR(NULL, "internal error in switch: IF not found");
            return NULL;
        }
        expr = ast->left;
        label = ast->right;
        if (label->kind != AST_THENELSE || label->left->kind != AST_STMTLIST) {
            ERROR(NULL, "internal error in switch: thenelse not found");
            return NULL;
        }
        label = label->left->left;
        if (label->kind != AST_GOTO) {
            ERROR(NULL, "internal error in switch: goto not found");
            return NULL;
        }
        label = label->left;
        if (!AddCases(&fb, ident, expr, label, force_reason)) {
            return NULL;
        }
    }
    // now sort the cases
    siz = flexbuf_curlen(&fb) / sizeof(CaseHolder);
    if (!siz) {
        return NULL;
    }
    cases = (CaseHolder *)flexbuf_get(&fb);
    qsort(cases, siz, sizeof(CaseHolder), caseCmp);
    minval = cases[0].val;
    range = (cases[siz-1].val - minval)+1;
    if (range > maxrange) {
        if (force_reason) {
            ERROR(switchstmt, "%s: too many entries needed for jump table", force_reason);
        }
        return NULL;
    }
    if (range < minrange && !force_reason) {
        return NULL;
    }
    ast = NewAST(AST_JUMPTABLE, NULL, NULL);

    // fix up the assignment
    expr = assign->right;
    if (minval < 0) {
        minval = -minval;
        expr = AstOperator('+', expr, AstInteger(minval));
    } else {
        expr = AstOperator('-', expr, AstInteger(minval));
    }
    expr = AstOperator(K_LIMITMAX_UNS, expr, AstInteger(range));
    assign->right = expr;
    ast->left = assign;
    curcase = cases;
    lastval = curcase->val;
    for (i = 0; i < siz; i++) {
        // insert any default branches we need
        while (lastval < curcase->val) {
            ast->right = AddToList(ast->right, NewAST(AST_LISTHOLDER, defaultlabel, NULL));
            lastval++;
        }
        ast->right = AddToList(ast->right,
                               NewAST(AST_LISTHOLDER, curcase->label, NULL));
        lastval++;
        curcase++;
    }
    ast->right = AddToList(ast->right,
                           NewAST(AST_LISTHOLDER, defaultlabel, NULL));
    //DumpAST(ast);
    return ast;
}

//
// transform a case statement
// we evaluate _tmpvar = expr
// then construct a series of statements like:
//  if (_tmpvar == case1expr) goto case1label
// for each case found within the stmt
// if "force" is nonzero, then we force creation of
// the jump table and print an error if it cannot be made
//
AST *
CreateSwitch(AST *expr, AST *stmt, const char *force_reason)
{
    AST *casetype;
    AST *tmpvar;
    AST *endswitch;
    AST *switchstmt = NULL;
    AST *defaultlabel = NULL;
    AST *gostmt;
    AST *endlabel;
    ASTReportInfo saveinfo;
    
    AstReportAs(stmt, &saveinfo);
    casetype = ExprType(expr);
    tmpvar = AstTempLocalVariable("_tmp_", casetype);
    endswitch = AstTempIdentifier("_endswitch");
    
    switchstmt = NewAST(AST_STMTLIST, AstAssign(tmpvar, expr), NULL);
    // find all CASE labels within stmt; turn them into labels, and
    // create if(tmpvar == val) goto label;

    endlabel = NewAST(AST_LABEL, endswitch, NULL);
    AddSymbolForLabel(endlabel);
    
    // switchstmt will have all the gotos
    switchstmt = CreateGotos(tmpvar, switchstmt, stmt, &defaultlabel, endswitch);
    // add a "goto default"
    if (!defaultlabel) {
        defaultlabel = endswitch;
    }
    gostmt = CreateJumpTable(switchstmt, defaultlabel, force_reason);
    if (gostmt) {
        switchstmt = gostmt;
    } else {
        gostmt = NewAST(AST_GOTO, defaultlabel, NULL);
        switchstmt = AddToList(switchstmt, NewAST(AST_STMTLIST, gostmt, NULL));
    }
    switchstmt = AddToList(switchstmt, stmt);
    switchstmt = AddToList(switchstmt,
                           NewAST(AST_STMTLIST, endlabel, NULL));
    AstReportDone(&saveinfo);
    return switchstmt;
}
