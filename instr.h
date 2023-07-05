/*
 * Spin to Pasm converter
 * Copyright 2016-2023 Total Spectrum Software Inc.
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
    OPC_ADDSX,
    OPC_ADDX,
    OPC_ALTD,
    OPC_ALTS,
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
    OPC_MAXU,
    OPC_MINU,
    OPC_MOV,
    OPC_MOVD,
    OPC_MOVS,
    OPC_MUXC,
    OPC_MUXNC,
    OPC_MUXNZ,
    OPC_MUXZ,
    OPC_NEG,
    OPC_NEGC,
    OPC_NEGNC,
    OPC_NEGNZ,
    OPC_NEGZ,
    OPC_NOP,
    OPC_OR,
    OPC_RDBYTE,
    OPC_RDLONG,
    OPC_RDWORD,
    OPC_RET,
    OPC_REV_P1,
    OPC_REV_P2,
    OPC_RCL,
    OPC_RCR,
    OPC_ROL,
    OPC_ROR,
    OPC_SAR,
    OPC_SHL,
    OPC_SHR,
    OPC_SUB,
    OPC_SUBSX,
    OPC_SUBX,
    OPC_SUMC,
    OPC_SUMNC,
    OPC_SUMNZ,
    OPC_SUMZ,
    OPC_TEST,
    OPC_TESTN,
    OPC_WAITCNT,
    OPC_WRBYTE,
    OPC_WRLONG,
    OPC_WRWORD,
    OPC_XOR,

    /* p2 instructions */
    OPC_ADDCT1,
    OPC_BITC,
    OPC_BITNC,
    OPC_BITH,
    OPC_BITL,
    OPC_BITNOT,
    OPC_BMASK,
    OPC_BREAK,
    OPC_DECOD,
    OPC_DRVC,
    OPC_DRVH,
    OPC_DRVL,
    OPC_DRVNC,
    OPC_DRVNZ,
    OPC_DRVZ,
    OPC_ENCOD,
    OPC_GETBYTE,
    OPC_GETCT,
    OPC_GETNIB,
    OPC_GETQX,
    OPC_GETQY,
    OPC_GETRND,
    OPC_GETWORD,
    OPC_HUBSET,
    OPC_JMPREL,
    OPC_LOCKTRY,
    OPC_LOCKREL,
    OPC_MULS,
    OPC_MULU,
    OPC_NOT,
    OPC_ONES,
    OPC_POP,
    OPC_PUSH,
    OPC_QDIV,
    OPC_QEXP,
    OPC_QFRAC,
    OPC_QLOG,
    OPC_QMUL,
    OPC_QROTATE,
    OPC_QSQRT,
    OPC_QVECTOR,
    OPC_MUXQ,
    OPC_RDPIN,
    OPC_SETBYTE,
    OPC_SETWORD,
    OPC_SETQ,
    OPC_SETQ2,
    OPC_SIGNX,
    OPC_SUBR,
    OPC_TESTB,
    OPC_TESTBN,
    OPC_WAITX,
    OPC_WRC,
    OPC_WRNC,
    OPC_WRNZ,
    OPC_WRZ,
    OPC_ZEROX,
    OPC_REPEAT,
    OPC_REPEAT_END,  // dummy instruction to mark end of repeat loop
    
    /* an instruction unknown to the optimizer */
    /* this must immediately follow the actual instructions */
    OPC_GENERIC,

    /* like OPC_GENERIC, but is guaranteed not to write its destination */
    OPC_GENERIC_NR,

    /* like OPC_GENERIC, but affects the next instruction too */
    OPC_GENERIC_DELAY,

    /* like OPC_GENERIC, but known not to use flags */
    OPC_GENERIC_NOFLAGS,

    /* like OPC_GENERIC_NR, but also ignores flags */
    OPC_GENERIC_NR_NOFLAGS,
    
    /* a branch that the optimizer does not know about */
    OPC_GENERIC_BRANCH,

    /* a branch with condition (like "tjz") where jmp destination is in src field */
    OPC_GENERIC_BRCOND,

    /* place non-instructions below here */
    OPC_PUSH_REGS,   /* pseudo-instruction to save registers on stack */
    OPC_POP_REGS,    /* pseudo-instruction to pop registers off stack */
    
    /* switch to hub mode */
    OPC_HUBMODE,

    /* reset assembler ORG counter; this will only work properly in
       restricted circumstances
    */
    OPC_ORG,

    /* reset ORG and force padding */
    OPC_ORGF,
    
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
    OPC_ALIGNL,    // align to long boundary
    /* pseudo-instruction to load FCACHE */
    OPC_FCACHE,

    /* for compressed instructions */
    OPC_COMPRESS2,
    OPC_COMPRESS3,
    
    /* special flag to indicate a register is used/modified */
    /* used for cases like array accesses where the optimizer may
       not be able to figure it out */
    OPC_LIVE,
    
    /* const declaration */
    OPC_CONST,
    /* indicates an instruction slated for removal */
    OPC_DUMMY,
    
    OPC_UNKNOWN,
} IROpcode;

