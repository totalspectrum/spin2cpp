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
    AST *loopstep; // if strength reduction is possible, do this each time through
    AST *basename; // base loop induction variable that "loopstep" depends on
} LoopValueEntry;

typedef struct LoopValueSet {
    LoopValueEntry *head;
    LoopValueEntry *tail;
} LoopValueSet;

/*
 * initialize a LoopValueSet
 */
static void
InitLoopValueSet(LoopValueSet *lvs)
{
    lvs->head = lvs->tail = NULL;
}

/*
 * destroy a loop value set
 */
static void
FreeLoopValueSet(LoopValueSet *lvs)
{
    LoopValueEntry *entry;
    LoopValueEntry *old;
    entry = lvs->head;
    while (entry) {
        old = entry;
        entry = entry->next;
        free(old);
    }
    lvs->head = lvs->tail = NULL;
}

/*
 * add a name to an LVSet
 */
static void
AddToLVS(LoopValueSet *lvs, LoopValueEntry *entry)
{
    entry->next = NULL;
    if (!lvs->tail) {
        lvs->head = lvs->tail = entry;
        return;
    }
    lvs->tail->next = entry;
    lvs->tail = entry;
}
    
/*
 * find a name in an LVSet
 */
static LoopValueEntry *
FindName(LoopValueSet *lvs, AST *name)
{
    LoopValueEntry *entry;

    for (entry = lvs->head; entry; entry = entry->next) {
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
    e2 = l2->head;
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
            AddToLVS(lvs, old);
        }
    }
    l2->head = NULL;
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
        entry->parent = parent; // keep track of last assignment
        entry->flags |= flags;
        return entry;
    }
    entry = calloc(sizeof(*entry), 1);
    entry->hits = 1;
    entry->name = name;
    entry->value = value;
    entry->parent = parent;
    entry->flags = flags;
    AddToLVS(lvs, entry);
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
    case AST_METHODREF:
    case AST_CONSTREF:
        // don't recurse inside these
        return;
    case AST_COMMENTEDNODE:
        // don't update parent here
        break;
    case AST_STMTLIST:
        parent = ast;
        break;
    default:
        parent = ast; // do we really want this?
        break;
    }
    FindAllAssignments(lvs, parent, ast->left, flags);
    FindAllAssignments(lvs, parent, ast->right, flags);
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
        if (!sym) {
            return true;
        }
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
                unsigned saveflag;
                bool r;
                // if entry->hits > 1 then variable is loop dependent
                if (entry->hits > 1) return true;
                // temporarily pretend the variable is dependent
                // and see if that makes its assigned value dependent
                // (this detects inter-variable circular dependencies)
                saveflag = entry->flags;
                entry->flags |= LVFLAG_LOOPDEPEND;
                r = IsLoopDependent(lvs, entry->value);
                entry->flags = saveflag;
                return r;
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
            if (ast->left) {
                if (ast->left->kind == AST_IDENTIFIER) {
                    return IsLoopDependent(lvs, ast->right);
                } else if (ast->left->kind == AST_MEMREF) {
                    return IsLoopDependent(lvs, ast->right) || IsLoopDependent(lvs, ast->left);
                }
            }
        }
        return IsLoopDependent(lvs, ast);
    }
    case AST_MEMREF:
        // left side is type, we don't need to check that
        return IsLoopDependent(lvs, expr->right);
    default:
        return true;
    }
}

//
// given an expression "val"
// find an expression for the difference in values
// between this loop step and the next
// Sets *baseName to point to the bottom level name
// that changes between loop updates
//

static int
ElementSize(AST *typ)
{
    while (typ->kind == AST_ARRAYTYPE) {
        typ = typ->left;
    }
    return EvalConstExpr(typ->left);
}

