/*
 * Spin to Pasm converter
 * Copyright 2016 Total Spectrum Software Inc.
 * Intermediate representation definitions
 */

#ifndef SPIN_IR_H
#define SPIN_IR_H

typedef struct IR IR;
typedef struct Operand Operand;
typedef struct parserstate ParserState;

enum IROpcode {
    OPC_COMMENT,
    
    OPC_LABEL,
    OPC_BYTE,
    OPC_WORD,
    OPC_LONG,

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
    OPC_NEG,
    OPC_NOT,
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
    
    OPC_UNKNOWN,
};

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
    COND_GT
} IRCond;

struct IR {
    enum IROpcode opc;
    enum IRCond cond;
    Operand *dst;
    Operand *src;
        
    IR *prev;
    IR *next;
};

typedef struct IRList {
    IR *head;
    IR *tail;
} IRList;

enum Operandkind {
    REG_IMM,  // for an immediate value (possibly stored in a register)
    REG_HW,   // for a hardware register
    REG_REG,  // for a regular register
    REG_LOCAL, // for a "local" register (only live inside function)
    REG_ARG,   // for an argument to a function
    REG_LABEL, // for a code label
    REG_STRING,
};

typedef enum Operandkind Operandkind;

struct Operand {
    enum Operandkind kind;
    const char *name;
    int val;
    int used;
};

char *IRAssemble(IRList *list);
bool CompileToIR(IRList *list, ParserState *P);

#endif
