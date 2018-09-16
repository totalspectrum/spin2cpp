#include <propeller.h>
#include "btest006.h"

int32_t btest006::testfloat(int32_t x)
{
  if (x > (1 << 16)) {
    return x;
  }
  return ((int32_t)(1 << 16));
}

void btest006::program(void)
{
  testfloat(2 << 16);
}

