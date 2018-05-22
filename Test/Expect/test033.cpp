#include <propeller.h>
#include "test033.h"

int32_t test033::Byteextend(int32_t X)
{
  return ((X << 24) >> 24);
}

int32_t test033::Wordextend(int32_t X)
{
  return ((X << 16) >> 16);
}

