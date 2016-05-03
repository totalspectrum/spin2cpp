//
// binary data output for spin2cpp
//
// Copyright 2012-2016 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "spinc.h"

static void putbyte(Flexbuf *f, unsigned int x)
{
    flexbuf_putc(x & 0xff, f);
}
static void putword(Flexbuf *f, unsigned int x)
{
    putbyte(f, x & 0xff);
    putbyte(f,  (x>>8) & 0xff);
}
static void putlong(Flexbuf *f, unsigned int x)
{
    putbyte(f, x & 0xff);
    putbyte(f,  (x>>8) & 0xff);
    putbyte(f, (x>>16) & 0xff);
    putbyte(f, (x>>24) & 0xff);
}

static void
OutputSpinHeader(Flexbuf *f, Module *P)
{
    unsigned int clkfreq;
    unsigned int clkmodeval;

    if (!GetClkFreq(P, &clkfreq, &clkmodeval)) {
        // use defaults
        clkfreq = 80000000;
        clkmodeval = 0x6f;
    }
    
    putlong(f, clkfreq);
    putbyte(f, clkmodeval);
    putbyte(f, 0);      // checksum
    putword(f, 0x0010); // PBASE
    putword(f, 0x7fe8); // VBASE
    putword(f, 0x7ff0); // DBASE
    putword(f, 0x0018); // PCURR
    putword(f, 0x7ff8); // DCURR
    putword(f, 0x0008); // object length?
    putbyte(f, 0x02);
    putbyte(f, 0x00);
    putword(f, 0x0008);
    putword(f, 0x0000); // initial stack: 0 == first run of program

    // simple spin program
    putbyte(f, 0x3f);
    putbyte(f, 0x89);
    putbyte(f, 0xc7);
    putbyte(f, 0x10);
    putbyte(f, 0xa4);
    putbyte(f, 0x6);
    putbyte(f, 0x2c);
    putbyte(f, 0x32);
}

void
OutputDatFile(const char *fname, Module *P, int prefixBin)
{
    FILE *f = NULL;
    Module *save;
    Flexbuf fb;
    
    save = current;
    current = P;

    f = fopen(fname, "wb");
    if (!f) {
        perror(fname);
        exit(1);
    }

    flexbuf_init(&fb, BUFSIZ);
    if (prefixBin && !gl_p2) {
        /* output a binary header */
        OutputSpinHeader(&fb, P);
    }
    PrintDataBlock(&fb, P, NULL, NULL);
    fwrite(flexbuf_peek(&fb), flexbuf_curlen(&fb), 1, f);
    fclose(f);
    flexbuf_delete(&fb);
    
    current = save;
}

/*
 * functions for output of DAT sections
 */
/*
 * data block printing functions
 */
#define BYTES_PER_LINE 16  /* must be at least 4 */
static int datacount = 0;

static DataBlockOutFunc outc;

static void
outputByteBinary(Flexbuf *f, int c)
{
    flexbuf_putc(c, f);
}

static void
outputByte(Flexbuf *f, int c)
{
    (*outc)(f, c);
    datacount++;
}

static void
outputLong(Flexbuf *f, uint32_t c)
{
    int i;
    for (i = 0; i < 4; i++) {
        outputByte(f, (c & 0xff));
        c = c>>8;
    }
}

static void
initDataOutput(DataBlockOutFunc func)
{
    datacount = 0;
    if (func) {
        outc = func;
    } else {
        outc = outputByteBinary;
    }
}

static int
GetAddrOffset(AST *ast)
{
    Symbol *sym;
    Label *label;
    
    if (ast->kind != AST_IDENTIFIER) {
        ERROR(ast, "@@@ supported only on identifiers");
        return 0;
    }
    sym = LookupSymbol(ast->d.string);
    if (!sym) {
        ERROR(ast, "Unknown symbol %s", ast->d.string);
        return 0;
    }
    if (sym->type != SYM_LABEL) {
        ERROR(ast, "@@@ supported only on labels");
        return 0;
    }
    label = (Label *)sym->val;
    return label->offset;
}
        
