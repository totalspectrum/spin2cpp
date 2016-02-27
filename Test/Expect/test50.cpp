#include <propeller.h>
#include "test50.h"

void test50::Myinit(int32_t A, int32_t B, int32_t C)
{
  int32_t _parm__0000[3];
  _parm__0000[0] = A;
  _parm__0000[1] = B;
  _parm__0000[2] = C;
  memmove( (void *)&Gb, (void *)&_parm__0000[1], 4*(2));
}

