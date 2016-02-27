#include <propeller.h>
#include "test46.h"

void test46::Fun(int32_t X, int32_t Y)
{
  if (X == 0) {
    if (Y == 0) {
      OUTA ^= 0x1;
    } else if (Y == 1) {
      OUTA ^= 0x2;
    }
  } else if ((10 <= X) && (X <= 12)) {
    OUTA ^= 0x4;
  } else if (1) {
    OUTA ^= 0x8;
  }
}

