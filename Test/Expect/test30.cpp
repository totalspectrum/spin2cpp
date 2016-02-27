#include <propeller.h>
#include "test30.h"

int32_t test30::Test(void)
{
  int32_t X = 0;
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

