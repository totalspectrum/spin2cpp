//
// Bytecode (nucode) compiler for spin2cpp
//
// Copyright 2021-2023 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include "outnu.h"
#include "nuir.h"

/////////////////////////////////////////////////////
// nucode peephole optimization
/////////////////////////////////////////////////////

typedef struct NuPeepholePattern {
    NuIrOpcode op;
    intptr_t   arg;
    unsigned   flags;
} NuPeepholePattern;

#define MAX_IR_IN_PATTERN 16
static NuIr *match_ops[MAX_IR_IN_PATTERN];

#define PEEP_ARG_ANY (-1)

#define PEEP_FLAGS_NONE      0x0000
#define PEEP_FLAGS_MATCH_ARG 0x0001
#define PEEP_FLAGS_MATCH_IMM 0x0002
#define PEEP_FLAGS_DONE      0xffff

/* verify that the argument of srcir matches that in origir */
static bool NuMatchArg(NuIr *srcir, NuIr *origir) {
    if (srcir->val != origir->val) return 0;
    return 1;
}

/* check for a match, return 0 if none */
static int NuMatchPattern(NuPeepholePattern *patrn, NuIr *ir)
{
    int ircount = 0;
    unsigned flags;
    for(;;) {
        flags = patrn->flags;
        if (flags == PEEP_FLAGS_DONE) {
            break; // we've matched so far
        }
        if (!ir) {
            return 0; // no match, ran out of instructions
        }
        if (patrn->op == NU_OP_CBxx) {
            if ( ! (ir->op >= NU_OP_CBEQ && ir->op <= NU_OP_CBGEU) ) {
                return 0;  // failure to match
            }
        } else if (patrn->op != NU_OP_ANY) {
            if (patrn->op != ir->op) {
                return 0;
            }
        }

        // see if we have to match an argument
        if ( (patrn->flags & PEEP_FLAGS_MATCH_ARG) ) {
            if (patrn->arg >= ircount || !NuMatchArg(ir, match_ops[patrn->arg])) {
                return 0;
            }
        } else if ( patrn->flags & PEEP_FLAGS_MATCH_IMM ) {
            if (ir->val != patrn->arg) {
                return 0;
            }
        }
        patrn++;
        match_ops[ircount++] = ir;
        ir = ir->next;
    }
    return ircount;
}