/* condition for conditional execution */
/* NOTE: These are basically the hardware's bit patterns, but inverted 
 * so COND_TRUE is zero. This allows some useful bit operations. */
typedef enum IRCond {
    COND_TRUE,      // 1111
    COND_C_OR_Z,    // 1110
    COND_C_OR_NZ,   // 1101
    COND_C,         // 1100
    COND_NC_OR_Z,   // 1011
    COND_Z,         // 1010
    COND_C_EQ_Z,    // 1001
    COND_C_AND_Z,   // 1000
    COND_NC_OR_NZ,  // 0111
    COND_C_NE_Z,    // 0110
    COND_NZ,        // 0101
    COND_C_AND_NZ,  // 0100
    COND_NC,        // 0011
    COND_NC_AND_Z,  // 0010
    COND_NC_AND_NZ, // 0001
    COND_FALSE,     // 0000


    COND_LE = COND_C_OR_Z,
    COND_LT = COND_C,
    COND_GE = COND_NC,
    COND_EQ = COND_Z,
    COND_NE = COND_NZ,
    COND_GT = COND_NC_AND_NZ,
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

    // warn if there are no wc, wz markers on the instruction
    FLAG_WARN_NOTUSED = 0x800,
    
    // not exactly an assembler feature, but should not
    // be touched by the optimizer
    FLAG_KEEP_INSTR = 0x1000,

    // to mark jump table instructions
    FLAG_JMPTABLE_INSTR = 0x2000,

    // label is not associated with any jump
    FLAG_LABEL_NOJUMP = 0x4000,

    // instruction came from user inline assembly
    FLAG_USER_INSTR   = 0x8000,
    
    // rest of the bits are used by the optimizer
    FLAG_LABEL_USED = 0x100000,
    FLAG_INSTR_NEW  = 0x200000,
    FLAG_OPTIMIZER = 0xFFF00000,
};

#define FLAG_P1_STD (FLAG_WZ|FLAG_WC|FLAG_NR|FLAG_WR)
#define FLAG_P2_STD (FLAG_WZ|FLAG_WC|FLAG_WCZ)
#define FLAG_P2_JMP (FLAG_P2_STD|FLAG_WR)
#define FLAG_P2_CZTEST (FLAG_WZ|FLAG_WC|FLAG_ANDC|FLAG_ANDZ|FLAG_ORC|FLAG_ORZ|FLAG_XORC|FLAG_XORZ)
#define FLAG_CZSET (FLAG_P2_CZTEST|FLAG_WCZ)
#define FLAG_CSET (FLAG_WC|FLAG_WCZ|FLAG_ANDC|FLAG_ORC|FLAG_XORC)
#define FLAG_ZSET (FLAG_WZ|FLAG_WCZ|FLAG_ANDZ|FLAG_ORZ|FLAG_XORZ)