void
outputDataList(Flexbuf *f, int size, AST *ast, Flexbuf *relocs)
{
    unsigned val, origval;
    int i, reps;
    AST *sub;
    Reloc r;
    
    origval = 0;
    while (ast) {
        sub = ast->left;
        if (sub->kind == AST_ARRAYDECL || sub->kind == AST_ARRAYREF) {
            origval = EvalPasmExpr(ast->left->left);
            reps = EvalPasmExpr(ast->left->right);
        } else if (sub->kind == AST_STRING) {
            const char *ptr = sub->d.string;
            while (*ptr) {
                val = (*ptr++) & 0xff;
                outputByte(f, val);
                for (i = 1; i < size; i++) {
                    outputByte(f, 0);
                }
            }
            reps = 0;
        } else if (sub->kind == AST_RANGE) {
            int start = EvalPasmExpr(sub->left);
            int end = EvalPasmExpr(sub->right);
            while (start <= end) {
                val = start;
                for (i = 0; i < size; i++) {
                    outputByte(f, val & 0xff);
                    val = val >> 8;
                }
                start++;
            }
            reps = 0;
        } else if (sub->kind == AST_ABSADDROF) {
            if (relocs) {
                int addr = flexbuf_curlen(f);
                if (size != LONG_SIZE) {
                    ERROR(ast, "@@@ supported only on long values");
                }
                if ( (addr & 3) != 0 ) {
                    ERROR(ast, "@@@ supported only on long boundary");
                }
                r.addr = addr;
                r.value = origval = GetAddrOffset(sub->left);
                flexbuf_addmem(relocs, (const char *)&r, sizeof(r));
            } else {
                origval = EvalPasmExpr(sub);
            }
            reps = 1;
        } else {
            origval = EvalPasmExpr(sub);
            reps = 1;
        }
        while (reps > 0) {
            val = origval;
            for (i = 0; i < size; i++) {
                outputByte(f, val & 0xff);
                val = val >> 8;
            }
            --reps;
        }
        ast = ast->right;
    }
}

/*
 * check immediates
 */
static int
isImmediate(InstrModifier *im)
{
    return im->name[0] == '#';
}

#define BIG_IMM_SRC 0x01
#define BIG_IMM_DST 0x02
#define ANY_BIG_IMM (BIG_IMM_SRC|BIG_IMM_DST)

static unsigned
ImmMask(Instruction *instr, int numoperands, AST *ast)
{
    unsigned mask;
    InstrModifier *mod = (InstrModifier *)ast->d.ptr;
    bool bigImm = strcmp(mod->name, "##") == 0;
    
    if (gl_p2) {
        mask = P2_IMM_SRC;
        if (bigImm) {
            mask |= BIG_IMM_SRC;
        }
    } else {
        mask = IMMEDIATE_INSTR;
    }
    switch(instr->ops) {
    case P2_JUMP:
    case P2_TJZ_OPERANDS:
    case SRC_OPERAND_ONLY:
    case CALL_OPERAND:
        return mask;
    case TWO_OPERANDS:
    case JMPRET_OPERANDS:
        if (numoperands < 2) {
            ERROR(ast, "bad immediate operand to %s", instr->name);
            return 0;
        }
        return mask;
    case P2_TWO_OPERANDS:
    case P2_RDWR_OPERANDS:
        return mask;
    case P2_DST_CONST_OK:
        mask = P2_IMM_DST;
        if (bigImm) {
            mask |= BIG_IMM_DST;
        }
        return mask;
    default:
        ERROR(ast, "immediate not supported for %s instruction", instr->name);
        return 0;
    }
}

/*
 * assemble an instruction, along with its modifiers
 */
