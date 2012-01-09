#include <propeller.h>
#include "test31.h"

int32_t test31::fun(int32_t y)
{
  int32_t result = 0;
  switch ((x + y)) {
    case 10:
      _OUTA ^= 0x1;
      break;
    case (4 * 2):
      _OUTA ^= 0x2;
      _OUTA ^= 0x4;
      break;
    default:
      _OUTA ^= 0x8;
      break;
  }
  x = (x + 5);
  return result;
}

