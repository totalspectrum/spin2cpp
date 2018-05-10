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
    OPC_JMPRET,
    OPC_LOCKCLR,
    OPC_LOCKNEW,
    OPC_LOCKRET,
    OPC_LOCKSET,
    OPC_MAXS,
    OPC_MINS,
    OPC_MOV,
    OPC_MOVD,
    OPC_MOVS,
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
    OPC_REV_P1,
    OPC_REV_P2,
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

    /* p2 instructions */
    OPC_ADDCT1,
    OPC_HUBSET,
    
    /* an instruction unknown to the optimizer */
    /* this must immediately follow the actual instructions */
    OPC_GENERIC,

    /* like OPC_GENERIC, but is guaranteed not to write its destination */
    OPC_GENERIC_NR,

    /* a branch that the optimizer does not know about */
    OPC_GENERIC_BRANCH,
    
    /* place non-instructions below here */

    /* switch to hub mode */
    OPC_HUBMODE,

    /* reset assembler ORG counter; this will only work properly in
       restricted circumstances
    */
    OPC_ORG,

    /* make sure code fits in a specific size */
    OPC_FIT,
    
    /* a literal string to place in the output */
    OPC_LITERAL,

    /* a comment to output */
    OPC_COMMENT,
    
    /* various assembler declarations */
    OPC_LABEL,
    OPC_BYTE,
    OPC_WORD,
    OPC_LONG,
    OPC_STRING,
    OPC_LABELED_BLOB, // binary blob
    OPC_RESERVE,   // reserve space in cog
    OPC_RESERVEH,  // reserve space in hub
    
    /* pseudo-instruction to load FCACHE */
    OPC_FCACHE,
    
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
    FLAG_WCZ = 0x10,
    FLAG_ANDC = 0x20,
    FLAG_ANDZ = 0x40,
    FLAG_ORC = 0x80,
    FLAG_ORZ = 0x100,
    FLAG_XORC = 0x200,
    FLAG_XORZ = 0x400,
    
    // not exactly an assembler feature, but should not
    // be touched by the optimizer
    FLAG_KEEP_INSTR = 0x800,

    // rest of the bits are used by the optimizer

    FLAG_LABEL_USED = 0x1000,
    FLAG_INSTR_NEW  = 0x2000,
    FLAG_OPTIMIZER = 0xFFF000,
};

#define FLAG_P1_STD (FLAG_WZ|FLAG_WC|FLAG_NR|FLAG_WR)
#define FLAG_P2_STD (FLAG_WZ|FLAG_WC|FLAG_WCZ)
#define FLAG_P2_CZTEST (FLAG_WZ|FLAG_WC|FLAG_ANDC|FLAG_ANDZ|FLAG_ORC|FLAG_ORZ|FLAG_XORC|FLAG_XORZ)

typedef struct IRList {
    IR *head;
    IR *tail;
} IRList;

enum Operandkind {
    IMM_INT,  // for an immediate value (possibly stored in a register)
    IMM_COG_LABEL, // an immediate holding a memory address
    IMM_HUB_LABEL, // ditto, but for HUB rather than COG memory
    IMM_STRING, // a string to print or store
    IMM_BINARY, // a dat section, including relocations
    
    REG_HW,   // for a hardware register
    REG_REG,  // for a regular register
    REG_LOCAL, // for a "local" register (only live inside function)
    REG_HUBPTR, // a register which holds a hub address
    REG_COGPTR, // a register which holds a cog address
    REG_ARG,   // for an argument to a function
    
#define IsRegister(kind) ((kind) >= REG_HW && (kind) <= REG_ARG)

    // all of these memory references must go together
    LONG_REF,      // register indirect memory access; val is the offset
    WORD_REF,
    BYTE_REF,
    COG_REF,       // like LONG_REF but is in COG memory
    
    // memory
    STRING_DEF, // data to go in memory
    LONG_DEF,
    WORD_DEF,
    BYTE_DEF,
};

typedef enum Operandkind Operandkind;

enum OperandEffect {
    OPEFFECT_NONE,
    OPEFFECT_PREDEC,
    OPEFFECT_POSTDEC,
    OPEFFECT_PREINC,
    OPEFFECT_POSTINC,
};

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

    /* P2 extensions */
    TWO_OPERANDS_OPTIONAL,  /* one of the 2 operands is optional */
    P2_TJZ_OPERANDS,        /* like TJZ */
    P2_JINT_OPERANDS,       /* like TJZ, but source only */
    P2_RDWR_OPERANDS,       /* like rdlong/wrlong, accepts postinc and such */
    P2_DST_CONST_OK,        /* dst only, but immediate is OK */
    P2_JUMP,                /* jump and call, opcode may change based on dest */
    P2_LOC,		    /* like JUMP, but no relative version */
    P2_TWO_OPERANDS,        /* two operands, both may be imm */
    P2_DST_TESTP,           /* special flag handling for testp/testpn */
    
    THREE_OPERANDS_NIBBLE,
    THREE_OPERANDS_BYTE,
    THREE_OPERANDS_WORD,

    P2_AUG,
    P2_MODCZ,
    
} InstrOps;


/* structure describing a PASM instruction */
typedef struct Instruction {
    const char *name;      /* instruction mnemonic */
    uint32_t    binary;    /* binary form of instruction */
    InstrOps    ops;       /* operand forms */
    IROpcode    opc;       /* information for optimizer */
    uint32_t    flags;     /* allowable flags */
} Instruction;

/* instruction modifiers */
typedef struct instrmodifier {
    const char *name;
    uint32_t modifier; // bits to modify in the instruction
    uint32_t flags;    // internal meaning (0 for condition codes)
} InstrModifier;

#define IMMEDIATE_INSTR (1<<22)
#define BIGIMM_INSTR    (-1)

#define P2_IMM_DST (1<<19)
#define P2_IMM_SRC (1<<18)

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
    enum OperandEffect srceffect; // special effect (e.g. postinc) for source
    Operand *fcache;   // if non-NULL, fcache root
};

void AppendOperand(OperandList **listptr, Operand *op);

#endif
