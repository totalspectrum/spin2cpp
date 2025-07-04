//
// Bytecode (nucode) compiler for spin2cpp
//
// Copyright 2021-2025 Total Spectrum Software Inc.
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

#define PEEP_FLAGS_NONE        0x0000
#define PEEP_FLAGS_MATCH_ARG   0x0001
#define PEEP_FLAGS_MATCH_IMM   0x0002
#define PEEP_FLAGS_MATCH_OP    0x0004
#define PEEP_FLAGS_REPLACE   0x400000
#define PEEP_FLAGS_DONE      0x800000

/* verify that the argument of srcir matches that in origir */
static bool NuMatchArg(NuIr *srcir, NuIr *origir) {
    if (srcir->val != origir->val) return 0;
    return 1;
}
/* verify that the opcode of srcir matches that in origir */
static bool NuMatchOpcode(NuIr *srcir, NuIr *origir) {
    if (srcir->op != origir->op) return 0;
    return 1;
}

/* check for a match, return 0 if none */
/* otherwise do whatever replacement is called for and return 1 */
static int NuMatchPattern(NuIrList *irl, NuPeepholePattern *patrn, NuIr *origir)
{
    int ircount = 0;
    int i;
    unsigned flags;
    NuIr *ir = origir;
    NuIrOpcode opc;
    
    for(;;) {
        flags = patrn->flags;
        if (flags == PEEP_FLAGS_DONE) {
            // printf("PEEP_FLAGS_DONE without PEEP_FLAGS_REPLACE\n");
            return ircount;
        }
        if (flags & PEEP_FLAGS_REPLACE) {
            // match achieved, break out
            break;
        }
        if (!ir) {
            return 0; // no match, ran out of instructions
        }
        if (patrn->op == NU_OP_CBxx) {
            if ( ! (ir->op >= NU_OP_CBEQ && ir->op <= NU_OP_CBGEU) ) {
                return 0;  // failure to match
            }
        } else if (patrn->op == NU_OP_ADD_xBASE) {
            if ( ! (ir->op >= NU_OP_ADD_VBASE && ir->op <= NU_OP_ADD_SUPER) ) {
                return 0;
            }
        } else if (patrn->op != NU_OP_ANY) {
            if (patrn->op != ir->op) {
                return 0;
            }
        }

        // see if we have to match opcode
        if ( (patrn->flags & PEEP_FLAGS_MATCH_OP) ) {
            if (patrn->arg >= ircount || !NuMatchOpcode(ir, match_ops[patrn->arg])) {
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

    /* OK, we have a match, need to replace it */
    /* first, delete the old pattern */
    for (i = 0; i < ircount; i++) {
        ir = origir;
        origir = origir->next;
        NuDeleteIr(irl, ir);
    }
    /* and now insert the new pattern */
    while ( (patrn->flags & PEEP_FLAGS_REPLACE) && !(patrn->flags & PEEP_FLAGS_DONE) ) {
        if (patrn->flags & PEEP_FLAGS_MATCH_OP) {
            opc = match_ops[patrn->arg]->op;
        } else {
            opc = patrn->op;
        }
        ir = NuCreateIrOp(opc);
        if (patrn->flags & PEEP_FLAGS_MATCH_ARG) {
            ir->val = match_ops[patrn->arg]->val;
        } else if (patrn->flags & PEEP_FLAGS_MATCH_IMM) {
            ir->val = patrn->arg;
        }
        NuIrInsertBefore(irl, origir, ir);
        patrn++;
    }

    /* replacement made */
    return 1;
}


//////////////////////////////////////////////////////////////
// Simple patterns with inline replacement
//////////////////////////////////////////////////////////////
// eliminate DUP / DROP sequence
static NuPeepholePattern pat_dup_drop[] = {
    { NU_OP_DUP,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_DROP,      PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* just delete */
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_REPLACE|PEEP_FLAGS_DONE },
    
};

// eliminate PUSH 0 / ADD sequence
static NuPeepholePattern pat_add_0[] = {
    { NU_OP_PUSHI,     0,            PEEP_FLAGS_MATCH_IMM },
    { NU_OP_ADD,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* just delete */
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_REPLACE|PEEP_FLAGS_DONE },
    
};

// eliminate PUSH 1 / BZ sequence
static NuPeepholePattern pat_push_1_bz[] = {
    { NU_OP_PUSHI,     1,            PEEP_FLAGS_MATCH_IMM },
    { NU_OP_BZ,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* just delete */
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_REPLACE|PEEP_FLAGS_DONE },
    
};

// change LDW / SIGNX #15 into LDWS
static NuPeepholePattern pat_ldws[] = { 
    { NU_OP_LDW,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     15,           PEEP_FLAGS_MATCH_IMM },
    { NU_OP_SIGNX,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* replace */
    { NU_OP_LDWS,      PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_DONE },
    
};
// change LDB / SIGNX #7 into LDBS
static NuPeepholePattern pat_ldbs[] = { 
    { NU_OP_LDB,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     7,           PEEP_FLAGS_MATCH_IMM },
    { NU_OP_SIGNX,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* replace */
    { NU_OP_LDBS,      PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_DONE },
    
};

// delete LDWS / SIGNX #15
static NuPeepholePattern pat_ldwss[] = { 
    { NU_OP_LDWS,      PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     15,           PEEP_FLAGS_MATCH_IMM },
    { NU_OP_SIGNX,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* just do the LDWS */
    { NU_OP_LDWS,      PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_DONE },
    
};
// change LDBS / SIGNX #7 into LDBS
static NuPeepholePattern pat_ldbss[] = { 
    { NU_OP_LDBS,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     7,           PEEP_FLAGS_MATCH_IMM },
    { NU_OP_SIGNX,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* just do the LDBS */
    { NU_OP_LDBS,      PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_DONE },
};
// change LDB / AND #255 into LDB
static NuPeepholePattern pat_ldband[] = { 
    { NU_OP_LDB,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     255,           PEEP_FLAGS_MATCH_IMM },
    { NU_OP_AND,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* just do the LDB */
    { NU_OP_LDB,       PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_DONE },
};
// change LDW / AND #$FFFF into LDW
static NuPeepholePattern pat_ldwand[] = { 
    { NU_OP_LDW,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     0xffff,       PEEP_FLAGS_MATCH_IMM },
    { NU_OP_AND,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    /* just do the LDB */
    { NU_OP_LDW,       PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL,   0,            PEEP_FLAGS_DONE },
};

// pattern for INC/DEC
static NuPeepholePattern pat_dec[] = {
    { NU_OP_PUSHI,      1,            PEEP_FLAGS_MATCH_IMM },
    { NU_OP_SUB,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    // replace with
    { NU_OP_DEC,        PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};
static NuPeepholePattern pat_inc[] = {
    { NU_OP_PUSHI,      1,            PEEP_FLAGS_MATCH_IMM },
    { NU_OP_ADD,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    // replace with
    { NU_OP_INC,        PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// pattern for SHL -> DOUBLE
static NuPeepholePattern pat_shl_1[] = {
    { NU_OP_PUSHI,      1,            PEEP_FLAGS_MATCH_IMM },
    { NU_OP_SHL,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    // replace with
    { NU_OP_DOUBLE,     PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// pattern for dup/add -> DOUBLE
static NuPeepholePattern pat_dup_add[] = {
    { NU_OP_DUP,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    // replace
    { NU_OP_DOUBLE,     PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },    
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// pattern for swap/add -> add
static NuPeepholePattern pat_swap_add[] = {
    { NU_OP_SWAP,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    // replace
    { NU_OP_ADD,     PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },    
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};
// pattern for swap/and -> and
static NuPeepholePattern pat_swap_and[] = {
    { NU_OP_SWAP,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_AND,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    // replace
    { NU_OP_AND,     PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },    
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};
// pattern for swap/ior -> ior
static NuPeepholePattern pat_swap_ior[] = {
    { NU_OP_SWAP,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_IOR,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    // replace
    { NU_OP_IOR,     PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },    
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};
// pattern for swap/xor -> xor
static NuPeepholePattern pat_swap_xor[] = {
    { NU_OP_SWAP,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_XOR,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    // replace
    { NU_OP_XOR,     PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },    
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

//////////////////////////////////////////////////////////////
// patterns requiring more sophisticated handling
//////////////////////////////////////////////////////////////

//
// CBXX label ; BRA label2; label:
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
    NuIrOpcode new_opc = NuInvertCondition(ir->op);
    nextir->op = new_opc;
    NuDeleteIr(irl, ir);
    return 1;
}

//
// SWAP; CBxx label
// can become CByy label xx -> reverse(yy) (e.g. <= becomes >=)
//
static NuPeepholePattern pat_swap_cbxx[] = {
    { NU_OP_SWAP,  PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_CBxx,  PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// replace SWAP; CBxx label; -> CBNxx label
//
static int NuReplaceSwapCBxx(int arg, NuIrList *irl, NuIr *ir) {
    NuIr *nextir = ir->next;
    NuIrOpcode old_opc = nextir->op;
    NuIrOpcode new_opc;
    new_opc = NuReverseDirCondition(old_opc);

    nextir->op = new_opc;
    NuDeleteIr(irl, ir);  /* remove the swap */
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

// replace second instruction with "arg"
static int NuReplaceSecond(int arg, NuIrList *irl, NuIr *ir) {
    NuIr *oldir = ir;
    ir = ir->next;
    ir->op = (NuIrOpcode)arg;
    NuDeleteIr(irl, oldir); // delete original pushi
    return 1;
}

// pattern for DJNZ
static NuPeepholePattern pat_djnz[] = {
    { NU_OP_PUSHI,      PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // op 0
    { NU_OP_ADD_xBASE,  PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // op 1
    { NU_OP_LDL,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del op 2
    { NU_OP_DEC,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del op 3
    { NU_OP_DUP,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del op 4
    { NU_OP_PUSHI,      0,            PEEP_FLAGS_MATCH_ARG },    // del op 5
    { NU_OP_ADD_xBASE,  1,            PEEP_FLAGS_MATCH_OP },     // del op 6
    { NU_OP_STL,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // del op 7
    { NU_OP_BNZ,        PEEP_ARG_ANY, PEEP_FLAGS_NONE },         // op 8

    // replace with
    { NU_OP_PUSHI,      0,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_ARG },
    { NU_OP_ADD_xBASE,  1,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_OP  },
    { NU_OP_DJNZ,       8,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_ARG },
    
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// replace ST / LD with DUP / ST
static NuPeepholePattern pat_st_ld[] = {
    { NU_OP_PUSHI,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 0
    { NU_OP_ADD_xBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 1
    { NU_OP_STL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 2
    { NU_OP_PUSHI,     0,            PEEP_FLAGS_MATCH_ARG },  // op 3
    { NU_OP_ADD_xBASE, 1,            PEEP_FLAGS_MATCH_OP },   // op 4
    { NU_OP_LDL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 5

    // replace with
    { NU_OP_DUP,       PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_PUSHI,     0,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_ARG },       // op 0
    { NU_OP_ADD_xBASE, 1,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_OP },       // op 1
    { NU_OP_STL,       PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// replace ST / LD / LD with DUP / ST / LD / SWAP
static NuPeepholePattern pat_st_ld_ld[] = {
    { NU_OP_PUSHI,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 0
    { NU_OP_ADD_xBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 1
    { NU_OP_STL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 2
    { NU_OP_PUSHI,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 3
    { NU_OP_ADD_xBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 4
    { NU_OP_LDL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 5
    { NU_OP_PUSHI,     0,            PEEP_FLAGS_MATCH_ARG },  // op 6
    { NU_OP_ADD_xBASE, 1,            PEEP_FLAGS_MATCH_OP },   // op 7
    { NU_OP_LDL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },       // op 8

    // replace with
    { NU_OP_DUP,       PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_PUSHI,     0,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_ARG },
    { NU_OP_ADD_xBASE, 1,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_OP },
    { NU_OP_STL,       PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    
    { NU_OP_PUSHI,     3,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_ARG },       // op 3
    { NU_OP_ADD_xBASE, 4,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_OP },
    { NU_OP_LDL,       PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },       // op 5

    { NU_OP_SWAP,      PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

// eliminate local ST before RET
static NuPeepholePattern pat_dead_st[] = {
    { NU_OP_PUSHI,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD_DBASE,   PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_STL,         PEEP_ARG_ANY, PEEP_FLAGS_NONE },
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

// eliminate DUP / ST / DROP sequence
// that is, DUP / ST/ DROP -> ST

static NuPeepholePattern pat_dup_st_drop[] = {
    { NU_OP_DUP,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_PUSHI,     PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_ADD_DBASE, PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_STL,       PEEP_ARG_ANY, PEEP_FLAGS_NONE },
    { NU_OP_DROP,      PEEP_ARG_ANY, PEEP_FLAGS_NONE },

    { NU_OP_PUSHI,     1,            PEEP_FLAGS_REPLACE | PEEP_FLAGS_MATCH_ARG },
    { NU_OP_ADD_DBASE, PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    { NU_OP_STL,       PEEP_ARG_ANY, PEEP_FLAGS_REPLACE },
    
    { NU_OP_ILLEGAL, 0, PEEP_FLAGS_DONE }
};

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
    return 1;
}

// list of patterns and functions to invoke when the patterns are matched
struct nupeeps {
    NuPeepholePattern *check;
    int arg;
    int (*replace)(int arg, NuIrList *irl, NuIr *ir);
} nupeep[] = {
    { pat_dup_st_drop, 0, NULL },
    { pat_dup_drop, 0, NULL },
    { pat_add_0, 0, NULL },
    { pat_push_1_bz, 0, NULL },
    { pat_ldws, 0, NULL },
    { pat_ldbs, 0, NULL },
    { pat_ldwss, 0, NULL },
    { pat_ldwand, 0, NULL },
    { pat_ldbss, 0, NULL },
    { pat_ldband, 0, NULL },
    { pat_cbxx, 0, NuReplaceCBxx },
    { pat_swap_cbxx, 0, NuReplaceSwapCBxx },
    { pat_cbnz, NU_OP_BNZ, NuReplaceSecond },
    { pat_cbz,  NU_OP_BZ,  NuReplaceSecond },
    { pat_inc,  0,         NULL },
    { pat_dec,  0,         NULL },
    { pat_djnz,  0, NULL },
    { pat_shl_1, 0, NULL },
    { pat_dup_add, 0, NULL },
    { pat_swap_add, 0, NULL },
    { pat_swap_and, 0, NULL },
    { pat_swap_ior, 0, NULL },
    { pat_swap_xor, 0, NULL },
    { pat_st_ld, 0, NULL },
    { pat_st_ld_ld, 0, NULL },
    { pat_dead_st, 3, NuReplaceWithDrop },
    { pat_ld_ld, 3, NuReplaceDup },
};

//
// Analyze various conditions that the peephole analyzer may care about
// e.g. whether a branch has a single source
//
static void NuScanIr(NuIrList *irl) {
    NuIr *ir;
    NuIrLabel *lab;
    unsigned seqno = 1;

    for(ir = irl->head; ir; ir = ir->next) {
        ir->seqno = seqno++;
        if (ir->op == NU_OP_LABEL && ir->label) {
            ir->label->seqno = ir->seqno;
        }
        else if (ir->op == NU_OP_PUSHA ||
            (ir->op >= NU_OP_BRA && ir->op <= NU_OP_DJNZ)) {
            lab = ir->label;
            if (lab) {
                if (lab->comefrom_valid && lab->comefrom != ir) {
                    lab->comefrom = NULL; // multiple sources
                } else {
                    lab->comefrom = ir;
                    lab->comefrom_valid = true;
                }
            }
        }
    }
}

//
// Nu optimize peephole
// returns 0 if no changes
//
int NuOptimizePeephole(NuIrList *irl) {
    NuIr *ir;
    int match, i;
    int change = 0;

    NuScanIr(irl); /* analyze various conditions */
    ir = irl->head;
    for(;;) {
        while (ir && NuIsDummy(ir)) {
            ir = ir->next;
        }
        if (!ir) break;
        for (i = 0; i < sizeof(nupeep) / sizeof(nupeep[0]); i++) {
#if 0
            if (nupeep[i].check == pat_dec && ir->op == NU_OP_PUSHI && ir->next && ir->next->op == NU_OP_SUB) {
                printf("??\n");
            }
#endif
            match = NuMatchPattern(irl, nupeep[i].check, ir);
            if (match) {
                if (nupeep[i].replace) {
                    // further processing required
                    match = (*nupeep[i].replace)(nupeep[i].arg, irl, ir);
                }
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
    case NU_OP_NEG:
    case NU_OP_INC:
    case NU_OP_DEC:
    case NU_OP_DOUBLE:
        return 0;
    case NU_OP_ADD:
    case NU_OP_SUB:
    case NU_OP_AND:
    case NU_OP_IOR:
    case NU_OP_XOR:
    case NU_OP_SHL:
    case NU_OP_SHR:
    case NU_OP_SAR:
    case NU_OP_MUL:
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
            if (ir->op < NU_OP_DUMMY && ir->op != NU_OP_BRA3) {
                inJumpTable = false;
            }
        } else if (ir->op == NU_OP_JMPREL) {
            inJumpTable = true;
        } else if (ir->op == NU_OP_BRA3) {
            if (!inJumpTable) inDeadCode = true;
        }
    }
    return changes;
}

// check for branch target inside loop
static bool
NuLabelInsideLoop(NuIrLabel *dest, NuIr *start, NuIr *finish) {
    if (!dest) return false;
    return dest->seqno && dest->seqno >= start->seqno && dest->seqno <= finish->seqno;
}
static bool
NuIrInsideLoop(NuIr *dest, NuIr *start, NuIr *finish) {
    if (!dest) return false;
    return dest->seqno && dest->seqno >= start->seqno && dest->seqno <= finish->seqno;
}

// utility function: check for DJNZ variable used within loop
static bool
NoDjnzConflict(NuIrList *irl, NuIr *labelir, NuIr *djnzir, int varOffset)
{
    NuIr *ir;
    
    for (ir = labelir->next; ir != djnzir; ir = ir->next) {
        if (ir->op == NU_OP_PUSHI && ir->val == varOffset && ir->next && ir->next->op == NU_OP_ADD_DBASE) {
            // potential conflict here, unless this is the end of the loop
            if (ir->next->next == djnzir) {
                return true; // all good
            }
            return false;
        }
        // branches out of the loop are bad
        if (ir->op == NU_OP_JMP || ir->op == NU_OP_RET || ir->op == NU_OP_LONGJMP || ir->op == NU_OP_JMPREL) {
            return false;
        }
        // so are calls, for now
        if (ir->op == NU_OP_CALLA || ir->op == NU_OP_CALLM || ir->op == NU_OP_CALL) {
            return false;
        }
        // hmmm, how about labels where someone could jump into the loop?
        // if we know there's a single comefrom and it's inside the loop,
        // it's OK, otherwise bail
        if (ir->op == NU_OP_LABEL) {
            NuIrLabel *lab = ir->label;
            if (lab && lab->comefrom_valid && lab->comefrom) {
                /* single source jump; may be OK */
                if (!NuIrInsideLoop(lab->comefrom, labelir, djnzir))
                    return false; /* not in the loop */
            } else {
                return false;
            }
        }
        // relative branches should be checked for being within the loop
        if (ir->op >= NU_OP_CBEQ && ir->op <= NU_OP_CBGEU) {
            if (!NuLabelInsideLoop(ir->label, labelir, djnzir))
                return false;
        }
    }
    // shouldn't get here, but bail if we do
    return false;
}

//
// convert DJNZ to DJNZ_FAST where we can
//
int NuConvertDjnz(NuIrList *irl)
{
    int changes = 0;
    NuIr *ir;
    NuIrLabel *label;
    NuIr *labelir;
    
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->op == NU_OP_DJNZ) {
            if (ir->prev->op == NU_OP_ADD_DBASE && ir->prev->prev->op == NU_OP_PUSHI && ir->next) {
                int offset = ir->prev->prev->val;
                label = ir->label;
                // look back to find the label
                labelir = ir->prev->prev;
                while (labelir) {
                    if (labelir->op == NU_OP_LABEL && labelir->label == label) break;
                    labelir = labelir->prev;
                }
                if (labelir && NoDjnzConflict(irl, labelir, ir, offset)) {
                    NuIr *newIr;
                    const char *comment1 = ir->prev->prev->comment;
                    const char *comment2 = ir->prev->comment;
                    NuDeleteIr(irl, ir->prev->prev);
                    NuDeleteIr(irl, ir->prev);
                    ir->op = NU_OP_DJNZ_FAST;
                    newIr = NuCreateIrOp(NU_OP_PUSHI);
                    newIr->val = offset;
                    newIr->comment = comment1;
                    NuIrInsertBefore(irl, labelir, newIr);
                    newIr = NuCreateIrOp(NU_OP_ADD_DBASE);
                    newIr->comment = comment2;
                    NuIrInsertBefore(irl, labelir, newIr);
                    newIr = NuCreateIrOp(NU_OP_LDL);
                    NuIrInsertBefore(irl, labelir, newIr);

                    // remember to store the final value back
                    // into memory, too
                    ir = ir->next;
                    newIr = NuCreateIrOp(NU_OP_PUSHI);
                    newIr->val = offset;
                    NuIrInsertBefore(irl, ir, newIr);
                    newIr = NuCreateIrOp(NU_OP_ADD_DBASE);
                    NuIrInsertBefore(irl, ir, newIr);
                    newIr = NuCreateIrOp(NU_OP_STL);
                    NuIrInsertBefore(irl, ir, newIr);
                    changes++;
                }
            }
        }
    }
    return changes;
}

// remove unused labels
static bool
LabelUsed(NuIrList *irl, NuIrLabel *label)
{
    NuIr *ir;
    for (ir = irl->head; ir; ir = ir->next) {
        if (ir->op == NU_OP_LABEL) {
            continue;
        }
        if (ir->label == label) {
            return true;
        }
    }
    return false;
}

int
NuRemoveUnusedLabels(NuIrList *irl)
{
    int changes = 0;
    NuIr *ir;

    ir = irl->head;
    if (ir && ir->op == NU_OP_LABEL) {
        ir = ir->next;
    }
    for (; ir; ir = ir->next) {
        if (ir->op == NU_OP_LABEL && !LabelUsed(irl, ir->label)) {
            NuDeleteIr(irl, ir);
            changes++;
        }
    }
    return changes;
}
