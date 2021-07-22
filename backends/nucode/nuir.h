#ifndef NUIR_H
#define NUIR_H

#include <stdint.h>
#include <stdlib.h>

// Nu interpreter intermediate representation

#define NU_OP_XMACRO \
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
    X(SIGNX)     /* sign extend tos := nos SIGNX tos */ \
    X(ZEROX)     /* zero extend tos := nos ZEROX tos */ \
    \
    X(ADD)       /* tos := nos + tos */ \
    X(SUB)       /* tos := nos - tos */ \
    X(AND)       /* tos := nos & tos */ \
    X(OR)        /* tos := nos | tos */ \
    X(XOR)       /* tos := nos ^ tos */ \
    X(SHL)       /* tos := nos << tos */ \
    X(SHR)       /* tos := nos >> tos (unsigned right shift) */ \
    X(SAR)       /* tos := nos SAR tos (signed right shift) */ \
    \
    X(MULU)      /* unsigned multiply: leaves low, high on stack */ \
    X(MULS)      /* signed multiply:   leaves low, high on stack */ \
    X(DIVU)      /* unsigned divide: leaves quotient, remainder on stack */ \
    X(DIVS)      /* signed divide:   leaves quotient, remainder on stack */ \
    \
    X(NEG)       /* negate tos */ \
    X(NOT)       /* bit complement tos */ \
    X(ABS)       /* abs value tos */ \
    X(ISQRT)     /* find integer sqrt of tos */ \
    X(REV)       /* bit reverse tos */ \
    \
    X(DROP)      /* drop element on top of stack */ \
    X(DUP)       /* dup element on top of stack */ \
    X(SWAP)      /* swap tos and nos */ \
    \
    X(ENTER)     /* enter subroutine */ \
    X(RET)       /* return from subroutine */ \
    X(CALL)      /* call subroutine */ \
    X(JMP)       /* jump to address at tos */ \
    X(JMPZ)      /* jump to address to nos if tos is = 0 (discards tos, nos) */ \
    X(JMPNZ)     /* jump to address to nos if tos is <> 0 (discards tos, nos) */ \
    \
    X(PINHI)     /* set pin high */ \
    X(PINLO)     /* set pin low */  \
    X(PINNOT)    /* toggle pin  */  \
    X(PINRND)    /* set pin to random value */ \
    X(PINWR)     /* write value in nos to pin at tos */ \
    \
    X(PUSHI)     /* push immediate */ \
    X(PUSHA)     /* push address */ \
    \
    X(UNDEF)     /* Error/undefined */ \
    \
    X(DUMMY)     /* this and everything above needs no code */ \
    X(LABEL)     /* label for jump target */ \
    X(ALIGN)     /* align data */ \


typedef enum NuIrOpcode {
    #define X(m) NU_OP_ ## m,
    NU_OP_XMACRO
    #undef X
} NuIrOpcode;

typedef struct {
    int addr;
} NuIrLabel;

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
} NuIr;

typedef struct {
    NuIr *head;
    NuIr *tail;
} NuIrList;

void NuIrInit();
NuIr *NuEmitOp(NuIrList *irl, NuIrOpcode op);
NuIr *NuEmitConst(NuIrList *irl, int32_t val);
NuIr *NuEmitAddress(NuIrList *irl, NuIrLabel *label);
NuIr *NuEmitLabel(NuIrList *irl, NuIrLabel *label);

NuIrLabel *NuCreateLabel();

void NuAssignOpcodes();

#endif
