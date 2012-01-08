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
            ERROR(lower, "Expected list in constant, found %d instead", lower->kind);
        }
    }
}

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
    EnterVars(&fdef->localsyms, ast_type_long, fdef->params);
    EnterVars(&fdef->localsyms, ast_type_long, fdef->locals);
    EnterVariable(&fdef->localsyms, fdef->resultname, ast_type_long);
    fdef->body = body;

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
    fprintf(f, "  int32_t %s = 0;\n", func->resultname);
    if (func->locals) {
        PrintVarList(f, ast_type_long, func->locals);
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
 * print a counting for loop
 */
static int
PrintCountRepeat(FILE *f, AST *ast, int indent)
{
    const char *loopname = NULL;
    AST *fromval, *toval;
    AST *stepval;
    int sawreturn = 0;
    int32_t delta;

    fprintf(f, "%*cfor (", indent, ' ');
    if (ast->left) {
        if (ast->left->kind == AST_IDENTIFIER)
            loopname = ast->left->d.string;
        else
            ERROR(ast, "Need a variable name for the loop");
    }
    if (!loopname) {
        loopname = NewTemporaryVariable(NULL);
        fprintf(f, "int32_t ");
    }
    fprintf(f, "%s = ", loopname);
    ast = ast->right;
    if (ast->kind != AST_FROM) {
        ERROR(ast, "expected FROM");
        return 0;
    }
    fromval = ast->left;
    PrintExpr(f, fromval);
    fprintf(f, "; %s != ", loopname);
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
        if (IsConstExpr(fromval) && IsConstExpr(toval)) {
            delta = EvalConstExpr(toval) - EvalConstExpr(fromval);
            stepval = (delta < 0) ? AstInteger(-1) : AstInteger(+1);
        } else {
            ERROR(ast, "Unable to calculate step value");
            return 0;
        }
    }
    toval = AstOperator('+', toval, stepval);
    if (IsConstExpr(toval)) {
        fprintf(f, "%d", EvalConstExpr(toval));
    } else {
        PrintExpr(f, toval);
    }
    fprintf(f, "; %s += ", loopname);
    PrintExpr(f, stepval);
    fprintf(f, ") {\n");
    sawreturn = PrintStatementList(f, ast->right, indent+2);
    fprintf(f, "%*c}\n", indent, ' ');
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
    case AST_IF:
        fprintf(f, "%*cif (", indent, ' ');
        PrintExpr(f, ast->left);
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
        PrintExpr(f, ast->left);
        fprintf(f, ") {\n");
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c}\n", indent, ' ');
        break;
    case AST_DOWHILE:
        fprintf(f, "%*cdo {\n", indent, ' ');
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c} while (", indent, ' ');
        PrintExpr(f, ast->left);
        fprintf(f, ");\n");
        break;
    case AST_COUNTFOR:
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

