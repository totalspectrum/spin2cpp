#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "symbol.h"

/* do case insensitive comparisons */
#define STRCMP strcasecmp

/*
 * hash function
 */
int
hashfunc(const char *str)
{
    unsigned hash = 987654321;
    unsigned c;

    while (*str) {
        c = *str++;
        c = toupper(c);
        hash = hash * 65537;
        hash = hash ^ c;
    }
    return hash % SYMTABLE_HASH_SIZE;
}

/* find a symbol in the table */

Symbol *
FindSymbol(SymbolTable *table, const char *name)
{
    int hash;
    Symbol *sym;

    hash = hashfunc(name);
    sym = table->hash[hash];
    while (sym) {
        if (!STRCMP(sym->name, name)) {
            return sym;
        }
        sym = sym->next;
    }
    return NULL;
}

Symbol *
NewSymbol(void)
{
    Symbol *sym = calloc(1, sizeof(*sym));
    return sym;
}

/*
 * add a symbol to the table
 * returns NULL if there was a conflict
 */
Symbol *
AddSymbol(SymbolTable *table, const char *name, int type, void *val)
{
    int hash;
    Symbol *sym;

    hash = hashfunc(name);
    sym = table->hash[hash];
    while (sym) {
        if (!STRCMP(sym->name, name)) {
            return NULL;
        }
        sym = sym->next;
    }

    sym = NewSymbol();
    sym->name = name;
    sym->type = type;
    sym->val = val;
    sym->next = table->hash[hash];
    table->hash[hash] = sym;
    return sym;
}
