/*
 * main header file
 */

#ifndef SPINC_H
#define SPINC_H

#include <stdint.h>

#define VERSIONSTR "0.4"

/* Yacc define */
/* we need to put it up here because the lexer includes spin.tab.h */
#define YYSTYPE AST *

#include "ast.h"
#include "lexer.h"
#include "symbol.h"
#include "expr.h"

/* useful macro */
#define N_ELEMENTS(x) (sizeof(x)/sizeof(x[0]))

/* some globals */
extern AST *ast_type_long;
extern AST *ast_type_word;
extern AST *ast_type_byte;

typedef enum InstrOps {
    NO_OPERANDS,
    NOP_OPERANDS,
    SRC_OPERAND_ONLY,
    DST_OPERAND_ONLY,
    TWO_OPERANDS,
    CALL_OPERAND,
} InstrOps;

/* structure describing a PASM instruction */
typedef struct Instruction {
    const char *name;      /* instruction mnemonic */
    uint32_t    binary;    /* binary form of instruction */
    InstrOps    ops;       /* operand forms */
} Instruction;

/* instruction modifiers */
typedef struct instrmodifier {
    const char *name;
    uint32_t modifier;
} InstrModifier;

#define IMMEDIATE_INSTR (1<<22)

/* structure describing a dat block label */
typedef struct label {
    uint32_t offset;
    uint32_t asmval;
    AST *type;  /* type of value following the label */
} Label;

/* structure describing a hardware register */
typedef struct hwreg {
    const char *name;
    uint32_t    addr;
    const char *cname;
} HwReg;

/* forward declaration */
typedef struct parserstate ParserState;

/* structure describing an object function (method) */
typedef struct funcdef {
    struct funcdef *next;
    int is_public;
    const char *name;
    AST *type;        /* the function type, normally long */
    const char *resultname;
    int numparams;
    AST *params;      /* parameter list */
    AST *locals;      /* local variables */
    AST *body;

    /* array holding parameters, if necessary */
    const char *parmarray;
    /* local symbols */
    SymbolTable localsyms;

    /* parser state during compilation */
    ParserState *parse;
} Function;

/* structure describing a builtin function */
typedef struct builtin {
    const char *name;
    int         numparameters;
    void        (*printit)(FILE *, struct builtin *, AST *params);
    const char *cname;
} Builtin;

/* parser state structure */
struct parserstate {
    struct parserstate *next;  /* to make a stack */
    /* top level objects */
    AST *conblock;
    AST *datblock;
    AST *varblock;
    AST *objblock;

    /* list of methods */
    Function *functions;

    /* list of lookup arrays required */
    AST *arrays;

    /* AST for current token */
    AST *ast;

    /* lexer state */
    LexStream L;

    /* the symbol table */
    SymbolTable objsyms;

    /* various file name related strings */
    char *basename;    /* the file name without ".spin" */
    char *classname;   /* the class name */

    /* flags for emitting macros */
    char needsMinMax;
    char needsRotate;
    char needsStdlib;
    char needsYield;
};

/* the current parser state */
extern ParserState *current;
extern Function *curfunc;

/* printing functions */
void PrintVarList(FILE *f, AST *type, AST *list);
void PrintAssign(FILE *f, AST *left, AST *right);
void PrintLookupArray(FILE *f, AST *array);

/* create a new AST describing a table lookup */
AST *NewLookup(AST *expr, AST *table);

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
void PrintDataBlock(FILE *f, ParserState *P);
void EnterVars(SymbolTable *stab, AST *type, AST *varlist);

/* checks to see whether an AST is a function parameter */
int funcParameterNum(Function *func, AST *var);

/* code for printing errors */
extern int gl_errors;
void ERROR(AST *, const char *msg, ...);

extern SymbolTable reservedWords;
extern void InitPasm(void);

/* return a new object */
AST *NewObject(AST *identifier, AST *string);
#endif
