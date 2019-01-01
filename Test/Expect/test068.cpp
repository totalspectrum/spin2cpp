#include <propeller.h>
#include "test068.h"

int32_t test068::Foo(int32_t N)
{
  int32_t 	_step__0001, _limit__0002;
  int32_t result = 0;
  for(( ( (result = 1), (_step__0001 = ((N > 1) ? 1 : -1)) ), (_limit__0002 = N + _step__0001) ); result != _limit__0002; result = result + _step__0001) {
    _OUTA = result;
  }
  return result;
}

