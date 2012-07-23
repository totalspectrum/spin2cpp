#include <propeller.h>
#include "test88.h"

int32_t test88::Sum(void)
{
  int32_t result = 0;
  int32_t	R, X;
  R = 0;
  X = 0;
  do {
    R = (R + X);
    X = (X + 1);
  } while (X <= 4);
  return R;
}

