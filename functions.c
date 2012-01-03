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
                ERROR("Internal error: bad AST value %d", ast->kind);
                break;
            }
        } else {
            ERROR("Expected list in constant, found %d instead", lower->kind);
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
        ERROR("Internal error: bad function definition");
        return;
    }
    src = funcdef->left;
    if (src->left->kind != AST_IDENTIFIER) {
        ERROR("Internal error: no function name");
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
        ERROR("Internal error: bad variable declaration");
    }

    /* enter the variables into the local symbol table */
    fdef->params = vars->left;
    EnterVars(&fdef->localsyms, ast_type_long, vars->left);
    EnterVars(&fdef->localsyms, ast_type_long, vars->right);
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
            ERROR("Internal error: expected parameter list");
            return;
        }
        ast = list->left;
        if (ast->kind != AST_IDENTIFIER) {
            ERROR("Internal error: expected identifier in function parameter list");
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

static void
PrintFunctionVariables(FILE *f, Function *func)
{
    fprintf(f, "  int32_t %s = 0;\n", func->resultname);
}
 
static int PrintStatement(FILE *f, AST *ast, int indent); /* forward declaration */

static int
PrintStatementList(FILE *f, AST *ast, int indent)
{
    int sawreturn = 0;
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR("Internal error: expected statement list, got %d",
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
            PrintExpr(f, ast->left);
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
            ERROR("Internal error in case list");
            return 0;
        }
        sawreturn = sawreturn && PrintCaseItem(f, ast->left, indent);
        ast = ast->right;
        items++;
    }

    return (items > 0) && sawreturn;
}

/*
 * returns 1 if a return statement was seen
 */
static int
PrintStatement(FILE *f, AST *ast, int indent)
{
    int sawreturn = 0;
    AST *lhsast = NULL;
    char *tmpvarname;

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
            ERROR("error parsing if/then/else");
            return 0;
        }
        sawreturn = PrintStatementList(f, ast->left, indent+2);
        if (ast->right) {
            fprintf(f, "%*c} else {\n", indent, ' ');
            sawreturn = sawreturn && PrintStatementList(f, ast->right, indent+2);
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
        tmpvarname = NewTemporaryVariable(NULL);
        fprintf(f, "%*cfor (int %s = ", indent, ' ', tmpvarname);
        PrintExpr(f, ast->left);
        fprintf(f, "; %s > 0; --%s) {\n", tmpvarname, tmpvarname);
        sawreturn = PrintStatementList(f, ast->right, indent+2);
        fprintf(f, "%*c}\n", indent, ' ');
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
        fprintf(f, "%*c", indent, ' ');
        PrintExpr(f, ast);
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
    }
}