static AST *
FindLoopStep(LoopValueSet *lvs, AST *val, AST **basename)
{
    AST *loopstep;
    LoopValueEntry *entry;
    AST *newval;
    int stepval;
    
    if (!val) return NULL;
    switch(val->kind) {
    case AST_IDENTIFIER:
        newval = val;
        for(;;) {
            entry = FindName(lvs, newval);
            if (!entry) return NULL;
            if (entry->hits != 1) return NULL;
            newval = entry->value;
            if (!newval) return NULL;
            if (newval->kind != AST_IDENTIFIER) break;
        }
        if (AstUses(newval, val)) {
            AST *increment = NULL;
            if (newval->kind == AST_OPERATOR && newval->d.ival == '+') {
                if (AstMatch(val, newval->left) && IsConstExpr(newval->right)) {
                    increment = newval->right;
                }
            } else if (newval->kind == AST_OPERATOR && newval->d.ival == '-') {
                if (AstMatch(val, newval->left) && IsConstExpr(newval->right)) {
                    increment = AstOperator(T_NEGATE, NULL, newval->right);
                }
            } else if (newval->kind == AST_OPERATOR && newval->d.ival == T_INCREMENT && (AstMatch(val, newval->left) || AstMatch(val, newval->right))) {
                increment = AstInteger(1);
            } else if (newval->kind == AST_OPERATOR && newval->d.ival == T_DECREMENT && (AstMatch(val, newval->left) || AstMatch(val, newval->right))) {
                increment = AstOperator(T_NEGATE, NULL, AstInteger(1));
            }
            if (increment) {
                if (*basename == NULL) {
                    *basename = val;
                } else {
                    if (!AstMatch(val, *basename)) {
                        return NULL;
                    }
                }
                return increment;
            }
            return NULL;
        }
        return FindLoopStep(lvs, newval, basename);
    case AST_ADDROF:
        val = val->left;
        if (val && val->kind == AST_ARRAYREF) {
            int elementsize = 4;
            AST *arrayname = val->left;
            Symbol *sym;
            if (arrayname->kind == AST_MEMREF) {
                elementsize = ElementSize(arrayname->left);
            }
            else if (arrayname->kind == AST_IDENTIFIER) {
                sym = LookupSymbol(arrayname->d.string);
                if (!sym) {
                    return NULL;
                }
                switch (sym->type) {
                case SYM_VARIABLE:
                case SYM_TEMPVAR:
                case SYM_LOCALVAR:
                case SYM_PARAMETER:
                    elementsize = ElementSize((AST *)sym->val);
                    break;
                case SYM_LABEL:
                {
                    Label *lab = (Label *)sym->val;
                    elementsize = ElementSize(lab->type);
                    break;
                }
                default:
                    return NULL;
                }
            } else {
                return NULL;
            }
            val = val->right;
            if (!val) return NULL;
            loopstep = FindLoopStep(lvs, val, basename);
            if (!loopstep) return NULL;
            if (!IsConstExpr(loopstep)) return NULL;
            if (!*basename) return NULL;
            stepval = elementsize * EvalConstExpr(loopstep);
            if (stepval >= 0) {
                return AstInteger(stepval);
            } else {
                return AstOperator(T_NEGATE, NULL, AstInteger(-stepval));
            }
        }
        return NULL;
    default:
        return NULL;
    }
}

static bool
AstUsesMemory(AST *ast)
{
    if (!ast) return false;
    switch (ast->kind) {
    case AST_MEMREF:
        return true;
    case AST_CONSTREF:
        return false;
    case AST_FUNCCALL:
        return true;
    case AST_IDENTIFIER:
    {
        Symbol *sym = LookupSymbol(ast->d.string);
        if (!sym) {
            return true;
        }
        switch (sym->type) {
        case SYM_PARAMETER:
        case SYM_RESULT:
        case SYM_LOCALVAR:
        case SYM_TEMPVAR:
            return false;
        default:
            return true;
        }
    }
    default:
        return AstUsesMemory(ast->left) || AstUsesMemory(ast->right);
    }
}

static void
MarkDependencies(LoopValueSet *lvs)
{
    LoopValueEntry *entry;
    int change = 1;

    // mark any self-dependent entries
    for (entry = lvs->head; entry; entry = entry->next) {
        if (AstUses(entry->value, entry->name)) {
            entry->flags |= LVFLAG_LOOPDEPEND;
        }
        if (AstUsesMemory(entry->value)) {
            entry->flags |= LVFLAG_LOOPDEPEND;
        }
    }
    
    while (change != 0) {
        change = 0;
        for (entry = lvs->head; entry; entry = entry->next) {
            if (0 == (entry->flags & LVFLAG_VARYMASK)) {
                if (IsLoopDependent(lvs, entry->value)) {
                    entry->flags |= LVFLAG_LOOPDEPEND;
                    change = 1;
                }
            }
        }
    }

    // now look here for expressions that can be strength reduced
    for (entry = lvs->head; entry; entry = entry->next) {
        if (entry->hits == 1 && (entry->flags & LVFLAG_VARYMASK)) {
            entry->basename = NULL;
            entry->loopstep = FindLoopStep(lvs, entry->value, &entry->basename);
        }
    }
}

/*
 * place an assignment statement after a parent
 */
static bool
PlaceAssignAfter(AST *parent, AST *assign)
{
    AST *stmt;
    if (!parent) {
        return false;
    }
    if (parent->kind == AST_STMTLIST) {
        stmt = NewAST(AST_STMTLIST, assign, NULL);
        parent->left = NewAST(AST_STMTLIST, parent->left, stmt);
        return true;
    }
    if (parent->kind == AST_SEQUENCE) {
        stmt = NewAST(AST_SEQUENCE, assign, NULL);
        parent->left = NewAST(AST_SEQUENCE, parent->left, stmt);
        return true;
    }
    return false;
}

/*
 * actually perform loop strength reduction on a single loop body
 * "initial" is a loop value set holding potential initial values for
 * loop variables
 * returns a statement list of assignments which should be performed
 * before the loop
 * and sets *updates to a list of assignments which should be performed on each loop
 * iteration
 */
