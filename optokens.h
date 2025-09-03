//
// definitions for various binary (and unary) operators
//
#ifndef BINOP_H
#define BINOP_H

enum OpToken {
    K_NOP    = 0x001,
    K_ASSIGN = 0x100,
    K_BOOL_OR,
    K_BOOL_AND,    
    K_GE,
    K_LE,
    K_NE,
    K_EQ,
    K_LIMITMIN,    
    K_LIMITMAX,
    K_MODULUS,
    K_HIGHMULT,
    K_ROTR,
    K_ROTL,
    K_SHL,
    K_SHR,
    K_SAR,
    K_REV,
    K_NEGATE,
    K_BIT_NOT,
    K_SQRT,
    K_ABS,
    K_DECODE,
    K_ENCODE,
    K_BOOL_NOT,
    K_SGNCOMP,
    K_INCREMENT,
    K_DECREMENT,
    K_BOOL_XOR,
    K_UNS_DIV,
    K_UNS_MOD,
    K_SIGNEXTEND,
    K_ZEROEXTEND,
    K_LTU,
    K_LEU,
    K_GTU,
    K_GEU,
    
    K_ASC,     /* BASIC ASC operator */
    K_STRLEN,  /* BASIC LEN operator */
    
    K_LIMITMIN_UNS,
    K_LIMITMAX_UNS,
    K_POWER,
    K_FRAC64,
    K_UNS_HIGHMULT,
    K_ONES_COUNT,
    K_QLOG,
    K_QEXP,
    K_SCAS,
    K_ENCODE2,
/* spin versions of && and ||, which cannot short-circuit */
    K_LOGIC_AND,
    K_LOGIC_OR,
    K_LOGIC_XOR,

/* floating point */
/* these get turned into function calls */
    K_FEQ,
    K_FNE,
    K_FLT,
    K_FGT,
    K_FLE,
    K_FGE,
    K_FADD,
    K_FSUB,
    K_FMUL,
    K_FDIV,
    K_FNEGATE,
    K_FABS,
    K_FSQRT,

/* special 16 bit multiplies */
    K_MULU16,
    K_MULS16,

    /* reference pointer pre-inc/dec */
    K_REF_PREINC,
    K_REF_PREDEC,
    K_REF_POSTINC,
    K_REF_POSTDEC,
};

#endif
