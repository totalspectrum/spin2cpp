/*
 * Spin to C/C++ converter
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * main header file
 */

#ifndef SPINC_H
#define SPINC_H

#include <stdint.h>

#define VERSIONSTR "3.00-preview-3"

/* Yacc define */
/* we need to put it up here because the lexer includes spin.tab.h */
#define YYSTYPE AST *

#include "ast.h"
#include "lexer.h"
#include "symbol.h"
#include "expr.h"
#include "ir.h"

/* useful macro */
#define N_ELEMENTS(x) (sizeof(x)/sizeof(x[0]))

/* standard Spin defines */
#define RCFAST 0x0001
#define RCSLOW 0x0002
#define XINPUT 0x0004
#define XTAL1  0x0008
#define XTAL2  0x0010
#define XTAL3  0x0020
#define PLL1X  0x0040
#define PLL2X  0x0080
#define PLL4X  0x0100
#define PLL8X  0x0200
#define PLL16X  0x0400

#define NUM_COGS 8

/* some globals */

/* compilation flags */
extern int gl_outcode;  /* type of code to output */
extern int gl_nospin; /* if set, suppress output of Spin methods */
extern int gl_preprocess; /* if set, run the preprocessor on input */
extern int gl_gas_dat;    /* if set, output GAS assembly code inline */
extern char *gl_header; /* comment to prepend to files */
extern int gl_normalizeIdents; /* if set, change case of all identifiers to all lower except first letter upper */
extern int gl_debug;    /* flag: if set, include debugging directives */
extern int gl_expand_constants; /* flag: if set, print constant values rather than symbolic references */
extern int gl_optimize_flags; /* flags for optimization */
#define OPT_REMOVE_UNUSED_FUNCS 0x01
#define OPT_NO_ASM 0x100

// temporary hack
#define gl_ccode (gl_outcode == OUTCODE_C)

#define OUTCODE_CPP  0
#define OUTCODE_C    1
#define OUTCODE_DAT  2
#define OUTCODE_ASM  3

/* types */
extern AST *ast_type_long;
extern AST *ast_type_word;
extern AST *ast_type_byte;
extern AST *ast_type_float;
extern AST *ast_type_string;
extern AST *ast_type_generic;
extern AST *ast_type_void;

