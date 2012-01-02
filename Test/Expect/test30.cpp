#include <propeller.h>
#include "test30.h"

int32_t test30::test(void)
{
  int32_t x = 0;
  if (((a == 0) && (b == 0))) {
    return 0;
  } else {
    if ((a == 0)) {
      return 1;
    } else {
      if ((b == 0)) {
        return 2;
      } else {
        x = (-1);
      }
    }
  }
  return x;
}

