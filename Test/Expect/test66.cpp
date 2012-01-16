#include <propeller.h>
#include "test66.h"

int32_t test66::myinit(int32_t a, int32_t b)
{
  int32_t _parm__0000[] = { 0, a, b };
  _parm__0000[1] = 1;
  memmove( (void *)&_parm__0000[0], (void *)&ga, 4*(3));
  return _parm__0000[0];
}

