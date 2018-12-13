#ifndef FRONTEND_COMMON_H
#define FRONTEND_COMMON_H

#include <stdio.h>
#include <stdbool.h>

/* forward declaration */
typedef struct modulestate Module;

#include "ast.h"
#include "symbol.h"
#include "expr.h"
#include "util/util.h"
#include "util/flexbuf.h"
#include "instr.h"

#include "optokens.h"
#include "util/flexbuf.h"

/*
 * input stream; could be either a string or a file
 */

struct lexstream {
    void *ptr;  /* current pointer */
    void *arg;  /* original value of pointer */
    int (*getcf)(LexStream *);
#define UNGET_MAX 8 /* we can ungetc this many times */
    int ungot[UNGET_MAX];
    int ungot_ptr;

#define MAX_INDENTS 64
    /* input state */
    int in_block;  /* T_DAT, T_PUB, or T_CON */
    int save_block; /* so we can nest T_ASM inside others */
    int block_firstline; /* what line does the block start on? */
    int indent[MAX_INDENTS];    /* current indentation level */
    int indentsp;               /* pointer into stack of indentation level */
    int pending_indent;
    int eoln;      /* 1 if end of line seen */
    int eof;       /* 1 if EOF seen */
    int firstNonBlank;

    /* last global label */
    const char *lastGlobal;

    /* for error messages */
    int colCounter;
    int lineCounter;
    const char *fileName;

    /* for handling Unicode CR+LF */
    int sawCr;

    int pendingLine;  /* 1 if lineCounter needs incrementing */

    Flexbuf curLine;  /* string of current line */
    Flexbuf lineInfo; /* pointers to line info about the file */
};

#define getLineInfoIndex(L) (flexbuf_curlen(&(L)->lineInfo) / sizeof(LineInfo))

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

#define LONG_SIZE 4

/* some globals */

/* compilation flags */
extern int gl_p2;      /* set for P2 output */
extern int gl_output;  /* type of output to produce */
extern int gl_outputflags; /* modifiers (e.g. LMM or COG code */
extern int gl_nospin; /* if set, suppress output of Spin methods */
extern int gl_preprocess; /* if set, run the preprocessor on input */
extern int gl_gas_dat;    /* if set, output GAS assembly code inline */
extern char *gl_header1; /* first comment line to prepend to files */
extern char *gl_header2; /* second comment line to prepend to files */
extern int gl_normalizeIdents; /* if set, change case of all identifiers to all lower except first letter upper */
extern int gl_debug;    /* flag: if set, include debugging directives */
extern int gl_srccomments; /* if set, include original source as comments */
extern int gl_listing;     /* if set, produce an assembly listing */
extern int gl_expand_constants; /* flag: if set, print constant values rather than symbolic references */
extern int gl_infer_ctypes; /* flag: use inferred types for generated C/C++ code */
extern int gl_optimize_flags; /* flags for optimization */
#define OPT_REMOVE_UNUSED_FUNCS 0x01
#define OPT_PERFORM_CSE         0x02
#define OPT_REMOVE_HUB_BSS      0x04
#define OPT_BASIC_ASM           0x08  /* basic peephole optimizations &c */
#define OPT_INLINE_SMALLFUNCS   0x10  /* inline small functions */
#define OPT_INLINE_SINGLEUSE    0x20  /* inline single use functions */

#define DEFAULT_ASM_OPTS        (OPT_REMOVE_UNUSED_FUNCS|OPT_INLINE_SMALLFUNCS|OPT_BASIC_ASM)
#define EXTRA_ASM_OPTS          (OPT_INLINE_SINGLEUSE|OPT_PERFORM_CSE|OPT_REMOVE_HUB_BSS) /* extras added with -O */

extern int gl_printprogress;  /* print files as we process them */
extern int gl_fcache_size;   /* size of fcache for LMM mode */
extern const char *gl_cc; /* C compiler to use; NULL means default (PropGCC) */
extern const char *gl_intstring; /* int string to use */

extern int gl_dat_offset; /* offset for @@@ operator */
extern int gl_compressed; /* if instructions should be compressed (NOT IMPLEMENTED) */
extern int gl_fixedreal;  /* if instead of float we should use 16.16 fixed point */
#define G_FIXPOINT 16  /* number of bits of fraction */

extern int gl_caseSensitive; /* whether Spin/PASM is case sensitive */

