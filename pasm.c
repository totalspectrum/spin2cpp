/*
 * PASM and data compilation
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "spinc.h"

/*
 * data block printing functions
 */
#define BYTES_PER_LINE 16  /* must be at least 4 */
int datacount = 0;
int totaldata = 0;

static int binFlag = 0;

static void
initDataOutput(int isBinary)
{
    totaldata = datacount = 0;
    binFlag = isBinary;
}

static void
outputByte(FILE *f, int c)
{
    if (binFlag) {
        fputc(c, f);
        datacount++;
        totaldata++;
        return;
    }
    if (datacount == 0) {
        fprintf(f, "  ");
    }
    fprintf(f, "0x%02x, ", c);
    datacount++;
    totaldata++;
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
    AST *sub;

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
            InstrModifier *mod = (InstrModifier *)ast->d.ptr;
            mask = mod->modifier;
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
    case JMPRET_OPERANDS:
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
    case JMPRET_OPERANDS:
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
    /* make sure it is aligned */
    while ((datacount % 4) != 0) {
        outputByte(f, 0);
    }
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
    AST *sub;

    while (ast) {
        sub = ast->left;
        if (sub) {
            if (sub->kind == AST_ARRAYDECL) {
                numelems = EvalPasmExpr(ast->left->right);
            } else if (sub->kind == AST_STRING) {
                numelems = strlen(sub->d.string);
            } else {
                numelems = 1;
            }
            size += (numelems*elemsize);
        }
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
EnterLabel(ParserState *P, AST *origLabel, long offset, long asmpc, AST *ltype)
{
    const char *name;
    Label *labelref;

    labelref = calloc(1, sizeof(*labelref));
    if (!labelref) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    name = origLabel->left->d.string;
    labelref->offset = offset;
    labelref->asmval = asmpc;
    labelref->type = ltype;
    if (!AddSymbol(&P->objsyms, name, SYM_LABEL, labelref)) {
      ERROR(origLabel, "Duplicate definition of label %s", name);
    }
}

/*
 * emit pending labels
 */
AST *
emitPendingLabels(ParserState *P, AST *label, unsigned pc, unsigned asmpc, AST *ltype)
{
    while (label) {
        EnterLabel(P, label, pc, asmpc, ltype);
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
 * do replaceHeres on each element of a long list
 */
void
replaceHereDataList(AST *ast, uint32_t asmpc, uint32_t inc)
{
    AST *sub;

    while (ast) {
        sub = ast->left;
        if (sub)
            replaceHeres(sub, asmpc/4);
        asmpc += inc;
        ast = ast->right;
    }
}

/*
 * find the length of a file
 */
static long
filelen(AST *ast)
{
    FILE *f;
    const char *name = ast->d.string;
    int r;
    long siz;

    f = fopen(name, "rb");
    if (!f) {
        ERROR(ast, "file %s: %s", name, strerror(errno));
        return 0;
    }
    r = fseek(f, 0L, SEEK_END);
    if (r < 0) {
        ERROR(ast, "file %s: %s", name, strerror(errno));
        return 0;
    }
    siz = ftell(f);
    fclose(f);
    return siz;
}

/*
 * output bytes for a file
 */
static void
assembleFile(FILE *f, AST *ast)
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
            replaceHereDataList(ast->left, asmpc, 1);
            INCPC(dataListLen(ast->left, 1));
            break;
        case AST_WORDLIST:
            ALIGNPC(2);
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_word);
            replaceHereDataList(ast->left, asmpc, 2);
            INCPC(dataListLen(ast->left, 2));
            break;
        case AST_LONGLIST:
            ALIGNPC(4);
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_long);
            replaceHereDataList(ast->left, asmpc, 4);
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
                replaceHeres(ast->left, asmpc/4);
                asmpc = 4*EvalPasmExpr(ast->left);
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
                int32_t max = EvalPasmExpr(ast->left);
                int32_t cur = (asmpc) / 4;
                if ( cur > max ) {
                    ERROR(ast, "fit %d failed: pc is %d", max, cur);
                }
            }
            break;
        case AST_FILE:
            pendingLabels = emitPendingLabels(P, pendingLabels, datoff, asmpc, ast_type_byte);
            INCPC(filelen(ast->left));
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
PrintDataBlock(FILE *f, ParserState *P, int isBinary)
{
    AST *ast;

    initDataOutput(isBinary);
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
        case AST_FILE:
            assembleFile(f, ast->left);
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

    if (datacount != 0 && !isBinary) {
        fprintf(f, "\n");
    }
}

static void
outputGasDataList(FILE *f, const char *prefix, AST *ast)
{
    int reps;
    AST *sub;
    char *comma = "";
    AST *origval = NULL;

    fprintf(f, "\t\t%s\t", prefix);
    while (ast) {
        sub = ast->left;
        if (sub->kind == AST_ARRAYDECL) {
            origval = ast->left->left;
            reps = EvalPasmExpr(ast->left->right);
        } else if (sub->kind == AST_STRING) {
            const char *ptr = sub->d.string;
            unsigned val;
            while (*ptr) {
                val = (*ptr++) & 0xff;
                if (val >= ' ' && val < 0x7f) {
                    fprintf(f, "%s'%c'", comma, val);
                } else {
                    fprintf(f, "%s%d", comma, val);
                }
                comma = ", ";
            }
            reps = 0;
        } else {
            origval = ast->left;
            reps = 1;
        }
        while (reps > 0) {
            fprintf(f, "%s", comma);
            PrintExpr(f, origval);
            comma = ", ";
            --reps;
        }
        ast = ast->right;
    }
}

static void
outputGasDirective(FILE *f, const char *prefix, AST *expr)
{
    fprintf(f, "\t\t%s\t", prefix);
    if (expr)
        PrintExpr(f, expr);
    else
        fprintf(f, "0");
}

#define GAS_WZ 1
#define GAS_WC 2
#define GAS_NR 4
#define GAS_WR 8

static void
outputGasInstruction(FILE *f, AST *ast)
{
    Instruction *instr;
    AST *operand[MAX_OPERANDS];
    AST *sub;
    int immflag = 0;
    int effects = 0;
    int i;
    int numoperands = 0;
    const char *opcode;

    instr = (Instruction *)ast->d.ptr;
    fprintf(f, "\t");
    /* print modifiers */
    sub = ast->right;
    while (sub != NULL) {
        if (sub->kind == AST_INSTRMODIFIER) {
            InstrModifier *mod = sub->d.ptr;
            if (!strncmp(mod->name, "if_", 3)) {
                fprintf(f, "%s ", mod->name);
            } else if (!strcmp(mod->name, "wz")) {
                effects |= GAS_WZ;
            } else if (!strcmp(mod->name, "wc")) {
                effects |= GAS_WC;
            } else if (!strcmp(mod->name, "wr")) {
                effects |= GAS_WR;
            } else if (!strcmp(mod->name, "nr")) {
                effects |= GAS_NR;
            } else if (!strcmp(mod->name, "#")) {
                immflag = 1;
            } else {
                ERROR(sub, "unknown modifier %s", mod->name);
            }
        } else if (sub->kind == AST_EXPRLIST) {
            if (numoperands >= MAX_OPERANDS) {
                ERROR(ast, "Too many operands to instruction");
                return;
            }
            operand[numoperands++] = sub->left;
        } else {
            ERROR(ast, "Internal error parsing instruction");
        }
        sub = sub->right;
    }

    /* print instruction opcode */
    opcode = instr->name;
    fprintf(f, "\t%s", opcode);
    /* now print the operands */
    for (i = 0; i < numoperands; i++) {
        if (i == 0)
            fprintf(f, "\t");
        else
            fprintf(f, ", ");
        if (immflag) {
            switch (instr->ops) {
            case CALL_OPERAND:
            case SRC_OPERAND_ONLY:
                if (i == 0) {
                    fprintf(f, "#");
                    immflag = 0;
                }
                break;
            default:
                if (i == 1) {
                    fprintf(f, "#");
                    immflag = 0;
                    if (instr->ops == TWO_OPERANDS)
                    {
                        current->fixImmediate = 1;
                    }
                }
                break;
            }
        }
        PrintExpr(f, operand[i]);
        current->fixImmediate = 0;
    }
    if (effects) {
        const char *comma = "";
        const char *effnames[] = { "wz", "wc", "nr", "wr" };
        fprintf(f, "\t");
        for (i = 0; i < 4; i++) {
            if (effects & (1<<i)) {
                fprintf(f, "%s%s", comma, effnames[i]);
                comma = ", ";
            }
        }
    }
}

static void
outputGasLabel(FILE *f, AST *id)
{
    fprintf(f, "%s", id->d.string);
}

static void
startLine(FILE *f, int inlineAsm)
{
    if (inlineAsm) {
        fprintf(f, "\"");
    }
}

static void
endLine(FILE *f, int inlineAsm)
{
    if (inlineAsm) {
        fprintf(f, "\\n\"");
    }
    fprintf(f, "\n");
}

static void
PrintGasConstantDecl(FILE *f, AST *ast, int inlineAsm)
{
    startLine(f, inlineAsm);
    fprintf(f, "\t\t.equ\t%s, ", ast->d.string);
    PrintInteger(f, EvalConstExpr(ast));
    endLine(f, inlineAsm);
}

void
PrintConstantsGas(FILE *f, ParserState *P, int inlineAsm)
{
    AST *upper, *ast;

    for (upper = P->conblock; upper; upper = upper->right) {
        ast = upper->left;
        while (ast) {
            switch (ast->kind) {
            case AST_IDENTIFIER:
                PrintGasConstantDecl(f, ast, inlineAsm);
                ast = ast->right;
                break;
            case AST_ASSIGN:
                PrintGasConstantDecl(f, ast->left, inlineAsm);
                ast = NULL;
                break;
            default:
                /* do nothing */
                ast = ast->right;
                break;
            }
        }
    }
}

void
PrintDataBlockForGas(FILE *f, ParserState *P, int inlineAsm)
{
    AST *ast;
    int saveState;

    if (gl_errors != 0)
        return;
    saveState = P->printLabelsVerbatim;
    P->printLabelsVerbatim = 1;

    if (inlineAsm) {
        fprintf(f, "__asm__(\n");
        fprintf(f, "\"\t\t.section .%s.cog, \\\"ax\\\"\\n\"\n",
                P->basename);
    }
    /* print constant declarations */
    PrintConstantsGas(f, P, inlineAsm);
    if (inlineAsm) {
        fprintf(f, "\"\t\t.compress off\\n\"\n");
    }
    startLine(f, inlineAsm);
    fprintf(f, "..start");
    endLine(f, inlineAsm);
    for (ast = P->datblock; ast; ast = ast->right) {
        /* print anything for start of line here */
        startLine(f, inlineAsm);
        switch (ast->kind) {
        case AST_BYTELIST:
            outputGasDataList(f, ".byte", ast->left);
            break;
        case AST_WORDLIST:
            outputGasDataList(f, ".word", ast->left);
            break;
        case AST_LONGLIST:
            outputGasDataList(f, ".long", ast->left);
            break;
        case AST_INSTRHOLDER:
            outputGasInstruction(f, ast->left);
            break;
        case AST_IDENTIFIER:
            outputGasLabel(f, ast);
            break;
        case AST_FILE:
            ERROR(ast, "File directive not supported in GAS output");
            break;
        case AST_ORG:
            outputGasDirective(f, ".org", ast->left);
            break;
        case AST_RES:
            outputGasDirective(f, ".res", ast->left);
            break;
        case AST_FIT:
            outputGasDirective(f, ".fit", ast->left ? ast->left : AstInteger(496));
            break;
        default:
            ERROR(ast, "unknown element in data block");
            break;
        }
        /* print end of line stuff here */
        endLine(f, inlineAsm);
    }

    if (inlineAsm) {
        fprintf(f, "\"\t\t.compress default\\n\"\n");
        fprintf(f, "\"\t\t.text\\n\"\n");
        fprintf(f, "\n);\n");
    }
    P->printLabelsVerbatim = saveState;
}
