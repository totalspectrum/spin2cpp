#include <propeller.h>
#include "test33.h"

int32_t test33::byteextend(int32_t x)
{
  int32_t result = 0;
  return ((x << 24) >> 24);
}

int32_t test33::wordextend(int32_t x)
{
  int32_t result = 0;
  return ((x << 16) >> 16);
}

