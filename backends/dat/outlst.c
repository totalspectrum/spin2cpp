#include "spinc.h"
#include <stdio.h>
#include <stdlib.h>

uint32_t cogPc;
uint32_t hubPc;
int inCog;
int bytesOnLine;

static LexStream *current_lex = NULL;

static void initOutput(Module *P)
{
    inCog = 0;
    cogPc = hubPc = 0;
    bytesOnLine = 0;
    current_lex = &P->L;
    current_lex->lineCounter = 0;
}

//
// every byte will start with 24 bytes
//  HHHHH CCC DDDDDDDD  |   (3 spaces at end)
// HHHHH is the hub address
// CCC is the cog address, or blank
// DDDDDDDD is up to 4 bytes of data

static void startNewLine(Flexbuf *f) {
    bytesOnLine = 0;
    flexbuf_printf(f, "\n%05x ", hubPc);
    if (inCog) {
        flexbuf_printf(f, "%03x ", cogPc / 4);
    } else {
        flexbuf_printf(f, "    ");
    }
}

static void lstPutByte(Flexbuf *f, int c) {
    if (bytesOnLine >= 4) {
        startNewLine(f);
    }
    flexbuf_printf(f, "%02X", c);
    bytesOnLine++;
    hubPc++;
    cogPc++;
}

static void AddRestOfLine(Flexbuf *f, const char *s) {
    int c;
    while (bytesOnLine < 4) {
        flexbuf_printf(f, "  ");
        bytesOnLine++;
    }
    // at this point we have written 6+4+8 == 18 characters
    // line it up to 24 to make tabs work
    flexbuf_printf(f, "    | ");
    while (*s) {
        c = *s++;
        if (c == '\n') {
            break;
        }
        flexbuf_addchar(f, c);
    }
}

static void AppendOneSrcLine(Flexbuf *f, int line, LexStream *L)
{
    const char **srclines;
    const char *theline;
    int maxline;

    srclines = (const char **)flexbuf_peek(&L->lines);
    maxline = flexbuf_curlen(&L->lines) / sizeof(char **);
    theline = srclines[line];
    if (!theline) return;
    if (line < 0 || line >= maxline) return;
    
    // skip over preprocessor comments
    if (theline[0] == '{' && theline[1] == '#') {
        theline += 2;
        while (*theline && *theline != '}') theline++;
        if (*theline) theline++;
    }

    AddRestOfLine(f, theline);
}

static void catchUpToLine(Flexbuf *f, const char *fileName, int line, bool needsStart)
{
    LexStream *L;
    int i;
    
    L = &current->L;
    if (strcmp(L->fileName, fileName) != 0) {
        // wrong file, just bail
        return;
    }
    i = L->lineCounter;
    while (i < line) {
        if (needsStart) {
            startNewLine(f);
        }
        needsStart = true;
        AppendOneSrcLine(f, i, L);
        i++;
    }
    L->lineCounter = i;
}

static int ignoreAst(AST *ast)
{
    if (!ast) return 1;
    switch (ast->kind) {
    case AST_COMMENT:
    case AST_COMMENTEDNODE:
    case AST_LINEBREAK:
        return 1;
    default:
        return 0;
    }
}
static void lstStartAst(Flexbuf *f, AST *ast)
{
    if (ignoreAst(ast)) {
        return;
    }
    catchUpToLine(f, ast->fileName, ast->line, true);

    // update PCs as appropriate
    switch (ast->kind) {
    case AST_ORG:
        inCog = 1;
        cogPc = 4*EvalPasmExpr(ast->left);
        break;
    case AST_ORGH:
        inCog = 0;
        break;
    case AST_INSTRHOLDER:
        inCog = (ast->d.ival & (1<<30)) != 0;
        if (inCog) {
            cogPc = (ast->d.ival & 0x00ffffff);
        } else {
            hubPc = (ast->d.ival & 0x00ffffff);
        }
        break;
    default:
        break;
    }
    startNewLine(f);
}

static void lstEndAst(Flexbuf *f, AST *ast)
{
    if (ignoreAst(ast)) {
        return;
    }
    // do anything special we need to here
    switch (ast->kind) {
    case AST_IDENTIFIER:
        /* do not catch up to line, yet */
        return;
    case AST_ORGH:
        hubPc = ast->d.ival;
        break;
    default:
        break;
    }
    catchUpToLine(f, ast->fileName, ast->line+1, false);
}

static DataBlockOutFuncs lstOutputFuncs = {
    lstStartAst,
    lstPutByte,
    lstEndAst,
};

void
OutputLstFile(const char *fname, Module *P)
{
    FILE *f = NULL;
    Module *save = current;
    Flexbuf fb;
    size_t curlen;
    
    f = fopen(fname, "wb");
    if (!f) {
        fprintf(stderr, "Unable to open output file: ");
        perror(fname);
        exit(1);
    }
    current = P;
    flexbuf_init(&fb, BUFSIZ);
    initOutput(P);
    
    PrintDataBlock(&fb, P, &lstOutputFuncs, NULL);
    
    curlen = flexbuf_curlen(&fb);
    fwrite(flexbuf_peek(&fb), curlen, 1, f);
    fclose(f);
    flexbuf_delete(&fb);
    current = save;
}
