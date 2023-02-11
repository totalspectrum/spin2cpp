#ifndef NUIR_H
#define NUIR_H

#include <stdint.h>
#include <stdlib.h>

// Nu interpreter intermediate representation

#define NU_OP_XMACRO \
    X(ILLEGAL) /* illegal instruction: must always come first */ \
    X(LDB)     /* load byte */ \
    X(LDW)     /* load word */ \
    X(LDL)     /* load long */ \
    X(LDD)     /* load double */ \
    X(STB)     /* store byte */ \
    X(STW)     /* store word */ \
    X(STL)     /* store long */ \
    X(STD)     /* store double */ \
    X(LDREG)   /* load register (long) */ \
    X(STREG)   /* store register (long) */ \
    \
    X(ADD_VBASE) /* add object base to tos */ \
    X(ADD_DBASE) /* add frame pointer to tos */ \
    X(ADD_SUPER) /* add super pointer to tos */ \
    X(ADD_SP)    /* add stack pointer to tos */ \
    X(ADD_PC)    /* add program counter to tos */ \
    X(SET_SP)    /* set stack pointer to tos */ \
    /* BINARY math operations start here */     \
    X(ADD)       /* tos := nos + tos */ \
    X(SUB)       /* tos := nos - tos */ \
    X(AND)       /* tos := nos & tos */ \
    X(IOR)        /* tos := nos | tos */ \
    X(XOR)       /* tos := nos ^ tos */ \
    \
    X(MINS)      /* tos := min(nos, tos) (signed) */ \
    X(MAXS)      /* tos := max(nos, tos) (signed) */ \
    X(MINU)      /* tos := min(nos, tos) (unsigned) */ \
    X(MAXU)      /* tos := max(nos, tos) (unsigned) */ \
    \
    X(SIGNX)     /* sign extend tos := nos SIGNX tos */ \
    X(ZEROX)     /* zero extend tos := nos ZEROX tos */ \
    X(SHL)       /* tos := nos << tos */ \
    X(SHR)       /* tos := nos >> tos (unsigned right shift) */ \
    X(SAR)       /* tos := nos SAR tos (signed right shift) */ \
    X(ROL)       /* tos := nos ROL tos */ \
    X(ROR)       /* tos := nos ROR tos */ \
    X(MOVBYTS)   /* execute MOVBYTS tos, nos */ \
    X(MUL)       /* 32 bit multiply: tos := tos * nos */ \
    \
    /* BINARY math operations end here (ones below leave 2 items on stack */ \
    \
    X(MULU)      /* unsigned multiply: leaves low, high on stack */ \
    X(MULS)      /* signed multiply:   leaves low, high on stack */ \
    X(DIVU)      /* unsigned divide: leaves quotient, remainder on stack */ \
    X(DIVS)      /* signed divide:   leaves quotient, remainder on stack */ \
    X(DIV64)     /* calculate lo, hi / n, leaves quotient, remainder on stack */ \
    X(MULDIV64)  /* calculate a*b/c with full precision */ \
    X(SQRT64)    /* calculate sqrt(nos, tos) with full precision */ \
    X(ROTXY)     /* rotate x, y */ \
    X(XYPOL)     /* convert x, y to polar */ \
    \
    /* UNARY math operations start here */ \
    X(NEG)       /* negate tos */ \
    X(NOT)       /* bit complement tos */ \
    X(ABS)       /* abs value tos */ \
    X(REV)       /* reverse bits of tos */ \
    X(INC)       /* tos := tos+1 */ \
    X(DEC)       /* tos := tos-1 */ \
    X(DOUBLE)    /* tos := tos*2 */ \
    X(X4)        /* tos := tos*4 */                \
    X(ONES)      /* count one bits of tos */ \
    X(MERGEW)    /* merge words (pit permute) */ \
    X(SPLITW)    /* split words (pit permute) */ \
    X(MERGEB)    /* merge bytes (pit permute) */ \
    X(SPLITB)    /* split bytes (pit permute) */ \
    X(SEUSSF)    /* scramble bits forward */ \
    X(SEUSSR)    /* scramble bits reverse */ \
    X(QEXP)      /* exp2 */ \
    X(QLOG)      /* log2 */ \
    X(ENCODE)    /* tos := ENCODE tos (0-32) */ \
    X(ENCODE2)   /* tos := ENCODE tos (0-31) */ \
    /* UNARY math operations end here */ \
    \
    X(ADD64)     /* 64 bit add of 4 items on stack */ \
    X(SUB64)     /* 64 bit add of 4 items on stack */ \
    X(CMP64S)    /* 64 bit add of 4 items on stack */      \
    X(CMP64U)    /* 64 bit add of 4 items on stack */ \
    \
    X(DROP)      /* drop element on top of stack */ \
    X(DROP2)     /* drop two elements on top of stack */ \
    X(DUP)       /* dup element on top of stack: A B -> A B B */ \
    X(DUP2)      /* dup 2 elements on top of stack: A B -> A B A B */ \
    X(OVER)      /* changes stack from A B -> A B A */ \
    X(SWAP)      /* swap tos and nos: A B -> B A */ \
    X(SWAP2)     /* swap 2 elements: A B C -> C A B */ \
    X(HALT)      /* halt processor */ \
    \
    X(ENTER)     /* enter subroutine */ \
    \
    X(DRVH)      /* set pin high */ \
    X(DRVL)      /* set pin low */  \
    X(DRVNOT)    /* toggle pin  */  \
    X(DRVRND)    /* set pin to random value */ \
    X(DRVWR)     /* write value in nos to pin at tos */ \
    X(FLTL)      /* float pin low */  \
    X(DIRL)      /* float pin low */  \
    X(DIRH)      /* float pin low */  \
    \
    X(PINR)      /* read a pin */ \
    \
    X(WRPIN)     /* smart pin write */ \
    X(WXPIN)     /* smart pin write */ \
    X(WYPIN)     /* smart pin write */ \
    X(RDPINX)    /* smart pin read: returns value and c */ \
    X(RQPIN)     /* smart pin read (quiet) */ \
    X(AKPIN)     /* smart pin acknowledge */ \
    \
    X(GETCTHL)   /* get current cycle counter */ \
    X(GETHEAP)   /* get current heap base */ \
    X(WAITX)     /* delay for tos cycles */ \
    X(WAITCNT)   /* wait for a particular cycle */ \
    X(HUBSET)    /* hubset instruction */ \
    X(COGSTOP)   /* stop a particular cog */ \
    X(COGATN)    /* raise attention */ \
    X(POLLATN)   /* check attention */ \
    X(COGID)     /* get current COG id */ \
    X(COGINIT)   /* launch new COG */ \
    X(LOCKMEM)   /* lock using a memory location instead of hw lock */ \
    X(LOCKNEW)   /* allocate a new hw lock */ \
    X(LOCKRET)   /* free a hw lock */ \
    X(LOCKTRY)   /* try to get a hw lock */ \
    X(LOCKSET)   /* try to get a hw lock (P1 style return value) */ \
    X(LOCKCLR)   /* try to free a hw lock */ \
    \
    X(XORO)    /* xoro32 random number generation */ \
    \
    X(PUSHI)     /* push immediate 32 bit */ \
    X(PUSHA)     /* push address */ \
    \
    X(BYTECOPY)   /* does memmove(a, b, n) */ \
    \
    X(INLINEASM)  /* load inline assembly */ \
    X(SETJMP)    /* setjmp(buf) -> r,s  r is result, s is 0 for original, 1 for longjmp */ \
    X(BREAK)     /* debug break instruction */ \
    \
    /* put all branches together here at the end */ \
    X(JMP)       /* jump to address at tos */ \
    X(RET)       /* return from subroutine */ \
    X(CALL)      /* call subroutine */ \
    X(CALLM)     /* call method (changes object pointer) */ \
    X(CALLA)     /* call subroutine direct */ \
    X(GOSUB)     /* combo CALL+ENTER */ \
    X(LONGJMP)   /* longjmp(buf, n, nocatch_flag): sets n as return value from setjmp */ \
    \
    X(BRA)       /* branch always */ \
    X(JMPREL)    /* jump forward relative based on table */ \
    X(CBEQ)      /* compare branch if equal */ \
    X(CBNE)      /* compare branch if not equal */ \
    X(BZ)        /* branch if tos is zero */ \
    X(BNZ)       /* branch if tos is non-zero */ \
    X(DJNZ)      /* decrement *tos, branch if result non-zero */ \
    X(CBLTS)     /* compare branch if < signed */ \
    X(CBLES)     /* compare branch if <= signed */ \
    X(CBLTU)     /* compare branch if < unsigned */ \
    X(CBLEU)     /* compare branch if <= unsigned */ \
    X(CBGTS)     /* compare branch if > signed */ \
    X(CBGES)     /* compare branch if >= signed */ \
    X(CBGTU)     /* compare branch if > unsigned */ \
    X(CBGEU)     /* compare branch if >= unsigned */ \
    \
    X(DUMMY)     /* this and everything following needs no code */ \
    X(COMMENT)   /* just a comment */ \
    X(ALIGN)     /* align data */ \
    X(LABEL)     /* label for jump target */ \
    X(CBxx)      /* matches any CBxx instruction */ \
    X(MathBinary) /* matches any binary math operations */ \
    X(MathUnary)  /* matches any unary math operations */ \
    X(ADD_xBASE)  /* matches ADD_VBASE or ADD_DBASE */ \
    X(ANY)        /* matches any opcode */ \


