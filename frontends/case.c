/*
 * Spin to C/C++ translator
 * Copyright 2011-2022 Total Spectrum Software Inc.
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
#include "../backends/bytecode/bc_bedata.h"
#include "util/flexbuf.h"

void
AddSymbolForLabel(AST *ast)
{
    AST *ident = ast->left;
    if (!ident) {
        return; /* can arise due to DEBUG statements */
    }
    if (!IsIdentifier(ident)) {
        ERROR(ast, "Label is not an identifier");
    } else {
        // labels occupy a different namespace from ordinary identifiers
        // so we do not use the internal name (GetIdentifierName) but
        // rather directly use the user's name (GetUserIdentifierName)
        const char *name = GetUserIdentifierName(ident);
        Symbol *sym = FindSymbol(&curfunc->localsyms, name);
        if (sym) {
            WARNING(ident, "Redefining %s as a label", GetUserIdentifierName(ident));
        }
        AddSymbol(&curfunc->localsyms, name, SYM_LOCALLABEL, NULL, NULL);
    }
}

/* turn an expression list into an expression */
static AST *MakeCaseTest(AST *ident, AST *expr)
{
    AST *r, *r2;
    if (!expr) {
        return NULL;
    }
    if (expr->kind == AST_RANGE) {
        if (IsConstExpr(expr->left) && IsConstExpr(expr->right)) {
            AST *left = expr->left;
            AST *right = expr->right;
            int32_t lval = EvalConstExpr(left);
            int32_t rval = EvalConstExpr(right);
            if (lval > rval) {
                left = expr->right;
                right = expr->left;
            }
            r = AstOperator(K_GE, ident, left);
            r2 = AstOperator(K_LE, ident, right);
            r = AstOperator(K_BOOL_AND, r, r2);
        } else {
            r = NewAST(AST_ISBETWEEN, ident, expr);
        }
        return r;
    }
    if (expr->kind == AST_EXPRLIST) {
        r = MakeCaseTest(ident, expr->left);
        if (expr->right) {
            r2 = MakeCaseTest(ident, expr->right);
            r = AstOperator(K_BOOL_OR, r, r2);
        }
        return r;
    }
    if (expr->kind == AST_STRING) {
        const char *s = expr->d.string;
        r = AstOperator(K_EQ, ident, AstInteger(*s));
        s++;
        while (*s) {
            r = AstOperator(K_BOOL_OR, r, AstOperator(K_EQ, ident, AstInteger(*s)));
            s++;
        }
        return r;
    }
    return AstOperator(K_EQ, ident, expr);
}

/*
 * SelectActiveCase:
 * selects just the case that is going to run
 * (call this only if the case selection expression is known constant)
 */
static AST *
SelectActiveCase(AST *switchstmt, AST *stmts, AST *endlabel, AST *defaultlabel)
{
    AST *goodlabel = NULL;
    AST *ast;
    AST *ifstmt;
    AST *block;
    AST *tryblock;
    AST *blockast = NULL;

    for (ast = switchstmt; ast; ast = ast->right) {
        if (ast->kind != AST_STMTLIST) {
            return NULL;
        }
        ifstmt = ast->left;
        if (ifstmt->kind == AST_ASSIGN) {
            // ignore the initial assignment
            continue;
        }
        if (ifstmt->kind != AST_IF) {
            return NULL;
        }
        if (!IsConstExpr(ifstmt->left)) {
            return NULL;
        }
        if (EvalConstExpr(ifstmt->left) != 0) {
            goodlabel = ifstmt->right->left->left;
            if (goodlabel->kind != AST_GOTO) {
                return NULL;
            }
            goodlabel = goodlabel->left;
            break;
        }
    }
    if (!goodlabel) {
        // choose just the default case
        goodlabel = defaultlabel;
    }
    block = NULL;
    for (ast = stmts; ast; ast=ast->right) {
        tryblock = ast->left;
        if (tryblock->kind == AST_GOTO) {
            continue;
        }
        if (tryblock->kind != AST_STMTLIST) {
            return NULL;
        }
        if (tryblock->left->kind != AST_LABEL) {
            return NULL;
        }
        if (BlockContainsLabel(tryblock->right)) {
            // goto may happen into this block; we
            // cannot remove it
            return NULL;
        }
        if (AstMatch(tryblock->left->left, goodlabel)) {
            block = tryblock;
            blockast = ast;
            continue;
        }
    }
    if (!block) {
        return NULL;
    }
    // verify that "goto endlabel" follows block
    // as it happens our construction leaves the goto just after "ast"
    // if it does, then we can replace all of the stmt with just "block"
    ast = blockast;
    if (ast) {
        ast = ast->right;
    }
    if (ast && ast->left && ast->left->kind == AST_GOTO && AstMatch(ast->left->left, endlabel))
    {
        return block;
    }
    return NULL;
}

