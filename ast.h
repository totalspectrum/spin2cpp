#ifndef SPIN_AST_H
#define SPIN_AST_H

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

/* AST types */
enum astkind {
    AST_UNKNOWN = 0,
    AST_INTEGER,
    AST_STRING,
    AST_IDENTIFIER,
    AST_EXPR,
    AST_FLOAT,
    AST_ASSIGN,
    AST_CONDECL,
    AST_ENUMSET
};

struct AST {
    enum astkind kind;        /* type of this node */
    union ASTdata d; /* data in this node */
    AST *left;
    AST *right;
};

/* function declarations */
AST *NewAST(int kind, AST *left, AST *right);
AST *AddToList(AST *list, AST *newelement);

#endif
