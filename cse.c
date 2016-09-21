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

void DumpCSE(CSESet *cse); // forward declaration

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
InitCSESet(CSESet *cse)
{
    int i;
    for (i = 0; i < CSE_HASH_SIZE; i++) {
        cse->list[i] = NULL;
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

// clear out all memory entries in a CSE set
static void
ClearMemoryCSESet(CSESet *cse)
{
    ClearCSESet(cse); // FIXME: we can be more liberal than this!
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

    if (modified->kind == AST_ARRAYREF) {
        modified = modified->left;
    }
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

AST *
ArrayBaseType(AST *var)
{
    Symbol *sym;
    AST *stype;
    if (var->kind == AST_MEMREF) {
        return var->left;
    }
    if (var->kind != AST_IDENTIFIER) {
        return NULL;
    }
    sym = LookupAstSymbol(var, "array reference");
    if (!sym) return NULL;
    switch(sym->type) {
    case SYM_LABEL:
        {
            Label *label = (Label *)sym->val;
            return label->type;
        }
    case SYM_VARIABLE:
    case SYM_LOCALVAR:
    case SYM_PARAMETER:
        stype = (AST *)sym->val;
        if (!stype) {
            ERROR(var, "illegal array reference");
            return NULL;
        }
        if (stype->kind != AST_ARRAYTYPE) {
            ERROR(var, "array reference to non-array");
            return NULL;
        }
        return stype->left;
    default:
        return NULL;
    }
}

// create a new CSESet entry for an expression
// "point" is where to add a new assignment
static void
AddToCSESet(CSESet *set, AST **point, AST *expr, unsigned exprHash)
{
    CSEEntry *entry = malloc(sizeof(*entry));
    unsigned idx = exprHash & (CSE_HASH_SIZE-1);

    if (expr->kind == AST_ARRAYREF && !ArrayBaseType(expr->left)) {
        // cannot figure out type of array
        return;
    }
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
        AST *origexpr = entry->expr;
        entry->replace = AstTempLocalVariable("_cse_");
        if (origexpr->kind == AST_ARRAYREF) {
            AST *reftype = ArrayBaseType(origexpr->left);
            if (!reftype) {
                ERROR(origexpr, "Internal error, array expression too complicated");
                return;
            }
            origexpr = NewAST(AST_ADDROF, origexpr, NULL);
            assign = AstAssign(T_ASSIGN, entry->replace, origexpr);
            assign = NewAST(AST_MEMREF, reftype, assign);
            entry->replace = NewAST(AST_MEMREF, reftype, entry->replace);
        } else {
            assign = AstAssign(T_ASSIGN, entry->replace, origexpr);
        }
        *entry->first = assign;
    }
    *astptr = entry->replace;
}

// flags:
//
#define CSE_NO_REPLACE 0x01 // do not perform CSE replacement
#define CSE_NO_ADD     0x02 // re-using old expressions in this one is OK, but do not add it

//
// perform CSE on a loop body
//
static unsigned doPerformCSE(AST **ptr, CSESet *cse, unsigned flags);

//
// "astptr" points to the loop statement itself
// "body" is the loop body
// "condition" is the loop test condition
// "update" is an optional loop update statement (like in a for loop)
//
static unsigned
loopCSE(AST **astptr, AST **body, AST **condition, AST **update, CSESet *cse, unsigned flags)
{
    CSESet bodycse;
    
    // remove any CSE expressions modified inside the loop
    doPerformCSE(body, cse, flags | CSE_NO_REPLACE);
    if (update) {
        doPerformCSE(update, cse, flags | CSE_NO_REPLACE);
    }
    // now do any CSE replacements still valid
    // (CSE_NO_ADD says not to create new ones inside the loop)
    doPerformCSE(condition, cse, flags | CSE_NO_ADD);
    doPerformCSE(body, cse, flags | CSE_NO_ADD);

    // OK, now CSE the body for repeats during each individual iteration
    // only bother doing this if we would be able to do unlimited CSE
    if (flags == 0) {
        InitCSESet(&bodycse);
        doPerformCSE(body, &bodycse, flags);

#if 0
        // now perhaps allow for hoisting loop invariants out
        // do this by running through the body again removing
        // modified expressions
        doPerformCSE(body, &bodycse, flags | CSE_NO_REPLACE);
        doPerformCSE(condition, &bodycse, flags | CSE_NO_REPLACE);

        // here's where we would hoist
        // make sure all items have a replacement (but add no new ones)
        // kind of clunky, probably want to do this differently
        doPerformCSE(body, &bodycse, flags | CSE_NO_ADD);
        doPerformCSE(condition, &bodycse, flags | CSE_NO_ADD);

        // bodycse contains the expressions that should be hoisted
        // to the start of the loop
        printf("bodycse=\n");
        DumpCSE(&bodycse);
#endif
    }
    return flags;
}

//
// walk through an AST and replace any
// common subexpressions

static unsigned
doPerformCSE(AST **astptr, CSESet *cse, unsigned flags)
{
    AST *ast = *astptr;
    CSEEntry *entry;
    unsigned hash;
    unsigned newflags = flags;
    
    if (!ast) return true;
    switch(ast->kind) {
    case AST_STMTLIST:
        while (ast) {
            (void)doPerformCSE(&ast->left, cse, flags);
            ast = ast->right;
        }
        return newflags;
    case AST_EXPRLIST:
        while (ast) {
            newflags |= doPerformCSE(&ast->left, cse, flags);
            ast = ast->right;
        }
        return newflags;
    case AST_ASSIGN:
        newflags |= doPerformCSE(&ast->right, cse, flags);
        newflags |= doPerformCSE(&ast->left, cse, flags);
        // now we have to invalidate any CSE involving the destination
        RemoveCSEUsing(cse, ast->left);
        return newflags;
    case AST_OPERATOR:
        // handle various special cases
        switch(ast->d.ival) {
        case T_OR:
        case T_AND:
            // may not actually execute both sides of this, so
            // do not add any new entries on the second half
            flags |= CSE_NO_ADD;
            break;
        case '?':
            // random number operator; cannot CSE this
            newflags |= CSE_NO_REPLACE;
            break;
        case T_INCREMENT:
        case T_DECREMENT:
            if (ast->left) {
                doPerformCSE(&ast->left, cse, flags);
                RemoveCSEUsing(cse, ast->left);
            }
            if (ast->right) {
                doPerformCSE(&ast->right, cse, flags | CSE_NO_REPLACE);
                RemoveCSEUsing(cse, ast->right);
            }
            newflags |= CSE_NO_REPLACE;
            return newflags;;
                
        default:
            break;
        }
        newflags |= doPerformCSE(&ast->left, cse, flags);
        newflags |= doPerformCSE(&ast->right, cse, flags);
        if (!(newflags & CSE_NO_REPLACE)) {
            hash = ASTHash(ast);
            if ( 0 != (entry = FindCSE(cse, ast, hash))) {
                ReplaceCSE(astptr, entry);
            } else if (!(newflags & CSE_NO_ADD)) {
                AddToCSESet(cse, astptr, ast, hash);
            }
        }
        return newflags;
    case AST_ARRAYREF:
        newflags |= doPerformCSE(&ast->right, cse, flags);
        newflags |= doPerformCSE(&ast->left, cse, flags);
        if (!(newflags & CSE_NO_REPLACE)) {
            hash = ASTHash(ast);
            if ( 0 != (entry = FindCSE(cse, ast, hash))) {
                ReplaceCSE(astptr, entry);
            } else if (!(newflags & CSE_NO_ADD)) {
                AddToCSESet(cse, astptr, ast, hash);
            }
        }
        return newflags;
    case AST_ADDROF:
    case AST_ABSADDROF:
        // for addressof, we cannot do CSE inside
        (void) doPerformCSE(&ast->left, cse, flags | CSE_NO_REPLACE);
        (void) doPerformCSE(&ast->right, cse, flags | CSE_NO_REPLACE);
        if (!(newflags & CSE_NO_REPLACE)) {
            hash = ASTHash(ast);
            if ( 0 != (entry = FindCSE(cse, ast, hash))) {
                ReplaceCSE(astptr, entry);
            } else if (!(newflags & CSE_NO_ADD)) {
                AddToCSESet(cse, astptr, ast, hash);
            }
        }
        return newflags;
    case AST_INTEGER:
    case AST_FLOAT:
    case AST_CONSTANT:
    case AST_STRING:
    case AST_RESULT:
    case AST_IDENTIFIER:
        return newflags;
    case AST_HWREG:
        return CSE_NO_REPLACE; // do not CSE expressions involving hardware
    case AST_COMMENTEDNODE:
    case AST_RETURN:
    case AST_THENELSE:
    case AST_CASEITEM:
    case AST_OTHER:
        (void) doPerformCSE(&ast->right, cse, flags);
        (void) doPerformCSE(&ast->left, cse, flags);
        return newflags;
    case AST_CONDRESULT:
        // do CSE on the expression
        (void) doPerformCSE(&ast->left, cse, flags);
        // invalidate CSE entries resulting from assignments, and
        // allow replacements using existing entries, but do not
        // create any new entries
        (void)doPerformCSE(&ast->right, cse, flags | CSE_NO_ADD);
        newflags |= CSE_NO_REPLACE; // this expression should not be CSE'd
        return newflags;
    case AST_IF:
    case AST_CASE:
        // do CSE on the expression
        (void) doPerformCSE(&ast->left, cse, flags);
        // invalidate CSE entries resulting from assignments, and
        // allow replacements using existing entries, but do not
        // create any new entries
        doPerformCSE(&ast->right, cse, flags | CSE_NO_ADD);
        return newflags;
    case AST_WHILE:
    case AST_DOWHILE:
        // invalidate CSE entries that are inside the loop
        // (ast->right is the body, ast->left is the condition)
        loopCSE(astptr, &ast->right, &ast->left, NULL, cse, flags);
        return newflags;
    case AST_FOR:
    case AST_FORATLEASTONCE:
        {
            AST **condtestptr;
            AST **stepstmtptr;
            AST **bodyptr;
            
            condtestptr = &ast->right;
            // process the initial statement
            doPerformCSE(&ast->left, cse, flags);

            stepstmtptr = &(*condtestptr)->right;
            bodyptr = &(*stepstmtptr)->right;
            condtestptr = &(*condtestptr)->left;
            stepstmtptr = &(*stepstmtptr)->left;
            // handle the body of the loop
            loopCSE(astptr, bodyptr, condtestptr, stepstmtptr, cse, flags);
        }
        return newflags;
    case AST_FUNCCALL:
    case AST_COGINIT:
        {
            AST *exprlist = ast->right;
            while (exprlist) {
                doPerformCSE(&exprlist->left, cse, flags);
                exprlist = exprlist->right;
            }
            // after the function call memory may be modified
            ClearMemoryCSESet(cse);
        }
        return newflags;
    case AST_CONSTREF:
    case AST_ROUND:
    case AST_TRUNC:
    case AST_TOFLOAT:
    case AST_SEQUENCE:
    case AST_ISBETWEEN:
        newflags |= doPerformCSE(&ast->left, cse, flags);
        newflags |= doPerformCSE(&ast->right, cse, flags);
        return newflags;
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
    
    if ((gl_optimize_flags & OPT_PERFORM_CSE) == 0)
        return;
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

//
// debug code
//
void
DumpCSEEntry(CSEEntry *entry)
{
    printf("Expr:\n");
    DumpAST(entry->expr);
    printf("Replace with:\n");
    DumpAST(entry->replace);
    printf("\n");
}

void
DumpCSE(CSESet *cse)
{
    int i;
    CSEEntry *entry;
    for (i = 0; i < CSE_HASH_SIZE; i++) {
        entry = cse->list[i];
        while (entry) {
            DumpCSEEntry(entry);
            entry = entry->next;
        }
    }
}
