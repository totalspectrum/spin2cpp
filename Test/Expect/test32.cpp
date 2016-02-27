#include <propeller.h>
#include "test32.h"

void test32::Fun(int32_t Y)
{
  int32_t _tmp__0000 = (X + Y);
  if (_tmp__0000 == 10 || _tmp__0000 == 'C') {
    OUTA ^= 0x1;
  } else if (_tmp__0000 == (A * 2)) {
    OUTA ^= 0x2;
  } else if ((30 <= _tmp__0000) && (_tmp__0000 <= 40)) {
    OUTA ^= 0x4;
    OUTA ^= 0x8;
  } else if (1) {
    OUTA ^= 0x10;
  }
  X = X + 5;
}