/* types of output */
#define OUTPUT_CPP  0
#define OUTPUT_C    1
#define OUTPUT_DAT  2
#define OUTPUT_ASM  3
#define OUTPUT_COGSPIN 4  /* like ASM, but with a Spin wrapper */

/* flags for output */
#define OUTFLAG_COG_CODE 0x01
#define OUTFLAG_COG_DATA 0x02
#define OUTFLAGS_DEFAULT (OUTFLAG_COG_CODE)

/* types */
extern AST *ast_type_long;
extern AST *ast_type_word;
extern AST *ast_type_byte;
extern AST *ast_type_unsigned_long;
extern AST *ast_type_signed_word;
extern AST *ast_type_signed_byte;
extern AST *ast_type_float;
extern AST *ast_type_string;
extern AST *ast_type_generic;
extern AST *ast_type_void;
extern AST *ast_type_ptr_long;
extern AST *ast_type_ptr_word;
extern AST *ast_type_ptr_byte;
extern AST *ast_type_ptr_void;

/* structure describing a dat block label */
typedef struct label {
    uint32_t hubval;  // for P1, offset in dat block; for P2, a real address
    uint32_t cogval;  // cog address; note that it is in bytes, needs to be divided by 4 for most uses
    AST *type;   /* type of value following the label */
    Symbol *org; /* symbol associated with last .org, if needed */
    unsigned flags; /* flags for the label */
#define LABEL_USED_IN_SPIN      0x01
#define LABEL_NEEDS_EXTRA_ALIGN 0x02
#define LABEL_IN_HUB            0x04
} Label;


/* structure describing a hardware register */
typedef struct hwreg {
    const char *name;
    uint32_t    addr;
    const char *cname;
} HwReg;

/* structure describing an object function (method) */
typedef struct funcdef {
    struct funcdef *next;
    int is_public;
    const char *name;
    AST *decl;        /* always filled in with the line numbers of the declaration */
    AST *overalltype; /* the type of the function, including types of parameters */
    AST *annotations; /* any annotations for the function (section, etc.) */
    AST *doccomment;  /* documentation comments */
    int numparams;
    AST *params;      /* parameter list */
    AST *defaultparams; /* default values for parameters */
    int numlocals;
    AST *locals;      /* local variables */
    AST *body;
    int numresults;
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

    /* containing module/object */
    Module *module;

    /* various flags */
    unsigned result_used:1;
    unsigned is_static:1; // nonzero if no member variables referenced
    unsigned is_recursive:1; // if 1, function is called recursively
    unsigned force_static:1; // 1 if the function is forced to be static
    unsigned cog_code:1;     // 1 if function should always be placed in cog
    unsigned cog_task:1;     // 1 if function is started in another cog
    unsigned used_as_ptr:1;  // 1 if function's address is taken as a pointer
    unsigned local_address_taken: 1; // 1 if a local variable or parameter has its address taken
    unsigned no_inline:1;    // 1 if function cannot be inlined
    unsigned is_leaf:1;      // 1 if function is a leaf function
    
    /* number of places this function is called from */
    /* 0 == unused function, 1== ripe for inlining */
    unsigned callSites;
    
    /* for walking through functions and avoiding visiting the same one multiple times */
    unsigned visitFlag;

    /* back-end specific data */
    void *bedata;
    
    /* struct to hold closure data */
    Module *closure;

    /* name of function that called us if callSites > 0 */
    const char *caller;

    /* language of this function */
    int language;
} Function;

/* structure describing a builtin function */
typedef struct builtin {
    const char *name;
    int         numparameters;
    /* function to actually print the builtin */
    void        (*printit)(Flexbuf *, struct builtin *, AST *params);
    const char *cname;  /* c version of the name */
    const char *gasname; /* name to use if --gas is given */
    
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

    /* lexer state during input */
    LexStream *Lptr;

    /* the symbol table */
    SymbolTable objsyms;

    /* size of variables & objects used */
    int varsize;

    /* size of the data section */
    int datsize;
    
    /* various file name related strings */
    const char *fullname;    /* full name and path of the file */
    char *basename;    /* the file name without ".spin" and without a directory */
    char *classname;   /* the class name */
    const char *outfilename; /* file name for output */
    char *datname;     /* the name of the dat section (normally "dat") */

    /* for walking through modules and avoiding visiting the same one multiple times */
    unsigned visitflag;

    /* flags for output */
    char pasmLabels;
    char volatileVariables;
    char sawToken;
    char codeCog; // if 1, module should be placed in COG memory
    char datHasCode; // if 1, DAT section has PASM code in it
    char gasPasm;    // if 1, output is in GAS format
    
    /* back end specific flags */
    void *bedata;

    /* last language the module was presented with */
    int lastLanguage;

    /* "body" (statements outside any function) */
    /* not all languages support this */
    AST *body;

    /* closures/classes that are associated with this module and need processing */
    struct modulestate *subclasses;
};

