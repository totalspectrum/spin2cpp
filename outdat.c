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
#include "spinc.h"

void
OutputDatFile(const char *fname, ParserState *P)
{
    FILE *f = NULL;
    ParserState *save;

    save = current;
    current = P;

    f = fopen(fname, "wb");
    if (!f) {
        perror(fname);
        exit(1);
    }

    PrintDataBlock(f, P, BINARY_OUTPUT);

    current = save;

    fclose(f);
}

void
OutputGasFile(const char *fname, ParserState *P)
{
    FILE *f = NULL;
    ParserState *save;

    save = current;
    current = P;

    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        exit(1);
    }

    PrintDataBlockForGas(f, P, 0);

    current = save;

    fclose(f);
}
