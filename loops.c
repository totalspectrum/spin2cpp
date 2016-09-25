/*
 * Spin to C/C++ converter
 * Copyright 2011-2016 Total Spectrum Software Inc.
 * See the file COPYING for terms of use
 *
 * code for handling loops
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spinc.h"

#define LVFLAG_CONDITIONAL 0x01 /* assignment is conditional */
#define LVFLAG_NESTED      0x02 /* assignment is nested in a loop */
#define LVFLAG_LOOPDEPEND  0x04 /* assignment value is loop dependent */
#define LVFLAG_VARYMASK    0xff /* any of these bits set means assignment depends on loop */

//
// loop value table
// this keeps track of state of various variables
// if the variable has only been assigned once, then it
// will have an entry in the table with its initial value
// if it has been assigned multiple times, then it
// will have an entry saying that; in this case, if the second
// assignment was a simple one like i := i+1 we mark the entry
//
typedef struct LoopValueEntry {
    struct LoopValueEntry *next;
    AST *name;  // name of the variable
    AST *value; // last assignment (only really useful if hits == 1)
    AST *parent; // parent of the last assignment statement
    unsigned flags; // info about the assignment
    int hits;   // number of times we see assignments to this variable
} LoopValueEntry;

typedef struct LoopValueSet {
    LoopValueEntry *list;
} LoopValueSet;

/*
 * initialize a LoopValueSet
 */
static void
InitLoopValueSet(LoopValueSet *lvs)
{
    lvs->list = NULL;
}

/*
 * destroy a loop value set
 */
static void
FreeLoopValueSet(LoopValueSet *lvs)
{
    LoopValueEntry *entry;
    LoopValueEntry *old;
    entry = lvs->list;
    while (entry) {
        old = entry;
        entry = entry->next;
        free(old);
    }
}

/*
 * find a name in an LVSet
 */
static LoopValueEntry *
FindName(LoopValueSet *lvs, AST *name)
{
    LoopValueEntry *entry;

    for (entry = lvs->list; entry; entry = entry->next) {
        if (AstMatch(entry->name, name)) {
            return entry;
        }
    }
    return NULL;
}

/*
 * Merge loop value set "l2" into "lvs", updating assignments as
 * applicable and freeing duplicate entries
 */
static void
MergeAndFreeLoopValueSets(LoopValueSet *lvs, LoopValueSet *l2)
{
    LoopValueEntry *e2;
    LoopValueEntry *old;
    LoopValueEntry *orig;
    e2 = l2->list;
    while(e2) {
        old = e2;
        e2 = e2->next;
        orig = FindName(lvs, old->name);
        if (orig) {
            orig->value = old->value;
            orig->parent = old->parent;
            orig->flags |= old->flags;
            orig->hits += old->hits;
            free(old);
        } else {
            old->next = lvs->list;
            lvs->list = old;
        }
    }
    l2->list = NULL;
}

/*
 * Add a new assignment "name = val" to a LoopValueSet
 */
static LoopValueEntry *
AddAssignment(LoopValueSet *lvs, AST *name, AST *value, unsigned flags, AST *parent)
{
    LoopValueEntry *entry;

    if (name->kind != AST_IDENTIFIER) {
        return NULL;
    }
    entry = FindName(lvs, name);
    if (entry) {
        entry->hits++;
        entry->value = value;
        entry->parent = NULL; // multiple assignments, bad news
        entry->flags |= flags;
        return entry;
    }
    entry = calloc(sizeof(*entry), 1);
    entry->hits = 1;
    entry->name = name;
    entry->value = value;
    entry->parent = parent;
    entry->flags = flags;
    entry->next = lvs->list;
    lvs->list = entry;
    return entry;
}

/*
 * check an operator for any assignments
 */
