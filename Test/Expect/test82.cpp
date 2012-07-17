#include <propeller.h>
#include "test82.h"

int32_t test82::Flip(void)
{
  int32_t result = 0;
  X = (~X);
  return result;
}

int32_t test82::Toggle(int32_t Pin)
{
  int32_t result = 0;
  _OUTA ^= (1<<Pin);
  return result;
}

