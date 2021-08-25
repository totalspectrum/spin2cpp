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
    X(ADD_SP)    /* add stack pointer to tos */ \
    X(ADD_PC)    /* add program counter to tos */ \
    \
    X(ADD)       /* tos := nos + tos */ \
    X(SUB)       /* tos := nos - tos */ \
    X(AND)       /* tos := nos & tos */ \
    X(IOR)        /* tos := nos | tos */ \
    X(XOR)       /* tos := nos ^ tos */ \
    \
    X(SIGNX)     /* sign extend tos := nos SIGNX tos */ \
    X(ZEROX)     /* zero extend tos := nos ZEROX tos */ \
    X(SHL)       /* tos := nos << tos */ \
    X(SHR)       /* tos := nos >> tos (unsigned right shift) */ \
    X(SAR)       /* tos := nos SAR tos (signed right shift) */ \
    X(ENCODE)    /* tos := nos ENCODE tos (0-32) */ \
    X(ENCODE2)   /* tos := nos ENCODE tos (0-31) */ \
    \
    X(MINS)      /* tos := min(nos, tos) (signed) */ \
    X(MAXS)      /* tos := max(nos, tos) (signed) */ \
    X(MINU)      /* tos := min(nos, tos) (unsigned) */ \
    X(MAXU)      /* tos := max(nos, tos) (unsigned) */ \
    \
    X(MULU)      /* unsigned multiply: leaves low, high on stack */ \
    X(MULS)      /* signed multiply:   leaves low, high on stack */ \
    X(DIVU)      /* unsigned divide: leaves quotient, remainder on stack */ \
    X(DIVS)      /* signed divide:   leaves quotient, remainder on stack */ \
    X(DIV64)     /* calculate lo, hi / n, leaves quotient, remainder on stack */ \
    X(MULDIV64)  /* calculate a*b/c with full precision */ \
    X(SQRT64)    /* calculate sqrt(nos, tos) with full precision */ \
    \
    X(NEG)       /* negate tos */ \
    X(NOT)       /* bit complement tos */ \
    X(ABS)       /* abs value tos */ \
    \
    X(DROP)      /* drop element on top of stack */ \
    X(DROP2)     /* drop two elements on top of stack */ \
    X(DUP)       /* dup element on top of stack: A B -> A B B */ \
    X(OVER)      /* changes stack from A B -> A B A */ \
    X(SWAP)      /* swap tos and nos */ \
    X(HALT)      /* halt processor */ \
    \
    X(ENTER)     /* enter subroutine */ \
    X(RET)       /* return from subroutine */ \
    X(CALL)      /* call subroutine */ \
    X(CALLM)     /* call method (changes object pointer) */ \
    X(GOSUB)     /* combo CALL+ENTER */ \
    X(JMP)       /* jump to address at tos */ \
    X(JMPZ)      /* jump to address to nos if tos is = 0 (discards tos, nos) */ \
    X(JMPNZ)     /* jump to address to nos if tos is <> 0 (discards tos, nos) */ \
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
    \
    X(GETCTHL)   /* get current cycle counter */ \
    X(WAITX)     /* delay for tos cycles */ \
    X(WAITCNT)   /* wait for a particular cycle */ \
    X(HUBSET)    /* hubset instruction */ \
    X(COGSTOP)   /* stop a particular cog */ \
    X(COGID)     /* get current COG id */ \
    X(LOCKMEM)   /* lock memory */ \
    \
    X(XORO)    /* xoro32 random number generation */ \
    \
    X(PUSHI32)   /* push immediate 32 bit */ \
    X(PUSHI16)   /* push immediate 16 bit */ \
    X(PUSHI8)    /* push immediate 8 bit */ \
    X(PUSHA)     /* push address */ \
    X(PUSH_0)    /* push zero */ \
    X(PUSH_1)    /* push one */ \
    X(PUSH_2)    /* push two */ \
    X(PUSH_4)    /* push four */ \
    X(PUSH_8)    /* push eight */ \
    \
    X(INLINEASM)  /* load inline assembly */ \
    \
    X(JMPREL)    /* jump forward relative 3*n bytes */ \
    X(BRA)       /* branch always */ \
    X(CBEQ)      /* compare branch if equal */ \
    X(CBNE)      /* compare branch if not equal */ \
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
    X(LABEL)     /* label for jump target */ \
    X(ALIGN)     /* align data */ \


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

typedef struct nuir {
    struct nuir *next;
    struct nuir *prev;
    int addr;
    NuIrOpcode op;
    union {
        int32_t val;
        NuIrLabel *label;
        void *ptr;
    };
    const char *comment;
} NuIr;

typedef struct {
    NuIr *head;
    NuIr *tail;
} NuIrList;

typedef struct {
    unsigned  clockFreq;
    unsigned  clockMode;
    NuIrLabel *entryPt;
    NuIrLabel *initObj;
    NuIrLabel *initFrame;
    NuIrLabel *initSp;
} NuContext;

void NuIrInit(NuContext *ctxt);
NuIr *NuEmitOp(NuIrList *irl, NuIrOpcode op);
NuIr *NuEmitCommentedOp(NuIrList *irl, NuIrOpcode op, const char *comment);
NuIr *NuEmitNamedOpcode(NuIrList *irl, const char *name);
NuIr *NuEmitConst(NuIrList *irl, int32_t val);
NuIr *NuEmitAddress(NuIrList *irl, NuIrLabel *label);
NuIr *NuEmitLabel(NuIrList *irl, NuIrLabel *label);
NuIr *NuEmitBranch(NuIrList *irl, NuIrOpcode op, NuIrLabel *label);

NuIrLabel *NuCreateLabel();

void NuAssignOpcodes();
void NuOutputInterpreter(struct flexbuf *fb, NuContext *ctxt);
void NuOutputFinish(struct flexbuf *fb, NuContext *ctxt);
void NuOutputIrList(struct flexbuf *fb, NuIrList *irl);
void NuOutputLabel(struct flexbuf *fb, NuIrLabel *label);
void NuOutputLabelNL(struct flexbuf *fb, NuIrLabel *label);

#endif
