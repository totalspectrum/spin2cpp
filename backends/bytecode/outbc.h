
#ifndef OUTBC_H
#define OUTBC_H

#include "spinc.h"
#include "bcbuffers.h"
#include "bcir.h"
#include "bc_bedata.h"



#define ModData(P) ((BCModData *)(P)->bedata)
#define FunData(P) ((BCFunData *)(P)->bedata)

typedef struct {
    int hiddenVariables; // Count of hidden variables on stack
    ByteOpIR *quitLabel,*nextLabel;
} BCContext;

void OutputByteCode(const char *fname, Module *P);

#endif
