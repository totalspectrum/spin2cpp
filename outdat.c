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

static void putbyte(FILE *f, unsigned int x)
{
    fputc(x & 0xff, f);
}
static void putword(FILE *f, unsigned int x)
{
    fputc(x & 0xff, f);
    fputc( (x>>8) & 0xff, f);
}
static void putlong(FILE *f, unsigned int x)
{
    fputc(x & 0xff, f);
    fputc( (x>>8) & 0xff, f);
    fputc( (x>>16) & 0xff, f);
    fputc( (x>>24) & 0xff, f);
}

static void
OutputSpinHeader(FILE *f, ParserState *P)
{
    unsigned int clkfreq = 80000000;
    unsigned int clkmodeval = 0x6f;
    
    putlong(f, clkfreq);
    putbyte(f, clkmodeval);
    putbyte(f, 0);      // checksum
    putword(f, 0x0010); // PBASE
    putword(f, 0x7fe8); // VBASE
    putword(f, 0x7ff0); // DBASE
    putword(f, 0x0018); // PCURR
    putword(f, 0x7ff8); // DCURR
    putword(f, 0x0008); // object length?
    putbyte(f, 0x02);
    putbyte(f, 0x00);
    putword(f, 0x0008);
    putword(f, 0x8000); // stack end

    // simple spin program
    putbyte(f, 0x3f);
    putbyte(f, 0x89);
    putbyte(f, 0xc7);
    putbyte(f, 0x10);
    putbyte(f, 0xa4);
    putbyte(f, 0x6);
    putbyte(f, 0x2c);
    putbyte(f, 0x32);
}

void
OutputDatFile(const char *fname, ParserState *P, int prefixBin)
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

    if (prefixBin) {
        /* output a binary header */
        OutputSpinHeader(f, P);
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
