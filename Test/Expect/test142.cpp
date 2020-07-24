#include <propeller.h>
#include "test142.h"

int32_t test142::Hibyte(int32_t X)
{
  return ((X >> 24) & 0xff);
}

