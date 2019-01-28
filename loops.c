/*
 * Spin to C/C++ converter
 * Copyright 2011-2018 Total Spectrum Software Inc.
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
#define LVFLAG_LOOPUSED    0x08 /* variable in assignment is used */
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
    bool valid;  //  if false, abandon all attempts to use this LVS
} LoopValueSet;

/*
 * initialize a LoopValueSet
 */
static void
InitLoopValueSet(LoopValueSet *lvs)
{
    lvs->head = lvs->tail = NULL;
    lvs->valid = true;
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
 * Add a new assignment "name = value" to a LoopValueSet
 * if value is NULL then we're adding a use rather than an assignment
 */
static LoopValueEntry *
AddAssignment(LoopValueSet *lvs, AST *name, AST *value, unsigned flags, AST *parent)
{
    LoopValueEntry *entry;

    switch (name->kind) {
    case AST_EXPRLIST:
    {
        // multiple assignment... punt on this for now
        // pretend all of these depend on a hardware register
        while (name) {
            AddAssignment(lvs, name->left, NewAST(AST_HWREG, NULL, NULL), LVFLAG_VARYMASK, NULL);
            name = name->right;
        }
        return NULL;
    }
    case AST_ARRAYREF:
    case AST_MEMREF:
    case AST_HWREG:
    case AST_RANGEREF:
    {
        // we mostly just leave array updates out of things
        return NULL;
    }
    case AST_IDENTIFIER:
        break;
    default:
        // unexpected item, bail
        lvs->valid = false;
        return NULL;
    }
    entry = FindName(lvs, name);
    if (entry) {
        if (value) {
            entry->hits++;
            entry->value = value;
            entry->parent = parent; // keep track of last assignment
            entry->flags |= flags;
        }
        return entry;
    }
    entry = calloc(sizeof(*entry), 1);
    entry->hits = value ? 1 : 0;
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
    case K_INCREMENT:
        name = ast->left ? ast->left : ast->right;
        AddAssignment(lvs, name, val, flags, parent);
        break;
    case K_DECREMENT:
        name = ast->left ? ast->left : ast->right;
        AddAssignment(lvs, name, val, flags, parent);
        break;
    case K_BOOL_OR:
    case K_BOOL_AND:
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
        if (AddAssignment(lvs, ast->left, ast->right, flags, parent)) {
            FindAllAssignments(lvs, parent, ast->right, flags);
            return;
        }
        break;
    case AST_OPERATOR:
        flags = CheckOperatorForAssignment(lvs, parent, ast, flags);
        break;
    case AST_POSTSET:
        ERROR(NULL, "Internal error, should not see POSTSET in LVS");
        lvs->valid = false;
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
#if 0        
    case AST_METHODREF:
    case AST_CONSTREF:
        // don't recurse inside these
        return;
#endif        
    case AST_COMMENTEDNODE:
        // don't update parent here
        break;
    case AST_STMTLIST:
        parent = ast;
        break;
    case AST_IDENTIFIER:
        // if this identifier is being used in the loop, but has not
        // yet been assigned, add an entry for it
        AddAssignment(lvs, ast, NULL, flags | LVFLAG_LOOPUSED, NULL);
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
            if (!entry || !entry->value) {
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
        case K_INCREMENT:
        case K_DECREMENT:
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
    typ = RemoveTypeModifiers(typ);
    while (typ && typ->kind == AST_ARRAYTYPE) {
        typ = RemoveTypeModifiers(typ->left);
    }
    if (!typ) {
        typ = ast_type_generic;
    }
    return EvalConstExpr(RemoveTypeModifiers(typ->left));
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
                    increment = AstOperator(K_NEGATE, NULL, newval->right);
                }
            } else if (newval->kind == AST_OPERATOR && newval->d.ival == K_INCREMENT && (AstMatch(val, newval->left) || AstMatch(val, newval->right))) {
                increment = AstInteger(1);
            } else if (newval->kind == AST_OPERATOR && newval->d.ival == K_DECREMENT && (AstMatch(val, newval->left) || AstMatch(val, newval->right))) {
                increment = AstOperator(K_NEGATE, NULL, AstInteger(1));
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
                return AstOperator(K_NEGATE, NULL, AstInteger(-stepval));
            }
        }
        return NULL;
    case AST_OPERATOR:
        if (val->d.ival == '*') {
            // CONST * index or index * CONST may be
            // strength reduced to adding CONST to the index each time
            AST *constval;
            AST *indexval;
            AST *loopstep;
            int32_t stepval;
            if (IsConstExpr(val->left)) {
                constval = val->left;
                indexval = val->right;
            } else if (IsConstExpr(val->right)) {
                constval = val->right;
                indexval = val->left;
            } else {
                return NULL;
            }
            stepval = EvalConstExpr(constval);
            loopstep = FindLoopStep(lvs, indexval, basename);
            if (!loopstep || !IsConstExpr(loopstep))
                return NULL;
            if (!*basename) return NULL;
            stepval = stepval * EvalConstExpr(loopstep);
            if (stepval >= 0) {
                return AstInteger(stepval);
            } else {
                return AstOperator(K_NEGATE, NULL, AstInteger(-stepval));
            }
        } else if (val->d.ival == '-' || val->d.ival == '+' ) {
            if (IsConstExpr(val->right)) {
                AST *loopstep = FindLoopStep(lvs, val->left, basename);
                if (!loopstep || !IsConstExpr(loopstep) || !*basename) {
                    return NULL;
                }
                return loopstep;
            }
            // C - indexval may be strength reduced
            if (IsConstExpr(val->left)) {
                AST *loopstep = FindLoopStep(lvs, val->right, basename);
                if (!loopstep || !IsConstExpr(loopstep) || !*basename) {
                    return NULL;
                }
                return (val->d.ival == '+') ? loopstep : AstOperator(K_NEGATE, NULL, loopstep);
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
        if (!entry->value) continue;
        if (AstUses(entry->value, entry->name)) {
            entry->flags |= LVFLAG_LOOPDEPEND;
        }
        if (AstUsesMemory(entry->value) || AstUsesMemory(entry->name)) {
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
    if (!lv.valid) {
        return NULL;
    }
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
            if (AstMatch(entry->name, entry->basename)) {
                // entry depends on itself, do not update
                continue;
            }
            // if value is used in a non-assignment before its update, skip
            if (entry->flags & LVFLAG_LOOPUSED) {
                continue;
            }
            pullvalue = DupASTWithReplace(entry->value, entry->basename, initEntry->value);
            if (entry->loopstep->kind == AST_OPERATOR && entry->loopstep->d.ival == K_NEGATE) {
                replace = AstAssign(entry->name,
                                    AstOperator('-', entry->name, entry->loopstep->right));
            } else {
                replace = AstAssign(entry->name,
                                    AstOperator('+', entry->name, entry->loopstep));
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
        stmt = AstAssign(entry->name, pullvalue);
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
    if (!lvs->valid) {
        return NULL;
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
// convert a <= b to a < b+1
// and a <= b-1 to a < b
// leave a < b alone
//
AST *GetRevisedLimit(int updateTestOp, AST *oldLimit)
{
    if (IsConstExpr(oldLimit) || oldLimit->kind == AST_IDENTIFIER) {
        // a constant expression is always good
        if (updateTestOp == K_LE) {
            oldLimit = AstOperator('+', oldLimit, AstInteger(1));
        }
        return oldLimit;
    }
    if (updateTestOp != K_LE) {
        return NULL;
    }
    // only accept very simple expressions
    if (oldLimit->kind == AST_OPERATOR && oldLimit->d.ival == '-') {
        int32_t offset;
        if (oldLimit->left->kind != AST_IDENTIFIER) {
            return NULL;
        }
        if (!IsConstExpr(oldLimit->right)) {
            return NULL;
        }
        offset = EvalConstExpr(oldLimit->right);
        if (offset == 1) {
            return oldLimit->left;
        }
        oldLimit->right = AstInteger(offset - 1);
        return oldLimit;
    }
    return NULL;
}

//
// check for a "simple" decrement loop (counting down from N to 0
//

static bool
CheckSimpleDecrementLoop(AST *stmt)
{
    AST *initial;
    AST *condtest;
    AST *update;
    AST *body;
    AST *updateparent;
    AST *updateVar = NULL;
    AST *updateLimit = NULL;
    int updateTestOp = 0;

    initial = stmt->left;
    condtest = stmt->right;
    updateparent = condtest->right;
    condtest = condtest->left;
    body = updateparent->right;
    update = updateparent->left;
    updateVar = NULL;

    (void) body;
    (void) initial;
    
    // check for a simple count down to 0
    if (!condtest || condtest->kind != AST_OPERATOR) {
        return false;
    }
    updateTestOp = condtest->d.ival;
    if (updateTestOp == '>' || updateTestOp == K_GTU) {
        int32_t ival;
        updateVar = condtest->left;
        if (!updateVar) {
            return false;
        }
        if (updateVar->kind != AST_SYMBOL && updateVar->kind != AST_IDENTIFIER) {
            return false;
        }
        updateLimit = condtest->right;
        if (!IsConstExpr(updateLimit)) {
            return false;
        }
        ival = EvalConstExpr(updateLimit);
        if (ival != 0) {
            return false;
        }
    } else {
        return false;
    }

    /* check that the update is a decrement */
    while (update->kind == AST_SEQUENCE) {
        if (AstUses(update->right, updateVar)) {
            return false;
        }
        update = update->left;
    }
    if (!update) return false;
    if (update->kind != AST_OPERATOR) {
        return false;
    }
    if (update->d.ival == K_DECREMENT) {    
        if (!AstMatch(update->left, updateVar) && !AstMatch(update->right, updateVar)) {
            return false;
        }
    } else {
        return false;
    }

    if (updateTestOp == K_GTU) {
        condtest->d.ival = K_NE;
        return true;
    }
    return false;
}

//
// check for "simple" loops using increments (like "repeat i from 0 to 9") these may be further
// optimized into "repeat 10" if we discover that the loop index is not
// used inside the body
//
static void
CheckSimpleIncrementLoop(AST *stmt)
{
    AST *condtest;
    AST *updateparent;
    AST *update;
    AST *body;
    AST *initial;
    AST *updateVar = NULL;
    AST *updateInit = NULL;
    AST *updateLimit = NULL;
    AST *newInitial = NULL;
    AST *ifskip = NULL;
    AST *thenelseskip = NULL;
    Symbol *sym;
    int updateTestOp = 0;
    int32_t initVal;
    
    initial = stmt->left;
    condtest = stmt->right;
    updateparent = condtest->right;
    condtest = condtest->left;
    body = updateparent->right;
    update = updateparent->left;
    updateVar = NULL;

    /* check initial assignment */
    if (!initial || initial->kind != AST_ASSIGN)
        return;
    updateVar = initial->left;
    if (updateVar->kind != AST_IDENTIFIER)
        return;
    sym = LookupSymbol(updateVar->d.string);
    if (!sym) return;
    switch (sym->type) {
    case SYM_PARAMETER:
    case SYM_RESULT:
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
        /* OK, we can see all uses of these variables */
        break;
    default:
        /* memory based variable, may be modified inside a function call */
        return;
    }
    updateInit = initial->right;
    if (!IsConstExpr(updateInit))
        return;
    /* find where the loop starts */
    initVal = EvalConstExpr(updateInit);

    /* check condition */
    if (!condtest || condtest->kind != AST_OPERATOR)
        return;

    updateTestOp = condtest->d.ival;
    updateLimit = condtest->right;
    if (updateTestOp == K_LE || updateTestOp == '<'
        || updateTestOp == K_LEU || updateTestOp == K_LTU)
    {
        newInitial = GetRevisedLimit(updateTestOp, updateLimit);
    } else {
        return;
    }
    if (!AstMatch(updateVar, condtest->left))
        return;

    /* if this is a FOR rather than FORATLEASTONCE, we have to construct
     * an IF to skip it if the initial condition is false
     */
    if (stmt->kind == AST_FOR) {
        AST *revisedTest;
        revisedTest = NewAST(AST_OPERATOR, NULL, NULL);
        revisedTest->left = AstInteger(initVal);
        revisedTest->right = condtest->right;
        revisedTest->d.ival = condtest->d.ival;
        thenelseskip = NewAST(AST_THENELSE, NULL, NULL);
        ifskip = NewAST(AST_IF, revisedTest, thenelseskip);
        stmt->kind = AST_FORATLEASTONCE;
    }
    
    /* check that the update is i++, and the variable is not used anywhere else */
    while (update->kind == AST_SEQUENCE) {
        if (AstUses(update->right, updateVar)) {
            return;
        }
        update = update->left;
    }
    if (update->kind != AST_OPERATOR || update->d.ival != K_INCREMENT) {
        return;
    }
    if (!AstMatch(update->left, updateVar) && !AstMatch(update->right, updateVar)) {
        return;
    }
    /* if i is used inside the loop, bail */
    if (AstUses(body, updateVar)) {
        return;
    }
    
    AstReportAs(update); // new ASTs should be associated with the "update" line
    /* flip the update to -- */
    update->d.ival = K_DECREMENT;
    /* change the initialization */

    /* if initVal is nonzero, we want to go from 0 to X-N instead of from N to X */
    if (initVal) {
        newInitial = AstOperator('-', newInitial, AstInteger(initVal));
        if (IsConstExpr(newInitial)) {
            newInitial = AstInteger(EvalConstExpr(newInitial));
        }
    }
    initial = AstAssign(updateVar, newInitial);
    condtest = AstOperator(K_NE, updateVar, AstInteger(0));

    /* update statement */
    stmt->left = initial;
    stmt->right->left = condtest;

    if (thenelseskip) {
        AST *newstmt = NewAST(AST_FORATLEASTONCE, NULL, NULL);
        *newstmt = *stmt;
        thenelseskip->left = NewAST(AST_STMTLIST, newstmt, NULL);
        *stmt = *ifskip;
    }
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
            AstReportAs(initial);
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
            // now that we (may have) hoisted some things out of the loop,
            // see if we can rewrite the loop initial and conditions
            // to use djnz
            if (!CheckSimpleDecrementLoop(stmt)) {
                CheckSimpleIncrementLoop(stmt);
            }
            break;
        }
        default:
            FindAllAssignments(lvs, stmtptr, stmt, 0);
            break;
        }
    loop_next:
        list = list->right;
        if (pull) {
            // we pulled some statements out of the loop body and
            // need to put them before the loop
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
        if (func->body && func->body->kind != AST_STRING) {
            InitLoopValueSet(&lv);
            doLoopOptimizeList(&lv, func->body);
            FreeLoopValueSet(&lv);
        }
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
