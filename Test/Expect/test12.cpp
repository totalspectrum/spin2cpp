#include <propeller.h>
#include "test12.h"

int32_t test12::sum(int32_t x, int32_t y)
{
  int32_t result = 0;
  return (x + y);
}

int32_t test12::next(int32_t x)
{
  int32_t result = 0;
  return sum(x, 1);
}

