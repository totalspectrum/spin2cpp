#ifndef OUTNU_H
#define OUTNU_H

#include "spinc.h"
#include "bcbuffers.h"
#include "nuir.h"

#define ModData(P) ((NuModData *)(P)->bedata)
#define FunData(F) ((NuFunData *)(F)->bedata)

typedef struct {
    NuIrLabel *datLabel;
} NuModData;

typedef struct {
    int localSize;
    NuIrList irl;
    NuIrLabel *entryLabel;
    NuIrLabel *dataLabel;
    Flexbuf dataBuf;  // inline data for the function
} NuFunData;

void OutputNuCode(const char *fname, Module *P);

#endif
