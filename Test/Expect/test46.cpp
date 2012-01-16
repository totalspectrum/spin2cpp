#include <propeller.h>
#include "test46.h"

extern inline int Between__(int32_t x, int32_t a, int32_t b){ if (a <= b) return x >= a && x <= b; return x >= b && x <= a; }

int32_t test46::fun(int32_t x, int32_t y)
{
  int32_t result = 0;
  if (x == 0) {
    if (y == 0) {
      _OUTA ^= 0x1;
    } else if (y == 1) {
      _OUTA ^= 0x2;
    }
  } else if (Between__(x, 10, 12)) {
    _OUTA ^= 0x4;
  } else if (1) {
    _OUTA ^= 0x8;
  }
  return result;
}

