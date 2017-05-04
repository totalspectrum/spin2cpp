#include <stdlib.h>
#include <propeller.h>
#include "test44.h"

void test44::Fun(int32_t X, int32_t Y)
{
  switch(X) {
  case 10:
    switch(Y) {
    case 0:
      OUTA ^= 0x1;
      break;
    case 1:
      OUTA ^= 0x2;
      break;
    }
    break;
  case 20:
    OUTA ^= 0x4;
    break;
  default:
    OUTA ^= 0x8;
    break;
  }
}

