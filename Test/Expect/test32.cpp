#include <propeller.h>
#include "test32.h"

extern inline int Between__(int32_t x, int32_t a, int32_t b){ if (a <= b) return x >= a && x <= b; return x >= b && x <= a; }

int32_t test32::fun(int32_t y)
{
  int32_t result = 0;
  int32_t _tmp__0000 = (x + y);
  if (_tmp__0000 == 10 || _tmp__0000 == 'C') {
    _OUTA ^= 0x1;
  } else if (_tmp__0000 == (4 * 2)) {
    _OUTA ^= 0x2;
  } else if (Between__(_tmp__0000, 30, 40)) {
    _OUTA ^= 0x4;
    _OUTA ^= 0x8;
  } else if (1) {
    _OUTA ^= 0x10;
  }
  x = (x + 5);
  return result;
}

