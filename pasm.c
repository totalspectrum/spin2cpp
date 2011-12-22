/*
 * PASM and data compilation
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "spinc.h"

static void
nofunc(FILE *f, AST *expr)
{
    ERROR("bad assembly instruction in spin code");
}

Instruction
instr[] = {
    { "abs",    0xa8800000, TWO_OPERANDS, nofunc },
    { "absneg", 0xac800000, TWO_OPERANDS, nofunc },
    { "add",    0x80800000, TWO_OPERANDS, nofunc },
    { "addabs", 0x88800000, TWO_OPERANDS, nofunc },
    { "adds",   0xd0800000, TWO_OPERANDS, nofunc },
    { "addsx",  0xd8800000, TWO_OPERANDS, nofunc },
    { "addx",   0xc8800000, TWO_OPERANDS, nofunc },
    { "and",    0x60800000, TWO_OPERANDS, nofunc },
    { "andn",   0x64800000, TWO_OPERANDS, nofunc },

    { "call",   0x5cc00000, CALL_OPERAND, nofunc },
    { "clkset", 0x0c400000, ONE_OPERAND, nofunc },
    { "cmp",    0x84000000, TWO_OPERANDS, nofunc },
    { "cmps",   0xc0000000, TWO_OPERANDS, nofunc },
    { "cmpsx",  0xc4000000, TWO_OPERANDS, nofunc },
    { "cmpx",   0xcc000000, TWO_OPERANDS, nofunc },

    { "cogid",  0x0c400001, ONE_OPERAND, nofunc },
    { "coginit", 0x0c400002, ONE_OPERAND, nofunc },
    { "cogstop", 0x0c400003, ONE_OPERAND, nofunc },

    { "djnz",   0xe4800000, TWO_OPERANDS, nofunc },
    { "hubop",  0x0c000000, TWO_OPERANDS, nofunc },
    { "jmp",    0x5c000000, JMP_OPERAND, nofunc },
    { "jmpret", 0x5c800000, TWO_OPERANDS, nofunc },

    { "lockclr",0x0c400007, ONE_OPERAND, nofunc },
    { "locknew",0x0c400004, ONE_OPERAND, nofunc },
    { "lockret",0x0c400005, ONE_OPERAND, nofunc },
    { "lockset",0x0c400006, ONE_OPERAND, nofunc },

    { "max",    0x4c800000, TWO_OPERANDS, nofunc },
    { "maxs",   0x44800000, TWO_OPERANDS, nofunc },
    { "min",    0x48800000, TWO_OPERANDS, nofunc },
    { "mins",   0x40800000, TWO_OPERANDS, nofunc },
    { "mov",    0xa0800000, TWO_OPERANDS, nofunc },
    { "movd",   0x54800000, TWO_OPERANDS, nofunc },
    { "movi",   0x58800000, TWO_OPERANDS, nofunc },
    { "movs",   0x50800000, TWO_OPERANDS, nofunc },

    { "muxc",   0x70800000, TWO_OPERANDS, nofunc },
    { "muxnc",  0x74800000, TWO_OPERANDS, nofunc },
    { "muxz",   0x78800000, TWO_OPERANDS, nofunc },
    { "muxnz",  0x7c800000, TWO_OPERANDS, nofunc },

    { "neg",    0xa4800000, TWO_OPERANDS, nofunc },
    { "negc",   0xb0800000, TWO_OPERANDS, nofunc },
    { "negnc",  0xb4800000, TWO_OPERANDS, nofunc },
    { "negnz",  0xbc800000, TWO_OPERANDS, nofunc },
    { "negz",   0xb8800000, TWO_OPERANDS, nofunc },
    { "nop",    0x00000000, NO_OPERANDS, nofunc },

    { "or",     0x68800000, TWO_OPERANDS, nofunc },

    { "rcl",    0x34800000, TWO_OPERANDS, nofunc },
    { "rcr",    0x30800000, TWO_OPERANDS, nofunc },
    { "rdbyte", 0x00800000, TWO_OPERANDS, nofunc },
    { "rdlong", 0x08800000, TWO_OPERANDS, nofunc },
    { "rdword", 0x04800000, TWO_OPERANDS, nofunc },
    { "ret",    0x5c400000, NO_OPERANDS, nofunc },
    { "rev",    0x3c800000, TWO_OPERANDS, nofunc },
    { "rol",    0x24800000, TWO_OPERANDS, nofunc },
    { "ror",    0x20800000, TWO_OPERANDS, nofunc },

    { "sar",    0x38800000, TWO_OPERANDS, nofunc },
    { "shl",    0x2c800000, TWO_OPERANDS, nofunc },
    { "shr",    0x28800000, TWO_OPERANDS, nofunc },
    { "sub",    0x84800000, TWO_OPERANDS, nofunc },
    { "subs",   0xc0800000, TWO_OPERANDS, nofunc },
    { "subsx",  0xc4800000, TWO_OPERANDS, nofunc },
    { "subx",   0xcc800000, TWO_OPERANDS, nofunc },
    { "sumc",   0x90800000, TWO_OPERANDS, nofunc },
    { "sumnc",  0x94800000, TWO_OPERANDS, nofunc },
    { "sumnz",  0x9c800000, TWO_OPERANDS, nofunc },
    { "sumz",   0x98800000, TWO_OPERANDS, nofunc },

    { "test",   0x60000000, TWO_OPERANDS, nofunc },
    { "testn",  0x64000000, TWO_OPERANDS, nofunc },
    { "tjnz",   0xe8000000, TWO_OPERANDS, nofunc },
    { "tjz",    0xec000000, TWO_OPERANDS, nofunc },

    { "waitcnt", 0xf8800000, TWO_OPERANDS, nofunc },
    { "waitpeq", 0xf0800000, TWO_OPERANDS, nofunc },
    { "waitpne", 0xf4800000, TWO_OPERANDS, nofunc },
    { "waitvid", 0xfc800000, TWO_OPERANDS, nofunc },

    { "wrbyte", 0x00000000, TWO_OPERANDS, nofunc },
    { "wrlong", 0x08000000, TWO_OPERANDS, nofunc },
    { "wrword", 0x04000000, TWO_OPERANDS, nofunc },

    { "xor",    0x6c800000, TWO_OPERANDS, nofunc },
};
