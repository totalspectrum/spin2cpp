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
 * print out a data block
 */
void
PrintDataBlock(FILE *f, ParserState *P)
{
    AST *ast;

    initDataOutput();
    //DeclareLabels(P);
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
        default:
            ERROR("unknown element in data block");
            break;
        }
    }

    if (datacount != 0) {
        fprintf(f, "\n");
    }
}
