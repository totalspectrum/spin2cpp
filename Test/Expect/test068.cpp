#define __SPIN2CPP__
#include <propeller.h>
#include "test068.h"

int32_t test068::Foo(int32_t N)
{
  int32_t 	_step__0000, _limit__0001;
  int32_t result;
  for(( ( (result = 1), (_step__0000 = ((N >= 1) ? 1 : -1)) ), (_limit__0001 = N + _step__0000) ); result != _limit__0001; result = result + _step__0000) {
    _OUTA = result;
  }
  return result;
}

