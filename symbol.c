#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include "symbol.h"

/* do case insensitive comparisons */
#define STRCMP strcasecmp

/*
 * hash function
 */
static int
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
    /* symbol was not found, give up */
    /* it's up to our caller to look through a containing context */
    return NULL;
}

/*
 * find a symbol by offset
 * this is slow (we have to search the whole table) and
 * not guaranteed by us to be unique
 */
Symbol *
FindSymbolByOffset(SymbolTable *table, int offset)
{
    Symbol *sym;
    int hash;
    for (hash = 0; hash < SYMTABLE_HASH_SIZE; hash++) {
        sym = table->hash[hash];
        while (sym) {
            if (sym->offset == offset)
                return sym;
            sym = sym->next;
        }
    }
    return NULL;
}
/*
 * create a new symbol
 */
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

static int tmpvarnum = 1;
static int tmpvarmax = 99999;

/*
 * set the base and max for temporary variable names
 * returns the old base
 */
int
SetTempVariableBase(int base, int max)
{
    int oldbase = tmpvarnum;

    tmpvarnum = base;
    if (max > 0) {
        tmpvarmax = max;
    }
    return oldbase;
}

/*
 * create a temporary variable name
 */
char *
NewTemporaryVariable(const char *prefix)
{
    char *str;
    char *s;

    if (!prefix)
        prefix = "_tmp_";
    s = str = malloc(strlen(prefix)+16);
    if (!s) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    while (*prefix)
        *s++ = *prefix++;
    *s++ = '_';
    sprintf(s, "%04d", tmpvarnum);
    tmpvarnum++;
    if (tmpvarnum > tmpvarmax) {
        fprintf(stderr, "Temporary variable limit of %d exceeded", tmpvarmax);
        abort();
    }
    return str;
}
