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
#define UNGET_MAX 16 /* we can ungetc this many times */
    int ungot[UNGET_MAX];
    int ungot_ptr;

    /* input state */
    int block_type;  /* BLOCK_DAT, BLOCK_PUB etc. */
#define BLOCK_DAT 100  /* Spin DAT block */
#define BLOCK_ASM 101  /* Spin inline ASM */
#define BLOCK_PASM 102 /* C PASM block */
#define BLOCK_PUB 103
#define BLOCK_PRI 104
#define BLOCK_CON 105
#define BLOCK_VAR 106
#define BLOCK_OBJ 108
#define BLOCK_UNKNOWN 999
    
    int save_block; /* so we can nest T_ASM inside others */
    int block_firstline; /* what line does the block start on? */
#define MAX_INDENTS 64
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

    /* current language being parsed */
    int language;
    
    /* for handling Unicode CR+LF */
    int sawCr;

    int pendingLine;  /* 1 if lineCounter needs incrementing */

    Flexbuf curLine;  /* string of current line */
    Flexbuf lineInfo; /* pointers to line info about the file */

    unsigned flags;
#define LEXSTREAM_FLAG_NOSRC 0x01
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
# define P2_REV_A 1
# define P2_REV_B 2
#define DEFAULT_P2_VERSION P2_REV_B

extern int gl_have_lut; /* are there any functions placed in LUT? */
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
#define OPT_REMOVE_UNUSED_FUNCS 0x000001
#define OPT_PERFORM_CSE         0x000002
#define OPT_REMOVE_HUB_BSS      0x000004
#define OPT_BASIC_REGS          0x000008  /* basic register optimization */
#define OPT_INLINE_SMALLFUNCS   0x000010  /* inline small functions */
#define OPT_INLINE_SINGLEUSE    0x000020  /* inline single use functions */
#define OPT_AUTO_FCACHE         0x000040  /* use FCACHE for P2 */
#define OPT_PERFORM_LOOPREDUCE  0x000080  /* loop reduction */
#define OPT_DEADCODE            0x000100  /* dead code elimination */
#define OPT_BRANCHES            0x000200  /* branch conditionalization */
#define OPT_PEEPHOLE            0x000400  /* peephole optimization */
#define OPT_CONST_PROPAGATE     0x000800  /* constant propagation */
#define OPT_LOOP_BASIC          0x001000  /* simple loop conversion */
#define OPT_TAIL_CALLS          0x002000  /* tail call optimization */

#define OPT_FLAGS_ALL           0xffffff
#define OPT_ASM_BASIC  (OPT_BASIC_REGS|OPT_BRANCHES|OPT_PEEPHOLE|OPT_CONST_PROPAGATE)                        
#define DEFAULT_ASM_OPTS        (OPT_DEADCODE|OPT_REMOVE_UNUSED_FUNCS|OPT_INLINE_SMALLFUNCS|OPT_ASM_BASIC|OPT_AUTO_FCACHE|OPT_LOOP_BASIC|OPT_TAIL_CALLS)
#define EXTRA_ASM_OPTS          (OPT_INLINE_SINGLEUSE|OPT_PERFORM_CSE|OPT_PERFORM_LOOPREDUCE|OPT_REMOVE_HUB_BSS) /* extras added with -O2 */

extern int gl_warn_flags;     /* flags for warnings */
#define WARN_LANG_EXTENSIONS    0x01
#define WARN_HIDE_MEMBERS       0x02
#define WARN_ALL                0xFFFF

extern int gl_printprogress;  /* print files as we process them */
extern int gl_fcache_size;   /* size of fcache for LMM mode */
extern const char *gl_cc; /* C compiler to use; NULL means default (PropGCC) */
extern const char *gl_intstring; /* int string to use */

extern int gl_dat_offset; /* offset for @@@ operator */
extern int gl_compress;   /* if instructions should be compressed (NOT IMPLEMENTED) */
extern int gl_fixedreal;  /* if instead of float we should use 16.16 fixed point */
#define G_FIXPOINT 16  /* number of bits of fraction */

extern int gl_caseSensitive; /* whether Spin/PASM is case sensitive */
extern int gl_no_coginit;    /* skip coginit code */

extern int gl_exit_status;   /* after exit return exit code via special sequence */
extern int gl_relocatable;   /* 1 for position independent output */

/* LMM kind selected */
extern int gl_lmm_kind;
#define LMM_KIND_ORIG  0
#define LMM_KIND_SLOW  1
#define LMM_KIND_TRACE 2
#define LMM_KIND_CACHE 3
#define LMM_KIND_COMPRESS 4

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

