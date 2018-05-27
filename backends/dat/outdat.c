//
// binary data output for spin2cpp
//
// Copyright 2012-2018 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "spinc.h"

int gl_compressed = 0;

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
    size_t curlen;
    size_t desiredlen;
    save = current;
    current = P;

    f = fopen(fname, "wb");
    if (!f) {
        fprintf(stderr, "Unable to open output file: ");
        perror(fname);
        exit(1);
    }

    flexbuf_init(&fb, BUFSIZ);
    if (prefixBin && !gl_p2) {
        /* output a binary header */
        OutputSpinHeader(&fb, P);
    }
    PrintDataBlock(&fb, P, NULL, NULL);
    curlen = flexbuf_curlen(&fb);
    fwrite(flexbuf_peek(&fb), curlen, 1, f);
    if (gl_p2) {
        desiredlen = (curlen + 31) & ~31;
    } else {
        desiredlen = (curlen + 3) & ~3;
    }
    while (curlen < desiredlen) {
        fputc(0, f);
        curlen++;
    }
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

static void
outputByteBinary(Flexbuf *f, int c)
{
    flexbuf_putc(c, f);
}

static DataBlockOutFuncs defaultOutFuncs = {
    NULL,
    outputByteBinary,
    NULL,
};

static DataBlockOutFuncs *outFuncs;