typedef enum InstrOps {
    NO_OPERANDS,
    NOP_OPERANDS,
    SRC_OPERAND_ONLY,
    DST_OPERAND_ONLY,
    TWO_OPERANDS,
    CALL_OPERAND,
    JMPRET_OPERANDS,
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
typedef struct modulestate Module;

/* structure describing an object function (method) */
typedef struct funcdef {
    struct funcdef *next;
    int is_public;
    const char *name;
    AST *decl;        /* always filled in with the line numbers of the declaration */
    AST *rettype;        /* the function return type, normally long */
    AST *annotations; /* any annotations for the function (section, etc.) */
    AST *doccomment;  /* documentation comments */
    int numparams;
    AST *params;      /* parameter list */
    AST *locals;      /* local variables */
    AST *body;
    AST *resultexpr;
    AST *extradecl;
    
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
    Module *parse;

    /* various flags */
    unsigned result_used:1;
    unsigned is_static:1; // nonzero if no member variables referenced
    unsigned is_used:1;   // if 0, function is not used
    unsigned is_recursive:1; // if 1, function is called recursively
    unsigned force_static:1; // 1 if the function is forced to be static
    
    /* for walking through functions and avoiding visiting the same one multiple times */
    unsigned visitFlag;

    /* assembly output name */
    Operand *asmname;
    Operand *asmretname;

    /* temporary register info */
    int curtempreg;
    int maxtempreg;
} Function;

/* structure describing a builtin function */
typedef struct builtin {
    const char *name;
    int         numparameters;
    /* function to actually print the builtin */
    void        (*printit)(FILE *, struct builtin *, AST *params);
    const char *cname;  /* c version of the name */

    /* extra data */
    int extradata;

    /* hook called during parsing, or NULL if none needed */
    void        (*parsehook)(struct builtin *);

} Builtin;

/* parser state structure */
struct modulestate {
    struct modulestate *next;  /* to make a stack */
    /* top level objects */
    AST *conblock;
    AST *datblock;
    AST *varblock;
    AST *objblock;
    AST *funcblock;
    AST *topcomment;
    AST *botcomment;

    /* annotations for the DAT block */
    AST *datannotations;

    /* list of methods */
    Function *functions;

    /* lexer state */
    LexStream L;

    /* the symbol table */
    SymbolTable objsyms;

    /* various file name related strings */
    const char *fullname;    /* full name and path of the file */
    char *basename;    /* the file name without ".spin" */
    char *classname;   /* the class name */

    char *datname;     /* the name of the dat section (normally "dat") */

    /* flags for emitting macros */
    char needsPosteffect;
    char needsMinMax;
    char needsRotate;
    char needsShr;
    char needsStdlib;
    char needsYield;
    char needsAbortdef;
    char needsRand;
    char needsSqrt;
    char needsLookup;
    char needsLookdown;
    char needsHighmult;
    char needsBitEncode;
    char needsLockFuncs;
    char needsCogAccess;
    char needsCoginit;
    
    /* flags for output */
    char printLabelsVerbatim;
    char fixImmediate;
    char volatileVariables;
    char sawToken;
};

/* the current parser state */
extern Module *current;
extern Function *curfunc;

/* defines given on the command line */
struct cmddefs {
    const char *name;
    const char *val;
};

/* printing functions */

/* flags for PrintVarList */
#define PUBLIC 0
#define PRIVATE 1
#define LOCAL 2
#define VOLATILE 4
int PrintVarList(FILE *f, AST *type, AST *list, int flags);

void PrintAssign(FILE *f, AST *left, AST *right);

/* create a new AST describing a table lookup */
AST *NewLookup(AST *expr, AST *table);

/* declare labels in PASM */
void DeclareLabels(Module *);

/* declare constants */
void DeclareConstants(AST **conlist);

/* declare all functions */
void DeclareFunctions(Module *);

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
 "comment" is the list of comments preceding this function
*/
void DeclareFunction(int is_public, AST *funcdef, AST *body, AST *annotate, AST *comment);
void DeclareAnnotation(AST *annotation);
int PrintPublicFunctionDecls(FILE *f, Module *P);
int PrintPrivateFunctionDecls(FILE *f, Module *P);
void PrintFunctionBodies(FILE *f, Module *P);
void PrintDataBlock(FILE *f, Module *P, int isBinary);
void PrintDataBlockForGas(FILE *f, Module *P, int inlineAsm);
int  EnterVars(int kind, SymbolTable *stab, void *symval, AST *varlist, int count);
void PrintAnnotationList(FILE *f, AST *ast, char terminal);
void PrintIndentedComment(FILE *f, AST *ast, int indent);
void PrintDebugDirective(FILE *f, AST *ast);
void PrintNewline(FILE *f);

#define PrintComment(f, ast) PrintIndentedComment(f, ast, 0)

void DeclareObjects(AST *newobjs);

/* defines for isBinary flag of PrintDataBlock */
#define TEXT_OUTPUT 0
#define BINARY_OUTPUT 1

/* checks to see whether an AST is a function parameter */
int funcParameterNum(Function *func, AST *var);

/* try to infer function, parameter, and variable types */
/* returns number of new inferences (0 if nothing changed) */
int InferTypes(Module *P);

/* process functions and perform basic transformations on them */
void ProcessFuncs(Module *P);

/* mark a function (and all functions it references) as used */
void MarkUsed(Function *f);

/* check to see if a function is recursive */
void CheckRecursive(Function *f);

/* code for printing errors */
extern int gl_errors;
void ERROR(AST *, const char *msg, ...);
void WARNING(AST *, const char *msg, ...);

extern SymbolTable reservedWords;
extern void InitPasm(int flags);

extern int IsReservedWord(const char *str);

/* return a new object */
AST *NewObject(AST *identifier, AST *string);

/* utility to create a new string by adding an extension to a base file name */
/* if the base string has an extension already, we remove it */
char *ReplaceExtension(const char *base, const char *ext);

/* different kinds of output functions */
void OutputCppCode(const char *name, Module *P, int printMain);
void OutputDatFile(const char *name, Module *P, int prefixBin);
void OutputGasFile(const char *name, Module *P);
void OutputAsmCode(const char *name, Module *P);

/* function to canonicalize an identifier */
void CanonicalizeIdentifier(char *idstr);

/* detect coginit/cognew calls that are for spin methods, return pointer to method */
Function *IsSpinCoginit(AST *body);

/* set a function type, checking for errors */
void SetFunctionType(Function *func, AST *type);

/* perform useful Spin specific transformations */
void SpinTransform(Module *Q);

/* find function symbol in a function call; optionally returns the object ref */
Symbol *FindFuncSymbol(AST *funccall, AST **objrefPtr, Symbol **objsymPtr);

/* get full name for FILE directive */
AST *GetFullFileName(AST *baseString);
    
Symbol *LookupSymbolInFunc(Function *func, const char *name);

#endif
