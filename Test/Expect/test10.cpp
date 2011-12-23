#include <propeller.h>
#include "test10.h"

int32_t test10::test1(void)
{
  int32_t result = 0;
  return (x + (y * z));
}

int32_t test10::test2(void)
{
  int32_t result = 0;
  return ((x + y) * z);
}

int32_t test10::test3(void)
{
  int32_t result = 0;
  return ((x * y) + z);
}