/* maximum number of items in a multiple assignment */
#define MAX_TUPLE 8

/* the current parser state */
extern Module *current;
extern Function *curfunc;
extern SymbolTable *currentTypes;

/* defines given on the command line */
struct cmddefs {
    const char *name;
    const char *val;
};

extern SymbolTable spinReservedWords;  // in the lexer
extern SymbolTable basicReservedWords;
extern Module *globalModule;       // global functions and variables

/* create a new AST describing a table lookup */
AST *NewLookup(AST *expr, AST *table);

/* declare labels in PASM */
void DeclareLabels(Module *);

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
 "rettype" is the return type of the function, if it is known, or NULL
*/
void DeclareFunction(Module *P, AST *rettype, int is_public, AST *funcdef, AST *body, AST *annotate, AST *comment);

/* streamlined DeclareFunction: "ftype" is the function type (return + parameters) */
void DeclareTypedFunction(Module *P, AST *ftype, AST *name, int is_public, AST *body);
                          
void DeclareToplevelAnnotation(AST *annotation);

/* functions for printing data into a flexbuf */
typedef struct DataBlockOutFuncs {
    void (*startAst)(Flexbuf *f, AST *ast);
    void (*putByte)(Flexbuf *f, int c);
    void (*endAst)(Flexbuf *f, AST *ast);
} DataBlockOutFuncs;

/*
 * information about relocations:
 * data blocks are normally just binary blobs; but if an absolute address
 * (like @@@foo) is requested, we need a way to specify that address
 * Note that a normal @foo in a dat section is a relative address;
 * @@@foo requires that we add the base of the dat to @foo.
 * For now, the relocation system works only on longs, and only in some
 * modes. For each long that needs the base of dat added to it we emit
 * a relocation r, which contains (a) the offset of the relocatable long
 * in bytes from the start of the dat, and (b) the value to add to the base
 * of the dat section at that long.
 * The relocs should be sorted in order of increasing offset, so we can
 * easily process them in order along with the output.
 *
 * We also re-use the Reloc struct to hold debug information as well, so
 * that we can provide source listings for the DAT section contents.
 * for that purpose we emit DebugEntry
 */
typedef struct Reloc {
    int32_t  kind;    // reloc or debug
    int32_t  off;     // the address the entry affects (offset from dat base)
    intptr_t val;     // value to add to dat base, or pointer to LineInfo
} Reloc;

#define RELOC_KIND_LONG  0
#define RELOC_KIND_DEBUG 1

void PrintDataBlock(Flexbuf *f, Module *P, DataBlockOutFuncs *funcs, Flexbuf *relocs);
void PrintDataBlockForGas(Flexbuf *f, Module *P, int inlineAsm);
int  EnterVars(int kind, SymbolTable *stab, AST *symtype, AST *varlist, int startoffset);

// find the variable symbol for an identifier or array decl
Symbol *VarSymbol(Function *func, AST *ast);

// add a local variable to a function
void AddLocalVariable(Function *func, AST *var, AST *vartype, int symbolkind);

// find the size of a type
int TypeSize(AST *ast);

// check if two types are compatible
int CompatibleTypes(AST *A, AST *B);

#define PrintComment(f, ast) PrintIndentedComment(f, ast, 0)

void DeclareObjects(AST *newobjs);

/* checks to see whether an AST is a function parameter */
int funcParameterNum(Function *func, AST *var);

/* try to infer function, parameter, and variable types */
/* returns number of new inferences (0 if nothing changed) */
int InferTypes(Module *P);

/* process a function and perform basic transformations on it */
void ProcessOneFunc(Function *pf);

/* mark a function (and all functions it references) as used */
void MarkUsed(Function *f, const char *callerName);

/* check to see if a function is recursive */
void CheckRecursive(Function *f);

/* code for printing errors */
extern int gl_errors;
void ERROR(AST *, const char *msg, ...);
void WARNING(AST *, const char *msg, ...);
void ERROR_UNKNOWN_SYMBOL(AST *);

