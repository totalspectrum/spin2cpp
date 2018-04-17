#include <propeller.h>
#include "test68.h"

int32_t test68::Foo(int32_t N)
{
  int32_t 	_limit__0001, _step__0002;
  int32_t result = 0;
  for(( ( ( ( (result = 1), (_limit__0001 = N) ), (_step__0002 = 1) ), (_step__0002 = ((result >= _limit__0001) ? -_step__0002 : _step__0002)) ), (_limit__0001 = _limit__0001 + _step__0002) ); result != _limit__0001; result = result + _step__0002) {
    OUTA = result;
  }
  return result;
}

