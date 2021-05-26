
#ifndef BCIR_H
#define BCIR_H

#include "spinc.h"
#include "bcbuffers.h"


#define BYTE_OP_KINDS_XMACRO \
    X(UHHH) /*Error/undefined*/ \
    \
    X(LABEL) /* Virtual jump target opcode */\
    \
    X(CONSTANT)  /* Push immediate */\
    X(POP)  /* Pop value */\
    \
    X(RETURN_PLAIN)  /* Plain return (keep set results)*/\
    X(RETURN_POP)  /* Pop return */\
    X(ABORT_PLAIN)  /* Plain abort */\
    X(ABORT_POP)  /* Pop abort */\
    \
    X(MEM_READ) \
    X(MEM_WRITE) \
    X(MEM_MODIFY)  /* What Chip calls "assign" */\
    X(MEM_ADDRESS)  /* Push effective address */\
    \
    X(ANCHOR) /* Set up stack frame for call */\
    \
    X(CALL_SELF)  /* Call method in current object */\
    X(CALL_OTHER)  /* Call method in other object */\
    X(CALL_OTHER_IDX)  /* Call method in other object from obj array (pop index) */\
    \
    X(JUMP)  /* Unconditional jump */\
    X(JUMP_TJZ) \
    X(JUMP_DJNZ) \
    X(JUMP_IF_Z) \
    X(JUMP_IF_NZ) \
    \
    X(MATHOP) \
    \
    /* Register stuffs */ \
    X(REG_READ) \
    X(REG_WRITE) \
    X(REG_MODIFY) \
    X(REGBITS_READ) \
    X(REGBITS_WRITE) \
    X(REGBITS_MODIFY) \
    \
    /* Memory builtins */ \
    X(BUILTIN_STRSIZE) \
    X(BUILTIN_STRCOMP) \
    X(BUILTIN_BULKMEM) /* BYTEFILL, LONGMOVE and friends */ \
    \
    X(WAIT)

enum ByteOpKind {
    #define X(en) BOK_ ## en,
    BYTE_OP_KINDS_XMACRO
    #undef X
};
extern const char *byteOpKindNames[];

#define MATH_OP_KINDS_XMACRO \
    X(UHHH)  /* Error/undefined */ \
    \
    X(MOD_WRITE)  /* ???? */ \
    \
    X(MOD_REPEATN)  /* DJNZ? */ \
    \
    X(MOD_SIGNX) \
    \
    X(MOD_PREINC) X(MOD_POSTINC) \
    X(MOD_PREDEC) X(MOD_POSTDEC) \
    \
    X(MOD_POSTCLEAR) X(MOD_POSTSET) \
    \
    /* binary ops */ \
    X(ROR) X(ROL) X(SHR) X(SHL) X(SAR) X(REV) \
    X(ADD) X(SUB) \
    X(BITAND) X(BITOR) X(BITXOR) \
    X(LOGICAND) X(LOGICOR) /* Named as per the AST operators. These don't short-circuit (duh) */ \
    /* TODO: Signed versions? */ \
    X(MULLOW) X(MULHIGH)  \
    X(DIVIDE) X(REMAINDER) \
    X(MIN) X(MAX) \
    X(CMP_B) X(CMP_A) X(CMP_NE) X(CMP_E) X(CMP_BE) X(CMP_AE) \
    \
    /* unary ops */ \
    X(NEG) X(BITNOT) X(ABS) \
    X(BOOLNOT) \
    X(SQRT) \
    X(ENCODE) X(DECODE) 

// Also contains other operations applicable to BOK_MEM_MODIFY (MOD_*)
enum MathOpKind {
    #define X(en) MOK_ ## en,
    MATH_OP_KINDS_XMACRO
    #undef X
};
extern const char *mathOpKindNames[];

static inline bool isModOperator(enum MathOpKind opk) {
    return opk <= MOK_MOD_POSTSET && opk != MOK_UHHH;
}

enum BCWaitType {
    BCW_UHHH,
    BCW_WAITPEQ, BCW_WAITPNE,
    BCW_WAITCNT,
    BCW_WAITVID,
};

typedef struct bcirstruct {
    struct bcirstruct *next,*prev;
    enum ByteOpKind kind;
    enum MathOpKind mathKind;
    union {
        struct {
            unsigned popIndex:1; //  pop index?
            unsigned pushModifyResult:1; // For BOK_MEM_MODIFY: push result onto stack
            unsigned modifyReverseMath:1; // For BOK_MEM_MODIFY: Swap math operands?
            #define MEMOP_BASE_POP   0
            #define MEMOP_BASE_PBASE 1 // DAT?
            #define MEMOP_BASE_VBASE 2 // VAR
            #define MEMOP_BASE_DBASE 3 // Stack
            unsigned base:2;
            #define MEMOP_SIZE_BIT  0
            #define MEMOP_SIZE_BYTE 1
            #define MEMOP_SIZE_WORD 2
            #define MEMOP_SIZE_LONG 3
            unsigned memSize:2;
            unsigned modSize:2; // In Spin1, the size of modify ops is independent, oddly enough.
        } memop;
        struct {
            #define BULKMEM_SIZE_BYTE 1
            #define BULKMEM_SIZE_WORD 2
            #define BULKMEM_SIZE_LONG 3
            unsigned memSize:2;
            unsigned isMove:1; // Fill otherwise
        } bulkmem;
        struct {
            unsigned rescue:1;
            unsigned withResult:1;
        } anchor;
        struct {
            int16_t funID,objID;
        } call;
        struct {
            enum BCWaitType type;
        } wait;
    } attr;

    union {
        struct bcirstruct *jumpTo;
        int32_t int32;
    } data;

    int fixedSize; // If set, the size of this op is known. For BOK_LABEL and other zero-size ops, this is irrelevant

} ByteOpIR;

typedef struct {
    ByteOpIR *head,*tail;
    int opCount;
} BCIRBuffer;

void BIRB_Push(BCIRBuffer *buf,ByteOpIR *ir);
ByteOpIR *BIRB_PushCopy(BCIRBuffer *buf,ByteOpIR *ir);

void BCIR_to_BOB(BCIRBuffer *irbuf,ByteOutputBuffer *bob);

void BCIR_Init();

#endif