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
    unsigned val, origval;
    int i, reps;

    while (ast) {
        if (ast->left->kind == AST_ARRAYDECL) {
            origval = EvalPasmExpr(ast->left->left);
            reps = EvalPasmExpr(ast->left->right);
        } else {
            origval = EvalPasmExpr(ast->left);
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
    AST *line = ast;
    char *callname;
    AST *retast;

    instr = (Instruction *)ast->d.ptr;
    val = instr->binary;
    if (instr->ops != NOP_OPERANDS) {
        /* for anything except NOP set the condition to "always" */
        val |= 0xf << 18;
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
            mask = ast->d.ival;
            if (mask & 0x80000000) {
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
    case NOP_OPERANDS:
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
        ERROR(line, "Expected %d operands for %s, found %d", expectops, instr->name, numoperands);
        return;
    }
    src = dst = 0;
    switch (instr->ops) {
    case NO_OPERANDS:
    case NOP_OPERANDS:
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
    if (src > 511) {
        ERROR(line, "Source operand too big for %s", instr->name);
        return;
    }
    if (dst > 511) {
        ERROR(line, "Destination operand too big for %s", instr->name);
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
    unsigned numelems;
    while (ast) {
        if (ast->left && ast->left->kind == AST_ARRAYDECL) {
            numelems = EvalConstExpr(ast->left->right);
        } else {
            numelems = 1;
        }
        size += (numelems*elemsize);
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
EnterLabel(ParserState *P, const char *name, long offset, long asmpc, AST *ltype)
{
    Label *labelref;

    labelref = calloc(1, sizeof(*labelref));
    if (!labelref) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    labelref->offset = offset;
    labelref->asmval = asmpc;
    labelref->type = ltype;
    AddSymbol(&P->objsyms, name, SYM_LABEL, labelref);
}

/*
 * emit pending labels
 */
AST *
emitPendingLabels(ParserState *P, AST *label, unsigned pc, unsigned asmpc, AST *ltype)
{
    while (label) {
        EnterLabel(P, label->left->d.string, pc, asmpc, ltype);
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

#define ALIGNPC(size)  do { inc = size; asmpc = align(asmpc, inc); datoff = align(datoff, inc); } while (0)
#define INCPC(size)  do { inc = size; asmpc += inc; datoff += inc; } while (0)

void
DeclareLabels(ParserState *P)
{
    unsigned asmpc = 0;
    unsigned datoff = 0;
    unsigned inc = 0;
    unsigned delta;

    AST *ast = NULL;
    AST *pendingLabels = NULL;

    for (ast = P->datblock; ast; ast = ast->right) {
        switch (ast->kind) {
        case AST_BYTELIST:
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_byte);
            INCPC(dataListLen(ast->left, 1));
            break;
        case AST_WORDLIST:
            ALIGNPC(2);
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_word);
            INCPC(dataListLen(ast->left, 2));
            break;
        case AST_LONGLIST:
            ALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_long);
            INCPC(dataListLen(ast->left, 4));
            break;
        case AST_INSTRHOLDER:
            ALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_long);
            replaceHeres(ast->left, asmpc/4);
            INCPC(4);
            break;
        case AST_IDENTIFIER:
            pendingLabels = AddToList(pendingLabels, NewAST(AST_LISTHOLDER, ast, NULL));
            break;
        case AST_ORG:
            if (ast->left) {
                asmpc = EvalPasmExpr(ast->left);
            } else {
                asmpc = 0;
            }
            break;
        case AST_RES:
            asmpc = align(asmpc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_long);
            delta = EvalPasmExpr(ast->left);
            asmpc += 4*delta;
            break;
        case AST_FIT:
            asmpc = align(asmpc, 4);
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_long);
            if (ast->left) {
                int32_t max = EvalConstExpr(ast->left);
                int32_t cur = (asmpc) / 4;
                if ( cur > max ) {
                    ERROR(ast, "fit %d failed: pc is %d", max, cur);
                }
            }
            break;
        default:
            ERROR(ast, "unknown element %d in data block", ast->kind);
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
        case AST_RES:
        case AST_FIT:
            break;
        default:
            ERROR(ast, "unknown element in data block");
            break;
        }
    }

    if (datacount != 0) {
        fprintf(f, "\n");
    }
}