//
// simple pattern: CBXX label ; BRA label2; label:
// can become CBNXX label2; label:
//
static NuPeepholePattern pat_cbxx[] = {
    { NU_OP_CBxx, PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_BRA,  PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_LABEL, 0,           PEEP_FLAGS_MATCH_ARG },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// replace CBxx label; BRA label2; LABEL label -> CBNxx label2; LABEL label
static int NuReplaceCBxx(int arg, NuIrList *irl, NuIr *ir) {
    NuIr *nextir = ir->next;
    NuIrOpcode new_opc = NuFlipCondition(ir->op);
    nextir->op = new_opc;
    NuDeleteIr(irl, ir);
    return 1;
}

// replace PUSHI 0; CBNE x -> BNZ x
static NuPeepholePattern pat_cbnz[] = {
    { NU_OP_PUSHI, 0,            PEEP_FLAGS_MATCH_IMM },
    { NU_OP_CBNE,  PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

static NuPeepholePattern pat_cbz[] = {
    { NU_OP_PUSHI, 0,            PEEP_FLAGS_MATCH_IMM },
    { NU_OP_CBEQ,  PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

static int NuReplaceCbnz(int arg, NuIrList *irl, NuIr *ir) {
    NuIr *oldir = ir;
    ir = ir->next;
    ir->op = (NuIrOpcode)arg;
    NuDeleteIr(irl, oldir); // delete original pushi
    return 1;
}

// pattern for DJNZ
static NuPeepholePattern pat_djnz[] = {
    { NU_OP_PUSHI,      PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD_DBASE,  PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_LDL,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del 1
    { NU_OP_PUSHI,      1,            PEEP_FLAGS_MATCH_IMM },    // del 2
    { NU_OP_SUB,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del 3
    { NU_OP_DUP,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del 4
    { NU_OP_PUSHI,      0,            PEEP_FLAGS_MATCH_ARG },    // del 5
    { NU_OP_ADD_DBASE,  PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del 6
    { NU_OP_STL,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del 7
    { NU_OP_BNZ,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

static int NuReplaceDjnz(int arg, NuIrList *irl, NuIr *ir) {
    NuIr *delir;

    // preserve first two ir's (PUSHI, ADD_DBASE)
    ir = ir->next;
    ir = ir->next;

    // delete the next 7 ir's
    for (int i = 0; i < 7; i++) {
        delir = ir;
        ir = ir->next;
        NuDeleteIr(irl, delir);
    }
    // make next ir the djnz
    ir->op = (NuIrOpcode)arg;
    return 1;
}

// replace ST / LD with DUP / ST
static NuPeepholePattern pat_st_ld[] = {
    { NU_OP_PUSHI,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD_DBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_STL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     0,            PEEP_FLAGS_MATCH_ARG },
    { NU_OP_ADD_DBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_LDL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};
static int NuReplaceStLd(int arg, NuIrList *irl, NuIr *ir) {
    NuIr *dupit = NuCreateIrOp(NU_OP_DUP);
    NuIrInsertBefore(irl, ir, dupit);
    ir = ir->next; // go to add_dbase
    ir = ir->next; // go to stl
    NuDeleteIr(irl, ir->next); // delete pushi
    NuDeleteIr(irl, ir->next); // delete add_dbase
    NuDeleteIr(irl, ir->next); // delete ld
    return 1;
}

// eliminate local ST before RET
static NuPeepholePattern pat_dead_st[] = {
    { NU_OP_PUSHI,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD_DBASE,   PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_STL,         PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_RET,         PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};
// replace "arg" instructions starting at ir with a DROP
static int NuReplaceWithDrop(int arg, NuIrList *irl, NuIr *ir) {
    ir->op = NU_OP_DROP;
    --arg;
    while (arg > 0) {
        NuDeleteIr(irl, ir->next);
        --arg;
    }
    return 1;
}

// elimiinate DUP / DROP sequence
static NuPeepholePattern pat_dup_drop[] = {
    { NU_OP_DUP,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_DROP,      PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_DONE }
};
static int NuDeleteInst(int arg, NuIrList *irl, NuIr *ir) {
    while (arg > 1) {
        NuDeleteIr(irl, ir->next);
        --arg;
    }
    NuDeleteIr(irl, ir);
    return 1;
}

// elimiinate DUP / ST / DROP sequence
static NuPeepholePattern pat_dup_st_drop[] = {
    { NU_OP_DUP,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD_DBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_STL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_DROP,      PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};
static int NuDeletePair(int arg, NuIrList *irl, NuIr *ir) {
    NuIr *delir = ir->next;
    while (arg > 0) {
        delir = delir->next;
        --arg;
    }
    NuDeleteIr(irl, delir);
    NuDeleteIr(irl, ir);
    return 1;
}

// turn repeated load into a DUP
static NuPeepholePattern pat_ld_ld[] = {
    { NU_OP_PUSHI,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD_DBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_LDL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     0,            PEEP_FLAGS_MATCH_ARG },
    { NU_OP_ADD_DBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_LDL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// replace a sequence of "arg" instructions following a similar sequence with a DUP
static int NuReplaceDup(int arg, NuIrList *irl, NuIr *ir) {
    NuIr *delir;
    int i;
    for (i = 0; i < arg; i++) {
        ir = ir->next;
    }
    NuIr *dupit = NuCreateIrOp(NU_OP_DUP);
    NuIrInsertBefore(irl, ir, dupit);
    for (i = 0; i < arg; i++) {
        delir = ir;
        ir = ir->next;
        NuDeleteIr(irl, delir);
    }
}
        
// list of patterns and functions to invoke when the patterns are matched
struct nupeeps {
    NuPeepholePattern *check;
    int arg;
    int (*replace)(int arg, NuIrList *irl, NuIr *ir);
} nupeep[] = {
    { pat_dup_st_drop, 3, NuDeletePair },
    { pat_dup_drop, 2, NuDeleteInst },
    { pat_cbxx, 0, NuReplaceCBxx },
    { pat_cbnz, NU_OP_BNZ, NuReplaceCbnz },
    { pat_cbz,  NU_OP_BZ,  NuReplaceCbnz },
    { pat_djnz, NU_OP_DJNZ, NuReplaceDjnz },
    { pat_st_ld, 0, NuReplaceStLd },
    { pat_dead_st, 3, NuReplaceWithDrop },
    { pat_ld_ld, 3, NuReplaceDup },
};

//
// Nu optimize peephole
// returns 0 if no changes
//
int NuOptimizePeephole(NuIrList *irl) {
    NuIr *ir;
    int match, i;
    int change = 0;

    ir = irl->head;
    for(;;) {
        while (ir && NuIsDummy(ir)) {
            ir = ir->next;
        }
        if (!ir) break;
        for (i = 0; i < sizeof(nupeep) / sizeof(nupeep[0]); i++) {
#if 0
            if (nupeep[i].check == pat_djnz && ir->op == NU_OP_PUSHI && ir->next && ir->next->op == NU_OP_ADD_DBASE && ir->next->next && ir->next->next->op == NU_OP_LDL) {
                printf("??\n");
            }
#endif
            match = NuMatchPattern(nupeep[i].check, ir);
            if (match) {
                match = (*nupeep[i].replace)(nupeep[i].arg, irl, ir);
                if (match) {
                    change++;
                    break;
                }
            }
        }
        ir = ir->next;
    }
    return change;
}

#define STACK_CHANGE_UNKNOWN 999999

static int NuStackChange(NuIr *ir) {
    switch (ir->op) {
    case NU_OP_PUSHI:
        return 1;
    case NU_OP_ADD_DBASE:
    case NU_OP_LDL:
        return 0;
    case NU_OP_ADD:
    case NU_OP_SUB:
        return -1;
    case NU_OP_STL:
        return -2;
    default:
        return STACK_CHANGE_UNKNOWN;
    }
}

//
// some other peephole style optimization functions which don't quite match the general paradigm
//
int NuRemoveDupDrop(NuIrList *irl) {
    NuIr *ir;
    NuIr *subir;
    int stackLevel, delta;
    int changes = 0;
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->op == NU_OP_DUP) {
            stackLevel = 1;
            for (subir = ir->next; subir; subir = subir->next) {
                if (subir->op == NU_OP_DROP) {
                    if (stackLevel == 0) {
                        NuDeleteIr(irl, subir);
                        NuDeleteIr(irl, ir);
                        changes++;
                        break;
                    }
                }
                delta = NuStackChange(subir);
                if (delta == STACK_CHANGE_UNKNOWN) {
                    stackLevel = delta;
                    break;
                }
                stackLevel += delta;
            }
        }
    }
    return changes;
}

//
// remove unreachable code
//
int NuRemoveDeadCode(NuIrList *irl)
{
    int changes = 0;
    NuIr *ir;
    bool inDeadCode = false;
    bool inJumpTable = false;

    for (ir = irl->head; ir; ir = ir->next) {
        if (inDeadCode) {
            if (ir->op < NU_OP_DUMMY) {
                ir->op = NU_OP_DUMMY;
                changes++;
            } else if (ir->op == NU_OP_LABEL) {
                inDeadCode = false;
            }
        } else if (inJumpTable) {
            if (ir->op < NU_OP_DUMMY && ir->op != NU_OP_BRA) {
                inJumpTable = false;
            }
        } else if (ir->op == NU_OP_JMPREL) {
            inJumpTable = true;
        } else if (ir->op == NU_OP_BRA) {
            if (!inJumpTable) inDeadCode = true;
        }
    }
    return changes;
}