/* various global feature flags */
/* set in MarkUsed() in spinc.c */
extern int gl_features_used;
#define FEATURE_LONGJMP_USED 0x01

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
extern AST *ast_type_const_generic;
extern AST *ast_type_void;
extern AST *ast_type_ptr_long;
extern AST *ast_type_ptr_word;
extern AST *ast_type_ptr_byte;
extern AST *ast_type_ptr_void;
extern AST *ast_type_bitfield;
extern AST *ast_type_long64;
extern AST *ast_type_unsigned_long64;
extern AST *ast_type_generic_funcptr;
// special function pointer used for Spin2 SEND builtin
extern AST *ast_type_sendptr;

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
#define LABEL_HAS_INSTR         0x08 /* an instruction follows the label */
#define LABEL_HAS_JMP           0x10 /* a jmp instruction follows the label */
    unsigned size;
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
    const char *name; /* internal name */
    const char *user_name; /* name the user used; differs from "name" for static functions */
    AST *decl;        /* always filled in with the line numbers of the declaration */
    AST *overalltype; /* the type of the function, including types of parameters */
    AST *annotations; /* any annotations for the function (section, etc.) */
    AST *doccomment;  /* documentation comments */
    /* number of parameters:
       if negative, it is the number of fixed parameters before a varargs
       if UNKNOWN_PARAMCOUNT it is unknown (any number may be passed in registers)
       otherwise it's the number of parameters
    */
#define UNKNOWN_PARAMCOUNT 32768
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
    int optimize_flags;   // optimizations to be applied
    unsigned code_placement:2;
#define CODE_PLACE_DEFAULT 0
#define CODE_PLACE_HUB 1
#define CODE_PLACE_COG 2
#define CODE_PLACE_LUT 3    
    unsigned result_used:1;
    unsigned result_declared:1; // if 1, function result was explicitly declared
    unsigned is_static:1; // nonzero if no member variables referenced
    unsigned is_recursive:1; // if 1, function is called recursively
    unsigned force_static:1; // 1 if the function is forced to be static
    unsigned cog_task:1;     // 1 if function is started in another cog
    unsigned used_as_ptr:1;  // 1 if function's address is taken as a pointer
    unsigned local_address_taken: 1; // 1 if a local variable or parameter has its address taken
    unsigned no_inline:1;    // 1 if function cannot be inlined
    unsigned is_leaf:1;      // 1 if function is a leaf function
    unsigned uses_alloca:1;  // 1 if function uses alloca
    unsigned stack_local:1;  // 1 if function has a local that must go on stack
    unsigned has_throw:1;    // 1 if function has a "throw" in it
    unsigned toplevel:1;     // 1 if function is top level

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

    /* true if language specific processing has been done */
    char lang_processed;

    /* counter for local variables */
    int local_var_counter;
    
} Function;

/* structure describing a builtin function */
typedef struct builtin {
    const char *name;
    int         numparameters;
    /* function to actually print the builtin */
    void        (*printit)(Flexbuf *, struct builtin *, AST *params);
    const char *p1_cname;  /* c version of the name */
    const char *p2_cname;  /* name to use if --p2 is given */
    const char *gas_cname; /* name to use if --gas is given */
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
    AST *pendingvarblock;
    AST *finalvarblock;
    AST *objblock;
    AST *funcblock;
    AST *topcomment;
    AST *botcomment;

    /* helpers for AddToListEx */
    AST *datblock_tail;
    AST *conblock_tail;
    AST *parse_tail;

    /* annotations for the DAT block */
    AST *datannotations;

    /* BASIC data section */
    AST *bas_data;
    AST *bas_data_tail;
    
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
    char isUnion;    // if 1, module actually represents a union
    char defaultPrivate; // if 1, member variables default to private
    char longOnly;   // if 1, all module variables are longs
    
    /* back end specific flags */
    void *bedata;

    /* "main" language for the module */
    int mainLanguage;
    
    /* current language we are processing */
    int curLanguage;

    /* "body" (statements outside any function) */
    /* not all languages support this */
    AST *body;

    /* closures/classes that are associated with this module and need processing */
    struct modulestate *subclasses;

    /* superclass of this one */
    struct modulestate *superclass;

    /* type of this class */
    AST *type;
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

extern SymbolTable spinCommonReservedWords;  // in the lexer
extern SymbolTable basicReservedWords;
extern SymbolTable cReservedWords;
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
AST *DeclareFunction(Module *P, AST *rettype, int is_public, AST *funcdef, AST *body, AST *annotate, AST *comment);

/* streamlined DeclareFunction: "ftype" is the function type (return + parameters) */
AST *DeclareTypedFunction(Module *P, AST *ftype, AST *name, int is_public, AST *body, AST *annotation, AST *comment);

/* declare a template for a function */
void DeclareFunctionTemplate(Module *P, AST *templ);

/* instantiate a templated function */
AST *InstantiateTemplateFunction(Module *P, AST *templ, AST *call);

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
 * in bytes from the start of the dat, and (b) the symbol to add to the base
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
    int32_t  addr;    // the address the entry affects (offset from dat base)
    Symbol   *sym;    // the symbol this relocation is relative to (NULL for dat base)
    int32_t  symoff;  // offset relative to sym
} Reloc;

