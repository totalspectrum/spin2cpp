#include <propeller.h>
#include "test010.h"

int32_t test010::Test1(int32_t X, int32_t Y, int32_t Z)
{
  return (X + (Y * Z));
}

int32_t test010::Test2(int32_t X, int32_t Y, int32_t Z)
{
  return ((X + Y) * Z);
}

int32_t test010::Test3(int32_t X, int32_t Y, int32_t Z)
{
  return ((X * Y) + Z);
}

