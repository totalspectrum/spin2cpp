#define __SPIN2CPP__
#include <propeller.h>
#include "btest006.h"

int32_t btest006::testfloat(int32_t x)
{
  if (x > 65536) {
    return x;
  }
  return ((int32_t)65536);
}

void btest006::program(void)
{
  testfloat(131072);
}

