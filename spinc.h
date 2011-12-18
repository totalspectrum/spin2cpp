/*
 * main header file
 */

#ifndef SPINC_H
#define SPINC_H

#include <stdint.h>

/*
 * types of data which may be contained within an AST node
 */

union ASTdata {
    uint32_t ival;      /* unsigned integer value */
    const char *string; /* string value */
    void *ptr;          /* generic pointer */
};

typedef struct AST AST;

struct AST {
    int kind;        /* type of this node */
    union ASTdata d; /* data in this node */
    AST *left;
    AST *right;
};

/* AST types */
#define AST_INTEGER    1
#define AST_STRING     2
#define AST_IDENTIFIER 3
#define AST_EXPR       4

/* Yacc define */
#define YYSTYPE AST *

/* function declarations */
AST *NewAST(int kind, AST *left, AST *right);

#endif