static void
outputByte(Flexbuf *f, int c)
{
    (outFuncs->putByte)(f, c);
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
outputInstrLong(Flexbuf *f, uint32_t c)
{
    outputLong(f, c);
}

static void
initDataOutput(DataBlockOutFuncs *funcs)
{
    datacount = 0;
    if (funcs) {
        outFuncs = funcs;
    } else {
        outFuncs = &defaultOutFuncs;
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
    return label->hubval;
}
        
void
outputDataList(Flexbuf *f, int size, AST *ast, Flexbuf *relocs)
{
    unsigned val, origval;
    int i, reps;
    AST *sub;
    
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
                Reloc r;
                int addr = flexbuf_curlen(f);
                if (size != LONG_SIZE) {
                    ERROR(ast, "@@@ supported only on long values");
                }
                r.kind = RELOC_KIND_LONG;
                r.off = addr;
                r.val = origval = GetAddrOffset(sub->left);
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

#define BIG_IMM_SRC 0x01
#define BIG_IMM_DST 0x02
#define ANY_BIG_IMM (BIG_IMM_SRC|BIG_IMM_DST)
#define DUMMY_MASK  0x80

static unsigned
ImmMask(Instruction *instr, int opnum, bool bigImm, AST *ast)
{
    unsigned mask;
    
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
    case P2_LOC:
    case P2_AUG:
    case P2_JINT_OPERANDS:
    case SRC_OPERAND_ONLY:
    case CALL_OPERAND:
        return mask;
    case TWO_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case TWO_OPERANDS_DEFZ:
    case JMPRET_OPERANDS:
    case P2_TJZ_OPERANDS:
    case P2_TWO_OPERANDS:
    case P2_RDWR_OPERANDS:
    case THREE_OPERANDS_BYTE:
    case THREE_OPERANDS_NIBBLE:
    case THREE_OPERANDS_WORD:
        if (gl_p2) {
            if (opnum == 0) {
                mask = P2_IMM_DST;
                if (bigImm) {
                    mask |= BIG_IMM_DST;
                }
            } else if (opnum == 2) {
                mask = DUMMY_MASK; // no change to opcode
            }
        } else {
            if (opnum == 0) {
                ERROR(ast, "bad immediate operand to %s", instr->name);
                return 0;
            }
        }
        return mask;
    case P2_DST_CONST_OK:
        /* uses the I bit instead of L?? */
        mask = P2_IMM_SRC;
        if (bigImm) {
            mask |= BIG_IMM_DST;
        }
        return mask;
    default:
        ERROR(ast, "immediate value not supported for %s instruction", instr->name);
        return 0;
    }
}

/*
 * assemble a special read operand
 */
unsigned
SpecialRdOperand(AST *ast, uint32_t opimm)
{
    HwReg *hwreg;
    uint32_t val;
    int subval = 0;
    int negsubval = 0;

    if (opimm) {
        // user provided an immediate value; make sure it
        // fits in $00-$ff
        if (!IsConstExpr(ast)) {
            ERROR(ast, "bad immediate value");
            return 0x1ff;
        }
        val = EvalConstExpr(ast);
        if ( (val > 0xff) && 0 == (opimm & BIG_IMM_SRC) ) {
            WARNING(ast, "immediate value out of range");
        }
    }
    val = 0;
    if (ast->kind == AST_OPERATOR && (ast->d.ival == K_INCREMENT
                                      || ast->d.ival == K_DECREMENT))
    {
        if (ast->d.ival == K_INCREMENT) {
            if (ast->left) {
                ast = ast->left;
                // a++: x110 0001
                val = 0x60;
                subval = 1;
            } else {
                // ++a: x100 0001
                ast = ast->right;
                val = 0x40;
                subval = 1;
            }
        } else {
            if (ast->left) {
                ast = ast->left;
                // a--
                val = 0x60;
                subval = -1;
            } else {
                // --a: x101 1111
                ast = ast->right;
                val = 0x40;
                subval = -1;
            }
        }
    }

    negsubval = subval < 0 ? -1 : 1;
    if (ast->kind == AST_RANGEREF) {
        AST *idx = ast->right;
        if (idx && idx->kind == AST_RANGE) {
            if (idx->right) {
                ERROR(ast, "illegal ptr dereference");
                return 0;
            }
            subval = EvalPasmExpr(idx->left) * negsubval;
            if (subval < -16 || subval > 15) {
                ERROR(ast, "ptr index out of range");
                subval = 0;
            }
            ast = ast->left;
        } else {
            ERROR(ast, "bad ptr expression");
            return 0;
        }
    }
    if (ast->kind == AST_HWREG) {
        hwreg = (HwReg *)ast->d.ptr;
        if (hwreg->addr == 0x1f8) {
            // ptra
            val |= 0x100;
        } else if (hwreg->addr == 0x1f9) {
            // ptrb
            val |= 0x180;
        } else {
            if (val) {
                ERROR(ast, "bad hardware reference");
            }
            return 0;
        }
    } else if (val) {
        ERROR(ast, "bad rdlong/wrlong pointer reference");
        return 0;
    }
    return val | (subval & 0x1f);
}

/* utility routine to fix up an opcode based on the #N at the end */
uint32_t
FixupThreeOperands(uint32_t val, AST *op, uint32_t immflags, uint32_t maxN, AST *line, Instruction *instr)
{
    uint32_t NN;
    if (!op || immflags == 0) {
        ERROR(line, "Third operand to %s must be an immediate\n", instr->name);
        return val;
    }
    NN = EvalPasmExpr(op);
    if (NN >= maxN) {
        ERROR(line, "Third operand to %s must be less than %u\n", instr->name, maxN);
        return val;
    }
    val |= NN << 19;
    return val;
}

#define MAX_OPERANDS 3
/*
 * decode operands, taking care of alternate forms and such
 * fills in the operands table with operands for the instruction,
 * and opimm with immediate bits
 * returns number of operands
 */

int
DecodeAsmOperands(Instruction *instr, AST *ast, AST **operand, uint32_t *opimm, uint32_t *val, uint32_t *effectFlags)
{
    int numoperands = 0;
    AST *line = ast;
    bool sawFlagUsed = false;
    uint32_t mask;
    int expectops;
    
    /* check for modifiers and operands */
    numoperands = 0;
    ast = ast->right;
    while (ast != NULL) {
        if (ast->kind == AST_SRCCOMMENT || ast->kind == AST_COMMENT) {
            /* do nothing */
        } else if (ast->kind == AST_EXPRLIST) {
            uint32_t imask;
            AST *op;
            if (numoperands >= MAX_OPERANDS) {
                ERROR(line, "Too many operands to instruction");
                return -1;
            }
            if (ast->left && ast->left->kind == AST_IMMHOLDER) {
                imask = ImmMask(instr, numoperands, false, ast);
                op = ast->left->left;
            } else if (ast->left && ast->left->kind == AST_BIGIMMHOLDER) {
                imask = ImmMask(instr, numoperands, true, ast);
                op = ast->left->left;
            } else {
                imask = 0;
                op = ast->left;
            }
            operand[numoperands] = op;
            opimm[numoperands] = imask;
            numoperands++;
        } else if (ast->kind == AST_INSTRMODIFIER) {
            InstrModifier *mod = (InstrModifier *)ast->d.ptr;
            mask = mod->modifier;
            /* record if C or Z flags were set */
            if (mod->flags & (FLAG_CZSET)) {
                sawFlagUsed = true;
            }
            if (effectFlags) {
                *effectFlags |= mod->flags;
            }
            /* verify here that the modifier is permitted for this instruction */
            if (mod->flags) {
                if (0 == (mod->flags & instr->flags)) {
                    ERROR(line, "modifier %s not valid for %s", mod->name, instr->name);
                }
                if (instr->flags == FLAG_P2_CZTEST) {
                    // need to tweak the instruction bits here based on instruction type
                    uint32_t instrMask = 0;
                    if (mod->flags == FLAG_WZ || mod->flags == FLAG_WC) {
                        instrMask = 0;
                    } else if (mod->flags == FLAG_ANDC || mod->flags == FLAG_ANDZ) {
                        instrMask = 2;
                    } else if (mod->flags == FLAG_ORC || mod->flags == FLAG_ORZ) {
                        instrMask = 4;
                    } else if (mod->flags == FLAG_XORC || mod->flags == FLAG_XORZ) {
                        instrMask = 6;
                    }
                    if (instr->ops == P2_DST_CONST_OK) {
                        // testp: modify src bits
                        if (val) *val |= instrMask;
                    } else if (instr->ops == TWO_OPERANDS) {
                        // testb: modify opcode bits
                        if (val) *val |= (instrMask << 21);
                    } else {
                        ERROR(line, "internal error in instruction table");
                    }
                }
            }
            if (val) {
                if (mask & 0x0003ffff) {
                    *val = *val & mask;
                } else {
                    *val = *val | mask;
                }
            }
        } else {
            ERROR(line, "Internal error: expected instruction modifier found %d", ast->kind);
            return -1;
        }
        ast = ast->right;
    }
    
    /* warn about some instructions not having wc or wz */
    if ((instr->flags & FLAG_WARN_NOTUSED) && !sawFlagUsed) {
        WARNING(line, "instruction %s used without flags being set", instr->name);
    }
    /* parse operands and put them in place */
    switch (instr->ops) {
    case NO_OPERANDS:
        expectops = 0;
        break;
    case TWO_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case TWO_OPERANDS_DEFZ:
    case JMPRET_OPERANDS:
    case P2_TJZ_OPERANDS:
    case P2_TWO_OPERANDS:
    case P2_RDWR_OPERANDS:
    case P2_LOC:
    case P2_MODCZ:
        expectops = 2;
        break;
    case THREE_OPERANDS_BYTE:
    case THREE_OPERANDS_WORD:
    case THREE_OPERANDS_NIBBLE:
        expectops = 3;
        break;
    default:
        expectops = 1;
        break;
    }
    if (instr->ops == TWO_OPERANDS_OPTIONAL && numoperands == 1) {
        // duplicate the operand
        // so neg r0 -> neg r0,r0
        operand[numoperands++] = operand[0];
    } else if (instr->ops == TWO_OPERANDS_DEFZ && numoperands == 1) {
        // supply #0 for second operand
        int defval = 0;
        if (!strcmp(instr->name, "alti")) {
            defval = 0x164; // 1 01_10 0_100
        }
        operand[1] = AstInteger(defval);
        opimm[1] = P2_IMM_SRC;
        numoperands = 2;
    } else if (instr->ops == P2_MODCZ && numoperands == 1) {
        // modc x -> modcz x, 0
        // modz y -> modcz 0, y
        if (!strcmp(instr->name, "modc")) {
            operand[1] = AstInteger(0);
            opimm[1] = 0;
            numoperands = 2;
        } else if (!strcmp(instr->name, "modz")) {
            operand[1] = operand[0];
            operand[0] = AstInteger(0);
            opimm[1] = opimm[0];
            opimm[0] = 0;
            numoperands = 2;
        }
    }
    if (expectops == 3 && numoperands == 1) {
        // SETNIB reg/# -> SETNIB reg/#, 0, #0
        operand[1] = AstInteger(0);
        opimm[1] = 0;
        operand[2] = AstInteger(0);
        opimm[2] = DUMMY_MASK;
        numoperands = 3;
    }
    if (expectops != numoperands) {
        ERROR(line, "Expected %d operands for %s, found %d", expectops, instr->name, numoperands);
        return -1;
    }

    return numoperands;
}

// assemble comments
// returns first non-comment thing seen
static AST* AssembleComments(Flexbuf *f, Flexbuf *relocs, AST *ast)
{
    Reloc r;
    while (ast) {
        if (ast->kind == AST_SRCCOMMENT && gl_srccomments) {
            if (relocs) {
                r.kind = RELOC_KIND_DEBUG;
                r.off = flexbuf_curlen(f);
                r.val = (intptr_t)GetLineInfo(ast);
                flexbuf_addmem(relocs, (const char *)&r, sizeof(r));
            }
        } else if (ast->kind == AST_COMMENT) {
            /* do nothing */
        } else {
            return ast;
        }
        ast = ast->right;
    }
    return ast;
}

/*
 * assemble an instruction, along with its modifiers,
 * into a flexbuf
 */

void
AssembleInstruction(Flexbuf *f, AST *ast, Flexbuf *relocs)
{
    uint32_t val, src, dst;
    uint32_t immmask;
    uint32_t curpc;
    int inHub;
    int32_t isrc;
    Instruction *instr;
    int numoperands;
    AST *operand[MAX_OPERANDS];
    uint32_t opimm[MAX_OPERANDS];
    AST *line = ast;
    char *callname;
    AST *retast;
    AST *origast;
    int isRelJmp = 0;
    int opidx;
    bool needIndirect = false;
    
    extern Instruction instr_p2[];
    curpc = ast->d.ival & 0x00ffffff;
    inHub = (0 == (ast->d.ival & (1<<30)));

    if (relocs && ast->right) {
        AssembleComments(f, relocs, ast->right);
    }
    ast = AssembleComments(f, relocs, ast->left);
        
    instr = (Instruction *)ast->d.ptr;
decode_instr:
    origast = ast;
    val = instr->binary;
    if (instr->opc != OPC_NOP) {
        /* for anything except NOP set the condition to "always" */
        if (gl_p2) {
            val |= 0xf << 28;
        } else {
            val |= 0xf << 18;
        }
    }
    opidx = 0;

    numoperands = DecodeAsmOperands(instr, ast, operand, opimm, &val, NULL);
    if (numoperands < 0) return;

    immmask = 0;

    {
        int j;
        for (j = 0; j < numoperands; j++) {
            immmask |= opimm[j];
        }
    }
    
    src = dst = 0;
    switch (instr->ops) {
    case NO_OPERANDS:
        break;
    case P2_TWO_OPERANDS:
        if (!strcmp(instr->name, "rep")) {
            // special case: rep @x, N says to count the instructions up to x
            // and repeat them N times; fixup the operand here
            if (operand[0]->kind == AST_ADDROF && !opimm[0]) {
                int32_t label = EvalPasmExpr(operand[0]->left);
                int32_t count;
                if (inHub) {
                    count = (label - (curpc+4)) / 4;
                } else {
                    count = (label - (curpc+4)/4);
                }
                operand[0] = AstInteger(count);
                opimm[0] = P2_IMM_DST;
                immmask |= P2_IMM_DST;
            }
        }
        // fall through
    case TWO_OPERANDS:
    case JMPRET_OPERANDS:
    case TWO_OPERANDS_OPTIONAL:
    case TWO_OPERANDS_DEFZ:
    handle_two_operands:
        dst = EvalPasmExpr(operand[0]);
        src = EvalPasmExpr(operand[1]);
        break;
    case P2_MODCZ:
        dst = EvalPasmExpr(operand[0]);
        src = EvalPasmExpr(operand[1]);
        if (dst > 0xf || src > 0xf) {
            ERROR(line, "bad operand for %s", instr->name);
            dst = src = 0;
        }
        dst = (dst<<4) | src;
        src = 0;
        break;
    case P2_RDWR_OPERANDS:
        dst = EvalPasmExpr(operand[0]);
        src = SpecialRdOperand(operand[1], opimm[1]);
        if (src == 0) {
            src = EvalPasmExpr(operand[1]);
        } else {
            immmask |= P2_IMM_SRC;
        }
        break;
    case P2_TJZ_OPERANDS:
        dst = EvalPasmExpr(operand[0]);
        if (!strcmp(instr->name, "calld") && opimm[1] && (dst >= 0x1f6 && dst <= 0x1f9)) {
            int k = 0;
            // use the .loc version of this instruction
            // if we cannot reach the src address
            // or if we are requested to by the user
            if (operand[1]->kind == AST_CATCH) {
                goto force_loc;
            }
            isrc = EvalPasmExpr(operand[1]);
            if (isrc < 0x400) {
                if (inHub) goto force_loc;
                isrc *= 4;
            } else if (!inHub) {
                goto force_loc;
            }
            isrc = (isrc - (curpc+4));
            if (0 != (isrc & 0x3)) {
                // "loc" required
                goto force_loc;
            }
            isrc /= 4;
            if ((isrc >= -256) && (isrc < 255)) {
                goto skip_loc;
            }
        force_loc:
            while (instr_p2[k].name && strcmp(instr_p2[k].name, "calld.loc") != 0) {
                k++;
            }
            if (!instr_p2[k].name) {
                ERROR(line, "Internal error in calld parsing");
                return;
            }
            instr = &instr_p2[k];
            ast = origast;
            goto decode_instr;
        }
    skip_loc:
        /* fall through */
    case P2_JINT_OPERANDS:
        opidx = (instr->ops == P2_TJZ_OPERANDS) ? 1 : 0;
        if (operand[opidx]->kind == AST_CATCH) {
            // asking for absolute address
            ERROR(line, "Absolute address not valid for %s", instr->name);
            isrc = 0;
        } else if (opimm[opidx]) {
            isrc = EvalPasmExpr(operand[opidx]);
            if (isrc < 0x400) {
                isrc *= 4;
            }
            isrc = isrc - (curpc+4);
            isrc /= 4;
            if ( (isrc < -256) || (isrc > 255) ) {
                ERROR(line, "Source out of range for relative branch %s", instr->name);
                isrc = 0;
            }
            src = isrc & 0x1ff;
        } else {
            src = EvalPasmExpr(operand[opidx]);
        }
        break;
    case SRC_OPERAND_ONLY:
    case P2_AUG:
        dst = 0;
        src = EvalPasmExpr(operand[0]);
        break;
    case P2_LOC:
        dst = EvalPasmExpr(operand[0]);
        if (dst >= 0x1f6 && dst <= 0x1f9) {
            val |= ( (dst-0x1f6) & 0x3) << 21;
        } else {
            if (instr->opc == OPC_GENERIC_BRANCH) {
                // try the .ind version
                needIndirect = true;
            } else {
                ERROR(line, "bad first operand to %s instruction", instr->name);
            }
        }
        opidx = 1;
        // fall through
    case P2_JUMP:
        // indirect jump needs to be handled
        if (needIndirect || !opimm[opidx]) {
            char tempName[80];
            int k;
            if (instr->ops == P2_LOC && instr->opc != OPC_GENERIC_BRANCH) {
                ERROR(line, "loc requires immediate operand");
                return;
            }
            strcpy(tempName, instr->name);
            strcat(tempName, ".ind"); // convert to indirect
            k = 0;
            while (instr_p2[k].name && strcmp(tempName, instr_p2[k].name) != 0) {
                k++;
            }
            if (!instr_p2[k].name) {
                ERROR(line, "Internal error, could not find %s\n", tempName);
                return;
            }
            instr = &instr_p2[k];
            ast = origast;
            goto decode_instr;
        }
        dst = 0;
        immmask = 0; // handle immediates specially for jumps
        // figure out if absolute or relative
        // if  crossing from COG to HUB or vice-versa,
        // make it absolute
        // if it's of the form \xxx, which we parse as CATCH(xxx),
        // then make it absolute
        if (operand[opidx]->kind == AST_CATCH) {
            AST *realval = operand[opidx]->left;
            if (realval->kind == AST_FUNCCALL) {
                realval = realval->left;  // correct for parser misreading
            }
            isrc = EvalPasmExpr(realval);
            isRelJmp = 0;
        } else {
            isrc = EvalPasmExpr(operand[opidx]);
            if ( (inHub && isrc < 0x400)
                 || (!inHub && isrc >= 0x400)
                )
            {
                isRelJmp = 0;
            } else {
                isRelJmp = 1;
            }
        }
        if (isRelJmp) {
            if (!inHub) {
                isrc *= 4;
            }
            isrc = isrc - (curpc+4);
            src = isrc & 0xfffff;
            val = val | (1<<20);
        } else {
            src = isrc & 0xfffff;
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
        callname = (char *)malloc(strlen(operand[0]->d.string) + 8);
        strcpy(callname, operand[0]->d.string);
        strcat(callname, "_ret");
        retast = NewAST(AST_IDENTIFIER, NULL, NULL);
        retast->d.string = callname;
        dst = EvalPasmExpr(retast);
        break;
    case THREE_OPERANDS_NIBBLE:
        val = FixupThreeOperands(val, operand[2], opimm[2], 8, line, instr);
        goto handle_two_operands;
    case THREE_OPERANDS_BYTE:
        val = FixupThreeOperands(val, operand[2], opimm[2], 4, line, instr);
        goto handle_two_operands;
    case THREE_OPERANDS_WORD:
        val = FixupThreeOperands(val, operand[2], opimm[2], 2, line, instr);
        goto handle_two_operands;
    default:
        ERROR(line, "Unsupported instruction `%s'", instr->name);
        return;
    }

    if (instr->ops == P2_AUG) {
        if (immmask == 0) {
            ERROR(line, "%s requires immediate operand", instr->name);
        }
        immmask = 0;
        src = src >> 9;
    } else {
        if (immmask & BIG_IMM_DST) {
            uint32_t augval = val & 0xf0000000; // preserve condition
            if (augval == 0) augval = 0xf0000000; // except _ret_
            augval |= (dst >> 9) & 0x007fffff;
            augval |= 0x0f800000; // AUGD
            dst &= 0x1ff;
            outputInstrLong(f, augval);
            immmask &= ~BIG_IMM_DST;
        }
        if (immmask & BIG_IMM_SRC) {
            uint32_t augval = val & 0xf0000000; // preserve condition
            if (augval == 0) augval = 0xf0000000; // except _ret_
            augval |= (src >> 9) & 0x007fffff;
            augval |= 0x0f000000; // AUGS
            src &= 0x1ff;
            outputInstrLong(f, augval);
            immmask &= ~BIG_IMM_SRC;
        }
        if (src > 511) {
            ERROR(line, "Source operand too big for %s", instr->name);
            return;
        }
    }
    if (dst > 511) {
        ERROR(line, "Destination operand too big for %s", instr->name);
        return;
    }
instr_ok:
    val = val | (dst << 9) | src | (immmask & ~0xff);
    /* output the instruction */
    outputInstrLong(f, val);
}

static void
AlignPc(Flexbuf *f, int size)
{
        while ((datacount % size) != 0) {
            outputByte(f, 0);
        }
}

void
outputAlignedDataList(Flexbuf *f, int size, AST *ast, Flexbuf *relocs)
{
    if (size > 1 && !gl_p2) {
        AlignPc(f, size);
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
 * output padding bytes
 */
static void
padBytes(Flexbuf *f, AST *ast, int bytes)
{
    while (bytes-- > 0) {
        outputByte(f, 0);
    }
}

/*
 * print out a data block
 */
void
PrintDataBlock(Flexbuf *f, Module *P, DataBlockOutFuncs *funcs, Flexbuf *relocs)
{
    AST *ast;
    AST *top;
    Reloc r;
    void (*startAst)(Flexbuf *f, AST *ast) = NULL;
    void (*endAst)(Flexbuf *f, AST *ast) = NULL;
    
    initDataOutput(funcs);
    if (funcs) {
        startAst = funcs->startAst;
        endAst = funcs->endAst;
    }
    
    if (gl_errors != 0)
        return;
    for (top = P->datblock; top; top = top->right) {
        ast = top;
        while (ast && ast->kind == AST_COMMENTEDNODE) {
            if (startAst) {
                (*startAst)(f, ast);
            }
            if (endAst) {
                (*endAst)(f, ast);
            }
            ast = ast->left;
        }
        if (!ast) continue;
        if (startAst) {
            (*startAst)(f, ast);
        }
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
        case AST_ALIGN:
        {
            int size = EvalPasmExpr(ast->left);
            AlignPc(f, size);
            break;
        }
        case AST_INSTRHOLDER:
            /* make sure it is aligned */
            if (!gl_p2 && !gl_compressed) {
                while ((datacount % 4) != 0) {
                    outputByte(f, 0);
                }
            }
            AssembleInstruction(f, ast, relocs);
            break;
        case AST_IDENTIFIER:
            /* just skip labels */
            break;
        case AST_FILE:
            assembleFile(f, ast->left);
            break;
        case AST_ORGH:
        case AST_ORGF:
            /* need to skip ahead to PC */
            padBytes(f, ast, ast->d.ival);
            break;
        case AST_ORG:
        case AST_RES:
        case AST_FIT:
        case AST_LINEBREAK:
            break;
        case AST_COMMENT:
            break;
        case AST_SRCCOMMENT:
            if (relocs) {
                // emit a debug entry
                r.kind = RELOC_KIND_DEBUG;
                r.off = flexbuf_curlen(f);
                r.val = (intptr_t)GetLineInfo(ast);
                flexbuf_addmem(relocs, (const char *)&r, sizeof(r));
            }
            break;
        default:
            ERROR(ast, "unknown element in data block");
            break;
        }
        if (endAst) {
            (*endAst)(f, ast);
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
