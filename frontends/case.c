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

//
// transform a case statement
// we evaluate _tmpvar = expr
// then construct a series of statements like:
//  if (_tmpvar == case1expr) goto case1label
// for each case found within the stmt
//
AST *
CreateSwitch(AST *expr, AST *stmt)
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
    gostmt = NewAST(AST_GOTO, defaultlabel, NULL);
    switchstmt = AddToList(switchstmt, NewAST(AST_STMTLIST, gostmt, NULL));
    switchstmt = AddToList(switchstmt, stmt);
    switchstmt = AddToList(switchstmt,
                           NewAST(AST_STMTLIST, endlabel, NULL));
    AstReportDone(&saveinfo);
    return switchstmt;
}
