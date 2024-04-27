#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdint.h>


typedef enum symtype {
    SYM_UNKNOWN = 0,     /* unknown symbol */
    SYM_HW_REG,          /* hardware register */
    SYM_RESERVED,        /* reserved word */
    SYM_CONSTANT,        /* integer constant */
    SYM_VARIABLE,        /* member variable */
    SYM_INSTR,           /* PASM instruction */
    SYM_HWREG,           /* hardware register */
    SYM_FUNCTION,        /* function */
    SYM_LABEL,           /* dat block label */
    SYM_INSTRMODIFIER,   /* effect or condition */
    SYM_BUILTIN,         /* builtin symbol */
    SYM_NAME,            /* a plain identifier */
    SYM_PARAMETER,       /* function parameter */
    SYM_FLOAT_CONSTANT,  /* floating point constant */
    SYM_RESULT,          /* result expression */
    SYM_LOCALVAR,        /* local variable in function */
    SYM_TEMPVAR,         /* temporary variable in function */
    SYM_LOCALLABEL,      /* label in inline assembly or function */
    SYM_TYPEDEF,         /* a type definition */
    SYM_CLOSURE,         /* a function's closure object */
    SYM_WEAK_ALIAS,      /* an alias for another symbol; may be overwritten */
    SYM_FILE,            /* name of a file included in the object */
    SYM_REDEF,           /* name redefinition, used for implementing static */
    SYM_ALIAS,           /* makes a name an alias for an expression */
    SYM_TEMPLATE,        /* points to a function or class template */
} Symtype;

#define IsAlias(sym) ((sym)->kind == SYM_WEAK_ALIAS)

typedef union {
    void *ptr;   /* a generic pointer */
    int64_t i;   /* an integer value */
} SymbolVal;

//
// Symbols have two names:
// user_name is the name that should be used for error messages and the like
// our_name is an internal name that we use for generating code
//
typedef struct symbol {
    struct symbol *next;  /* next in hash table */
    const char   *user_name;   /* name given by the user */
    const char   *our_name;    /* internal compiler name */
    Symtype       kind;   /* kind of symbol */
    SymbolVal     v;      /* symbol value */
    int           flags;  /* various flags */
    int           offset;  /* extra value recording symbol order within a function */
    /* NOTE: offset is overloaded to record language version for SYM_RESERVED */
    void         *module;  /* module info */
    void         *def;     /* info about original definition */
    struct symbol *i_next; /* next in overal symbol list (for iteration) */
} Symbol;

/* symbol flags */
#define SYMF_GLOBAL   0x01  /* used for some special system globals */
#define SYMF_PRIVATE  0x02  /* symbol should not be used from other modules */
#define SYMF_INTERNAL 0x04  /* symbol is created by flexspin itself, should not be shown in listings */
#define SYMF_NOALLOC  0x08  /* symbol should not be allocated */
#define SYMF_ADDRESSABLE 0x10 /* symbol has its address taken and therefore must exist in memory */

#define INTVAL(sym) ((sym)->v.i)


/*
 * symbol tables are basically just hash tables containing
 * all the symbols
 * they can be linked together; the "next" pointer points at the
 * next symbol table to search for if the symbol is not
 * found here. typically the search would go:
 *  function symbols -> module symbols -> global symbols
 */

/* make this a power of two, please */
#define SYMTABLE_HASH_SIZE 128

typedef struct symtab {
    Symbol *hash[SYMTABLE_HASH_SIZE];
    struct symtab *next;
    unsigned flags;
    Symbol *i_first;  // for iterating over symbols
    Symbol *i_last;
} SymbolTable;
#define SYMTAB_FLAG_NOCASE 0x01  /* do case insensitive comparisons */

unsigned RawSymbolHash(const char *str);
unsigned SymbolHash(const char *str);
Symbol *AddSymbol(SymbolTable *table, const char *name, int type, void *val, const char *user_name);
Symbol *FindSymbolEx(SymbolTable *table, const char *name,int forceCaseSens);
Symbol *FindSymbolByOffsetAndKind(SymbolTable *table, int offset, int kind);
Symbol *LookupSymbolInTable(SymbolTable *table, const char *name);

#define FindSymbol(t, n) FindSymbolEx( (t), (n), 0)
Symbol *FindSymbolInContext(SymbolTable *table, const char *name);

/* like AddSymbol, but sets the SYMF_INTERNAL flag */
Symbol *AddInternalSymbol(SymbolTable *table, const char *name, int type, void *val, const char *user_name);

/* create a new temporary variable */
/* counter is an optional pointer to the counter to increment */
char *NewTemporaryVariable(const char *prefix, int *counter);

/* set the number to use in temporary variables, and the max allowed */
/* if max <= 0 then the max is left alone */
/* returns the old base */
int SetTempVariableBase(int base, int max);

/*
 * iterate over all symbols in a table
 * "func" returns nonzero if we should continue, 0 if we are done
 */
typedef int (*SymbolFunc)(Symbol *sym, void *arg);

void  IterateOverSymbols(SymbolTable *table, SymbolFunc func, void *arg);    
#endif
