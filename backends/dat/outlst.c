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
    cogPc = 0;
    hubPc = gl_dat_offset;
    if (hubPc == (uint32_t)-1) {
        hubPc = 0;
    }
    bytesOnLine = 0;
    current_lex = P->Lptr;
    current_lex->lineCounter = 0;
}

//
// every line will start with 24 bytes
//  HHHHH CCC DD DD DD DD |
// HHHHH is the hub address (5 + 1 space)
// CCC is the cog address, or blank (3 + 1 space)
// DD DD DD DD is up to 4 bytes of data (12)

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
    flexbuf_printf(f, "%02X ", c);
    bytesOnLine++;
    hubPc++;
    cogPc++;
}

static void AddRestOfLine(Flexbuf *f, const char *s) {
    int c;
    while (bytesOnLine < 4) {
        flexbuf_printf(f, "   ");
        bytesOnLine++;
    }
    // at this point we have written 6+4+12 == 22 characters
    // line it up to 24 to make tabs work
    flexbuf_printf(f, "| ");
    while (*s) {
        c = *s++;
        if (c == '\n') {
            break;
        }
        flexbuf_addchar(f, c);
    }
}

// AppendOneSrcLine:
// normally, this should just append line "line"
// however, if the line starts with '-' then skip it
// if the previous line starts with '-', then use that previous line
// returns true if something was printed
//
static bool AppendOneSrcLine(Flexbuf *f, int line, LexStream *L, bool needsStart)
{
    LineInfo *srcinfo;
    const char *theline;
    const char *prevline = NULL;
    const char *nextline = NULL;
    int maxline;

    srcinfo = (LineInfo *)flexbuf_peek(&L->lineInfo);
    maxline = flexbuf_curlen(&L->lineInfo) / sizeof(LineInfo);
    if (line < 0 || line >= maxline) return false;

    theline = srcinfo[line].linedata;
    if (!theline) return true;

    if (!strncmp(theline, "'-' ", 4)) {
        // we may want to skip this line, if the next line is
        // not a comment
        if (line < maxline-1) {
            nextline = srcinfo[line+1].linedata;
        }
        if (!strncmp(nextline, "'-' ", 4)) {
            theline = theline + 4;
        } else {
            return false; // skip this line
        }
    } else {
        if (line > 0) {
            prevline = srcinfo[line-1].linedata;
        }
    }
    if (prevline && !strncmp(prevline, "'-' ", 4)) {
        theline = prevline + 4;
    }
    // skip over preprocessor comments
    if (theline[0] == '{' && theline[1] == '#') {
        theline += 2;
        while (*theline && *theline != '}') theline++;
        if (*theline) theline++;
    }

    AddRestOfLine(f, theline);
    return true;
}

static bool catchUpToLine(Flexbuf *f, LexStream *L, int line, bool needsStart)
{
    int i;
    
    i = L->lineCounter;
    while (i < line) {
        if (needsStart) {
            startNewLine(f);
        }
        needsStart = AppendOneSrcLine(f, i, L, needsStart);
        i++;
    }
    L->lineCounter = i;
    return needsStart;
}

static int ignoreAst(AST *ast)
{
    if (!ast) return 1;
    switch (ast->kind) {
    case AST_COMMENT:
    case AST_SRCCOMMENT:
    case AST_COMMENTEDNODE:
    case AST_LINEBREAK:
        return 1;
    default:
        return 0;
    }
}
static void lstStartAst(Flexbuf *f, AST *ast)
{
    bool needsStart;
    
    if (ignoreAst(ast)) {
        return;
    }
    needsStart = catchUpToLine(f, ast->lexdata, ast->lineidx, true);

    // update PCs as appropriate
    switch (ast->kind) {
    case AST_ORG:
        inCog = 1;
        cogPc = 4*EvalPasmExpr(ast->left);
        break;
    case AST_ORGH:
    case AST_ORGH_MIN:
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
    if (needsStart) {
        startNewLine(f);
    }
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
        break; // return;
    case AST_ORGH:
        break;
    default:
        break;
    }
    catchUpToLine(f, ast->lexdata, ast->lineidx+1, false);
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
