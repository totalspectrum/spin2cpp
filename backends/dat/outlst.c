//
// listing file output for spin2cpp
//
// Copyright 2012-2023 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "spinc.h"
#include "becommon.h"

int gl_list_options = 0;

static uint32_t cogPc;
static uint32_t hubPc;
static int inCog;
static int bytesOnLine;
static int cogPcUpdate = 0;

static LexStream *current_lex = NULL;

static void initOutput(Module *P)
{
    inCog = 0;
    cogPc = 0;
    hubPc = gl_dat_offset;
    if (hubPc == (uint32_t)-1) {
        hubPc = 0;
    }
    hubPc += gl_hub_base;
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
    if (f->len) {
        flexbuf_addchar(f, '\n');
    }
    flexbuf_printf(f, "%05x ", hubPc);
    if (inCog) {
        flexbuf_printf(f, "%03x ", cogPc / 4);
        cogPc += cogPcUpdate;
        cogPcUpdate = 0;
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

// check for Org in DAT output
// DAT sections get compiled to binary blobs with '-' comments giving the
// original code; but in listings we want COG addresses to show up, so we
// need to look for ORG / ORGH
static void CheckForOrg(const char *theline)
{
    const char *s = theline;
    
    while (*s && isspace(*s)) s++;
    if (*s == '\'' || *s == '{') return;
    if (!strncasecmp(s, "org", 3)) {
        if (s[3] == 'h' || s[3] == 'H') {
            inCog = 0;
        } else if (!s[3] || isspace(s[3])) {
            s += 3;
            if (isspace(*s)) s++;
            // Aargh, need to find the actual COG PC
            while (*s && *s != '$') s++;
            if (*s == '$') {
                s++;
                if (*s) {
                    cogPc = strtoul(s, NULL, 16)*4;
                    inCog = 1;
                }
            } else if (!*s) {
                cogPc = 0;
                inCog = 1;
            }
        }
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
            CheckForOrg(theline);
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
        // check for ORG and ORGH
        CheckForOrg(theline);
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
    uint32_t expectHubPc;
    
    if (ignoreAst(ast)) {
        return;
    }
    needsStart = catchUpToLine(f, ast->lexdata, ast->lineidx, true);

    // update PCs as appropriate
    switch (ast->kind) {
    case AST_ORG:
        inCog = 1;
        if (ast->left) {
            AST *addr = ast->left;
            if (addr->kind == AST_RANGE) {
                addr = addr->left;
            }
            cogPc = 4*EvalPasmExpr(addr);
        } else {
            cogPc = 0;
        }
        break;
    case AST_ORGH:
        inCog = 0;
        break;
    case AST_RES:
        if (inCog) {
            int delta = 4*EvalPasmExpr(ast->left);
            cogPcUpdate = delta; // advance cog PC after this line
        }
        break;
    case AST_INSTRHOLDER:
        inCog = (ast->d.ival & (1<<30)) != 0;
        if (inCog) {
            cogPc = (ast->d.ival & 0x00ffffff);
        } else {
            expectHubPc = (ast->d.ival & 0x00ffffff);
            if (expectHubPc != hubPc) {
                ERROR(ast, "mismatch in hub PC: expected 0x%x got 0x%x", expectHubPc, hubPc);
                abort();
            }
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
#if 1
        /* sanity check on label */
        {
            const char *name = ast->d.string;
            Symbol *sym = LookupSymbol(name);
            Label *labelref;
            if (sym && sym->kind == SYM_LABEL) {
//                printf("got label %s\n", name);
                labelref = (Label *)sym->v.ptr;
                if (labelref->hubval != hubPc) {
                    ERROR(ast, "HUB PC mismatch for %s: at $%x but label value is $%x", name, hubPc, labelref->hubval);
                }
            }
        }
#endif    
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

//
// compress the listing file by removing duplicate lines
//

// utility: decide if two lines are the same
static int
MatchLines(char *line1, char *line2)
{
    if (!line1 || !line2) return 0;

    if (line2 - line1 < 12) return 0;
    // ignore the addresses, just look at the hex output
    // addresses are 5 + 1 + 3 + 1 bytes
    // hex are 12 bytes
#define LINE_ADDRLEN 10    
#define LINE_HEXLEN 12    
    if (!strncmp(line1+LINE_ADDRLEN, line2+LINE_ADDRLEN, LINE_HEXLEN)) {
        // OK, they may match
        // make sure neither line ends with a '|'
        if (line1[LINE_ADDRLEN + LINE_HEXLEN] != '\n') return 0;
        if (line2[LINE_ADDRLEN + LINE_HEXLEN] != '\n') return 0;
        return 1;
    }
    return 0;
}

static size_t
CompressListing(char *listing)
{
#if 1
    char *line1 = listing;
    char *line2 = NULL;
    char *line3 = NULL;
    int skipping = 0;

    line2 = strchr(line1, '\n');
    if (!line2) return strlen(listing);
    line2++;
    line3 = strchr(line2, '\n');
    
    while (line3) {
        line3++;
        // OK, now if line1, line2, and line3 are identical then
        // enter the skipping state
        if (MatchLines(line1, line2) && MatchLines(line2, line3)) {
            skipping++;
        } else if (skipping) {
            // we were skipping, so line1...line2 are identical, but line3 is different
            if (skipping > 4) {
                size_t delta;
                line1 = strchr(line1, '\n') + 1;
                memset(line1, ' ', LINE_ADDRLEN + LINE_HEXLEN);
                memset(line1 + 6, '.', 3);
                line1 = strchr(line1, '\n') + 1;
                // line1 and line2 overlap, so we cannot use strcpy
                memcpy(line1, line2, strlen(line2) + 1);
                delta = line2 - line1;
                line2 = line1;
                line3 -= delta;
            }
            skipping = 0;
        }
        if (!skipping) {
            line1 = line2;
        }
        line2 = line3;
        line3 = strchr(line2, '\n');
   }
#endif    
    return strlen(listing);
}

static int
putConstant(Symbol *sym, void *arg)
{
    Flexbuf *f = (Flexbuf *)arg;
    if (sym->kind == SYM_CONSTANT || sym->kind == SYM_FLOAT_CONSTANT) {
        uint32_t val = EvalConstExpr((AST *)sym->v.ptr);
        // skip some internal symbols
        if (sym->flags & SYMF_INTERNAL) {
            return 1;
        }
        flexbuf_printf(f, "%20s = $%08x (%d)\n", sym->user_name, val, val);
    }
   
    return 1;
}

static void
DumpConstants(Flexbuf *f, Module *P)
{
    flexbuf_printf(f, "\nConstants:\n\n");
    IterateOverSymbols(&P->objsyms, putConstant, (void *)f);
}

void
OutputLstFile(const char *fname, Module *P)
{
    FILE *f = NULL;
    Module *save = current;
    Flexbuf fb;
    size_t curlen;
    char *listing;
    
    f = fopen(fname, "wb");
    if (!f) {
        fprintf(stderr, "Unable to open output file: ");
        perror(fname);
        exit(1);
    }
    current = P;
    flexbuf_init(&fb, BUFSIZ);
    initOutput(P);
    
    PrintDataBlock(&fb, P->datblock, &lstOutputFuncs, NULL);

    // finish up any pending source code
    if (current_lex) {
        LexStream *L = current_lex;
        int maxline = flexbuf_curlen(&L->lineInfo) / sizeof(LineInfo);
        catchUpToLine(&fb, L, maxline, true);
    }
    
    // make sure it ends with a newline
    flexbuf_addchar(&fb, '\n');

    if (gl_list_options & LIST_INCLUDE_CONSTANTS) {
        // add in constants
        DumpConstants(&fb, P);
    }
    
    flexbuf_addchar(&fb, 0);
    
    listing = flexbuf_get(&fb);
    curlen = CompressListing(listing);
    fwrite(listing, curlen, 1, f);
    fclose(f);
    flexbuf_delete(&fb);
    current = save;
}
