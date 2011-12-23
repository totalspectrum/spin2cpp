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
    return f;
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

    fdef->body = body;
}

static void
PrintParameterList(FILE *f, AST *ast)
{
    int needcomma = 0;
    if (!ast) {
        fprintf(f, "void");
        return;
    }
    while (ast) {
        if (ast->kind != AST_IDENTIFIER) {
            ERROR("Internal error: expected identifier in function parameter list");
            return;
        }
        if (needcomma)
            fprintf(f, ", ");
        fprintf(f, "%s", ast->d.string);
        needcomma = 1;
        ast = ast->right;
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
 
static void PrintStatement(FILE *f, AST *ast, int indent); /* forward declaration */

static void
PrintStatementList(FILE *f, AST *ast, int indent)
{
    while (ast) {
        if (ast->kind != AST_STMTLIST) {
            ERROR("Internal error: expected statement list, got %d",
                  ast->kind);
            return;
        }
        PrintStatement(f, ast->left, indent);
        ast = ast->right;
    }
}

static void
PrintStatement(FILE *f, AST *ast, int indent)
{
    switch (ast->kind) {
    case AST_RETURN:
        fprintf(f, "%*creturn ", indent, ' ');
        if (ast->left) {
            PrintExpr(f, ast->left);
        } else {
            fprintf(f, "%s", curfunc->resultname);
        }
        fprintf(f, ";\n");
        break;
    default:
        fprintf(f, "%*c", indent, ' ');
        PrintExpr(f, ast);
        fprintf(f, ";\n");
        break;
    }
            
}

static void
PrintFunctionStmts(FILE *f, Function *func)
{
    PrintStatementList(f, func->body, 2);
}

void
PrintFunctionBodies(FILE *f, ParserState *parse)
{
    Function *pf;

    for (pf = parse->functions; pf; pf = pf->next) {
        curfunc = pf;
        fprintf(f, "int32_t ");
        fprintf(f, "%s::%s(", parse->classname, pf->name);
        PrintParameterList(f, pf->params);
        fprintf(f, ")\n{\n");
        PrintFunctionVariables(f, pf);
        PrintFunctionStmts(f, pf);
        fprintf(f, "}\n\n");
    }
}
