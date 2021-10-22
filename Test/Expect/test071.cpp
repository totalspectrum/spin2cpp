#define __SPIN2CPP__
#include <propeller.h>
#include "test071.h"

int32_t test071::Blah(void)
{
  int32_t _parm__0000[10];
  _parm__0000[0] = 0;
  for(_parm__0000[1] = 0; _parm__0000[1] < 8; _parm__0000[1]++) {
    Foo((&_parm__0000[0]), (&_parm__0000[2 + _parm__0000[1]]));
  }
  return _parm__0000[0];
}

void test071::Foo(int32_t *M, int32_t *N)
{
  _OUTA |= (1 << ((int32_t)N));
}

