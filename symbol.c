#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include "symbol.h"
#include "util/util.h"

extern int gl_caseSensitive;

/*
 * hash function
 */
unsigned
RawSymbolHash(const char *str)
{
    unsigned hash = 987654321;
    unsigned c;

    while (*str) {
        c = (*str++) & ~0x20; // ignore case distinction
        hash = hash * 65537;
        hash = hash ^ c;
    }
    return hash;
}

unsigned
SymbolHash(const char *str)
{
    unsigned hash = RawSymbolHash(str);
    return hash % SYMTABLE_HASH_SIZE;
}

/* find a symbol in the table */
//extern inline Symbol *FindSymbol(SymbolTable *table, const char *name);

Symbol *
FindSymbolEx(SymbolTable *table, const char *name,int forceCaseSens)
{
    unsigned hash;
    Symbol *sym;
    int nocase = ((table->flags & SYMTAB_FLAG_NOCASE) != 0) && !gl_caseSensitive;
    if (forceCaseSens) nocase = forceCaseSens < 0; // 1 forces case sensitive, -1 forces case insensitive
    hash = SymbolHash(name);
    sym = table->hash[hash];

    if (nocase) {
        while (sym) {
            if (!strcasecmp(sym->our_name, name)) {
                return sym;
            }
            sym = sym->next;
        }
    } else {
        while (sym) {
            if (!strcmp(sym->our_name, name)) {
                return sym;
            }
            sym = sym->next;
        }
    }
    /* symbol was not found, give up */
    /* it's up to our caller to look through a containing context */
    return NULL;
}

//
// like FindSymbol, but goes back through previous contexts too
//
Symbol *
FindSymbolInContext(SymbolTable *table, const char *name)
{
    Symbol *sym;
    if (!table) return NULL;
    sym = FindSymbolEx(table, name, 0);
    if (sym) return sym;
    return FindSymbolInContext(table->next, name);
}

/* find a symbol in a table or in any of its links */
/* code to find a symbol */
static Symbol *
doLookupSymbolInTable(SymbolTable *table, const char *name, int level)
{
    Symbol *sym = NULL;
    if (!table) return NULL;
    sym = FindSymbol(table, name);
    if (!sym) {
        if (table->next) {
           sym = doLookupSymbolInTable(table->next, name, level);
	}
    }
    if (sym && IsAlias(sym)) {
        // have to look it up again
        Symbol *alias;
        level++;
        if (level > 32) {
            //ERROR(NULL, "recursive definition for symbol %s", name);
            return NULL;
        }
        alias = doLookupSymbolInTable(table, (const char *)sym->v.ptr, level);
        if (alias) {
            return alias;
        } else {
            return sym;
        }
    }
    return sym;
}

/* find a symbol in a table or in any of its links */
/* code to find a symbol */
Symbol *
LookupSymbolInTable(SymbolTable *table, const char *name)
{
    return doLookupSymbolInTable(table, name, 0);
}

/*
 * find a symbol by offset
 * this is slow (we have to search the whole table) and
 * not guaranteed by us to be unique
 * Also note: if "origsym" is non-NULL then it's a pointer to
 * the last symbol we tried to find, and it's used to cycle
 * through RESULT, PARAMETER, LOCALS as Spin needs
 */
Symbol *
FindSymbolByOffsetAndKind(SymbolTable *table, int offset, int kind)
{
    Symbol *sym = NULL;
    int hash;
    for (hash = 0; hash < SYMTABLE_HASH_SIZE; hash++) {
        sym = table->hash[hash];
        while (sym) {
            if (sym->offset == offset && sym->kind == kind)
            {
                return sym;
            }
            sym = sym->next;
        }
    }
    /* could not find it */
    if (kind == SYM_RESULT) {
        /* look for first parameter */
        kind = SYM_PARAMETER;
        sym = FindSymbolByOffsetAndKind(table, 0, SYM_PARAMETER);
    }
    if (!sym && kind == SYM_PARAMETER) {
        sym = FindSymbolByOffsetAndKind(table, 0, SYM_LOCALVAR);
    }
    return sym;
}