#define MAX_OPERANDS 2

void
assembleInstruction(Flexbuf *f, AST *ast, int asmpc)
{
    uint32_t val, mask, src, dst;
    uint32_t immmask;
    int32_t isrc;
    Instruction *instr;
    int numoperands, expectops;
    AST *operand[MAX_OPERANDS];
    AST *line = ast;
    char *callname;
    AST *retast;
    int immSrc = 0;
    int isRelJmp = 0;
    
    /* make sure it is aligned */
    while ((datacount % 4) != 0) {
        outputByte(f, 0);
    }

    instr = (Instruction *)ast->d.ptr;
    val = instr->binary;
    immmask = 0;
    if (instr->opc != OPC_NOP) {
        /* for anything except NOP set the condition to "always" */
        if (gl_p2) {
            val |= 0xf << 28;
        } else {
            val |= 0xf << 18;
        }
    }
    /* check for modifiers and operands */
    numoperands = 0;
    ast = ast->right;
    while (ast != NULL) {
        if (ast->kind == AST_EXPRLIST) {
            if (numoperands >= MAX_OPERANDS) {
                ERROR(line, "Too many operands to instruction");
                return;
            }
            operand[numoperands++] = ast->left;
        } else if (ast->kind == AST_INSTRMODIFIER) {
            InstrModifier *mod = (InstrModifier *)ast->d.ptr;
            if (isImmediate(mod)) {
                // sanity check that the immediate
                // is on the correct operand
                immmask = ImmMask(instr, numoperands, ast);
                if (gl_p2) {
                    immSrc = immmask & P2_IMM_SRC;
                    //immDst = mask & P2_IMM_DST;
                } else {
                    immSrc = immmask & IMMEDIATE_INSTR;
                }
                mask = 0;
            } else {
                mask = mod->modifier;
            }
            if (mask & 0x0003ffff) {
                val = val & mask;
            } else {
                val = val | mask;
            }
        } else {
            ERROR(line, "Internal error: expected instruction modifier found %d", ast->kind);
            return;
        }
        ast = ast->right;
    }

    /* parse operands and put them in place */
    switch (instr->ops) {
    case NO_OPERANDS:
        expectops = 0;
        break;
    case TWO_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case JMPRET_OPERANDS:
    case P2_TJZ_OPERANDS:
    case P2_TWO_OPERANDS:
    case P2_RDWR_OPERANDS:
        expectops = 2;
        break;
    default:
        expectops = 1;
        break;
    }
    if (instr->ops == TWO_OPERANDS_OPTIONAL && numoperands == 1) {
        // duplicate the operand
        // so neg r0 -> neg r0,r0
        operand[numoperands++] = operand[0];
    }
    if (expectops != numoperands) {
        ERROR(line, "Expected %d operands for %s, found %d", expectops, instr->name, numoperands);
        return;
    }
    src = dst = 0;
    switch (instr->ops) {
    case NO_OPERANDS:
        break;
    case TWO_OPERANDS:
    case JMPRET_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case P2_TWO_OPERANDS:
    case P2_RDWR_OPERANDS:
        dst = EvalPasmExpr(operand[0]);
        src = EvalPasmExpr(operand[1]);
        break;
    case P2_TJZ_OPERANDS:
        dst = EvalPasmExpr(operand[0]);
        if (immSrc) {
            isrc = EvalPasmExpr(operand[1]) - (asmpc+4);
            isrc /= 4;
            if ( (isrc < -256) || (isrc > 255) ) {
                ERROR(line, "Source out of range for relative branch %s", instr->name);
                isrc = 0;
            }
            src = isrc & 0x1ff;
        } else {
            src = EvalPasmExpr(operand[1]);
        }
        break;
    case SRC_OPERAND_ONLY:
        dst = 0;
        src = EvalPasmExpr(operand[0]);
        break;
    case P2_JUMP:
        // indirect jump needs to be handled
        if (!immSrc) {
            ERROR(line, "indirect jumps via %s not implemented yet", instr->name);
            return;
        }
        dst = 0;
        isRelJmp = 1;
        if (isRelJmp) {
            isrc = EvalPasmExpr(operand[1]) - (asmpc+4);
            isrc /= 4;
            src = isrc & 0xfffff;
            val = val | (1<<20);
        }
        if (immmask & ANY_BIG_IMM) {
            ERROR(line, "big immediates not yet supported for %s", instr->name);
            immmask &= ~ANY_BIG_IMM;
        }
        goto instr_ok;
    case DST_OPERAND_ONLY:
    case P2_DST_CONST_OK:
        dst = EvalPasmExpr(operand[0]);
        src = 0;
        break;
    case CALL_OPERAND:
        if (operand[0]->kind != AST_IDENTIFIER) {
            ERROR(operand[0], "call operand must be an identifier");
            return;
        }
        src = EvalPasmExpr(operand[0]);
        callname = malloc(strlen(operand[0]->d.string) + 8);
        strcpy(callname, operand[0]->d.string);
        strcat(callname, "_ret");
        retast = NewAST(AST_IDENTIFIER, NULL, NULL);
        retast->d.string = callname;
        dst = EvalPasmExpr(retast);
        break;
    default:
        ERROR(line, "Unsupported instruction `%s'", instr->name);
        return;
    }

    if (immmask & BIG_IMM_SRC) {
        uint32_t augval = val & 0xf0000000; // preserve condition
        augval |= (src >> 9) & 0x007fffff;
        augval |= 0x0f000000; // AUGS
        src &= 0x1ff;
        outputLong(f, augval);
        immmask &= ~BIG_IMM_SRC;
    }
    if (immmask & BIG_IMM_DST) {
        uint32_t augval = val & 0xf0000000; // preserve condition
        augval |= (dst >> 9) & 0x007fffff;
        augval |= 0x0f800000; // AUGD
        dst &= 0x1ff;
        outputLong(f, augval);
        immmask &= ~BIG_IMM_DST;
    }
    if (src > 511) {
        ERROR(line, "Source operand too big for %s", instr->name);
        return;
    }
    if (dst > 511) {
        ERROR(line, "Destination operand too big for %s", instr->name);
        return;
    }
instr_ok:
    val = val | (dst << 9) | src | immmask;
    /* output the instruction */
    outputLong(f, val);
}

