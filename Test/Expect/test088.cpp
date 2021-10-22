//
// make sure tab counts as 8 spaces!!
//
#define __SPIN2CPP__
#include <propeller.h>
#include "test088.h"

int32_t test088::Sum(void)
{
  int32_t 	R;
  int32_t 	X;
  R = 0;
  for(X = 0; X < 5; X++) {
    R = R + X;
  }
  return R;
}

