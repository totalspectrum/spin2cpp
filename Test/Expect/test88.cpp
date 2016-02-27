//
// make sure tab counts as 8 spaces!!
//
#include <propeller.h>
#include "test88.h"

int32_t test88::Sum(void)
{
  int32_t	R, X;
  R = 0;
  for(X = 0; X <= 4; X++) {
    R = R + X;
  }
  return R;
}