#define RELOC_KIND_NONE  0  // no relocation, should not produce an entry
#define RELOC_KIND_DEBUG 1  // not a real relocation, a debug entry
#define RELOC_KIND_I32   2  // add a symbol to a 32 bit value
#define RELOC_KIND_AUGS  3  // relocation for AUGS
#define RELOC_KIND_AUGD  4  // relocation for AUGD

void PrintDataBlock(Flexbuf *f, Module *P, DataBlockOutFuncs *funcs, Flexbuf *relocs);
void PrintDataBlockForGas(Flexbuf *f, Module *P, int inlineAsm);
int  EnterVars(int kind, SymbolTable *stab, AST *symtype, AST *varlist, int startoffset, int isUnion, unsigned symFlags);

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

/* fix up function parameter types */
void FixupParameterTypes(Function *func);

/* process a function and perform basic transformations on it */
void ProcessOneFunc(Function *pf);

/* mark a function (and all functions it references) as used */
void MarkUsed(Function *f, const char *callerName);

/* check to see if a function is recursive */
void CheckRecursive(Function *f);

/* code for printing errors */
extern int gl_errors;
extern int gl_warnings_are_errors;
void ERROR(AST *, const char *msg, ...);
void WARNING(AST *, const char *msg, ...);
void ERROR_UNKNOWN_SYMBOL(AST *);
void NOTE(AST *, const char *msg, ...);
int DifferentLineNumbers(AST *, AST *);

/* this one is used by the lexer */
void SYNTAX_ERROR(const char *msg, ...);

/* also used by lexer and some other places */
void LANGUAGE_WARNING(int lang, AST *ast, const char *msg, ...);

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

/* detect coginit/cognew calls that are for spin methods, return pointer to method involved */
bool IsSpinCoginit(AST *body, Function **thefunc);

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

/* convert a class to an object type */
AST *ClassType(Module *P);

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
int GetClkFreqP1(Module *P, unsigned int *clkfreqptr, unsigned int *clkregptr);
int GetClkFreqP2(Module *P, unsigned int *clkfreqptr, unsigned int *clkregptr);

// some string utilities

// find the last directory separator (/ or, for windows, \)
const char *FindLastDirectoryChar(const char *name);

/* utility to create a new string by adding an extension to a base file name */
/* if the base string has an extension already, we remove it */
char *ReplaceExtension(const char *base, const char *ext);

/* like ReplaceExtension but adds the extension on */
char *AddExtension(const char *base, const char *ext);

/* add "basename" to the path portion of  "directory" to make a complete file name */
char *ReplaceDirectory(const char *basename, const char *directory);

// add a propeller checksum to a binary file
// if eepromSize is non-zero, create an EEPROM compatible file
// of the given size
int DoPropellerChecksum(const char *fname, size_t eepromSize);

// initialization functions
void Init();
void InitPreprocessor(const char *argv[]);
void SetPreprocessorLanguage(int language);

// perform various high level optimizations
void DoHighLevelOptimize(Module *P);

// perform common sub-expression elimination on a function
void PerformCSE(Module *P);
void PerformLoopOptimization(Module *P);

// simplify statments like a^=b to a = a^b
void SimplifyAssignments(AST **astptr);

extern unsigned int gl_hub_base;
#define P2_HUB_BASE gl_hub_base
#define P2_CONFIG_BASE 0x10   /* clkfreq and such go here */
/* we use the same layout as TAQOZ for config:
   0x10 == crystal frequency (not used by fastspin)
   0x14 == CPU frequency
   0x18 == CLKSET mode setting
   0x1c == baud rate
   0x20 - 0x30 (VGA settings (not used by fastspin)
   0x30 == SEND pointer
*/