#define NuIsDummy(ir) ((ir)->op == NU_OP_DUMMY || ((ir)->op == NU_OP_ALIGN))

typedef enum NuIrOpcode {
    #define X(m) NU_OP_ ## m,
    NU_OP_XMACRO
    #undef X
} NuIrOpcode;

typedef struct {
    int offset;
    int num;
    char name[16];
} NuIrLabel;

#define NuLabelName(L) ((L)->name)

typedef struct NuBytecode {
    struct NuBytecode *link;
    int	code;
    int usage;
    intptr_t value;
    const char *name;
    const char *impl_ptr;
    int impl_size;
    unsigned char macro_depth;
    unsigned is_rel_branch:1;
    unsigned is_any_branch:1;
    unsigned is_inline_asm:1;
    unsigned is_const:1;
    unsigned is_label:1;
    unsigned in_hub:1;
    unsigned is_binary_op:1;
    unsigned is_small_const:1;
} NuBytecode;

typedef struct nuir {
    struct nuir *next;
    struct nuir *prev;
    NuIrOpcode op;
    union {
        intptr_t   val;
        NuIrLabel *label;
        void *ptr;
    };
    const char *comment;
    NuBytecode *bytecode;
} NuIr;

typedef struct NuIrList {
    NuIr *head;
    NuIr *tail;
    struct NuIrList *nextList;
} NuIrList;

