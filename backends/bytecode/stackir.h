/*
 * Spin to ByteCode converter
 * Copyright 2018 Total Spectrum Software Inc.
 */

#ifndef STACKIR_H
#define STACKIR_H

// reserve space for 32 "native" instructions
// the rest of the opcode space will be for the most
// commonly occuring sequences

typedef enum stackop {
    // 8 arithmetic; val := pop(), tos := tos OP val
    SOP_ADD,
    SOP_SUB,
    SOP_SHL,
    
    SOP_SHR,
    SOP_SAR,
    SOP_AND,
    
    SOP_OR,
    SOP_XOR,

    // 6 compares: val := pop(), tos := tos CMP val
    SOP_EQ,
    SOP_NE,
    SOP_LT,
    SOP_LE,
    SOP_LTU,
    SOP_LEU,

    // a > b gets done as b < a

    // 6 branches
    
    SOP_JZ,  // val := pop(); if (tos == 0) pc := val; pop()
    SOP_JNZ,
    SOP_JMP, // unconditional jump
    
    // calls; address is on stack
    SOP_CALL, // saves ret address on ret stack
    SOP_CALLASM, // jumps to native code
    SOP_RET, // return from subroutine
    
    // 11 stack access
    
    // push an immediate value
    SOP_PUSHIMM,
    // take an immediate value from stack
    // (pick 0 is the same as dup)
    SOP_PICK,
    // replace down in stack with top of stack, then pop
    // (so put 0 is the same as drop)
    SOP_PUT,
    
    // stores
    // val := pop()
    // mem[val] := tos
    // pop()
    //
    SOP_STOB,
    SOP_STOW,
    SOP_STOL,

    // loads
    // tos := mem[tos]
    SOP_LODB,
    SOP_LODW,
    SOP_LODL,

    // misc
    // access the cog registers
    // operand is 0-32 (16 hw registers, 16 emulated sw registers)
    // note: reg0 is sp, reg1 is object pointer
    SOP_PUSHREG,
    SOP_POPREG,

    // pseudo-ops
    SOP_LABEL,
    
} StackOp;

typedef struct stackir StackIR;

typedef struct stackir {
    /* must match the start of IR in instr.h */
    StackIR *next;
    StackIR *prev;
    
    StackOp opc;
    AST *operand;
} StackIr;

typedef struct StackIRList {
    StackIR *head;
    StackIR *tail;
} StackIRList;

#endif
