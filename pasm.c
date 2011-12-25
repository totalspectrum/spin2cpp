/*
 * PASM and data compilation
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "spinc.h"

/*
 * data block printing functions
 */
#define BYTES_PER_LINE 16  /* must be at least 4 */
int datacount = 0;

static void
initDataOutput(void)
{
    datacount = 0;
}

static void
outputByte(FILE *f, int c)
{
    if (datacount == 0) {
        fprintf(f, "  ");
    }
    fprintf(f, "0x%02x, ", c);
    datacount++;
    if (datacount == BYTES_PER_LINE) {
        fprintf(f, "\n");
        datacount = 0;
    }
}

void
outputDataList(FILE *f, int size, AST *ast)
{
    unsigned val;
    int i;

    while (ast) {
        val = EvalConstExpr(ast->left);
        for (i = 0; i < size; i++) {
            outputByte(f, val & 0xff);
            val = val >> 8;
        }
        ast = ast->right;
    }
}

/*
 * assemble an instruction, along with its modifiers
 */
#define MAX_OPERANDS 2

void
assembleInstruction(FILE *f, AST *ast)
{
    uint32_t val, mask, src, dst;
    Instruction *instr;
    int i, numoperands, expectops;
    AST *operand[MAX_OPERANDS];

    instr = (Instruction *)ast->d.ptr;
    val = instr->binary;
    if (val != 0) {
        /* for anything except NOP set the condition to "always" */
        val |= 0xf << 18;
    }
    /* check for modifiers and operands */
    numoperands = 0;
    ast = ast->right;
    while (ast != NULL) {
        if (ast->kind == AST_EXPRLIST) {
            if (numoperands >= MAX_OPERANDS) {
                ERROR("Too many operands to instruction");
                return;
            }
            operand[numoperands++] = ast->left;
        } else if (ast->kind != AST_INSTRMODIFIER) {
            ERROR("Internal error: expected instruction modifier found %d", ast->kind);
            return;
        }
        mask = ast->d.ival;
        if (mask & 0x80000000U) {
            val = val & mask;
        } else {
            val = val | mask;
        }
        ast = ast->right;
    }

    /* parse operands and put them in place */
    switch (instr->ops) {
    case NO_OPERANDS:
        expectops = 0;
        break;
    case TWO_OPERANDS:
        expectops = 2;
        break;
    default:
        expectops = 1;
        break;
    }
    if (expectops != numoperands) {
        ERROR("Expected %d operands for %s, found %d", expectops, instr->name, numoperands);
        return;
    }
    src = dst = 0;
    switch (instr->ops) {
    case NO_OPERANDS:
        break;
    case TWO_OPERANDS:
        dst = EvalPasmExpr(operand[0]);
        src = EvalPasmExpr(operand[1]);
        break;
    case SRC_OPERAND_ONLY:
        dst = 0;
        src = EvalPasmExpr(operand[0]);
        break;
    case DST_OPERAND_ONLY:
        dst = EvalPasmExpr(operand[0]);
        src = 0;
        break;
    default:
        ERROR("Unsupported instruction %s", instr->name);
        return;
    }
    if (src > 511) {
        ERROR("Source operand too big for %s", instr->name);
        return;
    }
    if (dst > 511) {
        ERROR("Destination operand too big for %s", instr->name);
        return;
    }
    val = val | (dst << 9) | src;
    /* output the instruction */
    for (i = 0; i < 4; i++) {
        outputByte(f, val & 0xff);
        val = val >> 8;
    }
}

void
outputAlignedDataList(FILE *f, int size, AST *ast)
{
    if (size > 1) {
        while ((datacount % size) != 0) {
            outputByte(f, 0);
        }
    }
    outputDataList(f, size, ast);
}

/*
 * find the length of a data list, in bytes
 */
unsigned
dataListLen(AST *ast, int elemsize)
{
    unsigned size = 0;

    while (ast) {
        size += elemsize;
        ast = ast->right;
    }
    return size;
}