static unsigned
CheckOperatorForAssignment(LoopValueSet *lvs, AST *parent, AST *ast, unsigned flags)
{
    AST *name = NULL;
    AST *val = ast;
    
    switch (ast->d.ival) {
    case T_INCREMENT:
        name = ast->left ? ast->left : ast->right;
        AddAssignment(lvs, name, val, flags, parent);
        break;
    case T_DECREMENT:
        name = ast->left ? ast->left : ast->right;
        AddAssignment(lvs, name, val, flags, parent);
        break;
    case T_OR:
    case T_AND:
        /* lhs will be unconditional, but we cannot check that here */
        flags |= LVFLAG_CONDITIONAL;
        break;
    default:
        break;
    }
    return flags;
}

/*
 * analyze a statement list, updating the LoopValueSet with all
 * assignments
 */

static void
FindAllAssignments(LoopValueSet *lvs, AST *parent, AST *ast, unsigned flags)
{
    if (!ast) return;
    switch (ast->kind) {
    case AST_ASSIGN:
        AddAssignment(lvs, ast->left, ast->right, flags, parent);
        break;
    case AST_OPERATOR:
        flags = CheckOperatorForAssignment(lvs, parent, ast, flags);
        break;
    case AST_POSTEFFECT:
        ERROR(NULL, "Internal error, should not see POSTEFFECT in LVS");
        break;
    case AST_IF:
    case AST_CASE:
        flags |= LVFLAG_CONDITIONAL;
        break;
    case AST_WHILE:
    case AST_DOWHILE:
    case AST_FOR:
    case AST_FORATLEASTONCE:
        flags |= LVFLAG_NESTED;
        break;
    default:
        break;
    }
    FindAllAssignments(lvs, ast, ast->left, flags);
    FindAllAssignments(lvs, ast, ast->right, flags);
}

//
// return true if an expression is loop dependent
// be conservative about this!
//
static bool
IsLoopDependent(LoopValueSet *lvs, AST *expr)
{
    if (!expr) {
        return false;
    }
    switch (expr->kind) {
    case AST_INTEGER:
        return false;
    case AST_IDENTIFIER:
    {
        Symbol *sym = LookupSymbol(expr->d.string);
        LoopValueEntry *entry;
        switch (sym->type) {
        case SYM_PARAMETER:
        case SYM_RESULT:
        case SYM_LOCALVAR:
        case SYM_TEMPVAR:
            entry = FindName(lvs, expr);
            if (!entry) {
                // never assigned in the loop
                return false;
            }
            if ((0 == (entry->flags & LVFLAG_VARYMASK))) {
                // if entry->hits > 1 then variable is loop dependent
                return entry->hits > 1;
            }
            return true;
        default:
            return true;
        }
    }
    case AST_OPERATOR:
        switch (expr->d.ival) {
        case T_INCREMENT:
        case T_DECREMENT:
            return true;
        default:
            break;
        }
        return IsLoopDependent(lvs, expr->left) || IsLoopDependent(lvs, expr->right);
    case AST_ARRAYREF:
        return IsLoopDependent(lvs, expr->left) || IsLoopDependent(lvs, expr->right);
        
    case AST_ADDROF:
    case AST_ABSADDROF:
        // addr of a variable is loop independent, even if the variable
        // isn't
    {
        AST *ast = expr->left;
        if (!ast) return false;
        if (ast->kind == AST_IDENTIFIER) return false;
        if (ast->kind == AST_ARRAYREF) {
            if (ast->left && ast->left->kind == AST_IDENTIFIER) {
                return IsLoopDependent(lvs, ast->right);
            }
        }
        return IsLoopDependent(lvs, ast);
    }
    case AST_MEMREF:
        // left side is type, so definitely not loop dependent
        return IsLoopDependent(lvs, expr->right);
    default:
        return true;
    }
}

static void
MarkDependencies(LoopValueSet *lvs)
{
    LoopValueEntry *entry;
    int change = 1;

    while (change != 0) {
        change = 0;
        for (entry = lvs->list; entry; entry = entry->next) {
            if (0 == (entry->flags & LVFLAG_VARYMASK)) {
                if (IsLoopDependent(lvs, entry->value)) {
                    entry->flags |= LVFLAG_LOOPDEPEND;
                    change = 1;
                }
            }
        }
    }
}