typedef struct {
    unsigned  clockFreq;
    unsigned  clockMode;
    unsigned  varSize;
    NuIrLabel *entryPt;
    NuIrLabel *initObj;
    NuIrLabel *initFrame;
    NuIrLabel *initSp;
} NuContext;

void NuIrInit(NuContext *ctxt);
NuIr *NuCreateIrOp(NuIrOpcode op);
NuIr *NuEmitOp(NuIrList *irl, NuIrOpcode op);
NuIr *NuEmitCommentedOp(NuIrList *irl, NuIrOpcode op, const char *comment);
NuIr *NuEmitNamedOpcode(NuIrList *irl, const char *name);
NuIr *NuEmitConst(NuIrList *irl, int32_t val);
NuIr *NuEmitAddress(NuIrList *irl, NuIrLabel *label);
NuIr *NuEmitCommentedAddress(NuIrList *irl, NuIrLabel *label, const char *comment);
NuIr *NuEmitCall(NuIrList *irl, NuIrOpcode op, NuIrLabel *label, const char *comment);
NuIr *NuEmitLabel(NuIrList *irl, NuIrLabel *label);
NuIr *NuEmitBranch(NuIrList *irl, NuIrOpcode op, NuIrLabel *label);
NuIr *NuEmitComment(NuIrList *irl, const char *comment);

NuIrLabel *NuCreateLabel();

void NuCreateBytecodes(NuIrList *irl);
void NuOutputInterpreter(struct flexbuf *fb, NuContext *ctxt);
void NuOutputFinish(struct flexbuf *fb, NuContext *ctxt);
void NuOutputIrList(struct flexbuf *fb, NuIrList *irl);
void NuOutputLabel(struct flexbuf *fb, NuIrLabel *label);
void NuOutputLabelNL(struct flexbuf *fb, NuIrLabel *label);

#endif
