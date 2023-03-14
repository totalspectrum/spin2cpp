#ifndef OUTNU_H
#define OUTNU_H

#include "spinc.h"
#include "bcbuffers.h"
#include "nuir.h"

#define ModData(P) ((NuModData *)(P)->bedata)
#define FunData(F) ((NuFunData *)(F)->bedata)

typedef struct {
    NuIrLabel  *datLabel;
    bool        isCompiled;
} NuModData;

typedef struct {
    int localSize;
    NuIrList irl;
    NuIrLabel *entryLabel;
    NuIrLabel *dataLabel;
    Flexbuf dataBuf;  // inline data for the function
} NuFunData;

/* optimization functions */
int NuOptimizePeephole(NuIrList *irl);
int NuRemoveDupDrop(NuIrList *irl);
int NuRemoveDeadCode(NuIrList *irl);
int NuConvertDjnz(NuIrList *irl);
int NuRemoveUnusedLabels(NuIrList *irl);

/* utility functions */
NuIrOpcode NuInvertCondition(NuIrOpcode op);
NuIrOpcode NuReverseDirCondition(NuIrOpcode op);
void NuDeleteIr(NuIrList *irl, NuIr *ir);
void NuIrInsertBefore(NuIrList *irl, NuIr *anchor, NuIr *newitem);

/* ultimate output function */
void OutputNuCode(const char *fname, Module *P);

#endif