static AST *
doLoopStrengthReduction(LoopValueSet *initial, AST *body, AST *condition, AST *update)
{
    LoopValueSet lv;
    LoopValueEntry *entry;
    LoopValueEntry *initEntry;
    LoopValueEntry *lastAssign;
    AST *stmtlist = NULL;
    AST *stmt;
    AST *replace;
    AST *pullvalue; // initial value for pulled out code
    AST *parent;
    
    InitLoopValueSet(&lv);
    FindAllAssignments(&lv, body, body, 0);
    FindAllAssignments(&lv, update, update, 0);
    FindAllAssignments(&lv, NULL, condition, 0);
    MarkDependencies(&lv);
    for (entry = lv.head; entry; entry = entry->next) {
        if (entry->hits > 1) {
            continue;
        }
        parent = entry->parent;
        if (!parent) continue;
        if (parent->kind == AST_STMTLIST) {
            /* OK */
        } else {
            continue;
        }
        if (entry->flags & (LVFLAG_VARYMASK)) {
            // if the new value is sufficiently simple,
            // and we know the initial value
            // maybe we can apply loop strength reduction
            if (!entry->loopstep || !entry->basename) {
                continue;
            }
            if (entry->basename->kind != AST_IDENTIFIER) {
                continue;
            }
            initEntry = FindName(initial, entry->basename);
            if (!initEntry) {
                continue;
            }
            lastAssign = FindName(&lv, entry->basename);
            if (!lastAssign) {
                continue;
            }
            pullvalue = DupASTWithReplace(entry->value, entry->basename, initEntry->value);
            if (entry->loopstep->kind == AST_OPERATOR && entry->loopstep->d.ival == T_NEGATE) {
                replace = AstAssign('-', entry->name, entry->loopstep->right);
            } else {
                replace = AstAssign('+', entry->name, entry->loopstep);
            }

            // we have to place the "replace" after the last
            // update to "basename" (the loop variable this value
            // ultimately depends on)

            if (!PlaceAssignAfter(lastAssign->parent, replace)) {
                continue;
            }
            parent->left = NULL;
        } else {
            pullvalue = entry->value;
            parent->left = NULL; // null out original statement
        }
        // this statement can be pulled out
        stmt = AstAssign(T_ASSIGN, entry->name, pullvalue);
        stmt = NewAST(AST_STMTLIST, stmt, NULL);

        stmtlist = AddToList(stmtlist, stmt);
    }
    MergeAndFreeLoopValueSets(initial, &lv);
    return stmtlist;
}

static void doLoopOptimizeList(LoopValueSet *lvs, AST *list);

//
// helper for doLoopOptimizeList()
//
static AST *
doLoopHelper(LoopValueSet *lvs, AST *initial, AST *condtest, AST *update,
             AST *body)
{
    LoopValueSet sub;
    AST *pull;
    
    // initial loop assignments take place before the loop
    if (initial) {
        FindAllAssignments(lvs, NULL, initial, 0);
    }
    // optimize sub-loops
    InitLoopValueSet(&sub);
    doLoopOptimizeList(&sub, body);
    FreeLoopValueSet(&sub);
    // pull out loop invariants
    pull = doLoopStrengthReduction(lvs, body, condtest, update);
    return pull;
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
    AST *pull;
    
    while (list != NULL) {
        pull = NULL;
        if (list->kind != AST_STMTLIST) {
            ERROR(list, "expected statement list");
        }
        stmtptr = list;
        stmt = stmtptr->left;
        while (stmt && stmt->kind == AST_COMMENTEDNODE) {
            stmt = stmt->left;
        }
        if (!stmt) goto loop_next;
        switch (stmt->kind) {
        case AST_STMTLIST:
            doLoopOptimizeList(lvs, stmt);
            break;
        case AST_WHILE:
        case AST_DOWHILE:
        {
            AST *condtest = stmt->left;
            AST *body = stmt->right;
            AST *initial = NULL;
            AST *update = NULL;
            pull = doLoopHelper(lvs, initial, condtest, update, body);
            break;
        }
        case AST_FOR:
        case AST_FORATLEASTONCE:
        {
            AST *condtest;
            AST *updateparent;
            AST *update;
            AST *body;
            AST *initial;
            initial = stmt->left;
            condtest = stmt->right;
            updateparent = condtest->right;
            condtest = condtest->left;
            body = updateparent->right;
            update = updateparent->left;
            if (update && update->kind != AST_SEQUENCE) {
                update = NewAST(AST_SEQUENCE, update, NULL);
                updateparent->left = update;
            }
            pull = doLoopHelper(lvs, initial, condtest, update, body);
            break;
        }
        default:
            FindAllAssignments(lvs, stmtptr, stmt, 0);
            break;
        }
    loop_next:
        list = list->right;
        if (pull) {
            stmt = NewAST(AST_STMTLIST, stmt, NULL);
            pull = AddToList(pull, stmt);
            stmtptr->left = pull;
        }
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

//
// debug code
//
void
DumpLVS(LoopValueSet *lvs)
{
    LoopValueEntry *e;
    for (e = lvs->head; e; e = e->next) {
        printf("entry for: ");
        DumpAST(e->name);
        printf("value: ");
        DumpAST(e->value);
        printf("\n");
    }
}
