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
    unsigned exprHash; // hash of "expr"
    unsigned flags;  // flags describing this expression
} CSEEntry;

typedef struct CSESet {
    // hash table of potential CSE replacements
    CSEEntry *list[CSE_HASH_SIZE];
    // list of pending CSE assignments
    AST *assignList;
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
    cse->assignList = NULL;
}

// clear out everything in a CSESet
static void
ClearCSESetFiltered(CSESet *cse, bool (*filter)(AST *expr))
{
    int i;
    CSEEntry *old, *cur;
    CSEEntry **curptr;
    // free the old stuff that matches "filter"
    
    for (i = 0; i < CSE_HASH_SIZE; i++) {
        curptr = &cse->list[i];
        cur = cse->list[i];
        while (cur) {
            if (filter(cur->expr)) {
                old = cur;
                cur = cur->next;
                *curptr = cur;
                free(old);
            } else {
                curptr = &cur->next;
                cur = cur->next;
            }
        }
    }
}

static bool
Always(AST *ast)
{
    return true;
}

static bool
UsesMemory(AST *ast) {
    if (ast == NULL)
        return false;
    switch(ast->kind) {
    case AST_OPERATOR:
        return UsesMemory(ast->left) || UsesMemory(ast->right);
        break;
    case AST_ARRAYREF:
    case AST_MEMREF:
        return true;
    case AST_IDENTIFIER:
    {
        Symbol *sym = LookupAstSymbol(ast, "memory reference check");
        if (!sym) return true; // assume it uses memory
        switch (sym->type) {
        case SYM_PARAMETER:
        case SYM_RESULT:
        case SYM_LOCALVAR:
            // whether these use memory depends on the
            // function configuration
            // for now punt and assume not
            return false;
        case SYM_TEMPVAR:
        case SYM_CONSTANT:
        case SYM_FUNCTION:
        case SYM_FLOAT_CONSTANT:
            return false;
        default:
            return true;
        }
    }
    case AST_ADDROF:
    case AST_ABSADDROF:
        return false;
    case AST_CONSTREF:
    case AST_INTEGER:
        return false;
    default:
        return true;
    }
}

static void
ClearCSESet(CSESet *cse)
{
    ClearCSESetFiltered(cse, Always);
}

// clear out all memory entries in a CSE set
static void
ClearMemoryCSESet(CSESet *cse)
{
    ClearCSESetFiltered(cse, UsesMemory);
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
bool
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
    const char *name = NULL;

    if (modified->kind == AST_ARRAYREF) {
        modified = modified->left;
    }
    switch(modified->kind) {
    case AST_IDENTIFIER:
        name = modified->d.string;
        break;
    case AST_MEMREF:
        ClearMemoryCSESet(set);
        return;
    default:
        ClearCSESet(set);
        return;
    }
    for (i = 0; i < CSE_HASH_SIZE; i++) {
        pCur = &set->list[i];
        for(;;) {
            cur = *pCur;
            if (!cur) break;
            if (name && ASTUsesName(cur->expr, name)) {
                *pCur = cur->next;
            } else {
                pCur = &cur->next;
            }
        }
    }
}

//
// place pending assigments into a statement list
//
static void
PlacePendingAssignments(AST *stmtlist, CSESet *cse)
{
    AST *sublist;
    AST *oldstmt;
    if (!cse->assignList) {
        return;
    }
    oldstmt = NewAST(AST_STMTLIST, stmtlist->left, NULL);
    sublist = AddToList(cse->assignList, oldstmt);
    stmtlist->left = sublist;
    cse->assignList = NULL;
}

//////////////////////////////
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
    case SYM_LOCALVAR:
    case SYM_PARAMETER:
    case SYM_TEMPVAR:
        // if this function uses registers for arrays,
        // give up
        if (!curfunc->localarray) {
            return NULL;
        }
        /* fall through */
    case SYM_VARIABLE:
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
// the new assignment to create is added to the CSE
// pending assignList list
// if "name" is non-NULL then it is used as the replacement
// name, otherwise a new temporary name is generated

