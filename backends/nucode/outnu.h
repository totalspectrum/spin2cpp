#ifndef OUTNU_H
#define OUTNU_H

#include "spinc.h"
#include "../bytecode/bcbuffers.h"

#define ModData(P) ((NuModData *)(P)->bedata)
#define FunData(P) ((NuFunData *)(P)->bedata)

typedef struct {
    int32_t compiledAddress; // -1 if not yet compiled
    BCRelocList *relocList;  // relocations
} NuModData;

typedef struct {
    OutputSpan *headerEntry;
    int compiledAddress; // -1 if not yet compiled
    int localSize;
} NuFunData;

void OutputNuCode(const char *fname, Module *P);

#endif
