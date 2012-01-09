#include <propeller.h>
#include "test32.h"

int32_t test32::fun(int32_t y)
{
  int32_t result = 0;
  switch ((x + y)) {
    case 10:
    case 'C':
      _OUTA ^= 0x1;
      break;
    case (4 * 2):
      _OUTA ^= 0x2;
      break;
    case 30 ... 40:
      _OUTA ^= 0x4;
      _OUTA ^= 0x8;
      break;
    default:
      _OUTA ^= 0x10;
      break;
  }
  x = (x + 5);
  return result;
}

