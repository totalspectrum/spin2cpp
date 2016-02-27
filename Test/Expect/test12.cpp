#include <propeller.h>
#include "test12.h"

int32_t test12::Sum(int32_t X, int32_t Y)
{
  return (X + Y);
}

int32_t test12::Donext(int32_t X)
{
  return Sum(X, 1);
}

