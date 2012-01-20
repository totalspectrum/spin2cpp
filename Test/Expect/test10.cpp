#include <propeller.h>
#include "test10.h"

int32_t test10::Test1(int32_t X, int32_t Y, int32_t Z)
{
  int32_t result = 0;
  return (X + (Y * Z));
}

int32_t test10::Test2(int32_t X, int32_t Y, int32_t Z)
{
  int32_t result = 0;
  return ((X + Y) * Z);
}

int32_t test10::Test3(int32_t X, int32_t Y, int32_t Z)
{
  int32_t result = 0;
  return ((X * Y) + Z);
}

