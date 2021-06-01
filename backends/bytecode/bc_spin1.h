
#ifndef BC_SPIN1_H
#define BC_SPIN1_H

#include "bcir.h"

void GetSizeBound_Spin1(ByteOpIR *ir, int *min, int *max, int recursionsLeft);
const char *CompileIROP_Spin1(uint8_t *buf,int size,ByteOpIR *ir);


#endif
