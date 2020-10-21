//
// object file output for spin2cpp
//
// Copyright 2012-2020 Total Spectrum Software Inc.
// see the file COPYING for conditions of redistribution
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "spinc.h"

void
OutputObjFile(const char *fname, Module *P)
{
    FILE *f;
    LexStream *L = P->Lptr;
    LineInfo *srcinfo;
    int maxline;
    int i;
    const char *theline;

    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        gl_errors++;
        return;
    }
    srcinfo = (LineInfo *)flexbuf_peek(&L->lineInfo);
    maxline = flexbuf_curlen(&L->lineInfo) / sizeof(LineInfo);

    for (i = 0; i < maxline; i++) {
        theline = srcinfo[i].linedata;
        if (!theline) continue;
        fprintf(f, "%s", theline);
    }
    fclose(f);
}
