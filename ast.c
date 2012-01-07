/*
 * functions for dealing with Abstract Syntax Trees
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

AST *
NewAST(int kind, AST *left, AST *right)
{
    AST *ast;

    ast = malloc(sizeof(*ast));
    if (!ast) {
        fprintf(stderr, "FATAL ERROR: out of memory\n");
        abort();
    }
    ast->kind = kind;
    ast->left = left;
    ast->right = right;
    if (current) {
        ast->fileName = current->L.fileName;
        ast->line = current->L.lineCounter;
    } else {
        ast->fileName = "<unknown>";
        ast->line = 0;
    }
    return ast;
}

/*
 * add an AST to a list
 * the "right" pointers link the list, the "left" pointers are free
 * to hold other data
 * returns a pointer to the head of the list
 */
AST *
AddToList(AST *list, AST *newelement)
{
    AST *p;
    if (!list)
        return newelement;
    if (!newelement)
        return list;
    for (p = list; p->right != NULL; p = p->right)
        ;
    p->right = newelement;
    return list;
}

/*
 * duplicate an AST
 */
AST *
DupAST(AST *orig)
{
    AST *dup;

    if (!orig)
        return NULL;

    dup = NewAST(orig->kind, NULL, NULL);
    memcpy(dup, orig, sizeof(*dup));
    dup->left = DupAST(orig->left);
    dup->right = DupAST(orig->right);
    return dup;
}

/* see if two trees match */
int
AstMatch(AST *a, AST *b)
{
    if (a == NULL)
        return b == NULL;
    if (b == NULL)
        return 0;
    if (a->kind != b->kind)
        return 0;
    switch (a->kind) {
    case AST_INTEGER:
        return a->d.ival == b->d.ival;
    case AST_STRING:
    case AST_IDENTIFIER:
        return strcasecmp(a->d.string, b->d.string) == 0;
    case AST_OPERATOR:
    case AST_ASSIGN:
        if (a->d.ival != b->d.ival)
            return 0;
    default:
        break;
    }
    return AstMatch(a->left, b->left) && AstMatch(a->right, b->right);
}

/* create an integer */
AST *
AstInteger(long ival)
{
    AST *ast = NewAST(AST_INTEGER, NULL, NULL);
    ast->d.ival = ival;
    return ast;
}

/*
 * create an instruction modifier with a specific bit pattern
 */
AST *
AstInstrModifier(int32_t ival)
{
    AST *ast = NewAST(AST_INSTRMODIFIER, NULL, NULL);
    ast->d.ival = ival;
    return ast;
}

/*
 * create an operator
 */
AST *
AstOperator(int32_t ival, AST *left, AST *right)
{
    AST *ast = NewAST(AST_OPERATOR, left, right);
    ast->d.ival = ival;
    return ast;
}

AST *
AstAssign(int32_t ival, AST *left, AST *right)
{
    AST *ast;
    AST *rhs;

    if (ival == T_ASSIGN)
        rhs = right;
    else
        rhs = AstOperator(ival, left, right);

    ast = NewAST(AST_ASSIGN, left, rhs);
    ast->d.ival = ival;
    return ast;
}

AST *
AstTempVariable(const char *prefix)
{
    char *name;
    AST *ast = NewAST(AST_IDENTIFIER, NULL, NULL);

    name = NewTemporaryVariable(prefix);
    AddSymbol(&current->objsyms, name, SYM_VARIABLE, (void *)ast_type_long);
    ast->d.string = name;
    return ast;
}

int
AstListLen(AST *list)
{
    int val = 0;
    while (list) {
        val++;
        list = list->right;
    }
    return val;
}