static CSEEntry *
AddToCSESet(AST *name, CSESet *cse, AST *expr, unsigned exprHash, AST **replaceptr)
{
    CSEEntry *entry = malloc(sizeof(*entry));
    unsigned idx = exprHash & (CSE_HASH_SIZE-1);

    if (expr->kind == AST_ARRAYREF && !ArrayBaseType(expr->left)) {
        // cannot figure out type of array
        return NULL;
    }
    // do not add entries for some simple expressions
    if (expr->kind == AST_ARRAYREF &&
        IsConstExpr(expr->right))
    {
        return NULL;
    }
    
    entry->expr = expr;
    entry->replace = name;
    entry->flags = 0;
    entry->exprHash = exprHash;
    entry->next = cse->list[idx];
    cse->list[idx] = entry;
    if (!entry->replace) {
        AST *assign;
        AST *origexpr = entry->expr;
        entry->replace = AstTempLocalVariable("_cse_");
        if (origexpr->kind == AST_ARRAYREF) {
            AST *reftype = ArrayBaseType(origexpr->left);
            if (!reftype) {
                ERROR(origexpr, "Internal error, array expression too complicated");
                return NULL;
            }
            origexpr = NewAST(AST_ADDROF, origexpr, NULL);
            assign = AstAssign(T_ASSIGN, entry->replace, origexpr);
            entry->replace = NewAST(AST_ARRAYREF,
                                    NewAST(AST_MEMREF, reftype, entry->replace),
                                    AstInteger(0));
        } else {
            assign = AstAssign(T_ASSIGN, entry->replace, origexpr);
        }
        assign = NewAST(AST_STMTLIST, assign, NULL);
        cse->assignList = AddToList(cse->assignList, assign);
        *replaceptr = entry->replace;
    }
    return entry;
}

//
// replace an AST expression with a variable having the same value
//
static void
ReplaceCSE(AST **astptr, CSEEntry *entry)
{
    if (!entry) return; // sanity check
    *astptr = entry->replace;
}

// flags:
//
#define CSE_NO_REPLACE 0x01 // do not perform CSE replacement
#define CSE_NO_ADD     0x02 // re-using old expressions in this one is OK, but do not add it

//
// perform CSE on a loop body
//
static unsigned doPerformCSE(AST *stmtptr, AST **ast, CSESet *cse, unsigned flags, AST *name);

//
// perform CSE on a block which may be conditinally executed
// we can re-use any existing CSE definitions from the main set
// but new ones can be created only in a local set that doesn't
// get saved
//
static unsigned
blockCSE(AST *stmtptr, AST **block, CSESet *cse, unsigned flags)
{
    CSESet bodycse;
    unsigned newflags = flags;
    
    // use the existing set to do any replacements
    doPerformCSE(stmtptr, block, cse, flags | CSE_NO_ADD, NULL);

    if (flags == 0) {
        InitCSESet(&bodycse);
        doPerformCSE(NULL, block, &bodycse, flags, NULL);
        ClearCSESet(&bodycse);
    }
    return newflags;
}

//
// "astptr" points to the loop statement itself
// "body" is the loop body
// "condition" is the loop test condition
// "update" is an optional loop update statement (like in a for loop)
//
static unsigned
loopCSE(AST *stmtptr, AST **astptr, AST **body, AST **condition, AST **update, CSESet *cse, unsigned flags)
{
    CSESet bodycse;
    
    PlacePendingAssignments(stmtptr, cse);
    // remove any CSE expressions modified inside the loop
    doPerformCSE(stmtptr, body, cse, flags | CSE_NO_REPLACE, NULL);
    if (update) {
        doPerformCSE(stmtptr, update, cse, flags | CSE_NO_REPLACE, NULL);
    }
    // now do any CSE replacements still valid
    // (CSE_NO_ADD says not to create new ones inside the loop)
    doPerformCSE(stmtptr, condition, cse, flags | CSE_NO_ADD, NULL);
    doPerformCSE(stmtptr, body, cse, flags | CSE_NO_ADD, NULL);

    // OK, now CSE the body for repeats during each individual iteration
    // only bother doing this if we would be able to do unlimited CSE
    if (flags == 0) {
        InitCSESet(&bodycse);
        doPerformCSE(NULL, body, &bodycse, flags, NULL);
        ClearCSESet(&bodycse);
    }
    return flags;
}

