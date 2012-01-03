#include <propeller.h>
#include "test31.h"

int32_t test31::fun(int32_t Y)
{
  int32_t result = 0;
  switch ((X + Y)) {
    case 10:
      _OUTA ^= 0x1;
      break;
    case (A * 2):
      _OUTA ^= 0x2;
      _OUTA ^= 0x4;
      break;
    default:
      _OUTA ^= 0x8;
      break;
  }
  X = (X + 5);
  return result;
}

