#include <propeller.h>
#include "test10.h"

int32_t test10::test1(int32_t x, int32_t y, int32_t z)
{
  int32_t result = 0;
  return (x + (y * z));
}

int32_t test10::test2(int32_t x, int32_t y, int32_t z)
{
  int32_t result = 0;
  return ((x + y) * z);
}

int32_t test10::test3(int32_t x, int32_t y, int32_t z)
{
  int32_t result = 0;
  return ((x * y) + z);
}