void
outputAlignedDataList(Flexbuf *f, int size, AST *ast, Flexbuf *relocs)
{
    if (size > 1) {
        while ((datacount % size) != 0) {
            outputByte(f, 0);
        }
    }
    outputDataList(f, size, ast, relocs);
}

/*
 * output bytes for a file
 */
static void
assembleFile(Flexbuf *f, AST *ast)
{
    FILE *inf;
    const char *name = ast->d.string;
    int c;

    inf = fopen(name, "rb");
    if (!inf) {
        ERROR(ast, "file %s: %s", name, strerror(errno));
        return;
    }
    while ((c = fgetc(inf)) >= 0) {
        outputByte(f, c);
    }
    fclose(inf);
}

/*
 * print out a data block
 */
void
PrintDataBlock(Flexbuf *f, Module *P, DataBlockOutFunc func, Flexbuf *relocs)
{
    AST *ast;

    initDataOutput(func);
    if (gl_errors != 0)
        return;
    for (ast = P->datblock; ast; ast = ast->right) {
        switch (ast->kind) {
        case AST_BYTELIST:
            outputAlignedDataList(f, 1, ast->left, relocs);
            break;
        case AST_WORDLIST:
            outputAlignedDataList(f, 2, ast->left, relocs);
            break;
        case AST_LONGLIST:
            outputAlignedDataList(f, 4, ast->left, relocs);
            break;
        case AST_INSTRHOLDER:
            assembleInstruction(f, ast->left, ast->d.ival);
            break;
        case AST_IDENTIFIER:
            /* just skip labels */
            break;
        case AST_FILE:
            assembleFile(f, ast->left);
            break;
        case AST_ORGH:
        case AST_ORG:
        case AST_RES:
        case AST_FIT:
        case AST_LINEBREAK:
            break;
        default:
            ERROR(ast, "unknown element in data block");
            break;
        }
    }
}

