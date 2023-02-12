/*
 * Spin to C/C++ converter
 * Copyright 2011-2023 Total Spectrum Software Inc.
 * MIT Licensed
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
 * utility functions
 */
bool AstMatchName(AST *expr, AST *name)
{
    if (expr && expr->kind == AST_LOCAL_IDENTIFIER) {
        expr = expr->left;
    }
    if (name && name->kind == AST_LOCAL_IDENTIFIER) {
        name = name->left;
    }
    return AstMatch(expr, name);
}
bool AstUsesName(AST *expr, AST *name)
{
    if (expr && expr->kind == AST_LOCAL_IDENTIFIER) {
        expr = expr->left;
    }
    if (name && name->kind == AST_LOCAL_IDENTIFIER) {
        name = name->left;
    }
    return AstUses(expr, name);
}

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
        if (AstMatchName(entry->name, name)) {
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
    case AST_LOCAL_IDENTIFIER:
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
    entry = (LoopValueEntry*)calloc(sizeof(*entry), 1);
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
    case K_DECREMENT:
    case '?':
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
    case AST_ADDROF:
    case AST_ABSADDROF:
        AddAssignment(lvs, ast->left, NewAST(AST_HWREG, NULL, NULL), LVFLAG_VARYMASK, NULL);
        break;
    case AST_OPERATOR:
        flags = CheckOperatorForAssignment(lvs, parent, ast, flags);
        break;
    case AST_POSTSET:
        //ERROR(NULL, "Internal error, should not see POSTSET in LVS");
        // bytecode generates POSTSETs, so just invalidate for now
        lvs->valid = false;
        break;
    case AST_IF:
    case AST_CASE:
    case AST_LABEL:
        flags |= LVFLAG_CONDITIONAL;
        break;
    case AST_GOTO:
        /* cannot cope with this */
        lvs->valid = 0;
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
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
        // if this identifier is being used in the loop, but has not
        // yet been assigned, add an entry for it
        AddAssignment(lvs, ast, NULL, flags | LVFLAG_LOOPUSED, NULL);
        return;
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
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
    {
        Symbol *sym = LookupAstSymbol(expr, NULL);
        LoopValueEntry *entry;
        if (!sym) {
            return true;
        }
        switch (sym->kind) {
        case SYM_PARAMETER:
        case SYM_RESULT:
        case SYM_LOCALVAR:
        case SYM_TEMPVAR:
            entry = FindName(lvs, expr);
            if (!entry || !entry->value) {
                // never assigned in the loop
                if (curfunc->local_address_taken) return true;
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
        if (IsIdentifier(ast)) return false;
        if (ast->kind == AST_ARRAYREF) {
            if (ast->left) {
                if (IsIdentifier(ast->left)) {
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
    if (!typ) {
        typ = ast_type_generic;
    }
    typ = RemoveTypeModifiers(typ);
    if (typ->kind == AST_ARRAYTYPE) {
        typ = typ->left;
    }
    return TypeSize(typ);
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
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
        newval = val;
        for(;;) {
            entry = FindName(lvs, newval);
            if (!entry) return NULL;
            if (entry->hits != 1) return NULL;
            newval = entry->value;
            if (!newval) return NULL;
            if (!IsIdentifier(newval)) break;
        }
        if (AstUsesName(newval, val)) {
            AST *increment = NULL;
            if (newval->kind == AST_OPERATOR && newval->d.ival == '+') {
                if (AstMatchName(val, newval->left) && IsConstExpr(newval->right)) {
                    increment = newval->right;
                }
            } else if (newval->kind == AST_OPERATOR && newval->d.ival == '-') {
                if (AstMatchName(val, newval->left) && IsConstExpr(newval->right)) {
                    increment = AstOperator(K_NEGATE, NULL, newval->right);
                }
            } else if (newval->kind == AST_OPERATOR && newval->d.ival == K_INCREMENT && (AstMatchName(val, newval->left) || AstMatchName(val, newval->right))) {
                increment = AstInteger(1);
            } else if (newval->kind == AST_OPERATOR && newval->d.ival == K_DECREMENT && (AstMatchName(val, newval->left) || AstMatchName(val, newval->right))) {
                increment = AstOperator(K_NEGATE, NULL, AstInteger(1));
            }
            if (increment) {
                if (*basename == NULL) {
                    *basename = val;
                } else {
                    if (!AstMatchName(val, *basename)) {
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
            else if (IsIdentifier(arrayname)) {
                sym = LookupAstSymbol(arrayname, NULL);
                if (!sym) {
                    return NULL;
                }
                switch (sym->kind) {
                case SYM_VARIABLE:
                case SYM_TEMPVAR:
                case SYM_LOCALVAR:
                case SYM_PARAMETER:
                    elementsize = ElementSize((AST *)sym->v.ptr);
                    break;
                case SYM_LABEL:
                {
                    Label *lab = (Label *)sym->v.ptr;
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
    case AST_ARRAYREF:
        return true;
    case AST_CONSTREF:
        return false;
    case AST_FUNCCALL:
    case AST_GOSUB:
        return true;
    case AST_LOCAL_IDENTIFIER:
    case AST_IDENTIFIER:
    {
        Symbol *sym = LookupAstSymbol(ast, NULL);
        if (!sym) {
            return true;
        }
        switch (sym->kind) {
        case SYM_TEMPVAR:
        case SYM_PARAMETER:
        case SYM_RESULT:
        case SYM_LOCALVAR:
            return (curfunc->local_address_taken != 0);
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
        if (AstUsesName(entry->value, entry->name)) {
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
            if (!IsIdentifier(entry->basename)) {
                continue;
            }
            initEntry = FindName(initial, entry->basename);
            if (!initEntry || (initEntry->flags & LVFLAG_CONDITIONAL) ) {
                continue;
            }
            lastAssign = FindName(&lv, entry->basename);
            if (!lastAssign) {
                continue;
            }
            if (AstMatchName(entry->name, entry->basename)) {
                // entry depends on itself, do not update
                continue;
            }
            // if value is used in a non-assignment before its update, skip
            if (entry->flags & LVFLAG_LOOPUSED) {
                continue;
            }
            // if this assignment is conditional don't mess with it
            if (entry->flags & LVFLAG_CONDITIONAL) {
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
    // and update assignments based on loop
    FindAllAssignments(lvs, NULL, body, 0);
    return pull;
}

//
// convert a <= b to a < b+1
// and a <= b-1 to a < b
// leave a < b alone
//
AST *GetRevisedLimit(int updateTestOp, AST *oldLimit)
{
    if (IsConstExpr(oldLimit) || IsIdentifier(oldLimit)) {
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
        if (!IsIdentifier(oldLimit->left)) {
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
    if (ExprHasSideEffects(condtest)) {
        return false;
    }
    updateTestOp = condtest->d.ival;
    if (updateTestOp == '>' || updateTestOp == K_GTU) {
        int32_t ival;
        updateVar = condtest->left;
        if (!updateVar) {
            return false;
        }
        if (!IsIdentifier(updateVar)) {
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
    } else if (updateTestOp != K_NE) {
        return false;
    }

    /* check that the update is a decrement */
    while (update && update->kind == AST_SEQUENCE) {
        if (AstUsesName(update->right, updateVar)) {
            return false;
        }
        update = update->left;
    }
    if (!update) return false;
    if (update->kind != AST_OPERATOR) {
        return false;
    }
    if (update->d.ival == K_DECREMENT) {    
        if (!AstMatchName(update->left, updateVar) && !AstMatchName(update->right, updateVar)) {
            return false;
        }
    } else {
        return false;
    }

    if (updateTestOp == K_GTU) {
        condtest->d.ival = K_NE;
        return true;
    }
    if (condtest->d.ival != K_NE) {
        return false;
    }
    // FIXME: should we change AST_FOR to AST_FORATLEASTONCE here?
    if (stmt->kind == AST_FOR) {
        AST *newstmt = NewAST(AST_FORATLEASTONCE, NULL, NULL);
        AST *skipif;
        *newstmt = *stmt;
        newstmt->kind = AST_FORATLEASTONCE;
        newstmt->left = NULL; // initial will be performed earlier
        newstmt = NewAST(AST_STMTLIST, newstmt, NULL);
        newstmt = NewAST(AST_THENELSE, newstmt, NULL);
        skipif = NewAST(AST_IF, DupAST(condtest), newstmt);
        skipif = NewAST(AST_STMTLIST, initial,
                        NewAST(AST_STMTLIST, skipif, NULL));
        
        *stmt = *skipif;
    }
    return true;
}

//
// check for branches
// function calls are OK, I think, because we already reject loop
// transforms that can affect variables in memory
//
static bool
HasBranch(AST *body)
{
    if (!body) return false;
    switch (body->kind) {
    case AST_GOTO:
    case AST_GOSUB:
    case AST_LABEL:
        return true;
    default:
        return HasBranch(body->left) || HasBranch(body->right);
    }
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
    ASTReportInfo saveinfo;
    
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
    if (!IsIdentifier(updateVar))
        return;
    sym = LookupAstSymbol(updateVar, NULL);
    if (!sym) return;
    switch (sym->kind) {
    case SYM_PARAMETER:
    case SYM_RESULT:
    case SYM_LOCALVAR:
    case SYM_TEMPVAR:
        if (curfunc->local_address_taken)
            return;
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
        if (!newInitial) {
            return;
        }
    } else {
        return;
    }
    if (!AstMatchName(updateVar, condtest->left))
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
        if (AstUsesName(update->right, updateVar)) {
            return;
        }
        update = update->left;
    }
    if (update->kind != AST_OPERATOR || update->d.ival != K_INCREMENT) {
        return;
    }
    if (!AstMatchName(update->left, updateVar) && !AstMatchName(update->right, updateVar)) {
        return;
    }
    /* if i is used inside the loop, bail */
    if (AstUsesName(body, updateVar)) {
        return;
    }
    /* similarly if there are gotos or gosubs */
    if (HasBranch(body)) {
        return;
    }
    AstReportAs(update, &saveinfo); // new ASTs should be associated with the "update" line
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
    AstReportDone(&saveinfo);
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
            ASTReportInfo saveinfo;
            
            initial = stmt->left;
            AstReportAs(initial, &saveinfo);
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
            // add the pulled stuff to initialization, if necessary
            if (pull && stmt->left) {
                AST *init = stmt->left;
                AST *initvar;
                bool pullDependsOnInit = true;
                if (init->kind == AST_ASSIGN) {
                    initvar = init->left;
                    if (IsIdentifier(initvar) && !AstUses(pull, initvar)) {
                        pullDependsOnInit = false;
                    }
                }
                if (pullDependsOnInit) {
                    init = NewAST(AST_SEQUENCE, init, pull);
                    stmt->left = init;
                    pull = NULL;
                }
            }
            // now that we (may have) hoisted some things out of the loop,
            // see if we can rewrite the loop initial and conditions
            // to use djnz
            if (!CheckSimpleDecrementLoop(stmt)) {
                CheckSimpleIncrementLoop(stmt);
            }
            AstReportDone(&saveinfo);
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

static void
doBasicLoopOptimization(AST *list) {
    AST *stmt;
    
    while (list != NULL) {
        if (list->kind != AST_STMTLIST) return;
        stmt = list->left;
        while (stmt && stmt->kind == AST_COMMENTEDNODE) {
            stmt = stmt->left;
        }
        if (!stmt) {
            list = list->right;
            continue;
        }
        switch (stmt->kind) {
        case AST_STMTLIST:
            doBasicLoopOptimization(stmt);
            break;
        case AST_WHILE:
        case AST_DOWHILE:
            doBasicLoopOptimization(stmt->right);
            break;
        case AST_FOR:
        case AST_FORATLEASTONCE:
        {
            AST *condtest;
            AST *updateparent;
            AST *update;
            AST *body;
            AST *initial;
            ASTReportInfo saveinfo;
            initial = stmt->left;
            condtest = stmt->right;
            updateparent = condtest->right;
            condtest = condtest->left;
            body = updateparent->right;
            update = updateparent->left;
            (void)update;
            
            doBasicLoopOptimization(body);
            AstReportAs(initial, &saveinfo);
            if (!CheckSimpleDecrementLoop(stmt)) {
                CheckSimpleIncrementLoop(stmt);
            }
            AstReportDone(&saveinfo);
            break;
        }
        case AST_IF:
            doBasicLoopOptimization(stmt->right->left);
            doBasicLoopOptimization(stmt->right->right);
            break;
        default:
            break;
        }
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

    if (gl_output == OUTPUT_C || gl_output == OUTPUT_CPP) {
        return;
    }
    current = Q;
    for (func = Q->functions; func; func = func->next) {
        curfunc = func;
        if (func->optimize_flags & OPT_PERFORM_LOOPREDUCE) {
            if (func->body && func->body->kind != AST_STRING && func->body->kind != AST_BYTECODE) {
                InitLoopValueSet(&lv);
                doLoopOptimizeList(&lv, func->body);
                FreeLoopValueSet(&lv);
            }
        } else if (func->optimize_flags & OPT_LOOP_BASIC) {
            doBasicLoopOptimization(func->body);
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
        printf("hits=%d flags=0x%04x", e->hits, e->flags);
        printf("\n");
    }
}

/*
 * check for trivial loop tests
 */
static int
LoopTestAlwaysTrue(AST *expr)
{
    AST *typ;
    int32_t endval;
    int32_t maxval;
    int siz;
    
    if (expr->kind != AST_OPERATOR) {
        return 0;
    }
    if (!IsConstExpr(expr->right)) {
        return 0;
    }
    typ = ExprType(expr->left);
    if (!typ || (siz = TypeSize(typ)) >= 4) {
        return 0;
    }
    if (!IsUnsignedType(typ) || !IsIntType(typ)) {
        return 0;
    }
    if (expr->d.ival != '<' && expr->d.ival != K_LTU) {
        return 0;
    }
    endval = EvalConstExpr(expr->right);
    if ( expr->d.ival == K_LTU ) {
        maxval = (1<<(8*siz)) - 1;
    } else {
        maxval = (1<<((8*siz)-1)) - 1;
    }
    if (endval > maxval) {
        return 1;
    }
    return 0;
}

/*
 * transform AST for a counting repeat loop
 */
AST *
TransformCountRepeat(AST *ast)
{
    AST *origast = ast;

    // initial values for the loop
    AST *loopvar = NULL;
    AST *fromval, *toval;
    AST *stepval;
    AST *body;

    AST *looptype; // type of loop
    int isIntegerLoop = 1;
    int isUnsignedLoop = 0;
    
    // what kind of test to perform (loopvar < toval, loopvar != toval, etc.)
    // note that if not set, we haven't figured it out yet (and probably
    // need to use between)
    int testOp = 0;

    // if the step value is known, put +1 (for increasing) or -1 (for decreasing) here
    int knownStepDir = 0;

    // if the step value is constant, put it here
    int knownStepVal = 0;
    
    // test for terminating the loop
    AST *condtest = NULL;

    int loopkind = AST_FOR;
    AST *forast;
    
    AST *initstmt = NULL;
    AST *stepstmt = NULL;

    AST *limitvar = NULL;
    AST *finalstmt = NULL;
    AST *updatevar = NULL;
    
    ASTReportInfo saveinfo;
    bool delayedUpdate = false;
    
    /* create new ast elements using this ast's line info, at least for now */
    AstReportAs(ast, &saveinfo);

    if (curfunc && curfunc->language == LANG_SPIN_SPIN2 && 0) {  // Spin2 used to have weird loop finishing
        delayedUpdate = true;
    }
    if (ast->left) {
        if (ast->left->kind == AST_IDENTIFIER) {
            loopvar = ast->left;
        } else if (ast->left->kind == AST_RESULT) {
            loopvar = ast->left;
        } else {
            ERROR(ast, "Need a variable name for the loop");
            AstReportDone(&saveinfo);
            return origast;
        }
    }
    looptype = ExprType(loopvar);
    isIntegerLoop = (looptype == NULL) || IsIntType(looptype);
    if (isIntegerLoop && looptype && IsUnsignedType(looptype)) {
        isUnsignedLoop = 1;
    }
    ast = ast->right;
    if (ast->kind != AST_FROM) {
        ERROR(ast, "expected FROM");
        AstReportDone(&saveinfo);
        return origast;
    }
    fromval = ast->left;
    ast = ast->right;
    if (ast->kind != AST_TO) {
        ERROR(ast, "expected TO");
        AstReportDone(&saveinfo);
        return origast;
    }
    toval = ast->left;
    ast = ast->right;
    if (ast->kind != AST_STEP) {
        ERROR(ast, "expected STEP");
        AstReportDone(&saveinfo);
        return origast;
    }
    if (ast->left) {
        stepval = ast->left;
    } else {
        stepval = AstInteger(1);
    }
    if (IsConstExpr(stepval)) {
        knownStepVal = EvalConstExpr(stepval);
        if (!isIntegerLoop) {
            knownStepDir = (knownStepVal < 0) ? -1 : 1;
            knownStepVal = 0;
        }
    }

    if (gl_output == OUTPUT_BYTECODE && gl_interp_kind != INTERP_KIND_NUCODE) {
        // Leave REPEAT N kinda loops alone for bytecode
        if (!loopvar && knownStepVal == 1 && fromval == NULL && isIntegerLoop) {
            AstReportDone(&saveinfo);
            return origast;
        }
        // Leave REPEAT i from X to Y (STEP Z) kinda loops alone for bytecode
        if (loopvar && fromval && isIntegerLoop && (!isUnsignedLoop || (looptype && TypeSize(looptype) <= 2))) {
            AstReportDone(&saveinfo);
            return origast;
        }
    }

    if (!IsSpinLang(curfunc->language)) {
        // only Spin does the weirdness where
        // repeat i from a to b step 1
        // walks backwards if a > b; other languages
        // just count up if the step is constant
        if (knownStepVal) {
            knownStepDir = (knownStepVal > 0) ? 1 : -1;
        }
    }
    
    body = ast->right;

    /* for fixed counts (like "REPEAT expr") we get a NULL value
       for fromval; this signals that we should be counting
       from 0 to toval - 1 (in C) or from toval down to 1 (in asm)
       it also means that we don't have to do the "between" check,
       we can just count one way
    */
    if (fromval == NULL && isIntegerLoop) {
        if ((gl_output == OUTPUT_C || gl_output == OUTPUT_CPP) && IsConstExpr(toval)) {
            // for (i = 0; i < 10; i++) is more idiomatic
            fromval = AstInteger(0);
            testOp = isUnsignedLoop ? K_LTU : '<';
            knownStepDir = 1;
        } else {
            fromval = toval;
            toval = AstInteger(0);
            testOp = isUnsignedLoop ? K_GTU : '>';
            knownStepDir = -1;
            if (knownStepVal == 1) {
                testOp = K_NE;
            }
        }
    }
    if (isIntegerLoop) {
        if (IsConstExpr(fromval) && IsConstExpr(toval)) {
            int32_t fromi, toi;
            int32_t reps;
            
            fromi = EvalConstExpr(fromval);
            toi = EvalConstExpr(toval);
            if (!knownStepDir) {
                if (isUnsignedLoop) {
                    knownStepDir = ((uint32_t)fromi > (uint32_t)toi) ? -1 : 1;
                } else {
                    knownStepDir = (fromi > toi) ? -1 : 1;
                }
            }
            reps = (toi - fromi) * knownStepDir;
            if ( reps >= 0 ) {
                // loop will execute at least once
                if (!(gl_output == OUTPUT_C || gl_output == OUTPUT_CPP)) {
                    if ((int32_t)fromi > 0 && (int32_t)toi > 0 && knownStepVal == 1 ) {
                        isUnsignedLoop = 1;
                    }
                    loopkind = AST_FORATLEASTONCE;
                    if (knownStepVal == 1 && knownStepDir == 1 && !AstUses(body, loopvar) && !HasBranch(body)) {
                        // the loop will execute exactly "reps" times
                        // and the loop variable seems not to be used inside
                        // if it's a local variable, we can switch to count down
                        // PROBLEM: loop variable must be left with correct final value
                        Symbol *sym = LookupAstSymbol(loopvar, NULL);
                        if (sym && (sym->kind == SYM_LOCALVAR||sym->kind == SYM_TEMPVAR) ) {
                            //printf("switch loop to count down");
                                               initstmt = AstAssign(loopvar, AstInteger(toi+1));
                            loopvar = AstTempLocalVariable("_idx_", looptype);
                            fromi = reps+1;
                            toi = 1;
                            knownStepDir = -1;
                            if (IsSpinLang(curfunc->language)) {
                                knownStepVal = 1;
                            } else {
                                knownStepVal = -1;
                            }
                            stepval = AstInteger(-1);
                            fromval = AstInteger(fromi);
                            toval = AstInteger(toi);
                            isUnsignedLoop = 1;
                        }
                    }
                }
            }
        } else {
            // check for loops like repeat i from @label1 to @label2
            // where the actual start and end are not known, but
            // the difference @label2 - @label1 is in fact constant
            AST *delta = AstOperator('-', toval, fromval);
            int32_t deltaval;
            if (IsConstExpr(delta)) {
                deltaval = EvalConstExpr(delta);
                knownStepDir = (deltaval >= 0) ? 1 : -1;
            }
        }
    }

    /* get the loop variable, if we don't already have one */
    if (!loopvar) {
        loopvar = AstTempLocalVariable("_idx_", looptype);
        delayedUpdate = false; // no need to restore this variable
    }

    if (!IsConstExpr(fromval) && AstUses(fromval, loopvar)) {
        AST *initvar = AstTempLocalVariable("_start_", looptype);
        initstmt = AstAssign(loopvar, AstAssign(initvar, fromval));
        fromval = initvar;
    } else {
        if (initstmt) {
            initstmt = NewAST(AST_SEQUENCE, initstmt, AstAssign(loopvar, fromval));
        } else {
            initstmt = AstAssign(loopvar, fromval);
        }
    }
    /* set the limit variable */
    if (IsConstExpr(toval)) {
        if (gl_expand_constants && isIntegerLoop) {
            toval = AstInteger(EvalConstExpr(toval));
        }
    } else if (toval->kind == AST_IDENTIFIER && !AstModifiesIdentifier(body, toval)) {
        /* do nothing, toval is already OK */
    } else {
        limitvar = AstTempLocalVariable("_limit_", looptype);
        initstmt = NewAST(AST_SEQUENCE, initstmt, AstAssign(limitvar, toval));
        toval = limitvar;
    }
    if (delayedUpdate) {
        updatevar = AstTempLocalVariable("_update_", looptype);
    } else {
        updatevar = NULL;
    }
    /* set the step variable */
    if (knownStepVal && knownStepDir) {
        if (knownStepDir < 0 && IsSpinLang(curfunc->language)) {
            if (knownStepVal < 0) {
                // ugh, nasty
                // Spin1 will just terminate the loop early
                if (curfunc->language == LANG_SPIN_SPIN1) {
                    toval = fromval;
                }
            } else {
                stepval = AstOperator(K_NEGATE, NULL, stepval);
                knownStepVal = -knownStepVal;
            }
        }
    } else {
        AST *stepvar = AstTempLocalVariable("_step_", looptype);
        AST *stepdir;
        int op = isUnsignedLoop ? K_GEU : K_GE;
        if (knownStepDir < 0) {
            op = isUnsignedLoop ? K_LTU : '<';
        }
        if (knownStepDir == 0 && knownStepVal == 1 && op == K_GE) {
            stepdir = AstOperator(K_SAR,
                                  AstOperator('-', toval, fromval),
                                  AstInteger(31));
            stepdir = AstOperator('|', stepdir, AstInteger(1));
        } else {
            stepdir = NewAST(AST_CONDRESULT, AstOperator(op, toval, fromval),
                             NewAST(AST_THENELSE, stepval,
                                    AstOperator(K_NEGATE, NULL, stepval)));
        }
        initstmt = NewAST(AST_SEQUENCE, initstmt,
                          AstAssign(stepvar, stepdir));
        stepval = stepvar;
    }

    stepstmt = NULL;
    if (knownStepDir) {
        if (knownStepVal == 1 && isIntegerLoop) {
            stepstmt = AstOperator(K_INCREMENT, loopvar, NULL);
        } else if (knownStepVal == -1 && isIntegerLoop) {
            stepstmt = AstOperator(K_DECREMENT, NULL, loopvar);
        } else if (knownStepVal < 0) {
            stepstmt = AstAssign(loopvar, AstOperator('-', loopvar, AstInteger(-knownStepVal)));
        }
    }
    if (stepstmt == NULL) {
        stepstmt = AstAssign(loopvar, AstOperator('+', loopvar, stepval));
    }

    if (!condtest && testOp && !isUnsignedLoop) {
        // we know how to construct the test
        // we may want to adjust the test slightly though
        condtest = AstOperator(testOp, loopvar, toval);
    }
    
    if (!condtest && knownStepVal == 1) {
        // we can optimize the condition test by adjusting the to value
        // and testing for loopvar != to
        if (IsConstExpr(toval)) {
            if (knownStepDir) {
                testOp = (knownStepDir > 0) ? '+' : '-';
                toval = SimpleOptimizeExpr(AstOperator(testOp, toval, AstInteger(1)));
                /* no limit variable change necessary here */
            } else {
                /* toval is constant, but step isn't, so we need to introduce
                   a variable for the limit = toval + step */
                if (!limitvar) {
                    limitvar = AstTempLocalVariable("_limit_", looptype);
                }
                initstmt = NewAST(AST_SEQUENCE, initstmt, AstAssign(limitvar, SimpleOptimizeExpr(AstOperator('+', toval, stepval))));
                toval = limitvar;
            }
        } else {
            if (!limitvar) {
                limitvar = AstTempLocalVariable("_limit_", looptype);
            }
            initstmt = NewAST(AST_SEQUENCE, initstmt,
                              AstAssign(limitvar,
                                        SimpleOptimizeExpr(
                                            AstOperator('+', toval, stepval))));
            toval = limitvar;
        }
        if (knownStepDir > 0) {
            condtest = AstOperator( isUnsignedLoop ? K_LTU : '<', loopvar, toval);
        } else {
            condtest = AstOperator(K_NE, loopvar, toval);
            if (!(gl_output == OUTPUT_C || gl_output == OUTPUT_CPP)) {
                loopkind = AST_FORATLEASTONCE;
            }
        }
    }

    // special case some unsigned loops
    if (!condtest && isUnsignedLoop) {
        if (IsConstExpr(toval) && knownStepDir) {
            int32_t to_i = EvalConstExpr(toval);
            if (knownStepDir < 0) {
                // count until we wrap around
                if (to_i == 0) {
                    condtest = AstOperator(K_LEU, loopvar, fromval);
                } else if (to_i == 1 && knownStepVal == -1) {
                    condtest = AstOperator(K_NE, loopvar, AstInteger(0));
                } else {
                    condtest = AstOperator(K_GEU, loopvar, toval);
                }
            } else if (knownStepDir > 0) {
                // watch for wrapping around at $FFFF_FFFF
                if (to_i == -1) {
                    condtest = AstOperator(K_GEU, loopvar, fromval);
                } else {
                    condtest = AstOperator(K_LEU, loopvar, toval);
                }
            }
        } else {
            // beware if destination is unknown and stepping using an
            // unsigned variable!
            condtest = NewAST(AST_ISBETWEEN, loopvar,
                              NewAST(AST_RANGE, fromval, toval));
        }
    }
    
    if (!condtest) {
        /* otherwise, we have to just test for loopvar between to and from */
        if (isIntegerLoop) {
            // optimize cases where we know the step direction and value
            if (knownStepDir && knownStepVal) {
                if (knownStepVal > 0) {
                    condtest = AstOperator(K_LE, loopvar, toval);
                } else {
                    condtest = AstOperator(K_GE, loopvar, toval);
                }
            } else {
                condtest = NewAST(AST_ISBETWEEN, loopvar,
                                  NewAST(AST_RANGE, fromval, toval));
            }
        } else if (knownStepDir > 0) {
            condtest = AstOperator(K_LE, loopvar, toval);
        } else if (knownStepDir < 0) {
            condtest = AstOperator(K_GE, loopvar, toval);
        } else {
            condtest = NewAST(AST_ISBETWEEN, loopvar,
                              NewAST(AST_RANGE, fromval, toval));
        }
        if (!(gl_output == OUTPUT_C || gl_output == OUTPUT_CPP)) {
            loopkind = AST_FORATLEASTONCE;
        }
    }

    if (LoopTestAlwaysTrue(condtest)) {
        WARNING(ast, "Loop will never terminate");
    }
    if (delayedUpdate) {
        stepstmt = NewAST(AST_SEQUENCE, AstAssign(updatevar, loopvar), stepstmt);
        finalstmt = AstAssign(loopvar, updatevar);
        finalstmt = NewAST(AST_IF,
                           condtest,
                           NewAST(AST_THENELSE,
                                  NULL,
                                  NewAST(AST_STMTLIST, finalstmt, NULL)));
    }
    stepstmt = NewAST(AST_STEP, stepstmt, body);
    condtest = NewAST(AST_TO, condtest, stepstmt);
    forast = NewAST((enum astkind)loopkind, initstmt, condtest);
    forast->lineidx = origast->lineidx;
    forast->lexdata = origast->lexdata;
    if (finalstmt) {
        forast = NewAST(AST_STMTLIST, forast,
                           NewAST(AST_STMTLIST, finalstmt, NULL));
    }
    AstReportDone(&saveinfo);
    return forast;
}
