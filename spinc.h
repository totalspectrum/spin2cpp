/*
 * main header file
 */

#ifndef SPINC_H
#define SPINC_H

/* Yacc define */
/* we need to put it up here because the lexer includes spin.tab.h */
#define YYSTYPE AST *

#include "ast.h"
#include "lexer.h"
#include "symbol.h"

/* useful macro */
#define N_ELEMENTS(x) (sizeof(x)/sizeof(x[0]))

/* some globals */
extern AST *ast_type_long;
extern AST *ast_type_word;
extern AST *ast_type_byte;

typedef enum InstrOps {
    NO_OPERANDS,
    ONE_OPERAND,
    TWO_OPERANDS,
    CALL_OPERAND,
    JMP_OPERAND,
} InstrOps;

/* structure describing a PASM instruction */
typedef struct Instruction {
    const char *name;      /* instruction mnemonic */
    uint32_t    binary;    /* binary form of instruction */
    InstrOps    ops;       /* operand forms */

    /* function for the spin form of the instruction */
    void      (*spinform)(FILE *, AST *);
} Instruction;

/* structure describing a hardware register */
typedef struct hwreg {
    const char *name;
    uint32_t    addr;
    const char *cname;
} HwReg;

/* structure describing a function */
typedef struct funcdef {
    struct funcdef *next;
    int is_public;
    const char *name;
    AST *type;        /* the function type, normally long */
    const char *resultname;
    int numparams;
    AST *params;      /* parameter list */
    AST *body;
} Function;

/* parser state structure */
typedef struct parserstate {
    struct parserstate *next;  /* to make a stack */
    /* top level objects */
    AST *conblock;
    AST *datblock;
    AST *varblock;

    /* list of methods */
    Function *functions;

    /* AST for current token */
    AST *ast;

    /* lexer state */
    LexStream L;

    /* the symbol table */
    SymbolTable objsyms;

    /* various file name related strings */
    char *basename;    /* the file name without ".spin" */
    char *classname;   /* the class name */
} ParserState;

/* the current parser state */
extern ParserState *current;
extern Function *curfunc;

/* printing functions */
void PrintExpr(FILE *f, AST *expr);

/* evaluate a constant expression */
long EvalConstExpr(AST *expr);

/* declare a function */
/* "body" is the list of statements */
/* "funcdef" is an AST_FUNCDEF; this is set up as follows:
            AST_FUNCDEF
           /           \
    AST_FUNCDECL       AST_FUNCVARS
     /      \            /       \
    name    result    parameters locals

 here:
 "name" and "result" are AST_IDENTIFIERS
 "parameters" and "locals" are AST_LISTHOLDERS
 holding a list of identifiers and/or array declarations
*/
void DeclareFunction(int is_public, AST *funcdef, AST *body);
void PrintPublicFunctionDecls(FILE *f, ParserState *P);
void PrintPrivateFunctionDecls(FILE *f, ParserState *P);
void PrintFunctionBodies(FILE *f, ParserState *P);

/* code for printing errors */
extern int gl_errors;
void ERROR(const char *msg, ...);

extern SymbolTable reservedWords;
extern void InitPasm(void);

#endif
