/*
 * code for handling functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

Function *curfunc;

Function *
NewFunction(void)
{
    Function *f;
    Function *pf;

    f = calloc(1, sizeof(*f));
    if (!f) {
        fprintf(stderr, "FATAL ERROR: Out of memory!\n");
        exit(1);
    }
    /* now link it into the current object */
    if (current->functions == NULL) {
        current->functions = f;
    } else {
        pf = current->functions;
        while (pf->next != NULL)
            pf = pf->next;
        pf->next = f;
    }
    f->parse = current;
    return f;
}

static Symbol *
EnterVariable(SymbolTable *stab, const char *name, AST *type)
{
    Symbol *sym;

    sym = AddSymbol(stab, name, SYM_VARIABLE, (void *)type);
    return sym;
}

void
EnterVars(SymbolTable *stab, AST *curtype, AST *varlist)
{
    AST *lower;
    AST *ast;

    for (lower = varlist; lower; lower = lower->right) {
        if (lower->kind == AST_LISTHOLDER) {
            ast = lower->left;
            switch (ast->kind) {
            case AST_IDENTIFIER:
                EnterVariable(stab, ast->d.string, curtype);
                break;
            case AST_ARRAYDECL:
                EnterVariable(stab, ast->left->d.string, NewAST(AST_ARRAYTYPE, curtype, ast->right));
                break;
            default:
                ERROR(ast, "Internal error: bad AST value %d", ast->kind);
                break;
            }
        } else {
            ERROR(lower, "Expected list of variables, found %d instead", lower->kind);
        }
    }
}

void
EnterParameters(SymbolTable *stab, AST *curtype, AST *varlist)
{
    AST *lower;
    AST *ast;
    int paramnum = 0;

    if (curtype != ast_type_long) {
        ERROR(varlist, "bad type in parameter list");
        return;
    }
    for (lower = varlist; lower; lower = lower->right) {
        if (lower->kind == AST_LISTHOLDER) {
            ast = lower->left;
            switch (ast->kind) {
            case AST_IDENTIFIER:
                AddSymbol(stab, ast->d.string, SYM_PARAMETER, (void *)(intptr_t)paramnum);
                paramnum++;
                break;
            default:
                ERROR(ast, "Internal error: bad AST value %d", ast->kind);
                break;
            }
        } else {
            ERROR(lower, "Expected list for parameters, found %d instead", lower->kind);
        }
    }
}

/*
 * scan a function body for various special conditions
 */
static void
ScanFunctionBody(Function *fdef, AST *body)
{
    AST *ast;
    if (!body)
        return;
    switch(body->kind) {
    case AST_ADDROF:
        /* see if it's a parameter whose address is being taken */
        ast = body->left;
        if (ast->kind == AST_IDENTIFIER && fdef->parmarray == NULL) {
            Symbol *sym = FindSymbol(&fdef->localsyms, ast->d.string);
            if (sym && sym->type == SYM_PARAMETER) {
                fdef->parmarray = NewTemporaryVariable("_parm_");
            }
        }
        break;
    default:
        break;
    }
    ScanFunctionBody(fdef, body->left);
    ScanFunctionBody(fdef, body->right);
}

/*
 * declare a function and create a Function structure for it
 */

void
DeclareFunction(int is_public, AST *funcdef, AST *body)
{
    Function *fdef;
    AST *vars;
    AST *src;
    if (funcdef->kind != AST_FUNCDEF || funcdef->left->kind != AST_FUNCDECL) {
        ERROR(funcdef, "Internal error: bad function definition");
        return;
    }
    src = funcdef->left;
    if (src->left->kind != AST_IDENTIFIER) {
        ERROR(funcdef, "Internal error: no function name");
        return;
    }
    fdef = NewFunction();
    fdef->name = src->left->d.string;
    if (src->right)
        fdef->resultname = src->right->d.string;
    else
        fdef->resultname = "result";
    fdef->is_public = is_public;
    fdef->type = ast_type_long;

    vars = funcdef->right;
    if (vars->kind != AST_FUNCVARS) {
        ERROR(vars, "Internal error: bad variable declaration");
    }

    /* enter the variables into the local symbol table */
    fdef->params = vars->left;
    fdef->locals = vars->right;
    EnterParameters(&fdef->localsyms, ast_type_long, fdef->params);
    EnterVars(&fdef->localsyms, ast_type_long, fdef->locals);
    EnterVariable(&fdef->localsyms, fdef->resultname, ast_type_long);
    fdef->body = body;

    /* check for special conditions */
    ScanFunctionBody(fdef, body);
    AddSymbol(&current->objsyms, fdef->name, SYM_FUNCTION, fdef);
}