#define FLAG_JMPSET (FLAG_CZSET|FLAG_WR)

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
    REG_TEMP,  // for a temporary register
    REG_HUBPTR, // a register which holds a hub address
    REG_COGPTR, // a register which holds a cog address
    REG_ARG,   // for an argument to a function
    REG_RESULT, // for a function result
    REG_SUBREG, // for a sub-register: "name" holds the pointer to original reg, "val" is the offset
    
#define IsRegister(kind) ((kind) >= REG_HW && (kind) <= REG_SUBREG)

    // these memory references must go together between LONG_REF and COG_REF
    // "val" is the offset from the register named "name"
    // "size" is the size of the reference
    HUBMEM_REF,      // register indirect memory access; val is the offset
    COGMEM_REF,      // like HUBMEM_REF but is in COG memory
    
    // memory
    STRING_DEF, // data to go in memory
    LONG_DEF,
    WORD_DEF,
    BYTE_DEF,

    // relative reference
    IMM_PCRELATIVE,
};

typedef enum Operandkind Operandkind;

enum OperandEffect {
    OPEFFECT_NONE = 0,
    OPEFFECT_PREDEC = 1,
    OPEFFECT_POSTDEC = 2,
    OPEFFECT_PREINC = 3,
    OPEFFECT_POSTINC = 4,
    OPEFFECT_FORCEABS = 0x100,
    OPEFFECT_FORCEHUB = 0x200,
    OPEFFECT_NOIMM    = 0x400,
    OPEFFECT_DUMMY_ZERO = 0x800,
    
    OPEFFECT_OFFSET_MASK = 0xfffff000,
};

#define OPEFFECT_OFFSET_SHIFT 12

struct Operand {
    enum Operandkind kind;
    const char *name;
    intptr_t val;
    int size; // only really used for MEMREFs
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
    JMP_OPERAND,
    
    /* P2 extensions */
    TWO_OPERANDS_OPTIONAL,  /* one of the 2 operands is optional */
    P2_TJZ_OPERANDS,        /* like TJZ */
    P2_JINT_OPERANDS,       /* like TJZ, but source only */
    P2_RDWR_OPERANDS,       /* like rdlong/wrlong, accepts postinc and such */
    P2_DST_CONST_OK,        /* dst only, but immediate is OK */
    P2_JUMP,                /* jump and call, opcode may change based on dest */
    P2_LOC,		    /* loc instruction, dest may be pa,pb,ptra,ptb */
    P2_CALLD,               /* calld instruction; like loc, but jmp addressess */
    P2_TWO_OPERANDS,        /* two operands, both may be imm */
    P2_DST_TESTP,           /* special flag handling for testp/testpn */
    
    THREE_OPERANDS_NIBBLE,
    THREE_OPERANDS_BYTE,
    THREE_OPERANDS_WORD,

    P2_AUG,
    P2_MODCZ,
    TWO_OPERANDS_DEFZ,
    
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
    IR *next;
    IR *prev;
    enum IROpcode opc;
    enum IRCond cond;
    Operand *dst;
    Operand *src;
    Operand *src2; // for a very few instructions like getword
    int flags;
    unsigned addr;
    void *aux; // auxiliary data for back end
    Instruction *instr; // PASM assembler data for instruction
    enum OperandEffect srceffect; // special effect (e.g. postinc) for source
    enum OperandEffect dsteffect; // special effect for dest
    Operand *fcache;   // if non-NULL, fcache root
    AST *line;         // line number for user error messages
};

void AppendOperand(OperandList **listptr, Operand *op);

// decode the operands of an assembly language instruction
// places the ASTs for the various operands into the operands[] array,
// and immediate bits into opimm[]
int DecodeAsmOperands(Instruction *instr, AST *ast, AST **operand, uint32_t *opimm, uint32_t *val, uint32_t *effectFlags);

#endif
