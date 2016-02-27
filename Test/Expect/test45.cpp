#include <propeller.h>
#include "test45.h"

void test45::Fun(int32_t X, int32_t Y)
{
  if (X == 0) {
    if (Y == 0) {
      OUTA ^= 0x1;
    } else if (Y == 1) {
      OUTA ^= 0x2;
    }
  } else if (X == 20) {
    OUTA ^= 0x4;
  } else if (1) {
    OUTA ^= 0x8;
  }
}