static void
PrintParameterList(FILE *f, AST *list)
{
    int needcomma = 0;
    AST *ast;

    if (!list) {
        fprintf(f, "void");
        return;
    }
    while (list) {
        if (list->kind != AST_LISTHOLDER) {
            ERROR(list, "Internal error: expected parameter list");
            return;
        }
        ast = list->left;
        if (ast->kind != AST_IDENTIFIER) {
            ERROR(ast, "Internal error: expected identifier in function parameter list");
            return;
        }
        if (needcomma)
            fprintf(f, ", ");
        fprintf(f, "int32_t %s", ast->d.string);
        needcomma = 1;
        list = list->right;
    }
}

static void
PrintFunction(FILE *f, Function *func)
{
    fprintf(f, "  int32_t\t%s(", func->name);
    PrintParameterList(f, func->params);
    fprintf(f, ");\n");
}

void
PrintPublicFunctionDecls(FILE *f, ParserState *parse)
{
    Function *pf;

    for (pf = parse->functions; pf; pf = pf->next) {
        if (!pf->is_public)
            continue;
        PrintFunction(f, pf);
    }
}

void
PrintPrivateFunctionDecls(FILE *f, ParserState *parse)
{
    Function *pf;

    for (pf = parse->functions; pf; pf = pf->next) {
        if (pf->is_public)
            continue;
        PrintFunction(f, pf);
    }
}

void
PrintVarList(FILE *f, AST *typeast, AST *ast)
{
    AST *decl;
    int needcomma = 0;

    fprintf(f, "  ");
    PrintType(f, typeast);
    fprintf(f, "\t");
    while (ast != NULL) {
        if (needcomma) {
            fprintf(f, ", ");
        }
        needcomma = 1;
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Expected variable list element\n");
            return;
        }
        decl = ast->left;
        switch (decl->kind) {
        case AST_IDENTIFIER:
            fprintf(f, "%s", decl->d.string);
            break;
        case AST_ARRAYDECL:
            fprintf(f, "%s[%d]", decl->left->d.string,
                    (int)EvalConstExpr(decl->right));
            break;
        default:
            ERROR(decl, "Internal problem in variable list: type=%d\n", decl->kind);
            break;
        }
        ast = ast->right;
    }
    fprintf(f, ";\n");
}

static void
PrintFunctionVariables(FILE *f, Function *func)
{
    AST *v;
    fprintf(f, "  int32_t %s = 0;\n", func->resultname);
    if (func->locals) {
        PrintVarList(f, ast_type_long, func->locals);
    }
    if (func->parmarray) {
        fprintf(f, "  int32_t %s[] = { ", func->parmarray);
        for (v = func->params; v; v = v->right) {
            fprintf(f, "%s", v->left->d.string);
            if (v->right) fprintf(f, ", ");
        }
        fprintf(f, " };\n");
    }
}
 
static int PrintStatement(FILE *f, AST *ast, int indent); /* forward declaration */

static int
PrintStatementList(FILE *f, AST *ast, int indent)
{
    int sawreturn = 0;
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR(ast, "Internal error: expected statement list, got %d",
                  ast->kind);
            return 0;
        }
        sawreturn |= PrintStatement(f, ast->left, indent);
        ast = ast->right;
    }
    return sawreturn;
}

static void
PrintCaseExprList(FILE *f, AST *ast, int indent)
{
    while (ast) {
        if (ast->kind == AST_OTHER) {
            fprintf(f, "%*cdefault:\n", indent, ' ');
        } else {
            fprintf(f, "%*ccase ", indent, ' ');
            if (ast->left->kind == AST_RANGE) {
                AST *a, *b;
                a = ast->left->left;
                b = ast->left->right;
                if (!IsConstExpr(a) || !IsConstExpr(b)) {
                    ERROR(ast, "spin2c cannot handle non-constant expressions in case");
                }
                PrintExpr(f, a);
                fprintf(f, " ... ");
                PrintExpr(f, b);
            } else {
                if (!IsConstExpr(ast->left)) {
                    ERROR(ast, "spin2c cannot handle non-constant expressions in case");
                }
                PrintExpr(f, ast->left);
            }
            fprintf(f, ":\n");
        }
        ast = ast->right;
    }
}

static int
PrintCaseItem(FILE *f, AST *ast, int indent)
{
    int sawreturn;

    PrintCaseExprList(f, ast->left, indent);
    sawreturn = PrintStatementList(f, ast->right, indent+2);
    fprintf(f, "%*cbreak;\n", indent+2, ' ');
    return sawreturn;
}

