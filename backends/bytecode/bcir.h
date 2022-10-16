
#ifndef BCIR_H
#define BCIR_H

#include "spinc.h"
#include "bcbuffers.h"
#include "bc_bedata.h"


#define BYTE_OP_KINDS_XMACRO \
    X(UHHH) /*Error/undefined*/ \
    \
    X(LABEL) /* Virtual jump target opcode */\
    X(NAMEDLABEL) /* For arbitrary GOTO, gets converted to LABEL early on. Also allocated loose as a jump target */\
    \
    X(ALIGN) \
    \
    X(CONSTANT)  /* Push immediate */\
    X(CONSTANT_FUNCREF)  /* Push immediate function reference */\
    X(CONSTANT_DATREF)   /* Push immediate DAT reference */\
    \
    X(POP)  /* Pop N/4 values */\
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
    X(FUNDATA_PUSHADDRESS) /* Absolute address of function-relative labeled data (such as STRING) */ \
    X(FUNDATA_LOOKUPJUMP) /* read(!) value from a jump table with popped index */\
    X(FUNDATA_STRING) \
    X(FUNDATA_JUMPENTRY) \
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
    X(REGBIT_READ) \
    X(REGBIT_WRITE) \
    X(REGBIT_MODIFY) \
    X(REGBITRANGE_READ) \
    X(REGBITRANGE_WRITE) \
    X(REGBITRANGE_MODIFY) \
    X(REGIDX_READ) \
    X(REGIDX_WRITE) \
    X(REGIDX_MODIFY) \
    \
    X(CASE) \
    X(CASE_RANGE)\
    X(CASE_DONE) /* pops hidden case var and then pops jump target */ \
    X(LOOKUP) \
    X(LOOKDOWN) \
    X(LOOKUP_RANGE) \
    X(LOOKDOWN_RANGE) \
    X(LOOKEND) \
    \
    /* Memory builtins */ \
    X(BUILTIN_STRSIZE) \
    X(BUILTIN_STRCOMP) \
    X(BUILTIN_BULKMEM) /* BYTEFILL, LONGMOVE and friends */ \
    \
    X(COGINIT) \
    X(COGINIT_PREPARE) /* Chip calls this "run" */\
    X(COGSTOP) \
    \
    X(LOCKNEW) \
    X(LOCKRET) \
    X(LOCKSET) \
    X(LOCKCLR) \
    \
    X(WAIT) \
    X(CLKSET) /* only relevant on P1 */

enum ByteOpKind {
    #define X(en) BOK_ ## en,
    BYTE_OP_KINDS_XMACRO
    #undef X
};
extern const char *byteOpKindNames[];

#define MATH_OP_KINDS_XMACRO \
    X(UHHH)  /* Error/undefined */ \
    \
    X(__MOD_FIRST)\
    X(MOD_WRITE)  /* ???? */ \
    \
    X(MOD_REPEATSTEP) \
    \
    X(__MODUNARY_FIRST)\
    X(MOD_RANDFORWARD) \
    X(MOD_RANDBACKWARD) \
    \
    X(MOD_SIGNX_BYTE) \
    X(MOD_SIGNX_WORD) \
    \
    X(MOD_PREINC) X(MOD_POSTINC) \
    X(MOD_PREDEC) X(MOD_POSTDEC) \
    \
    X(MOD_POSTCLEAR) X(MOD_POSTSET) \
    X(__MODUNARY_LAST) X(__MOD_LAST)\
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
    X(__MATHUNARY_FIRST)\
    X(NEG) X(BITNOT) X(ABS) \
    X(BOOLNOT) \
    X(SQRT) \
    X(ENCODE) X(DECODE) \
    X(__MATHUNARY_LAST)\

// Also contains other operations applicable to BOK_MEM_MODIFY (MOD_*)
enum MathOpKind {
    #define X(en) MOK_ ## en,
    MATH_OP_KINDS_XMACRO
    #undef X
};
extern const char *mathOpKindNames[];

static inline bool isModOperator(enum MathOpKind opk) {
    return opk > MOK___MOD_FIRST && opk < MOK___MOD_LAST;
}

static inline bool isUnaryOperator(enum MathOpKind opk) {
    return (opk > MOK___MATHUNARY_FIRST && opk < MOK___MATHUNARY_LAST) || (opk > MOK___MODUNARY_FIRST && opk < MOK___MODUNARY_LAST);
}

static inline bool ModOperatorPushesTrueResult(enum MathOpKind opk) {
    switch(opk) {
    case MOK_MOD_PREINC:
    case MOK_MOD_PREDEC:
    case MOK_MOD_WRITE:
        return true;
    default: 
        return !isModOperator(opk);
    }
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
        struct bcir_memop_attr {
            unsigned popIndex:1; //  pop index?
            unsigned pushModifyResult:1; // For BOK_MEM_MODIFY: push result onto stack
            unsigned modifyReverseMath:1; // For BOK_MEM_MODIFY: Swap math operands?
            unsigned repeatPopStep:1; // FOR MOK_MOD_REPEATSTEP
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
            unsigned numResults;
        } call;
        struct {
            enum BCWaitType type;
        } wait;
        struct { // Also used for LOCKNEW/LOCKSET/LOCKCLR
            unsigned pushResult:1;
        } coginit;
        struct {
            unsigned addPbase:1;
            unsigned forJump:1;
        } pushaddress;
        struct {
            unsigned numResults;
        } returninfo;
        struct {
            unsigned logicallyTerminal:1; // For conditional jumps that logically can't fail.
        } condjump;
        struct {
            Module *modref;
        } funcval;
        struct {
            Module *modref;
        } datval;
        int stringLength;
        int labelHiddenVars; // Not used by the actual IR step
    } attr;

    union {
        int32_t int32;
        const char *stringPtr;
    } data;

    struct bcirstruct *jumpTo; // Label that is referenced

    int fixedSize; // If set, the size of this op is known. For BOK_LABEL and other zero-size ops, this is irrelevant

} ByteOpIR;

typedef struct BCIRBuffer {
    ByteOpIR *head,*tail;
    struct BCIRBuffer *pending;
    int opCount;
} BCIRBuffer;

void BIRB_Push(BCIRBuffer *buf,ByteOpIR *ir);
ByteOpIR *BIRB_MakeCopy(ByteOpIR *ir);
ByteOpIR *BIRB_PushCopy(BCIRBuffer *buf,ByteOpIR *ir);
void BIRB_InsertBefore(BCIRBuffer *buf,ByteOpIR *target,ByteOpIR *ir);
void BIRB_AppendPending(BCIRBuffer *buf);

void BCIR_GetJumpOffsetBounds(ByteOpIR *jump,bool func_relative,int *minDist, int *maxDist,int recursionsLeft);
int BCIR_GetJumpOffset(ByteOpIR *jump,bool func_relative);

extern int pbase_offset; // distance of current function from PBASE (obj header)
extern BCIRBuffer *current_birb;

void BCIR_ResolveNamedLabels(BCIRBuffer *irbuf);
void BCIR_Optimize(BCIRBuffer *irbuf);
void BCIR_to_BOB(BCIRBuffer *irbuf,ByteOutputBuffer *bob,int pbase_funoffset);

void BCIR_Init();

static inline bool isPowerOf2(uint32_t x)
{
    return (x & (x-1)) == 0;
}

#endif
