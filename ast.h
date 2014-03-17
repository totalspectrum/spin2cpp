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

    AST_INSTR = 20,
    AST_HWREG,
    AST_RETURN,
    AST_IF,

    AST_THENELSE = 24,
    AST_RANGE,
    AST_RANGEREF,
    AST_FUNCCALL,

    AST_EXPRLIST = 28,
    AST_INSTRHOLDER,
    AST_INSTRMODIFIER,
    AST_ORG,

    AST_HERE = 32,
    AST_POSTEFFECT,
    AST_WHILE,
    AST_DOWHILE,

    AST_FOR = 36,
    AST_MEMREF,
    AST_ARRAYREF,
    AST_COUNTREPEAT,

    AST_CASE = 40,
    AST_CASEITEM,
    AST_OTHER,
    AST_RES,

    AST_FROM = 44,
    AST_TO,
    AST_STEP,
    AST_FIT,

    AST_ADDROF = 48,
    AST_LOOKUP,
    AST_LOOKDOWN,
    AST_OBJECT,

    AST_METHODREF = 52,
    AST_CONSTREF,
    AST_OBJDECL,
    AST_STRINGPTR,

    AST_YIELD = 56,
    AST_CONSTANT,
    AST_QUIT,
    AST_NEXT,

    AST_RESULT = 60,
    AST_ROUND,
    AST_TRUNC,
    AST_TOFLOAT,

    AST_FILE = 64,
    AST_ABORT,
    AST_CATCH,
    AST_LOOKEXPR,

    AST_DATADDROF = 68,
    AST_ANNOTATION,
    AST_TEMPARRAYDECL,
    AST_TEMPARRAYUSE,

    AST_PUBFUNC = 72,
    AST_PRIFUNC,
    AST_FUNCHOLDER,
    AST_ENUMSKIP,

    AST_LINEBREAK = 76,
    AST_COMMENT,
    AST_COMMENTEDNODE,
};

struct AST {
    enum astkind kind;        /* type of this node */
    union ASTdata d; /* data in this node */
    const char *fileName; /* file associated with this */
    int line;        /* line number */
    AST *left;
    AST *right;
};

/* function declarations */
AST *NewAST(enum astkind kind, AST *left, AST *right);
AST *AddToList(AST *list, AST *newelement);
void RemoveFromList(AST **listptr, AST *newelement);
AST *DupAST(AST *ast);
AST *AstInteger(long intval);
AST *AstIdentifier(const char *name);
AST *AstInstrModifier(int32_t intval);
AST *AstOperator(int32_t intval, AST *left, AST *right);
AST *AstAssign(int32_t intval, AST *left, AST *right);
AST *AstTempVariable(const char *prefix);
AST *AstLookup(enum astkind kind, int index, AST *expr, AST *table);

/* check to see if two trees are identical */
int AstMatch(AST *a, AST *b);

/* length of an AST list */
int AstListLen(AST *a);

#endif