static int
PrintCaseList(FILE *f, AST *ast, int indent)
{
    int sawreturn = 1;
    int items = 0;

    while (ast) {
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Internal error in case list");
            return 0;
        }
        sawreturn = PrintCaseItem(f, ast->left, indent) && sawreturn;
        ast = ast->right;
        items++;
    }

    return (items > 0) && sawreturn;
}

/*
 * print a counting repeat loop
 */
static int
PrintCountRepeat(FILE *f, AST *ast, int indent)
{
    const char *loopname = NULL;
    AST *fromval, *toval;
    AST *stepval;
    AST *limit;
    AST *step;
    AST *loop_le_limit, *loop_ge_limit;
    AST *loopvar = NULL;
    AST *loopleft, *loopright;
    AST *stepstmt;
    int sawreturn = 0;
    int negstep = 0;
    int needsteptest = 1;
    int deltaknown = 0;
    int32_t delta;

    if (ast->left) {
        if (ast->left->kind == AST_IDENTIFIER) {
            loopvar = ast->left;
            loopname = ast->left->d.string;
        } else {
            ERROR(ast, "Need a variable name for the loop");
            return 0;
        }
    }
    ast = ast->right;
    if (ast->kind != AST_FROM) {
        ERROR(ast, "expected FROM");
        return 0;
    }
    fromval = ast->left;
    ast = ast->right;
    if (ast->kind != AST_TO) {
        ERROR(ast, "expected TO");
        return 0;
    }
    toval = ast->left;
    ast = ast->right;
    if (ast->kind != AST_STEP) {
        ERROR(ast, "expected STEP");
        return 0;
    }
    if (ast->left) {
        stepval = ast->left;
    } else {
        stepval = AstInteger(1);
    }

    /* for fixed counts (like "REPEAT expr") we get a NULL value
       for toval; this signals that we should be counting
       down from expr to 1
    */
    if (toval == NULL) {
        needsteptest = 0;
        negstep = 0;
        toval = AstInteger(1);
    } else if (IsConstExpr(fromval) && IsConstExpr(toval)) {
        int32_t fromi, toi;

        fromi = EvalConstExpr(fromval);
        toi = EvalConstExpr(toval);
        needsteptest = 0;
        negstep = (fromi > toi);
    }

    /* set the loop variable */
            
    if (!loopname) {
        loopvar = AstTempVariable("_idx_");
        loopname = loopvar->d.string;
        fprintf(f, "%*cint32_t %s;\n", indent, ' ', loopname);
    }
    fprintf(f, "%*c%s = ", indent, ' ', loopname);
    PrintExpr(f, fromval);
    fprintf(f, ";\n");

    /* set the limit variable */
    if (IsConstExpr(toval)) {
        limit = AstInteger(EvalConstExpr(toval));
    } else {
        limit = AstTempVariable("_limit_");
        fprintf(f, "%*cint32_t ", indent, ' ');
        PrintExpr(f, limit);
        fprintf(f, " = ");
        PrintExpr(f, toval);
        fprintf(f, ";\n");
    }
    /* set the step variable */
    if (IsConstExpr(stepval) && !needsteptest) {
        delta = EvalConstExpr(stepval);
        if (negstep) delta = -delta;
        step = AstInteger(delta);
        deltaknown = 1;
    } else {
        if (negstep) stepval = AstOperator(T_NEGATE, NULL, stepval);
        step = AstTempVariable("_step_");
        fprintf(f, "%*cint32_t ", indent, ' ');
        PrintExpr(f, step);
        fprintf(f, " = ");
        PrintExpr(f, stepval);
        fprintf(f, ";\n");
    }

    stepstmt = AstAssign('+', loopvar, step);

    /* want to do:
     * if (loopvar > limit) step = -step;
     * do {
     *   body
     * } while ( (step > 0 && loopvar <= limit) || (step < 0 && loopvar >= limit));
     */
    loop_ge_limit = AstOperator(T_GE, loopvar, limit);
    loop_le_limit = AstOperator(T_LE, loopvar, limit);
    if (needsteptest) {
        fprintf(f, "%*cif (", indent, ' ');
        PrintBoolExpr(f, loop_ge_limit);
        fprintf(f, ") ");
        PrintExpr(f, step);
        fprintf(f, " = -");
        PrintExpr(f, step);
        fprintf(f, ";\n");
    }

    if (deltaknown) {
        if (delta > 0) {
            loopleft = loop_le_limit;
            loopright = AstInteger(0);
        } else if (delta < 0) {
            loopleft = AstInteger(0);
            loopright = loop_ge_limit;
        } else {
            loopleft = loopright = AstInteger(0);
        }
    } else {
        loopleft = AstOperator(T_AND, AstOperator('>', step, AstInteger(0)), loop_le_limit);
        loopright = AstOperator(T_AND, AstOperator('<', step, AstInteger(0)), loop_ge_limit);
    }

    fprintf(f, "%*cdo {\n", indent, ' ');
    sawreturn = PrintStatementList(f, ast->right, indent+2);
    PrintStatement(f, stepstmt, indent+2);
    fprintf(f, "%*c} while (", indent, ' ');
    if (IsConstExpr(loopleft)) {
        PrintBoolExpr(f, loopright);
    } else if (IsConstExpr(loopright)) {
        PrintBoolExpr(f, loopleft);
    } else {
        PrintBoolExpr(f, AstOperator(T_OR, loopleft, loopright));
    }
    fprintf(f, ");\n");
    return sawreturn;
}

