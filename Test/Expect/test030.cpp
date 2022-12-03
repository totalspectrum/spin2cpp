#define __SPIN2CPP__
#include <propeller.h>
#include "test030.h"

int32_t test030::Test(void)
{
  int32_t X;
  X = 0;
  if ((A == 0) && (B == 0)) {
    return 0;
  } else {
    if (A == 0) {
      return 1;
    } else {
      if (B == 0) {
        return 2;
      } else {
        X = -1;
      }
    }
  }
  return X;
}

