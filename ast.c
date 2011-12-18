/*
 * functions for dealing with Abstract Syntax Trees
 */
#include <stdio.h>
#include <stdlib.h>
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
