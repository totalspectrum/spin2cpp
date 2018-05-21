#include <propeller.h>
#include "test150.h"

int32_t test150::Testit(int32_t A, int32_t B, int32_t C, int32_t D)
{
  if (-((-(A != 0) ^ -(B != 0)) != 0) ^ -(C != 0)) {
    return D;
  }
  return (-(A != 0) ^ -(B != 0));
}

