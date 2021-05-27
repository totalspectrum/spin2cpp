
#ifndef OUTBC_H
#define OUTBC_H

#include "spinc.h"
#include "bcbuffers.h"

#define BC_MAX_POINTERS 256


typedef struct {
    int compiledAddress; // -1 if not yet compiled
    int pub_cnt, pri_cnt, obj_cnt;
    Function *pubs[BC_MAX_POINTERS],*pris[BC_MAX_POINTERS];
    AST *objs[BC_MAX_POINTERS];
    int objs_arr_index[BC_MAX_POINTERS]; // Index of the object in the array it is part of. 0 for single objects
} BCModData;

typedef struct {
    OutputSpan *headerEntry;
    int compiledAddress; // -1 if not yet compiled
} BCFunData;

#define ModData(P) ((BCModData *)(P)->bedata)
#define FunData(P) ((BCFunData *)(P)->bedata)

typedef struct {
    int inCountedRepeat:1; // Have to pop counter on QUIT?
} BCContext;

void OutputByteCode(const char *fname, Module *P);

#endif