/*
 * returns a list of if x goto y; statments where x is a case condition and
 * y is the case label
 * "tmpvar" is the case selector (may be a constant)
 * "switchstmt" is the list of gotos
 * "stmt" points to the case statement itself
 * "defaultlabel" will be set to the default case, if any
 * "endswitch" points to a label after the switch
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
        // This can happen now, no need to err
        // ERROR(stmt, "Internal error, case not transformed");
        return switchstmt;
    case AST_CASEITEM:
    {
        AST *ifgoto;
        AST *ifcond;

        AstReportAs(stmt, &saveinfo);
        labelid = AstTempIdentifier("_case_");
        label = NewAST(AST_LABEL, labelid, NULL);
        AddSymbolForLabel(label);
        ifcond = MakeCaseTest(tmpvar, stmt->left);
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
    case AST_ENDCASE:
        if (tmpvar->kind != AST_CASEEXPR) {
            AstReportAs(stmt, &saveinfo);
            if (endswitch) {
                *stmt = *NewAST(AST_GOTO, endswitch, NULL);
            }
            AstReportDone(&saveinfo);
        }
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

static int labelCmp(const void *av, const void *bv)
{
    const CaseHolder *a = (const CaseHolder *)av;
    const CaseHolder *b = (const CaseHolder *)bv;
    return a->label - b->label;
}

static int AddCases(Flexbuf *fb, AST *ident, AST *expr, AST *label, const char *force_reason)
{
    CaseHolder temp;

    memset(&temp, 0, sizeof(temp));
    
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
        return 1;
    } else if (expr->d.ival == K_BOOL_OR) {
        return AddCases(fb, ident, expr->left, label, force_reason) && AddCases(fb, ident, expr->right, label, force_reason);
    } else if (expr->d.ival == K_BOOL_AND) {
        AST *left, *right;
        int32_t minval, maxval;
        left = expr->left;
        right = expr->right;
        if (left->kind != AST_OPERATOR || left->d.ival != K_GE) {
            goto skip;
        }
        if (right->kind != AST_OPERATOR || right->d.ival != K_LE) {
            goto skip;
        }
        if (!AstMatch(ident, left->left) || !AstMatch(ident, right->left)) {
            goto skip;
        }
        if (!IsConstExpr(left->right) || !IsConstExpr(right->right)) {
            goto skip;
        }
        minval = EvalConstExpr(left->right);
        maxval = EvalConstExpr(right->right);
        if ( (maxval - minval) > 256 ) {
            if (force_reason) {
                ERROR(expr, "%s: expression has too many cases", force_reason);
            }
            return 0;
        }
        while (minval <= maxval) {
            temp.val = minval;
            temp.label = label;
            flexbuf_addmem(fb, (char *)&temp, sizeof(temp));
            minval++;
        }
        return 1;
    }
skip:
    if (force_reason) {
        ERROR(expr, "%s: case expression is not valid", force_reason);
    }
    return 0;
}

//
// check a list of if x goto y statements to see if they can be turned into a
// jump table
// if force_reason is non-zero, we must create one
//
AST *CreateJumpTable(AST *switchstmt, AST *defaultlabel, const char *force_reason)
{
    AST *top, *ast;
    AST *assign, *ident;
    AST *label;
    AST *exprtype;
    Flexbuf fb;
    CaseHolder *cases;
    CaseHolder *curcase;
    size_t siz;
    int i;
    int range, minval;
    int lastval;
    int defaults_seen = 0;
    int distinct_cases = 0;

    if (gl_output == OUTPUT_C || gl_output == OUTPUT_CPP) {
        return NULL;
    }
    if (!force_reason && gl_output == OUTPUT_BYTECODE && !(gl_interp_kind == INTERP_KIND_NUCODE) && !(curfunc->optimize_flags & OPT_CASETABLE)) {
        // Don't auto-convert normal CASEs in bytecode mode by default
        return NULL;
    }
    assign = switchstmt->left;
    switchstmt = switchstmt->right;
    if (assign->kind != AST_ASSIGN) {
        ERROR(NULL, "internal error in switch: assignment not found");
        return NULL;
    }
    ident = assign->left;
    if (IsConstExpr(assign->right)) {
        // no point in doing a jump table for a constant expression
        return NULL;
    }
    exprtype = ExprType(ident);
    if (exprtype && !IsIntOrGenericType(exprtype)) {
        return NULL;
    }
    flexbuf_init(&fb, 128);
    for (top = switchstmt; top; top = top->right) {
        ast = top->left;
        if (ast->kind != AST_IF) {
            ERROR(NULL, "internal error in switch: IF not found");
            return NULL;
        }
        AST *expr = ast->left;
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
    if (!force_reason) {
        // initial sort to see how many distinct labels there are
        qsort(cases, siz, sizeof(CaseHolder), labelCmp);
        for (i = 0; i < siz-1; i++) {
            if (cases[i].label != cases[i+1].label) {
                distinct_cases++;
            }
        }
        // if only 1/6 of the cases are distinct, skip it
        if (distinct_cases * 6 < siz) {
            return NULL;
        }
    }
    // final sort to get into numerical order
    qsort(cases, siz, sizeof(CaseHolder), caseCmp);
    minval = cases[0].val;
    range = (cases[siz-1].val - minval)+1;

    // Perform some checks
    AST *expr = assign->right;
    const bool signed_limit = gl_output == OUTPUT_BYTECODE && !interp_can_unsigned() && !(minval == 0 && CanUseEitherSignedOrUnsigned(expr));

    const int maxrange = 255;
    const int minrange = gl_output != OUTPUT_BYTECODE ? 3 : (signed_limit ? 7 : 5);

    if (range > maxrange) {
        if (force_reason) {
            ERROR(switchstmt, "%s: too many entries needed for jump table", force_reason);
        }
        return NULL;
    }
    if (range < minrange && !force_reason) {
        return NULL;
    }

    // at least density/maxrange items must be non-default
    const int density = force_reason
                        ? 0 // we will always use a jump table
                        : maxrange / 2; // otherwise at least half the jump table entries must be non-default

    ast = NewAST(AST_JUMPTABLE, NULL, NULL);

    curcase = cases;
    lastval = curcase->val;
    for (i = 0; i < siz; i++) {
        // insert any default branches we need
        while (lastval < curcase->val) {
            ast->right = AddToList(ast->right, NewAST(AST_LISTHOLDER, defaultlabel, NULL));
            lastval++;
            defaults_seen++;
        }
        ast->right = AddToList(ast->right,
                               NewAST(AST_LISTHOLDER, curcase->label, NULL));
        curcase++;
        if (i < (siz-1) && curcase->val == lastval && curcase->label) {
            ERROR(curcase->label, "Duplicate case value (%d)", curcase->val);
        } else {
            lastval++;
        }
    }
    ast->right = AddToList(ast->right,
                           NewAST(AST_LISTHOLDER, defaultlabel, NULL));
    defaults_seen++;

    if (signed_limit) {
        minval--;
        range++;
        ast->right = NewAST(AST_LISTHOLDER, defaultlabel,ast->right);
        defaults_seen++;
    }


    // if the jump table is mostly defaults, revert to if/else
    // mathematically we want defaults_seen / range > density / maxrange
    if ( (defaults_seen * maxrange) / range > density  && density > 0) {
        return NULL;
    }
    // fix up the assignment
    if (minval < 0) {
        expr = AstOperator('+', expr, AstInteger(-minval));
    } else if (minval > 0) {
        expr = AstOperator('-', expr, AstInteger(minval));
    }
    if (signed_limit) {
        expr = AstOperator(K_LIMITMIN,AstOperator(K_LIMITMAX, expr, AstInteger(range)),AstInteger(0));
    } else {
        expr = AstOperator(K_LIMITMAX_UNS, expr, AstInteger(range));
    }
    assign->right = expr;
    ast->left = assign;

    //DumpAST(ast);
    return ast;
}

//
// transform a case statement
// we evaluate _tmpvar = expr
// then construct a series of statements like:
//  if (_tmpvar == case1expr) goto case1label
// for each case found within the stmt
// if "force_reason" is nonzero, then we force creation of
// the jump table and print an error if it cannot be made
//
AST *
CreateSwitch(AST *origast, const char *force_reason)
{
    AST *expr = origast->left, *stmt = DupAST(origast->right);

    AST *casetype;
    AST *tmpvar;
    AST *endswitch;
    AST *switchstmt = NULL;
    AST *defaultlabel = NULL;
    AST *gostmt;
    AST *endlabel;
    AST *use_expr;
    ASTReportInfo saveinfo;
    int filterCases;
    int optimize_flags = curfunc->optimize_flags;

    //DumpAST(stmt);

    AstReportAs(stmt, &saveinfo);
    casetype = ExprType(expr);

    if (IsConstExpr(expr) && (optimize_flags & OPT_DEADCODE)) {
        use_expr = expr;
        tmpvar = NewAST(AST_EMPTY,NULL,NULL);
        filterCases = 1;
    } else if (gl_output == OUTPUT_BYTECODE && !(gl_interp_kind == INTERP_KIND_NUCODE)) {
        use_expr = NewAST(AST_CASEEXPR,AstTempIdentifier("_caseexpr"),NULL);
        tmpvar = use_expr; // Assigning to AST_CASEEXPR can't compile to anything meaningful and thus it should not leave this function...
        filterCases = 0;
    } else {
        tmpvar = AstTempLocalVariable("_tmp_", RemoveTypeModifiers(casetype));
        use_expr = tmpvar;
        filterCases = 0;
    }


    endswitch = AstTempIdentifier("_endswitch");

    switchstmt = NewAST(AST_STMTLIST, AstAssign(tmpvar, expr), NULL);

    // find all CASE labels within stmt; turn them into labels, and
    // create if(tmpvar == val) goto label;

    endlabel = NewAST(AST_LABEL, endswitch, NULL);
    AddSymbolForLabel(endlabel);

    // switchstmt will have all the gotos we make
    switchstmt = CreateGotos(use_expr, switchstmt, stmt, &defaultlabel, endswitch);
    // add a "goto default"
    if (!defaultlabel) {
        defaultlabel = endswitch;
    }
    if (filterCases) {
        // do not want jump table
        // instead, just pick the active part
        AST *pick;
        pick = SelectActiveCase(switchstmt, stmt, endswitch, defaultlabel);
        if (pick) {
            return pick;
        }
        gostmt = NULL;
    } else {
        gostmt = CreateJumpTable(switchstmt, defaultlabel, force_reason);
    }
    if (gostmt) {
        switchstmt = gostmt;
    } else if (use_expr->kind == AST_CASEEXPR) {
        AstReportDone(&saveinfo);
        return origast;
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