/*
 * align a pc on a boundary
 */
unsigned
align(unsigned pc, int size)
{
    pc = (pc + (size-1)) & ~(size-1);
    return pc;
}

/*
 * enter a label
 */
void
EnterLabel(ParserState *P, const char *name, long offset, long asmpc)
{
    Symbol *sym;
    Label *labelref;

    labelref = calloc(1, sizeof(*labelref));
    if (!labelref) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    labelref->offset = offset;
    labelref->asmval = asmpc;
    labelref->type = ast_type_long;
    sym = AddSymbol(&P->objsyms, name, SYM_LABEL, labelref);
}

/*
 * emit pending labels
 */
AST *
emitPendingLabels(ParserState *P, AST *label, unsigned pc, unsigned asmpc)
{
    while (label) {
        EnterLabel(P, label->left->d.string, pc, asmpc);
        label = label->right;
    }
    return NULL;
}

/*
 * replace AST_HERE with AST_INTEGER having pc as its value
 */
void
replaceHeres(AST *ast, uint32_t asmpc)
{
    if (ast == NULL)
        return;
    if (ast->kind == AST_HERE) {
        ast->kind = AST_INTEGER;
        ast->d.ival = asmpc;
        return;
    }
    replaceHeres(ast->left, asmpc);
    replaceHeres(ast->right, asmpc);
}

/*
 * declare labels for a data block
 */
void
DeclareLabels(ParserState *P)
{
    unsigned pc = 0;
    unsigned asmbase = 0;
    unsigned asmpc = 0;
    unsigned offset;
    AST *ast = NULL;
    AST *pendingLabels = NULL;

    for (ast = P->datblock; ast; ast = ast->right) {
        asmpc = pc - asmbase;
        switch (ast->kind) {
        case AST_BYTELIST:
            pendingLabels = emitPendingLabels(P, pendingLabels, pc, asmpc);
            pc += dataListLen(ast->left, 1);
            break;
        case AST_WORDLIST:
            pc = align(pc, 2);
            pendingLabels = emitPendingLabels(P, pendingLabels, pc, asmpc);
            pc += dataListLen(ast->left, 2);
            break;
        case AST_LONGLIST:
            pc = align(pc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, pc, asmpc);
            pc += dataListLen(ast->left, 4);
            break;
        case AST_INSTRHOLDER:
            pc = align(pc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, pc, asmpc);
            replaceHeres(ast->left, asmpc/4);
            pc += 4;
            break;
        case AST_IDENTIFIER:
            pendingLabels = AddToList(pendingLabels, NewAST(AST_LISTHOLDER, ast, NULL));
            break;
        case AST_ORG:
            if (ast->left) {
                offset = EvalPasmExpr(ast->left);
            } else {
                offset = 0;
            }
            asmbase = pc + offset;
            break;
        default:
            ERROR("unknown element %d in data block", ast->kind);
            break;
        }
    }
}

/*
 * print out a data block
 */
void
PrintDataBlock(FILE *f, ParserState *P)
{
    AST *ast;

    initDataOutput();
    DeclareLabels(P);
    if (gl_errors != 0)
        return;
    for (ast = P->datblock; ast; ast = ast->right) {
        switch (ast->kind) {
        case AST_BYTELIST:
            outputAlignedDataList(f, 1, ast->left);
            break;
        case AST_WORDLIST:
            outputAlignedDataList(f, 2, ast->left);
            break;
        case AST_LONGLIST:
            outputAlignedDataList(f, 4, ast->left);
            break;
        case AST_INSTRHOLDER:
            assembleInstruction(f, ast->left);
            break;
        case AST_IDENTIFIER:
            /* just skip labels */
            break;
        case AST_ORG:
            break;
        default:
            ERROR("unknown element in data block");
            break;
        }
    }

    if (datacount != 0) {
        fprintf(f, "\n");
    }
}
