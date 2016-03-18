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

////////////////////////////////////////////////////////////
// "IR" is intermediate represenation used by the optimizer
// most instructions are just passed straight through, but
// some commonly used ones are recognized specially
///////////////////////////////////////////////////////////
// opcodes
// these include pseudo-opcodes for data directives
// and also dummy opcodes used internally by the compiler
typedef enum IROpcode {
    /* various instructions */
    /* note that actual machine instructions must come first */
    OPC_ABS,
    OPC_ADD,
    OPC_AND,
    OPC_ANDN,
    OPC_CALL,
    OPC_CMP,
    OPC_CMPS,
    OPC_COGID,
    OPC_COGSTOP,
    OPC_DJNZ,
    OPC_JUMP,
    OPC_LOCKCLR,
    OPC_LOCKNEW,
    OPC_LOCKRET,
    OPC_LOCKSET,
    OPC_MAXS,
    OPC_MINS,
    OPC_MOV,
    OPC_MUXC,
    OPC_MUXNC,
    OPC_MUXNZ,
    OPC_MUXZ,
    OPC_NEG,
    OPC_NOP,
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
    OPC_TESTN,
    OPC_WAITCNT,
    OPC_WRBYTE,
    OPC_WRLONG,
    OPC_WRWORD,
    OPC_XOR,

    /* an instruction unknown to the optimizer */
    /* this must immediately follow the actual instructions */
    OPC_GENERIC,

    /* like OPC_GENERIC, but is guaranteed not to write its destination */
    OPC_GENERIC_NR,
    
    /* place non-instructions below here */

    /* switch to hub mode */
    OPC_ORGH,

    /* a literal string to place in the output */
    OPC_COMMENT,
    
    /* various assembler declarations */
    OPC_LABEL,
    OPC_BYTE,
    OPC_WORD,
    OPC_LONG,
    OPC_STRING,
    OPC_LABELED_BLOB, // binary blob

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

enum flags {
    // first 8 bits are for various features of the instruction
    FLAG_WZ = 1,
    FLAG_WC = 2,
    FLAG_NR = 4,
    FLAG_WR = 8,
    
    // rest of the bits are used by the optimizer

    FLAG_LABEL_USED = 0x100,
    FLAG_INSTR_NEW  = 0x200,
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

typedef struct OperandList {
  struct OperandList *next;
  Operand *op;
} OperandList;


typedef enum InstrOps {
    NO_OPERANDS,
    SRC_OPERAND_ONLY,
    DST_OPERAND_ONLY,
    TWO_OPERANDS,
    CALL_OPERAND,
    JMPRET_OPERANDS,
} InstrOps;

/* structure describing a PASM instruction */
typedef struct Instruction {
    const char *name;      /* instruction mnemonic */
    uint32_t    binary;    /* binary form of instruction */
    InstrOps    ops;       /* operand forms */
    IROpcode    opc;       /* information for optimizer */
} Instruction;

/* instruction modifiers */
typedef struct instrmodifier {
    const char *name;
    uint32_t modifier;
} InstrModifier;

#define IMMEDIATE_INSTR (1<<22)


/* optimizer friendly form of instructions */
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
    Instruction *instr; // PASM assembler data for instruction
};

void AppendOperand(OperandList **listptr, Operand *op);

#endif
