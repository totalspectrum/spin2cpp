/*
 * Spin to ByteCode converter
 * Copyright 2018 Total Spectrum Software Inc.
 */

#ifndef OUTBYTECODE_H
#define OUTBYTECODE_H

#include "stackir.h"

//
// back end data for functions
//
typedef struct bc_bedata {
    Flexbuf code;
    /* flag for whether we might want to inline this function (not used yet) */
    bool isInline;
} ByteCodeFuncData;

#define FuncData(f) ((ByteCodeFuncData*)(f)->bedata)

//
// back end data for modules
//
typedef struct bcmoddata {
    Flexbuf *mem;   // final compiled form of the object
    Flexbuf *data;
    Flexbuf *dataRelocs;
} ByteCodeModData;

#define ModData(P) ((ByteCodeModData *)(P)->bedata)

#endif
