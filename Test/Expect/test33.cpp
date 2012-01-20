#include <propeller.h>
#include "test33.h"

int32_t test33::Byteextend(int32_t X)
{
  int32_t result = 0;
  return ((X << 24) >> 24);
}

int32_t test33::Wordextend(int32_t X)
{
  int32_t result = 0;
  return ((X << 16) >> 16);
}