/*
 * iterate over all symbols in a table
 */
void
IterateOverSymbols(SymbolTable *table, SymbolFunc func, void *arg)
{
    Symbol *sym;
    int more;
    for (sym = table->i_first; sym; sym = sym->i_next) {
        more = func(sym, arg);
        if (!more) return;
    }
}

/*
 * create a new symbol
 */
Symbol *
NewSymbol(void)
{
    Symbol *sym = (Symbol *)calloc(1, sizeof(*sym));
    return sym;
}

/*
 * add a symbol to the table
 * returns NULL if there was a conflict
 */
Symbol *
AddSymbol(SymbolTable *table, const char *name, int type, void *val, const char *user_name)
{
    unsigned hash;
    Symbol *sym;
    int nocase = ((table->flags & SYMTAB_FLAG_NOCASE) != 0) && !gl_caseSensitive;
    
    hash = SymbolHash(name);
    sym = table->hash[hash];
    if (nocase) {
        while (sym) {
            if (!strcasecmp(sym->our_name, name)) {
                if (sym->kind == SYM_WEAK_ALIAS) {
                    // it's OK to override aliases
                    break;
                }
                return NULL;
            }
            sym = sym->next;
        }
    } else {
        while (sym) {
            if (!strcmp(sym->our_name, name)) {
                if (sym->kind == SYM_WEAK_ALIAS) {
                    // it's OK to override aliases
                    break;
                }
                return NULL;
            }
            sym = sym->next;
        }
    }

    if (sym) {
        // it's OK for us to override a weak alias
        if (sym->kind != SYM_WEAK_ALIAS) {
            fprintf(stderr, "Duplicate definition for symbol `%s'\n", user_name);
        }
    }
    
    sym = NewSymbol();
    sym->next = table->hash[hash];
    table->hash[hash] = sym;
    // now link into global list
    if (table->i_last) {
        table->i_last->i_next = sym;
        table->i_last = sym;
    } else {
        table->i_last = table->i_first = sym;
    }
    sym->i_next = 0;

    sym->our_name = name;
    sym->user_name = user_name ? user_name : name;
    sym->kind = (Symtype)type;
    sym->v.ptr = val;
    sym->module = 0;
    
    return sym;
}

Symbol *
AddInternalSymbol(SymbolTable *table, const char *name, int type, void *val, const char *user_name)
{
    Symbol *sym;
    sym = AddSymbol(table, name, type, val, user_name);
    if (sym) {
        sym->flags |= SYMF_INTERNAL;
    }
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
NewTemporaryVariable(const char *prefix, int *counter)
{
    char *str;
    char buf[32];
    int countval;
    
    if (!prefix) {
        prefix = "_tmp_";
    }
    if (!counter) {
        counter = &tmpvarnum;
    }
    countval = *counter;
    sprintf(buf, "_%04d", countval);
    str = strdupcat(prefix, buf);
    countval++;
    if (countval > tmpvarmax) {
        fprintf(stderr, "Temporary variable limit of %d exceeded", tmpvarmax);
        abort();
    }
    *counter = countval;
    return str;
}

/*
 * debug: dump all symbols
 */
static const char *
safe_str(const char *s)
{
    if (!s) return "<null>";
    return s;
}

static int
PrintSymbol(Symbol *sym, void *arg)
{
    printf("%s (%s)\n", safe_str(sym->user_name), safe_str(sym->our_name));
    return 1;
}

void
DumpSymbolTable(SymbolTable *table)
{
    IterateOverSymbols(table, PrintSymbol, NULL);
}

SymbolTable *
GetNamespace(SymbolTable *orig, const char *name)
{
    SymbolTable *p;
    Symbol *sym;

    sym = LookupSymbolInTable(orig, name);
    if (sym && sym->kind == SYM_NAMESPACE) {
        p = (SymbolTable *)sym->v.ptr;
    } else {
        p = (SymbolTable *)calloc(1, sizeof(SymbolTable));
        p->flags = orig->flags;
        p->next = NULL;
        sym = AddSymbol(orig, name, SYM_NAMESPACE, p, name);
    }
    return p;
}
