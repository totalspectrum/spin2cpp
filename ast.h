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
    AST_LISTHOLDER,
    AST_INTEGER,
    AST_STRING,

    AST_IDENTIFIER = 4,
    AST_OPERATOR,
    AST_FLOAT,
    AST_ASSIGN,

    AST_ENUMSET = 8,
    AST_ARRAYDECL,
    AST_BYTELIST,
    AST_WORDLIST,

    AST_LONGLIST = 12,
    AST_INTTYPE,
    AST_UNSIGNEDTYPE,
    AST_ARRAYTYPE,

    AST_FUNCDECL = 16,
    AST_FUNCDEF,
    AST_FUNCVARS,
    AST_STMTLIST,

    AST_RETURN = 20,
    AST_IF,
    AST_THENELSE,
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
AST *DupAST(AST *ast);
AST *AstInteger(long intval);

#endif
