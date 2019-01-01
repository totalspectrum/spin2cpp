#include <propeller.h>
#include "test046.h"

void test046::Fun(int32_t X, int32_t Y)
{
   if (X == 0) {
    switch(Y) {
    case 0:
      _OUTA ^= 0x1;
      break;
    case 1:
      _OUTA ^= 0x2;
      break;
    }
  } else if ((10 <= X) && (X <= 12)) {
    _OUTA ^= 0x4;
  } else if (1) {
    _OUTA ^= 0x8;
  }
}