//
// walk through an AST and replace any
// common subexpressions
// "stmtptr" contains a pointer to the current
// statement being processed
// "astptr" points to where the AST might be replaced
// cse is the CSESet holding potential replacements
// flags are various flags governing replacements
// "name", if non-zero, is the name of a variable that should
// be used for replacements
//
static unsigned
doPerformCSE(AST *stmtptr, AST **astptr, CSESet *cse, unsigned flags, AST *name)
{
    AST *ast = *astptr;
    CSEEntry *entry;
    unsigned hash;
    unsigned newflags = flags;
    
    if (!ast) return true;
    switch(ast->kind) {
    case AST_STMTLIST:
        while (ast) {
            stmtptr = ast;
            (void)doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
            PlacePendingAssignments(stmtptr, cse);
            ast = ast->right;
        }
        return newflags;
    case AST_EXPRLIST:
        while (ast) {
            newflags |= doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
            ast = ast->right;
        }
        return newflags;
    case AST_ASSIGN:
        if (!name && ast->left->kind == AST_IDENTIFIER) {
            name = ast->left;
        }
        newflags |= doPerformCSE(stmtptr, &ast->right, cse, flags, name);
        newflags |= doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
        // now we have to invalidate any CSE involving the destination
        RemoveCSEUsing(cse, ast->left);
	// if the right hand side is a CSE-able expression, and the left
	// hand side is a local variable, use that as the CSE replacement
	// instead of creating a new variable
	hash = ASTHash(ast->right);
	entry = FindCSE(cse, ast->right, hash);
	if (entry && !entry->replace && ast->left->kind == AST_IDENTIFIER) {
	  Symbol *sym = LookupSymbol(ast->left->d.string);
	  if (sym &&
	      (sym->type == SYM_PARAMETER || sym->type == SYM_LOCALVAR
	       || sym->type == SYM_RESULT || sym->type == SYM_TEMPVAR)
	      )
	    {
	      entry->replace = ast->left;
	    }
	}
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
        case '<':
        case '>':
        case T_LE:
        case T_GE:
        case T_EQ:
        case T_NE:
            // do not add CSE entries for boolean operators,
            // it generally won't help code generation and
            // actually hurts it a lot of times
            flags |= CSE_NO_ADD;
            break;
        case '?':
            // random number operator; cannot CSE this
            newflags |= CSE_NO_REPLACE;
            break;
        case T_INCREMENT:
        case T_DECREMENT:
            if (ast->left) {
                doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
                RemoveCSEUsing(cse, ast->left);
            }
            if (ast->right) {
                doPerformCSE(stmtptr, &ast->right, cse, flags | CSE_NO_REPLACE, NULL);
                RemoveCSEUsing(cse, ast->right);
            }
            newflags |= CSE_NO_REPLACE;
            return newflags;;
                
        default:
            break;
        }
        newflags |= doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
        newflags |= doPerformCSE(stmtptr, &ast->right, cse, flags, NULL);
        if (!(newflags & CSE_NO_REPLACE)) {
            hash = ASTHash(ast);
            if ( 0 != (entry = FindCSE(cse, ast, hash))) {
                ReplaceCSE(astptr, entry);
            } else if (!(newflags & CSE_NO_ADD)) {
                AddToCSESet(name, cse, ast, hash, astptr);
            }
        }
        return newflags;
    case AST_ARRAYREF:
        newflags |= doPerformCSE(stmtptr, &ast->right, cse, flags, NULL);
        newflags |= doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
        if (!(newflags & CSE_NO_REPLACE)) {
            hash = ASTHash(ast);
            if ( 0 != (entry = FindCSE(cse, ast, hash))) {
                ReplaceCSE(astptr, entry);
            } else if (!(newflags & CSE_NO_ADD)) {
                AddToCSESet(NULL, cse, ast, hash, astptr);
            }
        }
        return newflags;
    case AST_MEMREF:
        // left side of ast is just the type, no need to do CSE on it
        newflags |= doPerformCSE(stmtptr, &ast->right, cse, flags, NULL);
        /* do not try to add this to CSE, but we can use the memref in a CSE */
        return newflags;
    case AST_ADDROF:
    case AST_ABSADDROF:
        // for addressof, we cannot do CSE inside
        (void) doPerformCSE(stmtptr, &ast->left, cse, flags | CSE_NO_REPLACE, NULL);
        (void) doPerformCSE(stmtptr, &ast->right, cse, flags | CSE_NO_REPLACE, NULL);
        if (!(newflags & CSE_NO_REPLACE)) {
            hash = ASTHash(ast);
            if ( 0 != (entry = FindCSE(cse, ast, hash))) {
                ReplaceCSE(astptr, entry);
            } else if (!(newflags & CSE_NO_ADD)) {
                AddToCSESet(name, cse, ast, hash, astptr);
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
    case AST_COMMENT:
    case AST_COMMENTEDNODE:
    case AST_RETURN:
    case AST_THENELSE:
    case AST_CASEITEM:
    case AST_OTHER:
        (void) doPerformCSE(stmtptr, &ast->right, cse, flags, NULL);
        (void) doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
        return newflags;
    case AST_CONDRESULT:
        // do CSE on the expression
        (void) doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
        // invalidate CSE entries resulting from assignments, and
        // allow replacements using existing entries, but do not
        // create any new entries
        (void)doPerformCSE(stmtptr, &ast->right, cse, flags | CSE_NO_ADD, NULL);
        newflags |= CSE_NO_REPLACE; // this expression should not be CSE'd
        return newflags;
    case AST_IF:
    {
        // do CSE on the expression
        // NOTE: pulling expressions out can make it harder for
        // later optimizations to eliminate compares, so don't
        // do that; but do re-use any existing CSE entries
        (void) doPerformCSE(stmtptr, &ast->left, cse, flags | CSE_NO_ADD, NULL);
        PlacePendingAssignments(stmtptr, cse);
        // ast->right should be a THENELSE
        ast = ast->right;
        while (ast->kind == AST_COMMENTEDNODE) {
            ast = ast->left;
        }
        if (ast->kind != AST_THENELSE) {
            ERROR(ast, "Expecting THENELSE block");
            return newflags;
        }
        blockCSE(stmtptr, &ast->left, cse, flags);
        blockCSE(stmtptr, &ast->right, cse, flags);
        return newflags;
    }
    case AST_CASE:
        // do CSE on the expression
        // NOTE: pulling expressions out can make it harder for
        // later optimizations to eliminate compares, so don't
        // do that; but do re-use any existing CSE entries
        (void) doPerformCSE(stmtptr, &ast->left, cse, flags | CSE_NO_ADD, NULL);
        PlacePendingAssignments(stmtptr, cse);
        // invalidate CSE entries resulting from assignments, and
        // allow replacements using existing entries, but do not
        // create any new entries
        doPerformCSE(stmtptr, &ast->right, cse, flags | CSE_NO_ADD, NULL);
        return newflags;
    case AST_WHILE:
    case AST_DOWHILE:
        // invalidate CSE entries that are inside the loop
        // (ast->right is the body, ast->left is the condition)
        loopCSE(stmtptr, astptr, &ast->right, &ast->left, NULL, cse, flags);
        return newflags;
    case AST_FOR:
    case AST_FORATLEASTONCE:
        {
            AST **condtestptr;
            AST **stepstmtptr;
            AST **bodyptr;
            
            condtestptr = &ast->right;
            // process the initial statement
            doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);

            stepstmtptr = &(*condtestptr)->right;
            bodyptr = &(*stepstmtptr)->right;
            condtestptr = &(*condtestptr)->left;
            stepstmtptr = &(*stepstmtptr)->left;
            // handle the body of the loop
            loopCSE(stmtptr, astptr, bodyptr, condtestptr, stepstmtptr, cse, flags);
        }
        return newflags;
    case AST_FUNCCALL:
    case AST_COGINIT:
        {
            AST *exprlist = ast->right;
            while (exprlist) {
                doPerformCSE(stmtptr, &exprlist->left, cse, flags, NULL);
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
    case AST_ISBETWEEN:
        newflags |= doPerformCSE(stmtptr, &ast->left, cse, flags, NULL);
        newflags |= doPerformCSE(stmtptr, &ast->right, cse, flags, NULL);
        return newflags;
    case AST_SEQUENCE:
        // processing an AST_SEQUENCE is problematic because it might have multiple
        // assignments within it, and we want to place the created CSE assignments
        // at the beginning of the statement
        // we can re-use any existing CSE statements within the sequence, but
        // do not create any new ones
        newflags |= doPerformCSE(stmtptr, &ast->left, cse, flags | CSE_NO_ADD, NULL);
        newflags |= doPerformCSE(stmtptr, &ast->right, cse, flags | CSE_NO_ADD, NULL);
        return newflags;
    default:
        doPerformCSE(stmtptr, &ast->left, cse, flags | CSE_NO_REPLACE, NULL);
        doPerformCSE(stmtptr, &ast->right, cse, flags | CSE_NO_REPLACE, NULL);
        ClearMemoryCSESet(cse);
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
        doPerformCSE(NULL, &func->body, &cse, 0, NULL);
        ClearCSESet(&cse);
    }
    curfunc = savefunc;
    current = savecur;

    PerformLoopOptimization(Q);
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
