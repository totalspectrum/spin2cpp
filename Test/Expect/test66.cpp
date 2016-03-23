#include <propeller.h>
#include "test66.h"

int32_t test66::Myinit(int32_t A, int32_t B)
{
  int32_t _parm__0001[3];
  _parm__0001[0] = 0;
  _parm__0001[1] = A;
  _parm__0001[2] = B;
  _parm__0001[1] = 1;
  memmove( (void *)&_parm__0001[0], (void *)&Ga, 4*(3));
  return _parm__0001[0];
}

