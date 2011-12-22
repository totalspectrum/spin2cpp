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
    memcpy(dup, orig, sizeof(dup));
    dup->left = DupAST(orig->left);
    dup->right = DupAST(orig->right);
    return dup;
}

AST *
AstInteger(long ival)
{
    AST *ast = NewAST(AST_INTEGER, NULL, NULL);
    ast->d.ival = ival;
    return ast;
}
