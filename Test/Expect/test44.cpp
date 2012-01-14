#include <propeller.h>
#include "test44.h"

int32_t test44::fun(int32_t x, int32_t y)
{
  int32_t result = 0;
  if (x == 10) {
    if (y == 0) {
      _OUTA ^= 0x1;
    } else if (y == 1) {
      _OUTA ^= 0x2;
    }
  } else if (x == 20) {
    _OUTA ^= 0x4;
  } else if (1) {
      _OUTA ^= 0x8;
  }
  return result;
}

