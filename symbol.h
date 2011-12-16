#ifndef SYMBOL_H
#define SYMBOL_H

#include <stdint.h>

typedef struct symbol {
    struct symbol *next;  /* next in hash table */
    const char   *name;   /* name */
    int    type;          /* symbol type */
    void   *val;          /* symbol value */
} Symbol;

#define INTVAL(sym) ((intptr_t)((sym)->val))

#define SYM_UNKNOWN   0     /* unknown symbol */
#define SYM_HW_REG    1     /* hardware register */
#define SYM_RESERVED  2     /* reserved word */
#define SYM_CONSTANT  3     /* constant */

/*
 * symbol tables are basically just hash tables containing
 * all the symbols
 */

/* make this a power of two, please */
#define SYMTABLE_HASH_SIZE 256

typedef struct symtab {
    Symbol *hash[SYMTABLE_HASH_SIZE];
} SymbolTable;

Symbol *AddSymbol(SymbolTable *table, const char *name, int type, void *val);
Symbol *FindSymbol(SymbolTable *table, const char *name);

#endif
