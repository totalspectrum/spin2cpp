//
// binary data output for spin2cpp
//
// Copyright 2012 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "spinc.h"
#include "outcpp.h"

void
OutputGasFile(const char *fname, Module *P)
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
    PrintDataBlockForGas(&fb, P, 1 /* inline asm */);
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

static void
startLine(Flexbuf *f, int inlineAsm)
{
    if (inlineAsm) {
        flexbuf_printf(f, "\"");
    }
}

static void
endLine(Flexbuf *f, int inlineAsm)
{
    if (inlineAsm) {
        flexbuf_printf(f, "\\n\"");
    }
    flexbuf_printf(f, "\n");
}

static void
outputGasDataList(Flexbuf *f, const char *prefix, AST *ast, int size, int inlineAsm)
{
    int reps;
    AST *sub;
    char *comma = "";
    AST *origval = NULL;

    if ( (datacount % size) != 0 ) {
        flexbuf_printf(f, "\t\t.balign\t%d", size);
        endLine(f, inlineAsm);
        datacount = (datacount + size - 1) & ~(size-1);
        startLine(f, inlineAsm);
    }
    flexbuf_printf(f, "\t\t%s\t", prefix);
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
                if (0 && val >= ' ' && val < 0x7f) {
                    flexbuf_printf(f, "%s'%c'", comma, val);
                } else {
                    flexbuf_printf(f, "%s%d", comma, val);
                }
                comma = ", ";
                datacount += size;
            }
            reps = 0;
        } else {
            origval = ast->left;
            reps = 1;
        }
        while (reps > 0) {
            flexbuf_printf(f, "%s", comma);
            PrintGasExpr(f, origval);
            comma = ", ";
            --reps;
            datacount += size;
        }
        ast = ast->right;
    }
}

static void
outputGasDirective(Flexbuf *f, const char *prefix, AST *expr)
{
    flexbuf_printf(f, "\t\t%s\t", prefix);
    if (expr)
        PrintExpr(f, expr);
    else
        flexbuf_printf(f, "0");
}

#define GAS_WZ 1
#define GAS_WC 2
#define GAS_NR 4
#define GAS_WR 8
#define MAX_OPERANDS 2

static void
outputGasInstruction(Flexbuf *f, AST *ast, int inlineAsm)
{
    Instruction *instr;
    AST *operand[MAX_OPERANDS];
    AST *sub;
    int immflag = 0;
    int effects = 0;
    int i;
    int numoperands = 0;
    const char *opcode;

    if ( (datacount % 4) != 0) {
        flexbuf_printf(f, "\t.balign 4");
        endLine(f, inlineAsm);
        datacount = (datacount + 3) & ~3;
        startLine(f, inlineAsm);
    }
    
    instr = (Instruction *)ast->d.ptr;
    flexbuf_printf(f, "\t");
    /* print modifiers */
    sub = ast->right;
    while (sub != NULL) {
        if (sub->kind == AST_INSTRMODIFIER) {
            InstrModifier *mod = sub->d.ptr;
            if (!strncmp(mod->name, "if_", 3)) {
                flexbuf_printf(f, "%s ", mod->name);
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
    flexbuf_printf(f, "\t%s", opcode);
    datacount += 4;
    /* now print the operands */
    for (i = 0; i < numoperands; i++) {
        if (i == 0)
            flexbuf_printf(f, "\t");
        else
            flexbuf_printf(f, ", ");
        if (immflag) {
            switch (instr->ops) {
            case CALL_OPERAND:
            case SRC_OPERAND_ONLY:
                if (i == 0) {
                    flexbuf_printf(f, "#");
                    immflag = 0;
                }
                break;
            default:
                if (i == 1) {
                    flexbuf_printf(f, "#");
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
        flexbuf_printf(f, "\t");
        for (i = 0; i < 4; i++) {
            if (effects & (1<<i)) {
                flexbuf_printf(f, "%s%s", comma, effnames[i]);
                comma = ", ";
            }
        }
    }
}

static void
outputGasLabel(Flexbuf *f, AST *id)
{
    flexbuf_printf(f, "%s", id->d.string);
}

static void
PrintGasConstantDecl(Flexbuf *f, AST *ast, int inlineAsm)
{
    startLine(f, inlineAsm);
    flexbuf_printf(f, "\t\t.equ\t%s, ", ast->d.string);
    PrintInteger(f, EvalConstExpr(ast));
    endLine(f, inlineAsm);
}

void
PrintConstantsGas(Flexbuf *f, Module *P, int inlineAsm)
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
            case AST_ENUMSKIP:
                PrintGasConstantDecl(f, ast->left, inlineAsm);
                break;
            case AST_COMMENTEDNODE:
                // FIXME: these nodes are backwards, the rest of the list is on the left
                // also, should we print the comment here?
                ast = ast->left;
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
PrintDataBlockForGas(Flexbuf *f, Module *P, int inlineAsm)
{
    AST *ast;
    int saveState;

    if (gl_errors != 0)
        return;
    saveState = P->printLabelsVerbatim;
    P->printLabelsVerbatim = 1;

    if (inlineAsm) {
        flexbuf_printf(f, "__asm__(\n");
        flexbuf_printf(f, "\"\t\t.section .%s.cog, \\\"ax\\\"\\n\"\n",
                P->basename);
    }
    /* print constant declarations */
    PrintConstantsGas(f, P, inlineAsm);
    if (inlineAsm) {
        flexbuf_printf(f, "\"\t\t.compress off\\n\"\n");
    }
    startLine(f, inlineAsm);
    flexbuf_printf(f, "..start");
    endLine(f, inlineAsm);
    for (ast = P->datblock; ast; ast = ast->right) {
        /* print anything for start of line here */
        startLine(f, inlineAsm);
        switch (ast->kind) {
        case AST_BYTELIST:
            outputGasDataList(f, ".byte", ast->left, 1, inlineAsm);
            break;
        case AST_WORDLIST:
            outputGasDataList(f, ".word", ast->left, 2, inlineAsm);
            break;
        case AST_LONGLIST:
            outputGasDataList(f, ".long", ast->left, 4, inlineAsm);
            break;
        case AST_INSTRHOLDER:
            outputGasInstruction(f, ast->left, inlineAsm);
            break;
        case AST_LINEBREAK:
            break;
        case AST_IDENTIFIER:
            // FIXME: need to handle labels not on lines (type and alignment)
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
        flexbuf_printf(f, "\"\t\t.compress default\\n\"\n");
        flexbuf_printf(f, "\"\t\t.text\\n\"\n");
        flexbuf_printf(f, "\n);\n");
    }
    P->printLabelsVerbatim = saveState;
}
