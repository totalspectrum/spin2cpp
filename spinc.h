/*
 * main header file
 */

#ifndef SPINC_H
#define SPINC_H

#include <stdint.h>

#define VERSIONSTR "0.97"

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

/* compilation flags */
extern int gl_ccode;  /* if set, we are outputting C code instead of C++*/
extern int gl_nospin; /* if set, suppress output of Spin methods */
extern int gl_static; /* if set, objects are static; this is the default in C mode */
extern int gl_preprocess; /* if set, run the preprocessor on input */

extern char *gl_header; /* comment to prepend to files */

/* types */
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
    AST *annotations; /* any annotations for the function (section, etc.) */
    int numparams;
    AST *params;      /* parameter list */
    AST *locals;      /* local variables */
    AST *body;
    AST *resultexpr;
    /* array holding parameters, if necessary */
    const char *parmarray;
    /* true if the result should be placed in the parameter array */
    int result_in_parmarray;
    /* array holding locals, if necessary */
    const char *localarray;
    /* total size of localarray */
    int localarray_len;

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
    char needsShr;
    char needsStdlib;
    char needsYield;
    char needsBetween;
    char needsAbortdef;
    char needsRand;
    char needsSqrt;
    char needsLookup;
    char needsLookdown;
};

/* the current parser state */
extern ParserState *current;
extern Function *curfunc;

/* defines given on the command line */
struct cmddefs {
    const char *name;
    const char *val;
};

/* printing functions */
#define PUBLIC 0
#define PRIVATE 1
#define LOCAL 2
void PrintVarList(FILE *f, AST *type, AST *list, int scope);
void PrintAssign(FILE *f, AST *left, AST *right);

/* create a new AST describing a table lookup */
AST *NewLookup(AST *expr, AST *table);

/* declare labels in PASM */
void DeclareLabels(ParserState *);

/* declare constants */
void DeclareConstants(AST *conlist);

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

 "annotate" is a list of C++ annotation strings
*/
void DeclareFunction(int is_public, AST *funcdef, AST *body, AST *annotate);
void DeclareAnnotation(AST *annotation);
void PrintPublicFunctionDecls(FILE *f, ParserState *P);
void PrintPrivateFunctionDecls(FILE *f, ParserState *P);
void PrintFunctionBodies(FILE *f, ParserState *P);
void PrintDataBlock(FILE *f, ParserState *P, int isBinary);
int  EnterVars(int kind, SymbolTable *stab, void *symval, AST *varlist);

void DeclareObjects(AST *newobjs);

/* defines for isBinary flag of PrintDataBlock */
#define TEXT_OUTPUT 0
#define BINARY_OUTPUT 1

/* checks to see whether an AST is a function parameter */
int funcParameterNum(Function *func, AST *var);

/* code for printing errors */
extern int gl_errors;
void ERROR(AST *, const char *msg, ...);

extern SymbolTable reservedWords;
extern void InitPasm(void);

/* return a new object */
AST *NewObject(AST *identifier, AST *string);

/* different kinds of output functions */
void OutputCppCode(const char *name, ParserState *P, int printMain);
void OutputDatFile(const char *name, ParserState *P);

#endif
