#define __SPIN2CPP__
#include <propeller.h>
#include "test066.h"

int32_t test066::Myinit(int32_t A, int32_t B)
{
  int32_t _parm__0000[3];
  _parm__0000[0] = 0;
  _parm__0000[1] = A;
  _parm__0000[2] = B;
  _parm__0000[1] = 1;
  memmove( (void *)&_parm__0000[0], (void *)&Ga, 4*(3));
  return _parm__0000[0];
}

