/*
 * Spin to C/C++ converter
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for Common Subexpression Elimination
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

#define CSE_HASH_SIZE 32  /* make this a power of two */

// a single possible common subexpression replacement
typedef struct CSEEntry {
    struct CSEEntry *next;
    AST *expr;       // the expression to replace
    AST *replace; // the symbol to replace it with, or NULL if no substitute yet
    AST **first;     // pointer to where the expression first occurs
    unsigned exprHash; // hash of "expr"
    unsigned flags;  // flags describing this expression
} CSEEntry;

typedef struct CSESet {
    CSEEntry *list[CSE_HASH_SIZE];
} CSESet;

//
// hash an AST tree
//
static unsigned
ASTHash(AST *ast)
{
    unsigned hash = 0;
    if (!ast) {
        return hash;
    }
    switch (ast->kind) {
    case AST_IDENTIFIER:
    case AST_STRING:
        hash = SymbolHash(ast->d.string);
        break;
    case AST_INTEGER:
    case AST_FLOAT:
    case AST_OPERATOR:
        hash = ast->d.ival;
        break;
    default:
        break;
    }
    hash += ast->kind + (hash<<2) + 131*ASTHash(ast->left) + 65537*ASTHash(ast->right);
    return hash;
}

// initialize a CSESet to empty
static void
InitCSESet(CSESet *set)
{
    int i;
    for (i = 0; i < CSE_HASH_SIZE; i++) {
        set->list[i] = NULL;
    }
}

// clear out everything in a CSESet
static void
ClearCSESet(CSESet *set)
{
    int i;
    CSEEntry *old, *cur;
    // free the old stuff
    for (i = 0; i < CSE_HASH_SIZE; i++) {
        cur = set->list[i];
        set->list[i] = NULL;
        while (cur) {
            old = cur;
            cur = cur->next;
            free(old);
        }
    }
}

// find a CSESet entry for an expression, if one exists
static CSEEntry *
FindCSE(CSESet *set, AST *expr, unsigned exprHash)
{
    int idx = exprHash & (CSE_HASH_SIZE-1);
    CSEEntry *ptr = set->list[idx];
    while (ptr) {
        if (ptr->exprHash == exprHash && AstMatch(ptr->expr, expr)) {
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

// check to see if an AST involves a particular name
static bool
ASTUsesName(AST *expr, const char *name)
{
    if (!expr) return false;
    switch(expr->kind) {
    case AST_IDENTIFIER:
        return strcasecmp(expr->d.string, name) == 0;
    default:
        return ASTUsesName(expr->left, name) || ASTUsesName(expr->right, name);
    }
}

// remove any CSEEntries that depend upon "modified"
static void
RemoveCSEUsing(CSESet *set, AST *modified)
{
    int i;
    CSEEntry **pCur;
    CSEEntry *cur;
    const char *name;

    switch(modified->kind) {
    case AST_IDENTIFIER:
        name = modified->d.string;
        break;
    default:
        ClearCSESet(set);
        return;
    }
    for (i = 0; i < CSE_HASH_SIZE; i++) {
        pCur = &set->list[i];
        for(;;) {
            cur = *pCur;
            if (!cur) break;
            if (ASTUsesName(cur->expr, name)) {
                *pCur = cur->next;
            } else {
                pCur = &cur->next;
            }
        }
    }
}

// create a new CSESet entry for an expression
// "point" is where to add a new assignment
static void
AddToCSESet(CSESet *set, AST **point, AST *expr, unsigned exprHash)
{
    CSEEntry *entry = malloc(sizeof(*entry));
    unsigned idx = exprHash & (CSE_HASH_SIZE-1);
    
    entry->expr = expr;
    entry->replace = NULL;
    entry->first = point;
    entry->flags = 0;
    entry->exprHash = exprHash;
    entry->next = set->list[idx];
    set->list[idx] = entry;
}

//
// replace an AST expression with a variable having the same value
//
static void
ReplaceCSE(AST **astptr, CSEEntry *entry)
{
    if (!entry->replace) {
        AST *assign;
        entry->replace = AstTempLocalVariable("_cse_");
        assign = AstAssign(T_ASSIGN, entry->replace, entry->expr);
        *entry->first = assign;
    }
    *astptr = entry->replace;
}

//
// walk through an AST and replace any
// common subexpressions
// flags:
//
#define CSE_NO_REPLACE 0x01 // do not perform CSE replacement

static unsigned
doPerformCSE(AST **astptr, CSESet *cse, unsigned flags)
{
    AST *ast = *astptr;
    CSEEntry *entry;
    unsigned hash;
    
    if (!ast) return true;
    switch(ast->kind) {
    case AST_STMTLIST:
        while (ast) {
            (void)doPerformCSE(&ast->left, cse, flags);
            ast = ast->right;
        }
        return flags;
    case AST_ASSIGN:
        (void)doPerformCSE(&ast->right, cse, flags);
        (void)doPerformCSE(&ast->left, cse, flags);
        // now we have to invalidate any CSE involving the right hand side
        RemoveCSEUsing(cse, ast->left);
        return flags;
    case AST_OPERATOR:
        flags |= doPerformCSE(&ast->left, cse, flags);
        flags |= doPerformCSE(&ast->right, cse, flags);
        if (!(flags & CSE_NO_REPLACE)) {
            hash = ASTHash(ast);
            if ( 0 != (entry = FindCSE(cse, ast, hash))) {
                ReplaceCSE(astptr, entry);
            } else {
                AddToCSESet(cse, astptr, ast, hash);
            }
        }
        return flags;
    case AST_INTEGER:
    case AST_FLOAT:
    case AST_CONSTANT:
    case AST_STRING:
    case AST_RESULT:
    case AST_IDENTIFIER:
        return flags;
    case AST_HWREG:
        return CSE_NO_REPLACE; // do not CSE expressions involving hardware
    case AST_CONSTREF:
    case AST_ROUND:
    case AST_TRUNC:
    case AST_TOFLOAT:
    case AST_CONDRESULT:
    case AST_ISBETWEEN:
    case AST_ADDROF:
    case AST_ABSADDROF:
        flags |= doPerformCSE(&ast->right, cse, flags);
        flags |= doPerformCSE(&ast->left, cse, flags);
        return flags;
    case AST_COMMENTEDNODE:
    case AST_RETURN:
        (void) doPerformCSE(&ast->right, cse, flags);
        (void) doPerformCSE(&ast->left, cse, flags);
        return flags;
    case AST_IF:
    case AST_CASE:
        // do CSE on the expression
        (void) doPerformCSE(&ast->left, cse, flags);
        // invalidate CSE entries resulting from assignments, but do
        // not otherwise perform CSE
        doPerformCSE(&ast->right, cse, flags | CSE_NO_REPLACE);
        return flags;
    case AST_WHILE:
    case AST_DOWHILE:
        (void)doPerformCSE(&ast->left, cse, flags);
        // invalidate CSE entries that are inside the loop
        doPerformCSE(&ast->right, cse, flags | CSE_NO_REPLACE);
        return flags;
    default:
        doPerformCSE(&ast->left, cse, flags | CSE_NO_REPLACE);
        doPerformCSE(&ast->right, cse, flags | CSE_NO_REPLACE);
        return CSE_NO_REPLACE; // AST not set up for processing yet
    }
}

void
PerformCSE(Module *Q)
{
    CSESet cse;
    Module *savecur = current;
    Function *func;
    Function *savefunc = curfunc;
    
    InitCSESet(&cse);
    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;
        doPerformCSE(&func->body, &cse, 0);
        ClearCSESet(&cse);
    }
    curfunc = savefunc;
    current = savecur;
}
