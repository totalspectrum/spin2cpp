/*
 * Spin to Pasm converter
 * Copyright 2016 Total Spectrum Software Inc.
 * Intermediate representation definitions
 */

#ifndef SPIN_IR_H
#define SPIN_IR_H

#include "util/flexbuf.h"

// forward definitions
typedef struct IR IR;
typedef struct Operand Operand;
typedef struct modulestate Module;

// opcodes
// these include pseudo-opcodes for data directives
// and also dummy opcodes used internally by the compiler
typedef enum IROpcode {
    OPC_COMMENT,
    
    OPC_LABEL,
    OPC_BYTE,
    OPC_WORD,
    OPC_LONG,
    OPC_STRING,
    OPC_BLOB, // binary blob

    /* various instructions */
    OPC_MOVE,
    OPC_ABS,
    OPC_ADD,
    OPC_AND,
    OPC_ANDN,
    OPC_CALL,
    OPC_CMP,
    OPC_CMPS,
    OPC_DJNZ,
    OPC_JUMP,
    OPC_MAXS,
    OPC_MINS,
    OPC_NEG,
    OPC_OR,
    OPC_RDBYTE,
    OPC_RDLONG,
    OPC_RDWORD,
    OPC_RET,
    OPC_REV,
    OPC_ROL,
    OPC_ROR,
    OPC_SAR,
    OPC_SHL,
    OPC_SHR,
    OPC_SUB,
    OPC_TEST,
    OPC_WAITCNT,
    OPC_WAITPEQ,
    OPC_WAITPNE,
    OPC_WAITVID,
    OPC_WRBYTE,
    OPC_WRLONG,
    OPC_WRWORD,
    OPC_XOR,

    /* special flag to indicate a dead register */
    OPC_DEAD,
    /* const declaration */
    OPC_CONST,
    /* indicates an instruction slated for removal */
    OPC_DUMMY,
    
    OPC_UNKNOWN,
} IROpcode;

/* condition for conditional execution */
/* NOTE: opposite conditions must go together
 * in pairs so that InvertCond can easily
 * find the opposite of any condition
 */
typedef enum IRCond {
    COND_TRUE,
    COND_FALSE,
    COND_LT,
    COND_GE,
    COND_EQ,
    COND_NE,
    COND_LE,
    COND_GT,

    COND_C,
    COND_NC,
} IRCond;

struct IR {
    enum IROpcode opc;
    enum IRCond cond;
    Operand *dst;
    Operand *src;
    int flags;
    IR *prev;
    IR *next;
    unsigned addr;
    void *aux; // auxiliary data for back end
};

enum flags {
    // first 8 bits are for various features of the instruction
    FLAG_WZ = 1,
    FLAG_WC = 2,
    FLAG_NR = 4,

    // rest of the bits are used by the optimizer

    FLAG_LABEL_USED = 0x100,
    FLAG_OPTIMIZER = 0xFFFF00,
};

typedef struct IRList {
    IR *head;
    IR *tail;
} IRList;

enum Operandkind {
    IMM_INT,  // for an immediate value (possibly stored in a register)
    IMM_COG_LABEL, // an immediate holding a memory address
    IMM_HUB_LABEL, // ditto, but for HUB rather than COG memory
    IMM_STRING, // a string to print or store

    REG_HW,   // for a hardware register
    REG_REG,  // for a regular register
    REG_LOCAL, // for a "local" register (only live inside function)
    REG_HUBPTR, // a register which holds a hub address
    REG_ARG,   // for an argument to a function

#define IsRegister(kind) ((kind) >= REG_HW && (kind) <= REG_ARG)

    // all of these memory references must go together
    LONG_REF,      // register indirect memory access; val is the offset
    WORD_REF,
    BYTE_REF,

    // memory
    STRING_DEF, // data to go in memory
    LONG_DEF,
    WORD_DEF,
    BYTE_DEF,
};

typedef enum Operandkind Operandkind;

struct Operand {
    enum Operandkind kind;
    const char *name;
    intptr_t val;
    int used;
};

//
// functions for manipulating IR lists
//

// append an IR at the end of a list
void AppendIR(IRList *irl, IR *ir);
// insert an IR after another in a list
void InsertAfterIR(IRList *irl, IR *orig, IR *ir);
void DeleteIR(IRList *irl, IR *ir);
void AppendIRList(IRList *irl, IRList *sub);
void ReplaceIRWithInline(IRList *irl, IR *ir, Function *func);

//
// functions for operand manipulation
//
Operand *NewOperand(enum Operandkind, const char *name, int val);
Operand *NewImmediate(int32_t val);

// utility functions
IRCond InvertCond(IRCond v);

// function to convert an IR list into a text representation of the
// assembly
char *IRAssemble(IRList *list);

// create an IR list from a module definition
bool CompileToIR(IRList *list, Module *P);

// optimization functions
void OptimizeIRLocal(IRList *irl);
void OptimizeIRGlobal(IRList *irl);
bool ShouldBeInlined(Function *f);
int  ExpandInlines(IRList *irl);

//
// back end data for functions
//
typedef struct ir_bedata {
    /* temporary register info */
    int curtempreg;
    int maxtempreg;

    /* assembly output name */
    Operand *asmname;
    Operand *asmretname;

    /* instructions for this function */
    IRList irl;

    /* flag for whether we should inline the function */
    bool isInline;
} IRFuncData;

#define FuncData(f) ((IRFuncData *)(f)->bedata)
#define FuncIRL(f)  (&FuncData(f)->irl)

#endif