/* this one is used by the lexer */
void SYNTAX_ERROR(const char *msg, ...);

/* just prints the start of the error message, formatted appropriately
   with file name and line number */
void ERRORHEADER(const char *fileName, int lineno, const char *msg);

/* return a new object */
AST *NewObject(AST *identifier, AST *string);
/* like NewObject, but does not instantiate data */
AST *NewAbstractObject(AST *identifier, AST *string);

/* different kinds of output functions */
void OutputCppCode(const char *name, Module *P, int printMain);
void OutputDatFile(const char *name, Module *P, int prefixBin);
void OutputGasFile(const char *name, Module *P);
void OutputLstFile(const char *name, Module *P);
void OutputAsmCode(const char *name, Module *P, int printMain);

/* detect coginit/cognew calls that are for spin methods, return pointer to method */
Function *IsSpinCoginit(AST *body);

/* set a function type, checking for errors */
void SetFunctionReturnType(Function *func, AST *type);

/* get the return type for a function */
AST *GetFunctionReturnType(Function *func);

/* find function symbol in a function call; optionally returns the object ref */
Symbol *FindFuncSymbol(AST *funccall, AST **objrefPtr, int errflag);

/* find function symbol in a function call; new version that can work on abstract types */
Symbol *FindCalledFuncSymbol(AST *funccall, AST **objrefPtr, int errflag);

/* get full name for FILE directive */
AST *GetFullFileName(AST *baseString);
    
Symbol *LookupSymbolInFunc(Function *func, const char *name);

/* convert an object type to a class */
Module *GetClassPtr(AST *objtype);

/*
 * see if an AST refers to a parameter of this function, and return
 * an index into the list if it is
 * otherwise, return -1
 */
int FuncParameterNum(Function *func, AST *var);

//
// compile IR code for the functions within a module
//
void CompileIntermediate(Module *P);

/* fetch clock frequency settings */
int GetClkFreq(Module *P, unsigned int *clkfreqptr, unsigned int *clkregptr);

// some string utilities

// find the last directory separator (/ or, for windows, \)
const char *FindLastDirectoryChar(const char *name);

/* utility to create a new string by adding an extension to a base file name */
/* if the base string has an extension already, we remove it */
char *ReplaceExtension(const char *base, const char *ext);

/* add "basename" to the path portion of  "directory" to make a complete file name */
char *ReplaceDirectory(const char *basename, const char *directory);

// add a propeller checksum to a binary file
// if eepromSize is non-zero, create an EEPROM compatible file
// of the given size
int DoPropellerChecksum(const char *fname, size_t eepromSize);

// initialization functions
void Init();
void InitPreprocessor();
void SetPreprocessorLanguage(int language);

// perform common sub-expression elimination on a function
void PerformCSE(Module *P);
void PerformLoopOptimization(Module *P);

// simplify statments like a^=b to a = a^b
void SimplifyAssignments(AST **astptr);

#define P2_HUB_BASE 0x400

/*
 * functions to initialize lexers
 */
void initSpinLexer(int flags);

/* parsing functions */
#define LANG_SPIN  0
#define LANG_BASIC 1
#define LANG_C     2

void InitGlobalModule(void);
Module *NewModule(const char *modulename, int language);
Module *ParseFile(const char *filename);

/* declare a single member variable of P */
void DeclareOneMemberVar(Module *P, AST *ident, AST *typ);

/* declare a member variable of P if it does not already exist */
void MaybeDeclareMemberVar(Module *P, AST *ident, AST *typ);

/* declare a single global variable */
void DeclareOneGlobalVar(Module *P, AST *ident, AST *typ);

// type inference based on BASIC name (e.g. A$ is a string)
AST *InferTypeFromName(AST *identifier);

/* determine whether a loop needs a yield, and if so, insert one */
AST *CheckYield(AST *body);

/* add a list element together with accumulated comments */
AST *CommentedListHolder(AST *ast);

/* add an instruction together with comments */
AST *NewCommentedInstr(AST *instr);

/* fetch pending comments from the lexer */
AST *GetComments(void);

/* is an AST identifier a local variable? */
bool IsLocalVariable(AST *ast);

/* push the current types identifier */
void PushCurrentTypes(void);

/* pop the current types identifier */
void PopCurrentTypes(void);

/* create a declaration list, or add a type name to currentTypes */
AST *MakeDeclaration(AST *decl);

#endif