// output _clkmode and _clkfreq settings
int
GetClkFreq(Module *P, unsigned int *clkfreqptr, unsigned int *clkregptr)
{
    // look up in P->objsyms
    Symbol *clkmodesym = FindSymbol(&P->objsyms, "_clkmode");
    Symbol *sym;
    AST *ast;
    int32_t clkmode, clkfreq, xinfreq;
    int32_t multiplier = 1;
    uint8_t clkreg;
    
    if (!clkmodesym) {
        return 0;  // nothing to do
    }
    ast = (AST *)clkmodesym->val;
    if (clkmodesym->type != SYM_CONSTANT) {
        WARNING(ast, "_clkmode is not a constant");
        return 0;
    }
    clkmode = EvalConstExpr(ast);
    // now we need to figure out the frequency
    clkfreq = 0;
    sym = FindSymbol(&P->objsyms, "_clkfreq");
    if (sym) {
        if (sym->type == SYM_CONSTANT) {
            clkfreq = EvalConstExpr((AST*)sym->val);
        } else {
            WARNING((AST*)sym->val, "_clkfreq is not a constant");
        }
    }
    xinfreq = 0;
    sym = FindSymbol(&P->objsyms, "_xinfreq");
    if (sym) {
        if (sym->type == SYM_CONSTANT) {
            xinfreq = EvalConstExpr((AST*)sym->val);
        } else {
            WARNING((AST*)sym->val, "_xinfreq is not a constant");
        }
    }
    // calculate the multiplier
    clkreg = 0;
    if (clkmode & RCFAST) {
        // nothing to do here
    } else if (clkmode & RCSLOW) {
        clkreg |= 0x01;   // CLKSELx
    } else if (clkmode & (XINPUT)) {
        clkreg |= (1<<5); // OSCENA
        clkreg |= 0x02;   // CLKSELx
    } else {
        clkreg |= (1<<5); // OSCENA
        clkreg |= (1<<6); // PLLENA
        if (clkmode & XTAL1) {
            clkreg |= (1<<3);
        } else if (clkmode & XTAL2) {
            clkreg |= (2<<3);
        } else {
            clkreg |= (3<<3);
        }
        if (clkmode & PLL1X) {
            multiplier = 1;
            clkreg |= 0x3;  // CLKSELx
        } else if (clkmode & PLL2X) {
            multiplier = 2;
            clkreg |= 0x4;  // CLKSELx
        } else if (clkmode & PLL4X) {
            multiplier = 4;
            clkreg |= 0x5;  // CLKSELx
        } else if (clkmode & PLL8X) {
            multiplier = 8;
            clkreg |= 0x6;  // CLKSELx
        } else if (clkmode & PLL16X) {
            multiplier = 16;
            clkreg |= 0x7;  // CLKSELx
        }
    }
    
    // validate xinfreq and clkfreq
    if (xinfreq == 0) {
        if (clkfreq == 0) {
            ERROR(NULL, "Must set at least one of _XINFREQ or _CLKFREQ");
            return 0;
        }
    } else {
        int32_t calcfreq = xinfreq * multiplier;
        if (clkfreq != 0) {
            if (calcfreq != clkfreq) {
                ERROR(NULL, "Inconsistent values for _XINFREQ and _CLKFREQ");
                return 0;
            }
        }
        clkfreq = calcfreq;
    }

    *clkfreqptr = clkfreq;
    *clkregptr = clkreg;
    return 1;
}
