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
EnterVariable(int kind, SymbolTable *stab, const char *name, AST *type)
{
    Symbol *sym;

    sym = AddSymbol(stab, name, kind, (void *)type);
    return sym;
}

int
EnterVars(int kind, SymbolTable *stab, void *symval, AST *varlist)
{
    AST *lower;
    AST *ast;
    Symbol *sym;
    int count = 0;
    int size;

    for (lower = varlist; lower; lower = lower->right) {
        if (lower->kind == AST_LISTHOLDER) {
            ast = lower->left;
            switch (ast->kind) {
            case AST_IDENTIFIER:
                sym = EnterVariable(kind, stab, ast->d.string, symval);
                sym->count = count++;
                break;
            case AST_ARRAYDECL:
                sym = EnterVariable(kind, stab, ast->left->d.string, NewAST(AST_ARRAYTYPE, symval, ast->right));
                size = EvalConstExpr(ast->right);
                sym->count = count;
                count += size;
                break;
            default:
                ERROR(ast, "Internal error: bad AST value %d", ast->kind);
                break;
            }
        } else {
            ERROR(lower, "Expected list of variables, found %d instead", lower->kind);
            return 0;
        }
    }
    return count;
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
        if (ast->kind == AST_IDENTIFIER) {
            Symbol *sym = FindSymbol(&fdef->localsyms, ast->d.string);
            if (sym) {
                if (sym->type == SYM_PARAMETER) {
                    if (!fdef->parmarray)
                        fdef->parmarray = NewTemporaryVariable("_parm_");
                } else if (sym->type == SYM_LOCALVAR) {
                    if (!fdef->localarray)
                        fdef->localarray = NewTemporaryVariable("_local_");
                }
            }
        } else if (ast->kind == AST_RESULT) {
            /* hack to make F32.spin work: it expects all variables
               to be arranged as result,parameters,locals
               so force all of those into the _parm_ array
            */
            if (!fdef->parmarray) {
                fdef->parmarray = NewTemporaryVariable("_parm_");
            }
            fdef->localarray = fdef->parmarray;
            if (!fdef->result_in_parmarray) {
                fdef->result_in_parmarray = 1;
                fdef->resultexpr = NewAST(AST_RESULT, NULL, NULL);
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
    const char *resultname;
    int localcount;

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
        resultname = src->right->d.string;
    else
        resultname = "result";
    fdef->resultexpr = AstIdentifier(resultname);
    fdef->is_public = is_public;
    fdef->type = ast_type_long;

    vars = funcdef->right;
    if (vars->kind != AST_FUNCVARS) {
        ERROR(vars, "Internal error: bad variable declaration");
    }

    /* enter the variables into the local symbol table */
    fdef->params = vars->left;
    fdef->locals = vars->right;
    EnterVars(SYM_PARAMETER, &fdef->localsyms, ast_type_long, fdef->params);
    localcount = EnterVars(SYM_LOCALVAR, &fdef->localsyms, ast_type_long, fdef->locals);

    AddSymbol(&fdef->localsyms, resultname, SYM_RESULT, ast_type_long);

    fdef->body = body;

    /* check for special conditions */
    ScanFunctionBody(fdef, body);

    /* if we put the locals into an array, record the size of that array */
    if (fdef->localarray)
        fdef->localarray_len = localcount;

    /* define the function itself */
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
PrintFunctionDecl(FILE *f, Function *func)
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
        PrintFunctionDecl(f, pf);
    }
}

void
PrintPrivateFunctionDecls(FILE *f, ParserState *parse)
{
    Function *pf;

    for (pf = parse->functions; pf; pf = pf->next) {
        if (pf->is_public)
            continue;
        PrintFunctionDecl(f, pf);
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

Symbol *VarSymbol(Function *func, AST *ast)
{
    if (ast && ast->kind == AST_ARRAYDECL)
        ast = ast->left;
    if (ast == 0 || ast->kind != AST_IDENTIFIER) {
        ERROR(ast, "internal error: expected variable name\n");
        return NULL;
    }
    return FindSymbol(&func->localsyms, ast->d.string);
}

/*
 * calculate the size of a type
 */
int typeSize(AST *ast)
{
    if (!ast)
        return 1;
    if (ast->kind == AST_ARRAYTYPE) {
        return typeSize(ast->left)*EvalConstExpr(ast->right);
    }
    if (ast->kind == AST_INTTYPE || ast->kind == AST_UNSIGNEDTYPE) {
        return EvalConstExpr(ast->left);
    }
    ERROR(ast, "internal error: bad type kind %d", ast->kind);
    return 0;
}

static void
PrintFunctionVariables(FILE *f, Function *func)
{
    AST *v;
    int offset = 0;

    if (func->parmarray) {
        int n;
        fprintf(f, "  int32_t %s[] = { ", func->parmarray);
        if (func->result_in_parmarray) {
            fprintf(f, "0"); offset++;
        }
        for (v = func->params; v; v = v->right) {
            if (offset) fprintf(f, ", ");
            fprintf(f, "%s", v->left->d.string);
            offset++;
            
        }
        if (func->localarray == func->parmarray) {
            Symbol *sym;
            for (v = func->locals; v; v = v->right) {
                sym = VarSymbol(func, v->left);
                if (sym) {
                    sym->count += offset;
                    n = typeSize((AST *)sym->val);
                    while (n > 0) {
                        fprintf(f, ", ");
                        fprintf(f, "0");
                        n -= 4;
                    }
                }
            }
        }
        fprintf(f, " };\n");
    }
    if (!func->result_in_parmarray)
        fprintf(f, "  int32_t %s = 0;\n", func->resultexpr->d.string);
    if (func->locals) {
        if (func->localarray && func->localarray == func->parmarray) {
            /* nothing to do here */
        } else if (func->localarray && func->localarray_len > 0) {
            fprintf(f, "  int32_t %s[%d];\n", func->localarray, func->localarray_len);
        } else {
            PrintVarList(f, ast_type_long, func->locals);
        }
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
PrintCaseExprList(FILE *f, AST *var, AST *ast)
{
    int needor = 0;
    while (ast) {
        if (needor) fprintf(f, " || ");
        if (ast->kind == AST_OTHER) {
            fprintf(f, "1");
        } else {
            if (ast->left->kind == AST_RANGE) {
                AST *a, *b;
                a = ast->left->left;
                b = ast->left->right;
                fprintf(f, "Between__(");
                PrintExpr(f, var);
                fprintf(f, ", ");
                PrintExpr(f, a);
                fprintf(f, ", ");
                PrintExpr(f, b);
                fprintf(f, ")");
            } else {
                PrintBoolExpr(f, AstOperator(T_EQ, var, ast->left));
            }
        }
        needor = 1;
        ast = ast->right;
    }
}

static int
PrintCaseItem(FILE *f, AST *var, AST *ast, int indent)
{
    int sawreturn;

    fprintf(f, "(");
    PrintCaseExprList(f, var, ast->left);
    fprintf(f, ") {\n");
    sawreturn = PrintStatementList(f, ast->right, indent);
    fprintf(f, "%*c}", indent, ' ');
    return sawreturn;
}

static int
PrintCaseStmt(FILE *f, AST *expr, AST *ast, int indent)
{
    int sawreturn = 1;
    int items = 0;
    int first = 1;
    AST *var;

    if (expr->kind == AST_IDENTIFIER) {
        var = expr;
    } else {
        var = AstTempVariable(NULL);
        fprintf(f, "%*cint32_t %s = ", indent, ' ', var->d.string);
        PrintExpr(f, expr);
        fprintf(f, ";\n");
    }
    while (ast) {
        if (ast->kind != AST_LISTHOLDER) {
            ERROR(ast, "Internal error in case list");
            return 0;
        }
        if (first) {
            fprintf(f, "%*cif ", indent, ' ');
            first = 0;
        } else {
            fprintf(f, " else if ");
        }
        sawreturn = PrintCaseItem(f, var, ast->left, indent) && sawreturn;
        ast = ast->right;
        items++;
    }
    fprintf(f, "\n");
    return (items > 0) && sawreturn;
}

/*
 * print a counting repeat loop
 */
static int
PrintCountRepeat(FILE *f, AST *ast, int indent)
{
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
        } else if (ast->left->kind == AST_RESULT) {
            loopvar = ast->left;
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
            
    if (!loopvar) {
        loopvar = AstTempVariable("_idx_");
        fprintf(f, "%*cint32_t %s;\n", indent, ' ', loopvar->d.string);
    }
    fprintf(f, "%*c", indent, ' ');
    PrintExpr(f, loopvar);
    fprintf(f, " = ");
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
            PrintExpr(f, curfunc->resultexpr);
        }
        fprintf(f, ";\n");
        sawreturn = 1;
        break;
    case AST_ABORT:
        fprintf(f, "%*cif (!abortChain__) abort();\n", indent, ' ');
        fprintf(f, "%*cabortChain__->val =  ", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left);
        } else {
            PrintExpr(f, curfunc->resultexpr);
        }
        fprintf(f, ";\n");
        fprintf(f, "%*clongjmp(abortChain__->jmp, 1);\n", indent, ' ');
        break;
    case AST_YIELD:
        fprintf(f, "%*cYield__();\n", indent, ' ');
        break;
    case AST_QUIT:
        fprintf(f, "%*cbreak;\n", indent, ' ');
        break;
    case AST_NEXT:
        fprintf(f, "%*ccontinue;\n", indent, ' ');
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
        sawreturn = PrintCaseStmt(f, ast->left, ast->right, indent);
        break;
    case AST_OPERATOR:
        switch (ast->d.ival) {
        case T_NEGATE:
        case T_ABS:
        case T_SQRT:
        case T_BIT_NOT:
        case T_DECODE:
        case T_ENCODE:
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
    return PrintStatementList(f, func->body, 2);
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
            fprintf(f, "  return ");
            PrintExpr(f, pf->resultexpr);
            fprintf(f, ";\n");
        }
        fprintf(f, "}\n\n");
        curfunc = NULL;
    }
}

