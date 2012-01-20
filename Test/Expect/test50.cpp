#include <propeller.h>
#include "test50.h"

int32_t test50::Myinit(int32_t A, int32_t B, int32_t C)
{
  int32_t _parm__0000[] = { A, B, C };
  int32_t result = 0;
  memmove( (void *)&Gb, (void *)&_parm__0000[1], 4*(2));
  return result;
}

