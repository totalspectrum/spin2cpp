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

static void lstPutByte(Flexbuf *f, int c) {
    if (bytesOnLine < 4) {
        flexbuf_printf(f, "%02x", c);
        bytesOnLine++;
    }
    hubPc++;
    cogPc++;
}

//
// every byte will start with 20 bytes
//  HHHHH CCC DDDDDDDD  (two spaces at end)
// HHHHH is the hub address
// CCC is the cog address, or blank
// DDDDDDDD is up to 4 bytes of data

static void startNewLine(Flexbuf *f) {
    bytesOnLine = 0;
    flexbuf_printf(f, "%05x ", hubPc);
    if (inCog) {
        flexbuf_printf(f, "%03x ", cogPc / 4);
    } else {
        flexbuf_printf(f, "    ");
    }
}

static void AddRestOfLine(Flexbuf *f, const char *s) {
    while (bytesOnLine < 4) {
        flexbuf_printf(f, "  ");
        bytesOnLine++;
    }
    flexbuf_printf(f, "  %s\n", s);
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

static void lstStartAst(Flexbuf *f, AST *ast)
{
    if (!ast) return;
    catchUpToLine(f, ast->fileName, ast->line, true);

    startNewLine(f);
}

static void lstEndAst(Flexbuf *f, AST *ast)
{
    catchUpToLine(f, ast->fileName, ast->line+1, false);
    // update PCs as appropriate
    switch (ast->kind) {
    case AST_ORG:
        cogPc = ast->d.ival;
        inCog = 1;
        break;
    case AST_ORGH:
        inCog = 0;
        hubPc = ast->d.ival;
        break;
    case AST_INSTRHOLDER:
        inCog = !(ast->d.ival & (1<<30));
        if (inCog) {
            cogPc = (ast->d.ival & 0x00ffffff);
        } else {
            hubPc = (ast->d.ival & 0x00ffffff);
        }
        break;
    default:
        break;
    }
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
