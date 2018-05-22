#include <propeller.h>
#include "test089.h"

void test089::Catchlong(int32_t Longvar)
{
  int32_t _parm__0001[1];
  _parm__0001[0] = Longvar;
  B0 = ((char *)(&_parm__0001[0]))[0];
  B1 = ((char *)(&_parm__0001[0]))[1];
  B2 = ((char *)(&_parm__0001[0]))[2];
  B3 = ((char *)(&_parm__0001[0]))[3];
}

