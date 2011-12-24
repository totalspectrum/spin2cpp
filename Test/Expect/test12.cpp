#include <propeller.h>
#include "test12.h"

int32_t test12::sum(void)
{
  int32_t result = 0;
  return (x + y);
}

int32_t test12::next(void)
{
  int32_t result = 0;
  return sum(x, 1);
}