/*
 * functions to initialize lexers
 */
void initSpinLexer(int flags);

/* The various language families */
#define LANG_SPIN_SPIN1  0x00
#define LANG_SPIN_SPIN2  0x01
#define IsSpinLang(lang) ((lang)>=LANG_SPIN_SPIN1 && (lang)<=LANG_SPIN_SPIN2)

#define LANG_BASIC_FBASIC 0x10
#define IsBasicLang(lang) ((lang) == LANG_BASIC_FBASIC)

#define LANG_CFAMILY_C     0x20
#define LANG_CFAMILY_CPP   0x21
#define IsCLang(lang) ((lang)>=LANG_CFAMILY_C && (lang)<=LANG_CFAMILY_CPP)

#define LANG_PYTHON_CIRCUS 0x30
#define IsPythonLang(lang) ((lang)==LANG_PYTHON_CIRCUS)

#define LANG_DEFAULT LANG_SPIN_SPIN1
#define LANG_ANY     (0xffff)

// language features
#define LangBoolIsOne(lang) (IsCLang(lang)||IsPythonLang(lang))
#define LangCaseSensitive(lang) (IsCLang(lang))
#define LangCaseInSensitive(lang) (!LangCaseSensitive(lang))

void InitGlobalModule(void);
Module *NewModule(const char *modulename, int language);
Module *ParseFile(const char *filename);

/* declare a single global variable */
void DeclareOneGlobalVar(Module *P, AST *ident, AST *typ, int inDat);

/* declare a single member variable of P */
void DeclareOneMemberVar(Module *P, AST *ident, AST *typ, int is_private);

/* declare a member variable of P if it does not already exist */
void MaybeDeclareMemberVar(Module *P, AST *ident, AST *typ, int is_private);

/* declare a member alias of P */
void DeclareMemberAlias(Module *P, AST *ident, AST *expr);

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

/* enter a local alias in a symbol table */
void EnterLocalAlias(SymbolTable *table, AST *globalName, const char *localName);

/* create a declaration list, or add a type name to currentTypes */
AST *MakeDeclarations(AST *decl, SymbolTable *table);

/* find the symbol containing a mask for implicit types, as defined by DEFINT and DEFSNG */
Symbol *GetCurImplicitTypes(void);

/* returns 1 if a block contains a label, 0 if not */
int BlockContainsLabel(AST *block);

/* create a "normalized" form of a file name that we can use for comparison */
char *NormalizePath(const char *path);

/* check types in an expression; returns the overall result of the expression */
AST *CheckTypes(AST *expr);
/* type conversion */
/* "kind" is AST_ASSIGN, AST_FUNCCALL, AST_RETURN to indicate reason for conversion */
AST *CoerceAssignTypes(AST *line, int kind, AST **astptr, AST *desttype, AST *srctype);

/* change parameter types as necessary for calling conventions; for example,
 * "large" values are passed as reference to data on the stack rather than
 * in registers
 */
void FixupParameters(Function *func);

/* add a subclass to a class */
void AddSubClass(Module *P, Module *subP);

/* declare all member variables in a module */
void DeclareMemberVariables(Module *);

/* declare some global variables; if inDat is true, put them in
 * DAT, otherwise make them member variables
 */
void DeclareTypedGlobalVariables(AST *ast, int inDat);

/* add a symbol for a label in the current function */
void AddSymbolForLabel(AST *ast);

/* transform a switch/case expression into a sequence of GOTOs */
AST *CreateSwitch(AST *expr, AST *stmt, const char *force_reason);

// check to see if module is top level
int IsTopLevel(Module *P);

// fetch top level module
Module *GetTopLevelModule(void);

// returns non-zero if a variable of type typ must go on the stack
int TypeGoesOnStack(AST *typ);

// declare a symbol together with a location of its definition
Symbol *AddSymbolPlaced(SymbolTable *table, const char *name, int type, void *val, const char *user_name, AST *def);

// parse an optimization string
// updates flags based on what we find
// returns 0 on failure to parse, 1 otherwise
int ParseOptimizeString(AST *lineNum, const char *str, int *flags);

// external vars
extern AST *basic_get_float;
extern AST *basic_get_string;
extern AST *basic_get_integer;
extern AST *basic_read_line;

extern AST *basic_print_float;
extern AST *basic_print_string;
extern AST *basic_print_integer;
extern AST *basic_print_unsigned;
extern AST *basic_print_char;
extern AST *basic_print_nl;
extern AST *basic_put;

#endif
