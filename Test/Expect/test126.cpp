#define __SPIN2CPP__
#include <propeller.h>
#include "test126.h"

int32_t test126::Select(int32_t X, int32_t Y, int32_t Z)
{
  return ((X) ? Y : Z + 2);
}