/*
 * returns 1 if a return statement was seen
 */
static int
PrintStatement(FILE *f, AST *ast, int indent)
{
    int sawreturn = 0;
    AST *lhsast = NULL;

    if (!ast) return 0;

    switch (ast->kind) {
    case AST_RETURN:
        fprintf(f, "%*creturn ", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left);
        } else {
            fprintf(f, "%s", curfunc->resultname);
        }
        fprintf(f, ";\n");
        sawreturn = 1;
        break;
    case AST_YIELD:
        fprintf(f, "%*cYield__();\n", indent, ' ');
        break;
    case AST_IF:
        fprintf(f, "%*cif (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ") {\n");
        ast = ast->right;
        if (ast->kind != AST_THENELSE) {
            ERROR(ast, "error parsing if/then/else");
            return 0;
        }
        sawreturn = PrintStatementList(f, ast->left, indent+2);
        if (ast->right) {
            fprintf(f, "%*c} else {\n", indent, ' ');
            sawreturn = PrintStatementList(f, ast->right, indent+2) && sawreturn;
        }
        fprintf(f, "%*c}\n", indent, ' ');
        break;
    case AST_WHILE:
        fprintf(f, "%*cwhile (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ") {\n");
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c}\n", indent, ' ');
        break;
    case AST_DOWHILE:
        fprintf(f, "%*cdo {\n", indent, ' ');
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c} while (", indent, ' ');
        PrintBoolExpr(f, ast->left);
        fprintf(f, ");\n");
        break;
    case AST_COUNTREPEAT:
        sawreturn = PrintCountRepeat(f, ast, indent);
        break;
    case AST_STMTLIST:
        sawreturn = PrintStatementList(f, ast, indent+2);
        break;
    case AST_CASE:
        fprintf(f, "%*cswitch (", indent, ' ');
        PrintExpr(f, ast->left);
        fprintf(f, ") {\n");
        sawreturn = PrintCaseList(f, ast->right, indent+2);
        fprintf(f, "%*c}\n", indent, ' ');
        break;
    case AST_OPERATOR:
        switch (ast->d.ival) {
        case T_NEGATE:
        case T_ABS:
        case T_BIT_NOT:
            lhsast = DupAST(ast->right);
            ast = AstAssign(T_ASSIGN, lhsast, ast);
            break;
        }
        /* fall through */
    default:
    case AST_ASSIGN:
        fprintf(f, "%*c", indent, ' ');
        if (ast->kind == AST_ASSIGN) {
            PrintAssign(f, ast->left, ast->right);
        } else {
            PrintExpr(f, ast);
        }
        fprintf(f, ";\n");
        break;
    }
    return sawreturn;
}

static int
PrintFunctionStmts(FILE *f, Function *func)
{
    return PrintStatementList(f, func->body, 0);
}

void
PrintFunctionBodies(FILE *f, ParserState *parse)
{
    Function *pf;
    int sawreturn;

    for (pf = parse->functions; pf; pf = pf->next) {
        curfunc = pf;
        fprintf(f, "int32_t ");
        fprintf(f, "%s::%s(", parse->classname, pf->name);
        PrintParameterList(f, pf->params);
        fprintf(f, ")\n{\n");
        PrintFunctionVariables(f, pf);
        sawreturn = PrintFunctionStmts(f, pf);
        if (!sawreturn) {
            fprintf(f, "  return %s;\n", pf->resultname);
        }
        fprintf(f, "}\n\n");
        curfunc = NULL;
    }
}

