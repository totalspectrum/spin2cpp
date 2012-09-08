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
OutputDatFile(const char *name, ParserState *P)
{
    FILE *f = NULL;
    char *fname = NULL;
    ParserState *save;

    save = current;
    current = P;

    fname = malloc(strlen(name) + 8);
    sprintf(fname, "%s.dat", name);
    f = fopen(fname, "w");
    if (!f) {
        perror(fname);
        free(fname);
        exit(1);
    }

    PrintDataBlock(f, P, BINARY_OUTPUT);

    current = save;

    fclose(f);
    free(fname);
}
