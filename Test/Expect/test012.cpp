#define __SPIN2CPP__
#include <propeller.h>
#include "test012.h"

int32_t test012::Sum(int32_t X, int32_t Y)
{
  return (X + Y);
}

int32_t test012::Donext(int32_t X)
{
  return Sum(X, 1);
}

