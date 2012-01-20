#include <propeller.h>
#include "test12.h"

int32_t test12::Sum(int32_t X, int32_t Y)
{
  int32_t result = 0;
  return (X + Y);
}

int32_t test12::Donext(int32_t X)
{
  int32_t result = 0;
  return Sum(X, 1);
}