/*
 * actually perform loop strength reduction on a single loop body
 * return a statement list of assignments which should be performed
 * before the loop
 * "initial" is a loop value set holding potential initial values for
 * loop variables
 */
static AST *
doLoopStrengthReduction(LoopValueSet *initial, AST *body, AST *condition, AST *update)
{
    LoopValueSet lv;
    LoopValueEntry *entry;
    AST *stmtlist = NULL;
    AST *stmt;
    AST *replace;
    bool replaceLeft = true;
    
    InitLoopValueSet(&lv);
    FindAllAssignments(&lv, NULL, body, 0);
    FindAllAssignments(&lv, NULL, update, 0);
    FindAllAssignments(&lv, NULL, condition, 0);
    MarkDependencies(&lv);
    for (entry = lv.list; entry; entry = entry->next) {
        if (entry->hits > 1) {
            continue;
        }
        if (entry->flags & (LVFLAG_VARYMASK)) {
            // if the new value is sufficiently simple,
            // and we know the initial value
            // maybe we can apply loop strength reduction
            continue;
        }
        if (!entry->parent) continue;
        if (entry->parent->kind == AST_STMTLIST) {
            replace = NULL;
        } else if (entry->parent->kind == AST_MEMREF) {
            replace = entry->name;
            replaceLeft = false;
        } else {
            continue;
        }
        // this statement can be pulled out
        stmt = AstAssign(T_ASSIGN, entry->name, entry->value);
        stmt = NewAST(AST_STMTLIST, stmt, NULL);
        stmtlist = AddToList(stmtlist, stmt);
        if (replaceLeft) {
            entry->parent->left = replace; // null out original statement
        } else {
            entry->parent->right = replace;
        }
    }
    MergeAndFreeLoopValueSets(initial, &lv);
    return stmtlist;
}

//
// optimize a statement list
// "lvs" keeps track of current variable assignments
// so we (may) know some of the initial values of
// loop control variables
//
static void
doLoopOptimizeList(LoopValueSet *lvs, AST *list)
{
    AST *stmt, *stmtptr;
    while (list != NULL) {
        if (list->kind != AST_STMTLIST) {
            ERROR(list, "expected statement list");
        }
        stmtptr = list;
        stmt = stmtptr->left;
        while (stmt && stmt->kind == AST_COMMENTEDNODE) {
            stmtptr = stmt;
            stmt = stmtptr->left;
        }
        if (!stmt) goto loop_next;
        switch (stmt->kind) {
        case AST_STMTLIST:
            doLoopOptimizeList(lvs, stmt);
            break;
        case AST_FOR:
        case AST_FORATLEASTONCE:
        {
            AST *condtest;
            AST *update;
            AST *body;
            AST *initial;
            AST *pull;
            initial = stmt->left;
            condtest = stmt->right;
            update = condtest->right;
            condtest = condtest->left;
            body = update->right;
            update = update->left;
            // find initial assignments
            FindAllAssignments(lvs, NULL, initial, 0);
            // optimize sub-loops
            doLoopOptimizeList(lvs, body);
            // now pull out loop invariants
            pull = doLoopStrengthReduction(lvs, body, condtest, update);
            if (pull) {
                stmt = NewAST(AST_STMTLIST, stmt, NULL);
                pull = AddToList(pull, stmt);
                stmtptr->left = pull;
            }
            break;
        }
        default:
            FindAllAssignments(lvs, NULL, stmt, 0);
            break;
        }
    loop_next:
        list = list->right;
    }
}

void
PerformLoopOptimization(Module *Q)
{
    Module *savecur = current;
    Function *func;
    Function *savefunc = curfunc;
    LoopValueSet lv;
    
    if ((gl_optimize_flags & OPT_PERFORM_CSE) == 0)
        return;

    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;
        InitLoopValueSet(&lv);
        doLoopOptimizeList(&lv, func->body);
        FreeLoopValueSet(&lv);
    }
    curfunc = savefunc;
    current = savecur;
}
