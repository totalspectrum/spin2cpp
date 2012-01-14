#include <propeller.h>
#include "test31.h"

int32_t test31::fun(int32_t y)
{
  int32_t result = 0;
  int32_t _tmp__0000 = (x + y);
  if (_tmp__0000 == 10) {
    _OUTA ^= 0x1;
  } else if (_tmp__0000 == (4 * 2)) {
    _OUTA ^= 0x2;
    _OUTA ^= 0x4;
  } else if (1) {
    _OUTA ^= 0x8;
  }
  x = (x + 5);
  return result;
}

