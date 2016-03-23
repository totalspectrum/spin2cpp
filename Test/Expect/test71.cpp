#include <propeller.h>
#include "test71.h"

int32_t test71::Blah(void)
{
  int32_t _parm__0001[10];
  _parm__0001[0] = 0;
  for(_parm__0001[1] = 0; _parm__0001[1] <= 7; _parm__0001[1]++) {
    Foo((int32_t)(&_parm__0001[0]), (int32_t)(&_parm__0001[2 + _parm__0001[1]]));
  }
  return _parm__0001[0];
}

void test71::Foo(int32_t M, int32_t N)
{
  OUTA |= (1 << N);
}

